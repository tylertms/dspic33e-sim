#include "device/dspic33ep_mu/communication/can/internal.h"

void dspic33_can_test_error_groups(TestState* state, Dspic33* cpu) {
    dspic33_can_test_priority_and_abort_cases(state, cpu);
    dspic33_can_test_mode_and_power_cases(state, cpu);
    dspic33_can_test_mode_transition_cases(state, cpu);
    dspic33_can_test_physical_debug_mode_cases(state, cpu);
    dspic33_can_test_capture_timestamp_cases(state, cpu);
    dspic33_can_test_interrupt_and_error_cases(state, cpu);
    dspic33_can_test_invalid_message_cases(state, cpu);
    dspic33_can_test_copy_and_reset_cases(state, cpu);
}

void dspic33_can_test_priority_and_abort_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame output;
    uint8_t buffer;
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xb000u);
    for (buffer = 0u; buffer < 8u; buffer++) {
        dspic33_can_test_write_memory_word(cpu, (uint32_t)(0xb000u + buffer * 16u),
                                           (uint16_t)((0x100u + buffer) << 2u));
        dspic33_can_test_write_memory_word(cpu, (uint32_t)(0xb004u + buffer * 16u), 0u);
    }
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x8988u);
    dspic33_write_word(cpu, 0x0432u, 0x8a8bu);
    dspic33_write_word(cpu, 0x0434u, 0x8a89u);
    dspic33_write_word(cpu, 0x0436u, 0x8b8bu);
    expect(state, dspic33_device_advance(cpu, 32768u), "priority transmissions");
    expect(state, dspic33_can_transmit(cpu, 0u, &output) && output.identifier == 0x107u,
           "highest priority natural order");
    expect(state, dspic33_can_transmit(cpu, 0u, &output) && output.identifier == 0x106u,
           "second highest natural order");
    dspic33_reset(cpu, 0u);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_write_word(cpu, 0x0430u, 0x8989u);
    dspic33_write_word(cpu, 0x0400u, (uint16_t)(dspic33_read_word(cpu, 0x0400u) | 0x1000u));
    expect(state, (dspic33_read_word(cpu, 0x0430u) & 0x4848u) == 0x4040u,
           "abort flags and request clear");
    expect(state, (dspic33_read_word(cpu, 0x0400u) & 0x1000u) == 0u, "abort all self clear");
}

void dspic33_can_test_mode_and_power_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t modes[] = {0u, 1u, 2u, 3u, 4u, 7u};
    uint8_t index;
    for (index = 0u; index < sizeof(modes); index++) {
        uint8_t mode = modes[index];
        Dspic33CanFrame input = dspic33_can_test_frame(0x234u, false, false, 1u, mode);
        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, 0u, 0xc000u, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, 0u, mode == 7u ? 0u : 1u);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_set_mode(cpu, 0u, mode);
        expect(state, ((dspic33_read_word(cpu, 0x0400u) >> 5u) & 7u) == mode,
               "mode acknowledgement matrix");
        expect(state, dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
               "mode receive schedule");
        expect(state,
               dspic33_can_test_receive_full(cpu, 0u, 0u) ==
                   (mode == 0u || mode == 2u || mode == 3u || mode == 7u),
               "mode receive behavior");
    }
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, 0u, 0xc000u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
    dspic33_can_test_enable_filter(cpu, 0u, 1u);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0760u, (uint16_t)(dspic33_read_word(cpu, 0x0760u) | 2u));
    {
        Dspic33CanFrame input = dspic33_can_test_frame(0x234u, false, false, 1u, 0u);
        expect(state, dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
               "PMD receive schedule");
        expect(state, !dspic33_can_test_receive_full(cpu, 0u, 0u), "PMD blocks receive");
    }
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x040cu, 0x0040u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    {
        Dspic33CanFrame input = dspic33_can_test_frame(0x234u, false, false, 1u, 0u);
        expect(state, dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 1u),
               "unfiltered sleep activity schedule");
        expect(state, (dspic33_read_word(cpu, 0x040au) & 0x0040u) == 0u,
               "disabled CAN wake filter rejects sleep activity");
    }
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0412u, 0x4000u);
    dspic33_write_word(cpu, 0x040cu, 0x0040u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    {
        Dspic33CanFrame input = dspic33_can_test_frame(0x234u, false, false, 1u, 0u);
        expect(state, dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 1u),
               "filtered sleep wake schedule");
        expect(state, (dspic33_read_word(cpu, 0x040au) & 0x0040u) != 0u,
               "enabled CAN wake filter raises wake flag");
    }
}

