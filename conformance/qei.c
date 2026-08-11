#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "sfr_side_effect_coverage.h"

static const SfrSideEffectCoverage qei_sfr_side_effect_coverage[] = {
    {0x01c4u, 0x2aaau},
    {0x01ccu, 0xffffu},
    {0x05c4u, 0x2aaau},
    {0x05ccu, 0xffffu},
};

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} QeiConformance;

enum {
    QEI_ENABLE = 0x8000u,
    QEI_STOP_IDLE = 0x2000u,
    QEI_POSITION_MODE_SHIFT = 10u,
    QEI_INDEX_MATCH_SHIFT = 8u,
    QEI_DIVIDER_SHIFT = 4u,
    QEI_GATE_ENABLE = 0x0004u,
    QEI_MODE_QUADRATURE = 0u,
    QEI_MODE_UP_DOWN = 1u,
    QEI_MODE_GATE = 2u,
    QEI_MODE_TIMER = 3u,
    QEI_CAPTURE_HOME = 0x8000u,
    QEI_FILTER_ENABLE = 0x4000u,
    QEI_OUTPUT_GREATER_EQUAL = 0x0200u,
    QEI_OUTPUT_LESS_EQUAL = 0x0400u,
    QEI_OUTPUT_OUTSIDE = 0x0600u,
    QEI_SWAP = 0x0100u,
    QEI_PHASE_A_POLARITY = 0x0010u,
    QEI_STATUS_INDEX_ENABLE = 0x0001u,
    QEI_STATUS_INDEX = 0x0002u,
    QEI_STATUS_HOME_ENABLE = 0x0004u,
    QEI_STATUS_HOME = 0x0008u,
    QEI_STATUS_VELOCITY_OVERFLOW_ENABLE = 0x0010u,
    QEI_STATUS_VELOCITY_OVERFLOW = 0x0020u,
    QEI_STATUS_INITIALIZED_ENABLE = 0x0040u,
    QEI_STATUS_INITIALIZED = 0x0080u,
    QEI_STATUS_POSITION_OVERFLOW_ENABLE = 0x0100u,
    QEI_STATUS_POSITION_OVERFLOW = 0x0200u,
    QEI_STATUS_LOW_COMPARE_ENABLE = 0x0400u,
    QEI_STATUS_LOW_COMPARE = 0x0800u,
    QEI_STATUS_HIGH_COMPARE_ENABLE = 0x1000u,
    QEI_STATUS_HIGH_COMPARE = 0x2000u,
    QEI_VECTOR = 0x0240u
};

static const uint16_t bases[DSPIC33_QEI_COUNT] = {0x01c0u, 0x05c0u};
static const uint16_t pmd_addresses[DSPIC33_QEI_COUNT] = {0x0760u, 0x0764u};
static const uint16_t pmd_masks[DSPIC33_QEI_COUNT] = {0x0400u, 0x0020u};
static const uint16_t interrupt_addresses[DSPIC33_QEI_COUNT] = {0x0806u, 0x0808u};
static const uint16_t interrupt_masks[DSPIC33_QEI_COUNT] = {0x0400u, 0x0800u};

static void expect(QeiConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[qei-failed] %s\n", name);
    }
}

static uint32_t read_counter(Dspic33* cpu, uint16_t low) {
    uint32_t value = dspic33_read_word(cpu, low);
    return value | ((uint32_t)dspic33_read_word(cpu, (uint16_t)(low + 2u)) << 16u);
}

static void write_counter(Dspic33* cpu, uint16_t low, uint16_t hold, uint32_t value) {
    dspic33_write_word(cpu, hold, (uint16_t)(value >> 16u));
    dspic33_write_word(cpu, low, (uint16_t)value);
}

static bool input(Dspic33* cpu, uint8_t channel, Dspic33QeiInput source, bool high) {
    return dspic33_qei_input(cpu, channel, source, high, 0u) &&
           dspic33_device_advance(cpu, 0u);
}

static void reset_qei(Dspic33* cpu) {
    uint8_t channel;
    uint8_t source;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        for (source = 0u; source < 4u; source++) {
            dspic33_qei_input(cpu, channel, (Dspic33QeiInput)source, false, 0u);
        }
    }
    dspic33_device_advance(cpu, 0u);
    dspic33_reset(cpu, 0u);
}

static void clear_interrupt(Dspic33* cpu, uint8_t channel) {
    dspic33_write_word(cpu, interrupt_addresses[channel],
                       (uint16_t)(dspic33_read_word(cpu, interrupt_addresses[channel]) &
                                  ~interrupt_masks[channel]));
}

static bool interrupt_set(Dspic33* cpu, uint8_t channel) {
    return (dspic33_read_word(cpu, interrupt_addresses[channel]) &
            interrupt_masks[channel]) != 0u;
}

static void configure_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = channel == 0u ? 58u : 75u;
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
    cpu->program[(0x0014u + irq * 2u) / 2u] = QEI_VECTOR;
    cpu->w[15] = 0x1800u;
}

