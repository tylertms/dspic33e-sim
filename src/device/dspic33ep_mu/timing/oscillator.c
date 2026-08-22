#include "device/dspic33ep_mu/internal.h"

static void abort_oscillator_switch(Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    if (cpu->oscillator.active) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL,
                                           (uint16_t)(control & ~OSCILLATOR_SWITCH_ENABLE));
}

void dspic33_device_abort_oscillator_switch(Dspic33* cpu) {
    if (!cpu->oscillator.active && cpu->oscillator.lock_pending) {
        cpu->oscillator.generation++;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    abort_oscillator_switch(cpu);
}

static bool oscillator_pll_mode(uint16_t control) {
    uint16_t source = (uint16_t)((control & OSCILLATOR_REQUEST_MASK) >> 8u);
    return source == 1u || source == 3u;
}

uint8_t dspic33_device_internal_oscillator_current_source(uint16_t control) {
    return (uint8_t)((control & OSCILLATOR_CURRENT_MASK) >> 12u);
}

static uint8_t oscillator_requested_source(uint16_t control) {
    return (uint8_t)((control & OSCILLATOR_REQUEST_MASK) >> 8u);
}

static bool oscillator_source_available(const Dspic33* cpu, uint8_t source) {
    return (source != 2u && source != 3u) || (cpu->configuration[8u] & 0x03u) != 0x03u;
}

static bool oscillator_source_immediately_ready(uint8_t source) {
    return source == 0u || source >= 5u;
}

static bool oscillator_configuration_locked(const Dspic33* cpu, uint16_t control) {
    return (control & OSCILLATOR_CLOCK_LOCK) != 0u &&
           (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_CLOCK_LOCK) != 0u;
}

static bool oscillator_direct_pll_transition(uint16_t control) {
    uint8_t current = dspic33_device_internal_oscillator_current_source(control);
    uint8_t requested = oscillator_requested_source(control);
    return (current == 1u && requested == 3u) || (current == 3u && requested == 1u);
}

static bool oscillator_pll_lock_enabled(const Dspic33* cpu) {
    return (cpu->configuration[10u] & OSCILLATOR_CONFIGURATION_PLL_LOCK) != 0u;
}

static bool schedule_oscillator_event(Dspic33* cpu, uint16_t phase, uint64_t delay) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_OSCILLATOR, phase, cpu->oscillator.generation,
                          delay)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    return true;
}
static void schedule_oscillator_readiness(Dspic33* cpu, uint16_t control) {
    if (!oscillator_source_available(cpu, oscillator_requested_source(control))) {
        return;
    }
    schedule_oscillator_event(cpu, OSCILLATOR_EVENT_SWITCH,
                              oscillator_pll_mode(control) ? OSCILLATOR_SOURCE_DELAY
                                                           : OSCILLATOR_SWITCH_DELAY);
}

static uint8_t configured_main_pll_source(const Dspic33* cpu, uint16_t control) {
    if (cpu->oscillator.active) {
        if (oscillator_direct_pll_transition(control)) {
            return UINT8_MAX;
        }
        return oscillator_requested_source(control);
    }
    return dspic33_device_internal_oscillator_current_source(control);
}

static bool main_pll_relock_required(const Dspic33* cpu, uint16_t address, uint16_t previous,
                                     uint16_t current) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t source = configured_main_pll_source(cpu, control);
    uint16_t changed = (uint16_t)(previous ^ current);
    if (source != 1u && source != 3u) {
        return false;
    }
    if (address == MAIN_PLL_FEEDBACK) {
        return (changed & MAIN_PLL_FEEDBACK_DIVISOR) != 0u;
    }
    if (address == MAIN_OSCILLATOR_TUNING) {
        return source == 1u && (changed & MAIN_FRC_TUNING) != 0u;
    }
    if (address != MAIN_CLOCK_DIVISOR) {
        return false;
    }
    if ((changed & MAIN_PLL_PRESCALER) != 0u) {
        return true;
    }
    return source == 1u && (changed & MAIN_FRC_DIVISOR) != 0u;
}

