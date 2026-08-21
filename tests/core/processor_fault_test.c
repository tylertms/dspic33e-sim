#include "processor_test_support.h"

static void program_target_address_error_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_0X55800);
    load_instruction(state, cpu, 2u, 0x000005u);
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u &&
               cpu->sequential_program_hole_pc == 0u,
           "unimplemented literal GOTO target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u &&
               dspic33_read_word(cpu, 0x5000u) == 2u && (cpu->corcon & 0x0004u) != 0u,
           "literal GOTO stacks extension address and preserves SFA");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X55800);
    load_instruction(state, cpu, 2u, 0x000005u);
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented literal CALL target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5008u &&
               dspic33_read_word(cpu, 0x5000u) == 5u && dspic33_read_word(cpu, 0x5004u) == 2u &&
               (cpu->corcon & 0x0004u) == 0u,
           "literal CALL completes return push before target trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_LONG_W0);
    cpu->w[0] = 0x5800u;
    cpu->w[1] = 0x0005u;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented GOTO.L target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u && (cpu->corcon & 0x0004u) != 0u,
           "GOTO.L target trap preserves registers and SFA");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_CALL_LONG_W0);
    cpu->w[0] = 0x5800u;
    cpu->w[1] = 0x0005u;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented CALL.L target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5008u &&
               dspic33_read_word(cpu, 0x5000u) == 3u && (cpu->corcon & 0x0004u) == 0u,
           "CALL.L completes return push before target trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_RETURN);
    dspic33_write_word(cpu, 0x5000u, 0x5800u);
    dspic33_write_word(cpu, 0x5002u, 0x0005u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "unimplemented RETURN target traps in five cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u && cpu->call_depth == 0u &&
               (cpu->corcon & 0x0004u) == 0u,
           "RETURN completes frame pop before target trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    dspic33_write_word(cpu, 0x5000u, 0x5800u);
    dspic33_write_word(cpu, 0x5002u, 0x0f05u);
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "unimplemented RETFIE target traps in five cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u &&
               (dspic33_read_word(cpu, 0x5002u) & 0xff00u) == 0x0f00u,
           "RETFIE restores frame state before target trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X123_W2);
    dspic33_write_word(cpu, 0x5000u, 0x5800u);
    dspic33_write_word(cpu, 0x5002u, 0x0005u);
    cpu->call_depth = 1u;
    cpu->w[2] = 0xa5a5u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "unimplemented RETLW target traps in five cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[2] == 0x0123u && cpu->w[15] == 0x5004u &&
               cpu->call_depth == 0u,
           "RETLW completes frame pop and literal before target trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_0X55800);
    cpu->pc = 0x557dau;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented literal BRA target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 0x557dcu && cpu->w[15] == 0x5004u &&
               (cpu->corcon & 0x0004u) != 0u,
           "literal BRA stacks following PC and preserves SFA");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0x557eau, OPCODE_RCALL_0X55800);
    cpu->pc = 0x557eau;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented literal RCALL target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 0x557ecu && cpu->w[15] == 0x5008u &&
               dspic33_read_word(cpu, 0x5000u) == 0x57edu &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u && (cpu->corcon & 0x0004u) == 0u,
           "literal RCALL completes return push before target trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_Z_0X55800);
    cpu->pc = 0x557dau;
    cpu->sr |= 0x0002u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "taken conditional BRA validates target in four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_Z_0X55800);
    cpu->pc = 0x557dau;
    cpu->sr &= (uint16_t)~0x0002u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557dcu && cpu->cycles == 1u &&
               cpu->last_trap == UINT16_MAX,
           "untaken conditional BRA skips target validation in one cycle");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_OA_0X55800);
    cpu->pc = 0x557dau;
    cpu->sr = 0x8800u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "taken accumulator BRA validates target in four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_OA_0X55800);
    cpu->pc = 0x557dau;
    cpu->sr = 0x0800u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557dcu && cpu->cycles == 1u &&
               cpu->last_trap == UINT16_MAX,
           "untaken accumulator BRA ignores combined overflow status");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_0X300);
    load_instruction(state, cpu, 2u, 0u);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u && cpu->cycles == 4u,
           "implemented literal GOTO target remains valid");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_GOTO_0X300);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u && cpu->cycles == 4u &&
               cpu->w[15] == 0x5000u && dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
               dspic33_read_word(cpu, 0x5002u) == 0x5a5au && (cpu->corcon & 0x0004u) != 0u &&
               cpu->sequential_program_hole_pc == 0u,
           "boundary GOTO reads zero extension without sequential provenance");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 2u, 0u);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u && cpu->cycles == 4u &&
               dspic33_read_word(cpu, 0x5000u) == 4u,
           "implemented literal CALL target remains valid");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u && cpu->cycles == 4u &&
               cpu->w[15] == 0x5004u && cpu->call_depth == 1u &&
               dspic33_read_word(cpu, 0x5000u) == 0x5803u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u && (cpu->corcon & 0x0004u) == 0u &&
               cpu->sequential_program_hole_pc == 0u,
           "boundary CALL stacks hole return and clears sequential provenance");

    reset_processor_test(cpu, 0x557feu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557feu;
    load_instruction(state, cpu, 0x557feu, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x300u, OPCODE_RETURN);
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u && cpu->cycles == 4u &&
               cpu->sequential_program_hole_pc == 0u,
           "boundary CALL prepares explicit return to program hole");
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 10u && cpu->last_trap == 1u &&
               cpu->last_trap_return == 0x302u && cpu->call_depth == 0u && cpu->w[15] == 0x5004u &&
               cpu->sequential_program_hole_pc == 0u,
           "boundary CALL return validates hole target without provenance reuse");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_W0);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[0] = 0x300u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u && cpu->cycles == 4u,
           "implemented GOTO Wn target remains valid");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_LONG_W0);
    load_instruction(state, cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_NOP);
    cpu->w[0] = 0xc000u;
    cpu->w[1] = 0x007fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x7fc000u && cpu->cycles == 4u &&
               cpu->last_trap == UINT16_MAX,
           "auxiliary GOTO.L target is accepted");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u,
           "auxiliary target executes through mapped Flash");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_W0);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[0] = 0x300u;
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u && cpu->cycles == 4u &&
               dspic33_read_word(cpu, 0x5000u) == 2u,
           "implemented CALL Wn target remains valid");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0x557eau, OPCODE_RCALL_W0);
    cpu->pc = 0x557eau;
    cpu->w[0] = 0x000au;
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557eau;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented RCALL Wn target traps in four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0x557eau, OPCODE_BRA_W0);
    cpu->pc = 0x557eau;
    cpu->w[0] = 0x000au;
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557eau;
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u &&
               cpu->last_trap_return == 0x557ecu && cpu->w[15] == 0x5004u &&
               (cpu->corcon & 0x0004u) != 0u,
           "unimplemented BRA Wn target traps without call side effects");
}

