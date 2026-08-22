#include "architecture/dspic33/execution/data/internal.h"

void dspic33_data_test_general_unary_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const DirectFileOperation operations[4][2] = {{DIRECT_FILE_INC, DIRECT_FILE_INC2},
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
        bool valid =
            destination_mode < 6u && (nullary ? (opcode & 0x00007fu) == 0u : source_mode < 6u);

        if (valid) {
            dspic33_data_test_run_legal_unary_matrix_case(state, cpu, opcode,
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
    uint16_t initial_status = (uint16_t)(((address >> 10u) & 1u) | (((address >> 11u) & 1u) << 1u));
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
        dspic33_set_working_register(cpu, reg,
                                     (uint16_t)(0x4200u + (uint16_t)reg * 0x0101u + address));
    }
    dspic33_set_working_register(cpu, 0u, byte_mode ? (uint16_t)(0xa500u | operand) : operand);
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

static void write_direct_file_reference_result(Dspic33* cpu, uint16_t value, bool byte_mode) {
    if (byte_mode) {
        cpu->w[0] = (uint16_t)((cpu->w[0] & 0xff00u) | (value & 0x00ffu));
    } else {
        cpu->w[0] = value;
        cpu->initialized_working_registers |= 0x0001u;
    }
    cpu->instruction_working_register_writes |= 0x0001u;
}

static bool direct_file_event_queues_match(const Dspic33* actual, const Dspic33* expected) {
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

static bool direct_file_io_states_match(const Dspic33* actual, const Dspic33* expected) {
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
                                          uint16_t address, bool byte_mode, bool file_destination) {
    uint64_t device_ratio = dspic33_device_instruction_cycles(cpu, 1u);
    bool reads_source = dspic33_data_test_direct_file_reads_source(operation);
    bool writes_result = dspic33_data_test_direct_file_writes_result(operation);
    bool non_cpu_sfr = reads_source && (address & (byte_mode ? 0xffffu : 0xfffeu)) >= 0x005au &&
                       address < 0x1000u &&
                       dspic33_data_test_direct_file_address_implemented(address);
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
        left = byte_mode ? dspic33_read_byte(cpu, address) : dspic33_read_word(cpu, address);
    }
    value = dspic33_data_test_direct_file_result(operation, left, right, initial_status, byte_mode);
    status =
        dspic33_data_test_direct_file_status(operation, left, right, initial_status, byte_mode);
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
    uint8_t current_priority =
        (uint8_t)(((cpu->corcon & 0x0008u) != 0u ? 8u : 0u) | ((cpu->sr >> 5u) & 0x07u));
    size_t index;

    for (index = 0u; index < 4u; index++) {
        const Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay == 0u && pending->priority > current_priority &&
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
    final_status = (uint16_t)((expected->sr & ~0x00e0u) | ((pending->priority & 7u) << 5u));
    final_status &= (uint16_t)~0x0010u;
    final_control = pending->priority > 7u ? (uint16_t)((expected->corcon & ~0x0004u) | 0x0008u)
                                           : (uint16_t)(expected->corcon & ~(uint16_t)0x000cu);
    return actual->stop_reason == DSPIC33_TRAPPED && actual->last_trap == pending->trap &&
           actual->last_trap_return == 2u && actual->pc == 0x000340u && actual->w[15] == 0x5004u &&
           actual->sr == final_status && actual->corcon == final_control &&
           actual->trap_count == 1u && actual->interrupt_depth == 1u &&
           memcmp(actual->w, expected->w, 15u * sizeof(*actual->w)) == 0 &&
           memcmp(actual->data, expected->data, 0x08c8u) == 0 &&
           memcmp(actual->data + 0x08cau, expected->data + 0x08cau, 0x2000u - 0x08cau) == 0 &&
           (uint16_t)(actual->data[0x08c8u] | ((uint16_t)actual->data[0x08c9u] << 8u)) ==
               (uint16_t)(((uint16_t)pending->priority << 8u) | pending->trap) &&
           (uint16_t)(actual->data[0x5000u] | ((uint16_t)actual->data[0x5001u] << 8u)) == 2u &&
           (uint16_t)(actual->data[0x5002u] | ((uint16_t)actual->data[0x5003u] << 8u)) ==
               stacked_high &&
           direct_file_io_states_match(actual, expected) &&
           direct_file_event_queues_match(actual, expected);
}

static bool run_direct_file_odd_word_case(Dspic33* cpu, Dspic33* reference, uint32_t opcode,
                                          DirectFileOperation operation, uint16_t address,
                                          bool file_destination) {
    uint16_t initial_status = reference->sr;
    uint16_t right = reference->w[0];
    bool reads_source = dspic33_data_test_direct_file_reads_source(operation);
    uint16_t left = 0u;
    uint16_t value;
    uint16_t status;
    bool matches;
    bool writes_result = dspic33_data_test_direct_file_writes_result(operation);
    uint64_t device_ratio = dspic33_device_instruction_cycles(reference, 1u);
    bool non_cpu_sfr = reads_source && address >= 0x005bu && address < 0x1000u &&
                       dspic33_data_test_direct_file_address_implemented(address);
    uint64_t expected_cycles = reads_source && address >= 0x005bu && address < 0x1000u &&
                                       dspic33_data_test_direct_file_address_implemented(address)
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
    value = dspic33_data_test_direct_file_result(operation, left, right, initial_status, false);
    status = dspic33_data_test_direct_file_status(operation, left, right, initial_status, false);
    reference->sr = status;
    if (!file_destination && writes_result) {
        write_direct_file_reference_result(reference, value, false);
    }
    reference->instruction_active = false;
    reference->previous_working_register_writes = reference->instruction_working_register_writes;
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
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_TRAPPED &&
              cpu->last_trap == 1u && cpu->last_trap_return == 2u && cpu->pc == 0x000340u &&
              cpu->cycles == expected_cycles && cpu->w[15] == 0x5004u &&
              (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
              (dspic33_read_word(cpu, 0x5002u) >> 8u) == (status & 0x00ffu) &&
              (uint16_t)(cpu->data[0x08c8u] | ((uint16_t)cpu->data[0x08c9u] << 8u)) == 0x0e01u &&
              memcmp(cpu->w, reference->w, 15u * sizeof(*cpu->w)) == 0 &&
              memcmp(cpu->data, reference->data, 0x08c0u) == 0 &&
              memcmp(cpu->data + 0x08c2u, reference->data + 0x08c2u, 0x08c8u - 0x08c2u) == 0 &&
              memcmp(cpu->data + 0x08cau, reference->data + 0x08cau, 0x2000u - 0x08cau) == 0 &&
              memcmp(&cpu->nvm, &reference->nvm, sizeof(cpu->nvm)) == 0 &&
              memcmp(&cpu->oscillator, &reference->oscillator, sizeof(cpu->oscillator)) == 0 &&
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

bool dspic33_data_test_load_direct_file_trap_vectors(Dspic33* cpu) {
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
                                               DirectFileOperation operation, bool byte_mode) {
    bool expected[512] = {false};
    uint32_t maximum = byte_mode ? UINT8_MAX : UINT16_MAX;
    uint32_t value;
    uint16_t status;

    for (value = 0u; value <= maximum; value++) {
        uint8_t initial;
        for (initial = 0u; initial < 4u; initial++) {
            if (operation >= DIRECT_FILE_AND && operation <= DIRECT_FILE_IOR) {
                status =
                    dspic33_data_test_direct_file_logic_status(initial, (uint16_t)value, byte_mode);
            } else {
                status = dspic33_data_test_direct_file_status(operation, (uint16_t)value, 0u,
                                                              initial, byte_mode);
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

bool dspic33_data_test_run_direct_file_case(Dspic33* actual, Dspic33* reference, uint32_t opcode,
                                            DirectFileOperation operation, uint16_t address,
                                            bool byte_mode, bool file_destination) {
    bool matches;

    prepare_direct_file_case(actual, address, byte_mode);
    prepare_direct_file_case(reference, address, byte_mode);
    if (!byte_mode && (address & 1u) != 0u &&
        (dspic33_data_test_direct_file_reads_source(operation) || file_destination)) {
        return run_direct_file_odd_word_case(actual, reference, opcode, operation, address,
                                             file_destination);
    }
    matches = dspic33_load_program_word(actual, 0u, opcode) &&
              dspic33_load_program_word(reference, 0u, opcode);
    dspic33_step(actual);
    run_direct_file_reference(reference, operation, address, byte_mode, file_destination);
    return matches && (file_destination && address >= 0x08c0u && address <= 0x08c7u
                           ? direct_file_trap_register_state_matches(actual, reference)
                           : direct_file_states_match(actual, reference));
}

void dspic33_data_test_direct_file_arithmetic_encoding_matrix_cases(TestState* state) {
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
           dspic33_data_test_load_direct_file_trap_vectors(&actual) &&
               dspic33_data_test_load_direct_file_trap_vectors(&reference),
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
                    bool matches = dspic33_data_test_run_direct_file_case(
                        &actual, &reference, opcode, (DirectFileOperation)operation, address,
                        byte_mode != 0u, file_destination != 0u);
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file arithmetic encoding");
                }
            }
        }
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}
void dspic33_data_test_direct_file_logical_encoding_matrix_cases(TestState* state,
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
           dspic33_data_test_load_direct_file_trap_vectors(&actual) &&
               dspic33_data_test_load_direct_file_trap_vectors(&reference),
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
                    bool matches = dspic33_data_test_run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);

                    if (address >= 0x1000u) {
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] = true;
                    }
                    expect_dsp_matrix_case(state, matches, opcode, "direct-file logical encoding");
                }
            }
            for (uint8_t status = 0u; status < 4u; status++) {
                for (uint8_t operand = 0u; operand < 16u; operand++) {
                    for (uint8_t source = 0u; source < 16u; source++) {
                        uint16_t address = (uint16_t)(0x1000u | ((uint16_t)status << 10u) |
                                                      ((uint16_t)operand << 5u) | source);
                        uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) | address;
                        bool matches = dspic33_data_test_run_direct_file_case(
                            &actual, &reference, opcode, operations[operation], address,
                            byte_mode != 0u, false);
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] = true;
                        expect_dsp_matrix_case(state, matches, opcode,
                                               "direct-file logical boundary values");
                    }
                }
            }
        }
    }
    for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (uint16_t address = 0u; address < 0x2000u; address += 31u) {
            run_invalid_binary_matrix_case(state, invalid_cpu,
                                           0xb78000u | ((uint32_t)byte_mode << 14u) | address);
        }
    }
    for (operation = 0u; operation < 3u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            expect(state,
                   direct_file_flag_outcomes_complete(flag_outcomes[operation][byte_mode],
                                                      operations[operation], byte_mode != 0u),
                   "direct-file logical flag outcomes are complete");
        }
    }
    dspic33_release(&actual);
    dspic33_release(&reference);
}

