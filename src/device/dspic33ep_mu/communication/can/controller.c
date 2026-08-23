#include "device/dspic33ep_mu/internal.h"

uint16_t dspic33_device_internal_can_filter_word(const Dspic33* cpu, uint8_t channel_index,
                                                 uint16_t register_offset) {
    return cpu->io.can_filter_window[channel_index][(register_offset - 0x20u) / 2u];
}

uint8_t dspic33_device_internal_can_mode(const Dspic33* cpu, uint8_t channel_index) {
    return (
        uint8_t)((dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel_index]) >>
                  5u) &
                 7u);
}

bool dspic33_device_internal_can_power_enabled(const Dspic33* cpu, uint8_t channel_index) {
    const uint16_t can_control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel_index]);

    if ((dspic33_device_internal_raw_word(cpu, 0x0760u) & (uint16_t)(2u << channel_index)) != 0u) {
        return false;
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    return cpu->power_state == DSPIC33_POWER_IDLE && (can_control & CAN_STOP_IDLE) == 0u;
}

uint8_t dspic33_device_internal_can_buffer_count(const Dspic33* cpu, uint8_t channel_index) {
    static const uint8_t buffer_counts[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u, 32u};
    const uint16_t can_control = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 6u));

    return buffer_counts[(can_control >> 13u) & 7u];
}

uint16_t dspic33_device_internal_can_buffer_control(const Dspic33* cpu, uint8_t channel_index,
                                                    uint8_t buffer_index) {
    const uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_can_bases[channel_index] +
                                                         0x30u + (buffer_index / 2u) * 2u));

    return (uint16_t)(control_word >> ((buffer_index & 1u) * 8u));
}

void dspic33_device_internal_can_set_buffer_control(Dspic33* cpu, uint8_t channel_index,
                                                    uint8_t buffer_index,
                                                    uint16_t requested_value) {
    const uint16_t register_address =
        (uint16_t)(dspic33_device_can_bases[channel_index] + 0x30u + (buffer_index / 2u) * 2u);
    const uint8_t byte_shift = (uint8_t)((buffer_index & 1u) * 8u);
    uint16_t control_word = dspic33_device_internal_raw_word(cpu, register_address);

    control_word = (uint16_t)((control_word & ~(uint16_t)(0xffu << byte_shift)) |
                              ((requested_value & 0xffu) << byte_shift));
    dspic33_device_internal_raw_write_word(cpu, register_address, control_word);
}

static uint16_t can_buffer_flag_address(uint8_t channel_index, uint8_t buffer_index,
                                        bool overflow) {
    return (uint16_t)(dspic33_device_can_bases[channel_index] + (overflow ? 0x28u : 0x20u) +
                      (buffer_index >= 16u ? 2u : 0u));
}

bool dspic33_device_internal_can_buffer_flag(const Dspic33* cpu, uint8_t channel_index,
                                             uint8_t buffer_index, bool overflow) {
    const uint16_t register_address =
        can_buffer_flag_address(channel_index, buffer_index, overflow);

    return (dspic33_device_internal_raw_word(cpu, register_address) &
            (uint16_t)(1u << (buffer_index & 15u))) != 0u;
}

void dspic33_device_internal_can_set_buffer_flag(Dspic33* cpu, uint8_t channel_index,
                                                 uint8_t buffer_index, bool overflow) {
    const uint16_t register_address =
        can_buffer_flag_address(channel_index, buffer_index, overflow);

    dspic33_device_internal_raw_write_word(
        cpu, register_address,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, register_address) |
                   (uint16_t)(1u << (buffer_index & 15u))));
}

void dspic33_device_internal_can_update_vector(Dspic33* cpu, uint8_t channel_index) {
    const uint16_t can_base = dspic33_device_can_bases[channel_index];
    const uint16_t active_flags =
        (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(can_base + 0x0au)) &
                   dspic33_device_internal_raw_word(cpu, (uint16_t)(can_base + 0x0cu)));
    uint8_t vector_code = 0x40u;

    if ((active_flags & (CAN_INTERRUPT_TRANSMIT | CAN_INTERRUPT_RECEIVE)) != 0u) {
        vector_code = cpu->io.can_last_buffer[channel_index];
    } else if ((active_flags & CAN_INTERRUPT_ERROR) != 0u) {
        vector_code = 0x41u;
    } else if ((active_flags & CAN_INTERRUPT_WAKE) != 0u) {
        vector_code = 0x42u;
    } else if ((active_flags & CAN_INTERRUPT_OVERFLOW) != 0u) {
        vector_code = 0x43u;
    } else if ((active_flags & CAN_INTERRUPT_FIFO) != 0u) {
        vector_code = 0x44u;
    }
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(can_base + 4u),
        (uint16_t)(((uint16_t)cpu->io.can_last_filter[channel_index] << 8u) | vector_code));
    if (active_flags != 0u) {
        dspic33_raise_interrupt(cpu, dspic33_device_can_event_irqs[channel_index]);
    }
}

void dspic33_device_internal_can_raise_event(Dspic33* cpu, uint8_t channel_index,
                                             uint16_t interrupt_flag, uint8_t buffer_index,
                                             uint8_t filter_index) {
    const uint16_t status_address = (uint16_t)(dspic33_device_can_bases[channel_index] + 0x0au);

    dspic33_device_internal_raw_write_word(
        cpu, status_address,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, status_address) | interrupt_flag));
    cpu->io.can_last_buffer[channel_index] = buffer_index;
    cpu->io.can_last_filter[channel_index] = filter_index;
    dspic33_device_internal_can_update_vector(cpu, channel_index);
}

