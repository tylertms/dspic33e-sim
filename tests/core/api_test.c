#include <stdint.h>

#include "dspic33.h"
#include "test.h"

static void test_lifecycle(TestState* state) {
    Dspic33* source = dspic33_create();
    Dspic33* destination = dspic33_create();
    expect(state, source != NULL, "source != NULL");
    expect(state, destination != NULL, "destination != NULL");
    expect(state, !dspic33_copy(NULL, source), "!dspic33_copy(NULL, source)");
    expect(state, !dspic33_copy(destination, NULL), "!dspic33_copy(destination, NULL)");
    expect(state, dspic33_copy(destination, source),
           "dspic33_copy(destination, source)");
    dspic33_destroy(destination);
    dspic33_destroy(source);
    dspic33_destroy(NULL);
}

static void test_execution(TestState* state) {
    Dspic33* cpu = dspic33_create();
    expect(state, cpu != NULL, "cpu != NULL");
    expect(state, dspic33_load_program_word(cpu, 0u, 0u),
           "dspic33_load_program_word(cpu, 0u, 0u)");
    dspic33_reset(cpu, 0u);

    Dspic33Result step = dspic33_step_result(cpu);
    expect(state, step.stop == DSPIC33_RUNNING, "step.stop == DSPIC33_RUNNING");
    expect(state, step.instructions == 1u, "step.instructions == 1u");
    expect(state, step.pc == 2u, "step.pc == 2u");
    expect(state, dspic33_get_instruction_count(cpu) == step.instructions,
           "dspic33_get_instruction_count(cpu) == step.instructions");
    expect(state, dspic33_get_cycle_count(cpu) == step.cycles,
           "dspic33_get_cycle_count(cpu) == step.cycles");
    expect(state, dspic33_get_program_counter(cpu) == step.pc,
           "dspic33_get_program_counter(cpu) == step.pc");

    Dspic33Result run = dspic33_run_with_limits(cpu, (Dspic33RunLimits){1u, 0u});
    expect(state, run.stop == DSPIC33_INSTRUCTION_LIMIT,
           "run.stop == DSPIC33_INSTRUCTION_LIMIT");
    expect(state, run.instructions == 2u, "run.instructions == 2u");
    dspic33_destroy(cpu);

    expect(state, dspic33_step_result(NULL).stop == DSPIC33_HALTED,
           "dspic33_step_result(NULL).stop == DSPIC33_HALTED");
    expect(state,
           dspic33_run_with_limits(NULL, (Dspic33RunLimits){0u, 0u}).stop ==
               DSPIC33_HALTED,
           "dspic33_run_with_limits(NULL, limits).stop == DSPIC33_HALTED");
}

static void test_host_operations(TestState* state) {
    Dspic33* cpu = dspic33_create();
    expect(state, cpu != NULL, "cpu != NULL");
    const uint8_t bytes[] = {0x34u, 0x12u};
    expect(state, dspic33_seed_data(cpu, 0x100u, bytes, sizeof(bytes)),
           "dspic33_seed_data(cpu, 0x100u, bytes, sizeof(bytes))");
    expect(state, dspic33_read_word(cpu, 0x100u) == 0x1234u,
           "dspic33_read_word(cpu, 0x100u) == 0x1234u");
    expect(state, !dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, bytes, sizeof(bytes)),
           "!dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, bytes, sizeof(bytes))");
    expect(state, dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, NULL, 0u),
           "dspic33_seed_data(cpu, DSPIC33_DATA_SIZE, NULL, 0u)");
    expect(state, dspic33_begin_call(cpu, 0u, false),
           "dspic33_begin_call(cpu, 0u, false)");
    expect(state, !dspic33_begin_call(cpu, 1u, false),
           "!dspic33_begin_call(cpu, 1u, false)");
    expect(state, !dspic33_begin_call(cpu, DSPIC33_PROGRAM_LIMIT, false),
           "!dspic33_begin_call(cpu, DSPIC33_PROGRAM_LIMIT, false)");
    bool high = false;
    expect(state, dspic33_gpio_drive(cpu, 1u, 1u, 1u),
           "dspic33_gpio_drive(cpu, 1u, 1u, 1u)");
    expect(state, dspic33_gpio_signal(cpu, 1u, 0u, &high) && high,
           "dspic33_gpio_signal(cpu, 1u, 0u, &high) && high");
    expect(state, !dspic33_gpio_signal(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, &high),
           "!dspic33_gpio_signal(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, &high)");
    dspic33_destroy(cpu);
}

static void test_null_getters(TestState* state) {
    expect(state, dspic33_get_register(NULL, 0u) == 0u,
           "dspic33_get_register(NULL, 0u) == 0u");
    expect(state, dspic33_get_program_counter(NULL) == 0u,
           "dspic33_get_program_counter(NULL) == 0u");
    expect(state, dspic33_get_instruction_count(NULL) == 0u,
           "dspic33_get_instruction_count(NULL) == 0u");
    expect(state, dspic33_get_cycle_count(NULL) == 0u,
           "dspic33_get_cycle_count(NULL) == 0u");
    expect(state, dspic33_get_stop(NULL) == DSPIC33_HALTED,
           "dspic33_get_stop(NULL) == DSPIC33_HALTED");
    expect(state, dspic33_get_fault_address(NULL) == 0u,
           "dspic33_get_fault_address(NULL) == 0u");
    expect(state, dspic33_get_interrupt_count(NULL) == 0u,
           "dspic33_get_interrupt_count(NULL) == 0u");
    expect(state, dspic33_get_last_interrupt(NULL) == UINT16_MAX,
           "dspic33_get_last_interrupt(NULL) == UINT16_MAX");
    expect(state, dspic33_get_interrupt_depth(NULL) == 0u,
           "dspic33_get_interrupt_depth(NULL) == 0u");
}

int main(void) {
    TestState state = {0};
    test_lifecycle(&state);
    test_execution(&state);
    test_host_operations(&state);
    test_null_getters(&state);
    return test_finish(&state);
}
