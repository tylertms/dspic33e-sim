#include "firmware_image.h"

#include <stdio.h>
#include <string.h>

static bool is_elf(const char* path) {
    unsigned char magic[4];
    FILE* file = fopen(path, "rb");
    bool result = false;
    if (file != NULL) {
        result = fread(magic, 1u, sizeof(magic), file) == sizeof(magic) &&
                 magic[0] == 0x7fu && magic[1] == 'E' && magic[2] == 'L' &&
                 magic[3] == 'F';
        fclose(file);
    }
    return result;
}

bool firmware_image_open(FirmwareImage* image, const char* path, char* error,
                         size_t error_size) {
    memset(image, 0, sizeof(*image));
    if (is_elf(path)) {
        image->type = FIRMWARE_IMAGE_ELF;
        return elf_image_open(&image->elf, path, error, error_size);
    }
    image->type = FIRMWARE_IMAGE_HEX;
    return hex_image_open(&image->hex, path, error, error_size);
}

void firmware_image_close(FirmwareImage* image) {
    if (image->type == FIRMWARE_IMAGE_ELF) {
        elf_image_close(&image->elf);
    } else {
        hex_image_close(&image->hex);
    }
}

bool firmware_image_load_program(const FirmwareImage* image, Dspic33* cpu, char* error,
                                 size_t error_size) {
    if (image->type == FIRMWARE_IMAGE_ELF) {
        return elf_image_load_program(&image->elf, cpu, error, error_size);
    }
    return hex_image_load_program(&image->hex, cpu, error, error_size);
}

bool firmware_image_symbol(const FirmwareImage* image, const char* name,
                           uint32_t* address, char* error, size_t error_size) {
    if (image->type == FIRMWARE_IMAGE_ELF) {
        return elf_image_symbol(&image->elf, name, address, error, error_size);
    }
    if (error_size != 0u) {
        snprintf(error, error_size, "Intel HEX images do not contain symbols");
    }
    return false;
}
