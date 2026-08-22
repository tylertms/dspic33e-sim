#include "device/dspic33ep_mu/analog/comparator/internal.h"

uint16_t dspic33_comparator_test_comparator_base(uint8_t comparator) {
    return (uint16_t)(COMPARATOR_BASE + comparator * COMPARATOR_STRIDE);
}

bool dspic33_comparator_test_interrupt_flag(Dspic33* cpu) {
    return (dspic33_read_word(cpu, COMPARATOR_FLAG_ADDRESS) & COMPARATOR_INTERRUPT_BIT) != 0u;
}

void dspic33_comparator_test_clear_interrupt(Dspic33* cpu) {
    dspic33_write_word(
        cpu, COMPARATOR_FLAG_ADDRESS,
        (uint16_t)(dspic33_read_word(cpu, COMPARATOR_FLAG_ADDRESS) & ~COMPARATOR_INTERRUPT_BIT));
}

void dspic33_comparator_test_clear_event(Dspic33* cpu, uint8_t comparator) {
    uint16_t base = dspic33_comparator_test_comparator_base(comparator);
    dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) & ~COMPARATOR_EVENT));
}

bool dspic33_comparator_test_output_is(const Dspic33* cpu, uint8_t comparator, bool expected) {
    bool high;
    return dspic33_comparator_output(cpu, comparator, &high) && high == expected;
}

static bool pin_is(const Dspic33* cpu, uint8_t pin, bool expected) {
    bool high;
    return dspic33_comparator_pin(cpu, pin, &high) && high == expected;
}

static bool status_output(Dspic33* cpu, uint8_t comparator) {
    return (dspic33_read_word(cpu, COMPARATOR_STATUS) & (uint16_t)(1u << comparator)) != 0u;
}

bool dspic33_comparator_test_status_event(Dspic33* cpu, uint8_t comparator) {
    return (dspic33_read_word(cpu, COMPARATOR_STATUS) & (uint16_t)(0x0100u << comparator)) != 0u;
}

bool dspic33_comparator_test_prepare_relation(Dspic33* cpu, uint8_t comparator,
                                              Dspic33ComparatorInput negative, uint16_t positive,
                                              uint16_t negative_level) {
    return dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, positive,
                                    0u) &&
           dspic33_comparator_input(cpu, comparator, negative, negative_level, 0u) &&
           dspic33_device_advance(cpu, 0u);
}

void dspic33_comparator_test_configure_comparator(Dspic33* cpu, uint8_t comparator,
                                                  uint16_t channel, bool inverted,
                                                  uint16_t event_polarity) {
    uint16_t control = (uint16_t)(COMPARATOR_ENABLE | channel | event_polarity |
                                  (inverted ? COMPARATOR_POLARITY : 0u));
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(comparator), control);
}

void dspic33_comparator_test_set_comparator_relation(Dspic33* cpu, uint8_t comparator,
                                                     uint16_t positive, uint16_t negative) {
    cpu->io.comparator.input[comparator][DSPIC33_COMPARATOR_INPUT_POSITIVE] = positive;
    cpu->io.comparator.input[comparator][DSPIC33_COMPARATOR_INPUT_NEGATIVE_2] = negative;
}

static void configure_interrupt(Dspic33* cpu) {
    dspic33_write_word(
        cpu, COMPARATOR_ENABLE_ADDRESS,
        (uint16_t)(dspic33_read_word(cpu, COMPARATOR_ENABLE_ADDRESS) | COMPARATOR_INTERRUPT_BIT));
    dspic33_write_word(
        cpu, COMPARATOR_PRIORITY_ADDRESS,
        (uint16_t)((dspic33_read_word(cpu, COMPARATOR_PRIORITY_ADDRESS) & 0xf8ffu) | 0x0300u));
    cpu->program[(0x0014u + COMPARATOR_IRQ * 2u) / 2u] = COMPARATOR_VECTOR;
    cpu->w[15] = 0x1800u;
}

