#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} OutputCompareConformance;

static const uint8_t compare_irqs[DSPIC33_OUTPUT_COMPARE_COUNT] = {
    2u,  6u,   25u,  26u,  41u,  42u,  43u,  44u,
    92u, 124u, 126u, 128u, 134u, 136u, 138u, 140u};

enum {
    COMPARE_BASE = 0x0900u,
    COMPARE_STRIDE = 0x000au,
    COMPARE_FP_EDGE_PWM = 0x1c06u,
    COMPARE_NO_SYNC = 0x0000u,
    COMPARE_SELF_SYNC = 0x001fu,
    COMPARE_VECTOR = 0x0240u
};

static void expect(OutputCompareConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[output-compare-failed] %s\n", name);
    }
}

static uint16_t compare_base(uint8_t channel) {
    return (uint16_t)(COMPARE_BASE + channel * COMPARE_STRIDE);
}

static bool interrupt_flag(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = compare_irqs[channel];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = compare_irqs[channel];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t bit = (uint16_t)(1u << (irq % 16u));
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~bit));
}

static bool output_is(const Dspic33* cpu, uint8_t channel, bool expected) {
    bool high;
    return dspic33_output_compare_output(cpu, channel, &high) && high == expected;
}

static bool pin_is(const Dspic33* cpu, uint8_t pin, bool expected) {
    bool high;
    return dspic33_output_compare_pin(cpu, pin, &high) && high == expected;
}

static void configure_compare_source(Dspic33* cpu, uint8_t channel, uint16_t period,
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

static void configure_compare(Dspic33* cpu, uint8_t channel, uint16_t period,
                              uint16_t duty) {
    configure_compare_source(cpu, channel, period, duty, COMPARE_SELF_SYNC);
}

static void configure_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = compare_irqs[channel];
    uint16_t enable = (uint16_t)(0x0820u + (irq / 16u) * 2u);
    uint16_t priority = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(
        cpu, enable,
        (uint16_t)(dspic33_read_word(cpu, enable) | (uint16_t)(1u << (irq % 16u))));
    dspic33_write_word(
        cpu, priority,
        (uint16_t)((dspic33_read_word(cpu, priority) & ~(uint16_t)(7u << shift)) |
                   (uint16_t)(3u << shift)));
    cpu->program[(0x0014u + irq * 2u) / 2u] = COMPARE_VECTOR;
    cpu->w[15] = 0x1800u;
}

static void access_cases(OutputCompareConformance* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        expect(state, dspic33_read_word(cpu, base) == 0u, "OCCON1 reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x000cu,
               "OCCON2 reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "OCRS deterministic reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0u,
               "OCR deterministic reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "OCTMR deterministic reset");
        dspic33_write_word(cpu, base, UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), UINT16_MAX);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), UINT16_MAX);
        expect(state, dspic33_read_word(cpu, base) == 0x3fffu, "OCCON1 access mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0xf1ffu,
               "OCCON2 access mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == UINT16_MAX,
               "OCRS writable");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == UINT16_MAX,
               "OCR writable");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "OCTMR read only");
    }
}

static void waveform_cases(OutputCompareConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        dspic33_reset(cpu, 0u);
        configure_compare(cpu, channel, 4u, 2u);
        expect(state, output_is(cpu, channel, true), "PWM starts high at timer zero");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "PWM timer starts at zero");
        expect(state, dspic33_device_advance(cpu, 1u), "advance before duty match");
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u,
               "PWM remains high before duty match");
        expect(state, dspic33_device_advance(cpu, 1u), "advance duty match");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                   !interrupt_flag(cpu, channel),
               "duty match lowers output without interrupt");
        expect(state, dspic33_device_advance(cpu, 2u), "advance through period value");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 4u &&
                   !interrupt_flag(cpu, channel),
               "period value is the final low timer cycle");
        expect(state, dspic33_device_advance(cpu, 1u), "advance self synchronization");
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   interrupt_flag(cpu, channel),
               "RS plus one resets timer raises output and interrupt");
    }
}

static void constant_output_case(OutputCompareConformance* state, Dspic33* cpu,
                                 uint16_t period, uint16_t duty, bool high,
                                 const char* name) {
    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, period, duty);
    expect(state, output_is(cpu, 0u, high), name);
    expect(state, dspic33_device_advance(cpu, period),
           "advance constant PWM period value");
    expect(state,
           output_is(cpu, 0u, high) && dspic33_read_word(cpu, 0x0908u) == period &&
               !interrupt_flag(cpu, 0u),
           "constant PWM holds through period value");
    expect(state, dspic33_device_advance(cpu, 1u), "advance constant PWM rollover");
    expect(state,
           output_is(cpu, 0u, high) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               interrupt_flag(cpu, 0u),
           "constant PWM rolls over at RS plus one");
}