static void restart_main_pll_lock(Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    bool source_ready = cpu->oscillator.active && cpu->oscillator.source_ready;
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = source_ready || !cpu->oscillator.active;
    cpu->oscillator.source_ready = source_ready;
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL,
                                           (uint16_t)(control & ~OSCILLATOR_PLL_LOCK));
    if (cpu->oscillator.lock_pending) {
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK, OSCILLATOR_SWITCH_DELAY);
    } else {
        schedule_oscillator_readiness(cpu, control);
    }
}

void dspic33_device_internal_update_main_clock_configuration(Dspic33* cpu, uint16_t address,
                                                             uint16_t previous) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    uint16_t current = dspic33_device_internal_raw_word(cpu, address);
    if (address != MAIN_CLOCK_DIVISOR && address != MAIN_PLL_FEEDBACK &&
        address != MAIN_OSCILLATOR_TUNING) {
        return;
    }
    if (oscillator_configuration_locked(cpu, control)) {
        dspic33_device_internal_raw_write_word(cpu, address, previous);
        return;
    }
    if (address == MAIN_CLOCK_DIVISOR) {
        if ((previous & 0x0800u) != 0u) {
            current = (uint16_t)((current & ~0x7000u) | (previous & 0x7000u));
        }
        if ((current & 0x7000u) == 0u) {
            current &= 0xf7ffu;
        }
        dspic33_device_internal_raw_write_word(cpu, address, current);
    }
    if (main_pll_relock_required(cpu, address, previous, current)) {
        restart_main_pll_lock(cpu);
    }
}

void dspic33_device_internal_oscillator_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t requested = oscillator_requested_source(control);
    if (cpu->oscillator.active && !cpu->oscillator.automatic &&
        (previous & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u &&
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u) {
        abort_oscillator_switch(cpu);
        return;
    }
    if (!cpu->oscillator.active || oscillator_direct_pll_transition(control) ||
        (requested != 2u && requested != 3u) ||
        ((previous ^ cpu->configuration[8u]) & 0x03u) == 0u) {
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, control);
}

void dspic33_device_internal_oscillator_pll_configuration_changed(Dspic33* cpu, uint8_t previous) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    if (!cpu->oscillator.active || !cpu->oscillator.source_ready ||
        oscillator_direct_pll_transition(control) || !oscillator_pll_mode(control) ||
        ((previous ^ cpu->configuration[10u]) & OSCILLATOR_CONFIGURATION_PLL_LOCK) == 0u) {
        return;
    }
    if (!oscillator_pll_lock_enabled(cpu)) {
        control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                             ((control & OSCILLATOR_REQUEST_MASK) << 4u));
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->watchdog.ticks = 0u;
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    }
}

void dspic33_device_internal_oscillator_startup_configuration_changed(Dspic33* cpu,
                                                                      uint8_t previous) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if (((previous ^ cpu->configuration[6u]) & 0x07u) == 0u ||
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u) {
        return;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    dspic33_device_internal_start_automatic_oscillator_switch(cpu, source);
}

static void start_oscillator_switch(Dspic33* cpu, uint16_t control) {
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = false;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL,
                                           (uint16_t)((control | OSCILLATOR_SWITCH_ENABLE) &
                                                      ~OSCILLATOR_PLL_LOCK &
                                                      ~OSCILLATOR_CLOCK_FAIL));
    schedule_oscillator_readiness(cpu, control);
}

void dspic33_device_internal_start_automatic_oscillator_switch(Dspic33* cpu, uint8_t source) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    control = (uint16_t)((control & ~OSCILLATOR_REQUEST_MASK & ~OSCILLATOR_PLL_LOCK &
                          ~OSCILLATOR_CLOCK_FAIL & ~OSCILLATOR_SWITCH_ENABLE) |
                         ((uint16_t)source << 8u));
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    if (oscillator_source_immediately_ready(source)) {
        control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK) | ((uint16_t)source << 12u));
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = true;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, control);
}

