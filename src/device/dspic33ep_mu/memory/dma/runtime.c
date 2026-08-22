#include "device/dspic33ep_mu/internal.h"

bool dspic33_device_internal_comparator_pin_channel(const Dspic33* cpu, uint8_t pin,
                                                    uint8_t* comparator) {
    size_t index;
    for (index = 0u;
         index < sizeof(dspic33_device_pps_outputs) / sizeof(dspic33_device_pps_outputs[0]);
         index++) {
        if (dspic33_device_pps_outputs[index].pin == pin) {
            uint8_t function = (uint8_t)((dspic33_device_internal_raw_word(
                                              cpu, dspic33_device_pps_outputs[index].address) >>
                                          dspic33_device_pps_outputs[index].shift) &
                                         0x003fu);
            if (function >= COMPARATOR_PPS_FUNCTION &&
                function < COMPARATOR_PPS_FUNCTION + DSPIC33_COMPARATOR_COUNT) {
                *comparator = (uint8_t)(function - COMPARATOR_PPS_FUNCTION);
                return true;
            }
            return false;
        }
    }
    return false;
}

bool dspic33_device_internal_can_queue_push(Dspic33CanQueue* queue, const Dspic33CanFrame* frame) {
    uint8_t index;
    if (queue->count == 64u) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) % 64u);
    queue->frames[index] = *frame;
    queue->count++;
    return true;
}

bool dspic33_device_internal_can_queue_pop(Dspic33CanQueue* queue, Dspic33CanFrame* frame) {
    if (queue->count == 0u) {
        return false;
    }
    *frame = queue->frames[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % 64u);
    queue->count--;
    return true;
}

void dspic33_device_internal_can_queue_discard_last(Dspic33CanQueue* queue) {
    if (queue->count != 0u) {
        queue->count--;
    }
}

static bool event_less(const Dspic33Event* left, const Dspic33Event* right) {
    bool left_dma_completion;
    bool right_dma_completion;
    if (left->paused != right->paused) {
        return !left->paused;
    }
    if (left->cycle != right->cycle) {
        return left->cycle < right->cycle;
    }
    if (left->type == DSPIC33_EVENT_DMA && right->type == DSPIC33_EVENT_DMA &&
        left->source != right->source) {
        left_dma_completion = left->source >= DSPIC33_DMA_COUNT;
        right_dma_completion = right->source >= DSPIC33_DMA_COUNT;
        if (left_dma_completion != right_dma_completion) {
            return left_dma_completion;
        }
        return left->source % DSPIC33_DMA_COUNT < right->source % DSPIC33_DMA_COUNT;
    }
    return left->sequence < right->sequence;
}

static bool event_reserve(Dspic33EventQueue* queue) {
    Dspic33Event* items;
    size_t capacity;
    if (queue->count < queue->capacity) {
        return true;
    }
    capacity = queue->capacity == 0u ? 64u : queue->capacity * 2u;
    items = realloc(queue->items, capacity * sizeof(*items));
    if (items == NULL) {
        return false;
    }
    queue->items = items;
    queue->capacity = capacity;
    return true;
}

