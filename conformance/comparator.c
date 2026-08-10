#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} ComparatorConformance;

enum {
    COMPARATOR_STATUS = 0x0a80u,
    COMPARATOR_REFERENCE = 0x0a82u,
    COMPARATOR_BASE = 0x0a84u,
    COMPARATOR_STRIDE = 0x0008u,
    COMPARATOR_ENABLE = 0x8000u,
    COMPARATOR_OUTPUT_ENABLE = 0x4000u,
    COMPARATOR_POLARITY = 0x2000u,
    COMPARATOR_EVENT = 0x0200u,
    COMPARATOR_OUTPUT = 0x0100u,
    COMPARATOR_STOP_IDLE = 0x8000u,
    COMPARATOR_PMD_ADDRESS = 0x0764u,
    COMPARATOR_PMD = 0x0400u,
    COMPARATOR_IRQ = 18u,
    COMPARATOR_FLAG_ADDRESS = 0x0802u,
    COMPARATOR_ENABLE_ADDRESS = 0x0822u,
    COMPARATOR_PRIORITY_ADDRESS = 0x0848u,
    COMPARATOR_INTERRUPT_BIT = 0x0004u,
    COMPARATOR_VECTOR = 0x0240u
};

static const uint16_t register_addresses[14] = {
    0x0a80u, 0x0a82u, 0x0a84u, 0x0a86u, 0x0a88u, 0x0a8au, 0x0a8cu,
    0x0a8eu, 0x0a90u, 0x0a92u, 0x0a94u, 0x0a96u, 0x0a98u, 0x0a9au};

static const uint16_t register_writable[14] = {
    0x8000u, 0x07ffu, 0xe2d3u, 0x0fffu, 0xbfffu, 0x007fu, 0xe2d3u,
    0x0fffu, 0xbfffu, 0x007fu, 0xe2d3u, 0x0fffu, 0xbfffu, 0x007fu};

static const Dspic33ComparatorInput negative_inputs[3] = {
    DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, DSPIC33_COMPARATOR_INPUT_NEGATIVE_1,
    DSPIC33_COMPARATOR_INPUT_NEGATIVE_3};

static void expect(ComparatorConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[comparator-failed] %s\n", name);
    }
}

static uint16_t comparator_base(uint8_t comparator) {
    return (uint16_t)(COMPARATOR_BASE + comparator * COMPARATOR_STRIDE);
}

static bool interrupt_flag(Dspic33* cpu) {
    return (dspic33_read_word(cpu, COMPARATOR_FLAG_ADDRESS) &
            COMPARATOR_INTERRUPT_BIT) != 0u;
}

static void clear_interrupt(Dspic33* cpu) {
    dspic33_write_word(cpu, COMPARATOR_FLAG_ADDRESS,
                       (uint16_t)(dspic33_read_word(cpu, COMPARATOR_FLAG_ADDRESS) &
                                  ~COMPARATOR_INTERRUPT_BIT));
}

static void clear_event(Dspic33* cpu, uint8_t comparator) {
    uint16_t base = comparator_base(comparator);
    dspic33_write_word(cpu, base,
                       (uint16_t)(dspic33_read_word(cpu, base) & ~COMPARATOR_EVENT));
}

static bool output_is(const Dspic33* cpu, uint8_t comparator, bool expected) {
    bool high;
    return dspic33_comparator_output(cpu, comparator, &high) && high == expected;
}

static bool pin_is(const Dspic33* cpu, uint8_t pin, bool expected) {
    bool high;
    return dspic33_comparator_pin(cpu, pin, &high) && high == expected;
}

static bool status_output(Dspic33* cpu, uint8_t comparator) {
    return (dspic33_read_word(cpu, COMPARATOR_STATUS) & (uint16_t)(1u << comparator)) !=
           0u;
}

