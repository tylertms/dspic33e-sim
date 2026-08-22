#include "allocation_failure.h"
#include "device/dspic33ep_mu/communication/dci/internal.h"

enum {
    DCI_TEST_EVENT_START,
    DCI_TEST_EVENT_INTERNAL,
    DCI_TEST_EVENT_EXTERNAL,
    DCI_TEST_EVENT_EXTERNAL_FRAME,
    DCI_TEST_EVENT_SAMPLE,
    DCI_TEST_EVENT_FRAME_START
};

void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_run_dci(Dspic33* cpu, uint16_t source, uint32_t value);
bool dspic33_device_internal_dci_data_output(const Dspic33* cpu, bool* high);
bool dspic33_device_internal_dci_frame_output(const Dspic33* cpu, bool* high);
bool dspic33_device_internal_dci_internal_clock_high(const Dspic33* cpu, bool* high);
bool dspic33_device_internal_dci_read_register(Dspic33* cpu, uint16_t address, uint8_t* value);
void dspic33_device_internal_dci_discard_internal_events(Dspic33* cpu);

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill DCI allocation failure event queue");
    }
}
#endif

static void stale_event_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.dci.generation = 7u;
    cpu->io.dci.started = true;
    cpu->io.dci.initialized = true;
    cpu->io.dci.internal_scheduled = true;
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);

    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_START, 6u);
    expect(state, cpu->io.dci.internal_scheduled, "stale start event ignored");
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_INTERNAL, 6u);
    expect(state, cpu->io.dci.internal_scheduled, "stale internal event ignored");
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_SAMPLE, 6u);
    expect(state, cpu->io.dci.serial_bits == 0u, "stale sample event ignored");
    cpu->io.dci.pps_frame_pending = true;
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_FRAME_START, 6u);
    expect(state, cpu->io.dci.pps_frame_pending, "stale frame event ignored");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, 0u);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_START, 7u);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_INTERNAL, 7u);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL, 0u);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_FRAME_START, 7u);
    expect(state, !cpu->io.dci.internal_scheduled, "disabled internal events stop");
}

static void scheduling_failure_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 1u;
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;
    cpu->device_cycles = UINT64_MAX;
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_SAMPLE, 1u);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "DCI sample scheduling overflow stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 1u;
    cpu->device_cycles = UINT64_MAX;
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_START, 1u);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "DCI word scheduling overflow stops execution");

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_dma(cpu, 0u, 0x0001u, 0x4100u, DCI_RECEIVE_BASE, 0u,
                                   DCI_DMA_REQUEST);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL, 0x1357u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "DCI DMA allocation failure stops execution");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_dma(cpu, 0u, 0x0001u, 0x4200u, DCI_RECEIVE_BASE, 0u,
                                   DCI_DMA_REQUEST);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 0u);
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;
    cpu->io.dci.disable_pending = true;
    cpu->io.dci.receive_buffered = 1u;
    cpu->io.dci.receive[0] = 0x2468u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL, 0x2468u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "DCI deferred disable propagates DMA allocation failure");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_dma(cpu, 0u, 0x2001u, 0x4300u, DCI_TRANSMIT_BASE, 0u,
                                   DCI_DMA_REQUEST);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 1u, 0u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL, 0u);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "DCI transmit startup propagates DMA allocation failure");
#endif
}

static void external_event_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL, 0x1234u);
    expect(state, !cpu->io.dci.initialized, "internal-clock DCI ignores external word event");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL, 0x5678u);
    expect(state, cpu->io.dci.initialized && cpu->io.dci.started,
           "external-clock DCI starts without external frame");

    cpu->io.dci.initialized = true;
    cpu->io.dci.started = false;
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL, 0x9abcu);
    expect(state, cpu->io.dci.started, "initialized external-clock DCI resumes without frame");

    cpu->io.dci.slot = 0u;
    cpu->io.dci.serial_bits = 2u;
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2,
                                           dspic33_dci_test_configuration(4u, 4u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_EXTERNAL_CLOCK);
    expect(state, cpu->io.dci.disable_pending, "external multi-slot DCI begins deferred disable");
}

static void event_queue_boundary_cases(TestState* state, Dspic33* cpu) {
    bool high = true;
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
           "schedule unrelated event");
    dspic33_device_internal_dci_discard_internal_events(cpu);
    expect(state, cpu->events.count == 1u, "DCI discard preserves unrelated events");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 2u;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_TEST_EVENT_INTERNAL, 2u, 1000u),
           "schedule malformed DCI phase event");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &high) && !high,
           "DCI frame output rejects impossible event phase");

    cpu->events.count = 0u;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
           "schedule unrelated event before DCI disable");
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, !cpu->io.dci.initialized && !cpu->io.dci.started,
           "DCI disable aborts when no frame event remains");
}

static void output_boundary_cases(TestState* state, Dspic33* cpu) {
    bool high = true;
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_TRISTATE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state, !dspic33_device_internal_dci_data_output(cpu, &high),
           "inactive tristated DCI has no data output");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &high) && !high,
           "inactive internal DCI frame is low");
    expect(state, dspic33_device_internal_dci_internal_clock_high(cpu, &high),
           "configured internal DCI owns its clock");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_EXTERNAL_CLOCK);
    expect(state, !dspic33_device_internal_dci_internal_clock_high(cpu, &high),
           "external DCI does not own its clock");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &high) && !high,
           "inactive external DCI frame is low");
    cpu->io.dci.pmd_disabled = true;
    dspic33_write_word(cpu, DCI_PMD, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, !dspic33_device_internal_dci_internal_clock_high(cpu, &high),
           "PMD-disabled DCI does not own its clock");
}

static void register_boundary_cases(TestState* state, Dspic33* cpu) {
    uint8_t value = UINT8_MAX;
    dspic33_reset(cpu, 0u);
    cpu->io.dci.pmd_disabled = true;
    dspic33_write_word(cpu, DCI_CONTROL2, UINT16_MAX);
    expect(state, dspic33_read_word(cpu, DCI_CONTROL2) == 0u,
           "PMD-disabled DCI rejects register writes");
    expect(state,
           dspic33_device_internal_dci_read_register(cpu, DCI_CONTROL1, &value) && value == 0u,
           "PMD-disabled DCI reads zero");
    expect(state, !dspic33_device_internal_dci_read_register(cpu, DCI_CONTROL1 - 2u, &value),
           "DCI read rejects address below block");
    expect(state, !dspic33_device_internal_dci_read_register(cpu, DCI_TRANSMIT_BASE + 8u, &value),
           "DCI read rejects address above block");
    expect(state, !dspic33_device_internal_dci_read_register(cpu, 0x028au, &value),
           "DCI read rejects reserved transmit slot address");
    expect(state, !dspic33_device_internal_dci_read_register(cpu, 0x028eu, &value),
           "DCI read rejects reserved receive slot address");
}

void dspic33_dci_test_boundary_cases(TestState* state, Dspic33* cpu) {
    stale_event_cases(state, cpu);
    scheduling_failure_cases(state, cpu);
    external_event_cases(state, cpu);
    event_queue_boundary_cases(state, cpu);
    output_boundary_cases(state, cpu);
    register_boundary_cases(state, cpu);
}
