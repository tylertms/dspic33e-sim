#include "processor_test_support.h"

static void repeat_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t count;
    uint8_t reg;

    for (count = 0u; count <= 0x7fffu; count += 257u) {
        bool matches;
        uint32_t opcode = 0x090000u | count;
        reset_processor_test(cpu, 0x200u);
        matches = dspic33_load_program_word(cpu, 0x200u, opcode) &&
                  dspic33_load_program_word(cpu, 0x202u, OPCODE_NOP) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u &&
                  cpu->cycles == 1u && cpu->rcount == count &&
                  cpu->repeat_active == (count != 0u) &&
                  (cpu->sr & 0x0010u) == (count != 0u ? 0x0010u : 0u) &&
                  cpu->repeat_pc == (count != 0u ? 0x202u : 0u) &&
                  cpu->unsupported_opcode == 0u;
        expect_dsp_matrix_case(state, matches, opcode, "REPEAT literal encoding");
    }

    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value;
        uint32_t opcode = 0x098000u | reg;
        bool matches;
        reset_processor_test(cpu, 0x200u);
        dspic33_set_working_register(cpu, reg,
                                     reg == 0u ? 0u : (uint16_t)(0x1111u * reg));
        value = cpu->w[reg];
        matches = dspic33_load_program_word(cpu, 0x200u, opcode) &&
                  dspic33_load_program_word(cpu, 0x202u, OPCODE_NOP) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u &&
                  cpu->cycles == 1u && cpu->rcount == value &&
                  cpu->repeat_active == (value != 0u) &&
                  (cpu->sr & 0x0010u) == (value != 0u ? 0x0010u : 0u) &&
                  cpu->repeat_pc == (value != 0u ? 0x202u : 0u) &&
                  cpu->unsupported_opcode == 0u;
        expect_dsp_matrix_case(state, matches, opcode, "REPEAT register encoding");
    }

    reset_processor_test(cpu, 0x200u);
    expect_dsp_matrix_case(state,
                           dspic33_load_program_word(cpu, 0x200u, 0x098010u) &&
                               dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                               cpu->unsupported_opcode == 0x098010u &&
                               cpu->pc == 0x200u,
                           0x098010u, "REPEAT reserved register encoding");
}

static void run_do_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                 uint16_t count, uint32_t extension,
                                 uint32_t expected_end, const char* domain) {
    bool matches;
    reset_processor_test(cpu, 0x20000u);
    matches = dspic33_load_program_word(cpu, 0x20000u, opcode) &&
              dspic33_load_program_word(cpu, 0x20002u, extension) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20004u &&
              cpu->cycles == 2u && cpu->do_depth == 1u && cpu->do_count[0] == count &&
              cpu->dcount == count && cpu->do_start[0] == 0x20004u &&
              cpu->dostart == 0x20004u && cpu->do_end[0] == expected_end &&
              cpu->doend == expected_end && cpu->corcon == 0x0120u &&
              (cpu->sr & 0x0200u) != 0u && cpu->unsupported_opcode == 0u;
    expect_dsp_matrix_case(state, matches, opcode, domain);
}

static void do_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t count;
    uint32_t extension;
    uint8_t reg;

    for (count = 0u; count <= 0x7fffu; count += 257u) {
        run_do_encoding_case(state, cpu, 0x080000u | count, (uint16_t)count, 2u,
                             0x20008u, "DO literal encoding");
    }

    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value;
        uint32_t opcode = 0x088000u | reg;
        reset_processor_test(cpu, 0x20000u);
        dspic33_set_working_register(cpu, reg,
                                     reg == 0u ? 0u : (uint16_t)(0x1111u * reg));
        value = cpu->w[reg];
        expect_dsp_matrix_case(
            state,
            dspic33_load_program_word(cpu, 0x20000u, opcode) &&
                dspic33_load_program_word(cpu, 0x20002u, 2u) &&
                dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20004u &&
                cpu->cycles == 2u && cpu->do_depth == 1u && cpu->do_count[0] == value &&
                cpu->dcount == value && cpu->do_start[0] == 0x20004u &&
                cpu->do_end[0] == 0x20008u && cpu->dostart == 0x20004u &&
                cpu->doend == 0x20008u && cpu->corcon == 0x0120u &&
                (cpu->sr & 0x0200u) != 0u && cpu->unsupported_opcode == 0u,
            opcode, "DO register encoding");
    }

    for (extension = 0u; extension <= 0xffffu; extension += 257u) {
        uint32_t expected_end;
        if (extension == 0u || extension == 1u || extension == 0xffffu) {
            continue;
        }
        expected_end = (uint32_t)(0x20004 + (int32_t)(int16_t)extension * 2);
        run_do_encoding_case(state, cpu, OPCODE_DO_0, 0u, extension, expected_end,
                             "DO loop-length encoding");
    }

    for (extension = 1u; extension <= 0xffu; extension++) {
        uint32_t invalid_extension = extension << 16u | 2u;
        reset_processor_test(cpu, 0x20000u);
        expect_dsp_matrix_case(
            state,
            dspic33_load_program_word(cpu, 0x20000u, OPCODE_DO_0) &&
                dspic33_load_program_word(cpu, 0x20002u, invalid_extension) &&
                dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                cpu->unsupported_opcode == OPCODE_DO_0 && cpu->pc == 0x20000u &&
                cpu->cycles == 0u && cpu->do_depth == 0u,
            invalid_extension, "DO literal reserved extension");

        reset_processor_test(cpu, 0x20000u);
        dspic33_set_working_register(cpu, 0u, 0xaaaau);
        expect_dsp_matrix_case(
            state,
            dspic33_load_program_word(cpu, 0x20000u, OPCODE_DO_W0) &&
                dspic33_load_program_word(cpu, 0x20002u, invalid_extension) &&
                dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                cpu->unsupported_opcode == OPCODE_DO_W0 && cpu->pc == 0x20000u &&
                cpu->cycles == 0u && cpu->do_depth == 0u,
            invalid_extension, "DO register reserved extension");
    }

    reset_processor_test(cpu, 0x20000u);
    expect_dsp_matrix_case(state,
                           dspic33_load_program_word(cpu, 0x20000u, 0x088010u) &&
                               dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                               cpu->unsupported_opcode == 0x088010u &&
                               cpu->pc == 0x20000u,
                           0x088010u, "DO reserved register encoding");
}

static void load_three_instruction_do(TestState* state, Dspic33* cpu, uint16_t count) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x080000u | count);
    load_instruction(state, cpu, 0x202u, 2u);
    load_instruction(state, cpu, 0x204u, OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x206u, OPCODE_NOP);
    load_instruction(state, cpu, 0x208u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->do_depth == 1u,
           "DO control case starts loop");
}

static void do_register_control_cases(TestState* state, Dspic33* cpu) {
    uint32_t start;

    reset_processor_test(cpu, 0x200u);
    dspic33_write_word(cpu, 0x003au, 0xffffu);
    dspic33_write_word(cpu, 0x003cu, 0xffffu);
    dspic33_write_byte(cpu, 0x003au, 0xa5u);
    dspic33_write_byte(cpu, 0x003du, 0x3fu);
    expect(state,
           cpu->dostart == 0u && dspic33_read_word(cpu, 0x003au) == 0u &&
               dspic33_read_word(cpu, 0x003cu) == 0u,
           "DOSTART ignores word and byte writes while inactive");

    load_three_instruction_do(state, cpu, 2u);
    start = cpu->dostart;
    dspic33_write_word(cpu, 0x003au, 0xffffu);
    dspic33_write_word(cpu, 0x003cu, 0xffffu);
    expect(state,
           cpu->dostart == start && cpu->do_start[0] == start &&
               dspic33_read_word(cpu, 0x003au) == (uint16_t)start &&
               dspic33_read_word(cpu, 0x003cu) == (uint16_t)(start >> 16u),
           "DOSTART ignores writes while a loop is active");

    dspic33_write_word(cpu, 0x0038u, 1u);
    expect(state, cpu->dcount == 1u && cpu->do_count[0] == 1u,
           "DCOUNT word write updates the active loop counter");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->do_depth == 0u &&
               cpu->w[2] == 2u,
           "active DCOUNT write changes remaining iteration count");

    load_three_instruction_do(state, cpu, 2u);
    dspic33_write_byte(cpu, 0x0038u, 0x34u);
    dspic33_write_byte(cpu, 0x0039u, 0x12u);
    expect(state, cpu->dcount == 0x1234u && cpu->do_count[0] == 0x1234u,
           "DCOUNT byte writes update the active loop counter");

    load_three_instruction_do(state, cpu, 1u);
    dspic33_write_word(cpu, 0x003eu, 0x0206u);
    dspic33_write_word(cpu, 0x0040u, 0u);
    expect(state, cpu->doend == 0x0206u && cpu->do_end[0] == 0x0206u,
           "DOEND word write updates the active loop boundary");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->do_depth == 0u &&
               cpu->w[2] == 2u,
           "active DOEND write changes the loop completion boundary");

    load_three_instruction_do(state, cpu, 1u);
    dspic33_write_byte(cpu, 0x003eu, 0x0au);
    dspic33_write_byte(cpu, 0x003fu, 0x02u);
    dspic33_write_byte(cpu, 0x0040u, 0x01u);
    expect(state, cpu->doend == 0x01020au && cpu->do_end[0] == 0x01020au,
           "DOEND byte writes update every implemented address field");
}

static void run_do_to_completion(TestState* state, Dspic33* cpu,
                                 uint32_t expected_increments, const char* name) {
    uint32_t steps = 0u;
    while (cpu->do_depth != 0u && steps < 16u) {
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            break;
        }
        steps++;
    }
    expect(state, cpu->do_depth == 0u && cpu->w[2] == expected_increments, name);
}

static void do_early_termination_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;

    load_three_instruction_do(state, cpu, 7u);
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    expect(state, cpu->do_terminate[0] == 1u && (cpu->corcon & 0x0800u) == 0u,
           "EDT before the last two instructions requests current-iteration exit");
    run_do_to_completion(state, cpu, 1u, "early EDT exits after the current iteration");

    load_three_instruction_do(state, cpu, 7u);
    cpu->instruction_active = true;
    cpu->current_instruction_pc = 0x206u;
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    cpu->instruction_active = false;
    expect(state, cpu->do_terminate[0] == 2u && (cpu->corcon & 0x0800u) == 0u,
           "EDT in the penultimate instruction defers termination one iteration");
    run_do_to_completion(state, cpu, 2u,
                         "penultimate EDT permits exactly one additional iteration");

    load_three_instruction_do(state, cpu, 7u);
    cpu->instruction_active = true;
    cpu->current_instruction_pc = 0x208u;
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    cpu->instruction_active = false;
    expect(state, cpu->do_terminate[0] == 2u,
           "EDT in the final instruction defers termination one iteration");
    run_do_to_completion(
        state, cpu, 2u,
        "final-instruction EDT permits exactly one additional iteration");

    load_three_instruction_do(state, cpu, 7u);
    cpu->instruction_active = true;
    cpu->current_instruction_pc = 0x206u;
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    cpu->instruction_active = false;
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize active DO copy destination");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy active late-EDT loop state");
        cpu->do_terminate[0] = 1u;
        run_do_to_completion(state, &copy, 2u,
                             "copied late-EDT state completes independently");
        expect(state, cpu->do_terminate[0] == 1u && cpu->do_depth == 1u,
               "active DO source remains independent from its copy");
        dspic33_release(&copy);
    }
}

