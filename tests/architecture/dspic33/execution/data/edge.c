#include "architecture/dspic33/execution/data/internal.h"

static uint16_t multiple_shift_result(DirectFileOperation operation, uint16_t source,
                                      uint16_t amount) {
    if (amount >= 16u) {
        return operation == DIRECT_FILE_ASR && (source & 0x8000u) != 0u ? 0xffffu : 0u;
    }
    if (operation == DIRECT_FILE_SL) {
        return (uint16_t)(source << amount);
    }
    if (operation == DIRECT_FILE_ASR) {
        return (uint16_t)((int16_t)source >> amount);
    }
    return (uint16_t)(source >> amount);
}

static void run_multiple_shift_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                           DirectFileOperation operation) {
    uint8_t source = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool literal = (opcode & 0x0040u) != 0u;
    uint16_t source_value = (uint16_t)(0x8001u ^ (opcode * 0x45d9u));
    uint16_t count =
        literal ? (uint16_t)(opcode & 0x0fu) : (uint16_t)(0xa500u | ((opcode >> 7u) & 0x001fu));
    uint16_t amount = literal ? count : (uint16_t)(count & 0x001fu);
    uint16_t initial_status =
        (uint16_t)(0x0105u | ((opcode & 1u) << 1u) | (((opcode >> 7u) & 1u) << 3u));
    uint16_t expected;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, cpu->w, initial_status);
    dspic33_set_working_register(cpu, source, source_value);
    if (!literal) {
        cpu->w[opcode & 0x0fu] = count;
    }
    source_value = cpu->w[source];
    expected = multiple_shift_result(operation, source_value, amount);
    expected_status = (uint16_t)(initial_status & ~0x000au);
    if (expected == 0u) {
        expected_status |= 0x0002u;
    }
    if ((expected & 0x8000u) != 0u) {
        expected_status |= 0x0008u;
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u &&
              cpu->w[destination] == (destination == 15u ? (expected & 0xfffeu) : expected) &&
              cpu->sr == expected_status && !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
    expect_dsp_matrix_case(state, matches, opcode, "multiple-shift encoding");
}

