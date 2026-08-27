#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dspic33_firmware_image.h"
#include "test.h"

enum {
    ELF_HEADER_SIZE = 52,
    ELF_PROGRAM_SIZE = 32,
    ELF_SECTION_SIZE = 40,
    PROGRAM_TABLE_OFFSET = ELF_HEADER_SIZE,
    PROGRAM_SECTION_TABLE_OFFSET = PROGRAM_TABLE_OFFSET + ELF_PROGRAM_SIZE,
    PROGRAM_SECTION_OFFSET = PROGRAM_SECTION_TABLE_OFFSET + ELF_SECTION_SIZE,
    COMMENT_SECTION_OFFSET = PROGRAM_SECTION_OFFSET + ELF_SECTION_SIZE,
    NOLOAD_SECTION_OFFSET = COMMENT_SECTION_OFFSET + ELF_SECTION_SIZE,
    PROGRAM_DATA_OFFSET = NOLOAD_SECTION_OFFSET + ELF_SECTION_SIZE,
    COMMENT_DATA_OFFSET = PROGRAM_DATA_OFFSET + 4,
    NOLOAD_DATA_OFFSET = COMMENT_DATA_OFFSET + 4,
    IMAGE_SIZE = NOLOAD_DATA_OFFSET + 4,
    SYMBOL_OFFSET = ELF_HEADER_SIZE + 3 * ELF_SECTION_SIZE,
    STRING_OFFSET = SYMBOL_OFFSET + 16,
    SYMBOL_IMAGE_SIZE = STRING_OFFSET + 7,

    MALFORMED_STRING_OFFSET = SYMBOL_OFFSET + 24,
    MALFORMED_SYMBOL_IMAGE_SIZE = MALFORMED_STRING_OFFSET + 6,
};

static void write_u16_le(uint8_t* data, size_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
}

static void write_u32_le(uint8_t* data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1u] = (uint8_t)(value >> 8u);
    data[offset + 2u] = (uint8_t)(value >> 16u);
    data[offset + 3u] = (uint8_t)(value >> 24u);
}

static void initialize_elf_image(uint8_t* image) {
    memset(image, 0, IMAGE_SIZE);
    image[0] = 0x7fu;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 1u;
    image[5] = 1u;
    image[6] = 1u;
    write_u16_le(image, 16u, 2u);
    write_u16_le(image, 18u, 118u);
    write_u32_le(image, 20u, 1u);
    write_u32_le(image, 24u, 0x100u);
    write_u32_le(image, 28u, PROGRAM_TABLE_OFFSET);
    write_u32_le(image, 32u, PROGRAM_SECTION_TABLE_OFFSET);
    write_u16_le(image, 40u, ELF_HEADER_SIZE);
    write_u16_le(image, 42u, ELF_PROGRAM_SIZE);
    write_u16_le(image, 44u, 1u);
    write_u16_le(image, 46u, ELF_SECTION_SIZE);
    write_u16_le(image, 48u, 4u);

    write_u32_le(image, PROGRAM_TABLE_OFFSET, 1u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 4u, PROGRAM_DATA_OFFSET);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 8u, 0x100u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 12u, 0x100u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 16u, 4u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 20u, 4u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 24u, 5u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 28u, 2u);

    const size_t section = PROGRAM_SECTION_OFFSET;
    write_u32_le(image, section + 4u, 1u);
    write_u32_le(image, section + 8u, 0x00000006u);
    write_u32_le(image, section + 12u, 0x100u);
    write_u32_le(image, section + 16u, PROGRAM_DATA_OFFSET);
    write_u32_le(image, section + 20u, 4u);
    write_u32_le(image, PROGRAM_DATA_OFFSET, 0x00123456u);

    write_u32_le(image, COMMENT_SECTION_OFFSET + 4u, 1u);
    write_u32_le(image, COMMENT_SECTION_OFFSET + 8u, 0x40800000u);
    write_u32_le(image, COMMENT_SECTION_OFFSET + 16u, COMMENT_DATA_OFFSET);
    write_u32_le(image, COMMENT_SECTION_OFFSET + 20u, 4u);
    memcpy(image + COMMENT_DATA_OFFSET, "meta", 4u);

    write_u32_le(image, NOLOAD_SECTION_OFFSET + 4u, 1u);
    write_u32_le(image, NOLOAD_SECTION_OFFSET + 8u, 0x40800006u);
    write_u32_le(image, NOLOAD_SECTION_OFFSET + 12u, 0x200u);
    write_u32_le(image, NOLOAD_SECTION_OFFSET + 16u, NOLOAD_DATA_OFFSET);
    write_u32_le(image, NOLOAD_SECTION_OFFSET + 20u, 4u);
    write_u32_le(image, NOLOAD_DATA_OFFSET, 0x00654321u);
}

