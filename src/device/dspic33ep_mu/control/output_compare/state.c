#include "device/dspic33ep_mu/internal.h"

uint16_t dspic33_device_internal_output_compare_base(uint8_t channel) {
    return (uint16_t)(OUTPUT_COMPARE_BASE + channel * OUTPUT_COMPARE_STRIDE);
}

uint8_t dspic33_device_internal_output_compare_pair_low(uint8_t channel) {
    return (uint8_t)(channel & 0xfeu);
}

uint8_t dspic33_device_internal_output_compare_pair_high(uint8_t channel) {
    return (uint8_t)(dspic33_device_internal_output_compare_pair_low(channel) + 1u);
}

bool dspic33_device_internal_output_compare_cascade_requested(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u));
    return (control2 & OUTPUT_COMPARE_32_BIT) != 0u;
}

static bool output_compare_clock_supported(uint16_t control1) {
    uint16_t timer_source = control1 & OUTPUT_COMPARE_TIMER_SOURCE_MASK;
    return timer_source <= OUTPUT_COMPARE_TIMER_SOURCE_TIMER1 ||
           timer_source == OUTPUT_COMPARE_TIMER_SOURCE_FP;
}

bool dspic33_device_internal_output_compare_configuration_supported(uint8_t channel,
                                                                    uint16_t control1,
                                                                    uint16_t control2) {
    uint16_t output_mode = control1 & OUTPUT_COMPARE_MODE_MASK;
    uint16_t synchronization_source = control2 & OUTPUT_COMPARE_SYNC_MASK;
    bool trigger_enabled = (control2 & OUTPUT_COMPARE_TRIGGER) != 0u;
    bool owns_source =
        synchronization_source == OUTPUT_COMPARE_SYNC_SELF ||
        (channel < 9u && synchronization_source == OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
    return output_compare_clock_supported(control1) &&
           (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u && output_mode != 0u &&
           (control2 & OUTPUT_COMPARE_CON2_UNSUPPORTED) == 0u &&
           (control2 & OUTPUT_COMPARE_32_BIT) == 0u &&
           synchronization_source != OUTPUT_COMPARE_SYNC_RESERVED &&
           !(trigger_enabled && owns_source);
}

bool dspic33_device_internal_output_compare_cascade_controls_supported(uint8_t channel,
                                                                       uint16_t control1,
                                                                       uint16_t control2) {
    uint16_t synchronization_source = control2 & OUTPUT_COMPARE_SYNC_MASK;
    bool trigger_enabled = (control2 & OUTPUT_COMPARE_TRIGGER) != 0u;
    bool owns_source =
        synchronization_source == OUTPUT_COMPARE_SYNC_SELF ||
        (channel < 9u && synchronization_source == OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
    return output_compare_clock_supported(control1) &&
           (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u &&
           (control1 & OUTPUT_COMPARE_MODE_MASK) != 0u &&
           (control2 & OUTPUT_COMPARE_CON2_UNSUPPORTED) == 0u &&
           (control2 & OUTPUT_COMPARE_32_BIT) != 0u &&
           synchronization_source != OUTPUT_COMPARE_SYNC_RESERVED &&
           !(trigger_enabled && owns_source);
}

bool dspic33_device_internal_output_compare_cascade_supported(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    uint16_t low_base = dspic33_device_internal_output_compare_base(low);
    uint16_t high_base = dspic33_device_internal_output_compare_base(high);
    uint16_t low_control1 = dspic33_device_internal_raw_word(cpu, low_base);
    uint16_t low_control2 = dspic33_device_internal_raw_word(cpu, (uint16_t)(low_base + 2u));
    uint16_t high_control1 = dspic33_device_internal_raw_word(cpu, high_base);
    uint16_t high_control2 = dspic33_device_internal_raw_word(cpu, (uint16_t)(high_base + 2u));
    return dspic33_device_internal_output_compare_cascade_controls_supported(low, low_control1,
                                                                             low_control2) &&
           dspic33_device_internal_output_compare_cascade_controls_supported(high, high_control1,
                                                                             high_control2) &&
           (low_control1 & (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) ==
               (high_control1 & (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) &&
           (low_control2 & OUTPUT_COMPARE_SYNC_MASK) ==
               (high_control2 & OUTPUT_COMPARE_SYNC_MASK) &&
           (low_control2 & OUTPUT_COMPARE_TRISTATE) != 0u &&
           (high_control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRISTATE)) == 0u;
}

bool dspic33_device_internal_output_compare_supported(const Dspic33* cpu, uint8_t channel) {
    uint16_t register_base = dspic33_device_internal_output_compare_base(channel);
    if (dspic33_device_internal_output_compare_cascade_requested(cpu, channel)) {
        return dspic33_device_internal_output_compare_cascade_supported(cpu, channel);
    }
    return dspic33_device_internal_output_compare_configuration_supported(
        channel, dspic33_device_internal_raw_word(cpu, register_base),
        dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u)));
}

bool dspic33_device_internal_output_compare_cascade_owner(const Dspic33* cpu, uint8_t channel) {
    return dspic33_device_internal_output_compare_cascade_supported(cpu, channel) &&
           channel == dspic33_device_internal_output_compare_pair_low(channel);
}

bool dspic33_device_internal_output_compare_timer_owner(const Dspic33* cpu, uint8_t channel) {
    return !dspic33_device_internal_output_compare_cascade_requested(cpu, channel) ||
           dspic33_device_internal_output_compare_cascade_owner(cpu, channel);
}

uint8_t dspic33_device_internal_output_compare_output_channel(const Dspic33* cpu, uint8_t channel) {
    return dspic33_device_internal_output_compare_cascade_supported(cpu, channel)
               ? dspic33_device_internal_output_compare_pair_high(channel)
               : channel;
}

bool dspic33_device_internal_output_compare_pmd_disabled(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.output_compare.pmd_disabled & (uint16_t)(1u << channel)) != 0u;
}

bool dspic33_device_internal_output_compare_operating(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 =
        dspic33_device_internal_raw_word(cpu, dspic33_device_internal_output_compare_base(channel));
    if (!dspic33_device_internal_output_compare_supported(cpu, channel) ||
        cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    if (dspic33_device_internal_output_compare_cascade_requested(cpu, channel)) {
        uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
        uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
        return !dspic33_device_internal_output_compare_pmd_disabled(cpu, low) &&
               !dspic33_device_internal_output_compare_pmd_disabled(cpu, high) &&
               (cpu->power_state != DSPIC33_POWER_IDLE ||
                ((dspic33_device_internal_raw_word(
                      cpu, dspic33_device_internal_output_compare_base(low)) |
                  dspic33_device_internal_raw_word(
                      cpu, dspic33_device_internal_output_compare_base(high))) &
                 OUTPUT_COMPARE_STOP_IDLE) == 0u);
    }
    return !dspic33_device_internal_output_compare_pmd_disabled(cpu, channel) &&
           (cpu->power_state != DSPIC33_POWER_IDLE || (control1 & OUTPUT_COMPARE_STOP_IDLE) == 0u);
}

static bool output_compare_internal_event(const Dspic33Event* event, uint8_t channel) {
    return event->type == DSPIC33_EVENT_OUTPUT_COMPARE && event->source == channel &&
           (event->value & OUTPUT_COMPARE_EVENT_KIND_MASK) != OUTPUT_COMPARE_EVENT_PMD;
}

static void output_compare_pause_events(Dspic33* cpu, uint8_t channel) {
    bool changed = false;
    for (size_t index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!output_compare_internal_event(event, channel) || event->paused) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void output_compare_resume_events(Dspic33* cpu, uint8_t channel) {
    bool changed = false;
    if (!dspic33_device_internal_output_compare_operating(cpu, channel)) {
        return;
    }
    for (size_t index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (!output_compare_internal_event(event, channel) || !event->paused) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

void dspic33_device_internal_output_compare_update_power_state(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        if (dspic33_device_internal_output_compare_operating(cpu, channel)) {
            output_compare_resume_events(cpu, channel);
        } else {
            output_compare_pause_events(cpu, channel);
        }
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        uint16_t pending = cpu->io.output_compare.fault_interrupt_pending;
        cpu->io.output_compare.fault_interrupt_pending = 0u;
        for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
            if ((pending & (uint16_t)(1u << channel)) != 0u) {
                dspic33_device_internal_output_compare_raise(cpu, channel);
            }
        }
    }
}

bool dspic33_device_internal_output_compare_fp_clocked(const Dspic33* cpu, uint8_t channel) {
    return (dspic33_device_internal_raw_word(cpu,
                                             dspic33_device_internal_output_compare_base(channel)) &
            OUTPUT_COMPARE_TIMER_SOURCE_MASK) == OUTPUT_COMPARE_TIMER_SOURCE_FP;
}

uint8_t dspic33_device_internal_output_compare_timer_source(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t timers[] = {1u, 2u, 3u, 4u, 0u};
    uint16_t selection = dspic33_device_internal_raw_word(
                             cpu, dspic33_device_internal_output_compare_base(channel)) &
                         OUTPUT_COMPARE_TIMER_SOURCE_MASK;
    return timers[selection >> 10u];
}

void dspic33_device_internal_output_compare_set_high(Dspic33* cpu, uint8_t channel, bool high) {
    uint16_t channel_bit = (uint16_t)(1u << channel);
    if (high) {
        cpu->io.output_compare.output_high |= channel_bit;
    } else {
        cpu->io.output_compare.output_high &= (uint16_t)~channel_bit;
    }
}

bool dspic33_device_internal_output_compare_high(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.output_compare.output_high & (uint16_t)(1u << channel)) != 0u;
}

static uint16_t output_compare_fault_enable(uint8_t source) {
    return (uint16_t)(0x0080u << source);
}

static uint16_t output_compare_fault_status(uint8_t source) {
    return (uint16_t)(0x0010u << source);
}

static bool output_compare_fault_capable(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode = dspic33_device_internal_raw_word(
                        cpu, dspic33_device_internal_output_compare_base(channel)) &
                    OUTPUT_COMPARE_MODE_MASK;
    return dspic33_device_internal_output_compare_supported(cpu, channel) &&
           !dspic33_device_internal_output_compare_pmd_disabled(cpu, channel) &&
           (mode == OUTPUT_COMPARE_MODE_EDGE_PWM || mode == OUTPUT_COMPARE_MODE_CENTER_PWM);
}

static uint16_t output_compare_active_fault_status(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 =
        dspic33_device_internal_raw_word(cpu, dspic33_device_internal_output_compare_base(channel));
    uint16_t status = 0u;
    uint8_t source;
    if (!output_compare_fault_capable(cpu, channel)) {
        return 0u;
    }
    for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
        uint8_t input = (uint8_t)(1u << source);
        if ((control1 & output_compare_fault_enable(source)) != 0u &&
            (cpu->io.output_compare.fault_inputs & input) == 0u) {
            status |= output_compare_fault_status(source);
        }
    }
    return status;
}

static void output_compare_enter_fault(Dspic33* cpu, uint8_t channel, uint16_t active) {
    uint16_t base = dspic33_device_internal_output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    dspic33_device_internal_raw_write_word(
        cpu, base, (uint16_t)(dspic33_device_internal_raw_word(cpu, base) | active));
    if ((cpu->io.output_compare.fault_held & bit) != 0u) {
        return;
    }
    cpu->io.output_compare.fault_held |= bit;
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        cpu->io.output_compare.fault_interrupt_pending |= bit;
    } else {
        dspic33_device_internal_output_compare_raise(cpu, channel);
    }
}

void dspic33_device_internal_output_compare_refresh_fault(Dspic33* cpu, uint8_t channel) {
    uint16_t active = output_compare_active_fault_status(cpu, channel);
    uint16_t bit = (uint16_t)(1u << channel);
    if (!output_compare_fault_capable(cpu, channel)) {
        cpu->io.output_compare.fault_held &= (uint16_t)~bit;
        cpu->io.output_compare.fault_interrupt_pending &= (uint16_t)~bit;
    } else if (active != 0u) {
        output_compare_enter_fault(cpu, channel, active);
    }
}

static void output_compare_fault_boundary(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_internal_output_compare_base(channel);
    uint16_t control1 = dspic33_device_internal_raw_word(cpu, base);
    uint16_t control2 = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t active = output_compare_active_fault_status(cpu, channel);
    uint16_t status = control1 & OUTPUT_COMPARE_FAULT_STATUS_MASK;
    uint16_t bit = (uint16_t)(1u << channel);
    if ((control2 & OUTPUT_COMPARE_FAULT_INACTIVE) == 0u) {
        status = active;
        dspic33_device_internal_raw_write_word(
            cpu, base, (uint16_t)((control1 & ~OUTPUT_COMPARE_FAULT_STATUS_MASK) | status));
    } else if (active != 0u) {
        status |= active;
        dspic33_device_internal_raw_write_word(cpu, base, (uint16_t)(control1 | active));
    }
    if (status == 0u) {
        cpu->io.output_compare.fault_held &= (uint16_t)~bit;
        cpu->io.output_compare.fault_interrupt_pending &= (uint16_t)~bit;
    } else {
        cpu->io.output_compare.fault_held |= bit;
    }
}

static void output_compare_set_fault_input(Dspic33* cpu, uint8_t source, bool high) {
    uint8_t channel;
    uint8_t bit = (uint8_t)(1u << source);
    if (high) {
        cpu->io.output_compare.fault_inputs |= bit;
    } else {
        cpu->io.output_compare.fault_inputs &= (uint8_t)~bit;
    }
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        dspic33_device_internal_output_compare_refresh_fault(cpu, channel);
    }
}

