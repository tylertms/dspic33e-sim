#include "processor_test_support.h"

typedef struct {
    uint8_t register_offset;
    int8_t access_offset;
    int8_t update;
    bool present;
} DspMatrixPrefetch;

enum {
    DSP_MATRIX_WRITE_BACK_DIRECT = 0u,
    DSP_MATRIX_WRITE_BACK_INDIRECT = 1u,
    DSP_MATRIX_WRITE_BACK_NONE = 2u
};

static const DspMatrixPrefetch dsp_matrix_prefetches[16] = {
    {0u, 0, 0, true},  {0u, 0, 2, true},  {0u, 0, 4, true},  {0u, 0, 6, true},
    {0u, 0, 0, false}, {0u, 0, -6, true}, {0u, 0, -4, true}, {0u, 0, -2, true},
    {1u, 0, 0, true},  {1u, 0, 2, true},  {1u, 0, 4, true},  {1u, 0, 6, true},
    {1u, 2, 0, true},  {1u, 0, -6, true}, {1u, 0, -4, true}, {1u, 0, -2, true},
};

static uint16_t dsp_matrix_base_value(uint8_t reg) {
    static const uint16_t values[4] = {0x5008u, 0x5108u, 0x9008u, 0x9108u};
    return values[reg - 8u];
}

static uint16_t dsp_matrix_prefetch_value(bool y_space, uint8_t operation) {
    return (uint16_t)((y_space ? 0x2200u : 0x1100u) | operation);
}

static void prepare_dsp_matrix_case(Dspic33* cpu, uint8_t target_accumulator,
                                    uint8_t x_operation, uint8_t y_operation,
                                    uint16_t expected_w[14]) {
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
        dspic33_write_word(cpu,
                           (uint16_t)(dsp_matrix_base_value(base) + x->access_offset),
                           dsp_matrix_prefetch_value(false, x_operation));
    }
    if (y->present) {
        uint8_t base = (uint8_t)(10u + y->register_offset);
        dspic33_write_word(cpu,
                           (uint16_t)(dsp_matrix_base_value(base) + y->access_offset),
                           dsp_matrix_prefetch_value(true, y_operation));
    }
}

static void apply_dsp_matrix_prefetch(uint16_t expected_w[14], uint8_t operation,
                                      uint8_t destination, bool y_space,
                                      bool write_destination) {
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
    uint16_t status = (uint16_t)(((uint16_t)(flags & 0x01u) << 15u) |
                                 ((uint16_t)(flags & 0x02u) << 13u) |
                                 ((uint16_t)(flags & 0x04u) << 11u) |
                                 ((uint16_t)(flags & 0x08u) << 9u));
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

static void run_relative_branch_encoding_case(TestState* state, Dspic33* cpu,
                                              uint32_t opcode, uint16_t status,
                                              bool taken) {
    int16_t displacement = (int16_t)opcode;
    uint32_t expected_pc =
        taken ? (uint32_t)((0x020002 + (int32_t)displacement * 2) & 0x007ffffe)
              : 0x020002u;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_relative_branch_case(cpu, status);
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == (taken ? 4u : 1u) &&
              cpu->instructions - instructions == 1u && cpu->sr == status &&
              cpu->corcon == 0x0020u && cpu->w[0] == 0x1357u && cpu->w[15] == 0x5000u &&
              cpu->initialized_working_registers == 0x8001u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "conditional branch encoding");
}

static void conditional_branch_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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

static ComputedControlKind computed_control_reference_kind(uint32_t opcode,
                                                           uint8_t* source) {
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

static void run_computed_control_encoding_case(TestState* state, Dspic33* cpu,
                                               uint32_t opcode,
                                               ComputedControlKind kind,
                                               uint8_t source) {
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
        uint16_t value =
            (kind == COMPUTED_CONTROL_RCALL || kind == COMPUTED_CONTROL_BRA)
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
        uint16_t displacement = source == 15u && kind == COMPUTED_CONTROL_RCALL
                                    ? 0x5004u
                                    : initial_registers[source];
        target =
            (uint32_t)((0x002002 + (int32_t)(int16_t)displacement * 2) & 0x007ffffe);
    } else if (kind == COMPUTED_CONTROL_CALL_LONG ||
               kind == COMPUTED_CONTROL_GOTO_LONG) {
        target = initial_registers[source] & 0xfffeu;
    } else {
        target =
            (source == 15u && call ? 0x5004u : initial_registers[source]) & 0xfffeu;
    }
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x002000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == target &&
              cpu->cycles - cycles == 4u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0xf10fu && cpu->corcon == (call ? 0x0020u : 0x0024u) &&
              cpu->w[15] == (call ? 0x5004u : 0x5000u) &&
              cpu->call_depth == (call ? 1u : 0u) && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
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

static void computed_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
              cpu->w[15] == (call ? 0x5004u : 0x5000u) &&
              cpu->call_depth == (call ? 1u : 0u) && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
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

static void run_literal_control_target_fault_case(TestState* state, Dspic33* cpu,
                                                  bool call, uint16_t low,
                                                  uint8_t high) {
    uint32_t opcode = (call ? 0x020000u : 0x040000u) | low;
    uint64_t cycles;
    bool matches;

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    cpu->corcon |= 0x0004u;
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, high) &&
              dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles - cycles == 4u &&
              cpu->last_trap == 1u && cpu->last_trap_return == 2u &&
              cpu->pc == 0x000340u && cpu->w[15] == (call ? 0x5008u : 0x5004u) &&
              (cpu->corcon & 0x0004u) == (call ? 0u : 0x0004u);
    if (call) {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0x0005u &&
                  dspic33_read_word(cpu, 0x5002u) == 0u &&
                  dspic33_read_word(cpu, 0x5004u) == 2u;
    } else {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 2u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "literal control target fault");
}

static void run_reserved_literal_extension_case(TestState* state, Dspic33* cpu,
                                                bool call, uint32_t extension) {
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
    expect_dsp_matrix_case(state, matches, extension,
                           "literal control reserved extension");
}

static void run_reserved_literal_first_word_case(TestState* state, Dspic33* cpu,
                                                 uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    reset_processor_test(cpu, 0u);
    cpu->corcon |= 0x0004u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, 0u) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->call_depth == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
              dspic33_read_word(cpu, 0x5002u) == 0x5a5au;
    expect_dsp_matrix_case(state, matches, opcode,
                           "literal control reserved first word");
}

static void run_literal_rcall_encoding_case(TestState* state, Dspic33* cpu,
                                            uint16_t displacement) {
    uint32_t opcode = 0x070000u | displacement;
    uint32_t target =
        (uint32_t)((0x020002 + (int32_t)(int16_t)displacement * 2) & 0x007ffffe);
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
              dspic33_read_word(cpu, 0x5002u) == 0x0002u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "literal RCALL encoding");
}

