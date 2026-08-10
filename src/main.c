#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dspic33.h"
#include "firmware_image.h"
#include "firmware_runner.h"

typedef struct {
    const char* image_path;
    const char* entry_symbol;
    const char* stop_symbol;
    const char* write_symbol;
    const char* dump_memory_symbol;
    uint16_t write_value;
    uint8_t write_width;
    uint16_t write_offset;
    uint32_t dump_memory_size;
    bool program_word_set;
    uint32_t program_word_address;
    uint32_t program_word_value;
    bool register_set[16];
    uint16_t register_value[16];
    uint64_t instruction_limit;
    bool dump_registers;
    bool trace_address_set;
    uint32_t trace_address;
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
    arguments->stop_symbol = NULL;
    arguments->write_symbol = NULL;
    arguments->dump_memory_symbol = NULL;
    arguments->write_value = 0u;
    arguments->write_width = 0u;
    arguments->write_offset = 0u;
    arguments->dump_memory_size = 0u;
    arguments->program_word_set = false;
    arguments->program_word_address = 0u;
    arguments->program_word_value = 0u;
    memset(arguments->register_set, 0, sizeof(arguments->register_set));
    memset(arguments->register_value, 0, sizeof(arguments->register_value));
    arguments->instruction_limit = 1000000u;
    arguments->dump_registers = false;
    arguments->trace_address_set = false;
    arguments->trace_address = 0u;
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
        } else if (strcmp(argv[index], "--program-word") == 0 && index + 2 < argc) {
            if (!parse_u64(argv[++index], UINT32_MAX, &value)) {
                return false;
            }
            arguments->program_word_address = (uint32_t)value;
            if (!parse_u64(argv[++index], 0x00ffffffu, &value)) {
                return false;
            }
            arguments->program_word_value = (uint32_t)value;
            arguments->program_word_set = true;
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
        } else if (strcmp(argv[index], "--stop") == 0 && index + 1 < argc) {
            arguments->stop_symbol = argv[++index];
        } else if (strcmp(argv[index], "--dump-registers") == 0) {
            arguments->dump_registers = true;
        } else if (strcmp(argv[index], "--dump-memory") == 0 && index + 2 < argc) {
            arguments->dump_memory_symbol = argv[++index];
            if (!parse_u64(argv[++index], DSPIC33_DATA_SIZE, &value) || value == 0u) {
                return false;
            }
            arguments->dump_memory_size = (uint32_t)value;
        } else if (strcmp(argv[index], "--trace-address") == 0 && index + 1 < argc) {
            if (!parse_u64(argv[++index], UINT32_MAX, &value)) {
                return false;
            }
            arguments->trace_address_set = true;
            arguments->trace_address = (uint32_t)value;
        } else {
            return false;
        }
    }
    return true;
}

static void print_usage(const char* program) {
    fprintf(stderr,
            "Usage: %s IMAGE ENTRY [--write8 SYMBOL VALUE] [--write16 SYMBOL VALUE] "
            "[--write-offset BYTES] [--program-word ADDRESS VALUE] "
            "[--register Wn VALUE] "
            "[--stop ADDRESS] [--max-instructions COUNT] [--dump-registers] "
            "[--dump-memory SYMBOL SIZE] "
            "[--trace-address ADDRESS]\n",
            program);
}

