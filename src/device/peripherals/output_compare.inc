
static uint16_t output_compare_base(uint8_t channel) {
    return (uint16_t)(OUTPUT_COMPARE_BASE + channel * OUTPUT_COMPARE_STRIDE);
}

static uint8_t output_compare_pair_low(uint8_t channel) { return (uint8_t)(channel & 0xfeu); }

static uint8_t output_compare_pair_high(uint8_t channel) {
    return (uint8_t)(output_compare_pair_low(channel) + 1u);
}

static bool output_compare_cascade_requested(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
    return (control2 & OUTPUT_COMPARE_32_BIT) != 0u;
}

static bool output_compare_clock_supported(uint16_t control1) {
    uint16_t source = control1 & OUTPUT_COMPARE_TIMER_SOURCE_MASK;
    return source <= OUTPUT_COMPARE_TIMER_SOURCE_TIMER1 || source == OUTPUT_COMPARE_TIMER_SOURCE_FP;
}

static bool output_compare_configuration_supported(uint8_t channel, uint16_t control1,
                                                   uint16_t control2) {
    uint16_t mode = control1 & OUTPUT_COMPARE_MODE_MASK;
    uint16_t synchronization = control2 & OUTPUT_COMPARE_SYNC_MASK;
    bool trigger = (control2 & OUTPUT_COMPARE_TRIGGER) != 0u;
    bool own_source = synchronization == OUTPUT_COMPARE_SYNC_SELF ||
                      (channel < 9u && synchronization == OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
    return output_compare_clock_supported(control1) &&
           (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u && mode != 0u &&
           (control2 & OUTPUT_COMPARE_CON2_UNSUPPORTED) == 0u &&
           (control2 & OUTPUT_COMPARE_32_BIT) == 0u &&
           synchronization != OUTPUT_COMPARE_SYNC_RESERVED && !(trigger && own_source);
}

static bool output_compare_cascade_controls_supported(uint8_t channel, uint16_t control1,
                                                      uint16_t control2) {
    uint16_t synchronization = control2 & OUTPUT_COMPARE_SYNC_MASK;
    bool trigger = (control2 & OUTPUT_COMPARE_TRIGGER) != 0u;
    bool own_source = synchronization == OUTPUT_COMPARE_SYNC_SELF ||
                      (channel < 9u && synchronization == OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
    return output_compare_clock_supported(control1) &&
           (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u &&
           (control1 & OUTPUT_COMPARE_MODE_MASK) != 0u &&
           (control2 & OUTPUT_COMPARE_CON2_UNSUPPORTED) == 0u &&
           (control2 & OUTPUT_COMPARE_32_BIT) != 0u &&
           synchronization != OUTPUT_COMPARE_SYNC_RESERVED && !(trigger && own_source);
}

static bool output_compare_cascade_supported(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t low_control1 = raw_word(cpu, low_base);
    uint16_t low_control2 = raw_word(cpu, (uint16_t)(low_base + 2u));
    uint16_t high_control1 = raw_word(cpu, high_base);
    uint16_t high_control2 = raw_word(cpu, (uint16_t)(high_base + 2u));
    return output_compare_cascade_controls_supported(low, low_control1, low_control2) &&
           output_compare_cascade_controls_supported(high, high_control1, high_control2) &&
           (low_control1 & (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) ==
               (high_control1 & (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) &&
           (low_control2 & OUTPUT_COMPARE_SYNC_MASK) ==
               (high_control2 & OUTPUT_COMPARE_SYNC_MASK) &&
           (low_control2 & OUTPUT_COMPARE_TRISTATE) != 0u &&
           (high_control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRISTATE)) == 0u;
}

static bool output_compare_supported(const Dspic33* cpu, uint8_t channel) {
    uint16_t base = output_compare_base(channel);
    if (output_compare_cascade_requested(cpu, channel)) {
        return output_compare_cascade_supported(cpu, channel);
    }
    return output_compare_configuration_supported(channel, raw_word(cpu, base),
                                                  raw_word(cpu, (uint16_t)(base + 2u)));
}

static bool output_compare_cascade_owner(const Dspic33* cpu, uint8_t channel) {
    return output_compare_cascade_supported(cpu, channel) &&
           channel == output_compare_pair_low(channel);
}

static bool output_compare_timer_owner(const Dspic33* cpu, uint8_t channel) {
    return !output_compare_cascade_requested(cpu, channel) ||
           output_compare_cascade_owner(cpu, channel);
}

static uint8_t output_compare_output_channel(const Dspic33* cpu, uint8_t channel) {
    return output_compare_cascade_supported(cpu, channel) ? output_compare_pair_high(channel)
                                                          : channel;
}

static bool output_compare_pmd_disabled(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.output_compare.pmd_disabled & (uint16_t)(1u << channel)) != 0u;
}

static bool output_compare_operating(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 = raw_word(cpu, output_compare_base(channel));
    if (!output_compare_supported(cpu, channel) || cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    if (output_compare_cascade_requested(cpu, channel)) {
        uint8_t low = output_compare_pair_low(channel);
        uint8_t high = output_compare_pair_high(channel);
        return !output_compare_pmd_disabled(cpu, low) && !output_compare_pmd_disabled(cpu, high) &&
               (cpu->power_state != DSPIC33_POWER_IDLE ||
                ((raw_word(cpu, output_compare_base(low)) |
                  raw_word(cpu, output_compare_base(high))) &
                 OUTPUT_COMPARE_STOP_IDLE) == 0u);
    }
    return !output_compare_pmd_disabled(cpu, channel) &&
           (cpu->power_state != DSPIC33_POWER_IDLE || (control1 & OUTPUT_COMPARE_STOP_IDLE) == 0u);
}

static bool output_compare_internal_event(const Dspic33Event* event, uint8_t channel) {
    return event->type == DSPIC33_EVENT_OUTPUT_COMPARE && event->source == channel &&
           (event->value & OUTPUT_COMPARE_EVENT_KIND_MASK) != OUTPUT_COMPARE_EVENT_PMD;
}

static void output_compare_pause_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
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
    size_t index;
    bool changed = false;
    if (!output_compare_operating(cpu, channel)) {
        return;
    }
    for (index = 0u; index < cpu->events.count; index++) {
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

static void output_compare_update_power_state(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        if (output_compare_operating(cpu, channel)) {
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
                output_compare_raise(cpu, channel);
            }
        }
    }
}

static bool output_compare_fp_clocked(const Dspic33* cpu, uint8_t channel) {
    return (raw_word(cpu, output_compare_base(channel)) & OUTPUT_COMPARE_TIMER_SOURCE_MASK) ==
           OUTPUT_COMPARE_TIMER_SOURCE_FP;
}

static uint8_t output_compare_timer_source(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t timers[] = {1u, 2u, 3u, 4u, 0u};
    uint16_t selection =
        raw_word(cpu, output_compare_base(channel)) & OUTPUT_COMPARE_TIMER_SOURCE_MASK;
    return timers[selection >> 10u];
}

static void output_compare_set_high(Dspic33* cpu, uint8_t channel, bool high) {
    uint16_t bit = (uint16_t)(1u << channel);
    if (high) {
        cpu->io.output_compare.output_high |= bit;
    } else {
        cpu->io.output_compare.output_high &= (uint16_t)~bit;
    }
}

static bool output_compare_high(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.output_compare.output_high & (uint16_t)(1u << channel)) != 0u;
}

static uint16_t output_compare_fault_enable(uint8_t source) {
    return (uint16_t)(0x0080u << source);
}

static uint16_t output_compare_fault_status(uint8_t source) {
    return (uint16_t)(0x0010u << source);
}

static bool output_compare_fault_capable(const Dspic33* cpu, uint8_t channel) {
    uint16_t mode = raw_word(cpu, output_compare_base(channel)) & OUTPUT_COMPARE_MODE_MASK;
    return output_compare_supported(cpu, channel) && !output_compare_pmd_disabled(cpu, channel) &&
           (mode == OUTPUT_COMPARE_MODE_EDGE_PWM || mode == OUTPUT_COMPARE_MODE_CENTER_PWM);
}

static uint16_t output_compare_active_fault_status(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 = raw_word(cpu, output_compare_base(channel));
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
    uint16_t base = output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) | active));
    if ((cpu->io.output_compare.fault_held & bit) != 0u) {
        return;
    }
    cpu->io.output_compare.fault_held |= bit;
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        cpu->io.output_compare.fault_interrupt_pending |= bit;
    } else {
        output_compare_raise(cpu, channel);
    }
}