static void multiple_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0xdd0000u; opcode < 0xdf0000u; opcode += 257u) {
        bool left = (opcode & 0xff0000u) == 0xdd0000u;
        bool valid = (opcode & 0x0030u) == 0u && (!left || (opcode & 0x008000u) == 0u);
        if (valid) {
            DirectFileOperation operation = left                 ? DIRECT_FILE_SL
                                            : (opcode & 0x8000u) ? DIRECT_FILE_ASR
                                                                 : DIRECT_FILE_LSR;
            run_multiple_shift_matrix_case(state, cpu, opcode, operation);
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static uint16_t find_first_result(uint16_t source, bool left, bool sign_change) {
    if (sign_change) {
        bool sign = (source & 0x8000u) != 0u;
        uint16_t shifted = (uint16_t)(source << 1u);
        uint8_t count = 0u;
        while (count < 15u && ((shifted & 0x8000u) != 0u) == sign) {
            shifted <<= 1u;
            count++;
        }
        return (uint16_t)(-(int16_t)count);
    }
    for (uint8_t bit = 0u; bit < 16u; bit++) {
        uint16_t mask = left ? (uint16_t)(0x8000u >> bit) : (uint16_t)(1u << bit);
        if ((source & mask) != 0u) {
            return (uint16_t)(bit + 1u);
        }
    }
    return 0u;
}

static void run_find_first_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                       bool sign_change) {
    bool left = (opcode & 0x008000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t registers[16];
    BinaryMatrixOperand source;
    uint16_t source_value = (uint16_t)(opcode * 0x45d9u);
    uint16_t operand;
    uint16_t expected;
    uint16_t initial_status = (uint16_t)(0x010eu | (opcode & 1u));
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    source = dspic33_data_test_binary_matrix_operand(registers, source_mode, source_register, 2u);
    operand = source.direct ? registers[source_register] : source_value;
    if (!source.direct) {
        dspic33_write_word(cpu, source.address, source_value);
    }
    expected = find_first_result(operand, left, sign_change);
    expected_status =
        (uint16_t)((initial_status & ~1u) | (sign_change ? expected == 0xfff1u : expected == 0u));
    dspic33_data_test_binary_matrix_write_register(registers, destination, expected);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              dspic33_data_test_binary_matrix_registers_match(cpu, registers) &&
              !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
    if (!source.direct && destination != source_register) {
        matches = matches && dspic33_read_word(cpu, source.address) == source_value;
    }
    expect_dsp_matrix_case(state, matches, opcode, "find-first encoding");
}

static void find_first_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0xcf0000u; opcode < 0xd00000u; opcode += 257u) {
        bool valid = (opcode & 0x007800u) == 0u && ((opcode >> 4u) & 0x07u) < 6u;
        if (valid) {
            run_find_first_matrix_case(state, cpu, opcode, false);
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
    for (uint32_t opcode = 0xdf0000u; opcode < 0xe00000u; opcode += 257u) {
        bool valid = (opcode & 0x00f800u) == 0u && ((opcode >> 4u) & 0x07u) < 6u;
        if (valid) {
            run_find_first_matrix_case(state, cpu, opcode, true);
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static int64_t accumulator_shift_matrix_result(int64_t value, int16_t amount) {
    if (amount < 0) {
        return value * ((int64_t)1 << -amount);
    }
    if (amount == 0 || value >= 0) {
        return value >> amount;
    }
    return -((-value + ((int64_t)1 << amount) - 1) >> amount);
}

static void run_accumulator_shift_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    bool literal = (opcode & 0x0040u) != 0u;
    uint8_t encoded = literal ? (uint8_t)(opcode & 0x003fu) : (uint8_t)((opcode * 13u) & 0x003fu);
    int16_t amount = (int16_t)(encoded >= 32u ? encoded - 64u : encoded);
    int64_t initial = accumulator == 0u ? 0x0000012345 : -0x0000012345;
    int64_t expected = initial;
    uint64_t cycles;
    bool matches;

    cpu->pc = 0u;
    cpu->sr = 0u;
    cpu->corcon = 0x0020u;
    cpu->accumulator[0] = accumulator == 0u ? initial : 0x5555;
    cpu->accumulator[1] = accumulator == 1u ? initial : -0x5555;
    cpu->unsupported_opcode = 0u;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->events.count = 0u;
    memset(cpu->pending_soft_traps, 0, sizeof(cpu->pending_soft_traps));
    if (!literal) {
        cpu->w[opcode & 0x0fu] = (uint16_t)(0xa5c0u | encoded);
    }
    if (amount >= -16 && amount <= 16) {
        expected = accumulator_shift_matrix_result(initial, amount);
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u &&
              cpu->accumulator[accumulator] == expected &&
              cpu->accumulator[accumulator ^ 1u] == (accumulator == 0u ? -0x5555 : 0x5555) &&
              !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
    if (amount < -16 || amount > 16) {
        matches = matches && active_pending_traps(cpu) == 1u && pending_trap(cpu, 4u) != NULL;
    } else {
        matches = matches && active_pending_traps(cpu) == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "accumulator-shift encoding");
}

static void accumulator_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0xc80000u; opcode < 0xc90000u; opcode += 257u) {
        bool literal = (opcode & 0x0040u) != 0u;
        bool valid = (opcode & 0x7f00u) == 0u && (opcode & 0x0080u) == 0u &&
                     (literal || (opcode & 0x0030u) == 0u);
        if (valid) {
            run_accumulator_shift_matrix_case(state, cpu, opcode);
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static void single_shift_value_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[7] = {0xd00000u, 0xd10000u, 0xd18000u, 0xd20000u,
                                      0xd28000u, 0xd30000u, 0xd38000u};
    static const DirectFileOperation operations[7] = {
        DIRECT_FILE_SL,  DIRECT_FILE_LSR,  DIRECT_FILE_ASR, DIRECT_FILE_RLNC,
        DIRECT_FILE_RLC, DIRECT_FILE_RRNC, DIRECT_FILE_RRC};

    for (uint8_t operation = 0u; operation < 7u; operation++) {
        for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint32_t maximum = byte_mode != 0u ? UINT8_MAX : UINT16_MAX;
            for (uint32_t source = 0u; source <= maximum; source += byte_mode != 0u ? 17u : 257u) {
                for (uint8_t carry = 0u; carry < 2u; carry++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) | 0x000182u;
                    uint16_t initial_status = (uint16_t)(0x0104u | carry);
                    uint16_t expected = dspic33_data_test_shift_matrix_result(
                        operations[operation], (uint16_t)source, initial_status, byte_mode != 0u);
                    uint16_t expected_status = dspic33_data_test_shift_matrix_status(
                        operations[operation], (uint16_t)source, initial_status, byte_mode != 0u);
                    bool matches;

                    cpu->pc = 0u;
                    cpu->sr = initial_status;
                    cpu->corcon = 0x0020u;
                    cpu->unsupported_opcode = 0u;
                    cpu->illegal_reset = false;
                    cpu->stop_reason = DSPIC33_RUNNING;
                    cpu->events.count = 0u;
                    cpu->w[2] =
                        byte_mode != 0u ? (uint16_t)(0xa500u | (uint8_t)source) : (uint16_t)source;
                    cpu->w[3] = 0x5a5au;
                    if (byte_mode != 0u) {
                        expected |= 0x5a00u;
                    }
                    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                              cpu->w[3] == expected && cpu->sr == expected_status &&
                              !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
                    expect_dsp_matrix_case(state, matches, opcode, "single-shift value boundary");
                }
            }
        }
    }
}

static void multiple_shift_value_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t opcodes[3] = {0xdd1184u, 0xde1184u, 0xde9184u};
    static const DirectFileOperation operations[3] = {DIRECT_FILE_SL, DIRECT_FILE_LSR,
                                                      DIRECT_FILE_ASR};

    for (uint8_t operation = 0u; operation < 3u; operation++) {
        for (uint32_t source = 0u; source <= UINT16_MAX; source += 257u) {
            for (uint16_t amount = 0u; amount < 32u; amount++) {
                uint16_t expected =
                    multiple_shift_result(operations[operation], (uint16_t)source, amount);
                uint16_t initial_status =
                    (uint16_t)(0x0105u | ((source & 1u) << 1u) | (((source >> 15u) & 1u) << 3u));
                uint16_t expected_status = (uint16_t)(initial_status & ~0x000au);
                bool matches;

                if (expected == 0u) {
                    expected_status |= 0x0002u;
                }
                if ((expected & 0x8000u) != 0u) {
                    expected_status |= 0x0008u;
                }
                cpu->pc = 0u;
                cpu->sr = initial_status;
                cpu->corcon = 0x0020u;
                cpu->unsupported_opcode = 0u;
                cpu->illegal_reset = false;
                cpu->stop_reason = DSPIC33_RUNNING;
                cpu->events.count = 0u;
                cpu->w[2] = (uint16_t)source;
                cpu->w[3] = 0x5a5au;
                cpu->w[4] = (uint16_t)(0xa5c0u | amount);
                matches = dspic33_load_program_word(cpu, 0u, opcodes[operation]) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->w[3] == expected && cpu->sr == expected_status &&
                          !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcodes[operation],
                                       "multiple-shift value boundary");
            }
        }
    }
}

static void find_first_value_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t opcodes[3] = {0xcf0182u, 0xcf8182u, 0xdf0182u};

    for (uint8_t operation = 0u; operation < 3u; operation++) {
        bool left = operation == 1u;
        bool sign_change = operation == 2u;
        for (uint32_t source = 0u; source <= UINT16_MAX; source += 257u) {
            uint16_t expected = find_first_result((uint16_t)source, left, sign_change);
            uint16_t initial_status = (uint16_t)(0x010eu | (source & 1u));
            uint16_t expected_status =
                (uint16_t)((initial_status & ~1u) |
                           (sign_change ? expected == 0xfff1u : expected == 0u));
            bool matches;

            cpu->pc = 0u;
            cpu->sr = initial_status;
            cpu->corcon = 0x0020u;
            cpu->unsupported_opcode = 0u;
            cpu->illegal_reset = false;
            cpu->stop_reason = DSPIC33_RUNNING;
            cpu->events.count = 0u;
            cpu->w[2] = (uint16_t)source;
            cpu->w[3] = 0x5a5au;
            matches = dspic33_load_program_word(cpu, 0u, opcodes[operation]) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->w[3] == expected && cpu->sr == expected_status && !cpu->illegal_reset &&
                      cpu->unsupported_opcode == 0u;
            expect_dsp_matrix_case(state, matches, opcodes[operation], "find-first value boundary");
        }
    }
}

