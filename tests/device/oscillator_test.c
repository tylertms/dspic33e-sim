#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

enum {
    OSCILLATOR_CONTROL = 0x0742u,
    OSCILLATOR_SWITCH_ENABLE = 0x0001u,
    OSCILLATOR_LP_ENABLE = 0x0002u,
    OSCILLATOR_CLOCK_FAIL = 0x0008u,
    OSCILLATOR_PLL_LOCK = 0x0020u,
    OSCILLATOR_IO_LOCK = 0x0040u,
    OSCILLATOR_CLOCK_LOCK = 0x0080u,
    OSCILLATOR_SWITCH_DELAY = 32u,
    REFERENCE_CLOCK_CONTROL = 0x074eu,
    REFERENCE_CLOCK_ENABLE = 0x8000u,
    REFERENCE_CLOCK_SLEEP = 0x2000u,
    REFERENCE_CLOCK_SOURCE = 0x1000u,
    REFERENCE_CLOCK_DIVISOR = 0x0f00u,
    MAIN_CLOCK_DIVISOR = 0x0744u,
    MAIN_CLOCK_RECOVER_INTERRUPT = 0x8000u,
    MAIN_CLOCK_DOZE_MASK = 0x7000u,
    MAIN_CLOCK_DOZE_ENABLE = 0x0800u,
    MAIN_PLL_FEEDBACK = 0x0746u,
    MAIN_OSCILLATOR_TUNING = 0x0748u,
    TIMER1_COUNTER = 0x0100u,
    TIMER1_PERIOD = 0x0102u,
    TIMER1_CONTROL = 0x0104u,
    CRC_PMD_ADDRESS = 0x0764u,
    CRC_PMD = 0x0080u,
    CONFIGURATION_FOSCSEL = 0xf80006u,
    CONFIGURATION_FOSC = 0xf80008u,
    CONFIGURATION_FWDT = 0xf8000au,
    OPCODE_NOP = 0x000000u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_RESET = 0xfe0000u,
    OPCODE_GOTO_W0 = 0x010400u,
    OPCODE_MOV_IFS0_W2 = 0x804002u,
    OPCODE_MOV_W0_W1 = 0x780880u,
    OPCODE_MOV_BYTE_W0_W1 = 0x784880u,
    OPCODE_MOV_BYTE_W2_W1 = 0x784882u,
    OPCODE_MOV_BYTE_W3_W1 = 0x784883u
};

static uint16_t control(Dspic33* cpu) {
    return dspic33_read_word(cpu, OSCILLATOR_CONTROL);
}

static bool load_sequence(Dspic33* cpu, uint32_t first, uint32_t second,
                          uint32_t third) {
    return dspic33_load_program_word(cpu, 0x0200u, first) &&
           dspic33_load_program_word(cpu, 0x0202u, second) &&
           dspic33_load_program_word(cpu, 0x0204u, third);
}

static Dspic33StopReason write_protected_byte(Dspic33* cpu, uint16_t address,
                                              uint8_t value) {
    uint8_t first = address == OSCILLATOR_CONTROL ? 0x46u : 0x78u;
    uint8_t second = address == OSCILLATOR_CONTROL ? 0x57u : 0x9au;
    load_sequence(cpu, OPCODE_MOV_BYTE_W2_W1, OPCODE_MOV_BYTE_W3_W1,
                  OPCODE_MOV_BYTE_W0_W1);
    cpu->pc = 0x0200u;
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

static bool select_locked_main_pll(Dspic33* cpu, uint8_t source) {
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x005eu);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FWDT, 0x00ffu);
    dspic33_reset(cpu, 0u);
    if (write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, source) != DSPIC33_RUNNING ||
        write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE) !=
            DSPIC33_RUNNING ||
        !dspic33_device_advance(cpu, OSCILLATOR_SWITCH_DELAY)) {
        return false;
    }
    return control(cpu) == (uint16_t)(source * 0x1100u + OSCILLATOR_PLL_LOCK);
}

