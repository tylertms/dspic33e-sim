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
    CONFIGURATION_FOSCSEL = 0xf80006u,
    CONFIGURATION_FOSC = 0xf80008u,
    CONFIGURATION_FWDT = 0xf8000au,
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

static bool program_fosc(Dspic33* cpu, uint8_t value) {
    cpu->nvm.control = 0u;
    cpu->nvm.address = CONFIGURATION_FOSC;
    cpu->nvm.latches[0] = value;
    dspic33_complete_nvm(cpu);
    return dspic33_read_configuration_byte(cpu, CONFIGURATION_FOSC) == value;
}

static bool program_foscsel(Dspic33* cpu, uint8_t value) {
    cpu->nvm.control = 0u;
    cpu->nvm.address = CONFIGURATION_FOSCSEL;
    cpu->nvm.latches[0] = value;
    dspic33_complete_nvm(cpu);
    return dspic33_read_configuration_byte(cpu, CONFIGURATION_FOSCSEL) == value;
}

static bool program_fwdt(Dspic33* cpu, uint8_t value) {
    cpu->nvm.control = 0u;
    cpu->nvm.address = CONFIGURATION_FWDT;
    cpu->nvm.latches[0] = value;
    dspic33_complete_nvm(cpu);
    return dspic33_read_configuration_byte(cpu, CONFIGURATION_FWDT) == value;
}

