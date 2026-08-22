#include "device/dspic33ep_mu/communication/i2c/internal.h"

static bool address_matches(const Dspic33* cpu, uint8_t channel, uint16_t address) {
    uint16_t control =
        dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON));
    uint16_t configured =
        dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_ADD));
    uint16_t mask =
        dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_MSK));
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
    uint16_t control =
        dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON));
    uint16_t configured =
        dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_ADD));
    uint16_t mask =
        dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_MSK));
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
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t address = payload & 0x03ffu;
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    bool read = (payload & I2C_EXTERNAL_READ) != 0u;
    bool ten_bit = (payload & I2C_EXTERNAL_TEN_BIT) != 0u;
    bool general_call = !ten_bit && address == 0u && !read;
    bool acknowledge = dspic33_i2c_internal_slave_acknowledges(status);
    uint8_t bit = (uint8_t)(1u << channel);
    if (!dspic33_i2c_internal_module_enabled(cpu, channel)) {
        return;
    }
    if ((cpu->io.i2c_slave_rejected & bit) != 0u) {
        return;
    }
    status = (uint16_t)((status | I2C_START_STATUS) &
                        ~(I2C_STOP_STATUS | I2C_GENERAL_CALL | I2C_TEN_BIT));
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    if (ten_bit && !ten_bit_high_matches(cpu, channel, address)) {
        dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, false);
        reject_slave(cpu, channel);
        return;
    }
    if (!ten_bit && (control & I2C_IPMIEN) == 0u &&
        ((general_call && (control & I2C_GCEN) == 0u) ||
         (!general_call && !address_matches(cpu, channel, address)))) {
        dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, false);
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
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_RCV),
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
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        dspic33_i2c_internal_raise_slave(cpu, channel);
    }
    if (ten_bit && acknowledge && schedule_ten_second &&
        !dspic33_i2c_internal_schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_TEN_SECOND,
                                                      address, 1u)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static void slave_ten_second(Dspic33* cpu, uint8_t channel, uint16_t address, bool interrupt) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint16_t configured = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_ADD));
    uint16_t mask = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_MSK));
    bool acknowledge = dspic33_i2c_internal_slave_acknowledges(status);
    uint8_t bit = (uint8_t)(1u << channel);
    if (!dspic33_i2c_internal_module_enabled(cpu, channel) ||
        (cpu->io.i2c_slave_active & bit) == 0u || cpu->io.i2c_slave_address[channel] != address) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        dspic33_i2c_internal_schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_TEN_SECOND,
                                                     address, 1u);
        return;
    }
    if ((control & I2C_IPMIEN) == 0u &&
        ((address ^ configured) & (uint16_t)~mask & 0x00ffu) != 0u) {
        dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, false);
        reject_slave(cpu, channel);
        return;
    }
    status = (uint16_t)((status | I2C_TEN_BIT) & ~(I2C_DATA | I2C_READ));
    if ((status & I2C_RBF) != 0u) {
        status |= I2C_OVERFLOW;
    } else {
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_RCV), address & 0x00ffu);
        status |= I2C_RBF;
    }
    control &= (uint16_t)~I2C_SCLREL;
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        dspic33_i2c_internal_raise_slave(cpu, channel);
    }
}

static void slave_ten_restart(Dspic33* cpu, uint8_t channel, uint16_t address, bool interrupt) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    bool acknowledge = dspic33_i2c_internal_slave_acknowledges(status);
    bool ipmi;
    if (!dspic33_i2c_internal_module_enabled(cpu, channel) ||
        (cpu->io.i2c_slave_active & bit) == 0u || cpu->io.i2c_slave_address[channel] != address ||
        (status & I2C_TEN_BIT) == 0u) {
        return;
    }
    if (!ten_bit_high_matches(cpu, channel, address)) {
        dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, false);
        reject_slave(cpu, channel);
        return;
    }
    ipmi = (control & I2C_IPMIEN) != 0u;
    cpu->io.i2c_slave_read |= bit;
    status = (uint16_t)((status | I2C_START_STATUS | I2C_READ) & ~(I2C_STOP_STATUS | I2C_DATA));
    if ((status & I2C_RBF) != 0u) {
        status |= I2C_OVERFLOW;
    } else {
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_RCV),
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
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        dspic33_i2c_internal_raise_slave(cpu, channel);
    }
}