static void boundary_cases(OutputCompareConformance* state, Dspic33* cpu) {
    constant_output_case(state, cpu, 4u, 0u, false, "zero duty starts low");
    constant_output_case(state, cpu, 4u, 4u, true, "equal duty stays high");
    constant_output_case(state, cpu, 4u, 5u, true, "greater duty stays high");
    constant_output_case(state, cpu, 0u, 0u, false, "zero period zero duty starts low");
    constant_output_case(state, cpu, 0u, 1u, true,
                         "zero period nonzero duty starts high");
}

static void buffering_cases(OutputCompareConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance before buffered writes");
    dspic33_write_word(cpu, 0x0904u, 6u);
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               cpu->io.output_compare.active_r[0] == 2u &&
               dspic33_read_word(cpu, 0x0904u) == 6u &&
               dspic33_read_word(cpu, 0x0906u) == 3u,
           "R and RS writes remain buffered");
    expect(state, dspic33_device_advance(cpu, 1u), "advance old buffered duty");
    expect(state, output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 2u,
           "old duty controls current period");
    expect(state, dspic33_device_advance(cpu, 3u), "advance old buffered period");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               cpu->io.output_compare.active_rs[0] == 6u &&
               cpu->io.output_compare.active_r[0] == 3u,
           "period boundary loads both buffers");
    clear_interrupt(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance newly buffered duty");
    expect(state,
           output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 3u &&
               !interrupt_flag(cpu, 0u),
           "new duty controls next period");
    expect(state, dspic33_device_advance(cpu, 4u), "advance newly buffered period");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               interrupt_flag(cpu, 0u),
           "new period rolls over at new RS plus one");
}

static void free_running_cases(OutputCompareConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        dspic33_reset(cpu, 0u);
        configure_compare_source(cpu, channel, 4u, 2u, COMPARE_NO_SYNC);
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "free-running PWM starts at timer zero");
        expect(state, dspic33_device_advance(cpu, 5u),
               "advance free-running PWM beyond RS");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 5u &&
                   !interrupt_flag(cpu, channel),
               "free-running PWM ignores RS after duty match");
        expect(state, dspic33_device_advance(cpu, UINT16_MAX - 5u),
               "advance free-running PWM to timer maximum");
        expect(state,
               output_is(cpu, channel, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == UINT16_MAX &&
                   !interrupt_flag(cpu, channel),
               "free-running PWM holds low through timer maximum");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance free-running PWM rollover");
        expect(state,
               output_is(cpu, channel, true) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   interrupt_flag(cpu, channel),
               "free-running rollover starts a new PWM cycle");
    }
}

static void instruction_transition_cases(OutputCompareConformance* state,
                                         Dspic33* cpu) {
    static const uint32_t program[] = {
        0xef2900u, 0xef2902u, 0x200040u, 0x884820u, 0x200020u, 0x884830u,
        0x21c060u, 0x884800u, 0x000000u, 0x2001f0u, 0x884810u, 0x000000u,
        0x000000u, 0x200000u, 0x884810u, 0x000000u, 0x000000u, 0x000000u,
    };
    bool loaded = true;
    bool ran = true;
    size_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < sizeof(program) / sizeof(program[0]); index++) {
        loaded = loaded &&
                 dspic33_load_program_word(cpu, (uint32_t)(index * 2u), program[index]);
    }
    expect(state, loaded, "load exact OC synchronization transition sequence");
    for (index = 0u; index < 7u; index++) {
        ran = ran && dspic33_step(cpu) == DSPIC33_RUNNING;
    }
    expect(state, ran, "execute OC synchronization setup sequence");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 0u && output_is(cpu, 0u, true),
           "OC enable takes effect after its instruction cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 1u && output_is(cpu, 0u, true),
           "SYNCSEL zero advances on the next instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 2u && output_is(cpu, 0u, false),
           "SYNCSEL literal instruction preserves free-running phase");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0902u) == COMPARE_SELF_SYNC &&
               dspic33_read_word(cpu, 0x0908u) == 3u,
           "zero to self synchronization preserves timer phase");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 4u && !interrupt_flag(cpu, 0u),
           "self synchronization waits through the RS timer value");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 0u && output_is(cpu, 0u, true) &&
               interrupt_flag(cpu, 0u),
           "self synchronization resets on the next increment");
    clear_interrupt(cpu, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "no-sync literal instruction advances self-running timer");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0902u) == COMPARE_NO_SYNC &&
               dspic33_read_word(cpu, 0x0908u) == 2u && output_is(cpu, 0u, false),
           "self to zero synchronization preserves timer phase");
    ran = true;
    for (index = 0u; index < 3u; index++) {
        ran = ran && dspic33_step(cpu) == DSPIC33_RUNNING;
    }
    expect(state,
           ran && dspic33_read_word(cpu, 0x0908u) == 5u && output_is(cpu, 0u, false) &&
               !interrupt_flag(cpu, 0u),
           "removed self synchronization advances beyond RS");
}

