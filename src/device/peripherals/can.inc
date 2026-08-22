
static uint16_t can_filter_word(const Dspic33* cpu, uint8_t channel, uint16_t offset) {
    return cpu->io.can_filter_window[channel][(offset - 0x20u) / 2u];
}

static uint8_t can_mode(const Dspic33* cpu, uint8_t channel) {
    return (uint8_t)((raw_word(cpu, can_bases[channel]) >> 5u) & 7u);
}

static bool can_power_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = raw_word(cpu, can_bases[channel]);
    if ((raw_word(cpu, 0x0760u) & (uint16_t)(2u << channel)) != 0u) {
        return false;
    }
    if (cpu->power_state == DSPIC33_POWER_ACTIVE) {
        return true;
    }
    return cpu->power_state == DSPIC33_POWER_IDLE && (control & CAN_STOP_IDLE) == 0u;
}

static uint8_t can_buffer_count(const Dspic33* cpu, uint8_t channel) {
    static const uint8_t counts[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u, 32u};
    uint16_t control = raw_word(cpu, (uint16_t)(can_bases[channel] + 6u));
    return counts[(control >> 13u) & 7u];
}

static uint16_t can_buffer_control(const Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t value = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x30u + (buffer / 2u) * 2u));
    return (uint16_t)(value >> ((buffer & 1u) * 8u));
}

static void can_set_buffer_control(Dspic33* cpu, uint8_t channel, uint8_t buffer, uint16_t value) {
    uint16_t address = (uint16_t)(can_bases[channel] + 0x30u + (buffer / 2u) * 2u);
    uint16_t current = raw_word(cpu, address);
    uint8_t shift = (uint8_t)((buffer & 1u) * 8u);
    current = (uint16_t)((current & ~(uint16_t)(0xffu << shift)) | ((value & 0xffu) << shift));
    raw_write_word(cpu, address, current);
}

static uint16_t can_buffer_flag_address(uint8_t channel, uint8_t buffer, bool overflow) {
    return (uint16_t)(can_bases[channel] + (overflow ? 0x28u : 0x20u) + (buffer >= 16u ? 2u : 0u));
}

static bool can_buffer_flag(const Dspic33* cpu, uint8_t channel, uint8_t buffer, bool overflow) {
    uint16_t address = can_buffer_flag_address(channel, buffer, overflow);
    return (raw_word(cpu, address) & (uint16_t)(1u << (buffer & 15u))) != 0u;
}

static void can_set_buffer_flag(Dspic33* cpu, uint8_t channel, uint8_t buffer, bool overflow) {
    uint16_t address = can_buffer_flag_address(channel, buffer, overflow);
    raw_write_word(cpu, address,
                   (uint16_t)(raw_word(cpu, address) | (uint16_t)(1u << (buffer & 15u))));
}

static void can_update_vector(Dspic33* cpu, uint8_t channel) {
    uint16_t base = can_bases[channel];
    uint16_t active = (uint16_t)(raw_word(cpu, (uint16_t)(base + 0x0au)) &
                                 raw_word(cpu, (uint16_t)(base + 0x0cu)));
    uint8_t code = 0x40u;
    if ((active & (CAN_INTERRUPT_TRANSMIT | CAN_INTERRUPT_RECEIVE)) != 0u) {
        code = cpu->io.can_last_buffer[channel];
    } else if ((active & CAN_INTERRUPT_ERROR) != 0u) {
        code = 0x41u;
    } else if ((active & CAN_INTERRUPT_WAKE) != 0u) {
        code = 0x42u;
    } else if ((active & CAN_INTERRUPT_OVERFLOW) != 0u) {
        code = 0x43u;
    } else if ((active & CAN_INTERRUPT_FIFO) != 0u) {
        code = 0x44u;
    }
    raw_write_word(cpu, (uint16_t)(base + 4u),
                   (uint16_t)(((uint16_t)cpu->io.can_last_filter[channel] << 8u) | code));
    if (active != 0u) {
        dspic33_raise_interrupt(cpu, can_event_irqs[channel]);
    }
}

static void can_raise_event(Dspic33* cpu, uint8_t channel, uint16_t flag, uint8_t buffer,
                            uint8_t filter) {
    uint16_t address = (uint16_t)(can_bases[channel] + 0x0au);
    raw_write_word(cpu, address, (uint16_t)(raw_word(cpu, address) | flag));
    cpu->io.can_last_buffer[channel] = buffer;
    cpu->io.can_last_filter[channel] = filter;
    can_update_vector(cpu, channel);
}

