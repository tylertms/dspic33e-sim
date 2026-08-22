#include "architecture/dspic33/timing/instructions/internal.h"

void dspic33_timing_test_call_stack_timing_case(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000300u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x4ffeu);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "CALL stack fault matures within CALL cycles");
    expect(state, cpu->w[1] == 0u, "CALL stack fault precedes target instruction");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000200u,
           "CALL stack fault enters stack trap");
    expect(state, cpu->w[15] == 0x5008u && dspic33_read_word(cpu, 0x5004u) == 0x0300u,
           "CALL stack fault stacks target return PC");
}

void dspic33_timing_test_instruction_cycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u,
           "NOP consumes one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W14_W2);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u,
           "MOV.D consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "direct CALL consumes four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_W0);
    cpu->w[0] = 0x0300u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "CALL Wn consumes four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RCALL_NEXT);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "literal RCALL consumes four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RCALL_W0);
    cpu->w[0] = 0x007fu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "RCALL Wn consumes four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_LONG_W0);
    cpu->w[0] = 0x0300u;
    cpu->w[1] = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "CALL.L consumes four cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETFIE without pending exception consumes six cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x000014u, 0x000200u);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    cpu->sr = 0x00e0u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 5u,
           "RETFIE with pending exception consumes five cycles");
}

void dspic33_timing_test_register_move_instruction_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SWAP_W1);
    cpu->initialized_working_registers &= (uint16_t)~0x0002u;
    cpu->w[1] = 0xa587u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x87a5u && cpu->sr == 0x0105u &&
               cpu->cycles == 1u && (cpu->initialized_working_registers & 0x0002u) != 0u,
           "SWAP exchanges bytes and initializes word destination");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SWAP_BYTE_W1);
    cpu->initialized_working_registers &= (uint16_t)~0x0002u;
    cpu->w[1] = 0xa587u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xa578u && cpu->sr == 0x0105u &&
               cpu->cycles == 1u && (cpu->initialized_working_registers & 0x0002u) == 0u,
           "SWAP.B exchanges low nibbles without initializing byte destination");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SWAP_W15);
    cpu->w[15] = 0xa586u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0x86a4u && cpu->sr == 0x0105u &&
               cpu->cycles == 1u,
           "SWAP keeps stack pointer even");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SWAP_BYTE_W15);
    cpu->w[15] = 0xa586u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0xa568u && cpu->sr == 0x0105u &&
               cpu->cycles == 1u,
           "SWAP.B keeps stack pointer even");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_EXCH_W1_W2);
    cpu->initialized_working_registers &= (uint16_t)~0x0006u;
    cpu->w[1] = 0x1234u;
    cpu->w[2] = 0xa5a5u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xa5a5u && cpu->w[2] == 0x1234u &&
               cpu->sr == 0x0105u && cpu->cycles == 1u &&
               (cpu->initialized_working_registers & 0x0006u) == 0x0006u,
           "EXCH swaps registers and initializes both destinations");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_EXCH_W1_W1);
    cpu->initialized_working_registers &= (uint16_t)~0x0002u;
    cpu->w[1] = 0xa5a5u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xa5a5u && cpu->sr == 0x0105u &&
               cpu->cycles == 1u && (cpu->initialized_working_registers & 0x0002u) != 0u,
           "EXCH accepts identical source and destination");
}

