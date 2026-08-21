#ifndef DSPIC33E_OUTPUT_COMPARE_TEST_SUPPORT_H
#define DSPIC33E_OUTPUT_COMPARE_TEST_SUPPORT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

static const uint8_t compare_irqs[DSPIC33_OUTPUT_COMPARE_COUNT] = {
    2u, 6u, 25u, 26u, 41u, 42u, 43u, 44u, 92u, 124u, 126u, 128u, 134u, 136u, 138u, 140u};

enum {
    COMPARE_BASE = 0x0900u,
    COMPARE_STRIDE = 0x000au,
    COMPARE_FP = 0x1c00u,
    COMPARE_FP_EDGE_PWM = 0x1c06u,
    COMPARE_NO_SYNC = 0x0000u,
    COMPARE_SELF_SYNC = 0x001fu,
    COMPARE_VECTOR = 0x0240u,
    COMPARE_TRIGGER = 0x0080u,
    COMPARE_TRIGGER_STATUS = 0x0040u,
    COMPARE_TRIGGER_ONESHOT = 0x0008u,
    COMPARE_CASCADE = 0x0100u,
    COMPARE_STOP_IDLE = 0x2000u,
    COMPARE_TRISTATE = 0x0020u,
    COMPARE_FAULT_ENABLE_A = 0x0080u,
    COMPARE_FAULT_ENABLE_B = 0x0100u,
    COMPARE_FAULT_ENABLE_C = 0x0200u,
    COMPARE_FAULT_STATUS_A = 0x0010u,
    COMPARE_FAULT_STATUS_B = 0x0020u,
    COMPARE_FAULT_STATUS_C = 0x0040u,
    COMPARE_FAULT_INACTIVE = 0x8000u,
    COMPARE_FAULT_OUTPUT = 0x4000u,
    COMPARE_FAULT_TRISTATE = 0x2000u,
    COMPARE_DMA_MEMORY = 0x3000u,
    COMPARE_DMA_PAD = 0x0904u,
    COMPARE_OPCODE_MOV_W0_INDIRECT_W1 = 0x780880u,
    COMPARE_OPCODE_RESET = 0xfe0000u,
    COMPARE_OPCODE_SLEEP = 0xfe4000u
};

static const uint16_t timer_registers[] = {0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u};
static const uint16_t timer_periods[] = {0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu};
static const uint16_t timer_controls[] = {0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u};

static inline uint16_t compare_base(uint8_t channel) {
    return (uint16_t)(COMPARE_BASE + channel * COMPARE_STRIDE);
}

static inline uint16_t compare_raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static inline uint16_t compare_pmd_address(uint8_t channel) {
    return channel < 8u ? 0x0762u : 0x0768u;
}

static inline uint16_t compare_pmd_mask(uint8_t channel) {
    return (uint16_t)(1u << (channel & 7u));
}

static inline bool interrupt_flag(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = compare_irqs[channel];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static inline void clear_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = compare_irqs[channel];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t bit = (uint16_t)(1u << (irq % 16u));
    dspic33_write_word(cpu, address, (uint16_t)(dspic33_read_word(cpu, address) & ~bit));
}

static inline bool output_is(const Dspic33* cpu, uint8_t channel, bool expected) {
    bool high;
    return dspic33_output_compare_output(cpu, channel, &high) && high == expected;
}

static inline bool pin_is(const Dspic33* cpu, uint8_t pin, bool expected) {
    bool high;
    return dspic33_output_compare_pin(cpu, pin, &high) && high == expected;
}

static inline uint16_t compare_fault_enable(uint8_t source) {
    return (uint16_t)(COMPARE_FAULT_ENABLE_A << source);
}

static inline uint16_t compare_fault_status(uint8_t source) {
    return (uint16_t)(COMPARE_FAULT_STATUS_A << source);
}

static inline bool drive_compare_fault(Dspic33* cpu, uint8_t source, bool high) {
    return dspic33_output_compare_fault(cpu, source, high, 0u) && dspic33_device_advance(cpu, 0u);
}

static inline void configure_compare_source(Dspic33* cpu, uint8_t channel, uint16_t period,
                                            uint16_t duty, uint16_t synchronization) {
    uint16_t base = compare_base(channel);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), period);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), duty);
    clear_interrupt(cpu, channel);
    dspic33_write_word(cpu, base, COMPARE_FP_EDGE_PWM);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), synchronization);
}