static bool status_event(Dspic33* cpu, uint8_t comparator) {
    return (dspic33_read_word(cpu, COMPARATOR_STATUS) &
            (uint16_t)(0x0100u << comparator)) != 0u;
}

static bool prepare_relation(Dspic33* cpu, uint8_t comparator,
                             Dspic33ComparatorInput negative, uint16_t positive,
                             uint16_t negative_level) {
    return dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                    positive, 0u) &&
           dspic33_comparator_input(cpu, comparator, negative, negative_level, 0u) &&
           dspic33_device_advance(cpu, 0u);
}

static void configure_comparator(Dspic33* cpu, uint8_t comparator, uint16_t channel,
                                 bool inverted, uint16_t event_polarity) {
    uint16_t control = (uint16_t)(COMPARATOR_ENABLE | channel | event_polarity |
                                  (inverted ? COMPARATOR_POLARITY : 0u));
    dspic33_write_word(cpu, comparator_base(comparator), control);
}

static void configure_interrupt(Dspic33* cpu) {
    dspic33_write_word(cpu, COMPARATOR_ENABLE_ADDRESS,
                       (uint16_t)(dspic33_read_word(cpu, COMPARATOR_ENABLE_ADDRESS) |
                                  COMPARATOR_INTERRUPT_BIT));
    dspic33_write_word(
        cpu, COMPARATOR_PRIORITY_ADDRESS,
        (uint16_t)((dspic33_read_word(cpu, COMPARATOR_PRIORITY_ADDRESS) & 0xf8ffu) |
                   0x0300u));
    cpu->program[(0x0014u + COMPARATOR_IRQ * 2u) / 2u] = COMPARATOR_VECTOR;
    cpu->w[15] = 0x1800u;
}

static void access_cases(ComparatorConformance* state, Dspic33* cpu) {
    uint8_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < 14u; index++) {
        uint16_t address = register_addresses[index];
        expect(state, dspic33_read_word(cpu, address) == 0u,
               "comparator register reset");
        dspic33_write_word(cpu, address, UINT16_MAX);
        expect(state, dspic33_read_word(cpu, address) == register_writable[index],
               "comparator register access mask");
        dspic33_write_word(cpu, address, 0u);
        expect(state, dspic33_read_word(cpu, address) == 0u,
               "comparator register clears writable bits");
    }
}

static void selection_cases(ComparatorConformance* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint8_t channel;
        for (channel = 0u; channel < 3u; channel++) {
            uint8_t inverted;
            for (inverted = 0u; inverted < 2u; inverted++) {
                dspic33_reset(cpu, 0u);
                expect(state,
                       prepare_relation(cpu, comparator, negative_inputs[channel], 100u,
                                        150u),
                       "schedule selected comparator inputs");
                configure_comparator(cpu, comparator, channel, inverted != 0u, 0u);
                expect(state, output_is(cpu, comparator, inverted != 0u),
                       "selected comparator input controls output polarity");
                expect(state, status_output(cpu, comparator) == (inverted != 0u),
                       "CMSTAT mirrors selected comparator output");
            }
        }
    }
}

