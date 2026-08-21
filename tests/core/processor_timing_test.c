#include "processor_test_support.h"

static void call_stack_timing_case(TestState* state, Dspic33* cpu) {
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

static void instruction_cycle_cases(TestState* state, Dspic33* cpu) {
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

static void register_move_instruction_cases(TestState* state, Dspic33* cpu) {
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

static void direct_file_move_cases(TestState* state, Dspic33* cpu) {
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

static void move_double_mode_cases(TestState* state, Dspic33* cpu) {
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

static void non_cpu_sfr_timing_cases(TestState* state, Dspic33* cpu) {
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

static void psv_timing_cases(TestState* state, Dspic33* cpu) {
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

static void psv_repeat_timing_cases(TestState* state, Dspic33* cpu) {
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

static void psv_program_hole_cases(TestState* state, Dspic33* cpu) {
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

static void address_register_dependency_cases(TestState* state, Dspic33* cpu) {
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

static void dsp_x_prefetch_page_cases(TestState* state, Dspic33* cpu) {
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

static void dsp_prefetch_address_error_cases(TestState* state, Dspic33* cpu) {
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

static void dsp_program_hole_prefetch_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_NO_UPDATE_Y_W10_DECREMENT);
    expect(state, dspic33_load_program_word(cpu, 0x557feu, 0x001357u),
           "load last implemented DSP PSV word");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xd7feu);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 0x6789u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x020au;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->accumulator[0] == 12 &&
               cpu->w[4] == 0x1357u && cpu->w[5] == 0x6789u && cpu->w[8] == 0xd7feu &&
               cpu->w[10] == 0x9000u && cpu->dsrpag == 0x020au && cpu->cycles == 5u &&
               cpu->trap_count == 0u,
           "last implemented DSP PSV word remains readable");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_NO_UPDATE_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xd800u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 0x6789u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "DSP X program-hole access traps");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0u && cpu->w[5] == 0x6789u &&
               cpu->w[8] == 0xd800u && cpu->w[10] == 0x9000u && cpu->dsrpag == 0x020au &&
               cpu->cycles == 5u && cpu->w[15] == 0x5004u,
           "DSP X program-hole access preserves completed state and PSV timing");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    expect(state, dspic33_load_program_word(cpu, 0x557feu, 0x002468u),
           "load DSP PSV post-update boundary word");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xd7feu);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 0x789au);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "DSP X post-update into program hole traps");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0x2468u && cpu->w[5] == 0x789au &&
               cpu->w[8] == 0xd800u && cpu->w[10] == 0x9000u && cpu->dsrpag == 0x020au &&
               cpu->cycles == 5u,
           "DSP X post-update trap completes fetches and pointer state");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_NO_UPDATE_Y_W10_DECREMENT);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xd800u);
    dspic33_set_working_register(cpu, 10u, 0xe000u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "DSP program-hole and invalid Y faults coalesce");
    expect(state,
           cpu->accumulator[0] == 12 && cpu->w[4] == 0u && cpu->w[5] == 0u &&
               cpu->w[8] == 0xd800u && cpu->w[10] == 0xdffeu && cpu->cycles == 5u &&
               cpu->trap_count == 1u,
           "DSP dual fault completes once with PSV timing");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_CLEAR_DIRECT);
    dspic33_set_working_register(cpu, 8u, 0xd800u);
    cpu->accumulator[0] = 0x123456u;
    cpu->accumulator[1] = 0x654321u;
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "CLR with DSP X program-hole prefetch traps");
    expect(state,
           cpu->accumulator[0] == 0 && cpu->accumulator[1] == 0x654321 && cpu->w[4] == 0u &&
               cpu->w[8] == 0xd806u && cpu->w[13] == 0x0065u && cpu->cycles == 5u,
           "CLR program-hole trap completes accumulator prefetch and write-back");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_MOVSAC_WRITE_BACK);
    dspic33_set_working_register(cpu, 4u, 1u);
    dspic33_set_working_register(cpu, 5u, 1u);
    dspic33_set_working_register(cpu, 9u, 0xd800u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    dspic33_set_working_register(cpu, 13u, 0x5100u);
    dspic33_write_word(cpu, 0x5100u, 0xa55au);
    dspic33_write_word(cpu, 0x9000u, 0x2468u);
    cpu->accumulator[0] = 0x123456u;
    cpu->accumulator[1] = 0x654321u;
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "MOVSAC with DSP X program-hole prefetch traps");
    expect(state,
           cpu->accumulator[0] == 0x123456 && cpu->accumulator[1] == 0x654321 && cpu->w[4] == 0u &&
               cpu->w[5] == 0x2468u && cpu->w[9] == 0xd7feu && cpu->w[13] == 0x5102u &&
               dspic33_read_word(cpu, 0x5100u) == 0xa55au && cpu->cycles == 5u,
           "MOVSAC program-hole trap completes lanes and inhibits memory write-back");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_DSP_ED);
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 8u, 0xd800u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 2u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x020au;
    expect_address_trap(state, cpu, "ED with DSP X program-hole prefetch traps");
    expect(state,
           cpu->accumulator[0] == 9 && cpu->w[4] == 0xfffeu && cpu->w[8] == 0xd802u &&
               cpu->w[10] == 0x9000u && cpu->cycles == 5u,
           "ED program-hole trap completes distance and pointer state");
}