static int64_t accumulator_matrix_value(int64_t value) {
    uint64_t bits = (uint64_t)value & 0xffffffffffu;
    return (int64_t)bits - ((bits & 0x8000000000u) != 0u ? 0x10000000000ll : 0ll);
}

static void accumulator_shift_boundary_cases(TestState* state, Dspic33* cpu) {
    static const int64_t values[] = {0,
                                     1,
                                     -1,
                                     INT32_MAX,
                                     INT32_MIN,
                                     (int64_t)INT32_MAX + 1,
                                     (int64_t)INT32_MIN - 1,
                                     0x7fffffffffll,
                                     -0x8000000000ll};
    static const int8_t amounts[] = {-16, -1, 0, 1, 16};

    for (uint8_t accumulator = 0u; accumulator < 2u; accumulator++) {
        for (uint8_t saturation = 0u; saturation < 2u; saturation++) {
            for (uint8_t accumulator_saturation = 0u; accumulator_saturation < 2u;
                 accumulator_saturation++) {
                for (size_t value_index = 0u; value_index < sizeof(values) / sizeof(*values);
                     value_index++) {
                    for (size_t amount_index = 0u;
                         amount_index < sizeof(amounts) / sizeof(*amounts); amount_index++) {
                        int64_t result = accumulator_shift_matrix_result(values[value_index],
                                                                         amounts[amount_index]);
                        int64_t minimum =
                            accumulator_saturation != 0u ? -0x8000000000ll : INT32_MIN;
                        int64_t maximum = accumulator_saturation != 0u ? 0x7fffffffffll : INT32_MAX;
                        bool saturation_status =
                            result < -0x8000000000ll || result > 0x7fffffffffll;
                        uint16_t overflow_flag = accumulator == 0u ? 0x8000u : 0x4000u;
                        uint16_t saturation_flag = accumulator == 0u ? 0x2000u : 0x1000u;
                        uint16_t expected_status = 0u;
                        uint16_t corcon =
                            (uint16_t)(0x0020u | (accumulator_saturation != 0u ? 0x0010u : 0u) |
                                       (saturation != 0u ? (accumulator == 0u ? 0x0080u : 0x0040u)
                                                         : 0u));
                        uint8_t encoded = (uint8_t)(amounts[amount_index] & 0x3f);
                        uint32_t opcode = 0xc80040u | ((uint32_t)accumulator << 15u) | encoded;
                        bool matches;

                        if (saturation != 0u) {
                            if (result < minimum) {
                                result = minimum;
                                saturation_status = true;
                            } else if (result > maximum) {
                                result = maximum;
                                saturation_status = true;
                            }
                        }
                        result = accumulator_matrix_value(result);
                        if (result < INT32_MIN || result > INT32_MAX) {
                            expected_status |= overflow_flag | 0x0800u;
                        }
                        if (saturation_status) {
                            expected_status |= saturation_flag | 0x0400u;
                        }
                        cpu->pc = 0u;
                        cpu->sr = 0u;
                        cpu->corcon = corcon;
                        cpu->accumulator[accumulator] = values[value_index];
                        cpu->accumulator[accumulator ^ 1u] = 0x12345;
                        cpu->unsupported_opcode = 0u;
                        cpu->illegal_reset = false;
                        cpu->stop_reason = DSPIC33_RUNNING;
                        cpu->events.count = 0u;
                        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                                  cpu->accumulator[accumulator] == result &&
                                  cpu->accumulator[accumulator ^ 1u] == 0x12345 &&
                                  cpu->sr == expected_status && !cpu->illegal_reset &&
                                  cpu->unsupported_opcode == 0u;
                        expect_dsp_matrix_case(state, matches, opcode,
                                               "accumulator-shift boundary");
                    }
                }
            }
        }
    }
}