static void do_stack_overflow_cases(TestState* state, Dspic33* cpu) {
    uint32_t starts[4] = {0x300u, 0x320u, 0x340u, 0x360u};
    uint32_t ends[4] = {0x308u, 0x328u, 0x348u, 0x368u};
    uint16_t counts[4] = {1u, 2u, 3u, 4u};

    reset_processor_test(cpu, 0x200u);
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0x0010u, 0x000340u);
    load_instruction(state, cpu, 0x0340u, OPCODE_NOP);
    load_instruction(state, cpu, 0x200u, OPCODE_DO_0);
    load_instruction(state, cpu, 0x202u, 2u);
    memcpy(cpu->do_start, starts, sizeof(starts));
    memcpy(cpu->do_end, ends, sizeof(ends));
    memcpy(cpu->do_count, counts, sizeof(counts));
    cpu->do_depth = 4u;
    cpu->dostart = starts[3];
    cpu->doend = ends[3];
    cpu->dcount = counts[3];
    cpu->corcon = 0x0420u;
    cpu->sr |= 0x0200u;
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 6u &&
               cpu->pc == 0x000340u,
           "fifth nested DO enters the generic stack-overflow trap");
    expect(state,
           cpu->do_depth == 4u && memcmp(cpu->do_start, starts, sizeof(starts)) == 0 &&
               memcmp(cpu->do_end, ends, sizeof(ends)) == 0 &&
               memcmp(cpu->do_count, counts, sizeof(counts)) == 0 &&
               cpu->dostart == starts[3] && cpu->doend == ends[3] &&
               cpu->dcount == counts[3] && cpu->corcon == 0x0428u,
           "fifth nested DO leaves every active loop unchanged");
    expect(state,
           (dspic33_read_word(cpu, 0x08c4u) & 0x0010u) != 0u &&
               active_pending_traps(cpu) == 1u && cpu->do_depth == 4u,
           "fifth nested DO sets DOOVR and retains its level-sensitive source");
}

static void prepare_nested_zero_do_case(TestState* state, Dspic33* cpu,
                                        uint16_t inner_count, bool first_outer_nop,
                                        bool second_outer_nop, bool inner_nop) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x080001u);
    load_instruction(state, cpu, 0x202u, 6u);
    load_instruction(state, cpu, 0x204u,
                     first_outer_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x206u,
                     second_outer_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x208u, 0x080000u | inner_count);
    load_instruction(state, cpu, 0x20au, 1u);
    load_instruction(state, cpu, 0x20cu, inner_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x20eu, OPCODE_NOP);
    load_instruction(state, cpu, 0x210u, OPCODE_NOP);
    dspic33_step(cpu);
    dspic33_step(cpu);
    dspic33_step(cpu);
}

static void nested_zero_do_erratum_cases(TestState* state, Dspic33* cpu) {
    prepare_nested_zero_do_case(state, cpu, 0u, true, true, true);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu &&
               cpu->do_depth == 2u && cpu->do_count[1] == 0u,
           "nested zero-count DO accepts the documented NOP workaround");

    prepare_nested_zero_do_case(state, cpu, 0u, true, true, true);
    load_instruction(state, cpu, 0x208u, OPCODE_DO_W0);
    dspic33_set_working_register(cpu, 0u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu &&
               cpu->do_depth == 2u && cpu->do_count[1] == 0u,
           "nested zero-count register DO accepts the documented NOP workaround");

    prepare_nested_zero_do_case(state, cpu, 1u, false, false, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu &&
               cpu->do_depth == 2u && cpu->do_count[1] == 1u,
           "nested nonzero DO does not require the zero-count workaround");

    prepare_nested_zero_do_case(state, cpu, 0u, false, true, true);
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "nested zero-count DO rejects a missing first outer NOP");

    prepare_nested_zero_do_case(state, cpu, 0u, true, false, true);
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "nested zero-count DO rejects a missing second outer NOP");

    prepare_nested_zero_do_case(state, cpu, 0u, true, true, false);
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "nested zero-count DO rejects a missing inner NOP");
}

static void loop_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    repeat_encoding_matrix_cases(state, cpu);
    do_encoding_matrix_cases(state, cpu);
    do_register_control_cases(state, cpu);
    do_early_termination_cases(state, cpu);
    do_stack_overflow_cases(state, cpu);
    nested_zero_do_erratum_cases(state, cpu);
}

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

static bool binary_matrix_with_carry(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_SUBBR ||
           operation == ARITHMETIC_MATRIX_ADDC || operation == ARITHMETIC_MATRIX_SUBB;
}

static bool binary_matrix_reverse(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_SUBR || operation == ARITHMETIC_MATRIX_SUBBR;
}

static bool binary_matrix_addition(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_ADD || operation == ARITHMETIC_MATRIX_ADDC;
}

static bool binary_matrix_logical(BinaryMatrixOperation operation) {
    return operation >= ARITHMETIC_MATRIX_AND;
}

static uint16_t binary_matrix_result(BinaryMatrixOperation operation, uint16_t left,
                                     uint16_t right, uint16_t initial_status,
                                     bool byte_mode) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t carry = (initial_status & 0x0001u) != 0u ? 1u : 0u;
    uint16_t borrow = binary_matrix_with_carry(operation) && carry == 0u ? 1u : 0u;
    uint32_t result;

    left = (uint16_t)(left & mask);
    right = (uint16_t)(right & mask);
    if (operation == ARITHMETIC_MATRIX_AND) {
        result = left & right;
    } else if (operation == ARITHMETIC_MATRIX_XOR) {
        result = left ^ right;
    } else if (operation == ARITHMETIC_MATRIX_IOR) {
        result = left | right;
    } else if (binary_matrix_addition(operation)) {
        result =
            (uint32_t)left + right + (binary_matrix_with_carry(operation) ? carry : 0u);
    } else if (binary_matrix_reverse(operation)) {
        result = (uint16_t)(right - left - borrow);
    } else {
        result = (uint16_t)(left - right - borrow);
    }
    return (uint16_t)(result & mask);
}

static uint16_t binary_matrix_status(BinaryMatrixOperation operation, uint16_t left,
                                     uint16_t right, uint16_t initial_status,
                                     bool byte_mode) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t digit_mask = byte_mode ? 0x000fu : 0x00ffu;
    uint16_t carry = (initial_status & 0x0001u) != 0u ? 1u : 0u;
    uint16_t borrow = binary_matrix_with_carry(operation) && carry == 0u ? 1u : 0u;
    uint16_t add_carry =
        binary_matrix_addition(operation) && binary_matrix_with_carry(operation) ? carry
                                                                                 : 0u;
    bool sticky_zero = binary_matrix_with_carry(operation);
    uint16_t status = (uint16_t)(initial_status & ~0x010fu);
    uint16_t value;

    left = (uint16_t)(left & mask);
    right = (uint16_t)(right & mask);
    value = binary_matrix_result(operation, left, right, initial_status, byte_mode);
    if (binary_matrix_logical(operation)) {
        status = (uint16_t)(initial_status & ~0x000au);
        if (value == 0u) {
            status |= 0x0002u;
        }
        if ((value & sign) != 0u) {
            status |= 0x0008u;
        }
        return status;
    }
    if (value == 0u && (!sticky_zero || (initial_status & 0x0002u) != 0u)) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    if (binary_matrix_addition(operation)) {
        uint32_t result = (uint32_t)left + right + add_carry;
        int32_t signed_left = byte_mode ? (int8_t)left : (int16_t)left;
        int32_t signed_right = byte_mode ? (int8_t)right : (int16_t)right;
        int32_t signed_result = signed_left + signed_right + add_carry;
        int32_t minimum = byte_mode ? INT8_MIN : INT16_MIN;
        int32_t maximum = byte_mode ? INT8_MAX : INT16_MAX;
        if (result > mask) {
            status |= 0x0001u;
        }
        if (((left & digit_mask) + (right & digit_mask) + add_carry) > digit_mask) {
            status |= 0x0100u;
        }
        if (signed_result < minimum || signed_result > maximum) {
            status |= 0x0004u;
        }
    } else {
        uint16_t minuend = binary_matrix_reverse(operation) ? right : left;
        uint16_t subtrahend = binary_matrix_reverse(operation) ? left : right;
        uint32_t subtraction = (uint32_t)subtrahend + borrow;
        uint16_t operand = (uint16_t)(subtraction & mask);
        if (minuend >= subtraction) {
            status |= 0x0001u;
        }
        if ((minuend & digit_mask) >= (uint32_t)(subtrahend & digit_mask) + borrow) {
            status |= 0x0100u;
        }
        if ((((minuend ^ operand) & (minuend ^ value)) & sign) != 0u) {
            status |= 0x0004u;
        }
    }
    return status;
}

static void binary_matrix_write_register(uint16_t registers[16], uint8_t reg,
                                         uint16_t value) {
    registers[reg] = reg == 15u ? (uint16_t)(value & 0xfffeu) : value;
}

static BinaryMatrixOperand binary_matrix_operand(uint16_t registers[16], uint8_t mode,
                                                 uint8_t reg, uint8_t width) {
    BinaryMatrixOperand operand = {0u, mode == 0u};
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
        adjusted =
            (int32_t)registers[reg] + (mode == 3u ? (int32_t)width : -(int32_t)width);
        binary_matrix_write_register(registers, reg, (uint16_t)adjusted);
        return operand;
    }
    adjusted =
        (int32_t)registers[reg] + (mode == 5u ? (int32_t)width : -(int32_t)width);
    operand.address = (uint16_t)adjusted;
    binary_matrix_write_register(registers, reg, (uint16_t)adjusted);
    return operand;
}

static void prepare_arithmetic_matrix_case(Dspic33* cpu, uint16_t registers[16],
                                           uint16_t initial_status) {
    uint8_t reg;

    cpu->pc = 0u;
    cpu->sr = initial_status;
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
    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value = (uint16_t)(0x4000u + (uint16_t)reg * 0x0100u);
        dspic33_set_working_register(cpu, reg, value);
        registers[reg] = cpu->w[reg];
    }
}

static bool binary_matrix_registers_match(const Dspic33* cpu,
                                          const uint16_t registers[16]);

static uint16_t byte_extension_status(uint16_t initial_status, uint16_t value) {
    uint16_t status = (uint16_t)(initial_status & ~0x000bu);
    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & 0x8000u) != 0u) {
        status |= 0x0008u;
    } else {
        status |= 0x0001u;
    }
    return status;
}

