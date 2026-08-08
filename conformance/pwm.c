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
} PwmConformance;

static const uint8_t irqs[DSPIC33_PWM_COUNT] = {94u, 95u, 96u, 97u, 98u, 99u};

static uint16_t base(uint8_t generator) {
    return (uint16_t)(0x0c20u + generator * 0x20u);
}

static void expect(PwmConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[pwm-failed] %s\n", name);
    }
}

static bool interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void configure_generator(Dspic33* cpu, uint8_t generator, uint16_t mode,
                                uint16_t period, uint16_t duty, uint16_t control) {
    uint16_t address = base(generator);
    dspic33_write_word(cpu, 0x0c00u, 0u);
    dspic33_write_word(cpu, 0x0c02u, 1u);
    dspic33_write_word(cpu, 0x0c04u, period);
    dspic33_write_word(cpu, (uint16_t)(address + 6u), duty);
    dspic33_write_word(cpu, (uint16_t)(address + 0x0eu), duty);
    dspic33_write_word(cpu, (uint16_t)(address + 2u), (uint16_t)(0xc000u | mode));
    dspic33_write_word(cpu, address, control);
}

static void enable_pwm(Dspic33* cpu, uint16_t control) {
    dspic33_write_word(cpu, 0x0c00u, (uint16_t)(control | 0x8000u));
}

static void register_cases(PwmConformance* state, Dspic33* cpu) {
    static const uint16_t offsets[] = {0x00u, 0x02u, 0x04u, 0x06u, 0x08u, 0x0au,
                                       0x0cu, 0x0eu, 0x10u, 0x12u, 0x14u, 0x16u,
                                       0x18u, 0x1au, 0x1cu, 0x1eu};
    static const uint16_t masks[] = {
        0x1fefu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0x3fffu, 0x3fffu, 0xffffu,
        0xffffu, 0xffffu, 0xf03fu, 0x0000u, 0x0000u, 0xfc3fu, 0x0fffu, 0x0f3fu};
    uint8_t generator;
    uint8_t index;
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, 0x0c04u) == 0xffffu, "primary period reset");
    expect(state, dspic33_read_word(cpu, 0x0c12u) == 0xffffu, "secondary period reset");
    dspic33_write_word(cpu, 0x0c06u, 1u);
    dspic33_write_word(cpu, 0x0c14u, 1u);
    dspic33_write_word(cpu, 0x0c00u, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x0c00u) == 0xafffu, "primary control mask");
    dspic33_write_word(cpu, 0x0c00u, 0u);
    dspic33_write_word(cpu, 0x0c02u, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x0c02u) == 0x0007u, "primary divider mask");
    dspic33_write_word(cpu, 0x0c0eu, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x0c0eu) == 0x0fffu, "secondary control mask");
    dspic33_write_word(cpu, 0x0c10u, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x0c10u) == 0x0007u, "secondary divider mask");
    dspic33_write_word(cpu, 0x0c1au, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x0c1au) == 0x83ffu, "chop mask");
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); index++) {
            uint16_t address = (uint16_t)(base(generator) + offsets[index]);
            dspic33_write_word(cpu, address, 0xffffu);
            expect(state, dspic33_read_word(cpu, address) == masks[index],
                   "generator register mask");
            dspic33_write_word(cpu, address, 0u);
        }
    }
}

