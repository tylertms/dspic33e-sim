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
    OPCODE_CALL_0X55800 = 0x025800u,
    OPCODE_GOTO_0X100 = 0x040100u,
    OPCODE_GOTO_0X55800 = 0x045800u,
    OPCODE_CALL_W0 = 0x010000u,
    OPCODE_GOTO_W0 = 0x010400u,
    OPCODE_CALL_LONG_W0 = 0x018800u,
    OPCODE_GOTO_LONG_W0 = 0x018c00u,
    OPCODE_RCALL_W0 = 0x010200u,
    OPCODE_BRA_W0 = 0x010600u,
    OPCODE_RCALL_NEXT = 0x070000u,
    OPCODE_RCALL_0X55800 = 0x07000au,
    OPCODE_BRA_0X55800 = 0x370012u,
    OPCODE_BRA_Z_0X55800 = 0x320012u,
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
    OPCODE_MOV_BYTE_W2_W1_POST_INCREMENT = 0x785882u,
    OPCODE_MOV_W1_W4_LITERAL_2 = 0x980211u,
    OPCODE_MOV_W4_LITERAL_2_W2 = 0x900114u,
    OPCODE_MOV_0X9000_W2 = 0x848002u,
    OPCODE_MOV_W2_0X9000 = 0x8c8002u,
    OPCODE_MOV_DOUBLE_W2_W1_POST_INCREMENT = 0xbe9882u,
    OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2 = 0xbe0131u,
    OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT = 0x4112b4u,
    OPCODE_ADD_W2_W4_POST_INCREMENT_W5 = 0x4102b4u,
    OPCODE_COMPARE_ZERO_W4_POST_INCREMENT = 0xe00034u,
    OPCODE_ACCUMULATOR_ADD_W4_POST_INCREMENT = 0xc90034u,
    OPCODE_NEG_W4_POST_INCREMENT_W5_POST_DECREMENT = 0xea12b4u,
    OPCODE_ASR_W4_POST_INCREMENT_W5_POST_DECREMENT = 0xd192b4u,
    OPCODE_BSET_BYTE_W4_POST_DECREMENT = 0xa07424u,
    OPCODE_BSET_WORD_W4_POST_INCREMENT = 0xa09034u,
    OPCODE_TBLRDL_W2_W3 = 0xba0192u,
    OPCODE_TBLRDL_W2_W4_POST_INCREMENT = 0xba1a12u,
    OPCODE_TBLRDL_W2_POST_INCREMENT_W4_POST_INCREMENT = 0xba1a32u,
    OPCODE_TBLRDL_W2_W15_POST_INCREMENT = 0xba1f92u,
    OPCODE_TBLRDL_BYTE_W2_W3 = 0xba4192u,
    OPCODE_TBLRDH_W2_W3 = 0xba8192u,
    OPCODE_TBLRDH_BYTE_W2_W3 = 0xbac192u,
    OPCODE_TBLWTL_W2_W3 = 0xbb0982u,
    OPCODE_MOV_W0_SPLIM = 0x880100u,
    OPCODE_MOV_SENTINEL_W1 = 0x211111u,
    OPCODE_MOV_W0_W2 = 0x780110u,
    OPCODE_MOV_W1_W2 = 0x780111u,
    OPCODE_MOV_W15_W2 = 0x78011fu,
    OPCODE_MOV_W2_W1 = 0x780882u,
    OPCODE_MOV_BYTE_LITERAL_W1 = 0xb3c001u,
    OPCODE_MOVPAG_TBL_LITERAL = 0xfec8a5u,
    OPCODE_MOVPAG_INVALID_LITERAL = 0xfecc00u,
    OPCODE_MOVPAG_TBL_W1 = 0xfed801u,
    OPCODE_MOVPAG_INVALID_W1 = 0xfedc01u,
    OPCODE_ILLEGAL = 0x3f0000u,
    OPCODE_REPEAT_W0 = 0x098000u,
    OPCODE_DO_0 = 0x080000u,
    OPCODE_DO_W0 = 0x088000u,
    OPCODE_PUSH_SHADOW = 0xfea000u,
    OPCODE_DSP_INDEXED = 0xc00732u,
    OPCODE_DSP_DIRECT_W13 = 0xc00110u,
    OPCODE_DSP_WRITE_BACK = 0xc393b1u
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

static void reset_processor_conformance(Dspic33* cpu, uint32_t entry) {
    uint8_t reg;
    dspic33_reset(cpu, entry);
    for (reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(cpu, reg, cpu->w[reg]);
    }
}

