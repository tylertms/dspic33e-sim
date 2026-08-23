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
bool dspic33_device_internal_dci_configuration_supported(const Dspic33* cpu);
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
    bool is_high = true;

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
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && !is_high,
           "DCI frame output rejects impossible event phase");

    cpu->events.count = 0u;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_TEST_EVENT_SAMPLE, 2u, 0u),
           "schedule non-phase DCI event");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && !is_high,
           "DCI frame output ignores a non-phase event");

    cpu->events.count = 0u;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_TEST_EVENT_INTERNAL, 1u, 0u),
           "schedule stale DCI phase event");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && !is_high,
           "DCI frame output ignores a stale phase event");

    cpu->events.items[0].value = 2u;
    cpu->events.items[0].paused = true;
    cpu->events.items[0].paused_remaining = 0u;
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high),
           "DCI frame output accepts a paused current phase event");

    cpu->events.count = 0u;
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_TEST_EVENT_SAMPLE, 2u, 0u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_TEST_EVENT_INTERNAL, 1u, 0u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_TEST_EVENT_INTERNAL, 2u, 8u),
           "schedule mixed DCI disable events");
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, cpu->io.dci.disable_pending,
           "DCI disable finds its current internal phase among unrelated events");

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
    bool is_high = true;

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_TRISTATE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state, !dspic33_device_internal_dci_data_output(cpu, &is_high),
           "inactive tristated DCI has no data output");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && !is_high,
           "inactive internal DCI frame is low");
    expect(state, dspic33_device_internal_dci_internal_clock_high(cpu, &is_high),
           "configured internal DCI owns its clock");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_EXTERNAL_CLOCK);
    expect(state, !dspic33_device_internal_dci_internal_clock_high(cpu, &is_high),
           "external DCI does not own its clock");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && !is_high,
           "inactive external DCI frame is low");
    cpu->io.dci.pmd_disabled = true;
    dspic33_write_word(cpu, DCI_PMD, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, !dspic33_device_internal_dci_internal_clock_high(cpu, &is_high),
           "PMD-disabled DCI does not own its clock");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 12u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 0u);
    expect(state, !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &is_high),
           "unclocked DCI does not drive its mapped clock pin");
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_EXTERNAL_CLOCK);
    expect(state, !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &is_high),
           "disabled external DCI does not drive its mapped clock pin");
    cpu->io.dci.disable_pending = true;
    expect(state, !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &is_high),
           "deferred DCI disable does not assign an external clock output");
}

static void register_boundary_cases(TestState* state, Dspic33* cpu) {
    uint8_t register_value = UINT8_MAX;

    dspic33_reset(cpu, 0u);
    cpu->io.dci.pmd_disabled = true;
    dspic33_write_word(cpu, DCI_CONTROL2, UINT16_MAX);
    expect(state, dspic33_read_word(cpu, DCI_CONTROL2) == 0u,
           "PMD-disabled DCI rejects register writes");
    expect(state,
           dspic33_device_internal_dci_read_register(cpu, DCI_CONTROL1, &register_value) &&
               register_value == 0u,
           "PMD-disabled DCI reads zero");
    expect(state,
           !dspic33_device_internal_dci_read_register(cpu, DCI_CONTROL1 - 2u, &register_value),
           "DCI read rejects address below block");
    expect(state,
           !dspic33_device_internal_dci_read_register(cpu, DCI_TRANSMIT_BASE + 8u, &register_value),
           "DCI read rejects address above block");
    expect(state, !dspic33_device_internal_dci_read_register(cpu, 0x028au, &register_value),
           "DCI read rejects reserved transmit slot address");
    expect(state, !dspic33_device_internal_dci_read_register(cpu, 0x028eu, &register_value),
           "DCI read rejects reserved receive slot address");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, UINT16_MAX);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state, !dspic33_device_internal_dci_configuration_supported(cpu),
           "DCI rejects unsupported control bits");
}

static void direct_event_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 4u;
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_START, 4u);
    expect(state, cpu->io.dci.initialized && cpu->io.dci.internal_scheduled,
           "DCI start event initializes and schedules an internal word");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_FRAME);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2, 3u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.initialized = true;
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_EXTERNAL_FRAME, 0x1234u);
    expect(state, cpu->io.dci.started && cpu->io.dci.internal_scheduled,
           "external frame starts an internally clocked word");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2,
                                           dspic33_dci_test_configuration(16u, 1u, 1u));
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 2u;
    cpu->io.dci.initialized = true;
    cpu->io.dci.internal_scheduled = true;
    cpu->io.dci.pps_input_configured = true;
    dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_INTERNAL, 2u);
    expect(state, cpu->io.dci.internal_scheduled,
           "DCI reuses an existing internal event while adding its input sample");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_MODE_AC_LINK_16);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2,
                                           dspic33_dci_test_configuration(20u, 1u, 1u));
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 3u;
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;
    cpu->io.dci.slot = 1u;
    cpu->io.dci.input = UINT16_MAX;
    for (uint8_t bit_index = 0u; bit_index < 20u; bit_index++) {
        dspic33_device_internal_run_dci(cpu, DCI_TEST_EVENT_SAMPLE, 3u);
    }
    expect(state, cpu->io.dci.serial_bits == 20u && cpu->io.dci.serial_input == UINT16_MAX,
           "20-bit DCI sampling retains all available input bits");
}

