#include "output_compare_test_support.h"

static void cascade_pwm_cases(TestState* state, Dspic33* cpu) {
    uint8_t low;
    for (low = 0u; low < DSPIC33_OUTPUT_COMPARE_COUNT; low += 2u) {
        uint8_t high = (uint8_t)(low + 1u);
        uint16_t low_base = compare_base(low);
        uint16_t high_base = compare_base(high);
        bool value;
        dspic33_reset(cpu, 0u);
        configure_cascade(cpu, low, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                          COMPARE_FP, COMPARE_SELF_SYNC, false);
        expect(state,
               !dspic33_output_compare_output(cpu, low, &value) &&
                   output_is(cpu, high, false) && !interrupt_flag(cpu, low) &&
                   !interrupt_flag(cpu, high),
               "cascade exposes only the even output and interrupt owner");
        expect(state, dspic33_device_advance(cpu, UINT32_C(0x00010001)),
               "advance cascade before 32-bit compare");
        expect(state,
               compare_raw_word(cpu, (uint16_t)(low_base + 8u)) == 1u &&
                   compare_raw_word(cpu, (uint16_t)(high_base + 8u)) == 1u &&
                   output_is(cpu, high, false),
               "cascade carries low timer overflow into the high timer");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance cascade 32-bit compare");
        expect(state,
               compare_raw_word(cpu, (uint16_t)(low_base + 8u)) == 2u &&
                   compare_raw_word(cpu, (uint16_t)(high_base + 8u)) == 1u &&
                   output_is(cpu, high, true) && !interrupt_flag(cpu, high),
               "cascade compare drives the even output high");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance cascade through period value");
        expect(state,
               compare_raw_word(cpu, (uint16_t)(low_base + 8u)) == 4u &&
                   compare_raw_word(cpu, (uint16_t)(high_base + 8u)) == 1u &&
                   output_is(cpu, high, true) && !interrupt_flag(cpu, high),
               "cascade period value is the final high output cycle");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance cascade period boundary");
        expect(state,
               compare_raw_word(cpu, (uint16_t)(low_base + 8u)) == 0u &&
                   compare_raw_word(cpu, (uint16_t)(high_base + 8u)) == 0u &&
                   output_is(cpu, high, false) && interrupt_flag(cpu, high) &&
                   !interrupt_flag(cpu, low),
               "cascade period resets both timers and raises even interrupt");
    }
}

static void cascade_mode_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 1u; mode <= 7u; mode++) {
        uint32_t primary = mode >= 6u ? UINT32_C(0x00010002) : 2u;
        uint32_t secondary = mode >= 6u ? UINT32_C(0x00010004) : 4u;
        bool initial_high = mode == 2u;
        dspic33_reset(cpu, 0u);
        configure_cascade(cpu, 0u, mode, secondary, primary, COMPARE_FP,
                          COMPARE_SELF_SYNC, false);
        expect(state, output_is(cpu, 1u, initial_high),
               "cascade mode initializes the even output");
        expect(state, dspic33_device_advance(cpu, primary),
               "advance cascade mode to primary compare");
        expect(state,
               compare_raw_word(cpu, 0x0908u) == (uint16_t)primary &&
                   compare_raw_word(cpu, 0x0912u) == (uint16_t)(primary >> 16u) &&
                   output_is(cpu, 1u, mode == 2u || mode == 6u) &&
                   !interrupt_flag(cpu, 0u) && !interrupt_flag(cpu, 1u),
               "cascade mode detects its 32-bit primary compare");
        if (mode != 6u) {
            expect(state, dspic33_device_advance(cpu, 1u),
                   "advance cascade primary output pipeline");
            expect(state, output_is(cpu, 1u, mode != 2u) && !interrupt_flag(cpu, 1u),
                   "cascade primary pipeline updates only the even output");
        }
        if (mode >= 4u && mode != 6u) {
            expect(state, dspic33_device_advance(cpu, secondary - primary - 1u),
                   "advance cascade mode to secondary compare");
            expect(state,
                   compare_raw_word(cpu, 0x0908u) == (uint16_t)secondary &&
                       cpu->io.output_compare.phase[0] != 1u,
                   "cascade mode detects its 32-bit secondary compare");
            expect(state, dspic33_device_advance(cpu, 1u),
                   "advance cascade secondary output pipeline");
            expect(state, output_is(cpu, 1u, false),
                   "cascade secondary pipeline lowers the even output");
        }
    }
}