void dspic33_comparator_test_access_cases(TestState* state, Dspic33* cpu) {
    uint8_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < 14u; index++) {
        uint16_t address = register_addresses[index];
        expect(state, dspic33_read_word(cpu, address) == 0u, "comparator register reset");
        dspic33_write_word(cpu, address, UINT16_MAX);
        expect(state, dspic33_read_word(cpu, address) == register_writable[index],
               "comparator register access mask");
        dspic33_write_word(cpu, address, 0u);
        expect(state, dspic33_read_word(cpu, address) == 0u,
               "comparator register clears writable bits");
    }
}
void dspic33_comparator_test_selection_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint8_t channel;
        for (channel = 0u; channel < 3u; channel++) {
            uint8_t inverted;
            for (inverted = 0u; inverted < 2u; inverted++) {
                dspic33_reset(cpu, 0u);
                expect(state,
                       dspic33_comparator_test_prepare_relation(
                           cpu, comparator, negative_inputs[channel], 100u, 150u),
                       "schedule selected comparator inputs");
                dspic33_comparator_test_configure_comparator(cpu, comparator, channel,
                                                             inverted != 0u, 0u);
                expect(state, dspic33_comparator_test_output_is(cpu, comparator, inverted != 0u),
                       "selected comparator input controls output polarity");
                expect(state, status_output(cpu, comparator) == (inverted != 0u),
                       "CMSTAT mirrors selected comparator output");
            }
        }
    }
}

void dspic33_comparator_test_event_polarity_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint8_t inverted;
        for (inverted = 0u; inverted < 2u; inverted++) {
            uint8_t polarity;
            for (polarity = 0u; polarity < 4u; polarity++) {
                bool rise_event = polarity == 3u || (polarity == 1u && inverted == 0u) ||
                                  (polarity == 2u && inverted != 0u);
                bool fall_event = polarity == 3u || (polarity == 2u && inverted == 0u) ||
                                  (polarity == 1u && inverted != 0u);
                uint16_t event_polarity = (uint16_t)(polarity << 6u);
                dspic33_reset(cpu, 0u);
                expect(state,
                       dspic33_comparator_test_prepare_relation(
                           cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u),
                       "prepare comparator event baseline");
                dspic33_comparator_test_configure_comparator(cpu, comparator, 0u, inverted != 0u,
                                                             event_polarity);
                dspic33_comparator_test_clear_event(cpu, comparator);
                dspic33_comparator_test_clear_interrupt(cpu);
                dspic33_device_advance(cpu, 1u);
                expect(state,
                       dspic33_comparator_test_output_is(cpu, comparator, inverted != 0u) &&
                           !dspic33_comparator_test_status_event(cpu, comparator) &&
                           !dspic33_comparator_test_interrupt_flag(cpu),
                       "comparator event baseline is clear");
                expect(state,
                       dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                                200u, 0u) &&
                           dspic33_device_advance(cpu, 0u),
                       "apply comparator rising relation");
                expect(state,
                       dspic33_comparator_test_status_event(cpu, comparator) == rise_event &&
                           dspic33_comparator_test_interrupt_flag(cpu) == rise_event,
                       "rising comparator relation follows EVPOL");
                dspic33_comparator_test_clear_event(cpu, comparator);
                dspic33_comparator_test_clear_interrupt(cpu);
                expect(state,
                       dspic33_device_advance(cpu, 1u) &&
                           !dspic33_comparator_test_status_event(cpu, comparator) &&
                           !dspic33_comparator_test_interrupt_flag(cpu),
                       "comparator event rearms after one cycle");
                expect(state,
                       dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                                0u, 0u) &&
                           dspic33_device_advance(cpu, 0u) &&
                           dspic33_comparator_test_status_event(cpu, comparator) == fall_event &&
                           dspic33_comparator_test_interrupt_flag(cpu) == fall_event,
                       "falling comparator relation follows EVPOL");
            }
        }
    }
}

void dspic33_comparator_test_sticky_rearm_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        dspic33_reset(cpu, 0u);
        expect(state,
               dspic33_comparator_test_prepare_relation(
                   cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u),
               "prepare sticky comparator event");
        dspic33_comparator_test_configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
        dspic33_comparator_test_clear_event(cpu, comparator);
        dspic33_comparator_test_clear_interrupt(cpu);
        dspic33_device_advance(cpu, 1u);
        expect(
            state,
            dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u),
            "schedule first sticky comparator edge");
        expect(state, dspic33_device_advance(cpu, 0u), "advance first sticky comparator edge");
        expect(state,
               dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_comparator_test_interrupt_flag(cpu),
               "first comparator edge sets sticky event");
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(
            state,
            dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
                dspic33_device_advance(cpu, 0u),
            "apply blocked comparator edge");
        expect(state,
               dspic33_comparator_test_status_event(cpu, comparator) &&
                   !dspic33_comparator_test_interrupt_flag(cpu) &&
                   dspic33_comparator_test_output_is(cpu, comparator, false),
               "sticky event blocks later comparator interrupt");
        dspic33_comparator_test_clear_event(cpu, comparator);
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(state,
               dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u,
                                        0u) &&
                   dspic33_device_advance(cpu, 0u),
               "apply edge during comparator rearm delay");
        expect(state,
               !dspic33_comparator_test_status_event(cpu, comparator) &&
                   !dspic33_comparator_test_interrupt_flag(cpu),
               "rearm delay suppresses immediate comparator edge");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u,
                                            0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_comparator_test_interrupt_flag(cpu),
               "comparator edge fires after rearm cycle");
    }
}

