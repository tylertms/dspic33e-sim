#include "output_compare_test_support.h"

static void fault_cycle_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    uint8_t channel;
    uint8_t source;
    for (mode = 6u; mode <= 7u; mode++) {
        for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
            uint16_t base = compare_base(channel);
            for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
                bool normal_high = mode == 6u;
                bool fault_high = mode == 7u;
                dspic33_reset(cpu, 0u);
                configure_fault_compare(cpu, channel, mode, source,
                                        fault_high ? COMPARE_FAULT_OUTPUT : 0u);
                expect(state,
                       output_is(cpu, channel, normal_high) &&
                           (dspic33_read_word(cpu, base) &
                            (COMPARE_FAULT_STATUS_A | COMPARE_FAULT_STATUS_B |
                             COMPARE_FAULT_STATUS_C)) == 0u,
                       "inactive OC fault source preserves PWM output");
                expect(state,
                       drive_compare_fault(cpu, source, false) &&
                           output_is(cpu, channel, fault_high) &&
                           (dspic33_read_word(cpu, base) & compare_fault_status(source)) != 0u &&
                           (cpu->io.output_compare.fault_held & (uint16_t)(1u << channel)) != 0u &&
                           interrupt_flag(cpu, channel),
                       "active-low OC fault forces output status and interrupt");
                clear_interrupt(cpu, channel);
                expect(state,
                       drive_compare_fault(cpu, source, true) &&
                           output_is(cpu, channel, fault_high) &&
                           (dspic33_read_word(cpu, base) & compare_fault_status(source)) != 0u &&
                           !interrupt_flag(cpu, channel),
                       "cycle fault release waits for PWM boundary");
                expect(state,
                       dspic33_device_advance(cpu, 5u) && output_is(cpu, channel, normal_high) &&
                           (dspic33_read_word(cpu, base) & compare_fault_status(source)) == 0u &&
                           (cpu->io.output_compare.fault_held & (uint16_t)(1u << channel)) == 0u,
                       "cycle fault clears at the next PWM boundary");
            }
        }
    }
}

static void fault_inactive_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t source;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
            dspic33_reset(cpu, 0u);
            configure_fault_compare(cpu, channel, 6u, source, COMPARE_FAULT_INACTIVE);
            expect(state,
                   drive_compare_fault(cpu, source, false) &&
                       drive_compare_fault(cpu, source, true) && output_is(cpu, channel, false) &&
                       (dspic33_read_word(cpu, base) & compare_fault_status(source)) != 0u,
                   "inactive-mode fault remains latched after source release");
            dspic33_write_word(
                cpu, base,
                (uint16_t)(dspic33_read_word(cpu, base) & ~compare_fault_status(source)));
            expect(state,
                   output_is(cpu, channel, false) &&
                       (dspic33_read_word(cpu, base) & compare_fault_status(source)) == 0u &&
                       (cpu->io.output_compare.fault_held & (uint16_t)(1u << channel)) != 0u,
                   "inactive-mode software clear waits for PWM boundary");
            expect(state,
                   dspic33_device_advance(cpu, 4u) && output_is(cpu, channel, false) &&
                       (cpu->io.output_compare.fault_held & (uint16_t)(1u << channel)) != 0u,
                   "inactive-mode fault stays held through period value");
            expect(state,
                   dspic33_device_advance(cpu, 1u) && output_is(cpu, channel, true) &&
                       (cpu->io.output_compare.fault_held & (uint16_t)(1u << channel)) == 0u,
                   "inactive-mode fault releases when a new period starts");
        }
    }
}