static void run_legal_byte_extension_case(TestState* state, Dspic33* cpu,
                                          uint32_t opcode) {
    bool zero_extend = (opcode & 0x008000u) != 0u;
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint16_t registers[16];
    BinaryMatrixOperand source;
    uint8_t source_value = (uint8_t)((opcode >> 1u) ^ opcode ^ 0xa5u);
    uint16_t value;
    uint16_t initial_status = (uint16_t)(0x0104u | (opcode & 0x000bu));
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    source = binary_matrix_operand(registers, source_mode, source_register, 1u);
    if (source.direct) {
        source_value = (uint8_t)registers[source_register];
    } else {
        dspic33_write_byte(cpu, source.address, source_value);
    }
    value = zero_extend ? source_value : (uint16_t)(int16_t)(int8_t)source_value;
    binary_matrix_write_register(registers, destination, value);
    value = registers[destination];
    expected_status = byte_extension_status(initial_status, value);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!source.direct) {
        matches = matches && dspic33_read_byte(cpu, source.address) == source_value;
    }
    expect_dsp_matrix_case(state, matches, opcode, "SE and ZE legal encoding");
}

static void run_invalid_byte_extension_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->illegal_reset = false;
    cpu->last_trap = UINT16_MAX;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->splim_enabled = false;
    cpu->events.count = 0u;
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX &&
              cpu->trap_count == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u;
    expect_dsp_matrix_case(state, matches, opcode, "SE and ZE reserved encoding");
}

static void byte_extension_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;

    for (fields = 0u; fields <= 0xffffu; fields += 257u) {
        uint32_t opcode = 0xfb0000u | fields;
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        if ((opcode & 0x007800u) == 0u && source_mode < 6u) {
            run_legal_byte_extension_case(state, cpu, opcode);
        } else {
            run_invalid_byte_extension_case(state, cpu, opcode);
        }
    }
}

static void byte_extension_value_matrix_cases(TestState* state, Dspic33* cpu) {
    uint16_t value;
    uint8_t status_bits;
    uint8_t operation;

    for (operation = 0u; operation < 2u; operation++) {
        for (value = 0u; value <= 0xffu; value++) {
            for (status_bits = 0u; status_bits < 8u; status_bits++) {
                uint32_t opcode = 0xfb0182u | ((uint32_t)operation << 15u);
                uint16_t initial_status = (uint16_t)(0x0104u | (status_bits & 0x03u) |
                                                     ((status_bits & 0x04u) << 1u));
                uint16_t expected =
                    operation != 0u ? value : (uint16_t)(int16_t)(int8_t)value;
                uint16_t expected_status =
                    byte_extension_status(initial_status, expected);
                uint64_t cycles;
                bool matches;

                prepare_arithmetic_matrix_case(cpu, cpu->w, initial_status);
                dspic33_set_working_register(cpu, 2u, value);
                cycles = cpu->cycles;
                matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->cycles - cycles == 1u && cpu->w[3] == expected &&
                          cpu->sr == expected_status && !cpu->illegal_reset &&
                          cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "SE and ZE value and status");
            }
        }
    }
}

static void byte_extension_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xfb0192u);
    dspic33_set_working_register(cpu, 2u, 0x0800u);
    dspic33_write_word(cpu, 0x0800u, 0x00a5u);
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[3] == 0xffa5u && cpu->w[2] == 0x0800u && cpu->sr == 0x010cu,
           "SE non-CPU SFR byte source consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xfb8192u);
    cpu->w[2] = 0x5000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu,
                         "ZE uninitialized source pointer resets processor");
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

static void run_invalid_stack_encoding_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    prepare_stack_encoding_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->unsupported_opcode == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u;
    expect_dsp_matrix_case(state, matches, opcode, "reserved stack encoding");
}

static void run_direct_stack_encoding_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode, uint16_t address) {
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

static void direct_stack_value_cases(TestState* state, Dspic33* cpu) {
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

static void direct_stack_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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

static void link_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                      dspic33_read_word(cpu, 0x5000u) == 0x4444u &&
                      cpu->corcon == 0x0024u && cpu->sr == 0x010fu &&
                      !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
            expect_dsp_matrix_case(state, matches, opcode, "LNK legal encoding");
        } else {
            run_invalid_stack_encoding_case(state, cpu, opcode);
        }
    }
}

static void shadow_stack_encoding_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t legal_opcodes[] = {0xfe8000u, 0xfea000u, 0xfa8000u};
    size_t index;

    for (index = 0u; index < sizeof(legal_opcodes) / sizeof(legal_opcodes[0]);
         index++) {
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
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles == 1u && !cpu->illegal_reset &&
                  cpu->unsupported_opcode == 0u;
        if (legal_opcodes[index] == 0xfea000u) {
            matches = matches && cpu->shadow_w[0] == 0x1111u &&
                      cpu->shadow_w[1] == 0x2222u && cpu->shadow_w[2] == 0x3333u &&
                      cpu->shadow_w[3] == 0x4444u && cpu->shadow_status == 0x010fu &&
                      cpu->sr == 0x01efu;
        } else if (legal_opcodes[index] == 0xfe8000u) {
            matches = matches && cpu->w[0] == 0x1111u && cpu->w[1] == 0x2222u &&
                      cpu->w[2] == 0x3333u && cpu->w[3] == 0x4444u &&
                      cpu->sr == 0x01efu;
        } else {
            matches = matches && cpu->w[14] == 0x4444u && cpu->w[15] == 0x5000u &&
                      cpu->corcon == 0x0020u;
        }
        expect_dsp_matrix_case(state, matches, legal_opcodes[index],
                               "shadow stack and ULNK encoding");
    }
}

static bool binary_matrix_registers_match(const Dspic33* cpu,
                                          const uint16_t registers[16]) {
    uint8_t reg;

    for (reg = 0u; reg < 16u; reg++) {
        if (cpu->w[reg] != registers[reg]) {
            return false;
        }
    }
    return true;
}

static void run_legal_binary_matrix_case(TestState* state, Dspic33* cpu,
                                         uint32_t opcode,
                                         BinaryMatrixOperation operation) {
    uint8_t left_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint16_t initial_status =
        binary_matrix_logical(operation)
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

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    left = byte_mode ? (uint8_t)registers[left_register] : registers[left_register];
    if (source_mode >= 6u) {
        source.direct = false;
        source.address = 0u;
        right = (uint16_t)(opcode & 0x001fu);
    } else {
        source = binary_matrix_operand(registers, source_mode, source_register, width);
        right = source.direct ? (byte_mode ? (uint8_t)registers[source_register]
                                           : registers[source_register])
                              : source_value;
    }
    destination =
        binary_matrix_operand(registers, destination_mode, destination_register, width);
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
    value = binary_matrix_result(operation, left, right, initial_status, byte_mode);
    expected_status =
        binary_matrix_status(operation, left, right, initial_status, byte_mode);
    if (destination.direct) {
        if (byte_mode) {
            value = (uint16_t)((registers[destination_register] & 0xff00u) |
                               (value & 0x00ffu));
        }
        binary_matrix_write_register(registers, destination_register, value);
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!destination.direct) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, destination.address) == (uint8_t)value
                       : dspic33_read_word(cpu, destination.address) == value);
    }
    if (source_mode < 6u && !source.direct &&
        (destination.direct || destination.address != source.address)) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
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

static void run_legal_register_bit_case(TestState* state, Dspic33* cpu,
                                        uint32_t opcode) {
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

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    bit = kind == 5u ? (uint8_t)(registers[(opcode >> 11u) & 0x0fu] & 0x0fu)
                     : (uint8_t)((opcode >> 12u) & 0x0fu);
    operand = binary_matrix_operand(registers, mode, reg, width);
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
            expected_status = (uint16_t)((initial_status & ~0x0002u) |
                                         ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            expected_status = (uint16_t)((initial_status & ~0x0001u) |
                                         ((value & mask) != 0u ? 0x0001u : 0u));
        }
    } else if (kind == 4u) {
        bool zero_destination = (opcode & 0x000800u) != 0u;
        if (zero_destination) {
            expected_status = (uint16_t)((initial_status & ~0x0002u) |
                                         ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            expected_status = (uint16_t)((initial_status & ~0x0001u) |
                                         ((value & mask) != 0u ? 0x0001u : 0u));
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
            binary_matrix_write_register(registers, reg, value);
        }
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, 0u) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == expected_cycles && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!operand.direct) {
        uint16_t expected = kind <= 2u || kind == 4u ? value : original;
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, operand.address) == (uint8_t)expected
                       : dspic33_read_word(cpu, operand.address) == expected);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal register bit encoding");
}

static void run_legal_file_bit_admission_case(TestState* state, Dspic33* cpu,
                                              uint32_t opcode) {
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

static void bit_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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

static void direct_file_bit_value_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t kinds[] = {0u, 1u, 2u, 3u, 4u, 6u, 7u};
    static const uint16_t values[] = {0x0000u, 0xffffu, 0xa55au, 0x5aa5u};
    size_t kind_index;
    uint8_t bit;
    size_t value_index;

    for (kind_index = 0u; kind_index < sizeof(kinds) / sizeof(kinds[0]); kind_index++) {
        uint8_t kind = kinds[kind_index];
        for (bit = 0u; bit < 16u; bit++) {
            uint16_t address = (uint16_t)(0x1000u + (bit >> 3u));
            uint32_t opcode = 0xa80000u | ((uint32_t)kind << 16u) |
                              ((uint32_t)(bit & 7u) << 13u) | address;
            uint16_t mask = (uint16_t)(1u << bit);
            for (value_index = 0u; value_index < sizeof(values) / sizeof(values[0]);
                 value_index++) {
                uint16_t initial = values[value_index];
                uint16_t expected = initial;
                uint16_t initial_status =
                    (uint16_t)(0x010du | (uint16_t)(value_index & 2u));
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
                    expected_status =
                        (uint16_t)((initial_status & ~0x0002u) |
                                   ((initial & mask) == 0u ? 0x0002u : 0u));
                } else if (kind == 4u) {
                    expected_status =
                        (uint16_t)((initial_status & ~0x0002u) |
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
                          dspic33_step(cpu) == DSPIC33_RUNNING &&
                          cpu->pc == expected_pc && cpu->cycles == expected_cycles &&
                          cpu->sr == expected_status &&
                          dspic33_read_word(cpu, 0x1000u) == expected &&
                          !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "direct file bit value and status");
            }
        }
    }
}

static void bit_operand_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xa40012u);
    dspic33_set_working_register(cpu, 2u, 0x0042u);
    expect_illegal_reset(state, cpu, "indirect BTSTS targeting SR resets processor");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xa40012u);
    cpu->w[2] = 0x1000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu,
                         "BTSTS uninitialized source pointer resets processor");
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

static void run_legal_table_encoding_case(TestState* state, Dspic33* cpu,
                                          uint32_t opcode) {
    bool write = (opcode & 0x010000u) != 0u;
    bool matches;

    prepare_table_encoding_case(cpu, write);
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal table encoding");
}

static void table_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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