static bool schedule_event(Dspic33* cpu, Dspic33EventType type, uint16_t source, uint32_t value,
                           uint64_t delay, bool external) {
    Dspic33Event event;
    size_t index;
    size_t parent;
    if (delay > UINT64_MAX - cpu->device_cycles) {
        return false;
    }
    if (!event_reserve(&cpu->events)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    event.cycle = cpu->device_cycles + delay;
    event.sequence = cpu->events.sequence++;
    event.paused_remaining = 0u;
    event.value = value;
    event.source = source;
    event.type = type;
    event.paused = false;
    event.external = external;
    index = cpu->events.count++;
    while (index != 0u) {
        parent = (index - 1u) / 2u;
        if (!event_less(&event, &cpu->events.items[parent])) {
            break;
        }
        cpu->events.items[index] = cpu->events.items[parent];
        index = parent;
    }
    cpu->events.items[index] = event;
    return true;
}

bool dspic33_schedule(Dspic33* cpu, Dspic33EventType type, uint16_t source, uint32_t value,
                      uint64_t delay) {
    return schedule_event(cpu, type, source, value, delay, false);
}

bool dspic33_schedule_external(Dspic33* cpu, Dspic33EventType type, uint16_t source, uint32_t value,
                               uint64_t delay) {
    return schedule_event(cpu, type, source, value, delay, true);
}

void dspic33_reorder_events(Dspic33* cpu) {
    size_t parent;
    if (cpu->events.count < 2u) {
        return;
    }
    for (parent = cpu->events.count / 2u; parent != 0u; parent--) {
        Dspic33Event event = cpu->events.items[parent - 1u];
        size_t index = parent - 1u;
        size_t child = index * 2u + 1u;
        while (child < cpu->events.count) {
            if (child + 1u < cpu->events.count &&
                event_less(&cpu->events.items[child + 1u], &cpu->events.items[child])) {
                child++;
            }
            if (!event_less(&cpu->events.items[child], &event)) {
                break;
            }
            cpu->events.items[index] = cpu->events.items[child];
            index = child;
            child = index * 2u + 1u;
        }
        cpu->events.items[index] = event;
    }
}

Dspic33Event dspic33_device_internal_event_pop(Dspic33EventQueue* queue) {
    Dspic33Event result = queue->items[0];
    Dspic33Event tail = queue->items[--queue->count];
    size_t index = 0u;
    while (index * 2u + 1u < queue->count) {
        size_t child = index * 2u + 1u;
        if (child + 1u < queue->count &&
            event_less(&queue->items[child + 1u], &queue->items[child])) {
            child++;
        }
        if (!event_less(&queue->items[child], &tail)) {
            break;
        }
        queue->items[index] = queue->items[child];
        index = child;
    }
    if (queue->count != 0u) {
        queue->items[index] = tail;
    }
    return result;
}

static void update_nested_do_interrupt_request(Dspic33* cpu, uint16_t irq);

void dspic33_raise_interrupt(Dspic33* cpu, uint16_t irq) {
    uint16_t address;
    uint16_t mask;
    uint16_t value;
    if (irq >= DSPIC33_IRQ_COUNT) {
        return;
    }
    address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    mask = (uint16_t)(1u << (irq % 16u));
    value = dspic33_device_internal_raw_word(cpu, address);
    dspic33_device_internal_raw_write_word(cpu, address, (uint16_t)(value | mask));
    if ((value & mask) == 0u) {
        update_nested_do_interrupt_request(cpu, irq);
    }
}

void dspic33_device_internal_raise_external_interrupt(Dspic33* cpu, uint8_t channel) {
    if (channel == 0u) {
        uint8_t module;
        dspic33_dma_request(cpu, 0u, 0u, 0u);
        for (module = 0u; module < DSPIC33_ADC_COUNT; module++) {
            dspic33_adc_trigger(cpu, module, 1u, 0u);
        }
    } else if (channel == 1u) {
        dspic33_device_internal_output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_INT1);
    } else if (channel == 2u) {
        dspic33_device_internal_output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_INT2);
    }
    dspic33_raise_interrupt(cpu, dspic33_device_external_interrupt_irqs[channel]);
}

void dspic33_device_internal_raise_scheduled_interrupt(Dspic33* cpu, uint16_t irq) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT; channel++) {
        if (dspic33_device_external_interrupt_irqs[channel] == irq) {
            dspic33_device_internal_raise_external_interrupt(cpu, channel);
            return;
        }
    }
    dspic33_raise_interrupt(cpu, irq);
}

static uint8_t interrupt_priority(const Dspic33* cpu, uint16_t irq) {
    uint16_t value = dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0840u + (irq / 4u) * 2u));
    return (uint8_t)((value >> ((irq % 4u) * 4u)) & 0x07u);
}

bool dspic33_device_internal_interrupt_enabled(const Dspic33* cpu, uint16_t irq) {
    uint16_t mask = (uint16_t)(1u << (irq % 16u));
    uint16_t offset = (uint16_t)((irq / 16u) * 2u);
    return (dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0800u + offset)) & mask) != 0u &&
           (dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0820u + offset)) & mask) != 0u;
}

static bool interrupt_deferred(const Dspic33* cpu, uint16_t irq) {
    return (cpu->interrupt_deferred[irq / 16u] & (uint16_t)(1u << (irq % 16u))) != 0u;
}

void dspic33_device_latch_interrupt(Dspic33* cpu, uint8_t vector, uint8_t priority) {
    dspic33_device_internal_raw_write_word(cpu, 0x08c8u,
                                           (uint16_t)(((uint16_t)priority << 8u) | vector));
}

void dspic33_device_latch_math_error(Dspic33* cpu, uint16_t cause) {
    dspic33_device_internal_raw_write_word(
        cpu, 0x08c0u, (uint16_t)(dspic33_device_internal_raw_word(cpu, 0x08c0u) | cause | 0x0010u));
    dspic33_set_math_error_source(cpu, true);
}

