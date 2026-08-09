#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} ProcessorConformance;

enum {
    OPCODE_NOP = 0x000000u,
    OPCODE_RETURN = 0x060000u,
    OPCODE_RETFIE = 0x064000u,
    OPCODE_RETLW_0X123_W2 = 0x051232u,
    OPCODE_RETLW_0X122_W15 = 0x05122fu,
    OPCODE_CALL_0X100 = 0x020100u,
    OPCODE_CALL_W0 = 0x010000u,
    OPCODE_CALL_LONG_W0 = 0x018800u,
    OPCODE_RCALL_W0 = 0x010200u,
    OPCODE_RCALL_NEXT = 0x070000u,
    OPCODE_SFTAC_A_W5 = 0xc80005u,
    OPCODE_LNK_0 = 0xfa0000u,
    OPCODE_ULNK = 0xfa8000u,
    OPCODE_MOV_ODD_W15 = 0x25001fu,
    OPCODE_MOV_W0_W15 = 0x780780u,
    OPCODE_MOV_BYTE_W0_W15 = 0x784780u,
    OPCODE_MOV_BYTE_W15_POST_INCREMENT_W0 = 0x78403fu,
    OPCODE_MOV_BYTE_W15_PRE_INCREMENT_W1 = 0x7840dfu,
    OPCODE_MOV_BYTE_W15_POST_DECREMENT_W2 = 0x78412fu,
    OPCODE_MOV_BYTE_W15_PRE_DECREMENT_W3 = 0x7841cfu,
    OPCODE_MOV_DOUBLE_W14_W2 = 0xbe010eu,
    OPCODE_MOV_DOUBLE_W15_W2 = 0xbe011fu,
    OPCODE_MOV_W2_W1_POST_INCREMENT = 0x781882u,
    OPCODE_MOV_W1_POST_INCREMENT_W2 = 0x780131u,
    OPCODE_MOV_W1_PRE_INCREMENT_W2 = 0x780151u,
    OPCODE_MOV_W2_W1_POST_DECREMENT = 0x781082u,
    OPCODE_MOV_W2_W1_PRE_DECREMENT = 0x782082u,
    OPCODE_MOV_W1_MEMORY_W2_MEMORY = 0x780911u,
    OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2 = 0x784131u,
    OPCODE_MOV_DOUBLE_W2_W1_POST_INCREMENT = 0xbe9882u,
    OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2 = 0xbe0131u,
    OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT = 0x4112b4u,
    OPCODE_NEG_W4_POST_INCREMENT_W5_POST_DECREMENT = 0xea12b4u,
    OPCODE_ASR_W4_POST_INCREMENT_W5_POST_DECREMENT = 0xd192b4u,
    OPCODE_BSET_BYTE_W4_POST_DECREMENT = 0xa07424u,
    OPCODE_BSET_WORD_W4_POST_INCREMENT = 0xa09034u,
    OPCODE_TBLRDL_W2_W3 = 0xba0192u,
    OPCODE_MOV_W0_SPLIM = 0x880100u,
    OPCODE_MOV_SENTINEL_W1 = 0x211111u
};

static void expect(ProcessorConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[processor-failed] %s\n", name);
    }
}

static void load_instruction(ProcessorConformance* state, Dspic33* cpu,
                             uint32_t address, uint32_t opcode) {
    expect(state, dspic33_load_program_word(cpu, address, opcode),
           "load processor instruction");
}

static void prepare_trap_vectors(ProcessorConformance* state, Dspic33* cpu) {
    load_instruction(state, cpu, 0x00000au, 0x000100u);
    load_instruction(state, cpu, 0x00000cu, 0x000120u);
    load_instruction(state, cpu, 0x000100u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000120u, OPCODE_NOP);
}

static void prepare_address_trap(ProcessorConformance* state, Dspic33* cpu) {
    load_instruction(state, cpu, 0x000006u, 0x000140u);
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
}

static void expect_address_trap(ProcessorConformance* state, Dspic33* cpu,
                                const char* execution) {
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED, execution);
    expect(state,
           cpu->last_trap == 1u && cpu->last_trap_return == 2u && cpu->pc == 0x000140u,
           "address error enters hard trap");
    expect(state,
           (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u,
           "address error records status and priority");
}

