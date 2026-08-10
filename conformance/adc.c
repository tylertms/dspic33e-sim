#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} AdcConformance;

static const uint16_t buffers[DSPIC33_ADC_COUNT] = {0x0300u, 0x0340u};
static const uint16_t controls[DSPIC33_ADC_COUNT] = {0x0320u, 0x0360u};
static const uint16_t dma_controls[DSPIC33_ADC_COUNT] = {0x0332u, 0x0372u};
static const uint8_t irqs[DSPIC33_ADC_COUNT] = {13u, 21u};

static void expect(AdcConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[adc-failed] %s\n", name);
    }
}

static bool interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    dspic33_write_word(
        cpu, address,
        (uint16_t)(dspic33_read_word(cpu, address) & ~(uint16_t)(1u << (irq % 16u))));
}

static void set_input(Dspic33* cpu, uint8_t channel, uint16_t value) {
    dspic33_adc_input(cpu, channel, value);
}

static void start_manual(Dspic33* cpu, uint8_t module) {
    uint16_t control = dspic33_read_word(cpu, controls[module]);
    dspic33_write_word(cpu, controls[module], (uint16_t)(control | 0x0002u));
    dspic33_write_word(cpu, controls[module],
                       (uint16_t)(dspic33_read_word(cpu, controls[module]) & ~0x0002u));
}

static void finish_conversion(Dspic33* cpu, uint8_t module) {
    uint16_t timing = dspic33_read_word(cpu, (uint16_t)(controls[module] + 4u));
    uint64_t clock = (timing & 0x8000u) != 0u ? 1u : (timing & 0x00ffu) + 1u;
    uint64_t cycles =
        (module == 0u && (dspic33_read_word(cpu, controls[module]) & 0x0400u) != 0u)
            ? 14u * clock
            : 12u * clock;
    dspic33_device_advance(cpu, cycles);
}

static void configure_manual(Dspic33* cpu, uint8_t module, uint16_t control,
                             uint16_t control2, uint16_t channels) {
    dspic33_write_word(cpu, controls[module], 0u);
    dspic33_write_word(cpu, (uint16_t)(controls[module] + 2u), control2);
    dspic33_write_word(cpu, (uint16_t)(controls[module] + 8u), channels);
    dspic33_write_word(cpu, controls[module], (uint16_t)(control | 0x8000u));
}

static uint16_t expected_format(uint16_t code, uint8_t bits, uint8_t format) {
    uint8_t shift = (uint8_t)(16u - bits);
    uint16_t sign = (uint16_t)(1u << (bits - 1u));
    uint16_t mask = (uint16_t)((1u << bits) - 1u);
    if (format == 0u) {
        return code;
    }
    if (format == 2u) {
        return (uint16_t)(code << shift);
    }
    code ^= sign;
    if ((code & sign) != 0u) {
        code |= (uint16_t)~mask;
    }
    return format == 1u ? code : (uint16_t)(code << shift);
}

