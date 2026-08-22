#include "architecture/dspic33/timing/instructions/internal.h"

void dspic33_timing_test_psv_repeat_timing_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t values[] = {
        0x1111u, 0x2222u, 0x3333u, 0x4444u, 0x5555u, 0x6666u,
    };
    size_t index;

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    for (index = 0u; index < sizeof(values) / sizeof(values[0]); index++) {
        expect(state, dspic33_load_program_word(cpu, 0x4000u + (uint32_t)index * 2u, values[index]),
               "load repeated PSV timing value");
    }
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "REPEAT setup precedes optimized PSV access");
    expect_step_cycles(state, cpu, 5u, "first repeated PSV postincrement uses five cycles");
    expect_step_cycles(state, cpu, 1u, "middle repeated PSV postincrement uses one cycle");
    expect_step_cycles(state, cpu, 6u, "last repeated PSV postincrement uses six cycles");
    expect(state,
           cpu->w[1] == 0xc006u && cpu->w[2] == 0x3333u && cpu->rcount == 0u &&
               cpu->repeat_active == 0u && !cpu->repeat_psv_started,
           "optimized PSV postincrement completes repeat state");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_POST_DECREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xc006u);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "PSV postdecrement REPEAT setup");
    expect_step_cycles(state, cpu, 5u, "first repeated PSV postdecrement uses five cycles");
    expect_step_cycles(state, cpu, 1u, "middle repeated PSV postdecrement uses one cycle");
    expect_step_cycles(state, cpu, 6u, "last repeated PSV postdecrement uses six cycles");
    expect(state, cpu->w[1] == 0xc000u && cpu->w[2] == 0x2222u,
           "optimized PSV postdecrement reads each word");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "byte PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u, "first repeated byte PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u, "middle repeated byte PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u, "last repeated byte PSV access uses five cycles");
    expect(state, cpu->w[1] == 0xc003u,
           "byte PSV postincrement remains outside optimized schedule");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "preincrement PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u, "first repeated preincrement PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u, "middle repeated preincrement PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u, "last repeated preincrement PSV access uses five cycles");
    expect(state, cpu->w[1] == 0xc004u && cpu->w[2] == 0x3333u,
           "preincrement PSV remains outside optimized schedule");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "MOV.D PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u, "first repeated MOV.D PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u, "middle repeated MOV.D PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u, "last repeated MOV.D PSV access uses five cycles");
    expect(state, cpu->w[1] == 0xc00cu && cpu->w[2] == 0x5555u && cpu->w[3] == 0x6666u,
           "MOV.D PSV postincrement remains outside optimized schedule");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xc000u);
    dspic33_set_working_register(cpu, 10u, 0x9008u);
    dspic33_write_word(cpu, 0x9008u, 0x7777u);
    dspic33_write_word(cpu, 0x9006u, 0x8888u);
    dspic33_write_word(cpu, 0x9004u, 0x9999u);
    cpu->dsrpag = 0x0200u;
    cpu->corcon = 0x0021u;
    expect_step_cycles(state, cpu, 1u, "DSP PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u, "first repeated DSP PSV prefetch uses five cycles");
    expect_step_cycles(state, cpu, 1u, "middle repeated DSP PSV prefetch uses one cycle");
    expect_step_cycles(state, cpu, 6u, "last repeated DSP PSV prefetch uses six cycles");
    expect(state,
           cpu->w[8] == 0xc006u && cpu->w[10] == 0x9002u && cpu->w[4] == 0x3333u &&
               cpu->w[5] == 0x9999u,
           "DSP PSV postincrement uses optimized repeat schedule");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x090004u);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x0302u, OPCODE_RETFIE);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->w[15] = 0x5000u;
    cpu->dsrpag = 0x0200u;
    cpu->disicnt = 7u;
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    expect_step_cycles(state, cpu, 1u, "interruptible PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u, "first interruptible repeated PSV access");
    expect_step_cycles(state, cpu, 5u, "pre-interrupt repeated PSV access exits in five cycles");
    expect(state,
           cpu->disicnt == 0u && cpu->rcount == 2u && cpu->repeat_active != 0u &&
               cpu->w[1] == 0xc004u,
           "pre-interrupt PSV iteration preserves suspended repeat state");
    expect_step_cycles(state, cpu, 9u, "interrupt dispatch executes handler instruction");
    expect(state,
           cpu->pc == 0x0302u && cpu->repeat_active == 0u && cpu->rcount == 2u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x1000u) != 0u,
           "interrupt entry stacks optimized repeat state");
    dspic33_write_word(cpu, 0x0800u, 0u);
    expect_step_cycles(state, cpu, 6u, "RETFIE restores optimized repeat state");
    expect(state, cpu->repeat_active != 0u && cpu->repeat_psv_reentry && cpu->pc == 0x202u,
           "RETFIE arms PSV repeat re-entry timing");
    expect_step_cycles(state, cpu, 5u, "re-entered PSV repeat access uses five cycles");
    expect_step_cycles(state, cpu, 1u, "resumed middle PSV repeat access uses one cycle");
    expect_step_cycles(state, cpu, 6u, "resumed final PSV repeat access uses six cycles");
    expect(state,
           cpu->w[1] == 0xc00au && cpu->w[2] == 0x5555u && cpu->repeat_active == 0u &&
               !cpu->repeat_psv_started && !cpu->repeat_psv_reentry,
           "interrupted PSV repeat completes all iterations");

    cpu->repeat_psv_started = true;
    cpu->repeat_psv_reentry = true;
    cpu->psv_repeat_optimized = true;
    dspic33_reset(cpu, 0u);
    expect(state,
           !cpu->repeat_psv_started && !cpu->repeat_psv_reentry && !cpu->psv_repeat_optimized,
           "reset clears PSV repeat timing state");
}

