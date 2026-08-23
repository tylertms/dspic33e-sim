#include "device/dspic33ep_mu/internal.h"

static void abort_oscillator_switch(Dspic33* cpu) {
    uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    if (cpu->oscillator.active) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    dspic33_device_internal_raw_write_word(
        cpu, OSCILLATOR_CONTROL, (uint16_t)(oscillator_control & ~OSCILLATOR_SWITCH_ENABLE));
}

void dspic33_device_abort_oscillator_switch(Dspic33* cpu) {
    if (!cpu->oscillator.active && cpu->oscillator.lock_pending) {
        cpu->oscillator.generation++;
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
    }
    abort_oscillator_switch(cpu);
}

static bool oscillator_pll_mode(uint16_t oscillator_control) {
    const uint16_t requested_source_code =
        (uint16_t)((oscillator_control & OSCILLATOR_REQUEST_MASK) >> 8u);

    return requested_source_code == 1u || requested_source_code == 3u;
}

uint8_t dspic33_device_internal_oscillator_current_source(uint16_t oscillator_control) {
    return (uint8_t)((oscillator_control & OSCILLATOR_CURRENT_MASK) >> 12u);
}

static uint8_t oscillator_requested_source(uint16_t oscillator_control) {
    return (uint8_t)((oscillator_control & OSCILLATOR_REQUEST_MASK) >> 8u);
}

static bool oscillator_source_available(const Dspic33* cpu, uint8_t requested_source_code) {
    return (requested_source_code != 2u && requested_source_code != 3u) ||
           (cpu->configuration[8u] & 0x03u) != 0x03u;
}

static bool oscillator_source_immediately_ready(uint8_t requested_source_code) {
    return requested_source_code == 0u || requested_source_code >= 5u;
}

static bool oscillator_configuration_locked(const Dspic33* cpu, uint16_t oscillator_control) {
    return (oscillator_control & OSCILLATOR_CLOCK_LOCK) != 0u &&
           (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_CLOCK_LOCK) != 0u;
}

static bool oscillator_direct_pll_transition(uint16_t oscillator_control) {
    const uint8_t current_source_code =
        dspic33_device_internal_oscillator_current_source(oscillator_control);
    const uint8_t requested_source_code = oscillator_requested_source(oscillator_control);

    return (current_source_code == 1u && requested_source_code == 3u) ||
           (current_source_code == 3u && requested_source_code == 1u);
}

static bool oscillator_pll_lock_enabled(const Dspic33* cpu) {
    return (cpu->configuration[10u] & OSCILLATOR_CONFIGURATION_PLL_LOCK) != 0u;
}

static bool schedule_oscillator_event(Dspic33* cpu, uint16_t event_phase, uint64_t delay_cycles) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_OSCILLATOR, event_phase, cpu->oscillator.generation,
                          delay_cycles)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    return true;
}

static void schedule_oscillator_readiness(Dspic33* cpu, uint16_t oscillator_control) {
    if (!oscillator_source_available(cpu, oscillator_requested_source(oscillator_control))) {
        return;
    }
    const uint64_t source_readiness_delay =
        oscillator_pll_mode(oscillator_control) ? OSCILLATOR_SOURCE_DELAY : OSCILLATOR_SWITCH_DELAY;
    schedule_oscillator_event(cpu, OSCILLATOR_EVENT_SWITCH, source_readiness_delay);
}

static uint8_t configured_main_pll_source(const Dspic33* cpu, uint16_t oscillator_control) {
    if (cpu->oscillator.active) {
        if (oscillator_direct_pll_transition(oscillator_control)) {
            return UINT8_MAX;
        }
        return oscillator_requested_source(oscillator_control);
    }
    return dspic33_device_internal_oscillator_current_source(oscillator_control);
}

