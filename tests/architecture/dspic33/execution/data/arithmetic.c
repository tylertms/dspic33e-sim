#include "architecture/dspic33/execution/data/internal.h"

void dspic33_data_test_byte_extension_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xfb0192u);
    dspic33_set_working_register(cpu, 2u, 0x0800u);
    dspic33_write_word(cpu, 0x0800u, 0x00a5u);
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->w[3] == 0xffa5u &&
               cpu->w[2] == 0x0800u && cpu->sr == 0x010cu,
           "SE non-CPU SFR byte source consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xfb8192u);
    cpu->w[2] = 0x5000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu, "ZE uninitialized source pointer resets processor");
}

static void prepare_stack_encoding_case(Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    dspic33_set_working_register(cpu, 14u, 0x4444u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
}

static void run_invalid_stack_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    prepare_stack_encoding_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->illegal_reset && cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->pc == 0u && cpu->w[15] == 0x1000u &&
              cpu->initialized_working_registers == 0x8000u && cpu->last_trap == UINT16_MAX &&
              cpu->unsupported_opcode == 0u && (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u;
    expect_dsp_matrix_case(state, matches, opcode, "reserved stack encoding");
}

static void run_direct_stack_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                           uint16_t address) {
    uint16_t value = (uint16_t)(address ^ 0xa55au);
    bool matches;

    prepare_stack_encoding_case(cpu);
    dspic33_write_word(cpu, address, value);
    dspic33_write_word(cpu, 0x4ffeu, 0x1357u);
    dspic33_write_word(cpu, 0x5000u, 0x5aa5u);
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "direct PUSH and POP encoding");
}

void dspic33_data_test_direct_stack_value_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t addresses[] = {0x1000u, 0x2000u, 0xdffeu};
    size_t index;

    for (index = 0u; index < sizeof(addresses) / sizeof(addresses[0]); index++) {
        uint16_t address = addresses[index];
        uint16_t value = (uint16_t)(0xa55au ^ address);
        uint64_t cycles;

        prepare_stack_encoding_case(cpu);
        dspic33_write_word(cpu, address, value);
        cycles = cpu->cycles;
        expect(state,
               dspic33_load_program_word(cpu, 0u, 0xf80000u | address) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                   cpu->cycles - cycles == 1u && cpu->w[15] == 0x5002u &&
                   dspic33_read_word(cpu, 0x5000u) == value &&
                   dspic33_read_word(cpu, address) == value && !cpu->illegal_reset,
               "direct PUSH covers the full implemented file address range");

        prepare_stack_encoding_case(cpu);
        dspic33_write_word(cpu, 0x4ffeu, value);
        cycles = cpu->cycles;
        expect(state,
               dspic33_load_program_word(cpu, 0u, 0xf90000u | address) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                   cpu->cycles - cycles == 1u && cpu->w[15] == 0x4ffeu &&
                   dspic33_read_word(cpu, address) == value && !cpu->illegal_reset,
               "direct POP covers the full implemented file address range");
    }

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, 0xf8e000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
               cpu->last_trap_return == 2u && cpu->w[15] == 0x5006u &&
               dspic33_read_word(cpu, 0x5000u) == 0u,
           "direct PUSH unimplemented file source completes stack state before trap");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xf8dfffu);
    expect_illegal_reset(state, cpu, "direct PUSH odd file address resets processor");
}

void dspic33_data_test_direct_stack_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;
    uint8_t pop;

    for (pop = 0u; pop < 2u; pop++) {
        for (fields = 0u; fields <= 0xffffu; fields += 257u) {
            uint32_t opcode = (pop != 0u ? 0xf90000u : 0xf80000u) | fields;
            if ((fields & 1u) == 0u) {
                run_direct_stack_encoding_case(state, cpu, opcode, (uint16_t)fields);
            } else {
                run_invalid_stack_encoding_case(state, cpu, opcode);
            }
        }
    }
}

void dspic33_data_test_link_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;

    for (fields = 0u; fields < 0x8000u; fields += 257u) {
        uint32_t opcode = 0xfa0000u | fields;
        if ((fields & 0x4001u) == 0u) {
            uint16_t frame_size = (uint16_t)fields;
            uint64_t cycles;
            bool matches;

            prepare_stack_encoding_case(cpu);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[14] == 0x5002u &&
                      cpu->w[15] == (uint16_t)(0x5002u + frame_size) &&
                      dspic33_read_word(cpu, 0x5000u) == 0x4444u && cpu->corcon == 0x0024u &&
                      cpu->sr == 0x010fu && !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
            expect_dsp_matrix_case(state, matches, opcode, "LNK legal encoding");
        } else {
            run_invalid_stack_encoding_case(state, cpu, opcode);
        }
    }
}

