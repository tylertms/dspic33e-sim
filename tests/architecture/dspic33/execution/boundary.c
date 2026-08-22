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

static void range_cases(TestState* state, Dspic33* cpu) {
    expect(state, !dspic33_program_range_implemented(DSPIC33_PROGRAM_LIMIT - 1u, 2u),
           "program range rejects a main-region overflow");
    expect(state, !dspic33_program_range_implemented(DSPIC33_PROGRAM_LIMIT, 0u),
           "program range rejects the main program limit");
    expect(state, !dspic33_program_range_implemented(DSPIC33_AUXILIARY_PROGRAM_BASE - 2u, 2u),
           "program range rejects the gap before auxiliary memory");
    expect(state,
           dspic33_program_range_implemented(DSPIC33_AUXILIARY_PROGRAM_BASE,
                                             DSPIC33_AUXILIARY_PROGRAM_LIMIT -
                                                 DSPIC33_AUXILIARY_PROGRAM_BASE),
           "program range accepts the complete auxiliary region");
    expect(state, !dspic33_program_range_implemented(DSPIC33_AUXILIARY_PROGRAM_LIMIT - 2u, 4u),
           "program range rejects an auxiliary overflow");
    expect(state, dspic33_data_range_valid(DSPIC33_DATA_SIZE, 0u),
           "data range accepts an empty terminal range");
    expect(state, !dspic33_data_range_valid(DSPIC33_DATA_SIZE, 1u),
           "data range rejects a terminal byte");
    expect(state, !dspic33_data_range_valid(DSPIC33_DATA_SIZE + 1u, 0u),
           "data range rejects an address beyond memory");
    expect(state, !dspic33_internal_vector_segment_execution_address(1u),
           "vector segment rejects an odd address");
    expect(state, !dspic33_internal_vector_segment_execution_address(0u),
           "vector segment rejects an address below the segment");
    expect(state, !dspic33_internal_vector_segment_execution_address(VECTOR_SEGMENT_LIMIT),
           "vector segment rejects its upper limit");
    expect(state, !dspic33_internal_auxiliary_program_address(DSPIC33_AUXILIARY_PROGRAM_BASE - 2u),
           "auxiliary classification rejects the preceding address");
    expect(state, !dspic33_internal_auxiliary_program_address(DSPIC33_AUXILIARY_PROGRAM_LIMIT),
           "auxiliary classification rejects its upper limit");
    expect(state,
           !dspic33_internal_program_target_requires_address_error(cpu,
                                                                   DSPIC33_AUXILIARY_PROGRAM_LIMIT),
           "program target accepts the address beyond the auxiliary region");
    expect(state,
           dspic33_internal_program_target_requires_address_error(cpu, DSPIC33_PROGRAM_LIMIT),
           "program target rejects an unimplemented main address");

    expect(state, !dspic33_internal_codeguard_high_security(0x03u),
           "codeguard recognizes an unlocked configuration");
    expect(state, dspic33_internal_codeguard_high_security(0x33u),
           "codeguard recognizes a mismatched security key");

    Dspic33epMuDevice device = cpu->device;
    cpu->device = DSPIC33EP_MU_DEVICE_COUNT;
    expect(state,
           !dspic33_device_program_range_implemented(cpu, 0u, 2u) &&
               !dspic33_device_data_range_implemented(cpu, 0u, 2u) &&
               dspic33_internal_device_program_limit(cpu) == 0u,
           "invalid device profile exposes no implemented memory");
    cpu->device = device;
}

static void rmw_admission_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.cpu_write_valid = true;
    expect(state, !dspic33_cpu_rmw_matches(cpu, 0x100u, 2u),
           "non-RMW write does not match a target");
    cpu->io.cpu_write_rmw = true;
    expect(state, !dspic33_cpu_rmw_matches(cpu, 0x100u, 2u),
           "non-advancing RMW does not match a target");
    cpu->instruction_advancing = true;
    cpu->io.cpu_write_instruction = 1u;
    expect(state, !dspic33_cpu_rmw_matches(cpu, 0x100u, 2u),
           "RMW from a different instruction does not match");
    cpu->io.cpu_write_instruction = cpu->instructions;
    cpu->io.cpu_write_address = 0x100u;
    cpu->io.cpu_write_width = 2u;
    expect(state,
           !dspic33_cpu_rmw_matches(cpu, 0x0fcu, 2u) && !dspic33_cpu_rmw_matches(cpu, 0x104u, 2u) &&
               dspic33_cpu_rmw_matches(cpu, 0x101u, 2u),
           "RMW target matching requires overlapping ranges");
}

