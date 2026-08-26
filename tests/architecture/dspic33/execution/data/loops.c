#include "architecture/dspic33/execution/data/internal.h"

static void repeat_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t count;
    uint8_t reg;

    for (count = 0u; count <= 0x7fffu; count += 257u) {
        bool matches;
        uint32_t opcode = 0x090000u | count;
        reset_processor_test(cpu, 0x200u);
        matches = dspic33_load_program_word(cpu, 0x200u, opcode) &&
                  dspic33_load_program_word(cpu, 0x202u, OPCODE_NOP) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u && cpu->cycles == 1u &&
                  cpu->rcount == count && cpu->repeat_active == (count != 0u) &&
                  (cpu->sr & 0x0010u) == (count != 0u ? 0x0010u : 0u) &&
                  cpu->repeat_pc == (count != 0u ? 0x202u : 0u) && cpu->unsupported_opcode == 0u;
        expect_dsp_matrix_case(state, matches, opcode, "REPEAT literal encoding");
    }

    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value;
        uint32_t opcode = 0x098000u | reg;
        bool matches;
        reset_processor_test(cpu, 0x200u);
        dspic33_set_working_register(cpu, reg, reg == 0u ? 0u : (uint16_t)(0x1111u * reg));
        value = cpu->w[reg];
        matches = dspic33_load_program_word(cpu, 0x200u, opcode) &&
                  dspic33_load_program_word(cpu, 0x202u, OPCODE_NOP) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u && cpu->cycles == 1u &&
                  cpu->rcount == value && cpu->repeat_active == (value != 0u) &&
                  (cpu->sr & 0x0010u) == (value != 0u ? 0x0010u : 0u) &&
                  cpu->repeat_pc == (value != 0u ? 0x202u : 0u) && cpu->unsupported_opcode == 0u;
        expect_dsp_matrix_case(state, matches, opcode, "REPEAT register encoding");
    }

    reset_processor_test(cpu, 0x200u);
    expect_dsp_matrix_case(state,
                           dspic33_load_program_word(cpu, 0x200u, 0x098010u) &&
                               dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                               cpu->unsupported_opcode == 0x098010u && cpu->pc == 0x200u,
                           0x098010u, "REPEAT reserved register encoding");
}

static void run_do_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode, uint16_t count,
                                 uint32_t extension, uint32_t expected_end, const char* domain) {
    bool matches;
    reset_processor_test(cpu, 0x20000u);
    matches = dspic33_load_program_word(cpu, 0x20000u, opcode) &&
              dspic33_load_program_word(cpu, 0x20002u, extension) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20004u && cpu->cycles == 2u &&
              cpu->do_depth == 1u && cpu->do_count[0] == count && cpu->dcount == count &&
              cpu->do_start[0] == 0x20004u && cpu->dostart == 0x20004u &&
              cpu->do_end[0] == expected_end && cpu->doend == expected_end &&
              cpu->corcon == 0x0120u && (cpu->sr & 0x0200u) != 0u && cpu->unsupported_opcode == 0u;
    expect_dsp_matrix_case(state, matches, opcode, domain);
}

static void do_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t count;
    uint32_t extension;
    uint8_t reg;

    for (count = 0u; count <= 0x7fffu; count += 257u) {
        run_do_encoding_case(state, cpu, 0x080000u | count, (uint16_t)count, 2u, 0x20008u,
                             "DO literal encoding");
    }

    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value;
        uint32_t opcode = 0x088000u | reg;
        reset_processor_test(cpu, 0x20000u);
        dspic33_set_working_register(cpu, reg, reg == 0u ? 0u : (uint16_t)(0x1111u * reg));
        value = cpu->w[reg];
        expect_dsp_matrix_case(
            state,
            dspic33_load_program_word(cpu, 0x20000u, opcode) &&
                dspic33_load_program_word(cpu, 0x20002u, 2u) &&
                dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20004u && cpu->cycles == 2u &&
                cpu->do_depth == 1u && cpu->do_count[0] == value && cpu->dcount == value &&
                cpu->do_start[0] == 0x20004u && cpu->do_end[0] == 0x20008u &&
                cpu->dostart == 0x20004u && cpu->doend == 0x20008u && cpu->corcon == 0x0120u &&
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
        expect_dsp_matrix_case(state,
                               dspic33_load_program_word(cpu, 0x20000u, OPCODE_DO_0) &&
                                   dspic33_load_program_word(cpu, 0x20002u, invalid_extension) &&
                                   dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                                   cpu->unsupported_opcode == OPCODE_DO_0 && cpu->pc == 0x20000u &&
                                   cpu->cycles == 0u && cpu->do_depth == 0u,
                               invalid_extension, "DO literal reserved extension");

        reset_processor_test(cpu, 0x20000u);
        dspic33_set_working_register(cpu, 0u, 0xaaaau);
        expect_dsp_matrix_case(state,
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
                               cpu->unsupported_opcode == 0x088010u && cpu->pc == 0x20000u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->do_depth == 0u && cpu->w[2] == 2u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->do_depth == 0u && cpu->w[2] == 2u,
           "active DOEND write changes the loop completion boundary");

    load_three_instruction_do(state, cpu, 1u);
    dspic33_write_byte(cpu, 0x003eu, 0x0au);
    dspic33_write_byte(cpu, 0x003fu, 0x02u);
    dspic33_write_byte(cpu, 0x0040u, 0x01u);
    expect(state, cpu->doend == 0x01020au && cpu->do_end[0] == 0x01020au,
           "DOEND byte writes update every implemented address field");
}

