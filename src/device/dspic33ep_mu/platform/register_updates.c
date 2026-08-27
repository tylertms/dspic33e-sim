#include "device/dspic33ep_mu/internal.h"

static bool platform_pmd_location(uint8_t source, uint16_t* address, uint16_t* mask) {
    static const uint16_t addresses[PLATFORM_PMD_COUNT] = {
        0x0760u, 0x0760u, 0x0764u, 0x0766u, 0x0760u, 0x0760u, 0x076au, 0x076au,
        0x0760u, 0x0760u, 0x076cu, 0x076cu, 0x076cu, 0x076cu, 0x0766u,
    };
    static const uint16_t masks[PLATFORM_PMD_COUNT] = {
        0x0020u, 0x0040u, 0x0008u, 0x0020u, 0x0008u, 0x0010u, 0x0001u, 0x0002u,
        0x0002u, 0x0004u, 0x0010u, 0x0020u, 0x0040u, 0x0080u, 0x0008u,
    };
    if (source >= PLATFORM_PMD_COUNT) {
        return false;
    }
    *address = addresses[source];
    *mask = masks[source];
    return true;
}

bool dspic33_device_internal_platform_pmd_disabled(const Dspic33* cpu, uint8_t source) {
    return source >= PLATFORM_PMD_COUNT ||
           (cpu->io.platform_pmd_disabled & (uint16_t)(1u << source)) != 0u;
}

void dspic33_device_internal_run_platform_pmd(Dspic33* cpu, uint16_t source, uint32_t value) {
    uint16_t source_bit;
    bool disabled;
    if (source >= PLATFORM_PMD_COUNT ||
        (uint16_t)(value >> PLATFORM_PMD_EVENT_GENERATION_SHIFT) !=
            cpu->io.platform_pmd_generation[source]) {
        return;
    }
    source_bit = (uint16_t)(1u << source);
    disabled = (value & PLATFORM_PMD_EVENT_DISABLED) != 0u;
    if (disabled) {
        cpu->io.platform_pmd_disabled |= source_bit;
    } else {
        cpu->io.platform_pmd_disabled &= (uint16_t)~source_bit;
    }
    if (source < PLATFORM_PMD_SPI_BASE) {
        uint8_t channel = (uint8_t)(source - PLATFORM_PMD_UART_BASE);
        if (disabled) {
            dspic33_device_internal_uart_reset_runtime(cpu, channel);
        } else {
            dspic33_device_internal_uart_refresh_status(cpu, channel);
        }
    } else if (source < PLATFORM_PMD_CAN_BASE) {
        uint8_t channel = (uint8_t)(source - PLATFORM_PMD_SPI_BASE);
        if (disabled) {
            dspic33_device_internal_spi_clear_buffers(cpu, channel);
        } else {
            dspic33_device_internal_spi_refresh_status(cpu, channel);
        }
    } else if (source < PLATFORM_PMD_DMA_BASE) {
        uint8_t channel = (uint8_t)(source - PLATFORM_PMD_CAN_BASE);
        uint8_t channel_bit = (uint8_t)(1u << channel);
        if (disabled) {
            cpu->io.can_rx_busy &= (uint8_t)~channel_bit;
            cpu->io.can_tx_busy &= (uint8_t)~channel_bit;
        }
    } else if (source < PLATFORM_PMD_REFERENCE_CLOCK && disabled) {
        uint8_t first_channel = (uint8_t)((source - PLATFORM_PMD_DMA_BASE) * 4u);
        uint8_t last_channel = (uint8_t)(first_channel + 4u);
        if (last_channel > DSPIC33_DMA_COUNT) {
            last_channel = DSPIC33_DMA_COUNT;
        }
        for (uint8_t channel = first_channel; channel < last_channel; channel++) {
            uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel);
            uint16_t request_address =
                (uint16_t)(dspic33_device_internal_dma_channel_base(channel) + 2u);
            cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
            cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
            cpu->io.dma_active &= (uint16_t)~channel_bit;
            cpu->io.dma_arbiter_waiting &= (uint16_t)~channel_bit;
            dspic33_device_internal_dma_advance_generation(cpu, channel);
            dspic33_device_internal_raw_write_word(
                cpu, request_address,
                (uint16_t)(dspic33_device_internal_raw_word(cpu, request_address) &
                           ~DMA_REQ_FORCE));
        }
    }
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