static void cascade_configuration_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t low_control1;
        uint16_t low_control2;
        uint16_t high_control1;
        uint16_t high_control2;
    } invalid[] = {
        {0x1c06u, 0x013fu, 0x0000u, 0x001fu}, {0x1c06u, 0x011fu, 0x1c06u, 0x011fu},
        {0x1c06u, 0x013fu, 0x1c06u, 0x013fu}, {0x1c06u, 0x013fu, 0x1c06u, 0x019fu},
        {0x1c06u, 0x013fu, 0x1806u, 0x011fu}, {0x1c06u, 0x013fu, 0x1c05u, 0x011fu},
        {0x1c06u, 0x013fu, 0x1c06u, 0x011eu},
    };
    size_t index;
    for (index = 0u; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        bool high;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0902u, invalid[index].low_control2);
        dspic33_write_word(cpu, 0x090cu, invalid[index].high_control2);
        dspic33_write_word(cpu, 0x090au, invalid[index].high_control1);
        dspic33_write_word(cpu, 0x0900u, invalid[index].low_control1);
        expect(state,
               !dspic33_output_compare_output(cpu, 0u, &high) &&
                   !dspic33_output_compare_output(cpu, 1u, &high) &&
                   cpu->events.count == 0u,
               "invalid cascade pairing remains inactive");
        expect(state, dspic33_device_advance(cpu, 8u),
               "advance invalid cascade pairing");
        expect(state,
               compare_raw_word(cpu, 0x0908u) == 0u &&
                   compare_raw_word(cpu, 0x0912u) == 0u && !interrupt_flag(cpu, 0u) &&
                   !interrupt_flag(cpu, 1u),
               "invalid cascade pairing cannot count or interrupt");
    }

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance cascade before live mismatch");
    dspic33_write_word(cpu, 0x090au, 0x1806u);
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u && !output_is(cpu, 1u, true),
           "live cascade mismatch stops both timer halves");
    dspic33_write_word(cpu, 0x090au, 0x1c06u);
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u && output_is(cpu, 1u, false),
           "restoring a valid cascade restarts both timer halves");
}

static void cascade_buffering_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance cascade before buffered writes");
    dspic33_write_word(cpu, 0x0904u, 6u);
    dspic33_write_word(cpu, 0x090eu, 2u);
    dspic33_write_word(cpu, 0x0906u, 3u);
    dspic33_write_word(cpu, 0x0910u, 2u);
    expect(state,
           cpu->io.output_compare.active_rs[0] == 4u &&
               cpu->io.output_compare.active_rs[1] == 1u &&
               cpu->io.output_compare.active_r[0] == 2u &&
               cpu->io.output_compare.active_r[1] == 1u,
           "cascade PWM buffers both halves of R and RS");
    expect(state, dspic33_device_advance(cpu, UINT32_C(0x00010001)),
           "advance cascade to old buffered compare");
    expect(state, output_is(cpu, 1u, true),
           "cascade PWM uses the old 32-bit compare before rollover");
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance cascade through old buffered period");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u &&
               cpu->io.output_compare.active_rs[0] == 6u &&
               cpu->io.output_compare.active_rs[1] == 2u &&
               cpu->io.output_compare.active_r[0] == 3u &&
               cpu->io.output_compare.active_r[1] == 2u && output_is(cpu, 1u, false),
           "cascade PWM loads both buffered halves at the old period boundary");
    clear_interrupt(cpu, 1u);
    expect(state, dspic33_device_advance(cpu, UINT32_C(0x00020003)),
           "advance cascade to new buffered compare");
    expect(state, output_is(cpu, 1u, true) && !interrupt_flag(cpu, 1u),
           "cascade PWM applies the new 32-bit compare after rollover");

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 1u, UINT32_C(0x00030000), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance non-PWM cascade before immediate write");
    dspic33_write_word(cpu, 0x0910u, 2u);
    expect(state,
           cpu->io.output_compare.active_r[0] == 2u &&
               cpu->io.output_compare.active_r[1] == 2u,
           "non-PWM cascade applies a high-half compare write immediately");
    expect(state, dspic33_device_advance(cpu, UINT32_C(0x00020001)),
           "advance non-PWM cascade to rewritten compare");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 2u &&
               compare_raw_word(cpu, 0x0912u) == 2u && output_is(cpu, 1u, false),
           "non-PWM cascade reschedules the full 32-bit compare");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance non-PWM cascade output pipeline");
    expect(state, output_is(cpu, 1u, true),
           "rewritten non-PWM cascade drives the even output");
}

