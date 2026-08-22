
static void complete_nvm_event(Dspic33* cpu) {
    if (!cpu->nvm.active) {
        return;
    }
    dspic33_complete_nvm(cpu);
    cpu->nvm.active = false;
    raw_write_word(cpu, NVM_CONTROL,
                   (uint16_t)(raw_word(cpu, NVM_CONTROL) & ~(NVM_WRITE | NVM_WRITE_ERROR)));
    if (dspic33_complete_nvm_reset(cpu)) {
        return;
    }
    if (dspic33_watchdog_complete_nvm(cpu)) {
        return;
    }
    dspic33_raise_interrupt(cpu, 15u);
}

static uint8_t auxiliary_pll_input(uint16_t control) {
    if ((control & AUXILIARY_CLOCK_SOURCE_FRC) != 0u) {
        return 1u;
    }
    if ((control & AUXILIARY_CLOCK_SOURCE_PRIMARY) != 0u) {
        return 2u;
    }
    return (uint8_t)(4u | ((control & AUXILIARY_CLOCK_OSCILLATOR_MODE) >> 11u));
}

static bool auxiliary_pll_input_available(const Dspic33* cpu, uint16_t control) {
    uint8_t input = auxiliary_pll_input(control);
    if (input == 1u) {
        return true;
    }
    if (input == 2u) {
        return (cpu->configuration[8u] & 0x03u) != 0x03u;
    }
    return input != 4u;
}

static bool auxiliary_pll_reconfiguration(uint16_t previous, uint16_t control) {
    return ((previous ^ control) & (AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_PRESCALER)) != 0u ||
           auxiliary_pll_input(previous) != auxiliary_pll_input(control);
}

static bool auxiliary_clock_configuration_locked(const Dspic33* cpu) {
    return (raw_word(cpu, OSCILLATOR_CONTROL) & OSCILLATOR_CLOCK_LOCK) != 0u &&
           (cpu->configuration[8u] & OSCILLATOR_CONFIGURATION_CLOCK_LOCK) != 0u;
}

static void complete_auxiliary_pll(Dspic33* cpu, uint32_t generation) {
    uint16_t control = raw_word(cpu, AUXILIARY_CLOCK_CONTROL);
    if (generation == cpu->io.auxiliary_pll_generation && (control & AUXILIARY_PLL_ENABLE) != 0u &&
        auxiliary_pll_input_available(cpu, control)) {
        raw_write_word(cpu, AUXILIARY_CLOCK_CONTROL, (uint16_t)(control | AUXILIARY_PLL_LOCK));
    }
}

