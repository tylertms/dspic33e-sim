
static bool oscillator_pin_owned(const Dspic33* cpu) {
    uint8_t primary_mode = (uint8_t)(cpu->configuration[8u] & 0x03u);
    return primary_mode == 1u || primary_mode == 2u || (cpu->configuration[8u] & 0x04u) != 0u;
}

static bool oscillator_pin_clock_output(const Dspic33* cpu) {
    uint8_t primary_mode = (uint8_t)(cpu->configuration[8u] & 0x03u);
    return primary_mode != 1u && primary_mode != 2u && (cpu->configuration[8u] & 0x04u) != 0u;
}

static uint16_t gpio_pin_values(const Dspic33* cpu, uint8_t port) {
    uint16_t tris = raw_word(cpu, gpio_tris_addresses[port]);
    uint16_t lat = raw_word(cpu, gpio_latch_addresses[port]);
    uint16_t open_drain = raw_word(cpu, gpio_open_drain_addresses[port]);
    uint16_t driven = cpu->io.gpio_driven[port];
    uint16_t pull_up = raw_word(cpu, gpio_pull_up_addresses[port]);
    uint16_t pull_down = raw_word(cpu, gpio_pull_down_addresses[port]);
    uint16_t inputs = (uint16_t)(tris | gpio_input_only_masks[port]);
    uint16_t analog =
        gpio_analog_addresses[port] != 0u ? raw_word(cpu, gpio_analog_addresses[port]) : 0u;
    if ((cpu->io.adc_pmd_disabled & 1u) != 0u) {
        analog = 0u;
    } else if (port == 1u && (cpu->io.adc_pmd_disabled & 2u) != 0u) {
        analog = 0u;
    }
    uint16_t external =
        (uint16_t)((cpu->io.gpio[port] & driven) | (pull_up & ~driven & ~pull_down));
    uint16_t values;
    uint8_t pin;
    if (port == 2u && (cpu->configuration[8u] & 0x04u) == 0u) {
        uint8_t source = oscillator_current_source(raw_word(cpu, OSCILLATOR_CONTROL));
        bool frc = source == 0u || source == 1u || source == 6u || source == 7u;
        bool external_clock =
            (source == 2u || source == 3u) && (cpu->configuration[8u] & 0x03u) == 0u;
        if ((frc || external_clock) &&
            (raw_word(cpu, gpio_open_drain_addresses[0]) & 0x0008u) != 0u) {
            open_drain |= 0x8000u;
        }
    }
    if (port == 6u && (raw_word(cpu, USB_CON) & USB_ENABLE) != 0u) {
        analog |= 0x000cu;
    }
    values = (uint16_t)(((external & inputs & ~analog) | (lat & ~inputs & ~open_drain) |
                         (external & ~inputs & open_drain & lat)) &
                        gpio_port_masks[port]);
    for (pin = 0u; pin < 16u; pin++) {
        bool high;
        uint16_t bit = (uint16_t)(1u << pin);
        if (pwm_pin_value(cpu, port, pin, &high)) {
            values = high ? (uint16_t)(values | bit) : (uint16_t)(values & ~bit);
        } else if (uart_pps_output_value(cpu, port, pin, &high)) {
            if (high && (open_drain & bit) != 0u) {
                values =
                    (external & bit) != 0u ? (uint16_t)(values | bit) : (uint16_t)(values & ~bit);
            } else {
                values = high ? (uint16_t)(values | bit) : (uint16_t)(values & ~bit);
            }
        } else if (qei_pps_output_value(cpu, port, pin, &high)) {
            if (high && (open_drain & bit) != 0u) {
                values =
                    (external & bit) != 0u ? (uint16_t)(values | bit) : (uint16_t)(values & ~bit);
            } else {
                values = high ? (uint16_t)(values | bit) : (uint16_t)(values & ~bit);
            }
        }
    }
    if (port == 2u && oscillator_pin_owned(cpu)) {
        values &= 0x7fffu;
    }
    return values;
}

