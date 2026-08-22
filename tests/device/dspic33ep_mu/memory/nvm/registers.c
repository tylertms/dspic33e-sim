#include "device/dspic33ep_mu/memory/nvm/internal.h"

bool dspic33_nvm_test_interrupt_flag(Dspic33* cpu) {
    return (dspic33_read_word(cpu, 0x0800u) & 0x8000u) != 0u;
}

uint32_t dspic33_nvm_test_program_word(const Dspic33* cpu, uint32_t address) {
    return dspic33_read_program_word(cpu, address);
}

void dspic33_nvm_test_configure_operation(Dspic33* cpu, uint16_t operation, uint32_t address,
                                          bool write_enable) {
    dspic33_write_word(cpu, NVM_ADDRESS, (uint16_t)address);
    dspic33_write_word(cpu, NVM_ADDRESS_HIGH, (uint16_t)(address >> 16u));
    dspic33_write_word(cpu, NVM_CONTROL,
                       (uint16_t)(operation | (write_enable ? NVM_WRITE_ENABLE : 0u)));
}

static bool step_instructions(Dspic33* cpu, uint8_t count) {
    uint8_t index;
    for (index = 0u; index < count; index++) {
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            return false;
        }
    }
    return true;
}

bool dspic33_nvm_test_read_table(Dspic33* cpu, uint32_t address, uint32_t opcode, uint16_t* value) {
    dspic33_reset(cpu, 0u);
    if (!dspic33_load_program_word(cpu, 0u, opcode)) {
        return false;
    }
    cpu->tblpag = (uint16_t)(address >> 16u);
    dspic33_set_working_register(cpu, 2u, (uint16_t)address);
    dspic33_set_working_register(cpu, 3u, 0xa5a5u);
    if (dspic33_step(cpu) != DSPIC33_RUNNING) {
        return false;
    }
    *value = cpu->w[3];
    return true;
}