static void clock_cases(PwmConformance* state, Dspic33* cpu) {
    uint8_t divider;
    for (divider = 0u; divider < 7u; divider++) {
        uint16_t expected_ticks = divider == 0u ? 2u : divider == 1u ? 1u : 0u;
        dspic33_reset(cpu, 0u);
        configure_generator(cpu, 0u, 0u, 100u, 0u, 0x0080u);
        dspic33_write_word(cpu, 0x0c02u, divider);
        enable_pwm(cpu, 0u);
        dspic33_device_advance(cpu, 1u);
        expect(state, cpu->io.pwm_master_counter[0] == expected_ticks,
               "primary divider first cycle");
        if (divider >= 2u) {
            uint16_t cycles = (uint16_t)(1u << (divider - 1u));
            dspic33_device_advance(cpu, (uint16_t)(cycles - 1u));
            expect(state, cpu->io.pwm_master_counter[0] == 1u,
                   "primary divider accumulated cycle");
        }
    }
    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 9u, 5u, 0x0080u);
    enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c02u, 6u);
    expect(state, dspic33_read_word(cpu, 0x0c02u) == 1u,
           "divider locked while enabled");

    for (divider = 0u; divider < 7u; divider++) {
        uint16_t expected_ticks = divider == 0u ? 2u : divider == 1u ? 1u : 0u;
        dspic33_reset(cpu, 0u);
        configure_generator(cpu, 0u, 0u, 100u, 0u, 0x0088u);
        dspic33_write_word(cpu, 0x0c12u, 100u);
        dspic33_write_word(cpu, 0x0c10u, divider);
        enable_pwm(cpu, 0u);
        dspic33_device_advance(cpu, 1u);
        expect(state, cpu->io.pwm_master_counter[1] == expected_ticks,
               "secondary divider first cycle");
        if (divider >= 2u) {
            uint16_t cycles = (uint16_t)(1u << (divider - 1u));
            dspic33_device_advance(cpu, (uint16_t)(cycles - 1u));
            expect(state, cpu->io.pwm_master_counter[1] == 1u,
                   "secondary divider accumulated cycle");
        }
    }
}

static void mode_cases(PwmConformance* state, Dspic33* cpu) {
    static const uint16_t modes[] = {0x0000u, 0x0400u, 0x0800u, 0x0c00u};
    uint8_t mode;
    for (mode = 0u; mode < sizeof(modes) / sizeof(modes[0]); mode++) {
        dspic33_reset(cpu, 0u);
        configure_generator(cpu, 0u, modes[mode], 7u, 3u, 0x0080u);
        if (mode == 3u) {
            dspic33_write_word(cpu, 0x0c2eu, 1u);
        }
        enable_pwm(cpu, 0u);
        expect(state, dspic33_pwm_output(cpu, 0u, true), "mode initial high output");
        expect(state, dspic33_pwm_output(cpu, 0u, false) == (mode == 1u || mode == 3u),
               "mode initial low output");
        dspic33_device_advance(cpu, 4u);
        expect(state, !dspic33_pwm_output(cpu, 0u, true), "mode inactive high output");
        expect(state, dspic33_pwm_output(cpu, 0u, false) == (mode == 0u),
               "mode inactive low output");
    }

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c22u, 0xf3c0u);
    enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "override polarity high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "override polarity low");
    dspic33_write_word(cpu, 0x0c22u, 0xc3c0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "override high data");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "override low data");
    dspic33_write_word(cpu, 0x0c22u, 0xc382u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "swap high output");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "swap low output");
    dspic33_write_word(cpu, 0x0c22u, 0x0200u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "unowned high pin");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "unowned low pin");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0800u, 3u, 3u, 0x0080u);
    enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "push-pull first high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "push-pull first low");
    dspic33_device_advance(cpu, 4u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "push-pull second high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "push-pull second low");
}

