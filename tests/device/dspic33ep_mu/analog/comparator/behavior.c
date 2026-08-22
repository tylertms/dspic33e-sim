#include "allocation_failure.h"
#include "device/dspic33ep_mu/analog/comparator/internal.h"

void dspic33_device_internal_comparator_update_filter_power(Dspic33* cpu);
void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
enum { COMPARATOR_FILTER_EVENT_SOURCE = 0xfff0u };

static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill comparator event queue");
    }
}

static void filter_boundary_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, COMPARATOR_BASE, COMPARATOR_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(COMPARATOR_BASE + 6u),
                                           COMPARATOR_FILTER_ENABLE);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_COMPARATOR, COMPARATOR_FILTER_EVENT_SOURCE, 0u, 0u),
           "schedule paused comparator filter event");
    cpu->events.items[0].paused = true;
    cpu->events.items[0].paused_remaining = UINT64_MAX;
    cpu->device_cycles = 1u;
    dspic33_device_internal_comparator_update_filter_power(cpu);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "comparator filter resume rejects cycle overflow");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, COMPARATOR_BASE, COMPARATOR_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(COMPARATOR_BASE + 6u),
                                           COMPARATOR_FILTER_ENABLE);
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_comparator_update_filter_power(cpu);
    test_reject_reallocation(false);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "comparator filter creation rejects a full queue");
}
#endif

static void reference_selection_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t band_gap_levels[4] = {1200u, 600u, 200u, 1700u};
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint8_t selection;
        for (selection = 0u; selection < 4u; selection++) {
            uint16_t level = band_gap_levels[selection];
            dspic33_reset(cpu, 0u);
            expect(state,
                   dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE,
                                                1700u, 0u) &&
                       dspic33_device_advance(cpu, 0u),
                   "schedule IVREF external reference");
            dspic33_write_word(cpu, COMPARATOR_REFERENCE, (uint16_t)(selection << 8u));
            cpu->io.comparator.input[comparator][DSPIC33_COMPARATOR_INPUT_POSITIVE] = level;
            dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(comparator),
                               COMPARATOR_ENABLE | 0x0003u);
            expect(state, dspic33_comparator_test_output_is(cpu, comparator, false),
                   "IVREF equality stays low");
            expect(state,
                   dspic33_comparator_input(cpu, comparator, DSPIC33_COMPARATOR_INPUT_POSITIVE,
                                            (uint16_t)(level + 1u), 0u) &&
                       dspic33_device_advance(cpu, 0u) &&
                       dspic33_comparator_test_output_is(cpu, comparator, true),
                   "IVREF selected source controls threshold");
        }
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, COMPARATOR_REFERENCE, COMPARATOR_REFERENCE_EXTERNAL);
    cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_NEGATIVE_2] = 1699u;
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | 0x0010u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "external VREF positive input is available without ladder enable");
    dspic33_write_word(cpu, COMPARATOR_REFERENCE, 0u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &(bool){false}),
           "disabled CVREF ladder has no defined comparator output");
    dspic33_write_word(cpu, COMPARATOR_REFERENCE, 0x0300u | COMPARATOR_REFERENCE_SOURCE_EXTERNAL);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | 0x0003u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &(bool){false}),
           "invalid IVREF external-source combination is rejected");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE, 500u, 0u);
    dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_NEGATIVE, 600u, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_write_word(cpu, COMPARATOR_REFERENCE,
                       COMPARATOR_REFERENCE_ENABLE | COMPARATOR_REFERENCE_SOURCE_EXTERNAL);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | 0x0010u);
    expect(state, !dspic33_comparator_output(cpu, 0u, &(bool){false}),
           "reversed external reference rails are rejected");

    dspic33_reset(cpu, 0u);
    cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_NEGATIVE_2] = 1000u;
    dspic33_write_word(cpu, COMPARATOR_REFERENCE, COMPARATOR_REFERENCE_EXTERNAL);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | 0x0010u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "default VREF positive level drives comparator");
    expect(state,
           dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE, 500u, 2u),
           "schedule delayed comparator reference");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "delayed comparator reference waits for deadline");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "delayed comparator reference applies at deadline");
    expect(state, !dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_COUNT, 0u, 0u),
           "reject invalid comparator reference source");
    cpu->device_cycles = UINT64_MAX;
    expect(state, !dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_AVDD, 0u, 1u),
           "comparator reference rejects schedule overflow");
}