static void prepare_timer_source(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x0110u, 0x0008u);
    dspic33_write_word(cpu, 0x0108u, 0x5555u);
    dspic33_write_word(cpu, 0x0106u, 0x1234u);
    dspic33_write_word(cpu, 0x0108u, 0xaaaau);
}

static void completed_source_address_error_case(ProcessorConformance* state,
                                                Dspic33* cpu, uint32_t opcode,
                                                uint16_t stacked_flags,
                                                const char* execution,
                                                const char* completion) {
    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, opcode);
    prepare_timer_source(cpu);
    dspic33_write_word(cpu, 0x1000u, 0x1122u);
    dspic33_write_word(cpu, 0x1002u, 0x3344u);
    cpu->w[2] = 2u;
    cpu->w[4] = 0x0106u;
    cpu->w[5] = 0x1001u;
    expect_address_trap(state, cpu, execution);
    expect(state,
           dspic33_read_word(cpu, 0x0108u) == 0x5555u && cpu->w[4] == 0x0108u &&
               cpu->w[5] == 0x1001u && dspic33_read_word(cpu, 0x1000u) == 0x1122u &&
               dspic33_read_word(cpu, 0x1002u) == 0x3344u &&
               (dspic33_read_word(cpu, 0x5002u) & 0xff00u) == stacked_flags,
           completion);
}

static void address_error_cases(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x1000u, 0x1122u);
    dspic33_write_word(cpu, 0x1002u, 0x3344u);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd post-increment word write traps");
    expect(state,
           cpu->w[1] == 0x1001u && dspic33_read_word(cpu, 0x1000u) == 0x1122u &&
               dspic33_read_word(cpu, 0x1002u) == 0x3344u,
           "odd word write inhibits data and address update");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xbeefu;
    expect_address_trap(state, cpu, "odd post-increment word read traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0xbeefu,
           "odd word read preserves pointer and destination");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_MEMORY_W2_MEMORY);
    dspic33_write_word(cpu, 0x0108u, 0xaaaau);
    dspic33_write_word(cpu, 0x010au, 0x5555u);
    dspic33_write_word(cpu, 0x0110u, 0x0008u);
    cpu->w[1] = 0x0106u;
    cpu->w[2] = 0x1001u;
    expect_address_trap(state, cpu, "odd destination prevalidates before source read");
    expect(state, dspic33_read_word(cpu, 0x0108u) == 0xaaaau,
           "odd destination inhibits side-effecting timer source read");

    completed_source_address_error_case(
        state, cpu, OPCODE_ASR_W4_POST_INCREMENT_W5_POST_DECREMENT, 0x0000u,
        "odd shift destination traps after source execution",
        "shift source read update and flags complete before destination trap");
    completed_source_address_error_case(
        state, cpu, OPCODE_NEG_W4_POST_INCREMENT_W5_POST_DECREMENT, 0x0800u,
        "odd unary destination traps after source execution",
        "unary source read update and flags complete before destination trap");
    completed_source_address_error_case(
        state, cpu, OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT, 0x0000u,
        "odd binary destination traps after source execution",
        "binary source read update and flags complete before destination trap");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xbeefu;
    expect_address_trap(state, cpu, "odd pre-increment word read traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0xbeefu,
           "odd pre-increment read inhibits address update");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_DECREMENT);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd post-decrement word write traps");
    expect(state, cpu->w[1] == 0x1001u,
           "odd post-decrement write inhibits address update");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    cpu->w[1] = 0x1003u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd pre-decrement word write traps");
    expect(state, cpu->w[1] == 0x1003u,
           "odd pre-decrement write inhibits address update");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x1000u, 0x1122u);
    dspic33_write_word(cpu, 0x1002u, 0x3344u);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xa5a5u;
    cpu->w[3] = 0x5a5au;
    expect_address_trap(state, cpu, "odd MOV.D destination traps");
    expect(state,
           cpu->w[1] == 0x1001u && dspic33_read_word(cpu, 0x1000u) == 0x1122u &&
               dspic33_read_word(cpu, 0x1002u) == 0x3344u,
           "odd MOV.D inhibits both writes and pointer update");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0x1122u;
    cpu->w[3] = 0x3344u;
    expect_address_trap(state, cpu, "odd MOV.D source traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0x1122u && cpu->w[3] == 0x3344u,
           "odd MOV.D preserves destination and pointer");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_BSET_WORD_W4_POST_INCREMENT);
    dspic33_write_word(cpu, 0x1000u, 0x1111u);
    cpu->w[4] = 0x1001u;
    expect_address_trap(state, cpu, "odd indirect word bit operation traps");
    expect(state, cpu->w[4] == 0x1001u && dspic33_read_word(cpu, 0x1000u) == 0x1111u,
           "odd indirect bit operation inhibits data and address update");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BSET_BYTE_W4_POST_DECREMENT);
    dspic33_write_byte(cpu, 0x1001u, 0x11u);
    cpu->w[4] = 0x1001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "odd indirect byte bit operation remains valid");
    expect(state, cpu->w[4] == 0x1000u && dspic33_read_byte(cpu, 0x1001u) == 0x91u,
           "odd indirect byte bit operation updates data and pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_write_byte(cpu, 0x1001u, 0x5au);
    cpu->w[1] = 0x1001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "odd ordinary byte access remains valid");
    expect(state, cpu->w[1] == 0x1002u && (cpu->w[2] & 0x00ffu) == 0x005au,
           "odd ordinary byte access reads and updates pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x0200u, 0xabcdefu);
    cpu->tblpag = 0u;
    cpu->w[2] = 0x0201u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "odd table pointer remains valid");
    expect(state, cpu->w[2] == 0x0201u && cpu->w[3] == 0xcdefu,
           "table word read masks table pointer bit zero");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) == 0u,
           "table pointer does not set ADDRERR");
}

