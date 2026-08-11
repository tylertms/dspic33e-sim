#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device.h"
#include "dspic33.h"
#include "elf_image.h"
#include "hex_image.h"
#include "sfr_side_effect_coverage.h"

static const SfrSideEffectCoverage nvm_sfr_side_effect_coverage[] = {
    {0x0728u, 0x8000u},
};

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} NvmConformance;

enum {
    NVM_CONTROL = 0x0728u,
    NVM_ADDRESS = 0x072au,
    NVM_ADDRESS_HIGH = 0x072cu,
    NVM_KEY = 0x072eu,
    MAIN_CLOCK_DIVISOR = 0x0744u,
    NVM_WRITE = 0x8000u,
    NVM_WRITE_ENABLE = 0x4000u,
    NVM_WRITE_ERROR = 0x2000u,
    NVM_IRQ = 15u,
    NVM_SEQUENCE_BASE = 0x0400u,
    CODEGUARD_GENERAL_CONFIGURATION = 0xf80004u,
    CODEGUARD_AUXILIARY_CONFIGURATION = 0xf80010u,
    PERSISTENT_PROGRAM_TAG = 0x01000000u,
    PERSISTENT_PROGRAM_BASE = 0x2000u,
    PERSISTENT_PROGRAM_LIMIT = 0x5000u,
    MOVE_KEY_55 = 0x200550u,
    MOVE_KEY_AA = 0x200aa0u,
    WRITE_NVM_KEY = 0x883970u,
    SET_NVM_WRITE = 0xa8e729u,
    TBLRDL_W2_W3 = 0xba0192u,
    TBLRDL_BYTE_W2_W3 = 0xba4192u,
    TBLRDH_W2_W3 = 0xba8192u,
    TBLRDH_BYTE_W2_W3 = 0xbac192u,
    OPCODE_NOP = 0x000000u,
    OPCODE_RESET = 0xfe0000u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_IDLE = 0xfe4001u,
    OPCODE_RETURN = 0x060000u,
    OPCODE_RETFIE = 0x064000u,
    OPCODE_COMPUTED_CALL_W0 = 0x018800u,
    OPCODE_COMPUTED_GOTO_W0 = 0x018c00u,
    OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT = 0x4112b4u,
    OPCODE_MOV_LITERAL_0X1234_W2 = 0x212342u,
    OPCODE_MOV_W1_W2 = 0x780111u,
    OPCODE_BTSC_W2_BIT_0 = 0xa70002u,
    OPCODE_DO_1 = 0x080001u,
    OPCODE_GOTO_0X100 = 0x040100u
};

static void expect(NvmConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[nvm-failed] %s\n", name);
    }
}

static bool interrupt_flag(Dspic33* cpu) {
    return (dspic33_read_word(cpu, 0x0800u) & 0x8000u) != 0u;
}

static uint32_t program_word(const Dspic33* cpu, uint32_t address) {
    return dspic33_read_program_word(cpu, address);
}

static void configure_operation(Dspic33* cpu, uint16_t operation, uint32_t address,
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

static bool read_table(Dspic33* cpu, uint32_t address, uint32_t opcode,
                       uint16_t* value) {
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

static void configuration_table_view_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint32_t addresses[] = {0xf80004u, 0xf80006u, 0xf80008u, 0xf8000au,
                                         0xf8000cu, 0xf8000eu, 0xf80010u, 0xf80012u};
    static const uint8_t defaults[] = {0xcfu, 0xffu, 0xffu, 0xffu,
                                       0xffu, 0xdfu, 0xcfu, 0xffu};
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
               read_table(cpu, address, TBLRDL_W2_W3, &value) &&
                   value == defaults[index],
               "configuration factory TBLRDL");
        expect(state, read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
               "configuration factory TBLRDH");
    }

    for (index = 0u; index < sizeof(addresses) / sizeof(addresses[0]); index++) {
        uint32_t address = addresses[index];
        expect(state, dspic33_load_configuration_word(cpu, address, loaded[index]),
               "load raw configuration pair");
        expect(state,
               dspic33_read_configuration_byte(cpu, address) ==
                       (uint8_t)loaded[index] &&
                   dspic33_read_configuration_byte(cpu, address + 1u) ==
                       (uint8_t)(loaded[index] >> 8u),
               "raw configuration API preserves pair");
        expect(state,
               dspic33_read_program_byte(cpu, address) == (uint8_t)loaded[index] &&
                   dspic33_read_program_byte(cpu, address + 1u) == 0u,
               "CPU configuration view zero extends low byte");
        expect(state,
               read_table(cpu, address, TBLRDL_W2_W3, &value) &&
                   value == (uint8_t)loaded[index],
               "loaded configuration TBLRDL");
        expect(state, read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
               "loaded configuration TBLRDH");
    }

    expect(state,
           read_table(cpu, addresses[0], TBLRDL_BYTE_W2_W3, &value) && value == 0xa511u,
           "configuration TBLRDL low byte");
    expect(state,
           read_table(cpu, addresses[0] + 1u, TBLRDL_BYTE_W2_W3, &value) &&
               value == 0xa500u,
           "configuration TBLRDL upper byte zero");
    expect(state,
           read_table(cpu, addresses[0], TBLRDH_BYTE_W2_W3, &value) && value == 0xa500u,
           "configuration TBLRDH low byte zero");
    expect(state,
           read_table(cpu, addresses[0] + 1u, TBLRDH_BYTE_W2_W3, &value) &&
               value == 0xa500u,
           "configuration TBLRDH upper byte zero");

    for (index = 0u; index < sizeof(ids) / sizeof(ids[0]); index++) {
        uint32_t address = ids[index];
        uint16_t expected = id_values[index];
        expect(state,
               read_table(cpu, address, TBLRDL_W2_W3, &value) && value == expected,
               "device ID TBLRDL");
        expect(state, read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
               "device ID TBLRDH");
        expect(state,
               read_table(cpu, address, TBLRDL_BYTE_W2_W3, &value) &&
                   value == (uint16_t)(0xa500u | (uint8_t)expected),
               "device ID TBLRDL low byte");
        expect(state,
               read_table(cpu, address + 1u, TBLRDL_BYTE_W2_W3, &value) &&
                   value == (uint16_t)(0xa500u | (uint8_t)(expected >> 8u)),
               "device ID TBLRDL upper byte");
        expect(state,
               read_table(cpu, address, TBLRDH_BYTE_W2_W3, &value) && value == 0xa500u,
               "device ID TBLRDH low byte zero");
        expect(state,
               read_table(cpu, address + 1u, TBLRDH_BYTE_W2_W3, &value) &&
                   value == 0xa500u,
               "device ID TBLRDH upper byte zero");
        expect(state,
               dspic33_read_program_byte(cpu, address) == (uint8_t)expected &&
                   dspic33_read_program_byte(cpu, address + 1u) ==
                       (uint8_t)(expected >> 8u),
               "device ID host program bytes");
    }

    for (index = 0u; index < sizeof(reserved) / sizeof(reserved[0]); index++) {
        uint32_t address = reserved[index];
        expect(state, read_table(cpu, address, TBLRDL_W2_W3, &value) && value == 0u,
               "reserved configuration TBLRDL zero");
        expect(state, read_table(cpu, address, TBLRDH_W2_W3, &value) && value == 0u,
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
               dspic33_read_configuration_byte(&copy, addresses[0]) ==
                       (uint8_t)loaded[0] &&
                   dspic33_read_configuration_byte(&copy, addresses[0] + 1u) ==
                       (uint8_t)(loaded[0] >> 8u),
               "copy preserves raw configuration pair");
        expect(state,
               read_table(&copy, addresses[0], TBLRDL_W2_W3, &value) &&
                   value == (uint8_t)loaded[0],
               "copy preserves CPU configuration view");
        dspic33_destroy(&copy);
    }
}

static void load_start_sequence_at(Dspic33* cpu, uint32_t base, bool delayed_write) {
    dspic33_load_program_word(cpu, base, MOVE_KEY_55);
    dspic33_load_program_word(cpu, base + 2u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, base + 4u, MOVE_KEY_AA);
    dspic33_load_program_word(cpu, base + 6u, WRITE_NVM_KEY);
    dspic33_load_program_word(cpu, base + 8u,
                              delayed_write ? 0x000000u : SET_NVM_WRITE);
    dspic33_load_program_word(cpu, base + 10u, SET_NVM_WRITE);
    cpu->pc = base;
}

static void load_start_sequence(Dspic33* cpu, bool delayed_write) {
    load_start_sequence_at(cpu, NVM_SEQUENCE_BASE, delayed_write);
}

static bool execute_start_sequence(Dspic33* cpu, bool delayed_write) {
    load_start_sequence(cpu, delayed_write);
    return step_instructions(cpu, delayed_write ? 6u : 5u);
}

static bool start_operation(Dspic33* cpu, uint16_t operation, uint32_t address) {
    configure_operation(cpu, operation, address, true);
    if (!execute_start_sequence(cpu, false)) {
        return false;
    }
    return cpu->nvm.active;
}

static bool start_operation_from(Dspic33* cpu, uint16_t operation, uint32_t address,
                                 uint32_t execution_address) {
    configure_operation(cpu, operation, address, true);
    load_start_sequence_at(cpu, execution_address, false);
    if (!step_instructions(cpu, 5u)) {
        return false;
    }
    return cpu->nvm.active;
}

static bool finish_operation(Dspic33* cpu) { return dspic33_device_advance(cpu, 2u); }

static uint8_t codeguard_configuration_value(uint8_t index) {
    return (uint8_t)((index & 0x03u) | ((index & 0x0cu) << 2u));
}

static bool codeguard_configuration_high(uint8_t configuration) {
    uint8_t protection = (uint8_t)(configuration & 0x03u);
    uint8_t expected_key = protection == 0x03u ? 0u : 0x30u;
    return (protection & 0x02u) == 0u || (configuration & 0x30u) != expected_key;
}

static bool load_codeguard_configuration(Dspic33* cpu, uint8_t general,
                                         uint8_t auxiliary) {
    return dspic33_load_configuration_word(cpu, CODEGUARD_GENERAL_CONFIGURATION,
                                           (uint16_t)(0xff00u | general)) &&
           dspic33_load_configuration_word(cpu, CODEGUARD_AUXILIARY_CONFIGURATION,
                                           (uint16_t)(0xff00u | auxiliary));
}

static uint16_t execute_codeguard_table_read(Dspic33* cpu, uint32_t origin,
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

static uint16_t execute_codeguard_psv_read(Dspic33* cpu, uint32_t origin,
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

static void load_long_program_flow(Dspic33* cpu, uint32_t origin, uint32_t target,
                                   bool call) {
    dspic33_load_program_word(cpu, origin,
                              (call ? 0x020000u : 0x040000u) | (target & 0xffffu));
    dspic33_load_program_word(cpu, origin + 2u, (target >> 16u) & 0x007fu);
}

static void load_program_return(Dspic33* cpu, uint32_t origin, uint32_t target,
                                uint32_t opcode) {
    dspic33_load_program_word(cpu, origin, opcode);
    dspic33_set_working_register(cpu, 15u, 0x1004u);
    dspic33_write_word(cpu, 0x1000u, (uint16_t)target);
    dspic33_write_word(cpu, 0x1002u, (uint16_t)(target >> 16u));
}

static bool codeguard_security_reset(const Dspic33* cpu, uint64_t reset_count) {
    return cpu->illegal_reset && cpu->illegal_reset_count == reset_count + 1u &&
           cpu->pc == 0u && (cpu->data[0x0741u] & 0x40u) != 0u;
}

static void reset_and_access_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == 0u, "NVMCON reset");
    expect(state, dspic33_read_word(cpu, NVM_KEY) == 0u, "NVMKEY read zero");
    expect(state, !cpu->nvm.active, "NVM reset inactive");
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ERROR);
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == NVM_WRITE_ERROR,
           "WRERR software set");
    dspic33_write_word(cpu, NVM_CONTROL, 0u);
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == 0u, "WRERR software clear");

    configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_CONTROL,
                       (uint16_t)(dspic33_read_word(cpu, NVM_CONTROL) | NVM_WRITE));
    expect(state, !cpu->nvm.active, "WR without keys rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
           "rejected WR remains clear");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "rejected WR sets WRERR");
    expect(state, cpu->events.count == 0u, "rejected WR queues no event");
    expect(state, !interrupt_flag(cpu), "rejected WR raises no interrupt");
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | 1u);
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) == 0u,
           "WRERR clears after rejected write");
}

