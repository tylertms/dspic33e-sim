#include "device/dspic33ep_mu/communication/can/internal.h"

const uint16_t bases[DSPIC33_CAN_COUNT] = {0x0400u, 0x0500u};
const uint8_t event_irqs[DSPIC33_CAN_COUNT] = {35u, 56u};
const uint8_t receive_requests[DSPIC33_CAN_COUNT] = {34u, 55u};
const uint8_t transmit_requests[DSPIC33_CAN_COUNT] = {70u, 71u};

bool dspic33_can_test_interrupt_flag(Dspic33* cpu, uint8_t interrupt_number) {
    const uint16_t status_address = (uint16_t)(0x0800u + (interrupt_number / 16u) * 2u);

    return (dspic33_read_word(cpu, status_address) & (uint16_t)(1u << (interrupt_number % 16u))) !=
           0u;
}

void dspic33_can_test_clear_interrupt_flag(Dspic33* cpu, uint8_t interrupt_number) {
    const uint16_t status_address = (uint16_t)(0x0800u + (interrupt_number / 16u) * 2u);
    const uint16_t interrupt_mask = (uint16_t)(1u << (interrupt_number % 16u));

    dspic33_write_word(cpu, status_address,
                       (uint16_t)(dspic33_read_word(cpu, status_address) & ~interrupt_mask));
}

static uint16_t dma_channel_base(uint8_t channel_index) {
    return (uint16_t)(0x0b00u + channel_index * 0x10u);
}

static void configure_dma(Dspic33* cpu, uint8_t channel_index, uint16_t dma_control,
                          uint8_t request_source, uint32_t memory_address,
                          uint16_t peripheral_address) {
    const uint16_t channel_base = dma_channel_base(channel_index);

    dspic33_write_word(cpu, channel_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 2u), request_source);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 4u), (uint16_t)memory_address);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 6u), (uint16_t)(memory_address >> 16u));
    dspic33_write_word(cpu, (uint16_t)(channel_base + 0x0cu), peripheral_address);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 0x0eu), 7u);
    dspic33_write_word(cpu, channel_base, (uint16_t)(0x8000u | dma_control));
}

void dspic33_can_test_select_window(Dspic33* cpu, uint8_t channel_index, bool filter_window) {
    const uint16_t can_base = bases[channel_index];
    const uint16_t can_control = dspic33_read_word(cpu, can_base);

    dspic33_write_word(cpu, can_base,
                       filter_window ? (uint16_t)(can_control | 1u)
                                     : (uint16_t)(can_control & ~1u));
}

uint64_t dspic33_can_test_mode_transition_cycles(Dspic33* cpu, uint8_t channel_index) {
    const uint16_t can_base = bases[channel_index];
    const uint16_t configuration_one = dspic33_read_word(cpu, (uint16_t)(can_base + 0x10u));
    const uint16_t configuration_two = dspic33_read_word(cpu, (uint16_t)(can_base + 0x12u));
    const uint16_t can_control = dspic33_read_word(cpu, can_base);
    const uint64_t prescaler = (configuration_one & 0x003fu) + 1u;
    const uint64_t time_quanta = 1u + (configuration_two & 7u) + 1u +
                                 ((configuration_two >> 3u) & 7u) + 1u +
                                 ((configuration_two >> 8u) & 7u) + 1u;
    const uint64_t clock_divisor = (can_control & 0x0800u) != 0u ? 2u : 1u;

    return 11u * prescaler * time_quanta * clock_divisor;
}

void dspic33_can_test_request_mode(Dspic33* cpu, uint8_t channel_index, uint8_t requested_mode) {
    const uint16_t can_base = bases[channel_index];
    uint16_t can_control = dspic33_read_word(cpu, can_base);

    can_control = (uint16_t)((can_control & ~0x0700u) | ((uint16_t)requested_mode << 8u));
    dspic33_write_word(cpu, can_base, can_control);
}

void dspic33_can_test_set_mode(Dspic33* cpu, uint8_t channel_index, uint8_t requested_mode) {
    dspic33_can_test_request_mode(cpu, channel_index, requested_mode);
    dspic33_device_advance(cpu, dspic33_can_test_mode_transition_cycles(cpu, channel_index));
}

