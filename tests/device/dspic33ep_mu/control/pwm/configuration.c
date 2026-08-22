#include "device/dspic33ep_mu/control/pwm/internal.h"

uint16_t dspic33_pwm_test_base(uint8_t generator) {
    return (uint16_t)(0x0c20u + generator * 0x20u);
}

bool dspic33_pwm_test_interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

bool dspic33_pwm_test_gpio_pin_is(const Dspic33* cpu, uint8_t port, uint8_t bit, bool expected) {
    bool high;
    return dspic33_gpio_pin(cpu, port, bit, &high) && high == expected;
}

void dspic33_pwm_test_configure_interrupt(Dspic33* cpu, uint8_t generator, uint16_t vector) {
    uint8_t irq = irqs[generator];
    uint16_t enable = (uint16_t)(0x0820u + (irq / 16u) * 2u);
    uint16_t priority = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(cpu, enable,
                       (uint16_t)(dspic33_read_word(cpu, enable) | (uint16_t)(1u << (irq % 16u))));
    dspic33_write_word(cpu, priority,
                       (uint16_t)((dspic33_read_word(cpu, priority) & ~(uint16_t)(7u << shift)) |
                                  (uint16_t)(3u << shift)));
    cpu->program[(0x0014u + irq * 2u) / 2u] = vector;
    cpu->w[15] = 0x1800u;
}

void dspic33_pwm_test_configure_generator(Dspic33* cpu, uint8_t generator, uint16_t mode,
                                          uint16_t period, uint16_t duty, uint16_t control) {
    uint16_t address = dspic33_pwm_test_base(generator);
    dspic33_write_word(cpu, 0x0c00u, 0u);
    dspic33_write_word(cpu, 0x0c02u, 1u);
    dspic33_write_word(cpu, 0x0c04u, period);
    dspic33_write_word(cpu, (uint16_t)(address + 6u), duty);
    dspic33_write_word(cpu, (uint16_t)(address + 0x0eu), duty);
    dspic33_write_word(cpu, (uint16_t)(address + 2u), (uint16_t)(0xc000u | mode));
    dspic33_write_word(cpu, address, control);
}

void dspic33_pwm_test_enable_pwm(Dspic33* cpu, uint16_t control) {
    dspic33_write_word(cpu, 0x0c00u, (uint16_t)(control | 0x8000u));
}

void dspic33_pwm_test_register_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t offsets[] = {0x00u, 0x02u, 0x04u, 0x06u, 0x08u, 0x0au, 0x0cu, 0x0eu,
                                       0x10u, 0x12u, 0x14u, 0x16u, 0x18u, 0x1au, 0x1cu, 0x1eu};
    static const uint16_t masks[] = {0x1fefu, 0xffffu, 0xffffu, 0xffffu, 0xffffu, 0x3fffu,
                                     0x3fffu, 0xffffu, 0xffffu, 0xffffu, 0xf03fu, 0x0000u,
                                     0x0000u, 0xfc3fu, 0x0fffu, 0x0f3fu};
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
            uint16_t address = (uint16_t)(dspic33_pwm_test_base(generator) + offsets[index]);
            dspic33_write_word(cpu, address, 0xffffu);
            expect(state, dspic33_read_word(cpu, address) == masks[index],
                   "generator register mask");
            dspic33_write_word(cpu, address, 0u);
        }
    }
}

void dspic33_pwm_test_clock_cases(TestState* state, Dspic33* cpu) {
    uint8_t divider;
    for (divider = 0u; divider < 7u; divider++) {
        uint16_t expected_ticks = divider == 0u ? 2u : divider == 1u ? 1u : 0u;
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 100u, 0u, 0x0080u);
        dspic33_write_word(cpu, 0x0c02u, divider);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        dspic33_device_advance(cpu, 1u);
        expect(state, cpu->io.pwm_master_counter[0] == expected_ticks,
               "primary divider first cycle");
        if (divider >= 2u) {
            uint16_t cycles = (uint16_t)(1u << (divider - 1u));
            dspic33_device_advance(cpu, (uint16_t)(cycles - 1u));
            expect(state, cpu->io.pwm_master_counter[0] == 1u, "primary divider accumulated cycle");
        }
    }
    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 9u, 5u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c02u, 6u);
    expect(state, dspic33_read_word(cpu, 0x0c02u) == 1u, "divider locked while enabled");

    for (divider = 0u; divider < 7u; divider++) {
        uint16_t expected_ticks = divider == 0u ? 2u : divider == 1u ? 1u : 0u;
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 100u, 0u, 0x0088u);
        dspic33_write_word(cpu, 0x0c12u, 100u);
        dspic33_write_word(cpu, 0x0c10u, divider);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
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