static bool can_dma_ready(const Dspic33* cpu, uint8_t request, uint16_t pad, bool transmit) {
    uint8_t dma;
    for (dma = 0u; dma < DSPIC33_DMA_COUNT; dma++) {
        uint16_t base = dma_channel_base(dma);
        uint16_t control = raw_word(cpu, base);
        if ((control & DMA_CON_CHEN) != 0u &&
            (raw_word(cpu, (uint16_t)(base + 2u)) & DMA_REQ_SOURCE_MASK) == request &&
            raw_word(cpu, (uint16_t)(base + 0x0cu)) == pad && (control & DMA_CON_SIZE_BYTE) == 0u &&
            (control & DMA_CON_AMODE_MASK) == DMA_CON_AMODE_PERIPHERAL &&
            ((control & DMA_CON_RAM_TO_PERIPHERAL) != 0u) == transmit) {
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

static bool can_devicenet_match(const Dspic33CanFrame* frame, uint32_t expected, uint8_t bits) {
    uint32_t data = 0u;
    uint8_t available = (uint8_t)(frame->length * 8u);
    if (bits > 18u) {
        bits = 18u;
    }
    if (bits > available) {
        bits = available;
    }
    if (bits == 0u) {
        return true;
    }
    data = (uint32_t)frame->data[0] << 16u;
    if (frame->length > 1u) {
        data |= (uint32_t)frame->data[1] << 8u;
    }
    if (frame->length > 2u) {
        data |= frame->data[2];
    }
    return (data >> (24u - bits)) == (expected >> (18u - bits));
}

static bool can_filter_matches(const Dspic33* cpu, uint8_t channel, uint8_t filter,
                               const Dspic33CanFrame* frame) {
    uint16_t filter_sid = can_filter_word(cpu, channel, (uint16_t)(0x40u + filter * 4u));
    uint16_t filter_eid = can_filter_word(cpu, channel, (uint16_t)(0x42u + filter * 4u));
    uint16_t selection =
        raw_word(cpu, (uint16_t)(can_bases[channel] + (filter < 8u ? 0x18u : 0x1au)));
    uint8_t mask_index = (uint8_t)((selection >> ((filter & 7u) * 2u)) & 3u);
    uint16_t mask_sid;
    uint16_t mask_eid;
    uint32_t sid = can_identifier_sid(frame);
    uint32_t eid = can_identifier_eid(frame);
    uint8_t devicenet = (uint8_t)(raw_word(cpu, (uint16_t)(can_bases[channel] + 2u)) & 0x001fu);
    if (mask_index >= 3u) {
        return false;
    }
    mask_sid = can_filter_word(cpu, channel, (uint16_t)(0x30u + mask_index * 4u));
    mask_eid = can_filter_word(cpu, channel, (uint16_t)(0x32u + mask_index * 4u));
    if ((mask_sid & 0x0008u) != 0u && frame->extended != ((filter_sid & 0x0008u) != 0u)) {
        return false;
    }
    if ((((sid << 5u) ^ filter_sid) & mask_sid & 0xffe0u) != 0u) {
        return false;
    }
    if (!frame->extended && devicenet != 0u && (mask_sid & 0x0008u) != 0u &&
        (filter_sid & 0x0008u) == 0u) {
        uint32_t expected = ((uint32_t)(filter_sid & 3u) << 16u) | filter_eid;
        return can_devicenet_match(frame, expected, devicenet);
    }
    if (frame->extended) {
        uint32_t expected = ((uint32_t)(filter_sid & 3u) << 16u) | filter_eid;
        uint32_t mask = ((uint32_t)(mask_sid & 3u) << 16u) | mask_eid;
        return ((eid ^ expected) & mask) == 0u;
    }
    return true;
}

static uint8_t can_filter_buffer(const Dspic33* cpu, uint8_t channel, uint8_t filter) {
    uint16_t value = can_filter_word(cpu, channel, (uint16_t)(0x20u + (filter / 4u) * 2u));
    return (uint8_t)((value >> ((filter & 3u) * 4u)) & 0x0fu);
}

static uint8_t can_fifo_end(const Dspic33* cpu, uint8_t channel) {
    return (uint8_t)(can_buffer_count(cpu, channel) - 1u);
}

static uint8_t can_next_fifo_buffer(const Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint8_t start = (uint8_t)(raw_word(cpu, (uint16_t)(can_bases[channel] + 6u)) & 0x001fu);
    return buffer >= can_fifo_end(cpu, channel) ? start : (uint8_t)(buffer + 1u);
}

static uint8_t can_advance_fifo_write(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint8_t next = can_next_fifo_buffer(cpu, channel, buffer);
    uint16_t address = (uint16_t)(can_bases[channel] + 8u);
    uint16_t fifo = raw_word(cpu, address);
    cpu->io.can_fifo_write[channel] = next;
    raw_write_word(cpu, address, (uint16_t)((fifo & 0x003fu) | ((uint16_t)next << 8u)));
    return next;
}

static bool can_select_receive_buffer(Dspic33* cpu, uint8_t channel, const Dspic33CanFrame* frame,
                                      uint8_t* buffer, uint8_t* matched_filter) {
    uint16_t enabled = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x14u));
    uint8_t first_buffer = 0u;
    uint8_t first_filter = 0u;
    bool first_fifo = false;
    bool matched = false;
    uint8_t filter;
    for (filter = 0u; filter < 16u; filter++) {
        bool fifo;
        uint8_t target;
        uint16_t control;
        if ((enabled & (uint16_t)(1u << filter)) == 0u ||
            !can_filter_matches(cpu, channel, filter, frame)) {
            continue;
        }
        target = can_filter_buffer(cpu, channel, filter);
        fifo = target == 15u;
        if (fifo) {
            target = cpu->io.can_fifo_write[channel];
        }
        if (!matched) {
            matched = true;
            first_buffer = target;
            first_filter = filter;
            first_fifo = fifo;
        }
        if (target >= can_buffer_count(cpu, channel) || target > 31u) {
            continue;
        }
        control = target < 8u ? can_buffer_control(cpu, channel, target) : 0u;
        if (target < 8u && (control & CAN_BUFFER_TRANSMIT) != 0u) {
            if (frame->remote && (control & CAN_BUFFER_REMOTE) != 0u) {
                can_set_buffer_control(cpu, channel, target,
                                       (uint16_t)(control | CAN_BUFFER_REQUEST));
                dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
                return false;
            }
            continue;
        }
        if (can_buffer_flag(cpu, channel, target, false)) {
            continue;
        }
        *buffer = target;
        *matched_filter = filter;
        return true;
    }
    if (matched && first_buffer < 32u) {
        can_set_buffer_flag(cpu, channel, first_buffer, true);
        can_raise_event(cpu, channel, CAN_INTERRUPT_OVERFLOW, first_buffer, first_filter);
        if (first_fifo) {
            can_advance_fifo_write(cpu, channel, first_buffer);
        }
    }
    return false;
}

static void can_encode_frame(const Dspic33CanFrame* frame, uint8_t filter, uint16_t words[8]) {
    uint32_t sid = can_identifier_sid(frame);
    uint32_t eid = can_identifier_eid(frame);
    uint8_t index;
    memset(words, 0, sizeof(uint16_t) * 8u);
    words[0] = (uint16_t)(sid << 2u);
    if (frame->extended) {
        words[0] |= 3u;
        words[1] = (uint16_t)(eid >> 6u);
        words[2] = (uint16_t)((eid & 0x3fu) << 10u);
        if (frame->remote) {
            words[2] |= 0x0200u;
        }
    } else if (frame->remote) {
        words[0] |= 2u;
    }
    words[2] |= frame->length > 8u ? 8u : frame->length;
    for (index = 0u; index < frame->length && index < 8u; index++) {
        words[3u + index / 2u] |= (uint16_t)frame->data[index] << ((index & 1u) * 8u);
    }
    words[7] = (uint16_t)filter << 8u;
}

static Dspic33CanFrame can_decode_frame(const uint16_t words[8]) {
    Dspic33CanFrame frame;
    uint32_t sid = (words[0] >> 2u) & 0x7ffu;
    uint8_t index;
    memset(&frame, 0, sizeof(frame));
    frame.extended = (words[0] & 1u) != 0u;
    if (frame.extended) {
        frame.identifier =
            (sid << 18u) | ((uint32_t)(words[1] & 0x0fffu) << 6u) | ((words[2] >> 10u) & 0x3fu);
        frame.remote = (words[2] & 0x0200u) != 0u;
    } else {
        frame.identifier = sid;
        frame.remote = (words[0] & 2u) != 0u;
    }
    frame.length = (uint8_t)(words[2] & 0x0fu);
    if (frame.length > 8u) {
        frame.length = 8u;
    }
    for (index = 0u; index < frame.length; index++) {
        frame.data[index] = (uint8_t)(words[3u + index / 2u] >> ((index & 1u) * 8u));
    }
    return frame;
}

static void can_append_bits(bool* bits, uint8_t* count, uint32_t value, uint8_t width) {
    while (width != 0u) {
        width--;
        bits[(*count)++] = (value & (uint32_t)(1u << width)) != 0u;
    }
}

static uint16_t can_crc(const bool bits[128], uint8_t count) {
    uint16_t crc = 0u;
    for (uint8_t index = 0u; index < count; index++) {
        bool feedback = ((crc & 0x4000u) != 0u) != bits[index];
        crc = (uint16_t)((crc << 1u) & 0x7fffu);
        if (feedback) {
            crc ^= 0x4599u;
        }
    }
    return crc;
}

static uint16_t can_stuff_bits(const bool* raw, uint8_t raw_count, bool* bits) {
    uint16_t count = 0u;
    bool previous = raw[0];
    uint8_t run = 0u;
    for (uint8_t index = 0u; index < raw_count; index++) {
        bits[count++] = raw[index];
        if (raw[index] == previous) {
            run++;
        } else {
            previous = raw[index];
            run = 1u;
        }
        if (run == 5u) {
            bits[count++] = !previous;
            previous = !previous;
            run = 1u;
        }
    }
    return count;
}

static uint16_t can_arbitration_bit_count(const Dspic33CanFrame* frame) {
    bool raw[40];
    bool bits[48];
    uint8_t raw_count = 0u;
    can_append_bits(raw, &raw_count, 0u, 1u);
    if (frame->extended) {
        can_append_bits(raw, &raw_count, frame->identifier >> 18u, 11u);
        can_append_bits(raw, &raw_count, 3u, 2u);
        can_append_bits(raw, &raw_count, frame->identifier & 0x3ffffu, 18u);
        can_append_bits(raw, &raw_count, frame->remote ? 1u : 0u, 1u);
    } else {
        can_append_bits(raw, &raw_count, frame->identifier, 11u);
        can_append_bits(raw, &raw_count, frame->remote ? 1u : 0u, 1u);
    }
    return can_stuff_bits(raw, raw_count, bits);
}

static uint16_t can_frame_bits(const Dspic33CanFrame* frame, bool bits[160]) {
    bool raw[128];
    uint8_t raw_count = 0u;
    uint8_t length = frame->length > 8u ? 8u : frame->length;
    uint16_t count;
    can_append_bits(raw, &raw_count, 0u, 1u);
    if (frame->extended) {
        can_append_bits(raw, &raw_count, frame->identifier >> 18u, 11u);
        can_append_bits(raw, &raw_count, 3u, 2u);
        can_append_bits(raw, &raw_count, frame->identifier & 0x3ffffu, 18u);
        can_append_bits(raw, &raw_count, frame->remote ? 1u : 0u, 1u);
        can_append_bits(raw, &raw_count, 0u, 2u);
    } else {
        can_append_bits(raw, &raw_count, frame->identifier, 11u);
        can_append_bits(raw, &raw_count, frame->remote ? 1u : 0u, 1u);
        can_append_bits(raw, &raw_count, 0u, 2u);
    }
    can_append_bits(raw, &raw_count, length, 4u);
    if (!frame->remote) {
        for (uint8_t index = 0u; index < length; index++) {
            can_append_bits(raw, &raw_count, frame->data[index], 8u);
        }
    }
    can_append_bits(raw, &raw_count, can_crc(raw, raw_count), 15u);
    count = can_stuff_bits(raw, raw_count, bits);
    for (uint8_t index = 0u; index < 13u; index++) {
        bits[count++] = true;
    }
    return count;
}

static uint16_t can_frame_bit_count(const Dspic33CanFrame* frame) {
    bool bits[160];
    return can_frame_bits(frame, bits);
}

static uint64_t can_bit_cycles(const Dspic33* cpu, uint8_t channel) {
    uint16_t config1 = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x10u));
    uint16_t config2 = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x12u));
    uint16_t control = raw_word(cpu, can_bases[channel]);
    uint64_t prescaler = (config1 & 0x003fu) + 1u;
    uint64_t quanta =
        1u + (config2 & 7u) + 1u + ((config2 >> 3u) & 7u) + 1u + ((config2 >> 8u) & 7u) + 1u;
    uint64_t clock_divisor = (control & 0x0800u) != 0u ? 2u : 1u;
    return prescaler * quanta * clock_divisor;
}