void dspic33_can_test_configure_filter(Dspic33* cpu, uint8_t channel_index, uint8_t filter_index,
                                       uint32_t identifier, bool extended_identifier, uint32_t mask,
                                       bool mask_match_type, uint8_t buffer_index,
                                       uint8_t mask_index) {
    const uint16_t can_base = bases[channel_index];
    const uint32_t standard_identifier =
        extended_identifier ? (identifier >> 18u) & 0x7ffu : identifier & 0x7ffu;
    const uint32_t extended_identifier_bits = identifier & 0x3ffffu;
    const uint32_t mask_standard_identifier =
        extended_identifier ? (mask >> 18u) & 0x7ffu : mask & 0x7ffu;
    const uint32_t mask_extended_identifier = mask & 0x3ffffu;
    uint16_t filter_standard_id =
        (uint16_t)((standard_identifier << 5u) | ((extended_identifier_bits >> 16u) & 3u));
    uint16_t receive_mask =
        (uint16_t)((mask_standard_identifier << 5u) | ((mask_extended_identifier >> 16u) & 3u));
    uint16_t register_address;
    uint16_t register_value;

    if (extended_identifier) {
        filter_standard_id |= 8u;
    }
    if (mask_match_type) {
        receive_mask |= 8u;
    }
    dspic33_can_test_select_window(cpu, channel_index, true);
    dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u + mask_index * 4u), receive_mask);
    dspic33_write_word(cpu, (uint16_t)(can_base + 0x32u + mask_index * 4u),
                       (uint16_t)mask_extended_identifier);
    dspic33_write_word(cpu, (uint16_t)(can_base + 0x40u + filter_index * 4u), filter_standard_id);
    dspic33_write_word(cpu, (uint16_t)(can_base + 0x42u + filter_index * 4u),
                       (uint16_t)extended_identifier_bits);

    register_address = (uint16_t)(can_base + (filter_index < 8u ? 0x18u : 0x1au));
    register_value = dspic33_read_word(cpu, register_address);
    register_value = (uint16_t)((register_value & ~(uint16_t)(3u << ((filter_index & 7u) * 2u))) |
                                ((uint16_t)mask_index << ((filter_index & 7u) * 2u)));
    dspic33_write_word(cpu, register_address, register_value);

    register_address = (uint16_t)(can_base + 0x20u + (filter_index / 4u) * 2u);
    register_value = dspic33_read_word(cpu, register_address);
    register_value =
        (uint16_t)((register_value & ~(uint16_t)(0x0fu << ((filter_index & 3u) * 4u))) |
                   ((uint16_t)buffer_index << ((filter_index & 3u) * 4u)));
    dspic33_write_word(cpu, register_address, register_value);
}

void dspic33_can_test_enable_filter(Dspic33* cpu, uint8_t channel_index, uint16_t enable_mask) {
    dspic33_write_word(cpu, (uint16_t)(bases[channel_index] + 0x14u), enable_mask);
}

void dspic33_can_test_configure_receive(Dspic33* cpu, uint8_t channel_index,
                                        uint32_t memory_address, uint8_t dma_buffer_size,
                                        uint8_t fifo_start_index) {
    const uint16_t can_base = bases[channel_index];

    configure_dma(cpu, (uint8_t)(channel_index * 2u), 0x0020u, receive_requests[channel_index],
                  memory_address, (uint16_t)(can_base + 0x40u));
    dspic33_write_byte(cpu, (uint16_t)(can_base + 6u), fifo_start_index);
    dspic33_write_byte(cpu, (uint16_t)(can_base + 7u), (uint8_t)(dma_buffer_size << 5u));
}

void dspic33_can_test_fifo_control_write_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        const uint16_t control_address = (uint16_t)(bases[channel_index] + 6u);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, control_address, 0xa012u);
        expect(state, dspic33_read_word(cpu, control_address) == 0u,
               "CAN FIFO control rejects a word write");
        dspic33_write_byte(cpu, (uint16_t)(control_address + 1u), 0xa0u);
        expect(state, dspic33_read_word(cpu, control_address) == 0u,
               "CAN FIFO control rejects DMABS before FSA");
        dspic33_write_byte(cpu, control_address, 0x12u);
        expect(state, dspic33_read_word(cpu, control_address) == 0x0012u,
               "CAN FIFO control accepts FSA first");
        dspic33_write_byte(cpu, (uint16_t)(control_address + 1u), 0xa0u);
        expect(state,
               dspic33_read_word(cpu, control_address) == 0xa012u &&
                   cpu->io.can_fifo_write[channel_index] == 0x12u &&
                   (cpu->io.can_fctrl_fsa_ready & (uint8_t)(1u << channel_index)) == 0u,
               "CAN FIFO control accepts DMABS after FSA");
    }
}

void dspic33_can_test_register_groups(TestState* state, Dspic33* cpu) {
    dspic33_can_test_register_cases(state, cpu);
    dspic33_can_test_register_access_cases(state, cpu);
    dspic33_can_test_fifo_control_write_cases(state, cpu);
    dspic33_can_test_interrupt_flag_write_zero_cases(state, cpu);
    dspic33_can_test_standard_filter_domain(state, cpu);
    dspic33_can_test_extended_filter_cases(state, cpu);
    dspic33_can_test_payload_and_remote_cases(state, cpu);
    dspic33_can_test_devicenet_cases(state, cpu);
    dspic33_can_test_direct_buffer_cases(state, cpu);
    dspic33_can_test_fifo_cases(state, cpu);
    dspic33_can_test_fifo_overflow_advancement_cases(state, cpu);
    dspic33_can_test_receive_flag_write_zero_domain(state, cpu);
}

