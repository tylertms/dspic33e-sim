#include "device/dspic33ep_mu/internal.h"

static uint16_t input_capture_base(uint8_t channel) {
    return (uint16_t)(INPUT_CAPTURE_BASE + channel * INPUT_CAPTURE_STRIDE);
}

bool dspic33_device_internal_input_capture_pmd_disabled(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.input_capture.pmd_disabled & (uint16_t)(1u << channel)) != 0u;
}

bool dspic33_device_internal_input_capture_pair_configured(const Dspic33* cpu, uint8_t channel) {
    uint8_t first = (uint8_t)(channel & 0xfeu);
    if (first + 1u >= DSPIC33_INPUT_CAPTURE_COUNT) {
        return false;
    }
    return (dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(first) + 2u)) &
            INPUT_CAPTURE_32_BIT) != 0u &&
           (dspic33_device_internal_raw_word(
                cpu, (uint16_t)(input_capture_base((uint8_t)(first + 1u)) + 2u)) &
            INPUT_CAPTURE_32_BIT) != 0u;
}

static bool input_capture_event_belongs_to_channel(const Dspic33Event* event, uint8_t channel) {
    uint32_t kind = event->value & INPUT_CAPTURE_EVENT_KIND_MASK;
    if (event->source == channel) {
        return kind == INPUT_CAPTURE_EVENT_CAPTURE || kind == INPUT_CAPTURE_EVENT_INTERRUPT;
    }
    return kind == INPUT_CAPTURE_EVENT_CAPTURE &&
           (event->value & INPUT_CAPTURE_EVENT_PAIRED) != 0u &&
           event->source == (uint16_t)(channel & 0xfeu);
}

static bool input_capture_event_can_resume(const Dspic33* cpu, const Dspic33Event* event) {
    uint32_t kind = event->value & INPUT_CAPTURE_EVENT_KIND_MASK;
    if (kind == INPUT_CAPTURE_EVENT_CAPTURE && (event->value & INPUT_CAPTURE_EVENT_PAIRED) != 0u) {
        return !dspic33_device_internal_input_capture_pmd_disabled(cpu, (uint8_t)event->source) &&
               !dspic33_device_internal_input_capture_pmd_disabled(cpu,
                                                                   (uint8_t)(event->source + 1u));
    }
    return !dspic33_device_internal_input_capture_pmd_disabled(cpu, (uint8_t)event->source);
}

static void input_capture_pause_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (event->type != DSPIC33_EVENT_INPUT_CAPTURE || event->paused ||
            !input_capture_event_belongs_to_channel(event, channel)) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static void input_capture_resume_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        if (event->type != DSPIC33_EVENT_INPUT_CAPTURE || !event->paused ||
            !input_capture_event_belongs_to_channel(event, channel) ||
            !input_capture_event_can_resume(cpu, event)) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

static bool input_capture_fifo_push(Dspic33InputCaptureFifo* fifo, uint16_t value) {
    uint8_t index;
    if (fifo->count == DSPIC33_INPUT_CAPTURE_FIFO_SIZE) {
        return false;
    }
    index = (uint8_t)((fifo->head + fifo->count) % DSPIC33_INPUT_CAPTURE_FIFO_SIZE);
    fifo->words[index] = value;
    fifo->count++;
    return true;
}

static bool input_capture_fifo_pop(Dspic33InputCaptureFifo* fifo, uint16_t* value) {
    if (fifo->count == 0u) {
        return false;
    }
    *value = fifo->words[fifo->head];
    fifo->head = (uint8_t)((fifo->head + 1u) % DSPIC33_INPUT_CAPTURE_FIFO_SIZE);
    fifo->count--;
    return true;
}

static uint16_t input_capture_fifo_front(const Dspic33InputCaptureFifo* fifo) {
    return fifo->count == 0u ? 0u : fifo->words[fifo->head];
}

static void input_capture_refresh(Dspic33* cpu, uint8_t channel) {
    uint16_t base = input_capture_base(channel);
    uint16_t control = dspic33_device_internal_raw_word(cpu, base);
    control &= (uint16_t)~INPUT_CAPTURE_NOT_EMPTY;
    if (cpu->io.input_capture.fifo[channel].count != 0u) {
        control |= INPUT_CAPTURE_NOT_EMPTY;
    }
    dspic33_device_internal_raw_write_word(cpu, base, control);
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(base + 4u), input_capture_fifo_front(&cpu->io.input_capture.fifo[channel]));
}

