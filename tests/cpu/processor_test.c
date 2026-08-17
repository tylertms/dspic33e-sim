#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device.h"
#include "dspic33.h"
#include "dspic33ep512mu810_data.h"
#include "test.h"

enum {
    PSV_TEST_ADDRESS = 0x01004000u,
    OPCODE_NOP = 0x000000u,
    OPCODE_POWER_SAVE_SLEEP = 0xfe4000u,
    OPCODE_RETURN = 0x060000u,
    OPCODE_RETFIE = 0x064000u,
    OPCODE_RETLW_0X123_W2 = 0x051232u,
    OPCODE_RETLW_0X122_W15 = 0x05122fu,
    OPCODE_CALL_0X300 = 0x020300u,
    OPCODE_CALL_0X55800 = 0x025800u,
    OPCODE_GOTO_0X300 = 0x040300u,
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
    OPCODE_BRA_OA_0X55800 = 0x0c0012u,
    OPCODE_BTSS_W2_BIT_0 = 0xa60002u,
    OPCODE_BTSS_W4_POST_INCREMENT_BIT_0 = 0xa60034u,
    OPCODE_BTSC_W2_BIT_0 = 0xa70002u,
    OPCODE_BTSC_W4_POST_INCREMENT_BIT_0 = 0xa70034u,
    OPCODE_CPSEQ = 0xe78010u,
    OPCODE_CPSNE = 0xe70010u,
    OPCODE_CPSGT = 0xe60010u,
    OPCODE_CPSLT = 0xe68010u,
    OPCODE_COMPARE_SKIP_BYTE = 0x000400u,
    OPCODE_SFTAC_A_W5 = 0xc80005u,
    OPCODE_DIV_SW_W2_W3 = 0xd80103u,
    OPCODE_DIV_SD_W4_W3 = 0xd82a43u,
    OPCODE_DIV_UW_W2_W3 = 0xd88103u,
    OPCODE_DIV_UD_W4_W3 = 0xd8aa43u,
    OPCODE_DIVF_W2_W3 = 0xd91003u,
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
    OPCODE_MOV_DOUBLE_W1_W2 = 0xbe0111u,
    OPCODE_MOV_DOUBLE_W2_W1 = 0xbe8882u,
    OPCODE_MOV_W2_W1_POST_INCREMENT = 0x781882u,
    OPCODE_MOV_W1_POST_INCREMENT_W2 = 0x780131u,
    OPCODE_MOV_W1_POST_DECREMENT_W2 = 0x780121u,
    OPCODE_MOV_W1_PRE_DECREMENT_W2 = 0x780141u,
    OPCODE_MOV_W1_PRE_INCREMENT_W2 = 0x780151u,
    OPCODE_MOV_W1_W0_OFFSET_W2 = 0x780161u,
    OPCODE_MOV_W15_W0_OFFSET_W2 = 0x78016fu,
    OPCODE_MOV_W2_W1_POST_DECREMENT = 0x781082u,
    OPCODE_MOV_W2_W1_PRE_INCREMENT = 0x782882u,
    OPCODE_MOV_W2_W1_PRE_DECREMENT = 0x782082u,
    OPCODE_MOV_W1_MEMORY_W2_MEMORY = 0x780911u,
    OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2 = 0x784131u,
    OPCODE_MOV_BYTE_W1_POST_DECREMENT_W2 = 0x784121u,
    OPCODE_MOV_BYTE_W1_PRE_DECREMENT_W2 = 0x784141u,
    OPCODE_MOV_BYTE_W1_PRE_INCREMENT_W2 = 0x784151u,
    OPCODE_MOV_BYTE_W2_W1_POST_INCREMENT = 0x785882u,
    OPCODE_MOV_W1_W4_LITERAL_2 = 0x980211u,
    OPCODE_MOV_W4_LITERAL_2_W2 = 0x900114u,
    OPCODE_MOV_0X9000_W2 = 0x848002u,
    OPCODE_MOV_W2_0X9000 = 0x8c8002u,
    OPCODE_MOV_DOUBLE_W2_W1_POST_INCREMENT = 0xbe9882u,
    OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2 = 0xbe0131u,
    OPCODE_MOV_DOUBLE_W1_PRE_INCREMENT_W2 = 0xbe0151u,
    OPCODE_MOV_DOUBLE_W4_POST_DECREMENT_W2 = 0xbe0124u,
    OPCODE_MOV_DOUBLE_W4_POST_INCREMENT_W2 = 0xbe0134u,
    OPCODE_MOV_DOUBLE_W4_PRE_DECREMENT_W2 = 0xbe0144u,
    OPCODE_MOV_DOUBLE_W4_PRE_INCREMENT_W2 = 0xbe0154u,
    OPCODE_MOV_DOUBLE_W2_W4_POST_DECREMENT = 0xbe9202u,
    OPCODE_MOV_DOUBLE_W2_W4_POST_INCREMENT = 0xbe9a02u,
    OPCODE_MOV_DOUBLE_W2_W4_PRE_DECREMENT = 0xbea202u,
    OPCODE_MOV_DOUBLE_W2_W4_PRE_INCREMENT = 0xbeaa02u,
    OPCODE_MOV_DOUBLE_W2_POST_INCREMENT_W2 = 0xbe0132u,
    OPCODE_MOV_DOUBLE_W2_W2_POST_INCREMENT = 0xbe9902u,
    OPCODE_MOV_DOUBLE_W4_W6 = 0xbe0314u,
    OPCODE_MOV_DOUBLE_INVALID_SOURCE_MODE_6 = 0xbe0161u,
    OPCODE_MOV_DOUBLE_INVALID_SOURCE_MODE_7 = 0xbe0171u,
    OPCODE_MOV_DOUBLE_INVALID_DESTINATION_MODE_6 = 0xbeb082u,
    OPCODE_MOV_DOUBLE_INVALID_DESTINATION_MODE_7 = 0xbeb882u,
    OPCODE_MOV_DOUBLE_INVALID_MEMORY_PAIR = 0xbe8891u,
    OPCODE_MOV_DOUBLE_INVALID_ODD_SOURCE_PAIR = 0xbe0101u,
    OPCODE_MOV_DOUBLE_INVALID_ODD_DESTINATION_PAIR = 0xbe0082u,
    OPCODE_MOV_DOUBLE_INVALID_REVERSE_ODD_SOURCE_PAIR = 0xbe8881u,
    OPCODE_MOV_DOUBLE_INVALID_REVERSE_DIRECT = 0xbe8102u,
    OPCODE_MOV_DOUBLE_INVALID_DIRECTION_BIT = 0xbec902u,
    OPCODE_MOV_W0_IFS0 = 0x884000u,
    OPCODE_MOV_IFS0_W2 = 0x804002u,
    OPCODE_MOV_FILE_WORD_0X1000 = 0xbfb000u,
    OPCODE_MOV_FILE_BYTE_0X1001 = 0xbff001u,
    OPCODE_MOV_FILE_WORD_W0 = 0xbfa000u,
    OPCODE_MOV_FILE_BYTE_W0 = 0xbfe000u,
    OPCODE_MOV_FILE_WORD_CORCON = 0xbfa044u,
    OPCODE_MOV_FILE_WORD_PORTB = 0xbfae12u,
    OPCODE_MOV_FILE_BYTE_PORTB = 0xbfee12u,
    OPCODE_MOV_0X1000_WREG = 0xbf9000u,
    OPCODE_MOV_BYTE_0X1001_WREG = 0xbfd001u,
    OPCODE_BSET_BYTE_IFS0_BIT_0 = 0xa80800u,
    OPCODE_BSET_BYTE_CORCON_BIT_1 = 0xa82044u,
    OPCODE_BTSS_IFS0_BIT_0 = 0xae0800u,
    OPCODE_PUSH_IFS0 = 0xf80800u,
    OPCODE_CLEAR_TMR2 = 0xef2106u,
    OPCODE_SET_TMR2 = 0xefa106u,
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
    OPCODE_MOV_LITERAL_0X1000_W2 = 0x210002u,
    OPCODE_MOV_LITERAL_0X1000_W8 = 0x210008u,
    OPCODE_MOV_LITERAL_0X8100_W2 = 0x281002u,
    OPCODE_MOV_LITERAL_2_W4 = 0x200024u,
    OPCODE_MOV_W2_W3 = 0x780182u,
    OPCODE_MOV_W2_W2 = 0x780102u,
    OPCODE_MOV_W2_INDIRECT_W3 = 0x780192u,
    OPCODE_MOV_W2_INDIRECT_W4 = 0x780212u,
    OPCODE_MOV_W2_POST_INCREMENT_W3 = 0x7801b2u,
    OPCODE_MOV_W3_W2_INDIRECT = 0x780903u,
    OPCODE_MOV_W3_W2_POST_INCREMENT = 0x781903u,
    OPCODE_MOV_W2_W4_OFFSET_W3 = 0x7a01e2u,
    OPCODE_RETLW_0X10_W2 = 0x050102u,
    OPCODE_MOV_BYTE_LITERAL_W1 = 0xb3c001u,
    OPCODE_MOVPAG_TBL_LITERAL = 0xfec8a5u,
    OPCODE_MOVPAG_INVALID_LITERAL = 0xfecc00u,
    OPCODE_MOVPAG_TBL_W1 = 0xfed801u,
    OPCODE_MOVPAG_INVALID_W1 = 0xfedc01u,
    OPCODE_SWAP_W1 = 0xfd8001u,
    OPCODE_SWAP_BYTE_W1 = 0xfdc001u,
    OPCODE_SWAP_W15 = 0xfd800fu,
    OPCODE_SWAP_BYTE_W15 = 0xfdc00fu,
    OPCODE_EXCH_W1_W2 = 0xfd0101u,
    OPCODE_EXCH_W1_W1 = 0xfd0081u,
    OPCODE_ILLEGAL = 0x3f0000u,
    OPCODE_REPEAT_2 = 0x090002u,
    OPCODE_REPEAT_W0 = 0x098000u,
    OPCODE_DISI_2 = 0xfc0002u,
    OPCODE_MOV_W1_IFS1 = 0x884011u,
    OPCODE_CLEAR_RCOUNT = 0xef2036u,
    OPCODE_INCREMENT_W2 = 0xe80102u,
    OPCODE_RESET = 0xfe0000u,
    OPCODE_DO_0 = 0x080000u,
    OPCODE_DO_1 = 0x080001u,
    OPCODE_DO_W0 = 0x088000u,
    OPCODE_PUSH_SHADOW = 0xfea000u,
    OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT = 0xc0045fu,
    OPCODE_DSP_X_W8_INCREMENT_4_Y_W10_DECREMENT = 0xc0049fu,
    OPCODE_DSP_X_W8_INCREMENT_6_Y_W10_DECREMENT = 0xc004dfu,
    OPCODE_DSP_X_W8_DECREMENT_6_Y_W10_DECREMENT = 0xc0055fu,
    OPCODE_DSP_X_W8_DECREMENT_4_Y_W10_DECREMENT = 0xc0059fu,
    OPCODE_DSP_X_W8_DECREMENT_Y_W10_DECREMENT = 0xc005dfu,
    OPCODE_DSP_X_NO_UPDATE_Y_W10_DECREMENT = 0xc0041fu,
    OPCODE_DSP_Y_W10_DECREMENT = 0xc0051fu,
    OPCODE_DSP_INDEXED = 0xc00732u,
    OPCODE_DSP_DIRECT_W13 = 0xc00110u,
    OPCODE_DSP_WRITE_BACK = 0xc393b1u,
    OPCODE_DSP_CLEAR_DIRECT = 0xc300d0u,
    OPCODE_DSP_MOVSAC_WRITE_BACK = 0xc707f1u,
    OPCODE_DSP_ED = 0xf0405fu,
    OPCODE_DSP_PREFETCH_W4_COLLISION = 0xc00047u,
    OPCODE_DSP_PREFETCH_W5_COLLISION = 0xc01447u,
    OPCODE_DSP_PREFETCH_W6_COLLISION = 0xc02847u,
    OPCODE_DSP_PREFETCH_W7_COLLISION = 0xc03c47u,
    OPCODE_DSP_MOVSAC_W4_COLLISION = 0xc70046u
};

static void load_instruction(TestState* state, Dspic33* cpu, uint32_t address,
                             uint32_t opcode) {
    expect(state, dspic33_load_program_word(cpu, address, opcode),
           "load processor instruction");
}

static void expect_step_cycles(TestState* state, Dspic33* cpu, uint64_t expected_cycles,
                               const char* name) {
    uint64_t before = cpu->cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->cycles - before == expected_cycles,
           name);
}

static void reset_processor_test(Dspic33* cpu, uint32_t entry) {
    uint8_t reg;
    dspic33_reset(cpu, entry);
    for (reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(cpu, reg, cpu->w[reg]);
    }
}

static void expect_illegal_reset(TestState* state, Dspic33* cpu,
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

static void prepare_trap_vectors(TestState* state, Dspic33* cpu) {
    load_instruction(state, cpu, 0x00000au, 0x000300u);
    load_instruction(state, cpu, 0x00000cu, 0x000320u);
    load_instruction(state, cpu, 0x000300u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000320u, OPCODE_NOP);
}

static void prepare_address_trap(TestState* state, Dspic33* cpu) {
    load_instruction(state, cpu, 0x000006u, 0x000340u);
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
}

static void expect_address_trap(TestState* state, Dspic33* cpu, const char* execution) {
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED, execution);
    expect(state,
           cpu->last_trap == 1u && cpu->last_trap_return == 2u && cpu->pc == 0x000340u,
           "address error enters hard trap");
    expect(state,
           (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u,
           "address error records status and priority");
}

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
               dspic33_read_word(cpu, 0x5000u) == 5u &&
               dspic33_read_word(cpu, 0x5004u) == 2u && (cpu->corcon & 0x0004u) == 0u,
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
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u &&
               (cpu->corcon & 0x0004u) != 0u,
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
           cpu->last_trap_return == 2u && cpu->w[15] == 0x5004u &&
               cpu->call_depth == 0u && (cpu->corcon & 0x0004u) == 0u,
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
           cpu->last_trap_return == 2u && cpu->w[2] == 0x0123u &&
               cpu->w[15] == 0x5004u && cpu->call_depth == 0u,
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
               dspic33_read_word(cpu, 0x5002u) == 0x0005u &&
               (cpu->corcon & 0x0004u) == 0u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557dcu &&
               cpu->cycles == 1u && cpu->last_trap == UINT16_MAX,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557dcu &&
               cpu->cycles == 1u && cpu->last_trap == UINT16_MAX,
           "untaken accumulator BRA ignores combined overflow status");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_0X300);
    load_instruction(state, cpu, 2u, 0u);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u &&
               cpu->cycles == 4u,
           "implemented literal GOTO target remains valid");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_GOTO_0X300);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u &&
               cpu->cycles == 4u && cpu->w[15] == 0x5000u &&
               dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
               dspic33_read_word(cpu, 0x5002u) == 0x5a5au &&
               (cpu->corcon & 0x0004u) != 0u && cpu->sequential_program_hole_pc == 0u,
           "boundary GOTO reads zero extension without sequential provenance");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 2u, 0u);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u &&
               cpu->cycles == 4u && dspic33_read_word(cpu, 0x5000u) == 4u,
           "implemented literal CALL target remains valid");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[15] = 0x5000u;
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u &&
               cpu->cycles == 4u && cpu->w[15] == 0x5004u && cpu->call_depth == 1u &&
               dspic33_read_word(cpu, 0x5000u) == 0x5803u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0005u &&
               (cpu->corcon & 0x0004u) == 0u && cpu->sequential_program_hole_pc == 0u,
           "boundary CALL stacks hole return and clears sequential provenance");

    reset_processor_test(cpu, 0x557feu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557feu;
    load_instruction(state, cpu, 0x557feu, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 0x300u, OPCODE_RETURN);
    cpu->corcon |= 0x0004u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u &&
               cpu->cycles == 4u && cpu->sequential_program_hole_pc == 0u,
           "boundary CALL prepares explicit return to program hole");
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles == 10u &&
               cpu->last_trap == 1u && cpu->last_trap_return == 0x302u &&
               cpu->call_depth == 0u && cpu->w[15] == 0x5004u &&
               cpu->sequential_program_hole_pc == 0u,
           "boundary CALL return validates hole target without provenance reuse");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_W0);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[0] = 0x300u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u &&
               cpu->cycles == 4u,
           "implemented GOTO Wn target remains valid");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_GOTO_LONG_W0);
    load_instruction(state, cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_NOP);
    cpu->w[0] = 0xc000u;
    cpu->w[1] = 0x007fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x7fc000u &&
               cpu->cycles == 4u && cpu->last_trap == UINT16_MAX,
           "auxiliary GOTO.L target is accepted");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u,
           "auxiliary target executes through mapped Flash");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_CALL_W0);
    load_instruction(state, cpu, 0x300u, OPCODE_NOP);
    cpu->w[0] = 0x300u;
    cpu->w[15] = 0x5000u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x300u &&
               cpu->cycles == 4u && dspic33_read_word(cpu, 0x5000u) == 2u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[3] == 0xcdefu &&
               cpu->cycles == 5u && cpu->disicnt == 1u,
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
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5800u &&
               cpu->w[3] == 0u && cpu->cycles == 5u,
           "unimplemented high table word read returns zero in five cycles");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDH_BYTE_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5800u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5800u &&
               cpu->w[3] == 0xa500u && cpu->cycles == 5u,
           "unimplemented high table byte read clears the low byte in five cycles");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_TBLRDL_BYTE_W2_W3);
    cpu->tblpag = 5u;
    cpu->w[2] = 0x5801u;
    cpu->w[3] = 0xa5a5u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[2] == 0x5801u &&
               cpu->w[3] == 0xa500u && cpu->cycles == 5u,
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
               dspic33_read_word(cpu, 0x1000u) == 0xa5a5u &&
               cpu->last_trap_return == 2u,
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
           cpu->w[2] == 0x5800u && cpu->w[15] == 0x5006u &&
               dspic33_read_word(cpu, 0x5000u) == 0u &&
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

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DO_0);
    load_instruction(state, cpu, 2u, 2u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->cycles == 2u &&
               cpu->do_depth == 1u && cpu->do_count[0] == 0u &&
               cpu->do_start[0] == 4u && cpu->do_end[0] == 8u,
           "literal DO initializes loop state in two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DO_W0);
    load_instruction(state, cpu, 2u, 2u);
    cpu->w[0] = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u && cpu->cycles == 2u &&
               cpu->do_depth == 1u && cpu->do_count[0] == 1u &&
               cpu->do_start[0] == 4u && cpu->do_end[0] == 8u,
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
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x000340u &&
               cpu->cycles == 2u && cpu->last_trap == 1u &&
               cpu->last_trap_return == 0x557feu && cpu->do_depth == 1u &&
               cpu->do_count[0] == 1u && cpu->do_start[0] == DSPIC33_PROGRAM_LIMIT &&
               cpu->do_end[0] == 0x55804u && cpu->dcount == 1u &&
               cpu->dostart == DSPIC33_PROGRAM_LIMIT && cpu->doend == 0x55804u,
           "DO start in program hole traps after completing loop state");
    expect(state,
           cpu->w[15] == 0x5004u && dspic33_read_word(cpu, 0x5000u) == 0x57feu &&
               dspic33_read_word(cpu, 0x5002u) == 0x0f05u &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u &&
               cpu->sequential_program_hole_pc == 0u,
           "DO program-hole trap stacks extension PC and clears provenance");

    reset_processor_test(cpu, 0x557f8u);
    load_instruction(state, cpu, 0x557f8u, OPCODE_DO_1);
    load_instruction(state, cpu, 0x557fau, 2u);
    load_instruction(state, cpu, 0x557fcu, OPCODE_NOP);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557fcu &&
               cpu->cycles == 2u,
           "DO with program-hole end initializes normally");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557fcu &&
               cpu->dcount == 0u && cpu->last_trap == UINT16_MAX && cpu->cycles == 5u,
           "DO program-hole end executes zero and starts final iteration");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u &&
               cpu->do_depth == 0u && (cpu->sr & 0x0200u) == 0u &&
               (cpu->corcon & 0x0700u) == 0u && cpu->cycles == 8u,
           "DO program-hole final iteration exits normally");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_REPEAT_2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->rcount == 2u && cpu->repeat_active != 0u &&
               (cpu->sr & 0x0010u) != 0u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 1u,
           "REPEAT initializes a program-hole target");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->rcount == 1u && cpu->repeat_active != 0u &&
               (cpu->sr & 0x0010u) != 0u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 2u,
           "REPEAT retains provenance for a program-hole rewind");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->rcount == 0u && cpu->repeat_active != 0u &&
               (cpu->sr & 0x0010u) == 0u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 3u,
           "REPEAT clears RA when the counter reaches zero");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x55802u &&
               cpu->rcount == 0u && cpu->repeat_active == 0u &&
               (cpu->sr & 0x0010u) == 0u &&
               cpu->sequential_program_hole_pc == 0x55802u && cpu->cycles == 4u,
           "REPEAT executes the final program-hole iteration once");

    reset_processor_test(cpu, DSPIC33_PROGRAM_LIMIT);
    expect(state,
           dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 0u,
           "direct host entry into program hole remains a bounds stop");

    reset_processor_test(cpu, 0x557feu);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "sequential provenance prepares exact next hole address");
    cpu->pc = 0x55802u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_PROGRAM_BOUNDS &&
               cpu->sequential_program_hole_pc == 0u,
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
               cpu->interrupt_log_entry[0] == DSPIC33_PROGRAM_LIMIT &&
               cpu->w[15] == 0x5004u && dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->last_trap == 1u &&
               cpu->pc == 0x000340u && cpu->w[15] == 0x5004u &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 1u,
           "unimplemented soft-trap vector dispatches Address Error");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000342u &&
               cpu->cycles == 2u,
           "Address Error handler continues after invalid soft-trap vector");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, DSPIC33_AUXILIARY_PROGRAM_BASE, OPCODE_NOP);
    cpu->pc = DSPIC33_AUXILIARY_PROGRAM_BASE - 2u;
    cpu->sequential_program_hole_pc = cpu->pc;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE &&
               cpu->sequential_program_hole_pc == 0u && cpu->cycles == 1u,
           "sequential hole provenance ends at auxiliary program boundary");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->pc == DSPIC33_AUXILIARY_PROGRAM_BASE + 2u,
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
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x340u &&
               cpu->cycles == 2u && cpu->last_trap == 1u &&
               cpu->last_trap_return == DSPIC33_PROGRAM_LIMIT && cpu->w[15] == 0x5004u,
           "one-word boundary skip raises Address Error in two cycles");
    expect(state,
           dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
               dspic33_read_word(cpu, 0x5002u) == 0x0305u &&
               (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u &&
               cpu->sequential_program_hole_pc == 0u,
           "boundary skip stacks exact hole PC and hard-trap state");

    reset_processor_test(cpu, 0x557fcu);
    prepare_address_trap(state, cpu);
    cpu->pc = 0x557fcu;
    load_instruction(state, cpu, 0x557fcu, OPCODE_BTSC_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 0x557feu, OPCODE_NOP);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->w[4] == 0x1002u &&
               cpu->cycles == 2u,
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
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
               cpu->cycles == 3u && cpu->w[15] == 0x5000u && cpu->call_depth == 0u &&
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
               cpu->interrupt_log_entry[0] == DSPIC33_PROGRAM_LIMIT &&
               cpu->w[15] == 0x5004u && dspic33_read_word(cpu, 0x5000u) == 0x5800u &&
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

static void compare_skip_truth_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                    uint16_t left, uint16_t right, bool taken,
                                    const char* name) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, opcode | 1u);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    cpu->w[0] = left;
    cpu->w[1] = right;
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == (taken ? 4u : 2u) &&
               cpu->cycles == (taken ? 2u : 1u) && cpu->w[0] == left &&
               cpu->w[1] == right && cpu->sr == 0x010fu,
           name);
}

static void compare_skip_cases(TestState* state, Dspic33* cpu) {
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ, 0x1234u, 0x1234u, true,
                            "CPSEQ word takes equal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ, 0x1234u, 0x4321u, false,
                            "CPSEQ word rejects unequal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ | OPCODE_COMPARE_SKIP_BYTE,
                            0x80ffu, 0x7fffu, true,
                            "CPSEQ byte ignores high-byte difference");
    compare_skip_truth_case(state, cpu, OPCODE_CPSEQ | OPCODE_COMPARE_SKIP_BYTE,
                            0x8000u, 0x0001u, false,
                            "CPSEQ byte rejects unequal low bytes");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE, 0x1234u, 0x4321u, true,
                            "CPSNE word takes unequal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE, 0x1234u, 0x1234u, false,
                            "CPSNE word rejects equal comparison");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE | OPCODE_COMPARE_SKIP_BYTE,
                            0x80ffu, 0x7fffu, false,
                            "CPSNE byte ignores high-byte difference");
    compare_skip_truth_case(state, cpu, OPCODE_CPSNE | OPCODE_COMPARE_SKIP_BYTE,
                            0x8000u, 0x0001u, true,
                            "CPSNE byte takes unequal low bytes");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT, 0x0001u, 0xffffu, true,
                            "CPSGT word uses signed greater-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT, 0xffffu, 0x0001u, false,
                            "CPSGT word rejects signed reverse order");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT | OPCODE_COMPARE_SKIP_BYTE,
                            0x007fu, 0x0080u, true,
                            "CPSGT byte uses signed greater-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSGT | OPCODE_COMPARE_SKIP_BYTE,
                            0x0080u, 0x007fu, false,
                            "CPSGT byte rejects signed reverse order");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT, 0xffffu, 0x0001u, true,
                            "CPSLT word uses signed less-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT, 0x0001u, 0xffffu, false,
                            "CPSLT word rejects signed reverse order");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT | OPCODE_COMPARE_SKIP_BYTE,
                            0x0080u, 0x007fu, true, "CPSLT byte uses signed less-than");
    compare_skip_truth_case(state, cpu, OPCODE_CPSLT | OPCODE_COMPARE_SKIP_BYTE,
                            0x007fu, 0x0080u, false,
                            "CPSLT byte rejects signed reverse order");

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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u &&
               cpu->w[15] == 0x5000u && cpu->call_depth == 0u &&
               cpu->illegal_reset_count == 0u,
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
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x340u &&
               cpu->cycles == 2u && cpu->last_trap == 1u &&
               cpu->last_trap_return == 0x557feu && cpu->w[15] == 0x5004u,
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
               cpu->cycles == 3u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT &&
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
               cpu->cycles == 1u &&
               cpu->sequential_program_hole_pc == DSPIC33_PROGRAM_LIMIT,
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
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->pc == 0x340u &&
               cpu->cycles == 5u && cpu->last_trap == 1u &&
               cpu->last_trap_return == 0x557c4u && cpu->w[0] == 0x5a5au &&
               cpu->w[1] == 0x5a5au,
           "taken compare branch validates an unimplemented target");

    reset_processor_test(cpu, 0x557c2u);
    load_instruction(state, cpu, 0x557c2u, opcode);
    cpu->w[0] = 0x5a5au;
    cpu->w[1] = 0xa5a5u;
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x557c4u &&
               cpu->cycles == 1u && cpu->last_trap == UINT16_MAX && cpu->sr == 0x010fu,
           "untaken compare branch does not validate its encoded target");
}

static void prepare_timer_source(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x0110u, 0x0008u);
    dspic33_write_word(cpu, 0x0108u, 0x5555u);
    dspic33_write_word(cpu, 0x0106u, 0x1234u);
    dspic33_write_word(cpu, 0x0108u, 0xaaaau);
}

static void completed_source_address_error_case(TestState* state, Dspic33* cpu,
                                                uint32_t opcode, uint16_t stacked_flags,
                                                const char* execution,
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
    expect(state, cpu->w[1] == 0x1001u,
           "odd post-decrement write inhibits address update");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    cpu->w[1] = 0x1003u;
    cpu->w[2] = 0xa5a5u;
    expect_address_trap(state, cpu, "odd pre-decrement word write traps");
    expect(state, cpu->w[1] == 0x1003u,
           "odd pre-decrement write inhibits address update");

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
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "odd ordinary byte access remains valid");
    expect(state, cpu->w[1] == 0x1002u && (cpu->w[2] & 0x00ffu) == 0x005au,
           "odd ordinary byte access reads and updates pointer");

    reset_processor_test(cpu, 0u);
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

static void data_map_address_error_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0xdffeu, 0xa5a5u);
    cpu->w[1] = 0xdffeu;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "last implemented word read completes");
    expect(state, cpu->w[1] == 0xe000u && cpu->w[2] == 0xa5a5u,
           "last implemented word read updates result and pointer");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_POST_INCREMENT);
    cpu->w[1] = 0xdffeu;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "last implemented word write completes");
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
           cpu->w[1] == 0xe002u && cpu->w[2] == 0u &&
               dspic33_read_word(cpu, 0xe000u) == 0xbeefu,
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
           cpu->w[1] == 0x8000u && cpu->w[2] == 0u && cpu->dsrpag == 2u &&
               cpu->dswpag == 1u && dspic33_read_word(cpu, 0xfffeu) == 0xbeefu,
           "unimplemented read advances EDS read page only");

    reset_processor_test(cpu, 0u);
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

    reset_processor_test(cpu, 0u);
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

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    cpu->w[1] = 0x0056u;
    cpu->w[2] = 0x5a5au;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "unused SFR hole read remains valid");
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
    expect(state,
           cpu->w[1] == 0x8000u && cpu->w[2] == 0x125au && cpu->dsrpag == 0x0201u,
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
    expect(state,
           cpu->w[1] == 0x8000u && cpu->w[2] == 0x12a5u && cpu->dsrpag == 0x0201u,
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
    expect(state,
           cpu->w[1] == 0xffffu && cpu->w[2] == 0x12a5u && cpu->dsrpag == 0x0200u,
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
    expect(state,
           cpu->w[1] == 0xffffu && cpu->w[2] == 0x125au && cpu->dsrpag == 0x0200u,
           "byte post-decrement reads the original PSV page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x8000u, 0x005566u),
           "load word pre-increment PSV value");
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u &&
               cpu->w[2] == 0x5566u && cpu->dsrpag == 0x0201u,
           "word pre-increment reads after the page transition");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x002233u),
           "load word post-increment PSV value");
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u &&
               cpu->w[2] == 0x2233u && cpu->dsrpag == 0x0201u,
           "word post-increment reads before the page transition");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[0] = 0x1111u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x01ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0u &&
               cpu->w[2] == 0x1111u && cpu->dsrpag == 0x01ffu,
           "last EDS read page overflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[0] = 0x2222u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x03ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0u &&
               cpu->w[2] == 0x2222u && cpu->dsrpag == 0x03ffu,
           "last PSV read page overflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    cpu->w[0] = 0x5555u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0u &&
               cpu->w[2] == 0x5555u && cpu->dsrpag == 0u && !cpu->address_error,
           "page zero pre-increment wraps into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_DECREMENT_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0x3333u);
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0001u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu &&
               cpu->w[2] == 0x3333u && cpu->dsrpag == 0x0001u,
           "first EDS read page underflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_DECREMENT_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0x4444u);
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu &&
               cpu->w[2] == 0x4444u && cpu->dsrpag == 0x0200u,
           "first PSV read page underflows into base data space");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    expect(state, dspic33_load_program_word(cpu, 0u, 0x5a0000u),
           "load high-byte PSV transition value");
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x02ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u &&
               cpu->w[2] == 0x005au && cpu->dsrpag == 0x0300u,
           "PSV low-word page transitions into high-byte page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_DECREMENT_W2);
    load_instruction(state, cpu, 0x7ffffeu, 0x005a5au);
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0300u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xfffeu &&
               cpu->w[2] == 0x5a5au && cpu->dsrpag == 0x02ffu,
           "PSV high-byte page underflows into low-word page");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_INCREMENT);
    cpu->w[1] = 0xfffeu;
    cpu->w[2] = 0x6666u;
    cpu->dswpag = 0x01ffu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x6666u &&
               cpu->w[1] == 0u && cpu->dswpag == 0x01ffu,
           "last EDS write page overflows into base data space");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    dspic33_write_word(cpu, 0x7ffeu, 0xaaaau);
    cpu->w[1] = 0x8000u;
    cpu->w[2] = 0x7777u;
    cpu->dswpag = 0x0001u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu &&
               cpu->dswpag == 0x0001u && dspic33_read_word(cpu, 0x7ffeu) == 0x7777u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xfff8u &&
               cpu->w[2] == 0x6666u && cpu->dsrpag == 0x0200u,
           "modulo wrap leaves the PSV page unchanged");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W1_W0_OFFSET_W2);
    expect(state, dspic33_load_program_word(cpu, 0u, 0x00abcdu),
           "load indexed wrap PSV value");
    cpu->w[0] = 2u;
    cpu->w[1] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xfffeu &&
               cpu->w[2] == 0xabcdu && cpu->dsrpag == 0x0200u,
           "indexed overflow wraps within the current PSV page");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W1_W0_OFFSET_W2);
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x00bcdeu),
           "load indexed underflow PSV value");
    cpu->w[0] = 0xfffeu;
    cpu->w[1] = 0x8000u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u &&
               cpu->w[2] == 0xbcdeu && cpu->dsrpag == 0x0200u,
           "indexed underflow wraps within the current PSV page");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W15_W0_OFFSET_W2);
    cpu->w[0] = 2u;
    cpu->w[15] = 0xfffeu;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0xfffeu &&
               cpu->w[2] == 2u && cpu->dsrpag == 0x0200u,
           "indexed W15 overflow remains in base data space");

    reset_processor_test(cpu, 0x0600u);
    load_instruction(state, cpu, 0x0600u, OPCODE_MOV_W15_W0_OFFSET_W2);
    dspic33_write_word(cpu, 0x7ffeu, 0xdef0u);
    cpu->w[0] = 0xfffeu;
    cpu->w[15] = 0x8000u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0x8000u &&
               cpu->w[2] == 0xdef0u && cpu->dsrpag == 0x0200u,
           "indexed W15 underflow remains in base data space");

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W2_W1_PRE_DECREMENT);
    cpu->w[1] = 0x8000u;
    cpu->w[2] = 0xa5a5u;
    cpu->dswpag = 0x0002u;
    expect_address_trap(state, cpu, "pre-decrement write page transition traps");
    expect(state,
           cpu->w[1] == 0xfffeu && cpu->w[2] == 0xa5a5u && cpu->dswpag == 0x0001u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x8000u &&
               cpu->w[2] == 0x5566u && cpu->w[3] == 0x7788u && cpu->dsrpag == 0x0201u &&
               !cpu->address_error,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x7ffeu &&
               cpu->w[2] == 0x1111u && cpu->w[3] == 0x2222u && cpu->dsrpag == 0x0200u &&
               !cpu->address_error,
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
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au && cpu->dsrpag == 0u &&
               cpu->dswpag == 1u,
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
               dspic33_read_word(cpu, 0x1000u) == 0xa5a5u && cpu->dsrpag == 1u &&
               cpu->dswpag == 0u,
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
               dspic33_read_word(cpu, 0x1000u) == 0x5a5au && cpu->dsrpag == 0u &&
               cpu->dswpag == 1u,
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
               dspic33_read_word(cpu, 0x1000u) == 0xa55au && cpu->dsrpag == 1u &&
               cpu->dswpag == 0u,
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

static void unimplemented_data_page_address_error_cases(TestState* state,
                                                        Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x9000u, 0x5a5au);
    cpu->dsrpag = 1u;
    cpu->w[1] = 0x9000u;
    cpu->w[2] = 0xa5a5u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "implemented EDS page word read completes");
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
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "direct high-file page-one read completes");
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
           cpu->w[1] == 0x9002u && cpu->w[2] == 0u && cpu->dsrpag == 2u &&
               cpu->dswpag == 1u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
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
           cpu->w[1] == 0x9002u && cpu->w[2] == 0xa5a5u && cpu->dsrpag == 1u &&
               cpu->dswpag == 2u && dspic33_read_word(cpu, 0x11000u) == 0x5a5au,
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

static void valid_stack_frame_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_LNK_0);
    load_instruction(state, cpu, 0x202u, OPCODE_ULNK);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
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
    expect(state, active_pending_traps(cpu) == 2u,
           "simultaneous trap sources remain distinct");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "service simultaneous trap boundary");
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
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "service earlier math deadline");
    expect(state, cpu->last_trap == 4u && cpu->pc == 0x000320u,
           "earlier math deadline precedes later stack priority");
    pending = pending_trap(cpu, 3u);
    expect(state, pending != NULL && pending->delay == 1u,
           "later stack deadline remains pending");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "advance math handler to stack deadline");
    expect(state, cpu->last_trap == 3u && cpu->pc == 0x000300u && cpu->trap_count == 2u,
           "ready stack trap preempts math handler");
}