static void expect_illegal_reset(ProcessorConformance* state, Dspic33* cpu,
                                 const char* execution) {
    uint8_t reg;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, execution);
    expect(state,
           cpu->illegal_reset && cpu->illegal_reset_count == 1u &&
               cpu->software_reset_count == 0u && cpu->pc == 0u,
           "illegal condition performs warm reset");
    expect(state,
           (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
               cpu->last_trap == UINT16_MAX && cpu->trap_count == 0u,
           "illegal condition records reset without trap");
    for (reg = 0u; reg < 15u; reg++) {
        expect(state, cpu->w[reg] == 0u, "illegal reset clears working register");
    }
    expect(state,
           cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u,
           "illegal reset restores stack and initialization state");
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

static void program_target_address_error_cases(ProcessorConformance* state,
                                               Dspic33* cpu) {
    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X55800);
    load_instruction(state, cpu, 2u, 0x000005u);
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented literal CALL target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5008u &&
               dspic33_read_word(cpu, 0x5000u) == 5u &&
               dspic33_read_word(cpu, 0x5004u) == 2u && (cpu->corcon & 0x0004u) == 0u,
           "literal CALL completes return push before target trap");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_LONG_W0);
    cpu->w[0] = 0x5800u;
    cpu->w[1] = 0x0005u;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented GOTO.L target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u &&
               (cpu->corcon & 0x0004u) != 0u,
           "GOTO.L target trap preserves registers and SFA");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_RETURN);
    dspic33_write_word(cpu, 0x5000u, 0x5800u);
    dspic33_write_word(cpu, 0x5002u, 0x0005u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 5u,
           "unimplemented RETURN target traps in five cycles");
    expect(state,
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u &&
               cpu->call_depth == 0u && (cpu->corcon & 0x0004u) == 0u,
           "RETURN completes frame pop before target trap");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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
           cpu->last_trap_return == 2u && cpu->w[2] == 0x0123u &&
               cpu->w[15] == 0x5004u && cpu->call_depth == 0u,
           "RETLW completes frame pop and literal before target trap");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0x557eau, OPCODE_RCALL_0X55800);
    cpu->pc = 0x557eau;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented literal RCALL target traps in four cycles");
    expect(state,
           cpu->last_trap_return == 0x557ecu && cpu->w[15] == 0x5008u &&
               dspic33_read_word(cpu, 0x5000u) == 0x57edu &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u &&
               (cpu->corcon & 0x0004u) == 0u,
           "literal RCALL completes return push before target trap");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_Z_0X55800);
    cpu->pc = 0x557dau;
    cpu->sr |= 0x0002u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "taken conditional BRA validates target in four cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_Z_0X55800);
    cpu->pc = 0x557dau;
    cpu->sr &= (uint16_t)~0x0002u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557dcu &&
               cpu->cycles == 1u && cpu->last_trap == UINT16_MAX,
           "untaken conditional BRA skips target validation in one cycle");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_0X100);
    load_instruction(state, cpu, 2u, 0u);
    load_instruction(state, cpu, 0x100u, OPCODE_NOP);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x100u &&
               cpu->cycles == 4u,
           "implemented literal GOTO target remains valid");

    reset_processor_conformance(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_GOTO_0X100);
    load_instruction(state, cpu, 0x100u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x100u &&
               cpu->cycles == 4u && cpu->w[15] == 0x5000u &&
               dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
               dspic33_read_word(cpu, 0x5002u) == 0x5a5au &&
               (cpu->corcon & 0x0004u) != 0u && cpu->sequential_program_hole_pc == 0u,
           "boundary GOTO reads zero extension without sequential provenance");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X100);
    load_instruction(state, cpu, 2u, 0u);
    load_instruction(state, cpu, 0x100u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x100u &&
               cpu->cycles == 4u && dspic33_read_word(cpu, 0x5000u) == 4u,
           "implemented literal CALL target remains valid");

    reset_processor_conformance(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_CALL_0X100);
    load_instruction(state, cpu, 0x100u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x100u &&
               cpu->cycles == 4u && cpu->w[15] == 0x5004u && cpu->call_depth == 1u &&
               dspic33_read_word(cpu, 0x5000u) == 0x5803u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u &&
               (cpu->corcon & 0x0004u) == 0u && cpu->sequential_program_hole_pc == 0u,
           "boundary CALL stacks hole return and clears sequential provenance");

    reset_processor_conformance(cpu, 0x557feu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557feu;
    load_instruction(state, cpu, 0x557feu, OPCODE_CALL_0X100);
    load_instruction(state, cpu, 0x100u, OPCODE_RETURN);
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x100u &&
               cpu->cycles == 4u && cpu->sequential_program_hole_pc == 0u,
           "boundary CALL prepares explicit return to program hole");
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 9u &&
               cpu->last_trap == 1u && cpu->last_trap_return == 0x102u &&
               cpu->call_depth == 0u && cpu->w[15] == 0x5004u &&
               cpu->sequential_program_hole_pc == 0u,
           "boundary CALL return validates hole target without provenance reuse");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_W0);
    load_instruction(state, cpu, 0x100u, OPCODE_NOP);
    cpu->w[0] = 0x100u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x100u &&
               cpu->cycles == 4u,
           "implemented GOTO Wn target remains valid");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_LONG_W0);
    cpu->w[0] = 0xc000u;
    cpu->w[1] = 0x007fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x7fc000u &&
               cpu->cycles == 4u && cpu->last_trap == UINT16_MAX,
           "auxiliary GOTO.L target retains existing bounds behavior");
    expect(state, dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS,
           "auxiliary target remains outside implemented fetch space");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_W0);
    load_instruction(state, cpu, 0x100u, OPCODE_NOP);
    cpu->w[0] = 0x100u;
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x100u &&
               cpu->cycles == 4u && dspic33_read_word(cpu, 0x5000u) == 2u,
           "implemented CALL Wn target remains valid");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0x557dau, OPCODE_BRA_W0);
    cpu->pc = 0x557dau;
    cpu->w[0] = 0x0012u;
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557dau;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented BRA Wn target traps in four cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0x557eau, OPCODE_RCALL_W0);
    cpu->pc = 0x557eau;
    cpu->w[0] = 0x000au;
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557eau;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 4u,
           "unimplemented RCALL Wn target traps in four cycles");
}