void dspic33_device_internal_reset_main_oscillator(Dspic33* cpu) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    uint16_t control = (uint16_t)(source << 8u);
    if ((cpu->configuration[6u] & 0x80u) == 0u) {
        control |= (uint16_t)(source << 12u);
        if (source == 1u || source == 3u) {
            control |= OSCILLATOR_PLL_LOCK;
        }
    }
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

void dspic33_device_power_on_reset(Dspic33* cpu) {
    uint8_t source = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if ((cpu->configuration[6u] & 0x80u) != 0u && source != 0u) {
        dspic33_device_internal_start_automatic_oscillator_switch(cpu, source);
    }
}

void dspic33_device_reset_restored(Dspic33* cpu) {
    dspic33_device_internal_pps_capture_shadow(cpu);
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

void dspic33_device_brown_out_reset(Dspic33* cpu) {
    size_t destination = 0u;
    size_t source;
    for (source = 0u; source < cpu->events.count; source++) {
        if (cpu->events.items[source].type != DSPIC33_EVENT_OSCILLATOR) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
    memset(&cpu->oscillator, 0, sizeof(cpu->oscillator));
    dspic33_device_internal_reset_main_oscillator(cpu);
    dspic33_device_power_on_reset(cpu);
    dspic33_device_reset_restored(cpu);
}

void dspic33_device_internal_complete_oscillator_event(Dspic33* cpu, uint16_t phase,
                                                       uint32_t generation) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    if (generation != cpu->oscillator.generation) {
        return;
    }
    if (phase == OSCILLATOR_EVENT_LOCK) {
        if (!cpu->oscillator.lock_pending) {
            return;
        }
        if (cpu->oscillator.active && cpu->oscillator.source_ready) {
            control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                                 ((control & OSCILLATOR_REQUEST_MASK) << 4u) | OSCILLATOR_PLL_LOCK);
            cpu->oscillator.active = false;
            cpu->oscillator.automatic = false;
            cpu->watchdog.ticks = 0u;
            dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        } else if (dspic33_device_internal_oscillator_current_source(control) == 1u ||
                   dspic33_device_internal_oscillator_current_source(control) == 3u) {
            dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL,
                                                   (uint16_t)(control | OSCILLATOR_PLL_LOCK));
        }
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
        return;
    }
    if (!cpu->oscillator.active ||
        (!cpu->oscillator.automatic && (control & OSCILLATOR_SWITCH_ENABLE) == 0u)) {
        return;
    }
    if (!oscillator_source_available(cpu, oscillator_requested_source(control))) {
        return;
    }
    if (oscillator_pll_mode(control)) {
        cpu->oscillator.source_ready = true;
        cpu->oscillator.lock_pending = true;
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK,
                                  OSCILLATOR_SWITCH_DELAY - OSCILLATOR_SOURCE_DELAY);
        if (oscillator_pll_lock_enabled(cpu)) {
            return;
        }
    }
    control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                         ((control & OSCILLATOR_REQUEST_MASK) << 4u));
    cpu->oscillator.active = false;
    cpu->oscillator.automatic = false;
    cpu->watchdog.ticks = 0u;
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

static bool oscillator_key_authorized(const Dspic33* cpu, uint8_t lane, uint8_t stage) {
    return cpu->oscillator.key_stage == stage && cpu->oscillator.key_lane == lane &&
           cpu->instructions == cpu->oscillator.key_instruction + 1u &&
           cpu->interrupt_count == cpu->oscillator.key_interrupt_count &&
           cpu->trap_count == cpu->oscillator.key_trap_count;
}

static void oscillator_key_start(Dspic33* cpu, uint8_t lane) {
    cpu->oscillator.key_stage = 1u;
    cpu->oscillator.key_lane = lane;
    cpu->oscillator.key_instruction = cpu->instructions;
    cpu->oscillator.key_interrupt_count = cpu->interrupt_count;
    cpu->oscillator.key_trap_count = cpu->trap_count;
}

static void apply_oscillator_high(Dspic33* cpu, uint16_t previous, uint16_t requested) {
    uint16_t control;
    if (oscillator_configuration_locked(cpu, previous)) {
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, previous);
        return;
    }
    control =
        (uint16_t)((previous & ~OSCILLATOR_REQUEST_MASK) | (requested & OSCILLATOR_REQUEST_MASK));
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
        control &= (uint16_t)~OSCILLATOR_SWITCH_ENABLE;
    }
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
}

