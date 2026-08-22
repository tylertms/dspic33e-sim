#include "device/dspic33ep_mu/communication/can/internal.h"

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
    static const uint8_t offsets[] = {0x28u, 0x2au};
    static const uint16_t previous_words[] = {0x0000u, 0x5a5au};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t index;
        dspic33_reset(cpu, 0u);
        dspic33_can_test_select_window(cpu, channel, false);
        for (index = 0u; index < sizeof(offsets); index++) {
            uint16_t address = (uint16_t)(bases[channel] + offsets[index]);
            uint8_t previous_index;
            for (previous_index = 0u;
                 previous_index < sizeof(previous_words) / sizeof(previous_words[0]);
                 previous_index++) {
                uint16_t previous = previous_words[previous_index];
                uint32_t requested;
                for (requested = 0u; requested <= UINT16_MAX; requested += 257u) {
                    dspic33_can_test_write_memory_word(cpu, address, previous);
                    dspic33_write_word(cpu, address, (uint16_t)requested);
                    expect(state,
                           dspic33_read_word(cpu, address) == (uint16_t)(previous & requested),
                           "receive overflow write-zero prior domain");
                }
            }
        }
    }
}

void dspic33_can_test_receive_flag_read_pointer_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t sizes[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t selection;
        for (selection = 0u; selection < sizeof(sizes); selection++) {
            uint8_t size = sizes[selection];
            uint8_t start = (uint8_t)(size / 2u);
            uint8_t buffer;
            for (buffer = start; buffer < size; buffer++) {
                uint16_t address = (uint16_t)(bases[channel] + 0x20u + (buffer >= 16u ? 2u : 0u));
                uint16_t bit = (uint16_t)(1u << (buffer & 15u));
                uint16_t fifo_address = (uint16_t)(bases[channel] + 8u);
                uint8_t expected = buffer + 1u == size ? start : (uint8_t)(buffer + 1u);
                dspic33_reset(cpu, 0u);
                dspic33_can_test_configure_receive(cpu, channel, 0xf800u, selection, start);
                dspic33_can_test_select_window(cpu, channel, false);
                dspic33_can_test_write_memory_word(cpu, address, bit);
                dspic33_can_test_write_memory_word(cpu, fifo_address,
                                                   (uint16_t)(((uint16_t)start << 8u) | 0x003fu));
                dspic33_write_word(cpu, address, (uint16_t)~bit);
                expect(state, dspic33_read_word(cpu, address) == 0u, "FIFO receive flag clear");
                expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == expected,
                       "FIFO receive flag advances read pointer");
                expect(state,
                       (dspic33_read_word(cpu, fifo_address) & 0x3f00u) ==
                           (uint16_t)((uint16_t)start << 8u),
                       "FIFO receive flag preserves write pointer");
            }
            if (start != 0u) {
                uint8_t buffer = (uint8_t)(start - 1u);
                uint16_t address = (uint16_t)(bases[channel] + 0x20u);
                uint16_t bit = (uint16_t)(1u << buffer);
                uint16_t fifo_address = (uint16_t)(bases[channel] + 8u);
                dspic33_reset(cpu, 0u);
                dspic33_can_test_configure_receive(cpu, channel, 0xf800u, selection, start);
                dspic33_can_test_select_window(cpu, channel, false);
                dspic33_can_test_write_memory_word(cpu, address, bit);
                dspic33_can_test_write_memory_word(cpu, fifo_address,
                                                   (uint16_t)(((uint16_t)start << 8u) | 0x003eu));
                dspic33_write_word(cpu, address, (uint16_t)~bit);
                expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == 0x003eu,
                       "direct receive flag preserves FIFO read pointer");
            }
        }
    }
}

