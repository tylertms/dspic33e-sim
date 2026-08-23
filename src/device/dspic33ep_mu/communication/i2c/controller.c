#include <string.h>

#include "device/dspic33ep_mu/communication/i2c/api.h"
#include "device/dspic33ep_mu/communication/i2c/internal.h"
#include "device/dspic33ep_mu/device.h"

const uint16_t dspic33_i2c_bases[DSPIC33_I2C_COUNT] = {0x0200u, 0x0210u};
const uint8_t dspic33_i2c_slave_irqs[DSPIC33_I2C_COUNT] = {16u, 49u};
const uint8_t dspic33_i2c_master_irqs[DSPIC33_I2C_COUNT] = {17u, 50u};
void dspic33_i2c_internal_collide(Dspic33* cpu, uint8_t channel);

uint16_t dspic33_i2c_internal_raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

void dspic33_i2c_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8u);
}

bool dspic33_i2c_internal_channel_for_address(uint16_t address, uint8_t* channel,
                                              uint16_t* offset) {
    uint8_t channel_index;

    for (channel_index = 0u; channel_index < DSPIC33_I2C_COUNT; channel_index++) {
        if (address >= dspic33_i2c_bases[channel_index] &&
            address <= dspic33_i2c_bases[channel_index] + I2C_MSK) {
            *channel = channel_index;
            *offset = (uint16_t)(address - dspic33_i2c_bases[channel_index]);
            return true;
        }
    }
    return false;
}

bool dspic33_i2c_internal_module_disabled(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.i2c_pmd_disabled & (uint8_t)(1u << channel)) != 0u;
}

bool dspic33_i2c_internal_module_enabled(const Dspic33* cpu, uint8_t channel) {
    return !dspic33_i2c_internal_module_disabled(cpu, channel) &&
           (dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON)) &
            I2C_ENABLE) != 0u;
}

bool dspic33_i2c_internal_pin_mapping(const Dspic33* cpu, uint8_t channel,
                                      Dspic33I2cPinMapping* mapping) {
    uint8_t selection;
    if (channel >= DSPIC33_I2C_COUNT || mapping == NULL) {
        return false;
    }
    selection = (uint8_t)(cpu->configuration[12u] & (uint8_t)(0x10u << channel));
    if (channel == 0u) {
        if (selection != 0u) {
            return false;
        }
        mapping->port = 3u;
        mapping->clock = 10u;
        mapping->data = 9u;
    } else if (selection != 0u) {
        mapping->port = 5u;
        mapping->clock = 5u;
        mapping->data = 4u;
    } else {
        mapping->port = 0u;
        mapping->clock = 2u;
        mapping->data = 3u;
    }
    return true;
}

static Dspic33Event* find_scheduled_event(Dspic33* cpu, uint64_t sequence) {
    size_t event_index;

    for (event_index = 0u; event_index < cpu->events.count; event_index++) {
        if (cpu->events.items[event_index].sequence == sequence) {
            return &cpu->events.items[event_index];
        }
    }
    return NULL;
}

void dspic33_i2c_internal_pause_events(Dspic33* cpu, uint8_t channel) {
    size_t event_index;
    bool events_changed = false;

    for (event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];
        uint8_t event_kind = (uint8_t)(event->value >> I2C_EVENT_KIND_SHIFT);

        if (event->type != DSPIC33_EVENT_I2C || event->source != channel || event->paused ||
            (event_kind > I2C_EVENT_TRANSMIT_SHIFT && event_kind != I2C_EVENT_PIN)) {
            continue;
        }
        event->paused_remaining = event->cycle - cpu->device_cycles;
        event->paused = true;
        events_changed = true;
    }

    if (events_changed) {
        dspic33_reorder_events(cpu);
    }
}