static void repeat_exception_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t divide_opcodes[] = {OPCODE_DIV_SW_W2_W3, OPCODE_DIV_SD_W4_W3,
                                              OPCODE_DIV_UW_W2_W3, OPCODE_DIV_UD_W4_W3,
                                              OPCODE_DIVF_W2_W3};
    size_t index;

    for (index = 0u; index < sizeof(divide_opcodes) / sizeof(divide_opcodes[0]);
         index++) {
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
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "initialize repeated divide");
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u &&
                   cpu->rcount == 16u && cpu->repeat_active != 0u &&
                   (dspic33_read_word(cpu, 0x08c0u) & 0x0050u) == 0x0050u &&
                   pending_trap(cpu, 4u) != NULL && pending_trap(cpu, 4u)->delay == 1u,
               "first divide cycle latches delayed math trap");
        expect(state,
               dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 4u &&
                   cpu->last_trap_return == 0x202u && cpu->pc == 0x000320u &&
                   cpu->rcount == 15u && cpu->repeat_active == 0u &&
                   (cpu->sr & 0x0010u) == 0u && cpu->cycles == 3u,
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
    expect(state, (cpu->sr & 0x0004u) != 0u,
           "unaffected signed double divide overflow sets OV");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x00000cu, 0x000300u);
    load_instruction(state, cpu, 0x000300u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x200u, 0x090011u);
    load_instruction(state, cpu, 0x202u, OPCODE_DIV_SW_W2_W3);
    cpu->w[2] = 42u;
    cpu->w[3] = 0u;
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = false;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "initialize recursive math repeat");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "latch recursive math source");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
               cpu->trap_count == 1u && cpu->rcount == 15u,
           "enter first repeated divide math trap");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
               cpu->trap_count == 2u && cpu->last_trap_return == 0x202u &&
               cpu->rcount == 15u && cpu->w[15] == 0x5004u && cpu->cycles == 8u,
           "uncleared MATHERR re-enters before repeated instruction");
    dspic33_write_word(cpu, 0x08c0u, 0x0040u);
    expect(state,
           dspic33_read_word(cpu, 0x08c0u) == 0x0040u && pending_trap(cpu, 4u) == NULL,
           "clearing MATHERR preserves cause and clears level source");
    dspic33_write_word(cpu, 0x08c0u, 0u);
    expect(state, dspic33_read_word(cpu, 0x08c0u) == 0u,
           "software clears independent DIV0ERR cause");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u &&
               cpu->repeat_active != 0u && cpu->repeat_pc == 0x202u &&
               cpu->rcount == 15u && (cpu->sr & 0x0010u) != 0u && cpu->cycles == 14u,
           "RETFIE restores suspended repeat state");

    dspic33_write_word(cpu, 0x08c0u, 0x0010u);
    expect(state,
           dspic33_read_word(cpu, 0x08c0u) == 0x0010u && pending_trap(cpu, 4u) == NULL,
           "software MATHERR stores status without creating a trap source");
    dspic33_write_word(cpu, 0x08c0u, 0u);
    expect(state,
           dspic33_read_word(cpu, 0x08c0u) == 0u && pending_trap(cpu, 4u) == NULL,
           "software MATHERR clear cancels pending level source");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x090010u);
    load_instruction(state, cpu, 0x202u, OPCODE_DIV_SW_W2_W3);
    cpu->w[2] = 42u;
    cpu->w[3] = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "initialize short repeated divide");
    while (cpu->repeat_active != 0u) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "execute short repeated divide cycle");
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
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "initialize long repeated divide");
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
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "initialize interruptible repeat");
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0004u);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000302u &&
               cpu->repeat_active == 0u && cpu->rcount == 2u &&
               (cpu->sr & 0x0010u) == 0u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x1000u) != 0u,
           "interrupt entry suspends repeat and stacks RA");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u &&
               cpu->repeat_active != 0u && cpu->repeat_pc == 0x202u &&
               cpu->rcount == 2u && (cpu->sr & 0x0010u) != 0u,
           "interrupt RETFIE restores repeat state");
}

static void standalone_divide_zero_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t divide_opcodes[] = {OPCODE_DIV_SW_W2_W3, OPCODE_DIV_SD_W4_W3,
                                              OPCODE_DIV_UW_W2_W3, OPCODE_DIV_UD_W4_W3,
                                              OPCODE_DIVF_W2_W3};
    size_t index;

    for (index = 0u; index < sizeof(divide_opcodes) / sizeof(divide_opcodes[0]);
         index++) {
        reset_processor_test(cpu, 0x0200u);
        load_instruction(state, cpu, 0x0200u, divide_opcodes[index]);
        cpu->w[0] = 0xaaaau;
        cpu->w[1] = 0xbbbbu;
        cpu->w[2] = 0x2222u;
        cpu->w[3] = 0u;
        cpu->w[4] = 0x4444u;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (dspic33_read_word(cpu, 0x08c0u) & 0x0050u) == 0x0050u &&
                   cpu->w[0] == 0xaaaau && cpu->w[1] == 0xbbbbu &&
                   pending_trap(cpu, 4u) != NULL,
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
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 2u &&
               cpu->cycles == 1u,
           "DISI initializes integrated repeat interrupt window");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 1u &&
               cpu->cycles == 2u && (dspic33_read_word(cpu, 0x0802u) & 0x0010u) != 0u,
           "word IFS write consumes one disabled cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->disicnt == 0u &&
               cpu->repeat_active != 0u && cpu->repeat_pc == 0x206u &&
               cpu->rcount == 2u && cpu->cycles == 3u,
           "REPEAT consumes final disabled cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000302u &&
               cpu->last_interrupt == 20u && cpu->w[15] == 0x5004u &&
               dspic33_read_word(cpu, 0x5000u) == 0x206u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x1000u) != 0u &&
               cpu->repeat_active == 0u && cpu->rcount == 2u &&
               (cpu->sr & 0x00f0u) == 0x0080u && cpu->cycles == 12u,
           "integrated interrupt suspends repeat at target");
    dspic33_write_word(cpu, 0x0802u, 0u);
    dspic33_write_word(cpu, 0x0822u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x206u &&
               cpu->w[15] == 0x5000u && cpu->repeat_active != 0u &&
               cpu->repeat_pc == 0x206u && cpu->rcount == 2u &&
               (cpu->sr & 0x0010u) != 0u && cpu->cycles == 18u,
           "integrated RETFIE restores repeat state in six cycles");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 1u &&
               cpu->rcount == 1u && cpu->pc == 0x206u && cpu->cycles == 19u,
           "restored repeat executes first target");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 2u &&
               cpu->rcount == 0u && cpu->pc == 0x206u && cpu->cycles == 20u,
           "restored repeat executes second target");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 3u &&
               cpu->repeat_active == 0u && cpu->pc == 0x208u &&
               (cpu->sr & 0x0010u) == 0u && cpu->cycles == 21u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000302u &&
               cpu->w[15] == 0x5004u && cpu->rcount == 2u && cpu->repeat_active == 0u &&
               (cpu->sr & 0x00f0u) == 0x0080u && cpu->cycles == 12u,
           "early-termination handler observes suspended repeat");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000304u &&
               cpu->rcount == 0u && cpu->cycles == 13u,
           "handler clears suspended RCOUNT");
    dspic33_write_word(cpu, 0x0802u, 0u);
    dspic33_write_word(cpu, 0x0822u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x206u &&
               cpu->w[15] == 0x5000u && cpu->repeat_active != 0u &&
               cpu->repeat_pc == 0x206u && cpu->rcount == 0u &&
               (cpu->sr & 0x0010u) != 0u && cpu->cycles == 19u,
           "RETFIE restores prefetched target after RCOUNT clear");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 1u &&
               cpu->pc == 0x208u && cpu->rcount == 0u && cpu->repeat_active == 0u &&
               (cpu->sr & 0x0010u) == 0u && cpu->cycles == 20u,
           "cleared repeat executes final prefetched target once");
}

static void prepare_nested_do_interrupt_case(TestState* state, Dspic33* cpu,
                                             uint32_t entry, bool nesting_disabled) {
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->cycles == 10u && cpu->interrupt_depth == 1u &&
               (cpu->nested_do_extra_decrement_depth != 0u) ==
                   expected_extra_decrement &&
               cpu->nested_do_interrupt_armed == expected_armed,
           "first DO-loop interrupt entry evaluates nested request timing");
    dspic33_write_word(cpu, 0x0800u, (uint16_t)(dspic33_read_word(cpu, 0x0800u) & ~1u));
}

static void enter_first_nested_do_interrupt(TestState* state, Dspic33* cpu,
                                            bool expected_armed, uint8_t nested_delay,
                                            bool expected_extra_decrement) {
    dspic33_raise_interrupt(cpu, 0u);
    expect(state, cpu->nested_do_interrupt_armed == expected_armed,
           "first DO-loop interrupt request records the erratum window");
    if (nested_delay != 0u) {
        expect(state,
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, nested_delay),
               "schedule higher-priority request inside interrupt entry");
    }
    complete_first_nested_do_interrupt_entry(
        state, cpu, expected_armed && nested_delay == 0u, expected_extra_decrement);
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
    expect(state, cpu->interrupt_depth == 0u,
           "nested DO-loop interrupt stack fully unwinds");
    main_steps = (uint8_t)(((0x208u - cpu->pc) / 2u) + 1u);
    for (index = 0u; index < main_steps; index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "interrupted DO-loop reaches its iteration boundary");
    }
    expect(state,
           cpu->pc == 0x204u && cpu->do_depth == 1u &&
               cpu->dcount == (expected_extra_decrement ? 1u : 2u) &&
               cpu->do_count[0] == cpu->dcount &&
               cpu->nested_do_extra_decrement_depth == 0u &&
               cpu->nested_do_extra_decrement_end == 0u &&
               !cpu->nested_do_interrupt_armed,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x208u &&
               cpu->cycles == 1u && cpu->interrupt_depth == 0u &&
               cpu->nested_do_interrupt_armed &&
               cpu->nested_do_interrupt_end == 0x208u &&
               cpu->nested_do_interrupt_depth == 1u,
           "penultimate DO instruction event records the executing address");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->cycles == 11u && cpu->interrupt_depth == 1u &&
               cpu->nested_do_extra_decrement_depth == 1u &&
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x204u &&
               cpu->dcount == 2u && cpu->nested_do_interrupt_armed &&
               cpu->nested_do_interrupt_end == 0x208u,
           "final DO instruction event survives the loop-back PC update");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->nested_do_extra_decrement_depth == 1u &&
               !cpu->nested_do_interrupt_armed,
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
               cpu->nested_do_extra_decrement_depth == 0u &&
               !cpu->nested_do_interrupt_armed,
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
                   cpu->nested_do_extra_decrement_depth == 1u &&
                   !cpu->nested_do_interrupt_armed,
               "DOZE and ROI preserve the four-instruction-cycle erratum window");
    }

    for (entry_index = 0u; entry_index < sizeof(entries) / sizeof(entries[0]);
         entry_index++) {
        for (delay_index = 0u; delay_index < sizeof(delays) / sizeof(delays[0]);
             delay_index++) {
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
    for (delay_index = 0u; cpu->interrupt_depth != 0u && delay_index < 8u;
         delay_index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "single DO-loop interrupt handler returns normally");
    }
    expect(state,
           cpu->interrupt_depth == 0u && cpu->pc == 0x208u &&
               cpu->nested_do_interrupt_armed,
           "single DO-loop interrupt retains its window until the loop boundary");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->dcount == 2u &&
               !cpu->nested_do_interrupt_armed &&
               cpu->nested_do_interrupt_cycle == 0u &&
               cpu->nested_do_interrupt_end == 0u &&
               cpu->nested_do_interrupt_depth == 0u &&
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
    dspic33_destroy(&copy);

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, false);
    enter_first_nested_do_interrupt(state, cpu, true, 0u, false);
    load_instruction(state, cpu, 0x302u, OPCODE_RESET);
    dspic33_write_word(cpu, 0x0800u, 0u);
    dspic33_write_word(cpu, 0x0820u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->software_reset_count == 1u &&
               !cpu->nested_do_interrupt_armed &&
               cpu->nested_do_interrupt_cycle == 0u &&
               cpu->nested_do_interrupt_end == 0u &&
               cpu->nested_do_interrupt_depth == 0u &&
               cpu->nested_do_interrupt_priority == 0u &&
               cpu->nested_do_extra_decrement_depth == 0u &&
               cpu->nested_do_extra_decrement_end == 0u,
           "warm reset clears nested DO-loop erratum state");
}

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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0x87a5u &&
               cpu->sr == 0x0105u && cpu->cycles == 1u &&
               (cpu->initialized_working_registers & 0x0002u) != 0u,
           "SWAP exchanges bytes and initializes word destination");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SWAP_BYTE_W1);
    cpu->initialized_working_registers &= (uint16_t)~0x0002u;
    cpu->w[1] = 0xa587u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xa578u &&
               cpu->sr == 0x0105u && cpu->cycles == 1u &&
               (cpu->initialized_working_registers & 0x0002u) == 0u,
           "SWAP.B exchanges low nibbles without initializing byte destination");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SWAP_W15);
    cpu->w[15] = 0xa586u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0x86a4u &&
               cpu->sr == 0x0105u && cpu->cycles == 1u,
           "SWAP keeps stack pointer even");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_SWAP_BYTE_W15);
    cpu->w[15] = 0xa586u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[15] == 0xa568u &&
               cpu->sr == 0x0105u && cpu->cycles == 1u,
           "SWAP.B keeps stack pointer even");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_EXCH_W1_W2);
    cpu->initialized_working_registers &= (uint16_t)~0x0006u;
    cpu->w[1] = 0x1234u;
    cpu->w[2] = 0xa5a5u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xa5a5u &&
               cpu->w[2] == 0x1234u && cpu->sr == 0x0105u && cpu->cycles == 1u &&
               (cpu->initialized_working_registers & 0x0006u) == 0x0006u,
           "EXCH swaps registers and initializes both destinations");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_EXCH_W1_W1);
    cpu->initialized_working_registers &= (uint16_t)~0x0002u;
    cpu->w[1] = 0xa5a5u;
    cpu->sr = 0x0105u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xa5a5u &&
               cpu->sr == 0x0105u && cpu->cycles == 1u &&
               (cpu->initialized_working_registers & 0x0002u) != 0u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x1000u) == 0x8000u && cpu->w[0] == 0x5a5au &&
               cpu->sr == 0x010du && cpu->cycles == 1u && cpu->io.cpu_write_valid &&
               cpu->io.cpu_write_address == 0x1000u && cpu->io.cpu_write_width == 2u &&
               cpu->io.cpu_write_previous == 0x8000u,
           "word file destination writes back RAM before updating flags");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_BYTE_0X1001);
    dspic33_write_word(cpu, 0x1000u, 0x8000u);
    cpu->w[0] = 0x5a5au;
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x1000u) == 0x8000u && cpu->w[0] == 0x5a5au &&
               cpu->sr == 0x010du && cpu->cycles == 1u && cpu->io.cpu_write_valid &&
               cpu->io.cpu_write_address == 0x1001u && cpu->io.cpu_write_width == 1u &&
               cpu->io.cpu_write_previous == 0x0080u,
           "byte file destination writes back RAM before updating flags");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_WORD_W0);
    cpu->w[0] = 0u;
    cpu->initialized_working_registers &= (uint16_t)~0x0001u;
    cpu->sr = 0x010du;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0u &&
               cpu->sr == 0x0107u && cpu->cycles == 1u &&
               (cpu->initialized_working_registers & 0x0001u) != 0u &&
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x0080u &&
               cpu->sr == 0x010du && cpu->cycles == 1u &&
               (cpu->initialized_working_registers & 0x0001u) == 0u &&
               cpu->io.cpu_write_valid && cpu->io.cpu_write_address == 0u &&
               cpu->io.cpu_write_width == 1u,
           "byte file destination preserves uninitialized working-register alias");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X1000_WREG);
    dspic33_write_word(cpu, 0x1000u, 0x8000u);
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x8000u &&
               cpu->sr == 0x010du && cpu->cycles == 1u && !cpu->io.cpu_write_valid,
           "word WREG destination does not write back source file");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_0X1001_WREG);
    dspic33_write_word(cpu, 0x1000u, 0x8000u);
    cpu->w[0] = 0x5a5au;
    cpu->sr = 0x0107u;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 0x5a80u &&
               cpu->sr == 0x010du && cpu->cycles == 1u && !cpu->io.cpu_write_valid,
           "byte WREG destination does not write back source file");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_FILE_WORD_CORCON);
    cpu->sr = 0x010du;
    cpu->io.cpu_write_valid = false;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->corcon == 0x0020u &&
               cpu->sr == 0x0105u && cpu->cycles == 1u && cpu->io.cpu_write_valid,
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
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0e14u) == 0u && cpu->w[0] == 0x5a5au &&
               cpu->sr == 0x0107u && cpu->cycles == 2u && cpu->io.cpu_write_valid &&
               cpu->io.cpu_write_address == 0x0e12u && cpu->io.cpu_write_width == 2u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0e14u) == 0u && cpu->w[0] == 0x5a5au &&
               cpu->sr == 0x0107u && cpu->cycles == 2u && cpu->io.cpu_write_valid &&
               cpu->io.cpu_write_address == 0x0e12u && cpu->io.cpu_write_width == 1u,
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
        {OPCODE_MOV_DOUBLE_W4_POST_DECREMENT_W2, 0x5004u, 0x5004u, 0x5000u, 0x1112u,
         0x2212u, "MOV.D source post-decrement"},
        {OPCODE_MOV_DOUBLE_W4_POST_INCREMENT_W2, 0x5000u, 0x5000u, 0x5004u, 0x1113u,
         0x2213u, "MOV.D source post-increment"},
        {OPCODE_MOV_DOUBLE_W4_PRE_DECREMENT_W2, 0x5004u, 0x5000u, 0x5000u, 0x1114u,
         0x2214u, "MOV.D source pre-decrement"},
        {OPCODE_MOV_DOUBLE_W4_PRE_INCREMENT_W2, 0x4ffcu, 0x5000u, 0x5000u, 0x1115u,
         0x2215u, "MOV.D source pre-increment"},
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
        {OPCODE_MOV_DOUBLE_W2_W4_POST_DECREMENT, 0x5004u, 0x5004u, 0x5000u, 0x5000u,
         0x3312u, 0x4412u, "MOV.D destination post-decrement"},
        {OPCODE_MOV_DOUBLE_W2_W4_POST_INCREMENT, 0x5000u, 0x5000u, 0x5004u, 0x5004u,
         0x3313u, 0x4413u, "MOV.D destination post-increment"},
        {OPCODE_MOV_DOUBLE_W2_W4_PRE_DECREMENT, 0x5004u, 0x5000u, 0x5000u, 0x5004u,
         0x3314u, 0x4414u, "MOV.D destination pre-decrement"},
        {OPCODE_MOV_DOUBLE_W2_W4_PRE_INCREMENT, 0x4ffcu, 0x5000u, 0x5000u, 0x4ffcu,
         0x3315u, 0x4415u, "MOV.D destination pre-increment"},
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
        dspic33_write_word(cpu, source_cases[index].access_address,
                           source_cases[index].low);
        dspic33_write_word(cpu, source_cases[index].access_address + 2u,
                           source_cases[index].high);
        dspic33_set_working_register(cpu, 4u, source_cases[index].initial_pointer);
        cpu->sr = 0x010fu;
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   cpu->w[2] == source_cases[index].low &&
                   cpu->w[3] == source_cases[index].high &&
                   cpu->w[4] == source_cases[index].final_pointer &&
                   cpu->sr == 0x010fu && cpu->cycles == 2u,
               source_cases[index].name);
    }

    for (index = 0u; index < sizeof(destination_cases) / sizeof(destination_cases[0]);
         index++) {
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
                   dspic33_read_word(cpu, destination_cases[index].access_address +
                                              2u) == destination_cases[index].high &&
                   dspic33_read_word(cpu, destination_cases[index].preserved_address) ==
                       0xdeadu &&
                   cpu->w[4] == destination_cases[index].final_pointer &&
                   cpu->sr == 0x010fu && cpu->cycles == 2u,
               destination_cases[index].name);
    }

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_POST_INCREMENT_W2);
    dspic33_write_word(cpu, 0x5000u, 0x5511u);
    dspic33_write_word(cpu, 0x5002u, 0x6622u);
    dspic33_set_working_register(cpu, 2u, 0x5000u);
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5511u &&
               cpu->w[3] == 0x6622u && cpu->sr == 0x010fu && cpu->cycles == 2u,
           "MOV.D overlapping load writes destination after source pointer update");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W2_POST_INCREMENT);
    dspic33_write_word(cpu, 0x5000u, 0xdeadu);
    dspic33_write_word(cpu, 0x5002u, 0xdeadu);
    dspic33_set_working_register(cpu, 2u, 0x5000u);
    dspic33_set_working_register(cpu, 3u, 0x7788u);
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5004u &&
               cpu->w[3] == 0x7788u && dspic33_read_word(cpu, 0x5000u) == 0x5000u &&
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
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->disicnt == 0u,
           "non-CPU SFR wait cycle completes DISI countdown");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               cpu->w[15] == 0x5004u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->interrupt_count == 0u,
           "nested non-CPU SFR wait retains new interrupt deferral");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x204u &&
               cpu->interrupt_count == 0u,
           "nested new interrupt deferral spans following instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               cpu->interrupt_count == 1u,
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
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[2] == 0x1234u,
           "direct non-CPU SFR read consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0x0800u);
    dspic33_write_word(cpu, 0x0800u, 0x4321u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[1] == 0x0802u && cpu->w[2] == 0x4321u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
               cpu->corcon == 0x0022u,
           "CPU SFR read-modify-write remains one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W4_W6);
    dspic33_set_working_register(cpu, 4u, 0x0800u);
    dspic33_write_word(cpu, 0x0800u, 0x1357u);
    dspic33_write_word(cpu, 0x0802u, 0x2468u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[6] == 0x1357u && cpu->w[7] == 0x2468u,
           "double non-CPU SFR read retains two-cycle base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W4_W6);
    dspic33_set_working_register(cpu, 4u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x1357u);
    dspic33_write_word(cpu, 0x1002u, 0x2468u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[6] == 0x1357u && cpu->w[7] == 0x2468u,
           "double RAM read retains two-cycle base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W2_W1_POST_INCREMENT);
    dspic33_set_working_register(cpu, 1u, 0x0800u);
    cpu->w[2] = 0x1357u;
    cpu->w[3] = 0x2468u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[1] == 0x0804u && dspic33_read_word(cpu, 0x0800u) == 0x1357u &&
               dspic33_read_word(cpu, 0x0802u) == 0x2468u,
           "double non-CPU SFR write retains two-cycle base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u && cpu->pc == 2u,
           "non-taken non-CPU SFR bit skip adds one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_write_word(cpu, 0x0800u, 1u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 3u && cpu->pc == 4u,
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
           cpu->cycles == UINT64_MAX && cpu->disicnt == 1u &&
               cpu->pending_soft_traps[0].active &&
               cpu->pending_soft_traps[0].delay == 3u,
           "failed final non-CPU SFR wait cycle inhibits trap bookkeeping");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_IFS0_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 4u, 0u);
    dspic33_write_word(cpu, 0x0800u, 1u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 4u && cpu->pc == 6u,
           "two-word non-CPU SFR bit skip adds one cycle");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_PUSH_IFS0);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0800u, 0x55aau);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[15] == 0x5002u && dspic33_read_word(cpu, 0x5000u) == 0x55aau,
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
               dspic33_read_word(cpu, 0x0108u) == 0x5555u &&
               cpu->data[0x0106u] == 0xffu && cpu->data[0x0107u] == 0xffu,
           "SETM non-CPU SFR is a one-cycle pure write");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0x0056u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
               cpu->w[1] == 0x0058u && cpu->w[2] == 0u,
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
    expect(state, dspic33_load_program_word(cpu, 0x4000u, 0x001357u),
           "load PSV word timing value");
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    cpu->disicnt = 6u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1357u &&
               cpu->cycles == 5u && cpu->disicnt == 1u,
           "PSV word read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->w[2] = 0xa500u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xc001u &&
               cpu->w[2] == 0xa557u && cpu->cycles == 5u,
           "PSV byte read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xc000u &&
               cpu->w[2] == 0x1357u && cpu->cycles == 5u,
           "PSV pre-increment read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W0_OFFSET_W2);
    dspic33_set_working_register(cpu, 0u, 2u);
    dspic33_set_working_register(cpu, 1u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[0] == 2u &&
               cpu->w[1] == 0xbffeu && cpu->w[2] == 0x1357u && cpu->cycles == 5u,
           "PSV indexed read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W4_LITERAL_2_W2);
    dspic33_set_working_register(cpu, 4u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x1357u &&
               cpu->w[4] == 0xbffeu && cpu->cycles == 5u,
           "PSV literal-offset read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_0X9000_W2);
    expect(state, dspic33_load_program_word(cpu, 0x1000u, 0x005a5au),
           "load direct PSV timing value");
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x5a5au &&
               cpu->cycles == 5u,
           "direct PSV read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_DOUBLE_W4_W6);
    expect(state, dspic33_load_program_word(cpu, 0x4002u, 0x002468u),
           "load second PSV double timing value");
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[6] == 0x1357u &&
               cpu->w[7] == 0x2468u && cpu->cycles == 5u,
           "double PSV read consumes five total cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_ADD_W2_W4_POST_INCREMENT_W5);
    dspic33_set_working_register(cpu, 2u, 1u);
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0xc002u &&
               cpu->w[5] == 0x1358u && cpu->cycles == 5u,
           "arithmetic PSV source consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    dspic33_write_word(cpu, 0xc000u, 0x7777u);
    cpu->dsrpag = 1u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x7777u &&
               cpu->cycles == 1u,
           "EDS data read retains base timing");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOV_W1_W2);
    expect(state, dspic33_load_program_word(cpu, 0x4000u, 0xab1357u),
           "load PSV high-byte timing value");
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0300u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[2] == 0x00abu &&
               cpu->cycles == 5u,
           "PSV high-byte read consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    expect(state, dspic33_load_program_word(cpu, 0x4004u, 0x001356u),
           "load clear PSV skip bit");
    dspic33_set_working_register(cpu, 4u, 0xc004u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
               cpu->w[4] == 0xc006u && cpu->cycles == 5u,
           "untaken PSV bit skip consumes five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 4u &&
               cpu->w[4] == 0xc002u && cpu->cycles == 5u,
           "one-word PSV bit skip remains five cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_BTSS_W4_POST_INCREMENT_BIT_0);
    load_instruction(state, cpu, 2u, OPCODE_CALL_0X300);
    load_instruction(state, cpu, 4u, 0u);
    dspic33_set_working_register(cpu, 4u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 6u &&
               cpu->w[4] == 0xc002u && cpu->cycles == 5u,
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
        expect(state,
               dspic33_load_program_word(cpu, 0x4000u + (uint32_t)index * 2u,
                                         values[index]),
               "load repeated PSV timing value");
    }
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "REPEAT setup precedes optimized PSV access");
    expect_step_cycles(state, cpu, 5u,
                       "first repeated PSV postincrement uses five cycles");
    expect_step_cycles(state, cpu, 1u,
                       "middle repeated PSV postincrement uses one cycle");
    expect_step_cycles(state, cpu, 6u,
                       "last repeated PSV postincrement uses six cycles");
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
    expect_step_cycles(state, cpu, 5u,
                       "first repeated PSV postdecrement uses five cycles");
    expect_step_cycles(state, cpu, 1u,
                       "middle repeated PSV postdecrement uses one cycle");
    expect_step_cycles(state, cpu, 6u,
                       "last repeated PSV postdecrement uses six cycles");
    expect(state, cpu->w[1] == 0xc000u && cpu->w[2] == 0x2222u,
           "optimized PSV postdecrement reads each word");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_BYTE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "byte PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u,
                       "first repeated byte PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u,
                       "middle repeated byte PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u,
                       "last repeated byte PSV access uses five cycles");
    expect(state, cpu->w[1] == 0xc003u,
           "byte PSV postincrement remains outside optimized schedule");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_PRE_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xbffeu);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "preincrement PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u,
                       "first repeated preincrement PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u,
                       "middle repeated preincrement PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u,
                       "last repeated preincrement PSV access uses five cycles");
    expect(state, cpu->w[1] == 0xc004u && cpu->w[2] == 0x3333u,
           "preincrement PSV remains outside optimized schedule");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_REPEAT_2);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    dspic33_set_working_register(cpu, 1u, 0xc000u);
    cpu->dsrpag = 0x0200u;
    expect_step_cycles(state, cpu, 1u, "MOV.D PSV REPEAT setup");
    expect_step_cycles(state, cpu, 5u,
                       "first repeated MOV.D PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u,
                       "middle repeated MOV.D PSV access uses five cycles");
    expect_step_cycles(state, cpu, 5u,
                       "last repeated MOV.D PSV access uses five cycles");
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
    expect_step_cycles(state, cpu, 5u,
                       "first repeated DSP PSV prefetch uses five cycles");
    expect_step_cycles(state, cpu, 1u,
                       "middle repeated DSP PSV prefetch uses one cycle");
    expect_step_cycles(state, cpu, 6u,
                       "last repeated DSP PSV prefetch uses six cycles");
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
    expect_step_cycles(state, cpu, 5u,
                       "pre-interrupt repeated PSV access exits in five cycles");
    expect(state,
           cpu->disicnt == 0u && cpu->rcount == 2u && cpu->repeat_active != 0u &&
               cpu->w[1] == 0xc004u,
           "pre-interrupt PSV iteration preserves suspended repeat state");
    expect_step_cycles(state, cpu, 9u,
                       "interrupt dispatch executes handler instruction");
    expect(state,
           cpu->pc == 0x0302u && cpu->repeat_active == 0u && cpu->rcount == 2u &&
               (dspic33_read_word(cpu, 0x5002u) & 0x1000u) != 0u,
           "interrupt entry stacks optimized repeat state");
    dspic33_write_word(cpu, 0x0800u, 0u);
    expect_step_cycles(state, cpu, 6u, "RETFIE restores optimized repeat state");
    expect(state,
           cpu->repeat_active != 0u && cpu->repeat_psv_reentry && cpu->pc == 0x202u,
           "RETFIE arms PSV repeat re-entry timing");
    expect_step_cycles(state, cpu, 5u, "re-entered PSV repeat access uses five cycles");
    expect_step_cycles(state, cpu, 1u,
                       "resumed middle PSV repeat access uses one cycle");
    expect_step_cycles(state, cpu, 6u,
                       "resumed final PSV repeat access uses six cycles");
    expect(state,
           cpu->w[1] == 0xc00au && cpu->w[2] == 0x5555u && cpu->repeat_active == 0u &&
               !cpu->repeat_psv_started && !cpu->repeat_psv_reentry,
           "interrupted PSV repeat completes all iterations");

    cpu->repeat_psv_started = true;
    cpu->repeat_psv_reentry = true;
    cpu->psv_repeat_optimized = true;
    dspic33_reset(cpu, 0u);
    expect(state,
           !cpu->repeat_psv_started && !cpu->repeat_psv_reentry &&
               !cpu->psv_repeat_optimized,
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
           cpu->w[1] == 0xd804u && cpu->w[2] == 0u && cpu->w[3] == 0u &&
               cpu->dsrpag == 0x020au && cpu->cycles == 5u && cpu->trap_count == 1u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[1] == 0xd800u &&
               cpu->w[2] == 0x1357u && cpu->dsrpag == 0x020au && !cpu->address_error &&
               cpu->cycles == 5u,
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
    expect_step_cycles(state, cpu, 2u,
                       "same-value direct write still creates dependency");
    expect(state, cpu->w[3] == 0x1234u, "same-value dependency preserves source value");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_LITERAL_0X1000_W2);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_MOV_W2_INDIRECT_W3);
    dspic33_write_word(cpu, 0x1000u, 0x2345u);
    expect_step_cycles(state, cpu, 1u,
                       "intervening-control pointer setup uses one cycle");
    expect_step_cycles(state, cpu, 1u,
                       "intervening instruction consumes dependency window");
    expect_step_cycles(state, cpu, 1u,
                       "source after intervening instruction does not stall");
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
    expect_step_cycles(state, cpu, 2u,
                       "destination postincrement creates pointer dependency");
    expect(state, cpu->w[2] == 0x1002u && cpu->w[4] == 0x2222u,
           "destination dependency uses updated pointer");

    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_W2_POST_INCREMENT_W3);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W2_INDIRECT_W4);
    dspic33_set_working_register(cpu, 2u, 0x1000u);
    dspic33_write_word(cpu, 0x1000u, 0x3333u);
    dspic33_write_word(cpu, 0x1002u, 0x4444u);
    expect_step_cycles(state, cpu, 1u, "source postincrement uses one cycle");
    expect_step_cycles(state, cpu, 2u,
                       "source postincrement creates pointer dependency");
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
    expect_step_cycles(state, cpu, 14u,
                       "interrupt dispatch flushes pending dependency");
    dspic33_write_word(cpu, 0x0800u, 0u);
    expect_step_cycles(state, cpu, 1u,
                       "source after interrupt does not retain prior dependency");
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

static void expect_dsp_x_page_transition(TestState* state, Dspic33* cpu,
                                         uint32_t opcode, uint16_t pointer,
                                         uint16_t page, uint32_t program_address,
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
               cpu->w[4] == value && cpu->w[5] == 0x6789u &&
               cpu->w[8] == expected_pointer && cpu->w[10] == 0x9000u &&
               cpu->dsrpag == expected_page && cpu->cycles == 5u,
           name);
}

