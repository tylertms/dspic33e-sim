#include "device/dspic33ep_mu/communication/can/internal.h"

uint16_t dspic33_device_internal_can_frame_bits(const Dspic33CanFrame* frame, bool bits[160]);

void dspic33_can_test_receive_groups(TestState* state, Dspic33* cpu) {
    dspic33_can_test_receive_overflow_write_zero_prior_domain(state, cpu);
    dspic33_can_test_receive_flag_read_pointer_cases(state, cpu);
    dspic33_can_test_fifo_interrupt_boundary_cases(state, cpu);
    dspic33_can_test_receive_flag_hardware_event_cases(state, cpu);
    dspic33_can_test_overflow_and_fallback_cases(state, cpu);
    dspic33_can_test_transmission_cases(state, cpu);
    dspic33_can_test_clock_timing_cases(state, cpu);
    dspic33_can_test_stuffed_frame_timing_cases(state, cpu);
    dspic33_can_test_transmit_abort_timing_cases(state, cpu);
    dspic33_can_test_transmit_pps_cases(state, cpu);
    dspic33_can_test_triple_sample_cases(state, cpu);
    dspic33_can_test_resynchronization_cases(state, cpu);
    dspic33_can_test_overload_frame_cases(state, cpu);
}

void dspic33_can_test_receive_overflow_write_zero_prior_domain(TestState* state, Dspic33* cpu) {
    static const uint8_t overflow_offsets[] = {0x28u, 0x2au};
    static const uint16_t initial_words[] = {0x0000u, 0x5a5au};
    uint8_t channel_index;

    for (channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        uint8_t offset_index;

        dspic33_reset(cpu, 0u);
        dspic33_can_test_select_window(cpu, channel_index, false);
        for (offset_index = 0u; offset_index < sizeof(overflow_offsets); offset_index++) {
            uint16_t register_address =
                (uint16_t)(bases[channel_index] + overflow_offsets[offset_index]);
            uint8_t initial_word_index;

            for (initial_word_index = 0u;
                 initial_word_index < sizeof(initial_words) / sizeof(initial_words[0]);
                 initial_word_index++) {
                uint16_t initial_word = initial_words[initial_word_index];
                uint32_t requested_value;

                for (requested_value = 0u; requested_value <= UINT16_MAX; requested_value += 257u) {
                    dspic33_can_test_write_memory_word(cpu, register_address, initial_word);
                    dspic33_write_word(cpu, register_address, (uint16_t)requested_value);
                    expect(state,
                           dspic33_read_word(cpu, register_address) ==
                               (uint16_t)(initial_word & requested_value),
                           "receive overflow write-zero prior domain");
                }
            }
        }
    }
}

void dspic33_can_test_receive_flag_read_pointer_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t fifo_sizes[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u};
    uint8_t channel_index;

    for (channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        uint8_t fifo_size_index;

        for (fifo_size_index = 0u; fifo_size_index < sizeof(fifo_sizes); fifo_size_index++) {
            uint8_t receive_size = fifo_sizes[fifo_size_index];
            uint8_t start_buffer = (uint8_t)(receive_size / 2u);
            uint8_t buffer_index;

            for (buffer_index = start_buffer; buffer_index < receive_size; buffer_index++) {
                uint16_t register_address =
                    (uint16_t)(bases[channel_index] + 0x20u + (buffer_index >= 16u ? 2u : 0u));
                uint16_t bit_mask = (uint16_t)(1u << (buffer_index & 15u));
                uint16_t fifo_address = (uint16_t)(bases[channel_index] + 8u);
                uint8_t expected_buffer =
                    buffer_index + 1u == receive_size ? start_buffer : (uint8_t)(buffer_index + 1u);

                dspic33_reset(cpu, 0u);
                dspic33_can_test_configure_receive(cpu, channel_index, 0xd800u, fifo_size_index,
                                                   start_buffer);
                dspic33_can_test_select_window(cpu, channel_index, false);
                dspic33_can_test_write_memory_word(cpu, register_address, bit_mask);
                dspic33_can_test_write_memory_word(
                    cpu, fifo_address, (uint16_t)(((uint16_t)start_buffer << 8u) | 0x003fu));
                dspic33_write_word(cpu, register_address, (uint16_t)~bit_mask);
                expect(state, dspic33_read_word(cpu, register_address) == 0u,
                       "FIFO receive flag clear");
                expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == expected_buffer,
                       "FIFO receive flag advances read pointer");
                expect(state,
                       (dspic33_read_word(cpu, fifo_address) & 0x3f00u) ==
                           (uint16_t)((uint16_t)start_buffer << 8u),
                       "FIFO receive flag preserves write pointer");
            }
            if (start_buffer != 0u) {
                uint8_t previous_buffer_index = (uint8_t)(start_buffer - 1u);
                uint16_t register_address = (uint16_t)(bases[channel_index] + 0x20u);
                uint16_t bit_mask = (uint16_t)(1u << previous_buffer_index);
                uint16_t fifo_address = (uint16_t)(bases[channel_index] + 8u);

                dspic33_reset(cpu, 0u);
                dspic33_can_test_configure_receive(cpu, channel_index, 0xd800u, fifo_size_index,
                                                   start_buffer);
                dspic33_can_test_select_window(cpu, channel_index, false);
                dspic33_can_test_write_memory_word(cpu, register_address, bit_mask);
                dspic33_can_test_write_memory_word(
                    cpu, fifo_address, (uint16_t)(((uint16_t)start_buffer << 8u) | 0x003eu));
                dspic33_write_word(cpu, register_address, (uint16_t)~bit_mask);
                expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == 0x003eu,
                       "direct receive flag preserves FIFO read pointer");
            }
        }
    }
}

