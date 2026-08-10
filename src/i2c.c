#include "i2c.h"

#include <string.h>

#include "device.h"

static const uint16_t bases[DSPIC33_I2C_COUNT] = {0x0200u, 0x0210u};
static const uint8_t slave_irqs[DSPIC33_I2C_COUNT] = {16u, 49u};
static const uint8_t master_irqs[DSPIC33_I2C_COUNT] = {17u, 50u};

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
    I2C_EXTERNAL_READ = 0x00000800u,
    I2C_EXTERNAL_TEN_BIT = 0x00000400u
};

static uint16_t raw_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] |
                      ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
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
        if (event->type != DSPIC33_EVENT_I2C || event->source != channel ||
            event->paused || kind > I2C_EVENT_TRANSMIT_SHIFT) {
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
        if (event->type != DSPIC33_EVENT_I2C || event->source != channel ||
            !event->paused) {
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
        if (!dspic33_schedule(cpu, DSPIC33_EVENT_I2C, channel,
                              ((uint32_t)I2C_EVENT_PMD << I2C_EVENT_KIND_SHIFT) |
                                  ((uint32_t)cpu->io.i2c_pmd_generation[channel]
                                   << I2C_EVENT_GENERATION_SHIFT) |
                                  (disabled ? 1u : 0u),
                              1u)) {
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

static bool response_push(Dspic33I2cResponseQueue* queue,
                          const Dspic33I2cResponse* response) {
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
        uint8_t prior =
            (uint8_t)((queue->head + logical - 1u) % DSPIC33_I2C_QUEUE_SIZE);
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

static bool response_wait(const Dspic33I2cResponseQueue* queue, uint64_t cycle,
                          uint64_t* delay) {
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

static uint64_t operation_cycles(const Dspic33* cpu, uint8_t channel,
                                 uint8_t half_periods) {
    uint64_t scaled =
        (uint64_t)(raw_word(cpu, (uint16_t)(bases[channel] + I2C_BRG)) + 2u) *
        half_periods;
    return (scaled + 1u) / 2u;
}

static uint32_t internal_event_value(const Dspic33* cpu, uint8_t channel, uint8_t kind,
                                     uint16_t payload) {
    return ((uint32_t)kind << I2C_EVENT_KIND_SHIFT) |
           ((uint32_t)cpu->io.i2c_generation[channel] << I2C_EVENT_GENERATION_SHIFT) |
           payload;
}

static bool schedule_event(Dspic33* cpu, uint8_t channel, uint32_t value,
                           uint64_t delay) {
    uint64_t sequence = cpu->events.sequence;
    Dspic33Event* event;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_I2C, channel, value, delay)) {
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

static bool schedule_internal(Dspic33* cpu, uint8_t channel, uint8_t kind,
                              uint16_t payload, uint64_t delay) {
    return schedule_event(cpu, channel,
                          internal_event_value(cpu, channel, kind, payload), delay);
}

static bool schedule_external(Dspic33* cpu, uint8_t channel, uint8_t kind,
                              uint16_t payload, uint64_t delay) {
    return schedule_event(cpu, channel,
                          ((uint32_t)kind << I2C_EVENT_KIND_SHIFT) | payload, delay);
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
    cpu->io.i2c_slave_address[channel] = 0u;
    memset(&cpu->io.i2c_response[channel], 0, sizeof(cpu->io.i2c_response[channel]));
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
        uint64_t periods = 2u;
        if (selected == I2C_RCEN) {
            periods = 16u;
        } else if (selected == I2C_RSEN || selected == I2C_PEN) {
            periods = 3u;
        }
        if (schedule_internal(cpu, channel, I2C_EVENT_CONTROL, selected,
                              operation_cycles(cpu, channel, periods)) &&
            (selected == I2C_SEN || selected == I2C_RSEN || selected == I2C_PEN)) {
            uint8_t status_periods = selected == I2C_SEN ? 1u : 2u;
            schedule_internal(cpu, channel, I2C_EVENT_BUS_STATUS, selected,
                              operation_cycles(cpu, channel, status_periods));
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

static void write_transmit(Dspic33* cpu, uint8_t channel, uint16_t previous,
                           uint8_t value) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel)) {
        raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        return;
    }
    if ((control & I2C_MASTER_MASK) != 0u ||
        (status & (I2C_TBF | I2C_TRANSMIT_ACTIVE)) != 0u) {
        raw_write_word(cpu, (uint16_t)(base + I2C_TRN), previous);
        raw_write_word(cpu, (uint16_t)(base + I2C_STAT),
                       (uint16_t)(status | I2C_WRITE_COLLISION));
        return;
    }
    raw_write_word(cpu, (uint16_t)(base + I2C_TRN), value);
    status |= I2C_TBF;
    if ((cpu->io.i2c_slave_active & bit) != 0u &&
        (cpu->io.i2c_slave_read & bit) != 0u) {
        raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
        return;
    }
    cpu->io.i2c_master_active |= bit;
    status |= I2C_TRANSMIT_ACTIVE;
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    record_transfer(cpu, channel, DSPIC33_I2C_WRITE, value, false, true);
    schedule_internal(cpu, channel, I2C_EVENT_TRANSMIT_SHIFT, value,
                      operation_cycles(cpu, channel, 16u));
    schedule_internal(cpu, channel, I2C_EVENT_TRANSMIT, value,
                      operation_cycles(cpu, channel, 18u));
}

static void complete_control(Dspic33* cpu, uint8_t channel, uint16_t operation) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel) || (control & operation) == 0u) {
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
                        operation == I2C_SEN ? DSPIC33_I2C_START : DSPIC33_I2C_RESTART,
                        0u, false, true);
    } else if (operation == I2C_PEN) {
        status = (uint16_t)((status | I2C_STOP_STATUS) & ~I2C_START_STATUS);
        cpu->io.i2c_master_active &= (uint8_t)~bit;
        record_transfer(cpu, channel, DSPIC33_I2C_STOP, 0u, false, true);
    } else if (operation == I2C_RCEN) {
        Dspic33I2cResponse response;
        uint8_t received = 0xffu;
        if (response_pop(&cpu->io.i2c_response[channel], cpu->device_cycles,
                         &response)) {
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
        record_transfer(cpu, channel, DSPIC33_I2C_ACKNOWLEDGE, 0u,
                        (control & I2C_ACKDT) == 0u, true);
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
    return ((address ^ configured) & (uint16_t)~mask & 0x007fu) == 0u;
}

static bool ten_bit_high_matches(const Dspic33* cpu, uint8_t channel,
                                 uint16_t address) {
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

static void slave_start(Dspic33* cpu, uint8_t channel, uint16_t payload) {
    uint16_t base = bases[channel];
    uint16_t address = payload & 0x03ffu;
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    bool read = (payload & I2C_EXTERNAL_READ) != 0u;
    bool ten_bit = (payload & I2C_EXTERNAL_TEN_BIT) != 0u;
    bool general_call = !ten_bit && address == 0u && !read;
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
        reject_slave(cpu, channel);
        return;
    }
    if (!ten_bit && (!general_call || (control & I2C_GCEN) == 0u) &&
        !address_matches(cpu, channel, address)) {
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
    raise_slave(cpu, channel);
}

static void slave_ten_second(Dspic33* cpu, uint8_t channel, uint16_t address) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint16_t configured = raw_word(cpu, (uint16_t)(base + I2C_ADD));
    uint16_t mask = raw_word(cpu, (uint16_t)(base + I2C_MSK));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        cpu->io.i2c_slave_address[channel] != address) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        schedule_external(cpu, channel, I2C_EVENT_SLAVE_TEN_SECOND, address, 1u);
        return;
    }
    if ((control & I2C_IPMIEN) == 0u &&
        ((address ^ configured) & (uint16_t)~mask & 0x00ffu) != 0u) {
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
    raise_slave(cpu, channel);
}

static void slave_ten_restart(Dspic33* cpu, uint8_t channel, uint16_t address) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    bool ipmi;
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        cpu->io.i2c_slave_address[channel] != address || (status & I2C_TEN_BIT) == 0u ||
        !ten_bit_high_matches(cpu, channel, address)) {
        return;
    }
    ipmi = (control & I2C_IPMIEN) != 0u;
    cpu->io.i2c_slave_read |= bit;
    status = (uint16_t)((status | I2C_START_STATUS | I2C_READ) &
                        ~(I2C_STOP_STATUS | I2C_DATA));
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
    raise_slave(cpu, channel);
}