static void dsp_x_prefetch_page_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_DSP_X_W8_INCREMENT_Y_W10_DECREMENT);
    expect(state, dspic33_load_program_word(cpu, 0x0100u, 0x001234u),
           "load DSP X PSV value");
    expect(state, dspic33_load_program_word(cpu, 0x1002u, 0x009abcu),
           "load DSP Y isolation value");
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
    expect(state, dspic33_load_program_word(cpu, 0x7ffeu, 0x003456u),
           "load DSP X crossing value");
    dspic33_set_working_register(cpu, 4u, 3u);
    dspic33_set_working_register(cpu, 5u, 4u);
    dspic33_set_working_register(cpu, 8u, 0xfffeu);
    dspic33_set_working_register(cpu, 10u, 0x9002u);
    dspic33_write_word(cpu, 0x9002u, 0x6789u);
    cpu->corcon = 0x0021u;
    cpu->dsrpag = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0x3456u &&
               cpu->w[5] == 0x6789u && cpu->w[8] == 0x8000u && cpu->w[10] == 0x9000u &&
               cpu->dsrpag == 0x0201u && cpu->cycles == 5u,
           "DSP X post-increment reads the original PSV page before transition");

    expect_dsp_x_page_transition(
        state, cpu, OPCODE_DSP_X_W8_INCREMENT_4_Y_W10_DECREMENT, 0xfffcu, 0x0200u,
        0x7ffcu, 0x4567u, 0x8000u, 0x0201u,
        "DSP X four-byte post-increment crosses into the next PSV page");
    expect_dsp_x_page_transition(
        state, cpu, OPCODE_DSP_X_W8_INCREMENT_6_Y_W10_DECREMENT, 0xfffau, 0x0200u,
        0x7ffau, 0x5678u, 0x8000u, 0x0201u,
        "DSP X six-byte post-increment crosses into the next PSV page");
    expect_dsp_x_page_transition(
        state, cpu, OPCODE_DSP_X_W8_DECREMENT_4_Y_W10_DECREMENT, 0x8000u, 0x0201u,
        0x8000u, 0x6789u, 0xfffcu, 0x0200u,
        "DSP X four-byte post-decrement crosses into the prior PSV page");
    expect_dsp_x_page_transition(
        state, cpu, OPCODE_DSP_X_W8_DECREMENT_6_Y_W10_DECREMENT, 0x8000u, 0x0201u,
        0x8000u, 0x789au, 0xfffau, 0x0200u,
        "DSP X six-byte post-decrement crosses into the prior PSV page");
    expect_dsp_x_page_transition(
        state, cpu, OPCODE_DSP_X_W8_DECREMENT_Y_W10_DECREMENT, 0x8000u, 0x0201u,
        0x8000u, 0x89abu, 0xfffeu, 0x0200u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0x2468u &&
               cpu->w[5] == 0x789au && cpu->w[8] == 0x8102u && cpu->dsrpag == 1u &&
               cpu->cycles == 1u,
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
               cpu->w[11] == 0x9002u && cpu->w[12] == 0xfffeu &&
               cpu->dsrpag == 0x0200u && cpu->cycles == 5u,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->w[4] == 0xbcdeu &&
               cpu->w[5] == 0x9abcu && cpu->w[8] == 0xfff8u && cpu->dsrpag == 0x0200u &&
               cpu->cycles == 5u,
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
               dspic33_read_word(cpu, 0x5000u) == 2u &&
               dspic33_read_word(cpu, 0x5002u) == 0u && cpu->sr == 0x00c0u &&
               cpu->corcon == 0x0029u,
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
           cpu->accumulator[0] == 0 && cpu->w[4] == 0u && cpu->w[8] == 0x9006u &&
               cpu->w[13] == 0u,
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
           cpu->accumulator[0] == 0x123456 && cpu->accumulator[1] == 0x654321 &&
               cpu->w[4] == 0u && cpu->w[5] == 0u && cpu->w[9] == 0x8ffeu &&
               cpu->w[13] == 0x5102u && dspic33_read_word(cpu, 0x5100u) == 0xa55au,
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
               cpu->w[8] == 0xd800u && cpu->w[10] == 0x9000u &&
               cpu->dsrpag == 0x020au && cpu->cycles == 5u && cpu->w[15] == 0x5004u,
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
               cpu->w[8] == 0xd800u && cpu->w[10] == 0x9000u &&
               cpu->dsrpag == 0x020au && cpu->cycles == 5u,
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
           cpu->accumulator[0] == 0 && cpu->accumulator[1] == 0x654321 &&
               cpu->w[4] == 0u && cpu->w[8] == 0xd806u && cpu->w[13] == 0x0065u &&
               cpu->cycles == 5u,
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
           cpu->accumulator[0] == 0x123456 && cpu->accumulator[1] == 0x654321 &&
               cpu->w[4] == 0u && cpu->w[5] == 0x2468u && cpu->w[9] == 0xd7feu &&
               cpu->w[13] == 0x5102u && dspic33_read_word(cpu, 0x5100u) == 0xa55au &&
               cpu->cycles == 5u,
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
           (dspic33_read_word(cpu, 0x08c0u) & 0x0004u) == 0u &&
               active_pending_traps(cpu) == 0u,
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
    expect(state,
           cpu->w[2] == 0x0123u && cpu->w[1] == 0u &&
               cpu->last_trap_return == 0x000300u,
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
           cpu->pc == 0x000302u && cpu->splim == 0x5100u && pending != NULL &&
               pending->delay == 1u,
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
        {OPCODE_MOV_DOUBLE_INVALID_SOURCE_MODE_6,
         "MOV.D source mode 6 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_SOURCE_MODE_7,
         "MOV.D source mode 7 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_DESTINATION_MODE_6,
         "MOV.D destination mode 6 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_DESTINATION_MODE_7,
         "MOV.D destination mode 7 resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_MEMORY_PAIR, "MOV.D memory pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_ODD_SOURCE_PAIR,
         "MOV.D odd source register pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_ODD_DESTINATION_PAIR,
         "MOV.D odd destination register pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_REVERSE_ODD_SOURCE_PAIR,
         "MOV.D reverse odd source register pair resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_REVERSE_DIRECT,
         "MOV.D reverse direct destination resets processor"},
        {OPCODE_MOV_DOUBLE_INVALID_DIRECTION_BIT,
         "MOV.D reserved direction bit resets processor"},
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
               dspic33_read_word(cpu, 0x5002u) == 0x2222u &&
               cpu->illegal_reset_count == 0u,
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
            cases[index].result < INT32_MIN || cases[index].result > INT32_MAX
                ? 0x880fu
                : 0x000fu;
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
                   cpu->accumulator[0] == cases[index].result &&
                   cpu->accumulator[1] == 0,
               cases[index].execution);
        expect(state,
               cpu->corcon == 0x2001u && cpu->sr == expected_status &&
                   cpu->w[4] == 0xfffeu && cpu->w[5] == 0xfffdu &&
                   cpu->w[6] == 0x8002u && cpu->w[7] == 0x8003u &&
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
                   cpu->accumulator[0] == 12 &&
                   cpu->w[cases[index].destination] == 0x2222u,
               cases[index].execution);
        expect(
            state,
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
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 1u &&
               cpu->w[4] == 0x2222u && cpu->w[8] == 0x5002u && cpu->w[10] == 0x9004u,
           "MOVSAC prefetch collision resolves W4 after both lanes complete");
    expect(state,
           cpu->accumulator[0] == 0x123456789a &&
               cpu->accumulator[1] == -0x123456789a && cpu->sr == 0x010fu &&
               cpu->corcon == 0x0001u && cpu->illegal_reset_count == 0u,
           "MOVSAC prefetch collision preserves accumulators and control state");
}

typedef struct {
    uint8_t register_offset;
    int8_t access_offset;
    int8_t update;
    bool present;
} DspMatrixPrefetch;

enum {
    DSP_MATRIX_WRITE_BACK_DIRECT = 0u,
    DSP_MATRIX_WRITE_BACK_INDIRECT = 1u,
    DSP_MATRIX_WRITE_BACK_NONE = 2u
};

static const DspMatrixPrefetch dsp_matrix_prefetches[16] = {
    {0u, 0, 0, true},  {0u, 0, 2, true},  {0u, 0, 4, true},  {0u, 0, 6, true},
    {0u, 0, 0, false}, {0u, 0, -6, true}, {0u, 0, -4, true}, {0u, 0, -2, true},
    {1u, 0, 0, true},  {1u, 0, 2, true},  {1u, 0, 4, true},  {1u, 0, 6, true},
    {1u, 2, 0, true},  {1u, 0, -6, true}, {1u, 0, -4, true}, {1u, 0, -2, true},
};

static uint16_t dsp_matrix_base_value(uint8_t reg) {
    static const uint16_t values[4] = {0x5008u, 0x5108u, 0x9008u, 0x9108u};
    return values[reg - 8u];
}

static uint16_t dsp_matrix_prefetch_value(bool y_space, uint8_t operation) {
    return (uint16_t)((y_space ? 0x2200u : 0x1100u) | operation);
}

static void prepare_dsp_matrix_case(Dspic33* cpu, uint8_t target_accumulator,
                                    uint8_t x_operation, uint8_t y_operation,
                                    uint16_t expected_w[14]) {
    static const uint16_t values[10] = {
        2u, 3u, 5u, 7u, 0x5008u, 0x5108u, 0x9008u, 0x9108u, 2u, 0x5200u,
    };
    const DspMatrixPrefetch* x = &dsp_matrix_prefetches[x_operation];
    const DspMatrixPrefetch* y = &dsp_matrix_prefetches[y_operation];
    uint8_t reg;

    cpu->pc = 0u;
    cpu->sr = 0x000fu;
    cpu->corcon = 0x0001u;
    cpu->accumulator[target_accumulator] = 100;
    cpu->accumulator[target_accumulator ^ 1u] = 0x12348001;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->events.count = 0u;
    for (reg = 4u; reg <= 13u; reg++) {
        dspic33_set_working_register(cpu, reg, values[reg - 4u]);
        expected_w[reg] = values[reg - 4u];
    }
    dspic33_write_word(cpu, 0x5200u, 0xa5a5u);
    if (x->present) {
        uint8_t base = (uint8_t)(8u + x->register_offset);
        dspic33_write_word(cpu,
                           (uint16_t)(dsp_matrix_base_value(base) + x->access_offset),
                           dsp_matrix_prefetch_value(false, x_operation));
    }
    if (y->present) {
        uint8_t base = (uint8_t)(10u + y->register_offset);
        dspic33_write_word(cpu,
                           (uint16_t)(dsp_matrix_base_value(base) + y->access_offset),
                           dsp_matrix_prefetch_value(true, y_operation));
    }
}

static void apply_dsp_matrix_prefetch(uint16_t expected_w[14], uint8_t operation,
                                      uint8_t destination, bool y_space,
                                      bool write_destination) {
    const DspMatrixPrefetch* prefetch = &dsp_matrix_prefetches[operation];
    uint8_t base;
    if (!prefetch->present) {
        return;
    }
    base = (uint8_t)((y_space ? 10u : 8u) + prefetch->register_offset);
    expected_w[base] = (uint16_t)(dsp_matrix_base_value(base) + prefetch->update);
    if (write_destination) {
        expected_w[destination] = dsp_matrix_prefetch_value(y_space, operation);
    }
}

static void expect_dsp_matrix_case(TestState* state, bool condition, uint32_t opcode,
                                   const char* domain) {
    state->cases++;
    if (condition) {
        state->passed++;
        return;
    }
    state->failed++;
    printf("[processor-failed] %s opcode=%06" PRIx32 "\n", domain, opcode);
}

static void repeat_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t count;
    uint8_t reg;

    for (count = 0u; count <= 0x7fffu; count++) {
        bool matches;
        uint32_t opcode = 0x090000u | count;
        reset_processor_test(cpu, 0x200u);
        matches = dspic33_load_program_word(cpu, 0x200u, opcode) &&
                  dspic33_load_program_word(cpu, 0x202u, OPCODE_NOP) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u &&
                  cpu->cycles == 1u && cpu->rcount == count &&
                  cpu->repeat_active == (count != 0u) &&
                  (cpu->sr & 0x0010u) == (count != 0u ? 0x0010u : 0u) &&
                  cpu->repeat_pc == (count != 0u ? 0x202u : 0u) &&
                  cpu->unsupported_opcode == 0u;
        expect_dsp_matrix_case(state, matches, opcode, "REPEAT literal encoding");
    }

    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value;
        uint32_t opcode = 0x098000u | reg;
        bool matches;
        reset_processor_test(cpu, 0x200u);
        dspic33_set_working_register(cpu, reg,
                                     reg == 0u ? 0u : (uint16_t)(0x1111u * reg));
        value = cpu->w[reg];
        matches = dspic33_load_program_word(cpu, 0x200u, opcode) &&
                  dspic33_load_program_word(cpu, 0x202u, OPCODE_NOP) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x202u &&
                  cpu->cycles == 1u && cpu->rcount == value &&
                  cpu->repeat_active == (value != 0u) &&
                  (cpu->sr & 0x0010u) == (value != 0u ? 0x0010u : 0u) &&
                  cpu->repeat_pc == (value != 0u ? 0x202u : 0u) &&
                  cpu->unsupported_opcode == 0u;
        expect_dsp_matrix_case(state, matches, opcode, "REPEAT register encoding");
    }

    reset_processor_test(cpu, 0x200u);
    expect_dsp_matrix_case(state,
                           dspic33_load_program_word(cpu, 0x200u, 0x098010u) &&
                               dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                               cpu->unsupported_opcode == 0x098010u &&
                               cpu->pc == 0x200u,
                           0x098010u, "REPEAT reserved register encoding");
}

static void run_do_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                 uint16_t count, uint32_t extension,
                                 uint32_t expected_end, const char* domain) {
    bool matches;
    reset_processor_test(cpu, 0x20000u);
    matches = dspic33_load_program_word(cpu, 0x20000u, opcode) &&
              dspic33_load_program_word(cpu, 0x20002u, extension) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20004u &&
              cpu->cycles == 2u && cpu->do_depth == 1u && cpu->do_count[0] == count &&
              cpu->dcount == count && cpu->do_start[0] == 0x20004u &&
              cpu->dostart == 0x20004u && cpu->do_end[0] == expected_end &&
              cpu->doend == expected_end && cpu->corcon == 0x0120u &&
              (cpu->sr & 0x0200u) != 0u && cpu->unsupported_opcode == 0u;
    expect_dsp_matrix_case(state, matches, opcode, domain);
}

static void do_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t count;
    uint32_t extension;
    uint8_t reg;

    for (count = 0u; count <= 0x7fffu; count++) {
        run_do_encoding_case(state, cpu, 0x080000u | count, (uint16_t)count, 2u,
                             0x20008u, "DO literal encoding");
    }

    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value;
        uint32_t opcode = 0x088000u | reg;
        reset_processor_test(cpu, 0x20000u);
        dspic33_set_working_register(cpu, reg,
                                     reg == 0u ? 0u : (uint16_t)(0x1111u * reg));
        value = cpu->w[reg];
        expect_dsp_matrix_case(
            state,
            dspic33_load_program_word(cpu, 0x20000u, opcode) &&
                dspic33_load_program_word(cpu, 0x20002u, 2u) &&
                dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20004u &&
                cpu->cycles == 2u && cpu->do_depth == 1u && cpu->do_count[0] == value &&
                cpu->dcount == value && cpu->do_start[0] == 0x20004u &&
                cpu->do_end[0] == 0x20008u && cpu->dostart == 0x20004u &&
                cpu->doend == 0x20008u && cpu->corcon == 0x0120u &&
                (cpu->sr & 0x0200u) != 0u && cpu->unsupported_opcode == 0u,
            opcode, "DO register encoding");
    }

    for (extension = 0u; extension <= 0xffffu; extension++) {
        uint32_t expected_end;
        if (extension == 0u || extension == 1u || extension == 0xffffu) {
            continue;
        }
        expected_end = (uint32_t)(0x20004 + (int32_t)(int16_t)extension * 2);
        run_do_encoding_case(state, cpu, OPCODE_DO_0, 0u, extension, expected_end,
                             "DO loop-length encoding");
    }

    for (extension = 1u; extension <= 0xffu; extension++) {
        uint32_t invalid_extension = extension << 16u | 2u;
        reset_processor_test(cpu, 0x20000u);
        expect_dsp_matrix_case(
            state,
            dspic33_load_program_word(cpu, 0x20000u, OPCODE_DO_0) &&
                dspic33_load_program_word(cpu, 0x20002u, invalid_extension) &&
                dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                cpu->unsupported_opcode == OPCODE_DO_0 && cpu->pc == 0x20000u &&
                cpu->cycles == 0u && cpu->do_depth == 0u,
            invalid_extension, "DO literal reserved extension");

        reset_processor_test(cpu, 0x20000u);
        dspic33_set_working_register(cpu, 0u, 0xaaaau);
        expect_dsp_matrix_case(
            state,
            dspic33_load_program_word(cpu, 0x20000u, OPCODE_DO_W0) &&
                dspic33_load_program_word(cpu, 0x20002u, invalid_extension) &&
                dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                cpu->unsupported_opcode == OPCODE_DO_W0 && cpu->pc == 0x20000u &&
                cpu->cycles == 0u && cpu->do_depth == 0u,
            invalid_extension, "DO register reserved extension");
    }

    reset_processor_test(cpu, 0x20000u);
    expect_dsp_matrix_case(state,
                           dspic33_load_program_word(cpu, 0x20000u, 0x088010u) &&
                               dspic33_step(cpu) == DSPIC33_UNSUPPORTED_INSTRUCTION &&
                               cpu->unsupported_opcode == 0x088010u &&
                               cpu->pc == 0x20000u,
                           0x088010u, "DO reserved register encoding");
}

static void load_three_instruction_do(TestState* state, Dspic33* cpu, uint16_t count) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x080000u | count);
    load_instruction(state, cpu, 0x202u, 2u);
    load_instruction(state, cpu, 0x204u, OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x206u, OPCODE_NOP);
    load_instruction(state, cpu, 0x208u, OPCODE_NOP);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->do_depth == 1u,
           "DO control case starts loop");
}

static void do_register_control_cases(TestState* state, Dspic33* cpu) {
    uint32_t start;

    reset_processor_test(cpu, 0x200u);
    dspic33_write_word(cpu, 0x003au, 0xffffu);
    dspic33_write_word(cpu, 0x003cu, 0xffffu);
    dspic33_write_byte(cpu, 0x003au, 0xa5u);
    dspic33_write_byte(cpu, 0x003du, 0x3fu);
    expect(state,
           cpu->dostart == 0u && dspic33_read_word(cpu, 0x003au) == 0u &&
               dspic33_read_word(cpu, 0x003cu) == 0u,
           "DOSTART ignores word and byte writes while inactive");

    load_three_instruction_do(state, cpu, 2u);
    start = cpu->dostart;
    dspic33_write_word(cpu, 0x003au, 0xffffu);
    dspic33_write_word(cpu, 0x003cu, 0xffffu);
    expect(state,
           cpu->dostart == start && cpu->do_start[0] == start &&
               dspic33_read_word(cpu, 0x003au) == (uint16_t)start &&
               dspic33_read_word(cpu, 0x003cu) == (uint16_t)(start >> 16u),
           "DOSTART ignores writes while a loop is active");

    dspic33_write_word(cpu, 0x0038u, 1u);
    expect(state, cpu->dcount == 1u && cpu->do_count[0] == 1u,
           "DCOUNT word write updates the active loop counter");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->do_depth == 0u &&
               cpu->w[2] == 2u,
           "active DCOUNT write changes remaining iteration count");

    load_three_instruction_do(state, cpu, 2u);
    dspic33_write_byte(cpu, 0x0038u, 0x34u);
    dspic33_write_byte(cpu, 0x0039u, 0x12u);
    expect(state, cpu->dcount == 0x1234u && cpu->do_count[0] == 0x1234u,
           "DCOUNT byte writes update the active loop counter");

    load_three_instruction_do(state, cpu, 1u);
    dspic33_write_word(cpu, 0x003eu, 0x0206u);
    dspic33_write_word(cpu, 0x0040u, 0u);
    expect(state, cpu->doend == 0x0206u && cpu->do_end[0] == 0x0206u,
           "DOEND word write updates the active loop boundary");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->do_depth == 0u &&
               cpu->w[2] == 2u,
           "active DOEND write changes the loop completion boundary");

    load_three_instruction_do(state, cpu, 1u);
    dspic33_write_byte(cpu, 0x003eu, 0x0au);
    dspic33_write_byte(cpu, 0x003fu, 0x02u);
    dspic33_write_byte(cpu, 0x0040u, 0x01u);
    expect(state, cpu->doend == 0x01020au && cpu->do_end[0] == 0x01020au,
           "DOEND byte writes update every implemented address field");
}

static void run_do_to_completion(TestState* state, Dspic33* cpu,
                                 uint32_t expected_increments, const char* name) {
    uint32_t steps = 0u;
    while (cpu->do_depth != 0u && steps < 16u) {
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            break;
        }
        steps++;
    }
    expect(state, cpu->do_depth == 0u && cpu->w[2] == expected_increments, name);
}

static void do_early_termination_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;

    load_three_instruction_do(state, cpu, 7u);
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    expect(state, cpu->do_terminate[0] == 1u && (cpu->corcon & 0x0800u) == 0u,
           "EDT before the last two instructions requests current-iteration exit");
    run_do_to_completion(state, cpu, 1u, "early EDT exits after the current iteration");

    load_three_instruction_do(state, cpu, 7u);
    cpu->instruction_active = true;
    cpu->current_instruction_pc = 0x206u;
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    cpu->instruction_active = false;
    expect(state, cpu->do_terminate[0] == 2u && (cpu->corcon & 0x0800u) == 0u,
           "EDT in the penultimate instruction defers termination one iteration");
    run_do_to_completion(state, cpu, 2u,
                         "penultimate EDT permits exactly one additional iteration");

    load_three_instruction_do(state, cpu, 7u);
    cpu->instruction_active = true;
    cpu->current_instruction_pc = 0x208u;
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    cpu->instruction_active = false;
    expect(state, cpu->do_terminate[0] == 2u,
           "EDT in the final instruction defers termination one iteration");
    run_do_to_completion(
        state, cpu, 2u,
        "final-instruction EDT permits exactly one additional iteration");

    load_three_instruction_do(state, cpu, 7u);
    cpu->instruction_active = true;
    cpu->current_instruction_pc = 0x206u;
    dspic33_write_word(cpu, 0x0044u, 0x0800u);
    cpu->instruction_active = false;
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize active DO copy destination");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy active late-EDT loop state");
        cpu->do_terminate[0] = 1u;
        run_do_to_completion(state, &copy, 2u,
                             "copied late-EDT state completes independently");
        expect(state, cpu->do_terminate[0] == 1u && cpu->do_depth == 1u,
               "active DO source remains independent from its copy");
        dspic33_destroy(&copy);
    }
}

static void do_stack_overflow_cases(TestState* state, Dspic33* cpu) {
    uint32_t starts[4] = {0x300u, 0x320u, 0x340u, 0x360u};
    uint32_t ends[4] = {0x308u, 0x328u, 0x348u, 0x368u};
    uint16_t counts[4] = {1u, 2u, 3u, 4u};

    reset_processor_test(cpu, 0x200u);
    prepare_trap_vectors(state, cpu);
    load_instruction(state, cpu, 0x0010u, 0x000340u);
    load_instruction(state, cpu, 0x0340u, OPCODE_NOP);
    load_instruction(state, cpu, 0x200u, OPCODE_DO_0);
    load_instruction(state, cpu, 0x202u, 2u);
    memcpy(cpu->do_start, starts, sizeof(starts));
    memcpy(cpu->do_end, ends, sizeof(ends));
    memcpy(cpu->do_count, counts, sizeof(counts));
    cpu->do_depth = 4u;
    cpu->dostart = starts[3];
    cpu->doend = ends[3];
    cpu->dcount = counts[3];
    cpu->corcon = 0x0420u;
    cpu->sr |= 0x0200u;
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 6u &&
               cpu->pc == 0x000340u,
           "fifth nested DO enters the generic stack-overflow trap");
    expect(state,
           cpu->do_depth == 4u && memcmp(cpu->do_start, starts, sizeof(starts)) == 0 &&
               memcmp(cpu->do_end, ends, sizeof(ends)) == 0 &&
               memcmp(cpu->do_count, counts, sizeof(counts)) == 0 &&
               cpu->dostart == starts[3] && cpu->doend == ends[3] &&
               cpu->dcount == counts[3] && cpu->corcon == 0x0428u,
           "fifth nested DO leaves every active loop unchanged");
    expect(state,
           (dspic33_read_word(cpu, 0x08c4u) & 0x0010u) != 0u &&
               active_pending_traps(cpu) == 1u && cpu->do_depth == 4u,
           "fifth nested DO sets DOOVR and retains its level-sensitive source");
}

static void prepare_nested_zero_do_case(TestState* state, Dspic33* cpu,
                                        uint16_t inner_count, bool first_outer_nop,
                                        bool second_outer_nop, bool inner_nop) {
    reset_processor_test(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, 0x080001u);
    load_instruction(state, cpu, 0x202u, 6u);
    load_instruction(state, cpu, 0x204u,
                     first_outer_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x206u,
                     second_outer_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x208u, 0x080000u | inner_count);
    load_instruction(state, cpu, 0x20au, 1u);
    load_instruction(state, cpu, 0x20cu, inner_nop ? OPCODE_NOP : OPCODE_INCREMENT_W2);
    load_instruction(state, cpu, 0x20eu, OPCODE_NOP);
    load_instruction(state, cpu, 0x210u, OPCODE_NOP);
    dspic33_step(cpu);
    dspic33_step(cpu);
    dspic33_step(cpu);
}

static void nested_zero_do_erratum_cases(TestState* state, Dspic33* cpu) {
    prepare_nested_zero_do_case(state, cpu, 0u, true, true, true);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu &&
               cpu->do_depth == 2u && cpu->do_count[1] == 0u,
           "nested zero-count DO accepts the documented NOP workaround");

    prepare_nested_zero_do_case(state, cpu, 0u, true, true, true);
    load_instruction(state, cpu, 0x208u, OPCODE_DO_W0);
    dspic33_set_working_register(cpu, 0u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu &&
               cpu->do_depth == 2u && cpu->do_count[1] == 0u,
           "nested zero-count register DO accepts the documented NOP workaround");

    prepare_nested_zero_do_case(state, cpu, 1u, false, false, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x20cu &&
               cpu->do_depth == 2u && cpu->do_count[1] == 1u,
           "nested nonzero DO does not require the zero-count workaround");

    prepare_nested_zero_do_case(state, cpu, 0u, false, true, true);
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "nested zero-count DO rejects a missing first outer NOP");

    prepare_nested_zero_do_case(state, cpu, 0u, true, false, true);
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "nested zero-count DO rejects a missing second outer NOP");

    prepare_nested_zero_do_case(state, cpu, 0u, true, true, false);
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "nested zero-count DO rejects a missing inner NOP");
}

static void loop_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    repeat_encoding_matrix_cases(state, cpu);
    do_encoding_matrix_cases(state, cpu);
    do_register_control_cases(state, cpu);
    do_early_termination_cases(state, cpu);
    do_stack_overflow_cases(state, cpu);
    nested_zero_do_erratum_cases(state, cpu);
}

typedef enum {
    ARITHMETIC_MATRIX_SUBR,
    ARITHMETIC_MATRIX_SUBBR,
    ARITHMETIC_MATRIX_ADD,
    ARITHMETIC_MATRIX_ADDC,
    ARITHMETIC_MATRIX_SUB,
    ARITHMETIC_MATRIX_SUBB,
    ARITHMETIC_MATRIX_AND,
    ARITHMETIC_MATRIX_XOR,
    ARITHMETIC_MATRIX_IOR
} BinaryMatrixOperation;

typedef enum {
    DIRECT_FILE_SUBR,
    DIRECT_FILE_SUBBR,
    DIRECT_FILE_ADD,
    DIRECT_FILE_ADDC,
    DIRECT_FILE_SUB,
    DIRECT_FILE_SUBB,
    DIRECT_FILE_AND,
    DIRECT_FILE_XOR,
    DIRECT_FILE_IOR,
    DIRECT_FILE_INC,
    DIRECT_FILE_INC2,
    DIRECT_FILE_DEC,
    DIRECT_FILE_DEC2,
    DIRECT_FILE_NEG,
    DIRECT_FILE_COM,
    DIRECT_FILE_CLR,
    DIRECT_FILE_SETM,
    DIRECT_FILE_SL,
    DIRECT_FILE_LSR,
    DIRECT_FILE_ASR,
    DIRECT_FILE_RLNC,
    DIRECT_FILE_RLC,
    DIRECT_FILE_RRNC,
    DIRECT_FILE_RRC,
    DIRECT_FILE_CP,
    DIRECT_FILE_CPB,
    DIRECT_FILE_CP0
} DirectFileOperation;

typedef struct {
    uint16_t address;
    bool direct;
} BinaryMatrixOperand;

static bool binary_matrix_with_carry(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_SUBBR ||
           operation == ARITHMETIC_MATRIX_ADDC || operation == ARITHMETIC_MATRIX_SUBB;
}

static bool binary_matrix_reverse(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_SUBR || operation == ARITHMETIC_MATRIX_SUBBR;
}

static bool binary_matrix_addition(BinaryMatrixOperation operation) {
    return operation == ARITHMETIC_MATRIX_ADD || operation == ARITHMETIC_MATRIX_ADDC;
}

static bool binary_matrix_logical(BinaryMatrixOperation operation) {
    return operation >= ARITHMETIC_MATRIX_AND;
}

static uint16_t binary_matrix_result(BinaryMatrixOperation operation, uint16_t left,
                                     uint16_t right, uint16_t initial_status,
                                     bool byte_mode) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t carry = (initial_status & 0x0001u) != 0u ? 1u : 0u;
    uint16_t borrow = binary_matrix_with_carry(operation) && carry == 0u ? 1u : 0u;
    uint32_t result;

    left = (uint16_t)(left & mask);
    right = (uint16_t)(right & mask);
    if (operation == ARITHMETIC_MATRIX_AND) {
        result = left & right;
    } else if (operation == ARITHMETIC_MATRIX_XOR) {
        result = left ^ right;
    } else if (operation == ARITHMETIC_MATRIX_IOR) {
        result = left | right;
    } else if (binary_matrix_addition(operation)) {
        result =
            (uint32_t)left + right + (binary_matrix_with_carry(operation) ? carry : 0u);
    } else if (binary_matrix_reverse(operation)) {
        result = (uint16_t)(right - left - borrow);
    } else {
        result = (uint16_t)(left - right - borrow);
    }
    return (uint16_t)(result & mask);
}

static uint16_t binary_matrix_status(BinaryMatrixOperation operation, uint16_t left,
                                     uint16_t right, uint16_t initial_status,
                                     bool byte_mode) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t digit_mask = byte_mode ? 0x000fu : 0x00ffu;
    uint16_t carry = (initial_status & 0x0001u) != 0u ? 1u : 0u;
    uint16_t borrow = binary_matrix_with_carry(operation) && carry == 0u ? 1u : 0u;
    uint16_t add_carry =
        binary_matrix_addition(operation) && binary_matrix_with_carry(operation) ? carry
                                                                                 : 0u;
    bool sticky_zero = binary_matrix_with_carry(operation);
    uint16_t status = (uint16_t)(initial_status & ~0x010fu);
    uint16_t value;

    left = (uint16_t)(left & mask);
    right = (uint16_t)(right & mask);
    value = binary_matrix_result(operation, left, right, initial_status, byte_mode);
    if (binary_matrix_logical(operation)) {
        status = (uint16_t)(initial_status & ~0x000au);
        if (value == 0u) {
            status |= 0x0002u;
        }
        if ((value & sign) != 0u) {
            status |= 0x0008u;
        }
        return status;
    }
    if (value == 0u && (!sticky_zero || (initial_status & 0x0002u) != 0u)) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    if (binary_matrix_addition(operation)) {
        uint32_t result = (uint32_t)left + right + add_carry;
        int32_t signed_left = byte_mode ? (int8_t)left : (int16_t)left;
        int32_t signed_right = byte_mode ? (int8_t)right : (int16_t)right;
        int32_t signed_result = signed_left + signed_right + add_carry;
        int32_t minimum = byte_mode ? INT8_MIN : INT16_MIN;
        int32_t maximum = byte_mode ? INT8_MAX : INT16_MAX;
        if (result > mask) {
            status |= 0x0001u;
        }
        if (((left & digit_mask) + (right & digit_mask) + add_carry) > digit_mask) {
            status |= 0x0100u;
        }
        if (signed_result < minimum || signed_result > maximum) {
            status |= 0x0004u;
        }
    } else {
        uint16_t minuend = binary_matrix_reverse(operation) ? right : left;
        uint16_t subtrahend = binary_matrix_reverse(operation) ? left : right;
        uint32_t subtraction = (uint32_t)subtrahend + borrow;
        uint16_t operand = (uint16_t)(subtraction & mask);
        if (minuend >= subtraction) {
            status |= 0x0001u;
        }
        if ((minuend & digit_mask) >= (uint32_t)(subtrahend & digit_mask) + borrow) {
            status |= 0x0100u;
        }
        if ((((minuend ^ operand) & (minuend ^ value)) & sign) != 0u) {
            status |= 0x0004u;
        }
    }
    return status;
}

static void binary_matrix_write_register(uint16_t registers[16], uint8_t reg,
                                         uint16_t value) {
    registers[reg] = reg == 15u ? (uint16_t)(value & 0xfffeu) : value;
}

static BinaryMatrixOperand binary_matrix_operand(uint16_t registers[16], uint8_t mode,
                                                 uint8_t reg, uint8_t width) {
    BinaryMatrixOperand operand = {0u, mode == 0u};
    int32_t adjusted;

    if (mode == 0u) {
        return operand;
    }
    if (mode == 1u) {
        operand.address = registers[reg];
        return operand;
    }
    if (mode == 2u || mode == 3u) {
        operand.address = registers[reg];
        adjusted =
            (int32_t)registers[reg] + (mode == 3u ? (int32_t)width : -(int32_t)width);
        binary_matrix_write_register(registers, reg, (uint16_t)adjusted);
        return operand;
    }
    adjusted =
        (int32_t)registers[reg] + (mode == 5u ? (int32_t)width : -(int32_t)width);
    operand.address = (uint16_t)adjusted;
    binary_matrix_write_register(registers, reg, (uint16_t)adjusted);
    return operand;
}

static void prepare_arithmetic_matrix_case(Dspic33* cpu, uint16_t registers[16],
                                           uint16_t initial_status) {
    uint8_t reg;

    cpu->pc = 0u;
    cpu->sr = initial_status;
    cpu->corcon = 0x0020u;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value = (uint16_t)(0x4000u + (uint16_t)reg * 0x0100u);
        dspic33_set_working_register(cpu, reg, value);
        registers[reg] = cpu->w[reg];
    }
}

static bool binary_matrix_registers_match(const Dspic33* cpu,
                                          const uint16_t registers[16]);

static uint16_t byte_extension_status(uint16_t initial_status, uint16_t value) {
    uint16_t status = (uint16_t)(initial_status & ~0x000bu);
    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & 0x8000u) != 0u) {
        status |= 0x0008u;
    } else {
        status |= 0x0001u;
    }
    return status;
}

static void run_legal_byte_extension_case(TestState* state, Dspic33* cpu,
                                          uint32_t opcode) {
    bool zero_extend = (opcode & 0x008000u) != 0u;
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint16_t registers[16];
    BinaryMatrixOperand source;
    uint8_t source_value = (uint8_t)((opcode >> 1u) ^ opcode ^ 0xa5u);
    uint16_t value;
    uint16_t initial_status = (uint16_t)(0x0104u | (opcode & 0x000bu));
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    source = binary_matrix_operand(registers, source_mode, source_register, 1u);
    if (source.direct) {
        source_value = (uint8_t)registers[source_register];
    } else {
        dspic33_write_byte(cpu, source.address, source_value);
    }
    value = zero_extend ? source_value : (uint16_t)(int16_t)(int8_t)source_value;
    binary_matrix_write_register(registers, destination, value);
    value = registers[destination];
    expected_status = byte_extension_status(initial_status, value);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!source.direct) {
        matches = matches && dspic33_read_byte(cpu, source.address) == source_value;
    }
    expect_dsp_matrix_case(state, matches, opcode, "SE and ZE legal encoding");
}

static void run_invalid_byte_extension_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->illegal_reset = false;
    cpu->last_trap = UINT16_MAX;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->splim_enabled = false;
    cpu->events.count = 0u;
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX &&
              cpu->trap_count == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u;
    expect_dsp_matrix_case(state, matches, opcode, "SE and ZE reserved encoding");
}

static void byte_extension_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;
    uint32_t legal = 0u;
    uint32_t reserved = 0u;

    for (fields = 0u; fields <= 0xffffu; fields++) {
        uint32_t opcode = 0xfb0000u | fields;
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        if ((opcode & 0x007800u) == 0u && source_mode < 6u) {
            run_legal_byte_extension_case(state, cpu, opcode);
            legal++;
        } else {
            run_invalid_byte_extension_case(state, cpu, opcode);
            reserved++;
        }
    }
    expect(state, legal == 3072u, "SE and ZE legal encoding matrix is exhaustive");
    expect(state, reserved == 62464u,
           "SE and ZE reserved encoding matrix is exhaustive");
}

static void byte_extension_value_matrix_cases(TestState* state, Dspic33* cpu) {
    uint16_t value;
    uint8_t status_bits;
    uint8_t operation;

    for (operation = 0u; operation < 2u; operation++) {
        for (value = 0u; value <= 0xffu; value++) {
            for (status_bits = 0u; status_bits < 8u; status_bits++) {
                uint32_t opcode = 0xfb0182u | ((uint32_t)operation << 15u);
                uint16_t initial_status = (uint16_t)(0x0104u | (status_bits & 0x03u) |
                                                     ((status_bits & 0x04u) << 1u));
                uint16_t expected =
                    operation != 0u ? value : (uint16_t)(int16_t)(int8_t)value;
                uint16_t expected_status =
                    byte_extension_status(initial_status, expected);
                uint64_t cycles;
                bool matches;

                prepare_arithmetic_matrix_case(cpu, cpu->w, initial_status);
                dspic33_set_working_register(cpu, 2u, value);
                cycles = cpu->cycles;
                matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->cycles - cycles == 1u && cpu->w[3] == expected &&
                          cpu->sr == expected_status && !cpu->illegal_reset &&
                          cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "SE and ZE value and status");
            }
        }
    }
    expect(state, value == 0x0100u, "SE and ZE byte value matrix is exhaustive");
}

