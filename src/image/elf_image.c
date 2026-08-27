#define _CRT_SECURE_NO_WARNINGS

#include "elf_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ELF_HEADER_SIZE = 52,
    ELF_PROGRAM_SIZE = 32,
    ELF_SECTION_SIZE = 40,
    ELF_SYMBOL_SIZE = 16,
    ELF_SEGMENT_LOAD = 1,
    ELF_SECTION_PROGBITS = 1,
    ELF_SECTION_SYMTAB = 2,
    ELF_SECTION_STRTAB = 3,
    ELF_EXECUTABLE = 2,
    ELF_MACHINE_DSPIC = 118,
    ELF_VERSION_CURRENT = 1,
    ELF_SECTION_UNDEFINED = 0,
    ELF_FLAG_EXECUTE = 4,
    ELF_FLAG_PROGRAM = 0x40000000,
    ELF_FLAG_PSV = 0x10000000
};

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint32_t virtual_address;
    uint32_t file_offset;
    uint32_t byte_size;
    uint32_t linked_section;
    uint32_t entry_size;
} ElfSection;

typedef struct {
    uint32_t type;
    uint32_t file_offset;
    uint32_t physical_address;
    uint32_t file_size;
} ElfSegment;

static uint16_t read_u16(const uint8_t* bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static uint32_t read_u32(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static bool range_valid(size_t image_size, uint32_t file_offset, uint32_t byte_count) {
    return file_offset <= image_size && byte_count <= image_size - file_offset;
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
    if (bytes[4] != 1u || bytes[5] != 1u || bytes[6] != ELF_VERSION_CURRENT ||
        read_u32(bytes + 20u) != ELF_VERSION_CURRENT || read_u16(bytes + 40u) != ELF_HEADER_SIZE) {
        set_error(error, error_size, "image is not little-endian ELF32");
        return false;
    }
    if (read_u16(bytes + 16u) != ELF_EXECUTABLE || read_u16(bytes + 18u) != ELF_MACHINE_DSPIC) {
        set_error(error, error_size, "image is not a dsPIC executable");
        return false;
    }
    return true;
}

static bool section_table(const ElfImage* image, uint32_t* table_offset, uint16_t* section_count,
                          char* error, size_t error_size) {
    uint16_t section_entry_size;
    if (!header_valid(image, error, error_size)) {
        return false;
    }
    *table_offset = read_u32(image->bytes + 32u);
    section_entry_size = read_u16(image->bytes + 46u);
    *section_count = read_u16(image->bytes + 48u);
    if (section_entry_size != ELF_SECTION_SIZE ||
        !range_valid(image->size, *table_offset, (uint32_t)*section_count * section_entry_size)) {
        set_error(error, error_size, "ELF section table is invalid");
        return false;
    }
    return true;
}

static bool program_table(const ElfImage* image, uint32_t* table_offset, uint16_t* segment_count,
                          char* error, size_t error_size) {
    uint16_t program_entry_size;
    if (!header_valid(image, error, error_size)) {
        return false;
    }
    *table_offset = read_u32(image->bytes + 28u);
    program_entry_size = read_u16(image->bytes + 42u);
    *segment_count = read_u16(image->bytes + 44u);
    if (*segment_count != 0u &&
        (program_entry_size != ELF_PROGRAM_SIZE ||
         !range_valid(image->size, *table_offset, (uint32_t)*segment_count * program_entry_size))) {
        set_error(error, error_size, "ELF program table is invalid");
        return false;
    }
    return true;
}

static ElfSection read_section(const ElfImage* image, uint32_t table_offset,
                               uint16_t section_index) {
    const uint8_t* section_bytes =
        image->bytes + table_offset + (uint32_t)section_index * ELF_SECTION_SIZE;
    ElfSection section;
    section.type = read_u32(section_bytes + 4u);
    section.flags = read_u32(section_bytes + 8u);
    section.virtual_address = read_u32(section_bytes + 12u);
    section.file_offset = read_u32(section_bytes + 16u);
    section.byte_size = read_u32(section_bytes + 20u);
    section.linked_section = read_u32(section_bytes + 24u);
    section.entry_size = read_u32(section_bytes + 36u);
    return section;
}

static ElfSegment read_segment(const ElfImage* image, uint32_t table_offset,
                               uint16_t segment_index) {
    const uint8_t* segment_bytes =
        image->bytes + table_offset + (uint32_t)segment_index * ELF_PROGRAM_SIZE;
    ElfSegment segment;
    segment.type = read_u32(segment_bytes);
    segment.file_offset = read_u32(segment_bytes + 4u);
    segment.physical_address = read_u32(segment_bytes + 12u);
    segment.file_size = read_u32(segment_bytes + 16u);
    return segment;
}

static bool section_load_address(const ElfImage* image, const ElfSection* section,
                                 uint32_t program_table_offset, uint16_t segment_count,
                                 uint32_t* load_address) {
    if (segment_count == 0u) {
        *load_address = section->virtual_address;
        return true;
    }
    for (uint16_t segment_index = 0u; segment_index < segment_count; segment_index++) {
        const ElfSegment segment = read_segment(image, program_table_offset, segment_index);
        if (segment.type != ELF_SEGMENT_LOAD ||
            !range_valid(image->size, segment.file_offset, segment.file_size) ||
            section->file_offset < segment.file_offset) {
            continue;
        }
        const uint32_t file_delta = section->file_offset - segment.file_offset;
        if (file_delta > segment.file_size || section->byte_size > segment.file_size - file_delta) {
            continue;
        }
        if ((file_delta & 3u) != 0u || file_delta / 2u > UINT32_MAX - segment.physical_address) {
            return false;
        }
        *load_address = segment.physical_address + file_delta / 2u;
        return true;
    }
    return false;
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

bool elf_image_open_data(ElfImage* image, const void* image_data, size_t image_size, char* error,
                         size_t error_size) {
    memset(image, 0, sizeof(*image));
    if (image_data == NULL || image_size == 0u) {
        set_error(error, error_size, "ELF image is empty");
        return false;
    }
    image->bytes = malloc(image_size);
    if (image->bytes == NULL) {
        set_error(error, error_size, "cannot allocate ELF image");
        return false;
    }
    memcpy(image->bytes, image_data, image_size);
    image->size = image_size;
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
    uint32_t table_offset;
    uint32_t program_table_offset;
    uint16_t section_count;
    uint16_t segment_count;
    if (!section_table(image, &table_offset, &section_count, error, error_size) ||
        !program_table(image, &program_table_offset, &segment_count, error, error_size)) {
        return false;
    }
    for (uint16_t section_index = 0u; section_index < section_count; section_index++) {
        ElfSection program_section = read_section(image, table_offset, section_index);
        if (program_section.type != ELF_SECTION_PROGBITS ||
            (program_section.flags & (ELF_FLAG_PROGRAM | ELF_FLAG_PSV)) == 0u) {
            continue;
        }
        if ((program_section.byte_size & 3u) != 0u ||
            !range_valid(image->size, program_section.file_offset, program_section.byte_size)) {
            set_error(error, error_size, "ELF program section is invalid");
            return false;
        }
        uint32_t load_address;
        if (!section_load_address(image, &program_section, program_table_offset, segment_count,
                                  &load_address)) {
            set_error(error, error_size, "ELF program section is not loadable");
            return false;
        }
        for (uint32_t byte_offset = 0u; byte_offset < program_section.byte_size;
             byte_offset += 4u) {
            const uint32_t address_offset = byte_offset / 2u;
            if (address_offset > UINT32_MAX - load_address) {
                set_error(error, error_size, "ELF program address is invalid");
                return false;
            }
            uint32_t program_address = load_address + address_offset;
            uint32_t program_word =
                read_u32(image->bytes + program_section.file_offset + byte_offset);
            if (program_address >= DSPIC33_CONFIGURATION_BASE &&
                program_address < DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE) {
                if (!dspic33_load_configuration_word(cpu, program_address, program_word)) {
                    set_error(error, error_size, "ELF configuration exceeds device memory");
                    return false;
                }
            } else if (!dspic33_device_program_range_implemented(cpu, program_address, 2u)) {
                if (program_word != UINT32_MAX) {
                    set_error(error, error_size, "ELF program exceeds device memory");
                    return false;
                }
            } else if (!dspic33_load_program_word(cpu, program_address, program_word)) {
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
    uint32_t table_offset;
    uint16_t section_count;
    if (!section_table(image, &table_offset, &section_count, error, error_size)) {
        return false;
    }
    for (uint16_t section_index = 0u; section_index < section_count; section_index++) {
        ElfSection symbol_table = read_section(image, table_offset, section_index);
        ElfSection string_table;
        if (symbol_table.type != ELF_SECTION_SYMTAB || symbol_table.entry_size != ELF_SYMBOL_SIZE ||
            (symbol_table.byte_size % ELF_SYMBOL_SIZE) != 0u ||
            symbol_table.linked_section >= section_count ||
            !range_valid(image->size, symbol_table.file_offset, symbol_table.byte_size)) {
            continue;
        }
        string_table = read_section(image, table_offset, (uint16_t)symbol_table.linked_section);
        if (string_table.type != ELF_SECTION_STRTAB ||
            !range_valid(image->size, string_table.file_offset, string_table.byte_size)) {
            continue;
        }
        for (uint32_t byte_offset = 0u; byte_offset < symbol_table.byte_size;
             byte_offset += ELF_SYMBOL_SIZE) {
            const uint8_t* symbol_entry = image->bytes + symbol_table.file_offset + byte_offset;
            uint32_t string_offset = read_u32(symbol_entry);
            uint16_t symbol_section = read_u16(symbol_entry + 14u);
            const char* symbol_name;
            size_t string_bytes_remaining;
            if (symbol_section == ELF_SECTION_UNDEFINED ||
                string_offset >= string_table.byte_size) {
                continue;
            }
            symbol_name = (const char*)image->bytes + string_table.file_offset + string_offset;
            string_bytes_remaining = string_table.byte_size - string_offset;
            if (memchr(symbol_name, '\0', string_bytes_remaining) == NULL) {
                continue;
            }
            if (symbol_name_matches(symbol_name, name)) {
                *address = read_u32(symbol_entry + 4u);
                return true;
            }
        }
    }
    set_error(error, error_size, "ELF symbol was not found");
    return false;
}
