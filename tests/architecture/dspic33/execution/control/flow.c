#include "architecture/dspic33/execution/control/internal.h"

void dspic33_control_test_special_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t families[2] = {0xc30000u, 0xc70000u};
    uint8_t family;
    uint8_t accumulator;
    uint8_t write_back;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t x_destination;
    uint8_t y_destination;

    for (family = 0u; family < 2u; family++) {
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (write_back = 0u; write_back < 3u; write_back++) {
                for (x_operation = 0u; x_operation < 16u; x_operation++) {
                    for (y_operation = 0u; y_operation < 16u; y_operation++) {
                        for (x_destination = 0u; x_destination < 4u; x_destination++) {
                            for (y_destination = 0u; y_destination < 4u; y_destination++) {
                                uint32_t opcode = families[family] |
                                                  ((uint32_t)accumulator << 15u) |
                                                  ((uint32_t)x_destination << 12u) |
                                                  ((uint32_t)y_destination << 10u) |
                                                  ((uint32_t)x_operation << 6u) |
                                                  ((uint32_t)y_operation << 2u) | write_back;
                                dspic33_control_test_run_legal_dsp_matrix_case(
                                    state, cpu, opcode, accumulator, family == 0u ? 0 : 100,
                                    x_operation, y_operation, (uint8_t)(4u + x_destination),
                                    (uint8_t)(4u + y_destination), write_back, -1);
                            }
                        }
                    }
                }
            }
        }
    }
}

void dspic33_control_test_square_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const int64_t operands[4] = {2, 3, 5, 7};
    uint8_t source;
    uint8_t accumulator;
    uint8_t replace;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t x_destination;
    uint8_t y_destination;

    for (source = 0u; source < 4u; source++) {
        int64_t product = operands[source] * operands[source];
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (replace = 0u; replace < 2u; replace++) {
                int64_t result = replace != 0u ? product : 100 + product;
                for (x_operation = 0u; x_operation < 16u; x_operation++) {
                    for (y_operation = 0u; y_operation < 16u; y_operation++) {
                        for (x_destination = 0u; x_destination < 4u; x_destination++) {
                            for (y_destination = 0u; y_destination < 4u; y_destination++) {
                                uint32_t opcode = 0xf00000u | ((uint32_t)source << 16u) |
                                                  ((uint32_t)accumulator << 15u) |
                                                  ((uint32_t)x_destination << 12u) |
                                                  ((uint32_t)y_destination << 10u) |
                                                  ((uint32_t)x_operation << 6u) |
                                                  ((uint32_t)y_operation << 2u) | replace;
                                dspic33_control_test_run_legal_dsp_matrix_case(
                                    state, cpu, opcode, accumulator, result, x_operation,
                                    y_operation, (uint8_t)(4u + x_destination),
                                    (uint8_t)(4u + y_destination), DSP_MATRIX_WRITE_BACK_NONE, -1);
                            }
                        }
                    }
                }
            }
        }
    }
}

void dspic33_control_test_euclidean_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const int64_t operands[4] = {2, 3, 5, 7};
    uint8_t source;
    uint8_t accumulator;
    uint8_t operation;
    uint8_t destination;
    uint8_t x_operation;
    uint8_t y_operation;

    for (source = 0u; source < 4u; source++) {
        int64_t product = operands[source] * operands[source];
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (operation = 2u; operation < 4u; operation++) {
                int64_t result = operation == 2u ? 100 + product : product;
                for (destination = 0u; destination < 4u; destination++) {
                    for (x_operation = 0u; x_operation < 16u; x_operation++) {
                        if (x_operation == 4u) {
                            continue;
                        }
                        for (y_operation = x_operation; y_operation <= x_operation; y_operation++) {
                            uint32_t opcode;
                            if (y_operation == 4u) {
                                continue;
                            }
                            opcode = 0xf04000u | ((uint32_t)source << 16u) |
                                     ((uint32_t)accumulator << 15u) |
                                     ((uint32_t)destination << 12u) |
                                     ((uint32_t)x_operation << 6u) | ((uint32_t)y_operation << 2u) |
                                     operation;
                            dspic33_control_test_run_legal_dsp_matrix_case(
                                state, cpu, opcode, accumulator, result, x_operation, y_operation,
                                4u, 4u, DSP_MATRIX_WRITE_BACK_NONE, (int8_t)(4u + destination));
                        }
                    }
                }
            }
        }
    }
}

