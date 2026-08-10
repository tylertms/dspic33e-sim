#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

enum {
    AUXILIARY_CLOCK_CONTROL = 0x0758u,
    AUXILIARY_PLL_ENABLE = 0x8000u,
    AUXILIARY_PLL_LOCK = 0x4000u,
    AUXILIARY_CLOCK_WRITABLE = 0xbee7u
};

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} AuxiliaryClockConformance;

static void expect(AuxiliaryClockConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[auxiliary-clock-failed] %s\n", name);
    }
}

static uint16_t control(Dspic33* cpu) {
    return dspic33_read_word(cpu, AUXILIARY_CLOCK_CONTROL);
}

static void access_cases(AuxiliaryClockConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, control(cpu) == 0u, "POR clears ACLKCON3");
    expect(state, cpu->io.auxiliary_pll_generation == 0u,
           "POR clears auxiliary PLL generation");
    expect(state, cpu->events.count == 0u, "POR has no auxiliary PLL event");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0xffffu);
    expect(state, control(cpu) == AUXILIARY_CLOCK_WRITABLE,
           "ACLKCON3 word write applies target mask");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "APLLCK rejects software set");
    expect(state, (control(cpu) & 0x0118u) == 0u, "ACLKCON3 reserved bits read zero");
    expect(state, dspic33_read_byte(cpu, AUXILIARY_CLOCK_CONTROL + 1u) == 0xbeu,
           "ACLKCON3 high-byte read exposes implemented fields");
    expect(state, dspic33_read_byte(cpu, AUXILIARY_CLOCK_CONTROL) == 0xe7u,
           "ACLKCON3 low-byte read exposes implemented fields");

    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL, 0u);
    expect(state, control(cpu) == 0xbe00u,
           "ACLKCON3 low-byte write preserves high fields");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL, 0xffu);
    expect(state, control(cpu) == AUXILIARY_CLOCK_WRITABLE,
           "ACLKCON3 low-byte write applies low mask");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL + 1u, 0u);
    expect(state, control(cpu) == 0x00e7u,
           "ACLKCON3 high-byte write preserves low fields");
    dspic33_write_byte(cpu, AUXILIARY_CLOCK_CONTROL + 1u, 0xffu);
    expect(state, control(cpu) == AUXILIARY_CLOCK_WRITABLE,
           "ACLKCON3 high-byte write applies high mask");
}

static void lock_cases(AuxiliaryClockConformance* state, Dspic33* cpu) {
    uint32_t generation;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    expect(state, control(cpu) == AUXILIARY_PLL_ENABLE,
           "enabling auxiliary PLL starts unlocked");
    expect(state, cpu->events.count == 1u, "enable schedules one lock event");
    expect(state, cpu->io.auxiliary_pll_generation == 1u,
           "enable advances lock generation");
    expect(state, dspic33_device_advance(cpu, 31u), "advance before lock deadline");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "auxiliary PLL stays unlocked before deadline");
    expect(state, cpu->events.count == 1u, "lock event remains before deadline");
    expect(state, dspic33_device_advance(cpu, 1u), "advance to lock deadline");
    expect(state, control(cpu) == (AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_LOCK),
           "auxiliary PLL locks at modeled deadline");
    expect(state, cpu->events.count == 0u, "lock event is consumed");
    expect(state, cpu->io.auxiliary_pll_generation == 1u,
           "lock completion preserves generation");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    expect(state, cpu->events.count == 0u, "same configuration does not relock");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "same configuration preserves lock");
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL,
                       AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_LOCK);
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "software lock write cannot clear hardware lock");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0u);
    expect(state, control(cpu) == 0u, "disable clears auxiliary PLL lock");
    expect(state, cpu->events.count == 0u, "disable leaves no new lock event");
    expect(state, cpu->io.auxiliary_pll_generation == 2u,
           "disable invalidates lock generation");
    expect(state, dspic33_device_advance(cpu, 64u) && control(cpu) == 0u,
           "disabled auxiliary PLL cannot relock");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    expect(state, control(cpu) == AUXILIARY_PLL_ENABLE,
           "reenable starts a fresh unlocked interval");
    expect(state, cpu->events.count == 1u, "reenable schedules fresh lock event");
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               control(cpu) == (AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_LOCK),
           "reenabled auxiliary PLL reaches lock");

    generation = cpu->io.auxiliary_pll_generation;
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE | 0x0200u);
    expect(state, control(cpu) == (AUXILIARY_PLL_ENABLE | 0x0200u),
           "live source reconfiguration clears lock");
    expect(state, cpu->events.count == 1u, "live reconfiguration schedules relock");
    expect(state, cpu->io.auxiliary_pll_generation == generation + 1u,
           "live reconfiguration advances generation");
    expect(state,
           dspic33_device_advance(cpu, 31u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "reconfigured PLL remains unlocked before deadline");
    expect(state,
           dspic33_device_advance(cpu, 1u) && (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "reconfigured PLL locks at fresh deadline");
}

static void stale_event_cases(AuxiliaryClockConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    dspic33_device_advance(cpu, 1u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE | 0x0020u);
    expect(state, cpu->events.count == 2u,
           "reconfiguration retains stale event for generation check");
    expect(state, cpu->io.auxiliary_pll_generation == 2u,
           "reconfiguration creates second generation");
    expect(state,
           dspic33_device_advance(cpu, 31u) &&
               (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "stale completion cannot lock current generation");
    expect(state, cpu->events.count == 1u, "stale completion consumes only old event");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance to current generation event");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) != 0u,
           "current generation completion sets lock");
    expect(state, cpu->events.count == 0u, "current completion drains lock events");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    dspic33_device_advance(cpu, 1u);
    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0u);
    expect(state, cpu->events.count == 1u, "disable retains stale queued completion");
    expect(state, dspic33_device_advance(cpu, 31u) && control(cpu) == 0u,
           "disabled generation rejects stale completion");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance after disabled stale deadline");
    expect(state, cpu->events.count == 0u, "disabled stale event is consumed");

    dspic33_write_word(cpu, AUXILIARY_CLOCK_CONTROL, 0x0020u);
    expect(state, cpu->events.count == 0u,
           "disabled configuration change schedules no lock");
    expect(state, cpu->io.auxiliary_pll_generation == 3u,
           "disabled configuration change advances generation");
    expect(state, (control(cpu) & AUXILIARY_PLL_LOCK) == 0u,
           "disabled configuration remains unlocked");
}