static void key_sequence_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_KEY, 0x00aau);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "reverse keys rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "reverse keys set WRERR");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_KEY, 0x0000u);
    dspic33_write_word(cpu, NVM_KEY, 0x00aau);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "wrong intervening key rejected");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2000u, true);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_KEY, 0x0055u);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "repeated first key rejected");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2000u, false);
    expect(state, execute_start_sequence(cpu, false), "missing WREN sequence executes");
    expect(state, !cpu->nvm.active, "missing WREN rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "missing WREN sets WRERR");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2000u, true);
    expect(state, execute_start_sequence(cpu, true), "delayed WR sequence executes");
    expect(state, !cpu->nvm.active, "expired WR window rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "expired WR window sets WRERR");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x2000u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x2002u, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, start_operation(cpu, 1u, 0x2000u), "exact key window starts");
    expect(state, cpu->instructions == 5u,
           "canonical sequence executes five instructions");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) != 0u,
           "valid start sets WR");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "valid start sets WRERR pending state");
    dspic33_write_word(cpu, NVM_CONTROL, 1u);
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) != 0u,
           "software zero cannot clear active WR");
    expect(state, cpu->events.count == 1u, "active write has one completion event");
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE | 1u);
    expect(state, cpu->events.count == 1u, "repeated WR does not reschedule");
    expect(state, finish_operation(cpu), "valid key operation completes");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) == 0u,
           "normal completion clears WRERR");
    expect(state,
           program_word(cpu, 0x2000u) == 0x00123456u &&
               program_word(cpu, 0x2002u) == 0x00654321u,
           "canonical sequence programs pair");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2000u, true);
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
    configure_operation(cpu, 1u, 0x2000u, true);
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
    configure_operation(cpu, 1u, 0x2000u, true);
    load_start_sequence(cpu, false);
    expect(state, step_instructions(cpu, 4u), "step unlock before trap");
    dspic33_load_program_word(cpu, 0x000eu, NVM_SEQUENCE_BASE + 8u);
    dspic33_raise_dma_collision_trap(cpu);
    expect(state, step_instructions(cpu, 1u), "step WR after trap");
    expect(state, !cpu->nvm.active, "trap invalidates unlocked WR");
}

static void key_byte_access_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2100u, true);
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
    expect(state, cpu->nvm.key_instruction == 3u,
           "low-byte AA captures instruction number");
    expect(state, finish_operation(cpu), "low-byte key operation completes");
    expect(state,
           program_word(cpu, 0x2100u) == 0x00010203u &&
               program_word(cpu, 0x2102u) == 0x00040506u,
           "low-byte key operation programs pair");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x2100u, true);
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

static void invalid_operation_cases(NvmConformance* state, Dspic33* cpu) {
    uint16_t operation;
    for (operation = 4u; operation < 16u; operation++) {
        if (operation == 0x0au || operation == 0x0du) {
            continue;
        }
        dspic33_reset(cpu, 0u);
        configure_operation(cpu, operation, 0x2000u, true);
        expect(state, execute_start_sequence(cpu, false),
               "reserved operation sequence executes");
        expect(state, !cpu->nvm.active, "invalid NVMOP rejected");
        expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
               "invalid NVMOP leaves WR clear");
        expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
               "invalid NVMOP sets WRERR");
        expect(state, cpu->events.count == 0u, "invalid NVMOP queues no event");
        expect(state, !interrupt_flag(cpu), "invalid NVMOP raises no interrupt");
    }
}

static void invalid_target_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint32_t targets[] = {0x2000u, DSPIC33_CONFIGURATION_BASE,
                                       DSPIC33_CONFIGURATION_BASE,
                                       DSPIC33_CONFIGURATION_BASE};
    uint16_t operation;
    for (operation = 0u; operation < 4u; operation++) {
        dspic33_reset(cpu, 0u);
        configure_operation(cpu, operation, targets[operation], true);
        expect(state, execute_start_sequence(cpu, false),
               "invalid target sequence executes");
        expect(state, !cpu->nvm.active, "invalid operation target rejected");
        expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
               "invalid operation target sets WRERR");
        expect(state, cpu->events.count == 0u, "invalid target queues no event");
        expect(state, !interrupt_flag(cpu), "invalid target raises no interrupt");
    }
    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 0u, DSPIC33_CONFIGURATION_BASE + 5u, true);
    expect(state, execute_start_sequence(cpu, false),
           "synthetic configuration sequence executes");
    expect(state, !cpu->nvm.active, "synthetic configuration address rejected");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "synthetic configuration address sets WRERR");
}

static void program_range_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x557fcu, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x557feu, 0x00ffffffu);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state, start_operation(cpu, 1u, 0x557feu), "main upper pair starts");
    expect(state, finish_operation(cpu), "main upper pair completes");
    expect(state,
           program_word(cpu, 0x557fcu) == 0x00112233u &&
               program_word(cpu, 0x557feu) == 0x00445566u,
           "main upper pair programmed");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, DSPIC33_PROGRAM_LIMIT, true);
    expect(state, execute_start_sequence(cpu, false),
           "main out-of-range sequence executes");
    expect(state, !cpu->nvm.active, "main out-of-range pair rejected");
}

static void configuration_operation_cases(NvmConformance* state, Dspic33* cpu) {
    uint32_t target = DSPIC33_CONFIGURATION_BASE + 4u;
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_configuration_word(cpu, target, 0xffcfu),
           "load FGS configuration pair");
    cpu->write_latches[0] = 0x000000fdu;
    expect(state, start_operation(cpu, 0u, target), "configuration byte starts");
    expect(state, finish_operation(cpu), "configuration byte completes");
    expect(state, dspic33_read_configuration_byte(cpu, target) == 0xfdu,
           "FGS key and protection bits replaced");
    expect(state, dspic33_read_configuration_byte(cpu, target + 1u) == 0xffu,
           "configuration adjacent byte unchanged");
    expect(state, dspic33_read_program_byte(cpu, target) == 0xfdu,
           "FGS CPU view follows programmed raw byte");
    expect(state, !cpu->nvm.active, "configuration completion inactive");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
           "configuration completion clears WR");
    expect(state, interrupt_flag(cpu), "configuration completion raises NVMIF");
    cpu->write_latches[0] = 0xffu;
    expect(state, start_operation(cpu, 0u, target), "FGS protection rewrite starts");
    expect(state, finish_operation(cpu), "FGS protection rewrite completes");
    expect(state, dspic33_read_configuration_byte(cpu, target) == 0xfdu,
           "FGS protection bit cannot change zero to one");

    target = DSPIC33_CONFIGURATION_BASE + 6u;
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_configuration_word(cpu, target, 0xa578u),
           "load FOSCSEL configuration pair");
    cpu->write_latches[0] = 0u;
    expect(state, start_operation(cpu, 0u, target), "FOSCSEL clear starts");
    expect(state, finish_operation(cpu), "FOSCSEL clear completes");
    expect(state,
           dspic33_read_configuration_byte(cpu, target) == 0x78u &&
               dspic33_read_configuration_byte(cpu, target + 1u) == 0xa5u,
           "FOSCSEL clear preserves unimplemented raw bits");
    expect(state, dspic33_read_program_byte(cpu, target) == 0x78u,
           "FOSCSEL clear updates CPU view");

    cpu->write_latches[0] = 0x87u;
    expect(state, start_operation(cpu, 0u, target), "FOSCSEL set starts");
    expect(state, finish_operation(cpu), "FOSCSEL set completes");
    expect(state,
           dspic33_read_configuration_byte(cpu, target) == 0xffu &&
               dspic33_read_configuration_byte(cpu, target + 1u) == 0xa5u,
           "FOSCSEL supports zero-to-one reprogramming");
    expect(state, dspic33_read_program_byte(cpu, target) == 0xffu,
           "FOSCSEL set updates CPU view");

    target = DSPIC33_CONFIGURATION_BASE + 0x10u;
    dspic33_reset(cpu, 0u);
    expect(
        state,
        dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 4u, 0xffcfu),
        "load unprotected FGS before FAS programming");
    expect(state, dspic33_load_configuration_word(cpu, target, 0xffcfu),
           "load FAS configuration pair");
    cpu->write_latches[0] = 0xfdu;
    expect(state, start_operation(cpu, 0u, target), "FAS programming starts");
    expect(state, finish_operation(cpu), "FAS programming completes");
    expect(state,
           dspic33_read_configuration_byte(cpu, target) == 0xfdu &&
               dspic33_read_configuration_byte(cpu, target + 1u) == 0xffu,
           "FAS key and protection bits replaced");
    cpu->write_latches[0] = 0xffu;
    expect(state, start_operation(cpu, 0u, target), "FAS protection rewrite starts");
    expect(state, finish_operation(cpu), "FAS protection rewrite completes");
    expect(state, dspic33_read_configuration_byte(cpu, target) == 0xfdu,
           "FAS protection bit cannot change zero to one");
    dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 4u, 0xffcfu);
    dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 0x10u, 0xffcfu);
}

static uint8_t programmed_configuration_value(uint8_t index, uint8_t current,
                                              uint8_t latch) {
    static const uint8_t masks[] = {0x33u, 0x87u, 0xe7u, 0xffu,
                                    0x3fu, 0xf7u, 0x33u, 0xffu};
    uint8_t mask = masks[index];
    if (index == 0u || index == 6u) {
        return (uint8_t)((current & (uint8_t)~mask) | (latch & 0x30u) |
                         (current & latch & 0x03u));
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

static void configuration_programming_matrix_cases(NvmConformance* state,
                                                   Dspic33* cpu) {
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
                complete_configuration_byte(cpu, index, (uint8_t)current,
                                            (uint8_t)latch);
                expect(state,
                       cpu->configuration[offset] ==
                               programmed_configuration_value(index, (uint8_t)current,
                                                              (uint8_t)latch) &&
                           cpu->configuration[offset + 1u] == 0xa5u,
                       "configuration programming value matrix");
            }
        }
    }

    for (general = 0u; general < 0x100u; general++) {
        for (auxiliary = 0u; auxiliary < 0x100u; auxiliary++) {
            bool allowed = ((uint8_t)general & 0x33u) == 0x03u &&
                           ((uint8_t)auxiliary & 0x33u) == 0x03u;
            cpu->configuration[4u] = (uint8_t)general;
            complete_configuration_byte(cpu, 6u, (uint8_t)auxiliary, 0x31u);
            expect(state,
                   cpu->configuration[4u] == (uint8_t)general &&
                       cpu->configuration[0x10u] ==
                           (allowed ? programmed_configuration_value(
                                          6u, (uint8_t)auxiliary, 0x31u)
                                    : (uint8_t)auxiliary) &&
                       cpu->configuration[0x11u] == 0xa5u,
                   "FAS programming admission matrix");
            if (!allowed) {
                continue;
            }
            for (latch = 0u; latch < 0x100u; latch++) {
                cpu->configuration[4u] = (uint8_t)general;
                complete_configuration_byte(cpu, 6u, (uint8_t)auxiliary,
                                            (uint8_t)latch);
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

static void pair_and_capture_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_program_word(cpu, 0x2200u, 0x00f0f0f0u),
           "load pair first target");
    expect(state, dspic33_load_program_word(cpu, 0x2202u, 0x000f0f0fu),
           "load pair second target");
    expect(state, dspic33_load_program_word(cpu, 0x2300u, 0x00112233u),
           "load pair alternate target");
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, start_operation(cpu, 1u, 0x2202u), "pair operation starts");
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
    expect(state, finish_operation(cpu), "captured pair completes");
    expect(state, program_word(cpu, 0x2200u) == 0x00103050u,
           "captured pair first word programmed");
    expect(state, program_word(cpu, 0x2202u) == 0x00050301u,
           "captured pair second word programmed");
    expect(state, program_word(cpu, 0x2300u) == 0x00112233u,
           "changed address remains untouched");
    expect(state, cpu->write_latches[0] == 0u && cpu->write_latches[1] == 0u,
           "post-start latch values retained");
    expect(state, interrupt_flag(cpu), "pair completion raises NVMIF");
}