void dspic33_pwm_test_mode_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t modes[] = {0x0000u, 0x0400u, 0x0800u, 0x0c00u};
    uint8_t mode;
    for (mode = 0u; mode < sizeof(modes) / sizeof(modes[0]); mode++) {
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, 0u, modes[mode], 7u, 3u, 0x0080u);
        if (mode == 3u) {
            dspic33_write_word(cpu, 0x0c2eu, 1u);
        }
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        expect(state, dspic33_pwm_output(cpu, 0u, true), "mode initial high output");
        expect(state, dspic33_pwm_output(cpu, 0u, false) == (mode == 1u || mode == 3u),
               "mode initial low output");
        dspic33_device_advance(cpu, 4u);
        expect(state, !dspic33_pwm_output(cpu, 0u, true), "mode inactive high output");
        expect(state, dspic33_pwm_output(cpu, 0u, false) == (mode == 0u),
               "mode inactive low output");
    }

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c22u, 0xf3c0u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
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
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0800u, 3u, 3u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "push-pull first high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "push-pull first low");
    dspic33_device_advance(cpu, 4u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "push-pull second high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "push-pull second low");
}

void dspic33_pwm_test_dead_time_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 15u, 7u, 0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 3u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
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
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 15u, 7u, 0x0040u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 3u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 6u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "negative dead-time high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "negative dead-time overlap");
    dspic33_device_advance(cpu, 5u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "negative dead-time high end");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "negative dead-time low");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 4u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "redundant dead-time start");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "redundant alternate dead-time start");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "redundant dead-time high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "redundant alternate dead-time wait");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, false), "redundant alternate dead-time low");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0800u, 7u, 7u, 0u);
    dspic33_write_word(cpu, 0x0c2au, 1u);
    dspic33_write_word(cpu, 0x0c2cu, 2u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "push-pull dead-time start");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "push-pull dead-time high");
    dspic33_device_advance(cpu, 7u);
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "push-pull alternate dead-time start");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, false), "push-pull alternate dead-time low");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 15u, 3u, 0x0204u);
    dspic33_write_word(cpu, 0x0c28u, 8u);
    dspic33_write_word(cpu, 0x0c2au, 7u);
    dspic33_write_word(cpu, 0x0c2cu, 2u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "center dead-time high");
    dspic33_device_advance(cpu, 3u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "center dead-time gap high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "center dead-time gap low");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, false), "center dead-time low");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_pwm_dead_time(cpu, 0u, false, 0u), "schedule subtract compensation");
    dspic33_device_advance(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 15u, 7u, 0x00c0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 1u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "subtract compensation shortens high");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_pwm_dead_time(cpu, 0u, true, 0u), "schedule add compensation");
    dspic33_device_advance(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 15u, 7u, 0x00c0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 1u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 8u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "add compensation lengthens high");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_dead_time(cpu, 0u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 15u, 7u, 0x00e0u);
    dspic33_write_word(cpu, 0x0c2au, 2u);
    dspic33_write_word(cpu, 0x0c2cu, 1u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "compensation polarity inversion");
}

