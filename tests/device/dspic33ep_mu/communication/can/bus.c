#include "device/dspic33ep_mu/communication/can/internal.h"

static void loopback_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        const uint16_t can_base = bases[channel_index];
        const uint8_t bus_pin = (uint8_t)(65u + channel_index);
        const uint32_t receive_memory_address = (uint32_t)(0xd000u + channel_index * 0x400u);
        const uint32_t transmit_memory_address = receive_memory_address + 0x200u;
        const Dspic33CanFrame received_frame =
            dspic33_can_test_frame(channel_index == 0u ? 0x345u : 0x1234567u, channel_index != 0u,
                                   channel_index != 0u, channel_index == 0u ? 8u : 3u, 0x40u);
        Dspic33CanFrame transmitted_frame;
        uint16_t transmit_cycle_count = 0u;
        uint8_t receive_cycle_count = 0u;
        bool loopback_isolated = true;

        dspic33_reset(cpu, 0u);
        for (uint8_t word_index = 0u; word_index < 8u; word_index++) {
            dspic33_can_test_write_memory_word(cpu, receive_memory_address + word_index * 2u,
                                               0xa55au);
        }
        dspic33_write_word(cpu, channel_index == 0u ? 0x0680u : 0x0682u,
                           channel_index == 0u ? 0x0e00u : 0x000fu);
        dspic33_can_test_configure_receive(cpu, channel_index, receive_memory_address, 4u, 0u);
        dspic33_can_test_configure_filter(
            cpu, channel_index, 0u, received_frame.identifier, received_frame.extended,
            received_frame.extended ? 0x1fffffffu : 0x7ffu, true, 1u, 0u);
        dspic33_can_test_enable_filter(cpu, channel_index, 1u);
        dspic33_can_test_configure_transmit(cpu, channel_index, transmit_memory_address);
        dspic33_can_test_write_transmit_frame(cpu, transmit_memory_address, &received_frame);
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x12u), 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x0cu), 3u);
        dspic33_can_test_set_mode(cpu, channel_index, 2u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_on_bus & (uint8_t)(1u << channel_index)) != 0u &&
                   !dspic33_can_test_receive_full(cpu, channel_index, 1u) &&
                   ((dspic33_read_word(cpu, can_base) >> 5u) & 7u) == 2u,
               "CAN loopback enters transmission without early receive completion");

        while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << channel_index)) != 0u &&
               transmit_cycle_count < 800u) {
            bool bus_level = false;
            const bool pin_readable = dspic33_can_pin(cpu, bus_pin, &bus_level);
            const bool receive_incomplete = !dspic33_can_test_receive_full(cpu, channel_index, 1u);
            const bool transfer_advanced = dspic33_device_advance(cpu, 1u);

            loopback_isolated = loopback_isolated && pin_readable && bus_level &&
                                receive_incomplete && transfer_advanced;
            transmit_cycle_count++;
        }
        expect(state,
               loopback_isolated && transmit_cycle_count != 0u && transmit_cycle_count < 800u &&
                   (dspic33_read_word(cpu, (uint16_t)(can_base + 0x0au)) & 3u) == 1u &&
                   !dspic33_can_transmit(cpu, channel_index, &transmitted_frame),
               "CAN loopback completes transmit internally without driving or exporting a frame");
        expect(state,
               cpu->io.can_rx[channel_index].count != 0u ||
                   (cpu->io.can_rx_busy & (uint8_t)(1u << channel_index)) != 0u,
               "CAN loopback queues or starts internal receive delivery");
        dspic33_can_test_clear_interrupt_flag(cpu, event_irqs[channel_index]);
        expect(state, !dspic33_can_test_interrupt_flag(cpu, event_irqs[channel_index]),
               "CAN loopback transmit interrupt can be acknowledged before receive completion");

        while (!dspic33_can_test_receive_full(cpu, channel_index, 1u) &&
               receive_cycle_count < 64u) {
            const bool transfer_advanced = dspic33_device_advance(cpu, 1u);

            loopback_isolated = loopback_isolated && transfer_advanced;
            receive_cycle_count++;
        }
        expect(state, loopback_isolated && receive_cycle_count != 0u && receive_cycle_count < 64u,
               "CAN loopback completes internal receive DMA within its bounded latency");
        expect(state, dspic33_can_test_receive_full(cpu, channel_index, 1u),
               "CAN loopback marks the selected receive buffer full");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(can_base + 0x0au)) & 3u) == 3u,
               "CAN loopback raises transmit then receive status");
        expect(state, dspic33_can_test_interrupt_flag(cpu, event_irqs[channel_index]),
               "CAN loopback raises the enabled module interrupt");

        bool frame_words_match = true;
        for (uint8_t word_index = 0u; word_index < 8u; word_index++) {
            frame_words_match =
                frame_words_match &&
                dspic33_can_test_memory_word(cpu, receive_memory_address + 16u + word_index * 2u) ==
                    dspic33_can_test_memory_word(cpu, transmit_memory_address + word_index * 2u) &&
                dspic33_can_test_memory_word(cpu, receive_memory_address + word_index * 2u) ==
                    0xa55au;
        }
        expect(state, frame_words_match, "CAN loopback preserves every encoded frame word");
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel_index)) == 0u &&
                   (cpu->io.can_rx_busy & (uint8_t)(1u << channel_index)) == 0u,
               "CAN loopback finishes both internal engines");
    }
}