void dspic33_timing_test_direct_file_move_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_WORD_0X1000);
    dspic33_write_word(cpu, 0x1000u, 0x8000u);
    cpu->w[0] = 0x5a5au;
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x1000u) == 0x8000u &&
               cpu->w[0] == 0x5a5au && cpu->sr == 0x010du && cpu->cycles == 1u &&
               cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0x1000u &&
               cpu->io.cpu_write_width == 2u && cpu->io.cpu_write_previous == 0x8000u,
           "word file destination writes back RAM before updating flags");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_BYTE_0X1001);
    dspic33_write_word(cpu, 0x1000u, 0x8000u);
    cpu->w[0] = 0x5a5au;
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x1000u) == 0x8000u &&
               cpu->w[0] == 0x5a5au && cpu->sr == 0x010du && cpu->cycles == 1u &&
               cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0x1001u &&
               cpu->io.cpu_write_width == 1u && cpu->io.cpu_write_previous == 0x0080u,
           "byte file destination writes back RAM before updating flags");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_WORD_W0);
    cpu->w[0] = 0u;
    cpu->initialized_working_registers &= (uint16_t)~0x0001u;
    cpu->sr = 0x010du;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0u && cpu->sr == 0x0107u &&
               cpu->cycles == 1u && (cpu->initialized_working_registers & 0x0001u) != 0u &&
               cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0u &&
               cpu->io.cpu_write_width == 2u,
           "word file destination initializes working-register alias");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_BYTE_W0);
    cpu->w[0] = 0x0080u;
    cpu->initialized_working_registers &= (uint16_t)~0x0001u;
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x0080u && cpu->sr == 0x010du &&
               cpu->cycles == 1u && (cpu->initialized_working_registers & 0x0001u) == 0u &&
               cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0u &&
               cpu->io.cpu_write_width == 1u,
           "byte file destination preserves uninitialized working-register alias");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X1000_WREG);
    dspic33_write_word(cpu, 0x1000u, 0x8000u);
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x8000u && cpu->sr == 0x010du &&
               cpu->cycles == 1u && !cpu->io.cpu_write_valid,
           "word WREG destination does not write back source file");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_0X1001_WREG);
    dspic33_write_word(cpu, 0x1000u, 0x8000u);
    cpu->w[0] = 0x5a5au;
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x5a80u && cpu->sr == 0x010du &&
               cpu->cycles == 1u && !cpu->io.cpu_write_valid,
           "byte WREG destination does not write back source file");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_WORD_CORCON);
    cpu->sr = 0x010du;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->corcon == 0x0020u && cpu->sr == 0x0105u &&
               cpu->cycles == 1u && cpu->io.cpu_write_valid,
           "CPU SFR file destination writes back in one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_WORD_PORTB);
    dspic33_gpio_input(cpu, 1u, 0u);
    dspic33_write_word(cpu, 0x0e10u, 0xffffu);
    dspic33_write_word(cpu, 0x0e14u, 0xffffu);
    dspic33_write_word(cpu, 0x0e1eu, 0u);
    cpu->w[0] = 0x5a5au;
    cpu->sr = 0x010du;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0e14u) == 0u &&
               cpu->w[0] == 0x5a5au && cpu->sr == 0x0107u && cpu->cycles == 2u &&
               cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0x0e12u &&
               cpu->io.cpu_write_width == 2u,
           "non-CPU SFR word file destination writes pins back to latch");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_BYTE_PORTB);
    dspic33_gpio_input(cpu, 1u, 0u);
    dspic33_write_word(cpu, 0x0e10u, 0xffffu);
    dspic33_write_word(cpu, 0x0e14u, 0xffffu);
    dspic33_write_word(cpu, 0x0e1eu, 0u);
    cpu->w[0] = 0x5a5au;
    cpu->sr = 0x010du;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0e14u) == 0u &&
               cpu->w[0] == 0x5a5au && cpu->sr == 0x0107u && cpu->cycles == 2u &&
               cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0x0e12u &&
               cpu->io.cpu_write_width == 1u,
           "non-CPU SFR byte file destination writes pins back to latch");
}