static void program_read_address_error_cases(ProcessorConformance* state,
                                             Dspic33* cpu) {
    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x0200u, 0xabcdefu);
    cpu->tblpag = 0u;
    cpu->w[2] = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[3] == 0xcdefu &&
               cpu->cycles == 2u,
           "implemented table read consumes two cycles");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[3] = 0xa5a5u;
    cpu->corcon |= 0x0004u;
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 2u,
           "table read from unimplemented main program traps in two cycles");
    expect(state,
           cpu->w[2] == 0x5800u && cpu->w[3] == 0u && cpu->last_trap_return == 2u &&
               cpu->w[15] == 0x5004u && (cpu->corcon & 0x0004u) != 0u,
           "unimplemented table read returns zero and completes pointer state");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W4_POST_INCREMENT);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[4] = 0x1000u;
    dspic33_write_word(cpu, 0x1000u, 0xa5a5u);
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "unimplemented table read with indirect destination traps");
    expect(state, cpu->w[4] == 0x1002u && dspic33_read_word(cpu, 0x1000u) == 0u,
           "table read trap completes pointer update and zero result write");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDH_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5800u &&
               cpu->w[3] == 0u && cpu->cycles == 2u,
           "unimplemented high table word read returns zero in two cycles");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDH_BYTE_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5800u &&
               cpu->w[3] == 0xa500u && cpu->cycles == 2u,
           "unimplemented high table byte read clears the low byte in two cycles");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_BYTE_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5801u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5801u &&
               cpu->w[3] == 0xa500u && cpu->cycles == 2u,
           "unimplemented low table byte read accepts odd source in two cycles");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_POST_INCREMENT_W4_POST_INCREMENT);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[4] = 0x1001u;
    dspic33_write_word(cpu, 0x1000u, 0xa5a5u);
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 2u,
           "unimplemented table read and odd destination coalesce in two cycles");
    expect(state,
           cpu->w[2] == 0x5802u && cpu->w[4] == 0x1001u &&
               dspic33_read_word(cpu, 0x1000u) == 0xa5a5u &&
               cpu->last_trap_return == 2u,
           "table read collision completes source and inhibits destination state");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W15_POST_INCREMENT);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 2u,
           "unimplemented table read through stack pointer traps in two cycles");
    expect(state,
           cpu->w[2] == 0x5800u && cpu->w[15] == 0x5006u &&
               dspic33_read_word(cpu, 0x5000u) == 0u &&
               dspic33_read_word(cpu, 0x5002u) == 2u,
           "stack destination update and zero write precede the trap frame");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLWTL_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5a5au;
    cpu->w[3] = 0x5800u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == UINT16_MAX &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) == 0u && cpu->cycles == 2u,
           "table write to unimplemented main program remains valid in two cycles");

    reset_processor_conformance(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 1u,
           "one-word sequential execution enters the program hole");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u &&
               cpu->sequential_program_hole_pc == 0x55802u && cpu->cycles == 2u,
           "sequential program hole executes as one-cycle zero instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55804u &&
               cpu->sequential_program_hole_pc == 0x55804u && cpu->cycles == 3u,
           "sequential program hole provenance advances across zero instructions");

    reset_processor_conformance(cpu, 0x557fcu);
    load_instruction(state, cpu, 0x557fcu, OPCODE_DO_0);
    load_instruction(state, cpu, 0x557feu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 1u,
           "two-word sequential execution uses decoded program length at boundary");

    reset_processor_conformance(cpu, DSPIC33_PROGRAM_LIMIT);
    expect(state,
           dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 0u,
           "direct host entry into program hole remains a bounds stop");

    reset_processor_conformance(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "sequential provenance prepares exact next hole address");
    cpu->pc = 0x55802u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS &&
               cpu->sequential_program_hole_pc == 0u,
           "host PC rewrite cannot reuse stale sequential provenance");

    reset_processor_conformance(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    load_instruction(state, cpu, 0x000014u, 0x000100u);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "sequential program hole prepares interrupt return");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000102u &&
               cpu->interrupt_log_entry[0] == DSPIC33_PROGRAM_LIMIT &&
               cpu->w[15] == 0x5004u && dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 2u,
           "interrupt redirects and stacks program hole PC without provenance leak");

    reset_processor_conformance(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    load_instruction(state, cpu, 0x000014u, DSPIC33_PROGRAM_LIMIT);
    cpu->w[15] = 0x5000u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "same-PC interrupt prepares sequential provenance");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS && cpu->interrupt_count == 1u &&
               cpu->pc == DSPIC33_PROGRAM_LIMIT && cpu->w[15] == 0x5004u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 1u,
           "confirmed same-PC interrupt dispatch clears sequential provenance");

    reset_processor_conformance(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    load_instruction(state, cpu, 0x00000cu, DSPIC33_PROGRAM_LIMIT);
    cpu->stop_on_trap = false;
    cpu->w[15] = 0x5000u;
    cpu->pending_soft_traps[0].trap = 4u;
    cpu->pending_soft_traps[0].vector = 0x00000cu;
    cpu->pending_soft_traps[0].priority = 11u;
    cpu->pending_soft_traps[0].delay = 1u;
    cpu->pending_soft_traps[0].active = true;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 4u &&
               cpu->pc == DSPIC33_PROGRAM_LIMIT && cpu->w[15] == 0x5004u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 1u,
           "delayed same-PC soft trap clears sequential provenance");
    expect(state, dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS,
           "soft-trap target cannot inherit sequential hole authorization");

    reset_processor_conformance(cpu, 0u);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE - 2u;
    cpu->sequential_program_hole_pc = cpu->pc;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 1u,
           "sequential hole provenance ends at auxiliary program boundary");
    expect(state, dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS,
           "auxiliary program boundary retains existing bounds behavior");
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xbeefu;
    expect_address_trap(state, cpu, "odd post-increment word read traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0xbeefu,
           "odd word read preserves pointer and destination");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xbeefu;
    expect_address_trap(state, cpu, "odd pre-increment word read traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0xbeefu,
           "odd pre-increment read inhibits address update");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_DECREMENT);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd post-decrement word write traps");
    expect(state, cpu->w[1] == 0x1001u,
           "odd post-decrement write inhibits address update");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    cpu->w[1] = 0x1003u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd pre-decrement word write traps");
    expect(state, cpu->w[1] == 0x1003u,
           "odd pre-decrement write inhibits address update");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x1001u;
    cpu->w[2] = 0x1122u;
    cpu->w[3] = 0x3344u;
    expect_address_trap(state, cpu, "odd MOV.D source traps");
    expect(state, cpu->w[1] == 0x1001u && cpu->w[2] == 0x1122u && cpu->w[3] == 0x3344u,
           "odd MOV.D preserves destination and pointer");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_BSET_WORD_W4_POST_INCREMENT);
    dspic33_write_word(cpu, 0x1000u, 0x1111u);
    cpu->w[4] = 0x1001u;
    expect_address_trap(state, cpu, "odd indirect word bit operation traps");
    expect(state, cpu->w[4] == 0x1001u && dspic33_read_word(cpu, 0x1000u) == 0x1111u,
           "odd indirect bit operation inhibits data and address update");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BSET_BYTE_W4_POST_DECREMENT);
    dspic33_write_byte(cpu, 0x1001u, 0x11u);
    cpu->w[4] = 0x1001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "odd indirect byte bit operation remains valid");
    expect(state, cpu->w[4] == 0x1000u && dspic33_read_byte(cpu, 0x1001u) == 0x91u,
           "odd indirect byte bit operation updates data and pointer");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_write_byte(cpu, 0x1001u, 0x5au);
    cpu->w[1] = 0x1001u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "odd ordinary byte access remains valid");
    expect(state, cpu->w[1] == 0x1002u && (cpu->w[2] & 0x00ffu) == 0x005au,
           "odd ordinary byte access reads and updates pointer");

    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xdffeu, 0xa5a5u);
    cpu->w[1] = 0xdffeu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "last implemented word read completes");
    expect(state, cpu->w[1] == 0xe000u && cpu->w[2] == 0xa5a5u,
           "last implemented word read updates result and pointer");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    cpu->w[1] = 0xdffeu;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "last implemented word write completes");
    expect(state, cpu->w[1] == 0xe000u && dspic33_read_word(cpu, 0xdffeu) == 0x5a5au,
           "last implemented word write updates memory and pointer");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x0056u;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "unused SFR hole read remains valid");
    expect(state, cpu->w[1] == 0x0058u && cpu->w[2] == 0u,
           "unused SFR hole reads zero and updates pointer");

    reset_processor_conformance(cpu, 0u);
    cpu->instruction_active = true;
    cpu->io.dma_transfer_active = true;
    dspic33_write_word(cpu, 0xe000u, 0x1234u);
    expect(state, dspic33_read_word(cpu, 0xe000u) == 0x1234u && !cpu->address_error,
           "DMA raw access bypasses CPU data map trap");
    cpu->instruction_active = false;
    cpu->io.dma_transfer_active = false;
}