void dspic33_can_test_mode_transition_cases(TestState* state, Dspic33* cpu) {
    uint64_t cycles;
    dspic33_reset(cpu, 0u);
    cycles = dspic33_can_test_mode_transition_cycles(cpu, 0u);
    dspic33_can_test_request_mode(cpu, 0u, 0u);
    expect(state, (dspic33_read_word(cpu, 0x0400u) & 0x07e0u) == 0x0080u,
           "CAN mode request preserves the active mode before bus idle");
    expect(state,
           dspic33_device_advance(cpu, cycles - 1u) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x00e0u) == 0x0080u,
           "CAN mode request remains pending before 11 recessive bits");
    expect(state,
           dspic33_device_advance(cpu, 1u) && (dspic33_read_word(cpu, 0x0400u) & 0x00e0u) == 0u,
           "CAN mode request completes after 11 recessive bits");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_can_test_request_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_device_advance(cpu, cycles - 1u) && dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) && dspic33_can_input_pin(cpu, 64u, true, 0u) &&
               dspic33_device_advance(cpu, 0u) && dspic33_device_advance(cpu, 1u) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x00e0u) == 0x0080u,
           "dominant CAN input restarts the mode idle boundary");
    expect(state,
           dspic33_device_advance(cpu, cycles - 1u) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x00e0u) == 0u,
           "CAN mode transition completes after the restarted idle boundary");

    dspic33_can_test_request_mode(cpu, 0u, 3u);
    dspic33_can_test_request_mode(cpu, 0u, 2u);
    expect(state,
           dspic33_device_advance(cpu, cycles) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x07e0u) == 0x0240u,
           "new CAN mode request supersedes a pending transition");

    Dspic33 copy;
    expect(state, dspic33_initialize(&copy), "initialize pending CAN mode copy");
    dspic33_can_test_request_mode(cpu, 0u, 3u);
    expect(state, dspic33_copy(&copy, cpu), "copy pending CAN mode transition");
    expect(state,
           dspic33_device_advance(cpu, cycles) && dspic33_device_advance(&copy, cycles) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x00e0u) == 0x0060u &&
               (dspic33_read_word(&copy, 0x0400u) & 0x00e0u) == 0x0060u,
           "copy preserves pending CAN mode transition phase");
    dspic33_release(&copy);

    dspic33_reset(cpu, 0u);
    dspic33_can_test_request_mode(cpu, 0u, 0u);
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_device_advance(cpu, cycles) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x07e0u) == 0x0480u && cpu->events.count == 0u,
           "reset cancels a pending CAN mode transition");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_can_test_request_mode(cpu, 0u, 0u);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u &&
               (dspic33_read_word(cpu, 0x0400u) & 0x07e0u) == 0x0080u,
           "CAN mode schedule overflow leaves the request pending");

    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xde00u);
    Dspic33CanFrame input = dspic33_can_test_frame(0x234u, false, false, 1u, 0x5au);
    dspic33_can_test_write_transmit_frame(cpu, 0xde00u, &input);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state, dspic33_device_advance(cpu, 8u) && (cpu->io.can_tx_on_bus & 1u) != 0u,
           "CAN mode transition test reaches an active frame");
    dspic33_can_test_request_mode(cpu, 0u, 4u);
    expect(state,
           dspic33_device_advance(cpu, cycles) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x00e0u) == 0u &&
               (cpu->io.can_tx_on_bus & 1u) != 0u,
           "CAN mode transition waits for the active frame");
    expect(state,
           dspic33_device_advance(cpu, 256u) &&
               (dspic33_read_word(cpu, 0x0400u) & 0x00e0u) == 0x0080u,
           "CAN mode transition completes after the active frame and idle boundary");
}