void dspic33_device_internal_output_compare_fault_input(Dspic33* cpu, uint8_t source, bool high) {
    cpu->io.output_compare.fault_direct_mask |= (uint8_t)(1u << source);
    output_compare_set_fault_input(cpu, source, high);
}

static uint8_t output_compare_fault_pps_pin(const Dspic33* cpu, uint8_t source) {
    uint16_t mapping;
    if (source < 2u) {
        mapping = dspic33_device_internal_raw_word(cpu, OUTPUT_COMPARE_FAULT_PPS_AB);
        return source == 0u ? (uint8_t)(mapping & 0x007fu) : (uint8_t)((mapping >> 8u) & 0x007fu);
    }
    return (uint8_t)(dspic33_device_internal_raw_word(cpu, OUTPUT_COMPARE_FAULT_PPS_C) & 0x007fu);
}

static bool output_compare_fault_selected_high(const Dspic33* cpu, uint8_t source, bool* high) {
    uint8_t selection = output_compare_fault_pps_pin(cpu, source);
    if (selection == 0u) {
        *high = false;
        return true;
    }
    if (selection <= DSPIC33_COMPARATOR_COUNT) {
        *high = (cpu->io.comparator.output_high & (uint8_t)(1u << (selection - 1u))) != 0u;
        return true;
    }
    return dspic33_device_internal_pps_physical_input_high(cpu, selection, high);
}