static uint16_t gpio_change_notification_qualified(const Dspic33* cpu, uint8_t port) {
    uint16_t inputs =
        (uint16_t)(raw_word(cpu, gpio_tris_addresses[port]) | gpio_input_only_masks[port]);
    uint16_t analog =
        gpio_analog_addresses[port] != 0u ? raw_word(cpu, gpio_analog_addresses[port]) : 0u;
    if ((cpu->io.adc_pmd_disabled & 1u) != 0u) {
        analog = 0u;
    } else if (port == 1u && (cpu->io.adc_pmd_disabled & 2u) != 0u) {
        analog = 0u;
    }
    if (port == 6u && (raw_word(cpu, USB_CON) & USB_ENABLE) != 0u) {
        analog |= 0x000cu;
    }
    return (uint16_t)(raw_word(cpu, gpio_change_notification_addresses[port]) & inputs & ~analog &
                      gpio_port_masks[port]);
}

static uint16_t gpio_change_notification_mismatch(const Dspic33* cpu, uint8_t port) {
    return (uint16_t)((cpu->io.gpio_cn_values[port] ^ cpu->io.gpio_cn_reference[port]) &
                      cpu->io.gpio_cn_qualified[port]);
}

static void refresh_gpio_change_notification(Dspic33* cpu) {
    bool changed = false;
    uint8_t port;
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t previous_mismatch = gpio_change_notification_mismatch(cpu, port);
        uint16_t values = gpio_pin_values(cpu, port);
        uint16_t qualified = gpio_change_notification_qualified(cpu, port);
        uint16_t newly_qualified = (uint16_t)(qualified & ~cpu->io.gpio_cn_qualified[port]);
        cpu->io.gpio_cn_reference[port] =
            (uint16_t)((cpu->io.gpio_cn_reference[port] & qualified & ~newly_qualified) |
                       (values & (~qualified | newly_qualified)));
        cpu->io.gpio_cn_values[port] = values;
        cpu->io.gpio_cn_qualified[port] = qualified;
        changed =
            changed || (gpio_change_notification_mismatch(cpu, port) & ~previous_mismatch) != 0u;
    }
    if (changed) {
        dspic33_raise_interrupt(cpu, 19u);
    }
}

static uint8_t external_interrupt_selection(const Dspic33* cpu, uint8_t channel) {
    static const uint16_t addresses[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {0u, 0x06a0u, 0x06a2u,
                                                                         0x06a2u, 0x06a4u};
    static const uint8_t shifts[DSPIC33_EXTERNAL_INTERRUPT_COUNT] = {0u, 8u, 0u, 8u, 0u};
    if (channel == 0u) {
        return 64u;
    }
    return (uint8_t)((raw_word(cpu, addresses[channel]) >> shifts[channel]) & 0x007fu);
}

static bool external_interrupt_level(const Dspic33* cpu, uint8_t channel, uint8_t selection,
                                     bool* high) {
    if (channel != 0u && selection == 0u) {
        *high = false;
        return true;
    }
    if (channel != 0u && selection == 1u) {
        *high = (cpu->io.comparator.output_high & 1u) != 0u;
        return true;
    }
    return pps_physical_input_high(cpu, selection, high);
}

static void refresh_external_interrupts(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        uint8_t mask = (uint8_t)(1u << channel);
        uint8_t selection = external_interrupt_selection(cpu, channel);
        bool previous_high = (cpu->io.external_interrupt_levels & mask) != 0u;
        bool previous_qualified = (cpu->io.external_interrupt_qualified & mask) != 0u;
        bool high = false;
        bool qualified = external_interrupt_level(cpu, channel, selection, &high);
        bool reconfigured = selection != cpu->io.external_interrupt_selection[channel] ||
                            qualified != previous_qualified;
        cpu->io.external_interrupt_selection[channel] = selection;
        if (high) {
            cpu->io.external_interrupt_levels |= mask;
        } else {
            cpu->io.external_interrupt_levels &= (uint8_t)~mask;
        }
        if (qualified) {
            cpu->io.external_interrupt_qualified |= mask;
        } else {
            cpu->io.external_interrupt_qualified &= (uint8_t)~mask;
        }
        if (!reconfigured && qualified && high != previous_high) {
            bool falling = (raw_word(cpu, 0x08c2u) & mask) != 0u;
            if (falling == previous_high) {
                raise_external_interrupt(cpu, channel);
            }
        }
    }
}