static void table_value_cases(TestState* state, Dspic33* cpu) {
    uint8_t high;
    uint8_t byte_mode;
    uint8_t odd;

    for (high = 0u; high < 2u; high++) {
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            for (odd = 0u; odd < 2u; odd++) {
                uint32_t opcode = 0xba0000u | ((uint32_t)high << 15u) |
                                  ((uint32_t)byte_mode << 14u) | ((uint32_t)3u << 7u) |
                                  0x0012u;
                uint16_t expected = high != 0u ? 0x0012u : 0xab56u;
                bool matches;

                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                load_instruction(state, cpu, 0x0200u, 0x12ab56u);
                cpu->tblpag = 0u;
                dspic33_set_working_register(cpu, 2u, (uint16_t)(0x0200u + odd));
                dspic33_set_working_register(cpu, 3u, 0xa500u);
                if (byte_mode != 0u) {
                    expected = high != 0u ? (odd != 0u ? 0u : 0x0012u)
                                          : (odd != 0u ? 0x00abu : 0x0056u);
                    expected |= 0xa500u;
                }
                matches = dspic33_step(cpu) == DSPIC33_RUNNING &&
                          cpu->w[2] == (uint16_t)(0x0200u + odd) &&
                          cpu->w[3] == expected && cpu->sr == 0u && cpu->cycles == 5u &&
                          !cpu->illegal_reset;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "table read value and byte selection");

                opcode = 0xbb0000u | ((uint32_t)high << 15u) |
                         ((uint32_t)byte_mode << 14u) | ((uint32_t)1u << 11u) |
                         ((uint32_t)3u << 7u) | 2u;
                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                cpu->tblpag = 0x00fau;
                dspic33_set_working_register(cpu, 2u, 0xa5c3u);
                dspic33_set_working_register(cpu, 3u, odd);
                matches = dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
                          !cpu->illegal_reset;
                expected = 0xffffu;
                if (high != 0u) {
                    matches =
                        matches && ((cpu->write_latches[0] >> 16u) & 0xffu) ==
                                       (byte_mode != 0u && odd != 0u ? 0xffu : 0xc3u);
                } else {
                    expected = byte_mode == 0u ? 0xa5c3u
                               : odd != 0u     ? 0xc3ffu
                                               : 0xffc3u;
                    matches = matches && (cpu->write_latches[0] & 0xffffu) == expected;
                }
                expect_dsp_matrix_case(state, matches, opcode,
                                       "table write latch and byte selection");
            }
        }
    }
}

static void table_operand_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xbb0982u);
    dspic33_set_working_register(cpu, 2u, 0xa55au);
    cpu->w[3] = 0x5000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0008u;
    expect_illegal_reset(state, cpu,
                         "table write uninitialized destination resets processor");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xbb0992u);
    cpu->w[2] = 0x5000u;
    dspic33_set_working_register(cpu, 3u, 0u);
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu,
                         "table write uninitialized source resets processor");
}

static bool documented_system_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);

    if (family == 0xfcu) {
        return (opcode & 0x00c000u) == 0u;
    }
    if (family != 0xfeu) {
        return true;
    }
    if (opcode == 0xfe0000u || opcode == 0xfe2000u ||
        (opcode & 0xfffffeu) == 0xfe4000u || opcode == 0xfe6000u ||
        opcode == 0xfe8000u || opcode == 0xfea000u) {
        return true;
    }
    if ((opcode & 0xfff000u) == 0xfec000u) {
        return ((opcode >> 10u) & 3u) != 3u;
    }
    return (opcode & 0xfff000u) == 0xfed000u && (opcode & 0x0003f0u) == 0u &&
           ((opcode >> 10u) & 3u) != 3u;
}

static void prepare_system_encoding_case(Dspic33* cpu) {
    reset_processor_test(cpu, 0x0200u);
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    dspic33_set_working_register(cpu, 15u, 0x5000u);
}

static void run_legal_system_encoding_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode) {
    bool matches;

    prepare_system_encoding_case(cpu);
    matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal system encoding");
}

static void run_illegal_system_encoding_case(TestState* state, Dspic33* cpu,
                                             uint32_t opcode) {
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

static void system_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;

    for (fields = 0u; fields <= 0xffffu; fields += 257u) {
        run_legal_system_encoding_case(state, cpu, fields);
        run_legal_system_encoding_case(state, cpu, 0xff0000u | fields);
    }
    for (fields = 0u; fields <= 0xffffu; fields += 257u) {
        uint32_t opcode = 0xfc0000u | fields;
        if (documented_system_encoding_valid(opcode)) {
            run_legal_system_encoding_case(state, cpu, opcode);
        } else {
            run_illegal_system_encoding_case(state, cpu, opcode);
        }
    }
    for (fields = 0u; fields <= 0xffffu; fields += 257u) {
        uint32_t opcode = 0xfe0000u | fields;
        if (documented_system_encoding_valid(opcode)) {
            run_legal_system_encoding_case(state, cpu, opcode);
        } else {
            run_illegal_system_encoding_case(state, cpu, opcode);
        }
    }
}

static void system_control_value_cases(TestState* state, Dspic33* cpu) {
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
                  cpu->cycles - cycles == 1u &&
                  cpu->disicnt == (literal == 0u ? 0u : literal) &&
                  cpu->sr == 0x010fu && !cpu->illegal_reset;
        expect_dsp_matrix_case(state, matches, opcode, "DISI literal value");
    }

    prepare_system_encoding_case(cpu);
    expect(state,
           dspic33_load_program_word(cpu, 0x0200u, 0xfe2000u) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u &&
               cpu->cycles == 1u && cpu->sr == 0x010fu && cpu->corcon == 0x0020u,
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
        quotient =
            (int64_t)(int16_t)cpu->w[high] * 32768 / (int16_t)cpu->w[divisor_register];
        remainder =
            (int64_t)(int16_t)cpu->w[high] * 32768 % (int16_t)cpu->w[divisor_register];
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
        !dspic33_load_program_word(cpu, 0x0202u, opcode) ||
        dspic33_step(cpu) != DSPIC33_RUNNING) {
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
    return (overflow ||
            (cpu->w[0] == (uint16_t)quotient && cpu->w[1] == (uint16_t)remainder)) &&
           (cpu->sr & 0x000eu) == expected_status && cpu->pc == 0x0204u &&
           cpu->cycles == 19u && !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
}

static void divide_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    for (opcode = 0xd80000u; opcode <= 0xd9ffffu; opcode += 257u) {
        if (documented_divide_encoding_valid(opcode)) {
            expect_dsp_matrix_case(state, run_legal_divide_matrix_case(cpu, opcode),
                                   opcode, "legal divide encoding and result");
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

static void decimal_adjust_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;
    uint16_t value;
    uint16_t status_inputs;
    uint8_t destination;

    for (destination = 0u; destination < 16u; destination++) {
        for (status_inputs = 0u; status_inputs < 4u; status_inputs++) {
            uint16_t status =
                (uint16_t)(0x000eu | ((status_inputs & 1u) != 0u ? 1u : 0u) |
                           ((status_inputs & 2u) != 0u ? 0x0100u : 0u));
            for (value = 0u; value <= UINT8_MAX; value++) {
                bool carry;
                opcode = 0xfd4000u | destination;
                uint16_t initial = (uint16_t)(0xa500u | value);
                uint16_t expected = decimal_adjust_reference(initial, status, &carry);
                uint16_t expected_status =
                    (uint16_t)((status & ~1u) | (carry ? 1u : 0u));
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
                expect_dsp_matrix_case(state, matches, opcode,
                                       "decimal adjust value and flags");
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
                run_legal_binary_matrix_case(state, cpu, opcode,
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
                run_legal_binary_matrix_case(state, cpu, opcode, operations[operation]);
            }
        }
    }
}

static void run_literal_binary_matrix_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode,
                                           BinaryMatrixOperation operation,
                                           uint16_t literal, bool byte_mode) {
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    uint16_t initial_status =
        binary_matrix_logical(operation)
            ? (uint16_t)((destination & 1u) | (((literal >> 1u) & 1u) << 1u) |
                         (((literal >> 2u) & 1u) << 2u) |
                         (((literal >> 3u) & 1u) << 3u) |
                         (((literal >> 4u) & 1u) << 8u))
            : (uint16_t)((destination & 1u) | (((literal >> 1u) & 1u) << 1u));
    static const uint16_t byte_values[4] = {0x0000u, 0x0080u, 0x00ffu, 0x0055u};
    static const uint16_t word_values[4] = {0x0000u, 0x8000u, 0xffffu, 0x5555u};
    uint16_t left =
        byte_mode ? byte_values[destination & 3u] : word_values[destination & 3u];
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
    dspic33_set_working_register(cpu, destination,
                                 byte_mode ? (uint16_t)(0xa500u | left) : left);
    left = byte_mode ? (uint8_t)cpu->w[destination] : cpu->w[destination];
    expected =
        binary_matrix_result(operation, left, literal, initial_status, byte_mode);
    expected_status =
        binary_matrix_status(operation, left, literal, initial_status, byte_mode);
    if (byte_mode) {
        expected = (uint16_t)((cpu->w[destination] & 0xff00u) | expected);
    }
    if (destination == 15u) {
        expected &= 0xfffeu;
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
              cpu->sr == expected_status && cpu->corcon == 0x0020u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
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
                    run_literal_binary_matrix_case(state, cpu, opcode,
                                                   operations[operation], literal,
                                                   byte_mode != 0u);
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
                    run_literal_binary_matrix_case(state, cpu, opcode,
                                                   operations[operation], literal,
                                                   byte_mode != 0u);
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
    static const uint16_t byte_values[8] = {0x00u, 0x01u, 0x0fu, 0x10u,
                                            0x7fu, 0x80u, 0xfeu, 0xffu};
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
                            uint16_t initial_status =
                                (uint16_t)(carry | ((uint16_t)zero << 1u));
                            uint32_t opcode = bases[operation] | ((uint32_t)2u << 15u) |
                                              ((uint32_t)byte_mode << 14u) |
                                              ((uint32_t)4u << 7u) | 3u;
                            uint16_t expected = binary_matrix_result(
                                (BinaryMatrixOperation)operation, values[left_index],
                                values[right_index], initial_status, byte_mode != 0u);
                            uint16_t expected_status = binary_matrix_status(
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
                                      dspic33_step(cpu) == DSPIC33_RUNNING &&
                                      cpu->pc == 2u && cpu->w[4] == expected &&
                                      cpu->sr == expected_status &&
                                      cpu->unsupported_opcode == 0u &&
                                      !cpu->address_error && !cpu->illegal_reset;
                            expect_dsp_matrix_case(state, matches, opcode,
                                                   "arithmetic flag boundary");
                        }
                    }
                }
            }
        }
    }
}

static void arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    general_arithmetic_encoding_matrix_cases(state, cpu);
    general_logical_encoding_matrix_cases(state, cpu);
    literal_arithmetic_encoding_matrix_cases(state, cpu);
    literal_logical_encoding_matrix_cases(state, cpu);
    arithmetic_flag_boundary_cases(state, cpu);
}