static void accumulator_shift_register_count_cases(TestState* state, Dspic33* cpu) {

    for (uint8_t accumulator = 0u; accumulator < 2u; accumulator++) {
        for (uint8_t encoded = 0u; encoded < 64u; encoded++) {
            int16_t amount = (int16_t)(encoded >= 32u ? encoded - 64u : encoded);
            int64_t initial = accumulator == 0u ? 0x12345 : -0x12345;
            int64_t expected = initial;
            uint32_t opcode = 0xc80002u | ((uint32_t)accumulator << 15u);
            bool matches;

            reset_processor_test(cpu, 0u);
            dspic33_set_async_events(cpu, false);
            cpu->accumulator[accumulator] = initial;
            cpu->accumulator[accumulator ^ 1u] = 0x5a5a;
            cpu->w[2] = (uint16_t)(0xa5c0u | encoded);
            if (amount >= -16 && amount <= 16) {
                expected = accumulator_shift_matrix_result(initial, amount);
            }
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->accumulator[accumulator] == expected &&
                      cpu->accumulator[accumulator ^ 1u] == 0x5a5a && !cpu->illegal_reset &&
                      cpu->unsupported_opcode == 0u;
            if (amount < -16 || amount > 16) {
                matches = matches && (dspic33_read_word(cpu, 0x08c0u) & 0x0080u) != 0u &&
                          active_pending_traps(cpu) == 1u && pending_trap(cpu, 4u) != NULL;
            } else {
                matches = matches && (dspic33_read_word(cpu, 0x08c0u) & 0x0080u) == 0u &&
                          active_pending_traps(cpu) == 0u;
            }
            expect_dsp_matrix_case(state, matches, opcode, "accumulator-shift register count");
        }
    }
}

