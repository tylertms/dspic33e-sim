#include "architecture/dspic33/exceptions/fault/internal.h"

static void compare_skip_truth_case(TestState* state, Dspic33* cpu, uint32_t opcode, uint16_t left,
                                    uint16_t right, bool taken, const char* name) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, opcode | 1u);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    cpu->w[0] = left;
    cpu->w[1] = right;
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == (taken ? 4u : 2u) &&
               cpu->cycles == (taken ? 2u : 1u) && cpu->w[0] == left && cpu->w[1] == right &&
               cpu->sr == 0x010fu,
           name);
}

void dspic33_fault_test_compare_skip_cases(TestState* state, Dspic33* cpu) {
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ, 0x1234u, 0x1234u, true,
                            "CPSEQ word takes equal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ, 0x1234u, 0x4321u, false,
                            "CPSEQ word rejects unequal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ | OPCODE_COMPARE_SKIP_BYTE, 0x80ffu, 0x7fffu,
                            true, "CPSEQ byte ignores high-byte difference");
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ | OPCODE_COMPARE_SKIP_BYTE, 0x8000u, 0x0001u,
                            false, "CPSEQ byte rejects unequal low bytes");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE, 0x1234u, 0x4321u, true,
                            "CPSNE word takes unequal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE, 0x1234u, 0x1234u, false,
                            "CPSNE word rejects equal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE | OPCODE_COMPARE_SKIP_BYTE, 0x80ffu, 0x7fffu,
                            false, "CPSNE byte ignores high-byte difference");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE | OPCODE_COMPARE_SKIP_BYTE, 0x8000u, 0x0001u,
                            true, "CPSNE byte takes unequal low bytes");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT, 0x0001u, 0xffffu, true,
                            "CPSGT word uses signed greater-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT, 0xffffu, 0x0001u, false,
                            "CPSGT word rejects signed reverse order");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT | OPCODE_COMPARE_SKIP_BYTE, 0x007fu, 0x0080u,
                            true, "CPSGT byte uses signed greater-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT | OPCODE_COMPARE_SKIP_BYTE, 0x0080u, 0x007fu,
                            false, "CPSGT byte rejects signed reverse order");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT, 0xffffu, 0x0001u, true,
                            "CPSLT word uses signed less-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT, 0x0001u, 0xffffu, false,
                            "CPSLT word rejects signed reverse order");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT | OPCODE_COMPARE_SKIP_BYTE, 0x0080u, 0x007fu,
                            true, "CPSLT byte uses signed less-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT | OPCODE_COMPARE_SKIP_BYTE, 0x007fu, 0x0080u,
                            false, "CPSLT byte rejects signed reverse order");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CPSEQ | 1u);
    load_instruction(state, cpu, 2u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 4u, 0u);
    cpu->w[0] = 0x55aau;
    cpu->w[1] = 0x55aau;
    cpu->w[15] = 0x5000u;
    cpu->sr = 0x010fu;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 6u && cpu->cycles == 3u &&
               cpu->w[15] == 0x5000u && cpu->call_depth == 0u &&
               dspic33_read_word(cpu, 0x5000u) == 0xa5a5u && cpu->sr == 0x010fu,
           "CPSEQ skips complete two-word instruction in three cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CPSEQ | (15u << 11u) | 14u);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    cpu->w[14] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->w[15] == 0x5000u &&
               cpu->call_depth == 0u && cpu->illegal_reset_count == 0u,
           "CPSEQ treats W15 as comparison data without pointer side effects");

    reset_processor_test(cpu, 0x557fcu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557fcu;
    load_instruction(state, cpu, 0x557fcu, OPCODE_CPSEQ | 2u);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    cpu->w[0] = 0x1234u;
    cpu->w[2] = 0x1234u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x340u && cpu->cycles == 2u &&
               cpu->last_trap == 1u && cpu->last_trap_return == 0x557feu && cpu->w[15] == 0x5004u,
           "CPSEQ boundary skip traps with caller-specific return PC");
    expect(state,
           dspic33_read_word(cpu, 0x5000u) == 0x57feu &&
               dspic33_read_word(cpu, 0x5002u) == 0x0f05u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u && cpu->sr == 0x01cfu &&
               cpu->corcon == 0x0028u,
           "CPSEQ boundary trap preserves exact frame and priority state");

    reset_processor_test(cpu, 0x557fau);
    load_instruction(state, cpu, 0x557fau, OPCODE_CPSEQ | 2u);
    load_instruction(state, cpu, 0x557fcu, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x557feu, 0u);
    cpu->w[0] = 0x55aau;
    cpu->w[2] = 0x55aau;
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 3u && cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->w[15] == 0x5000u && cpu->call_depth == 0u,
           "CPSEQ two-word boundary skip establishes sequential provenance");
    load_instruction(state, cpu, 0x000014u, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->interrupt_log_entry[0] == DSPIC33_PROGRAM_LIMIT &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 13u,
           "interrupt clears CPSEQ two-word skip provenance");

    reset_processor_test(cpu, 0x557feu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557feu;
    load_instruction(state, cpu, 0x557feu, OPCODE_CPSEQ | 1u);
    cpu->w[0] = 0x1234u;
    cpu->w[1] = 0x1234u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 2u &&
               cpu->last_trap_return == DSPIC33_PROGRAM_LIMIT &&
               dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0f05u,
           "taken last-word CPSEQ traps as a two-cycle one-word skip");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_CPSEQ | 1u);
    cpu->w[0] = 0u;
    cpu->w[1] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 1u && cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT,
           "untaken last-word CPSEQ enters the hole sequentially in one cycle");

    reset_processor_test(cpu, 0x557feu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557feu;
    load_instruction(state, cpu, 0x557feu, OPCODE_BTSS_W2_BIT_0);
    cpu->w[2] = 1u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 2u &&
               cpu->last_trap_return == DSPIC33_PROGRAM_LIMIT &&
               dspic33_read_word(cpu, 0x5000u) == 0x5800u,
           "taken last-word BTSS shares the two-cycle Address Error fix");

    reset_processor_test(cpu, 0x557feu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557feu;
    load_instruction(state, cpu, 0x557feu, OPCODE_BTSC_W4_POST_INCREMENT_BIT_0);
    dspic33_set_working_register(cpu, 4u, 0x1001u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 1u &&
               cpu->last_trap_return == DSPIC33_PROGRAM_LIMIT && cpu->w[4] == 0x1001u &&
               dspic33_read_word(cpu, 0x5000u) == 0x5800u,
           "last-word indirect BTSC operand fault remains a one-cycle Address Error");
}

