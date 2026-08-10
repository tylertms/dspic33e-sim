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
    CAPTURE_VECTOR = 0x0200u
};

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
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "hardware sync source is excluded");
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
        dspic33_destroy(&cpu);
    }
    printf("[input-capture-summary] cases=%u passed=%u failed=%u\n", state.cases,
           state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
