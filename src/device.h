#ifndef DSPIC33E_SIM_DSPIC33_DEVICE_H
#define DSPIC33E_SIM_DSPIC33_DEVICE_H

#include "dspic33_internal.h"

void dspic33_device_reset(Dspic33* cpu);
void dspic33_device_power_on_reset(Dspic33* cpu);
void dspic33_device_reset_restored(Dspic33* cpu);
void dspic33_device_brown_out_reset(Dspic33* cpu);
void dspic33_device_write_byte(Dspic33* cpu, uint16_t address, uint16_t previous);
uint8_t dspic33_device_read_byte(Dspic33* cpu, uint16_t address, uint8_t value);
bool dspic33_device_advance(Dspic33* cpu, uint64_t cycles);
bool dspic33_device_advance_instruction(Dspic33* cpu, uint64_t cpu_cycles, uint64_t device_cycles);
uint64_t dspic33_device_instruction_cycles(const Dspic33* cpu, uint64_t cycles);
bool dspic33_device_advance_nvm(Dspic33* cpu);
bool dspic33_device_service_interrupt(Dspic33* cpu);
bool dspic33_device_interrupt_pending(const Dspic33* cpu);
bool dspic33_device_wake(Dspic33* cpu);
bool dspic33_device_dma_pad_valid(uint16_t pad, bool write);
bool dspic33_device_gpio_input_high(const Dspic33* cpu, uint8_t port, uint8_t bit, bool* high);
void dspic33_device_latch_interrupt(Dspic33* cpu, uint8_t vector, uint8_t priority);
void dspic33_device_latch_math_error(Dspic33* cpu, uint16_t cause);
void dspic33_device_return_interrupt(Dspic33* cpu);
void dspic33_device_abort_oscillator_switch(Dspic33* cpu);
void dspic33_device_configuration_changed(Dspic33* cpu, uint32_t address, uint8_t previous);
bool dspic33_watchdog_complete_nvm(Dspic33* cpu);
void dspic33_raise_dma_address_trap(Dspic33* cpu);
void dspic33_raise_dma_collision_trap(Dspic33* cpu);

#endif