static void register_cases(AdcConformance* state, Dspic33* cpu) {
    uint8_t module;
    uint8_t index;
    dspic33_reset(cpu, 0u);
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        expect(state, dspic33_read_word(cpu, controls[module]) == 0u,
               "control one reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(controls[module] + 2u)) == 0u,
               "control two reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(controls[module] + 4u)) == 0u,
               "control three reset");
        dspic33_write_word(cpu, (uint16_t)(controls[module] + 2u), 0xffffu);
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(controls[module] + 2u)) ==
                   (module == 0u ? 0xe77fu : 0xe73fu),
               "control two mask");
        dspic33_write_word(cpu, (uint16_t)(controls[module] + 4u), 0xffffu);
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(controls[module] + 4u)) == 0x9fffu,
               "control three mask");
        dspic33_write_word(cpu, (uint16_t)(controls[module] + 6u), 0xffffu);
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(controls[module] + 6u)) == 0x0707u,
               "channel one through three mask");
        dspic33_write_word(cpu, (uint16_t)(controls[module] + 8u), 0xffffu);
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(controls[module] + 8u)) == 0x9f9fu,
               "channel zero mask");
        for (index = 0u; index < 16u; index++) {
            dspic33_write_word(cpu, (uint16_t)(buffers[module] + index * 2u), 0xa55au);
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(buffers[module] + index * 2u)) ==
                       0u,
                   "result buffer read only");
        }
    }
    dspic33_write_word(cpu, 0x032eu, 0xffffu);
    dspic33_write_word(cpu, 0x0330u, 0xffffu);
    dspic33_write_word(cpu, 0x0332u, 0xffffu);
    dspic33_write_word(cpu, 0x0370u, 0xffffu);
    dspic33_write_word(cpu, 0x0372u, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x032eu) == 0xffffu, "adc one high scan mask");
    expect(state, dspic33_read_word(cpu, 0x0330u) == 0xffffu, "adc one low scan mask");
    expect(state, dspic33_read_word(cpu, 0x0332u) == 0x0107u, "adc one dma mask");
    expect(state, dspic33_read_word(cpu, 0x0370u) == 0xffffu, "adc two scan mask");
    expect(state, dspic33_read_word(cpu, 0x0372u) == 0x0107u, "adc two dma mask");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0360u, 0x4c01u);
    expect(state, dspic33_read_word(cpu, 0x0360u) == 0u,
           "adc two unimplemented resolution and done");
    dspic33_write_word(cpu, 0x0322u, 0x0300u);
    dspic33_write_word(cpu, 0x0326u, 0x0707u);
    dspic33_write_word(cpu, 0x0320u, 0x0408u);
    expect(state, dspic33_read_word(cpu, 0x0320u) == 0x0400u,
           "twelve bit simultaneous unavailable");
    expect(state, (dspic33_read_word(cpu, 0x0322u) & 0x0300u) == 0u,
           "twelve bit channel selection unavailable");
    expect(state, dspic33_read_word(cpu, 0x0326u) == 0u,
           "twelve bit extra channels unavailable");
    dspic33_write_word(cpu, 0x0320u, 0x8400u);
    dspic33_write_word(cpu, 0x0320u, 0x8000u);
    expect(state, (dspic33_read_word(cpu, 0x0320u) & 0x0400u) != 0u,
           "resolution locked while enabled");
}

static void format_cases(AdcConformance* state, Dspic33* cpu) {
    static const uint16_t inputs[] = {0u, 4u, 0x07fcu, 0x0800u, 0x0ffcu};
    uint8_t module;
    uint8_t resolution;
    uint8_t format;
    uint8_t index;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        for (resolution = 0u; resolution < (module == 0u ? 2u : 1u); resolution++) {
            uint8_t bits = resolution != 0u ? 12u : 10u;
            for (format = 0u; format < 4u; format++) {
                for (index = 0u; index < sizeof(inputs) / sizeof(inputs[0]); index++) {
                    uint16_t code = resolution != 0u ? inputs[index]
                                                     : (uint16_t)(inputs[index] >> 2u);
                    dspic33_reset(cpu, 0u);
                    set_input(cpu, 0u, inputs[index]);
                    configure_manual(cpu, module,
                                     (uint16_t)((resolution != 0u ? 0x0400u : 0u) |
                                                ((uint16_t)format << 8u)),
                                     0u, 0u);
                    start_manual(cpu, module);
                    finish_conversion(cpu, module);
                    expect(state,
                           dspic33_read_word(cpu, buffers[module]) ==
                               expected_format(code, bits, format),
                           "conversion output format");
                    expect(state, (dspic33_read_word(cpu, controls[module]) & 1u) != 0u,
                           "conversion done flag");
                    dspic33_write_word(
                        cpu, controls[module],
                        (uint16_t)(dspic33_read_word(cpu, controls[module]) & ~1u));
                    expect(state, (dspic33_read_word(cpu, controls[module]) & 1u) == 0u,
                           "done flag software clear");
                }
            }
        }
    }
}

static void done_access_cases(AdcConformance* state, Dspic33* cpu) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, controls[module], 0x0001u);
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) == 0u,
               "done write one cannot set");
        set_input(cpu, 0u, 400u);
        configure_manual(cpu, module, 0u, 0u, 0u);
        start_manual(cpu, module);
        finish_conversion(cpu, module);
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) != 0u,
               "done hardware set");
        dspic33_write_word(
            cpu, controls[module],
            (uint16_t)(dspic33_read_word(cpu, controls[module]) | 0x0001u));
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) != 0u,
               "done write one preserves set");
        dspic33_write_word(
            cpu, controls[module],
            (uint16_t)(dspic33_read_word(cpu, controls[module]) & ~0x0001u));
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) == 0u,
               "done software clear");
    }
}

