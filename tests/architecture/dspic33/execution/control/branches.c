#include "architecture/dspic33/execution/control/internal.h"

static uint16_t dsp_matrix_base_value(uint8_t reg) {
    static const uint16_t values[4] = {0x5008u, 0x5108u, 0x9008u, 0x9108u};
    return values[reg - 8u];
}

static uint16_t dsp_matrix_prefetch_value(bool y_space, uint8_t operation) {
    return (uint16_t)((y_space ? 0x2200u : 0x1100u) | operation);
}

static void prepare_dsp_matrix_case(Dspic33* cpu, uint8_t target_accumulator, uint8_t x_operation,
                                    uint8_t y_operation, uint16_t expected_w[14]) {
    static const uint16_t values[10] = {
        2u, 3u, 5u, 7u, 0x5008u, 0x5108u, 0x9008u, 0x9108u, 2u, 0x5200u,
    };
    const DspMatrixPrefetch* x = &dsp_matrix_prefetches[x_operation];
    const DspMatrixPrefetch* y = &dsp_matrix_prefetches[y_operation];
    uint8_t reg;

    cpu->pc = 0u;
    cpu->sr = 0x000fu;
    cpu->corcon = 0x0001u;
    cpu->accumulator[target_accumulator] = 100;
    cpu->accumulator[target_accumulator ^ 1u] = 0x12348001;
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
    for (reg = 4u; reg <= 13u; reg++) {
        dspic33_set_working_register(cpu, reg, values[reg - 4u]);
        expected_w[reg] = values[reg - 4u];
    }
    dspic33_write_word(cpu, 0x5200u, 0xa5a5u);
    if (x->present) {
        uint8_t base = (uint8_t)(8u + x->register_offset);
        dspic33_write_word(cpu, (uint16_t)(dsp_matrix_base_value(base) + x->access_offset),
                           dsp_matrix_prefetch_value(false, x_operation));
    }
    if (y->present) {
        uint8_t base = (uint8_t)(10u + y->register_offset);
        dspic33_write_word(cpu, (uint16_t)(dsp_matrix_base_value(base) + y->access_offset),
                           dsp_matrix_prefetch_value(true, y_operation));
    }
}
static void apply_dsp_matrix_prefetch(uint16_t expected_w[14], uint8_t operation,
                                      uint8_t destination, bool y_space, bool write_destination) {
    const DspMatrixPrefetch* prefetch = &dsp_matrix_prefetches[operation];
    uint8_t base;
    if (!prefetch->present) {
        return;
    }
    base = (uint8_t)((y_space ? 10u : 8u) + prefetch->register_offset);
    expected_w[base] = (uint16_t)(dsp_matrix_base_value(base) + prefetch->update);
    if (write_destination) {
        expected_w[destination] = dsp_matrix_prefetch_value(y_space, operation);
    }
}

static bool status_branch_reference_taken(uint8_t condition, uint16_t status) {
    bool carry = (status & 0x0001u) != 0u;
    bool zero = (status & 0x0002u) != 0u;
    bool overflow = (status & 0x0004u) != 0u;
    bool negative = (status & 0x0008u) != 0u;
    switch (condition) {
    case 0u:
        return overflow;
    case 1u:
        return carry;
    case 2u:
        return zero;
    case 3u:
        return negative;
    case 4u:
        return zero || negative != overflow;
    case 5u:
        return negative != overflow;
    case 6u:
        return !carry || zero;
    case 7u:
        return true;
    case 8u:
        return !overflow;
    case 9u:
        return !carry;
    case 10u:
        return !zero;
    case 11u:
        return !negative;
    case 12u:
        return !zero && negative == overflow;
    case 13u:
        return negative == overflow;
    default:
        return carry && !zero;
    }
}

static uint16_t accumulator_branch_status(uint8_t flags) {
    uint16_t status =
        (uint16_t)(((uint16_t)(flags & 0x01u) << 15u) | ((uint16_t)(flags & 0x02u) << 13u) |
                   ((uint16_t)(flags & 0x04u) << 11u) | ((uint16_t)(flags & 0x08u) << 9u));
    if ((status & 0xc000u) != 0u) {
        status |= 0x0800u;
    }
    if ((status & 0x3000u) != 0u) {
        status |= 0x0400u;
    }
    return (uint16_t)(status | 0x010fu);
}

