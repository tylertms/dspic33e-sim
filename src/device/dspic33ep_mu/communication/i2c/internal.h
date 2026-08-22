#ifndef DSPIC33EP_MU_SIM_I2C_INTERNAL_H
#define DSPIC33EP_MU_SIM_I2C_INTERNAL_H

#include "device/dspic33ep_mu/communication/i2c/api.h"
#include "device/dspic33ep_mu/device.h"

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

extern const uint16_t dspic33_i2c_bases[DSPIC33_I2C_COUNT];
extern const uint8_t dspic33_i2c_slave_irqs[DSPIC33_I2C_COUNT];
extern const uint8_t dspic33_i2c_master_irqs[DSPIC33_I2C_COUNT];

bool dspic33_i2c_internal_channel_for_address(uint16_t address, uint8_t* channel, uint16_t* offset);
bool dspic33_i2c_internal_module_disabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_i2c_internal_module_enabled(const Dspic33* cpu, uint8_t channel);
bool dspic33_i2c_internal_pin_input_high(const Dspic33* cpu, const Dspic33I2cPinMapping* mapping,
                                         bool clock);
bool dspic33_i2c_internal_pin_mapping(const Dspic33* cpu, uint8_t channel,
                                      Dspic33I2cPinMapping* mapping);
bool dspic33_i2c_internal_response_push(Dspic33I2cResponseQueue* queue,
                                        const Dspic33I2cResponse* response);
bool dspic33_i2c_internal_schedule_event(Dspic33* cpu, uint8_t channel, uint32_t value,
                                         uint64_t delay, bool external);
bool dspic33_i2c_internal_schedule_external_event(Dspic33* cpu, uint8_t channel, uint8_t kind,
                                                  uint16_t payload, uint64_t delay);
bool dspic33_i2c_internal_slave_acknowledges(uint16_t status);
bool dspic33_i2c_internal_transfer_pop(Dspic33I2cQueue* queue, Dspic33I2cTransfer* transfer);
uint16_t dspic33_i2c_internal_raw_word(const Dspic33* cpu, uint16_t address);
void dspic33_i2c_internal_begin_control(Dspic33* cpu, uint8_t channel, uint16_t operation);
void dspic33_i2c_internal_collide(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_complete_bus_status(Dspic33* cpu, uint8_t channel, uint16_t operation);
void dspic33_i2c_internal_complete_control(Dspic33* cpu, uint8_t channel, uint16_t operation);
void dspic33_i2c_internal_complete_transmit_shift(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_complete_transmit(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_pause_events(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_pin_abort(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_pin_run(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_pin_set_low(Dspic33* cpu, uint8_t channel, bool clock, bool low);
void dspic33_i2c_internal_raise_master(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_raise_slave(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);
void dspic33_i2c_internal_record_slave_acknowledgement(Dspic33* cpu, uint8_t channel,
                                                       bool acknowledge);
void dspic33_i2c_internal_record_transfer(Dspic33* cpu, uint8_t channel,
                                          Dspic33I2cTransferType type, uint16_t value,
                                          bool acknowledge, bool master);
void dspic33_i2c_internal_reset_runtime(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_resume_events(Dspic33* cpu, uint8_t channel);
void dspic33_i2c_internal_write_transmit(Dspic33* cpu, uint8_t channel, uint16_t previous,
                                         uint8_t value);

#endif
