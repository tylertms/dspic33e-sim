#include "architecture/dspic33/execution/data/internal.h"

static void prepare_system_encoding_case(Dspic33* cpu) {
    reset_processor_test(cpu, 0x0200u);
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    dspic33_set_working_register(cpu, 15u, 0x5000u);
}

static void run_legal_system_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    bool matches;

    prepare_system_encoding_case(cpu);
    matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal system encoding");
}

static void run_illegal_system_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    prepare_system_encoding_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->unsupported_opcode == 0u;
    expect_dsp_matrix_case(state, matches, opcode, "illegal system encoding");
}

void dspic33_data_test_system_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;

    for (fields = 0u; fields <= 0xffffu; fields += 257u) {
        run_legal_system_encoding_case(state, cpu, fields);
        run_legal_system_encoding_case(state, cpu, 0xff0000u | fields);
    }
    for (fields = 0u; fields <= 0xffffu; fields += 257u) {
        uint32_t opcode = 0xfc0000u | fields;
        if (dspic33_data_test_documented_system_encoding_valid(opcode)) {
            run_legal_system_encoding_case(state, cpu, opcode);
        } else {
            run_illegal_system_encoding_case(state, cpu, opcode);
        }
    }
    for (fields = 0u; fields <= 0xffffu; fields += 257u) {
        uint32_t opcode = 0xfe0000u | fields;
        if (dspic33_data_test_documented_system_encoding_valid(opcode)) {
            run_legal_system_encoding_case(state, cpu, opcode);
        } else {
            run_illegal_system_encoding_case(state, cpu, opcode);
        }
    }
}

void dspic33_data_test_system_control_value_cases(TestState* state, Dspic33* cpu) {
    uint32_t literal;

    for (literal = 0u; literal < 0x4000u; literal += 257u) {
        uint32_t opcode = 0xfc0000u | literal;
        uint64_t cycles;
        bool matches;

        prepare_system_encoding_case(cpu);
        cpu->disicnt = 0u;
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u &&
                  cpu->cycles - cycles == 1u && cpu->disicnt == (literal == 0u ? 0u : literal) &&
                  cpu->sr == 0x010fu && !cpu->illegal_reset;
        expect_dsp_matrix_case(state, matches, opcode, "DISI literal value");
    }

    prepare_system_encoding_case(cpu);
    expect(state,
           dspic33_load_program_word(cpu, 0x0200u, 0xfe2000u) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u && cpu->cycles == 1u &&
               cpu->sr == 0x010fu && cpu->corcon == 0x0020u,
           "BOOTSWP executes as NOP when dual boot is unavailable");

    prepare_system_encoding_case(cpu);
    cpu->watchdog.ticks = 123u;
    cpu->configuration[10u] |= 0x40u;
    expect(state,
           dspic33_load_program_word(cpu, 0x0200u, 0xfe6000u) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->watchdog.ticks == 0u &&
               cpu->sr == 0x010fu,
           "CLRWDT clears watchdog state and preserves status");
}

static bool documented_divide_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    uint8_t divisor = (uint8_t)(opcode & 0x0fu);

    if (family == 0xd9u) {
        return (opcode & 0x0087f0u) == 0u;
    }
    if (divisor < 2u) {
        return false;
    }
    if (family != 0xd8u || (opcode & 0x000030u) != 0u) {
        return false;
    }
    uint8_t high = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t low = (uint8_t)((opcode >> 7u) & 0x0fu);
    if ((opcode & 0x000040u) == 0u) {
        return high == 0u;
    }
    return (low & 1u) == 0u && high == low + 1u;
}

