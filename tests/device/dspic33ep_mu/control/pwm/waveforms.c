#include "device/dspic33ep_mu/control/pwm/internal.h"

static void protection_cases(TestState* state, Dspic33* cpu) {
    uint8_t generator;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t address = dspic33_pwm_test_base(generator);
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0x0400u, 7u, 7u, 0x1080u);
        dspic33_write_word(cpu, (uint16_t)(address + 4u), (uint16_t)(2u << 3u));
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        expect(state, dspic33_pwm_fault(cpu, 2u, true, 0u), "schedule PWM fault");
        dspic33_device_advance(cpu, 0u);
        expect(state, !dspic33_pwm_output(cpu, generator, true), "latched fault output");
        expect(state, dspic33_pwm_test_interrupt_flag(cpu, irqs[generator]),
               "latched fault interrupt");
        expect(state, (dspic33_read_word(cpu, address) & 0x8000u) != 0u, "latched fault status");
        dspic33_pwm_fault(cpu, 2u, false, 0u);
        dspic33_device_advance(cpu, 0u);
        dspic33_write_word(cpu, address, 0x0080u);
        dspic33_device_advance(cpu, 8u);
        expect(state, dspic33_pwm_output(cpu, generator, true), "latched fault release");
    }

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0880u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((3u << 10u) | 0x0100u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "current-limit output");
    expect(state, dspic33_pwm_test_interrupt_flag(cpu, 94u), "current-limit interrupt");
    expect(state, (dspic33_read_word(cpu, 0x0c20u) & 0x4000u) != 0u, "current-limit status");
    expect(state, dspic33_read_word(cpu, 0x0c38u) == 0u, "current-limit time capture");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((4u << 3u) | 0x0005u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 4u, false, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "active-low fault");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0282u);
    dspic33_write_word(cpu, 0x0c28u, 7u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)(5u << 10u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_current_limit(cpu, 5u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, cpu->io.pwm_counter[0][0] == 0u, "current reset time base");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((6u << 3u) | 1u));
    dspic33_write_word(cpu, 0x0c3au, 0x8800u);
    dspic33_write_word(cpu, 0x0c3cu, 4u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 6u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "leading-edge fault blanking");
    dspic33_device_advance(cpu, 4u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "fault after blanking");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_pwm_test_configure_generator(cpu, 1u, 0x0400u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((6u << 3u) | 1u));
    dspic33_write_word(cpu, 0x0c3au, 0x0820u);
    dspic33_write_word(cpu, 0x0c3eu, 0x0200u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 6u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "state blanked fault");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "state blanking end");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((6u << 3u) | 1u));
    dspic33_write_word(cpu, 0x0c3au, 0x0820u);
    dspic33_write_word(cpu, 0x0c3eu, 0x0700u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 6u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "reserved state blank source");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((2u << 3u) | 1u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_pwm_fault(cpu, 2u, false, 0u);
    dspic33_device_advance(cpu, 7u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "cycle fault held before boundary");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "cycle fault released at boundary");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((3u << 10u) | 0x0100u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    dspic33_pwm_current_limit(cpu, 3u, false, 0u);
    dspic33_device_advance(cpu, 7u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "current limit held before boundary");
    dspic33_device_advance(cpu, 1u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "current limit released at boundary");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x1080u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((2u << 3u) | 3u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "disabled fault output");
    expect(state, dspic33_pwm_test_interrupt_flag(cpu, 94u), "disabled fault interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c22u, 0xc40cu);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)((3u << 10u) | 0x0100u | (2u << 3u) | 1u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "current-limit high data");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "current-limit low data");
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "fault priority high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "fault priority low");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c22u, 0xc430u);
    dspic33_write_word(cpu, 0x0c24u, (uint16_t)(0x8000u | (3u << 10u) | 0x0100u | (2u << 3u) | 1u));
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 3u);
    dspic33_pwm_current_limit(cpu, 3u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "independent current high");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "independent current low");
    dspic33_pwm_fault(cpu, 2u, true, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "independent fault high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "independent fault low");
}