static void dead_time_cases(PwmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 15u, 7u, 0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 3u);
    enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "positive dead-time start");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "positive dead-time low start");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "positive dead-time high");
    dspic33_device_advance(cpu, 6u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "positive dead-time gap high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "positive dead-time gap low");
    dspic33_device_advance(cpu, 3u);
    expect(state, dspic33_pwm_output(cpu, 0u, false), "positive dead-time low");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 15u, 7u, 0x0040u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 3u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 6u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "negative dead-time high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "negative dead-time overlap");
    dspic33_device_advance(cpu, 5u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "negative dead-time high end");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "negative dead-time low");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 4u);
    enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "redundant dead-time start");
    expect(state, !dspic33_pwm_output(cpu, 0u, false),
           "redundant alternate dead-time start");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "redundant dead-time high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false),
           "redundant alternate dead-time wait");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, false),
           "redundant alternate dead-time low");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0800u, 7u, 7u, 0u);
    dspic33_write_word(cpu, 0x0c2au, 1u);
    dspic33_write_word(cpu, 0x0c2cu, 2u);
    enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "push-pull dead-time start");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "push-pull dead-time high");
    dspic33_device_advance(cpu, 7u);
    expect(state, !dspic33_pwm_output(cpu, 0u, false),
           "push-pull alternate dead-time start");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, false),
           "push-pull alternate dead-time low");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 15u, 3u, 0x0204u);
    dspic33_write_word(cpu, 0x0c28u, 8u);
    dspic33_write_word(cpu, 0x0c2au, 7u);
    dspic33_write_word(cpu, 0x0c2cu, 2u);
    enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "center dead-time high");
    dspic33_device_advance(cpu, 3u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "center dead-time gap high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "center dead-time gap low");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, false), "center dead-time low");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_pwm_dead_time(cpu, 0u, false, 0u),
           "schedule subtract compensation");
    dspic33_device_advance(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 15u, 7u, 0x00c0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 1u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "subtract compensation shortens high");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_pwm_dead_time(cpu, 0u, true, 0u),
           "schedule add compensation");
    dspic33_device_advance(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 15u, 7u, 0x00c0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 1u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 8u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "add compensation lengthens high");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_dead_time(cpu, 0u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 15u, 7u, 0x00e0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 1u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "compensation polarity inversion");
}

static void duty_cases(PwmConformance* state, Dspic33* cpu) {
    uint8_t generator;
    uint16_t duty;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        for (duty = 0u; duty <= 8u; duty++) {
            dspic33_reset(cpu, 0u);
            configure_generator(cpu, generator, 0x0400u, 7u, duty, 0x0080u);
            enable_pwm(cpu, 0u);
            expect(state, dspic33_pwm_output(cpu, generator, true),
                   "duty starts asserted");
            dspic33_device_advance(cpu, 4u);
            expect(state, dspic33_pwm_output(cpu, generator, true) == (duty >= 4u),
                   "duty boundary output");
        }
    }

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 1u, 0x0180u);
    dspic33_write_word(cpu, 0x0c0au, 6u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "master duty selection");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0c00u, 7u, 7u, 0x0284u);
    dspic33_write_word(cpu, 0x0c28u, 3u);
    dspic33_write_word(cpu, 0x0c2eu, 1u);
    dspic33_write_word(cpu, 0x0c30u, 2u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 1u);
    expect(state, cpu->io.pwm_counter[0][0] == 1u, "independent center counter up");
    dspic33_device_advance(cpu, 2u);
    expect(state, cpu->io.pwm_counter[0][0] == 3u, "independent center peak");
    dspic33_device_advance(cpu, 3u);
    expect(state, cpu->io.pwm_counter[0][0] == 0u, "independent center counter down");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0c00u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c28u, 2u);
    dspic33_write_word(cpu, 0x0c30u, 4u);
    enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "phase-shifted high idle");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "phase-shifted low idle");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "phase-shifted high start");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "phase-shifted low wait");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "phase-shifted high end");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "phase-shifted low start");
}

static void update_cases(PwmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c26u, 1u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 7u, "deferred duty update");
    dspic33_device_advance(cpu, 8u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 1u, "boundary duty update");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0081u);
    enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c26u, 2u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 2u, "immediate duty update");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    expect(state, cpu->io.pwm_active_period[0] == 7u, "deferred period update");
    dspic33_write_word(cpu, 0x0c00u, 0x8400u);
    dspic33_write_word(cpu, 0x0c04u, 5u);
    expect(state, cpu->io.pwm_active_period[0] == 5u, "immediate period update");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0c00u, 7u, 1u, 0x0080u);
    enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c28u, 2u);
    expect(state, cpu->io.pwm_active_phase[0][0] == 0u, "deferred phase update");
    dspic33_device_advance(cpu, 8u);
    expect(state, cpu->io.pwm_active_phase[0][0] == 2u, "boundary phase update");
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "boundary phase output");
}

