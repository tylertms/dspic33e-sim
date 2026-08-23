#include "device/dspic33ep_mu/internal.h"

void dspic33_device_internal_run_output_compare(Dspic33* cpu, uint16_t source, uint32_t value) {
    uint16_t event_generation;
    uint8_t channel;
    uint32_t event_kind;
    bool is_timer_event;
    if (source >= DSPIC33_OUTPUT_COMPARE_COUNT) {
        return;
    }
    channel = (uint8_t)source;
    event_kind = value & OUTPUT_COMPARE_EVENT_KIND_MASK;
    if (event_kind == OUTPUT_COMPARE_EVENT_PMD) {
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
        dspic33_device_internal_output_compare_refresh_fault(cpu, channel);
        if (dspic33_device_internal_output_compare_cascade_requested(cpu, channel)) {
            dspic33_device_internal_output_compare_refresh_fault(
                cpu, dspic33_device_internal_output_compare_pair_low(channel));
            dspic33_device_internal_output_compare_refresh_fault(
                cpu, dspic33_device_internal_output_compare_pair_high(channel));
        }
        dspic33_device_internal_output_compare_update_power_state(cpu);
        return;
    }
    is_timer_event = event_kind == OUTPUT_COMPARE_EVENT_PRIMARY ||
                     event_kind == OUTPUT_COMPARE_EVENT_SECONDARY ||
                     event_kind == OUTPUT_COMPARE_EVENT_BOUNDARY ||
                     event_kind == OUTPUT_COMPARE_EVENT_SYNC ||
                     event_kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY ||
                     event_kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY ||
                     event_kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC;
    event_generation = (uint16_t)(value >> OUTPUT_COMPARE_EVENT_GENERATION_SHIFT);
    if (event_generation != (is_timer_event ? cpu->io.output_compare.timer_generation[channel]
                                            : cpu->io.output_compare.generation[channel]) ||
        !dspic33_device_internal_output_compare_operating(cpu, channel)) {
        return;
    }
    {
        uint16_t register_base = dspic33_device_internal_output_compare_base(channel);
        uint16_t mode =
            dspic33_device_internal_raw_word(cpu, register_base) & OUTPUT_COMPARE_MODE_MASK;
        if (event_kind == OUTPUT_COMPARE_EVENT_PRIMARY) {
            if (!dspic33_device_internal_output_compare_primary_match(cpu, channel, mode)) {
                return;
            }
        } else if (event_kind == OUTPUT_COMPARE_EVENT_SECONDARY) {
            if (!dspic33_device_internal_output_compare_secondary_match(cpu, channel, mode)) {
                return;
            }
        } else if (event_kind == OUTPUT_COMPARE_EVENT_BOUNDARY) {
            if (!dspic33_device_internal_output_compare_boundary(cpu, channel, mode)) {
                return;
            }
        } else if (event_kind == OUTPUT_COMPARE_EVENT_SYNC) {
            cpu->io.output_compare.sync_emitted[channel] = true;
            dspic33_device_internal_output_compare_pulse_sync_source(cpu, channel);
        } else if (event_kind == OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY) {
            if (!dspic33_device_internal_output_compare_boundary(cpu, channel, mode)) {
                return;
            }
            dspic33_device_internal_output_compare_pulse_sync_source(cpu, channel);
        } else if (event_kind == OUTPUT_COMPARE_EVENT_SYNC_PRIMARY) {
            cpu->io.output_compare.sync_emitted[channel] = true;
            dspic33_device_internal_output_compare_pulse_sync_source(cpu, channel);
            if (!dspic33_device_internal_output_compare_primary_match(cpu, channel, mode)) {
                return;
            }
        } else if (event_kind == OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC) {
            uint8_t synchronization =
                (uint8_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(register_base + 2u)) &
                          OUTPUT_COMPARE_SYNC_MASK);
            cpu->io.output_compare.sync_reset_pending &= (uint16_t)~(uint16_t)(1u << channel);
            if (!dspic33_device_internal_output_compare_boundary(cpu, channel, mode)) {
                return;
            }
            if (synchronization >= OUTPUT_COMPARE_SYNC_IC_FIRST &&
                synchronization < OUTPUT_COMPARE_SYNC_COMPARATOR_FIRST) {
                dspic33_device_internal_output_compare_adopt_input_capture_timer(cpu, channel,
                                                                                 synchronization);
            }
        } else if (event_kind == OUTPUT_COMPARE_EVENT_APPLY_PRIMARY) {
            uint8_t output_channel =
                dspic33_device_internal_output_compare_output_channel(cpu, channel);
            if (mode == OUTPUT_COMPARE_MODE_SINGLE_TOGGLE) {
                dspic33_device_internal_output_compare_set_high(
                    cpu, output_channel,
                    !dspic33_device_internal_output_compare_high(cpu, output_channel));
            } else {
                dspic33_device_internal_output_compare_set_high(
                    cpu, output_channel, mode != OUTPUT_COMPARE_MODE_SINGLE_LOW);
            }
            return;
        } else if (event_kind == OUTPUT_COMPARE_EVENT_APPLY_SECONDARY) {
            dspic33_device_internal_output_compare_set_high(
                cpu, dspic33_device_internal_output_compare_output_channel(cpu, channel), false);
            return;
        } else {
            dspic33_device_internal_output_compare_raise(cpu, channel);
            return;
        }
        dspic33_device_internal_output_compare_schedule_next(cpu, channel, 0u);
    }
}