void dspic33_nvm_test_configuration_table_view_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t addresses[] = {0xf80004u, 0xf80006u, 0xf80008u, 0xf8000au,
                                         0xf8000cu, 0xf8000eu, 0xf80010u, 0xf80012u};
    static const uint8_t defaults[] = {0xcfu, 0xffu, 0xffu, 0xffu, 0xffu, 0xdfu, 0xcfu, 0xffu};
    static const uint16_t loaded[] = {0xa511u, 0xb622u, 0xc733u, 0xd844u,
                                      0xe955u, 0xfa66u, 0x8b77u, 0x9c88u};
    static const uint32_t ids[] = {0xff0000u, 0xff0002u};
    static const uint16_t id_values[] = {0x1872u, 0x4002u};
    static const uint32_t reserved[] = {0xf80000u, 0xf80002u};
    uint16_t value;
    size_t index;
    Dspic33 copy;
    bool copy_initialized;

    for (index = 0u; index < sizeof(addresses) / sizeof(addresses[0]); index++) {
        uint32_t address = addresses[index];
        expect(state,
               dspic33_read_configuration_byte(cpu, address) == defaults[index] &&
                   dspic33_read_configuration_byte(cpu, address + 1u) == 0xffu,
               "configuration factory raw bytes");
        expect(state,
               dspic33_read_program_byte(cpu, address) == defaults[index] &&
                   dspic33_read_program_byte(cpu, address + 1u) == 0u,
               "configuration factory CPU bytes");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDL_W2_W3, &value) &&
                   value == defaults[index],
               "configuration factory TBLRDL");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
               "configuration factory TBLRDH");
    }

    for (index = 0u; index < sizeof(addresses) / sizeof(addresses[0]); index++) {
        uint32_t address = addresses[index];
        expect(state, dspic33_load_configuration_word(cpu, address, loaded[index]),
               "load raw configuration pair");
        expect(state,
               dspic33_read_configuration_byte(cpu, address) == (uint8_t)loaded[index] &&
                   dspic33_read_configuration_byte(cpu, address + 1u) ==
                       (uint8_t)(loaded[index] >> 8u),
               "raw configuration API preserves pair");
        expect(state,
               dspic33_read_program_byte(cpu, address) == (uint8_t)loaded[index] &&
                   dspic33_read_program_byte(cpu, address + 1u) == 0u,
               "CPU configuration view zero extends low byte");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDL_W2_W3, &value) &&
                   value == (uint8_t)loaded[index],
               "loaded configuration TBLRDL");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
               "loaded configuration TBLRDH");
    }

    expect(state,
           dspic33_nvm_test_read_table(cpu, addresses[0], TBLRDL_BYTE_W2_W3, &value) &&
               value == 0xa511u,
           "configuration TBLRDL low byte");
    expect(state,
           dspic33_nvm_test_read_table(cpu, addresses[0] + 1u, TBLRDL_BYTE_W2_W3, &value) &&
               value == 0xa500u,
           "configuration TBLRDL upper byte zero");
    expect(state,
           dspic33_nvm_test_read_table(cpu, addresses[0], TBLRDH_BYTE_W2_W3, &value) &&
               value == 0xa500u,
           "configuration TBLRDH low byte zero");
    expect(state,
           dspic33_nvm_test_read_table(cpu, addresses[0] + 1u, TBLRDH_BYTE_W2_W3, &value) &&
               value == 0xa500u,
           "configuration TBLRDH upper byte zero");

    for (index = 0u; index < sizeof(ids) / sizeof(ids[0]); index++) {
        uint32_t address = ids[index];
        uint16_t expected = id_values[index];
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDL_W2_W3, &value) && value == expected,
               "device ID TBLRDL");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
               "device ID TBLRDH");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDL_BYTE_W2_W3, &value) &&
                   value == (uint16_t)(0xa500u | (uint8_t)expected),
               "device ID TBLRDL low byte");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address + 1u, TBLRDL_BYTE_W2_W3, &value) &&
                   value == (uint16_t)(0xa500u | (uint8_t)(expected >> 8u)),
               "device ID TBLRDL upper byte");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDH_BYTE_W2_W3, &value) &&
                   value == 0xa500u,
               "device ID TBLRDH low byte zero");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address + 1u, TBLRDH_BYTE_W2_W3, &value) &&
                   value == 0xa500u,
               "device ID TBLRDH upper byte zero");
        expect(state,
               dspic33_read_program_byte(cpu, address) == (uint8_t)expected &&
                   dspic33_read_program_byte(cpu, address + 1u) == (uint8_t)(expected >> 8u),
               "device ID host program bytes");
    }

    for (index = 0u; index < sizeof(reserved) / sizeof(reserved[0]); index++) {
        uint32_t address = reserved[index];
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDL_W2_W3, &value) && value == 0u,
               "reserved configuration TBLRDL zero");
        expect(state,
               dspic33_nvm_test_read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
               "reserved configuration TBLRDH zero");
        expect(state,
               dspic33_read_program_byte(cpu, address) == 0u &&
                   dspic33_read_program_byte(cpu, address + 1u) == 0u,
               "reserved configuration host bytes zero");
    }

    copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized, "initialize configuration copy");
    if (copy_initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy configuration state");
        expect(state,
               dspic33_read_configuration_byte(&copy, addresses[0]) == (uint8_t)loaded[0] &&
                   dspic33_read_configuration_byte(&copy, addresses[0] + 1u) ==
                       (uint8_t)(loaded[0] >> 8u),
               "copy preserves raw configuration pair");
        expect(state,
               dspic33_nvm_test_read_table(&copy, addresses[0], TBLRDL_W2_W3, &value) &&
                   value == (uint8_t)loaded[0],
               "copy preserves CPU configuration view");
        dspic33_release(&copy);
    }
}

static void load_start_sequence_at(Dspic33* cpu, uint32_t base, bool delayed_write) {
    dspic33_load_program_word(cpu, base, MOVE_KEY_55);
    dspic33_load_program_word(cpu, base + 2u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, base + 4u, MOVE_KEY_AA);
    dspic33_load_program_word(cpu, base + 6u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, base + 8u, delayed_write ? 0x000000u : SET_NVM_WRITE);
    dspic33_load_program_word(cpu, base + 10u, SET_NVM_WRITE);
    cpu->pc = base;
}

