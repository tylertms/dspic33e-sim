
bool dspic33_uart_receive(Dspic33* cpu, uint8_t channel, uint8_t value, uint64_t delay) {
    Dspic33UartFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.value = value;
    return dspic33_uart_receive_frame(cpu, channel, &frame, delay);
}

bool dspic33_uart_receive_frame(Dspic33* cpu, uint8_t channel, const Dspic33UartFrame* frame,
                                uint64_t delay) {
    uint32_t event_value;
    if (channel >= DSPIC33_UART_COUNT || frame == NULL || frame->value > 0x01ffu) {
        return false;
    }
    event_value = frame->value | ((uint32_t)frame->baud_period << UART_EVENT_BAUD_SHIFT);
    if (frame->parity_error) {
        event_value |= UART_EVENT_PARITY_ERROR;
    }
    if (frame->framing_error) {
        event_value |= UART_EVENT_FRAMING_ERROR;
    }
    return dspic33_schedule_external(cpu, DSPIC33_EVENT_UART, channel, event_value, delay);
}

bool dspic33_uart_set_cts(Dspic33* cpu, uint8_t channel, bool clear, uint64_t delay) {
    return channel < DSPIC33_UART_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_UART, channel,
                                     UART_EVENT_CTS | (clear ? 1u : 0u), delay);
}

bool dspic33_uart_transmit(Dspic33* cpu, uint8_t channel, Dspic33UartFrame* frame) {
    return channel < DSPIC33_UART_COUNT && frame != NULL &&
           uart_queue_pop(&cpu->io.uart_tx[channel], frame);
}

bool dspic33_spi_receive(Dspic33* cpu, uint8_t channel, uint16_t value, uint64_t delay) {
    return channel < DSPIC33_SPI_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_SPI, channel, SPI_EVENT_EXTERNAL | value,
                                     delay);
}

bool dspic33_spi_select(Dspic33* cpu, uint8_t channel, bool selected, uint64_t delay) {
    return channel < DSPIC33_SPI_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_SPI_SELECT, channel,
                                     selected ? SPI_SELECT_ACTIVE : 0u, delay);
}