static void oscillator_pin_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    for (uint8_t primary_mode = 0u; primary_mode < 4u; primary_mode++) {
        for (uint8_t clock_select = 0u; clock_select < 2u; clock_select++) {
            bool gpio_high = false;
            bool clock_output = false;
            uint64_t edges = 0u;
            bool owned = primary_mode == 1u || primary_mode == 2u || clock_select != 0u;
            bool clock = owned && primary_mode != 1u && primary_mode != 2u;
            dspic33_load_configuration_word(
                source, CONFIGURATION_FOSC,
                (uint16_t)(0x0058u | primary_mode | (clock_select << 2u)));
            dspic33_reset(source, 0u);
            dspic33_write_word(source, 0x0e20u, 0u);
            dspic33_write_word(source, 0x0e24u, 0x8000u);
            expect(
                state,
                (dspic33_gpio_pin(source, 2u, 15u, &gpio_high) == !owned) &&
                    (dspic33_oscillator_pin(source, &clock_output, &edges) == owned) &&
                    (!owned || (clock_output == clock &&
                                edges == (clock ? source->device_cycles * 2u : 0u))) &&
                    (owned || gpio_high),
                "OSC2 ownership follows POSCMD and OSCIOFNC");
        }
    }

    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005cu);
    dspic33_reset(source, 0u);
    bool clock_output = false;
    uint64_t initial_edges = 0u;
    uint64_t advanced_edges = 0u;
    expect(state,
           dspic33_oscillator_pin(source, &clock_output, &initial_edges) &&
               clock_output && dspic33_device_advance(source, 7u) &&
               dspic33_oscillator_pin(source, &clock_output, &advanced_edges) &&
               advanced_edges - initial_edges == 14u,
           "OSC2 clock output produces two FCY edges per device cycle");
    expect(state,
           dspic33_copy(copy, source) &&
               dspic33_oscillator_pin(copy, &clock_output, &initial_edges) &&
               initial_edges == advanced_edges && dspic33_device_advance(source, 3u) &&
               dspic33_oscillator_pin(source, &clock_output, &advanced_edges) &&
               dspic33_oscillator_pin(copy, &clock_output, &initial_edges) &&
               advanced_edges - initial_edges == 6u,
           "copied OSC2 clock output advances independently");
    source->device_cycles = UINT64_MAX;
    expect(state,
           dspic33_oscillator_pin(source, &clock_output, &advanced_edges) &&
               clock_output && advanced_edges == UINT64_MAX,
           "OSC2 edge counter saturates without overflow");
    expect(state,
           !dspic33_oscillator_pin(source, NULL, &advanced_edges) &&
               !dspic33_oscillator_pin(source, &clock_output, NULL),
           "OSC2 observation rejects null outputs");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005au);
    dspic33_reset(source, 0u);
    expect(state,
           dspic33_oscillator_pin(source, &clock_output, &advanced_edges) &&
               !clock_output && advanced_edges == 0u &&
               !dspic33_gpio_pin(source, 2u, 15u, &clock_output),
           "HS mode reserves OSC2 for the crystal oscillator");
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x0059u);
    dspic33_reset(source, 0u);
    expect(state,
           dspic33_oscillator_pin(source, &clock_output, &advanced_edges) &&
               !clock_output && advanced_edges == 0u &&
               !dspic33_gpio_pin(source, 2u, 15u, &clock_output),
           "XT mode reserves OSC2 for the crystal oscillator");
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_reset(source, 0u);
}

static void reset_cases(TestState* state, Dspic33* cpu) {
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

static void protection_cases(TestState* state, Dspic33* cpu) {
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
    dspic33_load_program_word(cpu, 0x0206u, OPCODE_MOV_BYTE_W0_W1);
    cpu->pc = 0x0200u;
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
    cpu->pc = 0x0200u;
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
    cpu->pc = 0x0200u;
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

static void switch_cases(TestState* state, Dspic33* cpu) {
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
    dspic33_load_program_word(cpu, 0x0200u, OPCODE_SLEEP);
    cpu->pc = 0x0200u;
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

static void failure_trap_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_FAIL) ==
               DSPIC33_RUNNING,
           "CF write one executes without a software-generated failure");
    expect(state, (control(cpu) & OSCILLATOR_CLOCK_FAIL) == 0u && cpu->trap_count == 0u,
           "CF write one cannot set hardware status or dispatch a trap");
    cpu->data[OSCILLATOR_CONTROL] = OSCILLATOR_CLOCK_FAIL;
    expect(state,
           write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_FAIL) ==
               DSPIC33_RUNNING,
           "CF write one executes while hardware status is set");
    expect(state, (control(cpu) & OSCILLATOR_CLOCK_FAIL) != 0u,
           "CF write one preserves hardware-set status");
    expect(state, write_protected_byte(cpu, OSCILLATOR_CONTROL, 0u) == DSPIC33_RUNNING,
           "CF clear sequence executes");
    expect(state, (control(cpu) & OSCILLATOR_CLOCK_FAIL) == 0u,
           "CF write zero clears failure flag");
}