static uint64_t can_frame_cycles(const Dspic33* cpu, uint8_t channel,
                                 const Dspic33CanFrame* frame) {
    return (uint64_t)can_frame_bit_count(frame) * can_bit_cycles(cpu, channel);
}

static void can_capture_received_frame(Dspic33* cpu, uint8_t channel) {
    if ((raw_word(cpu, can_bases[channel]) & CAN_CAPTURE) == 0u) {
        return;
    }
    input_capture_level(cpu, 1u, true);
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_CAPTURE_RELEASE,
                          can_bit_cycles(cpu, channel))) {
        input_capture_level(cpu, 1u, false);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

typedef enum { CAN_SERIAL_INCOMPLETE, CAN_SERIAL_VALID, CAN_SERIAL_INVALID } Dspic33CanSerialResult;

static uint32_t can_serial_value(const bool* bits, uint16_t first, uint8_t width) {
    uint32_t value = 0u;
    for (uint8_t index = 0u; index < width; index++) {
        value = (value << 1u) | (bits[first + index] ? 1u : 0u);
    }
    return value;
}

static Dspic33CanSerialResult can_decode_serial(const Dspic33* cpu, uint8_t channel,
                                                Dspic33CanFrame* frame, uint16_t* tail_start) {
    bool raw[128];
    uint16_t serial_count = cpu->io.can_rx_serial_count[channel];
    uint16_t serial_index = 0u;
    uint16_t raw_count = 0u;
    uint16_t expected = 0u;
    bool previous = false;
    bool expect_stuff = false;
    uint8_t run = 0u;
    bool extended = false;
    bool remote = false;
    uint8_t length = 0u;
    memset(frame, 0, sizeof(*frame));
    *tail_start = 0u;
    while (serial_index < serial_count) {
        bool bit = cpu->io.can_rx_serial_bits[channel][serial_index++] != 0u;
        if (expect_stuff) {
            if (bit == previous) {
                return CAN_SERIAL_INVALID;
            }
            previous = bit;
            run = 1u;
            expect_stuff = false;
            if (expected != 0u && raw_count == expected) {
                break;
            }
            continue;
        }
        raw[raw_count++] = bit;
        if (raw_count == 1u && bit) {
            return CAN_SERIAL_INVALID;
        }
        if (run == 0u || bit != previous) {
            previous = bit;
            run = 1u;
        } else {
            run++;
        }
        if (run == 5u) {
            expect_stuff = true;
        }
        if (raw_count >= 14u) {
            extended = raw[13];
        }
        if (!extended && raw_count >= 19u) {
            remote = raw[12];
            length = (uint8_t)can_serial_value(raw, 15u, 4u);
            if (length > 8u) {
                length = 8u;
            }
            expected = (uint16_t)(34u + (remote ? 0u : length * 8u));
        } else if (extended && raw_count >= 39u) {
            if (!raw[12]) {
                return CAN_SERIAL_INVALID;
            }
            remote = raw[32];
            length = (uint8_t)can_serial_value(raw, 35u, 4u);
            if (length > 8u) {
                length = 8u;
            }
            expected = (uint16_t)(54u + (remote ? 0u : length * 8u));
        }
        if (expected != 0u && raw_count == expected && !expect_stuff) {
            break;
        }
        if (raw_count >= sizeof(raw)) {
            return CAN_SERIAL_INVALID;
        }
    }
    if (expected == 0u || raw_count < expected || expect_stuff) {
        return CAN_SERIAL_INCOMPLETE;
    }
    frame->extended = extended;
    frame->remote = remote;
    frame->length = length;
    if (extended) {
        frame->identifier =
            (can_serial_value(raw, 1u, 11u) << 18u) | can_serial_value(raw, 14u, 18u);
    } else {
        frame->identifier = can_serial_value(raw, 1u, 11u);
    }
    uint16_t data_index = extended ? 39u : 19u;
    for (uint8_t index = 0u; index < length && !remote; index++) {
        frame->data[index] = (uint8_t)can_serial_value(raw, data_index + index * 8u, 8u);
    }
    *tail_start = serial_index;
    uint16_t crc_index = (uint16_t)(expected - 15u);
    if (can_crc(raw, (uint8_t)crc_index) != (uint16_t)can_serial_value(raw, crc_index, 15u)) {
        return CAN_SERIAL_INVALID;
    }
    if ((uint16_t)(serial_count - serial_index) < 10u) {
        return CAN_SERIAL_INCOMPLETE;
    }
    if (!cpu->io.can_rx_serial_bits[channel][serial_index] ||
        !cpu->io.can_rx_serial_bits[channel][serial_index + 2u]) {
        return CAN_SERIAL_INVALID;
    }
    for (uint8_t index = 3u; index < 10u; index++) {
        if (!cpu->io.can_rx_serial_bits[channel][serial_index + index]) {
            return CAN_SERIAL_INVALID;
        }
    }
    return CAN_SERIAL_VALID;
}

static uint64_t can_sample_cycles(const Dspic33* cpu, uint8_t channel) {
    uint16_t config1 = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x10u));
    uint16_t config2 = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x12u));
    uint16_t control = raw_word(cpu, can_bases[channel]);
    uint64_t prescaler = (config1 & 0x003fu) + 1u;
    uint64_t quanta = 1u + (config2 & 7u) + 1u + ((config2 >> 3u) & 7u) + 1u;
    uint64_t clock_divisor = (control & 0x0800u) != 0u ? 2u : 1u;
    return prescaler * quanta * clock_divisor;
}

