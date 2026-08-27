#include "architecture/dspic33/exceptions/internal.h"

void dspic33_fault_test_page_zero_address_error_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X9000_W2);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dsrpag = 0u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "direct page-zero word read traps");
    expect(state, cpu->w[2] == 0x5a5au && dspic33_read_word(cpu, 0x1000u) == 0x5a5au,
           "direct page-zero word read completes before the trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_0X9000);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dswpag = 0u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "direct page-zero word write traps");
    expect(state, dspic33_read_word(cpu, 0x1000u) == 0xa5a5u,
           "direct page-zero word write completes before the trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_MOVSAC_WRITE_BACK);
    dspic33_set_working_register(cpu, 4u, 1u);
    dspic33_set_working_register(cpu, 5u, 1u);
    dspic33_set_working_register(cpu, 9u, 0x1002u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    dspic33_set_working_register(cpu, 13u, 0x9000u);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    dspic33_write_word(cpu, 0x9000u, 0xa5a5u);
    cpu->accumulator[1] = 0x654321u;
    cpu->dswpag = 0u;
    expect_address_trap(state, cpu, "DSP write-back through page zero traps");
    expect(state,
           cpu->w[13] == 0x9002u && dspic33_read_word(cpu, 0x1000u) == 0x0065u &&
               dspic33_read_word(cpu, 0x9000u) == 0xa5a5u,
           "DSP write-back uses X WAGU data-page translation before trapping");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dsrpag = 0u;
    cpu->dswpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "page-zero word read traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0x5a5au &&
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au && cpu->dsrpag == 0u && cpu->dswpag == 1u,
           "page-zero word read completes through DSRPAG alias");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->dswpag = 0u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "page-zero word write traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa5a5u &&
               dspic33_read_word(cpu, 0x1000u) == 0xa5a5u && cpu->dsrpag == 1u && cpu->dswpag == 0u,
           "page-zero word write completes through DSWPAG alias");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dsrpag = 0u;
    cpu->dswpag = 1u;
    cpu->w[1] = 0x9001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "page-zero byte read traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa55au &&
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au && cpu->dsrpag == 0u && cpu->dswpag == 1u,
           "page-zero byte read completes through DSRPAG alias");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->dswpag = 0u;
    cpu->w[1] = 0x9001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "page-zero byte write traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa5a5u &&
               dspic33_read_word(cpu, 0x1000u) == 0xa55au && cpu->dsrpag == 1u && cpu->dswpag == 0u,
           "page-zero byte write completes through DSWPAG alias");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W4_LITERAL_2_W2);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dsrpag = 0u;
    cpu->w[2] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "page-zero literal-offset read traps");
    expect(state, cpu->w[2] == 0x5a5au && cpu->w[4] == 0x8ffeu,
           "page-zero literal-offset read completes without pointer update");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W4_LITERAL_2);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dswpag = 0u;
    cpu->w[1] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "page-zero literal-offset write traps");
    expect(state, dspic33_read_word(cpu, 0x1000u) == 0xa5a5u && cpu->w[4] == 0x8ffeu,
           "page-zero literal-offset write completes without pointer update");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_fault_test_prepare_timer_source(cpu);
    cpu->dsrpag = 0u;
    cpu->w[1] = 0x8106u;
    cpu->w[2] = 0xbeefu;
    expect_address_trap(state, cpu, "page-zero side-effecting SFR read traps");
    expect(state,
           cpu->w[1] == 0x8108u && cpu->w[2] == 0x1234u &&
               dspic33_read_word(cpu, 0x0108u) == 0x5555u,
           "page-zero SFR read completes value and latch side effect");

    reset_processor_test(cpu, 0u);
    cpu->dsrpag = 0u;
    cpu->dswpag = 0u;
    cpu->instruction_active = true;
    cpu->io.dma_transfer_active = true;
    dspic33_write_word(cpu, 0x9000u, 0x1234u);
    expect(state, dspic33_read_word(cpu, 0x9000u) == 0x1234u && !cpu->address_error,
           "DMA raw access bypasses page-zero CPU trap");
    cpu->instruction_active = false;
    cpu->io.dma_transfer_active = false;
}