static void fault_control_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    expect(state,
           drive_compare_fault(cpu, 0u, false) && dspic33_device_advance(cpu, 5u) &&
               output_is(cpu, 0u, false) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u,
           "active cycle fault persists across PWM boundaries");
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) & ~COMPARE_FAULT_STATUS_A));
    expect(state,
           (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u &&
               output_is(cpu, 0u, false),
           "active source hardware reasserts software-cleared status");
    expect(state,
           drive_compare_fault(cpu, 0u, true) && dspic33_device_advance(cpu, 5u) &&
               output_is(cpu, 0u, true) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) == 0u,
           "released cycle fault clears at a later boundary");

    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_C));
    expect(
        state,
        drive_compare_fault(cpu, 0u, false) && drive_compare_fault(cpu, 2u, false) &&
            (dspic33_read_word(cpu, 0x0900u) & (COMPARE_FAULT_STATUS_A | COMPARE_FAULT_STATUS_C)) ==
                (COMPARE_FAULT_STATUS_A | COMPARE_FAULT_STATUS_C),
        "multiple OC fault sources retain independent status");
    expect(state,
           drive_compare_fault(cpu, 0u, true) && dspic33_device_advance(cpu, 5u) &&
               output_is(cpu, 0u, false) &&
               (dspic33_read_word(cpu, 0x0900u) &
                (COMPARE_FAULT_STATUS_A | COMPARE_FAULT_STATUS_C)) == COMPARE_FAULT_STATUS_C,
           "cycle boundary clears only released fault sources");
    expect(state,
           drive_compare_fault(cpu, 2u, true) && dspic33_device_advance(cpu, 5u) &&
               output_is(cpu, 0u, true) &&
               (dspic33_read_word(cpu, 0x0900u) &
                (COMPARE_FAULT_STATUS_A | COMPARE_FAULT_STATUS_C)) == 0u,
           "last released source restores PWM output");

    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    expect(state,
           drive_compare_fault(cpu, 1u, false) && output_is(cpu, 0u, true) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_B) == 0u &&
               !interrupt_flag(cpu, 0u),
           "disabled OC fault source is ignored");
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_B));
    expect(state,
           output_is(cpu, 0u, false) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_B) != 0u &&
               interrupt_flag(cpu, 0u),
           "enabling a low OC fault source applies it immediately");

    for (mode = 1u; mode <= 5u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_fault_compare(cpu, 0u, mode, 0u, 0u);
        expect(state,
               drive_compare_fault(cpu, 0u, false) &&
                   (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) == 0u &&
                   cpu->io.output_compare.fault_held == 0u && !interrupt_flag(cpu, 0u),
               "non-PWM OC mode ignores fault controls");
    }

    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, (uint16_t)(COMPARE_FAULT_OUTPUT | 0x1000u));
    dspic33_write_word(cpu, 0x0698u, 0x0010u);
    expect(state, output_is(cpu, 0u, false) && pin_is(cpu, 109u, false),
           "OC inversion applies only to the normal PWM output");
    expect(state,
           drive_compare_fault(cpu, 0u, false) && output_is(cpu, 0u, true) &&
               pin_is(cpu, 109u, true),
           "fault output level bypasses OC inversion");
    dspic33_write_word(cpu, 0x0902u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0902u) | COMPARE_FAULT_TRISTATE));
    {
        bool high;
        expect(state, output_is(cpu, 0u, true) && !dspic33_output_compare_pin(cpu, 109u, &high),
               "FLTTRIEN disconnects a faulted PPS output");
    }
    dspic33_write_word(cpu, 0x0902u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0902u) & ~COMPARE_FAULT_TRISTATE));
    expect(state, pin_is(cpu, 109u, true), "clearing FLTTRIEN restores the selected fault level");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_byte(cpu, 0x0900u,
                       (uint8_t)(dspic33_read_byte(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    expect(state,
           drive_compare_fault(cpu, 0u, false) &&
               (dspic33_read_byte(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u,
           "low-byte OC fault enable and status use the documented lane");
    expect(state, drive_compare_fault(cpu, 0u, true), "release low-byte OC fault input");
    dspic33_write_byte(cpu, 0x0901u,
                       (uint8_t)(dspic33_read_byte(cpu, 0x0901u) | (COMPARE_FAULT_ENABLE_C >> 8u)));
    expect(state,
           drive_compare_fault(cpu, 2u, false) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_C) != 0u &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_ENABLE_A) != 0u,
           "high-byte OC fault enable preserves low-byte controls");

    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 1u, 0u);
    configure_fault_compare(cpu, 1u, 6u, 1u, COMPARE_FAULT_OUTPUT);
    expect(state,
           drive_compare_fault(cpu, 1u, false) && output_is(cpu, 0u, false) &&
               output_is(cpu, 1u, true) && interrupt_flag(cpu, 0u) && interrupt_flag(cpu, 1u),
           "one OC fault source fans out to every enabled channel");
}

static void fault_pps_cases(TestState* state, Dspic33* cpu) {
    uint8_t source;
    for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
        uint8_t pin = (uint8_t)(64u + source);
        uint16_t base = compare_base(0u);
        uint16_t mapping = source < 2u ? (uint16_t)(pin << (source * 8u)) : pin;
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
        expect(state,
               dspic33_output_compare_fault_pin(cpu, pin, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "store physical OC fault level before live PPS mapping");
        dspic33_write_word(cpu, source < 2u ? 0x06b6u : 0x06eau, mapping);
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) | compare_fault_enable(source)));
        expect(state,
               dspic33_output_compare_fault_pin(cpu, 67u, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) && output_is(cpu, 0u, true) &&
                   (dspic33_read_word(cpu, 0x0900u) & compare_fault_status(source)) == 0u,
               "unmapped PPS pin does not drive an OC fault source");
        expect(state,
               dspic33_output_compare_fault_pin(cpu, pin, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) && output_is(cpu, 0u, false) &&
                   (dspic33_read_word(cpu, 0x0900u) & compare_fault_status(source)) != 0u,
               "RPINR routes each physical OC fault source");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    expect(state,
           output_is(cpu, 0u, false) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u,
           "default VSS virtual mapping asserts an enabled active-low OC fault");

    for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
        uint16_t base = compare_base(0u);
        uint16_t mapping =
            source < 2u ? (uint16_t)((source + 1u) << (source * 8u)) : (uint16_t)(source + 1u);
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
        expect(state,
               dspic33_comparator_input(cpu, source, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
                   dspic33_comparator_input(cpu, source, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 100u,
                                            0u) &&
                   dspic33_device_advance(cpu, 0u),
               "prepare virtual comparator OC fault source");
        dspic33_write_word(cpu, (uint16_t)(0x0a84u + source * 8u), 0x8000u);
        dspic33_write_word(cpu, source < 2u ? 0x06b6u : 0x06eau, mapping);
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) | compare_fault_enable(source)));
        expect(state,
               output_is(cpu, 0u, true) &&
                   (dspic33_read_word(cpu, base) & compare_fault_status(source)) == 0u,
               "high comparator virtual source keeps OC fault inactive");
        expect(state,
               dspic33_comparator_input(cpu, source, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
                   dspic33_device_advance(cpu, 0u) && output_is(cpu, 0u, false) &&
                   (dspic33_read_word(cpu, base) & compare_fault_status(source)) != 0u,
               "comparator virtual transition asserts mapped OC fault");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state,
           dspic33_output_compare_fault_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 0u),
           "establish high OC fault pin before output qualification");
    dspic33_write_word(cpu, 0x06b6u, 64u);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    dspic33_write_word(cpu, 0x0e30u, (uint16_t)(dspic33_read_word(cpu, 0x0e30u) & ~1u));
    expect(state,
           dspic33_output_compare_fault_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) && output_is(cpu, 0u, true) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) == 0u,
           "PPS output pin cannot drive an OC fault input");
    dspic33_write_word(cpu, 0x0e30u, (uint16_t)(dspic33_read_word(cpu, 0x0e30u) | 1u));
    expect(state,
           output_is(cpu, 0u, false) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u,
           "input qualification applies the retained physical OC fault level");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    expect(state,
           dspic33_output_compare_fault_pin(cpu, 64u, true, 0u) && dspic33_device_advance(cpu, 0u),
           "establish high physical OC fault before warm reset");
    dspic33_write_word(cpu, 0x06b6u, 64u);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    expect(state,
           output_is(cpu, 0u, true) && dspic33_load_program_word(cpu, 0u, COMPARE_OPCODE_RESET) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.output_compare.fault_inputs == 0u &&
               cpu->io.output_compare.fault_direct_mask == 0u,
           "warm reset replaces physical OC routing with reset VSS input");
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    expect(state, output_is(cpu, 0u, false),
           "reset VSS asserts OC fault after physical-route warm reset");

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
               dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 100u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "establish high comparator OC fault before warm reset");
    dspic33_write_word(cpu, 0x0a84u, 0x8000u);
    dspic33_write_word(cpu, 0x06b6u, 1u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    expect(state,
           output_is(cpu, 0u, true) && dspic33_load_program_word(cpu, 0u, COMPARE_OPCODE_RESET) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.output_compare.fault_inputs == 0u,
           "warm reset replaces virtual OC routing with reset VSS input");
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    expect(state, output_is(cpu, 0u, false),
           "reset VSS asserts OC fault after virtual-route warm reset");
    expect(state,
           !dspic33_output_compare_fault(cpu, DSPIC33_OUTPUT_COMPARE_FAULT_COUNT, false, 0u) &&
               !dspic33_output_compare_fault_pin(cpu, 0u, false, 0u) &&
               !dspic33_output_compare_fault_pin(cpu, 128u, false, 0u),
           "OC fault APIs reject invalid sources and pins");
}

