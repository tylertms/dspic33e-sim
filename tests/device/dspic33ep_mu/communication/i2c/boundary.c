#include <string.h>

#include "../../../../../src/device/dspic33ep_mu/communication/i2c/internal.h"
#include "allocation_failure.h"
#include "device/dspic33ep_mu/communication/i2c/internal.h"
#include "device/dspic33ep_mu/internal.h"

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
    bool event_scheduled = dspic33_i2c_internal_schedule_event(cpu, 0u, 0u, 1u, false);
    test_reject_reallocation(false);
    expect(state, !event_scheduled, "I2C internal scheduling reports allocation failure");

    const uint16_t operations[] = {I2C_SEN, I2C_RSEN, I2C_PEN, I2C_RCEN, I2C_ACKEN};
    for (uint8_t operation_index = 0u; operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
        expect(state, dspic33_gpio_drive(cpu, 3u, 0x0600u, 0x0600u),
               "drive I2C pins for scheduling failure");
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(bases[0] + I2C_CON),
            (uint16_t)(I2C_ENABLE | operations[operation_index]));
        fill_event_queue(state, cpu);
        test_reject_reallocation(true);
        dspic33_i2c_internal_begin_control(cpu, 0u, operations[operation_index]);
        test_reject_reallocation(false);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(bases[0] + I2C_CON)) &
                operations[operation_index]) == 0u,
               "I2C control scheduling failure clears the operation");
    }

    for (uint8_t operation_index = 0u; operation_index < 3u; operation_index++) {
        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
        cpu->configuration[12u] &= (uint8_t)~0x10u;
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(bases[0] + I2C_CON),
            (uint16_t)(I2C_ENABLE | operations[operation_index]));
        fill_event_queue(state, cpu);
        cpu->events.count--;
        test_reject_reallocation(true);
        dspic33_i2c_internal_begin_control(cpu, 0u, operations[operation_index]);
        test_reject_reallocation(false);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(bases[0] + I2C_CON)) &
                operations[operation_index]) == 0u,
               "I2C bus-status scheduling failure clears the operation");
    }

    for (uint8_t operation_index = 0u; operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(bases[0] + I2C_CON),
            (uint16_t)(I2C_ENABLE | operations[operation_index]));
        cpu->io.gpio[3] |= 0x0600u;
        cpu->io.gpio_driven[3] |= 0x0600u;
        cpu->configuration[12u] &= (uint8_t)~0x10u;
        fill_event_queue(state, cpu);
        cpu->events.count -= operation_index < 3u ? 2u : 1u;
        test_reject_reallocation(true);
        dspic33_i2c_internal_begin_control(cpu, 0u, operations[operation_index]);
        test_reject_reallocation(false);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(bases[0] + I2C_CON)) &
                operations[operation_index]) == 0u,
               "I2C pin scheduling failure clears the operation");
    }
}
#endif

void dspic33_i2c_test_boundary_cases(TestState* state, Dspic33* cpu) {
    Dspic33I2cPinMapping mapping;
    Dspic33I2cTransfer transfer;
    Dspic33I2cResponse response = {1u, 0u, false};
    Dspic33I2cResponseQueue responses;
    memset(&responses, 0, sizeof(responses));
    dspic33_i2c_test_state_matrix_cases(state, cpu);
    dspic33_reset(cpu, 0u);
    cpu->device_cycles = 1u;
    expect(state,
           !dspic33_i2c_respond(cpu, DSPIC33_I2C_COUNT, 0u, false, 0u) &&
               !dspic33_i2c_respond(cpu, 0u, 0u, false, UINT64_MAX) &&
               !dspic33_i2c_status(cpu, DSPIC33_I2C_COUNT, 0u) &&
               !dspic33_i2c_status(cpu, 0u, 1u) &&
               !dspic33_i2c_slave_start(cpu, DSPIC33_I2C_COUNT, 0u, false, false, 0u) &&
               !dspic33_i2c_slave_start(cpu, 0u, 0x80u, false, false, 0u) &&
               !dspic33_i2c_slave_start(cpu, 0u, 0u, false, false, UINT64_MAX) &&
               !dspic33_i2c_slave_write(cpu, DSPIC33_I2C_COUNT, 0u, 0u) &&
               !dspic33_i2c_slave_write(cpu, 0u, 0u, UINT64_MAX) &&
               !dspic33_i2c_slave_read(cpu, DSPIC33_I2C_COUNT, false, 0u) &&
               !dspic33_i2c_slave_read(cpu, 0u, false, UINT64_MAX) &&
               !dspic33_i2c_slave_stop(cpu, DSPIC33_I2C_COUNT, 0u) &&
               !dspic33_i2c_slave_stop(cpu, 0u, UINT64_MAX) &&
               !dspic33_i2c_collision(cpu, DSPIC33_I2C_COUNT, 0u) &&
               !dspic33_i2c_collision(cpu, 0u, UINT64_MAX) &&
               !dspic33_i2c_transmit(cpu, DSPIC33_I2C_COUNT, &transfer) &&
               !dspic33_i2c_transmit(cpu, 0u, NULL),
           "I2C public APIs reject invalid boundaries");
    cpu->device_cycles = 0u;
    responses.count = DSPIC33_I2C_QUEUE_SIZE;
    expect(state,
           !dspic33_i2c_internal_pin_mapping(cpu, DSPIC33_I2C_COUNT, &mapping) &&
               !dspic33_i2c_internal_pin_mapping(cpu, 0u, NULL),
           "I2C invalid pin mappings are rejected");
    expect(state, !dspic33_i2c_internal_transfer_pop(&cpu->io.i2c_tx[0], &transfer),
           "I2C empty transfer queue cannot pop");
    expect(state, !dspic33_i2c_internal_response_push(&responses, &response),
           "I2C full response queue cannot grow");

    memset(&responses, 0, sizeof(responses));
    response.cycle = 1u;
    expect(state, dspic33_i2c_internal_response_push(&responses, &response),
           "I2C response queue accepts its first response");
    response.cycle = 2u;
    expect(state, dspic33_i2c_internal_response_push(&responses, &response),
           "I2C response queue preserves ordered responses");

    dspic33_reset(cpu, 0u);
    dspic33_i2c_internal_complete_control(cpu, 0u, I2C_RCEN);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(bases[0] + I2C_CON),
                                           (uint16_t)(I2C_ENABLE | I2C_RCEN));
    memset(&cpu->io.i2c_response[0], 0, sizeof(cpu->io.i2c_response[0]));
    response.cycle = cpu->device_cycles + 5u;
    expect(state, dspic33_i2c_internal_response_push(&cpu->io.i2c_response[0], &response),
           "I2C delayed receive response is queued");
    dspic33_i2c_internal_complete_control(cpu, 0u, I2C_RCEN);
    expect(state, cpu->events.count != 0u, "I2C receive waits for its scheduled response");

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
    expect(state,
           dspic33_i2c_internal_schedule_event(cpu, 0u, 0u, 7u, false) &&
               cpu->events.items[0].paused && cpu->events.items[0].paused_remaining == 7u,
           "I2C disabled module pauses internal events");

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    allocation_failure_cases(state, cpu);
#endif
}