void dspic33_timing_test_psv_program_hole_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xd800u);
    dspic33_set_working_register(cpu, 2u, 0xa5a5u);
    dspic33_set_working_register(cpu, 3u, 0xbeefu);
    dspic33_write_word(cpu, 0x5008u, 0x1357u);
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "PSV program-hole word read traps");
    expect(state,
           cpu->w[1] == 0xd802u && cpu->w[2] == 0u && cpu->w[3] == 0xbeefu &&
               cpu->dsrpag == 0x020au && dspic33_read_word(cpu, 0x5008u) == 0x1357u &&
               cpu->cycles == 5u,
           "PSV program-hole word read completes destination and pointer");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xd800u);
    dspic33_set_working_register(cpu, 2u, 0xa5a5u);
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "PSV program-hole low-byte read traps");
    expect(state,
           cpu->w[1] == 0xd801u && cpu->w[2] == 0xa500u && cpu->dsrpag == 0x020au &&
               cpu->cycles == 5u,
           "PSV program-hole low-byte read preserves destination high byte");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xd800u);
    dspic33_set_working_register(cpu, 2u, 0x5a5au);
    cpu->dsrpag = 0x030au;
    expect_address_trap(state, cpu, "PSV program-hole high-byte read traps");
    expect(state,
           cpu->w[1] == 0xd801u && cpu->w[2] == 0x5a00u && cpu->dsrpag == 0x030au &&
               cpu->cycles == 5u,
           "PSV program-hole high-byte read preserves destination high byte");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xd800u);
    dspic33_set_working_register(cpu, 2u, 0xa5a5u);
    dspic33_set_working_register(cpu, 3u, 0x5a5au);
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "PSV program-hole MOV.D read traps");
    expect(state,
           cpu->w[1] == 0xd804u && cpu->w[2] == 0u && cpu->w[3] == 0u && cpu->dsrpag == 0x020au &&
               cpu->cycles == 5u && cpu->trap_count == 1u,
           "PSV program-hole MOV.D coalesces reads and completes pointer");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_MEMORY_W2_MEMORY);
    dspic33_set_working_register(cpu, 1u, 0xd800u);
    dspic33_set_working_register(cpu, 2u, 0x5008u);
    dspic33_write_word(cpu, 0x5008u, 0x2468u);
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "PSV program-hole memory move traps");
    expect(state,
           cpu->w[1] == 0xd800u && cpu->w[2] == 0x5008u &&
               dspic33_read_word(cpu, 0x5008u) == 0x2468u && cpu->cycles == 5u,
           "PSV program-hole fault inhibits data destination write");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x557feu, 0x001357u),
           "load final implemented PSV word");
    dspic33_set_working_register(cpu, 1u, 0xd7feu);
    dspic33_set_working_register(cpu, 2u, 0xa5a5u);
    cpu->dsrpag = 0x020au;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xd800u && cpu->w[2] == 0x1357u &&
               cpu->dsrpag == 0x020au && !cpu->address_error && cpu->cycles == 5u,
           "final implemented PSV word remains valid before hole boundary");

    reset_processor_test(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x01055800u) == 0u && !cpu->address_error,
           "raw PSV program-hole read bypasses CPU fault state");
}