static void synchronization_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0x0080u);
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 5u, "counter before sync");
    expect(state, dspic33_pwm_sync(cpu, 0u, true, 0u), "schedule PWM sync");
    dspic33_device_advance(cpu, 0u);
    expect(state, cpu->io.pwm_master_counter[0] == 0u, "rising PWM sync");
    dspic33_write_word(cpu, 0x0c00u, 0x8280u);
    dspic33_pwm_sync(cpu, 0u, false, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, cpu->io.pwm_master_counter[0] == 0u, "falling PWM sync");
    expect(state, !dspic33_pwm_sync(cpu, 2u, true, 0u), "reject PWM sync input");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 15u, 15u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0c22u, 0xc201u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "synchronized override deferred");
    dspic33_device_advance(cpu, 15u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "synchronized override before boundary");
    dspic33_device_advance(cpu, 1u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "synchronized override at boundary");
    dspic33_write_word(cpu, 0x0c22u, 0xc001u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "synchronized override release deferred");
    dspic33_device_advance(cpu, 16u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "synchronized override release boundary");

    dspic33_write_word(cpu, 0x0c22u, 0xc003u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "synchronized swap deferred");
    expect(state, !dspic33_pwm_output(cpu, 0u, false), "synchronized swap low deferred");
    dspic33_device_advance(cpu, 16u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "synchronized swap high");
    expect(state, dspic33_pwm_output(cpu, 0u, false), "synchronized swap low");

    dspic33_write_word(cpu, 0x0c22u, 0xc200u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "asynchronous override");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 31u, 1u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0x0100u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "sync output idle");
    dspic33_device_advance(cpu, 32u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "sync output pulse");
    dspic33_device_advance(cpu, 11u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "sync output pulse width");
    dspic33_device_advance(cpu, 1u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "sync output pulse end");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 31u, 1u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0x0300u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "inverted sync output idle");
    dspic33_device_advance(cpu, 32u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "inverted sync output pulse");
    expect(state, !dspic33_pwm_sync_output(cpu, 2u), "reject sync output time base");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 31u, 1u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0x0100u);
    dspic33_device_advance(cpu, 100u);
    expect(state, dspic33_pwm_sync_output(cpu, 0u), "batched sync output pulse");
    dspic33_device_advance(cpu, 8u);
    expect(state, !dspic33_pwm_sync_output(cpu, 0u), "batched sync output pulse end");
}

static void chop_and_power_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 15u, 0x0080u);
    dspic33_write_word(cpu, 0x0c1au, 0x8003u);
    dspic33_write_word(cpu, 0x0c3eu, 2u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "chop initial output");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "chop gated output");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "chop repeated output");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 15u, 0x0080u);
    dspic33_pwm_test_configure_generator(cpu, 1u, 0x0400u, 15u, 1u, 0x0080u);
    dspic33_write_word(cpu, 0x0c3eu, 0x000au);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_output(cpu, 0u, true), "PWM source chop high");
    dspic33_device_advance(cpu, 2u);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "PWM source chop low");
    dspic33_write_word(cpu, 0x0c3eu, 0x001eu);
    expect(state, !dspic33_pwm_output(cpu, 0u, true), "reserved chop source low");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0x2000u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 0u, "PWM stops in idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_write_word(cpu, 0x0c00u, 0x8000u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 5u, "PWM continues in idle");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_advance(cpu, 5u);
    expect(state, cpu->io.pwm_master_counter[0] == 5u, "PWM stops in sleep");
}

