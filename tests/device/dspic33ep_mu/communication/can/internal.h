#ifndef DSPIC33_CAN_TEST_INTERNAL_H
#define DSPIC33_CAN_TEST_INTERNAL_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

static inline void dspic33_can_test_reset_can_raw(Dspic33* cpu, uint32_t entry) {
    dspic33_reset(cpu, entry);
}

static inline void dspic33_can_test_reset(Dspic33* cpu, uint32_t entry) {
    dspic33_can_test_reset_can_raw(cpu, entry);
    dspic33_gpio_drive(cpu, 3u, 0xffffu, 0xffffu);
}

#define dspic33_reset dspic33_can_test_reset

extern const uint16_t bases[DSPIC33_CAN_COUNT];
extern const uint8_t event_irqs[DSPIC33_CAN_COUNT];
extern const uint8_t receive_requests[DSPIC33_CAN_COUNT];
extern const uint8_t transmit_requests[DSPIC33_CAN_COUNT];

enum {
    CAN_INTERRUPT_ERROR = 0x0020u,
    CAN_ERROR_WARNING = 0x0100u,
    CAN_RECEIVE_WARNING = 0x0200u,
    CAN_TRANSMIT_WARNING = 0x0400u,
    CAN_RECEIVE_PASSIVE = 0x0800u,
    CAN_TRANSMIT_PASSIVE = 0x1000u,
    CAN_BUS_OFF = 0x2000u,
    CAN_ERROR_STATUS_MASK = 0x3f00u,
    FIFO_RELATION_THRESHOLD = 0u,
    FIFO_RELATION_EQUAL = 1u,
    FIFO_RELATION_DISTANT = 2u
};

bool dspic33_can_test_bridge_can_pins(Dspic33* cpu, uint8_t transmit_channel, uint8_t pin,
                                      uint8_t acknowledge_pin, uint64_t bit_cycles, int corrupt_bit,
                                      bool* acknowledge_observed);
bool dspic33_can_test_interrupt_flag(Dspic33* cpu, uint8_t irq);
bool dspic33_can_test_receive_full(Dspic33* cpu, uint8_t channel, uint8_t buffer);
Dspic33CanFrame dspic33_can_test_frame(uint32_t identifier, bool extended, bool remote,
                                       uint8_t length, uint8_t seed);
uint16_t dspic33_can_test_memory_word(Dspic33* cpu, uint32_t address);
uint64_t dspic33_can_test_mode_transition_cycles(Dspic33* cpu, uint8_t channel);
void dspic33_can_test_acknowledge_error_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_arbitration_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_arbitration_field_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_bus_off_recovery_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_capture_timestamp_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_clear_interrupt_flag(Dspic33* cpu, uint8_t irq);
void dspic33_can_test_clock_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_configure_filter(Dspic33* cpu, uint8_t channel, uint8_t filter,
                                       uint32_t identifier, bool extended, uint32_t mask,
                                       bool match_type, uint8_t buffer, uint8_t mask_index);
void dspic33_can_test_configure_receive(Dspic33* cpu, uint8_t channel, uint32_t memory,
                                        uint8_t dmabs, uint8_t fifo_start);
void dspic33_can_test_configure_transmit(Dspic33* cpu, uint8_t channel, uint32_t memory);
void dspic33_can_test_copy_and_reset_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_devicenet_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_direct_buffer_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_enable_filter(Dspic33* cpu, uint8_t channel, uint16_t enabled);
void dspic33_can_test_error_counter_recovery_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_extended_filter_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_fifo_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_fifo_control_write_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_fifo_interrupt_boundary_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_fifo_overflow_advancement_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_interrupt_and_error_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_interrupt_flag_write_zero_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_invalid_message_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_mode_and_power_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_mode_transition_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_overflow_and_fallback_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_overload_frame_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_payload_and_remote_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_physical_debug_mode_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_priority_and_abort_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_error_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_flag_hardware_event_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_flag_read_pointer_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_flag_write_zero_domain(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_overflow_write_zero_prior_domain(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_pps_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_pps_qualification_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_register_access_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_register_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_request_mode(Dspic33* cpu, uint8_t channel, uint8_t mode);
void dspic33_can_test_resynchronization_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_select_window(Dspic33* cpu, uint8_t channel, bool filter);
void dspic33_can_test_set_mode(Dspic33* cpu, uint8_t channel, uint8_t mode);
void dspic33_can_test_standard_filter_domain(TestState* state, Dspic33* cpu);
void dspic33_can_test_stuffed_frame_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_transmission_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_transmit_abort_timing_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_transmit_error_variant_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_transmit_pps_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_triple_sample_cases(TestState* state, Dspic33* cpu);
void dspic33_can_test_write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value);
void dspic33_can_test_write_transmit_frame(Dspic33* cpu, uint32_t memory,
                                           const Dspic33CanFrame* value);
void dspic33_can_test_bus_groups(TestState* state, Dspic33* cpu);
void dspic33_can_test_boundary_groups(TestState* state, Dspic33* cpu);
void dspic33_can_test_error_groups(TestState* state, Dspic33* cpu);
void dspic33_can_test_receive_groups(TestState* state, Dspic33* cpu);
void dspic33_can_test_register_groups(TestState* state, Dspic33* cpu);

#endif
