#include "i2c.h"

#include <string.h>

#include "device.h"

static const uint16_t bases[DSPIC33_I2C_COUNT] = {0x0200u, 0x0210u};
static const uint8_t slave_irqs[DSPIC33_I2C_COUNT] = {16u, 49u};
static const uint8_t master_irqs[DSPIC33_I2C_COUNT] = {17u, 50u};
static void collide(Dspic33* cpu, uint8_t channel);

typedef struct {
    uint8_t port;
    uint8_t clock;
    uint8_t data;
} Dspic33I2cPinMapping;

enum {
    I2C_RCV = 0u,
    I2C_TRN = 2u,
    I2C_BRG = 4u,
    I2C_CON = 6u,
    I2C_STAT = 8u,
    I2C_ADD = 10u,
    I2C_MSK = 12u,
    I2C_SEN = 0x0001u,
    I2C_RSEN = 0x0002u,
    I2C_PEN = 0x0004u,
    I2C_RCEN = 0x0008u,
    I2C_ACKEN = 0x0010u,
    I2C_ACKDT = 0x0020u,
    I2C_STREN = 0x0040u,
    I2C_GCEN = 0x0080u,
    I2C_A10M = 0x0400u,
    I2C_IPMIEN = 0x0800u,
    I2C_SCLREL = 0x1000u,
    I2C_ENABLE = 0x8000u,
    I2C_MASTER_MASK = 0x001fu,
    I2C_TBF = 0x0001u,
    I2C_RBF = 0x0002u,
    I2C_READ = 0x0004u,
    I2C_START_STATUS = 0x0008u,
    I2C_STOP_STATUS = 0x0010u,
    I2C_DATA = 0x0020u,
    I2C_OVERFLOW = 0x0040u,
    I2C_WRITE_COLLISION = 0x0080u,
    I2C_TEN_BIT = 0x0100u,
    I2C_GENERAL_CALL = 0x0200u,
    I2C_BUS_COLLISION = 0x0400u,
    I2C_TRANSMIT_ACTIVE = 0x4000u,
    I2C_NOT_ACKNOWLEDGED = 0x8000u,
    I2C_EVENT_KIND_SHIFT = 24u,
    I2C_EVENT_GENERATION_SHIFT = 16u,
    I2C_EVENT_PAYLOAD_MASK = 0xffffu,
    I2C_EVENT_CONTROL = 1u,
    I2C_EVENT_BUS_STATUS = 2u,
    I2C_EVENT_TRANSMIT = 3u,
    I2C_EVENT_TRANSMIT_SHIFT = 4u,
    I2C_EVENT_SLAVE_START = 5u,
    I2C_EVENT_SLAVE_WRITE = 6u,
    I2C_EVENT_SLAVE_READ = 7u,
    I2C_EVENT_SLAVE_STOP = 8u,
    I2C_EVENT_COLLISION = 9u,
    I2C_EVENT_SLAVE_TEN_SECOND = 10u,
    I2C_EVENT_SLAVE_TEN_RESTART = 11u,
    I2C_EVENT_PMD = 12u,
    I2C_EVENT_PIN = 13u,
    I2C_PIN_START = 1u,
    I2C_PIN_RESTART = 2u,
    I2C_PIN_STOP = 3u,
    I2C_PIN_TRANSMIT = 4u,
    I2C_PIN_RECEIVE = 5u,
    I2C_PIN_ACKNOWLEDGE = 6u,
    I2C_SLAVE_PIN_IDLE = 0u,
    I2C_SLAVE_PIN_ADDRESS = 1u,
    I2C_SLAVE_PIN_RECEIVE = 2u,
    I2C_SLAVE_PIN_ACKNOWLEDGE = 3u,
    I2C_SLAVE_PIN_TRANSMIT = 4u,
    I2C_SLAVE_PIN_MASTER_ACKNOWLEDGE = 5u,
    I2C_SLAVE_PIN_REJECTED = 6u,
    I2C_SLAVE_PIN_TEN_SECOND = 7u,
    I2C_SLAVE_PIN_RECEIVED = 8u,
    I2C_EXTERNAL_READ = 0x00000800u,
    I2C_EXTERNAL_TEN_BIT = 0x00000400u
};

static uint16_t raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

static void raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[(uint16_t)(address + 1u)] = (uint8_t)(value >> 8u);
}

static bool channel_for_address(uint16_t address, uint8_t* channel, uint16_t* offset) {
    uint8_t index;
    for (index = 0u; index < DSPIC33_I2C_COUNT; index++) {
        if (address >= bases[index] && address <= bases[index] + I2C_MSK) {
            *channel = index;
            *offset = (uint16_t)(address - bases[index]);
            return true;
        }
    }
    return false;
}

static bool module_disabled(const Dspic33* cpu, uint8_t channel) {
    return (cpu->io.i2c_pmd_disabled & (uint8_t)(1u << channel)) != 0u;
}

static bool module_enabled(const Dspic33* cpu, uint8_t channel) {
    return !module_disabled(cpu, channel) &&
           (raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON)) & I2C_ENABLE) != 0u;
}