static void load_start_sequence(Dspic33* cpu, bool delayed_write) {
    load_start_sequence_at(cpu, NVM_SEQUENCE_BASE, delayed_write);
}

bool dspic33_nvm_test_execute_start_sequence(Dspic33* cpu, bool delayed_write) {
    load_start_sequence(cpu, delayed_write);
    return step_instructions(cpu, delayed_write ? 6u : 5u);
}

bool dspic33_nvm_test_start_operation(Dspic33* cpu, uint16_t operation, uint32_t address) {
    dspic33_write_word(cpu, 0x08c2u, (uint16_t)(dspic33_read_word(cpu, 0x08c2u) & ~0x8000u));
    dspic33_nvm_test_configure_operation(cpu, operation, address, true);
    if (!dspic33_nvm_test_execute_start_sequence(cpu, false)) {
        return false;
    }
    return cpu->nvm.active;
}

bool dspic33_nvm_test_start_operation_from(Dspic33* cpu, uint16_t operation, uint32_t address,
                                           uint32_t execution_address) {
    dspic33_write_word(cpu, 0x08c2u, (uint16_t)(dspic33_read_word(cpu, 0x08c2u) & ~0x8000u));
    dspic33_nvm_test_configure_operation(cpu, operation, address, true);
    load_start_sequence_at(cpu, execution_address, false);
    if (!step_instructions(cpu, 5u)) {
        return false;
    }
    return cpu->nvm.active;
}

bool dspic33_nvm_test_finish_operation(Dspic33* cpu) { return dspic33_device_advance(cpu, 2u); }

uint8_t dspic33_nvm_test_codeguard_configuration_value(uint8_t index) {
    return (uint8_t)((index & 0x03u) | ((index & 0x0cu) << 2u));
}

bool dspic33_nvm_test_codeguard_configuration_high(uint8_t configuration) {
    uint8_t protection = (uint8_t)(configuration & 0x03u);
    uint8_t expected_key = protection == 0x03u ? 0u : 0x30u;
    return (protection & 0x02u) == 0u || (configuration & 0x30u) != expected_key;
}

bool dspic33_nvm_test_load_codeguard_configuration(Dspic33* cpu, uint8_t general,
                                                   uint8_t auxiliary) {
    return dspic33_load_configuration_word(cpu, CODEGUARD_GENERAL_CONFIGURATION,
                                           (uint16_t)(0xff00u | general)) &&
           dspic33_load_configuration_word(cpu, CODEGUARD_AUXILIARY_CONFIGURATION,
                                           (uint16_t)(0xff00u | auxiliary));
}

uint16_t dspic33_nvm_test_execute_codeguard_table_read(Dspic33* cpu, uint32_t origin,
                                                       uint32_t target) {
    dspic33_load_program_word(cpu, origin, TBLRDL_W2_W3);
    cpu->pc = origin;
    cpu->tblpag = (uint16_t)(target >> 16u);
    dspic33_set_working_register(cpu, 2u, (uint16_t)target);
    dspic33_set_working_register(cpu, 3u, 0xa5a5u);
    if (dspic33_step(cpu) != DSPIC33_RUNNING) {
        return 0xffffu;
    }
    return cpu->w[3];
}

uint16_t dspic33_nvm_test_execute_codeguard_psv_read(Dspic33* cpu, uint32_t origin,
                                                     uint32_t target) {
    dspic33_load_program_word(cpu, origin, OPCODE_MOV_W1_W2);
    cpu->pc = origin;
    cpu->dsrpag = (uint16_t)(0x0200u | ((target >> 15u) & 0x00ffu));
    dspic33_set_working_register(cpu, 1u, (uint16_t)(0x8000u | (target & 0x7fffu)));
    dspic33_set_working_register(cpu, 2u, 0xa5a5u);
    if (dspic33_step(cpu) != DSPIC33_RUNNING) {
        return 0xffffu;
    }
    return cpu->w[2];
}