static void program_read_address_error_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x0200u, 0xabcdefu);
    cpu->tblpag = 0u;
    cpu->w[2] = 0x0200u;
    cpu->disicnt = 6u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[3] == 0xcdefu && cpu->cycles == 5u &&
               cpu->disicnt == 1u,
           "implemented table read consumes five cycles");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[3] = 0xa5a5u;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "table read from unimplemented main program traps in five cycles");
    expect(state,
           cpu->w[2] == 0x5800u && cpu->w[3] == 0u && cpu->last_trap_return == 2u &&
               cpu->w[15] == 0x5004u && (cpu->corcon & 0x0004u) != 0u,
           "unimplemented table read returns zero and completes pointer state");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W4_POST_INCREMENT);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[4] = 0x1000u;
    dspic33_write_word(cpu, 0x1000u, 0xa5a5u);
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "unimplemented table read with indirect destination traps in five cycles");
    expect(state, cpu->w[4] == 0x1002u && dspic33_read_word(cpu, 0x1000u) == 0u,
           "table read trap completes pointer update and zero result write");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDH_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5800u && cpu->w[3] == 0u &&
               cpu->cycles == 5u,
           "unimplemented high table word read returns zero in five cycles");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDH_BYTE_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5800u && cpu->w[3] == 0xa500u &&
               cpu->cycles == 5u,
           "unimplemented high table byte read clears the low byte in five cycles");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_BYTE_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5801u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5801u && cpu->w[3] == 0xa500u &&
               cpu->cycles == 5u,
           "unimplemented low table byte read accepts odd source in five cycles");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_POST_INCREMENT_W4_POST_INCREMENT);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[4] = 0x1001u;
    dspic33_write_word(cpu, 0x1000u, 0xa5a5u);
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "unimplemented table read and odd destination coalesce in five cycles");
    expect(state,
           cpu->w[2] == 0x5802u && cpu->w[4] == 0x1001u &&
               dspic33_read_word(cpu, 0x1000u) == 0xa5a5u && cpu->last_trap_return == 2u,
           "table read collision completes source and inhibits destination state");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W15_POST_INCREMENT);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "unimplemented table read through stack pointer traps in five cycles");
    expect(state,
           cpu->w[2] == 0x5800u && cpu->w[15] == 0x5006u && dspic33_read_word(cpu, 0x5000u) == 0u &&
               dspic33_read_word(cpu, 0x5002u) == 2u,
           "stack destination update and zero write precede the trap frame");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLWTL_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5a5au;
    cpu->w[3] = 0x5800u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == UINT16_MAX &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) == 0u && cpu->cycles == 2u,
           "table write to unimplemented main program remains valid in two cycles");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT && cpu->cycles == 1u,
           "one-word sequential execution enters the program hole");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u &&
               cpu->sequential_program_hole_pc == 0x55802u && cpu->cycles == 2u,
           "sequential program hole executes as one-cycle zero instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55804u &&
               cpu->sequential_program_hole_pc == 0x55804u && cpu->cycles == 3u,
           "sequential program hole provenance advances across zero instructions");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DO_0);
    load_instruction(state, cpu, 2u, 2u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->cycles == 2u &&
               cpu->do_depth == 1u && cpu->do_count[0] == 0u && cpu->do_start[0] == 4u &&
               cpu->do_end[0] == 8u,
           "literal DO initializes loop state in two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DO_W0);
    load_instruction(state, cpu, 2u, 2u);
    cpu->w[0] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->cycles == 2u &&
               cpu->do_depth == 1u && cpu->do_count[0] == 1u && cpu->do_start[0] == 4u &&
               cpu->do_end[0] == 8u,
           "register DO initializes loop state in two cycles");

    reset_processor_test(cpu, 0x557fcu);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0x557fcu, OPCODE_DO_1);
    load_instruction(state, cpu, 0x557feu, 2u);
    cpu->sr = 0x010fu;
    cpu->corcon |= 0x0004u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x000340u && cpu->cycles == 2u &&
               cpu->last_trap == 1u && cpu->last_trap_return == 0x557feu && cpu->do_depth == 1u &&
               cpu->do_count[0] == 1u && cpu->do_start[0] == DSPIC33_PROGRAM_LIMIT &&
               cpu->do_end[0] == 0x55804u && cpu->dcount == 1u &&
               cpu->dostart == DSPIC33_PROGRAM_LIMIT && cpu->doend == 0x55804u,
           "DO start in program hole traps after completing loop state");
    expect(state,
           cpu->w[15] == 0x5004u && dspic33_read_word(cpu, 0x5000u) == 0x57feu &&
               dspic33_read_word(cpu, 0x5002u) == 0x0f05u &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u && cpu->sequential_program_hole_pc == 0u,
           "DO program-hole trap stacks extension PC and clears provenance");

    reset_processor_test(cpu, 0x557f8u);
    load_instruction(state, cpu, 0x557f8u, OPCODE_DO_1);
    load_instruction(state, cpu, 0x557fau, 2u);
    load_instruction(state, cpu, 0x557fcu, OPCODE_NOP);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557fcu && cpu->cycles == 2u,
           "DO with program-hole end initializes normally");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557fcu && cpu->dcount == 0u &&
               cpu->last_trap == UINT16_MAX && cpu->cycles == 5u,
           "DO program-hole end executes zero and starts final iteration");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u && cpu->do_depth == 0u &&
               (cpu->sr & 0x0200u) == 0u && (cpu->corcon & 0x0700u) == 0u && cpu->cycles == 8u,
           "DO program-hole final iteration exits normally");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_REPEAT_2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->rcount == 2u && cpu->repeat_active != 0u && (cpu->sr & 0x0010u) != 0u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT && cpu->cycles == 1u,
           "REPEAT initializes a program-hole target");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->rcount == 1u && cpu->repeat_active != 0u && (cpu->sr & 0x0010u) != 0u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT && cpu->cycles == 2u,
           "REPEAT retains provenance for a program-hole rewind");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->rcount == 0u && cpu->repeat_active != 0u && (cpu->sr & 0x0010u) == 0u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT && cpu->cycles == 3u,
           "REPEAT clears RA when the counter reaches zero");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u && cpu->rcount == 0u &&
               cpu->repeat_active == 0u && (cpu->sr & 0x0010u) == 0u &&
               cpu->sequential_program_hole_pc == 0x55802u && cpu->cycles == 4u,
           "REPEAT executes the final program-hole iteration once");

    reset_processor_test(cpu, DSPIC33_PROGRAM_LIMIT);
    expect(state,
           dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS && cpu->sequential_program_hole_pc == 0u &&
               cpu->cycles == 0u,
           "direct host entry into program hole remains a bounds stop");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "sequential provenance prepares exact next hole address");
    cpu->pc = 0x55802u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS && cpu->sequential_program_hole_pc == 0u,
           "host PC rewrite cannot reuse stale sequential provenance");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    load_instruction(state, cpu, 0x000014u, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "sequential program hole prepares interrupt return");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000302u &&
               cpu->interrupt_log_entry[0] == DSPIC33_PROGRAM_LIMIT && cpu->w[15] == 0x5004u &&
               dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 11u,
           "interrupt redirects and stacks program hole PC without provenance leak");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    load_instruction(state, cpu, 0x000014u, DSPIC33_PROGRAM_LIMIT);
    load_instruction(state, cpu, 0x000006u, 0x000340u);
    load_instruction(state, cpu, 0x000340u, OPCODE_NOP);
    cpu->stop_on_trap = false;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "same-PC interrupt prepares sequential provenance");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_count == 0u &&
               cpu->last_trap == 1u && cpu->pc == 0x000342u && cpu->w[15] == 0x5004u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 2u,
           "unimplemented interrupt vector dispatches Address Error");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    load_instruction(state, cpu, 0x00000cu, DSPIC33_PROGRAM_LIMIT);
    load_instruction(state, cpu, 0x000006u, 0x000340u);
    load_instruction(state, cpu, 0x000340u, OPCODE_NOP);
    cpu->stop_on_trap = false;
    cpu->w[15] = 0x5000u;
    cpu->pending_soft_traps[0].trap = 4u;
    cpu->pending_soft_traps[0].vector = 0x00000cu;
    cpu->pending_soft_traps[0].priority = 11u;
    cpu->pending_soft_traps[0].delay = 1u;
    cpu->pending_soft_traps[0].active = true;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u && cpu->pc == 0x000340u &&
               cpu->w[15] == 0x5004u && cpu->sequential_program_hole_pc == 0u && cpu->cycles == 1u,
           "unimplemented soft-trap vector dispatches Address Error");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000342u && cpu->cycles == 2u,
           "Address Error handler continues after invalid soft-trap vector");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_NOP);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE - 2u;
    cpu->sequential_program_hole_pc = cpu->pc;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 1u,
           "sequential hole provenance ends at auxiliary program boundary");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u,
           "auxiliary program boundary enters implemented Flash");
}