void dspic33_data_test_direct_file_unary_encoding_matrix_cases(TestState* state) {
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
           dspic33_data_test_load_direct_file_trap_vectors(&actual) &&
               dspic33_data_test_load_direct_file_trap_vectors(&reference),
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
                    bool matches = dspic33_data_test_run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);

                    if (address >= 0x1000u) {
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] = true;
                    }
                    expect_dsp_matrix_case(state, matches, opcode, "direct-file unary encoding");
                }
            }
        }
    }
    for (operation = 0u; operation < 8u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            expect(state,
                   direct_file_flag_outcomes_complete(flag_outcomes[operation][byte_mode],
                                                      operations[operation], byte_mode != 0u),
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

void dspic33_data_test_single_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t fields = 0u; fields < 0x040000u; fields += 257u) {
        uint32_t opcode = 0xd00000u | fields;
        uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
        bool alternate = (opcode & 0x008000u) != 0u;
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        bool valid = (family != 0u || !alternate) && destination_mode < 6u && source_mode < 6u;

        if (valid) {
            dspic33_data_test_run_legal_unary_matrix_case(
                state, cpu, opcode, shift_matrix_operation(family, alternate));
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
        }
    }
}

void dspic33_data_test_direct_file_shift_encoding_matrix_cases(TestState* state,
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
           dspic33_data_test_load_direct_file_trap_vectors(&actual) &&
               dspic33_data_test_load_direct_file_trap_vectors(&reference),
           "load direct-file shift address-error vectors");
    for (uint8_t operation = 0u; operation < 7u; operation++) {
        for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            for (uint8_t file_destination = 0u; file_destination < 2u; file_destination++) {
                for (uint16_t address = 0u; address < 0x2000u; address += 31u) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = dspic33_data_test_run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);
                    expect_dsp_matrix_case(state, matches, opcode, "direct-file shift encoding");
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
