#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

enum {
    OSCILLATOR_CONTROL = 0x0742u,
    OSCILLATOR_SWITCH_ENABLE = 0x0001u,
    OSCILLATOR_LP_ENABLE = 0x0002u,
    OSCILLATOR_CLOCK_FAIL = 0x0008u,
    OSCILLATOR_PLL_LOCK = 0x0020u,
    OSCILLATOR_IO_LOCK = 0x0040u,
    OSCILLATOR_CLOCK_LOCK = 0x0080u,
    OSCILLATOR_SWITCH_DELAY = 32u,
    OPCODE_NOP = 0x000000u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_RESET = 0xfe0000u,
    OPCODE_MOV_BYTE_W0_W1 = 0x784880u,
    OPCODE_MOV_BYTE_W2_W1 = 0x784882u,
    OPCODE_MOV_BYTE_W3_W1 = 0x784883u
};

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} OscillatorConformance;

static void expect(OscillatorConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[oscillator-failed] %s\n", name);
    }
}

static uint16_t control(Dspic33* cpu) {
    return dspic33_read_word(cpu, OSCILLATOR_CONTROL);
}

static bool load_sequence(Dspic33* cpu, uint32_t first, uint32_t second,
                          uint32_t third) {
    return dspic33_load_program_word(cpu, 0x0020u, first) &&
           dspic33_load_program_word(cpu, 0x0022u, second) &&
           dspic33_load_program_word(cpu, 0x0024u, third);
}

static Dspic33StopReason write_protected_byte(Dspic33* cpu, uint16_t address,
                                              uint8_t value) {
    uint8_t first = address == OSCILLATOR_CONTROL ? 0x46u : 0x78u;
    uint8_t second = address == OSCILLATOR_CONTROL ? 0x57u : 0x9au;
    load_sequence(cpu, OPCODE_MOV_BYTE_W2_W1, OPCODE_MOV_BYTE_W3_W1,
                  OPCODE_MOV_BYTE_W0_W1);
    cpu->pc = 0x0020u;
    dspic33_set_working_register(cpu, 0u, value);
    dspic33_set_working_register(cpu, 1u, address);
    dspic33_set_working_register(cpu, 2u, first);
    dspic33_set_working_register(cpu, 3u, second);
    if (dspic33_step(cpu) != DSPIC33_RUNNING || dspic33_step(cpu) != DSPIC33_RUNNING) {
        return cpu->stop_reason;
    }
    return dspic33_step(cpu);
}

static void reset_cases(OscillatorConformance* state, Dspic33* cpu) {
    expect(state, dspic33_load_configuration_word(cpu, 0xf80006u, 0x00ffu),
           "load nondefault FNOSC configuration");
    dspic33_reset(cpu, 0u);
    expect(state, control(cpu) == 0x0700u, "POR derives NOSC from FNOSC");
    expect(state, (control(cpu) & 0x7020u) == 0u,
           "POR starts from FRC without lock status");
    expect(state, cpu->oscillator.generation == 0u && !cpu->oscillator.active,
           "POR clears oscillator lifecycle state");
    expect(state, cpu->events.count == 0u, "POR leaves no oscillator event");

    expect(state, dspic33_load_configuration_word(cpu, 0xf80006u, 0x0078u),
           "restore firmware FNOSC configuration");
    dspic33_reset(cpu, 0u);
    expect(state, control(cpu) == 0u, "firmware configuration starts from FRC");
}

