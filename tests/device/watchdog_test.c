#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

enum {
    RCON = 0x0740u,
    FWDT = 0xf8000au,
    WDTO = 0x0010u,
    SWDTEN = 0x0020u,
    SWR = 0x0040u,
    SLEEP = 0x0008u,
    IDLE = 0x0004u,
    FWDTEN = 0x80u,
    WINDIS = 0x40u,
    WDTPRE = 0x10u,
    OPCODE_NOP = 0x000000u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_IDLE = 0xfe4001u,
    OPCODE_CLRWDT = 0xfe6000u,
    OPCODE_RESET = 0xfe0000u,
    OSCILLATOR_CONTROL = 0x0742u,
    OSCILLATOR_SWITCH_ENABLE = 0x0001u,
    NVM_INTERRUPT = 0x8000u
};

static uint16_t rcon(Dspic33* cpu) { return dspic33_read_word(cpu, RCON); }

static bool configure(Dspic33* cpu, uint8_t value) {
    return dspic33_load_configuration_word(cpu, FWDT, (uint16_t)(0xff00u | value));
}

static Dspic33StopReason execute(Dspic33* cpu, uint32_t opcode) {
    if (!dspic33_load_program_word(cpu, 0u, opcode)) {
        return DSPIC33_PROGRAM_BOUNDS;
    }
    cpu->pc = 0u;
    return dspic33_step(cpu);
}

static void period_cases(TestState* state, Dspic33* cpu) {
    uint8_t prescaler;
    uint8_t postscaler;
    for (prescaler = 0u; prescaler < 2u; prescaler++) {
        for (postscaler = 0u; postscaler < 16u; postscaler++) {
            uint32_t period = (prescaler != 0u ? 128u : 32u) << postscaler;
            uint8_t configuration =
                (uint8_t)(FWDTEN | WINDIS | (prescaler != 0u ? WDTPRE : 0u) | postscaler);
            configure(cpu, configuration);
            dspic33_reset(cpu, 0u);
            expect(state, cpu->watchdog.ticks == 0u && (rcon(cpu) & WDTO) == 0u,
                   "period starts from cleared counter");
            dspic33_watchdog_advance_lprc(cpu, period - 1u);
            expect(state, cpu->watchdog.ticks == period - 1u,
                   "period accumulates every pre-timeout LPRC edge");
            expect(state, (rcon(cpu) & WDTO) == 0u && cpu->pc == 0u,
                   "period does not expire one edge early");
            dspic33_watchdog_advance_lprc(cpu, 1u);
            expect(state,
                   cpu->watchdog.ticks == 0u && (rcon(cpu) & WDTO) != 0u && cpu->pc == 0u &&
                       cpu->software_reset_count == 0u && cpu->illegal_reset_count == 0u,
                   "period expires at exact divider product as hardware reset");
        }
    }
}

