#include "allocation_failure.h"
#include "device/dspic33ep_mu/internal.h"
#include "test.h"

static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill output compare event queue");
    }
}

static void configure_channel(Dspic33* cpu, uint16_t mode, uint16_t synchronization) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(
        cpu, OUTPUT_COMPARE_BASE, (uint16_t)(OUTPUT_COMPARE_TIMER_SOURCE_FP | mode));
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(OUTPUT_COMPARE_BASE + 2u),
                                           synchronization);
}

static void reject_event(TestState* state, Dspic33* cpu, uint16_t mode, uint16_t synchronization,
                         uint32_t kind, const char* name) {
    configure_channel(cpu, mode, synchronization);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_output_compare(cpu, 0u, kind);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR, name);
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0};
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        dspic33_device_internal_run_output_compare(&cpu, DSPIC33_OUTPUT_COMPARE_COUNT, 0u);
        reject_event(&state, &cpu, OUTPUT_COMPARE_MODE_SINGLE_TOGGLE, OUTPUT_COMPARE_SYNC_SELF,
                     OUTPUT_COMPARE_EVENT_PRIMARY, "primary event rejects a full queue");
        reject_event(&state, &cpu, OUTPUT_COMPARE_MODE_DUAL_CONTINUOUS, OUTPUT_COMPARE_SYNC_SELF,
                     OUTPUT_COMPARE_EVENT_SECONDARY, "secondary event rejects a full queue");
        reject_event(&state, &cpu, OUTPUT_COMPARE_MODE_SINGLE_TOGGLE, OUTPUT_COMPARE_SYNC_NONE,
                     OUTPUT_COMPARE_EVENT_BOUNDARY, "boundary event rejects a full queue");
        reject_event(&state, &cpu, OUTPUT_COMPARE_MODE_SINGLE_TOGGLE, OUTPUT_COMPARE_SYNC_NONE,
                     OUTPUT_COMPARE_EVENT_SYNC_BOUNDARY,
                     "synchronized boundary rejects a full queue");
        reject_event(&state, &cpu, OUTPUT_COMPARE_MODE_SINGLE_TOGGLE, OUTPUT_COMPARE_SYNC_SELF,
                     OUTPUT_COMPARE_EVENT_SYNC_PRIMARY,
                     "synchronized primary rejects a full queue");
        reject_event(&state, &cpu, OUTPUT_COMPARE_MODE_SINGLE_TOGGLE, OUTPUT_COMPARE_SYNC_NONE,
                     OUTPUT_COMPARE_EVENT_EXTERNAL_SYNC,
                     "external synchronization rejects a full queue");
        dspic33_release(&cpu);
    }
    test_reject_reallocation(false);
    return test_finish(&state);
}