static bool direct_file_address_implemented(uint16_t address) {
    return dspic33ep512mu810_address_implemented(address);
}

static bool direct_file_reads_source(DirectFileOperation operation) {
    return operation != DIRECT_FILE_CLR && operation != DIRECT_FILE_SETM;
}

static bool direct_file_writes_result(DirectFileOperation operation) {
    return operation < DIRECT_FILE_CP;
}

static bool direct_file_shift_operation(DirectFileOperation operation) {
    return operation >= DIRECT_FILE_SL && operation <= DIRECT_FILE_RRC;
}

static uint16_t shift_matrix_result(DirectFileOperation operation, uint16_t source,
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

static uint16_t shift_matrix_status(DirectFileOperation operation, uint16_t source,
                                    uint16_t initial_status, bool byte_mode) {
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t value = shift_matrix_result(operation, source, initial_status, byte_mode);
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

static uint16_t direct_file_result(DirectFileOperation operation, uint16_t left,
                                   uint16_t right, uint16_t initial_status,
                                   bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;

    if (operation <= DIRECT_FILE_SUBB) {
        return binary_matrix_result((BinaryMatrixOperation)operation, left, right,
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
        return binary_matrix_result(ARITHMETIC_MATRIX_ADD, left,
                                    operation == DIRECT_FILE_INC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_DEC || operation == DIRECT_FILE_DEC2) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, left,
                                    operation == DIRECT_FILE_DEC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_NEG) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, 0u, left, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_COM) {
        return (uint16_t)(~left & mask);
    }
    if (direct_file_shift_operation(operation)) {
        return shift_matrix_result(operation, left, initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CP) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CPB) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUBB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CP0) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, left, 0u, initial_status,
                                    byte_mode);
    }
    return operation == DIRECT_FILE_SETM ? mask : 0u;
}

static uint16_t direct_file_logic_status(uint16_t initial_status, uint16_t value,
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

static uint16_t direct_file_status(DirectFileOperation operation, uint16_t left,
                                   uint16_t right, uint16_t initial_status,
                                   bool byte_mode) {
    uint16_t value =
        direct_file_result(operation, left, right, initial_status, byte_mode);

    if (operation <= DIRECT_FILE_SUBB) {
        return binary_matrix_status((BinaryMatrixOperation)operation, left, right,
                                    initial_status, byte_mode);
    }
    if (operation <= DIRECT_FILE_IOR || operation == DIRECT_FILE_COM) {
        return direct_file_logic_status(initial_status, value, byte_mode);
    }
    if (direct_file_shift_operation(operation)) {
        return shift_matrix_status(operation, left, initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_INC || operation == DIRECT_FILE_INC2) {
        return binary_matrix_status(ARITHMETIC_MATRIX_ADD, left,
                                    operation == DIRECT_FILE_INC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_DEC || operation == DIRECT_FILE_DEC2) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, left,
                                    operation == DIRECT_FILE_DEC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_NEG) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, 0u, left, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CP) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CPB) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUBB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CP0) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, left, 0u, initial_status,
                                    byte_mode);
    }
    return initial_status;
}

static void run_legal_unary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                        DirectFileOperation operation) {
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    bool reads_source = direct_file_reads_source(operation);
    uint16_t initial_status =
        (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) |
                   (((opcode >> 8u) & 1u) << 2u) | (((opcode >> 9u) & 1u) << 3u) |
                   (((opcode >> 10u) & 1u) << 8u));
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

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    if (reads_source) {
        source = binary_matrix_operand(registers, source_mode, source_register, width);
        source_operand = source.direct
                             ? (byte_mode ? (uint8_t)registers[source_register]
                                          : registers[source_register])
                             : source_value;
    }
    destination =
        binary_matrix_operand(registers, destination_mode, destination_register, width);
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
    value =
        direct_file_result(operation, source_operand, 0u, initial_status, byte_mode);
    expected_status =
        direct_file_status(operation, source_operand, 0u, initial_status, byte_mode);
    if (destination.direct) {
        if (byte_mode) {
            value = (uint16_t)((registers[destination_register] & 0xff00u) |
                               (value & 0x00ffu));
        }
        binary_matrix_write_register(registers, destination_register, value);
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!destination.direct) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, destination.address) == (uint8_t)value
                       : dspic33_read_word(cpu, destination.address) == value);
    }
    if (reads_source && !source.direct &&
        (destination.direct || destination.address != source.address)) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                       : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal unary encoding");
}

static void general_unary_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const DirectFileOperation operations[4][2] = {
        {DIRECT_FILE_INC, DIRECT_FILE_INC2},
        {DIRECT_FILE_DEC, DIRECT_FILE_DEC2},
        {DIRECT_FILE_NEG, DIRECT_FILE_COM},
        {DIRECT_FILE_CLR, DIRECT_FILE_SETM}};
    uint32_t fields;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (fields = 0u; fields < 0x040000u; fields += 257u) {
        uint32_t opcode = 0xe80000u | fields;
        uint8_t family = (uint8_t)((opcode >> 16u) - 0xe8u);
        uint8_t alternate = (uint8_t)((opcode >> 15u) & 1u);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        bool nullary = family == 3u;
        bool valid = destination_mode < 6u &&
                     (nullary ? (opcode & 0x00007fu) == 0u : source_mode < 6u);

        if (valid) {
            run_legal_unary_matrix_case(state, cpu, opcode,
                                        operations[family][alternate]);
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static uint16_t direct_file_boundary_value(uint8_t index, bool byte_mode) {
    static const uint16_t byte_values[32] = {
        0x00u, 0x01u, 0x02u, 0x0eu, 0x0fu, 0x10u, 0x7du, 0x7eu, 0x7fu, 0x80u, 0x81u,
        0xfdu, 0xfeu, 0xffu, 0x55u, 0xaau, 0x8eu, 0x8fu, 0x90u, 0x91u, 0xf0u, 0x11u,
        0x22u, 0x33u, 0x44u, 0x66u, 0x77u, 0x88u, 0x99u, 0xbbu, 0xccu, 0xddu};
    static const uint16_t word_values[32] = {
        0x0000u, 0x0001u, 0x0002u, 0x00feu, 0x00ffu, 0x0100u, 0x7ffdu, 0x7ffeu,
        0x7fffu, 0x8000u, 0x8001u, 0xfffdu, 0xfffeu, 0xffffu, 0x5555u, 0xaaaau,
        0x80feu, 0x80ffu, 0x8100u, 0x8101u, 0xff00u, 0x1111u, 0x2222u, 0x3333u,
        0x4444u, 0x6666u, 0x7777u, 0x8888u, 0x9999u, 0xbbbbu, 0xccccu, 0xddddu};

    return byte_mode ? byte_values[index & 0x1fu] : word_values[index & 0x1fu];
}

static void prepare_direct_file_case(Dspic33* cpu, uint16_t address, bool byte_mode) {
    uint8_t reg;
    uint16_t initial_status =
        (uint16_t)(((address >> 10u) & 1u) | (((address >> 11u) & 1u) << 1u));
    uint16_t operand = direct_file_boundary_value((uint8_t)(address >> 5u), byte_mode);
    uint16_t source = direct_file_boundary_value((uint8_t)address, byte_mode);

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->stop_on_trap = true;
    cpu->instructions = 0u;
    cpu->cycles = 0u;
    cpu->device_cycles = 0u;
    cpu->interrupt_count = 0u;
    cpu->software_reset_count = 0u;
    cpu->illegal_reset_count = 0u;
    cpu->trap_count = 0u;
    for (reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(
            cpu, reg, (uint16_t)(0x4200u + (uint16_t)reg * 0x0101u + address));
    }
    dspic33_set_working_register(cpu, 0u,
                                 byte_mode ? (uint16_t)(0xa500u | operand) : operand);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    cpu->sr = initial_status;
    cpu->corcon = 0x0020u;
    if (address >= 0x1000u) {
        if (byte_mode) {
            dspic33_write_word(cpu, address & 0xfffeu, 0x5aa5u);
            dspic33_write_byte(cpu, address, (uint8_t)source);
        } else {
            dspic33_write_word(cpu, address & 0xfffeu, source);
        }
    }
}

static void write_direct_file_reference_result(Dspic33* cpu, uint16_t value,
                                               bool byte_mode) {
    if (byte_mode) {
        cpu->w[0] = (uint16_t)((cpu->w[0] & 0xff00u) | (value & 0x00ffu));
    } else {
        cpu->w[0] = value;
        cpu->initialized_working_registers |= 0x0001u;
    }
    cpu->instruction_working_register_writes |= 0x0001u;
}

static bool direct_file_event_queues_match(const Dspic33* actual,
                                           const Dspic33* expected) {
    size_t index;

    if (actual->events.count != expected->events.count ||
        actual->events.sequence != expected->events.sequence) {
        return false;
    }
    for (index = 0u; index < actual->events.count; index++) {
        const Dspic33Event* actual_event = &actual->events.items[index];
        const Dspic33Event* expected_event = &expected->events.items[index];
        if (actual_event->cycle != expected_event->cycle ||
            actual_event->sequence != expected_event->sequence ||
            actual_event->paused_remaining != expected_event->paused_remaining ||
            actual_event->value != expected_event->value ||
            actual_event->source != expected_event->source ||
            actual_event->type != expected_event->type ||
            actual_event->paused != expected_event->paused) {
            return false;
        }
    }
    return true;
}

static bool direct_file_io_states_match(const Dspic33* actual,
                                        const Dspic33* expected) {
    static Dspic33Io actual_io;
    static Dspic33Io expected_io;

    memcpy(&actual_io, &actual->io, sizeof(actual_io));
    memcpy(&expected_io, &expected->io, sizeof(expected_io));
    actual_io.cpu_write_cycle = expected_io.cpu_write_cycle;
    actual_io.cpu_write_instruction = expected_io.cpu_write_instruction;
    actual_io.cpu_write_address = expected_io.cpu_write_address;
    actual_io.cpu_write_previous = expected_io.cpu_write_previous;
    actual_io.cpu_write_width = expected_io.cpu_write_width;
    actual_io.cpu_write_valid = expected_io.cpu_write_valid;
    actual_io.cpu_write_rmw = expected_io.cpu_write_rmw;
    actual_io.cpu_read_address = expected_io.cpu_read_address;
    actual_io.cpu_read_width = expected_io.cpu_read_width;
    actual_io.cpu_read_valid = expected_io.cpu_read_valid;
    return memcmp(&actual_io, &expected_io, sizeof(actual_io)) == 0;
}

static bool direct_file_states_match(const Dspic33* actual, const Dspic33* expected) {
    static Dspic33 actual_state;
    static Dspic33 expected_state;

    memcpy(&actual_state, actual, sizeof(actual_state));
    memcpy(&expected_state, expected, sizeof(expected_state));

    actual_state.program = NULL;
    actual_state.auxiliary_program = NULL;
    actual_state.persistent_program = NULL;
    actual_state.data = NULL;
    actual_state.var_write_domains = NULL;
    actual_state.events.items = NULL;
    actual_state.events.capacity = 0u;
    expected_state.program = NULL;
    expected_state.auxiliary_program = NULL;
    expected_state.persistent_program = NULL;
    expected_state.data = NULL;
    expected_state.var_write_domains = NULL;
    expected_state.events.items = NULL;
    expected_state.events.capacity = 0u;
    return memcmp(&actual_state, &expected_state, sizeof(actual_state)) == 0 &&
           memcmp(actual->data, expected->data, 0x2000u) == 0 &&
           direct_file_event_queues_match(actual, expected);
}

static uint16_t run_direct_file_reference(Dspic33* cpu, DirectFileOperation operation,
                                          uint16_t address, bool byte_mode,
                                          bool file_destination) {
    uint64_t device_ratio = dspic33_device_instruction_cycles(cpu, 1u);
    bool reads_source = direct_file_reads_source(operation);
    bool writes_result = direct_file_writes_result(operation);
    bool non_cpu_sfr = reads_source &&
                       (address & (byte_mode ? 0xffffu : 0xfffeu)) >= 0x005au &&
                       address < 0x1000u && direct_file_address_implemented(address);
    uint16_t initial_status = cpu->sr;
    uint16_t right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
    uint16_t left = 0u;
    uint16_t value;
    uint16_t status;
    uint8_t instruction_cycles = non_cpu_sfr ? 2u : 1u;
    size_t index;

    cpu->pc = 2u;
    cpu->instructions++;
    cpu->non_cpu_sfr_read = non_cpu_sfr;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    cpu->instruction_working_register_writes = 0u;
    cpu->instruction_source_address_registers = 0u;
    cpu->current_instruction_cycles = 1u;
    cpu->current_instruction_pc = 0u;
    cpu->instruction_active = true;
    if (reads_source) {
        left = byte_mode ? dspic33_read_byte(cpu, address)
                         : dspic33_read_word(cpu, address);
    }
    value = direct_file_result(operation, left, right, initial_status, byte_mode);
    status = direct_file_status(operation, left, right, initial_status, byte_mode);
    cpu->sr = status;
    if (writes_result) {
        if (file_destination) {
            if (byte_mode) {
                dspic33_write_byte(cpu, address, (uint8_t)value);
            } else {
                dspic33_write_word(cpu, address, value);
            }
        } else {
            write_direct_file_reference_result(cpu, value, byte_mode);
        }
    }
    cpu->instruction_active = false;
    cpu->previous_working_register_writes = cpu->instruction_working_register_writes;
    cpu->current_instruction_cycles = 0u;
    cpu->non_cpu_sfr_read = false;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    cpu->instruction_advancing = true;
    if (non_cpu_sfr) {
        dspic33_device_advance_instruction(cpu, 1u, device_ratio);
        dspic33_device_advance_instruction(cpu, 1u, device_ratio);
    } else {
        dspic33_device_advance_instruction(cpu, 1u, device_ratio);
    }
    for (index = 0u; index < 4u; index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay != 0u) {
            pending->delay = pending->delay > instruction_cycles
                                 ? (uint8_t)(pending->delay - instruction_cycles)
                                 : 0u;
        }
    }
    cpu->instruction_advancing = false;
    cpu->io.cpu_bus_cycle = UINT64_MAX;
    cpu->io.cpu_write_rmw = false;
    return value;
}

static const Dspic33PendingSoftTrap* direct_file_pending_trap(const Dspic33* cpu) {
    const Dspic33PendingSoftTrap* selected = NULL;
    uint8_t current_priority = (uint8_t)(((cpu->corcon & 0x0008u) != 0u ? 8u : 0u) |
                                         ((cpu->sr >> 5u) & 0x07u));
    size_t index;

    for (index = 0u; index < 4u; index++) {
        const Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay == 0u &&
            pending->priority > current_priority &&
            (selected == NULL || pending->priority > selected->priority)) {
            selected = pending;
        }
    }
    return selected;
}