static bool trigger_unread_rising_event(Dspic33* cpu, uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    if (!dspic33_comparator_test_prepare_relation(cpu, comparator,
                                                  DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u, 100u)) {
        return false;
    }
    dspic33_comparator_test_configure_comparator(cpu, comparator, 0u, false, 0x0040u);
    dspic33_comparator_test_clear_event(cpu, comparator);
    dspic33_comparator_test_clear_interrupt(cpu);
    if (!dspic33_device_advance(cpu, 1u)) {
        return false;
    }
    return dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
           dspic33_device_advance(cpu, 0u) &&
           dspic33_comparator_test_status_event(cpu, comparator) &&
           dspic33_comparator_test_interrupt_flag(cpu);
}

void dspic33_comparator_test_last_read_cout_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint16_t base = dspic33_comparator_test_comparator_base(comparator);
        bool prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(state,
               prepared && !dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_device_advance(cpu, 1u) &&
                   dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_comparator_test_interrupt_flag(cpu),
               "literal CEVT clear retriggers from unread COUT mismatch");

        prepared = trigger_unread_rising_event(cpu, comparator);
        uint16_t control = dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(state,
               prepared && (control & COMPARATOR_OUTPUT) != 0u && dspic33_device_advance(cpu, 1u) &&
                   !dspic33_comparator_test_status_event(cpu, comparator) &&
                   !dspic33_comparator_test_interrupt_flag(cpu),
               "word COUT read suppresses retained mismatch");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        dspic33_comparator_test_clear_interrupt(cpu);
        uint8_t high = dspic33_read_byte(cpu, (uint16_t)(base + 1u));
        expect(state,
               prepared && (high & 1u) != 0u && dspic33_device_advance(cpu, 1u) &&
                   !dspic33_comparator_test_status_event(cpu, comparator) &&
                   !dspic33_comparator_test_interrupt_flag(cpu),
               "high-byte COUT read during rearm suppresses mismatch");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_read_byte(cpu, base);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(state,
               prepared && dspic33_device_advance(cpu, 1u) &&
                   dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_comparator_test_interrupt_flag(cpu),
               "low-byte CMxCON read does not acknowledge COUT");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_read_word(cpu, COMPARATOR_STATUS);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(state,
               prepared && dspic33_device_advance(cpu, 1u) &&
                   dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_comparator_test_interrupt_flag(cpu),
               "CMSTAT read does not acknowledge COUT");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_comparator_test_output_is(cpu, comparator, true);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(state,
               prepared && dspic33_device_advance(cpu, 1u) &&
                   dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_comparator_test_interrupt_flag(cpu),
               "logical output query does not acknowledge COUT");

        prepared = trigger_unread_rising_event(cpu, comparator);
        dspic33_read_byte(cpu, (uint16_t)(base + 1u));
        expect(state,
               prepared && dspic33_comparator_test_status_event(cpu, comparator) &&
                   dspic33_comparator_test_interrupt_flag(cpu) &&
                   (cpu->io.comparator.last_read_cout & (uint8_t)(1u << comparator)) != 0u,
               "sticky CEVT survives COUT baseline refresh");

        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | 0x0040u);
        dspic33_comparator_test_clear_interrupt(cpu);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !dspic33_comparator_test_status_event(cpu, comparator) &&
                   !dspic33_comparator_test_interrupt_flag(cpu) &&
                   (cpu->io.comparator.last_read_cout & (uint8_t)(1u << comparator)) != 0u,
               "CMxCON disable preserves last-read COUT baseline");
    }
}