static void input_capture_flush(Dspic33* cpu, uint8_t channel) {
    Dspic33InputCaptureFifo* fifo = &cpu->io.input_capture.fifo[channel];
    uint16_t base = input_capture_base(channel);
    fifo->head = 0u;
    fifo->count = 0u;
    cpu->io.input_capture.interrupt_count[channel] = 0u;
    cpu->io.input_capture.prescaler_count[channel] = 0u;
    cpu->io.input_capture.timer[channel] = 0u;
    cpu->io.input_capture.generation[channel]++;
    dspic33_device_internal_raw_write_word(
        cpu, base,
        (uint16_t)(dspic33_device_internal_raw_word(cpu, base) &
                   ~(INPUT_CAPTURE_NOT_EMPTY | INPUT_CAPTURE_OVERFLOW)));
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 4u), 0u);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 6u), 0u);
}

static bool input_capture_sync_source_valid(uint8_t channel, uint16_t control2) {
    uint8_t source = (uint8_t)(control2 & INPUT_CAPTURE_SYNC_MASK);
    if (source == INPUT_CAPTURE_SYNC_RESERVED ||
        (channel < 8u && source == INPUT_CAPTURE_SYNC_IC_FIRST + channel)) {
        return false;
    }
    return (control2 & INPUT_CAPTURE_TRIGGER) != 0u ||
           source < INPUT_CAPTURE_SYNC_COMPARATOR_FIRST || source >= 29u;
}

static bool input_capture_configuration_supported(const Dspic33* cpu, uint8_t channel) {
    uint16_t base = input_capture_base(channel);
    uint16_t control1 = dspic33_device_internal_raw_word(cpu, base);
    uint16_t control2 = dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
    uint16_t mode = control1 & INPUT_CAPTURE_MODE_MASK;
    uint16_t timer_source = control1 & INPUT_CAPTURE_TIMER_SOURCE_MASK;
    return mode != 0u && mode != 6u && mode != INPUT_CAPTURE_MODE_INTERRUPT &&
           timer_source != 0x1400u && timer_source != 0x1800u &&
           input_capture_sync_source_valid(channel, control2);
}

static bool input_capture_operating(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, input_capture_base(channel));
    if (dspic33_device_internal_input_capture_pmd_disabled(cpu, channel) ||
        (dspic33_device_internal_input_capture_pair_configured(cpu, channel) &&
         dspic33_device_internal_input_capture_pmd_disabled(cpu, (uint8_t)(channel ^ 1u))) ||
        !input_capture_configuration_supported(cpu, channel) ||
        cpu->power_state == DSPIC33_POWER_SLEEP) {
        return false;
    }
    return cpu->power_state != DSPIC33_POWER_IDLE || (control & INPUT_CAPTURE_STOP_IDLE) == 0u;
}

static bool input_capture_timer_running(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2;
    if (!input_capture_operating(cpu, channel)) {
        return false;
    }
    control2 = dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    return (control2 & INPUT_CAPTURE_TRIGGER) == 0u ||
           (control2 & INPUT_CAPTURE_TRIGGER_STATUS) != 0u;
}

static bool input_capture_pair_enabled(const Dspic33* cpu, uint8_t channel) {
    uint16_t first;
    uint16_t second;
    if ((channel & 1u) != 0u || channel + 1u >= DSPIC33_INPUT_CAPTURE_COUNT ||
        !input_capture_operating(cpu, channel) ||
        !input_capture_operating(cpu, (uint8_t)(channel + 1u))) {
        return false;
    }
    first = dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    second = dspic33_device_internal_raw_word(
        cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 2u));
    return (first & INPUT_CAPTURE_32_BIT) != 0u && (second & INPUT_CAPTURE_32_BIT) != 0u &&
           (dspic33_device_internal_raw_word(cpu, input_capture_base(channel)) &
            INPUT_CAPTURE_TIMER_SOURCE_MASK) ==
               (dspic33_device_internal_raw_word(cpu, input_capture_base((uint8_t)(channel + 1u))) &
                INPUT_CAPTURE_TIMER_SOURCE_MASK);
}

static bool input_capture_pair_timer_running(const Dspic33* cpu, uint8_t channel) {
    return input_capture_pair_enabled(cpu, channel) && input_capture_timer_running(cpu, channel) &&
           input_capture_timer_running(cpu, (uint8_t)(channel + 1u));
}

static bool input_capture_schedule_interrupt(Dspic33* cpu, uint8_t channel) {
    uint32_t value =
        INPUT_CAPTURE_EVENT_INTERRUPT | ((uint32_t)cpu->io.input_capture.generation[channel]
                                         << INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
    return dspic33_schedule(cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel, value, 2u);
}

static bool input_capture_request_dma(Dspic33* cpu, uint8_t channel) {
    uint16_t base = input_capture_base(channel);
    if (channel >= 4u ||
        (dspic33_device_internal_raw_word(cpu, base) & INPUT_CAPTURE_INTERRUPT_MASK) != 0u) {
        return true;
    }
    return dspic33_dma_request(cpu, dspic33_device_input_capture_irqs[channel],
                               (uint16_t)(base + 4u), 0u);
}

static void input_capture_reset_timer(Dspic33* cpu, uint8_t channel) {
    cpu->io.input_capture.timer[channel] = 0u;
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(input_capture_base(channel) + 6u), 0u);
}

