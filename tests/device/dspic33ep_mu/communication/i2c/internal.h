#ifndef DSPIC33_I2C_TEST_INTERNAL_H
#define DSPIC33_I2C_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

static const uint16_t bases[DSPIC33_I2C_COUNT] = {0x0200u, 0x0210u};
static const uint8_t slave_irqs[DSPIC33_I2C_COUNT] = {16u, 49u};
static const uint8_t master_irqs[DSPIC33_I2C_COUNT] = {17u, 50u};

typedef struct {
    uint16_t configuration;
    uint8_t channel;
    uint8_t port;
    uint8_t clock;
    uint8_t data;
} I2cPinRoute;

static const I2cPinRoute pin_routes[] = {
    {0xffefu, 0u, 3u, 10u, 9u}, {0xffffu, 1u, 5u, 5u, 4u}, {0xffdfu, 1u, 0u, 2u, 3u}};

bool dspic33_i2c_test_interrupt_flag(Dspic33* cpu, uint8_t irq);
bool dspic33_i2c_test_pin_levels(const Dspic33* cpu, uint8_t port, uint8_t clock, uint8_t data,
                                 bool clock_high, bool data_high);
bool dspic33_i2c_test_pop_slave_acknowledgement(Dspic33* cpu, uint8_t channel, bool acknowledge);
uint16_t dspic33_i2c_test_stored_word(const Dspic33* cpu, uint16_t address);
uint64_t dspic33_i2c_test_byte_cycles(uint16_t baud);
uint64_t dspic33_i2c_test_control_cycles(uint16_t baud);
uint64_t dspic33_i2c_test_operation_cycles(uint16_t baud, uint8_t half_periods);
uint64_t dspic33_i2c_test_receive_cycles(uint16_t baud);
void dspic33_i2c_test_address_mode_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_address_rejection_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_bus_status_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_boundary_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_clear_interrupt(Dspic33* cpu, uint8_t irq);
void dspic33_i2c_test_configure_dma_channel(Dspic33* cpu, uint8_t channel, uint8_t request,
                                            uint16_t start, uint16_t pad);
void dspic33_i2c_test_disable_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_dma_isolation_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_drive_byte(const I2cPinRoute* route, Dspic33* cpu, uint8_t value);
void dspic33_i2c_test_drive_pin(const I2cPinRoute* route, Dspic33* cpu, bool clock, bool high);
void dspic33_i2c_test_enable_interrupt(Dspic33* cpu, uint8_t irq, uint8_t priority,
                                       uint16_t vector);
void dspic33_i2c_test_enable(Dspic33* cpu, uint8_t channel, uint16_t options, uint16_t baud);
void dspic33_i2c_test_isolation_and_power_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_master_error_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_master_pin_lifecycle_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_master_pin_sequence_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_master_sequence_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_pin_routing_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_pmd_transition_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_register_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_slave_acknowledgement_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_slave_pin_receive_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_slave_pin_rejection_and_transmit_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_slave_power_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_slave_receive_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_slave_transmit_cases(TestState* state, Dspic33* cpu);
void dspic33_i2c_test_timing_cases(TestState* state, Dspic33* cpu);

#endif
