#include "output_compare_test_support.h"

static void alternate_clock_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t selections[] = {4u, 0u, 1u, 2u, 3u};
    static const uint8_t timers[] = {0u, 1u, 2u, 3u, 4u};
    uint8_t channel;
    size_t index;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        for (index = 0u; index < sizeof(selections) / sizeof(selections[0]); index++) {
            uint16_t base = compare_base(channel);
            uint8_t timer = timers[index];
            dspic33_reset(cpu, 0u);
            dspic33_write_word(cpu, timer_periods[timer], 100u);
            dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
            dspic33_write_word(cpu, (uint16_t)(base + 4u), 4u);
            dspic33_write_word(cpu, (uint16_t)(base + 6u), 2u);
            dspic33_write_word(cpu, base, (uint16_t)((uint16_t)selections[index] << 10u | 6u));
            dspic33_write_word(cpu, (uint16_t)(base + 2u), COMPARE_SELF_SYNC);
            expect(state,
                   output_is(cpu, channel, true) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
                   "alternate clock starts PWM at zero");
            expect(state, dspic33_device_advance(cpu, 2u), "advance alternate clock to duty");
            expect(state,
                   output_is(cpu, channel, false) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                       dspic33_read_word(cpu, timer_registers[timer]) == 2u,
                   "selected timer clock reaches OC duty match");
            expect(state, dspic33_device_advance(cpu, 2u),
                   "advance alternate clock through period value");
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 4u &&
                       !interrupt_flag(cpu, channel),
                   "alternate clock retains final period value");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "advance alternate clock through rollover");
            expect(state,
                   output_is(cpu, channel, true) &&
                       dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                       interrupt_flag(cpu, channel),
                   "alternate clock resets and raises PWM interrupt");
        }
    }
}

static bool prepare_compare_trigger_source(Dspic33* cpu, uint8_t source) {
    if (source >= 1u && source <= 9u) {
        configure_compare_mode(cpu, (uint8_t)(source - 1u), 5u, 1u, 0u, COMPARE_NO_SYNC);
        return true;
    }
    if (source >= 11u && source <= 15u) {
        uint8_t timer = (uint8_t)(source - 11u);
        dspic33_write_word(cpu, timer_periods[timer], 1u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        return true;
    }
    if (source >= 16u && source <= 23u) {
        uint16_t base = (uint16_t)(0x0140u + (source - 16u) * 8u);
        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
        dspic33_write_word(cpu, base, 0x1c03u);
        return true;
    }
    if (source >= 24u && source <= 26u) {
        uint16_t base = (uint16_t)(0x0a84u + (source - 24u) * 8u);
        dspic33_write_word(cpu, base, 0x8040u);
        return true;
    }
    if (source == 27u) {
        dspic33_write_word(cpu, 0x0320u, 0x8000u);
        return true;
    }
    return source == 29u || source == 30u;
}

static bool emit_compare_trigger_source(Dspic33* cpu, uint8_t source) {
    if (source >= 1u && source <= 9u) {
        return dspic33_device_advance(cpu, 2u);
    }
    if (source >= 11u && source <= 15u) {
        return dspic33_device_advance(cpu, 1u);
    }
    if (source >= 16u && source <= 23u) {
        return dspic33_input_capture_input(cpu, (uint8_t)(source - 16u), true, 0u) &&
               dspic33_device_advance(cpu, 3u);
    }
    if (source >= 24u && source <= 26u) {
        return dspic33_comparator_input(cpu, (uint8_t)(source - 24u),
                                        DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u);
    }
    if (source == 27u) {
        dspic33_write_word(cpu, 0x0320u, 0x8002u);
        dspic33_write_word(cpu, 0x0320u, 0x8000u);
        return dspic33_device_advance(cpu, 12u);
    }
    if (source == 29u || source == 30u) {
        uint16_t irq = source == 29u ? 20u : 29u;
        return dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, irq, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u);
    }
    return false;
}

static void trigger_source_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t sources[] = {1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,  9u,  11u,
                                      12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u,
                                      22u, 23u, 24u, 25u, 26u, 27u, 29u, 30u};
    size_t index;
    for (index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        uint8_t source = sources[index];
        uint16_t base = compare_base(15u);
        dspic33_reset(cpu, 0u);
        expect(state, prepare_compare_trigger_source(cpu, source),
               "prepare documented OC trigger source");
        configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | source));
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & COMPARE_TRIGGER_STATUS) == 0u,
               "triggered OC timer starts held clear");
        expect(state, emit_compare_trigger_source(cpu, source),
               "emit documented OC trigger source");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & COMPARE_TRIGGER_STATUS) != 0u,
               "documented source sets OC trigger status without same-edge count");
        expect(state, dspic33_device_advance(cpu, 1u), "advance first clock after OC trigger");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u,
               "triggered OC timer counts on following clock");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, COMPARE_TRIGGER);
    expect(state, dspic33_device_advance(cpu, 5u), "advance software-only triggered OC while held");
    expect(state, dspic33_read_word(cpu, 0x099eu) == 0u,
           "zero trigger source remains software-only");
    dspic33_write_word(cpu, 0x0998u, (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS));
    expect(state, dspic33_device_advance(cpu, 1u), "advance software-released OC trigger");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 1u &&
               (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u,
           "software sets trigger status for source zero");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 10u));
    expect(state, dspic33_device_advance(cpu, 5u), "advance no-source trigger while held");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 0u &&
               (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "no-source trigger remains software controlled");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 28u,
                     "reserved OC synchronization source is inactive");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, (uint16_t)(COMPARE_TRIGGER | 1u),
                     "OC cannot select itself as alternate trigger source");
}

static void trigger_one_shot_cases(TestState* state, Dspic33* cpu) {
    uint16_t control2;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0904u, 2u);
    dspic33_write_word(cpu, 0x0906u, 1u);
    dspic33_write_word(cpu, 0x0900u, (uint16_t)(COMPARE_FP | COMPARE_TRIGGER_ONESHOT | 6u));
    dspic33_write_word(cpu, 0x0902u, (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS | 29u));
    expect(state, (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u,
           "one-shot trigger status rejects software set");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "emit first one-shot trigger");
    expect(state,
           (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "hardware releases one-shot timer");
    expect(state, dspic33_device_advance(cpu, 3u), "advance one-shot trigger through period");
    expect(state,
           (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
           "one-shot rollover clears trigger status and holds timer");
    clear_interrupt(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance held one-shot timer");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u,
           "one-shot timer remains held for next trigger");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 2u),
           "retrigger one-shot timer");
    control2 = dspic33_read_word(cpu, 0x0902u);
    expect(state,
           (control2 & COMPARE_TRIGGER_STATUS) != 0u && dspic33_read_word(cpu, 0x0908u) == 1u,
           "new hardware trigger starts another one-shot period");
}