void dspic33_fault_test_unimplemented_data_page_address_error_cases(TestState* state,
                                                                    Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "implemented EDS page word read completes");
    expect(state, cpu->w[1] == 0x9002u && cpu->w[2] == 0x5a5au,
           "implemented EDS page word read updates result and pointer");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dswpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "implemented EDS page word write completes");
    expect(state, cpu->w[1] == 0x9002u && dspic33_read_word(cpu, 0x9000u) == 0xa5a5u,
           "implemented EDS page word write updates memory and pointer");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X9000_W2);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "direct high-file page-one read completes");
    expect(state, cpu->w[2] == 0x5a5au && !cpu->address_error,
           "direct high-file page-one read uses implemented memory");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_0X9000);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dswpag = 1u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "direct high-file page-one write completes");
    expect(state, dspic33_read_word(cpu, 0x9000u) == 0xa5a5u && !cpu->address_error,
           "direct high-file page-one write uses implemented memory");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X9000_W2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 2u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "direct unimplemented EDS page read traps");
    expect(state, cpu->w[2] == 0u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "direct unimplemented EDS read returns zero");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_0X9000);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dswpag = 2u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "direct unimplemented EDS page write traps");
    expect(state, cpu->w[2] == 0xa5a5u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "direct unimplemented EDS write preserves source and backing");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 2u;
    cpu->dswpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented EDS page word read traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0u && cpu->dsrpag == 2u && cpu->dswpag == 1u &&
               dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "unimplemented EDS word read returns zero and completes pointer update");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->dswpag = 2u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented EDS page word write traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa5a5u && cpu->dsrpag == 1u && cpu->dswpag == 2u &&
               dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "unimplemented EDS word write preserves source and raw backing");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 2u;
    cpu->w[1] = 0x9001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented EDS page byte read traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa500u &&
               dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "unimplemented EDS byte read returns zero and updates pointer");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dswpag = 2u;
    cpu->w[1] = 0x9001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented EDS page byte write traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa5a5u &&
               dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "unimplemented EDS byte write preserves source and raw backing");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W4_LITERAL_2_W2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 2u;
    cpu->w[2] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "unimplemented EDS literal-offset read traps");
    expect(state, cpu->w[2] == 0u && cpu->w[4] == 0x8ffeu,
           "unimplemented EDS literal read returns zero without pointer update");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W4_LITERAL_2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dswpag = 2u;
    cpu->w[1] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "unimplemented EDS literal-offset write traps");
    expect(state, dspic33_read_word(cpu, 0x11000u) == 0x5a5au && cpu->w[4] == 0x8ffeu,
           "unimplemented EDS literal write preserves backing and pointer");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x11000u, 0x1122u);
    dspic33_write_word(cpu, 0x11002u, 0x3344u);
    cpu->dsrpag = 2u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    cpu->w[3] = 0x5a5au;
    expect_address_trap(state, cpu, "unimplemented EDS MOV.D read traps");
    expect(state,
           cpu->w[1] == 0x9004u && cpu->w[2] == 0u && cpu->w[3] == 0u &&
               dspic33_read_word(cpu, 0x11000u) == 0x1122u &&
               dspic33_read_word(cpu, 0x11002u) == 0x3344u,
           "unimplemented EDS MOV.D read returns two zero words");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x11000u, 0x1122u);
    dspic33_write_word(cpu, 0x11002u, 0x3344u);
    cpu->dswpag = 2u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0x5555u;
    cpu->w[3] = 0x6666u;
    expect_address_trap(state, cpu, "unimplemented EDS MOV.D write traps");
    expect(state,
           cpu->w[1] == 0x9004u && cpu->w[2] == 0x5555u && cpu->w[3] == 0x6666u &&
               dspic33_read_word(cpu, 0x11000u) == 0x1122u &&
               dspic33_read_word(cpu, 0x11002u) == 0x3344u,
           "unimplemented EDS MOV.D write inhibits both words");

    reset_processor_test(cpu, 0u);
    dspic33_write_word(cpu, 0x11000u, 0x1234u);
    expect(state, dspic33_read_word(cpu, 0x11000u) == 0x1234u && !cpu->address_error,
           "debugger raw access bypasses unimplemented EDS trap");

    cpu->instruction_active = true;
    cpu->io.dma_transfer_active = true;
    dspic33_write_word(cpu, 0x11000u, 0x5678u);
    expect(state, dspic33_read_word(cpu, 0x11000u) == 0x5678u && !cpu->address_error,
           "DMA raw access bypasses unimplemented EDS trap");
    cpu->instruction_active = false;
    cpu->io.dma_transfer_active = false;
}