static void set_mask_source(Dspic33* cpu, uint8_t selection, bool high) {
    if (selection < 14u) {
        uint8_t output = (uint8_t)((selection / 2u) * 2u + ((selection & 1u) != 0u ? 0u : 1u));
        cpu->io.pwm[output] = high ? 1u : 0u;
    } else {
        uint32_t bit = (uint32_t)1u << (selection == 14u ? 1u : 3u);
        cpu->io.pwm_fault_direct |= bit;
        if (high) {
            cpu->io.pwm_fault_inputs |= bit;
        } else {
            cpu->io.pwm_fault_inputs &= ~bit;
        }
    }
}

static bool mask_or_term(bool source, uint16_t control, uint16_t positive, uint16_t negative) {
    return ((control & positive) != 0u && source) || ((control & negative) != 0u && !source);
}

static bool mask_and_term(bool source, uint16_t control, uint16_t positive, uint16_t negative,
                          bool result) {
    if ((control & positive) != 0u) {
        result &= source;
    }
    if ((control & negative) != 0u) {
        result &= !source;
    }
    return result;
}

static bool expected_mask(bool source_a, bool source_b, bool source_c, uint16_t control) {
    bool and_result = true;
    bool mask = mask_or_term(source_a, control, 0x0200u, 0x0100u) ||
                mask_or_term(source_b, control, 0x0800u, 0x0400u) ||
                mask_or_term(source_c, control, 0x2000u, 0x1000u);
    and_result = mask_and_term(source_a, control, 0x0002u, 0x0001u, and_result);
    and_result = mask_and_term(source_b, control, 0x0008u, 0x0004u, and_result);
    and_result = mask_and_term(source_c, control, 0x0020u, 0x0010u, and_result);
    return mask || ((control & 0x0040u) != 0u && and_result) ||
           ((control & 0x0080u) != 0u && !and_result);
}

static void blanking_source_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t gate_bits[3][2] = {
        {0x0200u, 0x0100u}, {0x0800u, 0x0400u}, {0x2000u, 0x1000u}};
    uint8_t gate;
    for (gate = 0u; gate < 3u; gate++) {
        uint8_t inverted;
        for (inverted = 0u; inverted < 2u; inverted++) {
            uint8_t selection;
            for (selection = 0u; selection < 16u; selection++) {
                uint8_t high;
                for (high = 0u; high < 2u; high++) {
                    uint16_t source = (uint16_t)(selection << (gate * 4u));
                    dspic33_reset(cpu, 0u);
                    memset(cpu->io.pwm, 0, sizeof(cpu->io.pwm));
                    cpu->io.pwm_fault_inputs = 0u;
                    set_mask_source(cpu, selection, high != 0u);
                    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
                    dspic33_write_word(
                        cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u), source);
                    dspic33_write_word(cpu,
                                       (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 4u),
                                       gate_bits[gate][inverted]);
                    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                                       COMPARATOR_ENABLE);
                    {
                        bool source_high = high != 0u && selection != 12u && selection != 13u;
                        bool expected = inverted != 0u ? source_high : !source_high;
                        expect(state, dspic33_comparator_test_output_is(cpu, 0u, expected),
                               "blanking mux and polarity select documented source");
                    }
                }
            }
        }
    }
}