static void reset_cases(OscillatorConformance* state, Dspic33* cpu) {
    expect(state, dspic33_load_configuration_word(cpu, 0xf80006u, 0x00ffu),
           "load nondefault FNOSC configuration");
    dspic33_reset(cpu, 0u);
    expect(state, control(cpu) == 0x7700u,
           "POR immediately selects ready internal FNOSC source");
    expect(state, (control(cpu) & OSCILLATOR_PLL_LOCK) == 0u,
           "POR internal source has no PLL lock status");
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
    expect(state, control(cpu) == 0x0080u, "CLKLOCK blocks subsequent source switch");
    expect(state, cpu->events.count == 0u && !cpu->oscillator.active,
           "CLKLOCK rejection schedules no switch");
    write_protected_byte(cpu, OSCILLATOR_CONTROL,
                         OSCILLATOR_CLOCK_LOCK | OSCILLATOR_LP_ENABLE);
    expect(state, control(cpu) == 0x0080u, "CLKLOCK blocks LPOSCEN changes");
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

static void configuration_admission_cases(OscillatorConformance* state, Dspic33* cpu) {
    static const uint8_t disabled_configurations[] = {0x9eu, 0xdeu};
    size_t index;
    for (index = 0u;
         index < sizeof(disabled_configurations) / sizeof(disabled_configurations[0]);
         index++) {
        dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC,
                                        disabled_configurations[index]);
        dspic33_reset(cpu, 0u);
        write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x02u);
        write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
        expect(state, control(cpu) == 0x0200u,
               "FCKSM disabled mode rejects software switch");
        expect(state, !cpu->oscillator.active && cpu->events.count == 0u,
               "FCKSM rejection schedules no switch");
    }

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x1eu);
    dspic33_reset(cpu, 0u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_LOCK);
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x02u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL,
                         OSCILLATOR_CLOCK_LOCK | OSCILLATOR_SWITCH_ENABLE);
    expect(state, control(cpu) == 0x0281u && cpu->oscillator.active,
           "FCKSM zero permits switch with CLKLOCK set");
    expect(state, cpu->events.count == 1u,
           "conditional CLKLOCK permission schedules switch");
    write_protected_byte(cpu, OSCILLATOR_CONTROL,
                         OSCILLATOR_CLOCK_LOCK | OSCILLATOR_LP_ENABLE);
    expect(state, (control(cpu) & OSCILLATOR_LP_ENABLE) != 0u,
           "FCKSM zero permits LPOSCEN change with CLKLOCK set");

    dspic33_reset(cpu, 0u);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x9eu);
    dspic33_reset(cpu, 0u);
    expect(state, program_fosc(cpu, 0x1eu), "FOSC RTSP enables clock switching");
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x02u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, control(cpu) == 0x0201u && cpu->oscillator.active,
           "immediate FCKSM update affects next switch");
    expect(state, cpu->events.count == 1u, "RTSP-enabled switch schedules completion");
    expect(state, program_fosc(cpu, 0x009eu),
           "FOSC RTSP disables active software switching");
    expect(state, !cpu->oscillator.active && control(cpu) == 0x0200u,
           "switch disable aborts active software request");
    expect(state, dspic33_device_advance(cpu, 32u) && control(cpu) == 0x0200u,
           "switch disable invalidates stale completion");

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x5eu);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x0079u);
    dspic33_reset(cpu, 0u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, control(cpu) == 0x1321u,
           "FRCPLL to POSCPLL direct switch remains pending");
    expect(state, cpu->oscillator.active && cpu->events.count == 0u,
           "first direct PLL request schedules no completion");
    expect(state, program_fosc(cpu, 0x005du),
           "POSCMD mode changes during direct PLL request");
    expect(state,
           dspic33_device_advance(cpu, 32u) && control(cpu) == 0x1321u &&
               cpu->events.count == 0u,
           "POSCMD mode change cannot complete direct PLL request");
    expect(state, program_fosc(cpu, 0x005fu),
           "POSCMD disables target during direct PLL request");
    expect(state, program_fosc(cpu, 0x005eu),
           "POSCMD restores target during direct PLL request");
    expect(state,
           dspic33_device_advance(cpu, 32u) && control(cpu) == 0x1321u &&
               cpu->events.count == 0u,
           "POSCMD restoration cannot complete direct PLL request");
    expect(state, program_fwdt(cpu, 0x00dfu), "PLLK clears during direct PLL request");
    expect(state,
           dspic33_device_advance(cpu, 32u) && control(cpu) == 0x1321u &&
               cpu->events.count == 0u,
           "PLLK clear cannot complete direct PLL request");
    expect(state, program_fwdt(cpu, 0x00ffu),
           "PLLK restores during direct PLL request");
    expect(state,
           dspic33_device_advance(cpu, 32u) && control(cpu) == 0x1321u &&
               cpu->events.count == 0u,
           "PLLK restore cannot complete direct PLL request");
    write_protected_byte(cpu, OSCILLATOR_CONTROL, 0u);
    expect(state, control(cpu) == 0x1320u && !cpu->oscillator.active,
           "explicit clear aborts unsupported direct PLL request");

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x007bu);
    dspic33_reset(cpu, 0u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x01u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, control(cpu) == 0x3121u,
           "POSCPLL to FRCPLL direct switch remains pending");
    expect(state, cpu->oscillator.active && cpu->events.count == 0u,
           "second direct PLL request schedules no completion");

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x005fu);
    dspic33_reset(cpu, 0u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x02u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, control(cpu) == 0x0201u && cpu->oscillator.active,
           "disabled POSC leaves switch pending");
    expect(state, cpu->events.count == 0u, "disabled POSC schedules no completion");
    expect(state, program_fosc(cpu, 0x005eu), "FOSC RTSP enables pending POSC");
    expect(state, cpu->events.count == 1u && cpu->oscillator.generation == 2u,
           "enabled POSC restarts modeled switch interval");
    expect(state, dspic33_device_advance(cpu, 32u) && control(cpu) == 0x2200u,
           "enabled POSC completes pending switch");

    dspic33_reset(cpu, 0u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, 0x02u);
    write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, cpu->events.count == 1u && cpu->oscillator.active,
           "available POSC starts modeled switch");
    expect(state, program_fosc(cpu, 0x005fu), "FOSC RTSP disables pending POSC");
    expect(state, dspic33_device_advance(cpu, 32u) && control(cpu) == 0x0201u,
           "disabled POSC invalidates scheduled completion");
    expect(state, program_fosc(cpu, 0x005eu), "FOSC RTSP restores pending POSC");
    expect(state, dspic33_device_advance(cpu, 32u) && control(cpu) == 0x2200u,
           "restored POSC completes fresh interval");
}

