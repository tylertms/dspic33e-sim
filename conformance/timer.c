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
} TimerConformance;

enum { TIMER_DMA_WRITE_PAD = 0x0298u };

static const uint16_t timer_registers[DSPIC33_TIMER_COUNT] = {
    0x0100u, 0x0106u, 0x010au, 0x0114u, 0x0118u, 0x0122u, 0x0126u, 0x0130u, 0x0134u};
static const uint16_t timer_periods[DSPIC33_TIMER_COUNT] = {
    0x0102u, 0x010cu, 0x010eu, 0x011au, 0x011cu, 0x0128u, 0x012au, 0x0136u, 0x0138u};
static const uint16_t timer_controls[DSPIC33_TIMER_COUNT] = {
    0x0104u, 0x0110u, 0x0112u, 0x011eu, 0x0120u, 0x012cu, 0x012eu, 0x013au, 0x013cu};
static const uint16_t timer_holding_registers[4] = {0x0108u, 0x0116u, 0x0124u, 0x0132u};
static const uint16_t timer_masks[DSPIC33_TIMER_COUNT] = {
    0xa076u, 0xa07au, 0xa072u, 0xa07au, 0xa072u, 0xa07au, 0xa072u, 0xa07au, 0xa072u};
static const uint8_t timer_irqs[DSPIC33_TIMER_COUNT] = {3u,  7u,  8u,  27u, 28u,
                                                        47u, 48u, 51u, 52u};

static void expect(TimerConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[timer-failed] %s\n", name);
    }
}

static uint16_t stored_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static bool interrupt_flag(Dspic33* cpu, uint8_t timer) {
    uint8_t irq = timer_irqs[timer];
    return (dspic33_read_word(cpu, (uint16_t)(0x0800u + (irq / 16u) * 2u)) &
            (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t timer) {
    uint8_t irq = timer_irqs[timer];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t bit = (uint16_t)(1u << (irq % 16u));
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~bit));
}

static void configure_16_bit(Dspic33* cpu, uint8_t timer, uint16_t counter,
                             uint16_t period, uint16_t control) {
    dspic33_write_word(cpu, timer_controls[timer], 0u);
    dspic33_write_word(cpu, timer_registers[timer], counter);
    dspic33_write_word(cpu, timer_periods[timer], period);
    clear_interrupt(cpu, timer);
    dspic33_write_word(cpu, timer_controls[timer], (uint16_t)(control | 0x8000u));
}

static void configure_dma(Dspic33* cpu, uint8_t request, uint16_t source,
                          uint16_t destination) {
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, request);
    dspic33_write_word(cpu, 0x0b04u, source);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, destination);
    dspic33_write_word(cpu, 0x0b0eu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0xa001u);
}

static void register_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    uint8_t pair;
    dspic33_reset(cpu, 0u);
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        expect(state, dspic33_read_word(cpu, timer_periods[timer]) == 0xffffu,
               "period reset");
        expect(state, dspic33_read_word(cpu, timer_controls[timer]) == 0u,
               "control reset");
        dspic33_write_word(cpu, timer_controls[timer], 0xffffu);
        expect(state,
               dspic33_read_word(cpu, timer_controls[timer]) == timer_masks[timer],
               "control mask");
        dspic33_write_word(cpu, timer_registers[timer], 0x5aa5u);
        dspic33_write_word(cpu, timer_periods[timer], 0xa55au);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0x5aa5u,
               "counter read write");
        expect(state, dspic33_read_word(cpu, timer_periods[timer]) == 0xa55au,
               "period read write");
    }
    for (pair = 0u; pair < 4u; pair++) {
        dspic33_write_word(cpu, timer_holding_registers[pair], 0xa500u + pair);
        expect(state,
               dspic33_read_word(cpu, timer_holding_registers[pair]) ==
                   (uint16_t)(0xa500u + pair),
               "holding register read write");
    }
}