void dspic33_device_internal_output_compare_refresh_fault_pps_inputs(Dspic33* cpu) {
    uint8_t source;
    for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
        bool high;
        if ((cpu->io.output_compare.fault_direct_mask & (uint8_t)(1u << source)) == 0u &&
            output_compare_fault_selected_high(cpu, source, &high)) {
            output_compare_set_fault_input(cpu, source, high);
        }
    }
}

static void output_compare_abort(Dspic33* cpu, uint8_t channel) {
    if (dspic33_device_internal_output_compare_cascade_owner(cpu, channel)) {
        uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
        uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
        uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
        uint8_t member;
        for (member = low; member <= high; member++) {
            uint16_t member_base = dspic33_device_internal_output_compare_base(member);
            cpu->io.output_compare.generation[member]++;
            cpu->io.output_compare.timer_generation[member]++;
            dspic33_device_internal_raw_write_word(
                cpu, member_base,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, member_base) &
                           ~OUTPUT_COMPARE_MODE_MASK));
            dspic33_device_internal_raw_write_word(cpu, (uint16_t)(member_base + 8u), 0u);
            dspic33_device_internal_output_compare_set_high(cpu, member, false);
            cpu->io.output_compare.phase[member] = 0u;
            cpu->io.output_compare.sync_emitted[member] = false;
            cpu->io.output_compare.activation_cycle[member] = 0u;
        }
        cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
        cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    uint16_t base = dspic33_device_internal_output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    dspic33_device_internal_raw_write_word(
        cpu, base,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, base) & ~OUTPUT_COMPARE_MODE_MASK));
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    dspic33_device_internal_output_compare_set_high(cpu, channel, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] = 0u;
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
}