static uint64_t can_time_quantum(const Dspic33* cpu, uint8_t channel) {
    uint16_t config1 = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x10u));
    uint16_t control = raw_word(cpu, can_bases[channel]);
    uint64_t prescaler = (config1 & 0x003fu) + 1u;
    uint64_t clock_divisor = (control & 0x0800u) != 0u ? 2u : 1u;
    return prescaler * clock_divisor;
}

static bool can_triple_sample(const Dspic33* cpu, uint8_t channel) {
    return (raw_word(cpu, (uint16_t)(can_bases[channel] + 0x12u)) & 0x0040u) != 0u;
}

static uint8_t can_receive_pps_pin(const Dspic33* cpu, uint8_t channel) {
    uint16_t mapping = raw_word(cpu, 0x06d4u);
    return channel == 0u ? (uint8_t)(mapping & 0x007fu) : (uint8_t)((mapping >> 8u) & 0x007fu);
}

static bool can_serial_receive_enabled(const Dspic33* cpu, uint8_t channel) {
    uint8_t mode = can_mode(cpu, channel);
    return can_power_enabled(cpu, channel) && cpu->power_state != DSPIC33_POWER_SLEEP &&
           (raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) & CAN_BUS_OFF) == 0u &&
           (mode == CAN_MODE_NORMAL || mode == CAN_MODE_LISTEN || mode == CAN_MODE_LISTEN_ALL);
}

static bool can_schedule_mode_transition(Dspic33* cpu, uint8_t channel, uint8_t mode) {
    uint32_t value =
        CAN_EVENT_MODE_TRANSITION | ((uint32_t)mode << CAN_EVENT_MODE_SHIFT) |
        ((uint32_t)cpu->io.can_mode_generation[channel] << CAN_EVENT_MODE_GENERATION_SHIFT);
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, value,
                            11u * can_bit_cycles(cpu, channel));
}

static void can_remove_transmit_events(Dspic33* cpu, uint8_t channel);
static void can_error_event(Dspic33* cpu, uint8_t channel, uint32_t value);
static void can_invalid_event(Dspic33* cpu, uint8_t channel);
static void can_refresh_error_status(Dspic33* cpu, uint8_t channel);

static bool can_schedule_receive_sample(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    uint32_t event =
        can_triple_sample(cpu, channel) ? CAN_EVENT_RECEIVE_SAMPLE_FIRST : CAN_EVENT_RECEIVE_SAMPLE;
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, event, delay);
}

static uint64_t can_first_sample_delay(const Dspic33* cpu, uint8_t channel) {
    uint64_t delay = can_sample_cycles(cpu, channel);
    return can_triple_sample(cpu, channel) ? delay - 2u * can_time_quantum(cpu, channel) : delay;
}

static bool can_schedule_transmit_sample(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    uint32_t event = can_triple_sample(cpu, channel) ? CAN_EVENT_TRANSMIT_SAMPLE_FIRST
                                                     : CAN_EVENT_TRANSMIT_SAMPLE;
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, event, delay);
}

static bool can_receive_sample_event(uint32_t kind) {
    return kind == CAN_EVENT_RECEIVE_SAMPLE || kind == CAN_EVENT_RECEIVE_SAMPLE_FIRST ||
           kind == CAN_EVENT_RECEIVE_SAMPLE_SECOND;
}

static bool can_transmit_timing_event(uint32_t kind) {
    return kind == CAN_EVENT_TRANSMIT_SAMPLE || kind == CAN_EVENT_TRANSMIT_SAMPLE_FIRST ||
           kind == CAN_EVENT_TRANSMIT_SAMPLE_SECOND || kind == CAN_EVENT_TRANSMIT_BUS_FINISH;
}

static uint64_t can_receive_sample_point(const Dspic33* cpu, uint8_t channel) {
    for (size_t index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        uint32_t kind = event->value & CAN_EVENT_KIND_MASK;
        if (event->type != DSPIC33_EVENT_CAN || event->source != channel ||
            !can_receive_sample_event(kind)) {
            continue;
        }
        if (kind == CAN_EVENT_RECEIVE_SAMPLE_FIRST) {
            return event->cycle + 2u * can_time_quantum(cpu, channel);
        }
        if (kind == CAN_EVENT_RECEIVE_SAMPLE_SECOND) {
            return event->cycle + can_time_quantum(cpu, channel);
        }
        return event->cycle;
    }
    return 0u;
}

static void can_shift_timing(Dspic33* cpu, uint8_t channel, int64_t adjustment) {
    for (size_t index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        uint32_t kind = event->value & CAN_EVENT_KIND_MASK;
        if (event->type == DSPIC33_EVENT_CAN && event->source == channel &&
            (can_receive_sample_event(kind) || can_transmit_timing_event(kind))) {
            event->cycle = (uint64_t)((int64_t)event->cycle + adjustment);
        }
    }
    cpu->io.can_tx_phase_adjustment[channel] += adjustment;
    dspic33_reorder_events(cpu);
}

static void can_resynchronize(Dspic33* cpu, uint8_t channel) {
    uint16_t count = cpu->io.can_rx_serial_count[channel];
    uint64_t sample = can_receive_sample_point(cpu, channel);
    if (sample == 0u || cpu->io.can_resync_count[channel] == count) {
        return;
    }
    int64_t expected_start = (int64_t)sample - (int64_t)can_sample_cycles(cpu, channel);
    int64_t phase_error = (int64_t)cpu->device_cycles - expected_start;
    uint16_t config1 = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x10u));
    int64_t jump = (int64_t)(((config1 >> 6u) & 3u) + 1u) * (int64_t)can_time_quantum(cpu, channel);
    int64_t adjustment = phase_error;
    if (adjustment > jump) {
        adjustment = jump;
    } else if (adjustment < -jump) {
        adjustment = -jump;
    }
    cpu->io.can_resync_count[channel] = count;
    can_shift_timing(cpu, channel, adjustment);
}