void dspic33_nvm_test_load_long_program_flow(Dspic33* cpu, uint32_t origin, uint32_t target,
                                             bool call) {
    dspic33_load_program_word(cpu, origin, (call ? 0x020000u : 0x040000u) | (target & 0xffffu));
    dspic33_load_program_word(cpu, origin + 2u, (target >> 16u) & 0x007fu);
}

void dspic33_nvm_test_load_program_return(Dspic33* cpu, uint32_t origin, uint32_t target,
                                          uint32_t opcode) {
    dspic33_load_program_word(cpu, origin, opcode);
    dspic33_set_working_register(cpu, 15u, 0x1004u);
    dspic33_write_word(cpu, 0x1000u, (uint16_t)target);
    dspic33_write_word(cpu, 0x1002u, (uint16_t)(target >> 16u));
}

bool dspic33_nvm_test_codeguard_security_reset(const Dspic33* cpu, uint64_t reset_count) {
    return cpu->illegal_reset && cpu->illegal_reset_count == reset_count + 1u && cpu->pc == 0u &&
           (cpu->data[0x0741u] & 0x40u) != 0u;
}

void dspic33_nvm_test_reset_and_access_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == 0u, "NVMCON reset");
    expect(state, dspic33_read_word(cpu, NVM_KEY) == 0u, "NVMKEY read zero");
    expect(state, !cpu->nvm.active, "NVM reset inactive");
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ERROR);
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == NVM_WRITE_ERROR, "WRERR software set");
    dspic33_write_word(cpu, NVM_CONTROL, 0u);
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == 0u, "WRERR software clear");

    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_CONTROL,
                       (uint16_t)(dspic33_read_word(cpu, NVM_CONTROL) | NVM_WRITE));
    expect(state, !cpu->nvm.active, "WR without keys rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
           "rejected WR remains clear");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "rejected WR sets WRERR");
    expect(state, cpu->events.count == 0u, "rejected WR queues no event");
    expect(state, !dspic33_nvm_test_interrupt_flag(cpu), "rejected WR raises no interrupt");
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | 1u);
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) == 0u,
           "WRERR clears after rejected write");
}

void dspic33_nvm_test_key_sequence_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_KEY, 0x00aau);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "reverse keys rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "reverse keys set WRERR");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_KEY, 0x0000u);
    dspic33_write_word(cpu, NVM_KEY, 0x00aau);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "wrong intervening key rejected");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "repeated first key rejected");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, false);
    expect(state, dspic33_nvm_test_execute_start_sequence(cpu, false),
           "missing WREN sequence executes");
    expect(state, !cpu->nvm.active, "missing WREN rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "missing WREN sets WRERR");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    expect(state, dspic33_nvm_test_execute_start_sequence(cpu, true),
           "delayed WR sequence executes");
    expect(state, !cpu->nvm.active, "expired WR window rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "expired WR window sets WRERR");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x2000u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x2002u, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x2000u), "exact key window starts");
    expect(state, cpu->instructions == 5u, "canonical sequence executes five instructions");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) != 0u, "valid start sets WR");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "valid start sets WRERR pending state");
    dspic33_write_word(cpu, NVM_CONTROL, 1u);
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) != 0u,
           "software zero cannot clear active WR");
    expect(state, cpu->events.count == 1u, "active write has one completion event");
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE | 1u);
    expect(state, cpu->events.count == 1u, "repeated WR does not reschedule");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "valid key operation completes");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) == 0u,
           "normal completion clears WRERR");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x2000u) == 0x00123456u &&
               dspic33_nvm_test_program_word(cpu, 0x2002u) == 0x00654321u,
           "canonical sequence programs pair");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    load_start_sequence(cpu, false);
    expect(state, step_instructions(cpu, 2u), "step through first key");
    dspic33_load_program_word(cpu, 0x0016u, NVM_SEQUENCE_BASE + 4u);
    dspic33_write_word(cpu, 0x0800u, 0x0002u);
    dspic33_write_word(cpu, 0x0820u, 0x0002u);
    dspic33_write_word(cpu, 0x0840u, 0x0030u);
    expect(state, dspic33_device_service_interrupt(cpu), "interrupt between keys");
    expect(state, step_instructions(cpu, 3u), "step remainder after key interrupt");
    expect(state, !cpu->nvm.active, "interrupt invalidates first key");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    load_start_sequence(cpu, false);
    expect(state, step_instructions(cpu, 4u), "step through second key");
    dspic33_load_program_word(cpu, 0x0016u, NVM_SEQUENCE_BASE + 8u);
    dspic33_write_word(cpu, 0x0800u, 0x0002u);
    dspic33_write_word(cpu, 0x0820u, 0x0002u);
    dspic33_write_word(cpu, 0x0840u, 0x0030u);
    expect(state, dspic33_device_service_interrupt(cpu), "interrupt before WR");
    expect(state, step_instructions(cpu, 1u), "step WR after interrupt");
    expect(state, !cpu->nvm.active, "interrupt invalidates unlocked WR");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2000u, true);
    load_start_sequence(cpu, false);
    expect(state, step_instructions(cpu, 4u), "step unlock before trap");
    dspic33_load_program_word(cpu, 0x000eu, NVM_SEQUENCE_BASE + 8u);
    dspic33_raise_dma_collision_trap(cpu);
    expect(state, step_instructions(cpu, 1u), "step WR after trap");
    expect(state, !cpu->nvm.active, "trap invalidates unlocked WR");
}

