#include "device/dspic33ep_mu/internal.h"

bool dspic33_device_internal_timer_is_type_b(uint8_t timer) {
    return timer >= 1u && (timer & 1u) != 0u;
}

static bool timer_is_type_c(uint8_t timer) { return timer >= 2u && (timer & 1u) == 0u; }

bool dspic33_device_internal_timer_pair_enabled(const Dspic33* cpu, uint8_t timer) {
    return dspic33_device_internal_timer_is_type_b(timer) &&
           (dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]) &
            TIMER_32_BIT) != 0u;
}

bool dspic33_device_internal_timer_is_paired_high(const Dspic33* cpu, uint8_t timer) {
    return timer_is_type_c(timer) &&
           dspic33_device_internal_timer_pair_enabled(cpu, (uint8_t)(timer - 1u));
}

uint32_t dspic33_device_internal_timer_prescale(uint16_t control) {
    switch (control & TIMER_PRESCALE_MASK) {
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

bool dspic33_device_internal_timer_power_enabled(const Dspic33* cpu, uint8_t timer, bool external) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]);
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE && (control & TIMER_STOP_IDLE) == 0u &&
        (!dspic33_device_internal_timer_pair_enabled(cpu, timer) ||
         (dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer + 1u]) &
          TIMER_STOP_IDLE) == 0u)) {
        return true;
    }
    return external && timer == 0u && (control & TIMER_SYNC) == 0u;
}

bool dspic33_device_internal_timer_pmd_disabled(const Dspic33* cpu, uint8_t timer) {
    uint16_t mask = (uint16_t)(1u << timer);
    if (timer >= 5u) {
        return false;
    }
    if (dspic33_device_internal_timer_is_paired_high(cpu, timer)) {
        mask |= (uint16_t)(1u << (timer - 1u));
    } else if (dspic33_device_internal_timer_pair_enabled(cpu, timer)) {
        mask |= (uint16_t)(1u << (timer + 1u));
    }
    return (cpu->io.timer_pmd_disabled & mask) != 0u;
}

typedef struct {
    uint64_t value;
    uint64_t matches;
} TimerAdvance;

static TimerAdvance advance_counter(uint64_t current, uint64_t period, uint64_t maximum,
                                    uint64_t ticks) {
    TimerAdvance result = {current, 0u};
    uint64_t cycle = period + 1u;
    uint64_t first_match;
    uint64_t first_reset;
    uint64_t remaining;
    if (ticks == 0u) {
        return result;
    }
    if (current == period) {
        first_match = cycle;
        first_reset = 1u;
    } else if (current < period) {
        first_match = period - current;
        first_reset = first_match + 1u;
    } else {
        first_match = maximum - current + 1u + period;
        first_reset = first_match + 1u;
    }
    if (ticks >= first_match) {
        result.matches = 1u + (ticks - first_match) / cycle;
    }
    if (ticks < first_reset) {
        result.value = (current + ticks) & maximum;
        return result;
    }
    remaining = ticks - first_reset;
    result.value = remaining % cycle;
    return result;
}