static void move_double_stack_timing_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
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

    reset_processor_test(cpu, 0u);
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
           (dspic33_read_word(cpu, 0x08c0u) & 0x0004u) == 0u && active_pending_traps(cpu) == 0u,
           "MOV.D derived high word leaves STKERR clear");
}

static void return_instruction_cycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETURN);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETURN without pending exception consumes six cycles");

    reset_processor_test(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_RETURN);
    load_instruction(state, cpu, 0x000300u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "RETURN with pending exception consumes five cycles");
    expect(state, cpu->w[1] == 0u && cpu->last_trap_return == 0x000300u,
           "RETURN stack fault precedes restored instruction");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X123_W2);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETLW without pending exception consumes six cycles");
    expect(state, cpu->w[2] == 0x0123u, "RETLW writes return literal");

    reset_processor_test(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X123_W2);
    load_instruction(state, cpu, 0x000300u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "RETLW with pending exception consumes five cycles");
    expect(state, cpu->w[2] == 0x0123u && cpu->w[1] == 0u && cpu->last_trap_return == 0x000300u,
           "RETLW stack fault completes literal and precedes target");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X122_W15);
    load_instruction(state, cpu, 0x000300u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u,
           "RETLW W15 restores PC from original stack frame");
    expect(state, cpu->w[15] == 0x0122u && cpu->cycles == 6u,
           "RETLW W15 writes even literal after frame pop");
}

static void retfie_stack_timing_case(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000300u, OPCODE_MOV_SENTINEL_W1);
    dspic33_write_word(cpu, 0x0020u, 0x5000u);
    dspic33_write_word(cpu, 0x5000u, 0x0300u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "RETFIE stack fault matures within RETFIE cycles");
    expect(state, cpu->w[1] == 0u, "RETFIE stack fault precedes restored instruction");
    expect(state, cpu->last_trap_return == 0x000300u && cpu->pc == 0x000200u,
           "RETFIE stack fault stacks restored PC");
}

static void interrupt_stack_timing_case(TestState* state, Dspic33* cpu) {
    const Dspic33PendingSoftTrap* pending;
    reset_processor_test(cpu, 0u);
    cpu->stop_on_trap = true;
    load_instruction(state, cpu, 0x00000au, 0x000200u);
    load_instruction(state, cpu, 0x000014u, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_MOV_W0_SPLIM);
    load_instruction(state, cpu, 0x000302u, OPCODE_MOV_SENTINEL_W1);
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
           cpu->pc == 0x000302u && cpu->splim == 0x5100u && pending != NULL && pending->delay == 1u,
           "IRQ stack fault retains second handler boundary");
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "IRQ stack fault traps after second handler instruction");
    expect(state, cpu->w[1] == 0x1111u && cpu->last_trap_return == 0x000304u,
           "IRQ stack fault stacks third handler PC");
}

