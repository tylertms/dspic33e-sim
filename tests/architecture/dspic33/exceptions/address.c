#include "architecture/dspic33/exceptions/internal.h"

void dspic33_fault_test_program_target_address_error_cases(TestState* state, Dspic33* cpu) {
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
    cpu->sr &= (uint16_t)~(uint16_t)0x0002u;
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

void dspic33_fault_test_program_read_address_error_cases(TestState* state, Dspic33* cpu) {
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

void dspic33_fault_test_skip_boundary_cases(TestState* state, Dspic33* cpu) {
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