bool dspic33_device_internal_can_dma_ready(const Dspic33* cpu, uint8_t dma_request,
                                           uint16_t peripheral_address, bool transmit_direction) {
    for (uint8_t dma_channel_index = 0u; dma_channel_index < DSPIC33_DMA_COUNT;
         dma_channel_index++) {
        const uint16_t dma_base = dspic33_device_internal_dma_channel_base(dma_channel_index);
        const uint16_t dma_control = dspic33_device_internal_raw_word(cpu, dma_base);

        if ((dma_control & DMA_CON_CHEN) != 0u &&
            (dspic33_device_internal_raw_word(cpu, (uint16_t)(dma_base + 2u)) &
             DMA_REQ_SOURCE_MASK) == dma_request &&
            dspic33_device_internal_raw_word(cpu, (uint16_t)(dma_base + 0x0cu)) ==
                peripheral_address &&
            (dma_control & DMA_CON_SIZE_BYTE) == 0u &&
            (dma_control & DMA_CON_AMODE_MASK) == DMA_CON_AMODE_PERIPHERAL &&
            ((dma_control & DMA_CON_RAM_TO_PERIPHERAL) != 0u) == transmit_direction) {
            return true;
        }
    }
    return false;
}

static uint32_t can_identifier_sid(const Dspic33CanFrame* frame) {
    return frame->extended ? (frame->identifier >> 18u) & 0x7ffu : frame->identifier & 0x7ffu;
}

static uint32_t can_identifier_eid(const Dspic33CanFrame* frame) {
    return frame->identifier & 0x3ffffu;
}

static bool can_devicenet_match(const Dspic33CanFrame* frame, uint32_t expected_value,
                                uint8_t requested_bits) {
    uint32_t payload_value = 0u;
    const uint8_t available_bits = (uint8_t)(frame->length * 8u);

    if (requested_bits > 18u) {
        requested_bits = 18u;
    }
    if (requested_bits > available_bits) {
        requested_bits = available_bits;
    }
    if (requested_bits == 0u) {
        return true;
    }
    payload_value = (uint32_t)frame->data[0] << 16u;
    if (frame->length > 1u) {
        payload_value |= (uint32_t)frame->data[1] << 8u;
    }
    if (frame->length > 2u) {
        payload_value |= frame->data[2];
    }
    return (payload_value >> (24u - requested_bits)) == (expected_value >> (18u - requested_bits));
}

static bool can_filter_matches(const Dspic33* cpu, uint8_t channel_index, uint8_t filter_index,
                               const Dspic33CanFrame* frame) {
    const uint16_t filter_standard_id = dspic33_device_internal_can_filter_word(
        cpu, channel_index, (uint16_t)(0x40u + filter_index * 4u));
    const uint16_t filter_extended_id = dspic33_device_internal_can_filter_word(
        cpu, channel_index, (uint16_t)(0x42u + filter_index * 4u));
    const uint16_t mask_selection =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(dspic33_device_can_bases[channel_index] +
                                                         (filter_index < 8u ? 0x18u : 0x1au)));
    const uint8_t mask_index = (uint8_t)((mask_selection >> ((filter_index & 7u) * 2u)) & 3u);
    uint16_t mask_standard_id;
    uint16_t mask_extended_id;
    const uint32_t standard_id = can_identifier_sid(frame);
    const uint32_t extended_id = can_identifier_eid(frame);
    const uint8_t devicenet_bits =
        (uint8_t)(dspic33_device_internal_raw_word(
                      cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 2u)) &
                  0x001fu);

    if (mask_index >= 3u) {
        return false;
    }
    mask_standard_id = dspic33_device_internal_can_filter_word(cpu, channel_index,
                                                               (uint16_t)(0x30u + mask_index * 4u));
    mask_extended_id = dspic33_device_internal_can_filter_word(cpu, channel_index,
                                                               (uint16_t)(0x32u + mask_index * 4u));
    if ((mask_standard_id & 0x0008u) != 0u &&
        frame->extended != ((filter_standard_id & 0x0008u) != 0u)) {
        return false;
    }
    if ((((standard_id << 5u) ^ filter_standard_id) & mask_standard_id & 0xffe0u) != 0u) {
        return false;
    }
    if (!frame->extended && devicenet_bits != 0u && (mask_standard_id & 0x0008u) != 0u &&
        (filter_standard_id & 0x0008u) == 0u) {
        const uint32_t expected_value =
            ((uint32_t)(filter_standard_id & 3u) << 16u) | filter_extended_id;
        return can_devicenet_match(frame, expected_value, devicenet_bits);
    }
    if (frame->extended) {
        const uint32_t expected_value =
            ((uint32_t)(filter_standard_id & 3u) << 16u) | filter_extended_id;
        const uint32_t identifier_mask =
            ((uint32_t)(mask_standard_id & 3u) << 16u) | mask_extended_id;
        return ((extended_id ^ expected_value) & identifier_mask) == 0u;
    }
    return true;
}

uint8_t dspic33_device_internal_can_filter_buffer(const Dspic33* cpu, uint8_t channel_index,
                                                  uint8_t filter_index) {
    const uint16_t filter_word = dspic33_device_internal_can_filter_word(
        cpu, channel_index, (uint16_t)(0x20u + (filter_index / 4u) * 2u));

    return (uint8_t)((filter_word >> ((filter_index & 3u) * 4u)) & 0x0fu);
}

uint8_t dspic33_device_internal_can_fifo_end(const Dspic33* cpu, uint8_t channel_index) {
    return (uint8_t)(dspic33_device_internal_can_buffer_count(cpu, channel_index) - 1u);
}

uint8_t dspic33_device_internal_can_next_fifo_buffer(const Dspic33* cpu, uint8_t channel_index,
                                                     uint8_t buffer_index) {
    const uint8_t fifo_start_buffer =
        (uint8_t)(dspic33_device_internal_raw_word(
                      cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 6u)) &
                  0x001fu);

    return buffer_index >= dspic33_device_internal_can_fifo_end(cpu, channel_index)
               ? fifo_start_buffer
               : (uint8_t)(buffer_index + 1u);
}

uint8_t dspic33_device_internal_can_advance_fifo_write(Dspic33* cpu, uint8_t channel_index,
                                                       uint8_t buffer_index) {
    const uint8_t next_buffer =
        dspic33_device_internal_can_next_fifo_buffer(cpu, channel_index, buffer_index);
    const uint16_t register_address = (uint16_t)(dspic33_device_can_bases[channel_index] + 8u);
    const uint16_t fifo_state = dspic33_device_internal_raw_word(cpu, register_address);

    cpu->io.can_fifo_write[channel_index] = next_buffer;
    dspic33_device_internal_raw_write_word(
        cpu, register_address, (uint16_t)((fifo_state & 0x003fu) | ((uint16_t)next_buffer << 8u)));
    return next_buffer;
}

