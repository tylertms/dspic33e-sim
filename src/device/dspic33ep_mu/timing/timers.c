#include "device/dspic33ep_mu/internal.h"

bool dspic33_device_internal_timer_is_type_b(uint8_t timer_index) {
    return timer_index >= 1u && (timer_index & 1u) != 0u;
}

static bool timer_is_type_c(uint8_t timer_index) {
    return timer_index >= 2u && (timer_index & 1u) == 0u;
}

bool dspic33_device_internal_timer_pair_enabled(const Dspic33* cpu, uint8_t timer_index) {
    return dspic33_device_internal_timer_is_type_b(timer_index) &&
           (dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer_index]) &
            TIMER_32_BIT) != 0u;
}

bool dspic33_device_internal_timer_is_paired_high(const Dspic33* cpu, uint8_t timer_index) {
    return timer_is_type_c(timer_index) &&
           dspic33_device_internal_timer_pair_enabled(cpu, (uint8_t)(timer_index - 1u));
}

uint32_t dspic33_device_internal_timer_prescale(uint16_t timer_control) {
    switch (timer_control & TIMER_PRESCALE_MASK) {
    case 0x0010u:
        return 8u;
    case 0x0020u:
        return 64u;
    case 0x0030u:
        return 256u;
    default:
        return 1u;
    }
}

bool dspic33_device_internal_timer_power_enabled(const Dspic33* cpu, uint8_t timer_index,
                                                 bool is_external_clock) {
    const uint16_t timer_control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer_index]);

    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE && (timer_control & TIMER_STOP_IDLE) == 0u &&
        (!dspic33_device_internal_timer_pair_enabled(cpu, timer_index) ||
         (dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer_index + 1u]) &
          TIMER_STOP_IDLE) == 0u)) {
        return true;
    }
    return is_external_clock && timer_index == 0u && (timer_control & TIMER_SYNC) == 0u;
}

bool dspic33_device_internal_timer_pmd_disabled(const Dspic33* cpu, uint8_t timer_index) {
    uint16_t timer_mask = (uint16_t)(1u << timer_index);

    if (timer_index >= 5u) {
        return false;
    }
    if (dspic33_device_internal_timer_is_paired_high(cpu, timer_index)) {
        timer_mask |= (uint16_t)(1u << (timer_index - 1u));
    } else if (dspic33_device_internal_timer_pair_enabled(cpu, timer_index)) {
        timer_mask |= (uint16_t)(1u << (timer_index + 1u));
    }
    return (cpu->io.timer_pmd_disabled & timer_mask) != 0u;
}

typedef struct {
    uint64_t counter_value;
    uint64_t period_matches;
} TimerAdvance;

static TimerAdvance advance_counter(uint64_t current_value, uint64_t period_value,
                                    uint64_t maximum_value, uint64_t tick_count) {
    TimerAdvance timer_advance = {current_value, 0u};
    const uint64_t period_cycle_length = period_value + 1u;
    uint64_t first_match_ticks;
    uint64_t first_reset_ticks;
    uint64_t remaining_ticks;

    if (tick_count == 0u) {
        return timer_advance;
    }
    if (current_value == period_value) {
        first_match_ticks = period_cycle_length;
        first_reset_ticks = 1u;
    } else if (current_value < period_value) {
        first_match_ticks = period_value - current_value;
        first_reset_ticks = first_match_ticks + 1u;
    } else {
        first_match_ticks = maximum_value - current_value + 1u + period_value;
        first_reset_ticks = first_match_ticks + 1u;
    }
    if (tick_count >= first_match_ticks) {
        timer_advance.period_matches = 1u + (tick_count - first_match_ticks) / period_cycle_length;
    }
    if (tick_count < first_reset_ticks) {
        timer_advance.counter_value = (current_value + tick_count) & maximum_value;
        return timer_advance;
    }
    remaining_ticks = tick_count - first_reset_ticks;
    timer_advance.counter_value = remaining_ticks % period_cycle_length;
    return timer_advance;
}