static uint8_t timer_input_selection(const Dspic33* cpu, uint8_t timer) {
    if (timer == 0u) {
        return 62u;
    }
    uint16_t mapping = raw_word(cpu, (uint16_t)(0x06a6u + ((timer - 1u) / 2u) * 2u));
    return (timer & 1u) != 0u ? (uint8_t)(mapping & 0x007fu) : (uint8_t)((mapping >> 8u) & 0x007fu);
}

static bool timer_input_level(const Dspic33* cpu, uint8_t selection, bool* high) {
    if (selection == 0u) {
        *high = false;
        return true;
    }
    return pps_physical_input_high(cpu, selection, high);
}

static void refresh_timer_inputs(Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        uint16_t bit = (uint16_t)(1u << timer);
        bool previous_high = (cpu->io.timer_pin_levels & bit) != 0u;
        bool previous_qualified = (cpu->io.timer_pin_qualified & bit) != 0u;
        bool high = false;
        bool qualified = timer_input_level(cpu, timer_input_selection(cpu, timer), &high);
        if (high) {
            cpu->io.timer_pin_levels |= bit;
        } else {
            cpu->io.timer_pin_levels &= (uint16_t)~bit;
        }
        if (qualified) {
            cpu->io.timer_pin_qualified |= bit;
        } else {
            cpu->io.timer_pin_qualified &= (uint16_t)~bit;
        }
        if (previous_qualified && qualified && !previous_high && high) {
            pulse_timer(cpu, timer, 1u);
        }
        if (previous_qualified && qualified && previous_high && !high) {
            set_timer_gate(cpu, timer, false);
        }
        if (qualified && (!previous_qualified || high != previous_high)) {
            if (high) {
                set_timer_gate(cpu, timer, true);
            }
        }
    }
}

static void refresh_input_capture_pps_inputs(Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t bit = (uint16_t)(1u << channel);
        uint8_t selection = input_capture_pps_pin(cpu, channel);
        bool previous_qualified = (cpu->io.input_capture.pps_qualified & bit) != 0u;
        bool high = false;
        bool qualified = selection != 0u && (channel != 1u || !can_capture_enabled(cpu)) &&
                         pps_physical_input_high(cpu, selection, &high);
        bool reconfigured = selection != cpu->io.input_capture.pps_selection[channel] ||
                            qualified != previous_qualified;
        cpu->io.input_capture.pps_selection[channel] = selection;
        if (qualified) {
            cpu->io.input_capture.pps_qualified |= bit;
        } else {
            cpu->io.input_capture.pps_qualified &= (uint16_t)~bit;
        }
        if (reconfigured) {
            if (qualified && high) {
                cpu->io.input_capture.input_high |= bit;
            } else if (qualified) {
                cpu->io.input_capture.input_high &= (uint16_t)~bit;
            }
        } else if (qualified) {
            input_capture_level(cpu, channel, high);
        }
    }
}

static void refresh_qei_pps_inputs(Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint8_t values = cpu->qei_inputs[channel];
        uint8_t affected = 0u;
        for (uint8_t input = 0u; input < 4u; input++) {
            uint8_t mask = (uint8_t)(1u << input);
            uint16_t mapping = raw_word(cpu, qei_pps_registers[channel][input / 2u]);
            uint8_t selection = (uint8_t)((mapping >> ((input & 1u) * 8u)) & 0x007fu);
            bool previous_qualified = (cpu->io.qei.pps_qualified[channel] & mask) != 0u;
            bool high = false;
            bool qualified = selection != 0u && pps_physical_input_high(cpu, selection, &high);
            bool reconfigured = selection != cpu->io.qei.pps_selection[channel][input] ||
                                qualified != previous_qualified;
            cpu->io.qei.pps_selection[channel][input] = selection;
            if (qualified) {
                cpu->io.qei.pps_qualified[channel] |= mask;
            } else {
                cpu->io.qei.pps_qualified[channel] &= (uint8_t)~mask;
            }
            if (reconfigured) {
                if (qualified || previous_qualified) {
                    affected |= mask;
                    values =
                        qualified && high ? (uint8_t)(values | mask) : (uint8_t)(values & ~mask);
                }
            } else if (qualified) {
                affected |= mask;
                values = high ? (uint8_t)(values | mask) : (uint8_t)(values & ~mask);
            }
        }
        if (affected != 0u && values != cpu->qei_inputs[channel]) {
            qei_set_physical_inputs(cpu, channel, values);
        }
    }
}