static void slave_write(Dspic33* cpu, uint8_t channel, uint8_t value, bool interrupt) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    bool acknowledge = dspic33_i2c_internal_slave_acknowledges(status);
    uint8_t bit = (uint8_t)(1u << channel);
    if (!dspic33_i2c_internal_module_enabled(cpu, channel) ||
        (cpu->io.i2c_slave_active & bit) == 0u || (cpu->io.i2c_slave_read & bit) != 0u) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        dspic33_i2c_internal_schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_WRITE, value,
                                                     1u);
        return;
    }
    status |= I2C_DATA;
    if ((status & I2C_RBF) != 0u) {
        status |= I2C_OVERFLOW;
    } else {
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_RCV), value);
        status |= I2C_RBF;
    }
    if ((control & I2C_STREN) != 0u) {
        control &= (uint16_t)~I2C_SCLREL;
    }
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_record_slave_acknowledgement(cpu, channel, acknowledge);
    if (interrupt) {
        dspic33_i2c_internal_raise_slave(cpu, channel);
    }
}

static void slave_read(Dspic33* cpu, uint8_t channel, bool acknowledge, bool require_buffer) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    bool ten_bit = (status & I2C_TEN_BIT) != 0u;
    uint8_t value;
    if (!dspic33_i2c_internal_module_enabled(cpu, channel) ||
        (cpu->io.i2c_slave_active & bit) == 0u || (cpu->io.i2c_slave_read & bit) == 0u ||
        (require_buffer && (status & I2C_TBF) == 0u)) {
        return;
    }
    if ((control & I2C_SCLREL) == 0u) {
        dspic33_i2c_internal_schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_READ,
                                                     acknowledge ? 1u : 0u, 1u);
        return;
    }
    value = (uint8_t)dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_TRN));
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
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_record_transfer(cpu, channel, DSPIC33_I2C_WRITE, value, acknowledge,
                                         false);
    if (acknowledge || !ten_bit) {
        dspic33_i2c_internal_raise_slave(cpu, channel);
    }
}

static void slave_stop(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!dspic33_i2c_internal_module_enabled(cpu, channel)) {
        return;
    }
    status = (uint16_t)((status | I2C_STOP_STATUS) &
                        ~(I2C_START_STATUS | I2C_TEN_BIT | I2C_GENERAL_CALL | I2C_TBF |
                          I2C_TRANSMIT_ACTIVE));
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    cpu->io.i2c_slave_active &= (uint8_t)~bit;
    cpu->io.i2c_slave_read &= (uint8_t)~bit;
    cpu->io.i2c_slave_rejected &= (uint8_t)~bit;
}

void dspic33_i2c_internal_collide(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    if (!dspic33_i2c_internal_module_enabled(cpu, channel)) {
        return;
    }
    control &= (uint16_t)~I2C_MASTER_MASK;
    status = (uint16_t)((status | I2C_BUS_COLLISION) & ~(I2C_TBF | I2C_TRANSMIT_ACTIVE));
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), control);
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), status);
    dspic33_i2c_internal_pin_abort(cpu, channel);
    cpu->io.i2c_generation[channel]++;
    cpu->io.i2c_master_active &= (uint8_t)~bit;
    dspic33_i2c_internal_record_transfer(cpu, channel, DSPIC33_I2C_COLLISION, 0u, false, true);
    dspic33_i2c_internal_raise_master(cpu, channel);
}

static bool slave_pin_operating(const Dspic33* cpu, uint8_t channel) {
    return dspic33_i2c_internal_module_enabled(cpu, channel) &&
           !(cpu->power_state == DSPIC33_POWER_IDLE &&
             (dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON)) &
              0x2000u) != 0u);
}

