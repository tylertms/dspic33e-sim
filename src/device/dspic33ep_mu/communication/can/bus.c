#include "device/dspic33ep_mu/internal.h"

static void can_receive_sample(Dspic33* cpu, uint8_t channel_index, bool bus_level) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);
    Dspic33CanFrame received_frame;
    Dspic33CanSerialResult decode_result;
    uint16_t bit_count;
    uint16_t stuffing_tail_index;

    if ((cpu->io.can_rx_serial_active & channel_bit) == 0u ||
        !dspic33_device_internal_can_serial_receive_enabled(cpu, channel_index)) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_bit;
        return;
    }
    bit_count = cpu->io.can_rx_serial_count[channel_index];
    if (bit_count >= sizeof(cpu->io.can_rx_serial_bits[channel_index])) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_bit;
        dspic33_device_internal_can_invalid_event(cpu, channel_index);
        return;
    }
    cpu->io.can_rx_serial_bits[channel_index][bit_count] = bus_level;
    cpu->io.can_rx_serial_count[channel_index]++;
    decode_result = dspic33_device_internal_can_decode_serial(cpu, channel_index, &received_frame,
                                                              &stuffing_tail_index);
    if (decode_result == CAN_SERIAL_INCOMPLETE && stuffing_tail_index != 0u &&
        cpu->io.can_rx_serial_count[channel_index] == stuffing_tail_index + 1u &&
        cpu->io.can_rx_serial_bits[channel_index][stuffing_tail_index] != 0u &&
        (cpu->io.can_tx_on_bus & channel_bit) == 0u &&
        dspic33_device_internal_can_mode(cpu, channel_index) != CAN_MODE_LISTEN) {
        const uint64_t bit_cycles = dspic33_device_internal_can_bit_cycles(cpu, channel_index);
        const uint64_t sample_cycles =
            dspic33_device_internal_can_sample_cycles(cpu, channel_index);
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_ACK_START,
                              bit_cycles - sample_cycles)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            cpu->io.can_rx_serial_active &= (uint8_t)~channel_bit;
            return;
        }
    }
    if (decode_result == CAN_SERIAL_VALID) {
        const bool overload = !cpu->io.can_rx_serial_bits[channel_index][stuffing_tail_index + 9u];
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_bit;
        cpu->io.can_intermission_generation[channel_index]++;
        cpu->io.can_overload_count[channel_index] = 0u;
        if (!dspic33_device_internal_can_schedule_intermission(cpu, channel_index)) {
            cpu->io.can_intermission_active &= (uint8_t)~channel_bit;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
        if ((cpu->io.can_tx_on_bus & channel_bit) != 0u) {
            return;
        }
        dspic33_device_internal_can_receive_success(cpu, channel_index);
        if (!dspic33_device_internal_can_queue_push(&cpu->io.can_rx[channel_index],
                                                    &received_frame) ||
            !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_START, 0u) ||
            (((cpu->io.can_tx_retry_wait & channel_bit) != 0u) &&
             !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_RETRY,
                               3u * dspic33_device_internal_can_bit_cycles(cpu, channel_index)))) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        if (overload && dspic33_device_internal_can_mode(cpu, channel_index) != CAN_MODE_LISTEN) {
            dspic33_device_internal_can_start_overload(cpu, channel_index);
        }
        return;
    }
    if (decode_result == CAN_SERIAL_INVALID) {
        dspic33_device_internal_can_receive_error(cpu, channel_index, &received_frame);
        return;
    }
    uint64_t sample_delay_cycles = dspic33_device_internal_can_bit_cycles(cpu, channel_index);
    if (dspic33_device_internal_can_triple_sample(cpu, channel_index)) {
        sample_delay_cycles -= 2u * dspic33_device_internal_can_time_quantum(cpu, channel_index);
    }
    if (!dspic33_device_internal_can_schedule_receive_sample(cpu, channel_index,
                                                             sample_delay_cycles)) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_first(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_rx_serial_active & channel_bit) == 0u) {
        return;
    }
    cpu->io.can_rx_sample_high[channel_index] =
        (cpu->io.can_rx_pin_high & channel_bit) != 0u ? 1u : 0u;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_SAMPLE_SECOND,
                          dspic33_device_internal_can_time_quantum(cpu, channel_index))) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_second(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if ((cpu->io.can_rx_serial_active & channel_bit) == 0u) {
        return;
    }
    if ((cpu->io.can_rx_pin_high & channel_bit) != 0u) {
        cpu->io.can_rx_sample_high[channel_index]++;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_SAMPLE,
                          dspic33_device_internal_can_time_quantum(cpu, channel_index))) {
        cpu->io.can_rx_serial_active &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_final(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);
    uint8_t sample_count = cpu->io.can_rx_sample_high[channel_index];

    if ((cpu->io.can_rx_pin_high & channel_bit) != 0u) {
        sample_count++;
    }
    cpu->io.can_rx_sample_high[channel_index] = 0u;
    can_receive_sample(cpu, channel_index, sample_count >= 2u);
}

