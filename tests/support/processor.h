#ifndef DSPIC33EP_MU_PROCESSOR_TEST_SUPPORT_H
#define DSPIC33EP_MU_PROCESSOR_TEST_SUPPORT_H

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/data.h"
#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
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
    OPCODE_DSP_MOVSAC_W4_COLLISION = 0xc70046u,
    OPCODE_ACCUMULATOR_ADD_A_AND_B = 0xcb0000u,
    OPCODE_ACCUMULATOR_NEGATE_B = 0xcb9000u,
    OPCODE_ACCUMULATOR_SUBTRACT_B_FROM_A = 0xcb3000u,
    OPCODE_ACCUMULATOR_STORE_A_W2 = 0xcc0002u,
    OPCODE_ACCUMULATOR_ROUNDED_STORE_A_W2 = 0xcd0002u,
    OPCODE_ACCUMULATOR_SHIFTED_STORE_A_W4 = 0xcc0784u,
    OPCODE_MOV_W1_W2_BIT_REVERSED_INCREMENT = 0x781901u
};

static inline void load_instruction(TestState* state, Dspic33* cpu, uint32_t address,
                                    uint32_t opcode) {
    expect(state, dspic33_load_program_word(cpu, address, opcode), "load processor instruction");
}

static inline void expect_step_cycles(TestState* state, Dspic33* cpu, uint64_t expected_cycles,
                                      const char* name) {
    uint64_t before = cpu->cycles;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->cycles - before == expected_cycles,
           name);
}

static inline void reset_processor_test(Dspic33* cpu, uint32_t entry) {
    uint8_t reg;
    dspic33_reset(cpu, entry);
    for (reg = 0u; reg < 15u; reg++) {
        dspic33_set_working_register(cpu, reg, cpu->w[reg]);
    }
}

static inline void expect_illegal_reset(TestState* state, Dspic33* cpu, const char* execution) {
    uint8_t reg;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, execution);
    expect(state,
           cpu->illegal_reset && cpu->illegal_reset_count == 1u &&
               cpu->software_reset_count == 0u && cpu->pc == 0u,
           "illegal condition performs warm reset");
    expect(state,
           (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u && cpu->last_trap == UINT16_MAX &&
               cpu->trap_count == 0u,
           "illegal condition records reset without trap");
    for (reg = 0u; reg < 15u; reg++) {
        expect(state, cpu->w[reg] == 0u, "illegal reset clears working register");
    }
    expect(state, cpu->w[15] == 0x1000u && cpu->initialized_working_registers == 0x8000u,
           "illegal reset restores stack and initialization state");
}

static inline void prepare_trap_vectors(TestState* state, Dspic33* cpu) {
    load_instruction(state, cpu, 0x00000au, 0x000300u);
    load_instruction(state, cpu, 0x00000cu, 0x000320u);
    load_instruction(state, cpu, 0x000300u, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x000320u, OPCODE_NOP);
}

static inline void prepare_address_trap(TestState* state, Dspic33* cpu) {
    load_instruction(state, cpu, 0x000006u, 0x000340u);
    cpu->w[15] = 0x5000u;
    cpu->stop_on_trap = true;
}

static inline void expect_address_trap(TestState* state, Dspic33* cpu, const char* execution) {
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED, execution);
    expect(state, cpu->last_trap == 1u && cpu->last_trap_return == 2u && cpu->pc == 0x000340u,
           "address error enters hard trap");
    expect(state,
           (dspic33_read_word(cpu, 0x08c0u) & 0x0008u) != 0u &&
               dspic33_read_word(cpu, 0x08c8u) == 0x0e01u,
           "address error records status and priority");
}

static inline uint16_t active_pending_traps(const Dspic33* cpu) {
    uint16_t count = 0u;
    size_t index;
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active) {
            count++;
        }
    }
    return count;
}

static inline const Dspic33PendingSoftTrap* pending_trap(const Dspic33* cpu, uint16_t trap) {
    size_t index;
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active && cpu->pending_soft_traps[index].trap == trap) {
            return &cpu->pending_soft_traps[index];
        }
    }
    return NULL;
}

static inline void expect_dsp_matrix_case(TestState* state, bool condition, uint32_t opcode,
                                          const char* domain) {
    state->cases++;
    if (condition) {
        state->passed++;
        return;
    }
    state->failed++;
    printf("[processor-failed] %s opcode=%06" PRIx32 "\n", domain, opcode);
}

static inline void run_invalid_binary_matrix_case(TestState* state, Dspic33* cpu, uint32_t opcode) {
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
        dspic33_set_working_register(cpu, reg, (uint16_t)(0x5000u + (uint16_t)reg * 2u));
    }
    dspic33_write_word(cpu, 0x5000u, 0xa5a5u);
    illegal_resets = cpu->illegal_reset_count;
    matches = dspic33_load_program_word(cpu, 0u, opcode) && dspic33_step(cpu) == DSPIC33_RUNNING &&
              cpu->illegal_reset && cpu->illegal_reset_count == illegal_resets + 1u &&
              cpu->last_trap == UINT16_MAX && cpu->pc == 0u && cpu->w[15] == 0x1000u &&
              cpu->initialized_working_registers == 0x8000u &&
              (dspic33_read_word(cpu, 0x0740u) & 0x4000u) != 0u &&
              dspic33_read_word(cpu, 0x5000u) == 0xa5a5u;
    for (reg = 0u; reg < 15u; reg++) {
        matches = matches && cpu->w[reg] == 0u;
    }
    expect_dsp_matrix_case(state, matches, opcode, "illegal binary encoding");
}

#endif