static void skip_boundary_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSC_W2_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    cpu->w[2] = 1u;
    cpu->sr = 0x0103u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u && cpu->cycles == 1u &&
               cpu->sr == 0x0103u,
           "untaken BTSC consumes one cycle and preserves status");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSC_W2_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    cpu->w[2] = 0u;
    cpu->sr = 0x0103u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->cycles == 2u &&
               cpu->sr == 0x0103u,
           "taken BTSC over one-word instruction consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_W2_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 4u, 0u);
    cpu->w[2] = 1u;
    cpu->w[15] = 0x5000u;
    cpu->sr = 0x0103u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 6u && cpu->cycles == 3u &&
               cpu->w[15] == 0x5000u && cpu->call_depth == 0u &&
               dspic33_read_word(cpu, 0x5000u) == 0xa5a5u && cpu->sr == 0x0103u,
           "taken BTSS discards complete two-word CALL in three cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSC_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->cycles == 2u &&
               cpu->w[4] == 0x1002u,
           "taken indirect BTSC completes source pointer update");

    reset_processor_test(cpu, 0x557fcu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557fcu;
    load_instruction(state, cpu, 0x557fcu, OPCODE_BTSC_W2_BIT_0);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    cpu->w[2] = 0u;
    cpu->sr = 0x0103u;
    cpu->corcon = 0x0020u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x340u && cpu->cycles == 2u &&
               cpu->last_trap == 1u && cpu->last_trap_return == DSPIC33_PROGRAM_LIMIT &&
               cpu->w[15] == 0x5004u,
           "one-word boundary skip raises Address Error in two cycles");
    expect(state,
           dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0305u &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u && cpu->sequential_program_hole_pc == 0u,
           "boundary skip stacks exact hole PC and hard-trap state");

    reset_processor_test(cpu, 0x557fcu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557fcu;
    load_instruction(state, cpu, 0x557fcu, OPCODE_BTSC_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0u);
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[4] == 0x1002u && cpu->cycles == 2u,
           "boundary skip trap preserves completed indirect source update");

    reset_processor_test(cpu, 0x557fau);
    load_instruction(state, cpu, 0x557fau, OPCODE_BTSC_W2_BIT_0);
    load_instruction(state, cpu, 0x557fcu, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x557feu, 0u);
    cpu->w[2] = 0u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT && cpu->cycles == 3u &&
               cpu->w[15] == 0x5000u && cpu->call_depth == 0u &&
               dspic33_read_word(cpu, 0x5000u) == 0xa5a5u,
           "two-word boundary skip enters the program hole in three cycles");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u &&
               cpu->sequential_program_hole_pc == 0x55802u && cpu->cycles == 4u,
           "two-word skip provenance authorizes the next hole instruction");

    reset_processor_test(cpu, 0x557fau);
    load_instruction(state, cpu, 0x557fau, OPCODE_BTSC_W2_BIT_0);
    load_instruction(state, cpu, 0x557fcu, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x557feu, 0u);
    load_instruction(state, cpu, 0x000014u, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    cpu->w[2] = 0u;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "two-word skip prepares interrupt lifecycle case");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->interrupt_log_entry[0] == DSPIC33_PROGRAM_LIMIT && cpu->w[15] == 0x5004u &&
               dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 13u,
           "interrupt clears two-word skip provenance and stacks hole PC");

    reset_processor_test(cpu, 0x557fcu);
    load_instruction(state, cpu, 0x557fcu, OPCODE_BTSC_W2_BIT_0);
    load_instruction(state, cpu, 0x557feu, OPCODE_CALL_0X300);
    cpu->w[2] = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 3u,
           "skipped extension collision remains outside sequential provenance");
    expect(state, dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS,
           "excluded skipped extension collision retains bounds behavior");
}

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