static bool run_legal_divide_matrix_case(Dspic33* cpu, uint32_t opcode) {
    bool unsigned_divide = (opcode & 0x008000u) != 0u;
    bool double_word = (opcode & 0x000040u) != 0u;
    bool fractional = (opcode & 0xff0000u) == 0xd90000u;
    uint8_t high = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t low = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t divisor_register = (uint8_t)(opcode & 0x0fu);
    int64_t quotient;
    int64_t remainder;
    bool overflow;
    uint8_t reg;

    reset_processor_test(cpu, 0x0200u);
    for (reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg, (uint16_t)(0x0100u + reg));
    }
    if (fractional) {
        dspic33_set_working_register(cpu, high, 0x1000u);
        dspic33_set_working_register(cpu, divisor_register, 0x4000u);
        quotient = (int64_t)(int16_t)cpu->w[high] * 32768 / (int16_t)cpu->w[divisor_register];
        remainder = (int64_t)(int16_t)cpu->w[high] * 32768 % (int16_t)cpu->w[divisor_register];
        overflow = quotient < INT16_MIN || quotient > INT16_MAX;
    } else if (double_word) {
        dspic33_set_working_register(cpu, low, 0x1234u);
        dspic33_set_working_register(cpu, high, 0u);
        dspic33_set_working_register(cpu, divisor_register, 17u);
        if (unsigned_divide) {
            uint32_t dividend = ((uint32_t)cpu->w[high] << 16u) | cpu->w[low];
            quotient = dividend / cpu->w[divisor_register];
            remainder = dividend % cpu->w[divisor_register];
            overflow = quotient > UINT16_MAX;
        } else {
            int32_t dividend = (int32_t)(((uint32_t)cpu->w[high] << 16u) | cpu->w[low]);
            quotient = (int64_t)dividend / (int16_t)cpu->w[divisor_register];
            remainder = (int64_t)dividend % (int16_t)cpu->w[divisor_register];
            overflow = quotient < INT16_MIN || quotient > INT16_MAX;
        }
    } else {
        dspic33_set_working_register(cpu, low, 0x1234u);
        dspic33_set_working_register(cpu, divisor_register, 17u);
        if (unsigned_divide) {
            quotient = cpu->w[low] / cpu->w[divisor_register];
            remainder = cpu->w[low] % cpu->w[divisor_register];
        } else {
            quotient = (int16_t)cpu->w[low] / (int16_t)cpu->w[divisor_register];
            remainder = (int16_t)cpu->w[low] % (int16_t)cpu->w[divisor_register];
        }
        overflow = quotient < INT16_MIN || quotient > UINT16_MAX;
    }
    if (!dspic33_load_program_word(cpu, 0x0200u, 0x090011u) ||
        !dspic33_load_program_word(cpu, 0x0202u, opcode) || dspic33_step(cpu) != DSPIC33_RUNNING) {
        return false;
    }
    while (cpu->repeat_active != 0u) {
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            return false;
        }
    }
    uint16_t expected_status =
        (uint16_t)((remainder == 0 ? 0x0002u : 0u) | (remainder < 0 ? 0x0008u : 0u) |
                   (overflow ? 0x0004u : 0u));
    return (overflow || (cpu->w[0] == (uint16_t)quotient && cpu->w[1] == (uint16_t)remainder)) &&
           (cpu->sr & 0x000eu) == expected_status && cpu->pc == 0x0204u && cpu->cycles == 19u &&
           !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
}

