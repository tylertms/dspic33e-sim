#include "output_compare_test_support.h"

static void unsupported_cases(TestState* state, Dspic33* cpu) {
    unsupported_case(state, cpu, 0x1806u, COMPARE_SELF_SYNC, "non-FP clock is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x001cu,
                     "reserved synchronization source is excluded");
    unsupported_case(state, cpu, COMPARE_FP_EDGE_PWM, 0x009fu, "trigger mode is excluded");
    unsupported_case(state, cpu, 0x1c0eu, (uint16_t)(COMPARE_TRIGGER | 1u),
                     "one-shot self trigger is excluded");
}

static void power_state_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, channel, 6u, 4u, 2u, COMPARE_SELF_SYNC);
        dspic33_write_word(cpu, base, 0x3c06u);
        expect(state, dspic33_device_advance(cpu, 1u), "advance stop-in-idle OC before Idle");
        cpu->power_state = DSPIC33_POWER_IDLE;
        dspic33_device_power_state_changed(cpu);
        expect(state, dspic33_device_advance(cpu, 5u), "advance stopped OC through Idle");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u &&
                   output_is(cpu, channel, true) && !interrupt_flag(cpu, channel),
               "OCSIDL holds timer output and interrupt state in Idle");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                   output_is(cpu, channel, false),
               "OCSIDL resumes from the retained Idle phase");

        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, channel, 6u, 4u, 2u, COMPARE_SELF_SYNC);
        cpu->power_state = DSPIC33_POWER_IDLE;
        dspic33_device_power_state_changed(cpu);
        expect(state,
               dspic33_device_advance(cpu, 2u) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                   output_is(cpu, channel, false),
               "OCSIDL clear keeps the channel operating in Idle");

        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, channel, 6u, 4u, 2u, COMPARE_SELF_SYNC);
        expect(state, dspic33_device_advance(cpu, 1u), "advance OC before Sleep");
        cpu->power_state = DSPIC33_POWER_SLEEP;
        dspic33_device_power_state_changed(cpu);
        expect(state, dspic33_device_advance(cpu, 5u), "advance halted OC through Sleep");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u &&
                   output_is(cpu, channel, true) && !interrupt_flag(cpu, channel),
               "Sleep holds every OC timer output and interrupt state");
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u &&
                   output_is(cpu, channel, false),
               "OC resumes from the retained Sleep phase");
    }

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance running OC in Idle before OCSIDL write");
    dspic33_write_byte(cpu, 0x0901u, 0x3cu);
    expect(state, dspic33_device_advance(cpu, 4u) && dspic33_read_word(cpu, 0x0908u) == 1u,
           "live OCSIDL set halts an active Idle channel");
    dspic33_write_byte(cpu, 0x0901u, 0x1cu);
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0908u) == 2u,
           "live OCSIDL clear resumes an active Idle channel");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_controls[1], 0x8000u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, 0x0900u, 0x2006u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_read_word(cpu, 0x0908u) == 0u &&
               dspic33_read_word(cpu, timer_registers[1]) == 3u,
           "OCSIDL halts alternate-clock OC while its timer continues");
}

static void pmd_channel_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    bool high;
    for (channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        uint16_t base = compare_base(channel);
        uint16_t pmd_address = compare_pmd_address(channel);
        uint16_t pmd_mask = compare_pmd_mask(channel);
        dspic33_reset(cpu, 0u);
        configure_compare_mode(cpu, channel, 6u, 4u, 2u, COMPARE_SELF_SYNC);
        dspic33_write_word(cpu, pmd_address, pmd_mask);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 6u);
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 6u &&
                   dspic33_output_compare_output(cpu, channel, &high),
               "OC PMD write remains accessible for one cycle");
        expect(state, dspic33_device_advance(cpu, 1u), "OC PMD disable transition advances");
        expect(state, (cpu->io.output_compare.pmd_disabled & (uint16_t)(1u << channel)) != 0u,
               "OC PMD disable becomes effective after one cycle");
        expect(state,
               dspic33_read_word(cpu, base) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u &&
                   !dspic33_output_compare_output(cpu, channel, &high),
               "OC PMD disabled registers read zero and release output");
        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        expect(state, dspic33_device_advance(cpu, 5u), "advance disabled OC after ignored writes");
        dspic33_write_word(cpu, pmd_address, 0u);
        expect(state, dspic33_read_word(cpu, base) == 0u,
               "OC PMD enable remains inaccessible for one cycle");
        expect(state, dspic33_device_advance(cpu, 1u), "OC PMD enable transition advances");
        expect(state,
               (cpu->io.output_compare.pmd_disabled & (uint16_t)(1u << channel)) == 0u &&
                   dspic33_read_word(cpu, base) == COMPARE_FP_EDGE_PWM &&
                   dspic33_read_word(cpu, (uint16_t)(base + 2u)) == COMPARE_SELF_SYNC &&
                   dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 6u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 2u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 1u,
               "OC PMD enable restores preserved registers and timer phase");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 2u,
               "OC resumes after PMD enable boundary");
    }
}