static void page_zero_address_error_cases(ProcessorConformance* state, Dspic33* cpu) {
    reset_processor_conformance(cpu, 0u);
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
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au && cpu->dsrpag == 0u &&
               cpu->dswpag == 1u,
           "page-zero word read completes through DSRPAG alias");

    reset_processor_conformance(cpu, 0u);
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
               dspic33_read_word(cpu, 0x1000u) == 0xa5a5u && cpu->dsrpag == 1u &&
               cpu->dswpag == 0u,
           "page-zero word write completes through DSWPAG alias");

    reset_processor_conformance(cpu, 0u);
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
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au && cpu->dsrpag == 0u &&
               cpu->dswpag == 1u,
           "page-zero byte read completes through DSRPAG alias");

    reset_processor_conformance(cpu, 0u);
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
               dspic33_read_word(cpu, 0x1000u) == 0xa55au && cpu->dsrpag == 1u &&
               cpu->dswpag == 0u,
           "page-zero byte write completes through DSWPAG alias");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W4_LITERAL_2_W2);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dsrpag = 0u;
    cpu->w[2] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "page-zero literal-offset read traps");
    expect(state, cpu->w[2] == 0x5a5au && cpu->w[4] == 0x8ffeu,
           "page-zero literal-offset read completes without pointer update");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W4_LITERAL_2);
    dspic33_write_word(cpu, 0x1000u, 0x5a5au);
    cpu->dswpag = 0u;
    cpu->w[1] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "page-zero literal-offset write traps");
    expect(state, dspic33_read_word(cpu, 0x1000u) == 0xa5a5u && cpu->w[4] == 0x8ffeu,
           "page-zero literal-offset write completes without pointer update");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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