void dspic33_data_test_divide_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    for (opcode = 0xd80000u; opcode <= 0xd9ffffu; opcode += 257u) {
        if (documented_divide_encoding_valid(opcode)) {
            expect_dsp_matrix_case(state, run_legal_divide_matrix_case(cpu, opcode), opcode,
                                   "legal divide encoding and result");
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static uint16_t decimal_adjust_reference(uint16_t value, uint16_t status, bool* carry) {
    uint16_t adjusted = (uint8_t)value;

    if ((adjusted & 0x000fu) > 9u || (status & 0x0100u) != 0u) {
        adjusted += 6u;
    }
    if (adjusted > 0x009fu || (status & 0x0001u) != 0u) {
        adjusted += 0x0060u;
    }
    *carry = (status & 0x0001u) != 0u || adjusted > 0x00ffu;
    return (uint16_t)((value & 0xff00u) | (adjusted & 0x00ffu));
}

void dspic33_data_test_decimal_adjust_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;
    uint16_t value;
    uint16_t status_inputs;
    uint8_t destination;

    for (destination = 0u; destination < 16u; destination++) {
        for (status_inputs = 0u; status_inputs < 4u; status_inputs++) {
            uint16_t status = (uint16_t)(0x000eu | ((status_inputs & 1u) != 0u ? 1u : 0u) |
                                         ((status_inputs & 2u) != 0u ? 0x0100u : 0u));
            for (value = 0u; value <= UINT8_MAX; value++) {
                bool carry;
                opcode = 0xfd4000u | destination;
                uint16_t initial = (uint16_t)(0xa500u | value);
                uint16_t expected = decimal_adjust_reference(initial, status, &carry);
                uint16_t expected_status = (uint16_t)((status & ~1u) | (carry ? 1u : 0u));
                if (destination == 15u) {
                    expected &= 0xfffeu;
                }
                bool matches;

                reset_processor_test(cpu, 0x0200u);
                cpu->sr = status;
                dspic33_set_working_register(cpu, destination, initial);
                matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u &&
                          cpu->cycles == 1u && cpu->w[destination] == expected &&
                          cpu->sr == expected_status && !cpu->illegal_reset &&
                          cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode, "decimal adjust value and flags");
            }
        }
    }

    for (opcode = 0xfd4000u; opcode <= 0xfd4fffu; opcode++) {
        if ((opcode & 0xfffff0u) != 0xfd4000u) {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static void general_arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[6] = {0x100000u, 0x180000u, 0x400000u,
                                      0x480000u, 0x500000u, 0x580000u};
    uint8_t operation;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 6u; operation++) {
        uint32_t fields;
        for (fields = 0u; fields < 0x080000u; fields += 257u) {
            uint32_t opcode = bases[operation] | fields;
            uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
            if (destination_mode >= 6u) {
                run_invalid_binary_matrix_case(state, cpu, opcode);
            } else {
                dspic33_data_test_run_legal_binary_matrix_case(state, cpu, opcode,
                                                               (BinaryMatrixOperation)operation);
            }
        }
    }
}

static void general_logical_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[3] = {0x600000u, 0x680000u, 0x700000u};
    static const BinaryMatrixOperation operations[3] = {
        ARITHMETIC_MATRIX_AND, ARITHMETIC_MATRIX_XOR, ARITHMETIC_MATRIX_IOR};
    uint8_t operation;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 3u; operation++) {
        uint32_t fields;
        for (fields = 0u; fields < 0x080000u; fields += 257u) {
            uint32_t opcode = bases[operation] | fields;
            uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
            if (destination_mode >= 6u) {
                run_invalid_binary_matrix_case(state, cpu, opcode);
            } else {
                dspic33_data_test_run_legal_binary_matrix_case(state, cpu, opcode,
                                                               operations[operation]);
            }
        }
    }
}

static void run_literal_binary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                           BinaryMatrixOperation operation, uint16_t literal,
                                           bool byte_mode) {
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    uint16_t initial_status =
        dspic33_data_test_binary_matrix_logical(operation)
            ? (uint16_t)((destination & 1u) | (((literal >> 1u) & 1u) << 1u) |
                         (((literal >> 2u) & 1u) << 2u) | (((literal >> 3u) & 1u) << 3u) |
                         (((literal >> 4u) & 1u) << 8u))
            : (uint16_t)((destination & 1u) | (((literal >> 1u) & 1u) << 1u));
    static const uint16_t byte_values[4] = {0x0000u, 0x0080u, 0x00ffu, 0x0055u};
    static const uint16_t word_values[4] = {0x0000u, 0x8000u, 0xffffu, 0x5555u};
    uint16_t left = byte_mode ? byte_values[destination & 3u] : word_values[destination & 3u];
    uint16_t expected;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    cpu->pc = 0u;
    cpu->sr = initial_status;
    cpu->corcon = 0x0020u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->events.count = 0u;
    dspic33_set_working_register(cpu, destination, byte_mode ? (uint16_t)(0xa500u | left) : left);
    left = byte_mode ? (uint8_t)cpu->w[destination] : cpu->w[destination];
    expected =
        dspic33_data_test_binary_matrix_result(operation, left, literal, initial_status, byte_mode);
    expected_status =
        dspic33_data_test_binary_matrix_status(operation, left, literal, initial_status, byte_mode);
    if (byte_mode) {
        expected = (uint16_t)((cpu->w[destination] & 0xff00u) | expected);
    }
    if (destination == 15u) {
        expected &= 0xfffeu;
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
              cpu->sr == expected_status && cpu->corcon == 0x0020u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "literal binary encoding");
}

