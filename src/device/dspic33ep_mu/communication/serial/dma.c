#include "device/dspic33ep_mu/internal.h"

bool dspic33_uart_receive(Dspic33* cpu, uint8_t channel, uint8_t received_value,
                          uint64_t event_delay) {
    Dspic33UartFrame frame;

    memset(&frame, 0, sizeof(frame));
    frame.value = received_value;
    return dspic33_uart_receive_frame(cpu, channel, &frame, event_delay);
}

bool dspic33_uart_receive_frame(Dspic33* cpu, uint8_t channel, const Dspic33UartFrame* frame,
                                uint64_t event_delay) {
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
    return dspic33_schedule_external(cpu, DSPIC33_EVENT_UART, channel, event_value, event_delay);
}

bool dspic33_uart_set_cts(Dspic33* cpu, uint8_t channel, bool clear, uint64_t event_delay) {
    return channel < DSPIC33_UART_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_UART, channel,
                                     UART_EVENT_CTS | (clear ? 1u : 0u), event_delay);
}

bool dspic33_uart_transmit(Dspic33* cpu, uint8_t channel, Dspic33UartFrame* output_frame) {
    return channel < DSPIC33_UART_COUNT && output_frame != NULL &&
           dspic33_device_internal_uart_queue_pop(&cpu->io.uart_tx[channel], output_frame);
}

bool dspic33_spi_receive(Dspic33* cpu, uint8_t channel, uint16_t received_value,
                         uint64_t event_delay) {
    return channel < DSPIC33_SPI_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_SPI, channel,
                                     SPI_EVENT_EXTERNAL | received_value, event_delay);
}

bool dspic33_spi_select(Dspic33* cpu, uint8_t channel, bool selected, uint64_t event_delay) {
    return channel < DSPIC33_SPI_COUNT &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_SPI_SELECT, channel,
                                     selected ? SPI_SELECT_ACTIVE : 0u, event_delay);
}