static void initialize_symbol_image(uint8_t* image) {
    memset(image, 0, SYMBOL_IMAGE_SIZE);
    image[0] = 0x7fu;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 1u;
    image[5] = 1u;
    image[6] = 1u;
    write_u16_le(image, 16u, 2u);
    write_u16_le(image, 18u, 118u);
    write_u32_le(image, 20u, 1u);
    write_u32_le(image, 32u, ELF_HEADER_SIZE);
    write_u16_le(image, 40u, ELF_HEADER_SIZE);
    write_u16_le(image, 46u, ELF_SECTION_SIZE);
    write_u16_le(image, 48u, 3u);

    size_t section = ELF_HEADER_SIZE + ELF_SECTION_SIZE;
    write_u32_le(image, section + 4u, 2u);
    write_u32_le(image, section + 16u, SYMBOL_OFFSET);
    write_u32_le(image, section + 20u, 16u);
    write_u32_le(image, section + 24u, 2u);
    write_u32_le(image, section + 36u, 16u);

    section += ELF_SECTION_SIZE;
    write_u32_le(image, section + 4u, 3u);
    write_u32_le(image, section + 16u, STRING_OFFSET);
    write_u32_le(image, section + 20u, 7u);
    write_u32_le(image, SYMBOL_OFFSET, 1u);
    write_u32_le(image, SYMBOL_OFFSET + 4u, 0x1234u);
    write_u16_le(image, SYMBOL_OFFSET + 14u, 0xfff1u);
    memcpy(image + STRING_OFFSET, "\0_test", 7u);
}

static bool write_file(const char* path, const void* data, size_t size) {
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    const bool written = fwrite(data, 1u, size, file) == size;
    return fclose(file) == 0 && written;
}

static void test_file_loading(TestState* state, Dspic33* cpu) {
    static const char elf_path[] = "dspic33_firmware_image_test.elf";
    static const char binary_path[] = "dspic33_firmware_image_test.bin";
    static const char missing_path[] = "dspic33_missing_firmware_image.bin";
    uint8_t elf[IMAGE_SIZE];
    const uint8_t binary[] = {0x56u, 0x34u, 0x12u, 0u};
    char error[160] = {0};
    FirmwareImage image;
    initialize_elf_image(elf);
    expect(state, write_file(elf_path, elf, sizeof(elf)), "write ELF file");
    expect(state, firmware_image_open(&image, elf_path, error, sizeof(error)),
           "firmware_image_open ELF");
    expect(state, image.type == FIRMWARE_IMAGE_ELF, "file image type is ELF");
    expect(state, firmware_image_load_program(&image, cpu, error, sizeof(error)),
           "firmware_image_load_program ELF");
    firmware_image_close(&image);
    expect(state, write_file(binary_path, binary, sizeof(binary)), "write binary file");
    expect(state, firmware_image_open(&image, binary_path, error, sizeof(error)),
           "firmware_image_open binary");
    expect(state, image.type == FIRMWARE_IMAGE_BINARY, "file image type is binary");
    expect(state, firmware_image_load_program(&image, cpu, error, sizeof(error)),
           "firmware_image_load_program binary");
    uint32_t symbol_address = 0u;
    expect(state, !firmware_image_symbol(&image, "missing", &symbol_address, error, sizeof(error)),
           "binary firmware symbol is rejected");
    firmware_image_close(&image);
    expect(state, write_file(elf_path, elf, 4u), "write truncated ELF file");
    expect(state, !firmware_image_open(&image, elf_path, error, sizeof(error)),
           "truncated file ELF is rejected");
    expect(state, !firmware_image_open(&image, missing_path, error, sizeof(error)),
           "missing firmware image is rejected");
    (void)remove(elf_path);
    (void)remove(binary_path);
}