static void blanking_logic_cases(TestState* state, Dspic33* cpu) {
    uint8_t inputs;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u), 0x0210u);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    for (inputs = 0u; inputs < 8u; inputs++) {
        uint16_t configuration;
        cpu->io.pwm[1] = (inputs & 1u) != 0u ? 1u : 0u;
        cpu->io.pwm[0] = (inputs & 2u) != 0u ? 1u : 0u;
        cpu->io.pwm[3] = (inputs & 4u) != 0u ? 1u : 0u;
        for (configuration = 0u; configuration < 0x8000u; configuration += 257u) {
            uint16_t control =
                (uint16_t)((configuration & 0x3fffu) | ((configuration & 0x4000u) << 1u));
            bool high_level_mask = (control & 0x8000u) != 0u;
            bool raw = !high_level_mask;
            bool mask = expected_mask((inputs & 1u) != 0u, (inputs & 2u) != 0u, (inputs & 4u) != 0u,
                                      control);
            dspic33_comparator_test_set_comparator_relation(cpu, 0u, raw ? 200u : 0u, 100u);
            dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 4u),
                               control);
            expect(state, dspic33_comparator_test_output_is(cpu, 0u, mask ? high_level_mask : raw),
                   "blanking AND-OR truth table");
        }
    }
}

static void configure_filter_clock(Dspic33* cpu, uint8_t source) {
    if (source == 2u) {
        dspic33_write_word(cpu, 0x0c04u, 1u);
        dspic33_write_word(cpu, 0x0c00u, 0x8100u);
    } else if (source == 3u) {
        dspic33_write_word(cpu, 0x0c12u, 1u);
        dspic33_write_word(cpu, 0x0c0eu, 0x0100u);
        dspic33_write_word(cpu, 0x0c00u, 0x8000u);
    } else if (source >= 4u) {
        static const uint16_t controls[4] = {0x0110u, 0x0112u, 0x011eu, 0x0120u};
        uint8_t timer = (uint8_t)(source - 3u);
        dspic33_write_word(cpu, controls[source - 4u], 0x8002u);
        cpu->io.timer_external_started |= (uint16_t)(1u << timer);
    }
}

static void advance_filter_clock(Dspic33* cpu, uint8_t source, uint64_t clocks) {
    if (source == 0u) {
        dspic33_device_advance(cpu, clocks);
    } else if (source == 1u) {
        dspic33_device_advance(cpu, clocks > 1u ? clocks / 2u : 1u);
    } else if (source <= 3u) {
        dspic33_device_advance(cpu, clocks);
    } else {
        uint8_t timer = (uint8_t)(source - 3u);
        dspic33_timer_pulse(cpu, timer, (uint32_t)clocks, 0u);
        dspic33_device_advance(cpu, 0u);
    }
}

static void filter_clock_cases(TestState* state, Dspic33* cpu) {
    uint8_t comparator;
    for (comparator = 0u; comparator < DSPIC33_COMPARATOR_COUNT; comparator++) {
        uint8_t source;
        for (source = 0u; source < 8u; source++) {
            uint8_t divider_selection;
            for (divider_selection = 0u; divider_selection < 8u; divider_selection++) {
                uint16_t divider = (uint16_t)(1u << divider_selection);
                uint64_t first = (uint64_t)divider * 2u;
                dspic33_reset(cpu, 0u);
                dspic33_comparator_test_set_comparator_relation(cpu, comparator, 200u, 100u);
                dspic33_write_word(
                    cpu, (uint16_t)(dspic33_comparator_test_comparator_base(comparator) + 6u),
                    (uint16_t)((source << 4u) | COMPARATOR_FILTER_ENABLE | divider_selection));
                dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(comparator),
                                   COMPARATOR_ENABLE);
                configure_filter_clock(cpu, source);
                expect(state, dspic33_comparator_test_output_is(cpu, comparator, false),
                       "filtered comparator starts from reset output");
                if (source == 1u && divider == 1u) {
                    first = 1u;
                }
                advance_filter_clock(cpu, source, first);
                expect(state, dspic33_comparator_test_output_is(cpu, comparator, false),
                       "filter rejects fewer than three equal samples");
                advance_filter_clock(cpu, source, source == 1u && divider == 1u ? 1u : divider);
                expect(state, dspic33_comparator_test_output_is(cpu, comparator, true),
                       "filter accepts third equal sample");
            }
        }
    }
}

