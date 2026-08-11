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
    COMPARE_FP = 0x1c00u,
    COMPARE_FP_EDGE_PWM = 0x1c06u,
    COMPARE_NO_SYNC = 0x0000u,
    COMPARE_SELF_SYNC = 0x001fu,
    COMPARE_VECTOR = 0x0240u,
    COMPARE_TRIGGER = 0x0080u,
    COMPARE_TRIGGER_STATUS = 0x0040u,
    COMPARE_TRIGGER_ONESHOT = 0x0008u
};

static const uint16_t timer_registers[] = {0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u};
static const uint16_t timer_periods[] = {0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu};
static const uint16_t timer_controls[] = {0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u};

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

static void configure_compare_mode(Dspic33* cpu, uint8_t channel, uint8_t mode,
                                   uint16_t secondary, uint16_t primary,
                                   uint16_t control2) {
    uint16_t base = compare_base(channel);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), secondary);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), primary);
    clear_interrupt(cpu, channel);
    dspic33_write_word(cpu, base, (uint16_t)(COMPARE_FP | mode));
    dspic33_write_word(cpu, (uint16_t)(base + 2u), control2);
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
    dspic33_reset(cpu, 0x200u);
    for (index = 0u; index < sizeof(program) / sizeof(program[0]); index++) {
        loaded = loaded && dspic33_load_program_word(
                               cpu, 0x200u + (uint32_t)(index * 2u), program[index]);
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

static void single_compare_cases(OutputCompareConformance* state, Dspic33* cpu) {
    static const bool initial[3] = {false, true, false};
    static const bool matched[3] = {true, false, true};
    uint8_t mode;
    for (mode = 1u; mode <= 3u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 4u, 2u, COMPARE_SELF_SYNC);
        expect(state, output_is(cpu, 0u, initial[mode - 1u]),
               "single compare initializes documented level");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance single compare primary match");
        expect(state,
               output_is(cpu, 0u, initial[mode - 1u]) &&
                   dspic33_read_word(cpu, 0x0908u) == 2u && !interrupt_flag(cpu, 0u),
               "single compare match precedes output transition");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance single compare output pipeline");
        expect(state,
               output_is(cpu, 0u, matched[mode - 1u]) &&
                   dspic33_read_word(cpu, 0x0908u) == 3u && !interrupt_flag(cpu, 0u),
               "single compare output changes one clock after match");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance single compare interrupt pipeline");
        expect(state,
               output_is(cpu, 0u, matched[mode - 1u]) &&
                   dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
               "single compare interrupt follows output by two clocks");
        clear_interrupt(cpu, 0u);
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance single compare next-cycle match");
        expect(state,
               output_is(cpu, 0u, matched[mode - 1u]) && !interrupt_flag(cpu, 0u),
               "single compare next match does not change output immediately");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance single compare repeated output pipeline");
        expect(state,
               output_is(cpu, 0u, mode == 3u ? false : matched[mode - 1u]) &&
                   !interrupt_flag(cpu, 0u),
               "single-shot holds while toggle changes on the next clock");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance repeated single compare interrupt pipeline");
        expect(state, interrupt_flag(cpu, 0u) == (mode == 3u),
               "only toggle mode repeats its interrupt");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 3u, 4u, 4u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance equal single compare and period");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "equal single compare records match before output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance equal single compare output and boundary");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               !interrupt_flag(cpu, 0u),
           "equal single compare changes output on next clock");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance equal single compare interrupt");
    expect(state, interrupt_flag(cpu, 0u),
           "equal single compare raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 3u, 0u, COMPARE_SELF_SYNC);
    expect(state, output_is(cpu, 0u, false), "zero single compare starts unchanged");
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero single compare through first boundary");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero single compare remains initial at synchronization");
    expect(state, dspic33_device_advance(cpu, 8u),
           "advance zero single compare across later synchronizations");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 0u,
           "zero single compare never generates an event while held in reset");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 3u, 0u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, UINT16_MAX + 1u),
           "advance zero free-running single compare to rollover");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero free-running single compare detects at rollover");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance zero free-running single compare output");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "zero free-running single compare changes after rollover");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance zero free-running single compare interrupt");
    expect(state, interrupt_flag(cpu, 0u),
           "zero free-running single compare raises delayed interrupt");

    for (mode = 1u; mode <= 3u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 2u, 4u, COMPARE_SELF_SYNC);
        expect(state, dspic33_device_advance(cpu, 9u),
               "advance single compare beyond synchronization period");
        expect(state,
               output_is(cpu, 0u, mode == 2u) && !interrupt_flag(cpu, 0u) &&
                   cpu->io.output_compare.phase[0] == 0u,
               "single compare beyond period remains at initial level");
    }
}