static void set_open_comparison_window(Dspic33* cpu, uint16_t base) {
    dspic33_write_word(cpu, (uint16_t)(base + 0x1eu), 0x7fffu);
    dspic33_write_word(cpu, (uint16_t)(base + 0x1cu), 0xffffu);
    dspic33_write_word(cpu, (uint16_t)(base + 0x22u), 0x8000u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x20u), 0x0000u);
}

static void register_cases(QeiConformance* state, Dspic33* cpu) {
    static const uint16_t offsets[18] = {0x00u, 0x02u, 0x04u, 0x06u, 0x08u, 0x0au,
                                         0x0cu, 0x0eu, 0x10u, 0x12u, 0x14u, 0x16u,
                                         0x18u, 0x1au, 0x1cu, 0x1eu, 0x20u, 0x22u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t index;
        reset_qei(cpu);
        for (index = 0u; index < 18u; index++) {
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + offsets[index])) == 0u,
                   "QEI register reset");
        }
        dspic33_write_word(cpu, base, UINT16_MAX);
        expect(state, dspic33_read_word(cpu, base) == 0xbf7fu,
               "QEI control access mask");
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
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u,
               "QEI direct position words read coherently");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) == 0x1234u,
               "QEI position low read captures high hold");
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                      0xa1b2c3d4u);
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 0xa1b2c3d4u,
               "QEI position hold write commits with low word");
        write_counter(cpu, (uint16_t)(base + 0x16u), (uint16_t)(base + 0x1au),
                      0x10203040u);
        expect(state,
               read_counter(cpu, (uint16_t)(base + 0x16u)) == 0x10203040u &&
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

static void quadrature_cases(QeiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_QUADRATURE);
        expect(state, input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "schedule QEI quadrature phase A rise");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, false),
               "complete QEI positive quadrature cycle");
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 4u,
               "QEI x4 quadrature positive count");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 4u,
               "QEI velocity follows quadrature count");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (read_counter(cpu, (uint16_t)(base + 0x16u)) == 1u ||
                    read_counter(cpu, (uint16_t)(base + 0x16u)) == UINT32_MAX),
               "B1 quadrature Index counter stays within its direction ambiguity");
        input(cpu, channel, DSPIC33_QEI_INDEX, false);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, false),
               "complete QEI negative quadrature cycle");
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI x4 quadrature negative count");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_SWAP);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state, input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "schedule swapped QEI input");
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX,
               "QEI input swap reverses first phase direction");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_PHASE_A_POLARITY);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) == 0u,
               "QEI polarity controls live phase status");
    }
}

static void quadrature_transition_cases(QeiConformance* state, Dspic33* cpu) {
    static const int8_t actions[16] = {0, 1, -1, 0,  -1, 0,  0, 1,
                                       1, 0, 0,  -1, 0,  -1, 1, 0};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t previous;
        for (previous = 0u; previous < 4u; previous++) {
            uint8_t current;
            for (current = 0u; current < 4u; current++) {
                int8_t action = actions[(previous << 2u) | current];
                uint32_t expected = (uint32_t)(0x100 + action);
                reset_qei(cpu);
                set_open_comparison_window(cpu, base);
                dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
                input(cpu, channel, DSPIC33_QEI_PHASE_A, (previous & 1u) != 0u);
                input(cpu, channel, DSPIC33_QEI_PHASE_B, (previous & 2u) != 0u);
                dspic33_device_advance(cpu, 3u);
                write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                              0x100u);
                dspic33_write_word(cpu, base, QEI_ENABLE);
                input(cpu, channel, DSPIC33_QEI_PHASE_A, (current & 1u) != 0u);
                input(cpu, channel, DSPIC33_QEI_PHASE_B, (current & 2u) != 0u);
                expect(state,
                       dspic33_device_advance(cpu, 3u) &&
                           read_counter(cpu, (uint16_t)(base + 6u)) == expected &&
                           dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) ==
                               (uint16_t)action,
                       "QEI quadrature truth table transition");
            }
        }
    }
}