static void test_binary(TestState* state, Dspic33* cpu) {
    const uint8_t image[] = {0x56u, 0x34u, 0x12u, 0u};
    const uint8_t erased[] = {0xffu, 0xffu, 0xffu, 0u};
    uint32_t entry_address = UINT32_MAX;
    expect(state, dspic33_load_binary_data(cpu, image, sizeof(image), 0x200u, &entry_address),
           "dspic33_load_binary_data(cpu, image, sizeof(image), 0x200u, &entry_address)");
    expect(state, entry_address == 0x100u, "entry_address == 0x100u");
    expect(state, dspic33_read_program_word(cpu, 0x100u) == 0x00123456u,
           "dspic33_read_program_word(cpu, 0x100u) == 0x00123456u");
    expect(state, !dspic33_load_binary_data(cpu, image, 3u, 0u, &entry_address),
           "!dspic33_load_binary_data(cpu, image, 3u, 0u, &entry_address)");
    expect(state, !dspic33_load_binary_data(cpu, image, sizeof(image), 2u, &entry_address),
           "!dspic33_load_binary_data(cpu, image, sizeof(image), 2u, &entry_address)");
    expect(state, !dspic33_load_binary_data(cpu, image, sizeof(image), 0x100000u, &entry_address),
           "out-of-map binary content is rejected");
    expect(state, dspic33_load_binary_data(cpu, erased, sizeof(erased), 0x100000u, &entry_address),
           "out-of-map erased binary content is ignored");
    expect(state, dspic33_load_binary_data(cpu, image, sizeof(image), 0x204u, NULL),
           "dspic33_load_binary_data(cpu, image, sizeof(image), 0x204u, NULL)");
}