static void prepare_invalid_dsp_matrix_case(Dspic33* cpu) {
    uint8_t reg;
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0001u;
    cpu->accumulator[0] = 0x123456789a;
    cpu->accumulator[1] = -0x123456789a;
    cpu->illegal_reset = false;
    cpu->last_trap = UINT16_MAX;
    cpu->stop_reason = DSPIC33_RUNNING;
    for (reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(cpu, reg, (uint16_t)(0x5000u + (uint16_t)reg * 2u));
    }
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
}

static void run_invalid_dsp_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;
    uint8_t reg;

    prepare_invalid_dsp_matrix_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->illegal_reset && cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->software_reset_count == 0u && cpu->trap_count == 0u &&
              cpu->last_trap == UINT16_MAX && cpu->pc == 0u && cpu->w[15] == 0x1000u &&
              cpu->initialized_working_registers == 0x8000u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    for (reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "illegal DSP encoding");
}

void dspic33_control_test_invalid_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t pair;
    uint8_t accumulator;
    uint8_t alternate;
    uint8_t x_destination;
    uint8_t y_destination;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t low;

    for (pair = 0u; pair < 8u; pair++) {
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (alternate = 0u; alternate < 2u; alternate++) {
                for (x_destination = 0u; x_destination < 4u; x_destination++) {
                    for (y_destination = 0u; y_destination < 4u; y_destination++) {
                        for (x_operation = 0u; x_operation < 16u; x_operation++) {
                            for (y_operation = x_operation; y_operation <= x_operation;
                                 y_operation++) {
                                for (low = 0u; low < 4u; low++) {
                                    bool valid = (pair != 3u && pair != 7u) ||
                                                 (alternate == 0u && low != 3u);
                                    uint32_t opcode;
                                    if (valid) {
                                        continue;
                                    }
                                    opcode = 0xc00000u | ((uint32_t)pair << 16u) |
                                             ((uint32_t)accumulator << 15u) |
                                             ((uint32_t)alternate << 14u) |
                                             ((uint32_t)x_destination << 12u) |
                                             ((uint32_t)y_destination << 10u) |
                                             ((uint32_t)x_operation << 6u) |
                                             ((uint32_t)y_operation << 2u) | low;
                                    run_invalid_dsp_matrix_case(state, cpu, opcode);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (pair = 0u; pair < 4u; pair++) {
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (alternate = 0u; alternate < 2u; alternate++) {
                for (x_destination = 0u; x_destination < 4u; x_destination++) {
                    for (y_destination = 0u; y_destination < 4u; y_destination++) {
                        for (x_operation = 0u; x_operation < 16u; x_operation++) {
                            for (y_operation = x_operation; y_operation <= x_operation;
                                 y_operation++) {
                                for (low = 0u; low < 4u; low++) {
                                    bool valid = alternate == 0u
                                                     ? low < 2u
                                                     : low >= 2u && y_destination == 0u &&
                                                           x_operation != 4u && y_operation != 4u;
                                    uint32_t opcode;
                                    if (valid) {
                                        continue;
                                    }
                                    opcode = 0xf00000u | ((uint32_t)pair << 16u) |
                                             ((uint32_t)accumulator << 15u) |
                                             ((uint32_t)alternate << 14u) |
                                             ((uint32_t)x_destination << 12u) |
                                             ((uint32_t)y_destination << 10u) |
                                             ((uint32_t)x_operation << 6u) |
                                             ((uint32_t)y_operation << 2u) | low;
                                    run_invalid_dsp_matrix_case(state, cpu, opcode);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static int64_t generic_multiply_operand(uint16_t value, bool signed_value) {
    return signed_value ? (int16_t)value : value;
}

static uint16_t generic_multiply_source_value(uint8_t mode) {
    static const uint16_t values[6] = {
        0u, 0x8003u, 0x8003u, 0x8003u, 0x8005u, 0x8007u,
    };
    return values[mode];
}

static void prepare_generic_multiply_case(Dspic33* cpu, uint8_t source_mode,
                                          uint8_t source_register, uint16_t expected_w[16]) {
    uint8_t reg;
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0005u;
    cpu->accumulator[0] = 0x1111222233;
    cpu->accumulator[1] = -0x1111222233;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value = (uint16_t)(0x8000u + (uint16_t)reg * 2u);
        dspic33_set_working_register(cpu, reg, value);
        expected_w[reg] = cpu->w[reg];
    }
    if (source_mode != 0u && source_mode < 6u) {
        dspic33_set_working_register(cpu, source_register, 0x5008u);
        expected_w[source_register] = 0x5008u;
        dspic33_write_word(cpu, 0x5006u, 0x8005u);
        dspic33_write_word(cpu, 0x5008u, 0x8003u);
        dspic33_write_word(cpu, 0x500au, 0x8007u);
    }
}

static void run_legal_generic_multiply_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                            bool base_signed, bool source_signed,
                                            uint8_t base_register, uint8_t destination,
                                            uint8_t source_mode, uint8_t source_register) {
    uint16_t expected_w[16] = {0u};
    int64_t expected_accumulator[2] = {0x1111222233, -0x1111222233};
    uint16_t source;
    int64_t product;
    uint64_t cycles;
    bool matches;
    uint8_t reg;

    prepare_generic_multiply_case(cpu, source_mode, source_register, expected_w);
    source = source_mode >= 6u   ? (uint16_t)(opcode & 0x001fu)
             : source_mode == 0u ? expected_w[source_register]
                                 : generic_multiply_source_value(source_mode);
    if (source_mode == 2u || source_mode == 4u) {
        expected_w[source_register] = 0x5006u;
    } else if (source_mode == 3u || source_mode == 5u) {
        expected_w[source_register] = 0x500au;
    }
    product = generic_multiply_operand(expected_w[base_register], base_signed) *
              generic_multiply_operand(source, source_signed);
    if (destination >= 14u) {
        expected_accumulator[destination & 1u] = product;
    } else {
        uint8_t result_register = (uint8_t)(destination & 0x0eu);
        expected_w[result_register] = (uint16_t)product;
        if ((destination & 1u) == 0u) {
            expected_w[result_register + 1u] = (uint16_t)((uint32_t)product >> 16u);
        }
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u &&
              cpu->accumulator[0] == expected_accumulator[0] &&
              cpu->accumulator[1] == expected_accumulator[1] && cpu->sr == 0x010fu &&
              cpu->corcon == 0x0005u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX;
    for (reg = 0u; reg < 16u; reg++) {
        matches = matches && cpu->w[reg] == expected_w[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal generic multiply encoding");
}

void dspic33_control_test_generic_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t base_signed;
    uint8_t source_signed;
    uint8_t base_register;
    uint8_t destination;
    uint8_t source_mode;
    uint8_t source_register;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (base_signed = 0u; base_signed < 2u; base_signed++) {
        for (source_signed = 0u; source_signed < 2u; source_signed++) {
            for (base_register = 0u; base_register < 16u; base_register += 5u) {
                for (destination = 0u; destination < 16u; destination += 5u) {
                    for (source_mode = 0u; source_mode < 8u; source_mode++) {
                        for (source_register = 0u; source_register < 16u; source_register += 5u) {
                            uint32_t opcode = 0xb80000u | ((uint32_t)base_signed << 16u) |
                                              ((uint32_t)source_signed << 15u) |
                                              ((uint32_t)base_register << 11u) |
                                              ((uint32_t)destination << 7u) |
                                              ((uint32_t)source_mode << 4u) | source_register;
                            if (source_signed != 0u && source_mode >= 6u) {
                                run_invalid_dsp_matrix_case(state, cpu, opcode);
                            } else {
                                run_legal_generic_multiply_case(
                                    state, cpu, opcode, base_signed != 0u, source_signed != 0u,
                                    base_register, destination, source_mode, source_register);
                            }
                        }
                    }
                }
            }
        }
    }
}

void dspic33_control_test_file_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t byte_mode;
    uint16_t address;

    expect(state, dspic33_load_program_word(cpu, 0x000006u, 0x000340u),
           "load file multiply address-error vector");
    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (address = 0u; address < 0x2000u; address += 31u) {
            uint32_t opcode = 0xbc0000u | ((uint32_t)byte_mode << 14u) | address;
            bool matches;
            dspic33_reset(cpu, 0u);
            cpu->stop_on_trap = true;
            dspic33_set_working_register(cpu, 0u, 0u);
            dspic33_set_working_register(cpu, 2u, 0xa5a5u);
            dspic33_set_working_register(cpu, 3u, 0x5a5au);
            dspic33_set_working_register(cpu, 15u, 0x5000u);
            cpu->sr = 0x010fu;
            matches = dspic33_load_program_word(cpu, 0u, opcode);
            if (byte_mode == 0u && (address & 1u) != 0u) {
                matches = matches && dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
                          cpu->last_trap_return == 2u && cpu->pc == 0x000340u &&
                          (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u;
            } else {
                matches = matches && dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->w[2] == 0u && cpu->w[3] == (byte_mode != 0u ? 0x5a5au : 0u) &&
                          cpu->sr == 0x010fu && cpu->last_trap == UINT16_MAX &&
                          cpu->unsupported_opcode == 0u;
            }
            expect_dsp_matrix_case(state, matches, opcode, "file multiply encoding");
        }
    }
}

void dspic33_control_test_prepare_move_matrix_case(Dspic33* cpu) {
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0005u;
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
}

void dspic33_control_test_run_invalid_move_matrix_case(TestState* state, Dspic33* cpu,
                                                       uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;
    uint8_t reg;

    prepare_invalid_dsp_matrix_case(cpu);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->illegal_reset && cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->software_reset_count == 0u && cpu->trap_count == 0u &&
              cpu->last_trap == UINT16_MAX && cpu->pc == 0u && cpu->w[15] == 0x1000u &&
              cpu->initialized_working_registers == 0x8000u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    for (reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "illegal move encoding");
}

void dspic33_control_test_move_literal_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t literal;
    uint8_t destination;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (literal = 0u; literal <= UINT16_MAX; literal += 257u) {
        for (destination = 0u; destination < 16u; destination++) {
            uint32_t opcode = 0x200000u | (literal << 4u) | (uint32_t)destination;
            uint16_t expected =
                destination == 15u ? (uint16_t)(literal & 0xfffeu) : (uint16_t)literal;
            uint64_t cycles;
            bool matches;

            dspic33_control_test_prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, destination, 0x5a5au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                      cpu->last_trap == UINT16_MAX;
            expect_dsp_matrix_case(state, matches, opcode, "MOV literal encoding");
        }
    }

    for (literal = 0u; literal <= UINT8_MAX; literal++) {
        for (destination = 0u; destination < 16u; destination++) {
            uint32_t opcode = 0xb3c000u | (literal << 4u) | (uint32_t)destination;
            uint16_t expected = (uint16_t)(0x5a00u | literal);
            uint64_t cycles;
            bool matches;
            if (destination == 15u) {
                expected &= 0xfffeu;
            }
            dspic33_control_test_prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, destination, 0x5a5au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                      cpu->last_trap == UINT16_MAX;
            expect_dsp_matrix_case(state, matches, opcode, "MOV byte literal encoding");
        }
    }
}

void dspic33_control_test_move_register_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xfd0000u; opcode <= 0xfd0fffu; opcode++) {
        bool legal = (opcode & 0xfff870u) == 0xfd0000u;
        if (!legal) {
            dspic33_control_test_run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t source = (uint8_t)(opcode & 0x0fu);
            uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
            uint16_t source_value = (uint16_t)(0x1100u | source);
            uint16_t destination_value = (uint16_t)(0x2200u | destination);
            uint16_t expected_source;
            uint16_t expected_destination;
            uint64_t cycles;
            bool matches;

            dspic33_control_test_prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, source, source_value);
            dspic33_set_working_register(cpu, destination, destination_value);
            if (source == destination) {
                dspic33_set_working_register(cpu, source, source_value);
            }
            expected_destination = cpu->w[source];
            expected_source = cpu->w[destination];
            if (source == 15u) {
                expected_source &= 0xfffeu;
            }
            if (destination == 15u) {
                expected_destination &= 0xfffeu;
            }
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[source] == expected_source &&
                      cpu->w[destination] == expected_destination && cpu->sr == 0x010fu &&
                      cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
            expect_dsp_matrix_case(state, matches, opcode, "EXCH encoding");
        }
    }

    for (opcode = 0xfd8000u; opcode <= 0xfdffffu; opcode++) {
        bool legal = (opcode & 0xffbff0u) == 0xfd8000u;
        if (!legal) {
            dspic33_control_test_run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t reg = (uint8_t)(opcode & 0x0fu);
            bool byte_mode = (opcode & 0x004000u) != 0u;
            uint16_t expected = byte_mode ? 0xa5a5u : 0x5aa5u;
            uint64_t cycles;
            bool matches;
            if (reg == 15u) {
                expected &= 0xfffeu;
            }
            dspic33_control_test_prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, reg, 0xa55au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[reg] == expected && cpu->sr == 0x010fu &&
                      cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
            expect_dsp_matrix_case(state, matches, opcode, "SWAP encoding");
        }
    }
}

void dspic33_control_test_movpag_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xfec000u; opcode <= 0xfecfffu; opcode++) {
        uint8_t page_register = (uint8_t)((opcode >> 10u) & 3u);
        uint16_t literal = (uint16_t)(opcode & 0x03ffu);
        if (page_register == 3u) {
            dspic33_control_test_run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint16_t expected = page_register == 0u   ? literal
                                : page_register == 1u ? (uint16_t)(literal & 0x01ffu)
                                                      : (uint16_t)(literal & 0x00ffu);
            uint64_t cycles;
            bool matches;
            dspic33_control_test_prepare_move_matrix_case(cpu);
            cpu->dsrpag = 0x0155u;
            cpu->dswpag = 0x00aau;
            cpu->tblpag = 0x005au;
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            if (page_register == 0u) {
                matches = matches && cpu->dsrpag == expected;
            } else if (page_register == 1u) {
                matches = matches && cpu->dswpag == expected;
            } else {
                matches = matches && cpu->tblpag == expected;
            }
            expect_dsp_matrix_case(state, matches, opcode, "MOVPAG literal encoding");
        }
    }

    for (opcode = 0xfed000u; opcode <= 0xfedfffu; opcode++) {
        bool fields_valid = (opcode & 0x0003f0u) == 0u;
        uint8_t page_register = (uint8_t)((opcode >> 10u) & 3u);
        if (!fields_valid || page_register == 3u) {
            dspic33_control_test_run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t source = (uint8_t)(opcode & 0x0fu);
            uint16_t value = (uint16_t)(0x03a0u | source);
            uint16_t expected;
            uint64_t cycles;
            bool matches;
            dspic33_control_test_prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, source, value);
            value = cpu->w[source];
            expected = page_register == 0u   ? (uint16_t)(value & 0x03ffu)
                       : page_register == 1u ? (uint16_t)(value & 0x01ffu)
                                             : (uint16_t)(value & 0x00ffu);
            cpu->dsrpag = 0x0155u;
            cpu->dswpag = 0x00aau;
            cpu->tblpag = 0x005au;
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            if (page_register == 0u) {
                matches = matches && cpu->dsrpag == expected;
            } else if (page_register == 1u) {
                matches = matches && cpu->dswpag == expected;
            } else {
                matches = matches && cpu->tblpag == expected;
            }
            expect_dsp_matrix_case(state, matches, opcode, "MOVPAG register encoding");
        }
    }
}

MoveMatrixOperand dspic33_control_test_resolve_move_matrix_operand(uint16_t registers[16],
                                                                   uint8_t mode, uint8_t reg,
                                                                   uint8_t offset_reg,
                                                                   uint8_t width) {
    MoveMatrixOperand operand = {0u, mode == 0u};
    int32_t adjusted;
    if (mode == 0u) {
        return operand;
    }
    if (mode == 1u) {
        operand.address = registers[reg];
        return operand;
    }
    if (mode == 2u || mode == 3u) {
        operand.address = registers[reg];
        adjusted = (int32_t)registers[reg] + (mode == 3u ? (int32_t)width : -(int32_t)width);
        registers[reg] = reg == 15u ? (uint16_t)adjusted & 0xfffeu : (uint16_t)adjusted;
        return operand;
    }
    if (mode == 4u || mode == 5u) {
        adjusted = (int32_t)registers[reg] + (mode == 5u ? (int32_t)width : -(int32_t)width);
        operand.address = (uint16_t)adjusted;
        registers[reg] = reg == 15u ? (uint16_t)adjusted & 0xfffeu : (uint16_t)adjusted;
        return operand;
    }
    operand.address = (uint16_t)(registers[reg] + registers[offset_reg]);
    return operand;
}

void dspic33_control_test_prepare_move_registers(Dspic33* cpu, uint16_t expected[16], uint16_t base,
                                                 uint16_t stride) {
    uint8_t reg;
    dspic33_control_test_prepare_move_matrix_case(cpu);
    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value = (uint16_t)(base + (uint16_t)reg * stride);
        dspic33_set_working_register(cpu, reg, value);
        expected[reg] = cpu->w[reg];
    }
}