void dspic33_data_test_shadow_stack_encoding_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t legal_opcodes[] = {0xfe8000u, 0xfea000u, 0xfa8000u};
    size_t index;

    for (index = 0u; index < sizeof(legal_opcodes) / sizeof(legal_opcodes[0]); index++) {
        bool matches;

        reset_processor_test(cpu, 0u);
        if (legal_opcodes[index] == 0xfea000u) {
            cpu->w[0] = 0x1111u;
            cpu->w[1] = 0x2222u;
            cpu->w[2] = 0x3333u;
            cpu->w[3] = 0x4444u;
            cpu->sr = 0x01efu;
        } else if (legal_opcodes[index] == 0xfe8000u) {
            cpu->shadow_w[0] = 0x1111u;
            cpu->shadow_w[1] = 0x2222u;
            cpu->shadow_w[2] = 0x3333u;
            cpu->shadow_w[3] = 0x4444u;
            cpu->shadow_status = 0x010fu;
            cpu->sr = 0x00e0u;
        } else {
            cpu->corcon |= 0x0004u;
            dspic33_set_working_register(cpu, 14u, 0x5002u);
            dspic33_set_working_register(cpu, 15u, 0x5100u);
            dspic33_write_word(cpu, 0x5000u, 0x4444u);
        }
        matches = dspic33_load_program_word(cpu, 0u, legal_opcodes[index]) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u && cpu->cycles == 1u &&
                  !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
        if (legal_opcodes[index] == 0xfea000u) {
            matches = matches && cpu->shadow_w[0] == 0x1111u && cpu->shadow_w[1] == 0x2222u &&
                      cpu->shadow_w[2] == 0x3333u && cpu->shadow_w[3] == 0x4444u &&
                      cpu->shadow_status == 0x010fu && cpu->sr == 0x01efu;
        } else if (legal_opcodes[index] == 0xfe8000u) {
            matches = matches && cpu->w[0] == 0x1111u && cpu->w[1] == 0x2222u &&
                      cpu->w[2] == 0x3333u && cpu->w[3] == 0x4444u && cpu->sr == 0x01efu;
        } else {
            matches =
                matches && cpu->w[14] == 0x4444u && cpu->w[15] == 0x5000u && cpu->corcon == 0x0020u;
        }
        expect_dsp_matrix_case(state, matches, legal_opcodes[index],
                               "shadow stack and ULNK encoding");
    }
}

bool dspic33_data_test_binary_matrix_registers_match(const Dspic33* cpu,
                                                     const uint16_t registers[16]) {
    uint8_t reg;

    for (reg = 0u; reg < 16u; reg++) {
        if (cpu->w[reg] != registers[reg]) {
            return false;
        }
    }
    return true;
}