void dspic33_can_test_bus_groups(TestState* state, Dspic33* cpu) {
    loopback_cases(state, cpu);
    dspic33_can_test_arbitration_field_cases(state, cpu);
    dspic33_can_test_arbitration_cases(state, cpu);
    dspic33_can_test_acknowledge_error_cases(state, cpu);
    dspic33_can_test_transmit_error_variant_cases(state, cpu);
    dspic33_can_test_receive_error_cases(state, cpu);
    dspic33_can_test_bus_off_recovery_cases(state, cpu);
    dspic33_can_test_error_counter_recovery_cases(state, cpu);
    dspic33_can_test_receive_pps_cases(state, cpu);
    dspic33_can_test_receive_pps_qualification_cases(state, cpu);
}

static bool drive_shared_can_bus(Dspic33* can1, Dspic33* can2, uint8_t active_channel_index,
                                 uint64_t bit_duration) {
    uint16_t bit_count = 0u;
    Dspic33* active_controller = active_channel_index == 0u ? can1 : can2;

    while ((active_controller->io.can_tx_on_bus & (uint8_t)(1u << active_channel_index)) != 0u &&
           bit_count < 160u) {
        bool can1_level;
        bool can2_level;
        bool bus_level;

        if (!dspic33_can_pin(can1, 65u, &can1_level) || !dspic33_can_pin(can2, 66u, &can2_level)) {
            return false;
        }
        bus_level = can1_level && can2_level;
        if (!dspic33_can_input_pin(can1, 64u, bus_level, 0u) ||
            !dspic33_can_input_pin(can2, 64u, bus_level, 0u) ||
            !dspic33_device_advance(can1, bit_duration) ||
            !dspic33_device_advance(can2, bit_duration)) {
            return false;
        }
        bit_count++;
    }
    return bit_count != 0u && bit_count < 160u;
}

void dspic33_can_test_arbitration_field_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame arbitration_frames[4][2];
    Dspic33CanFrame transmitted_frame;
    Dspic33* peer_cpu = &(Dspic33){0};

    arbitration_frames[0][0] = dspic33_can_test_frame(0x155u, false, false, 0u, 0u);
    arbitration_frames[0][1] = dspic33_can_test_frame(0x155u, false, true, 0u, 0u);
    arbitration_frames[1][0] = dspic33_can_test_frame(0x155u, false, false, 0u, 0u);
    arbitration_frames[1][1] = dspic33_can_test_frame(0x5540000u, true, false, 0u, 0u);
    arbitration_frames[2][0] = dspic33_can_test_frame(0x1550000u, true, false, 0u, 0u);
    arbitration_frames[2][1] = dspic33_can_test_frame(0x1550001u, true, false, 0u, 0u);
    arbitration_frames[3][0] = dspic33_can_test_frame(0x1550000u, true, false, 0u, 0u);
    arbitration_frames[3][1] = dspic33_can_test_frame(0x1550000u, true, true, 0u, 0u);
    expect(state, dspic33_initialize(peer_cpu), "initialize independent CAN arbitration contender");
    for (uint8_t case_index = 0u; case_index < 4u; case_index++) {
        dspic33_reset(cpu, 0u);
        dspic33_reset(peer_cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0e00u);
        dspic33_write_word(cpu, 0x06d4u, 0x0040u);
        dspic33_write_word(peer_cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(peer_cpu, 0x0e3eu, 0u);
        dspic33_write_word(peer_cpu, 0x0682u, 0x000fu);
        dspic33_write_word(peer_cpu, 0x06d4u, 0x4000u);
        dspic33_can_test_configure_transmit(cpu, 0u, 0xd400u);
        dspic33_can_test_configure_transmit(peer_cpu, 1u, 0xd600u);
        dspic33_can_test_write_transmit_frame(cpu, 0xd400u, &arbitration_frames[case_index][0]);
        dspic33_can_test_write_transmit_frame(peer_cpu, 0xd600u,
                                              &arbitration_frames[case_index][1]);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_select_window(peer_cpu, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, 0u);
        dspic33_write_word(peer_cpu, 0x0510u, 0u);
        dspic33_write_word(peer_cpu, 0x0512u, 0u);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        dspic33_can_test_set_mode(peer_cpu, 1u, 0u);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        dspic33_write_word(peer_cpu, 0x0530u, 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) && dspic33_device_advance(peer_cpu, 8u) &&
                   (cpu->io.can_tx_on_bus & 1u) != 0u && (peer_cpu->io.can_tx_on_bus & 2u) != 0u,
               "CAN arbitration field contenders start together");
        expect(state, drive_shared_can_bus(cpu, peer_cpu, 0u, 4u),
               "CAN arbitration field selects the dominant contender");
        expect(state,
               (dspic33_read_word(peer_cpu, 0x0530u) & 0x0028u) == 0x0028u &&
                   dspic33_can_transmit(cpu, 0u, &transmitted_frame) &&
                   transmitted_frame.identifier == arbitration_frames[case_index][0].identifier &&
                   transmitted_frame.extended == arbitration_frames[case_index][0].extended &&
                   transmitted_frame.remote == arbitration_frames[case_index][0].remote,
               "CAN arbitration field records loss and completes the winner");
    }
    dspic33_release(peer_cpu);
}