bool dspic33_spi_pin_input(Dspic33* cpu, uint8_t channel, bool clock_high, bool data_high,
                           bool select_high) {
    uint16_t base;
    uint16_t control1;
    uint16_t control2;
    uint8_t bit;
    uint8_t width;
    bool previous_clock;
    bool previous_selected;
    bool selected;
    bool sample_high;
    if (channel >= DSPIC33_SPI_COUNT) {
        return false;
    }
    base = spi_bases[channel];
    control1 = raw_word(cpu, (uint16_t)(base + 2u));
    control2 = raw_word(cpu, (uint16_t)(base + 4u));
    bit = (uint8_t)(1u << channel);
    previous_selected = (cpu->io.spi_selected & bit) != 0u;
    cpu->io.spi_pin_input_enabled |= bit;
    previous_clock = (cpu->io.spi_pin_clock_high & bit) != 0u;
    if (clock_high) {
        cpu->io.spi_pin_clock_high |= bit;
    } else {
        cpu->io.spi_pin_clock_high &= (uint8_t)~bit;
    }
    if (data_high) {
        cpu->io.spi_pin_data_high |= bit;
    } else {
        cpu->io.spi_pin_data_high &= (uint8_t)~bit;
    }
    if (select_high) {
        cpu->io.spi_pin_select_high |= bit;
    } else {
        cpu->io.spi_pin_select_high &= (uint8_t)~bit;
    }
    if ((control2 & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) == (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) {
        selected = select_high == ((control2 & SPI_FRAME_ACTIVE_HIGH) != 0u);
    } else if ((control1 & SPI_SLAVE_SELECT) != 0u) {
        selected = !select_high;
    } else {
        selected = true;
    }
    if (selected) {
        cpu->io.spi_selected |= bit;
    } else {
        cpu->io.spi_selected &= (uint8_t)~bit;
        cpu->io.spi_pin_receive[channel] = 0u;
        cpu->io.spi_pin_bits[channel] = 0u;
    }
    spi_update_slave_selection(cpu, channel, previous_selected, selected);
    if (previous_selected != selected && spi_master_frame_slave(cpu, channel)) {
        spi_schedule_frame_input_sample(cpu, channel);
    }
    if (clock_high == previous_clock || !selected || (raw_word(cpu, base) & SPI_ENABLE) == 0u ||
        spi_module_disabled(cpu, channel) || !spi_power_enabled(cpu, channel) ||
        spi_master(cpu, channel)) {
        return true;
    }
    sample_high = (control2 & SPI_FRAME_ENABLE) != 0u ? (control1 & SPI_CLOCK_POLARITY) != 0u
                                                      : ((control1 & SPI_CLOCK_EDGE) != 0u) !=
                                                            ((control1 & SPI_CLOCK_POLARITY) != 0u);
    if (clock_high != sample_high) {
        if (spi_slave_frame_master(cpu, channel) && (cpu->io.spi_busy & bit) != 0u) {
            if ((cpu->io.spi_frame_output_pending & bit) != 0u) {
                cpu->io.spi_frame_output_pending &= (uint8_t)~bit;
                cpu->io.spi_frame_output_clear_pending |= bit;
                if ((control2 & SPI_FRAME_ACTIVE_HIGH) != 0u) {
                    cpu->io.spi_frame_active |= bit;
                }
            } else if ((cpu->io.spi_frame_output_clear_pending & bit) != 0u) {
                cpu->io.spi_frame_output_clear_pending &= (uint8_t)~bit;
                cpu->io.spi_frame_active &= (uint8_t)~bit;
                cpu->io.spi_pin_output_started |= bit;
                return true;
            }
        }
        if ((cpu->io.spi_busy & bit) != 0u) {
            width = (control1 & SPI_MODE_16) != 0u ? 16u : 8u;
            if ((cpu->io.spi_pin_output_started & bit) != 0u) {
                if (cpu->io.spi_pin_output_index[channel] + 1u < width) {
                    cpu->io.spi_pin_output_index[channel]++;
                }
            } else {
                cpu->io.spi_pin_output_started |= bit;
            }
        }
        return true;
    }
    if (spi_slave_frame_master(cpu, channel) &&
        ((cpu->io.spi_frame_output_pending | cpu->io.spi_frame_output_clear_pending) & bit) != 0u) {
        return true;
    }
    if ((cpu->io.spi_busy & bit) == 0u) {
        cpu->io.spi_busy |= bit;
        cpu->io.spi_shift[channel] = raw_word(cpu, (uint16_t)(base + 8u));
        cpu->io.spi_pin_output_index[channel] = 0u;
        if ((control1 & SPI_CLOCK_EDGE) != 0u) {
            cpu->io.spi_pin_output_started |= bit;
        } else {
            cpu->io.spi_pin_output_started &= (uint8_t)~bit;
        }
    }
    cpu->io.spi_pin_receive[channel] =
        (uint16_t)((cpu->io.spi_pin_receive[channel] << 1u) | (data_high ? 1u : 0u));
    cpu->io.spi_pin_bits[channel]++;
    width = (control1 & SPI_MODE_16) != 0u ? 16u : 8u;
    if (cpu->io.spi_pin_bits[channel] == width) {
        uint16_t value = cpu->io.spi_pin_receive[channel];
        cpu->io.spi_pin_receive[channel] = 0u;
        cpu->io.spi_pin_bits[channel] = 0u;
        spi_complete_transfer(cpu, channel, value);
    }
    return true;
}

bool dspic33_spi_transmit(Dspic33* cpu, uint8_t channel, uint8_t* value) {
    return channel < DSPIC33_SPI_COUNT && value != NULL &&
           byte_queue_pop(&cpu->io.spi_tx[channel], value);
}

bool dspic33_spi_clock_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t base;
    uint16_t control;
    uint16_t frame;
    uint8_t bit;
    bool running;
    bool active;
    uint64_t period;
    uint64_t elapsed;
    if (channel >= DSPIC33_SPI_COUNT || high == NULL || spi_module_disabled(cpu, channel)) {
        return false;
    }
    base = spi_bases[channel];
    control = raw_word(cpu, (uint16_t)(base + 2u));
    frame = raw_word(cpu, (uint16_t)(base + 4u));
    if ((raw_word(cpu, base) & SPI_ENABLE) == 0u || !spi_master(cpu, channel) ||
        (control & SPI_DISABLE_CLOCK) != 0u) {
        return false;
    }
    bit = (uint8_t)(1u << channel);
    running = ((cpu->io.spi_busy & bit) != 0u || (frame & SPI_FRAME_ENABLE) != 0u) &&
              spi_power_enabled(cpu, channel);
    if (!running) {
        *high = (control & SPI_CLOCK_POLARITY) != 0u;
        return true;
    }
    period = spi_transfer_cycles(cpu, channel) / ((control & SPI_MODE_16) != 0u ? 16u : 8u);
    elapsed = cpu->device_cycles - cpu->io.spi_clock_start_cycle[channel];
    active = elapsed % period < period / 2u;
    *high = active != ((control & SPI_CLOCK_POLARITY) != 0u);
    return true;
}