static bool direct_file_trap_register_state_matches(const Dspic33* actual,
                                                    const Dspic33* expected) {
    const Dspic33PendingSoftTrap* pending = direct_file_pending_trap(expected);
    uint16_t stacked_high;
    uint16_t final_status;
    uint16_t final_control;

    if (pending == NULL) {
        return direct_file_states_match(actual, expected);
    }
    stacked_high = (uint16_t)(((expected->sr & 0x00ffu) << 8u) |
                              ((expected->corcon & 0x0008u) != 0u ? 0x0080u : 0u));
    final_status =
        (uint16_t)((expected->sr & ~0x00e0u) | ((pending->priority & 7u) << 5u));
    final_status &= (uint16_t)~0x0010u;
    final_control = pending->priority > 7u
                        ? (uint16_t)((expected->corcon & ~0x0004u) | 0x0008u)
                        : (uint16_t)(expected->corcon & ~(uint16_t)0x000cu);
    return actual->stop_reason == DSPIC33_TRAPPED &&
           actual->last_trap == pending->trap && actual->last_trap_return == 2u &&
           actual->pc == 0x000340u && actual->w[15] == 0x5004u &&
           actual->sr == final_status && actual->corcon == final_control &&
           actual->trap_count == 1u && actual->interrupt_depth == 1u &&
           memcmp(actual->w, expected->w, 15u * sizeof(*actual->w)) == 0 &&
           memcmp(actual->data, expected->data, 0x08c8u) == 0 &&
           memcmp(actual->data + 0x08cau, expected->data + 0x08cau,
                  0x2000u - 0x08cau) == 0 &&
           (uint16_t)(actual->data[0x08c8u] |
                      ((uint16_t)actual->data[0x08c9u] << 8u)) ==
               (uint16_t)(((uint16_t)pending->priority << 8u) | pending->trap) &&
           (uint16_t)(actual->data[0x5000u] |
                      ((uint16_t)actual->data[0x5001u] << 8u)) == 2u &&
           (uint16_t)(actual->data[0x5002u] |
                      ((uint16_t)actual->data[0x5003u] << 8u)) == stacked_high &&
           direct_file_io_states_match(actual, expected) &&
           direct_file_event_queues_match(actual, expected);
}

static bool run_direct_file_odd_word_case(Dspic33* cpu, Dspic33* reference,
                                          uint32_t opcode,
                                          DirectFileOperation operation,
                                          uint16_t address, bool file_destination) {
    uint16_t initial_status = reference->sr;
    uint16_t right = reference->w[0];
    bool reads_source = direct_file_reads_source(operation);
    uint16_t left = 0u;
    uint16_t value;
    uint16_t status;
    bool matches;
    bool writes_result = direct_file_writes_result(operation);
    uint64_t device_ratio = dspic33_device_instruction_cycles(reference, 1u);
    bool non_cpu_sfr = reads_source && address >= 0x005bu && address < 0x1000u &&
                       direct_file_address_implemented(address);
    uint64_t expected_cycles = reads_source && address >= 0x005bu &&
                                       address < 0x1000u &&
                                       direct_file_address_implemented(address)
                                   ? 2u
                                   : 1u;
    reference->pc = 2u;
    reference->instructions++;
    reference->non_cpu_sfr_read = non_cpu_sfr;
    reference->psv_read = false;
    reference->psv_repeat_optimized = false;
    reference->instruction_working_register_writes = 0u;
    reference->instruction_source_address_registers = 0u;
    reference->current_instruction_cycles = 1u;
    reference->current_instruction_pc = 0u;
    reference->instruction_active = true;
    if (reads_source) {
        left = dspic33_read_word(reference, address & 0xfffeu);
    }
    value = direct_file_result(operation, left, right, initial_status, false);
    status = direct_file_status(operation, left, right, initial_status, false);
    reference->sr = status;
    if (!file_destination && writes_result) {
        write_direct_file_reference_result(reference, value, false);
    }
    reference->instruction_active = false;
    reference->previous_working_register_writes =
        reference->instruction_working_register_writes;
    reference->current_instruction_cycles = 0u;
    reference->non_cpu_sfr_read = false;
    reference->psv_read = false;
    reference->psv_repeat_optimized = false;
    reference->instruction_advancing = true;
    if (non_cpu_sfr) {
        dspic33_device_advance_instruction(reference, 1u, device_ratio);
        dspic33_device_advance_instruction(reference, 1u, device_ratio);
    } else {
        dspic33_device_advance_instruction(reference, 1u, device_ratio);
    }
    reference->instruction_advancing = false;
    reference->io.cpu_bus_cycle = UINT64_MAX;
    reference->io.cpu_write_rmw = false;
    matches =
        dspic33_load_program_word(cpu, 0u, opcode) &&
        dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
        cpu->last_trap_return == 2u && cpu->pc == 0x000340u &&
        cpu->cycles == expected_cycles && cpu->w[15] == 0x5004u &&
        (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
        (dspic33_read_word(cpu, 0x5002u) >> 8u) == (status & 0x00ffu) &&
        (uint16_t)(cpu->data[0x08c8u] | ((uint16_t)cpu->data[0x08c9u] << 8u)) ==
            0x0e01u &&
        memcmp(cpu->w, reference->w, 15u * sizeof(*cpu->w)) == 0 &&
        memcmp(cpu->data, reference->data, 0x08c0u) == 0 &&
        memcmp(cpu->data + 0x08c2u, reference->data + 0x08c2u, 0x08c8u - 0x08c2u) ==
            0 &&
        memcmp(cpu->data + 0x08cau, reference->data + 0x08cau, 0x2000u - 0x08cau) ==
            0 &&
        memcmp(&cpu->nvm, &reference->nvm, sizeof(cpu->nvm)) == 0 &&
        memcmp(&cpu->oscillator, &reference->oscillator, sizeof(cpu->oscillator)) ==
            0 &&
        memcmp(&cpu->watchdog, &reference->watchdog, sizeof(cpu->watchdog)) == 0 &&
        direct_file_io_states_match(cpu, reference) &&
        direct_file_event_queues_match(cpu, reference);

    if (file_destination || !writes_result) {
        matches = matches && cpu->w[0] == right;
    } else {
        matches = matches && cpu->w[0] == value;
    }
    return matches;
}

static bool load_direct_file_trap_vectors(Dspic33* cpu) {
    static const uint32_t vectors[] = {0x000004u, 0x000006u, 0x000008u,
                                       0x00000au, 0x00000eu, 0x000010u};
    size_t index;

    for (index = 0u; index < sizeof(vectors) / sizeof(*vectors); index++) {
        if (!dspic33_load_program_word(cpu, vectors[index], 0x000340u)) {
            return false;
        }
    }
    return true;
}

static bool direct_file_flag_outcomes_complete(const bool observed[512],
                                               DirectFileOperation operation,
                                               bool byte_mode) {
    bool expected[512] = {false};
    uint32_t maximum = byte_mode ? UINT8_MAX : UINT16_MAX;
    uint32_t value;
    uint16_t status;

    for (value = 0u; value <= maximum; value++) {
        uint8_t initial;
        for (initial = 0u; initial < 4u; initial++) {
            if (operation >= DIRECT_FILE_AND && operation <= DIRECT_FILE_IOR) {
                status = direct_file_logic_status(initial, (uint16_t)value, byte_mode);
            } else {
                status = direct_file_status(operation, (uint16_t)value, 0u, initial,
                                            byte_mode);
            }
            expected[status & 0x01ffu] = true;
        }
    }
    for (status = 0u; status < 512u; status++) {
        if (expected[status] != observed[status]) {
            return false;
        }
    }
    return true;
}

static bool run_direct_file_case(Dspic33* actual, Dspic33* reference, uint32_t opcode,
                                 DirectFileOperation operation, uint16_t address,
                                 bool byte_mode, bool file_destination) {
    bool matches;

    prepare_direct_file_case(actual, address, byte_mode);
    prepare_direct_file_case(reference, address, byte_mode);
    if (!byte_mode && (address & 1u) != 0u &&
        (direct_file_reads_source(operation) || file_destination)) {
        return run_direct_file_odd_word_case(actual, reference, opcode, operation,
                                             address, file_destination);
    }
    matches = dspic33_load_program_word(actual, 0u, opcode) &&
              dspic33_load_program_word(reference, 0u, opcode);
    dspic33_step(actual);
    run_direct_file_reference(reference, operation, address, byte_mode,
                              file_destination);
    return matches && (file_destination && address >= 0x08c0u && address <= 0x08c7u
                           ? direct_file_trap_register_state_matches(actual, reference)
                           : direct_file_states_match(actual, reference));
}

static void direct_file_arithmetic_encoding_matrix_cases(TestState* state) {
    static const uint32_t bases[6] = {0xbd0000u, 0xbd8000u, 0xb40000u,
                                      0xb48000u, 0xb50000u, 0xb58000u};
    static Dspic33 actual;
    static Dspic33 reference;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);
    uint8_t operation;

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file arithmetic processors");
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
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file arithmetic address-error vectors");
    for (operation = 0u; operation < 6u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint8_t file_destination;
            for (file_destination = 0u; file_destination < 2u; file_destination++) {
                uint16_t address;
                for (address = 0u; address < 0x2000u; address += 31u) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, (DirectFileOperation)operation,
                        address, byte_mode != 0u, file_destination != 0u);
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file arithmetic encoding");
                }
            }
        }
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}

