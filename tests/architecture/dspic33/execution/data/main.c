#include "architecture/dspic33/execution/data/internal.h"

static void compare_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xe60000u; opcode < 0xe80000u; opcode += 257u) {
        dspic33_data_test_run_compare_control_encoding_case(state, cpu, opcode, false);
        dspic33_data_test_run_compare_control_encoding_case(state, cpu, opcode, true);
    }
}

static void compare_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    dspic33_data_test_compare_register_encoding_matrix_cases(state, cpu);
    dspic33_data_test_compare_direct_file_encoding_matrix_cases(state, cpu);
    compare_control_encoding_matrix_cases(state, cpu);
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "processor data test initializes");
    if (initialized) {
        dspic33_data_test_loop_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_bit_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_direct_file_bit_value_cases(&state, &cpu);
        dspic33_data_test_bit_operand_lifecycle_cases(&state, &cpu);
        dspic33_data_test_table_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_table_value_cases(&state, &cpu);
        dspic33_data_test_table_operand_lifecycle_cases(&state, &cpu);
        dspic33_data_test_system_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_system_control_value_cases(&state, &cpu);
        dspic33_data_test_divide_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_decimal_adjust_cases(&state, &cpu);
        dspic33_data_test_arithmetic_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_shift_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_byte_extension_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_byte_extension_value_matrix_cases(&state, &cpu);
        dspic33_data_test_byte_extension_lifecycle_cases(&state, &cpu);
        dspic33_data_test_direct_stack_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_direct_stack_value_cases(&state, &cpu);
        dspic33_data_test_link_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_shadow_stack_encoding_cases(&state, &cpu);
        dspic33_data_test_general_unary_encoding_matrix_cases(&state, &cpu);
        compare_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_direct_file_arithmetic_encoding_matrix_cases(&state);
        dspic33_data_test_direct_file_logical_encoding_matrix_cases(&state, &cpu);
        dspic33_data_test_direct_file_unary_encoding_matrix_cases(&state);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