static void filter_sequence_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x0048u);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | 0x00c0u);
    configure_filter_clock(cpu, 4u);
    advance_filter_clock(cpu, 4u, 2u);
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, false) &&
               !dspic33_comparator_test_interrupt_flag(cpu),
           "two filtered samples retain output and event state");
    dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u);
    dspic33_device_advance(cpu, 0u);
    advance_filter_clock(cpu, 4u, 1u);
    dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u);
    dspic33_device_advance(cpu, 0u);
    advance_filter_clock(cpu, 4u, 2u);
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, false) &&
               !dspic33_comparator_test_interrupt_flag(cpu),
           "opposite sample restarts filter qualification");
    advance_filter_clock(cpu, 4u, 1u);
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, true) &&
               dspic33_comparator_test_interrupt_flag(cpu) &&
               dspic33_comparator_test_status_event(cpu, 0u),
           "qualified filter output raises comparator event");
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | 0x00c0u);
    dspic33_comparator_test_clear_interrupt(cpu);
    dspic33_device_advance(cpu, 1u);
    advance_filter_clock(cpu, 4u, 1u);
    expect(state, dspic33_comparator_test_interrupt_flag(cpu),
           "filtered stable output rearms against last read value");
    (void)dspic33_read_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 1u));
    dspic33_comparator_test_clear_event(cpu, 0u);
    dspic33_comparator_test_clear_interrupt(cpu);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0u);
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, true) &&
               !dspic33_comparator_test_interrupt_flag(cpu),
           "disabling filter bypasses samples without a false transition");
}

static void blanking_transition_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, 0x0c00u, 0x8000u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u), 0x0001u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 4u), 0x0200u);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u),
                       COMPARATOR_ENABLE | 0x00c0u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "inactive PWM mask passes comparator output");
    (void)dspic33_read_word(cpu, dspic33_comparator_test_comparator_base(0u));
    dspic33_comparator_test_clear_event(cpu, 0u);
    dspic33_comparator_test_clear_interrupt(cpu);
    dspic33_device_advance(cpu, 1u);
    dspic33_write_word(cpu, 0x0c22u, 0xc3c0u);
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, false) &&
               dspic33_comparator_test_interrupt_flag(cpu),
           "PWM output transition blanks comparator immediately");
    (void)dspic33_read_word(cpu, dspic33_comparator_test_comparator_base(0u));
    dspic33_comparator_test_clear_event(cpu, 0u);
    dspic33_comparator_test_clear_interrupt(cpu);
    dspic33_write_word(cpu, 0x0c22u, 0xc300u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "PWM output release restores comparator output");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u), 0x000eu);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 4u), 0x0200u);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "inactive FLT2 mask passes comparator output");
    dspic33_pwm_fault(cpu, 1u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "FLT2 assertion blanks comparator output");
    dspic33_pwm_fault(cpu, 1u, false, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "FLT2 release restores comparator output");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u), 0x000eu);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 4u), 0x0200u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x0048u);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    configure_filter_clock(cpu, 4u);
    advance_filter_clock(cpu, 4u, 3u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "filter qualifies unblanked comparator output");
    dspic33_pwm_fault(cpu, 1u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    advance_filter_clock(cpu, 4u, 2u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "blanking precedes filter without immediate output change");
    advance_filter_clock(cpu, 4u, 1u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "filtered blanking applies after three samples");
}