static void cascade_trigger_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_compare_source(cpu, 0u, 1u, 1u, COMPARE_SELF_SYNC);
    configure_cascade(cpu, 2u, 1u, 10u, 2u, COMPARE_FP, 1u, true);
    expect(state,
           (compare_raw_word(cpu, 0x0916u) & COMPARE_TRIGGER_STATUS) == 0u &&
               compare_raw_word(cpu, 0x091cu) == 0u &&
               compare_raw_word(cpu, 0x0926u) == 0u,
           "triggered cascade holds both timer halves in reset");
    expect(state, dspic33_device_advance(cpu, 2u), "advance source to cascade trigger");
    expect(state,
           (compare_raw_word(cpu, 0x0916u) & COMPARE_TRIGGER_STATUS) != 0u &&
               compare_raw_word(cpu, 0x091cu) == 0u &&
               compare_raw_word(cpu, 0x0926u) == 0u,
           "cascade trigger releases without counting on the source edge");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance first cascade clock after trigger");
    expect(state,
           compare_raw_word(cpu, 0x091cu) == 1u && compare_raw_word(cpu, 0x0926u) == 0u,
           "triggered cascade begins on the following selected clock");
    expect(state, dspic33_device_advance(cpu, 1u), "advance triggered cascade compare");
    expect(state, cpu->io.output_compare.phase[2] == 2u,
           "triggered cascade detects the paired compare");

    dspic33_reset(cpu, 0u);
    configure_compare_source(cpu, 0u, 1u, 1u, COMPARE_SELF_SYNC);
    configure_cascade(cpu, 2u, 1u, 0u, 5u, COMPARE_FP, 1u, false);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance synchronized cascade before source");
    expect(state, compare_raw_word(cpu, 0x091cu) == 1u,
           "synchronized cascade counts before the source event");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance synchronized cascade source event");
    expect(state,
           (cpu->io.output_compare.sync_reset_pending & (1u << 2u)) != 0u &&
               compare_raw_word(cpu, 0x091cu) == 2u,
           "cascade synchronization waits for the next selected clock");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance cascade synchronization boundary");
    expect(state,
           compare_raw_word(cpu, 0x091cu) == 0u &&
               compare_raw_word(cpu, 0x0926u) == 0u &&
               (cpu->io.output_compare.sync_reset_pending & (1u << 2u)) == 0u,
           "cascade synchronization resets both timer halves together");
}

static void cascade_power_cases(TestState* state, Dspic33* cpu) {
    uint8_t member;
    bool high;
    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 1u), "advance cascade before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_device_advance(cpu, 16u), "advance cascade through Sleep");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 1u && compare_raw_word(cpu, 0x0912u) == 0u,
           "Sleep holds both cascade timer halves");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 1u) && compare_raw_word(cpu, 0x0908u) == 2u,
           "cascade resumes from retained Sleep phase");

    for (member = 0u; member < 2u; member++) {
        uint16_t base = compare_base(member);
        dspic33_reset(cpu, 0u);
        configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                          COMPARE_FP, COMPARE_SELF_SYNC, false);
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance cascade before pair OCSIDL");
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) | 0x2000u));
        cpu->power_state = DSPIC33_POWER_IDLE;
        dspic33_device_power_state_changed(cpu);
        expect(state, dspic33_device_advance(cpu, 8u),
               "advance cascade pair through OCSIDL");
        expect(state, compare_raw_word(cpu, 0x0908u) == 1u && output_is(cpu, 1u, false),
               "OCSIDL on either cascade half pauses the pair");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        expect(state,
               dspic33_device_advance(cpu, 1u) && compare_raw_word(cpu, 0x0908u) == 2u,
               "cascade resumes after leaving OCSIDL");
    }

    for (member = 0u; member < 2u; member++) {
        uint16_t pmd_address = compare_pmd_address(member);
        uint16_t pmd_mask = compare_pmd_mask(member);
        dspic33_reset(cpu, 0u);
        configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                          COMPARE_FP, COMPARE_SELF_SYNC, false);
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance cascade before pair PMD disable");
        dspic33_write_word(cpu, pmd_address,
                           (uint16_t)(dspic33_read_word(cpu, pmd_address) | pmd_mask));
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance cascade pair PMD disable boundary");
        expect(state, dspic33_device_advance(cpu, 8u), "advance disabled cascade pair");
        expect(state,
               compare_raw_word(cpu, 0x0908u) == 2u &&
                   compare_raw_word(cpu, 0x0912u) == 0u &&
                   !dspic33_output_compare_output(cpu, 1u, &high),
               "disabling either cascade half pauses the pair");
        dspic33_write_word(cpu, pmd_address,
                           (uint16_t)(dspic33_read_word(cpu, pmd_address) & ~pmd_mask));
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance cascade pair PMD enable boundary");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   compare_raw_word(cpu, 0x0908u) == 3u && output_is(cpu, 1u, false),
               "cascade resumes only after both halves are PMD-enabled");
    }
}