void dspic33_nvm_test_key_byte_access_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2100u, true);
    dspic33_load_program_word(cpu, 0x2100u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x2102u, 0x00ffffffu);
    cpu->write_latches[0] = 0x00010203u;
    cpu->write_latches[1] = 0x00040506u;
    cpu->instructions = 1u;
    dspic33_write_byte(cpu, NVM_KEY, 0x55u);
    cpu->instructions = 3u;
    dspic33_write_byte(cpu, NVM_KEY, 0xaau);
    cpu->instructions = 4u;
    dspic33_write_byte(cpu, NVM_CONTROL + 1u, 0xc0u);
    expect(state, cpu->nvm.active, "low-byte keys authorize WR");
    expect(state, cpu->nvm.key_instruction == 3u, "low-byte AA captures instruction number");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "low-byte key operation completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x2100u) == 0x00010203u &&
               dspic33_nvm_test_program_word(cpu, 0x2102u) == 0x00040506u,
           "low-byte key operation programs pair");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x2100u, true);
    cpu->instructions = 1u;
    dspic33_write_byte(cpu, NVM_KEY, 0x55u);
    cpu->instructions = 2u;
    dspic33_write_byte(cpu, NVM_KEY + 1u, 0xaau);
    cpu->instructions = 3u;
    dspic33_write_byte(cpu, NVM_CONTROL + 1u, 0xc0u);
    expect(state, !cpu->nvm.active, "high-byte key write does not authorize WR");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "high-byte key attempt sets WRERR");
}

void dspic33_nvm_test_invalid_operation_cases(TestState* state, Dspic33* cpu) {
    uint16_t operation;
    for (operation = 4u; operation < 16u; operation++) {
        if (operation == 0x0au || operation == 0x0du) {
            continue;
        }
        dspic33_reset(cpu, 0u);
        dspic33_nvm_test_configure_operation(cpu, operation, 0x2000u, true);
        expect(state, dspic33_nvm_test_execute_start_sequence(cpu, false),
               "reserved operation sequence executes");
        expect(state, !cpu->nvm.active, "invalid NVMOP rejected");
        expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
               "invalid NVMOP leaves WR clear");
        expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
               "invalid NVMOP sets WRERR");
        expect(state, cpu->events.count == 0u, "invalid NVMOP queues no event");
        expect(state, !dspic33_nvm_test_interrupt_flag(cpu), "invalid NVMOP raises no interrupt");
    }
}