static Dspic33StopReason run_with_trace(Dspic33* cpu, uint32_t stop_address,
                                        bool stop_enabled, uint64_t limit,
                                        uint32_t trace_address) {
    uint64_t start = cpu->instructions;
    cpu->stop_reason = DSPIC33_RUNNING;
    while (limit == 0u || cpu->instructions - start < limit) {
        if (stop_enabled && cpu->pc == stop_address) {
            cpu->stop_reason = DSPIC33_STOPPED;
            return cpu->stop_reason;
        }
        if (cpu->pc == trace_address) {
            uint16_t stack_low =
                cpu->w[15] >= 4u ? dspic33_read_word(cpu, cpu->w[15] - 4u) : 0u;
            uint16_t stack_high =
                cpu->w[15] >= 2u ? dspic33_read_word(cpu, cpu->w[15] - 2u) : 0u;
            printf("[trace] instruction=%" PRIu64 " cycles=%" PRIu64 " PC=0x%06" PRIx32
                   " W0=0x%04x W1=0x%04x W2=0x%04x W3=0x%04x "
                   "W4=0x%04x W5=0x%04x W6=0x%04x W7=0x%04x "
                   "W8=0x%04x W9=0x%04x W10=0x%04x W11=0x%04x "
                   "W12=0x%04x W13=0x%04x W14=0x%04x W15=0x%04x SR=0x%04x "
                   "TBLPAG=0x%04x DSRPAG=0x%04x DSWPAG=0x%04x "
                   "return=0x%02x%04x\n",
                   cpu->instructions, cpu->cycles, cpu->pc, cpu->w[0], cpu->w[1],
                   cpu->w[2], cpu->w[3], cpu->w[4], cpu->w[5], cpu->w[6], cpu->w[7],
                   cpu->w[8], cpu->w[9], cpu->w[10], cpu->w[11], cpu->w[12], cpu->w[13],
                   cpu->w[14], cpu->w[15], cpu->sr, cpu->tblpag, cpu->dsrpag,
                   cpu->dswpag, stack_high & 0x007fu, stack_low & 0xfffeu);
        }
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            return cpu->stop_reason;
        }
    }
    cpu->stop_reason = DSPIC33_INSTRUCTION_LIMIT;
    return cpu->stop_reason;
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
    uint32_t stop_address;
    uint32_t dump_memory_address;
    Dspic33StopReason reason;
    uint8_t reg;
    char error[160];

    if (argc >= 2 && strcmp(argv[1], "--suite") == 0) {
        return firmware_runner_main(argc, argv);
    }
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
    if (arguments.program_word_set &&
        !dspic33_load_program_word(&cpu, arguments.program_word_address,
                                   arguments.program_word_value)) {
        fprintf(stderr, "[error] invalid program word address 0x%08" PRIx32 "\n",
                arguments.program_word_address);
        dspic33_destroy(&cpu);
        firmware_image_close(&image);
        return 1;
    }
    dspic33_reset(&cpu, entry);
    for (reg = 0u; reg < 16u; reg++) {
        if (arguments.register_set[reg]) {
            dspic33_set_working_register(&cpu, reg, arguments.register_value[reg]);
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
    if (arguments.stop_symbol != NULL) {
        if (!resolve_location(&image, arguments.stop_symbol, &stop_address, error,
                              sizeof(error))) {
            fprintf(stderr, "[error] %s\n", error);
            dspic33_destroy(&cpu);
            firmware_image_close(&image);
            return 1;
        }
        reason =
            arguments.trace_address_set
                ? run_with_trace(&cpu, stop_address, true, arguments.instruction_limit,
                                 arguments.trace_address)
                : dspic33_run_until(&cpu, stop_address, arguments.instruction_limit);
    } else {
        reason = arguments.trace_address_set
                     ? run_with_trace(&cpu, 0u, false, arguments.instruction_limit,
                                      arguments.trace_address)
                     : dspic33_run(&cpu, arguments.instruction_limit);
    }
    if (arguments.dump_memory_symbol != NULL) {
        if (!resolve_location(&image, arguments.dump_memory_symbol,
                              &dump_memory_address, error, sizeof(error))) {
            fprintf(stderr, "[error] %s\n", error);
            dspic33_destroy(&cpu);
            firmware_image_close(&image);
            return 1;
        }
        if (dump_memory_address > DSPIC33_DATA_SIZE - arguments.dump_memory_size) {
            fprintf(stderr, "[error] memory dump exceeds data memory\n");
            dspic33_destroy(&cpu);
            firmware_image_close(&image);
            return 1;
        }
    }
    printf("[%s] pc=0x%06" PRIx32 " instructions=%" PRIu64 " W0=0x%04x SR=0x%04x\n",
           reason == DSPIC33_RETURNED || reason == DSPIC33_STOPPED ||
                   reason == DSPIC33_SLEEPING || reason == DSPIC33_IDLING
               ? "passed"
               : "failed",
           cpu.pc, cpu.instructions, cpu.w[0], cpu.sr);
    if (reason == DSPIC33_UNSUPPORTED_INSTRUCTION) {
        printf("[detail] unsupported opcode=0x%06" PRIx32 "\n", cpu.unsupported_opcode);
    } else if (reason != DSPIC33_RETURNED) {
        printf("[detail] stop reason=%s\n", dspic33_stop_reason_name(reason));
    }
    if (arguments.dump_registers) {
        for (reg = 0u; reg < 16u; reg++) {
            printf("W%u=0x%04x%s", reg, cpu.w[reg], reg == 15u ? "\n" : " ");
        }
    }
    if (arguments.dump_memory_symbol != NULL) {
        uint32_t offset;
        printf("[memory] %s address=0x%04" PRIx32 " size=%" PRIu32 " data=",
               arguments.dump_memory_symbol, dump_memory_address,
               arguments.dump_memory_size);
        for (offset = 0u; offset < arguments.dump_memory_size; offset++) {
            printf("%02x", dspic33_read_byte(&cpu, dump_memory_address + offset));
        }
        printf("\n");
    }
    dspic33_destroy(&cpu);
    firmware_image_close(&image);
    return reason == DSPIC33_RETURNED || reason == DSPIC33_STOPPED ||
                   reason == DSPIC33_SLEEPING || reason == DSPIC33_IDLING
               ? 0
               : 1;
}