static void row_operation_cases(NvmConformance* state, Dspic33* cpu) {
    uint32_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        dspic33_load_program_word(cpu, 0x2400u + index * 2u, 0x00ffffffu);
        cpu->write_latches[index] = 0x00550000u | index;
    }
    dspic33_load_program_word(cpu, 0x23feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2500u, 0x00040506u);
    expect(state, start_operation(cpu, 2u, 0x247au), "row operation starts");
    expect(state, finish_operation(cpu), "row operation completes");
    expect(state, program_word(cpu, 0x2400u) == 0x00550000u,
           "row first word programmed");
    expect(state, program_word(cpu, 0x247eu) == 0x0055003fu,
           "row middle word programmed");
    expect(state, program_word(cpu, 0x24feu) == 0x0055007fu,
           "row last word programmed");
    expect(state, program_word(cpu, 0x23feu) == 0x00010203u,
           "row preceding word unchanged");
    expect(state, program_word(cpu, 0x2500u) == 0x00040506u,
           "row following word unchanged");
    expect(state,
           cpu->write_latches[0] == 0x00550000u &&
               cpu->write_latches[DSPIC33_WRITE_LATCH_WORDS - 1u] == 0x0055007fu,
           "row completion retains write latches");
    dspic33_load_program_word(cpu, 0x2600u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x2602u, 0x00ffffffu);
    expect(state, start_operation(cpu, 1u, 0x2600u), "retained-latch pair starts");
    expect(state, finish_operation(cpu), "retained-latch pair completes");
    expect(state,
           program_word(cpu, 0x2600u) == 0x00550000u &&
               program_word(cpu, 0x2602u) == 0x00550001u,
           "second target reuses retained write latches");
}

static void erase_operation_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x27feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2800u, 0u);
    dspic33_load_program_word(cpu, 0x2c00u, 0u);
    dspic33_load_program_word(cpu, 0x2ffeu, 0u);
    dspic33_load_program_word(cpu, 0x3000u, 0x00040506u);
    expect(state, start_operation(cpu, 3u, 0x2abcu), "page erase starts");
    expect(state, finish_operation(cpu), "page erase completes");
    expect(state, program_word(cpu, 0x2800u) == 0x00ffffffu, "page first word erased");
    expect(state, program_word(cpu, 0x2c00u) == 0x00ffffffu, "page middle word erased");
    expect(state, program_word(cpu, 0x2ffeu) == 0x00ffffffu, "page last word erased");
    expect(state, program_word(cpu, 0x27feu) == 0x00010203u,
           "page preceding word unchanged");
    expect(state, program_word(cpu, 0x3000u) == 0x00040506u,
           "page following word unchanged");
}

static void write_u16(uint8_t* bytes, uint32_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t* bytes, uint32_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8u);
    bytes[offset + 2u] = (uint8_t)(value >> 16u);
    bytes[offset + 3u] = (uint8_t)(value >> 24u);
}

static void auxiliary_loader_cases(NvmConformance* state, Dspic33* cpu) {
    static uint8_t hex_bytes[] = ":0200000400FFFB\n:0480000056341200E0\n:00000001FF\n";
    uint8_t elf_bytes[136] = {0u};
    HexImage hex = {hex_bytes, sizeof(hex_bytes) - 1u, false};
    ElfImage elf = {elf_bytes, sizeof(elf_bytes)};
    char error[128] = {0};

    dspic33_reset(cpu, 0u);
    expect(state, hex_image_load_program(&hex, cpu, error, sizeof(error)),
           "Intel HEX loads auxiliary program section");
    expect(state,
           dspic33_read_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) ==
               0x00123456u,
           "Intel HEX auxiliary word retained");

    elf_bytes[0] = 0x7fu;
    elf_bytes[1] = 'E';
    elf_bytes[2] = 'L';
    elf_bytes[3] = 'F';
    elf_bytes[4] = 1u;
    elf_bytes[5] = 1u;
    write_u16(elf_bytes, 16u, 2u);
    write_u16(elf_bytes, 18u, 118u);
    write_u32(elf_bytes, 32u, 52u);
    write_u16(elf_bytes, 46u, 40u);
    write_u16(elf_bytes, 48u, 2u);
    write_u32(elf_bytes, 96u, 1u);
    write_u32(elf_bytes, 100u, 0x40000000u);
    write_u32(elf_bytes, 104u, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u);
    write_u32(elf_bytes, 108u, 132u);
    write_u32(elf_bytes, 112u, 4u);
    write_u32(elf_bytes, 132u, 0x00654321u);
    memset(error, 0, sizeof(error));
    expect(state, elf_image_load_program(&elf, cpu, error, sizeof(error)),
           "ELF loads auxiliary program section");
    expect(state,
           dspic33_read_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u) ==
               0x00654321u,
           "ELF auxiliary word retained");
}

static void auxiliary_access_and_execution_cases(NvmConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    uint64_t instructions;
    uint16_t value;
    bool initialized;

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_program_range_implemented(DSPIC33_AUXILIARY_PROGRAM_BASE, 2u) &&
               dspic33_program_range_implemented(0x007ffffeu, 2u) &&
               !dspic33_program_range_implemented(DSPIC33_PROGRAM_LIMIT, 2u) &&
               !dspic33_program_range_implemented(DSPIC33_AUXILIARY_PROGRAM_LIMIT, 2u),
           "program map distinguishes primary gap and auxiliary segment");
    expect(
        state,
        dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00123456u) &&
            dspic33_load_program_word(cpu, 0x007ffffeu, 0x00654321u),
        "host loads auxiliary boundary words");
    expect(state,
           !dspic33_load_program_word(cpu, DSPIC33_PROGRAM_LIMIT, 0u) &&
               !dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_LIMIT, 0u),
           "host rejects unimplemented program addresses");
    expect(state,
           dspic33_read_program_byte(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x56u &&
               dspic33_read_program_byte(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 1u) ==
                   0x34u &&
               dspic33_read_program_byte(cpu, 0x007fffffu) == 0x43u,
           "host reads auxiliary byte lanes");
    expect(state,
           read_table(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, TBLRDL_W2_W3, &value) &&
               value == 0x3456u,
           "TBLRDL reads auxiliary low word");
    expect(state,
           read_table(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, TBLRDH_W2_W3, &value) &&
               value == 0x0012u,
           "TBLRDH reads auxiliary high byte");
    expect(state,
           read_table(cpu, 0x007ffffeu, TBLRDL_W2_W3, &value) && value == 0x4321u,
           "table read reaches auxiliary final word");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00123456u);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x02ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x3456u &&
               cpu->cycles == 5u,
           "PSV low word reads auxiliary Flash");
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x03ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x0012u &&
               cpu->cycles == 5u,
           "PSV high byte reads auxiliary Flash");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE,
                              OPCODE_MOV_LITERAL_0X1234_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u,
           "CPU executes auxiliary instruction");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE,
                              OPCODE_BTSC_W2_BIT_0);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u, OPCODE_NOP);
    cpu->w[2] = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 4u && cpu->cycles == 2u,
           "skip decodes following auxiliary instruction");

    dspic33_reset(cpu, 0x007ffffcu);
    dspic33_load_program_word(cpu, 0x007ffffcu, OPCODE_BTSC_W2_BIT_0);
    dspic33_load_program_word(cpu, 0x007ffffeu, OPCODE_NOP);
    cpu->w[2] = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0u && cpu->cycles == 2u,
           "taken skip wraps across auxiliary program limit");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_DO_1);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u, 1u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->do_depth == 1u &&
               cpu->do_start[0] == DSPIC33_AUXILIARY_PROGRAM_BASE + 4u &&
               cpu->do_end[0] == DSPIC33_AUXILIARY_PROGRAM_BASE + 6u,
           "DO reads auxiliary extension word");

    dspic33_reset(cpu, 0x007ffffeu);
    dspic33_load_program_word(cpu, 0x007ffffeu, OPCODE_NOP);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_LITERAL_0X1234_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0u &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u,
           "23-bit PC wraps from auxiliary final word to primary reset word");

    dspic33_reset(cpu, 0u);
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdbu);
    dspic33_load_program_word(cpu, 0u, OPCODE_RESET);
    dspic33_load_program_word(cpu, 0x007ffffcu, OPCODE_GOTO_0X100);
    dspic33_load_program_word(cpu, 0x007ffffeu, 0u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x007ffffcu,
           "RSTPRI selects auxiliary reset vector");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0100u,
           "auxiliary reset GOTO uses final two words");
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdfu);

    for (uint8_t index = 0u; index < 16u; index++) {
        uint8_t configuration = codeguard_configuration_value(index);
        bool protected = codeguard_configuration_high(configuration);
        uint64_t illegal_resets;
        uint64_t software_resets;
        dspic33_reset(cpu, 0u);
        cpu->stop_on_trap = false;
        expect(state, load_codeguard_configuration(cpu, 0x03u, configuration),
               "load B1 auxiliary reset protection");
        dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdbu);
        dspic33_load_program_word(cpu, 0u, OPCODE_RESET);
        dspic33_load_program_word(cpu, 0x007ffffcu, OPCODE_GOTO_0X100);
        dspic33_load_program_word(cpu, 0x007ffffeu, 0u);
        dspic33_load_program_word(cpu, 0x000100u, OPCODE_NOP);
        illegal_resets = cpu->illegal_reset_count;
        software_resets = cpu->software_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (protected
                        ? cpu->illegal_reset && cpu->pc == 0u &&
                              cpu->illegal_reset_count == illegal_resets + 1u &&
                              cpu->software_reset_count == software_resets + 1u &&
                              (dspic33_read_word(cpu, 0x0740u) & 0x4040u) == 0x4040u
                        : !cpu->illegal_reset && cpu->pc == 0x007ffffcu &&
                              cpu->illegal_reset_count == illegal_resets &&
                              cpu->software_reset_count == software_resets + 1u),
               "B1 auxiliary reset protection matrix");
        instructions = cpu->instructions;
        bool next_valid = dspic33_step(cpu) == DSPIC33_RUNNING &&
                          (protected ? cpu->illegal_reset && cpu->pc == 0u &&
                                           cpu->instructions == instructions
                                     : cpu->pc == 0x000100u &&
                                           cpu->instructions == instructions + 1u);
        expect(state, next_valid, "B1 auxiliary reset execution matrix");
    }
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdfu);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);

    dspic33_reset(cpu, 0u);
    expect(state, load_codeguard_configuration(cpu, 0x03u, 0x31u),
           "load B1 protected auxiliary hardware reset");
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdbu);
    instructions = cpu->instructions;
    dspic33_configuration_mismatch_reset(cpu);
    expect(state,
           cpu->reset_locked && cpu->illegal_reset && cpu->pc == 0u &&
               (dspic33_read_word(cpu, 0x0740u) & 0x4200u) == 0x4200u,
           "B1 protected auxiliary hardware reset locks execution");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->instructions == instructions,
           "B1 protected hardware reset cannot fetch application code");
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize B1 protected reset copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy B1 protected reset lock");
        expect(state,
               dspic33_step(&copy) == DSPIC33_RUNNING && copy.reset_locked &&
                   copy.instructions == instructions,
               "copied B1 protected reset remains locked");
        dspic33_destroy(&copy);
    }
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->reset_locked && !cpu->illegal_reset,
           "power-on reset clears B1 protected reset lock");
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdfu);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_NOP);
    dspic33_load_program_word(cpu, 0x007ffffau,
                              DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u,
                              OPCODE_NOP);
    dspic33_load_program_word(cpu, 0x0014u, 0x0200u);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_interrupt == 0u &&
               cpu->interrupt_log_entry[0] == DSPIC33_AUXILIARY_PROGRAM_BASE &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0102u &&
               dspic33_read_word(cpu, 0x5000u) == 0xc000u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x007fu) == 0x007fu,
           "auxiliary execution routes IRQ through single vector");
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0102u,
                              OPCODE_RETFIE);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE,
           "RETFIE restores auxiliary return address");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_MOV_W1_W2);
    dspic33_load_program_word(cpu, 0x000006u, 0x000240u);
    dspic33_load_program_word(cpu, 0x007ffffau,
                              DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0200u);
    dspic33_set_working_register(cpu, 1u, 0x1001u);
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
               cpu->last_trap_return == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u &&
               cpu->pc == 0x000240u,
           "B1 auxiliary Address Error routes through general vector");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    cpu->stop_on_trap = false;
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_NOP);
    dspic33_load_program_word(cpu, 0x000006u, 0x000280u);
    dspic33_load_program_word(cpu, 0x000280u, OPCODE_NOP);
    dspic33_load_program_word(cpu, 0x007ffffau, 0x00060000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->last_trap_return == DSPIC33_AUXILIARY_PROGRAM_BASE &&
               cpu->pc == 0x000282u && cpu->last_interrupt == UINT16_MAX,
           "B1 invalid auxiliary interrupt vector uses general Address Error");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, 0x000006u, 0x0002a0u);
    dspic33_load_program_word(cpu, 0x007ffffau, 0x00060000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    dspic33_raise_dma_collision_trap(cpu);
    expect(state,
           cpu->last_trap == 1u &&
               cpu->last_trap_return == DSPIC33_AUXILIARY_PROGRAM_BASE &&
               cpu->pc == 0x0002a0u &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0028u) == 0x0028u,
           "B1 invalid auxiliary trap vector uses general Address Error");

    dspic33_reset(cpu, 0x007ffffeu);
    dspic33_load_program_word(cpu, 0x007ffffeu, OPCODE_MOV_W1_W2);
    dspic33_load_program_word(cpu, 0x000006u, 0x000260u);
    dspic33_load_program_word(cpu, 0x007ffffau,
                              DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0300u);
    dspic33_set_working_register(cpu, 1u, 0x1001u);
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
               cpu->last_trap_return == 0u && cpu->pc == 0x000260u &&
               dspic33_read_word(cpu, 0x5000u) == 0u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x007fu) == 0u,
           "B1 final auxiliary Address Error uses general vector");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize auxiliary program copy");
    if (initialized) {
        dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00010203u);
        expect(state, dspic33_copy(&copy, cpu), "copy auxiliary program state");
        dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00040506u);
        expect(state,
               dspic33_read_program_word(&copy, DSPIC33_AUXILIARY_PROGRAM_BASE) ==
                       0x00010203u &&
                   dspic33_read_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) ==
                       0x00040506u,
               "auxiliary program copies remain independent");
        dspic33_reset(&copy, 0u);
        expect(state,
               dspic33_read_program_word(&copy, DSPIC33_AUXILIARY_PROGRAM_BASE) ==
                   0x00010203u,
               "reset preserves auxiliary Flash");
        dspic33_destroy(&copy);
    }
}

