#ifndef DSPIC33_INPUT_CAPTURE_TEST_INTERNAL_H
#define DSPIC33_INPUT_CAPTURE_TEST_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

static const uint8_t capture_irqs[DSPIC33_INPUT_CAPTURE_COUNT] = {
    1u, 5u, 37u, 38u, 39u, 40u, 22u, 23u, 93u, 125u, 127u, 129u, 135u, 137u, 139u, 141u};

enum {
    CAPTURE_BASE = 0x0140u,
    CAPTURE_STRIDE = 0x0008u,
    CAPTURE_FP_RISING = 0x1c03u,
    CAPTURE_32_BIT = 0x0100u,
    CAPTURE_TRIGGER = 0x00c0u,
    CAPTURE_NOT_EMPTY = 0x0008u,
    CAPTURE_OVERFLOW = 0x0010u,
    CAPTURE_DMA_DESTINATION = 0x3000u,
    CAPTURE_VECTOR = 0x0200u,
    CAPTURE_PMD_LOW = 0x0762u,
    CAPTURE_PMD_HIGH = 0x0768u,
    COMPARE_BASE = 0x0900u,
    COMPARE_STRIDE = 0x000au,
    COMPARATOR_BASE = 0x0a84u,
    COMPARATOR_STRIDE = 0x0008u
};

static const uint16_t timer_registers[5] = {0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u};
static const uint16_t timer_periods[5] = {0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu};
static const uint16_t timer_controls[5] = {0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u};
static const uint16_t capture_timer_sources[5] = {0x1000u, 0x0400u, 0x0000u, 0x0800u, 0x0c00u};

bool dspic33_input_capture_test_rising_edge(Dspic33* cpu, uint8_t channel);
uint16_t dspic33_input_capture_test_capture_base(uint8_t channel);
void dspic33_input_capture_test_access_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_configure_capture_source(Dspic33* cpu, uint8_t channel,
                                                         uint16_t timer_source, bool triggered,
                                                         bool running, uint8_t sync_source,
                                                         bool paired);
void dspic33_input_capture_test_configure_comparator_source(Dspic33* cpu, uint8_t comparator);
void dspic33_input_capture_test_configure_compare_source(Dspic33* cpu, uint8_t channel);
void dspic33_input_capture_test_configure_timer_source(Dspic33* cpu, uint8_t timer, uint16_t period,
                                                       uint16_t prescale);
void dspic33_input_capture_test_dma_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_fifo_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_interrupt_rate_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_mode_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_paired_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_pmd_channel_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_pmd_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_power_cases(TestState* state, Dspic33* cpu);
void dspic33_input_capture_test_zero_interval_overflow_cases(TestState* state, Dspic33* cpu);

#endif