static void software_enable_cases(TestState* state, Dspic33* cpu) {
    configure(cpu, WINDIS);
    dspic33_reset(cpu, 0u);
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, cpu->watchdog.ticks == 0u && (rcon(cpu) & WDTO) == 0u,
           "software-controlled watchdog starts disabled");

    cpu->watchdog.ticks = 7u;
    dspic33_write_word(cpu, RCON, (uint16_t)(rcon(cpu) | SWDTEN));
    expect(state, cpu->watchdog.ticks == 0u && (rcon(cpu) & SWDTEN) != 0u,
           "software enable clears counter");
    dspic33_watchdog_advance_lprc(cpu, 31u);
    expect(state, cpu->watchdog.ticks == 31u && (rcon(cpu) & WDTO) == 0u,
           "software-enabled watchdog counts");
    dspic33_write_word(cpu, RCON, rcon(cpu));
    expect(state, cpu->watchdog.ticks == 31u, "repeated software enable does not restart counter");
    dspic33_watchdog_advance_lprc(cpu, 1u);
    expect(state, (rcon(cpu) & (WDTO | SWDTEN)) == WDTO,
           "watchdog reset records timeout and clears software enable");
    dspic33_write_word(cpu, RCON, (uint16_t)(rcon(cpu) & ~WDTO));
    expect(state, (rcon(cpu) & WDTO) == 0u, "software clears sticky timeout flag");

    configure(cpu, (uint8_t)(FWDTEN | WINDIS));
    dspic33_reset(cpu, 0u);
    dspic33_watchdog_advance_lprc(cpu, 7u);
    dspic33_write_word(cpu, RCON, (uint16_t)(rcon(cpu) & ~SWDTEN));
    expect(state, cpu->watchdog.ticks == 7u, "hardware enable ignores software control writes");
    dspic33_watchdog_advance_lprc(cpu, 25u);
    expect(state, (rcon(cpu) & WDTO) != 0u,
           "hardware-enabled watchdog expires with software bit clear");

    configure(cpu, WINDIS);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, RCON, (uint16_t)(rcon(cpu) | SWDTEN));
    dspic33_watchdog_advance_lprc(cpu, 9u);
    expect(state, execute(cpu, OPCODE_RESET) == DSPIC33_RUNNING,
           "software reset executes while watchdog enabled");
    expect(state,
           cpu->watchdog.ticks == 0u && (rcon(cpu) & SWDTEN) == 0u && (rcon(cpu) & WDTO) == 0u &&
               cpu->software_reset_count == 1u,
           "software reset clears watchdog enable and counter");

    configure(cpu, (uint8_t)(FWDTEN | WINDIS));
    dspic33_reset(cpu, 0u);
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, execute(cpu, OPCODE_RESET) == DSPIC33_RUNNING,
           "software reset follows watchdog reset");
    expect(state, (rcon(cpu) & (WDTO | SWR)) == (WDTO | SWR),
           "later reset preserves sticky watchdog cause");
    expect(state, execute(cpu, OPCODE_CLRWDT) == DSPIC33_RUNNING && (rcon(cpu) & WDTO) != 0u,
           "CLRWDT does not clear sticky timeout cause");

    configure(cpu, WINDIS);
    dspic33_reset(cpu, 0u);
    configure(cpu, (uint8_t)(FWDTEN | WINDIS));
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, (rcon(cpu) & WDTO) != 0u, "configuration enable takes effect without reset");
    dspic33_reset(cpu, 0u);
    dspic33_watchdog_advance_lprc(cpu, 7u);
    configure(cpu, WINDIS);
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, cpu->watchdog.ticks == 0u && (rcon(cpu) & WDTO) == 0u,
           "configuration disable takes effect without reset");
}

static void window_cases(TestState* state, Dspic33* cpu) {
    uint8_t postscaler;
    for (postscaler = 0u; postscaler < 16u; postscaler++) {
        uint32_t period = 32u << postscaler;
        uint32_t threshold = period - period / 4u;
        configure(cpu, (uint8_t)(FWDTEN | postscaler));
        dspic33_reset(cpu, 0u);
        dspic33_watchdog_advance_lprc(cpu, threshold - 1u);
        execute(cpu, OPCODE_CLRWDT);
        expect(state, (rcon(cpu) & WDTO) != 0u && cpu->watchdog.ticks == 0u,
               "windowed clear before last quarter resets device");

        dspic33_reset(cpu, 0u);
        dspic33_watchdog_advance_lprc(cpu, threshold);
        expect(state,
               execute(cpu, OPCODE_CLRWDT) == DSPIC33_RUNNING && (rcon(cpu) & WDTO) == 0u &&
                   cpu->watchdog.ticks == 0u && cpu->pc == 2u,
               "windowed clear at last-quarter boundary restarts counter");
    }

    configure(cpu, (uint8_t)(FWDTEN | WINDIS));
    dspic33_reset(cpu, 0u);
    dspic33_watchdog_advance_lprc(cpu, 1u);
    expect(state,
           execute(cpu, OPCODE_CLRWDT) == DSPIC33_RUNNING && cpu->watchdog.ticks == 0u &&
               (rcon(cpu) & WDTO) == 0u,
           "nonwindowed clear is legal throughout period");

    configure(cpu, 0u);
    dspic33_reset(cpu, 0u);
    expect(state, execute(cpu, OPCODE_CLRWDT) == DSPIC33_RUNNING && (rcon(cpu) & WDTO) == 0u,
           "disabled windowed watchdog accepts clear");
}

