#include <string.h>

#include "allocation_failure.h"
#include "device/dspic33ep_mu/communication/i2c/internal.h"
#include "device/dspic33ep_mu/internal.h"
#include "../../../../../src/device/dspic33ep_mu/communication/i2c/internal.h"

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill I2C allocation failure event queue");
    }
}

static void allocation_failure_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    bool scheduled = dspic33_i2c_internal_schedule_event(cpu, 0u, 0u, 1u, false);
    test_reject_reallocation(false);
    expect(state, !scheduled, "I2C internal scheduling reports allocation failure");

    dspic33_reset(cpu, 0u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_CON),
                                           (uint16_t)(I2C_ENABLE | I2C_SEN));
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_i2c_internal_begin_control(cpu, 0u, I2C_SEN);
    test_reject_reallocation(false);
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[0] + I2C_CON)) & I2C_SEN) == 0u,
           "I2C control scheduling failure clears the operation");
}
#endif

void dspic33_i2c_test_boundary_cases(TestState* state, Dspic33* cpu) {
    Dspic33I2cPinMapping mapping;
    Dspic33I2cTransfer transfer;
    Dspic33I2cResponse response = {1u, 0u, false};
    Dspic33I2cResponseQueue responses;
    memset(&responses, 0, sizeof(responses));
    responses.count = DSPIC33_I2C_QUEUE_SIZE;
    expect(state, !dspic33_i2c_internal_pin_mapping(cpu, DSPIC33_I2C_COUNT, &mapping) &&
                      !dspic33_i2c_internal_pin_mapping(cpu, 0u, NULL),
           "I2C invalid pin mappings are rejected");
    expect(state, !dspic33_i2c_internal_transfer_pop(&cpu->io.i2c_tx[0], &transfer),
           "I2C empty transfer queue cannot pop");
    expect(state, !dspic33_i2c_internal_response_push(&responses, &response),
           "I2C full response queue cannot grow");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_I2C, 0u, 0u, 5u),
           "I2C pause fixture schedules an event");
    dspic33_i2c_internal_pause_events(cpu, 0u);
    cpu->device_cycles = 1u;
    cpu->events.items[0].paused_remaining = UINT64_MAX;
    dspic33_i2c_internal_resume_events(cpu, 0u);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.items[0].paused,
           "I2C paused event overflow is retained and reported");

    dspic33_reset(cpu, 0u);
    cpu->io.i2c_pmd_disabled = 1u;
    expect(state, dspic33_i2c_internal_schedule_event(cpu, 0u, 0u, 7u, false) &&
                      cpu->events.items[0].paused && cpu->events.items[0].paused_remaining == 7u,
           "I2C disabled module pauses internal events");

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    allocation_failure_cases(state, cpu);
#endif
}
