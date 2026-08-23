#include "dspic33_firmware_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool is_elf_file(const char* path) {
    unsigned char magic[4];
    FILE* file = fopen(path, "rb");
    bool is_elf_file = false;
    if (file != NULL) {
        is_elf_file = fread(magic, 1u, sizeof(magic), file) == sizeof(magic) && magic[0] == 0x7fu &&
                      magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F';
        fclose(file);
    }
    return is_elf_file;
}

static bool read_file(const char* path, uint8_t** bytes, size_t* size) {
    FILE* file = fopen(path, "rb");
    long length;
    if (file == NULL || fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        if (file != NULL) {
            fclose(file);
        }
        return false;
    }
    *bytes = malloc((size_t)length);
    if (*bytes == NULL || fread(*bytes, 1u, (size_t)length, file) != (size_t)length) {
        free(*bytes);
        *bytes = NULL;
        fclose(file);
        return false;
    }
    fclose(file);
    *size = (size_t)length;
    return true;
}

bool firmware_image_open(FirmwareImage* image, const char* path, char* error, size_t error_size) {
    memset(image, 0, sizeof(*image));
    if (is_elf_file(path)) {
        image->type = FIRMWARE_IMAGE_ELF;
        return elf_image_open(&image->elf, path, error, error_size);
    }
    image->type = FIRMWARE_IMAGE_BINARY;
    if (!read_file(path, &image->bytes, &image->size)) {
        if (error_size != 0u) {
            snprintf(error, error_size, "cannot read firmware image");
        }
        return false;
    }
    return true;
}

void firmware_image_close(FirmwareImage* image) {
    if (image->type == FIRMWARE_IMAGE_ELF) {
        elf_image_close(&image->elf);
    } else {
        free(image->bytes);
        image->bytes = NULL;
        image->size = 0u;
    }
}

bool firmware_image_load_program(const FirmwareImage* image, Dspic33* cpu, char* error,
                                 size_t error_size) {
    if (image->type == FIRMWARE_IMAGE_ELF) {
        return elf_image_load_program(&image->elf, cpu, error, error_size);
    }
    uint32_t entry_address;
    if (!dspic33_load_binary_data(cpu, image->bytes, image->size, 0u, &entry_address)) {
        if (error_size != 0u) {
            snprintf(error, error_size, "binary firmware image is invalid");
        }
        return false;
    }
    return true;
}

bool firmware_image_symbol(const FirmwareImage* image, const char* name, uint32_t* address,
                           char* error, size_t error_size) {
    if (image->type == FIRMWARE_IMAGE_ELF) {
        return elf_image_symbol(&image->elf, name, address, error, error_size);
    }
    if (error_size != 0u) {
        snprintf(error, error_size, "binary images do not contain symbols");
    }
    return false;
}

bool dspic33_load_elf_data(Dspic33* cpu, const void* image_data, size_t image_size,
                           uint32_t* entry_address) {
    ElfImage image;
    char error[160];
    if (cpu == NULL || !elf_image_open_data(&image, image_data, image_size, error, sizeof(error))) {
        return false;
    }
    const bool loaded = elf_image_load_program(&image, cpu, error, sizeof(error));
    if (loaded && entry_address != NULL) {
        const uint8_t* bytes = image.bytes + 24u;
        *entry_address = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) |
                         ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
    }
    elf_image_close(&image);
    return loaded;
}

bool dspic33_load_binary_data(Dspic33* cpu, const void* image_data, size_t image_size,
                              uint32_t load_address, uint32_t* entry_address) {
    const uint8_t* bytes = image_data;
    size_t byte_offset;
    if (cpu == NULL || bytes == NULL || image_size == 0u || (image_size & 3u) != 0u ||
        (load_address & 3u) != 0u || image_size > UINT32_MAX - load_address) {
        return false;
    }
    for (byte_offset = 0u; byte_offset < image_size; byte_offset += 4u) {
        uint32_t storage = load_address + (uint32_t)byte_offset;
        uint32_t program_address = storage / 2u;
        uint32_t word = (uint32_t)bytes[byte_offset] | ((uint32_t)bytes[byte_offset + 1u] << 8u) |
                        ((uint32_t)bytes[byte_offset + 2u] << 16u);
        if (word == 0x00ffffffu && bytes[byte_offset + 3u] == 0xffu) {
            continue;
        }
        if (dspic33_program_range_implemented(program_address, 2u) &&
            !dspic33_load_program_word(cpu, program_address, word)) {
            return false;
        }
        if (program_address >= DSPIC33_CONFIGURATION_BASE &&
            program_address < DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE &&
            !dspic33_load_configuration_word(cpu, program_address, word)) {
            return false;
        }
    }
    if (entry_address != NULL) {
        *entry_address = load_address / 2u;
    }
    return true;
}

bool dspic33_elf_symbol_data(const void* image_data, size_t image_size, const char* name,
                             uint32_t* address) {
    ElfImage image;
    char error[160];
    if (name == NULL || address == NULL ||
        !elf_image_open_data(&image, image_data, image_size, error, sizeof(error))) {
        return false;
    }
    const bool found = elf_image_symbol(&image, name, address, error, sizeof(error));
    elf_image_close(&image);
    return found;
}