void dspic33_can_test_physical_debug_mode_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t mode = 3u; mode <= 7u; mode += 4u) {
        Dspic33CanFrame input = dspic33_can_test_frame(0x234u, false, false, 1u, 0x5au);
        bool acknowledge_observed = false;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, 0x4000u);
        dspic33_can_test_configure_transmit(cpu, 0u, 0xde00u);
        dspic33_can_test_configure_receive(cpu, 1u, 0xdc00u, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, 1u, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, 1u, 1u);
        dspic33_can_test_write_transmit_frame(cpu, 0xde00u, &input);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_select_window(cpu, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, 0u);
        dspic33_write_word(cpu, 0x0510u, 0u);
        dspic33_write_word(cpu, 0x0512u, 0u);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        dspic33_can_test_set_mode(cpu, 1u, mode);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        expect(
            state,
            dspic33_device_advance(cpu, 8u) &&
                dspic33_can_test_bridge_can_pins(cpu, 0u, 64u, 65u, 4u, -1, &acknowledge_observed),
            "CAN debug receive mode advances a valid physical frame");
        expect(state,
               dspic33_can_test_receive_full(cpu, 1u, 0u) &&
                   (dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 0u &&
                   (cpu->io.can_rx_error_active & 2u) == 0u,
               "CAN debug receive mode accepts a valid physical frame");
        expect(state, acknowledge_observed == (mode == 7u),
               "CAN listen-only mode suppresses physical acknowledgement");

        acknowledge_observed = false;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, 0x4000u);
        dspic33_can_test_configure_transmit(cpu, 0u, 0xde00u);
        dspic33_can_test_configure_receive(cpu, 1u, 0xdc00u, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, 1u, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, 1u, 1u);
        dspic33_can_test_write_transmit_frame(cpu, 0xde00u, &input);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_select_window(cpu, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, 0u);
        dspic33_write_word(cpu, 0x0510u, 0u);
        dspic33_write_word(cpu, 0x0512u, 0u);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        dspic33_can_test_set_mode(cpu, 1u, mode);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        expect(
            state,
            dspic33_device_advance(cpu, 8u) &&
                dspic33_can_test_bridge_can_pins(cpu, 0u, 64u, 65u, 4u, 30, &acknowledge_observed),
            "CAN debug receive mode advances an invalid physical frame");
        expect(state,
               (dspic33_read_word(cpu, 0x050au) & 0x0080u) != 0u &&
                   (dspic33_can_test_receive_full(cpu, 1u, 0u) == (mode == 7u)),
               "CAN listen-all mode transfers an invalid physical frame");
        expect(state,
               mode == 7u || ((dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 0u &&
                              (cpu->io.can_rx_error_active & 2u) == 0u && !acknowledge_observed),
               "CAN listen-only mode freezes counters and suppresses error output");
    }
}

void dspic33_can_test_capture_timestamp_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xc800u + channel * 0x100u);
        Dspic33CanFrame input = dspic33_can_test_frame(0x345u, false, false, 1u, 0x5au);
        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel, memory, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, channel, 0u, 0x345u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, channel, 1u);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x12u), 0u);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, 0x0148u, 0u);
        dspic33_write_word(cpu, 0x014au, 0u);
        dspic33_write_word(cpu, 0x0148u, 0x1c03u);
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) | 0x0008u));
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.input_capture.input_high & 2u) != 0u,
               "CAN timestamp pulse starts after frame acceptance");
        expect(state,
               dspic33_device_advance(cpu, 3u) && (cpu->io.input_capture.input_high & 2u) != 0u,
               "CAN timestamp pulse remains high before one bit time");
        expect(state,
               dspic33_device_advance(cpu, 1u) && cpu->io.input_capture.fifo[1].count == 1u &&
                   (cpu->io.input_capture.input_high & 2u) == 0u,
               "CAN timestamp pulse clears after one bit time");
        dspic33_write_word(cpu, 0x06aeu, 0x4000u);
        expect(state,
               dspic33_input_capture_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_input_capture_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 1u),
               "IC2 pin edge advances while CAN capture is selected");
        expect(state, cpu->io.input_capture.fifo[1].count == 1u,
               "CANCAP disconnects the physical IC2 pin");
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) & ~0x0008u));
        expect(state,
               dspic33_input_capture_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_input_capture_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 1u),
               "IC2 pin edge advances after CAN capture is cleared");
        expect(state, cpu->io.input_capture.fifo[1].count == 2u,
               "clearing CANCAP restores the physical IC2 pin");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel, memory, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, channel, 0u, 0x345u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, channel, 1u);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, 0x0148u, 0x1c03u);
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 32u),
               "disabled CAN timestamp receive schedule");
        expect(state, cpu->io.input_capture.fifo[1].count == 0u,
               "CANCAP clear preserves the IC2 pin source");
    }
}

