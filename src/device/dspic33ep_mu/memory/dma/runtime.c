#include "device/dspic33ep_mu/internal.h"

bool dspic33_device_internal_comparator_pin_channel(const Dspic33* cpu, uint8_t pin_index,
                                                    uint8_t* comparator_index) {
    const uint8_t output_function = dspic33_device_internal_pps_output_function(cpu, pin_index);

    if (output_function >= COMPARATOR_PPS_FUNCTION &&
        output_function < COMPARATOR_PPS_FUNCTION + DSPIC33_COMPARATOR_COUNT) {
        *comparator_index = (uint8_t)(output_function - COMPARATOR_PPS_FUNCTION);
        return true;
    }
    return false;
}

bool dspic33_device_internal_can_queue_push(Dspic33CanQueue* queue,
                                            const Dspic33CanFrame* input_frame) {
    uint8_t queue_index;

    if (queue->count == 64u) {
        return false;
    }
    queue_index = (uint8_t)((queue->head + queue->count) % 64u);
    queue->frames[queue_index] = *input_frame;
    queue->count++;
    return true;
}

bool dspic33_device_internal_can_queue_pop(Dspic33CanQueue* queue, Dspic33CanFrame* output_frame) {
    if (queue->count == 0u) {
        return false;
    }
    *output_frame = queue->frames[queue->head];
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
    const bool left_is_dma = left->type == DSPIC33_EVENT_DMA;
    const bool right_is_dma = right->type == DSPIC33_EVENT_DMA;

    if (left->paused != right->paused) {
        return !left->paused;
    }
    if (left->cycle != right->cycle) {
        return left->cycle < right->cycle;
    }
    if (left_is_dma && right_is_dma && left->source != right->source) {
        const bool left_is_completion = left->source >= DSPIC33_DMA_COUNT;
        const bool right_is_completion = right->source >= DSPIC33_DMA_COUNT;

        if (left_is_completion != right_is_completion) {
            return left_is_completion;
        }
        return left->source % DSPIC33_DMA_COUNT < right->source % DSPIC33_DMA_COUNT;
    }
    return left->sequence < right->sequence;
}

static bool event_reserve(Dspic33EventQueue* queue) {
    Dspic33Event* resized_events;
    size_t new_event_capacity;

    if (queue->count < queue->capacity) {
        return true;
    }
    new_event_capacity = queue->capacity == 0u ? 64u : queue->capacity * 2u;
    resized_events = realloc(queue->items, new_event_capacity * sizeof(*resized_events));
    if (resized_events == NULL) {
        return false;
    }
    queue->items = resized_events;
    queue->capacity = new_event_capacity;
    return true;
}

static bool event_source_valid(const Dspic33* cpu, Dspic33EventType type, uint16_t event_source,
                               uint32_t event_value) {
    switch (type) {
    case DSPIC33_EVENT_TIMER_INTERRUPT:
        return event_source < DSPIC33_TIMER_COUNT;
    case DSPIC33_EVENT_TIMER_PMD:
        return event_source < 5u;
    case DSPIC33_EVENT_PWM_FAULT:
    case DSPIC33_EVENT_PWM_CURRENT_LIMIT:
        return event_source < DSPIC33_PWM_INPUT_COUNT;
    case DSPIC33_EVENT_PWM_DEAD_TIME:
        return event_source < dspic33_device_internal_pwm_generator_count(cpu);
    case DSPIC33_EVENT_PWM_SYNC:
        return event_source < 2u;
    case DSPIC33_EVENT_OUTPUT_COMPARE_FAULT:
        return (event_value & OUTPUT_COMPARE_FAULT_EVENT_PIN) != 0u
                   ? event_source <= UINT8_MAX &&
                         dspic33_device_internal_pps_pin_bonded(cpu, (uint8_t)event_source)
                   : event_source < DSPIC33_OUTPUT_COMPARE_FAULT_COUNT;
    default:
        return true;
    }
}

static bool schedule_event(Dspic33* cpu, Dspic33EventType type, uint16_t event_source,
                           uint32_t event_value, uint64_t event_delay, bool is_external) {
    Dspic33Event event;
    size_t heap_index;
    size_t parent_heap_index;

    if (!event_source_valid(cpu, type, event_source, event_value) ||
        event_delay > UINT64_MAX - cpu->device_cycles) {
        return false;
    }
    if (!event_reserve(&cpu->events)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }

    event.cycle = cpu->device_cycles + event_delay;
    event.sequence = cpu->events.sequence++;
    event.paused_remaining = 0u;
    event.value = event_value;
    event.source = event_source;
    event.type = type;
    event.paused = false;
    event.external = is_external;
    heap_index = cpu->events.count++;
    while (heap_index != 0u) {
        parent_heap_index = (heap_index - 1u) / 2u;
        if (!event_less(&event, &cpu->events.items[parent_heap_index])) {
            break;
        }
        cpu->events.items[heap_index] = cpu->events.items[parent_heap_index];
        heap_index = parent_heap_index;
    }
    cpu->events.items[heap_index] = event;
    return true;
}

