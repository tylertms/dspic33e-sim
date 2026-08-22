#include "architecture/dspic33/execution/internal.h"
#include "test.h"

static void accumulator_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_internal_execute_accumulator_arithmetic(cpu, 0u),
           "accumulator arithmetic rejects an unknown operation");

    cpu->corcon = 0x0020u;
    cpu->accumulator[0] = (int64_t)INT32_MAX + 1;
    expect(state, dspic33_internal_execute_accumulator_store(cpu, 0u) && cpu->w[0] == 0x7fffu,
           "accumulator store saturates a positive overflow");
    cpu->accumulator[0] = (int64_t)INT32_MIN - 1;
    expect(state, dspic33_internal_execute_accumulator_store(cpu, 0u) && cpu->w[0] == 0x8000u,
           "accumulator store saturates a negative overflow");
}

static void multiply_guard_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_internal_execute_dsp_multiply(cpu, 0u),
           "DSP multiply rejects an unrelated opcode");
    expect(state, !dspic33_internal_execute_dsp_multiply(cpu, 0xc30000u),
           "DSP multiply rejects a reserved register pair");
    expect(state, !dspic33_internal_execute_dsp_clear_or_move(cpu, 0xc30003u),
           "DSP clear rejects a reserved write-back mode");

    cpu->corcon = 0x3000u;
    expect(state, !dspic33_internal_execute_dsp_multiply(cpu, 0xf00000u),
           "DSP square rejects an invalid sign mode");
    cpu->corcon = 0u;
    expect(state, dspic33_internal_execute_dsp_multiply(cpu, 0xf00000u),
           "DSP square accepts its base register form");
}

static void divide_flag_case(TestState* state, Dspic33* cpu) {
    cpu->sr = 0u;
    dspic33_internal_update_divide_flags(cpu, -1, false);
    expect(state, (cpu->sr & 0x0008u) != 0u, "negative divide remainder sets the negative flag");
}

static void addressing_guard_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_internal_execute_compare(cpu, 0u) && cpu->illegal_reset,
           "compare rejects an unrelated opcode");

    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_internal_execute_unary(cpu, 0u),
           "unary execution rejects an unrelated opcode");
    expect(state, !dspic33_internal_execute_compare_control(cpu, 0u),
           "compare control rejects an unrelated opcode");
    expect(state, !dspic33_internal_execute_file_unary(cpu, 0u),
           "file unary execution rejects an unrelated opcode");
}

static void soft_trap_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_internal_schedule_soft_trap(cpu, 1u, 0x10u, 1u, 5u);
    cpu->instruction_active = true;
    cpu->current_instruction_pc = 0x200u;
    dspic33_internal_schedule_soft_trap(cpu, 1u, 0x10u, 1u, 2u);
    expect(state,
           cpu->pending_soft_traps[0].delay == 2u && !cpu->pending_soft_traps[0].auxiliary_program,
           "duplicate soft trap keeps the earlier deadline and instruction address");

    dspic33_internal_schedule_soft_trap(cpu, 2u, 0x20u, 1u, 1u);
    dspic33_internal_schedule_soft_trap(cpu, 3u, 0x30u, 1u, 1u);
    dspic33_internal_schedule_soft_trap(cpu, 4u, 0x40u, 1u, 1u);
    dspic33_internal_schedule_soft_trap(cpu, 5u, 0x50u, 1u, 1u);
    expect(state, cpu->pending_soft_traps[3].trap == 4u,
           "full soft-trap queue ignores an additional source");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize execution boundary processor");
    if (initialized) {
        accumulator_cases(&state, &cpu);
        multiply_guard_cases(&state, &cpu);
        divide_flag_case(&state, &cpu);
        addressing_guard_cases(&state, &cpu);
        soft_trap_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