static void pmd_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;
    uint8_t generator;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        uint16_t address = dspic33_pwm_test_base(generator);
        uint16_t pmd = (uint16_t)(0x0100u << generator);
        uint16_t counter;
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0x0400u, 15u, 7u, 0x0080u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        dspic33_device_advance(cpu, 2u);
        dspic33_write_word(cpu, 0x076au, pmd);
        expect(state, cpu->io.pwm_pmd_disabled == 0u, "generator PMD waits one instruction cycle");
        expect(state, dspic33_device_advance(cpu, 1u), "advance generator PMD disable boundary");
        expect(state,
               cpu->io.pwm_pmd_disabled == (uint8_t)(2u << generator) &&
                   dspic33_read_word(cpu, address) == 0u &&
                   !dspic33_pwm_output(cpu, generator, true),
               "generator PMD disables access and output");
        counter = cpu->io.pwm_counter[generator][0];
        dspic33_write_word(cpu, (uint16_t)(address + 6u), 1u);
        dspic33_device_advance(cpu, 3u);
        expect(state, cpu->io.pwm_counter[generator][0] == counter,
               "generator PMD freezes timing state");
        dspic33_write_word(cpu, 0x076au, 0u);
        expect(state, dspic33_device_advance(cpu, 1u), "advance generator PMD enable boundary");
        expect(state,
               cpu->io.pwm_pmd_disabled == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(address + 6u)) == 7u,
               "generator PMD preserves registers across disable");
    }

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_device_advance(cpu, 2u);
    dspic33_write_word(cpu, 0x0760u, 0x0200u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance global PWM PMD disable boundary");
    expect(state,
           cpu->io.pwm_pmd_disabled == 1u && dspic33_read_word(cpu, 0x0c00u) == 0u &&
               !dspic33_pwm_output(cpu, 0u, true),
           "global PWM PMD disables complete module");
    {
        uint16_t counter = cpu->io.pwm_master_counter[0];
        dspic33_device_advance(cpu, 3u);
        expect(state, cpu->io.pwm_master_counter[0] == counter,
               "global PWM PMD freezes master time base");
    }
    dspic33_write_word(cpu, 0x0760u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pwm_pmd_disabled == 0u &&
               dspic33_read_word(cpu, 0x0c00u) == 0x8000u,
           "global PWM PMD enable restores preserved registers");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0744u, 0x3800u);
    dspic33_write_word(cpu, 0x0760u, 0x0200u);
    expect(state, dspic33_device_advance(cpu, 7u) && cpu->io.pwm_pmd_disabled == 0u,
           "DOZE scales PWM PMD instruction boundary");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.pwm_pmd_disabled == 1u,
           "PWM PMD completes at divided instruction boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0760u, 0x0200u);
    dspic33_write_word(cpu, 0x0760u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pwm_pmd_disabled == 0u &&
               cpu->events.count == 0u,
           "new PWM PMD request invalidates stale transition");

    dspic33_reset(cpu, 0u);
    cpu->io.pwm_pmd_generation[0] = 0x7fffu;
    dspic33_write_word(cpu, 0x0760u, 0x0200u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pwm_pmd_generation[0] == 0x8000u &&
               cpu->io.pwm_pmd_disabled == 1u && cpu->events.count == 0u,
           "PWM PMD transition crosses high generation bit");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, 0x0760u, 0x0200u);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && dspic33_read_word(cpu, 0x0760u) == 0u &&
               cpu->io.pwm_pmd_disabled == 0u && cpu->events.count == 0u,
           "PWM PMD scheduling failure rolls back request");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0760u, 0x0200u);
    dspic33_write_word(cpu, 0x076au, 0x3f00u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pwm_pmd_disabled == 0x7fu &&
               cpu->events.count == 0u,
           "global and generator PWM PMD transitions are independent");
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize PWM PMD copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy PWM PMD state");
        expect(state,
               copy.io.pwm_pmd_disabled == cpu->io.pwm_pmd_disabled &&
                   copy.io.pwm_pmd_generation[6] == cpu->io.pwm_pmd_generation[6],
               "PWM PMD state survives copy");
        dspic33_release(&copy);
    }
    cpu->program[0x0200u / 2u] = 0xfe0000u;
    cpu->pc = 0x0200u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.pwm_pmd_disabled == 0u,
           "warm reset clears PWM PMD effective state");
    dspic33_reset(cpu, 0u);
    expect(state, cpu->io.pwm_pmd_disabled == 0u && cpu->io.pwm_pmd_generation[0] == 0u,
           "power-on reset clears PWM PMD lifecycle");
}