static void invalid_move_double_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint32_t opcode;
        const char* execution;
    } cases[] = {
        {OPCODE_MOV_DOUBLE_INVALID_SOURCE_MODE_6, "MOV.D source mode 6 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_SOURCE_MODE_7, "MOV.D source mode 7 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_DESTINATION_MODE_6, "MOV.D destination mode 6 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_DESTINATION_MODE_7, "MOV.D destination mode 7 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_MEMORY_PAIR, "MOV.D memory pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_ODD_SOURCE_PAIR,
         "MOV.D odd source register pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_ODD_DESTINATION_PAIR,
         "MOV.D odd destination register pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_REVERSE_ODD_SOURCE_PAIR,
         "MOV.D reverse odd source register pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_REVERSE_DIRECT,
         "MOV.D reverse direct destination resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_DIRECTION_BIT, "MOV.D reserved direction bit resets processor"},
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        dspic33_reset(cpu, 0u);
        load_instruction(state, cpu, 0u, cases[index].opcode);
        dspic33_set_working_register(cpu, 0u, 2u);
        dspic33_set_working_register(cpu, 1u, 0x5000u);
        dspic33_set_working_register(cpu, 2u, 0x1111u);
        dspic33_set_working_register(cpu, 3u, 0x2222u);
        dspic33_write_word(cpu, 0x5000u, 0xaaaau);
        dspic33_write_word(cpu, 0x5002u, 0x5555u);
        dspic33_write_word(cpu, 0x5004u, 0x3333u);
        expect_illegal_reset(state, cpu, cases[index].execution);
        expect(state,
               dspic33_read_word(cpu, 0x5000u) == 0xaaaau &&
                   dspic33_read_word(cpu, 0x5002u) == 0x5555u &&
                   dspic33_read_word(cpu, 0x5004u) == 0x3333u,
               "invalid MOV.D preserves destination memory");
    }

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W4_PRE_INCREMENT);
    dspic33_set_working_register(cpu, 2u, 0x1111u);
    dspic33_set_working_register(cpu, 3u, 0x2222u);
    dspic33_set_working_register(cpu, 4u, 0x4ffcu);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0x5000u &&
               dspic33_read_word(cpu, 0x5000u) == 0x1111u &&
               dspic33_read_word(cpu, 0x5002u) == 0x2222u && cpu->illegal_reset_count == 0u,
           "MOV.D destination mode 5 remains valid");
}