void dspic33_device_internal_update_platform_pmd(Dspic33* cpu, uint16_t address,
                                                 uint16_t previous) {
    uint16_t current = dspic33_device_internal_raw_word(cpu, address);
    uint16_t changed_sources = 0u;
    for (uint8_t source = 0u; source < PLATFORM_PMD_COUNT; source++) {
        uint16_t source_address;
        uint16_t source_mask;
        if (!platform_pmd_location(source, &source_address, &source_mask) ||
            source_address != address || ((current ^ previous) & source_mask) == 0u) {
            continue;
        }
        changed_sources |= (uint16_t)(1u << source);
        cpu->io.platform_pmd_generation[source]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_PLATFORM_PMD, source,
                ((uint32_t)cpu->io.platform_pmd_generation[source]
                 << PLATFORM_PMD_EVENT_GENERATION_SHIFT) |
                    ((current & source_mask) != 0u ? PLATFORM_PMD_EVENT_DISABLED : 0u),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            dspic33_device_internal_raw_write_word(cpu, address, previous);
            for (uint8_t invalidate = 0u; invalidate < PLATFORM_PMD_COUNT; invalidate++) {
                if ((changed_sources & (uint16_t)(1u << invalidate)) != 0u) {
                    cpu->io.platform_pmd_generation[invalidate]++;
                }
            }
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
}

void dspic33_device_internal_update_timer_register(Dspic33* cpu, uint16_t address,
                                                   uint16_t previous) {
    uint8_t timer;
    if (address == dspic33_device_timer_registers[0] &&
        (dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[0]) &
         (TIMER_ON | TIMER_SYNC | TIMER_EXTERNAL)) == (TIMER_ON | TIMER_SYNC | TIMER_EXTERNAL)) {
        dspic33_device_internal_raw_write_word(cpu, address, previous);
        return;
    }
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        if (dspic33_device_internal_timer_pmd_disabled(cpu, timer) &&
            (address == dspic33_device_timer_controls[timer] ||
             address == dspic33_device_timer_registers[timer] ||
             address == dspic33_device_timer_periods[timer] ||
             (dspic33_device_internal_timer_is_type_b(timer) &&
              address == dspic33_device_timer_holding_registers[timer / 2u]))) {
            dspic33_device_internal_raw_write_word(cpu, address, previous);
            return;
        }
        if ((address & 0xfffeu) == dspic33_device_timer_controls[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            if ((dspic33_device_internal_raw_word(cpu, dspic33_device_timer_controls[timer]) &
                 TIMER_ON) != 0u) {
                cpu->io.timer_enabled |= (uint16_t)(1u << timer);
            } else {
                cpu->io.timer_enabled &= (uint16_t)~(1u << timer);
            }
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            return;
        }
        if ((address & 0xfffeu) == dspic33_device_timer_registers[timer]) {
            uint16_t bit = (uint16_t)(1u << timer);
            cpu->io.timer_fraction[timer] = 0u;
            cpu->io.timer_external_started &= (uint16_t)~bit;
            if (dspic33_device_internal_timer_pair_enabled(cpu, timer)) {
                dspic33_device_internal_raw_write_word(
                    cpu, dspic33_device_timer_registers[timer + 1u],
                    dspic33_device_internal_raw_word(
                        cpu, dspic33_device_timer_holding_registers[timer / 2u]));
            }
            return;
        }
    }
}
void dspic33_device_internal_run_timer_pmd(Dspic33* cpu, uint16_t timer, uint32_t value) {
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

void dspic33_device_internal_update_timer_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t current;
    uint16_t changed;
    uint8_t timer;
    if (address != 0x0760u) {
        return;
    }
    current = dspic33_device_internal_raw_word(cpu, address);
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
            dspic33_device_internal_raw_write_word(cpu, address, previous);
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

bool dspic33_device_internal_adc_module_address(uint16_t address, uint8_t module) {
    if (address >= dspic33_device_adc_buffers[module] &&
        address < dspic33_device_adc_buffers[module] + 0x20u) {
        return true;
    }
    return address >= dspic33_device_adc_controls[module] &&
           address <= dspic33_device_adc_controls[module] + 0x12u;
}

bool dspic33_device_internal_adc_pmd_disabled(const Dspic33* cpu, uint8_t module) {
    return (cpu->io.adc_pmd_disabled & (uint8_t)(1u << module)) != 0u;
}

static void adc_reset_module(Dspic33* cpu, uint8_t module) {
    uint16_t control = dspic33_device_adc_controls[module];
    uint8_t bit = (uint8_t)(1u << module);
    uint8_t index;
    cpu->io.adc_generation[module]++;
    cpu->io.adc_latched_count[module] = 0u;
    cpu->io.adc_conversion_index[module] = 0u;
    cpu->io.adc_buffer_index[module] = 0u;
    cpu->io.adc_sample_count[module] = 0u;
    cpu->io.adc_scan_index[module] = 0u;
    cpu->io.adc_dma_index[module] = 0u;
    cpu->io.adc_mux_b &= (uint8_t)~bit;
    cpu->io.adc_sleep_disabled &= (uint8_t)~bit;
    for (index = 0u; index < 5u; index++) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(control + index * 2u), 0u);
    }
    if (module == 0u) {
        dspic33_device_internal_raw_write_word(cpu, 0x032eu, 0u);
        dspic33_device_internal_raw_write_word(cpu, 0x0330u, 0u);
        dspic33_device_internal_raw_write_word(cpu, 0x0332u, 0u);
    } else {
        dspic33_device_internal_raw_write_word(cpu, 0x0370u, 0u);
        dspic33_device_internal_raw_write_word(cpu, 0x0372u, 0u);
    }
}