static void cascade_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool copy_initialized;
    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 1u), "advance cascade before copy");
    copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized && dspic33_copy(&copy, cpu),
           "copy active cascade state");
    if (copy_initialized) {
        expect(state,
               dspic33_device_advance(cpu, 2u) && dspic33_device_advance(&copy, 3u) &&
                   compare_raw_word(cpu, 0x0908u) == 3u &&
                   compare_raw_word(&copy, 0x0908u) == 4u,
               "copied cascade timers advance independently");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 1u), "advance cascade before reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u &&
               cpu->io.output_compare.output_high == 0u && cpu->events.count == 0u,
           "reset clears cascade timers output and events");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (compare_raw_word(cpu, 0x0900u) & 7u) == 0u &&
               (compare_raw_word(cpu, 0x090au) & 7u) == 0u &&
               compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u && cpu->events.count == 0u,
           "cascade schedule failure atomically aborts both halves");
}

static void cascade_clock_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t clocks[] = {0x0000u, 0x0400u, 0x0800u, 0x0c00u, 0x1000u};
    static const uint8_t timers[] = {1u, 2u, 3u, 4u, 0u};
    size_t index;
    for (index = 0u; index < sizeof(clocks) / sizeof(clocks[0]); index++) {
        uint8_t timer = timers[index];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[timer], UINT16_MAX);
        dspic33_write_word(cpu, timer_controls[timer], 0x8000u);
        configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                          clocks[index], COMPARE_SELF_SYNC, false);
        expect(state, dspic33_device_advance(cpu, UINT32_C(0x00010001)),
               "advance alternate-clock cascade before compare");
        expect(state,
               compare_raw_word(cpu, 0x0908u) == 1u &&
                   compare_raw_word(cpu, 0x0912u) == 1u && output_is(cpu, 1u, false),
               "alternate clock carries the cascade low timer into high timer");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance alternate-clock cascade compare");
        expect(state,
               compare_raw_word(cpu, 0x0908u) == 2u &&
                   compare_raw_word(cpu, 0x0912u) == 1u && output_is(cpu, 1u, true),
               "alternate clock drives the cascaded even output");
    }

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 1u, 0u, UINT32_C(0xfffffffe), COMPARE_FP,
                      COMPARE_NO_SYNC, false);
    expect(state, dspic33_device_advance(cpu, UINT32_C(0xfffffffe)),
           "advance free-running cascade to maximum compare");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0xfffeu &&
               compare_raw_word(cpu, 0x0912u) == 0xffffu &&
               cpu->io.output_compare.phase[0] == 2u,
           "free-running cascade reaches the full 32-bit compare");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance free-running cascade through 32-bit rollover");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u && compare_raw_word(cpu, 0x0912u) == 0u,
           "free-running cascade rolls over both timer halves at 32 bits");

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, 6u, 3u, COMPARE_FP, COMPARE_SELF_SYNC, false);
    {
        Dspic33 stepped;
        bool initialized = dspic33_initialize(&stepped);
        expect(state, initialized && dspic33_copy(&stepped, cpu),
               "copy cascade for batch equivalence");
        if (initialized) {
            uint8_t tick;
            expect(state, dspic33_device_advance(cpu, 7u),
                   "advance cascade in one batch");
            for (tick = 0u; tick < 7u; tick++) {
                expect(state, dspic33_device_advance(&stepped, 1u),
                       "advance cascade one selected clock");
            }
            expect(state,
                   compare_raw_word(cpu, 0x0908u) ==
                           compare_raw_word(&stepped, 0x0908u) &&
                       compare_raw_word(cpu, 0x0912u) ==
                           compare_raw_word(&stepped, 0x0912u) &&
                       cpu->io.output_compare.output_high ==
                           stepped.io.output_compare.output_high &&
                       interrupt_flag(cpu, 1u) == interrupt_flag(&stepped, 1u),
                   "cascade batched and stepped advancement are equivalent");
            dspic33_release(&stepped);
        }
    }
}

