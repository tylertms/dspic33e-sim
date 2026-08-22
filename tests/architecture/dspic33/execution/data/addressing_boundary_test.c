#include "architecture/dspic33/execution/internal.h"
#include "test.h"

static void source_stack_pointer_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    dspic33_write_word(cpu, 0x1002u, 0x5678u);

    expect(state, dspic33_internal_execute_move_double(cpu, 0xbe001fu),
           "double move accepts W15 as an indirect source");
    expect(state, cpu->w[0] == 0x1234u && cpu->w[1] == 0x5678u,
           "double move reads adjacent words through W15");
}

static void destination_stack_pointer_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_set_working_register(cpu, 0u, 0x1234u);
    dspic33_set_working_register(cpu, 1u, 0x5678u);
    dspic33_set_working_register(cpu, 15u, 0x1000u);

    expect(state, dspic33_internal_execute_move_double(cpu, 0xbe8f80u),
           "double move accepts W15 as an indirect destination");
    expect(state,
           dspic33_read_word(cpu, 0x1000u) == 0x1234u && dspic33_read_word(cpu, 0x1002u) == 0x5678u,
           "double move writes adjacent words through W15");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize addressing boundary processor");
    if (initialized) {
        source_stack_pointer_case(&state, &cpu);
        destination_stack_pointer_case(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