static void hardware_failure_cases(OscillatorConformance* state, Dspic33* cpu) {
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x007au);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x001eu);
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0004u, 0x000100u);
    dspic33_set_working_register(cpu, 15u, 0x2000u);
    cpu->stop_on_trap = true;
    expect(state, dspic33_oscillator_failure_detected(cpu),
           "FSCM accepts primary oscillator failure");
    expect(state, control(cpu) == 0x0208u,
           "FSCM falls back to FRC and retains requested source");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0002u) != 0u,
           "FSCM latches oscillator-fail status");
    expect(state, dspic33_step(cpu) == DSPIC33_TRAPPED,
           "FSCM dispatches oscillator-fail trap");
    expect(state,
           cpu->pc == 0x000102u && cpu->last_trap == 0u && cpu->last_trap_return == 0u,
           "FSCM enters oscillator-fail vector");

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x005eu);
    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_oscillator_failure_detected(cpu) && control(cpu) == 0x2200u,
           "disabled FSCM ignores external-source failure");

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x001eu);
    dspic33_reset(cpu, 0u);
    expect(state, !dspic33_oscillator_failure_detected(cpu) && control(cpu) == 0u,
           "FSCM ignores internal-source failure");

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x007au);
    dspic33_reset(cpu, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state, !dspic33_oscillator_failure_detected(cpu) && control(cpu) == 0x2200u,
           "FSCM is disabled during Sleep");

    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x005eu);
}

static void pll_lock_sequence_cases(OscillatorConformance* state, Dspic33* source,
                                    Dspic33* copy) {
    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00dfu);
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state,
           control(source) == 0x3300u && !source->oscillator.active &&
               source->oscillator.lock_pending,
           "PLLK zero transfers source before PLL lock");
    expect(state, source->events.count == 1u && source->events.items[0].source == 2u,
           "PLLK zero leaves one pending lock phase");
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_IO_LOCK);
    expect(state,
           control(source) == 0x3340u && source->oscillator.lock_pending &&
               source->events.count == 1u,
           "unrelated OSCCON write preserves pending PLL lock");
    expect(state, dspic33_device_advance(source, 27u) && control(source) == 0x3340u,
           "PLLK zero remains unlocked before modeled lock deadline");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x3360u,
           "PLLK zero sets lock after source transfer");
    expect(state, !source->oscillator.lock_pending && source->events.count == 0u,
           "PLLK zero completion clears lock lifecycle");

    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, control(source) == 0x0301u && source->oscillator.active,
           "PLLK one holds source transfer until lock");
    expect(state,
           source->oscillator.source_ready && source->oscillator.lock_pending &&
               source->events.count == 1u && source->events.items[0].source == 2u,
           "PLLK one waits for the pending lock phase");
    expect(state, program_fwdt(source, 0x00dfu),
           "PLLK RTSP clears pending wait policy");
    expect(state,
           source->oscillator.generation == 1u && source->events.count == 1u &&
               control(source) == 0x3300u && source->oscillator.lock_pending,
           "PLLK clear releases source without moving lock deadline");
    expect(state, dspic33_device_advance(source, 30u) && control(source) == 0x3300u,
           "PLLK-cleared source remains unlocked before original deadline");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x3320u,
           "PLLK clear retains original lock completion");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005fu);
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state, source->oscillator.active && source->events.count == 0u,
           "disabled POSC holds PLLK-zero request");
    expect(state, program_fwdt(source, 0x00ffu),
           "PLLK RTSP enables wait before source availability");
    expect(state, source->events.count == 0u,
           "unavailable source still schedules no PLL event");
    expect(state, program_fosc(source, 0x005eu),
           "POSCMD RTSP enables PLLK-one request");
    expect(state, source->events.count == 1u && source->events.items[0].source == 0u,
           "available source honors updated PLLK policy");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x3320u,
           "updated PLLK policy completes locked switch");

    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00dfu);
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves pending PLL lock phase");
    expect(state,
           copy->oscillator.lock_pending && copy->events.count == 1u &&
               copy->events.items[0].source == 2u,
           "copy retains PLL lock phase identity");
    expect(state, dspic33_device_advance(source, 21u) && control(source) == 0x3320u,
           "source completes copied PLL lock phase");
    expect(state, dspic33_device_advance(copy, 21u) && control(copy) == 0x3320u,
           "copy independently completes PLL lock phase");

    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 10u);
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes during pending PLL lock");
    expect(state,
           control(source) == 0x3300u && source->oscillator.lock_pending &&
               source->events.count == 1u && source->events.items[0].source == 2u,
           "warm reset reconstructs pending PLL lock phase");
    expect(state, dspic33_device_advance(source, 19u) && control(source) == 0x3300u,
           "warm-reset PLL lock remains pending before deadline");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x3320u,
           "warm-reset PLL lock completes at preserved deadline");

    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_load_program_word(source, 0u, OPCODE_SLEEP);
    source->pc = 0u;
    expect(state, dspic33_step(source) == DSPIC33_SLEEPING,
           "Sleep executes during pending PLL lock");
    expect(state, !source->oscillator.lock_pending && control(source) == 0x3300u,
           "Sleep cancels pending PLL lock phase");
    expect(state, dspic33_device_advance(source, 31u) && control(source) == 0x3300u,
           "Sleep prevents stale PLL lock completion");

    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
}

