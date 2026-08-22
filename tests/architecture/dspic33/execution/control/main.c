#include "architecture/dspic33/execution/control/internal.h"

static void accumulator_operation_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_ADD_A_AND_B);
    cpu->accumulator[0] = 7;
    cpu->accumulator[1] = -2;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 5 &&
               cpu->accumulator[1] == -2,
           "accumulator addition stores in A");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_NEGATE_B);
    cpu->accumulator[0] = 7;
    cpu->accumulator[1] = -2;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 7 &&
               cpu->accumulator[1] == 2,
           "accumulator negation stores in B");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_SUBTRACT_B_FROM_A);
    cpu->accumulator[0] = 7;
    cpu->accumulator[1] = -2;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 9 &&
               cpu->accumulator[1] == -2,
           "accumulator subtraction stores in A");
}

static void accumulator_store_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_STORE_A_W2);
    cpu->accumulator[0] = 0x12348000;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u,
           "accumulator store truncates the low word");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_ROUNDED_STORE_A_W2);
    cpu->corcon |= 0x0002u;
    cpu->accumulator[0] = 0x12348000;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1235u,
           "accumulator store applies conventional rounding");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_SHIFTED_STORE_A_W4);
    cpu->accumulator[0] = 0x00008000;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 1u,
           "accumulator store applies the encoded shift");
}

static void bit_reversed_addressing_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2_BIT_REVERSED_INCREMENT);
    dspic33_set_working_register(cpu, 1u, 0x5a5au);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x0046u, 0x0200u);
    dspic33_write_word(cpu, 0x0050u, 0x8002u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1004u &&
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au,
           "bit-reversed post-increment updates the selected pointer");
}

static void run_limit_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state,
           dspic33_run(cpu, 1u) == DSPIC33_INSTRUCTION_LIMIT && cpu->pc == 2u &&
               cpu->instructions == 1u,
           "run stops at the instruction limit");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "processor control test initializes");
    if (initialized) {
        dspic33_control_test_conditional_branch_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_computed_control_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_literal_control_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_return_encoding_matrix_cases(&state, &cpu);
        dspic33_reset(&cpu, 0u);
        dspic33_set_async_events(&cpu, false);
        dspic33_control_test_general_dsp_encoding_matrix_cases(&state, &cpu);
        dspic33_reset(&cpu, 0u);
        dspic33_set_async_events(&cpu, false);
        dspic33_control_test_special_dsp_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_square_dsp_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_euclidean_dsp_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_invalid_dsp_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_generic_multiply_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_file_multiply_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_move_encoding_matrix_cases(&state, &cpu);
        dspic33_control_test_flash_read_erratum_cases(&state, &cpu);
        dspic33_control_test_do_flash_access_erratum_cases(&state, &cpu);
        dspic33_control_test_illegal_condition_reset_cases(&state, &cpu);
        accumulator_operation_cases(&state, &cpu);
        accumulator_store_cases(&state, &cpu);
        bit_reversed_addressing_cases(&state, &cpu);
        run_limit_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