static void direct_file_logical_encoding_matrix_cases(TestState* state,
                                                      Dspic33* invalid_cpu) {
    static const uint32_t bases[3] = {0xb60000u, 0xb68000u, 0xb70000u};
    static const DirectFileOperation operations[3] = {DIRECT_FILE_AND, DIRECT_FILE_XOR,
                                                      DIRECT_FILE_IOR};
    static Dspic33 actual;
    static Dspic33 reference;
    bool flag_outcomes[3][2][512] = {{{false}}};
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);
    uint8_t operation;

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file logical processors");
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
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file logical address-error vectors");
    for (operation = 0u; operation < 3u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint8_t file_destination;
            for (file_destination = 0u; file_destination < 2u; file_destination++) {
                uint16_t address;
                for (address = 0u; address < 0x2000u; address += 31u) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);

                    if (address >= 0x1000u) {
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] =
                            true;
                    }
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file logical encoding");
                }
            }
            for (uint8_t status = 0u; status < 4u; status++) {
                for (uint8_t operand = 0u; operand < 16u; operand++) {
                    for (uint8_t source = 0u; source < 16u; source++) {
                        uint16_t address =
                            (uint16_t)(0x1000u | ((uint16_t)status << 10u) |
                                       ((uint16_t)operand << 5u) | source);
                        uint32_t opcode =
                            bases[operation] | ((uint32_t)byte_mode << 14u) | address;
                        bool matches = run_direct_file_case(
                            &actual, &reference, opcode, operations[operation], address,
                            byte_mode != 0u, false);
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] =
                            true;
                        expect_dsp_matrix_case(state, matches, opcode,
                                               "direct-file logical boundary values");
                    }
                }
            }
        }
    }
    for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (uint16_t address = 0u; address < 0x2000u; address += 31u) {
            run_invalid_binary_matrix_case(
                state, invalid_cpu, 0xb78000u | ((uint32_t)byte_mode << 14u) | address);
        }
    }
    for (operation = 0u; operation < 3u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            expect(state,
                   direct_file_flag_outcomes_complete(
                       flag_outcomes[operation][byte_mode], operations[operation],
                       byte_mode != 0u),
                   "direct-file logical flag outcomes are complete");
        }
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}

static void direct_file_unary_encoding_matrix_cases(TestState* state) {
    static const uint32_t bases[8] = {0xec0000u, 0xec8000u, 0xed0000u, 0xed8000u,
                                      0xee0000u, 0xee8000u, 0xef0000u, 0xef8000u};
    static const DirectFileOperation operations[8] = {
        DIRECT_FILE_INC, DIRECT_FILE_INC2, DIRECT_FILE_DEC, DIRECT_FILE_DEC2,
        DIRECT_FILE_NEG, DIRECT_FILE_COM,  DIRECT_FILE_CLR, DIRECT_FILE_SETM};
    static Dspic33 actual;
    static Dspic33 reference;
    bool flag_outcomes[8][2][512] = {{{false}}};
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);
    uint8_t operation;

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file unary processors");
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
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file unary address-error vectors");
    for (operation = 0u; operation < 8u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint8_t file_destination;
            for (file_destination = 0u; file_destination < 2u; file_destination++) {
                uint16_t address;
                for (address = 0u; address < 0x2000u; address += 31u) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);

                    if (address >= 0x1000u) {
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] =
                            true;
                    }
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file unary encoding");
                }
            }
        }
    }
    for (operation = 0u; operation < 8u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            expect(state,
                   direct_file_flag_outcomes_complete(
                       flag_outcomes[operation][byte_mode], operations[operation],
                       byte_mode != 0u),
                   "direct-file unary flag outcomes are exhaustive");
        }
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}

static DirectFileOperation shift_matrix_operation(uint8_t family, bool alternate) {
    if (family == 0u) {
        return DIRECT_FILE_SL;
    }
    if (family == 1u) {
        return alternate ? DIRECT_FILE_ASR : DIRECT_FILE_LSR;
    }
    if (family == 2u) {
        return alternate ? DIRECT_FILE_RLC : DIRECT_FILE_RLNC;
    }
    return alternate ? DIRECT_FILE_RRC : DIRECT_FILE_RRNC;
}

static void single_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t fields = 0u; fields < 0x040000u; fields += 257u) {
        uint32_t opcode = 0xd00000u | fields;
        uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
        bool alternate = (opcode & 0x008000u) != 0u;
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        bool valid =
            (family != 0u || !alternate) && destination_mode < 6u && source_mode < 6u;

        if (valid) {
            run_legal_unary_matrix_case(state, cpu, opcode,
                                        shift_matrix_operation(family, alternate));
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

static void direct_file_shift_encoding_matrix_cases(TestState* state,
                                                    Dspic33* invalid_cpu) {
    static const uint32_t bases[7] = {0xd40000u, 0xd50000u, 0xd58000u, 0xd60000u,
                                      0xd68000u, 0xd70000u, 0xd78000u};
    static const DirectFileOperation operations[7] = {
        DIRECT_FILE_SL,  DIRECT_FILE_LSR,  DIRECT_FILE_ASR, DIRECT_FILE_RLNC,
        DIRECT_FILE_RLC, DIRECT_FILE_RRNC, DIRECT_FILE_RRC};
    static Dspic33 actual;
    static Dspic33 reference;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file shift processors");
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
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file shift address-error vectors");
    for (uint8_t operation = 0u; operation < 7u; operation++) {
        for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            for (uint8_t file_destination = 0u; file_destination < 2u;
                 file_destination++) {
                for (uint16_t address = 0u; address < 0x2000u; address += 31u) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file shift encoding");
                }
            }
        }
    }
    for (uint32_t opcode = 0xd48000u; opcode < 0xd50000u; opcode += 257u) {
        run_invalid_binary_matrix_case(state, invalid_cpu, opcode);
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}

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

static void run_multiple_shift_matrix_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode,
                                           DirectFileOperation operation) {
    uint8_t source = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool literal = (opcode & 0x0040u) != 0u;
    uint16_t source_value = (uint16_t)(0x8001u ^ (opcode * 0x45d9u));
    uint16_t count = literal ? (uint16_t)(opcode & 0x0fu)
                             : (uint16_t)(0xa500u | ((opcode >> 7u) & 0x001fu));
    uint16_t amount = literal ? count : (uint16_t)(count & 0x001fu);
    uint16_t initial_status =
        (uint16_t)(0x0105u | ((opcode & 1u) << 1u) | (((opcode >> 7u) & 1u) << 3u));
    uint16_t expected;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, cpu->w, initial_status);
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
    matches =
        dspic33_load_program_word(cpu, 0u, opcode) &&
        dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
        cpu->cycles - cycles == 1u &&
        cpu->w[destination] == (destination == 15u ? (expected & 0xfffeu) : expected) &&
        cpu->sr == expected_status && !cpu->illegal_reset &&
        cpu->unsupported_opcode == 0u;
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

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    source = binary_matrix_operand(registers, source_mode, source_register, 2u);
    operand = source.direct ? registers[source_register] : source_value;
    if (!source.direct) {
        dspic33_write_word(cpu, source.address, source_value);
    }
    expected = find_first_result(operand, left, sign_change);
    expected_status = (uint16_t)((initial_status & ~1u) |
                                 (sign_change ? expected == 0xfff1u : expected == 0u));
    binary_matrix_write_register(registers, destination, expected);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              binary_matrix_registers_match(cpu, registers) && !cpu->illegal_reset &&
              cpu->unsupported_opcode == 0u;
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