static void output_compare_refresh_fault(Dspic33* cpu, uint8_t channel) {
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
    uint16_t base = output_compare_base(channel);
    uint16_t control1 = raw_word(cpu, base);
    uint16_t control2 = raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t active = output_compare_active_fault_status(cpu, channel);
    uint16_t status = control1 & OUTPUT_COMPARE_FAULT_STATUS_MASK;
    uint16_t bit = (uint16_t)(1u << channel);
    if ((control2 & OUTPUT_COMPARE_FAULT_INACTIVE) == 0u) {
        status = active;
        raw_write_word(cpu, base,
                       (uint16_t)((control1 & ~OUTPUT_COMPARE_FAULT_STATUS_MASK) | status));
    } else if (active != 0u) {
        status |= active;
        raw_write_word(cpu, base, (uint16_t)(control1 | active));
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
        output_compare_refresh_fault(cpu, channel);
    }
}

static void output_compare_fault_input(Dspic33* cpu, uint8_t source, bool high) {
    cpu->io.output_compare.fault_direct_mask |= (uint8_t)(1u << source);
    output_compare_set_fault_input(cpu, source, high);
}

static uint8_t output_compare_fault_pps_pin(const Dspic33* cpu, uint8_t source) {
    uint16_t mapping;
    if (source < 2u) {
        mapping = raw_word(cpu, OUTPUT_COMPARE_FAULT_PPS_AB);
        return source == 0u ? (uint8_t)(mapping & 0x007fu) : (uint8_t)((mapping >> 8u) & 0x007fu);
    }
    return (uint8_t)(raw_word(cpu, OUTPUT_COMPARE_FAULT_PPS_C) & 0x007fu);
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
    return pps_physical_input_high(cpu, selection, high);
}

static void output_compare_refresh_fault_pps_inputs(Dspic33* cpu) {
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
    if (output_compare_cascade_owner(cpu, channel)) {
        uint8_t low = output_compare_pair_low(channel);
        uint8_t high = output_compare_pair_high(channel);
        uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
        uint8_t member;
        for (member = low; member <= high; member++) {
            uint16_t member_base = output_compare_base(member);
            cpu->io.output_compare.generation[member]++;
            cpu->io.output_compare.timer_generation[member]++;
            raw_write_word(cpu, member_base,
                           (uint16_t)(raw_word(cpu, member_base) & ~OUTPUT_COMPARE_MODE_MASK));
            raw_write_word(cpu, (uint16_t)(member_base + 8u), 0u);
            output_compare_set_high(cpu, member, false);
            cpu->io.output_compare.phase[member] = 0u;
            cpu->io.output_compare.sync_emitted[member] = false;
            cpu->io.output_compare.activation_cycle[member] = 0u;
        }
        cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
        cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    uint16_t base = output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    raw_write_word(cpu, base, (uint16_t)(raw_word(cpu, base) & ~OUTPUT_COMPARE_MODE_MASK));
    raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    output_compare_set_high(cpu, channel, false);
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
    uint16_t control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
    uint16_t source = control2 & OUTPUT_COMPARE_SYNC_MASK;
    return (control2 & OUTPUT_COMPARE_TRIGGER) == 0u &&
           (source == OUTPUT_COMPARE_SYNC_SELF ||
            (channel < 9u && source == OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
}

static bool output_compare_internal_period(const Dspic33* cpu, uint8_t channel) {
    return output_compare_self_synchronized(cpu, channel) ||
           (raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u)) &
            OUTPUT_COMPARE_TRIGGER) != 0u;
}

static void output_compare_pulse_source(Dspic33* cpu, uint8_t source);

static uint32_t output_compare_cascade_timer(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    return (uint32_t)raw_word(cpu, (uint16_t)(output_compare_base(low) + 8u)) |
           ((uint32_t)raw_word(cpu, (uint16_t)(output_compare_base(high) + 8u)) << 16u);
}

static void output_compare_write_cascade_timer(Dspic33* cpu, uint8_t channel, uint32_t timer) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    raw_write_word(cpu, (uint16_t)(output_compare_base(low) + 8u), (uint16_t)timer);
    raw_write_word(cpu, (uint16_t)(output_compare_base(high) + 8u), (uint16_t)(timer >> 16u));
}

