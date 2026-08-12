#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} InputCaptureConformance;

static const uint8_t capture_irqs[DSPIC33_INPUT_CAPTURE_COUNT] = {
    1u,  5u,   37u,  38u,  39u,  40u,  22u,  23u,
    93u, 125u, 127u, 129u, 135u, 137u, 139u, 141u};

enum {
    CAPTURE_BASE = 0x0140u,
    CAPTURE_STRIDE = 0x0008u,
    CAPTURE_FP_RISING = 0x1c03u,
    CAPTURE_32_BIT = 0x0100u,
    CAPTURE_TRIGGER = 0x00c0u,
    CAPTURE_NOT_EMPTY = 0x0008u,
    CAPTURE_OVERFLOW = 0x0010u,
    CAPTURE_DMA_DESTINATION = 0x3000u,
    CAPTURE_VECTOR = 0x0200u,
    CAPTURE_PMD_LOW = 0x0762u,
    CAPTURE_PMD_HIGH = 0x0768u,
    COMPARE_BASE = 0x0900u,
    COMPARE_STRIDE = 0x000au,
    COMPARATOR_BASE = 0x0a84u,
    COMPARATOR_STRIDE = 0x0008u
};

static const uint16_t timer_registers[5] = {0x0100u, 0x0106u, 0x010au, 0x0114u,
                                            0x0118u};
static const uint16_t timer_periods[5] = {0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu};
static const uint16_t timer_controls[5] = {0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u};
static const uint16_t capture_timer_sources[5] = {0x1000u, 0x0400u, 0x0000u, 0x0800u,
                                                  0x0c00u};

static void expect(InputCaptureConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[input-capture-failed] %s\n", name);
    }
}

static uint16_t capture_base(uint8_t channel) {
    return (uint16_t)(CAPTURE_BASE + channel * CAPTURE_STRIDE);
}

static uint16_t capture_pmd_address(uint8_t channel) {
    return channel < 8u ? CAPTURE_PMD_LOW : CAPTURE_PMD_HIGH;
}

static uint16_t capture_pmd_mask(uint8_t channel) {
    return (uint16_t)(1u << (8u + channel % 8u));
}

static bool interrupt_flag(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = capture_irqs[channel];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = capture_irqs[channel];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t bit = (uint16_t)(1u << (irq % 16u));
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~bit));
}

static void configure_capture(Dspic33* cpu, uint8_t channel, uint8_t interval,
                              bool paired) {
    uint16_t base = capture_base(channel);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u),
                       (uint16_t)(CAPTURE_TRIGGER | (paired ? CAPTURE_32_BIT : 0u)));
    clear_interrupt(cpu, channel);
    dspic33_write_word(cpu, base, (uint16_t)(CAPTURE_FP_RISING | (interval << 5u)));
}

static void configure_capture_source(Dspic33* cpu, uint8_t channel,
                                     uint16_t timer_source, bool triggered,
                                     bool running, uint8_t sync_source, bool paired) {
    uint16_t base = capture_base(channel);
    uint16_t control2 = sync_source | (paired ? CAPTURE_32_BIT : 0u);
    if (triggered) {
        control2 |= 0x0080u;
        if (running) {
            control2 |= 0x0040u;
        }
    }
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), control2);
    clear_interrupt(cpu, channel);
    dspic33_write_word(cpu, base, (uint16_t)(timer_source | 0x0003u));
}

static void configure_timer_source(Dspic33* cpu, uint8_t timer, uint16_t period,
                                   uint16_t prescale) {
    dspic33_write_word(cpu, timer_controls[timer], 0u);
    dspic33_write_word(cpu, timer_registers[timer], 0u);
    dspic33_write_word(cpu, timer_periods[timer], period);
    dspic33_write_word(cpu, timer_controls[timer], (uint16_t)(0x8000u | prescale));
}

static void configure_compare_source(Dspic33* cpu, uint8_t channel) {
    uint16_t base = (uint16_t)(COMPARE_BASE + channel * COMPARE_STRIDE);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
    dspic33_write_word(cpu, base, 0x1c06u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x001fu);
}

static void configure_comparator_source(Dspic33* cpu, uint8_t comparator) {
    uint16_t base = (uint16_t)(COMPARATOR_BASE + comparator * COMPARATOR_STRIDE);
    dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u,
                             0u);
    dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 100u,
                             0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_write_word(cpu, base, 0x8000u);
}

static bool rising_edge(Dspic33* cpu, uint8_t channel) {
    return dspic33_input_capture_input(cpu, channel, false, 0u) &&
           dspic33_device_advance(cpu, 0u) &&
           dspic33_input_capture_input(cpu, channel, true, 0u) &&
           dspic33_device_advance(cpu, 1u);
}

static bool set_level(Dspic33* cpu, uint8_t channel, bool high) {
    return dspic33_input_capture_input(cpu, channel, high, 0u) &&
           dspic33_device_advance(cpu, 1u);
}

static void configure_dma(Dspic33* cpu, uint8_t request, uint16_t pad,
                          uint16_t destination) {
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, request);
    dspic33_write_word(cpu, 0x0b04u, destination);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, pad);
    dspic33_write_word(cpu, 0x0b0eu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0x8001u);
}

static void access_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t base = capture_base(channel);
        expect(state, dspic33_read_word(cpu, base) == 0u, "ICCON1 reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x000du,
               "ICCON2 reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "ICBUF deterministic empty reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0u,
               "ICTMR deterministic reset");
        dspic33_write_word(cpu, base, 0xffffu);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0xffffu);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 0xffffu);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0xffffu);
        expect(state, dspic33_read_word(cpu, base) == 0x3c67u, "ICCON1 access mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x01dfu,
               "ICCON2 access mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "ICBUF read only");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0u,
               "ICTMR read only");
    }
}

