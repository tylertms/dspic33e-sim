#include "allocation_failure.h"
#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum { DMA_TEST_BASE = 0x0b00u, DMA_TEST_CONTROL_ENABLE = 0x8000u, DMA_TEST_FORCE = 0x00010000u };

void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_device_internal_run_dma(Dspic33* cpu, uint16_t source, uint32_t event_value);
bool dspic33_device_internal_can_queue_push(Dspic33CanQueue* queue, const Dspic33CanFrame* frame);
bool dspic33_device_internal_comparator_pin_channel(const Dspic33* cpu, uint8_t pin,
                                                    uint8_t* comparator);
void dspic33_device_internal_dma_advance_generation(Dspic33* cpu, uint8_t channel);
void dspic33_device_internal_dma_update_power_state(Dspic33* cpu);
bool dspic33_device_service_interrupt(Dspic33* cpu);

#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
static void fill_event_queue(TestState* state, Dspic33* cpu) {
    while (cpu->events.capacity == 0u || cpu->events.count < cpu->events.capacity) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
               "fill DMA allocation failure event queue");
    }
}

static void active_channel_failure_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.dma_active = 2u;
    cpu->io.dma_peripheral_pending = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_dma(cpu, 0u, 0u);
    test_reject_reallocation(false);
    expect(state, (cpu->io.dma_peripheral_pending & 1u) == 0u,
           "blocked peripheral DMA clears failed pending request");

    dspic33_reset(cpu, 0u);
    cpu->io.dma_active = 2u;
    cpu->io.dma_forced_pending = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_dma(cpu, 0u, DMA_TEST_FORCE);
    test_reject_reallocation(false);
    expect(state, (cpu->io.dma_forced_pending & 1u) == 0u,
           "blocked forced DMA clears failed pending request");
}

static void completion_failure_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DMA_TEST_BASE, DMA_TEST_CONTROL_ENABLE);
    dspic33_device_internal_raw_write_word(cpu, DMA_TEST_BASE + 4u, 0x4000u);
    dspic33_device_internal_raw_write_word(cpu, DMA_TEST_BASE + 6u, 0u);
    dspic33_device_internal_raw_write_word(cpu, DMA_TEST_BASE + 0x0cu, 0x0290u);
    cpu->io.dma_forced_pending = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_dma(cpu, 0u, DMA_TEST_FORCE);
    test_reject_reallocation(false);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->io.dma_active == 0u &&
               (cpu->io.dma_forced_pending & 1u) == 0u,
           "DMA completion allocation failure clears active forced transfer");

    dspic33_reset(cpu, 0u);
    dspic33_device_internal_raw_write_word(cpu, DMA_TEST_BASE, DMA_TEST_CONTROL_ENABLE);
    cpu->io.dma_address[0] = 0x4000u;
    cpu->io.cpu_bus_cycle = cpu->cycles;
    cpu->io.dma_peripheral_pending = 1u;
    fill_event_queue(state, cpu);
    test_reject_reallocation(true);
    dspic33_device_internal_run_dma(cpu, 0u, 0u);
    test_reject_reallocation(false);
    expect(state,
           (cpu->io.dma_peripheral_pending & 1u) == 0u && (cpu->io.dma_arbiter_waiting & 1u) == 0u,
           "busy-bus DMA allocation failure clears arbiter state");
}
#endif

static void event_guard_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->io.dma_forced_pending = 1u;
    dspic33_device_internal_run_dma(cpu, 0u, DMA_TEST_FORCE);
    expect(state, (cpu->io.dma_forced_pending & 1u) == 0u,
           "disabled forced DMA request clears pending state");

    cpu->io.dma_peripheral_pending = 1u;
    cpu->io.dma_arbiter_waiting = 1u;
    dspic33_device_internal_run_dma(cpu, 0u, 0u);
    expect(state,
           (cpu->io.dma_peripheral_pending & 1u) == 0u && (cpu->io.dma_arbiter_waiting & 1u) == 0u,
           "disabled peripheral DMA request clears pending state");

    dspic33_device_internal_run_dma(cpu, DSPIC33_DMA_COUNT, 0u);
    expect(state, cpu->io.dma_active == 0u, "inactive DMA completion event is ignored");

    dspic33_device_internal_raw_write_word(cpu, DMA_TEST_BASE, DMA_TEST_CONTROL_ENABLE);
    cpu->io.dma_address[0] = UINT32_MAX;
    cpu->io.dma_forced_pending = 1u;
    dspic33_device_internal_run_dma(cpu, 0u, DMA_TEST_FORCE);
    expect(state,
           cpu->stop_reason != DSPIC33_EVENT_QUEUE_ERROR && (cpu->io.dma_forced_pending & 1u) == 0u,
           "invalid forced DMA address raises trap and clears pending state");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 0u),
           "schedule unrelated DMA generation event");
    cpu->io.dma_generation[0] = 0x7fffu;
    dspic33_device_internal_dma_advance_generation(cpu, 0u);
    expect(state, cpu->events.count == 1u, "DMA generation wrap preserves unrelated event");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_DMA, 0u, 0u, 0u),
           "schedule paused DMA overflow event");
    cpu->events.items[0].paused = true;
    cpu->events.items[0].paused_remaining = UINT64_MAX;
    cpu->device_cycles = 1u;
    dspic33_device_internal_dma_update_power_state(cpu);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "DMA resume overflow stops execution");

    uint8_t comparator = 0u;
    expect(state, !dspic33_device_internal_comparator_pin_channel(cpu, UINT8_MAX, &comparator),
           "unknown comparator pin is rejected");
    Dspic33CanQueue queue = {0};
    Dspic33CanFrame frame = {0};
    queue.count = 64u;
    expect(state, !dspic33_device_internal_can_queue_push(&queue, &frame),
           "full CAN queue rejects frame");
    dspic33_raise_interrupt(cpu, UINT16_MAX);
    cpu->async_events_enabled = false;
    expect(state, !dspic33_device_service_interrupt(cpu),
           "disabled asynchronous events suppress interrupt service");
}

void dspic33_dma_test_boundary_cases(TestState* state, Dspic33* cpu) {
    event_guard_cases(state, cpu);
#ifdef DSPIC33_TEST_ALLOCATION_FAILURE
    active_channel_failure_cases(state, cpu);
    completion_failure_cases(state, cpu);
#endif
}