void dspic33_data_test_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    dspic33_data_test_single_shift_encoding_matrix_cases(state, cpu);
    dspic33_data_test_direct_file_shift_encoding_matrix_cases(state, cpu);
    multiple_shift_encoding_matrix_cases(state, cpu);
    find_first_encoding_matrix_cases(state, cpu);
    accumulator_shift_encoding_matrix_cases(state, cpu);
    single_shift_value_matrix_cases(state, cpu);
    multiple_shift_value_matrix_cases(state, cpu);
    find_first_value_matrix_cases(state, cpu);
    accumulator_shift_boundary_cases(state, cpu);
    accumulator_shift_register_count_cases(state, cpu);
}

static void run_legal_compare_register_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    bool compare_zero = (opcode & 0xff0000u) == 0xe00000u;
    bool with_borrow = !compare_zero && (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x000400u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t base_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    bool literal = !compare_zero && source_mode >= 6u;
    uint16_t initial_status =
        (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) | (((opcode >> 8u) & 1u) << 2u) |
                   (((opcode >> 9u) & 1u) << 3u) | (((opcode >> 10u) & 1u) << 8u));
    uint16_t registers[16];
    BinaryMatrixOperand source = {0u, true};
    uint16_t source_value = byte_mode ? (uint16_t)(0x0080u | (opcode & 0x007fu))
                                      : (uint16_t)(0x8000u | (opcode & 0x03ffu));
    uint16_t left;
    uint16_t right;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    left = compare_zero
               ? 0u
               : (byte_mode ? (uint8_t)registers[base_register] : registers[base_register]);
    if (literal) {
        right = (uint16_t)(((opcode >> 2u) & 0x00e0u) | (opcode & 0x001fu));
    } else {
        source =
            dspic33_data_test_binary_matrix_operand(registers, source_mode, source_register, width);
        right = source.direct
                    ? (byte_mode ? (uint8_t)registers[source_register] : registers[source_register])
                    : source_value;
    }
    if (compare_zero) {
        left = right;
        right = 0u;
    }
    if (!literal && !source.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, source.address, (uint8_t)source_value);
        } else {
            dspic33_write_word(cpu, source.address, source_value);
        }
    }
    expected_status = dspic33_data_test_binary_matrix_status(
        with_borrow ? ARITHMETIC_MATRIX_SUBB : ARITHMETIC_MATRIX_SUB, left, right, initial_status,
        byte_mode);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX &&
              dspic33_data_test_binary_matrix_registers_match(cpu, registers);
    if (!literal && !source.direct) {
        matches =
            matches && (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                                  : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal compare encoding");
}

void dspic33_data_test_compare_register_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xe00000u; opcode < 0xe20000u; opcode += 257u) {
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        bool compare_zero = (opcode & 0xff0000u) == 0xe00000u;
        bool valid = compare_zero ? (opcode & 0x00fb80u) == 0u && source_mode < 6u
                                  : source_mode >= 6u || (opcode & 0x000380u) == 0u;

        if (valid) {
            run_legal_compare_register_case(state, cpu, opcode);
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

void dspic33_data_test_compare_direct_file_encoding_matrix_cases(TestState* state,
                                                                 Dspic33* invalid_cpu) {
    static Dspic33 actual;
    static Dspic33 reference;
    uint32_t opcode;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file compare processors");
    if (!actual_initialized || !reference_initialized) {
        if (actual_initialized) {
            dspic33_release(&actual);
        }
        if (reference_initialized) {
            dspic33_release(&reference);
        }
        return;
    }
    expect(state,
           dspic33_data_test_load_direct_file_trap_vectors(&actual) &&
               dspic33_data_test_load_direct_file_trap_vectors(&reference),
           "load direct-file compare address-error vectors");
    for (opcode = 0xe20000u; opcode < 0xe40000u; opcode += 257u) {
        bool compare_zero = (opcode & 0xff0000u) == 0xe20000u;
        bool valid = compare_zero ? (opcode & 0x00a000u) == 0u : (opcode & 0x002000u) == 0u;

        if (valid) {
            uint16_t address = (uint16_t)(opcode & 0x1fffu);
            bool byte_mode = (opcode & 0x004000u) != 0u;
            DirectFileOperation operation = compare_zero                 ? DIRECT_FILE_CP0
                                            : (opcode & 0x008000u) != 0u ? DIRECT_FILE_CPB
                                                                         : DIRECT_FILE_CP;
            bool matches = dspic33_data_test_run_direct_file_case(
                &actual, &reference, opcode, operation, address, byte_mode, false);
            expect_dsp_matrix_case(state, matches, opcode, "direct-file compare encoding");
        } else {
            run_invalid_binary_matrix_case(state, invalid_cpu, opcode);
        }
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}

static bool compare_control_reference_taken(uint32_t opcode, uint16_t left, uint16_t right) {
    bool byte_mode = (opcode & 0x000400u) != 0u;
    int32_t signed_left;
    int32_t signed_right;

    if (byte_mode) {
        left &= 0x00ffu;
        right &= 0x00ffu;
    }
    if ((opcode & 0xff8000u) == 0xe78000u) {
        return left == right;
    }
    if ((opcode & 0xff8000u) == 0xe70000u) {
        return left != right;
    }
    signed_left = byte_mode ? (int8_t)left : (int16_t)left;
    signed_right = byte_mode ? (int8_t)right : (int16_t)right;
    return (opcode & 0xff8000u) == 0xe60000u ? signed_left > signed_right
                                             : signed_left < signed_right;
}

static int8_t compare_control_reference_displacement(uint32_t opcode) {
    uint8_t encoded = (uint8_t)((opcode >> 4u) & 0x3fu);
    return (int8_t)((encoded & 0x20u) != 0u ? encoded | 0xc0u : encoded);
}

static void compare_control_operands(uint32_t opcode, bool alternate, uint16_t* left,
                                     uint16_t* right) {
    bool byte_mode = (opcode & 0x000400u) != 0u;
    uint32_t kind = opcode & 0xff8000u;

    if (kind == 0xe78000u) {
        *left = alternate ? (byte_mode ? 0x12a5u : 0xa5a5u) : 0x0000u;
        *right = alternate ? (byte_mode ? 0x34a5u : 0xa5a5u) : 0x0001u;
    } else if (kind == 0xe70000u) {
        *left = alternate ? 0x0000u : (byte_mode ? 0x12a5u : 0xa5a5u);
        *right = alternate ? 0x0001u : (byte_mode ? 0x34a5u : 0xa5a5u);
    } else if (kind == 0xe60000u) {
        *left = alternate ? (byte_mode ? 0x127fu : 0x7fffu) : (byte_mode ? 0x1280u : 0x8000u);
        *right = alternate ? (byte_mode ? 0x3480u : 0x8000u) : (byte_mode ? 0x347fu : 0x7fffu);
    } else {
        *left = alternate ? (byte_mode ? 0x1280u : 0x8000u) : (byte_mode ? 0x127fu : 0x7fffu);
        *right = alternate ? (byte_mode ? 0x347fu : 0x7fffu) : (byte_mode ? 0x3480u : 0x8000u);
    }
}

void dspic33_data_test_run_compare_control_encoding_case(TestState* state, Dspic33* cpu,
                                                         uint32_t opcode, bool alternate) {
    uint8_t left_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t right_register = (uint8_t)(opcode & 0x0fu);
    uint16_t initial_status = alternate ? 0x010fu : 0u;
    uint16_t registers[16];
    uint16_t left;
    uint16_t right;
    int8_t displacement = compare_control_reference_displacement(opcode);
    bool taken;
    uint32_t expected_pc;
    uint64_t expected_cycles;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    compare_control_operands(opcode, alternate, &left, &right);
    dspic33_data_test_binary_matrix_write_register(registers, left_register, left);
    if (right_register != left_register) {
        dspic33_data_test_binary_matrix_write_register(registers, right_register, right);
    }
    for (uint8_t reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg, registers[reg]);
    }
    left = registers[left_register];
    right = registers[right_register];
    taken = compare_control_reference_taken(opcode, left, right);
    expected_pc =
        taken ? (displacement == 1 ? 0x2004u
                                   : (uint32_t)((0x2002 + (int32_t)displacement * 2) & 0x007ffffe))
              : 0x2002u;
    expected_cycles = taken ? (displacement == 1 ? 2u : 5u) : 1u;
    cpu->pc = 0x2000u;
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x2000u, opcode) &&
              dspic33_load_program_word(cpu, 0x2002u, OPCODE_NOP) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == expected_cycles && cpu->instructions - instructions == 1u &&
              cpu->sr == initial_status && cpu->corcon == 0x0020u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              dspic33_data_test_binary_matrix_registers_match(cpu, registers);
    expect_dsp_matrix_case(state, matches, opcode,
                           alternate ? "compare control alternate encoding"
                                     : "compare control primary encoding");
}