static void fifo_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t base = capture_base(channel);
        uint8_t sample;
        dspic33_reset(cpu, 0u);
        configure_capture(cpu, channel, 3u, false);
        for (sample = 0u; sample < DSPIC33_INPUT_CAPTURE_FIFO_SIZE; sample++) {
            expect(state, rising_edge(cpu, channel), "capture FIFO edge");
        }
        expect(state, cpu->io.input_capture.fifo[channel].count == 4u,
               "capture FIFO fills to four");
        expect(state,
               (dspic33_read_word(cpu, base) & CAPTURE_NOT_EMPTY) != 0u &&
                   (dspic33_read_word(cpu, base) & CAPTURE_OVERFLOW) == 0u,
               "capture FIFO status before overflow");
        expect(state,
               cpu->io.input_capture.fifo[channel]
                       .words[cpu->io.input_capture.fifo[channel].head] == 1u,
               "capture FIFO exposes oldest word");
        expect(state, rising_edge(cpu, channel), "capture overflow edge");
        expect(state,
               cpu->io.input_capture.fifo[channel].count == 4u &&
                   (dspic33_read_word(cpu, base) & CAPTURE_OVERFLOW) != 0u,
               "fifth capture sets overflow and preserves FIFO");
        dspic33_write_word(cpu, base, (uint16_t)(CAPTURE_FP_RISING | (3u << 5u)));
        expect(
            state,
            (dspic33_read_word(cpu, base) & (CAPTURE_NOT_EMPTY | CAPTURE_OVERFLOW)) ==
                (CAPTURE_NOT_EMPTY | CAPTURE_OVERFLOW),
            "status bits ignore software writes");
        expect(state, !interrupt_flag(cpu, channel),
               "fourth capture interrupt remains delayed");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance capture interrupt delay");
        expect(state, interrupt_flag(cpu, channel),
               "fourth capture raises channel interrupt");
        for (sample = 0u; sample < DSPIC33_INPUT_CAPTURE_FIFO_SIZE; sample++) {
            expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == sample + 1u,
                   "capture FIFO preserves order");
        }
        expect(state,
               cpu->io.input_capture.fifo[channel].count == 0u &&
                   (dspic33_read_word(cpu, base) &
                    (CAPTURE_NOT_EMPTY | CAPTURE_OVERFLOW)) == 0u,
               "empty read clears FIFO status");
    }
}

static void interrupt_rate_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint8_t interval;
    for (interval = 0u; interval < 4u; interval++) {
        uint8_t sample;
        dspic33_reset(cpu, 0u);
        configure_capture(cpu, 0u, interval, false);
        for (sample = 0u; sample <= interval; sample++) {
            expect(state, rising_edge(cpu, 0u), "capture interrupt-rate edge");
            expect(state, !interrupt_flag(cpu, 0u),
                   "capture interrupt not raised on FIFO write");
        }
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance first interrupt-delay cycle");
        expect(state, !interrupt_flag(cpu, 0u), "capture interrupt waits two cycles");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance second interrupt-delay cycle");
        expect(state, interrupt_flag(cpu, 0u),
               "capture interrupt follows selected interval");
    }
}

static void zero_interval_overflow_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint8_t sample;
    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    for (sample = 0u; sample < DSPIC33_INPUT_CAPTURE_FIFO_SIZE; sample++) {
        expect(state, rising_edge(cpu, 0u), "ICI zero FIFO edge");
    }
    expect(state, dspic33_device_advance(cpu, 2u), "drain ICI zero interrupt delay");
    clear_interrupt(cpu, 0u);
    configure_dma(cpu, capture_irqs[0], 0x0144u, CAPTURE_DMA_DESTINATION);
    expect(state, rising_edge(cpu, 0u), "ICI zero overflow edge");
    expect(
        state,
        cpu->io.input_capture.fifo[0].count == 3u &&
            (dspic33_read_word(cpu, 0x0140u) & CAPTURE_OVERFLOW) == 0u &&
            cpu->io.input_capture.fifo[0].words[cpu->io.input_capture.fifo[0].head] ==
                2u,
        "ICI zero fifth capture discards sample without overflow");
    expect(state,
           dspic33_read_word(cpu, CAPTURE_DMA_DESTINATION) == 1u &&
               !interrupt_flag(cpu, 0u),
           "ICI zero overflow preserves immediate DMA and delayed interrupt");
    expect(state, dspic33_device_advance(cpu, 1u), "complete ICI zero overflow DMA");
    expect(state, !interrupt_flag(cpu, 0u),
           "ICI zero overflow interrupt waits two cycles");
    expect(state, dspic33_device_advance(cpu, 1u),
           "complete ICI zero overflow interrupt delay");
    expect(state, interrupt_flag(cpu, 0u),
           "ICI zero overflow still raises capture interrupt");
}