void dspic33_can_test_configure_transmit(Dspic33* cpu, uint8_t channel_index,
                                         uint32_t memory_address) {
    configure_dma(cpu, (uint8_t)(channel_index * 2u + 1u), 0x2020u,
                  transmit_requests[channel_index], memory_address,
                  (uint16_t)(bases[channel_index] + 0x42u));
}

Dspic33CanFrame dspic33_can_test_frame(uint32_t identifier, bool extended, bool remote,
                                       uint8_t length, uint8_t data_seed) {
    Dspic33CanFrame frame;

    memset(&frame, 0, sizeof(frame));
    frame.identifier = identifier;
    frame.extended = extended;
    frame.remote = remote;
    frame.length = length;
    for (uint8_t byte_index = 0u; byte_index < length && byte_index < sizeof(frame.data);
         byte_index++) {
        frame.data[byte_index] = (uint8_t)(data_seed + byte_index * 17u);
    }
    return frame;
}

void dspic33_can_test_write_transmit_frame(Dspic33* cpu, uint32_t memory_address,
                                           const Dspic33CanFrame* frame) {
    uint16_t frame_words[8] = {0};
    const uint32_t standard_identifier =
        frame->extended ? (frame->identifier >> 18u) & 0x7ffu : frame->identifier;
    const uint32_t extended_identifier = frame->identifier & 0x3ffffu;

    frame_words[0] = (uint16_t)(standard_identifier << 2u);
    if (frame->extended) {
        frame_words[0] |= 3u;
        frame_words[1] = (uint16_t)(extended_identifier >> 6u);
        frame_words[2] = (uint16_t)((extended_identifier & 0x3fu) << 10u);
        if (frame->remote) {
            frame_words[2] |= 0x0200u;
        }
    } else if (frame->remote) {
        frame_words[0] |= 2u;
    }
    frame_words[2] |= frame->length;
    for (uint8_t byte_index = 0u; byte_index < frame->length; byte_index++) {
        frame_words[3u + byte_index / 2u] |= (uint16_t)frame->data[byte_index]
                                             << ((byte_index & 1u) * 8u);
    }
    for (uint8_t word_index = 0u; word_index < 8u; word_index++) {
        dspic33_can_test_write_memory_word(cpu, memory_address + word_index * 2u,
                                           frame_words[word_index]);
    }
}

static void clear_receive_flag(Dspic33* cpu, uint8_t channel_index, uint8_t buffer_index) {
    const uint16_t status_address =
        (uint16_t)(bases[channel_index] + 0x20u + (buffer_index >= 16u ? 2u : 0u));
    const uint16_t status_value = dspic33_read_word(cpu, status_address);

    dspic33_write_word(cpu, status_address,
                       (uint16_t)(status_value & ~(uint16_t)(1u << (buffer_index & 15u))));
}

bool dspic33_can_test_receive_full(Dspic33* cpu, uint8_t channel_index, uint8_t buffer_index) {
    const uint16_t status_address =
        (uint16_t)(bases[channel_index] + 0x20u + (buffer_index >= 16u ? 2u : 0u));

    return (dspic33_read_word(cpu, status_address) & (uint16_t)(1u << (buffer_index & 15u))) != 0u;
}

uint16_t dspic33_can_test_memory_word(Dspic33* cpu, uint32_t memory_address) {
    return (uint16_t)(cpu->data[memory_address] | ((uint16_t)cpu->data[memory_address + 1u] << 8u));
}

void dspic33_can_test_write_memory_word(Dspic33* cpu, uint32_t memory_address,
                                        uint16_t word_value) {
    cpu->data[memory_address] = (uint8_t)word_value;
    cpu->data[memory_address + 1u] = (uint8_t)(word_value >> 8u);
}

void dspic33_can_test_register_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        const uint16_t can_base = bases[channel_index];

        expect(state, dspic33_read_word(cpu, can_base) == 0x0480u, "control reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 4u)) == 0x0040u, "vector reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x14u)) == 0x003fu,
               "filter enable reset");
        dspic33_write_word(cpu, (uint16_t)(can_base + 2u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 2u)) == 0x001fu,
               "control two mask");
        dspic33_write_byte(cpu, (uint16_t)(can_base + 6u), 0xffu);
        dspic33_write_byte(cpu, (uint16_t)(can_base + 7u), 0xffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 6u)) == 0xe01fu,
               "fifo control byte masks");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x0cu), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x0cu)) == 0x00efu,
               "interrupt enable mask");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x10u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x10u)) == 0x00ffu,
               "baud one mask");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x12u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x12u)) == 0x47ffu,
               "baud two mask");
        dspic33_can_test_select_window(cpu, channel_index, true);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x40u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x40u)) == 0xffebu,
               "filter SID mask");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x42u), 0xa55au);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x42u)) == 0xa55au,
               "filter EID write");
        dspic33_can_test_select_window(cpu, channel_index, false);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x40u)) == 0u,
               "window isolation");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x30u)) == 0x8f8fu,
               "buffer control mask");
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        expect(state, (dspic33_read_word(cpu, can_base) & 0x00e0u) == 0u,
               "normal mode acknowledgement");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x10u), 0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(can_base + 0x10u)) == 0x00ffu,
               "baud locked outside configuration");
        dspic33_can_test_set_mode(cpu, channel_index, 4u);
    }
}

