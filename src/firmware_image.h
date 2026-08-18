#ifndef DSPIC33E_SIM_FIRMWARE_IMAGE_H
#define DSPIC33E_SIM_FIRMWARE_IMAGE_H

#include "elf_image.h"
#include "hex_image.h"

typedef enum { FIRMWARE_IMAGE_ELF, FIRMWARE_IMAGE_HEX } FirmwareImageType;

typedef struct {
    FirmwareImageType type;
    ElfImage elf;
    HexImage hex;
} FirmwareImage;

bool firmware_image_open(FirmwareImage* image, const char* path, char* error,
                         size_t error_size);
void firmware_image_close(FirmwareImage* image);
bool firmware_image_load_program(const FirmwareImage* image, Dspic33* cpu, char* error,
                                 size_t error_size);
bool firmware_image_symbol(const FirmwareImage* image, const char* name,
                           uint32_t* address, char* error, size_t error_size);

#endif