static void mode_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint16_t base = capture_base(0u);
    uint8_t edge;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1803u);
    expect(state, rising_edge(cpu, 0u), "non-FP edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "non-FP timer source is excluded");
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x00c1u);
    dspic33_write_word(cpu, base, CAPTURE_FP_RISING);
    expect(state, rising_edge(cpu, 0u), "nonsoftware sync edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 1u,
           "hardware source selection preserves software trigger");
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x0080u);
    dspic33_write_word(cpu, base, CAPTURE_FP_RISING);
    expect(state, rising_edge(cpu, 0u), "inactive trigger edge advances");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u &&
               cpu->io.input_capture.fifo[0].words[0] == 0u &&
               cpu->io.input_capture.timer[0] == 0u,
           "cleared trigger status captures held-zero timer");
    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed every-edge input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c01u);
    expect(state, set_level(cpu, 0u, true) && set_level(cpu, 0u, false),
           "every-edge capture transitions advance");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 2u &&
               cpu->io.input_capture.fifo[0].words[0] == 1u &&
               cpu->io.input_capture.fifo[0].words[1] == 2u,
           "every-edge mode captures rising and falling transitions");
    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed every-edge interrupt input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c61u);
    expect(state, set_level(cpu, 0u, true), "every-edge interrupt event advances");
    expect(state, dspic33_device_advance(cpu, 2u),
           "every-edge interrupt delay advances");
    expect(state, interrupt_flag(cpu, 0u),
           "every-edge mode ignores capture interrupt interval");
    clear_interrupt(cpu, 0u);
    expect(state,
           set_level(cpu, 0u, false) && set_level(cpu, 0u, true) &&
               set_level(cpu, 0u, false),
           "every-edge FIFO reaches four entries");
    expect(state, set_level(cpu, 0u, true), "every-edge overflow event advances");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 4u &&
               (dspic33_read_word(cpu, base) & CAPTURE_OVERFLOW) == 0u,
           "every-edge overflow discards sample without ICOV");

    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed fourth-edge input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c02u);
    expect(state, set_level(cpu, 0u, true) && set_level(cpu, 0u, false),
           "falling capture transitions advance");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u &&
               cpu->io.input_capture.fifo[0].words[0] == 2u,
           "falling mode captures only falling transitions");

    configure_capture(cpu, 0u, 0u, false);
    expect(state, rising_edge(cpu, 0u), "supported rising edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 1u,
           "FP software-triggered rising capture succeeds");

    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed sixteenth-edge input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c04u);
    for (edge = 0u; edge < 3u; edge++) {
        expect(state, set_level(cpu, 0u, true) && set_level(cpu, 0u, false),
               "fourth-edge prescaler advances below threshold");
    }
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "fourth-edge prescaler suppresses first three rising edges");
    expect(state, set_level(cpu, 0u, true), "fourth rising edge advances");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u &&
               cpu->io.input_capture.fifo[0].words[0] == 7u,
           "fourth-edge prescaler captures fourth rising edge");

    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed switched prescaler input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c05u);
    for (edge = 0u; edge < 15u; edge++) {
        expect(state, set_level(cpu, 0u, false) && set_level(cpu, 0u, true),
               "sixteenth-edge prescaler advances below threshold");
    }
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "sixteenth-edge prescaler suppresses first fifteen rising edges");
    expect(state, set_level(cpu, 0u, false) && set_level(cpu, 0u, true),
           "sixteenth rising edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 1u,
           "sixteenth-edge prescaler captures sixteenth rising edge");

    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed unused-mode input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c04u);
    for (edge = 0u; edge < 3u; edge++) {
        expect(state, set_level(cpu, 0u, false) && set_level(cpu, 0u, true),
               "active-mode prescaler advances before mode switch");
    }
    dspic33_write_word(cpu, base, 0x1c05u);
    for (edge = 0u; edge < 13u; edge++) {
        expect(state, set_level(cpu, 0u, false) && set_level(cpu, 0u, true),
               "active-mode prescaler advances after mode switch");
    }
    expect(state, cpu->io.input_capture.fifo[0].count == 1u,
           "active-mode switch preserves prescaler count");

    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed reverse prescaler input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c05u);
    for (edge = 0u; edge < 5u; edge++) {
        expect(state, set_level(cpu, 0u, true) && set_level(cpu, 0u, false),
               "sixteenth-edge prescaler advances before reverse switch");
    }
    dspic33_write_word(cpu, base, 0x1c04u);
    for (edge = 0u; edge < 2u; edge++) {
        expect(state, set_level(cpu, 0u, true) && set_level(cpu, 0u, false),
               "fourth-edge prescaler advances below next phase boundary");
    }
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "reverse mode switch retains nonzero prescaler phase");
    expect(state, set_level(cpu, 0u, true),
           "fourth-edge prescaler reaches next phase boundary");
    expect(state, cpu->io.input_capture.fifo[0].count == 1u,
           "reverse active-mode switch captures at finite next boundary");

    dspic33_write_word(cpu, base, 0u);
    expect(state, set_level(cpu, 0u, false), "seed reset prescaler input low");
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c04u);
    for (edge = 0u; edge < 3u; edge++) {
        expect(state, set_level(cpu, 0u, true) && set_level(cpu, 0u, false),
               "prescaler advances before module disable");
    }
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, base, 0x1c04u);
    expect(state, set_level(cpu, 0u, true),
           "prescaler advances after module re-enable");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "module disable clears prescaler count");

    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), CAPTURE_TRIGGER);
    dspic33_write_word(cpu, base, 0x1c06u);
    expect(state, set_level(cpu, 0u, false) && set_level(cpu, 0u, true),
           "unused capture mode edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "unused capture mode remains disabled");
}

static void configure_capture_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = capture_irqs[channel];
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
    cpu->program[(0x0014u + irq * 2u) / 2u] = CAPTURE_VECTOR;
    cpu->w[15] = 0x1800u;
}

static void power_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint16_t base = capture_base(0u);

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state, set_level(cpu, 0u, true), "sleep rising capture edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u && !interrupt_flag(cpu, 0u),
           "normal capture mode stops in Sleep");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, set_level(cpu, 0u, true), "idle-running capture edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 1u,
           "ICSIDL clear continues capture in Idle");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    dspic33_write_word(cpu, base, (uint16_t)(CAPTURE_FP_RISING | 0x2000u));
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, set_level(cpu, 0u, true), "idle-stopped capture edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "ICSIDL set stops normal capture in Idle");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, base, 7u);
    configure_capture_interrupt(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state, set_level(cpu, 0u, true), "active interrupt-only edge advances");
    expect(state, !interrupt_flag(cpu, 0u) && cpu->io.input_capture.fifo[0].count == 0u,
           "interrupt-only mode is inactive outside Sleep and Idle");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, base, 7u);
    configure_capture_interrupt(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state, set_level(cpu, 0u, true), "sleep interrupt-only edge advances");
    expect(state, interrupt_flag(cpu, 0u) && cpu->io.input_capture.fifo[0].count == 0u,
           "interrupt-only Sleep edge raises IRQ without FIFO data");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_device_wake(cpu) &&
               cpu->last_interrupt == capture_irqs[0] && cpu->pc == CAPTURE_VECTOR,
           "interrupt-only Sleep edge wakes through capture vector");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, base, 7u);
    configure_capture_interrupt(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, set_level(cpu, 0u, true), "idle interrupt-only edge advances");
    expect(state, interrupt_flag(cpu, 0u) && cpu->io.input_capture.fifo[0].count == 0u,
           "interrupt-only Idle edge raises IRQ without FIFO data");
}