static void prepare_relative_branch_case(Dspic33* cpu, uint16_t status) {
    cpu->pc = 0x020000u;
    cpu->sr = status;
    cpu->corcon = 0x0020u;
    cpu->w[0] = 0x1357u;
    cpu->w[15] = 0x5000u;
    cpu->initialized_working_registers = 0x8001u;
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
}

static void run_relative_branch_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                              uint16_t status, bool taken) {
    int16_t displacement = (int16_t)opcode;
    uint32_t expected_pc =
        taken ? (uint32_t)((0x020002 + (int32_t)displacement * 2) & 0x007ffffe) : 0x020002u;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_relative_branch_case(cpu, status);
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == (taken ? 4u : 1u) && cpu->instructions - instructions == 1u &&
              cpu->sr == status && cpu->corcon == 0x0020u && cpu->w[0] == 0x1357u &&
              cpu->w[15] == 0x5000u && cpu->initialized_working_registers == 0x8001u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "conditional branch encoding");
}

void dspic33_control_test_conditional_branch_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    bool outcomes[19][2] = {{false}};
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x0c0000u; opcode < 0x100000u; opcode += 257u) {
        uint8_t family = (uint8_t)(opcode >> 16u);
        uint8_t flags;
        for (flags = 0u; flags < 16u; flags++) {
            uint16_t status = accumulator_branch_status(flags);
            bool taken = (status & (uint16_t)(0x8000u >> (family - 0x0cu))) != 0u;
            run_relative_branch_encoding_case(state, cpu, opcode, status, taken);
            outcomes[15u + family - 0x0cu][taken ? 1u : 0u] = true;
        }
    }
    for (opcode = 0x300000u; opcode < 0x3f0000u; opcode += 257u) {
        uint8_t condition = (uint8_t)((opcode >> 16u) & 0x0fu);
        uint8_t flags;
        for (flags = 0u; flags < 16u; flags++) {
            uint16_t status = (uint16_t)(0x0100u | flags);
            bool taken = status_branch_reference_taken(condition, status);
            run_relative_branch_encoding_case(state, cpu, opcode, status, taken);
            outcomes[condition][taken ? 1u : 0u] = true;
        }
    }
    for (opcode = 0x3f0000u; opcode < 0x400000u; opcode += 257u) {
        run_invalid_binary_matrix_case(state, cpu, opcode);
    }
    for (uint8_t family = 0u; family < 19u; family++) {
        bool complete = outcomes[family][1u] && (family == 7u || outcomes[family][0u]);
        expect(state, complete, "conditional branch outcomes cover each family");
    }
}

typedef enum {
    COMPUTED_CONTROL_INVALID,
    COMPUTED_CONTROL_CALL,
    COMPUTED_CONTROL_RCALL,
    COMPUTED_CONTROL_GOTO,
    COMPUTED_CONTROL_BRA,
    COMPUTED_CONTROL_CALL_LONG,
    COMPUTED_CONTROL_GOTO_LONG,
} ComputedControlKind;

static ComputedControlKind computed_control_reference_kind(uint32_t opcode, uint8_t* source) {
    if ((opcode & 0xfffff0u) == 0x010000u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_CALL;
    }
    if ((opcode & 0xfffff0u) == 0x010200u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_RCALL;
    }
    if ((opcode & 0xfffff0u) == 0x010400u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_GOTO;
    }
    if ((opcode & 0xfffff0u) == 0x010600u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_BRA;
    }
    *source = (uint8_t)opcode & 0x0fu;
    if ((*source & 1u) == 0u && *source <= 12u) {
        uint32_t base = 0x018000u | ((uint32_t)(*source + 1u) << 11u) | *source;
        if (opcode == base) {
            return COMPUTED_CONTROL_CALL_LONG;
        }
        if (opcode == (base | 0x000400u)) {
            return COMPUTED_CONTROL_GOTO_LONG;
        }
    }
    return COMPUTED_CONTROL_INVALID;
}