static void reference_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized, "initialize comparator reference copy");
    if (!copy_initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_AVDD, 3000u, 0u);
    dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE, 1800u, 0u);
    dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_NEGATIVE, 200u, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_copy(&copy, cpu), "copy comparator reference state");
    dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE, 900u, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state,
           copy.io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_AVDD] == 3000u &&
               copy.io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE] == 1800u &&
               cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE] == 900u,
           "comparator reference copy is independent");
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_AVDD] == 3000u &&
               cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE] == 900u &&
               cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_NEGATIVE] == 200u,
           "warm reset preserves physical comparator references");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_AVDD] == 3300u &&
               cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE] == 3300u &&
               cpu->io.comparator.reference[DSPIC33_COMPARATOR_REFERENCE_VREF_NEGATIVE] == 0u,
           "cold reset restores nominal comparator references");
    dspic33_release(&copy);
}

static void filter_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool high;
    bool copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized, "initialize comparator filter copy");
    if (!copy_initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000au);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_copy(&copy, cpu), "copy pending comparator filter");
    dspic33_device_advance(cpu, 10u);
    dspic33_device_advance(&copy, 10u);
    expect(state,
           dspic33_comparator_test_output_is(cpu, 0u, true) &&
               dspic33_comparator_test_output_is(&copy, 0u, true) &&
               cpu->io.comparator.filter_generation[0] == copy.io.comparator.filter_generation[0],
           "copied comparator filter completes independently");
    dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_comparator_test_output_is(&copy, 0u, true),
           "copied comparator filter input remains independent");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 4u);
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, COMPARATOR_PMD);
    dspic33_device_advance(cpu, 1u);
    expect(state, cpu->io.comparator.pmd_disabled && !dspic33_comparator_output(cpu, 0u, &high),
           "PMD disables filtered comparator at effective boundary");
    dspic33_device_advance(cpu, 20u);
    dspic33_write_word(cpu, COMPARATOR_PMD_ADDRESS, 0u);
    dspic33_device_advance(cpu, 1u);
    dspic33_device_advance(cpu, 18u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "PMD pause retains remaining filter phase");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "PMD resume completes retained filter samples");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 4u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    dspic33_device_advance(cpu, 20u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "Sleep pauses FP comparator filter clock");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    dspic33_device_advance(cpu, 36u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "wake resumes retained FP comparator filter phase");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x0008u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               dspic33_read_word(cpu, dspic33_comparator_test_comparator_base(0u)) == 0u &&
               cpu->events.count == 0u,
           "failed filter activation rolls back comparator control");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x0008u);
    cpu->device_cycles = UINT64_MAX - 1u;
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    expect(state, !dspic33_device_advance(cpu, 1u) && cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "recurring filter schedule overflow stops deterministically");
    dspic33_release(&copy);
}

static void filter_reconfiguration_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 4u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_device_advance(cpu, 20u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "same filter write preserves sampling phase");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 4u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x0008u);
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "changed filter configuration invalidates old phase");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "changed filter configuration uses new deadline");
    dspic33_device_advance(cpu, 1u);
    expect(state,
           cpu->stop_reason == DSPIC33_RUNNING && dspic33_comparator_test_output_is(cpu, 0u, true),
           "stale filter event has no effect");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 4u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    dspic33_device_advance(cpu, 20u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "filter FP clock continues in unrestricted Idle");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, COMPARATOR_STATUS, COMPARATOR_STOP_IDLE);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 4u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    dspic33_device_advance(cpu, 20u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "CMSIDL pauses comparator filter in Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    dspic33_device_advance(cpu, 20u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "leaving Idle resumes retained comparator filter phase");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x000bu);
    dspic33_write_word(cpu, dspic33_comparator_test_comparator_base(0u), COMPARATOR_ENABLE);
    dspic33_device_advance(cpu, 4u);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(
        state,
        dspic33_step(cpu) == DSPIC33_RUNNING &&
            dspic33_read_word(cpu, dspic33_comparator_test_comparator_base(0u)) == 0u &&
            dspic33_read_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u)) ==
                0u &&
            cpu->io.comparator.filter_generation[0] == 0u && cpu->io.comparator.output_high == 0u,
        "warm reset clears comparator filter and pending phase");
}

