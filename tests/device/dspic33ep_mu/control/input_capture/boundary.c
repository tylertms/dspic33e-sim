#include "allocation_failure.h"
#include "device/dspic33ep_mu/control/input_capture/internal.h"

bool dspic33_device_internal_input_capture_pair_configured(const Dspic33* cpu, uint8_t channel);
bool dspic33_device_internal_pps_physical_input_enabled(const Dspic33* cpu, uint8_t pin);
void dspic33_device_internal_run_input_capture(Dspic33* cpu, uint16_t source, uint32_t value);
void dspic33_device_internal_input_capture_advance_clock(Dspic33* cpu, uint16_t timer_source,
                                                         uint64_t cycles);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);

enum {
    CAPTURE_EVENT_SNAPSHOT = 1u,
    CAPTURE_EVENT_INTERRUPT = 2u,
    CAPTURE_EVENT_PMD = 4u,
    CAPTURE_EVENT_PAIRED_SNAPSHOT = 0x11u
};

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill input capture event queue");
    }
}

static void configure_dma(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, capture_irqs[0]);
    dspic33_write_word(cpu, 0x0b04u, CAPTURE_DMA_DESTINATION);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, (uint16_t)(CAPTURE_BASE + 4u));
    dspic33_write_word(cpu, 0x0b0eu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0x8001u);
}

static void allocation_failure_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 0u, false);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_input_capture(
        cpu, 0u, CAPTURE_EVENT_SNAPSHOT | ((uint32_t)cpu->io.input_capture.generation[0] << 8u));
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "input capture interrupt rejects a full queue");

    dspic33_reset(cpu, 0u);
    configure_dma(cpu);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 0u, false);
    cpu->io.input_capture.interrupt_count[0] = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_input_capture(
        cpu, 0u, CAPTURE_EVENT_SNAPSHOT | ((uint32_t)cpu->io.input_capture.generation[0] << 8u));
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "input capture DMA request rejects a full queue");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 0u, true);
    dspic33_device_internal_run_input_capture(
        cpu, 0u,
        CAPTURE_EVENT_PAIRED_SNAPSHOT | ((uint32_t)cpu->io.input_capture.generation[0] << 8u));
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "input capture rejects an incomplete paired snapshot");

    dspic33_input_capture_test_configure_capture_source(cpu, 1u, 0x1c00u, false, true, 0u, true);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_input_capture(
        cpu, 0u,
        CAPTURE_EVENT_PAIRED_SNAPSHOT | ((uint32_t)cpu->io.input_capture.generation[0] << 8u));
    test_reject_reallocation(false);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               cpu->io.input_capture.fifo[1].count == 0u,
           "paired input capture stops after a failed low snapshot");
}
#endif

static void resume_overflow_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 0u, false);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INPUT_CAPTURE, 0u, 1u, 5u),
           "schedule paused input capture event");
    cpu->events.items[0].paused = true;
    cpu->events.items[0].paused_remaining = UINT64_MAX;
    cpu->device_cycles = 1u;
    dspic33_device_internal_run_input_capture(cpu, 0u, 4u);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.items[0].paused,
           "input capture resume overflow remains paused");
}

static void paired_reset_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 0u, true);
    dspic33_input_capture_test_configure_capture_source(cpu, 1u, 0x1c00u, false, true, 0u, true);
    cpu->io.input_capture.timer[0] = 7u;
    cpu->io.input_capture.timer[1] = 9u;
    cpu->io.input_capture.sync_reset_pending = 3u;
    dspic33_device_internal_input_capture_advance_clock(cpu, 0x1c00u, 1u);
    expect(state,
           cpu->io.input_capture.sync_reset_pending == 0u && cpu->io.input_capture.timer[0] == 0u &&
               cpu->io.input_capture.timer[1] == 0u,
           "paired input capture reset consumes one clock");
}

static void event_admission_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 0u, true);
    dspic33_input_capture_test_configure_capture_source(cpu, 1u, 0x1c00u, false, true, 0u, true);
    cpu->io.input_capture.pmd_disabled = 2u;
    dspic33_device_internal_run_input_capture(
        cpu, 0u,
        CAPTURE_EVENT_PAIRED_SNAPSHOT | ((uint32_t)cpu->io.input_capture.generation[0] << 8u));
    dspic33_device_internal_run_input_capture(
        cpu, 1u,
        CAPTURE_EVENT_PAIRED_SNAPSHOT | ((uint32_t)cpu->io.input_capture.generation[1] << 8u));
    dspic33_input_capture_test_configure_capture_source(cpu, 15u, 0x1c00u, false, true, 0u, true);
    dspic33_device_internal_run_input_capture(
        cpu, 15u,
        CAPTURE_EVENT_PAIRED_SNAPSHOT | ((uint32_t)cpu->io.input_capture.generation[15] << 8u));
    expect(state,
           dspic33_device_internal_input_capture_pair_configured(cpu, 0u) &&
               !dspic33_device_internal_input_capture_pair_configured(cpu, 15u) &&
               cpu->io.input_capture.fifo[0].count == 0u &&
               cpu->io.input_capture.fifo[1].count == 0u &&
               cpu->io.input_capture.fifo[15].count == 0u,
           "input capture paired snapshots reject disabled, odd, and terminal members");

    dspic33_reset(cpu, 0u);
    dspic33_input_capture_test_configure_capture_source(cpu, 0u, 0x1c00u, false, true, 0u, false);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INPUT_CAPTURE, 0u, CAPTURE_EVENT_SNAPSHOT, 1u),
           "schedule active input capture event");
    cpu->io.input_capture.pmd_disabled = 1u;
    dspic33_device_internal_run_input_capture(cpu, 0u, CAPTURE_EVENT_PMD);
    expect(state, !cpu->events.items[0].paused,
           "input capture resume preserves an already active event");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, CAPTURE_BASE, 0x1c07u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_internal_run_input_capture(cpu, 0u, CAPTURE_EVENT_INTERRUPT);
    dspic33_device_internal_run_input_capture(cpu, 0u, CAPTURE_EVENT_INTERRUPT | 0x0100u);
    expect(state, (dspic33_read_word(cpu, 0x0800u) & 0x0002u) != 0u,
           "input capture interrupt mode accepts only its current generation");
}

void dspic33_input_capture_test_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           !dspic33_device_internal_input_capture_pair_configured(cpu, DSPIC33_INPUT_CAPTURE_COUNT),
           "input capture rejects an invalid pair");
    expect(state, !dspic33_device_internal_pps_physical_input_enabled(cpu, UINT8_MAX),
           "input capture rejects an invalid physical pin");
    dspic33_device_internal_run_input_capture(cpu, DSPIC33_INPUT_CAPTURE_COUNT, 1u);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING,
           "input capture ignores an invalid event source");
    resume_overflow_case(state, cpu);
    paired_reset_case(state, cpu);
    event_admission_cases(state, cpu);
#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    allocation_failure_cases(state, cpu);
#endif
}