static void test_elf(TestState* state, Dspic33* cpu) {
    uint8_t image[IMAGE_SIZE];
    initialize_elf_image(image);
    uint32_t entry_address = UINT32_MAX;
    expect(state, dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address)");
    expect(state, entry_address == 0x100u, "entry_address == 0x100u");
    expect(state, dspic33_read_program_word(cpu, 0x100u) == 0x00123456u,
           "dspic33_read_program_word(cpu, 0x100u) == 0x00123456u");
    expect(state, dspic33_read_program_word(cpu, 0x200u) == 0x00ffffffu,
           "allocated noload ELF program section is ignored");

    initialize_elf_image(image);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 16u, 8u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 20u, 8u);
    write_u32_le(image, COMMENT_SECTION_OFFSET + 8u, 0x40000002u);
    write_u32_le(image, COMMENT_SECTION_OFFSET + 12u, 0x8300u);
    write_u32_le(image, COMMENT_DATA_OFFSET, 0x00ab3412u);
    expect(state, dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "packedflash ELF section loads");
    expect(state, dspic33_read_program_word(cpu, 0x102u) == 0x00ab3412u,
           "packedflash ELF section retains the upper Flash byte");

    initialize_elf_image(image);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 8u, 0x8300u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 12u, 0x300u);
    write_u32_le(image, PROGRAM_SECTION_OFFSET + 8u, 0x10000002u);
    write_u32_le(image, PROGRAM_SECTION_OFFSET + 12u, 0x8300u);
    expect(state, dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "PSV ELF section loads from its physical address");
    expect(state,
           dspic33_read_program_word(cpu, 0x300u) == 0x00123456u &&
               dspic33_read_program_word(cpu, 0x8300u) == 0x00ffffffu,
           "PSV ELF section uses LMA instead of VMA");
    image[0] = 0u;
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "!dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address)");
    initialize_elf_image(image);
    expect(state, dspic33_load_elf_data(cpu, image, sizeof(image), NULL),
           "dspic33_load_elf_data(cpu, image, sizeof(image), NULL)");
    expect(state, !dspic33_load_elf_data(cpu, image, ELF_HEADER_SIZE - 1u, &entry_address),
           "truncated ELF header is rejected");
    initialize_elf_image(image);
    image[4] = 2u;
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "ELF64 image is rejected");
    initialize_elf_image(image);
    image[6] = 0u;
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "invalid ELF identification version is rejected");
    initialize_elf_image(image);
    write_u32_le(image, 20u, 0u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "invalid ELF header version is rejected");
    initialize_elf_image(image);
    write_u16_le(image, 40u, ELF_HEADER_SIZE - 1u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "invalid ELF header size is rejected");
    initialize_elf_image(image);
    write_u16_le(image, 42u, ELF_PROGRAM_SIZE - 1u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "short program header is rejected");
    initialize_elf_image(image);
    write_u16_le(image, 46u, ELF_SECTION_SIZE - 1u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "short section header is rejected");
    initialize_elf_image(image);
    write_u32_le(image, 32u, UINT32_MAX);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "out-of-range section table is rejected");
    initialize_elf_image(image);
    write_u32_le(image, PROGRAM_SECTION_OFFSET + 20u, 3u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "unaligned program section is rejected");
    initialize_elf_image(image);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 8u, 0x80000u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 12u, 0x80000u);
    write_u32_le(image, PROGRAM_SECTION_OFFSET + 12u, 0x80000u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "out-of-map ELF content is rejected");
    write_u32_le(image, PROGRAM_DATA_OFFSET, 0x00ffffffu);
    expect(state, dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "out-of-map erased ELF content is ignored");

    uint8_t wrapping_image[IMAGE_SIZE + 4u];
    initialize_elf_image(wrapping_image);
    write_u32_le(wrapping_image, PROGRAM_TABLE_OFFSET + 8u, UINT32_MAX - 1u);
    write_u32_le(wrapping_image, PROGRAM_TABLE_OFFSET + 12u, UINT32_MAX - 1u);
    write_u32_le(wrapping_image, PROGRAM_TABLE_OFFSET + 16u, 8u);
    write_u32_le(wrapping_image, PROGRAM_TABLE_OFFSET + 20u, 8u);
    write_u32_le(wrapping_image, PROGRAM_SECTION_OFFSET + 12u, UINT32_MAX - 1u);
    write_u32_le(wrapping_image, PROGRAM_SECTION_OFFSET + 20u, 8u);
    write_u32_le(wrapping_image, PROGRAM_DATA_OFFSET, UINT32_MAX);
    write_u32_le(wrapping_image, PROGRAM_DATA_OFFSET + 4u, 0x00123456u);
    expect(state,
           !dspic33_load_elf_data(cpu, wrapping_image, sizeof(wrapping_image), &entry_address),
           "wrapping ELF program address is rejected");
}