static void fifo_interrupt_boundary_case(TestState* state, Dspic33* cpu, uint8_t channel_index,
                                         bool wraparound, uint8_t fifo_relation) {
    uint16_t can_base = bases[channel_index];
    uint16_t fifo_address = (uint16_t)(can_base + 8u);
    uint16_t interrupt_address = (uint16_t)(can_base + 0x0au);
    uint32_t receive_memory = (uint32_t)(0xd800u + channel_index * 0x100u);
    Dspic33CanFrame input_frame = dspic33_can_test_frame(0x456u, false, false, 1u, 0x90u);
    uint8_t preparation_count = wraparound ? 2u : 0u;
    uint8_t frame_index;
    uint8_t expected_write_pointer = wraparound ? 5u : 3u;
    uint8_t expected_read_pointer;
    bool interrupt_asserted = fifo_relation == FIFO_RELATION_THRESHOLD;

    if (interrupt_asserted) {
        expected_read_pointer = wraparound ? 2u : 4u;
    } else if (fifo_relation == FIFO_RELATION_EQUAL) {
        expected_read_pointer = expected_write_pointer;
    } else {
        expected_read_pointer = wraparound ? 3u : 5u;
    }

    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, channel_index, receive_memory, 1u, 2u);
    dspic33_can_test_configure_filter(cpu, channel_index, 0u, input_frame.identifier, false, 0x7ffu,
                                      true, 15u, 0u);
    dspic33_can_test_enable_filter(cpu, channel_index, 1u);
    dspic33_can_test_select_window(cpu, channel_index, false);
    dspic33_can_test_set_mode(cpu, channel_index, 0u);
    for (frame_index = 0u; frame_index < preparation_count; frame_index++) {
        expect(state,
               dspic33_can_receive(cpu, channel_index, &input_frame, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "FIFO interrupt boundary preparation");
    }
    dspic33_can_test_write_memory_word(
        cpu, fifo_address,
        (uint16_t)((dspic33_read_word(cpu, fifo_address) & 0x3f00u) | expected_read_pointer));
    dspic33_write_word(cpu, interrupt_address,
                       (uint16_t)(dspic33_read_word(cpu, interrupt_address) & ~0x000au));
    dspic33_write_word(cpu, (uint16_t)(can_base + 0x0cu), 0x0008u);
    dspic33_can_test_clear_interrupt_flag(cpu, event_irqs[channel_index]);
    expect(state,
           dspic33_can_receive(cpu, channel_index, &input_frame, 0u) &&
               dspic33_device_advance(cpu, 32u),
           "FIFO interrupt boundary receive");
    expect(state,
           ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_write_pointer,
           "FIFO interrupt uses updated write pointer");
    expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == expected_read_pointer,
           "FIFO interrupt preserves read pointer");
    expect(state,
           ((dspic33_read_word(cpu, interrupt_address) & 0x0008u) != 0u) == interrupt_asserted,
           "FIFO interrupt boundary result");
    expect(state,
           dspic33_can_test_interrupt_flag(cpu, event_irqs[channel_index]) == interrupt_asserted,
           "FIFO interrupt boundary IFS result");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(can_base + 4u)) & 0x007fu) ==
               (interrupt_asserted ? 0x44u : 0x40u),
           "FIFO interrupt boundary vector result");
}

void dspic33_can_test_fifo_interrupt_boundary_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel_index;

    for (channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        uint8_t wraparound;

        for (wraparound = 0u; wraparound < 2u; wraparound++) {
            uint8_t fifo_relation;

            for (fifo_relation = FIFO_RELATION_THRESHOLD; fifo_relation <= FIFO_RELATION_DISTANT;
                 fifo_relation++) {
                fifo_interrupt_boundary_case(state, cpu, channel_index, wraparound != 0u,
                                             fifo_relation);
            }
        }
    }
}

