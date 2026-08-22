#ifndef DSPIC33_CONTROL_TEST_INTERNAL_H
#define DSPIC33_CONTROL_TEST_INTERNAL_H

#include "processor.h"

typedef struct {
    uint8_t register_offset;
    int8_t access_offset;
    int8_t update;
    bool present;
} DspMatrixPrefetch;

typedef struct {
    uint16_t address;
    bool direct;
} MoveMatrixOperand;

enum {
    DSP_MATRIX_WRITE_BACK_DIRECT = 0u,
    DSP_MATRIX_WRITE_BACK_INDIRECT = 1u,
    DSP_MATRIX_WRITE_BACK_NONE = 2u
};

static const DspMatrixPrefetch dsp_matrix_prefetches[16] = {
    {0u, 0, 0, true},  {0u, 0, 2, true},  {0u, 0, 4, true},  {0u, 0, 6, true},
    {0u, 0, 0, false}, {0u, 0, -6, true}, {0u, 0, -4, true}, {0u, 0, -2, true},
    {1u, 0, 0, true},  {1u, 0, 2, true},  {1u, 0, 4, true},  {1u, 0, 6, true},
    {1u, 2, 0, true},  {1u, 0, -6, true}, {1u, 0, -4, true}, {1u, 0, -2, true},
};

MoveMatrixOperand dspic33_control_test_resolve_move_matrix_operand(uint16_t registers[16],
                                                                   uint8_t mode, uint8_t reg,
                                                                   uint8_t offset_reg,
                                                                   uint8_t width);
void dspic33_control_test_computed_control_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_conditional_branch_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_do_flash_access_erratum_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_euclidean_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_file_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_flash_read_erratum_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_general_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_generic_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_illegal_condition_reset_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_invalid_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_literal_control_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_move_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_move_literal_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_move_register_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_movpag_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_prepare_move_matrix_case(Dspic33* cpu);
void dspic33_control_test_prepare_move_registers(Dspic33* cpu, uint16_t expected[16], uint16_t base,
                                                 uint16_t stride);
void dspic33_control_test_return_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_run_invalid_move_matrix_case(TestState* state, Dspic33* cpu,
                                                       uint32_t opcode);
void dspic33_control_test_run_legal_dsp_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                                    uint8_t target_accumulator,
                                                    int64_t target_result, uint8_t x_operation,
                                                    uint8_t y_operation, uint8_t x_destination,
                                                    uint8_t y_destination, uint8_t write_back,
                                                    int8_t difference_destination);
void dspic33_control_test_special_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_control_test_square_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu);

#endif
