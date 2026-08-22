#ifndef DSPIC33EP_MU_SIM_ELF_IMAGE_H
#define DSPIC33EP_MU_SIM_ELF_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dspic33.h"

typedef struct {
    uint8_t* bytes;
    size_t size;
} ElfImage;

bool elf_image_open(ElfImage* image, const char* path, char* error, size_t error_size);
bool elf_image_open_data(ElfImage* image, const void* data, size_t size, char* error,
                         size_t error_size);
void elf_image_close(ElfImage* image);
bool elf_image_load_program(const ElfImage* image, Dspic33* cpu, char* error, size_t error_size);
bool elf_image_symbol(const ElfImage* image, const char* name, uint32_t* address, char* error,
                      size_t error_size);

#endif