static uint16_t output_compare_previous_word(const Dspic33* cpu, uint16_t changed_address,
                                             uint16_t previous, uint16_t address) {
    return address == changed_address ? previous : dspic33_device_internal_raw_word(cpu, address);
}

static bool output_compare_cascade_supported_before(const Dspic33* cpu, uint8_t channel,
                                                    uint16_t changed_address, uint16_t previous) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    uint16_t low_base = dspic33_device_internal_output_compare_base(low);
    uint16_t high_base = dspic33_device_internal_output_compare_base(high);
    uint16_t low_control1 = output_compare_previous_word(cpu, changed_address, previous, low_base);
    uint16_t low_control2 =
        output_compare_previous_word(cpu, changed_address, previous, (uint16_t)(low_base + 2u));
    uint16_t high_control1 =
        output_compare_previous_word(cpu, changed_address, previous, high_base);
    uint16_t high_control2 =
        output_compare_previous_word(cpu, changed_address, previous, (uint16_t)(high_base + 2u));
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

static bool output_compare_update_cascade(Dspic33* cpu, uint8_t channel, uint16_t address,
                                          uint16_t previous) {
    uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    uint16_t low_base = dspic33_device_internal_output_compare_base(low);
    uint16_t high_base = dspic33_device_internal_output_compare_base(high);
    uint16_t offset = (uint16_t)(address - dspic33_device_internal_output_compare_base(channel));
    bool was_supported = output_compare_cascade_supported_before(cpu, channel, address, previous);
    bool is_supported = dspic33_device_internal_output_compare_cascade_supported(cpu, channel);
    bool cascade_involved = was_supported || is_supported ||
                            dspic33_device_internal_output_compare_cascade_requested(cpu, low) ||
                            dspic33_device_internal_output_compare_cascade_requested(cpu, high) ||
                            (offset == 2u && (previous & OUTPUT_COMPARE_32_BIT) != 0u);
    if (!cascade_involved) {
        return false;
    }
    if (!was_supported && is_supported) {
        dspic33_device_internal_output_compare_start_cascade(cpu, low);
    } else if (was_supported && !is_supported) {
        dspic33_device_internal_output_compare_stop_cascade(cpu, low);
    } else if (was_supported && is_supported) {
        uint16_t mode = dspic33_device_internal_raw_word(cpu, low_base) & OUTPUT_COMPARE_MODE_MASK;
        if (offset == 0u || offset == 2u) {
            uint16_t changed =
                (uint16_t)(dspic33_device_internal_raw_word(cpu, address) ^ previous);
            if ((offset == 0u &&
                 (changed & ~(OUTPUT_COMPARE_STOP_IDLE | OUTPUT_COMPARE_FAULT_ENABLE_MASK |
                              OUTPUT_COMPARE_FAULT_STATUS_MASK)) == 0u) ||
                (offset == 2u &&
                 (changed & ~(OUTPUT_COMPARE_INVERT | OUTPUT_COMPARE_TRISTATE |
                              OUTPUT_COMPARE_FAULT_INACTIVE | OUTPUT_COMPARE_FAULT_OUTPUT |
                              OUTPUT_COMPARE_FAULT_TRISTATE)) == 0u)) {
                dspic33_device_internal_output_compare_update_power_state(cpu);
            } else {
                dspic33_device_internal_output_compare_start_cascade(cpu, low);
            }
        } else if (offset == 4u || offset == 6u) {
            if (mode != OUTPUT_COMPARE_MODE_EDGE_PWM && mode != OUTPUT_COMPARE_MODE_CENTER_PWM) {
                uint16_t value = dspic33_device_internal_raw_word(cpu, address);
                if (offset == 4u) {
                    cpu->io.output_compare.active_rs[channel] = value;
                    cpu->io.output_compare.sync_emitted[low] = false;
                } else {
                    cpu->io.output_compare.active_r[channel] = value;
                }
                cpu->io.output_compare.timer_generation[low]++;
                dspic33_device_internal_output_compare_schedule_next(cpu, low, 0u);
            }
        }
    }
    if (was_supported && !is_supported) {
        uint16_t control1 = dspic33_device_internal_raw_word(
            cpu, dspic33_device_internal_output_compare_base(channel));
        uint16_t control2 = dspic33_device_internal_raw_word(
            cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u));
        if (dspic33_device_internal_output_compare_configuration_supported(channel, control1,
                                                                           control2)) {
            dspic33_device_internal_output_compare_start(cpu, channel);
        }
    }
    if (address == low_base || address == high_base) {
        dspic33_device_internal_output_compare_update_power_state(cpu);
    }
    dspic33_device_internal_output_compare_refresh_fault(cpu, low);
    dspic33_device_internal_output_compare_refresh_fault(cpu, high);
    return true;
}