static bool select_interrupt(const Dspic33* cpu, uint16_t* selected_irq,
                             uint8_t* selected_priority) {
    uint8_t current = (uint8_t)((cpu->sr >> 5u) & 0x07u);
    uint8_t best_priority = current;
    uint16_t best_irq = DSPIC33_IRQ_COUNT;
    uint16_t irq;
    uint16_t group;
    if (!cpu->async_events_enabled ||
        ((dspic33_device_internal_raw_word(cpu, 0x08c2u) & 0x8000u) == 0u &&
         cpu->gie_disable_deferred == 0u) ||
        (cpu->corcon & 0x0008u) != 0u) {
        return false;
    }
    for (group = 0u; group < (DSPIC33_IRQ_COUNT + 15u) / 16u; group++) {
        uint16_t offset = (uint16_t)(group * 2u);
        if ((dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0800u + offset)) &
             dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0820u + offset))) != 0u) {
            break;
        }
    }
    if (group == (DSPIC33_IRQ_COUNT + 15u) / 16u) {
        return false;
    }
    for (irq = 0u; irq < DSPIC33_IRQ_COUNT; irq++) {
        uint8_t priority;
        if (!dspic33_device_internal_interrupt_enabled(cpu, irq) || interrupt_deferred(cpu, irq)) {
            continue;
        }
        priority = interrupt_priority(cpu, irq);
        if (cpu->disicnt != 0u && priority < 7u) {
            continue;
        }
        if (priority > best_priority) {
            best_priority = priority;
            best_irq = irq;
        }
    }
    if (best_irq == DSPIC33_IRQ_COUNT) {
        return false;
    }
    *selected_irq = best_irq;
    *selected_priority = best_priority;
    return true;
}

static void recover_from_doze(Dspic33* cpu) {
    if ((dspic33_device_internal_raw_word(cpu, MAIN_CLOCK_DIVISOR) & 0x8000u) != 0u) {
        dspic33_device_internal_raw_write_word(
            cpu, MAIN_CLOCK_DIVISOR,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, MAIN_CLOCK_DIVISOR) & ~0x0800u));
    }
}

static void update_nested_do_interrupt_request(Dspic33* cpu, uint16_t irq) {
    uint8_t depth = cpu->nested_do_interrupt_depth;
    uint8_t priority;
    uint16_t selected_irq;
    if ((dspic33_device_internal_raw_word(cpu, 0x08c0u) & 0x8000u) != 0u) {
        cpu->nested_do_interrupt_armed = false;
        return;
    }
    if (!select_interrupt(cpu, &selected_irq, &priority) || selected_irq != irq) {
        return;
    }
    if (cpu->nested_do_interrupt_armed) {
        if (priority > cpu->nested_do_interrupt_priority &&
            cpu->device_cycles - cpu->nested_do_interrupt_cycle ==
                dspic33_device_instruction_cycles(cpu, 4u) &&
            depth != 0u && cpu->do_depth >= depth &&
            cpu->do_end[depth - 1u] == cpu->nested_do_interrupt_end) {
            cpu->nested_do_extra_decrement_depth = depth;
            cpu->nested_do_extra_decrement_end = cpu->nested_do_interrupt_end;
        }
        cpu->nested_do_interrupt_armed = false;
        return;
    }
    if (cpu->interrupt_entry_active) {
        return;
    }
    depth = cpu->do_depth;
    if (depth != 0u) {
        uint32_t end = cpu->do_end[depth - 1u];
        uint32_t previous = (end - 2u) & 0x007ffffeu;
        uint32_t origin = cpu->instruction_advancing ? cpu->current_instruction_pc : cpu->pc;
        if (origin == end || origin == previous) {
            cpu->nested_do_interrupt_cycle = cpu->device_cycles;
            cpu->nested_do_interrupt_end = end;
            cpu->nested_do_interrupt_depth = depth;
            cpu->nested_do_interrupt_priority = priority;
            cpu->nested_do_interrupt_armed = true;
        }
    }
}