static void compare_skip_cases(TestState* state, Dspic33* cpu) {
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

static void compare_branch_target_cases(TestState* state, Dspic33* cpu) {
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

static void prepare_timer_source(Dspic33* cpu) {
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

static void address_error_cases(TestState* state, Dspic33* cpu) {
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

static void data_map_address_error_cases(TestState* state, Dspic33* cpu) {
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

static void pseudo_linear_page_cases(TestState* state, Dspic33* cpu) {
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

static void page_zero_address_error_cases(TestState* state, Dspic33* cpu) {
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
    prepare_timer_source(cpu);
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

static void unimplemented_data_page_address_error_cases(TestState* state, Dspic33* cpu) {
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

static void w15_write_cases(TestState* state, Dspic33* cpu) {
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

static void valid_stack_frame_cases(TestState* state, Dspic33* cpu) {
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

static void invalid_lnk_case(TestState* state, Dspic33* cpu) {
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

static void invalid_ulnk_case(TestState* state, Dspic33* cpu) {
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

static void simultaneous_trap_case(TestState* state, Dspic33* cpu) {
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

static void earlier_deadline_case(TestState* state, Dspic33* cpu) {
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

static void repeat_exception_cases(TestState* state, Dspic33* cpu) {
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

static void standalone_divide_zero_cases(TestState* state, Dspic33* cpu) {
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

static void repeat_interrupt_cases(TestState* state, Dspic33* cpu) {
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

static void prepare_nested_do_interrupt_case(TestState* state, Dspic33* cpu, uint32_t entry,
                                             bool nesting_disabled) {
    uint32_t address;
    reset_processor_test(cpu, entry);
    for (address = 0x204u; address <= 0x208u; address += 2u) {
        load_instruction(state, cpu, address, OPCODE_NOP);
    }
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0016u, 0x000320u);
    for (address = 0x300u; address <= 0x30au; address += 2u) {
        load_instruction(state, cpu, address, OPCODE_NOP);
    }
    load_instruction(state, cpu, 0x30cu, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x320u, OPCODE_NOP);
    load_instruction(state, cpu, 0x322u, OPCODE_RETFIE);
    cpu->w[15] = 0x5000u;
    cpu->do_depth = 1u;
    cpu->do_start[0] = 0x204u;
    cpu->do_end[0] = 0x208u;
    cpu->do_count[0] = 3u;
    cpu->dostart = 0x204u;
    cpu->doend = 0x208u;
    cpu->dcount = 3u;
    cpu->sr |= 0x0200u;
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0700u) | 0x0100u);
    dspic33_write_word(cpu, 0x08c0u, nesting_disabled ? 0x8000u : 0u);
    dspic33_write_word(cpu, 0x0820u, 0x0003u);
    dspic33_write_word(cpu, 0x0840u, 0x0042u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
}

static void complete_first_nested_do_interrupt_entry(TestState* state, Dspic33* cpu,
                                                     bool expected_armed,
                                                     bool expected_extra_decrement) {
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u && cpu->cycles == 10u &&
               cpu->interrupt_depth == 1u &&
               (cpu->nested_do_extra_decrement_depth != 0u) == expected_extra_decrement &&
               cpu->nested_do_interrupt_armed == expected_armed,
           "first DO-loop interrupt entry evaluates nested request timing");
    dspic33_write_word(cpu, 0x0800u, (uint16_t)(dspic33_read_word(cpu, 0x0800u) & ~1u));
}

static void enter_first_nested_do_interrupt(TestState* state, Dspic33* cpu, bool expected_armed,
                                            uint8_t nested_delay, bool expected_extra_decrement) {
    dspic33_raise_interrupt(cpu, 0u);
    expect(state, cpu->nested_do_interrupt_armed == expected_armed,
           "first DO-loop interrupt request records the erratum window");
    if (nested_delay != 0u) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, nested_delay),
               "schedule higher-priority request inside interrupt entry");
    }
    complete_first_nested_do_interrupt_entry(state, cpu, expected_armed && nested_delay == 0u,
                                             expected_extra_decrement);
}

static void complete_nested_do_interrupt_case(TestState* state, Dspic33* cpu,
                                              bool expected_extra_decrement) {
    uint8_t index;
    uint8_t main_steps;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x322u &&
               cpu->interrupt_depth == 2u &&
               (cpu->nested_do_extra_decrement_depth != 0u) == expected_extra_decrement,
           "higher-priority nested interrupt evaluates the exact four-cycle window");
    dspic33_write_word(cpu, 0x0800u, 0u);
    dspic33_write_word(cpu, 0x0820u, 0u);
    for (index = 0u; cpu->interrupt_depth != 0u && index < 8u; index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "nested DO-loop interrupt handlers return normally");
    }
    expect(state, cpu->interrupt_depth == 0u, "nested DO-loop interrupt stack fully unwinds");
    main_steps = (uint8_t)(((0x208u - cpu->pc) / 2u) + 1u);
    for (index = 0u; index < main_steps; index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "interrupted DO-loop reaches its iteration boundary");
    }
    expect(state,
           cpu->pc == 0x204u && cpu->do_depth == 1u &&
               cpu->dcount == (expected_extra_decrement ? 1u : 2u) &&
               cpu->do_count[0] == cpu->dcount && cpu->nested_do_extra_decrement_depth == 0u &&
               cpu->nested_do_extra_decrement_end == 0u && !cpu->nested_do_interrupt_armed,
           "DO-loop iteration applies only the documented nested decrement");
}

static void nested_do_interrupt_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t entries[] = {0x206u, 0x208u};
    static const uint8_t delays[] = {3u, 4u, 5u};
    static const uint16_t divisors[] = {0x1800u, 0x9800u};
    static const uint8_t divided_deadlines[] = {10u, 6u};
    Dspic33 copy;
    size_t entry_index;
    size_t delay_index;
    size_t divisor_index;

    prepare_nested_do_interrupt_case(state, cpu, 0x206u, false);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "schedule both interrupts from the executing DO instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x208u && cpu->cycles == 1u &&
               cpu->interrupt_depth == 0u && cpu->nested_do_interrupt_armed &&
               cpu->nested_do_interrupt_end == 0x208u && cpu->nested_do_interrupt_depth == 1u,
           "penultimate DO instruction event records the executing address");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u && cpu->cycles == 11u &&
               cpu->interrupt_depth == 1u && cpu->nested_do_extra_decrement_depth == 1u &&
               !cpu->nested_do_interrupt_armed,
           "scheduled higher-priority request reaches the exact entry-cycle deadline");
    dspic33_write_word(cpu, 0x0800u, (uint16_t)(dspic33_read_word(cpu, 0x0800u) & ~1u));
    complete_nested_do_interrupt_case(state, cpu, true);

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, false);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "schedule nested requests from the final DO instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x204u && cpu->dcount == 2u &&
               cpu->nested_do_interrupt_armed && cpu->nested_do_interrupt_end == 0x208u,
           "final DO instruction event survives the loop-back PC update");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->nested_do_extra_decrement_depth == 1u && !cpu->nested_do_interrupt_armed,
           "final DO instruction request participates in the four-cycle erratum");

    prepare_nested_do_interrupt_case(state, cpu, 0x204u, false);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "schedule nested requests outside the final DO instructions");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x206u &&
               !cpu->nested_do_interrupt_armed,
           "earlier DO instruction event does not arm the erratum");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->nested_do_extra_decrement_depth == 0u && !cpu->nested_do_interrupt_armed,
           "entry-time second request cannot become a replacement first request");

    for (divisor_index = 0u; divisor_index < sizeof(divisors) / sizeof(divisors[0]);
         divisor_index++) {
        prepare_nested_do_interrupt_case(state, cpu, 0x206u, false);
        dspic33_write_word(cpu, 0x0744u, divisors[divisor_index]);
        expect(state,
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 2u) &&
                   dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u,
                                    divided_deadlines[divisor_index]),
               "schedule nested requests across a divided instruction boundary");
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->device_cycles == 2u &&
                   cpu->nested_do_interrupt_armed,
               "divided DO instruction records its interrupt request cycle");
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
                   cpu->nested_do_extra_decrement_depth == 1u && !cpu->nested_do_interrupt_armed,
               "DOZE and ROI preserve the four-instruction-cycle erratum window");
    }

    for (entry_index = 0u; entry_index < sizeof(entries) / sizeof(entries[0]); entry_index++) {
        for (delay_index = 0u; delay_index < sizeof(delays) / sizeof(delays[0]); delay_index++) {
            prepare_nested_do_interrupt_case(state, cpu, entries[entry_index], false);
            enter_first_nested_do_interrupt(state, cpu, true, delays[delay_index],
                                            delays[delay_index] == 4u);
            complete_nested_do_interrupt_case(state, cpu, delays[delay_index] == 4u);
        }
    }

    prepare_nested_do_interrupt_case(state, cpu, 0x204u, false);
    enter_first_nested_do_interrupt(state, cpu, false, 4u, false);
    complete_nested_do_interrupt_case(state, cpu, false);

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, false);
    enter_first_nested_do_interrupt(state, cpu, true, 0u, false);
    dspic33_write_word(cpu, 0x0820u, 0u);
    for (delay_index = 0u; cpu->interrupt_depth != 0u && delay_index < 8u; delay_index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "single DO-loop interrupt handler returns normally");
    }
    expect(state, cpu->interrupt_depth == 0u && cpu->pc == 0x208u && cpu->nested_do_interrupt_armed,
           "single DO-loop interrupt retains its window until the loop boundary");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->dcount == 2u &&
               !cpu->nested_do_interrupt_armed && cpu->nested_do_interrupt_cycle == 0u &&
               cpu->nested_do_interrupt_end == 0u && cpu->nested_do_interrupt_depth == 0u &&
               cpu->nested_do_interrupt_priority == 0u,
           "DO-loop boundary expires an unused nested-interrupt window");

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, true);
    enter_first_nested_do_interrupt(state, cpu, false, 4u, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_depth == 1u &&
               cpu->interrupt_count == 1u && cpu->nested_do_extra_decrement_depth == 0u,
           "NSTDIS prevents the nested DO-loop erratum sequence");

    prepare_nested_do_interrupt_case(state, cpu, 0x206u, false);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state, cpu->nested_do_interrupt_armed,
           "copy source records the first DO-loop interrupt request");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 4u),
           "copy source schedules the exact nested request");
    expect(state, dspic33_initialize(&copy), "initialize nested DO-loop erratum copy");
    expect(state, dspic33_copy(&copy, cpu), "copy armed nested DO-loop erratum state");
    complete_first_nested_do_interrupt_entry(state, cpu, false, true);
    complete_first_nested_do_interrupt_entry(state, &copy, false, true);
    complete_nested_do_interrupt_case(state, cpu, true);
    complete_nested_do_interrupt_case(state, &copy, true);
    dspic33_release(&copy);

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, false);
    enter_first_nested_do_interrupt(state, cpu, true, 0u, false);
    load_instruction(state, cpu, 0x302u, OPCODE_RESET);
    dspic33_write_word(cpu, 0x0800u, 0u);
    dspic33_write_word(cpu, 0x0820u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->software_reset_count == 1u &&
               !cpu->nested_do_interrupt_armed && cpu->nested_do_interrupt_cycle == 0u &&
               cpu->nested_do_interrupt_end == 0u && cpu->nested_do_interrupt_depth == 0u &&
               cpu->nested_do_interrupt_priority == 0u &&
               cpu->nested_do_extra_decrement_depth == 0u &&
               cpu->nested_do_extra_decrement_end == 0u,
           "warm reset clears nested DO-loop erratum state");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "processor fault test initializes");
    if (initialized) {
        program_target_address_error_cases(&state, &cpu);
        program_read_address_error_cases(&state, &cpu);
        compare_skip_cases(&state, &cpu);
        compare_branch_target_cases(&state, &cpu);
        skip_boundary_cases(&state, &cpu);
        address_error_cases(&state, &cpu);
        data_map_address_error_cases(&state, &cpu);
        pseudo_linear_page_cases(&state, &cpu);
        page_zero_address_error_cases(&state, &cpu);
        unimplemented_data_page_address_error_cases(&state, &cpu);
        w15_write_cases(&state, &cpu);
        valid_stack_frame_cases(&state, &cpu);
        invalid_lnk_case(&state, &cpu);
        invalid_ulnk_case(&state, &cpu);
        simultaneous_trap_case(&state, &cpu);
        earlier_deadline_case(&state, &cpu);
        repeat_exception_cases(&state, &cpu);
        standalone_divide_zero_cases(&state, &cpu);
        repeat_interrupt_cases(&state, &cpu);
        nested_do_interrupt_erratum_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
