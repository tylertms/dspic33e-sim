#include "device/dspic33ep_mu/internal.h"

static void can_receive_sample(Dspic33* cpu, uint8_t channel_index, bool sampled_bus_level) {
    const uint8_t channel_mask = (uint8_t)(1u << channel_index);
    Dspic33CanFrame received_frame;
    Dspic33CanSerialResult serial_result;
    uint16_t received_bit_count;
    uint16_t stuffing_index;

    if ((cpu->io.can_rx_serial_active & channel_mask) == 0u ||
        !dspic33_device_internal_can_serial_receive_enabled(cpu, channel_index)) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
        return;
    }
    received_bit_count = cpu->io.can_rx_serial_count[channel_index];
    if (received_bit_count >= sizeof(cpu->io.can_rx_serial_bits[channel_index])) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
        dspic33_device_internal_can_invalid_event(cpu, channel_index);
        return;
    }
    cpu->io.can_rx_serial_bits[channel_index][received_bit_count] = sampled_bus_level;
    cpu->io.can_rx_serial_count[channel_index]++;
    serial_result = dspic33_device_internal_can_decode_serial(cpu, channel_index, &received_frame,
                                                              &stuffing_index);

    if (serial_result == CAN_SERIAL_INCOMPLETE && stuffing_index != 0u &&
        cpu->io.can_rx_serial_count[channel_index] == stuffing_index + 1u &&
        cpu->io.can_rx_serial_bits[channel_index][stuffing_index] != 0u &&
        (cpu->io.can_tx_on_bus & channel_mask) == 0u &&
        dspic33_device_internal_can_mode(cpu, channel_index) != CAN_MODE_LISTEN) {
        const uint64_t bit_cycles = dspic33_device_internal_can_bit_cycles(cpu, channel_index);
        const uint64_t sample_cycles =
            dspic33_device_internal_can_sample_cycles(cpu, channel_index);
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_ACK_START,
                              bit_cycles - sample_cycles)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
            return;
        }
    }
    if (serial_result == CAN_SERIAL_VALID) {
        const bool overload_detected =
            !cpu->io.can_rx_serial_bits[channel_index][stuffing_index + 9u];
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
        cpu->io.can_intermission_generation[channel_index]++;
        cpu->io.can_overload_count[channel_index] = 0u;
        if (!dspic33_device_internal_can_schedule_intermission(cpu, channel_index)) {
            cpu->io.can_intermission_active &= (uint8_t)~channel_mask;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
        if ((cpu->io.can_tx_on_bus & channel_mask) != 0u) {
            return;
        }
        dspic33_device_internal_can_receive_success(cpu, channel_index);
        if (!dspic33_device_internal_can_queue_push(&cpu->io.can_rx[channel_index],
                                                    &received_frame) ||
            !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_START, 0u) ||
            (((cpu->io.can_tx_retry_wait & channel_mask) != 0u) &&
             !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_RETRY,
                               3u * dspic33_device_internal_can_bit_cycles(cpu, channel_index)))) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        if (overload_detected &&
            dspic33_device_internal_can_mode(cpu, channel_index) != CAN_MODE_LISTEN) {
            dspic33_device_internal_can_start_overload(cpu, channel_index);
        }
        return;
    }
    if (serial_result == CAN_SERIAL_INVALID) {
        dspic33_device_internal_can_receive_error(cpu, channel_index, &received_frame);
        return;
    }
    uint64_t sample_delay = dspic33_device_internal_can_bit_cycles(cpu, channel_index);
    if (dspic33_device_internal_can_triple_sample(cpu, channel_index)) {
        sample_delay -= 2u * dspic33_device_internal_can_time_quantum(cpu, channel_index);
    }
    if (!dspic33_device_internal_can_schedule_receive_sample(cpu, channel_index, sample_delay)) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_first(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_mask = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_rx_serial_active & channel_mask) == 0u) {
        return;
    }
    cpu->io.can_rx_sample_high[channel_index] =
        (cpu->io.can_rx_pin_high & channel_mask) != 0u ? 1u : 0u;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_SAMPLE_SECOND,
                          dspic33_device_internal_can_time_quantum(cpu, channel_index))) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_second(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_mask = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_rx_serial_active & channel_mask) == 0u) {
        return;
    }
    if ((cpu->io.can_rx_pin_high & channel_mask) != 0u) {
        cpu->io.can_rx_sample_high[channel_index]++;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_SAMPLE,
                          dspic33_device_internal_can_time_quantum(cpu, channel_index))) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_final(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_mask = (uint8_t)(1u << channel_index);
    uint8_t high_sample_count = cpu->io.can_rx_sample_high[channel_index];

    if ((cpu->io.can_rx_pin_high & channel_mask) != 0u) {
        high_sample_count++;
    }
    cpu->io.can_rx_sample_high[channel_index] = 0u;
    can_receive_sample(cpu, channel_index, high_sample_count >= 2u);
}