void dspic33_timing_test_address_register_dependency_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W3);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_W2_W3);
    dspic33_write_word(cpu, 0x1000u, 0x4567u);
    expect_step_cycles(state, cpu, 1u, "direct pointer write uses one cycle");
    expect_step_cycles(state, cpu, 2u, "direct write to indirect source stalls");
    expect(state, cpu->w[3] == 0x4567u, "dependency stall preserves source value");
    expect_step_cycles(state, cpu, 1u, "direct source does not calculate an address");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_W2_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W3);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    expect_step_cycles(state, cpu, 1u, "same-value direct write uses one cycle");
    expect_step_cycles(state, cpu, 2u, "same-value direct write still creates dependency");
    expect(state, cpu->w[3] == 0x1234u, "same-value dependency preserves source value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_W2_INDIRECT_W3);
    dspic33_write_word(cpu, 0x1000u, 0x2345u);
    expect_step_cycles(state, cpu, 1u, "intervening-control pointer setup uses one cycle");
    expect_step_cycles(state, cpu, 1u, "intervening instruction consumes dependency window");
    expect_step_cycles(state, cpu, 1u, "source after intervening instruction does not stall");
    expect(state, cpu->w[3] == 0x2345u, "intervening instruction control reads value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_W3_W2_INDIRECT);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W4);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_set_working_register(cpu, 3u, 0xa55au);
    expect_step_cycles(state, cpu, 1u, "indirect destination preserves pointer timing");
    expect_step_cycles(state, cpu, 1u,
                       "unmodified indirect destination does not create dependency");
    expect(state, cpu->w[4] == 0xa55au, "unmodified pointer reads stored value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_W3_W2_POST_INCREMENT);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W4);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_set_working_register(cpu, 3u, 0x1111u);
    dspic33_write_word(cpu, 0x1002u, 0x2222u);
    expect_step_cycles(state, cpu, 1u, "destination postincrement uses one cycle");
    expect_step_cycles(state, cpu, 2u, "destination postincrement creates pointer dependency");
    expect(state, cpu->w[2] == 0x1002u && cpu->w[4] == 0x2222u,
           "destination dependency uses updated pointer");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_W2_POST_INCREMENT_W3);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W4);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x3333u);
    dspic33_write_word(cpu, 0x1002u, 0x4444u);
    expect_step_cycles(state, cpu, 1u, "source postincrement uses one cycle");
    expect_step_cycles(state, cpu, 2u, "source postincrement creates pointer dependency");
    expect(state, cpu->w[3] == 0x3333u && cpu->w[4] == 0x4444u,
           "source dependency preserves both reads");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W3_W2_INDIRECT);
    dspic33_set_working_register(cpu, 3u, 0x5a5au);
    expect_step_cycles(state, cpu, 1u, "destination pointer setup uses one cycle");
    expect_step_cycles(state, cpu, 1u,
                       "destination address calculation does not create source stall");
    expect(state, dspic33_read_word(cpu, 0x1000u) == 0x5a5au,
           "destination-only dependency control stores value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_W4_OFFSET_W3);
    dspic33_set_working_register(cpu, 4u, 2u);
    dspic33_write_word(cpu, 0x1002u, 0x6789u);
    expect_step_cycles(state, cpu, 1u, "indexed base setup uses one cycle");
    expect_step_cycles(state, cpu, 2u, "indexed source depends on base register");
    expect(state, cpu->w[3] == 0x6789u, "indexed base dependency reads value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_2_W4);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_W4_OFFSET_W3);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1002u, 0x789au);
    expect_step_cycles(state, cpu, 1u, "indexed offset setup uses one cycle");
    expect_step_cycles(state, cpu, 2u, "indexed source depends on offset register");
    expect(state, cpu->w[3] == 0x789au, "indexed offset dependency reads value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_TBLRDL_W2_W3);
    expect(state, dspic33_load_program_word(cpu, 0x1000u, 0x00abcdefu),
           "load table dependency value");
    expect_step_cycles(state, cpu, 1u, "table pointer setup uses one cycle");
    expect_step_cycles(state, cpu, 6u, "table read source pointer dependency stalls");
    expect(state, cpu->w[3] == 0xcdefu, "table dependency preserves read value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X8100_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W3);
    expect(state, dspic33_load_program_word(cpu, 0x0100u, 0x00123456u),
           "load PSV dependency value");
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "PSV pointer setup uses one cycle");
    expect_step_cycles(state, cpu, 6u, "PSV access composes with dependency stall");
    expect(state, cpu->w[3] == 0x3456u, "PSV dependency preserves read value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W8);
    load_instruction(state, cpu, 0x202u, OPCODE_DSP_X_NO_UPDATE_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x1000u, 0x1357u);
    dspic33_write_word(cpu, 0x9002u, 0x2468u);
    cpu->corcon = 0x0021u;
    expect_step_cycles(state, cpu, 1u, "DSP source pointer setup uses one cycle");
    expect_step_cycles(state, cpu, 2u, "DSP prefetch source dependency stalls");
    expect(state, cpu->w[4] == 0x1357u && cpu->w[5] == 0x2468u,
           "DSP dependency preserves both prefetches");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x202u, 0u);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_W2_INDIRECT_W3);
    load_instruction(state, cpu, 0x0300u, OPCODE_RETURN);
    cpu->w[15] = 0x5000u;
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x3579u);
    expect_step_cycles(state, cpu, 4u, "CALL writes stack pointer");
    expect_step_cycles(state, cpu, 7u, "RETURN source depends on CALL stack write");
    expect_step_cycles(state, cpu, 1u, "RETURN does not create following dependency");
    expect(state, cpu->w[3] == 0x3579u, "post-RETURN source completes without stall");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x202u, 0u);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_W2_INDIRECT_W3);
    load_instruction(state, cpu, 0x0300u, OPCODE_RETLW_0X10_W2);
    cpu->w[15] = 0x5000u;
    dspic33_set_working_register(cpu, 8u, 0x5000u);
    expect_step_cycles(state, cpu, 4u, "RETLW caller writes stack pointer");
    expect_step_cycles(state, cpu, 7u, "RETLW source depends on CALL stack write");
    expect_step_cycles(state, cpu, 2u, "RETLW destination creates dependency");
    expect(state, cpu->w[3] == 0x5000u, "RETLW dependency reads literal pointer");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W3);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_RETFIE);
    dspic33_write_word(cpu, 0x1000u, 0x9abcu);
    cpu->w[15] = 0x5000u;
    expect_step_cycles(state, cpu, 1u, "interrupt dependency writer completes");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect_step_cycles(state, cpu, 14u, "interrupt dispatch flushes pending dependency");
    dspic33_write_word(cpu, 0x0800u, 0u);
    expect_step_cycles(state, cpu, 1u, "source after interrupt does not retain prior dependency");
    expect(state, cpu->w[3] == 0x9abcu, "post-interrupt source preserves value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_POST_INCREMENT_W3);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1111u);
    dspic33_write_word(cpu, 0x1002u, 0x2222u);
    dspic33_write_word(cpu, 0x1004u, 0x3333u);
    expect_step_cycles(state, cpu, 1u, "REPEAT setup uses one cycle");
    expect_step_cycles(state, cpu, 1u, "first repeated pointer read has no dependency");
    expect_step_cycles(state, cpu, 2u, "middle repeated pointer read stalls");
    expect_step_cycles(state, cpu, 2u, "last repeated pointer read stalls");
    expect(state, cpu->w[2] == 0x1006u && cpu->w[3] == 0x3333u,
           "REPEAT dependency advances every iteration");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_DO_1);
    load_instruction(state, cpu, 0x202u, 0u);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_W2_POST_INCREMENT_W3);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x5555u);
    dspic33_write_word(cpu, 0x1002u, 0xaaaau);
    expect_step_cycles(state, cpu, 2u, "DO setup uses two cycles");
    expect_step_cycles(state, cpu, 1u, "first DO pointer read has no dependency");
    expect_step_cycles(state, cpu, 2u, "looped DO pointer read stalls");
    expect(state, cpu->w[2] == 0x1004u && cpu->w[3] == 0xaaaau && cpu->do_depth == 0u,
           "DO dependency completes loop state");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W3);
    expect_step_cycles(state, cpu, 1u, "reset dependency writer completes");
    dspic33_reset(cpu, 0x202u);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0xabcdu);
    expect_step_cycles(state, cpu, 1u, "reset clears pending dependency");
    expect(state, cpu->w[3] == 0xabcdu, "post-reset source preserves value");
}