static void power_cases(TestState* state, Dspic33* cpu) {
    configure(cpu, (uint8_t)(FWDTEN | WINDIS));
    dspic33_reset(cpu, 0u);
    dspic33_watchdog_advance_lprc(cpu, 10u);
    expect(state, execute(cpu, OPCODE_SLEEP) == DSPIC33_SLEEPING, "PWRSAV enters Sleep");
    expect(state,
           cpu->power_state == DSPIC33_POWER_SLEEP && cpu->watchdog.ticks == 0u &&
               (rcon(cpu) & SLEEP) != 0u,
           "Sleep entry clears watchdog counter");
    dspic33_watchdog_advance_lprc(cpu, 31u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_SLEEP && cpu->watchdog.ticks == 31u &&
               (rcon(cpu) & WDTO) == 0u,
           "watchdog continues counting in Sleep");
    dspic33_watchdog_advance_lprc(cpu, 1u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE && cpu->stop_reason == DSPIC33_RUNNING &&
               cpu->pc == 2u && cpu->watchdog.ticks == 0u &&
               (rcon(cpu) & (WDTO | SLEEP)) == (WDTO | SLEEP),
           "Sleep timeout wakes after PWRSAV without reset");
    expect(state, cpu->software_reset_count == 0u && cpu->illegal_reset_count == 0u,
           "Sleep timeout is not a software or illegal reset");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, cpu->pc == 0u && (rcon(cpu) & WDTO) != 0u,
           "next active timeout resets after Sleep wake");

    dspic33_reset(cpu, 0u);
    dspic33_watchdog_advance_lprc(cpu, 12u);
    expect(state, execute(cpu, OPCODE_IDLE) == DSPIC33_IDLING, "PWRSAV enters Idle");
    expect(state,
           cpu->power_state == DSPIC33_POWER_IDLE && cpu->watchdog.ticks == 0u &&
               (rcon(cpu) & IDLE) != 0u,
           "Idle entry clears watchdog counter");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE && cpu->pc == 2u && cpu->watchdog.ticks == 0u &&
               (rcon(cpu) & (WDTO | IDLE)) == (WDTO | IDLE),
           "Idle timeout wakes after PWRSAV without reset");

    configure(cpu, (uint8_t)(FWDTEN | WINDIS));
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0744u, 0x9800u);
    execute(cpu, OPCODE_IDLE);
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE && dspic33_read_word(cpu, 0x0744u) == 0x9800u,
           "watchdog wake preserves DOZE and ROI state");

    dspic33_reset(cpu, 0u);
    execute(cpu, OPCODE_IDLE);
    dspic33_watchdog_advance_lprc(cpu, 10u);
    dspic33_write_word(cpu, 0x0820u, 0x0008u);
    dspic33_device_advance(cpu, 2u);
    dspic33_load_program_word(cpu, 0x001au, 0x0300u);
    dspic33_load_program_word(cpu, 0x0300u, OPCODE_NOP);
    dspic33_set_working_register(cpu, 15u, 0x1000u);
    dspic33_raise_interrupt(cpu, 3u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_count == 1u &&
               cpu->last_interrupt == 3u && cpu->pc == 0x0302u && cpu->trap_count == 0u &&
               !cpu->illegal_reset,
           "interrupt wakes processor from Idle");
    expect(state, cpu->power_state == DSPIC33_POWER_ACTIVE && cpu->watchdog.ticks == 0u,
           "interrupt wake clears watchdog counter");

    configure(cpu, WINDIS);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, RCON, (uint16_t)(rcon(cpu) | SWDTEN));
    execute(cpu, OPCODE_SLEEP);
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE &&
               (rcon(cpu) & (WDTO | SWDTEN)) == (WDTO | SWDTEN),
           "software-enabled watchdog wakes Sleep without clearing enable");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, (rcon(cpu) & (WDTO | SWDTEN)) == WDTO,
           "active timeout after software wake resets and clears enable");
}