static void synchronization_source_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t sources[] = {1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,  9u,  11u,
                                      12u, 13u, 14u, 15u, 24u, 25u, 26u, 27u, 29u, 30u};
    size_t index;
    for (index = 0u; index < sizeof(sources) / sizeof(sources[0]); index++) {
        uint8_t source = sources[index];
        uint16_t base = compare_base(15u);
        dspic33_reset(cpu, 0u);
        expect(state, prepare_compare_trigger_source(cpu, source),
               "prepare documented OC synchronization source");
        configure_compare_mode(cpu, 15u, 6u, 100u, 50u, source);
        expect(state, emit_compare_trigger_source(cpu, source),
               "emit documented OC synchronization source");
        expect(state,
               (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u &&
                   !interrupt_flag(cpu, 15u),
               "synchronization pulse preserves timer through source edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance clock after OC synchronization pulse");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u && interrupt_flag(cpu, 15u),
               "synchronization resets OC timer on following clock");
    }
}

static void input_capture_synchronization_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t clock_selections[] = {4u, 0u, 1u, 2u, 3u};
    static const uint16_t capture_clock_selections[] = {0x1000u, 0x0400u, 0x0000u, 0x0800u,
                                                        0x0c00u};
    uint8_t source;
    uint8_t timer;
    uint8_t channel;
    uint8_t mode;
    for (source = 0u; source < 8u; source++) {
        uint16_t capture_base = (uint16_t)(0x0140u + source * 8u);
        dspic33_reset(cpu, 0u);
        expect(state, prepare_compare_trigger_source(cpu, (uint8_t)(16u + source)),
               "prepare input capture synchronization source");
        expect(state, dspic33_device_advance(cpu, 5u),
               "advance input capture timer before synchronization");
        configure_compare_mode(cpu, 15u, 6u, 100u, 50u, (uint16_t)(16u + source));
        expect(state,
               dspic33_input_capture_input(cpu, source, true, 0u) &&
                   dspic33_device_advance(cpu, 3u),
               "emit input capture synchronization interrupt");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(capture_base + 6u)) == 8u &&
                   dspic33_read_word(cpu, 0x099eu) == 3u &&
                   (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u,
               "input capture interrupt retains distinct timer phases");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance input capture synchronization clock");
        expect(state,
               dspic33_read_word(cpu, 0x099eu) == 9u &&
                   dspic33_read_word(cpu, 0x099eu) ==
                       dspic33_read_word(cpu, (uint16_t)(capture_base + 6u)) &&
                   (cpu->io.output_compare.sync_reset_pending & 0x8000u) == 0u &&
                   interrupt_flag(cpu, 15u),
               "OC timer adopts each input capture timer phase");
    }

    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        for (source = 0u; source < 8u; source++) {
            uint16_t capture_base = (uint16_t)(0x0140u + source * 8u);
            uint16_t base = compare_base(channel);
            dspic33_reset(cpu, 0u);
            expect(state,
                   prepare_compare_trigger_source(cpu, (uint8_t)(16u + source)) &&
                       dspic33_device_advance(cpu, 2u),
                   "prepare OC channel and input capture source matrix");
            configure_compare_mode(cpu, channel, 6u, 100u, 50u, (uint16_t)(16u + source));
            expect(state,
                   dspic33_input_capture_input(cpu, source, true, 0u) &&
                       dspic33_device_advance(cpu, 3u) &&
                       (cpu->io.output_compare.sync_reset_pending & (uint16_t)(1u << channel)) !=
                           0u,
                   "all OC channels accept every input capture source");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "advance OC channel and input capture source matrix");
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) ==
                           dspic33_read_word(cpu, (uint16_t)(capture_base + 6u)) &&
                       (cpu->io.output_compare.sync_reset_pending & (uint16_t)(1u << channel)) ==
                           0u,
                   "OC channel matrix adopts selected input capture phase");
        }
    }

    for (mode = 1u; mode <= 7u; mode++) {
        dspic33_reset(cpu, 0u);
        expect(state, prepare_compare_trigger_source(cpu, 16u) && dspic33_device_advance(cpu, 5u),
               "prepare input capture synchronization mode matrix");
        configure_compare_mode(cpu, 0u, mode, 100u, 50u, 16u);
        expect(state,
               dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u) &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
               "all OC modes accept input capture synchronization");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance input capture synchronization mode matrix");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(cpu, 0x0146u) &&
                   dspic33_read_word(cpu, 0x0908u) == 9u,
               "all OC modes adopt the input capture timer phase");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0142u, 0u);
    dspic33_write_word(cpu, 0x0140u, 0x1c23u);
    configure_compare_mode(cpu, 0u, 6u, 100u, 50u, 16u);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 2u),
           "emit capture without the configured IC interrupt interval");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 2u &&
               (cpu->io.output_compare.sync_reset_pending & 1u) == 0u,
           "capture event alone does not synchronize the OC timer");
    expect(state,
           dspic33_input_capture_input(cpu, 0u, false, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u),
           "emit capture that reaches the IC interrupt interval");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 5u &&
               (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
           "generated IC interrupt requests OC synchronization");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(cpu, 0x0146u),
           "OC adopts phase only after the IC interrupt");

    for (timer = 0u; timer < 5u; timer++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0142u, 0u);
        dspic33_write_word(cpu, 0x0140u, (uint16_t)(capture_clock_selections[timer] | 3u));
        expect(state,
               dspic33_read_word(cpu, 0x0140u) == (uint16_t)(capture_clock_selections[timer] | 3u),
               "configure matching alternate-clock input capture source");
        dspic33_write_word(cpu, timer_periods[timer], 100u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        expect(state, dspic33_device_advance(cpu, 5u),
               "advance alternate-clock input capture phase");
        configure_compare_mode(cpu, 0u, 6u, 100u, 50u, 16u);
        dspic33_write_word(cpu, 0x0900u, (uint16_t)((uint16_t)clock_selections[timer] << 10u | 6u));
        expect(state,
               dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u) &&
                   dspic33_read_word(cpu, 0x0908u) == 3u && dspic33_read_word(cpu, 0x0146u) == 8u &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
               "input capture interrupt preserves alternate-clock phases");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance selected clock after input capture interrupt");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == 9u &&
                   dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(cpu, 0x0146u) &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) == 0u,
               "all selected OC clocks adopt the input capture phase");
    }
}