static void dual_compare_cases(OutputCompareConformance* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 4u; mode <= 5u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 4u, 2u, COMPARE_SELF_SYNC);
        expect(state, output_is(cpu, 0u, false), "dual compare initializes low");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance dual compare rising edge");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 2u,
               "dual primary match precedes rising edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance dual compare rising pipeline");
        expect(state,
               output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 3u,
               "dual primary output changes one clock after match");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance dual compare secondary match");
        expect(state,
               output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 4u,
               "dual secondary match precedes falling edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance dual compare falling pipeline");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   dspic33_read_word(cpu, 0x0908u) == 0u,
               "dual secondary output changes one clock after match");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance dual compare interrupt pipeline");
        expect(state, output_is(cpu, 0u, false) && interrupt_flag(cpu, 0u),
               "dual interrupt follows falling edge by two clocks");
        clear_interrupt(cpu, 0u);
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance continuous dual repeated rising pipeline");
        expect(state, output_is(cpu, 0u, mode == 5u) && !interrupt_flag(cpu, 0u),
               "dual single-shot stops while continuous mode repeats");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 2u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance equal dual primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "equal dual values detect primary before output");
    expect(state, dspic33_device_advance(cpu, 1u), "advance equal dual primary output");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "equal dual values raise output in first cycle");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance equal dual secondary match in next cycle");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "equal dual secondary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance equal dual secondary output");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "equal dual values lower output in following cycle");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance equal dual interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u), "equal dual values raise delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 3u, 0u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero-primary dual first cycle");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero-primary dual records match at first synchronization");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance zero-primary dual rising pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "zero-primary dual rises after first synchronization");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance zero-primary dual secondary match");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "zero-primary dual secondary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance zero-primary dual falling pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "zero-primary dual falls after secondary match");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance zero-primary dual interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "zero-primary dual raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 0u, 0u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero dual values across boundaries");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "zero dual values remain low without interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 2u, 4u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance reversed dual primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "reversed dual primary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance reversed dual rising pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "reversed dual values raise output before rollover");
    expect(state, dspic33_device_advance(cpu, UINT16_MAX - 5u),
           "advance reversed dual to timer maximum");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == UINT16_MAX,
           "reversed dual holds output through timer maximum");
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance reversed dual across rollover to secondary match");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 2u &&
               !interrupt_flag(cpu, 0u),
           "reversed dual secondary match precedes output after rollover");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance reversed dual falling pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "reversed dual falls one clock after secondary match");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance reversed dual interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u), "reversed dual raises delayed interrupt");

    for (mode = 4u; mode <= 5u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 2u, 4u, COMPARE_SELF_SYNC);
        expect(state, dspic33_device_advance(cpu, 9u),
               "advance self-synchronized reversed dual values");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   cpu->io.output_compare.phase[0] == 0u,
               "synchronization before primary compare suppresses dual pulse");
    }

    for (mode = 4u; mode <= 7u; mode++) {
        if (mode == 6u) {
            continue;
        }
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, mode, 0u, 0u, COMPARE_SELF_SYNC);
        expect(state, dspic33_device_advance(cpu, 4u),
               "advance zero dual values across mode");
        expect(state,
               output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
                   cpu->io.output_compare.phase[0] == 0u,
               "zero dual values suppress every pulse mode");
    }
}

static void center_aligned_cases(OutputCompareConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 7u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, output_is(cpu, 0u, false), "center PWM initializes low");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM before buffered writes");
    dspic33_write_word(cpu, 0x0904u, 6u);
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               cpu->io.output_compare.active_r[0] == 2u,
           "center PWM compare writes remain buffered");
    expect(state, dspic33_device_advance(cpu, 1u), "advance center PWM primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "center PWM primary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM rising pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "center PWM primary output changes one clock later");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM secondary match");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "center PWM secondary match precedes output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance center PWM falling pipeline and boundary");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.active_rs[0] == 6u &&
               cpu->io.output_compare.active_r[0] == 3u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "center PWM falling edge and boundary load compare buffers");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance center PWM interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "center PWM interrupt follows falling edge by two clocks");
}

static void immediate_compare_write_cases(OutputCompareConformance* state,
                                          Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 3u, 6u, 4u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance before immediate primary write");
    dspic33_write_word(cpu, 0x0906u, 2u);
    expect(state,
           cpu->io.output_compare.active_r[0] == 2u &&
               dspic33_device_advance(cpu, 1u) && output_is(cpu, 0u, false) &&
               !interrupt_flag(cpu, 0u),
           "non-PWM primary write changes current compare cycle");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance immediate primary output pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "immediate primary write changes output after one clock");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance immediate primary interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "immediate primary write raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 6u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance before immediate secondary write");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance primary output before secondary write");
    dspic33_write_word(cpu, 0x0904u, 4u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               dspic33_device_advance(cpu, 1u) && output_is(cpu, 0u, true) &&
               !interrupt_flag(cpu, 0u),
           "non-PWM secondary write changes current compare cycle");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance immediate secondary output pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "immediate secondary write changes output after one clock");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance immediate secondary interrupt pipeline");
    expect(state, interrupt_flag(cpu, 0u),
           "immediate secondary write raises delayed interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance primary match before compare rewrite");
    dspic33_write_word(cpu, 0x0906u, 3u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance latched primary output after compare rewrite");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "primary compare rewrite preserves latched output event");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance latched primary interrupt after compare rewrite");
    expect(state, interrupt_flag(cpu, 0u),
           "primary compare rewrite preserves latched interrupt event");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance secondary match before compare rewrite");
    dspic33_write_word(cpu, 0x0904u, 6u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance latched secondary output after compare rewrite");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.active_rs[0] == 6u,
           "secondary compare rewrite preserves latched output event");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance latched secondary interrupt after compare rewrite");
    expect(state, interrupt_flag(cpu, 0u),
           "secondary compare rewrite preserves latched interrupt event");
}

static void output_control_cases(OutputCompareConformance* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u,
                           (uint16_t)(COMPARE_SELF_SYNC | 0x1000u));
    expect(state, output_is(cpu, 0u, true), "OCINV inverts initialized output");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance inverted single compare match");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "OCINV remains unchanged at compare match");
    expect(state, dspic33_device_advance(cpu, 1u), "advance inverted output pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "OCINV inverts delayed matched output");
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 3u,
           "live OCINV clear preserves timer and mode phase");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_write_word(cpu, 0x0698u, 0x0010u);
    expect(state, pin_is(cpu, 109u, true), "driven OC pin is observable");
    dspic33_write_word(cpu, 0x0902u, (uint16_t)(COMPARE_SELF_SYNC | 0x0020u));
    expect(state,
           dspic33_output_compare_output(cpu, 0u, &high) && high &&
               !dspic33_output_compare_pin(cpu, 109u, &high),
           "OCTRIS disconnects pin without stopping module output");
    expect(state, dspic33_device_advance(cpu, 2u), "advance tri-stated OC module");
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, pin_is(cpu, 109u, false),
           "clearing OCTRIS reconnects preserved module phase");
}