static uint16_t expected_error_status(bool transmit, uint16_t count, bool bus_off) {
    uint16_t status = 0u;
    if (count >= 96u) {
        status |= CAN_ERROR_WARNING;
        if (transmit) {
            if (count < 128u) {
                status |= CAN_TRANSMIT_WARNING;
            } else if (!bus_off) {
                status |= CAN_TRANSMIT_PASSIVE;
            }
        } else {
            if (count < 128u) {
                status |= CAN_RECEIVE_WARNING;
            } else {
                status |= CAN_RECEIVE_PASSIVE;
            }
        }
    }
    if (bus_off) {
        status |= CAN_BUS_OFF;
    }
    return status;
}

static void configure_error_test(Dspic33* cpu, uint8_t channel) {
    dspic33_reset(cpu, 0u);
    dspic33_can_test_set_mode(cpu, channel, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 0x0cu), CAN_INTERRUPT_ERROR);
}

static void expect_error_step(TestState* state, Dspic33* cpu, uint8_t channel, bool transmit,
                              uint8_t increment, uint16_t expected_counts,
                              uint16_t expected_status) {
    uint16_t status_address = (uint16_t)(bases[channel] + 0x0au);
    uint16_t status;
    expect(state,
           dspic33_can_error(cpu, channel, transmit, increment, 0u) &&
               dspic33_device_advance(cpu, 1u),
           "error counter event schedule");
    status = dspic33_read_word(cpu, status_address);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == expected_counts,
           "error counter result");
    expect(state, (status & CAN_ERROR_STATUS_MASK) == expected_status, "error state result");
    expect(state, (status & CAN_INTERRUPT_ERROR) == 0u, "B1 error transition leaves ERRIF clear");
    expect(state, !dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
           "B1 error transition does not interrupt");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "B1 error transition keeps default vector");
}

static void clear_error_interrupt(TestState* state, Dspic33* cpu, uint8_t channel) {
    uint16_t address = (uint16_t)(bases[channel] + 0x0au);
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~CAN_INTERRUPT_ERROR));
    dspic33_can_test_clear_interrupt_flag(cpu, event_irqs[channel]);
    expect(state, (dspic33_read_word(cpu, address) & CAN_INTERRUPT_ERROR) == 0u,
           "error flag clear");
    expect(state, !dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
           "error interrupt clear");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "error vector clear");
}

static void error_threshold_domain(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t direction;
        for (direction = 0u; direction < 2u; direction++) {
            bool transmit = direction != 0u;
            uint16_t count;
            for (count = 1u; count <= UINT8_MAX; count++) {
                uint16_t counts = transmit ? (uint16_t)(count << 8u) : count;
                configure_error_test(cpu, channel);
                expect_error_step(state, cpu, channel, transmit, (uint8_t)count, counts,
                                  expected_error_status(transmit, count, false));
            }
        }
    }
}