static bool pin_mapping(const Dspic33* cpu, uint8_t channel, Dspic33I2cPinMapping* mapping) {
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

static Dspic33Event* scheduled_event(Dspic33* cpu, uint64_t sequence) {
    size_t index;
    for (index = 0u; index < cpu->events.count; index++) {
        if (cpu->events.items[index].sequence == sequence) {
            return &cpu->events.items[index];
        }
    }
    return NULL;
}

static void pause_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
        uint8_t kind = (uint8_t)(event->value >> I2C_EVENT_KIND_SHIFT);
        if (event->type != DSPIC33_EVENT_I2C || event->source != channel || event->paused ||
            (kind > I2C_EVENT_TRANSMIT_SHIFT && kind != I2C_EVENT_PIN)) {
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

static void resume_events(Dspic33* cpu, uint8_t channel) {
    size_t index;
    bool changed = false;
    for (index = 0u; index < cpu->events.count; index++) {
        Dspic33Event* event = &cpu->events.items[index];
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
        changed = true;
    }
    if (changed) {
        dspic33_reorder_events(cpu);
    }
}

void dspic33_i2c_update_pmd(Dspic33* cpu, uint16_t address, uint16_t previous) {
    static const uint16_t addresses[DSPIC33_I2C_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t masks[DSPIC33_I2C_COUNT] = {0x0080u, 0x0002u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        bool disabled;
        if (address != addresses[channel]) {
            continue;
        }
        disabled = (raw_word(cpu, address) & masks[channel]) != 0u;
        if (((previous & masks[channel]) != 0u) == disabled) {
            return;
        }
        cpu->io.i2c_pmd_generation[channel]++;
        if (!dspic33_schedule(
                cpu, DSPIC33_EVENT_I2C, channel,
                ((uint32_t)I2C_EVENT_PMD << I2C_EVENT_KIND_SHIFT) |
                    ((uint32_t)cpu->io.i2c_pmd_generation[channel] << I2C_EVENT_GENERATION_SHIFT) |
                    (disabled ? 1u : 0u),
                dspic33_device_instruction_cycles(cpu, 1u))) {
            raw_write_word(cpu, address, previous);
            cpu->io.i2c_pmd_generation[channel]++;
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        return;
    }
}

static bool transfer_push(Dspic33I2cQueue* queue, const Dspic33I2cTransfer* transfer) {
    uint8_t index;
    if (queue->count == DSPIC33_I2C_QUEUE_SIZE) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) % DSPIC33_I2C_QUEUE_SIZE);
    queue->transfers[index] = *transfer;
    queue->count++;
    return true;
}

static bool transfer_pop(Dspic33I2cQueue* queue, Dspic33I2cTransfer* transfer) {
    if (queue->count == 0u) {
        return false;
    }
    *transfer = queue->transfers[queue->head];
    queue->head = (uint8_t)((queue->head + 1u) % DSPIC33_I2C_QUEUE_SIZE);
    queue->count--;
    return true;
}

static bool response_push(Dspic33I2cResponseQueue* queue, const Dspic33I2cResponse* response) {
    uint8_t index;
    uint8_t logical;
    if (queue->count == DSPIC33_I2C_QUEUE_SIZE) {
        return false;
    }
    index = (uint8_t)((queue->head + queue->count) % DSPIC33_I2C_QUEUE_SIZE);
    queue->responses[index] = *response;
    queue->count++;
    logical = (uint8_t)(queue->count - 1u);
    while (logical != 0u) {
        uint8_t current = (uint8_t)((queue->head + logical) % DSPIC33_I2C_QUEUE_SIZE);
        uint8_t prior = (uint8_t)((queue->head + logical - 1u) % DSPIC33_I2C_QUEUE_SIZE);
        Dspic33I2cResponse temporary;
        if (queue->responses[prior].cycle <= queue->responses[current].cycle) {
            break;
        }
        temporary = queue->responses[prior];
        queue->responses[prior] = queue->responses[current];
        queue->responses[current] = temporary;
        logical--;
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

static void record_transfer(Dspic33* cpu, uint8_t channel, Dspic33I2cTransferType type,
                            uint16_t value, bool acknowledge, bool master) {
    Dspic33I2cTransfer transfer;
    transfer.type = type;
    transfer.value = value;
    transfer.acknowledge = acknowledge;
    transfer.master = master;
    if (!transfer_push(&cpu->io.i2c_tx[channel], &transfer)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static bool slave_acknowledges(uint16_t status) {
    return (status & (I2C_RBF | I2C_OVERFLOW)) == 0u;
}

static void record_slave_acknowledgement(Dspic33* cpu, uint8_t channel, bool acknowledge) {
    record_transfer(cpu, channel, DSPIC33_I2C_ACKNOWLEDGE, 0u, acknowledge, false);
}

static uint64_t operation_cycles(const Dspic33* cpu, uint8_t channel, uint8_t half_periods) {
    uint64_t scaled =
        (uint64_t)(raw_word(cpu, (uint16_t)(bases[channel] + I2C_BRG)) + 2u) * half_periods;
    return (scaled + 1u) / 2u;
}

static uint32_t internal_event_value(const Dspic33* cpu, uint8_t channel, uint8_t kind,
                                     uint16_t payload) {
    return ((uint32_t)kind << I2C_EVENT_KIND_SHIFT) |
           ((uint32_t)cpu->io.i2c_generation[channel] << I2C_EVENT_GENERATION_SHIFT) | payload;
}

static bool schedule_event(Dspic33* cpu, uint8_t channel, uint32_t value, uint64_t delay,
                           bool external) {
    uint64_t sequence = cpu->events.sequence;
    Dspic33Event* event;
    bool scheduled = external
                         ? dspic33_schedule_external(cpu, DSPIC33_EVENT_I2C, channel, value, delay)
                         : dspic33_schedule(cpu, DSPIC33_EVENT_I2C, channel, value, delay);
    if (!scheduled) {
        return false;
    }
    event = scheduled_event(cpu, sequence);
    if (event == NULL) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    event->paused_remaining = 0u;
    event->paused = false;
    if (module_disabled(cpu, channel) &&
        (uint8_t)(value >> I2C_EVENT_KIND_SHIFT) <= I2C_EVENT_TRANSMIT_SHIFT) {
        event->paused_remaining = delay;
        event->paused = true;
        dspic33_reorder_events(cpu);
    }
    return true;
}

static bool schedule_internal(Dspic33* cpu, uint8_t channel, uint8_t kind, uint16_t payload,
                              uint64_t delay) {
    bool scheduled = schedule_event(cpu, channel, internal_event_value(cpu, channel, kind, payload),
                                    delay, false);
    if (!scheduled) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    return scheduled;
}

static bool schedule_external_event(Dspic33* cpu, uint8_t channel, uint8_t kind, uint16_t payload,
                                    uint64_t delay) {
    return schedule_event(cpu, channel, ((uint32_t)kind << I2C_EVENT_KIND_SHIFT) | payload, delay,
                          true);
}

static void raise_master(Dspic33* cpu, uint8_t channel) {
    dspic33_raise_interrupt(cpu, master_irqs[channel]);
}

static void raise_slave(Dspic33* cpu, uint8_t channel) {
    dspic33_raise_interrupt(cpu, slave_irqs[channel]);
}

static void reset_runtime(Dspic33* cpu, uint8_t channel) {
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

static bool pin_input_high(const Dspic33* cpu, const Dspic33I2cPinMapping* mapping, bool clock) {
    bool high = false;
    dspic33_device_gpio_input_high(cpu, mapping->port, clock ? mapping->clock : mapping->data,
                                   &high);
    return high;
}

static void pin_set_low(Dspic33* cpu, uint8_t channel, bool clock, bool low) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t* state = clock ? &cpu->io.i2c_pin_clock_low : &cpu->io.i2c_pin_data_low;
    if (low) {
        *state |= bit;
    } else {
        *state &= (uint8_t)~bit;
    }
}

static bool pin_schedule_next(Dspic33* cpu, uint8_t channel, uint8_t phase) {
    uint64_t previous = operation_cycles(cpu, channel, phase);
    uint64_t next = operation_cycles(cpu, channel, (uint8_t)(phase + 1u));
    return schedule_internal(cpu, channel, I2C_EVENT_PIN, 0u, next - previous);
}

static void pin_abort(Dspic33* cpu, uint8_t channel) {
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
    if (!pin_mapping(cpu, channel, &mapping)) {
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
    pin_set_low(cpu, channel, true,
                operation == I2C_PIN_RESTART || operation == I2C_PIN_STOP ||
                    operation == I2C_PIN_TRANSMIT || operation == I2C_PIN_RECEIVE ||
                    operation == I2C_PIN_ACKNOWLEDGE);
    pin_set_low(cpu, channel, false,
                operation == I2C_PIN_STOP ||
                    (operation == I2C_PIN_TRANSMIT && (value & 0x80u) == 0u) ||
                    (operation == I2C_PIN_ACKNOWLEDGE &&
                     (raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON)) & I2C_ACKDT) == 0u));
    if (!pin_schedule_next(cpu, channel, 0u)) {
        pin_abort(cpu, channel);
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
    pin_set_low(cpu, channel, true, false);
    if (pin_input_high(cpu, mapping, true)) {
        return true;
    }
    if (pin_delay_master_events(cpu, channel) &&
        schedule_internal(cpu, channel, I2C_EVENT_PIN, 0u, 1u)) {
        return false;
    }
    pin_abort(cpu, channel);
    cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    return false;
}

static void pin_record_response(Dspic33* cpu, uint8_t channel, uint8_t value, bool acknowledge) {
    Dspic33I2cResponse response;
    response.cycle = cpu->device_cycles;
    response.value = value;
    response.acknowledge = acknowledge;
    if (!response_push(&cpu->io.i2c_response[channel], &response)) {
        pin_abort(cpu, channel);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void pin_run(Dspic33* cpu, uint8_t channel) {
    Dspic33I2cPinMapping mapping;
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t operation = cpu->io.i2c_pin_operation[channel];
    uint8_t phase = (uint8_t)(cpu->io.i2c_pin_phase[channel] + 1u);
    uint8_t final_phase;
    bool data_high;
    if ((cpu->io.i2c_pin_active & bit) == 0u || !pin_mapping(cpu, channel, &mapping)) {
        return;
    }
    if (operation == I2C_PIN_START) {
        if (phase == 1u) {
            if (!pin_input_high(cpu, &mapping, true) || !pin_input_high(cpu, &mapping, false)) {
                collide(cpu, channel);
                return;
            }
            pin_set_low(cpu, channel, false, true);
        } else {
            pin_set_low(cpu, channel, true, true);
        }
        final_phase = 2u;
    } else if (operation == I2C_PIN_RESTART) {
        if (phase == 1u && !pin_rising_edge(cpu, channel, &mapping)) {
            return;
        }
        if (phase == 1u && !pin_input_high(cpu, &mapping, false)) {
            collide(cpu, channel);
            return;
        }
        if (phase == 2u) {
            pin_set_low(cpu, channel, false, true);
        } else if (phase == 3u) {
            pin_set_low(cpu, channel, true, true);
        }
        final_phase = 3u;
    } else if (operation == I2C_PIN_STOP) {
        if (phase == 1u && !pin_rising_edge(cpu, channel, &mapping)) {
            return;
        }
        if (phase == 2u) {
            pin_set_low(cpu, channel, false, false);
            dspic33_i2c_refresh_pins(cpu);
            if (!pin_input_high(cpu, &mapping, false)) {
                collide(cpu, channel);
                return;
            }
        }
        final_phase = 3u;
    } else if (operation == I2C_PIN_TRANSMIT) {
        uint8_t value = (uint8_t)raw_word(cpu, (uint16_t)(bases[channel] + I2C_TRN));
        if ((phase & 1u) != 0u) {
            if (!pin_rising_edge(cpu, channel, &mapping)) {
                return;
            }
            data_high = pin_input_high(cpu, &mapping, false);
            if (phase < 17u && (cpu->io.i2c_pin_data_low & bit) == 0u && !data_high) {
                collide(cpu, channel);
                return;
            }
            if (phase == 17u) {
                pin_record_response(cpu, channel, 0u, !data_high);
            }
        } else {
            pin_set_low(cpu, channel, true, true);
            if (phase < 16u) {
                uint8_t data_bit = (uint8_t)(7u - phase / 2u);
                pin_set_low(cpu, channel, false, (value & (uint8_t)(1u << data_bit)) == 0u);
            } else {
                pin_set_low(cpu, channel, false, false);
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
                          (pin_input_high(cpu, &mapping, false) ? 1u : 0u));
            if (phase == 15u) {
                pin_record_response(cpu, channel, cpu->io.i2c_pin_receive[channel], true);
            }
        } else {
            pin_set_low(cpu, channel, true, true);
        }
        final_phase = 16u;
    } else {
        if (phase == 1u && !pin_rising_edge(cpu, channel, &mapping)) {
            return;
        }
        if (phase == 1u &&
            (raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON)) & I2C_ACKDT) != 0u &&
            !pin_input_high(cpu, &mapping, false)) {
            collide(cpu, channel);
            return;
        }
        if (phase == 2u) {
            pin_set_low(cpu, channel, true, true);
            pin_set_low(cpu, channel, false, false);
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
        pin_abort(cpu, channel);
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void begin_control(Dspic33* cpu, uint8_t channel, uint16_t operation) {
    uint16_t control = raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON));
    uint16_t selected = 0u;
    uint16_t bit;
    for (bit = 1u; bit <= I2C_ACKEN; bit <<= 1u) {
        if ((operation & bit) != 0u) {
            selected = bit;
            break;
        }
    }
    control = (uint16_t)((control & ~I2C_MASTER_MASK) | selected);
    raw_write_word(cpu, (uint16_t)(bases[channel] + I2C_CON), control);
    if (selected != 0u) {
        uint8_t periods = 2u;
        bool condition = selected == I2C_SEN || selected == I2C_RSEN || selected == I2C_PEN;
        if (selected == I2C_RCEN) {
            periods = 16u;
        } else if (selected == I2C_RSEN || selected == I2C_PEN) {
            periods = 3u;
        }
        if (!schedule_internal(cpu, channel, I2C_EVENT_CONTROL, selected,
                               operation_cycles(cpu, channel, periods))) {
            raw_write_word(cpu, (uint16_t)(bases[channel] + I2C_CON),
                           (uint16_t)(control & ~selected));
            return;
        }
        if (condition) {
            uint8_t status_periods = selected == I2C_SEN ? 1u : 2u;
            if (!schedule_internal(cpu, channel, I2C_EVENT_BUS_STATUS, selected,
                                   operation_cycles(cpu, channel, status_periods))) {
                cpu->io.i2c_generation[channel]++;
                raw_write_word(cpu, (uint16_t)(bases[channel] + I2C_CON),
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
                raw_write_word(cpu, (uint16_t)(bases[channel] + I2C_CON),
                               (uint16_t)(control & ~selected));
            }
        }
    }
}

static void complete_bus_status(Dspic33* cpu, uint8_t channel, uint16_t operation) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    if (!module_enabled(cpu, channel) || (control & operation) == 0u) {
        return;
    }
    if (operation == I2C_SEN || operation == I2C_RSEN) {
        status = (uint16_t)((status | I2C_START_STATUS) & ~I2C_STOP_STATUS);
    } else if (operation == I2C_PEN) {
        status = (uint16_t)((status | I2C_STOP_STATUS) & ~I2C_START_STATUS);
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
}

static void write_transmit(Dspic33* cpu, uint8_t channel, uint16_t previous, uint8_t value) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel)) {
        raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        return;
    }
    if ((control & I2C_MASTER_MASK) != 0u || (status & (I2C_TBF | I2C_TRANSMIT_ACTIVE)) != 0u) {
        raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        raw_write_word(cpu, (uint16_t)(base + I2C_STAT), (uint16_t)(status | I2C_WRITE_COLLISION));
        return;
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_TRN), value);
    status |= I2C_TBF;
    if ((cpu->io.i2c_slave_active & bit) != 0u && (cpu->io.i2c_slave_read & bit) != 0u) {
        raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
        return;
    }
    cpu->io.i2c_master_active |= bit;
    status |= I2C_TRANSMIT_ACTIVE;
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    if (!schedule_internal(cpu, channel, I2C_EVENT_TRANSMIT_SHIFT, value,
                           operation_cycles(cpu, channel, 16u)) ||
        !schedule_internal(cpu, channel, I2C_EVENT_TRANSMIT, value,
                           operation_cycles(cpu, channel, 18u)) ||
        !pin_begin(cpu, channel, I2C_PIN_TRANSMIT, value)) {
        cpu->io.i2c_generation[channel]++;
        cpu->io.i2c_master_active &= (uint8_t)~bit;
        raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        raw_write_word(cpu, (uint16_t)(base + I2C_STAT),
                       (uint16_t)(status & ~(I2C_TBF | I2C_TRANSMIT_ACTIVE)));
        return;
    }
    record_transfer(cpu, channel, DSPIC33_I2C_WRITE, value, false, true);
}

static void complete_control(Dspic33* cpu, uint8_t channel, uint16_t operation) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel) || (control & operation) == 0u) {
        return;
    }
    if (operation == I2C_ACKEN && dspic33_cpu_rmw_matches(cpu, base + I2C_CON, 2u)) {
        cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
        return;
    }
    if (operation == I2C_RCEN) {
        uint64_t delay;
        if (response_wait(&cpu->io.i2c_response[channel], cpu->device_cycles, &delay)) {
            schedule_internal(cpu, channel, I2C_EVENT_CONTROL, operation, delay);
            return;
        }
    }
    control &= (uint16_t)~operation;
    if (operation == I2C_SEN || operation == I2C_RSEN) {
        status = (uint16_t)((status | I2C_START_STATUS) & ~I2C_STOP_STATUS);
        cpu->io.i2c_master_active |= bit;
        record_transfer(cpu, channel,
                        operation == I2C_SEN ? DSPIC33_I2C_START : DSPIC33_I2C_RESTART, 0u, false,
                        true);
    } else if (operation == I2C_PEN) {
        status = (uint16_t)((status | I2C_STOP_STATUS) & ~I2C_START_STATUS);
        cpu->io.i2c_master_active &= (uint8_t)~bit;
        record_transfer(cpu, channel, DSPIC33_I2C_STOP, 0u, false, true);
    } else if (operation == I2C_RCEN) {
        Dspic33I2cResponse response;
        uint8_t received = 0xffu;
        if (response_pop(&cpu->io.i2c_response[channel], cpu->device_cycles, &response)) {
            received = response.value;
        }
        if ((status & I2C_RBF) != 0u) {
            status |= I2C_OVERFLOW;
        } else {
            raw_write_word(cpu, (uint16_t)(base + I2C_RCV), received);
            status |= I2C_RBF;
        }
        record_transfer(cpu, channel, DSPIC33_I2C_READ, received, true, true);
    } else if (operation == I2C_ACKEN) {
        record_transfer(cpu, channel, DSPIC33_I2C_ACKNOWLEDGE, 0u, (control & I2C_ACKDT) == 0u,
                        true);
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    raise_master(cpu, channel);
}

static void complete_transmit(Dspic33* cpu, uint8_t channel) {
    Dspic33I2cResponse response;
    uint16_t base = bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    bool acknowledge = false;
    if (!module_enabled(cpu, channel) || (status & I2C_TRANSMIT_ACTIVE) == 0u) {
        return;
    }
    {
        uint64_t delay;
        if (response_wait(&cpu->io.i2c_response[channel], cpu->device_cycles, &delay)) {
            schedule_internal(cpu, channel, I2C_EVENT_TRANSMIT, 0u, delay);
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
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    raise_master(cpu, channel);
}

static void complete_transmit_shift(Dspic33* cpu, uint8_t channel) {
    uint16_t base = bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    if (module_enabled(cpu, channel) && (status & I2C_TRANSMIT_ACTIVE) != 0u) {
        raw_write_word(cpu, (uint16_t)(base + I2C_STAT), (uint16_t)(status & ~I2C_TBF));
    }
}

static bool address_matches(const Dspic33* cpu, uint8_t channel, uint16_t address) {
    uint16_t control = raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON));
    uint16_t configured = raw_word(cpu, (uint16_t)(bases[channel] + I2C_ADD));
    uint16_t mask = raw_word(cpu, (uint16_t)(bases[channel] + I2C_MSK));
    if ((control & I2C_IPMIEN) != 0u) {
        return true;
    }
    if ((control & I2C_A10M) != 0u) {
        return false;
    }
    if ((address <= 0x07u || address >= 0x78u) && address != (configured & 0x007fu)) {
        return false;
    }
    return ((address ^ configured) & (uint16_t)~mask & 0x007fu) == 0u;
}

static bool ten_bit_high_matches(const Dspic33* cpu, uint8_t channel, uint16_t address) {
    uint16_t control = raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON));
    uint16_t configured = raw_word(cpu, (uint16_t)(bases[channel] + I2C_ADD));
    uint16_t mask = raw_word(cpu, (uint16_t)(bases[channel] + I2C_MSK));
    return (control & I2C_IPMIEN) != 0u ||
           ((control & I2C_A10M) != 0u &&
            ((address ^ configured) & (uint16_t)~mask & 0x0300u) == 0u);
}

