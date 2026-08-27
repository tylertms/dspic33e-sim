#include "device/dspic33ep_mu/internal.h"

void dspic33_adc_input(Dspic33* cpu, uint8_t channel, uint16_t value) {
    const Dspic33epMuProfile* profile = dspic33_device_profile(cpu);
    if (profile != NULL && channel < DSPIC33_ADC_CHANNEL_COUNT &&
        (profile->adc_channel_mask & (UINT32_C(1) << channel)) != 0u) {
        cpu->io.adc[channel] = (uint16_t)(value & 0x0fffu);
    }
}

bool dspic33_gpio_drive(Dspic33* cpu, uint8_t port, uint16_t value, uint16_t mask) {
    uint16_t selected;
    if (port >= DSPIC33_GPIO_PORT_COUNT) {
        return false;
    }
    selected = (uint16_t)(mask & dspic33_device_internal_gpio_port_mask(cpu, port));
    if (selected == 0u) {
        return false;
    }
    cpu->io.gpio[port] = (uint16_t)((cpu->io.gpio[port] & ~selected) | (value & selected));
    cpu->io.gpio_driven[port] |= selected;
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
    return true;
}

bool dspic33_gpio_release(Dspic33* cpu, uint8_t port, uint16_t mask) {
    if (port >= DSPIC33_GPIO_PORT_COUNT) {
        return false;
    }
    mask &= dspic33_device_internal_gpio_port_mask(cpu, port);
    if (mask == 0u) {
        return false;
    }
    cpu->io.gpio_driven[port] &= (uint16_t)~mask;
    dspic33_device_internal_refresh_physical_pin_inputs(cpu);
    return true;
}

bool dspic33_gpio_pin(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* high) {
    uint16_t mask;
    if (port >= DSPIC33_GPIO_PORT_COUNT || bit >= 16u || high == NULL) {
        return false;
    }
    mask = (uint16_t)(1u << bit);
    if ((dspic33_device_internal_gpio_port_mask(cpu, port) & mask) == 0u ||
        (port == 2u && bit == 15u && dspic33_device_internal_oscillator_pin_owned(cpu))) {
        return false;
    }
    *high = (dspic33_device_internal_gpio_pin_values(cpu, port) & mask) != 0u;
    return true;
}

bool dspic33_gpio_output(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* enabled, bool* high) {
    if (cpu == NULL || port >= DSPIC33_GPIO_PORT_COUNT || bit >= 16u || enabled == NULL ||
        high == NULL) {
        return false;
    }
    const uint16_t mask = (uint16_t)(1u << bit);
    if ((dspic33_device_internal_gpio_port_mask(cpu, port) & mask) == 0u ||
        (port == 2u && bit == 15u && dspic33_device_internal_oscillator_pin_owned(cpu))) {
        return false;
    }
    const uint16_t input_mask =
        (uint16_t)(dspic33_device_internal_raw_word(cpu, dspic33_device_gpio_tris_addresses[port]) |
                   dspic33_device_gpio_input_only_masks[port]);
    *enabled = (input_mask & mask) == 0u;
    *high = (dspic33_device_internal_gpio_pin_values(cpu, port) & mask) != 0u;
    return true;
}

bool dspic33_gpio_signal(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* high) {
    if (cpu == NULL || port >= DSPIC33_GPIO_PORT_COUNT || bit >= 16u || high == NULL) {
        return false;
    }
    const uint16_t mask = (uint16_t)(1u << bit);
    if ((cpu->io.gpio_driven[port] & mask) != 0u) {
        *high = (cpu->io.gpio[port] & mask) != 0u;
        return true;
    }
    return dspic33_gpio_pin(cpu, port, bit, high);
}

bool dspic33_oscillator_pin(const Dspic33* cpu, bool* clock_output, uint64_t* edges) {
    if (clock_output == NULL || edges == NULL ||
        !dspic33_device_internal_oscillator_pin_owned(cpu)) {
        return false;
    }
    *clock_output = dspic33_device_internal_oscillator_pin_clock_output(cpu);
    *edges = !*clock_output                          ? 0u
             : cpu->device_cycles <= UINT64_MAX / 2u ? cpu->device_cycles * 2u
                                                     : UINT64_MAX;
    return true;
}

bool dspic33_reference_clock_pin(const Dspic33* cpu, uint8_t pin, uint64_t primary_edges,
                                 uint64_t* edges) {
    if (edges == NULL) {
        return false;
    }
    uint8_t function = dspic33_device_internal_pps_output_function(cpu, pin);
    uint16_t control = dspic33_device_internal_raw_word(cpu, REFERENCE_CLOCK_CONTROL);
    if (function != 49u || (control & REFERENCE_CLOCK_ENABLE) == 0u ||
        (cpu->power_state == DSPIC33_POWER_SLEEP && (control & 0x2000u) == 0u)) {
        return false;
    }
    uint64_t source_edges =
        (control & 0x1000u) != 0u
            ? primary_edges
            : (cpu->device_cycles <= UINT64_MAX / 2u ? cpu->device_cycles * 2u : UINT64_MAX);
    *edges = source_edges >> ((control & REFERENCE_CLOCK_DIVISOR) >> 8u);
    return true;
}