void dspic33_device_internal_update_output_compare_register(Dspic33* cpu, uint16_t address,
                                                            uint16_t previous) {
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
    base = dspic33_device_internal_output_compare_base(channel);
    offset = (uint16_t)(address - base);
    if (dspic33_device_internal_output_compare_pmd_disabled(cpu, channel) && offset <= 8u) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + offset), previous);
        return;
    }
    if (offset != 0u && offset != 2u && offset != 4u && offset != 6u) {
        return;
    }
    if (output_compare_update_cascade(cpu, channel, address, previous)) {
        return;
    }
    control1 = dspic33_device_internal_raw_word(cpu, base);
    control2 = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
    previous1 = offset == 0u ? previous : control1;
    previous2 = offset == 2u ? previous : control2;
    if (offset == 2u && (control1 & OUTPUT_COMPARE_TRIGGER_ONESHOT) != 0u &&
        (control1 & OUTPUT_COMPARE_CON1_UNSUPPORTED) == 0u &&
        (control1 & OUTPUT_COMPARE_MODE_MASK) != 0u && (control2 & OUTPUT_COMPARE_TRIGGER) != 0u &&
        ((control2 ^ previous2) & OUTPUT_COMPARE_TRIGGER_STATUS) != 0u) {
        control2 = (uint16_t)((control2 & ~OUTPUT_COMPARE_TRIGGER_STATUS) |
                              (previous2 & OUTPUT_COMPARE_TRIGGER_STATUS));
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 2u), control2);
    }
    was_supported = dspic33_device_internal_output_compare_configuration_supported(
        channel, previous1, previous2);
    is_supported =
        dspic33_device_internal_output_compare_configuration_supported(channel, control1, control2);
    if (!was_supported && is_supported) {
        dspic33_device_internal_output_compare_start(cpu, channel);
    } else if (was_supported && !is_supported) {
        dspic33_device_internal_output_compare_stop(cpu, channel);
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
            dspic33_device_internal_output_compare_start(cpu, channel);
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
                cpu->io.output_compare.active_rs[channel] =
                    dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 4u));
                cpu->io.output_compare.sync_emitted[channel] = false;
            } else if (offset == 6u) {
                cpu->io.output_compare.active_r[channel] =
                    dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 6u));
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
                dspic33_device_internal_output_compare_schedule_next(cpu, channel,
                                                                     activation_delay);
                return;
            }
            cpu->io.output_compare.timer_generation[channel]++;
            dspic33_device_internal_output_compare_schedule_next(cpu, channel, 0u);
        }
    }
    if (offset == 0u && ((previous1 ^ control1) & OUTPUT_COMPARE_STOP_IDLE) != 0u &&
        cpu->power_state == DSPIC33_POWER_IDLE) {
        dspic33_device_internal_output_compare_update_power_state(cpu);
    }
    dspic33_device_internal_output_compare_refresh_fault(cpu, channel);
}