void dspic33_can_test_arbitration_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame higher = dspic33_can_test_frame(0x400u, false, false, 2u, 0xa0u);
    Dspic33CanFrame lower = dspic33_can_test_frame(0u, false, false, 2u, 0xb0u);
    Dspic33CanFrame output;
    Dspic33 winner;
    expect(state, dspic33_initialize(&winner), "initialize independent CAN arbitration winner");
    dspic33_reset(cpu, 0u);
    dspic33_reset(&winner, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0e00u);
    dspic33_write_word(cpu, 0x06d4u, 0x0040u);
    dspic33_write_word(&winner, 0x0e30u, 0xffffu);
    dspic33_write_word(&winner, 0x0e3eu, 0u);
    dspic33_write_word(&winner, 0x0682u, 0x000fu);
    dspic33_write_word(&winner, 0x06d4u, 0x4000u);
    dspic33_can_test_configure_receive(cpu, 0u, 0xd000u, 4u, 0u);
    dspic33_can_test_configure_receive(&winner, 1u, 0xd200u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, lower.identifier, false, 0x7ffu, true, 0u, 0u);
    dspic33_can_test_configure_filter(&winner, 1u, 0u, higher.identifier, false, 0x7ffu, true, 0u,
                                      0u);
    dspic33_can_test_enable_filter(cpu, 0u, 1u);
    dspic33_can_test_enable_filter(&winner, 1u, 1u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xd400u);
    dspic33_can_test_configure_transmit(&winner, 1u, 0xd600u);
    dspic33_can_test_write_transmit_frame(cpu, 0xd400u, &higher);
    dspic33_can_test_write_transmit_frame(&winner, 0xd600u, &lower);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_select_window(&winner, 1u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_write_word(&winner, 0x0510u, 0u);
    dspic33_write_word(&winner, 0x0512u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_can_test_set_mode(&winner, 1u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    dspic33_write_word(&winner, 0x0530u, 0x008bu);
    expect(state,
           dspic33_device_advance(cpu, 8u) && dspic33_device_advance(&winner, 8u) &&
               (cpu->io.can_tx_on_bus & 1u) != 0u && (winner.io.can_tx_on_bus & 2u) != 0u,
           "competing CAN transmissions enter the bus together");
    expect(state, drive_shared_can_bus(cpu, &winner, 1u, 4u),
           "lower identifier wins CAN arbitration");
    expect(state,
           (dspic33_read_word(cpu, 0x0430u) & 0x0028u) == 0x0028u &&
               (cpu->io.can_tx_busy & 1u) != 0u && (cpu->io.can_tx_on_bus & 1u) == 0u &&
               !dspic33_can_transmit(cpu, 0u, &output),
           "losing CAN transmission records TXLARB and begins an automatic retry");
    expect(state,
           dspic33_can_transmit(&winner, 1u, &output) && output.identifier == lower.identifier,
           "winning CAN frame completes before the retry");
    expect(state,
           cpu->io.can_rx_serial_count[0] != 0u && dspic33_device_advance(cpu, 7u) &&
               dspic33_device_advance(&winner, 7u) && (cpu->io.can_tx_retry_wait & 1u) == 0u &&
               (cpu->io.can_tx_on_bus & 1u) != 0u,
           "losing node monitors the winner and retries after intermission");
    expect(state, drive_shared_can_bus(cpu, &winner, 0u, 4u),
           "retried CAN transmission completes on the shared bus");
    expect(state,
           dspic33_device_advance(cpu, 8u) && dspic33_can_transmit(cpu, 0u, &output) &&
               output.identifier == higher.identifier &&
               (dspic33_read_word(cpu, 0x0430u) & 0x0078u) == 0x0020u &&
               winner.io.can_rx_serial_count[1] != 0u,
           "successful retry preserves TXLARB and clears TXREQ without errors");
    dspic33_release(&winner);
}

static bool drive_unacknowledged_can_frame(Dspic33* cpu, uint8_t channel_index,
                                           uint8_t transmit_pin_index, uint8_t receive_pin_index,
                                           uint64_t bit_duration) {
    uint16_t bit_count = 0u;

    while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << channel_index)) != 0u && bit_count < 160u) {
        bool bus_level;

        if (!dspic33_can_pin(cpu, transmit_pin_index, &bus_level) ||
            !dspic33_can_input_pin(cpu, receive_pin_index, bus_level, 0u) ||
            !dspic33_device_advance(cpu, bit_duration)) {
            return false;
        }
        bit_count++;
    }
    return bit_count != 0u && bit_count < 160u;
}