static void byte_access_behavior_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_comparator_reference(cpu, DSPIC33_COMPARATOR_REFERENCE_VREF_POSITIVE, 1700u, 0u);
    dspic33_device_advance(cpu, 0u);
    cpu->io.comparator.input[0][DSPIC33_COMPARATOR_INPUT_NEGATIVE_2] = 1600u;
    dspic33_write_byte(cpu, (uint16_t)(COMPARATOR_REFERENCE + 1u), 0x04u);
    dspic33_write_byte(cpu, dspic33_comparator_test_comparator_base(0u), 0x10u);
    dspic33_write_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 1u), 0x80u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "byte writes configure external comparator reference");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    cpu->io.pwm[1] = 1u;
    dspic33_write_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u), 0x00u);
    dspic33_write_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 5u), 0x02u);
    dspic33_write_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 1u), 0x80u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "byte writes configure comparator blanking gate");

    dspic33_reset(cpu, 0u);
    dspic33_comparator_test_set_comparator_relation(cpu, 0u, 200u, 100u);
    dspic33_write_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x08u);
    dspic33_write_byte(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 1u), 0x80u);
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, false),
           "low-byte filter configuration requires three samples");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_comparator_test_output_is(cpu, 0u, true),
           "low-byte filter configuration completes on third sample");
}

static void dma_and_completed_feature_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0x8000u);
    dspic33_write_word(cpu, 0x0b02u, COMPARATOR_IRQ);
    dspic33_write_word(cpu, 0x0b04u, 0x0000u);
    dspic33_write_word(cpu, 0x0b0cu, 0x0000u);
    dspic33_comparator_test_prepare_relation(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 0u,
                                             100u);
    dspic33_comparator_test_configure_comparator(cpu, 0u, 0u, false, 0x00c0u);
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
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
    dspic33_write_word(cpu, COMPARATOR_REFERENCE, 0x0555u);
    expect(state, dspic33_read_word(cpu, COMPARATOR_REFERENCE) == 0x0555u,
           "CVRCON retains completed reference configuration");
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u), 0x0abcu);
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 2u)) ==
               0x0abcu,
           "CMMSKSRC retains completed source routing");
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 4u), 0x8001u);
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 4u)) ==
               0x8001u,
           "CMMSKCON retains completed blanking configuration");
    dspic33_write_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u), 0x007fu);
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(dspic33_comparator_test_comparator_base(0u) + 6u)) ==
               0x007fu,
           "CMFLTR retains completed filter configuration");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize comparator processor");
    if (initialized) {
        dspic33_comparator_test_access_cases(&state, &cpu);
        dspic33_comparator_test_selection_cases(&state, &cpu);
        dspic33_comparator_test_event_polarity_cases(&state, &cpu);
        dspic33_comparator_test_sticky_rearm_cases(&state, &cpu);
        dspic33_comparator_test_last_read_cout_cases(&state, &cpu);
        dspic33_comparator_test_software_event_cases(&state, &cpu);
        dspic33_comparator_test_pps_cases(&state, &cpu);
        dspic33_comparator_test_power_cases(&state, &cpu);
        dspic33_comparator_test_lifecycle_cases(&state, &cpu);
        dspic33_comparator_test_reference_ladder_cases(&state, &cpu);
        reference_selection_cases(&state, &cpu);
        blanking_source_cases(&state, &cpu);
        blanking_logic_cases(&state, &cpu);
        filter_clock_cases(&state, &cpu);
        filter_sequence_cases(&state, &cpu);
        blanking_transition_cases(&state, &cpu);
        reference_lifecycle_cases(&state, &cpu);
        filter_lifecycle_cases(&state, &cpu);
        filter_reconfiguration_cases(&state, &cpu);
        byte_access_behavior_cases(&state, &cpu);
        dma_and_completed_feature_cases(&state, &cpu);
#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
        filter_boundary_cases(&state, &cpu);
#endif
        dspic33_release(&cpu);
    }
    test_reject_reallocation(false);
    return test_finish(&state);
}