void dspic33_nvm_test_invalid_target_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t targets[] = {0x2000u, DSPIC33_CONFIGURATION_BASE,
                                       DSPIC33_CONFIGURATION_BASE, DSPIC33_CONFIGURATION_BASE};
    uint16_t operation;
    for (operation = 0u; operation < 4u; operation++) {
        dspic33_reset(cpu, 0u);
        dspic33_nvm_test_configure_operation(cpu, operation, targets[operation], true);
        expect(state, dspic33_nvm_test_execute_start_sequence(cpu, false),
               "invalid target sequence executes");
        expect(state, !cpu->nvm.active, "invalid operation target rejected");
        expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
               "invalid operation target sets WRERR");
        expect(state, cpu->events.count == 0u, "invalid target queues no event");
        expect(state, !dspic33_nvm_test_interrupt_flag(cpu), "invalid target raises no interrupt");
    }
    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 0u, DSPIC33_CONFIGURATION_BASE + 5u, true);
    expect(state, dspic33_nvm_test_execute_start_sequence(cpu, false),
           "synthetic configuration sequence executes");
    expect(state, !cpu->nvm.active, "synthetic configuration address rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "synthetic configuration address sets WRERR");
}

void dspic33_nvm_test_program_range_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x557fcu, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x557feu, 0x00ffffffu);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x557feu), "main upper pair starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "main upper pair completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x557fcu) == 0x00112233u &&
               dspic33_nvm_test_program_word(cpu, 0x557feu) == 0x00445566u,
           "main upper pair programmed");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, DSPIC33_PROGRAM_LIMIT, true);
    expect(state, dspic33_nvm_test_execute_start_sequence(cpu, false),
           "main out-of-range sequence executes");
    expect(state, !cpu->nvm.active, "main out-of-range pair rejected");
}

void dspic33_nvm_test_configuration_operation_cases(TestState* state, Dspic33* cpu) {
    uint32_t target = DSPIC33_CONFIGURATION_BASE + 4u;
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_configuration_word(cpu, target, 0xffcfu),
           "load FGS configuration pair");
    cpu->write_latches[0] = 0x000000fdu;
    expect(state, dspic33_nvm_test_start_operation(cpu, 0u, target), "configuration byte starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "configuration byte completes");
    expect(state, dspic33_read_configuration_byte(cpu, target) == 0xfdu,
           "FGS key and protection bits replaced");
    expect(state, dspic33_read_configuration_byte(cpu, target + 1u) == 0xffu,
           "configuration adjacent byte unchanged");
    expect(state, dspic33_read_program_byte(cpu, target) == 0xfdu,
           "FGS CPU view follows programmed raw byte");
    expect(state, !cpu->nvm.active, "configuration completion inactive");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
           "configuration completion clears WR");
    expect(state, dspic33_nvm_test_interrupt_flag(cpu), "configuration completion raises NVMIF");
    cpu->write_latches[0] = 0xffu;
    expect(state, dspic33_nvm_test_start_operation(cpu, 0u, target),
           "FGS protection rewrite starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "FGS protection rewrite completes");
    expect(state, dspic33_read_configuration_byte(cpu, target) == 0xfdu,
           "FGS protection bit cannot change zero to one");

    target = DSPIC33_CONFIGURATION_BASE + 6u;
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_configuration_word(cpu, target, 0xa578u),
           "load FOSCSEL configuration pair");
    cpu->write_latches[0] = 0u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 0u, target), "FOSCSEL clear starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "FOSCSEL clear completes");
    expect(state,
           dspic33_read_configuration_byte(cpu, target) == 0x78u &&
               dspic33_read_configuration_byte(cpu, target + 1u) == 0xa5u,
           "FOSCSEL clear preserves unimplemented raw bits");
    expect(state, dspic33_read_program_byte(cpu, target) == 0x78u,
           "FOSCSEL clear updates CPU view");

    cpu->write_latches[0] = 0x87u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 0u, target), "FOSCSEL set starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "FOSCSEL set completes");
    expect(state,
           dspic33_read_configuration_byte(cpu, target) == 0xffu &&
               dspic33_read_configuration_byte(cpu, target + 1u) == 0xa5u,
           "FOSCSEL supports zero-to-one reprogramming");
    expect(state, dspic33_read_program_byte(cpu, target) == 0xffu, "FOSCSEL set updates CPU view");

    target = DSPIC33_CONFIGURATION_BASE + 0x10u;
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 4u, 0xffcfu),
           "load unprotected FGS before FAS programming");
    expect(state, dspic33_load_configuration_word(cpu, target, 0xffcfu),
           "load FAS configuration pair");
    cpu->write_latches[0] = 0xfdu;
    expect(state, dspic33_nvm_test_start_operation(cpu, 0u, target), "FAS programming starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "FAS programming completes");
    expect(state,
           dspic33_read_configuration_byte(cpu, target) == 0xfdu &&
               dspic33_read_configuration_byte(cpu, target + 1u) == 0xffu,
           "FAS key and protection bits replaced");
    cpu->write_latches[0] = 0xffu;
    expect(state, dspic33_nvm_test_start_operation(cpu, 0u, target),
           "FAS protection rewrite starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "FAS protection rewrite completes");
    expect(state, dspic33_read_configuration_byte(cpu, target) == 0xfdu,
           "FAS protection bit cannot change zero to one");
    dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 4u, 0xffcfu);
    dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 0x10u, 0xffcfu);
}