static bool output_compare_schedule(Dspic33* cpu, uint8_t channel, uint32_t kind, uint64_t delay) {
    bool timer_event =
        kind == OUTPUT_COMPARE_EVENT_PRIMARY || kind == OUTPUT_COMPARE_EVENT_SECONDARY ||
        kind == OUTPUT_COMPARE_EVENT_BOUNDARY || kind == OUTPUT_COMPARE_EVENT_SYNC ||
        kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY || kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY ||
        kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
    uint16_t generation = timer_event ? cpu->io.output_compare.timer_generation[channel]
                                      : cpu->io.output_compare.generation[channel];
    uint32_t value = kind | ((uint32_t)generation << OUTPUT_COMPARE_EVENT_GENERATION_SHIFT);
    if (dspic33_schedule(cpu, DSPIC33_EVENT_OUTPUT_COMPARE, channel, value, delay)) {
        return true;
    }
    output_compare_abort(cpu, channel);
    return false;
}

static bool output_compare_self_synchronized(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u));
    uint16_t source = control2 & OUTPUT_COMPARE_SYNC_MASK;
    return (control2 & OUTPUT_COMPARE_TRIGGER) == 0u &&
           (source == OUTPUT_COMPARE_SYNC_SELF ||
            (channel < 9u && source == OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
}

static bool output_compare_internal_period(const Dspic33* cpu, uint8_t channel) {
    return output_compare_self_synchronized(cpu, channel) ||
           (dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u)) &
            OUTPUT_COMPARE_TRIGGER) != 0u;
}

void dspic33_device_internal_output_compare_pulse_source(Dspic33* cpu, uint8_t source);

uint32_t dspic33_device_internal_output_compare_cascade_timer(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    return (uint32_t)dspic33_device_internal_raw_word(
               cpu, (uint16_t)(dspic33_device_internal_output_compare_base(low) + 8u)) |
           ((uint32_t)dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_internal_output_compare_base(high) + 8u))
            << 16u);
}

void dspic33_device_internal_output_compare_write_cascade_timer(Dspic33* cpu, uint8_t channel,
                                                                uint32_t timer) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(dspic33_device_internal_output_compare_base(low) + 8u), (uint16_t)timer);
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(dspic33_device_internal_output_compare_base(high) + 8u),
        (uint16_t)(timer >> 16u));
}

static uint32_t output_compare_input_capture_timer(const Dspic33* cpu, uint8_t source) {
    uint8_t channel = (uint8_t)(source - OUTPUT_COMPARE_SYNC_IC_FIRST);
    uint32_t timer = cpu->io.input_capture.timer[channel];
    if ((channel & 1u) == 0u &&
        dspic33_device_internal_input_capture_pair_configured(cpu, channel)) {
        timer |= (uint32_t)cpu->io.input_capture.timer[channel + 1u] << 16u;
    }
    return timer;
}

void dspic33_device_internal_output_compare_adopt_input_capture_timer(Dspic33* cpu, uint8_t channel,
                                                                      uint8_t source) {
    uint32_t timer = output_compare_input_capture_timer(cpu, source);
    if (dspic33_device_internal_output_compare_cascade_owner(cpu, channel)) {
        dspic33_device_internal_output_compare_write_cascade_timer(cpu, channel, timer);
    } else {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 8u),
            (uint16_t)timer);
    }
}

static uint32_t output_compare_cascade_r(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    return (uint32_t)cpu->io.output_compare.active_r[low] |
           ((uint32_t)cpu->io.output_compare.active_r[high] << 16u);
}

static uint32_t output_compare_cascade_rs(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    return (uint32_t)cpu->io.output_compare.active_rs[low] |
           ((uint32_t)cpu->io.output_compare.active_rs[high] << 16u);
}

static bool output_compare_cascade_pwm_mode(uint16_t mode) {
    return mode == OUTPUT_COMPARE_MODE_EDGE_PWM || mode == OUTPUT_COMPARE_MODE_CENTER_PWM;
}

static bool output_compare_cascade_pwm_degenerate(const Dspic33* cpu, uint8_t channel,
                                                  uint16_t mode) {
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    return output_compare_cascade_pwm_mode(mode) && (cpu->io.output_compare.active_r[high] == 0u ||
                                                     cpu->io.output_compare.active_rs[high] == 0u);
}