void dspic33_comparator_test_software_event_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint16_t base = dspic33_comparator_test_comparator_base(comparator);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | COMPARATOR_EVENT);
        expect(state,
               (dspic33_read_word(cpu, base) & COMPARATOR_EVENT) != 0u &&
                   dspic33_comparator_test_status_event(cpu, comparator),
               "software CEVT sets register and CMSTAT mirror");
        expect(state, dspic33_comparator_test_interrupt_flag(cpu),
               "software CEVT raises combined interrupt");
        dspic33_comparator_test_clear_interrupt(cpu);
        dspic33_write_word(cpu, base, COMPARATOR_ENABLE | COMPARATOR_EVENT);
        expect(state, !dspic33_comparator_test_interrupt_flag(cpu),
               "set CEVT does not refire while sticky");
        dspic33_write_word(cpu, COMPARATOR_STATUS, 0u);
        expect(state, dspic33_comparator_test_status_event(cpu, comparator),
               "CMSTAT cannot clear CEVT mirror");
        dspic33_comparator_test_clear_event(cpu, comparator);
        expect(state,
               (dspic33_read_word(cpu, base) & COMPARATOR_EVENT) == 0u &&
                   !dspic33_comparator_test_status_event(cpu, comparator),
               "CMxCON clears software CEVT");
    }

    dspic33_reset(cpu, 0u);
    configure_interrupt(cpu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | COMPARATOR_EVENT);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(2u),
                       COMPARATOR_ENABLE | COMPARATOR_EVENT);
    expect(state, dspic33_comparator_test_interrupt_flag(cpu),
           "shared comparator flag collects sources");
    expect(state, dspic33_device_interrupt_pending(cpu),
           "shared comparator interrupt becomes pending");
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == COMPARATOR_IRQ &&
               cpu->pc == COMPARATOR_VECTOR,
           "combined comparator interrupt uses IRQ18 vector");
    dspic33_device_return_interrupt(cpu);
    expect(state,
           dspic33_comparator_test_status_event(cpu, 0u) &&
               dspic33_comparator_test_status_event(cpu, 2u),
           "combined comparator status identifies both sources");
    dspic33_comparator_test_clear_event(cpu, 0u);
    expect(state,
           !dspic33_comparator_test_status_event(cpu, 0u) &&
               dspic33_comparator_test_status_event(cpu, 2u),
           "clearing one comparator preserves another event");
    dspic33_comparator_test_clear_interrupt(cpu);
    expect(state, !dspic33_device_interrupt_pending(cpu),
           "cleared shared flag does not level retrigger");
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(1u),
                       COMPARATOR_ENABLE | COMPARATOR_EVENT);
    expect(state, dspic33_comparator_test_interrupt_flag(cpu),
           "third comparator raises shared flag");
    expect(state, dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == COMPARATOR_IRQ,
           "third comparator services the same IRQ");
}

static void pps_case(TestState* state, Dspic33* cpu, uint8_t comparator, uint8_t pin,
                     uint16_t address, uint8_t shift) {
    uint16_t mapping = (uint16_t)((0x18u + comparator) << shift);
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_comparator_test_prepare_relation(
               cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 200u, 100u),
           "prepare comparator PPS output");
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(comparator),
                       COMPARATOR_ENABLE | COMPARATOR_OUTPUT_ENABLE);
    dspic33_write_word(cpu, address, mapping);
    expect(state, pin_is(cpu, pin, true), "mapped comparator PPS output is high");
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "transition mapped comparator PPS output");
    expect(state, pin_is(cpu, pin, false), "mapped comparator PPS output follows low");
}

void dspic33_comparator_test_pps_cases(TestState* state, Dspic33* cpu) {
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
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high), "wrong PPS function is rejected");
    dspic33_write_word(cpu, 0x0680u, 0x0018u);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high),
           "COE disconnects comparator PPS output");
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | COMPARATOR_OUTPUT_ENABLE | 0x0010u);
    expect(state, !dspic33_comparator_pin(cpu, 64u, &high),
           "disabled internal comparator reference disconnects output");
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | COMPARATOR_OUTPUT_ENABLE);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x0008u);
    expect(state, pin_is(cpu, 64u, false), "filtered comparator remains available on PPS output");
}

static void sleep_case(TestState* state, Dspic33* cpu, uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2,
                                             0u, 100u);
    dspic33_comparator_test_configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
    dspic33_comparator_test_clear_event(cpu, comparator);
    dspic33_comparator_test_clear_interrupt(cpu);
    configure_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u),
           "schedule comparator edge in sleep");
    expect(state, dspic33_device_advance(cpu, 0u), "advance comparator edge in sleep");
    expect(state,
           dspic33_comparator_test_output_is(cpu, comparator, true) &&
               dspic33_comparator_test_status_event(cpu, comparator) &&
               dspic33_comparator_test_interrupt_flag(cpu),
           "comparator remains active in sleep");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->power_state == DSPIC33_POWER_ACTIVE &&
               cpu->last_interrupt == COMPARATOR_IRQ,
           "comparator interrupt wakes sleep");
}