void dspic33_fault_test_compare_branch_target_cases(TestState* state, Dspic33* cpu) {
    const uint32_t opcode = 0xe781e1u;

    reset_processor_test(cpu, 0x557c2u);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557c2u;
    load_instruction(state, cpu, 0x557c2u, opcode);
    cpu->w[0] = 0x5a5au;
    cpu->w[1] = 0x5a5au;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x340u && cpu->cycles == 5u &&
               cpu->last_trap == 1u && cpu->last_trap_return == 0x557c4u && cpu->w[0] == 0x5a5au &&
               cpu->w[1] == 0x5a5au,
           "taken compare branch validates an unimplemented target");

    reset_processor_test(cpu, 0x557c2u);
    load_instruction(state, cpu, 0x557c2u, opcode);
    cpu->w[0] = 0x5a5au;
    cpu->w[1] = 0xa5a5u;
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557c4u && cpu->cycles == 1u &&
               cpu->last_trap == UINT16_MAX && cpu->sr == 0x010fu,
           "untaken compare branch does not validate its encoded target");
}

void dspic33_fault_test_prepare_timer_source(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x0110u, 0x0008u);
    dspic33_write_word(cpu, 0x0108u, 0x5555u);
    dspic33_write_word(cpu, 0x0106u, 0x1234u);
    dspic33_write_word(cpu, 0x0108u, 0xaaaau);
}

static void completed_source_address_error_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                                uint16_t stacked_flags, const char* execution,
                                                const char* completion) {
    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, opcode);
    dspic33_fault_test_prepare_timer_source(cpu);
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