static void input_capture_refresh_sync_outputs(Dspic33* cpu);

static void input_capture_trigger_source(Dspic33* cpu, uint8_t source) {
    uint8_t channel = 0u;
    while (channel < DSPIC33_INPUT_CAPTURE_COUNT) {
        uint16_t control2 =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
        bool paired = input_capture_pair_enabled(cpu, channel);
        if (input_capture_operating(cpu, channel) && (control2 & INPUT_CAPTURE_TRIGGER) != 0u &&
            (control2 & INPUT_CAPTURE_SYNC_MASK) == source) {
            dspic33_device_internal_raw_write_word(
                cpu, (uint16_t)(input_capture_base(channel) + 2u),
                (uint16_t)(control2 | INPUT_CAPTURE_TRIGGER_STATUS));
        }
        if (paired) {
            uint16_t second = dspic33_device_internal_raw_word(
                cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 2u));
            if (input_capture_operating(cpu, (uint8_t)(channel + 1u)) &&
                (second & INPUT_CAPTURE_TRIGGER) != 0u &&
                (second & INPUT_CAPTURE_SYNC_MASK) == source) {
                dspic33_device_internal_raw_write_word(
                    cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 2u),
                    (uint16_t)(second | INPUT_CAPTURE_TRIGGER_STATUS));
            }
            channel += 2u;
        } else {
            channel++;
        }
    }
}

void dspic33_device_internal_input_capture_pulse_source(Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    input_capture_trigger_source(cpu, source);
    input_capture_refresh_sync_outputs(cpu);
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t control2 =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
        if (input_capture_operating(cpu, channel) && (control2 & INPUT_CAPTURE_TRIGGER) == 0u &&
            (control2 & INPUT_CAPTURE_SYNC_MASK) == source) {
            cpu->io.input_capture.sync_reset_pending |= (uint16_t)(1u << channel);
        }
    }
}

bool dspic33_device_internal_input_capture_source_awaited(const Dspic33* cpu, uint8_t source) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_INPUT_CAPTURE_COUNT; channel++) {
        uint16_t control2 =
            dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
        if (input_capture_operating(cpu, channel) &&
            (control2 & INPUT_CAPTURE_SYNC_MASK) == source &&
            ((control2 & INPUT_CAPTURE_TRIGGER) == 0u ||
             (control2 & INPUT_CAPTURE_TRIGGER_STATUS) == 0u)) {
            return true;
        }
    }
    return false;
}

static bool input_capture_sync_output_base_high(const Dspic33* cpu, uint8_t channel) {
    uint16_t control1 = dspic33_device_internal_raw_word(cpu, input_capture_base(channel));
    uint16_t control2 =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    if (dspic33_device_internal_input_capture_pmd_disabled(cpu, channel) ||
        (control1 & INPUT_CAPTURE_MODE_MASK) == 0u || (control1 & INPUT_CAPTURE_MODE_MASK) == 6u) {
        return true;
    }
    if ((control2 & INPUT_CAPTURE_TRIGGER) != 0u &&
        (control2 & INPUT_CAPTURE_TRIGGER_STATUS) == 0u) {
        return true;
    }
    return cpu->io.input_capture.timer[channel] == UINT16_MAX;
}

static void input_capture_refresh_sync_outputs(Dspic33* cpu) {
    uint8_t pass;
    for (pass = 0u; pass < 8u; pass++) {
        uint16_t previous = cpu->io.input_capture.sync_output_high & 0x00ffu;
        uint16_t current = 0u;
        uint16_t rising;
        uint8_t channel;
        for (channel = 0u; channel < 8u; channel++) {
            if (input_capture_sync_output_base_high(cpu, channel)) {
                current |= (uint16_t)(1u << channel);
            }
        }
        for (;;) {
            uint16_t expanded = current;
            for (channel = 0u; channel < 8u; channel++) {
                uint16_t control2 = dspic33_device_internal_raw_word(
                    cpu, (uint16_t)(input_capture_base(channel) + 2u));
                uint8_t source = (uint8_t)(control2 & INPUT_CAPTURE_SYNC_MASK);
                if ((control2 & INPUT_CAPTURE_TRIGGER) == 0u &&
                    source >= INPUT_CAPTURE_SYNC_IC_FIRST &&
                    source < INPUT_CAPTURE_SYNC_COMPARATOR_FIRST &&
                    (current & (uint16_t)(1u << (source - INPUT_CAPTURE_SYNC_IC_FIRST))) != 0u) {
                    expanded |= (uint16_t)(1u << channel);
                }
            }
            if (expanded == current) {
                break;
            }
            current = expanded;
        }
        cpu->io.input_capture.sync_output_high = current;
        rising = (uint16_t)(current & ~previous);
        if (rising == 0u) {
            break;
        }
        for (channel = 0u; channel < 8u; channel++) {
            if ((rising & (uint16_t)(1u << channel)) != 0u) {
                input_capture_trigger_source(cpu, (uint8_t)(INPUT_CAPTURE_SYNC_IC_FIRST + channel));
            }
        }
    }
}