static void auxiliary_nvm_cases(NvmConformance* state, Dspic33* cpu) {
    uint32_t index;
    uint64_t instructions;

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00f0f0f0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u, 0x000f0f0fu);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE),
           "auxiliary pair operation starts");
    expect(state, finish_operation(cpu), "auxiliary pair operation completes");
    expect(state,
           program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00103050u &&
               program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u) == 0x00050301u,
           "auxiliary pair programs both words");

    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        dspic33_load_program_word(
            cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u + index * 2u, 0x00ffffffu);
        cpu->write_latches[index] = 0x00330000u | index;
    }
    expect(state, start_operation(cpu, 2u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x17au),
           "auxiliary row operation starts");
    expect(state, finish_operation(cpu), "auxiliary row operation completes");
    expect(state,
           program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u) == 0x00330000u &&
               program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1feu) ==
                   0x0033007fu,
           "auxiliary row programs exact aligned range");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x7feu, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x800u,
                              0x00123456u);
    expect(state, start_operation(cpu, 3u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x456u),
           "auxiliary page erase starts");
    expect(state, finish_operation(cpu), "auxiliary page erase completes");
    expect(
        state,
        program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00ffffffu &&
            program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u) == 0x00ffffffu &&
            program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x7feu) == 0x00ffffffu &&
            program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x800u) == 0x00123456u,
        "auxiliary page erase preserves adjacent page");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x007ffffcu, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x007ffffeu, 0x00ffffffu);
    cpu->write_latches[0] = 0x00010203u;
    cpu->write_latches[1] = 0x00040506u;
    expect(state, start_operation(cpu, 1u, 0x007ffffeu), "auxiliary final pair starts");
    expect(state, finish_operation(cpu), "auxiliary final pair completes");
    expect(state,
           program_word(cpu, 0x007ffffcu) == 0x00010203u &&
               program_word(cpu, 0x007ffffeu) == 0x00040506u,
           "auxiliary final pair aligns within segment");

    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        dspic33_load_program_word(cpu, 0x007fff00u + index * 2u, 0x00ffffffu);
        cpu->write_latches[index] = 0x00660000u | index;
    }
    expect(state, start_operation(cpu, 2u, 0x007ffffeu), "auxiliary final row starts");
    expect(state, finish_operation(cpu), "auxiliary final row completes");
    expect(state,
           program_word(cpu, 0x007fff00u) == 0x00660000u &&
               program_word(cpu, 0x007ffffeu) == 0x0066007fu,
           "auxiliary final row remains within segment");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x007ff800u, 0u);
    dspic33_load_program_word(cpu, 0x007ffffeu, 0u);
    expect(state, start_operation(cpu, 3u, 0x007ffffeu), "auxiliary final page starts");
    expect(state, finish_operation(cpu), "auxiliary final page completes");
    expect(state,
           program_word(cpu, 0x007ff800u) == 0x00ffffffu &&
               program_word(cpu, 0x007ffffeu) == 0x00ffffffu,
           "auxiliary final page remains within segment");

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_LIMIT, true);
    expect(state, execute_start_sequence(cpu, false),
           "auxiliary out-of-range pair sequence executes");
    expect(state, !cpu->nvm.active, "auxiliary out-of-range pair rejected");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x2000u, 0x00010203u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00040506u);
    dspic33_load_configuration_word(cpu, 0xf80004u, 0xff30u);
    dspic33_load_configuration_word(cpu, 0xf80010u, 0xff30u);
    expect(state, start_operation(cpu, 0x0au, 0u), "auxiliary bulk erase starts");
    expect(state, finish_operation(cpu), "auxiliary bulk erase completes");
    expect(state,
           program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00ffffffu &&
               program_word(cpu, 0x2000u) == 0x00010203u,
           "auxiliary bulk erase preserves primary Flash");
    expect(state,
           dspic33_read_configuration_byte(cpu, 0xf80010u) == 0xcfu &&
               dspic33_read_configuration_byte(cpu, 0xf80011u) == 0xffu &&
               dspic33_read_configuration_byte(cpu, 0xf80004u) == 0x30u,
           "auxiliary bulk erase restores only FAS");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x2000u, 0x00010203u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00040506u);
    dspic33_load_configuration_word(cpu, 0xf80004u, 0xff30u);
    dspic33_load_configuration_word(cpu, 0xf80010u, 0xff30u);
    expect(
        state,
        start_operation_from(cpu, 0x0du, 0u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u),
        "primary bulk erase starts");
    expect(state, finish_operation(cpu), "primary bulk erase completes");
    expect(state,
           program_word(cpu, 0x2000u) == 0x00ffffffu &&
               program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00040506u,
           "primary bulk erase preserves auxiliary Flash");
    expect(state,
           dspic33_read_configuration_byte(cpu, 0xf80004u) == 0xcfu &&
               dspic33_read_configuration_byte(cpu, 0xf80005u) == 0xffu &&
               dspic33_read_configuration_byte(cpu, 0xf80010u) == 0x30u,
           "primary bulk erase restores only FGS");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state, start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1000u),
           "opposite-segment auxiliary program starts");
    dspic33_load_program_word(cpu, cpu->pc, OPCODE_NOP);
    instructions = cpu->instructions;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               cpu->instructions == instructions + 1u,
           "primary execution continues during auxiliary programming");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state, start_operation(cpu, 1u, 0x3000u),
           "opposite-segment primary program starts");
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1800u,
                              OPCODE_NOP);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1800u;
    instructions = cpu->instructions;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               cpu->instructions == instructions + 1u,
           "auxiliary execution continues during primary programming");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state, start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2000u),
           "same-segment auxiliary program starts");
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2800u,
                              OPCODE_NOP);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2800u;
    instructions = cpu->instructions;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               cpu->instructions == instructions,
           "auxiliary execution stalls during auxiliary programming");
}

static void stall_and_interrupt_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0032u, 0x000300u);
    dspic33_load_program_word(cpu, 0x0300u, 0x000000u);
    dspic33_write_word(cpu, 0x0820u, 0x8000u);
    dspic33_write_word(cpu, 0x0846u, 0x3000u);
    expect(state, start_operation(cpu, 1u, 0x3200u), "stall operation starts");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "completion stall cycle advances");
    expect(state, !cpu->nvm.active && interrupt_flag(cpu),
           "completion ends stall and raises NVMIF");
    expect(state,
           cpu->pc == NVM_SEQUENCE_BASE + 10u && cpu->instructions == 5u &&
               cpu->interrupt_count == 0u,
           "completion cycle defers interrupt service");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "post-completion instruction advances");
    expect(state, cpu->last_interrupt == NVM_IRQ && cpu->interrupt_count == 1u,
           "NVM interrupt serviced after stall");
    expect(state, cpu->pc == 0x0302u && cpu->instructions == 6u,
           "NVM vector instruction executes after service");
}