static void receive_flag_hardware_event_case(TestState* state, Dspic33* cpu, uint8_t channel_index,
                                             uint8_t target_buffer_index) {
    uint16_t can_base = bases[channel_index];
    uint16_t fifo_address = (uint16_t)(can_base + 8u);
    uint16_t interrupt_address = (uint16_t)(can_base + 0x0au);
    uint16_t overflow_address =
        (uint16_t)(can_base + 0x28u + (target_buffer_index >= 16u ? 2u : 0u));
    uint16_t target_bit_mask = (uint16_t)(1u << (target_buffer_index & 15u));
    uint32_t receive_memory = (uint32_t)(0xd000u + channel_index * 0x400u);
    uint16_t preserved_message_words[8];
    uint8_t receive_buffer_index;
    uint8_t word_index;

    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, channel_index, receive_memory, 6u, target_buffer_index);
    dspic33_can_test_configure_filter(cpu, channel_index, 0u, 0x456u, false, 0x7ffu, true, 15u, 0u);
    dspic33_can_test_enable_filter(cpu, channel_index, 1u);
    dspic33_can_test_select_window(cpu, channel_index, false);
    dspic33_can_test_set_mode(cpu, channel_index, 0u);
    for (receive_buffer_index = target_buffer_index; receive_buffer_index < 32u;
         receive_buffer_index++) {
        Dspic33CanFrame input_frame = dspic33_can_test_frame(
            0x456u, false, false, 1u, (uint8_t)(0x20u + receive_buffer_index));
        uint8_t expected_write_pointer = receive_buffer_index == 31u
                                             ? target_buffer_index
                                             : (uint8_t)(receive_buffer_index + 1u);

        expect(state,
               dspic33_can_receive(cpu, channel_index, &input_frame, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "receive flag hardware fill event");
        expect(state, dspic33_can_test_receive_full(cpu, channel_index, receive_buffer_index),
               "receive flag hardware sets RXFUL");
        expect(state,
               dspic33_can_test_memory_word(cpu, receive_memory + receive_buffer_index * 16u +
                                                     6u) == (uint8_t)(0x20u + receive_buffer_index),
               "receive flag hardware stores message");
        expect(state,
               ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_write_pointer,
               "receive flag hardware advances write pointer");
        expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == target_buffer_index,
               "receive flag hardware preserves read pointer");
    }
    for (word_index = 0u; word_index < 8u; word_index++) {
        preserved_message_words[word_index] = dspic33_can_test_memory_word(
            cpu, receive_memory + target_buffer_index * 16u + word_index * 2u);
    }
    expect(state, (dspic33_read_word(cpu, overflow_address) & target_bit_mask) == 0u,
           "receive overflow flag initially clear");
    expect(state, (dspic33_read_word(cpu, interrupt_address) & 0x0004u) == 0u,
           "receive overflow interrupt initially clear");
    {
        Dspic33CanFrame overflow_frame = dspic33_can_test_frame(0x456u, false, false, 1u, 0xe0u);
        uint8_t expected_write_pointer =
            target_buffer_index == 31u ? target_buffer_index : (uint8_t)(target_buffer_index + 1u);

        expect(state,
               dspic33_can_receive(cpu, channel_index, &overflow_frame, 0u) &&
                   dspic33_device_advance(cpu, 2u),
               "receive overflow hardware event");
        expect(state, dspic33_can_test_receive_full(cpu, channel_index, target_buffer_index),
               "receive overflow preserves RXFUL");
        expect(state, (dspic33_read_word(cpu, overflow_address) & target_bit_mask) != 0u,
               "receive overflow hardware sets RXOVF");
        expect(state, (dspic33_read_word(cpu, interrupt_address) & 0x0004u) != 0u,
               "receive overflow hardware sets RBOVIF");
        expect(state,
               ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_write_pointer,
               "receive overflow advances write pointer");
        expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == target_buffer_index,
               "receive overflow preserves read pointer");
        for (word_index = 0u; word_index < 8u; word_index++) {
            expect(state,
                   dspic33_can_test_memory_word(cpu, receive_memory + target_buffer_index * 16u +
                                                         word_index * 2u) ==
                       preserved_message_words[word_index],
                   "receive overflow loses complete message");
        }
    }
}

void dspic33_can_test_receive_flag_hardware_event_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel_index;

    for (channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        uint8_t target_buffer_index;

        for (target_buffer_index = 0u; target_buffer_index < 32u; target_buffer_index++) {
            receive_flag_hardware_event_case(state, cpu, channel_index, target_buffer_index);
        }
    }
}

void dspic33_can_test_overflow_and_fallback_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame input_frame = dspic33_can_test_frame(0x123u, false, false, 2u, 0x40u);

    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, 0u, 0x8000u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x123u, false, 0x7ffu, true, 1u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 1u, 0x123u, false, 0x7ffu, true, 2u, 0u);
    dspic33_can_test_enable_filter(cpu, 0u, 3u);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_receive(cpu, 0u, &input_frame, 0u) && dspic33_device_advance(cpu, 32u),
           "fallback first transfer");
    expect(state, dspic33_can_test_receive_full(cpu, 0u, 1u), "fallback first buffer");
    expect(state,
           dspic33_can_receive(cpu, 0u, &input_frame, 0u) && dspic33_device_advance(cpu, 32u),
           "fallback second transfer");
    expect(state, dspic33_can_test_receive_full(cpu, 0u, 2u), "fallback second buffer");
    expect(state, dspic33_can_receive(cpu, 0u, &input_frame, 0u) && dspic33_device_advance(cpu, 2u),
           "fallback overflow attempt");
    expect(state, (dspic33_read_word(cpu, 0x0428u) & 2u) != 0u, "fallback lowest overflow");
}