static void cascade_pipeline_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 1u, 10u, 2u, COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance cascade single compare match");
    expect(state, output_is(cpu, 1u, false) && !interrupt_flag(cpu, 1u),
           "cascade single compare match precedes its output pipeline");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance cascade single output pipeline");
    expect(state, output_is(cpu, 1u, true) && !interrupt_flag(cpu, 1u),
           "cascade single output changes one clock after compare");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance cascade single interrupt pipeline");
    expect(state, interrupt_flag(cpu, 1u) && !interrupt_flag(cpu, 0u),
           "cascade single interrupt follows the even output by two clocks");

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 5u, 10u, 2u, COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance cascade dual primary compare");
    expect(state, dspic33_device_advance(cpu, 1u) && output_is(cpu, 1u, true),
           "cascade dual primary output is delayed one clock");
    expect(state, dspic33_device_advance(cpu, 7u),
           "advance cascade dual secondary compare");
    expect(state, output_is(cpu, 1u, true) && !interrupt_flag(cpu, 1u),
           "cascade dual secondary compare precedes its output pipeline");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance cascade dual secondary output");
    expect(state, output_is(cpu, 1u, false) && !interrupt_flag(cpu, 1u),
           "cascade dual secondary output is delayed one clock");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance cascade dual interrupt pipeline");
    expect(state, interrupt_flag(cpu, 1u) && !interrupt_flag(cpu, 0u),
           "cascade dual interrupt follows the even output by two clocks");
}

static void cascade_short_pwm_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, 4u, UINT32_C(0x00010002), COMPARE_FP,
                      COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero-even-period cascade through low RS");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 4u &&
               compare_raw_word(cpu, 0x0912u) == 0u && output_is(cpu, 1u, false) &&
               !interrupt_flag(cpu, 1u),
           "zero even RS ignores the low-half period value");
    expect(state, dspic33_device_advance(cpu, UINT32_C(0x0000fffe)),
           "advance zero-even-period cascade to compare");
    expect(state, output_is(cpu, 1u, true) && !interrupt_flag(cpu, 1u),
           "zero even RS raises the output at the full compare");
    expect(state, dspic33_device_advance(cpu, UINT32_MAX - UINT32_C(0x00010002)),
           "advance zero-even-period cascade to 32-bit maximum");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == UINT16_MAX &&
               compare_raw_word(cpu, 0x0912u) == UINT16_MAX &&
               output_is(cpu, 1u, true) && !interrupt_flag(cpu, 1u),
           "zero even RS holds high through the full timer range");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance zero-even-period cascade rollover");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u && output_is(cpu, 1u, true) &&
               interrupt_flag(cpu, 1u),
           "zero even RS resets only at 32-bit overflow and remains high");

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), 2u, COMPARE_FP,
                      COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance zero-even-compare cascade through low RS");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 4u &&
               compare_raw_word(cpu, 0x0912u) == 0u && output_is(cpu, 1u, true) &&
               !interrupt_flag(cpu, 1u),
           "zero even R remains high past the low-half period value");
    expect(state, dspic33_device_advance(cpu, UINT32_C(0x0000fffc)),
           "advance zero-even-compare cascade to first carry");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u && output_is(cpu, 1u, true) &&
               interrupt_flag(cpu, 1u),
           "zero even R clears the high timer on its RS match and remains high");

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, 4u, 2u, COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance all-short cascade through low period value");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 4u &&
               compare_raw_word(cpu, 0x0912u) == 0u && output_is(cpu, 1u, true) &&
               !interrupt_flag(cpu, 1u),
           "all-short cascade holds high through low RS");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance all-short cascade low-period boundary");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 1u && output_is(cpu, 1u, true) &&
               interrupt_flag(cpu, 1u),
           "all-short cascade clears low timer and carries high at first period");

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 7u, 4u, 2u, COMPARE_FP, COMPARE_SELF_SYNC, false);
    expect(state, dspic33_device_advance(cpu, 3u),
           "advance all-short center PWM output pipeline");
    expect(state, output_is(cpu, 1u, true) && !interrupt_flag(cpu, 1u),
           "all-short center PWM raises only the even output");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance all-short center PWM period boundary");
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 1u && output_is(cpu, 1u, true) &&
               interrupt_flag(cpu, 1u),
           "all-short center PWM follows cascade carry behavior");
}

