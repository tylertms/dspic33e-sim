#include "device/dspic33ep_mu/control/qei/internal.h"

uint32_t dspic33_qei_test_read_counter(Dspic33* cpu, uint16_t low) {
    uint32_t value = dspic33_read_word(cpu, low);
    return value | ((uint32_t)dspic33_read_word(cpu, (uint16_t)(low + 2u)) << 16u);
}

void dspic33_qei_test_write_counter(Dspic33* cpu, uint16_t low, uint16_t hold, uint32_t value) {
    dspic33_write_word(cpu, hold, (uint16_t)(value >> 16u));
    dspic33_write_word(cpu, low, (uint16_t)value);
}

bool dspic33_qei_test_input(Dspic33* cpu, uint8_t channel, Dspic33QeiInput source, bool high) {
    return dspic33_qei_input(cpu, channel, source, high, 0u) && dspic33_device_advance(cpu, 0u);
}

void dspic33_qei_test_reset_qei(Dspic33* cpu) {
    uint8_t channel;
    uint8_t source;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        for (source = 0u; source < 4u; source++) {
            dspic33_qei_input(cpu, channel, (Dspic33QeiInput)source, false, 0u);
        }
    }
    dspic33_device_advance(cpu, 0u);
    dspic33_reset(cpu, 0u);
}

void dspic33_qei_test_clear_interrupt(Dspic33* cpu, uint8_t channel) {
    dspic33_write_word(cpu, interrupt_addresses[channel],
                       (uint16_t)(dspic33_read_word(cpu, interrupt_addresses[channel]) &
                                  ~interrupt_masks[channel]));
}

void dspic33_qei_test_select_pps_input(Dspic33* cpu, uint8_t channel, uint8_t input, uint8_t pin) {
    uint16_t address = pps_input_registers[channel][input / 2u];
    if ((input & 1u) == 0u) {
        dspic33_write_byte(cpu, address, pin);
    } else {
        dspic33_write_byte(cpu, (uint16_t)(address + 1u), pin);
    }
}
bool dspic33_qei_test_interrupt_set(Dspic33* cpu, uint8_t channel) {
    return (dspic33_read_word(cpu, interrupt_addresses[channel]) & interrupt_masks[channel]) != 0u;
}

void dspic33_qei_test_configure_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = channel == 0u ? 58u : 75u;
    uint16_t enable = (uint16_t)(0x0820u + (irq / 16u) * 2u);
    uint16_t priority = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(cpu, enable,
                       (uint16_t)(dspic33_read_word(cpu, enable) | (uint16_t)(1u << (irq % 16u))));
    dspic33_write_word(cpu, priority,
                       (uint16_t)((dspic33_read_word(cpu, priority) & ~(uint16_t)(7u << shift)) |
                                  (uint16_t)(3u << shift)));
    cpu->program[(0x0014u + irq * 2u) / 2u] = QEI_VECTOR;
    cpu->w[15] = 0x1800u;
}

void dspic33_qei_test_set_open_comparison_window(Dspic33* cpu, uint16_t base) {
    dspic33_write_word(cpu, (uint16_t)(base + 0x1eu), 0x7fffu);
    dspic33_write_word(cpu, (uint16_t)(base + 0x1cu), 0xffffu);
    dspic33_write_word(cpu, (uint16_t)(base + 0x22u), 0x8000u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x20u), 0x0000u);
}

void dspic33_qei_test_register_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t offsets[18] = {0x00u, 0x02u, 0x04u, 0x06u, 0x08u, 0x0au,
                                         0x0cu, 0x0eu, 0x10u, 0x12u, 0x14u, 0x16u,
                                         0x18u, 0x1au, 0x1cu, 0x1eu, 0x20u, 0x22u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t index;
        dspic33_qei_test_reset_qei(cpu);
        for (index = 0u; index < 18u; index++) {
            expect(state, dspic33_read_word(cpu, (uint16_t)(base + offsets[index])) == 0u,
                   "QEI register reset");
        }
        dspic33_write_word(cpu, base, UINT16_MAX);
        expect(state, dspic33_read_word(cpu, base) == 0xbf7fu, "QEI control access mask");
        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), UINT16_MAX);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0xffffu,
               "QEI IO control keeps live inputs read only");
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), UINT16_MAX);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0x1555u,
               "QEI status flags cannot be software set");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x5678u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1234u);
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u,
               "QEI direct position words read coherently");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) == 0x1234u,
               "QEI position low read captures high hold");
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                                       0xa1b2c3d4u);
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0xa1b2c3d4u,
               "QEI position hold write commits with low word");
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x16u), (uint16_t)(base + 0x1au),
                                       0x10203040u);
        expect(state,
               dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == 0x10203040u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x1au)) == 0x1020u,
               "QEI index hold transfers both directions");
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x55aau);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0x55aau,
               "QEI velocity read returns pre-clear value");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0u,
               "QEI velocity read clears counter");
        dspic33_write_word(cpu, (uint16_t)(base + 0x1cu), 0x2468u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x1cu)) == 0x2468u,
               "QEI GEC and IC alias storage is writable");
    }
}

