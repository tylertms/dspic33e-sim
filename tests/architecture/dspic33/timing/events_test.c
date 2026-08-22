#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

static bool interrupt_flag(const Dspic33* cpu, uint16_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t value = (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
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

static size_t external_event_count(const Dspic33* cpu) {
    size_t count = 0u;
    size_t index;
    for (index = 0u; index < cpu->events.count; index++) {
        count += cpu->events.items[index].external ? 1u : 0u;
    }
    return count;
}

static size_t active_usb_pending_count(const Dspic33* cpu) {
    size_t count = 0u;
    size_t index;
    for (index = 0u; index < DSPIC33_USB_PENDING_COUNT; index++) {
        count += cpu->io.usb_pending[index].active ? 1u : 0u;
    }
    return count;
}

static bool single_external_event(const Dspic33* cpu, Dspic33EventType type) {
    return cpu->events.count == 1u && cpu->events.items[0].external &&
           cpu->events.items[0].type == type;
}

static void stop_reason_name_cases(TestState* state) {
    static const char* names[] = {
        "running",
        "returned",
        "stop point",
        "sleeping",
        "idling",
        "halted",
        "trap",
        "unsupported instruction",
        "program bounds",
        "instruction limit",
        "event queue error",
        "silicon result undefined",
    };
    size_t index;
    for (index = 0u; index < sizeof(names) / sizeof(names[0]); index++) {
        expect(state, strcmp(dspic33_stop_reason_name((Dspic33StopReason)index), names[index]) == 0,
               "stop reason name");
    }
    expect(state, strcmp(dspic33_stop_reason_name((Dspic33StopReason)UINT32_MAX), "unknown") == 0,
           "unknown stop reason name");
}

static void external_event_classification_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_uart_receive(cpu, 0u, 0x5au, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_UART),
           "UART input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_spi_receive(cpu, 0u, 0x1234u, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_SPI),
           "SPI input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_i2c_slave_start(cpu, 0u, 0x42u, false, false, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_I2C),
           "I2C input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x5au, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_PMP),
           "PMP input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_input_capture_input(cpu, 0u, true, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_INPUT_CAPTURE),
           "input capture event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_output_compare_fault(cpu, 0u, true, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_OUTPUT_COMPARE_FAULT),
           "output compare fault classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 1u, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_COMPARATOR),
           "comparator input classified external");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_rtcc_clock(cpu, 1u, 1u) && single_external_event(cpu, DSPIC33_EVENT_RTCC),
           "RTCC clock event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_qei_input(cpu, 0u, DSPIC33_QEI_PHASE_A, true, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_QEI),
           "QEI input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_dci_clock(cpu, 0x1234u, true, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_DCI),
           "DCI input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_timer_pulse(cpu, 0u, 1u, 1u) && single_external_event(cpu, DSPIC33_EVENT_TIMER),
           "timer input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_adc_trigger(cpu, 0u, 1u, 1u) && single_external_event(cpu, DSPIC33_EVENT_ADC),
           "ADC trigger event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_pwm_fault(cpu, 0u, true, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_PWM_FAULT),
           "PWM input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_can_invalid(cpu, 0u, 1u) && single_external_event(cpu, DSPIC33_EVENT_CAN),
           "CAN input event classified external");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ATTACH, 1u, 1u) &&
               single_external_event(cpu, DSPIC33_EVENT_USB),
           "USB input event classified external");
}

static void delayed_interrupt_cases(TestState* state, Dspic33* cpu) {
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

static void relative_and_zero_delay_cases(TestState* state, Dspic33* cpu) {
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

static void ordering_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0x1234u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0xabcdu, 1u),
           "schedule equal-cycle events");
    expect(state, dspic33_device_advance(cpu, 1u), "dispatch equal-cycle events");
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0xabcdu, "equal-cycle sequence order");

    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0x1111u, 5u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_SPI, 0u, 0x2222u, 2u),
           "schedule reverse-cycle events");
    expect(state, dspic33_device_advance(cpu, 2u), "dispatch earlier event");
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0x2222u, "earlier event ordered first");
    expect(state, cpu->events.count == 1u, "later event retained");
    expect(state, dspic33_device_advance(cpu, 3u), "dispatch later event");
    expect(state, dspic33_read_word(cpu, 0x0248u) == 0x1111u, "later event ordered second");
}

static void growth_and_overflow_cases(TestState* state, Dspic33* cpu) {
    bool scheduled = true;
    uint16_t irq;
    dspic33_reset(cpu, 0u);
    for (irq = 0u; irq < 96u; irq++) {
        scheduled = scheduled && dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, irq, 0u, 1u);
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
    expect(state, cpu->events.items[0].cycle == UINT64_MAX, "maximum event cycle retained");
    expect(state, dspic33_device_advance(cpu, 1u), "dispatch maximum event cycle");
    expect(state, interrupt_flag(cpu, 0u), "maximum event cycle dispatched");
}