static void event_polarity_cases(ComparatorConformance* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint8_t inverted;
        for (inverted = 0u; inverted < 2u; inverted++) {
            uint8_t polarity;
            for (polarity = 0u; polarity < 4u; polarity++) {
                bool rise_event = polarity == 3u ||
                                  (polarity == 1u && inverted == 0u) ||
                                  (polarity == 2u && inverted != 0u);
                bool fall_event = polarity == 3u ||
                                  (polarity == 2u && inverted == 0u) ||
                                  (polarity == 1u && inverted != 0u);
                uint16_t event_polarity = (uint16_t)(polarity << 6u);
                dspic33_reset(cpu, 0u);
                expect(state,
                       prepare_relation(cpu, comparator,
                                        DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u),
                       "prepare comparator event baseline");
                configure_comparator(cpu, comparator, 0u, inverted != 0u,
                                     event_polarity);
                clear_event(cpu, comparator);
                clear_interrupt(cpu);
                dspic33_device_advance(cpu, 1u);
                expect(state,
                       output_is(cpu, comparator, inverted != 0u) &&
                           !status_event(cpu, comparator) && !interrupt_flag(cpu),
                       "comparator event baseline is clear");
                expect(state,
                       dspic33_comparator_input(cpu, comparator,
                                                DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u,
                                                0u) &&
                           dspic33_device_advance(cpu, 0u),
                       "apply comparator rising relation");
                expect(state,
                       status_event(cpu, comparator) == rise_event &&
                           interrupt_flag(cpu) == rise_event,
                       "rising comparator relation follows EVPOL");
                clear_event(cpu, comparator);
                clear_interrupt(cpu);
                expect(state,
                       dspic33_device_advance(cpu, 1u) &&
                           !status_event(cpu, comparator) && !interrupt_flag(cpu),
                       "comparator event rearms after one cycle");
                expect(state,
                       dspic33_comparator_input(cpu, comparator,
                                                DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u,
                                                0u) &&
                           dspic33_device_advance(cpu, 0u) &&
                           status_event(cpu, comparator) == fall_event &&
                           interrupt_flag(cpu) == fall_event,
                       "falling comparator relation follows EVPOL");
            }
        }
    }
}

static void sticky_rearm_cases(ComparatorConformance* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        dspic33_reset(cpu, 0u);
        expect(state,
               prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2,
                                0u, 100u),
               "prepare sticky comparator event");
        configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
        clear_event(cpu, comparator);
        clear_interrupt(cpu);
        dspic33_device_advance(cpu, 1u);
        expect(state,
               dspic33_comparator_input(cpu, comparator,
                                        DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u),
               "schedule first sticky comparator edge");
        expect(state, dspic33_device_advance(cpu, 0u),
               "advance first sticky comparator edge");
        expect(state, status_event(cpu, comparator) && interrupt_flag(cpu),
               "first comparator edge sets sticky event");
        clear_interrupt(cpu);
        expect(state,
               dspic33_comparator_input(cpu, comparator,
                                        DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "apply blocked comparator edge");
        expect(state,
               status_event(cpu, comparator) && !interrupt_flag(cpu) &&
                   output_is(cpu, comparator, false),
               "sticky event blocks later comparator interrupt");
        clear_event(cpu, comparator);
        clear_interrupt(cpu);
        expect(state,
               dspic33_comparator_input(cpu, comparator,
                                        DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "apply edge during comparator rearm delay");
        expect(state, !status_event(cpu, comparator) && !interrupt_flag(cpu),
               "rearm delay suppresses immediate comparator edge");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_comparator_input(
                       cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
                   dspic33_device_advance(cpu, 0u) && status_event(cpu, comparator) &&
                   interrupt_flag(cpu),
               "comparator edge fires after rearm cycle");
    }
}

static bool trigger_unread_rising_event(Dspic33* cpu, uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    if (!prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u,
                          100u)) {
        return false;
    }
    configure_comparator(cpu, comparator, 0u, false, 0x0040u);
    clear_event(cpu, comparator);
    clear_interrupt(cpu);
    if (!dspic33_device_advance(cpu, 1u)) {
        return false;
    }
    return dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                    200u, 0u) &&
           dspic33_device_advance(cpu, 0u) && status_event(cpu, comparator) &&
           interrupt_flag(cpu);
}