static bool resolved_pin_high(const Dspic33* cpu, uint8_t channel,
                              const Dspic33I2cPinMapping* mapping, bool clock) {
    uint8_t bit = (uint8_t)(1u << channel);
    if (((clock ? cpu->io.i2c_pin_clock_low : cpu->io.i2c_pin_data_low) & bit) != 0u) {
        return false;
    }
    return dspic33_i2c_internal_pin_input_high(cpu, mapping, clock);
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
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit_index = cpu->io.i2c_slave_pin_bits[channel];
    if (cpu->io.i2c_slave_pin_state[channel] != I2C_SLAVE_PIN_TRANSMIT) {
        return;
    }
    if (bit_index >= 8u || (status & I2C_TBF) == 0u) {
        dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
        return;
    }
    dspic33_i2c_internal_pin_set_low(
        cpu, channel, false,
        (dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_TRN)) &
         (uint16_t)(0x0080u >> bit_index)) == 0u);
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
    dspic33_i2c_internal_pin_set_low(cpu, channel, true, false);
    dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
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
    dspic33_i2c_internal_pin_set_low(cpu, channel, true, false);
    dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
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
    uint16_t base = dspic33_i2c_bases[channel];
    uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
    uint8_t bit = (uint8_t)(1u << channel);
    uint8_t state = cpu->io.i2c_slave_pin_next[channel];
    bool acknowledge;
    bool interrupt = false;
    uint16_t resulting_control;
    acknowledge = dspic33_i2c_internal_slave_acknowledges(status);
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
    resulting_control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
    if ((resulting_control & I2C_SCLREL) == 0u) {
        cpu->io.i2c_slave_pin_stretch |= bit;
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON),
                                            (uint16_t)(resulting_control | I2C_SCLREL));
    } else {
        cpu->io.i2c_slave_pin_stretch &= (uint8_t)~bit;
    }
    cpu->io.i2c_slave_pin_state[channel] = I2C_SLAVE_PIN_ACKNOWLEDGE;
    cpu->io.i2c_slave_pin_bits[channel] = 0u;
    cpu->io.i2c_slave_pin_shift[channel] = 0u;
    dspic33_i2c_internal_pin_set_low(cpu, channel, false, acknowledge);
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
            dspic33_i2c_internal_pin_set_low(
                cpu, channel, false,
                (cpu->io.i2c_slave_pin_acknowledge & (uint8_t)(1u << channel)) != 0u);
            cpu->io.i2c_slave_pin_bits[channel] = 1u;
        } else if (cpu->io.i2c_slave_pin_bits[channel] == 2u) {
            dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
            cpu->io.i2c_slave_pin_state[channel] = cpu->io.i2c_slave_pin_next[channel];
            cpu->io.i2c_slave_pin_bits[channel] = 0u;
            if ((cpu->io.i2c_slave_pin_stretch & bit) != 0u) {
                uint16_t base = dspic33_i2c_bases[channel];
                uint16_t control = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_CON));
                cpu->io.i2c_slave_pin_stretch &= (uint8_t)~bit;
                dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON),
                                                    (uint16_t)(control & ~I2C_SCLREL));
                dspic33_i2c_internal_pin_set_low(cpu, channel, true, true);
            }
            if ((cpu->io.i2c_slave_pin_interrupt & bit) != 0u) {
                cpu->io.i2c_slave_pin_interrupt &= (uint8_t)~bit;
                dspic33_i2c_internal_raise_slave(cpu, channel);
            }
            slave_pin_prepare_transmit(cpu, channel);
        }
    } else if (state == I2C_SLAVE_PIN_TRANSMIT) {
        slave_pin_prepare_transmit(cpu, channel);
    } else if (state == I2C_SLAVE_PIN_MASTER_ACKNOWLEDGE) {
        if (cpu->io.i2c_slave_pin_bits[channel] == 0u) {
            uint16_t base = dspic33_i2c_bases[channel];
            uint16_t status = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(base + I2C_STAT));
            dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT),
                                                (uint16_t)(status & ~I2C_TBF));
            dspic33_i2c_internal_pin_set_low(cpu, channel, false, false);
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
        if (!dspic33_i2c_internal_pin_mapping(cpu, channel, &mapping)) {
            continue;
        }
        if ((cpu->io.i2c_pin_active & bit) == 0u && (cpu->io.i2c_slave_active & bit) != 0u) {
            dspic33_i2c_internal_pin_set_low(
                cpu, channel, true,
                (dspic33_i2c_internal_raw_word(cpu,
                                               (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON)) &
                 I2C_SCLREL) == 0u);
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
    if (!dspic33_i2c_internal_channel_for_address(base, &channel, &offset)) {
        return false;
    }
    if (dspic33_i2c_internal_module_disabled(cpu, channel)) {
        dspic33_i2c_internal_raw_write_word(cpu, base, previous);
        return true;
    }
    if (offset == I2C_RCV) {
        dspic33_i2c_internal_raw_write_word(cpu, base, previous);
    } else if (offset == I2C_TRN) {
        if ((address & 1u) == 0u || cpu->io.cpu_write_width == 2u) {
            dspic33_i2c_internal_write_transmit(cpu, channel, previous, (uint8_t)requested);
        } else {
            dspic33_i2c_internal_raw_write_word(cpu, base, previous);
        }
    } else if (offset == I2C_BRG) {
        dspic33_i2c_internal_raw_write_word(cpu, base, requested & 0x01ffu);
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
        dspic33_i2c_internal_raw_write_word(cpu, base, control);
        if (disabling) {
            dspic33_i2c_internal_raw_write_word(
                cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_STAT), 0u);
            dspic33_i2c_internal_reset_runtime(cpu, channel);
        } else if ((control & I2C_ENABLE) != 0u && active == 0u) {
            dspic33_i2c_internal_begin_control(cpu, channel, control & I2C_MASTER_MASK);
        }
    } else if (offset == I2C_STAT) {
        dspic33_i2c_internal_raw_write_word(
            cpu, base, (uint16_t)((previous & ~0x04c0u) | (previous & requested & 0x04c0u)));
    } else if (offset == I2C_ADD || offset == I2C_MSK) {
        dspic33_i2c_internal_raw_write_word(cpu, base, requested & 0x03ffu);
    }
    dspic33_i2c_refresh_pins(cpu);
    return true;
}