void dspic33_i2c_internal_resume_events(Dspic33* cpu, uint8_t channel) {
    size_t event_index;
    bool events_changed = false;

    for (event_index = 0u; event_index < cpu->events.count; event_index++) {
        Dspic33Event* event = &cpu->events.items[event_index];

        if (event->type != DSPIC33_EVENT_I2C || event->source != channel || !event->paused) {
            continue;
        }
        if (event->paused_remaining > UINT64_MAX - cpu->device_cycles) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            continue;
        }
        event->cycle = cpu->device_cycles + event->paused_remaining;
        event->paused_remaining = 0u;
        event->paused = false;
        events_changed = true;
    }

    if (events_changed) {
        dspic33_reorder_events(cpu);
    }
}

void dspic33_i2c_update_pmd(Dspic33* cpu, uint16_t address, uint16_t previous_word) {
    static const uint16_t addresses[DSPIC33_I2C_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t masks[DSPIC33_I2C_COUNT] = {0x0080u, 0x0002u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        bool disabled;
        if (address != addresses[channel]) {
            continue;
        }
        disabled = (dspic33_i2c_internal_raw_word(cpu, address) & masks[channel]) != 0u;
        if (((previous_word & masks[channel]) != 0u) == disabled) {
            return;
        }
        cpu->io.i2c_pmd_generation[channel]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_I2C, channel,
                ((uint32_t)I2C_EVENT_PMD << I2C_EVENT_KIND_SHIFT) |
                    ((uint32_t)cpu->io.i2c_pmd_generation[channel] << I2C_EVENT_GENERATION_SHIFT) |
                    (disabled ? 1u : 0u),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            dspic33_i2c_internal_raw_write_word(cpu, address, previous_word);
            cpu->io.i2c_pmd_generation[channel]++;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        return;
    }
}

static bool enqueue_transfer(Dspic33I2cQueue* queue, const Dspic33I2cTransfer* transfer) {
    uint8_t transfer_index;

    if (queue->count == DSPIC33_I2C_QUEUE_SIZE) {
        return false;
    }
    transfer_index = (uint8_t)((queue->head + queue->count) % DSPIC33_I2C_QUEUE_SIZE);
    queue->transfers[transfer_index] = *transfer;
    queue->count++;
    return true;
}

bool dspic33_i2c_internal_transfer_pop(Dspic33I2cQueue* queue, Dspic33I2cTransfer* transfer) {
    if (queue->count == 0u) {
        return false;
    }
    *transfer = queue->transfers[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % DSPIC33_I2C_QUEUE_SIZE);
    queue->count--;
    return true;
}

bool dspic33_i2c_internal_response_push(Dspic33I2cResponseQueue* queue,
                                        const Dspic33I2cResponse* response) {
    uint8_t response_index;

    if (queue->count == DSPIC33_I2C_QUEUE_SIZE) {
        return false;
    }
    response_index = (uint8_t)((queue->head + queue->count) % DSPIC33_I2C_QUEUE_SIZE);
    queue->responses[response_index] = *response;
    queue->count++;
    response_index = (uint8_t)(queue->count - 1u);

    while (response_index != 0u) {
        uint8_t current_index = (uint8_t)((queue->head + response_index) % DSPIC33_I2C_QUEUE_SIZE);
        uint8_t previous_index =
            (uint8_t)((queue->head + response_index - 1u) % DSPIC33_I2C_QUEUE_SIZE);
        Dspic33I2cResponse response_swap;

        if (queue->responses[previous_index].cycle <= queue->responses[current_index].cycle) {
            break;
        }
        response_swap = queue->responses[previous_index];
        queue->responses[previous_index] = queue->responses[current_index];
        queue->responses[current_index] = response_swap;
        response_index--;
    }
    return true;
}

static bool response_wait(const Dspic33I2cResponseQueue* queue, uint64_t cycle, uint64_t* delay) {
    if (queue->count == 0u || queue->responses[queue->head].cycle <= cycle) {
        return false;
    }
    *delay = queue->responses[queue->head].cycle - cycle;
    return true;
}

static bool response_pop(Dspic33I2cResponseQueue* queue, uint64_t cycle,
                         Dspic33I2cResponse* response) {
    if (queue->count == 0u || queue->responses[queue->head].cycle > cycle) {
        return false;
    }
    *response = queue->responses[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % DSPIC33_I2C_QUEUE_SIZE);
    queue->count--;
    return true;
}

void dspic33_i2c_internal_record_transfer(Dspic33* cpu, uint8_t channel,
                                          Dspic33I2cTransferType type, uint16_t transfer_value,
                                          bool acknowledge, bool master) {
    Dspic33I2cTransfer transfer;

    transfer.type = type;
    transfer.value = transfer_value;
    transfer.acknowledge = acknowledge;
    transfer.master = master;
    if (!enqueue_transfer(&cpu->io.i2c_tx[channel], &transfer)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

bool dspic33_i2c_internal_slave_acknowledges(uint16_t status) {
    return (status & (I2C_RBF | I2C_OVERFLOW)) == 0u;
}

void dspic33_i2c_internal_record_slave_acknowledgement(Dspic33* cpu, uint8_t channel,
                                                       bool acknowledge) {
    dspic33_i2c_internal_record_transfer(cpu, channel, DSPIC33_I2C_ACKNOWLEDGE, 0u, acknowledge,
                                         false);
}

static uint64_t operation_cycles(const Dspic33* cpu, uint8_t channel, uint8_t half_period_count) {
    uint64_t scaled = (uint64_t)(dspic33_i2c_internal_raw_word(
                                     cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_BRG)) +
                                 2u) *
                      half_period_count;
    return (scaled + 1u) / 2u;
}

static uint32_t build_internal_event_value(const Dspic33* cpu, uint8_t channel, uint8_t event_kind,
                                           uint16_t payload) {
    return ((uint32_t)event_kind << I2C_EVENT_KIND_SHIFT) |
           ((uint32_t)cpu->io.i2c_generation[channel] << I2C_EVENT_GENERATION_SHIFT) | payload;
}

bool dspic33_i2c_internal_schedule_event(Dspic33* cpu, uint8_t channel, uint32_t value,
                                         uint64_t delay, bool external) {
    uint64_t sequence = cpu->events.sequence;
    Dspic33Event* event;

    bool event_scheduled =
        external ? dspic33_schedule_external(cpu, DSPIC33_EVENT_I2C, channel, value, delay)
                 : dspic33_schedule(cpu, DSPIC33_EVENT_I2C, channel, value, delay);
    if (!event_scheduled) {
        return false;
    }
    event = find_scheduled_event(cpu, sequence);
    if (event == NULL) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    event->paused_remaining = 0u;
    event->paused = false;
    if (dspic33_i2c_internal_module_disabled(cpu, channel) &&
        (uint8_t)(value >> I2C_EVENT_KIND_SHIFT) <= I2C_EVENT_TRANSMIT_SHIFT) {
        event->paused_remaining = delay;
        event->paused = true;
        dspic33_reorder_events(cpu);
    }
    return true;
}

static bool schedule_internal_event(Dspic33* cpu, uint8_t channel, uint8_t event_kind,
                                    uint16_t payload, uint64_t delay) {
    bool event_scheduled = dspic33_i2c_internal_schedule_event(
        cpu, channel, build_internal_event_value(cpu, channel, event_kind, payload), delay, false);

    if (!event_scheduled) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    return event_scheduled;
}

bool dspic33_i2c_internal_schedule_external_event(Dspic33* cpu, uint8_t channel, uint8_t event_kind,
                                                  uint16_t payload, uint64_t delay) {
    return dspic33_i2c_internal_schedule_event(
        cpu, channel, ((uint32_t)event_kind << I2C_EVENT_KIND_SHIFT) | payload, delay, true);
}

void dspic33_i2c_internal_raise_master(Dspic33* cpu, uint8_t channel) {
    dspic33_raise_interrupt(cpu, dspic33_i2c_master_irqs[channel]);
}

void dspic33_i2c_internal_raise_slave(Dspic33* cpu, uint8_t channel) {
    dspic33_raise_interrupt(cpu, dspic33_i2c_slave_irqs[channel]);
}

void dspic33_i2c_internal_reset_runtime(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.i2c_generation[channel]++;
    cpu->io.i2c_master_active &= (uint8_t)~bit;
    cpu->io.i2c_slave_active &= (uint8_t)~bit;
    cpu->io.i2c_slave_read &= (uint8_t)~bit;
    cpu->io.i2c_slave_rejected &= (uint8_t)~bit;
    cpu->io.i2c_pin_active &= (uint8_t)~bit;
    cpu->io.i2c_pin_physical &= (uint8_t)~bit;
    cpu->io.i2c_pin_clock_low &= (uint8_t)~bit;
    cpu->io.i2c_pin_data_low &= (uint8_t)~bit;
    cpu->io.i2c_pin_operation[channel] = 0u;
    cpu->io.i2c_pin_phase[channel] = 0u;
    cpu->io.i2c_pin_receive[channel] = 0u;
    cpu->io.i2c_slave_pin_active &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_acknowledge &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_interrupt &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_stretch &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_state[channel] = I2C_SLAVE_PIN_IDLE;
    cpu->io.i2c_slave_pin_next[channel] = I2C_SLAVE_PIN_IDLE;
    cpu->io.i2c_slave_pin_bits[channel] = 0u;
    cpu->io.i2c_slave_pin_shift[channel] = 0u;
    cpu->io.i2c_slave_address[channel] = 0u;
    memset(&cpu->io.i2c_response[channel], 0, sizeof(cpu->io.i2c_response[channel]));
}

bool dspic33_i2c_internal_pin_input_high(const Dspic33* cpu, const Dspic33I2cPinMapping* mapping,
                                         bool clock) {
    bool high = false;
    dspic33_device_gpio_input_high(cpu, mapping->port, clock ? mapping->clock : mapping->data,
                                   &high);
    return high;
}

void dspic33_i2c_internal_pin_set_low(Dspic33* cpu, uint8_t channel, bool clock, bool low) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t* state = clock ? &cpu->io.i2c_pin_clock_low : &cpu->io.i2c_pin_data_low;
    if (low) {
        *state |= bit;
    } else {
        *state &= (uint8_t)~bit;
    }
}

static bool pin_schedule_next(Dspic33* cpu, uint8_t channel, uint8_t phase) {
    uint64_t previous_cycle = operation_cycles(cpu, channel, phase);
    uint64_t next_cycle = operation_cycles(cpu, channel, (uint8_t)(phase + 1u));

    return schedule_internal_event(cpu, channel, I2C_EVENT_PIN, 0u, next_cycle - previous_cycle);
}

void dspic33_i2c_internal_pin_abort(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.i2c_pin_active &= (uint8_t)~bit;
    cpu->io.i2c_pin_physical &= (uint8_t)~bit;
    cpu->io.i2c_pin_clock_low &= (uint8_t)~bit;
    cpu->io.i2c_pin_data_low &= (uint8_t)~bit;
    cpu->io.i2c_pin_operation[channel] = 0u;
    cpu->io.i2c_pin_phase[channel] = 0u;
}

static bool pin_begin(Dspic33* cpu, uint8_t channel, uint8_t operation, uint8_t value) {
    Dspic33I2cPinMapping mapping;
    uint8_t bit = (uint8_t)(1u << channel);
    uint16_t pins;
    if (!dspic33_i2c_internal_pin_mapping(cpu, channel, &mapping)) {
        return true;
    }
    pins = (uint16_t)((1u << mapping.clock) | (1u << mapping.data));
    if ((cpu->io.gpio_driven[mapping.port] & pins) != pins) {
        return true;
    }
    cpu->io.i2c_pin_active |= bit;
    cpu->io.i2c_pin_physical |= bit;
    cpu->io.i2c_pin_operation[channel] = operation;
    cpu->io.i2c_pin_phase[channel] = 0u;
    cpu->io.i2c_pin_receive[channel] = 0u;
    dspic33_i2c_internal_pin_set_low(cpu, channel, true,
                                     operation == I2C_PIN_RESTART || operation == I2C_PIN_STOP ||
                                         operation == I2C_PIN_TRANSMIT ||
                                         operation == I2C_PIN_RECEIVE ||
                                         operation == I2C_PIN_ACKNOWLEDGE);
    dspic33_i2c_internal_pin_set_low(
        cpu, channel, false,
        operation == I2C_PIN_STOP || (operation == I2C_PIN_TRANSMIT && (value & 0x80u) == 0u) ||
            (operation == I2C_PIN_ACKNOWLEDGE &&
             (dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON)) &
              I2C_ACKDT) == 0u));
    if (!pin_schedule_next(cpu, channel, 0u)) {
        dspic33_i2c_internal_pin_abort(cpu, channel);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    return true;
}