static void reject_slave(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.i2c_slave_active &= (uint8_t)~bit;
    cpu->io.i2c_slave_read &= (uint8_t)~bit;
    cpu->io.i2c_slave_rejected |= bit;
}

static void slave_start(Dspic33* cpu, uint8_t channel, uint16_t payload, bool schedule_ten_second,
                        bool interrupt) {
    uint16_t base = bases[channel];
    uint16_t address = payload & 0x03ffu;
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    bool read = (payload & I2C_EXTERNAL_READ) != 0u;
    bool ten_bit = (payload & I2C_EXTERNAL_TEN_BIT) != 0u;
    bool general_call = !ten_bit && address == 0u && !read;
    bool acknowledge = slave_acknowledges(status);
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel)) {
        return;
    }
    if ((cpu->io.i2c_slave_rejected & bit) != 0u) {
        return;
    }
    status = (uint16_t)((status | I2C_START_STATUS) &
                        ~(I2C_STOP_STATUS | I2C_GENERAL_CALL | I2C_TEN_BIT));
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    if (ten_bit && !ten_bit_high_matches(cpu, channel, address)) {
        record_slave_acknowledgement(cpu, channel, false);
        reject_slave(cpu, channel);
        return;
    }
    if (!ten_bit && (control & I2C_IPMIEN) == 0u &&
        ((general_call && (control & I2C_GCEN) == 0u) ||
         (!general_call && !address_matches(cpu, channel, address)))) {
        record_slave_acknowledgement(cpu, channel, false);
        reject_slave(cpu, channel);
        return;
    }
    cpu->io.i2c_slave_active |= bit;
    if (read && !ten_bit) {
        cpu->io.i2c_slave_read |= bit;
    } else {
        cpu->io.i2c_slave_read &= (uint8_t)~bit;
    }
    cpu->io.i2c_slave_address[channel] = address;
    status &= (uint16_t)~(I2C_DATA | I2C_READ | I2C_GENERAL_CALL | I2C_TEN_BIT);
    if (read && !ten_bit) {
        status |= I2C_READ;
    }
    if (general_call) {
        status |= I2C_GENERAL_CALL;
    }
    if ((status & I2C_RBF) != 0u) {
        status |= I2C_OVERFLOW;
    } else {
        raw_write_word(cpu, (uint16_t)(base + I2C_RCV),
                       ten_bit ? (uint16_t)(0x00f0u | ((address >> 7u) & 6u))
                               : (uint16_t)((address << 1u) | read));
        status |= I2C_RBF;
    }
    control &= (uint16_t)~I2C_SCLREL;
    if (read && !ten_bit && (control & I2C_IPMIEN) != 0u) {
        cpu->io.i2c_slave_active &= (uint8_t)~bit;
        cpu->io.i2c_slave_read &= (uint8_t)~bit;
        control |= I2C_SCLREL;
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        raise_slave(cpu, channel);
    }
    if (ten_bit && acknowledge && schedule_ten_second &&
        !schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_TEN_SECOND, address, 1u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void slave_ten_second(Dspic33* cpu, uint8_t channel, uint16_t address, bool interrupt) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint16_t configured = raw_word(cpu, (uint16_t)(base + I2C_ADD));
    uint16_t mask = raw_word(cpu, (uint16_t)(base + I2C_MSK));
    bool acknowledge = slave_acknowledges(status);
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        cpu->io.i2c_slave_address[channel] != address) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_TEN_SECOND, address, 1u);
        return;
    }
    if ((control & I2C_IPMIEN) == 0u &&
        ((address ^ configured) & (uint16_t)~mask & 0x00ffu) != 0u) {
        record_slave_acknowledgement(cpu, channel, false);
        reject_slave(cpu, channel);
        return;
    }
    status = (uint16_t)((status | I2C_TEN_BIT) & ~(I2C_DATA | I2C_READ));
    if ((status & I2C_RBF) != 0u) {
        status |= I2C_OVERFLOW;
    } else {
        raw_write_word(cpu, (uint16_t)(base + I2C_RCV), address & 0x00ffu);
        status |= I2C_RBF;
    }
    control &= (uint16_t)~I2C_SCLREL;
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        raise_slave(cpu, channel);
    }
}