static void fault_power_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint8_t source = (uint8_t)(channel % DSPIC33_OUTPUT_COMPARE_FAULT_COUNT);
        uint16_t base = compare_base(channel);
        uint16_t bit = (uint16_t)(1u << channel);
        dspic33_reset(cpu, 0u);
        configure_fault_compare(cpu, channel, 6u, source, 0u);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        dspic33_device_power_state_changed(cpu);
        expect(state,
               drive_compare_fault(cpu, source, false) && output_is(cpu, channel, false) &&
                   (dspic33_read_word(cpu, base) & compare_fault_status(source)) != 0u &&
                   !interrupt_flag(cpu, channel) &&
                   (cpu->io.output_compare.fault_interrupt_pending & bit) != 0u,
               "asynchronous OC fault remains active without waking from Sleep");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        expect(state,
               interrupt_flag(cpu, channel) &&
                   (cpu->io.output_compare.fault_interrupt_pending & bit) == 0u,
               "queued Sleep fault interrupt is raised on wake");

        dspic33_reset(cpu, 0u);
        configure_fault_compare(cpu, channel, 6u, source, 0u);
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) | COMPARE_STOP_IDLE));
        cpu->power_state = DSPIC33_POWER_IDLE;
        dspic33_device_power_state_changed(cpu);
        expect(state,
               drive_compare_fault(cpu, source, false) && output_is(cpu, channel, false) &&
                   interrupt_flag(cpu, channel) &&
                   (dspic33_read_word(cpu, base) & compare_fault_status(source)) != 0u,
               "stopped Idle OC still accepts asynchronous fault input");
    }

    dspic33_reset(cpu, 0u);
    configure_interrupt(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           drive_compare_fault(cpu, 0u, false) && dspic33_step(cpu) == DSPIC33_RUNNING &&
               cpu->power_state == DSPIC33_POWER_ACTIVE && cpu->last_interrupt == compare_irqs[0] &&
               cpu->pc == (uint32_t)(COMPARE_VECTOR + 2u),
           "Idle OC fault wakes, vectors, and executes the first handler instruction");

    dspic33_reset(cpu, 0u);
    cpu->configuration[10u] = 0x80u;
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           drive_compare_fault(cpu, 0u, false) && cpu->power_state == DSPIC33_POWER_SLEEP &&
               !interrupt_flag(cpu, 0u),
           "Sleep OC fault waits for an independent wake source");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE && interrupt_flag(cpu, 0u) &&
               cpu->io.output_compare.fault_interrupt_pending == 0u,
           "watchdog wake publishes the queued Sleep fault interrupt");
}