static bool pin_delay_master_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        uint8_t kind = (uint8_t)(event->value >> I2C_EVENT_KIND_SHIFT);
        uint8_t generation = (uint8_t)(event->value >> I2C_EVENT_GENERATION_SHIFT);
        if (event->type != DSPIC33_EVENT_I2C || event->source != channel ||
            kind > I2C_EVENT_TRANSMIT_SHIFT || generation != cpu->io.i2c_generation[channel]) {
            continue;
        }
        if (event->paused) {
            event->paused_remaining++;
        } else if (event->cycle == UINT64_MAX) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
            return false;
        } else {
            event->cycle++;
        }
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
    return true;
}

static bool pin_rising_edge(Dspic33* cpu, uint8_t channel, const Dspic33I2cPinMapping* mapping) {
    dspic33_i2c_internal_pin_set_low(cpu, channel, true, false);
    if (dspic33_i2c_internal_pin_input_high(cpu, mapping, true)) {
        return true;
    }
    if (pin_delay_master_events(cpu, channel) &&
        schedule_internal_event(cpu, channel, I2C_EVENT_PIN, 0u, 1u)) {
        return false;
    }
    dspic33_i2c_internal_pin_abort(cpu, channel);
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    return false;
}

static void pin_record_response(Dspic33* cpu, uint8_t channel, uint8_t value, bool acknowledge) {
    Dspic33I2cResponse response;
    response.cycle = cpu->device_cycles;
    response.value = value;
    response.acknowledge = acknowledge;
    if (!dspic33_i2c_internal_response_push(&cpu->io.i2c_response[channel], &response)) {
        dspic33_i2c_internal_pin_abort(cpu, channel);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_i2c_internal_pin_run(Dspic33* cpu, uint8_t channel) {
    Dspic33I2cPinMapping mapping;
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t operation = cpu->io.i2c_pin_operation[channel];
    uint8_t phase = (uint8_t)(cpu->io.i2c_pin_phase[channel] + 1u);
    uint8_t final_phase;
    bool data_high;
    if ((cpu->io.i2c_pin_active & bit) == 0u ||
        !dspic33_i2c_internal_pin_mapping(cpu, channel, &mapping)) {
        return;
    }
    if (operation == I2C_PIN_START) {
        if (phase == 1u) {
            if (!dspic33_i2c_internal_pin_input_high(cpu, &mapping, true) ||
                !dspic33_i2c_internal_pin_input_high(cpu, &mapping, false)) {
                dspic33_i2c_internal_collide(cpu, channel);
                return;
            }
            dspic33_i2c_internal_pin_set_low(cpu, channel, false, true);
        } else {
            dspic33_i2c_internal_pin_set_low(cpu, channel, true, true);
        }
        final_phase = 2u;
    } else if (operation == I2C_PIN_RESTART) {
        if (phase == 1u && !pin_rising_edge(cpu, channel, &mapping)) {
            return;
        }
        if (phase == 1u && !dspic33_i2c_internal_pin_input_high(cpu, &mapping, false)) {
            dspic33_i2c_internal_collide(cpu, channel);
            return;
        }
        if (phase == 2u) {
            dspic33_i2c_internal_pin_set_low(cpu, channel, false, true);
        } else if (phase == 3u) {
            dspic33_i2c_internal_pin_set_low(cpu, channel, true, true);
        }
        final_phase = 3u;
    } else if (operation == I2C_PIN_STOP) {
        if (phase == 1u && !pin_rising_edge(cpu, channel, &mapping)) {
            return;
        }
        if (phase == 2u) {
            dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
            dspic33_i2c_refresh_pins(cpu);
            if (!dspic33_i2c_internal_pin_input_high(cpu, &mapping, false)) {
                dspic33_i2c_internal_collide(cpu, channel);
                return;
            }
        }
        final_phase = 3u;
    } else if (operation == I2C_PIN_TRANSMIT) {
        uint8_t value = (uint8_t)dspic33_i2c_internal_raw_word(
            cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_TRN));
        if ((phase & 1u) != 0u) {
            if (!pin_rising_edge(cpu, channel, &mapping)) {
                return;
            }
            data_high = dspic33_i2c_internal_pin_input_high(cpu, &mapping, false);
            if (phase < 17u && (cpu->io.i2c_pin_data_low & bit) == 0u && !data_high) {
                dspic33_i2c_internal_collide(cpu, channel);
                return;
            }
            if (phase == 17u) {
                pin_record_response(cpu, channel, 0u, !data_high);
            }
        } else {
            dspic33_i2c_internal_pin_set_low(cpu, channel, true, true);
            if (phase < 16u) {
                uint8_t data_bit = (uint8_t)(7u - phase / 2u);
                dspic33_i2c_internal_pin_set_low(cpu, channel, false,
                                                 (value & (uint8_t)(1u << data_bit)) == 0u);
            } else {
                dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
            }
        }
        final_phase = 18u;
    } else if (operation == I2C_PIN_RECEIVE) {
        if ((phase & 1u) != 0u) {
            if (!pin_rising_edge(cpu, channel, &mapping)) {
                return;
            }
            cpu->io.i2c_pin_receive[channel] =
                (uint8_t)((cpu->io.i2c_pin_receive[channel] << 1u) |
                          (dspic33_i2c_internal_pin_input_high(cpu, &mapping, false) ? 1u : 0u));
            if (phase == 15u) {
                pin_record_response(cpu, channel, cpu->io.i2c_pin_receive[channel], true);
            }
        } else {
            dspic33_i2c_internal_pin_set_low(cpu, channel, true, true);
        }
        final_phase = 16u;
    } else {
        if (phase == 1u && !pin_rising_edge(cpu, channel, &mapping)) {
            return;
        }
        if (phase == 1u &&
            (dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON)) &
             I2C_ACKDT) != 0u &&
            !dspic33_i2c_internal_pin_input_high(cpu, &mapping, false)) {
            dspic33_i2c_internal_collide(cpu, channel);
            return;
        }
        if (phase == 2u) {
            dspic33_i2c_internal_pin_set_low(cpu, channel, true, true);
            dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
        }
        final_phase = 2u;
    }
    cpu->io.i2c_pin_phase[channel] = phase;
    dspic33_i2c_refresh_pins(cpu);
    if (phase == final_phase) {
        cpu->io.i2c_pin_active &= (uint8_t)~bit;
        cpu->io.i2c_pin_operation[channel] = 0u;
        return;
    }
    if (!pin_schedule_next(cpu, channel, phase)) {
        dspic33_i2c_internal_pin_abort(cpu, channel);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

void dspic33_i2c_internal_begin_control(Dspic33* cpu, uint8_t channel, uint16_t operation) {
    uint16_t control =
        dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON));
    uint16_t selected = 0u;
    uint16_t bit;
    for (bit = 1u; bit <= I2C_ACKEN; bit <<= 1u) {
        if ((operation & bit) != 0u) {
            selected = bit;
            break;
        }
    }
    control = (uint16_t)((control & ~I2C_MASTER_MASK) | selected);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON),
                                        control);
    if (selected != 0u) {
        uint8_t periods = 2u;
        bool condition = selected == I2C_SEN || selected == I2C_RSEN || selected == I2C_PEN;
        if (selected == I2C_RCEN) {
            periods = 16u;
        } else if (selected == I2C_RSEN || selected == I2C_PEN) {
            periods = 3u;
        }
        if (!schedule_internal_event(cpu, channel, I2C_EVENT_CONTROL, selected,
                                     operation_cycles(cpu, channel, periods))) {
            dspic33_i2c_internal_raw_write_word(cpu,
                                                (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON),
                                                (uint16_t)(control & ~selected));
            return;
        }
        if (condition) {
            uint8_t status_periods = selected == I2C_SEN ? 1u : 2u;
            if (!schedule_internal_event(cpu, channel, I2C_EVENT_BUS_STATUS, selected,
                                         operation_cycles(cpu, channel, status_periods))) {
                cpu->io.i2c_generation[channel]++;
                dspic33_i2c_internal_raw_write_word(
                    cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON),
                    (uint16_t)(control & ~selected));
                return;
            }
        }
        {
            bool pin_started = true;
            if (selected == I2C_SEN) {
                pin_started = pin_begin(cpu, channel, I2C_PIN_START, 0u);
            } else if (selected == I2C_RSEN) {
                pin_started = pin_begin(cpu, channel, I2C_PIN_RESTART, 0u);
            } else if (selected == I2C_PEN) {
                pin_started = pin_begin(cpu, channel, I2C_PIN_STOP, 0u);
            } else if (selected == I2C_RCEN) {
                pin_started = pin_begin(cpu, channel, I2C_PIN_RECEIVE, 0u);
            } else if (selected == I2C_ACKEN) {
                pin_started = pin_begin(cpu, channel, I2C_PIN_ACKNOWLEDGE, 0u);
            }
            if (!pin_started) {
                cpu->io.i2c_generation[channel]++;
                dspic33_i2c_internal_raw_write_word(
                    cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON),
                    (uint16_t)(control & ~selected));
            }
        }
    }
}