bool dspic33_spi_pin_input(Dspic33* cpu, uint8_t channel, bool clock_high, bool data_high,
                           bool select_high) {
    uint16_t spi_base;
    uint16_t control1_value;
    uint16_t control2_value;
    uint8_t channel_bit;
    uint8_t transfer_width;
    bool previous_clock;
    bool previous_selected;
    bool selected;
    bool sample_high;

    if (channel >= DSPIC33_SPI_COUNT) {
        return false;
    }
    spi_base = dspic33_device_spi_bases[channel];
    control1_value = dspic33_device_internal_raw_word(cpu, (uint16_t)(spi_base + 2u));
    control2_value = dspic33_device_internal_raw_word(cpu, (uint16_t)(spi_base + 4u));
    channel_bit = (uint8_t)(1u << channel);
    previous_selected = (cpu->io.spi_selected & channel_bit) != 0u;
    cpu->io.spi_pin_input_enabled |= channel_bit;
    previous_clock = (cpu->io.spi_pin_clock_high & channel_bit) != 0u;
    if (clock_high) {
        cpu->io.spi_pin_clock_high |= channel_bit;
    } else {
        cpu->io.spi_pin_clock_high &= (uint8_t)~channel_bit;
    }
    if (data_high) {
        cpu->io.spi_pin_data_high |= channel_bit;
    } else {
        cpu->io.spi_pin_data_high &= (uint8_t)~channel_bit;
    }
    if (select_high) {
        cpu->io.spi_pin_select_high |= channel_bit;
    } else {
        cpu->io.spi_pin_select_high &= (uint8_t)~channel_bit;
    }
    if ((control2_value & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) ==
        (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) {
        selected = select_high == ((control2_value & SPI_FRAME_ACTIVE_HIGH) != 0u);
    } else if ((control1_value & SPI_SLAVE_SELECT) != 0u) {
        selected = !select_high;
    } else {
        selected = true;
    }
    if (selected) {
        cpu->io.spi_selected |= channel_bit;
    } else {
        cpu->io.spi_selected &= (uint8_t)~channel_bit;
        cpu->io.spi_pin_receive[channel] = 0u;
        cpu->io.spi_pin_bits[channel] = 0u;
    }
    dspic33_device_internal_spi_update_slave_selection(cpu, channel, previous_selected, selected);
    if (previous_selected != selected &&
        dspic33_device_internal_spi_master_frame_slave(cpu, channel)) {
        dspic33_device_internal_spi_schedule_frame_input_sample(cpu, channel);
    }
    if (clock_high == previous_clock || !selected ||
        (dspic33_device_internal_raw_word(cpu, spi_base) & SPI_ENABLE) == 0u ||
        dspic33_device_internal_spi_module_disabled(cpu, channel) ||
        !dspic33_device_internal_spi_power_enabled(cpu, channel) ||
        dspic33_device_internal_spi_master(cpu, channel)) {
        return true;
    }
    sample_high = (control2_value & SPI_FRAME_ENABLE) != 0u
                      ? (control1_value & SPI_CLOCK_POLARITY) != 0u
                      : ((control1_value & SPI_CLOCK_EDGE) != 0u) !=
                            ((control1_value & SPI_CLOCK_POLARITY) != 0u);
    if (clock_high != sample_high) {
        if (dspic33_device_internal_spi_slave_frame_master(cpu, channel) &&
            (cpu->io.spi_busy & channel_bit) != 0u) {
            if ((cpu->io.spi_frame_output_pending & channel_bit) != 0u) {
                cpu->io.spi_frame_output_pending &= (uint8_t)~channel_bit;
                cpu->io.spi_frame_output_clear_pending |= channel_bit;
                if ((control2_value & SPI_FRAME_ACTIVE_HIGH) != 0u) {
                    cpu->io.spi_frame_active |= channel_bit;
                }
            } else if ((cpu->io.spi_frame_output_clear_pending & channel_bit) != 0u) {
                cpu->io.spi_frame_output_clear_pending &= (uint8_t)~channel_bit;
                cpu->io.spi_frame_active &= (uint8_t)~channel_bit;
                cpu->io.spi_pin_output_started |= channel_bit;
                return true;
            }
        }
        if ((cpu->io.spi_busy & channel_bit) != 0u) {
            transfer_width = (control1_value & SPI_MODE_16) != 0u ? 16u : 8u;
            if ((cpu->io.spi_pin_output_started & channel_bit) != 0u) {
                if (cpu->io.spi_pin_output_index[channel] + 1u < transfer_width) {
                    cpu->io.spi_pin_output_index[channel]++;
                }
            } else {
                cpu->io.spi_pin_output_started |= channel_bit;
            }
        }
        return true;
    }
    if (dspic33_device_internal_spi_slave_frame_master(cpu, channel) &&
        ((cpu->io.spi_frame_output_pending | cpu->io.spi_frame_output_clear_pending) &
         channel_bit) != 0u) {
        return true;
    }
    if ((cpu->io.spi_busy & channel_bit) == 0u) {
        cpu->io.spi_busy |= channel_bit;
        cpu->io.spi_shift[channel] =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(spi_base + 8u));
        cpu->io.spi_pin_output_index[channel] = 0u;
        if ((control1_value & SPI_CLOCK_EDGE) != 0u) {
            cpu->io.spi_pin_output_started |= channel_bit;
        } else {
            cpu->io.spi_pin_output_started &= (uint8_t)~channel_bit;
        }
    }
    cpu->io.spi_pin_receive[channel] =
        (uint16_t)((cpu->io.spi_pin_receive[channel] << 1u) | (data_high ? 1u : 0u));
    cpu->io.spi_pin_bits[channel]++;
    transfer_width = (control1_value & SPI_MODE_16) != 0u ? 16u : 8u;
    if (cpu->io.spi_pin_bits[channel] == transfer_width) {
        uint16_t received_value = cpu->io.spi_pin_receive[channel];

        cpu->io.spi_pin_receive[channel] = 0u;
        cpu->io.spi_pin_bits[channel] = 0u;
        dspic33_device_internal_spi_complete_transfer(cpu, channel, received_value);
    }
    return true;
}

bool dspic33_spi_transmit(Dspic33* cpu, uint8_t channel, uint8_t* value) {
    return channel < DSPIC33_SPI_COUNT && value != NULL &&
           dspic33_device_internal_byte_queue_pop(&cpu->io.spi_tx[channel], value);
}

