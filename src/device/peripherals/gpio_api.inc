
void dspic33_adc_input(Dspic33* cpu, uint8_t channel, uint16_t value) {
    if (channel < DSPIC33_ADC_CHANNEL_COUNT) {
        cpu->io.adc[channel] = (uint16_t)(value & 0x0fffu);
    }
}

bool dspic33_gpio_drive(Dspic33* cpu, uint8_t port, uint16_t value, uint16_t mask) {
    uint16_t selected;
    if (port >= DSPIC33_GPIO_PORT_COUNT) {
        return false;
    }
    selected = (uint16_t)(mask & gpio_port_masks[port]);
    cpu->io.gpio[port] = (uint16_t)((cpu->io.gpio[port] & ~selected) | (value & selected));
    cpu->io.gpio_driven[port] |= selected;
    refresh_physical_pin_inputs(cpu);
    return true;
}

bool dspic33_gpio_release(Dspic33* cpu, uint8_t port, uint16_t mask) {
    if (port >= DSPIC33_GPIO_PORT_COUNT) {
        return false;
    }
    cpu->io.gpio_driven[port] &= (uint16_t)~(mask & gpio_port_masks[port]);
    refresh_physical_pin_inputs(cpu);
    return true;
}

bool dspic33_gpio_pin(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* high) {
    uint16_t mask;
    if (port >= DSPIC33_GPIO_PORT_COUNT || bit >= 16u || high == NULL) {
        return false;
    }
    mask = (uint16_t)(1u << bit);
    if ((gpio_port_masks[port] & mask) == 0u ||
        (port == 2u && bit == 15u && oscillator_pin_owned(cpu))) {
        return false;
    }
    *high = (gpio_pin_values(cpu, port) & mask) != 0u;
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
    if (clock_output == NULL || edges == NULL || !oscillator_pin_owned(cpu)) {
        return false;
    }
    *clock_output = oscillator_pin_clock_output(cpu);
    *edges = !*clock_output                          ? 0u
             : cpu->device_cycles <= UINT64_MAX / 2u ? cpu->device_cycles * 2u
                                                     : UINT64_MAX;
    return true;
}

bool dspic33_reference_clock_pin(const Dspic33* cpu, uint8_t pin, uint64_t primary_edges,
                                 uint64_t* edges) {
    uint8_t function = 0u;
    size_t mapping;
    if (edges == NULL) {
        return false;
    }
    for (mapping = 0u; mapping < sizeof(pps_outputs) / sizeof(pps_outputs[0]); mapping++) {
        if (pps_outputs[mapping].pin == pin) {
            function = (uint8_t)((raw_word(cpu, pps_outputs[mapping].address) >>
                                  pps_outputs[mapping].shift) &
                                 0x003fu);
            break;
        }
    }
    uint16_t control = raw_word(cpu, REFERENCE_CLOCK_CONTROL);
    if (mapping == sizeof(pps_outputs) / sizeof(pps_outputs[0]) || function != 49u ||
        (control & REFERENCE_CLOCK_ENABLE) == 0u ||
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
    if ((gpio_port_masks[port] & mask) == 0u) {
        return false;
    }
    driven = cpu->io.gpio_driven[port];
    pull_up = raw_word(cpu, gpio_pull_up_addresses[port]);
    pull_down = raw_word(cpu, gpio_pull_down_addresses[port]);
    *high = (((cpu->io.gpio[port] & driven) | (pull_up & ~driven & ~pull_down)) & mask) != 0u;
    return true;
}

void dspic33_gpio_input(Dspic33* cpu, uint8_t port, uint16_t value) {
    if (port < DSPIC33_GPIO_PORT_COUNT) {
        dspic33_gpio_drive(cpu, port, value, gpio_port_masks[port]);
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
    for (index = 0u; index < sizeof(reset_values) / sizeof(reset_values[0]); index++) {
        raw_write_word(cpu, reset_values[index].address, reset_values[index].value);
    }
    dspic33_i2c_reset(cpu);
    usb_reset_registers(cpu);
    raw_write_word(cpu, USB_PWRC, 0u);
    reset_main_oscillator(cpu);
    raw_write_word(cpu, 0x08c2u, 0x8000u);
    raw_write_word(cpu, 0x08c8u, 0u);
    raw_write_word(cpu, DMA_LCA, 0x000fu);
    dspic33_device_reset_restored(cpu);
}