static void signal_timer_period(Dspic33* cpu, uint8_t timer_index, uint64_t period_matches,
                                bool is_gated, uint16_t* synchronization_sources) {
    if (period_matches != 0u) {
        if (timer_index < 5u) {
            *synchronization_sources |= (uint16_t)(1u << timer_index);
        }
        if (timer_index >= 1u && timer_index <= 4u) {
            dspic33_dma_request(cpu, dspic33_device_timer_irqs[timer_index], 0u, 0u);
        }
        if (timer_index == 2u) {
            dspic33_adc_trigger(cpu, 0u, 2u, 0u);
        } else if (timer_index == 4u) {
            dspic33_adc_trigger(cpu, 1u, 4u, 0u);
        }
        if (!is_gated) {
            const uint64_t interrupt_delay = cpu->io.timer_instruction_active
                                                 ? cpu->io.timer_instruction_ratio
                                                 : dspic33_device_instruction_cycles(cpu, 1u);
            if (!dspic33_schedule(cpu, DSPIC33_EVENT_TIMER_INTERRUPT, timer_index, 0u,
                                  interrupt_delay)) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
    }
}

static void advance_timer_ticks(Dspic33* cpu, uint8_t timer_index, uint64_t tick_count,
                                uint16_t* synchronization_sources) {
    const uint16_t timer_control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer_index]);
    const bool is_paired = dspic33_device_internal_timer_pair_enabled(cpu, timer_index);
    const bool is_gated =
        (timer_control & TIMER_GATE) != 0u && (timer_control & TIMER_EXTERNAL) == 0u;

    if (tick_count == 0u) {
        return;
    }
    if (is_paired) {
        const uint64_t current_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer_index]) |
            ((uint64_t)dspic33_device_internal_raw_word(
                 cpu, dspic33_device_timer_registers[timer_index + 1u])
             << 16u);
        const uint64_t period_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer_index]) |
            ((uint64_t)dspic33_device_internal_raw_word(
                 cpu, dspic33_device_timer_periods[timer_index + 1u])
             << 16u);
        const TimerAdvance timer_advance =
            advance_counter(current_value, period_value, UINT32_MAX, tick_count);

        dspic33_device_internal_raw_write_word(cpu, dspic33_device_timer_registers[timer_index],
                                               (uint16_t)timer_advance.counter_value);
        dspic33_device_internal_raw_write_word(cpu,
                                               dspic33_device_timer_registers[timer_index + 1u],
                                               (uint16_t)(timer_advance.counter_value >> 16u));
        if (period_value != 0u) {
            signal_timer_period(cpu, (uint8_t)(timer_index + 1u), timer_advance.period_matches,
                                is_gated, synchronization_sources);
        }
    } else {
        const uint16_t period_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer_index]);
        const TimerAdvance timer_advance = advance_counter(
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer_index]),
            period_value, UINT16_MAX, tick_count);

        dspic33_device_internal_raw_write_word(cpu, dspic33_device_timer_registers[timer_index],
                                               (uint16_t)timer_advance.counter_value);
        if (period_value != 0u) {
            signal_timer_period(cpu, timer_index, timer_advance.period_matches, is_gated,
                                synchronization_sources);
        }
    }
}

void dspic33_device_internal_pulse_timer_synchronization_sources(Dspic33* cpu,
                                                                 uint16_t* pending_sources) {
    uint16_t pending_source_bits = *pending_sources;
    uint8_t timer_index = 0u;

    *pending_sources = 0u;
    while (pending_source_bits != 0u) {
        if ((pending_source_bits & 1u) != 0u) {
            const uint8_t timer_source = (uint8_t)(INPUT_CAPTURE_SYNC_TIMER_FIRST + timer_index);
            dspic33_device_internal_input_capture_pulse_source(cpu, timer_source);
            dspic33_device_internal_output_compare_pulse_source(cpu, timer_source);
        }
        pending_source_bits >>= 1u;
        timer_index++;
    }
}

uint64_t dspic33_device_internal_timer_ticks_until_period(const Dspic33* cpu, uint8_t timer_index) {
    uint64_t current_value;
    uint64_t period_value;
    uint64_t maximum_value;
    if (dspic33_device_internal_timer_pair_enabled(cpu, timer_index)) {
        current_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer_index]) |
            ((uint64_t)dspic33_device_internal_raw_word(
                 cpu, dspic33_device_timer_registers[timer_index + 1u])
             << 16u);
        period_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer_index]) |
            ((uint64_t)dspic33_device_internal_raw_word(
                 cpu, dspic33_device_timer_periods[timer_index + 1u])
             << 16u);
        maximum_value = UINT32_MAX;
    } else {
        current_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer_index]);
        period_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer_index]);
        maximum_value = UINT16_MAX;
    }
    if (period_value == 0u) {
        return UINT64_MAX;
    }
    if (current_value == period_value) {
        return period_value + 1u;
    }
    if (current_value < period_value) {
        return period_value - current_value;
    }
    return maximum_value - current_value + 1u + period_value;
}