static void data_map_address_error_cases(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xdffeu, 0xa5a5u);
    cpu->w[1] = 0xdffeu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "last implemented word read completes");
    expect(state, cpu->w[1] == 0xe000u && cpu->w[2] == 0xa5a5u,
           "last implemented word read updates result and pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    cpu->w[1] = 0xdffeu;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "last implemented word write completes");
    expect(state, cpu->w[1] == 0xe000u && dspic33_read_word(cpu, 0xdffeu) == 0x5a5au,
           "last implemented word write updates memory and pointer");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xe000u, 0xbeefu);
    cpu->w[1] = 0xe000u;
    cpu->w[2] = 0x5a5au;
    expect_address_trap(state, cpu, "first unimplemented word read traps");
    expect(state,
           cpu->w[1] == 0xe002u && cpu->w[2] == 0u &&
               dspic33_read_word(cpu, 0xe000u) == 0xbeefu,
           "unimplemented read returns zero and preserves raw backing");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0xe000u, 0xbeefu);
    cpu->w[1] = 0xe000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "first unimplemented word write traps");
    expect(state,
           cpu->w[1] == 0xe002u && cpu->w[2] == 0xa5a5u &&
               dspic33_read_word(cpu, 0xe000u) == 0xbeefu,
           "unimplemented write preserves source and raw backing");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xfffeu, 0xbeefu);
    cpu->w[1] = 0xfffeu;
    cpu->w[2] = 0x5a5au;
    expect_address_trap(state, cpu, "last unimplemented word read traps");
    expect(state,
           cpu->w[1] == 0x8000u && cpu->w[2] == 0u && cpu->dsrpag == 2u &&
               cpu->dswpag == 1u && dspic33_read_word(cpu, 0xfffeu) == 0xbeefu,
           "unimplemented read advances EDS read page only");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0xfffeu, 0xbeefu);
    cpu->w[1] = 0xfffeu;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "last unimplemented word write traps");
    expect(state,
           cpu->w[1] == 0x8000u && cpu->w[2] == 0xa5a5u && cpu->dsrpag == 1u &&
               cpu->dswpag == 2u && dspic33_read_word(cpu, 0xfffeu) == 0xbeefu,
           "unimplemented write advances EDS write page only");

    dspic33_reset(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_write_byte(cpu, 0xe000u, 0x5au);
    cpu->w[1] = 0xe000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented byte read traps");
    expect(state,
           cpu->w[1] == 0xe001u && cpu->w[2] == 0xa500u &&
               dspic33_read_byte(cpu, 0xe000u) == 0x5au,
           "unimplemented byte read returns zero and updates pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x0056u;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "unused SFR hole read remains valid");
    expect(state, cpu->w[1] == 0x0058u && cpu->w[2] == 0u,
           "unused SFR hole reads zero and updates pointer");

    dspic33_reset(cpu, 0u);
    cpu->instruction_active = true;
    cpu->io.dma_transfer_active = true;
    dspic33_write_word(cpu, 0xe000u, 0x1234u);
    expect(state, dspic33_read_word(cpu, 0xe000u) == 0x1234u && !cpu->address_error,
           "DMA raw access bypasses CPU data map trap");
    cpu->instruction_active = false;
    cpu->io.dma_transfer_active = false;
}

static uint16_t active_pending_traps(const Dspic33* cpu) {
    uint16_t count = 0u;
    size_t index;
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active) {
            count++;
        }
    }
    return count;
}

