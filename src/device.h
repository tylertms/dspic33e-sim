#ifndef OPENTEC_DSPIC33_DEVICE_H
#define OPENTEC_DSPIC33_DEVICE_H

#include "dspic33.h"

void dspic33_device_reset(Dspic33* cpu);
void dspic33_device_write_byte(Dspic33* cpu, uint16_t address);
uint8_t dspic33_device_read_byte(Dspic33* cpu, uint16_t address, uint8_t value);
bool dspic33_device_advance(Dspic33* cpu, uint64_t cycles);
bool dspic33_device_service_interrupt(Dspic33* cpu);
void dspic33_device_return_interrupt(Dspic33* cpu);

#endif