static void expect_dsp_x_page_transition(TestState* state, Dspic33* cpu, uint32_t opcode,
                                         uint16_t pointer, uint16_t page, uint32_t program_address,
                                         uint16_t value, uint16_t expected_pointer,
                                         uint16_t expected_page, const char* name) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, opcode);
    expect(state, dspic33_load_program_word(cpu, program_address, value),
           "load DSP X transition value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, pointer);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 0x6789u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = page;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 12 &&
               cpu->w[4] == value && cpu->w[5] == 0x6789u && cpu->w[8] == expected_pointer &&
               cpu->w[10] == 0x9000u && cpu->dsrpag == expected_page && cpu->cycles == 5u,
           name);
}

void dspic33_timing_test_dsp_x_prefetch_page_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    expect(state, dspic33_load_program_word(cpu, 0x0100u, 0x001234u), "load DSP X PSV value");
    expect(state, dspic33_load_program_word(cpu, 0x1002u, 0x009abcu), "load DSP Y isolation value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0x8100u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x8100u, 0x1111u);
    dspic33_write_word(cpu, 0x9002u, 0x5678u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 12 &&
               cpu->w[4] == 0x1234u && cpu->w[5] == 0x5678u && cpu->w[8] == 0x8102u &&
               cpu->w[10] == 0x9000u && cpu->dsrpag == 0x0200u && cpu->cycles == 5u &&
               cpu->corcon == 0x0021u && cpu->sr == 0u,
           "DSP X PSV prefetch maps through DSRPAG while Y remains base-only");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x003456u), "load DSP X crossing value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xfffeu);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 0x6789u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0x3456u && cpu->w[5] == 0x6789u &&
               cpu->w[8] == 0x8000u && cpu->w[10] == 0x9000u && cpu->dsrpag == 0x0201u &&
               cpu->cycles == 5u,
           "DSP X post-increment reads the original PSV page before transition");

    expect_dsp_x_page_transition(state, cpu, OPCODE_DSP_X_W8_INCREMENT_4_Y_W10_DECREMENT, 0xfffcu,
                                 0x0200u, 0x7ffcu, 0x4567u, 0x8000u, 0x0201u,
                                 "DSP X four-byte post-increment crosses into the next PSV page");
    expect_dsp_x_page_transition(state, cpu, OPCODE_DSP_X_W8_INCREMENT_6_Y_W10_DECREMENT, 0xfffau,
                                 0x0200u, 0x7ffau, 0x5678u, 0x8000u, 0x0201u,
                                 "DSP X six-byte post-increment crosses into the next PSV page");
    expect_dsp_x_page_transition(state, cpu, OPCODE_DSP_X_W8_DECREMENT_4_Y_W10_DECREMENT, 0x8000u,
                                 0x0201u, 0x8000u, 0x6789u, 0xfffcu, 0x0200u,
                                 "DSP X four-byte post-decrement crosses into the prior PSV page");
    expect_dsp_x_page_transition(state, cpu, OPCODE_DSP_X_W8_DECREMENT_6_Y_W10_DECREMENT, 0x8000u,
                                 0x0201u, 0x8000u, 0x789au, 0xfffau, 0x0200u,
                                 "DSP X six-byte post-decrement crosses into the prior PSV page");
    expect_dsp_x_page_transition(state, cpu, OPCODE_DSP_X_W8_DECREMENT_Y_W10_DECREMENT, 0x8000u,
                                 0x0201u, 0x8000u, 0x89abu, 0xfffeu, 0x0200u,
                                 "DSP X two-byte post-decrement crosses into the prior PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0x8100u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x8100u, 0x2468u);
    dspic33_write_word(cpu, 0x9002u, 0x789au);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0x2468u && cpu->w[5] == 0x789au &&
               cpu->w[8] == 0x8102u && cpu->dsrpag == 1u && cpu->cycles == 1u,
           "DSP X EDS prefetch retains data-space timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    expect(state, dspic33_load_program_word(cpu, 0x0100u, 0x00abcdu),
           "load indexed DSP X PSV value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 9u, 0x80feu);
    dspic33_set_working_register(cpu, 11u, 0x8ffeu);
    dspic33_set_working_register(cpu, 12u, 2u);
    dspic33_write_word(cpu, 0x9000u, 0x89abu);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0200u;
    cpu->accumulator[0] = 5;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 17 &&
               cpu->w[4] == 0xabcdu && cpu->w[5] == 0x89abu && cpu->w[9] == 0x80feu &&
               cpu->w[11] == 0x8ffeu && cpu->w[12] == 2u && cpu->dsrpag == 0x0200u &&
               cpu->cycles == 5u,
           "indexed DSP X PSV prefetch does not update pointer or page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    expect(state, dspic33_load_program_word(cpu, 0x8000u, 0x00cdefu),
           "load indexed DSP X overflow value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 9u, 0xfffeu);
    dspic33_set_working_register(cpu, 11u, 0x8ffeu);
    dspic33_set_working_register(cpu, 12u, 2u);
    dspic33_write_word(cpu, 0x9000u, 0x9abcu);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0201u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 12 &&
               cpu->w[4] == 0xcdefu && cpu->w[5] == 0x9abcu && cpu->w[9] == 0xfffeu &&
               cpu->w[11] == 0x8ffeu && cpu->w[12] == 2u && cpu->dsrpag == 0x0201u &&
               cpu->cycles == 5u,
           "indexed DSP X overflow wraps within the current PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x00def0u),
           "load indexed DSP X underflow value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 9u, 0x8000u);
    dspic33_set_working_register(cpu, 11u, 0x9002u);
    dspic33_set_working_register(cpu, 12u, 0xfffeu);
    dspic33_write_word(cpu, 0x9000u, 0x9abcu);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 12 &&
               cpu->w[4] == 0xdef0u && cpu->w[5] == 0x9abcu && cpu->w[9] == 0x8000u &&
               cpu->w[11] == 0x9002u && cpu->w[12] == 0xfffeu && cpu->dsrpag == 0x0200u &&
               cpu->cycles == 5u,
           "indexed DSP X underflow wraps within the current PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    expect(state, dspic33_load_program_word(cpu, 0x7ff8u, 0x00def0u),
           "load indexed modulo DSP X PSV value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 9u, 0xfffeu);
    dspic33_set_working_register(cpu, 11u, 0x8ffeu);
    dspic33_set_working_register(cpu, 12u, 2u);
    dspic33_write_word(cpu, 0x0046u, 0x8009u);
    dspic33_write_word(cpu, 0x0048u, 0xfff8u);
    dspic33_write_word(cpu, 0x004au, 0xffffu);
    dspic33_write_word(cpu, 0x9000u, 0x9abcu);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 12 &&
               cpu->w[4] == 0xdef0u && cpu->w[5] == 0x9abcu && cpu->w[9] == 0xfffeu &&
               cpu->w[11] == 0x8ffeu && cpu->w[12] == 2u && cpu->dsrpag == 0x0200u &&
               cpu->cycles == 5u,
           "indexed DSP X modulo wrap retains pointer and PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x00bcdeu),
           "load modulo DSP X PSV value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xfffeu);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x0046u, 0x8008u);
    dspic33_write_word(cpu, 0x0048u, 0xfff8u);
    dspic33_write_word(cpu, 0x004au, 0xffffu);
    dspic33_write_word(cpu, 0x9002u, 0x9abcu);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0xbcdeu && cpu->w[5] == 0x9abcu &&
               cpu->w[8] == 0xfff8u && cpu->dsrpag == 0x0200u && cpu->cycles == 5u,
           "DSP X modulo wrap retains the PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 0xfffdu);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 0xa55au);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x03ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == -12 &&
               cpu->w[4] == 0xfffdu && cpu->w[5] == 0xa55au && cpu->w[10] == 0x9000u &&
               cpu->dsrpag == 0x03ffu && cpu->cycles == 1u && cpu->corcon == 0x0021u &&
               cpu->sr == 0u,
           "DSP Y prefetch ignores DSRPAG without X activity");
}