static bool main_pll_relock_required(const Dspic33* cpu, uint16_t address, uint16_t previous_value,
                                     uint16_t current_value) {
    const uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    const uint8_t configured_source_code = configured_main_pll_source(cpu, oscillator_control);
    const uint16_t changed_mask = (uint16_t)(previous_value ^ current_value);

    if (configured_source_code != 1u && configured_source_code != 3u) {
        return false;
    }
    if (address == MAIN_PLL_FEEDBACK) {
        return (changed_mask & MAIN_PLL_FEEDBACK_DIVISOR) != 0u;
    }
    if (address == MAIN_OSCILLATOR_TUNING) {
        return configured_source_code == 1u && (changed_mask & MAIN_FRC_TUNING) != 0u;
    }
    if (address != MAIN_CLOCK_DIVISOR) {
        return false;
    }
    if ((changed_mask & MAIN_PLL_PRESCALER) != 0u) {
        return true;
    }
    return configured_source_code == 1u && (changed_mask & MAIN_FRC_DIVISOR) != 0u;
}

static void restart_main_pll_lock(Dspic33* cpu) {
    const uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    const bool is_source_ready = cpu->oscillator.active && cpu->oscillator.source_ready;
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = is_source_ready || !cpu->oscillator.active;
    cpu->oscillator.source_ready = is_source_ready;
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL,
                                           (uint16_t)(oscillator_control & ~OSCILLATOR_PLL_LOCK));
    if (cpu->oscillator.lock_pending) {
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK, OSCILLATOR_SWITCH_DELAY);
    } else {
        schedule_oscillator_readiness(cpu, oscillator_control);
    }
}

void dspic33_device_internal_update_main_clock_configuration(Dspic33* cpu, uint16_t address,
                                                             uint16_t previous_value) {
    uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    uint16_t current_value = dspic33_device_internal_raw_word(cpu, address);
    if (address != MAIN_CLOCK_DIVISOR && address != MAIN_PLL_FEEDBACK &&
        address != MAIN_OSCILLATOR_TUNING) {
        return;
    }
    if (oscillator_configuration_locked(cpu, oscillator_control)) {
        dspic33_device_internal_raw_write_word(cpu, address, previous_value);
        return;
    }
    if (address == MAIN_CLOCK_DIVISOR) {
        if ((previous_value & 0x0800u) != 0u) {
            current_value = (uint16_t)((current_value & ~0x7000u) | (previous_value & 0x7000u));
        }
        if ((current_value & 0x7000u) == 0u) {
            current_value &= 0xf7ffu;
        }
        dspic33_device_internal_raw_write_word(cpu, address, current_value);
    }
    if (main_pll_relock_required(cpu, address, previous_value, current_value)) {
        restart_main_pll_lock(cpu);
    }
}