static void pin_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t ports[DSPIC33_PWM_COUNT] = {4u, 4u, 4u, 4u, 2u, 2u};
    static const uint8_t low_pins[DSPIC33_PWM_COUNT] = {0u, 2u, 4u, 6u, 1u, 3u};
    static const uint8_t high_pins[DSPIC33_PWM_COUNT] = {1u, 3u, 5u, 7u, 2u, 4u};
    bool high;
    uint8_t generator;
    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, generator, 0u, 7u, 3u, 0x0080u);
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        expect(state,
               dspic33_pwm_test_gpio_pin_is(cpu, ports[generator], high_pins[generator], true) &&
                   dspic33_pwm_test_gpio_pin_is(cpu, ports[generator], low_pins[generator], false),
               "dedicated PWM pins expose generator output");
        dspic33_device_advance(cpu, 4u);
        expect(state,
               dspic33_pwm_test_gpio_pin_is(cpu, ports[generator], high_pins[generator], false) &&
                   dspic33_pwm_test_gpio_pin_is(cpu, ports[generator], low_pins[generator], true),
               "dedicated PWM pins follow waveform transition");
        dspic33_write_word(cpu, 0x076au, (uint16_t)(0x0100u << generator));
        dspic33_device_advance(cpu, 1u);
        expect(state,
               dspic33_pwm_test_gpio_pin_is(cpu, ports[generator], high_pins[generator], false) &&
                   dspic33_pwm_test_gpio_pin_is(cpu, ports[generator], low_pins[generator], false),
               "generator PMD releases dedicated PWM pins to GPIO");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e40u, 0xfffcu);
    dspic33_write_word(cpu, 0x0e44u, 1u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 7u, 3u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_test_gpio_pin_is(cpu, 4u, 0u, false), "PWM owns pin over GPIO latch");
    dspic33_write_word(cpu, 0x0c00u, 0u);
    expect(state, dspic33_pwm_test_gpio_pin_is(cpu, 4u, 0u, true),
           "disabled PWM returns pin to GPIO");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e40u, 0xfffcu);
    dspic33_write_word(cpu, 0x0e44u, 1u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0u, 7u, 3u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0x2000u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_pwm_test_gpio_pin_is(cpu, 4u, 0u, true),
           "stopped-idle PWM returns pin to GPIO");
    expect(state, dspic33_read_word(cpu, 0x0c26u) == 0u,
           "stopped-idle PWM registers are inaccessible");
    dspic33_write_word(cpu, 0x0c26u, 6u);
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_read_word(cpu, 0x0c26u) == 3u,
           "stopped-idle PWM register write is ignored");
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_pwm_test_gpio_pin_is(cpu, 4u, 0u, false),
           "sleep preserves PWM pin ownership and frozen output");

    for (generator = 0u; generator < 7u; generator++) {
        uint16_t register_address = generator < 4u
                                        ? (uint16_t)(0x06b8u + (generator / 2u) * 2u)
                                        : (uint16_t)(0x06f4u + ((generator - 4u) / 2u) * 2u);
        uint8_t shift = (generator & 1u) != 0u ? 8u : 0u;
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
        dspic33_write_word(cpu, 0x0c24u, (uint16_t)((generator << 3u) | 1u));
        dspic33_write_word(cpu, register_address, (uint16_t)(64u << shift));
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        expect(state, dspic33_gpio_drive(cpu, 3u, 1u, 1u), "drive mapped PWM fault pin");
        expect(state,
               (cpu->io.pwm_fault_inputs & ((uint32_t)1u << generator)) != 0u &&
                   !dspic33_pwm_output(cpu, 0u, true),
               "mapped PWM fault pin controls generator");
    }

    for (generator = 0u; generator < DSPIC33_PWM_COUNT; generator++) {
        static const uint16_t addresses[DSPIC33_PWM_COUNT] = {0x06ecu, 0x06eeu, 0x06eeu,
                                                              0x06f0u, 0x06f0u, 0x06f2u};
        static const uint8_t shifts[DSPIC33_PWM_COUNT] = {8u, 0u, 8u, 0u, 8u, 0u};
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, addresses[generator], (uint16_t)(64u << shifts[generator]));
        expect(state, dspic33_gpio_drive(cpu, 3u, 1u, 1u),
               "drive mapped dead-time compensation pin");
        expect(state, (cpu->io.pwm_dead_time_inputs & (uint8_t)(1u << generator)) != 0u,
               "mapped dead-time compensation pin updates input");
    }

    for (generator = 0u; generator < 2u; generator++) {
        uint16_t address = generator == 0u ? 0x06eau : 0x06ecu;
        uint8_t shift = generator == 0u ? 8u : 0u;
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
        dspic33_pwm_test_enable_pwm(cpu, (uint16_t)(0x0080u | (generator << 4u)));
        dspic33_device_advance(cpu, 3u);
        dspic33_write_word(cpu, address, (uint16_t)(64u << shift));
        expect(state, dspic33_gpio_drive(cpu, 3u, 1u, 1u), "drive mapped PWM synchronization pin");
        expect(state, cpu->io.pwm_master_counter[0] == 0u,
               "mapped PWM synchronization pin resets time base");
    }

    for (generator = 0u; generator < DSPIC33_COMPARATOR_COUNT; generator++) {
        dspic33_reset(cpu, 0u);
        dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
        dspic33_write_word(cpu, 0x0c24u, (uint16_t)(((8u + generator) << 3u) | 1u));
        dspic33_pwm_test_enable_pwm(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(0x0a84u + generator * 8u), 0x8000u);
        expect(
            state,
            dspic33_comparator_input(cpu, generator, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
                dspic33_comparator_input(cpu, generator, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 100u,
                                         0u) &&
                dspic33_device_advance(cpu, 0u),
            "drive comparator-selected PWM fault source");
        expect(state,
               (cpu->io.pwm_fault_inputs & ((uint32_t)1u << (uint8_t)(8u + generator))) != 0u &&
                   !dspic33_pwm_output(cpu, 0u, true),
               "comparator output controls PWM fault source directly");
    }

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x0080u);
    dspic33_write_word(cpu, 0x06b8u, 1u);
    dspic33_write_word(cpu, 0x0c24u, 1u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    dspic33_write_word(cpu, 0x0a84u, 0x8000u);
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u, 0u) &&
               dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 100u, 0u) &&
               dspic33_device_advance(cpu, 0u) && (cpu->io.pwm_fault_inputs & 1u) == 0u &&
               dspic33_pwm_output(cpu, 0u, true),
           "B1 virtual comparator remap does not reach PWM fault input");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 7u, 7u, 0x1080u);
    dspic33_write_word(cpu, 0x0c24u, 1u);
    dspic33_write_word(cpu, 0x06b8u, 64u);
    dspic33_pwm_test_configure_interrupt(cpu, 0u, 0x0300u);
    cpu->program[0x0300u / 2u] = 0u;
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_gpio_drive(cpu, 3u, 1u, 1u) && !dspic33_pwm_output(cpu, 0u, true) &&
               dspic33_pwm_test_interrupt_flag(cpu, irqs[0]),
           "physical PWM fault remains asynchronous in Sleep");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->power_state == DSPIC33_POWER_ACTIVE &&
               cpu->last_interrupt == irqs[0] && cpu->pc == 0x0302u,
           "physical PWM fault wakes and executes generator interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0c02u, 1u);
    dspic33_write_word(cpu, 0x0c04u, 31u);
    dspic33_write_word(cpu, 0x0c0eu, 0x0100u);
    dspic33_write_word(cpu, 0x0c10u, 1u);
    dspic33_write_word(cpu, 0x0c12u, 31u);
    dspic33_write_word(cpu, 0x0680u, 0x2e2du);
    dspic33_pwm_test_enable_pwm(cpu, 0x0100u);
    expect(state,
           dspic33_pwm_sync_pin(cpu, 64u, &high) && !high &&
               dspic33_pwm_sync_pin(cpu, 65u, &high) && !high,
           "PPS exposes inactive primary and secondary PWM sync outputs");
    dspic33_device_advance(cpu, 32u);
    expect(state,
           dspic33_pwm_sync_pin(cpu, 64u, &high) && high && dspic33_pwm_sync_pin(cpu, 65u, &high) &&
               high,
           "PPS exposes primary and secondary PWM sync pulses");
    dspic33_write_word(cpu, 0x0e40u, 0xffffu);
    dspic33_write_word(cpu, 0x0e4eu, 0xffffu);
    dspic33_write_word(cpu, 0x0688u, 0x2d00u);
    expect(state, dspic33_pwm_sync_pin(cpu, 80u, &high) && high,
           "PWM sync PPS output overrides TRIS and analog configuration");
    dspic33_write_word(cpu, 0x0680u, 0u);
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high),
           "null PPS function disconnects PWM sync output");
    expect(state, !dspic33_pwm_sync_pin(cpu, 63u, &high),
           "PWM sync output rejects unavailable PPS pin");
    dspic33_write_word(cpu, 0x0760u, 0x0200u);
    dspic33_device_advance(cpu, 1u);
    expect(state, !dspic33_pwm_sync_pin(cpu, 80u, &high),
           "global PWM PMD releases remappable sync output");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x002du);
    dspic33_write_word(cpu, 0x0c00u, 0x2100u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state, !dspic33_pwm_sync_pin(cpu, 64u, &high),
           "stopped-idle PWM releases remappable sync output");
}