bool dspic33_schedule(Dspic33* cpu, Dspic33EventType type, uint16_t event_source,
                      uint32_t event_value, uint64_t event_delay) {
    return schedule_event(cpu, type, event_source, event_value, event_delay, false);
}

bool dspic33_schedule_external(Dspic33* cpu, Dspic33EventType type, uint16_t event_source,
                               uint32_t event_value, uint64_t event_delay) {
    return schedule_event(cpu, type, event_source, event_value, event_delay, true);
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
    Dspic33Event first_event = queue->items[0];
    Dspic33Event last_event = queue->items[--queue->count];
    size_t insertion_index = 0u;

    while (insertion_index * 2u + 1u < queue->count) {
        size_t child_index = insertion_index * 2u + 1u;
        if (child_index + 1u < queue->count &&
            event_less(&queue->items[child_index + 1u], &queue->items[child_index])) {
            child_index++;
        }
        if (!event_less(&queue->items[child_index], &last_event)) {
            break;
        }
        queue->items[insertion_index] = queue->items[child_index];
        insertion_index = child_index;
    }
    if (queue->count != 0u) {
        queue->items[insertion_index] = last_event;
    }
    return first_event;
}

static void update_nested_do_interrupt_request(Dspic33* cpu, uint16_t interrupt_number);

void dspic33_raise_interrupt(Dspic33* cpu, uint16_t interrupt_number) {
    uint16_t status_address;
    uint16_t interrupt_mask;
    uint16_t interrupt_status;

    if (interrupt_number >= DSPIC33_IRQ_COUNT) {
        return;
    }
    status_address = (uint16_t)(0x0800u + (interrupt_number / 16u) * 2u);
    interrupt_mask = (uint16_t)(1u << (interrupt_number % 16u));
    interrupt_status = dspic33_device_internal_raw_word(cpu, status_address);
    dspic33_device_internal_raw_write_word(cpu, status_address,
                                           (uint16_t)(interrupt_status | interrupt_mask));
    if ((interrupt_status & interrupt_mask) == 0u) {
        update_nested_do_interrupt_request(cpu, interrupt_number);
    }
}

void dspic33_device_internal_raise_external_interrupt(Dspic33* cpu, uint8_t external_channel) {
    if (external_channel == 0u) {
        uint8_t adc_module;

        dspic33_dma_request(cpu, 0u, 0u, 0u);
        for (adc_module = 0u; adc_module < DSPIC33_ADC_COUNT; adc_module++) {
            dspic33_adc_trigger(cpu, adc_module, 1u, 0u);
        }
    } else if (external_channel == 1u) {
        dspic33_device_internal_output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_INT1);
    } else if (external_channel == 2u) {
        dspic33_device_internal_output_compare_pulse_source(cpu, OUTPUT_COMPARE_SYNC_INT2);
    }
    dspic33_raise_interrupt(cpu, dspic33_device_external_interrupt_irqs[external_channel]);
}

void dspic33_device_internal_raise_scheduled_interrupt(Dspic33* cpu, uint16_t interrupt_number) {
    uint8_t external_channel;

    for (external_channel = 0u; external_channel < DSPIC33_EXTERNAL_INTERRUPT_COUNT;
         external_channel++) {
        if (dspic33_device_external_interrupt_irqs[external_channel] == interrupt_number) {
            dspic33_device_internal_raise_external_interrupt(cpu, external_channel);
            return;
        }
    }
    dspic33_raise_interrupt(cpu, interrupt_number);
}

static uint8_t interrupt_priority(const Dspic33* cpu, uint16_t interrupt_number) {
    const uint16_t priority_word =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0840u + (interrupt_number / 4u) * 2u));

    return (uint8_t)((priority_word >> ((interrupt_number % 4u) * 4u)) & 0x07u);
}

bool dspic33_device_internal_interrupt_enabled(const Dspic33* cpu, uint16_t interrupt_number) {
    const uint16_t interrupt_mask = (uint16_t)(1u << (interrupt_number % 16u));
    const uint16_t status_offset = (uint16_t)((interrupt_number / 16u) * 2u);

    return (dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0800u + status_offset)) &
            interrupt_mask) != 0u &&
           (dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0820u + status_offset)) &
            interrupt_mask) != 0u;
}

static bool interrupt_deferred(const Dspic33* cpu, uint16_t interrupt_number) {
    return (cpu->interrupt_deferred[interrupt_number / 16u] &
            (uint16_t)(1u << (interrupt_number % 16u))) != 0u;
}

void dspic33_device_latch_interrupt(Dspic33* cpu, uint8_t interrupt_vector, uint8_t priority) {
    dspic33_device_internal_raw_write_word(
        cpu, 0x08c8u, (uint16_t)(((uint16_t)priority << 8u) | interrupt_vector));
}

void dspic33_device_latch_math_error(Dspic33* cpu, uint16_t error_flags) {
    dspic33_device_internal_raw_write_word(
        cpu, 0x08c0u,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, 0x08c0u) | error_flags | 0x0010u));
    dspic33_set_math_error_source(cpu, true);
}