static void protection_cases(OscillatorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, OSCILLATOR_CONTROL, 0xffffu);
    expect(state, control(cpu) == 0u, "word write cannot unlock OSCCON");
    dspic33_write_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u);
    expect(state, control(cpu) == 0u, "direct byte write cannot unlock OSCCON");
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u) == DSPIC33_RUNNING,
           "high-byte unlock sequence executes");
    expect(state, control(cpu) == 0x0300u, "high-byte unlock writes NOSC only");
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL, 0x42u) == DSPIC33_RUNNING,
           "low-byte unlock sequence executes");
    expect(state, control(cpu) == 0x0342u, "low-byte unlock writes IOLOCK and LPOSCEN");
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL, 0x80u) == DSPIC33_RUNNING,
           "CLKLOCK set sequence executes");
    expect(state, control(cpu) == 0x0380u, "CLKLOCK sets and other controls clear");
    expect(state, write_protected_byte(cpu, OSCILLATOR_CONTROL, 0u) == DSPIC33_RUNNING,
           "CLKLOCK clear attempt executes");
    expect(state, control(cpu) == 0x0380u, "CLKLOCK is software set-only");

    dspic33_reset(cpu, 0u);
    load_sequence(cpu, OPCODE_MOV_BYTE_W2_W1, OPCODE_NOP, OPCODE_MOV_BYTE_W3_W1);
    dspic33_load_program_word(cpu, 0x0026u, OPCODE_MOV_BYTE_W0_W1);
    cpu->pc = 0x0020u;
    dspic33_set_working_register(cpu, 0u, 0x40u);
    dspic33_set_working_register(cpu, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(cpu, 2u, 0x46u);
    dspic33_set_working_register(cpu, 3u, 0x57u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "delayed key first write");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "delayed key intervening NOP");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "delayed key second write");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "delayed key final write");
    expect(state, control(cpu) == 0u, "intervening instruction invalidates key");

    dspic33_reset(cpu, 0u);
    load_sequence(cpu, OPCODE_MOV_BYTE_W2_W1, OPCODE_MOV_BYTE_W3_W1,
                  OPCODE_MOV_BYTE_W0_W1);
    cpu->pc = 0x0020u;
    dspic33_set_working_register(cpu, 0u, 0x40u);
    dspic33_set_working_register(cpu, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(cpu, 2u, 0x46u);
    dspic33_set_working_register(cpu, 3u, 0x56u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "wrong key first write");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "wrong key second write");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "wrong key final write");
    expect(state, control(cpu) == 0u, "wrong second key blocks write");

    dspic33_reset(cpu, 0u);
    load_sequence(cpu, OPCODE_MOV_BYTE_W2_W1, OPCODE_MOV_BYTE_W3_W1,
                  OPCODE_MOV_BYTE_W0_W1);
    cpu->pc = 0x0020u;
    dspic33_set_working_register(cpu, 0u, 0x40u);
    dspic33_set_working_register(cpu, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(cpu, 2u, 0x46u);
    dspic33_set_working_register(cpu, 3u, 0x57u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "interrupt key first write");
    cpu->interrupt_count++;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "interrupt key second write");
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "interrupt key final write");
    expect(state, control(cpu) == 0u, "interrupt entry invalidates key");
}

static void switch_cases(OscillatorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->data[OSCILLATOR_CONTROL] = OSCILLATOR_CLOCK_FAIL;
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u) == DSPIC33_RUNNING,
           "firmware high-byte clock request executes");
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE) ==
               DSPIC33_RUNNING,
           "firmware low-byte switch request executes");
    expect(state, control(cpu) == 0x0301u,
           "switch starts with requested source and cleared status");
    expect(state, cpu->oscillator.active && cpu->events.count == 1u,
           "switch schedules one active event");
    expect(state, cpu->oscillator.generation == 1u,
           "switch advances oscillator generation");
    expect(state, dspic33_device_advance(cpu, OSCILLATOR_SWITCH_DELAY - 2u),
           "advance before modeled switch deadline");
    expect(state, control(cpu) == 0x0301u,
           "switch remains pending before modeled deadline");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance to modeled switch deadline");
    expect(state, control(cpu) == 0x3320u,
           "POSCPLL switch transfers COSC and sets LOCK");
    expect(state, !cpu->oscillator.active && cpu->events.count == 0u,
           "switch completion clears lifecycle state");

    dspic33_reset(cpu, 0u);
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE) ==
               DSPIC33_RUNNING,
           "redundant switch request executes");
    expect(state, control(cpu) == 0u && cpu->events.count == 0u,
           "redundant switch clears OSWEN without event");

    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, cpu->oscillator.active, "abort case starts active switch");
    expect(state, write_protected_byte(cpu, OSCILLATOR_CONTROL, 0u) == DSPIC33_RUNNING,
           "explicit switch abort executes");
    expect(state, !cpu->oscillator.active && (control(cpu) & 1u) == 0u,
           "explicit switch abort clears OSWEN");
    expect(state, dspic33_device_advance(cpu, OSCILLATOR_SWITCH_DELAY),
           "advance through stale aborted event");
    expect(state, (control(cpu) & 0x7021u) == 0u,
           "aborted event cannot change source or lock");

    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, cpu->oscillator.active, "replacement case starts first switch");
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x02u);
    expect(state, !cpu->oscillator.active && control(cpu) == 0x0200u,
           "new source request aborts active switch");
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, cpu->oscillator.active && cpu->events.count == 2u,
           "second switch retains stale event for generation check");
    expect(state, dspic33_device_advance(cpu, 25u) && control(cpu) == 0x0201u,
           "stale first completion cannot finish second switch");
    expect(state, cpu->events.count == 1u && cpu->oscillator.active,
           "stale completion preserves current switch event");
    expect(state, dspic33_device_advance(cpu, 6u) && control(cpu) == 0x2200u,
           "second non-PLL switch completes without LOCK");

    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_load_program_word(cpu, 0x0020u, OPCODE_SLEEP);
    cpu->pc = 0x0020u;
    expect(state, dspic33_step(cpu) == DSPIC33_SLEEPING,
           "Sleep executes during clock switch");
    expect(state, !cpu->oscillator.active && (control(cpu) & 1u) == 0u,
           "Sleep aborts clock switch");

    dspic33_reset(cpu, 0u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_LOCK);
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL,
                         OSCILLATOR_CLOCK_LOCK | OSCILLATOR_SWITCH_ENABLE);
    expect(state, control(cpu) == 0x0380u, "CLKLOCK blocks subsequent source switch");
    expect(state, cpu->events.count == 0u && !cpu->oscillator.active,
           "CLKLOCK rejection schedules no switch");
}