void dspic33_can_test_register_access_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t received_words[] = {0x0c84u, 0x0000u, 0x0004u, 0x5140u,
                                              0x7362u, 0x0000u, 0x0000u, 0x0000u};
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        const uint16_t can_base = bases[channel_index];
        const uint16_t receive_data_address = (uint16_t)(can_base + 0x40u);
        const uint32_t memory_address = (uint32_t)(0xd600u + channel_index * 0x100u);
        const Dspic33CanFrame received_frame =
            dspic33_can_test_frame(0x321u, false, false, 4u, 0x40u);
        bool error_counters_preserved;
        bool transfer_started;
        bool dma_words_match;

        dspic33_reset(cpu, 0u);
        dspic33_can_test_write_memory_word(cpu, (uint16_t)(can_base + 0x0eu), 0x5aa5u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x0eu), 0u);
        error_counters_preserved = dspic33_read_word(cpu, (uint16_t)(can_base + 0x0eu)) == 0x5aa5u;
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x0eu), 0xffffu);
        expect(state,
               error_counters_preserved &&
                   dspic33_read_word(cpu, (uint16_t)(can_base + 0x0eu)) == 0x5aa5u,
               "error counters reject CPU writes");

        dspic33_write_word(cpu, can_base,
                           (uint16_t)((dspic33_read_word(cpu, can_base) & ~0x07e0u) | 0x02e0u));
        expect(state,
               (dspic33_read_word(cpu, can_base) & 0x07e0u) == 0x0280u &&
                   dspic33_device_advance(
                       cpu, dspic33_can_test_mode_transition_cycles(cpu, channel_index)) &&
                   (dspic33_read_word(cpu, can_base) & 0x07e0u) == 0x0240u,
               "requested mode controls operating mode");
        dspic33_write_word(cpu, can_base, (uint16_t)(dspic33_read_word(cpu, can_base) | 0x00a0u));
        expect(state, (dspic33_read_word(cpu, can_base) & 0x07e0u) == 0x0240u,
               "operating mode rejects direct writes");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x0cu), 1u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x8989u);
        dspic33_write_word(cpu, can_base, (uint16_t)(dspic33_read_word(cpu, can_base) | 0x1000u));
        expect(state, (dspic33_read_word(cpu, (uint16_t)(can_base + 0x30u)) & 0x4848u) == 0x4040u,
               "abort all marks pending transmissions aborted");
        expect(state,
               (dspic33_read_word(cpu, can_base) & 0x1000u) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(can_base + 0x0au)) & 1u) != 0u &&
                   dspic33_can_test_interrupt_flag(cpu, event_irqs[channel_index]),
               "abort all self clears and raises transmit event");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel_index, memory_address, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, channel_index, 0u, 0x321u, false, 0x7ffu, true, 0u,
                                          0u);
        dspic33_can_test_enable_filter(cpu, channel_index, 1u);
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        dspic33_write_word(cpu, receive_data_address, 0xa55au);
        expect(state, dspic33_read_word(cpu, receive_data_address) == 0xa55au,
               "receive data CPU word access");
        dspic33_write_byte(cpu, receive_data_address, 0x3cu);
        dspic33_write_byte(cpu, (uint16_t)(receive_data_address + 1u), 0xc3u);
        expect(state, dspic33_read_word(cpu, receive_data_address) == 0xc33cu,
               "receive data CPU byte access");
        transfer_started = dspic33_can_receive(cpu, channel_index, &received_frame, 0u) &&
                           dspic33_device_advance(cpu, 0u);
        expect(state,
               transfer_started && (cpu->io.can_rx_busy & (uint8_t)(1u << channel_index)) != 0u &&
                   dspic33_read_word(cpu, receive_data_address) == received_words[0] &&
                   dspic33_can_test_memory_word(cpu, memory_address) == received_words[0],
               "receive stream overrides CPU backing for DMA");
        dspic33_write_word(cpu, receive_data_address, 0xc55cu);
        expect(state, dspic33_read_word(cpu, receive_data_address) == received_words[0],
               "receive stream survives concurrent CPU write");
        expect(state, dspic33_device_advance(cpu, 32u), "receive stream completion advance");
        dma_words_match = true;
        for (uint8_t word_index = 0u; word_index < 8u; word_index++) {
            dma_words_match = dma_words_match &&
                              dspic33_can_test_memory_word(cpu, memory_address + word_index * 2u) ==
                                  received_words[word_index];
        }
        expect(state, dma_words_match, "receive stream DMA words");
        expect(state,
               (cpu->io.can_rx_busy & (uint8_t)(1u << channel_index)) == 0u &&
                   dspic33_read_word(cpu, receive_data_address) == 0xc55cu,
               "receive data backing returns after stream");
    }
}