bool dspic33_spi_clock_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t spi_base;
    uint16_t control_value;
    uint16_t frame_control;
    uint8_t channel_bit;
    bool transfer_active;
    bool clock_active;
    uint64_t bit_period_cycles;
    uint64_t elapsed_cycles;

    if (channel >= DSPIC33_SPI_COUNT || high == NULL ||
        dspic33_device_internal_spi_module_disabled(cpu, channel)) {
        return false;
    }
    spi_base = dspic33_device_spi_bases[channel];
    control_value = dspic33_device_internal_raw_word(cpu, (uint16_t)(spi_base + 2u));
    frame_control = dspic33_device_internal_raw_word(cpu, (uint16_t)(spi_base + 4u));
    if ((dspic33_device_internal_raw_word(cpu, spi_base) & SPI_ENABLE) == 0u ||
        !dspic33_device_internal_spi_master(cpu, channel) ||
        (control_value & SPI_DISABLE_CLOCK) != 0u) {
        return false;
    }
    channel_bit = (uint8_t)(1u << channel);
    transfer_active =
        ((cpu->io.spi_busy & channel_bit) != 0u || (frame_control & SPI_FRAME_ENABLE) != 0u) &&
        dspic33_device_internal_spi_power_enabled(cpu, channel);
    if (!transfer_active) {
        *high = (control_value & SPI_CLOCK_POLARITY) != 0u;
        return true;
    }
    bit_period_cycles = dspic33_device_internal_spi_transfer_cycles(cpu, channel) /
                        ((control_value & SPI_MODE_16) != 0u ? 16u : 8u);
    elapsed_cycles = cpu->device_cycles - cpu->io.spi_clock_start_cycle[channel];
    clock_active = elapsed_cycles % bit_period_cycles < bit_period_cycles / 2u;
    *high = clock_active != ((control_value & SPI_CLOCK_POLARITY) != 0u);
    return true;
}

bool dspic33_spi_data_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t spi_base;
    uint16_t control_value;
    uint8_t channel_bit;
    uint8_t transfer_width;
    uint8_t bit_index;
    uint64_t bit_period_cycles;
    uint64_t elapsed_cycles;

    if (channel >= DSPIC33_SPI_COUNT || high == NULL ||
        dspic33_device_internal_spi_module_disabled(cpu, channel)) {
        return false;
    }
    spi_base = dspic33_device_spi_bases[channel];
    control_value = dspic33_device_internal_raw_word(cpu, (uint16_t)(spi_base + 2u));
    if ((dspic33_device_internal_raw_word(cpu, spi_base) & SPI_ENABLE) == 0u ||
        (control_value & SPI_DISABLE_OUTPUT) != 0u) {
        return false;
    }
    channel_bit = (uint8_t)(1u << channel);
    transfer_width = (control_value & SPI_MODE_16) != 0u ? 16u : 8u;
    if (!dspic33_device_internal_spi_master(cpu, channel)) {
        if ((control_value & SPI_SLAVE_SELECT) != 0u &&
            !dspic33_device_internal_spi_selected(cpu, channel)) {
            return false;
        }
        bit_index = cpu->io.spi_pin_output_index[channel];
        if (bit_index >= transfer_width) {
            bit_index = (uint8_t)(transfer_width - 1u);
        }
        *high = (cpu->io.spi_shift[channel] &
                 (uint16_t)(1u << (transfer_width - bit_index - 1u))) != 0u;
        return true;
    }
    if ((cpu->io.spi_busy & channel_bit) == 0u) {
        *high = (cpu->io.spi_shift[channel] & 1u) != 0u;
        return true;
    }
    bit_period_cycles = dspic33_device_internal_spi_transfer_cycles(cpu, channel) / transfer_width;
    elapsed_cycles = cpu->device_cycles - cpu->io.spi_start_cycle[channel];
    bit_index = (uint8_t)(elapsed_cycles / bit_period_cycles);
    if ((control_value & SPI_CLOCK_EDGE) != 0u &&
        elapsed_cycles % bit_period_cycles >= bit_period_cycles / 2u) {
        bit_index++;
    }
    if (bit_index >= transfer_width) {
        bit_index = (uint8_t)(transfer_width - 1u);
    }
    *high =
        (cpu->io.spi_shift[channel] & (uint16_t)(1u << (transfer_width - bit_index - 1u))) != 0u;
    return true;
}

bool dspic33_spi_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    static const uint8_t data_functions[DSPIC33_SPI_COUNT] = {5u, 0u, 31u, 34u};
    static const uint8_t clock_functions[DSPIC33_SPI_COUNT] = {6u, 0u, 32u, 35u};
    uint8_t channel_index;

    if (high == NULL) {
        return false;
    }
    const uint8_t output_function = dspic33_device_internal_pps_output_function(cpu, pin);

    for (channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        if (output_function == data_functions[channel_index] &&
            data_functions[channel_index] != 0u) {
            return dspic33_spi_data_output(cpu, channel_index, high);
        }
        if (output_function == clock_functions[channel_index] &&
            clock_functions[channel_index] != 0u) {
            return dspic33_spi_clock_output(cpu, channel_index, high);
        }
    }
    return false;
}