static void done_active_conversion_cases(AdcConformance* state, Dspic33* cpu) {
    uint8_t module;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        dspic33_reset(cpu, 0u);
        set_input(cpu, 0u, 400u);
        configure_manual(cpu, module, 0u, 0u, 0u);
        start_manual(cpu, module);
        finish_conversion(cpu, module);
        expect(state, dspic33_read_word(cpu, buffers[module]) == 100u,
               "done active initial result");
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) != 0u,
               "done active initial completion");
        set_input(cpu, 0u, 800u);
        start_manual(cpu, module);
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) == 0u,
               "conversion start clears done");
        dspic33_write_word(
            cpu, controls[module],
            (uint16_t)(dspic33_read_word(cpu, controls[module]) & ~0x0001u));
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) == 0u,
               "active conversion done clear");
        expect(state, dspic33_device_advance(cpu, 11u),
               "active conversion precompletion advance");
        expect(state, dspic33_read_word(cpu, buffers[module]) == 100u,
               "active conversion preserves prior result");
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) == 0u,
               "active conversion remains incomplete");
        expect(state, dspic33_device_advance(cpu, 1u),
               "active conversion completion advance");
        expect(state, dspic33_read_word(cpu, buffers[module]) == 200u,
               "active conversion completes after clear");
        expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) != 0u,
               "active conversion restores done");
    }
}

static void channel_cases(AdcConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < 32u; channel++) {
        dspic33_reset(cpu, 0u);
        set_input(cpu, channel, (uint16_t)(0x40u + channel));
        configure_manual(cpu, 0u, 0x0400u, 0u, channel);
        start_manual(cpu, 0u);
        finish_conversion(cpu, 0u);
        expect(state, dspic33_read_word(cpu, 0x0300u) == (uint16_t)(0x40u + channel),
               "adc one channel selection");
    }
    for (channel = 0u; channel < 16u; channel++) {
        dspic33_reset(cpu, 0u);
        set_input(cpu, channel, (uint16_t)((channel + 1u) * 4u));
        configure_manual(cpu, 1u, 0u, 0u, channel);
        start_manual(cpu, 1u);
        finish_conversion(cpu, 1u);
        expect(state, dspic33_read_word(cpu, 0x0340u) == (uint16_t)(channel + 1u),
               "adc two channel selection");
    }

    dspic33_reset(cpu, 0u);
    set_input(cpu, 1u, 1000u);
    set_input(cpu, 2u, 3000u);
    configure_manual(cpu, 0u, 0x0400u, 0u, 0x0082u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 2000u,
           "differential channel subtraction");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    set_input(cpu, 1u, 800u);
    set_input(cpu, 2u, 1200u);
    set_input(cpu, 5u, 2000u);
    configure_manual(cpu, 0u, 0u, 0x020cu, 5u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 500u, "four channel lane zero");
    expect(state, dspic33_read_word(cpu, 0x0302u) == 100u, "four channel lane one");
    expect(state, dspic33_read_word(cpu, 0x0304u) == 200u, "four channel lane two");
    expect(state, dspic33_read_word(cpu, 0x0306u) == 300u, "four channel lane three");
}

static void sequence_cases(AdcConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    set_input(cpu, 2u, 400u);
    set_input(cpu, 3u, 800u);
    configure_manual(cpu, 0u, 0u, 0x0005u, 0x0302u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 100u, "alternate sample a");
    expect(state, dspic33_read_word(cpu, 0x0302u) == 200u, "alternate sample b");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    set_input(cpu, 6u, 800u);
    set_input(cpu, 8u, 1200u);
    dspic33_write_word(cpu, 0x0330u, 0x0141u);
    configure_manual(cpu, 0u, 0u, 0x0408u, 0u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 100u, "scan first channel");
    expect(state, dspic33_read_word(cpu, 0x0302u) == 200u, "scan second channel");
    expect(state, dspic33_read_word(cpu, 0x0304u) == 300u, "scan third channel");
    expect(state, interrupt_flag(cpu, 13u), "scan boundary interrupt");
    clear_interrupt(cpu, 13u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 100u, "scan boundary restart");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    configure_manual(cpu, 0u, 0u, 0x0006u, 0u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, !interrupt_flag(cpu, 13u), "split buffer no early interrupt");
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, interrupt_flag(cpu, 13u), "split buffer interrupt");
    expect(state, (dspic33_read_word(cpu, 0x0322u) & 0x0080u) != 0u,
           "split buffer second half status");
    clear_interrupt(cpu, 13u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0310u) == 100u,
           "split buffer second half result");
}