void dspic33_can_test_transmission_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel_index;
    uint8_t is_extended;
    uint8_t is_remote;

    for (channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        for (is_extended = 0u; is_extended < 2u; is_extended++) {
            for (is_remote = 0u; is_remote < 2u; is_remote++) {
                uint16_t can_base = bases[channel_index];
                uint32_t transmit_memory = (uint32_t)(0x9000u + channel_index * 0x1000u);
                Dspic33CanFrame expected_frame = dspic33_can_test_frame(
                    is_extended != 0u ? 0x1234567u : 0x345u, is_extended != 0u, is_remote != 0u, 8u,
                    (uint8_t)(0x50u + is_extended * 8u));
                Dspic33CanFrame actual_frame;
                uint16_t transmit_words[8] = {0};
                uint8_t word_index;

                uint32_t standard_identifier = expected_frame.extended
                                                   ? (expected_frame.identifier >> 18u) & 0x7ffu
                                                   : expected_frame.identifier;
                uint32_t extended_identifier = expected_frame.identifier & 0x3ffffu;
                transmit_words[0] = (uint16_t)(standard_identifier << 2u);
                if (expected_frame.extended) {
                    transmit_words[0] |= 3u;
                    transmit_words[1] = (uint16_t)(extended_identifier >> 6u);
                    transmit_words[2] = (uint16_t)((extended_identifier & 0x3fu) << 10u);
                    if (expected_frame.remote) {
                        transmit_words[2] |= 0x0200u;
                    }
                } else if (expected_frame.remote) {
                    transmit_words[0] |= 2u;
                }
                transmit_words[2] |= expected_frame.length;
                for (word_index = 0u; word_index < expected_frame.length; word_index++) {
                    transmit_words[3u + word_index / 2u] |=
                        (uint16_t)expected_frame.data[word_index] << ((word_index & 1u) * 8u);
                }

                dspic33_reset(cpu, 0u);
                dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory);
                for (word_index = 0u; word_index < 8u; word_index++) {
                    dspic33_can_test_write_memory_word(cpu, transmit_memory + word_index * 2u,
                                                       transmit_words[word_index]);
                }
                dspic33_can_test_select_window(cpu, channel_index, false);
                dspic33_can_test_set_mode(cpu, channel_index, 0u);
                dspic33_write_word(cpu, (uint16_t)(can_base + 0x0cu), 1u);
                dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
                expect(state, dspic33_device_advance(cpu, 4096u), "transmit advance");
                expect(state, dspic33_can_transmit(cpu, channel_index, &actual_frame),
                       "transmit queue output");
                expect(state,
                       actual_frame.identifier == expected_frame.identifier &&
                           actual_frame.extended == expected_frame.extended &&
                           actual_frame.remote == expected_frame.remote &&
                           actual_frame.length == expected_frame.length,
                       "transmit frame header");
                expect(state, memcmp(actual_frame.data, expected_frame.data, 8u) == 0,
                       "transmit payload");
                expect(state, (dspic33_read_word(cpu, (uint16_t)(can_base + 0x30u)) & 8u) == 0u,
                       "transmit request clear");
                expect(state, (dspic33_read_word(cpu, (uint16_t)(can_base + 0x0au)) & 1u) != 0u,
                       "transmit event flag");
                expect(state, dspic33_can_test_interrupt_flag(cpu, event_irqs[channel_index]),
                       "transmit event interrupt");
            }
        }
    }
}