static void last_read_cout_cases(ComparatorConformance* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint16_t base = comparator_base(comparator);
        bool prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        clear_interrupt(cpu);
        expect(state,
               prepared && !status_event(cpu, comparator) &&
                   dspic33_device_advance(cpu, 1u) && status_event(cpu, comparator) &&
                   interrupt_flag(cpu),
               "literal CEVT clear retriggers from unread COUT mismatch");

        prepared = trigger_unread_rising_event(cpu, comparator);
        uint16_t control = dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        clear_interrupt(cpu);
        expect(state,
               prepared && (control & COMPARATOR_OUTPUT) != 0u &&
                   dspic33_device_advance(cpu, 1u) && !status_event(cpu, comparator) &&
                   !interrupt_flag(cpu),
               "word COUT read suppresses retained mismatch");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        clear_interrupt(cpu);
        uint8_t high = dspic33_read_byte(cpu, (uint16_t)(base + 1u));
        expect(state,
               prepared && (high & 1u) != 0u && dspic33_device_advance(cpu, 1u) &&
                   !status_event(cpu, comparator) && !interrupt_flag(cpu),
               "high-byte COUT read during rearm suppresses mismatch");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_read_byte(cpu, base);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        clear_interrupt(cpu);
        expect(state,
               prepared && dspic33_device_advance(cpu, 1u) &&
                   status_event(cpu, comparator) && interrupt_flag(cpu),
               "low-byte CMxCON read does not acknowledge COUT");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_read_word(cpu, COMPARATOR_STATUS);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        clear_interrupt(cpu);
        expect(state,
               prepared && dspic33_device_advance(cpu, 1u) &&
                   status_event(cpu, comparator) && interrupt_flag(cpu),
               "CMSTAT read does not acknowledge COUT");

        prepared = trigger_unread_rising_event(cpu, comparator);
        output_is(cpu, comparator, true);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        clear_interrupt(cpu);
        expect(state,
               prepared && dspic33_device_advance(cpu, 1u) &&
                   status_event(cpu, comparator) && interrupt_flag(cpu),
               "logical output query does not acknowledge COUT");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_read_byte(cpu, (uint16_t)(base + 1u));
        expect(state,
               prepared && status_event(cpu, comparator) && interrupt_flag(cpu) &&
                   (cpu->io.comparator.last_read_cout & (uint8_t)(1u << comparator)) !=
                       0u,
               "sticky CEVT survives COUT baseline refresh");

        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        clear_interrupt(cpu);
        expect(state,
               dspic33_device_advance(cpu, 1u) && !status_event(cpu, comparator) &&
                   !interrupt_flag(cpu) &&
                   (cpu->io.comparator.last_read_cout & (uint8_t)(1u << comparator)) !=
                       0u,
               "CMxCON disable preserves last-read COUT baseline");
    }
}

static void software_event_cases(ComparatorConformance* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint16_t base = comparator_base(comparator);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | COMPARATOR_EVENT);
        expect(state,
               (dspic33_read_word(cpu, base) & COMPARATOR_EVENT) != 0u &&
                   status_event(cpu, comparator),
               "software CEVT sets register and CMSTAT mirror");
        expect(state, interrupt_flag(cpu), "software CEVT raises combined interrupt");
        clear_interrupt(cpu);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | COMPARATOR_EVENT);
        expect(state, !interrupt_flag(cpu), "set CEVT does not refire while sticky");
        dspic33_write_word(cpu, COMPARATOR_STATUS, 0u);
        expect(state, status_event(cpu, comparator), "CMSTAT cannot clear CEVT mirror");
        clear_event(cpu, comparator);
        expect(state,
               (dspic33_read_word(cpu, base) & COMPARATOR_EVENT) == 0u &&
                   !status_event(cpu, comparator),
               "CMxCON clears software CEVT");
    }

    dspic33_reset(cpu, 0u);
    configure_interrupt(cpu);
    dspic33_write_word(cpu, comparator_base(0u), COMPARATOR_ENABLE | COMPARATOR_EVENT);
    dspic33_write_word(cpu, comparator_base(2u), COMPARATOR_ENABLE | COMPARATOR_EVENT);
    expect(state, interrupt_flag(cpu), "shared comparator flag collects sources");
    expect(state, dspic33_device_interrupt_pending(cpu),
           "shared comparator interrupt becomes pending");
    expect(state,
           dspic33_device_service_interrupt(cpu) &&
               cpu->last_interrupt == COMPARATOR_IRQ && cpu->pc == COMPARATOR_VECTOR,
           "combined comparator interrupt uses IRQ18 vector");
    dspic33_device_return_interrupt(cpu);
    expect(state, status_event(cpu, 0u) && status_event(cpu, 2u),
           "combined comparator status identifies both sources");
    clear_event(cpu, 0u);
    expect(state, !status_event(cpu, 0u) && status_event(cpu, 2u),
           "clearing one comparator preserves another event");
    clear_interrupt(cpu);
    expect(state, !dspic33_device_interrupt_pending(cpu),
           "cleared shared flag does not level retrigger");
    dspic33_write_word(cpu, comparator_base(1u), COMPARATOR_ENABLE | COMPARATOR_EVENT);
    expect(state, interrupt_flag(cpu), "third comparator raises shared flag");
    expect(state,
           dspic33_device_service_interrupt(cpu) &&
               cpu->last_interrupt == COMPARATOR_IRQ,
           "third comparator services the same IRQ");
}

