#ifndef DSPIC33_SPI_TEST_INTERNAL_H
#define DSPIC33_SPI_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

static const uint16_t bases[DSPIC33_SPI_COUNT] = {0x0240u, 0x0260u, 0x02a0u, 0x02c0u};
static const uint8_t irqs[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};
static const uint8_t error_irqs[DSPIC33_SPI_COUNT] = {9u, 32u, 90u, 122u};
static const uint8_t requests[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};

bool dspic33_spi_test_interrupt_flag(Dspic33* cpu, uint8_t irq);
bool dspic33_spi_test_transfer_interrupt_after_cycle(Dspic33* cpu, uint8_t irq);
uint16_t dspic33_spi_test_dma_base(uint8_t channel);
uint64_t dspic33_spi_test_transfer_cycles(uint16_t control);
void dspic33_spi_test_b1_frame_output_cases(TestState* state, Dspic33* cpu, Dspic33* copy);
void dspic33_spi_test_boundary_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_clear_interrupt(Dspic33* cpu, uint8_t irq);
void dspic33_spi_test_configure_dma(Dspic33* cpu, uint8_t channel, uint16_t control,
                                    uint8_t request, uint32_t memory, uint16_t pad, uint16_t count);
void dspic33_spi_test_configure_spi(Dspic33* cpu, uint8_t channel, uint16_t control,
                                    uint16_t control2, uint8_t interrupt_mode);
void dspic33_spi_test_enhanced_fifo_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_interrupt_mode_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_master_input_cases(TestState* state, Dspic33* cpu, Dspic33* copy);
void dspic33_spi_test_master_output_cases(TestState* state, Dspic33* cpu, Dspic33* copy);
void dspic33_spi_test_mode_transition_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_physical_slave_input_cases(TestState* state, Dspic33* cpu, Dspic33* copy);
void dspic33_spi_test_pps_slave_input_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_receive_only_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_register_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_selection_and_frame_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_slave_select_retry_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_split_buffer_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_standard_buffer_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_timing_matrix_cases(TestState* state, Dspic33* cpu);
void dspic33_spi_test_transmit_output_cases(TestState* state, Dspic33* cpu);

#endif