void dspic33_device_internal_run_adc_pmd(Dspic33* cpu, uint16_t module, uint32_t value) {
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
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}

void dspic33_device_internal_update_adc_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint8_t module;
    uint16_t current;
    if (address != 0x0760u && address != 0x0764u) {
        return;
    }
    module = address == 0x0760u ? 0u : 1u;
    current = dspic33_device_internal_raw_word(cpu, address);
    if (((current ^ previous) & 1u) == 0u) {
        return;
    }
    cpu->io.adc_pmd_generation[module]++;
    if (!dspic33_schedule(
            cpu, DSPIC33_EVENT_ADC_PMD, module,
            ((uint32_t)cpu->io.adc_pmd_generation[module] << ADC_PMD_EVENT_GENERATION_SHIFT) |
                ((current & 1u) != 0u ? ADC_PMD_EVENT_DISABLED : 0u),
            dspic33_device_instruction_cycles(cpu, 1u))) {
        dspic33_device_internal_raw_write_word(cpu, address, previous);
        cpu->io.adc_pmd_generation[module]++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_update_adc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        uint16_t control_address = dspic33_device_adc_controls[module];
        uint16_t control;
        if (address != control_address && address != control_address + 2u &&
            address != control_address + 6u) {
            continue;
        }
        control = dspic33_device_internal_raw_word(cpu, control_address);
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
                dspic33_device_internal_raw_write_word(
                    cpu, (uint16_t)(control_address + 2u),
                    (uint16_t)(dspic33_device_internal_adc_register(cpu, module, 2u) &
                               ~ADC_CHANNELS_MASK));
                dspic33_device_internal_raw_write_word(cpu, (uint16_t)(control_address + 6u), 0u);
            }
            dspic33_device_internal_raw_write_word(cpu, control_address, control);
            on = (control & ADC_ON) != 0u;
            sampling = (control & ADC_SAMPLE) != 0u;
            source = (uint8_t)((control & ADC_TRIGGER_MASK) >> 4u);
            if (!on) {
                dspic33_device_internal_adc_abort(cpu, module);
                return;
            }
            if (!was_on) {
                cpu->io.adc_buffer_index[module] = 0u;
                cpu->io.adc_sample_count[module] = 0u;
                cpu->io.adc_scan_index[module] = 0u;
                cpu->io.adc_dma_index[module] = 0u;
                cpu->io.adc_mux_b &= (uint8_t)~(1u << module);
            }
            if (was_on && was_sampling && !sampling && source == 0u) {
                dspic33_device_internal_raw_write_word(cpu, control_address,
                                                       (uint16_t)(control | ADC_SAMPLE));
                dspic33_device_internal_adc_start_conversion(cpu, module);
                return;
            }
            if (sampling &&
                (!was_sampling || !was_on || ((control ^ previous) & ADC_TRIGGER_MASK) != 0u)) {
                dspic33_device_internal_adc_begin_sampling(cpu, module);
                return;
            }
            if ((control & ADC_AUTO_SAMPLE) != 0u &&
                (!was_on || (previous & ADC_AUTO_SAMPLE) == 0u)) {
                dspic33_device_internal_adc_begin_sampling(cpu, module);
                return;
            }
            if (was_sampling && !sampling) {
                cpu->io.adc_generation[module]++;
            }
            return;
        }
        if (module == 0u && (control & ADC_12_BIT) != 0u) {
            if (address == control_address + 2u) {
                dspic33_device_internal_raw_write_word(
                    cpu, address,
                    (uint16_t)(dspic33_device_internal_raw_word(cpu, address) &
                               ~ADC_CHANNELS_MASK));
            } else if (address == control_address + 6u) {
                dspic33_device_internal_raw_write_word(cpu, address, 0u);
            }
        }
        return;
    }
}

