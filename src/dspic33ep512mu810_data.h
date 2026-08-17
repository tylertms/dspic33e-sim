#ifndef DSPIC33EP512MU810_DATA_H
#define DSPIC33EP512MU810_DATA_H

#include <stdbool.h>
#include <stdint.h>

enum {
    DSPIC33_SFR_WORD_COUNT = 2048u,
    DSPIC33_SFR_IMPLEMENTATION_BITMAP_SIZE = 256u,
    DSPIC33_SFR_IMPLEMENTED_WORD_COUNT = 977u,
    DSPIC33_SFR_ABSENT_WORD_COUNT = 1071u,
    DSPIC33_SFR_ABSENT_RANGE_COUNT = 77u,
    DSPIC33_SFR_MASTER_CLEAR_RESET_COUNT = 49u,
};

typedef struct {
    uint16_t address;
    uint16_t known_mask;
    uint16_t value;
    uint16_t unchanged;
} Dspic33SfrMasterClearReset;

extern const uint8_t
    dspic33_sfr_implementation_bitmap[DSPIC33_SFR_IMPLEMENTATION_BITMAP_SIZE];
extern const Dspic33SfrMasterClearReset
    dspic33_sfr_master_clear_resets[DSPIC33_SFR_MASTER_CLEAR_RESET_COUNT];

bool dspic33ep512mu810_address_implemented(uint32_t address);

#endif