void dspic33_qei_test_quadrature_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_QUADRATURE);
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "schedule QEI quadrature phase A rise");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, false),
               "complete QEI positive quadrature cycle");
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 4u,
               "QEI x4 quadrature positive count");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 4u,
               "QEI velocity follows quadrature count");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   cpu->stop_reason == DSPIC33_SILICON_RESULT_UNDEFINED,
               "B1 positive-direction Index counter remains silicon-undefined");
        cpu->stop_reason = DSPIC33_RUNNING;
        dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, false);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, false),
               "complete QEI negative quadrature cycle");
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI x4 quadrature negative count");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_SWAP);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "schedule swapped QEI input");
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX,
               "QEI input swap reverses first phase direction");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_PHASE_A_POLARITY);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) == 0u,
               "QEI polarity controls live phase status");
    }
}

void dspic33_qei_test_quadrature_transition_cases(TestState* state, Dspic33* cpu) {
    static const int8_t actions[16] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t previous;
        for (previous = 0u; previous < 4u; previous++) {
            uint8_t current;
            for (current = 0u; current < 4u; current++) {
                int8_t action = actions[(previous << 2u) | current];
                uint32_t expected = (uint32_t)(0x100 + action);
                dspic33_qei_test_reset_qei(cpu);
                dspic33_qei_test_set_open_comparison_window(cpu, base);
                dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
                dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, (previous & 1u) != 0u);
                dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, (previous & 2u) != 0u);
                dspic33_device_advance(cpu, 3u);
                dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                                               0x100u);
                dspic33_write_word(cpu, base, QEI_ENABLE);
                dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, (current & 1u) != 0u);
                dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, (current & 2u) != 0u);
                expect(state,
                       dspic33_device_advance(cpu, 3u) &&
                           dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == expected &&
                           dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == (uint16_t)action,
                       "QEI quadrature truth table transition");
            }
        }
    }
}

void dspic33_qei_test_divider_polarity_output_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t counter_divisors[8] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
    static const uint16_t filter_divisors[8] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 256u};
    static const int32_t positions[3] = {1, 3, 6};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t selection;
        for (selection = 0u; selection < 8u; selection++) {
            uint16_t divisor = counter_divisors[selection];
            dspic33_qei_test_reset_qei(cpu);
            dspic33_qei_test_set_open_comparison_window(cpu, base);
            dspic33_write_word(cpu, base,
                               (uint16_t)(QEI_ENABLE | ((uint16_t)selection << QEI_DIVIDER_SHIFT) |
                                          QEI_MODE_TIMER));
            expect(state,
                   dspic33_device_advance(cpu, (uint16_t)(divisor - 1u)) &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
                   "QEI counter divider waits for its complete period");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
                   "QEI counter divider clocks at the selected period");

            divisor = filter_divisors[selection];
            dspic33_qei_test_reset_qei(cpu);
            dspic33_qei_test_set_open_comparison_window(cpu, base);
            dspic33_write_word(cpu, (uint16_t)(base + 2u),
                               (uint16_t)(QEI_FILTER_ENABLE | ((uint16_t)selection << 11u)));
            dspic33_write_word(cpu, base, QEI_ENABLE);
            dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true);
            expect(state,
                   dspic33_device_advance(cpu, (uint16_t)(3u * divisor - 1u)) &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
                   "QEI filter divider waits for three complete samples");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
                   "QEI filter divider accepts the third stable sample");
        }

        for (selection = 0u; selection < 4u; selection++) {
            uint16_t polarity = (uint16_t)(1u << (4u + selection));
            dspic33_qei_test_reset_qei(cpu);
            dspic33_write_word(cpu, (uint16_t)(base + 2u), polarity);
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x000fu) ==
                       (uint16_t)(1u << selection),
                   "QEI input polarity inverts a low physical input");
            expect(state,
                   dspic33_qei_test_input(cpu, channel, (Dspic33QeiInput)selection, true) &&
                       (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x000fu) == 0u,
                   "QEI input polarity inverts a high physical input");
        }

        for (selection = 0u; selection < 4u; selection++) {
            uint8_t position_index;
            for (position_index = 0u; position_index < 3u; position_index++) {
                bool high;
                bool expected = (selection == 1u && positions[position_index] >= 2) ||
                                (selection == 2u && positions[position_index] <= 5) ||
                                (selection == 3u && positions[position_index] >= 2 &&
                                 positions[position_index] <= 5);
                dspic33_qei_test_reset_qei(cpu);
                dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu),
                                               (uint16_t)(base + 0x1eu), 2u);
                dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u),
                                               (uint16_t)(base + 0x22u), 5u);
                dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                                               (uint32_t)positions[position_index]);
                dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(selection << 9u));
                expect(state, dspic33_qei_compare_output(cpu, channel, &high) && high == expected,
                       "QEI compare output mode follows both signed boundaries");
            }
        }
    }
}