static void slave_ten_restart(Dspic33* cpu, uint8_t channel, uint16_t address, bool interrupt) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    bool acknowledge = slave_acknowledges(status);
    bool ipmi;
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        cpu->io.i2c_slave_address[channel] != address || (status & I2C_TEN_BIT) == 0u) {
        return;
    }
    if (!ten_bit_high_matches(cpu, channel, address)) {
        record_slave_acknowledgement(cpu, channel, false);
        reject_slave(cpu, channel);
        return;
    }
    ipmi = (control & I2C_IPMIEN) != 0u;
    cpu->io.i2c_slave_read |= bit;
    status = (uint16_t)((status | I2C_START_STATUS | I2C_READ) & ~(I2C_STOP_STATUS | I2C_DATA));
    if ((status & I2C_RBF) != 0u) {
        status |= I2C_OVERFLOW;
    } else {
        raw_write_word(cpu, (uint16_t)(base + I2C_RCV),
                       (uint16_t)(0x00f1u | ((address >> 7u) & 6u)));
        status |= I2C_RBF;
    }
    if (ipmi) {
        cpu->io.i2c_slave_active &= (uint8_t)~bit;
        cpu->io.i2c_slave_read &= (uint8_t)~bit;
        control |= I2C_SCLREL;
    } else {
        control &= (uint16_t)~I2C_SCLREL;
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        raise_slave(cpu, channel);
    }
}

