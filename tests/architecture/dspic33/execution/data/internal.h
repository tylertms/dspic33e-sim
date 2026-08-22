#ifndef DSPIC33_DATA_TEST_INTERNAL_H
#define DSPIC33_DATA_TEST_INTERNAL_H

#include "processor.h"

typedef enum {
    ARITHMETIC_MATRIX_SUBR,
    ARITHMETIC_MATRIX_SUBBR,
    ARITHMETIC_MATRIX_ADD,
    ARITHMETIC_MATRIX_ADDC,
    ARITHMETIC_MATRIX_SUB,
    ARITHMETIC_MATRIX_SUBB,
    ARITHMETIC_MATRIX_AND,
    ARITHMETIC_MATRIX_XOR,
    ARITHMETIC_MATRIX_IOR
} BinaryMatrixOperation;

typedef enum {
    DIRECT_FILE_SUBR,
    DIRECT_FILE_SUBBR,
    DIRECT_FILE_ADD,
    DIRECT_FILE_ADDC,
    DIRECT_FILE_SUB,
    DIRECT_FILE_SUBB,
    DIRECT_FILE_AND,
    DIRECT_FILE_XOR,
    DIRECT_FILE_IOR,
    DIRECT_FILE_INC,
    DIRECT_FILE_INC2,
    DIRECT_FILE_DEC,
    DIRECT_FILE_DEC2,
    DIRECT_FILE_NEG,
    DIRECT_FILE_COM,
    DIRECT_FILE_CLR,
    DIRECT_FILE_SETM,
    DIRECT_FILE_SL,
    DIRECT_FILE_LSR,
    DIRECT_FILE_ASR,
    DIRECT_FILE_RLNC,
    DIRECT_FILE_RLC,
    DIRECT_FILE_RRNC,
    DIRECT_FILE_RRC,
    DIRECT_FILE_CP,
    DIRECT_FILE_CPB,
    DIRECT_FILE_CP0
} DirectFileOperation;

typedef struct {
    uint16_t address;
    bool direct;
} BinaryMatrixOperand;

BinaryMatrixOperand dspic33_data_test_binary_matrix_operand(uint16_t registers[16], uint8_t mode,
                                                            uint8_t reg, uint8_t width);
bool dspic33_data_test_binary_matrix_logical(BinaryMatrixOperation operation);
bool dspic33_data_test_binary_matrix_registers_match(const Dspic33* cpu,
                                                     const uint16_t registers[16]);
bool dspic33_data_test_direct_file_address_implemented(uint16_t address);
bool dspic33_data_test_direct_file_reads_source(DirectFileOperation operation);
bool dspic33_data_test_direct_file_writes_result(DirectFileOperation operation);
bool dspic33_data_test_documented_system_encoding_valid(uint32_t opcode);
bool dspic33_data_test_load_direct_file_trap_vectors(Dspic33* cpu);
bool dspic33_data_test_run_direct_file_case(Dspic33* actual, Dspic33* reference, uint32_t opcode,
                                            DirectFileOperation operation, uint16_t address,
                                            bool byte_mode, bool file_destination);
uint16_t dspic33_data_test_binary_matrix_result(BinaryMatrixOperation operation, uint16_t left,
                                                uint16_t right, uint16_t initial_status,
                                                bool byte_mode);
uint16_t dspic33_data_test_binary_matrix_status(BinaryMatrixOperation operation, uint16_t left,
                                                uint16_t right, uint16_t initial_status,
                                                bool byte_mode);
uint16_t dspic33_data_test_direct_file_logic_status(uint16_t initial_status, uint16_t value,
                                                    bool byte_mode);
uint16_t dspic33_data_test_direct_file_result(DirectFileOperation operation, uint16_t left,
                                              uint16_t right, uint16_t initial_status,
                                              bool byte_mode);
uint16_t dspic33_data_test_direct_file_status(DirectFileOperation operation, uint16_t left,
                                              uint16_t right, uint16_t initial_status,
                                              bool byte_mode);
uint16_t dspic33_data_test_shift_matrix_result(DirectFileOperation operation, uint16_t source,
                                               uint16_t initial_status, bool byte_mode);
uint16_t dspic33_data_test_shift_matrix_status(DirectFileOperation operation, uint16_t source,
                                               uint16_t initial_status, bool byte_mode);
void dspic33_data_test_arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_binary_matrix_write_register(uint16_t registers[16], uint8_t reg,
                                                    uint16_t value);
void dspic33_data_test_bit_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_bit_operand_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_byte_extension_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_byte_extension_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_byte_extension_value_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_compare_direct_file_encoding_matrix_cases(TestState* state,
                                                                 Dspic33* invalid_cpu);
void dspic33_data_test_compare_register_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_decimal_adjust_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_direct_file_arithmetic_encoding_matrix_cases(TestState* state);
void dspic33_data_test_direct_file_bit_value_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_direct_file_logical_encoding_matrix_cases(TestState* state,
                                                                 Dspic33* invalid_cpu);
void dspic33_data_test_direct_file_shift_encoding_matrix_cases(TestState* state,
                                                               Dspic33* invalid_cpu);
void dspic33_data_test_direct_file_unary_encoding_matrix_cases(TestState* state);
void dspic33_data_test_direct_stack_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_direct_stack_value_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_divide_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_general_unary_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_link_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_loop_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_prepare_arithmetic_matrix_case(Dspic33* cpu, uint16_t registers[16],
                                                      uint16_t initial_status);
void dspic33_data_test_run_compare_control_encoding_case(TestState* state, Dspic33* cpu,
                                                         uint32_t opcode, bool alternate);
void dspic33_data_test_run_legal_binary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                                    BinaryMatrixOperation operation);
void dspic33_data_test_run_legal_unary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                                   DirectFileOperation operation);
void dspic33_data_test_shadow_stack_encoding_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_single_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_system_control_value_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_system_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_table_encoding_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_table_operand_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_data_test_table_value_cases(TestState* state, Dspic33* cpu);

#endif