void dspic33_pwm_test_b1_dead_time_cases(TestState* state, Dspic33* cpu) {
    uint8_t generator;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t address = dspic33_pwm_test_base(generator);
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0u, 15u, 5u, 0x00c0u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0au), 3u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0cu), 1u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        expect(state, dspic33_pwm_output(cpu, generator, false),
               "B1 zero compensation bypasses alternate dead time");
        dspic33_device_advance(cpu, 1u);
        expect(state, !dspic33_pwm_output(cpu, generator, true),
               "B1 subtract compensation saturates below twice DTR");

        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0u, 15u, 6u, 0x00c0u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0au), 3u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0cu), 1u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        dspic33_device_advance(cpu, 1u);
        expect(state, dspic33_pwm_output(cpu, generator, true),
               "B1 subtract compensation retains twice DTR boundary");

        dspic33_reset(cpu, 0u);
        expect(state, dspic33_pwm_dead_time(cpu, generator, true, 0u),
               "schedule B1 additive compensation");
        dspic33_device_advance(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0u, 15u, 9u, 0x00c0u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0au), 3u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0cu), 1u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        expect(state, dspic33_pwm_output(cpu, generator, true),
               "B1 full compensation bypasses alternate dead time");
        dspic33_device_advance(cpu, 7u);
        expect(state, dspic33_pwm_output(cpu, generator, true),
               "B1 add compensation saturates at twice DTR boundary");

        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0u, 15u, 2u, 0u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0au), 1u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0cu), 3u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        expect(state, dspic33_pwm_output(cpu, generator, true),
               "B1 short edge duty removes leading dead time");
        dspic33_device_advance(cpu, 3u);
        expect(state, dspic33_pwm_output(cpu, generator, false),
               "B1 short edge duty removes trailing dead time");

        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0u, 15u, 7u, 0x02c4u);
        dspic33_write_word(cpu, (uint16_t)(address + 8u), 15u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0au), 0u);
        dspic33_write_word(cpu, (uint16_t)(address + 0x0cu), 4u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        dspic33_device_advance(cpu, 6u);
        expect(state, dspic33_pwm_output(cpu, generator, true),
               "B1 center compensation leaves high edge unchanged");
        dspic33_device_advance(cpu, 2u);
        expect(state, !dspic33_pwm_output(cpu, generator, false),
               "B1 center compensation extends low edge");
    }
}

void dspic33_pwm_test_duty_cases(TestState* state, Dspic33* cpu) {
    uint8_t generator;
    uint16_t duty;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        for (duty = 0u; duty <= 8u; duty++) {
            dspic33_reset(cpu, 0u);
            dspic33_pwm_test_configure_generator(cpu, generator, 0x0400u, 7u, duty, 0x0080u);
            dspic33_pwm_test_enable_pwm(cpu, 0u);
            expect(state, dspic33_pwm_output(cpu, generator, true), "duty starts asserted");
            dspic33_device_advance(cpu, 4u);
            expect(state, dspic33_pwm_output(cpu, generator, true) == (duty >= 4u),
                   "duty boundary output");
        }
    }

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 1u, 0x0180u);
    dspic33_write_word(cpu, 0x0c0au, 6u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "master duty selection");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0c00u, 7u, 7u, 0x0284u);
    dspic33_write_word(cpu, 0x0c28u, 3u);
    dspic33_write_word(cpu, 0x0c2eu, 1u);
    dspic33_write_word(cpu, 0x0c30u, 2u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 1u);
    expect(state, cpu->io.pwm_counter[0][0] == 1u, "independent center counter up");
    dspic33_device_advance(cpu, 2u);
    expect(state, cpu->io.pwm_counter[0][0] == 3u, "independent center peak");
    dspic33_device_advance(cpu, 3u);
    expect(state, cpu->io.pwm_counter[0][0] == 0u, "independent center counter down");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0c00u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c28u, 2u);
    dspic33_write_word(cpu, 0x0c30u, 4u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "phase-shifted high idle");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "phase-shifted low idle");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "phase-shifted high start");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "phase-shifted low wait");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "phase-shifted high end");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "phase-shifted low start");
}

void dspic33_pwm_test_update_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c26u, 1u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 7u, "deferred duty update");
    dspic33_device_advance(cpu, 8u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 1u, "boundary duty update");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0081u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c26u, 2u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 2u, "immediate duty update");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    expect(state, cpu->io.pwm_active_period[0] == 7u, "deferred period update");
    dspic33_write_word(cpu, 0x0c00u, 0x8400u);
    dspic33_write_word(cpu, 0x0c04u, 5u);
    expect(state, cpu->io.pwm_active_period[0] == 5u, "immediate period update");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0c00u, 7u, 1u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c28u, 2u);
    expect(state, cpu->io.pwm_active_phase[0][0] == 0u, "deferred phase update");
    dspic33_device_advance(cpu, 8u);
    expect(state, cpu->io.pwm_active_phase[0][0] == 2u, "boundary phase update");
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "boundary phase output");
}

static uint16_t active_timing_value(const Dspic33* cpu, uint8_t generator, uint16_t offset) {
    switch (offset) {
    case 6u:
        return cpu->io.pwm_active_duty[generator][0];
    case 8u:
        return cpu->io.pwm_active_phase[generator][0];
    case 0x0au:
        return cpu->io.pwm_active_dead_time[generator][0];
    case 0x0cu:
        return cpu->io.pwm_active_dead_time[generator][1];
    case 0x0eu:
        return cpu->io.pwm_active_duty[generator][1];
    case 0x10u:
        return cpu->io.pwm_active_phase[generator][1];
    default:
        return UINT16_MAX;
    }
}