static void invalid_dsp_encoding_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint32_t opcode;
        const char* execution;
    } cases[] = {
        {0xc30113u, "CLR write-back encoding 3 resets processor"},
        {0xc70113u, "MOVSAC write-back encoding 3 resets processor"},
        {0xc34110u, "CLR reserved operation encoding 0 resets processor"},
        {0xc34111u, "CLR reserved operation encoding 1 resets processor"},
        {0xc34112u, "CLR reserved operation encoding 2 resets processor"},
        {0xc34113u, "CLR reserved operation encoding 3 resets processor"},
        {0xc74110u, "MOVSAC reserved operation encoding 0 resets processor"},
        {0xc74111u, "MOVSAC reserved operation encoding 1 resets processor"},
        {0xc74112u, "MOVSAC reserved operation encoding 2 resets processor"},
        {0xc74113u, "MOVSAC reserved operation encoding 3 resets processor"},
        {0xf00112u, "square EDAC operation encoding resets processor"},
        {0xf00113u, "square ED operation encoding resets processor"},
        {0xf0405cu, "EDAC square operation encoding resets processor"},
        {0xf0405du, "ED square operation encoding resets processor"},
        {0xf0445eu, "EDAC reserved destination encoding 1 resets processor"},
        {0xf0485eu, "EDAC reserved destination encoding 2 resets processor"},
        {0xf04c5eu, "EDAC reserved destination encoding 3 resets processor"},
        {0xf0445fu, "ED reserved destination encoding 1 resets processor"},
        {0xf0485fu, "ED reserved destination encoding 2 resets processor"},
        {0xf04c5fu, "ED reserved destination encoding 3 resets processor"},
        {0xf0411eu, "EDAC missing X prefetch resets processor"},
        {0xf04052u, "EDAC missing Y prefetch resets processor"},
        {0xf04112u, "EDAC missing both prefetches resets processor"},
        {0xf0411fu, "ED missing X prefetch resets processor"},
        {0xf04053u, "ED missing Y prefetch resets processor"},
        {0xf04113u, "ED missing both prefetches resets processor"},
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        dspic33_reset(cpu, 0u);
        load_instruction(state, cpu, 0u, cases[index].opcode);
        dspic33_set_working_register(cpu, 4u, 0x1111u);
        dspic33_set_working_register(cpu, 5u, 0x2222u);
        dspic33_set_working_register(cpu, 8u, 0x5000u);
        dspic33_set_working_register(cpu, 10u, 0x5002u);
        dspic33_set_working_register(cpu, 13u, 0x5004u);
        cpu->accumulator[0] = 0x123456789a;
        cpu->accumulator[1] = -0x123456789a;
        dspic33_write_word(cpu, 0x5000u, 0xaaaau);
        dspic33_write_word(cpu, 0x5002u, 0x5555u);
        dspic33_write_word(cpu, 0x5004u, 0x3333u);
        expect_illegal_reset(state, cpu, cases[index].execution);
        expect(state,
               dspic33_read_word(cpu, 0x5000u) == 0xaaaau &&
                   dspic33_read_word(cpu, 0x5002u) == 0x5555u &&
                   dspic33_read_word(cpu, 0x5004u) == 0x3333u,
               "invalid DSP encoding preserves data memory");
    }
}

static void valid_dsp_register_pair_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint32_t opcode;
        int64_t result;
        const char* execution;
    } cases[] = {
        {0xc00113u, -196602ll, "MPY maps W4 times W5"},
        {0xc10113u, 2147549180ll, "MPY maps W4 times W6"},
        {0xc20113u, -2147221510ll, "MPY maps W4 times W7"},
        {0xc40113u, -98310ll, "MPY maps W5 times W6"},
        {0xc50113u, 98295ll, "MPY maps W5 times W7"},
        {0xc60113u, -1073709050ll, "MPY maps W6 times W7"},
        {0xf00111u, 4294705156ll, "MPY maps W4 square"},
        {0xf10111u, 9ll, "MPY maps W5 square"},
        {0xf20111u, 1073872900ll, "MPY maps W6 square"},
        {0xf30111u, 1073545225ll, "MPY maps W7 square"},
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        uint16_t expected_status =
            cases[index].result < INT32_MIN || cases[index].result > INT32_MAX ? 0x880fu : 0x000fu;
        dspic33_reset(cpu, 0u);
        load_instruction(state, cpu, 0u, cases[index].opcode);
        dspic33_set_working_register(cpu, 4u, 0xfffeu);
        dspic33_set_working_register(cpu, 5u, 0xfffdu);
        dspic33_set_working_register(cpu, 6u, 0x8002u);
        dspic33_set_working_register(cpu, 7u, 0x8003u);
        cpu->corcon = 0x2001u;
        cpu->sr = 0x000fu;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
                   cpu->accumulator[0] == cases[index].result && cpu->accumulator[1] == 0,
               cases[index].execution);
        expect(state,
               cpu->corcon == 0x2001u && cpu->sr == expected_status && cpu->w[4] == 0xfffeu &&
                   cpu->w[5] == 0xfffdu && cpu->w[6] == 0x8002u && cpu->w[7] == 0x8003u &&
                   cpu->illegal_reset_count == 0u,
               "DSP register pair preserves control, status, and operands");
    }
}