static void can_ack_start(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_mask = (uint8_t)(1u << channel_index);

    if (!dspic33_device_internal_can_serial_receive_enabled(cpu, channel_index) ||
        dspic33_device_internal_can_mode(cpu, channel_index) == CAN_MODE_LISTEN) {
        return;
    }
    cpu->io.can_rx_ack |= channel_mask;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_ACK_FINISH,
                          dspic33_device_internal_can_bit_cycles(cpu, channel_index))) {
        cpu->io.can_rx_ack &= (uint8_t)~channel_mask;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_ack_finish(Dspic33* cpu, uint8_t channel_index) {
    cpu->io.can_rx_ack &= (uint8_t)~(uint8_t)(1u << channel_index);
}

static void can_receive_error_start(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_mask = (uint8_t)(1u << channel_index);

    cpu->io.can_rx_error_active |= channel_mask;
    cpu->io.can_rx_error_start_cycle[channel_index] = cpu->device_cycles;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_ERROR_FINISH,
                          14u * dspic33_device_internal_can_bit_cycles(cpu, channel_index))) {
        cpu->io.can_rx_error_active &= (uint8_t)~channel_mask;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_error_finish(Dspic33* cpu, uint8_t channel_index) {
    cpu->io.can_rx_error_active &= (uint8_t)~(uint8_t)(1u << channel_index);
}

static void can_mode_transition(Dspic33* cpu, uint8_t channel_index, uint32_t event_value) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);
    const uint8_t requested_mode = (uint8_t)((event_value >> CAN_EVENT_MODE_SHIFT) & 7u);
    const uint16_t mode_generation = (uint16_t)(event_value >> CAN_EVENT_MODE_GENERATION_SHIFT);
    const uint16_t mode_control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel_index]);
    const uint8_t configured_mode = (uint8_t)((mode_control & CAN_MODE_MASK) >> CAN_MODE_SHIFT);

    if (mode_generation != cpu->io.can_mode_generation[channel_index] ||
        requested_mode != configured_mode ||
        requested_mode == dspic33_device_internal_can_mode(cpu, channel_index)) {
        return;
    }
    if (!dspic33_device_internal_can_power_enabled(cpu, channel_index) ||
        (cpu->io.can_rx_pin_high & channel_bit) == 0u ||
        ((cpu->io.can_tx_busy | cpu->io.can_rx_busy | cpu->io.can_rx_serial_active |
          cpu->io.can_tx_error_active | cpu->io.can_rx_error_active | cpu->io.can_rx_ack) &
         channel_bit) != 0u) {
        cpu->io.can_mode_generation[channel_index]++;
        if (!dspic33_device_internal_can_schedule_mode_transition(cpu, channel_index,
                                                                  requested_mode)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        return;
    }
    dspic33_device_internal_raw_write_word(
        cpu, dspic33_device_can_bases[channel_index],
        (uint16_t)((mode_control & ~0x00e0u) | ((uint16_t)requested_mode << 5u)));
    if (requested_mode == CAN_MODE_CONFIGURATION) {
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0eu), 0u);
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0au),
            (uint16_t)(dspic33_device_internal_raw_word(
                           cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0au)) &
                       0x00ffu));
        dspic33_device_internal_can_refresh_error_status(cpu, channel_index);
    }
    if (requested_mode == CAN_MODE_NORMAL || requested_mode == CAN_MODE_LOOPBACK) {
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_START,
                              0u)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