static void power_save_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint16_t operations[] = {1u, 2u, 3u, 0x0au, 0x0du};
    static const uint32_t targets[] = {
        DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u, 0x3a00u,
        DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2000u, 0u, 0u};
    static const uint32_t execution_addresses[] = {
        NVM_SEQUENCE_BASE + 10u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u,
        NVM_SEQUENCE_BASE + 10u, NVM_SEQUENCE_BASE + 10u,
        DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3200u};
    size_t index;

    for (index = 0u; index < sizeof(operations) / sizeof(operations[0]); index++) {
        uint32_t execution_address = execution_addresses[index];
        uint32_t opcode = (index & 1u) == 0u ? OPCODE_SLEEP : OPCODE_IDLE;
        Dspic33StopReason stop_reason =
            (index & 1u) == 0u ? DSPIC33_SLEEPING : DSPIC33_IDLING;
        Dspic33PowerState power_state =
            (index & 1u) == 0u ? DSPIC33_POWER_SLEEP : DSPIC33_POWER_IDLE;
        uint16_t rcon_bit = (index & 1u) == 0u ? 0x0008u : 0x0004u;
        uint64_t instructions;
        uint16_t rcon;

        dspic33_reset(cpu, 0u);
        cpu->write_latches[0] = 0x00112233u;
        cpu->write_latches[1] = 0x00445566u;
        expect(state, start_operation(cpu, operations[index], targets[index]),
               "opposite-segment power-save operation starts");
        dspic33_load_program_word(cpu, execution_address, opcode);
        dspic33_load_program_word(cpu, execution_address + 2u, opcode);
        cpu->pc = execution_address;
        cpu->watchdog.ticks = 17u;
        rcon = dspic33_read_word(cpu, 0x0740u);
        instructions = cpu->instructions;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   cpu->power_state == DSPIC33_POWER_ACTIVE &&
                   cpu->stop_reason == DSPIC33_RUNNING &&
                   dspic33_read_word(cpu, 0x0740u) == rcon &&
                   cpu->watchdog.ticks == 17u &&
                   cpu->instructions == instructions + 1u &&
                   cpu->pc == execution_address + 2u && !cpu->nvm.active,
               "active NVM ignores opposite-segment power-save instruction");
        expect(state,
               dspic33_step(cpu) == stop_reason && cpu->power_state == power_state &&
                   (dspic33_read_word(cpu, 0x0740u) & rcon_bit) != 0u &&
                   cpu->watchdog.ticks == 0u,
               "completed NVM permits the same power-save instruction");
    }

    for (index = 0u; index < 2u; index++) {
        uint16_t operation = 1u;
        uint32_t target =
            index == 0u ? 0x3c00u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2000u;
        uint32_t execution_address = index == 0u
                                         ? NVM_SEQUENCE_BASE + 10u
                                         : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3400u;
        uint32_t opcode = index == 0u ? OPCODE_SLEEP : OPCODE_IDLE;
        Dspic33StopReason stop_reason = index == 0u ? DSPIC33_SLEEPING : DSPIC33_IDLING;
        Dspic33PowerState power_state =
            index == 0u ? DSPIC33_POWER_SLEEP : DSPIC33_POWER_IDLE;
        uint16_t rcon_bit = index == 0u ? 0x0008u : 0x0004u;
        uint64_t instructions;
        uint16_t rcon;

        dspic33_reset(cpu, 0u);
        cpu->write_latches[0] = 0x00112233u;
        cpu->write_latches[1] = 0x00445566u;
        expect(state, start_operation(cpu, operation, target),
               "same-segment power-save operation starts");
        dspic33_load_program_word(cpu, execution_address, opcode);
        cpu->pc = execution_address;
        cpu->watchdog.ticks = 19u;
        rcon = dspic33_read_word(cpu, 0x0740u);
        instructions = cpu->instructions;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   cpu->pc == execution_address && cpu->instructions == instructions &&
                   dspic33_read_word(cpu, 0x0740u) == rcon &&
                   cpu->watchdog.ticks == 19u &&
                   cpu->power_state == DSPIC33_POWER_ACTIVE,
               "same-segment NVM stall retires no power-save instruction");
        expect(state,
               dspic33_step(cpu) == stop_reason && cpu->power_state == power_state &&
                   (dspic33_read_word(cpu, 0x0740u) & rcon_bit) != 0u &&
                   cpu->watchdog.ticks == 0u && cpu->pc == execution_address + 2u,
               "same-segment power-save executes after NVM completion");
    }

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    dspic33_load_program_word(cpu, 0x0032u, 0x000300u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_NOP);
    expect(state, start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3800u),
           "interrupt-before-power-save operation starts");
    dspic33_load_program_word(cpu, cpu->pc, OPCODE_SLEEP);
    dspic33_write_word(cpu, 0x0800u, 0x8000u);
    dspic33_write_word(cpu, 0x0820u, 0x8000u);
    dspic33_write_word(cpu, 0x0846u, 0x3000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_interrupt == NVM_IRQ &&
               cpu->interrupt_count == 1u && cpu->pc == 0x0302u &&
               cpu->power_state == DSPIC33_POWER_ACTIVE &&
               (dspic33_read_word(cpu, 0x0740u) & 0x000cu) == 0u,
           "eligible interrupt precedes ignored power-save instruction");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state, start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u),
           "priority-zero power-save operation starts");
    dspic33_load_program_word(cpu, cpu->pc, OPCODE_IDLE);
    dspic33_write_word(cpu, 0x0800u, 0x8000u);
    dspic33_write_word(cpu, 0x0820u, 0x8000u);
    dspic33_write_word(cpu, 0x0846u, 0u);
    {
        uint32_t pc = cpu->pc;
        uint64_t instructions = cpu->instructions;
        uint16_t rcon = dspic33_read_word(cpu, 0x0740u);
        cpu->watchdog.ticks = 23u;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_interrupt != NVM_IRQ &&
                   cpu->interrupt_count == 0u && interrupt_flag(cpu) &&
                   cpu->power_state == DSPIC33_POWER_ACTIVE && cpu->pc == pc + 2u &&
                   cpu->instructions == instructions + 1u &&
                   dspic33_read_word(cpu, 0x0740u) == rcon &&
                   cpu->watchdog.ticks == 23u,
               "priority-zero interrupt remains pending after ignored power-save");
    }
}

static void codeguard_configuration_cases(NvmConformance* state, Dspic33* cpu) {
    uint8_t general_index;
    uint8_t auxiliary_index;

    for (general_index = 0u; general_index < 16u; general_index++) {
        for (auxiliary_index = 0u; auxiliary_index < 16u; auxiliary_index++) {
            uint8_t general = codeguard_configuration_value(general_index);
            uint8_t auxiliary = codeguard_configuration_value(auxiliary_index);
            bool allowed = general == 0x03u && auxiliary == 0x03u;
            bool started;
            bool completed;

            dspic33_reset(cpu, 0u);
            load_codeguard_configuration(cpu, general, auxiliary);
            cpu->write_latches[0] = 0x31u;
            started = start_operation(cpu, 0u, CODEGUARD_AUXILIARY_CONFIGURATION);
            completed = finish_operation(cpu);
            expect(state,
                   started && completed &&
                       dspic33_read_configuration_byte(
                           cpu, CODEGUARD_AUXILIARY_CONFIGURATION) ==
                           (allowed ? 0x31u : auxiliary),
                   "CodeGuard FAS programming requires both unprotected segments");
        }
    }
}

static void codeguard_programming_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint16_t operations[] = {1u, 2u, 3u};
    size_t operation_index;
    uint8_t target_segment;
    uint8_t origin_segment;
    uint8_t configuration_index;

    for (operation_index = 0u;
         operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        uint16_t operation = operations[operation_index];
        for (target_segment = 0u; target_segment < 2u; target_segment++) {
            uint32_t target = target_segment == 0u
                                  ? 0x3000u
                                  : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1000u;
            for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
                uint32_t origin = origin_segment == 0u
                                      ? NVM_SEQUENCE_BASE
                                      : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
                for (configuration_index = 0u; configuration_index < 16u;
                     configuration_index++) {
                    uint8_t configuration =
                        codeguard_configuration_value(configuration_index);
                    bool write_protected = (configuration & 0x01u) == 0u;
                    bool high = codeguard_configuration_high(configuration);
                    bool allowed =
                        !write_protected && (!high || origin_segment == target_segment);
                    uint32_t initial = operation == 3u ? 0u : 0x00ffffffu;
                    uint32_t expected = operation == 3u ? allowed ? 0x00ffffffu : 0u
                                        : allowed       ? 0x00123456u
                                                        : 0x00ffffffu;
                    bool started;
                    bool completed;

                    dspic33_reset(cpu, 0u);
                    load_codeguard_configuration(
                        cpu, target_segment == 0u ? configuration : 0x03u,
                        target_segment == 0u ? 0x03u : configuration);
                    dspic33_load_program_word(cpu, target, initial);
                    cpu->write_latches[0] = 0x00123456u;
                    started = start_operation_from(cpu, operation, target, origin);
                    completed = finish_operation(cpu);
                    expect(state,
                           started && completed &&
                               cpu->nvm.auxiliary_origin == (origin_segment != 0u) &&
                               program_word(cpu, target) == expected,
                           "CodeGuard row and page programming matrix");
                }
            }
        }
    }

    for (operation_index = 0u;
         operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        uint16_t operation = operations[operation_index];
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin = origin_segment == 0u
                                  ? NVM_SEQUENCE_BASE
                                  : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            for (configuration_index = 0u; configuration_index < 16u;
                 configuration_index++) {
                uint8_t configuration =
                    codeguard_configuration_value(configuration_index);
                bool allowed = (configuration & 0x01u) != 0u &&
                               !codeguard_configuration_high(configuration);
                uint32_t target = operation == 3u ? 0u : 0x0100u;
                uint32_t initial = operation == 3u ? 0u : 0x00ffffffu;
                uint32_t expected = operation == 3u ? allowed ? 0x00ffffffu : 0u
                                    : allowed       ? 0x00123456u
                                                    : 0x00ffffffu;
                bool started;
                bool completed;

                dspic33_reset(cpu, 0u);
                load_codeguard_configuration(cpu, configuration, 0x03u);
                dspic33_load_program_word(cpu, target, initial);
                cpu->write_latches[0] = 0x00123456u;
                started = start_operation_from(cpu, operation, target, origin);
                completed = finish_operation(cpu);
                expect(state,
                       started && completed && program_word(cpu, target) == expected,
                       "CodeGuard IVT programming matrix");
            }
        }
    }
}

static void codeguard_segment_erase_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint16_t operations[] = {0x0au, 0x0du};
    size_t operation_index;
    uint8_t origin_segment;

    for (operation_index = 0u;
         operation_index < sizeof(operations) / sizeof(operations[0]);
         operation_index++) {
        uint16_t operation = operations[operation_index];
        bool auxiliary_target = operation == 0x0au;
        uint32_t target = auxiliary_target ? DSPIC33_AUXILIARY_PROGRAM_BASE : 0x2000u;
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin = origin_segment == 0u
                                  ? NVM_SEQUENCE_BASE
                                  : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            bool allowed = auxiliary_target != (origin_segment != 0u);
            bool started;
            bool completed;

            dspic33_reset(cpu, 0u);
            load_codeguard_configuration(cpu, 0x30u, 0x30u);
            dspic33_load_program_word(cpu, target, 0u);
            started = start_operation_from(cpu, operation, 0u, origin);
            completed = finish_operation(cpu);
            expect(state,
                   started && completed &&
                       program_word(cpu, target) == (allowed ? 0x00ffffffu : 0u) &&
                       dspic33_read_configuration_byte(
                           cpu, auxiliary_target ? CODEGUARD_AUXILIARY_CONFIGURATION
                                                 : CODEGUARD_GENERAL_CONFIGURATION) ==
                           (allowed ? 0xcfu : 0x30u),
                   "CodeGuard segment erase requires opposite-segment execution");
        }
    }
}

static void codeguard_origin_capture_cases(NvmConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    uint32_t target = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1000u;
    bool copy_initialized;

    dspic33_reset(cpu, 0u);
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, target, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    expect(state, start_operation_from(cpu, 1u, target, NVM_SEQUENCE_BASE),
           "CodeGuard captures general NVM origin");
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
    expect(state,
           finish_operation(cpu) && !cpu->nvm.auxiliary_origin &&
               program_word(cpu, target) == 0x00ffffffu,
           "CodeGuard preserves rejected cross-segment origin until completion");

    dspic33_reset(cpu, 0u);
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, target, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    expect(
        state,
        start_operation_from(cpu, 1u, target, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u),
        "CodeGuard captures auxiliary NVM origin");
    cpu->pc = NVM_SEQUENCE_BASE;
    expect(state,
           finish_operation(cpu) && cpu->nvm.auxiliary_origin &&
               program_word(cpu, target) == 0x00123456u,
           "CodeGuard preserves accepted self-segment origin until completion");

    dspic33_reset(cpu, 0u);
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, target, 0x00ffffffu);
    cpu->write_latches[0] = 0x00654321u;
    expect(
        state,
        start_operation_from(cpu, 1u, target, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u),
        "CodeGuard auxiliary-origin copy operation starts");
    copy_initialized = dspic33_initialize(&copy);
    expect(state,
           copy_initialized && dspic33_copy(&copy, cpu) && copy.nvm.active &&
               copy.nvm.auxiliary_origin,
           "CodeGuard copy retains active auxiliary origin");
    if (copy_initialized) {
        cpu->pc = NVM_SEQUENCE_BASE;
        copy.pc = NVM_SEQUENCE_BASE;
        expect(state,
               finish_operation(cpu) && finish_operation(&copy) &&
                   program_word(cpu, target) == 0x00654321u &&
                   program_word(&copy, target) == 0x00654321u &&
                   cpu->nvm.auxiliary_origin && copy.nvm.auxiliary_origin,
               "CodeGuard copied auxiliary origins complete independently");
        dspic33_destroy(&copy);
    }
}