bool dspic33_device_gpio_input_high(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* high) {
    uint16_t driven;
    uint16_t mask;
    uint16_t pull_down;
    uint16_t pull_up;
    if (port >= DSPIC33_GPIO_PORT_COUNT || bit >= 16u || high == NULL) {
        return false;
    }
    mask = (uint16_t)(1u << bit);
    if ((dspic33_device_internal_gpio_port_mask(cpu, port) & mask) == 0u) {
        return false;
    }
    driven = cpu->io.gpio_driven[port];
    pull_up = dspic33_device_internal_raw_word(cpu, dspic33_device_gpio_pull_up_addresses[port]);
    pull_down =
        dspic33_device_internal_raw_word(cpu, dspic33_device_gpio_pull_down_addresses[port]);
    *high = (((cpu->io.gpio[port] & driven) | (pull_up & ~driven & ~pull_down)) & mask) != 0u;
    return true;
}

void dspic33_gpio_input(Dspic33* cpu, uint8_t port, uint16_t value) {
    if (port < DSPIC33_GPIO_PORT_COUNT) {
        dspic33_gpio_drive(cpu, port, value, dspic33_device_internal_gpio_port_mask(cpu, port));
    }
}

void dspic33_device_reset(Dspic33* cpu) {
    uint16_t gpio[DSPIC33_GPIO_PORT_COUNT];
    uint16_t gpio_driven[DSPIC33_GPIO_PORT_COUNT];
    size_t index;
    memcpy(gpio, cpu->io.gpio, sizeof(gpio));
    memcpy(gpio_driven, cpu->io.gpio_driven, sizeof(gpio_driven));
    for (size_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        cpu->qei_inputs[channel] &= (uint8_t)~cpu->io.qei.pps_qualified[channel];
    }
    memset(&cpu->io, 0, sizeof(cpu->io));
    cpu->io.cpu_bus_cycle = UINT64_MAX;
    cpu->io.cpu_write_cycle = UINT64_MAX;
    cpu->io.dci.bcg_paused = true;
    cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_AVDD] = 3300u;
    cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE] = 3300u;
    memcpy(cpu->io.gpio, gpio, sizeof(gpio));
    memcpy(cpu->io.gpio_driven, gpio_driven, sizeof(gpio_driven));
    memcpy(cpu->io.qei.filtered_inputs, cpu->qei_inputs, sizeof(cpu->qei_inputs));
    memcpy(cpu->io.qei.logical_inputs, cpu->qei_inputs, sizeof(cpu->qei_inputs));
    cpu->io.uart_cts = (uint8_t)((1u << DSPIC33_UART_COUNT) - 1u);
    cpu->io.can_rx_pin_high = (uint8_t)((1u << DSPIC33_CAN_COUNT) - 1u);
    cpu->io.input_capture.sync_output_high = 0x00ffu;
    cpu->io.output_compare.fault_inputs = 0u;
    memset(cpu->interrupt_deferred, 0, sizeof(cpu->interrupt_deferred));
    memset(cpu->interrupt_deferred_next, 0, sizeof(cpu->interrupt_deferred_next));
    cpu->gie_disable_deferred = 0u;
    cpu->gie_disable_deferred_next = 0u;
    for (index = 0u;
         index < sizeof(dspic33_device_reset_values) / sizeof(dspic33_device_reset_values[0]);
         index++) {
        dspic33_device_internal_raw_write_word(cpu, dspic33_device_reset_values[index].address,
                                               dspic33_device_reset_values[index].value);
    }
    if (dspic33_device_internal_pwm_generator_count(cpu) == DSPIC33_PWM_MAX_COUNT) {
        dspic33_device_internal_raw_write_word(cpu, 0x0872u, 0x0004u);
    }
    dspic33_i2c_reset(cpu);
    dspic33_device_internal_usb_reset_registers(cpu);
    dspic33_device_internal_raw_write_word(cpu, USB_PWRC, 0u);
    dspic33_device_internal_reset_main_oscillator(cpu);
    dspic33_device_internal_raw_write_word(cpu, 0x08c2u, 0x8000u);
    dspic33_device_internal_raw_write_word(cpu, 0x08c8u, 0u);
    dspic33_device_internal_raw_write_word(cpu, DMA_LCA, 0x000fu);
    dspic33_device_reset_restored(cpu);
}