static void trigger_cases(PwmConformance* state, Dspic33* cpu) {
    uint8_t generator;
    uint8_t divider;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        for (divider = 0u; divider < 4u; divider++) {
            uint16_t address = base(generator);
            uint8_t match;
            dspic33_reset(cpu, 0u);
            configure_generator(cpu, generator, 0x0400u, 3u, 1u, 0x0480u);
            dspic33_write_word(cpu, (uint16_t)(address + 0x12u), 1u);
            dspic33_write_word(cpu, (uint16_t)(address + 0x14u),
                               (uint16_t)(divider << 12u));
            enable_pwm(cpu, 0u);
            for (match = 0u; match <= divider; match++) {
                dspic33_device_advance(cpu, match == 0u ? 1u : 4u);
                expect(state,
                       interrupt_flag(cpu, irqs[generator]) == (match == divider),
                       "generator trigger divider");
            }
            expect(state, (dspic33_read_word(cpu, address) & 0x2000u) != 0u,
                   "generator trigger status");
            dspic33_write_word(cpu, address, 0x0080u);
            expect(state, (dspic33_read_word(cpu, address) & 0x2000u) == 0u,
                   "generator trigger status clear");
        }
    }

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 3u, 1u, 0x0480u);
    dspic33_write_word(cpu, 0x0c32u, 1u);
    dspic33_write_word(cpu, 0x0c34u, 2u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 5u);
    expect(state, !interrupt_flag(cpu, 94u), "trigger start delay");
    dspic33_device_advance(cpu, 4u);
    expect(state, interrupt_flag(cpu, 94u), "trigger after start delay");

    for (divider = 0u; divider < 4u; divider++) {
        uint8_t match;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0c02u, 1u);
        dspic33_write_word(cpu, 0x0c04u, 3u);
        dspic33_write_word(cpu, 0x0c06u, 1u);
        enable_pwm(cpu, (uint16_t)(0x0800u | divider));
        for (match = 0u; match <= divider; match++) {
            dspic33_device_advance(cpu, match == 0u ? 1u : 4u);
            expect(state, interrupt_flag(cpu, 57u) == (match == divider),
                   "special event divider");
        }
        expect(state, (dspic33_read_word(cpu, 0x0c00u) & 0x1000u) != 0u,
               "special event status");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0c02u, 1u);
    dspic33_write_word(cpu, 0x0c04u, 7u);
    dspic33_write_word(cpu, 0x0c0eu, 0x0800u);
    dspic33_write_word(cpu, 0x0c12u, 3u);
    dspic33_write_word(cpu, 0x0c14u, 2u);
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 2u);
    expect(state, interrupt_flag(cpu, 73u), "secondary special event interrupt");
}