void dspic33_data_test_run_legal_binary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                                    BinaryMatrixOperation operation) {
    uint8_t left_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint16_t initial_status =
        dspic33_data_test_binary_matrix_logical(operation)
            ? (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) |
                         (((opcode >> 8u) & 1u) << 2u) | (((opcode >> 9u) & 1u) << 3u) |
                         (((opcode >> 10u) & 1u) << 8u))
            : (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u));
    uint16_t registers[16];
    BinaryMatrixOperand source;
    BinaryMatrixOperand destination;
    uint16_t left;
    uint16_t right;
    uint16_t value;
    uint16_t expected_status;
    uint16_t source_value = byte_mode ? (uint16_t)(0x0040u | (opcode & 0x003fu))
                                      : (uint16_t)(0x2100u | (opcode & 0x00ffu));
    uint64_t cycles;
    bool matches;

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    left = byte_mode ? (uint8_t)registers[left_register] : registers[left_register];
    if (source_mode >= 6u) {
        source.direct = false;
        source.address = 0u;
        right = (uint16_t)(opcode & 0x001fu);
    } else {
        source =
            dspic33_data_test_binary_matrix_operand(registers, source_mode, source_register, width);
        right = source.direct
                    ? (byte_mode ? (uint8_t)registers[source_register] : registers[source_register])
                    : source_value;
    }
    destination = dspic33_data_test_binary_matrix_operand(registers, destination_mode,
                                                          destination_register, width);
    if (!destination.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, destination.address, 0x5au);
        } else {
            dspic33_write_word(cpu, destination.address, 0x5a5au);
        }
    }
    if (source_mode < 6u && !source.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, source.address, (uint8_t)source_value);
        } else {
            dspic33_write_word(cpu, source.address, source_value);
        }
    }
    value =
        dspic33_data_test_binary_matrix_result(operation, left, right, initial_status, byte_mode);
    expected_status =
        dspic33_data_test_binary_matrix_status(operation, left, right, initial_status, byte_mode);
    if (destination.direct) {
        if (byte_mode) {
            value = (uint16_t)((registers[destination_register] & 0xff00u) | (value & 0x00ffu));
        }
        dspic33_data_test_binary_matrix_write_register(registers, destination_register, value);
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX &&
              dspic33_data_test_binary_matrix_registers_match(cpu, registers);
    if (!destination.direct) {
        matches =
            matches && (byte_mode ? dspic33_read_byte(cpu, destination.address) == (uint8_t)value
                                  : dspic33_read_word(cpu, destination.address) == value);
    }
    if (source_mode < 6u && !source.direct &&
        (destination.direct || destination.address != source.address)) {
        matches =
            matches && (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                                  : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal binary encoding");
}

static bool documented_bit_encoding_valid(uint32_t opcode) {
    uint8_t kind = (uint8_t)((opcode >> 16u) & 0x07u);
    bool file = (opcode & 0x080000u) != 0u;
    uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);

    if (file) {
        return kind != 5u && (kind != 4u || (opcode & 0x001ffeu) != 0x000042u);
    }
    if (mode >= 6u) {
        return false;
    }
    if (kind <= 2u) {
        bool byte_mode = (opcode & 0x000400u) != 0u;
        uint8_t bit = (uint8_t)((opcode >> 12u) & 0x0fu);
        return (opcode & 0x000b80u) == 0u && (!byte_mode || bit < 8u);
    }
    if (kind <= 5u) {
        return (opcode & 0x000780u) == 0u;
    }
    return (opcode & 0x000f80u) == 0u;
}

static void run_legal_register_bit_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint8_t kind = (uint8_t)((opcode >> 16u) & 0x07u);
    uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t reg = (uint8_t)(opcode & 0x0fu);
    bool byte_mode = kind <= 2u && (opcode & 0x000400u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint16_t initial_status = (uint16_t)(0x010cu | (opcode & 0x0003u));
    uint16_t registers[16];
    BinaryMatrixOperand operand;
    uint16_t value;
    uint16_t original;
    uint8_t bit;
    uint16_t mask;
    uint16_t expected_status = initial_status;
    uint64_t cycles;
    uint64_t expected_cycles = 1u;
    uint32_t expected_pc = 2u;
    bool matches;

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    bit = kind == 5u ? (uint8_t)(registers[(opcode >> 11u) & 0x0fu] & 0x0fu)
                     : (uint8_t)((opcode >> 12u) & 0x0fu);
    operand = dspic33_data_test_binary_matrix_operand(registers, mode, reg, width);
    original = byte_mode ? (uint8_t)(0x5au ^ opcode) : (uint16_t)(0x5aa5u ^ opcode);
    if (operand.direct) {
        original = byte_mode ? (uint8_t)registers[reg] : registers[reg];
    } else if (byte_mode) {
        dspic33_write_byte(cpu, operand.address, (uint8_t)original);
    } else {
        dspic33_write_word(cpu, operand.address, original);
    }
    value = original;
    mask = (uint16_t)(1u << bit);
    if (kind == 0u) {
        value |= mask;
    } else if (kind == 1u) {
        value &= (uint16_t)~mask;
    } else if (kind == 2u) {
        value ^= mask;
    } else if (kind == 3u || kind == 5u) {
        bool zero_destination = (opcode & (kind == 5u ? 0x008000u : 0x000800u)) != 0u;
        if (zero_destination) {
            expected_status =
                (uint16_t)((initial_status & ~0x0002u) | ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            expected_status =
                (uint16_t)((initial_status & ~0x0001u) | ((value & mask) != 0u ? 0x0001u : 0u));
        }
    } else if (kind == 4u) {
        bool zero_destination = (opcode & 0x000800u) != 0u;
        if (zero_destination) {
            expected_status =
                (uint16_t)((initial_status & ~0x0002u) | ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            expected_status =
                (uint16_t)((initial_status & ~0x0001u) | ((value & mask) != 0u ? 0x0001u : 0u));
        }
        value |= mask;
    } else {
        bool set = (value & mask) != 0u;
        if ((kind == 6u && set) || (kind == 7u && !set)) {
            expected_pc = 4u;
            expected_cycles = 2u;
        }
    }
    if (kind <= 2u || kind == 4u) {
        if (operand.direct) {
            if (byte_mode) {
                value = (uint16_t)((registers[reg] & 0xff00u) | (value & 0x00ffu));
            }
            dspic33_data_test_binary_matrix_write_register(registers, reg, value);
        }
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, 0u) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == expected_pc && cpu->cycles - cycles == expected_cycles &&
              cpu->sr == expected_status && cpu->corcon == 0x0020u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              dspic33_data_test_binary_matrix_registers_match(cpu, registers);
    if (!operand.direct) {
        uint16_t expected = kind <= 2u || kind == 4u ? value : original;
        matches =
            matches && (byte_mode ? dspic33_read_byte(cpu, operand.address) == (uint8_t)expected
                                  : dspic33_read_word(cpu, operand.address) == expected);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal register bit encoding");
}

static void run_legal_file_bit_admission_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    bool matches;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->pc = 0u;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, 0u) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal file bit encoding");
}

void dspic33_data_test_bit_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (fields = 0u; fields < 0x100000u; fields += 257u) {
        uint32_t opcode = 0xa00000u | fields;
        if (documented_bit_encoding_valid(opcode)) {
            if ((opcode & 0x080000u) != 0u) {
                run_legal_file_bit_admission_case(state, cpu, opcode);
            } else {
                run_legal_register_bit_case(state, cpu, opcode);
            }
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

void dspic33_data_test_direct_file_bit_value_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t kinds[] = {0u, 1u, 2u, 3u, 4u, 6u, 7u};
    static const uint16_t values[] = {0x0000u, 0xffffu, 0xa55au, 0x5aa5u};
    size_t kind_index;
    uint8_t bit;
    size_t value_index;

    for (kind_index = 0u; kind_index < sizeof(kinds) / sizeof(kinds[0]); kind_index++) {
        uint8_t kind = kinds[kind_index];
        for (bit = 0u; bit < 16u; bit++) {
            uint16_t address = (uint16_t)(0x1000u + (bit >> 3u));
            uint32_t opcode =
                0xa80000u | ((uint32_t)kind << 16u) | ((uint32_t)(bit & 7u) << 13u) | address;
            uint16_t mask = (uint16_t)(1u << bit);
            for (value_index = 0u; value_index < sizeof(values) / sizeof(values[0]);
                 value_index++) {
                uint16_t initial = values[value_index];
                uint16_t expected = initial;
                uint16_t initial_status = (uint16_t)(0x010du | (uint16_t)(value_index & 2u));
                uint16_t expected_status = initial_status;
                uint32_t expected_pc = 2u;
                uint64_t expected_cycles = 1u;
                bool matches;

                reset_processor_test(cpu, 0u);
                dspic33_write_word(cpu, 0x1000u, initial);
                cpu->sr = initial_status;
                if (kind == 0u) {
                    expected |= mask;
                } else if (kind == 1u) {
                    expected &= (uint16_t)~mask;
                } else if (kind == 2u) {
                    expected ^= mask;
                } else if (kind == 3u) {
                    expected_status = (uint16_t)((initial_status & ~0x0002u) |
                                                 ((initial & mask) == 0u ? 0x0002u : 0u));
                } else if (kind == 4u) {
                    expected_status = (uint16_t)((initial_status & ~0x0002u) |
                                                 ((initial & mask) == 0u ? 0x0002u : 0u));
                    expected |= mask;
                } else {
                    bool set = (initial & mask) != 0u;
                    if ((kind == 6u && set) || (kind == 7u && !set)) {
                        expected_pc = 4u;
                        expected_cycles = 2u;
                    }
                }
                matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                          dspic33_load_program_word(cpu, 2u, 0u) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
                          cpu->cycles == expected_cycles && cpu->sr == expected_status &&
                          dspic33_read_word(cpu, 0x1000u) == expected && !cpu->illegal_reset &&
                          cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode, "direct file bit value and status");
            }
        }
    }
}

void dspic33_data_test_bit_operand_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xa40012u);
    dspic33_set_working_register(cpu, 2u, 0x0042u);
    expect_illegal_reset(state, cpu, "indirect BTSTS targeting SR resets processor");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xa40012u);
    cpu->w[2] = 0x1000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu, "BTSTS uninitialized source pointer resets processor");
}