static bool select_interrupt(const Dspic33* cpu, uint16_t* selected_interrupt,
                             uint8_t* selected_priority) {
    const uint8_t current_priority = (uint8_t)((cpu->sr >> 5u) & 0x07u);
    uint8_t highest_priority = current_priority;
    uint16_t highest_interrupt = DSPIC33_IRQ_COUNT;
    uint16_t interrupt_number;
    uint16_t interrupt_group;

    if (!cpu->async_events_enabled ||
        ((dspic33_device_internal_raw_word(cpu, 0x08c2u) & 0x8000u) == 0u &&
         cpu->gie_disable_deferred == 0u) ||
        (cpu->corcon & 0x0008u) != 0u) {
        return false;
    }
    for (interrupt_group = 0u; interrupt_group < (DSPIC33_IRQ_COUNT + 15u) / 16u;
         interrupt_group++) {
        const uint16_t status_offset = (uint16_t)(interrupt_group * 2u);

        if ((dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0800u + status_offset)) &
             dspic33_device_internal_raw_word(cpu, (uint16_t)(0x0820u + status_offset))) != 0u) {
            break;
        }
    }
    if (interrupt_group == (DSPIC33_IRQ_COUNT + 15u) / 16u) {
        return false;
    }
    for (interrupt_number = 0u; interrupt_number < DSPIC33_IRQ_COUNT; interrupt_number++) {
        const uint8_t priority = interrupt_priority(cpu, interrupt_number);

        if (!dspic33_device_internal_interrupt_enabled(cpu, interrupt_number) ||
            interrupt_deferred(cpu, interrupt_number)) {
            continue;
        }
        if (cpu->disicnt != 0u && priority < 7u) {
            continue;
        }
        if (priority > highest_priority) {
            highest_priority = priority;
            highest_interrupt = interrupt_number;
        }
    }
    if (highest_interrupt == DSPIC33_IRQ_COUNT) {
        return false;
    }
    *selected_interrupt = highest_interrupt;
    *selected_priority = highest_priority;
    return true;
}

static void recover_from_doze(Dspic33* cpu) {
    if ((dspic33_device_internal_raw_word(cpu, MAIN_CLOCK_DIVISOR) & 0x8000u) != 0u) {
        dspic33_device_internal_raw_write_word(
            cpu, MAIN_CLOCK_DIVISOR,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, MAIN_CLOCK_DIVISOR) & ~0x0800u));
    }
}

static void update_nested_do_interrupt_request(Dspic33* cpu, uint16_t interrupt_number) {
    uint8_t do_depth = cpu->nested_do_interrupt_depth;
    uint8_t selected_priority;
    uint16_t selected_interrupt;

    if ((dspic33_device_internal_raw_word(cpu, 0x08c0u) & 0x8000u) != 0u) {
        cpu->nested_do_interrupt_armed = false;
        return;
    }
    if (!select_interrupt(cpu, &selected_interrupt, &selected_priority) ||
        selected_interrupt != interrupt_number) {
        return;
    }
    if (cpu->nested_do_interrupt_armed) {
        if (selected_priority > cpu->nested_do_interrupt_priority &&
            cpu->device_cycles - cpu->nested_do_interrupt_cycle ==
                dspic33_device_instruction_cycles(cpu, 4u) &&
            do_depth != 0u && cpu->do_depth >= do_depth &&
            cpu->do_end[do_depth - 1u] == cpu->nested_do_interrupt_end) {
            cpu->nested_do_extra_decrement_depth = do_depth;
            cpu->nested_do_extra_decrement_end = cpu->nested_do_interrupt_end;
        }
        cpu->nested_do_interrupt_armed = false;
        return;
    }
    if (cpu->interrupt_entry_active) {
        return;
    }
    do_depth = cpu->do_depth;
    if (do_depth != 0u) {
        const uint32_t do_end = cpu->do_end[do_depth - 1u];
        const uint32_t previous_instruction = (do_end - 2u) & 0x007ffffeu;
        const uint32_t instruction_origin =
            cpu->instruction_advancing ? cpu->current_instruction_pc : cpu->pc;

        if (instruction_origin == do_end || instruction_origin == previous_instruction) {
            cpu->nested_do_interrupt_cycle = cpu->device_cycles;
            cpu->nested_do_interrupt_end = do_end;
            cpu->nested_do_interrupt_depth = do_depth;
            cpu->nested_do_interrupt_priority = selected_priority;
            cpu->nested_do_interrupt_armed = true;
        }
    }
}