static void channel_mode_matrix_cases(OutputCompareConformance* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t mode;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        for (mode = 1u; mode <= 7u; mode++) {
            bool initial = mode == 2u || mode == 6u;
            bool primary_detection = mode == 2u;
            bool primary_output = mode != 2u && mode != 6u;
            bool boundary_output = mode == 1u || mode == 3u || mode == 6u;
            bool delayed_secondary_interrupt = mode == 4u || mode == 5u || mode == 7u;
            bool next_output = mode == 1u || mode == 5u || mode == 7u;
            uint16_t base = compare_base(channel);
            dspic33_reset(cpu, 0u);
            configure_compare_mode(cpu, channel, mode, 4u, 2u, COMPARE_SELF_SYNC);
            expect(state,
                   output_is(cpu, channel, initial) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
                   "channel mode matrix initial state");
            expect(state, dspic33_device_advance(cpu, 2u),
                   "channel mode matrix primary advance");
            expect(state,
                   output_is(cpu, channel, primary_detection) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix primary detection");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix primary output advance");
            expect(state,
                   output_is(cpu, channel, primary_output) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 3u &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix primary output pipeline");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix secondary advance");
            expect(state,
                   output_is(cpu, channel, primary_output) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 4u &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix secondary detection");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix boundary advance");
            expect(state,
                   output_is(cpu, channel, boundary_output) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                       interrupt_flag(cpu, channel) == (mode <= 3u || mode == 6u),
                   "channel mode matrix output and first interrupt pipeline");
            clear_interrupt(cpu, channel);
            expect(state, dspic33_device_advance(cpu, 2u),
                   "channel mode matrix delayed interrupt advance");
            expect(state,
                   output_is(cpu, channel, mode == 1u || mode == 3u) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                       interrupt_flag(cpu, channel) == delayed_secondary_interrupt,
                   "channel mode matrix delayed secondary interrupt");
            clear_interrupt(cpu, channel);
            expect(state, dspic33_device_advance(cpu, 1u),
                   "channel mode matrix next output advance");
            expect(state,
                   output_is(cpu, channel, next_output) &&
                       !interrupt_flag(cpu, channel),
                   "channel mode matrix next-cycle output pipeline");
        }
    }
}

static void one_shot_restart_cases(OutputCompareConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 4u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 7u), "advance first one-shot pulse");
    expect(state,
           output_is(cpu, 0u, false) && interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "one-shot pulse reaches completed state");
    clear_interrupt(cpu, 0u);
    dspic33_write_byte(cpu, 0x0901u, 0x1cu);
    expect(state,
           cpu->io.output_compare.phase[0] == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "high control byte write does not restart one-shot");
    dspic33_write_byte(cpu, 0x0900u, 0x04u);
    expect(state,
           output_is(cpu, 0u, false) && cpu->io.output_compare.phase[0] == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "same mode low-byte write restarts one-shot");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance restarted one-shot primary match");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "restarted one-shot detects a new primary match");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance restarted one-shot output pipeline");
    expect(state, output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "restarted one-shot generates a delayed rising edge");
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
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x001cu,
                     "reserved synchronization source is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x011fu,
                     "32-bit mode is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x009fu,
                     "trigger mode is excluded");
    unsupported_case(state, cpu, 0x3c06u, COMPARE_SELF_SYNC,
                     "stop-in-idle mode is excluded");
    unsupported_case(state, cpu, 0x1c0eu, (uint16_t)(COMPARE_TRIGGER | 1u),
                     "one-shot self trigger is excluded");
    unsupported_case(state, cpu, 0x1c86u, COMPARE_SELF_SYNC, "fault input is excluded");
}

static void alternate_clock_cases(OutputCompareConformance* state, Dspic33* cpu) {
    static const uint8_t selections[] = {4u, 0u, 1u, 2u, 3u};
    static const uint8_t timers[] = {0u, 1u, 2u, 3u, 4u};
    uint8_t channel;
    size_t index;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        for (index = 0u; index < sizeof(selections) / sizeof(selections[0]); index++) {
            uint16_t base = compare_base(channel);
            uint8_t timer = timers[index];
            dspic33_reset(cpu, 0u);
            dspic33_write_word(cpu, timer_periods[timer], 100u);
            dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
            dspic33_write_word(cpu, (uint16_t)(base + 4u), 4u);
            dspic33_write_word(cpu, (uint16_t)(base + 6u), 2u);
            dspic33_write_word(cpu, base,
                               (uint16_t)((uint16_t)selections[index] << 10u | 6u));
            dspic33_write_word(cpu, (uint16_t)(base + 2u), COMPARE_SELF_SYNC);
            expect(state,
                   output_is(cpu, channel, true) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
                   "alternate clock starts PWM at zero");
            expect(state, dspic33_device_advance(cpu, 2u),
                   "advance alternate clock to duty");
            expect(state,
                   output_is(cpu, channel, false) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                       dspic33_read_word(cpu, timer_registers[timer]) == 2u,
                   "selected timer clock reaches OC duty match");
            expect(state, dspic33_device_advance(cpu, 2u),
                   "advance alternate clock through period value");
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 4u &&
                       !interrupt_flag(cpu, channel),
                   "alternate clock retains final period value");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "advance alternate clock through rollover");
            expect(state,
                   output_is(cpu, channel, true) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                       interrupt_flag(cpu, channel),
                   "alternate clock resets and raises PWM interrupt");
        }
    }
}