void dspic33_qei_test_external_mode_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_UP_DOWN);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI external up direction inputs");
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI external up direction increments");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI external down direction inputs");
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI external down direction decrements");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_GATE_ENABLE | QEI_MODE_GATE);
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI gated external clock while gate low");
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI low gate inhibits external clock");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI gated external clock while gate high");
        expect(state,
               dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == 0u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI external mode applies independent timer gates");
        expect(state,
               dspic33_device_advance(cpu, 5u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u,
               "QEI external gate mode does not also clock from FCY");

        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 2u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == 1u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI external clock drives each enabled timer gate");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_GATE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == 0u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI external ungated mode clocks position and velocity together");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply QEI external timer gate inputs");
        dspic33_write_word(cpu, base, QEI_ENABLE | 0x0008u | QEI_MODE_GATE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == UINT32_MAX &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == UINT16_MAX,
               "QEI external timer direction excludes only the interval timer");
    }
}

void dspic33_qei_test_timer_filter_power_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        expect(state, dspic33_device_advance(cpu, 8u), "advance QEI internal timer");
        expect(state,
               dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 8u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == 0u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 8u,
               "QEI internal ungated mode clocks position and velocity together");
        dspic33_write_word(cpu, base, QEI_ENABLE | (2u << QEI_INDEX_MATCH_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   dspic33_device_advance(cpu, 4u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 12u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == 4u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 4u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 4u,
               "QEI internal mode clocks each counter through its input gate");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply QEI internal timer gate inputs");
        dspic33_write_word(cpu, base, QEI_ENABLE | 0x0008u | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x16u)) == UINT32_MAX &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == UINT16_MAX,
               "QEI internal direction excludes only the interval timer");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_GATE_ENABLE | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI internal position gate inhibits while QEB is low");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI internal position gate enables while QEB is high");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | (7u << QEI_DIVIDER_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 127u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI prescaler 111 waits 128 clocks");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI prescaler 111 clocks at 1 to 128");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE | (7u << 11u));
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 255u) &&
                   cpu->io.qei.filter_stability[channel][0] == 0u,
               "QEI filter divider 111 waits 256 clocks");
        expect(state,
               dspic33_device_advance(cpu, 1u) && cpu->io.qei.filter_stability[channel][0] == 1u,
               "QEI filter divider 111 samples at 1 to 256");
        expect(state,
               dspic33_device_advance(cpu, 512u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filtered input accepts after three divided samples");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) != 0u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "QEI filter and live input operate while counters are disabled");
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI counter enable uses the established filtered baseline");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "apply QEI filtered phase change");
        expect(state,
               dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_device_advance(cpu, 2u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI filter rejects before three samples");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filter accepts third stable sample");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u,
               "QEI aggregate filtered edge preserves interval chronology");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_GATE_ENABLE | QEI_MODE_TIMER);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0u,
               "QEI aggregate filter does not retroactively enable timer gates");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI filtered timer gate applies after acceptance");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               dspic33_device_advance(cpu, 5u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x12u)) == 0u,
               "QEI first count pulse arms and clears interval timer");
        expect(state,
               dspic33_device_advance(cpu, 7u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x12u)) == 7u &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u,
               "QEI second count pulse captures elapsed interval");

        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) == 7u &&
                   cpu->io.qei.interval_hold_locked[channel],
               "QEI interval hold low read locks the captured pair");
        expect(state,
               dspic33_device_advance(cpu, 6u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) == 7u,
               "QEI locked interval hold resists the next capture");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 0x14u)) == 0u &&
                   !cpu->io.qei.interval_hold_locked[channel],
               "QEI interval hold high read releases the captured pair");
        expect(state,
               dspic33_device_advance(cpu, 9u) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x12u)) == 9u,
               "QEI released interval hold accepts the next capture");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_STOP_IDLE | QEI_MODE_TIMER);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state,
               dspic33_device_advance(cpu, 4u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI stop idle freezes counters");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI resumes after idle");
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI sleep freezes counters");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
    }
}