static void source_admission_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t fcksm;
    uint8_t source;
    for (fcksm = 0u; fcksm < 4u; fcksm++) {
        for (source = 0u; source < 8u; source++) {
            uint16_t expected;
            bool switching_enabled = (fcksm & 2u) == 0u;
            dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x0078u);
            dspic33_load_configuration_word(
                cpu, CONFIGURATION_FOSC, (uint8_t)(0x001eu | (uint8_t)(fcksm << 6u)));
            dspic33_load_configuration_word(cpu, CONFIGURATION_FWDT, 0x00ffu);
            dspic33_reset(cpu, 0u);
            write_protected_byte(cpu, OSCILLATOR_CONTROL + 1u, source);
            write_protected_byte(cpu, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
            if (switching_enabled && source != 0u) {
                expected = (uint16_t)(source * 0x1100u);
                if (source == 1u || source == 3u) {
                    expected |= OSCILLATOR_PLL_LOCK;
                }
                expect(state,
                       cpu->oscillator.active && cpu->events.count == 1u &&
                           (control(cpu) & OSCILLATOR_SWITCH_ENABLE) != 0u,
                       "enabled source request starts one switch lifecycle");
                expect(state,
                       dspic33_device_advance(cpu, OSCILLATOR_SWITCH_DELAY) &&
                           control(cpu) == expected && !cpu->oscillator.active &&
                           cpu->events.count == 0u,
                       "enabled source request completes with exact status");
            } else {
                expected = (uint16_t)(source << 8u);
                expect(state,
                       control(cpu) == expected && !cpu->oscillator.active &&
                           cpu->events.count == 0u,
                       "disabled or redundant source request schedules no lifecycle");
                expect(state,
                       dspic33_device_advance(cpu, OSCILLATOR_SWITCH_DELAY) &&
                           control(cpu) == expected,
                       "disabled or redundant source request remains stable");
            }
        }
    }
}

static void fail_safe_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t fcksm;
    uint8_t source;
    for (fcksm = 0u; fcksm < 4u; fcksm++) {
        for (source = 0u; source < 8u; source++) {
            bool expected = fcksm == 0u && source >= 2u && source <= 4u;
            uint16_t previous;
            dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL,
                                            (uint8_t)(0x0078u | source));
            dspic33_load_configuration_word(
                cpu, CONFIGURATION_FOSC, (uint8_t)(0x001eu | (uint8_t)(fcksm << 6u)));
            dspic33_reset(cpu, 0u);
            previous = control(cpu);
            expect(state, dspic33_oscillator_failure_detected(cpu) == expected,
                   "FSCM admission follows source and FCKSM configuration");
            expect(state,
                   expected ? control(cpu) == (uint16_t)((uint16_t)source << 8u |
                                                         OSCILLATOR_CLOCK_FAIL)
                            : control(cpu) == previous,
                   "FSCM matrix preserves or replaces oscillator status exactly");
        }
    }
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x005eu);
}

static void configuration_admission_cases(TestState* state, Dspic33* cpu) {
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

static void hardware_failure_cases(TestState* state, Dspic33* cpu) {
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSCSEL, 0x007au);
    dspic33_load_configuration_word(cpu, CONFIGURATION_FOSC, 0x001eu);
    dspic33_reset(cpu, 0u);
    dspic33_load_program_word(cpu, 0x0004u, 0x000300u);
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
           cpu->pc == 0x000302u && cpu->last_trap == 0u && cpu->last_trap_return == 0u,
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

static void pll_lock_sequence_cases(TestState* state, Dspic33* source, Dspic33* copy) {
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

static void two_speed_startup_cases(TestState* state, Dspic33* source, Dspic33* copy) {
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

static void reference_clock_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    dspic33_reset(source, 0u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0u,
           "REFOCON resets disabled");

    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x0500u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0500u,
           "disabled reference clock accepts divisor");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x8500u);
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x8a00u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x8500u,
           "enabled reference clock preserves divisor on word write");

    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                       REFERENCE_CLOCK_SLEEP | REFERENCE_CLOCK_SOURCE | 0x0300u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x3500u,
           "disable write preserves divisor while applying controls");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x0300u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0300u,
           "subsequent disabled write changes divisor");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0x8700u);
    expect(state, dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x8700u,
           "disabled reference clock enables with new divisor");

    dspic33_write_byte(source, REFERENCE_CLOCK_CONTROL + 1u, 0xbau);
    expect(state,
           dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) ==
               (REFERENCE_CLOCK_ENABLE | REFERENCE_CLOCK_SLEEP |
                REFERENCE_CLOCK_SOURCE | 0x0700u),
           "enabled high-byte write preserves divisor and applies controls");

    dspic33_load_program_word(source, 0u, OPCODE_MOV_W0_W1);
    source->pc = 0u;
    dspic33_set_working_register(source, 0u, 0x8900u);
    dspic33_set_working_register(source, 1u, REFERENCE_CLOCK_CONTROL);
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x8700u,
           "stepped word write preserves enabled divisor");

    dspic33_set_working_register(source, 0u, 0x0900u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0700u,
           "stepped disable preserves enabled divisor");
    dspic33_set_working_register(source, 0u, 0x0900u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0x0900u,
           "stepped disabled write changes divisor");

    expect(state, dspic33_copy(copy, source), "copy preserves reference clock state");
    expect(state, dspic33_read_word(copy, REFERENCE_CLOCK_CONTROL) == 0x0900u,
           "copy retains reference clock divisor");
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, REFERENCE_CLOCK_CONTROL) == 0u,
           "warm reset clears reference clock control");
    dspic33_reset(copy, 0u);
    expect(state, dspic33_read_word(copy, REFERENCE_CLOCK_CONTROL) == 0u,
           "cold reset clears copied reference clock control");
    expect(state,
           source->events.count == 0u && !source->oscillator.active &&
               copy->events.count == 0u && !copy->oscillator.active,
           "reference clock writes create no oscillator lifecycle");
}