static void fifo_interrupt_boundary_case(TestState* state, Dspic33* cpu, uint8_t channel, bool wrap,
                                         uint8_t relation) {
    uint16_t base = bases[channel];
    uint16_t fifo_address = (uint16_t)(base + 8u);
    uint16_t interrupt_address = (uint16_t)(base + 0x0au);
    uint32_t memory = (uint32_t)(0xf800u + channel * 0x100u);
    Dspic33CanFrame input = dspic33_can_test_frame(0x456u, false, false, 1u, 0x90u);
    uint8_t preparation = wrap ? 2u : 0u;
    uint8_t index;
    uint8_t expected_fbp = wrap ? 5u : 3u;
    uint8_t fnrb;
    bool asserted = relation == FIFO_RELATION_THRESHOLD;
    if (asserted) {
        fnrb = wrap ? 2u : 4u;
    } else if (relation == FIFO_RELATION_EQUAL) {
        fnrb = expected_fbp;
    } else {
        fnrb = wrap ? 3u : 5u;
    }
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, channel, memory, 1u, 2u);
    dspic33_can_test_configure_filter(cpu, channel, 0u, input.identifier, false, 0x7ffu, true, 15u,
                                      0u);
    dspic33_can_test_enable_filter(cpu, channel, 1u);
    dspic33_can_test_select_window(cpu, channel, false);
    dspic33_can_test_set_mode(cpu, channel, 0u);
    for (index = 0u; index < preparation; index++) {
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 32u),
               "FIFO interrupt boundary preparation");
    }
    dspic33_can_test_write_memory_word(
        cpu, fifo_address, (uint16_t)((dspic33_read_word(cpu, fifo_address) & 0x3f00u) | fnrb));
    dspic33_write_word(cpu, interrupt_address,
                       (uint16_t)(dspic33_read_word(cpu, interrupt_address) & ~0x000au));
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x0008u);
    dspic33_can_test_clear_interrupt_flag(cpu, event_irqs[channel]);
    expect(state, dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 32u),
           "FIFO interrupt boundary receive");
    expect(state, ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_fbp,
           "FIFO interrupt uses updated write pointer");
    expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == fnrb,
           "FIFO interrupt preserves read pointer");
    expect(state, ((dspic33_read_word(cpu, interrupt_address) & 0x0008u) != 0u) == asserted,
           "FIFO interrupt boundary result");
    expect(state, dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]) == asserted,
           "FIFO interrupt boundary IFS result");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & 0x007fu) == (asserted ? 0x44u : 0x40u),
           "FIFO interrupt boundary vector result");
}

void dspic33_can_test_fifo_interrupt_boundary_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t wrap;
        for (wrap = 0u; wrap < 2u; wrap++) {
            uint8_t relation;
            for (relation = FIFO_RELATION_THRESHOLD; relation <= FIFO_RELATION_DISTANT;
                 relation++) {
                fifo_interrupt_boundary_case(state, cpu, channel, wrap != 0u, relation);
            }
        }
    }
}