static void signal_timer_period(Dspic33* cpu, uint8_t timer, uint64_t matches, bool gated,
                                uint16_t* synchronization_sources) {
    if (matches != 0u) {
        if (timer < 5u) {
            *synchronization_sources |= (uint16_t)(1u << timer);
        }
        if (timer >= 1u && timer <= 4u) {
            dspic33_dma_request(cpu, dspic33_device_timer_irqs[timer], 0u, 0u);
        }
        if (timer == 2u) {
            dspic33_adc_trigger(cpu, 0u, 2u, 0u);
        } else if (timer == 4u) {
            dspic33_adc_trigger(cpu, 1u, 4u, 0u);
        }
        if (!gated) {
            uint64_t delay = cpu->io.timer_instruction_active
                                 ? cpu->io.timer_instruction_ratio
                                 : dspic33_device_instruction_cycles(cpu, 1u);
            if (!dspic33_schedule(cpu, DSPIC33_EVENT_TIMER_INTERRUPT, timer, 0u, delay)) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
    }
}

static void advance_timer_ticks(Dspic33* cpu, uint8_t timer, uint64_t ticks,
                                uint16_t* synchronization_sources) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]);
    bool paired = dspic33_device_internal_timer_pair_enabled(cpu, timer);
    bool gated = (control & TIMER_GATE) != 0u && (control & TIMER_EXTERNAL) == 0u;
    if (ticks == 0u) {
        return;
    }
    if (paired) {
        uint64_t current =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer]) |
            ((uint64_t)dspic33_device_internal_raw_word(cpu,
                                                        dspic33_device_timer_registers[timer + 1u])
             << 16u);
        uint64_t period =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer]) |
            ((uint64_t)dspic33_device_internal_raw_word(cpu,
                                                        dspic33_device_timer_periods[timer + 1u])
             << 16u);
        TimerAdvance result = advance_counter(current, period, UINT32_MAX, ticks);
        dspic33_device_internal_raw_write_word(cpu, dspic33_device_timer_registers[timer],
                                               (uint16_t)result.value);
        dspic33_device_internal_raw_write_word(cpu, dspic33_device_timer_registers[timer + 1u],
                                               (uint16_t)(result.value >> 16u));
        if (period != 0u) {
            signal_timer_period(cpu, (uint8_t)(timer + 1u), result.matches, gated,
                                synchronization_sources);
        }
    } else {
        uint16_t period =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer]);
        TimerAdvance result = advance_counter(
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer]), period,
            UINT16_MAX, ticks);
        dspic33_device_internal_raw_write_word(cpu, dspic33_device_timer_registers[timer],
                                               (uint16_t)result.value);
        if (period != 0u) {
            signal_timer_period(cpu, timer, result.matches, gated, synchronization_sources);
        }
    }
}

void dspic33_device_internal_pulse_timer_synchronization_sources(Dspic33* cpu, uint16_t* pending) {
    uint16_t sources = *pending;
    uint8_t timer = 0u;
    *pending = 0u;
    while (sources != 0u) {
        if ((sources & 1u) != 0u) {
            uint8_t source = (uint8_t)(INPUT_CAPTURE_SYNC_TIMER_FIRST + timer);
            dspic33_device_internal_input_capture_pulse_source(cpu, source);
            dspic33_device_internal_output_compare_pulse_source(cpu, source);
        }
        sources >>= 1u;
        timer++;
    }
}

uint64_t dspic33_device_internal_timer_ticks_until_period(const Dspic33* cpu, uint8_t timer) {
    uint64_t current;
    uint64_t period;
    uint64_t maximum;
    if (dspic33_device_internal_timer_pair_enabled(cpu, timer)) {
        current = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer]) |
                  ((uint64_t)dspic33_device_internal_raw_word(
                       cpu, dspic33_device_timer_registers[timer + 1u])
                   << 16u);
        period = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer]) |
                 ((uint64_t)dspic33_device_internal_raw_word(
                      cpu, dspic33_device_timer_periods[timer + 1u])
                  << 16u);
        maximum = UINT32_MAX;
    } else {
        current = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_registers[timer]);
        period = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer]);
        maximum = UINT16_MAX;
    }
    if (period == 0u) {
        return UINT64_MAX;
    }
    if (current == period) {
        return period + 1u;
    }
    if (current < period) {
        return period - current;
    }
    return maximum - current + 1u + period;
}