static void literal_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint8_t call = 0u; call < 2u; call++) {
        for (uint32_t low = 0u; low <= UINT16_MAX; low += 257u) {
            uint32_t opcode = (call != 0u ? 0x020000u : 0x040000u) | low;
            if ((low & 1u) == 0u) {
                run_literal_control_encoding_case(state, cpu, call != 0u, (uint16_t)low,
                                                  0u);
            } else {
                run_reserved_literal_first_word_case(state, cpu, opcode);
            }
        }
        for (uint16_t high = 0u; high < 128u; high++) {
            uint16_t low = high == 127u ? 0xc000u : 0x1234u;
            uint32_t target = ((uint32_t)high << 16u) | low;
            if (dspic33_program_range_implemented(target, 2u)) {
                run_literal_control_encoding_case(state, cpu, call != 0u, low,
                                                  (uint8_t)high);
            } else {
                run_literal_control_target_fault_case(state, cpu, call != 0u, low,
                                                      (uint8_t)high);
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
    uint16_t expected_stack =
        destination == 15u
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
              cpu->sr == 0x010fu && cpu->corcon == 0x0024u &&
              cpu->w[15] == expected_stack && cpu->call_depth == 0u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
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
              cpu->cycles - cycles == 6u && cpu->w[15] == 0x5000u &&
              cpu->call_depth == 0u && cpu->sr == 0x010fu && cpu->corcon == 0x0024u;
    expect_dsp_matrix_case(state, matches, OPCODE_RETURN, "RETURN encoding");

    prepare_return_encoding_case(cpu);
    cpu->call_depth = 0u;
    cpu->interrupt_depth = 1u;
    dspic33_write_word(cpu, 0x5000u, 0x0301u);
    dspic33_write_word(cpu, 0x5002u, 0x0f80u);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0x020000u, OPCODE_RETFIE) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
              cpu->cycles - cycles == 6u && cpu->w[15] == 0x5000u &&
              cpu->interrupt_depth == 0u && cpu->sr == 0x010fu &&
              cpu->corcon == 0x002cu;
    expect_dsp_matrix_case(state, matches, OPCODE_RETFIE, "RETFIE encoding");
}

static void return_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

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

static void run_legal_dsp_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                      uint8_t target_accumulator, int64_t target_result,
                                      uint8_t x_operation, uint8_t y_operation,
                                      uint8_t x_destination, uint8_t y_destination,
                                      uint8_t write_back,
                                      int8_t difference_destination) {
    uint16_t expected_w[14] = {0u};
    uint16_t expected_memory = 0xa5a5u;
    uint64_t cycles;
    bool matches;
    uint8_t reg;

    prepare_dsp_matrix_case(cpu, target_accumulator, x_operation, y_operation,
                            expected_w);
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
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u &&
              cpu->accumulator[target_accumulator] == target_result &&
              cpu->accumulator[target_accumulator ^ 1u] == 0x12348001 &&
              cpu->sr == 0x000fu && cpu->corcon == 0x0001u &&
              dspic33_read_word(cpu, 0x5200u) == expected_memory &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX;
    for (reg = 4u; reg <= 13u; reg++) {
        matches = matches && cpu->w[reg] == expected_w[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal DSP encoding");
}

static void general_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                    for (y_operation = x_operation; y_operation <= x_operation;
                         y_operation++) {
                        for (x_destination = 0u; x_destination < 4u; x_destination++) {
                            for (y_destination = 0u; y_destination < 4u;
                                 y_destination++) {
                                uint32_t opcode =
                                    0xc00000u |
                                    ((uint32_t)pair_encodings[pair] << 16u) |
                                    ((uint32_t)accumulator << 15u) |
                                    ((uint32_t)x_destination << 12u) |
                                    ((uint32_t)y_destination << 10u) |
                                    ((uint32_t)x_operation << 6u) |
                                    ((uint32_t)y_operation << 2u) | forms[form].bits;
                                run_legal_dsp_matrix_case(state, cpu, opcode,
                                                          accumulator, result,
                                                          x_operation, y_operation,
                                                          (uint8_t)(4u + x_destination),
                                                          (uint8_t)(4u + y_destination),
                                                          forms[form].write_back, -1);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void special_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                            for (y_destination = 0u; y_destination < 4u;
                                 y_destination++) {
                                uint32_t opcode =
                                    families[family] | ((uint32_t)accumulator << 15u) |
                                    ((uint32_t)x_destination << 12u) |
                                    ((uint32_t)y_destination << 10u) |
                                    ((uint32_t)x_operation << 6u) |
                                    ((uint32_t)y_operation << 2u) | write_back;
                                run_legal_dsp_matrix_case(
                                    state, cpu, opcode, accumulator,
                                    family == 0u ? 0 : 100, x_operation, y_operation,
                                    (uint8_t)(4u + x_destination),
                                    (uint8_t)(4u + y_destination), write_back, -1);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void square_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                            for (y_destination = 0u; y_destination < 4u;
                                 y_destination++) {
                                uint32_t opcode =
                                    0xf00000u | ((uint32_t)source << 16u) |
                                    ((uint32_t)accumulator << 15u) |
                                    ((uint32_t)x_destination << 12u) |
                                    ((uint32_t)y_destination << 10u) |
                                    ((uint32_t)x_operation << 6u) |
                                    ((uint32_t)y_operation << 2u) | replace;
                                run_legal_dsp_matrix_case(
                                    state, cpu, opcode, accumulator, result,
                                    x_operation, y_operation,
                                    (uint8_t)(4u + x_destination),
                                    (uint8_t)(4u + y_destination),
                                    DSP_MATRIX_WRITE_BACK_NONE, -1);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void euclidean_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                        for (y_operation = x_operation; y_operation <= x_operation;
                             y_operation++) {
                            uint32_t opcode;
                            if (y_operation == 4u) {
                                continue;
                            }
                            opcode = 0xf04000u | ((uint32_t)source << 16u) |
                                     ((uint32_t)accumulator << 15u) |
                                     ((uint32_t)destination << 12u) |
                                     ((uint32_t)x_operation << 6u) |
                                     ((uint32_t)y_operation << 2u) | operation;
                            run_legal_dsp_matrix_case(
                                state, cpu, opcode, accumulator, result, x_operation,
                                y_operation, 4u, 4u, DSP_MATRIX_WRITE_BACK_NONE,
                                (int8_t)(4u + destination));
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
        dspic33_set_working_register(cpu, reg,
                                     (uint16_t)(0x5000u + (uint16_t)reg * 2u));
    }
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
}

static void run_invalid_dsp_matrix_case(TestState* state, Dspic33* cpu,
                                        uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;
    uint8_t reg;

    prepare_invalid_dsp_matrix_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u &&
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

static void invalid_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                                                     : low >= 2u &&
                                                           y_destination == 0u &&
                                                           x_operation != 4u &&
                                                           y_operation != 4u;
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
                                          uint8_t source_register,
                                          uint16_t expected_w[16]) {
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

static void run_legal_generic_multiply_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode, bool base_signed,
                                            bool source_signed, uint8_t base_register,
                                            uint8_t destination, uint8_t source_mode,
                                            uint8_t source_register) {
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
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u &&
              cpu->accumulator[0] == expected_accumulator[0] &&
              cpu->accumulator[1] == expected_accumulator[1] && cpu->sr == 0x010fu &&
              cpu->corcon == 0x0005u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX;
    for (reg = 0u; reg < 16u; reg++) {
        matches = matches && cpu->w[reg] == expected_w[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal generic multiply encoding");
}

static void generic_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                        for (source_register = 0u; source_register < 16u;
                             source_register += 5u) {
                            uint32_t opcode =
                                0xb80000u | ((uint32_t)base_signed << 16u) |
                                ((uint32_t)source_signed << 15u) |
                                ((uint32_t)base_register << 11u) |
                                ((uint32_t)destination << 7u) |
                                ((uint32_t)source_mode << 4u) | source_register;
                            if (source_signed != 0u && source_mode >= 6u) {
                                run_invalid_dsp_matrix_case(state, cpu, opcode);
                            } else {
                                run_legal_generic_multiply_case(
                                    state, cpu, opcode, base_signed != 0u,
                                    source_signed != 0u, base_register, destination,
                                    source_mode, source_register);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void file_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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
                matches = matches && dspic33_step(cpu) == DSPIC33_TRAPPED &&
                          cpu->last_trap == 1u && cpu->last_trap_return == 2u &&
                          cpu->pc == 0x000340u &&
                          (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u;
            } else {
                matches = matches && dspic33_step(cpu) == DSPIC33_RUNNING &&
                          cpu->pc == 2u && cpu->w[2] == 0u &&
                          cpu->w[3] == (byte_mode != 0u ? 0x5a5au : 0u) &&
                          cpu->sr == 0x010fu && cpu->last_trap == UINT16_MAX &&
                          cpu->unsupported_opcode == 0u;
            }
            expect_dsp_matrix_case(state, matches, opcode, "file multiply encoding");
        }
    }
}

static void prepare_move_matrix_case(Dspic33* cpu) {
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

static void run_invalid_move_matrix_case(TestState* state, Dspic33* cpu,
                                         uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;
    uint8_t reg;

    prepare_invalid_dsp_matrix_case(cpu);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u &&
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

static void move_literal_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
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

            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, destination, 0x5a5au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
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
            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, destination, 0x5a5au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
            expect_dsp_matrix_case(state, matches, opcode, "MOV byte literal encoding");
        }
    }
}

static void move_register_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xfd0000u; opcode <= 0xfd0fffu; opcode++) {
        bool legal = (opcode & 0xfff870u) == 0xfd0000u;
        if (!legal) {
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t source = (uint8_t)(opcode & 0x0fu);
            uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
            uint16_t source_value = (uint16_t)(0x1100u | source);
            uint16_t destination_value = (uint16_t)(0x2200u | destination);
            uint16_t expected_source;
            uint16_t expected_destination;
            uint64_t cycles;
            bool matches;

            prepare_move_matrix_case(cpu);
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
                      cpu->w[destination] == expected_destination &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            expect_dsp_matrix_case(state, matches, opcode, "EXCH encoding");
        }
    }

    for (opcode = 0xfd8000u; opcode <= 0xfdffffu; opcode++) {
        bool legal = (opcode & 0xffbff0u) == 0xfd8000u;
        if (!legal) {
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t reg = (uint8_t)(opcode & 0x0fu);
            bool byte_mode = (opcode & 0x004000u) != 0u;
            uint16_t expected = byte_mode ? 0xa5a5u : 0x5aa5u;
            uint64_t cycles;
            bool matches;
            if (reg == 15u) {
                expected &= 0xfffeu;
            }
            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, reg, 0xa55au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[reg] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            expect_dsp_matrix_case(state, matches, opcode, "SWAP encoding");
        }
    }
}

static void movpag_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xfec000u; opcode <= 0xfecfffu; opcode++) {
        uint8_t page_register = (uint8_t)((opcode >> 10u) & 3u);
        uint16_t literal = (uint16_t)(opcode & 0x03ffu);
        if (page_register == 3u) {
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint16_t expected = page_register == 0u   ? literal
                                : page_register == 1u ? (uint16_t)(literal & 0x01ffu)
                                                      : (uint16_t)(literal & 0x00ffu);
            uint64_t cycles;
            bool matches;
            prepare_move_matrix_case(cpu);
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
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t source = (uint8_t)(opcode & 0x0fu);
            uint16_t value = (uint16_t)(0x03a0u | source);
            uint16_t expected;
            uint64_t cycles;
            bool matches;
            prepare_move_matrix_case(cpu);
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

typedef struct {
    uint16_t address;
    bool direct;
} MoveMatrixOperand;

static MoveMatrixOperand resolve_move_matrix_operand(uint16_t registers[16],
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
        adjusted =
            (int32_t)registers[reg] + (mode == 3u ? (int32_t)width : -(int32_t)width);
        registers[reg] = reg == 15u ? (uint16_t)adjusted & 0xfffeu : (uint16_t)adjusted;
        return operand;
    }
    if (mode == 4u || mode == 5u) {
        adjusted =
            (int32_t)registers[reg] + (mode == 5u ? (int32_t)width : -(int32_t)width);
        operand.address = (uint16_t)adjusted;
        registers[reg] = reg == 15u ? (uint16_t)adjusted & 0xfffeu : (uint16_t)adjusted;
        return operand;
    }
    operand.address = (uint16_t)(registers[reg] + registers[offset_reg]);
    return operand;
}

static void prepare_move_registers(Dspic33* cpu, uint16_t expected[16], uint16_t base,
                                   uint16_t stride) {
    uint8_t reg;
    prepare_move_matrix_case(cpu);
    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value = (uint16_t)(base + (uint16_t)reg * stride);
        dspic33_set_working_register(cpu, reg, value);
        expected[reg] = cpu->w[reg];
    }
}

static bool move_matrix_registers_match(const Dspic33* cpu,
                                        const uint16_t expected[16]) {
    uint8_t reg;
    for (reg = 0u; reg < 16u; reg++) {
        if (cpu->w[reg] != expected[reg]) {
            return false;
        }
    }
    return true;
}

static void generic_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x780000u; opcode <= 0x7fffffu; opcode += 257u) {
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t offset_register = (uint8_t)((opcode >> 15u) & 0x0fu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        uint8_t width = byte_mode ? 1u : 2u;
        uint16_t expected[16];
        MoveMatrixOperand source;
        MoveMatrixOperand destination;
        uint16_t value;
        uint64_t cycles;
        bool matches;

        prepare_move_registers(cpu, expected, 0x2000u, 2u);
        source = resolve_move_matrix_operand(expected, source_mode, source_register,
                                             offset_register, width);
        if (source.direct) {
            value = byte_mode ? (uint8_t)expected[source_register]
                              : expected[source_register];
        } else if (byte_mode) {
            dspic33_write_byte(cpu, source.address, 0xa5u);
            value = 0x00a5u;
        } else {
            dspic33_write_word(cpu, source.address, 0xa55au);
            value = 0xa55au;
        }
        destination = resolve_move_matrix_operand(
            expected, destination_mode, destination_register, offset_register, width);
        if (destination.direct) {
            if (byte_mode) {
                expected[destination_register] =
                    (uint16_t)((expected[destination_register] & 0xff00u) | value);
            } else {
                expected[destination_register] = value;
            }
            if (destination_register == 15u) {
                expected[destination_register] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 1u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX &&
                  move_matrix_registers_match(cpu, expected);
        if (!destination.direct) {
            matches =
                matches &&
                (byte_mode ? dspic33_read_byte(cpu, destination.address) == value
                           : dspic33_read_word(cpu, destination.address) == value);
        }
        expect_dsp_matrix_case(state, matches, opcode, "generic MOV encoding");
    }
}

static int16_t move_offset_literal(uint32_t opcode, bool byte_mode) {
    uint16_t encoded =
        (uint16_t)((((opcode >> 15u) & 0x0fu) << 6u) |
                   (((opcode >> 11u) & 0x07u) << 3u) | ((opcode >> 4u) & 0x07u));
    int16_t offset = (int16_t)(encoded | ((encoded & 0x0200u) != 0u ? 0xfc00u : 0u));
    return byte_mode ? offset : (int16_t)(offset * 2);
}

static void offset_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x900000u; opcode <= 0x9fffffu; opcode += 257u) {
        uint8_t source = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        bool store = (opcode & 0x080000u) != 0u;
        int16_t offset = move_offset_literal(opcode, byte_mode);
        uint16_t expected[16];
        uint16_t address;
        uint16_t value;
        uint64_t cycles;
        bool matches;

        prepare_move_registers(cpu, expected, 0x4000u, 2u);
        if (store) {
            address = (uint16_t)(expected[destination] + offset);
            value = byte_mode ? (uint8_t)expected[source] : expected[source];
        } else {
            address = (uint16_t)(expected[source] + offset);
            value = byte_mode ? 0x00a5u : 0xa55au;
            if (byte_mode) {
                dspic33_write_byte(cpu, address, (uint8_t)value);
                expected[destination] =
                    (uint16_t)((expected[destination] & 0xff00u) | value);
            } else {
                dspic33_write_word(cpu, address, value);
                expected[destination] = value;
            }
            if (destination == 15u) {
                expected[destination] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 1u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX &&
                  move_matrix_registers_match(cpu, expected);
        if (store) {
            matches = matches && (byte_mode ? dspic33_read_byte(cpu, address) == value
                                            : dspic33_read_word(cpu, address) == value);
        }
        expect_dsp_matrix_case(state, matches, opcode, "offset MOV encoding");
    }
}

static void move_double_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xbe0000u; opcode <= 0xbeffffu; opcode += 257u) {
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
        bool load = (opcode & 0xfff880u) == 0xbe0000u && source_mode <= 5u &&
                    (source_mode != 0u || (source_register & 1u) == 0u);
        bool store = (opcode & 0xffc071u) == 0xbe8000u && destination_mode >= 1u &&
                     destination_mode <= 5u;
        if (!load && !store) {
            run_invalid_move_matrix_case(state, cpu, opcode);
            continue;
        }
        uint16_t expected[16];
        MoveMatrixOperand source;
        MoveMatrixOperand destination;
        uint16_t low;
        uint16_t high;
        uint64_t cycles;
        bool matches;

        prepare_move_registers(cpu, expected, 0x3000u, 4u);
        source =
            resolve_move_matrix_operand(expected, source_mode, source_register, 0u, 4u);
        if (source.direct) {
            source_register &= 0x0eu;
            low = expected[source_register];
            high = expected[source_register + 1u];
        } else {
            dspic33_write_word(cpu, source.address, 0x1111u);
            dspic33_write_word(cpu, (uint16_t)(source.address + 2u), 0x2222u);
            low = 0x1111u;
            high = 0x2222u;
        }
        destination = resolve_move_matrix_operand(expected, destination_mode,
                                                  destination_register, 0u, 4u);
        if (destination.direct) {
            destination_register &= 0x0eu;
            expected[destination_register] = low;
            expected[destination_register + 1u] = high;
            if (destination_register + 1u == 15u) {
                expected[15] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 2u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX &&
                  move_matrix_registers_match(cpu, expected);
        if (!destination.direct) {
            matches =
                matches && dspic33_read_word(cpu, destination.address) == low &&
                dspic33_read_word(cpu, (uint16_t)(destination.address + 2u)) == high;
        }
        expect_dsp_matrix_case(state, matches, opcode, "MOV.D encoding");
        if (load) {
        } else {
        }
    }
}

static void move_data_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_load_program_word(cpu, 0x000006u, 0x000340u),
           "load direct data MOV address trap vector");
    for (opcode = 0x800000u; opcode <= 0x8fffffu; opcode += 257u) {
        bool store = (opcode & 0x080000u) != 0u;
        uint8_t reg = (uint8_t)(opcode & 0x0fu);
        uint16_t encoded_address =
            (uint16_t)((((opcode >> 4u) & 0x7fffu) << 1u) & 0xffffu);
        uint32_t address;
        uint16_t transfer_value;
        uint64_t cycles;
        Dspic33StopReason reason;
        bool matches;

        dspic33_reset(cpu, 0u);
        dspic33_set_async_events(cpu, false);
        prepare_move_matrix_case(cpu);
        cpu->stop_on_trap = true;
        cpu->dsrpag = 1u;
        cpu->dswpag = 1u;
        dspic33_set_working_register(cpu, 15u, 0x5000u);
        address = encoded_address < 0x8000u
                      ? encoded_address
                      : (uint32_t)(0x8000u | (encoded_address & 0x7fffu));
        if (store) {
            dspic33_set_working_register(
                cpu, reg, address >= 0x1000u ? (uint16_t)(0xa500u | reg) : 0u);
            transfer_value = cpu->w[reg];
        } else {
            dspic33_set_working_register(cpu, reg, 0x5a5au);
            transfer_value = 0xa55au;
            if (address >= 0x1000u && encoded_address < 0xe000u) {
                dspic33_write_word(cpu, address, 0xa55au);
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode);
        reason = dspic33_step(cpu);
        if (encoded_address >= 0xe000u) {
            matches = matches && reason == DSPIC33_TRAPPED && cpu->pc == 0x000340u &&
                      cpu->last_trap == 1u && cpu->last_trap_return == 2u;
        } else {
            matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->last_trap == UINT16_MAX;
        }
        matches = matches && cpu->cycles > cycles && cpu->unsupported_opcode == 0u &&
                  !cpu->illegal_reset;
        if (address >= 0x1000u && encoded_address < 0xe000u) {
            matches = matches &&
                      (store ? dspic33_read_word(cpu, address) == transfer_value
                             : cpu->w[reg] == (reg == 15u ? (transfer_value & 0xfffeu)
                                                          : transfer_value));
        }
        expect_dsp_matrix_case(state, matches, opcode, "direct data MOV encoding");
    }
}

static uint16_t move_matrix_logic_status(uint16_t status, uint16_t value,
                                         bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    value &= mask;
    status &= (uint16_t)~0x000au;
    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    return status;
}

static uint16_t move_matrix_file_value(uint16_t address, bool byte_mode) {
    switch (address & 3u) {
    case 0u:
        return 0u;
    case 1u:
        return byte_mode ? 0x0080u : 0x8000u;
    default:
        return byte_mode ? 0x0034u : 0x1234u;
    }
}

static void file_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint16_t address;
    uint8_t byte_mode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_load_program_word(cpu, 0x000006u, 0x000340u),
           "load file MOV address trap vector");
    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (address = 0u; address < 0x2000u; address += 31u) {
            uint32_t opcode = 0xb7a000u | ((uint32_t)byte_mode << 14u) | address;
            uint64_t cycles;
            Dspic33StopReason reason;
            bool matches;

            dspic33_reset(cpu, 0u);
            dspic33_set_async_events(cpu, false);
            prepare_move_matrix_case(cpu);
            cpu->stop_on_trap = true;
            dspic33_set_working_register(cpu, 0u, address >= 0x1000u ? 0xa5a5u : 0u);
            dspic33_set_working_register(cpu, 15u, 0x5000u);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode);
            reason = dspic33_step(cpu);
            if (byte_mode == 0u && (address & 1u) != 0u) {
                matches = matches && reason == DSPIC33_TRAPPED &&
                          cpu->pc == 0x000340u && cpu->last_trap == 1u &&
                          cpu->last_trap_return == 2u;
            } else {
                matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->last_trap == UINT16_MAX;
            }
            matches = matches && cpu->cycles > cycles &&
                      cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
            if (address >= 0x1000u && (byte_mode != 0u || (address & 1u) == 0u)) {
                matches = matches && (byte_mode != 0u
                                          ? dspic33_read_byte(cpu, address) == 0xa5u
                                          : dspic33_read_word(cpu, address) == 0xa5a5u);
            }
            expect_dsp_matrix_case(state, matches, opcode, "WREG-to-file MOV encoding");
        }
    }

    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        uint8_t file_destination;
        for (file_destination = 0u; file_destination < 2u; file_destination++) {
            for (address = 0u; address < 0x2000u; address += 31u) {
                uint32_t opcode = 0xbf8000u | ((uint32_t)byte_mode << 14u) |
                                  ((uint32_t)file_destination << 13u) | address;
                uint16_t expected[16];
                uint16_t value;
                uint16_t expected_status;
                uint64_t cycles;
                Dspic33StopReason reason;
                bool matches;

                dspic33_reset(cpu, 0u);
                dspic33_set_async_events(cpu, false);
                prepare_move_registers(cpu, expected, 0x2000u, 2u);
                cpu->stop_on_trap = true;
                value = move_matrix_file_value(address, byte_mode != 0u);
                if (address >= 0x1000u) {
                    if (byte_mode != 0u) {
                        dspic33_write_byte(cpu, address, (uint8_t)value);
                    } else {
                        dspic33_write_word(cpu, address, value);
                    }
                } else if (address == 0x002eu) {
                    value = 2u;
                } else if (address == 0x0030u) {
                    value = 0u;
                } else {
                    value = byte_mode != 0u ? dspic33_read_byte(cpu, address)
                                            : dspic33_read_word(cpu, address);
                }
                expected_status =
                    move_matrix_logic_status(0x010fu, value, byte_mode != 0u);
                if (file_destination == 0u) {
                    expected[0] = byte_mode != 0u
                                      ? (uint16_t)((expected[0] & 0xff00u) | value)
                                      : value;
                }
                cycles = cpu->cycles;
                matches = dspic33_load_program_word(cpu, 0u, opcode);
                reason = dspic33_step(cpu);
                if (byte_mode == 0u && (address & 1u) != 0u) {
                    matches = matches && reason == DSPIC33_TRAPPED &&
                              cpu->pc == 0x000340u && cpu->last_trap == 1u &&
                              cpu->last_trap_return == 2u;
                } else {
                    matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                              cpu->last_trap == UINT16_MAX &&
                              cpu->sr == expected_status &&
                              move_matrix_registers_match(cpu, expected);
                }
                matches = matches && cpu->cycles > cycles &&
                          cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "file-to-destination MOV encoding");
            }
        }
    }
}

