#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} EventConformance;

static void expect(EventConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[event-failed] %s\n", name);
    }
}

static bool interrupt_flag(const Dspic33* cpu, uint16_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t value =
        (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    return (value & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static bool interrupt_range(const Dspic33* cpu, uint16_t count) {
    uint16_t irq;
    for (irq = 0u; irq < count; irq++) {
        if (!interrupt_flag(cpu, irq)) {
            return false;
        }
    }
    return true;
}

static void delayed_interrupt_cases(EventConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, cpu->events.count == 0u, "reset clears event queue");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 3u, 0u, 3u),
           "schedule delayed interrupt");
    expect(state, cpu->events.count == 1u, "delayed interrupt queued");
    expect(state, dspic33_device_advance(cpu, 2u), "advance before event");
    expect(state, !interrupt_flag(cpu, 3u), "delayed interrupt not early");
    expect(state, cpu->events.count == 1u, "early advance retains event");
    expect(state, dspic33_device_advance(cpu, 1u), "advance to event");
    expect(state, interrupt_flag(cpu, 3u), "delayed interrupt dispatched");
    expect(state, cpu->events.count == 0u, "dispatched event removed");
    expect(state, cpu->cycles == 3u, "event cycles accumulated");
}

static void relative_and_zero_delay_cases(EventConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 10u), "advance event epoch");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 3u),
           "schedule relative interrupt");
    expect(state, cpu->events.items[0].cycle == 13u, "relative event cycle");
    expect(state, dspic33_device_advance(cpu, 2u), "relative event early advance");
    expect(state, !interrupt_flag(cpu, 1u), "relative interrupt not early");
    expect(state, dspic33_device_advance(cpu, 1u), "relative event due advance");
    expect(state, interrupt_flag(cpu, 1u), "relative interrupt dispatched");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 5u, 0u, 0u),
           "schedule zero-delay interrupt");
    expect(state, !interrupt_flag(cpu, 5u), "zero-delay event remains queued");
    expect(state, dspic33_device_advance(cpu, 0u), "process zero-delay event");
    expect(state, interrupt_flag(cpu, 5u), "zero-delay interrupt dispatched");
}

static void ordering_cases(EventConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0x1234u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0xabcdu, 1u),
           "schedule equal-cycle events");
    expect(state, dspic33_device_advance(cpu, 1u), "dispatch equal-cycle events");
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0xabcdu,
           "equal-cycle sequence order");

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0x1111u, 5u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0x2222u, 2u),
           "schedule reverse-cycle events");
    expect(state, dspic33_device_advance(cpu, 2u), "dispatch earlier event");
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0x2222u,
           "earlier event ordered first");
    expect(state, cpu->events.count == 1u, "later event retained");
    expect(state, dspic33_device_advance(cpu, 3u), "dispatch later event");
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0x1111u,
           "later event ordered second");
}

static void growth_and_overflow_cases(EventConformance* state, Dspic33* cpu) {
    bool scheduled = true;
    uint16_t irq;
    dspic33_reset(cpu, 0u);
    for (irq = 0u; irq < 96u; irq++) {
        scheduled =
            scheduled && dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, irq, 0u, 1u);
    }
    expect(state, scheduled, "grow event queue");
    expect(state, cpu->events.count == 96u, "grown event count");
    expect(state, cpu->events.capacity >= 96u, "grown event capacity");
    expect(state, cpu->events.sequence == 96u, "event sequence monotonic");
    expect(state, dspic33_device_advance(cpu, 1u), "dispatch grown event queue");
    expect(state, cpu->events.count == 0u, "grown event queue drained");
    expect(state, interrupt_range(cpu, 96u), "grown event queue complete");

    dspic33_reset(cpu, 0u);
    cpu->cycles = UINT64_MAX - 1u;
    cpu->device_cycles = UINT64_MAX - 1u;
    expect(state, !dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 2u),
           "reject event cycle overflow");
    expect(state, cpu->events.count == 0u, "overflow event not queued");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u),
           "schedule maximum event cycle");
    expect(state, cpu->events.items[0].cycle == UINT64_MAX,
           "maximum event cycle retained");
    expect(state, dspic33_device_advance(cpu, 1u), "dispatch maximum event cycle");
    expect(state, interrupt_flag(cpu, 0u), "maximum event cycle dispatched");
}

static void paused_device_clock_cases(EventConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 6u, 0u, 3u),
           "schedule event before device clock pause");
    dspic33_set_async_events(cpu, false);
    expect(state, dspic33_device_advance(cpu, 5u), "advance paused device clock");
    expect(state, cpu->cycles == 5u, "paused device clock retains CPU time");
    expect(state, cpu->device_cycles == 0u, "paused device clock retains epoch");
    expect(state, !interrupt_flag(cpu, 6u), "paused event remains pending");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 7u, 0u, 2u),
           "schedule event during device clock pause");
    dspic33_set_async_events(cpu, true);
    expect(state, dspic33_device_advance(cpu, 2u), "resume device clock");
    expect(state, interrupt_flag(cpu, 7u), "paused epoch event dispatched");
    expect(state, !interrupt_flag(cpu, 6u), "later paused event remains pending");
    expect(state, dspic33_device_advance(cpu, 1u), "reach retained event");
    expect(state, interrupt_flag(cpu, 6u), "retained event dispatched");
}

static void copy_and_reset_cases(EventConformance* state, Dspic33* cpu, Dspic33* copy) {
    dspic33_reset(cpu, 0u);
    dspic33_reset(copy, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 4u, 0u, 7u),
           "schedule copied event");
    expect(state, dspic33_copy(copy, cpu), "copy event queue");
    expect(state, copy->events.count == 1u, "copied event count");
    expect(state, copy->events.items != cpu->events.items,
           "copied event storage independent");
    expect(state, dspic33_device_advance(copy, 6u), "advance copied event early");
    expect(state, !interrupt_flag(copy, 4u), "copied event not early");
    expect(state, dspic33_device_advance(copy, 1u), "dispatch copied event");
    expect(state, interrupt_flag(copy, 4u), "copied event dispatched");
    expect(state, !interrupt_flag(cpu, 4u), "source event state independent");
    expect(state, cpu->events.count == 1u, "source event queue independent");
    dspic33_reset(cpu, 0u);
    expect(state, cpu->events.count == 0u, "reset discards queued events");
    expect(state, cpu->events.sequence == 0u, "reset clears event sequence");
}

int main(void) {
    EventConformance state = {0u, 0u, 0u};
    Dspic33 cpu;
    Dspic33 copy;
    bool cpu_initialized = dspic33_initialize(&cpu);
    bool copy_initialized = dspic33_initialize(&copy);
    expect(&state, cpu_initialized, "initialize event processor");
    expect(&state, copy_initialized, "initialize event copy");
    if (cpu_initialized && copy_initialized) {
        delayed_interrupt_cases(&state, &cpu);
        relative_and_zero_delay_cases(&state, &cpu);
        ordering_cases(&state, &cpu);
        growth_and_overflow_cases(&state, &cpu);
        paused_device_clock_cases(&state, &cpu);
        copy_and_reset_cases(&state, &cpu, &copy);
    }
    if (copy_initialized) {
        dspic33_destroy(&copy);
    }
    if (cpu_initialized) {
        dspic33_destroy(&cpu);
    }
    printf("[event-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