static void can_ack_start(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    if (!dspic33_device_internal_can_serial_receive_enabled(cpu, channel_index) ||
        dspic33_device_internal_can_mode(cpu, channel_index) == CAN_MODE_LISTEN) {
        return;
    }
    cpu->io.can_rx_ack |= channel_bit;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_ACK_FINISH,
                          dspic33_device_internal_can_bit_cycles(cpu, channel_index))) {
        cpu->io.can_rx_ack &= (uint8_t)~channel_bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_ack_finish(Dspic33* cpu, uint8_t channel_index) {
    cpu->io.can_rx_ack &= (uint8_t)~(uint8_t)(1u << channel_index);
}

static void can_receive_error_start(Dspic33* cpu, uint8_t channel_index) {
    const uint8_t channel_bit = (uint8_t)(1u << channel_index);

    cpu->io.can_rx_error_active |= channel_bit;
    cpu->io.can_rx_error_start_cycle[channel_index] = cpu->device_cycles;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_RECEIVE_ERROR_FINISH,
                          14u * dspic33_device_internal_can_bit_cycles(cpu, channel_index))) {
        cpu->io.can_rx_error_active &= (uint8_t)~channel_bit;
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

static void can_transmit_start(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    int selected;
    if ((cpu->io.can_tx_busy & bit) != 0u || (cpu->io.can_tx_retry_wait & bit) != 0u ||
        !dspic33_device_internal_can_power_enabled(cpu, channel) ||
        (dspic33_device_internal_can_mode(cpu, channel) != CAN_MODE_NORMAL &&
         dspic33_device_internal_can_mode(cpu, channel) != CAN_MODE_LOOPBACK) ||
        (dspic33_device_internal_raw_word(cpu,
                                          (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) &
         CAN_BUS_OFF) != 0u) {
        return;
    }
    selected = can_transmit_selection(cpu, channel);
    if (selected < 0) {
        return;
    }
    if (!dspic33_device_internal_can_dma_ready(
            cpu, dspic33_device_can_tx_requests[channel],
            (uint16_t)(dspic33_device_can_bases[channel] + 0x42u), true)) {
        dspic33_raise_interrupt(cpu, dspic33_device_can_tx_irqs[channel]);
        return;
    }
    cpu->io.can_tx_buffer[channel] = (uint8_t)selected;
    cpu->io.can_tx_word[channel] = 0u;
    memset(cpu->io.can_tx_words[channel], 0, sizeof(cpu->io.can_tx_words[channel]));
    cpu->io.can_tx_busy |= bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_WORD, 0u);
}

static void can_transmit_word(Dspic33* cpu, uint8_t channel) {
    uint8_t word = cpu->io.can_tx_word[channel];
    if (word >= 8u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_FINISH, 0u);
        return;
    }
    cpu->io.can_tx_word[channel]++;
    dspic33_raise_interrupt(cpu, dspic33_device_can_tx_irqs[channel]);
    dspic33_dma_request(cpu, dspic33_device_can_tx_requests[channel],
                        (uint16_t)(cpu->io.can_tx_buffer[channel] * 16u + word * 2u), 0u);
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_WORD, 1u);
}

static void can_transmit_bus_finish(Dspic33* cpu, uint8_t channel) {
    uint8_t buffer = cpu->io.can_tx_buffer[channel];
    uint8_t bit = (uint8_t)(1u << channel);
    uint16_t control = dspic33_device_internal_can_buffer_control(cpu, channel, buffer);
    uint16_t counts = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0eu));
    Dspic33CanFrame frame = dspic33_device_internal_can_decode_frame(cpu->io.can_tx_words[channel]);
    dspic33_device_internal_can_set_buffer_control(cpu, channel, buffer,
                                                   (uint16_t)(control & ~CAN_BUFFER_REQUEST));
    if ((counts >> 8u) != 0u) {
        counts = (uint16_t)(counts - 0x0100u);
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0eu), counts);
        dspic33_device_internal_can_refresh_error_status(cpu, channel);
    }
    dspic33_device_internal_can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT, buffer, 0u);
    if (dspic33_device_internal_can_mode(cpu, channel) == CAN_MODE_LOOPBACK) {
        dspic33_device_internal_can_queue_push(&cpu->io.can_rx[channel], &frame);
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u);
    } else {
        dspic33_device_internal_can_queue_push(&cpu->io.can_tx[channel], &frame);
    }
    cpu->io.can_tx_on_bus &= (uint8_t)~bit;
    cpu->io.can_tx_busy &= (uint8_t)~bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
}