void dspic33_device_internal_can_refresh_error_status(Dspic33* cpu, uint8_t channel_index) {
    const uint16_t can_base = dspic33_device_can_bases[channel_index];
    const uint16_t error_counters =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(can_base + 0x0eu));
    uint16_t error_status = dspic33_device_internal_raw_word(cpu, (uint16_t)(can_base + 0x0au));
    const uint8_t receive_error_count = (uint8_t)error_counters;
    const uint8_t transmit_error_count = (uint8_t)(error_counters >> 8u);
    const bool bus_off = (error_status & CAN_BUS_OFF) != 0u;

    error_status &= 0x00ffu;
    if (receive_error_count >= 96u || transmit_error_count >= 96u) {
        error_status |= CAN_ERROR_WARNING;
    }
    if (receive_error_count >= 96u && receive_error_count < 128u) {
        error_status |= CAN_RECEIVE_WARNING;
    }
    if (transmit_error_count >= 96u && transmit_error_count < 128u) {
        error_status |= CAN_TRANSMIT_WARNING;
    }
    if (receive_error_count >= 128u) {
        error_status |= CAN_RECEIVE_PASSIVE;
    }
    if (transmit_error_count >= 128u && !bus_off) {
        error_status |= CAN_TRANSMIT_PASSIVE;
    }
    if (bus_off) {
        error_status |= CAN_BUS_OFF;
    }
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(can_base + 0x0au), error_status);
}

static void can_receive_start(Dspic33* cpu, uint8_t channel_index) {
    Dspic33CanFrame received_frame;
    uint8_t receive_buffer;
    uint8_t filter_index;
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_rx_busy & channel_bit) != 0u ||
        !dspic33_device_internal_can_queue_pop(&cpu->io.can_rx[channel_index], &received_frame)) {
        return;
    }
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        if ((dspic33_device_internal_raw_word(
                 cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x12u)) &
             CAN_WAKE_FILTER) != 0u) {
            dspic33_device_internal_can_raise_event(cpu, channel_index, CAN_INTERRUPT_WAKE, 0u, 0u);
        }
        return;
    }
    if (!dspic33_device_internal_can_power_enabled(cpu, channel_index) ||
        dspic33_device_internal_can_mode(cpu, channel_index) == CAN_MODE_DISABLE ||
        dspic33_device_internal_can_mode(cpu, channel_index) == CAN_MODE_CONFIGURATION) {
        return;
    }
    dspic33_device_internal_can_capture_received_frame(cpu, channel_index);
    if (dspic33_device_internal_can_mode(cpu, channel_index) == CAN_MODE_LISTEN_ALL) {
        bool is_fifo_buffer;
        bool is_transmit_buffer;
        filter_index = 0u;
        receive_buffer =
            dspic33_device_internal_can_filter_buffer(cpu, channel_index, filter_index);
        is_fifo_buffer = receive_buffer == 15u;
        if (is_fifo_buffer) {
            receive_buffer = cpu->io.can_fifo_write[channel_index];
        }
        is_transmit_buffer =
            receive_buffer < 8u &&
            (dspic33_device_internal_can_buffer_control(cpu, channel_index, receive_buffer) &
             CAN_BUFFER_TRANSMIT) != 0u;
        if (receive_buffer >= dspic33_device_internal_can_buffer_count(cpu, channel_index) ||
            is_transmit_buffer ||
            dspic33_device_internal_can_buffer_flag(cpu, channel_index, receive_buffer, false)) {
            if (receive_buffer < 32u) {
                dspic33_device_internal_can_set_buffer_flag(cpu, channel_index, receive_buffer,
                                                            true);
                dspic33_device_internal_can_raise_event(cpu, channel_index, CAN_INTERRUPT_OVERFLOW,
                                                        receive_buffer, filter_index);
                if (is_fifo_buffer) {
                    dspic33_device_internal_can_advance_fifo_write(cpu, channel_index,
                                                                   receive_buffer);
                }
            }
            return;
        }
    } else if (!dspic33_device_internal_can_select_receive_buffer(
                   cpu, channel_index, &received_frame, &receive_buffer, &filter_index)) {
        return;
    }
    if (!dspic33_device_internal_can_dma_ready(
            cpu, dspic33_device_can_rx_requests[channel_index],
            (uint16_t)(dspic33_device_can_bases[channel_index] + 0x40u), false)) {
        dspic33_raise_interrupt(cpu, dspic33_device_can_rx_irqs[channel_index]);
        return;
    }
    dspic33_device_internal_can_encode_frame(&received_frame, filter_index,
                                             cpu->io.can_rx_words[channel_index]);
    cpu->io.can_rx_buffer[channel_index] = receive_buffer;
    cpu->io.can_rx_filter[channel_index] = filter_index;
    cpu->io.can_rx_word[channel_index] = 0u;
    cpu->io.can_rx_busy |= channel_bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_WORD, 0u);
}