static uint32_t output_compare_input_capture_timer(const Dspic33* cpu, uint8_t source) {
    uint8_t channel = (uint8_t)(source - OUTPUT_COMPARE_SYNC_IC_FIRST);
    uint32_t timer = cpu->io.input_capture.timer[channel];
    if ((channel & 1u) == 0u && input_capture_pair_configured(cpu, channel)) {
        timer |= (uint32_t)cpu->io.input_capture.timer[channel + 1u] << 16u;
    }
    return timer;
}

static void output_compare_adopt_input_capture_timer(Dspic33* cpu, uint8_t channel,
                                                     uint8_t source) {
    uint32_t timer = output_compare_input_capture_timer(cpu, source);
    if (output_compare_cascade_owner(cpu, channel)) {
        output_compare_write_cascade_timer(cpu, channel, timer);
    } else {
        raw_write_word(cpu, (uint16_t)(output_compare_base(channel) + 8u), (uint16_t)timer);
    }
}

static uint32_t output_compare_cascade_r(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    return (uint32_t)cpu->io.output_compare.active_r[low] |
           ((uint32_t)cpu->io.output_compare.active_r[high] << 16u);
}

static uint32_t output_compare_cascade_rs(const Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    return (uint32_t)cpu->io.output_compare.active_rs[low] |
           ((uint32_t)cpu->io.output_compare.active_rs[high] << 16u);
}

static bool output_compare_cascade_pwm_mode(uint16_t mode) {
    return mode == OUTPUT_COMPARE_MODE_EDGE_PWM || mode == OUTPUT_COMPARE_MODE_CENTER_PWM;
}

static bool output_compare_cascade_pwm_degenerate(const Dspic33* cpu, uint8_t channel,
                                                  uint16_t mode) {
    uint8_t high = output_compare_pair_high(channel);
    return output_compare_cascade_pwm_mode(mode) && (cpu->io.output_compare.active_r[high] == 0u ||
                                                     cpu->io.output_compare.active_rs[high] == 0u);
}

static uint64_t output_compare_boundary_delay(const Dspic33* cpu, uint8_t channel) {
    if (output_compare_cascade_owner(cpu, channel)) {
        uint8_t high = output_compare_pair_high(channel);
        uint16_t mode = raw_word(cpu, output_compare_base(channel)) & OUTPUT_COMPARE_MODE_MASK;
        uint32_t timer = output_compare_cascade_timer(cpu, channel);
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
    uint16_t base = output_compare_base(channel);
    uint16_t timer = raw_word(cpu, (uint16_t)(base + 8u));
    uint64_t delay = UINT32_C(0x10000) - timer;
    if (output_compare_internal_period(cpu, channel) &&
        timer <= cpu->io.output_compare.active_rs[channel]) {
        delay = (uint32_t)cpu->io.output_compare.active_rs[channel] + 1u - timer;
    }
    return delay;
}

static uint64_t output_compare_next_timer_event(const Dspic33* cpu, uint8_t channel,
                                                uint32_t* kind) {
    uint16_t base = output_compare_base(channel);
    uint16_t mode = raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK;
    uint16_t r = cpu->io.output_compare.active_r[channel];
    uint16_t rs = cpu->io.output_compare.active_rs[channel];
    uint16_t timer = raw_word(cpu, (uint16_t)(base + 8u));
    uint64_t boundary = output_compare_boundary_delay(cpu, channel);
    uint32_t target = r;
    uint32_t next_kind = OUTPUT_COMPARE_EVENT_PRIMARY;
    uint64_t delay = boundary;
    bool eligible = false;
    if ((cpu->io.output_compare.sync_reset_pending & (uint16_t)(1u << channel)) != 0u) {
        *kind = OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
        return 1u;
    }
    if ((raw_word(cpu, (uint16_t)(base + 2u)) &
         (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRIGGER_STATUS)) == OUTPUT_COMPARE_TRIGGER) {
        *kind = OUTPUT_COMPARE_EVENT_BOUNDARY;
        return UINT64_MAX;
    }
    if (output_compare_cascade_owner(cpu, channel)) {
        uint32_t cascade_r = output_compare_cascade_r(cpu, channel);
        uint32_t cascade_rs = output_compare_cascade_rs(cpu, channel);
        uint32_t cascade_timer = output_compare_cascade_timer(cpu, channel);
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

static bool output_compare_schedule_next(Dspic33* cpu, uint8_t channel, uint64_t initial_delay) {
    uint32_t kind;
    uint64_t delay;
    if (!output_compare_fp_clocked(cpu, channel)) {
        return true;
    }
    delay = output_compare_next_timer_event(cpu, channel, &kind);
    return delay == UINT64_MAX ||
           output_compare_schedule(cpu, channel, kind, initial_delay + delay);
}

static void output_compare_start(Dspic33* cpu, uint8_t channel) {
    uint16_t base = output_compare_base(channel);
    uint16_t bit = (uint16_t)(1u << channel);
    uint64_t activation_delay =
        cpu->instruction_active
            ? dspic33_device_instruction_cycles(cpu, cpu->current_instruction_cycles)
            : 0u;
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    cpu->io.output_compare.active_rs[channel] = raw_word(cpu, (uint16_t)(base + 4u));
    cpu->io.output_compare.active_r[channel] = raw_word(cpu, (uint16_t)(base + 6u));
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    if (cpu->instruction_active) {
        cpu->io.output_compare.activation_pending |= bit;
    } else {
        cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    }
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] = cpu->device_cycles + activation_delay;
    raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    output_compare_set_high(
        cpu, channel,
        (raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK) == OUTPUT_COMPARE_MODE_SINGLE_LOW ||
            ((raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK) == OUTPUT_COMPARE_MODE_EDGE_PWM &&
             cpu->io.output_compare.active_r[channel] != 0u));
    output_compare_schedule_next(cpu, channel, activation_delay);
}

static void output_compare_start_cascade(Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
    uint64_t activation_delay =
        cpu->instruction_active
            ? dspic33_device_instruction_cycles(cpu, cpu->current_instruction_cycles)
            : 0u;
    cpu->io.output_compare.generation[low]++;
    cpu->io.output_compare.generation[high]++;
    cpu->io.output_compare.timer_generation[low]++;
    cpu->io.output_compare.timer_generation[high]++;
    cpu->io.output_compare.active_rs[low] = raw_word(cpu, (uint16_t)(low_base + 4u));
    cpu->io.output_compare.active_rs[high] = raw_word(cpu, (uint16_t)(high_base + 4u));
    cpu->io.output_compare.active_r[low] = raw_word(cpu, (uint16_t)(low_base + 6u));
    cpu->io.output_compare.active_r[high] = raw_word(cpu, (uint16_t)(high_base + 6u));
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
    output_compare_write_cascade_timer(cpu, low, 0u);
    output_compare_set_high(cpu, low, false);
    output_compare_set_high(cpu, high,
                            (raw_word(cpu, low_base) & OUTPUT_COMPARE_MODE_MASK) ==
                                OUTPUT_COMPARE_MODE_SINGLE_LOW);
    output_compare_schedule_next(cpu, low, activation_delay);
}

static void output_compare_stop(Dspic33* cpu, uint8_t channel) {
    uint16_t bit = (uint16_t)(1u << channel);
    cpu->io.output_compare.generation[channel]++;
    cpu->io.output_compare.timer_generation[channel]++;
    raw_write_word(cpu, (uint16_t)(output_compare_base(channel) + 8u), 0u);
    output_compare_set_high(cpu, channel, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~bit;
    cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
    cpu->io.output_compare.phase[channel] = 0u;
    cpu->io.output_compare.sync_emitted[channel] = false;
    cpu->io.output_compare.activation_cycle[channel] = 0u;
    if (channel < 9u) {
        output_compare_pulse_source(cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
    }
}

static void output_compare_stop_cascade(Dspic33* cpu, uint8_t channel) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t pair_bits = (uint16_t)((1u << low) | (1u << high));
    cpu->io.output_compare.generation[low]++;
    cpu->io.output_compare.generation[high]++;
    cpu->io.output_compare.timer_generation[low]++;
    cpu->io.output_compare.timer_generation[high]++;
    output_compare_write_cascade_timer(cpu, low, 0u);
    output_compare_set_high(cpu, low, false);
    output_compare_set_high(cpu, high, false);
    cpu->io.output_compare.sync_reset_pending &= (uint16_t)~pair_bits;
    cpu->io.output_compare.activation_pending &= (uint16_t)~pair_bits;
    cpu->io.output_compare.phase[low] = 0u;
    cpu->io.output_compare.phase[high] = 0u;
    cpu->io.output_compare.sync_emitted[low] = false;
    cpu->io.output_compare.sync_emitted[high] = false;
    cpu->io.output_compare.activation_cycle[low] = 0u;
    cpu->io.output_compare.activation_cycle[high] = 0u;
    if (low < 9u) {
        output_compare_pulse_source(cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + low));
    }
}

static void output_compare_raise(Dspic33* cpu, uint8_t channel) {
    uint8_t output = output_compare_output_channel(cpu, channel);
    if (output < 4u) {
        dspic33_dma_request(cpu, output_compare_irqs[output], 0u, 0u);
    }
    dspic33_raise_interrupt(cpu, output_compare_irqs[output]);
}

static void output_compare_pulse_source(Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = output_compare_base(channel);
        uint16_t control2 = raw_word(cpu, (uint16_t)(base + 2u));
        uint16_t bit = (uint16_t)(1u << channel);
        if (!output_compare_timer_owner(cpu, channel) || !output_compare_operating(cpu, channel) ||
            (control2 & OUTPUT_COMPARE_SYNC_MASK) != source ||
            output_compare_self_synchronized(cpu, channel)) {
            continue;
        }
        if ((control2 & OUTPUT_COMPARE_TRIGGER) != 0u) {
            if ((control2 & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u) {
                continue;
            }
            raw_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(control2 | OUTPUT_COMPARE_TRIGGER_STATUS));
        } else {
            if ((cpu->io.output_compare.sync_reset_pending & bit) != 0u) {
                continue;
            }
            cpu->io.output_compare.sync_reset_pending |= bit;
        }
        cpu->io.output_compare.timer_generation[channel]++;
        output_compare_schedule_next(cpu, channel, 0u);
    }
}

static bool output_compare_source_awaited(const Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
        if (output_compare_timer_owner(cpu, channel) && output_compare_operating(cpu, channel) &&
            (control2 & OUTPUT_COMPARE_SYNC_MASK) == source &&
            ((control2 & OUTPUT_COMPARE_TRIGGER) == 0u ||
             (control2 & OUTPUT_COMPARE_TRIGGER_STATUS) == 0u)) {
            return true;
        }
    }
    return false;
}