static void protection_cases(PwmConformance* state, Dspic33* cpu) {
    uint8_t generator;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t address = base(generator);
        dspic33_reset(cpu, 0u);
        configure_generator(cpu, generator, 0x0400u, 7u, 7u, 0x1080u);
        dspic33_write_word(cpu, (uint16_t)(address + 4u), (uint16_t)(2u << 3u));
        enable_pwm(cpu, 0u);
        expect(state, dspic33_pwm_fault(cpu, 2u, true, 0u), "schedule PWM fault");
        dspic33_device_advance(cpu, 0u);
        expect(state, !dspic33_pwm_output(cpu, generator, true),
               "latched fault output");
        expect(state, interrupt_flag(cpu, irqs[generator]), "latched fault interrupt");
        expect(state, (dspic33_read_word(cpu, address) & 0x8000u) != 0u,
               "latched fault status");
        dspic33_pwm_fault(cpu, 2u, false, 0u);
        dspic33_device_advance(cpu, 0u);
        dspic33_write_word(cpu, address, 0x0080u);
        dspic33_device_advance(cpu, 8u);
        expect(state, dspic33_pwm_output(cpu, generator, true),
               "latched fault release");
    }

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0880u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((3u << 10u) | 0x0100u));
    enable_pwm(cpu, 0u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "current-limit output");
    expect(state, interrupt_flag(cpu, 94u), "current-limit interrupt");
    expect(state, (dspic33_read_word(cpu, 0x0c20u) & 0x4000u) != 0u,
           "current-limit status");
    expect(state, dspic33_read_word(cpu, 0x0c38u) == 0u, "current-limit time capture");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((4u << 3u) | 0x0005u));
    enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 4u, false, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "active-low fault");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0282u);
    dspic33_write_word(cpu, 0x0c28u, 7u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)(5u << 10u));
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_current_limit(cpu, 5u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, cpu->io.pwm_counter[0][0] == 0u, "current reset time base");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((6u << 3u) | 1u));
    dspic33_write_word(cpu, 0x0c3au, 0x8800u);
    dspic33_write_word(cpu, 0x0c3cu, 4u);
    enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 6u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "leading-edge fault blanking");
    dspic33_device_advance(cpu, 4u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "fault after blanking");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    configure_generator(cpu, 1u, 0x0400u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((6u << 3u) | 1u));
    dspic33_write_word(cpu, 0x0c3au, 0x0820u);
    dspic33_write_word(cpu, 0x0c3eu, 0x0200u);
    enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 6u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "state blanked fault");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "state blanking end");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((6u << 3u) | 1u));
    dspic33_write_word(cpu, 0x0c3au, 0x0820u);
    dspic33_write_word(cpu, 0x0c3eu, 0x0700u);
    enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 6u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "reserved state blank source");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((2u << 3u) | 1u));
    enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_pwm_fault(cpu, 2u, false, 0u);
    dspic33_device_advance(cpu, 7u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "cycle fault held before boundary");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_pwm_output(cpu, 0u, true),
           "cycle fault released at boundary");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((3u << 10u) | 0x0100u));
    enable_pwm(cpu, 0u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_pwm_current_limit(cpu, 3u, false, 0u);
    dspic33_device_advance(cpu, 7u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "current limit held before boundary");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_pwm_output(cpu, 0u, true),
           "current limit released at boundary");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x1080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((2u << 3u) | 3u));
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "disabled fault output");
    expect(state, interrupt_flag(cpu, 94u), "disabled fault interrupt");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c22u, 0xc40cu);
    dspic33_write_word(cpu, 0x0c24u,
                       (uint16_t)((3u << 10u) | 0x0100u | (2u << 3u) | 1u));
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "current-limit high data");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "current-limit low data");
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "fault priority high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "fault priority low");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c22u, 0xc430u);
    dspic33_write_word(cpu, 0x0c24u,
                       (uint16_t)(0x8000u | (3u << 10u) | 0x0100u | (2u << 3u) | 1u));
    enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "independent current high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "independent current low");
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "independent fault high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "independent fault low");
}

static void synchronization_cases(PwmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
    enable_pwm(cpu, 0x0080u);
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 5u, "counter before sync");
    expect(state, dspic33_pwm_sync(cpu, 0u, true, 0u), "schedule PWM sync");
    dspic33_device_advance(cpu, 0u);
    expect(state, cpu->io.pwm_master_counter[0] == 0u, "rising PWM sync");
    dspic33_write_word(cpu, 0x0c00u, 0x8280u);
    dspic33_pwm_sync(cpu, 0u, false, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, cpu->io.pwm_master_counter[0] == 0u, "falling PWM sync");
    expect(state, !dspic33_pwm_sync(cpu, 2u, true, 0u), "reject PWM sync input");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0u, 15u, 15u, 0x0080u);
    enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c22u, 0xc201u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "synchronized override deferred");
    dspic33_device_advance(cpu, 15u);
    expect(state, dspic33_pwm_output(cpu, 0u, true),
           "synchronized override before boundary");
    dspic33_device_advance(cpu, 1u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "synchronized override at boundary");
    dspic33_write_word(cpu, 0x0c22u, 0xc001u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "synchronized override release deferred");
    dspic33_device_advance(cpu, 16u);
    expect(state, dspic33_pwm_output(cpu, 0u, true),
           "synchronized override release boundary");

    dspic33_write_word(cpu, 0x0c22u, 0xc003u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "synchronized swap deferred");
    expect(state, !dspic33_pwm_output(cpu, 0u, false),
           "synchronized swap low deferred");
    dspic33_device_advance(cpu, 16u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "synchronized swap high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "synchronized swap low");

    dspic33_write_word(cpu, 0x0c22u, 0xc200u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "asynchronous override");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 31u, 1u, 0x0080u);
    enable_pwm(cpu, 0x0100u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "sync output idle");
    dspic33_device_advance(cpu, 32u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "sync output pulse");
    dspic33_device_advance(cpu, 11u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "sync output pulse width");
    dspic33_device_advance(cpu, 1u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "sync output pulse end");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 31u, 1u, 0x0080u);
    enable_pwm(cpu, 0x0300u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "inverted sync output idle");
    dspic33_device_advance(cpu, 32u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "inverted sync output pulse");
    expect(state, !dspic33_pwm_sync_output(cpu, 2u), "reject sync output time base");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 31u, 1u, 0x0080u);
    enable_pwm(cpu, 0x0100u);
    dspic33_device_advance(cpu, 100u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "batched sync output pulse");
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "batched sync output pulse end");
}

