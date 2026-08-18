#ifndef DSPIC33E_SIM_HEX_IMAGE_H
#define DSPIC33E_SIM_HEX_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dspic33.h"

typedef struct {
    uint8_t* bytes;
    size_t size;
    bool encrypted;
} HexImage;

bool hex_image_open(HexImage* image, const char* path, char* error, size_t error_size);
void hex_image_close(HexImage* image);
bool hex_image_load_program(const HexImage* image, Dspic33* cpu, char* error,
                            size_t error_size);

#endif