static void codeguard_persistent_read_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint32_t targets[] = {
        PERSISTENT_PROGRAM_BASE, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE};
    static const uint8_t configurations[] = {0x03u, 0x31u};
    size_t target_index;
    size_t configuration_index;
    uint8_t origin_segment;

    for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]);
         target_index++) {
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin = origin_segment == 0u
                                  ? 0x1000u
                                  : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            for (configuration_index = 0u; configuration_index < sizeof(configurations);
                 configuration_index++) {
                uint8_t configuration = configurations[configuration_index];
                bool allowed = origin_segment == 0u || configuration == 0x03u;

                dspic33_reset(cpu, 0u);
                load_codeguard_configuration(cpu, configuration, 0x03u);
                dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00ab1357u);
                expect(
                    state,
                    execute_codeguard_table_read(cpu, origin, targets[target_index]) ==
                            (allowed ? 0x1357u : 0u) &&
                        program_word(cpu, PERSISTENT_PROGRAM_BASE) == 0x00ab1357u &&
                        program_word(cpu, PERSISTENT_PROGRAM_TAG +
                                              PERSISTENT_PROGRAM_BASE) == 0x00ab1357u,
                    "CodeGuard physical and tagged persistent table reads");
            }
        }
    }
}

static void codeguard_read_cases(NvmConformance* state, Dspic33* cpu) {
    uint8_t target_segment;
    uint8_t origin_segment;
    uint8_t configuration_index;

    for (target_segment = 0u; target_segment < 2u; target_segment++) {
        uint32_t target =
            target_segment == 0u ? 0x4000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u;
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin = origin_segment == 0u
                                  ? 0x1000u
                                  : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            for (configuration_index = 0u; configuration_index < 16u;
                 configuration_index++) {
                uint8_t configuration =
                    codeguard_configuration_value(configuration_index);
                bool allowed = origin_segment == target_segment ||
                               !codeguard_configuration_high(configuration);

                dspic33_reset(cpu, 0u);
                load_codeguard_configuration(
                    cpu, target_segment == 0u ? configuration : 0x03u,
                    target_segment == 0u ? 0x03u : configuration);
                dspic33_load_program_word(cpu, target, 0x00ab1357u);
                expect(state,
                       execute_codeguard_table_read(cpu, origin, target) ==
                               (allowed ? 0x1357u : 0u) &&
                           program_word(cpu, target) == 0x00ab1357u,
                       "CodeGuard cross-segment table-read matrix");
            }
        }
    }

    for (target_segment = 0u; target_segment < 2u; target_segment++) {
        uint32_t target =
            target_segment == 0u ? 0x4000u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u;
        for (origin_segment = 0u; origin_segment < 2u; origin_segment++) {
            uint32_t origin = origin_segment == 0u
                                  ? 0x1000u
                                  : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u;
            static const uint8_t configurations[] = {0x03u, 0x31u};
            size_t index;
            for (index = 0u; index < sizeof(configurations); index++) {
                uint8_t configuration = configurations[index];
                bool allowed = origin_segment == target_segment ||
                               !codeguard_configuration_high(configuration);

                dspic33_reset(cpu, 0u);
                load_codeguard_configuration(
                    cpu, target_segment == 0u ? configuration : 0x03u,
                    target_segment == 0u ? 0x03u : configuration);
                dspic33_load_program_word(cpu, target, 0x00ab1357u);
                expect(state,
                       execute_codeguard_psv_read(cpu, origin, target) ==
                               (allowed ? 0x1357u : 0u) &&
                           program_word(cpu, target) == 0x00ab1357u,
                       "CodeGuard cross-segment PSV matrix");
            }
        }
    }
}

static void codeguard_program_flow_configuration_cases(NvmConformance* state,
                                                       Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_LIMIT - 66u,
                                       DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u};
    const uint32_t origin = 0x4000u;
    uint8_t configuration_index;
    size_t target_index;
    for (configuration_index = 0u; configuration_index < 16u; configuration_index++) {
        uint8_t configuration = codeguard_configuration_value(configuration_index);
        bool high = codeguard_configuration_high(configuration);
        for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]);
             target_index++) {
            uint32_t target = targets[target_index];
            uint64_t reset_count;
            bool allowed = !high || target_index != 0u;
            dspic33_reset(cpu, origin);
            load_codeguard_configuration(cpu, 0x03u, configuration);
            load_long_program_flow(cpu, origin, target, false);
            reset_count = cpu->illegal_reset_count;
            expect(state,
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                       (allowed ? !cpu->illegal_reset &&
                                      cpu->illegal_reset_count == reset_count &&
                                      cpu->pc == target
                                : codeguard_security_reset(cpu, reset_count)),
                   "CodeGuard PFC configuration matrix");
        }
    }
}

static void codeguard_program_flow_instruction_cases(NvmConformance* state,
                                                     Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u,
                                       DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u};
    const uint32_t origin = 0x4200u;
    uint64_t cycles;
    uint64_t device_cycles;
    uint64_t reset_count;
    uint64_t trap_count;
    size_t target_index;
    for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]);
         target_index++) {
        uint32_t target = targets[target_index];
        bool allowed = target_index != 0u;

        dspic33_reset(cpu, origin);
        load_codeguard_configuration(cpu, 0x03u, 0x31u);
        load_long_program_flow(cpu, origin, target, true);
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->call_depth == 1u
                            : codeguard_security_reset(cpu, reset_count)),
               "CodeGuard long CALL PFC");

        dspic33_reset(cpu, origin);
        load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, origin, OPCODE_COMPUTED_GOTO_W0);
        dspic33_set_working_register(cpu, 0u, (uint16_t)target);
        dspic33_set_working_register(cpu, 1u, (uint16_t)(target >> 16u));
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target
                            : codeguard_security_reset(cpu, reset_count)),
               "CodeGuard computed GOTO PFC");

        dspic33_reset(cpu, origin);
        load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, origin, OPCODE_COMPUTED_CALL_W0);
        dspic33_set_working_register(cpu, 0u, (uint16_t)target);
        dspic33_set_working_register(cpu, 1u, (uint16_t)(target >> 16u));
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->call_depth == 1u
                            : codeguard_security_reset(cpu, reset_count)),
               "CodeGuard computed CALL PFC");

        dspic33_reset(cpu, origin);
        load_codeguard_configuration(cpu, 0x03u, 0x31u);
        load_program_return(cpu, origin, target, OPCODE_RETURN);
        cpu->call_depth = 1u;
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->call_depth == 0u
                            : codeguard_security_reset(cpu, reset_count)),
               "CodeGuard RETURN PFC");

        dspic33_reset(cpu, origin);
        load_codeguard_configuration(cpu, 0x03u, 0x31u);
        load_program_return(cpu, origin, target, OPCODE_RETFIE);
        cpu->interrupt_depth = 1u;
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target && cpu->interrupt_depth == 0u
                            : codeguard_security_reset(cpu, reset_count)),
               "CodeGuard RETFIE PFC");
    }

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u);
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    load_long_program_flow(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u,
                           DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u &&
               !cpu->illegal_reset,
           "CodeGuard same Auxiliary Segment PFC");

    dspic33_reset(cpu, origin);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);
    load_long_program_flow(cpu, origin, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u &&
               !cpu->illegal_reset,
           "CodeGuard unprotected Auxiliary Segment PFC");

    dspic33_reset(cpu, origin);
    load_codeguard_configuration(cpu, 0x31u, 0x31u);
    load_long_program_flow(cpu, origin, 0x4400u, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x4400u &&
               !cpu->illegal_reset,
           "CodeGuard General Segment PFC");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    load_long_program_flow(cpu, origin, 0x060000u, false);
    dspic33_load_program_word(cpu, 0x0006u, 0x004400u);
    dspic33_load_program_word(cpu, 0x4400u, OPCODE_NOP);
    reset_count = cpu->illegal_reset_count;
    trap_count = cpu->trap_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->trap_count == trap_count + 1u &&
               cpu->illegal_reset_count == reset_count && cpu->pc == 0x4400u,
           "unimplemented PFC takes Address Error before CodeGuard");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    load_long_program_flow(cpu, origin, 0x060000u, false);
    dspic33_load_program_word(cpu, 0x0006u, DSPIC33_AUXILIARY_PROGRAM_LIMIT - 66u);
    cpu->corcon |= 0x0004u;
    cycles = cpu->cycles;
    device_cycles = cpu->device_cycles;
    reset_count = cpu->illegal_reset_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               codeguard_security_reset(cpu, reset_count) && cpu->corcon == 0x0020u &&
               cpu->cycles == cycles && cpu->device_cycles == device_cycles,
           "restricted Address Error VFC retains reset CORCON");
}

static void codeguard_vector_flow_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u,
                                       DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u};
    const uint32_t origin = 0x4600u;
    size_t target_index;
    for (target_index = 0u; target_index < sizeof(targets) / sizeof(targets[0]);
         target_index++) {
        uint32_t target = targets[target_index];
        bool allowed = target_index != 0u;
        uint64_t reset_count;

        dspic33_reset(cpu, origin);
        cpu->stop_on_trap = false;
        load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, 0x0014u, target);
        dspic33_load_program_word(cpu, target, OPCODE_NOP);
        dspic33_write_word(cpu, 0x0820u, 0x0001u);
        dspic33_write_word(cpu, 0x0840u, 0x0001u);
        dspic33_raise_interrupt(cpu, 0u);
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target + 2u && cpu->last_interrupt == 0u
                            : codeguard_security_reset(cpu, reset_count)),
               "CodeGuard interrupt VFC");

        dspic33_reset(cpu, origin);
        load_codeguard_configuration(cpu, 0x03u, 0x31u);
        dspic33_load_program_word(cpu, 0x0004u, target);
        dspic33_load_program_word(cpu, target, OPCODE_NOP);
        dspic33_raise_oscillator_fail_trap(cpu);
        reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (allowed ? cpu->pc == target + 2u && cpu->last_trap == 0u
                            : codeguard_security_reset(cpu, reset_count)),
               "CodeGuard trap VFC");
    }

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x0014u, 0x060000u);
    dspic33_load_program_word(cpu, 0x0006u, 0x004400u);
    dspic33_load_program_word(cpu, 0x4400u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_raise_interrupt(cpu, 0u);
    uint64_t trap_count = cpu->trap_count;
    uint64_t reset_count = cpu->illegal_reset_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->trap_count == trap_count + 1u && cpu->pc == 0x4402u &&
               cpu->illegal_reset_count == reset_count,
           "unimplemented interrupt VFC dispatches Address Error");

    dspic33_reset(cpu, origin);
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x000eu, 0x060000u);
    dspic33_load_program_word(cpu, 0x0006u, 0x004400u);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    trap_count = cpu->trap_count;
    reset_count = cpu->illegal_reset_count;
    dspic33_raise_dma_collision_trap(cpu);
    expect(state,
           cpu->last_trap == 1u && cpu->trap_count == trap_count + 1u &&
               cpu->pc == 0x4400u && cpu->illegal_reset_count == reset_count &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0028u) == 0x0028u,
           "unimplemented synchronous trap VFC dispatches Address Error");

    dspic33_reset(cpu, origin);
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x0014u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_raise_interrupt(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    uint64_t instructions = cpu->instructions;
    reset_count = cpu->illegal_reset_count;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               codeguard_security_reset(cpu, reset_count) &&
               cpu->instructions == instructions && cpu->w[2] == 0u,
           "sleeping restricted interrupt VFC stops at security reset");
}