static void input_capture_cascade_synchronization_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t source;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0142u, COMPARE_CASCADE);
    dspic33_write_word(cpu, 0x014au, COMPARE_CASCADE);
    dspic33_write_word(cpu, 0x0140u, 0x1c03u);
    dspic33_write_word(cpu, 0x0148u, 0x1c03u);
    expect(state, dspic33_device_advance(cpu, UINT32_C(0x00010002)),
           "advance cascaded input capture phase");
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00020000), UINT32_C(0x00010080), COMPARE_FP, 16u,
                      false);
    expect(state,
           dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u) &&
               (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
           "emit cascaded input capture synchronization");
    expect(state, dspic33_device_advance(cpu, 1u), "advance cascaded OC synchronization clock");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 6u && compare_raw_word(cpu, 0x0912u) == 1u &&
               compare_raw_word(cpu, 0x0908u) == compare_raw_word(cpu, 0x0146u) &&
               compare_raw_word(cpu, 0x0912u) == compare_raw_word(cpu, 0x014eu),
           "cascaded OC adopts the full input capture timer phase");

    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel += 2u) {
        source = (uint8_t)(channel / 2u);
        dspic33_reset(cpu, 0u);
        expect(state,
               prepare_compare_trigger_source(cpu, (uint8_t)(16u + source)) &&
                   dspic33_device_advance(cpu, 5u),
               "prepare cascaded OC input capture matrix");
        configure_cascade(cpu, channel, 6u, UINT32_C(0x00010064), UINT32_C(0x00000032), COMPARE_FP,
                          (uint16_t)(16u + source), false);
        expect(state,
               dspic33_input_capture_input(cpu, source, true, 0u) &&
                   dspic33_device_advance(cpu, 3u) &&
                   (cpu->io.output_compare.sync_reset_pending & (uint16_t)(1u << channel)) != 0u,
               "all cascaded OC pairs accept input capture synchronization");
        expect(state, dspic33_device_advance(cpu, 1u), "advance cascaded OC input capture matrix");
        expect(state,
               compare_raw_word(cpu, (uint16_t)(compare_base(channel) + 8u)) == 9u &&
                   compare_raw_word(cpu, (uint16_t)(compare_base((uint8_t)(channel + 1u)) + 8u)) ==
                       0u,
               "all cascaded OC pairs adopt a 16-bit input capture phase");
    }
}

static void input_capture_synchronization_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    uint64_t cycle;
    expect(state, initialized, "initialize input capture synchronization copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    expect(state, prepare_compare_trigger_source(cpu, 16u),
           "prepare restartable input capture synchronization");
    expect(state, dspic33_device_advance(cpu, 5u), "advance restartable input capture source");
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 16u);
    expect(state,
           dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 4u) &&
               dspic33_read_word(cpu, 0x099eu) == 9u,
           "initial input capture synchronization aligns OC phase");
    dspic33_write_word(cpu, 0x0140u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 2u) && dspic33_read_word(cpu, 0x0146u) == 0u &&
               dspic33_read_word(cpu, 0x099eu) == 11u,
           "stopped input capture leaves synchronized OC running independently");
    expect(state,
           dspic33_input_capture_input(cpu, 0u, false, 0u) && dspic33_device_advance(cpu, 0u),
           "return restarted input capture source low");
    dspic33_write_word(cpu, 0x0140u, 0x1c03u);
    expect(state,
           dspic33_device_advance(cpu, 4u) && dspic33_input_capture_input(cpu, 0u, true, 0u) &&
               dspic33_device_advance(cpu, 4u),
           "restart and emit another input capture synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == dspic33_read_word(cpu, 0x0146u) &&
               dspic33_read_word(cpu, 0x099eu) == 8u,
           "restarted input capture realigns the OC timer phase");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0142u, 0u);
    dspic33_write_word(cpu, 0x0140u, 0x0403u);
    expect(state, dspic33_read_word(cpu, 0x0140u) == 0x0403u,
           "prepare PMD-held input capture synchronization");
    expect(state, dspic33_device_advance(cpu, 5u) && dspic33_read_word(cpu, 0x0146u) == 0u,
           "stopped common clock holds PMD input capture phase");
    dspic33_write_word(cpu, timer_periods[1], 100u);
    configure_compare_mode(cpu, 0u, 6u, 100u, 50u, 16u);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    expect(state,
           dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u) &&
               (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
           "queue input capture synchronization before OC PMD");
    dspic33_write_word(cpu, 0x0762u, 1u);
    expect(state, dspic33_device_advance(cpu, 1u), "apply OC PMD while synchronization is pending");
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
           "PMD holds pending input capture synchronization");
    dspic33_write_word(cpu, 0x0762u, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "apply OC PMD re-enable before synchronization");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(cpu, 0x0146u) &&
               dspic33_read_word(cpu, 0x0908u) == 5u &&
               (cpu->io.output_compare.sync_reset_pending & 1u) == 0u,
           "re-enabled OC adopts the current input capture phase");

    dspic33_reset(cpu, 0u);
    expect(state, prepare_compare_trigger_source(cpu, 16u),
           "prepare sleeping input capture synchronization");
    expect(state, dspic33_device_advance(cpu, 5u), "advance sleeping input capture source");
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 16u);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u),
           "queue input capture synchronization before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 5u) && dspic33_read_word(cpu, 0x0146u) == 8u &&
               dspic33_read_word(cpu, 0x099eu) == 3u &&
               (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u,
           "Sleep holds both sides of input capture synchronization");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, 0x099eu) == dspic33_read_word(cpu, 0x0146u) &&
               dspic33_read_word(cpu, 0x099eu) == 9u,
           "wake resumes and aligns input capture synchronization");

    dspic33_reset(cpu, 0u);
    expect(state, prepare_compare_trigger_source(cpu, 16u),
           "prepare copied input capture synchronization");
    expect(state, dspic33_device_advance(cpu, 5u), "advance copied input capture source");
    configure_compare_mode(cpu, 0u, 6u, 100u, 50u, 16u);
    expect(state,
           dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u) &&
               dspic33_copy(&copy, cpu),
           "copy pending input capture synchronization");
    cpu->io.input_capture.timer[0] = 20u;
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance independent copied input capture phases");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 21u && dspic33_read_word(&copy, 0x0908u) == 9u &&
               cpu->io.output_compare.sync_reset_pending == 0u &&
               copy.io.output_compare.sync_reset_pending == 0u,
           "copied OC synchronization adopts each source phase independently");

    dspic33_reset(cpu, 0u);
    expect(state, prepare_compare_trigger_source(cpu, 16u),
           "prepare batched input capture synchronization");
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 16u);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 5u) && dspic33_copy(&copy, cpu),
           "copy scheduled input capture synchronization source");
    expect(state, dspic33_device_advance(cpu, 20u), "batch advance input capture synchronization");
    for (cycle = 0u; cycle < 20u; cycle++) {
        if (!dspic33_device_advance(&copy, 1u)) {
            break;
        }
    }
    expect(state, cycle == 20u, "step input capture synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == dspic33_read_word(&copy, 0x099eu) &&
               dspic33_read_word(cpu, 0x0146u) == dspic33_read_word(&copy, 0x0146u) &&
               interrupt_flag(cpu, 15u) == interrupt_flag(&copy, 15u) &&
               cpu->events.count == copy.events.count,
           "batched input capture synchronization matches stepped execution");
    dspic33_release(&copy);
}