static void receive_flag_hardware_event_case(TestState* state, Dspic33* cpu, uint8_t channel,
                                             uint8_t target) {
    uint16_t base = bases[channel];
    uint16_t fifo_address = (uint16_t)(base + 8u);
    uint16_t interrupt_address = (uint16_t)(base + 0x0au);
    uint16_t overflow_address = (uint16_t)(base + 0x28u + (target >= 16u ? 2u : 0u));
    uint16_t target_bit = (uint16_t)(1u << (target & 15u));
    uint32_t memory = (uint32_t)(0xf000u + channel * 0x400u);
    uint16_t preserved_words[8];
    uint8_t buffer;
    uint8_t word;
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, channel, memory, 6u, target);
    dspic33_can_test_configure_filter(cpu, channel, 0u, 0x456u, false, 0x7ffu, true, 15u, 0u);
    dspic33_can_test_enable_filter(cpu, channel, 1u);
    dspic33_can_test_select_window(cpu, channel, false);
    dspic33_can_test_set_mode(cpu, channel, 0u);
    for (buffer = target; buffer < 32u; buffer++) {
        Dspic33CanFrame input =
            dspic33_can_test_frame(0x456u, false, false, 1u, (uint8_t)(0x20u + buffer));
        uint8_t expected_fbp = buffer == 31u ? target : (uint8_t)(buffer + 1u);
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 32u),
               "receive flag hardware fill event");
        expect(state, dspic33_can_test_receive_full(cpu, channel, buffer),
               "receive flag hardware sets RXFUL");
        expect(state,
               dspic33_can_test_memory_word(cpu, memory + buffer * 16u + 6u) ==
                   (uint8_t)(0x20u + buffer),
               "receive flag hardware stores message");
        expect(state, ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_fbp,
               "receive flag hardware advances write pointer");
        expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == target,
               "receive flag hardware preserves read pointer");
    }
    for (word = 0u; word < 8u; word++) {
        preserved_words[word] =
            dspic33_can_test_memory_word(cpu, memory + target * 16u + word * 2u);
    }
    expect(state, (dspic33_read_word(cpu, overflow_address) & target_bit) == 0u,
           "receive overflow flag initially clear");
    expect(state, (dspic33_read_word(cpu, interrupt_address) & 0x0004u) == 0u,
           "receive overflow interrupt initially clear");
    {
        Dspic33CanFrame overflow = dspic33_can_test_frame(0x456u, false, false, 1u, 0xe0u);
        uint8_t expected_fbp = target == 31u ? target : (uint8_t)(target + 1u);
        expect(state,
               dspic33_can_receive(cpu, channel, &overflow, 0u) && dspic33_device_advance(cpu, 2u),
               "receive overflow hardware event");
        expect(state, dspic33_can_test_receive_full(cpu, channel, target),
               "receive overflow preserves RXFUL");
        expect(state, (dspic33_read_word(cpu, overflow_address) & target_bit) != 0u,
               "receive overflow hardware sets RXOVF");
        expect(state, (dspic33_read_word(cpu, interrupt_address) & 0x0004u) != 0u,
               "receive overflow hardware sets RBOVIF");
        expect(state, ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_fbp,
               "receive overflow advances write pointer");
        expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == target,
               "receive overflow preserves read pointer");
        for (word = 0u; word < 8u; word++) {
            expect(state,
                   dspic33_can_test_memory_word(cpu, memory + target * 16u + word * 2u) ==
                       preserved_words[word],
                   "receive overflow loses complete message");
        }
    }
}

void dspic33_can_test_receive_flag_hardware_event_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t target;
        for (target = 0u; target < 32u; target++) {
            receive_flag_hardware_event_case(state, cpu, channel, target);
        }
    }
}

void dspic33_can_test_overflow_and_fallback_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame input = dspic33_can_test_frame(0x123u, false, false, 2u, 0x40u);
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, 0u, 0x8000u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x123u, false, 0x7ffu, true, 1u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 1u, 0x123u, false, 0x7ffu, true, 2u, 0u);
    dspic33_can_test_enable_filter(cpu, 0u, 3u);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state, dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
           "fallback first transfer");
    expect(state, dspic33_can_test_receive_full(cpu, 0u, 1u), "fallback first buffer");
    expect(state, dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
           "fallback second transfer");
    expect(state, dspic33_can_test_receive_full(cpu, 0u, 2u), "fallback second buffer");
    expect(state, dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 2u),
           "fallback overflow attempt");
    expect(state, (dspic33_read_word(cpu, 0x0428u) & 2u) != 0u, "fallback lowest overflow");
}