static bool service_interrupt(Dspic33* cpu) {
    uint8_t selected_priority;
    uint16_t selected_interrupt;
    uint16_t entry_priority;
    size_t interrupt_log_index;

    uint16_t stacked_high;
    uint64_t entry_device_cycles;
    uint64_t first_entry_cycles;
    uint64_t first_entry_device_cycles;
    uint64_t entry_cycles;
    uint32_t instruction_address;
    uint32_t vector_address;

    if (!select_interrupt(cpu, &selected_interrupt, &selected_priority)) {
        return false;
    }
    dspic33_cancel_flash_read_sequence(cpu);
    instruction_address = cpu->pc;
    vector_address =
        dspic33_read_program_word(cpu, instruction_address >= DSPIC33_AUXILIARY_PROGRAM_BASE
                                           ? 0x007ffffau
                                           : 0x0014u + selected_interrupt * 2u) &
        0x007ffffeu;
    if (!dspic33_device_program_range_implemented(cpu, vector_address, 2u)) {
        dspic33_raise_program_vector_error(cpu, instruction_address);
        return true;
    }
    if (!dspic33_codeguard_admit_program_flow(cpu, instruction_address, vector_address)) {
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
    entry_priority = (dspic33_device_internal_raw_word(cpu, 0x08c0u) & 0x8000u) != 0u
                         ? UINT16_C(0x00e0)
                         : (uint16_t)((uint16_t)selected_priority << 5u);
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
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | entry_priority);
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
    interrupt_log_index = (size_t)(cpu->interrupt_count % 16u);
    cpu->interrupt_log_irq[interrupt_log_index] = selected_interrupt;
    cpu->interrupt_log_entry[interrupt_log_index] = cpu->pc;
    cpu->interrupt_log_return[interrupt_log_index] = 0u;
    cpu->pc = vector_address;
    cpu->last_interrupt = selected_interrupt;
    cpu->interrupt_count++;
    cpu->interrupt_depth++;
    dspic33_device_latch_interrupt(cpu, (uint8_t)(selected_interrupt + 8u), selected_priority);
    cpu->repeat_active = 0u;
    cpu->repeat_pc = 0u;
    cpu->sr &= 0xffefu;
    return true;
}

bool dspic33_device_interrupt_pending(const Dspic33* cpu) {
    uint8_t selected_priority;
    uint16_t interrupt_number;

    return select_interrupt(cpu, &interrupt_number, &selected_priority);
}

bool dspic33_device_service_interrupt(Dspic33* cpu) { return service_interrupt(cpu); }

bool dspic33_device_wake(Dspic33* cpu) {
    uint16_t interrupt_number;

    if (!cpu->async_events_enabled) {
        return false;
    }
    for (interrupt_number = 0u; interrupt_number < DSPIC33_IRQ_COUNT; interrupt_number++) {
        if (dspic33_device_internal_interrupt_enabled(cpu, interrupt_number) &&
            !interrupt_deferred(cpu, interrupt_number) &&
            interrupt_priority(cpu, interrupt_number) != 0u) {
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
    uint16_t stacked_high;
    uint16_t stacked_low;

    dspic33_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u, 2u);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    stacked_high = dspic33_read_word(cpu, cpu->w[15]);
    dspic33_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u, 2u);
    dspic33_set_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    stacked_low = dspic33_read_word(cpu, cpu->w[15]);
    cpu->pc = ((uint32_t)(stacked_high & 0x007fu) << 16u) | (stacked_low & 0xfffeu);
    cpu->last_interrupt_return = cpu->pc;
    if (cpu->interrupt_count != 0u) {
        cpu->interrupt_log_return[(cpu->interrupt_count - 1u) % 16u] = cpu->pc;
    }
    cpu->sr = (uint16_t)((cpu->sr & 0xff00u) | (stacked_high >> 8u));
    if ((stacked_high & 0x0080u) != 0u) {
        cpu->corcon |= 0x0008u;
    } else {
        cpu->corcon &= 0xfff7u;
    }
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0004u) | ((stacked_low & 1u) << 2u));
    cpu->repeat_active = (cpu->sr & 0x0010u) != 0u;
    cpu->repeat_pc = cpu->repeat_active != 0u ? cpu->pc : 0u;
    cpu->repeat_psv_reentry = cpu->repeat_active != 0u;
    if (cpu->interrupt_depth != 0u) {
        cpu->interrupt_depth--;
    }
}

uint16_t dspic33_device_internal_dma_channel_base(uint8_t channel_index) {
    return (uint16_t)(DMA_CHANNEL_BASE + channel_index * DMA_CHANNEL_STRIDE);
}

uint16_t dspic33_device_internal_dma_channel_bit(uint8_t channel_index) {
    return (uint16_t)(1u << channel_index);
}

static bool dma_channel_event(const Dspic33Event* event, uint8_t channel_index) {
    return event->type == DSPIC33_EVENT_DMA && event->source % DSPIC33_DMA_COUNT == channel_index;
}

static void dma_remove_channel_events(Dspic33* cpu, uint8_t channel_index) {
    size_t event_index;
    size_t compacted_index = 0u;

    for (event_index = 0u; event_index < cpu->events.count; event_index++) {
        if (!dma_channel_event(&cpu->events.items[event_index], channel_index)) {
            cpu->events.items[compacted_index++] = cpu->events.items[event_index];
        }
    }
    cpu->events.count = compacted_index;
    dspic33_reorder_events(cpu);
}

void dspic33_device_internal_dma_advance_generation(Dspic33* cpu, uint8_t channel_index) {
    if ((cpu->io.dma_generation[channel_index] & DMA_EVENT_GENERATION_MASK) ==
        DMA_EVENT_GENERATION_MASK) {
        dma_remove_channel_events(cpu, channel_index);
    }
    cpu->io.dma_generation[channel_index]++;
}