static void lifecycle_cases(TestState* state, Dspic33* source, Dspic33* copy) {
    configure(source, (uint8_t)(FWDTEN | WINDIS));
    dspic33_reset(source, 0u);
    dspic33_watchdog_advance_lprc(source, 15u);
    source->oscillator.active = true;
    source->oscillator.generation = 1u;
    source->data[OSCILLATOR_CONTROL] = OSCILLATOR_SWITCH_ENABLE;
    expect(state,
           dspic33_schedule(source, DSPIC33_EVENT_OSCILLATOR, 0u, 1u, 1u) &&
               dspic33_device_advance(source, 1u),
           "clock switch completion event executes");
    expect(state, source->watchdog.ticks == 0u, "clock switch completion clears watchdog counter");

    dspic33_watchdog_advance_lprc(source, 11u);
    dspic33_reset(copy, 0u);
    expect(state, dspic33_copy(copy, source), "copy preserves watchdog state");
    expect(state, copy->watchdog.ticks == 11u, "copy preserves watchdog counter value");
    dspic33_watchdog_advance_lprc(source, 5u);
    dspic33_watchdog_advance_lprc(copy, 7u);
    expect(state, source->watchdog.ticks == 16u && copy->watchdog.ticks == 18u,
           "copied watchdog counters advance independently");
    dspic33_watchdog_advance_lprc(source, 16u);
    expect(state, (rcon(source) & WDTO) != 0u && (rcon(copy) & WDTO) == 0u,
           "source timeout does not affect copied watchdog");
    dspic33_watchdog_advance_lprc(copy, 14u);
    expect(state, (rcon(copy) & WDTO) != 0u, "copied watchdog reaches its independent timeout");

    dspic33_reset(source, 0u);
    dspic33_watchdog_advance_lprc(source, 9u);
    dspic33_reset(source, 0u);
    expect(state, source->watchdog.ticks == 0u && (rcon(source) & WDTO) == 0u,
           "cold reset clears watchdog state");
}

static void nvm_cases(TestState* state, Dspic33* cpu) {
    configure(cpu, (uint8_t)(FWDTEN | WINDIS));
    dspic33_reset(cpu, 0u);
    cpu->pc = 0x0020u;
    cpu->data[0x1000u] = 0xa5u;
    cpu->nvm.active = true;
    cpu->nvm.control = 0x000fu;
    cpu->nvm.completion_cycle = cpu->cycles + 2u;
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_NVM, 0u, 0u, 2u),
           "NVM completion event schedules");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state, cpu->watchdog.reset_pending, "active NVM defers watchdog reset");
    expect(state, cpu->nvm.active && cpu->pc == 0x0020u && (rcon(cpu) & WDTO) == 0u,
           "deferred timeout leaves NVM and CPU state intact");
    expect(state, dspic33_device_advance(cpu, 1u), "NVM remains active before completion");
    expect(state, cpu->watchdog.reset_pending && cpu->pc == 0x0020u,
           "deferred reset remains pending before NVM completion");
    expect(state, dspic33_device_advance(cpu, 1u), "NVM completion executes");
    expect(state,
           !cpu->nvm.active && !cpu->watchdog.reset_pending && cpu->pc == 0u &&
               (rcon(cpu) & WDTO) != 0u,
           "NVM completion releases deferred watchdog reset");
    expect(state, (dspic33_read_word(cpu, 0x0800u) & NVM_INTERRUPT) == 0u,
           "deferred watchdog reset suppresses stale NVM interrupt");
    expect(state,
           cpu->data[0x1000u] == 0xa5u && cpu->software_reset_count == 0u &&
               cpu->illegal_reset_count == 0u,
           "deferred watchdog reset preserves RAM and hardware reset accounting");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    TestState state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    period_cases(&state, &source);
    software_enable_cases(&state, &source);
    window_cases(&state, &source);
    power_cases(&state, &source);
    lifecycle_cases(&state, &source, &copy);
    nvm_cases(&state, &source);
    dspic33_release(&copy);
    dspic33_release(&source);
    return test_finish(&state);
}