static void run_accumulator_shift_matrix_case(TestState* state, Dspic33* cpu,
                                              uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    bool literal = (opcode & 0x0040u) != 0u;
    uint8_t encoded =
        literal ? (uint8_t)(opcode & 0x003fu) : (uint8_t)((opcode * 13u) & 0x003fu);
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
    matches =
        dspic33_load_program_word(cpu, 0u, opcode) &&
        dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
        cpu->cycles - cycles == 1u && cpu->accumulator[accumulator] == expected &&
        cpu->accumulator[accumulator ^ 1u] == (accumulator == 0u ? -0x5555 : 0x5555) &&
        !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
    if (amount < -16 || amount > 16) {
        matches =
            matches && active_pending_traps(cpu) == 1u && pending_trap(cpu, 4u) != NULL;
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
            for (uint32_t source = 0u; source <= maximum;
                 source += byte_mode != 0u ? 17u : 257u) {
                for (uint8_t carry = 0u; carry < 2u; carry++) {
                    uint32_t opcode =
                        bases[operation] | ((uint32_t)byte_mode << 14u) | 0x000182u;
                    uint16_t initial_status = (uint16_t)(0x0104u | carry);
                    uint16_t expected =
                        shift_matrix_result(operations[operation], (uint16_t)source,
                                            initial_status, byte_mode != 0u);
                    uint16_t expected_status =
                        shift_matrix_status(operations[operation], (uint16_t)source,
                                            initial_status, byte_mode != 0u);
                    bool matches;

                    cpu->pc = 0u;
                    cpu->sr = initial_status;
                    cpu->corcon = 0x0020u;
                    cpu->unsupported_opcode = 0u;
                    cpu->illegal_reset = false;
                    cpu->stop_reason = DSPIC33_RUNNING;
                    cpu->events.count = 0u;
                    cpu->w[2] = byte_mode != 0u ? (uint16_t)(0xa500u | (uint8_t)source)
                                                : (uint16_t)source;
                    cpu->w[3] = 0x5a5au;
                    if (byte_mode != 0u) {
                        expected |= 0x5a00u;
                    }
                    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                              cpu->w[3] == expected && cpu->sr == expected_status &&
                              !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "single-shift value boundary");
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
                uint16_t expected = multiple_shift_result(operations[operation],
                                                          (uint16_t)source, amount);
                uint16_t initial_status = (uint16_t)(0x0105u | ((source & 1u) << 1u) |
                                                     (((source >> 15u) & 1u) << 3u));
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
                      cpu->w[3] == expected && cpu->sr == expected_status &&
                      !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
            expect_dsp_matrix_case(state, matches, opcodes[operation],
                                   "find-first value boundary");
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
                for (size_t value_index = 0u;
                     value_index < sizeof(values) / sizeof(*values); value_index++) {
                    for (size_t amount_index = 0u;
                         amount_index < sizeof(amounts) / sizeof(*amounts);
                         amount_index++) {
                        int64_t result = accumulator_shift_matrix_result(
                            values[value_index], amounts[amount_index]);
                        int64_t minimum =
                            accumulator_saturation != 0u ? -0x8000000000ll : INT32_MIN;
                        int64_t maximum =
                            accumulator_saturation != 0u ? 0x7fffffffffll : INT32_MAX;
                        bool saturation_status =
                            result < -0x8000000000ll || result > 0x7fffffffffll;
                        uint16_t overflow_flag = accumulator == 0u ? 0x8000u : 0x4000u;
                        uint16_t saturation_flag =
                            accumulator == 0u ? 0x2000u : 0x1000u;
                        uint16_t expected_status = 0u;
                        uint16_t corcon =
                            (uint16_t)(0x0020u |
                                       (accumulator_saturation != 0u ? 0x0010u : 0u) |
                                       (saturation != 0u
                                            ? (accumulator == 0u ? 0x0080u : 0x0040u)
                                            : 0u));
                        uint8_t encoded = (uint8_t)(amounts[amount_index] & 0x3f);
                        uint32_t opcode =
                            0xc80040u | ((uint32_t)accumulator << 15u) | encoded;
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
                                  dspic33_step(cpu) == DSPIC33_RUNNING &&
                                  cpu->pc == 2u &&
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
                      cpu->accumulator[accumulator ^ 1u] == 0x5a5a &&
                      !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
            if (amount < -16 || amount > 16) {
                matches =
                    matches && (dspic33_read_word(cpu, 0x08c0u) & 0x0080u) != 0u &&
                    active_pending_traps(cpu) == 1u && pending_trap(cpu, 4u) != NULL;
            } else {
                matches = matches &&
                          (dspic33_read_word(cpu, 0x08c0u) & 0x0080u) == 0u &&
                          active_pending_traps(cpu) == 0u;
            }
            expect_dsp_matrix_case(state, matches, opcode,
                                   "accumulator-shift register count");
        }
    }
}

static void shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    single_shift_encoding_matrix_cases(state, cpu);
    direct_file_shift_encoding_matrix_cases(state, cpu);
    multiple_shift_encoding_matrix_cases(state, cpu);
    find_first_encoding_matrix_cases(state, cpu);
    accumulator_shift_encoding_matrix_cases(state, cpu);
    single_shift_value_matrix_cases(state, cpu);
    multiple_shift_value_matrix_cases(state, cpu);
    find_first_value_matrix_cases(state, cpu);
    accumulator_shift_boundary_cases(state, cpu);
    accumulator_shift_register_count_cases(state, cpu);
}

static void run_legal_compare_register_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode) {
    bool compare_zero = (opcode & 0xff0000u) == 0xe00000u;
    bool with_borrow = !compare_zero && (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x000400u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t base_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    bool literal = !compare_zero && source_mode >= 6u;
    uint16_t initial_status =
        (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) |
                   (((opcode >> 8u) & 1u) << 2u) | (((opcode >> 9u) & 1u) << 3u) |
                   (((opcode >> 10u) & 1u) << 8u));
    uint16_t registers[16];
    BinaryMatrixOperand source = {0u, true};
    uint16_t source_value = byte_mode ? (uint16_t)(0x0080u | (opcode & 0x007fu))
                                      : (uint16_t)(0x8000u | (opcode & 0x03ffu));
    uint16_t left;
    uint16_t right;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    left = compare_zero ? 0u
                        : (byte_mode ? (uint8_t)registers[base_register]
                                     : registers[base_register]);
    if (literal) {
        right = (uint16_t)(((opcode >> 2u) & 0x00e0u) | (opcode & 0x001fu));
    } else {
        source = binary_matrix_operand(registers, source_mode, source_register, width);
        right = source.direct ? (byte_mode ? (uint8_t)registers[source_register]
                                           : registers[source_register])
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
    expected_status = binary_matrix_status(with_borrow ? ARITHMETIC_MATRIX_SUBB
                                                       : ARITHMETIC_MATRIX_SUB,
                                           left, right, initial_status, byte_mode);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!literal && !source.direct) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                       : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal compare encoding");
}

static void compare_register_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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

static void compare_direct_file_encoding_matrix_cases(TestState* state,
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
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file compare address-error vectors");
    for (opcode = 0xe20000u; opcode < 0xe40000u; opcode += 257u) {
        bool compare_zero = (opcode & 0xff0000u) == 0xe20000u;
        bool valid =
            compare_zero ? (opcode & 0x00a000u) == 0u : (opcode & 0x002000u) == 0u;

        if (valid) {
            uint16_t address = (uint16_t)(opcode & 0x1fffu);
            bool byte_mode = (opcode & 0x004000u) != 0u;
            DirectFileOperation operation = compare_zero ? DIRECT_FILE_CP0
                                            : (opcode & 0x008000u) != 0u
                                                ? DIRECT_FILE_CPB
                                                : DIRECT_FILE_CP;
            bool matches = run_direct_file_case(&actual, &reference, opcode, operation,
                                                address, byte_mode, false);
            expect_dsp_matrix_case(state, matches, opcode,
                                   "direct-file compare encoding");
        } else {
            run_invalid_binary_matrix_case(state, invalid_cpu, opcode);
        }
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}

static bool compare_control_reference_taken(uint32_t opcode, uint16_t left,
                                            uint16_t right) {
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
        *left = alternate ? (byte_mode ? 0x127fu : 0x7fffu)
                          : (byte_mode ? 0x1280u : 0x8000u);
        *right = alternate ? (byte_mode ? 0x3480u : 0x8000u)
                           : (byte_mode ? 0x347fu : 0x7fffu);
    } else {
        *left = alternate ? (byte_mode ? 0x1280u : 0x8000u)
                          : (byte_mode ? 0x127fu : 0x7fffu);
        *right = alternate ? (byte_mode ? 0x347fu : 0x7fffu)
                           : (byte_mode ? 0x3480u : 0x8000u);
    }
}

static void run_compare_control_encoding_case(TestState* state, Dspic33* cpu,
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

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    compare_control_operands(opcode, alternate, &left, &right);
    binary_matrix_write_register(registers, left_register, left);
    if (right_register != left_register) {
        binary_matrix_write_register(registers, right_register, right);
    }
    for (uint8_t reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg, registers[reg]);
    }
    left = registers[left_register];
    right = registers[right_register];
    taken = compare_control_reference_taken(opcode, left, right);
    expected_pc =
        taken ? (displacement == 1
                     ? 0x2004u
                     : (uint32_t)((0x2002 + (int32_t)displacement * 2) & 0x007ffffe))
              : 0x2002u;
    expected_cycles = taken ? (displacement == 1 ? 2u : 5u) : 1u;
    cpu->pc = 0x2000u;
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x2000u, opcode) &&
              dspic33_load_program_word(cpu, 0x2002u, OPCODE_NOP) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == expected_cycles &&
              cpu->instructions - instructions == 1u && cpu->sr == initial_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    expect_dsp_matrix_case(state, matches, opcode,
                           alternate ? "compare control alternate encoding"
                                     : "compare control primary encoding");
}

static void compare_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xe60000u; opcode < 0xe80000u; opcode += 257u) {
        run_compare_control_encoding_case(state, cpu, opcode, false);
        run_compare_control_encoding_case(state, cpu, opcode, true);
    }
}

static void compare_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    compare_register_encoding_matrix_cases(state, cpu);
    compare_direct_file_encoding_matrix_cases(state, cpu);
    compare_control_encoding_matrix_cases(state, cpu);
}

static bool group_selected(int argc, char** argv, const char* group) {
    return argc == 1 || (argc == 2 && strcmp(argv[1], group) == 0);
}

int main(int argc, char** argv) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "processor data test initializes");
    if (initialized) {
        if (group_selected(argc, argv, "loop_bit")) {
            loop_encoding_matrix_cases(&state, &cpu);
            bit_encoding_matrix_cases(&state, &cpu);
            direct_file_bit_value_cases(&state, &cpu);
            bit_operand_lifecycle_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "table_system")) {
            table_encoding_matrix_cases(&state, &cpu);
            table_value_cases(&state, &cpu);
            table_operand_lifecycle_cases(&state, &cpu);
            system_encoding_matrix_cases(&state, &cpu);
            system_control_value_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "divide_decimal")) {
            divide_encoding_matrix_cases(&state, &cpu);
            decimal_adjust_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "arithmetic")) {
            arithmetic_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "shift")) {
            shift_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "stack_unary")) {
            byte_extension_encoding_matrix_cases(&state, &cpu);
            byte_extension_value_matrix_cases(&state, &cpu);
            byte_extension_lifecycle_cases(&state, &cpu);
            direct_stack_encoding_matrix_cases(&state, &cpu);
            direct_stack_value_cases(&state, &cpu);
            link_encoding_matrix_cases(&state, &cpu);
            shadow_stack_encoding_cases(&state, &cpu);
            general_unary_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "compare")) {
            compare_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "direct_arithmetic")) {
            direct_file_arithmetic_encoding_matrix_cases(&state);
        }
        if (group_selected(argc, argv, "direct_logical")) {
            direct_file_logical_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "direct_unary")) {
            direct_file_unary_encoding_matrix_cases(&state);
        }
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