static void input_capture_record(Dspic33* cpu, uint8_t channel, uint16_t value) {
    Dspic33InputCaptureFifo* fifo = &cpu->io.input_capture.fifo[channel];
    uint16_t base = input_capture_base(channel);
    uint16_t control = dspic33_device_internal_raw_word(cpu, base);
    uint16_t mode = control & INPUT_CAPTURE_MODE_MASK;
    uint8_t interrupt_interval =
        mode == INPUT_CAPTURE_MODE_EVERY_EDGE
            ? 1u
            : (uint8_t)(((control & INPUT_CAPTURE_INTERRUPT_MASK) >> 5u) + 1u);
    bool interrupt = false;
    if (!input_capture_fifo_push(fifo, value)) {
        if (mode != INPUT_CAPTURE_MODE_EVERY_EDGE &&
            (control & INPUT_CAPTURE_INTERRUPT_MASK) != 0u) {
            dspic33_device_internal_raw_write_word(cpu, base,
                                                   (uint16_t)(control | INPUT_CAPTURE_OVERFLOW));
            return;
        }
        interrupt = true;
    } else {
        cpu->io.input_capture.interrupt_count[channel]++;
        if (cpu->io.input_capture.interrupt_count[channel] == interrupt_interval) {
            cpu->io.input_capture.interrupt_count[channel] = 0u;
            interrupt = true;
        }
        input_capture_refresh(cpu, channel);
    }
    if (interrupt && !input_capture_schedule_interrupt(cpu, channel)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return;
    }
    if (!input_capture_request_dma(cpu, channel)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void input_capture_snapshot(Dspic33* cpu, uint8_t channel, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
    bool paired = (value & INPUT_CAPTURE_EVENT_PAIRED) != 0u;
    if (generation != cpu->io.input_capture.generation[channel] ||
        !input_capture_operating(cpu, channel)) {
        return;
    }
    if (paired) {
        if (!input_capture_pair_enabled(cpu, channel)) {
            return;
        }
        input_capture_record(cpu, channel, cpu->io.input_capture.timer[channel]);
        if (cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR) {
            return;
        }
        input_capture_record(cpu, (uint8_t)(channel + 1u),
                             cpu->io.input_capture.timer[channel + 1u]);
        return;
    }
    input_capture_record(cpu, channel, cpu->io.input_capture.timer[channel]);
}

static bool input_capture_edge_matches(Dspic33* cpu, uint8_t channel, bool was_high, bool high) {
    uint16_t mode = dspic33_device_internal_raw_word(cpu, input_capture_base(channel)) &
                    INPUT_CAPTURE_MODE_MASK;
    if (was_high == high) {
        return false;
    }
    if (mode == INPUT_CAPTURE_MODE_EVERY_EDGE) {
        return true;
    }
    if (mode == INPUT_CAPTURE_MODE_FALLING) {
        return was_high && !high;
    }
    if (mode == INPUT_CAPTURE_MODE_RISING) {
        return !was_high && high;
    }
    if (!was_high && high &&
        (mode == INPUT_CAPTURE_MODE_EVERY_FOURTH_RISING ||
         mode == INPUT_CAPTURE_MODE_EVERY_SIXTEENTH_RISING)) {
        uint8_t interval = mode == INPUT_CAPTURE_MODE_EVERY_FOURTH_RISING ? 4u : 16u;
        cpu->io.input_capture.prescaler_count[channel] =
            (uint8_t)((cpu->io.input_capture.prescaler_count[channel] + 1u) & 0x0fu);
        if ((cpu->io.input_capture.prescaler_count[channel] & (interval - 1u)) == 0u) {
            return true;
        }
    }
    return false;
}

static bool input_capture_interrupt_mode(const Dspic33* cpu, uint8_t channel) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, input_capture_base(channel));
    return !dspic33_device_internal_input_capture_pmd_disabled(cpu, channel) &&
           (control & INPUT_CAPTURE_MODE_MASK) == INPUT_CAPTURE_MODE_INTERRUPT &&
           (cpu->power_state == DSPIC33_POWER_SLEEP || cpu->power_state == DSPIC33_POWER_IDLE);
}

