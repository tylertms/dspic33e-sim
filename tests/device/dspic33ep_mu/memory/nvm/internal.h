#ifndef DSPIC33_NVM_TEST_INTERNAL_H
#define DSPIC33_NVM_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "dspic33_firmware_image.h"
#include "elf_image.h"
#include "test.h"

enum {
    NVM_CONTROL = 0x0728u,
    NVM_ADDRESS = 0x072au,
    NVM_ADDRESS_HIGH = 0x072cu,
    NVM_KEY = 0x072eu,
    MAIN_CLOCK_DIVISOR = 0x0744u,
    NVM_WRITE = 0x8000u,
    NVM_WRITE_ENABLE = 0x4000u,
    NVM_WRITE_ERROR = 0x2000u,
    NVM_IRQ = 15u,
    NVM_SEQUENCE_BASE = 0x0400u,
    CODEGUARD_GENERAL_CONFIGURATION = 0xf80004u,
    CODEGUARD_AUXILIARY_CONFIGURATION = 0xf80010u,
    PERSISTENT_PROGRAM_TAG = 0x01000000u,
    PERSISTENT_PROGRAM_BASE = 0x2000u,
    PERSISTENT_PROGRAM_LIMIT = 0x5000u,
    MOVE_KEY_55 = 0x200550u,
    MOVE_KEY_AA = 0x200aa0u,
    WRITE_NVM_KEY = 0x883970u,
    SET_NVM_WRITE = 0xa8e729u,
    TBLRDL_W2_W3 = 0xba0192u,
    TBLRDL_BYTE_W2_W3 = 0xba4192u,
    TBLRDH_W2_W3 = 0xba8192u,
    TBLRDH_BYTE_W2_W3 = 0xbac192u,
    OPCODE_NOP = 0x000000u,
    OPCODE_RESET = 0xfe0000u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_IDLE = 0xfe4001u,
    OPCODE_RETURN = 0x060000u,
    OPCODE_RETFIE = 0x064000u,
    OPCODE_COMPUTED_CALL_W0 = 0x018800u,
    OPCODE_COMPUTED_GOTO_W0 = 0x018c00u,
    OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT = 0x4112b4u,
    OPCODE_MOV_LITERAL_0X1234_W2 = 0x212342u,
    OPCODE_MOV_W1_W2 = 0x780111u,
    OPCODE_BTSC_W2_BIT_0 = 0xa70002u,
    OPCODE_DO_1 = 0x080001u,
    OPCODE_GOTO_0X100 = 0x040100u
};

bool dspic33_nvm_test_codeguard_configuration_high(uint8_t configuration);
bool dspic33_nvm_test_codeguard_security_reset(const Dspic33* cpu, uint64_t reset_count);
bool dspic33_nvm_test_execute_start_sequence(Dspic33* cpu, bool delayed_write);
bool dspic33_nvm_test_finish_operation(Dspic33* cpu);
bool dspic33_nvm_test_interrupt_flag(Dspic33* cpu);
bool dspic33_nvm_test_load_codeguard_configuration(Dspic33* cpu, uint8_t general,
                                                   uint8_t auxiliary);
bool dspic33_nvm_test_read_table(Dspic33* cpu, uint32_t address, uint32_t opcode, uint16_t* value);
bool dspic33_nvm_test_start_operation_from(Dspic33* cpu, uint16_t operation, uint32_t address,
                                           uint32_t execution_address);
bool dspic33_nvm_test_start_operation(Dspic33* cpu, uint16_t operation, uint32_t address);
uint16_t dspic33_nvm_test_execute_codeguard_psv_read(Dspic33* cpu, uint32_t origin,
                                                     uint32_t target);
uint16_t dspic33_nvm_test_execute_codeguard_table_read(Dspic33* cpu, uint32_t origin,
                                                       uint32_t target);
uint32_t dspic33_nvm_test_program_word(const Dspic33* cpu, uint32_t address);
uint8_t dspic33_nvm_test_codeguard_configuration_value(uint8_t index);
void dspic33_nvm_test_auxiliary_access_and_execution_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_auxiliary_loader_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_auxiliary_nvm_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_codeguard_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_configuration_operation_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_configuration_programming_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_configuration_table_view_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_configure_operation(Dspic33* cpu, uint16_t operation, uint32_t address,
                                          bool write_enable);
void dspic33_nvm_test_erase_operation_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_invalid_operation_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_invalid_target_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_key_byte_access_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_key_sequence_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_load_long_program_flow(Dspic33* cpu, uint32_t origin, uint32_t target,
                                             bool call);
void dspic33_nvm_test_load_program_return(Dspic33* cpu, uint32_t origin, uint32_t target,
                                          uint32_t opcode);
void dspic33_nvm_test_pair_and_capture_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_power_save_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_program_range_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_reset_and_access_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_row_operation_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_same_segment_stall_erratum_cases(TestState* state, Dspic33* cpu);
void dspic33_nvm_test_stall_and_interrupt_cases(TestState* state, Dspic33* cpu);

#endif