static void internal_counting_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 9u, 0u);
        expect(state, dspic33_device_advance(cpu, 8u), "advance before match");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 8u,
               "counter before match");
        expect(state, !interrupt_flag(cpu, timer), "no early interrupt");
        expect(state, dspic33_device_advance(cpu, 1u), "advance to match");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 9u,
               "counter holds period");
        expect(state, !interrupt_flag(cpu, timer), "interrupt delayed after match");
        expect(state, dspic33_device_advance(cpu, 1u), "advance after match");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "counter resets after match");
        expect(state, interrupt_flag(cpu, timer), "period interrupt");
        clear_interrupt(cpu, timer);
        expect(state, dspic33_device_advance(cpu, 25u), "advance multiple periods");
        expect(state, !interrupt_flag(cpu, timer), "multiple period interrupt delayed");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance after multiple periods");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 6u,
               "multiple period remainder");
        expect(state, interrupt_flag(cpu, timer), "multiple period interrupt");
    }
}

static void prescaler_cases(TimerConformance* state, Dspic33* cpu) {
    static const uint16_t controls[4] = {0x0000u, 0x0010u, 0x0020u, 0x0030u};
    static const uint16_t divisors[4] = {1u, 8u, 64u, 256u};
    uint8_t timer;
    uint8_t prescaler;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        for (prescaler = 0u; prescaler < 4u; prescaler++) {
            dspic33_reset(cpu, 0u);
            configure_16_bit(cpu, timer, 0u, 0xffffu, controls[prescaler]);
            expect(state, dspic33_device_advance(cpu, divisors[prescaler] - 1u),
                   "advance within prescaler");
            expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
                   "prescaler retains fraction");
            expect(state, dspic33_device_advance(cpu, 1u), "advance prescaler edge");
            expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 1u,
                   "prescaler increments counter");
        }
    }
}

static void boundary_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 0u, 0u);
        expect(state, dspic33_device_advance(cpu, 1000u), "advance zero period");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "zero period holds zero");
        expect(state, !interrupt_flag(cpu, timer), "zero period suppresses interrupt");

        configure_16_bit(cpu, timer, 0xfffeu, 3u, 0u);
        expect(state, dspic33_device_advance(cpu, 5u), "advance counter above period");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 3u,
               "counter wraps before lower period match");
        expect(state, !interrupt_flag(cpu, timer), "lower period match delayed");
        expect(state, dspic33_device_advance(cpu, 1u), "reset lower period");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "lower period resets after match");
        expect(state, interrupt_flag(cpu, timer), "lower period interrupt");

        configure_16_bit(cpu, timer, 0xfffeu, 0xffffu, 0u);
        expect(state, dspic33_device_advance(cpu, 1u), "match maximum period");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0xffffu,
               "maximum period holds at match");
        expect(state, !interrupt_flag(cpu, timer), "maximum period interrupt delayed");
        expect(state, dspic33_device_advance(cpu, 1u), "advance maximum period");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "maximum period resets");
        expect(state, interrupt_flag(cpu, timer), "maximum period interrupt");
    }
}

static void prescaler_reset_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 100u, 0x0010u);
        dspic33_device_advance(cpu, 7u);
        dspic33_write_word(cpu, timer_controls[timer], 0x8010u);
        dspic33_device_advance(cpu, 1u);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "control write clears prescaler");
        dspic33_device_advance(cpu, 7u);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 1u,
               "control reset prescaler completes");
        dspic33_device_advance(cpu, 7u);
        dspic33_write_word(cpu, timer_registers[timer], 5u);
        dspic33_device_advance(cpu, 1u);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 5u,
               "counter write clears prescaler");
    }
}

static void gate_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 100u, 0x0040u);
        expect(state, dspic33_device_advance(cpu, 5u), "advance closed gate");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "closed gate stops timer");
        expect(state, dspic33_timer_gate(cpu, timer, true, 0u), "queue open gate");
        expect(state, dspic33_device_advance(cpu, 0u), "open gate");
        expect(state, dspic33_device_advance(cpu, 5u), "advance open gate");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 5u,
               "open gate counts");
        expect(state, !interrupt_flag(cpu, timer), "open gate no interrupt");
        expect(state, dspic33_timer_gate(cpu, timer, false, 0u), "queue close gate");
        expect(state, dspic33_device_advance(cpu, 0u), "close gate");
        expect(state, interrupt_flag(cpu, timer), "falling gate interrupt");

        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 2u, 0x0040u);
        dspic33_timer_gate(cpu, timer, true, 0u);
        dspic33_device_advance(cpu, 0u);
        dspic33_device_advance(cpu, 3u);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "gated timer resets after period");
        expect(state, !interrupt_flag(cpu, timer),
               "gated period does not raise interrupt");
        dspic33_timer_gate(cpu, timer, false, 0u);
        dspic33_device_advance(cpu, 0u);
        expect(state, interrupt_flag(cpu, timer),
               "gated falling edge raises interrupt after period");
    }
}