static void run_computed_control_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                               ComputedControlKind kind, uint8_t source) {
    bool call = kind == COMPUTED_CONTROL_CALL || kind == COMPUTED_CONTROL_RCALL ||
                kind == COMPUTED_CONTROL_CALL_LONG;
    uint16_t initial_registers[16];
    uint32_t target;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    cpu->pc = 0x002000u;
    cpu->sr = 0xf10fu;
    cpu->corcon = 0x0024u;
    cpu->call_depth = 0u;
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
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        uint16_t value = (kind == COMPUTED_CONTROL_RCALL || kind == COMPUTED_CONTROL_BRA)
                             ? (reg < 8u ? (uint16_t)(0x0010u + reg) : (uint16_t)(0xffe0u + reg))
                             : (uint16_t)(0x3001u + (uint16_t)reg * 2u);
        dspic33_set_working_register(cpu, reg, value);
        initial_registers[reg] = value;
    }
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    initial_registers[15] = 0x5000u;
    if (kind == COMPUTED_CONTROL_CALL_LONG || kind == COMPUTED_CONTROL_GOTO_LONG) {
        dspic33_set_working_register(cpu, source, (uint16_t)(0x3001u + source * 2u));
        dspic33_set_working_register(cpu, (uint8_t)(source + 1u), 0u);
        initial_registers[source] = cpu->w[source];
        initial_registers[source + 1u] = 0u;
    }
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    if (kind == COMPUTED_CONTROL_RCALL || kind == COMPUTED_CONTROL_BRA) {
        uint16_t displacement =
            source == 15u && kind == COMPUTED_CONTROL_RCALL ? 0x5004u : initial_registers[source];
        target = (uint32_t)((0x002002 + (int32_t)(int16_t)displacement * 2) & 0x007ffffe);
    } else if (kind == COMPUTED_CONTROL_CALL_LONG || kind == COMPUTED_CONTROL_GOTO_LONG) {
        target = initial_registers[source] & 0xfffeu;
    } else {
        target = (source == 15u && call ? 0x5004u : initial_registers[source]) & 0xfffeu;
    }
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x002000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == target &&
              cpu->cycles - cycles == 4u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0xf10fu && cpu->corcon == (call ? 0x0020u : 0x0024u) &&
              cpu->w[15] == (call ? 0x5004u : 0x5000u) && cpu->call_depth == (call ? 1u : 0u) &&
              cpu->unsupported_opcode == 0u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX;
    if (call) {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0x2003u &&
                  dspic33_read_word(cpu, 0x5002u) == 0u;
    } else {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
                  dspic33_read_word(cpu, 0x5002u) == 0x5a5au;
    }
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == initial_registers[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "computed control encoding");
}

void dspic33_control_test_computed_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x010000u; opcode < 0x020000u; opcode += 257u) {
        uint8_t source;
        ComputedControlKind kind = computed_control_reference_kind(opcode, &source);
        if (kind == COMPUTED_CONTROL_INVALID) {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        } else {
            run_computed_control_encoding_case(state, cpu, opcode, kind, source);
        }
    }
}

static void prepare_literal_control_encoding_case(Dspic33* cpu) {
    cpu->pc = 0x020000u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0024u;
    cpu->call_depth = 0u;
    cpu->w[15] = 0x5000u;
    cpu->initialized_working_registers = 0x8000u;
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
    cpu->stop_on_trap = false;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
}

static void run_literal_control_encoding_case(TestState* state, Dspic33* cpu, bool call,
                                              uint16_t low, uint8_t high) {
    uint32_t opcode = (call ? 0x020000u : 0x040000u) | low;
    uint32_t target = (((uint32_t)high << 16u) | low) & 0x007ffffeu;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_literal_control_encoding_case(cpu);
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_load_program_word(cpu, 0x020002u, high) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == target &&
              cpu->cycles - cycles == 4u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0x010fu && cpu->corcon == (call ? 0x0020u : 0x0024u) &&
              cpu->w[15] == (call ? 0x5004u : 0x5000u) && cpu->call_depth == (call ? 1u : 0u) &&
              cpu->unsupported_opcode == 0u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX;
    if (call) {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0x0005u &&
                  dspic33_read_word(cpu, 0x5002u) == 0x0002u;
    } else {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
                  dspic33_read_word(cpu, 0x5002u) == 0x5a5au;
    }
    expect_dsp_matrix_case(state, matches, opcode, "literal control encoding");
}