static const Dspic33PendingSoftTrap* pending_trap(const Dspic33* cpu, uint16_t trap) {
    size_t index;
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active &&
            cpu->pending_soft_traps[index].trap == trap) {
            return &cpu->pending_soft_traps[index];
        }
    }
    return NULL;
}

static void w15_write_cases(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_ODD_W15);
    load_instruction(state, cpu, 2u, OPCODE_MOV_W0_W15);
    load_instruction(state, cpu, 4u, OPCODE_MOV_BYTE_W0_W15);
    load_instruction(state, cpu, 6u, OPCODE_MOV_BYTE_W15_POST_INCREMENT_W0);
    load_instruction(state, cpu, 8u, OPCODE_MOV_BYTE_W15_PRE_INCREMENT_W1);
    load_instruction(state, cpu, 10u, OPCODE_MOV_BYTE_W15_POST_DECREMENT_W2);
    load_instruction(state, cpu, 12u, OPCODE_MOV_BYTE_W15_PRE_DECREMENT_W3);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute odd W15 literal");
    expect(state, cpu->w[15] == 0x5000u, "literal write clears W15 low bit");
    cpu->w[0] = 0x5001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute odd W15 register move");
    expect(state, cpu->w[15] == 0x5000u, "register move clears W15 low bit");
    cpu->w[0] = 0x0001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute odd W15 byte register move");
    expect(state, cpu->w[15] == 0x5000u, "byte register move clears W15 low bit");
    dspic33_write_byte(cpu, 0x001eu, 0x01u);
    expect(state, cpu->w[15] == 0x5000u, "byte alias write clears W15 low bit");
    dspic33_write_word(cpu, 0x001eu, 0x5001u);
    expect(state, cpu->w[15] == 0x5000u, "word alias write clears W15 low bit");
    dspic33_write_word(cpu, 0x5000u, 0x2211u);
    dspic33_write_word(cpu, 0x5002u, 0x4433u);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute W15 byte post-increment");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[0] & 0x00ffu) == 0x0011u,
           "W15 byte post-increment retains even pointer and old address");
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute W15 byte pre-increment");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[1] & 0x00ffu) == 0x0022u,
           "W15 byte pre-increment uses transient odd address");
    cpu->w[15] = 0x5002u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute W15 byte post-decrement");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[2] & 0x00ffu) == 0x0033u,
           "W15 byte post-decrement retains old address");
    cpu->w[15] = 0x5002u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute W15 byte pre-decrement");
    expect(state, cpu->w[15] == 0x5000u && (cpu->w[3] & 0x00ffu) == 0x0022u,
           "W15 byte pre-decrement uses transient odd address");
}

static void valid_stack_frame_cases(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_LNK_0);
    load_instruction(state, cpu, 2u, OPCODE_ULNK);
    load_instruction(state, cpu, 4u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    cpu->w[14] = 0x4444u;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute valid LNK");
    expect(state,
           cpu->w[14] == 0x5002u && cpu->w[15] == 0x5002u &&
               (cpu->corcon & 0x0004u) != 0u,
           "valid LNK updates frame state");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute valid ULNK");
    expect(state,
           cpu->w[14] == 0x4444u && cpu->w[15] == 0x5000u &&
               (cpu->corcon & 0x0004u) == 0u,
           "valid ULNK restores frame state");
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0u,
           "valid frame operations do not set STKERR");
    expect(state, active_pending_traps(cpu) == 0u,
           "valid frame operations do not schedule traps");
}