static void dsp_prefetch_destination_collision_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint32_t opcode;
        uint8_t destination;
        const char* execution;
    } cases[] = {
        {OPCODE_DSP_PREFETCH_W4_COLLISION, 4u, "DSP prefetch collision resolves W4"},
        {OPCODE_DSP_PREFETCH_W5_COLLISION, 5u, "DSP prefetch collision resolves W5"},
        {OPCODE_DSP_PREFETCH_W6_COLLISION, 6u, "DSP prefetch collision resolves W6"},
        {OPCODE_DSP_PREFETCH_W7_COLLISION, 7u, "DSP prefetch collision resolves W7"},
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        dspic33_reset(cpu, 0u);
        load_instruction(state, cpu, 0u, cases[index].opcode);
        dspic33_set_working_register(cpu, 4u, 3u);
        dspic33_set_working_register(cpu, 5u, 4u);
        dspic33_set_working_register(cpu, 6u, 0x6666u);
        dspic33_set_working_register(cpu, 7u, 0x7777u);
        dspic33_set_working_register(cpu, 8u, 0x5000u);
        dspic33_set_working_register(cpu, 10u, 0x9002u);
        dspic33_write_word(cpu, 0x5000u, 0x1111u);
        dspic33_write_word(cpu, 0x9002u, 0x2222u);
        cpu->corcon = 0x0001u;
        cpu->sr = 0x000fu;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
                   cpu->accumulator[0] == 12 && cpu->w[cases[index].destination] == 0x2222u,
               cases[index].execution);
        expect(state,
               cpu->w[8] == 0x5002u && cpu->w[10] == 0x9004u && cpu->sr == 0x000fu &&
                   cpu->corcon == 0x0001u && cpu->illegal_reset_count == 0u,
               "DSP prefetch collision completes both lanes and preserves control state");
    }

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_MOVSAC_W4_COLLISION);
    dspic33_set_working_register(cpu, 4u, 0x4444u);
    dspic33_set_working_register(cpu, 8u, 0x5000u);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x5000u, 0x1111u);
    dspic33_write_word(cpu, 0x9002u, 0x2222u);
    cpu->accumulator[0] = 0x123456789a;
    cpu->accumulator[1] = -0x123456789a;
    cpu->corcon = 0x0001u;
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u && cpu->w[4] == 0x2222u &&
               cpu->w[8] == 0x5002u && cpu->w[10] == 0x9004u,
           "MOVSAC prefetch collision resolves W4 after both lanes complete");
    expect(state,
           cpu->accumulator[0] == 0x123456789a && cpu->accumulator[1] == -0x123456789a &&
               cpu->sr == 0x010fu && cpu->corcon == 0x0001u && cpu->illegal_reset_count == 0u,
           "MOVSAC prefetch collision preserves accumulators and control state");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "processor timing test initializes");
    if (initialized) {
        instruction_cycle_cases(&state, &cpu);
        register_move_instruction_cases(&state, &cpu);
        direct_file_move_cases(&state, &cpu);
        move_double_mode_cases(&state, &cpu);
        non_cpu_sfr_timing_cases(&state, &cpu);
        psv_timing_cases(&state, &cpu);
        psv_repeat_timing_cases(&state, &cpu);
        psv_program_hole_cases(&state, &cpu);
        address_register_dependency_cases(&state, &cpu);
        dsp_x_prefetch_page_cases(&state, &cpu);
        dsp_prefetch_address_error_cases(&state, &cpu);
        dsp_program_hole_prefetch_cases(&state, &cpu);
        call_stack_timing_case(&state, &cpu);
        move_double_stack_timing_cases(&state, &cpu);
        return_instruction_cycle_cases(&state, &cpu);
        retfie_stack_timing_case(&state, &cpu);
        interrupt_stack_timing_case(&state, &cpu);
        invalid_move_double_cases(&state, &cpu);
        invalid_dsp_encoding_cases(&state, &cpu);
        valid_dsp_register_pair_cases(&state, &cpu);
        dsp_prefetch_destination_collision_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