static void frame_output_cases(TestState* state, Dspic33* cpu) {
    bool is_high = false;

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1,
                                           DCI_ENABLE | DCI_EXTERNAL_CLOCK | DCI_DATA_JUSTIFY);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 0u);
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2,
                                           dspic33_dci_test_configuration(16u, 1u, 0u));
    dspic33_device_internal_raw_write_word(
        cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK | DCI_DATA_JUSTIFY | DCI_MODE_I2S);
    cpu->io.dci.output_frame_high = true;
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && is_high,
           "external I2S frame reports its stored level");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2,
                                           dspic33_dci_test_configuration(16u, 1u, 2u));
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1,
                                           DCI_ENABLE | DCI_EXTERNAL_CLOCK | DCI_DATA_JUSTIFY |
                                               DCI_MODE_AC_LINK_16);
    cpu->io.dci.serial_frame_bits = 15u;
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && is_high,
           "external AC-link frame remains high for its first word");
    cpu->io.dci.serial_frame_bits = 16u;
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && !is_high,
           "external AC-link frame falls after its first word");

    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2,
                                           dspic33_dci_test_configuration(16u, 1u, 1u));
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1,
                                           DCI_ENABLE | DCI_EXTERNAL_CLOCK | DCI_DATA_JUSTIFY);
    cpu->io.dci.slot = 0u;
    cpu->io.dci.serial_bits = 0u;
    cpu->io.dci.serial_delay = false;
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && is_high,
           "immediate external frame is high at the first slot boundary");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_MODE_I2S);
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL2,
                                           dspic33_dci_test_configuration(16u, 2u, 1u));
    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
    cpu->io.dci.generation = 1u;
    cpu->io.dci.initialized = true;
    cpu->io.dci.started = true;
    cpu->io.dci.slot = 1u;
    cpu->io.dci.output_frame_high = true;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_DCI, DCI_TEST_EVENT_INTERNAL, 1u, 0u),
           "schedule final I2S slot phase");
    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high) && !is_high,
           "final internal I2S slot previews the next frame polarity");
}

static void internal_output_phase_cases(TestState* state, Dspic33* cpu) {
    uint32_t high_output_count = 0u;
    uint32_t driven_output_count = 0u;
    for (uint8_t mode = 0u; mode < 4u; mode++) {
        for (uint8_t data_justified = 0u; data_justified < 2u; data_justified++) {
            for (uint8_t event_source = DCI_TEST_EVENT_START;
                 event_source <= DCI_TEST_EVENT_INTERNAL; event_source++) {
                for (uint8_t output_phase = 0u; output_phase < 4u; output_phase++) {
                    bool is_high = false;
                    dspic33_reset(cpu, 0u);
                    dspic33_device_internal_raw_write_word(
                        cpu, DCI_CONTROL1,
                        (uint16_t)(DCI_ENABLE | mode |
                                   (data_justified != 0u ? DCI_DATA_JUSTIFY : 0u) |
                                   (output_phase & 1u ? DCI_SAMPLE_RISING : 0u)));
                    dspic33_device_internal_raw_write_word(
                        cpu, DCI_CONTROL2, dspic33_dci_test_configuration(16u, 2u, mode));
                    dspic33_device_internal_raw_write_word(cpu, DCI_CONTROL3, 1u);
                    cpu->io.dci.generation = 3u;
                    cpu->io.dci.initialized = true;
                    cpu->io.dci.started = true;
                    cpu->io.dci.slot = output_phase >= 2u ? 1u : 0u;
                    cpu->io.dci.buffer = 0u;
                    cpu->io.dci.transmit[0] = 0xa55au;
                    cpu->io.dci.output_frame_high = (output_phase & 1u) != 0u;
                    dspic33_device_internal_raw_write_word(cpu, DCI_TRANSMIT_SLOTS, 3u);
                    const uint64_t event_delay = event_source == DCI_TEST_EVENT_START
                                                     ? (uint64_t)output_phase * 4u
                                                     : (uint64_t)output_phase * 16u;
                    expect(state,
                           dspic33_schedule(cpu, DSPIC33_EVENT_DCI, event_source, 3u, event_delay),
                           "schedule internal DCI output phase");
                    expect(state, dspic33_device_internal_dci_frame_output(cpu, &is_high),
                           "internal DCI frame output is available");
                    high_output_count += is_high;
                    driven_output_count += dspic33_device_internal_dci_data_output(cpu, &is_high);
                    high_output_count += is_high;
                }
            }
        }
    }
    expect(state, high_output_count == 62u && driven_output_count == 64u,
           "internal DCI output phase census matches");
}

void dspic33_dci_test_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_dci_test_state_matrix_cases(state, cpu);
    stale_event_cases(state, cpu);
    scheduling_failure_cases(state, cpu);
    external_event_cases(state, cpu);
    event_queue_boundary_cases(state, cpu);
    output_boundary_cases(state, cpu);
    register_boundary_cases(state, cpu);
    direct_event_cases(state, cpu);
    frame_output_cases(state, cpu);
    internal_output_phase_cases(state, cpu);
}