static uint8_t programmed_configuration_value(uint8_t index, uint8_t current, uint8_t latch) {
    static const uint8_t masks[] = {0x33u, 0x87u, 0xe7u, 0xffu, 0x3fu, 0xf7u, 0x33u, 0xffu};
    uint8_t mask = masks[index];
    if (index == 0u || index == 6u) {
        return (uint8_t)((current & (uint8_t)~mask) | (latch & 0x30u) | (current & latch & 0x03u));
    }
    return (uint8_t)((current & (uint8_t)~mask) | (latch & mask));
}

static void complete_configuration_byte(Dspic33* cpu, uint8_t index, uint8_t current,
                                        uint8_t latch) {
    uint32_t offset = 4u + (uint32_t)index * 2u;
    cpu->events.count = 0u;
    cpu->configuration[offset] = current;
    cpu->configuration[offset + 1u] = 0xa5u;
    cpu->nvm.control = 0u;
    cpu->nvm.address = DSPIC33_CONFIGURATION_BASE + offset;
    cpu->nvm.latches[0] = latch;
    dspic33_complete_nvm(cpu);
}

void dspic33_nvm_test_configuration_programming_matrix_cases(TestState* state, Dspic33* cpu) {
    uint16_t current;
    uint16_t latch;
    uint16_t general;
    uint16_t auxiliary;
    uint8_t index;

    dspic33_reset(cpu, 0u);
    for (index = 0u; index < 8u; index++) {
        uint32_t offset = 4u + (uint32_t)index * 2u;
        if (index == 6u) {
            continue;
        }
        for (current = 0u; current < 0x100u; current++) {
            for (latch = 0u; latch < 0x100u; latch++) {
                complete_configuration_byte(cpu, index, (uint8_t)current, (uint8_t)latch);
                expect(state,
                       cpu->configuration[offset] == programmed_configuration_value(
                                                         index, (uint8_t)current, (uint8_t)latch) &&
                           cpu->configuration[offset + 1u] == 0xa5u,
                       "configuration programming value matrix");
            }
        }
    }

    for (general = 0u; general < 0x100u; general++) {
        for (auxiliary = 0u; auxiliary < 0x100u; auxiliary++) {
            bool allowed =
                ((uint8_t)general & 0x33u) == 0x03u && ((uint8_t)auxiliary & 0x33u) == 0x03u;
            cpu->configuration[4u] = (uint8_t)general;
            complete_configuration_byte(cpu, 6u, (uint8_t)auxiliary, 0x31u);
            expect(state,
                   cpu->configuration[4u] == (uint8_t)general &&
                       cpu->configuration[0x10u] ==
                           (allowed ? programmed_configuration_value(6u, (uint8_t)auxiliary, 0x31u)
                                    : (uint8_t)auxiliary) &&
                       cpu->configuration[0x11u] == 0xa5u,
                   "FAS programming admission matrix");
            if (!allowed) {
                continue;
            }
            for (latch = 0u; latch < 0x100u; latch++) {
                cpu->configuration[4u] = (uint8_t)general;
                complete_configuration_byte(cpu, 6u, (uint8_t)auxiliary, (uint8_t)latch);
                expect(state,
                       cpu->configuration[4u] == (uint8_t)general &&
                           cpu->configuration[0x10u] ==
                               programmed_configuration_value(6u, (uint8_t)auxiliary,
                                                              (uint8_t)latch) &&
                           cpu->configuration[0x11u] == 0xa5u,
                       "FAS programming value matrix");
            }
        }
    }
    dspic33_load_configuration_word(cpu, CODEGUARD_GENERAL_CONFIGURATION, 0xffcfu);
    dspic33_load_configuration_word(cpu, CODEGUARD_AUXILIARY_CONFIGURATION, 0xffcfu);
}