bool dspic33_device_internal_can_select_receive_buffer(Dspic33* cpu, uint8_t channel_index,
                                                       const Dspic33CanFrame* received_frame,
                                                       uint8_t* selected_buffer,
                                                       uint8_t* matched_filter_index) {
    const uint16_t enabled_filters = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel_index] + 0x14u));
    uint8_t first_receive_buffer = 0u;
    uint8_t first_filter_index = 0u;
    bool first_match_is_fifo = false;
    bool filter_match_found = false;

    for (uint8_t filter_index = 0u; filter_index < 16u; filter_index++) {
        bool is_fifo_buffer;
        uint8_t target_buffer;
        uint16_t buffer_control;

        if ((enabled_filters & (uint16_t)(1u << filter_index)) == 0u ||
            !can_filter_matches(cpu, channel_index, filter_index, received_frame)) {
            continue;
        }
        target_buffer = dspic33_device_internal_can_filter_buffer(cpu, channel_index, filter_index);
        is_fifo_buffer = target_buffer == 15u;
        if (is_fifo_buffer) {
            target_buffer = cpu->io.can_fifo_write[channel_index];
        }
        if (!filter_match_found) {
            filter_match_found = true;
            first_receive_buffer = target_buffer;
            first_filter_index = filter_index;
            first_match_is_fifo = is_fifo_buffer;
        }
        if (target_buffer >= dspic33_device_internal_can_buffer_count(cpu, channel_index) ||
            target_buffer > 31u) {
            continue;
        }
        buffer_control =
            target_buffer < 8u
                ? dspic33_device_internal_can_buffer_control(cpu, channel_index, target_buffer)
                : 0u;
        if (target_buffer < 8u && (buffer_control & CAN_BUFFER_TRANSMIT) != 0u) {
            if (received_frame->remote && (buffer_control & CAN_BUFFER_REMOTE) != 0u) {
                dspic33_device_internal_can_set_buffer_control(
                    cpu, channel_index, target_buffer,
                    (uint16_t)(buffer_control | CAN_BUFFER_REQUEST));
                dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel_index, CAN_EVENT_TRANSMIT_START,
                                 0u);
                return false;
            }
            continue;
        }
        if (dspic33_device_internal_can_buffer_flag(cpu, channel_index, target_buffer, false)) {
            continue;
        }
        *selected_buffer = target_buffer;
        *matched_filter_index = filter_index;
        return true;
    }
    if (filter_match_found && first_receive_buffer < 32u) {
        dspic33_device_internal_can_set_buffer_flag(cpu, channel_index, first_receive_buffer, true);
        dspic33_device_internal_can_raise_event(cpu, channel_index, CAN_INTERRUPT_OVERFLOW,
                                                first_receive_buffer, first_filter_index);
        if (first_match_is_fifo) {
            dspic33_device_internal_can_advance_fifo_write(cpu, channel_index,
                                                           first_receive_buffer);
        }
    }
    return false;
}

void dspic33_device_internal_can_encode_frame(const Dspic33CanFrame* can_frame,
                                              uint8_t filter_index, uint16_t encoded_words[8]) {
    const uint32_t standard_id = can_identifier_sid(can_frame);
    const uint32_t extended_id = can_identifier_eid(can_frame);

    memset(encoded_words, 0, sizeof(uint16_t) * 8u);
    encoded_words[0] = (uint16_t)(standard_id << 2u);
    if (can_frame->extended) {
        encoded_words[0] |= 3u;
        encoded_words[1] = (uint16_t)(extended_id >> 6u);
        encoded_words[2] = (uint16_t)((extended_id & 0x3fu) << 10u);
        if (can_frame->remote) {
            encoded_words[2] |= 0x0200u;
        }
    } else if (can_frame->remote) {
        encoded_words[0] |= 2u;
    }
    encoded_words[2] |= can_frame->length > 8u ? 8u : can_frame->length;
    for (uint8_t data_index = 0u; data_index < can_frame->length && data_index < 8u; data_index++) {
        encoded_words[3u + data_index / 2u] |= (uint16_t)can_frame->data[data_index]
                                               << ((data_index & 1u) * 8u);
    }
    encoded_words[7] = (uint16_t)filter_index << 8u;
}

Dspic33CanFrame dspic33_device_internal_can_decode_frame(const uint16_t encoded_words[8]) {
    Dspic33CanFrame decoded_frame;
    const uint32_t standard_id = (encoded_words[0] >> 2u) & 0x7ffu;

    memset(&decoded_frame, 0, sizeof(decoded_frame));
    decoded_frame.extended = (encoded_words[0] & 1u) != 0u;
    if (decoded_frame.extended) {
        decoded_frame.identifier = (standard_id << 18u) |
                                   ((uint32_t)(encoded_words[1] & 0x0fffu) << 6u) |
                                   ((encoded_words[2] >> 10u) & 0x3fu);
        decoded_frame.remote = (encoded_words[2] & 0x0200u) != 0u;
    } else {
        decoded_frame.identifier = standard_id;
        decoded_frame.remote = (encoded_words[0] & 2u) != 0u;
    }
    decoded_frame.length = (uint8_t)(encoded_words[2] & 0x0fu);
    if (decoded_frame.length > 8u) {
        decoded_frame.length = 8u;
    }
    for (uint8_t data_index = 0u; data_index < decoded_frame.length; data_index++) {
        decoded_frame.data[data_index] =
            (uint8_t)(encoded_words[3u + data_index / 2u] >> ((data_index & 1u) * 8u));
    }
    return decoded_frame;
}

static void can_append_bits(bool* bit_stream, uint8_t* bit_count, uint32_t field_value,
                            uint8_t field_width) {
    while (field_width != 0u) {
        field_width--;
        bit_stream[(*bit_count)++] = (field_value & (uint32_t)(1u << field_width)) != 0u;
    }
}

static uint16_t can_crc(const bool bit_stream[128], uint8_t bit_count) {
    uint16_t crc_value = 0u;

    for (uint8_t bit_index = 0u; bit_index < bit_count; bit_index++) {
        const bool crc_feedback = ((crc_value & 0x4000u) != 0u) != bit_stream[bit_index];
        crc_value = (uint16_t)((crc_value << 1u) & 0x7fffu);
        if (crc_feedback) {
            crc_value ^= 0x4599u;
        }
    }
    return crc_value;
}

