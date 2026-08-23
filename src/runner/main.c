#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dspic33.h"
#include "dspic33_firmware_image.h"

enum { PROGRAM_WORD_LIMIT = 8 };

typedef struct {
    uint32_t address;
    uint32_t value;
} ProgramWord;

typedef struct {
    const char* image_path;
    const char* reset_location;
    const char* stop_location;
    Dspic33epMuDevice device;
    Dspic33RunLimits limits;
    ProgramWord program_words[PROGRAM_WORD_LIMIT];
    uint8_t program_word_count;
} Arguments;

static bool parse_u64(const char* text, uint64_t maximum, uint64_t* parsed_value) {
    char* end = NULL;
    errno = 0;
    const unsigned long long parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > maximum) {
        return false;
    }
    *parsed_value = (uint64_t)parsed;
    return true;
}

static bool parse_arguments(int argc, char** argv, Arguments* arguments) {
    if (argc < 4) {
        return false;
    }
    *arguments = (Arguments){0};
    arguments->image_path = argv[1];
    arguments->device = DSPIC33EP_MU_DEVICE_512MU810;
    arguments->limits = (Dspic33RunLimits){1000000u, 10000000u};
    for (int argument_index = 2; argument_index < argc; argument_index++) {
        uint64_t parsed_value;
        if (strcmp(argv[argument_index], "--reset-address") == 0 && argument_index + 1 < argc) {
            arguments->reset_location = argv[++argument_index];
        } else if (strcmp(argv[argument_index], "--device") == 0 && argument_index + 1 < argc) {
            if (!dspic33ep_mu_device_from_name(argv[++argument_index], &arguments->device)) {
                return false;
            }
        } else if (strcmp(argv[argument_index], "--stop-address") == 0 &&
                   argument_index + 1 < argc) {
            arguments->stop_location = argv[++argument_index];
        } else if (strcmp(argv[argument_index], "--max-instructions") == 0 &&
                   argument_index + 1 < argc) {
            if (!parse_u64(argv[++argument_index], UINT64_MAX, &parsed_value)) {
                return false;
            }
            arguments->limits.instruction_limit = parsed_value;
        } else if (strcmp(argv[argument_index], "--max-cycles") == 0 && argument_index + 1 < argc) {
            if (!parse_u64(argv[++argument_index], UINT64_MAX, &parsed_value)) {
                return false;
            }
            arguments->limits.cycle_limit = parsed_value;
        } else if (strcmp(argv[argument_index], "--program-word") == 0 &&
                   argument_index + 2 < argc) {
            if (arguments->program_word_count == PROGRAM_WORD_LIMIT) {
                return false;
            }
            ProgramWord* program_word = &arguments->program_words[arguments->program_word_count];
            if (!parse_u64(argv[++argument_index], UINT32_MAX, &parsed_value)) {
                return false;
            }
            program_word->address = (uint32_t)parsed_value;
            if (!parse_u64(argv[++argument_index], 0x00ffffffu, &parsed_value)) {
                return false;
            }
            program_word->value = (uint32_t)parsed_value;
            arguments->program_word_count++;
        } else {
            return false;
        }
    }
    return arguments->reset_location != NULL;
}

static void print_usage(const char* program) {
    fprintf(stderr,
            "usage: %s IMAGE --reset-address ADDRESS "
            "[--device DEVICE] [--max-instructions COUNT] [--max-cycles COUNT] "
            "[--stop-address ADDRESS] [--program-word ADDRESS VALUE]\n",
            program);
}

static bool resolve_location(const FirmwareImage* image, const char* text, uint32_t* address,
                             char* error, size_t error_size) {
    uint64_t numeric;
    if (parse_u64(text, UINT32_MAX, &numeric)) {
        *address = (uint32_t)numeric;
        return true;
    }
    return firmware_image_symbol(image, text, address, error, error_size);
}