bool dspic33_spi_data_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t base;
    uint16_t control;
    uint8_t bit;
    uint8_t bits;
    uint8_t index;
    uint64_t period;
    uint64_t elapsed;
    if (channel >= DSPIC33_SPI_COUNT || high == NULL || spi_module_disabled(cpu, channel)) {
        return false;
    }
    base = spi_bases[channel];
    control = raw_word(cpu, (uint16_t)(base + 2u));
    if ((raw_word(cpu, base) & SPI_ENABLE) == 0u || (control & SPI_DISABLE_OUTPUT) != 0u) {
        return false;
    }
    bit = (uint8_t)(1u << channel);
    bits = (control & SPI_MODE_16) != 0u ? 16u : 8u;
    if (!spi_master(cpu, channel)) {
        if ((control & SPI_SLAVE_SELECT) != 0u && !spi_selected(cpu, channel)) {
            return false;
        }
        index = cpu->io.spi_pin_output_index[channel];
        if (index >= bits) {
            index = (uint8_t)(bits - 1u);
        }
        *high = (cpu->io.spi_shift[channel] & (uint16_t)(1u << (bits - index - 1u))) != 0u;
        return true;
    }
    if ((cpu->io.spi_busy & bit) == 0u) {
        *high = (cpu->io.spi_shift[channel] & 1u) != 0u;
        return true;
    }
    period = spi_transfer_cycles(cpu, channel) / bits;
    elapsed = cpu->device_cycles - cpu->io.spi_start_cycle[channel];
    index = (uint8_t)(elapsed / period);
    if ((control & SPI_CLOCK_EDGE) != 0u && elapsed % period >= period / 2u) {
        index++;
    }
    if (index >= bits) {
        index = (uint8_t)(bits - 1u);
    }
    *high = (cpu->io.spi_shift[channel] & (uint16_t)(1u << (bits - index - 1u))) != 0u;
    return true;
}

bool dspic33_spi_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    static const uint8_t data_functions[DSPIC33_SPI_COUNT] = {5u, 0u, 31u, 34u};
    static const uint8_t clock_functions[DSPIC33_SPI_COUNT] = {6u, 0u, 32u, 35u};
    uint8_t function = 0u;
    uint8_t channel;
    size_t index;
    if (high == NULL) {
        return false;
    }
    for (index = 0u; index < sizeof(pps_outputs) / sizeof(pps_outputs[0]); index++) {
        if (pps_outputs[index].pin == pin) {
            function =
                (uint8_t)((raw_word(cpu, pps_outputs[index].address) >> pps_outputs[index].shift) &
                          0x003fu);
            break;
        }
    }
    if (index == sizeof(pps_outputs) / sizeof(pps_outputs[0])) {
        return false;
    }
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        if (function == data_functions[channel] && data_functions[channel] != 0u) {
            return dspic33_spi_data_output(cpu, channel, high);
        }
        if (function == clock_functions[channel] && clock_functions[channel] != 0u) {
            return dspic33_spi_clock_output(cpu, channel, high);
        }
    }
    return false;
}