static bool prepare_compare_trigger_source(Dspic33* cpu, uint8_t source) {
    if (source >= 1u && source <= 9u) {
        configure_compare_mode(cpu, (uint8_t)(source - 1u), 5u, 1u, 0u,
                               COMPARE_NO_SYNC);
        return true;
    }
    if (source >= 11u && source <= 15u) {
        uint8_t timer = (uint8_t)(source - 11u);
        dspic33_write_word(cpu, timer_periods[timer], 1u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        return true;
    }
    if (source >= 16u && source <= 23u) {
        uint16_t base = (uint16_t)(0x0140u + (source - 16u) * 8u);
        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
        dspic33_write_word(cpu, base, 0x1c03u);
        return true;
    }
    if (source >= 24u && source <= 26u) {
        uint16_t base = (uint16_t)(0x0a84u + (source - 24u) * 8u);
        dspic33_write_word(cpu, base, 0x8040u);
        return true;
    }
    if (source == 27u) {
        dspic33_write_word(cpu, 0x0320u, 0x8000u);
        return true;
    }
    return source == 29u || source == 30u;
}

static bool emit_compare_trigger_source(Dspic33* cpu, uint8_t source) {
    if (source >= 1u && source <= 9u) {
        return dspic33_device_advance(cpu, 2u);
    }
    if (source >= 11u && source <= 15u) {
        return dspic33_device_advance(cpu, 1u);
    }
    if (source >= 16u && source <= 23u) {
        return dspic33_input_capture_input(cpu, (uint8_t)(source - 16u), true, 0u) &&
               dspic33_device_advance(cpu, 3u);
    }
    if (source >= 24u && source <= 26u) {
        return dspic33_comparator_input(cpu, (uint8_t)(source - 24u),
                                        DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u);
    }
    if (source == 27u) {
        dspic33_write_word(cpu, 0x0320u, 0x8002u);
        dspic33_write_word(cpu, 0x0320u, 0x8000u);
        return dspic33_device_advance(cpu, 12u);
    }
    if (source == 29u || source == 30u) {
        uint16_t irq = source == 29u ? 20u : 29u;
        return dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, irq, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u);
    }
    return false;
}

static void trigger_source_cases(OutputCompareConformance* state, Dspic33* cpu) {
    static const uint8_t sources[] = {1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,  9u,  11u,
                                      12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u,
                                      22u, 23u, 24u, 25u, 26u, 27u, 29u, 30u};
    size_t index;
    for (index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        uint8_t source = sources[index];
        uint16_t base = compare_base(15u);
        dspic33_reset(cpu, 0u);
        expect(state, prepare_compare_trigger_source(cpu, source),
               "prepare documented OC trigger source");
        configure_compare_mode(cpu, 15u, 6u, 4u, 2u,
                               (uint16_t)(COMPARE_TRIGGER | source));
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) &
                    COMPARE_TRIGGER_STATUS) == 0u,
               "triggered OC timer starts held clear");
        expect(state, emit_compare_trigger_source(cpu, source),
               "emit documented OC trigger source");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) &
                    COMPARE_TRIGGER_STATUS) != 0u,
               "documented source sets OC trigger status without same-edge count");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance first clock after OC trigger");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u,
               "triggered OC timer counts on following clock");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, COMPARE_TRIGGER);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance software-only triggered OC while held");
    expect(state, dspic33_read_word(cpu, 0x099eu) == 0u,
           "zero trigger source remains software-only");
    dspic33_write_word(cpu, 0x0998u,
                       (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS));
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance software-released OC trigger");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 1u &&
               (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u,
           "software sets trigger status for source zero");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 10u));
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance no-source trigger while held");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 0u &&
               (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "no-source trigger remains software controlled");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 28u,
                     "reserved OC synchronization source is inactive");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 16u,
                     "input capture synchronization is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, (uint16_t)(COMPARE_TRIGGER | 1u),
                     "OC cannot select itself as alternate trigger source");
}

static void trigger_one_shot_cases(OutputCompareConformance* state, Dspic33* cpu) {
    uint16_t control2;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0904u, 2u);
    dspic33_write_word(cpu, 0x0906u, 1u);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(COMPARE_FP | COMPARE_TRIGGER_ONESHOT | 6u));
    dspic33_write_word(cpu, 0x0902u,
                       (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS | 29u));
    expect(state, (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u,
           "one-shot trigger status rejects software set");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "emit first one-shot trigger");
    expect(state,
           (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "hardware releases one-shot timer");
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance one-shot trigger through period");
    expect(state,
           (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
           "one-shot rollover clears trigger status and holds timer");
    clear_interrupt(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance held one-shot timer");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u,
           "one-shot timer remains held for next trigger");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 2u),
           "retrigger one-shot timer");
    control2 = dspic33_read_word(cpu, 0x0902u);
    expect(state,
           (control2 & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "new hardware trigger starts another one-shot period");
}

static void synchronization_source_cases(OutputCompareConformance* state,
                                         Dspic33* cpu) {
    static const uint8_t sources[] = {1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,  9u,  11u,
                                      12u, 13u, 14u, 15u, 24u, 25u, 26u, 27u, 29u, 30u};
    size_t index;
    for (index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        uint8_t source = sources[index];
        uint16_t base = compare_base(15u);
        dspic33_reset(cpu, 0u);
        expect(state, prepare_compare_trigger_source(cpu, source),
               "prepare documented OC synchronization source");
        configure_compare_mode(cpu, 15u, 6u, 100u, 50u, source);
        expect(state, emit_compare_trigger_source(cpu, source),
               "emit documented OC synchronization source");
        expect(state,
               (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u &&
                   !interrupt_flag(cpu, 15u),
               "synchronization pulse preserves timer through source edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance clock after OC synchronization pulse");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   interrupt_flag(cpu, 15u),
               "synchronization resets OC timer on following clock");
    }
}