static void can_lose_arbitration(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t buffer = cpu->io.can_tx_buffer[channel];
    uint16_t control = can_buffer_control(cpu, channel, buffer);
    can_set_buffer_control(cpu, channel, buffer, (uint16_t)(control | CAN_BUFFER_LOST));
    can_remove_transmit_events(cpu, channel);
    cpu->io.can_tx_on_bus &= (uint8_t)~bit;
    cpu->io.can_tx_busy &= (uint8_t)~bit;
    cpu->io.can_tx_retry_wait |= bit;
}

static void can_transmit_error(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t buffer = cpu->io.can_tx_buffer[channel];
    uint16_t control = can_buffer_control(cpu, channel, buffer);
    uint64_t remaining = can_bit_cycles(cpu, channel) - can_sample_cycles(cpu, channel);
    can_set_buffer_control(cpu, channel, buffer, (uint16_t)(control | CAN_BUFFER_ERROR));
    can_remove_transmit_events(cpu, channel);
    cpu->io.can_tx_on_bus &= (uint8_t)~bit;
    cpu->io.can_tx_busy &= (uint8_t)~bit;
    cpu->io.can_tx_retry_wait |= bit;
    cpu->io.can_rx_serial_active &= (uint8_t)~bit;
    can_error_event(cpu, channel,
                    CAN_EVENT_ERROR | CAN_EVENT_TRANSMIT_ERROR |
                        (8u << CAN_EVENT_ERROR_COUNT_SHIFT));
    if ((raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) & CAN_BUS_OFF) != 0u) {
        cpu->io.can_tx_retry_wait &= (uint8_t)~bit;
        return;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_ERROR_START,
                          remaining)) {
        cpu->io.can_tx_retry_wait &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_monitor_transmit_sample(Dspic33* cpu, uint8_t channel, bool bus_high) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_tx_on_bus & bit) == 0u || (cpu->io.can_overload_active & bit) != 0u) {
        return;
    }
    Dspic33CanFrame frame = can_decode_frame(cpu->io.can_tx_words[channel]);
    bool bits[160];
    uint64_t bit_cycles = can_bit_cycles(cpu, channel);
    int64_t elapsed = (int64_t)(cpu->device_cycles - cpu->io.can_tx_start_cycle[channel]) -
                      cpu->io.can_tx_phase_adjustment[channel];
    if (elapsed < 0) {
        elapsed = 0;
    }
    uint16_t index = (uint16_t)((uint64_t)elapsed / bit_cycles);
    uint16_t count = can_frame_bits(&frame, bits);
    if (index >= count) {
        return;
    }
    if (index == count - 12u) {
        if (bus_high) {
            can_transmit_error(cpu, channel);
        }
    } else if (bits[index] == bus_high) {
        return;
    } else if (index < can_arbitration_bit_count(&frame)) {
        if (bits[index]) {
            can_lose_arbitration(cpu, channel);
        } else {
            can_transmit_error(cpu, channel);
        }
    } else {
        can_transmit_error(cpu, channel);
    }
}

static void can_receive_error(Dspic33* cpu, uint8_t channel, const Dspic33CanFrame* frame) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t mode = can_mode(cpu, channel);
    uint64_t remaining = can_bit_cycles(cpu, channel) - can_sample_cycles(cpu, channel);
    cpu->io.can_rx_serial_active &= (uint8_t)~bit;
    can_invalid_event(cpu, channel);
    if (mode == CAN_MODE_LISTEN) {
        return;
    }
    if (mode == CAN_MODE_LISTEN_ALL &&
        (!can_queue_push(&cpu->io.can_rx[channel], frame) ||
         !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u))) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    can_error_event(cpu, channel, CAN_EVENT_ERROR | (1u << CAN_EVENT_ERROR_COUNT_SHIFT));
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_ERROR_START,
                          remaining)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_success(Dspic33* cpu, uint8_t channel) {
    uint16_t address = (uint16_t)(can_bases[channel] + 0x0eu);
    uint16_t counts = raw_word(cpu, address);
    uint8_t receive = (uint8_t)counts;
    if (can_mode(cpu, channel) == CAN_MODE_LISTEN || receive == 0u) {
        return;
    }
    receive = receive > 127u ? 127u : (uint8_t)(receive - 1u);
    raw_write_word(cpu, address, (uint16_t)((counts & 0xff00u) | receive));
    can_refresh_error_status(cpu, channel);
}

static bool can_bus_off_input(Dspic33* cpu, uint8_t channel, bool high) {
    uint16_t status_address = (uint16_t)(can_bases[channel] + 0x0au);
    uint16_t status = raw_word(cpu, status_address);
    if ((status & CAN_BUS_OFF) == 0u) {
        return false;
    }
    if (high) {
        cpu->io.can_bus_off_recessive_bits[channel]++;
    } else {
        cpu->io.can_bus_off_recessive_bits[channel] = 0u;
    }
    if (cpu->io.can_bus_off_recessive_bits[channel] < 128u * 11u) {
        return true;
    }
    cpu->io.can_bus_off_recessive_bits[channel] = 0u;
    raw_write_word(cpu, (uint16_t)(can_bases[channel] + 0x0eu), 0u);
    raw_write_word(cpu, status_address, (uint16_t)(status & ~CAN_BUS_OFF));
    can_refresh_error_status(cpu, channel);
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    return true;
}

static bool can_schedule_intermission(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint32_t value =
        CAN_EVENT_INTERMISSION_FINISH |
        ((uint32_t)cpu->io.can_intermission_generation[channel] << CAN_EVENT_GENERATION_SHIFT);
    cpu->io.can_intermission_active |= bit;
    return dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, value,
                            3u * can_bit_cycles(cpu, channel));
}

static void can_start_overload(Dspic33* cpu, uint8_t channel) {
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
                          14u * can_bit_cycles(cpu, channel))) {
        cpu->io.can_overload_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_intermission_finish(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> CAN_EVENT_GENERATION_SHIFT);
    if (generation != cpu->io.can_intermission_generation[channel]) {
        return;
    }
    cpu->io.can_intermission_active &= (uint8_t)~(uint8_t)(1u << channel);
    cpu->io.can_overload_count[channel] = 0u;
}