static void slave_write(Dspic33* cpu, uint8_t channel, uint8_t value, bool interrupt) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    bool acknowledge = slave_acknowledges(status);
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        (cpu->io.i2c_slave_read & bit) != 0u) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_WRITE, value, 1u);
        return;
    }
    status |= I2C_DATA;
    if ((status & I2C_RBF) != 0u) {
        status |= I2C_OVERFLOW;
    } else {
        raw_write_word(cpu, (uint16_t)(base + I2C_RCV), value);
        status |= I2C_RBF;
    }
    if ((control & I2C_STREN) != 0u) {
        control &= (uint16_t)~I2C_SCLREL;
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        raise_slave(cpu, channel);
    }
}

static void slave_read(Dspic33* cpu, uint8_t channel, bool acknowledge, bool require_buffer) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    bool ten_bit = (status & I2C_TEN_BIT) != 0u;
    uint8_t value;
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        (cpu->io.i2c_slave_read & bit) == 0u || (require_buffer && (status & I2C_TBF) == 0u)) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_READ, acknowledge ? 1u : 0u, 1u);
        return;
    }
    value = (uint8_t)raw_word(cpu, (uint16_t)(base + I2C_TRN));
    status &= (uint16_t)~I2C_TBF;
    status |= I2C_DATA;
    if (!acknowledge) {
        status |= I2C_NOT_ACKNOWLEDGED;
        cpu->io.i2c_slave_active &= (uint8_t)~bit;
        cpu->io.i2c_slave_read &= (uint8_t)~bit;
    } else {
        status &= (uint16_t)~I2C_NOT_ACKNOWLEDGED;
        control &= (uint16_t)~I2C_SCLREL;
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    record_transfer(cpu, channel, DSPIC33_I2C_WRITE, value, acknowledge, false);
    if (acknowledge || !ten_bit) {
        raise_slave(cpu, channel);
    }
}

static void slave_stop(Dspic33* cpu, uint8_t channel) {
    uint16_t base = bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel)) {
        return;
    }
    status = (uint16_t)((status | I2C_STOP_STATUS) &
                        ~(I2C_START_STATUS | I2C_TEN_BIT | I2C_GENERAL_CALL | I2C_TBF |
                          I2C_TRANSMIT_ACTIVE));
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    cpu->io.i2c_slave_active &= (uint8_t)~bit;
    cpu->io.i2c_slave_read &= (uint8_t)~bit;
    cpu->io.i2c_slave_rejected &= (uint8_t)~bit;
}

static void collide(Dspic33* cpu, uint8_t channel) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel)) {
        return;
    }
    control &= (uint16_t)~I2C_MASTER_MASK;
    status = (uint16_t)((status | I2C_BUS_COLLISION) & ~(I2C_TBF | I2C_TRANSMIT_ACTIVE));
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    pin_abort(cpu, channel);
    cpu->io.i2c_generation[channel]++;
    cpu->io.i2c_master_active &= (uint8_t)~bit;
    record_transfer(cpu, channel, DSPIC33_I2C_COLLISION, 0u, false, true);
    raise_master(cpu, channel);
}

static bool slave_pin_operating(const Dspic33* cpu, uint8_t channel) {
    return module_enabled(cpu, channel) &&
           !(cpu->power_state == DSPIC33_POWER_IDLE &&
             (raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON)) & 0x2000u) != 0u);
}

static bool resolved_pin_high(const Dspic33* cpu, uint8_t channel,
                              const Dspic33I2cPinMapping* mapping, bool clock) {
    uint8_t bit = (uint8_t)(1u << channel);
    if (((clock ? cpu->io.i2c_pin_clock_low : cpu->io.i2c_pin_data_low) & bit) != 0u) {
        return false;
    }
    return pin_input_high(cpu, mapping, clock);
}

static void slave_pin_baseline(Dspic33* cpu, uint8_t channel, const Dspic33I2cPinMapping* mapping) {
    uint8_t bit = (uint8_t)(1u << channel);
    if (resolved_pin_high(cpu, channel, mapping, true)) {
        cpu->io.i2c_pin_clock_high |= bit;
    } else {
        cpu->io.i2c_pin_clock_high &= (uint8_t)~bit;
    }
    if (resolved_pin_high(cpu, channel, mapping, false)) {
        cpu->io.i2c_pin_data_high |= bit;
    } else {
        cpu->io.i2c_pin_data_high &= (uint8_t)~bit;
    }
}

static void slave_pin_prepare_transmit(Dspic33* cpu, uint8_t channel) {
    uint16_t base = bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit_index = cpu->io.i2c_slave_pin_bits[channel];
    if (cpu->io.i2c_slave_pin_state[channel] != I2C_SLAVE_PIN_TRANSMIT) {
        return;
    }
    if (bit_index >= 8u || (status & I2C_TBF) == 0u) {
        pin_set_low(cpu, channel, false, false);
        return;
    }
    pin_set_low(cpu, channel, false,
                (raw_word(cpu, (uint16_t)(base + I2C_TRN)) & (uint16_t)(0x0080u >> bit_index)) ==
                    0u);
}

static void slave_pin_start(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    cpu->io.i2c_pin_physical |= bit;
    cpu->io.i2c_slave_pin_active |= bit;
    cpu->io.i2c_slave_pin_acknowledge &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_interrupt &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_stretch &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_state[channel] = I2C_SLAVE_PIN_ADDRESS;
    cpu->io.i2c_slave_pin_next[channel] = I2C_SLAVE_PIN_IDLE;
    cpu->io.i2c_slave_pin_bits[channel] = 0u;
    cpu->io.i2c_slave_pin_shift[channel] = 0u;
    pin_set_low(cpu, channel, true, false);
    pin_set_low(cpu, channel, false, false);
}