static void alternate_clock_batch_cases(OutputCompareConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    uint64_t cycle;
    expect(state, initialized, "initialize alternate-clock batch copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[2], 100u);
    dspic33_write_word(cpu, timer_controls[2], 0x8010u);
    dspic33_write_word(cpu, 0x0904u, 4u);
    dspic33_write_word(cpu, 0x0906u, 2u);
    dspic33_write_word(cpu, 0x0900u, 0x0406u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_copy(&copy, cpu), "copy prescaled alternate OC clock");
    expect(state, dspic33_device_advance(cpu, 160u),
           "batch advance prescaled alternate OC clock");
    for (cycle = 0u; cycle < 160u; cycle++) {
        if (!dspic33_device_advance(&copy, 1u)) {
            break;
        }
    }
    expect(state, cycle == 160u, "step prescaled alternate OC clock");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(&copy, 0x0908u) &&
               dspic33_read_word(cpu, timer_registers[2]) ==
                   dspic33_read_word(&copy, timer_registers[2]) &&
               (cpu->io.output_compare.output_high & 1u) ==
                   (copy.io.output_compare.output_high & 1u) &&
               interrupt_flag(cpu, 0u) == interrupt_flag(&copy, 0u) &&
               cpu->device_cycles == copy.device_cycles,
           "batched alternate OC clock matches stepped execution");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[0], 2u);
    dspic33_write_word(cpu, timer_controls[0], 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 11u);
    expect(state, dspic33_copy(&copy, cpu), "copy timer-synchronized OC state");
    expect(state, dspic33_device_advance(cpu, 25u),
           "batch advance repeated timer synchronization");
    for (cycle = 0u; cycle < 25u; cycle++) {
        if (!dspic33_device_advance(&copy, 1u)) {
            break;
        }
    }
    expect(state, cycle == 25u, "step repeated timer synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == dspic33_read_word(&copy, 0x099eu) &&
               dspic33_read_word(cpu, timer_registers[0]) ==
                   dspic33_read_word(&copy, timer_registers[0]) &&
               interrupt_flag(cpu, 15u) == interrupt_flag(&copy, 15u) &&
               cpu->events.count == copy.events.count,
           "batched timer synchronization matches stepped execution");
    dspic33_destroy(&copy);
}

static void timer_clock_synchronization_cases(OutputCompareConformance* state,
                                              Dspic33* cpu) {
    static const uint8_t selections[] = {4u, 0u, 1u, 2u, 3u};
    uint8_t timer;
    for (timer = 0u; timer < 5u; timer++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[timer], 2u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        dspic33_write_word(cpu, 0x0904u, 100u);
        dspic33_write_word(cpu, 0x0906u, 50u);
        dspic33_write_word(cpu, 0x0900u,
                           (uint16_t)((uint16_t)selections[timer] << 10u | 6u));
        dspic33_write_word(cpu, 0x0902u, (uint16_t)(11u + timer));
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance common timer clock to synchronization match");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == 2u &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
               "timer match requests synchronization after common clock edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance common clock through synchronization reset");
        expect(state, dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
               "common timer clock resets OC on following edge");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[timer], 2u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        dspic33_write_word(cpu, 0x0904u, 4u);
        dspic33_write_word(cpu, 0x0906u, 2u);
        dspic33_write_word(cpu, 0x0900u,
                           (uint16_t)((uint16_t)selections[timer] << 10u | 6u));
        dspic33_write_word(cpu, 0x0902u,
                           (uint16_t)(COMPARE_TRIGGER | (uint16_t)(11u + timer)));
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance common timer clock to trigger match");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == 0u &&
                   (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u,
               "timer match releases trigger without same-edge count");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance common timer clock after trigger");
        expect(state, dspic33_read_word(cpu, 0x0908u) == 1u,
               "common timer clock counts after trigger edge");
    }
}

static void cross_timer_source_ordering_cases(OutputCompareConformance* state,
                                              Dspic33* cpu) {
    static const uint8_t source_timers[] = {0u, 1u};
    static const uint8_t clock_timers[] = {1u, 0u};
    size_t order;
    for (order = 0u; order < 2u; order++) {
        uint8_t source_timer = source_timers[order];
        uint8_t clock_timer = clock_timers[order];
        uint16_t clock_selection = clock_timer == 0u ? 0x1000u : 0u;
        uint16_t source = (uint16_t)(11u + source_timer);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[source_timer], 1u);
        dspic33_write_word(cpu, timer_periods[clock_timer], 100u);
        dspic33_write_word(cpu, timer_controls[source_timer], 0x8000u);
        dspic33_write_word(cpu, timer_controls[clock_timer], 0x8000u);
        configure_compare_mode(cpu, 0u, 6u, 100u, 50u,
                               (uint16_t)(COMPARE_TRIGGER | source));
        dspic33_write_word(cpu, 0x0900u, (uint16_t)(clock_selection | 6u));
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
                   dspic33_read_word(cpu, 0x0908u) == 0u,
               "cross-timer trigger does not count on the source edge");
        expect(state,
               dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 1u,
               "cross-timer trigger counts on the following selected clock");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[source_timer], 1u);
        dspic33_write_word(cpu, timer_periods[clock_timer], 100u);
        dspic33_write_word(cpu, timer_controls[clock_timer], 0x8000u);
        configure_compare_mode(cpu, 0u, 6u, 100u, 50u, source);
        dspic33_write_word(cpu, 0x0900u, (uint16_t)(clock_selection | 6u));
        expect(state,
               dspic33_device_advance(cpu, 5u) && dspic33_read_word(cpu, 0x0908u) == 5u,
               "cross-timer synchronization starts from a nonzero timer");
        dspic33_write_word(cpu, timer_controls[source_timer], 0x8000u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, 0x0908u) == 6u &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
               "cross-timer synchronization remains pending on the source edge");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, 0x0908u) == 0u &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) == 0u,
               "cross-timer synchronization resets on the following selected clock");
    }
}