static void paused_device_clock_cases(TestState* state, Dspic33* cpu) {
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

static void split_clock_domain_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 8u, 0u, 8u) &&
               dspic33_device_advance_instruction(cpu, 1u, 8u) && cpu->cycles == 1u &&
               cpu->device_cycles == 8u && interrupt_flag(cpu, 8u),
           "split advance applies independent CPU and device deltas");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, 3u) && cpu->cycles == 3u && cpu->device_cycles == 3u,
           "public advance retains equal clock domains");

    dspic33_reset(cpu, 0u);
    cpu->cycles = UINT64_MAX;
    expect(state,
           !dspic33_device_advance_instruction(cpu, 1u, 8u) && cpu->cycles == UINT64_MAX &&
               cpu->device_cycles == 0u,
           "split advance rejects CPU overflow atomically");

    dspic33_reset(cpu, 0u);
    cpu->device_cycles = UINT64_MAX - 4u;
    expect(state,
           !dspic33_device_advance_instruction(cpu, 1u, 8u) && cpu->cycles == 0u &&
               cpu->device_cycles == UINT64_MAX - 4u,
           "split advance rejects device overflow atomically");
}

static void copy_and_reset_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    dspic33_reset(cpu, 0u);
    dspic33_reset(copy, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 4u, 0u, 7u),
           "schedule copied event");
    expect(state, dspic33_copy(copy, cpu), "copy event queue");
    expect(state, copy->events.count == 1u, "copied event count");
    expect(state, copy->events.items != cpu->events.items, "copied event storage independent");
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

static void warm_reset_external_event_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    uint64_t sequence;
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 9u, 0u, 5u) &&
               dspic33_timer_gate(cpu, 0u, true, 7u),
           "schedule internal and external reset events");
    expect(state, external_event_count(cpu) == 1u, "external reset event classified");
    expect(state, dspic33_device_advance(cpu, 2u), "advance before warm reset");
    sequence = cpu->events.sequence;
    dspic33_mclr_reset(cpu);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].external &&
               cpu->events.items[0].type == DSPIC33_EVENT_TIMER_GATE &&
               cpu->events.items[0].cycle == 7u,
           "warm reset retains only external event");
    expect(state, cpu->events.sequence == sequence, "warm reset retains event sequence epoch");
    expect(state, dspic33_copy(copy, cpu) && copy->events.items[0].external,
           "copy retains external event classification");
    expect(state, dspic33_device_advance(cpu, 4u), "advance retained event early");
    expect(state, (cpu->io.timer_gate & 1u) == 0u, "retained external event not early");
    expect(state, !interrupt_flag(cpu, 9u), "warm reset cancels internal event");
    expect(state, dspic33_device_advance(cpu, 1u), "dispatch retained external event");
    expect(state, (cpu->io.timer_gate & 1u) != 0u, "retained external event dispatched");
}

static void warm_reset_external_payload_cases(TestState* state, Dspic33* cpu) {
    Dspic33CanFrame frame = {0u};
    frame.identifier = 0x123u;
    frame.length = 1u;
    frame.data[0] = 0x5au;
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_can_receive(cpu, 0u, &frame, 8u) &&
               dspic33_usb_bus(cpu, DSPIC33_USB_BUS_ATTACH, 1u, 9u) &&
               dspic33_pmp_respond(cpu, 0x1234u, 10u) &&
               dspic33_i2c_respond(cpu, 0u, 0x56u, true, 11u),
           "schedule external payload queues");
    expect(state,
           cpu->io.can_rx[0].count == 1u && active_usb_pending_count(cpu) == 1u &&
               cpu->io.pmp.input.count == 1u && cpu->io.i2c_response[0].count == 1u,
           "external payload queues populated");
    expect(state, external_event_count(cpu) == 2u, "external payload events classified");
    dspic33_mclr_reset(cpu);
    expect(state,
           cpu->io.can_rx[0].count == 1u && active_usb_pending_count(cpu) == 1u &&
               cpu->io.pmp.input.count == 1u && cpu->io.i2c_response[0].count == 1u,
           "warm reset retains external payload queues");
    expect(state, cpu->events.count == 2u && external_event_count(cpu) == 2u,
           "warm reset retains external payload events");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->events.count == 0u && cpu->io.can_rx[0].count == 0u &&
               active_usb_pending_count(cpu) == 0u && cpu->io.pmp.input.count == 0u &&
               cpu->io.i2c_response[0].count == 0u,
           "explicit processor reset clears external environment queue");
    cpu->device_cycles = UINT64_MAX;
    expect(state, !dspic33_can_receive(cpu, 0u, &frame, 1u) && cpu->io.can_rx[0].count == 0u,
           "failed external CAN scheduling rolls back payload queue");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    Dspic33 copy;
    bool cpu_initialized = dspic33_initialize(&cpu);
    bool copy_initialized = dspic33_initialize(&copy);
    expect(&state, cpu_initialized, "initialize event processor");
    expect(&state, copy_initialized, "initialize event copy");
    stop_reason_name_cases(&state);
    if (cpu_initialized && copy_initialized) {
        delayed_interrupt_cases(&state, &cpu);
        relative_and_zero_delay_cases(&state, &cpu);
        ordering_cases(&state, &cpu);
        growth_and_overflow_cases(&state, &cpu);
        paused_device_clock_cases(&state, &cpu);
        split_clock_domain_cases(&state, &cpu);
        copy_and_reset_cases(&state, &cpu, &copy);
        external_event_classification_cases(&state, &cpu);
        warm_reset_external_event_cases(&state, &cpu, &copy);
        warm_reset_external_payload_cases(&state, &cpu);
    }
    if (copy_initialized) {
        dspic33_release(&copy);
    }
    if (cpu_initialized) {
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