static void can_overload_finish(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint16_t generation = (uint16_t)(value >> CAN_EVENT_GENERATION_SHIFT);
    if (generation != cpu->io.can_intermission_generation[channel]) {
        return;
    }
    cpu->io.can_overload_active &= (uint8_t)~bit;
    if (!can_schedule_intermission(cpu, channel)) {
        cpu->io.can_intermission_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_pin_level(Dspic33* cpu, uint8_t pin, bool high) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t bit = (uint8_t)(1u << channel);
        bool previous = (cpu->io.can_rx_pin_high & bit) != 0u;
        if (can_receive_pps_pin(cpu, channel) != pin || !pps_physical_input_enabled(cpu, pin)) {
            continue;
        }
        cpu->io.can_rx_physical_active |= bit;
        if (high) {
            cpu->io.can_rx_pin_high |= bit;
        } else {
            cpu->io.can_rx_pin_high &= (uint8_t)~bit;
        }
        uint8_t requested_mode =
            (uint8_t)((raw_word(cpu, can_bases[channel]) & CAN_MODE_MASK) >> CAN_MODE_SHIFT);
        if (!high && requested_mode != can_mode(cpu, channel)) {
            cpu->io.can_mode_generation[channel]++;
            if (!can_schedule_mode_transition(cpu, channel, requested_mode)) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
        if (can_bus_off_input(cpu, channel, high)) {
            continue;
        }
        if (previous && !high && (cpu->io.can_intermission_active & bit) != 0u) {
            can_start_overload(cpu, channel);
            continue;
        }
        if (previous && !high && cpu->power_state == DSPIC33_POWER_SLEEP &&
            (raw_word(cpu, (uint16_t)(can_bases[channel] + 0x12u)) & CAN_WAKE_FILTER) != 0u &&
            (raw_word(cpu, 0x0760u) & (uint16_t)(2u << channel)) == 0u) {
            can_raise_event(cpu, channel, CAN_INTERRUPT_WAKE, 0u, 0u);
            continue;
        }
        if (previous && !high && (cpu->io.can_rx_serial_active & bit) != 0u) {
            can_resynchronize(cpu, channel);
        }
        if (previous && !high && (cpu->io.can_rx_serial_active & bit) == 0u &&
            (cpu->io.can_rx_error_active & bit) == 0u &&
            (cpu->io.can_tx_error_active & bit) == 0u && can_serial_receive_enabled(cpu, channel)) {
            cpu->io.can_rx_serial_active |= bit;
            cpu->io.can_rx_serial_count[channel] = 0u;
            cpu->io.can_resync_count[channel] = 0u;
            cpu->io.can_rx_sample_high[channel] = 0u;
            if (!can_schedule_receive_sample(cpu, channel, can_first_sample_delay(cpu, channel))) {
                cpu->io.can_rx_serial_active &= (uint8_t)~bit;
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            }
        }
    }
}

static void refresh_can_pps_inputs(Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t pin = can_receive_pps_pin(cpu, channel);
        const Dspic33PpsPin* mapping = pps_pin(pin);
        if (mapping == NULL) {
            continue;
        }
        uint16_t mask = (uint16_t)(1u << mapping->bit);
        if ((cpu->io.gpio_driven[mapping->port] & mask) != 0u) {
            can_receive_pin_level(cpu, pin, (cpu->io.gpio[mapping->port] & mask) != 0u);
        }
    }
}

