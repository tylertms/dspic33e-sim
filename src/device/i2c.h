#ifndef DSPIC33EP_MU_SIM_DSPIC33_I2C_H
#define DSPIC33EP_MU_SIM_DSPIC33_I2C_H

#include "dspic33_internal.h"

bool dspic33_i2c_write_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                uint16_t requested);
void dspic33_i2c_update_pmd(Dspic33* cpu, uint16_t address, uint16_t previous);
bool dspic33_i2c_read_register(Dspic33* cpu, uint16_t address, uint8_t* value);
void dspic33_i2c_process_event(Dspic33* cpu, uint8_t channel, uint32_t value, bool external);
void dspic33_i2c_refresh_pins(Dspic33* cpu);
void dspic33_i2c_reset(Dspic33* cpu);

#endif
