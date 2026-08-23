#include "device/dspic33ep_mu/communication/spi/internal.h"
#include "device/dspic33ep_mu/internal.h"

static void configure(Dspic33* cpu, uint16_t status, uint16_t control, uint16_t frame) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, bases[0], status);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + 2u), control);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + 4u), frame);
}

static void power_matrix(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t control;
        uint16_t status;
        Dspic33PowerState power;
        bool enabled;
    } cases[] = {
        {0u, SPI_STOP_IDLE, DSPIC33_POWER_IDLE, false},
        {0u, 0u, DSPIC33_POWER_IDLE, true},
        {0u, 0u, DSPIC33_POWER_SLEEP, true},
        {SPI_MASTER, 0u, DSPIC33_POWER_ACTIVE, true},
        {SPI_MASTER, 0u, DSPIC33_POWER_IDLE, true},
        {SPI_MASTER, SPI_STOP_IDLE, DSPIC33_POWER_IDLE, false},
        {SPI_MASTER, 0u, DSPIC33_POWER_SLEEP, false},
    };
    for (size_t case_index = 0u; case_index < sizeof(cases) / sizeof(cases[0]); case_index++) {
        configure(cpu, cases[case_index].status, cases[case_index].control, 0u);
        cpu->power_state = cases[case_index].power;
        expect(state,
               dspic33_device_internal_spi_power_enabled(cpu, 0u) == cases[case_index].enabled,
               "SPI power admission matrix");
    }
}

static void predicate_matrix(TestState* state, Dspic33* cpu) {
    configure(cpu, SPI_ENABLE, 0u, SPI_FRAME_ENABLE);
    expect(state, dspic33_device_internal_spi_slave_frame_master(cpu, 0u),
           "slave SPI can provide the frame signal");
    expect(state, !dspic33_device_internal_spi_master_frame_slave(cpu, 0u),
           "slave SPI is not a framed master");

    configure(cpu, SPI_ENABLE, SPI_MASTER, (uint16_t)(SPI_FRAME_ENABLE | SPI_FRAME_SLAVE));
    expect(state, !dspic33_device_internal_spi_slave_frame_master(cpu, 0u),
           "master SPI is not a framed slave");
    expect(state, dspic33_device_internal_spi_master_frame_slave(cpu, 0u),
           "master SPI accepts a slave frame signal");

    configure(cpu, SPI_ENABLE, SPI_MASTER, SPI_ENHANCED);
    dspic33_device_internal_spi_raise_mode(cpu, 0u, 7u);
    expect(state, !dspic33_spi_test_interrupt_flag(cpu, irqs[0]),
           "enhanced SPI ignores a different interrupt mode");

    dspic33_device_internal_raw_write_word(cpu, 0x06c8u, 1u);
    dspic33_device_internal_spi_refresh_pps_inputs(cpu);
    expect(state, (cpu->io.spi_pin_input_enabled & 1u) != 0u,
           "SPI PPS input enables physical sampling");
}

static void event_filter_matrix(TestState* state, Dspic33* cpu) {
    configure(cpu, SPI_ENABLE, SPI_SLAVE_SELECT, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, SPI_EVENT_INTERNAL, 1u),
           "schedule internal SPI event");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, SPI_EVENT_EXTERNAL, 2u),
           "schedule external SPI event");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 1u, SPI_EVENT_INTERNAL, 3u),
           "schedule other-channel SPI event");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 4u),
           "schedule non-SPI event");
    dspic33_device_internal_spi_remove_internal_events(cpu, 0u);
    expect(state, cpu->events.count == 3u, "SPI internal event filtering preserves other work");
    cpu->io.spi_busy = 1u;
    cpu->io.spi_shift[0] = 0x5a5au;
    dspic33_device_internal_spi_update_slave_selection(cpu, 0u, true, false);
    expect(state, cpu->events.count == 2u, "SPI deselection removes only external events");
}

static void scheduling_admission_matrix(TestState* state, Dspic33* cpu) {
    static const uint16_t controls[] = {0u, SPI_DISABLE_CLOCK, SPI_MASTER, SPI_MASTER};
    static const Dspic33PowerState powers[] = {DSPIC33_POWER_ACTIVE, DSPIC33_POWER_ACTIVE,
                                               DSPIC33_POWER_SLEEP, DSPIC33_POWER_ACTIVE};
    static const bool busy[] = {true, true, true, false};
    for (size_t case_index = 0u; case_index < sizeof(controls) / sizeof(controls[0]);
         case_index++) {
        configure(cpu, SPI_ENABLE, controls[case_index], 0u);
        cpu->power_state = powers[case_index];
        cpu->io.spi_busy = busy[case_index] ? 1u : 0u;
        dspic33_device_internal_spi_schedule_current(cpu, 0u);
        expect(state, cpu->events.count == 0u, "SPI scheduling admission rejects inactive state");
    }
}

static void run_admission_matrix(TestState* state, Dspic33* cpu) {
    uint32_t generation_bits;
    configure(cpu, SPI_ENABLE, SPI_MASTER, (uint16_t)(SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH));
    generation_bits = (uint32_t)cpu->io.spi_generation[0] << SPI_EVENT_GENERATION_SHIFT;
    dspic33_device_internal_run_spi(cpu, 0u, SPI_EVENT_FRAME | generation_bits);
    cpu->io.spi_busy = 1u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_internal_run_spi(cpu, 0u, SPI_EVENT_FRAME | generation_bits);
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + 4u), 0u);
    dspic33_device_internal_run_spi(cpu, 0u, SPI_EVENT_FRAME | generation_bits);

    dspic33_device_internal_run_spi(
        cpu, 0u, SPI_EVENT_SAMPLE | (generation_bits + (1u << SPI_EVENT_GENERATION_SHIFT)));
    cpu->io.spi_busy = 0u;
    dspic33_device_internal_run_spi(cpu, 0u, SPI_EVENT_SAMPLE | generation_bits);
    cpu->io.spi_busy = 1u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_internal_run_spi(cpu, 0u, SPI_EVENT_SAMPLE | generation_bits);
    expect(state, cpu->io.spi_pin_bits[0] == 0u, "SPI event admission rejects invalid samples");
}

void dspic33_spi_test_state_matrix_cases(TestState* state, Dspic33* cpu) {
    power_matrix(state, cpu);
    predicate_matrix(state, cpu);
    event_filter_matrix(state, cpu);
    scheduling_admission_matrix(state, cpu);
    run_admission_matrix(state, cpu);
}
