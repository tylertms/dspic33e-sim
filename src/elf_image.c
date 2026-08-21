#include "elf_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ELF_HEADER_SIZE = 52,
    ELF_SECTION_SIZE = 40,
    ELF_SYMBOL_SIZE = 16,
    ELF_SECTION_PROGBITS = 1,
    ELF_SECTION_SYMTAB = 2,
    ELF_EXECUTABLE = 2,
    ELF_MACHINE_DSPIC = 118,
    ELF_FLAG_EXECUTE = 4,
    ELF_FLAG_PROGRAM = 0x40000000,
    ELF_FLAG_PSV = 0x10000000
};

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint32_t address;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t entry_size;
} Section;

static uint16_t read_u16(const uint8_t* bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static uint32_t read_u32(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static bool range_valid(size_t size, uint32_t offset, uint32_t length) {
    return offset <= size && length <= size - offset;
}

static void set_error(char* error, size_t error_size, const char* message) {
    if (error_size != 0u) {
        snprintf(error, error_size, "%s", message);
    }
}

static bool header_valid(const ElfImage* image, char* error, size_t error_size) {
    const uint8_t* bytes = image->bytes;
    if (image->size < ELF_HEADER_SIZE || bytes[0] != 0x7fu || bytes[1] != 'E' || bytes[2] != 'L' ||
        bytes[3] != 'F') {
        set_error(error, error_size, "image is not ELF");
        return false;
    }
    if (bytes[4] != 1u || bytes[5] != 1u) {
        set_error(error, error_size, "image is not little-endian ELF32");
        return false;
    }
    if (read_u16(bytes + 16u) != ELF_EXECUTABLE || read_u16(bytes + 18u) != ELF_MACHINE_DSPIC) {
        set_error(error, error_size, "image is not a dsPIC executable");
        return false;
    }
    return true;
}

static bool section_table(const ElfImage* image, uint32_t* offset, uint16_t* count, char* error,
                          size_t error_size) {
    uint16_t entry_size;
    if (!header_valid(image, error, error_size)) {
        return false;
    }
    *offset = read_u32(image->bytes + 32u);
    entry_size = read_u16(image->bytes + 46u);
    *count = read_u16(image->bytes + 48u);
    if (entry_size != ELF_SECTION_SIZE ||
        !range_valid(image->size, *offset, (uint32_t)*count * entry_size)) {
        set_error(error, error_size, "ELF section table is invalid");
        return false;
    }
    return true;
}

static Section read_section(const ElfImage* image, uint32_t table, uint16_t index) {
    const uint8_t* bytes = image->bytes + table + (uint32_t)index * ELF_SECTION_SIZE;
    Section section;
    section.type = read_u32(bytes + 4u);
    section.flags = read_u32(bytes + 8u);
    section.address = read_u32(bytes + 12u);
    section.offset = read_u32(bytes + 16u);
    section.size = read_u32(bytes + 20u);
    section.link = read_u32(bytes + 24u);
    section.entry_size = read_u32(bytes + 36u);
    return section;
}

bool elf_image_open(ElfImage* image, const char* path, char* error, size_t error_size) {
    FILE* file;
    long length;
    memset(image, 0, sizeof(*image));
    file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "cannot open ELF image");
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error(error, error_size, "cannot measure ELF image");
        return false;
    }
    image->size = (size_t)length;
    image->bytes = malloc(image->size);
    if (image->bytes == NULL || fread(image->bytes, 1u, image->size, file) != image->size) {
        fclose(file);
        elf_image_close(image);
        set_error(error, error_size, "cannot read ELF image");
        return false;
    }
    fclose(file);
    if (!header_valid(image, error, error_size)) {
        elf_image_close(image);
        return false;
    }
    return true;
}

bool elf_image_open_data(ElfImage* image, const void* data, size_t size, char* error,
                         size_t error_size) {
    memset(image, 0, sizeof(*image));
    if (data == NULL || size == 0u) {
        set_error(error, error_size, "ELF image is empty");
        return false;
    }
    image->bytes = malloc(size);
    if (image->bytes == NULL) {
        set_error(error, error_size, "cannot allocate ELF image");
        return false;
    }
    memcpy(image->bytes, data, size);
    image->size = size;
    if (!header_valid(image, error, error_size)) {
        elf_image_close(image);
        return false;
    }
    return true;
}

void elf_image_close(ElfImage* image) {
    free(image->bytes);
    image->bytes = NULL;
    image->size = 0u;
}

bool elf_image_load_program(const ElfImage* image, Dspic33* cpu, char* error, size_t error_size) {
    uint32_t table;
    uint16_t count;
    uint16_t index;
    if (!section_table(image, &table, &count, error, error_size)) {
        return false;
    }
    for (index = 0u; index < count; index++) {
        Section section = read_section(image, table, index);
        uint32_t position;
        if (section.type != ELF_SECTION_PROGBITS ||
            (section.flags & (ELF_FLAG_PROGRAM | ELF_FLAG_PSV)) == 0u) {
            continue;
        }
        if ((section.size & 3u) != 0u || !range_valid(image->size, section.offset, section.size)) {
            set_error(error, error_size, "ELF program section is invalid");
            return false;
        }
        for (position = 0u; position < section.size; position += 4u) {
            uint32_t address = section.address + position / 2u;
            uint32_t word = read_u32(image->bytes + section.offset + position);
            if (address >= DSPIC33_CONFIGURATION_BASE &&
                address < DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE) {
                if (!dspic33_load_configuration_word(cpu, address, word)) {
                    set_error(error, error_size, "ELF configuration exceeds device memory");
                    return false;
                }
            } else if (dspic33_program_range_implemented(address, 2u) &&
                       !dspic33_load_program_word(cpu, address, word)) {
                set_error(error, error_size, "ELF program exceeds device memory");
                return false;
            }
        }
    }
    return true;
}

static bool symbol_name_matches(const char* actual, const char* requested) {
    return strcmp(actual, requested) == 0 ||
           (actual[0] == '_' && strcmp(actual + 1, requested) == 0);
}

bool elf_image_symbol(const ElfImage* image, const char* name, uint32_t* address, char* error,
                      size_t error_size) {
    uint32_t table;
    uint16_t count;
    uint16_t index;
    if (!section_table(image, &table, &count, error, error_size)) {
        return false;
    }
    for (index = 0u; index < count; index++) {
        Section symbols = read_section(image, table, index);
        Section strings;
        uint32_t position;
        if (symbols.type != ELF_SECTION_SYMTAB || symbols.entry_size != ELF_SYMBOL_SIZE ||
            symbols.link >= count || !range_valid(image->size, symbols.offset, symbols.size)) {
            continue;
        }
        strings = read_section(image, table, (uint16_t)symbols.link);
        if (!range_valid(image->size, strings.offset, strings.size)) {
            continue;
        }
        for (position = 0u; position < symbols.size; position += ELF_SYMBOL_SIZE) {
            const uint8_t* symbol = image->bytes + symbols.offset + position;
            uint32_t name_offset = read_u32(symbol);
            const char* actual;
            size_t remaining;
            if (name_offset >= strings.size) {
                continue;
            }
            actual = (const char*)image->bytes + strings.offset + name_offset;
            remaining = strings.size - name_offset;
            if (memchr(actual, '\0', remaining) == NULL) {
                continue;
            }
            if (symbol_name_matches(actual, name)) {
                *address = read_u32(symbol + 4u);
                return true;
            }
        }
    }
    set_error(error, error_size, "ELF symbol was not found");
    return false;
}