void dspic33_device_internal_update_pwm_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t current;
    uint16_t changed;
    uint8_t first;
    uint8_t count;
    uint8_t index;
    if (address == 0x0760u) {
        current = dspic33_device_internal_raw_word(cpu, address);
        changed = (uint16_t)((current ^ previous) & 0x0200u);
        first = 0u;
        count = 1u;
    } else if (address == 0x076au) {
        current = dspic33_device_internal_raw_word(cpu, address);
        uint16_t available =
            (uint16_t)(((1u << dspic33_device_internal_pwm_generator_count(cpu)) - 1u) << 8u);
        changed = (uint16_t)((current ^ previous) & available);
        first = 1u;
        count = dspic33_device_internal_pwm_generator_count(cpu);
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
            dspic33_device_internal_raw_write_word(cpu, address, previous);
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

void dspic33_device_internal_update_pwm_register(Dspic33* cpu, uint16_t address,
                                                 uint16_t previous) {
    uint16_t primary = dspic33_device_internal_raw_word(cpu, PWM_GLOBAL_BASE);
    bool enabled = (primary & PWM_ENABLE) != 0u;
    uint8_t generator;
    if (address == PWM_GLOBAL_BASE) {
        if ((primary & PWM_SPECIAL_INTERRUPT) == 0u) {
            dspic33_device_internal_raw_write_word(cpu, address,
                                                   (uint16_t)(primary & ~PWM_SPECIAL_STATUS));
        }
        if ((previous & PWM_ENABLE) == 0u && enabled) {
            dspic33_device_internal_pwm_start(cpu);
        } else if ((previous & PWM_ENABLE) != 0u && !enabled) {
            memset(cpu->io.pwm, 0, sizeof(cpu->io.pwm));
            memset(cpu->io.pwm_fraction, 0, sizeof(cpu->io.pwm_fraction));
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 2u && enabled) {
        dspic33_device_internal_raw_write_word(cpu, address, previous);
        return;
    }
    if (address == PWM_GLOBAL_BASE + 4u) {
        if (!enabled || (primary & 0x0400u) != 0u) {
            cpu->io.pwm_active_period[0] = dspic33_device_internal_raw_word(cpu, address);
            cpu->io.pwm_period_update &= 0xfeu;
        } else {
            cpu->io.pwm_period_update |= 1u;
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x0eu) {
        uint16_t control = dspic33_device_internal_raw_word(cpu, address);
        if ((control & PWM_SPECIAL_INTERRUPT) == 0u) {
            dspic33_device_internal_raw_write_word(cpu, address,
                                                   (uint16_t)(control & ~PWM_SPECIAL_STATUS));
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x10u && enabled) {
        dspic33_device_internal_raw_write_word(cpu, address, previous);
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x12u) {
        uint16_t secondary = dspic33_device_internal_raw_word(cpu, PWM_GLOBAL_BASE + 0x0eu);
        if (!enabled || (secondary & 0x0400u) != 0u) {
            cpu->io.pwm_active_period[1] = dspic33_device_internal_raw_word(cpu, address);
            cpu->io.pwm_period_update &= 0xfdu;
        } else {
            cpu->io.pwm_period_update |= 2u;
        }
        return;
    }
    if (address == PWM_GLOBAL_BASE + 0x0au) {
        for (generator = 0u; generator < dspic33_device_internal_pwm_generator_count(cpu);
             generator++) {
            uint8_t bit = (uint8_t)(1u << generator);
            if (!enabled || (dspic33_device_internal_pwm_register(cpu, generator, 0u) &
                             PWM_IMMEDIATE_UPDATE) != 0u) {
                dspic33_device_internal_pwm_latch_generator(cpu, generator);
                cpu->io.pwm_timing_update &= (uint8_t)~bit;
                if (enabled) {
                    dspic33_device_internal_pwm_update_output(cpu, generator);
                }
            } else if ((dspic33_device_internal_pwm_register(cpu, generator, 0u) &
                        PWM_MASTER_DUTY) != 0u) {
                cpu->io.pwm_timing_update |= bit;
            }
        }
        return;
    }
    if (address < PWM_GENERATOR_BASE ||
        address >= PWM_GENERATOR_BASE +
                       dspic33_device_internal_pwm_generator_count(cpu) * PWM_GENERATOR_STRIDE) {
        return;
    }
    generator = (uint8_t)((address - PWM_GENERATOR_BASE) / PWM_GENERATOR_STRIDE);
    {
        uint16_t base = dspic33_device_internal_pwm_generator_base(generator);
        uint16_t offset = (uint16_t)(address - base);
        uint16_t control = dspic33_device_internal_raw_word(cpu, base);
        uint16_t fault = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 4u));
        uint8_t bit = (uint8_t)(1u << generator);
        bool timing_register = offset == 6u || offset == 8u || offset == 0x0au || offset == 0x0cu ||
                               offset == 0x0eu || offset == 0x10u;
        if (offset == 0u) {
            if ((control & PWM_FAULT_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_FAULT_STATUS;
                if (!dspic33_device_internal_pwm_fault_active(cpu, generator)) {
                    cpu->io.pwm_fault_release |= bit;
                }
            }
            if ((control & PWM_CURRENT_LIMIT_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_CURRENT_LIMIT_STATUS;
            }
            if ((control & PWM_TRIGGER_INTERRUPT) == 0u) {
                control &= (uint16_t)~PWM_TRIGGER_STATUS;
            }
            dspic33_device_internal_raw_write_word(cpu, base, control);
        }
        if (offset == 4u && (fault & PWM_FAULT_MODE_MASK) == PWM_FAULT_DISABLED) {
            cpu->io.pwm_fault_release |= bit;
        }
        if (offset == 2u && (!enabled || (dspic33_device_internal_pwm_register(cpu, generator, 2u) &
                                          PWM_OVERRIDE_SYNCHRONIZED) == 0u)) {
            cpu->io.pwm_active_io[generator] =
                dspic33_device_internal_pwm_register(cpu, generator, 2u);
        }
        if (!enabled || (control & PWM_IMMEDIATE_UPDATE) != 0u) {
            dspic33_device_internal_pwm_latch_generator(cpu, generator);
            cpu->io.pwm_timing_update &= (uint8_t)~bit;
        } else if (timing_register) {
            cpu->io.pwm_timing_update |= bit;
        }
        dspic33_device_internal_pwm_refresh_status(cpu, generator);
        if (enabled) {
            dspic33_device_internal_pwm_update_output(cpu, generator);
        } else {
            cpu->io.pwm[generator * 2u] = 0u;
            cpu->io.pwm[generator * 2u + 1u] = 0u;
        }
    }
}

void dspic33_device_internal_update_uart_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                  uint16_t requested) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t base = dspic33_device_uart_bases[channel];
        uint16_t offset = (uint16_t)(address - base);
        if (offset > 8u || (offset & 1u) != 0u) {
            continue;
        }
        if (offset == 0u) {
            uint16_t mode = dspic33_device_internal_raw_word(cpu, base);
            cpu->io.uart_rx_selection[channel] = 0xffu;
            if (((previous ^ mode) & UART_MODE_AUTO_BAUD) != 0u) {
                dspic33_device_internal_uart_reset_auto_baud(cpu, channel);
            }
            if ((cpu->io.uart_rx_active & (uint8_t)(1u << channel)) != 0u &&
                ((previous ^ mode) &
                 (UART_MODE_ENABLE | UART_MODE_IREN | UART_MODE_LOOPBACK | UART_MODE_HIGH_SPEED |
                  UART_MODE_DATA_MASK | UART_MODE_TWO_STOP_BITS | 0x0010u)) != 0u) {
                dspic33_device_internal_uart_cancel_physical_receive(cpu, channel);
            }
            if ((previous & UART_MODE_ENABLE) != 0u && (mode & UART_MODE_ENABLE) == 0u) {
                dspic33_device_internal_uart_disable_module(cpu, channel);
            } else {
                dspic33_device_internal_uart_refresh_status(cpu, channel);
            }
            return;
        }
        if (offset == 2u) {
            uint16_t status = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
            bool transmitter_was_enabled = (previous & UART_STATUS_TX_ENABLE) != 0u;
            bool transmitter_enabled;
            status = (uint16_t)((status & ~UART_STATUS_OVERRUN) |
                                (previous & requested & UART_STATUS_OVERRUN));
            if ((dspic33_device_internal_raw_word(cpu, base) & UART_MODE_ENABLE) == 0u) {
                status &= (uint16_t)~(UART_STATUS_TX_ENABLE | UART_STATUS_BREAK);
            }
            dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 2u), status);
            transmitter_enabled = (status & UART_STATUS_TX_ENABLE) != 0u;
            if ((previous & UART_STATUS_OVERRUN) != 0u && (requested & UART_STATUS_OVERRUN) == 0u) {
                dspic33_device_internal_uart_clear_receive(cpu, channel);
            }
            if (transmitter_was_enabled && !transmitter_enabled) {
                dspic33_device_internal_uart_clear_transmit(cpu, channel);
            } else if (!transmitter_was_enabled && transmitter_enabled) {
                dspic33_device_internal_uart_raise_transmit(
                    cpu, channel,
                    dspic33_device_internal_uart_transmit_interrupt_mode(cpu, channel) == 0u);
                dspic33_device_internal_uart_start_transmit(cpu, channel);
            }
            dspic33_device_internal_uart_refresh_status(cpu, channel);
            return;
        }
        if (offset == 4u) {
            Dspic33UartFrame frame;
            memset(&frame, 0, sizeof(frame));
            frame.value = requested & 0x01ffu;
            dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 4u), 0u);
            if (!dspic33_device_internal_uart_module_disabled(cpu, channel) &&
                (dspic33_device_internal_raw_word(cpu, base) & UART_MODE_ENABLE) != 0u &&
                dspic33_device_internal_uart_fifo_push(&cpu->io.uart_tx_fifo[channel], &frame)) {
                dspic33_device_internal_uart_start_transmit(cpu, channel);
            }
            dspic33_device_internal_uart_refresh_status(cpu, channel);
            return;
        }
        if (offset == 6u) {
            dspic33_device_internal_uart_refresh_status(cpu, channel);
            return;
        }
        return;
    }
}
