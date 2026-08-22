#ifndef DSPIC33EP_MU_SIM_DEVICE_DATA_H
#define DSPIC33EP_MU_SIM_DEVICE_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dspic33.h"

enum {
    DSPIC33_SFR_WORD_COUNT = 2048u,
    DSPIC33_SFR_IMPLEMENTATION_BITMAP_SIZE = 256u,
};

typedef struct {
    uint16_t address;
    uint16_t known_mask;
    uint16_t value;
    uint16_t unchanged;
} Dspic33SfrMasterClearReset;

bool dspic33ep_mu_address_implemented(Dspic33epMuDevice device, uint32_t address);
uint16_t dspic33ep_mu_gpio_port_mask(Dspic33epMuDevice device, uint8_t port);
const uint8_t* dspic33ep_mu_implementation_bitmap(Dspic33epMuDevice device);
const Dspic33SfrMasterClearReset* dspic33ep_mu_master_clear_resets(Dspic33epMuDevice device,
                                                                   size_t* count);

#endif