void dspic33_fault_test_address_error_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
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

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xbeefu;
    expect_address_trap(state, cpu, "odd post-increment word read traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0xbeefu,
           "odd word read preserves pointer and destination");

    reset_processor_test(cpu, 0u);
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

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xbeefu;
    expect_address_trap(state, cpu, "odd pre-increment word read traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0xbeefu,
           "odd pre-increment read inhibits address update");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_DECREMENT);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd post-decrement word write traps");
    expect(state, cpu->w[1] == 0x1001u, "odd post-decrement write inhibits address update");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    cpu->w[1] = 0x1003u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd pre-decrement word write traps");
    expect(state, cpu->w[1] == 0x1003u, "odd pre-decrement write inhibits address update");

    reset_processor_test(cpu, 0u);
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

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0x1122u;
    cpu->w[3] = 0x3344u;
    expect_address_trap(state, cpu, "odd MOV.D source traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0x1122u && cpu->w[3] == 0x3344u,
           "odd MOV.D preserves destination and pointer");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_BSET_WORD_W4_POST_INCREMENT);
    dspic33_write_word(cpu, 0x1000u, 0x1111u);
    cpu->w[4] = 0x1001u;
    expect_address_trap(state, cpu, "odd indirect word bit operation traps");
    expect(state, cpu->w[4] == 0x1001u && dspic33_read_word(cpu, 0x1000u) == 0x1111u,
           "odd indirect bit operation inhibits data and address update");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BSET_BYTE_W4_POST_DECREMENT);
    dspic33_write_byte(cpu, 0x1001u, 0x11u);
    cpu->w[4] = 0x1001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "odd indirect byte bit operation remains valid");
    expect(state, cpu->w[4] == 0x1000u && dspic33_read_byte(cpu, 0x1001u) == 0x91u,
           "odd indirect byte bit operation updates data and pointer");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_write_byte(cpu, 0x1001u, 0x5au);
    cpu->w[1] = 0x1001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "odd ordinary byte access remains valid");
    expect(state, cpu->w[1] == 0x1002u && (cpu->w[2] & 0x00ffu) == 0x005au,
           "odd ordinary byte access reads and updates pointer");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x0200u, 0xabcdefu);
    cpu->tblpag = 0u;
    cpu->w[2] = 0x0201u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "odd table pointer remains valid");
    expect(state, cpu->w[2] == 0x0201u && cpu->w[3] == 0xcdefu,
           "table word read masks table pointer bit zero");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) == 0u,
           "table pointer does not set ADDRERR");
}

void dspic33_fault_test_data_map_address_error_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xdffeu, 0xa5a5u);
    cpu->w[1] = 0xdffeu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "last implemented word read completes");
    expect(state, cpu->w[1] == 0xe000u && cpu->w[2] == 0xa5a5u,
           "last implemented word read updates result and pointer");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    cpu->w[1] = 0xdffeu;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "last implemented word write completes");
    expect(state, cpu->w[1] == 0xe000u && dspic33_read_word(cpu, 0xdffeu) == 0x5a5au,
           "last implemented word write updates memory and pointer");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xe000u, 0xbeefu);
    cpu->w[1] = 0xe000u;
    cpu->w[2] = 0x5a5au;
    expect_address_trap(state, cpu, "first unimplemented word read traps");
    expect(state,
           cpu->w[1] == 0xe002u && cpu->w[2] == 0u && dspic33_read_word(cpu, 0xe000u) == 0xbeefu,
           "unimplemented read returns zero and preserves raw backing");

    reset_processor_test(cpu, 0u);
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

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xfffeu, 0xbeefu);
    cpu->w[1] = 0xfffeu;
    cpu->w[2] = 0x5a5au;
    expect_address_trap(state, cpu, "last unimplemented word read traps");
    expect(state,
           cpu->w[1] == 0x8000u && cpu->w[2] == 0u && cpu->dsrpag == 2u && cpu->dswpag == 1u &&
               dspic33_read_word(cpu, 0xfffeu) == 0xbeefu,
           "unimplemented read advances EDS read page only");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0xfffeu, 0xbeefu);
    cpu->w[1] = 0xfffeu;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "last unimplemented word write traps");
    expect(state,
           cpu->w[1] == 0x8000u && cpu->w[2] == 0xa5a5u && cpu->dsrpag == 1u && cpu->dswpag == 2u &&
               dspic33_read_word(cpu, 0xfffeu) == 0xbeefu,
           "unimplemented write advances EDS write page only");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_write_byte(cpu, 0xe000u, 0x5au);
    cpu->w[1] = 0xe000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented byte read traps");
    expect(state,
           cpu->w[1] == 0xe001u && cpu->w[2] == 0xa500u && dspic33_read_byte(cpu, 0xe000u) == 0x5au,
           "unimplemented byte read returns zero and updates pointer");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x0056u;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "unused SFR hole read remains valid");
    expect(state, cpu->w[1] == 0x0058u && cpu->w[2] == 0u,
           "unused SFR hole reads zero and updates pointer");

    reset_processor_test(cpu, 0u);
    cpu->instruction_active = true;
    cpu->io.dma_transfer_active = true;
    dspic33_write_word(cpu, 0xe000u, 0x1234u);
    expect(state, dspic33_read_word(cpu, 0xe000u) == 0x1234u && !cpu->address_error,
           "DMA raw access bypasses CPU data map trap");
    cpu->instruction_active = false;
    cpu->io.dma_transfer_active = false;
}