static void run_do_to_completion(TestState* state, Dspic33* cpu, uint32_t expected_increments,
                                 const char* name) {
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
    run_do_to_completion(state, cpu, 2u,
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
        run_do_to_completion(state, &copy, 2u, "copied late-EDT state completes independently");
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
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 6u && cpu->pc == 0x000340u,
           "fifth nested DO enters the generic stack-overflow trap");
    expect(state,
           cpu->do_depth == 4u && memcmp(cpu->do_start, starts, sizeof(starts)) == 0 &&
               memcmp(cpu->do_end, ends, sizeof(ends)) == 0 &&
               memcmp(cpu->do_count, counts, sizeof(counts)) == 0 && cpu->dostart == starts[3] &&
               cpu->doend == ends[3] && cpu->dcount == counts[3] && cpu->corcon == 0x0428u,
           "fifth nested DO leaves every active loop unchanged");
    expect(state,
           (dspic33_read_word(cpu, 0x08c4u) & 0x0010u) != 0u && active_pending_traps(cpu) == 1u &&
               cpu->do_depth == 4u,
           "fifth nested DO sets DOOVR and retains its level-sensitive source");
}

static void prepare_nested_zero_do_case(TestState* state, Dspic33* cpu, uint16_t inner_count,
                                        bool first_outer_nop, bool second_outer_nop,
                                        bool inner_nop) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x080001u);
    load_instruction(state, cpu, 0x202u, 6u);
    load_instruction(state, cpu, 0x204u, first_outer_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x206u, second_outer_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu && cpu->do_depth == 2u &&
               cpu->do_count[1] == 0u,
           "nested zero-count DO accepts the documented NOP workaround");
    for (uint8_t step = 0u; step < 3u && cpu->do_depth == 2u; step++) {
        dspic33_step(cpu);
    }
    expect(state,
           cpu->do_depth == 1u && cpu->dostart == cpu->do_start[0] &&
               cpu->doend == cpu->do_end[0] && cpu->dcount == cpu->do_count[0],
           "completed inner DO restores the outer loop registers");

    prepare_nested_zero_do_case(state, cpu, 0u, true, true, true);
    load_instruction(state, cpu, 0x208u, OPCODE_DO_W0);
    dspic33_set_working_register(cpu, 0u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu && cpu->do_depth == 2u &&
               cpu->do_count[1] == 0u,
           "nested zero-count register DO accepts the documented NOP workaround");

    prepare_nested_zero_do_case(state, cpu, 1u, false, false, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu && cpu->do_depth == 2u &&
               cpu->do_count[1] == 1u,
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

void dspic33_data_test_loop_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    repeat_encoding_matrix_cases(state, cpu);
    do_encoding_matrix_cases(state, cpu);
    do_register_control_cases(state, cpu);
    do_early_termination_cases(state, cpu);
    do_stack_overflow_cases(state, cpu);
    nested_zero_do_erratum_cases(state, cpu);
}

static bool binary_matrix_with_carry(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_SUBBR || operation == ARITHMETIC_MATRIX_ADDC ||
           operation == ARITHMETIC_MATRIX_SUBB;
}

static bool binary_matrix_reverse(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_SUBR || operation == ARITHMETIC_MATRIX_SUBBR;
}

static bool binary_matrix_addition(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_ADD || operation == ARITHMETIC_MATRIX_ADDC;
}

bool dspic33_data_test_binary_matrix_logical(BinaryMatrixOperation operation) {
    return operation >= ARITHMETIC_MATRIX_AND;
}

uint16_t dspic33_data_test_binary_matrix_result(BinaryMatrixOperation operation, uint16_t left,
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
        result = (uint32_t)left + right + (binary_matrix_with_carry(operation) ? carry : 0u);
    } else if (binary_matrix_reverse(operation)) {
        result = (uint16_t)(right - left - borrow);
    } else {
        result = (uint16_t)(left - right - borrow);
    }
    return (uint16_t)(result & mask);
}