static void slave_write(Dspic33* cpu, uint8_t channel, uint8_t value) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        (cpu->io.i2c_slave_read & bit) != 0u) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        schedule_external(cpu, channel, I2C_EVENT_SLAVE_WRITE, value, 1u);
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
    raise_slave(cpu, channel);
}

static void slave_read(Dspic33* cpu, uint8_t channel, bool acknowledge) {
    uint16_t base = bases[channel];
    uint16_t control = raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    bool ten_bit = (status & I2C_TEN_BIT) != 0u;
    uint8_t value;
    if (!module_enabled(cpu, channel) || (cpu->io.i2c_slave_active & bit) == 0u ||
        (cpu->io.i2c_slave_read & bit) == 0u || (status & I2C_TBF) == 0u) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        schedule_external(cpu, channel, I2C_EVENT_SLAVE_READ, acknowledge ? 1u : 0u,
                          1u);
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
    status =
        (uint16_t)((status | I2C_BUS_COLLISION) & ~(I2C_TBF | I2C_TRANSMIT_ACTIVE));
    raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    cpu->io.i2c_generation[channel]++;
    cpu->io.i2c_master_active &= (uint8_t)~bit;
    record_transfer(cpu, channel, DSPIC33_I2C_COLLISION, 0u, false, true);
    raise_master(cpu, channel);
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
        raw_write_word(
            cpu, base,
            (uint16_t)((previous & ~0x04c0u) | (previous & requested & 0x04c0u)));
    } else if (offset == I2C_ADD || offset == I2C_MSK) {
        raw_write_word(cpu, base, requested & 0x03ffu);
    }
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
                       (uint16_t)(raw_word(cpu, (uint16_t)(bases[channel] + I2C_STAT)) &
                                  ~I2C_RBF));
    }
    return true;
}