static void divider_polarity_output_cases(QeiConformance* state, Dspic33* cpu) {
    static const uint16_t counter_divisors[8] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
    static const uint16_t filter_divisors[8] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 256u};
    static const int32_t positions[3] = {1, 3, 6};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t selection;
        for (selection = 0u; selection < 8u; selection++) {
            uint16_t divisor = counter_divisors[selection];
            reset_qei(cpu);
            set_open_comparison_window(cpu, base);
            dspic33_write_word(cpu, base,
                               (uint16_t)(QEI_ENABLE |
                                          ((uint16_t)selection << QEI_DIVIDER_SHIFT) |
                                          QEI_MODE_TIMER));
            expect(state,
                   dspic33_device_advance(cpu, (uint16_t)(divisor - 1u)) &&
                       read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
                   "QEI counter divider waits for its complete period");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
                   "QEI counter divider clocks at the selected period");

            divisor = filter_divisors[selection];
            reset_qei(cpu);
            set_open_comparison_window(cpu, base);
            dspic33_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(QEI_FILTER_ENABLE | ((uint16_t)selection << 11u)));
            dspic33_write_word(cpu, base, QEI_ENABLE);
            input(cpu, channel, DSPIC33_QEI_PHASE_A, true);
            expect(state,
                   dspic33_device_advance(cpu, (uint16_t)(3u * divisor - 1u)) &&
                       read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
                   "QEI filter divider waits for three complete samples");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
                   "QEI filter divider accepts the third stable sample");
        }

        for (selection = 0u; selection < 4u; selection++) {
            uint16_t polarity = (uint16_t)(1u << (4u + selection));
            reset_qei(cpu);
            dspic33_write_word(cpu, (uint16_t)(base + 2u), polarity);
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x000fu) ==
                       (uint16_t)(1u << selection),
                   "QEI input polarity inverts a low physical input");
            expect(state,
                   input(cpu, channel, (Dspic33QeiInput)selection, true) &&
                       (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x000fu) == 0u,
                   "QEI input polarity inverts a high physical input");
        }

        for (selection = 0u; selection < 4u; selection++) {
            uint8_t position_index;
            for (position_index = 0u; position_index < 3u; position_index++) {
                bool high;
                bool expected = (selection == 1u && positions[position_index] >= 5) ||
                                (selection == 2u && positions[position_index] <= 2) ||
                                (selection == 3u && (positions[position_index] >= 5 ||
                                                     positions[position_index] <= 2));
                reset_qei(cpu);
                write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                              5u);
                write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u),
                              2u);
                write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                              (uint32_t)positions[position_index]);
                dspic33_write_word(cpu, (uint16_t)(base + 2u),
                                   (uint16_t)(selection << 9u));
                expect(state,
                       dspic33_qei_compare_output(cpu, channel, &high) &&
                           high == expected,
                       "QEI compare output mode follows both signed boundaries");
            }
        }
    }
}

static void external_mode_cases(QeiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_UP_DOWN);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI external up direction inputs");
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI external up direction increments");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI external down direction inputs");
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI external down direction decrements");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_GATE_ENABLE | QEI_MODE_GATE);
        expect(state, input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI gated external clock while gate low");
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI low gate inhibits external clock");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI gated external clock while gate high");
        expect(state,
               read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   read_counter(cpu, (uint16_t)(base + 0x16u)) == 0u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI external mode applies independent timer gates");
        expect(state,
               dspic33_device_advance(cpu, 5u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u,
               "QEI external gate mode does not also clock from FCY");

        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 2u &&
                   read_counter(cpu, (uint16_t)(base + 0x16u)) == 1u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI external clock drives each enabled timer gate");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_GATE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   read_counter(cpu, (uint16_t)(base + 0x16u)) == 0u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI external ungated mode clocks position and velocity together");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply QEI external timer gate inputs");
        dspic33_write_word(cpu, base, QEI_ENABLE | 0x0008u | QEI_MODE_GATE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX &&
                   read_counter(cpu, (uint16_t)(base + 0x16u)) == UINT32_MAX &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == UINT16_MAX,
               "QEI external timer direction excludes only the interval timer");
    }
}

