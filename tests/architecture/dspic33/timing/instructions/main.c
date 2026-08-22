#include "architecture/dspic33/timing/instructions/internal.h"

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
        dspic33_timing_test_instruction_cycle_cases(&state, &cpu);
        dspic33_timing_test_register_move_instruction_cases(&state, &cpu);
        dspic33_timing_test_direct_file_move_cases(&state, &cpu);
        dspic33_timing_test_move_double_mode_cases(&state, &cpu);
        dspic33_timing_test_non_cpu_sfr_timing_cases(&state, &cpu);
        dspic33_timing_test_psv_timing_cases(&state, &cpu);
        dspic33_timing_test_psv_repeat_timing_cases(&state, &cpu);
        dspic33_timing_test_psv_program_hole_cases(&state, &cpu);
        dspic33_timing_test_address_register_dependency_cases(&state, &cpu);
        dspic33_timing_test_dsp_x_prefetch_page_cases(&state, &cpu);
        dspic33_timing_test_dsp_prefetch_address_error_cases(&state, &cpu);
        dsp_program_hole_prefetch_cases(&state, &cpu);
        dspic33_timing_test_call_stack_timing_case(&state, &cpu);
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