static void unimplemented_data_page_address_error_cases(ProcessorConformance* state,
                                                        Dspic33* cpu) {
    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "implemented EDS page word read completes");
    expect(state, cpu->w[1] == 0x9002u && cpu->w[2] == 0x5a5au,
           "implemented EDS page word read updates result and pointer");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dswpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "implemented EDS page word write completes");
    expect(state, cpu->w[1] == 0x9002u && dspic33_read_word(cpu, 0x9000u) == 0xa5a5u,
           "implemented EDS page word write updates memory and pointer");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X9000_W2);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "direct high-file page-one read completes");
    expect(state, cpu->w[2] == 0x5a5au && !cpu->address_error,
           "direct high-file page-one read uses implemented memory");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_0X9000);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dswpag = 1u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "direct high-file page-one write completes");
    expect(state, dspic33_read_word(cpu, 0x9000u) == 0xa5a5u && !cpu->address_error,
           "direct high-file page-one write uses implemented memory");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X9000_W2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 2u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "direct unimplemented EDS page read traps");
    expect(state, cpu->w[2] == 0u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "direct unimplemented EDS read returns zero");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_0X9000);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dswpag = 2u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "direct unimplemented EDS page write traps");
    expect(state, cpu->w[2] == 0xa5a5u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "direct unimplemented EDS write preserves source and backing");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 2u;
    cpu->dswpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented EDS page word read traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0u && cpu->dsrpag == 2u &&
               cpu->dswpag == 1u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "unimplemented EDS word read returns zero and completes pointer update");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->dswpag = 2u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "unimplemented EDS page word write traps");
    expect(state,
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa5a5u && cpu->dsrpag == 1u &&
               cpu->dswpag == 2u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
           "unimplemented EDS word write preserves source and raw backing");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W4_LITERAL_2_W2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dsrpag = 2u;
    cpu->w[2] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "unimplemented EDS literal-offset read traps");
    expect(state, cpu->w[2] == 0u && cpu->w[4] == 0x8ffeu,
           "unimplemented EDS literal read returns zero without pointer update");

    reset_processor_conformance(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W4_LITERAL_2);
    dspic33_write_word(cpu, 0x11000u, 0x5a5au);
    cpu->dswpag = 2u;
    cpu->w[1] = 0xa5a5u;
    cpu->w[4] = 0x8ffeu;
    expect_address_trap(state, cpu, "unimplemented EDS literal-offset write traps");
    expect(state, dspic33_read_word(cpu, 0x11000u) == 0x5a5au && cpu->w[4] == 0x8ffeu,
           "unimplemented EDS literal write preserves backing and pointer");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u,
           "NOP consumes one cycle");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W14_W2);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u,
           "MOV.D consumes two cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X100);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "direct CALL consumes four cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_W0);
    cpu->w[0] = 0x0100u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "CALL Wn consumes four cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RCALL_NEXT);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "literal RCALL consumes four cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RCALL_W0);
    cpu->w[0] = 0x007fu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "RCALL Wn consumes four cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_LONG_W0);
    cpu->w[0] = 0x0100u;
    cpu->w[1] = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u,
           "CALL.L consumes four cycles");

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETFIE without pending exception consumes six cycles");

    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETURN);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETURN without pending exception consumes six cycles");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_RETLW_0X123_W2);
    load_instruction(state, cpu, 0x000100u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x5000u, 0x0100u);
    dspic33_write_word(cpu, 0x5002u, 0u);
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 6u,
           "RETLW without pending exception consumes six cycles");
    expect(state, cpu->w[2] == 0x0123u, "RETLW writes return literal");

    reset_processor_conformance(cpu, 0u);
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

    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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
    reset_processor_conformance(cpu, 0u);
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