void dspic33_device_internal_input_capture_level(Dspic33* cpu, uint8_t channel, bool high) {
    uint16_t bit = (uint16_t)(1u << channel);
    bool was_high = (cpu->io.input_capture.input_high & bit) != 0u;
    uint32_t value;
    if (high) {
        cpu->io.input_capture.input_high |= bit;
    } else {
        cpu->io.input_capture.input_high &= (uint16_t)~bit;
    }
    if (input_capture_interrupt_mode(cpu, channel)) {
        if (!was_high && high) {
            dspic33_raise_interrupt(cpu, dspic33_device_input_capture_irqs[channel]);
        }
        return;
    }
    if (!input_capture_operating(cpu, channel) ||
        !input_capture_edge_matches(cpu, channel, was_high, high) ||
        ((channel & 1u) != 0u && input_capture_pair_enabled(cpu, (uint8_t)(channel - 1u)))) {
        return;
    }
    value = INPUT_CAPTURE_EVENT_CAPTURE | ((uint32_t)cpu->io.input_capture.generation[channel]
                                           << INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
    if (input_capture_pair_enabled(cpu, channel)) {
        value |= INPUT_CAPTURE_EVENT_PAIRED;
    }
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel, value, 1u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

uint8_t dspic33_device_internal_input_capture_pps_pin(const Dspic33* cpu, uint8_t channel) {
    uint16_t mapping = dspic33_device_internal_raw_word(
        cpu, dspic33_device_input_capture_pps_registers[channel / 2u]);
    return (channel & 1u) == 0u ? (uint8_t)(mapping & 0x007fu)
                                : (uint8_t)((mapping >> 8u) & 0x007fu);
}

const Dspic33PpsPin* dspic33_device_internal_pps_pin(uint8_t pin) {
    size_t first = 0u;
    size_t count = sizeof(dspic33_device_pps_pins) / sizeof(dspic33_device_pps_pins[0]);
    while (count != 0u) {
        size_t step = count / 2u;
        size_t index = first + step;
        if (dspic33_device_pps_pins[index].pin < pin) {
            first = index + 1u;
            count -= step + 1u;
        } else {
            count = step;
        }
    }
    if (first == sizeof(dspic33_device_pps_pins) / sizeof(dspic33_device_pps_pins[0]) ||
        dspic33_device_pps_pins[first].pin != pin) {
        return NULL;
    }
    return &dspic33_device_pps_pins[first];
}

bool dspic33_device_internal_pps_pin_bonded(const Dspic33* cpu, uint8_t pin) {
    const Dspic33PpsPin* mapping = dspic33_device_internal_pps_pin(pin);
    return cpu != NULL && mapping != NULL &&
           (dspic33_device_internal_gpio_port_mask(cpu, mapping->port) &
            (uint16_t)(1u << mapping->bit)) != 0u;
}

static bool pps_output_capable(uint8_t pin) {
    for (size_t index = 0u;
         index < sizeof(dspic33_device_pps_outputs) / sizeof(dspic33_device_pps_outputs[0]);
         index++) {
        if (dspic33_device_pps_outputs[index].pin == pin) {
            return true;
        }
    }
    return false;
}

uint8_t dspic33_device_internal_pps_output_function(const Dspic33* cpu, uint8_t pin) {
    if (!dspic33_device_internal_pps_pin_bonded(cpu, pin)) {
        return 0u;
    }
    for (size_t index = 0u;
         index < sizeof(dspic33_device_pps_outputs) / sizeof(dspic33_device_pps_outputs[0]);
         index++) {
        if (dspic33_device_pps_outputs[index].pin == pin) {
            return (uint8_t)((dspic33_device_internal_raw_word(
                                  cpu, dspic33_device_pps_outputs[index].address) >>
                              dspic33_device_pps_outputs[index].shift) &
                             0x003fu);
        }
    }
    return 0u;
}

bool dspic33_device_internal_pps_physical_input_enabled(const Dspic33* cpu, uint8_t pin) {
    const Dspic33PpsPin* mapping = dspic33_device_internal_pps_pin(pin);
    uint16_t bit;
    if (mapping == NULL) {
        return false;
    }
    bit = (uint16_t)(1u << mapping->bit);
    bool input =
        (dspic33_device_internal_raw_word(cpu, dspic33_device_gpio_tris_addresses[mapping->port]) &
         bit) != 0u;
    bool analog_capable = (dspic33_device_gpio_analog_masks[mapping->port] & bit) != 0u;
    bool analog = dspic33_device_gpio_analog_addresses[mapping->port] != 0u &&
                  (dspic33_device_internal_raw_word(
                       cpu, dspic33_device_gpio_analog_addresses[mapping->port]) &
                   bit) != 0u;
    return input && (!analog_capable || (pps_output_capable(pin) ? !analog : analog));
}

bool dspic33_device_internal_pps_physical_input_high(const Dspic33* cpu, uint8_t pin, bool* high) {
    const Dspic33PpsPin* mapping = dspic33_device_internal_pps_pin(pin);
    uint16_t bit;
    if (mapping == NULL || high == NULL ||
        !dspic33_device_internal_pps_physical_input_enabled(cpu, pin)) {
        return false;
    }
    bit = (uint16_t)(1u << mapping->bit);
    if (!pps_output_capable(pin) && (dspic33_device_gpio_analog_masks[mapping->port] & bit) != 0u) {
        *high = (cpu->io.gpio[mapping->port] & cpu->io.gpio_driven[mapping->port] & bit) != 0u;
    } else {
        *high = (dspic33_device_internal_gpio_pin_values(cpu, mapping->port) & bit) != 0u;
    }
    return true;
}

bool dspic33_device_internal_can_capture_enabled(const Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        if ((dspic33_device_internal_raw_word(cpu, dspic33_device_can_bases[channel]) &
             CAN_CAPTURE) != 0u) {
            return true;
        }
    }
    return false;
}

void dspic33_device_internal_run_input_capture(Dspic33* cpu, uint16_t source, uint32_t value) {
    uint32_t kind = value & INPUT_CAPTURE_EVENT_KIND_MASK;
    if (kind == INPUT_CAPTURE_EVENT_PIN) {
        dspic33_device_internal_apply_physical_pin_level(cpu, (uint8_t)source,
                                                         (value & INPUT_CAPTURE_EVENT_HIGH) != 0u);
        return;
    }
    if (source >= DSPIC33_INPUT_CAPTURE_COUNT) {
        return;
    }
    if (kind == INPUT_CAPTURE_EVENT_PMD) {
        uint16_t generation = (uint16_t)(value >> INPUT_CAPTURE_EVENT_GENERATION_SHIFT);
        uint16_t bit = (uint16_t)(1u << source);
        if (generation != cpu->io.input_capture.pmd_generation[source]) {
            return;
        }
        if ((value & INPUT_CAPTURE_EVENT_PMD_DISABLED) != 0u) {
            cpu->io.input_capture.pmd_disabled |= bit;
            input_capture_pause_events(cpu, (uint8_t)source);
        } else {
            cpu->io.input_capture.pmd_disabled &= (uint16_t)~bit;
            input_capture_resume_events(cpu, (uint8_t)source);
        }
        input_capture_refresh_sync_outputs(cpu);
    } else if (kind == INPUT_CAPTURE_EVENT_INPUT) {
        dspic33_device_internal_input_capture_level(cpu, (uint8_t)source,
                                                    (value & INPUT_CAPTURE_EVENT_HIGH) != 0u);
    } else if (kind == INPUT_CAPTURE_EVENT_CAPTURE) {
        input_capture_snapshot(cpu, (uint8_t)source, value);
    } else if (kind == INPUT_CAPTURE_EVENT_INTERRUPT &&
               (uint16_t)(value >> INPUT_CAPTURE_EVENT_GENERATION_SHIFT) ==
                   cpu->io.input_capture.generation[source] &&
               (input_capture_operating(cpu, (uint8_t)source) ||
                input_capture_interrupt_mode(cpu, (uint8_t)source))) {
        dspic33_raise_interrupt(cpu, dspic33_device_input_capture_irqs[source]);
        if (source < 8u) {
            dspic33_device_internal_output_compare_pulse_source(
                cpu, (uint8_t)(OUTPUT_COMPARE_SYNC_IC_FIRST + source));
        }
    }
}

void dspic33_device_internal_update_input_capture_register(Dspic33* cpu, uint16_t address,
                                                           uint16_t previous) {
    uint16_t base;
    uint16_t offset;
    uint16_t current;
    uint8_t channel;
    if (address < INPUT_CAPTURE_BASE ||
        address >= INPUT_CAPTURE_BASE + DSPIC33_INPUT_CAPTURE_COUNT * INPUT_CAPTURE_STRIDE) {
        return;
    }
    channel = (uint8_t)((address - INPUT_CAPTURE_BASE) / INPUT_CAPTURE_STRIDE);
    base = input_capture_base(channel);
    offset = (uint16_t)(address - base);
    current = dspic33_device_internal_raw_word(cpu, base + offset);
    if (dspic33_device_internal_input_capture_pmd_disabled(cpu, channel)) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + offset), previous);
        return;
    }
    if (offset == 0u) {
        if ((previous & INPUT_CAPTURE_MODE_MASK) != 0u &&
            (current & INPUT_CAPTURE_MODE_MASK) == 0u) {
            input_capture_flush(cpu, channel);
        } else if ((previous & INPUT_CAPTURE_CON1_WRITABLE) !=
                   (current & INPUT_CAPTURE_CON1_WRITABLE)) {
            cpu->io.input_capture.generation[channel]++;
        }
        input_capture_refresh_sync_outputs(cpu);
        return;
    }
    if (offset == 2u &&
        (previous & INPUT_CAPTURE_CON2_WRITABLE) != (current & INPUT_CAPTURE_CON2_WRITABLE)) {
        if ((previous & (INPUT_CAPTURE_CON2_WRITABLE & ~INPUT_CAPTURE_TRIGGER_STATUS)) !=
            (current & (INPUT_CAPTURE_CON2_WRITABLE & ~INPUT_CAPTURE_TRIGGER_STATUS))) {
            cpu->io.input_capture.generation[channel]++;
            cpu->io.input_capture.sync_reset_pending &= (uint16_t)~(uint16_t)(1u << channel);
        }
        if ((current & (INPUT_CAPTURE_TRIGGER | INPUT_CAPTURE_TRIGGER_STATUS)) ==
            INPUT_CAPTURE_TRIGGER) {
            input_capture_reset_timer(cpu, channel);
        }
        input_capture_refresh_sync_outputs(cpu);
    }
}