static void external_clock_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        uint16_t expected_first = (timer == 0u || (timer & 1u) != 0u) ? 0u : 1u;
        uint32_t pulses_to_match = expected_first == 0u ? 3u : 2u;
        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 2u, 0x0002u);
        expect(state, dspic33_timer_pulse(cpu, timer, 1u, 0u),
               "queue first external edge");
        expect(state, dspic33_device_advance(cpu, 0u), "apply first external edge");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == expected_first,
               "external first edge behavior");
        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 2u, 0x0002u);
        expect(state, dspic33_timer_pulse(cpu, timer, pulses_to_match, 0u),
               "queue external match edges");
        expect(state, dspic33_device_advance(cpu, 0u), "apply external match edges");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 2u,
               "external counter reaches period");
        expect(state, !interrupt_flag(cpu, timer), "external match interrupt delayed");
        expect(state, dspic33_timer_pulse(cpu, timer, 1u, 0u),
               "queue external reset edge");
        expect(state, dspic33_device_advance(cpu, 0u), "apply external reset edge");
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "external counter resets");
        expect(state, !interrupt_flag(cpu, timer),
               "external interrupt waits for instruction cycle");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance external interrupt delay");
        expect(state, interrupt_flag(cpu, timer), "external period interrupt");

        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 100u, 0x0042u);
        dspic33_timer_pulse(cpu, timer, 4u, 0u);
        dspic33_device_advance(cpu, 0u);
        expect(state,
               dspic33_read_word(cpu, timer_registers[timer]) ==
                   (uint16_t)(expected_first + 3u),
               "counter mode ignores gate selection");
    }
}

static void delayed_event_cases(TimerConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_16_bit(cpu, 2u, 0u, 100u, 0x0002u);
    expect(state, dspic33_timer_pulse(cpu, 2u, 4u, 5u), "queue delayed pulses");
    expect(state, dspic33_device_advance(cpu, 4u), "advance before delayed pulses");
    expect(state, dspic33_read_word(cpu, timer_registers[2]) == 0u,
           "delayed pulses remain queued");
    expect(state, dspic33_device_advance(cpu, 1u), "advance to delayed pulses");
    expect(state, dspic33_read_word(cpu, timer_registers[2]) == 4u,
           "delayed pulses apply");

    dspic33_reset(cpu, 0u);
    configure_16_bit(cpu, 0u, 0u, 100u, 0x0040u);
    expect(state, dspic33_timer_gate(cpu, 0u, true, 3u), "queue delayed gate");
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_read_word(cpu, timer_registers[0]) == 0u,
           "delayed gate stays closed");
    dspic33_device_advance(cpu, 1u);
    dspic33_device_advance(cpu, 2u);
    expect(state, dspic33_read_word(cpu, timer_registers[0]) == 2u,
           "delayed gate opens");
}