static void idle_running_case(TestState* state, Dspic33* cpu, uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2,
                                             0u, 100u);
    dspic33_comparator_test_configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
    dspic33_comparator_test_clear_event(cpu, comparator);
    dspic33_comparator_test_clear_interrupt(cpu);
    configure_interrupt(cpu);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u),
           "schedule running comparator edge in idle");
    expect(state, dspic33_device_advance(cpu, 0u), "advance running comparator edge in idle");
    expect(state,
           dspic33_comparator_test_output_is(cpu, comparator, true) &&
               dspic33_comparator_test_status_event(cpu, comparator) &&
               dspic33_comparator_test_interrupt_flag(cpu),
           "CMSIDL clear keeps comparator active in idle");
    expect(state, dspic33_device_interrupt_pending(cpu),
           "running idle comparator interrupt is pending");
}

static void idle_stopped_case(TestState* state, Dspic33* cpu, uint8_t comparator) {
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_prepare_relation(cpu, comparator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2,
                                             0u, 100u);
    dspic33_comparator_test_configure_comparator(cpu, comparator, 0u, false, 0x00c0u);
    dspic33_comparator_test_clear_event(cpu, comparator);
    dspic33_comparator_test_clear_interrupt(cpu);
    dspic33_write_word(cpu, COMPARATOR_STATUS, COMPARATOR_STOP_IDLE);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u),
           "schedule stopped comparator edge in idle");
    expect(state, dspic33_device_advance(cpu, 0u), "advance stopped comparator edge in idle");
    expect(state,
           dspic33_comparator_test_output_is(cpu, comparator, false) &&
               !dspic33_comparator_test_status_event(cpu, comparator) &&
               !dspic33_comparator_test_interrupt_flag(cpu),
           "CMSIDL stops comparator in idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_comparator_test_output_is(cpu, comparator, true) &&
               dspic33_comparator_test_status_event(cpu, comparator) &&
               dspic33_comparator_test_interrupt_flag(cpu),
           "comparator evaluates retained input after idle");
}

static void pmd_cases(TestState* state, Dspic33* cpu) {
    bool high;
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 200u,
                                             100u);
    dspic33_comparator_test_configure_comparator(cpu, 0u, 0u, false, 0u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "comparator starts before PMD disable");
    dspic33_read_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 1u));
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "PMD disable waits one instruction cycle");
    expect(state, dspic33_device_advance(cpu, 1u), "advance PMD disable delay");
    expect(state, !dspic33_comparator_output(cpu, 0u, &high) && cpu->io.comparator.pmd_disabled,
           "PMD disables comparator after one cycle");
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), 0u);
    dspic33_read_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 1u));
    expect(state,
           cpu->io.comparator.pmd_disabled && !dspic33_comparator_output(cpu, 0u, &high) &&
               (cpu->io.comparator.last_read_cout & 1u) != 0u,
           "PMD ignores comparator register writes while disabled");
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, 0u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &high),
           "PMD enable waits one instruction cycle");
    expect(state, dspic33_device_advance(cpu, 1u), "advance PMD enable delay");
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "PMD enable restores comparator state");
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    dspic33_device_advance(cpu, 1u);
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "physical input changes while comparator PMD is set");
    expect(state,
           !dspic33_comparator_output(cpu, 0u, &high) &&
               !dspic33_comparator_test_interrupt_flag(cpu),
           "disabled comparator ignores physical input event");
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_comparator_test_output_is(cpu, 0u, false),
           "PMD enable evaluates latest physical input");
    expect(state,
           (dspic33_read_word(cpu, COMPARATOR_PMD_ADDRESS) & COMPARATOR_PMD) == 0u &&
               dspic33_read_word(cpu, dspic33_comparator_test_comparator_base(0u)) ==
                   COMPARATOR_ENABLE,
           "PMD enable preserves comparator configuration");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    expect(state, (dspic33_read_word(cpu, COMPARATOR_PMD_ADDRESS) & COMPARATOR_PMD) == 0u,
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

void dspic33_comparator_test_power_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        sleep_case(state, cpu, comparator);
        idle_running_case(state, cpu, comparator);
        idle_stopped_case(state, cpu, comparator);
    }
    pmd_cases(state, cpu);
}

