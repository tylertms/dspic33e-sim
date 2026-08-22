#include "device/dspic33ep_mu/communication/pmp/internal.h"

void dspic33_device_internal_pmp_clear_busy(Dspic33* cpu, uint16_t generation);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_update_pmp_register(Dspic33* cpu, uint16_t address, uint16_t previous);

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
    dspic33_reset(cpu, 0u);
    cpu->io.pmp.pmd_disabled = true;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->stop_reason == DSPIC33_RUNNING,
           "PMP power update accepts an unavailable master clock");

    dspic33_device_internal_pmp_clear_busy(cpu, 0u);
    expect(state, !cpu->io.pmp.completing_active, "inactive PMP ignores BUSY completion");

    resume_overflow_case(state, cpu);
    event_discard_case(state, cpu);

    dspic33_reset(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_internal_raw_write_word(cpu, PMP_CONTROL, PMP_STOP_IDLE);
    dspic33_device_internal_update_pmp_register(cpu, PMP_CONTROL, 0u);
    expect(state, cpu->power_state == DSPIC33_POWER_IDLE,
           "PMP stop-in-idle change refreshes the clock state");
}