static void illegal_condition_reset_cases(ProcessorConformance* state, Dspic33* cpu) {
    static const uint16_t preserved_addresses[] = {
        0x0620u, 0x0622u, 0x0624u, 0x0626u, 0x0742u, 0x0744u,
        0x0746u, 0x0748u, 0x074eu, 0x0758u, 0x075au,
    };
    uint16_t
        preserved_values[sizeof(preserved_addresses) / sizeof(preserved_addresses[0])];
    Dspic33 copy;
    size_t index;

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ILLEGAL);
    dspic33_set_working_register(cpu, 0u, 0x1234u);
    dspic33_set_working_register(cpu, 1u, 0x5000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    cpu->splim = 0x6000u;
    cpu->splim_enabled = true;
    cpu->rcount = 3u;
    cpu->dcount = 4u;
    cpu->dostart = 0x100u;
    cpu->doend = 0x200u;
    cpu->tblpag = 0xa5u;
    cpu->dsrpag = 0x123u;
    cpu->dswpag = 0x123u;
    cpu->repeat_active = 1u;
    cpu->do_depth = 1u;
    cpu->data[0x0740u] = 0x83u;
    cpu->data[0x0741u] = 0u;
    for (index = 0u;
         index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        uint16_t address = preserved_addresses[index];
        preserved_values[index] = (uint16_t)(0xa100u + index);
        cpu->data[address] = (uint8_t)preserved_values[index];
        cpu->data[address + 1u] = (uint8_t)(preserved_values[index] >> 8u);
    }
    cpu->io.adc[3] = 0x0456u;
    cpu->io.gpio[2] = 0x789au;
    cpu->io.uart_cts = 0x05u;
    cpu->io.spi_selected = 0x09u;
    cpu->io.timer_gate = 0x0105u;
    cpu->io.pwm_dead_time_inputs = 0x25u;
    cpu->io.pwm_sync_inputs = 0x02u;
    cpu->io.pwm_fault_inputs = 0x81234567u;
    cpu->io.pwm_current_limit_inputs = 0x89abcdefu;
    cpu->io.usb_host_attached = true;
    cpu->io.timer_enabled = 0xffffu;
    cpu->io.uart_rx_fifo[0].count = 1u;
    cpu->io.usb_host_pending = true;
    cpu->io.cpu_write_valid = true;
    expect(state, dspic33_uart_set_cts(cpu, 0u, true, 20u),
           "schedule peripheral state before illegal reset");
    dspic33_write_word(cpu, 0x0100u, 0xffffu);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    dspic33_write_word(cpu, 0x5002u, 0x5555u);
    expect_illegal_reset(state, cpu, "known illegal opcode resets processor");
    expect(state,
           dspic33_read_word(cpu, 0x0740u) == 0x4083u && cpu->sr == 0u &&
               cpu->corcon == 0x0020u && cpu->splim == 0u && !cpu->splim_enabled,
           "illegal reset preserves RCON history and resets core state");
    expect(state,
           cpu->rcount == 0u && cpu->dcount == 0u && cpu->dostart == 0u &&
               cpu->doend == 0u && cpu->tblpag == 0u && cpu->dsrpag == 1u &&
               cpu->dswpag == 1u && cpu->repeat_active == 0u && cpu->do_depth == 0u,
           "illegal reset restores loop and page state");
    expect(state,
           dspic33_read_word(cpu, 0x5000u) == 0xaaaau &&
               dspic33_read_word(cpu, 0x5002u) == 0x5555u &&
               dspic33_read_word(cpu, 0x0100u) == 0u,
           "warm reset retains RAM without writing an exception frame");
    for (index = 0u;
         index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        expect(state,
               dspic33_read_word(cpu, preserved_addresses[index]) ==
                   preserved_values[index],
               "warm reset retains oscillator and RTCC register");
    }
    expect(state,
           cpu->io.adc[3] == 0x0456u && cpu->io.gpio[2] == 0x789au &&
               cpu->io.uart_cts == 0x05u && cpu->io.spi_selected == 0x09u &&
               cpu->io.timer_gate == 0x0105u && cpu->io.pwm_dead_time_inputs == 0x25u &&
               cpu->io.pwm_sync_inputs == 0x02u &&
               cpu->io.pwm_fault_inputs == 0x81234567u &&
               cpu->io.pwm_current_limit_inputs == 0x89abcdefu &&
               cpu->io.usb_host_attached,
           "warm reset retains external input state");
    expect(state,
           cpu->io.timer_enabled == 0u && cpu->io.uart_rx_fifo[0].count == 0u &&
               !cpu->io.usb_host_pending && !cpu->io.cpu_write_valid &&
               cpu->events.count == 0u,
           "warm reset clears peripheral execution state");
    expect(state, dspic33_initialize(&copy), "initialize illegal reset copy");
    expect(state, dspic33_copy(&copy, cpu), "copy illegal reset state");
    expect(state,
           copy.illegal_reset && copy.illegal_reset_count == 1u &&
               copy.initialized_working_registers == 0x8000u,
           "copy retains illegal reset lifecycle state");
    dspic33_destroy(&copy);
    load_instruction(state, cpu, 0u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && !cpu->illegal_reset,
           "next instruction clears transient illegal reset state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_LITERAL);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u,
           "MOVPAG literal PP2 writes TBLPAG");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_INVALID_LITERAL);
    expect_illegal_reset(state, cpu, "MOVPAG literal PP3 resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_W1);
    dspic33_set_working_register(cpu, 1u, 0x00a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u,
           "MOVPAG register PP2 writes TBLPAG");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_W1);
    cpu->w[1] = 0x00a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u,
           "MOVPAG data source ignores initialization tag");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_INVALID_W1);
    dspic33_set_working_register(cpu, 1u, 0x00a5u);
    expect_illegal_reset(state, cpu, "MOVPAG register PP3 resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    cpu->w[1] = 0x1000u;
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    expect_illegal_reset(state, cpu, "raw host W pointer does not initialize register");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_LITERAL_W1);
    load_instruction(state, cpu, 2u, OPCODE_MOV_W1_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (cpu->initialized_working_registers & 0x0002u) == 0u,
           "byte instruction destination leaves W pointer uninitialized");
    expect_illegal_reset(state, cpu, "byte instruction result remains invalid pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W15_W2);
    dspic33_write_word(cpu, 0x1000u, 0xa5a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0xa5a5u &&
               cpu->illegal_reset_count == 0u,
           "W15 indirect access is initialized after reset");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "host word setter initializes same-value pointer");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_byte(cpu, 2u, 0u);
    expect_illegal_reset(state, cpu,
                         "single byte W alias leaves pointer uninitialized");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_byte(cpu, 2u, 0u);
    dspic33_write_byte(cpu, 3u, 0x10u);
    expect_illegal_reset(state, cpu, "two byte W aliases leave pointer uninitialized");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_write_word(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1234u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1234u &&
               (cpu->initialized_working_registers & 0x0006u) == 0x0006u,
           "word W alias initializes pointer and destination");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0x1000u);
    dspic33_write_byte(cpu, 2u, 0u);
    dspic33_write_word(cpu, 0x1000u, 0x5678u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5678u,
           "byte W alias preserves prior initialized state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1);
    cpu->w[1] = 0x5000u;
    dspic33_set_working_register(cpu, 2u, 0xbeefu);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    expect_illegal_reset(state, cpu, "uninitialized store pointer resets processor");
    expect(state, dspic33_read_word(cpu, 0x5000u) == 0xaaaau,
           "uninitialized store does not modify RAM");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5_POST_DECREMENT);
    dspic33_set_working_register(cpu, 2u, 1u);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    cpu->w[5] = 0x5000u;
    dspic33_write_word(cpu, 0x1000u, 2u);
    dspic33_write_word(cpu, 0x5000u, 0xaaaau);
    expect_illegal_reset(state, cpu,
                         "uninitialized two-operand destination resets processor");
    expect(state, !cpu->address_error, "illegal reset clears address error flag");
    expect(state, !cpu->address_error_access_allowed,
           "illegal reset clears address error access state");
    expect(state, !cpu->address_error_working_state_completed,
           "illegal reset clears address error working state");
    expect(state, !cpu->address_error_control_state_completed,
           "illegal reset clears address error control state");
    expect(state, cpu->address_error_return == 0u,
           "illegal reset clears address error return");
    expect(state, cpu->sr == 0u && cpu->corcon == 0x0020u,
           "illegal reset discards post-validation flags");
    expect(state,
           dspic33_read_word(cpu, 0x1000u) == 2u &&
               dspic33_read_word(cpu, 0x5000u) == 0xaaaau,
           "uninitialized destination preserves source and destination data");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5);
    cpu->w[2] = 7u;
    cpu->w[4] = 0x1000u;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    dspic33_write_word(cpu, 0x1000u, 0xabcdu);
    expect_illegal_reset(state, cpu,
                         "uninitialized binary source resets before direct result");
    expect(state,
           cpu->w[5] == 0u && cpu->sr == 0u && cpu->corcon == 0x0020u &&
               dspic33_read_word(cpu, 0x1000u) == 0xabcdu,
           "binary source reset prevents data, result and flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_COMPARE_ZERO_W4_POST_INCREMENT);
    cpu->w[4] = 0x1000u;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    expect_illegal_reset(state, cpu,
                         "uninitialized compare source resets before flags");
    expect(state, cpu->sr == 0u && cpu->corcon == 0x0020u,
           "compare source reset prevents flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ACCUMULATOR_ADD_W4_POST_INCREMENT);
    cpu->w[4] = 0x1000u;
    cpu->accumulator[0] = 0x12345678;
    cpu->sr = 0xffffu;
    cpu->corcon = 0xffffu;
    expect_illegal_reset(state, cpu,
                         "uninitialized accumulator source resets before result");
    expect(state, cpu->accumulator[0] == 0 && cpu->sr == 0u && cpu->corcon == 0x0020u,
           "accumulator source reset prevents result and flag mutation");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    cpu->w[2] = 0x1000u;
    expect_illegal_reset(state, cpu, "uninitialized table pointer resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W3);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "initialized table pointer completes access");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_W2_W4_POST_INCREMENT);
    cpu->tblpag = 5u;
    dspic33_set_working_register(cpu, 2u, 0x5800u);
    cpu->w[4] = 0x1000u;
    expect_illegal_reset(
        state, cpu,
        "unimplemented table read with uninitialized destination resets processor");
    expect(state,
           !cpu->address_error && !cpu->address_error_access_allowed &&
               !cpu->address_error_working_state_completed &&
               !cpu->address_error_control_state_completed &&
               cpu->address_error_return == 0u,
           "table destination reset clears address error lifecycle state");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    cpu->w[9] = 0x1000u;
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    expect_illegal_reset(state, cpu, "uninitialized DSP base resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_INDEXED);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "initialized DSP bases complete access");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_WRITE_BACK);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    cpu->w[13] = 0x5000u;
    expect_illegal_reset(state, cpu,
                         "uninitialized DSP write-back pointer resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_WRITE_BACK);
    dspic33_set_working_register(cpu, 9u, 0x1000u);
    dspic33_set_working_register(cpu, 11u, 0x9000u);
    dspic33_set_working_register(cpu, 12u, 0u);
    dspic33_set_working_register(cpu, 13u, 0x5000u);
    cpu->w[4] = 1u;
    cpu->w[5] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[13] == 0x5002u &&
               cpu->illegal_reset_count == 0u,
           "initialized DSP write-back ignores multiplicand tags");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_DIRECT_W13);
    cpu->w[4] = 1u;
    cpu->w[5] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (cpu->initialized_working_registers & 0x2000u) != 0u &&
               cpu->illegal_reset_count == 0u,
           "direct DSP W13 result initializes destination");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BRA_W0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "branch register is not an address-pointer tag use");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_REPEAT_W0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "REPEAT count register is not an address-pointer tag use");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DO_W0);
    load_instruction(state, cpu, 2u, 0x000002u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "DO count register is not an address-pointer tag use");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_PUSH_SHADOW);
    load_instruction(state, cpu, 2u, OPCODE_MOV_W0_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "PUSH.S values are not address-pointer tag uses");
    expect_illegal_reset(state, cpu, "PUSH.S does not initialize W0 pointer");
}

int main(void) {
    ProcessorConformance state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize processor conformance");
    if (initialized) {
        program_target_address_error_cases(&state, &cpu);
        program_read_address_error_cases(&state, &cpu);
        address_error_cases(&state, &cpu);
        data_map_address_error_cases(&state, &cpu);
        page_zero_address_error_cases(&state, &cpu);
        unimplemented_data_page_address_error_cases(&state, &cpu);
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
        illegal_condition_reset_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[processor-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32
           "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
