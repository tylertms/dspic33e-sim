#include "allocation_failure.h"
#include "device/dspic33ep_mu/communication/spi/internal.h"
#include "device/dspic33ep_mu/internal.h"

static void configure_spi(Dspic33* cpu, uint16_t spi_control, uint16_t frame_control) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, bases[0], SPI_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + 2u), spi_control);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + 4u), frame_control);
}

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill SPI allocation failure event queue");
    }
}

static void allocation_failure_cases(TestState* state, Dspic33* cpu) {
    configure_spi(cpu, SPI_MASTER, 0u);
    cpu->io.spi_busy = 1u;
    cpu->io.spi_shift[0] = 0x1234u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_spi_schedule_current(cpu, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->io.spi_busy == 0u,
           "SPI transfer scheduling failure clears the transfer");

    configure_spi(cpu, SPI_MASTER, (uint16_t)(SPI_FRAME_ENABLE | SPI_FRAME_SLAVE));
    cpu->io.spi_selected = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_spi(cpu, 0u, SPI_EVENT_FRAME_INPUT);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "SPI frame input scheduling failure stops execution");

    configure_spi(cpu, SPI_MASTER, (uint16_t)(SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH));
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_spi(cpu, 0u, SPI_EVENT_FRAME_START);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->io.spi_busy == 0u,
           "SPI frame scheduling failure clears the transfer");

    configure_spi(cpu, SPI_MASTER, (uint16_t)(SPI_FRAME_ENABLE | SPI_FRAME_SLAVE));
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_spi_schedule_frame_input_sample(cpu, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "SPI input sample scheduling failure stops execution");
}
#endif

void dspic33_spi_test_boundary_cases(TestState* state, Dspic33* cpu) {
    bool output_level;

    dspic33_spi_test_state_matrix_cases(state, cpu);
    configure_spi(cpu, SPI_MASTER,
                  (uint16_t)(SPI_FRAME_ENABLE | SPI_FRAME_ACTIVE_HIGH | SPI_FRAME_DELAY));
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 8u), 0x55aau);
    expect(state, cpu->events.count != 0u, "SPI delayed frame schedules an assertion");

    cpu->io.spi_busy = 1u;
    dspic33_device_internal_run_spi(
        cpu, 0u,
        SPI_EVENT_FRAME | ((uint32_t)cpu->io.spi_generation[0] << SPI_EVENT_GENERATION_SHIFT));
    expect(state, (cpu->io.spi_frame_active & 1u) != 0u, "SPI frame event asserts output");

    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_internal_run_spi(
        cpu, 0u,
        SPI_EVENT_INTERNAL | ((uint32_t)cpu->io.spi_generation[0] << SPI_EVENT_GENERATION_SHIFT));
    expect(state, cpu->io.spi_busy == 0u, "SPI sleeping transfer is discarded");

    configure_spi(cpu, SPI_SLAVE_SELECT, 0u);
    cpu->io.spi_busy = 1u;
    cpu->io.spi_shift[0] = 0x4321u;
    cpu->io.spi_tx_fifo[0].count = 8u;
    dspic33_device_internal_spi_update_slave_selection(cpu, 0u, true, false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->io.spi_busy == 0u,
           "SPI deselection overflow clears the transfer");

    configure_spi(cpu, SPI_MASTER, 0u);
    dspic33_device_internal_run_spi(cpu, DSPIC33_SPI_COUNT, SPI_EVENT_INTERNAL);
    dspic33_device_internal_run_spi_select(cpu, DSPIC33_SPI_COUNT, true);
    dspic33_device_internal_run_spi(cpu, 0u,
                                    SPI_EVENT_FRAME_START | (1u << SPI_EVENT_GENERATION_SHIFT));
    expect(state, cpu->stop_reason == DSPIC33_RUNNING, "SPI stale and invalid events are ignored");

    configure_spi(cpu, 0u, 0u);
    cpu->io.spi_pin_output_index[0] = UINT8_MAX;
    cpu->io.spi_shift[0] = 1u;
    expect(state, dspic33_spi_data_output(cpu, 0u, &output_level) && output_level,
           "slave SPI output clamps a completed bit index");

    configure_spi(cpu, SPI_MASTER, 0u);
    cpu->io.spi_busy = 1u;
    cpu->io.spi_shift[0] = 1u;
    cpu->device_cycles = 1000u;
    expect(state, dspic33_spi_data_output(cpu, 0u, &output_level),
           "master SPI output clamps an elapsed transfer index");

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    allocation_failure_cases(state, cpu);
#endif
}