static void vector_segment_execution_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint32_t vector_addresses[] = {0x000002u, 0x000004u, 0x000100u,
                                                0x0001feu};
    static const uint32_t vector_opcodes[] = {OPCODE_NOP, OPCODE_SLEEP};
    const uint32_t handler = 0x000400u;
    const uint32_t origin = 0x004600u;
    size_t index;

    for (index = 0u; index < sizeof(vector_addresses) / sizeof(vector_addresses[0]);
         index++) {
        uint32_t address = vector_addresses[index];
        uint64_t cycles;
        uint64_t instructions;
        uint64_t trap_count;

        dspic33_reset(cpu, address);
        cpu->stop_on_trap = true;
        dspic33_load_program_word(cpu, address, OPCODE_MOV_LITERAL_0X1234_W2);
        dspic33_load_program_word(cpu, 0x000006u, handler);
        dspic33_load_program_word(cpu, handler, OPCODE_NOP);
        dspic33_set_working_register(cpu, 15u, 0x1000u);
        dspic33_set_working_register(cpu, 2u, 0xa5a5u);
        cycles = cpu->cycles;
        instructions = cpu->instructions;
        trap_count = cpu->trap_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
                   cpu->trap_count == trap_count + 1u && cpu->pc == handler &&
                   cpu->last_trap_return == address + 2u && cpu->w[2] == 0xa5a5u &&
                   cpu->w[15] == 0x1004u &&
                   dspic33_read_word(cpu, 0x1000u) == address + 2u &&
                   cpu->instructions == instructions && cpu->cycles == cycles + 1u,
               "vector-segment instruction fetch raises Address Error");
    }

    dspic33_reset(cpu, 0u);
    load_long_program_flow(cpu, 0u, 0x000600u, false);
    dspic33_load_program_word(cpu, 0x000600u, OPCODE_MOV_LITERAL_0X1234_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000600u &&
               cpu->instructions == 1u && cpu->cycles == 4u,
           "primary Reset GOTO reads its address extension");

    dspic33_reset(cpu, 0x000200u);
    dspic33_load_program_word(cpu, 0x000200u, OPCODE_MOV_LITERAL_0X1234_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000202u &&
               cpu->w[2] == 0x1234u,
           "General Segment code after IVT remains executable");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    load_long_program_flow(cpu, origin, 0x000100u, false);
    dspic33_load_program_word(cpu, 0x000100u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x000006u, handler);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_set_working_register(cpu, 2u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000100u &&
               cpu->last_trap == UINT16_MAX && !cpu->illegal_reset,
           "PFC may target the vector segment");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->pc == handler && cpu->last_trap_return == 0x000102u &&
               cpu->w[2] == 0xa5a5u && cpu->instructions == 1u && cpu->cycles == 5u,
           "execution after vector-segment PFC raises Address Error");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_load_program_word(cpu, 0x000014u, 0x000100u);
    dspic33_load_program_word(cpu, 0x000100u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x000006u, handler);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_interrupt == 0u &&
               cpu->interrupt_count == 1u && cpu->last_trap == 1u &&
               cpu->trap_count == 1u && cpu->pc == handler &&
               cpu->last_trap_return == 0x000102u && cpu->w[15] == 0x1008u &&
               cpu->instructions == 0u && cpu->cycles == 10u,
           "interrupt VFC into vector segment traps before execution");

    dspic33_reset(cpu, origin);
    cpu->stop_on_trap = false;
    dspic33_load_program_word(cpu, 0x000004u, 0x000100u);
    dspic33_load_program_word(cpu, 0x000100u, OPCODE_MOV_LITERAL_0X1234_W2);
    dspic33_load_program_word(cpu, 0x000006u, handler);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_raise_oscillator_fail_trap(cpu);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->trap_count == 2u && cpu->pc == handler &&
               cpu->last_trap_return == 0x000102u && cpu->w[15] == 0x1008u &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x000au) == 0x000au,
           "trap VFC into vector segment traps before execution");

    for (index = 0u; index < sizeof(vector_opcodes) / sizeof(vector_opcodes[0]);
         index++) {
        dspic33_reset(cpu, 0x000100u);
        cpu->stop_on_trap = false;
        dspic33_load_program_word(cpu, 0x000100u, vector_opcodes[index]);
        dspic33_load_program_word(cpu, 0x000014u, 0x000300u);
        dspic33_load_program_word(cpu, 0x000300u, OPCODE_NOP);
        dspic33_set_working_register(cpu, 15u, 0x1000u);
        dspic33_write_word(cpu, 0x0820u, 0x0001u);
        dspic33_write_word(cpu, 0x0840u, 0x0001u);
        dspic33_raise_interrupt(cpu, 0u);
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_count == 1u &&
                   cpu->last_interrupt == 0u && cpu->pc == 0x000302u &&
                   cpu->trap_count == 0u && cpu->last_trap == UINT16_MAX &&
                   cpu->instructions == 1u && cpu->w[15] == 0x1004u,
               "vector contents do not alter pending interrupt predispatch");
    }

    dspic33_reset(cpu, origin);
    dspic33_load_program_word(cpu, 0x000100u, 0x00ab1357u);
    expect(state,
           execute_codeguard_table_read(cpu, origin, 0x000100u) == 0x1357u &&
               program_word(cpu, 0x000100u) == 0x00ab1357u,
           "vector-segment data reads remain permitted");

    dspic33_reset(cpu, 0x000100u);
    cpu->stop_on_trap = false;
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x000006u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u);
    uint64_t reset_count = cpu->illegal_reset_count;
    uint64_t cycles = cpu->cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               codeguard_security_reset(cpu, reset_count) && cpu->cycles == cycles,
           "restricted Address Error vector retains security-reset precedence");
}

static void codeguard_cases(NvmConformance* state, Dspic33* cpu) {
    codeguard_configuration_cases(state, cpu);
    codeguard_programming_cases(state, cpu);
    codeguard_segment_erase_cases(state, cpu);
    codeguard_origin_capture_cases(state, cpu);
    codeguard_read_cases(state, cpu);
    codeguard_persistent_read_cases(state, cpu);
    codeguard_program_flow_configuration_cases(state, cpu);
    codeguard_program_flow_instruction_cases(state, cpu);
    codeguard_vector_flow_cases(state, cpu);
    vector_segment_execution_cases(state, cpu);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00ffffffu);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);
}

static void async_suppression_cases(NvmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, start_operation(cpu, 1u, 0x3a00u),
           "suppressed-events operation starts");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "suppressed-events completion cycle");
    expect(state, !cpu->nvm.active && cpu->events.count == 0u,
           "suppressed-events completion removes NVM event");
    expect(state, interrupt_flag(cpu), "suppressed-events completion raises NVMIF");

    dspic33_reset(cpu, 0u);
    expect(state, start_operation(cpu, 1u, 0x3c00u), "disable-during operation starts");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "queue unrelated event during NVM");
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "disabled-during completion cycle");
    expect(state, !cpu->nvm.active && cpu->events.count == 1u,
           "disabled-during keeps unrelated event only");
    dspic33_write_word(cpu, 0x0800u, 0u);
    dspic33_set_async_events(cpu, true);
    expect(state, dspic33_device_advance(cpu, 5u), "advance retained unrelated event");
    expect(state, (dspic33_read_word(cpu, 0x0800u) & 0x0002u) != 0u,
           "retained unrelated event dispatched");
    expect(state, (dspic33_read_word(cpu, 0x0800u) & 0x8000u) == 0u,
           "stale NVM event does not redispatch");

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, start_operation(cpu, 1u, 0x3e00u), "reenabled operation starts");
    dspic33_set_async_events(cpu, true);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "reenabled operation completion cycle");
    expect(state, !cpu->nvm.active && cpu->events.count == 0u,
           "reenabled operation completes without stale event");
}

static void reset_copy_and_failure_cases(NvmConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x3400u, 0x00ffffffu);
    cpu->write_latches[0] = 0x00112233u;
    expect(state, start_operation(cpu, 1u, 0x3400u), "reset-abort operation starts");
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->nvm.active && cpu->events.count == 0u,
           "POR aborts operation and event");
    expect(state, dspic33_read_word(cpu, NVM_CONTROL) == 0u, "POR clears WR and WRERR");
    expect(state, program_word(cpu, 0x3400u) == 0x00ffffffu,
           "POR-aborted operation does not program");
    expect(state, cpu->write_latches[0] == 0x00ffffffu, "POR resets write latches");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize NVM copy");
    if (initialized) {
        dspic33_reset(cpu, 0u);
        dspic33_load_program_word(cpu, 0x3600u, 0x00ffffffu);
        cpu->write_latches[0] = 0x00010203u;
        cpu->write_latches[1] = 0x00040506u;
        expect(state, start_operation(cpu, 1u, 0x3600u), "copy operation starts");
        expect(state, dspic33_copy(&copy, cpu), "copy active NVM state");
        expect(state, copy.nvm.active && copy.events.count == 1u,
               "copy retains active event");
        expect(state,
               copy.nvm.address == cpu->nvm.address &&
                   copy.nvm.latches[1] == cpu->nvm.latches[1],
               "copy retains captured operation");
        expect(state, finish_operation(&copy), "copied operation completes");
        expect(state,
               program_word(&copy, 0x3600u) == 0x00010203u &&
                   program_word(&copy, 0x3602u) == 0x00040506u,
               "copied event programs captured pair");
        expect(state, cpu->nvm.active && program_word(cpu, 0x3600u) == 0x00ffffffu,
               "source operation remains independent");
        dspic33_destroy(&copy);
    }

    dspic33_reset(cpu, 0u);
    configure_operation(cpu, 1u, 0x3800u, true);
    cpu->cycles = 1u;
    cpu->device_cycles = UINT64_MAX;
    cpu->instructions = 1u;
    cpu->nvm.key_stage = 2u;
    cpu->nvm.key_instruction = 0u;
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE | 1u);
    expect(state, !cpu->nvm.active, "event overflow cancels operation");
    expect(state, cpu->events.count == 0u, "event overflow queues no completion");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE) == 0u,
           "event overflow clears WR");
    expect(state, (dspic33_read_word(cpu, NVM_CONTROL) & NVM_WRITE_ERROR) != 0u,
           "event overflow sets WRERR");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, NVM_CONTROL, NVM_WRITE_ENABLE | NVM_WRITE_ERROR | 2u);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "software reset executes");
    expect(state, cpu->software_reset_count == 1u, "software reset counted");
    expect(state,
           dspic33_read_word(cpu, NVM_CONTROL) ==
               (NVM_WRITE_ENABLE | NVM_WRITE_ERROR | 2u),
           "warm reset preserves NVMCON POR-only fields");
}