static bool service_interrupt(Dspic33* cpu) {
    uint8_t best_priority;
    uint16_t best_irq;
    uint16_t next_priority;
    size_t log_index;
    uint16_t stacked_high;
    uint64_t entry_device_cycles;
    uint64_t first_entry_cycles;
    uint64_t first_entry_device_cycles;
    uint64_t entry_cycles;
    uint32_t origin;
    uint32_t target;
    if (!select_interrupt(cpu, &best_irq, &best_priority)) {
        return false;
    }
    dspic33_cancel_flash_read_sequence(cpu);
    origin = cpu->pc;
    target = dspic33_read_program_word(cpu, origin >= DSPIC33_AUXILIARY_PROGRAM_BASE
                                                ? 0x007ffffau
                                                : 0x0014u + best_irq * 2u) &
             0x007ffffeu;
    if (!dspic33_device_program_range_implemented(cpu, target, 2u)) {
        dspic33_raise_program_vector_error(cpu, origin);
        return true;
    }
    if (!dspic33_codeguard_admit_program_flow(cpu, origin, target)) {
        return true;
    }
    recover_from_doze(cpu);
    entry_cycles = 9u - cpu->interrupt_entry_overlap;
    cpu->interrupt_entry_overlap = 0u;
    entry_device_cycles = dspic33_device_instruction_cycles(cpu, entry_cycles);
    if (entry_cycles > UINT64_MAX - cpu->cycles ||
        (cpu->async_events_enabled && entry_device_cycles > UINT64_MAX - cpu->device_cycles)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return true;
    }
    first_entry_cycles = entry_cycles - 3u;
    first_entry_device_cycles = dspic33_device_instruction_cycles(cpu, first_entry_cycles);
    stacked_high =
        (uint16_t)(((cpu->sr & 0x00ffu) << 8u) | ((cpu->corcon & 0x0008u) != 0u ? 0x0080u : 0u) |
                   ((cpu->pc >> 16u) & 0x007fu));
    next_priority = (dspic33_device_internal_raw_word(cpu, 0x08c0u) & 0x8000u) != 0u
                        ? UINT16_C(0x00e0)
                        : (uint16_t)((uint16_t)best_priority << 5u);
    cpu->interrupt_entry_active = true;
    if (!dspic33_device_advance_instruction(cpu, first_entry_cycles, first_entry_device_cycles)) {
        cpu->interrupt_entry_active = false;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return true;
    }
    if (cpu->reset_occurred || cpu->nvm.reset_pending) {
        cpu->interrupt_entry_active = false;
        return true;
    }
    dspic33_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu, 2u);
    dspic33_write_word(cpu, cpu->w[15],
                       (uint16_t)((cpu->pc & 0xfffeu) | ((cpu->corcon >> 2u) & 1u)));
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    cpu->corcon &= 0xfffbu;
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | next_priority);
    if (!dspic33_device_advance_instruction(cpu, 3u,
                                            entry_device_cycles - first_entry_device_cycles)) {
        cpu->interrupt_entry_active = false;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return true;
    }
    cpu->interrupt_entry_active = false;
    if (cpu->reset_occurred || cpu->nvm.reset_pending) {
        return true;
    }
    dspic33_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu, 2u);
    dspic33_write_word(cpu, cpu->w[15], stacked_high);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    log_index = (size_t)(cpu->interrupt_count % 16u);
    cpu->interrupt_log_irq[log_index] = best_irq;
    cpu->interrupt_log_entry[log_index] = cpu->pc;
    cpu->interrupt_log_return[log_index] = 0u;
    cpu->pc = target;
    cpu->last_interrupt = best_irq;
    cpu->interrupt_count++;
    cpu->interrupt_depth++;
    dspic33_device_latch_interrupt(cpu, (uint8_t)(best_irq + 8u), best_priority);
    cpu->repeat_active = 0u;
    cpu->repeat_pc = 0u;
    cpu->sr &= 0xffefu;
    return true;
}

bool dspic33_device_interrupt_pending(const Dspic33* cpu) {
    uint8_t priority;
    uint16_t irq;
    return select_interrupt(cpu, &irq, &priority);
}

bool dspic33_device_service_interrupt(Dspic33* cpu) { return service_interrupt(cpu); }

bool dspic33_device_wake(Dspic33* cpu) {
    uint16_t irq;
    if (!cpu->async_events_enabled) {
        return false;
    }
    for (irq = 0u; irq < DSPIC33_IRQ_COUNT; irq++) {
        if (dspic33_device_internal_interrupt_enabled(cpu, irq) && !interrupt_deferred(cpu, irq) &&
            interrupt_priority(cpu, irq) != 0u) {
            service_interrupt(cpu);
            if (cpu->reset_occurred || cpu->nvm.reset_pending) {
                return true;
            }
            recover_from_doze(cpu);
            return true;
        }
    }
    return false;
}

void dspic33_device_return_interrupt(Dspic33* cpu) {
    uint16_t high;
    uint16_t low;
    dspic33_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u, 2u);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    high = dspic33_read_word(cpu, cpu->w[15]);
    dspic33_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u, 2u);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    low = dspic33_read_word(cpu, cpu->w[15]);
    cpu->pc = ((uint32_t)(high & 0x007fu) << 16u) | (low & 0xfffeu);
    cpu->last_interrupt_return = cpu->pc;
    if (cpu->interrupt_count != 0u) {
        cpu->interrupt_log_return[(cpu->interrupt_count - 1u) % 16u] = cpu->pc;
    }
    cpu->sr = (uint16_t)((cpu->sr & 0xff00u) | (high >> 8u));
    if ((high & 0x0080u) != 0u) {
        cpu->corcon |= 0x0008u;
    } else {
        cpu->corcon &= 0xfff7u;
    }
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0004u) | ((low & 1u) << 2u));
    cpu->repeat_active = (cpu->sr & 0x0010u) != 0u;
    cpu->repeat_pc = cpu->repeat_active != 0u ? cpu->pc : 0u;
    cpu->repeat_psv_reentry = cpu->repeat_active != 0u;
    if (cpu->interrupt_depth != 0u) {
        cpu->interrupt_depth--;
    }
}