static void fault_pmd_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint8_t source = (uint8_t)(channel % DSPIC33_OUTPUT_COMPARE_FAULT_COUNT);
        uint16_t base = compare_base(channel);
        uint16_t pmd = compare_pmd_address(channel);
        uint16_t mask = compare_pmd_mask(channel);
        bool high;
        dspic33_reset(cpu, 0u);
        configure_fault_compare(cpu, channel, 6u, source, 0u);
        dspic33_write_word(cpu, pmd, (uint16_t)(dspic33_read_word(cpu, pmd) | mask));
        expect(state, dspic33_device_advance(cpu, 1u), "apply OC fault PMD disable boundary");
        expect(state,
               drive_compare_fault(cpu, source, false) &&
                   !dspic33_output_compare_output(cpu, channel, &high) &&
                   (compare_raw_word(cpu, base) & compare_fault_status(source)) == 0u &&
                   (cpu->io.output_compare.fault_held & (uint16_t)(1u << channel)) == 0u,
               "PMD-disabled OC retains input level without fault side effects");
        dspic33_write_word(cpu, pmd, (uint16_t)(dspic33_read_word(cpu, pmd) & ~mask));
        expect(state,
               dspic33_device_advance(cpu, 1u) && output_is(cpu, channel, false) &&
                   (dspic33_read_word(cpu, base) & compare_fault_status(source)) != 0u,
               "PMD re-enable applies a retained active fault level");
    }
}