static Dspic33Result run(Dspic33* cpu, Dspic33RunLimits limits, uint32_t stop_address,
                         bool stop_enabled) {
    if (!stop_enabled) {
        return dspic33_run_with_limits(cpu, limits);
    }
    const uint64_t start_instructions = dspic33_get_instruction_count(cpu);
    const uint64_t start_cycles = dspic33_get_cycle_count(cpu);
    Dspic33Result result = {DSPIC33_RUNNING, start_instructions, start_cycles,
                            dspic33_get_program_counter(cpu),
                            dspic33_read_program_word(cpu, dspic33_get_program_counter(cpu))};
    while (result.stop == DSPIC33_RUNNING) {
        if (result.pc == stop_address) {
            result.stop = DSPIC33_STOPPED;
            return result;
        }
        if ((limits.instruction_limit != 0u &&
             result.instructions - start_instructions >= limits.instruction_limit) ||
            (limits.cycle_limit != 0u && result.cycles - start_cycles >= limits.cycle_limit)) {
            result.stop = DSPIC33_INSTRUCTION_LIMIT;
            return result;
        }
        result = dspic33_step_result(cpu);
    }
    return result;
}

static bool failed(Dspic33StopReason reason) {
    return reason == DSPIC33_HALTED || reason == DSPIC33_TRAPPED ||
           reason == DSPIC33_UNSUPPORTED_INSTRUCTION || reason == DSPIC33_PROGRAM_BOUNDS ||
           reason == DSPIC33_EVENT_QUEUE_ERROR || reason == DSPIC33_SILICON_RESULT_UNDEFINED;
}

int main(int argc, char** argv) {
    Arguments arguments;
    if (!parse_arguments(argc, argv, &arguments)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    FirmwareImage image;
    char error[160] = {0};
    if (!firmware_image_open(&image, arguments.image_path, error, sizeof(error))) {
        fprintf(stderr, "failed to open the firmware image: %s\n", error);
        return EXIT_FAILURE;
    }
    Dspic33* cpu = dspic33_create_for_device(arguments.device);
    if (cpu == NULL) {
        fprintf(stderr, "failed to create the device\n");
        firmware_image_close(&image);
        return EXIT_FAILURE;
    }
    uint32_t entry;
    if (!firmware_image_load_program(&image, cpu, error, sizeof(error)) ||
        !resolve_location(&image, arguments.reset_location, &entry, error, sizeof(error))) {
        fprintf(stderr, "failed to load the firmware image: %s\n", error);
        dspic33_destroy(cpu);
        firmware_image_close(&image);
        return EXIT_FAILURE;
    }
    for (uint8_t word_index = 0u; word_index < arguments.program_word_count; word_index++) {
        const ProgramWord program_word = arguments.program_words[word_index];
        if (!dspic33_load_program_word(cpu, program_word.address, program_word.value)) {
            fprintf(stderr, "invalid program word address: 0x%08" PRIx32 "\n",
                    program_word.address);
            dspic33_destroy(cpu);
            firmware_image_close(&image);
            return EXIT_FAILURE;
        }
    }
    uint32_t stop_address = 0u;
    const bool stop_enabled = arguments.stop_location != NULL;
    if (stop_enabled &&
        !resolve_location(&image, arguments.stop_location, &stop_address, error, sizeof(error))) {
        fprintf(stderr, "invalid stop address: %s\n", error);
        dspic33_destroy(cpu);
        firmware_image_close(&image);
        return EXIT_FAILURE;
    }
    dspic33_reset(cpu, entry);
    const Dspic33Result result = run(cpu, arguments.limits, stop_address, stop_enabled);
    printf("stop=%u pc=0x%08" PRIx32 " opcode=0x%08" PRIx32 " instructions=%" PRIu64
           " cycles=%" PRIu64 " entry=0x%08" PRIx32 " fault=0x%08" PRIx32 "\n",
           result.stop, result.pc, result.opcode, result.instructions, result.cycles, entry,
           dspic33_get_fault_address(cpu));
    for (uint8_t register_index = 0u; register_index < 16u; register_index++) {
        printf("W%u=0x%04" PRIx32 "%c", register_index, dspic33_get_register(cpu, register_index),
               register_index == 15u ? '\n' : ' ');
    }
    dspic33_destroy(cpu);
    firmware_image_close(&image);
    return failed(result.stop) ? EXIT_FAILURE : EXIT_SUCCESS;
}