static void timer_filter_power_cases(QeiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        expect(state, dspic33_device_advance(cpu, 8u), "advance QEI internal timer");
        expect(state,
               read_counter(cpu, (uint16_t)(base + 6u)) == 8u &&
                   read_counter(cpu, (uint16_t)(base + 0x16u)) == 0u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 8u,
               "QEI internal ungated mode clocks position and velocity together");
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (2u << QEI_INDEX_MATCH_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   dspic33_device_advance(cpu, 4u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 12u &&
                   read_counter(cpu, (uint16_t)(base + 0x16u)) == 4u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 4u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 4u,
               "QEI internal mode clocks each counter through its input gate");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply QEI internal timer gate inputs");
        dspic33_write_word(cpu, base, QEI_ENABLE | 0x0008u | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == UINT32_MAX &&
                   read_counter(cpu, (uint16_t)(base + 0x16u)) == UINT32_MAX &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == UINT16_MAX,
               "QEI internal direction excludes only the interval timer");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_GATE_ENABLE | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI internal position gate inhibits while QEB is low");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI internal position gate enables while QEB is high");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (7u << QEI_DIVIDER_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 127u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI prescaler 111 waits 128 clocks");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI prescaler 111 clocks at 1 to 128");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE | (7u << 11u));
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 255u) &&
                   cpu->io.qei.filter_stability[channel][0] == 0u,
               "QEI filter divider 111 waits 256 clocks");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   cpu->io.qei.filter_stability[channel][0] == 1u,
               "QEI filter divider 111 samples at 1 to 256");
        expect(state,
               dspic33_device_advance(cpu, 512u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filtered input accepts after three divided samples");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) != 0u &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "QEI filter and live input operate while counters are disabled");
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI counter enable uses the established filtered baseline");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state, input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "apply QEI filtered phase change");
        expect(state,
               read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_device_advance(cpu, 2u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI filter rejects before three samples");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filter accepts third stable sample");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u,
               "QEI aggregate filtered edge preserves interval chronology");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_GATE_ENABLE | QEI_MODE_TIMER);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   dspic33_device_advance(cpu, 3u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0u,
               "QEI aggregate filter does not retroactively enable timer gates");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 1u,
               "QEI filtered timer gate applies after acceptance");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               dspic33_device_advance(cpu, 5u) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   read_counter(cpu, (uint16_t)(base + 0x12u)) == 0u,
               "QEI first count pulse arms and clears interval timer");
        expect(state,
               dspic33_device_advance(cpu, 7u) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   read_counter(cpu, (uint16_t)(base + 0x12u)) == 7u &&
                   read_counter(cpu, (uint16_t)(base + 0x0eu)) == 0u,
               "QEI second count pulse captures elapsed interval");

        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) == 7u &&
                   cpu->io.qei.interval_hold_locked[channel],
               "QEI interval hold low read locks the captured pair");
        expect(state,
               dspic33_device_advance(cpu, 6u) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, false) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) == 7u,
               "QEI locked interval hold resists the next capture");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 0x14u)) == 0u &&
                   !cpu->io.qei.interval_hold_locked[channel],
               "QEI interval hold high read releases the captured pair");
        expect(state,
               dspic33_device_advance(cpu, 9u) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   read_counter(cpu, (uint16_t)(base + 0x12u)) == 9u,
               "QEI released interval hold accepts the next capture");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_STOP_IDLE | QEI_MODE_TIMER);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state,
               dspic33_device_advance(cpu, 4u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI stop idle freezes counters");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI resumes after idle");
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI sleep freezes counters");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
    }
}