static uint16_t can_stuff_bits(const bool* raw_bits, uint8_t raw_count, bool* stuffed_bits) {
    uint16_t stuffed_count = 0u;
    bool previous_bit = raw_bits[0];
    uint8_t same_bit_count = 0u;

    for (uint8_t bit_index = 0u; bit_index < raw_count; bit_index++) {
        stuffed_bits[stuffed_count++] = raw_bits[bit_index];
        if (raw_bits[bit_index] == previous_bit) {
            same_bit_count++;
        } else {
            previous_bit = raw_bits[bit_index];
            same_bit_count = 1u;
        }
        if (same_bit_count == 5u) {
            stuffed_bits[stuffed_count++] = !previous_bit;
            previous_bit = !previous_bit;
            same_bit_count = 1u;
        }
    }
    return stuffed_count;
}

static uint16_t can_arbitration_bit_count(const Dspic33CanFrame* frame) {
    bool raw_bits[40];
    bool stuffed_bits[48];
    uint8_t raw_bit_count = 0u;
    can_append_bits(raw_bits, &raw_bit_count, 0u, 1u);
    if (frame->extended) {
        can_append_bits(raw_bits, &raw_bit_count, frame->identifier >> 18u, 11u);
        can_append_bits(raw_bits, &raw_bit_count, 3u, 2u);
        can_append_bits(raw_bits, &raw_bit_count, frame->identifier & 0x3ffffu, 18u);
        can_append_bits(raw_bits, &raw_bit_count, frame->remote ? 1u : 0u, 1u);
    } else {
        can_append_bits(raw_bits, &raw_bit_count, frame->identifier, 11u);
        can_append_bits(raw_bits, &raw_bit_count, frame->remote ? 1u : 0u, 1u);
    }
    return can_stuff_bits(raw_bits, raw_bit_count, stuffed_bits);
}

uint16_t dspic33_device_internal_can_frame_bits(const Dspic33CanFrame* frame, bool bits[160]) {
    bool raw_bits[128];
    uint8_t raw_bit_count = 0u;
    uint8_t data_length_code = frame->length > 15u ? 15u : frame->length;
    uint8_t data_length = data_length_code > 8u ? 8u : data_length_code;
    uint16_t stuffed_bit_count;
    can_append_bits(raw_bits, &raw_bit_count, 0u, 1u);
    if (frame->extended) {
        can_append_bits(raw_bits, &raw_bit_count, frame->identifier >> 18u, 11u);
        can_append_bits(raw_bits, &raw_bit_count, 3u, 2u);
        can_append_bits(raw_bits, &raw_bit_count, frame->identifier & 0x3ffffu, 18u);
        can_append_bits(raw_bits, &raw_bit_count, frame->remote ? 1u : 0u, 1u);
        can_append_bits(raw_bits, &raw_bit_count, 0u, 2u);
    } else {
        can_append_bits(raw_bits, &raw_bit_count, frame->identifier, 11u);
        can_append_bits(raw_bits, &raw_bit_count, frame->remote ? 1u : 0u, 1u);
        can_append_bits(raw_bits, &raw_bit_count, 0u, 2u);
    }
    can_append_bits(raw_bits, &raw_bit_count, data_length_code, 4u);
    if (!frame->remote) {
        for (uint8_t data_index = 0u; data_index < data_length; data_index++) {
            can_append_bits(raw_bits, &raw_bit_count, frame->data[data_index], 8u);
        }
    }
    can_append_bits(raw_bits, &raw_bit_count, can_crc(raw_bits, raw_bit_count), 15u);
    stuffed_bit_count = can_stuff_bits(raw_bits, raw_bit_count, bits);

    for (uint8_t bit_index = 0u; bit_index < 13u; bit_index++) {
        bits[stuffed_bit_count++] = true;
    }
    return stuffed_bit_count;
}

static uint16_t can_frame_bit_count(const Dspic33CanFrame* frame) {
    bool bits[160];
    return dspic33_device_internal_can_frame_bits(frame, bits);
}

uint64_t dspic33_device_internal_can_bit_cycles(const Dspic33* cpu, uint8_t channel) {
    uint16_t config1 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x10u));
    uint16_t config2 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x12u));
    uint16_t control = dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel]);
    uint64_t prescaler = (config1 & 0x003fu) + 1u;
    uint64_t quanta =
        1u + (config2 & 7u) + 1u + ((config2 >> 3u) & 7u) + 1u + ((config2 >> 8u) & 7u) + 1u;
    uint64_t clock_divisor = (control & 0x0800u) != 0u ? 2u : 1u;
    return prescaler * quanta * clock_divisor;
}

uint64_t dspic33_device_internal_can_frame_cycles(const Dspic33* cpu, uint8_t channel,
                                                  const Dspic33CanFrame* frame) {
    return (uint64_t)can_frame_bit_count(frame) *
           dspic33_device_internal_can_bit_cycles(cpu, channel);
}