uint16_t dspic33_data_test_binary_matrix_status(BinaryMatrixOperation operation, uint16_t left,
                                                uint16_t right, uint16_t initial_status,
                                                bool byte_mode) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t digit_mask = byte_mode ? 0x000fu : 0x00ffu;
    uint16_t carry = (initial_status & 0x0001u) != 0u ? 1u : 0u;
    uint16_t borrow = binary_matrix_with_carry(operation) && carry == 0u ? 1u : 0u;
    uint16_t add_carry =
        binary_matrix_addition(operation) && binary_matrix_with_carry(operation) ? carry : 0u;
    bool sticky_zero = binary_matrix_with_carry(operation);
    uint16_t status = (uint16_t)(initial_status & ~0x010fu);
    uint16_t value;

    left = (uint16_t)(left & mask);
    right = (uint16_t)(right & mask);
    value =
        dspic33_data_test_binary_matrix_result(operation, left, right, initial_status, byte_mode);
    if (dspic33_data_test_binary_matrix_logical(operation)) {
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
        int32_t signed_minuend = byte_mode ? (int8_t)minuend : (int16_t)minuend;
        int32_t signed_subtrahend = byte_mode ? (int8_t)subtrahend : (int16_t)subtrahend;
        int32_t signed_result = signed_minuend - signed_subtrahend - borrow;
        int32_t minimum = byte_mode ? INT8_MIN : INT16_MIN;
        int32_t maximum = byte_mode ? INT8_MAX : INT16_MAX;
        if (minuend >= subtraction) {
            status |= 0x0001u;
        }
        if ((uint32_t)(minuend & digit_mask) >= (uint32_t)(subtrahend & digit_mask) + borrow) {
            status |= 0x0100u;
        }
        if (signed_result < minimum || signed_result > maximum) {
            status |= 0x0004u;
        }
    }
    return status;
}

void dspic33_data_test_binary_matrix_write_register(uint16_t registers[16], uint8_t reg,
                                                    uint16_t value) {
    registers[reg] = reg == 15u ? (uint16_t)(value & 0xfffeu) : value;
}

BinaryMatrixOperand dspic33_data_test_binary_matrix_operand(uint16_t registers[16], uint8_t mode,
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
        adjusted = (int32_t)registers[reg] + (mode == 3u ? (int32_t)width : -(int32_t)width);
        dspic33_data_test_binary_matrix_write_register(registers, reg, (uint16_t)adjusted);
        return operand;
    }
    adjusted = (int32_t)registers[reg] + (mode == 5u ? (int32_t)width : -(int32_t)width);
    operand.address = (uint16_t)adjusted;
    dspic33_data_test_binary_matrix_write_register(registers, reg, (uint16_t)adjusted);
    return operand;
}

void dspic33_data_test_prepare_arithmetic_matrix_case(Dspic33* cpu, uint16_t registers[16],
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

bool dspic33_data_test_binary_matrix_registers_match(const Dspic33* cpu,
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

static void run_legal_byte_extension_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
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

    dspic33_data_test_prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    source = dspic33_data_test_binary_matrix_operand(registers, source_mode, source_register, 1u);
    if (source.direct) {
        source_value = (uint8_t)registers[source_register];
    } else {
        dspic33_write_byte(cpu, source.address, source_value);
    }
    value = zero_extend ? source_value : (uint16_t)(int16_t)(int8_t)source_value;
    dspic33_data_test_binary_matrix_write_register(registers, destination, value);
    value = registers[destination];
    expected_status = byte_extension_status(initial_status, value);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX &&
              dspic33_data_test_binary_matrix_registers_match(cpu, registers);
    if (!source.direct) {
        matches = matches && dspic33_read_byte(cpu, source.address) == source_value;
    }
    expect_dsp_matrix_case(state, matches, opcode, "SE and ZE legal encoding");
}

static void run_invalid_byte_extension_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
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
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->illegal_reset && cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->pc == 0u && cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX &&
              cpu->trap_count == 0u && (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u;
    expect_dsp_matrix_case(state, matches, opcode, "SE and ZE reserved encoding");
}

void dspic33_data_test_byte_extension_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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

void dspic33_data_test_byte_extension_value_matrix_cases(TestState* state, Dspic33* cpu) {
    uint16_t value;
    uint8_t status_bits;
    uint8_t operation;

    for (operation = 0u; operation < 2u; operation++) {
        for (value = 0u; value <= 0xffu; value++) {
            for (status_bits = 0u; status_bits < 8u; status_bits++) {
                uint32_t opcode = 0xfb0182u | ((uint32_t)operation << 15u);
                uint16_t initial_status =
                    (uint16_t)(0x0104u | (status_bits & 0x03u) | ((status_bits & 0x04u) << 1u));
                uint16_t expected = operation != 0u ? value : (uint16_t)(int16_t)(int8_t)value;
                uint16_t expected_status = byte_extension_status(initial_status, expected);
                uint64_t cycles;
                bool matches;

                dspic33_data_test_prepare_arithmetic_matrix_case(cpu, cpu->w, initial_status);
                dspic33_set_working_register(cpu, 2u, value);
                cycles = cpu->cycles;
                matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->cycles - cycles == 1u && cpu->w[3] == expected &&
                          cpu->sr == expected_status && !cpu->illegal_reset &&
                          cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode, "SE and ZE value and status");
            }
        }
    }
}