static void interrupt_compare_index_cases(QeiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t status_address = (uint16_t)(base + 4u);
        bool output;
        reset_qei(cpu);
        configure_interrupt(cpu, channel);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, status_address,
                           QEI_STATUS_INDEX_ENABLE | QEI_STATUS_HOME_ENABLE |
                               QEI_STATUS_POSITION_OVERFLOW_ENABLE |
                               QEI_STATUS_VELOCITY_OVERFLOW_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state, input(cpu, channel, DSPIC33_QEI_INDEX, true),
               "apply QEI accepted index event");
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   interrupt_set(cpu, channel),
               "QEI index event sets status and module IRQ");
        expect(state,
               dspic33_device_interrupt_pending(cpu) &&
                   dspic33_device_service_interrupt(cpu) &&
                   cpu->last_interrupt == (channel == 0u ? 58u : 75u) &&
                   cpu->pc == QEI_VECTOR,
               "QEI module IRQ reaches its firmware vector");
        dspic33_device_return_interrupt(cpu);
        dspic33_write_word(
            cpu, status_address,
            (uint16_t)(dspic33_read_word(cpu, status_address) & ~QEI_STATUS_INDEX));
        clear_interrupt(cpu, channel);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u &&
                   !interrupt_set(cpu, channel),
               "QEI status flag is software clear only");
        dspic33_write_word(
            cpu, status_address,
            (uint16_t)(dspic33_read_word(cpu, status_address) | QEI_STATUS_INDEX));
        expect(state, (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u,
               "QEI cleared status flag cannot be software set");
        expect(state, input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply QEI home event");
        expect(state, (dspic33_read_word(cpu, status_address) & QEI_STATUS_HOME) != 0u,
               "QEI home event sets status");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, status_address,
                           QEI_STATUS_POSITION_OVERFLOW_ENABLE |
                               QEI_STATUS_VELOCITY_OVERFLOW_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                      0x7fffffffu);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x7fffu);
        expect(state, input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "apply QEI signed overflow pulse");
        expect(state,
               (dspic33_read_word(cpu, status_address) &
                (QEI_STATUS_POSITION_OVERFLOW | QEI_STATUS_VELOCITY_OVERFLOW)) ==
                   (QEI_STATUS_POSITION_OVERFLOW | QEI_STATUS_VELOCITY_OVERFLOW),
               "QEI position and velocity signed overflow flags set");
        expect(state,
               read_counter(cpu, (uint16_t)(base + 6u)) == 0x80000000u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0x8000u,
               "QEI signed counters wrap at positive limit");
        expect(state, interrupt_set(cpu, channel),
               "QEI overflow sources aggregate IRQ");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au),
                      0x80000000u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x8000u);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0x7fffffffu &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0x7fffu,
               "QEI signed counters wrap at negative limit");

        reset_qei(cpu);
        dspic33_write_word(cpu, (uint16_t)(base + 0x1eu), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x1cu), 5u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x22u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x20u), 2u);
        dspic33_write_word(cpu, status_address,
                           QEI_STATUS_LOW_COMPARE_ENABLE |
                               QEI_STATUS_HIGH_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        expect(state, dspic33_device_advance(cpu, 5u),
               "advance QEI comparison boundary");
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_HIGH_COMPARE) != 0u,
               "QEI high compare status includes equal boundary");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_GREATER_EQUAL);
        expect(state, dspic33_qei_compare_output(cpu, channel, &output) && output,
               "QEI greater-equal output follows comparison");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_LESS_EQUAL);
        expect(state, dspic33_qei_compare_output(cpu, channel, &output) && !output,
               "QEI less-equal output follows comparison");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_OUTPUT_OUTSIDE);
        expect(state, dspic33_qei_compare_output(cpu, channel, &output) && output,
               "QEI combined comparison output follows either bound");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (1u << QEI_POSITION_MODE_SHIFT));
        expect(state, input(cpu, channel, DSPIC33_QEI_INDEX, true),
               "apply QEI mode one index");
        expect(state, read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI mode one index clears position");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                      0x12345678u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (2u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u &&
                   (dspic33_read_word(cpu, base) & (7u << QEI_POSITION_MODE_SHIFT)) ==
                       0u,
               "QEI mode two loads IC on the next Index event");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                      0x12345678u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INITIALIZED_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | (3u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) !=
                       0u &&
                   (dspic33_read_word(cpu, base) & (7u << QEI_POSITION_MODE_SHIFT)) ==
                       0u,
               "QEI mode three loads IC on the first Index after Home");

        reset_qei(cpu);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 4u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u),
                      UINT32_MAX);
        dspic33_write_word(cpu, base, QEI_ENABLE | (5u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI mode five resets position at the greater-equal count");

        reset_qei(cpu);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 5u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 2u);
        dspic33_write_word(cpu, base, QEI_ENABLE | (6u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 2u,
               "QEI mode six wraps the upper limit to the lower limit");

        {
            uint8_t match;
            for (match = 0u; match < 4u; match++) {
                uint8_t initial = (uint8_t)(match ^ 3u);
                reset_qei(cpu);
                set_open_comparison_window(cpu, base);
                expect(
                    state,
                    input(cpu, channel, DSPIC33_QEI_PHASE_A, (initial & 1u) != 0u) &&
                        input(cpu, channel, DSPIC33_QEI_PHASE_B, (initial & 2u) != 0u),
                    "QEI establishes the nonmatching IMV phase baseline");
                dspic33_write_word(cpu, status_address, QEI_STATUS_INDEX_ENABLE);
                dspic33_write_word(
                    cpu, base, QEI_ENABLE | ((uint16_t)match << QEI_INDEX_MATCH_SHIFT));
                expect(
                    state,
                    input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                        (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) ==
                            0u &&
                        input(cpu, channel, DSPIC33_QEI_PHASE_A, (match & 1u) != 0u) &&
                        input(cpu, channel, DSPIC33_QEI_PHASE_B, (match & 2u) != 0u) &&
                        (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) !=
                            0u &&
                        interrupt_set(cpu, channel),
                    "QEI IMV phase table accepts only the programmed state");
            }
        }

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INDEX_ENABLE);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (1u << QEI_POSITION_MODE_SHIFT) |
                               (3u << QEI_INDEX_MATCH_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 10u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u,
               "QEI asserted Index waits for the programmed phase match");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   interrupt_set(cpu, channel),
               "QEI phase transition completes an asserted Index match");
        dspic33_write_word(
            cpu, status_address,
            (uint16_t)(dspic33_read_word(cpu, status_address) & ~QEI_STATUS_INDEX));
        clear_interrupt(cpu, channel);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, false) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) == 0u &&
                   !interrupt_set(cpu, channel),
               "QEI asserted Index cannot retrigger after phase re-entry");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, false) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   interrupt_set(cpu, channel),
               "QEI deasserted Index rearms the next match event");
        dspic33_write_word(cpu, base, 0u);
        expect(state, input(cpu, channel, DSPIC33_QEI_INDEX, false),
               "QEI disabled Index deassertion updates the input baseline");
        dspic33_write_word(
            cpu, status_address,
            (uint16_t)(dspic33_read_word(cpu, status_address) & ~QEI_STATUS_INDEX));
        clear_interrupt(cpu, channel);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (1u << QEI_POSITION_MODE_SHIFT) |
                               (3u << QEI_INDEX_MATCH_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INDEX) != 0u &&
                   interrupt_set(cpu, channel),
               "QEI disabled Index deassertion rearms after enable");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 7u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_CAPTURE_HOME);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   read_counter(cpu, (uint16_t)(base + 0x1cu)) != 7u,
               "QEI index does not trigger position capture");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   read_counter(cpu, (uint16_t)(base + 0x1cu)) == 7u,
               "QEI Home captures position into IC alias");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                      0x12345678u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INITIALIZED_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | (4u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 9u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) ==
                       0u,
               "QEI mode four waits after first index following Home");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, false) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0x12345678u &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) !=
                       0u,
               "QEI mode four initializes on second index following Home");

        reset_qei(cpu);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 1u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 1u);
        write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 5u);
        dspic33_write_word(
            cpu, base, QEI_ENABLE | (6u << QEI_POSITION_MODE_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 2u,
               "QEI timer mode ignores position initialization mode");

        reset_qei(cpu);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 9u);
        dspic33_write_word(
            cpu, base, QEI_ENABLE | (1u << QEI_POSITION_MODE_SHIFT) | QEI_MODE_GATE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 9u,
               "QEI external gate mode ignores index initialization mode");

        reset_qei(cpu);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 2u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 2u);
        write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 5u);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (6u << QEI_POSITION_MODE_SHIFT) | 0x0008u);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 5u,
               "QEI B1 modulo erratum swaps limits with inverted direction");

        reset_qei(cpu);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   input(cpu, channel, DSPIC33_QEI_HOME, true),
               "apply persistent QEI inputs before warm reset");
        expect(state,
               dspic33_load_program_word(cpu, 0u, 0xfe0000u) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x000fu) == 0x0009u,
               "QEI warm reset preserves external input levels");
    }
}

