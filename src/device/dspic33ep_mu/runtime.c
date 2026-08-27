#include "device/dspic33ep_mu/internal.h"

static void complete_nvm_event(Dspic33* cpu) {
    if (!cpu->nvm.active) {
        return;
    }
    dspic33_complete_nvm(cpu);
    cpu->nvm.active = false;
    dspic33_device_internal_raw_write_word(
        cpu, NVM_CONTROL,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, NVM_CONTROL) &
                   ~(NVM_WRITE | NVM_WRITE_ERROR)));
    if (dspic33_complete_nvm_reset(cpu)) {
        return;
    }
    if (dspic33_watchdog_complete_nvm(cpu)) {
        return;
    }
    dspic33_raise_interrupt(cpu, 15u);
}

uint8_t dspic33_device_internal_auxiliary_pll_input(uint16_t control) {
    if ((control & AUXILIARY_CLOCK_SOURCE_FRC) != 0u) {
        return 1u;
    }
    if ((control & AUXILIARY_CLOCK_SOURCE_PRIMARY) != 0u) {
        return 2u;
    }
    return (uint8_t)(4u | ((control & AUXILIARY_CLOCK_OSCILLATOR_MODE) >> 11u));
}

static bool auxiliary_pll_input_available(const Dspic33* cpu, uint16_t control) {
    uint8_t input = dspic33_device_internal_auxiliary_pll_input(control);
    if (input == 1u) {
        return true;
    }
    if (input == 2u) {
        return (cpu->configuration[8u] & 0x03u) != 0x03u;
    }
    return input != 4u;
}

bool dspic33_device_internal_auxiliary_usb_clock_available(const Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, AUXILIARY_CLOCK_CONTROL);
    uint8_t input = dspic33_device_internal_auxiliary_pll_input(control);
    if (!auxiliary_pll_input_available(cpu, control)) {
        return false;
    }
    if ((control & AUXILIARY_PLL_ENABLE) != 0u) {
        return (control & AUXILIARY_PLL_LOCK) != 0u;
    }
    return input == 7u || (input == 2u && (cpu->configuration[8u] & 0x03u) == 0u);
}

bool dspic33_device_internal_auxiliary_pll_reconfiguration(uint16_t previous, uint16_t control) {
    return ((previous ^ control) & (AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_PRESCALER)) != 0u ||
           dspic33_device_internal_auxiliary_pll_input(previous) !=
               dspic33_device_internal_auxiliary_pll_input(control);
}

bool dspic33_device_internal_auxiliary_clock_configuration_locked(const Dspic33* cpu) {
    return (dspic33_device_internal_raw_word(cpu, OSCILLATOR_CONTROL) & OSCILLATOR_CLOCK_LOCK) !=
               0u &&
           (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_CLOCK_LOCK) != 0u;
}

static void complete_auxiliary_pll(Dspic33* cpu, uint32_t generation) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, AUXILIARY_CLOCK_CONTROL);
    if (generation == cpu->io.auxiliary_pll_generation && (control & AUXILIARY_PLL_ENABLE) != 0u &&
        auxiliary_pll_input_available(cpu, control)) {
        dspic33_device_internal_raw_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                                               (uint16_t)(control | AUXILIARY_PLL_LOCK));
        dspic33_device_internal_usb_update_power_state(cpu);
    }
}

void dspic33_device_internal_reconfigure_auxiliary_pll(Dspic33* cpu) {
    uint16_t control = (uint16_t)(dspic33_device_internal_raw_word(cpu, AUXILIARY_CLOCK_CONTROL) &
                                  ~AUXILIARY_PLL_LOCK);
    cpu->io.auxiliary_pll_generation++;
    dspic33_device_internal_raw_write_word(cpu, AUXILIARY_CLOCK_CONTROL, control);
    dspic33_device_internal_usb_update_power_state(cpu);
    if ((control & AUXILIARY_PLL_ENABLE) != 0u && auxiliary_pll_input_available(cpu, control) &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_AUX_PLL, 0u, cpu->io.auxiliary_pll_generation,
                          AUXILIARY_PLL_LOCK_DELAY)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_configuration_changed(Dspic33* cpu, uint32_t address, uint8_t previous) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, AUXILIARY_CLOCK_CONTROL);
    if (address == DSPIC33_CONFIGURATION_BASE + 6u) {
        dspic33_device_internal_oscillator_startup_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 8u) {
        if (dspic33_device_internal_auxiliary_pll_input(control) == 2u &&
            ((previous ^ cpu->configuration[8u]) & 0x03u) != 0u) {
            dspic33_device_internal_reconfigure_auxiliary_pll(cpu);
        }
        dspic33_device_internal_oscillator_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 10u) {
        dspic33_device_internal_oscillator_pll_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 12u) {
        dspic33_i2c_refresh_pins(cpu);
    }
}