static void output_compare_pulse_sync_source(Dspic33* cpu, uint8_t channel) {
    if (channel < 9u) {
        uint8_t source = (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel);
        input_capture_pulse_source(cpu, source);
        if (cpu->io.output_compare.clock_advancing) {
            cpu->io.output_compare.deferred_sync_pulses |= (uint16_t)(1u << channel);
        } else {
            output_compare_pulse_source(cpu, source);
        }
    }
}

static bool output_compare_primary_match(Dspic33* cpu, uint8_t channel, uint16_t mode) {
    uint8_t output = output_compare_output_channel(cpu, channel);
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
        if (output_compare_cascade_owner(cpu, channel) &&
            output_compare_cascade_pwm_degenerate(cpu, channel, mode)) {
            cpu->io.output_compare.phase[channel] = 1u;
        }
        output_compare_set_high(cpu, output, output_compare_cascade_owner(cpu, channel));
        return true;
    }
    if (mode <= OUTPUT_COMPARE_MODE_SINGLE_TOGGLE &&
        !output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_INTERRUPT, 3u)) {
        return false;
    }
    return output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_APPLY_PRIMARY, 1u);
}

static bool output_compare_secondary_match(Dspic33* cpu, uint8_t channel, uint16_t mode) {
    cpu->io.output_compare.phase[channel] = mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ? 2u : 0u;
    return output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_INTERRUPT, 3u) &&
           output_compare_schedule(cpu, channel, OUTPUT_COMPARE_EVENT_APPLY_SECONDARY, 1u);
}