static void slave_pin_stop(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    slave_stop(cpu, channel);
    cpu->io.i2c_pin_physical &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_active &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_acknowledge &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_interrupt &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_stretch &= (uint8_t)~bit;
    cpu->io.i2c_slave_pin_state[channel] = I2C_SLAVE_PIN_IDLE;
    cpu->io.i2c_slave_pin_next[channel] = I2C_SLAVE_PIN_IDLE;
    cpu->io.i2c_slave_pin_bits[channel] = 0u;
    pin_set_low(cpu, channel, true, false);
    pin_set_low(cpu, channel, false, false);
}

static void slave_pin_receive_rising(Dspic33* cpu, uint8_t channel, bool data_high) {
    cpu->io.i2c_slave_pin_shift[channel] =
        (uint8_t)((cpu->io.i2c_slave_pin_shift[channel] << 1u) | (data_high ? 1u : 0u));
    cpu->io.i2c_slave_pin_bits[channel]++;
    if (cpu->io.i2c_slave_pin_bits[channel] != 8u) {
        return;
    }
    cpu->io.i2c_slave_pin_next[channel] = cpu->io.i2c_slave_pin_state[channel];
    cpu->io.i2c_slave_pin_state[channel] = I2C_SLAVE_PIN_RECEIVED;
}

static void slave_pin_receive_falling(Dspic33* cpu, uint8_t channel) {
    uint16_t base = bases[channel];
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t state = cpu->io.i2c_slave_pin_next[channel];
    bool acknowledge;
    bool interrupt = false;
    uint16_t resulting_control;
    acknowledge = slave_acknowledges(status);
    if (state == I2C_SLAVE_PIN_ADDRESS) {
        uint8_t byte = cpu->io.i2c_slave_pin_shift[channel];
        bool read = (byte & 1u) != 0u;
        if ((byte & 0xf8u) == 0xf0u) {
            uint16_t address = (uint16_t)((byte & 6u) << 7u);
            if (read) {
                address = cpu->io.i2c_slave_address[channel];
                slave_ten_restart(cpu, channel, address, false);
            } else {
                slave_start(cpu, channel, (uint16_t)(address | I2C_EXTERNAL_TEN_BIT), false, false);
            }
        } else {
            uint8_t address = (uint8_t)(byte >> 1u);
            slave_start(cpu, channel, (uint16_t)(address | (read ? I2C_EXTERNAL_READ : 0u)), true,
                        false);
        }
        acknowledge = acknowledge && (cpu->io.i2c_slave_active & bit) != 0u &&
                      (cpu->io.i2c_slave_rejected & bit) == 0u;
        interrupt = acknowledge;
        if (acknowledge && (byte & 0xf8u) == 0xf0u && !read) {
            cpu->io.i2c_slave_pin_next[channel] = I2C_SLAVE_PIN_TEN_SECOND;
        } else {
            cpu->io.i2c_slave_pin_next[channel] =
                acknowledge ? (read ? I2C_SLAVE_PIN_TRANSMIT : I2C_SLAVE_PIN_RECEIVE)
                            : I2C_SLAVE_PIN_REJECTED;
        }
    } else if (state == I2C_SLAVE_PIN_TEN_SECOND) {
        uint16_t address = (uint16_t)((cpu->io.i2c_slave_address[channel] & 0x0300u) |
                                      cpu->io.i2c_slave_pin_shift[channel]);
        cpu->io.i2c_slave_address[channel] = address;
        slave_ten_second(cpu, channel, address, false);
        acknowledge = acknowledge && (cpu->io.i2c_slave_active & bit) != 0u &&
                      (cpu->io.i2c_slave_rejected & bit) == 0u;
        interrupt = acknowledge;
        cpu->io.i2c_slave_pin_next[channel] =
            acknowledge ? I2C_SLAVE_PIN_RECEIVE : I2C_SLAVE_PIN_REJECTED;
    } else {
        slave_write(cpu, channel, cpu->io.i2c_slave_pin_shift[channel], false);
        acknowledge = acknowledge && (cpu->io.i2c_slave_active & bit) != 0u;
        interrupt = (cpu->io.i2c_slave_active & bit) != 0u;
        cpu->io.i2c_slave_pin_next[channel] = I2C_SLAVE_PIN_RECEIVE;
    }
    if (acknowledge) {
        cpu->io.i2c_slave_pin_acknowledge |= bit;
    } else {
        cpu->io.i2c_slave_pin_acknowledge &= (uint8_t)~bit;
    }
    if (interrupt) {
        cpu->io.i2c_slave_pin_interrupt |= bit;
    } else {
        cpu->io.i2c_slave_pin_interrupt &= (uint8_t)~bit;
    }
    resulting_control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    if ((resulting_control & I2C_SCLREL) == 0u) {
        cpu->io.i2c_slave_pin_stretch |= bit;
        raw_write_word(cpu, (uint16_t)(base + I2C_CON), (uint16_t)(resulting_control | I2C_SCLREL));
    } else {
        cpu->io.i2c_slave_pin_stretch &= (uint8_t)~bit;
    }
    cpu->io.i2c_slave_pin_state[channel] = I2C_SLAVE_PIN_ACKNOWLEDGE;
    cpu->io.i2c_slave_pin_bits[channel] = 0u;
    cpu->io.i2c_slave_pin_shift[channel] = 0u;
    pin_set_low(cpu, channel, false, acknowledge);
    cpu->io.i2c_slave_pin_bits[channel] = 1u;
}

static void slave_pin_rising(Dspic33* cpu, uint8_t channel, bool data_high) {
    uint8_t state = cpu->io.i2c_slave_pin_state[channel];
    if (state == I2C_SLAVE_PIN_ADDRESS || state == I2C_SLAVE_PIN_RECEIVE ||
        state == I2C_SLAVE_PIN_TEN_SECOND) {
        slave_pin_receive_rising(cpu, channel, data_high);
    } else if (state == I2C_SLAVE_PIN_ACKNOWLEDGE) {
        if (cpu->io.i2c_slave_pin_bits[channel] == 1u) {
            cpu->io.i2c_slave_pin_bits[channel] = 2u;
        }
    } else if (state == I2C_SLAVE_PIN_TRANSMIT) {
        cpu->io.i2c_slave_pin_bits[channel]++;
        if (cpu->io.i2c_slave_pin_bits[channel] == 8u) {
            cpu->io.i2c_slave_pin_state[channel] = I2C_SLAVE_PIN_MASTER_ACKNOWLEDGE;
            cpu->io.i2c_slave_pin_bits[channel] = 0u;
        }
    } else if (state == I2C_SLAVE_PIN_MASTER_ACKNOWLEDGE &&
               cpu->io.i2c_slave_pin_bits[channel] == 1u) {
        cpu->io.i2c_slave_pin_shift[channel] = data_high ? 0u : 1u;
        cpu->io.i2c_slave_pin_bits[channel] = 2u;
    }
}