void dspic33_can_test_transmission_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t extended;
    uint8_t remote;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (extended = 0u; extended < 2u; extended++) {
            for (remote = 0u; remote < 2u; remote++) {
                uint16_t base = bases[channel];
                uint32_t memory = (uint32_t)(0x9000u + channel * 0x1000u);
                Dspic33CanFrame expected =
                    dspic33_can_test_frame(extended != 0u ? 0x1234567u : 0x345u, extended != 0u,
                                           remote != 0u, 8u, (uint8_t)(0x50u + extended * 8u));
                Dspic33CanFrame actual;
                uint16_t words[8] = {0};
                uint8_t index;
                uint32_t sid =
                    expected.extended ? (expected.identifier >> 18u) & 0x7ffu : expected.identifier;
                uint32_t eid = expected.identifier & 0x3ffffu;
                words[0] = (uint16_t)(sid << 2u);
                if (expected.extended) {
                    words[0] |= 3u;
                    words[1] = (uint16_t)(eid >> 6u);
                    words[2] = (uint16_t)((eid & 0x3fu) << 10u);
                    if (expected.remote) {
                        words[2] |= 0x0200u;
                    }
                } else if (expected.remote) {
                    words[0] |= 2u;
                }
                words[2] |= expected.length;
                for (index = 0u; index < expected.length; index++) {
                    words[3u + index / 2u] |= (uint16_t)expected.data[index] << ((index & 1u) * 8u);
                }
                dspic33_reset(cpu, 0u);
                dspic33_can_test_configure_transmit(cpu, channel, memory);
                for (index = 0u; index < 8u; index++) {
                    dspic33_can_test_write_memory_word(cpu, memory + index * 2u, words[index]);
                }
                dspic33_can_test_select_window(cpu, channel, false);
                dspic33_can_test_set_mode(cpu, channel, 0u);
                dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 1u);
                dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
                expect(state, dspic33_device_advance(cpu, 4096u), "transmit advance");
                expect(state, dspic33_can_transmit(cpu, channel, &actual), "transmit queue output");
                expect(state,
                       actual.identifier == expected.identifier &&
                           actual.extended == expected.extended &&
                           actual.remote == expected.remote && actual.length == expected.length,
                       "transmit frame header");
                expect(state, memcmp(actual.data, expected.data, 8u) == 0, "transmit payload");
                expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 8u) == 0u,
                       "transmit request clear");
                expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 1u) != 0u,
                       "transmit event flag");
                expect(state, dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
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
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (uint8_t timing = 0u; timing < sizeof(completion_cycles) / sizeof(completion_cycles[0]);
             timing++) {
            uint16_t base = bases[channel];
            uint32_t memory = (uint32_t)(0xb800u + channel * 0x100u);
            Dspic33CanFrame output;
            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_transmit(cpu, channel, memory);
            dspic33_can_test_write_memory_word(cpu, memory, 2u);
            for (uint8_t word = 1u; word < 8u; word++) {
                dspic33_can_test_write_memory_word(cpu, memory + word * 2u, 0u);
            }
            dspic33_can_test_select_window(cpu, channel, false);
            dspic33_can_test_set_mode(cpu, channel, 4u);
            dspic33_write_word(cpu, (uint16_t)(base + 0x10u), config1_values[timing]);
            dspic33_write_word(cpu, (uint16_t)(base + 0x12u), config2_values[timing]);
            dspic33_write_word(
                cpu, base,
                (uint16_t)((dspic33_read_word(cpu, base) & ~0x0800u) | clock_controls[timing]));
            dspic33_can_test_set_mode(cpu, channel, 0u);
            dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 0x10u)) == config1_values[timing] &&
                       dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) == config2_values[timing] &&
                       (dspic33_read_word(cpu, base) & 0x0800u) == clock_controls[timing],
                   "CAN bit timing configuration is retained");
            expect(state,
                   dspic33_device_advance(cpu, completion_cycles[timing] - 1u) &&
                       !dspic33_can_transmit(cpu, channel, &output),
                   "CAN frame remains active before its final bus bit");
            expect(state,
                   dspic33_device_advance(cpu, 1u) && dspic33_can_transmit(cpu, channel, &output) &&
                       output.identifier == 0u && output.remote && output.length == 0u,
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
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (uint8_t frame_index = 0u;
             frame_index < sizeof(completion_cycles) / sizeof(completion_cycles[0]);
             frame_index++) {
            uint16_t base = bases[channel];
            uint32_t memory = (uint32_t)(0xba00u + channel * 0x100u);
            Dspic33CanFrame output;
            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_transmit(cpu, channel, memory);
            for (uint8_t word = 0u; word < 8u; word++) {
                dspic33_can_test_write_memory_word(cpu, memory + word * 2u,
                                                   words[frame_index][word]);
            }
            dspic33_can_test_select_window(cpu, channel, false);
            dspic33_can_test_set_mode(cpu, channel, 0u);
            dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
            expect(state,
                   dspic33_device_advance(cpu, completion_cycles[frame_index] - 1u) &&
                       !dspic33_can_transmit(cpu, channel, &output),
                   "stuffed CAN frame remains active before its calculated boundary");
            expect(state,
                   dspic33_device_advance(cpu, 1u) && dspic33_can_transmit(cpu, channel, &output) &&
                       output.extended == (frame_index != 0u) &&
                       output.length == (frame_index == 0u ? 1u : 8u),
                   "stuffed CAN frame completes on its calculated boundary");
        }
    }
}