void dspic33_can_test_clock_timing_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t clock_controls[] = {0u, 0x0800u, 0u, 0u};
    static const uint16_t config1_values[] = {0u, 0u, 1u, 0u};
    static const uint16_t config2_values[] = {0u, 0u, 0u, 0x0311u};
    static const uint64_t completion_cycles[] = {208u, 408u, 408u, 508u};
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        for (uint8_t timing_index = 0u;
             timing_index < sizeof(completion_cycles) / sizeof(completion_cycles[0]);
             timing_index++) {
            uint16_t can_base = bases[channel_index];
            uint32_t transmit_memory = (uint32_t)(0xb800u + channel_index * 0x100u);
            Dspic33CanFrame output_frame;

            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory);
            dspic33_can_test_write_memory_word(cpu, transmit_memory, 2u);
            for (uint8_t word_index = 1u; word_index < 8u; word_index++) {
                dspic33_can_test_write_memory_word(cpu, transmit_memory + word_index * 2u, 0u);
            }
            dspic33_can_test_select_window(cpu, channel_index, false);
            dspic33_can_test_set_mode(cpu, channel_index, 4u);
            dspic33_write_word(cpu, (uint16_t)(can_base + 0x10u), config1_values[timing_index]);
            dspic33_write_word(cpu, (uint16_t)(can_base + 0x12u), config2_values[timing_index]);
            dspic33_write_word(cpu, can_base,
                               (uint16_t)((dspic33_read_word(cpu, can_base) & ~0x0800u) |
                                          clock_controls[timing_index]));
            dspic33_can_test_set_mode(cpu, channel_index, 0u);
            dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(can_base + 0x10u)) ==
                           config1_values[timing_index] &&
                       dspic33_read_word(cpu, (uint16_t)(can_base + 0x12u)) ==
                           config2_values[timing_index] &&
                       (dspic33_read_word(cpu, can_base) & 0x0800u) == clock_controls[timing_index],
                   "CAN bit timing configuration is retained");
            expect(state,
                   dspic33_device_advance(cpu, completion_cycles[timing_index] - 1u) &&
                       !dspic33_can_transmit(cpu, channel_index, &output_frame),
                   "CAN frame remains active before its final bus bit");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       dspic33_can_transmit(cpu, channel_index, &output_frame) &&
                       output_frame.identifier == 0u && output_frame.remote &&
                       output_frame.length == 0u,
                   "CAN frame completes on its configured B1 clock boundary");
        }
    }
}

void dspic33_can_test_stuffed_frame_timing_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t words[][8] = {
        {0x0c84u, 0u, 1u, 0x00a5u, 0u, 0u, 0u, 0u},
        {0x0123u, 0x0d15u, 0x9c08u, 0x0100u, 0x0302u, 0x0504u, 0x0706u, 0u},
    };
    static const uint64_t completion_cycles[] = {236u, 576u};
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        for (uint8_t frame_index = 0u;
             frame_index < sizeof(completion_cycles) / sizeof(completion_cycles[0]);
             frame_index++) {
            uint16_t can_base = bases[channel_index];
            uint32_t transmit_memory = (uint32_t)(0xba00u + channel_index * 0x100u);
            Dspic33CanFrame output_frame;

            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory);
            for (uint8_t word_index = 0u; word_index < 8u; word_index++) {
                dspic33_can_test_write_memory_word(cpu, transmit_memory + word_index * 2u,
                                                   words[frame_index][word_index]);
            }
            dspic33_can_test_select_window(cpu, channel_index, false);
            dspic33_can_test_set_mode(cpu, channel_index, 0u);
            dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
            expect(state,
                   dspic33_device_advance(cpu, completion_cycles[frame_index] - 1u) &&
                       !dspic33_can_transmit(cpu, channel_index, &output_frame),
                   "stuffed CAN frame remains active before its calculated boundary");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       dspic33_can_transmit(cpu, channel_index, &output_frame) &&
                       output_frame.extended == (frame_index != 0u) &&
                       output_frame.length == (frame_index == 0u ? 1u : 8u),
                   "stuffed CAN frame completes on its calculated boundary");
        }
    }
}

void dspic33_can_test_transmit_abort_timing_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        uint16_t can_base = bases[channel_index];
        uint32_t transmit_memory = (uint32_t)(0xbc00u + channel_index * 0x100u);
        Dspic33CanFrame output_frame;

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory);
        dspic33_can_test_write_memory_word(cpu, transmit_memory, 2u);
        for (uint8_t word_index = 1u; word_index < 8u; word_index++) {
            dspic33_can_test_write_memory_word(cpu, transmit_memory + word_index * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_busy & (uint8_t)(1u << channel_index)) != 0u &&
                   !dspic33_can_transmit(cpu, channel_index, &output_frame),
               "CAN abort test reaches the on-bus interval");
        dspic33_write_word(cpu, can_base, (uint16_t)(dspic33_read_word(cpu, can_base) | 0x1000u));
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel_index)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(can_base + 0x30u)) & 0x0048u) == 0x0040u &&
                   dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_can_transmit(cpu, channel_index, &output_frame),
               "CAN abort cancels the pending on-bus completion");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory);
        dspic33_can_test_write_memory_word(cpu, transmit_memory, 2u);
        for (uint8_t word_index = 1u; word_index < 8u; word_index++) {
            dspic33_can_test_write_memory_word(cpu, transmit_memory + word_index * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_busy & (uint8_t)(1u << channel_index)) != 0u,
               "individual CAN abort reaches the on-bus interval");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x0083u);
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel_index)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(can_base + 0x30u)) & 0x0048u) == 0x0040u &&
                   dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_can_transmit(cpu, channel_index, &output_frame),
               "clearing TXREQ aborts the active CAN transmission");
    }
}