void dspic33_device_internal_update_input_capture_pmd(Dspic33* cpu, uint16_t address,
                                                      uint16_t previous) {
    uint8_t first_channel;
    uint8_t channel;
    uint16_t changed;
    uint16_t current;
    if (address == 0x0762u) {
        first_channel = 0u;
    } else if (address == 0x0768u) {
        first_channel = 8u;
    } else {
        return;
    }
    current = dspic33_device_internal_raw_word(cpu, address);
    changed = (uint16_t)((previous ^ current) & 0xff00u);
    for (channel = first_channel; channel < first_channel + 8u; channel++) {
        uint16_t register_mask = (uint16_t)(1u << (8u + channel - first_channel));
        if ((changed & register_mask) == 0u) {
            continue;
        }
        cpu->io.input_capture.pmd_generation[channel]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_INPUT_CAPTURE, channel,
                INPUT_CAPTURE_EVENT_PMD |
                    ((current & register_mask) != 0u ? INPUT_CAPTURE_EVENT_PMD_DISABLED : 0u) |
                    ((uint32_t)cpu->io.input_capture.pmd_generation[channel]
                     << INPUT_CAPTURE_EVENT_GENERATION_SHIFT),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            uint8_t invalidate;
            dspic33_device_internal_raw_write_word(cpu, address, previous);
            for (invalidate = first_channel; invalidate < first_channel + 8u; invalidate++) {
                if ((changed & (uint16_t)(1u << (8u + invalidate - first_channel))) != 0u) {
                    cpu->io.input_capture.pmd_generation[invalidate]++;
                }
            }
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return;
        }
    }
}