static void move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    move_literal_encoding_matrix_cases(state, cpu);
    move_register_encoding_matrix_cases(state, cpu);
    movpag_encoding_matrix_cases(state, cpu);
    generic_move_encoding_matrix_cases(state, cpu);
    offset_move_encoding_matrix_cases(state, cpu);
    move_double_encoding_matrix_cases(state, cpu);
    move_data_encoding_matrix_cases(state, cpu);
    file_move_encoding_matrix_cases(state, cpu);
}

static void prepare_flash_read_erratum_case(TestState* state, Dspic33* cpu,
                                            uint32_t start) {
    reset_processor_test(cpu, start);
    load_instruction(state, cpu, start, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    expect(state,
           dspic33_load_program_word(cpu, 0x1000u, 0x001000u) &&
               dspic33_load_program_word(cpu, 0x1002u, 0u),
           "load B1 Flash-read erratum PSV MOV.D data");
    dspic33_set_working_register(cpu, 1u, 0x9000u);
    cpu->dsrpag = 0x0200u;
    cpu->tblpag = 0u;
}

static void flash_read_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t flash_read_pairs[][2] = {
        {OPCODE_MOV_W1_W2, OPCODE_MOV_W1_W2},
        {OPCODE_TBLRDL_W2_W3, OPCODE_MOV_W1_W2},
    };
    Dspic33 copy;
    bool initialized;
    size_t index;

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "B1 back-to-back Flash-read sequence reports an undefined silicon result");

    for (index = 0u; index < sizeof(flash_read_pairs) / sizeof(flash_read_pairs[0]);
         index++) {
        prepare_flash_read_erratum_case(state, cpu, 0x200u);
        load_instruction(state, cpu, 0x202u, OPCODE_NOP);
        load_instruction(state, cpu, 0x204u, OPCODE_NOP);
        load_instruction(state, cpu, 0x206u, flash_read_pairs[index][0]);
        load_instruction(state, cpu, 0x208u, flash_read_pairs[index][1]);
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
               "B1 PSV and mixed back-to-back Flash reads share the erratum boundary");
    }

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_NOP);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "separating Flash reads with a NOP applies the documented workaround");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, 0x370000u);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "BRA to the next instruction applies the documented flow workaround");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, 0x090001u);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "REPEAT-ended connecting code does not arm the B1 Flash-read erratum");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x0302u, OPCODE_RETFIE);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING,
           "B1 Flash-read connecting code reaches the interrupt boundary");
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               !cpu->flash_read_erratum_armed,
           "interrupt vectoring cancels the B1 Flash-read sequence");
    dspic33_write_word(cpu, 0x0800u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0204u &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "post-interrupt Flash reads execute outside the cancelled erratum sequence");

    prepare_flash_read_erratum_case(state, cpu, 0x202u);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_NOP);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "misaligned PSV MOV.D does not arm the B1 Flash-read erratum");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    expect(
        state,
        dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
            dspic33_step(cpu) == DSPIC33_RUNNING &&
            dspic33_step(cpu) == DSPIC33_RUNNING && cpu->flash_read_erratum_candidate,
        "first qualifying Flash read arms the B1 erratum boundary");
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize B1 Flash-read erratum copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy armed B1 Flash-read state");
        expect(state,
               dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED &&
                   dspic33_step(&copy) == DSPIC33_SILICON_RESULT_UNDEFINED,
               "copied B1 Flash-read state retains the same sequence boundary");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0x200u);
    expect(state,
           !cpu->flash_read_erratum_armed && !cpu->flash_read_erratum_candidate &&
               cpu->flash_read_connecting_words == 0u &&
               !cpu->flash_read_connecting_ends_repeat,
           "reset clears B1 Flash-read sequence state");
}