void dspic33_pwm_test_b1_update_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t timing_offsets[] = {6u, 8u, 0x0au, 0x0cu, 0x0eu, 0x10u};
    Dspic33 copy;
    bool initialized;
    uint8_t generator;
    uint8_t index;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t address = dspic33_pwm_test_base(generator);
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0x0400u, 7u, 6u, 0x0080u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        if ((generator & 1u) == 0u) {
            dspic33_write_word(cpu, 0x0c04u, 3u);
            dspic33_write_word(cpu, (uint16_t)(address + 6u), 1u);
        } else {
            dspic33_write_word(cpu, (uint16_t)(address + 6u), 1u);
            dspic33_write_word(cpu, 0x0c04u, 3u);
        }
        dspic33_device_advance(cpu, 8u);
        expect(state, cpu->io.pwm_active_period[0] == 3u,
               "B1 primary period updates at first boundary");
        expect(state, cpu->io.pwm_active_duty[generator][0] == 6u,
               "B1 primary duty waits after period update");
        dspic33_device_advance(cpu, 4u);
        expect(state, cpu->io.pwm_active_duty[generator][0] == 1u,
               "B1 primary duty updates at second boundary");
    }

    for (index = 0u; index < sizeof(timing_offsets) / sizeof(timing_offsets[0]); index++) {
        uint16_t offset = timing_offsets[index];
        uint16_t previous = offset == 6u || offset == 0x0eu ? 6u : 0u;
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 6u, 0x0080u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        dspic33_write_word(cpu, 0x0c04u, 3u);
        dspic33_write_word(cpu, (uint16_t)(dspic33_pwm_test_base(0u) + offset), 2u);
        dspic33_device_advance(cpu, 8u);
        expect(state, active_timing_value(cpu, 0u, offset) == previous,
               "B1 timing register waits after period update");
        dspic33_device_advance(cpu, 4u);
        expect(state, active_timing_value(cpu, 0u, offset) == 2u,
               "B1 timing register updates one cycle later");
    }

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 6u, 0x0088u);
    dspic33_write_word(cpu, 0x0c12u, 7u);
    dspic33_write_word(cpu, 0x0c10u, 1u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c12u, 3u);
    dspic33_write_word(cpu, 0x0c26u, 1u);
    dspic33_device_advance(cpu, 8u);
    expect(state, cpu->io.pwm_active_period[1] == 3u,
           "B1 secondary period updates at first boundary");
    expect(state, cpu->io.pwm_active_duty[0][0] == 6u,
           "B1 secondary duty waits after period update");
    dspic33_device_advance(cpu, 4u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 1u,
           "B1 secondary duty updates at second boundary");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 1u, 0x0180u);
    dspic33_write_word(cpu, 0x0c0au, 6u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    dspic33_write_word(cpu, 0x0c0au, 1u);
    dspic33_device_advance(cpu, 8u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 6u, "B1 master duty waits after period update");
    dspic33_device_advance(cpu, 4u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 1u, "B1 master duty updates at second boundary");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 6u, 0x0280u);
    dspic33_write_word(cpu, 0x0c28u, 7u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    dspic33_write_word(cpu, 0x0c26u, 1u);
    dspic33_device_advance(cpu, 8u);
    expect(state, cpu->io.pwm_active_duty[0][0] == 1u,
           "B1 independent timing ignores master period delay");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 6u, 0x0081u);
    dspic33_pwm_test_enable_pwm(cpu, 0x0400u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    dspic33_write_word(cpu, 0x0c26u, 1u);
    expect(state, cpu->io.pwm_active_period[0] == 3u && cpu->io.pwm_active_duty[0][0] == 1u,
           "B1 immediate period and duty bypass delay");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 6u, 0x00c0u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    expect(state, dspic33_pwm_dead_time(cpu, 0u, true, 0u),
           "schedule B1 delayed compensation signal");
    dspic33_device_advance(cpu, 0u);
    dspic33_device_advance(cpu, 8u);
    expect(state, (cpu->io.pwm_dead_time_sampled & 1u) == 0u,
           "B1 compensation signal waits after period update");
    dspic33_device_advance(cpu, 4u);
    expect(state, (cpu->io.pwm_dead_time_sampled & 1u) != 0u,
           "B1 compensation signal updates at second boundary");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((2u << 3u) | 1u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_fault(cpu, 2u, true, 0u), "schedule B1 cycle fault assertion");
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_fault(cpu, 2u, false, 0u), "schedule B1 cycle fault release");
    dspic33_device_advance(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "B1 cycle fault release waits after period update");
    dspic33_device_advance(cpu, 4u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "B1 cycle fault releases at second boundary");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((3u << 10u) | 0x0100u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_current_limit(cpu, 3u, true, 0u),
           "schedule B1 current-limit assertion");
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_current_limit(cpu, 3u, false, 0u),
           "schedule B1 current-limit release");
    dspic33_device_advance(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true),
           "B1 current-limit release waits after period update");
    dspic33_device_advance(cpu, 4u);
    expect(state, dspic33_pwm_output(cpu, 0u, true),
           "B1 current-limit releases at second boundary");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 6u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c04u, 3u);
    dspic33_write_word(cpu, 0x0c26u, 1u);
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize B1 PWM update copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy B1 PWM update state");
        dspic33_device_advance(cpu, 8u);
        dspic33_device_advance(&copy, 8u);
        expect(state,
               copy.io.pwm_active_period[0] == cpu->io.pwm_active_period[0] &&
                   copy.io.pwm_active_duty[0][0] == cpu->io.pwm_active_duty[0][0],
               "copied B1 PWM first boundary");
        dspic33_device_advance(cpu, 4u);
        dspic33_device_advance(&copy, 4u);
        expect(state, copy.io.pwm_active_duty[0][0] == cpu->io.pwm_active_duty[0][0],
               "copied B1 PWM second boundary");
        dspic33_release(&copy);
    }
    dspic33_reset(cpu, 0u);
    expect(state, cpu->io.pwm_period_update == 0u && cpu->io.pwm_timing_update == 0u,
           "reset B1 PWM update state");
}