void dspic33_device_internal_dma_update_power_state(Dspic33* cpu) {
    size_t event_index;
    const bool dma_available = cpu->power_state != DSPIC33_POWER_SLEEP;
    bool events_reordered = false;

    for (event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        if (event->type != DSPIC33_EVENT_DMA) {
            continue;
        }
        if (!dma_available && !event->paused) {
            event->paused_remaining = event->cycle - cpu->device_cycles;
            event->paused = true;
            events_reordered = true;
        } else if (dma_available && event->paused) {
            if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
                cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
                continue;
            }
            event->cycle = cpu->device_cycles + event->paused_remaining;
            event->paused_remaining = 0u;
            event->paused = false;
            events_reordered = true;
        }
    }
    if (events_reordered) {
        dspic33_reorder_events(cpu);
    }
}

static uint32_t dma_start_address(const Dspic33* cpu, uint8_t channel_index) {
    return (cpu->io.dma_bank & dspic33_device_internal_dma_channel_bit(channel_index)) != 0u
               ? cpu->io.dma_start_b[channel_index]
               : cpu->io.dma_start_a[channel_index];
}

static uint32_t dma_transfer_address(const Dspic33* cpu, uint8_t channel_index,
                                     uint16_t dma_control, uint16_t peripheral_offset) {
    const uint32_t start_address = dma_start_address(cpu, channel_index);
    const uint16_t address_mode = dma_control & DMA_CON_AMODE_MASK;

    if (address_mode == DMA_CON_AMODE_PERIPHERAL) {
        return start_address | peripheral_offset;
    }
    return cpu->io.dma_address[channel_index];
}

static bool dma_memory_address_valid(const Dspic33* cpu, uint32_t memory_address,
                                     uint8_t transfer_width) {
    return memory_address >= 0x1000u && (transfer_width == 1u || (memory_address & 1u) == 0u) &&
           dspic33_device_data_range_implemented(cpu, memory_address, transfer_width);
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

static bool dma_cpu_wrote_byte(const Dspic33* cpu, uint32_t memory_address) {
    return dma_write_cycle_matches(cpu) && memory_address >= cpu->io.cpu_write_address &&
           memory_address < cpu->io.cpu_write_address + cpu->io.cpu_write_width;
}

static uint8_t dma_read_memory_byte(const Dspic33* cpu, uint32_t memory_address) {
    if (dma_cpu_wrote_byte(cpu, memory_address)) {
        const uint8_t byte_shift = (uint8_t)((memory_address - cpu->io.cpu_write_address) * 8u);
        return (uint8_t)(cpu->io.cpu_write_previous >> byte_shift);
    }
    return cpu->data[memory_address];
}

static uint16_t dma_read_memory(const Dspic33* cpu, uint32_t memory_address,
                                uint8_t transfer_width) {
    if (transfer_width == 1u) {
        return dma_read_memory_byte(cpu, memory_address);
    }
    return (uint16_t)(dma_read_memory_byte(cpu, memory_address) |
                      ((uint16_t)dma_read_memory_byte(cpu, memory_address + 1u) << 8u));
}

static void dma_write_memory(Dspic33* cpu, uint32_t memory_address, uint8_t transfer_width,
                             uint16_t transfer_value) {
    if (!dma_cpu_wrote_byte(cpu, memory_address)) {
        cpu->data[memory_address] = (uint8_t)transfer_value;
    }
    if (transfer_width == 2u && !dma_cpu_wrote_byte(cpu, memory_address + 1u)) {
        cpu->data[memory_address + 1u] = (uint8_t)(transfer_value >> 8u);
    }
}

static void dma_record_transfer(Dspic33* cpu, uint8_t channel_index, uint16_t dma_control,
                                uint32_t memory_address) {
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);
    const uint16_t channel_base = dspic33_device_internal_dma_channel_base(channel_index);
    const uint16_t start_offset = (cpu->io.dma_bank & channel_bit) != 0u ? 8u : 4u;
    uint16_t ping_pong = dspic33_device_internal_raw_word(cpu, DMA_PPS);
    if ((cpu->io.dma_bank & channel_bit) != 0u) {
        ping_pong |= channel_bit;
    } else {
        ping_pong &= (uint16_t)~channel_bit;
    }
    dspic33_device_internal_raw_write_word(cpu, DMA_PPS, (uint16_t)(ping_pong & DMA_CHANNEL_MASK));
    dspic33_device_internal_raw_write_word(cpu, DMA_LCA, channel_index);
    dspic33_device_internal_raw_write_word(cpu, DMA_SADRL, (uint16_t)memory_address);
    dspic33_device_internal_raw_write_word(cpu, DMA_SADRH,
                                           (uint16_t)((memory_address >> 16u) & 0x00ffu));
    if ((dma_control & DMA_CON_AMODE_MASK) == DMA_CON_AMODE_PERIPHERAL) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(channel_base + start_offset),
                                               (uint16_t)memory_address);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(channel_base + start_offset + 2u),
                                               (uint16_t)((memory_address >> 16u) & 0x00ffu));
    }
}