static void cascade_access_activation_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t program[] = {0x21c060u, 0x884800u, 0x000000u};
    size_t index;
    bool loaded = true;
    bool value;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0904u, 4u);
    dspic33_write_word(cpu, 0x090eu, 1u);
    dspic33_write_word(cpu, 0x0906u, 2u);
    dspic33_write_word(cpu, 0x0910u, 1u);
    dspic33_write_word(cpu, 0x090cu, 0x011fu);
    dspic33_write_word(cpu, 0x0902u, 0x013fu);
    dspic33_write_word(cpu, 0x090au, 0x1c06u);
    for (index = 0u; index < sizeof(program) / sizeof(program[0]); index++) {
        loaded = loaded && dspic33_load_program_word(
                               cpu, 0x200u + (uint32_t)(index * 2u), program[index]);
    }
    cpu->pc = 0x200u;
    expect(state, loaded, "load cascade instruction activation sequence");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && compare_raw_word(cpu, 0x0908u) == 0u,
           "cascade setup literal does not activate the timer");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               compare_raw_word(cpu, 0x0908u) == 0u && output_is(cpu, 1u, false),
           "cascade enable takes effect after its instruction cycle");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && compare_raw_word(cpu, 0x0908u) == 1u,
           "cascade starts on the first clock after its enable instruction");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0904u, 4u);
    dspic33_write_word(cpu, 0x090eu, 1u);
    dspic33_write_word(cpu, 0x0906u, 2u);
    dspic33_write_word(cpu, 0x0910u, 1u);
    dspic33_write_byte(cpu, 0x090cu, 0x1fu);
    dspic33_write_byte(cpu, 0x090du, 0x01u);
    dspic33_write_byte(cpu, 0x090au, 0x06u);
    dspic33_write_byte(cpu, 0x090bu, 0x1cu);
    dspic33_write_byte(cpu, 0x0902u, 0x3fu);
    dspic33_write_byte(cpu, 0x0903u, 0x01u);
    dspic33_write_byte(cpu, 0x0900u, 0x06u);
    dspic33_write_byte(cpu, 0x0901u, 0x1cu);
    expect(state,
           !dspic33_output_compare_output(cpu, 0u, &value) &&
               output_is(cpu, 1u, false) && cpu->events.count == 1u,
           "cascade accepts documented byte-lane initialization");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance byte-configured cascade before inversion");
    dspic33_write_byte(cpu, 0x090du, 0x11u);
    expect(state,
           compare_raw_word(cpu, 0x0908u) == 1u && output_is(cpu, 1u, true) &&
               (cpu->io.output_compare.output_high & 2u) == 0u,
           "cascade inversion changes the even output without restarting the timer");
    dspic33_write_byte(cpu, 0x0903u, 0u);
    expect(state,
           output_is(cpu, 0u, true) &&
               !dspic33_output_compare_output(cpu, 1u, &value) &&
               compare_raw_word(cpu, 0x0908u) == 0u &&
               compare_raw_word(cpu, 0x0912u) == 0u,
           "clearing one byte-lane OC32 bit returns the low half to 16-bit mode");

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002),
                      COMPARE_FP, COMPARE_SELF_SYNC, false);
    dspic33_write_word(cpu, 0x0698u, 0x0010u);
    dspic33_write_word(cpu, 0x0680u, 0x1100u);
    expect(state, !dspic33_output_compare_pin(cpu, 109u, &value),
           "cascade disconnects the odd PPS output");
    expect(state, pin_is(cpu, 65u, false), "cascade exposes the even PPS output");
    expect(state, dspic33_device_advance(cpu, UINT32_C(0x00010002)),
           "advance cascaded PPS compare");
    expect(state, pin_is(cpu, 65u, true),
           "cascaded even PPS output follows the 32-bit compare");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "output-compare cascade test initializes");
    if (initialized) {
        cascade_pwm_cases(&state, &cpu);
        cascade_mode_cases(&state, &cpu);
        cascade_configuration_cases(&state, &cpu);
        cascade_buffering_cases(&state, &cpu);
        cascade_trigger_cases(&state, &cpu);
        cascade_power_cases(&state, &cpu);
        cascade_lifecycle_cases(&state, &cpu);
        cascade_clock_cases(&state, &cpu);
        cascade_pipeline_cases(&state, &cpu);
        cascade_short_pwm_cases(&state, &cpu);
        cascade_access_activation_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