static void operand_resolution_cases(TestState* state, Dspic33* cpu) {
    uint16_t registers[16] = {0u};
    OperandResolution resolution;
    uint32_t address = 0u;

    dspic33_reset(cpu, 0u);
    expect(state,
           !dspic33_internal_resolve_operand_address(cpu, registers, 0u, 0u, 0u, 2u, false,
                                                     &resolution),
           "operand resolution rejects register-direct mode");

    cpu->data[0x0046u] = 0u;
    cpu->data[0x0047u] = 0x80u;
    cpu->data[0x0048u] = 0x00u;
    cpu->data[0x0049u] = 0x01u;
    cpu->data[0x004au] = 0x0fu;
    cpu->data[0x004bu] = 0x01u;
    expect(state, dspic33_internal_modulo_address(cpu, 0u, 0x00ff, -1, false) == 0x010fu,
           "negative modulo addressing wraps to the end");
    expect(state, dspic33_internal_modulo_address(cpu, 0u, 0x0110, 1, false) == 0x0100u,
           "positive modulo addressing wraps to the start");
    expect(state, dspic33_internal_modulo_address(cpu, 0u, 0x0120, 1, false) == 0x0120u,
           "positive modulo addressing preserves a distant address");
    expect(state, dspic33_internal_modulo_address(cpu, 0u, 0x00e0, -1, false) == 0x00e0u,
           "negative modulo addressing preserves a distant address");
    expect(state, dspic33_internal_modulo_address(cpu, 0u, 0x0100, 0, false) == 0x0100u,
           "zero-delta modulo addressing preserves its address");

    cpu->data[0x0050u] = 1u;
    cpu->data[0x0051u] = 0x80u;
    registers[0] = 0x1000u;
    expect(state,
           dspic33_internal_resolve_operand_address(cpu, registers, 3u, 0u, 0u, 2u, true,
                                                    &resolution) &&
               resolution.updates_register,
           "postincrement resolves a bit-reversed destination");
    expect(state,
           dspic33_internal_resolve_operand_address(cpu, registers, 5u, 0u, 0u, 2u, true,
                                                    &resolution) &&
               resolution.address == registers[0],
           "preincrement resolves a bit-reversed destination");

    cpu->data[0x0047u] = 0u;
    registers[0] = 0u;
    expect(state,
           dspic33_internal_resolve_operand_address(cpu, registers, 2u, 0u, 0u, 2u, false,
                                                    &resolution) &&
               resolution.wrapped,
           "postdecrement records an address wrap");
    expect(state,
           dspic33_internal_resolve_operand_address(cpu, registers, 4u, 0u, 0u, 2u, false,
                                                    &resolution) &&
               resolution.wrapped,
           "predecrement records an address wrap");
    registers[0] = UINT16_MAX;
    expect(state,
           dspic33_internal_resolve_operand_address(cpu, registers, 3u, 0u, 0u, 2u, false,
                                                    &resolution) &&
               resolution.wrapped,
           "postincrement records an address wrap");

    registers[0] = 0x8000u;
    registers[1] = UINT16_MAX;
    cpu->dsrpag = 1u;
    expect(state,
           dspic33_internal_resolve_operand_address(cpu, registers, 6u, 0u, 1u, 2u, false,
                                                    &resolution) &&
               !resolution.wrapped && resolution.address == UINT16_MAX,
           "indexed paged addressing preserves the mapped half after wrap");

    dspic33_reset(cpu, 0u);
    cpu->address_error = true;
    cpu->address_error_access_allowed = false;
    expect(state, !dspic33_internal_validate_destination_after_source_execution(cpu, 0u, 0u, 2u),
           "destination validation preserves a blocking address error");

    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_internal_write_operand_byte(cpu, 1u, 0u, 0u, 1u),
           "operand write rejects an uninitialized address register");
    expect(state, cpu->illegal_reset, "uninitialized operand write raises an illegal reset");

    dspic33_reset(cpu, 0u);
    dspic33_set_working_register(cpu, 0u, 0x8000u);
    cpu->dsrpag = 0x0300u;
    expect(state,
           dspic33_internal_indirect_literal_address(cpu, 0u, 0, false, &address) &&
               (address & PSV_ADDRESS) != 0u && (address & PSV_HIGH_BYTE) != 0u,
           "indirect program-space read selects the high byte");
}