static void apply_oscillator_low(Dspic33* cpu, uint16_t previous, uint16_t requested) {
    uint16_t control = previous;
    uint16_t writable = OSCILLATOR_IO_LOCK;
    bool was_io_locked = (previous & OSCILLATOR_IO_LOCK) != 0u;
    bool requests_io_lock = (requested & OSCILLATOR_IO_LOCK) != 0u;
    if (was_io_locked && !requests_io_lock && (cpu->configuration[8u] & 0x20u) != 0u &&
        cpu->io.pps.one_way_committed) {
        requested |= OSCILLATOR_IO_LOCK;
    }
    if (!oscillator_configuration_locked(cpu, previous)) {
        writable |= OSCILLATOR_LP_ENABLE;
    }
    control = (uint16_t)((control & ~writable) | (requested & writable));
    if (!was_io_locked && (control & OSCILLATOR_IO_LOCK) != 0u) {
        cpu->io.pps.one_way_committed = true;
    }
    control |= (uint16_t)(requested & OSCILLATOR_CLOCK_LOCK);
    if ((requested & OSCILLATOR_CLOCK_FAIL) == 0u) {
        control &= (uint16_t)~OSCILLATOR_CLOCK_FAIL;
    }
    if ((requested & OSCILLATOR_SWITCH_ENABLE) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if ((cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u ||
        oscillator_configuration_locked(cpu, control) ||
        ((control & OSCILLATOR_CURRENT_MASK) >> 4u) == (control & OSCILLATOR_REQUEST_MASK)) {
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL,
                                               (uint16_t)(control & ~OSCILLATOR_SWITCH_ENABLE));
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if (oscillator_direct_pll_transition(control)) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = true;
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL,
                                               (uint16_t)(control | OSCILLATOR_SWITCH_ENABLE));
        return;
    }
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
    }
    start_oscillator_switch(cpu, control);
}

bool dspic33_oscillator_failure_detected(Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    uint8_t current = dspic33_device_internal_oscillator_current_source(control);
    if ((cpu->configuration[8u] & 0xc0u) != 0u || cpu->power_state == DSPIC33_POWER_SLEEP ||
        (current != 2u && current != 3u && current != 4u)) {
        return false;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    control = (uint16_t)((control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_PLL_LOCK) |
                         OSCILLATOR_CLOCK_FAIL);
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, control);
    dspic33_raise_oscillator_fail_trap(cpu);
    return true;
}

bool dspic33_device_internal_protect_oscillator_write(Dspic33* cpu, uint16_t address,
                                                      uint16_t previous) {
    uint16_t requested;
    uint8_t lane;
    uint8_t value;
    uint8_t first;
    uint8_t second;
    if (address != OSCILLATOR_CONTROL && address != OSCILLATOR_CONTROL + 1u) {
        return false;
    }
    requested = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    lane = (uint8_t)(address - OSCILLATOR_CONTROL);
    value = cpu->data[address];
    first = lane == 0u ? 0x46u : 0x78u;
    second = lane == 0u ? 0x57u : 0x9au;
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, previous);
    if (!cpu->instruction_active || cpu->io.cpu_write_width != 1u) {
        cpu->oscillator.key_stage = 0u;
        return true;
    }
    if (oscillator_key_authorized(cpu, lane, 2u)) {
        cpu->oscillator.key_stage = 0u;
        if (lane == 0u) {
            apply_oscillator_low(cpu, previous, requested);
        } else {
            apply_oscillator_high(cpu, previous, requested);
        }
        return true;
    }
    if (oscillator_key_authorized(cpu, lane, 1u) && value == second) {
        cpu->oscillator.key_stage = 2u;
        cpu->oscillator.key_instruction = cpu->instructions;
        return true;
    }
    if (value == first) {
        oscillator_key_start(cpu, lane);
        return true;
    }
    cpu->oscillator.key_stage = 0u;
    return true;
}
