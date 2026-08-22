#include "device/dspic33ep_mu/timing/oscillator/internal.h"

static void lifecycle_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    dspic33_reset(source, 0u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL,
                                                 OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves oscillator state");
    expect(state,
           copy->oscillator.active && copy->oscillator.generation == source->oscillator.generation,
           "copy preserves active oscillator generation");
    expect(state, copy->events.count == source->events.count,
           "copy preserves pending oscillator event");
    expect(state,
           dspic33_device_advance(source, 22u) &&
               dspic33_oscillator_test_control(source) == 0x3320u,
           "source completes copied switch interval");
    expect(state,
           dspic33_device_advance(copy, 22u) && dspic33_oscillator_test_control(copy) == 0x3320u,
           "copy independently completes switch interval");

    dspic33_reset(source, 0u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL,
                                                 OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes during oscillator switch");
    expect(state, dspic33_oscillator_test_control(source) == 0x0301u && source->oscillator.active,
           "warm reset preserves active oscillator switch");
    expect(state, source->events.count == 1u, "warm reset reconstructs pending oscillator event");
    expect(state, source->events.items[0].cycle - source->device_cycles == 20u,
           "warm reset preserves remaining oscillator interval");
    expect(state,
           dspic33_device_advance(source, 19u) &&
               dspic33_oscillator_test_control(source) == 0x0301u,
           "warm-reset switch remains pending before preserved deadline");
    expect(state,
           dspic33_device_advance(source, 1u) && dspic33_oscillator_test_control(source) == 0x3320u,
           "warm-reset switch completes at preserved deadline");

    dspic33_reset(source, 0u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL,
                                                 OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    dspic33_reset(source, 0u);
    expect(state, dspic33_oscillator_test_control(source) == 0u,
           "POR resets OSCCON from configuration");
    expect(state, !source->oscillator.active && source->events.count == 0u,
           "POR cancels pending oscillator switch");
    expect(state,
           dspic33_device_advance(source, 32u) && dspic33_oscillator_test_control(source) == 0u,
           "POR prevents stale oscillator completion");

    dspic33_reset(source, 0u);
    dspic33_oscillator_test_write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    dspic33_oscillator_test_load_sequence(source, OPCODE_MOV_BYTE_W2_W1, OPCODE_MOV_BYTE_W3_W1,
                                          OPCODE_MOV_BYTE_W0_W1);
    source->pc = 0x0200u;
    dspic33_set_working_register(source, 0u, OSCILLATOR_SWITCH_ENABLE);
    dspic33_set_working_register(source, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(source, 2u, 0x46u);
    dspic33_set_working_register(source, 3u, 0x57u);
    dspic33_step(source);
    dspic33_step(source);
    source->device_cycles = UINT64_MAX;
    dspic33_step(source);
    expect(state, source->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "switch schedule overflow reports event error");
    expect(state, dspic33_oscillator_test_control(source) == 0x0301u && source->events.count == 0u,
           "failed schedule leaves requested switch state visible");

    dspic33_reset(source, 0u);
    dspic33_oscillator_test_load_sequence(source, OPCODE_MOV_BYTE_W2_W1, OPCODE_RESET,
                                          OPCODE_MOV_BYTE_W3_W1);
    dspic33_load_program_word(source, 0x0206u, OPCODE_MOV_BYTE_W0_W1);
    source->pc = 0x0200u;
    dspic33_set_working_register(source, 0u, 0x40u);
    dspic33_set_working_register(source, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(source, 2u, 0x46u);
    dspic33_set_working_register(source, 3u, 0x57u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING, "reset invalidation first key executes");
    expect(state, dspic33_step(source) == DSPIC33_RUNNING, "reset executes between protected keys");
    source->pc = 0x0204u;
    expect(state, dspic33_step(source) == DSPIC33_RUNNING, "post-reset second key executes");
    expect(state, dspic33_step(source) == DSPIC33_RUNNING, "post-reset final write executes");
    expect(state, (dspic33_oscillator_test_control(source) & OSCILLATOR_IO_LOCK) == 0u,
           "reset invalidates protected key sequence");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    TestState state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    dspic33_load_configuration_word(&source, 0xf80008u, 0x005eu);
    dspic33_oscillator_test_reset_cases(&state, &source);
    dspic33_oscillator_test_protection_cases(&state, &source);
    dspic33_oscillator_test_switch_cases(&state, &source);
    dspic33_oscillator_test_failure_trap_cases(&state, &source);
    dspic33_oscillator_test_source_admission_matrix_cases(&state, &source);
    dspic33_oscillator_test_fail_safe_matrix_cases(&state, &source);
    dspic33_oscillator_test_configuration_admission_cases(&state, &source);
    dspic33_oscillator_test_hardware_failure_cases(&state, &source);
    dspic33_oscillator_test_pll_lock_sequence_cases(&state, &source, &copy);
    dspic33_oscillator_test_two_speed_startup_cases(&state, &source, &copy);
    dspic33_oscillator_test_reference_clock_cases(&state, &source, &copy);
    dspic33_oscillator_test_reference_clock_pin_cases(&state, &source, &copy);
    dspic33_oscillator_test_main_pll_configuration_cases(&state, &source, &copy);
    dspic33_oscillator_test_doze_cases(&state, &source, &copy);
    dspic33_oscillator_test_oscillator_pin_cases(&state, &source, &copy);
    lifecycle_cases(&state, &source, &copy);
    dspic33_release(&copy);
    dspic33_release(&source);
    return test_finish(&state);
}
