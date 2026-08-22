#ifndef DSPIC33_DCI_TEST_INTERNAL_H
#define DSPIC33_DCI_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    DCI_CONTROL1 = 0x0280u,
    DCI_CONTROL2 = 0x0282u,
    DCI_CONTROL3 = 0x0284u,
    DCI_STATUS = 0x0286u,
    DCI_TRANSMIT_SLOTS = 0x0288u,
    DCI_RECEIVE_SLOTS = 0x028cu,
    DCI_RECEIVE_BASE = 0x0290u,
    DCI_TRANSMIT_BASE = 0x0298u,
    DCI_ENABLE = 0x8000u,
    DCI_STOP_IDLE = 0x2000u,
    DCI_LOOPBACK = 0x0800u,
    DCI_EXTERNAL_CLOCK = 0x0400u,
    DCI_EXTERNAL_FRAME = 0x0100u,
    DCI_UNDERFLOW_LAST = 0x0080u,
    DCI_TRISTATE = 0x0040u,
    DCI_DATA_JUSTIFY = 0x0020u,
    DCI_SAMPLE_RISING = 0x0200u,
    DCI_MODE_I2S = 0x0001u,
    DCI_MODE_AC_LINK_16 = 0x0002u,
    DCI_MODE_AC_LINK_20 = 0x0003u,
    DCI_RECEIVE_OVERFLOW = 0x0008u,
    DCI_RECEIVE_FULL = 0x0004u,
    DCI_TRANSMIT_UNDERFLOW = 0x0002u,
    DCI_TRANSMIT_EMPTY = 0x0001u,
    DCI_PMD = 0x0760u,
    DCI_PMD_MASK = 0x0100u,
    DCI_ERROR_IRQ = 59u,
    DCI_TRANSFER_IRQ = 60u,
    DCI_DMA_REQUEST = 0x3cu,
    DCI_VECTOR = 0x0200u,
    DCI_PPS_INPUTS = 0x06d0u,
    DCI_PPS_FRAME = 0x06d2u,
    DCI_PPS_DATA_OUTPUT = 0x0682u,
    DCI_PPS_CLOCK_FRAME_OUTPUT = 0x0684u,
    GPIO_TRIS_D = 0x0e30u,
    GPIO_ANALOG_D = 0x0e3eu,
    GPIO_PORT_D = 3u,
    GPIO_DATA_MASK = 0x0001u,
    GPIO_CLOCK_MASK = 0x0002u,
    GPIO_FRAME_MASK = 0x0004u,
    GPIO_ANALOG_CLOCK_MASK = 0x0040u,
    PPS_DATA_PIN = 64u,
    PPS_CLOCK_PIN = 65u,
    PPS_FRAME_PIN = 66u,
    PPS_DATA_OUTPUT_PIN = 67u,
    PPS_CLOCK_OUTPUT_PIN = 68u,
    PPS_FRAME_OUTPUT_PIN = 69u,
    PPS_ANALOG_CLOCK_PIN = 70u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_IDLE = 0xfe4001u
};

bool dspic33_dci_test_activate_serial_clock(Dspic33* cpu, bool rising, uint16_t clock_mask);
bool dspic33_dci_test_clock_word(Dspic33* cpu, uint16_t value, bool frame_sync);
bool dspic33_dci_test_drive_internal_pin_slot(Dspic33* cpu, uint16_t value, uint8_t width,
                                              uint64_t start_delay);
bool dspic33_dci_test_drive_mapped_serial_word(Dspic33* cpu, uint16_t value, uint8_t width,
                                               bool rising, uint16_t data_mask,
                                               uint16_t clock_mask);
bool dspic33_dci_test_drive_serial_bit(Dspic33* cpu, bool high, bool rising);
bool dspic33_dci_test_drive_serial_edge(Dspic33* cpu, bool high, bool rising, uint16_t clock_mask);
bool dspic33_dci_test_drive_serial_word(Dspic33* cpu, uint16_t value, uint8_t width, bool rising);
bool dspic33_dci_test_interrupt_set(Dspic33* cpu, uint8_t irq);
uint16_t dspic33_dci_test_configuration(uint8_t width, uint8_t slots, uint8_t buffers);
uint16_t dspic33_dci_test_serial_word_mask(uint8_t width);
void dspic33_dci_test_ac_link_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_access_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_admission_and_clock_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_clear_interrupt(Dspic33* cpu, uint8_t irq);
void dspic33_dci_test_configure_dma(Dspic33* cpu, uint8_t channel, uint16_t control,
                                    uint32_t memory, uint16_t pad, uint16_t count, uint8_t request);
void dspic33_dci_test_configure_external(Dspic33* cpu, uint16_t control, uint8_t width,
                                         uint8_t slots, uint8_t buffers, uint16_t transmit,
                                         uint16_t receive);
void dspic33_dci_test_configure_internal(Dspic33* cpu, uint16_t control, uint8_t width,
                                         uint8_t slots, uint8_t buffers, uint16_t transmit,
                                         uint16_t receive);
void dspic33_dci_test_configure_serial_pins(Dspic33* cpu);
void dspic33_dci_test_disable_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_enable_interrupt(Dspic33* cpu, uint8_t irq, uint8_t priority);
void dspic33_dci_test_generation_and_frame_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_internal_clock_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_interrupt_dma_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_mode_and_status_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_bcg_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_disable_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_external_frame_output_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_frame_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_internal_frame_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_internal_frame_output_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_internal_input_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_internal_sample_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_output_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_qualification_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_selection_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_serial_input_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_serial_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_pps_startup_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_protocol_frame_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_protocol_geometry_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_protocol_integration_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_slot_buffer_status_cases(TestState* state, Dspic33* cpu);
void dspic33_dci_test_width_and_lane_cases(TestState* state, Dspic33* cpu);

#endif