static bool dma_write_collision(const Dspic33* cpu, uint16_t peripheral_address,
                                uint8_t transfer_width) {
    const uint32_t cpu_write_end = cpu->io.cpu_write_address + cpu->io.cpu_write_width;
    const uint32_t dma_write_end = (uint32_t)peripheral_address + transfer_width;

    if (!dma_write_cycle_matches(cpu)) {
        return false;
    }
    return cpu->io.cpu_write_address < dma_write_end && peripheral_address < cpu_write_end;
}

static bool dma_can_write_pad(uint16_t peripheral_address) {
    return peripheral_address == 0x0442u || peripheral_address == 0x0542u;
}

static void dma_peripheral_write_collision(Dspic33* cpu, uint8_t channel_index) {
    uint16_t status = dspic33_device_internal_raw_word(cpu, DMA_PWC);
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);

    if ((status & channel_bit) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, DMA_PWC, (uint16_t)(status | channel_bit));
        dspic33_raise_dma_collision_trap(cpu);
    }
}

static uint16_t uart_dma_error_bits(const Dspic33* cpu, uint16_t peripheral_address) {
    uint8_t uart_channel;

    for (uart_channel = 0u; uart_channel < DSPIC33_UART_COUNT; uart_channel++) {
        if (peripheral_address == dspic33_device_uart_bases[uart_channel] + 6u) {
            uint16_t status = dspic33_device_internal_raw_word(
                cpu, (uint16_t)(dspic33_device_uart_bases[uart_channel] + 2u));
            return (uint16_t)(((status & UART_STATUS_PARITY_ERROR) != 0u ? 0x0800u : 0u) |
                              ((status & UART_STATUS_FRAMING_ERROR) != 0u ? 0x0400u : 0u));
        }
    }
    return 0u;
}

static bool dma_pad_in_set(uint16_t peripheral_address, const uint16_t* peripheral_addresses,
                           size_t address_count) {
    size_t address_index;

    for (address_index = 0u; address_index < address_count; address_index++) {
        if (peripheral_address == peripheral_addresses[address_index]) {
            return true;
        }
    }
    return false;
}

static bool dma_pad_readable(uint16_t peripheral_address) {
    static const uint16_t readable_addresses[] = {
        0x0144u, 0x014cu, 0x0154u, 0x015cu, 0x0226u, 0x0236u, 0x0248u, 0x0256u, 0x0268u,
        0x0290u, 0x02a8u, 0x02b6u, 0x02c8u, 0x0300u, 0x0340u, 0x0440u, 0x0540u, 0x0608u};
    return dma_pad_in_set(peripheral_address, readable_addresses,
                          sizeof(readable_addresses) / sizeof(readable_addresses[0]));
}

static bool dma_pad_writable(uint16_t peripheral_address) {
    static const uint16_t writable_addresses[] = {
        0x0224u, 0x0234u, 0x0248u, 0x0254u, 0x0268u, 0x0298u, 0x02a8u, 0x02b4u, 0x02c8u, 0x0442u,
        0x0542u, 0x0608u, 0x0904u, 0x0906u, 0x090eu, 0x0910u, 0x0918u, 0x091au, 0x0922u, 0x0924u};
    return dma_pad_in_set(peripheral_address, writable_addresses,
                          sizeof(writable_addresses) / sizeof(writable_addresses[0]));
}

bool dspic33_device_dma_pad_valid(uint16_t peripheral_address, bool is_write) {
    return is_write ? dma_pad_writable(peripheral_address) : dma_pad_readable(peripheral_address);
}

static void dma_disable_channel(Dspic33* cpu, uint8_t channel_index, uint16_t dma_control) {
    const uint16_t channel_base = dspic33_device_internal_dma_channel_base(channel_index);
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);

    dspic33_device_internal_raw_write_word(cpu, channel_base,
                                           (uint16_t)(dma_control & ~DMA_CON_CHEN));
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(channel_base + 2u),
        (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 2u)) &
                   ~DMA_REQ_FORCE));
    cpu->io.dma_enabled &= (uint16_t)~channel_bit;
    cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
    cpu->io.dma_active &= (uint16_t)~channel_bit;
    cpu->io.dma_arbiter_waiting &= (uint16_t)~channel_bit;
    dspic33_device_internal_dma_advance_generation(cpu, channel_index);
}

static void dma_complete_block(Dspic33* cpu, uint8_t channel_index, uint16_t dma_control) {
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);
    const uint16_t transfer_mode = dma_control & DMA_CON_MODE_MASK;
    const bool is_secondary_buffer = (cpu->io.dma_bank & channel_bit) != 0u;

    if ((dma_control & DMA_CON_HALF) == 0u) {
        dspic33_raise_interrupt(cpu, dspic33_device_dma_irqs[channel_index]);
    }
    cpu->io.dma_index[channel_index] = 0u;
    cpu->io.dma_half_raised &= (uint16_t)~channel_bit;
    if ((transfer_mode & DMA_CON_MODE_ONE_SHOT) != 0u &&
        ((transfer_mode & DMA_CON_MODE_PING_PONG) == 0u || is_secondary_buffer)) {
        dma_disable_channel(cpu, channel_index, dma_control);
        return;
    }
    if ((transfer_mode & DMA_CON_MODE_PING_PONG) != 0u) {
        cpu->io.dma_bank ^= channel_bit;
    }
    cpu->io.dma_address[channel_index] = dma_start_address(cpu, channel_index);
}

