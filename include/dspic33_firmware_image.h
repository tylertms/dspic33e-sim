#ifndef DSPIC33EP_MU_SIM_FIRMWARE_IMAGE_H
#define DSPIC33EP_MU_SIM_FIRMWARE_IMAGE_H

#include "elf_image.h"

typedef enum { FIRMWARE_IMAGE_ELF, FIRMWARE_IMAGE_BINARY } FirmwareImageType;

typedef struct {
    FirmwareImageType type;
    ElfImage elf;
    uint8_t* bytes;
    size_t size;
} FirmwareImage;

bool firmware_image_open(FirmwareImage* image, const char* path, char* error, size_t error_size);
void firmware_image_close(FirmwareImage* image);
bool firmware_image_load_program(const FirmwareImage* image, Dspic33* cpu, char* error,
                                 size_t error_size);
bool firmware_image_symbol(const FirmwareImage* image, const char* name, uint32_t* address,
                           char* error, size_t error_size);
Dspic33Coverage* firmware_image_create_coverage(const FirmwareImage* image, char* error,
                                                size_t error_size);

bool dspic33_load_elf_data(Dspic33* cpu, const void* image_data, size_t image_size,
                           uint32_t* entry_address);
bool dspic33_load_binary_data(Dspic33* cpu, const void* image_data, size_t image_size,
                              uint32_t load_address, uint32_t* entry_address);
bool dspic33_elf_symbol_data(const void* image_data, size_t image_size, const char* name,
                             uint32_t* address);
Dspic33Coverage* dspic33_coverage_create_elf_data(const void* image_data, size_t image_size);
Dspic33Coverage* dspic33_coverage_create_elf(const char* path);

#endif