void dspic33_can_test_acknowledge_error_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel_index = 0u; channel_index < DSPIC33_CAN_COUNT; channel_index++) {
        const uint16_t can_base = bases[channel_index];
        const uint32_t memory_address = (uint32_t)(0xdc00u + channel_index * 0x100u);
        const uint8_t pps_output_function = (uint8_t)(14u + channel_index);
        Dspic33CanFrame transmitted_frame;
        bool pin_level;

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, pps_output_function);
        dspic33_write_word(cpu, 0x06d4u, channel_index == 0u ? 65u : (uint16_t)(65u << 8u));
        dspic33_can_test_configure_transmit(cpu, channel_index, memory_address);
        const Dspic33CanFrame received_frame = dspic33_can_test_frame(
            (uint32_t)(0x240u + channel_index), false, false, 1u, (uint8_t)(0xc0u + channel_index));
        dspic33_can_test_write_transmit_frame(cpu, memory_address, &received_frame);
        dspic33_can_test_select_window(cpu, channel_index, false);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x12u), 0u);
        dspic33_can_test_set_mode(cpu, channel_index, 0u);
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_on_bus & (uint8_t)(1u << channel_index)) != 0u,
               "unacknowledged CAN frame reaches the bus");
        expect(state, drive_unacknowledged_can_frame(cpu, channel_index, 64u, 65u, 4u),
               "unacknowledged CAN frame reaches the ACK slot");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(can_base + 0x30u)) & 0x0018u) == 0x0018u &&
                   (dspic33_read_word(cpu, (uint16_t)(can_base + 0x0eu)) >> 8u) == 8u &&
                   (dspic33_read_word(cpu, (uint16_t)(can_base + 0x0au)) & 1u) == 0u &&
                   (cpu->io.can_tx_error_active & (uint8_t)(1u << channel_index)) != 0u &&
                   !dspic33_can_transmit(cpu, channel_index, &transmitted_frame),
               "missing CAN ACK sets TXERR and TEC without completing transmission");
        expect(state, dspic33_can_pin(cpu, 64u, &pin_level) && !pin_level,
               "CAN acknowledge error emits an active error flag");
        if (channel_index == 0u) {
            Dspic33 copy_cpu;
            bool copy_pin_level;

            expect(state, dspic33_initialize(&copy_cpu), "initialize CAN error-frame copy");
            expect(state,
                   dspic33_copy(&copy_cpu, cpu) && copy_cpu.io.can_tx_error_active == 1u &&
                       copy_cpu.io.can_tx_error_start_cycle[0] ==
                           cpu->io.can_tx_error_start_cycle[0],
                   "copy preserves active CAN error-frame phase");
            expect(state,
                   dspic33_device_advance(cpu, 24u) && dspic33_device_advance(&copy_cpu, 24u) &&
                       dspic33_can_pin(cpu, 64u, &pin_level) && pin_level &&
                       dspic33_can_pin(&copy_cpu, 64u, &copy_pin_level) && copy_pin_level &&
                       copy_cpu.io.can_tx_error_active == 1u,
                   "copied CAN error frames enter the recessive delimiter together");
            dspic33_release(&copy_cpu);
        } else {
            expect(state,
                   dspic33_device_advance(cpu, 24u) && dspic33_can_pin(cpu, 64u, &pin_level) &&
                       pin_level &&
                       (cpu->io.can_tx_error_active & (uint8_t)(1u << channel_index)) != 0u,
                   "CAN active error flag is followed by a recessive delimiter");
        }
        expect(state,
               dspic33_device_advance(cpu, 52u) &&
                   (cpu->io.can_tx_error_active & (uint8_t)(1u << channel_index)) == 0u &&
                   (cpu->io.can_tx_on_bus & (uint8_t)(1u << channel_index)) != 0u,
               "unacknowledged CAN transmission automatically retries after intermission");
        dspic33_write_word(cpu, (uint16_t)(can_base + 0x30u), 0x0093u);
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel_index)) == 0u &&
                   (cpu->io.can_tx_retry_wait & (uint8_t)(1u << channel_index)) == 0u &&
                   dspic33_can_pin(cpu, 64u, &pin_level) && pin_level,
               "aborting a retried CAN frame clears error-bus state");
    }
}