void dspic33_device_internal_input_capture_read_complete(Dspic33* cpu, uint8_t channel) {
    uint16_t discarded;
    if (!input_capture_fifo_pop(&cpu->io.input_capture.fifo[channel], &discarded)) {
        return;
    }
    if (cpu->io.input_capture.fifo[channel].count == 0u) {
        cpu->io.input_capture.interrupt_count[channel] = 0u;
        dspic33_device_internal_raw_write_word(
            cpu, input_capture_base(channel),
            (uint16_t)(dspic33_device_internal_raw_word(cpu, input_capture_base(channel)) &
                       ~INPUT_CAPTURE_OVERFLOW));
    }
    input_capture_refresh(cpu, channel);
}

static bool input_capture_sync_held(const Dspic33* cpu, uint8_t channel) {
    uint16_t control2 =
        dspic33_device_internal_raw_word(cpu, (uint16_t)(input_capture_base(channel) + 2u));
    uint8_t source = (uint8_t)(control2 & INPUT_CAPTURE_SYNC_MASK);
    return (control2 & INPUT_CAPTURE_TRIGGER) == 0u && source >= INPUT_CAPTURE_SYNC_IC_FIRST &&
           source < INPUT_CAPTURE_SYNC_COMPARATOR_FIRST &&
           (cpu->io.input_capture.sync_output_high &
            (uint16_t)(1u << (source - INPUT_CAPTURE_SYNC_IC_FIRST))) != 0u;
}

static uint64_t input_capture_sync_transition(const Dspic33* cpu, uint8_t channel,
                                              uint16_t timer_source) {
    uint16_t control1 = dspic33_device_internal_raw_word(cpu, input_capture_base(channel));
    if (channel >= 8u || !input_capture_timer_running(cpu, channel) ||
        (control1 & INPUT_CAPTURE_TIMER_SOURCE_MASK) != timer_source ||
        input_capture_sync_held(cpu, channel)) {
        return UINT64_MAX;
    }
    if (input_capture_pair_enabled(cpu, (uint8_t)(channel & 0xfeu))) {
        uint8_t first = (uint8_t)(channel & 0xfeu);
        uint32_t timer = (uint32_t)cpu->io.input_capture.timer[first] |
                         ((uint32_t)cpu->io.input_capture.timer[first + 1u] << 16u);
        if (channel == first) {
            uint16_t low = (uint16_t)timer;
            return low == UINT16_MAX ? 1u : UINT16_MAX - low;
        }
        {
            uint16_t low = (uint16_t)timer;
            uint16_t high = (uint16_t)(timer >> 16u);
            return high == UINT16_MAX ? UINT16_MAX + 1ull - low
                                      : ((uint64_t)(UINT16_MAX - high) << 16u) - low;
        }
    }
    return cpu->io.input_capture.timer[channel] == UINT16_MAX
               ? 1u
               : UINT16_MAX - cpu->io.input_capture.timer[channel];
}