static void byte_extension_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xfb0192u);
    dspic33_set_working_register(cpu, 2u, 0x0800u);
    dspic33_write_word(cpu, 0x0800u, 0x00a5u);
    cpu->sr = 0x010fu;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
               cpu->w[3] == 0xffa5u && cpu->w[2] == 0x0800u && cpu->sr == 0x010cu,
           "SE non-CPU SFR byte source consumes two cycles");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xfb8192u);
    cpu->w[2] = 0x5000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu,
                         "ZE uninitialized source pointer resets processor");
}

static void prepare_stack_encoding_case(Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    dspic33_set_working_register(cpu, 14u, 0x4444u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
}

static void run_invalid_stack_encoding_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    prepare_stack_encoding_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->unsupported_opcode == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u;
    expect_dsp_matrix_case(state, matches, opcode, "reserved stack encoding");
}

static void run_direct_stack_encoding_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode, uint16_t address) {
    uint16_t value = (uint16_t)(address ^ 0xa55au);
    bool matches;

    prepare_stack_encoding_case(cpu);
    dspic33_write_word(cpu, address, value);
    dspic33_write_word(cpu, 0x4ffeu, 0x1357u);
    dspic33_write_word(cpu, 0x5000u, 0x5aa5u);
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "direct PUSH and POP encoding");
}

static void direct_stack_value_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t addresses[] = {0x1000u, 0x2000u, 0xdffeu};
    size_t index;

    for (index = 0u; index < sizeof(addresses) / sizeof(addresses[0]); index++) {
        uint16_t address = addresses[index];
        uint16_t value = (uint16_t)(0xa55au ^ address);
        uint64_t cycles;

        prepare_stack_encoding_case(cpu);
        dspic33_write_word(cpu, address, value);
        cycles = cpu->cycles;
        expect(state,
               dspic33_load_program_word(cpu, 0u, 0xf80000u | address) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                   cpu->cycles - cycles == 1u && cpu->w[15] == 0x5002u &&
                   dspic33_read_word(cpu, 0x5000u) == value &&
                   dspic33_read_word(cpu, address) == value && !cpu->illegal_reset,
               "direct PUSH covers the full implemented file address range");

        prepare_stack_encoding_case(cpu);
        dspic33_write_word(cpu, 0x4ffeu, value);
        cycles = cpu->cycles;
        expect(state,
               dspic33_load_program_word(cpu, 0u, 0xf90000u | address) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                   cpu->cycles - cycles == 1u && cpu->w[15] == 0x4ffeu &&
                   dspic33_read_word(cpu, address) == value && !cpu->illegal_reset,
               "direct POP covers the full implemented file address range");
    }

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    load_instruction(state, cpu, 0u, 0xf8e000u);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
               cpu->last_trap_return == 2u && cpu->w[15] == 0x5006u &&
               dspic33_read_word(cpu, 0x5000u) == 0u,
           "direct PUSH unimplemented file source completes stack state before trap");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xf8dfffu);
    expect_illegal_reset(state, cpu, "direct PUSH odd file address resets processor");
}

static void direct_stack_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;
    uint32_t legal = 0u;
    uint32_t reserved = 0u;
    uint8_t pop;

    for (pop = 0u; pop < 2u; pop++) {
        for (fields = 0u; fields <= 0xffffu; fields++) {
            uint32_t opcode = (pop != 0u ? 0xf90000u : 0xf80000u) | fields;
            if ((fields & 1u) == 0u) {
                run_direct_stack_encoding_case(state, cpu, opcode, (uint16_t)fields);
                legal++;
            } else {
                run_invalid_stack_encoding_case(state, cpu, opcode);
                reserved++;
            }
        }
    }
    expect(state, legal == 65536u,
           "direct PUSH and POP legal encoding matrix is exhaustive");
    expect(state, reserved == 65536u,
           "direct PUSH and POP reserved encoding matrix is exhaustive");
}

static void link_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;
    uint32_t legal = 0u;
    uint32_t reserved = 0u;

    for (fields = 0u; fields < 0x8000u; fields++) {
        uint32_t opcode = 0xfa0000u | fields;
        if ((fields & 0x4001u) == 0u) {
            uint16_t frame_size = (uint16_t)fields;
            uint64_t cycles;
            bool matches;

            prepare_stack_encoding_case(cpu);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[14] == 0x5002u &&
                      cpu->w[15] == (uint16_t)(0x5002u + frame_size) &&
                      dspic33_read_word(cpu, 0x5000u) == 0x4444u &&
                      cpu->corcon == 0x0024u && cpu->sr == 0x010fu &&
                      !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
            expect_dsp_matrix_case(state, matches, opcode, "LNK legal encoding");
            legal++;
        } else {
            run_invalid_stack_encoding_case(state, cpu, opcode);
            reserved++;
        }
    }
    expect(state, legal == 8192u, "LNK legal encoding matrix is exhaustive");
    expect(state, reserved == 24576u, "LNK reserved encoding matrix is exhaustive");
}

static void shadow_stack_encoding_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t legal_opcodes[] = {0xfe8000u, 0xfea000u, 0xfa8000u};
    size_t index;

    for (index = 0u; index < sizeof(legal_opcodes) / sizeof(legal_opcodes[0]);
         index++) {
        bool matches;

        reset_processor_test(cpu, 0u);
        if (legal_opcodes[index] == 0xfea000u) {
            cpu->w[0] = 0x1111u;
            cpu->w[1] = 0x2222u;
            cpu->w[2] = 0x3333u;
            cpu->w[3] = 0x4444u;
            cpu->sr = 0x01efu;
        } else if (legal_opcodes[index] == 0xfe8000u) {
            cpu->shadow_w[0] = 0x1111u;
            cpu->shadow_w[1] = 0x2222u;
            cpu->shadow_w[2] = 0x3333u;
            cpu->shadow_w[3] = 0x4444u;
            cpu->shadow_status = 0x010fu;
            cpu->sr = 0x00e0u;
        } else {
            cpu->corcon |= 0x0004u;
            dspic33_set_working_register(cpu, 14u, 0x5002u);
            dspic33_set_working_register(cpu, 15u, 0x5100u);
            dspic33_write_word(cpu, 0x5000u, 0x4444u);
        }
        matches = dspic33_load_program_word(cpu, 0u, legal_opcodes[index]) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles == 1u && !cpu->illegal_reset &&
                  cpu->unsupported_opcode == 0u;
        if (legal_opcodes[index] == 0xfea000u) {
            matches = matches && cpu->shadow_w[0] == 0x1111u &&
                      cpu->shadow_w[1] == 0x2222u && cpu->shadow_w[2] == 0x3333u &&
                      cpu->shadow_w[3] == 0x4444u && cpu->shadow_status == 0x010fu &&
                      cpu->sr == 0x01efu;
        } else if (legal_opcodes[index] == 0xfe8000u) {
            matches = matches && cpu->w[0] == 0x1111u && cpu->w[1] == 0x2222u &&
                      cpu->w[2] == 0x3333u && cpu->w[3] == 0x4444u &&
                      cpu->sr == 0x01efu;
        } else {
            matches = matches && cpu->w[14] == 0x4444u && cpu->w[15] == 0x5000u &&
                      cpu->corcon == 0x0020u;
        }
        expect_dsp_matrix_case(state, matches, legal_opcodes[index],
                               "shadow stack and ULNK encoding");
    }
}

static bool binary_matrix_registers_match(const Dspic33* cpu,
                                          const uint16_t registers[16]) {
    uint8_t reg;

    for (reg = 0u; reg < 16u; reg++) {
        if (cpu->w[reg] != registers[reg]) {
            return false;
        }
    }
    return true;
}

static void run_legal_binary_matrix_case(TestState* state, Dspic33* cpu,
                                         uint32_t opcode,
                                         BinaryMatrixOperation operation) {
    uint8_t left_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint16_t initial_status =
        binary_matrix_logical(operation)
            ? (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) |
                         (((opcode >> 8u) & 1u) << 2u) | (((opcode >> 9u) & 1u) << 3u) |
                         (((opcode >> 10u) & 1u) << 8u))
            : (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u));
    uint16_t registers[16];
    BinaryMatrixOperand source;
    BinaryMatrixOperand destination;
    uint16_t left;
    uint16_t right;
    uint16_t value;
    uint16_t expected_status;
    uint16_t source_value = byte_mode ? (uint16_t)(0x0040u | (opcode & 0x003fu))
                                      : (uint16_t)(0x2100u | (opcode & 0x00ffu));
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    left = byte_mode ? (uint8_t)registers[left_register] : registers[left_register];
    if (source_mode >= 6u) {
        source.direct = false;
        source.address = 0u;
        right = (uint16_t)(opcode & 0x001fu);
    } else {
        source = binary_matrix_operand(registers, source_mode, source_register, width);
        right = source.direct ? (byte_mode ? (uint8_t)registers[source_register]
                                           : registers[source_register])
                              : source_value;
    }
    destination =
        binary_matrix_operand(registers, destination_mode, destination_register, width);
    if (!destination.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, destination.address, 0x5au);
        } else {
            dspic33_write_word(cpu, destination.address, 0x5a5au);
        }
    }
    if (source_mode < 6u && !source.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, source.address, (uint8_t)source_value);
        } else {
            dspic33_write_word(cpu, source.address, source_value);
        }
    }
    value = binary_matrix_result(operation, left, right, initial_status, byte_mode);
    expected_status =
        binary_matrix_status(operation, left, right, initial_status, byte_mode);
    if (destination.direct) {
        if (byte_mode) {
            value = (uint16_t)((registers[destination_register] & 0xff00u) |
                               (value & 0x00ffu));
        }
        binary_matrix_write_register(registers, destination_register, value);
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!destination.direct) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, destination.address) == (uint8_t)value
                       : dspic33_read_word(cpu, destination.address) == value);
    }
    if (source_mode < 6u && !source.direct &&
        (destination.direct || destination.address != source.address)) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                       : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal binary encoding");
}

static void run_invalid_binary_matrix_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;
    uint8_t reg;

    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->illegal_reset = false;
    cpu->last_trap = UINT16_MAX;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->splim_enabled = false;
    for (reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg,
                                     (uint16_t)(0x5000u + (uint16_t)reg * 2u));
    }
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->last_trap == UINT16_MAX && cpu->pc == 0u && cpu->w[15] == 0x1000u &&
              cpu->initialized_working_registers == 0x8000u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    for (reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "illegal binary encoding");
}

static bool documented_bit_encoding_valid(uint32_t opcode) {
    uint8_t kind = (uint8_t)((opcode >> 16u) & 0x07u);
    bool file = (opcode & 0x080000u) != 0u;
    uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);

    if (file) {
        return kind != 5u && (kind != 4u || (opcode & 0x001ffeu) != 0x000042u);
    }
    if (mode >= 6u) {
        return false;
    }
    if (kind <= 2u) {
        bool byte_mode = (opcode & 0x000400u) != 0u;
        uint8_t bit = (uint8_t)((opcode >> 12u) & 0x0fu);
        return (opcode & 0x000b80u) == 0u && (!byte_mode || bit < 8u);
    }
    if (kind <= 5u) {
        return (opcode & 0x000780u) == 0u;
    }
    return (opcode & 0x000f80u) == 0u;
}

static void run_legal_register_bit_case(TestState* state, Dspic33* cpu,
                                        uint32_t opcode) {
    uint8_t kind = (uint8_t)((opcode >> 16u) & 0x07u);
    uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t reg = (uint8_t)(opcode & 0x0fu);
    bool byte_mode = kind <= 2u && (opcode & 0x000400u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint16_t initial_status = (uint16_t)(0x010cu | (opcode & 0x0003u));
    uint16_t registers[16];
    BinaryMatrixOperand operand;
    uint16_t value;
    uint16_t original;
    uint8_t bit;
    uint16_t mask;
    uint16_t expected_status = initial_status;
    uint64_t cycles;
    uint64_t expected_cycles = 1u;
    uint32_t expected_pc = 2u;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    bit = kind == 5u ? (uint8_t)(registers[(opcode >> 11u) & 0x0fu] & 0x0fu)
                     : (uint8_t)((opcode >> 12u) & 0x0fu);
    operand = binary_matrix_operand(registers, mode, reg, width);
    original = byte_mode ? (uint8_t)(0x5au ^ opcode) : (uint16_t)(0x5aa5u ^ opcode);
    if (operand.direct) {
        original = byte_mode ? (uint8_t)registers[reg] : registers[reg];
    } else if (byte_mode) {
        dspic33_write_byte(cpu, operand.address, (uint8_t)original);
    } else {
        dspic33_write_word(cpu, operand.address, original);
    }
    value = original;
    mask = (uint16_t)(1u << bit);
    if (kind == 0u) {
        value |= mask;
    } else if (kind == 1u) {
        value &= (uint16_t)~mask;
    } else if (kind == 2u) {
        value ^= mask;
    } else if (kind == 3u || kind == 5u) {
        bool zero_destination = (opcode & (kind == 5u ? 0x008000u : 0x000800u)) != 0u;
        if (zero_destination) {
            expected_status = (uint16_t)((initial_status & ~0x0002u) |
                                         ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            expected_status = (uint16_t)((initial_status & ~0x0001u) |
                                         ((value & mask) != 0u ? 0x0001u : 0u));
        }
    } else if (kind == 4u) {
        bool zero_destination = (opcode & 0x000800u) != 0u;
        if (zero_destination) {
            expected_status = (uint16_t)((initial_status & ~0x0002u) |
                                         ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            expected_status = (uint16_t)((initial_status & ~0x0001u) |
                                         ((value & mask) != 0u ? 0x0001u : 0u));
        }
        value |= mask;
    } else {
        bool set = (value & mask) != 0u;
        if ((kind == 6u && set) || (kind == 7u && !set)) {
            expected_pc = 4u;
            expected_cycles = 2u;
        }
    }
    if (kind <= 2u || kind == 4u) {
        if (operand.direct) {
            if (byte_mode) {
                value = (uint16_t)((registers[reg] & 0xff00u) | (value & 0x00ffu));
            }
            binary_matrix_write_register(registers, reg, value);
        }
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, 0u) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == expected_cycles && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!operand.direct) {
        uint16_t expected = kind <= 2u || kind == 4u ? value : original;
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, operand.address) == (uint8_t)expected
                       : dspic33_read_word(cpu, operand.address) == expected);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal register bit encoding");
}

static void run_legal_file_bit_admission_case(TestState* state, Dspic33* cpu,
                                              uint32_t opcode) {
    bool matches;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->pc = 0u;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, 0u) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal file bit encoding");
}

static void bit_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;
    uint32_t legal = 0u;
    uint32_t reserved = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (fields = 0u; fields < 0x100000u; fields++) {
        uint32_t opcode = 0xa00000u | fields;
        if (documented_bit_encoding_valid(opcode)) {
            if ((opcode & 0x080000u) != 0u) {
                run_legal_file_bit_admission_case(state, cpu, opcode);
            } else {
                run_legal_register_bit_case(state, cpu, opcode);
            }
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            reserved++;
        }
    }
    expect(state, legal == 477936u, "bit legal encoding matrix is exhaustive");
    expect(state, reserved == 570640u, "bit illegal encoding matrix is exhaustive");
}

static void direct_file_bit_value_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t kinds[] = {0u, 1u, 2u, 3u, 4u, 6u, 7u};
    static const uint16_t values[] = {0x0000u, 0xffffu, 0xa55au, 0x5aa5u};
    size_t kind_index;
    uint8_t bit;
    size_t value_index;
    uint32_t cases = 0u;

    for (kind_index = 0u; kind_index < sizeof(kinds) / sizeof(kinds[0]); kind_index++) {
        uint8_t kind = kinds[kind_index];
        for (bit = 0u; bit < 16u; bit++) {
            uint16_t address = (uint16_t)(0x1000u + (bit >> 3u));
            uint32_t opcode = 0xa80000u | ((uint32_t)kind << 16u) |
                              ((uint32_t)(bit & 7u) << 13u) | address;
            uint16_t mask = (uint16_t)(1u << bit);
            for (value_index = 0u; value_index < sizeof(values) / sizeof(values[0]);
                 value_index++) {
                uint16_t initial = values[value_index];
                uint16_t expected = initial;
                uint16_t initial_status =
                    (uint16_t)(0x010du | (uint16_t)(value_index & 2u));
                uint16_t expected_status = initial_status;
                uint32_t expected_pc = 2u;
                uint64_t expected_cycles = 1u;
                bool matches;

                reset_processor_test(cpu, 0u);
                dspic33_write_word(cpu, 0x1000u, initial);
                cpu->sr = initial_status;
                if (kind == 0u) {
                    expected |= mask;
                } else if (kind == 1u) {
                    expected &= (uint16_t)~mask;
                } else if (kind == 2u) {
                    expected ^= mask;
                } else if (kind == 3u) {
                    expected_status =
                        (uint16_t)((initial_status & ~0x0002u) |
                                   ((initial & mask) == 0u ? 0x0002u : 0u));
                } else if (kind == 4u) {
                    expected_status =
                        (uint16_t)((initial_status & ~0x0002u) |
                                   ((initial & mask) == 0u ? 0x0002u : 0u));
                    expected |= mask;
                } else {
                    bool set = (initial & mask) != 0u;
                    if ((kind == 6u && set) || (kind == 7u && !set)) {
                        expected_pc = 4u;
                        expected_cycles = 2u;
                    }
                }
                matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                          dspic33_load_program_word(cpu, 2u, 0u) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING &&
                          cpu->pc == expected_pc && cpu->cycles == expected_cycles &&
                          cpu->sr == expected_status &&
                          dspic33_read_word(cpu, 0x1000u) == expected &&
                          !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "direct file bit value and status");
                cases++;
            }
        }
    }
    expect(state, cases == 448u,
           "direct file bit value matrix covers every operation and bit");
}

static void bit_operand_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xa40012u);
    dspic33_set_working_register(cpu, 2u, 0x0042u);
    expect_illegal_reset(state, cpu, "indirect BTSTS targeting SR resets processor");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xa40012u);
    cpu->w[2] = 0x1000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu,
                         "BTSTS uninitialized source pointer resets processor");
}

static bool documented_table_encoding_valid(uint32_t opcode) {
    bool write = (opcode & 0x010000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);

    return write ? source_mode < 6u && destination_mode >= 1u && destination_mode < 6u
                 : source_mode >= 1u && source_mode < 6u && destination_mode < 6u;
}

static void prepare_table_encoding_case(Dspic33* cpu, bool write) {
    uint8_t reg;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->pc = 0u;
    cpu->tblpag = write ? 0x00fau : 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    for (reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg, 0x5000u);
    }
    dspic33_write_word(cpu, 0x4ffeu, 0xa55au);
    dspic33_write_word(cpu, 0x5000u, 0xa55au);
    dspic33_write_word(cpu, 0x5002u, 0xa55au);
    dspic33_load_program_word(cpu, 0x4ffeu, 0x12ab56u);
    dspic33_load_program_word(cpu, 0x5000u, 0x12ab56u);
    dspic33_load_program_word(cpu, 0x5002u, 0x12ab56u);
}

static void run_legal_table_encoding_case(TestState* state, Dspic33* cpu,
                                          uint32_t opcode) {
    bool write = (opcode & 0x010000u) != 0u;
    bool matches;

    prepare_table_encoding_case(cpu, write);
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal table encoding");
}

static void table_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;
    uint32_t legal = 0u;
    uint32_t illegal = 0u;

    for (fields = 0u; fields < 0x20000u; fields++) {
        uint32_t opcode = 0xba0000u | fields;
        if (documented_table_encoding_valid(opcode)) {
            run_legal_table_encoding_case(state, cpu, opcode);
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            illegal++;
        }
    }
    expect(state, legal == 61440u, "table legal encoding matrix is exhaustive");
    expect(state, illegal == 69632u, "table illegal encoding matrix is exhaustive");
}

static void table_value_cases(TestState* state, Dspic33* cpu) {
    uint8_t high;
    uint8_t byte_mode;
    uint8_t odd;

    for (high = 0u; high < 2u; high++) {
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            for (odd = 0u; odd < 2u; odd++) {
                uint32_t opcode = 0xba0000u | ((uint32_t)high << 15u) |
                                  ((uint32_t)byte_mode << 14u) | ((uint32_t)3u << 7u) |
                                  0x0012u;
                uint16_t expected = high != 0u ? 0x0012u : 0xab56u;
                bool matches;

                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                load_instruction(state, cpu, 0x0200u, 0x12ab56u);
                cpu->tblpag = 0u;
                dspic33_set_working_register(cpu, 2u, (uint16_t)(0x0200u + odd));
                dspic33_set_working_register(cpu, 3u, 0xa500u);
                if (byte_mode != 0u) {
                    expected = high != 0u ? (odd != 0u ? 0u : 0x0012u)
                                          : (odd != 0u ? 0x00abu : 0x0056u);
                    expected |= 0xa500u;
                }
                matches = dspic33_step(cpu) == DSPIC33_RUNNING &&
                          cpu->w[2] == (uint16_t)(0x0200u + odd) &&
                          cpu->w[3] == expected && cpu->sr == 0u && cpu->cycles == 5u &&
                          !cpu->illegal_reset;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "table read value and byte selection");

                opcode = 0xbb0000u | ((uint32_t)high << 15u) |
                         ((uint32_t)byte_mode << 14u) | ((uint32_t)1u << 11u) |
                         ((uint32_t)3u << 7u) | 2u;
                reset_processor_test(cpu, 0u);
                load_instruction(state, cpu, 0u, opcode);
                cpu->tblpag = 0x00fau;
                dspic33_set_working_register(cpu, 2u, 0xa5c3u);
                dspic33_set_working_register(cpu, 3u, odd);
                matches = dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles == 2u &&
                          !cpu->illegal_reset;
                expected = 0xffffu;
                if (high != 0u) {
                    matches =
                        matches && ((cpu->write_latches[0] >> 16u) & 0xffu) ==
                                       (byte_mode != 0u && odd != 0u ? 0xffu : 0xc3u);
                } else {
                    expected = byte_mode == 0u ? 0xa5c3u
                               : odd != 0u     ? 0xc3ffu
                                               : 0xffc3u;
                    matches = matches && (cpu->write_latches[0] & 0xffffu) == expected;
                }
                expect_dsp_matrix_case(state, matches, opcode,
                                       "table write latch and byte selection");
            }
        }
    }
}

static void table_operand_lifecycle_cases(TestState* state, Dspic33* cpu) {
    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xbb0982u);
    dspic33_set_working_register(cpu, 2u, 0xa55au);
    cpu->w[3] = 0x5000u;
    cpu->initialized_working_registers &= (uint16_t)~0x0008u;
    expect_illegal_reset(state, cpu,
                         "table write uninitialized destination resets processor");

    reset_processor_test(cpu, 0u);
    load_instruction(state, cpu, 0u, 0xbb0992u);
    cpu->w[2] = 0x5000u;
    dspic33_set_working_register(cpu, 3u, 0u);
    cpu->initialized_working_registers &= (uint16_t)~0x0004u;
    expect_illegal_reset(state, cpu,
                         "table write uninitialized source resets processor");
}

static bool documented_system_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);

    if (family == 0xfcu) {
        return (opcode & 0x00c000u) == 0u;
    }
    if (family != 0xfeu) {
        return true;
    }
    if (opcode == 0xfe0000u || opcode == 0xfe2000u ||
        (opcode & 0xfffffeu) == 0xfe4000u || opcode == 0xfe6000u ||
        opcode == 0xfe8000u || opcode == 0xfea000u) {
        return true;
    }
    if ((opcode & 0xfff000u) == 0xfec000u) {
        return ((opcode >> 10u) & 3u) != 3u;
    }
    return (opcode & 0xfff000u) == 0xfed000u && (opcode & 0x0003f0u) == 0u &&
           ((opcode >> 10u) & 3u) != 3u;
}

static void prepare_system_encoding_case(Dspic33* cpu) {
    reset_processor_test(cpu, 0x0200u);
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    dspic33_set_working_register(cpu, 15u, 0x5000u);
}

static void run_legal_system_encoding_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode) {
    bool matches;

    prepare_system_encoding_case(cpu);
    matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
              dspic33_step(cpu) != DSPIC33_UNSUPPORTED_INSTRUCTION &&
              cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
    expect_dsp_matrix_case(state, matches, opcode, "legal system encoding");
}

static void run_illegal_system_encoding_case(TestState* state, Dspic33* cpu,
                                             uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    prepare_system_encoding_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->unsupported_opcode == 0u;
    expect_dsp_matrix_case(state, matches, opcode, "illegal system encoding");
}

static void system_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t fields;
    uint32_t legal = 0u;
    uint32_t illegal = 0u;

    for (fields = 0u; fields <= 0xffffu; fields++) {
        run_legal_system_encoding_case(state, cpu, fields);
        legal++;
        run_legal_system_encoding_case(state, cpu, 0xff0000u | fields);
        legal++;
    }
    for (fields = 0u; fields <= 0xffffu; fields++) {
        uint32_t opcode = 0xfc0000u | fields;
        if (documented_system_encoding_valid(opcode)) {
            run_legal_system_encoding_case(state, cpu, opcode);
            legal++;
        } else {
            run_illegal_system_encoding_case(state, cpu, opcode);
            illegal++;
        }
    }
    for (fields = 0u; fields <= 0xffffu; fields++) {
        uint32_t opcode = 0xfe0000u | fields;
        if (documented_system_encoding_valid(opcode)) {
            run_legal_system_encoding_case(state, cpu, opcode);
            legal++;
        } else {
            run_illegal_system_encoding_case(state, cpu, opcode);
            illegal++;
        }
    }
    expect(state, legal == 150583u, "system legal encoding matrix is exhaustive");
    expect(state, illegal == 111561u, "system illegal encoding matrix is exhaustive");
}

static void system_control_value_cases(TestState* state, Dspic33* cpu) {
    uint32_t literal;

    for (literal = 0u; literal < 0x4000u; literal++) {
        uint32_t opcode = 0xfc0000u | literal;
        uint64_t cycles;
        bool matches;

        prepare_system_encoding_case(cpu);
        cpu->disicnt = 0u;
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u &&
                  cpu->cycles - cycles == 1u &&
                  cpu->disicnt == (literal == 0u ? 0u : literal) &&
                  cpu->sr == 0x010fu && !cpu->illegal_reset;
        expect_dsp_matrix_case(state, matches, opcode, "DISI literal value");
    }
    expect(state, literal == 0x4000u, "DISI value matrix is exhaustive");

    prepare_system_encoding_case(cpu);
    expect(state,
           dspic33_load_program_word(cpu, 0x0200u, 0xfe2000u) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u &&
               cpu->cycles == 1u && cpu->sr == 0x010fu && cpu->corcon == 0x0020u,
           "BOOTSWP executes as NOP when dual boot is unavailable");

    prepare_system_encoding_case(cpu);
    cpu->watchdog.ticks = 123u;
    cpu->configuration[10u] |= 0x40u;
    expect(state,
           dspic33_load_program_word(cpu, 0x0200u, 0xfe6000u) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->watchdog.ticks == 0u &&
               cpu->sr == 0x010fu,
           "CLRWDT clears watchdog state and preserves status");
}

static bool documented_divide_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    uint8_t divisor = (uint8_t)(opcode & 0x0fu);

    if (family == 0xd9u) {
        return (opcode & 0x0087f0u) == 0u;
    }
    if (divisor < 2u) {
        return false;
    }
    if (family != 0xd8u || (opcode & 0x000030u) != 0u) {
        return false;
    }
    uint8_t high = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t low = (uint8_t)((opcode >> 7u) & 0x0fu);
    if ((opcode & 0x000040u) == 0u) {
        return high == 0u;
    }
    return (low & 1u) == 0u && high == low + 1u;
}

static bool run_legal_divide_matrix_case(Dspic33* cpu, uint32_t opcode) {
    bool unsigned_divide = (opcode & 0x008000u) != 0u;
    bool double_word = (opcode & 0x000040u) != 0u;
    bool fractional = (opcode & 0xff0000u) == 0xd90000u;
    uint8_t high = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t low = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t divisor_register = (uint8_t)(opcode & 0x0fu);
    int64_t quotient;
    int64_t remainder;
    bool overflow;
    uint8_t reg;

    reset_processor_test(cpu, 0x0200u);
    for (reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg, (uint16_t)(0x0100u + reg));
    }
    if (fractional) {
        dspic33_set_working_register(cpu, high, 0x1000u);
        dspic33_set_working_register(cpu, divisor_register, 0x4000u);
        quotient =
            (int64_t)(int16_t)cpu->w[high] * 32768 / (int16_t)cpu->w[divisor_register];
        remainder =
            (int64_t)(int16_t)cpu->w[high] * 32768 % (int16_t)cpu->w[divisor_register];
        overflow = quotient < INT16_MIN || quotient > INT16_MAX;
    } else if (double_word) {
        dspic33_set_working_register(cpu, low, 0x1234u);
        dspic33_set_working_register(cpu, high, 0u);
        dspic33_set_working_register(cpu, divisor_register, 17u);
        if (unsigned_divide) {
            uint32_t dividend = ((uint32_t)cpu->w[high] << 16u) | cpu->w[low];
            quotient = dividend / cpu->w[divisor_register];
            remainder = dividend % cpu->w[divisor_register];
            overflow = quotient > UINT16_MAX;
        } else {
            int32_t dividend = (int32_t)(((uint32_t)cpu->w[high] << 16u) | cpu->w[low]);
            quotient = (int64_t)dividend / (int16_t)cpu->w[divisor_register];
            remainder = (int64_t)dividend % (int16_t)cpu->w[divisor_register];
            overflow = quotient < INT16_MIN || quotient > INT16_MAX;
        }
    } else {
        dspic33_set_working_register(cpu, low, 0x1234u);
        dspic33_set_working_register(cpu, divisor_register, 17u);
        if (unsigned_divide) {
            quotient = cpu->w[low] / cpu->w[divisor_register];
            remainder = cpu->w[low] % cpu->w[divisor_register];
        } else {
            quotient = (int16_t)cpu->w[low] / (int16_t)cpu->w[divisor_register];
            remainder = (int16_t)cpu->w[low] % (int16_t)cpu->w[divisor_register];
        }
        overflow = quotient < INT16_MIN || quotient > UINT16_MAX;
    }
    if (!dspic33_load_program_word(cpu, 0x0200u, 0x090011u) ||
        !dspic33_load_program_word(cpu, 0x0202u, opcode) ||
        dspic33_step(cpu) != DSPIC33_RUNNING) {
        return false;
    }
    while (cpu->repeat_active != 0u) {
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            return false;
        }
    }
    uint16_t expected_status =
        (uint16_t)((remainder == 0 ? 0x0002u : 0u) | (remainder < 0 ? 0x0008u : 0u) |
                   (overflow ? 0x0004u : 0u));
    return (overflow ||
            (cpu->w[0] == (uint16_t)quotient && cpu->w[1] == (uint16_t)remainder)) &&
           (cpu->sr & 0x000eu) == expected_status && cpu->pc == 0x0204u &&
           cpu->cycles == 19u && !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
}

static void divide_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t opcode;
    uint32_t legal = 0u;
    uint32_t illegal = 0u;

    for (opcode = 0xd80000u; opcode <= 0xd9ffffu; opcode++) {
        if (documented_divide_encoding_valid(opcode)) {
            expect_dsp_matrix_case(state, run_legal_divide_matrix_case(cpu, opcode),
                                   opcode, "legal divide encoding and result");
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            illegal++;
        }
    }
    expect(state, legal == 928u, "divide legal encoding matrix is exhaustive");
    expect(state, illegal == 130144u, "divide illegal encoding matrix is exhaustive");
}

static uint16_t decimal_adjust_reference(uint16_t value, uint16_t status, bool* carry) {
    uint16_t adjusted = (uint8_t)value;

    if ((adjusted & 0x000fu) > 9u || (status & 0x0100u) != 0u) {
        adjusted += 6u;
    }
    if (adjusted > 0x009fu || (status & 0x0001u) != 0u) {
        adjusted += 0x0060u;
    }
    *carry = (status & 0x0001u) != 0u || adjusted > 0x00ffu;
    return (uint16_t)((value & 0xff00u) | (adjusted & 0x00ffu));
}

static void decimal_adjust_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint32_t invalid = 0u;
    uint32_t opcode;
    uint16_t value;
    uint16_t status_inputs;
    uint8_t destination;

    for (destination = 0u; destination < 16u; destination++) {
        for (status_inputs = 0u; status_inputs < 4u; status_inputs++) {
            uint16_t status =
                (uint16_t)(0x000eu | ((status_inputs & 1u) != 0u ? 1u : 0u) |
                           ((status_inputs & 2u) != 0u ? 0x0100u : 0u));
            for (value = 0u; value <= UINT8_MAX; value++) {
                bool carry;
                opcode = 0xfd4000u | destination;
                uint16_t initial = (uint16_t)(0xa500u | value);
                uint16_t expected = decimal_adjust_reference(initial, status, &carry);
                uint16_t expected_status =
                    (uint16_t)((status & ~1u) | (carry ? 1u : 0u));
                if (destination == 15u) {
                    expected &= 0xfffeu;
                }
                bool matches;

                reset_processor_test(cpu, 0x0200u);
                cpu->sr = status;
                dspic33_set_working_register(cpu, destination, initial);
                matches = dspic33_load_program_word(cpu, 0x0200u, opcode) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0202u &&
                          cpu->cycles == 1u && cpu->w[destination] == expected &&
                          cpu->sr == expected_status && !cpu->illegal_reset &&
                          cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "decimal adjust value and flags");
                cases++;
            }
        }
    }
    expect(state, cases == 16384u,
           "decimal adjust matrix covers every byte, flag and register");

    for (opcode = 0xfd4000u; opcode <= 0xfd4fffu; opcode++) {
        if ((opcode & 0xfffff0u) != 0xfd4000u) {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, invalid == 4080u,
           "decimal adjust illegal encoding matrix is exhaustive");
}

static void general_arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[6] = {0x100000u, 0x180000u, 0x400000u,
                                      0x480000u, 0x500000u, 0x580000u};
    uint32_t legal = 0u;
    uint32_t invalid = 0u;
    uint8_t operation;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 6u; operation++) {
        uint32_t fields;
        for (fields = 0u; fields < 0x080000u; fields++) {
            uint32_t opcode = bases[operation] | fields;
            uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
            if (destination_mode >= 6u) {
                run_invalid_binary_matrix_case(state, cpu, opcode);
                invalid++;
            } else {
                run_legal_binary_matrix_case(state, cpu, opcode,
                                             (BinaryMatrixOperation)operation);
                legal++;
            }
        }
    }
    expect(state, legal == 2359296u,
           "general arithmetic legal encoding matrix is exhaustive");
    expect(state, invalid == 786432u,
           "general arithmetic illegal encoding matrix is exhaustive");
}

static void general_logical_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[3] = {0x600000u, 0x680000u, 0x700000u};
    static const BinaryMatrixOperation operations[3] = {
        ARITHMETIC_MATRIX_AND, ARITHMETIC_MATRIX_XOR, ARITHMETIC_MATRIX_IOR};
    uint32_t legal = 0u;
    uint32_t invalid = 0u;
    uint8_t operation;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 3u; operation++) {
        uint32_t fields;
        for (fields = 0u; fields < 0x080000u; fields++) {
            uint32_t opcode = bases[operation] | fields;
            uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
            if (destination_mode >= 6u) {
                run_invalid_binary_matrix_case(state, cpu, opcode);
                invalid++;
            } else {
                run_legal_binary_matrix_case(state, cpu, opcode, operations[operation]);
                legal++;
            }
        }
    }
    expect(state, legal == 1179648u,
           "general logical legal encoding matrix is exhaustive");
    expect(state, invalid == 393216u,
           "general logical illegal encoding matrix is exhaustive");
}

static void run_literal_binary_matrix_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode,
                                           BinaryMatrixOperation operation,
                                           uint16_t literal, bool byte_mode) {
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    uint16_t initial_status =
        binary_matrix_logical(operation)
            ? (uint16_t)((destination & 1u) | (((literal >> 1u) & 1u) << 1u) |
                         (((literal >> 2u) & 1u) << 2u) |
                         (((literal >> 3u) & 1u) << 3u) |
                         (((literal >> 4u) & 1u) << 8u))
            : (uint16_t)((destination & 1u) | (((literal >> 1u) & 1u) << 1u));
    static const uint16_t byte_values[4] = {0x0000u, 0x0080u, 0x00ffu, 0x0055u};
    static const uint16_t word_values[4] = {0x0000u, 0x8000u, 0xffffu, 0x5555u};
    uint16_t left =
        byte_mode ? byte_values[destination & 3u] : word_values[destination & 3u];
    uint16_t expected;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    cpu->pc = 0u;
    cpu->sr = initial_status;
    cpu->corcon = 0x0020u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->events.count = 0u;
    dspic33_set_working_register(cpu, destination,
                                 byte_mode ? (uint16_t)(0xa500u | left) : left);
    left = byte_mode ? (uint8_t)cpu->w[destination] : cpu->w[destination];
    expected =
        binary_matrix_result(operation, left, literal, initial_status, byte_mode);
    expected_status =
        binary_matrix_status(operation, left, literal, initial_status, byte_mode);
    if (byte_mode) {
        expected = (uint16_t)((cpu->w[destination] & 0xff00u) | expected);
    }
    if (destination == 15u) {
        expected &= 0xfffeu;
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
              cpu->sr == expected_status && cpu->corcon == 0x0020u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "literal binary encoding");
}