void dspic33_can_test_transmit_error_variant_cases(TestState* state, Dspic33* cpu) {
    bool high;
    Dspic33CanFrame input = dspic33_can_test_frame(0u, false, false, 0u, 0u);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 14u);
    dspic33_write_word(cpu, 0x06d4u, 65u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xdc00u);
    dspic33_can_test_write_transmit_frame(cpu, 0xdc00u, &input);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state, dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &high) && !high,
           "CAN dominant-bit mismatch test reaches SOF");
    expect(state, dspic33_can_input_pin(cpu, 65u, true, 0u) && dspic33_device_advance(cpu, 4u),
           "CAN transmitter samples recessive while driving dominant");
    expect(state,
           (dspic33_read_word(cpu, 0x0430u) & 0x0038u) == 0x0018u &&
               (dspic33_read_word(cpu, 0x040eu) >> 8u) == 8u &&
               (cpu->io.can_tx_error_active & 1u) != 0u,
           "dominant CAN mismatch is a bit error rather than arbitration loss");
    expect(state, dspic33_can_pin(cpu, 64u, &high) && !high,
           "active CAN bit error drives a dominant error flag");
    dspic33_write_word(cpu, 0x0430u, 0x0093u);

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 14u);
    dspic33_write_word(cpu, 0x06d4u, 65u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xdc00u);
    dspic33_can_test_write_transmit_frame(cpu, 0xdc00u, &input);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_error(cpu, 0u, true, 120u, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x040eu) >> 8u) == 120u,
           "CAN transmitter reaches the error-passive boundary precursor");
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state,
           dspic33_device_advance(cpu, 8u) && drive_unacknowledged_can_frame(cpu, 0u, 64u, 65u, 4u),
           "error-passive CAN transmitter encounters a missing ACK");
    expect(state,
           (dspic33_read_word(cpu, 0x040eu) >> 8u) == 128u &&
               (dspic33_read_word(cpu, 0x040au) & 0x1000u) != 0u &&
               (dspic33_read_word(cpu, 0x0430u) & 0x0018u) == 0x0018u,
           "missing ACK transitions the CAN transmitter to error-passive");
    expect(state,
           dspic33_can_pin(cpu, 64u, &high) && high && (cpu->io.can_tx_error_active & 1u) != 0u,
           "error-passive CAN flag remains recessive");
    expect(state, dspic33_device_advance(cpu, 99u),
           "error-passive CAN Suspend Transmission advances");
    expect(state, (cpu->io.can_tx_error_active & 1u) != 0u,
           "error-passive CAN flag remains active through Suspend Transmission");
    expect(state, (cpu->io.can_tx_on_bus & 1u) == 0u,
           "error-passive CAN transmitter remains off-bus during suspension");
    expect(state,
           dspic33_device_advance(cpu, 1u) && (cpu->io.can_tx_error_active & 1u) == 0u &&
               (cpu->io.can_tx_on_bus & 1u) == 0u,
           "error-passive CAN suspension ends after eight additional bits");
    expect(state, dspic33_device_advance(cpu, 8u) && (cpu->io.can_tx_on_bus & 1u) != 0u,
           "error-passive CAN transmission retries after suspension");
    dspic33_write_word(cpu, 0x0430u, 0x0093u);
    expect(state,
           cpu->io.can_tx_busy == 0u && cpu->io.can_tx_retry_wait == 0u &&
               dspic33_can_pin(cpu, 64u, &high) && high,
           "aborting an error-passive retry releases the CAN bus");
}