static bool documented_table_encoding_valid(uint32_t opcode) {
    bool write = (opcode & 0x010000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);

    return write ? source_mode < 6u && destination_mode >= 1u && destination_mode < 6u
                 : source_mode >= 1u && source_mode < 6u && destination_mode < 6u;
}

static void prepare_table_encoding_case(Dspic33* cpu, bool write) {
    uint8_t reg;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->pc = 0u;
    cpu->tblpag = write ? 0x00fau : 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    for (reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg, 0x5000u);
    }
    dspic33_write_word(cpu, 0x4ffeu, 0xa55au);
    dspic33_write_word(cpu, 0x5000u, 0xa55au);
    dspic33_write_word(cpu, 0x5002u, 0xa55au);
    dspic33_load_program_word(cpu, 0x4ffeu, 0x12ab56u);
    dspic33_load_program_word(cpu, 0x5000u, 0x12ab56u);
    dspic33_load_program_word(cpu, 0x5002u, 0x12ab56u);
}

static void run_legal_table_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    bool write = (opcode & 0x010000u) != 0u;
    bool matches;

    prepare_table_encoding_case(cpu, write);
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal table encoding");
}

void dspic33_data_test_table_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;

    for (fields = 0u; fields < 0x20000u; fields += 257u) {
        uint32_t opcode = 0xba0000u | fields;
        if (documented_table_encoding_valid(opcode)) {
            run_legal_table_encoding_case(state, cpu, opcode);
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

void dspic33_data_test_table_value_cases(TestState* state, Dspic33* cpu) {
    uint8_t high;
    uint8_t byte_mode;
    uint8_t odd;

    for (high = 0u; high < 2u; high++) {
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            for (odd = 0u; odd < 2u; odd++) {
                uint32_t opcode = 0xba0000u | ((uint32_t)high << 15u) |
                                  ((uint32_t)byte_mode << 14u) | ((uint32_t)3u << 7u) | 0x0012u;
                uint16_t expected = high != 0u ? 0x0012u : 0xab56u;
                bool matches;

                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                load_instruction(state, cpu, 0x0200u, 0x12ab56u);
                cpu->tblpag = 0u;
                dspic33_set_working_register(cpu, 2u, (uint16_t)(0x0200u + odd));
                dspic33_set_working_register(cpu, 3u, 0xa500u);
                if (byte_mode != 0u) {
                    expected =
                        high != 0u ? (odd != 0u ? 0u : 0x0012u) : (odd != 0u ? 0x00abu : 0x0056u);
                    expected |= 0xa500u;
                }
                matches = dspic33_step(cpu) == DSPIC33_RUNNING &&
                          cpu->w[2] == (uint16_t)(0x0200u + odd) && cpu->w[3] == expected &&
                          cpu->sr == 0u && cpu->cycles == 5u && !cpu->illegal_reset;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "table read value and byte selection");

                opcode = 0xbb0000u | ((uint32_t)high << 15u) | ((uint32_t)byte_mode << 14u) |
                         ((uint32_t)1u << 11u) | ((uint32_t)3u << 7u) | 2u;
                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                cpu->tblpag = 0x00fau;
                dspic33_set_working_register(cpu, 2u, 0xa5c3u);
                dspic33_set_working_register(cpu, 3u, odd);
                matches = dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
                          !cpu->illegal_reset;
                expected = 0xffffu;
                if (high != 0u) {
                    matches = matches && ((cpu->write_latches[0] >> 16u) & 0xffu) ==
                                             (byte_mode != 0u && odd != 0u ? 0xffu : 0xc3u);
                } else {
                    expected = byte_mode == 0u ? 0xa5c3u : odd != 0u ? 0xc3ffu : 0xffc3u;
                    matches = matches && (cpu->write_latches[0] & 0xffffu) == expected;
                }
                expect_dsp_matrix_case(state, matches, opcode,
                                       "table write latch and byte selection");
            }
        }
    }
}