static void compare_refresh_cases(QeiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t status_address = (uint16_t)(base + 4u);
        reset_qei(cpu);
        configure_interrupt(cpu, channel);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 5u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 2u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_HIGH_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_HIGH_COMPARE) !=
                       0u &&
                   interrupt_set(cpu, channel),
               "QEI enable refreshes the active high comparison");

        reset_qei(cpu);
        configure_interrupt(cpu, channel);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 0u);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                      0x00010000u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_HIGH_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        clear_interrupt(cpu, channel);
        dspic33_write_word(cpu, status_address,
                           (uint16_t)(dspic33_read_word(cpu, status_address) &
                                      ~QEI_STATUS_HIGH_COMPARE));
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 1u);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_HIGH_COMPARE) !=
                       0u &&
                   interrupt_set(cpu, channel),
               "QEI position high write refreshes the active comparison");

        reset_qei(cpu);
        configure_interrupt(cpu, channel);
        write_counter(cpu, (uint16_t)(base + 6u), (uint16_t)(base + 0x0au), 2u);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu), 5u);
        write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u), 2u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_LOW_COMPARE_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               (dspic33_read_word(cpu, status_address) & QEI_STATUS_LOW_COMPARE) !=
                       0u &&
                   interrupt_set(cpu, channel),
               "QEI low comparison raises its enabled interrupt");

        reset_qei(cpu);
        configure_interrupt(cpu, channel);
        set_open_comparison_window(cpu, base);
        write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                      0x12345678u);
        dspic33_write_word(cpu, status_address, QEI_STATUS_INITIALIZED_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE | (3u << QEI_POSITION_MODE_SHIFT));
        expect(state,
               input(cpu, channel, DSPIC33_QEI_HOME, true) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, status_address) & QEI_STATUS_INITIALIZED) !=
                       0u &&
                   interrupt_set(cpu, channel),
               "QEI completed homing initialization raises its enabled interrupt");
    }
}

static void power_lifecycle_cases(QeiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 3u,
               "QEI continues its internal timer in Idle when enabled");
        cpu->power_state = DSPIC33_POWER_ACTIVE;

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI ignores external count edges in Sleep");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI resumes from the physical input level after Sleep");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   dspic33_device_advance(cpu, 10u) &&
                   cpu->io.qei.filter_stability[channel][0] == 0u,
               "QEI filter clock stops in Sleep");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filter restarts after Sleep");

        reset_qei(cpu);
        input(cpu, channel, DSPIC33_QEI_HOME, true);
        dspic33_reset(cpu, 0u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 0x0008u) != 0u,
               "QEI cold reset preserves the physical input level");
        input(cpu, channel, DSPIC33_QEI_HOME, false);
    }

    {
        bool high;
        expect(
            state,
            !dspic33_qei_input(cpu, DSPIC33_QEI_COUNT, DSPIC33_QEI_PHASE_A, true, 0u) &&
                !dspic33_qei_input(cpu, 0u, (Dspic33QeiInput)4u, true, 0u) &&
                !dspic33_qei_compare_output(cpu, DSPIC33_QEI_COUNT, &high) &&
                !dspic33_qei_compare_output(cpu, 0u, NULL),
            "QEI public APIs reject invalid arguments");
    }
}

