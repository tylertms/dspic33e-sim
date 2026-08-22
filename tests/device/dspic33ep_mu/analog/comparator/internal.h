#ifndef DSPIC33_COMPARATOR_TEST_INTERNAL_H
#define DSPIC33_COMPARATOR_TEST_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    COMPARATOR_STATUS = 0x0a80u,
    COMPARATOR_REFERENCE = 0x0a82u,
    COMPARATOR_BASE = 0x0a84u,
    COMPARATOR_STRIDE = 0x0008u,
    COMPARATOR_ENABLE = 0x8000u,
    COMPARATOR_OUTPUT_ENABLE = 0x4000u,
    COMPARATOR_POLARITY = 0x2000u,
    COMPARATOR_EVENT = 0x0200u,
    COMPARATOR_OUTPUT = 0x0100u,
    COMPARATOR_REFERENCE_EXTERNAL = 0x0400u,
    COMPARATOR_REFERENCE_ENABLE = 0x0080u,
    COMPARATOR_REFERENCE_LOW_RANGE = 0x0020u,
    COMPARATOR_REFERENCE_SOURCE_EXTERNAL = 0x0010u,
    COMPARATOR_FILTER_ENABLE = 0x0008u,
    COMPARATOR_STOP_IDLE = 0x8000u,
    COMPARATOR_PMD_ADDRESS = 0x0764u,
    COMPARATOR_PMD = 0x0400u,
    COMPARATOR_IRQ = 18u,
    COMPARATOR_FLAG_ADDRESS = 0x0802u,
    COMPARATOR_ENABLE_ADDRESS = 0x0822u,
    COMPARATOR_PRIORITY_ADDRESS = 0x0848u,
    COMPARATOR_INTERRUPT_BIT = 0x0004u,
    COMPARATOR_VECTOR = 0x0240u
};

static const uint16_t register_addresses[14] = {0x0a80u, 0x0a82u, 0x0a84u, 0x0a86u, 0x0a88u,
                                                0x0a8au, 0x0a8cu, 0x0a8eu, 0x0a90u, 0x0a92u,
                                                0x0a94u, 0x0a96u, 0x0a98u, 0x0a9au};

static const uint16_t register_writable[14] = {0x8000u, 0x07ffu, 0xe2d3u, 0x0fffu, 0xbfffu,
                                               0x007fu, 0xe2d3u, 0x0fffu, 0xbfffu, 0x007fu,
                                               0xe2d3u, 0x0fffu, 0xbfffu, 0x007fu};

static const Dspic33ComparatorInput negative_inputs[3] = {DSPIC33_COMPARATOR_INPUT_NEGATIVE_2,
                                                          DSPIC33_COMPARATOR_INPUT_NEGATIVE_1,
                                                          DSPIC33_COMPARATOR_INPUT_NEGATIVE_3};

bool dspic33_comparator_test_interrupt_flag(Dspic33* cpu);
bool dspic33_comparator_test_output_is(const Dspic33* cpu, uint8_t comparator, bool expected);
bool dspic33_comparator_test_prepare_relation(Dspic33* cpu, uint8_t comparator,
                                              Dspic33ComparatorInput negative, uint16_t positive,
                                              uint16_t negative_level);
bool dspic33_comparator_test_status_event(Dspic33* cpu, uint8_t comparator);
uint16_t dspic33_comparator_test_comparator_base(uint8_t comparator);
void dspic33_comparator_test_access_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_clear_event(Dspic33* cpu, uint8_t comparator);
void dspic33_comparator_test_clear_interrupt(Dspic33* cpu);
void dspic33_comparator_test_configure_comparator(Dspic33* cpu, uint8_t comparator,
                                                  uint16_t channel, bool inverted,
                                                  uint16_t event_polarity);
void dspic33_comparator_test_event_polarity_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_last_read_cout_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_power_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_pps_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_reference_ladder_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_selection_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_set_comparator_relation(Dspic33* cpu, uint8_t comparator,
                                                     uint16_t positive, uint16_t negative);
void dspic33_comparator_test_software_event_cases(TestState* state, Dspic33* cpu);
void dspic33_comparator_test_sticky_rearm_cases(TestState* state, Dspic33* cpu);

#endif