static void receive_error_transition_cases(TestState* state, Dspic33* cpu, uint8_t channel) {
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, false, 95u, 0x005fu, 0u);
    expect_error_step(state, cpu, channel, false, 1u, 0x0060u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, false, 1u, 0x0061u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING);
    expect_error_step(state, cpu, channel, false, 31u, 0x0080u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, false, 1u, 0x0081u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    expect_error_step(state, cpu, channel, false, 126u, 0x00ffu,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    expect_error_step(state, cpu, channel, false, 1u, 0x00ffu,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    dspic33_can_test_set_mode(cpu, channel, 4u);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0u,
           "configuration clears receive error counter");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) & CAN_ERROR_STATUS_MASK) ==
               0u,
           "configuration clears receive error state");
    dspic33_can_test_set_mode(cpu, channel, 0u);
    expect_error_step(state, cpu, channel, false, 96u, 0x0060u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING);
}

static void transmit_error_transition_cases(TestState* state, Dspic33* cpu, uint8_t channel) {
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, true, 95u, 0x5f00u, 0u);
    expect_error_step(state, cpu, channel, true, 1u, 0x6000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0x6100u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING);
    expect_error_step(state, cpu, channel, true, 31u, 0x8000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0x8100u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    expect_error_step(state, cpu, channel, true, 126u, 0xff00u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    expect_error_step(state, cpu, channel, true, 1u, 0xff00u, CAN_ERROR_WARNING | CAN_BUS_OFF);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0xff00u, CAN_ERROR_WARNING | CAN_BUS_OFF);
    dspic33_can_test_set_mode(cpu, channel, 4u);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0u,
           "configuration clears transmit error counter");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) & CAN_ERROR_STATUS_MASK) ==
               0u,
           "configuration clears bus off state");
    dspic33_can_test_set_mode(cpu, channel, 0u);
    expect_error_step(state, cpu, channel, true, 96u, 0x6000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING);
}

static void complete_error_test_transmission(TestState* state, Dspic33* cpu, uint8_t channel) {
    uint32_t memory = (uint32_t)(0xe000u + channel * 0x100u);
    Dspic33CanFrame output;
    uint8_t word;
    dspic33_can_test_configure_transmit(cpu, channel, memory);
    for (word = 0u; word < 8u; word++) {
        dspic33_can_test_write_memory_word(cpu, memory + word * 2u, 0u);
    }
    dspic33_can_test_select_window(cpu, channel, false);
    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 0x30u), 0x008bu);
    expect(state, dspic33_device_advance(cpu, 4096u), "error recovery transmission advance");
    expect(state, dspic33_can_transmit(cpu, channel, &output),
           "error recovery transmission output");
}

static void transmit_error_descending_entry_cases(TestState* state, Dspic33* cpu, uint8_t channel) {
    uint16_t status_address = (uint16_t)(bases[channel] + 0x0au);
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, true, 128u, 0x8000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    clear_error_interrupt(state, cpu, channel);
    complete_error_test_transmission(state, cpu, channel);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0x7f00u,
           "successful transmission decrements error counter");
    expect(state,
           (dspic33_read_word(cpu, status_address) & CAN_ERROR_STATUS_MASK) ==
               (CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING),
           "successful transmission enters error warning");
    expect(state, (dspic33_read_word(cpu, status_address) & CAN_INTERRUPT_ERROR) == 0u,
           "B1 descending error transition leaves ERRIF clear");
    expect(state, !dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
           "B1 descending error transition does not interrupt");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "B1 descending error transition keeps default vector");
    clear_error_interrupt(state, cpu, channel);
    complete_error_test_transmission(state, cpu, channel);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0x7e00u,
           "within-warning transmission decrements error counter");
    expect(state,
           (dspic33_read_word(cpu, status_address) & CAN_ERROR_STATUS_MASK) ==
               (CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING),
           "within-warning transmission preserves error state");
    expect(state, (dspic33_read_word(cpu, status_address) & CAN_INTERRUPT_ERROR) == 0u,
           "within-warning transmission does not set error flag");
    expect(state, !dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
           "within-warning transmission does not raise error interrupt");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "within-warning transmission keeps default vector");
}