void dspic33_device_internal_oscillator_configuration_changed(Dspic33* cpu,
                                                              uint8_t previous_configuration) {
    const uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    const uint8_t requested_source_code = oscillator_requested_source(oscillator_control);
    if (cpu->oscillator.active && !cpu->oscillator.automatic &&
        (previous_configuration & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u &&
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u) {
        abort_oscillator_switch(cpu);
        return;
    }
    if (!cpu->oscillator.active || oscillator_direct_pll_transition(oscillator_control) ||
        (requested_source_code != 2u && requested_source_code != 3u) ||
        ((previous_configuration ^ cpu->configuration[8u]) & 0x03u) == 0u) {
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, oscillator_control);
}

void dspic33_device_internal_oscillator_pll_configuration_changed(Dspic33* cpu,
                                                                  uint8_t previous_configuration) {
    uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    if (!cpu->oscillator.active || !cpu->oscillator.source_ready ||
        oscillator_direct_pll_transition(oscillator_control) ||
        !oscillator_pll_mode(oscillator_control) ||
        ((previous_configuration ^ cpu->configuration[10u]) & OSCILLATOR_CONFIGURATION_PLL_LOCK) ==
            0u) {
        return;
    }
    if (!oscillator_pll_lock_enabled(cpu)) {
        oscillator_control =
            (uint16_t)((oscillator_control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                       ((oscillator_control & OSCILLATOR_REQUEST_MASK) << 4u));
        cpu->oscillator.active = false;
        cpu->oscillator.automatic = false;
        cpu->watchdog.ticks = 0u;
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
    }
}

void dspic33_device_internal_oscillator_startup_configuration_changed(
    Dspic33* cpu, uint8_t previous_configuration) {
    const uint8_t startup_source_code = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if (((previous_configuration ^ cpu->configuration[6u]) & 0x07u) == 0u ||
        (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) == 0u) {
        return;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    dspic33_device_internal_start_automatic_oscillator_switch(cpu, startup_source_code);
}

static void start_oscillator_switch(Dspic33* cpu, uint16_t oscillator_control) {
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = false;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    dspic33_device_internal_raw_write_word(
        cpu, OSCILLATOR_CONTROL,
        (uint16_t)((oscillator_control | OSCILLATOR_SWITCH_ENABLE) & ~OSCILLATOR_PLL_LOCK &
                   ~OSCILLATOR_CLOCK_FAIL));
    schedule_oscillator_readiness(cpu, oscillator_control);
}

void dspic33_device_internal_start_automatic_oscillator_switch(Dspic33* cpu,
                                                               uint8_t requested_source_code) {
    uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    oscillator_control =
        (uint16_t)((oscillator_control & ~OSCILLATOR_REQUEST_MASK & ~OSCILLATOR_PLL_LOCK &
                    ~OSCILLATOR_CLOCK_FAIL & ~OSCILLATOR_SWITCH_ENABLE) |
                   ((uint16_t)requested_source_code << 8u));
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
    if (oscillator_source_immediately_ready(requested_source_code)) {
        oscillator_control = (uint16_t)((oscillator_control & ~OSCILLATOR_CURRENT_MASK) |
                                        ((uint16_t)requested_source_code << 12u));
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
        return;
    }
    cpu->oscillator.generation++;
    cpu->oscillator.active = true;
    cpu->oscillator.automatic = true;
    cpu->oscillator.lock_pending = false;
    cpu->oscillator.source_ready = false;
    schedule_oscillator_readiness(cpu, oscillator_control);
}

void dspic33_device_internal_reset_main_oscillator(Dspic33* cpu) {
    const uint8_t startup_source_code = (uint8_t)(cpu->configuration[6u] & 0x07u);
    uint16_t oscillator_control = (uint16_t)(startup_source_code << 8u);
    if ((cpu->configuration[6u] & 0x80u) == 0u) {
        oscillator_control |= (uint16_t)(startup_source_code << 12u);
        if (startup_source_code == 1u || startup_source_code == 3u) {
            oscillator_control |= OSCILLATOR_PLL_LOCK;
        }
    }
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
}

void dspic33_device_power_on_reset(Dspic33* cpu) {
    const uint8_t startup_source_code = (uint8_t)(cpu->configuration[6u] & 0x07u);
    if ((cpu->configuration[6u] & 0x80u) != 0u && startup_source_code != 0u) {
        dspic33_device_internal_start_automatic_oscillator_switch(cpu, startup_source_code);
    }
}

void dspic33_device_reset_restored(Dspic33* cpu) {
    dspic33_device_internal_pps_capture_shadow(cpu);
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

void dspic33_device_brown_out_reset(Dspic33* cpu) {
    size_t retained_count = 0u;
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        if (cpu->events.items[event_index].type != DSPIC33_EVENT_OSCILLATOR) {
            cpu->events.items[retained_count++] = cpu->events.items[event_index];
        }
    }
    cpu->events.count = retained_count;
    dspic33_reorder_events(cpu);
    memset(&cpu->oscillator, 0, sizeof(cpu->oscillator));
    dspic33_device_internal_reset_main_oscillator(cpu);
    dspic33_device_power_on_reset(cpu);
    dspic33_device_reset_restored(cpu);
}

void dspic33_device_internal_complete_oscillator_event(Dspic33* cpu, uint16_t phase,
                                                       uint32_t generation) {
    uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    if (generation != cpu->oscillator.generation) {
        return;
    }
    if (phase == OSCILLATOR_EVENT_LOCK) {
        if (!cpu->oscillator.lock_pending) {
            return;
        }
        if (cpu->oscillator.active && cpu->oscillator.source_ready) {
            oscillator_control = (uint16_t)((oscillator_control & ~OSCILLATOR_CURRENT_MASK &
                                             ~OSCILLATOR_SWITCH_ENABLE) |
                                            ((oscillator_control & OSCILLATOR_REQUEST_MASK) << 4u) |
                                            OSCILLATOR_PLL_LOCK);
            cpu->oscillator.active = false;
            cpu->oscillator.automatic = false;
            cpu->watchdog.ticks = 0u;
            dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
        } else if (dspic33_device_internal_oscillator_current_source(oscillator_control) == 1u ||
                   dspic33_device_internal_oscillator_current_source(oscillator_control) == 3u) {
            dspic33_device_internal_raw_write_word(
                cpu, OSCILLATOR_CONTROL, (uint16_t)(oscillator_control | OSCILLATOR_PLL_LOCK));
        }
        cpu->oscillator.lock_pending = false;
        cpu->oscillator.source_ready = false;
        return;
    }
    if (!cpu->oscillator.active ||
        (!cpu->oscillator.automatic && (oscillator_control & OSCILLATOR_SWITCH_ENABLE) == 0u)) {
        return;
    }
    if (!oscillator_source_available(cpu, oscillator_requested_source(oscillator_control))) {
        return;
    }
    if (oscillator_pll_mode(oscillator_control)) {
        cpu->oscillator.source_ready = true;
        cpu->oscillator.lock_pending = true;
        schedule_oscillator_event(cpu, OSCILLATOR_EVENT_LOCK,
                                  OSCILLATOR_SWITCH_DELAY - OSCILLATOR_SOURCE_DELAY);
        if (oscillator_pll_lock_enabled(cpu)) {
            return;
        }
    }
    oscillator_control =
        (uint16_t)((oscillator_control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_SWITCH_ENABLE) |
                   ((oscillator_control & OSCILLATOR_REQUEST_MASK) << 4u));
    cpu->oscillator.active = false;
    cpu->oscillator.automatic = false;
    cpu->watchdog.ticks = 0u;
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
}

static bool oscillator_key_authorized(const Dspic33* cpu, uint8_t key_lane, uint8_t key_stage) {
    return cpu->oscillator.key_stage == key_stage && cpu->oscillator.key_lane == key_lane &&
           cpu->instructions == cpu->oscillator.key_instruction + 1u &&
           cpu->interrupt_count == cpu->oscillator.key_interrupt_count &&
           cpu->trap_count == cpu->oscillator.key_trap_count;
}

static void oscillator_key_start(Dspic33* cpu, uint8_t key_lane) {
    cpu->oscillator.key_stage = 1u;
    cpu->oscillator.key_lane = key_lane;
    cpu->oscillator.key_instruction = cpu->instructions;
    cpu->oscillator.key_interrupt_count = cpu->interrupt_count;
    cpu->oscillator.key_trap_count = cpu->trap_count;
}

static void apply_oscillator_high(Dspic33* cpu, uint16_t previous_control,
                                  uint16_t requested_control) {
    uint16_t oscillator_control;
    if (oscillator_configuration_locked(cpu, previous_control)) {
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, previous_control);
        return;
    }
    oscillator_control = (uint16_t)((previous_control & ~OSCILLATOR_REQUEST_MASK) |
                                    (requested_control & OSCILLATOR_REQUEST_MASK));
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
        oscillator_control &= (uint16_t)~OSCILLATOR_SWITCH_ENABLE;
    }
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
}

static void apply_oscillator_low(Dspic33* cpu, uint16_t previous_control,
                                 uint16_t requested_control) {
    uint16_t oscillator_control = previous_control;
    uint16_t writable_bits = OSCILLATOR_IO_LOCK;
    const bool was_io_locked = (previous_control & OSCILLATOR_IO_LOCK) != 0u;
    const bool requested_io_lock = (requested_control & OSCILLATOR_IO_LOCK) != 0u;

    if (was_io_locked && !requested_io_lock && (cpu->configuration[8u] & 0x20u) != 0u &&
        cpu->io.pps.one_way_committed) {
        requested_control |= OSCILLATOR_IO_LOCK;
    }
    if (!oscillator_configuration_locked(cpu, previous_control)) {
        writable_bits |= OSCILLATOR_LP_ENABLE;
    }
    oscillator_control =
        (uint16_t)((oscillator_control & ~writable_bits) | (requested_control & writable_bits));
    if (!was_io_locked && (oscillator_control & OSCILLATOR_IO_LOCK) != 0u) {
        cpu->io.pps.one_way_committed = true;
    }
    oscillator_control |= (uint16_t)(requested_control & OSCILLATOR_CLOCK_LOCK);
    if ((requested_control & OSCILLATOR_CLOCK_FAIL) == 0u) {
        oscillator_control &= (uint16_t)~OSCILLATOR_CLOCK_FAIL;
    }
    if ((requested_control & OSCILLATOR_SWITCH_ENABLE) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if ((cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_SWITCH_DISABLE) != 0u ||
        oscillator_configuration_locked(cpu, oscillator_control) ||
        ((oscillator_control & OSCILLATOR_CURRENT_MASK) >> 4u) ==
            (oscillator_control & OSCILLATOR_REQUEST_MASK)) {
        dspic33_device_internal_raw_write_word(
            cpu, OSCILLATOR_CONTROL, (uint16_t)(oscillator_control & ~OSCILLATOR_SWITCH_ENABLE));
        if (!cpu->oscillator.automatic) {
            abort_oscillator_switch(cpu);
        }
        return;
    }
    if (oscillator_direct_pll_transition(oscillator_control)) {
        cpu->oscillator.generation++;
        cpu->oscillator.active = true;
        dspic33_device_internal_raw_write_word(
            cpu, OSCILLATOR_CONTROL, (uint16_t)(oscillator_control | OSCILLATOR_SWITCH_ENABLE));
        return;
    }
    if (cpu->oscillator.active) {
        abort_oscillator_switch(cpu);
    }
    start_oscillator_switch(cpu, oscillator_control);
}

bool dspic33_oscillator_failure_detected(Dspic33* cpu) {
    uint16_t oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    const uint8_t current_source_code =
        dspic33_device_internal_oscillator_current_source(oscillator_control);
    if ((cpu->configuration[8u] & 0xc0u) != 0u || cpu->power_state == DSPIC33_POWER_SLEEP ||
        (current_source_code != 2u && current_source_code != 3u && current_source_code != 4u)) {
        return false;
    }
    dspic33_device_abort_oscillator_switch(cpu);
    oscillator_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    oscillator_control =
        (uint16_t)((oscillator_control & ~OSCILLATOR_CURRENT_MASK & ~OSCILLATOR_PLL_LOCK) |
                   OSCILLATOR_CLOCK_FAIL);
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, oscillator_control);
    dspic33_raise_oscillator_fail_trap(cpu);
    return true;
}

bool dspic33_device_internal_protect_oscillator_write(Dspic33* cpu, uint16_t address,
                                                      uint16_t previous_control) {
    uint16_t requested_control;
    uint8_t control_write_lane;
    uint8_t control_write_value;
    uint8_t first_unlock_key;
    uint8_t second_unlock_key;
    if (address != OSCILLATOR_CONTROL && address != OSCILLATOR_CONTROL + 1u) {
        return false;
    }
    requested_control = dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL);
    control_write_lane = (uint8_t)(address - OSCILLATOR_CONTROL);
    control_write_value = cpu->data[address];
    first_unlock_key = control_write_lane == 0u ? 0x46u : 0x78u;
    second_unlock_key = control_write_lane == 0u ? 0x57u : 0x9au;
    dspic33_device_internal_raw_write_word(cpu, OSCILLATOR_CONTROL, previous_control);
    if (!cpu->instruction_active || cpu->io.cpu_write_width != 1u) {
        cpu->oscillator.key_stage = 0u;
        return true;
    }
    if (oscillator_key_authorized(cpu, control_write_lane, 2u)) {
        cpu->oscillator.key_stage = 0u;
        if (control_write_lane == 0u) {
            apply_oscillator_low(cpu, previous_control, requested_control);
        } else {
            apply_oscillator_high(cpu, previous_control, requested_control);
        }
        return true;
    }
    if (oscillator_key_authorized(cpu, control_write_lane, 1u) &&
        control_write_value == second_unlock_key) {
        cpu->oscillator.key_stage = 2u;
        cpu->oscillator.key_instruction = cpu->instructions;
        return true;
    }
    if (control_write_value == first_unlock_key) {
        oscillator_key_start(cpu, control_write_lane);
        return true;
    }
    cpu->oscillator.key_stage = 0u;
    return true;
}