static void literal_arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[4] = {0xb00000u, 0xb08000u, 0xb10000u, 0xb18000u};
    static const BinaryMatrixOperation operations[4] = {
        ARITHMETIC_MATRIX_ADD, ARITHMETIC_MATRIX_ADDC, ARITHMETIC_MATRIX_SUB,
        ARITHMETIC_MATRIX_SUBB};
    uint8_t operation;
    uint8_t byte_mode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 4u; operation++) {
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint16_t maximum = byte_mode != 0u ? UINT8_MAX : 0x03ffu;
            uint16_t literal;
            for (literal = 0u; literal <= maximum; literal += 17u) {
                uint8_t destination;
                for (destination = 0u; destination < 16u; destination++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)literal << 4u) | destination;
                    run_literal_binary_matrix_case(state, cpu, opcode, operations[operation],
                                                   literal, byte_mode != 0u);
                }
            }
        }
    }
}

static void literal_logical_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[3] = {0xb20000u, 0xb28000u, 0xb30000u};
    static const BinaryMatrixOperation operations[3] = {
        ARITHMETIC_MATRIX_AND, ARITHMETIC_MATRIX_XOR, ARITHMETIC_MATRIX_IOR};
    uint8_t operation;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 3u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint16_t maximum = byte_mode != 0u ? UINT8_MAX : 0x03ffu;
            uint16_t literal;
            for (literal = 0u; literal <= maximum; literal += 17u) {
                uint8_t destination;
                for (destination = 0u; destination < 16u; destination++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)literal << 4u) | destination;
                    run_literal_binary_matrix_case(state, cpu, opcode, operations[operation],
                                                   literal, byte_mode != 0u);
                }
            }
        }
    }
    for (uint32_t opcode = 0xb38000u; opcode < 0xb40000u; opcode += 257u) {
        if ((opcode & 0xfff000u) != 0xb3c000u) {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static void arithmetic_flag_boundary_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[6] = {0x100000u, 0x180000u, 0x400000u,
                                      0x480000u, 0x500000u, 0x580000u};
    static const uint16_t byte_values[8] = {0x00u, 0x01u, 0x0fu, 0x10u, 0x7fu, 0x80u, 0xfeu, 0xffu};
    static const uint16_t word_values[8] = {0x0000u, 0x0001u, 0x00ffu, 0x0100u,
                                            0x7fffu, 0x8000u, 0xfffeu, 0xffffu};
    uint8_t operation;
    uint8_t byte_mode;
    uint8_t left_index;
    uint8_t right_index;
    uint8_t carry;
    uint8_t zero;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 6u; operation++) {
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            const uint16_t* values = byte_mode != 0u ? byte_values : word_values;
            for (left_index = 0u; left_index < 8u; left_index++) {
                for (right_index = 0u; right_index < 8u; right_index++) {
                    for (carry = 0u; carry < 2u; carry++) {
                        for (zero = 0u; zero < 2u; zero++) {
                            uint16_t initial_status = (uint16_t)(carry | ((uint16_t)zero << 1u));
                            uint32_t opcode = bases[operation] | ((uint32_t)2u << 15u) |
                                              ((uint32_t)byte_mode << 14u) | ((uint32_t)4u << 7u) |
                                              3u;
                            uint16_t expected = dspic33_data_test_binary_matrix_result(
                                (BinaryMatrixOperation)operation, values[left_index],
                                values[right_index], initial_status, byte_mode != 0u);
                            uint16_t expected_status = dspic33_data_test_binary_matrix_status(
                                (BinaryMatrixOperation)operation, values[left_index],
                                values[right_index], initial_status, byte_mode != 0u);
                            bool matches;

                            cpu->pc = 0u;
                            cpu->sr = initial_status;
                            cpu->corcon = 0x0020u;
                            cpu->unsupported_opcode = 0u;
                            cpu->last_trap = UINT16_MAX;
                            cpu->address_error = false;
                            cpu->illegal_reset = false;
                            cpu->stop_reason = DSPIC33_RUNNING;
                            cpu->events.count = 0u;
                            dspic33_set_working_register(cpu, 2u, values[left_index]);
                            dspic33_set_working_register(cpu, 3u, values[right_index]);
                            dspic33_set_working_register(cpu, 4u, 0xa55au);
                            if (byte_mode != 0u) {
                                expected = (uint16_t)(0xa500u | expected);
                            }
                            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                                      cpu->w[4] == expected && cpu->sr == expected_status &&
                                      cpu->unsupported_opcode == 0u && !cpu->address_error &&
                                      !cpu->illegal_reset;
                            expect_dsp_matrix_case(state, matches, opcode,
                                                   "arithmetic flag boundary");
                        }
                    }
                }
            }
        }
    }
}