static void invalid_lnk_case(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = false;
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_LNK_0);
    load_instruction(state, cpu, 2u, OPCODE_LNK_0);
    load_instruction(state, cpu, 4u, OPCODE_MOV_SENTINEL_W1);
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
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000100u,
           "invalid SFA LNK enters stack trap");
    expect(state, cpu->w[15] == 0x5008u && dspic33_read_word(cpu, 0x5004u) == 0x0007u,
           "invalid SFA LNK stacks completed return state");
}

static void invalid_ulnk_case(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_ULNK);
    load_instruction(state, cpu, 2u, OPCODE_MOV_SENTINEL_W1);
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
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000100u,
           "invalid SFA ULNK enters stack trap");
}

static void simultaneous_trap_case(ProcessorConformance* state, Dspic33* cpu) {
    const Dspic33PendingSoftTrap* pending;
    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = false;
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_SFTAC_A_W5);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    cpu->w[5] = 17u;
    cpu->w[15] = 0x5000u;
    dspic33_check_stack_address(cpu, 0x5102, false, 2u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "schedule simultaneous stack and math traps");
    expect(state, active_pending_traps(cpu) == 2u,
           "simultaneous trap sources remain distinct");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "service simultaneous trap boundary");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000100u,
           "simultaneous trap boundary chooses stack priority");
    pending = pending_trap(cpu, 4u);
    expect(state, pending != NULL && pending->delay == 0u,
           "simultaneous boundary retains ready math trap");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "return from stack trap with retained math trap");
    expect(state, cpu->last_trap == 4u && cpu->pc == 0x000120u && cpu->trap_count == 2u,
           "retained math trap enters after stack RETFIE");
}

static void earlier_deadline_case(ProcessorConformance* state, Dspic33* cpu) {
    const Dspic33PendingSoftTrap* pending;
    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = false;
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_SFTAC_A_W5);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0020u, 0x5100u);
    cpu->w[5] = 17u;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "schedule earlier math trap");
    dspic33_check_stack_address(cpu, 0x5102, false, 2u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "service earlier math deadline");
    expect(state, cpu->last_trap == 4u && cpu->pc == 0x000120u,
           "earlier math deadline precedes later stack priority");
    pending = pending_trap(cpu, 3u);
    expect(state, pending != NULL && pending->delay == 1u,
           "later stack deadline remains pending");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "advance math handler to stack deadline");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000100u && cpu->trap_count == 2u,
           "ready stack trap preempts math handler");
}

static void call_stack_timing_case(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X100);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000100u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x4ffeu);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "CALL stack fault matures within CALL cycles");
    expect(state, cpu->w[1] == 0u, "CALL stack fault precedes target instruction");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000200u,
           "CALL stack fault enters stack trap");
    expect(state, cpu->w[15] == 0x5008u && dspic33_read_word(cpu, 0x5004u) == 0x0100u,
           "CALL stack fault stacks target return PC");
}

static void instruction_cycle_cases(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u,
           "NOP consumes one cycle");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W14_W2);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u,
           "MOV.D consumes two cycles");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X100);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "direct CALL consumes four cycles");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_W0);
    cpu->w[0] = 0x0100u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "CALL Wn consumes four cycles");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RCALL_NEXT);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "literal RCALL consumes four cycles");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RCALL_W0);
    cpu->w[0] = 0x007fu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "RCALL Wn consumes four cycles");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_LONG_W0);
    cpu->w[0] = 0x0100u;
    cpu->w[1] = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "CALL.L consumes four cycles");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETFIE without pending exception consumes six cycles");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000014u, 0x000200u);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    cpu->sr = 0x00e0u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 5u,
           "RETFIE with pending exception consumes five cycles");
}

