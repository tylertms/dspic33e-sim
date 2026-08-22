#ifndef DSPIC33EP_MU_SIM_DSPIC33_EXECUTION_H
#define DSPIC33EP_MU_SIM_DSPIC33_EXECUTION_H

#include <stdlib.h>
#include <string.h>

#include "architecture/dspic33/internal.h"
#include "device/dspic33ep_mu/data.h"
#include "device/dspic33ep_mu/device.h"

#define DSPIC33_ACCUMULATOR_MASK UINT64_C(0xffffffffff)

enum {
    PSV_ADDRESS = 0x01000000u,
    PSV_HIGH_BYTE = 0x02000000u,
    PSV_ADDRESS_MASK = 0x007fffffu,
    PERSISTENT_PROGRAM_PHYSICAL_BASE = 0x2000u,
    PERSISTENT_PROGRAM_PHYSICAL_LIMIT = 0x5000u,
    VECTOR_SEGMENT_EXECUTION_BASE = 0x000002u,
    VECTOR_SEGMENT_LIMIT = 0x000200u,
    CODEGUARD_GENERAL_CONFIGURATION_OFFSET = 0x04u,
    CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET = 0x10u
};

typedef enum {
    DSPIC33_RESET_SOFTWARE,
    DSPIC33_RESET_ILLEGAL,
    DSPIC33_RESET_HARDWARE
} Dspic33ResetKind;

typedef enum {
    COMPARE_CONTROL_NONE,
    COMPARE_CONTROL_EQUAL,
    COMPARE_CONTROL_NOT_EQUAL,
    COMPARE_CONTROL_GREATER_THAN,
    COMPARE_CONTROL_LESS_THAN
} CompareControlKind;

typedef struct {
    uint32_t address;
    int32_t effective_address;
    uint16_t updated_register;
    uint16_t access_register;
    uint16_t access_data_page;
    uint16_t updated_data_page;
    bool wrapped;
    bool updates_register;
    bool paged_addressing_enabled;
    bool updates_data_page;
    bool unimplemented_data_page;
} OperandResolution;

typedef struct {
    uint32_t address;
    uint16_t updated_register;
    uint16_t updated_data_page;
    uint8_t base_register;
    bool present;
    bool access_valid;
    bool update_valid;
    bool updates_register;
    bool updates_data_page;
} DspPrefetchOutcome;

extern const uint8_t dspic33_internal_configuration_factory_defaults[8];
extern const uint8_t dspic33_internal_configuration_program_masks[8];
extern const int8_t dspic33_internal_dsp_prefetch_updates[16];

bool dspic33_internal_accumulator_byte_location(uint32_t address, uint8_t* accumulator,
                                                uint8_t* byte);
bool dspic33_internal_accumulator_shift_encoding_valid(uint32_t opcode);
bool dspic33_internal_address_register_initialized(Dspic33* cpu, uint8_t reg);
bool dspic33_internal_auxiliary_program_address(uint32_t address);
bool dspic33_internal_bit_encoding_valid(uint32_t opcode);
bool dspic33_internal_byte_extension_encoding_valid(uint32_t opcode);
bool dspic33_internal_check_data_alignment(Dspic33* cpu, uint32_t address);
bool dspic33_internal_check_data_implementation(Dspic33* cpu, uint32_t address, uint8_t width);
bool dspic33_internal_codeguard_high_security(uint8_t configuration);
bool dspic33_internal_codeguard_programming_allowed(const Dspic33* cpu, uint32_t target);
bool dspic33_internal_compare_control_taken(const Dspic33* cpu, uint32_t opcode,
                                            CompareControlKind kind);
bool dspic33_internal_computed_control_transfer_encoding(uint32_t opcode);
bool dspic33_internal_configuration_register_index(uint32_t address, uint8_t* index);
bool dspic33_internal_data_byte_is_implemented(const Dspic33* cpu, uint32_t address);
bool dspic33_internal_divide_encoding_valid(uint32_t opcode);
bool dspic33_internal_do_flash_access_boundary(const Dspic33* cpu, uint32_t opcode,
                                               uint32_t instruction_pc, bool psv_read);