uint16_t dspic33_device_internal_dma_channel_base(uint8_t channel) {
    return (uint16_t)(DMA_CHANNEL_BASE + channel * DMA_CHANNEL_STRIDE);
}

uint16_t dspic33_device_internal_dma_channel_bit(uint8_t channel) {
    return (uint16_t)(1u << channel);
}

static bool dma_channel_event(const Dspic33Event* event, uint8_t channel) {
    return event->type == DSPIC33_EVENT_DMA && event->source % DSPIC33_DMA_COUNT == channel;
}

static void dma_remove_channel_events(Dspic33* cpu, uint8_t channel) {
    size_t source;
    size_t destination = 0u;
    for (source = 0u; source < cpu->events.count; source++) {
        if (!dma_channel_event(&cpu->events.items[source], channel)) {
            cpu->events.items[destination++] = cpu->events.items[source];
        }
    }
    cpu->events.count = destination;
    dspic33_reorder_events(cpu);
}

void dspic33_device_internal_dma_advance_generation(Dspic33* cpu, uint8_t channel) {
    if ((cpu->io.dma_generation[channel] & DMA_EVENT_GENERATION_MASK) ==
        DMA_EVENT_GENERATION_MASK) {
        dma_remove_channel_events(cpu, channel);
    }
    cpu->io.dma_generation[channel]++;
}

void dspic33_device_internal_dma_update_power_state(Dspic33* cpu) {
    size_t index;
    bool available = cpu->power_state != DSPIC33_POWER_SLEEP;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (event->type != DSPIC33_EVENT_DMA) {
            continue;
        }
        if (!available && !event->paused) {
            event->paused_remaining = event->cycle - cpu->device_cycles;
            event->paused = true;
            changed = true;
        } else if (available && event->paused) {
            if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                continue;
            }
            event->cycle = cpu->device_cycles + event->paused_remaining;
            event->paused_remaining = 0u;
            event->paused = false;
            changed = true;
        }
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static uint32_t dma_start_address(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.dma_bank & dspic33_device_internal_dma_channel_bit(channel)) != 0u
               ? cpu->io.dma_start_b[channel]
               : cpu->io.dma_start_a[channel];
}

static uint32_t dma_transfer_address(const Dspic33* cpu, uint8_t channel, uint16_t control,
                                     uint16_t indirect_address) {
    uint32_t start = dma_start_address(cpu, channel);
    uint16_t mode = control & DMA_CON_AMODE_MASK;
    if (mode == DMA_CON_AMODE_PERIPHERAL) {
        return start | indirect_address;
    }
    return cpu->io.dma_address[channel];
}

static bool dma_memory_address_valid(uint32_t address, uint8_t width) {
    return dspic33_data_range_valid(address, width);
}

static bool dma_write_cycle_matches(const Dspic33* cpu) {
    return cpu->io.cpu_write_valid &&
           (cpu->io.cpu_write_cycle == cpu->cycles ||
            (cpu->io.cpu_write_cycle != UINT64_MAX && cpu->io.cpu_write_cycle + 1u == cpu->cycles));
}

static bool dma_cpu_bus_busy(const Dspic33* cpu) {
    return cpu->io.cpu_bus_cycle == cpu->cycles ||
           (cpu->io.cpu_bus_cycle != UINT64_MAX && cpu->io.cpu_bus_cycle + 1u == cpu->cycles);
}

static bool dma_cpu_wrote_byte(const Dspic33* cpu, uint32_t address) {
    return dma_write_cycle_matches(cpu) && address >= cpu->io.cpu_write_address &&
           address < cpu->io.cpu_write_address + cpu->io.cpu_write_width;
}

static uint8_t dma_read_memory_byte(const Dspic33* cpu, uint32_t address) {
    if (dma_cpu_wrote_byte(cpu, address)) {
        uint8_t shift = (uint8_t)((address - cpu->io.cpu_write_address) * 8u);
        return (uint8_t)(cpu->io.cpu_write_previous >> shift);
    }
    return cpu->data[address];
}

static uint16_t dma_read_memory(const Dspic33* cpu, uint32_t address, uint8_t width) {
    if (!dma_memory_address_valid(address, width)) {
        return 0u;
    }
    if (width == 1u) {
        return dma_read_memory_byte(cpu, address);
    }
    return (uint16_t)(dma_read_memory_byte(cpu, address) |
                      ((uint16_t)dma_read_memory_byte(cpu, address + 1u) << 8u));
}

static void dma_write_memory(Dspic33* cpu, uint32_t address, uint8_t width, uint16_t value) {
    if (!dma_memory_address_valid(address, width)) {
        return;
    }
    if (!dma_cpu_wrote_byte(cpu, address)) {
        cpu->data[address] = (uint8_t)value;
    }
    if (width == 2u && !dma_cpu_wrote_byte(cpu, address + 1u)) {
        cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    }
}