static void slave_pin_falling(Dspic33* cpu, uint8_t channel) {
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t state = cpu->io.i2c_slave_pin_state[channel];
    if (state == I2C_SLAVE_PIN_RECEIVED) {
        slave_pin_receive_falling(cpu, channel);
    } else if (state == I2C_SLAVE_PIN_ACKNOWLEDGE) {
        if (cpu->io.i2c_slave_pin_bits[channel] == 0u) {
            pin_set_low(cpu, channel, false,
                        (cpu->io.i2c_slave_pin_acknowledge & (uint8_t)(1u << channel)) != 0u);
            cpu->io.i2c_slave_pin_bits[channel] = 1u;
        } else if (cpu->io.i2c_slave_pin_bits[channel] == 2u) {
            pin_set_low(cpu, channel, false, false);
            cpu->io.i2c_slave_pin_state[channel] = cpu->io.i2c_slave_pin_next[channel];
            cpu->io.i2c_slave_pin_bits[channel] = 0u;
            if ((cpu->io.i2c_slave_pin_stretch & bit) != 0u) {
                uint16_t base = bases[channel];
                uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
                cpu->io.i2c_slave_pin_stretch &= (uint8_t)~bit;
                raw_write_word(cpu, (uint16_t)(base + I2C_CON), (uint16_t)(control & ~I2C_SCLREL));
                pin_set_low(cpu, channel, true, true);
            }
            if ((cpu->io.i2c_slave_pin_interrupt & bit) != 0u) {
                cpu->io.i2c_slave_pin_interrupt &= (uint8_t)~bit;
                raise_slave(cpu, channel);
            }
            slave_pin_prepare_transmit(cpu, channel);
        }
    } else if (state == I2C_SLAVE_PIN_TRANSMIT) {
        slave_pin_prepare_transmit(cpu, channel);
    } else if (state == I2C_SLAVE_PIN_MASTER_ACKNOWLEDGE) {
        if (cpu->io.i2c_slave_pin_bits[channel] == 0u) {
            uint16_t base = bases[channel];
            uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
            raw_write_word(cpu, (uint16_t)(base + I2C_STAT), (uint16_t)(status & ~I2C_TBF));
            pin_set_low(cpu, channel, false, false);
            cpu->io.i2c_slave_pin_bits[channel] = 1u;
        } else if (cpu->io.i2c_slave_pin_bits[channel] == 2u) {
            bool acknowledge = cpu->io.i2c_slave_pin_shift[channel] != 0u;
            slave_read(cpu, channel, acknowledge, false);
            cpu->io.i2c_slave_pin_state[channel] =
                acknowledge ? I2C_SLAVE_PIN_TRANSMIT : I2C_SLAVE_PIN_REJECTED;
            cpu->io.i2c_slave_pin_bits[channel] = 0u;
            cpu->io.i2c_slave_pin_shift[channel] = 0u;
            slave_pin_prepare_transmit(cpu, channel);
        }
    }
}

void dspic33_i2c_refresh_pins(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        Dspic33I2cPinMapping mapping;
        uint8_t bit = (uint8_t)(1u << channel);
        bool clock_high;
        bool data_high;
        bool previous_clock_high;
        bool previous_data_high;
        if (!pin_mapping(cpu, channel, &mapping)) {
            continue;
        }
        if ((cpu->io.i2c_pin_active & bit) == 0u && (cpu->io.i2c_slave_active & bit) != 0u) {
            pin_set_low(cpu, channel, true,
                        (raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON)) & I2C_SCLREL) == 0u);
        }
        clock_high = resolved_pin_high(cpu, channel, &mapping, true);
        data_high = resolved_pin_high(cpu, channel, &mapping, false);
        previous_clock_high = (cpu->io.i2c_pin_clock_high & bit) != 0u;
        previous_data_high = (cpu->io.i2c_pin_data_high & bit) != 0u;
        if (!slave_pin_operating(cpu, channel) || (cpu->io.i2c_pin_active & bit) != 0u ||
            (cpu->io.i2c_master_active & bit) != 0u) {
            slave_pin_baseline(cpu, channel, &mapping);
            continue;
        }
        slave_pin_prepare_transmit(cpu, channel);
        clock_high = resolved_pin_high(cpu, channel, &mapping, true);
        data_high = resolved_pin_high(cpu, channel, &mapping, false);
        if (previous_data_high && !data_high && clock_high) {
            slave_pin_start(cpu, channel);
        } else if (!previous_data_high && data_high && clock_high &&
                   (cpu->io.i2c_slave_pin_active & bit) != 0u) {
            slave_pin_stop(cpu, channel);
        } else if (!previous_clock_high && clock_high &&
                   (cpu->io.i2c_slave_pin_active & bit) != 0u) {
            slave_pin_rising(cpu, channel, data_high);
        } else if (previous_clock_high && !clock_high &&
                   (cpu->io.i2c_slave_pin_active & bit) != 0u) {
            slave_pin_falling(cpu, channel);
        }
        slave_pin_baseline(cpu, channel, &mapping);
    }
}

bool dspic33_i2c_write_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested) {
    uint8_t channel;
    uint16_t offset;
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (!channel_for_address(base, &channel, &offset)) {
        return false;
    }
    if (module_disabled(cpu, channel)) {
        raw_write_word(cpu, base, previous);
        return true;
    }
    if (offset == I2C_RCV) {
        raw_write_word(cpu, base, previous);
    } else if (offset == I2C_TRN) {
        if ((address & 1u) == 0u || cpu->io.cpu_write_width == 2u) {
            write_transmit(cpu, channel, previous, (uint8_t)requested);
        } else {
            raw_write_word(cpu, base, previous);
        }
    } else if (offset == I2C_BRG) {
        raw_write_word(cpu, base, requested & 0x01ffu);
    } else if (offset == I2C_CON) {
        uint16_t control = requested & 0xbfffu;
        uint16_t active = previous & I2C_MASTER_MASK;
        bool disabling = (previous & I2C_ENABLE) != 0u && (control & I2C_ENABLE) == 0u;
        if ((control & I2C_STREN) == 0u && (control & I2C_SCLREL) == 0u) {
            control |= previous & I2C_SCLREL;
        }
        if (disabling) {
            control = (uint16_t)((control & ~I2C_MASTER_MASK) | I2C_SCLREL);
        } else if (active != 0u) {
            control = (uint16_t)((control & ~I2C_MASTER_MASK) | active);
        }
        if ((control & I2C_ENABLE) == 0u) {
            control |= I2C_SCLREL;
        }
        raw_write_word(cpu, base, control);
        if (disabling) {
            raw_write_word(cpu, (uint16_t)(bases[channel] + I2C_STAT), 0u);
            reset_runtime(cpu, channel);
        } else if ((control & I2C_ENABLE) != 0u && active == 0u) {
            begin_control(cpu, channel, control & I2C_MASTER_MASK);
        }
    } else if (offset == I2C_STAT) {
        raw_write_word(cpu, base,
                       (uint16_t)((previous & ~0x04c0u) | (previous & requested & 0x04c0u)));
    } else if (offset == I2C_ADD || offset == I2C_MSK) {
        raw_write_word(cpu, base, requested & 0x03ffu);
    }
    dspic33_i2c_refresh_pins(cpu);
    return true;
}

bool dspic33_i2c_read_register(Dspic33* cpu, uint16_t address, uint8_t* value) {
    uint8_t channel;
    uint16_t offset;
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (!channel_for_address(base, &channel, &offset)) {
        return false;
    }
    if (offset == I2C_TRN) {
        *value = 0u;
    } else {
        *value = cpu->data[address];
    }
    if (offset == I2C_RCV) {
        raw_write_word(cpu, (uint16_t)(bases[channel] + I2C_STAT),
                       (uint16_t)(raw_word(cpu, (uint16_t)(bases[channel] + I2C_STAT)) & ~I2C_RBF));
    }
    return true;
}

