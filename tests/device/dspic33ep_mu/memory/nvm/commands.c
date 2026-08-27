#include "device/dspic33ep_mu/memory/nvm/internal.h"

void dspic33_nvm_test_erase_operation_cases(TestState* state, Dspic33* cpu) {
    Dspic33* partial_page_cpu;

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x27feu, 0x00010203u);
    dspic33_load_program_word(cpu, 0x2800u, 0u);
    dspic33_load_program_word(cpu, 0x2c00u, 0u);
    dspic33_load_program_word(cpu, 0x2ffeu, 0u);
    dspic33_load_program_word(cpu, 0x3000u, 0x00040506u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 3u, 0x2abcu), "page erase starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "page erase completes");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2800u) == 0x00ffffffu,
           "page first word erased");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2c00u) == 0x00ffffffu,
           "page middle word erased");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x2ffeu) == 0x00ffffffu,
           "page last word erased");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x27feu) == 0x00010203u,
           "page preceding word unchanged");
    expect(state, dspic33_nvm_test_program_word(cpu, 0x3000u) == 0x00040506u,
           "page following word unchanged");

    partial_page_cpu = dspic33_create_for_device(DSPIC33EP_MU_DEVICE_256MU806);
    expect(state, partial_page_cpu != NULL, "create partial-page Flash device");
    if (partial_page_cpu != NULL) {
        dspic33_load_program_word(partial_page_cpu, 0x2a7feu, 0x00010203u);
        dspic33_load_program_word(partial_page_cpu, 0x2a800u, 0u);
        dspic33_load_program_word(partial_page_cpu, 0x2abfeu, 0u);
        expect(state, dspic33_nvm_test_start_operation(partial_page_cpu, 3u, 0x2abfeu),
               "final partial page erase starts");
        expect(state, dspic33_nvm_test_finish_operation(partial_page_cpu),
               "final partial page erase completes");
        expect(state,
               dspic33_nvm_test_program_word(partial_page_cpu, 0x2a800u) == 0x00ffffffu &&
                   dspic33_nvm_test_program_word(partial_page_cpu, 0x2abfeu) == 0x00ffffffu,
               "final partial page erases implemented words");
        expect(state, dspic33_nvm_test_program_word(partial_page_cpu, 0x2a7feu) == 0x00010203u,
               "final partial page preserves preceding word");
        dspic33_destroy(partial_page_cpu);
    }
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

void dspic33_nvm_test_auxiliary_loader_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t binary_bytes[] = {0x56u, 0x34u, 0x12u, 0x00u};
    uint8_t elf_bytes[136] = {0u};
    ElfImage elf = {elf_bytes, sizeof(elf_bytes)};
    uint32_t entry;
    char error[128] = {0};

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_load_binary_data(cpu, binary_bytes, sizeof(binary_bytes),
                                    DSPIC33_AUXILIARY_PROGRAM_BASE * 2u, &entry),
           "binary loads auxiliary program section");
    expect(state, dspic33_read_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00123456u,
           "binary auxiliary word retained");

    elf_bytes[0] = 0x7fu;
    elf_bytes[1] = 'E';
    elf_bytes[2] = 'L';
    elf_bytes[3] = 'F';
    elf_bytes[4] = 1u;
    elf_bytes[5] = 1u;
    elf_bytes[6] = 1u;
    write_u16(elf_bytes, 16u, 2u);
    write_u16(elf_bytes, 18u, 118u);
    write_u32(elf_bytes, 20u, 1u);
    write_u32(elf_bytes, 32u, 52u);
    write_u16(elf_bytes, 40u, 52u);
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
           dspic33_read_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u) == 0x00654321u,
           "ELF auxiliary word retained");
}