void dspic33_data_test_arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    general_arithmetic_encoding_matrix_cases(state, cpu);
    general_logical_encoding_matrix_cases(state, cpu);
    literal_arithmetic_encoding_matrix_cases(state, cpu);
    literal_logical_encoding_matrix_cases(state, cpu);
    arithmetic_flag_boundary_cases(state, cpu);
}

bool dspic33_data_test_direct_file_address_implemented(uint16_t address) {
    return dspic33ep_mu_address_implemented(DSPIC33EP_MU_DEVICE_512MU810, address);
}

bool dspic33_data_test_direct_file_reads_source(DirectFileOperation operation) {
    return operation != DIRECT_FILE_CLR && operation != DIRECT_FILE_SETM;
}

bool dspic33_data_test_direct_file_writes_result(DirectFileOperation operation) {
    return operation < DIRECT_FILE_CP;
}

static bool direct_file_shift_operation(DirectFileOperation operation) {
    return operation >= DIRECT_FILE_SL && operation <= DIRECT_FILE_RRC;
}

uint16_t dspic33_data_test_shift_matrix_result(DirectFileOperation operation, uint16_t source,
                                               uint16_t initial_status, bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t carry = initial_status & 1u;

    source &= mask;
    if (operation == DIRECT_FILE_SL) {
        return (uint16_t)((source << 1u) & mask);
    }
    if (operation == DIRECT_FILE_LSR) {
        return (uint16_t)(source >> 1u);
    }
    if (operation == DIRECT_FILE_ASR) {
        return (uint16_t)((source >> 1u) | (source & sign));
    }
    if (operation == DIRECT_FILE_RLNC) {
        return (uint16_t)(((source << 1u) & mask) | ((source & sign) != 0u ? 1u : 0u));
    }
    if (operation == DIRECT_FILE_RLC) {
        return (uint16_t)(((source << 1u) & mask) | carry);
    }
    if (operation == DIRECT_FILE_RRNC) {
        return (uint16_t)((source >> 1u) | ((source & 1u) != 0u ? sign : 0u));
    }
    return (uint16_t)((source >> 1u) | (carry != 0u ? sign : 0u));
}

uint16_t dspic33_data_test_shift_matrix_status(DirectFileOperation operation, uint16_t source,
                                               uint16_t initial_status, bool byte_mode) {
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t value =
        dspic33_data_test_shift_matrix_result(operation, source, initial_status, byte_mode);
    uint16_t status = (uint16_t)(initial_status & ~0x000au);

    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    if (operation == DIRECT_FILE_SL || operation == DIRECT_FILE_RLC) {
        status = (uint16_t)((status & ~1u) | ((source & sign) != 0u ? 1u : 0u));
    } else if (operation == DIRECT_FILE_LSR || operation == DIRECT_FILE_ASR ||
               operation == DIRECT_FILE_RRC) {
        status = (uint16_t)((status & ~1u) | (source & 1u));
    }
    return status;
}