static void do_flash_access_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t opcodes[] = {
        OPCODE_TBLRDL_W2_W3,
        OPCODE_TBLWTL_W2_W3,
        OPCODE_MOV_W1_W2,
    };
    size_t index;
    for (index = 0u; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        uint32_t boundary;
        for (boundary = 0x0200u; boundary <= 0x0204u; boundary += 4u) {
            reset_processor_test(cpu, boundary);
            load_instruction(state, cpu, boundary, opcodes[index]);
            cpu->do_depth = 1u;
            cpu->do_start[0] = 0x0200u;
            cpu->do_end[0] = 0x0204u;
            cpu->do_count[0] = 1u;
            cpu->dostart = 0x0200u;
            cpu->doend = 0x0204u;
            cpu->dcount = 1u;
            dspic33_set_working_register(cpu, 1u, 0xc000u);
            dspic33_set_working_register(cpu, 2u, 0u);
            dspic33_set_working_register(cpu, 3u, 0x1357u);
            cpu->dsrpag = 0x0200u;
            expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
                   "B1 DO boundary Flash access reports an undefined silicon result");
        }
    }

    reset_processor_test(cpu, 0x0202u);
    load_instruction(state, cpu, 0x0202u, OPCODE_TBLRDL_W2_W3);
    cpu->do_depth = 1u;
    cpu->do_start[0] = 0x0200u;
    cpu->do_end[0] = 0x0204u;
    cpu->do_count[0] = 1u;
    cpu->dostart = 0x0200u;
    cpu->doend = 0x0204u;
    cpu->dcount = 1u;
    dspic33_set_working_register(cpu, 2u, 0u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "Flash access inside a DO body remains defined away from both boundaries");
}