static void dma_record_transfer(Dspic33* cpu, uint8_t channel, uint16_t control, uint32_t address) {
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    uint16_t base = dspic33_device_internal_dma_channel_base(channel);
    uint16_t start_offset = (cpu->io.dma_bank & bit) != 0u ? 8u : 4u;
    uint16_t ping_pong = dspic33_device_internal_raw_word(cpu, DMA_PPS);
    if ((cpu->io.dma_bank & bit) != 0u) {
        ping_pong |= bit;
    } else {
        ping_pong &= (uint16_t)~bit;
    }
    dspic33_device_internal_raw_write_word(cpu, DMA_PPS, (uint16_t)(ping_pong & DMA_CHANNEL_MASK));
    dspic33_device_internal_raw_write_word(cpu, DMA_LCA, channel);
    dspic33_device_internal_raw_write_word(cpu, DMA_SADRL, (uint16_t)address);
    dspic33_device_internal_raw_write_word(cpu, DMA_SADRH, (uint16_t)((address >> 16u) & 0x00ffu));
    if ((control & DMA_CON_AMODE_MASK) == DMA_CON_AMODE_PERIPHERAL) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + start_offset),
                                               (uint16_t)address);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + start_offset + 2u),
                                               (uint16_t)((address >> 16u) & 0x00ffu));
    }
}

static bool dma_write_collision(const Dspic33* cpu, uint16_t pad, uint8_t width) {
    uint32_t previous_end;
    uint32_t dma_end;
    if (!dma_write_cycle_matches(cpu)) {
        return false;
    }
    previous_end = cpu->io.cpu_write_address + cpu->io.cpu_write_width;
    dma_end = (uint32_t)pad + width;
    return cpu->io.cpu_write_address < dma_end && pad < previous_end;
}

static bool dma_can_write_pad(uint16_t pad) { return pad == 0x0442u || pad == 0x0542u; }

static void dma_peripheral_write_collision(Dspic33* cpu, uint8_t channel) {
    uint16_t status = dspic33_device_internal_raw_word(cpu, DMA_PWC);
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    if ((status & bit) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, DMA_PWC, (uint16_t)(status | bit));
        dspic33_raise_dma_collision_trap(cpu);
    }
}

static uint16_t uart_dma_error_bits(const Dspic33* cpu, uint16_t address) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        if (address == dspic33_device_uart_bases[channel] + 6u) {
            uint16_t status = dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_uart_bases[channel] + 2u));
            return (uint16_t)(((status & UART_STATUS_PARITY_ERROR) != 0u ? 0x0800u : 0u) |
                              ((status & UART_STATUS_FRAMING_ERROR) != 0u ? 0x0400u : 0u));
        }
    }
    return 0u;
}

static bool dma_pad_in_set(uint16_t pad, const uint16_t* pads, size_t count) {
    size_t index;
    for (index = 0u; index < count; index++) {
        if (pad == pads[index]) {
            return true;
        }
    }
    return false;
}

static bool dma_pad_readable(uint16_t pad) {
    static const uint16_t pads[] = {0x0144u, 0x014cu, 0x0154u, 0x015cu, 0x0226u, 0x0236u,
                                    0x0248u, 0x0256u, 0x0268u, 0x0290u, 0x02a8u, 0x02b6u,
                                    0x02c8u, 0x0300u, 0x0340u, 0x0440u, 0x0540u, 0x0608u};
    return dma_pad_in_set(pad, pads, sizeof(pads) / sizeof(pads[0]));
}

static bool dma_pad_writable(uint16_t pad) {
    static const uint16_t pads[] = {0x0224u, 0x0234u, 0x0248u, 0x0254u, 0x0268u, 0x0298u, 0x02a8u,
                                    0x02b4u, 0x02c8u, 0x0442u, 0x0542u, 0x0608u, 0x0904u, 0x0906u,
                                    0x090eu, 0x0910u, 0x0918u, 0x091au, 0x0922u, 0x0924u};
    return dma_pad_in_set(pad, pads, sizeof(pads) / sizeof(pads[0]));
}

bool dspic33_device_dma_pad_valid(uint16_t pad, bool write) {
    return write ? dma_pad_writable(pad) : dma_pad_readable(pad);
}