static void failure_trap_cases(OscillatorConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0004u, 0x000100u);
    dspic33_set_working_register(cpu, 15u, 0x2000u);
    cpu->stop_on_trap = true;
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_FAIL) ==
               DSPIC33_TRAPPED,
           "CF write one dispatches oscillator-fail trap");
    expect(state, (control(cpu) & OSCILLATOR_CLOCK_FAIL) != 0u,
           "oscillator-fail trap leaves CF set");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0002u) != 0u,
           "oscillator-fail trap sets OSCFAIL status");
    expect(state, cpu->last_trap == 0u && cpu->last_trap_return == 0x0026u,
           "oscillator-fail trap records source and return PC");
    expect(state,
           dspic33_read_word(cpu, 0x2000u) == 0x0026u &&
               (dspic33_read_word(cpu, 0x2002u) & 0x007fu) == 0u,
           "oscillator-fail trap stacks return address");
    expect(state, cpu->pc == 0x000100u && cpu->w[15] == 0x2004u,
           "oscillator-fail trap enters vector four");
    dspic33_reset(cpu, 0u);
    cpu->data[OSCILLATOR_CONTROL] = OSCILLATOR_CLOCK_FAIL;
    expect(state, write_protected_byte(cpu, OSCILLATOR_CONTROL, 0u) == DSPIC33_RUNNING,
           "CF clear sequence executes");
    expect(state, (control(cpu) & OSCILLATOR_CLOCK_FAIL) == 0u,
           "CF write zero clears failure flag");
}

static void lifecycle_cases(OscillatorConformance* state, Dspic33* source,
                            Dspic33* copy) {
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves oscillator state");
    expect(state,
           copy->oscillator.active &&
               copy->oscillator.generation == source->oscillator.generation,
           "copy preserves active oscillator generation");
    expect(state, copy->events.count == source->events.count,
           "copy preserves pending oscillator event");
    expect(state, dspic33_device_advance(source, 22u) && control(source) == 0x3320u,
           "source completes copied switch interval");
    expect(state, dspic33_device_advance(copy, 22u) && control(copy) == 0x3320u,
           "copy independently completes switch interval");

    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes during oscillator switch");
    expect(state, control(source) == 0x0301u && source->oscillator.active,
           "warm reset preserves active oscillator switch");
    expect(state, source->events.count == 1u,
           "warm reset reconstructs pending oscillator event");
    expect(state, source->events.items[0].cycle - source->device_cycles == 20u,
           "warm reset preserves remaining oscillator interval");
    expect(state, dspic33_device_advance(source, 19u) && control(source) == 0x0301u,
           "warm-reset switch remains pending before preserved deadline");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x3320u,
           "warm-reset switch completes at preserved deadline");

    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    dspic33_reset(source, 0u);
    expect(state, control(source) == 0u, "POR resets OSCCON from configuration");
    expect(state, !source->oscillator.active && source->events.count == 0u,
           "POR cancels pending oscillator switch");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0u,
           "POR prevents stale oscillator completion");

    dspic33_reset(source, 0u);
    source->device_cycles = UINT64_MAX;
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, source->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "switch schedule overflow reports event error");
    expect(state, control(source) == 0x0301u && source->events.count == 0u,
           "failed schedule leaves requested switch state visible");

    dspic33_reset(source, 0u);
    load_sequence(source, OPCODE_MOV_BYTE_W2_W1, OPCODE_RESET, OPCODE_MOV_BYTE_W3_W1);
    dspic33_load_program_word(source, 0x0026u, OPCODE_MOV_BYTE_W0_W1);
    source->pc = 0x0020u;
    dspic33_set_working_register(source, 0u, 0x40u);
    dspic33_set_working_register(source, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(source, 2u, 0x46u);
    dspic33_set_working_register(source, 3u, 0x57u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "reset invalidation first key executes");
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "reset executes between protected keys");
    source->pc = 0x0024u;
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "post-reset second key executes");
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "post-reset final write executes");
    expect(state, (control(source) & OSCILLATOR_IO_LOCK) == 0u,
           "reset invalidates protected key sequence");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    OscillatorConformance state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    dspic33_load_configuration_word(&source, 0xf80008u, 0x005eu);
    reset_cases(&state, &source);
    protection_cases(&state, &source);
    switch_cases(&state, &source);
    failure_trap_cases(&state, &source);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 86u, "oscillator assertion arithmetic");
    printf("[oscillator-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32
           "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