static void large_timer_advance_cases(QeiConformance* state, Dspic33* cpu) {
    static const uint16_t status_mask =
        QEI_STATUS_VELOCITY_OVERFLOW | QEI_STATUS_POSITION_OVERFLOW |
        QEI_STATUS_LOW_COMPARE | QEI_STATUS_HIGH_COMPARE;
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t inverted;
        for (inverted = 0u; inverted < 2u; inverted++) {
            uint32_t expected = inverted != 0u ? 0xfffffffcu : 4u;
            uint16_t expected_velocity = inverted != 0u ? 0xfffcu : 4u;
            reset_qei(cpu);
            write_counter(cpu, (uint16_t)(base + 0x1cu), (uint16_t)(base + 0x1eu),
                          100u);
            write_counter(cpu, (uint16_t)(base + 0x20u), (uint16_t)(base + 0x22u),
                          0xffffff9cu);
            input(cpu, channel, DSPIC33_QEI_INDEX, true);
            input(cpu, channel, DSPIC33_QEI_HOME, true);
            dspic33_write_word(cpu, base,
                               (uint16_t)(QEI_ENABLE | QEI_MODE_TIMER |
                                          (inverted != 0u ? 0x0008u : 0u)));
            expect(state,
                   dspic33_device_advance(cpu, (uint64_t)UINT32_MAX + 5u) &&
                       read_counter(cpu, (uint16_t)(base + 6u)) == expected &&
                       dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) ==
                           expected_velocity &&
                       read_counter(cpu, (uint16_t)(base + 0x0eu)) == 4u &&
                       read_counter(cpu, (uint16_t)(base + 0x16u)) == expected,
                   "QEI large timer advance preserves modular counter results");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & status_mask) ==
                       status_mask,
                   "QEI large timer advance preserves crossed status events");
        }
    }
}

static void pmd_cases(QeiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        uint16_t base = bases[channel];
        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 3u,
               "QEI advances before PMD");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               !cpu->io.qei.pmd_disabled[channel] &&
                   dspic33_read_word(cpu, base) == (QEI_ENABLE | QEI_MODE_TIMER),
               "QEI PMD set is delayed one cycle");
        expect(state,
               dspic33_device_advance(cpu, 1u) && cpu->io.qei.pmd_disabled[channel] &&
                   dspic33_read_word(cpu, base) == 0u,
               "QEI PMD disables after one enabled cycle");
        dspic33_write_word(cpu, base, 0u);
        expect(state,
               dspic33_device_advance(cpu, 5u) && dspic33_read_word(cpu, base) == 0u,
               "QEI PMD blocks register writes and counter progress");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) && !cpu->io.qei.pmd_disabled[channel] &&
                   dspic33_read_word(cpu, base) == (QEI_ENABLE | QEI_MODE_TIMER),
               "QEI PMD clear enables after one disabled cycle");
        expect(state,
               read_counter(cpu, (uint16_t)(base + 6u)) == 4u &&
                   dspic33_device_advance(cpu, 2u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 6u,
               "QEI PMD preserves and resumes counter state");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base,
                           QEI_ENABLE | (2u << QEI_DIVIDER_SHIFT) | QEI_MODE_TIMER);
        expect(state,
               dspic33_device_advance(cpu, 2u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI accumulates partial prescaler phase before PMD");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) && cpu->io.qei.pmd_disabled[channel],
               "QEI PMD transition preserves partial prescaler phase");
        expect(state,
               dspic33_device_advance(cpu, 7u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD freezes partial prescaler phase");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) && !cpu->io.qei.pmd_disabled[channel] &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD clear does not consume prescaler phase");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI resumes after the exact remaining prescaler phase");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), QEI_STATUS_INDEX_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        expect(state,
               input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & QEI_STATUS_INDEX) !=
                       0u,
               "QEI Index match latches before PMD");
        dspic33_write_word(cpu, (uint16_t)(base + 4u),
                           (uint16_t)(dspic33_read_word(cpu, (uint16_t)(base + 4u)) &
                                      ~QEI_STATUS_INDEX));
        clear_interrupt(cpu, channel);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, false),
               "QEI PMD Index deassertion updates the external level");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   input(cpu, channel, DSPIC33_QEI_INDEX, true) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & QEI_STATUS_INDEX) !=
                       0u &&
                   interrupt_set(cpu, channel),
               "QEI PMD Index deassertion rearms after resume");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD blocks bypassed input changes");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & 1u) != 0u,
               "QEI PMD resume synchronizes bypassed input without an edge");
        expect(state,
               input(cpu, channel, DSPIC33_QEI_PHASE_B, true) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI bypassed input counts the next physical edge");

        reset_qei(cpu);
        set_open_comparison_window(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), QEI_FILTER_ENABLE);
        dspic33_write_word(cpu, base, QEI_ENABLE);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   input(cpu, channel, DSPIC33_QEI_PHASE_A, true),
               "QEI PMD blocks filtered input changes");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u,
               "QEI PMD resume does not bypass the input filter");
        expect(state,
               dspic33_device_advance(cpu, 2u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_device_advance(cpu, 1u) &&
                   read_counter(cpu, (uint16_t)(base + 6u)) == 1u,
               "QEI filtered input resumes after three stable samples");

        reset_qei(cpu);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) && !cpu->io.qei.pmd_disabled[channel],
               "QEI PMD generation ignores stale transitions");

        {
            uint64_t device_cycles = cpu->device_cycles;
            size_t queued = cpu->events.count;
            cpu->device_cycles = UINT64_MAX;
            expect(state,
                   !dspic33_qei_input(cpu, channel, DSPIC33_QEI_PHASE_A, true, 1u) &&
                       cpu->events.count == queued,
                   "QEI input scheduling failure queues no partial event");
            cpu->device_cycles = device_cycles;
        }

        {
            uint64_t device_cycles = cpu->device_cycles;
            uint16_t generation = cpu->io.qei.pmd_generation[channel];
            bool disabled = cpu->io.qei.pmd_disabled[channel];
            size_t queued = cpu->events.count;
            uint16_t pmd = dspic33_read_word(cpu, pmd_addresses[channel]);
            cpu->device_cycles = UINT64_MAX;
            dspic33_write_word(cpu, pmd_addresses[channel],
                               (uint16_t)(pmd | pmd_masks[channel]));
            expect(state,
                   dspic33_read_word(cpu, pmd_addresses[channel]) == pmd &&
                       cpu->io.qei.pmd_generation[channel] ==
                           (uint16_t)(generation + 2u) &&
                       cpu->io.qei.pmd_disabled[channel] == disabled &&
                       cpu->events.count == queued &&
                       cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
                   "QEI PMD scheduling failure rolls back and invalidates the event");
            cpu->device_cycles = device_cycles;
            cpu->stop_reason = DSPIC33_RUNNING;
        }
    }
}

