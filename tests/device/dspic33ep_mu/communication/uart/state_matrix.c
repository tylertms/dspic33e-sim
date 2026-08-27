#include "device/dspic33ep_mu/internal.h"
#include "test.h"

static void configure(Dspic33* cpu, uint16_t mode, Dspic33PowerState power) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, 0x0220u, mode);
    cpu->power_state = power;
}

static void receiver_admission_matrix(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t mode;
        uint16_t pmd;
        Dspic33PowerState power;
        bool operating;
    } cases[] = {
        {UART_MODE_ENABLE, 0x0020u, DSPIC33_POWER_ACTIVE, false},
        {0u, 0u, DSPIC33_POWER_ACTIVE, false},
        {(uint16_t)(UART_MODE_ENABLE | UART_MODE_LOOPBACK), 0u, DSPIC33_POWER_ACTIVE, false},
        {(uint16_t)(UART_MODE_ENABLE | UART_MODE_IREN), 0u, DSPIC33_POWER_ACTIVE, true},
        {UART_MODE_ENABLE, 0u, DSPIC33_POWER_SLEEP, false},
        {(uint16_t)(UART_MODE_ENABLE | UART_MODE_STOP_IDLE), 0u, DSPIC33_POWER_IDLE, false},
        {UART_MODE_ENABLE, 0u, DSPIC33_POWER_IDLE, true},
        {UART_MODE_ENABLE, 0u, DSPIC33_POWER_ACTIVE, true},
    };
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        configure(cpu, cases[index].mode, cases[index].power);
        dspic33_device_internal_raw_write_word(cpu, 0x0760u, cases[index].pmd);
        cpu->io.platform_pmd_disabled = cases[index].pmd != 0u ? 1u : 0u;
        expect(state,
               dspic33_device_internal_uart_receiver_operating(cpu, 0u) == cases[index].operating,
               "UART receiver admission matrix");
    }
}

static void auto_baud_matrix(TestState* state, Dspic33* cpu) {
    configure(cpu, UART_MODE_AUTO_BAUD, DSPIC33_POWER_ACTIVE);
    dspic33_device_internal_uart_auto_baud_edge(cpu, 0u, false, true);
    expect(state, cpu->io.uart_auto_baud_active == 0u,
           "UART auto-baud ignores a rising edge before activation");
    dspic33_device_internal_uart_auto_baud_edge(cpu, 0u, true, false);
    expect(state, cpu->io.uart_auto_baud_active == 1u, "UART auto-baud starts on a falling edge");
    for (uint8_t edge = 0u; edge < 5u; edge++) {
        cpu->device_cycles = edge + 1u;
        dspic33_device_internal_uart_auto_baud_edge(cpu, 0u, false, true);
    }
    expect(state,
           dspic33_device_internal_raw_word(cpu, 0x0228u) == 0u &&
               cpu->io.uart_auto_baud_active == 0u,
           "UART auto-baud clamps a sub-clock measurement");
}

static void receive_event_admission_matrix(TestState* state, Dspic33* cpu) {
    configure(cpu, UART_MODE_ENABLE, DSPIC33_POWER_ACTIVE);
    cpu->io.uart_rx_generation[0] = 2u;
    cpu->io.uart_rx_samples[0] = 1u;
    cpu->io.uart_rx_active = 1u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_PIN | 1u);
    cpu->io.uart_rx_samples[0] = 0u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_PIN | 2u);
    cpu->io.uart_rx_samples[0] = 1u;
    cpu->io.uart_rx_active = 0u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_PIN | 2u);
    cpu->io.uart_rx_active = 1u;
    dspic33_device_internal_raw_write_word(cpu, 0x06c4u, 0x007fu);
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_PIN | 2u);
    expect(state, cpu->io.uart_rx_votes[0] == 0u, "UART rejects invalid physical receive events");
}

static void transmit_event_admission_matrix(TestState* state, Dspic33* cpu) {
    configure(cpu, UART_MODE_ENABLE, DSPIC33_POWER_ACTIVE);
    cpu->io.uart_generation[0] = 2u;
    cpu->io.uart_tx_active = 1u;
    cpu->io.uart_tx_scheduled = 1u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_TRANSMIT | 1u);
    cpu->io.uart_tx_active = 0u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_TRANSMIT | 2u);
    cpu->io.uart_tx_active = 1u;
    cpu->io.uart_tx_scheduled = 0u;
    dspic33_device_internal_run_uart(cpu, 0u, UART_EVENT_TRANSMIT | 2u);
    expect(state, cpu->io.uart_tx_active == 1u, "UART rejects invalid transmit completions");
}

static void output_power_matrix(TestState* state, Dspic33* cpu) {
    bool high;
    static const struct {
        uint16_t mode;
        Dspic33PowerState power;
        bool scheduled;
    } cases[] = {
        {UART_MODE_ENABLE, DSPIC33_POWER_ACTIVE, false},
        {UART_MODE_ENABLE, DSPIC33_POWER_SLEEP, true},
        {(uint16_t)(UART_MODE_ENABLE | UART_MODE_STOP_IDLE), DSPIC33_POWER_IDLE, true},
    };
    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        configure(cpu, cases[index].mode, cases[index].power);
        dspic33_device_internal_raw_write_word(cpu, 0x0680u, 1u);
        dspic33_device_internal_raw_write_word(cpu, 0x0222u, UART_STATUS_TX_ENABLE);
        cpu->io.uart_tx_scheduled = cases[index].scheduled ? 1u : 0u;
        expect(state, dspic33_device_internal_uart_pps_output_value(cpu, 3u, 0u, &high) && high,
               "UART output power matrix returns the idle level");
    }
}

void dspic33_uart_test_state_matrix_cases(TestState* state, Dspic33* cpu) {
    receiver_admission_matrix(state, cpu);
    auto_baud_matrix(state, cpu);
    receive_event_admission_matrix(state, cpu);
    transmit_event_admission_matrix(state, cpu);
    output_power_matrix(state, cpu);
}