void dspic33_can_test_interrupt_and_error_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    error_threshold_domain(state, cpu);
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        receive_error_transition_cases(state, cpu, channel);
        transmit_error_transition_cases(state, cpu, channel);
        transmit_error_descending_entry_cases(state, cpu, channel);
    }
}

void dspic33_can_test_invalid_message_cases(TestState* state, Dspic33* cpu) {
    expect(state, !dspic33_can_invalid(cpu, DSPIC33_CAN_COUNT, 0u),
           "invalid CAN message rejects unavailable channel");
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x0080u);
        dspic33_can_test_clear_interrupt_flag(cpu, event_irqs[channel]);
        expect(state, dspic33_can_invalid(cpu, channel, 2u), "schedule invalid CAN message");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u &&
                   !dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
               "invalid CAN message waits for its event boundary");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) != 0u &&
                   dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & 0x007fu) == 0x40u,
               "invalid CAN message raises IVRIF and the event interrupt");
        dspic33_write_word(cpu, (uint16_t)(base + 0x0au),
                           (uint16_t)(dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & ~0x0080u));
        dspic33_can_test_clear_interrupt_flag(cpu, event_irqs[channel]);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u &&
                   !dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
               "software clears the invalid CAN message event");

        dspic33_reset(cpu, 0u);
        expect(state,
               dspic33_can_invalid(cpu, channel, 0u) && dspic33_device_advance(cpu, 0u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u,
               "configuration mode suppresses invalid CAN message events");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, 0x0760u,
                           (uint16_t)(dspic33_read_word(cpu, 0x0760u) | (uint16_t)(2u << channel)));
        expect(state,
               dspic33_can_invalid(cpu, channel, 0u) && dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u,
               "PMD-disabled CAN suppresses invalid message events");
    }
}