static void run_literal_control_target_fault_case(TestState* state, Dspic33* cpu, bool call,
                                                  uint16_t low, uint8_t high) {
    uint32_t opcode = (call ? 0x020000u : 0x040000u) | low;
    uint64_t cycles;
    bool matches;

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    cpu->corcon |= 0x0004u;
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, high) && dspic33_step(cpu) == DSPIC33_TRAPPED &&
              cpu->cycles - cycles == 4u && cpu->last_trap == 1u && cpu->last_trap_return == 2u &&
              cpu->pc == 0x000340u && cpu->w[15] == (call ? 0x5008u : 0x5004u) &&
              (cpu->corcon & 0x0004u) == (call ? 0u : 0x0004u);
    if (call) {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0x0005u &&
                  dspic33_read_word(cpu, 0x5002u) == 0u && dspic33_read_word(cpu, 0x5004u) == 2u;
    } else {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 2u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "literal control target fault");
}

static void run_reserved_literal_extension_case(TestState* state, Dspic33* cpu, bool call,
                                                uint32_t extension) {
    uint32_t opcode = call ? 0x020246u : 0x040246u;
    uint64_t illegal_resets;
    bool matches;

    reset_processor_test(cpu, 0u);
    cpu->corcon |= 0x0004u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, extension) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->call_depth == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    expect_dsp_matrix_case(state, matches, extension, "literal control reserved extension");
}

static void run_reserved_literal_first_word_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    reset_processor_test(cpu, 0u);
    cpu->corcon |= 0x0004u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    illegal_resets = cpu->illegal_reset_count;
    matches =
        dspic33_load_program_word(cpu, 0u, opcode) && dspic33_load_program_word(cpu, 2u, 0u) &&
        dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
        cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u && cpu->w[15] == 0x1000u &&
        cpu->initialized_working_registers == 0x8000u && cpu->last_trap == UINT16_MAX &&
        cpu->call_depth == 0u && (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
        dspic33_read_word(cpu, 0x5000u) == 0xa5a5u && dspic33_read_word(cpu, 0x5002u) == 0x5a5au;
    expect_dsp_matrix_case(state, matches, opcode, "literal control reserved first word");
}

static void run_literal_rcall_encoding_case(TestState* state, Dspic33* cpu, uint16_t displacement) {
    uint32_t opcode = 0x070000u | displacement;
    uint32_t target = (uint32_t)((0x020002 + (int32_t)(int16_t)displacement * 2) & 0x007ffffe);
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_literal_control_encoding_case(cpu);
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == target &&
              cpu->cycles - cycles == 4u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0x010fu && cpu->corcon == 0x0020u && cpu->w[15] == 0x5004u &&
              cpu->call_depth == 1u && dspic33_read_word(cpu, 0x5000u) == 0x0003u &&
              dspic33_read_word(cpu, 0x5002u) == 0x0002u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "literal RCALL encoding");
}

void dspic33_control_test_literal_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint8_t call = 0u; call < 2u; call++) {
        for (uint32_t low = 0u; low <= UINT16_MAX; low += 257u) {
            uint32_t opcode = (call != 0u ? 0x020000u : 0x040000u) | low;
            if ((low & 1u) == 0u) {
                run_literal_control_encoding_case(state, cpu, call != 0u, (uint16_t)low, 0u);
            } else {
                run_reserved_literal_first_word_case(state, cpu, opcode);
            }
        }
        for (uint16_t high = 0u; high < 128u; high++) {
            uint16_t low = high == 127u ? 0xc000u : 0x1234u;
            uint32_t target = ((uint32_t)high << 16u) | low;
            if (dspic33_program_range_implemented(target, 2u)) {
                run_literal_control_encoding_case(state, cpu, call != 0u, low, (uint8_t)high);
            } else {
                run_literal_control_target_fault_case(state, cpu, call != 0u, low, (uint8_t)high);
            }
        }
        for (uint32_t upper = 1u; upper < 0x20000u; upper += 257u) {
            uint32_t extension = (upper << 7u) | (upper & 0x007fu);
            run_reserved_literal_extension_case(state, cpu, call != 0u, extension);
        }
    }
    for (uint32_t displacement = 0u; displacement <= UINT16_MAX; displacement += 257u) {
        run_literal_rcall_encoding_case(state, cpu, (uint16_t)displacement);
    }
}