static void alternate_clock_batch_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    uint64_t cycle;
    expect(state, initialized, "initialize alternate-clock batch copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[2], 100u);
    dspic33_write_word(cpu, timer_controls[2], 0x8010u);
    dspic33_write_word(cpu, 0x0904u, 4u);
    dspic33_write_word(cpu, 0x0906u, 2u);
    dspic33_write_word(cpu, 0x0900u, 0x0406u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_copy(&copy, cpu), "copy prescaled alternate OC clock");
    expect(state, dspic33_device_advance(cpu, 160u), "batch advance prescaled alternate OC clock");
    for (cycle = 0u; cycle < 160u; cycle++) {
        if (!dspic33_device_advance(&copy, 1u)) {
            break;
        }
    }
    expect(state, cycle == 160u, "step prescaled alternate OC clock");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(&copy, 0x0908u) &&
               dspic33_read_word(cpu, timer_registers[2]) ==
                   dspic33_read_word(&copy, timer_registers[2]) &&
               (cpu->io.output_compare.output_high & 1u) ==
                   (copy.io.output_compare.output_high & 1u) &&
               interrupt_flag(cpu, 0u) == interrupt_flag(&copy, 0u) &&
               cpu->device_cycles == copy.device_cycles,
           "batched alternate OC clock matches stepped execution");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[0], 2u);
    dspic33_write_word(cpu, timer_controls[0], 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 11u);
    expect(state, dspic33_copy(&copy, cpu), "copy timer-synchronized OC state");
    expect(state, dspic33_device_advance(cpu, 25u), "batch advance repeated timer synchronization");
    for (cycle = 0u; cycle < 25u; cycle++) {
        if (!dspic33_device_advance(&copy, 1u)) {
            break;
        }
    }
    expect(state, cycle == 25u, "step repeated timer synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == dspic33_read_word(&copy, 0x099eu) &&
               dspic33_read_word(cpu, timer_registers[0]) ==
                   dspic33_read_word(&copy, timer_registers[0]) &&
               interrupt_flag(cpu, 15u) == interrupt_flag(&copy, 15u) &&
               cpu->events.count == copy.events.count,
           "batched timer synchronization matches stepped execution");
    dspic33_release(&copy);
}

static void timer_clock_synchronization_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t selections[] = {4u, 0u, 1u, 2u, 3u};
    uint8_t timer;
    for (timer = 0u; timer < 5u; timer++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[timer], 2u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        dspic33_write_word(cpu, 0x0904u, 100u);
        dspic33_write_word(cpu, 0x0906u, 50u);
        dspic33_write_word(cpu, 0x0900u, (uint16_t)((uint16_t)selections[timer] << 10u | 6u));
        dspic33_write_word(cpu, 0x0902u, (uint16_t)(11u + timer));
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance common timer clock to synchronization match");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == 2u &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
               "timer match requests synchronization after common clock edge");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance common clock through synchronization reset");
        expect(state, dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
               "common timer clock resets OC on following edge");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[timer], 2u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        dspic33_write_word(cpu, 0x0904u, 4u);
        dspic33_write_word(cpu, 0x0906u, 2u);
        dspic33_write_word(cpu, 0x0900u, (uint16_t)((uint16_t)selections[timer] << 10u | 6u));
        dspic33_write_word(cpu, 0x0902u, (uint16_t)(COMPARE_TRIGGER | (uint16_t)(11u + timer)));
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance common timer clock to trigger match");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == 0u &&
                   (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u,
               "timer match releases trigger without same-edge count");
        expect(state, dspic33_device_advance(cpu, 1u), "advance common timer clock after trigger");
        expect(state, dspic33_read_word(cpu, 0x0908u) == 1u,
               "common timer clock counts after trigger edge");
    }
}