void dspic33_i2c_internal_complete_bus_status(Dspic33* cpu, uint8_t channel, uint16_t operation) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    if (!dspic33_i2c_internal_module_enabled(cpu, channel) || (control & operation) == 0u) {
        return;
    }
    if (operation == I2C_SEN || operation == I2C_RSEN) {
        status = (uint16_t)((status | I2C_START_STATUS) & ~I2C_STOP_STATUS);
    } else if (operation == I2C_PEN) {
        status = (uint16_t)((status | I2C_STOP_STATUS) & ~I2C_START_STATUS);
    }
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
}

void dspic33_i2c_internal_write_transmit(Dspic33* cpu, uint8_t channel, uint16_t previous,
                                         uint8_t value) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!dspic33_i2c_internal_module_enabled(cpu, channel)) {
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        return;
    }
    if ((control & I2C_MASTER_MASK) != 0u || (status & (I2C_TBF | I2C_TRANSMIT_ACTIVE)) != 0u) {
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT),
                                            (uint16_t)(status | I2C_WRITE_COLLISION));
        return;
    }
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_TRN), value);
    status |= I2C_TBF;
    if ((cpu->io.i2c_slave_active & bit) != 0u && (cpu->io.i2c_slave_read & bit) != 0u) {
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
        return;
    }
    cpu->io.i2c_master_active |= bit;
    status |= I2C_TRANSMIT_ACTIVE;
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    if (!schedule_internal_event(cpu, channel, I2C_EVENT_TRANSMIT_SHIFT, value,
                                 operation_cycles(cpu, channel, 16u)) ||
        !schedule_internal_event(cpu, channel, I2C_EVENT_TRANSMIT, value,
                                 operation_cycles(cpu, channel, 18u)) ||
        !pin_begin(cpu, channel, I2C_PIN_TRANSMIT, value)) {
        cpu->io.i2c_generation[channel]++;
        cpu->io.i2c_master_active &= (uint8_t)~bit;
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT),
                                            (uint16_t)(status & ~(I2C_TBF | I2C_TRANSMIT_ACTIVE)));
        return;
    }
    dspic33_i2c_internal_record_transfer(cpu, channel, DSPIC33_I2C_WRITE, value, false, true);
}