void dspic33_device_internal_clock_timer(Dspic33* cpu, uint8_t timer_index, uint64_t clock_count,
                                         uint16_t* synchronization_sources,
                                         bool flush_synchronization_sources) {
    const uint16_t timer_control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer_index]);
    const uint32_t prescale_factor = dspic33_device_internal_timer_prescale(timer_control);
    const uint64_t accumulated_clocks = cpu->io.timer_fraction[timer_index] + clock_count;
    uint64_t remaining_ticks = accumulated_clocks / prescale_factor;
    const uint8_t synchronization_timer =
        dspic33_device_internal_timer_pair_enabled(cpu, timer_index) ? (uint8_t)(timer_index + 1u)
                                                                     : timer_index;
    const uint8_t synchronization_source =
        (uint8_t)(INPUT_CAPTURE_SYNC_TIMER_FIRST + synchronization_timer);
    cpu->io.timer_fraction[timer_index] = (uint32_t)(accumulated_clocks % prescale_factor);

    while (remaining_ticks != 0u) {
        uint64_t step_ticks = remaining_ticks;
        if (synchronization_timer < 5u &&
            (dspic33_device_internal_input_capture_source_awaited(cpu, synchronization_source) ||
             dspic33_device_internal_output_compare_source_awaited(cpu, synchronization_source))) {
            const uint64_t period_boundary =
                dspic33_device_internal_timer_ticks_until_period(cpu, timer_index);
            if (period_boundary < step_ticks) {
                step_ticks = period_boundary;
            }
        }
        if (timer_index < 5u) {
            const uint64_t output_compare_boundary =
                dspic33_device_internal_output_compare_clock_boundary_ticks(cpu, timer_index);
            if (output_compare_boundary < step_ticks) {
                step_ticks = output_compare_boundary;
            }
        }
        if (synchronization_timer != timer_index && synchronization_timer < 5u) {
            const uint64_t paired_output_compare_boundary =
                dspic33_device_internal_output_compare_clock_boundary_ticks(cpu,
                                                                            synchronization_timer);
            if (paired_output_compare_boundary < step_ticks) {
                step_ticks = paired_output_compare_boundary;
            }
        }
        if (timer_index < 5u) {
            static const uint16_t capture_source_masks[5] = {0x1000u, 0x0400u, 0x0000u, 0x0800u,
                                                             0x0c00u};
            dspic33_device_internal_input_capture_advance_clock(
                cpu, capture_source_masks[timer_index], step_ticks);
            dspic33_device_internal_output_compare_advance_clock(cpu, timer_index, step_ticks);
            if (synchronization_timer != timer_index && synchronization_timer < 5u) {
                dspic33_device_internal_output_compare_advance_clock(cpu, synchronization_timer,
                                                                     step_ticks);
            }
        }
        advance_timer_ticks(cpu, timer_index, step_ticks, synchronization_sources);
        if (timer_index >= 1u && timer_index <= 4u) {
            dspic33_device_internal_comparator_filter_clock(cpu, (uint8_t)(timer_index + 3u),
                                                            step_ticks);
        }
        if (flush_synchronization_sources) {
            dspic33_device_internal_pulse_timer_synchronization_sources(cpu,
                                                                        synchronization_sources);
        }
        remaining_ticks -= step_ticks;
    }
}

void dspic33_device_internal_pulse_timer(Dspic33* cpu, uint8_t timer_index, uint32_t pulse_count) {
    uint16_t synchronization_sources = 0u;
    uint16_t timer_bit;
    uint16_t timer_control;

    if (timer_index >= DSPIC33_TIMER_COUNT || pulse_count == 0u ||
        dspic33_device_internal_timer_is_paired_high(cpu, timer_index)) {
        return;
    }
    timer_bit = (uint16_t)(1u << timer_index);
    timer_control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer_index]);
    if (dspic33_device_internal_timer_pmd_disabled(cpu, timer_index) ||
        (cpu->io.timer_enabled & timer_bit) == 0u || (timer_control & TIMER_EXTERNAL) == 0u ||
        !dspic33_device_internal_timer_power_enabled(cpu, timer_index, true)) {
        return;
    }
    if ((timer_index == 0u || dspic33_device_internal_timer_is_type_b(timer_index)) &&
        (cpu->io.timer_external_started & timer_bit) == 0u) {
        cpu->io.timer_external_started |= timer_bit;
        pulse_count--;
    }
    dspic33_device_internal_clock_timer(cpu, timer_index, pulse_count, &synchronization_sources,
                                        true);
    dspic33_device_internal_pulse_timer_synchronization_sources(cpu, &synchronization_sources);
}

void dspic33_device_internal_set_timer_gate(Dspic33* cpu, uint8_t timer_index, bool is_gate_high) {
    uint16_t timer_bit;
    bool was_gate_high;
    uint16_t timer_control;

    if (timer_index >= DSPIC33_TIMER_COUNT ||
        dspic33_device_internal_timer_is_paired_high(cpu, timer_index)) {
        return;
    }
    timer_bit = (uint16_t)(1u << timer_index);
    was_gate_high = (cpu->io.timer_gate & timer_bit) != 0u;
    if (is_gate_high) {
        cpu->io.timer_gate |= timer_bit;
    } else {
        cpu->io.timer_gate &= (uint16_t)~timer_bit;
    }
    timer_control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer_index]);
    if (!dspic33_device_internal_timer_pmd_disabled(cpu, timer_index) && was_gate_high &&
        !is_gate_high && (cpu->io.timer_enabled & timer_bit) != 0u &&
        (timer_control & (TIMER_GATE | TIMER_EXTERNAL)) == TIMER_GATE &&
        dspic33_device_internal_timer_power_enabled(cpu, timer_index, false)) {
        const uint8_t interrupt_timer = dspic33_device_internal_timer_pair_enabled(cpu, timer_index)
                                            ? (uint8_t)(timer_index + 1u)
                                            : timer_index;
        uint32_t period_value =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer_index]);
        if (dspic33_device_internal_timer_pair_enabled(cpu, timer_index)) {
            period_value |= (uint32_t)dspic33_device_internal_raw_word(
                                cpu, dspic33_device_timer_periods[timer_index + 1u])
                            << 16u;
        }
        if (period_value != 0u) {
            dspic33_raise_interrupt(cpu, dspic33_device_timer_irqs[interrupt_timer]);
        }
    }
}