bool dspic33_i2c_read_register(Dspic33* cpu, uint16_t address, uint8_t* value) {
    uint8_t channel;
    uint16_t offset;
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (!dspic33_i2c_internal_channel_for_address(base, &channel, &offset)) {
        return false;
    }
    if (offset == I2C_TRN) {
        *value = 0u;
    } else {
        *value = cpu->data[address];
    }
    if (offset == I2C_RCV) {
        dspic33_i2c_internal_raw_write_word(
            cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_STAT),
            (uint16_t)(dspic33_i2c_internal_raw_word(
                           cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_STAT)) &
                       ~I2C_RBF));
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
            dspic33_i2c_internal_pause_events(cpu, channel);
        } else {
            cpu->io.i2c_pmd_disabled &= (uint8_t)~bit;
            dspic33_i2c_internal_resume_events(cpu, channel);
        }
        dspic33_i2c_refresh_pins(cpu);
        return;
    }
    if ((kind <= I2C_EVENT_TRANSMIT_SHIFT || kind == I2C_EVENT_PIN) &&
        generation != cpu->io.i2c_generation[channel]) {
        return;
    }
    if (cpu->power_state == DSPIC33_POWER_IDLE &&
        (dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON)) &
         0x2000u) != 0u) {
        dspic33_i2c_internal_schedule_event(cpu, channel, value, 1u, external);
        return;
    }
    if (kind == I2C_EVENT_CONTROL) {
        dspic33_i2c_internal_complete_control(cpu, channel, payload);
    } else if (kind == I2C_EVENT_BUS_STATUS) {
        dspic33_i2c_internal_complete_bus_status(cpu, channel, payload);
    } else if (kind == I2C_EVENT_TRANSMIT) {
        dspic33_i2c_internal_complete_transmit(cpu, channel);
    } else if (kind == I2C_EVENT_TRANSMIT_SHIFT) {
        dspic33_i2c_internal_complete_transmit_shift(cpu, channel);
    } else if (kind == I2C_EVENT_SLAVE_START) {
        slave_start(cpu, channel, payload, true, true);
    } else if (kind == I2C_EVENT_SLAVE_WRITE) {
        slave_write(cpu, channel, (uint8_t)payload, true);
    } else if (kind == I2C_EVENT_SLAVE_READ) {
        slave_read(cpu, channel, payload != 0u, true);
    } else if (kind == I2C_EVENT_SLAVE_STOP) {
        slave_stop(cpu, channel);
    } else if (kind == I2C_EVENT_COLLISION) {
        dspic33_i2c_internal_collide(cpu, channel);
    } else if (kind == I2C_EVENT_SLAVE_TEN_SECOND) {
        slave_ten_second(cpu, channel, payload & 0x03ffu, true);
    } else if (kind == I2C_EVENT_SLAVE_TEN_RESTART) {
        slave_ten_restart(cpu, channel, payload & 0x03ffu, true);
    } else if (kind == I2C_EVENT_PIN) {
        dspic33_i2c_internal_pin_run(cpu, channel);
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
    return dspic33_i2c_internal_response_push(&cpu->io.i2c_response[channel], &response);
}