void dspic33_comparator_test_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool high;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize comparator copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u,
                                             100u);
    dspic33_comparator_test_configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(state, dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 2u),
           "schedule pending comparator input before copy");
    expect(state, dspic33_copy(&copy, cpu), "copy pending comparator state");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u),
           "advance original and copied comparator");
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, true) &&
               dspic33_comparator_test_output_is(&copy, 0u, true) &&
               dspic33_comparator_test_status_event(cpu, 0u) &&
               dspic33_comparator_test_status_event(&copy, 0u),
           "copy preserves comparator event and input state");
    dspic33_read_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 1u));
    expect(state,
           dspic33_copy(&copy, cpu) && (cpu->io.comparator.last_read_cout & 1u) != 0u &&
               (copy.io.comparator.last_read_cout & 1u) != 0u,
           "copy preserves last-read COUT baseline");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(state, dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 2u),
           "schedule comparator input before reset");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance reset comparator queue");
    expect(state,
           !dspic33_comparator_output(cpu, 0u, &high) && cpu->events.count == 0u &&
               cpu->io.comparator.output_high == 0u && cpu->io.comparator.last_read_cout == 0u,
           "reset clears comparator state and pending event");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u,
                                             100u);
    dspic33_comparator_test_configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(state, dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 1u),
           "schedule comparator input before disable");
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance disabled comparator input event");
    expect(state,
           !dspic33_comparator_output(cpu, 0u, &high) &&
               !dspic33_comparator_test_interrupt_flag(cpu) &&
               cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_POSITIVE] == 200u,
           "disabled comparator retains physical input without event");
    dspic33_comparator_test_configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, true) &&
               dspic33_comparator_test_status_event(cpu, 0u),
           "re-enabled comparator evaluates retained input");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(state, !dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 1u),
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
    dspic33_comparator_test_prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 200u,
                                             100u);
    dspic33_comparator_test_configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_POSITIVE] == 200u &&
               cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_NEGATIVE_2] == 100u &&
               cpu->io.comparator.last_read_cout == 0u &&
               dspic33_read_word(cpu, dspic33_comparator_test_comparator_base(0u)) == 0u &&
               cpu->io.comparator.output_high == 0u,
           "warm reset preserves physical comparator inputs and clears module state");
    dspic33_release(&copy);
}

static uint16_t expected_dac_level(uint16_t source, uint8_t tap, bool low_range) {
    return (uint16_t)(low_range ? (uint32_t)source * tap / 24u
                                : (uint32_t)source * (8u + tap) / 32u);
}

void dspic33_comparator_test_reference_ladder_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint8_t external_source;
        for (external_source = 0u; external_source < 2u; external_source++) {
            uint8_t low_range;
            for (low_range = 0u; low_range < 2u; low_range++) {
                uint8_t tap;
                for (tap = 0u; tap < 16u; tap++) {
                    uint16_t source = external_source != 0u ? 2400u : 3200u;
                    uint16_t level = expected_dac_level(source, tap, low_range != 0u);
                    uint16_t reference =
                        (uint16_t)(COMPARATOR_REFERENCE_ENABLE | tap |
                                   (external_source != 0u ? COMPARATOR_REFERENCE_SOURCE_EXTERNAL
                                                          : 0u) |
                                   (low_range != 0u ? COMPARATOR_REFERENCE_LOW_RANGE : 0u));
                    dspic33_reset(cpu, 0u);
                    expect(state,
                           dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_AVDD,
                                                        3200u, 0u) &&
                               dspic33_comparator_reference(
                                   cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE, 2800u, 0u) &&
                               dspic33_comparator_reference(
                                   cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_NEGATIVE, 400u, 0u) &&
                               dspic33_device_advance(cpu, 0u),
                           "schedule comparator voltage references");
                    dspic33_write_word(cpu, COMPARATOR_REFERENCE, reference);
                    cpu->io.comparator.input[comparator][DSPIC33_COMPARATOR_INPUT_NEGATIVE_2] =
                        level;
                    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(comparator),
                                       COMPARATOR_ENABLE | 0x0010u);
                    expect(state, dspic33_comparator_test_output_is(cpu, comparator, false),
                           "CVREF ladder equality stays low");
                    if (level != 0u) {
                        expect(state,
                               dspic33_comparator_input(cpu, comparator,
                                                        DSPIC33_COMPARATOR_INPUT_NEGATIVE_2,
                                                        (uint16_t)(level - 1u), 0u) &&
                                   dspic33_device_advance(cpu, 0u) &&
                                   dspic33_comparator_test_output_is(cpu, comparator, true),
                               "CVREF ladder exceeds lower channel input");
                    }
                }
            }
        }
    }
}