static void paired_firmware_case(InputCaptureConformance* state, Dspic33* cpu,
                                 uint8_t channel, uint8_t pin, uint16_t pps_register) {
    uint16_t first = capture_base(channel);
    uint16_t second = capture_base((uint8_t)(channel + 1u));
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, pps_register, pin);
    if (pin == 17u) {
        dspic33_write_word(cpu, 0x0e0eu, 0x0002u);
    }
    configure_capture(cpu, channel, 1u, true);
    configure_capture(cpu, (uint8_t)(channel + 1u), 1u, true);
    configure_capture_interrupt(cpu, channel);
    expect(state, dspic33_input_capture_pin(cpu, (uint8_t)(pin + 1u), true, 0u),
           "queue unmapped PPS edge");
    expect(state, dspic33_device_advance(cpu, 1u), "advance unmapped PPS edge");
    expect(state,
           cpu->io.input_capture.fifo[channel].count == 0u &&
               cpu->io.input_capture.fifo[channel + 1u].count == 0u,
           "unmapped PPS edge does not capture");
    expect(state, dspic33_input_capture_pin(cpu, pin, false, 0u),
           "queue mapped PPS low state");
    expect(state, dspic33_device_advance(cpu, 65534u),
           "advance paired counter toward wrap");
    expect(state, dspic33_input_capture_pin(cpu, pin, true, 0u),
           "queue first mapped PPS edge");
    expect(state, dspic33_device_advance(cpu, 1u), "capture first paired edge");
    expect(state,
           cpu->io.input_capture.fifo[channel].count == 1u &&
               cpu->io.input_capture.fifo[channel + 1u].count == 1u &&
               cpu->io.input_capture.fifo[channel].words[0] == 0u &&
               cpu->io.input_capture.fifo[channel + 1u].words[0] == 1u,
           "paired FIFO captures 32-bit wrap value");
    expect(state, !interrupt_flag(cpu, channel),
           "first paired capture does not meet ICI interval");
    expect(state, dspic33_input_capture_pin(cpu, pin, false, 0u),
           "queue paired falling state");
    expect(state, dspic33_device_advance(cpu, 0u), "apply paired falling state");
    expect(state, dspic33_input_capture_pin(cpu, pin, true, 0u),
           "queue second mapped PPS edge");
    expect(state, dspic33_device_advance(cpu, 1u), "capture second paired edge");
    expect(state, !interrupt_flag(cpu, channel),
           "paired interrupt remains delayed after FIFO write");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance paired interrupt first cycle");
    expect(state, !interrupt_flag(cpu, channel), "paired interrupt waits second cycle");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance paired interrupt second cycle");
    expect(state, interrupt_flag(cpu, channel) && dspic33_device_interrupt_pending(cpu),
           "paired channel IRQ becomes pending");
    expect(state,
           dspic33_device_service_interrupt(cpu) &&
               cpu->last_interrupt == capture_irqs[channel] &&
               cpu->pc == CAPTURE_VECTOR,
           "paired firmware channel vectors");
    expect(state, dspic33_read_word(cpu, (uint16_t)(first + 4u)) == 0u,
           "paired low FIFO first word");
    expect(state, dspic33_read_word(cpu, (uint16_t)(second + 4u)) == 1u,
           "paired high FIFO first word");
    expect(state, dspic33_read_word(cpu, (uint16_t)(first + 4u)) == 1u,
           "paired low FIFO second word");
    expect(state, dspic33_read_word(cpu, (uint16_t)(second + 4u)) == 1u,
           "paired high FIFO second word");
}

static void paired_cases(InputCaptureConformance* state, Dspic33* cpu) {
    paired_firmware_case(state, cpu, 0u, 17u, 0x06aeu);
    paired_firmware_case(state, cpu, 2u, 78u, 0x06b0u);
}

static void dma_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < 4u; channel++) {
        uint16_t destination = (uint16_t)(CAPTURE_DMA_DESTINATION + channel * 2u);
        dspic33_reset(cpu, 0u);
        configure_dma(cpu, capture_irqs[channel],
                      (uint16_t)(capture_base(channel) + 4u), destination);
        configure_capture(cpu, channel, 0u, false);
        expect(state, rising_edge(cpu, channel), "DMA capture edge");
        expect(state,
               dspic33_read_word(cpu, destination) == 1u &&
                   cpu->io.input_capture.fifo[channel].count == 0u,
               "IC1 through IC4 DMA consumes captured word");
        expect(state, dspic33_device_advance(cpu, 1u), "complete capture DMA transfer");
        expect(state, (dspic33_read_word(cpu, 0x0800u) & 0x0010u) != 0u,
               "capture DMA completion raises DMA0IF");
    }

    dspic33_reset(cpu, 0u);
    configure_dma(cpu, capture_irqs[0], (uint16_t)(capture_base(0u) + 4u),
                  CAPTURE_DMA_DESTINATION);
    configure_capture(cpu, 0u, 1u, false);
    expect(state, rising_edge(cpu, 0u), "ICI nonzero DMA-negative edge");
    expect(state,
           dspic33_read_word(cpu, CAPTURE_DMA_DESTINATION) == 0u &&
               cpu->io.input_capture.fifo[0].count == 1u,
           "ICI nonzero suppresses input capture DMA request");

    dspic33_reset(cpu, 0u);
    configure_dma(cpu, capture_irqs[4], (uint16_t)(capture_base(4u) + 4u),
                  CAPTURE_DMA_DESTINATION);
    configure_capture(cpu, 4u, 0u, false);
    expect(state, rising_edge(cpu, 4u), "IC5 DMA-negative edge");
    expect(state,
           dspic33_read_word(cpu, CAPTURE_DMA_DESTINATION) == 0u &&
               cpu->io.input_capture.fifo[4].count == 1u,
           "IC5 through IC16 have no capture DMA source");
}