void dspic33_timing_test_dsp_prefetch_address_error_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_NO_UPDATE_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0x9000u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x9002u, 0x6789u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "DSP X prefetch outside X space traps");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0u && cpu->w[5] == 0x6789u &&
               cpu->w[8] == 0x9000u && cpu->w[10] == 0x9000u && cpu->w[15] == 0x5004u &&
               dspic33_read_word(cpu, 0x5000u) == 2u && dspic33_read_word(cpu, 0x5002u) == 0u &&
               cpu->sr == 0x00c0u && cpu->corcon == 0x0029u,
           "invalid DSP X returns zero while valid Y and arithmetic complete");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0x8000u);
    dspic33_set_working_register(cpu, 10u, 0x8000u);
    dspic33_write_word(cpu, 0x8000u, 0x5a5au);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "DSP Y prefetch outside Y space traps");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0x5a5au && cpu->w[5] == 0u &&
               cpu->w[8] == 0x8002u && cpu->w[10] == 0x7ffeu,
           "invalid DSP Y returns zero while valid X and arithmetic complete");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_NO_UPDATE_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xe000u);
    dspic33_set_working_register(cpu, 10u, 0xe000u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "dual unimplemented DSP prefetches trap once");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0u && cpu->w[5] == 0u &&
               cpu->w[8] == 0xe000u && cpu->w[10] == 0xdffeu && cpu->trap_count == 1u,
           "dual invalid DSP lanes return zero and complete pointer state");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0x8ffeu);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x8ffeu, 0x1357u);
    dspic33_write_word(cpu, 0x9002u, 0x2468u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "DSP X invalid post-update address traps");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0x1357u && cpu->w[5] == 0x2468u &&
               cpu->w[8] == 0x9000u && cpu->w[10] == 0x9000u,
           "DSP X post-update trap preserves fetched values and pointer updates");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0x8000u);
    dspic33_set_working_register(cpu, 10u, 0x9000u);
    dspic33_write_word(cpu, 0x8000u, 0x3579u);
    dspic33_write_word(cpu, 0x9000u, 0x468au);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "DSP Y invalid post-update address traps");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0x3579u && cpu->w[5] == 0x468au &&
               cpu->w[8] == 0x8002u && cpu->w[10] == 0x8ffeu,
           "DSP Y post-update trap preserves fetched values and pointer updates");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_CLEAR_DIRECT);
    dspic33_set_working_register(cpu, 8u, 0x9000u);
    cpu->accumulator[0] = 0x123456u;
    cpu->accumulator[1] = 0u;
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "CLR with invalid DSP X prefetch traps");
    expect(state,
           cpu->accumulator[0] == 0 && cpu->w[4] == 0u && cpu->w[8] == 0x9006u && cpu->w[13] == 0u,
           "CLR accumulator, prefetch destination, pointer and direct write-back "
           "complete");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_MOVSAC_WRITE_BACK);
    dspic33_set_working_register(cpu, 4u, 1u);
    dspic33_set_working_register(cpu, 5u, 1u);
    dspic33_set_working_register(cpu, 9u, 0x9000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    dspic33_set_working_register(cpu, 13u, 0x5100u);
    dspic33_write_word(cpu, 0x5100u, 0xa55au);
    cpu->accumulator[0] = 0x123456u;
    cpu->accumulator[1] = 0x654321u;
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "MOVSAC indirect write-back prefetch fault traps");
    expect(state,
           cpu->accumulator[0] == 0x123456 && cpu->accumulator[1] == 0x654321 && cpu->w[4] == 0u &&
               cpu->w[5] == 0u && cpu->w[9] == 0x8ffeu && cpu->w[13] == 0x5102u &&
               dspic33_read_word(cpu, 0x5100u) == 0xa55au,
           "DSP prefetch fault inhibits memory write while pointer update completes");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_ED);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 8u, 0x9000u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 2u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "ED invalid DSP X prefetch traps");
    expect(state,
           cpu->accumulator[0] == 9 && cpu->w[4] == 0xfffeu && cpu->w[8] == 0x9002u &&
               cpu->w[10] == 0x9000u,
           "ED result, distance destination and pointers complete before trap");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0x8001u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    cpu->accumulator[0] = 0x123456;
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 1u;
    expect_address_trap(state, cpu, "misaligned DSP X prefetch traps");
    expect(state,
           cpu->accumulator[0] == 0x123456 && cpu->w[4] == 3u && cpu->w[5] == 4u &&
               cpu->w[8] == 0x8001u && cpu->w[10] == 0x9002u,
           "DSP alignment fault rolls back accumulator and working state");
}