static void cross_timer_source_ordering_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t source_timers[] = {0u, 1u};
    static const uint8_t clock_timers[] = {1u, 0u};
    size_t order;
    for (order = 0u; order < 2u; order++) {
        uint8_t source_timer = source_timers[order];
        uint8_t clock_timer = clock_timers[order];
        uint16_t clock_selection = clock_timer == 0u ? 0x1000u : 0u;
        uint16_t source = (uint16_t)(11u + source_timer);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[source_timer], 1u);
        dspic33_write_word(cpu, timer_periods[clock_timer], 100u);
        dspic33_write_word(cpu, timer_controls[source_timer], 0x8000u);
        dspic33_write_word(cpu, timer_controls[clock_timer], 0x8000u);
        configure_compare_mode(cpu, 0u, 6u, 100u, 50u, (uint16_t)(COMPARE_TRIGGER | source));
        dspic33_write_word(cpu, 0x0900u, (uint16_t)(clock_selection | 6u));
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
                   dspic33_read_word(cpu, 0x0908u) == 0u,
               "cross-timer trigger does not count on the source edge");
        expect(state, dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 1u,
               "cross-timer trigger counts on the following selected clock");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[source_timer], 1u);
        dspic33_write_word(cpu, timer_periods[clock_timer], 100u);
        dspic33_write_word(cpu, timer_controls[clock_timer], 0x8000u);
        configure_compare_mode(cpu, 0u, 6u, 100u, 50u, source);
        dspic33_write_word(cpu, 0x0900u, (uint16_t)(clock_selection | 6u));
        expect(state, dspic33_device_advance(cpu, 5u) && dspic33_read_word(cpu, 0x0908u) == 5u,
               "cross-timer synchronization starts from a nonzero timer");
        dspic33_write_word(cpu, timer_controls[source_timer], 0x8000u);
        expect(state,
               dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 6u &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) != 0u,
               "cross-timer synchronization remains pending on the source edge");
        expect(state,
               dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 0u &&
                   (cpu->io.output_compare.sync_reset_pending & 1u) == 0u,
               "cross-timer synchronization resets on the following selected clock");
    }
}

static void synchronization_control_cases(TestState* state, Dspic33* cpu) {
    uint16_t generation;
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 2u, 1u, 1u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance alternate self-selection period");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u,
           "alternate self-selection retains final period value");
    expect(state, dspic33_device_advance(cpu, 1u), "advance alternate self-selection rollover");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u && interrupt_flag(cpu, 0u),
           "channel-number self-selection uses secondary period");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance channel-number self-selection after rollover");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 1u &&
               (cpu->io.output_compare.sync_reset_pending & 1u) == 0u,
           "alternate self-selection does not queue a duplicate reset");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 5u, 2u, 1u, COMPARE_NO_SYNC);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 1u));
    dspic33_write_word(cpu, 0x0900u, 0u);
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u,
           "disabling OC source emits documented trigger pulse");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 29u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "queue synchronization before source replacement");
    expect(state, (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u,
           "old source leaves synchronization pending");
    dspic33_write_word(cpu, 0x0998u, 30u);
    expect(state, (cpu->io.output_compare.sync_reset_pending & 0x8000u) == 0u,
           "source replacement cancels pending synchronization");
    expect(state, dspic33_device_advance(cpu, 1u), "advance stale synchronization event");
    expect(state, dspic33_read_word(cpu, 0x099eu) == 2u && !interrupt_flag(cpu, 15u),
           "stale synchronization event cannot reset replaced source");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
    generation = cpu->io.output_compare.timer_generation[15u];
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "emit repeated trigger pulses on one cycle");
    expect(state,
           (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u &&
               cpu->io.output_compare.timer_generation[15u] == (uint16_t)(generation + 1u),
           "trigger status coalesces repeated source pulses");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, COMPARE_TRIGGER);
    dspic33_write_word(cpu, 0x0998u, (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS));
    expect(state, dspic33_device_advance(cpu, 2u), "advance software-triggered OC before clear");
    dspic33_write_word(cpu, 0x0998u, COMPARE_TRIGGER);
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 2u &&
               (cpu->io.output_compare.sync_reset_pending & 0x8000u) != 0u,
           "software trigger clear waits for the next clock reset");
    expect(state, dspic33_device_advance(cpu, 1u), "advance software trigger clear edge");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 0u &&
               (cpu->io.output_compare.sync_reset_pending & 0x8000u) == 0u &&
               (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "software trigger clear resets and holds the timer");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 2u, 1u, (uint16_t)(COMPARE_TRIGGER | 29u));
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 1u));
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 4u),
           "advance triggered OC source through its period");
    expect(state,
           (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x099eu) == 0u,
           "triggered OC period emits downstream trigger output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance downstream OC after triggered source period");
    expect(state, dspic33_read_word(cpu, 0x099eu) == 1u,
           "downstream OC counts after triggered source output");
}

static void alternate_clock_control_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 5u), "advance OC with selected timer stopped");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u,
           "stopped selected timer supplies no OC clocks");
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    expect(state, dspic33_device_advance(cpu, 1u), "start selected timer clock");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 1u, "started selected timer advances OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8040u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 3u), "advance gated selected timer while gate low");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 0u,
           "low timer gate suppresses selected OC clock");
    expect(state,
           dspic33_timer_gate(cpu, 1u, true, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_device_advance(cpu, 2u),
           "raise selected timer gate and advance");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u,
           "high timer gate supplies selected OC clocks");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8002u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_timer_pulse(cpu, 1u, 3u, 0u) && dspic33_device_advance(cpu, 0u),
           "pulse external selected timer clock");
    expect(state,
           dspic33_read_word(cpu, timer_registers[1]) == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "external timer pulses clock OC after synchronizer start");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[0], 100u);
    dspic33_write_word(cpu, timer_controls[0], 0x8002u);
    dspic33_write_word(cpu, 0x0904u, 100u);
    dspic33_write_word(cpu, 0x0906u, 50u);
    dspic33_write_word(cpu, 0x0900u, 0x1006u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    expect(state, dspic33_timer_pulse(cpu, 0u, 3u, 0u) && dspic33_device_advance(cpu, 0u),
           "pulse asynchronous Timer1 source");
    expect(state,
           dspic33_read_word(cpu, timer_registers[0]) == 2u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "asynchronous Timer1 clock is unavailable to OC");
    dspic33_write_word(cpu, timer_controls[0], 0x8006u);
    expect(state, dspic33_timer_pulse(cpu, 0u, 3u, 0u) && dspic33_device_advance(cpu, 0u),
           "pulse synchronized external Timer1 source");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u, "synchronized Timer1 clock advances OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_periods[2], 0u);
    dspic33_write_word(cpu, timer_controls[1], 0x8008u);
    configure_compare_mode(cpu, 0u, 6u, 100u, 50u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    configure_compare_mode(cpu, 1u, 6u, 100u, 50u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x090au, 0x0406u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance paired Timer2 and Timer3 clocks");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 3u && dspic33_read_word(cpu, 0x0912u) == 3u &&
               dspic33_read_word(cpu, timer_registers[1]) == 3u &&
               dspic33_read_word(cpu, timer_registers[2]) == 0u,
           "paired low and high timer selections share clock edges");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    configure_compare_mode(cpu, 0u, 6u, 100u, 50u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 1u), "advance FP clock before live source change");
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance Timer2 after live OC clock change");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 2u,
           "live FP to Timer2 change preserves timer phase without double count");
    dspic33_write_word(cpu, 0x0900u, COMPARE_FP_EDGE_PWM);
    expect(state, dspic33_device_advance(cpu, 1u), "advance FP after restoring live OC clock");
    expect(state, dspic33_read_word(cpu, 0x0908u) == 3u,
           "live Timer2 to FP change preserves timer phase");
}