void dspic33_i2c_internal_complete_control(Dspic33* cpu, uint8_t channel, uint16_t operation) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!dspic33_i2c_internal_module_enabled(cpu, channel) || (control & operation) == 0u) {
        return;
    }
    if (operation == I2C_ACKEN && dspic33_cpu_rmw_matches(cpu, base + I2C_CON, 2u)) {
        cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
        return;
    }
    if (operation == I2C_RCEN) {
        uint64_t delay;
        if (response_wait(&cpu->io.i2c_response[channel], cpu->device_cycles, &delay)) {
            schedule_internal_event(cpu, channel, I2C_EVENT_CONTROL, operation, delay);
            return;
        }
    }
    control &= (uint16_t)~operation;
    if (operation == I2C_SEN || operation == I2C_RSEN) {
        status = (uint16_t)((status | I2C_START_STATUS) & ~I2C_STOP_STATUS);
        cpu->io.i2c_master_active |= bit;
        dspic33_i2c_internal_record_transfer(
            cpu, channel, operation == I2C_SEN ? DSPIC33_I2C_START : DSPIC33_I2C_RESTART, 0u, false,
            true);
    } else if (operation == I2C_PEN) {
        status = (uint16_t)((status | I2C_STOP_STATUS) & ~I2C_START_STATUS);
        cpu->io.i2c_master_active &= (uint8_t)~bit;
        dspic33_i2c_internal_record_transfer(cpu, channel, DSPIC33_I2C_STOP, 0u, false, true);
    } else if (operation == I2C_RCEN) {
        Dspic33I2cResponse response;
        uint8_t received = 0xffu;
        if (response_pop(&cpu->io.i2c_response[channel], cpu->device_cycles, &response)) {
            received = response.value;
        }
        if ((status & I2C_RBF) != 0u) {
            status |= I2C_OVERFLOW;
        } else {
            dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_RCV), received);
            status |= I2C_RBF;
        }
        dspic33_i2c_internal_record_transfer(cpu, channel, DSPIC33_I2C_READ, received, true, true);
    } else if (operation == I2C_ACKEN) {
        dspic33_i2c_internal_record_transfer(cpu, channel, DSPIC33_I2C_ACKNOWLEDGE, 0u,
                                             (control & I2C_ACKDT) == 0u, true);
    }
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_raise_master(cpu, channel);
}