void dspic33_can_test_transmit_abort_timing_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xbc00u + channel * 0x100u);
        Dspic33CanFrame output;
        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_transmit(cpu, channel, memory);
        dspic33_can_test_write_memory_word(cpu, memory, 2u);
        for (uint8_t word = 1u; word < 8u; word++) {
            dspic33_can_test_write_memory_word(cpu, memory + word * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u &&
                   !dspic33_can_transmit(cpu, channel, &output),
               "CAN abort test reaches the on-bus interval");
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) | 0x1000u));
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x0048u) == 0x0040u &&
                   dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_can_transmit(cpu, channel, &output),
               "CAN abort cancels the pending on-bus completion");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_transmit(cpu, channel, memory);
        dspic33_can_test_write_memory_word(cpu, memory, 2u);
        for (uint8_t word = 1u; word < 8u; word++) {
            dspic33_can_test_write_memory_word(cpu, memory + word * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u,
               "individual CAN abort reaches the on-bus interval");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x0083u);
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x0048u) == 0x0040u &&
                   dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_can_transmit(cpu, channel, &output),
               "clearing TXREQ aborts the active CAN transmission");
    }
}

void dspic33_can_test_transmit_pps_cases(TestState* state, Dspic33* cpu) {
    bool high;
    expect(state, !dspic33_can_pin(cpu, 64u, NULL), "CAN output rejects null pin level");
    expect(state, !dspic33_can_pin(cpu, 63u, &high), "CAN output rejects non-remappable pin");
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xbe00u + channel * 0x100u);
        uint8_t function = (uint8_t)(14u + channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, function);
        expect(state, dspic33_can_pin(cpu, 64u, &high) && high,
               "mapped CAN transmit pin is recessive while idle");
        dspic33_can_test_configure_transmit(cpu, channel, memory);
        for (uint8_t word = 0u; word < 8u; word++) {
            dspic33_can_test_write_memory_word(cpu, memory + word * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state, dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &high) && !high,
               "CAN transmit pin drives dominant start of frame");
        expect(state, dspic33_device_advance(cpu, 20u) && dspic33_can_pin(cpu, 64u, &high) && high,
               "CAN transmit pin inserts the sixth complementary stuffed bit");
        expect(state, dspic33_device_advance(cpu, 4u) && dspic33_can_pin(cpu, 64u, &high) && !high,
               "CAN transmit pin resumes frame data after stuffing");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x0083u);
        expect(state, dspic33_can_pin(cpu, 64u, &high) && high,
               "aborted CAN transmit pin returns recessive");
        dspic33_write_word(cpu, 0x0680u, (uint16_t)(function << 8u));
        expect(state, !dspic33_can_pin(cpu, 64u, &high) && dspic33_can_pin(cpu, 65u, &high) && high,
               "CAN transmit output follows PPS remapping");
        dspic33_write_word(cpu, 0x0760u,
                           (uint16_t)(dspic33_read_word(cpu, 0x0760u) | (uint16_t)(2u << channel)));
        expect(state, dspic33_device_advance(cpu, 1u) && !dspic33_can_pin(cpu, 65u, &high),
               "PMD releases the CAN transmit PPS output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, function);
        dspic33_can_test_configure_transmit(cpu, channel, memory);
        for (uint8_t word = 0u; word < 8u; word++) {
            dspic33_can_test_write_memory_word(cpu, memory + word * 2u, 0u);
        }
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state, dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &high) && !high,
               "CAN Sleep output test reaches dominant bus phase");
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state, dspic33_can_pin(cpu, 64u, &high) && high,
               "Sleep forces the CAN transmit pin recessive");
    }
}