static inline void configure_compare_mode(Dspic33* cpu, uint8_t channel, uint8_t mode,
                                          uint16_t secondary, uint16_t primary, uint16_t control2) {
    uint16_t base = compare_base(channel);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), secondary);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), primary);
    clear_interrupt(cpu, channel);
    dspic33_write_word(cpu, base, (uint16_t)(COMPARE_FP | mode));
    dspic33_write_word(cpu, (uint16_t)(base + 2u), control2);
}

static inline void configure_compare(Dspic33* cpu, uint8_t channel, uint16_t period,
                                     uint16_t duty) {
    configure_compare_source(cpu, channel, period, duty, COMPARE_SELF_SYNC);
}

static inline void configure_cascade(Dspic33* cpu, uint8_t low, uint8_t mode, uint32_t secondary,
                                     uint32_t primary, uint16_t clock, uint16_t synchronization,
                                     bool trigger) {
    uint8_t high = (uint8_t)(low + 1u);
    uint16_t low_base = compare_base(low);
    uint16_t high_base = compare_base(high);
    uint16_t low_control2 = (uint16_t)(COMPARE_CASCADE | COMPARE_TRISTATE | synchronization |
                                       (trigger ? COMPARE_TRIGGER : 0u));
    uint16_t high_control2 = (uint16_t)(COMPARE_CASCADE | synchronization);
    dspic33_write_word(cpu, low_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(low_base + 2u), 0u);
    dspic33_write_word(cpu, high_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(high_base + 2u), 0u);
    dspic33_write_word(cpu, (uint16_t)(low_base + 4u), (uint16_t)secondary);
    dspic33_write_word(cpu, (uint16_t)(high_base + 4u), (uint16_t)(secondary >> 16u));
    dspic33_write_word(cpu, (uint16_t)(low_base + 6u), (uint16_t)primary);
    dspic33_write_word(cpu, (uint16_t)(high_base + 6u), (uint16_t)(primary >> 16u));
    clear_interrupt(cpu, low);
    clear_interrupt(cpu, high);
    dspic33_write_word(cpu, (uint16_t)(high_base + 2u), high_control2);
    dspic33_write_word(cpu, (uint16_t)(low_base + 2u), low_control2);
    dspic33_write_word(cpu, high_base, (uint16_t)(clock | mode));
    dspic33_write_word(cpu, low_base, (uint16_t)(clock | mode));
}

static inline void configure_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = compare_irqs[channel];
    uint16_t enable = (uint16_t)(0x0820u + (irq / 16u) * 2u);
    uint16_t priority = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(cpu, enable,
                       (uint16_t)(dspic33_read_word(cpu, enable) | (uint16_t)(1u << (irq % 16u))));
    dspic33_write_word(cpu, priority,
                       (uint16_t)((dspic33_read_word(cpu, priority) & ~(uint16_t)(7u << shift)) |
                                  (uint16_t)(3u << shift)));
    cpu->program[(0x0014u + irq * 2u) / 2u] = COMPARE_VECTOR;
    cpu->w[15] = 0x1800u;
}

static inline void configure_compare_dma(Dspic33* cpu, uint8_t request, uint16_t source,
                                         uint16_t pad) {
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, request);
    dspic33_write_word(cpu, 0x0b04u, source);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, pad);
    dspic33_write_word(cpu, 0x0b0eu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0xa001u);
}

static inline void unsupported_case(TestState* state, Dspic33* cpu, uint16_t control1,
                                    uint16_t control2, const char* name) {
    bool high;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0904u, 4u);
    dspic33_write_word(cpu, 0x0906u, 2u);
    dspic33_write_word(cpu, 0x0900u, control1);
    dspic33_write_word(cpu, 0x0902u, control2);
    expect(state, !dspic33_output_compare_output(cpu, 0u, &high), name);
    expect(state, dspic33_device_advance(cpu, 6u), "advance excluded OC tuple");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 0u && cpu->events.count == 0u &&
               !interrupt_flag(cpu, 0u),
           "excluded OC tuple remains inactive");
}

static inline void configure_fault_compare(Dspic33* cpu, uint8_t channel, uint8_t mode,
                                           uint8_t source, uint16_t control2) {
    uint16_t base = compare_base(channel);
    configure_compare_mode(cpu, channel, mode, 4u, 2u, (uint16_t)(COMPARE_SELF_SYNC | control2));
    dspic33_output_compare_fault(cpu, source, true, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_write_word(cpu, base,
                       (uint16_t)(dspic33_read_word(cpu, base) | compare_fault_enable(source)));
}

#endif