static void reference_clock_pin_cases(TestState* state, Dspic33* source,
                                      Dspic33* copy) {
    uint64_t edges = 0u;
    dspic33_reset(source, 0u);
    dspic33_write_word(source, 0x0680u, 49u);
    expect(state, !dspic33_reference_clock_pin(source, 64u, 0u, &edges),
           "disabled reference clock releases its PPS output");
    expect(state, dspic33_device_advance(source, 65536u),
           "reference clock edge domain advances");
    for (uint8_t divisor = 0u; divisor < 16u; divisor++) {
        dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, (uint16_t)(divisor << 8u));
        dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                           (uint16_t)(REFERENCE_CLOCK_ENABLE | divisor << 8u));
        expect(state,
               dspic33_reference_clock_pin(source, 64u, 0u, &edges) &&
                   edges == (source->device_cycles * 2u >> divisor),
               "REFCLKO divides the system clock by RODIV");
        dspic33_write_word(source, REFERENCE_CLOCK_CONTROL, 0u);
    }

    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                       REFERENCE_CLOCK_ENABLE | REFERENCE_CLOCK_SOURCE);
    expect(state,
           dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges) &&
               edges == 0x123456u,
           "REFCLKO selects supplied primary oscillator edges");

    source->power_state = DSPIC33_POWER_SLEEP;
    expect(state, !dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges),
           "REFCLKO stops in Sleep when ROSSLP is clear");
    dspic33_write_word(source, REFERENCE_CLOCK_CONTROL,
                       REFERENCE_CLOCK_ENABLE | REFERENCE_CLOCK_SLEEP |
                           REFERENCE_CLOCK_SOURCE);
    expect(state,
           dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges) &&
               edges == 0x123456u,
           "REFCLKO continues in Sleep when ROSSLP is set");
    source->power_state = DSPIC33_POWER_ACTIVE;

    dspic33_write_word(source, 0x0680u, (uint16_t)(49u << 8u));
    expect(state,
           !dspic33_reference_clock_pin(source, 64u, 0x123456u, &edges) &&
               dspic33_reference_clock_pin(source, 65u, 0x123456u, &edges),
           "REFCLKO follows PPS remapping");
    expect(state,
           dspic33_copy(copy, source) &&
               dspic33_reference_clock_pin(copy, 65u, 0x654321u, &edges) &&
               edges == 0x654321u,
           "copy preserves REFCLKO source, divisor and mapping");
    expect(state,
           !dspic33_reference_clock_pin(source, 63u, 0u, &edges) &&
               !dspic33_reference_clock_pin(source, 65u, 0u, NULL),
           "REFCLKO observation rejects invalid outputs");
}