static void fault_cascade_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    uint8_t low;
    uint8_t source;
    for (mode = 6u; mode <= 7u; mode++) {
        for (low = 0u; low < DSPIC33_OUTPUT_COMPARE_COUNT; low += 2u) {
            uint8_t high = (uint8_t)(low + 1u);
            uint16_t high_base = compare_base(high);
            bool fault_high = mode == 7u;
            for (source = 0u; source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT; source++) {
                bool value;
                dspic33_reset(cpu, 0u);
                configure_cascade(cpu, low, mode, UINT32_C(0x00010004), UINT32_C(0x00010002),
                                  COMPARE_FP, COMPARE_SELF_SYNC, false);
                dspic33_write_word(cpu, (uint16_t)(high_base + 2u),
                                   (uint16_t)(dspic33_read_word(cpu, (uint16_t)(high_base + 2u)) |
                                              (fault_high ? COMPARE_FAULT_OUTPUT : 0u)));
                dspic33_write_word(
                    cpu, high_base,
                    (uint16_t)(dspic33_read_word(cpu, high_base) | compare_fault_enable(source)));
                expect(state,
                       drive_compare_fault(cpu, source, false) &&
                           !dspic33_output_compare_output(cpu, low, &value) &&
                           output_is(cpu, high, fault_high) &&
                           (dspic33_read_word(cpu, high_base) & compare_fault_status(source)) !=
                               0u &&
                           interrupt_flag(cpu, high),
                       "cascade even half owns fault output status and interrupt");
            }
        }
    }

    dspic33_reset(cpu, 0u);
    configure_cascade(cpu, 0u, 6u, UINT32_C(0x00010004), UINT32_C(0x00010002), COMPARE_FP,
                      COMPARE_SELF_SYNC, false);
    dspic33_write_word(cpu, 0x090au,
                       (uint16_t)(dspic33_read_word(cpu, 0x090au) | COMPARE_FAULT_ENABLE_A));
    expect(state,
           drive_compare_fault(cpu, 0u, false) && drive_compare_fault(cpu, 0u, true) &&
               (cpu->io.output_compare.fault_held & 0x0002u) != 0u,
           "released cascade cycle fault remains held before the period boundary");
    expect(state,
           dspic33_device_advance(cpu, UINT32_C(0x00010005)) &&
               (cpu->io.output_compare.fault_held & 0x0002u) == 0u &&
               (dspic33_read_word(cpu, 0x090au) & COMPARE_FAULT_STATUS_A) == 0u,
           "cascade cycle fault recovers at the next 32-bit period boundary");
}

static void fault_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool copy_initialized;
    bool high;
    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    expect(state, dspic33_output_compare_fault(cpu, 0u, false, 3u),
           "schedule delayed OC fault before copy");
    copy_initialized = dspic33_initialize(&copy);
    expect(state, copy_initialized && dspic33_copy(&copy, cpu),
           "copy processor with delayed OC fault");
    if (copy_initialized) {
        expect(state,
               dspic33_device_advance(cpu, 3u) && dspic33_device_advance(&copy, 3u) &&
                   output_is(cpu, 0u, false) && output_is(&copy, 0u, false) &&
                   (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u &&
                   (dspic33_read_word(&copy, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u,
               "copied delayed OC faults complete independently");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    expect(state, drive_compare_fault(cpu, 0u, false), "activate OC fault before reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.output_compare.fault_inputs == 0u &&
               cpu->io.output_compare.fault_direct_mask == 0u &&
               cpu->io.output_compare.fault_held == 0u &&
               cpu->io.output_compare.fault_interrupt_pending == 0u &&
               !dspic33_output_compare_output(cpu, 0u, &high) && cpu->events.count == 0u,
           "reset clears OC fault state and restores default VSS inputs");

    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    expect(state,
           drive_compare_fault(cpu, 0u, false) &&
               dspic33_load_program_word(cpu, 0u, COMPARE_OPCODE_RESET) &&
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.output_compare.fault_inputs == 0u &&
               cpu->io.output_compare.fault_direct_mask == 1u,
           "warm reset preserves asserted external OC fault input state");
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0900u) | COMPARE_FAULT_ENABLE_A));
    expect(state,
           output_is(cpu, 0u, false) &&
               (dspic33_read_word(cpu, 0x0900u) & COMPARE_FAULT_STATUS_A) != 0u,
           "warm-reset retained OC fault applies after module reconfiguration");

    dspic33_reset(cpu, 0u);
    configure_fault_compare(cpu, 0u, 6u, 0u, 0u);
    expect(state, dspic33_output_compare_fault(cpu, 0u, false, 4u),
           "schedule OC fault before reset cancellation");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_device_advance(cpu, 4u) && cpu->io.output_compare.fault_held == 0u &&
               cpu->events.count == 0u,
           "reset cancels delayed OC fault input events");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(state,
           !dspic33_output_compare_fault(cpu, 0u, false, 1u) &&
               cpu->io.output_compare.fault_inputs == 0u &&
               cpu->io.output_compare.fault_direct_mask == 0u &&
               cpu->io.output_compare.fault_held == 0u && cpu->events.count == 0u,
           "OC fault schedule overflow leaves input state unchanged");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "output-compare fault test initializes");
    if (initialized) {
        fault_cycle_matrix_cases(&state, &cpu);
        fault_inactive_matrix_cases(&state, &cpu);
        fault_control_cases(&state, &cpu);
        fault_pps_cases(&state, &cpu);
        fault_power_cases(&state, &cpu);
        fault_pmd_cases(&state, &cpu);
        fault_cascade_cases(&state, &cpu);
        fault_lifecycle_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