static void alternate_instruction_activation_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t program[] = {0x200040u, 0x884820u, 0x200020u, 0x884830u,
                                       0x200060u, 0x884800u, 0x000000u};
    size_t index;
    bool loaded = true;
    bool ran = true;
    dspic33_reset(cpu, 0x200u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    dspic33_write_word(cpu, 0x0902u, COMPARE_SELF_SYNC);
    for (index = 0u; index < sizeof(program) / sizeof(program[0]); index++) {
        loaded = loaded &&
                 dspic33_load_program_word(cpu, 0x200u + (uint32_t)(index * 2u), program[index]);
    }
    expect(state, loaded, "load alternate-clock activation sequence");
    for (index = 0u; index < 6u; index++) {
        ran = ran && dspic33_step(cpu) == DSPIC33_RUNNING;
    }
    expect(state,
           ran && dspic33_read_word(cpu, 0x0908u) == 0u &&
               dspic33_read_word(cpu, timer_registers[1]) != 0u,
           "alternate OC enable instruction does not consume timer clocks");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0908u) == 1u,
           "alternate OC starts on first clock after enabling instruction");
}

static void trigger_instruction_transition_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t fp_program[] = {0x200c00u, 0x884810u, 0x000000u, 0x000000u,
                                          0x200800u, 0x884810u, 0x000000u};
    static const uint32_t timer_program[] = {0x200c00u, 0x884810u, 0x200800u, 0x884810u, 0x000000u};
    size_t index;
    bool loaded = true;

    dspic33_reset(cpu, 0x200u);
    configure_compare_mode(cpu, 0u, 6u, 20u, 10u, COMPARE_TRIGGER);
    for (index = 0u; index < sizeof(fp_program) / sizeof(fp_program[0]); index++) {
        loaded = loaded &&
                 dspic33_load_program_word(cpu, 0x200u + (uint32_t)(index * 2u), fp_program[index]);
    }
    expect(state, loaded, "load FP trigger-status instruction sequence");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "FP trigger set instruction does not advance OC timer");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0908u) == 1u,
           "FP trigger set takes effect on the following clock");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0908u) == 2u &&
               dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0908u) == 3u,
           "FP trigger timer advances before clear instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 3u,
           "FP trigger clear instruction preserves OC timer");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_read_word(cpu, 0x0908u) == 0u,
           "FP trigger clear resets OC timer on the following clock");

    loaded = true;
    dspic33_reset(cpu, 0x200u);
    configure_compare_mode(cpu, 0u, 6u, 20u, 10u, COMPARE_TRIGGER);
    dspic33_write_word(cpu, 0x0900u, 0x0006u);
    for (index = 0u; index < sizeof(timer_program) / sizeof(timer_program[0]); index++) {
        loaded = loaded && dspic33_load_program_word(cpu, 0x200u + (uint32_t)(index * 2u),
                                                     timer_program[index]);
    }
    expect(state, loaded && dspic33_step(cpu) == DSPIC33_RUNNING,
           "load prescaled Timer2 trigger-status instruction sequence");
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8010u);
    expect(state, dspic33_device_advance(cpu, 7u),
           "advance Timer2 prescaler before trigger set instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "Timer2 trigger set instruction does not consume selected clock");
    expect(state,
           dspic33_device_advance(cpu, 7u) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 1u,
           "Timer2 trigger set takes effect on the following selected clock");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_device_advance(cpu, 6u) &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "align Timer2 prescaler before trigger clear instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 1u,
           "Timer2 trigger clear instruction preserves OC timer");
    expect(state,
           dspic33_device_advance(cpu, 7u) && dspic33_read_word(cpu, 0x0908u) == 1u &&
               dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 0u,
           "Timer2 trigger clear resets OC timer on the following selected clock");
}

static void synchronization_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize synchronization lifecycle copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 100u, 50u, 29u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u) && dspic33_copy(&copy, cpu),
           "copy pending OC synchronization");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance original and copied synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x099eu) == 0u && dspic33_read_word(&copy, 0x099eu) == 0u &&
               interrupt_flag(cpu, 15u) && interrupt_flag(&copy, 15u) &&
               cpu->io.output_compare.sync_reset_pending ==
                   copy.io.output_compare.sync_reset_pending,
           "copied synchronization completes independently");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "set trigger state before reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.output_compare.sync_reset_pending == 0u &&
               cpu->io.output_compare.deferred_sync_pulses == 0u &&
               !cpu->io.output_compare.clock_advancing && dspic33_read_word(cpu, 0x099eu) == 0u &&
               cpu->events.count == 0u,
           "reset clears OC synchronization and trigger lifecycle");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 24u));
    dspic33_write_word(cpu, 0x0a84u, 0x8040u);
    cpu->device_cycles = UINT64_MAX;
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 0u) &&
               !dspic33_device_advance(cpu, 0u),
           "process trigger source at maximum device cycle");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0996u) & 7u) == 0u && cpu->events.count == 0u,
           "trigger reschedule failure aborts OC channel");
    dspic33_release(&copy);
}