void dspic33_nvm_test_auxiliary_access_and_execution_cases(TestState* state, Dspic33* cpu) {
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
    expect(state,
           dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00123456u) &&
               dspic33_load_program_word(cpu, 0x007ffffeu, 0x00654321u),
           "host loads auxiliary boundary words");
    expect(state,
           !dspic33_load_program_word(cpu, DSPIC33_PROGRAM_LIMIT, 0u) &&
               !dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_LIMIT, 0u),
           "host rejects unimplemented program addresses");
    expect(state,
           dspic33_read_program_byte(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x56u &&
               dspic33_read_program_byte(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 1u) == 0x34u &&
               dspic33_read_program_byte(cpu, 0x007fffffu) == 0x43u,
           "host reads auxiliary byte lanes");
    expect(state,
           dspic33_nvm_test_read_table(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, TBLRDL_W2_W3, &value) &&
               value == 0x3456u,
           "TBLRDL reads auxiliary low word");
    expect(state,
           dspic33_nvm_test_read_table(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, TBLRDH_W2_W3, &value) &&
               value == 0x0012u,
           "TBLRDH reads auxiliary high byte");
    expect(state,
           dspic33_nvm_test_read_table(cpu, 0x007ffffeu, TBLRDL_W2_W3, &value) && value == 0x4321u,
           "table read reaches auxiliary final word");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00123456u);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x02ffu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x3456u && cpu->cycles == 5u,
           "PSV low word reads auxiliary Flash");
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x03ffu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x0012u && cpu->cycles == 5u,
           "PSV high byte reads auxiliary Flash");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_MOV_LITERAL_0X1234_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u,
           "CPU executes auxiliary instruction");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_BTSC_W2_BIT_0);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u, OPCODE_NOP);
    cpu->w[2] = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 4u &&
               cpu->cycles == 2u,
           "skip decodes following auxiliary instruction");

    dspic33_reset(cpu, 0x007ffffcu);
    dspic33_load_program_word(cpu, 0x007ffffcu, OPCODE_BTSC_W2_BIT_0);
    dspic33_load_program_word(cpu, 0x007ffffeu, OPCODE_NOP);
    cpu->w[2] = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0u && cpu->cycles == 2u,
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
        uint8_t configuration = dspic33_nvm_test_codeguard_configuration_value(index);
        bool protected = dspic33_nvm_test_codeguard_configuration_high(configuration);
        uint64_t illegal_resets;
        uint64_t software_resets;
        dspic33_reset(cpu, 0u);
        cpu->stop_on_trap = false;
        expect(state, dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, configuration),
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
                   (protected ? cpu->illegal_reset && cpu->pc == 0u &&
                                    cpu->illegal_reset_count == illegal_resets + 1u &&
                                    cpu->software_reset_count == software_resets + 1u &&
                                    (dspic33_read_word(cpu, 0x0740u) & 0x4040u) == 0x4040u
                              : !cpu->illegal_reset && cpu->pc == 0x007ffffcu &&
                                    cpu->illegal_reset_count == illegal_resets &&
                                    cpu->software_reset_count == software_resets + 1u),
               "B1 auxiliary reset protection matrix");
        instructions = cpu->instructions;
        bool next_valid =
            dspic33_step(cpu) == DSPIC33_RUNNING &&
            (protected ? cpu->illegal_reset && cpu->pc == 0u && cpu->instructions == instructions
                       : cpu->pc == 0x000100u && cpu->instructions == instructions + 1u);
        expect(state, next_valid, "B1 auxiliary reset execution matrix");
    }
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdfu);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x31u),
           "load B1 protected auxiliary hardware reset");
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdbu);
    instructions = cpu->instructions;
    dspic33_configuration_mismatch_reset(cpu);
    expect(state,
           cpu->reset_locked && cpu->illegal_reset && cpu->pc == 0u &&
               (dspic33_read_word(cpu, 0x0740u) & 0x4200u) == 0x4200u,
           "B1 protected auxiliary hardware reset locks execution");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->instructions == instructions,
           "B1 protected hardware reset cannot fetch application code");
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize B1 protected reset copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy B1 protected reset lock");
        expect(state,
               dspic33_step(&copy) == DSPIC33_RUNNING && copy.reset_locked &&
                   copy.instructions == instructions,
               "copied B1 protected reset remains locked");
        dspic33_release(&copy);
    }
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->reset_locked && !cpu->illegal_reset,
           "power-on reset clears B1 protected reset lock");
    dspic33_load_configuration_word(cpu, 0xf8000eu, 0xffdfu);
    dspic33_nvm_test_load_codeguard_configuration(cpu, 0x03u, 0x03u);

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_NOP);
    dspic33_load_program_word(cpu, 0x007ffffau, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0100u, OPCODE_NOP);
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
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0102u, OPCODE_RETFIE);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE,
           "RETFIE restores auxiliary return address");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_MOV_W1_W2);
    dspic33_load_program_word(cpu, 0x000006u, 0x000240u);
    dspic33_load_program_word(cpu, 0x007ffffau, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0200u);
    dspic33_set_working_register(cpu, 1u, 0x1001u);
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
               cpu->last_trap_return == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u && cpu->pc == 0x000240u,
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
               cpu->last_trap_return == DSPIC33_AUXILIARY_PROGRAM_BASE && cpu->pc == 0x000282u &&
               cpu->last_interrupt == UINT16_MAX,
           "B1 invalid auxiliary interrupt vector uses general Address Error");

    dspic33_reset(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE);
    dspic33_load_program_word(cpu, 0x000006u, 0x0002a0u);
    dspic33_load_program_word(cpu, 0x007ffffau, 0x00060000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    dspic33_raise_dma_collision_trap(cpu);
    expect(state,
           cpu->last_trap == 1u && cpu->last_trap_return == DSPIC33_AUXILIARY_PROGRAM_BASE &&
               cpu->pc == 0x0002a0u && (dspic33_read_word(cpu, 0x08c0u) & 0x0028u) == 0x0028u,
           "B1 invalid auxiliary trap vector uses general Address Error");

    dspic33_reset(cpu, 0x007ffffeu);
    dspic33_load_program_word(cpu, 0x007ffffeu, OPCODE_MOV_W1_W2);
    dspic33_load_program_word(cpu, 0x000006u, 0x000260u);
    dspic33_load_program_word(cpu, 0x007ffffau, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x0300u);
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
               dspic33_read_program_word(&copy, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00010203u &&
                   dspic33_read_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00040506u,
               "auxiliary program copies remain independent");
        dspic33_reset(&copy, 0u);
        expect(state,
               dspic33_read_program_word(&copy, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00010203u,
               "reset preserves auxiliary Flash");
        dspic33_release(&copy);
    }
}
void dspic33_nvm_test_auxiliary_nvm_cases(TestState* state, Dspic33* cpu) {
    uint32_t index;
    uint64_t instructions;

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00f0f0f0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u, 0x000f0f0fu);
    cpu->write_latches[0] = 0x00123456u;
    cpu->write_latches[1] = 0x00654321u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE),
           "auxiliary pair operation starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "auxiliary pair operation completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00103050u &&
               dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 2u) ==
                   0x00050301u,
           "auxiliary pair programs both words");

    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u + index * 2u,
                                  0x00ffffffu);
        cpu->write_latches[index] = 0x00330000u | index;
    }
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 2u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x17au),
           "auxiliary row operation starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "auxiliary row operation completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u) ==
                   0x00330000u &&
               dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1feu) ==
                   0x0033007fu,
           "auxiliary row programs exact aligned range");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x7feu, 0u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x800u, 0x00123456u);
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 3u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x456u),
           "auxiliary page erase starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "auxiliary page erase completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x400u) ==
                   0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x7feu) ==
                   0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x800u) ==
                   0x00123456u,
           "auxiliary page erase preserves adjacent page");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x007ffffcu, 0x00ffffffu);
    dspic33_load_program_word(cpu, 0x007ffffeu, 0x00ffffffu);
    cpu->write_latches[0] = 0x00010203u;
    cpu->write_latches[1] = 0x00040506u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x007ffffeu),
           "auxiliary final pair starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "auxiliary final pair completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x007ffffcu) == 0x00010203u &&
               dspic33_nvm_test_program_word(cpu, 0x007ffffeu) == 0x00040506u,
           "auxiliary final pair aligns within segment");

    dspic33_reset(cpu, 0u);
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        dspic33_load_program_word(cpu, 0x007fff00u + index * 2u, 0x00ffffffu);
        cpu->write_latches[index] = 0x00660000u | index;
    }
    expect(state, dspic33_nvm_test_start_operation(cpu, 2u, 0x007ffffeu),
           "auxiliary final row starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "auxiliary final row completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x007fff00u) == 0x00660000u &&
               dspic33_nvm_test_program_word(cpu, 0x007ffffeu) == 0x0066007fu,
           "auxiliary final row remains within segment");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x007ff800u, 0u);
    dspic33_load_program_word(cpu, 0x007ffffeu, 0u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 3u, 0x007ffffeu),
           "auxiliary final page starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "auxiliary final page completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x007ff800u) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, 0x007ffffeu) == 0x00ffffffu,
           "auxiliary final page remains within segment");

    dspic33_reset(cpu, 0u);
    dspic33_nvm_test_configure_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_LIMIT, true);
    expect(state, dspic33_nvm_test_execute_start_sequence(cpu, false),
           "auxiliary out-of-range pair sequence executes");
    expect(state, !cpu->nvm.active, "auxiliary out-of-range pair rejected");

    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x2000u, 0x00010203u);
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, 0x00040506u);
    dspic33_load_configuration_word(cpu, 0xf80004u, 0xff30u);
    dspic33_load_configuration_word(cpu, 0xf80010u, 0xff30u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 0x0au, 0u), "auxiliary bulk erase starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "auxiliary bulk erase completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, 0x2000u) == 0x00010203u,
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
    expect(state,
           dspic33_nvm_test_start_operation_from(cpu, 0x0du, 0u,
                                                 DSPIC33_AUXILIARY_PROGRAM_BASE + 0x100u),
           "primary bulk erase starts");
    expect(state, dspic33_nvm_test_finish_operation(cpu), "primary bulk erase completes");
    expect(state,
           dspic33_nvm_test_program_word(cpu, 0x2000u) == 0x00ffffffu &&
               dspic33_nvm_test_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE) == 0x00040506u,
           "primary bulk erase preserves auxiliary Flash");
    expect(state,
           dspic33_read_configuration_byte(cpu, 0xf80004u) == 0xcfu &&
               dspic33_read_configuration_byte(cpu, 0xf80005u) == 0xffu &&
               dspic33_read_configuration_byte(cpu, 0xf80010u) == 0x30u,
           "primary bulk erase restores only FGS");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1000u),
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
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3000u),
           "opposite-segment primary program starts");
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1800u, OPCODE_NOP);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x1800u;
    instructions = cpu->instructions;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               cpu->instructions == instructions + 1u,
           "auxiliary execution continues during primary programming");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    cpu->write_latches[1] = 0x00445566u;
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2000u),
           "same-segment auxiliary program starts");
    dspic33_load_program_word(cpu, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2800u, OPCODE_NOP);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2800u;
    instructions = cpu->instructions;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               cpu->instructions == instructions,
           "auxiliary execution stalls during auxiliary programming");
}