void dspic33_timing_test_move_double_mode_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint32_t opcode;
        uint16_t initial_pointer;
        uint16_t access_address;
        uint16_t final_pointer;
        uint16_t low;
        uint16_t high;
        const char* name;
    } source_cases[] = {
        {OPCODE_MOV_DOUBLE_W4_POST_DECREMENT_W2, 0x5004u, 0x5004u, 0x5000u, 0x1112u, 0x2212u,
         "MOV.D source post-decrement"},
        {OPCODE_MOV_DOUBLE_W4_POST_INCREMENT_W2, 0x5000u, 0x5000u, 0x5004u, 0x1113u, 0x2213u,
         "MOV.D source post-increment"},
        {OPCODE_MOV_DOUBLE_W4_PRE_DECREMENT_W2, 0x5004u, 0x5000u, 0x5000u, 0x1114u, 0x2214u,
         "MOV.D source pre-decrement"},
        {OPCODE_MOV_DOUBLE_W4_PRE_INCREMENT_W2, 0x4ffcu, 0x5000u, 0x5000u, 0x1115u, 0x2215u,
         "MOV.D source pre-increment"},
    };
    static const struct {
        uint32_t opcode;
        uint16_t initial_pointer;
        uint16_t access_address;
        uint16_t final_pointer;
        uint16_t preserved_address;
        uint16_t low;
        uint16_t high;
        const char* name;
    } destination_cases[] = {
        {OPCODE_MOV_DOUBLE_W2_W4_POST_DECREMENT, 0x5004u, 0x5004u, 0x5000u, 0x5000u, 0x3312u,
         0x4412u, "MOV.D destination post-decrement"},
        {OPCODE_MOV_DOUBLE_W2_W4_POST_INCREMENT, 0x5000u, 0x5000u, 0x5004u, 0x5004u, 0x3313u,
         0x4413u, "MOV.D destination post-increment"},
        {OPCODE_MOV_DOUBLE_W2_W4_PRE_DECREMENT, 0x5004u, 0x5000u, 0x5000u, 0x5004u, 0x3314u,
         0x4414u, "MOV.D destination pre-decrement"},
        {OPCODE_MOV_DOUBLE_W2_W4_PRE_INCREMENT, 0x4ffcu, 0x5000u, 0x5000u, 0x4ffcu, 0x3315u,
         0x4415u, "MOV.D destination pre-increment"},
    };
    size_t index;

    for (index = 0u; index < sizeof(source_cases) / sizeof(source_cases[0]); index++) {
        reset_processor_test(cpu, 0u);
        load_instruction(state, cpu, 0u, source_cases[index].opcode);
        dspic33_write_word(cpu, 0x4ffcu, 0xdeadu);
        dspic33_write_word(cpu, 0x4ffeu, 0xdeadu);
        dspic33_write_word(cpu, 0x5000u, 0xdeadu);
        dspic33_write_word(cpu, 0x5002u, 0xdeadu);
        dspic33_write_word(cpu, 0x5004u, 0xdeadu);
        dspic33_write_word(cpu, 0x5006u, 0xdeadu);
        dspic33_write_word(cpu, source_cases[index].access_address, source_cases[index].low);
        dspic33_write_word(cpu, source_cases[index].access_address + 2u, source_cases[index].high);
        dspic33_set_working_register(cpu, 4u, source_cases[index].initial_pointer);
        cpu->sr = 0x010fu;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == source_cases[index].low &&
                   cpu->w[3] == source_cases[index].high &&
                   cpu->w[4] == source_cases[index].final_pointer && cpu->sr == 0x010fu &&
                   cpu->cycles == 2u,
               source_cases[index].name);
    }

    for (index = 0u; index < sizeof(destination_cases) / sizeof(destination_cases[0]); index++) {
        reset_processor_test(cpu, 0u);
        load_instruction(state, cpu, 0u, destination_cases[index].opcode);
        dspic33_write_word(cpu, 0x4ffcu, 0xdeadu);
        dspic33_write_word(cpu, 0x4ffeu, 0xdeadu);
        dspic33_write_word(cpu, 0x5000u, 0xdeadu);
        dspic33_write_word(cpu, 0x5002u, 0xdeadu);
        dspic33_write_word(cpu, 0x5004u, 0xdeadu);
        dspic33_write_word(cpu, 0x5006u, 0xdeadu);
        dspic33_set_working_register(cpu, 2u, destination_cases[index].low);
        dspic33_set_working_register(cpu, 3u, destination_cases[index].high);
        dspic33_set_working_register(cpu, 4u, destination_cases[index].initial_pointer);
        cpu->sr = 0x010fu;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_read_word(cpu, destination_cases[index].access_address) ==
                       destination_cases[index].low &&
                   dspic33_read_word(cpu, destination_cases[index].access_address + 2u) ==
                       destination_cases[index].high &&
                   dspic33_read_word(cpu, destination_cases[index].preserved_address) == 0xdeadu &&
                   cpu->w[4] == destination_cases[index].final_pointer && cpu->sr == 0x010fu &&
                   cpu->cycles == 2u,
               destination_cases[index].name);
    }

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x5000u, 0x5511u);
    dspic33_write_word(cpu, 0x5002u, 0x6622u);
    dspic33_set_working_register(cpu, 2u, 0x5000u);
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5511u && cpu->w[3] == 0x6622u &&
               cpu->sr == 0x010fu && cpu->cycles == 2u,
           "MOV.D overlapping load writes destination after source pointer update");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W2_POST_INCREMENT);
    dspic33_write_word(cpu, 0x5000u, 0xdeadu);
    dspic33_write_word(cpu, 0x5002u, 0xdeadu);
    dspic33_set_working_register(cpu, 2u, 0x5000u);
    dspic33_set_working_register(cpu, 3u, 0x7788u);
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5004u && cpu->w[3] == 0x7788u &&
               dspic33_read_word(cpu, 0x5000u) == 0x5000u &&
               dspic33_read_word(cpu, 0x5002u) == 0x7788u && cpu->sr == 0x010fu &&
               cpu->cycles == 2u,
           "MOV.D overlapping store captures source before destination pointer update");
}