void dspic33_device_internal_can_capture_received_frame(Dspic33* cpu, uint8_t channel) {
    if ((dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel]) & CAN_CAPTURE) ==
        0u) {
        return;
    }
    dspic33_device_internal_input_capture_level(cpu, 1u, true);
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_CAPTURE_RELEASE,
                          dspic33_device_internal_can_bit_cycles(cpu, channel))) {
        dspic33_device_internal_input_capture_level(cpu, 1u, false);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static uint32_t can_serial_value(const bool* serial_bits, uint16_t start_index, uint8_t bit_width) {
    uint32_t field_value = 0u;

    for (uint8_t bit_index = 0u; bit_index < bit_width; bit_index++) {
        field_value = (field_value << 1u) | (serial_bits[start_index + bit_index] ? 1u : 0u);
    }
    return field_value;
}

Dspic33CanSerialResult dspic33_device_internal_can_decode_serial(const Dspic33* cpu,
                                                                 uint8_t channel,
                                                                 Dspic33CanFrame* frame,
                                                                 uint16_t* tail_start) {
    bool raw_bits[128];
    uint16_t serial_bit_count = cpu->io.can_rx_serial_count[channel];
    uint16_t serial_bit_index = 0u;
    uint16_t raw_bit_count = 0u;
    uint16_t expected_raw_bit_count = 0u;
    bool previous_bit = false;
    bool expect_stuffed_bit = false;
    uint8_t same_bit_count = 0u;
    bool is_extended = false;
    bool is_remote = false;
    uint8_t data_length = 0u;

    memset(frame, 0, sizeof(*frame));
    *tail_start = 0u;
    while (serial_bit_index < serial_bit_count) {
        bool received_bit = cpu->io.can_rx_serial_bits[channel][serial_bit_index++] != 0u;

        if (expect_stuffed_bit) {
            if (received_bit == previous_bit) {
                return CAN_SERIAL_INVALID;
            }
            previous_bit = received_bit;
            same_bit_count = 1u;
            expect_stuffed_bit = false;
            if (expected_raw_bit_count != 0u && raw_bit_count == expected_raw_bit_count) {
                break;
            }
            continue;
        }

        raw_bits[raw_bit_count++] = received_bit;
        if (raw_bit_count == 1u && received_bit) {
            return CAN_SERIAL_INVALID;
        }

        if (same_bit_count == 0u || received_bit != previous_bit) {
            previous_bit = received_bit;
            same_bit_count = 1u;
        } else {
            same_bit_count++;
        }

        if (same_bit_count == 5u) {
            expect_stuffed_bit = true;
        }

        if (raw_bit_count >= 14u) {
            is_extended = raw_bits[13];
        }

        if (!is_extended && raw_bit_count >= 19u) {
            is_remote = raw_bits[12];
            data_length = (uint8_t)can_serial_value(raw_bits, 15u, 4u);
            if (data_length > 8u) {
                data_length = 8u;
            }
            expected_raw_bit_count = (uint16_t)(34u + (is_remote ? 0u : data_length * 8u));
        } else if (is_extended && raw_bit_count >= 39u) {
            is_remote = raw_bits[32];
            data_length = (uint8_t)can_serial_value(raw_bits, 35u, 4u);
            if (data_length > 8u) {
                data_length = 8u;
            }
            expected_raw_bit_count = (uint16_t)(54u + (is_remote ? 0u : data_length * 8u));
        }

        if (expected_raw_bit_count != 0u && raw_bit_count == expected_raw_bit_count &&
            !expect_stuffed_bit) {
            break;
        }
        if (raw_bit_count >= sizeof(raw_bits)) {
            return CAN_SERIAL_INVALID;
        }
    }

    if (expected_raw_bit_count == 0u || raw_bit_count < expected_raw_bit_count ||
        expect_stuffed_bit) {
        return CAN_SERIAL_INCOMPLETE;
    }
    frame->extended = is_extended;
    frame->remote = is_remote;
    frame->length = data_length;
    if (is_extended) {
        frame->identifier =
            (can_serial_value(raw_bits, 1u, 11u) << 18u) | can_serial_value(raw_bits, 14u, 18u);
    } else {
        frame->identifier = can_serial_value(raw_bits, 1u, 11u);
    }
    uint16_t data_start_index = is_extended ? 39u : 19u;

    for (uint8_t data_index = 0u; data_index < data_length && !is_remote; data_index++) {
        frame->data[data_index] =
            (uint8_t)can_serial_value(raw_bits, data_start_index + data_index * 8u, 8u);
    }
    *tail_start = serial_bit_index;
    uint16_t crc_start_index = (uint16_t)(expected_raw_bit_count - 15u);

    if (can_crc(raw_bits, (uint8_t)crc_start_index) !=
        (uint16_t)can_serial_value(raw_bits, crc_start_index, 15u)) {
        return CAN_SERIAL_INVALID;
    }
    if ((uint16_t)(serial_bit_count - serial_bit_index) < 10u) {
        return CAN_SERIAL_INCOMPLETE;
    }
    if (!cpu->io.can_rx_serial_bits[channel][serial_bit_index] ||
        !cpu->io.can_rx_serial_bits[channel][serial_bit_index + 2u]) {
        return CAN_SERIAL_INVALID;
    }
    for (uint8_t tail_index = 3u; tail_index < 9u; tail_index++) {
        if (!cpu->io.can_rx_serial_bits[channel][serial_bit_index + tail_index]) {
            return CAN_SERIAL_INVALID;
        }
    }
    return CAN_SERIAL_VALID;
}

uint64_t dspic33_device_internal_can_sample_cycles(const Dspic33* cpu, uint8_t channel) {
    uint16_t timing_config1 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x10u));
    uint16_t timing_config2 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x12u));
    uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel]);
    uint64_t prescaler = (timing_config1 & 0x003fu) + 1u;
    uint64_t time_quanta = 1u + (timing_config2 & 7u) + 1u + ((timing_config2 >> 3u) & 7u) + 1u;
    uint64_t peripheral_clock_divisor = (control_word & 0x0800u) != 0u ? 2u : 1u;
    return prescaler * time_quanta * peripheral_clock_divisor;
}

uint64_t dspic33_device_internal_can_time_quantum(const Dspic33* cpu, uint8_t channel) {
    uint16_t timing_config1 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x10u));
    uint16_t control_word =
        dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel]);
    uint64_t prescaler = (timing_config1 & 0x003fu) + 1u;
    uint64_t peripheral_clock_divisor = (control_word & 0x0800u) != 0u ? 2u : 1u;
    return prescaler * peripheral_clock_divisor;
}

bool dspic33_device_internal_can_triple_sample(const Dspic33* cpu, uint8_t channel) {
    return (dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x12u)) &
            0x0040u) != 0u;
}

static uint8_t can_receive_pps_pin(const Dspic33* cpu, uint8_t channel) {
    uint16_t pps_mapping = dspic33_device_internal_raw_word(cpu, 0x06d4u);
    return channel == 0u ? (uint8_t)(pps_mapping & 0x007fu)
                         : (uint8_t)((pps_mapping >> 8u) & 0x007fu);
}