static void pmd_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;
    bool high;
    uint16_t pmd_mask = compare_pmd_mask(0u);
    uint64_t device_cycles;

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 8u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 1u), "advance OC pipeline before PMD disable");
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    expect(state, dspic33_device_advance(cpu, 1u), "OC PMD disable pauses delayed output pipeline");
    expect(state,
           !output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u) && cpu->events.count == 3u &&
               cpu->events.items[0].paused && cpu->events.items[0].paused_remaining == 1u &&
               cpu->events.items[1].paused && cpu->events.items[1].paused_remaining == 3u &&
               cpu->events.items[2].paused && cpu->events.items[2].paused_remaining == 7u,
           "OC PMD preserves delayed output and interrupt events");
    expect(state, dspic33_device_advance(cpu, 10u),
           "advance paused OC pipeline while PMD disabled");
    expect(state, !dspic33_output_compare_output(cpu, 0u, &high) && !interrupt_flag(cpu, 0u),
           "OC PMD prevents paused pipeline completion");
    dspic33_write_word(cpu, compare_pmd_address(0u), 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "OC PMD enable resumes delayed output pipeline");
    expect(state,
           dspic33_output_compare_output(cpu, 0u, &high) && !high && !interrupt_flag(cpu, 0u),
           "resumed OC pipeline retains its remaining output delay");
    expect(state,
           dspic33_device_advance(cpu, 1u) && output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "resumed OC output applies after retained delay");
    expect(state, dspic33_device_advance(cpu, 2u) && interrupt_flag(cpu, 0u),
           "resumed OC interrupt applies after retained delay");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize OC PMD copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    expect(state, dspic33_copy(&copy, cpu), "copy pending OC PMD transition");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
           "advance original and copied OC PMD transition");
    expect(state,
           cpu->io.output_compare.pmd_disabled == 1u && copy.io.output_compare.pmd_disabled == 1u,
           "copy retains pending OC PMD state");
    dspic33_release(&copy);

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 2u), "advance reset OC PMD queue");
    expect(state,
           cpu->io.output_compare.pmd_disabled == 0u &&
               cpu->io.output_compare.pmd_generation[0] == 0u &&
               dspic33_read_word(cpu, compare_pmd_address(0u)) == 0u && cpu->events.count == 0u,
           "reset cancels OC PMD transition");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    dspic33_write_word(cpu, compare_pmd_address(0u), 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance replaced OC PMD transition");
    expect(state,
           cpu->io.output_compare.pmd_disabled == 0u &&
               dspic33_read_word(cpu, compare_pmd_address(0u)) == 0u,
           "new OC PMD request invalidates stale transition");

    dspic33_reset(cpu, 0u);
    cpu->io.output_compare.pmd_generation[0] = 0x7fffu;
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.output_compare.pmd_generation[0] == 0x8000u &&
               cpu->io.output_compare.pmd_disabled == 1u && cpu->events.count == 0u,
           "OC PMD disable applies across the high generation bit");
    cpu->io.output_compare.pmd_generation[0] = 0x7fffu;
    dspic33_write_word(cpu, compare_pmd_address(0u), 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.output_compare.pmd_generation[0] == 0x8000u &&
               cpu->io.output_compare.pmd_disabled == 0u && cpu->events.count == 0u,
           "OC PMD enable applies across the high generation bit");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, compare_pmd_address(0u), 0x03u);
    expect(state, cpu->events.count == 2u,
           "multi-channel OC PMD write schedules each changed channel");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               (cpu->io.output_compare.pmd_disabled & 0x0003u) == 0x0003u,
           "multi-channel OC PMD transition applies atomically at its deadline");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, (uint16_t)(COMPARE_TRIGGER | 29u));
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    expect(state, dspic33_device_advance(cpu, 1u), "disable triggered OC before external source");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u) &&
               (compare_raw_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u,
           "OC PMD drops external trigger sources while disabled");
    dspic33_write_word(cpu, compare_pmd_address(0u), 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) == 0u,
           "OC PMD re-enable does not replay missed trigger sources");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 20u, 0u, 1u) &&
               dspic33_device_advance(cpu, 1u) &&
               (dspic33_read_word(cpu, 0x0902u) & COMPARE_TRIGGER_STATUS) != 0u,
           "re-enabled OC accepts a new external trigger source");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0744u, 0x3800u);
    expect(state, dspic33_load_program_word(cpu, 0u, COMPARE_OPCODE_MOV_W0_INDIRECT_W1),
           "load stepped OC PMD write");
    dspic33_set_working_register(cpu, 0u, pmd_mask);
    dspic33_set_working_register(cpu, 1u, compare_pmd_address(0u));
    cpu->pc = 0u;
    device_cycles = cpu->device_cycles;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->device_cycles - device_cycles == 8u &&
               cpu->io.output_compare.pmd_disabled == 1u && cpu->events.count == 0u,
           "stepped OC PMD write completes after one divided instruction");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    expect(state, dspic33_device_advance(cpu, 1u),
           "establish stable OC PMD disable before warm reset");
    expect(state,
           dspic33_load_program_word(cpu, 0u, COMPARE_OPCODE_RESET) &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               dspic33_read_word(cpu, compare_pmd_address(0u)) == 0u &&
               cpu->io.output_compare.pmd_disabled == 0u && dspic33_read_word(cpu, 0x0900u) == 0u &&
               cpu->events.count == 0u,
           "warm reset clears OC PMD and channel runtime state");

    dspic33_reset(cpu, 0u);
    cpu->configuration[10u] = 0x80u;
    configure_compare_mode(cpu, 0u, 1u, 8u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 1u), "advance OC pipeline before stepped Sleep");
    expect(state, dspic33_load_program_word(cpu, 0u, COMPARE_OPCODE_SLEEP),
           "load stepped OC Sleep instruction");
    cpu->pc = 0u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_SLEEPING && cpu->power_state == DSPIC33_POWER_SLEEP &&
               cpu->events.count == 2u && cpu->events.items[0].paused &&
               cpu->events.items[0].paused_remaining == 1u && cpu->events.items[1].paused &&
               cpu->events.items[1].paused_remaining == 1u,
           "PWRSAV pauses the OC pipeline before its next clock");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, cpu->power_state == DSPIC33_POWER_ACTIVE && !cpu->events.items[0].paused,
           "watchdog wake resumes the retained OC pipeline");
    expect(state,
           dspic33_device_advance(cpu, 2u) && output_is(cpu, 0u, true) && !interrupt_flag(cpu, 0u),
           "watchdog-resumed OC completes its retained output delay");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 1u, 8u, 2u, COMPARE_SELF_SYNC);
    expect(state, dspic33_device_advance(cpu, 1u), "advance OC before resume overflow");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    cpu->device_cycles = UINT64_MAX;
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 2u &&
               cpu->events.items[0].paused && cpu->events.items[0].paused_remaining == 1u &&
               cpu->events.items[1].paused && cpu->events.items[1].paused_remaining == 1u,
           "OC resume overflow preserves the paused pipeline and reports failure");

    dspic33_reset(cpu, 0u);
    configure_compare_mode(cpu, 0u, 6u, 4u, 2u, COMPARE_SELF_SYNC);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, compare_pmd_address(0u), pmd_mask);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               dspic33_read_word(cpu, compare_pmd_address(0u)) == 0u &&
               cpu->io.output_compare.pmd_disabled == 0u && cpu->events.count == 2u &&
               dspic33_read_word(cpu, 0x0900u) == COMPARE_FP_EDGE_PWM,
           "OC PMD scheduling failure rolls back request");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "output-compare power test initializes");
    if (initialized) {
        unsupported_cases(&state, &cpu);
        power_state_cases(&state, &cpu);
        pmd_channel_cases(&state, &cpu);
        pmd_lifecycle_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