static void lifecycle_cases(InputCaptureConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize input capture copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue capture before copy");
    expect(state, dspic33_device_advance(cpu, 0u), "schedule capture before copy");
    expect(state, dspic33_copy(&copy, cpu), "copy pending capture");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance original and copied capture");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u &&
               copy.io.input_capture.fifo[0].count == 1u &&
               dspic33_read_word(cpu, 0x0144u) == dspic33_read_word(&copy, 0x0144u),
           "copy retains capture state and event");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 3u, 0u, 1u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06aeu, 64u);
    configure_capture(cpu, 0u, 0u, false);
    expect(state,
           cpu->io.input_capture.pps_qualified == 1u &&
               cpu->io.input_capture.pps_selection[0] == 64u,
           "mapped physical capture baselines its current level");
    expect(state, dspic33_copy(&copy, cpu),
           "copy preserves mapped physical capture state");
    expect(state,
           dspic33_gpio_drive(cpu, 3u, 1u, 1u) &&
               dspic33_gpio_drive(&copy, 3u, 1u, 1u) &&
               dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u) &&
               cpu->io.input_capture.fifo[0].count == 1u &&
               copy.io.input_capture.fifo[0].count == 1u,
           "copied physical mappings capture the same later edge");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue capture before disable");
    expect(state, dspic33_device_advance(cpu, 0u), "schedule capture before disable");
    dspic33_write_word(cpu, 0x0140u, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance disabled stale events");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u && !interrupt_flag(cpu, 0u),
           "disable flushes FIFO and cancels pending capture and IRQ");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue capture before reset");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance reset capture queue");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 0u &&
               dspic33_read_word(cpu, 0x0140u) == 0u && cpu->events.count == 0u,
           "reset clears capture state and pending events");

    expect(state, !dspic33_input_capture_input(cpu, 16u, true, 0u),
           "reject invalid logical capture channel");
    expect(state, !dspic33_input_capture_pin(cpu, 0u, true, 0u),
           "reject PPS ground source");
    expect(state, !dspic33_input_capture_pin(cpu, 128u, true, 0u),
           "reject invalid PPS source");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    cpu->device_cycles = UINT64_MAX;
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue edge at final device cycle");
    expect(state, !dspic33_device_advance(cpu, 0u),
           "capture scheduling overflow stops advance");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u,
           "capture scheduling overflow reports queue error");
    dspic33_destroy(&copy);
}

static void pmd_channel_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t base = capture_base(channel);
        uint16_t pmd_address = capture_pmd_address(channel);
        uint16_t pmd_mask = capture_pmd_mask(channel);

        dspic33_reset(cpu, 0u);
        configure_capture(cpu, channel, 0u, false);
        dspic33_write_word(cpu, pmd_address, pmd_mask);
        dspic33_write_word(cpu, base, 0x3c03u);
        expect(state, dspic33_read_word(cpu, base) == 0x3c03u,
               "capture PMD write remains accessible for one cycle");
        expect(state, dspic33_device_advance(cpu, 1u),
               "capture PMD disable transition advances");
        expect(state,
               (cpu->io.input_capture.pmd_disabled & (uint16_t)(1u << channel)) != 0u,
               "capture PMD disable becomes effective after one cycle");
        expect(state,
               dspic33_read_word(cpu, base) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0u,
               "capture PMD disabled registers read zero");
        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
        expect(state, set_level(cpu, channel, true),
               "capture PMD disabled external edge advances");
        expect(state, cpu->io.input_capture.fifo[channel].count == 0u,
               "capture PMD disabled edge is missed");

        dspic33_write_word(cpu, pmd_address, 0u);
        expect(state, dspic33_read_word(cpu, base) == 0u,
               "capture PMD enable remains inaccessible for one cycle");
        expect(state, dspic33_device_advance(cpu, 1u),
               "capture PMD enable transition advances");
        expect(state,
               (cpu->io.input_capture.pmd_disabled & (uint16_t)(1u << channel)) == 0u &&
                   dspic33_read_word(cpu, base) == 0x3c03u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 2u)) == CAPTURE_TRIGGER,
               "capture PMD enable restores preserved registers");
        expect(state, set_level(cpu, channel, false) && set_level(cpu, channel, true),
               "capture PMD re-enabled edge advances");
        expect(state, cpu->io.input_capture.fifo[channel].count == 1u,
               "capture PMD re-enabled channel captures new edge");
    }
}