void dspic33_device_internal_update_output_compare_pmd(Dspic33* cpu, uint16_t address,
                                                       uint16_t previous) {
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
    current = dspic33_device_internal_raw_word(cpu, address);
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
            dspic33_device_internal_raw_write_word(cpu, address, previous);
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

void dspic33_device_internal_advance_output_compare(Dspic33* cpu, uint64_t cycles) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        if (dspic33_device_internal_output_compare_timer_owner(cpu, channel) &&
            dspic33_device_internal_output_compare_operating(cpu, channel) &&
            dspic33_device_internal_output_compare_fp_clocked(cpu, channel)) {
            uint16_t address =
                (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 8u);
            uint16_t control2 = dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_internal_output_compare_base(channel) + 2u));
            uint64_t interval_start = cpu->device_cycles - cycles;
            uint64_t active_start = cpu->io.output_compare.activation_cycle[channel];
            uint64_t elapsed_start = interval_start > active_start ? interval_start : active_start;
            if (cpu->device_cycles > elapsed_start &&
                (control2 & (OUTPUT_COMPARE_TRIGGER | OUTPUT_COMPARE_TRIGGER_STATUS)) !=
                    OUTPUT_COMPARE_TRIGGER) {
                if (dspic33_device_internal_output_compare_cascade_owner(cpu, channel)) {
                    dspic33_device_internal_output_compare_write_cascade_timer(
                        cpu, channel,
                        dspic33_device_internal_output_compare_cascade_timer(cpu, channel) +
                            (uint32_t)(cpu->device_cycles - elapsed_start));
                } else {
                    dspic33_device_internal_raw_write_word(
                        cpu, address,
                        (uint16_t)(dspic33_device_internal_raw_word(cpu, address) +
                                   (uint16_t)(cpu->device_cycles - elapsed_start)));
                }
            }
        }
    }
}

static bool output_compare_uses_timer(const Dspic33* cpu, uint8_t channel, uint8_t timer) {
    return dspic33_device_internal_output_compare_timer_owner(cpu, channel) &&
           dspic33_device_internal_output_compare_operating(cpu, channel) &&
           !dspic33_device_internal_output_compare_fp_clocked(cpu, channel) &&
           dspic33_device_internal_output_compare_timer_source(cpu, channel) == timer &&
           (timer != 0u ||
            (dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[0]) &
             (TIMER_EXTERNAL | TIMER_SYNC)) != TIMER_EXTERNAL);
}

uint64_t dspic33_device_internal_output_compare_clock_boundary_ticks(const Dspic33* cpu,
                                                                     uint8_t timer) {
    uint64_t boundary = UINT64_MAX;
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint32_t kind;
        uint64_t delay;
        if (!output_compare_uses_timer(cpu, channel, timer)) {
            continue;
        }
        delay = dspic33_device_internal_output_compare_next_timer_event(cpu, channel, &kind);
        if (delay < boundary) {
            boundary = delay;
        }
    }
    return boundary;
}

void dspic33_device_internal_output_compare_advance_clock(Dspic33* cpu, uint8_t timer,
                                                          uint64_t ticks) {
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
        base = dspic33_device_internal_output_compare_base(channel);
        timer_address = (uint16_t)(base + 8u);
        delay = dspic33_device_internal_output_compare_next_timer_event(cpu, channel, &kind);
        if (delay == UINT64_MAX) {
            continue;
        }
        if (dspic33_device_internal_output_compare_cascade_owner(cpu, channel)) {
            dspic33_device_internal_output_compare_write_cascade_timer(
                cpu, channel,
                dspic33_device_internal_output_compare_cascade_timer(cpu, channel) +
                    (uint32_t)ticks);
        } else {
            dspic33_device_internal_raw_write_word(
                cpu, timer_address,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, timer_address) + (uint16_t)ticks));
        }
        if (ticks == delay) {
            dspic33_device_internal_run_output_compare(
                cpu, channel,
                kind | ((uint32_t)cpu->io.output_compare.timer_generation[channel]
                        << OUTPUT_COMPARE_EVENT_GENERATION_SHIFT));
        }
    }
    cpu->io.output_compare.clock_advancing = false;
    pulses = cpu->io.output_compare.deferred_sync_pulses;
    cpu->io.output_compare.deferred_sync_pulses = 0u;
    for (channel = 0u; channel < 9u; channel++) {
        if ((pulses & (uint16_t)(1u << channel)) != 0u) {
            dspic33_device_internal_output_compare_pulse_source(
                cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_OC_FIRST + channel));
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

bool dspic33_device_internal_output_compare_pin_channel(const Dspic33* cpu, uint8_t pin,
                                                        uint8_t* channel) {
    return output_compare_function_channel(dspic33_device_internal_pps_output_function(cpu, pin),
                                           channel);
}