static void threshold_cases(AdcConformance* state, Dspic33* cpu) {
    uint8_t module;
    uint8_t threshold;
    uint8_t sample;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        for (threshold = 1u; threshold <= 16u; threshold++) {
            dspic33_reset(cpu, 0u);
            set_input(cpu, 0u, 400u);
            configure_manual(cpu, module, 0u, (uint16_t)((threshold - 1u) << 2u), 0u);
            for (sample = 1u; sample <= threshold; sample++) {
                start_manual(cpu, module);
                finish_conversion(cpu, module);
                expect(state,
                       interrupt_flag(cpu, irqs[module]) == (sample == threshold),
                       "sample increment threshold");
            }
        }
    }
}

static void dma_interrupt_rate_cases(AdcConformance* state, Dspic33* cpu) {
    uint8_t module;
    uint8_t sample;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        dspic33_reset(cpu, 0u);
        set_input(cpu, 0u, 400u);
        configure_manual(cpu, module, 0u, 0x000cu, 0u);
        for (sample = 1u; sample <= 4u; sample++) {
            start_manual(cpu, module);
            expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) == 0u,
                   "non-dma conversion start clears done");
            finish_conversion(cpu, module);
            expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) != 0u,
                   "non-dma conversion sets done");
            expect(state, interrupt_flag(cpu, irqs[module]) == (sample == 4u),
                   "non-dma sample interrupt rate");
        }

        dspic33_reset(cpu, 0u);
        set_input(cpu, 0u, 400u);
        dspic33_write_word(cpu, dma_controls[module], 0x0100u);
        configure_manual(cpu, module, 0u, 0x000cu, 0u);
        for (sample = 1u; sample <= 4u; sample++) {
            start_manual(cpu, module);
            expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) == 0u,
                   "dma conversion start clears done");
            finish_conversion(cpu, module);
            expect(state, (dspic33_read_word(cpu, controls[module]) & 0x0001u) != 0u,
                   "dma conversion sets done");
            expect(state, interrupt_flag(cpu, irqs[module]),
                   "dma conversion interrupt rate");
            clear_interrupt(cpu, irqs[module]);
            expect(state, !interrupt_flag(cpu, irqs[module]),
                   "dma conversion interrupt clear");
        }
    }
}

static void trigger_cases(AdcConformance* state, Dspic33* cpu) {
    static const uint8_t sources[] = {1u, 2u,  3u,  4u,  5u,  8u,
                                      9u, 10u, 11u, 12u, 13u, 14u};
    uint8_t module;
    uint8_t index;
    for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
        for (index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
            uint8_t source = sources[index];
            dspic33_reset(cpu, 0u);
            set_input(cpu, 0u, 400u);
            configure_manual(cpu, module, (uint16_t)(source << 4u), 0u, 0u);
            dspic33_write_word(
                cpu, controls[module],
                (uint16_t)(dspic33_read_word(cpu, controls[module]) | 0x0002u));
            expect(state, dspic33_adc_trigger(cpu, module, source == 1u ? 2u : 1u, 0u),
                   "schedule wrong adc trigger");
            dspic33_device_advance(cpu, 0u);
            finish_conversion(cpu, module);
            expect(state, (dspic33_read_word(cpu, controls[module]) & 1u) == 0u,
                   "wrong adc trigger ignored");
            expect(state, dspic33_adc_trigger(cpu, module, source, 0u),
                   "schedule adc trigger");
            dspic33_device_advance(cpu, 0u);
            finish_conversion(cpu, module);
            expect(state, dspic33_read_word(cpu, buffers[module]) == 100u,
                   "external adc trigger result");
        }
    }
    expect(state, !dspic33_adc_trigger(cpu, 2u, 1u, 0u), "reject adc module");
    expect(state, !dspic33_adc_trigger(cpu, 0u, 0u, 0u), "reject manual trigger");
    expect(state, !dspic33_adc_trigger(cpu, 0u, 6u, 0u), "reject reserved trigger");
    expect(state, !dspic33_adc_trigger(cpu, 0u, 7u, 0u), "reject auto trigger");
    expect(state, !dspic33_adc_trigger(cpu, 0u, 15u, 0u), "reject trigger group");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    dspic33_write_word(cpu, 0x0324u, 0x0100u);
    dspic33_write_word(cpu, 0x0320u, 0x8074u);
    expect(state, dspic33_device_advance(cpu, 1u), "auto sample advance");
    expect(state, (dspic33_read_word(cpu, 0x0320u) & 0x0002u) == 0u,
           "auto sample starts conversion");
    expect(state, dspic33_device_advance(cpu, 12u), "auto conversion advance");
    expect(state, dspic33_read_word(cpu, 0x0300u) == 100u, "auto conversion result");
    expect(state, (dspic33_read_word(cpu, 0x0320u) & 0x0003u) == 0x0003u,
           "auto conversion restarts sampling");
    expect(state, dspic33_device_advance(cpu, 1u), "next auto sample advance");
    expect(state, (dspic33_read_word(cpu, 0x0320u) & 0x0003u) == 0u,
           "next auto conversion clears state");
}