static void channel_trigger_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, channel, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & COMPARE_TRIGGER_STATUS) == 0u,
               "channel trigger matrix starts held");
        expect(state,
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "channel trigger matrix emits source");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & COMPARE_TRIGGER_STATUS) != 0u,
               "channel trigger matrix sets hardware status");
        expect(state, dspic33_device_advance(cpu, 1u),
               "channel trigger matrix advances first count");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u,
               "channel trigger matrix counts after source");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 2u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 1u);
        dspic33_write_word(cpu, base, (uint16_t)(COMPARE_FP | COMPARE_TRIGGER_ONESHOT | 6u));
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(COMPARE_TRIGGER | 29u));
        dspic33_write_word(cpu, (uint16_t)(base + 2u),
                           (uint16_t)(COMPARE_TRIGGER | COMPARE_TRIGGER_STATUS | 29u));
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & COMPARE_TRIGGER_STATUS) == 0u,
               "one-shot channel rejects software trigger set");
        expect(state,
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "one-shot channel accepts hardware trigger");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(COMPARE_TRIGGER | 29u));
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & COMPARE_TRIGGER_STATUS) != 0u,
               "one-shot channel rejects software trigger clear");
        expect(state, dspic33_device_advance(cpu, 3u),
               "one-shot channel advances to hardware clear");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & COMPARE_TRIGGER_STATUS) == 0u,
               "one-shot channel clears trigger at rollover");
    }
}

static void trigger_source_negative_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
    dspic33_raise_interrupt(cpu, 20u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance after software INT1 flag set");
    expect(state,
           (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u &&
               dspic33_read_word(cpu, 0x099eu) == 0u,
           "software interrupt flag does not synthesize external trigger edge");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0142u, 0u);
    dspic33_write_word(cpu, 0x0140u, 0x1c23u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 16u));
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u) && dspic33_device_advance(cpu, 3u),
           "capture without interrupt advances");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "capture event without IC interrupt does not trigger OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0a84u, 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 24u));
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "comparator transition without event advances");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "comparator output without compare event does not trigger OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0360u, 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 27u));
    dspic33_write_word(cpu, 0x0360u, 0x8002u);
    dspic33_write_word(cpu, 0x0360u, 0x8000u);
    expect(state, dspic33_device_advance(cpu, 12u), "complete ADC2 conversion beside OC trigger");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "ADC2 completion does not drive ADC1 trigger source");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 1u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 11u));
    expect(state, dspic33_device_advance(cpu, 2u), "advance nonselected timer period");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "Timer2 period does not drive Timer1 trigger selection");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 15u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 30u));
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 21u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u),
           "raise unrelated interrupt beside INT2 trigger");
    expect(state, (dspic33_read_word(cpu, 0x0998u) & COMPARE_TRIGGER_STATUS) == 0u,
           "unrelated interrupt does not drive external trigger source");
}

static void coexistence_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t collision_modes[] = {1u, 5u, 6u};
    uint16_t capture_base = 0x01b8u;
    size_t index;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0142u, 0x00c0u);
    dspic33_write_word(cpu, 0x0140u, 0x1c03u);
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, dspic33_input_capture_input(cpu, 0u, true, 0u), "queue input capture beside OC");
    expect(state, dspic33_device_advance(cpu, 1u), "advance IC and OC together");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 1u && dspic33_read_word(cpu, 0x0144u) == 1u &&
               dspic33_read_word(cpu, 0x0908u) == 1u && output_is(cpu, 0u, true),
           "IC and OC advance independently");
    expect(state, dspic33_device_advance(cpu, 1u), "advance OC duty beside IC");
    expect(state,
           cpu->io.input_capture.fifo[0].count == 0u && output_is(cpu, 0u, false) &&
               dspic33_read_word(cpu, 0x0908u) == 2u,
           "IC read side effect does not disturb OC");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, capture_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, capture_base, 0x1c03u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 4u), "advance free-running OC to secondary compare");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 4u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
           "free-running OC sync output waits one clock after secondary compare");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance free-running OC synchronization output");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 5u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "free-running OC triggers downstream without resetting its timer");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance triggered capture timer after OC synchronization");
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(capture_base + 6u)) == 1u &&
               dspic33_read_word(cpu, 0x0908u) == 6u,
           "OC synchronization starts the downstream timer on the next clock");

    for (index = 0u; index < sizeof(collision_modes) / sizeof(collision_modes[0]); index++) {
        uint8_t mode = collision_modes[index];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, capture_base, 0u);
        dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
        dspic33_write_word(cpu, capture_base, 0x1c03u);
        configure_compare_mode(cpu, 0u, mode, 4u, 5u, COMPARE_NO_SYNC);
        expect(state, dspic33_device_advance(cpu, 4u),
               "advance before coincident OC compare and synchronization");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
               "coincident OC synchronization does not occur at compare value");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance coincident OC compare and synchronization");
        expect(state,
               dspic33_read_word(cpu, 0x0908u) == 5u && output_is(cpu, 0u, false) &&
                   (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
               "coincident primary match retains free-running synchronization");
        expect(state, dspic33_device_advance(cpu, 1u), "advance coincident OC output pipeline");
        expect(state, output_is(cpu, 0u, mode != 6u),
               "coincident primary match preserves documented output timing");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, capture_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, capture_base, 0x1c03u);
    configure_compare_mode(cpu, 0u, 5u, 4u, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance free-running OC to first synchronization output");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "free-running OC emits first synchronization output");
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, 0x0904u, 8u);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance before rewritten secondary synchronization");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 8u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
           "rewritten secondary compare waits for its following clock");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance rewritten secondary synchronization output");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 9u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "later secondary rewrite emits another synchronization before rollover");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, capture_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(capture_base + 2u), 0x0081u);
    dspic33_write_word(cpu, capture_base, 0x1c03u);
    configure_compare_mode(cpu, 0u, 6u, UINT16_MAX, 2u, COMPARE_NO_SYNC);
    expect(state, dspic33_device_advance(cpu, UINT16_MAX),
           "advance free-running OC to maximum secondary compare");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == UINT16_MAX &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) == 0u,
           "maximum secondary compare precedes synchronization output");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance maximum secondary compare through rollover");
    expect(state,
           dspic33_read_word(cpu, 0x0908u) == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(capture_base + 2u)) & 0x0040u) != 0u,
           "maximum secondary compare synchronizes on rollover without duplication");
}

