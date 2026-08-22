#include "allocation_failure.h"
#include "device/dspic33ep_mu/communication/pmp/internal.h"

void dspic33_device_internal_pmp_clear_busy(Dspic33* cpu, uint16_t generation);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_update_pmp_register(Dspic33* cpu, uint16_t address, uint16_t previous);
void dspic33_device_internal_run_pmp(Dspic33* cpu, uint16_t generation);

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill PMP event queue");
    }
}

static void dma_allocation_failure_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_dma(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, PMP_DATA, 1u);
    dspic33_pmp_test_configure_pmp(cpu, (uint16_t)(PMP_INTERRUPT_EACH | PMP_MASTER_MODE_2), 0u);
    dspic33_write_word(cpu, PMP_DATA, 0x1234u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_pmp(cpu, cpu->io.pmp.generation);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "PMP DMA request rejects a full event queue");
}
#endif

static void resume_overflow_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_PMP, 0u, 0u, 1u),
           "schedule PMP resume overflow case");
    cpu->events.items[0].paused = true;
    cpu->events.items[0].paused_remaining = UINT64_MAX;
    cpu->device_cycles = 1u;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "PMP rejects a resumed event cycle overflow");
}

static void event_discard_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u),
           "schedule non-PMP abort survivor");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_PMP, 0u, 0u, 1u),
           "schedule PMP abort cancellation");
    dspic33_device_internal_raw_write_word(cpu, PMP_CONTROL, 0u);
    dspic33_device_internal_update_pmp_register(cpu, PMP_CONTROL, PMP_ENABLE);
    expect(state, cpu->events.count == 1u && cpu->events.items[0].type == DSPIC33_EVENT_INTERRUPT,
           "PMP abort preserves unrelated events");
}

void dspic33_pmp_test_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_pmp_test_state_matrix_cases(state, cpu);
    dspic33_reset(cpu, 0u);
    cpu->io.pmp.pmd_disabled = true;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING,
           "PMP power update accepts an unavailable master clock");

    dspic33_device_internal_pmp_clear_busy(cpu, 0u);
    expect(state, !cpu->io.pmp.completing_active, "inactive PMP ignores BUSY completion");

    resume_overflow_case(state, cpu);
    event_discard_case(state, cpu);

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    dma_allocation_failure_case(state, cpu);
#endif

    dspic33_reset(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_internal_raw_write_word(cpu, PMP_CONTROL, PMP_STOP_IDLE);
    dspic33_device_internal_update_pmp_register(cpu, PMP_CONTROL, 0u);
    expect(state, cpu->power_state == DSPIC33_POWER_IDLE,
           "PMP stop-in-idle change refreshes the clock state");
}