static void can_receive_success_start(Dspic33* cpu, uint8_t channel_index) {
    dspic33_device_internal_can_receive_success(cpu, channel_index);
    can_receive_start(cpu, channel_index);
}

static void can_receive_word(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t receive_word_index = cpu->io.can_rx_word[channel_index];

    if (receive_word_index >= 8u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_FINISH, 0u);
        return;
    }
    cpu->io.can_rx_word[channel_index]++;
    dspic33_raise_interrupt(cpu, dspic33_device_can_rx_irqs[channel_index]);
    dspic33_dma_request(
        cpu, dspic33_device_can_rx_requests[channel_index],
        (uint16_t)(cpu->io.can_rx_buffer[channel_index] * 16u + receive_word_index * 2u), 0u);
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_WORD, 1u);
}

static void can_receive_finish(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t receive_buffer = cpu->io.can_rx_buffer[channel_index];
    const uint8_t filter_index = cpu->io.can_rx_filter[channel_index];
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);
    uint8_t next_fifo_buffer;
    const uint16_t can_control = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 6u));
    uint16_t fifo_state;

    dspic33_device_internal_can_set_buffer_flag(cpu, channel_index, receive_buffer, false);
    if (dspic33_device_internal_can_filter_buffer(cpu, channel_index, filter_index) == 15u) {
        next_fifo_buffer =
            dspic33_device_internal_can_advance_fifo_write(cpu, channel_index, receive_buffer);
        fifo_state = dspic33_device_internal_raw_word(
            cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 8u));
        if ((fifo_state & 0x003fu) == next_fifo_buffer + 1u ||
            (((fifo_state & 0x003fu) == (can_control & 0x001fu)) &&
             next_fifo_buffer == dspic33_device_internal_can_fifo_end(cpu, channel_index))) {
            dspic33_device_internal_can_raise_event(cpu, channel_index, CAN_INTERRUPT_FIFO,
                                                    receive_buffer, filter_index);
        }
    }
    dspic33_device_internal_can_raise_event(cpu, channel_index, CAN_INTERRUPT_RECEIVE,
                                            receive_buffer, filter_index);
    cpu->io.can_rx_busy &= (uint8_t)~channel_bit;
    if (cpu->io.can_rx[channel_index].count != 0u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_START, 0u);
    }
}