static void input_capture_advance_step(Dspic33* cpu, uint8_t channel, uint64_t cycles) {
    uint16_t bit = (uint16_t)(1u << channel);
    if ((cpu->io.input_capture.sync_reset_pending & bit) != 0u) {
        cpu->io.input_capture.sync_reset_pending &= (uint16_t)~bit;
        input_capture_reset_timer(cpu, channel);
        cycles--;
    }
    if (cycles == 0u || input_capture_sync_held(cpu, channel) ||
        !input_capture_timer_running(cpu, channel)) {
        if (input_capture_sync_held(cpu, channel)) {
            input_capture_reset_timer(cpu, channel);
        }
        return;
    }
    cpu->io.input_capture.timer[channel] =
        (uint16_t)(cpu->io.input_capture.timer[channel] + cycles);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(input_capture_base(channel) + 6u),
                                           cpu->io.input_capture.timer[channel]);
}

static void input_capture_advance_pair(Dspic33* cpu, uint8_t channel, uint64_t cycles) {
    uint16_t bits = (uint16_t)(3u << channel);
    bool reset = (cpu->io.input_capture.sync_reset_pending & bits) != 0u;
    bool held = input_capture_sync_held(cpu, channel) ||
                input_capture_sync_held(cpu, (uint8_t)(channel + 1u));
    uint32_t timer;
    if (reset) {
        cpu->io.input_capture.sync_reset_pending &= (uint16_t)~bits;
        input_capture_reset_timer(cpu, channel);
        input_capture_reset_timer(cpu, (uint8_t)(channel + 1u));
        cycles--;
    }
    if (cycles == 0u || held || !input_capture_pair_timer_running(cpu, channel)) {
        if (held) {
            input_capture_reset_timer(cpu, channel);
            input_capture_reset_timer(cpu, (uint8_t)(channel + 1u));
        }
        return;
    }
    timer = (uint32_t)cpu->io.input_capture.timer[channel] |
            ((uint32_t)cpu->io.input_capture.timer[channel + 1u] << 16u);
    timer += (uint32_t)cycles;
    cpu->io.input_capture.timer[channel] = (uint16_t)timer;
    cpu->io.input_capture.timer[channel + 1u] = (uint16_t)(timer >> 16u);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(input_capture_base(channel) + 6u),
                                           (uint16_t)timer);
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(input_capture_base((uint8_t)(channel + 1u)) + 6u),
        (uint16_t)(timer >> 16u));
}

void dspic33_device_internal_input_capture_advance_clock(Dspic33* cpu, uint16_t timer_source,
                                                         uint64_t cycles) {
    uint64_t remaining = cycles;
    input_capture_refresh_sync_outputs(cpu);
    while (remaining != 0u) {
        uint64_t step = remaining;
        uint8_t source;
        uint8_t channel = 0u;
        for (source = 0u; source < 8u; source++) {
            uint64_t transition = input_capture_sync_transition(cpu, source, timer_source);
            if (transition < step) {
                step = transition;
            }
        }
        if (step == 0u) {
            step = 1u;
        }
        while (channel < DSPIC33_INPUT_CAPTURE_COUNT) {
            bool paired = dspic33_device_internal_input_capture_pair_configured(cpu, channel);
            if (paired && (dspic33_device_internal_raw_word(cpu, input_capture_base(channel)) &
                           INPUT_CAPTURE_TIMER_SOURCE_MASK) == timer_source) {
                input_capture_advance_pair(cpu, channel, step);
                channel += 2u;
                continue;
            }
            if (!paired && (dspic33_device_internal_raw_word(cpu, input_capture_base(channel)) &
                            INPUT_CAPTURE_TIMER_SOURCE_MASK) == timer_source) {
                input_capture_advance_step(cpu, channel, step);
            }
            channel += paired ? 2u : 1u;
        }
        remaining -= step;
        input_capture_refresh_sync_outputs(cpu);
    }
}

void dspic33_device_internal_advance_input_capture(Dspic33* cpu, uint64_t cycles) {
    dspic33_device_internal_input_capture_advance_clock(cpu, INPUT_CAPTURE_TIMER_SOURCE_FP, cycles);
}
