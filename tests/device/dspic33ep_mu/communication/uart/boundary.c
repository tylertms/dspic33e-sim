#include "allocation_failure.h"
#include "device/dspic33ep_mu/internal.h"
#include "test.h"

void dspic33_uart_test_state_matrix_cases(TestState* state, Dspic33* cpu);

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill UART allocation failure event queue");
    }
}

static void allocation_failure_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.uart_tx_active = 1u;
    cpu->io.uart_tx_shift[0].data_bits = 8u;
    cpu->io.uart_tx_shift[0].stop_bits = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_uart_schedule_transmit(cpu, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->io.uart_tx_scheduled == 0u,
           "UART reports initial transmit scheduling failure");

    dspic33_reset(cpu, 0u);
    cpu->io.uart_tx_active = 1u;
    cpu->io.uart_tx_shift[0].data_bits = 8u;
    cpu->io.uart_tx_shift[0].stop_bits = 1u;
    fill_event_queue(state, cpu);
    cpu->events.count--;
    test_reject_reallocation(true);
    dspic33_device_internal_uart_schedule_transmit(cpu, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->io.uart_tx_active == 0u,
           "UART pin scheduling failure cancels transmission");

    dspic33_reset(cpu, 0u);
    cpu->io.uart_tx_active = 1u;
    cpu->io.uart_tx_shift[0].data_bits = 8u;
    cpu->io.uart_tx_shift[0].stop_bits = 1u;
    cpu->io.uart_tx_shift[0].irda = true;
    fill_event_queue(state, cpu);
    cpu->events.count -= 10u;
    test_reject_reallocation(true);
    dspic33_device_internal_uart_schedule_transmit(cpu, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->io.uart_tx_active == 0u,
           "UART IrDA scheduling failure cancels transmission");

    dspic33_reset(cpu, 0u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_uart_begin_physical_receive(cpu, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->io.uart_rx_active == 0u,
           "UART receive scheduling failure cancels reception");
}
#endif

static void parity_output_case(TestState* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, 0x0680u, 1u);
    dspic33_device_internal_raw_write_word(cpu, 0x0220u, UART_MODE_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, 0x0222u, UART_STATUS_TX_ENABLE);
    cpu->io.uart_tx_scheduled = 1u;
    cpu->io.uart_tx_clocks[0] = 16u;
    cpu->io.uart_tx_shift[0].data_bits = 8u;
    cpu->io.uart_tx_shift[0].stop_bits = 1u;
    cpu->io.uart_tx_shift[0].parity = DSPIC33_UART_PARITY_EVEN;
    cpu->io.uart_tx_shift[0].value = 1u;
    cpu->device_cycles = 9u * 16u;
    expect(state, dspic33_device_internal_uart_pps_output_value(cpu, 3u, 0u, &high),
           "UART PPS reports a parity output bit");
    cpu->device_cycles = 10u * 16u;
    expect(state, dspic33_device_internal_uart_pps_output_value(cpu, 3u, 0u, &high) && high,
           "UART PPS reports a stop bit after parity");
}

static void receive_sampling_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, 0x06c4u, 64u);
    dspic33_device_internal_raw_write_word(cpu, 0x0e3eu, 0u);
    expect(state, dspic33_gpio_drive(cpu, 3u, 1u, 1u), "drive UART invalid start level");
    cpu->io.uart_rx_active = 1u;
    cpu->io.uart_rx_samples[0] = 1u;
    cpu->io.uart_rx_generation[0] = 1u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_PIN | 1u);
    expect(state, cpu->io.uart_rx_active == 0u, "UART rejects a high physical start bit");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, 0x06c4u, 64u);
    dspic33_device_internal_raw_write_word(cpu, 0x0e3eu, 0u);
    expect(state, dspic33_gpio_drive(cpu, 3u, 0u, 1u), "drive UART invalid stop level");
    cpu->io.uart_rx_active = 1u;
    cpu->io.uart_rx_samples[0] = 1u;
    cpu->io.uart_rx_generation[0] = 1u;
    cpu->io.uart_rx_shift[0].data_bits = 8u;
    cpu->io.uart_rx_shift[0].stop_bits = 1u;
    dspic33_device_internal_run_uart(
        cpu, 0u, UART_EVENT_PIN | (UINT32_C(27) << UART_EVENT_PIN_PHASE_SHIFT) | 1u);
    expect(state, cpu->io.uart_rx_shift[0].framing_error,
           "UART marks a low physical stop bit as a framing error");
}

static void transmit_completion_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, 0x0222u, UART_STATUS_TX_INTERRUPT_LOW);
    cpu->io.uart_tx_active = 1u;
    cpu->io.uart_tx_scheduled = 1u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_TRANSMIT);
    expect(state, cpu->io.uart_tx_active == 0u, "UART empty completion raises its interrupt");
}

static void active_power_stop_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.uart_rx_active = 1u;
    cpu->io.uart_tx_active = 1u;
    cpu->io.uart_tx_scheduled = 1u;
    cpu->io.uart_tx_shift[0].value = 0x5au;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_internal_uart_update_power_state(cpu);
    expect(state,
           cpu->io.uart_rx_active == 0u && cpu->io.uart_tx_active == 0u &&
               cpu->io.uart_tx_scheduled == 0u && cpu->io.uart_tx_shift[0].value == 0u,
           "sleep cancels active physical UART reception and transmission");
}

void dspic33_uart_test_boundary_cases(TestState* state, Dspic33* cpu) {
    bool high;
    dspic33_uart_test_state_matrix_cases(state, cpu);
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_uart_auto_baud_edge(cpu, 0u, true, true);
    expect(state, cpu->io.uart_auto_baud_active == 0u, "UART auto-baud ignores a stable input");
    expect(state, !dspic33_device_internal_uart_pps_output_value(cpu, 0u, 0u, NULL),
           "UART PPS rejects a null output");
    expect(state, !dspic33_device_internal_uart_pps_output_value(cpu, UINT8_MAX, UINT8_MAX, &high),
           "UART PPS rejects an invalid pin");
    dspic33_device_internal_run_uart(cpu, DSPIC33_UART_COUNT, 0u);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING, "UART ignores an invalid event channel");
    parity_output_case(state, cpu);
    receive_sampling_cases(state, cpu);
    transmit_completion_case(state, cpu);
    active_power_stop_case(state, cpu);
#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    allocation_failure_cases(state, cpu);
#endif
}