static void synchronization_control_cases(OutputCompareConformance* state,
                                          Dspic33* cpu) {
    uint16_t generation;
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 2u, 1u, 1u);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance alternate self-selection period");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u,
           "alternate self-selection retains final period value");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance alternate self-selection rollover");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
           "channel-number self-selection uses secondary period");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance channel-number self-selection after rollover");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 1u &&
               (cpu->io.output_compare.sync_reset_pending & 1u) == 0u,
           "alternate self-selection does not queue a duplicate reset");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 2u, 1u, COMPARE_NO_SYNC);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 1u));
    dspic33_write_word(cpu, 0x0900u, 0u);
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u,
           "disabling OC source emits documented trigger pulse");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 29u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "queue synchronization before source replacement");
    expect(state, (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u,
           "old source leaves synchronization pending");
    dspic33_write_word(cpu, 0x0998u, 30u);
    expect(state, (cpu->io.output_compare.sync_reset_pending & 0x8000u) == 0u,
           "source replacement cancels pending synchronization");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance stale synchronization event");
    expect(state, dspic33_read_word(cpu, 0x099eu) == 2u && !interrupt_flag(cpu, 15u),
           "stale synchronization event cannot reset replaced source");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
    generation = cpu->io.output_compare.timer_generation[15u];
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "emit repeated trigger pulses on one cycle");
    expect(state,
           (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u &&
               cpu->io.output_compare.timer_generation[15u] ==
                   (uint16_t)(generation + 1u),
           "trigger status coalesces repeated source pulses");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, COMPARE_TRIGGER);
    dspic33_write_word(cpu, 0x0998u,
                       (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS));
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance software-triggered OC before clear");
    dspic33_write_word(cpu, 0x0998u, COMPARE_TRIGGER);
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 2u &&
               (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u,
           "software trigger clear waits for the next clock reset");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance software trigger clear edge");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 0u &&
               (cpu->io.output_compare.sync_reset_pending & 0x8000u) == 0u &&
               (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "software trigger clear resets and holds the timer");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 2u, 1u, (uint16_t)(COMPARE_TRIGGER | 29u));
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 1u));
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 4u),
           "advance triggered OC source through its period");
    expect(state,
           (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x099eu) == 0u,
           "triggered OC period emits downstream trigger output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance downstream OC after triggered source period");
    expect(state, dspic33_read_word(cpu, 0x099eu) == 1u,
           "downstream OC counts after triggered source output");
}

static void alternate_clock_control_cases(OutputCompareConformance* state,
                                          Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance OC with selected timer stopped");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u,
           "stopped selected timer supplies no OC clocks");
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    expect(state, dspic33_device_advance(cpu, 1u), "start selected timer clock");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 1u,
           "started selected timer advances OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8040u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance gated selected timer while gate low");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u,
           "low timer gate suppresses selected OC clock");
    expect(state,
           dspic33_timer_gate(cpu, 1u, true, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_device_advance(cpu, 2u),
           "raise selected timer gate and advance");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u,
           "high timer gate supplies selected OC clocks");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8002u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state,
           dspic33_timer_pulse(cpu, 1u, 3u, 0u) && dspic33_device_advance(cpu, 0u),
           "pulse external selected timer clock");
    expect(state,
           dspic33_read_word(cpu, timer_registers[1]) == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "external timer pulses clock OC after synchronizer start");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[0], 100u);
    dspic33_write_word(cpu, timer_controls[0], 0x8002u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x1006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state,
           dspic33_timer_pulse(cpu, 0u, 3u, 0u) && dspic33_device_advance(cpu, 0u),
           "pulse asynchronous Timer1 source");
    expect(state,
           dspic33_read_word(cpu, timer_registers[0]) == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "asynchronous Timer1 clock is unavailable to OC");
    dspic33_write_word(cpu, timer_controls[0], 0x8006u);
    expect(state,
           dspic33_timer_pulse(cpu, 0u, 3u, 0u) && dspic33_device_advance(cpu, 0u),
           "pulse synchronized external Timer1 source");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u,
           "synchronized Timer1 clock advances OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_periods[2], 0u);
    dspic33_write_word(cpu, timer_controls[1], 0x8008u);
    configure_compare_mode(cpu, 0u, 6u, 100u, 50u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    configure_compare_mode(cpu, 1u, 6u, 100u, 50u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x090au, 0x0406u);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance paired Timer2 and Timer3 clocks");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 3u &&
               dspic33_read_word(cpu, 0x0912u) == 3u &&
               dspic33_read_word(cpu, timer_registers[1]) == 3u &&
               dspic33_read_word(cpu, timer_registers[2]) == 0u,
           "paired low and high timer selections share clock edges");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    configure_compare_mode(cpu, 0u, 6u, 100u, 50u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance FP clock before live source change");
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance Timer2 after live OC clock change");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u,
           "live FP to Timer2 change preserves timer phase without double count");
    dspic33_write_word(cpu, 0x0900u, COMPARE_FP_EDGE_PWM);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance FP after restoring live OC clock");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 3u,
           "live Timer2 to FP change preserves timer phase");
}

static void alternate_instruction_activation_cases(OutputCompareConformance* state,
                                                   Dspic33* cpu) {
    static const uint32_t program[] = {0x200040u, 0x884820u, 0x200020u, 0x884830u,
                                       0x200060u, 0x884800u, 0x000000u};
    size_t index;
    bool loaded = true;
    bool ran = true;
    dspic33_reset(cpu, 0x200u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    for (index = 0u; index < sizeof(program) / sizeof(program[0]); index++) {
        loaded = loaded && dspic33_load_program_word(
                               cpu, 0x200u + (uint32_t)(index * 2u), program[index]);
    }
    expect(state, loaded, "load alternate-clock activation sequence");
    for (index = 0u; index < 6u; index++) {
        ran = ran && dspic33_step(cpu) == DSPIC33_RUNNING;
    }
    expect(state,
           ran && dspic33_read_word(cpu, 0x0908u) == 0u &&
               dspic33_read_word(cpu, timer_registers[1]) != 0u,
           "alternate OC enable instruction does not consume timer clocks");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "alternate OC starts on first clock after enabling instruction");
}