bool dspic33_spi_frame_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t control;
    uint8_t bit;
    if (channel >= DSPIC33_SPI_COUNT || high == NULL || spi_module_disabled(cpu, channel) ||
        (raw_word(cpu, spi_bases[channel]) & SPI_ENABLE) == 0u) {
        return false;
    }
    control = raw_word(cpu, (uint16_t)(spi_bases[channel] + 4u));
    if ((control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) != SPI_FRAME_ENABLE) {
        return false;
    }
    bit = (uint8_t)(1u << channel);
    *high = (control & SPI_FRAME_ACTIVE_HIGH) != 0u ? (cpu->io.spi_frame_active & bit) != 0u
                                                    : (cpu->io.spi_frame_active & bit) == 0u;
    return true;
}

bool dspic33_spi_frame_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    static const uint8_t functions[DSPIC33_SPI_COUNT] = {7u, 10u, 33u, 36u};
    uint8_t function = 0u;
    uint8_t channel;
    size_t index;
    if (high == NULL) {
        return false;
    }
    for (index = 0u; index < sizeof(pps_outputs) / sizeof(pps_outputs[0]); index++) {
        if (pps_outputs[index].pin == pin) {
            function =
                (uint8_t)((raw_word(cpu, pps_outputs[index].address) >> pps_outputs[index].shift) &
                          0x003fu);
            break;
        }
    }
    if (index == sizeof(pps_outputs) / sizeof(pps_outputs[0])) {
        return false;
    }
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        if (function == functions[channel]) {
            return dspic33_spi_frame_output(cpu, channel, high);
        }
    }
    return false;
}

bool dspic33_dma_request(Dspic33* cpu, uint8_t request, uint16_t indirect_address, uint64_t delay) {
    uint8_t channel;
    bool succeeded = true;
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint16_t base = dma_channel_base(channel);
        uint16_t bit = dma_channel_bit(channel);
        if ((raw_word(cpu, base) & DMA_CON_CHEN) == 0u ||
            (raw_word(cpu, (uint16_t)(base + 2u)) & DMA_REQ_SOURCE_MASK) != request ||
            (raw_word(cpu, DMA_PWC) & bit) != 0u) {
            continue;
        }
        if ((cpu->io.dma_peripheral_pending & bit) != 0u) {
            if ((request == can_rx_requests[0] || request == can_rx_requests[1]) &&
                (cpu->io.dma_arbiter_waiting & bit) != 0u) {
                cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            }
            continue;
        }
        if ((cpu->io.dma_forced_pending & bit) != 0u) {
            dma_request_collision(cpu, channel);
        }
        if (!schedule_dma_channel(cpu, channel, indirect_address, false, delay)) {
            succeeded = false;
        }
    }
    return succeeded;
}

bool dspic33_pmp_transmit(Dspic33* cpu, Dspic33PmpTransfer* transfer) {
    return transfer != NULL && pmp_output_pop(&cpu->io.pmp.output, transfer);
}

bool dspic33_pmp_respond(Dspic33* cpu, uint16_t value, uint64_t delay) {
    Dspic33PmpResponse response;
    if (delay > UINT64_MAX - cpu->device_cycles) {
        return false;
    }
    response.cycle = cpu->device_cycles + delay;
    response.value = value;
    return pmp_response_push(&cpu->io.pmp.input, &response);
}

bool dspic33_pmp_slave_read(Dspic33* cpu, uint8_t address, uint64_t delay) {
    return address < 4u &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_SLAVE_READ, address, delay);
}

bool dspic33_pmp_slave_write(Dspic33* cpu, uint8_t address, uint8_t value, uint64_t delay) {
    return address < 4u && dspic33_schedule_external(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_SLAVE_WRITE,
                                                     ((uint32_t)address << 8u) | value, delay);
}