void dspic33_timing_test_non_cpu_sfr_timing_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BSET_BYTE_IFS0_BIT_0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               (dspic33_read_word(cpu, 0x0800u) & 1u) != 0u,
           "non-CPU SFR bit RMW consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BSET_BYTE_IFS0_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    cpu->disicnt = 2u;
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->disicnt == 0u,
           "non-CPU SFR wait cycle completes DISI countdown");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u && cpu->w[15] == 0x5004u,
           "non-CPU SFR wait cycle releases new interrupt before next instruction");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_BSET_BYTE_IFS0_BIT_0);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    cpu->interrupt_depth = 1u;
    cpu->sr = 0x0060u;
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->interrupt_count == 0u,
           "nested non-CPU SFR wait retains new interrupt deferral");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x204u && cpu->interrupt_count == 0u,
           "nested new interrupt deferral spans following instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u && cpu->interrupt_count == 1u,
           "nested new interrupt dispatches after deferred instruction");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_POWER_SAVE_SLEEP);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0300u &&
               cpu->power_state == DSPIC33_POWER_ACTIVE && cpu->interrupt_count == 1u &&
               cpu->w[15] == 0x5004u && dspic33_read_word(cpu, 0x5000u) == 2u,
           "power-save instruction completes before wake dispatch");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               cpu->interrupt_count == 1u && cpu->w[15] == 0x5004u,
           "power-save wake dispatch executes handler");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_POWER_SAVE_SLEEP);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    dspic33_raise_interrupt(cpu, 0u);
    cpu->pc = 1u;
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               cpu->interrupt_count == 1u && cpu->w[15] == 0x5004u,
           "odd PC does not inherit preceding power-save dispatch ordering");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_IFS0_W2);
    dspic33_write_word(cpu, 0x0800u, 0x1234u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->w[2] == 0x1234u,
           "direct non-CPU SFR read consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0x0800u);
    dspic33_write_word(cpu, 0x0800u, 0x4321u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->w[1] == 0x0802u &&
               cpu->w[2] == 0x4321u,
           "indirect non-CPU SFR read consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W0_IFS0);
    cpu->w[0] = 0x2468u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
               dspic33_read_word(cpu, 0x0800u) == 0x2468u,
           "non-CPU SFR write remains one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BSET_BYTE_CORCON_BIT_1);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u && cpu->corcon == 0x0022u,
           "CPU SFR read-modify-write remains one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W4_W6);
    dspic33_set_working_register(cpu, 4u, 0x0800u);
    dspic33_write_word(cpu, 0x0800u, 0x1357u);
    dspic33_write_word(cpu, 0x0802u, 0x2468u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->w[6] == 0x1357u &&
               cpu->w[7] == 0x2468u,
           "double non-CPU SFR read retains two-cycle base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W4_W6);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1357u);
    dspic33_write_word(cpu, 0x1002u, 0x2468u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->w[6] == 0x1357u &&
               cpu->w[7] == 0x2468u,
           "double RAM read retains two-cycle base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W1_POST_INCREMENT);
    dspic33_set_working_register(cpu, 1u, 0x0800u);
    cpu->w[2] = 0x1357u;
    cpu->w[3] = 0x2468u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->w[1] == 0x0804u &&
               dspic33_read_word(cpu, 0x0800u) == 0x1357u &&
               dspic33_read_word(cpu, 0x0802u) == 0x2468u,
           "double non-CPU SFR write retains two-cycle base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->pc == 2u,
           "non-taken non-CPU SFR bit skip adds one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0800u, 1u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 3u && cpu->pc == 4u,
           "one-word non-CPU SFR bit skip adds one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0800u, 1u);
    cpu->cycles = UINT64_MAX - 1u;
    cpu->disicnt = 2u;
    dspic33_step(cpu);
    expect(state, cpu->cycles == UINT64_MAX - 1u && cpu->disicnt == 2u,
           "failed non-CPU SFR wait advance inhibits final cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0800u, 1u);
    cpu->cycles = UINT64_MAX - 2u;
    cpu->disicnt = 3u;
    cpu->pending_soft_traps[0].trap = 4u;
    cpu->pending_soft_traps[0].vector = 0x00000cu;
    cpu->pending_soft_traps[0].priority = 11u;
    cpu->pending_soft_traps[0].delay = 3u;
    cpu->pending_soft_traps[0].active = true;
    dspic33_step(cpu);
    expect(state,
           cpu->cycles == UINT64_MAX && cpu->disicnt == 1u && cpu->pending_soft_traps[0].active &&
               cpu->pending_soft_traps[0].delay == 3u,
           "failed final non-CPU SFR wait cycle inhibits trap bookkeeping");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 4u, 0u);
    dspic33_write_word(cpu, 0x0800u, 1u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u && cpu->pc == 6u,
           "two-word non-CPU SFR bit skip adds one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_PUSH_IFS0);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0800u, 0x55aau);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->w[15] == 0x5002u &&
               dspic33_read_word(cpu, 0x5000u) == 0x55aau,
           "non-CPU SFR PUSH source consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    dspic33_set_working_register(cpu, 9u, 0x0800u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    dspic33_write_word(cpu, 0x9000u, 0x1234u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u,
           "DSP X non-CPU SFR read adds one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CLEAR_TMR2);
    dspic33_write_word(cpu, 0x0110u, 0x0008u);
    dspic33_write_word(cpu, 0x010au, 0xaaaau);
    dspic33_write_word(cpu, 0x0108u, 0x5555u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
               dspic33_read_word(cpu, 0x0108u) == 0x5555u && cpu->data[0x0106u] == 0u &&
               cpu->data[0x0107u] == 0u,
           "CLR non-CPU SFR is a one-cycle pure write");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SET_TMR2);
    dspic33_write_word(cpu, 0x0110u, 0x0008u);
    dspic33_write_word(cpu, 0x010au, 0xaaaau);
    dspic33_write_word(cpu, 0x0108u, 0x5555u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
               dspic33_read_word(cpu, 0x0108u) == 0x5555u && cpu->data[0x0106u] == 0xffu &&
               cpu->data[0x0107u] == 0xffu,
           "SETM non-CPU SFR is a one-cycle pure write");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0x0056u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u && cpu->w[1] == 0x0058u &&
               cpu->w[2] == 0u,
           "unimplemented CPU SFR hole read remains one cycle");

    reset_processor_test(cpu, 0u);
    cpu->instruction_active = true;
    cpu->current_instruction_cycles = 1u;
    dspic33_read_word(cpu, 0x0800u);
    expect(state, !cpu->non_cpu_sfr_read && cpu->current_instruction_cycles == 1u,
           "raw non-CPU SFR read bypasses CPU instruction timing");
    cpu->instruction_active = false;
    cpu->current_instruction_cycles = 0u;

    cpu->non_cpu_sfr_read = true;
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->non_cpu_sfr_read, "reset clears non-CPU SFR timing state");
}

void dspic33_timing_test_psv_timing_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    expect(state, dspic33_load_program_word(cpu, 0x4000u, 0x001357u), "load PSV word timing value");
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    cpu->disicnt = 6u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1357u && cpu->cycles == 5u &&
               cpu->disicnt == 1u,
           "PSV word read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->w[2] = 0xa500u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xc001u && cpu->w[2] == 0xa557u &&
               cpu->cycles == 5u,
           "PSV byte read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xc000u && cpu->w[2] == 0x1357u &&
               cpu->cycles == 5u,
           "PSV pre-increment read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W0_OFFSET_W2);
    dspic33_set_working_register(cpu, 0u, 2u);
    dspic33_set_working_register(cpu, 1u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 2u && cpu->w[1] == 0xbffeu &&
               cpu->w[2] == 0x1357u && cpu->cycles == 5u,
           "PSV indexed read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W4_LITERAL_2_W2);
    dspic33_set_working_register(cpu, 4u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1357u && cpu->w[4] == 0xbffeu &&
               cpu->cycles == 5u,
           "PSV literal-offset read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X9000_W2);
    expect(state, dspic33_load_program_word(cpu, 0x1000u, 0x005a5au),
           "load direct PSV timing value");
    cpu->dsrpag = 0x0200u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5a5au && cpu->cycles == 5u,
           "direct PSV read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W4_W6);
    expect(state, dspic33_load_program_word(cpu, 0x4002u, 0x002468u),
           "load second PSV double timing value");
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[6] == 0x1357u && cpu->w[7] == 0x2468u &&
               cpu->cycles == 5u,
           "double PSV read consumes five total cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5);
    dspic33_set_working_register(cpu, 2u, 1u);
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0xc002u && cpu->w[5] == 0x1358u &&
               cpu->cycles == 5u,
           "arithmetic PSV source consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    dspic33_write_word(cpu, 0xc000u, 0x7777u);
    cpu->dsrpag = 1u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x7777u && cpu->cycles == 1u,
           "EDS data read retains base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    expect(state, dspic33_load_program_word(cpu, 0x4000u, 0xab1357u),
           "load PSV high-byte timing value");
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0300u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x00abu && cpu->cycles == 5u,
           "PSV high-byte read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    expect(state, dspic33_load_program_word(cpu, 0x4004u, 0x001356u), "load clear PSV skip bit");
    dspic33_set_working_register(cpu, 4u, 0xc004u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u && cpu->w[4] == 0xc006u &&
               cpu->cycles == 5u,
           "untaken PSV bit skip consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->w[4] == 0xc002u &&
               cpu->cycles == 5u,
           "one-word PSV bit skip remains five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 4u, 0u);
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 6u && cpu->w[4] == 0xc002u &&
               cpu->cycles == 5u,
           "two-word PSV bit skip remains five cycles");

    reset_processor_test(cpu, 0u);
    cpu->instruction_active = true;
    cpu->psv_read = false;
    dspic33_read_word(cpu, PSV_TEST_ADDRESS);
    expect(state, !cpu->psv_read, "raw PSV read bypasses CPU instruction timing");
    cpu->instruction_active = false;

    cpu->psv_read = true;
    dspic33_reset(cpu, 0u);
    expect(state, !cpu->psv_read, "reset clears PSV timing state");
}