static void can_receive_sample(Dspic33* cpu, uint8_t channel, bool bus_high) {
    uint8_t bit = (uint8_t)(1u << channel);
    Dspic33CanFrame frame;
    Dspic33CanSerialResult result;
    uint16_t count;
    uint16_t tail_start;
    if ((cpu->io.can_rx_serial_active & bit) == 0u || !can_serial_receive_enabled(cpu, channel)) {
        cpu->io.can_rx_serial_active &= (uint8_t)~bit;
        return;
    }
    count = cpu->io.can_rx_serial_count[channel];
    if (count >= sizeof(cpu->io.can_rx_serial_bits[channel])) {
        cpu->io.can_rx_serial_active &= (uint8_t)~bit;
        can_invalid_event(cpu, channel);
        return;
    }
    cpu->io.can_rx_serial_bits[channel][count] = bus_high;
    cpu->io.can_rx_serial_count[channel]++;
    result = can_decode_serial(cpu, channel, &frame, &tail_start);
    if (result == CAN_SERIAL_INCOMPLETE && tail_start != 0u &&
        cpu->io.can_rx_serial_count[channel] == tail_start + 1u &&
        cpu->io.can_rx_serial_bits[channel][tail_start] != 0u &&
        (cpu->io.can_tx_on_bus & bit) == 0u && can_mode(cpu, channel) != CAN_MODE_LISTEN) {
        uint64_t bit_cycles = can_bit_cycles(cpu, channel);
        uint64_t sample_cycles = can_sample_cycles(cpu, channel);
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_ACK_START,
                              bit_cycles - sample_cycles)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            cpu->io.can_rx_serial_active &= (uint8_t)~bit;
            return;
        }
    }
    if (result == CAN_SERIAL_VALID) {
        cpu->io.can_rx_serial_active &= (uint8_t)~bit;
        cpu->io.can_intermission_generation[channel]++;
        cpu->io.can_overload_count[channel] = 0u;
        if (!can_schedule_intermission(cpu, channel)) {
            cpu->io.can_intermission_active &= (uint8_t)~bit;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
        if ((cpu->io.can_tx_on_bus & bit) != 0u) {
            return;
        }
        can_receive_success(cpu, channel);
        if (!can_queue_push(&cpu->io.can_rx[channel], &frame) ||
            !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u) ||
            (((cpu->io.can_tx_retry_wait & bit) != 0u) &&
             !dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_RETRY,
                               3u * can_bit_cycles(cpu, channel)))) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        return;
    }
    if (result == CAN_SERIAL_INVALID) {
        can_receive_error(cpu, channel, &frame);
        return;
    }
    uint64_t delay = can_bit_cycles(cpu, channel);
    if (can_triple_sample(cpu, channel)) {
        delay -= 2u * can_time_quantum(cpu, channel);
    }
    if (!can_schedule_receive_sample(cpu, channel, delay)) {
        cpu->io.can_rx_serial_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_first(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_rx_serial_active & bit) == 0u) {
        return;
    }
    cpu->io.can_rx_sample_high[channel] = (cpu->io.can_rx_pin_high & bit) != 0u ? 1u : 0u;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_SAMPLE_SECOND,
                          can_time_quantum(cpu, channel))) {
        cpu->io.can_rx_serial_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_second(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_rx_serial_active & bit) == 0u) {
        return;
    }
    if ((cpu->io.can_rx_pin_high & bit) != 0u) {
        cpu->io.can_rx_sample_high[channel]++;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_SAMPLE,
                          can_time_quantum(cpu, channel))) {
        cpu->io.can_rx_serial_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_sample_final(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t high = cpu->io.can_rx_sample_high[channel];
    if ((cpu->io.can_rx_pin_high & bit) != 0u) {
        high++;
    }
    cpu->io.can_rx_sample_high[channel] = 0u;
    can_receive_sample(cpu, channel, high >= 2u);
}

static void can_ack_start(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    if (!can_serial_receive_enabled(cpu, channel) || can_mode(cpu, channel) == CAN_MODE_LISTEN) {
        return;
    }
    cpu->io.can_rx_ack |= bit;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_ACK_FINISH,
                          can_bit_cycles(cpu, channel))) {
        cpu->io.can_rx_ack &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_ack_finish(Dspic33* cpu, uint8_t channel) {
    cpu->io.can_rx_ack &= (uint8_t)~(uint8_t)(1u << channel);
}

static void can_receive_error_start(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.can_rx_error_active |= bit;
    cpu->io.can_rx_error_start_cycle[channel] = cpu->device_cycles;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_ERROR_FINISH,
                          14u * can_bit_cycles(cpu, channel))) {
        cpu->io.can_rx_error_active &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_receive_error_finish(Dspic33* cpu, uint8_t channel) {
    cpu->io.can_rx_error_active &= (uint8_t)~(uint8_t)(1u << channel);
}

static void can_mode_transition(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t mode = (uint8_t)((value >> CAN_EVENT_MODE_SHIFT) & 7u);
    uint16_t generation = (uint16_t)(value >> CAN_EVENT_MODE_GENERATION_SHIFT);
    uint16_t control = raw_word(cpu, can_bases[channel]);
    uint8_t requested = (uint8_t)((control & CAN_MODE_MASK) >> CAN_MODE_SHIFT);
    if (generation != cpu->io.can_mode_generation[channel] || mode != requested ||
        mode == can_mode(cpu, channel)) {
        return;
    }
    if (!can_power_enabled(cpu, channel) || (cpu->io.can_rx_pin_high & bit) == 0u ||
        ((cpu->io.can_tx_busy | cpu->io.can_rx_busy | cpu->io.can_rx_serial_active |
          cpu->io.can_tx_error_active | cpu->io.can_rx_error_active | cpu->io.can_rx_ack) &
         bit) != 0u) {
        cpu->io.can_mode_generation[channel]++;
        if (!can_schedule_mode_transition(cpu, channel, mode)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        return;
    }
    raw_write_word(cpu, can_bases[channel],
                   (uint16_t)((control & ~0x00e0u) | ((uint16_t)mode << 5u)));
    if (mode == CAN_MODE_CONFIGURATION) {
        raw_write_word(cpu, (uint16_t)(can_bases[channel] + 0x0eu), 0u);
        raw_write_word(cpu, (uint16_t)(can_bases[channel] + 0x0au),
                       (uint16_t)(raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) & 0x00ffu));
        can_refresh_error_status(cpu, channel);
    }
    if (mode == CAN_MODE_NORMAL || mode == CAN_MODE_LOOPBACK) {
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
    }
}

static void can_refresh_error_status(Dspic33* cpu, uint8_t channel) {
    uint16_t base = can_bases[channel];
    uint16_t counts = raw_word(cpu, (uint16_t)(base + 0x0eu));
    uint16_t status = raw_word(cpu, (uint16_t)(base + 0x0au));
    uint8_t receive = (uint8_t)counts;
    uint8_t transmit = (uint8_t)(counts >> 8u);
    bool bus_off = (status & CAN_BUS_OFF) != 0u;
    status &= 0x00ffu;
    if (receive >= 96u || transmit >= 96u) {
        status |= CAN_ERROR_WARNING;
    }
    if (receive >= 96u && receive < 128u) {
        status |= CAN_RECEIVE_WARNING;
    }
    if (transmit >= 96u && transmit < 128u) {
        status |= CAN_TRANSMIT_WARNING;
    }
    if (receive >= 128u) {
        status |= CAN_RECEIVE_PASSIVE;
    }
    if (transmit >= 128u && !bus_off) {
        status |= CAN_TRANSMIT_PASSIVE;
    }
    if (bus_off) {
        status |= CAN_BUS_OFF;
    }
    raw_write_word(cpu, (uint16_t)(base + 0x0au), status);
}

static void can_receive_start(Dspic33* cpu, uint8_t channel) {
    Dspic33CanFrame frame;
    uint8_t buffer;
    uint8_t filter;
    uint8_t bit = (uint8_t)(1u << channel);
    if ((cpu->io.can_rx_busy & bit) != 0u || !can_queue_pop(&cpu->io.can_rx[channel], &frame)) {
        return;
    }
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        if ((raw_word(cpu, (uint16_t)(can_bases[channel] + 0x12u)) & CAN_WAKE_FILTER) != 0u) {
            can_raise_event(cpu, channel, CAN_INTERRUPT_WAKE, 0u, 0u);
        }
        return;
    }
    if (!can_power_enabled(cpu, channel) || can_mode(cpu, channel) == CAN_MODE_DISABLE ||
        can_mode(cpu, channel) == CAN_MODE_CONFIGURATION) {
        return;
    }
    can_capture_received_frame(cpu, channel);
    if (can_mode(cpu, channel) == CAN_MODE_LISTEN_ALL) {
        bool fifo;
        bool transmit;
        filter = 0u;
        buffer = can_filter_buffer(cpu, channel, filter);
        fifo = buffer == 15u;
        if (fifo) {
            buffer = cpu->io.can_fifo_write[channel];
        }
        transmit =
            buffer < 8u && (can_buffer_control(cpu, channel, buffer) & CAN_BUFFER_TRANSMIT) != 0u;
        if (buffer >= can_buffer_count(cpu, channel) || transmit ||
            can_buffer_flag(cpu, channel, buffer, false)) {
            if (buffer < 32u) {
                can_set_buffer_flag(cpu, channel, buffer, true);
                can_raise_event(cpu, channel, CAN_INTERRUPT_OVERFLOW, buffer, filter);
                if (fifo) {
                    can_advance_fifo_write(cpu, channel, buffer);
                }
            }
            return;
        }
    } else if (!can_select_receive_buffer(cpu, channel, &frame, &buffer, &filter)) {
        return;
    }
    if (!can_dma_ready(cpu, can_rx_requests[channel], (uint16_t)(can_bases[channel] + 0x40u),
                       false)) {
        dspic33_raise_interrupt(cpu, can_rx_irqs[channel]);
        return;
    }
    can_encode_frame(&frame, filter, cpu->io.can_rx_words[channel]);
    cpu->io.can_rx_buffer[channel] = buffer;
    cpu->io.can_rx_filter[channel] = filter;
    cpu->io.can_rx_word[channel] = 0u;
    cpu->io.can_rx_busy |= bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_WORD, 0u);
}

static void can_receive_success_start(Dspic33* cpu, uint8_t channel) {
    can_receive_success(cpu, channel);
    can_receive_start(cpu, channel);
}

static void can_receive_word(Dspic33* cpu, uint8_t channel) {
    uint8_t word = cpu->io.can_rx_word[channel];
    if (word >= 8u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_FINISH, 0u);
        return;
    }
    cpu->io.can_rx_word[channel]++;
    dspic33_raise_interrupt(cpu, can_rx_irqs[channel]);
    dspic33_dma_request(cpu, can_rx_requests[channel],
                        (uint16_t)(cpu->io.can_rx_buffer[channel] * 16u + word * 2u), 0u);
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_WORD, 1u);
}

static void can_receive_finish(Dspic33* cpu, uint8_t channel) {
    uint8_t buffer = cpu->io.can_rx_buffer[channel];
    uint8_t filter = cpu->io.can_rx_filter[channel];
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t next;
    uint16_t control = raw_word(cpu, (uint16_t)(can_bases[channel] + 6u));
    uint16_t fifo;
    can_set_buffer_flag(cpu, channel, buffer, false);
    if (can_filter_buffer(cpu, channel, filter) == 15u) {
        next = can_advance_fifo_write(cpu, channel, buffer);
        fifo = raw_word(cpu, (uint16_t)(can_bases[channel] + 8u));
        if ((fifo & 0x003fu) == next + 1u ||
            (((fifo & 0x003fu) == (control & 0x001fu)) && next == can_fifo_end(cpu, channel))) {
            can_raise_event(cpu, channel, CAN_INTERRUPT_FIFO, buffer, filter);
        }
    }
    can_raise_event(cpu, channel, CAN_INTERRUPT_RECEIVE, buffer, filter);
    cpu->io.can_rx_busy &= (uint8_t)~bit;
    if (cpu->io.can_rx[channel].count != 0u) {
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u);
    }
}