static int can_transmit_selection(const Dspic33* cpu, uint8_t channel_index) {
    int selected_buffer = -1;
    uint8_t selected_priority = 0u;

    for (uint8_t buffer_index = 0u; buffer_index < 8u; buffer_index++) {
        const uint16_t buffer_control =
            dspic33_device_internal_can_buffer_control(cpu, channel_index, buffer_index);
        const uint8_t buffer_priority = (uint8_t)(buffer_control & 3u);

        if ((buffer_control & (CAN_BUFFER_TRANSMIT | CAN_BUFFER_REQUEST)) !=
            (CAN_BUFFER_TRANSMIT | CAN_BUFFER_REQUEST)) {
            continue;
        }
        if (selected_buffer < 0 || buffer_priority > selected_priority ||
            (buffer_priority == selected_priority && buffer_index > (uint8_t)selected_buffer)) {
            selected_buffer = buffer_index;
            selected_priority = buffer_priority;
        }
    }
    return selected_buffer;
}

static void can_transmit_start(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);
    int selected_buffer;

    if ((cpu->io.can_tx_busy & channel_bit) != 0u ||
        (cpu->io.can_tx_retry_wait & channel_bit) != 0u ||
        !dspic33_device_internal_can_power_enabled(cpu, channel_index) ||
        (dspic33_device_internal_can_mode(cpu, channel_index) != CAN_MODE_NORMAL &&
         dspic33_device_internal_can_mode(cpu, channel_index) != CAN_MODE_LOOPBACK) ||
        (dspic33_device_internal_raw_word(
             cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0au)) &
         CAN_BUS_OFF) != 0u) {
        return;
    }
    selected_buffer = can_transmit_selection(cpu, channel_index);
    if (selected_buffer < 0) {
        return;
    }
    if (!dspic33_device_internal_can_dma_ready(
            cpu, dspic33_device_can_tx_requests[channel_index],
            (uint16_t)(dspic33_device_can_bases[channel_index] + 0x42u), true)) {
        dspic33_raise_interrupt(cpu, dspic33_device_can_tx_irqs[channel_index]);
        return;
    }
    cpu->io.can_tx_buffer[channel_index] = (uint8_t)selected_buffer;
    cpu->io.can_tx_word[channel_index] = 0u;
    memset(cpu->io.can_tx_words[channel_index], 0, sizeof(cpu->io.can_tx_words[channel_index]));
    cpu->io.can_tx_busy |= channel_bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_WORD, 0u);
}

static void can_transmit_word(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t transmit_word_index = cpu->io.can_tx_word[channel_index];

    if (transmit_word_index >= 8u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_FINISH, 0u);
        return;
    }
    cpu->io.can_tx_word[channel_index]++;
    dspic33_raise_interrupt(cpu, dspic33_device_can_tx_irqs[channel_index]);
    dspic33_dma_request(
        cpu, dspic33_device_can_tx_requests[channel_index],
        (uint16_t)(cpu->io.can_tx_buffer[channel_index] * 16u + transmit_word_index * 2u), 0u);
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_WORD, 1u);
}

static void can_transmit_bus_finish(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t transmit_buffer = cpu->io.can_tx_buffer[channel_index];
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);
    const uint16_t buffer_control =
        dspic33_device_internal_can_buffer_control(cpu, channel_index, transmit_buffer);
    uint16_t error_counters = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0eu));
    const Dspic33CanFrame transmitted_frame =
        dspic33_device_internal_can_decode_frame(cpu->io.can_tx_words[channel_index]);

    dspic33_device_internal_can_set_buffer_control(
        cpu, channel_index, transmit_buffer, (uint16_t)(buffer_control & ~CAN_BUFFER_REQUEST));
    if ((error_counters >> 8u) != 0u) {
        error_counters = (uint16_t)(error_counters - 0x0100u);
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0eu), error_counters);
        dspic33_device_internal_can_refresh_error_status(cpu, channel_index);
    }
    dspic33_device_internal_can_raise_event(cpu, channel_index, CAN_INTERRUPT_TRANSMIT,
                                            transmit_buffer, 0u);
    if (dspic33_device_internal_can_mode(cpu, channel_index) == CAN_MODE_LOOPBACK) {
        dspic33_device_internal_can_queue_push(&cpu->io.can_rx[channel_index], &transmitted_frame);
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_START, 0u);
    } else {
        dspic33_device_internal_can_queue_push(&cpu->io.can_tx[channel_index], &transmitted_frame);
    }
    cpu->io.can_tx_on_bus &= (uint8_t)~channel_bit;
    cpu->io.can_tx_busy &= (uint8_t)~channel_bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_START, 0u);
}