void dspic33_fault_test_w15_write_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_ODD_W15);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W0_W15);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_BYTE_W0_W15);
    load_instruction(state, cpu, 0x206u, OPCODE_MOV_BYTE_W15_POST_INCREMENT_W0);
    load_instruction(state, cpu, 0x208u, OPCODE_MOV_BYTE_W15_PRE_INCREMENT_W1);
    load_instruction(state, cpu, 0x20au, OPCODE_MOV_BYTE_W15_POST_DECREMENT_W2);
    load_instruction(state, cpu, 0x20cu, OPCODE_MOV_BYTE_W15_PRE_DECREMENT_W3);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute odd W15 literal");
    expect(state, cpu->w[15] == 0x5000u, "literal write clears W15 low bit");
    cpu->w[0] = 0x5001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute odd W15 register move");
    expect(state, cpu->w[15] == 0x5000u, "register move clears W15 low bit");
    cpu->w[0] = 0x0001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute odd W15 byte register move");
    expect(state, cpu->w[15] == 0x5000u, "byte register move clears W15 low bit");
    dspic33_write_byte(cpu, 0x001eu, 0x01u);
    expect(state, cpu->w[15] == 0x5000u, "byte alias write clears W15 low bit");
    dspic33_write_word(cpu, 0x001eu, 0x5001u);
    expect(state, cpu->w[15] == 0x5000u, "word alias write clears W15 low bit");
    dspic33_write_word(cpu, 0x5000u, 0x2211u);
    dspic33_write_word(cpu, 0x5002u, 0x4433u);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute W15 byte post-increment");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[0] & 0x00ffu) == 0x0011u,
           "W15 byte post-increment retains even pointer and old address");
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute W15 byte pre-increment");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[1] & 0x00ffu) == 0x0022u,
           "W15 byte pre-increment uses transient odd address");
    cpu->w[15] = 0x5002u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute W15 byte post-decrement");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[2] & 0x00ffu) == 0x0033u,
           "W15 byte post-decrement retains old address");
    cpu->w[15] = 0x5002u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute W15 byte pre-decrement");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[3] & 0x00ffu) == 0x0022u,
           "W15 byte pre-decrement uses transient odd address");
}

void dspic33_fault_test_valid_stack_frame_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_LNK_0);
    load_instruction(state, cpu, 0x202u, OPCODE_ULNK);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    cpu->w[14] = 0x4444u;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute valid LNK");
    expect(state, cpu->w[14] == 0x5002u && cpu->w[15] == 0x5002u && (cpu->corcon & 0x0004u) != 0u,
           "valid LNK updates frame state");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute valid ULNK");
    expect(state, cpu->w[14] == 0x4444u && cpu->w[15] == 0x5000u && (cpu->corcon & 0x0004u) == 0u,
           "valid ULNK restores frame state");
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0u,
           "valid frame operations do not set STKERR");
    expect(state, active_pending_traps(cpu) == 0u, "valid frame operations do not schedule traps");
}

void dspic33_fault_test_invalid_lnk_case(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0x200u);
    cpu->stop_on_trap = false;
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0x200u, OPCODE_LNK_0);
    load_instruction(state, cpu, 0x202u, OPCODE_LNK_0);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    cpu->w[14] = 0x4444u;
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute initial valid LNK");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute invalid SFA LNK");
    expect(state, cpu->w[14] == 0x5004u && cpu->w[15] == 0x5004u,
           "invalid SFA LNK completes frame effects");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0004u) != 0u,
           "invalid SFA LNK sets STKERR immediately");
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "invalid SFA LNK traps after one instruction");
    expect(state, cpu->w[1] == 0x1111u, "invalid SFA LNK executes one sentinel");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000300u,
           "invalid SFA LNK enters stack trap");
    expect(state, cpu->w[15] == 0x5008u && dspic33_read_word(cpu, 0x5004u) == 0x0207u,
           "invalid SFA LNK stacks completed return state");
}

void dspic33_fault_test_invalid_ulnk_case(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0x200u);
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0x200u, OPCODE_ULNK);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    dspic33_write_word(cpu, 0x5000u, 0xabcdu);
    cpu->w[14] = 0x5002u;
    cpu->w[15] = 0x5010u;
    cpu->stop_on_trap = true;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute invalid SFA ULNK");
    expect(state, cpu->w[14] == 0xabcdu && cpu->w[15] == 0x5000u,
           "invalid SFA ULNK completes frame effects");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0004u) != 0u,
           "invalid SFA ULNK sets STKERR immediately");
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "invalid SFA ULNK traps after one instruction");
    expect(state, cpu->w[1] == 0x1111u, "invalid SFA ULNK executes one sentinel");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000300u,
           "invalid SFA ULNK enters stack trap");
}