static void lifecycle_cases(AuxiliaryClockConformance* state, Dspic33* source,
                            Dspic33* copy) {
    uint32_t generation;
    dspic33_reset(source, 0u);
    dspic33_reset(copy, 0u);
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    dspic33_device_advance(source, 10u);
    expect(state, dspic33_copy(copy, source), "copy preserves auxiliary PLL state");
    expect(state,
           copy->io.auxiliary_pll_generation == source->io.auxiliary_pll_generation,
           "copy preserves auxiliary PLL generation");
    expect(state, copy->events.count == source->events.count,
           "copy preserves pending lock event");
    expect(state,
           dspic33_device_advance(source, 22u) &&
               (control(source) & AUXILIARY_PLL_LOCK) != 0u,
           "source completes copied lock interval");
    expect(state,
           dspic33_device_advance(copy, 22u) &&
               (control(copy) & AUXILIARY_PLL_LOCK) != 0u,
           "copy independently completes lock interval");
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL, 0u);
    expect(state, (control(copy) & AUXILIARY_PLL_LOCK) != 0u,
           "source mutation does not alter copied lock state");

    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    dspic33_device_advance(source, 10u);
    expect(state, source->events.count == 1u,
           "cold reset starts with pending lock event");
    dspic33_reset(source, 0u);
    expect(state, control(source) == 0u, "cold reset clears ACLKCON3");
    expect(state, source->io.auxiliary_pll_generation == 0u,
           "cold reset clears lock generation");
    expect(state, source->events.count == 0u, "cold reset cancels lock events");
    expect(state, dspic33_device_advance(source, 32u) && control(source) == 0u,
           "cold reset prevents stale relock");

    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    dspic33_device_advance(source, 10u);
    generation = source->io.auxiliary_pll_generation;
    dspic33_load_program_word(source, 0u, 0xfe0000u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes with pending auxiliary PLL lock");
    expect(state,
           control(source) == AUXILIARY_PLL_ENABLE &&
               source->io.auxiliary_pll_generation == generation,
           "warm reset preserves enabled unlocked auxiliary PLL generation");
    expect(state, source->events.count == 1u,
           "warm reset preserves pending auxiliary PLL lock event");
    expect(state, source->events.items[0].cycle - source->device_cycles == 21u,
           "warm reset preserves remaining auxiliary PLL lock interval");
    expect(state,
           dspic33_device_advance(source, 20u) &&
               (control(source) & AUXILIARY_PLL_LOCK) == 0u,
           "warm-reset auxiliary PLL remains unlocked before preserved deadline");
    expect(state,
           dspic33_device_advance(source, 1u) &&
               control(source) == (AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_LOCK),
           "warm-reset auxiliary PLL locks at preserved deadline");
    dspic33_load_program_word(source, 0u, 0xfe0000u);
    expect(state, dspic33_step(source) == DSPIC33_RUNNING,
           "warm reset executes with locked auxiliary PLL");
    expect(state,
           control(source) == (AUXILIARY_PLL_ENABLE | AUXILIARY_PLL_LOCK) &&
               source->io.auxiliary_pll_generation == generation &&
               source->events.count == 0u,
           "warm reset preserves locked auxiliary PLL state");

    dspic33_reset(source, 0u);
    source->device_cycles = UINT64_MAX;
    dspic33_write_word(source, AUXILIARY_CLOCK_CONTROL, AUXILIARY_PLL_ENABLE);
    expect(state, source->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "lock schedule overflow reports event error");
    expect(state, control(source) == AUXILIARY_PLL_ENABLE,
           "failed schedule leaves enabled PLL unlocked");
    expect(state, source->events.count == 0u,
           "failed lock schedule leaves no pending event");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    AuxiliaryClockConformance state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    access_cases(&state, &source);
    lock_cases(&state, &source);
    stale_event_cases(&state, &source);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 73u, "auxiliary clock assertion arithmetic");
    printf("[auxiliary-clock-summary] cases=%" PRIu32 " passed=%" PRIu32
           " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