static void literal_arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[4] = {0xb00000u, 0xb08000u, 0xb10000u, 0xb18000u};
    static const BinaryMatrixOperation operations[4] = {
        ARITHMETIC_MATRIX_ADD, ARITHMETIC_MATRIX_ADDC, ARITHMETIC_MATRIX_SUB,
        ARITHMETIC_MATRIX_SUBB};
    uint32_t cases = 0u;
    uint8_t operation;
    uint8_t byte_mode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 4u; operation++) {
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint16_t maximum = byte_mode != 0u ? UINT8_MAX : 0x03ffu;
            uint16_t literal;
            for (literal = 0u; literal <= maximum; literal++) {
                uint8_t destination;
                for (destination = 0u; destination < 16u; destination++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)literal << 4u) | destination;
                    run_literal_binary_matrix_case(state, cpu, opcode,
                                                   operations[operation], literal,
                                                   byte_mode != 0u);
                    cases++;
                }
            }
        }
    }
    expect(state, cases == 81920u,
           "literal arithmetic encoding matrix covers every valid form");
}

static void literal_logical_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[3] = {0xb20000u, 0xb28000u, 0xb30000u};
    static const BinaryMatrixOperation operations[3] = {
        ARITHMETIC_MATRIX_AND, ARITHMETIC_MATRIX_XOR, ARITHMETIC_MATRIX_IOR};
    uint32_t legal = 0u;
    uint32_t invalid = 0u;
    uint8_t operation;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 3u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint16_t maximum = byte_mode != 0u ? UINT8_MAX : 0x03ffu;
            uint16_t literal;
            for (literal = 0u; literal <= maximum; literal++) {
                uint8_t destination;
                for (destination = 0u; destination < 16u; destination++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)literal << 4u) | destination;
                    run_literal_binary_matrix_case(state, cpu, opcode,
                                                   operations[operation], literal,
                                                   byte_mode != 0u);
                    legal++;
                }
            }
        }
    }
    for (uint32_t opcode = 0xb38000u; opcode < 0xb40000u; opcode++) {
        if ((opcode & 0xfff000u) != 0xb3c000u) {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 61440u,
           "literal logical legal encoding matrix is exhaustive");
    expect(state, invalid == 28672u,
           "literal logical illegal encoding matrix is exhaustive");
}

static void arithmetic_flag_boundary_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[6] = {0x100000u, 0x180000u, 0x400000u,
                                      0x480000u, 0x500000u, 0x580000u};
    static const uint16_t byte_values[8] = {0x00u, 0x01u, 0x0fu, 0x10u,
                                            0x7fu, 0x80u, 0xfeu, 0xffu};
    static const uint16_t word_values[8] = {0x0000u, 0x0001u, 0x00ffu, 0x0100u,
                                            0x7fffu, 0x8000u, 0xfffeu, 0xffffu};
    uint32_t cases = 0u;
    uint8_t operation;
    uint8_t byte_mode;
    uint8_t left_index;
    uint8_t right_index;
    uint8_t carry;
    uint8_t zero;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (operation = 0u; operation < 6u; operation++) {
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            const uint16_t* values = byte_mode != 0u ? byte_values : word_values;
            for (left_index = 0u; left_index < 8u; left_index++) {
                for (right_index = 0u; right_index < 8u; right_index++) {
                    for (carry = 0u; carry < 2u; carry++) {
                        for (zero = 0u; zero < 2u; zero++) {
                            uint16_t initial_status =
                                (uint16_t)(carry | ((uint16_t)zero << 1u));
                            uint32_t opcode = bases[operation] | ((uint32_t)2u << 15u) |
                                              ((uint32_t)byte_mode << 14u) |
                                              ((uint32_t)4u << 7u) | 3u;
                            uint16_t expected = binary_matrix_result(
                                (BinaryMatrixOperation)operation, values[left_index],
                                values[right_index], initial_status, byte_mode != 0u);
                            uint16_t expected_status = binary_matrix_status(
                                (BinaryMatrixOperation)operation, values[left_index],
                                values[right_index], initial_status, byte_mode != 0u);
                            bool matches;

                            cpu->pc = 0u;
                            cpu->sr = initial_status;
                            cpu->corcon = 0x0020u;
                            cpu->unsupported_opcode = 0u;
                            cpu->last_trap = UINT16_MAX;
                            cpu->address_error = false;
                            cpu->illegal_reset = false;
                            cpu->stop_reason = DSPIC33_RUNNING;
                            cpu->events.count = 0u;
                            dspic33_set_working_register(cpu, 2u, values[left_index]);
                            dspic33_set_working_register(cpu, 3u, values[right_index]);
                            dspic33_set_working_register(cpu, 4u, 0xa55au);
                            if (byte_mode != 0u) {
                                expected = (uint16_t)(0xa500u | expected);
                            }
                            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                                      dspic33_step(cpu) == DSPIC33_RUNNING &&
                                      cpu->pc == 2u && cpu->w[4] == expected &&
                                      cpu->sr == expected_status &&
                                      cpu->unsupported_opcode == 0u &&
                                      !cpu->address_error && !cpu->illegal_reset;
                            expect_dsp_matrix_case(state, matches, opcode,
                                                   "arithmetic flag boundary");
                            cases++;
                        }
                    }
                }
            }
        }
    }
    expect(state, cases == 3072u, "arithmetic flag boundary matrix is exhaustive");
}

static void arithmetic_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    general_arithmetic_encoding_matrix_cases(state, cpu);
    general_logical_encoding_matrix_cases(state, cpu);
    literal_arithmetic_encoding_matrix_cases(state, cpu);
    literal_logical_encoding_matrix_cases(state, cpu);
    arithmetic_flag_boundary_cases(state, cpu);
}

static bool direct_file_address_implemented(uint16_t address) {
    return dspic33ep512mu810_address_implemented(address);
}

static bool direct_file_reads_source(DirectFileOperation operation) {
    return operation != DIRECT_FILE_CLR && operation != DIRECT_FILE_SETM;
}

static bool direct_file_writes_result(DirectFileOperation operation) {
    return operation < DIRECT_FILE_CP;
}

static bool direct_file_shift_operation(DirectFileOperation operation) {
    return operation >= DIRECT_FILE_SL && operation <= DIRECT_FILE_RRC;
}

static uint16_t shift_matrix_result(DirectFileOperation operation, uint16_t source,
                                    uint16_t initial_status, bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t carry = initial_status & 1u;

    source &= mask;
    if (operation == DIRECT_FILE_SL) {
        return (uint16_t)((source << 1u) & mask);
    }
    if (operation == DIRECT_FILE_LSR) {
        return (uint16_t)(source >> 1u);
    }
    if (operation == DIRECT_FILE_ASR) {
        return (uint16_t)((source >> 1u) | (source & sign));
    }
    if (operation == DIRECT_FILE_RLNC) {
        return (uint16_t)(((source << 1u) & mask) | ((source & sign) != 0u ? 1u : 0u));
    }
    if (operation == DIRECT_FILE_RLC) {
        return (uint16_t)(((source << 1u) & mask) | carry);
    }
    if (operation == DIRECT_FILE_RRNC) {
        return (uint16_t)((source >> 1u) | ((source & 1u) != 0u ? sign : 0u));
    }
    return (uint16_t)((source >> 1u) | (carry != 0u ? sign : 0u));
}

static uint16_t shift_matrix_status(DirectFileOperation operation, uint16_t source,
                                    uint16_t initial_status, bool byte_mode) {
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t value = shift_matrix_result(operation, source, initial_status, byte_mode);
    uint16_t status = (uint16_t)(initial_status & ~0x000au);

    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    if (operation == DIRECT_FILE_SL || operation == DIRECT_FILE_RLC) {
        status = (uint16_t)((status & ~1u) | ((source & sign) != 0u ? 1u : 0u));
    } else if (operation == DIRECT_FILE_LSR || operation == DIRECT_FILE_ASR ||
               operation == DIRECT_FILE_RRC) {
        status = (uint16_t)((status & ~1u) | (source & 1u));
    }
    return status;
}

static uint16_t direct_file_result(DirectFileOperation operation, uint16_t left,
                                   uint16_t right, uint16_t initial_status,
                                   bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;

    if (operation <= DIRECT_FILE_SUBB) {
        return binary_matrix_result((BinaryMatrixOperation)operation, left, right,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_AND) {
        return (uint16_t)((left & right) & mask);
    }
    if (operation == DIRECT_FILE_XOR) {
        return (uint16_t)((left ^ right) & mask);
    }
    if (operation == DIRECT_FILE_IOR) {
        return (uint16_t)((left | right) & mask);
    }
    if (operation == DIRECT_FILE_INC || operation == DIRECT_FILE_INC2) {
        return binary_matrix_result(ARITHMETIC_MATRIX_ADD, left,
                                    operation == DIRECT_FILE_INC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_DEC || operation == DIRECT_FILE_DEC2) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, left,
                                    operation == DIRECT_FILE_DEC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_NEG) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, 0u, left, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_COM) {
        return (uint16_t)(~left & mask);
    }
    if (direct_file_shift_operation(operation)) {
        return shift_matrix_result(operation, left, initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_CP) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CPB) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUBB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CP0) {
        return binary_matrix_result(ARITHMETIC_MATRIX_SUB, left, 0u, initial_status,
                                    byte_mode);
    }
    return operation == DIRECT_FILE_SETM ? mask : 0u;
}

static uint16_t direct_file_logic_status(uint16_t initial_status, uint16_t value,
                                         bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t status = (uint16_t)(initial_status & ~0x000au);

    value &= mask;
    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    return status;
}

static uint16_t direct_file_status(DirectFileOperation operation, uint16_t left,
                                   uint16_t right, uint16_t initial_status,
                                   bool byte_mode) {
    uint16_t value =
        direct_file_result(operation, left, right, initial_status, byte_mode);

    if (operation <= DIRECT_FILE_SUBB) {
        return binary_matrix_status((BinaryMatrixOperation)operation, left, right,
                                    initial_status, byte_mode);
    }
    if (operation <= DIRECT_FILE_IOR || operation == DIRECT_FILE_COM) {
        return direct_file_logic_status(initial_status, value, byte_mode);
    }
    if (direct_file_shift_operation(operation)) {
        return shift_matrix_status(operation, left, initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_INC || operation == DIRECT_FILE_INC2) {
        return binary_matrix_status(ARITHMETIC_MATRIX_ADD, left,
                                    operation == DIRECT_FILE_INC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_DEC || operation == DIRECT_FILE_DEC2) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, left,
                                    operation == DIRECT_FILE_DEC2 ? 2u : 1u,
                                    initial_status, byte_mode);
    }
    if (operation == DIRECT_FILE_NEG) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, 0u, left, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CP) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CPB) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUBB, left, right, initial_status,
                                    byte_mode);
    }
    if (operation == DIRECT_FILE_CP0) {
        return binary_matrix_status(ARITHMETIC_MATRIX_SUB, left, 0u, initial_status,
                                    byte_mode);
    }
    return initial_status;
}

static void run_legal_unary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                        DirectFileOperation operation) {
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    bool reads_source = direct_file_reads_source(operation);
    uint16_t initial_status =
        (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) |
                   (((opcode >> 8u) & 1u) << 2u) | (((opcode >> 9u) & 1u) << 3u) |
                   (((opcode >> 10u) & 1u) << 8u));
    uint16_t registers[16];
    BinaryMatrixOperand source = {0u, true};
    BinaryMatrixOperand destination;
    uint16_t source_value = byte_mode ? (uint16_t)(0x0040u | (opcode & 0x003fu))
                                      : (uint16_t)(0x2100u | (opcode & 0x00ffu));
    uint16_t source_operand = 0u;
    uint16_t value;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    if (reads_source) {
        source = binary_matrix_operand(registers, source_mode, source_register, width);
        source_operand = source.direct
                             ? (byte_mode ? (uint8_t)registers[source_register]
                                          : registers[source_register])
                             : source_value;
    }
    destination =
        binary_matrix_operand(registers, destination_mode, destination_register, width);
    if (!destination.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, destination.address, 0x5au);
        } else {
            dspic33_write_word(cpu, destination.address, 0x5a5au);
        }
    }
    if (reads_source && !source.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, source.address, (uint8_t)source_value);
        } else {
            dspic33_write_word(cpu, source.address, source_value);
        }
    }
    value =
        direct_file_result(operation, source_operand, 0u, initial_status, byte_mode);
    expected_status =
        direct_file_status(operation, source_operand, 0u, initial_status, byte_mode);
    if (destination.direct) {
        if (byte_mode) {
            value = (uint16_t)((registers[destination_register] & 0xff00u) |
                               (value & 0x00ffu));
        }
        binary_matrix_write_register(registers, destination_register, value);
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!destination.direct) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, destination.address) == (uint8_t)value
                       : dspic33_read_word(cpu, destination.address) == value);
    }
    if (reads_source && !source.direct &&
        (destination.direct || destination.address != source.address)) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                       : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal unary encoding");
}

static void general_unary_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    static const DirectFileOperation operations[4][2] = {
        {DIRECT_FILE_INC, DIRECT_FILE_INC2},
        {DIRECT_FILE_DEC, DIRECT_FILE_DEC2},
        {DIRECT_FILE_NEG, DIRECT_FILE_COM},
        {DIRECT_FILE_CLR, DIRECT_FILE_SETM}};
    uint32_t legal = 0u;
    uint32_t invalid = 0u;
    uint32_t fields;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (fields = 0u; fields < 0x040000u; fields++) {
        uint32_t opcode = 0xe80000u | fields;
        uint8_t family = (uint8_t)((opcode >> 16u) - 0xe8u);
        uint8_t alternate = (uint8_t)((opcode >> 15u) & 1u);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        bool nullary = family == 3u;
        bool valid = destination_mode < 6u &&
                     (nullary ? (opcode & 0x00007fu) == 0u : source_mode < 6u);

        if (valid) {
            run_legal_unary_matrix_case(state, cpu, opcode,
                                        operations[family][alternate]);
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 110976u,
           "general unary legal encoding matrix is exhaustive");
    expect(state, invalid == 151168u,
           "general unary illegal encoding matrix is exhaustive");
}

static uint16_t direct_file_boundary_value(uint8_t index, bool byte_mode) {
    static const uint16_t byte_values[32] = {
        0x00u, 0x01u, 0x02u, 0x0eu, 0x0fu, 0x10u, 0x7du, 0x7eu, 0x7fu, 0x80u, 0x81u,
        0xfdu, 0xfeu, 0xffu, 0x55u, 0xaau, 0x8eu, 0x8fu, 0x90u, 0x91u, 0xf0u, 0x11u,
        0x22u, 0x33u, 0x44u, 0x66u, 0x77u, 0x88u, 0x99u, 0xbbu, 0xccu, 0xddu};
    static const uint16_t word_values[32] = {
        0x0000u, 0x0001u, 0x0002u, 0x00feu, 0x00ffu, 0x0100u, 0x7ffdu, 0x7ffeu,
        0x7fffu, 0x8000u, 0x8001u, 0xfffdu, 0xfffeu, 0xffffu, 0x5555u, 0xaaaau,
        0x80feu, 0x80ffu, 0x8100u, 0x8101u, 0xff00u, 0x1111u, 0x2222u, 0x3333u,
        0x4444u, 0x6666u, 0x7777u, 0x8888u, 0x9999u, 0xbbbbu, 0xccccu, 0xddddu};

    return byte_mode ? byte_values[index & 0x1fu] : word_values[index & 0x1fu];
}

static void prepare_direct_file_case(Dspic33* cpu, uint16_t address, bool byte_mode) {
    uint8_t reg;
    uint16_t initial_status =
        (uint16_t)(((address >> 10u) & 1u) | (((address >> 11u) & 1u) << 1u));
    uint16_t operand = direct_file_boundary_value((uint8_t)(address >> 5u), byte_mode);
    uint16_t source = direct_file_boundary_value((uint8_t)address, byte_mode);

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    cpu->stop_on_trap = true;
    cpu->instructions = 0u;
    cpu->cycles = 0u;
    cpu->device_cycles = 0u;
    cpu->interrupt_count = 0u;
    cpu->software_reset_count = 0u;
    cpu->illegal_reset_count = 0u;
    cpu->trap_count = 0u;
    for (reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(
            cpu, reg, (uint16_t)(0x4200u + (uint16_t)reg * 0x0101u + address));
    }
    dspic33_set_working_register(cpu, 0u,
                                 byte_mode ? (uint16_t)(0xa500u | operand) : operand);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    cpu->sr = initial_status;
    cpu->corcon = 0x0020u;
    if (address >= 0x1000u) {
        if (byte_mode) {
            dspic33_write_word(cpu, address & 0xfffeu, 0x5aa5u);
            dspic33_write_byte(cpu, address, (uint8_t)source);
        } else {
            dspic33_write_word(cpu, address & 0xfffeu, source);
        }
    }
}

static void write_direct_file_reference_result(Dspic33* cpu, uint16_t value,
                                               bool byte_mode) {
    if (byte_mode) {
        cpu->w[0] = (uint16_t)((cpu->w[0] & 0xff00u) | (value & 0x00ffu));
    } else {
        cpu->w[0] = value;
        cpu->initialized_working_registers |= 0x0001u;
    }
    cpu->instruction_working_register_writes |= 0x0001u;
}

static bool direct_file_event_queues_match(const Dspic33* actual,
                                           const Dspic33* expected) {
    size_t index;

    if (actual->events.count != expected->events.count ||
        actual->events.sequence != expected->events.sequence) {
        return false;
    }
    for (index = 0u; index < actual->events.count; index++) {
        const Dspic33Event* actual_event = &actual->events.items[index];
        const Dspic33Event* expected_event = &expected->events.items[index];
        if (actual_event->cycle != expected_event->cycle ||
            actual_event->sequence != expected_event->sequence ||
            actual_event->paused_remaining != expected_event->paused_remaining ||
            actual_event->value != expected_event->value ||
            actual_event->source != expected_event->source ||
            actual_event->type != expected_event->type ||
            actual_event->paused != expected_event->paused) {
            return false;
        }
    }
    return true;
}

static bool direct_file_io_states_match(const Dspic33* actual,
                                        const Dspic33* expected) {
    static Dspic33Io actual_io;
    static Dspic33Io expected_io;

    memcpy(&actual_io, &actual->io, sizeof(actual_io));
    memcpy(&expected_io, &expected->io, sizeof(expected_io));
    actual_io.cpu_write_cycle = expected_io.cpu_write_cycle;
    actual_io.cpu_write_instruction = expected_io.cpu_write_instruction;
    actual_io.cpu_write_address = expected_io.cpu_write_address;
    actual_io.cpu_write_previous = expected_io.cpu_write_previous;
    actual_io.cpu_write_width = expected_io.cpu_write_width;
    actual_io.cpu_write_valid = expected_io.cpu_write_valid;
    actual_io.cpu_write_rmw = expected_io.cpu_write_rmw;
    actual_io.cpu_read_address = expected_io.cpu_read_address;
    actual_io.cpu_read_width = expected_io.cpu_read_width;
    actual_io.cpu_read_valid = expected_io.cpu_read_valid;
    return memcmp(&actual_io, &expected_io, sizeof(actual_io)) == 0;
}

static bool direct_file_states_match(const Dspic33* actual, const Dspic33* expected) {
    static Dspic33 actual_state;
    static Dspic33 expected_state;

    memcpy(&actual_state, actual, sizeof(actual_state));
    memcpy(&expected_state, expected, sizeof(expected_state));

    actual_state.program = NULL;
    actual_state.auxiliary_program = NULL;
    actual_state.persistent_program = NULL;
    actual_state.data = NULL;
    actual_state.var_write_domains = NULL;
    actual_state.events.items = NULL;
    actual_state.events.capacity = 0u;
    expected_state.program = NULL;
    expected_state.auxiliary_program = NULL;
    expected_state.persistent_program = NULL;
    expected_state.data = NULL;
    expected_state.var_write_domains = NULL;
    expected_state.events.items = NULL;
    expected_state.events.capacity = 0u;
    return memcmp(&actual_state, &expected_state, sizeof(actual_state)) == 0 &&
           memcmp(actual->data, expected->data, 0x2000u) == 0 &&
           direct_file_event_queues_match(actual, expected);
}

static uint16_t run_direct_file_reference(Dspic33* cpu, DirectFileOperation operation,
                                          uint16_t address, bool byte_mode,
                                          bool file_destination) {
    uint64_t device_ratio = dspic33_device_instruction_cycles(cpu, 1u);
    bool reads_source = direct_file_reads_source(operation);
    bool writes_result = direct_file_writes_result(operation);
    bool non_cpu_sfr = reads_source &&
                       (address & (byte_mode ? 0xffffu : 0xfffeu)) >= 0x005au &&
                       address < 0x1000u && direct_file_address_implemented(address);
    uint16_t initial_status = cpu->sr;
    uint16_t right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
    uint16_t left = 0u;
    uint16_t value;
    uint16_t status;
    uint8_t instruction_cycles = non_cpu_sfr ? 2u : 1u;
    size_t index;

    cpu->pc = 2u;
    cpu->instructions++;
    cpu->non_cpu_sfr_read = non_cpu_sfr;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    cpu->instruction_working_register_writes = 0u;
    cpu->instruction_source_address_registers = 0u;
    cpu->current_instruction_cycles = 1u;
    cpu->current_instruction_pc = 0u;
    cpu->instruction_active = true;
    if (reads_source) {
        left = byte_mode ? dspic33_read_byte(cpu, address)
                         : dspic33_read_word(cpu, address);
    }
    value = direct_file_result(operation, left, right, initial_status, byte_mode);
    status = direct_file_status(operation, left, right, initial_status, byte_mode);
    cpu->sr = status;
    if (writes_result) {
        if (file_destination) {
            if (byte_mode) {
                dspic33_write_byte(cpu, address, (uint8_t)value);
            } else {
                dspic33_write_word(cpu, address, value);
            }
        } else {
            write_direct_file_reference_result(cpu, value, byte_mode);
        }
    }
    cpu->instruction_active = false;
    cpu->previous_working_register_writes = cpu->instruction_working_register_writes;
    cpu->current_instruction_cycles = 0u;
    cpu->non_cpu_sfr_read = false;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    cpu->instruction_advancing = true;
    if (non_cpu_sfr) {
        dspic33_device_advance_instruction(cpu, 1u, device_ratio);
        dspic33_device_advance_instruction(cpu, 1u, device_ratio);
    } else {
        dspic33_device_advance_instruction(cpu, 1u, device_ratio);
    }
    for (index = 0u; index < 4u; index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay != 0u) {
            pending->delay = pending->delay > instruction_cycles
                                 ? (uint8_t)(pending->delay - instruction_cycles)
                                 : 0u;
        }
    }
    cpu->instruction_advancing = false;
    cpu->io.cpu_bus_cycle = UINT64_MAX;
    cpu->io.cpu_write_rmw = false;
    return value;
}

static const Dspic33PendingSoftTrap* direct_file_pending_trap(const Dspic33* cpu) {
    const Dspic33PendingSoftTrap* selected = NULL;
    uint8_t current_priority = (uint8_t)(((cpu->corcon & 0x0008u) != 0u ? 8u : 0u) |
                                         ((cpu->sr >> 5u) & 0x07u));
    size_t index;

    for (index = 0u; index < 4u; index++) {
        const Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay == 0u &&
            pending->priority > current_priority &&
            (selected == NULL || pending->priority > selected->priority)) {
            selected = pending;
        }
    }
    return selected;
}

static bool direct_file_trap_register_state_matches(const Dspic33* actual,
                                                    const Dspic33* expected) {
    const Dspic33PendingSoftTrap* pending = direct_file_pending_trap(expected);
    uint16_t stacked_high;
    uint16_t final_status;
    uint16_t final_control;

    if (pending == NULL) {
        return direct_file_states_match(actual, expected);
    }
    stacked_high = (uint16_t)(((expected->sr & 0x00ffu) << 8u) |
                              ((expected->corcon & 0x0008u) != 0u ? 0x0080u : 0u));
    final_status =
        (uint16_t)((expected->sr & ~0x00e0u) | ((pending->priority & 7u) << 5u));
    final_status &= (uint16_t)~0x0010u;
    final_control = pending->priority > 7u
                        ? (uint16_t)((expected->corcon & ~0x0004u) | 0x0008u)
                        : (uint16_t)(expected->corcon & ~(uint16_t)0x000cu);
    return actual->stop_reason == DSPIC33_TRAPPED &&
           actual->last_trap == pending->trap && actual->last_trap_return == 2u &&
           actual->pc == 0x000340u && actual->w[15] == 0x5004u &&
           actual->sr == final_status && actual->corcon == final_control &&
           actual->trap_count == 1u && actual->interrupt_depth == 1u &&
           memcmp(actual->w, expected->w, 15u * sizeof(*actual->w)) == 0 &&
           memcmp(actual->data, expected->data, 0x08c8u) == 0 &&
           memcmp(actual->data + 0x08cau, expected->data + 0x08cau,
                  0x2000u - 0x08cau) == 0 &&
           (uint16_t)(actual->data[0x08c8u] |
                      ((uint16_t)actual->data[0x08c9u] << 8u)) ==
               (uint16_t)(((uint16_t)pending->priority << 8u) | pending->trap) &&
           (uint16_t)(actual->data[0x5000u] |
                      ((uint16_t)actual->data[0x5001u] << 8u)) == 2u &&
           (uint16_t)(actual->data[0x5002u] |
                      ((uint16_t)actual->data[0x5003u] << 8u)) == stacked_high &&
           direct_file_io_states_match(actual, expected) &&
           direct_file_event_queues_match(actual, expected);
}

static bool run_direct_file_odd_word_case(Dspic33* cpu, Dspic33* reference,
                                          uint32_t opcode,
                                          DirectFileOperation operation,
                                          uint16_t address, bool file_destination) {
    uint16_t initial_status = reference->sr;
    uint16_t right = reference->w[0];
    bool reads_source = direct_file_reads_source(operation);
    uint16_t left = 0u;
    uint16_t value;
    uint16_t status;
    bool matches;
    bool writes_result = direct_file_writes_result(operation);
    uint64_t device_ratio = dspic33_device_instruction_cycles(reference, 1u);
    bool non_cpu_sfr = reads_source && address >= 0x005bu && address < 0x1000u &&
                       direct_file_address_implemented(address);
    uint64_t expected_cycles = reads_source && address >= 0x005bu &&
                                       address < 0x1000u &&
                                       direct_file_address_implemented(address)
                                   ? 2u
                                   : 1u;
    reference->pc = 2u;
    reference->instructions++;
    reference->non_cpu_sfr_read = non_cpu_sfr;
    reference->psv_read = false;
    reference->psv_repeat_optimized = false;
    reference->instruction_working_register_writes = 0u;
    reference->instruction_source_address_registers = 0u;
    reference->current_instruction_cycles = 1u;
    reference->current_instruction_pc = 0u;
    reference->instruction_active = true;
    if (reads_source) {
        left = dspic33_read_word(reference, address & 0xfffeu);
    }
    value = direct_file_result(operation, left, right, initial_status, false);
    status = direct_file_status(operation, left, right, initial_status, false);
    reference->sr = status;
    if (!file_destination && writes_result) {
        write_direct_file_reference_result(reference, value, false);
    }
    reference->instruction_active = false;
    reference->previous_working_register_writes =
        reference->instruction_working_register_writes;
    reference->current_instruction_cycles = 0u;
    reference->non_cpu_sfr_read = false;
    reference->psv_read = false;
    reference->psv_repeat_optimized = false;
    reference->instruction_advancing = true;
    if (non_cpu_sfr) {
        dspic33_device_advance_instruction(reference, 1u, device_ratio);
        dspic33_device_advance_instruction(reference, 1u, device_ratio);
    } else {
        dspic33_device_advance_instruction(reference, 1u, device_ratio);
    }
    reference->instruction_advancing = false;
    reference->io.cpu_bus_cycle = UINT64_MAX;
    reference->io.cpu_write_rmw = false;
    matches =
        dspic33_load_program_word(cpu, 0u, opcode) &&
        dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->last_trap == 1u &&
        cpu->last_trap_return == 2u && cpu->pc == 0x000340u &&
        cpu->cycles == expected_cycles && cpu->w[15] == 0x5004u &&
        (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
        (dspic33_read_word(cpu, 0x5002u) >> 8u) == (status & 0x00ffu) &&
        (uint16_t)(cpu->data[0x08c8u] | ((uint16_t)cpu->data[0x08c9u] << 8u)) ==
            0x0e01u &&
        memcmp(cpu->w, reference->w, 15u * sizeof(*cpu->w)) == 0 &&
        memcmp(cpu->data, reference->data, 0x08c0u) == 0 &&
        memcmp(cpu->data + 0x08c2u, reference->data + 0x08c2u, 0x08c8u - 0x08c2u) ==
            0 &&
        memcmp(cpu->data + 0x08cau, reference->data + 0x08cau, 0x2000u - 0x08cau) ==
            0 &&
        memcmp(&cpu->nvm, &reference->nvm, sizeof(cpu->nvm)) == 0 &&
        memcmp(&cpu->oscillator, &reference->oscillator, sizeof(cpu->oscillator)) ==
            0 &&
        memcmp(&cpu->watchdog, &reference->watchdog, sizeof(cpu->watchdog)) == 0 &&
        direct_file_io_states_match(cpu, reference) &&
        direct_file_event_queues_match(cpu, reference);

    if (file_destination || !writes_result) {
        matches = matches && cpu->w[0] == right;
    } else {
        matches = matches && cpu->w[0] == value;
    }
    return matches;
}

static bool load_direct_file_trap_vectors(Dspic33* cpu) {
    static const uint32_t vectors[] = {0x000004u, 0x000006u, 0x000008u,
                                       0x00000au, 0x00000eu, 0x000010u};
    size_t index;

    for (index = 0u; index < sizeof(vectors) / sizeof(*vectors); index++) {
        if (!dspic33_load_program_word(cpu, vectors[index], 0x000340u)) {
            return false;
        }
    }
    return true;
}

static bool direct_file_flag_outcomes_complete(const bool observed[512],
                                               DirectFileOperation operation,
                                               bool byte_mode) {
    bool expected[512] = {false};
    uint32_t maximum = byte_mode ? UINT8_MAX : UINT16_MAX;
    uint32_t value;
    uint16_t status;

    for (value = 0u; value <= maximum; value++) {
        uint8_t initial;
        for (initial = 0u; initial < 4u; initial++) {
            if (operation >= DIRECT_FILE_AND && operation <= DIRECT_FILE_IOR) {
                status = direct_file_logic_status(initial, (uint16_t)value, byte_mode);
            } else {
                status = direct_file_status(operation, (uint16_t)value, 0u, initial,
                                            byte_mode);
            }
            expected[status & 0x01ffu] = true;
        }
    }
    for (status = 0u; status < 512u; status++) {
        if (expected[status] != observed[status]) {
            return false;
        }
    }
    return true;
}

static bool run_direct_file_case(Dspic33* actual, Dspic33* reference, uint32_t opcode,
                                 DirectFileOperation operation, uint16_t address,
                                 bool byte_mode, bool file_destination) {
    bool matches;

    prepare_direct_file_case(actual, address, byte_mode);
    prepare_direct_file_case(reference, address, byte_mode);
    if (!byte_mode && (address & 1u) != 0u &&
        (direct_file_reads_source(operation) || file_destination)) {
        return run_direct_file_odd_word_case(actual, reference, opcode, operation,
                                             address, file_destination);
    }
    matches = dspic33_load_program_word(actual, 0u, opcode) &&
              dspic33_load_program_word(reference, 0u, opcode);
    dspic33_step(actual);
    run_direct_file_reference(reference, operation, address, byte_mode,
                              file_destination);
    return matches && (file_destination && address >= 0x08c0u && address <= 0x08c7u
                           ? direct_file_trap_register_state_matches(actual, reference)
                           : direct_file_states_match(actual, reference));
}

static void direct_file_arithmetic_encoding_matrix_cases(TestState* state) {
    static const uint32_t bases[6] = {0xbd0000u, 0xbd8000u, 0xb40000u,
                                      0xb48000u, 0xb50000u, 0xb58000u};
    static Dspic33 actual;
    static Dspic33 reference;
    uint32_t cases = 0u;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);
    uint8_t operation;

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file arithmetic processors");
    if (!actual_initialized || !reference_initialized) {
        if (actual_initialized) {
            dspic33_destroy(&actual);
        }
        if (reference_initialized) {
            dspic33_destroy(&reference);
        }
        return;
    }
    expect(state,
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file arithmetic address-error vectors");
    for (operation = 0u; operation < 6u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint8_t file_destination;
            for (file_destination = 0u; file_destination < 2u; file_destination++) {
                uint16_t address;
                for (address = 0u; address < 0x2000u; address++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, (DirectFileOperation)operation,
                        address, byte_mode != 0u, file_destination != 0u);
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file arithmetic encoding");
                    cases++;
                }
            }
        }
    }
    expect(state, cases == 196608u,
           "direct-file arithmetic encoding matrix is exhaustive");
    dspic33_destroy(&actual);
    dspic33_destroy(&reference);
}

static void direct_file_logical_encoding_matrix_cases(TestState* state,
                                                      Dspic33* invalid_cpu) {
    static const uint32_t bases[3] = {0xb60000u, 0xb68000u, 0xb70000u};
    static const DirectFileOperation operations[3] = {DIRECT_FILE_AND, DIRECT_FILE_XOR,
                                                      DIRECT_FILE_IOR};
    static Dspic33 actual;
    static Dspic33 reference;
    bool flag_outcomes[3][2][512] = {{{false}}};
    uint32_t valid = 0u;
    uint32_t invalid = 0u;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);
    uint8_t operation;

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file logical processors");
    if (!actual_initialized || !reference_initialized) {
        if (actual_initialized) {
            dspic33_destroy(&actual);
        }
        if (reference_initialized) {
            dspic33_destroy(&reference);
        }
        return;
    }
    expect(state,
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file logical address-error vectors");
    for (operation = 0u; operation < 3u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint8_t file_destination;
            for (file_destination = 0u; file_destination < 2u; file_destination++) {
                uint16_t address;
                for (address = 0u; address < 0x2000u; address++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);

                    if (address >= 0x1000u) {
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] =
                            true;
                    }
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file logical encoding");
                    valid++;
                }
            }
        }
    }
    for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (uint16_t address = 0u; address < 0x2000u; address++) {
            run_invalid_binary_matrix_case(
                state, invalid_cpu, 0xb78000u | ((uint32_t)byte_mode << 14u) | address);
            invalid++;
        }
    }
    expect(state, valid == 98304u,
           "direct-file logical valid encoding matrix is exhaustive");
    expect(state, invalid == 16384u,
           "direct-file logical illegal encoding matrix is exhaustive");
    for (operation = 0u; operation < 3u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            expect(state,
                   direct_file_flag_outcomes_complete(
                       flag_outcomes[operation][byte_mode], operations[operation],
                       byte_mode != 0u),
                   "direct-file logical flag outcomes are exhaustive");
        }
    }
    dspic33_destroy(&actual);
    dspic33_destroy(&reference);
}