static bool drive_until_receive_error(Dspic33* cpu, uint16_t corrupt_bit) {
    for (uint16_t bit = 0u; bit < 160u; bit++) {
        bool transmit_high;
        bool receive_high;
        if ((cpu->io.can_rx_error_active & 2u) != 0u) {
            return true;
        }
        if (!dspic33_can_pin(cpu, 64u, &transmit_high) ||
            !dspic33_can_pin(cpu, 65u, &receive_high)) {
            return false;
        }
        if (bit == corrupt_bit) {
            transmit_high = !transmit_high;
        }
        if (!dspic33_can_input_pin(cpu, 64u, transmit_high && receive_high, 0u) ||
            !dspic33_device_advance(cpu, 4u)) {
            return false;
        }
    }
    return false;
}

void dspic33_can_test_receive_error_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t corrupt_bits[] = {5u, 30u, 42u};
    for (uint8_t index = 0u; index < 3u; index++) {
        bool high;
        Dspic33CanFrame input = dspic33_can_test_frame(0u, false, false, 0u, 0u);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, 0x4000u);
        dspic33_can_test_configure_transmit(cpu, 0u, 0xde00u);
        dspic33_can_test_write_transmit_frame(cpu, 0xde00u, &input);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_select_window(cpu, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, 0u);
        dspic33_write_word(cpu, 0x0510u, 0u);
        dspic33_write_word(cpu, 0x0512u, 0u);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        dspic33_can_test_set_mode(cpu, 1u, 0u);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   drive_until_receive_error(cpu, corrupt_bits[index]),
               "physical CAN corruption activates receiver error handling");
        expect(state,
               (dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 1u &&
                   (dspic33_read_word(cpu, 0x050au) & 0x0080u) != 0u &&
                   (dspic33_read_word(cpu, 0x050au) & 0x0020u) == 0u &&
                   !dspic33_can_test_receive_full(cpu, 1u, 0u),
               "CAN receiver error updates REC and IVRIF without B1 ERRIF");
        expect(state, dspic33_can_pin(cpu, 65u, &high) && !high,
               "error-active CAN receiver drives a dominant error flag");
        dspic33_write_word(cpu, 0x0430u, 0x0083u);
        expect(state,
               dspic33_device_advance(cpu, 24u) && dspic33_can_pin(cpu, 65u, &high) && high &&
                   dspic33_device_advance(cpu, 32u) && (cpu->io.can_rx_error_active & 2u) == 0u,
               "CAN receiver error flag ends after its delimiter");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
    dspic33_write_word(cpu, 0x06d4u, 0x4000u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xde00u);
    Dspic33CanFrame passive = dspic33_can_test_frame(0u, false, false, 0u, 0u);
    dspic33_can_test_write_transmit_frame(cpu, 0xde00u, &passive);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_select_window(cpu, 1u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_write_word(cpu, 0x0510u, 0u);
    dspic33_write_word(cpu, 0x0512u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_can_test_set_mode(cpu, 1u, 0u);
    expect(state, dspic33_can_error(cpu, 1u, false, 127u, 0u) && dspic33_device_advance(cpu, 0u),
           "CAN receiver reaches the error-passive boundary precursor");
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state,
           dspic33_device_advance(cpu, 8u) && drive_until_receive_error(cpu, 5u) &&
               (dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 128u &&
               (dspic33_read_word(cpu, 0x050au) & 0x0800u) != 0u,
           "physical CAN corruption transitions the receiver to error-passive");
    bool high;
    expect(state,
           dspic33_can_pin(cpu, 65u, &high) && high && (cpu->io.can_rx_error_active & 2u) != 0u,
           "error-passive CAN receiver flag remains recessive");
}

static bool drive_can_recessive_bits(Dspic33* cpu, uint8_t pin, uint16_t count) {
    for (uint16_t bit = 0u; bit < count; bit++) {
        if (!dspic33_can_input_pin(cpu, pin, true, 0u) || !dspic33_device_advance(cpu, 4u)) {
            return false;
        }
    }
    return true;
}

void dspic33_can_test_bus_off_recovery_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xde00u + channel * 0x100u);
        uint8_t function = (uint8_t)(14u + channel);
        bool high;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, function);
        dspic33_write_word(cpu, 0x06d4u, channel == 0u ? 65u : (uint16_t)(65u << 8u));
        dspic33_can_test_configure_transmit(cpu, channel, memory);
        Dspic33CanFrame input =
            dspic33_can_test_frame((uint32_t)(0x300u + channel), false, false, 0u, 0u);
        dspic33_can_test_write_transmit_frame(cpu, memory, &input);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x12u), 0u);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        expect(state,
               dspic33_can_error(cpu, channel, true, 248u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) >> 8u) == 248u,
               "CAN transmitter reaches the bus-off boundary precursor");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   drive_unacknowledged_can_frame(cpu, channel, 64u, 65u, 4u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x2000u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x0018u) == 0x0018u,
               "missing ACK at TEC 248 enters CAN bus-off");
        expect(state,
               dspic33_can_pin(cpu, 64u, &high) && high && cpu->io.can_tx_error_active == 0u &&
                   cpu->io.can_tx_retry_wait == 0u,
               "bus-off CAN controller releases the bus and suppresses retry");
        expect(state,
               drive_can_recessive_bits(cpu, 65u, 10u) &&
                   dspic33_can_input_pin(cpu, 65u, false, 0u) && dspic33_device_advance(cpu, 4u) &&
                   drive_can_recessive_bits(cpu, 65u, 1407u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x2000u) != 0u &&
                   cpu->io.can_bus_off_recessive_bits[channel] == 1407u,
               "dominant CAN bit resets the bus-off recovery sequence");
        expect(state,
               drive_can_recessive_bits(cpu, 65u, 1u) && dspic33_device_advance(cpu, 4u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x3f00u) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   (cpu->io.can_tx_on_bus & (uint8_t)(1u << channel)) != 0u,
               "CAN recovers after 128 occurrences of 11 recessive bits");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x0093u);
    }
}