static uint64_t output_compare_boundary_delay(const Dspic33* cpu, uint8_t channel) {
    if (dspic33_device_internal_output_compare_cascade_owner(cpu, channel)) {
        uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
        uint16_t mode = dspic33_device_internal_raw_word(
                            cpu, dspic33_device_internal_output_compare_base(channel)) &
                        OUTPUT_COMPARE_MODE_MASK;
        uint32_t timer = dspic33_device_internal_output_compare_cascade_timer(cpu, channel);
        uint64_t delay = UINT64_C(0x100000000) - timer;
        uint32_t rs = output_compare_cascade_rs(cpu, channel);
        if (output_compare_cascade_pwm_degenerate(cpu, channel, mode)) {
            uint16_t high_r = cpu->io.output_compare.active_r[high];
            uint16_t high_rs = cpu->io.output_compare.active_rs[high];
            uint16_t high_timer = (uint16_t)(timer >> 16u);
            uint16_t low_timer = (uint16_t)timer;
            if (high_r == 0u && high_rs == 0u && cpu->io.output_compare.phase[channel] == 1u &&
                high_timer == 0u && low_timer <= cpu->io.output_compare.active_rs[channel]) {
                return (uint32_t)cpu->io.output_compare.active_rs[channel] + 1u - low_timer;
            }
            if (high_r == 0u && high_rs != 0u && high_timer < high_rs) {
                return ((uint64_t)(high_rs - high_timer) << 16u) - low_timer;
            }
            return delay;
        }
        if (output_compare_internal_period(cpu, channel) && timer <= rs) {
            delay = (uint64_t)rs + 1u - timer;
        }
        return delay;
    }
    uint16_t base = dspic33_device_internal_output_compare_base(channel);
    uint16_t timer = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 8u));
    uint64_t delay = UINT32_C(0x10000) - timer;
    if (output_compare_internal_period(cpu, channel) &&
        timer <= cpu->io.output_compare.active_rs[channel]) {
        delay = (uint32_t)cpu->io.output_compare.active_rs[channel] + 1u - timer;
    }
    return delay;
}

uint64_t dspic33_device_internal_output_compare_next_timer_event(const Dspic33* cpu,
                                                                 uint8_t channel, uint32_t* kind) {
    uint16_t base = dspic33_device_internal_output_compare_base(channel);
    uint16_t mode = dspic33_device_internal_raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK;
    uint16_t r = cpu->io.output_compare.active_r[channel];
    uint16_t rs = cpu->io.output_compare.active_rs[channel];
    uint16_t timer = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 8u));
    uint64_t boundary = output_compare_boundary_delay(cpu, channel);
    uint32_t target = r;
    uint32_t next_kind = OUTPUT_COMPARE_EVENT_PRIMARY;
    uint64_t delay = boundary;
    bool eligible = false;
    if ((cpu->io.output_compare.sync_reset_pending & (uint16_t)(1u << channel)) != 0u) {
        *kind = OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
        return 1u;
    }
    if ((dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
         (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRIGGER_STATUS)) == OUTPUT_COMPARE_TRIGGER) {
        *kind = OUTPUT_COMPARE_EVENT_BOUNDARY;
        return UINT64_MAX;
    }
    if (dspic33_device_internal_output_compare_cascade_owner(cpu, channel)) {
        uint32_t cascade_r = output_compare_cascade_r(cpu, channel);
        uint32_t cascade_rs = output_compare_cascade_rs(cpu, channel);
        uint32_t cascade_timer = dspic33_device_internal_output_compare_cascade_timer(cpu, channel);
        target = cascade_r;
        if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
            eligible = cascade_r != 0u && cascade_timer < cascade_r &&
                       (output_compare_cascade_pwm_degenerate(cpu, channel, mode) ||
                        !output_compare_internal_period(cpu, channel) || cascade_r < cascade_rs);
        } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH ||
                   mode == OUTPUT_COMPARE_MODE_SINGLE_LOW ||
                   mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
            eligible = cpu->io.output_compare.phase[channel] == 0u && cascade_timer < cascade_r;
        } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
                   mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
                   mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
            if (cpu->io.output_compare.phase[channel] == 0u) {
                eligible = cascade_timer < cascade_r;
            } else if (cpu->io.output_compare.phase[channel] == 1u &&
                       !output_compare_cascade_pwm_degenerate(cpu, channel, mode)) {
                target = cascade_rs;
                next_kind = OUTPUT_COMPARE_EVENT_SECONDARY;
                eligible = cascade_timer < cascade_rs;
            }
        }
        if (eligible && (uint64_t)(target - cascade_timer) < delay) {
            delay = target - cascade_timer;
        } else {
            next_kind = OUTPUT_COMPARE_EVENT_BOUNDARY;
        }
        if (!output_compare_internal_period(cpu, channel) &&
            !cpu->io.output_compare.sync_emitted[channel] && cascade_timer <= cascade_rs) {
            uint64_t sync_delay = (uint64_t)cascade_rs + 1u - cascade_timer;
            if (sync_delay < delay) {
                next_kind = OUTPUT_COMPARE_EVENT_SYNC;
                delay = sync_delay;
            } else if (sync_delay == delay && next_kind == OUTPUT_COMPARE_EVENT_PRIMARY) {
                next_kind = OUTPUT_COMPARE_EVENT_SYNC_PRIMARY;
            } else if (sync_delay == delay && next_kind == OUTPUT_COMPARE_EVENT_BOUNDARY) {
                next_kind = OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY;
            }
        }
        *kind = next_kind;
        return delay;
    }
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
        eligible = r != 0u && timer < r;
        if (output_compare_internal_period(cpu, channel)) {
            eligible = eligible && r < rs;
        }
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH || mode == OUTPUT_COMPARE_MODE_SINGLE_LOW) {
        eligible = cpu->io.output_compare.phase[channel] == 0u && timer < r;
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
        eligible = cpu->io.output_compare.phase[channel] == 0u && timer < r;
    } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
               mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
               mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        if (cpu->io.output_compare.phase[channel] == 0u) {
            eligible = timer < r;
        } else if (cpu->io.output_compare.phase[channel] == 1u) {
            target = rs;
            next_kind = OUTPUT_COMPARE_EVENT_SECONDARY;
            eligible = timer < rs;
        }
    }
    if (eligible && (uint32_t)((uint16_t)target - timer) < delay) {
        delay = (uint32_t)((uint16_t)target - timer);
    } else {
        next_kind = OUTPUT_COMPARE_EVENT_BOUNDARY;
    }
    if (!output_compare_internal_period(cpu, channel) &&
        !cpu->io.output_compare.sync_emitted[channel] && timer <= rs) {
        uint32_t sync_delay = (uint32_t)rs + 1u - timer;
        if (sync_delay < delay) {
            next_kind = OUTPUT_COMPARE_EVENT_SYNC;
            delay = sync_delay;
        } else if (sync_delay == delay && next_kind == OUTPUT_COMPARE_EVENT_PRIMARY) {
            next_kind = OUTPUT_COMPARE_EVENT_SYNC_PRIMARY;
        } else if (sync_delay == delay && next_kind == OUTPUT_COMPARE_EVENT_BOUNDARY) {
            next_kind = OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY;
        }
    }
    *kind = next_kind;
    return delay;
}