static void paired_timer_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t low;
    for (low = 1u; low < DSPIC33_TIMER_COUNT; low += 2u) {
        uint8_t high = (uint8_t)(low + 1u);
        uint16_t holding = timer_holding_registers[low / 2u];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_controls[low], 0x0008u);
        dspic33_write_word(cpu, holding, 0x1234u);
        dspic33_write_word(cpu, timer_registers[low], 0xabcdu);
        expect(state, dspic33_read_word(cpu, timer_registers[high]) == 0x1234u,
               "paired low write transfers holding register");
        dspic33_write_word(cpu, timer_registers[high], 0x5678u);
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 0xabcdu,
               "paired low read returns low word");
        expect(state, dspic33_read_word(cpu, holding) == 0x5678u,
               "paired low read latches high word");

        dspic33_write_word(cpu, holding, 0u);
        dspic33_write_word(cpu, timer_registers[low], 0xfffeu);
        dspic33_write_word(cpu, timer_periods[low], 3u);
        dspic33_write_word(cpu, timer_periods[high], 1u);
        clear_interrupt(cpu, low);
        clear_interrupt(cpu, high);
        dspic33_write_word(cpu, timer_controls[low], 0x8008u);
        expect(state, dspic33_device_advance(cpu, 2u), "advance paired counter");
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 0u,
               "paired low counter overflow");
        expect(state, dspic33_read_word(cpu, timer_registers[high]) == 1u,
               "paired high counter increments");
        expect(state, !interrupt_flag(cpu, low) && !interrupt_flag(cpu, high),
               "paired counter no early interrupt");

        dspic33_write_word(cpu, holding, 0u);
        dspic33_write_word(cpu, timer_registers[low], 0u);
        dspic33_write_word(cpu, timer_periods[low], 2u);
        dspic33_write_word(cpu, timer_periods[high], 0u);
        clear_interrupt(cpu, low);
        clear_interrupt(cpu, high);
        expect(state, dspic33_device_advance(cpu, 2u), "advance paired to match");
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 2u,
               "paired counter holds period");
        expect(state, !interrupt_flag(cpu, high), "paired interrupt delayed");
        expect(state, dspic33_device_advance(cpu, 1u), "advance paired after match");
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 0u,
               "paired counter resets");
        expect(state, !interrupt_flag(cpu, low) && interrupt_flag(cpu, high),
               "paired counter uses high interrupt");

        dspic33_write_word(cpu, timer_controls[high], 0x8000u);
        dspic33_write_word(cpu, timer_registers[high], 0u);
        dspic33_device_advance(cpu, 1u);
        expect(state, dspic33_read_word(cpu, timer_registers[high]) == 0u,
               "paired mode ignores high timer control");
    }
}

static void paired_mode_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t low;
    for (low = 1u; low < DSPIC33_TIMER_COUNT; low += 2u) {
        uint8_t high = (uint8_t)(low + 1u);
        uint16_t holding = timer_holding_registers[low / 2u];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[low], 100u);
        dspic33_write_word(cpu, timer_periods[high], 0u);
        dspic33_write_word(cpu, timer_controls[low], 0x8008u | 0x0030u);
        dspic33_device_advance(cpu, 255u);
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 0u,
               "paired prescaler retains fraction");
        dspic33_device_advance(cpu, 1u);
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 1u,
               "paired prescaler increments");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[low], 2u);
        dspic33_write_word(cpu, timer_periods[high], 0u);
        dspic33_write_word(cpu, timer_controls[low], 0x800au);
        dspic33_timer_pulse(cpu, low, 3u, 0u);
        dspic33_device_advance(cpu, 0u);
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 2u,
               "paired external counter reaches period");
        expect(state, !interrupt_flag(cpu, high), "paired external interrupt delayed");
        dspic33_timer_pulse(cpu, low, 1u, 0u);
        dspic33_device_advance(cpu, 0u);
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 0u,
               "paired external counter resets");
        expect(state, !interrupt_flag(cpu, high),
               "paired external interrupt waits for instruction cycle");
        dspic33_device_advance(cpu, 1u);
        expect(state, interrupt_flag(cpu, high),
               "paired external counter uses high interrupt");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[low], 100u);
        dspic33_write_word(cpu, timer_periods[high], 0u);
        dspic33_write_word(cpu, timer_controls[low], 0x8048u);
        dspic33_device_advance(cpu, 5u);
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 0u,
               "paired closed gate stops counter");
        dspic33_timer_gate(cpu, low, true, 0u);
        dspic33_device_advance(cpu, 0u);
        dspic33_device_advance(cpu, 5u);
        expect(state, dspic33_read_word(cpu, timer_registers[low]) == 5u,
               "paired open gate counts");
        dspic33_timer_gate(cpu, low, false, 0u);
        dspic33_device_advance(cpu, 0u);
        expect(state, interrupt_flag(cpu, high),
               "paired falling gate uses high interrupt");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, timer_periods[low], 0u);
        dspic33_write_word(cpu, timer_periods[high], 0u);
        dspic33_write_word(cpu, holding, 0u);
        dspic33_write_word(cpu, timer_registers[low], 0u);
        dspic33_write_word(cpu, timer_controls[low], 0x8008u);
        dspic33_device_advance(cpu, 100u);
        expect(state,
               dspic33_read_word(cpu, timer_registers[low]) == 0u &&
                   dspic33_read_word(cpu, timer_registers[high]) == 0u,
               "paired zero period holds zero");
        expect(state, !interrupt_flag(cpu, high),
               "paired zero period suppresses interrupt");
    }
}