static void can_transmit_finish(Dspic33* cpu, uint8_t channel) {
    Dspic33CanFrame frame = dspic33_device_internal_can_decode_frame(cpu->io.can_tx_words[channel]);
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.can_tx_start_cycle[channel] = cpu->device_cycles;
    cpu->io.can_tx_phase_adjustment[channel] = 0;
    cpu->io.can_tx_on_bus |= bit;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_BUS_FINISH,
                          dspic33_device_internal_can_frame_cycles(cpu, channel, &frame)) ||
        !dspic33_device_internal_can_schedule_transmit_sample(
            cpu, channel, dspic33_device_internal_can_first_sample_delay(cpu, channel))) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        cpu->io.can_tx_on_bus &= (uint8_t)~bit;
        cpu->io.can_tx_busy &= (uint8_t)~(uint8_t)(1u << channel);
    }
}

static void can_transmit_sample(Dspic33* cpu, uint8_t channel, bool bus_high) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_tx_on_bus & bit) == 0u) {
        return;
    }
    if ((cpu->io.can_rx_physical_active & bit) != 0u) {
        dspic33_device_internal_can_monitor_transmit_sample(cpu, channel, bus_high);
    }
    uint64_t delay = dspic33_device_internal_can_bit_cycles(cpu, channel);
    if (dspic33_device_internal_can_triple_sample(cpu, channel)) {
        delay -= 2u * dspic33_device_internal_can_time_quantum(cpu, channel);
    }
    if ((cpu->io.can_tx_on_bus & bit) != 0u &&
        !dspic33_device_internal_can_schedule_transmit_sample(cpu, channel, delay)) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel);
        cpu->io.can_tx_on_bus &= (uint8_t)~bit;
        cpu->io.can_tx_busy &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_transmit_sample_first(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_tx_on_bus & bit) == 0u) {
        return;
    }
    cpu->io.can_tx_sample_high[channel] = (cpu->io.can_rx_pin_high & bit) != 0u ? 1u : 0u;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_SAMPLE_SECOND,
                          dspic33_device_internal_can_time_quantum(cpu, channel))) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel);
        cpu->io.can_tx_on_bus &= (uint8_t)~bit;
        cpu->io.can_tx_busy &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_transmit_sample_second(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_tx_on_bus & bit) == 0u) {
        return;
    }
    if ((cpu->io.can_rx_pin_high & bit) != 0u) {
        cpu->io.can_tx_sample_high[channel]++;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_SAMPLE,
                          dspic33_device_internal_can_time_quantum(cpu, channel))) {
        dspic33_device_internal_can_remove_transmit_events(cpu, channel);
        cpu->io.can_tx_on_bus &= (uint8_t)~bit;
        cpu->io.can_tx_busy &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_transmit_sample_final(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t high = cpu->io.can_tx_sample_high[channel];
    if ((cpu->io.can_rx_pin_high & bit) != 0u) {
        high++;
    }
    cpu->io.can_tx_sample_high[channel] = 0u;
    can_transmit_sample(cpu, channel, high >= 2u);
}

static void can_transmit_retry(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.can_tx_error_active &= (uint8_t)~bit;
    cpu->io.can_tx_retry_wait &= (uint8_t)~bit;
    can_transmit_start(cpu, channel);
}

static void can_transmit_error_start(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_tx_retry_wait & bit) == 0u) {
        return;
    }
    cpu->io.can_tx_error_active |= bit;
    cpu->io.can_tx_error_start_cycle[channel] = cpu->device_cycles;
    uint64_t bits = (dspic33_device_internal_raw_word(
                         cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) &
                     CAN_TRANSMIT_PASSIVE) != 0u
                        ? 25u
                        : 17u;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_RETRY,
                          bits * dspic33_device_internal_can_bit_cycles(cpu, channel))) {
        cpu->io.can_tx_error_active &= (uint8_t)~bit;
        cpu->io.can_tx_retry_wait &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_can_error_event(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint16_t address = (uint16_t)(dspic33_device_can_bases[channel] + 0x0eu);
    uint16_t counts = dspic33_device_internal_raw_word(cpu, address);
    uint16_t increment = (uint16_t)(value >> CAN_EVENT_ERROR_COUNT_SHIFT);
    if ((value & CAN_EVENT_TRANSMIT_ERROR) != 0u) {
        uint16_t transmit = (uint16_t)(counts >> 8u);
        uint32_t total = (uint32_t)transmit + increment;
        if (total > 0xffu) {
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0au),
                (uint16_t)(dspic33_device_internal_raw_word(
                               cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) |
                           CAN_BUS_OFF));
            transmit = 0xffu;
        } else {
            transmit = (uint16_t)total;
        }
        counts = (uint16_t)((counts & 0x00ffu) | (transmit << 8u));
    } else {
        uint16_t receive = (uint16_t)(counts & 0x00ffu);
        receive = receive + increment > 0xffu ? 0xffu : receive + increment;
        counts = (uint16_t)((counts & 0xff00u) | receive);
    }
    dspic33_device_internal_raw_write_word(cpu, address, counts);
    dspic33_device_internal_can_refresh_error_status(cpu, channel);
}