void dspic33_i2c_process_event(Dspic33* cpu, uint8_t channel, uint32_t value) {
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
        return;
    }
    if (kind <= I2C_EVENT_TRANSMIT_SHIFT &&
        generation != cpu->io.i2c_generation[channel]) {
        return;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE &&
        (raw_word(cpu, (uint16_t)(bases[channel] + I2C_CON)) & 0x2000u) != 0u) {
        schedule_event(cpu, channel, value, 1u);
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
        slave_start(cpu, channel, payload);
    } else if (kind == I2C_EVENT_SLAVE_WRITE) {
        slave_write(cpu, channel, (uint8_t)payload);
    } else if (kind == I2C_EVENT_SLAVE_READ) {
        slave_read(cpu, channel, payload != 0u);
    } else if (kind == I2C_EVENT_SLAVE_STOP) {
        slave_stop(cpu, channel);
    } else if (kind == I2C_EVENT_COLLISION) {
        collide(cpu, channel);
    } else if (kind == I2C_EVENT_SLAVE_TEN_SECOND) {
        slave_ten_second(cpu, channel, payload & 0x03ffu);
    } else if (kind == I2C_EVENT_SLAVE_TEN_RESTART) {
        slave_ten_restart(cpu, channel, payload & 0x03ffu);
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
            (raw_word(cpu, (uint16_t)(bases[channel] + I2C_STAT)) & I2C_TEN_BIT) ==
                0u) {
            return false;
        }
        return schedule_external(cpu, channel, I2C_EVENT_SLAVE_TEN_RESTART, address,
                                 delay);
    }
    payload = (uint16_t)(address | (read ? I2C_EXTERNAL_READ : 0u) |
                         (ten_bit ? I2C_EXTERNAL_TEN_BIT : 0u));
    if (ten_bit) {
        if (delay == UINT64_MAX ||
            !schedule_external(cpu, channel, I2C_EVENT_SLAVE_TEN_SECOND, address,
                               delay + 1u)) {
            return false;
        }
    }
    return schedule_external(cpu, channel, I2C_EVENT_SLAVE_START, payload, delay);
}

bool dspic33_i2c_slave_write(Dspic33* cpu, uint8_t channel, uint8_t value,
                             uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external(cpu, channel, I2C_EVENT_SLAVE_WRITE, value, delay);
}

bool dspic33_i2c_slave_read(Dspic33* cpu, uint8_t channel, bool acknowledge,
                            uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external(cpu, channel, I2C_EVENT_SLAVE_READ, acknowledge ? 1u : 0u,
                             delay);
}

bool dspic33_i2c_slave_stop(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external(cpu, channel, I2C_EVENT_SLAVE_STOP, 0u, delay);
}

bool dspic33_i2c_collision(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           schedule_external(cpu, channel, I2C_EVENT_COLLISION, 0u, delay);
}

bool dspic33_i2c_transmit(Dspic33* cpu, uint8_t channel, Dspic33I2cTransfer* transfer) {
    return channel < DSPIC33_I2C_COUNT && transfer != NULL &&
           transfer_pop(&cpu->io.i2c_tx[channel], transfer);
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