void dspic33_nvm_test_stall_and_interrupt_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0032u, 0x000300u);
    dspic33_load_program_word(cpu, 0x0300u, 0x000000u);
    dspic33_write_word(cpu, 0x0820u, 0x8000u);
    dspic33_write_word(cpu, 0x0846u, 0x3000u);
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3200u), "stall operation starts");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "completion stall cycle advances");
    expect(state, !cpu->nvm.active && dspic33_nvm_test_interrupt_flag(cpu),
           "completion ends stall and raises NVMIF");
    expect(state,
           cpu->pc == NVM_SEQUENCE_BASE + 10u && cpu->instructions == 5u &&
               cpu->interrupt_count == 0u,
           "completion cycle defers interrupt service");
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "post-completion instruction advances");
    expect(state, cpu->last_interrupt == NVM_IRQ && cpu->interrupt_count == 1u,
           "NVM interrupt serviced after stall");
    expect(state, cpu->pc == 0x0302u && cpu->instructions == 6u,
           "NVM vector instruction executes after service");
}

void dspic33_nvm_test_same_segment_stall_erratum_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00112233u;
    dspic33_nvm_test_configure_operation(cpu, 1u, 0x3400u, true);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state, dspic33_nvm_test_execute_start_sequence(cpu, false) && cpu->nvm.active,
           "same-segment RTSP starts without the documented interrupt workaround");
    expect(state,
           !cpu->nvm.stall_workaround && dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED &&
               cpu->nvm.active,
           "B1 same-segment RTSP stall remains silicon-undefined with GIE enabled");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x00445566u;
    expect(state, dspic33_nvm_test_start_operation(cpu, 1u, 0x3600u) && cpu->nvm.stall_workaround,
           "same-segment RTSP captures the disabled-interrupt workaround");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
               dspic33_nvm_test_program_word(cpu, 0x3600u) == 0x00445566u,
           "disabled-interrupt workaround permits the documented RTSP stall");

    dspic33_reset(cpu, 0u);
    cpu->write_latches[0] = 0x000000fdu;
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 0u, DSPIC33_CONFIGURATION_BASE + 4u) &&
               cpu->nvm.active,
           "configuration-byte programming starts in the execution segment");
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active,
           "configuration-byte programming remains outside the RTSP stall erratum");
}