static void can_transmit_finish(Dspic33* cpu, uint8_t channel_index) {
    const Dspic33CanFrame transmitted_frame =
        dspic33_device_internal_can_decode_frame(cpu->io.can_tx_words[channel_index]);
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    cpu->io.can_tx_start_cycle[channel_index] = cpu->device_cycles;
    cpu->io.can_tx_phase_adjustment[channel_index] = 0;
    cpu->io.can_tx_on_bus |= channel_bit;
    if (!dspic33_schedule(
            cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_BUS_FINISH,
            dspic33_device_internal_can_frame_cycles(cpu, channel_index, &transmitted_frame)) ||
        !dspic33_device_internal_can_schedule_transmit_sample(
            cpu, channel_index,
            dspic33_device_internal_can_first_sample_delay(cpu, channel_index))) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel_index);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        cpu->io.can_tx_on_bus &= (uint8_t)~channel_bit;
        cpu->io.can_tx_busy &= (uint8_t)~channel_bit;
    }
}

static void can_transmit_sample(Dspic33* cpu, uint8_t channel_index, bool bus_level) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_tx_on_bus & channel_bit) == 0u) {
        return;
    }
    if ((cpu->io.can_rx_physical_active & channel_bit) != 0u) {
        dspic33_device_internal_can_monitor_transmit_sample(cpu, channel_index, bus_level);
    }
    uint64_t sample_delay_cycles = dspic33_device_internal_can_bit_cycles(cpu, channel_index);
    if (dspic33_device_internal_can_triple_sample(cpu, channel_index)) {
        sample_delay_cycles -= 2u * dspic33_device_internal_can_time_quantum(cpu, channel_index);
    }
    if ((cpu->io.can_tx_on_bus & channel_bit) != 0u &&
        !dspic33_device_internal_can_schedule_transmit_sample(cpu, channel_index,
                                                              sample_delay_cycles)) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel_index);
        cpu->io.can_tx_on_bus &= (uint8_t)~channel_bit;
        cpu->io.can_tx_busy &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_transmit_sample_first(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_tx_on_bus & channel_bit) == 0u) {
        return;
    }
    cpu->io.can_tx_sample_high[channel_index] =
        (cpu->io.can_rx_pin_high & channel_bit) != 0u ? 1u : 0u;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_SAMPLE_SECOND,
                          dspic33_device_internal_can_time_quantum(cpu, channel_index))) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel_index);
        cpu->io.can_tx_on_bus &= (uint8_t)~channel_bit;
        cpu->io.can_tx_busy &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_transmit_sample_second(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_tx_on_bus & channel_bit) == 0u) {
        return;
    }
    if ((cpu->io.can_rx_pin_high & channel_bit) != 0u) {
        cpu->io.can_tx_sample_high[channel_index]++;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_SAMPLE,
                          dspic33_device_internal_can_time_quantum(cpu, channel_index))) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel_index);
        cpu->io.can_tx_on_bus &= (uint8_t)~channel_bit;
        cpu->io.can_tx_busy &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_transmit_sample_final(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);
    uint8_t sample_count = cpu->io.can_tx_sample_high[channel_index];

    if ((cpu->io.can_rx_pin_high & channel_bit) != 0u) {
        sample_count++;
    }
    cpu->io.can_tx_sample_high[channel_index] = 0u;
    can_transmit_sample(cpu, channel_index, sample_count >= 2u);
}

