#ifndef DSPIC33_PMP_TEST_INTERNAL_H
#define DSPIC33_PMP_TEST_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    PMP_CONTROL = 0x0600u,
    PMP_MODE = 0x0602u,
    PMP_ADDRESS = 0x0604u,
    PMP_OUTPUT_2 = 0x0606u,
    PMP_DATA = 0x0608u,
    PMP_INPUT_2 = 0x060au,
    PMP_ADDRESS_ENABLE = 0x060cu,
    PMP_STATUS = 0x060eu,
    PMP_PMD = 0x0764u,
    PMP_ENABLE = 0x8000u,
    PMP_STOP_IDLE = 0x2000u,
    PMP_READ_STROBE_ENABLE = 0x0100u,
    PMP_WRITE_STROBE_ENABLE = 0x0200u,
    PMP_CHIP_SELECT_ENABLE = 0x4000u,
    PMP_ADDRESS_INPUT_ENABLE = 0x0003u,
    PMP_BUSY = 0x8000u,
    PMP_INTERRUPT_EACH = 0x2000u,
    PMP_INTERRUPT_RESERVED = 0x4000u,
    PMP_INTERRUPT_LAST = 0x6000u,
    PMP_INCREMENT = 0x0800u,
    PMP_DECREMENT = 0x1000u,
    PMP_DATA_16_BIT = 0x0400u,
    PMP_SLAVE_ADDRESSABLE = 0x0100u,
    PMP_MASTER_MODE_2 = 0x0200u,
    PMP_MASTER_MODE_3 = 0x0300u,
    PMP_BUFFERED_SLAVE = 0x1800u,
    PMP_PARTIAL_MUX = 0x0800u,
    PMP_FULL_MUX = 0x1000u,
    PMP_ONE_CHIP_SELECT = 0x0040u,
    PMP_TWO_CHIP_SELECTS = 0x0080u,
    PMP_FIRMWARE_MODE = 0x22beu,
    PMP_INTERRUPT_FLAG = 0x2000u,
    PMP_INTERRUPT_ENABLE = 0x2000u,
    PMP_IRQ = 45u,
    PMP_PRIORITY = 3u,
    PMP_VECTOR = 0x0100u,
    PMP_DMA_REQUEST = 0x2du,
    PMP_DMA_CHANNEL = 10u,
    PMP_DMA_BASE = 0x0ba0u,
    PMP_DMA_SOURCE = 0x2000u,
    PMP_TRANSFER_COUNT = 8192u,
    PMP_MODULE_DISABLE = 0x0100u,
    PMP_INPUT_FULL = 0x8000u,
    PMP_INPUT_OVERFLOW = 0x4000u,
    PMP_INPUT_BUFFER_MASK = 0x0f00u,
    PMP_OUTPUT_EMPTY = 0x0080u,
    PMP_OUTPUT_UNDERFLOW = 0x0040u,
    PMP_OUTPUT_BUFFER_MASK = 0x000fu,
    OPCODE_POWER_SAVE_SLEEP = 0xfe4000u,
    OPCODE_RESET = 0xfe0000u
};

uint16_t dspic33_pmp_test_raw_data_word(const Dspic33* cpu, uint16_t address);
void dspic33_pmp_test_access_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_boundary_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_access_lane_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_address_update_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_addressable_slave_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_buffered_slave_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_configure_dma(Dspic33* cpu, uint8_t channel, uint8_t request, uint32_t source,
                                    uint16_t pad, uint16_t count);
void dspic33_pmp_test_configure_pmp_control(Dspic33* cpu, uint16_t control, uint16_t mode,
                                            uint16_t address);
void dspic33_pmp_test_configure_pmp_read(Dspic33* cpu, uint16_t control, uint16_t mode,
                                         uint16_t address, uint16_t previous);
void dspic33_pmp_test_configure_pmp_slave(Dspic33* cpu, uint16_t control, uint16_t mode);
void dspic33_pmp_test_configure_pmp(Dspic33* cpu, uint16_t mode, uint16_t address);
void dspic33_pmp_test_dma_chain_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_dma_negative_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_interrupt_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_legacy_slave_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_master_read_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_master_read_pipeline_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_master_write_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_pmp_extended_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_power_management_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_read_address_update_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_read_interrupt_dma_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_read_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_read_wait_state_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_sixteen_bit_lane_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_slave_dma_isolation_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_slave_mode_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_slave_power_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_state_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_pmp_test_wait_state_matrix_cases(TestState* state, Dspic33* cpu);

#endif