static void prepare_return_encoding_case(Dspic33* cpu) {
    cpu->pc = 0x020000u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    cpu->initialized_working_registers = 0x8000u;
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
    cpu->stop_on_trap = false;
    dspic33_write_word(cpu, 0x5000u, 0x0301u);
    dspic33_write_word(cpu, 0x5002u, 0u);
}

static void run_retlw_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint16_t literal = (uint16_t)((opcode >> 4u) & 0x03ffu);
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint16_t expected = byte_mode ? (uint16_t)(literal & 0x00ffu) : literal;
    uint16_t expected_stack = destination == 15u
                                  ? (uint16_t)(((byte_mode ? 0x5000u : 0u) | expected) & 0xfffeu)
                                  : 0x5000u;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_return_encoding_case(cpu);
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(cpu, reg, (uint16_t)(0xa500u | reg));
    }
    if (byte_mode && destination != 15u) {
        expected |= 0xa500u;
    }
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
              cpu->cycles - cycles == 6u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0x010fu && cpu->corcon == 0x0024u && cpu->w[15] == expected_stack &&
              cpu->call_depth == 0u && cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    if (destination != 15u) {
        matches = matches && cpu->w[destination] == expected;
    }
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        if (reg != destination) {
            matches = matches && cpu->w[reg] == (uint16_t)(0xa500u | reg);
        }
    }
    expect_dsp_matrix_case(state, matches, opcode, "RETLW encoding");
}

static void exact_return_encoding_cases(TestState* state, Dspic33* cpu) {
    uint64_t cycles;
    bool matches;

    prepare_return_encoding_case(cpu);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0x020000u, OPCODE_RETURN) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
              cpu->cycles - cycles == 6u && cpu->w[15] == 0x5000u && cpu->call_depth == 0u &&
              cpu->sr == 0x010fu && cpu->corcon == 0x0024u &&
              dspic33_get_executed_program_counter(cpu) == 0x020000u;
    expect_dsp_matrix_case(state, matches, OPCODE_RETURN, "RETURN encoding");

    prepare_return_encoding_case(cpu);
    cpu->call_depth = 0u;
    cpu->interrupt_depth = 1u;
    dspic33_write_word(cpu, 0x5000u, 0x0301u);
    dspic33_write_word(cpu, 0x5002u, 0x0f80u);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0x020000u, OPCODE_RETFIE) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
              cpu->cycles - cycles == 6u && cpu->w[15] == 0x5000u && cpu->interrupt_depth == 0u &&
              cpu->sr == 0x010fu && cpu->corcon == 0x002cu &&
              dspic33_get_executed_program_counter(cpu) == 0x020000u;
    expect_dsp_matrix_case(state, matches, OPCODE_RETFIE, "RETFIE encoding");
}

void dspic33_control_test_return_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0x050000u; opcode < 0x058000u; opcode += 257u) {
        run_retlw_encoding_case(state, cpu, opcode);
    }
    for (uint32_t opcode = 0x058000u; opcode < 0x060000u; opcode += 257u) {
        run_invalid_binary_matrix_case(state, cpu, opcode);
    }
    for (uint32_t opcode = 0x060000u; opcode < 0x070000u; opcode += 257u) {
        if (opcode == OPCODE_RETURN || opcode == OPCODE_RETFIE) {
            continue;
        }
        run_invalid_binary_matrix_case(state, cpu, opcode);
    }
    exact_return_encoding_cases(state, cpu);
}