bool dspic33_can_test_bridge_can_pins(Dspic33* cpu, uint8_t transmit_channel, uint8_t pin,
                                      uint8_t acknowledge_pin, uint64_t bit_cycles, int corrupt_bit,
                                      bool* acknowledge_observed) {
    uint16_t bit = 0u;
    while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << transmit_channel)) != 0u && bit < 160u) {
        bool high;
        bool acknowledge_high;
        if (dspic33_can_pin(cpu, acknowledge_pin, &acknowledge_high) && !acknowledge_high) {
            *acknowledge_observed = true;
        }
        if (!dspic33_can_pin(cpu, pin, &high)) {
            return false;
        }
        if (bit == corrupt_bit) {
            high = !high;
        }
        if (!dspic33_can_input_pin(cpu, pin, high && acknowledge_high, 0u) ||
            !dspic33_device_advance(cpu, bit_cycles)) {
            return false;
        }
        bit++;
    }
    return bit != 0u && bit < 160u && dspic33_device_advance(cpu, 32u);
}

static bool bridge_can_with_final_sample_glitch(Dspic33* cpu, uint8_t transmit_channel,
                                                uint8_t transmit_pin, uint8_t acknowledge_pin,
                                                uint8_t transmit_receive_pin, uint8_t receive_pin,
                                                bool glitch_transmitter,
                                                bool* acknowledge_observed) {
    uint16_t bit = 0u;
    while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << transmit_channel)) != 0u && bit < 160u) {
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
        if (bit == 0u &&
            !dspic33_can_input_pin(cpu, glitch_transmitter ? transmit_receive_pin : receive_pin,
                                   !bus_high, 0u)) {
            return false;
        }
        if (!dspic33_device_advance(cpu, 1u) ||
            !dspic33_can_input_pin(cpu, glitch_transmitter ? transmit_receive_pin : receive_pin,
                                   bus_high, 0u) ||
            !dspic33_device_advance(cpu, 1u)) {
            return false;
        }
        bit++;
    }
    return bit != 0u && bit < 160u && dspic33_device_advance(cpu, 32u);
}

void dspic33_can_test_triple_sample_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t glitch_transmitter = 0u; glitch_transmitter < 2u; glitch_transmitter++) {
        Dspic33CanFrame input =
            dspic33_can_test_frame((uint32_t)(0x360u + glitch_transmitter), false, false, 2u,
                                   (uint8_t)(0x80u + glitch_transmitter * 0x10u));
        Dspic33CanFrame output;
        bool acknowledge_observed = false;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, 0x4042u);
        dspic33_can_test_configure_receive(cpu, 1u, 0xda00u, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, 1u, 0u, input.identifier, false, 0x7ffu, true, 0u,
                                          0u);
        dspic33_can_test_enable_filter(cpu, 1u, 1u);
        dspic33_can_test_configure_transmit(cpu, 0u, 0xd800u);
        dspic33_can_test_write_transmit_frame(cpu, 0xd800u, &input);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_select_window(cpu, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, glitch_transmitter != 0u ? 0x0040u : 0u);
        dspic33_write_word(cpu, 0x0510u, 0u);
        dspic33_write_word(cpu, 0x0512u, glitch_transmitter == 0u ? 0x0040u : 0u);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        dspic33_can_test_set_mode(cpu, 1u, 0u);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   bridge_can_with_final_sample_glitch(cpu, 0u, 64u, 65u, 66u, 64u,
                                                       glitch_transmitter != 0u,
                                                       &acknowledge_observed),
               "CAN triple-sample bridge tolerates one final-sample glitch");
        expect(state,
               dspic33_can_test_receive_full(cpu, 1u, 0u) &&
                   (dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 0u &&
                   dspic33_can_test_memory_word(cpu, 0xda00u) ==
                       (uint16_t)(input.identifier << 2u) &&
                   (uint8_t)dspic33_can_test_memory_word(cpu, 0xda06u) == input.data[0] &&
                   (uint8_t)(dspic33_can_test_memory_word(cpu, 0xda06u) >> 8u) == input.data[1],
               "CAN triple-sample receiver uses the majority value");
        expect(state,
               acknowledge_observed && dspic33_can_transmit(cpu, 0u, &output) &&
                   output.identifier == input.identifier &&
                   (dspic33_read_word(cpu, 0x040eu) >> 8u) == 0u &&
                   (dspic33_read_word(cpu, 0x0430u) & 0x0018u) == 0u,
               "CAN triple-sample transmitter uses the majority value");
    }
}