static void free_running_buffer_cases(OutputCompareConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_source(cpu, 0u, 4u, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance before free-running buffered writes");
    dspic33_write_word(cpu, 0x0904u, 6u);
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               cpu->io.output_compare.active_r[0] == 2u,
           "free-running compare writes remain buffered");
    expect(state, dspic33_device_advance(cpu, UINT16_MAX - 4u),
           "advance free-running buffered rollover");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 0u && output_is(cpu, 0u, true) &&
               cpu->io.output_compare.active_rs[0] == 6u &&
               cpu->io.output_compare.active_r[0] == 3u && interrupt_flag(cpu, 0u),
           "free-running rollover loads both compare buffers");
    clear_interrupt(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance new free-running duty");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 3u && output_is(cpu, 0u, false) &&
               !interrupt_flag(cpu, 0u),
           "new free-running duty controls the next cycle");
}

static void pps_case(OutputCompareConformance* state, Dspic33* cpu, uint8_t channel,
                     uint8_t pin, uint16_t address, uint8_t shift, uint8_t function) {
    uint16_t mapping = (uint16_t)(function << shift);
    dspic33_reset(cpu, 0u);
    configure_compare(cpu, channel, 3u, 1u);
    dspic33_write_word(cpu, address, mapping);
    expect(state, pin_is(cpu, pin, true), "mapped PPS output starts high");
    expect(state, !dspic33_output_compare_pin(cpu, (uint8_t)(pin + 1u), NULL),
           "PPS output rejects null destination");
    {
        bool high;
        expect(state, !dspic33_output_compare_pin(cpu, (uint8_t)(pin + 1u), &high),
               "unmapped PPS pin is rejected");
    }
    expect(state, dspic33_device_advance(cpu, 1u), "advance mapped PPS duty");
    expect(state, pin_is(cpu, pin, false), "mapped PPS output follows duty transition");
    expect(state, dspic33_device_advance(cpu, 3u), "advance mapped PPS rollover");
    expect(state, pin_is(cpu, pin, true), "mapped PPS output follows rollover");
    dspic33_write_word(cpu, (uint16_t)(compare_base(channel) + 2u), 0x003fu);
    {
        bool high;
        expect(state, !dspic33_output_compare_pin(cpu, pin, &high),
               "OCTRIS disconnects PPS output");
    }
}

static void pps_cases(OutputCompareConformance* state, Dspic33* cpu) {
    pps_case(state, cpu, 0u, 109u, 0x0698u, 0u, 0x10u);
    pps_case(state, cpu, 1u, 65u, 0x0680u, 8u, 0x11u);
    pps_case(state, cpu, 4u, 108u, 0x0696u, 8u, 0x14u);
    pps_case(state, cpu, 15u, 64u, 0x0680u, 0u, 0x2cu);
}

static void interrupt_cases(OutputCompareConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        dspic33_reset(cpu, 0u);
        configure_interrupt(cpu, channel);
        configure_compare(cpu, channel, 0u, 0u);
        expect(state, !dspic33_device_interrupt_pending(cpu),
               "OC interrupt is not pending before rollover");
        expect(state, dspic33_device_advance(cpu, 1u), "advance OC interrupt rollover");
        expect(state,
               interrupt_flag(cpu, channel) && dspic33_device_interrupt_pending(cpu),
               "OC interrupt becomes pending at rollover");
        expect(state,
               dspic33_device_service_interrupt(cpu) &&
                   cpu->last_interrupt == compare_irqs[channel] &&
                   cpu->pc == COMPARE_VECTOR,
               "OC channel uses documented interrupt vector");
    }
}