static void copy_cases(QeiConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize QEI copy destination");
    if (!initialized) {
        return;
    }
    reset_qei(cpu);
    dspic33_reset(&copy, 0u);
    expect(state,
           dspic33_qei_input(cpu, 0u, DSPIC33_QEI_PHASE_A, true, 2u) &&
               dspic33_qei_input(cpu, 1u, DSPIC33_QEI_HOME, true, 3u) &&
               dspic33_copy(&copy, cpu),
           "copy QEI state with pending input events");
    expect(state,
           dspic33_device_advance(&copy, 3u) && (copy.qei_inputs[0] & 1u) != 0u &&
               (copy.qei_inputs[1] & 8u) != 0u && cpu->qei_inputs[0] == 0u &&
               cpu->qei_inputs[1] == 0u && cpu->events.count == 2u,
           "QEI copied events execute independently");

    reset_qei(cpu);
    set_open_comparison_window(cpu, bases[0]);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 2u),
                       (uint16_t)(QEI_FILTER_ENABLE | (1u << 11u)));
    dspic33_write_word(cpu, bases[0],
                       QEI_ENABLE | (2u << QEI_DIVIDER_SHIFT) | QEI_MODE_TIMER);
    input(cpu, 0u, DSPIC33_QEI_PHASE_A, true);
    expect(state,
           dspic33_device_advance(cpu, 2u) && cpu->io.qei.counter_fraction[0] == 2u &&
               cpu->io.qei.filter_stability[0][0] == 1u && dspic33_copy(&copy, cpu),
           "copy partial QEI counter and filter phases");
    expect(state,
           dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u) &&
               read_counter(cpu, (uint16_t)(bases[0] + 6u)) == 1u &&
               read_counter(&copy, (uint16_t)(bases[0] + 6u)) == 1u &&
               cpu->io.qei.filter_stability[0][0] == 2u &&
               copy.io.qei.filter_stability[0][0] == 2u,
           "copied QEI phases resume identically");
    expect(state,
           input(cpu, 0u, DSPIC33_QEI_PHASE_A, false) &&
               dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u) &&
               (cpu->io.qei.filtered_inputs[0] & 1u) == 0u &&
               (copy.io.qei.filtered_inputs[0] & 1u) != 0u,
           "copied QEI physical and filter state diverge independently");

    reset_qei(cpu);
    dspic33_write_word(cpu, pmd_addresses[0], pmd_masks[0]);
    expect(state, dspic33_copy(&copy, cpu), "copy pending QEI PMD transition");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u) &&
               cpu->io.qei.pmd_disabled[0] && copy.io.qei.pmd_disabled[0],
           "copied QEI PMD transition completes independently");
    dspic33_destroy(&copy);
}

int main(void) {
    Dspic33 cpu;
    QeiConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize QEI processor");
    if (initialized) {
        register_cases(&state, &cpu);
        quadrature_cases(&state, &cpu);
        quadrature_transition_cases(&state, &cpu);
        divider_polarity_output_cases(&state, &cpu);
        external_mode_cases(&state, &cpu);
        timer_filter_power_cases(&state, &cpu);
        interrupt_compare_index_cases(&state, &cpu);
        compare_refresh_cases(&state, &cpu);
        power_lifecycle_cases(&state, &cpu);
        large_timer_advance_cases(&state, &cpu);
        pmd_cases(&state, &cpu);
        copy_cases(&state, &cpu);
        expect(&state, state.cases == 472u, "QEI assertion accounting");
        dspic33_destroy(&cpu);
    }
    report_sfr_side_effect_coverage(
        "qei", qei_sfr_side_effect_coverage,
        SFR_SIDE_EFFECT_COVERAGE_COUNT(qei_sfr_side_effect_coverage),
        state.failed == 0u);
    printf("[qei-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
