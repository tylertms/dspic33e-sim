#include "device/dspic33ep_mu/internal.h"

void dspic33_device_write_byte(Dspic33* cpu, uint16_t address, uint16_t previous_value) {
    const uint16_t register_address = (uint16_t)(address & 0xfffeu);
    const uint16_t requested_value = dspic33_device_internal_raw_word(cpu, register_address);
    uint16_t writable;
    uint8_t channel;

    if (dspic33_device_internal_protect_oscillator_write(cpu, address, previous_value)) {
        return;
    }
    if (register_address >= 0x0680u && register_address <= 0x06f6u &&
        !dspic33_device_internal_pps_register_write_mask(cpu, register_address, &writable)) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        return;
    }
    if (register_address >= 0x0680u && register_address <= 0x06f6u &&
        (dspic33_device_internal_raw_word(cpu, 0x0742u) & 0x0040u) != 0u) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        return;
    }
    if (dspic33_i2c_write_register(cpu, address, previous_value, requested_value)) {
        return;
    }
    if (cpu->io.usb_pmd_disabled &&
        dspic33_device_internal_usb_register_address(register_address)) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        return;
    }
    for (channel = 0u; channel < DSPIC33_ADC_COUNT; channel++) {
        if (dspic33_device_internal_adc_pmd_disabled(cpu, channel) &&
            dspic33_device_internal_adc_module_address(register_address, channel)) {
            dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
            return;
        }
    }
    if (dspic33_device_internal_pwm_address_inaccessible(cpu, register_address)) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        return;
    }
    if (dspic33_device_internal_register_write_mask(cpu, register_address, &writable) ||
        dspic33_device_internal_input_capture_register_write_mask(register_address, &writable) ||
        dspic33_device_internal_output_compare_register_write_mask(register_address, &writable) ||
        dspic33_device_internal_comparator_register_write_mask(register_address, &writable) ||
        dspic33_device_internal_adc_register_write_mask(register_address, &writable) ||
        dspic33_device_internal_pwm_register_write_mask(cpu, register_address, &writable) ||
        dspic33_device_internal_uart_register_write_mask(cpu, register_address, &writable) ||
        dspic33_device_internal_spi_register_write_mask(register_address, &writable) ||
        dspic33_device_internal_can_register_write_mask(cpu, register_address, &writable) ||
        dspic33_device_internal_usb_register_write_mask(cpu, register_address, previous_value,
                                                        &writable) ||
        dspic33_device_internal_dma_register_write_mask(register_address, &writable)) {
        dspic33_device_internal_raw_write_word(
            cpu, register_address,
            (uint16_t)((previous_value & ~writable) | (requested_value & writable)));
    }
    dspic33_i2c_update_pmd(cpu, register_address, previous_value);
    if (register_address == PMP_PMD_ADDRESS) {
        dspic33_device_internal_update_pmp_pmd(cpu, previous_value);
    }
    dspic33_device_internal_update_input_capture_pmd(cpu, register_address, previous_value);
    dspic33_device_internal_update_output_compare_pmd(cpu, register_address, previous_value);
    dspic33_device_internal_update_timer_pmd(cpu, register_address, previous_value);
    dspic33_device_internal_update_adc_pmd(cpu, register_address, previous_value);
    dspic33_device_internal_update_pwm_pmd(cpu, register_address, previous_value);
    dspic33_device_internal_update_usb_pmd(cpu, register_address, previous_value);
    if (register_address == 0x0740u && (cpu->configuration[10u] & 0x80u) == 0u &&
        (previous_value & 0x0020u) == 0u &&
        (dspic33_device_internal_raw_word(cpu, register_address) & 0x0020u) != 0u) {
        cpu->watchdog.ticks = 0u;
    }
    dspic33_device_internal_pps_update_shadow(cpu, register_address);
    dspic33_device_internal_refresh_can_pps_inputs(cpu);
    dspic33_device_internal_update_gpio_latch(cpu, address, requested_value);
    if (register_address >= 0x0800u && register_address < 0x0800u + DSPIC33_IRQ_GROUP_COUNT * 2u) {
        const uint16_t interrupt_group = (uint16_t)((register_address - 0x0800u) / 2u);
        const uint16_t current_status = dspic33_device_internal_raw_word(cpu, register_address);
        const uint16_t cleared_flags = (uint16_t)(previous_value & ~current_status);
        cpu->interrupt_deferred[interrupt_group] &= (uint16_t)~cleared_flags;
        cpu->interrupt_deferred_next[interrupt_group] =
            (uint16_t)((cpu->interrupt_deferred_next[interrupt_group] & ~cleared_flags) |
                       (current_status & ~previous_value));
    }
    dspic33_device_internal_interrupt_control_write(cpu, register_address, previous_value,
                                                    requested_value);
    if (register_address == AUXILIARY_CLOCK_CONTROL) {
        if (dspic33_device_internal_auxiliary_clock_configuration_locked(cpu)) {
            dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        } else {
            const uint16_t clock_control = (uint16_t)((previous_value & ~AUXILIARY_CLOCK_WRITABLE) |
                                                      (requested_value & AUXILIARY_CLOCK_WRITABLE));
            dspic33_device_internal_raw_write_word(cpu, register_address, clock_control);
            if (dspic33_device_internal_auxiliary_pll_reconfiguration(previous_value,
                                                                      clock_control)) {
                dspic33_device_internal_reconfigure_auxiliary_pll(cpu);
            }
        }
    }
    if (register_address == AUXILIARY_CLOCK_DIVISOR) {
        if (dspic33_device_internal_auxiliary_clock_configuration_locked(cpu)) {
            dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        } else {
            const uint16_t divisor_value = requested_value & AUXILIARY_CLOCK_DIVISOR_WRITABLE;
            dspic33_device_internal_raw_write_word(cpu, register_address, divisor_value);
            if (((previous_value ^ divisor_value) & AUXILIARY_CLOCK_DIVISOR_WRITABLE) != 0u) {
                dspic33_device_internal_reconfigure_auxiliary_pll(cpu);
            }
        }
    }
    dspic33_device_internal_update_main_clock_configuration(cpu, register_address, previous_value);
    if (register_address == 0x0748u &&
        dspic33_device_internal_auxiliary_pll_input(
            dspic33_device_internal_raw_word(cpu, AUXILIARY_CLOCK_CONTROL)) == 1u &&
        ((previous_value ^ dspic33_device_internal_raw_word(cpu, register_address)) & 0x003fu) !=
            0u) {
        dspic33_device_internal_reconfigure_auxiliary_pll(cpu);
    }
    if (register_address == REFERENCE_CLOCK_CONTROL &&
        (previous_value & REFERENCE_CLOCK_ENABLE) != 0u) {
        const uint16_t reference_control = dspic33_device_internal_raw_word(cpu, register_address);
        dspic33_device_internal_raw_write_word(
            cpu, register_address,
            (uint16_t)((reference_control & ~REFERENCE_CLOCK_DIVISOR) |
                       (previous_value & REFERENCE_CLOCK_DIVISOR)));
    }
    dspic33_device_internal_update_timer_register(cpu, register_address, previous_value);
    dspic33_device_internal_update_adc_register(cpu, register_address, previous_value,
                                                requested_value);
    dspic33_device_internal_update_pwm_register(cpu, register_address, previous_value);
    dspic33_device_internal_update_spi_register(cpu, register_address, previous_value,
                                                requested_value);
    dspic33_device_internal_update_can_register(cpu, register_address, previous_value,
                                                requested_value);
    dspic33_device_internal_update_usb_register(cpu, register_address, previous_value,
                                                requested_value);
    dspic33_device_internal_update_crc_register(cpu, address, previous_value, requested_value);
    dspic33_device_internal_update_pmp_register(cpu, address, previous_value);
    dspic33_device_internal_update_input_capture_register(cpu, register_address, previous_value);
    dspic33_device_internal_update_output_compare_register(cpu, register_address, previous_value);
    dspic33_device_internal_update_comparator_register(cpu, register_address, previous_value,
                                                       requested_value);
    dspic33_device_internal_update_rtcc_register(cpu, address, previous_value);
    dspic33_device_internal_update_qei_register(cpu, register_address, previous_value,
                                                requested_value);
    dspic33_device_internal_update_dci_register(cpu, register_address, previous_value);
    dspic33_device_internal_update_uart_register(cpu, register_address, previous_value,
                                                 requested_value);
    if (register_address == NVM_KEY && (cpu->io.cpu_write_width == 2u || address == NVM_KEY)) {
        dspic33_device_internal_update_nvm_key(cpu, requested_value);
    } else if (register_address == NVM_CONTROL) {
        dspic33_device_internal_update_nvm_control(cpu, requested_value);
    }
    if (register_address >= DMA_CHANNEL_BASE &&
        register_address < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        channel = (uint8_t)((register_address - DMA_CHANNEL_BASE) / DMA_CHANNEL_STRIDE);
        if ((register_address & 0x000fu) == 0u) {
            dspic33_device_internal_update_dma_control(cpu, channel, previous_value);
        } else if ((register_address & 0x000fu) == 2u) {
            dspic33_device_internal_update_dma_request(cpu, channel, previous_value);
        }
    }
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}