static void illegal_condition_reset_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t preserved_addresses[] = {
        0x0742u, 0x0744u, 0x0746u, 0x0748u, 0x0758u, 0x075au,
    };
    uint16_t
        preserved_values[sizeof(preserved_addresses) / sizeof(preserved_addresses[0])];
    Dspic33 copy;
    size_t index;

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ILLEGAL);
    dspic33_set_working_register(cpu, 0u, 0x1234u);
    dspic33_set_working_register(cpu, 1u, 0x5000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    cpu->splim = 0x6000u;
    cpu->splim_enabled = true;
    cpu->rcount = 3u;
    cpu->dcount = 4u;
    cpu->dostart = 0x100u;
    cpu->doend = 0x200u;
    cpu->tblpag = 0xa5u;
    cpu->dsrpag = 0x123u;
    cpu->dswpag = 0x123u;
    cpu->repeat_active = 1u;
    cpu->do_depth = 1u;
    cpu->data[0x0740u] = 0x83u;
    cpu->data[0x0741u] = 0u;
    for (index = 0u;
         index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        uint16_t address = preserved_addresses[index];
        preserved_values[index] = (uint16_t)(0xa100u + index);
        cpu->data[address] = (uint8_t)preserved_values[index];
        cpu->data[address + 1u] = (uint8_t)(preserved_values[index] >> 8u);
    }
    dspic33_write_word(cpu, 0x074eu, 0x8500u);
    cpu->io.adc[3] = 0x0456u;
    cpu->io.gpio[2] = 0x789au;
    cpu->io.uart_cts = 0x05u;
    cpu->io.spi_selected = 0x09u;
    cpu->io.timer_gate = 0x0105u;
    cpu->io.pwm_dead_time_inputs = 0x25u;
    cpu->io.pwm_sync_inputs = 0x02u;
    cpu->io.pwm_fault_inputs = 0x81234567u;
    cpu->io.pwm_current_limit_inputs = 0x89abcdefu;
    cpu->io.pwm_dead_time_direct = 0x25u;
    cpu->io.pwm_sync_direct = 0x02u;
    cpu->io.pwm_fault_direct = 0x81234567u;
    cpu->io.pwm_current_limit_direct = 0x89abcdefu;
    cpu->io.usb_host_attached = true;
    cpu->io.timer_enabled = 0xffffu;
    cpu->io.uart_rx_fifo[0].count = 1u;
    cpu->io.usb_host_pending = true;
    cpu->io.cpu_write_valid = true;
    expect(state, dspic33_uart_set_cts(cpu, 0u, true, 20u),
           "schedule peripheral state before illegal reset");
    dspic33_write_word(cpu, 0x0100u, 0xffffu);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    dspic33_write_word(cpu, 0x5002u, 0x5555u);
    expect_illegal_reset(state, cpu, "known illegal opcode resets processor");
    expect(state,
           dspic33_read_word(cpu, 0x0740u) == 0x4083u && cpu->sr == 0u &&
               cpu->corcon == 0x0020u && cpu->splim == 0u && !cpu->splim_enabled,
           "illegal reset preserves RCON history and resets core state");
    expect(state,
           cpu->rcount == 0u && cpu->dcount == 0u && cpu->dostart == 0u &&
               cpu->doend == 0u && cpu->tblpag == 0u && cpu->dsrpag == 1u &&
               cpu->dswpag == 1u && cpu->repeat_active == 0u && cpu->do_depth == 0u,
           "illegal reset restores loop and page state");
    expect(state,
           dspic33_read_word(cpu, 0x5000u) == 0xaaaau &&
               dspic33_read_word(cpu, 0x5002u) == 0x5555u &&
               dspic33_read_word(cpu, 0x0100u) == 0u,
           "warm reset retains RAM without writing an exception frame");
    for (index = 0u;
         index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        expect(state,
               dspic33_read_word(cpu, preserved_addresses[index]) ==
                   preserved_values[index],
               "warm reset retains oscillator and RTCC register");
    }
    expect(state, dspic33_read_word(cpu, 0x074eu) == 0u,
           "warm reset clears reference oscillator control");
    expect(state,
           cpu->io.adc[3] == 0x0456u && cpu->io.gpio[2] == 0x789au &&
               cpu->io.uart_cts == 0x05u && cpu->io.spi_selected == 0x09u &&
               cpu->io.timer_gate == 0x0105u && cpu->io.pwm_dead_time_inputs == 0x25u &&
               cpu->io.pwm_sync_inputs == 0x02u &&
               cpu->io.pwm_fault_inputs == 0x81234567u &&
               cpu->io.pwm_current_limit_inputs == 0x89abcdefu &&
               cpu->io.usb_host_attached,
           "warm reset retains external input state");
    expect(state,
           cpu->io.timer_enabled == 0u && cpu->io.uart_rx_fifo[0].count == 0u &&
               !cpu->io.usb_host_pending && !cpu->io.cpu_write_valid &&
               cpu->events.count == 1u && cpu->events.items[0].external &&
               cpu->events.items[0].type == DSPIC33_EVENT_UART &&
               cpu->events.items[0].cycle == 20u,
           "warm reset clears internal peripheral execution state");
    expect(state, dspic33_initialize(&copy), "initialize illegal reset copy");
    expect(state, dspic33_copy(&copy, cpu), "copy illegal reset state");
    expect(state,
           copy.illegal_reset && copy.illegal_reset_count == 1u &&
               copy.initialized_working_registers == 0x8000u,
           "copy retains illegal reset lifecycle state");
    dspic33_release(&copy);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->illegal_reset,
           "next instruction clears transient illegal reset state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_LITERAL);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u && cpu->cycles == 1u,
           "MOVPAG literal PP2 writes TBLPAG");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_INVALID_LITERAL);
    expect_illegal_reset(state, cpu, "MOVPAG literal PP3 resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_W1);
    dspic33_set_working_register(cpu, 1u, 0x00a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u && cpu->cycles == 1u,
           "MOVPAG register PP2 writes TBLPAG");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_W1);
    cpu->w[1] = 0x00a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u,
           "MOVPAG data source ignores initialization tag");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_INVALID_W1);
    dspic33_set_working_register(cpu, 1u, 0x00a5u);
    expect_illegal_reset(state, cpu, "MOVPAG register PP3 resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    cpu->w[1] = 0x1000u;
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    expect_illegal_reset(state, cpu, "raw host W pointer does not initialize register");

    dspic33_reset(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_BYTE_LITERAL_W1);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (cpu->initialized_working_registers & 0x0002u) == 0u,
           "byte instruction destination leaves W pointer uninitialized");
    expect_illegal_reset(state, cpu, "byte instruction result remains invalid pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W15_W2);
    dspic33_write_word(cpu, 0x1000u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0xa5a5u &&
               cpu->illegal_reset_count == 0u,
           "W15 indirect access is initialized after reset");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "host word setter initializes same-value pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_byte(cpu, 2u, 0u);
    expect_illegal_reset(state, cpu,
                         "single byte W alias leaves pointer uninitialized");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_byte(cpu, 2u, 0u);
    dspic33_write_byte(cpu, 3u, 0x10u);
    expect_illegal_reset(state, cpu, "two byte W aliases leave pointer uninitialized");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_word(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u &&
               (cpu->initialized_working_registers & 0x0006u) == 0x0006u,
           "word W alias initializes pointer and destination");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0x1000u);
    dspic33_write_byte(cpu, 2u, 0u);
    dspic33_write_word(cpu, 0x1000u, 0x5678u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5678u,
           "byte W alias preserves prior initialized state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1);
    cpu->w[1] = 0x5000u;
    dspic33_set_working_register(cpu, 2u, 0xbeefu);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    expect_illegal_reset(state, cpu, "uninitialized store pointer resets processor");
    expect(state, dspic33_read_word(cpu, 0x5000u) == 0xaaaau,
           "uninitialized store does not modify RAM");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT);
    dspic33_set_working_register(cpu, 2u, 1u);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    cpu->w[5] = 0x5000u;
    dspic33_write_word(cpu, 0x1000u, 2u);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    expect_illegal_reset(state, cpu,
                         "uninitialized two-operand destination resets processor");
    expect(state, !cpu->address_error, "illegal reset clears address error flag");
    expect(state, !cpu->address_error_access_allowed,
           "illegal reset clears address error access state");
    expect(state, !cpu->address_error_working_state_completed,
           "illegal reset clears address error working state");
    expect(state, !cpu->address_error_accumulator_state_completed,
           "illegal reset clears address error accumulator state");
    expect(state, !cpu->address_error_control_state_completed,
           "illegal reset clears address error control state");
    expect(state, cpu->address_error_return == 0u,
           "illegal reset clears address error return");
    expect(state, cpu->sr == 0u && cpu->corcon == 0x0020u,
           "illegal reset discards post-validation flags");
    expect(state,
           dspic33_read_word(cpu, 0x1000u) == 2u &&
               dspic33_read_word(cpu, 0x5000u) == 0xaaaau,
           "uninitialized destination preserves source and destination data");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5);
    cpu->w[2] = 7u;
    cpu->w[4] = 0x1000u;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    dspic33_write_word(cpu, 0x1000u, 0xabcdu);
    expect_illegal_reset(state, cpu,
                         "uninitialized binary source resets before direct result");
    expect(state,
           cpu->w[5] == 0u && cpu->sr == 0u && cpu->corcon == 0x0020u &&
               dspic33_read_word(cpu, 0x1000u) == 0xabcdu,
           "binary source reset prevents data, result and flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_COMPARE_ZERO_W4_POST_INCREMENT);
    cpu->w[4] = 0x1000u;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    expect_illegal_reset(state, cpu,
                         "uninitialized compare source resets before flags");
    expect(state, cpu->sr == 0u && cpu->corcon == 0x0020u,
           "compare source reset prevents flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_ADD_W4_POST_INCREMENT);
    cpu->w[4] = 0x1000u;
    cpu->accumulator[0] = 0x12345678;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    expect_illegal_reset(state, cpu,
                         "uninitialized accumulator source resets before result");
    expect(state, cpu->accumulator[0] == 0 && cpu->sr == 0u && cpu->corcon == 0x0020u,
           "accumulator source reset prevents result and flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    cpu->w[2] = 0x1000u;
    expect_illegal_reset(state, cpu, "uninitialized table pointer resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "initialized table pointer completes access");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W4_POST_INCREMENT);
    cpu->tblpag = 5u;
    dspic33_set_working_register(cpu, 2u, 0x5800u);
    cpu->w[4] = 0x1000u;
    expect_illegal_reset(
        state, cpu,
        "unimplemented table read with uninitialized destination resets processor");
    expect(state,
           !cpu->address_error && !cpu->address_error_access_allowed &&
               !cpu->address_error_working_state_completed &&
               !cpu->address_error_accumulator_state_completed &&
               !cpu->address_error_control_state_completed &&
               cpu->address_error_return == 0u,
           "table destination reset clears address error lifecycle state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    cpu->w[9] = 0x1000u;
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    expect_illegal_reset(state, cpu, "uninitialized DSP base resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "initialized DSP bases complete access");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_WRITE_BACK);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    cpu->w[13] = 0x5000u;
    expect_illegal_reset(state, cpu,
                         "uninitialized DSP write-back pointer resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_WRITE_BACK);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    dspic33_set_working_register(cpu, 13u, 0x5000u);
    cpu->w[4] = 1u;
    cpu->w[5] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[13] == 0x5002u &&
               cpu->illegal_reset_count == 0u,
           "initialized DSP write-back ignores multiplicand tags");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_DIRECT_W13);
    cpu->w[4] = 1u;
    cpu->w[5] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (cpu->initialized_working_registers & 0x2000u) != 0u &&
               cpu->illegal_reset_count == 0u,
           "direct DSP W13 result initializes destination");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_W0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "computed jump register is not an address-pointer tag use");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_REPEAT_W0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "REPEAT count register is not an address-pointer tag use");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DO_W0);
    load_instruction(state, cpu, 2u, 0x000002u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "DO count register is not an address-pointer tag use");

    dspic33_reset(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_PUSH_SHADOW);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W0_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "PUSH.S values are not address-pointer tag uses");
    expect_illegal_reset(state, cpu, "PUSH.S does not initialize W0 pointer");
}