void dspic33_can_test_transmit_pps_cases(TestState* state, Dspic33* cpu) {
    bool pin_level;

    expect(state, !dspic33_can_pin(cpu, 64u, NULL), "CAN output rejects null pin level");
    expect(state, !dspic33_can_pin(cpu, 63u, &pin_level), "CAN output rejects non-remappable pin");

    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        uint16_t can_base = bases[channel_index];
        uint32_t transmit_memory = (uint32_t)(0xbe00u + channel_index * 0x100u);
        uint8_t pps_output_function = (uint8_t)(14u + channel_index);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, pps_output_function);
        expect(state, dspic33_can_pin(cpu, 64u, &pin_level) && pin_level,
               "mapped CAN transmit pin is recessive while idle");
        dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory);
        for (uint8_t word_index = 0u; word_index < 8u; word_index++) {
            dspic33_can_test_write_memory_word(cpu, transmit_memory + word_index * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &pin_level) &&
                   !pin_level,
               "CAN transmit pin drives dominant start of frame");
        expect(state,
               dspic33_device_advance(cpu, 20u) && dspic33_can_pin(cpu, 64u, &pin_level) &&
                   pin_level,
               "CAN transmit pin inserts the sixth complementary stuffed bit");
        expect(state,
               dspic33_device_advance(cpu, 4u) && dspic33_can_pin(cpu, 64u, &pin_level) &&
                   !pin_level,
               "CAN transmit pin resumes frame data after stuffing");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x0083u);
        expect(state, dspic33_can_pin(cpu, 64u, &pin_level) && pin_level,
               "aborted CAN transmit pin returns recessive");
        dspic33_write_word(cpu, 0x0680u, (uint16_t)(pps_output_function << 8u));
        expect(state,
               !dspic33_can_pin(cpu, 64u, &pin_level) && dspic33_can_pin(cpu, 65u, &pin_level) &&
                   pin_level,
               "CAN transmit output follows PPS remapping");
        dspic33_write_word(
            cpu, 0x0760u,
            (uint16_t)(dspic33_read_word(cpu, 0x0760u) | (uint16_t)(2u << channel_index)));
        expect(state, dspic33_device_advance(cpu, 1u) && !dspic33_can_pin(cpu, 65u, &pin_level),
               "PMD releases the CAN transmit PPS output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, pps_output_function);
        dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory);
        for (uint8_t word_index = 0u; word_index < 8u; word_index++) {
            dspic33_can_test_write_memory_word(cpu, transmit_memory + word_index * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &pin_level) &&
                   !pin_level,
               "CAN Sleep output test reaches dominant bus phase");
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state, dspic33_can_pin(cpu, 64u, &pin_level) && pin_level,
               "Sleep forces the CAN transmit pin recessive");
    }
}

bool dspic33_can_test_bridge_can_pins(Dspic33* cpu, uint8_t transmit_channel, uint8_t pin,
                                      uint8_t acknowledge_pin, uint64_t bit_cycles, int corrupt_bit,
                                      bool* acknowledge_observed) {
    uint16_t bit_index = 0u;
    while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << transmit_channel)) != 0u && bit_index < 160u) {
        bool transmit_level;
        bool acknowledge_high;

        if (dspic33_can_pin(cpu, acknowledge_pin, &acknowledge_high) && !acknowledge_high) {
            *acknowledge_observed = true;
        }
        if (!dspic33_can_pin(cpu, pin, &transmit_level)) {
            return false;
        }
        if (bit_index == corrupt_bit) {
            transmit_level = !transmit_level;
        }
        if (!dspic33_can_input_pin(cpu, pin, transmit_level && acknowledge_high, 0u) ||
            !dspic33_device_advance(cpu, bit_cycles)) {
            return false;
        }
        bit_index++;
    }
    return bit_index != 0u && bit_index < 160u && dspic33_device_advance(cpu, 32u);
}

static bool bridge_can_with_final_sample_glitch(Dspic33* cpu, uint8_t transmit_channel,
                                                uint8_t transmit_pin, uint8_t acknowledge_pin,
                                                uint8_t transmit_receive_pin, uint8_t receive_pin,
                                                bool transmitter_glitch,
                                                bool* acknowledge_observed) {
    uint16_t bit_index = 0u;
    while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << transmit_channel)) != 0u && bit_index < 160u) {
        bool transmit_high;
        bool acknowledge_high;
        bool bus_high;
        if (dspic33_can_pin(cpu, acknowledge_pin, &acknowledge_high) && !acknowledge_high) {
            *acknowledge_observed = true;
        }
        if (!dspic33_can_pin(cpu, transmit_pin, &transmit_high)) {
            return false;
        }
        bus_high = transmit_high && acknowledge_high;
        if (!dspic33_can_input_pin(cpu, transmit_receive_pin, bus_high, 0u) ||
            !dspic33_can_input_pin(cpu, receive_pin, bus_high, 0u) ||
            !dspic33_device_advance(cpu, 2u)) {
            return false;
        }
        if (bit_index == 0u &&
            !dspic33_can_input_pin(cpu, transmitter_glitch ? transmit_receive_pin : receive_pin,
                                   !bus_high, 0u)) {
            return false;
        }
        if (!dspic33_device_advance(cpu, 1u) ||
            !dspic33_can_input_pin(cpu, transmitter_glitch ? transmit_receive_pin : receive_pin,
                                   bus_high, 0u) ||
            !dspic33_device_advance(cpu, 1u)) {
            return false;
        }
        bit_index++;
    }
    return bit_index != 0u && bit_index < 160u && dspic33_device_advance(cpu, 32u);
}