static void pmd_lifecycle_cases(InputCaptureConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    uint16_t base = capture_base(0u);
    uint16_t pmd_mask = capture_pmd_mask(0u);
    bool initialized;

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue capture beside pending PMD disable");
    expect(state, dspic33_device_advance(cpu, 1u),
           "capture PMD disable pauses pending snapshot");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 0u && cpu->events.count == 1u &&
               cpu->events.items[0].paused,
           "capture PMD preserves pending internal snapshot");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, 0u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "capture PMD enable resumes pending snapshot");
    expect(state, cpu->io.input_capture.fifo[0].count == 1u && cpu->events.count == 1u,
           "capture PMD resumed snapshot completes and leaves delayed IRQ");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    expect(state, set_level(cpu, 0u, true),
           "capture before PMD interrupt pause advances");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    expect(state, dspic33_device_advance(cpu, 1u),
           "capture PMD disable pauses delayed interrupt");
    expect(state,
           !interrupt_flag(cpu, 0u) && cpu->events.count == 1u &&
               cpu->events.items[0].paused &&
               cpu->events.items[0].paused_remaining == 1u,
           "capture PMD preserves delayed interrupt remaining time");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, 0u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "capture PMD enable resumes delayed interrupt");
    expect(state, !interrupt_flag(cpu, 0u),
           "capture PMD resumed interrupt retains remaining cycle");
    expect(state, dspic33_device_advance(cpu, 1u) && interrupt_flag(cpu, 0u),
           "capture PMD resumed interrupt fires after remaining cycle");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    expect(state, dspic33_device_advance(cpu, 5u),
           "capture timer advances before PMD disable");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    expect(state, dspic33_device_advance(cpu, 1u),
           "capture timer reaches PMD disable boundary");
    expect(state, cpu->io.input_capture.timer[0] == 6u,
           "capture timer includes delayed PMD cycle");
    expect(state,
           dspic33_device_advance(cpu, 10u) && cpu->io.input_capture.timer[0] == 6u,
           "capture timer stops while PMD disabled");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.input_capture.timer[0] == 6u,
           "capture timer remains stopped through PMD enable delay");
    expect(state,
           dspic33_device_advance(cpu, 4u) && cpu->io.input_capture.timer[0] == 10u,
           "capture timer resumes after PMD enable");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    expect(state, set_level(cpu, 0u, true), "seed FIFO before PMD disable");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    expect(state, dspic33_device_advance(cpu, 1u), "disable capture with seeded FIFO");
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u &&
               cpu->io.input_capture.fifo[0].count == 1u,
           "disabled capture FIFO read returns zero without popping");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, 0u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "enable capture with preserved FIFO");
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 1u &&
               cpu->io.input_capture.fifo[0].count == 0u,
           "capture PMD preserves FIFO contents until re-enabled read");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, true);
    configure_capture(cpu, 1u, 0u, true);
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, capture_pmd_mask(1u));
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue paired capture beside high-channel PMD disable");
    expect(state, dspic33_device_advance(cpu, 1u),
           "high-channel PMD disable pauses paired snapshot");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 0u &&
               cpu->io.input_capture.fifo[1].count == 0u && cpu->events.count == 1u &&
               cpu->events.items[0].paused,
           "either channel PMD disable pauses paired pipeline");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, 0u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "high-channel PMD enable resumes paired snapshot");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u &&
               cpu->io.input_capture.fifo[1].count == 1u,
           "paired pipeline completes after both channels are enabled");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, true);
    configure_capture(cpu, 1u, 0u, true);
    dspic33_write_word(cpu, CAPTURE_PMD_LOW,
                       (uint16_t)(capture_pmd_mask(0u) | capture_pmd_mask(1u)));
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u),
           "queue paired capture beside dual PMD disable");
    expect(state, dspic33_device_advance(cpu, 1u),
           "dual PMD disable pauses paired snapshot");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, capture_pmd_mask(0u));
    expect(state, dspic33_device_advance(cpu, 1u), "enable one paired PMD channel");
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               cpu->io.input_capture.fifo[0].count == 0u &&
               cpu->io.input_capture.fifo[1].count == 0u,
           "paired snapshot remains paused until both channels enable");
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "enable final paired PMD channel");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u &&
               cpu->io.input_capture.fifo[1].count == 1u,
           "paired snapshot completes after final channel enables");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize capture PMD copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    expect(state, dspic33_copy(&copy, cpu), "copy pending capture PMD transition");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance original and copied capture PMD transition");
    expect(state,
           cpu->io.input_capture.pmd_disabled == 1u &&
               copy.io.input_capture.pmd_disabled == 1u,
           "copy retains pending capture PMD state");
    dspic33_destroy(&copy);

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance reset capture PMD queue");
    expect(state,
           cpu->io.input_capture.pmd_disabled == 0u &&
               cpu->io.input_capture.pmd_generation[0] == 0u &&
               dspic33_read_word(cpu, CAPTURE_PMD_LOW) == 0u && cpu->events.count == 0u,
           "reset cancels capture PMD transition");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, 0u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance replaced capture PMD transition");
    expect(state,
           cpu->io.input_capture.pmd_disabled == 0u &&
               dspic33_read_word(cpu, CAPTURE_PMD_LOW) == 0u && cpu->events.count == 0u,
           "new capture PMD request invalidates stale transition");

    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    expect(state, dspic33_device_advance(cpu, 1u),
           "complete capture PMD disable before reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.input_capture.pmd_disabled == 0u &&
               cpu->io.input_capture.pmd_generation[0] == 0u &&
               dspic33_read_word(cpu, CAPTURE_PMD_LOW) == 0u,
           "reset clears effective capture PMD state");

    dspic33_reset(cpu, 0u);
    configure_capture(cpu, 0u, 0u, false);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, CAPTURE_PMD_LOW, pmd_mask);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               dspic33_read_word(cpu, CAPTURE_PMD_LOW) == 0u &&
               cpu->io.input_capture.pmd_disabled == 0u && cpu->events.count == 0u &&
               dspic33_read_word(cpu, base) == CAPTURE_FP_RISING,
           "capture PMD scheduling failure rolls back request");
}

static void timer_source_cases(InputCaptureConformance* state, Dspic33* cpu) {
    static const uint8_t no_sources[5] = {0u, 10u, 29u, 30u, 31u};
    uint8_t index;
    for (index = 0u; index < 5u; index++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 0u, capture_timer_sources[index], true, true, 0u,
                                 false);
        configure_timer_source(cpu, index, UINT16_MAX, 0u);
        expect(state, dspic33_device_advance(cpu, 7u),
               "advance alternate capture clock");
        expect(state,
               dspic33_read_word(cpu, timer_registers[index]) == 7u &&
                   cpu->io.input_capture.timer[0] == 7u,
               "Timer1-5 prescaled clocks advance selected capture timer");
    }

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, capture_timer_sources[0], true, true, 0u, false);
    configure_timer_source(cpu, 0u, UINT16_MAX, 0x0010u);
    expect(state, dspic33_device_advance(cpu, 15u),
           "advance prescaled alternate capture clock");
    expect(state,
           dspic33_read_word(cpu, timer_registers[0]) == 1u &&
               cpu->io.input_capture.timer[0] == 1u,
           "alternate capture clock follows source timer prescaler");

    for (index = 0u; index < sizeof(no_sources) / sizeof(no_sources[0]); index++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 0u, 0x1c00u, false, false, no_sources[index],
                                 false);
        expect(state, dspic33_device_advance(cpu, 3u),
               "advance normal capture timer without sync source");
        expect(state, cpu->io.input_capture.timer[0] == 3u,
               "all no-source encodings select normal timer operation");
    }

    for (index = 0u; index < 2u; index++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 0u, index == 0u ? 0x1400u : 0x1800u, true, true,
                                 0u, false);
        expect(state, dspic33_device_advance(cpu, 4u),
               "advance reserved capture clock selection");
        expect(state, cpu->io.input_capture.timer[0] == 0u,
               "reserved capture clock selections remain inactive");
    }

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, capture_timer_sources[0], true, true, 0u, true);
    configure_capture_source(cpu, 1u, capture_timer_sources[0], true, true, 0u, true);
    configure_timer_source(cpu, 0u, UINT16_MAX, 0u);
    expect(state, dspic33_device_advance(cpu, 65537u),
           "advance cascaded alternate capture clock");
    expect(state,
           cpu->io.input_capture.timer[0] == 1u && cpu->io.input_capture.timer[1] == 1u,
           "matching alternate clocks advance cascaded timer");

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, capture_timer_sources[0], true, true, 0u, true);
    configure_capture_source(cpu, 1u, capture_timer_sources[1], true, true, 0u, true);
    configure_timer_source(cpu, 0u, UINT16_MAX, 0u);
    configure_timer_source(cpu, 1u, UINT16_MAX, 0u);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance mismatched cascaded capture clocks");
    expect(state,
           cpu->io.input_capture.timer[0] == 0u && cpu->io.input_capture.timer[1] == 0u,
           "cascaded capture requires matching clock selections");
}