static void prepare_resynchronization(Dspic33* cpu, uint16_t config1) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_write_word(cpu, 0x0410u, config1);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_can_input_pin(cpu, 64u, false, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_can_input_pin(cpu, 64u, true, 0u);
    dspic33_device_advance(cpu, 0u);
}

void dspic33_can_test_resynchronization_cases(TestState* state, Dspic33* cpu) {
    prepare_resynchronization(cpu, 0u);
    expect(state,
           cpu->io.can_rx_serial_count[0] == 1u && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 2u) && cpu->io.can_rx_serial_count[0] == 1u,
           "early CAN edge shortens Phase Segment 2");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 2u,
           "early CAN edge advances the next sample point by one TQ");

    prepare_resynchronization(cpu, 0u);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 1u,
           "late CAN edge lengthens Phase Segment 1");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 2u,
           "one-TQ SJW limits a late CAN resynchronization");

    prepare_resynchronization(cpu, 0x0040u);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 2u) && cpu->io.can_rx_serial_count[0] == 1u,
           "two-TQ SJW permits a larger CAN phase correction");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.can_rx_serial_count[0] == 2u,
           "two-TQ CAN resynchronization reaches its adjusted sample point");
}

static bool drive_can_to_intermission(Dspic33* cpu) {
    for (uint16_t bit = 0u; bit < 160u; bit++) {
        bool transmit_high;
        bool acknowledge_high;
        if ((cpu->io.can_intermission_active & 2u) != 0u) {
            return true;
        }
        if (!dspic33_can_pin(cpu, 64u, &transmit_high) ||
            !dspic33_can_pin(cpu, 65u, &acknowledge_high) ||
            !dspic33_can_input_pin(cpu, 66u, transmit_high && acknowledge_high, 0u) ||
            !dspic33_can_input_pin(cpu, 64u, transmit_high && acknowledge_high, 0u) ||
            !dspic33_device_advance(cpu, 4u)) {
            return false;
        }
    }
    return false;
}

void dspic33_can_test_overload_frame_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame input = dspic33_can_test_frame(0x365u, false, false, 1u, 0xa0u);
    bool high;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
    dspic33_write_word(cpu, 0x06d4u, 0x4042u);
    dspic33_can_test_configure_receive(cpu, 1u, 0xda00u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 1u, 0u, input.identifier, false, 0x7ffu, true, 0u, 0u);
    dspic33_can_test_enable_filter(cpu, 1u, 1u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xd800u);
    dspic33_can_test_write_transmit_frame(cpu, 0xd800u, &input);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_select_window(cpu, 1u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_write_word(cpu, 0x0510u, 0u);
    dspic33_write_word(cpu, 0x0512u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_can_test_set_mode(cpu, 1u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state, dspic33_device_advance(cpu, 8u), "CAN overload source reaches the bus");
    expect(state, drive_can_to_intermission(cpu), "valid CAN frame reaches Intermission");
    expect(state, cpu->io.can_rx_serial_count[1] != 0u,
           "valid CAN serial frame completes before Intermission");
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_can_input_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_can_pin(cpu, 65u, &high) && !high && cpu->io.can_overload_count[1] == 1u,
           "dominant Intermission edge starts a CAN overload flag");
    expect(state, dspic33_device_advance(cpu, 23u) && dspic33_can_pin(cpu, 65u, &high) && !high,
           "CAN overload flag remains dominant for six bits");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_can_pin(cpu, 65u, &high) && high,
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
               dspic33_can_pin(cpu, 65u, &high) && high && (cpu->io.can_overload_active & 2u) == 0u,
           "CAN suppresses a third sequential overload frame");
}