void dspic33_device_internal_clock_timer(Dspic33* cpu, uint8_t timer, uint64_t clocks,
                                         uint16_t* synchronization_sources, bool flush_sources) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]);
    uint32_t prescale = dspic33_device_internal_timer_prescale(control);
    uint64_t accumulated = cpu->io.timer_fraction[timer] + clocks;
    uint64_t ticks = accumulated / prescale;
    uint8_t signal_timer =
        dspic33_device_internal_timer_pair_enabled(cpu, timer) ? (uint8_t)(timer + 1u) : timer;
    uint8_t sync_source = (uint8_t)(INPUT_CAPTURE_SYNC_TIMER_FIRST + signal_timer);
    cpu->io.timer_fraction[timer] = (uint32_t)(accumulated % prescale);
    while (ticks != 0u) {
        uint64_t step = ticks;
        if (signal_timer < 5u &&
            (dspic33_device_internal_input_capture_source_awaited(cpu, sync_source) ||
             dspic33_device_internal_output_compare_source_awaited(cpu, sync_source))) {
            uint64_t boundary = dspic33_device_internal_timer_ticks_until_period(cpu, timer);
            if (boundary < step) {
                step = boundary;
            }
        }
        if (timer < 5u) {
            uint64_t boundary =
                dspic33_device_internal_output_compare_clock_boundary_ticks(cpu, timer);
            if (boundary < step) {
                step = boundary;
            }
        }
        if (signal_timer != timer && signal_timer < 5u) {
            uint64_t boundary =
                dspic33_device_internal_output_compare_clock_boundary_ticks(cpu, signal_timer);
            if (boundary < step) {
                step = boundary;
            }
        }
        if (timer < 5u) {
            static const uint16_t capture_sources[5] = {0x1000u, 0x0400u, 0x0000u, 0x0800u,
                                                        0x0c00u};
            dspic33_device_internal_input_capture_advance_clock(cpu, capture_sources[timer], step);
            dspic33_device_internal_output_compare_advance_clock(cpu, timer, step);
            if (signal_timer != timer && signal_timer < 5u) {
                dspic33_device_internal_output_compare_advance_clock(cpu, signal_timer, step);
            }
        }
        advance_timer_ticks(cpu, timer, step, synchronization_sources);
        if (timer >= 1u && timer <= 4u) {
            dspic33_device_internal_comparator_filter_clock(cpu, (uint8_t)(timer + 3u), step);
        }
        if (flush_sources) {
            dspic33_device_internal_pulse_timer_synchronization_sources(cpu,
                                                                        synchronization_sources);
        }
        ticks -= step;
    }
}

void dspic33_device_internal_pulse_timer(Dspic33* cpu, uint8_t timer, uint32_t pulses) {
    uint16_t synchronization_sources = 0u;
    uint16_t bit;
    uint16_t control;
    if (timer >= DSPIC33_TIMER_COUNT || pulses == 0u ||
        dspic33_device_internal_timer_is_paired_high(cpu, timer)) {
        return;
    }
    bit = (uint16_t)(1u << timer);
    control = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]);
    if (dspic33_device_internal_timer_pmd_disabled(cpu, timer) ||
        (cpu->io.timer_enabled & bit) == 0u || (control & TIMER_EXTERNAL) == 0u ||
        !dspic33_device_internal_timer_power_enabled(cpu, timer, true)) {
        return;
    }
    if ((timer == 0u || dspic33_device_internal_timer_is_type_b(timer)) &&
        (cpu->io.timer_external_started & bit) == 0u) {
        cpu->io.timer_external_started |= bit;
        pulses--;
    }
    dspic33_device_internal_clock_timer(cpu, timer, pulses, &synchronization_sources, true);
    dspic33_device_internal_pulse_timer_synchronization_sources(cpu, &synchronization_sources);
}

void dspic33_device_internal_set_timer_gate(Dspic33* cpu, uint8_t timer, bool high) {
    uint16_t bit;
    bool previous;
    uint16_t control;
    if (timer >= DSPIC33_TIMER_COUNT || dspic33_device_internal_timer_is_paired_high(cpu, timer)) {
        return;
    }
    bit = (uint16_t)(1u << timer);
    previous = (cpu->io.timer_gate & bit) != 0u;
    if (high) {
        cpu->io.timer_gate |= bit;
    } else {
        cpu->io.timer_gate &= (uint16_t)~bit;
    }
    control = dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]);
    if (!dspic33_device_internal_timer_pmd_disabled(cpu, timer) && previous && !high &&
        (cpu->io.timer_enabled & bit) != 0u &&
        (control & (TIMER_GATE | TIMER_EXTERNAL)) == TIMER_GATE &&
        dspic33_device_internal_timer_power_enabled(cpu, timer, false)) {
        uint8_t interrupt_timer =
            dspic33_device_internal_timer_pair_enabled(cpu, timer) ? (uint8_t)(timer + 1u) : timer;
        uint32_t period =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_periods[timer]);
        if (dspic33_device_internal_timer_pair_enabled(cpu, timer)) {
            period |= (uint32_t)dspic33_device_internal_raw_word(
                          cpu, dspic33_device_timer_periods[timer + 1u])
                      << 16u;
        }
        if (period != 0u) {
            dspic33_raise_interrupt(cpu, dspic33_device_timer_irqs[interrupt_timer]);
        }
    }
}