void dspic33_i2c_internal_complete_transmit(Dspic33* cpu, uint8_t channel) {
    Dspic33I2cResponse response;
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    bool acknowledge = false;
    if (!dspic33_i2c_internal_module_enabled(cpu, channel) ||
        (status & I2C_TRANSMIT_ACTIVE) == 0u) {
        return;
    }
    {
        uint64_t delay;
        if (response_wait(&cpu->io.i2c_response[channel], cpu->device_cycles, &delay)) {
            schedule_internal_event(cpu, channel, I2C_EVENT_TRANSMIT, 0u, delay);
            return;
        }
    }
    if (response_pop(&cpu->io.i2c_response[channel], cpu->device_cycles, &response)) {
        acknowledge = response.acknowledge;
    }
    status &= (uint16_t)~(I2C_TBF | I2C_TRANSMIT_ACTIVE | I2C_NOT_ACKNOWLEDGED);
    if (!acknowledge) {
        status |= I2C_NOT_ACKNOWLEDGED;
    }
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_raise_master(cpu, channel);
}

void dspic33_i2c_internal_complete_transmit_shift(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    if (dspic33_i2c_internal_module_enabled(cpu, channel) && (status & I2C_TRANSMIT_ACTIVE) != 0u) {
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT),
                                            (uint16_t)(status & ~I2C_TBF));
    }
}