void dspic33_can_test_triple_sample_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t transmitter_glitch = 0u; transmitter_glitch < 2u; transmitter_glitch++) {
        Dspic33CanFrame input_frame =
            dspic33_can_test_frame((uint32_t)(0x360u + transmitter_glitch), false, false, 2u,
                                   (uint8_t)(0x80u + transmitter_glitch * 0x10u));
        Dspic33CanFrame output_frame;
        bool acknowledge_observed = false;

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, 0x4042u);
        dspic33_can_test_configure_receive(cpu, 1u, 0xda00u, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, 1u, 0u, input_frame.identifier, false, 0x7ffu, true,
                                          0u, 0u);
        dspic33_can_test_enable_filter(cpu, 1u, 1u);
        dspic33_can_test_configure_transmit(cpu, 0u, 0xd800u);
        dspic33_can_test_write_transmit_frame(cpu, 0xd800u, &input_frame);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_select_window(cpu, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, transmitter_glitch != 0u ? 0x0040u : 0u);
        dspic33_write_word(cpu, 0x0510u, 0u);
        dspic33_write_word(cpu, 0x0512u, transmitter_glitch == 0u ? 0x0040u : 0u);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        dspic33_can_test_set_mode(cpu, 1u, 0u);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   bridge_can_with_final_sample_glitch(cpu, 0u, 64u, 65u, 66u, 64u,
                                                       transmitter_glitch != 0u,
                                                       &acknowledge_observed),
               "CAN triple-sample bridge tolerates one final-sample glitch");
        expect(state,
               dspic33_can_test_receive_full(cpu, 1u, 0u) &&
                   (dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 0u &&
                   dspic33_can_test_memory_word(cpu, 0xda00u) ==
                       (uint16_t)(input_frame.identifier << 2u) &&
                   (uint8_t)dspic33_can_test_memory_word(cpu, 0xda06u) == input_frame.data[0] &&
                   (uint8_t)(dspic33_can_test_memory_word(cpu, 0xda06u) >> 8u) ==
                       input_frame.data[1],
               "CAN triple-sample receiver uses the majority value");
        expect(state,
               acknowledge_observed && dspic33_can_transmit(cpu, 0u, &output_frame) &&
                   output_frame.identifier == input_frame.identifier &&
                   (dspic33_read_word(cpu, 0x040eu) >> 8u) == 0u &&
                   (dspic33_read_word(cpu, 0x0430u) & 0x0018u) == 0u,
               "CAN triple-sample transmitter uses the majority value");
    }
}

static void prepare_resynchronization(Dspic33* cpu, uint16_t can_config1, uint16_t can_config2) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_write_word(cpu, 0x0410u, can_config1);
    dspic33_write_word(cpu, 0x0412u, can_config2);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_can_input_pin(cpu, 64u, false, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_can_input_pin(cpu, 64u, true, 0u);
    dspic33_device_advance(cpu, 0u);
}

void dspic33_can_test_resynchronization_cases(TestState* state, Dspic33* cpu) {
    prepare_resynchronization(cpu, 0u, 0u);
    expect(state,
           cpu->io.can_rx_serial_count[0] == 1u && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 2u) && cpu->io.can_rx_serial_count[0] == 1u,
           "early CAN edge shortens Phase Segment 2");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 2u,
           "early CAN edge advances the next sample point by one TQ");

    prepare_resynchronization(cpu, 0u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 1u,
           "late CAN edge lengthens Phase Segment 1");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 2u,
           "one-TQ SJW limits a late CAN resynchronization");

    prepare_resynchronization(cpu, 0x0040u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 2u) && cpu->io.can_rx_serial_count[0] == 1u,
           "two-TQ SJW permits a larger CAN phase correction");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 2u,
           "two-TQ CAN resynchronization reaches its adjusted sample point");

    prepare_resynchronization(cpu, 0u, 0x0300u);
    expect(state,
           cpu->io.can_rx_serial_count[0] == 1u && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 5u) && cpu->io.can_rx_serial_count[0] == 1u,
           "one-TQ SJW clamps an early edge four TQ before the next bit");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 2u,
           "negative SJW clamp advances the sample point by exactly one TQ");
}