static void dma_cases(AdcConformance* state, Dspic33* cpu) {
    static const uint8_t scan_channels[] = {0u,  6u,  8u,  9u,  10u,
                                            11u, 12u, 13u, 14u, 15u};
    uint8_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < sizeof(scan_channels); index++) {
        set_input(cpu, scan_channels[index], (uint16_t)((index + 1u) * 16u));
    }
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, 13u);
    dspic33_write_word(cpu, 0x0b04u, 0x2000u);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, 0x0300u);
    dspic33_write_word(cpu, 0x0b0eu, 9u);
    dspic33_write_word(cpu, 0x0b00u, 0x8002u);
    dspic33_write_word(cpu, 0x0330u, 0xff41u);
    dspic33_write_word(cpu, 0x0332u, 0x0101u);
    configure_manual(cpu, 0u, 0x1400u, 0x0424u, 0u);
    for (index = 0u; index < sizeof(scan_channels); index++) {
        start_manual(cpu, 0u);
        finish_conversion(cpu, 0u);
    }
    for (index = 0u; index < sizeof(scan_channels); index++) {
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(0x2000u + index * 2u)) ==
                   (uint16_t)((index + 1u) * 16u),
               "ordered adc dma result");
    }
    expect(state, !interrupt_flag(cpu, 4u) && cpu->io.dma_index[0] == 9u,
           "ordered adc dma remains active before completion");
    expect(state, dspic33_device_advance(cpu, 1u),
           "ordered adc dma completion advance");
    expect(state, interrupt_flag(cpu, 4u), "ordered adc dma block interrupt");
    expect(state, interrupt_flag(cpu, 13u), "ordered adc increment interrupt");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 1u, 400u);
    set_input(cpu, 3u, 800u);
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, 13u);
    dspic33_write_word(cpu, 0x0b04u, 0x3000u);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, 0x0300u);
    dspic33_write_word(cpu, 0x0b0eu, 3u);
    dspic33_write_word(cpu, 0x0b00u, 0x8020u);
    dspic33_write_word(cpu, 0x0330u, 0x000au);
    dspic33_write_word(cpu, 0x0332u, 0x0101u);
    configure_manual(cpu, 0u, 0u, 0x040cu, 0u);
    for (index = 0u; index < 4u; index++) {
        start_manual(cpu, 0u);
        finish_conversion(cpu, 0u);
    }
    expect(state, dspic33_read_word(cpu, 0x3004u) == 100u,
           "scatter adc first channel first sample");
    expect(state, dspic33_read_word(cpu, 0x3006u) == 100u,
           "scatter adc first channel second sample");
    expect(state, dspic33_read_word(cpu, 0x300cu) == 200u,
           "scatter adc second channel first sample");
    expect(state, dspic33_read_word(cpu, 0x300eu) == 200u,
           "scatter adc second channel second sample");
}