static void direct_file_unary_encoding_matrix_cases(TestState* state) {
    static const uint32_t bases[8] = {0xec0000u, 0xec8000u, 0xed0000u, 0xed8000u,
                                      0xee0000u, 0xee8000u, 0xef0000u, 0xef8000u};
    static const DirectFileOperation operations[8] = {
        DIRECT_FILE_INC, DIRECT_FILE_INC2, DIRECT_FILE_DEC, DIRECT_FILE_DEC2,
        DIRECT_FILE_NEG, DIRECT_FILE_COM,  DIRECT_FILE_CLR, DIRECT_FILE_SETM};
    static Dspic33 actual;
    static Dspic33 reference;
    bool flag_outcomes[8][2][512] = {{{false}}};
    uint32_t cases = 0u;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);
    uint8_t operation;

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file unary processors");
    if (!actual_initialized || !reference_initialized) {
        if (actual_initialized) {
            dspic33_destroy(&actual);
        }
        if (reference_initialized) {
            dspic33_destroy(&reference);
        }
        return;
    }
    expect(state,
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file unary address-error vectors");
    for (operation = 0u; operation < 8u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint8_t file_destination;
            for (file_destination = 0u; file_destination < 2u; file_destination++) {
                uint16_t address;
                for (address = 0u; address < 0x2000u; address++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);

                    if (address >= 0x1000u) {
                        flag_outcomes[operation][byte_mode][reference.sr & 0x01ffu] =
                            true;
                    }
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file unary encoding");
                    cases++;
                }
            }
        }
    }
    expect(state, cases == 262144u, "direct-file unary encoding matrix is exhaustive");
    for (operation = 0u; operation < 8u; operation++) {
        uint8_t byte_mode;
        for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            expect(state,
                   direct_file_flag_outcomes_complete(
                       flag_outcomes[operation][byte_mode], operations[operation],
                       byte_mode != 0u),
                   "direct-file unary flag outcomes are exhaustive");
        }
    }
    dspic33_destroy(&actual);
    dspic33_destroy(&reference);
}

static DirectFileOperation shift_matrix_operation(uint8_t family, bool alternate) {
    if (family == 0u) {
        return DIRECT_FILE_SL;
    }
    if (family == 1u) {
        return alternate ? DIRECT_FILE_ASR : DIRECT_FILE_LSR;
    }
    if (family == 2u) {
        return alternate ? DIRECT_FILE_RLC : DIRECT_FILE_RLNC;
    }
    return alternate ? DIRECT_FILE_RRC : DIRECT_FILE_RRNC;
}

static void single_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal = 0u;
    uint32_t invalid = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t fields = 0u; fields < 0x040000u; fields++) {
        uint32_t opcode = 0xd00000u | fields;
        uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
        bool alternate = (opcode & 0x008000u) != 0u;
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        bool valid =
            (family != 0u || !alternate) && destination_mode < 6u && source_mode < 6u;

        if (valid) {
            run_legal_unary_matrix_case(state, cpu, opcode,
                                        shift_matrix_operation(family, alternate));
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 129024u, "single-shift legal encoding matrix is exhaustive");
    expect(state, invalid == 133120u,
           "single-shift illegal encoding matrix is exhaustive");
}

static void direct_file_shift_encoding_matrix_cases(TestState* state,
                                                    Dspic33* invalid_cpu) {
    static const uint32_t bases[7] = {0xd40000u, 0xd50000u, 0xd58000u, 0xd60000u,
                                      0xd68000u, 0xd70000u, 0xd78000u};
    static const DirectFileOperation operations[7] = {
        DIRECT_FILE_SL,  DIRECT_FILE_LSR,  DIRECT_FILE_ASR, DIRECT_FILE_RLNC,
        DIRECT_FILE_RLC, DIRECT_FILE_RRNC, DIRECT_FILE_RRC};
    static Dspic33 actual;
    static Dspic33 reference;
    uint32_t legal = 0u;
    uint32_t invalid = 0u;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file shift processors");
    if (!actual_initialized || !reference_initialized) {
        if (actual_initialized) {
            dspic33_destroy(&actual);
        }
        if (reference_initialized) {
            dspic33_destroy(&reference);
        }
        return;
    }
    expect(state,
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file shift address-error vectors");
    for (uint8_t operation = 0u; operation < 7u; operation++) {
        for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            for (uint8_t file_destination = 0u; file_destination < 2u;
                 file_destination++) {
                for (uint16_t address = 0u; address < 0x2000u; address++) {
                    uint32_t opcode = bases[operation] | ((uint32_t)byte_mode << 14u) |
                                      ((uint32_t)file_destination << 13u) | address;
                    bool matches = run_direct_file_case(
                        &actual, &reference, opcode, operations[operation], address,
                        byte_mode != 0u, file_destination != 0u);
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "direct-file shift encoding");
                    legal++;
                }
            }
        }
    }
    for (uint32_t opcode = 0xd48000u; opcode < 0xd50000u; opcode++) {
        run_invalid_binary_matrix_case(state, invalid_cpu, opcode);
        invalid++;
    }
    expect(state, legal == 229376u,
           "direct-file shift legal encoding matrix is exhaustive");
    expect(state, invalid == 32768u,
           "direct-file shift illegal encoding matrix is exhaustive");
    dspic33_destroy(&actual);
    dspic33_destroy(&reference);
}

static uint16_t multiple_shift_result(DirectFileOperation operation, uint16_t source,
                                      uint16_t amount) {
    if (amount >= 16u) {
        return operation == DIRECT_FILE_ASR && (source & 0x8000u) != 0u ? 0xffffu : 0u;
    }
    if (operation == DIRECT_FILE_SL) {
        return (uint16_t)(source << amount);
    }
    if (operation == DIRECT_FILE_ASR) {
        return (uint16_t)((int16_t)source >> amount);
    }
    return (uint16_t)(source >> amount);
}

static void run_multiple_shift_matrix_case(TestState* state, Dspic33* cpu,
                                           uint32_t opcode,
                                           DirectFileOperation operation) {
    uint8_t source = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool literal = (opcode & 0x0040u) != 0u;
    uint16_t source_value = (uint16_t)(0x8001u ^ (opcode * 0x45d9u));
    uint16_t count = literal ? (uint16_t)(opcode & 0x0fu)
                             : (uint16_t)(0xa500u | ((opcode >> 7u) & 0x001fu));
    uint16_t amount = literal ? count : (uint16_t)(count & 0x001fu);
    uint16_t initial_status =
        (uint16_t)(0x0105u | ((opcode & 1u) << 1u) | (((opcode >> 7u) & 1u) << 3u));
    uint16_t expected;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, cpu->w, initial_status);
    dspic33_set_working_register(cpu, source, source_value);
    if (!literal) {
        cpu->w[opcode & 0x0fu] = count;
    }
    source_value = cpu->w[source];
    expected = multiple_shift_result(operation, source_value, amount);
    expected_status = (uint16_t)(initial_status & ~0x000au);
    if (expected == 0u) {
        expected_status |= 0x0002u;
    }
    if ((expected & 0x8000u) != 0u) {
        expected_status |= 0x0008u;
    }
    cycles = cpu->cycles;
    matches =
        dspic33_load_program_word(cpu, 0u, opcode) &&
        dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
        cpu->cycles - cycles == 1u &&
        cpu->w[destination] == (destination == 15u ? (expected & 0xfffeu) : expected) &&
        cpu->sr == expected_status && !cpu->illegal_reset &&
        cpu->unsupported_opcode == 0u;
    expect_dsp_matrix_case(state, matches, opcode, "multiple-shift encoding");
}

static void multiple_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal = 0u;
    uint32_t invalid = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0xdd0000u; opcode < 0xdf0000u; opcode++) {
        bool left = (opcode & 0xff0000u) == 0xdd0000u;
        bool valid = (opcode & 0x0030u) == 0u && (!left || (opcode & 0x008000u) == 0u);
        if (valid) {
            DirectFileOperation operation = left                 ? DIRECT_FILE_SL
                                            : (opcode & 0x8000u) ? DIRECT_FILE_ASR
                                                                 : DIRECT_FILE_LSR;
            run_multiple_shift_matrix_case(state, cpu, opcode, operation);
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 24576u,
           "multiple-shift legal encoding matrix is exhaustive");
    expect(state, invalid == 106496u,
           "multiple-shift illegal encoding matrix is exhaustive");
}

static uint16_t find_first_result(uint16_t source, bool left, bool sign_change) {
    if (sign_change) {
        bool sign = (source & 0x8000u) != 0u;
        uint16_t shifted = (uint16_t)(source << 1u);
        uint8_t count = 0u;
        while (count < 15u && ((shifted & 0x8000u) != 0u) == sign) {
            shifted <<= 1u;
            count++;
        }
        return (uint16_t)(-(int16_t)count);
    }
    for (uint8_t bit = 0u; bit < 16u; bit++) {
        uint16_t mask = left ? (uint16_t)(0x8000u >> bit) : (uint16_t)(1u << bit);
        if ((source & mask) != 0u) {
            return (uint16_t)(bit + 1u);
        }
    }
    return 0u;
}

static void run_find_first_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                       bool sign_change) {
    bool left = (opcode & 0x008000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t registers[16];
    BinaryMatrixOperand source;
    uint16_t source_value = (uint16_t)(opcode * 0x45d9u);
    uint16_t operand;
    uint16_t expected;
    uint16_t initial_status = (uint16_t)(0x010eu | (opcode & 1u));
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    source = binary_matrix_operand(registers, source_mode, source_register, 2u);
    operand = source.direct ? registers[source_register] : source_value;
    if (!source.direct) {
        dspic33_write_word(cpu, source.address, source_value);
    }
    expected = find_first_result(operand, left, sign_change);
    expected_status = (uint16_t)((initial_status & ~1u) |
                                 (sign_change ? expected == 0xfff1u : expected == 0u));
    binary_matrix_write_register(registers, destination, expected);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              binary_matrix_registers_match(cpu, registers) && !cpu->illegal_reset &&
              cpu->unsupported_opcode == 0u;
    if (!source.direct && destination != source_register) {
        matches = matches && dspic33_read_word(cpu, source.address) == source_value;
    }
    expect_dsp_matrix_case(state, matches, opcode, "find-first encoding");
}

static void find_first_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal = 0u;
    uint32_t invalid = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0xcf0000u; opcode < 0xd00000u; opcode++) {
        bool valid = (opcode & 0x007800u) == 0u && ((opcode >> 4u) & 0x07u) < 6u;
        if (valid) {
            run_find_first_matrix_case(state, cpu, opcode, false);
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    for (uint32_t opcode = 0xdf0000u; opcode < 0xe00000u; opcode++) {
        bool valid = (opcode & 0x00f800u) == 0u && ((opcode >> 4u) & 0x07u) < 6u;
        if (valid) {
            run_find_first_matrix_case(state, cpu, opcode, true);
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 4608u, "find-first legal encoding matrix is exhaustive");
    expect(state, invalid == 126464u,
           "find-first illegal encoding matrix is exhaustive");
}

static int64_t accumulator_shift_matrix_result(int64_t value, int16_t amount) {
    if (amount < 0) {
        return value * ((int64_t)1 << -amount);
    }
    if (amount == 0 || value >= 0) {
        return value >> amount;
    }
    return -((-value + ((int64_t)1 << amount) - 1) >> amount);
}

static void run_accumulator_shift_matrix_case(TestState* state, Dspic33* cpu,
                                              uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    bool literal = (opcode & 0x0040u) != 0u;
    uint8_t encoded =
        literal ? (uint8_t)(opcode & 0x003fu) : (uint8_t)((opcode * 13u) & 0x003fu);
    int16_t amount = (int16_t)(encoded >= 32u ? encoded - 64u : encoded);
    int64_t initial = accumulator == 0u ? 0x0000012345 : -0x0000012345;
    int64_t expected = initial;
    uint64_t cycles;
    bool matches;

    cpu->pc = 0u;
    cpu->sr = 0u;
    cpu->corcon = 0x0020u;
    cpu->accumulator[0] = accumulator == 0u ? initial : 0x5555;
    cpu->accumulator[1] = accumulator == 1u ? initial : -0x5555;
    cpu->unsupported_opcode = 0u;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->events.count = 0u;
    memset(cpu->pending_soft_traps, 0, sizeof(cpu->pending_soft_traps));
    if (!literal) {
        cpu->w[opcode & 0x0fu] = (uint16_t)(0xa5c0u | encoded);
    }
    if (amount >= -16 && amount <= 16) {
        expected = accumulator_shift_matrix_result(initial, amount);
    }
    cycles = cpu->cycles;
    matches =
        dspic33_load_program_word(cpu, 0u, opcode) &&
        dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
        cpu->cycles - cycles == 1u && cpu->accumulator[accumulator] == expected &&
        cpu->accumulator[accumulator ^ 1u] == (accumulator == 0u ? -0x5555 : 0x5555) &&
        !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
    if (amount < -16 || amount > 16) {
        matches =
            matches && active_pending_traps(cpu) == 1u && pending_trap(cpu, 4u) != NULL;
    } else {
        matches = matches && active_pending_traps(cpu) == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "accumulator-shift encoding");
}

static void accumulator_shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal = 0u;
    uint32_t invalid = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0xc80000u; opcode < 0xc90000u; opcode++) {
        bool literal = (opcode & 0x0040u) != 0u;
        bool valid = (opcode & 0x7f00u) == 0u && (opcode & 0x0080u) == 0u &&
                     (literal || (opcode & 0x0030u) == 0u);
        if (valid) {
            run_accumulator_shift_matrix_case(state, cpu, opcode);
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 160u,
           "accumulator-shift legal encoding matrix is exhaustive");
    expect(state, invalid == 65376u,
           "accumulator-shift illegal encoding matrix is exhaustive");
}

static void single_shift_value_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t bases[7] = {0xd00000u, 0xd10000u, 0xd18000u, 0xd20000u,
                                      0xd28000u, 0xd30000u, 0xd38000u};
    static const DirectFileOperation operations[7] = {
        DIRECT_FILE_SL,  DIRECT_FILE_LSR,  DIRECT_FILE_ASR, DIRECT_FILE_RLNC,
        DIRECT_FILE_RLC, DIRECT_FILE_RRNC, DIRECT_FILE_RRC};
    uint32_t cases = 0u;

    for (uint8_t operation = 0u; operation < 7u; operation++) {
        for (uint8_t byte_mode = 0u; byte_mode < 2u; byte_mode++) {
            uint32_t maximum = byte_mode != 0u ? UINT8_MAX : UINT16_MAX;
            for (uint32_t source = 0u; source <= maximum; source++) {
                for (uint8_t carry = 0u; carry < 2u; carry++) {
                    uint32_t opcode =
                        bases[operation] | ((uint32_t)byte_mode << 14u) | 0x000182u;
                    uint16_t initial_status = (uint16_t)(0x0104u | carry);
                    uint16_t expected =
                        shift_matrix_result(operations[operation], (uint16_t)source,
                                            initial_status, byte_mode != 0u);
                    uint16_t expected_status =
                        shift_matrix_status(operations[operation], (uint16_t)source,
                                            initial_status, byte_mode != 0u);
                    bool matches;

                    cpu->pc = 0u;
                    cpu->sr = initial_status;
                    cpu->corcon = 0x0020u;
                    cpu->unsupported_opcode = 0u;
                    cpu->illegal_reset = false;
                    cpu->stop_reason = DSPIC33_RUNNING;
                    cpu->events.count = 0u;
                    cpu->w[2] = byte_mode != 0u ? (uint16_t)(0xa500u | (uint8_t)source)
                                                : (uint16_t)source;
                    cpu->w[3] = 0x5a5au;
                    if (byte_mode != 0u) {
                        expected |= 0x5a00u;
                    }
                    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                              cpu->w[3] == expected && cpu->sr == expected_status &&
                              !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
                    expect_dsp_matrix_case(state, matches, opcode,
                                           "single-shift value boundary");
                    cases++;
                }
            }
        }
    }
    expect(state, cases == 921088u,
           "single-shift value and carry matrix is exhaustive");
}

static void multiple_shift_value_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t opcodes[3] = {0xdd1184u, 0xde1184u, 0xde9184u};
    static const DirectFileOperation operations[3] = {DIRECT_FILE_SL, DIRECT_FILE_LSR,
                                                      DIRECT_FILE_ASR};
    uint32_t cases = 0u;

    for (uint8_t operation = 0u; operation < 3u; operation++) {
        for (uint32_t source = 0u; source <= UINT16_MAX; source++) {
            for (uint16_t amount = 0u; amount < 32u; amount++) {
                uint16_t expected = multiple_shift_result(operations[operation],
                                                          (uint16_t)source, amount);
                uint16_t initial_status = (uint16_t)(0x0105u | ((source & 1u) << 1u) |
                                                     (((source >> 15u) & 1u) << 3u));
                uint16_t expected_status = (uint16_t)(initial_status & ~0x000au);
                bool matches;

                if (expected == 0u) {
                    expected_status |= 0x0002u;
                }
                if ((expected & 0x8000u) != 0u) {
                    expected_status |= 0x0008u;
                }
                cpu->pc = 0u;
                cpu->sr = initial_status;
                cpu->corcon = 0x0020u;
                cpu->unsupported_opcode = 0u;
                cpu->illegal_reset = false;
                cpu->stop_reason = DSPIC33_RUNNING;
                cpu->events.count = 0u;
                cpu->w[2] = (uint16_t)source;
                cpu->w[3] = 0x5a5au;
                cpu->w[4] = (uint16_t)(0xa5c0u | amount);
                matches = dspic33_load_program_word(cpu, 0u, opcodes[operation]) &&
                          dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->w[3] == expected && cpu->sr == expected_status &&
                          !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
                expect_dsp_matrix_case(state, matches, opcodes[operation],
                                       "multiple-shift value boundary");
                cases++;
            }
        }
    }
    expect(state, cases == 6291456u,
           "multiple-shift value and count matrix is exhaustive");
}

static void find_first_value_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t opcodes[3] = {0xcf0182u, 0xcf8182u, 0xdf0182u};
    uint32_t cases = 0u;

    for (uint8_t operation = 0u; operation < 3u; operation++) {
        bool left = operation == 1u;
        bool sign_change = operation == 2u;
        for (uint32_t source = 0u; source <= UINT16_MAX; source++) {
            uint16_t expected = find_first_result((uint16_t)source, left, sign_change);
            uint16_t initial_status = (uint16_t)(0x010eu | (source & 1u));
            uint16_t expected_status =
                (uint16_t)((initial_status & ~1u) |
                           (sign_change ? expected == 0xfff1u : expected == 0u));
            bool matches;

            cpu->pc = 0u;
            cpu->sr = initial_status;
            cpu->corcon = 0x0020u;
            cpu->unsupported_opcode = 0u;
            cpu->illegal_reset = false;
            cpu->stop_reason = DSPIC33_RUNNING;
            cpu->events.count = 0u;
            cpu->w[2] = (uint16_t)source;
            cpu->w[3] = 0x5a5au;
            matches = dspic33_load_program_word(cpu, 0u, opcodes[operation]) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->w[3] == expected && cpu->sr == expected_status &&
                      !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
            expect_dsp_matrix_case(state, matches, opcodes[operation],
                                   "find-first value boundary");
            cases++;
        }
    }
    expect(state, cases == 196608u, "find-first value matrix is exhaustive");
}

static int64_t accumulator_matrix_value(int64_t value) {
    uint64_t bits = (uint64_t)value & 0xffffffffffu;
    return (int64_t)bits - ((bits & 0x8000000000u) != 0u ? 0x10000000000ll : 0ll);
}

static void accumulator_shift_boundary_cases(TestState* state, Dspic33* cpu) {
    static const int64_t values[] = {0,
                                     1,
                                     -1,
                                     INT32_MAX,
                                     INT32_MIN,
                                     (int64_t)INT32_MAX + 1,
                                     (int64_t)INT32_MIN - 1,
                                     0x7fffffffffll,
                                     -0x8000000000ll};
    static const int8_t amounts[] = {-16, -1, 0, 1, 16};
    uint32_t cases = 0u;

    for (uint8_t accumulator = 0u; accumulator < 2u; accumulator++) {
        for (uint8_t saturation = 0u; saturation < 2u; saturation++) {
            for (uint8_t accumulator_saturation = 0u; accumulator_saturation < 2u;
                 accumulator_saturation++) {
                for (size_t value_index = 0u;
                     value_index < sizeof(values) / sizeof(*values); value_index++) {
                    for (size_t amount_index = 0u;
                         amount_index < sizeof(amounts) / sizeof(*amounts);
                         amount_index++) {
                        int64_t result = accumulator_shift_matrix_result(
                            values[value_index], amounts[amount_index]);
                        int64_t minimum =
                            accumulator_saturation != 0u ? -0x8000000000ll : INT32_MIN;
                        int64_t maximum =
                            accumulator_saturation != 0u ? 0x7fffffffffll : INT32_MAX;
                        bool saturation_status =
                            result < -0x8000000000ll || result > 0x7fffffffffll;
                        uint16_t overflow_flag = accumulator == 0u ? 0x8000u : 0x4000u;
                        uint16_t saturation_flag =
                            accumulator == 0u ? 0x2000u : 0x1000u;
                        uint16_t expected_status = 0u;
                        uint16_t corcon =
                            (uint16_t)(0x0020u |
                                       (accumulator_saturation != 0u ? 0x0010u : 0u) |
                                       (saturation != 0u
                                            ? (accumulator == 0u ? 0x0080u : 0x0040u)
                                            : 0u));
                        uint8_t encoded = (uint8_t)(amounts[amount_index] & 0x3f);
                        uint32_t opcode =
                            0xc80040u | ((uint32_t)accumulator << 15u) | encoded;
                        bool matches;

                        if (saturation != 0u) {
                            if (result < minimum) {
                                result = minimum;
                                saturation_status = true;
                            } else if (result > maximum) {
                                result = maximum;
                                saturation_status = true;
                            }
                        }
                        result = accumulator_matrix_value(result);
                        if (result < INT32_MIN || result > INT32_MAX) {
                            expected_status |= overflow_flag | 0x0800u;
                        }
                        if (saturation_status) {
                            expected_status |= saturation_flag | 0x0400u;
                        }
                        cpu->pc = 0u;
                        cpu->sr = 0u;
                        cpu->corcon = corcon;
                        cpu->accumulator[accumulator] = values[value_index];
                        cpu->accumulator[accumulator ^ 1u] = 0x12345;
                        cpu->unsupported_opcode = 0u;
                        cpu->illegal_reset = false;
                        cpu->stop_reason = DSPIC33_RUNNING;
                        cpu->events.count = 0u;
                        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                                  dspic33_step(cpu) == DSPIC33_RUNNING &&
                                  cpu->pc == 2u &&
                                  cpu->accumulator[accumulator] == result &&
                                  cpu->accumulator[accumulator ^ 1u] == 0x12345 &&
                                  cpu->sr == expected_status && !cpu->illegal_reset &&
                                  cpu->unsupported_opcode == 0u;
                        expect_dsp_matrix_case(state, matches, opcode,
                                               "accumulator-shift boundary");
                        cases++;
                    }
                }
            }
        }
    }
    expect(state, cases == 360u, "accumulator-shift saturation matrix is exhaustive");
}

static void accumulator_shift_register_count_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;

    for (uint8_t accumulator = 0u; accumulator < 2u; accumulator++) {
        for (uint8_t encoded = 0u; encoded < 64u; encoded++) {
            int16_t amount = (int16_t)(encoded >= 32u ? encoded - 64u : encoded);
            int64_t initial = accumulator == 0u ? 0x12345 : -0x12345;
            int64_t expected = initial;
            uint32_t opcode = 0xc80002u | ((uint32_t)accumulator << 15u);
            bool matches;

            reset_processor_test(cpu, 0u);
            dspic33_set_async_events(cpu, false);
            cpu->accumulator[accumulator] = initial;
            cpu->accumulator[accumulator ^ 1u] = 0x5a5a;
            cpu->w[2] = (uint16_t)(0xa5c0u | encoded);
            if (amount >= -16 && amount <= 16) {
                expected = accumulator_shift_matrix_result(initial, amount);
            }
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->accumulator[accumulator] == expected &&
                      cpu->accumulator[accumulator ^ 1u] == 0x5a5a &&
                      !cpu->illegal_reset && cpu->unsupported_opcode == 0u;
            if (amount < -16 || amount > 16) {
                matches =
                    matches && (dspic33_read_word(cpu, 0x08c0u) & 0x0080u) != 0u &&
                    active_pending_traps(cpu) == 1u && pending_trap(cpu, 4u) != NULL;
            } else {
                matches = matches &&
                          (dspic33_read_word(cpu, 0x08c0u) & 0x0080u) == 0u &&
                          active_pending_traps(cpu) == 0u;
            }
            expect_dsp_matrix_case(state, matches, opcode,
                                   "accumulator-shift register count");
            cases++;
        }
    }
    expect(state, cases == 128u,
           "accumulator-shift register counts cover every low-six-bit value");
}

static void shift_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    single_shift_encoding_matrix_cases(state, cpu);
    direct_file_shift_encoding_matrix_cases(state, cpu);
    multiple_shift_encoding_matrix_cases(state, cpu);
    find_first_encoding_matrix_cases(state, cpu);
    accumulator_shift_encoding_matrix_cases(state, cpu);
    single_shift_value_matrix_cases(state, cpu);
    multiple_shift_value_matrix_cases(state, cpu);
    find_first_value_matrix_cases(state, cpu);
    accumulator_shift_boundary_cases(state, cpu);
    accumulator_shift_register_count_cases(state, cpu);
}

static void run_legal_compare_register_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode) {
    bool compare_zero = (opcode & 0xff0000u) == 0xe00000u;
    bool with_borrow = !compare_zero && (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x000400u) != 0u;
    uint8_t width = byte_mode ? 1u : 2u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t base_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    bool literal = !compare_zero && source_mode >= 6u;
    uint16_t initial_status =
        (uint16_t)((opcode & 1u) | (((opcode >> 7u) & 1u) << 1u) |
                   (((opcode >> 8u) & 1u) << 2u) | (((opcode >> 9u) & 1u) << 3u) |
                   (((opcode >> 10u) & 1u) << 8u));
    uint16_t registers[16];
    BinaryMatrixOperand source = {0u, true};
    uint16_t source_value = byte_mode ? (uint16_t)(0x0080u | (opcode & 0x007fu))
                                      : (uint16_t)(0x8000u | (opcode & 0x03ffu));
    uint16_t left;
    uint16_t right;
    uint16_t expected_status;
    uint64_t cycles;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    left = compare_zero ? 0u
                        : (byte_mode ? (uint8_t)registers[base_register]
                                     : registers[base_register]);
    if (literal) {
        right = (uint16_t)(((opcode >> 2u) & 0x00e0u) | (opcode & 0x001fu));
    } else {
        source = binary_matrix_operand(registers, source_mode, source_register, width);
        right = source.direct ? (byte_mode ? (uint8_t)registers[source_register]
                                           : registers[source_register])
                              : source_value;
    }
    if (compare_zero) {
        left = right;
        right = 0u;
    }
    if (!literal && !source.direct) {
        if (byte_mode) {
            dspic33_write_byte(cpu, source.address, (uint8_t)source_value);
        } else {
            dspic33_write_word(cpu, source.address, source_value);
        }
    }
    expected_status = binary_matrix_status(with_borrow ? ARITHMETIC_MATRIX_SUBB
                                                       : ARITHMETIC_MATRIX_SUB,
                                           left, right, initial_status, byte_mode);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u && cpu->sr == expected_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    if (!literal && !source.direct) {
        matches =
            matches &&
            (byte_mode ? dspic33_read_byte(cpu, source.address) == (uint8_t)source_value
                       : dspic33_read_word(cpu, source.address) == source_value);
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal compare encoding");
}

static void compare_register_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal = 0u;
    uint32_t invalid = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xe00000u; opcode < 0xe20000u; opcode++) {
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        bool compare_zero = (opcode & 0xff0000u) == 0xe00000u;
        bool valid = compare_zero ? (opcode & 0x00fb80u) == 0u && source_mode < 6u
                                  : source_mode >= 6u || (opcode & 0x000380u) == 0u;

        if (valid) {
            run_legal_compare_register_case(state, cpu, opcode);
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 22720u,
           "compare register legal encoding matrix is exhaustive");
    expect(state, invalid == 108352u,
           "compare register illegal encoding matrix is exhaustive");
}

static void compare_direct_file_encoding_matrix_cases(TestState* state,
                                                      Dspic33* invalid_cpu) {
    static Dspic33 actual;
    static Dspic33 reference;
    uint32_t legal = 0u;
    uint32_t invalid = 0u;
    uint32_t opcode;
    bool actual_initialized = dspic33_initialize(&actual);
    bool reference_initialized = dspic33_initialize(&reference);

    expect(state, actual_initialized && reference_initialized,
           "initialize direct-file compare processors");
    if (!actual_initialized || !reference_initialized) {
        if (actual_initialized) {
            dspic33_destroy(&actual);
        }
        if (reference_initialized) {
            dspic33_destroy(&reference);
        }
        return;
    }
    expect(state,
           load_direct_file_trap_vectors(&actual) &&
               load_direct_file_trap_vectors(&reference),
           "load direct-file compare address-error vectors");
    for (opcode = 0xe20000u; opcode < 0xe40000u; opcode++) {
        bool compare_zero = (opcode & 0xff0000u) == 0xe20000u;
        bool valid =
            compare_zero ? (opcode & 0x00a000u) == 0u : (opcode & 0x002000u) == 0u;

        if (valid) {
            uint16_t address = (uint16_t)(opcode & 0x1fffu);
            bool byte_mode = (opcode & 0x004000u) != 0u;
            DirectFileOperation operation = compare_zero ? DIRECT_FILE_CP0
                                            : (opcode & 0x008000u) != 0u
                                                ? DIRECT_FILE_CPB
                                                : DIRECT_FILE_CP;
            bool matches = run_direct_file_case(&actual, &reference, opcode, operation,
                                                address, byte_mode, false);
            expect_dsp_matrix_case(state, matches, opcode,
                                   "direct-file compare encoding");
            legal++;
        } else {
            run_invalid_binary_matrix_case(state, invalid_cpu, opcode);
            invalid++;
        }
    }
    expect(state, legal == 49152u,
           "direct-file compare legal encoding matrix is exhaustive");
    expect(state, invalid == 81920u,
           "direct-file compare illegal encoding matrix is exhaustive");
    dspic33_destroy(&actual);
    dspic33_destroy(&reference);
}

static bool compare_control_reference_taken(uint32_t opcode, uint16_t left,
                                            uint16_t right) {
    bool byte_mode = (opcode & 0x000400u) != 0u;
    int32_t signed_left;
    int32_t signed_right;

    if (byte_mode) {
        left &= 0x00ffu;
        right &= 0x00ffu;
    }
    if ((opcode & 0xff8000u) == 0xe78000u) {
        return left == right;
    }
    if ((opcode & 0xff8000u) == 0xe70000u) {
        return left != right;
    }
    signed_left = byte_mode ? (int8_t)left : (int16_t)left;
    signed_right = byte_mode ? (int8_t)right : (int16_t)right;
    return (opcode & 0xff8000u) == 0xe60000u ? signed_left > signed_right
                                             : signed_left < signed_right;
}

static int8_t compare_control_reference_displacement(uint32_t opcode) {
    uint8_t encoded = (uint8_t)((opcode >> 4u) & 0x3fu);
    return (int8_t)((encoded & 0x20u) != 0u ? encoded | 0xc0u : encoded);
}

static void compare_control_operands(uint32_t opcode, bool alternate, uint16_t* left,
                                     uint16_t* right) {
    bool byte_mode = (opcode & 0x000400u) != 0u;
    uint32_t kind = opcode & 0xff8000u;

    if (kind == 0xe78000u) {
        *left = alternate ? (byte_mode ? 0x12a5u : 0xa5a5u) : 0x0000u;
        *right = alternate ? (byte_mode ? 0x34a5u : 0xa5a5u) : 0x0001u;
    } else if (kind == 0xe70000u) {
        *left = alternate ? 0x0000u : (byte_mode ? 0x12a5u : 0xa5a5u);
        *right = alternate ? 0x0001u : (byte_mode ? 0x34a5u : 0xa5a5u);
    } else if (kind == 0xe60000u) {
        *left = alternate ? (byte_mode ? 0x127fu : 0x7fffu)
                          : (byte_mode ? 0x1280u : 0x8000u);
        *right = alternate ? (byte_mode ? 0x3480u : 0x8000u)
                           : (byte_mode ? 0x347fu : 0x7fffu);
    } else {
        *left = alternate ? (byte_mode ? 0x1280u : 0x8000u)
                          : (byte_mode ? 0x127fu : 0x7fffu);
        *right = alternate ? (byte_mode ? 0x347fu : 0x7fffu)
                           : (byte_mode ? 0x3480u : 0x8000u);
    }
}

static void run_compare_control_encoding_case(TestState* state, Dspic33* cpu,
                                              uint32_t opcode, bool alternate) {
    uint8_t left_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t right_register = (uint8_t)(opcode & 0x0fu);
    uint16_t initial_status = alternate ? 0x010fu : 0u;
    uint16_t registers[16];
    uint16_t left;
    uint16_t right;
    int8_t displacement = compare_control_reference_displacement(opcode);
    bool taken;
    uint32_t expected_pc;
    uint64_t expected_cycles;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_arithmetic_matrix_case(cpu, registers, initial_status);
    compare_control_operands(opcode, alternate, &left, &right);
    binary_matrix_write_register(registers, left_register, left);
    if (right_register != left_register) {
        binary_matrix_write_register(registers, right_register, right);
    }
    for (uint8_t reg = 0u; reg < 16u; reg++) {
        dspic33_set_working_register(cpu, reg, registers[reg]);
    }
    left = registers[left_register];
    right = registers[right_register];
    taken = compare_control_reference_taken(opcode, left, right);
    expected_pc =
        taken ? (displacement == 1
                     ? 0x2004u
                     : (uint32_t)((0x2002 + (int32_t)displacement * 2) & 0x007ffffe))
              : 0x2002u;
    expected_cycles = taken ? (displacement == 1 ? 2u : 5u) : 1u;
    cpu->pc = 0x2000u;
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x2000u, opcode) &&
              dspic33_load_program_word(cpu, 0x2002u, OPCODE_NOP) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == expected_cycles &&
              cpu->instructions - instructions == 1u && cpu->sr == initial_status &&
              cpu->corcon == 0x0020u && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX &&
              binary_matrix_registers_match(cpu, registers);
    expect_dsp_matrix_case(state, matches, opcode,
                           alternate ? "compare control alternate encoding"
                                     : "compare control primary encoding");
}

static void compare_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t encodings = 0u;
    uint32_t executions = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xe60000u; opcode < 0xe80000u; opcode++) {
        run_compare_control_encoding_case(state, cpu, opcode, false);
        run_compare_control_encoding_case(state, cpu, opcode, true);
        encodings++;
        executions += 2u;
    }
    expect(state, encodings == 131072u,
           "compare control encoding matrix is exhaustive");
    expect(state, executions == 262144u,
           "compare control outcome matrix is exhaustive");
}

static void compare_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    compare_register_encoding_matrix_cases(state, cpu);
    compare_direct_file_encoding_matrix_cases(state, cpu);
    compare_control_encoding_matrix_cases(state, cpu);
}

static bool status_branch_reference_taken(uint8_t condition, uint16_t status) {
    bool carry = (status & 0x0001u) != 0u;
    bool zero = (status & 0x0002u) != 0u;
    bool overflow = (status & 0x0004u) != 0u;
    bool negative = (status & 0x0008u) != 0u;
    switch (condition) {
    case 0u:
        return overflow;
    case 1u:
        return carry;
    case 2u:
        return zero;
    case 3u:
        return negative;
    case 4u:
        return zero || negative != overflow;
    case 5u:
        return negative != overflow;
    case 6u:
        return !carry || zero;
    case 7u:
        return true;
    case 8u:
        return !overflow;
    case 9u:
        return !carry;
    case 10u:
        return !zero;
    case 11u:
        return !negative;
    case 12u:
        return !zero && negative == overflow;
    case 13u:
        return negative == overflow;
    default:
        return carry && !zero;
    }
}

static uint16_t accumulator_branch_status(uint8_t flags) {
    uint16_t status = (uint16_t)(((uint16_t)(flags & 0x01u) << 15u) |
                                 ((uint16_t)(flags & 0x02u) << 13u) |
                                 ((uint16_t)(flags & 0x04u) << 11u) |
                                 ((uint16_t)(flags & 0x08u) << 9u));
    if ((status & 0xc000u) != 0u) {
        status |= 0x0800u;
    }
    if ((status & 0x3000u) != 0u) {
        status |= 0x0400u;
    }
    return (uint16_t)(status | 0x010fu);
}

static void prepare_relative_branch_case(Dspic33* cpu, uint16_t status) {
    cpu->pc = 0x020000u;
    cpu->sr = status;
    cpu->corcon = 0x0020u;
    cpu->w[0] = 0x1357u;
    cpu->w[15] = 0x5000u;
    cpu->initialized_working_registers = 0x8001u;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
}