static void trigger_instruction_transition_cases(OutputCompareConformance* state,
                                                 Dspic33* cpu) {
    static const uint32_t fp_program[] = {0x200c00u, 0x884810u, 0x000000u, 0x000000u,
                                          0x200800u, 0x884810u, 0x000000u};
    static const uint32_t timer_program[] = {0x200c00u, 0x884810u, 0x200800u, 0x884810u,
                                             0x000000u};
    size_t index;
    bool loaded = true;

    dspic33_reset(cpu, 0x200u);
    configure_compare_mode(cpu, 0u, 6u, 20u, 10u, COMPARE_TRIGGER);
    for (index = 0u; index < sizeof(fp_program) / sizeof(fp_program[0]); index++) {
        loaded = loaded && dspic33_load_program_word(
                               cpu, 0x200u + (uint32_t)(index * 2u), fp_program[index]);
    }
    expect(state, loaded, "load FP trigger-status instruction sequence");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "FP trigger set instruction does not advance OC timer");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "FP trigger set takes effect on the following clock");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 2u &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 3u,
           "FP trigger timer advances before clear instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 3u,
           "FP trigger clear instruction preserves OC timer");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "FP trigger clear resets OC timer on the following clock");

    loaded = true;
    dspic33_reset(cpu, 0x200u);
    configure_compare_mode(cpu, 0u, 6u, 20u, 10u, COMPARE_TRIGGER);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    for (index = 0u; index < sizeof(timer_program) / sizeof(timer_program[0]);
         index++) {
        loaded =
            loaded && dspic33_load_program_word(cpu, 0x200u + (uint32_t)(index * 2u),
                                                timer_program[index]);
    }
    expect(state, loaded && dspic33_step(cpu) == DSPIC33_RUNNING,
           "load prescaled Timer2 trigger-status instruction sequence");
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8010u);
    expect(state, dspic33_device_advance(cpu, 7u),
           "advance Timer2 prescaler before trigger set instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "Timer2 trigger set instruction does not consume selected clock");
    expect(state,
           dspic33_device_advance(cpu, 7u) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 1u,
           "Timer2 trigger set takes effect on the following selected clock");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_device_advance(cpu, 6u) &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "align Timer2 prescaler before trigger clear instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "Timer2 trigger clear instruction preserves OC timer");
    expect(state,
           dspic33_device_advance(cpu, 7u) && dspic33_read_word(cpu, 0x0908u) == 1u &&
               dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 0u,
           "Timer2 trigger clear resets OC timer on the following selected clock");
}

static void synchronization_lifecycle_cases(OutputCompareConformance* state,
                                            Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize synchronization lifecycle copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 29u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u) && dspic33_copy(&copy, cpu),
           "copy pending OC synchronization");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance original and copied synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 0u &&
               dspic33_read_word(&copy, 0x099eu) == 0u && interrupt_flag(cpu, 15u) &&
               interrupt_flag(&copy, 15u) &&
               cpu->io.output_compare.sync_reset_pending ==
                   copy.io.output_compare.sync_reset_pending,
           "copied synchronization completes independently");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "set trigger state before reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.output_compare.sync_reset_pending == 0u &&
               cpu->io.output_compare.deferred_sync_pulses == 0u &&
               !cpu->io.output_compare.clock_advancing &&
               dspic33_read_word(cpu, 0x099eu) == 0u && cpu->events.count == 0u,
           "reset clears OC synchronization and trigger lifecycle");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 24u));
    dspic33_write_word(cpu, 0x0a84u, 0x8040u);
    cpu->device_cycles = UINT64_MAX;
    expect(
        state,
        dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 0u) &&
            !dspic33_device_advance(cpu, 0u),
        "process trigger source at maximum device cycle");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0996u) & 7u) == 0u && cpu->events.count == 0u,
           "trigger reschedule failure aborts OC channel");
    dspic33_destroy(&copy);
}

static void channel_trigger_matrix_cases(OutputCompareConformance* state,
                                         Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, channel, 6u, 4u, 2u,
                               (uint16_t)(COMPARE_TRIGGER | 29u));
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) &
                    COMPARE_TRIGGER_STATUS) == 0u,
               "channel trigger matrix starts held");
        expect(state,
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "channel trigger matrix emits source");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) &
                    COMPARE_TRIGGER_STATUS) != 0u,
               "channel trigger matrix sets hardware status");
        expect(state, dspic33_device_advance(cpu, 1u),
               "channel trigger matrix advances first count");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u,
               "channel trigger matrix counts after source");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 2u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 1u);
        dspic33_write_word(cpu, base,
                           (uint16_t)(COMPARE_FP | COMPARE_TRIGGER_ONESHOT | 6u));
        dspic33_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(COMPARE_TRIGGER | 29u));
        dspic33_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS | 29u));
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 2u)) &
                COMPARE_TRIGGER_STATUS) == 0u,
               "one-shot channel rejects software trigger set");
        expect(state,
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "one-shot channel accepts hardware trigger");
        dspic33_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(COMPARE_TRIGGER | 29u));
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 2u)) &
                COMPARE_TRIGGER_STATUS) != 0u,
               "one-shot channel rejects software trigger clear");
        expect(state, dspic33_device_advance(cpu, 3u),
               "one-shot channel advances to hardware clear");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 2u)) &
                COMPARE_TRIGGER_STATUS) == 0u,
               "one-shot channel clears trigger at rollover");
    }
}

static void trigger_source_negative_cases(OutputCompareConformance* state,
                                          Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
    dspic33_raise_interrupt(cpu, 20u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance after software INT1 flag set");
    expect(state,
           (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x099eu) == 0u,
           "software interrupt flag does not synthesize external trigger edge");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0142u, 0u);
    dspic33_write_word(cpu, 0x0140u, 0x1c23u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 16u));
    expect(state,
           dspic33_input_capture_input(cpu, 0u, true, 0u) &&
               dspic33_device_advance(cpu, 3u),
           "capture without interrupt advances");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "capture event without IC interrupt does not trigger OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0a84u, 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 24u));
    expect(
        state,
        dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 0u) &&
            dspic33_device_advance(cpu, 0u),
        "comparator transition without event advances");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "comparator output without compare event does not trigger OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0360u, 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 27u));
    dspic33_write_word(cpu, 0x0360u, 0x8002u);
    dspic33_write_word(cpu, 0x0360u, 0x8000u);
    expect(state, dspic33_device_advance(cpu, 12u),
           "complete ADC2 conversion beside OC trigger");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "ADC2 completion does not drive ADC1 trigger source");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 1u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 11u));
    expect(state, dspic33_device_advance(cpu, 2u), "advance nonselected timer period");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "Timer2 period does not drive Timer1 trigger selection");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 30u));
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 21u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "raise unrelated interrupt beside INT2 trigger");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "unrelated interrupt does not drive external trigger source");
}