static void main_pll_configuration_cases(TestState* state, Dspic33* source,
                                         Dspic33* copy) {
    uint32_t generation;
    uint64_t deadline;

    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_CLOCK_LOCK);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0xffffu);
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0xffffu);
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0xffffu);
    expect(state,
           dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3040u &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u &&
               dspic33_read_word(source, MAIN_OSCILLATOR_TUNING) == 0u,
           "effective CLKLOCK rejects main PLL word writes");
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR, 0x1fu);
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR + 1u, 0x07u);
    dspic33_write_byte(source, MAIN_PLL_FEEDBACK, 0xffu);
    dspic33_write_byte(source, MAIN_PLL_FEEDBACK + 1u, 0x01u);
    dspic33_write_byte(source, MAIN_OSCILLATOR_TUNING, 0x3fu);
    expect(state,
           dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3040u &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u &&
               dspic33_read_word(source, MAIN_OSCILLATOR_TUNING) == 0u,
           "effective CLKLOCK rejects main PLL byte writes");
    expect(state,
           source->oscillator.generation == generation && source->events.count == 0u,
           "rejected main PLL writes preserve oscillator lifecycle");

    expect(state, program_fosc(source, 0x001eu),
           "FCKSM zero-zero immediately unlocks clock configuration");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x071fu);
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x01ffu);
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x003fu);
    expect(state,
           dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x071fu &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x01ffu &&
               dspic33_read_word(source, MAIN_OSCILLATOR_TUNING) == 0x003fu,
           "FCKSM zero-zero permits clock configuration with CLKLOCK set");
    expect(state,
           source->oscillator.generation == generation && !source->oscillator.active &&
               !source->oscillator.lock_pending && source->events.count == 0u,
           "non-PLL configuration writes create no relock lifecycle");
    expect(state, program_fosc(source, 0x00deu),
           "FCKSM one-one immediately locks clock configuration");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0030u);
    expect(state, dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x01ffu,
           "FCKSM one-one enforces CLKLOCK");
    expect(state, program_fosc(source, 0x009eu),
           "FCKSM one-zero immediately unlocks clock configuration");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0030u);
    expect(state, dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u,
           "FCKSM one-zero permits clock configuration with CLKLOCK set");

    expect(state, select_locked_main_pll(source, 1u),
           "select stable FRCPLL for relock cases");
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           control(source) == 0x1100u && source->oscillator.lock_pending &&
               !source->oscillator.active &&
               source->oscillator.generation == generation + 1u &&
               source->events.count == 1u && source->events.items[0].source == 2u,
           "FRCPLL feedback change restarts lock");
    expect(state, dspic33_device_advance(source, 31u) && control(source) == 0x1100u,
           "FRCPLL feedback remains unlocked before new deadline");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x1120u,
           "FRCPLL feedback relocks at new deadline");

    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3041u);
    expect(state, control(source) == 0x1100u && source->oscillator.lock_pending,
           "FRCPLL prescaler change restarts lock");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x1120u,
           "FRCPLL prescaler relocks");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3141u);
    expect(state, control(source) == 0x1100u && source->oscillator.lock_pending,
           "FRCPLL input divider change restarts lock");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x1120u,
           "FRCPLL input divider relocks");
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x0001u);
    expect(state, control(source) == 0x1100u && source->oscillator.lock_pending,
           "FRCPLL tuning change restarts lock");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x1120u,
           "FRCPLL tuning relocks");

    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3101u);
    expect(state,
           control(source) == 0x1120u && source->oscillator.generation == generation &&
               source->events.count == 0u,
           "FRCPLL postscaler change preserves lock");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x0101u);
    expect(state,
           control(source) == 0x1120u && source->oscillator.generation == generation &&
               source->events.count == 0u,
           "FRCPLL DOZE field change preserves PLL lock");

    expect(state, select_locked_main_pll(source, 3u),
           "select stable POSCPLL for relock cases");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state, control(source) == 0x3300u && source->oscillator.lock_pending,
           "POSCPLL feedback change restarts lock");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x3320u,
           "POSCPLL feedback relocks");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3041u);
    expect(state, control(source) == 0x3300u && source->oscillator.lock_pending,
           "POSCPLL prescaler change restarts lock");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x3320u,
           "POSCPLL prescaler relocks");
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3141u);
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x0001u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3101u);
    expect(state,
           control(source) == 0x3320u && source->oscillator.generation == generation &&
               source->events.count == 0u,
           "POSCPLL irrelevant fields preserve lock");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x00fbu);
    dspic33_reset(source, 0u);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           control(source) == 0x0300u && source->oscillator.active &&
               source->oscillator.automatic && !source->oscillator.source_ready &&
               source->oscillator.generation == generation + 1u,
           "pre-source PLL change restarts switch generation");
    expect(state,
           dspic33_device_advance(source, 1u) && source->oscillator.source_ready &&
               source->oscillator.lock_pending,
           "restarted PLL switch reaches source-ready phase");
    expect(state, dspic33_device_advance(source, 31u) && control(source) == 0x3320u,
           "restarted PLL switch locks and transfers");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 3u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 1u);
    generation = source->oscillator.generation;
    deadline = source->events.items[0].cycle;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           source->oscillator.active && source->oscillator.source_ready &&
               source->oscillator.lock_pending &&
               source->oscillator.generation == generation + 1u &&
               source->events.items[source->events.count - 1u].cycle ==
                   source->device_cycles + OSCILLATOR_SWITCH_DELAY &&
               source->events.items[0].cycle == deadline,
           "source-ready PLL change replaces lock deadline and leaves stale event");
    expect(state, dspic33_device_advance(source, 31u) && control(source) == 0x0301u,
           "stale source-ready lock event cannot transfer");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x3320u,
           "replacement source-ready lock event transfers");

    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00dfu);
    dspic33_reset(source, 0u);
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 1u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    dspic33_device_advance(source, 1u);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_OSCILLATOR_TUNING, 0x0001u);
    expect(state,
           control(source) == 0x1100u && !source->oscillator.active &&
               source->oscillator.lock_pending &&
               source->oscillator.generation == generation + 1u,
           "PLLK zero post-transfer change replaces lock deadline");
    expect(state, dspic33_device_advance(source, 31u) && control(source) == 0x1100u,
           "stale PLLK zero lock event cannot lock");
    expect(state, dspic33_device_advance(source, 1u) && control(source) == 0x1120u,
           "replacement PLLK zero event locks");

    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
    expect(state, select_locked_main_pll(source, 1u),
           "select FRCPLL for prohibited direct transition");
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 3u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           control(source) == 0x1321u && source->oscillator.active &&
               source->oscillator.generation == generation &&
               source->events.count == 0u,
           "PLL register write cannot complete prohibited direct transition");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0x1321u,
           "prohibited direct transition remains pending after PLL write");

    expect(state, select_locked_main_pll(source, 1u),
           "select FRCPLL for switch-away configuration change");
    generation = source->oscillator.generation;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0030u);
    expect(state,
           control(source) == 0x1120u && source->oscillator.generation == generation &&
               source->events.count == 0u,
           "unchanged PLL configuration preserves lock lifecycle");
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 2u);
    write_protected_byte(source, OSCILLATOR_CONTROL, OSCILLATOR_SWITCH_ENABLE);
    generation = source->oscillator.generation;
    deadline = source->events.items[0].cycle;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           control(source) == 0x1201u && source->oscillator.active &&
               source->oscillator.generation == generation &&
               source->events.count == 1u && source->events.items[0].cycle == deadline,
           "PLL change while switching away preserves destination readiness");
    expect(state, dspic33_device_advance(source, 31u) && control(source) == 0x2200u,
           "switch away from PLL completes after configuration change");

    expect(state, select_locked_main_pll(source, 1u),
           "select FRCPLL for relock lifecycle");
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves main PLL relock");
    expect(state,
           copy->oscillator.lock_pending && copy->events.count == 1u &&
               dspic33_device_advance(source, 22u) && control(source) == 0x1120u &&
               dspic33_device_advance(copy, 22u) && control(copy) == 0x1120u,
           "copied main PLL relocks complete independently");

    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0032u);
    dspic33_device_advance(source, 10u);
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->oscillator.lock_pending &&
               source->events.count == 1u && control(source) == 0x1100u,
           "warm reset reconstructs main PLL relock");
    expect(state, dspic33_device_advance(source, 21u) && control(source) == 0x1120u,
           "warm-reset main PLL relock keeps remaining deadline");

    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0033u);
    dspic33_reset(source, 0u);
    expect(state,
           control(source) == 0u && !source->oscillator.lock_pending &&
               source->events.count == 0u &&
               dspic33_read_word(source, MAIN_PLL_FEEDBACK) == 0x0030u,
           "cold reset cancels relock and restores PLL configuration");

    expect(state, select_locked_main_pll(source, 1u),
           "select FRCPLL for relock schedule failure");
    source->device_cycles = UINT64_MAX;
    dspic33_write_word(source, MAIN_PLL_FEEDBACK, 0x0031u);
    expect(state,
           source->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               control(source) == 0x1100u && source->oscillator.lock_pending &&
               source->events.count == 0u,
           "main PLL relock schedule failure is explicit and deterministic");

    dspic33_load_configuration_word(source, CONFIGURATION_FOSCSEL, 0x0078u);
    dspic33_load_configuration_word(source, CONFIGURATION_FOSC, 0x005eu);
    dspic33_load_configuration_word(source, CONFIGURATION_FWDT, 0x00ffu);
}