void dspic33_can_test_copy_and_reset_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    Dspic33CanFrame input = dspic33_can_test_frame(0x456u, false, false, 3u, 0x70u);
    expect(state, initialized, "initialize CAN copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, 0u, 0xd000u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x456u, false, 0x7ffu, true, 0u, 0u);
    dspic33_can_test_enable_filter(cpu, 0u, 1u);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state, dspic33_can_receive(cpu, 0u, &input, 2u), "copy pending receive schedule");
    expect(state, dspic33_copy(&copy, cpu), "copy pending CAN state");
    expect(state, dspic33_device_advance(cpu, 32u) && dspic33_device_advance(&copy, 32u),
           "copy advance");
    expect(state,
           dspic33_can_test_receive_full(cpu, 0u, 0u) &&
               dspic33_can_test_receive_full(&copy, 0u, 0u),
           "copy receives identically");
    expect(state,
           dspic33_can_test_memory_word(cpu, 0xd000u) ==
               dspic33_can_test_memory_word(&copy, 0xd000u),
           "copy DMA contents");
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, 14u);
    dspic33_can_test_configure_transmit(cpu, 0u, 0xd100u);
    dspic33_can_test_write_memory_word(cpu, 0xd100u, 2u);
    for (uint8_t word = 1u; word < 8u; word++) {
        dspic33_can_test_write_memory_word(cpu, 0xd100u + word * 2u, 0u);
    }
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state, dspic33_device_advance(cpu, 8u) && (cpu->io.can_tx_busy & 1u) != 0u,
           "copy reaches pending CAN bus completion");
    expect(state, dspic33_copy(&copy, cpu), "copy pending CAN bus state");
    expect(state, dspic33_device_advance(cpu, 20u) && dspic33_device_advance(&copy, 20u),
           "copied CAN bus phases advance");
    bool source_high;
    bool copy_high;
    expect(state,
           dspic33_can_pin(cpu, 64u, &source_high) && dspic33_can_pin(&copy, 64u, &copy_high) &&
               source_high && copy_high,
           "copy preserves CAN transmit bit phase");
    expect(state, dspic33_device_advance(cpu, 980u) && dspic33_device_advance(&copy, 980u),
           "copied CAN bus completions advance");
    Dspic33CanFrame source_output;
    Dspic33CanFrame copy_output;
    expect(state,
           dspic33_can_transmit(cpu, 0u, &source_output) &&
               dspic33_can_transmit(&copy, 0u, &copy_output) &&
               source_output.identifier == copy_output.identifier,
           "copy preserves pending CAN bus completion");
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 3u) &&
               cpu->io.can_rx_serial_count[0] == 1u && (cpu->io.can_rx_serial_active & 1u) != 0u,
           "copy reaches active CAN serial reception");
    expect(state, dspic33_copy(&copy, cpu), "copy active CAN serial state");
    expect(state,
           dspic33_can_input_pin(cpu, 64u, true, 1u) &&
               dspic33_can_input_pin(&copy, 64u, true, 1u) && dspic33_device_advance(cpu, 4u) &&
               dspic33_device_advance(&copy, 4u) && cpu->io.can_rx_serial_count[0] == 2u &&
               copy.io.can_rx_serial_count[0] == 2u &&
               cpu->io.can_rx_serial_bits[0][1] == copy.io.can_rx_serial_bits[0][1],
           "copy preserves CAN serial receive phase");
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_write_word(cpu, 0x0412u, 0x0040u);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_can_input_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 1u) &&
               cpu->io.can_rx_sample_high[0] == 1u && cpu->io.can_rx_serial_count[0] == 0u,
           "copy reaches the first CAN majority sample");
    expect(state, dspic33_copy(&copy, cpu) && copy.io.can_rx_sample_high[0] == 1u,
           "copy preserves partial CAN majority state");
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_can_input_pin(&copy, 64u, false, 0u) && dspic33_device_advance(cpu, 1u) &&
               dspic33_device_advance(&copy, 1u) &&
               cpu->io.can_rx_sample_high[0] == copy.io.can_rx_sample_high[0],
           "copied CAN majority samples advance together");
    dspic33_release(&copy);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_pin_high & 1u) == 0u,
           "CAN external pin holds a dominant level");
    dspic33_can_test_reset_can_raw(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    expect(state,
           (cpu->io.can_rx_pin_high & 1u) == 0u && (cpu->io.can_rx_physical_active & 1u) != 0u,
           "CAN power-on reset preserves the external pin level");
    dspic33_can_input_pin(cpu, 64u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_mclr_reset(cpu);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    expect(state,
           (cpu->io.can_rx_pin_high & 1u) != 0u && (cpu->io.can_rx_physical_active & 1u) != 0u,
           "CAN warm reset preserves the external pin level");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.can_rx[0].count == 0u && cpu->io.can_tx[0].count == 0u &&
               cpu->io.can_rx_busy == 0u && cpu->io.can_tx_busy == 0u &&
               cpu->io.can_rx_serial_active == 0u && cpu->io.can_rx_serial_count[0] == 0u &&
               cpu->io.can_rx_pin_high == 3u && cpu->io.can_rx_physical_active == 0u &&
               cpu->io.can_rx_ack == 0u && cpu->io.can_tx_retry_wait == 0u &&
               cpu->io.can_tx_error_active == 0u && cpu->io.can_rx_error_active == 0u &&
               cpu->io.can_intermission_active == 0u && cpu->io.can_overload_active == 0u &&
               cpu->io.can_bus_off_recessive_bits[0] == 0u &&
               cpu->io.can_bus_off_recessive_bits[1] == 0u && cpu->io.can_resync_count[0] == 0u &&
               cpu->io.can_resync_count[1] == 0u && cpu->io.can_intermission_generation[0] == 0u &&
               cpu->io.can_intermission_generation[1] == 0u &&
               cpu->io.can_tx_phase_adjustment[0] == 0 && cpu->io.can_tx_phase_adjustment[1] == 0 &&
               cpu->io.can_overload_count[0] == 0u && cpu->io.can_overload_count[1] == 0u &&
               cpu->io.can_rx_sample_high[0] == 0u && cpu->io.can_rx_sample_high[1] == 0u &&
               cpu->io.can_tx_sample_high[0] == 0u && cpu->io.can_tx_sample_high[1] == 0u,
           "reset clears CAN runtime");
}