static void pps_case(ComparatorConformance* state, Dspic33* cpu, uint8_t comparator,
                     uint8_t pin, uint16_t address, uint8_t shift) {
    uint16_t mapping = (uint16_t)((0x18u + comparator) << shift);
    dspic33_reset(cpu, 0u);
    expect(state,
           prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 200u,
                            100u),
           "prepare comparator PPS output");
    dspic33_write_word(cpu, comparator_base(comparator),
                       COMPARATOR_ENABLE | COMPARATOR_OUTPUT_ENABLE);
    dspic33_write_word(cpu, address, mapping);
    expect(state, pin_is(cpu, pin, true), "mapped comparator PPS output is high");
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                    0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "transition mapped comparator PPS output");
    expect(state, pin_is(cpu, pin, false), "mapped comparator PPS output follows low");
}

static void pps_cases(ComparatorConformance* state, Dspic33* cpu) {
    bool high;
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint16_t address = (uint16_t)(0x0680u + comparator * 2u);
        pps_case(state, cpu, comparator, (uint8_t)(64u + comparator * 2u), address, 0u);
        pps_case(state, cpu, comparator, (uint8_t)(65u + comparator * 2u), address, 8u);
    }
    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_comparator_output(cpu, 0u, NULL),
           "comparator output rejects null destination");
    expect(state, !dspic33_comparator_output(cpu, DSPIC33_COMPARATOR_COUNT, &high),
           "comparator output rejects invalid channel");
    expect(state, !dspic33_comparator_pin(cpu, 64u, NULL),
           "comparator pin rejects null destination");
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high),
           "unmapped comparator PPS pin is rejected");
    dspic33_write_word(cpu, 0x0680u, 0x0010u);
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high),
           "wrong PPS function is rejected");
    dspic33_write_word(cpu, 0x0680u, 0x0018u);
    dspic33_write_word(cpu, comparator_base(0u), COMPARATOR_ENABLE);
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high),
           "COE disconnects comparator PPS output");
    dspic33_write_word(cpu, comparator_base(0u),
                       COMPARATOR_ENABLE | COMPARATOR_OUTPUT_ENABLE | 0x0010u);
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high),
           "internal comparator reference is excluded");
    dspic33_write_word(cpu, comparator_base(0u),
                       COMPARATOR_ENABLE | COMPARATOR_OUTPUT_ENABLE);
    dspic33_write_word(cpu, (uint16_t)(comparator_base(0u) + 6u), 0x0008u);
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high),
           "comparator filter is excluded");
}