static void boundary_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized;
    expect(state, !dspic33_pwm_fault(cpu, DSPIC33_PWM_INPUT_COUNT, true, 0u),
           "reject PWM fault source");
    expect(state, !dspic33_pwm_current_limit(cpu, DSPIC33_PWM_INPUT_COUNT, true, 0u),
           "reject PWM current-limit source");
    expect(state, !dspic33_pwm_output(cpu, DSPIC33_PWM_COUNT, true), "reject PWM output generator");
    expect(state, !dspic33_pwm_dead_time(cpu, DSPIC33_PWM_COUNT, true, 0u),
           "reject PWM dead-time generator");

    dspic33_reset(cpu, 0u);
    dspic33_pwm_test_configure_generator(cpu, 0u, 0x0400u, 15u, 7u, 0x0080u);
    dspic33_pwm_test_enable_pwm(cpu, 0u);
    expect(state, dspic33_pwm_fault(cpu, 7u, true, 3u), "delayed PWM fault");
    dspic33_device_advance(cpu, 2u);
    expect(state, (cpu->io.pwm_fault_inputs & (1u << 7u)) == 0u, "PWM fault before delay");
    dspic33_device_advance(cpu, 1u);
    expect(state, (cpu->io.pwm_fault_inputs & (1u << 7u)) != 0u, "PWM fault after delay");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize PWM copy");
    if (!initialized) {
        return;
    }
    expect(state, dspic33_copy(&copy, cpu), "copy PWM state");
    expect(state, copy.io.pwm_master_counter[0] == cpu->io.pwm_master_counter[0],
           "copy PWM counter");
    expect(state, copy.io.pwm_fault_inputs == cpu->io.pwm_fault_inputs, "copy PWM inputs");
    expect(state, copy.io.pwm[0] == cpu->io.pwm[0], "copy PWM output");
    dspic33_device_advance(cpu, 4u);
    dspic33_device_advance(&copy, 4u);
    expect(state, copy.io.pwm_master_counter[0] == cpu->io.pwm_master_counter[0],
           "advance copied PWM counter");
    dspic33_release(&copy);

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_pwm_fault(cpu, 2u, true, 0u) && dspic33_pwm_current_limit(cpu, 3u, true, 0u) &&
               dspic33_pwm_dead_time(cpu, 0u, true, 0u) && dspic33_pwm_sync(cpu, 0u, true, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "assert direct PWM inputs before warm reset");
    cpu->program[0x0200u / 2u] = 0xfe0000u;
    cpu->pc = 0x0200u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute warm reset with direct PWM inputs");
    expect(state,
           cpu->io.pwm_fault_inputs == ((uint32_t)1u << 2u) &&
               cpu->io.pwm_current_limit_inputs == ((uint32_t)1u << 3u) &&
               cpu->io.pwm_dead_time_inputs == 1u && cpu->io.pwm_sync_inputs == 1u &&
               cpu->io.pwm_fault_direct == ((uint32_t)1u << 2u) &&
               cpu->io.pwm_current_limit_direct == ((uint32_t)1u << 3u) &&
               cpu->io.pwm_dead_time_direct == 1u && cpu->io.pwm_sync_direct == 1u,
           "warm reset preserves direct PWM input levels");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x06b8u, 64u);
    expect(state, dspic33_gpio_drive(cpu, 3u, 1u, 1u) && (cpu->io.pwm_fault_inputs & 1u) != 0u,
           "assert mapped PWM fault before warm reset");
    cpu->program[0x0200u / 2u] = 0xfe0000u;
    cpu->pc = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && (cpu->io.pwm_fault_inputs & 1u) == 0u &&
               cpu->io.pwm_fault_direct == 0u,
           "warm reset recomputes PWM input from reset PPS routing");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize PWM processor");
    if (!initialized) {
        fprintf(stderr, "cannot initialize PWM test simulator\n");
        return 1;
    }
    dspic33_pwm_test_register_cases(&state, &cpu);
    dspic33_pwm_test_clock_cases(&state, &cpu);
    dspic33_pwm_test_mode_cases(&state, &cpu);
    dspic33_pwm_test_duty_cases(&state, &cpu);
    dspic33_pwm_test_dead_time_cases(&state, &cpu);
    dspic33_pwm_test_b1_dead_time_cases(&state, &cpu);
    dspic33_pwm_test_update_cases(&state, &cpu);
    dspic33_pwm_test_b1_update_cases(&state, &cpu);
    dspic33_pwm_test_trigger_cases(&state, &cpu);
    protection_cases(&state, &cpu);
    synchronization_cases(&state, &cpu);
    chop_and_power_cases(&state, &cpu);
    pmd_cases(&state, &cpu);
    pin_cases(&state, &cpu);
    boundary_cases(&state, &cpu);
    dspic33_release(&cpu);
    return test_finish(&state);
}