void dspic33_nvm_test_power_save_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t operations[] = {1u, 2u, 3u, 0x0au, 0x0du};
    static const uint32_t targets[] = {DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u, 0x3a00u,
                                       DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2000u, 0u, 0u};
    static const uint32_t execution_addresses[] = {
        NVM_SEQUENCE_BASE + 10u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3000u, NVM_SEQUENCE_BASE + 10u,
        NVM_SEQUENCE_BASE + 10u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3200u};
    size_t index;

    for (index = 0u; index < sizeof(operations) / sizeof(operations[0]); index++) {
        uint32_t execution_address = execution_addresses[index];
        uint32_t opcode = (index & 1u) == 0u ? OPCODE_SLEEP : OPCODE_IDLE;
        Dspic33StopReason stop_reason = (index & 1u) == 0u ? DSPIC33_SLEEPING : DSPIC33_IDLING;
        Dspic33PowerState power_state =
            (index & 1u) == 0u ? DSPIC33_POWER_SLEEP : DSPIC33_POWER_IDLE;
        uint16_t rcon_bit = (index & 1u) == 0u ? 0x0008u : 0x0004u;
        uint64_t instructions;
        uint16_t rcon;

        dspic33_reset(cpu, 0u);
        cpu->write_latches[0] = 0x00112233u;
        cpu->write_latches[1] = 0x00445566u;
        expect(state, dspic33_nvm_test_start_operation(cpu, operations[index], targets[index]),
               "opposite-segment power-save operation starts");
        dspic33_load_program_word(cpu, execution_address, opcode);
        dspic33_load_program_word(cpu, execution_address + 2u, opcode);
        cpu->pc = execution_address;
        cpu->watchdog.ticks = 17u;
        rcon = dspic33_read_word(cpu, 0x0740u);
        instructions = cpu->instructions;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->power_state == DSPIC33_POWER_ACTIVE &&
                   cpu->stop_reason == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0740u) == rcon &&
                   cpu->watchdog.ticks == 17u && cpu->instructions == instructions + 1u &&
                   cpu->pc == execution_address + 2u && !cpu->nvm.active,
               "active NVM ignores opposite-segment power-save instruction");
        expect(state,
               dspic33_step(cpu) == stop_reason && cpu->power_state == power_state &&
                   (dspic33_read_word(cpu, 0x0740u) & rcon_bit) != 0u && cpu->watchdog.ticks == 0u,
               "completed NVM permits the same power-save instruction");
    }

    for (index = 0u; index < 2u; index++) {
        uint16_t operation = 1u;
        uint32_t target = index == 0u ? 0x3c00u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x2000u;
        uint32_t execution_address =
            index == 0u ? NVM_SEQUENCE_BASE + 10u : DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3400u;
        uint32_t opcode = index == 0u ? OPCODE_SLEEP : OPCODE_IDLE;
        Dspic33StopReason stop_reason = index == 0u ? DSPIC33_SLEEPING : DSPIC33_IDLING;
        Dspic33PowerState power_state = index == 0u ? DSPIC33_POWER_SLEEP : DSPIC33_POWER_IDLE;
        uint16_t rcon_bit = index == 0u ? 0x0008u : 0x0004u;
        uint64_t instructions;
        uint16_t rcon;

        dspic33_reset(cpu, 0u);
        cpu->write_latches[0] = 0x00112233u;
        cpu->write_latches[1] = 0x00445566u;
        expect(state, dspic33_nvm_test_start_operation(cpu, operation, target),
               "same-segment power-save operation starts");
        dspic33_load_program_word(cpu, execution_address, opcode);
        cpu->pc = execution_address;
        cpu->watchdog.ticks = 19u;
        rcon = dspic33_read_word(cpu, 0x0740u);
        instructions = cpu->instructions;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->nvm.active &&
                   cpu->pc == execution_address && cpu->instructions == instructions &&
                   dspic33_read_word(cpu, 0x0740u) == rcon && cpu->watchdog.ticks == 19u &&
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
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3800u),
           "interrupt-before-power-save operation starts");
    dspic33_load_program_word(cpu, cpu->pc, OPCODE_SLEEP);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
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
    expect(state,
           dspic33_nvm_test_start_operation(cpu, 1u, DSPIC33_AUXILIARY_PROGRAM_BASE + 0x3a00u),
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
                   cpu->interrupt_count == 0u && dspic33_nvm_test_interrupt_flag(cpu) &&
                   cpu->power_state == DSPIC33_POWER_ACTIVE && cpu->pc == pc + 2u &&
                   cpu->instructions == instructions + 1u &&
                   dspic33_read_word(cpu, 0x0740u) == rcon && cpu->watchdog.ticks == 23u,
               "priority-zero interrupt remains pending after ignored power-save");
    }
}