static void test_sectionless_elf(TestState* state) {
    uint8_t image[IMAGE_SIZE];
    uint32_t entry_address = UINT32_MAX;
    Dspic33* cpu = dspic33_create();
    expect(state, cpu != NULL, "create CPU for sectionless ELF");

    initialize_elf_image(image);
    write_u32_le(image, 32u, 0u);
    write_u16_le(image, 46u, 0u);
    write_u16_le(image, 48u, 0u);
    expect(state, dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "sectionless ELF loads from program segments");
    expect(state, entry_address == 0x100u && dspic33_read_program_word(cpu, 0x100u) == 0x00123456u,
           "sectionless ELF loads its entry program word");

    dspic33_load_program_word(cpu, 0x100u, 0x00ffffffu);
    initialize_elf_image(image);
    write_u32_le(image, 32u, 0u);
    write_u16_le(image, 48u, 0u);
    expect(state, dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "sectionless ELF accepts a retained section entry size");
    expect(state, dspic33_read_program_word(cpu, 0x100u) == 0x00123456u,
           "sectionless ELF with retained entry size loads its program word");

    dspic33_load_program_word(cpu, 0x100u, 0x00ffffffu);
    initialize_elf_image(image);
    write_u32_le(image, 32u, 0u);
    write_u16_le(image, 46u, 0u);
    write_u16_le(image, 48u, 0u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 24u, 6u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "sectionless ELF does not treat writable data as program content");
    expect(state, dspic33_read_program_word(cpu, 0x100u) == 0x00ffffffu,
           "sectionless ELF leaves writable data out of program memory");

    initialize_elf_image(image);
    write_u32_le(image, 32u, 0u);
    write_u16_le(image, 46u, 0u);
    write_u16_le(image, 48u, 0u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 20u, 0u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "sectionless ELF rejects a segment whose file image exceeds memory");

    initialize_elf_image(image);
    write_u32_le(image, 32u, 0u);
    write_u16_le(image, 46u, 0u);
    write_u16_le(image, 48u, 0u);
    write_u32_le(image, PROGRAM_TABLE_OFFSET + 16u, 3u);
    expect(state, !dspic33_load_elf_data(cpu, image, sizeof(image), &entry_address),
           "sectionless ELF rejects a partial program word");

    dspic33_destroy(cpu);
}

static void test_symbol(TestState* state) {
    uint8_t image[SYMBOL_IMAGE_SIZE];
    initialize_symbol_image(image);
    uint32_t symbol_address = UINT32_MAX;
    expect(state, dspic33_elf_symbol_data(image, sizeof(image), "test", &symbol_address),
           "dspic33_elf_symbol_data(image, sizeof(image), test, &symbol_address)");
    expect(state, symbol_address == 0x1234u, "symbol_address == 0x1234u");
    expect(state, !dspic33_elf_symbol_data(image, sizeof(image), "missing", &symbol_address),
           "!dspic33_elf_symbol_data(image, sizeof(image), missing, &symbol_address)");
    expect(state, !dspic33_elf_symbol_data(image, sizeof(image), NULL, &symbol_address),
           "!dspic33_elf_symbol_data(image, sizeof(image), NULL, &symbol_address)");
    initialize_symbol_image(image);
    write_u16_le(image, SYMBOL_OFFSET + 14u, 0u);
    expect(state, !dspic33_elf_symbol_data(image, sizeof(image), "test", &symbol_address),
           "undefined ELF symbol is rejected");
    initialize_symbol_image(image);
    write_u32_le(image, ELF_HEADER_SIZE + 2u * ELF_SECTION_SIZE + 4u, 1u);
    expect(state, !dspic33_elf_symbol_data(image, sizeof(image), "test", &symbol_address),
           "symbol table requires a linked string table");
    uint8_t malformed[MALFORMED_SYMBOL_IMAGE_SIZE] = {0};
    initialize_symbol_image(malformed);
    write_u32_le(malformed, ELF_HEADER_SIZE + ELF_SECTION_SIZE + 20u, 17u);
    write_u32_le(malformed, ELF_HEADER_SIZE + 2u * ELF_SECTION_SIZE + 16u, MALFORMED_STRING_OFFSET);
    write_u32_le(malformed, ELF_HEADER_SIZE + 2u * ELF_SECTION_SIZE + 20u, 6u);
    write_u32_le(malformed, SYMBOL_OFFSET + 16u, 1u);
    write_u32_le(malformed, SYMBOL_OFFSET + 20u, 0xdeadu);
    memcpy(malformed + MALFORMED_STRING_OFFSET, "\0test", 6u);
    expect(state, !dspic33_elf_symbol_data(malformed, sizeof(malformed), "test", &symbol_address),
           "partial symbol entry is rejected");
}

int main(void) {
    TestState state = {0};
    Dspic33* cpu = dspic33_create();
    expect(&state, cpu != NULL, "cpu != NULL");
    test_binary(&state, cpu);
    test_elf(&state, cpu);
    test_sectionless_elf(&state);
    test_symbol(&state);
    test_file_loading(&state, cpu);
    dspic33_destroy(cpu);
    return test_finish(&state);
}