static bool output_compare_boundary(Dspic33* cpu, uint8_t channel, uint16_t mode) {
    uint16_t base = output_compare_base(channel);
    uint8_t output = output_compare_output_channel(cpu, channel);
    bool cascade = output_compare_cascade_owner(cpu, channel);
    bool degenerate = cascade && output_compare_cascade_pwm_degenerate(cpu, channel, mode);
    bool first_low_period = degenerate && cpu->io.output_compare.active_r[output] == 0u &&
                            cpu->io.output_compare.active_rs[output] == 0u &&
                            cpu->io.output_compare.phase[channel] == 1u &&
                            (output_compare_cascade_timer(cpu, channel) >> 16u) == 0u;
    if (cascade) {
        output_compare_write_cascade_timer(cpu, channel, first_low_period ? UINT32_C(0x10000) : 0u);
    } else {
        raw_write_word(cpu, (uint16_t)(base + 8u), 0u);
    }
    cpu->io.output_compare.sync_emitted[channel] = false;
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM || mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        if (cascade) {
            uint8_t high = output_compare_pair_high(channel);
            uint16_t high_base = output_compare_base(high);
            cpu->io.output_compare.active_rs[channel] = raw_word(cpu, (uint16_t)(base + 4u));
            cpu->io.output_compare.active_rs[high] = raw_word(cpu, (uint16_t)(high_base + 4u));
            cpu->io.output_compare.active_r[channel] = raw_word(cpu, (uint16_t)(base + 6u));
            cpu->io.output_compare.active_r[high] = raw_word(cpu, (uint16_t)(high_base + 6u));
        } else {
            cpu->io.output_compare.active_rs[channel] = raw_word(cpu, (uint16_t)(base + 4u));
            cpu->io.output_compare.active_r[channel] = raw_word(cpu, (uint16_t)(base + 6u));
        }
    }
    if (mode == OUTPUT_COMPARE_MODE_EDGE_PWM) {
        output_compare_set_high(cpu, output,
                                degenerate ||
                                    (!cascade && cpu->io.output_compare.active_r[channel] != 0u));
        output_compare_raise(cpu, channel);
        if (degenerate) {
            cpu->io.output_compare.phase[channel] = 0u;
        }
    } else if (mode == OUTPUT_COMPARE_MODE_CENTER_PWM && degenerate) {
        output_compare_set_high(cpu, output, true);
        cpu->io.output_compare.phase[channel] = 0u;
        output_compare_raise(cpu, channel);
    } else if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
        cpu->io.output_compare.phase[channel] = 0u;
        if ((output_compare_cascade_owner(cpu, channel)
                 ? output_compare_cascade_r(cpu, channel) == 0u
                 : cpu->io.output_compare.active_r[channel] == 0u) &&
            !output_compare_self_synchronized(cpu, channel) &&
            !output_compare_primary_match(cpu, channel, mode)) {
            return false;
        }
    } else if ((mode == OUTPUT_COMPARE_MODE_SINGLE_HIGH ||
                mode == OUTPUT_COMPARE_MODE_SINGLE_LOW) &&
               cpu->io.output_compare.phase[channel] == 0u &&
               (output_compare_cascade_owner(cpu, channel)
                    ? output_compare_cascade_r(cpu, channel) == 0u
                    : cpu->io.output_compare.active_r[channel] == 0u) &&
               !output_compare_self_synchronized(cpu, channel) &&
               !output_compare_primary_match(cpu, channel, mode)) {
        return false;
    } else if (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE ||
               mode == OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS ||
               mode == OUTPUT_COMPARE_MODE_CENTER_PWM) {
        uint32_t r = output_compare_cascade_owner(cpu, channel)
                         ? output_compare_cascade_r(cpu, channel)
                         : cpu->io.output_compare.active_r[channel];
        uint32_t rs = output_compare_cascade_owner(cpu, channel)
                          ? output_compare_cascade_rs(cpu, channel)
                          : cpu->io.output_compare.active_rs[channel];
        if (r == 0u && rs == 0u) {
            output_compare_set_high(cpu, output, false);
            cpu->io.output_compare.phase[channel] = 0u;
        } else if (cpu->io.output_compare.phase[channel] == 1u && rs == 0u) {
            if (!output_compare_secondary_match(cpu, channel, mode)) {
                return false;
            }
        } else if (cpu->io.output_compare.phase[channel] == 0u && r == 0u) {
            if (!output_compare_primary_match(cpu, channel, mode)) {
                return false;
            }
        }
    }
    if (output_compare_internal_period(cpu, channel)) {
        output_compare_pulse_sync_source(cpu, channel);
    }
    if ((raw_word(cpu, base) & OUTPUT_COMPARE_TRIGGER_ONESHOT) != 0u &&
        (raw_word(cpu, (uint16_t)(base + 2u)) & OUTPUT_COMPARE_TRIGGER) != 0u) {
        raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(raw_word(cpu, (uint16_t)(base + 2u)) & ~OUTPUT_COMPARE_TRIGGER_STATUS));
    }
    output_compare_fault_boundary(cpu, channel);
    if (cascade) {
        output_compare_fault_boundary(cpu, output_compare_pair_high(channel));
    }
    return true;
}