static void sleep_case(ComparatorConformance* state, Dspic33* cpu, uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u);
    configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
    clear_event(cpu, comparator);
    clear_interrupt(cpu);
    configure_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                    200u, 0u),
           "schedule comparator edge in sleep");
    expect(state, dspic33_device_advance(cpu, 0u), "advance comparator edge in sleep");
    expect(state,
           output_is(cpu, comparator, true) && status_event(cpu, comparator) &&
               interrupt_flag(cpu),
           "comparator remains active in sleep");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->power_state == DSPIC33_POWER_ACTIVE &&
               cpu->last_interrupt == COMPARATOR_IRQ,
           "comparator interrupt wakes sleep");
}

static void idle_running_case(ComparatorConformance* state, Dspic33* cpu,
                              uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u);
    configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
    clear_event(cpu, comparator);
    clear_interrupt(cpu);
    configure_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                    200u, 0u),
           "schedule running comparator edge in idle");
    expect(state, dspic33_device_advance(cpu, 0u),
           "advance running comparator edge in idle");
    expect(state,
           output_is(cpu, comparator, true) && status_event(cpu, comparator) &&
               interrupt_flag(cpu),
           "CMSIDL clear keeps comparator active in idle");
    expect(state, dspic33_device_interrupt_pending(cpu),
           "running idle comparator interrupt is pending");
}

static void idle_stopped_case(ComparatorConformance* state, Dspic33* cpu,
                              uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u);
    configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
    clear_event(cpu, comparator);
    clear_interrupt(cpu);
    dspic33_write_word(cpu, COMPARATOR_STATUS, COMPARATOR_STOP_IDLE);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                    200u, 0u),
           "schedule stopped comparator edge in idle");
    expect(state, dspic33_device_advance(cpu, 0u),
           "advance stopped comparator edge in idle");
    expect(state,
           output_is(cpu, comparator, false) && !status_event(cpu, comparator) &&
               !interrupt_flag(cpu),
           "CMSIDL stops comparator in idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state,
           dspic33_device_advance(cpu, 1u) && output_is(cpu, comparator, true) &&
               status_event(cpu, comparator) && interrupt_flag(cpu),
           "comparator evaluates retained input after idle");
}

static void pmd_cases(ComparatorConformance* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 200u, 100u);
    configure_comparator(cpu, 0u, 0u, false, 0u);
    expect(state, output_is(cpu, 0u, true), "comparator starts before PMD disable");
    dspic33_read_byte(cpu, (uint16_t)(comparator_base(0u) + 1u));
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    expect(state, output_is(cpu, 0u, true), "PMD disable waits one instruction cycle");
    expect(state, dspic33_device_advance(cpu, 1u), "advance PMD disable delay");
    expect(state,
           !dspic33_comparator_output(cpu, 0u, &high) &&
               cpu->io.comparator.pmd_disabled,
           "PMD disables comparator after one cycle");
    dspic33_write_word(cpu, comparator_base(0u), 0u);
    dspic33_read_byte(cpu, (uint16_t)(comparator_base(0u) + 1u));
    expect(state,
           cpu->io.comparator.pmd_disabled &&
               !dspic33_comparator_output(cpu, 0u, &high) &&
               (cpu->io.comparator.last_read_cout & 1u) != 0u,
           "PMD ignores comparator register writes while disabled");
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, 0u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &high),
           "PMD enable waits one instruction cycle");
    expect(state, dspic33_device_advance(cpu, 1u), "advance PMD enable delay");
    expect(state, output_is(cpu, 0u, true), "PMD enable restores comparator state");
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    dspic33_device_advance(cpu, 1u);
    expect(
        state,
        dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
            dspic33_device_advance(cpu, 0u),
        "physical input changes while comparator PMD is set");
    expect(state, !dspic33_comparator_output(cpu, 0u, &high) && !interrupt_flag(cpu),
           "disabled comparator ignores physical input event");
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && output_is(cpu, 0u, false),
           "PMD enable evaluates latest physical input");
    expect(state,
           (dspic33_read_word(cpu, COMPARATOR_PMD_ADDRESS) & COMPARATOR_PMD) == 0u &&
               dspic33_read_word(cpu, comparator_base(0u)) == COMPARATOR_ENABLE,
           "PMD enable preserves comparator configuration");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    expect(state,
           (dspic33_read_word(cpu, COMPARATOR_PMD_ADDRESS) & COMPARATOR_PMD) == 0u,
           "failed comparator PMD transition rolls back SFR");
    expect(state, cpu->io.comparator.pmd_generation == 2u,
           "failed comparator PMD transition invalidates generation");
    expect(state, !cpu->io.comparator.pmd_disabled && cpu->events.count == 0u,
           "failed comparator PMD transition preserves effective state");
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "failed comparator PMD transition reports queue error");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, 0u);
    expect(state, cpu->io.comparator.pmd_generation == 2u && cpu->events.count == 2u,
           "rapid comparator PMD toggle queues distinct generations");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.comparator.pmd_disabled &&
               cpu->events.count == 0u,
           "stale comparator PMD transition cannot override latest state");
}

