#ifndef DSPIC33_QEI_TEST_INTERNAL_H
#define DSPIC33_QEI_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    QEI_ENABLE = 0x8000u,
    QEI_STOP_IDLE = 0x2000u,
    QEI_POSITION_MODE_SHIFT = 10u,
    QEI_INDEX_MATCH_SHIFT = 8u,
    QEI_DIVIDER_SHIFT = 4u,
    QEI_GATE_ENABLE = 0x0004u,
    QEI_DIRECTION_INVERT = 0x0008u,
    QEI_MODE_QUADRATURE = 0u,
    QEI_MODE_UP_DOWN = 1u,
    QEI_MODE_GATE = 2u,
    QEI_MODE_TIMER = 3u,
    QEI_CAPTURE_HOME = 0x8000u,
    QEI_FILTER_ENABLE = 0x4000u,
    QEI_OUTPUT_GREATER_EQUAL = 0x0200u,
    QEI_OUTPUT_LESS_EQUAL = 0x0400u,
    QEI_OUTPUT_OUTSIDE = 0x0600u,
    QEI_SWAP = 0x0100u,
    QEI_PHASE_A_POLARITY = 0x0010u,
    QEI_STATUS_INDEX_ENABLE = 0x0001u,
    QEI_STATUS_INDEX = 0x0002u,
    QEI_STATUS_HOME_ENABLE = 0x0004u,
    QEI_STATUS_HOME = 0x0008u,
    QEI_STATUS_VELOCITY_OVERFLOW_ENABLE = 0x0010u,
    QEI_STATUS_VELOCITY_OVERFLOW = 0x0020u,
    QEI_STATUS_INITIALIZED_ENABLE = 0x0040u,
    QEI_STATUS_INITIALIZED = 0x0080u,
    QEI_STATUS_POSITION_OVERFLOW_ENABLE = 0x0100u,
    QEI_STATUS_POSITION_OVERFLOW = 0x0200u,
    QEI_STATUS_LOW_COMPARE_ENABLE = 0x0400u,
    QEI_STATUS_LOW_COMPARE = 0x0800u,
    QEI_STATUS_HIGH_COMPARE_ENABLE = 0x1000u,
    QEI_STATUS_HIGH_COMPARE = 0x2000u,
    CAPTURE_BASE = 0x0140u,
    CAPTURE_FP_EVERY_EDGE = 0x1c01u,
    CAPTURE_FP_RISING = 0x1c03u,
    QEI_VECTOR = 0x0240u
};

static const uint16_t bases[DSPIC33_QEI_COUNT] = {0x01c0u, 0x05c0u};
static const uint16_t pmd_addresses[DSPIC33_QEI_COUNT] = {0x0760u, 0x0764u};
static const uint16_t pmd_masks[DSPIC33_QEI_COUNT] = {0x0400u, 0x0020u};
static const uint16_t interrupt_addresses[DSPIC33_QEI_COUNT] = {0x0806u, 0x0808u};
static const uint16_t interrupt_masks[DSPIC33_QEI_COUNT] = {0x0400u, 0x0800u};
static const uint16_t pps_input_registers[DSPIC33_QEI_COUNT][2] = {{0x06bcu, 0x06beu},
                                                                   {0x06c0u, 0x06c2u}};

bool dspic33_qei_test_input(Dspic33* cpu, uint8_t channel, Dspic33QeiInput source, bool high);
bool dspic33_qei_test_interrupt_set(Dspic33* cpu, uint8_t channel);
uint32_t dspic33_qei_test_read_counter(Dspic33* cpu, uint16_t low);
void dspic33_qei_test_clear_interrupt(Dspic33* cpu, uint8_t channel);
void dspic33_qei_test_configure_interrupt(Dspic33* cpu, uint8_t channel);
void dspic33_qei_test_divider_polarity_output_cases(TestState* state, Dspic33* cpu);
void dspic33_qei_test_external_mode_cases(TestState* state, Dspic33* cpu);
void dspic33_qei_test_interrupt_compare_index_cases(TestState* state, Dspic33* cpu);
void dspic33_qei_test_quadrature_cases(TestState* state, Dspic33* cpu);
void dspic33_qei_test_quadrature_transition_cases(TestState* state, Dspic33* cpu);
void dspic33_qei_test_register_cases(TestState* state, Dspic33* cpu);
void dspic33_qei_test_reset_qei(Dspic33* cpu);
void dspic33_qei_test_select_pps_input(Dspic33* cpu, uint8_t channel, uint8_t input, uint8_t pin);
void dspic33_qei_test_set_open_comparison_window(Dspic33* cpu, uint16_t base);
void dspic33_qei_test_timer_filter_power_cases(TestState* state, Dspic33* cpu);
void dspic33_qei_test_write_counter(Dspic33* cpu, uint16_t low, uint16_t hold, uint32_t value);

#endif