static void can_transmit_retry(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    cpu->io.can_tx_error_active &= (uint8_t)~channel_bit;
    cpu->io.can_tx_retry_wait &= (uint8_t)~channel_bit;
    can_transmit_start(cpu, channel_index);
}

static void can_transmit_error_start(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_tx_retry_wait & channel_bit) == 0u) {
        return;
    }
    cpu->io.can_tx_error_active |= channel_bit;
    cpu->io.can_tx_error_start_cycle[channel_index] = cpu->device_cycles;
    const uint64_t error_frame_bits =
        (dspic33_device_internal_raw_word(
             cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0au)) &
         CAN_TRANSMIT_PASSIVE) != 0u
            ? 25u
            : 17u;

    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_RETRY,
                          error_frame_bits *
                              dspic33_device_internal_can_bit_cycles(cpu, channel_index))) {
        cpu->io.can_tx_error_active &= (uint8_t)~channel_bit;
        cpu->io.can_tx_retry_wait &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_can_error_event(Dspic33* cpu, uint8_t channel_index,
                                             uint32_t event_value) {
    const uint16_t counter_address = (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0eu);
    uint16_t error_counters = dspic33_device_internal_raw_word(cpu, counter_address);
    const uint16_t error_increment = (uint16_t)(event_value >> CAN_EVENT_ERROR_COUNT_SHIFT);

    if ((event_value & CAN_EVENT_TRANSMIT_ERROR) != 0u) {
        uint16_t transmit_error_count = (uint16_t)(error_counters >> 8u);
        const uint32_t transmit_total = (uint32_t)transmit_error_count + error_increment;

        if (transmit_total > 0xffu) {
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0au),
                (uint16_t)(dspic33_device_internal_raw_word(
                               cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0au)) |
                           CAN_BUS_OFF));
            transmit_error_count = 0xffu;
        } else {
            transmit_error_count = (uint16_t)transmit_total;
        }
        error_counters = (uint16_t)((error_counters & 0x00ffu) | (transmit_error_count << 8u));
    } else {
        uint16_t receive_error_count = (uint16_t)(error_counters & 0x00ffu);
        receive_error_count = receive_error_count + error_increment > 0xffu
                                  ? 0xffu
                                  : receive_error_count + error_increment;
        error_counters = (uint16_t)((error_counters & 0xff00u) | receive_error_count);
    }
    dspic33_device_internal_raw_write_word(cpu, counter_address, error_counters);
    dspic33_device_internal_can_refresh_error_status(cpu, channel_index);
}

void dspic33_device_internal_can_invalid_event(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t current_mode = dspic33_device_internal_can_mode(cpu, channel_index);

    if (!dspic33_device_internal_can_power_enabled(cpu, channel_index) ||
        cpu->power_state == DSPIC33_POWER_SLEEP || current_mode == CAN_MODE_DISABLE ||
        current_mode == CAN_MODE_CONFIGURATION) {
        return;
    }
    dspic33_device_internal_can_raise_event(cpu, channel_index, CAN_INTERRUPT_INVALID, 0u, 0u);
}