static void power_cases(AdcConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    configure_manual(cpu, 0u, 0x2000u, 0u, 0u);
    start_manual(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    finish_conversion(cpu, 0u);
    expect(state, (dspic33_read_word(cpu, 0x0320u) & 1u) == 0u, "adc stops in idle");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    configure_manual(cpu, 0u, 0u, 0u, 0u);
    start_manual(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 100u, "adc continues in idle");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    dspic33_write_word(cpu, 0x0324u, 0x8000u);
    configure_manual(cpu, 0u, 0u, 0u, 0u);
    start_manual(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 100u,
           "internal adc clock in sleep");
}

static void boundary_cases(AdcConformance* state, Dspic33* cpu) {
    uint8_t sample;
    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 0xffffu);
    configure_manual(cpu, 0u, 0x0400u, 0u, 0u);
    start_manual(cpu, 0u);
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 0x0fffu, "adc input range mask");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    configure_manual(cpu, 0u, 0x0010u, 0u, 0u);
    dspic33_write_word(cpu, 0x0320u, 0x8012u);
    expect(state, dspic33_adc_trigger(cpu, 0u, 1u, 5u), "delayed adc trigger");
    expect(state, dspic33_device_advance(cpu, 4u), "advance before adc trigger");
    expect(state, (dspic33_read_word(cpu, 0x0320u) & 0x0002u) != 0u,
           "adc samples before delayed trigger");
    expect(state, dspic33_device_advance(cpu, 1u), "advance to adc trigger");
    expect(state, (dspic33_read_word(cpu, 0x0320u) & 0x0002u) == 0u,
           "delayed trigger starts conversion");
    finish_conversion(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0300u) == 100u,
           "delayed trigger conversion result");

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    configure_manual(cpu, 0u, 0u, 0x007cu, 0u);
    dspic33_write_word(cpu, 0x0332u, 0x0100u);
    for (sample = 1u; sample <= 32u; sample++) {
        start_manual(cpu, 0u);
        finish_conversion(cpu, 0u);
        expect(state,
               interrupt_flag(cpu, 13u) &&
                   cpu->io.adc_sample_count[0] == (uint8_t)(sample & 0x1fu),
               "adc one dma interrupt and increment threshold");
        clear_interrupt(cpu, 13u);
    }

    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    dspic33_write_word(cpu, 0x0324u, 0x0100u);
    dspic33_write_word(cpu, 0x0320u, 0x8074u);
    dspic33_write_word(cpu, 0x0320u, 0u);
    expect(state, dspic33_device_advance(cpu, 20u), "advance cancelled conversion");
    expect(state, dspic33_read_word(cpu, 0x0300u) == 0u,
           "disabled adc cancels conversion");
}

static void copy_cases(AdcConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize adc copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    set_input(cpu, 0u, 400u);
    dspic33_write_word(cpu, 0x0324u, 0x0100u);
    dspic33_write_word(cpu, 0x0320u, 0x8074u);
    expect(state, dspic33_copy(&copy, cpu), "copy adc state");
    expect(state, copy.io.adc_generation[0] == cpu->io.adc_generation[0],
           "copy adc generation");
    expect(state, copy.io.adc[0] == cpu->io.adc[0], "copy adc input");
    expect(state, copy.events.count == cpu->events.count, "copy adc events");
    dspic33_device_advance(cpu, 1u);
    dspic33_device_advance(&copy, 1u);
    dspic33_device_advance(cpu, 12u);
    dspic33_device_advance(&copy, 12u);
    expect(state, dspic33_read_word(&copy, 0x0300u) == dspic33_read_word(cpu, 0x0300u),
           "copied adc conversion result");
    expect(state, dspic33_read_word(&copy, 0x0320u) == dspic33_read_word(cpu, 0x0320u),
           "copied adc control state");
    dspic33_destroy(&copy);
}

int main(void) {
    Dspic33 cpu;
    AdcConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize adc processor");
    if (!initialized) {
        fprintf(stderr, "cannot initialize ADC conformance simulator\n");
        return 1;
    }
    register_cases(&state, &cpu);
    format_cases(&state, &cpu);
    done_access_cases(&state, &cpu);
    done_active_conversion_cases(&state, &cpu);
    channel_cases(&state, &cpu);
    sequence_cases(&state, &cpu);
    threshold_cases(&state, &cpu);
    dma_interrupt_rate_cases(&state, &cpu);
    trigger_cases(&state, &cpu);
    dma_cases(&state, &cpu);
    power_cases(&state, &cpu);
    boundary_cases(&state, &cpu);
    copy_cases(&state, &cpu);
    printf("[adc-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&cpu);
    return state.failed == 0u ? 0 : 1;
}