static void uart_refresh_pps_inputs(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t mapping = raw_word(cpu, uart_pps_registers[channel]);
        uint8_t selection = (uint8_t)(mapping & 0x007fu);
        uint8_t cts_selection = (uint8_t)((mapping >> 8u) & 0x007fu);
        uint8_t mask = (uint8_t)(1u << channel);
        bool previous_high = (cpu->io.uart_rx_pin_high & mask) != 0u;
        bool previous_qualified = (cpu->io.uart_rx_qualified & mask) != 0u;
        bool high = false;
        bool qualified = uart_rx_logical_level(cpu, channel, &high);
        bool reconfigured =
            selection != cpu->io.uart_rx_selection[channel] || qualified != previous_qualified;
        cpu->io.uart_rx_selection[channel] = selection;
        if (qualified) {
            cpu->io.uart_rx_qualified |= mask;
        } else {
            cpu->io.uart_rx_qualified &= (uint8_t)~mask;
        }
        if (high) {
            cpu->io.uart_rx_pin_high |= mask;
        } else {
            cpu->io.uart_rx_pin_high &= (uint8_t)~mask;
        }
        if (!reconfigured && qualified && high != previous_high) {
            uint16_t mode = raw_word(cpu, uart_bases[channel]);
            if ((mode & UART_MODE_AUTO_BAUD) != 0u) {
                uart_auto_baud_edge(cpu, channel, previous_high, high);
            } else if ((mode & UART_MODE_WAKE) != 0u && (mode & UART_MODE_LOOPBACK) == 0u) {
                if (previous_high && !high) {
                    dspic33_raise_interrupt(cpu, uart_rx_irqs[channel]);
                } else if (!previous_high && high) {
                    raw_write_word(cpu, uart_bases[channel], (uint16_t)(mode & ~UART_MODE_WAKE));
                }
            } else if (previous_high && !high && uart_receiver_operating(cpu, channel) &&
                       (cpu->io.uart_rx_active & mask) == 0u) {
                uart_begin_physical_receive(cpu, channel);
            }
        }
        if ((raw_word(cpu, uart_bases[channel]) & UART_MODE_UEN_MASK) == 0x0200u &&
            (cpu->io.uart_cts_direct & mask) == 0u) {
            bool cts_high = false;
            bool cts_qualified =
                cts_selection == 0u || pps_physical_input_high(cpu, cts_selection, &cts_high);
            if (cts_qualified && !cts_high) {
                cpu->io.uart_cts |= mask;
                uart_schedule_transmit(cpu, channel);
            } else {
                cpu->io.uart_cts &= (uint8_t)~mask;
            }
        }
    }
}

static void uart_update_power_state(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t mode = raw_word(cpu, uart_bases[channel]);
        uint8_t mask = (uint8_t)(1u << channel);
        bool stopped =
            cpu->power_state == DSPIC33_POWER_SLEEP ||
            (cpu->power_state == DSPIC33_POWER_IDLE && (mode & UART_MODE_STOP_IDLE) != 0u);
        if (stopped && (cpu->io.uart_rx_active & mask) != 0u) {
            uart_cancel_physical_receive(cpu, channel);
        }
        if (stopped) {
            uart_reset_auto_baud(cpu, channel);
        }
        if (stopped && (cpu->io.uart_tx_active & mask) != 0u) {
            cpu->io.uart_generation[channel]++;
            cpu->io.uart_tx_active &= (uint8_t)~mask;
            cpu->io.uart_tx_scheduled &= (uint8_t)~mask;
            memset(&cpu->io.uart_tx_shift[channel], 0, sizeof(cpu->io.uart_tx_shift[channel]));
            uart_refresh_status(cpu, channel);
        }
    }
    refresh_physical_pin_inputs(cpu);
}

static void refresh_physical_pin_inputs(Dspic33* cpu) {
    uart_refresh_pps_inputs(cpu);
    refresh_qei_pps_inputs(cpu);
    refresh_gpio_change_notification(cpu);
    refresh_external_interrupts(cpu);
    refresh_timer_inputs(cpu);
    refresh_input_capture_pps_inputs(cpu);
    output_compare_refresh_fault_pps_inputs(cpu);
    refresh_pwm_inputs(cpu);
    dci_refresh_pps_inputs(cpu);
    spi_refresh_pps_inputs(cpu);
    refresh_can_pps_inputs(cpu);
    dspic33_i2c_refresh_pins(cpu);
}