static bool drive_can_to_intermission(Dspic33* cpu) {
    for (uint16_t bit_index = 0u; bit_index < 160u; bit_index++) {
        bool transmit_level;
        bool acknowledge_level;

        if ((cpu->io.can_intermission_active & 2u) != 0u) {
            return true;
        }
        if (!dspic33_can_pin(cpu, 64u, &transmit_level) ||
            !dspic33_can_pin(cpu, 65u, &acknowledge_level) ||
            !dspic33_can_input_pin(cpu, 66u, transmit_level && acknowledge_level, 0u) ||
            !dspic33_can_input_pin(cpu, 64u, transmit_level && acknowledge_level, 0u) ||
            !dspic33_device_advance(cpu, 4u)) {
            return false;
        }
    }
    return false;
}

static bool drive_can_with_dominant_final_eof(Dspic33* cpu, const Dspic33CanFrame* input_frame) {
    bool frame_bits[160];
    const uint16_t final_eof_index =
        dspic33_device_internal_can_frame_bits(input_frame, frame_bits) - 4u;

    for (uint16_t bit_index = 0u; bit_index < 160u; bit_index++) {
        bool transmit_level;
        bool acknowledge_level;

        if ((cpu->io.can_overload_active & 2u) != 0u) {
            return true;
        }
        if (!dspic33_can_pin(cpu, 64u, &transmit_level) ||
            !dspic33_can_pin(cpu, 65u, &acknowledge_level) ||
            !dspic33_can_input_pin(cpu, 66u, transmit_level && acknowledge_level, 0u) ||
            !dspic33_can_input_pin(cpu, 64u,
                                   cpu->io.can_rx_serial_count[1] == final_eof_index
                                       ? false
                                       : transmit_level && acknowledge_level,
                                   0u) ||
            !dspic33_device_advance(cpu, 4u)) {
            return false;
        }
    }
    return false;
}

static void configure_overload_pair(Dspic33* cpu, const Dspic33CanFrame* input_frame) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
    dspic33_write_word(cpu, 0x06d4u, 0x4042u);
    dspic33_can_test_configure_receive(cpu, 1u, 0xda00u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 1u, 0u, input_frame->identifier, false, 0x7ffu, true, 0u,
                                      0u);
    dspic33_can_test_enable_filter(cpu, 1u, 1u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xd800u);
    dspic33_can_test_write_transmit_frame(cpu, 0xd800u, input_frame);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_select_window(cpu, 1u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_write_word(cpu, 0x0510u, 0u);
    dspic33_write_word(cpu, 0x0512u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_can_test_set_mode(cpu, 1u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
}

void dspic33_can_test_overload_frame_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame input_frame = dspic33_can_test_frame(0x365u, false, false, 1u, 0xa0u);
    bool pin_level;

    configure_overload_pair(cpu, &input_frame);
    expect(state, dspic33_device_advance(cpu, 8u), "CAN overload source reaches the bus");
    expect(state, drive_can_to_intermission(cpu), "valid CAN frame reaches Intermission");
    expect(state, cpu->io.can_rx_serial_count[1] != 0u,
           "valid CAN serial frame completes before Intermission");
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_can_input_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_can_pin(cpu, 65u, &pin_level) && !pin_level &&
               cpu->io.can_overload_count[1] == 1u,
           "dominant Intermission edge starts a CAN overload flag");
    expect(state,
           dspic33_device_advance(cpu, 23u) && dspic33_can_pin(cpu, 65u, &pin_level) && !pin_level,
           "CAN overload flag remains dominant for six bits");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_can_pin(cpu, 65u, &pin_level) && pin_level,
           "CAN overload delimiter becomes recessive after six bits");
    expect(state, dspic33_device_advance(cpu, 32u) && (cpu->io.can_intermission_active & 2u) != 0u,
           "CAN overload delimiter is followed by Intermission");
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_can_input_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 56u) &&
               cpu->io.can_overload_count[1] == 2u && (cpu->io.can_intermission_active & 2u) != 0u,
           "CAN permits two sequential overload frames");
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_can_pin(cpu, 65u, &pin_level) && pin_level &&
               (cpu->io.can_overload_active & 2u) == 0u,
           "CAN suppresses a third sequential overload frame");

    configure_overload_pair(cpu, &input_frame);
    expect(state, dspic33_device_advance(cpu, 8u), "CAN EOF overload source reaches the bus");
    expect(state,
           drive_can_with_dominant_final_eof(cpu, &input_frame) && dspic33_device_advance(cpu, 9u),
           "dominant final EOF bit completes CAN reception");
    expect(state, dspic33_can_test_receive_full(cpu, 1u, 0u),
           "dominant final EOF bit marks the CAN receive buffer full");
    expect(state, (uint8_t)dspic33_can_test_memory_word(cpu, 0xda06u) == input_frame.data[0],
           "dominant final EOF bit stores the CAN frame");
    expect(state, cpu->io.can_overload_count[1] == 1u,
           "dominant final EOF bit starts CAN overload");
}