bool dspic33_device_internal_can_serial_receive_enabled(const Dspic33* cpu, uint8_t channel) {
    uint8_t can_mode = dspic33_device_internal_can_mode(cpu, channel);

    return dspic33_device_internal_can_power_enabled(cpu, channel) &&
           cpu->power_state != DSPIC33_POWER_SLEEP &&
           (dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) &
            CAN_BUS_OFF) == 0u &&
           (can_mode == CAN_MODE_NORMAL || can_mode == CAN_MODE_LISTEN ||
            can_mode == CAN_MODE_LISTEN_ALL);
}

bool dspic33_device_internal_can_schedule_mode_transition(Dspic33* cpu, uint8_t channel,
                                                          uint8_t mode) {
    uint32_t mode_transition_event =
        CAN_EVENT_MODE_TRANSITION | ((uint32_t)mode << CAN_EVENT_MODE_SHIFT) |
        ((uint32_t)cpu->io.can_mode_generation[channel] << CAN_EVENT_MODE_GENERATION_SHIFT);
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, mode_transition_event,
                            11u * dspic33_device_internal_can_bit_cycles(cpu, channel));
}

void dspic33_device_internal_can_remove_transmit_events(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_error_event(Dspic33* cpu, uint8_t channel, uint32_t value);
void dspic33_device_internal_can_invalid_event(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_can_refresh_error_status(Dspic33* cpu, uint8_t channel);

bool dspic33_device_internal_can_schedule_receive_sample(Dspic33* cpu, uint8_t channel,
                                                         uint64_t delay) {
    uint32_t event = dspic33_device_internal_can_triple_sample(cpu, channel)
                         ? CAN_EVENT_RECEIVE_SAMPLE_FIRST
                         : CAN_EVENT_RECEIVE_SAMPLE;
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, event, delay);
}

uint64_t dspic33_device_internal_can_first_sample_delay(const Dspic33* cpu, uint8_t channel) {
    uint64_t delay = dspic33_device_internal_can_sample_cycles(cpu, channel);
    return dspic33_device_internal_can_triple_sample(cpu, channel)
               ? delay - 2u * dspic33_device_internal_can_time_quantum(cpu, channel)
               : delay;
}

bool dspic33_device_internal_can_schedule_transmit_sample(Dspic33* cpu, uint8_t channel,
                                                          uint64_t delay) {
    uint32_t event = dspic33_device_internal_can_triple_sample(cpu, channel)
                         ? CAN_EVENT_TRANSMIT_SAMPLE_FIRST
                         : CAN_EVENT_TRANSMIT_SAMPLE;
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, event, delay);
}

static bool can_receive_sample_event(uint32_t event_kind) {
    return event_kind == CAN_EVENT_RECEIVE_SAMPLE || event_kind == CAN_EVENT_RECEIVE_SAMPLE_FIRST ||
           event_kind == CAN_EVENT_RECEIVE_SAMPLE_SECOND;
}

static bool can_transmit_timing_event(uint32_t event_kind) {
    return event_kind == CAN_EVENT_TRANSMIT_SAMPLE ||
           event_kind == CAN_EVENT_TRANSMIT_SAMPLE_FIRST ||
           event_kind == CAN_EVENT_TRANSMIT_SAMPLE_SECOND ||
           event_kind == CAN_EVENT_TRANSMIT_BUS_FINISH;
}

static uint64_t can_receive_sample_point(const Dspic33* cpu, uint8_t channel) {
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        const Dspic33Event* event = &cpu->events.items[event_index];
        uint32_t event_kind = event->value & CAN_EVENT_KIND_MASK;

        if (event->type != DSPIC33_EVENT_CAN || event->source != channel ||
            !can_receive_sample_event(event_kind)) {
            continue;
        }
        if (event_kind == CAN_EVENT_RECEIVE_SAMPLE_FIRST) {
            return event->cycle + 2u * dspic33_device_internal_can_time_quantum(cpu, channel);
        }
        if (event_kind == CAN_EVENT_RECEIVE_SAMPLE_SECOND) {
            return event->cycle + dspic33_device_internal_can_time_quantum(cpu, channel);
        }
        return event->cycle;
    }
    return 0u;
}

static void can_shift_timing(Dspic33* cpu, uint8_t channel, int64_t adjustment) {
    for (size_t event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        uint32_t event_kind = event->value & CAN_EVENT_KIND_MASK;

        if (event->type == DSPIC33_EVENT_CAN && event->source == channel &&
            (can_receive_sample_event(event_kind) || can_transmit_timing_event(event_kind))) {
            event->cycle = (uint64_t)((int64_t)event->cycle + adjustment);
        }
    }
    cpu->io.can_tx_phase_adjustment[channel] += adjustment;
    dspic33_reorder_events(cpu);
}

static void can_resynchronize(Dspic33* cpu, uint8_t channel) {
    uint16_t received_bit_count = cpu->io.can_rx_serial_count[channel];
    uint64_t sample_cycle = can_receive_sample_point(cpu, channel);

    if (sample_cycle == 0u || cpu->io.can_resync_count[channel] == received_bit_count) {
        return;
    }
    int64_t expected_sample_start =
        (int64_t)sample_cycle - (int64_t)dspic33_device_internal_can_sample_cycles(cpu, channel);
    int64_t phase_error = (int64_t)cpu->device_cycles - expected_sample_start;
    uint16_t timing_config1 = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x10u));
    int64_t maximum_adjustment = (int64_t)(((timing_config1 >> 6u) & 3u) + 1u) *
                                 (int64_t)dspic33_device_internal_can_time_quantum(cpu, channel);
    int64_t phase_adjustment = phase_error;

    if (phase_adjustment > maximum_adjustment) {
        phase_adjustment = maximum_adjustment;
    } else if (phase_adjustment < -maximum_adjustment) {
        phase_adjustment = -maximum_adjustment;
    }
    cpu->io.can_resync_count[channel] = received_bit_count;
    can_shift_timing(cpu, channel, phase_adjustment);
}