static void deferred_reset_cases(NvmConformance* state, Dspic33* cpu) {
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3600u,
                                       0x3e00u};
    static const uint32_t origins[] = {NVM_SEQUENCE_BASE + 10u,
                                       DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3800u};
    Dspic33 copy;
    bool copy_initialized;
    size_t index;

    for (index = 0u; index < sizeof(targets) / sizeof(targets[0]); index++) {
        uint64_t reset_count;
        dspic33_reset(cpu, 0u);
        load_codeguard_configuration(cpu, 0x03u, 0x03u);
        cpu->write_latches[0] = 0x00112233u;
        cpu->write_latches[1] = 0x00445566u;
        expect(state, start_operation_from(cpu, 1u, targets[index], origins[index]),
               "deferred software-reset operation starts");
        dspic33_load_program_word(cpu, cpu->pc, OPCODE_RESET);
        reset_count = cpu->software_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending &&
                   cpu->software_reset_count == reset_count + 1u && cpu->pc == 0u &&
                   program_word(cpu, targets[index]) == 0x00112233u &&
                   program_word(cpu, targets[index] + 2u) == 0x00445566u &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x0040u) != 0u &&
                   (dspic33_read_word(cpu, NVM_CONTROL) &
                    (NVM_WRITE | NVM_WRITE_ERROR)) == 0u &&
                   !interrupt_flag(cpu),
               "software reset waits for opposite-segment programming");
    }

    dspic33_reset(cpu, 0u);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u,
                              0x00123456u);
    expect(state, start_operation(cpu, 0x0au, 0u),
           "deferred security-reset erase starts");
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, cpu->pc, OPCODE_COMPUTED_GOTO_W0);
    dspic33_set_working_register(cpu, 0u,
                                 (uint16_t)(DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u));
    dspic33_set_working_register(
        cpu, 1u, (uint16_t)((DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u) >> 16u));
    {
        uint64_t reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending &&
                   cpu->illegal_reset_count == reset_count + 1u && cpu->illegal_reset &&
                   cpu->pc == 0u &&
                   program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u) ==
                       0x00ffffffu &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
                   (dspic33_read_word(cpu, NVM_CONTROL) &
                    (NVM_WRITE | NVM_WRITE_ERROR)) == 0u &&
                   !interrupt_flag(cpu),
               "security reset waits for Auxiliary Segment erase");
    }

    dspic33_reset(cpu, 0u);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3e00u),
           "deferred illegal-source operation starts");
    dspic33_load_program_word(cpu, cpu->pc,
                              OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT);
    dspic33_set_working_register(cpu, 2u, 1u);
    cpu->w[4] = 0x1000u;
    dspic33_set_working_register(cpu, 5u, 0x5000u);
    dspic33_write_word(cpu, 0x1000u, 2u);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    {
        uint64_t reset_count = cpu->illegal_reset_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending && cpu->illegal_reset &&
                   cpu->illegal_reset_count == reset_count + 1u && cpu->pc == 0u &&
                   dspic33_read_word(cpu, 0x5000u) == 0xaaaau &&
                   program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3e00u) ==
                       0x00123456u &&
                   program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3e02u) ==
                       0x00654321u &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
                   !interrupt_flag(cpu),
               "deferred illegal source inhibits RAM write before reset");
    }

    dspic33_reset(cpu, 0u);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u,
                              0x00123456u);
    expect(state, start_operation(cpu, 0x0au, 0u),
           "deferred restricted-vector erase starts");
    load_codeguard_configuration(cpu, 0x03u, 0x31u);
    dspic33_load_program_word(cpu, 0x0014u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0xa55au);
    dspic33_write_word(cpu, 0x5002u, 0x5aa5u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_raise_interrupt(cpu, 0u);
    {
        uint64_t reset_count = cpu->illegal_reset_count;
        uint64_t interrupt_count = cpu->interrupt_count;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   !cpu->nvm.reset_pending && cpu->illegal_reset &&
                   cpu->illegal_reset_count == reset_count + 1u && cpu->pc == 0u &&
                   cpu->interrupt_count == interrupt_count &&
                   dspic33_read_word(cpu, 0x5000u) == 0xa55au &&
                   dspic33_read_word(cpu, 0x5002u) == 0x5aa5u &&
                   program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u) ==
                       0x00ffffffu &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
                   !interrupt_flag(cpu),
               "restricted vector reset inhibits frame before NVM completion");
    }

    copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized, "initialize deferred-reset copy");
    if (copy_initialized) {
        dspic33_reset(cpu, 0u);
        load_codeguard_configuration(cpu, 0x03u, 0x03u);
        cpu->write_latches[0] = 0x00010203u;
        cpu->write_latches[1] = 0x00040506u;
        expect(state,
               start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u),
               "copied deferred-reset operation starts");
        dspic33_configuration_mismatch_reset(cpu);
        expect(state, cpu->nvm.active && cpu->nvm.reset_pending,
               "configuration-mismatch reset remains pending");
        expect(state, dspic33_copy(&copy, cpu), "copy deferred-reset state");
        expect(state, finish_operation(cpu) && finish_operation(&copy),
               "copied deferred resets complete independently");
        expect(state,
               !cpu->nvm.active && !copy.nvm.active && !cpu->nvm.reset_pending &&
                   !copy.nvm.reset_pending && cpu->pc == 0u && copy.pc == 0u &&
                   (dspic33_read_word(cpu, 0x0740u) & 0x0200u) != 0u &&
                   (dspic33_read_word(&copy, 0x0740u) & 0x0200u) != 0u &&
                   program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u) ==
                       0x00010203u &&
                   program_word(&copy, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u) ==
                       0x00010203u &&
                   !interrupt_flag(cpu) && !interrupt_flag(&copy),
               "copied configuration-mismatch resets follow NVM completion");
        dspic33_destroy(&copy);
    }

    dspic33_reset(cpu, 0u);
    load_codeguard_configuration(cpu, 0x03u, 0x03u);
    cpu->write_latches[0] = 0x00654321u;
    expect(state, start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3c00u),
           "POR pending-reset operation starts");
    dspic33_configuration_mismatch_reset(cpu);
    expect(state, cpu->nvm.reset_pending, "POR operation has pending warm reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           !cpu->nvm.active && !cpu->nvm.reset_pending && cpu->events.count == 0u &&
               program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3c00u) ==
                   0x00ffffffu,
           "POR aborts operation and pending warm reset");
}

static void doze_stall_cases(NvmConformance* state, Dspic33* cpu) {
    uint64_t cpu_cycles;
    uint64_t device_cycles;

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_load_program_word(cpu, 0x3a00u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x3a02u, 0x00ffffffu);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, start_operation(cpu, 1u, 0x3a00u),
           "DOZE NVM operation remains active after WR instruction");
    expect(state,
           cpu->nvm.completion_cycle == cpu->cycles + 1u && cpu->events.count == 1u &&
               cpu->events.items[0].cycle - cpu->device_cycles == 8u &&
               program_word(cpu, 0x3a00u) == 0x00ffffffu && !interrupt_flag(cpu),
           "DOZE NVM event cannot complete before the CPU deadline");
    cpu_cycles = cpu->cycles;
    device_cycles = cpu->device_cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               cpu->cycles - cpu_cycles == 1u &&
               cpu->device_cycles - device_cycles == 8u &&
               program_word(cpu, 0x3a00u) == 0x00123456u &&
               program_word(cpu, 0x3a02u) == 0x00654321u && interrupt_flag(cpu),
           "DOZE NVM stall advances both domains and completes at the CPU deadline");
}

static void persistent_program_alias_cases(NvmConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    uint32_t index;
    bool copy_initialized;

    dspic33_reset(cpu, 0u);
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_BASE) == 0x00ffffffu &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE) ==
                   0x00ffffffu,
           "persistent physical and tagged views reset erased");
    expect(state,
           dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00112233u) &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE) ==
                   0x00112233u &&
               dspic33_load_program_word(
                   cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE + 2u,
                   0x00445566u) &&
               program_word(cpu, PERSISTENT_PROGRAM_BASE + 2u) == 0x00445566u,
           "persistent loader keeps physical and tagged views coherent");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE - 2u, 0x00010203u);
    dspic33_load_program_word(
        cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE - 2u, 0x00654321u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0x00ffffffu);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 2u, 0x00ffffffu);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 4u, 0x00040506u);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state,
           start_operation(cpu, 1u, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE),
           "persistent pair operation starts");
    expect(state, cpu->nvm.address == PERSISTENT_PROGRAM_BASE,
           "persistent pair request normalizes through NVMADRU");
    expect(state, finish_operation(cpu), "persistent pair operation completes");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE) ==
                   0x00123456u &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE +
                                     2u) == 0x00654321u,
           "persistent pair updates tagged firmware view");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_BASE - 2u) == 0x00010203u &&
               program_word(cpu, PERSISTENT_PROGRAM_BASE + 4u) == 0x00040506u,
           "persistent pair preserves adjacent physical words");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE - 2u) ==
               0x00654321u,
           "persistent lower boundary keeps physical and tagged storage separate");

    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        cpu->write_latches[index] = 0x00330000u | index;
    }
    dspic33_load_program_word(cpu, 0x22feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2300u, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x23feu, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x2400u, 0x00040506u);
    expect(state, start_operation(cpu, 2u, PERSISTENT_PROGRAM_TAG + 0x23feu),
           "persistent row operation starts");
    expect(state, cpu->nvm.address == 0x23feu,
           "persistent row request normalizes through NVMADRU");
    expect(state, finish_operation(cpu), "persistent row operation completes");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x2300u) == 0x00330000u &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x23feu) == 0x0033007fu,
           "persistent row updates exact tagged range");
    expect(state,
           program_word(cpu, 0x22feu) == 0x00010203u &&
               program_word(cpu, 0x2400u) == 0x00040506u,
           "persistent row preserves adjacent words");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x27feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2800u, 0u);
    dspic33_load_program_word(cpu, 0x2ffeu, 0u);
    dspic33_load_program_word(cpu, 0x3000u, 0x00040506u);
    expect(state, start_operation(cpu, 3u, PERSISTENT_PROGRAM_TAG + 0x2ffeu),
           "persistent page operation starts");
    expect(state, cpu->nvm.address == 0x2ffeu,
           "persistent page request normalizes through NVMADRU");
    expect(state, finish_operation(cpu), "persistent page operation completes");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x2800u) == 0x00ffffffu &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x2ffeu) == 0x00ffffffu,
           "persistent page erase updates tagged view");
    expect(state,
           program_word(cpu, 0x27feu) == 0x00010203u &&
               program_word(cpu, 0x3000u) == 0x00040506u,
           "persistent page erase preserves adjacent words");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x4800u, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_LIMIT - 2u, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_LIMIT, 0x00010203u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_LIMIT,
                              0x00040506u);
    expect(state,
           start_operation(cpu, 3u,
                           PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_LIMIT - 2u),
           "persistent upper page operation starts");
    expect(state, cpu->nvm.address == PERSISTENT_PROGRAM_LIMIT - 2u,
           "persistent upper page normalizes through NVMADRU");
    expect(state, finish_operation(cpu), "persistent upper page operation completes");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_TAG + 0x4800u) == 0x00ffffffu &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_LIMIT -
                                     2u) == 0x00ffffffu,
           "persistent upper page erases through physical limit");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_LIMIT) == 0x00010203u &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_LIMIT) ==
                   0x00040506u,
           "program origin and tagged boundary remain independently routed");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00010203u);
    dspic33_load_configuration_word(cpu, DSPIC33_CONFIGURATION_BASE + 4u, 0xff30u);
    expect(
        state,
        start_operation_from(cpu, 0x0du, 0u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x200u),
        "general bulk erase with persistent data starts");
    expect(state, finish_operation(cpu),
           "general bulk erase with persistent data completes");
    expect(state,
           program_word(cpu, PERSISTENT_PROGRAM_BASE) == 0x00ffffffu &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE) ==
                   0x00ffffffu &&
               dspic33_read_configuration_byte(cpu, DSPIC33_CONFIGURATION_BASE + 4u) ==
                   0xcfu,
           "general bulk erase clears persistent data and FGS");
    expect(state, program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00010203u,
           "general bulk erase preserves auxiliary program");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x200u, 0x00112233u);
    copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized, "initialize persistent alias copy");
    if (copy_initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy persistent alias state");
        dspic33_load_program_word(
            cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE + 0x200u,
            0x00445566u);
        expect(state,
               program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x200u) == 0x00445566u &&
                   program_word(&copy, PERSISTENT_PROGRAM_BASE + 0x200u) == 0x00112233u,
               "copied persistent aliases diverge independently");
        dspic33_destroy(&copy);
    }

    dspic33_load_program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x400u, 0x00123456u);
    dspic33_load_program_word(cpu, 0u, OPCODE_RESET);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               program_word(cpu, PERSISTENT_PROGRAM_TAG + PERSISTENT_PROGRAM_BASE +
                                     0x400u) == 0x00123456u,
           "warm reset preserves persistent program");
    dspic33_reset(cpu, 0u);
    expect(state, program_word(cpu, PERSISTENT_PROGRAM_BASE + 0x400u) == 0x00123456u,
           "cold processor reset preserves persistent program");
}

int main(void) {
    Dspic33 cpu;
    NvmConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize NVM processor");
    if (initialized) {
        configuration_table_view_cases(&state, &cpu);
        reset_and_access_cases(&state, &cpu);
        key_sequence_cases(&state, &cpu);
        key_byte_access_cases(&state, &cpu);
        invalid_operation_cases(&state, &cpu);
        invalid_target_cases(&state, &cpu);
        program_range_cases(&state, &cpu);
        configuration_operation_cases(&state, &cpu);
        configuration_programming_matrix_cases(&state, &cpu);
        pair_and_capture_cases(&state, &cpu);
        row_operation_cases(&state, &cpu);
        erase_operation_cases(&state, &cpu);
        auxiliary_loader_cases(&state, &cpu);
        auxiliary_access_and_execution_cases(&state, &cpu);
        auxiliary_nvm_cases(&state, &cpu);
        stall_and_interrupt_cases(&state, &cpu);
        power_save_cases(&state, &cpu);
        codeguard_cases(&state, &cpu);
        persistent_program_alias_cases(&state, &cpu);
        doze_stall_cases(&state, &cpu);
        async_suppression_cases(&state, &cpu);
        reset_copy_and_failure_cases(&state, &cpu);
        deferred_reset_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    report_sfr_side_effect_coverage(
        "nvm", nvm_sfr_side_effect_coverage,
        SFR_SIDE_EFFECT_COVERAGE_COUNT(nvm_sfr_side_effect_coverage),
        state.failed == 0u);
    printf("[nvm-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