bool dspic33_device_internal_output_compare_schedule_next(Dspic33* cpu, uint8_t channel,
                                                          uint64_t initial_delay) {
    uint32_t kind;
    uint64_t delay;
    if (!dspic33_device_internal_output_compare_fp_clocked(cpu, channel)) {
        return true;
    }
    delay = dspic33_device_internal_output_compare_next_timer_event(cpu, channel, &kind);
    return delay == UINT64_MAX ||
           output_compare_schedule(cpu, channel, kind, initial_delay + delay);
}

void dspic33_device_internal_output_compare_start(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_internal_output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    uint64_t activation_delay =
        cpu->instruction_active
            ? dspic33_device_instruction_cycles(cpu, cpu->current_instruction_cycles)
            : 0u;
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    cpu->io.output_compare.active_rs[channel] =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 4u));
    cpu->io.output_compare.active_r[channel] =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 6u));
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    if (cpu->instruction_active) {
        cpu->io.output_compare.activation_pending |= bit;
    } else {
        cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    }
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] = cpu->device_cycles + activation_delay;
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    dspic33_device_internal_output_compare_set_high(
        cpu, channel,
        (dspic33_device_internal_raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK) ==
                OUTPUT_COMPARE_MODE_SINGLE_LOW ||
            ((dspic33_device_internal_raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK) ==
                 OUTPUT_COMPARE_MODE_EDGE_PWM &&
             cpu->io.output_compare.active_r[channel] != 0u));
    dspic33_device_internal_output_compare_schedule_next(cpu, channel, activation_delay);
}

void dspic33_device_internal_output_compare_start_cascade(Dspic33* cpu, uint8_t channel) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    uint16_t low_base = dspic33_device_internal_output_compare_base(low);
    uint16_t high_base = dspic33_device_internal_output_compare_base(high);
    uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
    uint64_t activation_delay =
        cpu->instruction_active
            ? dspic33_device_instruction_cycles(cpu, cpu->current_instruction_cycles)
            : 0u;
    cpu->io.output_compare.generation[low]++;
    cpu->io.output_compare.generation[high]++;
    cpu->io.output_compare.timer_generation[low]++;
    cpu->io.output_compare.timer_generation[high]++;
    cpu->io.output_compare.active_rs[low] =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(low_base + 4u));
    cpu->io.output_compare.active_rs[high] =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(high_base + 4u));
    cpu->io.output_compare.active_r[low] =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(low_base + 6u));
    cpu->io.output_compare.active_r[high] =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(high_base + 6u));
    cpu->io.output_compare.phase[low] = 0u;
    cpu->io.output_compare.phase[high] = 0u;
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
    if (cpu->instruction_active) {
        cpu->io.output_compare.activation_pending |= pair_bits;
    } else {
        cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
    }
    cpu->io.output_compare.sync_emitted[low] = false;
    cpu->io.output_compare.sync_emitted[high] = false;
    cpu->io.output_compare.activation_cycle[low] = cpu->device_cycles + activation_delay;
    cpu->io.output_compare.activation_cycle[high] = cpu->device_cycles + activation_delay;
    dspic33_device_internal_output_compare_write_cascade_timer(cpu, low, 0u);
    dspic33_device_internal_output_compare_set_high(cpu, low, false);
    dspic33_device_internal_output_compare_set_high(
        cpu, high,
        (dspic33_device_internal_raw_word(cpu, low_base) & OUTPUT_COMPARE_MODE_MASK) ==
            OUTPUT_COMPARE_MODE_SINGLE_LOW);
    dspic33_device_internal_output_compare_schedule_next(cpu, low, activation_delay);
}