void dspic33_i2c_process_event(Dspic33* cpu, uint8_t channel, uint32_t value, bool external) {
    uint8_t kind = (uint8_t)(value >> I2C_EVENT_KIND_SHIFT);
    uint8_t generation = (uint8_t)(value >> I2C_EVENT_GENERATION_SHIFT);
    uint16_t payload = (uint16_t)(value & I2C_EVENT_PAYLOAD_MASK);
    if (channel >= DSPIC33_I2C_COUNT) {
        return;
    }
    if (kind == I2C_EVENT_PMD) {
        uint8_t bit = (uint8_t)(1u << channel);
        bool disabled;
        if (generation != cpu->io.i2c_pmd_generation[channel]) {
            return;
        }
        disabled = (payload & 1u) != 0u;
        if (disabled) {
            cpu->io.i2c_pmd_disabled |= bit;
            pause_events(cpu, channel);
        } else {
            cpu->io.i2c_pmd_disabled &= (uint8_t)~bit;
            resume_events(cpu, channel);
        }
        dspic33_i2c_refresh_pins(cpu);
        return;
    }
    if ((kind <= I2C_EVENT_TRANSMIT_SHIFT || kind == I2C_EVENT_PIN) &&
        generation != cpu->io.i2c_generation[channel]) {
        return;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE &&
        (raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON)) & 0x2000u) != 0u) {
        schedule_event(cpu, channel, value, 1u, external);
        return;
    }
    if (kind == I2C_EVENT_CONTROL) {
        complete_control(cpu, channel, payload);
    } else if (kind == I2C_EVENT_BUS_STATUS) {
        complete_bus_status(cpu, channel, payload);
    } else if (kind == I2C_EVENT_TRANSMIT) {
        complete_transmit(cpu, channel);
    } else if (kind == I2C_EVENT_TRANSMIT_SHIFT) {
        complete_transmit_shift(cpu, channel);
    } else if (kind == I2C_EVENT_SLAVE_START) {
        slave_start(cpu, channel, payload, true, true);
    } else if (kind == I2C_EVENT_SLAVE_WRITE) {
        slave_write(cpu, channel, (uint8_t)payload, true);
    } else if (kind == I2C_EVENT_SLAVE_READ) {
        slave_read(cpu, channel, payload != 0u, true);
    } else if (kind == I2C_EVENT_SLAVE_STOP) {
        slave_stop(cpu, channel);
    } else if (kind == I2C_EVENT_COLLISION) {
        collide(cpu, channel);
    } else if (kind == I2C_EVENT_SLAVE_TEN_SECOND) {
        slave_ten_second(cpu, channel, payload & 0x03ffu, true);
    } else if (kind == I2C_EVENT_SLAVE_TEN_RESTART) {
        slave_ten_restart(cpu, channel, payload & 0x03ffu, true);
    } else if (kind == I2C_EVENT_PIN) {
        pin_run(cpu, channel);
    }
}

bool dspic33_i2c_respond(Dspic33* cpu, uint8_t channel, uint8_t value, bool acknowledge,
                         uint64_t delay) {
    Dspic33I2cResponse response;
    if (channel >= DSPIC33_I2C_COUNT || delay > UINT64_MAX - cpu->device_cycles) {
        return false;
    }
    response.cycle = cpu->device_cycles + delay;
    response.value = value;
    response.acknowledge = acknowledge;
    return response_push(&cpu->io.i2c_response[channel], &response);
}

bool dspic33_i2c_status(Dspic33* cpu, uint8_t channel, uint16_t status) {
    const uint16_t hardware_status = 0x84c0u;
    uint16_t value;
    if (channel >= DSPIC33_I2C_COUNT || (status & (uint16_t)~hardware_status) != 0u) {
        return false;
    }
    value = raw_word(cpu, (uint16_t)(bases[channel] + I2C_STAT));
    raw_write_word(cpu, (uint16_t)(bases[channel] + I2C_STAT),
                   (uint16_t)((value & ~hardware_status) | status));
    return true;
}

bool dspic33_i2c_slave_start(Dspic33* cpu, uint8_t channel, uint16_t address, bool read,
                             bool ten_bit, uint64_t delay) {
    uint16_t payload;
    if (channel >= DSPIC33_I2C_COUNT || address > (ten_bit ? 0x03ffu : 0x007fu)) {
        return false;
    }
    if (ten_bit && read) {
        uint8_t bit = (uint8_t)(1u << channel);
        if ((cpu->io.i2c_slave_active & bit) == 0u ||
            cpu->io.i2c_slave_address[channel] != address ||
            (raw_word(cpu, (uint16_t)(bases[channel] + I2C_STAT)) & I2C_TEN_BIT) == 0u) {
            return false;
        }
        return schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_TEN_RESTART, address, delay);
    }
    payload = (uint16_t)(address | (read ? I2C_EXTERNAL_READ : 0u) |
                         (ten_bit ? I2C_EXTERNAL_TEN_BIT : 0u));
    return schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_START, payload, delay);
}

bool dspic33_i2c_slave_write(Dspic33* cpu, uint8_t channel, uint8_t value, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_WRITE, value, delay);
}

bool dspic33_i2c_slave_read(Dspic33* cpu, uint8_t channel, bool acknowledge, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_READ, acknowledge ? 1u : 0u,
                                   delay);
}

bool dspic33_i2c_slave_stop(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_STOP, 0u, delay);
}

bool dspic33_i2c_collision(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external_event(cpu, channel, I2C_EVENT_COLLISION, 0u, delay);
}

bool dspic33_i2c_transmit(Dspic33* cpu, uint8_t channel, Dspic33I2cTransfer* transfer) {
    return channel < DSPIC33_I2C_COUNT && transfer != NULL &&
           transfer_pop(&cpu->io.i2c_tx[channel], transfer);
}

bool dspic33_i2c_pin(const Dspic33* cpu, uint8_t port, uint8_t pin, bool* high) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        Dspic33I2cPinMapping mapping;
        uint16_t control;
        uint16_t status;
        uint8_t channel_bit;
        bool clock;
        if (!pin_mapping(cpu, channel, &mapping) || mapping.port != port ||
            (mapping.clock != pin && mapping.data != pin) || !module_enabled(cpu, channel)) {
            continue;
        }
        if (high == NULL) {
            return false;
        }
        control = raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON));
        status = raw_word(cpu, (uint16_t)(bases[channel] + I2C_STAT));
        channel_bit = (uint8_t)(1u << channel);
        clock = mapping.clock == pin;
        if ((cpu->io.i2c_pin_physical & channel_bit) != 0u) {
            if ((clock ? cpu->io.i2c_pin_clock_low : cpu->io.i2c_pin_data_low) & channel_bit) {
                *high = false;
                return true;
            }
            return dspic33_device_gpio_input_high(cpu, port, pin, high);
        }
        if ((control & I2C_MASTER_MASK) != 0u || (status & I2C_TRANSMIT_ACTIVE) != 0u ||
            (((cpu->io.i2c_slave_active & cpu->io.i2c_slave_read) & channel_bit) != 0u &&
             (control & I2C_SCLREL) != 0u)) {
            return false;
        }
        if (clock &&
            (((cpu->io.i2c_slave_active & channel_bit) != 0u && (control & I2C_SCLREL) == 0u) ||
             (cpu->io.i2c_master_active & channel_bit) != 0u)) {
            *high = false;
            return true;
        }
        if (!clock && (cpu->io.i2c_master_active & channel_bit) != 0u) {
            return false;
        }
        return dspic33_device_gpio_input_high(cpu, port, pin, high);
    }
    return false;
}

void dspic33_i2c_reset(Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        raw_write_word(cpu, (uint16_t)(base + I2C_RCV), 0u);
        raw_write_word(cpu, (uint16_t)(base + I2C_TRN), 0x00ffu);
        raw_write_word(cpu, (uint16_t)(base + I2C_BRG), 0u);
        raw_write_word(cpu, (uint16_t)(base + I2C_CON), I2C_SCLREL);
        raw_write_word(cpu, (uint16_t)(base + I2C_STAT), 0u);
        raw_write_word(cpu, (uint16_t)(base + I2C_ADD), 0u);
        raw_write_word(cpu, (uint16_t)(base + I2C_MSK), 0u);
    }
}
