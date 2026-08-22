#include "device/dspic33ep_mu/internal.h"

void dspic33_device_write_byte(Dspic33* cpu, uint16_t address, uint16_t previous) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint16_t requested = dspic33_device_internal_raw_word(cpu, base);
    uint16_t writable;
    uint8_t channel;
    if (dspic33_device_internal_protect_oscillator_write(cpu, address, previous)) {
        return;
    }
    if (base >= 0x0680u && base <= 0x06f6u &&
        !dspic33_device_internal_pps_register_write_mask(cpu, base, &writable)) {
        dspic33_device_internal_raw_write_word(cpu, base, previous);
        return;
    }
    if (base >= 0x0680u && base <= 0x06f6u &&
        (dspic33_device_internal_raw_word(cpu, 0x0742u) & 0x0040u) != 0u) {
        dspic33_device_internal_raw_write_word(cpu, base, previous);
        return;
    }
    if (dspic33_i2c_write_register(cpu, address, previous, requested)) {
        return;
    }
    if (cpu->io.usb_pmd_disabled && dspic33_device_internal_usb_register_address(base)) {
        dspic33_device_internal_raw_write_word(cpu, base, previous);
        return;
    }
    for (channel = 0u; channel < DSPIC33_ADC_COUNT; channel++) {
        if (dspic33_device_internal_adc_pmd_disabled(cpu, channel) &&
            dspic33_device_internal_adc_module_address(base, channel)) {
            dspic33_device_internal_raw_write_word(cpu, base, previous);
            return;
        }
    }
    if (dspic33_device_internal_pwm_address_inaccessible(cpu, base)) {
        dspic33_device_internal_raw_write_word(cpu, base, previous);
        return;
    }
    if (dspic33_device_internal_register_write_mask(cpu, base, &writable) ||
        dspic33_device_internal_input_capture_register_write_mask(base, &writable) ||
        dspic33_device_internal_output_compare_register_write_mask(base, &writable) ||
        dspic33_device_internal_comparator_register_write_mask(base, &writable) ||
        dspic33_device_internal_adc_register_write_mask(base, &writable) ||
        dspic33_device_internal_pwm_register_write_mask(cpu, base, &writable) ||
        dspic33_device_internal_uart_register_write_mask(cpu, base, &writable) ||
        dspic33_device_internal_spi_register_write_mask(base, &writable) ||
        dspic33_device_internal_can_register_write_mask(cpu, base, &writable) ||
        dspic33_device_internal_usb_register_write_mask(cpu, base, previous, &writable) ||
        dspic33_device_internal_dma_register_write_mask(base, &writable)) {
        dspic33_device_internal_raw_write_word(
            cpu, base, (uint16_t)((previous & ~writable) | (requested & writable)));
    }
    dspic33_i2c_update_pmd(cpu, base, previous);
    if (base == PMP_PMD_ADDRESS) {
        dspic33_device_internal_update_pmp_pmd(cpu, previous);
    }
    dspic33_device_internal_update_input_capture_pmd(cpu, base, previous);
    dspic33_device_internal_update_output_compare_pmd(cpu, base, previous);
    dspic33_device_internal_update_timer_pmd(cpu, base, previous);
    dspic33_device_internal_update_adc_pmd(cpu, base, previous);
    dspic33_device_internal_update_pwm_pmd(cpu, base, previous);
    dspic33_device_internal_update_usb_pmd(cpu, base, previous);
    if (base == 0x0740u && (cpu->configuration[10u] & 0x80u) == 0u && (previous & 0x0020u) == 0u &&
        (dspic33_device_internal_raw_word(cpu, base) & 0x0020u) != 0u) {
        cpu->watchdog.ticks = 0u;
    }
    dspic33_device_internal_pps_update_shadow(cpu, base);
    dspic33_device_internal_refresh_can_pps_inputs(cpu);
    dspic33_device_internal_update_gpio_latch(cpu, address, requested);
    if (base >= 0x0800u && base < 0x0800u + DSPIC33_IRQ_GROUP_COUNT * 2u) {
        uint16_t group = (uint16_t)((base - 0x0800u) / 2u);
        uint16_t current = dspic33_device_internal_raw_word(cpu, base);
        uint16_t cleared = (uint16_t)(previous & ~current);
        cpu->interrupt_deferred[group] &= (uint16_t)~cleared;
        cpu->interrupt_deferred_next[group] =
            (uint16_t)((cpu->interrupt_deferred_next[group] & ~cleared) | (current & ~previous));
    }
    dspic33_device_internal_interrupt_control_write(cpu, base, previous, requested);
    if (base == AUXILIARY_CLOCK_CONTROL) {
        if (dspic33_device_internal_auxiliary_clock_configuration_locked(cpu)) {
            dspic33_device_internal_raw_write_word(cpu, base, previous);
        } else {
            uint16_t control = (uint16_t)((previous & ~AUXILIARY_CLOCK_WRITABLE) |
                                          (requested & AUXILIARY_CLOCK_WRITABLE));
            dspic33_device_internal_raw_write_word(cpu, base, control);
            if (dspic33_device_internal_auxiliary_pll_reconfiguration(previous, control)) {
                dspic33_device_internal_reconfigure_auxiliary_pll(cpu);
            }
        }
    }
    if (base == AUXILIARY_CLOCK_DIVISOR) {
        if (dspic33_device_internal_auxiliary_clock_configuration_locked(cpu)) {
            dspic33_device_internal_raw_write_word(cpu, base, previous);
        } else {
            uint16_t divisor = requested & AUXILIARY_CLOCK_DIVISOR_WRITABLE;
            dspic33_device_internal_raw_write_word(cpu, base, divisor);
            if (((previous ^ divisor) & AUXILIARY_CLOCK_DIVISOR_WRITABLE) != 0u) {
                dspic33_device_internal_reconfigure_auxiliary_pll(cpu);
            }
        }
    }
    dspic33_device_internal_update_main_clock_configuration(cpu, base, previous);
    if (base == 0x0748u &&
        dspic33_device_internal_auxiliary_pll_input(
            dspic33_device_internal_raw_word(cpu, AUXILIARY_CLOCK_CONTROL)) == 1u &&
        ((previous ^ dspic33_device_internal_raw_word(cpu, base)) & 0x003fu) != 0u) {
        dspic33_device_internal_reconfigure_auxiliary_pll(cpu);
    }
    if (base == REFERENCE_CLOCK_CONTROL && (previous & REFERENCE_CLOCK_ENABLE) != 0u) {
        uint16_t control = dspic33_device_internal_raw_word(cpu, base);
        dspic33_device_internal_raw_write_word(cpu, base,
                                               (uint16_t)((control & ~REFERENCE_CLOCK_DIVISOR) |
                                                          (previous & REFERENCE_CLOCK_DIVISOR)));
    }
    dspic33_device_internal_update_timer_register(cpu, base, previous);
    dspic33_device_internal_update_adc_register(cpu, base, previous, requested);
    dspic33_device_internal_update_pwm_register(cpu, base, previous);
    dspic33_device_internal_update_spi_register(cpu, base, previous, requested);
    dspic33_device_internal_update_can_register(cpu, base, previous, requested);
    dspic33_device_internal_update_usb_register(cpu, base, previous, requested);
    dspic33_device_internal_update_crc_register(cpu, address, previous, requested);
    dspic33_device_internal_update_pmp_register(cpu, address, previous);
    dspic33_device_internal_update_input_capture_register(cpu, base, previous);
    dspic33_device_internal_update_output_compare_register(cpu, base, previous);
    dspic33_device_internal_update_comparator_register(cpu, base, previous, requested);
    dspic33_device_internal_update_rtcc_register(cpu, address, previous);
    dspic33_device_internal_update_qei_register(cpu, base, previous, requested);
    dspic33_device_internal_update_dci_register(cpu, base, previous);
    dspic33_device_internal_update_uart_register(cpu, base, previous, requested);
    if (base == NVM_KEY && (cpu->io.cpu_write_width == 2u || address == NVM_KEY)) {
        dspic33_device_internal_update_nvm_key(cpu, requested);
    } else if (base == NVM_CONTROL) {
        dspic33_device_internal_update_nvm_control(cpu, requested);
    }
    if (base >= DMA_CHANNEL_BASE &&
        base < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        channel = (uint8_t)((base - DMA_CHANNEL_BASE) / DMA_CHANNEL_STRIDE);
        if ((base & 0x000fu) == 0u) {
            dspic33_device_internal_update_dma_control(cpu, channel, previous);
        } else if ((base & 0x000fu) == 2u) {
            dspic33_device_internal_update_dma_request(cpu, channel, previous);
        }
    }
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
}