static void apply_physical_pin_level(Dspic33* cpu, uint8_t pin, bool high) {
    const Dspic33PpsPin* mapping = pps_pin(pin);
    if (mapping == NULL) {
        return;
    }
    uint16_t bit = (uint16_t)(1u << mapping->bit);
    cpu->io.gpio[mapping->port] =
        (uint16_t)((cpu->io.gpio[mapping->port] & ~bit) | (high ? bit : 0u));
    cpu->io.gpio_driven[mapping->port] |= bit;
    refresh_physical_pin_inputs(cpu);
}

static void acknowledge_gpio_change_notification(Dspic33* cpu, uint8_t port, uint16_t values) {
    cpu->io.gpio_cn_reference[port] = values;
    cpu->io.gpio_cn_values[port] = values;
    cpu->io.gpio_cn_qualified[port] = gpio_change_notification_qualified(cpu, port);
}

uint8_t dspic33_device_read_byte(Dspic33* cpu, uint16_t address, uint8_t value) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint8_t port;
    uint8_t channel;
    uint8_t timer;
    if (address == 0x0741u) {
        return (uint8_t)(value & ~0x08u);
    }
    for (timer = 0u; timer < 5u; timer++) {
        if (timer_pmd_disabled(cpu, timer) &&
            (base == timer_controls[timer] || base == timer_registers[timer] ||
             base == timer_periods[timer] ||
             (timer_is_type_b(timer) && base == timer_holding_registers[timer / 2u]))) {
            return 0u;
        }
    }
    for (channel = 0u; channel < DSPIC33_ADC_COUNT; channel++) {
        if (adc_pmd_disabled(cpu, channel) && adc_module_address(base, channel)) {
            return 0u;
        }
    }
    if (cpu->io.usb_pmd_disabled && usb_register_address(base)) {
        return 0u;
    }
    if (pwm_address_inaccessible(cpu, base)) {
        return 0u;
    }
    if (address >= INPUT_CAPTURE_BASE &&
        address < INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE) {
        channel = (uint8_t)((address - INPUT_CAPTURE_BASE) / INPUT_CAPTURE_STRIDE);
        if (input_capture_pmd_disabled(cpu, channel)) {
            return 0u;
        }
    }
    if (address >= OUTPUT_COMPARE_BASE &&
        address < OUTPUT_COMPARE_BASE + DSPIC33_OUTPUT_COMPARE_COUNT * OUTPUT_COMPARE_STRIDE) {
        channel = (uint8_t)((address - OUTPUT_COMPARE_BASE) / OUTPUT_COMPARE_STRIDE);
        if (output_compare_pmd_disabled(cpu, channel)) {
            return 0u;
        }
    }
    if (cpu->io.crc.pmd_disabled && base >= CRC_CONTROL && base <= CRC_SHIFT_HIGH) {
        return 0u;
    }
    if (cpu->io.pmp.pmd_disabled && base >= PMP_CONTROL && base <= PMP_STATUS) {
        return 0u;
    }
    pmp_read_register(cpu, address);
    if (dspic33_i2c_read_register(cpu, address, &value)) {
        return value;
    }
    if (qei_read_register(cpu, address, &value)) {
        return value;
    }
    if (dci_read_register(cpu, address, &value)) {
        return value;
    }
    if (base >= RTCC_ALARM_VALUE && base <= RTCC_CONTROL) {
        if (cpu->io.rtcc.pmd_disabled) {
            return 0u;
        }
        if (base == RTCC_ALARM_VALUE) {
            return rtcc_read_window(cpu, address, true);
        }
        if (base == RTCC_VALUE) {
            return rtcc_read_window(cpu, address, false);
        }
    }
    if ((address & 0xfffeu) == 0x072eu) {
        return 0u;
    }
    if ((address & 0xfffeu) == CRC_DATA_LOW || (address & 0xfffeu) == CRC_DATA_HIGH) {
        return 0u;
    }
    if (address == USB_IR && (raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
        return (uint8_t)(value & ~USB_ATTACH_INTERRUPT);
    }
    if (address == USB_IE && (raw_word(cpu, USB_CON) & USB_HOST_ENABLE) == 0u) {
        return (uint8_t)(value & ~USB_ATTACH_INTERRUPT);
    }
    if (!cpu->io.comparator.pmd_disabled && address >= COMPARATOR_BASE + 1u &&
        address < COMPARATOR_BASE + DSPIC33_COMPARATOR_COUNT * COMPARATOR_STRIDE &&
        ((address - COMPARATOR_BASE) % COMPARATOR_STRIDE) == 1u) {
        uint8_t comparator = (uint8_t)((address - COMPARATOR_BASE) / COMPARATOR_STRIDE);
        uint8_t bit = (uint8_t)(1u << comparator);
        if ((value & 1u) != 0u) {
            cpu->io.comparator.last_read_cout |= bit;
        } else {
            cpu->io.comparator.last_read_cout &= (uint8_t)~bit;
        }
    }
    if (address >= INPUT_CAPTURE_BASE &&
        address < INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE &&
        ((address - INPUT_CAPTURE_BASE) % INPUT_CAPTURE_STRIDE) == 5u) {
        channel = (uint8_t)((address - INPUT_CAPTURE_BASE) / INPUT_CAPTURE_STRIDE);
        input_capture_read_complete(cpu, channel);
    }
    if (address == 0x08c3u) {
        return cpu->disicnt != 0u ? (uint8_t)(value | 0x40u) : (uint8_t)(value & ~0x40u);
    }
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t channel_base = can_bases[channel];
        uint16_t word_address = (uint16_t)(address & 0xfffeu);
        uint16_t offset = (uint16_t)(word_address - channel_base);
        if (offset <= 0x7eu && (raw_word(cpu, channel_base) & CAN_WINDOW) != 0u &&
            offset >= 0x20u) {
            uint16_t word = can_filter_word(cpu, channel, offset);
            return (uint8_t)(word >> ((address & 1u) * 8u));
        }
        if (offset == 0x40u && (raw_word(cpu, channel_base) & CAN_WINDOW) == 0u &&
            (cpu->io.can_rx_busy & (uint8_t)(1u << channel)) != 0u &&
            cpu->io.can_rx_word[channel] != 0u) {
            uint16_t word = cpu->io.can_rx_words[channel][cpu->io.can_rx_word[channel] - 1u];
            return (uint8_t)(word >> ((address & 1u) * 8u));
        }
    }
    if ((address & 1u) == 0u) {
        for (timer = 1u; timer < DSPIC33_TIMER_COUNT; timer += 2u) {
            if (address == timer_registers[timer] && timer_pair_enabled(cpu, timer)) {
                raw_write_word(cpu, timer_holding_registers[timer / 2u],
                               raw_word(cpu, timer_registers[timer + 1u]));
                break;
            }
        }
    }
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        if ((address & 0xfffeu) == gpio_port_addresses[port]) {
            uint16_t pins = gpio_pin_values(cpu, port);
            acknowledge_gpio_change_notification(cpu, port, pins);
            return (uint8_t)(pins >> ((address & 1u) * 8u));
        }
    }
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t channel_base = uart_bases[channel];
        bool high_byte = (address & 1u) != 0u;
        bool nine_bit = (raw_word(cpu, channel_base) & UART_MODE_DATA_MASK) == UART_MODE_DATA_MASK;
        if ((address & 0xfffeu) == channel_base + 6u && high_byte == nine_bit) {
            uart_read_complete(cpu, channel);
        }
    }
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t channel_base = spi_bases[channel];
        if ((address & 0xfffeu) == channel_base + 8u && spi_read_complete(cpu, address)) {
            uint16_t discarded;
            if (word_queue_pop(&cpu->io.spi_rx_fifo[channel], &discarded)) {
                if (word_queue_front(&cpu->io.spi_rx_fifo[channel], &discarded)) {
                    raw_write_word(cpu, (uint16_t)(channel_base + 8u), discarded);
                }
                spi_refresh_status(cpu, channel);
                if (spi_enhanced(cpu, channel) && cpu->io.spi_rx_fifo[channel].count == 0u) {
                    spi_raise_mode(cpu, channel, 0u);
                }
            } else {
                raw_write_word(cpu, channel_base,
                               (uint16_t)(raw_word(cpu, channel_base) & ~SPI_RX_FULL));
            }
        }
    }
    return value;
}