static void remove_nvm_events(Dspic33* cpu) {
    size_t source;
    size_t destination = 0u;
    for (source = 0u; source < cpu->events.count; source++) {
        if (cpu->events.items[source].type != DSPIC33_EVENT_NVM) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

static void process_event(Dspic33* cpu, const Dspic33Event* event) {
    switch (event->type) {
    case DSPIC33_EVENT_INTERRUPT:
        dspic33_device_internal_raise_scheduled_interrupt(cpu, event->source);
        break;
    case DSPIC33_EVENT_TIMER:
        dspic33_device_internal_pulse_timer(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_TIMER_GATE:
        dspic33_device_internal_set_timer_gate(cpu, (uint8_t)event->source, event->value != 0u);
        break;
    case DSPIC33_EVENT_TIMER_INTERRUPT:
        dspic33_raise_interrupt(cpu, dspic33_device_timer_irqs[event->source]);
        break;
    case DSPIC33_EVENT_TIMER_PMD:
        dspic33_device_internal_run_timer_pmd(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_DMA:
        dspic33_device_internal_run_dma(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_ADC:
        dspic33_device_internal_run_adc(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_ADC_PMD:
        dspic33_device_internal_run_adc_pmd(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_PWM_FAULT:
        cpu->io.pwm_fault_direct |= (uint32_t)1u << event->source;
        dspic33_device_internal_pwm_input_event(cpu, (uint8_t)event->source,
                                                (event->value & PWM_INPUT_HIGH) != 0u, false);
        break;
    case DSPIC33_EVENT_PWM_CURRENT_LIMIT:
        cpu->io.pwm_current_limit_direct |= (uint32_t)1u << event->source;
        dspic33_device_internal_pwm_input_event(cpu, (uint8_t)event->source,
                                                (event->value & PWM_INPUT_HIGH) != 0u, true);
        break;
    case DSPIC33_EVENT_PWM_DEAD_TIME:
        cpu->io.pwm_dead_time_direct |= (uint8_t)(1u << event->source);
        dspic33_device_internal_pwm_dead_time_event(cpu, (uint8_t)event->source,
                                                    (event->value & PWM_INPUT_HIGH) != 0u);
        break;
    case DSPIC33_EVENT_PWM_SYNC:
        cpu->io.pwm_sync_direct |= (uint8_t)(1u << event->source);
        dspic33_device_internal_pwm_sync_event(cpu, (uint8_t)event->source,
                                               (event->value & PWM_INPUT_HIGH) != 0u);
        break;
    case DSPIC33_EVENT_PWM_PMD:
        dspic33_device_internal_run_pwm_pmd(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_UART:
        dspic33_device_internal_run_uart(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_SPI:
        dspic33_device_internal_run_spi(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_SPI_SELECT:
        dspic33_device_internal_run_spi_select(cpu, (uint8_t)event->source,
                                               (event->value & SPI_SELECT_ACTIVE) != 0u);
        break;
    case DSPIC33_EVENT_I2C:
        dspic33_i2c_process_event(cpu, (uint8_t)event->source, event->value, event->external);
        break;
    case DSPIC33_EVENT_CAN:
        dspic33_device_internal_run_can(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_USB:
        dspic33_device_internal_run_usb(cpu, event->source);
        break;
    case DSPIC33_EVENT_USB_PMD:
        dspic33_device_internal_run_usb_pmd(cpu, event->value);
        break;
    case DSPIC33_EVENT_CRC:
        if (event->source == CRC_EVENT_PMD_SOURCE) {
            dspic33_device_internal_run_crc_pmd(cpu, event->value);
        } else {
            dspic33_device_internal_run_crc(cpu, (uint16_t)event->value);
        }
        break;
    case DSPIC33_EVENT_PMP:
        if (event->source == PMP_EVENT_CLEAR_BUSY) {
            dspic33_device_internal_pmp_clear_busy(cpu, (uint16_t)event->value);
        } else if (event->source == PMP_EVENT_COMPLETE) {
            dspic33_device_internal_run_pmp(cpu, (uint16_t)event->value);
        } else if (event->source == PMP_EVENT_PMD) {
            dspic33_device_internal_run_pmp_pmd(cpu, event->value);
        } else if (event->source == PMP_EVENT_SLAVE_READ) {
            dspic33_device_internal_pmp_slave_read_event(cpu, (uint8_t)event->value);
        } else if (event->source == PMP_EVENT_SLAVE_WRITE) {
            dspic33_device_internal_pmp_slave_write_event(cpu, (uint8_t)(event->value >> 8u),
                                                          (uint8_t)event->value);
        }
        break;
    case DSPIC33_EVENT_INPUT_CAPTURE:
        dspic33_device_internal_run_input_capture(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_OUTPUT_COMPARE:
        dspic33_device_internal_run_output_compare(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_OUTPUT_COMPARE_FAULT:
        if ((event->value & OUTPUT_COMPARE_FAULT_EVENT_PIN) != 0u) {
            dspic33_device_internal_apply_physical_pin_level(
                cpu, (uint8_t)event->source,
                (event->value & OUTPUT_COMPARE_FAULT_EVENT_HIGH) != 0u);
        } else {
            dspic33_device_internal_output_compare_fault_input(
                cpu, (uint8_t)event->source,
                (event->value & OUTPUT_COMPARE_FAULT_EVENT_HIGH) != 0u);
        }
        break;
    case DSPIC33_EVENT_COMPARATOR:
        dspic33_device_internal_run_comparator(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_RTCC:
        dspic33_device_internal_run_rtcc(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_QEI:
        dspic33_device_internal_run_qei(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_DCI:
        dspic33_device_internal_run_dci(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_NVM:
        break;
    case DSPIC33_EVENT_AUX_PLL:
        complete_auxiliary_pll(cpu, event->value);
        break;
    case DSPIC33_EVENT_OSCILLATOR:
        dspic33_device_internal_complete_oscillator_event(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_PLATFORM_PMD:
        dspic33_device_internal_run_platform_pmd(cpu, event->source, event->value);
        break;
    }
}

static void advance_timers(Dspic33* cpu, uint64_t cycles) {
    uint16_t enabled = cpu->io.timer_enabled;
    uint16_t synchronization_sources = 0u;
    uint8_t timer = 0u;
    while (enabled != 0u) {
        if ((enabled & 1u) != 0u) {
            uint16_t control =
                dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]);
            bool gated = (control & TIMER_GATE) != 0u;
            bool gate_high = (cpu->io.timer_gate & (uint16_t)(1u << timer)) != 0u;
            if (!dspic33_device_internal_timer_pmd_disabled(cpu, timer) &&
                !dspic33_device_internal_timer_is_paired_high(cpu, timer) &&
                (control & TIMER_EXTERNAL) == 0u &&
                dspic33_device_internal_timer_power_enabled(cpu, timer, false) &&
                (!gated || gate_high)) {
                dspic33_device_internal_clock_timer(cpu, timer, cycles, &synchronization_sources,
                                                    false);
            }
        }
        enabled >>= 1u;
        timer++;
    }
    dspic33_device_internal_pulse_timer_synchronization_sources(cpu, &synchronization_sources);
}

static uint64_t timer_boundary_cycles(const Dspic33* cpu, uint64_t limit) {
    uint64_t boundary = limit;
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        uint16_t bit = (uint16_t)(1u << timer);
        uint16_t control =
            dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]);
        uint64_t ticks;
        uint64_t cycles;
        uint32_t prescale;
        bool gated = (control & TIMER_GATE) != 0u;
        bool gate_high = (cpu->io.timer_gate & bit) != 0u;
        if (dspic33_device_internal_timer_pmd_disabled(cpu, timer) ||
            (cpu->io.timer_enabled & bit) == 0u ||
            dspic33_device_internal_timer_is_paired_high(cpu, timer) ||
            (control & TIMER_EXTERNAL) != 0u ||
            !dspic33_device_internal_timer_power_enabled(cpu, timer, false) ||
            (gated && !gate_high)) {
            continue;
        }
        ticks = dspic33_device_internal_timer_ticks_until_period(cpu, timer);
        if (timer < 5u) {
            uint8_t signal_timer = dspic33_device_internal_timer_pair_enabled(cpu, timer)
                                       ? (uint8_t)(timer + 1u)
                                       : timer;
            uint64_t clock_boundary =
                dspic33_device_internal_output_compare_clock_boundary_ticks(cpu, timer);
            if (signal_timer != timer && signal_timer < 5u) {
                uint64_t high_boundary =
                    dspic33_device_internal_output_compare_clock_boundary_ticks(cpu, signal_timer);
                if (high_boundary < clock_boundary) {
                    clock_boundary = high_boundary;
                }
            }
            if (clock_boundary < ticks) {
                ticks = clock_boundary;
            }
        }
        if (ticks == UINT64_MAX) {
            continue;
        }
        prescale = dspic33_device_internal_timer_prescale(control);
        if (ticks > UINT64_MAX / prescale) {
            continue;
        }
        cycles = ticks * prescale - cpu->io.timer_fraction[timer];
        if (cycles < boundary) {
            boundary = cycles;
        }
    }
    return boundary;
}

static void advance_device_cycles(Dspic33* cpu, uint64_t cycles) {
    cpu->device_cycles += cycles;
    dspic33_device_internal_advance_input_capture(cpu, cycles);
    dspic33_device_internal_advance_output_compare(cpu, cycles);
    advance_timers(cpu, cycles);
    dspic33_device_internal_advance_pwm(cpu, cycles);
    dspic33_device_internal_advance_qei(cpu, cycles);
    dspic33_device_internal_comparator_evaluate_all(cpu);
}

uint64_t dspic33_device_instruction_cycles(const Dspic33* cpu, uint64_t cycles) {
    uint16_t divisor = dspic33_device_internal_raw_word(cpu, MAIN_CLOCK_DIVISOR);
    uint64_t ratio =
        (divisor & 0x0800u) != 0u ? UINT64_C(1) << ((divisor >> 12u) & 0x07u) : UINT64_C(1);
    return cycles > UINT64_MAX / ratio ? UINT64_MAX : cycles * ratio;
}

bool dspic33_device_advance_instruction(Dspic33* cpu, uint64_t cpu_cycles, uint64_t device_cycles) {
    uint64_t target;
    size_t group;
    if (!dspic33_device_internal_pps_shadow_matches(cpu)) {
        dspic33_configuration_mismatch_reset(cpu);
    }
    if (cpu_cycles > UINT64_MAX - cpu->cycles ||
        (cpu->async_events_enabled && device_cycles > UINT64_MAX - cpu->device_cycles)) {
        return false;
    }
    cpu->cycles += cpu_cycles;
    cpu->io.timer_instruction_ratio = cpu_cycles != 0u && device_cycles / cpu_cycles <= UINT16_MAX
                                          ? (uint16_t)(device_cycles / cpu_cycles)
                                          : 1u;
    cpu->io.timer_instruction_active = true;
    if (cpu->disicnt > cpu_cycles) {
        cpu->disicnt = (uint16_t)(cpu->disicnt - cpu_cycles);
    } else {
        cpu->disicnt = 0u;
    }
    if (cpu->async_events_enabled) {
        target = cpu->device_cycles + device_cycles;
        for (;;) {
            uint64_t next_cycle = target;
            uint64_t timer_boundary;
            uint64_t qei_boundary;
            if (cpu->events.count == 0u || cpu->events.items[0].paused ||
                cpu->events.items[0].cycle > target) {
                next_cycle = target;
            } else if (cpu->events.items[0].cycle < next_cycle) {
                next_cycle = cpu->events.items[0].cycle;
            }
            timer_boundary = timer_boundary_cycles(cpu, next_cycle - cpu->device_cycles);
            qei_boundary = dspic33_device_internal_qei_boundary_cycles(cpu, timer_boundary);
            if (qei_boundary < timer_boundary) {
                timer_boundary = qei_boundary;
            }
            if (timer_boundary != 0u) {
                advance_device_cycles(cpu, timer_boundary);
                continue;
            }
            if (cpu->device_cycles == target &&
                (cpu->events.count == 0u || cpu->events.items[0].paused ||
                 cpu->events.items[0].cycle > target)) {
                break;
            }
            {
                Dspic33Event event = dspic33_device_internal_event_pop(&cpu->events);
                process_event(cpu, &event);
                if (cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR) {
                    cpu->io.timer_instruction_active = false;
                    return false;
                }
                if (cpu->stop_reason == DSPIC33_SILICON_RESULT_UNDEFINED) {
                    cpu->io.timer_instruction_active = false;
                    return true;
                }
            }
        }
    }
    for (group = 0u; group < DSPIC33_IRQ_GROUP_COUNT; group++) {
        cpu->interrupt_deferred[group] = cpu->interrupt_deferred_next[group];
        cpu->interrupt_deferred_next[group] = 0u;
    }
    cpu->gie_disable_deferred = cpu->gie_disable_deferred_next;
    cpu->gie_disable_deferred_next = 0u;
    if (cpu->nvm.active && cpu->nvm.completion_cycle != 0u &&
        cpu->cycles >= cpu->nvm.completion_cycle) {
        complete_nvm_event(cpu);
        remove_nvm_events(cpu);
    }
    cpu->io.timer_instruction_active = false;
    return true;
}

bool dspic33_device_advance(Dspic33* cpu, uint64_t cycles) {
    return dspic33_device_advance_instruction(cpu, cycles, cycles);
}

bool dspic33_device_advance_nvm(Dspic33* cpu) {
    return dspic33_device_advance_instruction(cpu, 1u, dspic33_device_instruction_cycles(cpu, 1u));
}