void dspic33_fault_test_pseudo_linear_page_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_PRE_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x8000u, 0x00005au),
           "load pre-increment PSV byte");
    cpu->w[1] = 0xffffu;
    cpu->w[2] = 0x1200u;
    cpu->dsrpag = 0x0200u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute byte pre-increment page transition");
    expect(state, cpu->w[1] == 0x8000u && cpu->w[2] == 0x125au && cpu->dsrpag == 0x0201u,
           "byte pre-increment reads the new PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x00a500u),
           "load post-increment PSV byte");
    cpu->w[1] = 0xffffu;
    cpu->w[2] = 0x1200u;
    cpu->dsrpag = 0x0200u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute byte post-increment page transition");
    expect(state, cpu->w[1] == 0x8000u && cpu->w[2] == 0x12a5u && cpu->dsrpag == 0x0201u,
           "byte post-increment reads the original PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_PRE_DECREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x00a500u),
           "load pre-decrement PSV byte");
    cpu->w[1] = 0x8000u;
    cpu->w[2] = 0x1200u;
    cpu->dsrpag = 0x0201u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute byte pre-decrement page transition");
    expect(state, cpu->w[1] == 0xffffu && cpu->w[2] == 0x12a5u && cpu->dsrpag == 0x0200u,
           "byte pre-decrement reads the new PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_DECREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x8000u, 0x00005au),
           "load post-decrement PSV byte");
    cpu->w[1] = 0x8000u;
    cpu->w[2] = 0x1200u;
    cpu->dsrpag = 0x0201u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute byte post-decrement page transition");
    expect(state, cpu->w[1] == 0xffffu && cpu->w[2] == 0x125au && cpu->dsrpag == 0x0200u,
           "byte post-decrement reads the original PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x8000u, 0x005566u),
           "load word pre-increment PSV value");
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u && cpu->w[2] == 0x5566u &&
               cpu->dsrpag == 0x0201u,
           "word pre-increment reads after the page transition");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x002233u),
           "load word post-increment PSV value");
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u && cpu->w[2] == 0x2233u &&
               cpu->dsrpag == 0x0201u,
           "word post-increment reads before the page transition");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[0] = 0x1111u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x01ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0u && cpu->w[2] == 0x1111u &&
               cpu->dsrpag == 0x01ffu,
           "last EDS read page overflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[0] = 0x2222u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x03ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0u && cpu->w[2] == 0x2222u &&
               cpu->dsrpag == 0x03ffu,
           "last PSV read page overflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[0] = 0x5555u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0u && cpu->w[2] == 0x5555u &&
               cpu->dsrpag == 0u && !cpu->address_error,
           "page zero pre-increment wraps into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_DECREMENT_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0x3333u);
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0001u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu && cpu->w[2] == 0x3333u &&
               cpu->dsrpag == 0x0001u,
           "first EDS read page underflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_DECREMENT_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0x4444u);
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu && cpu->w[2] == 0x4444u &&
               cpu->dsrpag == 0x0200u,
           "first PSV read page underflows into base data space");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0u, 0x5a0000u),
           "load high-byte PSV transition value");
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x02ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u && cpu->w[2] == 0x005au &&
               cpu->dsrpag == 0x0300u,
           "PSV low-word page transitions into high-byte page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_DECREMENT_W2);
    load_instruction(state, cpu, 0x7ffffeu, 0x005a5au);
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0300u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xfffeu && cpu->w[2] == 0x5a5au &&
               cpu->dsrpag == 0x02ffu,
           "PSV high-byte page underflows into low-word page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_INCREMENT);
    cpu->w[1] = 0xfffeu;
    cpu->w[2] = 0x6666u;
    cpu->dswpag = 0x01ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x6666u && cpu->w[1] == 0u &&
               cpu->dswpag == 0x01ffu,
           "last EDS write page overflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    dspic33_write_word(cpu, 0x7ffeu, 0xaaaau);
    cpu->w[1] = 0x8000u;
    cpu->w[2] = 0x7777u;
    cpu->dswpag = 0x0001u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu && cpu->dswpag == 0x0001u &&
               dspic33_read_word(cpu, 0x7ffeu) == 0x7777u,
           "first EDS write page underflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x006666u),
           "load modulo boundary PSV value");
    dspic33_write_word(cpu, 0x0048u, 0xfff8u);
    dspic33_write_word(cpu, 0x004au, 0xffffu);
    dspic33_write_word(cpu, 0x0046u, 0x8001u);
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xfff8u && cpu->w[2] == 0x6666u &&
               cpu->dsrpag == 0x0200u,
           "modulo wrap leaves the PSV page unchanged");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W1_W0_OFFSET_W2);
    expect(state, dspic33_load_program_word(cpu, 0u, 0x00abcdu), "load indexed wrap PSV value");
    cpu->w[0] = 2u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xfffeu && cpu->w[2] == 0xabcdu &&
               cpu->dsrpag == 0x0200u,
           "indexed overflow wraps within the current PSV page");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W1_W0_OFFSET_W2);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x00bcdeu),
           "load indexed underflow PSV value");
    cpu->w[0] = 0xfffeu;
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u && cpu->w[2] == 0xbcdeu &&
               cpu->dsrpag == 0x0200u,
           "indexed underflow wraps within the current PSV page");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W15_W0_OFFSET_W2);
    cpu->w[0] = 2u;
    cpu->w[15] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0xfffeu && cpu->w[2] == 2u &&
               cpu->dsrpag == 0x0200u,
           "indexed W15 overflow remains in base data space");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W15_W0_OFFSET_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0xdef0u);
    cpu->w[0] = 0xfffeu;
    cpu->w[15] = 0x8000u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0x8000u && cpu->w[2] == 0xdef0u &&
               cpu->dsrpag == 0x0200u,
           "indexed W15 underflow remains in base data space");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    cpu->w[1] = 0x8000u;
    cpu->w[2] = 0xa5a5u;
    cpu->dswpag = 0x0002u;
    expect_address_trap(state, cpu, "pre-decrement write page transition traps");
    expect(state, cpu->w[1] == 0xfffeu && cpu->w[2] == 0xa5a5u && cpu->dswpag == 0x0001u,
           "write page transition completes pointer and DSWPAG before trap");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_PRE_INCREMENT_W2);
    expect(state,
           dspic33_load_program_word(cpu, 0x8000u, 0x005566u) &&
               dspic33_load_program_word(cpu, 0x8002u, 0x007788u),
           "load pre-increment MOV.D PSV values");
    cpu->w[1] = 0xfffcu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u && cpu->w[2] == 0x5566u &&
               cpu->w[3] == 0x7788u && cpu->dsrpag == 0x0201u && !cpu->address_error,
           "MOV.D pre-increment reads both words from the new page");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    expect(state,
           dspic33_load_program_word(cpu, 0x7ffeu, 0x002233u) &&
               dspic33_load_program_word(cpu, 0x8000u, 0x005566u),
           "load post-increment MOV.D straddle values");
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect_address_trap(state, cpu, "MOV.D post-increment page straddle traps");
    expect(state,
           cpu->w[1] == 0x8002u && cpu->w[2] == 0x2233u && cpu->w[3] == 0u &&
               cpu->dsrpag == 0x0201u,
           "MOV.D straddle completes low word and page state before trap");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_DOUBLE_W1_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0x1111u);
    expect(state, dspic33_load_program_word(cpu, 0u, 0x002222u),
           "load split base and PSV MOV.D values");
    cpu->w[1] = 0x7ffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu && cpu->w[2] == 0x1111u &&
               cpu->w[3] == 0x2222u && cpu->dsrpag == 0x0200u && !cpu->address_error,
           "MOV.D independently maps the high word into the PSV window");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W1);
    dspic33_write_word(cpu, 0x7ffeu, 0xaaaau);
    dspic33_write_word(cpu, 0x8000u, 0xbbbbu);
    cpu->w[1] = 0x7ffeu;
    cpu->w[2] = 0x1111u;
    cpu->w[3] = 0x2222u;
    cpu->dswpag = 0x0002u;
    expect_address_trap(state, cpu, "split base and EDS MOV.D write traps");
    expect(state,
           cpu->w[1] == 0x7ffeu && cpu->dswpag == 0x0002u &&
               dspic33_read_word(cpu, 0x7ffeu) == 0x1111u &&
               dspic33_read_word(cpu, 0x8000u) == 0xbbbbu,
           "MOV.D completes base low write and inhibits EDS high write");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0x1111u);
    cpu->w[0] = 0xabcdu;
    cpu->w[1] = 0x7ffeu;
    cpu->dsrpag = 0u;
    expect_address_trap(state, cpu, "MOV.D derived page-zero read traps");
    expect(state,
           cpu->w[1] == 0x7ffeu && cpu->w[2] == 0x1111u && cpu->w[3] == 0xabcdu &&
               cpu->dsrpag == 0u,
           "MOV.D page-zero high word completes through base alias");
}