static void table_pointer_boundary_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        uint16_t width = byte_mode != 0u ? 1u : 2u;
        for (uint8_t mode = 2u; mode <= 5u; mode++) {
            bool decrement = mode == 2u || mode == 4u;
            bool pre_modify = mode >= 4u;
            for (uint8_t wrap = 0u; wrap < 2u; wrap++) {
                uint16_t initial;
                uint16_t expected_pointer;
                uint16_t effective;
                uint16_t expected_value;
                uint32_t opcode;
                bool latches_unchanged;

                if (decrement) {
                    initial = wrap != 0u ? 0x8000u : (uint16_t)(0x8000u + width);
                    expected_pointer = wrap != 0u ? (uint16_t)(UINT16_MAX - width + 1u) : 0x8000u;
                } else {
                    initial = wrap != 0u ? (uint16_t)(UINT16_MAX - width + 1u)
                                         : (uint16_t)(0x8000u - width);
                    expected_pointer = 0x8000u;
                }
                effective = pre_modify ? expected_pointer : initial;
                expected_value =
                    byte_mode != 0u
                        ? (uint16_t)(0xa500u | ((effective & 1u) != 0u ? 0x00abu : 0x0056u))
                        : 0xab56u;

                opcode = 0xba0000u | ((uint32_t)byte_mode << 14u) | ((uint32_t)3u << 7u) |
                         ((uint32_t)mode << 4u) | 2u;
                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                load_instruction(state, cpu, (uint32_t)(effective & 0xfffeu), 0x12ab56u);
                cpu->tblpag = 0u;
                dspic33_set_working_register(cpu, 2u, initial);
                dspic33_set_working_register(cpu, 3u, 0xa500u);
                expect(state,
                       dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u && cpu->cycles == 5u &&
                           cpu->w[2] == expected_pointer && cpu->w[3] == expected_value &&
                           !cpu->illegal_reset,
                       "table read preserves high-half pointer wrap and boundary transitions");

                opcode = 0xbb0000u | ((uint32_t)byte_mode << 14u) | ((uint32_t)mode << 11u) |
                         ((uint32_t)3u << 7u) | 2u;
                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                cpu->tblpag = 0x00fau;
                dspic33_set_working_register(cpu, 2u, 0xa5c3u);
                dspic33_set_working_register(cpu, 3u, initial);
                for (size_t index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
                    cpu->write_latches[index] = 0x0055aa33u;
                }
                expect(state,
                       dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u && cpu->cycles == 2u &&
                           cpu->w[2] == 0xa5c3u && cpu->w[3] == expected_pointer &&
                           !cpu->illegal_reset,
                       "table write preserves high-half pointer wrap and boundary transitions");
                latches_unchanged = true;
                for (size_t index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
                    latches_unchanged =
                        latches_unchanged && cpu->write_latches[index] == 0x0055aa33u;
                }
                expect(state, latches_unchanged,
                       "out-of-row table write does not alias the write-latch window");
            }
        }
    }
}