static void lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    bool high;
    expect(state, initialized, "initialize OC copy");
    if (!initialized) {
        return;
    }

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance OC before copy");
    expect(state, dspic33_copy(&copy, cpu), "copy pending OC state");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance original and copied OC");
    expect(state,
           output_is(cpu, 0u, false) && output_is(&copy, 0u, false) &&
               dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(&copy, 0x0908u),
           "copy preserves OC event and state");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance delayed OC output before copy");
    expect(state,
           output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 2u && dspic33_copy(&copy, cpu),
           "copy delayed OC pipeline state");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance copied delayed OC output");
    expect(state,
           output_is(cpu, 0u, true) && output_is(&copy, 0u, true) && !interrupt_flag(cpu, 0u) &&
               !interrupt_flag(&copy, 0u),
           "copy preserves delayed OC output event");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 2u),
           "advance copied delayed OC interrupt");
    expect(state,
           interrupt_flag(cpu, 0u) && interrupt_flag(&copy, 0u) &&
               dspic33_read_word(cpu, 0x0908u) == dspic33_read_word(&copy, 0x0908u),
           "copy preserves delayed OC interrupt event");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance delayed OC before disable");
    dspic33_write_word(cpu, 0x0900u, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance disabled delayed OC events");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) && !interrupt_flag(cpu, 0u) &&
               cpu->events.count == 0u,
           "disable cancels delayed OC output and interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance delayed OC before restart");
    dspic33_write_byte(cpu, 0x0900u, 2u);
    expect(state,
           output_is(cpu, 0u, true) && cpu->io.output_compare.phase[0] == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u,
           "mode change restarts delayed OC pipeline");
    expect(state, dspic33_device_advance(cpu, 2u), "advance restarted OC through stale events");
    expect(state,
           output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.phase[0] == 2u,
           "stale delayed events cannot alter restarted OC");
    expect(state, dspic33_device_advance(cpu, 1u), "advance restarted OC output pipeline");
    expect(state, output_is(cpu, 0u, false) && !interrupt_flag(cpu, 0u),
           "restarted OC uses only its new delayed output");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 2u), "advance delayed OC before reset");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u), "advance reset delayed OC events");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) &&
               cpu->io.output_compare.output_high == 0u && cpu->io.output_compare.phase[0] == 0u &&
               cpu->events.count == 0u && !interrupt_flag(cpu, 0u),
           "reset cancels delayed OC output and interrupt");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_write_word(cpu, 0x0900u, 0u);
    expect(state, dspic33_device_advance(cpu, 6u), "advance disabled stale OC events");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) &&
               dspic33_read_word(cpu, 0x0908u) == 0u && !interrupt_flag(cpu, 0u),
           "disable cancels OC events and output");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_write_word(cpu, 0x0900u, 0u);
    configure_compare(cpu, 0u, 6u, 3u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance re-enabled OC across stale event");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 2u &&
               !interrupt_flag(cpu, 0u),
           "stale generation cannot alter re-enabled OC");
    expect(state, dspic33_device_advance(cpu, 1u), "advance re-enabled OC duty event");
    expect(state,
           output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 3u &&
               !interrupt_flag(cpu, 0u),
           "re-enabled OC follows new duty schedule");
    expect(state, dspic33_device_advance(cpu, 2u), "advance across stale period event");
    expect(state,
           output_is(cpu, 0u, false) && dspic33_read_word(cpu, 0x0908u) == 5u &&
               !interrupt_flag(cpu, 0u),
           "stale period cannot reset re-enabled OC");
    expect(state, dspic33_device_advance(cpu, 2u), "advance re-enabled OC period event");
    expect(state,
           output_is(cpu, 0u, true) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               interrupt_flag(cpu, 0u),
           "re-enabled OC follows new period schedule");

    dspic33_reset(cpu, 0u);
    configure_compare(cpu, 0u, 4u, 2u);
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 6u), "advance reset OC queue");
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &high) &&
               cpu->io.output_compare.output_high == 0u && cpu->events.count == 0u,
           "reset clears OC state and events");

    expect(state, !dspic33_output_compare_output(cpu, 16u, &high), "reject invalid OC channel");
    expect(state, !dspic33_output_compare_output(cpu, 0u, NULL),
           "reject null OC output destination");
    expect(state, !dspic33_output_compare_pin(cpu, 0u, &high), "reject unmapped OC pin");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0900u) & 7u) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u &&
               !dspic33_output_compare_output(cpu, 0u, &high) && cpu->events.count == 0u,
           "initial OC schedule failure aborts channel");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX - 2u;
    configure_compare(cpu, 0u, 4u, 2u);
    expect(state, !dspic33_device_advance(cpu, 2u), "OC reschedule overflow stops advance");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0900u) & 7u) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u &&
               !dspic33_output_compare_output(cpu, 0u, &high) && cpu->events.count == 0u,
           "OC reschedule failure aborts channel");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX - 3u;
    configure_compare_mode(cpu, 0u, 1u, 4u, 1u, COMPARE_SELF_SYNC);
    expect(state, !dspic33_device_advance(cpu, 1u), "delayed OC interrupt overflow stops advance");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, 0x0900u) & 7u) == 0u &&
               dspic33_read_word(cpu, 0x0908u) == 0u && cpu->io.output_compare.phase[0] == 0u &&
               !dspic33_output_compare_output(cpu, 0u, &high) && cpu->events.count == 0u &&
               !interrupt_flag(cpu, 0u),
           "delayed OC schedule failure aborts the pipeline");
    dspic33_release(&copy);
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "output-compare sync test initializes");
    if (initialized) {
        alternate_clock_cases(&state, &cpu);
        trigger_source_cases(&state, &cpu);
        trigger_one_shot_cases(&state, &cpu);
        synchronization_source_cases(&state, &cpu);
        input_capture_synchronization_cases(&state, &cpu);
        input_capture_cascade_synchronization_cases(&state, &cpu);
        input_capture_synchronization_lifecycle_cases(&state, &cpu);
        alternate_clock_batch_cases(&state, &cpu);
        timer_clock_synchronization_cases(&state, &cpu);
        cross_timer_source_ordering_cases(&state, &cpu);
        synchronization_control_cases(&state, &cpu);
        alternate_clock_control_cases(&state, &cpu);
        alternate_instruction_activation_cases(&state, &cpu);
        trigger_instruction_transition_cases(&state, &cpu);
        synchronization_lifecycle_cases(&state, &cpu);
        channel_trigger_matrix_cases(&state, &cpu);
        trigger_source_negative_cases(&state, &cpu);
        coexistence_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