bool dspic33_spi_frame_output(const Dspic33* cpu, uint8_t channel, bool* high) {
    uint16_t frame_control;
    uint8_t channel_bit;

    if (channel >= DSPIC33_SPI_COUNT || high == NULL ||
        dspic33_device_internal_spi_module_disabled(cpu, channel) ||
        (dspic33_device_internal_raw_word(cpu, dspic33_device_spi_bases[channel]) & SPI_ENABLE) ==
            0u) {
        return false;
    }
    frame_control =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_spi_bases[channel] + 4u));
    if ((frame_control & (SPI_FRAME_ENABLE | SPI_FRAME_SLAVE)) != SPI_FRAME_ENABLE) {
        return false;
    }
    channel_bit = (uint8_t)(1u << channel);
    *high = (frame_control & SPI_FRAME_ACTIVE_HIGH) != 0u
                ? (cpu->io.spi_frame_active & channel_bit) != 0u
                : (cpu->io.spi_frame_active & channel_bit) == 0u;
    return true;
}

bool dspic33_spi_frame_pin(const Dspic33* cpu, uint8_t pin, bool* high) {
    static const uint8_t frame_functions[DSPIC33_SPI_COUNT] = {7u, 10u, 33u, 36u};
    uint8_t channel_index;

    if (high == NULL) {
        return false;
    }
    const uint8_t output_function = dspic33_device_internal_pps_output_function(cpu, pin);

    for (channel_index = 0u; channel_index < DSPIC33_SPI_COUNT; channel_index++) {
        if (output_function == frame_functions[channel_index]) {
            return dspic33_spi_frame_output(cpu, channel_index, high);
        }
    }
    return false;
}

bool dspic33_dma_request(Dspic33* cpu, uint8_t request_source, uint16_t peripheral_offset,
                         uint64_t event_delay) {
    bool all_requests_succeeded = true;

    for (uint8_t channel_index = 0u; channel_index < DSPIC33_DMA_COUNT; channel_index++) {
        const uint16_t channel_base = dspic33_device_internal_dma_channel_base(channel_index);
        const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);

        if ((dspic33_device_internal_raw_word(cpu, channel_base) & DMA_CON_CHEN) == 0u ||
            (dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 2u)) &
             DMA_REQ_SOURCE_MASK) != request_source ||
            (dspic33_device_internal_raw_word(cpu, DMA_PWC) & channel_bit) != 0u) {
            continue;
        }
        if ((cpu->io.dma_peripheral_pending & channel_bit) != 0u) {
            if ((request_source == dspic33_device_can_rx_requests[0] ||
                 request_source == dspic33_device_can_rx_requests[1]) &&
                (cpu->io.dma_arbiter_waiting & channel_bit) != 0u) {
                cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            }
            continue;
        }
        if ((cpu->io.dma_forced_pending & channel_bit) != 0u) {
            dspic33_device_internal_dma_request_collision(cpu, channel_index);
        }
        if (!dspic33_device_internal_schedule_dma_channel(cpu, channel_index, peripheral_offset,
                                                          false, event_delay)) {
            all_requests_succeeded = false;
        }
    }
    return all_requests_succeeded;
}

bool dspic33_pmp_transmit(Dspic33* cpu, Dspic33PmpTransfer* transfer) {
    return transfer != NULL &&
           dspic33_device_internal_pmp_output_pop(&cpu->io.pmp.output, transfer);
}

bool dspic33_pmp_respond(Dspic33* cpu, uint16_t response_value, uint64_t event_delay) {
    Dspic33PmpResponse response;

    if (event_delay > UINT64_MAX - cpu->device_cycles) {
        return false;
    }
    response.cycle = cpu->device_cycles + event_delay;
    response.value = response_value;
    return dspic33_device_internal_pmp_response_push(&cpu->io.pmp.input, &response);
}

bool dspic33_pmp_slave_read(Dspic33* cpu, uint8_t slave_address, uint64_t event_delay) {
    return slave_address < 4u &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_SLAVE_READ, slave_address,
                                     event_delay);
}

bool dspic33_pmp_slave_write(Dspic33* cpu, uint8_t slave_address, uint8_t write_value,
                             uint64_t event_delay) {
    return slave_address < 4u &&
           dspic33_schedule_external(cpu, DSPIC33_EVENT_PMP, PMP_EVENT_SLAVE_WRITE,
                                     ((uint32_t)slave_address << 8u) | write_value, event_delay);
}