void dspic33_pwm_test_trigger_cases(TestState* state, Dspic33* cpu) {
    uint8_t generator;
    uint8_t divider;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        for (divider = 0u; divider < 4u; divider++) {
            uint16_t address = dspic33_pwm_test_base(generator);
            uint8_t match;
            dspic33_reset(cpu, 0u);
            dspic33_pwm_test_configure_generator(cpu, generator, 0x0400u, 3u, 1u, 0x0480u);
            dspic33_write_word(cpu, (uint16_t)(address + 0x12u), 1u);
            dspic33_write_word(cpu, (uint16_t)(address + 0x14u), (uint16_t)(divider << 12u));
            dspic33_pwm_test_enable_pwm(cpu, 0u);
            for (match = 0u; match <= divider; match++) {
                dspic33_device_advance(cpu, match == 0u ? 1u : 4u);
                expect(state,
                       dspic33_pwm_test_interrupt_flag(cpu, irqs[generator]) == (match == divider),
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
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 3u, 1u, 0x0480u);
    dspic33_write_word(cpu, 0x0c32u, 1u);
    dspic33_write_word(cpu, 0x0c34u, 2u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 5u);
    expect(state, !dspic33_pwm_test_interrupt_flag(cpu, 94u), "trigger start delay");
    dspic33_device_advance(cpu, 4u);
    expect(state, dspic33_pwm_test_interrupt_flag(cpu, 94u), "trigger after start delay");

    for (divider = 0u; divider < 4u; divider++) {
        uint8_t match;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0c02u, 1u);
        dspic33_write_word(cpu, 0x0c04u, 3u);
        dspic33_write_word(cpu, 0x0c06u, 1u);
        dspic33_pwm_test_enable_pwm(cpu, (uint16_t)(0x0800u | divider));
        for (match = 0u; match <= divider; match++) {
            dspic33_device_advance(cpu, match == 0u ? 1u : 4u);
            expect(state, dspic33_pwm_test_interrupt_flag(cpu, 57u) == (match == divider),
                   "special event divider");
        }
        expect(state, (dspic33_read_word(cpu, 0x0c00u) & 0x1000u) != 0u, "special event status");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0c02u, 1u);
    dspic33_write_word(cpu, 0x0c04u, 7u);
    dspic33_write_word(cpu, 0x0c0eu, 0x0800u);
    dspic33_write_word(cpu, 0x0c12u, 3u);
    dspic33_write_word(cpu, 0x0c14u, 2u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_test_interrupt_flag(cpu, 73u), "secondary special event interrupt");
    expect(state, (dspic33_read_word(cpu, 0x0c0eu) & 0x1000u) != 0u,
           "secondary special event status");
}