void dspic33_qei_test_interrupt_compare_index_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t status_address = (uint16_t)(base + 4u);
        bool output;
        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_configure_interrupt(cpu, channel);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, status_address,
                           QEI_STATUS_INDEX_ENABLE | QEI_STATUS_HOME_ENABLE |
                               QEI_STATUS_POSITION_OVERFLOW_ENABLE |
                               QEI_STATUS_VELOCITY_OVERFLOW_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true),
               "apply QEI accepted index event");
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI index event sets status and module IRQ");
        expect(state,
               dspic33_device_interrupt_pending(cpu) && dspic33_device_service_interrupt(cpu) &&
                   cpu->last_interrupt == (channel == 0u ? 58u : 75u) && cpu->pc == QEI_VECTOR,
               "QEI module IRQ reaches its firmware vector");
        dspic33_device_return_interrupt(cpu);
        dspic33_write_word(cpu, status_address,
                           (uint16_t)(dspic33_read_word(cpu, status_address) & ~QEI_STATUS_INDEX));
        dspic33_qei_test_clear_interrupt(cpu, channel);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u &&
                   !dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI status flag is software clear only");
        dspic33_write_word(cpu, status_address,
                           (uint16_t)(dspic33_read_word(cpu, status_address) | QEI_STATUS_INDEX));
        expect(state, (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u,
               "QEI cleared status flag cannot be software set");
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply QEI home event");
        expect(state, (dspic33_read_word(cpu, status_address) & QEI_STATUS_HOME) != 0u,
               "QEI home event sets status");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, status_address,
                           QEI_STATUS_POSITION_OVERFLOW_ENABLE |
                               QEI_STATUS_VELOCITY_OVERFLOW_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                                       0x7fffffffu);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x7fffu);
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "apply QEI signed overflow pulse");
        expect(state,
               (dspic33_read_word(cpu, status_address) &
                (QEI_STATUS_POSITION_OVERFLOW | QEI_STATUS_VELOCITY_OVERFLOW)) ==
                   (QEI_STATUS_POSITION_OVERFLOW | QEI_STATUS_VELOCITY_OVERFLOW),
               "QEI position and velocity signed overflow flags set");
        expect(state,
               dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0x80000000u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0x8000u,
               "QEI signed counters wrap at positive limit");
        expect(state, dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI overflow sources aggregate IRQ");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                                       0x80000000u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x8000u);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0x7fffffffu &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0x7fffu,
               "QEI signed counters wrap at negative limit");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_write_word(cpu, (uint16_t)(base + 0x1eu), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x1cu), 5u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x22u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x20u), 2u);
        dspic33_write_word(cpu, status_address,
                           QEI_STATUS_LOW_COMPARE_ENABLE | QEI_STATUS_HIGH_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        expect(state, dspic33_device_advance(cpu, 5u), "advance QEI comparison boundary");
        expect(state, (dspic33_read_word(cpu, status_address) & QEI_STATUS_HIGH_COMPARE) != 0u,
               "QEI high compare status includes equal boundary");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_GREATER_EQUAL);
        expect(state, dspic33_qei_compare_output(cpu, channel, &output) && output,
               "QEI greater-equal output follows comparison");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_LESS_EQUAL);
        expect(state, dspic33_qei_compare_output(cpu, channel, &output) && !output,
               "QEI less-equal output follows comparison");
        dspic33_write_word(cpu, (uint16_t)(base + 0x1cu), 2u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x20u), 5u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_WINDOW);
        expect(state, dspic33_qei_compare_output(cpu, channel, &output) && output,
               "QEI window output is high between both bounds");
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        expect(state, dspic33_qei_compare_output(cpu, channel, &output) && !output,
               "QEI window output is low outside either bound");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (1u << QEI_POSITION_MODE_SHIFT));
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true),
               "apply QEI mode one index");
        expect(state, dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI mode one index clears position");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                                       0x12345678u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (2u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u &&
                   (dspic33_read_word(cpu, base) & (7u << QEI_POSITION_MODE_SHIFT)) == 0u,
               "QEI mode two loads IC on the next Index event");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                                       0x12345678u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INITIALIZED_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | (3u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) != 0u &&
                   (dspic33_read_word(cpu, base) & (7u << QEI_POSITION_MODE_SHIFT)) == 0u,
               "QEI mode three loads IC on the first Index after Home");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 4u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u),
                                       UINT32_MAX);
        dspic33_write_word(cpu, base, QEI_ENABLE | (5u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI mode five resets position at the greater-equal count");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 5u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 2u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (6u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 2u,
               "QEI mode six wraps the upper limit to the lower limit");

        {
            uint8_t match;
            for (match = 0u; match < 4u; match++) {
                uint8_t initial = (uint8_t)(match ^ 3u);
                dspic33_qei_test_reset_qei(cpu);
                dspic33_qei_test_set_open_comparison_window(cpu, base);
                expect(state,
                       dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A,
                                              (initial & 1u) != 0u) &&
                           dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B,
                                                  (initial & 2u) != 0u),
                       "QEI establishes the nonmatching IMV phase baseline");
                dspic33_write_word(cpu, status_address, QEI_STATUS_INDEX_ENABLE);
                dspic33_write_word(cpu, base,
                                   QEI_ENABLE | ((uint16_t)match << QEI_INDEX_MATCH_SHIFT));
                expect(state,
                       dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                           (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u &&
                           dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A,
                                                  (match & 1u) != 0u) &&
                           dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B,
                                                  (match & 2u) != 0u) &&
                           (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                           dspic33_qei_test_interrupt_set(cpu, channel),
                       "QEI IMV phase table accepts only the programmed state");
            }
        }

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INDEX_ENABLE);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | QEI_MODE_UP_DOWN | (1u << QEI_POSITION_MODE_SHIFT) |
                               (3u << QEI_INDEX_MATCH_SHIFT) | QEI_DIRECTION_INVERT);
        {
            bool index_high = dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true);
            bool phase_high = dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true);
            uint16_t status = dspic33_read_word(cpu, status_address);
            expect(state, index_high && phase_high && (status & QEI_STATUS_INDEX) == 0u,
                   "QEI asserted Index waits for the programmed phase match");
        }
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI phase transition completes an asserted Index match");
        dspic33_write_word(cpu, status_address,
                           (uint16_t)(dspic33_read_word(cpu, status_address) & ~QEI_STATUS_INDEX));
        dspic33_qei_test_clear_interrupt(cpu, channel);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u &&
                   !dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI asserted Index cannot retrigger after phase re-entry");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI deasserted Index rearms the next match event");
        dspic33_write_word(cpu, base, 0u);
        expect(state, dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, false),
               "QEI disabled Index deassertion updates the input baseline");
        dspic33_write_word(cpu, status_address,
                           (uint16_t)(dspic33_read_word(cpu, status_address) & ~QEI_STATUS_INDEX));
        dspic33_qei_test_clear_interrupt(cpu, channel);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (1u << QEI_POSITION_MODE_SHIFT) |
                               (3u << QEI_INDEX_MATCH_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   dspic33_qei_test_interrupt_set(cpu, channel),
               "QEI disabled Index deassertion rearms after enable");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 7u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_CAPTURE_HOME);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x1cu)) != 7u,
               "QEI index does not trigger position capture");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 0x1cu)) == 7u,
               "QEI Home captures position into IC alias");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_set_open_comparison_window(cpu, base);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                                       0x12345678u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INITIALIZED_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | (4u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 9u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) == 0u,
               "QEI mode four waits after first index following Home");
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, false) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) != 0u,
               "QEI mode four initializes on second index following Home");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 1u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 1u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 5u);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (6u << QEI_POSITION_MODE_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 2u,
               "QEI timer mode ignores position initialization mode");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (1u << QEI_POSITION_MODE_SHIFT) | QEI_MODE_GATE);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 9u,
               "QEI external gate mode ignores index initialization mode");

        dspic33_qei_test_reset_qei(cpu);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 2u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 2u);
        dspic33_qei_test_write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 5u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (6u << QEI_POSITION_MODE_SHIFT) | 0x0008u);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_read_counter(cpu, (uint16_t)(base + 6u)) == 5u,
               "QEI B1 modulo erratum swaps limits with inverted direction");

        dspic33_qei_test_reset_qei(cpu);
        expect(state,
               dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_qei_test_input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply persistent QEI inputs before warm reset");
        expect(state,
               dspic33_load_program_word(cpu, 0u, 0xfe0000u) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x000fu) == 0x0009u,
               "QEI warm reset preserves external input levels");
    }
}