static void power_cases(ComparatorConformance* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        sleep_case(state, cpu, comparator);
        idle_running_case(state, cpu, comparator);
        idle_stopped_case(state, cpu, comparator);
    }
    pmd_cases(state, cpu);
}

static void lifecycle_cases(ComparatorConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool high;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize comparator copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u);
    configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(
        state,
        dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 2u),
        "schedule pending comparator input before copy");
    expect(state, dspic33_copy(&copy, cpu), "copy pending comparator state");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u),
           "advance original and copied comparator");
    expect(state,
           output_is(cpu, 0u, true) && output_is(&copy, 0u, true) &&
               status_event(cpu, 0u) && status_event(&copy, 0u),
           "copy preserves comparator event and input state");
    dspic33_read_byte(cpu, (uint16_t)(comparator_base(0u) + 1u));
    expect(state,
           dspic33_copy(&copy, cpu) && (cpu->io.comparator.last_read_cout & 1u) != 0u &&
               (copy.io.comparator.last_read_cout & 1u) != 0u,
           "copy preserves last-read COUT baseline");

    dspic33_reset(cpu, 0u);
    configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(
        state,
        dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 2u),
        "schedule comparator input before reset");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance reset comparator queue");
    expect(state,
           !dspic33_comparator_output(cpu, 0u, &high) && cpu->events.count == 0u &&
               cpu->io.comparator.output_high == 0u &&
               cpu->io.comparator.last_read_cout == 0u,
           "reset clears comparator state and pending event");

    dspic33_reset(cpu, 0u);
    prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u);
    configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(
        state,
        dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 1u),
        "schedule comparator input before disable");
    dspic33_write_word(cpu, comparator_base(0u), 0u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance disabled comparator input event");
    expect(state,
           !dspic33_comparator_output(cpu, 0u, &high) && !interrupt_flag(cpu) &&
               cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_POSITIVE] == 200u,
           "disabled comparator retains physical input without event");
    configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(state, output_is(cpu, 0u, true) && status_event(cpu, 0u),
           "re-enabled comparator evaluates retained input");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(
        state,
        !dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 1u),
        "comparator input rejects schedule overflow");
    expect(state,
           cpu->events.count == 0u &&
               cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_POSITIVE] == 0u,
           "failed comparator schedule leaves state unchanged");
    expect(state,
           !dspic33_comparator_input(cpu, DSPIC33_COMPARATOR_COUNT,
                                     DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u),
           "reject invalid comparator input channel");
    expect(state,
           !dspic33_comparator_input(
               cpu, 0u, (Dspic33ComparatorInput)DSPIC33_COMPARATOR_INPUT_COUNT, 0u, 0u),
           "reject invalid comparator input selection");
    expect(state, !dspic33_comparator_output(cpu, DSPIC33_COMPARATOR_COUNT, &high),
           "reject invalid comparator output channel");
    expect(state, !dspic33_comparator_output(cpu, 0u, NULL),
           "reject null comparator output destination");
    expect(state, !dspic33_comparator_pin(cpu, 64u, NULL),
           "reject null comparator pin destination");
    dspic33_reset(cpu, 0u);
    prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 200u, 100u);
    configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_POSITIVE] == 200u &&
               cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_NEGATIVE_2] ==
                   100u &&
               cpu->io.comparator.last_read_cout == 0u &&
               dspic33_read_word(cpu, comparator_base(0u)) == 0u &&
               cpu->io.comparator.output_high == 0u,
           "warm reset preserves physical comparator inputs and clears module state");
    dspic33_destroy(&copy);
}