void dspic33_can_test_interrupt_flag_write_zero_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t previous_words[] = {0x00efu, 0x0000u, 0x00a5u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t address = (uint16_t)(bases[channel] + 0x0au);
        uint8_t previous_index;
        dspic33_reset(cpu, 0u);
        for (previous_index = 0u;
             previous_index < sizeof(previous_words) / sizeof(previous_words[0]);
             previous_index++) {
            uint16_t previous = previous_words[previous_index];
            uint32_t requested;
            for (requested = 0u; requested <= UINT16_MAX; requested += 257u) {
                dspic33_can_test_write_memory_word(cpu, address, previous);
                dspic33_write_word(cpu, address, (uint16_t)requested);
                expect(state, dspic33_read_word(cpu, address) == (uint16_t)(previous & requested),
                       "interrupt flag write-zero clear domain");
            }
        }
    }
}

void dspic33_can_test_standard_filter_domain(TestState* state, Dspic33* cpu) {
    const uint16_t expected_identifier = 0x5a0u;
    const uint16_t identifier_mask = 0x7f0u;
    Dspic33CanFrame received_frame;

    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, 0u, 0x2000u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, expected_identifier, false, identifier_mask,
                                      true, 0u, 0u);
    dspic33_can_test_enable_filter(cpu, 0u, 1u);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    for (uint16_t identifier_value = 0u; identifier_value <= 0x7ffu; identifier_value++) {
        const bool filter_match =
            (identifier_value & identifier_mask) == (expected_identifier & identifier_mask);

        received_frame =
            dspic33_can_test_frame(identifier_value, false, false, 2u, (uint8_t)identifier_value);
        expect(state, dspic33_can_receive(cpu, 0u, &received_frame, 0u),
               "standard domain schedule");
        expect(state, dspic33_device_advance(cpu, 32u), "standard domain advance");
        expect(state, dspic33_can_test_receive_full(cpu, 0u, 0u) == filter_match,
               "standard domain filter result");
        if (filter_match) {
            expect(state,
                   dspic33_can_test_memory_word(cpu, 0x2000u) == (uint16_t)(identifier_value << 2u),
                   "standard domain identifier storage");
            clear_receive_flag(cpu, 0u, 0u);
        }
    }
}

void dspic33_can_test_extended_filter_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t identifiers[] = {0u,         1u,          0x3ffffu,    0x40000u,
                                           0x1ffffffu, 0x10000000u, 0x15555555u, 0x1fffffffu};
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        for (size_t identifier_index = 0u;
             identifier_index < sizeof(identifiers) / sizeof(identifiers[0]); identifier_index++) {
            const uint32_t identifier_value = identifiers[identifier_index];
            const Dspic33CanFrame received_frame = dspic33_can_test_frame(
                identifier_value, true, false, 8u, (uint8_t)identifier_index);

            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_receive(cpu, channel_index, 0x3000u, 6u, 0u);
            dspic33_can_test_configure_filter(cpu, channel_index, 3u, identifier_value, true,
                                              0x1fffffffu, true, 6u, 1u);
            dspic33_can_test_enable_filter(cpu, channel_index, 1u << 3u);
            dspic33_can_test_select_window(cpu, channel_index, false);
            dspic33_can_test_set_mode(cpu, channel_index, 0u);
            expect(state,
                   dspic33_can_receive(cpu, channel_index, &received_frame, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "extended exact transfer");
            expect(state, dspic33_can_test_receive_full(cpu, channel_index, 6u),
                   "extended exact full flag");
            expect(state,
                   (((uint32_t)(dspic33_can_test_memory_word(cpu, 0x3060u) >> 2u) & 0x7ffu)
                    << 18u) == (identifier_value & 0x1ffc0000u),
                   "extended SID storage");
            expect(state,
                   (((uint32_t)(dspic33_can_test_memory_word(cpu, 0x3062u) & 0x0fffu) << 6u) |
                    (dspic33_can_test_memory_word(cpu, 0x3064u) >> 10u)) ==
                       (identifier_value & 0x3ffffu),
                   "extended EID storage");
        }

        Dspic33CanFrame standard_frame = dspic33_can_test_frame(0x456u, false, false, 0u, 0u);
        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel_index, 0x3000u, 6u, 0u);
        dspic33_can_test_configure_filter(cpu, channel_index, 0u, 0x456u, true, 0x1fffffffu, true,
                                          0u, 0u);
        dspic33_can_test_enable_filter(cpu, channel_index, 1u);
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        expect(state,
               dspic33_can_receive(cpu, channel_index, &standard_frame, 0u) &&
                   dspic33_device_advance(cpu, 32u) &&
                   !dspic33_can_test_receive_full(cpu, channel_index, 0u),
               "filter format matching rejects a standard frame for an extended filter");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel_index, 0x3000u, 6u, 0u);
        dspic33_can_test_configure_filter(cpu, channel_index, 0u, 0x456u, false, 0x7ffu, true, 0u,
                                          3u);
        dspic33_can_test_enable_filter(cpu, channel_index, 1u);
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        expect(state,
               dspic33_can_receive(cpu, channel_index, &standard_frame, 0u) &&
                   dspic33_device_advance(cpu, 32u) &&
                   !dspic33_can_test_receive_full(cpu, channel_index, 0u),
               "invalid CAN mask selection rejects a frame");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel_index, 0x3000u, 2u, 0u);
        dspic33_can_test_configure_filter(cpu, channel_index, 0u, 0x456u, false, 0x7ffu, true, 14u,
                                          0u);
        dspic33_can_test_enable_filter(cpu, channel_index, 1u);
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        expect(state,
               dspic33_can_receive(cpu, channel_index, &standard_frame, 0u) &&
                   dspic33_device_advance(cpu, 32u) &&
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel_index] + 0x0au)) & 0x0004u) !=
                       0u,
               "CAN reports overflow for a filter target outside the configured buffer range");
    }
}