static void accumulator_operation_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_ADD_A_AND_B);
    cpu->accumulator[0] = 7;
    cpu->accumulator[1] = -2;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 5 &&
               cpu->accumulator[1] == -2,
           "accumulator addition stores in A");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_NEGATE_B);
    cpu->accumulator[0] = 7;
    cpu->accumulator[1] = -2;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 7 &&
               cpu->accumulator[1] == 2,
           "accumulator negation stores in B");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_SUBTRACT_B_FROM_A);
    cpu->accumulator[0] = 7;
    cpu->accumulator[1] = -2;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 9 &&
               cpu->accumulator[1] == -2,
           "accumulator subtraction stores in A");
}

static void accumulator_store_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_STORE_A_W2);
    cpu->accumulator[0] = 0x12348000;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u,
           "accumulator store truncates the low word");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_ROUNDED_STORE_A_W2);
    cpu->corcon |= 0x0002u;
    cpu->accumulator[0] = 0x12348000;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1235u,
           "accumulator store applies conventional rounding");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_SHIFTED_STORE_A_W4);
    cpu->accumulator[0] = 0x00008000;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 1u,
           "accumulator store applies the encoded shift");
}

static void bit_reversed_addressing_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2_BIT_REVERSED_INCREMENT);
    dspic33_set_working_register(cpu, 1u, 0x5a5au);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x0046u, 0x0200u);
    dspic33_write_word(cpu, 0x0050u, 0x8002u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1004u &&
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au,
           "bit-reversed post-increment updates the selected pointer");
}