static void reconfigure_auxiliary_pll(Dspic33* cpu) {
    uint16_t control = (uint16_t)(raw_word(cpu, AUXILIARY_CLOCK_CONTROL) & ~AUXILIARY_PLL_LOCK);
    cpu->io.auxiliary_pll_generation++;
    raw_write_word(cpu, AUXILIARY_CLOCK_CONTROL, control);
    if ((control & AUXILIARY_PLL_ENABLE) != 0u && auxiliary_pll_input_available(cpu, control) &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_AUX_PLL, 0u, cpu->io.auxiliary_pll_generation,
                          AUXILIARY_PLL_LOCK_DELAY)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_configuration_changed(Dspic33* cpu, uint32_t address, uint8_t previous) {
    uint16_t control = raw_word(cpu, AUXILIARY_CLOCK_CONTROL);
    if (address == DSPIC33_CONFIGURATION_BASE + 6u) {
        oscillator_startup_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 8u) {
        if (auxiliary_pll_input(control) == 2u &&
            ((previous ^ cpu->configuration[8u]) & 0x03u) != 0u) {
            reconfigure_auxiliary_pll(cpu);
        }
        oscillator_configuration_changed(cpu, previous);
    } else if (address == DSPIC33_CONFIGURATION_BASE + 10u) {
        oscillator_pll_configuration_changed(cpu, previous);
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
        raise_scheduled_interrupt(cpu, event->source);
        break;
    case DSPIC33_EVENT_TIMER:
        pulse_timer(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_TIMER_GATE:
        set_timer_gate(cpu, (uint8_t)event->source, event->value != 0u);
        break;
    case DSPIC33_EVENT_TIMER_INTERRUPT:
        dspic33_raise_interrupt(cpu, timer_irqs[event->source]);
        break;
    case DSPIC33_EVENT_TIMER_PMD:
        run_timer_pmd(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_DMA:
        run_dma(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_ADC:
        run_adc(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_ADC_PMD:
        run_adc_pmd(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_PWM_FAULT:
        cpu->io.pwm_fault_direct |= (uint32_t)1u << event->source;
        pwm_input_event(cpu, (uint8_t)event->source, (event->value & PWM_INPUT_HIGH) != 0u, false);
        break;
    case DSPIC33_EVENT_PWM_CURRENT_LIMIT:
        cpu->io.pwm_current_limit_direct |= (uint32_t)1u << event->source;
        pwm_input_event(cpu, (uint8_t)event->source, (event->value & PWM_INPUT_HIGH) != 0u, true);
        break;
    case DSPIC33_EVENT_PWM_DEAD_TIME:
        cpu->io.pwm_dead_time_direct |= (uint8_t)(1u << event->source);
        pwm_dead_time_event(cpu, (uint8_t)event->source, (event->value & PWM_INPUT_HIGH) != 0u);
        break;
    case DSPIC33_EVENT_PWM_SYNC:
        cpu->io.pwm_sync_direct |= (uint8_t)(1u << event->source);
        pwm_sync_event(cpu, (uint8_t)event->source, (event->value & PWM_INPUT_HIGH) != 0u);
        break;
    case DSPIC33_EVENT_PWM_PMD:
        run_pwm_pmd(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_UART:
        run_uart(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_SPI:
        run_spi(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_SPI_SELECT:
        run_spi_select(cpu, (uint8_t)event->source, (event->value & SPI_SELECT_ACTIVE) != 0u);
        break;
    case DSPIC33_EVENT_I2C:
        dspic33_i2c_process_event(cpu, (uint8_t)event->source, event->value, event->external);
        break;
    case DSPIC33_EVENT_CAN:
        run_can(cpu, (uint8_t)event->source, event->value);
        break;
    case DSPIC33_EVENT_USB:
        run_usb(cpu, event->source);
        break;
    case DSPIC33_EVENT_USB_PMD:
        run_usb_pmd(cpu, event->value);
        break;
    case DSPIC33_EVENT_CRC:
        if (event->source == CRC_EVENT_PMD_SOURCE) {
            run_crc_pmd(cpu, event->value);
        } else {
            run_crc(cpu, (uint16_t)event->value);
        }
        break;
    case DSPIC33_EVENT_PMP:
        if (event->source == PMP_EVENT_CLEAR_BUSY) {
            pmp_clear_busy(cpu, (uint16_t)event->value);
        } else if (event->source == PMP_EVENT_COMPLETE) {
            run_pmp(cpu, (uint16_t)event->value);
        } else if (event->source == PMP_EVENT_PMD) {
            run_pmp_pmd(cpu, event->value);
        } else if (event->source == PMP_EVENT_SLAVE_READ) {
            pmp_slave_read_event(cpu, (uint8_t)event->value);
        } else if (event->source == PMP_EVENT_SLAVE_WRITE) {
            pmp_slave_write_event(cpu, (uint8_t)(event->value >> 8u), (uint8_t)event->value);
        }
        break;
    case DSPIC33_EVENT_INPUT_CAPTURE:
        run_input_capture(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_OUTPUT_COMPARE:
        run_output_compare(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_OUTPUT_COMPARE_FAULT:
        if ((event->value & OUTPUT_COMPARE_FAULT_EVENT_PIN) != 0u) {
            apply_physical_pin_level(cpu, (uint8_t)event->source,
                                     (event->value & OUTPUT_COMPARE_FAULT_EVENT_HIGH) != 0u);
        } else {
            output_compare_fault_input(cpu, (uint8_t)event->source,
                                       (event->value & OUTPUT_COMPARE_FAULT_EVENT_HIGH) != 0u);
        }
        break;
    case DSPIC33_EVENT_COMPARATOR:
        run_comparator(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_RTCC:
        run_rtcc(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_QEI:
        run_qei(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_DCI:
        run_dci(cpu, event->source, event->value);
        break;
    case DSPIC33_EVENT_NVM:
        break;
    case DSPIC33_EVENT_AUX_PLL:
        complete_auxiliary_pll(cpu, event->value);
        break;
    case DSPIC33_EVENT_OSCILLATOR:
        complete_oscillator_event(cpu, event->source, event->value);
        break;
    }
}

static void advance_timers(Dspic33* cpu, uint64_t cycles) {
    uint16_t enabled = cpu->io.timer_enabled;
    uint16_t synchronization_sources = 0u;
    uint8_t timer = 0u;
    while (enabled != 0u) {
        if ((enabled & 1u) != 0u) {
            uint16_t control = raw_word(cpu, timer_controls[timer]);
            bool gated = (control & TIMER_GATE) != 0u;
            bool gate_high = (cpu->io.timer_gate & (uint16_t)(1u << timer)) != 0u;
            if (!timer_pmd_disabled(cpu, timer) && !timer_is_paired_high(cpu, timer) &&
                (control & TIMER_EXTERNAL) == 0u && timer_power_enabled(cpu, timer, false) &&
                (!gated || gate_high)) {
                clock_timer(cpu, timer, cycles, &synchronization_sources, false);
            }
        }
        enabled >>= 1u;
        timer++;
    }
    pulse_timer_synchronization_sources(cpu, &synchronization_sources);
}

static uint64_t timer_boundary_cycles(const Dspic33* cpu, uint64_t limit) {
    uint64_t boundary = limit;
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        uint16_t bit = (uint16_t)(1u << timer);
        uint16_t control = raw_word(cpu, timer_controls[timer]);
        uint64_t ticks;
        uint64_t cycles;
        uint32_t prescale;
        bool gated = (control & TIMER_GATE) != 0u;
        bool gate_high = (cpu->io.timer_gate & bit) != 0u;
        if (timer_pmd_disabled(cpu, timer) || (cpu->io.timer_enabled & bit) == 0u ||
            timer_is_paired_high(cpu, timer) || (control & TIMER_EXTERNAL) != 0u ||
            !timer_power_enabled(cpu, timer, false) || (gated && !gate_high)) {
            continue;
        }
        ticks = timer_ticks_until_period(cpu, timer);
        if (timer < 5u) {
            uint8_t signal_timer = timer_pair_enabled(cpu, timer) ? (uint8_t)(timer + 1u) : timer;
            uint64_t clock_boundary = output_compare_clock_boundary_ticks(cpu, timer);
            if (signal_timer != timer && signal_timer < 5u) {
                uint64_t high_boundary = output_compare_clock_boundary_ticks(cpu, signal_timer);
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
        prescale = timer_prescale(control);
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
    advance_input_capture(cpu, cycles);
    advance_output_compare(cpu, cycles);
    advance_timers(cpu, cycles);
    advance_pwm(cpu, cycles);
    advance_qei(cpu, cycles);
    comparator_evaluate_all(cpu);
}

uint64_t dspic33_device_instruction_cycles(const Dspic33* cpu, uint64_t cycles) {
    uint16_t divisor = raw_word(cpu, MAIN_CLOCK_DIVISOR);
    uint64_t ratio =
        (divisor & 0x0800u) != 0u ? UINT64_C(1) << ((divisor >> 12u) & 0x07u) : UINT64_C(1);
    return cycles > UINT64_MAX / ratio ? UINT64_MAX : cycles * ratio;
}

bool dspic33_device_advance_instruction(Dspic33* cpu, uint64_t cpu_cycles, uint64_t device_cycles) {
    uint64_t target;
    size_t group;
    if (!pps_shadow_matches(cpu)) {
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
            qei_boundary = qei_boundary_cycles(cpu, timer_boundary);
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
                Dspic33Event event = event_pop(&cpu->events);
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

static void update_timer_register(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint8_t timer;
    if (address == timer_registers[0] &&
        (raw_word(cpu, timer_controls[0]) & (TIMER_ON | TIMER_SYNC | TIMER_EXTERNAL)) ==
            (TIMER_ON | TIMER_SYNC | TIMER_EXTERNAL)) {
        raw_write_word(cpu, address, previous);
        return;
    }
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        if (timer_pmd_disabled(cpu, timer) &&
            (address == timer_controls[timer] || address == timer_registers[timer] ||
             address == timer_periods[timer] ||
             (timer_is_type_b(timer) && address == timer_holding_registers[timer / 2u]))) {
            raw_write_word(cpu, address, previous);
            return;
        }
        if ((address & 0xfffeu) == timer_controls[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            if ((raw_word(cpu, timer_controls[timer]) & TIMER_ON) != 0u) {
                cpu->io.timer_enabled |= (uint16_t)(1u << timer);
            } else {
                cpu->io.timer_enabled &= (uint16_t)~(1u << timer);
            }
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            return;
        }
        if ((address & 0xfffeu) == timer_registers[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            if (timer_pair_enabled(cpu, timer)) {
                raw_write_word(cpu, timer_registers[timer + 1u],
                               raw_word(cpu, timer_holding_registers[timer / 2u]));
            }
            return;
        }
    }
}

static void run_timer_pmd(Dspic33* cpu, uint16_t timer, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> TIMER_EVENT_PMD_GENERATION_SHIFT);
    uint16_t mask = (uint16_t)(1u << timer);
    if (generation != cpu->io.timer_pmd_generation[timer]) {
        return;
    }
    if ((value & TIMER_EVENT_PMD_DISABLED) != 0u) {
        cpu->io.timer_pmd_disabled |= mask;
    } else {
        cpu->io.timer_pmd_disabled &= (uint16_t)~mask;
    }
}

static void update_timer_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t current;
    uint16_t changed;
    uint8_t timer;
    if (address != 0x0760u) {
        return;
    }
    current = raw_word(cpu, address);
    changed = (uint16_t)((previous ^ current) & 0xf800u);
    for (timer = 0u; timer < 5u; timer++) {
        uint16_t register_mask = (uint16_t)(0x0800u << timer);
        if ((changed & register_mask) == 0u) {
            continue;
        }
        cpu->io.timer_pmd_generation[timer]++;
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_TIMER_PMD, timer,
                              ((uint32_t)cpu->io.timer_pmd_generation[timer]
                               << TIMER_EVENT_PMD_GENERATION_SHIFT) |
                                  ((current & register_mask) != 0u ? TIMER_EVENT_PMD_DISABLED : 0u),
                              dspic33_device_instruction_cycles(cpu, 1u))) {
            uint8_t invalidate;
            raw_write_word(cpu, address, previous);
            for (invalidate = 0u; invalidate < 5u; invalidate++) {
                if ((changed & (uint16_t)(0x0800u << invalidate)) != 0u) {
                    cpu->io.timer_pmd_generation[invalidate]++;
                }
            }
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
}

static bool adc_module_address(uint16_t address, uint8_t module) {
    if (address >= adc_buffers[module] && address < adc_buffers[module] + 0x20u) {
        return true;
    }
    return address >= adc_controls[module] && address <= adc_controls[module] + 0x12u;
}

static bool adc_pmd_disabled(const Dspic33* cpu, uint8_t module) {
    return (cpu->io.adc_pmd_disabled & (uint8_t)(1u << module)) != 0u;
}

static void adc_reset_module(Dspic33* cpu, uint8_t module) {
    uint16_t control = adc_controls[module];
    uint8_t bit = (uint8_t)(1u << module);
    uint8_t index;
    cpu->io.adc_generation[module]++;
    cpu->io.adc_latched_count[module] = 0u;
    cpu->io.adc_conversion_index[module] = 0u;
    cpu->io.adc_buffer_index[module] = 0u;
    cpu->io.adc_sample_count[module] = 0u;
    cpu->io.adc_scan_index[module] = 0u;
    cpu->io.adc_mux_b &= (uint8_t)~bit;
    cpu->io.adc_sleep_disabled &= (uint8_t)~bit;
    memset(cpu->io.adc_dma_sample[module], 0, sizeof(cpu->io.adc_dma_sample[module]));
    for (index = 0u; index < 5u; index++) {
        raw_write_word(cpu, (uint16_t)(control + index * 2u), 0u);
    }
    if (module == 0u) {
        raw_write_word(cpu, 0x032eu, 0u);
        raw_write_word(cpu, 0x0330u, 0u);
        raw_write_word(cpu, 0x0332u, 0u);
    } else {
        raw_write_word(cpu, 0x0370u, 0u);
        raw_write_word(cpu, 0x0372u, 0u);
    }
}

static void run_adc_pmd(Dspic33* cpu, uint16_t module, uint32_t value) {
    uint16_t generation;
    uint8_t bit;
    if (module >= DSPIC33_ADC_COUNT) {
        return;
    }
    generation = (uint16_t)(value >> ADC_PMD_EVENT_GENERATION_SHIFT);
    if (generation != cpu->io.adc_pmd_generation[module]) {
        return;
    }
    bit = (uint8_t)(1u << module);
    if ((value & ADC_PMD_EVENT_DISABLED) != 0u) {
        cpu->io.adc_pmd_disabled |= bit;
    } else {
        cpu->io.adc_pmd_disabled &= (uint8_t)~bit;
    }
    adc_reset_module(cpu, (uint8_t)module);
    refresh_physical_pin_inputs(cpu);
}

static void update_adc_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint8_t module;
    uint16_t current;
    if (address != 0x0760u && address != 0x0764u) {
        return;
    }
    module = address == 0x0760u ? 0u : 1u;
    current = raw_word(cpu, address);
    if (((current ^ previous) & 1u) == 0u) {
        return;
    }
    cpu->io.adc_pmd_generation[module]++;
    if (!dspic33_schedule(
            cpu, DSPIC33_EVENT_ADC_PMD, module,
            ((uint32_t)cpu->io.adc_pmd_generation[module] << ADC_PMD_EVENT_GENERATION_SHIFT) |
                ((current & 1u) != 0u ? ADC_PMD_EVENT_DISABLED : 0u),
            dspic33_device_instruction_cycles(cpu, 1u))) {
        raw_write_word(cpu, address, previous);
        cpu->io.adc_pmd_generation[module]++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void update_adc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control_address = adc_controls[module];
        uint16_t control;
        if (address != control_address && address != control_address + 2u &&
            address != control_address + 6u) {
            continue;
        }
        control = raw_word(cpu, control_address);
        if (address == control_address) {
            bool was_on = (previous & ADC_ON) != 0u;
            bool on;
            bool was_sampling = (previous & ADC_SAMPLE) != 0u;
            bool sampling;
            uint8_t source;
            if ((requested & ADC_DONE) == 0u) {
                control &= (uint16_t)~ADC_DONE;
            }
            if (was_on && module == 0u && ((control ^ previous) & ADC_12_BIT) != 0u) {
                control = (uint16_t)((control & ~ADC_12_BIT) | (previous & ADC_12_BIT));
            }
            if (module == 0u && (control & ADC_12_BIT) != 0u) {
                control &= (uint16_t)~ADC_SIMULTANEOUS;
                raw_write_word(cpu, (uint16_t)(control_address + 2u),
                               (uint16_t)(adc_register(cpu, module, 2u) & ~ADC_CHANNELS_MASK));
                raw_write_word(cpu, (uint16_t)(control_address + 6u), 0u);
            }
            raw_write_word(cpu, control_address, control);
            on = (control & ADC_ON) != 0u;
            sampling = (control & ADC_SAMPLE) != 0u;
            source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
            if (!on) {
                adc_abort(cpu, module);
                return;
            }
            if (!was_on) {
                cpu->io.adc_buffer_index[module] = 0u;
                cpu->io.adc_sample_count[module] = 0u;
                cpu->io.adc_scan_index[module] = 0u;
                cpu->io.adc_mux_b &= (uint8_t)~(1u << module);
                memset(cpu->io.adc_dma_sample[module], 0, sizeof(cpu->io.adc_dma_sample[module]));
            }
            if (was_on && was_sampling && !sampling && source == 0u) {
                raw_write_word(cpu, control_address, (uint16_t)(control | ADC_SAMPLE));
                adc_start_conversion(cpu, module);
                return;
            }
            if (sampling &&
                (!was_sampling || !was_on || ((control ^ previous) & ADC_TRIGGER_MASK) != 0u)) {
                adc_begin_sampling(cpu, module);
                return;
            }
            if ((control & ADC_AUTO_SAMPLE) != 0u &&
                (!was_on || (previous & ADC_AUTO_SAMPLE) == 0u)) {
                adc_begin_sampling(cpu, module);
                return;
            }
            if (was_sampling && !sampling) {
                cpu->io.adc_generation[module]++;
            }
            return;
        }
        if (module == 0u && (control & ADC_12_BIT) != 0u) {
            if (address == control_address + 2u) {
                raw_write_word(cpu, address,
                               (uint16_t)(raw_word(cpu, address) & ~ADC_CHANNELS_MASK));
            } else if (address == control_address + 6u) {
                raw_write_word(cpu, address, 0u);
            }
        }
        return;
    }
}

static void update_pwm_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t current;
    uint16_t changed;
    uint8_t first;
    uint8_t count;
    uint8_t index;
    if (address == 0x0760u) {
        current = raw_word(cpu, address);
        changed = (uint16_t)((current ^ previous) & 0x0200u);
        first = 0u;
        count = 1u;
    } else if (address == 0x076au) {
        current = raw_word(cpu, address);
        uint16_t available = (uint16_t)(((1u << pwm_generator_count(cpu)) - 1u) << 8u);
        changed = (uint16_t)((current ^ previous) & available);
        first = 1u;
        count = pwm_generator_count(cpu);
    } else {
        return;
    }
    for (index = 0u; index < count; index++) {
        uint8_t source = (uint8_t)(first + index);
        uint16_t mask = address == 0x0760u ? 0x0200u : (uint16_t)(0x0100u << index);
        if ((changed & mask) == 0u) {
            continue;
        }
        cpu->io.pwm_pmd_generation[source]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_PWM_PMD, source,
                ((uint32_t)cpu->io.pwm_pmd_generation[source] << PWM_PMD_EVENT_GENERATION_SHIFT) |
                    ((current & mask) != 0u ? PWM_PMD_EVENT_DISABLED : 0u),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            uint8_t invalidate;
            raw_write_word(cpu, address, previous);
            for (invalidate = 0u; invalidate < count; invalidate++) {
                uint16_t invalidate_mask =
                    address == 0x0760u ? 0x0200u : (uint16_t)(0x0100u << invalidate);
                if ((changed & invalidate_mask) != 0u) {
                    cpu->io.pwm_pmd_generation[first + invalidate]++;
                }
            }
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
}

static void update_pwm_register(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t primary = raw_word(cpu, PWM_GLOBAL_BASE);
    bool enabled = (primary & PWM_ENABLE) != 0u;
    uint8_t generator;
    if (address == PWM_GLOBAL_BASE) {
        if ((primary & PWM_SPECIAL_INTERRUPT) == 0u) {
            raw_write_word(cpu, address, (uint16_t)(primary & ~PWM_SPECIAL_STATUS));
        }
        if ((previous & PWM_ENABLE) == 0u && enabled) {
            pwm_start(cpu);
        } else if ((previous & PWM_ENABLE) != 0u && !enabled) {
            memset(cpu->io.pwm, 0, sizeof(cpu->io.pwm));
            memset(cpu->io.pwm_fraction, 0, sizeof(cpu->io.pwm_fraction));
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 2u && enabled) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == PWM_GLOBAL_BASE + 4u) {
        if (!enabled || (primary & 0x0400u) != 0u) {
            cpu->io.pwm_active_period[0] = raw_word(cpu, address);
            cpu->io.pwm_period_update &= 0xfeu;
        } else {
            cpu->io.pwm_period_update |= 1u;
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x0eu) {
        uint16_t control = raw_word(cpu, address);
        if ((control & PWM_SPECIAL_INTERRUPT) == 0u) {
            raw_write_word(cpu, address, (uint16_t)(control & ~PWM_SPECIAL_STATUS));
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x10u && enabled) {
        raw_write_word(cpu, address, previous);
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x12u) {
        uint16_t secondary = raw_word(cpu, PWM_GLOBAL_BASE + 0x0eu);
        if (!enabled || (secondary & 0x0400u) != 0u) {
            cpu->io.pwm_active_period[1] = raw_word(cpu, address);
            cpu->io.pwm_period_update &= 0xfdu;
        } else {
            cpu->io.pwm_period_update |= 2u;
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x0au) {
        for (generator = 0u; generator < pwm_generator_count(cpu); generator++) {
            uint8_t bit = (uint8_t)(1u << generator);
            if (!enabled || (pwm_register(cpu, generator, 0u) & PWM_IMMEDIATE_UPDATE) != 0u) {
                pwm_latch_generator(cpu, generator);
                cpu->io.pwm_timing_update &= (uint8_t)~bit;
                if (enabled) {
                    pwm_update_output(cpu, generator);
                }
            } else if ((pwm_register(cpu, generator, 0u) & PWM_MASTER_DUTY) != 0u) {
                cpu->io.pwm_timing_update |= bit;
            }
        }
        return;
    }
    if (address < PWM_GENERATOR_BASE ||
        address >= PWM_GENERATOR_BASE + pwm_generator_count(cpu) * PWM_GENERATOR_STRIDE) {
        return;
    }
    generator = (uint8_t)((address - PWM_GENERATOR_BASE) / PWM_GENERATOR_STRIDE);
    {
        uint16_t base = pwm_generator_base(generator);
        uint16_t offset = (uint16_t)(address - base);
        uint16_t control = raw_word(cpu, base);
        uint16_t fault = raw_word(cpu, (uint16_t)(base + 4u));
        uint8_t bit = (uint8_t)(1u << generator);
        bool timing_register = offset == 6u || offset == 8u || offset == 0x0au || offset == 0x0cu ||
                               offset == 0x0eu || offset == 0x10u;
        if (offset == 0u) {
            if ((control & PWM_FAULT_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_FAULT_STATUS;
                if (!pwm_fault_active(cpu, generator)) {
                    cpu->io.pwm_fault_release |= bit;
                }
            }
            if ((control & PWM_CURRENT_LIMIT_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_CURRENT_LIMIT_STATUS;
            }
            if ((control & PWM_TRIGGER_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_TRIGGER_STATUS;
            }
            raw_write_word(cpu, base, control);
        }
        if (offset == 4u && (fault & PWM_FAULT_MODE_MASK) == PWM_FAULT_DISABLED) {
            cpu->io.pwm_fault_release |= bit;
        }
        if (offset == 2u &&
            (!enabled || (pwm_register(cpu, generator, 2u) & PWM_OVERRIDE_SYNCHRONIZED) == 0u)) {
            cpu->io.pwm_active_io[generator] = pwm_register(cpu, generator, 2u);
        }
        if (!enabled || (control & PWM_IMMEDIATE_UPDATE) != 0u) {
            pwm_latch_generator(cpu, generator);
            cpu->io.pwm_timing_update &= (uint8_t)~bit;
        } else if (timing_register) {
            cpu->io.pwm_timing_update |= bit;
        }
        pwm_refresh_status(cpu, generator);
        if (enabled) {
            pwm_update_output(cpu, generator);
        } else {
            cpu->io.pwm[generator * 2u] = 0u;
            cpu->io.pwm[generator * 2u + 1u] = 0u;
        }
    }
}

static void update_uart_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                 uint16_t requested) {
    static const uint16_t pmd_addresses[DSPIC33_UART_COUNT] = {0x0760u, 0x0760u, 0x0764u, 0x0766u};
    static const uint16_t pmd_masks[DSPIC33_UART_COUNT] = {0x0020u, 0x0040u, 0x0008u, 0x0020u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t base = uart_bases[channel];
        uint16_t offset = (uint16_t)(address - base);
        if (offset > 8u || (offset & 1u) != 0u) {
            continue;
        }
        if (offset == 0u) {
            uint16_t mode = raw_word(cpu, base);
            cpu->io.uart_rx_selection[channel] = 0xffu;
            if (((previous ^ mode) & UART_MODE_AUTO_BAUD) != 0u) {
                uart_reset_auto_baud(cpu, channel);
            }
            if ((cpu->io.uart_rx_active & (uint8_t)(1u << channel)) != 0u &&
                ((previous ^ mode) &
                 (UART_MODE_ENABLE | UART_MODE_IREN | UART_MODE_LOOPBACK | UART_MODE_HIGH_SPEED |
                  UART_MODE_DATA_MASK | UART_MODE_TWO_STOP_BITS | 0x0010u)) != 0u) {
                uart_cancel_physical_receive(cpu, channel);
            }
            if ((previous & UART_MODE_ENABLE) != 0u && (mode & UART_MODE_ENABLE) == 0u) {
                uart_disable_module(cpu, channel);
            } else {
                uart_refresh_status(cpu, channel);
            }
            return;
        }
        if (offset == 2u) {
            uint16_t status = raw_word(cpu, (uint16_t)(base + 2u));
            bool transmitter_was_enabled = (previous & UART_STATUS_TX_ENABLE) != 0u;
            bool transmitter_enabled;
            status = (uint16_t)((status & ~UART_STATUS_OVERRUN) |
                                (previous & requested & UART_STATUS_OVERRUN));
            if ((raw_word(cpu, base) & UART_MODE_ENABLE) == 0u) {
                status &= (uint16_t)~(UART_STATUS_TX_ENABLE | UART_STATUS_BREAK);
            }
            raw_write_word(cpu, (uint16_t)(base + 2u), status);
            transmitter_enabled = (status & UART_STATUS_TX_ENABLE) != 0u;
            if ((previous & UART_STATUS_OVERRUN) != 0u && (requested & UART_STATUS_OVERRUN) == 0u) {
                uart_clear_receive(cpu, channel);
            }
            if (transmitter_was_enabled && !transmitter_enabled) {
                uart_clear_transmit(cpu, channel);
            } else if (!transmitter_was_enabled && transmitter_enabled) {
                uart_raise_transmit(cpu, channel, uart_transmit_interrupt_mode(cpu, channel) == 0u);
                uart_start_transmit(cpu, channel);
            }
            uart_refresh_status(cpu, channel);
            return;
        }
        if (offset == 4u) {
            Dspic33UartFrame frame;
            memset(&frame, 0, sizeof(frame));
            frame.value = requested & 0x01ffu;
            raw_write_word(cpu, (uint16_t)(base + 4u), 0u);
            if (!uart_module_disabled(cpu, channel) &&
                (raw_word(cpu, base) & UART_MODE_ENABLE) != 0u &&
                uart_fifo_push(&cpu->io.uart_tx_fifo[channel], &frame)) {
                uart_start_transmit(cpu, channel);
            }
            uart_refresh_status(cpu, channel);
            return;
        }
        if (offset == 6u) {
            uart_refresh_status(cpu, channel);
            return;
        }
        return;
    }
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        if (address == pmd_addresses[channel]) {
            bool was_disabled = (previous & pmd_masks[channel]) != 0u;
            bool disabled = uart_module_disabled(cpu, channel);
            if (!was_disabled && disabled) {
                uart_reset_runtime(cpu, channel);
            } else if (was_disabled && !disabled) {
                uart_refresh_status(cpu, channel);
            }
        }
    }
}

static void uart_read_complete(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33UartFrame discarded;
    if (!uart_fifo_pop(&cpu->io.uart_rx_fifo[channel], &discarded)) {
        return;
    }
    if ((cpu->io.uart_rx_hold_valid & bit) != 0u &&
        uart_fifo_push(&cpu->io.uart_rx_fifo[channel], &cpu->io.uart_rx_hold[channel])) {
        memset(&cpu->io.uart_rx_hold[channel], 0, sizeof(cpu->io.uart_rx_hold[channel]));
        cpu->io.uart_rx_hold_valid &= (uint8_t)~bit;
    }
    uart_refresh_status(cpu, channel);
    refresh_physical_pin_inputs(cpu);
}

static void spi_restore_buffer(Dspic33* cpu, uint8_t channel, uint16_t fallback) {
    uint16_t value;
    if (!word_queue_front(&cpu->io.spi_rx_fifo[channel], &value)) {
        value = fallback;
    }
    raw_write_word(cpu, (uint16_t)(spi_bases[channel] + 8u), value);
}

static bool spi_read_complete(const Dspic33* cpu, uint16_t address) {
    if (cpu->io.dma_transfer_active) {
        return cpu->io.dma_transfer_width == 1u || (address & 1u) != 0u;
    }
    if (cpu->io.cpu_read_valid) {
        return cpu->io.cpu_read_width == 1u || address == cpu->io.cpu_read_address + 1u;
    }
    return (address & 1u) != 0u;
}

static void update_spi_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = spi_bases[channel];
        uint16_t offset;
        uint16_t control;
        uint16_t value;
        uint8_t capacity;
        if (address < base || address > base + 8u) {
            continue;
        }
        offset = (uint16_t)(address - base);
        if (offset == 0u) {
            uint16_t status = raw_word(cpu, base);
            if ((requested & SPI_OVERFLOW) != 0u) {
                status = (uint16_t)((status & ~SPI_OVERFLOW) | (previous & SPI_OVERFLOW));
            } else {
                status &= (uint16_t)~SPI_OVERFLOW;
            }
            raw_write_word(cpu, base, status);
            if ((previous & SPI_ENABLE) == 0u && (status & SPI_ENABLE) != 0u) {
                cpu->io.spi_clock_start_cycle[channel] = cpu->device_cycles;
            }
            if ((status & SPI_ENABLE) == 0u) {
                spi_clear_buffers(cpu, channel);
            } else {
                spi_refresh_status(cpu, channel);
            }
            return;
        }
        if (offset == 2u) {
            control = raw_word(cpu, (uint16_t)(base + 2u));
            if ((control & SPI_MASTER) == 0u) {
                control &= (uint16_t)~SPI_SAMPLE_END;
                raw_write_word(cpu, (uint16_t)(base + 2u), control);
            }
            if (((control ^ previous) & (SPI_MODE_16 | SPI_MASTER)) != 0u) {
                spi_clear_buffers(cpu, channel);
            } else if ((previous & SPI_DISABLE_CLOCK) != 0u &&
                       (control & SPI_DISABLE_CLOCK) == 0u) {
                cpu->io.spi_generation[channel] =
                    (uint16_t)((cpu->io.spi_generation[channel] + 1u) & SPI_EVENT_GENERATION_MASK);
                spi_schedule_current(cpu, channel);
            }
            return;
        }
        if (offset == 4u) {
            control = raw_word(cpu, (uint16_t)(base + 4u));
            if (((control ^ previous) & SPI_ENHANCED) != 0u) {
                spi_clear_buffers(cpu, channel);
            } else if (((control ^ previous) & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE |
                                                SPI_FRAME_ACTIVE_HIGH | SPI_FRAME_DELAY)) != 0u) {
                uint8_t bit = (uint8_t)(1u << channel);
                cpu->io.spi_frame_active &= (uint8_t)~(1u << channel);
                cpu->io.spi_frame_output_pending &= (uint8_t)~bit;
                cpu->io.spi_frame_output_clear_pending &= (uint8_t)~bit;
                if ((cpu->io.spi_busy & bit) == 0u) {
                    cpu->io.spi_generation[channel] =
                        (uint16_t)((cpu->io.spi_generation[channel] + 1u) &
                                   SPI_EVENT_GENERATION_MASK);
                    spi_remove_internal_events(cpu, channel);
                }
                if ((previous & SPI_FRAME_ENABLE) == 0u && (control & SPI_FRAME_ENABLE) != 0u) {
                    cpu->io.spi_clock_start_cycle[channel] = cpu->device_cycles;
                }
                spi_start_next(cpu, channel);
            }
            return;
        }
        if (offset != 8u) {
            return;
        }
        value = raw_word(cpu, (uint16_t)(base + 8u));
        if ((raw_word(cpu, (uint16_t)(base + 2u)) & SPI_MODE_16) == 0u) {
            value &= 0x00ffu;
        }
        capacity = spi_enhanced(cpu, channel) ? 8u : 1u;
        if ((raw_word(cpu, base) & SPI_ENABLE) == 0u || spi_module_disabled(cpu, channel) ||
            cpu->io.spi_tx_fifo[channel].count >= capacity ||
            !word_queue_push(&cpu->io.spi_tx_fifo[channel], value)) {
            spi_restore_buffer(cpu, channel, previous);
            spi_refresh_status(cpu, channel);
            return;
        }
        if (spi_enhanced(cpu, channel) && cpu->io.spi_tx_fifo[channel].count == 8u) {
            spi_raise_mode(cpu, channel, 7u);
        }
        spi_start_next(cpu, channel);
        spi_restore_buffer(cpu, channel, previous);
        spi_refresh_status(cpu, channel);
        return;
    }
    if (address == 0x0760u || address == 0x076au) {
        for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
            if (spi_module_disabled(cpu, channel)) {
                spi_clear_buffers(cpu, channel);
            }
        }
    }
}