void dspic33_device_internal_can_invalid_event(Dspic33* cpu, uint8_t channel) {
    uint8_t mode = dspic33_device_internal_can_mode(cpu, channel);
    if (!dspic33_device_internal_can_power_enabled(cpu, channel) ||
        cpu->power_state == DSPIC33_POWER_SLEEP || mode == CAN_MODE_DISABLE ||
        mode == CAN_MODE_CONFIGURATION) {
        return;
    }
    dspic33_device_internal_can_raise_event(cpu, channel, CAN_INTERRUPT_INVALID, 0u, 0u);
}

void dspic33_device_internal_run_can(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint32_t kind = value & CAN_EVENT_KIND_MASK;
    if (kind == CAN_EVENT_RECEIVE_PIN) {
        dspic33_device_internal_apply_physical_pin_level(cpu, channel,
                                                         (value & CAN_EVENT_PIN_HIGH) != 0u);
        return;
    }
    if (channel >= DSPIC33_CAN_COUNT) {
        return;
    }
    switch (kind) {
    case CAN_EVENT_RECEIVE_START:
        can_receive_start(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_WORD:
        can_receive_word(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_FINISH:
        can_receive_finish(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_START:
        can_transmit_start(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_WORD:
        can_transmit_word(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_FINISH:
        can_transmit_finish(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_BUS_FINISH:
        can_transmit_bus_finish(cpu, channel);
        break;
    case CAN_EVENT_CAPTURE_RELEASE:
        dspic33_device_internal_input_capture_level(cpu, 1u, false);
        break;
    case CAN_EVENT_INVALID:
        dspic33_device_internal_can_invalid_event(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_SAMPLE:
        if (dspic33_device_internal_can_triple_sample(cpu, channel)) {
            can_receive_sample_final(cpu, channel);
        } else {
            can_receive_sample(cpu, channel,
                               (cpu->io.can_rx_pin_high & (uint8_t)(1u << channel)) != 0u);
        }
        break;
    case CAN_EVENT_RECEIVE_SAMPLE_FIRST:
        can_receive_sample_first(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_SAMPLE_SECOND:
        can_receive_sample_second(cpu, channel);
        break;
    case CAN_EVENT_ACK_START:
        can_ack_start(cpu, channel);
        break;
    case CAN_EVENT_ACK_FINISH:
        can_ack_finish(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_RETRY:
        can_transmit_retry(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_ERROR_START:
        can_transmit_error_start(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_SAMPLE:
        if (dspic33_device_internal_can_triple_sample(cpu, channel)) {
            can_transmit_sample_final(cpu, channel);
        } else {
            can_transmit_sample(cpu, channel,
                                (cpu->io.can_rx_pin_high & (uint8_t)(1u << channel)) != 0u);
        }
        break;
    case CAN_EVENT_TRANSMIT_SAMPLE_FIRST:
        can_transmit_sample_first(cpu, channel);
        break;
    case CAN_EVENT_TRANSMIT_SAMPLE_SECOND:
        can_transmit_sample_second(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_ERROR_START:
        can_receive_error_start(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_ERROR_FINISH:
        can_receive_error_finish(cpu, channel);
        break;
    case CAN_EVENT_MODE_TRANSITION:
        can_mode_transition(cpu, channel, value);
        break;
    case CAN_EVENT_INTERMISSION_FINISH:
        dspic33_device_internal_can_intermission_finish(cpu, channel, value);
        break;
    case CAN_EVENT_OVERLOAD_FINISH:
        dspic33_device_internal_can_overload_finish(cpu, channel, value);
        break;
    case CAN_EVENT_RECEIVE_SUCCESS:
        can_receive_success_start(cpu, channel);
        break;
    case CAN_EVENT_ERROR:
        dspic33_device_internal_can_error_event(cpu, channel, value);
        break;
    }
}