static void sync_trigger_cases(InputCaptureConformance* state, Dspic33* cpu) {
    uint8_t source;
    uint16_t target = capture_base(15u);
    for (source = 0u; source < 5u; source++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 15u, 0x1c00u, true, false,
                                 (uint8_t)(11u + source), false);
        configure_timer_source(cpu, source, 1u, 0u);
        expect(state, dspic33_device_advance(cpu, 1u), "advance timer trigger source");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "Timer1-5 period sources trigger capture timer");
    }

    for (source = 0u; source < 9u; source++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 15u, 0x1c00u, true, false, (uint8_t)(1u + source),
                                 false);
        configure_compare_source(cpu, source);
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance output compare trigger source");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "OC1-9 period sources trigger capture timer");
    }

    for (source = 0u; source < 8u; source++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, source, 0x1c00u, true, true, 0u, false);
        configure_capture_source(cpu, 15u, 0x1c00u, true, false,
                                 (uint8_t)(16u + source), false);
        cpu->io.input_capture.timer[source] = 0xfffeu;
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance input capture timer to sync output");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "IC1-8 timer sync outputs trigger capture timer");
    }

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, 0x1c00u, true, true, 0u, false);
    configure_capture_source(cpu, 15u, 0x1c00u, true, false, 16u, false);
    expect(state, rising_edge(cpu, 0u), "advance capture without sync output");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) == 0u,
           "capture event does not substitute for IC timer sync output");

    dspic33_write_word(cpu, capture_base(0u), 0u);
    expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
           "turning source module off asserts IC sync output trigger");

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, 0x1c00u, true, false, 0u, false);
    configure_capture_source(cpu, 15u, 0x1c00u, false, false, 16u, false);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance beside reset-held IC sync output");
    expect(state, cpu->io.input_capture.timer[15] == 0u,
           "source timer reset holds synchronized destination clear");
    dspic33_write_word(cpu, (uint16_t)(capture_base(0u) + 2u), 0x00c0u);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance after source timer leaves reset");
    expect(state, cpu->io.input_capture.timer[15] == 2u,
           "destination resumes after IC sync output negates");

    for (source = 0u; source < 3u; source++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 15u, 0x1c00u, true, false,
                                 (uint8_t)(24u + source), false);
        configure_comparator_source(cpu, source);
        expect(state,
               dspic33_comparator_input(cpu, source, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                        200u, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "advance comparator trigger source");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
               "CMP1-3 rising sources trigger capture timer");
    }

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 15u, 0x1c00u, true, false, 27u, false);
    dspic33_write_word(cpu, 0x0320u, 0u);
    dspic33_write_word(cpu, 0x0322u, 0u);
    dspic33_write_word(cpu, 0x0328u, 0u);
    dspic33_write_word(cpu, 0x0320u, 0x8010u);
    dspic33_write_word(cpu, 0x0320u, 0x8012u);
    expect(state,
           dspic33_adc_trigger(cpu, 0u, 1u, 0u) && dspic33_device_advance(cpu, 0u),
           "advance ADC1 trigger source");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(target + 2u)) & 0x0040u) != 0u,
           "accepted ADC1 conversion triggers capture timer");

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, 0x1c00u, false, false, 11u, false);
    expect(state, dspic33_device_advance(cpu, 5u), "seed synchronized timer");
    configure_timer_source(cpu, 0u, 1u, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance synchronized timer source");
    expect(state, cpu->io.input_capture.timer[0] == 6u,
           "synchronization pulse waits for next selected clock edge");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance synchronized timer reset edge");
    expect(state, cpu->io.input_capture.timer[0] == 0u,
           "synchronization pulse clears timer on next FP edge");

    dspic33_write_word(cpu, (uint16_t)(capture_base(0u) + 2u), 12u);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance after changing synchronous source");
    expect(state, cpu->io.input_capture.timer[0] == 2u,
           "synchronous source selection change preserves running timer");

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, 0x1c00u, false, false, 0u, false);
    configure_capture_source(cpu, 15u, 0x1c00u, false, false, 16u, false);
    expect(state, dspic33_device_advance(cpu, 2u), "seed IC synchronized timer");
    cpu->io.input_capture.timer[0] = 0xfffeu;
    expect(state, dspic33_device_advance(cpu, 1u),
           "assert IC timer synchronization output");
    expect(state, cpu->io.input_capture.timer[15] == 3u,
           "IC synchronization output assertion waits for next clock");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance IC synchronization reset edge");
    expect(state, cpu->io.input_capture.timer[15] == 0u,
           "IC synchronization output holds destination clear");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance after IC synchronization output negates");
    expect(state, cpu->io.input_capture.timer[15] == 1u,
           "IC synchronized timer resumes after source leaves FFFF");

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, 0x1c00u, false, false, 16u, false);
    expect(state, dspic33_device_advance(cpu, 3u) && rising_edge(cpu, 0u),
           "advance self-synchronized capture");
    expect(state,
           cpu->io.input_capture.timer[0] == 0u &&
               cpu->io.input_capture.fifo[0].count == 0u,
           "capture module rejects itself as sync source");

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, 0x1c00u, true, false, 16u, false);
    expect(state, rising_edge(cpu, 0u), "advance self-triggered capture");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(capture_base(0u) + 2u)) & 0x0040u) ==
                   0u &&
               cpu->io.input_capture.fifo[0].count == 0u,
           "capture module rejects itself as trigger source");

    for (source = 24u; source <= 28u; source++) {
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 15u, 0x1c00u, false, false, source, false);
        expect(state, dspic33_device_advance(cpu, 3u),
               "advance invalid synchronization selection");
        expect(state, cpu->io.input_capture.timer[15] == 0u,
               "comparator ADC and reserved sources cannot synchronize capture");
    }

    dspic33_reset(cpu, 0u);
    configure_capture_source(cpu, 0u, 0x1c00u, true, false, 11u, true);
    configure_capture_source(cpu, 1u, 0x1c00u, true, false, 11u, true);
    configure_timer_source(cpu, 0u, 1u, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance cascaded hardware trigger");
    expect(
        state,
        (dspic33_read_word(cpu, (uint16_t)(capture_base(0u) + 2u)) & 0x0040u) != 0u &&
            (dspic33_read_word(cpu, (uint16_t)(capture_base(1u) + 2u)) & 0x0040u) != 0u,
        "shared source releases both cascaded timers");

    dspic33_write_word(cpu, (uint16_t)(capture_base(0u) + 2u),
                       (uint16_t)(CAPTURE_32_BIT | 0x0080u | 11u));
    expect(state,
           cpu->io.input_capture.timer[0] == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base(0u) + 2u)) & 0x0040u) ==
                   0u,
           "software trigger clear resets selected cascaded half");
    dspic33_write_word(cpu, timer_registers[0], 0u);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance repeated cascaded hardware trigger");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(capture_base(0u) + 2u)) & 0x0040u) != 0u,
           "hardware source retriggers after software status clear");

    {
        Dspic33 stepped;
        uint16_t triggered_batch;
        uint16_t synchronized_batch;
        bool initialized = dspic33_initialize(&stepped);
        expect(state, initialized, "initialize batched source comparison");
        if (!initialized) {
            return;
        }
        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 15u, 0x1c00u, true, false, 11u, false);
        configure_timer_source(cpu, 0u, 3u, 0u);
        expect(state, dspic33_device_advance(cpu, 10u),
               "advance batched timer trigger source");
        triggered_batch = cpu->io.input_capture.timer[15];
        dspic33_reset(&stepped, 0u);
        configure_capture_source(&stepped, 15u, 0x1c00u, true, false, 11u, false);
        configure_timer_source(&stepped, 0u, 3u, 0u);
        for (source = 0u; source < 10u; source++) {
            dspic33_device_advance(&stepped, 1u);
        }
        expect(state,
               triggered_batch == stepped.io.input_capture.timer[15] &&
                   triggered_batch == 7u,
               "batched and stepped timer trigger timing agree");

        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 15u, 0x1c00u, false, false, 11u, false);
        configure_timer_source(cpu, 0u, 3u, 0u);
        expect(state, dspic33_device_advance(cpu, 10u),
               "advance batched timer synchronization source");
        synchronized_batch = cpu->io.input_capture.timer[15];
        dspic33_reset(&stepped, 0u);
        configure_capture_source(&stepped, 15u, 0x1c00u, false, false, 11u, false);
        configure_timer_source(&stepped, 0u, 3u, 0u);
        for (source = 0u; source < 10u; source++) {
            dspic33_device_advance(&stepped, 1u);
        }
        expect(state,
               synchronized_batch == stepped.io.input_capture.timer[15] &&
                   synchronized_batch == 2u,
               "batched and stepped timer synchronization timing agree");

        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 7u, 0x1c00u, false, true, 0u, false);
        configure_capture_source(cpu, 0u, 0x1c00u, false, true, 23u, false);
        configure_capture_source(cpu, 1u, 0x1c00u, false, true, 16u, false);
        cpu->io.input_capture.timer[7] = 0xfffeu;
        expect(state, dspic33_copy(&stepped, cpu),
               "copy reverse synchronization chain");
        expect(state, dspic33_device_advance(cpu, 3u),
               "advance batched reverse synchronization chain");
        expect(state,
               dspic33_device_advance(&stepped, 1u) &&
                   dspic33_device_advance(&stepped, 1u) &&
                   dspic33_device_advance(&stepped, 1u),
               "advance stepped reverse synchronization chain");
        expect(
            state,
            cpu->io.input_capture.timer[0] == 1u &&
                cpu->io.input_capture.timer[1] == 1u &&
                cpu->io.input_capture.timer[7] == 1u &&
                cpu->io.input_capture.timer[0] == stepped.io.input_capture.timer[0] &&
                cpu->io.input_capture.timer[1] == stepped.io.input_capture.timer[1] &&
                cpu->io.input_capture.timer[7] == stepped.io.input_capture.timer[7] &&
                cpu->io.input_capture.sync_output_high ==
                    stepped.io.input_capture.sync_output_high &&
                (cpu->io.input_capture.sync_output_high & 0x0083u) == 0u,
            "reverse synchronization chain batch and step agree");

        dspic33_reset(cpu, 0u);
        configure_capture_source(cpu, 15u, 0x1c00u, false, false, 1u, false);
        configure_compare_source(cpu, 0u);
        expect(state, dspic33_device_advance(cpu, 1u),
               "queue synchronization reset before copy");
        expect(state, dspic33_copy(&stepped, cpu),
               "copy pending synchronization reset");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_device_advance(&stepped, 1u) &&
                   cpu->io.input_capture.timer[15] == 0u &&
                   stepped.io.input_capture.timer[15] == 0u,
               "copy preserves pending synchronization reset");
        dspic33_reset(cpu, 0u);
        expect(state,
               cpu->io.input_capture.sync_reset_pending == 0u &&
                   cpu->io.input_capture.sync_output_high == 0x00ffu,
               "reset clears pending sync and restores off outputs high");
        dspic33_destroy(&stepped);
    }
}

int main(void) {
    Dspic33 cpu;
    InputCaptureConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize input capture processor");
    if (initialized) {
        access_cases(&state, &cpu);
        fifo_cases(&state, &cpu);
        interrupt_rate_cases(&state, &cpu);
        zero_interval_overflow_cases(&state, &cpu);
        mode_cases(&state, &cpu);
        power_cases(&state, &cpu);
        paired_cases(&state, &cpu);
        dma_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        pmd_channel_cases(&state, &cpu);
        pmd_lifecycle_cases(&state, &cpu);
        timer_source_cases(&state, &cpu);
        sync_trigger_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[input-capture-summary] cases=%u passed=%u failed=%u\n", state.cases,
           state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