static void power_mode_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 100u, 0u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        dspic33_device_advance(cpu, 3u);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 3u,
               "timer runs in idle");

        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 100u, 0x2000u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        dspic33_device_advance(cpu, 3u);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "timer stops in idle");

        dspic33_reset(cpu, 0u);
        configure_16_bit(cpu, timer, 0u, 100u, 0u);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        dspic33_device_advance(cpu, 3u);
        expect(state, dspic33_read_word(cpu, timer_registers[timer]) == 0u,
               "internal timer stops in sleep");
    }

    dspic33_reset(cpu, 0u);
    configure_16_bit(cpu, 0u, 0u, 100u, 0x0002u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_timer_pulse(cpu, 0u, 2u, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_read_word(cpu, timer_registers[0]) == 1u,
           "asynchronous Timer1 runs in sleep");

    dspic33_reset(cpu, 0u);
    configure_16_bit(cpu, 0u, 0u, 100u, 0x0006u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_timer_pulse(cpu, 0u, 2u, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state, dspic33_read_word(cpu, timer_registers[0]) == 0u,
           "synchronous Timer1 stops in sleep");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, timer_periods[1], 100u);
    dspic33_write_word(cpu, timer_periods[2], 0u);
    dspic33_write_word(cpu, timer_controls[2], 0x2000u);
    dspic33_write_word(cpu, timer_controls[1], 0x8008u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_advance(cpu, 3u);
    expect(state, dspic33_read_word(cpu, timer_registers[1]) == 0u,
           "paired high TSIDL stops timer in idle");
}

static void dma_trigger_cases(TimerConformance* state, Dspic33* cpu) {
    uint8_t timer;
    for (timer = 0u; timer < DSPIC33_TIMER_COUNT; timer++) {
        uint16_t source = (uint16_t)(0x2800u + timer * 2u);
        bool supported = timer >= 1u && timer <= 4u;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, source, (uint16_t)(0x5100u + timer));
        configure_dma(cpu, timer_irqs[timer], source, TIMER_DMA_WRITE_PAD);
        configure_16_bit(cpu, timer, 0u, 1u, 0u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (cpu->io.timer_interrupt_pending & (uint16_t)(1u << timer)) != 0u,
               "advance timer period match before DMA classification");
        expect(state,
               stored_word(cpu, TIMER_DMA_WRITE_PAD) ==
                       (supported ? (uint16_t)(0x5100u + timer) : 0u) &&
                   cpu->io.dma_index[0] == 0u &&
                   ((cpu->io.dma_active & 1u) != 0u) == supported,
               supported ? "TMR2 through TMR5 period requests DMA"
                         : "unsupported timer period does not request DMA");
        expect(state, !interrupt_flag(cpu, timer), "timer DMA precedes interrupt flag");
    }
}

static void copy_cases(TimerConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize timer copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_16_bit(cpu, 1u, 7u, 100u, 0x0012u);
    dspic33_timer_gate(cpu, 1u, true, 4u);
    expect(state, dspic33_copy(&copy, cpu), "copy timer state");
    expect(state, copy.io.timer_enabled == cpu->io.timer_enabled,
           "copy timer enabled state");
    expect(state, copy.io.timer_fraction[1] == cpu->io.timer_fraction[1],
           "copy timer prescaler state");
    expect(state, copy.events.count == cpu->events.count, "copy timer events");
    dspic33_destroy(&copy);
}

int main(void) {
    TimerConformance state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize timer processor");
    if (initialized) {
        register_cases(&state, &cpu);
        internal_counting_cases(&state, &cpu);
        prescaler_cases(&state, &cpu);
        boundary_cases(&state, &cpu);
        prescaler_reset_cases(&state, &cpu);
        gate_cases(&state, &cpu);
        external_clock_cases(&state, &cpu);
        delayed_event_cases(&state, &cpu);
        paired_timer_cases(&state, &cpu);
        paired_mode_cases(&state, &cpu);
        power_mode_cases(&state, &cpu);
        dma_trigger_cases(&state, &cpu);
        copy_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[timer-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