static void can_lose_arbitration(Dspic33* cpu, uint8_t channel) {
    uint8_t channel_mask = (uint8_t)(1u << channel);
    uint8_t tx_buffer = cpu->io.can_tx_buffer[channel];
    uint16_t buffer_control = dspic33_device_internal_can_buffer_control(cpu, channel, tx_buffer);

    dspic33_device_internal_can_set_buffer_control(cpu, channel, tx_buffer,
                                                   (uint16_t)(buffer_control | CAN_BUFFER_LOST));
    dspic33_device_internal_can_remove_transmit_events(cpu, channel);
    cpu->io.can_tx_on_bus &= (uint8_t)~channel_mask;
    cpu->io.can_tx_busy &= (uint8_t)~channel_mask;
    cpu->io.can_tx_retry_wait |= channel_mask;
}

static void can_transmit_error(Dspic33* cpu, uint8_t channel) {
    uint8_t channel_mask = (uint8_t)(1u << channel);
    uint8_t tx_buffer = cpu->io.can_tx_buffer[channel];
    uint16_t buffer_control = dspic33_device_internal_can_buffer_control(cpu, channel, tx_buffer);
    uint64_t remaining_bit_cycles = dspic33_device_internal_can_bit_cycles(cpu, channel) -
                                    dspic33_device_internal_can_sample_cycles(cpu, channel);

    dspic33_device_internal_can_set_buffer_control(cpu, channel, tx_buffer,
                                                   (uint16_t)(buffer_control | CAN_BUFFER_ERROR));
    dspic33_device_internal_can_remove_transmit_events(cpu, channel);
    cpu->io.can_tx_on_bus &= (uint8_t)~channel_mask;
    cpu->io.can_tx_busy &= (uint8_t)~channel_mask;
    cpu->io.can_tx_retry_wait |= channel_mask;
    cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
    dspic33_device_internal_can_error_event(cpu, channel,
                                            CAN_EVENT_ERROR | CAN_EVENT_TRANSMIT_ERROR |
                                                (8u << CAN_EVENT_ERROR_COUNT_SHIFT));
    if ((dspic33_device_internal_raw_word(cpu,
                                          (uint16_t)(dspic33_device_can_bases[channel] + 0x0au)) &
         CAN_BUS_OFF) != 0u) {
        cpu->io.can_tx_retry_wait &= (uint8_t)~channel_mask;
        return;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_ERROR_START,
                          remaining_bit_cycles)) {
        cpu->io.can_tx_retry_wait &= (uint8_t)~channel_mask;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_can_monitor_transmit_sample(Dspic33* cpu, uint8_t channel,
                                                         bool bus_high) {
    uint8_t channel_mask = (uint8_t)(1u << channel);

    if ((cpu->io.can_tx_on_bus & channel_mask) == 0u ||
        (cpu->io.can_overload_active & channel_mask) != 0u) {
        return;
    }
    Dspic33CanFrame frame = dspic33_device_internal_can_decode_frame(cpu->io.can_tx_words[channel]);
    bool encoded_bits[160];
    uint64_t bit_cycle_count = dspic33_device_internal_can_bit_cycles(cpu, channel);
    int64_t elapsed_cycles = (int64_t)(cpu->device_cycles - cpu->io.can_tx_start_cycle[channel]) -
                             cpu->io.can_tx_phase_adjustment[channel];

    if (elapsed_cycles < 0) {
        elapsed_cycles = 0;
    }
    uint16_t bit_index = (uint16_t)((uint64_t)elapsed_cycles / bit_cycle_count);
    uint16_t frame_bit_count = dspic33_device_internal_can_frame_bits(&frame, encoded_bits);

    if (bit_index >= frame_bit_count) {
        return;
    }
    if (bit_index == frame_bit_count - 12u) {
        if (bus_high) {
            can_transmit_error(cpu, channel);
        }
    } else if (encoded_bits[bit_index] == bus_high) {
        return;
    } else if (bit_index < can_arbitration_bit_count(&frame)) {
        if (encoded_bits[bit_index]) {
            can_lose_arbitration(cpu, channel);
        } else {
            can_transmit_error(cpu, channel);
        }
    } else {
        can_transmit_error(cpu, channel);
    }
}

void dspic33_device_internal_can_receive_error(Dspic33* cpu, uint8_t channel,
                                               const Dspic33CanFrame* frame) {
    uint8_t channel_mask = (uint8_t)(1u << channel);
    uint8_t receive_mode = dspic33_device_internal_can_mode(cpu, channel);
    uint64_t remaining_bit_cycles = dspic33_device_internal_can_bit_cycles(cpu, channel) -
                                    dspic33_device_internal_can_sample_cycles(cpu, channel);

    cpu->io.can_rx_serial_active &= (uint8_t)~channel_mask;
    dspic33_device_internal_can_invalid_event(cpu, channel);
    if (receive_mode == CAN_MODE_LISTEN) {
        return;
    }
    if (receive_mode == CAN_MODE_LISTEN_ALL &&
        (!dspic33_device_internal_can_queue_push(&cpu->io.can_rx[channel], frame) ||
         !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u))) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    dspic33_device_internal_can_error_event(cpu, channel,
                                            CAN_EVENT_ERROR | (1u << CAN_EVENT_ERROR_COUNT_SHIFT));
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_ERROR_START,
                          remaining_bit_cycles)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_can_receive_success(Dspic33* cpu, uint8_t channel) {
    uint16_t status_address = (uint16_t)(dspic33_device_can_bases[channel] + 0x0eu);
    uint16_t status_counts = dspic33_device_internal_raw_word(cpu, status_address);
    uint8_t receive_error_count = (uint8_t)status_counts;

    if (dspic33_device_internal_can_mode(cpu, channel) == CAN_MODE_LISTEN ||
        receive_error_count == 0u) {
        return;
    }
    receive_error_count = receive_error_count > 127u ? 127u : (uint8_t)(receive_error_count - 1u);
    dspic33_device_internal_raw_write_word(
        cpu, status_address, (uint16_t)((status_counts & 0xff00u) | receive_error_count));
    dspic33_device_internal_can_refresh_error_status(cpu, channel);
}