static void unsupported_case(OutputCompareConformance* state, Dspic33* cpu,
                             uint16_t control1, uint16_t control2, const char* name) {
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

static void unsupported_cases(OutputCompareConformance* state, Dspic33* cpu) {
    unsupported_case(state, cpu, 0x1806u, COMPARE_SELF_SYNC,
                     "non-FP clock is excluded");
    unsupported_case(state, cpu, 0x1c05u, COMPARE_SELF_SYNC,
                     "non-edge PWM mode is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x001cu,
                     "reserved synchronization source is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x011fu,
                     "32-bit mode is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x009fu,
                     "trigger mode is excluded");
    unsupported_case(state, cpu, 0x3c06u, COMPARE_SELF_SYNC,
                     "stop-in-idle mode is excluded");
    unsupported_case(state, cpu, 0x1c0eu, COMPARE_SELF_SYNC, "fault mode is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x101fu,
                     "inverted output is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x003fu,
                     "tristate output is excluded");
}

static void coexistence_cases(OutputCompareConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0142u, 0x00c0u);
    dspic33_write_word(cpu, 0x0140u, 0x1c03u);
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue input capture beside OC");
    expect(state, dspic33_device_advance(cpu, 1u), "advance IC and OC together");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u &&
               dspic33_read_word(cpu, 0x0144u) == 1u &&
               dspic33_read_word(cpu, 0x0908u) == 1u && output_is(cpu, 0u, true),
           "IC and OC advance independently");
    expect(state, dspic33_device_advance(cpu, 1u), "advance OC duty beside IC");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 0u && output_is(cpu, 0u, false) &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "IC read side effect does not disturb OC");
}

static void lifecycle_cases(OutputCompareConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    bool high;
    expect(state, initialized, "initialize OC copy");
    if (!initialized) {
        return;
    }

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance OC before copy");
    expect(state, dspic33_copy(&copy, cpu), "copy pending OC state");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance original and copied OC");
    expect(state,
           output_is(cpu, 0u, false) && output_is(&copy, 0u, false) &&
               dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(&copy, 0x0908u),
           "copy preserves OC event and state");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_write_word(cpu, 0x0900u, 0u);
    expect(state, dspic33_device_advance(cpu, 6u), "advance disabled stale OC events");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) &&
               dspic33_read_word(cpu, 0x0908u) == 0u && !interrupt_flag(cpu, 0u),
           "disable cancels OC events and output");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_write_word(cpu, 0x0900u, 0u);
    configure_compare(cpu, 0u, 6u, 3u);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance re-enabled OC across stale event");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 2u &&
               !interrupt_flag(cpu, 0u),
           "stale generation cannot alter re-enabled OC");
    expect(state, dspic33_device_advance(cpu, 1u), "advance re-enabled OC duty event");
    expect(state,
           output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 3u &&
               !interrupt_flag(cpu, 0u),
           "re-enabled OC follows new duty schedule");
    expect(state, dspic33_device_advance(cpu, 2u), "advance across stale period event");
    expect(state,
           output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 5u &&
               !interrupt_flag(cpu, 0u),
           "stale period cannot reset re-enabled OC");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance re-enabled OC period event");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               interrupt_flag(cpu, 0u),
           "re-enabled OC follows new period schedule");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 6u), "advance reset OC queue");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) &&
               cpu->io.output_compare.output_high == 0u && cpu->events.count == 0u,
           "reset clears OC state and events");

    expect(state, !dspic33_output_compare_output(cpu, 16u, &high),
           "reject invalid OC channel");
    expect(state, !dspic33_output_compare_output(cpu, 0u, NULL),
           "reject null OC output destination");
    expect(state, !dspic33_output_compare_pin(cpu, 0u, &high),
           "reject unmapped OC pin");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0900u) & 7u) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u &&
               !dspic33_output_compare_output(cpu, 0u, &high) &&
               cpu->events.count == 0u,
           "initial OC schedule failure aborts channel");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX - 2u;
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, !dspic33_device_advance(cpu, 2u),
           "OC reschedule overflow stops advance");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0900u) & 7u) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u &&
               !dspic33_output_compare_output(cpu, 0u, &high) &&
               cpu->events.count == 0u,
           "OC reschedule failure aborts channel");
    dspic33_destroy(&copy);
}

int main(void) {
    Dspic33 cpu;
    OutputCompareConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize output compare processor");
    if (initialized) {
        access_cases(&state, &cpu);
        waveform_cases(&state, &cpu);
        boundary_cases(&state, &cpu);
        buffering_cases(&state, &cpu);
        free_running_cases(&state, &cpu);
        instruction_transition_cases(&state, &cpu);
        free_running_buffer_cases(&state, &cpu);
        pps_cases(&state, &cpu);
        interrupt_cases(&state, &cpu);
        unsupported_cases(&state, &cpu);
        coexistence_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[output-compare-summary] cases=%u passed=%u failed=%u\n", state.cases,
           state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