static void complete_dma_transfer(Dspic33* cpu, uint8_t channel_index, uint32_t event_value) {
    const uint16_t channel_base = dspic33_device_internal_dma_channel_base(channel_index);
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);
    const uint16_t dma_control = dspic33_device_internal_raw_word(cpu, channel_base);
    const uint16_t transfer_count =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 0x0eu));
    const uint16_t transfer_index = cpu->io.dma_index[channel_index];
    const uint16_t transferred_count = (uint16_t)(transfer_index + 1u);
    const uint16_t half_count = (uint16_t)(((uint32_t)transfer_count + 2u) / 2u);
    const uint8_t transfer_width = (dma_control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    const bool is_forced = (event_value & DMA_EVENT_FORCE) != 0u;

    if ((cpu->io.dma_active & channel_bit) == 0u) {
        return;
    }
    if ((dma_control & DMA_CON_AMODE_MASK) == 0u) {
        cpu->io.dma_address[channel_index] += transfer_width;
    }
    if ((dma_control & DMA_CON_HALF) != 0u && transferred_count == half_count &&
        (cpu->io.dma_half_raised & channel_bit) == 0u) {
        cpu->io.dma_half_raised |= channel_bit;
        dspic33_raise_interrupt(cpu, dspic33_device_dma_irqs[channel_index]);
    }
    if (transfer_index >= transfer_count) {
        dma_complete_block(cpu, channel_index, dma_control);
    } else {
        cpu->io.dma_index[channel_index]++;
    }
    cpu->io.dma_active &= (uint16_t)~channel_bit;
    if (is_forced) {
        cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(channel_base + 2u),
            (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 2u)) &
                       ~DMA_REQ_FORCE));
    }
}

static bool dma_target_is_dual_port_ram(const Dspic33* cpu, uint8_t channel_index,
                                        uint16_t peripheral_offset);