static void move_double_stack_timing_cases(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W15_W2);
    load_instruction(state, cpu, 2u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x4ffeu);
    dspic33_write_word(cpu, 0x5000u, 0x1122u);
    dspic33_write_word(cpu, 0x5002u, 0x3344u);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "MOV.D stack fault matures within MOV.D cycles");
    expect(state, cpu->w[2] == 0x1122u && cpu->w[3] == 0x3344u,
           "MOV.D stack fault completes both reads");
    expect(state, cpu->w[1] == 0u, "MOV.D stack fault precedes following instruction");
    expect(state, cpu->last_trap_return == 2u, "MOV.D stack fault stacks following PC");

    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W15_W2);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0020u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0x1122u);
    dspic33_write_word(cpu, 0x5002u, 0x3344u);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "MOV.D derived high word does not trigger SPLIM");
    expect(state, cpu->w[2] == 0x1122u && cpu->w[3] == 0x3344u,
           "MOV.D legal base reads derived high word");
    expect(state,
           (dspic33_read_word(cpu, 0x08c0u) & 0x0004u) == 0u &&
               active_pending_traps(cpu) == 0u,
           "MOV.D derived high word leaves STKERR clear");
}

static void return_instruction_cycle_cases(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETURN);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETURN without pending exception consumes six cycles");

    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_RETURN);
    load_instruction(state, cpu, 0x000100u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "RETURN with pending exception consumes five cycles");
    expect(state, cpu->w[1] == 0u && cpu->last_trap_return == 0x000100u,
           "RETURN stack fault precedes restored instruction");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X123_W2);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETLW without pending exception consumes six cycles");
    expect(state, cpu->w[2] == 0x0123u, "RETLW writes return literal");

    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X123_W2);
    load_instruction(state, cpu, 0x000100u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "RETLW with pending exception consumes five cycles");
    expect(state,
           cpu->w[2] == 0x0123u && cpu->w[1] == 0u &&
               cpu->last_trap_return == 0x000100u,
           "RETLW stack fault completes literal and precedes target");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X122_W15);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000100u,
           "RETLW W15 restores PC from original stack frame");
    expect(state, cpu->w[15] == 0x0122u && cpu->cycles == 6u,
           "RETLW W15 writes even literal after frame pop");
}

static void retfie_stack_timing_case(ProcessorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000100u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "RETFIE stack fault matures within RETFIE cycles");
    expect(state, cpu->w[1] == 0u, "RETFIE stack fault precedes restored instruction");
    expect(state, cpu->last_trap_return == 0x000100u && cpu->pc == 0x000200u,
           "RETFIE stack fault stacks restored PC");
}

static void interrupt_stack_timing_case(ProcessorConformance* state, Dspic33* cpu) {
    const Dspic33PendingSoftTrap* pending;
    dspic33_reset(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0x000014u, 0x000100u);
    load_instruction(state, cpu, 0x000100u, OPCODE_MOV_W0_SPLIM);
    load_instruction(state, cpu, 0x000102u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x4ffeu);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    cpu->w[0] = 0x5100u;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "IRQ stack fault allows first handler instruction");
    pending = pending_trap(cpu, 3u);
    expect(state,
           cpu->pc == 0x000102u && cpu->splim == 0x5100u && pending != NULL &&
               pending->delay == 1u,
           "IRQ stack fault retains second handler boundary");
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "IRQ stack fault traps after second handler instruction");
    expect(state, cpu->w[1] == 0x1111u && cpu->last_trap_return == 0x000104u,
           "IRQ stack fault stacks third handler PC");
}

int main(void) {
    ProcessorConformance state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize processor conformance");
    if (initialized) {
        address_error_cases(&state, &cpu);
        data_map_address_error_cases(&state, &cpu);
        w15_write_cases(&state, &cpu);
        valid_stack_frame_cases(&state, &cpu);
        invalid_lnk_case(&state, &cpu);
        invalid_ulnk_case(&state, &cpu);
        simultaneous_trap_case(&state, &cpu);
        earlier_deadline_case(&state, &cpu);
        instruction_cycle_cases(&state, &cpu);
        call_stack_timing_case(&state, &cpu);
        move_double_stack_timing_cases(&state, &cpu);
        return_instruction_cycle_cases(&state, &cpu);
        retfie_stack_timing_case(&state, &cpu);
        interrupt_stack_timing_case(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[processor-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32
           "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
