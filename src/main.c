#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dspic33.h"
#include "firmware_image.h"

typedef struct {
    const char* image_path;
    const char* entry_symbol;
    const char* write_symbol;
    uint16_t write_value;
    uint8_t write_width;
    uint16_t write_offset;
    bool register_set[16];
    uint16_t register_value[16];
    uint64_t instruction_limit;
} Arguments;

static bool parse_u64(const char* text, uint64_t maximum, uint64_t* value);

static bool parse_register(const char* text, uint8_t* reg) {
    uint64_t value;
    if (text[0] != 'W' || !parse_u64(text + 1, 15u, &value)) {
        return false;
    }
    *reg = (uint8_t)value;
    return true;
}

static bool parse_u64(const char* text, uint64_t maximum, uint64_t* value) {
    char* end;
    unsigned long long parsed;
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || *text == '\0' || *end != '\0' || parsed > maximum) {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_arguments(int argc, char** argv, Arguments* arguments) {
    int index;
    uint8_t reg;
    uint64_t value;
    if (argc < 3) {
        return false;
    }
    arguments->image_path = argv[1];
    arguments->entry_symbol = argv[2];
    arguments->write_symbol = NULL;
    arguments->write_value = 0u;
    arguments->write_width = 0u;
    arguments->write_offset = 0u;
    memset(arguments->register_set, 0, sizeof(arguments->register_set));
    memset(arguments->register_value, 0, sizeof(arguments->register_value));
    arguments->instruction_limit = 1000000u;
    for (index = 3; index < argc; index++) {
        if (strcmp(argv[index], "--write8") == 0 && index + 2 < argc) {
            arguments->write_symbol = argv[++index];
            if (!parse_u64(argv[++index], UINT8_MAX, &value)) {
                return false;
            }
            arguments->write_value = (uint16_t)value;
            arguments->write_width = 1u;
        } else if (strcmp(argv[index], "--write16") == 0 && index + 2 < argc) {
            arguments->write_symbol = argv[++index];
            if (!parse_u64(argv[++index], UINT16_MAX, &value)) {
                return false;
            }
            arguments->write_value = (uint16_t)value;
            arguments->write_width = 2u;
        } else if (strcmp(argv[index], "--write-offset") == 0 && index + 1 < argc) {
            if (!parse_u64(argv[++index], UINT16_MAX, &value)) {
                return false;
            }
            arguments->write_offset = (uint16_t)value;
        } else if (strcmp(argv[index], "--register") == 0 && index + 2 < argc) {
            if (!parse_register(argv[++index], &reg) ||
                !parse_u64(argv[++index], UINT16_MAX, &value)) {
                return false;
            }
            arguments->register_set[reg] = true;
            arguments->register_value[reg] = (uint16_t)value;
        } else if (strcmp(argv[index], "--max-instructions") == 0 && index + 1 < argc) {
            if (!parse_u64(argv[++index], UINT64_MAX, &value) || value == 0u) {
                return false;
            }
            arguments->instruction_limit = value;
        } else {
            return false;
        }
    }
    return true;
}

static void print_usage(const char* program) {
    fprintf(stderr,
            "Usage: %s IMAGE ENTRY [--write8 SYMBOL VALUE] [--write16 SYMBOL VALUE] "
            "[--write-offset BYTES] [--register Wn VALUE] "
            "[--max-instructions COUNT]\n",
            program);
}

static bool resolve_location(const FirmwareImage* image, const char* text,
                             uint32_t* address, char* error, size_t error_size) {
    uint64_t numeric;
    if (parse_u64(text, UINT32_MAX, &numeric)) {
        *address = (uint32_t)numeric;
        return true;
    }
    return firmware_image_symbol(image, text, address, error, error_size);
}

int main(int argc, char** argv) {
    Arguments arguments;
    FirmwareImage image;
    Dspic33 cpu;
    uint32_t entry;
    uint32_t write_address;
    Dspic33StopReason reason;
    uint8_t reg;
    char error[160];

    if (!parse_arguments(argc, argv, &arguments)) {
        print_usage(argv[0]);
        return 2;
    }
    if (!firmware_image_open(&image, arguments.image_path, error, sizeof(error))) {
        fprintf(stderr, "[error] %s\n", error);
        return 1;
    }
    if (!dspic33_initialize(&cpu)) {
        firmware_image_close(&image);
        fprintf(stderr, "[error] cannot allocate simulator memory\n");
        return 1;
    }
    if (!firmware_image_load_program(&image, &cpu, error, sizeof(error)) ||
        !resolve_location(&image, arguments.entry_symbol, &entry, error,
                          sizeof(error))) {
        fprintf(stderr, "[error] %s\n", error);
        dspic33_destroy(&cpu);
        firmware_image_close(&image);
        return 1;
    }
    dspic33_reset(&cpu, entry);
    for (reg = 0u; reg < 16u; reg++) {
        if (arguments.register_set[reg]) {
            cpu.w[reg] = arguments.register_value[reg];
        }
    }
    if (arguments.write_symbol != NULL) {
        if (!resolve_location(&image, arguments.write_symbol, &write_address, error,
                              sizeof(error)) ||
            write_address + arguments.write_offset >= DSPIC33_DATA_SIZE) {
            fprintf(stderr, "[error] %s\n", error);
            dspic33_destroy(&cpu);
            firmware_image_close(&image);
            return 1;
        }
        write_address += arguments.write_offset;
        if (arguments.write_width == 1u) {
            dspic33_write_byte(&cpu, write_address, (uint8_t)arguments.write_value);
        } else {
            dspic33_write_word(&cpu, write_address, arguments.write_value);
        }
    }
    reason = dspic33_run(&cpu, arguments.instruction_limit);
    printf("[%s] pc=0x%06" PRIx32 " instructions=%" PRIu64 " W0=0x%04x SR=0x%04x\n",
           reason == DSPIC33_RETURNED ? "passed" : "failed", cpu.pc, cpu.instructions,
           cpu.w[0], cpu.sr);
    if (reason == DSPIC33_UNSUPPORTED_INSTRUCTION) {
        printf("[detail] unsupported opcode=0x%06" PRIx32 "\n", cpu.unsupported_opcode);
    } else if (reason != DSPIC33_RETURNED) {
        printf("[detail] stop reason=%s\n", dspic33_stop_reason_name(reason));
    }
    dspic33_destroy(&cpu);
    firmware_image_close(&image);
    return reason == DSPIC33_RETURNED ? 0 : 1;
}