static void run_output_compare(Dspic33* cpu, uint16_t source, uint32_t value) {
    uint16_t generation;
    uint8_t channel;
    uint32_t kind;
    bool timer_event;
    if (source >= DSPIC33_OUTPUT_COMPARE_COUNT) {
        return;
    }
    channel = (uint8_t)source;
    kind = value & OUTPUT_COMPARE_EVENT_KIND_MASK;
    if (kind == OUTPUT_COMPARE_EVENT_PMD) {
        uint16_t pmd_generation = (uint16_t)((value & ~OUTPUT_COMPARE_EVENT_PMD_DISABLED) >>
                                             OUTPUT_COMPARE_EVENT_PMD_GENERATION_SHIFT);
        uint16_t bit = (uint16_t)(1u << channel);
        if (pmd_generation != cpu->io.output_compare.pmd_generation[channel]) {
            return;
        }
        if ((value & OUTPUT_COMPARE_EVENT_PMD_DISABLED) != 0u) {
            cpu->io.output_compare.pmd_disabled |= bit;
        } else {
            cpu->io.output_compare.pmd_disabled &= (uint16_t)~bit;
        }
        output_compare_refresh_fault(cpu, channel);
        if (output_compare_cascade_requested(cpu, channel)) {
            output_compare_refresh_fault(cpu, output_compare_pair_low(channel));
            output_compare_refresh_fault(cpu, output_compare_pair_high(channel));
        }
        output_compare_update_power_state(cpu);
        return;
    }
    timer_event = kind == OUTPUT_COMPARE_EVENT_PRIMARY || kind == OUTPUT_COMPARE_EVENT_SECONDARY ||
                  kind == OUTPUT_COMPARE_EVENT_BOUNDARY || kind == OUTPUT_COMPARE_EVENT_SYNC ||
                  kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY ||
                  kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY ||
                  kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
    generation = (uint16_t)(value >> OUTPUT_COMPARE_EVENT_GENERATION_SHIFT);
    if (generation != (timer_event ? cpu->io.output_compare.timer_generation[channel]
                                   : cpu->io.output_compare.generation[channel]) ||
        !output_compare_operating(cpu, channel)) {
        return;
    }
    {
        uint16_t base = output_compare_base(channel);
        uint16_t mode = raw_word(cpu, base) & OUTPUT_COMPARE_MODE_MASK;
        if (kind == OUTPUT_COMPARE_EVENT_PRIMARY) {
            if (!output_compare_primary_match(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_SECONDARY) {
            if (!output_compare_secondary_match(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_BOUNDARY) {
            if (!output_compare_boundary(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_SYNC) {
            cpu->io.output_compare.sync_emitted[channel] = true;
            output_compare_pulse_sync_source(cpu, channel);
        } else if (kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY) {
            if (!output_compare_boundary(cpu, channel, mode)) {
                return;
            }
            output_compare_pulse_sync_source(cpu, channel);
        } else if (kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY) {
            cpu->io.output_compare.sync_emitted[channel] = true;
            output_compare_pulse_sync_source(cpu, channel);
            if (!output_compare_primary_match(cpu, channel, mode)) {
                return;
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC) {
            uint8_t synchronization =
                (uint8_t)(raw_word(cpu, (uint16_t)(base + 2u)) & OUTPUT_COMPARE_SYNC_MASK);
            cpu->io.output_compare.sync_reset_pending &= (uint16_t)~(uint16_t)(1u << channel);
            if (!output_compare_boundary(cpu, channel, mode)) {
                return;
            }
            if (synchronization >= OUTPUT_COMPARE_SYNC_IC_FIRST &&
                synchronization < OUTPUT_COMPARE_SYNC_COMPARATOR_FIRST) {
                output_compare_adopt_input_capture_timer(cpu, channel, synchronization);
            }
        } else if (kind == OUTPUT_COMPARE_EVENT_APPLY_PRIMARY) {
            uint8_t output = output_compare_output_channel(cpu, channel);
            if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
                output_compare_set_high(cpu, output, !output_compare_high(cpu, output));
            } else {
                output_compare_set_high(cpu, output, mode != OUTPUT_COMPARE_MODE_SINGLE_LOW);
            }
            return;
        } else if (kind == OUTPUT_COMPARE_EVENT_APPLY_SECONDARY) {
            output_compare_set_high(cpu, output_compare_output_channel(cpu, channel), false);
            return;
        } else {
            output_compare_raise(cpu, channel);
            return;
        }
        output_compare_schedule_next(cpu, channel, 0u);
    }
}

static uint16_t output_compare_previous_word(const Dspic33* cpu, uint16_t changed_address,
                                             uint16_t previous, uint16_t address) {
    return address == changed_address ? previous : raw_word(cpu, address);
}

static bool output_compare_cascade_supported_before(const Dspic33* cpu, uint8_t channel,
                                                    uint16_t changed_address, uint16_t previous) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t low_control1 = output_compare_previous_word(cpu, changed_address, previous, low_base);
    uint16_t low_control2 =
        output_compare_previous_word(cpu, changed_address, previous, (uint16_t)(low_base + 2u));
    uint16_t high_control1 =
        output_compare_previous_word(cpu, changed_address, previous, high_base);
    uint16_t high_control2 =
        output_compare_previous_word(cpu, changed_address, previous, (uint16_t)(high_base + 2u));
    return output_compare_cascade_controls_supported(low, low_control1, low_control2) &&
           output_compare_cascade_controls_supported(high, high_control1, high_control2) &&
           (low_control1 & (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) ==
               (high_control1 & (OUTPUT_COMPARE_TIMER_SOURCE_MASK | OUTPUT_COMPARE_MODE_MASK)) &&
           (low_control2 & OUTPUT_COMPARE_SYNC_MASK) ==
               (high_control2 & OUTPUT_COMPARE_SYNC_MASK) &&
           (low_control2 & OUTPUT_COMPARE_TRISTATE) != 0u &&
           (high_control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRISTATE)) == 0u;
}

static bool output_compare_update_cascade(Dspic33* cpu, uint8_t channel, uint16_t address,
                                          uint16_t previous) {
    uint8_t low = output_compare_pair_low(channel);
    uint8_t high = output_compare_pair_high(channel);
    uint16_t low_base = output_compare_base(low);
    uint16_t high_base = output_compare_base(high);
    uint16_t offset = (uint16_t)(address - output_compare_base(channel));
    bool was_supported = output_compare_cascade_supported_before(cpu, channel, address, previous);
    bool is_supported = output_compare_cascade_supported(cpu, channel);
    bool cascade_involved = was_supported || is_supported ||
                            output_compare_cascade_requested(cpu, low) ||
                            output_compare_cascade_requested(cpu, high) ||
                            (offset == 2u && (previous & OUTPUT_COMPARE_32_BIT) != 0u);
    if (!cascade_involved) {
        return false;
    }
    if (!was_supported && is_supported) {
        output_compare_start_cascade(cpu, low);
    } else if (was_supported && !is_supported) {
        output_compare_stop_cascade(cpu, low);
    } else if (was_supported && is_supported) {
        uint16_t mode = raw_word(cpu, low_base) & OUTPUT_COMPARE_MODE_MASK;
        if (offset == 0u || offset == 2u) {
            uint16_t changed = (uint16_t)(raw_word(cpu, address) ^ previous);
            if ((offset == 0u &&
                 (changed & ~(OUTPUT_COMPARE_STOP_IDLE | OUTPUT_COMPARE_FAULT_ENABLE_MASK |
                              OUTPUT_COMPARE_FAULT_STATUS_MASK)) == 0u) ||
                (offset == 2u &&
                 (changed & ~(OUTPUT_COMPARE_INVERT | OUTPUT_COMPARE_TRISTATE |
                              OUTPUT_COMPARE_FAULT_INACTIVE | OUTPUT_COMPARE_FAULT_OUTPUT |
                              OUTPUT_COMPARE_FAULT_TRISTATE)) == 0u)) {
                output_compare_update_power_state(cpu);
            } else {
                output_compare_start_cascade(cpu, low);
            }
        } else if (offset == 4u || offset == 6u) {
            if (mode != OUTPUT_COMPARE_MODE_EDGE_PWM && mode != OUTPUT_COMPARE_MODE_CENTER_PWM) {
                uint16_t value = raw_word(cpu, address);
                if (offset == 4u) {
                    cpu->io.output_compare.active_rs[channel] = value;
                    cpu->io.output_compare.sync_emitted[low] = false;
                } else {
                    cpu->io.output_compare.active_r[channel] = value;
                }
                cpu->io.output_compare.timer_generation[low]++;
                output_compare_schedule_next(cpu, low, 0u);
            }
        }
    }
    if (was_supported && !is_supported) {
        uint16_t control1 = raw_word(cpu, output_compare_base(channel));
        uint16_t control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
        if (output_compare_configuration_supported(channel, control1, control2)) {
            output_compare_start(cpu, channel);
        }
    }
    if (address == low_base || address == high_base) {
        output_compare_update_power_state(cpu);
    }
    output_compare_refresh_fault(cpu, low);
    output_compare_refresh_fault(cpu, high);
    return true;
}

static void update_output_compare_register(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t base;
    uint16_t control1;
    uint16_t control2;
    uint16_t previous1;
    uint16_t previous2;
    uint16_t offset;
    uint8_t channel;
    bool was_supported;
    bool is_supported;
    if (address < OUTPUT_COMPARE_BASE ||
        address >= OUTPUT_COMPARE_BASE + DSPIC33_OUTPUT_COMPARE_COUNT * OUTPUT_COMPARE_STRIDE) {
        return;
    }
    channel = (uint8_t)((address - OUTPUT_COMPARE_BASE) / OUTPUT_COMPARE_STRIDE);
    base = output_compare_base(channel);
    offset = (uint16_t)(address - base);
    if (output_compare_pmd_disabled(cpu, channel) && offset <= 8u) {
        raw_write_word(cpu, (uint16_t)(base + offset), previous);
        return;
    }
    if (offset != 0u && offset != 2u && offset != 4u && offset != 6u) {
        return;
    }
    if (output_compare_update_cascade(cpu, channel, address, previous)) {
        return;
    }
    control1 = raw_word(cpu, base);
    control2 = raw_word(cpu, (uint16_t)(base + 2u));
    previous1 = offset == 0u ? previous : control1;
    previous2 = offset == 2u ? previous : control2;
    if (offset == 2u && (control1 & OUTPUT_COMPARE_TRIGGER_ONESHOT) != 0u &&
        (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u &&
        (control1 & OUTPUT_COMPARE_MODE_MASK) != 0u && (control2 & OUTPUT_COMPARE_TRIGGER) != 0u &&
        ((control2 ^ previous2) & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u) {
        control2 = (uint16_t)((control2 & ~OUTPUT_COMPARE_TRIGGER_STATUS) |
                              (previous2 & OUTPUT_COMPARE_TRIGGER_STATUS));
        raw_write_word(cpu, (uint16_t)(base + 2u), control2);
    }
    was_supported = output_compare_configuration_supported(channel, previous1, previous2);
    is_supported = output_compare_configuration_supported(channel, control1, control2);
    if (!was_supported && is_supported) {
        output_compare_start(cpu, channel);
    } else if (was_supported && !is_supported) {
        output_compare_stop(cpu, channel);
    } else if (was_supported && is_supported) {
        uint16_t mode = control1 & OUTPUT_COMPARE_MODE_MASK;
        bool mode_changed = (previous1 & OUTPUT_COMPARE_MODE_MASK) != mode;
        bool trigger_changed = ((previous2 ^ control2) & OUTPUT_COMPARE_TRIGGER) != 0u;
        bool trigger_status_changed =
            ((previous2 ^ control2) & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u;
        bool clock_changed = ((previous1 ^ control1) & OUTPUT_COMPARE_TIMER_SOURCE_MASK) != 0u;
        bool low_control_written =
            offset == 0u && (!cpu->io.cpu_write_valid ||
                             (cpu->io.cpu_write_address <= base &&
                              cpu->io.cpu_write_address + cpu->io.cpu_write_width > base));
        if (mode_changed || trigger_changed ||
            (mode == OUTPUT_COMPARE_MODE_DUAL_SINGLE && low_control_written)) {
            output_compare_start(cpu, channel);
        } else if (clock_changed || trigger_status_changed ||
                   (previous2 & OUTPUT_COMPARE_SYNC_MASK) !=
                       (control2 & OUTPUT_COMPARE_SYNC_MASK) ||
                   ((offset == 4u || offset == 6u) && mode != OUTPUT_COMPARE_MODE_EDGE_PWM &&
                    mode != OUTPUT_COMPARE_MODE_CENTER_PWM)) {
            if ((previous2 & OUTPUT_COMPARE_SYNC_MASK) != (control2 & OUTPUT_COMPARE_SYNC_MASK) ||
                trigger_status_changed) {
                cpu->io.output_compare.sync_emitted[channel] = false;
            }
            if ((previous2 & OUTPUT_COMPARE_SYNC_MASK) != (control2 & OUTPUT_COMPARE_SYNC_MASK) ||
                (trigger_status_changed && (control2 & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u)) {
                cpu->io.output_compare.sync_reset_pending &= (uint16_t)~(uint16_t)(1u << channel);
            } else if (trigger_status_changed) {
                cpu->io.output_compare.sync_reset_pending |= (uint16_t)(1u << channel);
            }
            if (offset == 4u) {
                cpu->io.output_compare.active_rs[channel] = raw_word(cpu, (uint16_t)(base + 4u));
                cpu->io.output_compare.sync_emitted[channel] = false;
            } else if (offset == 6u) {
                cpu->io.output_compare.active_r[channel] = raw_word(cpu, (uint16_t)(base + 6u));
            }
            if (trigger_status_changed) {
                uint64_t activation_delay =
                    cpu->instruction_active
                        ? dspic33_device_instruction_cycles(cpu, cpu->current_instruction_cycles)
                        : 0u;
                uint16_t bit = (uint16_t)(1u << channel);
                cpu->io.output_compare.activation_cycle[channel] =
                    cpu->device_cycles + activation_delay;
                if (cpu->instruction_active) {
                    cpu->io.output_compare.activation_pending |= bit;
                } else {
                    cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
                }
                cpu->io.output_compare.timer_generation[channel]++;
                output_compare_schedule_next(cpu, channel, activation_delay);
                return;
            }
            cpu->io.output_compare.timer_generation[channel]++;
            output_compare_schedule_next(cpu, channel, 0u);
        }
    }
    if (offset == 0u && ((previous1 ^ control1) & OUTPUT_COMPARE_STOP_IDLE) != 0u &&
        cpu->power_state == DSPIC33_POWER_IDLE) {
        output_compare_update_power_state(cpu);
    }
    output_compare_refresh_fault(cpu, channel);
}

static void update_output_compare_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint8_t first_channel;
    uint8_t channel;
    uint16_t changed;
    uint16_t current;
    if (address == 0x0762u) {
        first_channel = 0u;
    } else if (address == 0x0768u) {
        first_channel = 8u;
    } else {
        return;
    }
    current = raw_word(cpu, address);
    changed = (uint16_t)((previous ^ current) & 0x00ffu);
    for (channel = first_channel; channel < first_channel + 8u; channel++) {
        uint16_t register_mask = (uint16_t)(1u << (channel - first_channel));
        if ((changed & register_mask) == 0u) {
            continue;
        }
        cpu->io.output_compare.pmd_generation[channel]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_OUTPUT_COMPARE, channel,
                OUTPUT_COMPARE_EVENT_PMD |
                    ((current & register_mask) != 0u ? OUTPUT_COMPARE_EVENT_PMD_DISABLED : 0u) |
                    ((uint32_t)cpu->io.output_compare.pmd_generation[channel]
                     << OUTPUT_COMPARE_EVENT_PMD_GENERATION_SHIFT),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            uint8_t invalidate;
            raw_write_word(cpu, address, previous);
            for (invalidate = first_channel; invalidate < first_channel + 8u; invalidate++) {
                if ((changed & (uint16_t)(1u << (invalidate - first_channel))) != 0u) {
                    cpu->io.output_compare.pmd_generation[invalidate]++;
                }
            }
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
}

static void advance_output_compare(Dspic33* cpu, uint64_t cycles) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        if (output_compare_timer_owner(cpu, channel) && output_compare_operating(cpu, channel) &&
            output_compare_fp_clocked(cpu, channel)) {
            uint16_t address = (uint16_t)(output_compare_base(channel) + 8u);
            uint16_t control2 = raw_word(cpu, (uint16_t)(output_compare_base(channel) + 2u));
            uint64_t interval_start = cpu->device_cycles - cycles;
            uint64_t active_start = cpu->io.output_compare.activation_cycle[channel];
            uint64_t elapsed_start = interval_start > active_start ? interval_start : active_start;
            if (cpu->device_cycles > elapsed_start &&
                (control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRIGGER_STATUS)) !=
                    OUTPUT_COMPARE_TRIGGER) {
                if (output_compare_cascade_owner(cpu, channel)) {
                    output_compare_write_cascade_timer(
                        cpu, channel,
                        output_compare_cascade_timer(cpu, channel) +
                            (uint32_t)(cpu->device_cycles - elapsed_start));
                } else {
                    raw_write_word(cpu, address,
                                   (uint16_t)(raw_word(cpu, address) +
                                              (uint16_t)(cpu->device_cycles - elapsed_start)));
                }
            }
        }
    }
}

static bool output_compare_uses_timer(const Dspic33* cpu, uint8_t channel, uint8_t timer) {
    return output_compare_timer_owner(cpu, channel) && output_compare_operating(cpu, channel) &&
           !output_compare_fp_clocked(cpu, channel) &&
           output_compare_timer_source(cpu, channel) == timer &&
           (timer != 0u ||
            (raw_word(cpu, timer_controls[0]) & (TIMER_EXTERNAL | TIMER_SYNC)) != TIMER_EXTERNAL);
}

static uint64_t output_compare_clock_boundary_ticks(const Dspic33* cpu, uint8_t timer) {
    uint64_t boundary = UINT64_MAX;
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint32_t kind;
        uint64_t delay;
        if (!output_compare_uses_timer(cpu, channel, timer)) {
            continue;
        }
        delay = output_compare_next_timer_event(cpu, channel, &kind);
        if (delay < boundary) {
            boundary = delay;
        }
    }
    return boundary;
}

static void output_compare_advance_clock(Dspic33* cpu, uint8_t timer, uint64_t ticks) {
    uint8_t channel;
    uint16_t pulses;
    cpu->io.output_compare.clock_advancing = true;
    cpu->io.output_compare.deferred_sync_pulses = 0u;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base;
        uint16_t bit = (uint16_t)(1u << channel);
        uint16_t timer_address;
        uint32_t kind;
        uint64_t delay;
        if (!output_compare_uses_timer(cpu, channel, timer) ||
            ((cpu->io.output_compare.activation_pending & bit) != 0u &&
             cpu->device_cycles <= cpu->io.output_compare.activation_cycle[channel])) {
            continue;
        }
        cpu->io.output_compare.activation_pending &= (uint16_t)~bit;
        base = output_compare_base(channel);
        timer_address = (uint16_t)(base + 8u);
        delay = output_compare_next_timer_event(cpu, channel, &kind);
        if (delay == UINT64_MAX) {
            continue;
        }
        if (output_compare_cascade_owner(cpu, channel)) {
            output_compare_write_cascade_timer(
                cpu, channel, output_compare_cascade_timer(cpu, channel) + (uint32_t)ticks);
        } else {
            raw_write_word(cpu, timer_address,
                           (uint16_t)(raw_word(cpu, timer_address) + (uint16_t)ticks));
        }
        if (ticks == delay) {
            run_output_compare(cpu, channel,
                               kind | ((uint32_t)cpu->io.output_compare.timer_generation[channel]
                                       << OUTPUT_COMPARE_EVENT_GENERATION_SHIFT));
        }
    }
    cpu->io.output_compare.clock_advancing = false;
    pulses = cpu->io.output_compare.deferred_sync_pulses;
    cpu->io.output_compare.deferred_sync_pulses = 0u;
    for (channel = 0u; channel < 9u; channel++) {
        if ((pulses & (uint16_t)(1u << channel)) != 0u) {
            output_compare_pulse_source(cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
        }
    }
}

static bool output_compare_function_channel(uint8_t function, uint8_t* channel) {
    if (function >= 0x10u && function <= 0x17u) {
        *channel = (uint8_t)(function - 0x10u);
        return true;
    }
    if (function >= 0x25u && function <= 0x2cu) {
        *channel = (uint8_t)(function - 0x25u + 8u);
        return true;
    }
    return false;
}

static bool output_compare_pin_channel(const Dspic33* cpu, uint8_t pin, uint8_t* channel) {
    size_t index;
    for (index = 0u; index < sizeof(pps_outputs) / sizeof(pps_outputs[0]); index++) {
        if (pps_outputs[index].pin == pin) {
            uint8_t function =
                (uint8_t)((raw_word(cpu, pps_outputs[index].address) >> pps_outputs[index].shift) &
                          0x003fu);
            return output_compare_function_channel(function, channel);
        }
    }
    return false;
}