void dspic33_device_internal_run_dma(Dspic33* cpu, uint16_t event_source, uint32_t event_value) {
    uint16_t channel_base;
    uint16_t channel_bit;
    uint16_t dma_control;
    uint16_t peripheral_address;
    uint16_t transfer_value;
    uint16_t uart_errors;
    uint16_t event_generation;
    uint32_t memory_address;
    uint8_t transfer_width;
    uint8_t channel_index;
    bool is_forced;
    bool is_completion;

    if (event_source >= DSPIC33_DMA_COUNT * 2u) {
        return;
    }
    is_completion = event_source >= DSPIC33_DMA_COUNT;
    channel_index = (uint8_t)(event_source % DSPIC33_DMA_COUNT);
    channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);
    is_forced = (event_value & DMA_EVENT_FORCE) != 0u;
    event_generation =
        (uint16_t)((event_value >> DMA_EVENT_GENERATION_SHIFT) & DMA_EVENT_GENERATION_MASK);
    if (event_generation != (cpu->io.dma_generation[channel_index] & DMA_EVENT_GENERATION_MASK)) {
        return;
    }
    channel_base = dspic33_device_internal_dma_channel_base(channel_index);
    if (is_completion) {
        complete_dma_transfer(cpu, channel_index, event_value);
        return;
    }
    if (cpu->io.dma_active != 0u) {
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, event_source, event_value, 1u)) {
            if (is_forced) {
                cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
            } else {
                cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
            }
        }
        return;
    }
    dma_control = dspic33_device_internal_raw_word(cpu, channel_base);
    if ((dma_control & DMA_CON_CHEN) == 0u ||
        (dspic33_device_internal_raw_word(cpu, DMA_PWC) & channel_bit) != 0u) {
        if (is_forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(channel_base + 2u),
                (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 2u)) &
                           ~DMA_REQ_FORCE));
        } else {
            cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
            cpu->io.dma_arbiter_waiting &= (uint16_t)~channel_bit;
        }
        return;
    }
    if (!is_forced && dma_cpu_bus_busy(cpu) &&
        (dspic33_device_internal_raw_word(cpu, 0x0058u) & 0x0020u) == 0u &&
        !dma_target_is_dual_port_ram(cpu, channel_index, (uint16_t)event_value)) {
        cpu->io.dma_arbiter_waiting |= channel_bit;
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, event_source, event_value, 1u)) {
            cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
            cpu->io.dma_arbiter_waiting &= (uint16_t)~channel_bit;
        }
        return;
    }
    if (!is_forced) {
        cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
        cpu->io.dma_arbiter_waiting &= (uint16_t)~channel_bit;
    }
    peripheral_address = dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 0x0cu));
    transfer_width = (dma_control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    memory_address = dma_transfer_address(cpu, channel_index, dma_control, (uint16_t)event_value);
    if (!dma_memory_address_valid(cpu, memory_address, transfer_width)) {
        dspic33_raise_dma_address_trap(cpu);
        if (is_forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(channel_base + 2u),
                (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 2u)) &
                           ~DMA_REQ_FORCE));
        }
        return;
    }
    dma_record_transfer(cpu, channel_index, dma_control, memory_address);
    cpu->io.dma_active |= channel_bit;
    cpu->io.dma_transfer_width = transfer_width;
    cpu->io.dma_transfer_active = true;
    if ((dma_control & DMA_CON_RAM_TO_PERIPHERAL) != 0u) {
        transfer_value = dma_read_memory(cpu, memory_address, transfer_width);
        if (dma_pad_writable(peripheral_address)) {
            if (dma_write_collision(cpu, peripheral_address, transfer_width)) {
                if (!dma_can_write_pad(peripheral_address)) {
                    dma_peripheral_write_collision(cpu, channel_index);
                }
            } else if (transfer_width == 1u) {
                dspic33_write_byte(cpu, peripheral_address, (uint8_t)transfer_value);
            } else {
                dspic33_write_word(cpu, peripheral_address, transfer_value);
            }
        }
    } else {
        transfer_value = 0u;
        if (dma_pad_readable(peripheral_address)) {
            uart_errors = transfer_width == 2u ? uart_dma_error_bits(cpu, peripheral_address) : 0u;
            transfer_value = transfer_width == 1u ? dspic33_read_byte(cpu, peripheral_address)
                                                  : dspic33_read_word(cpu, peripheral_address);
            transfer_value |= uart_errors;
        }
        dma_write_memory(cpu, memory_address, transfer_width, transfer_value);
        if ((dma_control & DMA_CON_NULL_WRITE) != 0u && dma_pad_writable(peripheral_address)) {
            if (dma_write_collision(cpu, peripheral_address, transfer_width)) {
                if (!dma_can_write_pad(peripheral_address)) {
                    dma_peripheral_write_collision(cpu, channel_index);
                }
            } else if (transfer_width == 1u) {
                dspic33_write_byte(cpu, peripheral_address, 0u);
            } else {
                dspic33_write_word(cpu, peripheral_address, 0u);
            }
        }
    }
    cpu->io.dma_transfer_active = false;
    cpu->io.dma_transfer_width = 0u;
    if ((cpu->io.dma_active & channel_bit) != 0u &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_DMA, (uint16_t)(channel_index + DSPIC33_DMA_COUNT),
                          event_value, 1u)) {
        cpu->io.dma_active &= (uint16_t)~channel_bit;
        if (is_forced) {
            cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(channel_base + 2u),
                (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 2u)) &
                           ~DMA_REQ_FORCE));
        }
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static uint32_t dma_event_value(const Dspic33* cpu, uint8_t channel_index,
                                uint16_t peripheral_offset, bool is_forced) {
    uint32_t event_value = peripheral_offset;
    event_value |= ((uint32_t)cpu->io.dma_generation[channel_index] & DMA_EVENT_GENERATION_MASK)
                   << DMA_EVENT_GENERATION_SHIFT;
    if (is_forced) {
        event_value |= DMA_EVENT_FORCE;
    }
    return event_value;
}

bool dspic33_device_internal_schedule_dma_channel(Dspic33* cpu, uint8_t channel_index,
                                                  uint16_t peripheral_offset, bool is_forced,
                                                  uint64_t event_delay) {
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);
    uint16_t* pending_events =
        is_forced ? &cpu->io.dma_forced_pending : &cpu->io.dma_peripheral_pending;

    if (!dspic33_schedule(cpu, DSPIC33_EVENT_DMA, channel_index,
                          dma_event_value(cpu, channel_index, peripheral_offset, is_forced),
                          event_delay)) {
        return false;
    }
    *pending_events |= channel_bit;
    if (cpu->power_state == DSPIC33_POWER_SLEEP) {
        dspic33_device_internal_dma_update_power_state(cpu);
    }
    return true;
}

static bool dma_target_is_dual_port_ram(const Dspic33* cpu, uint8_t channel_index,
                                        uint16_t peripheral_offset) {
    const Dspic33epMuProfile* profile = dspic33_device_profile(cpu);
    const uint16_t dma_control = dspic33_device_internal_raw_word(
        cpu, dspic33_device_internal_dma_channel_base(channel_index));
    const uint8_t transfer_width = (dma_control & DMA_CON_SIZE_BYTE) != 0u ? 1u : 2u;
    const uint32_t memory_address =
        dma_transfer_address(cpu, channel_index, dma_control, peripheral_offset);

    return profile != NULL && memory_address >= profile->dma_ram_base &&
           dspic33_device_data_range_implemented(cpu, memory_address, transfer_width);
}

void dspic33_device_internal_dma_request_collision(Dspic33* cpu, uint8_t channel_index) {
    uint16_t status = dspic33_device_internal_raw_word(cpu, DMA_RQC);
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);

    if ((status & channel_bit) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, DMA_RQC, (uint16_t)(status | channel_bit));
        dspic33_raise_dma_collision_trap(cpu);
    }
}