static void indirect_address_boundary_cases(TestState* state, Dspic33* cpu) {
    uint32_t address = 0u;
    uint16_t registers[16] = {0u};
    OperandResolution resolution;

    dspic33_reset(cpu, 0u);
    dspic33_set_working_register(cpu, 15u, 0u);
    expect(state,
           dspic33_internal_indirect_literal_address(cpu, 15u, -1, true, &address) &&
               address == UINT16_MAX,
           "stack indirect addressing retains a negative effective address result");

    dspic33_reset(cpu, 0u);
    dspic33_set_working_register(cpu, 15u, UINT16_MAX);
    cpu->w[15] = UINT16_MAX;
    expect(state,
           dspic33_internal_indirect_literal_address(cpu, 15u, 1, true, &address) && address == 0u,
           "stack indirect addressing retains an overflowing effective address result");

    dspic33_reset(cpu, 0u);
    dspic33_set_working_register(cpu, 14u, 0x8000u);
    cpu->corcon = 0x0004u;
    expect(state,
           dspic33_internal_indirect_literal_address(cpu, 14u, 0, false, &address) &&
               address == 0x8000u,
           "frame pointer bypasses paged addressing when enabled");

    dspic33_reset(cpu, 0u);
    dspic33_set_working_register(cpu, 13u, 0x8000u);
    cpu->dsrpag = 0u;
    expect(state,
           dspic33_internal_indirect_literal_address(cpu, 13u, 0, false, &address) &&
               cpu->address_error,
           "ordinary indirect access rejects an unset data page");

    dspic33_reset(cpu, 0u);
    registers[14] = 0x8000u;
    expect(state,
           dspic33_internal_resolve_operand_address(cpu, registers, 2u, 14u, 0u, 2u, false,
                                                    &resolution),
           "frame-pointer postdecrement resolves without pseudo-linear addressing");

    cpu->data[0x0046u] = 0xffu;
    cpu->data[0x0047u] = 0xffu;
    expect(state,
           dspic33_internal_modulo_address(cpu, 15u, 0x1234, 1, false) == 0x1234u &&
               dspic33_internal_modulo_address(cpu, 0u, 0x1234, 1, false) == 0x1234u,
           "reserved and mismatched modulo selectors preserve their addresses");

    cpu->data[0x0046u] = 0x00u;
    cpu->data[0x0047u] = 0x80u;
    cpu->data[0x0048u] = 0x10u;
    cpu->data[0x0049u] = 0x00u;
    cpu->data[0x004au] = 0x0fu;
    cpu->data[0x004bu] = 0x00u;
    expect(state, dspic33_internal_modulo_address(cpu, 0u, 0x1234, 1, false) == 0x1234u,
           "inverted modulo bounds preserve the requested address");
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

static void reserved_page_register_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_internal_execute(cpu, 0xfecc00u) && cpu->illegal_reset,
           "reserved literal page-register selector resets the processor");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_internal_execute(cpu, 0xfedc00u) && cpu->illegal_reset,
           "reserved register page-register selector resets the processor");
}

static void literal_control_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_program_word(cpu, 0u, 6u), "load literal control extension");
    cpu->pc = 0u;
    expect(state, dspic33_internal_execute(cpu, 0x040000u) && cpu->address_error,
           "literal branch raises an address error for an unimplemented target");

    dspic33_reset(cpu, 0u);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_LIMIT + 0x60000u;
    expect(state, !dspic33_internal_execute(cpu, 0x040000u) && cpu->pc == 0x60000u,
           "literal branch normalizes an out-of-range auxiliary extension address");

    dspic33_reset(cpu, 0u);
    cpu->pc = 0x60000u;
    expect(state, !dspic33_internal_execute(cpu, 0x080000u),
           "DO rejects an unimplemented extension address");
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
        range_cases(&state, &cpu);
        rmw_admission_cases(&state, &cpu);
        operand_resolution_cases(&state, &cpu);
        indirect_address_boundary_cases(&state, &cpu);
        soft_trap_cases(&state, &cpu);
        reserved_page_register_cases(&state, &cpu);
        literal_control_boundary_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