void dspic33_can_test_payload_and_remote_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        for (uint8_t extended_format = 0u; extended_format < 2u; extended_format++) {
            for (uint8_t remote_frame = 0u; remote_frame < 2u; remote_frame++) {
                for (uint8_t payload_length = 0u; payload_length <= 8u; payload_length++) {
                    const uint32_t identifier_value = extended_format != 0u ? 0x1234567u : 0x567u;
                    const Dspic33CanFrame received_frame = dspic33_can_test_frame(
                        identifier_value, extended_format != 0u, remote_frame != 0u, payload_length,
                        (uint8_t)(0x20u + payload_length));

                    dspic33_reset(cpu, 0u);
                    dspic33_can_test_configure_receive(cpu, channel_index, 0x4000u, 4u, 0u);
                    dspic33_can_test_configure_filter(
                        cpu, channel_index, 0u, identifier_value, extended_format != 0u,
                        extended_format != 0u ? 0x1fffffffu : 0x7ffu, true, 2u, 0u);
                    dspic33_can_test_enable_filter(cpu, channel_index, 1u);
                    dspic33_can_test_select_window(cpu, channel_index, false);
                    dspic33_can_test_set_mode(cpu, channel_index, 0u);
                    expect(state,
                           dspic33_can_receive(cpu, channel_index, &received_frame, 0u) &&
                               dspic33_device_advance(cpu, 32u),
                           "payload transfer");
                    expect(state,
                           (dspic33_can_test_memory_word(cpu, 0x4024u) & 0x0fu) == payload_length,
                           "payload length");
                    for (uint8_t byte_index = 0u; byte_index < payload_length; byte_index++) {
                        const uint16_t packed_word = dspic33_can_test_memory_word(
                            cpu, (uint32_t)(0x4026u + (byte_index / 2u) * 2u));

                        expect(state,
                               (uint8_t)(packed_word >> ((byte_index & 1u) * 8u)) ==
                                   received_frame.data[byte_index],
                               "payload byte");
                    }
                    expect(state,
                           extended_format != 0u
                               ? ((dspic33_can_test_memory_word(cpu, 0x4024u) & 0x0200u) != 0u) ==
                                     (remote_frame != 0u)
                               : ((dspic33_can_test_memory_word(cpu, 0x4020u) & 2u) != 0u) ==
                                     (remote_frame != 0u),
                           "remote encoding");
                }
            }
        }
    }
}

void dspic33_can_test_devicenet_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t qualification_bits = 1u; qualification_bits <= 31u; qualification_bits++) {
        for (uint8_t payload_length = 0u; payload_length <= 3u; payload_length++) {
            const Dspic33CanFrame received_frame =
                dspic33_can_test_frame(0x321u, false, false, payload_length, 0xa5u);
            uint32_t payload_bits = 0u;
            uint8_t compared_bits = qualification_bits > 18u ? 18u : qualification_bits;

            if (compared_bits > payload_length * 8u) {
                compared_bits = (uint8_t)(payload_length * 8u);
            }
            if (payload_length > 0u) {
                payload_bits = (uint32_t)received_frame.data[0] << 16u;
            }
            if (payload_length > 1u) {
                payload_bits |= (uint32_t)received_frame.data[1] << 8u;
            }
            if (payload_length > 2u) {
                payload_bits |= received_frame.data[2];
            }
            payload_bits = compared_bits == 0u
                               ? 0u
                               : (payload_bits >> (24u - compared_bits)) << (18u - compared_bits);

            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_receive(cpu, 0u, 0x5000u, 4u, 0u);
            dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x321u, false, 0x7ffu, true, 0u, 0u);
            dspic33_can_test_select_window(cpu, 0u, true);
            dspic33_write_word(cpu, 0x0430u, (uint16_t)((0x7ffu << 5u) | 8u));
            dspic33_write_word(cpu, 0x0440u, (uint16_t)((0x321u << 5u) | (payload_bits >> 16u)));
            dspic33_write_word(cpu, 0x0442u, (uint16_t)payload_bits);
            dspic33_can_test_enable_filter(cpu, 0u, 1u);
            dspic33_write_word(cpu, 0x0402u, qualification_bits);
            dspic33_can_test_select_window(cpu, 0u, false);
            dspic33_can_test_set_mode(cpu, 0u, 0u);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &received_frame, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "DeviceNet matching transfer");
            expect(state, dspic33_can_test_receive_full(cpu, 0u, 0u), "DeviceNet match");
        }
    }
}