void dspic33_device_internal_run_can(Dspic33* cpu, uint8_t channel_index, uint32_t event_value) {
    const uint32_t event_kind = event_value & CAN_EVENT_KIND_MASK;

    if (event_kind == CAN_EVENT_RECEIVE_PIN) {
        dspic33_device_internal_apply_physical_pin_level(cpu, channel_index,
                                                         (event_value & CAN_EVENT_PIN_HIGH) != 0u);
        return;
    }
    if (channel_index >= DSPIC33_CAN_COUNT) {
        return;
    }
    switch (event_kind) {
    case CAN_EVENT_RECEIVE_START:
        can_receive_start(cpu, channel_index);
        break;
    case CAN_EVENT_RECEIVE_WORD:
        can_receive_word(cpu, channel_index);
        break;
    case CAN_EVENT_RECEIVE_FINISH:
        can_receive_finish(cpu, channel_index);
        break;

    case CAN_EVENT_TRANSMIT_START:
        can_transmit_start(cpu, channel_index);
        break;
    case CAN_EVENT_TRANSMIT_WORD:
        can_transmit_word(cpu, channel_index);
        break;
    case CAN_EVENT_TRANSMIT_FINISH:
        can_transmit_finish(cpu, channel_index);
        break;
    case CAN_EVENT_TRANSMIT_BUS_FINISH:
        can_transmit_bus_finish(cpu, channel_index);
        break;

    case CAN_EVENT_CAPTURE_RELEASE:
        dspic33_device_internal_input_capture_level(cpu, 1u, false);
        break;
    case CAN_EVENT_INVALID:
        dspic33_device_internal_can_invalid_event(cpu, channel_index);
        break;

    case CAN_EVENT_RECEIVE_SAMPLE:
        if (dspic33_device_internal_can_triple_sample(cpu, channel_index)) {
            can_receive_sample_final(cpu, channel_index);
        } else {
            can_receive_sample(cpu, channel_index,
                               (cpu->io.can_rx_pin_high & (uint8_t)(1u << channel_index)) != 0u);
        }
        break;
    case CAN_EVENT_RECEIVE_SAMPLE_FIRST:
        can_receive_sample_first(cpu, channel_index);
        break;
    case CAN_EVENT_RECEIVE_SAMPLE_SECOND:
        can_receive_sample_second(cpu, channel_index);
        break;

    case CAN_EVENT_ACK_START:
        can_ack_start(cpu, channel_index);
        break;
    case CAN_EVENT_ACK_FINISH:
        can_ack_finish(cpu, channel_index);
        break;

    case CAN_EVENT_TRANSMIT_RETRY:
        can_transmit_retry(cpu, channel_index);
        break;
    case CAN_EVENT_TRANSMIT_ERROR_START:
        can_transmit_error_start(cpu, channel_index);
        break;
    case CAN_EVENT_TRANSMIT_SAMPLE:
        if (dspic33_device_internal_can_triple_sample(cpu, channel_index)) {
            can_transmit_sample_final(cpu, channel_index);
        } else {
            can_transmit_sample(cpu, channel_index,
                                (cpu->io.can_rx_pin_high & (uint8_t)(1u << channel_index)) != 0u);
        }
        break;
    case CAN_EVENT_TRANSMIT_SAMPLE_FIRST:
        can_transmit_sample_first(cpu, channel_index);
        break;
    case CAN_EVENT_TRANSMIT_SAMPLE_SECOND:
        can_transmit_sample_second(cpu, channel_index);
        break;

    case CAN_EVENT_RECEIVE_ERROR_START:
        can_receive_error_start(cpu, channel_index);
        break;
    case CAN_EVENT_RECEIVE_ERROR_FINISH:
        can_receive_error_finish(cpu, channel_index);
        break;

    case CAN_EVENT_MODE_TRANSITION:
        can_mode_transition(cpu, channel_index, event_value);
        break;
    case CAN_EVENT_INTERMISSION_FINISH:
        dspic33_device_internal_can_intermission_finish(cpu, channel_index, event_value);
        break;
    case CAN_EVENT_OVERLOAD_FINISH:
        dspic33_device_internal_can_overload_finish(cpu, channel_index, event_value);
        break;

    case CAN_EVENT_RECEIVE_SUCCESS:
        can_receive_success_start(cpu, channel_index);
        break;
    case CAN_EVENT_ERROR:
        dspic33_device_internal_can_error_event(cpu, channel_index, event_value);
        break;
    }
}