static void coexistence_cases(OutputCompareConformance* state, Dspic33* cpu) {
    static const uint8_t collision_modes[] = {1u, 5u, 6u};
    uint16_t capture_base = 0x01b8u;
    size_t index;
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

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, capture_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, capture_base, 0x1c03u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance free-running OC to secondary compare");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 4u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
           "free-running OC sync output waits one clock after secondary compare");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance free-running OC synchronization output");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 5u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "free-running OC triggers downstream without resetting its timer");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance triggered capture timer after OC synchronization");
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(capture_base + 6u)) == 1u &&
               dspic33_read_word(cpu, 0x0908u) == 6u,
           "OC synchronization starts the downstream timer on the next clock");

    for (index = 0u; index < sizeof(collision_modes) / sizeof(collision_modes[0]);
         index++) {
        uint8_t mode = collision_modes[index];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, capture_base, 0u);
        dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
        dspic33_write_word(cpu, capture_base, 0x1c03u);
        configure_compare_mode(cpu, 0u, mode, 4u, 5u, COMPARE_NO_SYNC);
        expect(state, dspic33_device_advance(cpu, 4u),
               "advance before coincident OC compare and synchronization");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
               "coincident OC synchronization does not occur at compare value");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance coincident OC compare and synchronization");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == 5u && output_is(cpu, 0u, false) &&
                   (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) !=
                       0u,
               "coincident primary match retains free-running synchronization");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance coincident OC output pipeline");
        expect(state, output_is(cpu, 0u, mode != 6u),
               "coincident primary match preserves documented output timing");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, capture_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, capture_base, 0x1c03u);
    configure_compare_mode(cpu, 0u, 5u, 4u, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance free-running OC to first synchronization output");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "free-running OC emits first synchronization output");
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, 0x0904u, 8u);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance before rewritten secondary synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 8u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
           "rewritten secondary compare waits for its following clock");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance rewritten secondary synchronization output");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 9u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "later secondary rewrite emits another synchronization before rollover");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, capture_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, capture_base, 0x1c03u);
    configure_compare_mode(cpu, 0u, 6u, UINT16_MAX, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, UINT16_MAX),
           "advance free-running OC to maximum secondary compare");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == UINT16_MAX &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
           "maximum secondary compare precedes synchronization output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance maximum secondary compare through rollover");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "maximum secondary compare synchronizes on rollover without duplication");
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
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance delayed OC output before copy");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 2u && dspic33_copy(&copy, cpu),
           "copy delayed OC pipeline state");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance copied delayed OC output");
    expect(state,
           output_is(cpu, 0u, true) && output_is(&copy, 0u, true) &&
               !interrupt_flag(cpu, 0u) && !interrupt_flag(&copy, 0u),
           "copy preserves delayed OC output event");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u),
           "advance copied delayed OC interrupt");
    expect(state,
           interrupt_flag(cpu, 0u) && interrupt_flag(&copy, 0u) &&
               dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(&copy, 0x0908u),
           "copy preserves delayed OC interrupt event");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance delayed OC before disable");
    dspic33_write_word(cpu, 0x0900u, 0u);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance disabled delayed OC events");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) && !interrupt_flag(cpu, 0u) &&
               cpu->events.count == 0u,
           "disable cancels delayed OC output and interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance delayed OC before restart");
    dspic33_write_byte(cpu, 0x0900u, 2u);
    expect(state,
           output_is(cpu, 0u, true) && cpu->io.output_compare.phase[0] == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "mode change restarts delayed OC pipeline");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance restarted OC through stale events");
    expect(state,
           output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 2u,
           "stale delayed events cannot alter restarted OC");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance restarted OC output pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "restarted OC uses only its new delayed output");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance delayed OC before reset");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance reset delayed OC events");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) &&
               cpu->io.output_compare.output_high == 0u &&
               cpu->io.output_compare.phase[0] == 0u && cpu->events.count == 0u &&
               !interrupt_flag(cpu, 0u),
           "reset cancels delayed OC output and interrupt");

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

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX - 3u;
    configure_compare_mode(cpu, 0u, 1u, 4u, 1u, COMPARE_SELF_SYNC);
    expect(state, !dspic33_device_advance(cpu, 1u),
           "delayed OC interrupt overflow stops advance");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0900u) & 7u) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u &&
               cpu->io.output_compare.phase[0] == 0u &&
               !dspic33_output_compare_output(cpu, 0u, &high) &&
               cpu->events.count == 0u && !interrupt_flag(cpu, 0u),
           "delayed OC schedule failure aborts the pipeline");
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
        single_compare_cases(&state, &cpu);
        dual_compare_cases(&state, &cpu);
        center_aligned_cases(&state, &cpu);
        immediate_compare_write_cases(&state, &cpu);
        output_control_cases(&state, &cpu);
        channel_mode_matrix_cases(&state, &cpu);
        one_shot_restart_cases(&state, &cpu);
        unsupported_cases(&state, &cpu);
        alternate_clock_cases(&state, &cpu);
        trigger_source_cases(&state, &cpu);
        trigger_one_shot_cases(&state, &cpu);
        synchronization_source_cases(&state, &cpu);
        alternate_clock_batch_cases(&state, &cpu);
        timer_clock_synchronization_cases(&state, &cpu);
        cross_timer_source_ordering_cases(&state, &cpu);
        synchronization_control_cases(&state, &cpu);
        alternate_clock_control_cases(&state, &cpu);
        alternate_instruction_activation_cases(&state, &cpu);
        trigger_instruction_transition_cases(&state, &cpu);
        synchronization_lifecycle_cases(&state, &cpu);
        channel_trigger_matrix_cases(&state, &cpu);
        trigger_source_negative_cases(&state, &cpu);
        coexistence_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[output-compare-summary] cases=%u passed=%u failed=%u\n", state.cases,
           state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