static void dma_disable_channel(Dspic33* cpu, uint8_t channel, uint16_t control) {
    uint16_t base = dspic33_device_internal_dma_channel_base(channel);
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    dspic33_device_internal_raw_write_word(cpu, base, (uint16_t)(control & ~DMA_CON_CHEN));
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(base + 2u),
        (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) & ~DMA_REQ_FORCE));
    cpu->io.dma_enabled &= (uint16_t)~bit;
    cpu->io.dma_forced_pending &= (uint16_t)~bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    cpu->io.dma_active &= (uint16_t)~bit;
    cpu->io.dma_arbiter_waiting &= (uint16_t)~bit;
    dspic33_device_internal_dma_advance_generation(cpu, channel);
}

static void dma_complete_block(Dspic33* cpu, uint8_t channel, uint16_t control) {
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    uint16_t mode = control & DMA_CON_MODE_MASK;
    bool secondary = (cpu->io.dma_bank & bit) != 0u;
    if ((control & DMA_CON_HALF) == 0u) {
        dspic33_raise_interrupt(cpu, dspic33_device_dma_irqs[channel]);
    }
    cpu->io.dma_index[channel] = 0u;
    cpu->io.dma_half_raised &= (uint16_t)~bit;
    if ((mode & DMA_CON_MODE_ONE_SHOT) != 0u &&
        ((mode & DMA_CON_MODE_PING_PONG) == 0u || secondary)) {
        dma_disable_channel(cpu, channel, control);
        return;
    }
    if ((mode & DMA_CON_MODE_PING_PONG) != 0u) {
        cpu->io.dma_bank ^= bit;
    }
    cpu->io.dma_address[channel] = dma_start_address(cpu, channel);
}

static void complete_dma_transfer(Dspic33* cpu, uint8_t channel, uint32_t event_value) {
    uint16_t base = dspic33_device_internal_dma_channel_base(channel);
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    uint16_t control = dspic33_device_internal_raw_word(cpu, base);
    uint16_t count = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 0x0eu));
    uint16_t index = cpu->io.dma_index[channel];
    uint16_t transferred = (uint16_t)(index + 1u);
    uint16_t half = (uint16_t)(((uint32_t)count + 2u) / 2u);
    uint8_t width = (control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    bool forced = (event_value & DMA_EVENT_FORCE) != 0u;
    if ((cpu->io.dma_active & bit) == 0u) {
        return;
    }
    if ((control & DMA_CON_AMODE_MASK) == 0u) {
        cpu->io.dma_address[channel] += width;
    }
    if ((control & DMA_CON_HALF) != 0u && transferred == half &&
        (cpu->io.dma_half_raised & bit) == 0u) {
        cpu->io.dma_half_raised |= bit;
        dspic33_raise_interrupt(cpu, dspic33_device_dma_irqs[channel]);
    }
    if (index >= count) {
        dma_complete_block(cpu, channel, control);
    } else {
        cpu->io.dma_index[channel]++;
    }
    cpu->io.dma_active &= (uint16_t)~bit;
    if (forced) {
        cpu->io.dma_forced_pending &= (uint16_t)~bit;
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
                       ~DMA_REQ_FORCE));
    }
}

static bool dma_target_is_dual_port_ram(const Dspic33* cpu, uint8_t channel,
                                        uint16_t indirect_address);