void dspic33_nvm_test_pair_and_capture_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_program_word(cpu, 0x2200u, 0x00f0f0f0u), "load pair first target");
    expect(state, dspic33_load_program_word(cpu, 0x2202u, 0x000f0f0fu), "load pair second target");
    expect(state, dspic33_load_program_word(cpu, 0x2300u, 0x00112233u),
           "load pair alternate target");
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x2202u), "pair operation starts");
    expect(state, cpu->nvm.address == 0x2202u, "pair address captured");
    expect(state, (cpu->nvm.control & 0x000fu) == 1u, "pair operation captured");
    cpu->write_latches[0] = 0x00000000u;
    cpu->write_latches[1] = 0x00000000u;
    dspic33_write_word(cpu, NVM_ADDRESS, 0x2300u);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | 3u);
    dspic33_write_word(cpu, 0x0760u, 0xffffu);
    dspic33_write_word(cpu, 0x0762u, 0xffffu);
    dspic33_write_word(cpu, 0x0764u, 0xffffu);
    dspic33_write_word(cpu, 0x0766u, 0xffffu);
    dspic33_write_word(cpu, 0x0768u, 0xffffu);
    dspic33_write_word(cpu, 0x076au, 0xffffu);
    dspic33_write_word(cpu, 0x076cu, 0xffffu);
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) != 0u,
           "captured operation remains active after register changes");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "captured pair completes");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2200u) == 0x00103050u,
           "captured pair first word programmed");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2202u) == 0x00050301u,
           "captured pair second word programmed");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2300u) == 0x00112233u,
           "changed address remains untouched");
    expect(state, cpu->write_latches[0] == 0u && cpu->write_latches[1] == 0u,
           "post-start latch values retained");
    expect(state, dspic33_nvm_test_interrupt_flag(cpu), "pair completion raises NVMIF");
}

void dspic33_nvm_test_row_operation_cases(TestState* state, Dspic33* cpu) {
    uint32_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        dspic33_load_program_word(cpu, 0x2400u + index * 2u, 0x00ffffffu);
        cpu->write_latches[index] = 0x00550000u | index;
    }
    dspic33_load_program_word(cpu, 0x23feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2500u, 0x00040506u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 2u, 0x247au), "row operation starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "row operation completes");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2400u) == 0x00550000u,
           "row first word programmed");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x247eu) == 0x0055003fu,
           "row middle word programmed");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x24feu) == 0x0055007fu,
           "row last word programmed");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x23feu) == 0x00010203u,
           "row preceding word unchanged");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2500u) == 0x00040506u,
           "row following word unchanged");
    expect(state,
           cpu->write_latches[0] == 0x00550000u &&
               cpu->write_latches[DSPIC33_WRITE_LATCH_WORDS - 1u] == 0x0055007fu,
           "row completion retains write latches");
    dspic33_load_program_word(cpu, 0x2600u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x2602u, 0x00ffffffu);
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x2600u), "retained-latch pair starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "retained-latch pair completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x2600u) == 0x00550000u &&
               dspic33_nvm_test_program_word(cpu, 0x2602u) == 0x00550001u,
           "second target reuses retained write latches");
}