void dspic33_fault_test_simultaneous_trap_case(TestState* state, Dspic33* cpu) {
    const Dspic33PendingSoftTrap* pending;
    reset_processor_test(cpu, 0x200u);
    cpu->stop_on_trap = false;
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0x200u, OPCODE_SFTAC_A_W5);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    cpu->w[5] = 17u;
    cpu->w[15] = 0x5000u;
    dspic33_check_stack_address(cpu, 0x5102, false, 2u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "schedule simultaneous stack and math traps");
    expect(state, active_pending_traps(cpu) == 2u, "simultaneous trap sources remain distinct");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "service simultaneous trap boundary");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000300u,
           "simultaneous trap boundary chooses stack priority");
    pending = pending_trap(cpu, 4u);
    expect(state, pending != NULL && pending->delay == 0u,
           "simultaneous boundary retains ready math trap");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "return from stack trap with retained math trap");
    expect(state, cpu->last_trap == 4u && cpu->pc == 0x000320u && cpu->trap_count == 2u,
           "retained math trap enters after stack RETFIE");
}

void dspic33_fault_test_earlier_deadline_case(TestState* state, Dspic33* cpu) {
    const Dspic33PendingSoftTrap* pending;
    reset_processor_test(cpu, 0x200u);
    cpu->stop_on_trap = false;
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0x200u, OPCODE_SFTAC_A_W5);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    cpu->w[5] = 17u;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "schedule earlier math trap");
    dspic33_check_stack_address(cpu, 0x5102, false, 2u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "service earlier math deadline");
    expect(state, cpu->last_trap == 4u && cpu->pc == 0x000320u,
           "earlier math deadline precedes later stack priority");
    pending = pending_trap(cpu, 3u);
    expect(state, pending != NULL && pending->delay == 1u, "later stack deadline remains pending");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "advance math handler to stack deadline");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000300u && cpu->trap_count == 2u,
           "ready stack trap preempts math handler");
}