void dspic33_can_test_direct_buffer_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        for (uint8_t buffer_index = 0u; buffer_index <= 14u; buffer_index++) {
            const Dspic33CanFrame received_frame = dspic33_can_test_frame(
                (uint32_t)(0x200u + buffer_index), false, false, 4u, buffer_index);

            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_receive(cpu, channel_index, 0x6000u, 6u, 0u);
            dspic33_can_test_configure_filter(cpu, channel_index, 0u, received_frame.identifier,
                                              false, 0x7ffu, true, buffer_index, 0u);
            dspic33_can_test_enable_filter(cpu, channel_index, 1u);
            dspic33_can_test_select_window(cpu, channel_index, false);
            dspic33_can_test_set_mode(cpu, channel_index, 0u);
            expect(state,
                   dspic33_can_receive(cpu, channel_index, &received_frame, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "direct buffer transfer");
            expect(state, dspic33_can_test_receive_full(cpu, channel_index, buffer_index),
                   "direct buffer full");
            expect(state,
                   dspic33_can_test_memory_word(cpu, (uint32_t)(0x6000u + buffer_index * 16u)) ==
                       (uint16_t)(received_frame.identifier << 2u),
                   "direct buffer address");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel_index] + 0x0au)) & 2u) != 0u,
                   "direct receive event");
        }
    }
}

void dspic33_can_test_fifo_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t sizes[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u};
    for (uint8_t selection_index = 0u; selection_index < sizeof(sizes); selection_index++) {
        const uint8_t fifo_size = sizes[selection_index];
        const uint8_t fifo_start_index = (uint8_t)(fifo_size / 2u);
        const uint8_t fifo_capacity = (uint8_t)(fifo_size - fifo_start_index);

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, 0u, 0x7000u, selection_index, fifo_start_index);
        dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x456u, false, 0x7ffu, true, 15u, 0u);
        dspic33_can_test_enable_filter(cpu, 0u, 1u);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        for (uint8_t fifo_offset = 0u; fifo_offset < fifo_capacity; fifo_offset++) {
            const Dspic33CanFrame received_frame =
                dspic33_can_test_frame(0x456u, false, false, 1u, fifo_offset);
            const uint8_t buffer_index = (uint8_t)(fifo_start_index + fifo_offset);

            expect(state,
                   dspic33_can_receive(cpu, 0u, &received_frame, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "FIFO transfer");
            expect(state, dspic33_can_test_receive_full(cpu, 0u, buffer_index),
                   "FIFO full sequence");
            expect(state,
                   dspic33_can_test_memory_word(
                       cpu, (uint32_t)(0x7000u + buffer_index * 16u + 6u)) == fifo_offset,
                   "FIFO payload sequence");
        }
        expect(state, ((dspic33_read_word(cpu, 0x0408u) >> 8u) & 0x3fu) == fifo_start_index,
               "FIFO write pointer wrap");
        {
            Dspic33CanFrame overflow = dspic33_can_test_frame(0x456u, false, false, 1u, 0xeeu);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &overflow, 0u) && dspic33_device_advance(cpu, 2u),
                   "FIFO overflow transfer attempt");
            expect(
                state,
                (dspic33_read_word(cpu, (uint16_t)(0x0428u + (fifo_start_index >= 16u ? 2u : 0u))) &
                 (uint16_t)(1u << (fifo_start_index & 15u))) != 0u,
                "FIFO overflow flag");
            expect(state, (dspic33_read_word(cpu, 0x040au) & 4u) != 0u,
                   "FIFO overflow interrupt flag");
        }
        clear_receive_flag(cpu, 0u, fifo_start_index);
        expect(state, (dspic33_read_word(cpu, 0x0408u) & 0x3fu) == fifo_start_index + 1u,
               "FIFO next read pointer");
    }
}

static void select_transmit_buffer(Dspic33* cpu, uint8_t channel_index, uint8_t buffer_index) {
    const uint16_t register_address =
        (uint16_t)(bases[channel_index] + 0x30u + (buffer_index / 2u) * 2u);
    const uint8_t shift_bits = (uint8_t)((buffer_index & 1u) * 8u);
    uint16_t register_value = dspic33_read_word(cpu, register_address);

    register_value = (uint16_t)((register_value & ~(uint16_t)(0xffu << shift_bits)) |
                                (uint16_t)(0x80u << shift_bits));
    dspic33_write_word(cpu, register_address, register_value);
}

static bool transmit_buffer_selected(Dspic33* cpu, uint8_t channel_index, uint8_t buffer_index) {
    const uint16_t register_address =
        (uint16_t)(bases[channel_index] + 0x30u + (buffer_index / 2u) * 2u);
    const uint8_t shift_bits = (uint8_t)((buffer_index & 1u) * 8u);

    return ((dspic33_read_word(cpu, register_address) >> shift_bits) & 0x80u) != 0u;
}