void dspic33_control_test_run_legal_dsp_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                                    uint8_t target_accumulator,
                                                    int64_t target_result, uint8_t x_operation,
                                                    uint8_t y_operation, uint8_t x_destination,
                                                    uint8_t y_destination, uint8_t write_back,
                                                    int8_t difference_destination) {
    uint16_t expected_w[14] = {0u};
    uint16_t expected_memory = 0xa5a5u;
    uint64_t cycles;
    bool matches;
    uint8_t reg;

    prepare_dsp_matrix_case(cpu, target_accumulator, x_operation, y_operation, expected_w);
    apply_dsp_matrix_prefetch(expected_w, x_operation, x_destination, false,
                              difference_destination < 0);
    apply_dsp_matrix_prefetch(expected_w, y_operation, y_destination, true,
                              difference_destination < 0);
    if (difference_destination >= 0) {
        expected_w[(uint8_t)difference_destination] =
            (uint16_t)(dsp_matrix_prefetch_value(false, x_operation) -
                       dsp_matrix_prefetch_value(true, y_operation));
    }
    if (write_back == DSP_MATRIX_WRITE_BACK_DIRECT) {
        expected_w[13] = 0x1235u;
    } else if (write_back == DSP_MATRIX_WRITE_BACK_INDIRECT) {
        expected_w[13] = 0x5202u;
        expected_memory = 0x1235u;
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->pc == 2u && cpu->cycles - cycles == 1u &&
              cpu->accumulator[target_accumulator] == target_result &&
              cpu->accumulator[target_accumulator ^ 1u] == 0x12348001 && cpu->sr == 0x000fu &&
              cpu->corcon == 0x0001u && dspic33_read_word(cpu, 0x5200u) == expected_memory &&
              !cpu->address_error && !cpu->illegal_reset && cpu->unsupported_opcode == 0u &&
              cpu->last_trap == UINT16_MAX;
    for (reg = 4u; reg <= 13u; reg++) {
        matches = matches && cpu->w[reg] == expected_w[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal DSP encoding");
}

void dspic33_control_test_general_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t pair_encodings[6] = {0u, 1u, 2u, 4u, 5u, 6u};
    static const uint8_t pair_left[6] = {0u, 0u, 0u, 1u, 1u, 2u};
    static const uint8_t pair_right[6] = {1u, 2u, 3u, 2u, 3u, 3u};
    static const int64_t operands[4] = {2, 3, 5, 7};
    static const struct {
        uint16_t bits;
        int8_t sign;
        bool replace;
        uint8_t write_back;
    } forms[8] = {
        {0x0003u, 1, true, DSP_MATRIX_WRITE_BACK_NONE},
        {0x4003u, -1, true, DSP_MATRIX_WRITE_BACK_NONE},
        {0x0002u, 1, false, DSP_MATRIX_WRITE_BACK_NONE},
        {0x0000u, 1, false, DSP_MATRIX_WRITE_BACK_DIRECT},
        {0x0001u, 1, false, DSP_MATRIX_WRITE_BACK_INDIRECT},
        {0x4002u, -1, false, DSP_MATRIX_WRITE_BACK_NONE},
        {0x4000u, -1, false, DSP_MATRIX_WRITE_BACK_DIRECT},
        {0x4001u, -1, false, DSP_MATRIX_WRITE_BACK_INDIRECT},
    };
    uint8_t pair;
    uint8_t accumulator;
    uint8_t form;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t x_destination;
    uint8_t y_destination;

    for (pair = 0u; pair < 6u; pair++) {
        int64_t product = operands[pair_left[pair]] * operands[pair_right[pair]];
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (form = 0u; form < 8u; form++) {
                int64_t result = forms[form].sign * product;
                if (!forms[form].replace) {
                    result += 100;
                }
                for (x_operation = 0u; x_operation < 16u; x_operation++) {
                    for (y_operation = x_operation; y_operation <= x_operation; y_operation++) {
                        for (x_destination = 0u; x_destination < 4u; x_destination++) {
                            for (y_destination = 0u; y_destination < 4u; y_destination++) {
                                uint32_t opcode = 0xc00000u |
                                                  ((uint32_t)pair_encodings[pair] << 16u) |
                                                  ((uint32_t)accumulator << 15u) |
                                                  ((uint32_t)x_destination << 12u) |
                                                  ((uint32_t)y_destination << 10u) |
                                                  ((uint32_t)x_operation << 6u) |
                                                  ((uint32_t)y_operation << 2u) | forms[form].bits;
                                dspic33_control_test_run_legal_dsp_matrix_case(
                                    state, cpu, opcode, accumulator, result, x_operation,
                                    y_operation, (uint8_t)(4u + x_destination),
                                    (uint8_t)(4u + y_destination), forms[form].write_back, -1);
                            }
                        }
                    }
                }
            }
        }
    }
}