void dspic33_device_internal_output_compare_stop(Dspic33* cpu, uint8_t channel) {
    uint16_t bit = (uint16_t)(1u << channel);
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 8u), 0u);
    dspic33_device_internal_output_compare_set_high(cpu, channel, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] = 0u;
    if (channel < 9u) {
        dspic33_device_internal_output_compare_pulse_source(
            cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
    }
}

void dspic33_device_internal_output_compare_stop_cascade(Dspic33* cpu, uint8_t channel) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
    cpu->io.output_compare.generation[low]++;
    cpu->io.output_compare.generation[high]++;
    cpu->io.output_compare.timer_generation[low]++;
    cpu->io.output_compare.timer_generation[high]++;
    dspic33_device_internal_output_compare_write_cascade_timer(cpu, low, 0u);
    dspic33_device_internal_output_compare_set_high(cpu, low, false);
    dspic33_device_internal_output_compare_set_high(cpu, high, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
    cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
    cpu->io.output_compare.phase[low] = 0u;
    cpu->io.output_compare.phase[high] = 0u;
    cpu->io.output_compare.sync_emitted[low] = false;
    cpu->io.output_compare.sync_emitted[high] = false;
    cpu->io.output_compare.activation_cycle[low] = 0u;
    cpu->io.output_compare.activation_cycle[high] = 0u;
    if (low < 9u) {
        dspic33_device_internal_output_compare_pulse_source(
            cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + low));
    }
}

void dspic33_device_internal_output_compare_raise(Dspic33* cpu, uint8_t channel) {
    uint8_t output = dspic33_device_internal_output_compare_output_channel(cpu, channel);
    if (output < 4u) {
        dspic33_dma_request(cpu, dspic33_device_output_compare_irqs[output], 0u, 0u);
    }
    dspic33_raise_interrupt(cpu, dspic33_device_output_compare_irqs[output]);
}

void dspic33_device_internal_output_compare_pulse_source(Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = dspic33_device_internal_output_compare_base(channel);
        uint16_t control2 = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
        uint16_t bit = (uint16_t)(1u << channel);
        if (!dspic33_device_internal_output_compare_timer_owner(cpu, channel) ||
            !dspic33_device_internal_output_compare_operating(cpu, channel) ||
            (control2 & OUTPUT_COMPARE_SYNC_MASK) != source ||
            output_compare_self_synchronized(cpu, channel)) {
            continue;
        }
        if ((control2 & OUTPUT_COMPARE_TRIGGER) != 0u) {
            if ((control2 & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u) {
                continue;
            }
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(base + 2u), (uint16_t)(control2 | OUTPUT_COMPARE_TRIGGER_STATUS));
        } else {
            if ((cpu->io.output_compare.sync_reset_pending & bit) != 0u) {
                continue;
            }
            cpu->io.output_compare.sync_reset_pending |= bit;
        }
        cpu->io.output_compare.timer_generation[channel]++;
        dspic33_device_internal_output_compare_schedule_next(cpu, channel, 0u);
    }
}

bool dspic33_device_internal_output_compare_source_awaited(const Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t control2 = dspic33_device_internal_raw_word(
            cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u));
        if (dspic33_device_internal_output_compare_timer_owner(cpu, channel) &&
            dspic33_device_internal_output_compare_operating(cpu, channel) &&
            (control2 & OUTPUT_COMPARE_SYNC_MASK) == source &&
            ((control2 & OUTPUT_COMPARE_TRIGGER) == 0u ||
             (control2 & OUTPUT_COMPARE_TRIGGER_STATUS) == 0u)) {
            return true;
        }
    }
    return false;
}

void dspic33_device_internal_output_compare_pulse_sync_source(Dspic33* cpu, uint8_t channel) {
    if (channel < 9u) {
        uint8_t source = (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
        dspic33_device_internal_input_capture_pulse_source(cpu, source);
        if (cpu->io.output_compare.clock_advancing) {
            cpu->io.output_compare.deferred_sync_pulses |= (uint16_t)(1u << channel);
        } else {
            dspic33_device_internal_output_compare_pulse_source(cpu, source);
        }
    }
}

bool dspic33_device_internal_output_compare_primary_match(Dspic33* cpu, uint8_t channel,
                                                          uint16_t mode) {
    uint8_t output = dspic33_device_internal_output_compare_output_channel(cpu, channel);
    if (mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH) {
        cpu->io.output_compare.phase[channel] = 2u;
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_LOW) {
        cpu->io.output_compare.phase[channel] = 2u;
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
        cpu->io.output_compare.phase[channel] = 1u;
    } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
               mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
               mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        cpu->io.output_compare.phase[channel] = 1u;
    } else if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
        if (dspic33_device_internal_output_compare_cascade_owner(cpu, channel) &&
            output_compare_cascade_pwm_degenerate(cpu, channel, mode)) {
            cpu->io.output_compare.phase[channel] = 1u;
        }
        dspic33_device_internal_output_compare_set_high(
            cpu, output, dspic33_device_internal_output_compare_cascade_owner(cpu, channel));
        return true;
    }
    if (mode <= OUTPUT_COMPARE_MODE_SINGLE_TOGGLE &&
        !output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_INTERRUPT, 3u)) {
        return false;
    }
    return output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_APPLY_PRIMARY, 1u);
}