static void fifo_overflow_advancement_case(TestState* state, Dspic33* cpu, uint8_t channel_index,
                                           bool listen_all, bool buffer_full, bool pointer_wrap) {
    const uint16_t can_base = bases[channel_index];
    const uint32_t memory_address = (uint32_t)(0xd000u + channel_index * 0x100u);
    const Dspic33CanFrame received_frame = dspic33_can_test_frame(0x456u, false, false, 1u, 0x80u);
    const uint8_t buffer_index = pointer_wrap ? 3u : 2u;
    const uint8_t expected_next_buffer = pointer_wrap ? 2u : 3u;
    const uint16_t receive_flag = (uint16_t)(1u << buffer_index);
    uint16_t next_read_buffer;

    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, channel_index, memory_address, 0u, 2u);
    dspic33_can_test_configure_filter(cpu, channel_index, 0u, received_frame.identifier, false,
                                      0x7ffu, true, 15u, 0u);
    dspic33_can_test_enable_filter(cpu, channel_index, 1u);
    dspic33_can_test_select_window(cpu, channel_index, false);
    dspic33_can_test_set_mode(cpu, channel_index, listen_all ? 7u : 0u);
    if (pointer_wrap) {
        expect(state,
               dspic33_can_receive(cpu, channel_index, &received_frame, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "FIFO overflow wrap preparation");
        expect(state,
               ((dspic33_read_word(cpu, (uint16_t)(can_base + 8u)) >> 8u) & 0x3fu) == buffer_index,
               "FIFO overflow wrap pointer preparation");
        clear_receive_flag(cpu, channel_index, 2u);
    }
    if (buffer_full) {
        dspic33_can_test_write_memory_word(cpu, (uint16_t)(can_base + 0x20u), receive_flag);
    } else {
        select_transmit_buffer(cpu, channel_index, buffer_index);
    }
    dspic33_can_test_write_memory_word(cpu, memory_address + buffer_index * 16u, 0xa55au);
    next_read_buffer = (uint16_t)(dspic33_read_word(cpu, (uint16_t)(can_base + 8u)) & 0x003fu);
    expect(state,
           dspic33_can_receive(cpu, channel_index, &received_frame, 0u) &&
               dspic33_device_advance(cpu, 2u),
           "FIFO overflow receive attempt");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(can_base + 0x28u)) & receive_flag) != 0u,
           "FIFO overflow buffer flag");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(can_base + 0x0au)) & 4u) != 0u,
           "FIFO overflow event flag");
    expect(state,
           ((dspic33_read_word(cpu, (uint16_t)(can_base + 8u)) >> 8u) & 0x3fu) ==
               expected_next_buffer,
           "FIFO overflow advances write pointer");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(can_base + 8u)) & 0x003fu) == next_read_buffer,
           "FIFO overflow preserves next read pointer");
    expect(state, dspic33_can_test_memory_word(cpu, memory_address + buffer_index * 16u) == 0xa55au,
           "FIFO overflow loses message");
    expect(state, dspic33_can_test_receive_full(cpu, channel_index, buffer_index) == buffer_full,
           "FIFO overflow preserves selected buffer state");
    expect(state, buffer_full || transmit_buffer_selected(cpu, channel_index, buffer_index),
           "FIFO overflow preserves transmit selection");
}

void dspic33_can_test_fifo_overflow_advancement_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        for (uint8_t listen_all_mode = 0u; listen_all_mode < 2u; listen_all_mode++) {
            for (uint8_t full_buffer_case = 0u; full_buffer_case < 2u; full_buffer_case++) {
                for (uint8_t pointer_wrap_case = 0u; pointer_wrap_case < 2u; pointer_wrap_case++) {
                    fifo_overflow_advancement_case(state, cpu, channel_index, listen_all_mode != 0u,
                                                   full_buffer_case == 0u, pointer_wrap_case != 0u);
                }
            }
        }
    }
}

void dspic33_can_test_receive_flag_write_zero_domain(TestState* state, Dspic33* cpu) {
    static const uint8_t offsets[] = {0x20u, 0x22u, 0x28u, 0x2au};
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        dspic33_reset(cpu, 0u);
        dspic33_can_test_select_window(cpu, channel_index, false);
        for (size_t offset_index = 0u; offset_index < sizeof(offsets); offset_index++) {
            const uint16_t register_address =
                (uint16_t)(bases[channel_index] + offsets[offset_index]);

            for (uint32_t requested_value = 0u; requested_value <= UINT16_MAX;
                 requested_value += 257u) {
                dspic33_can_test_write_memory_word(cpu, register_address, UINT16_MAX);
                dspic33_write_word(cpu, register_address, (uint16_t)requested_value);
                expect(state, dspic33_read_word(cpu, register_address) == requested_value,
                       "receive flag write-zero domain");
            }
        }
    }
}