void dspic33_can_test_error_counter_recovery_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xe000u + channel * 0x100u);
        Dspic33CanFrame input = dspic33_can_test_frame(0x234u, false, false, 1u, 0x5au);
        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel, memory, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, channel, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, channel, 1u);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        expect(state,
               dspic33_can_error(cpu, channel, false, 1u, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_can_receive(cpu, channel, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) & 0x00ffu) == 0u,
               "successful CAN reception decrements REC below error-passive");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel, memory, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, channel, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, channel, 1u);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        expect(
            state,
            dspic33_can_error(cpu, channel, false, 128u, 0u) && dspic33_device_advance(cpu, 0u) &&
                dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 32u) &&
                (dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) & 0x00ffu) == 127u &&
                (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0800u) == 0u,
            "successful CAN reception leaves error-passive in the documented range");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel, memory, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, channel, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, channel, 1u);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        bool prepared =
            dspic33_can_error(cpu, channel, false, 5u, 0u) && dspic33_device_advance(cpu, 0u);
        dspic33_can_test_set_mode(cpu, channel, 3u);
        expect(state,
               prepared && dspic33_can_receive(cpu, channel, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) & 0x00ffu) == 5u,
               "listen-only CAN reception freezes REC");
    }
}

void dspic33_can_test_receive_pps_cases(TestState* state, Dspic33* cpu) {
    expect(state, !dspic33_can_input_pin(cpu, 63u, true, 0u),
           "CAN input rejects non-remappable pin");
    for (uint8_t transmit_channel = 0u; transmit_channel < DSPIC33_CAN_COUNT; transmit_channel++) {
        uint8_t receive_channel = (uint8_t)(transmit_channel ^ 1u);
        uint8_t pin = (uint8_t)(64u + transmit_channel);
        uint16_t transmit_base = bases[transmit_channel];
        uint16_t receive_base = bases[receive_channel];
        uint32_t transmit_memory = (uint32_t)(0xd800u + transmit_channel * 0x100u);
        uint32_t receive_memory = (uint32_t)(0xda00u + transmit_channel * 0x100u);
        Dspic33CanFrame input = dspic33_can_test_frame(transmit_channel == 0u ? 0x345u : 0x1234567u,
                                                       transmit_channel != 0u, false, 3u,
                                                       (uint8_t)(0x60u + transmit_channel * 0x10u));
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, (uint16_t)(pin | ((uint16_t)pin << 8u)));
        dspic33_can_test_configure_receive(cpu, receive_channel, receive_memory, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, receive_channel, 0u, input.identifier,
                                          input.extended, input.extended ? 0x1fffffffu : 0x7ffu,
                                          true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, receive_channel, 1u);
        dspic33_can_test_configure_transmit(cpu, transmit_channel, transmit_memory);
        dspic33_can_test_write_transmit_frame(cpu, transmit_memory, &input);
        dspic33_can_test_select_window(cpu, transmit_channel, false);
        dspic33_can_test_select_window(cpu, receive_channel, false);
        dspic33_write_word(cpu, (uint16_t)(transmit_base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(transmit_base + 0x12u),
                           transmit_channel == 0u ? 0u : 0x0311u);
        dspic33_write_word(cpu, (uint16_t)(receive_base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(receive_base + 0x12u),
                           transmit_channel == 0u ? 0u : 0x0311u);
        dspic33_can_test_set_mode(cpu, transmit_channel, 0u);
        dspic33_can_test_set_mode(cpu, receive_channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(transmit_base + 0x30u), 0x008bu);
        bool acknowledge_observed = false;
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   dspic33_can_test_bridge_can_pins(
                       cpu, transmit_channel, pin, (uint8_t)(65u - transmit_channel),
                       transmit_channel == 0u ? 4u : 10u, -1, &acknowledge_observed),
               "CAN PPS serial frame bridge advances");
        expect(state,
               dspic33_can_test_receive_full(cpu, receive_channel, 0u) &&
                   cpu->io.can_rx_serial_count[receive_channel] != 0u &&
                   (cpu->io.can_rx_serial_active & (uint8_t)(1u << receive_channel)) == 0u,
               "CAN PPS receiver accepts a complete stuffed frame");
        expect(state,
               dspic33_can_test_memory_word(cpu, receive_memory) ==
                       (uint16_t)(((input.extended ? (input.identifier >> 18u) & 0x7ffu
                                                   : input.identifier)
                                   << 2u) |
                                  (input.extended ? 3u : 0u)) &&
                   (uint8_t)dspic33_can_test_memory_word(cpu, receive_memory + 6u) ==
                       input.data[0] &&
                   (uint8_t)(dspic33_can_test_memory_word(cpu, receive_memory + 6u) >> 8u) ==
                       input.data[1],
               "CAN PPS receiver preserves header and payload bits");
        expect(state,
               acknowledge_observed &&
                   (dspic33_read_word(cpu, (uint16_t)(transmit_base + 0x30u)) & 0x0010u) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(transmit_base + 0x0eu)) >> 8u) == 0u,
               "CAN PPS receiver drives the acknowledge slot dominant");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
    dspic33_write_word(cpu, 0x06d4u, 0x4000u);
    dspic33_can_test_configure_receive(cpu, 1u, 0xdc00u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 1u, 0u, 0u, false, 0x7ffu, true, 0u, 0u);
    dspic33_can_test_enable_filter(cpu, 1u, 1u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xde00u);
    Dspic33CanFrame invalid = dspic33_can_test_frame(0u, false, false, 0u, 0u);
    dspic33_can_test_write_transmit_frame(cpu, 0xde00u, &invalid);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_select_window(cpu, 1u, false);
    dspic33_write_word(cpu, 0x050cu, 0x0080u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_can_test_set_mode(cpu, 1u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    bool acknowledge_observed = false;
    expect(state,
           dspic33_device_advance(cpu, 8u) &&
               dspic33_can_test_bridge_can_pins(cpu, 0u, 64u, 65u, 4u, 42, &acknowledge_observed),
           "corrupted CAN PPS frame advances");
    expect(state,
           (dspic33_read_word(cpu, 0x050au) & 0x0080u) != 0u &&
               !dspic33_can_test_receive_full(cpu, 1u, 0u),
           "corrupted CAN PPS frame raises IVRIF without receive data");
}

void dspic33_can_test_receive_pps_qualification_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t modes[] = {0u, 3u, 7u, 2u, 4u, 1u};
    for (uint8_t index = 0u; index < sizeof(modes); index++) {
        uint8_t mode = modes[index];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x06d4u, 64u);
        dspic33_can_test_set_mode(cpu, 0u, mode);
        expect(state,
               dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
                   (((cpu->io.can_rx_serial_active & 1u) != 0u) ==
                    (mode == 0u || mode == 3u || mode == 7u)),
               "CAN receive mode qualifies physical start of frame");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xfffeu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_serial_active & 1u) == 0u,
           "CAN PPS receiver rejects an output pin");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0x0040u);
    dspic33_write_word(cpu, 0x06d4u, 70u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 70u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_serial_active & 1u) == 0u,
           "CAN PPS receiver rejects an analog pin");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 65u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_serial_active & 1u) == 0u,
           "CAN PPS receiver rejects an unmapped pin");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0760u, (uint16_t)(dspic33_read_word(cpu, 0x0760u) | 2u));
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) && (cpu->io.can_rx_serial_active & 1u) == 0u,
           "PMD suppresses the CAN PPS receiver");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x040au) & 0x0040u) == 0u,
           "disabled CAN wake filter rejects physical bus activity");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_write_word(cpu, 0x0412u, 0x4000u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x040au) & 0x0040u) != 0u,
           "enabled CAN wake filter accepts physical bus activity");
}