void dspic33_fault_test_repeat_exception_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t divide_opcodes[] = {OPCODE_DIV_SW_W2_W3, OPCODE_DIV_SD_W4_W3,
                                              OPCODE_DIV_UW_W2_W3, OPCODE_DIV_UD_W4_W3,
                                              OPCODE_DIVF_W2_W3};
    size_t index;

    for (index = 0u; index < sizeof(divide_opcodes) / sizeof(divide_opcodes[0]); index++) {
        reset_processor_test(cpu, 0x200u);
        load_instruction(state, cpu, 0x00000cu, 0x000320u);
        load_instruction(state, cpu, 0x200u, 0x090011u);
        load_instruction(state, cpu, 0x202u, divide_opcodes[index]);
        cpu->w[0] = 0xaaaau;
        cpu->w[1] = 0xbbbbu;
        cpu->w[2] = 0x2222u;
        cpu->w[3] = 0u;
        cpu->w[4] = 0x4444u;
        cpu->w[5] = 0x5555u;
        cpu->w[15] = 0x5000u;
        cpu->stop_on_trap = true;
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "initialize repeated divide");
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u && cpu->rcount == 16u &&
                   cpu->repeat_active != 0u &&
                   (dspic33_read_word(cpu, 0x08c0u) & 0x0050u) == 0x0050u &&
                   pending_trap(cpu, 4u) != NULL && pending_trap(cpu, 4u)->delay == 1u,
               "first divide cycle latches delayed math trap");
        expect(state,
               dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 4u &&
                   cpu->last_trap_return == 0x202u && cpu->pc == 0x000320u && cpu->rcount == 15u &&
                   cpu->repeat_active == 0u && (cpu->sr & 0x0010u) == 0u && cpu->cycles == 3u,
               "second divide cycle enters math trap");
        expect(state,
               cpu->w[15] == 0x5004u && dspic33_read_word(cpu, 0x5000u) == 0x202u &&
                   (dspic33_read_word(cpu, 0x5002u) & 0x1000u) != 0u &&
                   dspic33_read_word(cpu, 0x08c8u) == 0x0b04u,
               "divide math trap preserves repeat frame state");
    }

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x090011u);
    load_instruction(state, cpu, 0x202u, OPCODE_DIV_SD_W4_W3);
    cpu->w[3] = 0xffffu;
    cpu->w[4] = 0u;
    cpu->w[5] = 0x8000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "initialize B1 signed double divide overflow");
    while (cpu->repeat_active != 0u) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "execute B1 signed double divide overflow");
    }
    expect(state, cpu->w[0] == 0u && cpu->w[1] == 0u && (cpu->sr & 0x000au) == 0x0002u,
           "B1 affected signed double divide preserves defined flags");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x090011u);
    load_instruction(state, cpu, 0x202u, OPCODE_DIV_SD_W4_W3);
    cpu->w[3] = 1u;
    cpu->w[4] = 0x9c40u;
    cpu->w[5] = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "initialize unaffected signed double divide overflow");
    while (cpu->repeat_active != 0u) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "execute unaffected signed double divide overflow");
    }
    expect(state, (cpu->sr & 0x0004u) != 0u, "unaffected signed double divide overflow sets OV");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x00000cu, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x200u, 0x090011u);
    load_instruction(state, cpu, 0x202u, OPCODE_DIV_SW_W2_W3);
    cpu->w[2] = 42u;
    cpu->w[3] = 0u;
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = false;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "initialize recursive math repeat");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "latch recursive math source");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u && cpu->trap_count == 1u &&
               cpu->rcount == 15u,
           "enter first repeated divide math trap");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u && cpu->trap_count == 2u &&
               cpu->last_trap_return == 0x202u && cpu->rcount == 15u && cpu->w[15] == 0x5004u &&
               cpu->cycles == 8u,
           "uncleared MATHERR re-enters before repeated instruction");
    dspic33_write_word(cpu, 0x08c0u, 0x0040u);
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0x0040u && pending_trap(cpu, 4u) == NULL,
           "clearing MATHERR preserves cause and clears level source");
    dspic33_write_word(cpu, 0x08c0u, 0u);
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0u,
           "software clears independent DIV0ERR cause");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u && cpu->repeat_active != 0u &&
               cpu->repeat_pc == 0x202u && cpu->rcount == 15u && (cpu->sr & 0x0010u) != 0u &&
               cpu->cycles == 14u,
           "RETFIE restores suspended repeat state");

    dspic33_write_word(cpu, 0x08c0u, 0x0010u);
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0x0010u && pending_trap(cpu, 4u) == NULL,
           "software MATHERR stores status without creating a trap source");
    dspic33_write_word(cpu, 0x08c0u, 0u);
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0u && pending_trap(cpu, 4u) == NULL,
           "software MATHERR clear cancels pending level source");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x090010u);
    load_instruction(state, cpu, 0x202u, OPCODE_DIV_SW_W2_W3);
    cpu->w[2] = 42u;
    cpu->w[3] = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "initialize short repeated divide");
    while (cpu->repeat_active != 0u) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute short repeated divide cycle");
    }
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0u && cpu->trap_count == 0u,
           "repeat count below seventeen does not latch DIV0");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x090012u);
    load_instruction(state, cpu, 0x202u, OPCODE_DIV_SW_W2_W3);
    load_instruction(state, cpu, 0x00000cu, 0x000320u);
    cpu->w[2] = 42u;
    cpu->w[3] = 0u;
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "initialize long repeated divide");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->rcount == 17u,
           "leading nonstandard divide iteration completes without trap");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->rcount == 16u,
           "RCOUNT seventeen latches delayed DIV0");
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->rcount == 15u &&
               cpu->last_trap_return == 0x202u,
           "nonstandard long repeat enters delayed DIV0 at proven stage");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000014u, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000302u, OPCODE_RETFIE);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "initialize interruptible repeat");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000302u &&
               cpu->repeat_active == 0u && cpu->rcount == 2u && (cpu->sr & 0x0010u) == 0u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x1000u) != 0u,
           "interrupt entry suspends repeat and stacks RA");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u && cpu->repeat_active != 0u &&
               cpu->repeat_pc == 0x202u && cpu->rcount == 2u && (cpu->sr & 0x0010u) != 0u,
           "interrupt RETFIE restores repeat state");
}