static void run_relative_branch_encoding_case(TestState* state, Dspic33* cpu,
                                              uint32_t opcode, uint16_t status,
                                              bool taken) {
    int16_t displacement = (int16_t)opcode;
    uint32_t expected_pc =
        taken ? (uint32_t)((0x020002 + (int32_t)displacement * 2) & 0x007ffffe)
              : 0x020002u;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_relative_branch_case(cpu, status);
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == expected_pc &&
              cpu->cycles - cycles == (taken ? 4u : 1u) &&
              cpu->instructions - instructions == 1u && cpu->sr == status &&
              cpu->corcon == 0x0020u && cpu->w[0] == 0x1357u && cpu->w[15] == 0x5000u &&
              cpu->initialized_working_registers == 0x8001u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "conditional branch encoding");
}

static void conditional_branch_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    bool outcomes[19][2] = {{false}};
    uint32_t valid_encodings = 0u;
    uint32_t invalid_encodings = 0u;
    uint32_t executions = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x0c0000u; opcode < 0x100000u; opcode++) {
        uint8_t family = (uint8_t)(opcode >> 16u);
        uint8_t flags;
        for (flags = 0u; flags < 16u; flags++) {
            uint16_t status = accumulator_branch_status(flags);
            bool taken = (status & (uint16_t)(0x8000u >> (family - 0x0cu))) != 0u;
            run_relative_branch_encoding_case(state, cpu, opcode, status, taken);
            outcomes[15u + family - 0x0cu][taken ? 1u : 0u] = true;
            executions++;
        }
        valid_encodings++;
    }
    for (opcode = 0x300000u; opcode < 0x3f0000u; opcode++) {
        uint8_t condition = (uint8_t)((opcode >> 16u) & 0x0fu);
        uint8_t flags;
        for (flags = 0u; flags < 16u; flags++) {
            uint16_t status = (uint16_t)(0x0100u | flags);
            bool taken = status_branch_reference_taken(condition, status);
            run_relative_branch_encoding_case(state, cpu, opcode, status, taken);
            outcomes[condition][taken ? 1u : 0u] = true;
            executions++;
        }
        valid_encodings++;
    }
    for (opcode = 0x3f0000u; opcode < 0x400000u; opcode++) {
        run_invalid_binary_matrix_case(state, cpu, opcode);
        invalid_encodings++;
    }
    expect(state, valid_encodings == 1245184u,
           "conditional branch valid encoding matrix is exhaustive");
    expect(state, invalid_encodings == 65536u,
           "conditional branch reserved encoding matrix is exhaustive");
    expect(state, executions == 19922944u,
           "conditional branch flag outcome matrix is exhaustive");
    for (uint8_t family = 0u; family < 19u; family++) {
        bool complete = outcomes[family][1u] && (family == 7u || outcomes[family][0u]);
        expect(state, complete, "conditional branch outcomes cover each family");
    }
}

typedef enum {
    COMPUTED_CONTROL_INVALID,
    COMPUTED_CONTROL_CALL,
    COMPUTED_CONTROL_RCALL,
    COMPUTED_CONTROL_GOTO,
    COMPUTED_CONTROL_BRA,
    COMPUTED_CONTROL_CALL_LONG,
    COMPUTED_CONTROL_GOTO_LONG,
} ComputedControlKind;

static ComputedControlKind computed_control_reference_kind(uint32_t opcode,
                                                           uint8_t* source) {
    if ((opcode & 0xfffff0u) == 0x010000u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_CALL;
    }
    if ((opcode & 0xfffff0u) == 0x010200u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_RCALL;
    }
    if ((opcode & 0xfffff0u) == 0x010400u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_GOTO;
    }
    if ((opcode & 0xfffff0u) == 0x010600u) {
        *source = (uint8_t)opcode & 0x0fu;
        return COMPUTED_CONTROL_BRA;
    }
    *source = (uint8_t)opcode & 0x0fu;
    if ((*source & 1u) == 0u && *source <= 12u) {
        uint32_t base = 0x018000u | ((uint32_t)(*source + 1u) << 11u) | *source;
        if (opcode == base) {
            return COMPUTED_CONTROL_CALL_LONG;
        }
        if (opcode == (base | 0x000400u)) {
            return COMPUTED_CONTROL_GOTO_LONG;
        }
    }
    return COMPUTED_CONTROL_INVALID;
}

static void run_computed_control_encoding_case(TestState* state, Dspic33* cpu,
                                               uint32_t opcode,
                                               ComputedControlKind kind,
                                               uint8_t source) {
    bool call = kind == COMPUTED_CONTROL_CALL || kind == COMPUTED_CONTROL_RCALL ||
                kind == COMPUTED_CONTROL_CALL_LONG;
    uint16_t initial_registers[16];
    uint32_t target;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    cpu->pc = 0x002000u;
    cpu->sr = 0xf10fu;
    cpu->corcon = 0x0024u;
    cpu->call_depth = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        uint16_t value =
            (kind == COMPUTED_CONTROL_RCALL || kind == COMPUTED_CONTROL_BRA)
                ? (reg < 8u ? (uint16_t)(0x0010u + reg) : (uint16_t)(0xffe0u + reg))
                : (uint16_t)(0x3001u + (uint16_t)reg * 2u);
        dspic33_set_working_register(cpu, reg, value);
        initial_registers[reg] = value;
    }
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    initial_registers[15] = 0x5000u;
    if (kind == COMPUTED_CONTROL_CALL_LONG || kind == COMPUTED_CONTROL_GOTO_LONG) {
        dspic33_set_working_register(cpu, source, (uint16_t)(0x3001u + source * 2u));
        dspic33_set_working_register(cpu, (uint8_t)(source + 1u), 0u);
        initial_registers[source] = cpu->w[source];
        initial_registers[source + 1u] = 0u;
    }
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    if (kind == COMPUTED_CONTROL_RCALL || kind == COMPUTED_CONTROL_BRA) {
        uint16_t displacement = source == 15u && kind == COMPUTED_CONTROL_RCALL
                                    ? 0x5004u
                                    : initial_registers[source];
        target =
            (uint32_t)((0x002002 + (int32_t)(int16_t)displacement * 2) & 0x007ffffe);
    } else if (kind == COMPUTED_CONTROL_CALL_LONG ||
               kind == COMPUTED_CONTROL_GOTO_LONG) {
        target = initial_registers[source] & 0xfffeu;
    } else {
        target =
            (source == 15u && call ? 0x5004u : initial_registers[source]) & 0xfffeu;
    }
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x002000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == target &&
              cpu->cycles - cycles == 4u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0xf10fu && cpu->corcon == (call ? 0x0020u : 0x0024u) &&
              cpu->w[15] == (call ? 0x5004u : 0x5000u) &&
              cpu->call_depth == (call ? 1u : 0u) && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX;
    if (call) {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0x2003u &&
                  dspic33_read_word(cpu, 0x5002u) == 0u;
    } else {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
                  dspic33_read_word(cpu, 0x5002u) == 0x5a5au;
    }
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == initial_registers[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "computed control encoding");
}

static void computed_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t valid = 0u;
    uint32_t invalid = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x010000u; opcode < 0x020000u; opcode++) {
        uint8_t source;
        ComputedControlKind kind = computed_control_reference_kind(opcode, &source);
        if (kind == COMPUTED_CONTROL_INVALID) {
            run_invalid_binary_matrix_case(state, cpu, opcode);
            invalid++;
        } else {
            run_computed_control_encoding_case(state, cpu, opcode, kind, source);
            valid++;
        }
    }
    expect(state, valid == 78u, "computed control valid encoding matrix is exhaustive");
    expect(state, invalid == 65458u,
           "computed control reserved encoding matrix is exhaustive");
}

static void prepare_literal_control_encoding_case(Dspic33* cpu) {
    cpu->pc = 0x020000u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0024u;
    cpu->call_depth = 0u;
    cpu->w[15] = 0x5000u;
    cpu->initialized_working_registers = 0x8000u;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    cpu->stop_on_trap = false;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
}

static void run_literal_control_encoding_case(TestState* state, Dspic33* cpu, bool call,
                                              uint16_t low, uint8_t high) {
    uint32_t opcode = (call ? 0x020000u : 0x040000u) | low;
    uint32_t target = (((uint32_t)high << 16u) | low) & 0x007ffffeu;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_literal_control_encoding_case(cpu);
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_load_program_word(cpu, 0x020002u, high) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == target &&
              cpu->cycles - cycles == 4u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0x010fu && cpu->corcon == (call ? 0x0020u : 0x0024u) &&
              cpu->w[15] == (call ? 0x5004u : 0x5000u) &&
              cpu->call_depth == (call ? 1u : 0u) && cpu->unsupported_opcode == 0u &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->last_trap == UINT16_MAX;
    if (call) {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0x0005u &&
                  dspic33_read_word(cpu, 0x5002u) == 0x0002u;
    } else {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
                  dspic33_read_word(cpu, 0x5002u) == 0x5a5au;
    }
    expect_dsp_matrix_case(state, matches, opcode, "literal control encoding");
}

static void run_literal_control_target_fault_case(TestState* state, Dspic33* cpu,
                                                  bool call, uint16_t low,
                                                  uint8_t high) {
    uint32_t opcode = (call ? 0x020000u : 0x040000u) | low;
    uint64_t cycles;
    bool matches;

    reset_processor_test(cpu, 0u);
    prepare_address_trap(state, cpu);
    cpu->corcon |= 0x0004u;
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, high) &&
              dspic33_step(cpu) == DSPIC33_TRAPPED && cpu->cycles - cycles == 4u &&
              cpu->last_trap == 1u && cpu->last_trap_return == 2u &&
              cpu->pc == 0x000340u && cpu->w[15] == (call ? 0x5008u : 0x5004u) &&
              (cpu->corcon & 0x0004u) == (call ? 0u : 0x0004u);
    if (call) {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 0x0005u &&
                  dspic33_read_word(cpu, 0x5002u) == 0u &&
                  dspic33_read_word(cpu, 0x5004u) == 2u;
    } else {
        matches = matches && dspic33_read_word(cpu, 0x5000u) == 2u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "literal control target fault");
}

static void run_reserved_literal_extension_case(TestState* state, Dspic33* cpu,
                                                bool call, uint32_t extension) {
    uint32_t opcode = call ? 0x020246u : 0x040246u;
    uint64_t illegal_resets;
    bool matches;

    reset_processor_test(cpu, 0u);
    cpu->corcon |= 0x0004u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, extension) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->call_depth == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    expect_dsp_matrix_case(state, matches, extension,
                           "literal control reserved extension");
}

static void run_reserved_literal_first_word_case(TestState* state, Dspic33* cpu,
                                                 uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;

    reset_processor_test(cpu, 0u);
    cpu->corcon |= 0x0004u;
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    dspic33_write_word(cpu, 0x5002u, 0x5a5au);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_load_program_word(cpu, 2u, 0u) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u && cpu->pc == 0u &&
              cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u &&
              cpu->last_trap == UINT16_MAX && cpu->call_depth == 0u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u &&
              dspic33_read_word(cpu, 0x5002u) == 0x5a5au;
    expect_dsp_matrix_case(state, matches, opcode,
                           "literal control reserved first word");
}

static void run_literal_rcall_encoding_case(TestState* state, Dspic33* cpu,
                                            uint16_t displacement) {
    uint32_t opcode = 0x070000u | displacement;
    uint32_t target =
        (uint32_t)((0x020002 + (int32_t)(int16_t)displacement * 2) & 0x007ffffe);
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_literal_control_encoding_case(cpu);
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == target &&
              cpu->cycles - cycles == 4u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0x010fu && cpu->corcon == 0x0020u && cpu->w[15] == 0x5004u &&
              cpu->call_depth == 1u && dspic33_read_word(cpu, 0x5000u) == 0x0003u &&
              dspic33_read_word(cpu, 0x5002u) == 0x0002u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    expect_dsp_matrix_case(state, matches, opcode, "literal RCALL encoding");
}

static void literal_control_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t direct_first_words = 0u;
    uint32_t reserved_first_words = 0u;
    uint32_t extension_fields = 0u;
    uint32_t reserved_extensions = 0u;
    uint32_t relative_calls = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint8_t call = 0u; call < 2u; call++) {
        for (uint32_t low = 0u; low <= UINT16_MAX; low++) {
            uint32_t opcode = (call != 0u ? 0x020000u : 0x040000u) | low;
            if ((low & 1u) == 0u) {
                run_literal_control_encoding_case(state, cpu, call != 0u, (uint16_t)low,
                                                  0u);
                direct_first_words++;
            } else {
                run_reserved_literal_first_word_case(state, cpu, opcode);
                reserved_first_words++;
            }
        }
        for (uint16_t high = 0u; high < 128u; high++) {
            uint16_t low = high == 127u ? 0xc000u : 0x1234u;
            uint32_t target = ((uint32_t)high << 16u) | low;
            if (dspic33_program_range_implemented(target, 2u)) {
                run_literal_control_encoding_case(state, cpu, call != 0u, low,
                                                  (uint8_t)high);
            } else {
                run_literal_control_target_fault_case(state, cpu, call != 0u, low,
                                                      (uint8_t)high);
            }
            extension_fields++;
        }
        for (uint32_t upper = 1u; upper < 0x20000u; upper++) {
            uint32_t extension = (upper << 7u) | (upper & 0x007fu);
            run_reserved_literal_extension_case(state, cpu, call != 0u, extension);
            reserved_extensions++;
        }
    }
    for (uint32_t displacement = 0u; displacement <= UINT16_MAX; displacement++) {
        run_literal_rcall_encoding_case(state, cpu, (uint16_t)displacement);
        relative_calls++;
    }
    expect(state, direct_first_words == 65536u,
           "literal CALL and GOTO first-word encodings are exhaustive");
    expect(state, reserved_first_words == 65536u,
           "literal CALL and GOTO reserved first words are exhaustive");
    expect(state, extension_fields == 256u,
           "literal CALL and GOTO target extension fields are exhaustive");
    expect(state, reserved_extensions == 262142u,
           "literal CALL and GOTO reserved extension fields are exhaustive");
    expect(state, relative_calls == 65536u,
           "literal RCALL encoding matrix is exhaustive");
}

static void prepare_return_encoding_case(Dspic33* cpu) {
    cpu->pc = 0x020000u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0020u;
    cpu->call_depth = 1u;
    cpu->w[15] = 0x5004u;
    cpu->initialized_working_registers = 0x8000u;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    cpu->stop_on_trap = false;
    dspic33_write_word(cpu, 0x5000u, 0x0301u);
    dspic33_write_word(cpu, 0x5002u, 0u);
}

static void run_retlw_encoding_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
    uint16_t literal = (uint16_t)((opcode >> 4u) & 0x03ffu);
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint16_t expected = byte_mode ? (uint16_t)(literal & 0x00ffu) : literal;
    uint16_t expected_stack =
        destination == 15u
            ? (uint16_t)(((byte_mode ? 0x5000u : 0u) | expected) & 0xfffeu)
            : 0x5000u;
    uint64_t cycles;
    uint64_t instructions;
    bool matches;

    prepare_return_encoding_case(cpu);
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(cpu, reg, (uint16_t)(0xa500u | reg));
    }
    if (byte_mode && destination != 15u) {
        expected |= 0xa500u;
    }
    cycles = cpu->cycles;
    instructions = cpu->instructions;
    matches = dspic33_load_program_word(cpu, 0x020000u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
              cpu->cycles - cycles == 6u && cpu->instructions - instructions == 1u &&
              cpu->sr == 0x010fu && cpu->corcon == 0x0024u &&
              cpu->w[15] == expected_stack && cpu->call_depth == 0u &&
              cpu->unsupported_opcode == 0u && !cpu->address_error &&
              !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
    if (destination != 15u) {
        matches = matches && cpu->w[destination] == expected;
    }
    for (uint8_t reg = 0u; reg < 15u; reg++) {
        if (reg != destination) {
            matches = matches && cpu->w[reg] == (uint16_t)(0xa500u | reg);
        }
    }
    expect_dsp_matrix_case(state, matches, opcode, "RETLW encoding");
}

static void exact_return_encoding_cases(TestState* state, Dspic33* cpu) {
    uint64_t cycles;
    bool matches;

    prepare_return_encoding_case(cpu);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0x020000u, OPCODE_RETURN) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
              cpu->cycles - cycles == 6u && cpu->w[15] == 0x5000u &&
              cpu->call_depth == 0u && cpu->sr == 0x010fu && cpu->corcon == 0x0024u;
    expect_dsp_matrix_case(state, matches, OPCODE_RETURN, "RETURN encoding");

    prepare_return_encoding_case(cpu);
    cpu->call_depth = 0u;
    cpu->interrupt_depth = 1u;
    dspic33_write_word(cpu, 0x5000u, 0x0301u);
    dspic33_write_word(cpu, 0x5002u, 0x0f80u);
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0x020000u, OPCODE_RETFIE) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x000300u &&
              cpu->cycles - cycles == 6u && cpu->w[15] == 0x5000u &&
              cpu->interrupt_depth == 0u && cpu->sr == 0x010fu &&
              cpu->corcon == 0x002cu;
    expect_dsp_matrix_case(state, matches, OPCODE_RETFIE, "RETFIE encoding");
}

static void return_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t documented_retlw = 0u;
    uint32_t encoded_byte_aliases = 0u;
    uint32_t reserved_retlw = 0u;
    uint32_t reserved_return = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (uint32_t opcode = 0x050000u; opcode < 0x058000u; opcode++) {
        bool byte_mode = (opcode & 0x004000u) != 0u;
        uint16_t literal = (uint16_t)((opcode >> 4u) & 0x03ffu);
        run_retlw_encoding_case(state, cpu, opcode);
        if (!byte_mode || literal <= UINT8_MAX) {
            documented_retlw++;
        } else {
            encoded_byte_aliases++;
        }
    }
    for (uint32_t opcode = 0x058000u; opcode < 0x060000u; opcode++) {
        run_invalid_binary_matrix_case(state, cpu, opcode);
        reserved_retlw++;
    }
    for (uint32_t opcode = 0x060000u; opcode < 0x070000u; opcode++) {
        if (opcode == OPCODE_RETURN || opcode == OPCODE_RETFIE) {
            continue;
        }
        run_invalid_binary_matrix_case(state, cpu, opcode);
        reserved_return++;
    }
    exact_return_encoding_cases(state, cpu);
    expect(state, documented_retlw == 20480u,
           "documented RETLW operand encodings are exhaustive");
    expect(state, encoded_byte_aliases == 12288u,
           "RETLW byte upper-literal aliases are exhaustive");
    expect(state, reserved_retlw == 32768u, "reserved RETLW encodings are exhaustive");
    expect(state, reserved_return == 65534u,
           "reserved RETURN and RETFIE encodings are exhaustive");
}