static void configure_doze_interrupt(Dspic33* cpu, uint8_t priority) {
    dspic33_write_word(cpu, 0x0820u, 1u);
    dspic33_write_word(cpu, 0x0840u, priority);
    dspic33_load_program_word(cpu, 0x0014u, 0x0300u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 15u, 0x1800u);
    dspic33_raise_interrupt(cpu, 0u);
}

static void doze_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    uint64_t cpu_cycles;
    uint64_t device_cycles;
    uint8_t divisor;
    Dspic33StopReason trap_result;

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x7800u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x7800u,
           "disabled DOZE field and DOZEN set together");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1800u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x7800u,
           "enabled DOZE field ignores writes");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1000u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x7000u,
           "combined disable and divisor write preserves enabled divisor");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1000u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x1000u,
           "disabled DOZE field accepts a subsequent write");
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, MAIN_CLOCK_DOZE_ENABLE);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0u,
           "DOZEN cannot set at the one-to-one ratio");
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR + 1u, 0x38u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3800u,
           "high-byte write sets a disabled DOZE ratio");
    dspic33_write_byte(source, MAIN_CLOCK_DIVISOR + 1u, 0x18u);
    expect(state, dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3800u,
           "high-byte write preserves an enabled DOZE field");

    for (divisor = 1u; divisor <= 7u; divisor++) {
        uint64_t ratio = UINT64_C(1) << divisor;
        dspic33_reset(source, 0u);
        dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                           (uint16_t)((uint16_t)divisor << 12u) |
                               MAIN_CLOCK_DOZE_ENABLE);
        dspic33_load_program_word(source, 0u, OPCODE_NOP);
        source->pc = 0u;
        cpu_cycles = source->cycles;
        device_cycles = source->device_cycles;
        expect(state,
               dspic33_step(source) == DSPIC33_RUNNING &&
                   source->cycles - cpu_cycles == 1u &&
                   source->device_cycles - device_cycles == ratio,
               "DOZE ratio scales stepped peripheral cycles");
    }

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x2800u);
    dspic33_load_program_word(source, 0u, OPCODE_GOTO_W0);
    dspic33_load_program_word(source, 0x0100u, OPCODE_NOP);
    dspic33_set_working_register(source, 0u, 0x0100u);
    dspic33_schedule(source, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 15u);
    source->pc = 0u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->pc == 0x0100u &&
               source->cycles - cpu_cycles == 4u &&
               source->device_cycles - device_cycles == 16u &&
               (dspic33_read_word(source, 0x0800u) & 1u) != 0u &&
               source->events.count == 0u,
           "DOZE scales multi-cycle instructions across device events");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x2800u);
    dspic33_load_program_word(source, 0u, OPCODE_MOV_IFS0_W2);
    dspic33_write_word(source, 0x0800u, 0x1234u);
    source->pc = 0u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->w[2] == 0x1234u &&
               source->cycles - cpu_cycles == 2u &&
               source->device_cycles - device_cycles == 8u,
           "DOZE scales the separated non-CPU SFR wait cycle");

    dspic33_reset(source, 0x200u);
    dspic33_load_program_word(source, 0x200u, OPCODE_MOV_W0_W1);
    dspic33_load_program_word(source, 0x202u, OPCODE_NOP);
    dspic33_set_working_register(source, 0u, 0x1800u);
    dspic33_set_working_register(source, 1u, MAIN_CLOCK_DIVISOR);
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x1800u &&
               source->cycles - cpu_cycles == 1u &&
               source->device_cycles - device_cycles == 1u,
           "DOZEN setting instruction uses the previous ratio");
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               source->device_cycles - device_cycles == 2u,
           "instruction after DOZEN setting uses the new ratio");
    source->pc = 0x200u;
    dspic33_set_working_register(source, 0u, 0x1000u);
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x1000u &&
               source->device_cycles - device_cycles == 2u,
           "DOZEN clearing instruction uses the previous ratio");
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               source->device_cycles - device_cycles == 1u,
           "instruction after DOZEN clearing returns to one-to-one");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_write_word(source, TIMER1_COUNTER, 0u);
    dspic33_write_word(source, TIMER1_PERIOD, 0xffffu);
    dspic33_write_word(source, TIMER1_CONTROL, 0x8000u);
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    source->pc = 0u;
    source->disicnt = 5u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               source->cycles - cpu_cycles == 1u &&
               source->device_cycles - device_cycles == 8u && source->disicnt == 4u &&
               dspic33_read_word(source, TIMER1_COUNTER) == 8u,
           "DOZE keeps CPU state slow while peripherals run at full speed");
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_device_advance(source, 1u) && source->cycles - cpu_cycles == 1u &&
               source->device_cycles - device_cycles == 1u &&
               dspic33_read_word(source, TIMER1_COUNTER) == 9u,
           "public device advance remains an equal-domain stimulus");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_write_word(source, CRC_PMD_ADDRESS, CRC_PMD);
    expect(state,
           source->events.count == 1u &&
               source->events.items[0].cycle - source->device_cycles == 8u &&
               !source->io.crc.pmd_disabled,
           "DOZE scales one-instruction PMD transitions");
    expect(state, dspic33_device_advance(source, 7u) && !source->io.crc.pmd_disabled,
           "scaled PMD transition remains pending before its deadline");
    expect(state, dspic33_device_advance(source, 1u) && source->io.crc.pmd_disabled,
           "scaled PMD transition completes at one divided instruction");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    dspic33_load_program_word(source, 0u, OPCODE_MOV_W0_W1);
    dspic33_set_working_register(source, 0u, CRC_PMD);
    dspic33_set_working_register(source, 1u, CRC_PMD_ADDRESS);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->io.crc.pmd_disabled &&
               source->device_cycles - device_cycles == 8u &&
               source->events.count == 0u,
           "stepped PMD write completes after one divided instruction");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->last_interrupt == 0u &&
               source->pc == 0x0302u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) != 0u &&
               source->device_cycles - device_cycles == 20u,
           "interrupt preserves DOZE when ROI is clear");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->last_interrupt == 0u &&
               source->pc == 0x0302u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) == 0u &&
               source->device_cycles - device_cycles == 10u,
           "ROI clears DOZEN before the first handler instruction");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->sr = 0x0020u;
    source->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_device_wake(source) && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) == 0u &&
               (dspic33_read_word(source, 0x0800u) & 1u) != 0u,
           "ROI clears DOZEN on a wake-only interrupt");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->sr = 0x0020u;
    source->power_state = DSPIC33_POWER_IDLE;
    source->pc = 0u;
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               source->power_state == DSPIC33_POWER_ACTIVE &&
               source->interrupt_count == 0u && source->pc == 2u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) == 0u &&
               source->device_cycles - device_cycles == 1u,
           "wake-only ROI resumes stepped execution at one-to-one");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    source->pc = 0u;
    dspic33_raise_interrupt(source, 0u);
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) != 0u &&
               source->device_cycles - device_cycles == 2u,
           "masked interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 0u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "priority-zero interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    source->sr = 0x0020u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "IPL-blocked interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    source->pc = 0u;
    source->interrupt_deferred[0] = 1u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "deferred interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    configure_doze_interrupt(source, 1u);
    dspic33_write_word(source, 0x08c2u, 0u);
    source->pc = 0u;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING && source->interrupt_count == 0u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) != 0u,
           "GIE-disabled interrupt does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR,
                       MAIN_CLOCK_RECOVER_INTERRUPT | 0x1800u);
    dspic33_load_program_word(source, 0x0008u, 0x0300u);
    dspic33_load_program_word(source, 0x0300u, OPCODE_NOP);
    dspic33_set_working_register(source, 15u, 0x1800u);
    dspic33_write_word(source, 0x08c6u, 1u);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    trap_result = dspic33_step(source);
    expect(state,
           trap_result == DSPIC33_TRAPPED && source->last_trap == 2u &&
               source->pc == 0x0302u &&
               (dspic33_read_word(source, MAIN_CLOCK_DIVISOR) &
                MAIN_CLOCK_DOZE_ENABLE) != 0u &&
               source->device_cycles - device_cycles == 2u,
           "trap entry does not recover from DOZE");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x3800u);
    expect(state, dspic33_copy(copy, source), "copy preserves DOZE state");
    expect(state, dspic33_read_word(copy, MAIN_CLOCK_DIVISOR) == 0x3800u,
           "copy retains the selected DOZE ratio");
    dspic33_load_program_word(source, 0u, OPCODE_RESET);
    source->pc = 0u;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_RUNNING &&
               dspic33_read_word(source, MAIN_CLOCK_DIVISOR) == 0x3800u &&
               source->device_cycles - device_cycles == 8u,
           "warm reset preserves DOZE and finishes at the old ratio");
    dspic33_reset(copy, 0u);
    expect(state, dspic33_read_word(copy, MAIN_CLOCK_DIVISOR) == 0x3040u,
           "POR restores the configured clock divisor");

    dspic33_reset(source, 0u);
    dspic33_write_word(source, MAIN_CLOCK_DIVISOR, 0x7800u);
    dspic33_load_program_word(source, 0u, OPCODE_NOP);
    source->pc = 0u;
    source->device_cycles = UINT64_MAX - 100u;
    cpu_cycles = source->cycles;
    device_cycles = source->device_cycles;
    expect(state,
           dspic33_step(source) == DSPIC33_EVENT_QUEUE_ERROR &&
               source->cycles == cpu_cycles && source->device_cycles == device_cycles,
           "DOZE device-cycle overflow reports an event error without clock advance");
}