void dspic33_device_internal_run_dma(Dspic33* cpu, uint16_t source, uint32_t event_value) {
    uint16_t base;
    uint16_t bit;
    uint16_t control;
    uint16_t pad;
    uint16_t value;
    uint16_t uart_errors;
    uint16_t generation;
    uint32_t address;
    uint8_t width;
    uint8_t channel;
    bool forced;
    bool completion;
    if (source >= DSPIC33_DMA_COUNT * 2u) {
        return;
    }
    completion = source >= DSPIC33_DMA_COUNT;
    channel = (uint8_t)(source % DSPIC33_DMA_COUNT);
    bit = dspic33_device_internal_dma_channel_bit(channel);
    forced = (event_value & DMA_EVENT_FORCE) != 0u;
    generation =
        (uint16_t)((event_value >> DMA_EVENT_GENERATION_SHIFT) & DMA_EVENT_GENERATION_MASK);
    if (generation != (cpu->io.dma_generation[channel] & DMA_EVENT_GENERATION_MASK)) {
        return;
    }
    base = dspic33_device_internal_dma_channel_base(channel);
    if (completion) {
        complete_dma_transfer(cpu, channel, event_value);
        return;
    }
    if (cpu->io.dma_active != 0u) {
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, source, event_value, 1u)) {
            if (forced) {
                cpu->io.dma_forced_pending &= (uint16_t)~bit;
            } else {
                cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
            }
        }
        return;
    }
    control = dspic33_device_internal_raw_word(cpu, base);
    if ((control & DMA_CON_CHEN) == 0u ||
        (dspic33_device_internal_raw_word(cpu, DMA_PWC) & bit) != 0u) {
        if (forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~bit;
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
                           ~DMA_REQ_FORCE));
        } else {
            cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
            cpu->io.dma_arbiter_waiting &= (uint16_t)~bit;
        }
        return;
    }
    if (!forced && dma_cpu_bus_busy(cpu) &&
        (dspic33_device_internal_raw_word(cpu, 0x0058u) & 0x0020u) == 0u &&
        !dma_target_is_dual_port_ram(cpu, channel, (uint16_t)event_value)) {
        cpu->io.dma_arbiter_waiting |= bit;
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, source, event_value, 1u)) {
            cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
            cpu->io.dma_arbiter_waiting &= (uint16_t)~bit;
        }
        return;
    }
    if (!forced) {
        cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
        cpu->io.dma_arbiter_waiting &= (uint16_t)~bit;
    }
    pad = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 0x0cu));
    width = (control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    address = dma_transfer_address(cpu, channel, control, (uint16_t)event_value);
    if (!dma_memory_address_valid(address, width)) {
        dspic33_raise_dma_address_trap(cpu);
        if (forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~bit;
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
                           ~DMA_REQ_FORCE));
        }
        return;
    }
    dma_record_transfer(cpu, channel, control, address);
    cpu->io.dma_active |= bit;
    cpu->io.dma_transfer_width = width;
    cpu->io.dma_transfer_active = true;
    if ((control & DMA_CON_RAM_TO_PERIPHERAL) != 0u) {
        value = dma_read_memory(cpu, address, width);
        if (dma_pad_writable(pad)) {
            if (dma_write_collision(cpu, pad, width)) {
                if (!dma_can_write_pad(pad)) {
                    dma_peripheral_write_collision(cpu, channel);
                }
            } else if (width == 1u) {
                dspic33_write_byte(cpu, pad, (uint8_t)value);
            } else {
                dspic33_write_word(cpu, pad, value);
            }
        }
    } else {
        value = 0u;
        if (dma_pad_readable(pad)) {
            uart_errors = width == 2u ? uart_dma_error_bits(cpu, pad) : 0u;
            value = width == 1u ? dspic33_read_byte(cpu, pad) : dspic33_read_word(cpu, pad);
            value |= uart_errors;
        }
        dma_write_memory(cpu, address, width, value);
        if ((control & DMA_CON_NULL_WRITE) != 0u && dma_pad_writable(pad)) {
            if (dma_write_collision(cpu, pad, width)) {
                if (!dma_can_write_pad(pad)) {
                    dma_peripheral_write_collision(cpu, channel);
                }
            } else if (width == 1u) {
                dspic33_write_byte(cpu, pad, 0u);
            } else {
                dspic33_write_word(cpu, pad, 0u);
            }
        }
    }
    cpu->io.dma_transfer_active = false;
    cpu->io.dma_transfer_width = 0u;
    if ((cpu->io.dma_active & bit) != 0u &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_DMA, (uint16_t)(channel + DSPIC33_DMA_COUNT),
                          event_value, 1u)) {
        cpu->io.dma_active &= (uint16_t)~bit;
        if (forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~bit;
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(base + 2u),
                (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
                           ~DMA_REQ_FORCE));
        }
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static uint32_t dma_event_value(const Dspic33* cpu, uint8_t channel, uint16_t indirect_address,
                                bool forced) {
    uint32_t value = indirect_address;
    value |= ((uint32_t)cpu->io.dma_generation[channel] & DMA_EVENT_GENERATION_MASK)
             << DMA_EVENT_GENERATION_SHIFT;
    if (forced) {
        value |= DMA_EVENT_FORCE;
    }
    return value;
}

bool dspic33_device_internal_schedule_dma_channel(Dspic33* cpu, uint8_t channel,
                                                  uint16_t indirect_address, bool forced,
                                                  uint64_t delay) {
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    uint16_t* pending = forced ? &cpu->io.dma_forced_pending : &cpu->io.dma_peripheral_pending;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, channel,
                          dma_event_value(cpu, channel, indirect_address, forced), delay)) {
        return false;
    }
    *pending |= bit;
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        dspic33_device_internal_dma_update_power_state(cpu);
    }
    return true;
}

static bool dma_target_is_dual_port_ram(const Dspic33* cpu, uint8_t channel,
                                        uint16_t indirect_address) {
    uint16_t control =
        dspic33_device_internal_raw_word(cpu, dspic33_device_internal_dma_channel_base(channel));
    uint8_t width = (control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    uint32_t address = dma_transfer_address(cpu, channel, control, indirect_address);
    return address >= 0xd000u && address + width <= 0xe000u;
}

void dspic33_device_internal_dma_request_collision(Dspic33* cpu, uint8_t channel) {
    uint16_t status = dspic33_device_internal_raw_word(cpu, DMA_RQC);
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    if ((status & bit) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, DMA_RQC, (uint16_t)(status | bit));
        dspic33_raise_dma_collision_trap(cpu);
    }
}