bool dspic33_device_internal_output_compare_secondary_match(Dspic33* cpu, uint8_t channel,
                                                            uint16_t mode) {
    cpu->io.output_compare.phase[channel] = mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ? 2u : 0u;
    return output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_INTERRUPT, 3u) &&
           output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_APPLY_SECONDARY, 1u);
}

bool dspic33_device_internal_output_compare_boundary(Dspic33* cpu, uint8_t channel, uint16_t mode) {
    uint16_t base = dspic33_device_internal_output_compare_base(channel);
    uint8_t output = dspic33_device_internal_output_compare_output_channel(cpu, channel);
    bool cascade = dspic33_device_internal_output_compare_cascade_owner(cpu, channel);
    bool degenerate = cascade && output_compare_cascade_pwm_degenerate(cpu, channel, mode);
    bool first_low_period =
        degenerate && cpu->io.output_compare.active_r[output] == 0u &&
        cpu->io.output_compare.active_rs[output] == 0u &&
        cpu->io.output_compare.phase[channel] == 1u &&
        (dspic33_device_internal_output_compare_cascade_timer(cpu, channel) >> 16u) == 0u;
    if (cascade) {
        dspic33_device_internal_output_compare_write_cascade_timer(
            cpu, channel, first_low_period ? UINT32_C(0x10000) : 0u);
    } else {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    }
    cpu->io.output_compare.sync_emitted[channel] = false;
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM || mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        if (cascade) {
            uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
            uint16_t high_base = dspic33_device_internal_output_compare_base(high);
            cpu->io.output_compare.active_rs[channel] =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 4u));
            cpu->io.output_compare.active_rs[high] =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(high_base + 4u));
            cpu->io.output_compare.active_r[channel] =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 6u));
            cpu->io.output_compare.active_r[high] =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(high_base + 6u));
        } else {
            cpu->io.output_compare.active_rs[channel] =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 4u));
            cpu->io.output_compare.active_r[channel] =
                dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 6u));
        }
    }
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
        dspic33_device_internal_output_compare_set_high(
            cpu, output,
            degenerate || (!cascade && cpu->io.output_compare.active_r[channel] != 0u));
        dspic33_device_internal_output_compare_raise(cpu, channel);
        if (degenerate) {
            cpu->io.output_compare.phase[channel] = 0u;
        }
    } else if (mode == OUTPUT_COMPARE_MODE_CENTER_PWM && degenerate) {
        dspic33_device_internal_output_compare_set_high(cpu, output, true);
        cpu->io.output_compare.phase[channel] = 0u;
        dspic33_device_internal_output_compare_raise(cpu, channel);
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
        cpu->io.output_compare.phase[channel] = 0u;
        if ((dspic33_device_internal_output_compare_cascade_owner(cpu, channel)
                 ? output_compare_cascade_r(cpu, channel) == 0u
                 : cpu->io.output_compare.active_r[channel] == 0u) &&
            !output_compare_self_synchronized(cpu, channel) &&
            !dspic33_device_internal_output_compare_primary_match(cpu, channel, mode)) {
            return false;
        }
    } else if ((mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH ||
                mode == OUTPUT_COMPARE_MODE_SINGLE_LOW) &&
               cpu->io.output_compare.phase[channel] == 0u &&
               (dspic33_device_internal_output_compare_cascade_owner(cpu, channel)
                    ? output_compare_cascade_r(cpu, channel) == 0u
                    : cpu->io.output_compare.active_r[channel] == 0u) &&
               !output_compare_self_synchronized(cpu, channel) &&
               !dspic33_device_internal_output_compare_primary_match(cpu, channel, mode)) {
        return false;
    } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
               mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
               mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        uint32_t r = dspic33_device_internal_output_compare_cascade_owner(cpu, channel)
                         ? output_compare_cascade_r(cpu, channel)
                         : cpu->io.output_compare.active_r[channel];
        uint32_t rs = dspic33_device_internal_output_compare_cascade_owner(cpu, channel)
                          ? output_compare_cascade_rs(cpu, channel)
                          : cpu->io.output_compare.active_rs[channel];
        if (r == 0u && rs == 0u) {
            dspic33_device_internal_output_compare_set_high(cpu, output, false);
            cpu->io.output_compare.phase[channel] = 0u;
        } else if (cpu->io.output_compare.phase[channel] == 1u && rs == 0u) {
            if (!dspic33_device_internal_output_compare_secondary_match(cpu, channel, mode)) {
                return false;
            }
        } else if (cpu->io.output_compare.phase[channel] == 0u && r == 0u) {
            if (!dspic33_device_internal_output_compare_primary_match(cpu, channel, mode)) {
                return false;
            }
        }
    }
    if (output_compare_internal_period(cpu, channel)) {
        dspic33_device_internal_output_compare_pulse_sync_source(cpu, channel);
    }
    if ((dspic33_device_internal_raw_word(cpu, base) & OUTPUT_COMPARE_TRIGGER_ONESHOT) != 0u &&
        (dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) & OUTPUT_COMPARE_TRIGGER) !=
            0u) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
                       ~OUTPUT_COMPARE_TRIGGER_STATUS));
    }
    output_compare_fault_boundary(cpu, channel);
    if (cascade) {
        output_compare_fault_boundary(cpu,
                                      dspic33_device_internal_output_compare_pair_high(channel));
    }
    return true;
}
