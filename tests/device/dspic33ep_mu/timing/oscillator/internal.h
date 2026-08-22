#ifndef DSPIC33_OSCILLATOR_TEST_INTERNAL_H
#define DSPIC33_OSCILLATOR_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    OSCILLATOR_CONTROL = 0x0742u,
    OSCILLATOR_SWITCH_ENABLE = 0x0001u,
    OSCILLATOR_LP_ENABLE = 0x0002u,
    OSCILLATOR_CLOCK_FAIL = 0x0008u,
    OSCILLATOR_PLL_LOCK = 0x0020u,
    OSCILLATOR_IO_LOCK = 0x0040u,
    OSCILLATOR_CLOCK_LOCK = 0x0080u,
    OSCILLATOR_SWITCH_DELAY = 32u,
    REFERENCE_CLOCK_CONTROL = 0x074eu,
    REFERENCE_CLOCK_ENABLE = 0x8000u,
    REFERENCE_CLOCK_SLEEP = 0x2000u,
    REFERENCE_CLOCK_SOURCE = 0x1000u,
    REFERENCE_CLOCK_DIVISOR = 0x0f00u,
    MAIN_CLOCK_DIVISOR = 0x0744u,
    MAIN_CLOCK_RECOVER_INTERRUPT = 0x8000u,
    MAIN_CLOCK_DOZE_MASK = 0x7000u,
    MAIN_CLOCK_DOZE_ENABLE = 0x0800u,
    MAIN_PLL_FEEDBACK = 0x0746u,
    MAIN_OSCILLATOR_TUNING = 0x0748u,
    TIMER1_COUNTER = 0x0100u,
    TIMER1_PERIOD = 0x0102u,
    TIMER1_CONTROL = 0x0104u,
    CRC_PMD_ADDRESS = 0x0764u,
    CRC_PMD = 0x0080u,
    CONFIGURATION_FOSCSEL = 0xf80006u,
    CONFIGURATION_FOSC = 0xf80008u,
    CONFIGURATION_FWDT = 0xf8000au,
    OPCODE_NOP = 0x000000u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_RESET = 0xfe0000u,
    OPCODE_GOTO_W0 = 0x010400u,
    OPCODE_MOV_IFS0_W2 = 0x804002u,
    OPCODE_MOV_W0_W1 = 0x780880u,
    OPCODE_MOV_BYTE_W0_W1 = 0x784880u,
    OPCODE_MOV_BYTE_W2_W1 = 0x784882u,
    OPCODE_MOV_BYTE_W3_W1 = 0x784883u
};

bool dspic33_oscillator_test_load_sequence(Dspic33* cpu, uint32_t first, uint32_t second,
                                           uint32_t third);
bool dspic33_oscillator_test_program_fosc(Dspic33* cpu, uint8_t value);
bool dspic33_oscillator_test_select_locked_main_pll(Dspic33* cpu, uint8_t source);
Dspic33StopReason dspic33_oscillator_test_write_protected_byte(Dspic33* cpu, uint16_t address,
                                                               uint8_t value);
uint16_t dspic33_oscillator_test_control(Dspic33* cpu);
void dspic33_oscillator_test_configuration_admission_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_doze_cases(TestState* state, Dspic33* source, Dspic33* copy);
void dspic33_oscillator_test_fail_safe_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_failure_trap_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_hardware_failure_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_main_pll_configuration_cases(TestState* state, Dspic33* source,
                                                          Dspic33* copy);
void dspic33_oscillator_test_oscillator_pin_cases(TestState* state, Dspic33* source, Dspic33* copy);
void dspic33_oscillator_test_pll_lock_sequence_cases(TestState* state, Dspic33* source,
                                                     Dspic33* copy);
void dspic33_oscillator_test_protection_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_reference_clock_cases(TestState* state, Dspic33* source,
                                                   Dspic33* copy);
void dspic33_oscillator_test_reference_clock_pin_cases(TestState* state, Dspic33* source,
                                                       Dspic33* copy);
void dspic33_oscillator_test_reset_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_source_admission_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_switch_cases(TestState* state, Dspic33* cpu);
void dspic33_oscillator_test_two_speed_startup_cases(TestState* state, Dspic33* source,
                                                     Dspic33* copy);

#endif