void dspic33_data_test_table_operand_lifecycle_cases(TestState* state, Dspic33* cpu) {
    table_pointer_boundary_cases(state, cpu);
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xbb0982u);
    dspic33_set_working_register(cpu, 2u, 0xa55au);
    cpu->w[3] = 0x5000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0008u;
    expect_illegal_reset(state, cpu, "table write uninitialized destination resets processor");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xbb0992u);
    cpu->w[2] = 0x5000u;
    dspic33_set_working_register(cpu, 3u, 0u);
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu, "table write uninitialized source resets processor");
}

bool dspic33_data_test_documented_system_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);

    if (family == 0xfcu) {
        return (opcode & 0x00c000u) == 0u;
    }
    if (family != 0xfeu) {
        return true;
    }
    if (opcode == 0xfe0000u || opcode == 0xfe2000u || (opcode & 0xfffffeu) == 0xfe4000u ||
        opcode == 0xfe6000u || opcode == 0xfe8000u || opcode == 0xfea000u) {
        return true;
    }
    if ((opcode & 0xfff000u) == 0xfec000u) {
        return ((opcode >> 10u) & 3u) != 3u;
    }
    return (opcode & 0xfff000u) == 0xfed000u && (opcode & 0x0003f0u) == 0u &&
           ((opcode >> 10u) & 3u) != 3u;
}