uint16_t dspic33_data_test_direct_file_result(DirectFileOperation operation, uint16_t left,
                                              uint16_t right, uint16_t initial_status,
                                              bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;

    if (operation <= DIRECT_FILE_SUBB) {
        return dspic33_data_test_binary_matrix_result((BinaryMatrixOperation)operation, left, right,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_AND) {
        return (uint16_t)((left & right) & mask);
    }
    if (operation == DIRECT_FILE_XOR) {
        return (uint16_t)((left ^ right) & mask);
    }
    if (operation == DIRECT_FILE_IOR) {
        return (uint16_t)((left | right) & mask);
    }
    if (operation == DIRECT_FILE_INC || operation == DIRECT_FILE_INC2) {
        return dspic33_data_test_binary_matrix_result(ARITHMETIC_MATRIX_ADD, left,
                                                      operation == DIRECT_FILE_INC2 ? 2u : 1u,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_DEC || operation == DIRECT_FILE_DEC2) {
        return dspic33_data_test_binary_matrix_result(ARITHMETIC_MATRIX_SUB, left,
                                                      operation == DIRECT_FILE_DEC2 ? 2u : 1u,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_NEG) {
        return dspic33_data_test_binary_matrix_result(ARITHMETIC_MATRIX_SUB, 0u, left,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_COM) {
        return (uint16_t)(~left & mask);
    }
    if (direct_file_shift_operation(operation)) {
        return dspic33_data_test_shift_matrix_result(operation, left, initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CP) {
        return dspic33_data_test_binary_matrix_result(ARITHMETIC_MATRIX_SUB, left, right,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CPB) {
        return dspic33_data_test_binary_matrix_result(ARITHMETIC_MATRIX_SUBB, left, right,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CP0) {
        return dspic33_data_test_binary_matrix_result(ARITHMETIC_MATRIX_SUB, left, 0u,
                                                      initial_status, byte_mode);
    }
    return operation == DIRECT_FILE_SETM ? mask : 0u;
}

uint16_t dspic33_data_test_direct_file_logic_status(uint16_t initial_status, uint16_t value,
                                                    bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t status = (uint16_t)(initial_status & ~0x000au);

    value &= mask;
    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    return status;
}

uint16_t dspic33_data_test_direct_file_status(DirectFileOperation operation, uint16_t left,
                                              uint16_t right, uint16_t initial_status,
                                              bool byte_mode) {
    uint16_t value =
        dspic33_data_test_direct_file_result(operation, left, right, initial_status, byte_mode);

    if (operation <= DIRECT_FILE_SUBB) {
        return dspic33_data_test_binary_matrix_status((BinaryMatrixOperation)operation, left, right,
                                                      initial_status, byte_mode);
    }
    if (operation <= DIRECT_FILE_IOR || operation == DIRECT_FILE_COM) {
        return dspic33_data_test_direct_file_logic_status(initial_status, value, byte_mode);
    }
    if (direct_file_shift_operation(operation)) {
        return dspic33_data_test_shift_matrix_status(operation, left, initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_INC || operation == DIRECT_FILE_INC2) {
        return dspic33_data_test_binary_matrix_status(ARITHMETIC_MATRIX_ADD, left,
                                                      operation == DIRECT_FILE_INC2 ? 2u : 1u,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_DEC || operation == DIRECT_FILE_DEC2) {
        return dspic33_data_test_binary_matrix_status(ARITHMETIC_MATRIX_SUB, left,
                                                      operation == DIRECT_FILE_DEC2 ? 2u : 1u,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_NEG) {
        return dspic33_data_test_binary_matrix_status(ARITHMETIC_MATRIX_SUB, 0u, left,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CP) {
        return dspic33_data_test_binary_matrix_status(ARITHMETIC_MATRIX_SUB, left, right,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CPB) {
        return dspic33_data_test_binary_matrix_status(ARITHMETIC_MATRIX_SUBB, left, right,
                                                      initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CP0) {
        return dspic33_data_test_binary_matrix_status(ARITHMETIC_MATRIX_SUB, left, 0u,
                                                      initial_status, byte_mode);
    }
    return initial_status;
}

void dspic33_data_test_run_legal_unary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                                   DirectFileOperation operation) {
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    bool reads_source = dspic33_data_test_direct_file_reads_source(operation);
    uint16_t initial_status =
        (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) | (((opcode >> 8u) & 1u) << 2u) |
                   (((opcode >> 9u) & 1u) << 3u) | (((opcode >> 10u) & 1u) << 8u));
    uint16_t registers[16];
    BinaryMatrixOperand source = {0u, true};
    BinaryMatrixOperand destination;
    uint16_t source_value = byte_mode ? (uint16_t)(0x0040u | (opcode & 0x003fu))
                                      : (uint16_t)(0x2100u | (opcode & 0x00ffu));
    uint16_t source_operand = 0u;
    uint16_t value;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    if (reads_source) {
        source =
            dspic33_data_test_binary_matrix_operand(registers, source_mode, source_register, width);
        source_operand = source.direct ? (byte_mode ? (uint8_t)registers[source_register]
                                                    : registers[source_register])
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
    if (reads_source && !source.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, source.address, (uint8_t)source_value);
        } else {
            dspic33_write_word(cpu, source.address, source_value);
        }
    }
    value = dspic33_data_test_direct_file_result(operation, source_operand, 0u, initial_status,
                                                 byte_mode);
    expected_status = dspic33_data_test_direct_file_status(operation, source_operand, 0u,
                                                           initial_status, byte_mode);
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
    if (reads_source && !source.direct &&
        (destination.direct || destination.address != source.address)) {
        matches =
            matches && (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                                  : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal unary encoding");
}