static void dma_and_exclusion_cases(ComparatorConformance* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0x8000u);
    dspic33_write_word(cpu, 0x0b02u, COMPARATOR_IRQ);
    dspic33_write_word(cpu, 0x0b04u, 0x0000u);
    dspic33_write_word(cpu, 0x0b0cu, 0x0000u);
    prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u);
    configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u,
                                    0u) &&
               dspic33_device_advance(cpu, 0u),
           "generate comparator event beside DMA");
    expect(state, cpu->io.dma_index[0] == 0u, "comparator event does not index DMA");
    expect(state, dspic33_read_word(cpu, 0x1000u) == 0u,
           "comparator event does not transfer DMA data");
    expect(state, (dspic33_read_word(cpu, 0x0800u) & 0x0010u) == 0u,
           "comparator event does not raise DMA interrupt");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u,
           "comparator event preserves DMA enable");
    expect(state, cpu->io.dma_peripheral_pending == 0u && cpu->io.dma_active == 0u,
           "comparator event creates no DMA request");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, comparator_base(0u), COMPARATOR_ENABLE | 0x0010u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &high),
           "CVREF comparator source remains active incomplete");
    dspic33_write_word(cpu, comparator_base(0u), COMPARATOR_ENABLE | 0x0003u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &high),
           "IVREF comparator source remains active incomplete");
    dspic33_write_word(cpu, comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_write_word(cpu, (uint16_t)(comparator_base(0u) + 4u), 1u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &high),
           "comparator blanking remains active incomplete");
    dspic33_write_word(cpu, (uint16_t)(comparator_base(0u) + 4u), 0u);
    dspic33_write_word(cpu, (uint16_t)(comparator_base(0u) + 6u), 0x0008u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &high),
           "comparator filtering remains active incomplete");
    dspic33_write_word(cpu, COMPARATOR_REFERENCE, 0x0555u);
    expect(state, dspic33_read_word(cpu, COMPARATOR_REFERENCE) == 0x0555u,
           "CVRCON access is retained without electrical model");
    dspic33_write_word(cpu, (uint16_t)(comparator_base(0u) + 2u), 0x0abcu);
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(comparator_base(0u) + 2u)) == 0x0abcu,
           "CMMSKSRC access is retained without source routing");
    dspic33_write_word(cpu, (uint16_t)(comparator_base(0u) + 4u), 0x8001u);
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(comparator_base(0u) + 4u)) == 0x8001u,
           "CMMSKCON access is retained without blanking model");
    dspic33_write_word(cpu, (uint16_t)(comparator_base(0u) + 6u), 0x007fu);
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(comparator_base(0u) + 6u)) == 0x007fu,
           "CMFLTR access is retained without filter model");
}

int main(void) {
    Dspic33 cpu;
    ComparatorConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize comparator processor");
    if (initialized) {
        access_cases(&state, &cpu);
        selection_cases(&state, &cpu);
        event_polarity_cases(&state, &cpu);
        sticky_rearm_cases(&state, &cpu);
        last_read_cout_cases(&state, &cpu);
        software_event_cases(&state, &cpu);
        pps_cases(&state, &cpu);
        power_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        dma_and_exclusion_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[comparator-summary] cases=%u passed=%u failed=%u\n", state.cases,
           state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