static bool can_bus_off_input(Dspic33* cpu, uint8_t channel, bool input_is_high) {
    uint16_t status_address = (uint16_t)(dspic33_device_can_bases[channel] + 0x0au);
    uint16_t status_word = dspic33_device_internal_raw_word(cpu, status_address);

    if ((status_word & CAN_BUS_OFF) == 0u) {
        return false;
    }
    if (input_is_high) {
        cpu->io.can_bus_off_recessive_bits[channel]++;
    } else {
        cpu->io.can_bus_off_recessive_bits[channel] = 0u;
    }
    if (cpu->io.can_bus_off_recessive_bits[channel] < 128u * 11u) {
        return true;
    }
    cpu->io.can_bus_off_recessive_bits[channel] = 0u;
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x0eu), 0u);
    dspic33_device_internal_raw_write_word(cpu, status_address,
                                           (uint16_t)(status_word & ~CAN_BUS_OFF));
    dspic33_device_internal_can_refresh_error_status(cpu, channel);
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    return true;
}

bool dspic33_device_internal_can_schedule_intermission(Dspic33* cpu, uint8_t channel) {
    uint8_t channel_mask = (uint8_t)(1u << channel);
    uint32_t intermission_event =
        CAN_EVENT_INTERMISSION_FINISH |
        ((uint32_t)cpu->io.can_intermission_generation[channel] << CAN_EVENT_GENERATION_SHIFT);
    cpu->io.can_intermission_active |= channel_mask;
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, intermission_event,
                            3u * dspic33_device_internal_can_bit_cycles(cpu, channel));
}

void dspic33_device_internal_can_start_overload(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if (cpu->io.can_overload_count[channel] >= 2u) {
        return;
    }
    cpu->io.can_intermission_generation[channel]++;
    cpu->io.can_intermission_active &= (uint8_t)~bit;
    cpu->io.can_overload_active |= bit;
    cpu->io.can_overload_start_cycle[channel] = cpu->device_cycles;
    cpu->io.can_overload_count[channel]++;
    uint32_t value =
        CAN_EVENT_OVERLOAD_FINISH |
        ((uint32_t)cpu->io.can_intermission_generation[channel] << CAN_EVENT_GENERATION_SHIFT);
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, value,
                          14u * dspic33_device_internal_can_bit_cycles(cpu, channel))) {
        cpu->io.can_overload_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_device_internal_can_intermission_finish(Dspic33* cpu, uint8_t channel,
                                                     uint32_t value) {
    uint16_t generation = (uint16_t)(value >> CAN_EVENT_GENERATION_SHIFT);
    if (generation != cpu->io.can_intermission_generation[channel]) {
        return;
    }
    cpu->io.can_intermission_active &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_overload_count[channel] = 0u;
}

void dspic33_device_internal_can_overload_finish(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint16_t generation = (uint16_t)(value >> CAN_EVENT_GENERATION_SHIFT);
    if (generation != cpu->io.can_intermission_generation[channel]) {
        return;
    }
    cpu->io.can_overload_active &= (uint8_t)~bit;
    if (!dspic33_device_internal_can_schedule_intermission(cpu, channel)) {
        cpu->io.can_intermission_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_pin_level(Dspic33* cpu, uint8_t pin, bool high) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t bit = (uint8_t)(1u << channel);
        bool previous = (cpu->io.can_rx_pin_high & bit) != 0u;
        if (can_receive_pps_pin(cpu, channel) != pin ||
            !dspic33_device_internal_pps_physical_input_enabled(cpu, pin)) {
            continue;
        }
        cpu->io.can_rx_physical_active |= bit;
        if (high) {
            cpu->io.can_rx_pin_high |= bit;
        } else {
            cpu->io.can_rx_pin_high &= (uint8_t)~bit;
        }
        uint8_t requested_mode =
            (uint8_t)((dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel]) &
                       CAN_MODE_MASK) >>
                      CAN_MODE_SHIFT);
        if (!high && requested_mode != dspic33_device_internal_can_mode(cpu, channel)) {
            cpu->io.can_mode_generation[channel]++;
            if (!dspic33_device_internal_can_schedule_mode_transition(cpu, channel,
                                                                      requested_mode)) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
        if (can_bus_off_input(cpu, channel, high)) {
            continue;
        }
        if (previous && !high && (cpu->io.can_intermission_active & bit) != 0u) {
            dspic33_device_internal_can_start_overload(cpu, channel);
            continue;
        }
        if (previous && !high && cpu->power_state == DSPIC33_POWER_SLEEP &&
            (dspic33_device_internal_raw_word(
                 cpu, (uint16_t)(dspic33_device_can_bases[channel] + 0x12u)) &
             CAN_WAKE_FILTER) != 0u &&
            (dspic33_device_internal_raw_word(cpu, 0x0760u) & (uint16_t)(2u << channel)) == 0u) {
            dspic33_device_internal_can_raise_event(cpu, channel, CAN_INTERRUPT_WAKE, 0u, 0u);
            continue;
        }
        if (previous && !high && (cpu->io.can_rx_serial_active & bit) != 0u) {
            can_resynchronize(cpu, channel);
        }
        if (previous && !high && (cpu->io.can_rx_serial_active & bit) == 0u &&
            (cpu->io.can_rx_error_active & bit) == 0u &&
            (cpu->io.can_tx_error_active & bit) == 0u &&
            dspic33_device_internal_can_serial_receive_enabled(cpu, channel)) {
            cpu->io.can_rx_serial_active |= bit;
            cpu->io.can_rx_serial_count[channel] = 0u;
            cpu->io.can_resync_count[channel] = 0u;
            cpu->io.can_rx_sample_high[channel] = 0u;
            if (!dspic33_device_internal_can_schedule_receive_sample(
                    cpu, channel, dspic33_device_internal_can_first_sample_delay(cpu, channel))) {
                cpu->io.can_rx_serial_active &= (uint8_t)~bit;
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
    }
}

void dspic33_device_internal_refresh_can_pps_inputs(Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t pin = can_receive_pps_pin(cpu, channel);
        const Dspic33PpsPin* mapping = dspic33_device_internal_pps_pin(pin);
        if (mapping == NULL) {
            continue;
        }
        uint16_t mask = (uint16_t)(1u << mapping->bit);
        if ((cpu->io.gpio_driven[mapping->port] & mask) != 0u) {
            can_receive_pin_level(cpu, pin, (cpu->io.gpio[mapping->port] & mask) != 0u);
        }
    }
}