static void run_limit_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state,
           dspic33_run(cpu, 1u) == DSPIC33_INSTRUCTION_LIMIT && cpu->pc == 2u &&
               cpu->instructions == 1u,
           "run stops at the instruction limit");
}

static bool group_selected(int argc, char** argv, const char* group) {
    return argc == 1 || (argc == 2 && strcmp(argv[1], group) == 0);
}

int main(int argc, char** argv) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "processor control test initializes");
    if (initialized) {
        if (group_selected(argc, argv, "branch")) {
            conditional_branch_encoding_matrix_cases(&state, &cpu);
            computed_control_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "literal_return")) {
            literal_control_encoding_matrix_cases(&state, &cpu);
            return_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "dsp_general")) {
            dspic33_reset(&cpu, 0u);
            dspic33_set_async_events(&cpu, false);
            general_dsp_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "dsp_other")) {
            dspic33_reset(&cpu, 0u);
            dspic33_set_async_events(&cpu, false);
            special_dsp_encoding_matrix_cases(&state, &cpu);
            square_dsp_encoding_matrix_cases(&state, &cpu);
            euclidean_dsp_encoding_matrix_cases(&state, &cpu);
            invalid_dsp_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "multiply")) {
            generic_multiply_encoding_matrix_cases(&state, &cpu);
            file_multiply_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "move")) {
            move_encoding_matrix_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "errata")) {
            flash_read_erratum_cases(&state, &cpu);
            do_flash_access_erratum_cases(&state, &cpu);
            illegal_condition_reset_cases(&state, &cpu);
        }
        if (group_selected(argc, argv, "behavior")) {
            accumulator_operation_cases(&state, &cpu);
            accumulator_store_cases(&state, &cpu);
            bit_reversed_addressing_cases(&state, &cpu);
            run_limit_cases(&state, &cpu);
        }
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