bool dspic33_internal_dsp_encoding_valid(uint32_t opcode);
bool dspic33_internal_exception_pending(const Dspic33* cpu);
bool dspic33_internal_execute_accumulator_arithmetic(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_accumulator_shift(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_accumulator_store(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_accumulator_word(Dspic33* cpu, uint32_t opcode, bool add);
bool dspic33_internal_execute_binary(Dspic33* cpu, uint32_t opcode, uint32_t operation);
bool dspic33_internal_execute_bit(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_compare_control(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_compare(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_decimal_adjust(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_dsp_clear_or_move(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_dsp_multiply(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_euclidean_distance(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_file_binary(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_file_shift(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_file_unary(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_find_first_sign_change(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_find_first(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_literal_binary(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_move_double(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_move_literal(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_move_offset(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_move(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_multiply(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_shift(Dspic33* cpu, uint32_t opcode, bool left);
bool dspic33_internal_execute_single_shift(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_table(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute_unary(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_execute(Dspic33* cpu, uint32_t opcode);
bool dspic33_internal_file_shift_encoding_valid(uint32_t opcode);
bool dspic33_internal_find_first_encoding_valid(uint32_t opcode, bool sign_change);
bool dspic33_internal_flash_read_erratum_sequence_completed(Dspic33* cpu, uint32_t opcode,
                                                            uint32_t instruction_pc, bool psv_read);
bool dspic33_internal_following_operand_address(Dspic33* cpu, const OperandResolution* resolution,
                                                bool write, uint32_t* address);
bool dspic33_internal_indirect_literal_address(Dspic33* cpu, uint8_t reg, int16_t offset,
                                               bool write, uint32_t* resolved_address);
bool dspic33_internal_instruction_changes_program_flow(const Dspic33* cpu, uint32_t opcode,
                                                       uint32_t instruction_pc);
bool dspic33_internal_instruction_rmw_write_matches(const Dspic33* cpu, uint32_t address,
                                                    uint8_t width);
bool dspic33_internal_literal_control_extension_valid(uint32_t extension);
bool dspic33_internal_literal_control_first_word_valid(uint32_t opcode);
bool dspic33_internal_long_control_transfer(uint32_t opcode, uint8_t* source, bool* call);
bool dspic33_internal_multiple_shift_encoding_valid(uint32_t opcode, bool left);
bool dspic33_internal_multiply_encoding_valid(uint32_t opcode);
bool dspic33_internal_nested_zero_do_workaround_present(const Dspic33* cpu, uint32_t inner_start);
bool dspic33_internal_nvm_stall_erratum_applies(const Dspic33* cpu);
bool dspic33_internal_nvm_stalls_cpu(const Dspic33* cpu);
bool dspic33_internal_operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg, uint8_t offset_reg,
                                      uint8_t width, bool write, uint32_t* address);
bool dspic33_internal_operand_resolution(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                         uint8_t offset_reg, uint8_t width, bool write,
                                         OperandResolution* resolution);
bool dspic33_internal_program_target_requires_address_error(const Dspic33* cpu, uint32_t address);
bool dspic33_internal_relative_branch_condition(const Dspic33* cpu, uint32_t opcode, bool* take);
bool dspic33_internal_reserved_return_encoding(uint32_t opcode);
bool dspic33_internal_resolve_operand_address(const Dspic33* cpu, const uint16_t* registers,
                                              uint8_t mode, uint8_t reg, uint8_t offset_reg,
                                              uint8_t width, bool write,
                                              OperandResolution* resolution);
bool dspic33_internal_service_pending_soft_trap(Dspic33* cpu);
bool dspic33_internal_single_shift_encoding_valid(uint32_t opcode);
bool dspic33_internal_stack_encoding_valid(uint32_t opcode);
bool dspic33_internal_system_encoding_valid(uint32_t opcode);
bool dspic33_internal_table_encoding_valid(uint32_t opcode);
bool dspic33_internal_validate_destination_after_source_execution(Dspic33* cpu, uint8_t mode,
                                                                  uint8_t reg, uint8_t width);
bool dspic33_internal_validate_operand_alignment(Dspic33* cpu, uint16_t* registers, uint8_t mode,
                                                 uint8_t reg, uint8_t offset_reg, uint8_t width,
                                                 bool write, bool indirect_bit);
bool dspic33_internal_vector_segment_execution_address(uint32_t address);
bool dspic33_internal_write_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                         uint8_t offset_reg, uint8_t value);
bool dspic33_internal_write_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                         uint8_t offset_reg, uint16_t value);
CompareControlKind dspic33_internal_compare_control_kind(uint32_t opcode);
int64_t dspic33_internal_accumulator_value(uint64_t bits);
int8_t dspic33_internal_compare_control_displacement(uint32_t opcode);
uint16_t dspic33_internal_modulo_address(const Dspic33* cpu, uint8_t reg, int32_t address,
                                         int32_t delta, bool y_space);
uint16_t dspic33_internal_read_data_word(Dspic33* cpu, uint32_t address);
uint16_t dspic33_internal_read_file_word(Dspic33* cpu, uint16_t address);
uint16_t dspic33_internal_read_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                            uint8_t offset_reg);
uint16_t dspic33_internal_read_word(Dspic33* cpu, uint32_t address);
uint32_t dspic33_internal_device_program_limit(const Dspic33* cpu);
uint32_t dspic33_internal_direct_move_address(const Dspic33* cpu, uint16_t address, bool write);
uint32_t dspic33_internal_instruction_length(uint32_t opcode);
uint32_t dspic33_internal_mapped_data_address(uint16_t address, uint16_t page, bool write);
uint32_t dspic33_internal_pop_program_counter(Dspic33* cpu);
uint32_t dspic33_internal_program_address_add(uint32_t address, int32_t offset);
uint32_t dspic33_internal_read_cpu_program_word(const Dspic33* cpu, uint32_t address);
uint32_t* dspic33_internal_writable_program_word(Dspic33* cpu, uint32_t address);
uint64_t dspic33_internal_instruction_cycles(const Dspic33* cpu, uint32_t opcode,
                                             uint32_t instruction_pc);
uint8_t dspic33_internal_codeguard_configuration(const Dspic33* cpu, bool auxiliary);
uint8_t dspic33_internal_read_accumulator_byte(const Dspic33* cpu, uint8_t accumulator,
                                               uint8_t byte);
uint8_t dspic33_internal_read_byte_value(Dspic33* cpu, uint32_t address);
uint8_t dspic33_internal_read_data_byte(Dspic33* cpu, uint32_t address);
uint8_t dspic33_internal_read_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                           uint8_t offset_reg);
void dspic33_internal_advance_instruction(Dspic33* cpu, uint64_t cycles, bool separate_wait_cycle,
                                          uint64_t device_ratio);
void dspic33_internal_advance_pending_nvm_reset(Dspic33* cpu);
void dspic33_internal_apply_accumulator_result(Dspic33* cpu, uint8_t accumulator, int64_t result);
void dspic33_internal_check_stack_address(Dspic33* cpu, int32_t address, bool wrapped);
void dspic33_internal_clear_accumulator_status(Dspic33* cpu, uint8_t accumulator);
void dspic33_internal_clear_instruction_transients(Dspic33* cpu);
void dspic33_internal_clear_watchdog(Dspic33* cpu);
void dspic33_internal_enter_address_trap(Dspic33* cpu, uint32_t return_pc);
void dspic33_internal_enter_trap(Dspic33* cpu, uint16_t trap, uint32_t vector, uint8_t priority,
                                 uint16_t status, uint32_t return_pc, bool auxiliary_vector);
void dspic33_internal_perform_warm_reset(Dspic33* cpu, uint16_t cause, Dspic33ResetKind kind);
void dspic33_internal_push_program_counter(Dspic33* cpu, uint32_t address);
void dspic33_internal_raise_program_read_error(Dspic33* cpu);
void dspic33_internal_raise_program_target_error(Dspic33* cpu, uint32_t return_pc);
void dspic33_internal_record_source_address_register(Dspic33* cpu, uint8_t reg);
void dspic33_internal_record_var_write(Dspic33* cpu, uint32_t address, uint8_t width);
void dspic33_internal_reset_processor(Dspic33* cpu, uint32_t entry, bool clear_memory);
void dspic33_internal_schedule_soft_trap(Dspic33* cpu, uint16_t trap, uint32_t vector,
                                         uint8_t priority, uint8_t delay);
void dspic33_internal_set_trap_source(Dspic33* cpu, uint16_t trap, uint32_t vector,
                                      uint8_t priority, uint8_t delay, bool active);
void dspic33_internal_update_divide_flags(Dspic33* cpu, int64_t remainder, bool overflow);
void dspic33_internal_update_logic_flags(Dspic33* cpu, uint16_t value, bool byte_mode);
void dspic33_internal_write_accumulator_byte(Dspic33* cpu, uint8_t accumulator, uint8_t byte,
                                             uint8_t value);
void dspic33_internal_write_disicnt_byte(Dspic33* cpu, bool high_byte, uint8_t value);
void dspic33_internal_write_status_byte(Dspic33* cpu, bool high_byte, uint8_t value);
void dspic33_internal_write_word(Dspic33* cpu, uint32_t address, uint16_t value);
void dspic33_internal_write_working_register_byte(Dspic33* cpu, uint8_t reg, bool high,
                                                  uint8_t value);
void dspic33_internal_write_working_register(Dspic33* cpu, uint8_t reg, uint16_t value);

#endif