static void run_legal_dsp_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode,
                                      uint8_t target_accumulator, int64_t target_result,
                                      uint8_t x_operation, uint8_t y_operation,
                                      uint8_t x_destination, uint8_t y_destination,
                                      uint8_t write_back,
                                      int8_t difference_destination) {
    uint16_t expected_w[14] = {0u};
    uint16_t expected_memory = 0xa5a5u;
    uint64_t cycles;
    bool matches;
    uint8_t reg;

    prepare_dsp_matrix_case(cpu, target_accumulator, x_operation, y_operation,
                            expected_w);
    apply_dsp_matrix_prefetch(expected_w, x_operation, x_destination, false,
                              difference_destination < 0);
    apply_dsp_matrix_prefetch(expected_w, y_operation, y_destination, true,
                              difference_destination < 0);
    if (difference_destination >= 0) {
        expected_w[(uint8_t)difference_destination] =
            (uint16_t)(dsp_matrix_prefetch_value(false, x_operation) -
                       dsp_matrix_prefetch_value(true, y_operation));
    }
    if (write_back == DSP_MATRIX_WRITE_BACK_DIRECT) {
        expected_w[13] = 0x1235u;
    } else if (write_back == DSP_MATRIX_WRITE_BACK_INDIRECT) {
        expected_w[13] = 0x5202u;
        expected_memory = 0x1235u;
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u &&
              cpu->accumulator[target_accumulator] == target_result &&
              cpu->accumulator[target_accumulator ^ 1u] == 0x12348001 &&
              cpu->sr == 0x000fu && cpu->corcon == 0x0001u &&
              dspic33_read_word(cpu, 0x5200u) == expected_memory &&
              !cpu->address_error && !cpu->illegal_reset &&
              cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX;
    for (reg = 4u; reg <= 13u; reg++) {
        matches = matches && cpu->w[reg] == expected_w[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal DSP encoding");
}

static void general_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu,
                                              uint32_t* legal_cases) {
    static const uint8_t pair_encodings[6] = {0u, 1u, 2u, 4u, 5u, 6u};
    static const uint8_t pair_left[6] = {0u, 0u, 0u, 1u, 1u, 2u};
    static const uint8_t pair_right[6] = {1u, 2u, 3u, 2u, 3u, 3u};
    static const int64_t operands[4] = {2, 3, 5, 7};
    static const struct {
        uint16_t bits;
        int8_t sign;
        bool replace;
        uint8_t write_back;
    } forms[8] = {
        {0x0003u, 1, true, DSP_MATRIX_WRITE_BACK_NONE},
        {0x4003u, -1, true, DSP_MATRIX_WRITE_BACK_NONE},
        {0x0002u, 1, false, DSP_MATRIX_WRITE_BACK_NONE},
        {0x0000u, 1, false, DSP_MATRIX_WRITE_BACK_DIRECT},
        {0x0001u, 1, false, DSP_MATRIX_WRITE_BACK_INDIRECT},
        {0x4002u, -1, false, DSP_MATRIX_WRITE_BACK_NONE},
        {0x4000u, -1, false, DSP_MATRIX_WRITE_BACK_DIRECT},
        {0x4001u, -1, false, DSP_MATRIX_WRITE_BACK_INDIRECT},
    };
    uint8_t pair;
    uint8_t accumulator;
    uint8_t form;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t x_destination;
    uint8_t y_destination;

    for (pair = 0u; pair < 6u; pair++) {
        int64_t product = operands[pair_left[pair]] * operands[pair_right[pair]];
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (form = 0u; form < 8u; form++) {
                int64_t result = forms[form].sign * product;
                if (!forms[form].replace) {
                    result += 100;
                }
                for (x_operation = 0u; x_operation < 16u; x_operation++) {
                    for (y_operation = 0u; y_operation < 16u; y_operation++) {
                        for (x_destination = 0u; x_destination < 4u; x_destination++) {
                            for (y_destination = 0u; y_destination < 4u;
                                 y_destination++) {
                                uint32_t opcode =
                                    0xc00000u |
                                    ((uint32_t)pair_encodings[pair] << 16u) |
                                    ((uint32_t)accumulator << 15u) |
                                    ((uint32_t)x_destination << 12u) |
                                    ((uint32_t)y_destination << 10u) |
                                    ((uint32_t)x_operation << 6u) |
                                    ((uint32_t)y_operation << 2u) | forms[form].bits;
                                run_legal_dsp_matrix_case(state, cpu, opcode,
                                                          accumulator, result,
                                                          x_operation, y_operation,
                                                          (uint8_t)(4u + x_destination),
                                                          (uint8_t)(4u + y_destination),
                                                          forms[form].write_back, -1);
                                (*legal_cases)++;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void special_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu,
                                              uint32_t* legal_cases) {
    static const uint32_t families[2] = {0xc30000u, 0xc70000u};
    uint8_t family;
    uint8_t accumulator;
    uint8_t write_back;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t x_destination;
    uint8_t y_destination;

    for (family = 0u; family < 2u; family++) {
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (write_back = 0u; write_back < 3u; write_back++) {
                for (x_operation = 0u; x_operation < 16u; x_operation++) {
                    for (y_operation = 0u; y_operation < 16u; y_operation++) {
                        for (x_destination = 0u; x_destination < 4u; x_destination++) {
                            for (y_destination = 0u; y_destination < 4u;
                                 y_destination++) {
                                uint32_t opcode =
                                    families[family] | ((uint32_t)accumulator << 15u) |
                                    ((uint32_t)x_destination << 12u) |
                                    ((uint32_t)y_destination << 10u) |
                                    ((uint32_t)x_operation << 6u) |
                                    ((uint32_t)y_operation << 2u) | write_back;
                                run_legal_dsp_matrix_case(
                                    state, cpu, opcode, accumulator,
                                    family == 0u ? 0 : 100, x_operation, y_operation,
                                    (uint8_t)(4u + x_destination),
                                    (uint8_t)(4u + y_destination), write_back, -1);
                                (*legal_cases)++;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void square_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu,
                                             uint32_t* legal_cases) {
    static const int64_t operands[4] = {2, 3, 5, 7};
    uint8_t source;
    uint8_t accumulator;
    uint8_t replace;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t x_destination;
    uint8_t y_destination;

    for (source = 0u; source < 4u; source++) {
        int64_t product = operands[source] * operands[source];
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (replace = 0u; replace < 2u; replace++) {
                int64_t result = replace != 0u ? product : 100 + product;
                for (x_operation = 0u; x_operation < 16u; x_operation++) {
                    for (y_operation = 0u; y_operation < 16u; y_operation++) {
                        for (x_destination = 0u; x_destination < 4u; x_destination++) {
                            for (y_destination = 0u; y_destination < 4u;
                                 y_destination++) {
                                uint32_t opcode =
                                    0xf00000u | ((uint32_t)source << 16u) |
                                    ((uint32_t)accumulator << 15u) |
                                    ((uint32_t)x_destination << 12u) |
                                    ((uint32_t)y_destination << 10u) |
                                    ((uint32_t)x_operation << 6u) |
                                    ((uint32_t)y_operation << 2u) | replace;
                                run_legal_dsp_matrix_case(
                                    state, cpu, opcode, accumulator, result,
                                    x_operation, y_operation,
                                    (uint8_t)(4u + x_destination),
                                    (uint8_t)(4u + y_destination),
                                    DSP_MATRIX_WRITE_BACK_NONE, -1);
                                (*legal_cases)++;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void euclidean_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu,
                                                uint32_t* legal_cases) {
    static const int64_t operands[4] = {2, 3, 5, 7};
    uint8_t source;
    uint8_t accumulator;
    uint8_t operation;
    uint8_t destination;
    uint8_t x_operation;
    uint8_t y_operation;

    for (source = 0u; source < 4u; source++) {
        int64_t product = operands[source] * operands[source];
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (operation = 2u; operation < 4u; operation++) {
                int64_t result = operation == 2u ? 100 + product : product;
                for (destination = 0u; destination < 4u; destination++) {
                    for (x_operation = 0u; x_operation < 16u; x_operation++) {
                        if (x_operation == 4u) {
                            continue;
                        }
                        for (y_operation = 0u; y_operation < 16u; y_operation++) {
                            uint32_t opcode;
                            if (y_operation == 4u) {
                                continue;
                            }
                            opcode = 0xf04000u | ((uint32_t)source << 16u) |
                                     ((uint32_t)accumulator << 15u) |
                                     ((uint32_t)destination << 12u) |
                                     ((uint32_t)x_operation << 6u) |
                                     ((uint32_t)y_operation << 2u) | operation;
                            run_legal_dsp_matrix_case(
                                state, cpu, opcode, accumulator, result, x_operation,
                                y_operation, 4u, 4u, DSP_MATRIX_WRITE_BACK_NONE,
                                (int8_t)(4u + destination));
                            (*legal_cases)++;
                        }
                    }
                }
            }
        }
    }
}

static void prepare_invalid_dsp_matrix_case(Dspic33* cpu) {
    uint8_t reg;
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0001u;
    cpu->accumulator[0] = 0x123456789a;
    cpu->accumulator[1] = -0x123456789a;
    cpu->illegal_reset = false;
    cpu->last_trap = UINT16_MAX;
    cpu->stop_reason = DSPIC33_RUNNING;
    for (reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(cpu, reg,
                                     (uint16_t)(0x5000u + (uint16_t)reg * 2u));
    }
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
}

static void run_invalid_dsp_matrix_case(TestState* state, Dspic33* cpu,
                                        uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;
    uint8_t reg;

    prepare_invalid_dsp_matrix_case(cpu);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->software_reset_count == 0u && cpu->trap_count == 0u &&
              cpu->last_trap == UINT16_MAX && cpu->pc == 0u && cpu->w[15] == 0x1000u &&
              cpu->initialized_working_registers == 0x8000u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    for (reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "illegal DSP encoding");
}

static void invalid_dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu,
                                              uint32_t* invalid_cases) {
    uint8_t pair;
    uint8_t accumulator;
    uint8_t alternate;
    uint8_t x_destination;
    uint8_t y_destination;
    uint8_t x_operation;
    uint8_t y_operation;
    uint8_t low;

    for (pair = 0u; pair < 8u; pair++) {
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (alternate = 0u; alternate < 2u; alternate++) {
                for (x_destination = 0u; x_destination < 4u; x_destination++) {
                    for (y_destination = 0u; y_destination < 4u; y_destination++) {
                        for (x_operation = 0u; x_operation < 16u; x_operation++) {
                            for (y_operation = 0u; y_operation < 16u; y_operation++) {
                                for (low = 0u; low < 4u; low++) {
                                    bool valid = (pair != 3u && pair != 7u) ||
                                                 (alternate == 0u && low != 3u);
                                    uint32_t opcode;
                                    if (valid) {
                                        continue;
                                    }
                                    opcode = 0xc00000u | ((uint32_t)pair << 16u) |
                                             ((uint32_t)accumulator << 15u) |
                                             ((uint32_t)alternate << 14u) |
                                             ((uint32_t)x_destination << 12u) |
                                             ((uint32_t)y_destination << 10u) |
                                             ((uint32_t)x_operation << 6u) |
                                             ((uint32_t)y_operation << 2u) | low;
                                    run_invalid_dsp_matrix_case(state, cpu, opcode);
                                    (*invalid_cases)++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (pair = 0u; pair < 4u; pair++) {
        for (accumulator = 0u; accumulator < 2u; accumulator++) {
            for (alternate = 0u; alternate < 2u; alternate++) {
                for (x_destination = 0u; x_destination < 4u; x_destination++) {
                    for (y_destination = 0u; y_destination < 4u; y_destination++) {
                        for (x_operation = 0u; x_operation < 16u; x_operation++) {
                            for (y_operation = 0u; y_operation < 16u; y_operation++) {
                                for (low = 0u; low < 4u; low++) {
                                    bool valid = alternate == 0u
                                                     ? low < 2u
                                                     : low >= 2u &&
                                                           y_destination == 0u &&
                                                           x_operation != 4u &&
                                                           y_operation != 4u;
                                    uint32_t opcode;
                                    if (valid) {
                                        continue;
                                    }
                                    opcode = 0xf00000u | ((uint32_t)pair << 16u) |
                                             ((uint32_t)accumulator << 15u) |
                                             ((uint32_t)alternate << 14u) |
                                             ((uint32_t)x_destination << 12u) |
                                             ((uint32_t)y_destination << 10u) |
                                             ((uint32_t)x_operation << 6u) |
                                             ((uint32_t)y_operation << 2u) | low;
                                    run_invalid_dsp_matrix_case(state, cpu, opcode);
                                    (*invalid_cases)++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void dsp_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal_cases = 0u;
    uint32_t invalid_cases = 0u;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    general_dsp_encoding_matrix_cases(state, cpu, &legal_cases);
    special_dsp_encoding_matrix_cases(state, cpu, &legal_cases);
    square_dsp_encoding_matrix_cases(state, cpu, &legal_cases);
    euclidean_dsp_encoding_matrix_cases(state, cpu, &legal_cases);
    invalid_dsp_encoding_matrix_cases(state, cpu, &invalid_cases);
    expect(state, legal_cases == 522304u, "DSP legal encoding matrix is exhaustive");
    expect(state, invalid_cases == 264128u,
           "DSP illegal encoding matrix is exhaustive");
}

static int64_t generic_multiply_operand(uint16_t value, bool signed_value) {
    return signed_value ? (int16_t)value : value;
}

static uint16_t generic_multiply_source_value(uint8_t mode) {
    static const uint16_t values[6] = {
        0u, 0x8003u, 0x8003u, 0x8003u, 0x8005u, 0x8007u,
    };
    return values[mode];
}

static void prepare_generic_multiply_case(Dspic33* cpu, uint8_t source_mode,
                                          uint8_t source_register,
                                          uint16_t expected_w[16]) {
    uint8_t reg;
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0005u;
    cpu->accumulator[0] = 0x1111222233;
    cpu->accumulator[1] = -0x1111222233;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->events.count = 0u;
    cpu->splim_enabled = false;
    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value = (uint16_t)(0x8000u + (uint16_t)reg * 2u);
        dspic33_set_working_register(cpu, reg, value);
        expected_w[reg] = cpu->w[reg];
    }
    if (source_mode != 0u && source_mode < 6u) {
        dspic33_set_working_register(cpu, source_register, 0x5008u);
        expected_w[source_register] = 0x5008u;
        dspic33_write_word(cpu, 0x5006u, 0x8005u);
        dspic33_write_word(cpu, 0x5008u, 0x8003u);
        dspic33_write_word(cpu, 0x500au, 0x8007u);
    }
}

static void run_legal_generic_multiply_case(TestState* state, Dspic33* cpu,
                                            uint32_t opcode, bool base_signed,
                                            bool source_signed, uint8_t base_register,
                                            uint8_t destination, uint8_t source_mode,
                                            uint8_t source_register) {
    uint16_t expected_w[16] = {0u};
    int64_t expected_accumulator[2] = {0x1111222233, -0x1111222233};
    uint16_t source;
    int64_t product;
    uint64_t cycles;
    bool matches;
    uint8_t reg;

    prepare_generic_multiply_case(cpu, source_mode, source_register, expected_w);
    source = source_mode >= 6u   ? (uint16_t)(opcode & 0x001fu)
             : source_mode == 0u ? expected_w[source_register]
                                 : generic_multiply_source_value(source_mode);
    if (source_mode == 2u || source_mode == 4u) {
        expected_w[source_register] = 0x5006u;
    } else if (source_mode == 3u || source_mode == 5u) {
        expected_w[source_register] = 0x500au;
    }
    product = generic_multiply_operand(expected_w[base_register], base_signed) *
              generic_multiply_operand(source, source_signed);
    if (destination >= 14u) {
        expected_accumulator[destination & 1u] = product;
    } else {
        uint8_t result_register = (uint8_t)(destination & 0x0eu);
        expected_w[result_register] = (uint16_t)product;
        if ((destination & 1u) == 0u) {
            expected_w[result_register + 1u] = (uint16_t)((uint32_t)product >> 16u);
        }
    }
    cycles = cpu->cycles;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
              cpu->cycles - cycles == 1u &&
              cpu->accumulator[0] == expected_accumulator[0] &&
              cpu->accumulator[1] == expected_accumulator[1] && cpu->sr == 0x010fu &&
              cpu->corcon == 0x0005u && !cpu->address_error && !cpu->illegal_reset &&
              cpu->unsupported_opcode == 0u && cpu->last_trap == UINT16_MAX;
    for (reg = 0u; reg < 16u; reg++) {
        matches = matches && cpu->w[reg] == expected_w[reg];
    }
    expect_dsp_matrix_case(state, matches, opcode, "legal generic multiply encoding");
}

static void generic_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal_cases = 0u;
    uint32_t invalid_cases = 0u;
    uint8_t base_signed;
    uint8_t source_signed;
    uint8_t base_register;
    uint8_t destination;
    uint8_t source_mode;
    uint8_t source_register;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (base_signed = 0u; base_signed < 2u; base_signed++) {
        for (source_signed = 0u; source_signed < 2u; source_signed++) {
            for (base_register = 0u; base_register < 16u; base_register++) {
                for (destination = 0u; destination < 16u; destination++) {
                    for (source_mode = 0u; source_mode < 8u; source_mode++) {
                        for (source_register = 0u; source_register < 16u;
                             source_register++) {
                            uint32_t opcode =
                                0xb80000u | ((uint32_t)base_signed << 16u) |
                                ((uint32_t)source_signed << 15u) |
                                ((uint32_t)base_register << 11u) |
                                ((uint32_t)destination << 7u) |
                                ((uint32_t)source_mode << 4u) | source_register;
                            if (source_signed != 0u && source_mode >= 6u) {
                                run_invalid_dsp_matrix_case(state, cpu, opcode);
                                invalid_cases++;
                            } else {
                                run_legal_generic_multiply_case(
                                    state, cpu, opcode, base_signed != 0u,
                                    source_signed != 0u, base_register, destination,
                                    source_mode, source_register);
                                legal_cases++;
                            }
                        }
                    }
                }
            }
        }
    }
    expect(state, legal_cases == 114688u,
           "generic multiply legal encoding matrix is exhaustive");
    expect(state, invalid_cases == 16384u,
           "generic multiply illegal encoding matrix is exhaustive");
}

static void file_multiply_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint8_t byte_mode;
    uint16_t address;

    expect(state, dspic33_load_program_word(cpu, 0x000006u, 0x000340u),
           "load file multiply address-error vector");
    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (address = 0u; address < 0x2000u; address++) {
            uint32_t opcode = 0xbc0000u | ((uint32_t)byte_mode << 14u) | address;
            bool matches;
            dspic33_reset(cpu, 0u);
            cpu->stop_on_trap = true;
            dspic33_set_working_register(cpu, 0u, 0u);
            dspic33_set_working_register(cpu, 2u, 0xa5a5u);
            dspic33_set_working_register(cpu, 3u, 0x5a5au);
            dspic33_set_working_register(cpu, 15u, 0x5000u);
            cpu->sr = 0x010fu;
            matches = dspic33_load_program_word(cpu, 0u, opcode);
            if (byte_mode == 0u && (address & 1u) != 0u) {
                matches = matches && dspic33_step(cpu) == DSPIC33_TRAPPED &&
                          cpu->last_trap == 1u && cpu->last_trap_return == 2u &&
                          cpu->pc == 0x000340u &&
                          (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u;
            } else {
                matches = matches && dspic33_step(cpu) == DSPIC33_RUNNING &&
                          cpu->pc == 2u && cpu->w[2] == 0u &&
                          cpu->w[3] == (byte_mode != 0u ? 0x5a5au : 0u) &&
                          cpu->sr == 0x010fu && cpu->last_trap == UINT16_MAX &&
                          cpu->unsupported_opcode == 0u;
            }
            expect_dsp_matrix_case(state, matches, opcode, "file multiply encoding");
            cases++;
        }
    }
    expect(state, cases == 16384u, "file multiply encoding matrix is exhaustive");
}

static void prepare_move_matrix_case(Dspic33* cpu) {
    cpu->pc = 0u;
    cpu->sr = 0x010fu;
    cpu->corcon = 0x0005u;
    cpu->previous_working_register_writes = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_trap = UINT16_MAX;
    cpu->last_interrupt = UINT16_MAX;
    cpu->address_error = false;
    cpu->illegal_reset = false;
    cpu->stop_reason = DSPIC33_RUNNING;
    cpu->instruction_active = false;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->events.count = 0u;
}

static void run_invalid_move_matrix_case(TestState* state, Dspic33* cpu,
                                         uint32_t opcode) {
    uint64_t illegal_resets;
    bool matches;
    uint8_t reg;

    prepare_invalid_dsp_matrix_case(cpu);
    dspic33_set_working_register(cpu, 15u, 0x5000u);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) &&
              dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset &&
              cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->software_reset_count == 0u && cpu->trap_count == 0u &&
              cpu->last_trap == UINT16_MAX && cpu->pc == 0u && cpu->w[15] == 0x1000u &&
              cpu->initialized_working_registers == 0x8000u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    for (reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "illegal move encoding");
}

static void move_literal_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint32_t literal;
    uint8_t destination;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (literal = 0u; literal <= UINT16_MAX; literal++) {
        for (destination = 0u; destination < 16u; destination++) {
            uint32_t opcode = 0x200000u | (literal << 4u) | (uint32_t)destination;
            uint16_t expected =
                destination == 15u ? (uint16_t)(literal & 0xfffeu) : (uint16_t)literal;
            uint64_t cycles;
            bool matches;

            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, destination, 0x5a5au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
            expect_dsp_matrix_case(state, matches, opcode, "MOV literal encoding");
            cases++;
        }
    }
    expect(state, cases == 1048576u, "MOV literal encoding matrix is exhaustive");

    cases = 0u;
    for (literal = 0u; literal <= UINT8_MAX; literal++) {
        for (destination = 0u; destination < 16u; destination++) {
            uint32_t opcode = 0xb3c000u | (literal << 4u) | (uint32_t)destination;
            uint16_t expected = (uint16_t)(0x5a00u | literal);
            uint64_t cycles;
            bool matches;
            if (destination == 15u) {
                expected &= 0xfffeu;
            }
            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, destination, 0x5a5au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[destination] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset && cpu->last_trap == UINT16_MAX;
            expect_dsp_matrix_case(state, matches, opcode, "MOV byte literal encoding");
            cases++;
        }
    }
    expect(state, cases == 4096u, "MOV byte literal encoding matrix is exhaustive");
}

static void move_register_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xfd0000u; opcode <= 0xfd0fffu; opcode++) {
        bool legal = (opcode & 0xfff870u) == 0xfd0000u;
        if (!legal) {
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t source = (uint8_t)(opcode & 0x0fu);
            uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
            uint16_t source_value = (uint16_t)(0x1100u | source);
            uint16_t destination_value = (uint16_t)(0x2200u | destination);
            uint16_t expected_source;
            uint16_t expected_destination;
            uint64_t cycles;
            bool matches;

            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, source, source_value);
            dspic33_set_working_register(cpu, destination, destination_value);
            if (source == destination) {
                dspic33_set_working_register(cpu, source, source_value);
            }
            expected_destination = cpu->w[source];
            expected_source = cpu->w[destination];
            if (source == 15u) {
                expected_source &= 0xfffeu;
            }
            if (destination == 15u) {
                expected_destination &= 0xfffeu;
            }
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[source] == expected_source &&
                      cpu->w[destination] == expected_destination &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            expect_dsp_matrix_case(state, matches, opcode, "EXCH encoding");
        }
        cases++;
    }
    expect(state, cases == 4096u, "EXCH encoding matrix is exhaustive");

    cases = 0u;
    for (opcode = 0xfd8000u; opcode <= 0xfdffffu; opcode++) {
        bool legal = (opcode & 0xffbff0u) == 0xfd8000u;
        if (!legal) {
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t reg = (uint8_t)(opcode & 0x0fu);
            bool byte_mode = (opcode & 0x004000u) != 0u;
            uint16_t expected = byte_mode ? 0xa5a5u : 0x5aa5u;
            uint64_t cycles;
            bool matches;
            if (reg == 15u) {
                expected &= 0xfffeu;
            }
            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, reg, 0xa55au);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->w[reg] == expected &&
                      cpu->sr == 0x010fu && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            expect_dsp_matrix_case(state, matches, opcode, "SWAP encoding");
        }
        cases++;
    }
    expect(state, cases == 32768u, "SWAP encoding matrix is exhaustive");
}

static void movpag_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xfec000u; opcode <= 0xfecfffu; opcode++) {
        uint8_t page_register = (uint8_t)((opcode >> 10u) & 3u);
        uint16_t literal = (uint16_t)(opcode & 0x03ffu);
        if (page_register == 3u) {
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint16_t expected = page_register == 0u   ? literal
                                : page_register == 1u ? (uint16_t)(literal & 0x01ffu)
                                                      : (uint16_t)(literal & 0x00ffu);
            uint64_t cycles;
            bool matches;
            prepare_move_matrix_case(cpu);
            cpu->dsrpag = 0x0155u;
            cpu->dswpag = 0x00aau;
            cpu->tblpag = 0x005au;
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            if (page_register == 0u) {
                matches = matches && cpu->dsrpag == expected;
            } else if (page_register == 1u) {
                matches = matches && cpu->dswpag == expected;
            } else {
                matches = matches && cpu->tblpag == expected;
            }
            expect_dsp_matrix_case(state, matches, opcode, "MOVPAG literal encoding");
        }
        cases++;
    }
    expect(state, cases == 4096u, "MOVPAG literal encoding matrix is exhaustive");

    cases = 0u;
    for (opcode = 0xfed000u; opcode <= 0xfedfffu; opcode++) {
        bool fields_valid = (opcode & 0x0003f0u) == 0u;
        uint8_t page_register = (uint8_t)((opcode >> 10u) & 3u);
        if (!fields_valid || page_register == 3u) {
            run_invalid_move_matrix_case(state, cpu, opcode);
        } else {
            uint8_t source = (uint8_t)(opcode & 0x0fu);
            uint16_t value = (uint16_t)(0x03a0u | source);
            uint16_t expected;
            uint64_t cycles;
            bool matches;
            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, source, value);
            value = cpu->w[source];
            expected = page_register == 0u   ? (uint16_t)(value & 0x03ffu)
                       : page_register == 1u ? (uint16_t)(value & 0x01ffu)
                                             : (uint16_t)(value & 0x00ffu);
            cpu->dsrpag = 0x0155u;
            cpu->dswpag = 0x00aau;
            cpu->tblpag = 0x005au;
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                      dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->cycles - cycles == 1u && cpu->unsupported_opcode == 0u &&
                      !cpu->illegal_reset;
            if (page_register == 0u) {
                matches = matches && cpu->dsrpag == expected;
            } else if (page_register == 1u) {
                matches = matches && cpu->dswpag == expected;
            } else {
                matches = matches && cpu->tblpag == expected;
            }
            expect_dsp_matrix_case(state, matches, opcode, "MOVPAG register encoding");
        }
        cases++;
    }
    expect(state, cases == 4096u, "MOVPAG register encoding matrix is exhaustive");
}

typedef struct {
    uint16_t address;
    bool direct;
} MoveMatrixOperand;

static MoveMatrixOperand resolve_move_matrix_operand(uint16_t registers[16],
                                                     uint8_t mode, uint8_t reg,
                                                     uint8_t offset_reg,
                                                     uint8_t width) {
    MoveMatrixOperand operand = {0u, mode == 0u};
    int32_t adjusted;
    if (mode == 0u) {
        return operand;
    }
    if (mode == 1u) {
        operand.address = registers[reg];
        return operand;
    }
    if (mode == 2u || mode == 3u) {
        operand.address = registers[reg];
        adjusted =
            (int32_t)registers[reg] + (mode == 3u ? (int32_t)width : -(int32_t)width);
        registers[reg] = reg == 15u ? (uint16_t)adjusted & 0xfffeu : (uint16_t)adjusted;
        return operand;
    }
    if (mode == 4u || mode == 5u) {
        adjusted =
            (int32_t)registers[reg] + (mode == 5u ? (int32_t)width : -(int32_t)width);
        operand.address = (uint16_t)adjusted;
        registers[reg] = reg == 15u ? (uint16_t)adjusted & 0xfffeu : (uint16_t)adjusted;
        return operand;
    }
    operand.address = (uint16_t)(registers[reg] + registers[offset_reg]);
    return operand;
}

static void prepare_move_registers(Dspic33* cpu, uint16_t expected[16], uint16_t base,
                                   uint16_t stride) {
    uint8_t reg;
    prepare_move_matrix_case(cpu);
    for (reg = 0u; reg < 16u; reg++) {
        uint16_t value = (uint16_t)(base + (uint16_t)reg * stride);
        dspic33_set_working_register(cpu, reg, value);
        expected[reg] = cpu->w[reg];
    }
}

static bool move_matrix_registers_match(const Dspic33* cpu,
                                        const uint16_t expected[16]) {
    uint8_t reg;
    for (reg = 0u; reg < 16u; reg++) {
        if (cpu->w[reg] != expected[reg]) {
            return false;
        }
    }
    return true;
}

static void generic_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x780000u; opcode <= 0x7fffffu; opcode++) {
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t offset_register = (uint8_t)((opcode >> 15u) & 0x0fu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        uint8_t width = byte_mode ? 1u : 2u;
        uint16_t expected[16];
        MoveMatrixOperand source;
        MoveMatrixOperand destination;
        uint16_t value;
        uint64_t cycles;
        bool matches;

        prepare_move_registers(cpu, expected, 0x2000u, 2u);
        source = resolve_move_matrix_operand(expected, source_mode, source_register,
                                             offset_register, width);
        if (source.direct) {
            value = byte_mode ? (uint8_t)expected[source_register]
                              : expected[source_register];
        } else if (byte_mode) {
            dspic33_write_byte(cpu, source.address, 0xa5u);
            value = 0x00a5u;
        } else {
            dspic33_write_word(cpu, source.address, 0xa55au);
            value = 0xa55au;
        }
        destination = resolve_move_matrix_operand(
            expected, destination_mode, destination_register, offset_register, width);
        if (destination.direct) {
            if (byte_mode) {
                expected[destination_register] =
                    (uint16_t)((expected[destination_register] & 0xff00u) | value);
            } else {
                expected[destination_register] = value;
            }
            if (destination_register == 15u) {
                expected[destination_register] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 1u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX &&
                  move_matrix_registers_match(cpu, expected);
        if (!destination.direct) {
            matches =
                matches &&
                (byte_mode ? dspic33_read_byte(cpu, destination.address) == value
                           : dspic33_read_word(cpu, destination.address) == value);
        }
        expect_dsp_matrix_case(state, matches, opcode, "generic MOV encoding");
        cases++;
    }
    expect(state, cases == 524288u, "generic MOV encoding matrix is exhaustive");
}

static int16_t move_offset_literal(uint32_t opcode, bool byte_mode) {
    uint16_t encoded =
        (uint16_t)((((opcode >> 15u) & 0x0fu) << 6u) |
                   (((opcode >> 11u) & 0x07u) << 3u) | ((opcode >> 4u) & 0x07u));
    int16_t offset = (int16_t)(encoded | ((encoded & 0x0200u) != 0u ? 0xfc00u : 0u));
    return byte_mode ? offset : (int16_t)(offset * 2);
}

static void offset_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x900000u; opcode <= 0x9fffffu; opcode++) {
        uint8_t source = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        bool store = (opcode & 0x080000u) != 0u;
        int16_t offset = move_offset_literal(opcode, byte_mode);
        uint16_t expected[16];
        uint16_t address;
        uint16_t value;
        uint64_t cycles;
        bool matches;

        prepare_move_registers(cpu, expected, 0x4000u, 2u);
        if (store) {
            address = (uint16_t)(expected[destination] + offset);
            value = byte_mode ? (uint8_t)expected[source] : expected[source];
        } else {
            address = (uint16_t)(expected[source] + offset);
            value = byte_mode ? 0x00a5u : 0xa55au;
            if (byte_mode) {
                dspic33_write_byte(cpu, address, (uint8_t)value);
                expected[destination] =
                    (uint16_t)((expected[destination] & 0xff00u) | value);
            } else {
                dspic33_write_word(cpu, address, value);
                expected[destination] = value;
            }
            if (destination == 15u) {
                expected[destination] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 1u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX &&
                  move_matrix_registers_match(cpu, expected);
        if (store) {
            matches = matches && (byte_mode ? dspic33_read_byte(cpu, address) == value
                                            : dspic33_read_word(cpu, address) == value);
        }
        expect_dsp_matrix_case(state, matches, opcode, "offset MOV encoding");
        cases++;
    }
    expect(state, cases == 1048576u, "offset MOV encoding matrix is exhaustive");
}

static void move_double_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t legal_loads = 0u;
    uint32_t legal_stores = 0u;
    uint32_t invalid = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0xbe0000u; opcode <= 0xbeffffu; opcode++) {
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
        uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
        bool load = (opcode & 0xfff880u) == 0xbe0000u && source_mode <= 5u &&
                    (source_mode != 0u || (source_register & 1u) == 0u);
        bool store = (opcode & 0xffc071u) == 0xbe8000u && destination_mode >= 1u &&
                     destination_mode <= 5u;
        if (!load && !store) {
            run_invalid_move_matrix_case(state, cpu, opcode);
            invalid++;
            continue;
        }
        uint16_t expected[16];
        MoveMatrixOperand source;
        MoveMatrixOperand destination;
        uint16_t low;
        uint16_t high;
        uint64_t cycles;
        bool matches;

        prepare_move_registers(cpu, expected, 0x3000u, 4u);
        source =
            resolve_move_matrix_operand(expected, source_mode, source_register, 0u, 4u);
        if (source.direct) {
            source_register &= 0x0eu;
            low = expected[source_register];
            high = expected[source_register + 1u];
        } else {
            dspic33_write_word(cpu, source.address, 0x1111u);
            dspic33_write_word(cpu, (uint16_t)(source.address + 2u), 0x2222u);
            low = 0x1111u;
            high = 0x2222u;
        }
        destination = resolve_move_matrix_operand(expected, destination_mode,
                                                  destination_register, 0u, 4u);
        if (destination.direct) {
            destination_register &= 0x0eu;
            expected[destination_register] = low;
            expected[destination_register + 1u] = high;
            if (destination_register + 1u == 15u) {
                expected[15] &= 0xfffeu;
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode) &&
                  dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 2u &&
                  cpu->cycles - cycles == 2u && cpu->sr == 0x010fu &&
                  cpu->unsupported_opcode == 0u && !cpu->illegal_reset &&
                  cpu->last_trap == UINT16_MAX &&
                  move_matrix_registers_match(cpu, expected);
        if (!destination.direct) {
            matches =
                matches && dspic33_read_word(cpu, destination.address) == low &&
                dspic33_read_word(cpu, (uint16_t)(destination.address + 2u)) == high;
        }
        expect_dsp_matrix_case(state, matches, opcode, "MOV.D encoding");
        if (load) {
            legal_loads++;
        } else {
            legal_stores++;
        }
    }
    expect(state, legal_loads == 704u, "MOV.D load encodings are exhaustive");
    expect(state, legal_stores == 640u, "MOV.D store encodings are exhaustive");
    expect(state, invalid == 64192u, "MOV.D illegal encodings are exhaustive");
}

static void move_data_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint32_t opcode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    for (opcode = 0x800000u; opcode <= 0x8fffffu; opcode++) {
        bool store = (opcode & 0x080000u) != 0u;
        uint8_t reg = (uint8_t)(opcode & 0x0fu);
        uint16_t encoded_address =
            (uint16_t)((((opcode >> 4u) & 0x7fffu) << 1u) & 0xffffu);
        uint32_t address;
        uint16_t transfer_value;
        uint64_t cycles;
        Dspic33StopReason reason;
        bool matches;

        if (opcode == 0x880000u || encoded_address >= 0xe000u) {
            dspic33_reset(cpu, 0u);
            dspic33_set_async_events(cpu, false);
        }
        prepare_move_matrix_case(cpu);
        cpu->dsrpag = 1u;
        cpu->dswpag = 1u;
        dspic33_set_working_register(cpu, 15u, 0x5000u);
        address = encoded_address < 0x8000u
                      ? encoded_address
                      : (uint32_t)(0x8000u | (encoded_address & 0x7fffu));
        if (store) {
            dspic33_set_working_register(
                cpu, reg, address >= 0x1000u ? (uint16_t)(0xa500u | reg) : 0u);
            transfer_value = cpu->w[reg];
        } else {
            dspic33_set_working_register(cpu, reg, 0x5a5au);
            transfer_value = 0xa55au;
            if (address >= 0x1000u && encoded_address < 0xe000u) {
                dspic33_write_word(cpu, address, 0xa55au);
            }
        }
        cycles = cpu->cycles;
        matches = dspic33_load_program_word(cpu, 0u, opcode);
        reason = dspic33_step(cpu);
        if (encoded_address >= 0xe000u) {
            matches = matches && reason == DSPIC33_TRAPPED && cpu->pc == 0x000340u &&
                      cpu->last_trap == 1u && cpu->last_trap_return == 2u;
        } else {
            matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                      cpu->last_trap == UINT16_MAX;
        }
        matches = matches && cpu->cycles > cycles && cpu->unsupported_opcode == 0u &&
                  !cpu->illegal_reset;
        if (address >= 0x1000u && encoded_address < 0xe000u) {
            matches = matches &&
                      (store ? dspic33_read_word(cpu, address) == transfer_value
                             : cpu->w[reg] == (reg == 15u ? (transfer_value & 0xfffeu)
                                                          : transfer_value));
        }
        expect_dsp_matrix_case(state, matches, opcode, "direct data MOV encoding");
        cases++;
    }
    expect(state, cases == 1048576u, "direct data MOV encoding matrix is exhaustive");
}

static uint16_t move_matrix_logic_status(uint16_t status, uint16_t value,
                                         bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    value &= mask;
    status &= (uint16_t)~0x000au;
    if (value == 0u) {
        status |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        status |= 0x0008u;
    }
    return status;
}

static uint16_t move_matrix_file_value(uint16_t address, bool byte_mode) {
    switch (address & 3u) {
    case 0u:
        return 0u;
    case 1u:
        return byte_mode ? 0x0080u : 0x8000u;
    default:
        return byte_mode ? 0x0034u : 0x1234u;
    }
}

static void file_move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    uint32_t cases = 0u;
    uint16_t address;
    uint8_t byte_mode;

    dspic33_reset(cpu, 0u);
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_load_program_word(cpu, 0x000006u, 0x000340u),
           "load file MOV address trap vector");
    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        for (address = 0u; address < 0x2000u; address++) {
            uint32_t opcode = 0xb7a000u | ((uint32_t)byte_mode << 14u) | address;
            uint64_t cycles;
            Dspic33StopReason reason;
            bool matches;

            dspic33_reset(cpu, 0u);
            dspic33_set_async_events(cpu, false);
            prepare_move_matrix_case(cpu);
            dspic33_set_working_register(cpu, 0u, address >= 0x1000u ? 0xa5a5u : 0u);
            dspic33_set_working_register(cpu, 15u, 0x5000u);
            cycles = cpu->cycles;
            matches = dspic33_load_program_word(cpu, 0u, opcode);
            reason = dspic33_step(cpu);
            if (byte_mode == 0u && (address & 1u) != 0u) {
                matches = matches && reason == DSPIC33_TRAPPED &&
                          cpu->pc == 0x000340u && cpu->last_trap == 1u &&
                          cpu->last_trap_return == 2u;
            } else {
                matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                          cpu->last_trap == UINT16_MAX;
            }
            matches = matches && cpu->cycles > cycles &&
                      cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
            if (address >= 0x1000u && (byte_mode != 0u || (address & 1u) == 0u)) {
                matches = matches && (byte_mode != 0u
                                          ? dspic33_read_byte(cpu, address) == 0xa5u
                                          : dspic33_read_word(cpu, address) == 0xa5a5u);
            }
            expect_dsp_matrix_case(state, matches, opcode, "WREG-to-file MOV encoding");
            cases++;
        }
    }
    expect(state, cases == 16384u, "WREG-to-file MOV encoding matrix is exhaustive");

    cases = 0u;
    for (byte_mode = 0u; byte_mode < 2u; byte_mode++) {
        uint8_t file_destination;
        for (file_destination = 0u; file_destination < 2u; file_destination++) {
            for (address = 0u; address < 0x2000u; address++) {
                uint32_t opcode = 0xbf8000u | ((uint32_t)byte_mode << 14u) |
                                  ((uint32_t)file_destination << 13u) | address;
                uint16_t expected[16];
                uint16_t value;
                uint16_t expected_status;
                uint64_t cycles;
                Dspic33StopReason reason;
                bool matches;

                dspic33_reset(cpu, 0u);
                dspic33_set_async_events(cpu, false);
                prepare_move_registers(cpu, expected, 0x2000u, 2u);
                value = move_matrix_file_value(address, byte_mode != 0u);
                if (address >= 0x1000u) {
                    if (byte_mode != 0u) {
                        dspic33_write_byte(cpu, address, (uint8_t)value);
                    } else {
                        dspic33_write_word(cpu, address, value);
                    }
                } else if (address == 0x002eu) {
                    value = 2u;
                } else if (address == 0x0030u) {
                    value = 0u;
                } else {
                    value = byte_mode != 0u ? dspic33_read_byte(cpu, address)
                                            : dspic33_read_word(cpu, address);
                }
                expected_status =
                    move_matrix_logic_status(0x010fu, value, byte_mode != 0u);
                if (file_destination == 0u) {
                    expected[0] = byte_mode != 0u
                                      ? (uint16_t)((expected[0] & 0xff00u) | value)
                                      : value;
                }
                cycles = cpu->cycles;
                matches = dspic33_load_program_word(cpu, 0u, opcode);
                reason = dspic33_step(cpu);
                if (byte_mode == 0u && (address & 1u) != 0u) {
                    matches = matches && reason == DSPIC33_TRAPPED &&
                              cpu->pc == 0x000340u && cpu->last_trap == 1u &&
                              cpu->last_trap_return == 2u;
                } else {
                    matches = matches && reason == DSPIC33_RUNNING && cpu->pc == 2u &&
                              cpu->last_trap == UINT16_MAX &&
                              cpu->sr == expected_status &&
                              move_matrix_registers_match(cpu, expected);
                }
                matches = matches && cpu->cycles > cycles &&
                          cpu->unsupported_opcode == 0u && !cpu->illegal_reset;
                expect_dsp_matrix_case(state, matches, opcode,
                                       "file-to-destination MOV encoding");
                cases++;
            }
        }
    }
    expect(state, cases == 32768u,
           "file-to-destination MOV encoding matrix is exhaustive");
}

static void move_encoding_matrix_cases(TestState* state, Dspic33* cpu) {
    move_literal_encoding_matrix_cases(state, cpu);
    move_register_encoding_matrix_cases(state, cpu);
    movpag_encoding_matrix_cases(state, cpu);
    generic_move_encoding_matrix_cases(state, cpu);
    offset_move_encoding_matrix_cases(state, cpu);
    move_double_encoding_matrix_cases(state, cpu);
    move_data_encoding_matrix_cases(state, cpu);
    file_move_encoding_matrix_cases(state, cpu);
}

static void prepare_flash_read_erratum_case(TestState* state, Dspic33* cpu,
                                            uint32_t start) {
    reset_processor_test(cpu, start);
    load_instruction(state, cpu, start, OPCODE_MOV_DOUBLE_W1_POST_INCREMENT_W2);
    expect(state,
           dspic33_load_program_word(cpu, 0x1000u, 0x001000u) &&
               dspic33_load_program_word(cpu, 0x1002u, 0u),
           "load B1 Flash-read erratum PSV MOV.D data");
    dspic33_set_working_register(cpu, 1u, 0x9000u);
    cpu->dsrpag = 0x0200u;
    cpu->tblpag = 0u;
}

static void flash_read_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t flash_read_pairs[][2] = {
        {OPCODE_MOV_W1_W2, OPCODE_MOV_W1_W2},
        {OPCODE_TBLRDL_W2_W3, OPCODE_MOV_W1_W2},
    };
    Dspic33 copy;
    bool initialized;
    size_t index;

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "B1 back-to-back Flash-read sequence reports an undefined silicon result");

    for (index = 0u; index < sizeof(flash_read_pairs) / sizeof(flash_read_pairs[0]);
         index++) {
        prepare_flash_read_erratum_case(state, cpu, 0x200u);
        load_instruction(state, cpu, 0x202u, OPCODE_NOP);
        load_instruction(state, cpu, 0x204u, OPCODE_NOP);
        load_instruction(state, cpu, 0x206u, flash_read_pairs[index][0]);
        load_instruction(state, cpu, 0x208u, flash_read_pairs[index][1]);
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
               "B1 PSV and mixed back-to-back Flash reads share the erratum boundary");
    }

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_NOP);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "separating Flash reads with a NOP applies the documented workaround");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, 0x370000u);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "BRA to the next instruction applies the documented flow workaround");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, 0x090001u);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "REPEAT-ended connecting code does not arm the B1 Flash-read erratum");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0300u, OPCODE_NOP);
    load_instruction(state, cpu, 0x0302u, OPCODE_RETFIE);
    cpu->w[15] = 0x5000u;
    dspic33_write_word(cpu, 0x0820u, 0x0001u);
    dspic33_write_word(cpu, 0x0840u, 0x0001u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING,
           "B1 Flash-read connecting code reaches the interrupt boundary");
    dspic33_raise_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0302u &&
               !cpu->flash_read_erratum_armed,
           "interrupt vectoring cancels the B1 Flash-read sequence");
    dspic33_write_word(cpu, 0x0800u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x0204u &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "post-interrupt Flash reads execute outside the cancelled erratum sequence");

    prepare_flash_read_erratum_case(state, cpu, 0x202u);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_NOP);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x20au, OPCODE_TBLRDL_W2_W3);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING,
           "misaligned PSV MOV.D does not arm the B1 Flash-read erratum");

    prepare_flash_read_erratum_case(state, cpu, 0x200u);
    load_instruction(state, cpu, 0x202u, OPCODE_NOP);
    load_instruction(state, cpu, 0x204u, OPCODE_NOP);
    load_instruction(state, cpu, 0x206u, OPCODE_TBLRDL_W2_W3);
    load_instruction(state, cpu, 0x208u, OPCODE_TBLRDL_W2_W3);
    expect(
        state,
        dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
            dspic33_step(cpu) == DSPIC33_RUNNING &&
            dspic33_step(cpu) == DSPIC33_RUNNING && cpu->flash_read_erratum_candidate,
        "first qualifying Flash read arms the B1 erratum boundary");
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize B1 Flash-read erratum copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy armed B1 Flash-read state");
        expect(state,
               dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED &&
                   dspic33_step(&copy) == DSPIC33_SILICON_RESULT_UNDEFINED,
               "copied B1 Flash-read state retains the same sequence boundary");
        dspic33_destroy(&copy);
    }

    dspic33_reset(cpu, 0x200u);
    expect(state,
           !cpu->flash_read_erratum_armed && !cpu->flash_read_erratum_candidate &&
               cpu->flash_read_connecting_words == 0u &&
               !cpu->flash_read_connecting_ends_repeat,
           "reset clears B1 Flash-read sequence state");
}

static void do_flash_access_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t opcodes[] = {
        OPCODE_TBLRDL_W2_W3,
        OPCODE_TBLWTL_W2_W3,
        OPCODE_MOV_W1_W2,
    };
    size_t index;
    for (index = 0u; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        uint32_t boundary;
        for (boundary = 0x0200u; boundary <= 0x0204u; boundary += 4u) {
            reset_processor_test(cpu, boundary);
            load_instruction(state, cpu, boundary, opcodes[index]);
            cpu->do_depth = 1u;
            cpu->do_start[0] = 0x0200u;
            cpu->do_end[0] = 0x0204u;
            cpu->do_count[0] = 1u;
            cpu->dostart = 0x0200u;
            cpu->doend = 0x0204u;
            cpu->dcount = 1u;
            dspic33_set_working_register(cpu, 1u, 0xc000u);
            dspic33_set_working_register(cpu, 2u, 0u);
            dspic33_set_working_register(cpu, 3u, 0x1357u);
            cpu->dsrpag = 0x0200u;
            expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
                   "B1 DO boundary Flash access reports an undefined silicon result");
        }
    }

    reset_processor_test(cpu, 0x0202u);
    load_instruction(state, cpu, 0x0202u, OPCODE_TBLRDL_W2_W3);
    cpu->do_depth = 1u;
    cpu->do_start[0] = 0x0200u;
    cpu->do_end[0] = 0x0204u;
    cpu->do_count[0] = 1u;
    cpu->dostart = 0x0200u;
    cpu->doend = 0x0204u;
    cpu->dcount = 1u;
    dspic33_set_working_register(cpu, 2u, 0u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "Flash access inside a DO body remains defined away from both boundaries");
}

static void illegal_condition_reset_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t preserved_addresses[] = {
        0x0742u, 0x0744u, 0x0746u, 0x0748u, 0x0758u, 0x075au,
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
    dspic33_write_word(cpu, 0x074eu, 0x8500u);
    cpu->io.adc[3] = 0x0456u;
    cpu->io.gpio[2] = 0x789au;
    cpu->io.uart_cts = 0x05u;
    cpu->io.spi_selected = 0x09u;
    cpu->io.timer_gate = 0x0105u;
    cpu->io.pwm_dead_time_inputs = 0x25u;
    cpu->io.pwm_sync_inputs = 0x02u;
    cpu->io.pwm_fault_inputs = 0x81234567u;
    cpu->io.pwm_current_limit_inputs = 0x89abcdefu;
    cpu->io.pwm_dead_time_direct = 0x25u;
    cpu->io.pwm_sync_direct = 0x02u;
    cpu->io.pwm_fault_direct = 0x81234567u;
    cpu->io.pwm_current_limit_direct = 0x89abcdefu;
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
    expect(state, dspic33_read_word(cpu, 0x074eu) == 0u,
           "warm reset clears reference oscillator control");
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
               cpu->events.count == 1u && cpu->events.items[0].external &&
               cpu->events.items[0].type == DSPIC33_EVENT_UART &&
               cpu->events.items[0].cycle == 20u,
           "warm reset clears internal peripheral execution state");
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
               cpu->illegal_reset_count == 0u && cpu->cycles == 1u,
           "MOVPAG literal PP2 writes TBLPAG");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_INVALID_LITERAL);
    expect_illegal_reset(state, cpu, "MOVPAG literal PP3 resets processor");

    dspic33_reset(cpu, 0u);
    load_instruction(state, cpu, 0u, OPCODE_MOVPAG_TBL_W1);
    dspic33_set_working_register(cpu, 1u, 0x00a5u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->tblpag == 0x00a5u &&
               cpu->illegal_reset_count == 0u && cpu->cycles == 1u,
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

    dspic33_reset(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_MOV_BYTE_LITERAL_W1);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W1_W2);
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
    expect(state, !cpu->address_error_accumulator_state_completed,
           "illegal reset clears address error accumulator state");
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
               !cpu->address_error_accumulator_state_completed &&
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
    load_instruction(state, cpu, 0u, OPCODE_GOTO_W0);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "computed jump register is not an address-pointer tag use");

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

    dspic33_reset(cpu, 0x200u);
    load_instruction(state, cpu, 0x200u, OPCODE_PUSH_SHADOW);
    load_instruction(state, cpu, 0x202u, OPCODE_MOV_W0_W2);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->illegal_reset_count == 0u,
           "PUSH.S values are not address-pointer tag uses");
    expect_illegal_reset(state, cpu, "PUSH.S does not initialize W0 pointer");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize processor test");
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
        instruction_cycle_cases(&state, &cpu);
        register_move_instruction_cases(&state, &cpu);
        direct_file_move_cases(&state, &cpu);
        move_double_mode_cases(&state, &cpu);
        non_cpu_sfr_timing_cases(&state, &cpu);
        psv_timing_cases(&state, &cpu);
        psv_repeat_timing_cases(&state, &cpu);
        flash_read_erratum_cases(&state, &cpu);
        do_flash_access_erratum_cases(&state, &cpu);
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
        loop_encoding_matrix_cases(&state, &cpu);
        bit_encoding_matrix_cases(&state, &cpu);
        direct_file_bit_value_cases(&state, &cpu);
        bit_operand_lifecycle_cases(&state, &cpu);
        table_encoding_matrix_cases(&state, &cpu);
        table_value_cases(&state, &cpu);
        table_operand_lifecycle_cases(&state, &cpu);
        system_encoding_matrix_cases(&state, &cpu);
        system_control_value_cases(&state, &cpu);
        divide_encoding_matrix_cases(&state, &cpu);
        decimal_adjust_cases(&state, &cpu);
        arithmetic_encoding_matrix_cases(&state, &cpu);
        shift_encoding_matrix_cases(&state, &cpu);
        byte_extension_encoding_matrix_cases(&state, &cpu);
        byte_extension_value_matrix_cases(&state, &cpu);
        byte_extension_lifecycle_cases(&state, &cpu);
        direct_stack_encoding_matrix_cases(&state, &cpu);
        direct_stack_value_cases(&state, &cpu);
        link_encoding_matrix_cases(&state, &cpu);
        shadow_stack_encoding_cases(&state, &cpu);
        general_unary_encoding_matrix_cases(&state, &cpu);
        compare_encoding_matrix_cases(&state, &cpu);
        conditional_branch_encoding_matrix_cases(&state, &cpu);
        computed_control_encoding_matrix_cases(&state, &cpu);
        literal_control_encoding_matrix_cases(&state, &cpu);
        return_encoding_matrix_cases(&state, &cpu);
        direct_file_arithmetic_encoding_matrix_cases(&state);
        direct_file_logical_encoding_matrix_cases(&state, &cpu);
        direct_file_unary_encoding_matrix_cases(&state);
        dsp_encoding_matrix_cases(&state, &cpu);
        generic_multiply_encoding_matrix_cases(&state, &cpu);
        file_multiply_encoding_matrix_cases(&state, &cpu);
        move_encoding_matrix_cases(&state, &cpu);
        illegal_condition_reset_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[processor-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32
           "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