static void two_speed_startup_cases(OscillatorConformance* state, Dspic33* source,
                                    Dspic33* copy) {
    static const uint8_t internal_sources[] = {5u, 6u, 7u};
    uint64_t deadline;
    size_t index;
    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00f8u);
    dspic33_reset(source, 0u);
    expect(state, control(source) == 0u && !source->oscillator.active,
           "Two-Speed FRC target is redundant");

    for (index = 0u; index < sizeof(internal_sources) / sizeof(internal_sources[0]);
         index++) {
        uint8_t source_id = internal_sources[index];
        dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL,
                                        (uint8_t)(0xf8u | source_id));
        dspic33_reset(source, 0u);
        expect(state, control(source) == (uint16_t)(source_id * 0x1100u),
               "Two-Speed immediately selects ready internal source");
        expect(state, !source->oscillator.active && source->events.count == 0u,
               "ready internal source needs no startup event");
    }

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00fau);
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
    dspic33_reset(source, 0u);
    expect(state,
           control(source) == 0x0200u && source->oscillator.active &&
               source->oscillator.automatic && source->events.count == 1u &&
               source->events.items[0].source == 0u,
           "Two-Speed begins on FRC while POSC becomes ready");
    expect(state, dspic33_device_advance(source, 31u) && control(source) == 0x0200u,
           "Two-Speed POSC remains pending before readiness");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x2200u,
           "Two-Speed transfers to ready POSC");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00fcu);
    dspic33_reset(source, 0u);
    expect(state,
           control(source) == 0x0400u && source->oscillator.automatic &&
               source->events.count == 1u,
           "Two-Speed begins SOSC transition without LPOSCEN");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x4400u,
           "Two-Speed transfers to SOSC");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00fbu);
    dspic33_reset(source, 0u);
    expect(state,
           control(source) == 0x0300u && source->oscillator.active &&
               source->oscillator.automatic && source->events.items[0].source == 0u,
           "Two-Speed POSCPLL starts from FRC");
    expect(state,
           dspic33_device_advance(source, 1u) && control(source) == 0x0300u &&
               source->oscillator.source_ready && source->oscillator.lock_pending &&
               source->events.items[0].source == 2u,
           "Two-Speed PLLK one waits after source readiness");
    expect(state, dspic33_device_advance(source, 30u) && control(source) == 0x0300u,
           "Two-Speed PLL remains pending before lock");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x3320u,
           "Two-Speed PLLK one transfers with lock");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00f9u);
    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00dfu);
    dspic33_reset(source, 0u);
    expect(state, control(source) == 0x0100u && source->oscillator.automatic,
           "Two-Speed FRCPLL starts from FRC");
    expect(state,
           dspic33_device_advance(source, 1u) && control(source) == 0x1100u &&
               source->oscillator.lock_pending,
           "Two-Speed PLLK zero transfers at source readiness");
    expect(state, dspic33_device_advance(source, 30u) && control(source) == 0x1100u,
           "Two-Speed PLLK zero remains unlocked before deadline");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x1120u,
           "Two-Speed PLLK zero locks after transfer");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00fau);
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x009eu);
    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
    dspic33_reset(source, 0u);
    expect(state, source->oscillator.active && source->oscillator.automatic,
           "Two-Speed ignores software-switch disable");
    deadline = source->events.items[0].cycle;
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    expect(state,
           control(source) == 0x0200u && source->oscillator.active &&
               source->oscillator.automatic && source->oscillator.generation == 1u &&
               source->events.items[0].cycle == deadline,
           "FCKSM rejection preserves automatic startup deadline");
    expect(state, dspic33_device_advance(source, 29u) && control(source) == 0x2200u,
           "Two-Speed completes with FCKSM disabled");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_reset(source, 0u);
    deadline = source->events.items[0].cycle;
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_LOCK);
    write_protected_byte(source, OSCILLATOR_CONTROL,
                         OSCILLATOR_CLOCK_LOCK | OSCILLATOR_SWITCH_ENABLE);
    expect(state,
           control(source) == 0x0280u && source->oscillator.active &&
               source->oscillator.automatic && source->oscillator.generation == 1u &&
               source->events.items[0].cycle == deadline,
           "CLKLOCK rejection preserves automatic startup deadline");
    expect(state, dspic33_device_advance(source, 26u) && control(source) == 0x2280u,
           "Two-Speed completes with CLKLOCK set");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005fu);
    dspic33_reset(source, 0u);
    expect(state,
           control(source) == 0x0200u && source->oscillator.active &&
               source->oscillator.automatic && source->events.count == 0u,
           "Two-Speed waits for disabled POSC");
    expect(state, program_fosc(source, 0x005eu),
           "POSCMD RTSP enables Two-Speed target");
    expect(state, source->events.count == 1u && source->events.items[0].source == 0u,
           "enabled Two-Speed target schedules readiness");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x2200u,
           "enabled Two-Speed target completes");

    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_IO_LOCK);
    expect(state,
           control(source) == 0x0240u && source->oscillator.active &&
               source->oscillator.automatic,
           "OSCCON low write preserves automatic startup");
    expect(state, dspic33_device_advance(source, 29u) && control(source) == 0x2240u,
           "automatic startup retains original readiness deadline");

    dspic33_reset(source, 0u);
    dspic33_load_program_word(source, 0u, OPCODE_SLEEP);
    source->pc = 0u;
    expect(state, dspic33_step(source) == DSPIC33_SLEEPING,
           "Sleep executes during automatic startup");
    expect(state, !source->oscillator.active && control(source) == 0x0200u,
           "Sleep aborts automatic startup");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x0200u,
           "Sleep prevents stale automatic completion");

    dspic33_reset(source, 0u);
    dspic33_device_advance(source, 10u);
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes during automatic startup");
    expect(state,
           control(source) == 0x0200u && source->oscillator.active &&
               source->oscillator.automatic && source->events.count == 1u &&
               source->events.items[0].source == 0u,
           "warm reset reconstructs automatic startup");
    expect(state, dspic33_device_advance(source, 21u) && control(source) == 0x2200u,
           "warm-reset automatic startup completes at preserved deadline");

    dspic33_reset(source, 0u);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves automatic startup");
    expect(state,
           copy->oscillator.active && copy->oscillator.automatic &&
               copy->events.count == 1u,
           "copy retains automatic startup phase");
    expect(state,
           dspic33_device_advance(source, 22u) && control(source) == 0x2200u &&
               dspic33_device_advance(copy, 22u) && control(copy) == 0x2200u,
           "copied automatic startups complete independently");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x009eu);
    dspic33_reset(source, 0u);
    expect(state, program_foscsel(source, 0x007au),
           "FNOSC RTSP programs while software switching is disabled");
    expect(state,
           control(source) == 0x0200u && source->oscillator.active &&
               source->oscillator.automatic,
           "disabled switching applies FNOSC immediately");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x2200u,
           "immediate FNOSC transition completes");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_reset(source, 0u);
    expect(state, program_foscsel(source, 0x007cu),
           "FNOSC RTSP programs while switching is enabled");
    expect(state, control(source) == 0u && source->events.count == 0u,
           "enabled switching defers FNOSC until reset");
    dspic33_reset(source, 0u);
    expect(state, control(source) == 0x4400u,
           "reset applies deferred FNOSC configuration");
    expect(state, program_foscsel(source, 0x00fcu), "IESO RTSP changes startup policy");
    expect(state, control(source) == 0x4400u && source->events.count == 0u,
           "IESO RTSP does not start a mid-run transition");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
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
    configuration_admission_cases(&state, &source);
    hardware_failure_cases(&state, &source);
    pll_lock_sequence_cases(&state, &source, &copy);
    two_speed_startup_cases(&state, &source, &copy);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 209u, "oscillator assertion arithmetic");
    printf("[oscillator-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32
           "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