static int can_transmit_selection(const Dspic33* cpu, uint8_t channel) {
    int selected = -1;
    uint8_t priority = 0u;
    uint8_t buffer;
    for (buffer = 0u; buffer < 8u; buffer++) {
        uint16_t control = can_buffer_control(cpu, channel, buffer);
        uint8_t current_priority = (uint8_t)(control & 3u);
        if ((control & (CAN_BUFFER_TRANSMIT | CAN_BUFFER_REQUEST)) !=
            (CAN_BUFFER_TRANSMIT | CAN_BUFFER_REQUEST)) {
            continue;
        }
        if (selected < 0 || current_priority > priority ||
            (current_priority == priority && buffer > (uint8_t)selected)) {
            selected = buffer;
            priority = current_priority;
        }
    }
    return selected;
}

static void can_transmit_start(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    int selected;
    if ((cpu->io.can_tx_busy & bit) != 0u || (cpu->io.can_tx_retry_wait & bit) != 0u ||
        !can_power_enabled(cpu, channel) ||
        (can_mode(cpu, channel) != CAN_MODE_NORMAL &&
         can_mode(cpu, channel) != CAN_MODE_LOOPBACK) ||
        (raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) & CAN_BUS_OFF) != 0u) {
        return;
    }
    selected = can_transmit_selection(cpu, channel);
    if (selected < 0) {
        return;
    }
    if (!can_dma_ready(cpu, can_tx_requests[channel], (uint16_t)(can_bases[channel] + 0x42u),
                       true)) {
        dspic33_raise_interrupt(cpu, can_tx_irqs[channel]);
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
    dspic33_raise_interrupt(cpu, can_tx_irqs[channel]);
    dspic33_dma_request(cpu, can_tx_requests[channel],
                        (uint16_t)(cpu->io.can_tx_buffer[channel] * 16u + word * 2u), 0u);
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_WORD, 1u);
}

static void can_transmit_bus_finish(Dspic33* cpu, uint8_t channel) {
    uint8_t buffer = cpu->io.can_tx_buffer[channel];
    uint8_t bit = (uint8_t)(1u << channel);
    uint16_t control = can_buffer_control(cpu, channel, buffer);
    uint16_t counts = raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0eu));
    Dspic33CanFrame frame = can_decode_frame(cpu->io.can_tx_words[channel]);
    can_set_buffer_control(cpu, channel, buffer, (uint16_t)(control & ~CAN_BUFFER_REQUEST));
    if ((counts >> 8u) != 0u) {
        counts = (uint16_t)(counts - 0x0100u);
        raw_write_word(cpu, (uint16_t)(can_bases[channel] + 0x0eu), counts);
        can_refresh_error_status(cpu, channel);
    }
    can_raise_event(cpu, channel, CAN_INTERRUPT_TRANSMIT, buffer, 0u);
    if (can_mode(cpu, channel) == CAN_MODE_LOOPBACK) {
        can_queue_push(&cpu->io.can_rx[channel], &frame);
        dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_RECEIVE_START, 0u);
    } else {
        can_queue_push(&cpu->io.can_tx[channel], &frame);
    }
    cpu->io.can_tx_on_bus &= (uint8_t)~bit;
    cpu->io.can_tx_busy &= (uint8_t)~bit;
    dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_START, 0u);
}

static void can_transmit_finish(Dspic33* cpu, uint8_t channel) {
    Dspic33CanFrame frame = can_decode_frame(cpu->io.can_tx_words[channel]);
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.can_tx_start_cycle[channel] = cpu->device_cycles;
    cpu->io.can_tx_phase_adjustment[channel] = 0;
    cpu->io.can_tx_on_bus |= bit;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_BUS_FINISH,
                          can_frame_cycles(cpu, channel, &frame)) ||
        !can_schedule_transmit_sample(cpu, channel, can_first_sample_delay(cpu, channel))) {
        can_remove_transmit_events(cpu, channel);
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
        can_monitor_transmit_sample(cpu, channel, bus_high);
    }
    uint64_t delay = can_bit_cycles(cpu, channel);
    if (can_triple_sample(cpu, channel)) {
        delay -= 2u * can_time_quantum(cpu, channel);
    }
    if ((cpu->io.can_tx_on_bus & bit) != 0u && !can_schedule_transmit_sample(cpu, channel, delay)) {
        can_remove_transmit_events(cpu, channel);
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
                          can_time_quantum(cpu, channel))) {
        can_remove_transmit_events(cpu, channel);
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
                          can_time_quantum(cpu, channel))) {
        can_remove_transmit_events(cpu, channel);
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
    uint64_t bits =
        (raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) & CAN_TRANSMIT_PASSIVE) != 0u ? 25u
                                                                                             : 17u;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CAN, channel, CAN_EVENT_TRANSMIT_RETRY,
                          bits * can_bit_cycles(cpu, channel))) {
        cpu->io.can_tx_error_active &= (uint8_t)~bit;
        cpu->io.can_tx_retry_wait &= (uint8_t)~bit;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void can_error_event(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint16_t address = (uint16_t)(can_bases[channel] + 0x0eu);
    uint16_t counts = raw_word(cpu, address);
    uint16_t increment = (uint16_t)(value >> CAN_EVENT_ERROR_COUNT_SHIFT);
    if ((value & CAN_EVENT_TRANSMIT_ERROR) != 0u) {
        uint16_t transmit = (uint16_t)(counts >> 8u);
        uint32_t total = (uint32_t)transmit + increment;
        if (total > 0xffu) {
            raw_write_word(
                cpu, (uint16_t)(can_bases[channel] + 0x0au),
                (uint16_t)(raw_word(cpu, (uint16_t)(can_bases[channel] + 0x0au)) | CAN_BUS_OFF));
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
    raw_write_word(cpu, address, counts);
    can_refresh_error_status(cpu, channel);
}

static void can_invalid_event(Dspic33* cpu, uint8_t channel) {
    uint8_t mode = can_mode(cpu, channel);
    if (!can_power_enabled(cpu, channel) || cpu->power_state == DSPIC33_POWER_SLEEP ||
        mode == CAN_MODE_DISABLE || mode == CAN_MODE_CONFIGURATION) {
        return;
    }
    can_raise_event(cpu, channel, CAN_INTERRUPT_INVALID, 0u, 0u);
}

static void run_can(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint32_t kind = value & CAN_EVENT_KIND_MASK;
    if (kind == CAN_EVENT_RECEIVE_PIN) {
        apply_physical_pin_level(cpu, channel, (value & CAN_EVENT_PIN_HIGH) != 0u);
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
        input_capture_level(cpu, 1u, false);
        break;
    case CAN_EVENT_INVALID:
        can_invalid_event(cpu, channel);
        break;
    case CAN_EVENT_RECEIVE_SAMPLE:
        if (can_triple_sample(cpu, channel)) {
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
        if (can_triple_sample(cpu, channel)) {
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
        can_intermission_finish(cpu, channel, value);
        break;
    case CAN_EVENT_OVERLOAD_FINISH:
        can_overload_finish(cpu, channel, value);
        break;
    case CAN_EVENT_RECEIVE_SUCCESS:
        can_receive_success_start(cpu, channel);
        break;
    case CAN_EVENT_ERROR:
        can_error_event(cpu, channel, value);
        break;
    }
}