static void lifecycle_cases(TestState* state, Dspic33* source, Dspic33* copy) {
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
    write_protected_byte(source, OSCILLATOR_CONTROL + 1u, 0x03u);
    load_sequence(source, OPCODE_MOV_BYTE_W2_W1, OPCODE_MOV_BYTE_W3_W1,
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
    expect(state, control(source) == 0x0301u && source->events.count == 0u,
           "failed schedule leaves requested switch state visible");

    dspic33_reset(source, 0u);
    load_sequence(source, OPCODE_MOV_BYTE_W2_W1, OPCODE_RESET, OPCODE_MOV_BYTE_W3_W1);
    dspic33_load_program_word(source, 0x0206u, OPCODE_MOV_BYTE_W0_W1);
    source->pc = 0x0200u;
    dspic33_set_working_register(source, 0u, 0x40u);
    dspic33_set_working_register(source, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(source, 2u, 0x46u);
    dspic33_set_working_register(source, 3u, 0x57u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "reset invalidation first key executes");
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "reset executes between protected keys");
    source->pc = 0x0204u;
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
    TestState state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    dspic33_load_configuration_word(&source, 0xf80008u, 0x005eu);
    reset_cases(&state, &source);
    protection_cases(&state, &source);
    switch_cases(&state, &source);
    failure_trap_cases(&state, &source);
    source_admission_matrix_cases(&state, &source);
    fail_safe_matrix_cases(&state, &source);
    configuration_admission_cases(&state, &source);
    hardware_failure_cases(&state, &source);
    pll_lock_sequence_cases(&state, &source, &copy);
    two_speed_startup_cases(&state, &source, &copy);
    reference_clock_cases(&state, &source, &copy);
    reference_clock_pin_cases(&state, &source, &copy);
    main_pll_configuration_cases(&state, &source, &copy);
    doze_cases(&state, &source, &copy);
    oscillator_pin_cases(&state, &source, &copy);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 481u, "oscillator assertion arithmetic");
    printf("[oscillator-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32
           "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