static void chop_and_power_cases(PwmConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 15u, 15u, 0x0080u);
    dspic33_write_word(cpu, 0x0c1au, 0x8003u);
    dspic33_write_word(cpu, 0x0c3eu, 2u);
    enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "chop initial output");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "chop gated output");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "chop repeated output");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 15u, 15u, 0x0080u);
    configure_generator(cpu, 1u, 0x0400u, 15u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c3eu, 0x000au);
    enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "PWM source chop high");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "PWM source chop low");
    dspic33_write_word(cpu, 0x0c3eu, 0x001eu);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "reserved chop source low");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
    enable_pwm(cpu, 0x2000u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 0u, "PWM stops in idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_write_word(cpu, 0x0c00u, 0x8000u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 5u, "PWM continues in idle");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 5u, "PWM stops in sleep");
}

static void boundary_cases(PwmConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;
    expect(state, !dspic33_pwm_fault(cpu, DSPIC33_PWM_INPUT_COUNT, true, 0u),
           "reject PWM fault source");
    expect(state, !dspic33_pwm_current_limit(cpu, DSPIC33_PWM_INPUT_COUNT, true, 0u),
           "reject PWM current-limit source");
    expect(state, !dspic33_pwm_output(cpu, DSPIC33_PWM_COUNT, true),
           "reject PWM output generator");
    expect(state, !dspic33_pwm_dead_time(cpu, DSPIC33_PWM_COUNT, true, 0u),
           "reject PWM dead-time generator");

    dspic33_reset(cpu, 0u);
    configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
    enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_fault(cpu, 7u, true, 3u), "delayed PWM fault");
    dspic33_device_advance(cpu, 2u);
    expect(state, (cpu->io.pwm_fault_inputs & (1u << 7u)) == 0u,
           "PWM fault before delay");
    dspic33_device_advance(cpu, 1u);
    expect(state, (cpu->io.pwm_fault_inputs & (1u << 7u)) != 0u,
           "PWM fault after delay");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize PWM copy");
    if (!initialized) {
        return;
    }
    expect(state, dspic33_copy(&copy, cpu), "copy PWM state");
    expect(state, copy.io.pwm_master_counter[0] == cpu->io.pwm_master_counter[0],
           "copy PWM counter");
    expect(state, copy.io.pwm_fault_inputs == cpu->io.pwm_fault_inputs,
           "copy PWM inputs");
    expect(state, copy.io.pwm[0] == cpu->io.pwm[0], "copy PWM output");
    dspic33_device_advance(cpu, 4u);
    dspic33_device_advance(&copy, 4u);
    expect(state, copy.io.pwm_master_counter[0] == cpu->io.pwm_master_counter[0],
           "advance copied PWM counter");
    dspic33_destroy(&copy);
}

int main(void) {
    Dspic33 cpu;
    PwmConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize PWM processor");
    if (!initialized) {
        fprintf(stderr, "cannot initialize PWM conformance simulator\n");
        return 1;
    }
    register_cases(&state, &cpu);
    clock_cases(&state, &cpu);
    mode_cases(&state, &cpu);
    duty_cases(&state, &cpu);
    dead_time_cases(&state, &cpu);
    update_cases(&state, &cpu);
    trigger_cases(&state, &cpu);
    protection_cases(&state, &cpu);
    synchronization_cases(&state, &cpu);
    chop_and_power_cases(&state, &cpu);
    boundary_cases(&state, &cpu);
    printf("[pwm-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&cpu);
    return state.failed == 0u ? 0 : 1;
}