bool dspic33_i2c_status(Dspic33* cpu, uint8_t channel, uint16_t status) {
    const uint16_t hardware_status = 0x84c0u;
    uint16_t value;
    if (channel >= DSPIC33_I2C_COUNT || (status & (uint16_t)~hardware_status) != 0u) {
        return false;
    }
    value = dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_STAT));
    dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_STAT),
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
            (dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_STAT)) &
             I2C_TEN_BIT) == 0u) {
            return false;
        }
        return dspic33_i2c_internal_schedule_external_event(
            cpu, channel, I2C_EVENT_SLAVE_TEN_RESTART, address, delay);
    }
    payload = (uint16_t)(address | (read ? I2C_EXTERNAL_READ : 0u) |
                         (ten_bit ? I2C_EXTERNAL_TEN_BIT : 0u));
    return dspic33_i2c_internal_schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_START,
                                                        payload, delay);
}

bool dspic33_i2c_slave_write(Dspic33* cpu, uint8_t channel, uint8_t value, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT && dspic33_i2c_internal_schedule_external_event(
                                              cpu, channel, I2C_EVENT_SLAVE_WRITE, value, delay);
}

bool dspic33_i2c_slave_read(Dspic33* cpu, uint8_t channel, bool acknowledge, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT &&
           dspic33_i2c_internal_schedule_external_event(cpu, channel, I2C_EVENT_SLAVE_READ,
                                                        acknowledge ? 1u : 0u, delay);
}

bool dspic33_i2c_slave_stop(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT && dspic33_i2c_internal_schedule_external_event(
                                              cpu, channel, I2C_EVENT_SLAVE_STOP, 0u, delay);
}

bool dspic33_i2c_collision(Dspic33* cpu, uint8_t channel, uint64_t delay) {
    return channel < DSPIC33_I2C_COUNT && dspic33_i2c_internal_schedule_external_event(
                                              cpu, channel, I2C_EVENT_COLLISION, 0u, delay);
}

bool dspic33_i2c_transmit(Dspic33* cpu, uint8_t channel, Dspic33I2cTransfer* transfer) {
    return channel < DSPIC33_I2C_COUNT && transfer != NULL &&
           dspic33_i2c_internal_transfer_pop(&cpu->io.i2c_tx[channel], transfer);
}

bool dspic33_i2c_pin(const Dspic33* cpu, uint8_t port, uint8_t pin, bool* high) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        Dspic33I2cPinMapping mapping;
        uint16_t control;
        uint16_t status;
        uint8_t channel_bit;
        bool clock;
        if (!dspic33_i2c_internal_pin_mapping(cpu, channel, &mapping) || mapping.port != port ||
            (mapping.clock != pin && mapping.data != pin) ||
            !dspic33_i2c_internal_module_enabled(cpu, channel)) {
            continue;
        }
        if (high == NULL) {
            return false;
        }
        control =
            dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_CON));
        status =
            dspic33_i2c_internal_raw_word(cpu, (uint16_t)(dspic33_i2c_bases[channel] + I2C_STAT));
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
        uint16_t base = dspic33_i2c_bases[channel];
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_RCV), 0u);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_TRN), 0x00ffu);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_BRG), 0u);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_CON), I2C_SCLREL);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_STAT), 0u);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_ADD), 0u);
        dspic33_i2c_internal_raw_write_word(cpu, (uint16_t)(base + I2C_MSK), 0u);
    }
}