void dspic33_fault_test_standalone_divide_zero_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t divide_opcodes[] = {OPCODE_DIV_SW_W2_W3, OPCODE_DIV_SD_W4_W3,
                                              OPCODE_DIV_UW_W2_W3, OPCODE_DIV_UD_W4_W3,
                                              OPCODE_DIVF_W2_W3};
    size_t index;

    for (index = 0u; index < sizeof(divide_opcodes) / sizeof(divide_opcodes[0]); index++) {
        reset_processor_test(cpu, 0x0200u);
        load_instruction(state, cpu, 0x0200u, divide_opcodes[index]);
        cpu->w[0] = 0xaaaau;
        cpu->w[1] = 0xbbbbu;
        cpu->w[2] = 0x2222u;
        cpu->w[3] = 0u;
        cpu->w[4] = 0x4444u;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (dspic33_read_word(cpu, 0x08c0u) & 0x0050u) == 0x0050u && cpu->w[0] == 0xaaaau &&
                   cpu->w[1] == 0xbbbbu && pending_trap(cpu, 4u) != NULL,
               "standalone divide by zero latches first-cycle math error");
    }
}

void dspic33_fault_test_repeat_interrupt_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_DISI_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_IFS1);
    load_instruction(state, cpu, 0x204u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x206u, OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x00003cu, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000302u, OPCODE_RETFIE);
    cpu->w[1] = 0x0010u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0822u, 0x0010u);
    dspic33_write_word(cpu, 0x084au, 0x0004u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 2u && cpu->cycles == 1u,
           "DISI initializes integrated repeat interrupt window");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 1u && cpu->cycles == 2u &&
               (dspic33_read_word(cpu, 0x0802u) & 0x0010u) != 0u,
           "word IFS write consumes one disabled cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 0u && cpu->repeat_active != 0u &&
               cpu->repeat_pc == 0x206u && cpu->rcount == 2u && cpu->cycles == 3u,
           "REPEAT consumes final disabled cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000302u &&
               cpu->last_interrupt == 20u && cpu->w[15] == 0x5004u &&
               dspic33_read_word(cpu, 0x5000u) == 0x206u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x1000u) != 0u && cpu->repeat_active == 0u &&
               cpu->rcount == 2u && (cpu->sr & 0x00f0u) == 0x0080u && cpu->cycles == 12u,
           "integrated interrupt suspends repeat at target");
    dspic33_write_word(cpu, 0x0802u, 0u);
    dspic33_write_word(cpu, 0x0822u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x206u && cpu->w[15] == 0x5000u &&
               cpu->repeat_active != 0u && cpu->repeat_pc == 0x206u && cpu->rcount == 2u &&
               (cpu->sr & 0x0010u) != 0u && cpu->cycles == 18u,
           "integrated RETFIE restores repeat state in six cycles");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 1u && cpu->rcount == 1u &&
               cpu->pc == 0x206u && cpu->cycles == 19u,
           "restored repeat executes first target");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 2u && cpu->rcount == 0u &&
               cpu->pc == 0x206u && cpu->cycles == 20u,
           "restored repeat executes second target");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 3u && cpu->repeat_active == 0u &&
               cpu->pc == 0x208u && (cpu->sr & 0x0010u) == 0u && cpu->cycles == 21u,
           "restored repeat completes all targets");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_DISI_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_IFS1);
    load_instruction(state, cpu, 0x204u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x206u, OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x00003cu, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000302u, OPCODE_CLEAR_RCOUNT);
    load_instruction(state, cpu, 0x000304u, OPCODE_RETFIE);
    cpu->w[1] = 0x0010u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0822u, 0x0010u);
    dspic33_write_word(cpu, 0x084au, 0x0004u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "initialize early-termination interrupt window");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "raise early-termination interrupt with word write");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "arm repeat before early-termination interrupt");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000302u && cpu->w[15] == 0x5004u &&
               cpu->rcount == 2u && cpu->repeat_active == 0u && (cpu->sr & 0x00f0u) == 0x0080u &&
               cpu->cycles == 12u,
           "early-termination handler observes suspended repeat");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000304u && cpu->rcount == 0u &&
               cpu->cycles == 13u,
           "handler clears suspended RCOUNT");
    dspic33_write_word(cpu, 0x0802u, 0u);
    dspic33_write_word(cpu, 0x0822u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x206u && cpu->w[15] == 0x5000u &&
               cpu->repeat_active != 0u && cpu->repeat_pc == 0x206u && cpu->rcount == 0u &&
               (cpu->sr & 0x0010u) != 0u && cpu->cycles == 19u,
           "RETFIE restores prefetched target after RCOUNT clear");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 1u && cpu->pc == 0x208u &&
               cpu->rcount == 0u && cpu->repeat_active == 0u && (cpu->sr & 0x0010u) == 0u &&
               cpu->cycles == 20u,
           "cleared repeat executes final prefetched target once");
}
