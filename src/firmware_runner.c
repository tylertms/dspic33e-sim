#include "firmware_runner.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "dspic33.h"
#include "firmware_image.h"
#include "json.h"
#include "scenario_stream.h"

enum { MAX_STEP_PARTS = 32, MAX_MATRIX_DIMENSIONS = 16 };

typedef struct RunTask RunTask;

typedef struct {
    const JsonValue* suite;
    const char* suite_path;
    char suite_directory[1024];
    const char* scenario_filter;
    const char* step_filter;
    bool failures_only;
    size_t scenarios;
    size_t steps;
    size_t current_scenario;
    size_t current_step;
    size_t passed;
    size_t failed;
    size_t comparisons;
    uint64_t reference_instructions;
    uint64_t candidate_instructions;
    uint64_t reference_cycles;
    uint64_t candidate_cycles;
    uint64_t instruction_limit;
    FirmwareImage reference_image;
    FirmwareImage candidate_image;
    Dspic33 reference;
    Dspic33 candidate;
    Dspic33 reference_baseline;
    Dspic33 candidate_baseline;
    bool restore_baseline;
#ifdef _WIN32
    RunTask* run_tasks;
#endif
} Runner;

typedef struct {
    const JsonValue* items[MAX_STEP_PARTS];
    size_t count;
} StepParts;

typedef struct {
    const JsonValue* specification;
    const JsonValue* explicit_case;
    int64_t value;
    bool generated;
} MatrixSelection;

static bool parse_number(const JsonValue* value, int64_t* result) {
    const char* string;
    char* end;
    long long parsed;
    if (json_integer(value, result)) {
        return true;
    }
    string = json_string(value);
    if (string == NULL) {
        return false;
    }
    parsed = strtoll(string, &end, 0);
    if (*string == '\0' || *end != '\0') {
        return false;
    }
    *result = (int64_t)parsed;
    return true;
}

static bool parse_hex_bytes(const char* text, uint8_t* bytes, size_t size) {
    size_t index;
    if (text == NULL) {
        return false;
    }
    for (index = 0u; index < size; index++) {
        char* end;
        unsigned long value;
        while (*text == ' ' || *text == '\t') {
            text++;
        }
        if (*text == '\0') {
            return false;
        }
        value = strtoul(text, &end, 16);
        if (end == text || value > UINT8_MAX ||
            (*end != '\0' && *end != ' ' && *end != '\t')) {
            return false;
        }
        bytes[index] = (uint8_t)value;
        text = end;
    }
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return *text == '\0';
}

static bool wildcard_matches(const char* pattern, const char* value) {
    const char* star = NULL;
    const char* retry = NULL;
    while (*value != '\0') {
        if (*pattern == '?' || *pattern == *value) {
            pattern++;
            value++;
        } else if (*pattern == '*') {
            star = pattern++;
            retry = value;
        } else if (star != NULL) {
            pattern = star + 1;
            value = ++retry;
        } else {
            return false;
        }
    }
    while (*pattern == '*') {
        pattern++;
    }
    return *pattern == '\0';
}

static bool scenario_selected(const Runner* runner, const JsonValue* scenario) {
    const char* id = json_string(json_get(scenario, "id"));
    const char* name = json_string(json_get(scenario, "name"));
    return runner->scenario_filter == NULL ||
           (id != NULL && wildcard_matches(runner->scenario_filter, id)) ||
           (name != NULL && wildcard_matches(runner->scenario_filter, name));
}

static bool step_selected(const Runner* runner, const JsonValue* step) {
    const char* id = json_string(json_get(step, "id"));
    const char* name = json_string(json_get(step, "name"));
    return runner->step_filter == NULL ||
           (id != NULL && wildcard_matches(runner->step_filter, id)) ||
           (name != NULL && wildcard_matches(runner->step_filter, name));
}

static bool generated_step_selected(const Runner* runner, const char* name) {
    return runner->step_filter == NULL || wildcard_matches(runner->step_filter, name);
}

static size_t generated_step_count(const Runner* runner, const char* scenario_name,
                                   const char* kind, size_t count) {
    size_t selected = 0u;
    size_t index;
    char name[512];
    if (runner->step_filter == NULL) {
        return count;
    }
    for (index = 0u; index < count; index++) {
        snprintf(name, sizeof(name), "%s %s %zu", scenario_name, kind, index + 1u);
        if (generated_step_selected(runner, name)) {
            selected++;
        }
    }
    return selected;
}

static const JsonValue* named_entry(const JsonValue* object, const char* name) {
    return json_get(object, name);
}

static const JsonValue* suite_section(const Runner* runner, const char* name) {
    return json_get(runner->suite, name);
}

static const JsonValue* location_entry(const Runner* runner,
                                       const JsonValue* specification) {
    const char* name;
    const JsonValue* location;
    if (specification == NULL) {
        return NULL;
    }
    if (specification->type == JSON_STRING) {
        name = json_string(specification);
    } else {
        location = json_get(specification, "location");
        name = json_string(location);
        if (name == NULL) {
            return specification;
        }
    }
    return named_entry(suite_section(runner, "locations"), name);
}

static const JsonValue* entry_field(const Runner* runner,
                                    const JsonValue* specification, const char* name) {
    const JsonValue* value = json_get(specification, name);
    const JsonValue* location;
    if (value != NULL) {
        return value;
    }
    location = location_entry(runner, specification);
    return location == specification ? NULL : json_get(location, name);
}

static bool mapped_address(const Runner* runner, const JsonValue* specification,
                           bool candidate, uint32_t* result, char* error,
                           size_t error_size) {
    const JsonValue* value;
    const char* symbol;
    int64_t address;
    int64_t common_offset = 0;
    int64_t image_offset = 0;
    if (candidate) {
        value = entry_field(runner, specification, "candidate_symbol");
        symbol = json_string(value);
        if (symbol != NULL) {
            if (!firmware_image_symbol(&runner->candidate_image, symbol, result, error,
                                       error_size)) {
                return false;
            }
        } else {
            value = entry_field(runner, specification, "candidate_address");
            if (value == NULL) {
                value = entry_field(runner, specification, "reference_address");
            }
            if (value == NULL) {
                value = entry_field(runner, specification, "address");
            }
            if (!parse_number(value, &address) || address < 0 || address > UINT32_MAX) {
                snprintf(error, error_size, "candidate location has no address");
                return false;
            }
            *result = (uint32_t)address;
        }
        value = entry_field(runner, specification, "candidate_offset");
    } else {
        value = entry_field(runner, specification, "reference_address");
        if (value == NULL) {
            value = entry_field(runner, specification, "address");
        }
        if (!parse_number(value, &address) || address < 0 || address > UINT32_MAX) {
            snprintf(error, error_size, "reference location has no address");
            return false;
        }
        *result = (uint32_t)address;
        value = entry_field(runner, specification, "reference_offset");
    }
    if (value != NULL && !parse_number(value, &image_offset)) {
        snprintf(error, error_size, "location offset is invalid");
        return false;
    }
    value = entry_field(runner, specification, "offset");
    if (value != NULL && !parse_number(value, &common_offset)) {
        snprintf(error, error_size, "location offset is invalid");
        return false;
    }
    *result = (uint32_t)(*result + image_offset + common_offset);
    return true;
}

static bool add_part(StepParts* parts, const JsonValue* value, char* error,
                     size_t error_size) {
    if (value == NULL) {
        return true;
    }
    if (value->type != JSON_OBJECT || parts->count == MAX_STEP_PARTS) {
        snprintf(error, error_size, "step has too many or invalid overlays");
        return false;
    }
    parts->items[parts->count++] = value;
    return true;
}

static const JsonValue* scalar_field(const StepParts* parts, const char* name) {
    size_t index = parts->count;
    while (index != 0u) {
        const JsonValue* value = json_get(parts->items[--index], name);
        if (value != NULL) {
            return value;
        }
    }
    return NULL;
}

static size_t fixture_step_count(const Runner* runner, const char* name,
                                 unsigned depth) {
    const JsonValue* fixture;
    const JsonValue* nested;
    const JsonValue* steps;
    size_t count = 0u;
    size_t index;
    if (depth > 32u) {
        return 0u;
    }
    fixture = named_entry(suite_section(runner, "fixtures"), name);
    if (fixture == NULL) {
        return 0u;
    }
    if (fixture->type == JSON_ARRAY) {
        return fixture->as.array.count;
    }
    nested = json_get(fixture, "fixtures");
    if (nested != NULL && nested->type == JSON_ARRAY) {
        for (index = 0u; index < nested->as.array.count; index++) {
            const char* child = json_string(nested->as.array.items[index]);
            if (child != NULL) {
                count += fixture_step_count(runner, child, depth + 1u);
            }
        }
    }
    steps = json_get(fixture, "steps");
    if (steps != NULL && steps->type == JSON_ARRAY) {
        count += steps->as.array.count;
    }
    return count;
}

static size_t values_count(const Runner* runner, const JsonValue* specification) {
    const char* set_name = json_string(json_get(specification, "values"));
    const JsonValue* values;
    int64_t start;
    int64_t end;
    int64_t stride = 1;
    if (set_name != NULL) {
        values = named_entry(suite_section(runner, "value_sets"), set_name);
        return values != NULL && values->type == JSON_ARRAY ? values->as.array.count
                                                            : 0u;
    }
    if (!parse_number(json_get(specification, "start"), &start) ||
        !parse_number(json_get(specification, "end"), &end)) {
        return 0u;
    }
    if (json_get(specification, "stride") != NULL &&
        !parse_number(json_get(specification, "stride"), &stride)) {
        return 0u;
    }
    return stride > 0 && end >= start ? (size_t)((end - start) / stride + 1) : 0u;
}

static size_t matrix_count(const Runner* runner, const JsonValue* matrix) {
    const JsonValue* dimensions = json_get(matrix, "dimensions");
    const char* strategy = json_string(json_get(matrix, "strategy"));
    size_t counts[MAX_MATRIX_DIMENSIONS];
    size_t index;
    size_t result = 1u;
    if (dimensions == NULL || dimensions->type != JSON_ARRAY ||
        dimensions->as.array.count > MAX_MATRIX_DIMENSIONS) {
        return 0u;
    }
    for (index = 0u; index < dimensions->as.array.count; index++) {
        const JsonValue* dimension = dimensions->as.array.items[index];
        counts[index] = dimension->type == JSON_ARRAY ? dimension->as.array.count
                                                      : values_count(runner, dimension);
        if (counts[index] == 0u) {
            return 0u;
        }
    }
    if (strategy == NULL || strcmp(strategy, "product") == 0) {
        for (index = 0u; index < dimensions->as.array.count; index++) {
            result *= counts[index];
        }
        return result;
    }
    if (strcmp(strategy, "pairwise") == 0) {
        size_t left;
        result = 1u;
        for (index = 0u; index < dimensions->as.array.count; index++) {
            result += counts[index] - 1u;
        }
        for (left = 0u; left < dimensions->as.array.count; left++) {
            for (index = left + 1u; index < dimensions->as.array.count; index++) {
                result += (counts[left] - 1u) * (counts[index] - 1u);
            }
        }
        return result;
    }
    return true;
}

static size_t scenario_step_count(const Runner* runner, const JsonValue* scenario) {
    const JsonValue* fixtures = json_get(scenario, "fixtures");
    const JsonValue* mode;
    const char* scenario_name = json_string(json_get(scenario, "name"));
    size_t count = 0u;
    size_t index;
    if (fixtures != NULL && fixtures->type == JSON_ARRAY) {
        for (index = 0u; index < fixtures->as.array.count; index++) {
            const char* name = json_string(fixtures->as.array.items[index]);
            if (name != NULL) {
                count += fixture_step_count(runner, name, 0u);
            }
        }
    }
    mode = json_get(scenario, "steps");
    if (mode == NULL) {
        mode = json_get(scenario, "vectors");
    }
    if (mode != NULL && mode->type == JSON_ARRAY) {
        size_t selected = 0u;
        for (index = 0u; index < mode->as.array.count; index++) {
            if (step_selected(runner, mode->as.array.items[index])) {
                selected++;
            }
        }
        return selected == 0u ? 0u : count + selected;
    }
    mode = json_get(scenario, "range");
    if (mode != NULL) {
        return runner->step_filter == NULL ? count + values_count(runner, mode) : 0u;
    }
    mode = json_get(scenario, "matrix");
    if (mode != NULL) {
        size_t selected = generated_step_count(runner, scenario_name, "matrix case",
                                               matrix_count(runner, mode));
        return selected == 0u ? 0u : count + selected;
    }
    return count;
}

static bool count_scenario(const char* path, const JsonValue* scenario, void* context,
                           char* error, size_t error_size) {
    Runner* runner = context;
    size_t count;
    (void)path;
    if (!scenario_selected(runner, scenario)) {
        return true;
    }
    count = scenario_step_count(runner, scenario);
    if (count == 0u && runner->step_filter != NULL) {
        return true;
    }
    if (count == 0u) {
        snprintf(error, error_size, "scenario has no executable steps: %s",
                 json_string(json_get(scenario, "id")));
        return false;
    }
    runner->scenarios++;
    runner->steps += count;
    return true;
}

static bool build_pattern(const Runner* runner, const char* relative, char* path,
                          size_t size) {
    int length;
    if (relative[0] == '/' || relative[0] == '\\' ||
        (strlen(relative) > 2u && relative[1] == ':')) {
        length = snprintf(path, size, "%s", relative);
    } else {
        length = snprintf(path, size, "%s\\%s", runner->suite_directory, relative);
    }
    return length >= 0 && (size_t)length < size;
}

static bool stream_patterns(Runner* runner, ScenarioVisitor visitor, char* error,
                            size_t error_size) {
    const JsonValue* patterns = suite_section(runner, "scenario_files");
    size_t index;
    if (patterns == NULL || patterns->type != JSON_ARRAY) {
        snprintf(error, error_size, "suite has no scenario_files array");
        return false;
    }
    for (index = 0u; index < patterns->as.array.count; index++) {
        const char* relative = json_string(patterns->as.array.items[index]);
        char pattern[2048];
        if (relative == NULL ||
            !build_pattern(runner, relative, pattern, sizeof(pattern)) ||
            !scenario_stream(pattern, visitor, runner, error, error_size)) {
            return false;
        }
    }
    return true;
}

static bool suite_directory(const char* path, char* directory, size_t size) {
    const char* slash = strrchr(path, '\\');
    const char* forward = strrchr(path, '/');
    size_t length;
    if (forward != NULL && (slash == NULL || forward > slash)) {
        slash = forward;
    }
    if (slash == NULL) {
        return snprintf(directory, size, ".") > 0;
    }
    length = (size_t)(slash - path);
    if (length + 1u > size) {
        return false;
    }
    memcpy(directory, path, length);
    directory[length] = '\0';
    return true;
}

static bool open_images(Runner* runner, char* error, size_t error_size) {
    const char* reference_path =
        json_string(json_get(suite_section(runner, "reference"), "path"));
    const char* candidate_path =
        json_string(json_get(suite_section(runner, "candidate"), "path"));
    if (reference_path == NULL || candidate_path == NULL) {
        snprintf(error, error_size, "suite image paths are invalid");
        return false;
    }
    printf("[prepare] Loading reference image\n");
    fflush(stdout);
    if (!firmware_image_open(&runner->reference_image, reference_path, error,
                             error_size)) {
        return false;
    }
    printf("[prepare] Loading candidate image\n");
    fflush(stdout);
    if (!firmware_image_open(&runner->candidate_image, candidate_path, error,
                             error_size)) {
        firmware_image_close(&runner->reference_image);
        return false;
    }
    if (!dspic33_initialize(&runner->reference) ||
        !dspic33_initialize(&runner->candidate) ||
        !dspic33_initialize(&runner->reference_baseline) ||
        !dspic33_initialize(&runner->candidate_baseline) ||
        !firmware_image_load_program(&runner->reference_image, &runner->reference,
                                     error, error_size) ||
        !firmware_image_load_program(&runner->candidate_image, &runner->candidate,
                                     error, error_size)) {
        return false;
    }
    return true;
}

static void close_images(Runner* runner) {
    dspic33_destroy(&runner->reference);
    dspic33_destroy(&runner->candidate);
    dspic33_destroy(&runner->reference_baseline);
    dspic33_destroy(&runner->candidate_baseline);
    firmware_image_close(&runner->reference_image);
    firmware_image_close(&runner->candidate_image);
}

static bool register_index(const char* name, uint8_t* result) {
    char* end;
    long value;
    if (name == NULL || name[0] != 'W') {
        return false;
    }
    value = strtol(name + 1, &end, 10);
    if (*end != '\0' || value < 0 || value > 15) {
        return false;
    }
    *result = (uint8_t)value;
    return true;
}

static bool pin_index(const char* name, uint8_t* port, uint8_t* bit) {
    char* end;
    long value;
    if (name == NULL || name[0] != 'R' || name[1] < 'A' || name[1] > 'G') {
        return false;
    }
    value = strtol(name + 2, &end, 10);
    if (*end != '\0' || value < 0 || value > 15) {
        return false;
    }
    *port = (uint8_t)(name[1] - 'A');
    *bit = (uint8_t)value;
    return true;
}

static bool field_number(const Runner* runner, const JsonValue* specification,
                         const char* name, int64_t default_value, int64_t* result) {
    const JsonValue* value = entry_field(runner, specification, name);
    if (value == NULL) {
        *result = default_value;
        return true;
    }
    return parse_number(value, result);
}

static uint8_t read_memory_byte(Dspic33* cpu, const char* space, uint32_t address) {
    if (space != NULL && strcmp(space, "program") == 0) {
        return dspic33_read_program_byte(cpu, address);
    }
    if (space != NULL && strcmp(space, "configuration") == 0) {
        return dspic33_read_configuration_byte(cpu, address);
    }
    return dspic33_read_byte(cpu, address);
}

static bool write_memory_value(Runner* runner, Dspic33* cpu, const FirmwareImage* image,
                               const JsonValue* specification, bool candidate,
                               char* error, size_t error_size) {
    uint32_t address;
    const JsonValue* bytes = json_get(specification, "bytes");
    const JsonValue* words = json_get(specification, "words");
    const JsonValue* value_location = json_get(specification, "value_location");
    const char* space = json_string(entry_field(runner, specification, "space"));
    int64_t repeat_value = 1;
    size_t repeat;
    size_t index;
    if (!mapped_address(runner, specification, candidate, &address, error,
                        error_size) ||
        !field_number(runner, specification, "repeat", 1, &repeat_value) ||
        repeat_value < 1) {
        snprintf(error, error_size, "invalid memory stimulus");
        return false;
    }
    repeat = (size_t)repeat_value;
    if (value_location != NULL) {
        uint32_t location;
        int64_t offset = 0;
        if (!mapped_address(runner, value_location, candidate, &location, error,
                            error_size) ||
            (json_get(specification, "value_location_offset") != NULL &&
             !parse_number(json_get(specification, "value_location_offset"),
                           &offset))) {
            return false;
        }
        for (index = 0u; index < repeat; index++) {
            dspic33_write_word(cpu, address + (uint32_t)index * 2u,
                               (uint16_t)(location + offset));
        }
        return true;
    }
    if (bytes != NULL && bytes->type == JSON_ARRAY) {
        size_t item;
        for (index = 0u; index < repeat; index++) {
            for (item = 0u; item < bytes->as.array.count; item++) {
                int64_t value;
                if (!parse_number(bytes->as.array.items[item], &value) || value < 0 ||
                    value > UINT8_MAX) {
                    snprintf(error, error_size, "invalid byte stimulus");
                    return false;
                }
                if (space != NULL && strcmp(space, "program") == 0) {
                    snprintf(error, error_size,
                             "program byte stimuli must use 24-bit words");
                    return false;
                }
                if (space != NULL && strcmp(space, "configuration") == 0) {
                    snprintf(error, error_size,
                             "configuration stimuli must use 24-bit words");
                    return false;
                }
                dspic33_write_byte(
                    cpu, address + (uint32_t)(index * bytes->as.array.count + item),
                    (uint8_t)value);
            }
        }
        return true;
    }
    if (words == NULL || words->type != JSON_ARRAY) {
        snprintf(error, error_size, "memory stimulus has no values");
        return false;
    }
    for (index = 0u; index < repeat * words->as.array.count; index++) {
        int64_t value;
        if (!parse_number(words->as.array.items[index % words->as.array.count],
                          &value) ||
            value < 0 || value > 0x00ffffff) {
            snprintf(error, error_size, "invalid word stimulus");
            return false;
        }
        if (space != NULL && strcmp(space, "program") == 0) {
            if (!dspic33_load_program_word(cpu, address + (uint32_t)index * 2u,
                                           (uint32_t)value)) {
                snprintf(error, error_size, "program stimulus is out of range");
                return false;
            }
        } else if (space != NULL && strcmp(space, "configuration") == 0) {
            if (!dspic33_load_configuration_word(cpu, address + (uint32_t)index * 2u,
                                                 (uint32_t)value)) {
                snprintf(error, error_size, "configuration stimulus is out of range");
                return false;
            }
        } else {
            dspic33_write_word(cpu, address + (uint32_t)index * 2u, (uint16_t)value);
        }
    }
    (void)image;
    return true;
}

static bool apply_register(Runner* runner, Dspic33* cpu, const FirmwareImage* image,
                           const JsonValue* specification, bool candidate, char* error,
                           size_t error_size) {
    const char* name = json_string(json_get(specification, "name"));
    const JsonValue* value_entry = json_get(specification, "value");
    int64_t value;
    uint8_t reg;
    uint32_t address;
    if (!register_index(name, &reg)) {
        snprintf(error, error_size, "invalid working register: %s",
                 name == NULL ? "<missing>" : name);
        return false;
    }
    if (value_entry != NULL) {
        if (!parse_number(value_entry, &value)) {
            snprintf(error, error_size, "invalid register stimulus");
            return false;
        }
    } else if (mapped_address(runner, specification, candidate, &address, error,
                              error_size)) {
        int64_t offset = 0;
        if (json_get(specification, "register_offset") != NULL &&
            !parse_number(json_get(specification, "register_offset"), &offset)) {
            return false;
        }
        value = address + offset;
    } else {
        (void)image;
        return false;
    }
    cpu->w[reg] = (uint16_t)value;
    return true;
}

static bool apply_pin(Dspic33* cpu, const JsonValue* specification, char* error,
                      size_t error_size) {
    const char* name = json_string(json_get(specification, "name"));
    const char* value = json_string(json_get(specification, "value"));
    uint8_t port;
    uint8_t bit;
    uint16_t pins;
    bool high;
    if (!pin_index(name, &port, &bit) || value == NULL) {
        snprintf(error, error_size, "invalid pin stimulus");
        return false;
    }
    high = strcmp(value, "3.3V") == 0 || strcmp(value, "1") == 0;
    pins = cpu->io.gpio[port];
    if (high) {
        pins |= (uint16_t)(1u << bit);
    } else {
        pins &= (uint16_t)~(1u << bit);
    }
    dspic33_gpio_input(cpu, port, pins);
    return true;
}

static bool event_number(const JsonValue* specification, const char* name,
                         uint64_t maximum, uint64_t default_value, bool required,
                         uint64_t* result) {
    const JsonValue* value = json_get(specification, name);
    int64_t parsed;
    if (value == NULL) {
        if (required) {
            return false;
        }
        *result = default_value;
        return true;
    }
    if (!parse_number(value, &parsed) || parsed < 0 || (uint64_t)parsed > maximum) {
        return false;
    }
    *result = (uint64_t)parsed;
    return true;
}

static bool apply_device_stimulus(Dspic33* cpu, const char* type,
                                  const JsonValue* specification, char* error,
                                  size_t error_size) {
    uint64_t channel = 0u;
    uint64_t value = 0u;
    uint64_t delay = 0u;
    bool succeeded = false;
    if (!event_number(specification, "delay", UINT64_MAX, 0u, false, &delay)) {
        snprintf(error, error_size, "invalid %s stimulus delay", type);
        return false;
    }
    if (strcmp(type, "interrupts") == 0) {
        succeeded = event_number(specification, "irq", DSPIC33_IRQ_COUNT - 1u, 0u, true,
                                 &channel) &&
                    dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, (uint16_t)channel,
                                     0u, delay);
    } else if (strcmp(type, "uart_rx") == 0) {
        succeeded = event_number(specification, "channel", DSPIC33_UART_COUNT - 1u, 0u,
                                 true, &channel) &&
                    event_number(specification, "value", UINT8_MAX, 0u, true, &value) &&
                    dspic33_uart_receive(cpu, (uint8_t)channel, (uint8_t)value, delay);
    } else if (strcmp(type, "spi_rx") == 0) {
        succeeded =
            event_number(specification, "channel", DSPIC33_SPI_COUNT - 1u, 0u, true,
                         &channel) &&
            event_number(specification, "value", UINT16_MAX, 0u, true, &value) &&
            dspic33_spi_receive(cpu, (uint8_t)channel, (uint16_t)value, delay);
    } else if (strcmp(type, "adc") == 0) {
        succeeded = event_number(specification, "channel",
                                 DSPIC33_ADC_CHANNEL_COUNT - 1u, 0u, true, &channel) &&
                    event_number(specification, "value", 0x0fffu, 0u, true, &value);
        if (succeeded) {
            dspic33_adc_input(cpu, (uint8_t)channel, (uint16_t)value);
        }
    } else if (strcmp(type, "can_rx") == 0) {
        const JsonValue* data = json_get(specification, "data");
        const JsonValue* extended_value = json_get(specification, "extended");
        const JsonValue* remote_value = json_get(specification, "remote");
        Dspic33CanFrame frame;
        size_t index;
        memset(&frame, 0, sizeof(frame));
        succeeded =
            event_number(specification, "channel", DSPIC33_CAN_COUNT - 1u, 0u, true,
                         &channel) &&
            event_number(specification, "identifier", 0x1fffffffu, 0u, true, &value) &&
            data != NULL && data->type == JSON_ARRAY &&
            data->as.array.count <= sizeof(frame.data);
        frame.identifier = (uint32_t)value;
        frame.length = succeeded ? (uint8_t)data->as.array.count : 0u;
        if (extended_value != NULL && !json_boolean(extended_value, &frame.extended)) {
            succeeded = false;
        }
        if (remote_value != NULL && !json_boolean(remote_value, &frame.remote)) {
            succeeded = false;
        }
        for (index = 0u; succeeded && index < frame.length; index++) {
            int64_t byte;
            succeeded = parse_number(data->as.array.items[index], &byte) && byte >= 0 &&
                        byte <= UINT8_MAX;
            frame.data[index] = (uint8_t)byte;
        }
        succeeded =
            succeeded && dspic33_can_receive(cpu, (uint8_t)channel, &frame, delay);
    } else if (strcmp(type, "usb_rx") == 0) {
        const JsonValue* data = json_get(specification, "data");
        uint8_t bytes[64];
        size_t index;
        succeeded = event_number(specification, "endpoint", 15u, 0u, true, &channel) &&
                    data != NULL && data->type == JSON_ARRAY &&
                    data->as.array.count <= sizeof(bytes);
        for (index = 0u; succeeded && index < data->as.array.count; index++) {
            int64_t byte;
            succeeded = parse_number(data->as.array.items[index], &byte) && byte >= 0 &&
                        byte <= UINT8_MAX;
            bytes[index] = (uint8_t)byte;
        }
        succeeded =
            succeeded && dspic33_usb_receive(cpu, (uint8_t)channel, bytes,
                                             (uint16_t)data->as.array.count, delay);
    }
    if (!succeeded) {
        snprintf(error, error_size, "invalid %s stimulus", type);
    }
    return succeeded;
}

static bool apply_device_stimuli_pair(Runner* runner, const JsonValue* stimuli,
                                      const char* type, char* error,
                                      size_t error_size) {
    const JsonValue* values = json_get(stimuli, type);
    size_t index;
    if (values == NULL) {
        return true;
    }
    if (values->type != JSON_ARRAY) {
        snprintf(error, error_size, "%s stimuli must be an array", type);
        return false;
    }
    for (index = 0u; index < values->as.array.count; index++) {
        if (!apply_device_stimulus(&runner->reference, type,
                                   values->as.array.items[index], error, error_size) ||
            !apply_device_stimulus(&runner->candidate, type,
                                   values->as.array.items[index], error, error_size)) {
            return false;
        }
    }
    return true;
}

static bool apply_stimuli_part(Runner* runner, const JsonValue* part, char* error,
                               size_t error_size) {
    const JsonValue* stimuli = json_get(part, "stimuli");
    const JsonValue* values;
    size_t index;
    if (stimuli == NULL) {
        return true;
    }
    values = json_get(stimuli, "memory");
    if (values != NULL && values->type == JSON_ARRAY) {
        for (index = 0u; index < values->as.array.count; index++) {
            if (!write_memory_value(
                    runner, &runner->reference, &runner->reference_image,
                    values->as.array.items[index], false, error, error_size) ||
                !write_memory_value(
                    runner, &runner->candidate, &runner->candidate_image,
                    values->as.array.items[index], true, error, error_size)) {
                return false;
            }
        }
    }
    values = json_get(stimuli, "registers");
    if (values != NULL && values->type == JSON_ARRAY) {
        for (index = 0u; index < values->as.array.count; index++) {
            if (!apply_register(runner, &runner->reference, &runner->reference_image,
                                values->as.array.items[index], false, error,
                                error_size) ||
                !apply_register(runner, &runner->candidate, &runner->candidate_image,
                                values->as.array.items[index], true, error,
                                error_size)) {
                return false;
            }
        }
    }
    values = json_get(stimuli, "pins");
    if (values != NULL && values->type == JSON_ARRAY) {
        for (index = 0u; index < values->as.array.count; index++) {
            if (!apply_pin(&runner->reference, values->as.array.items[index], error,
                           error_size) ||
                !apply_pin(&runner->candidate, values->as.array.items[index], error,
                           error_size)) {
                return false;
            }
        }
    }
    return apply_device_stimuli_pair(runner, stimuli, "interrupts", error,
                                     error_size) &&
           apply_device_stimuli_pair(runner, stimuli, "uart_rx", error, error_size) &&
           apply_device_stimuli_pair(runner, stimuli, "spi_rx", error, error_size) &&
           apply_device_stimuli_pair(runner, stimuli, "adc", error, error_size) &&
           apply_device_stimuli_pair(runner, stimuli, "can_rx", error, error_size) &&
           apply_device_stimuli_pair(runner, stimuli, "usb_rx", error, error_size);
}

static void reset_pair(Runner* runner) {
    dspic33_reset(&runner->reference, 0u);
    dspic33_reset(&runner->candidate, 0u);
    dspic33_gpio_input(&runner->reference, 1u, 1u);
    dspic33_gpio_input(&runner->candidate, 1u, 1u);
}

static bool save_baseline(Runner* runner, char* error, size_t error_size) {
    if (!dspic33_copy(&runner->reference_baseline, &runner->reference) ||
        !dspic33_copy(&runner->candidate_baseline, &runner->candidate)) {
        snprintf(error, error_size, "cannot save simulator baseline");
        return false;
    }
    return true;
}

static bool restore_baseline(Runner* runner, char* error, size_t error_size) {
    if (!dspic33_copy(&runner->reference, &runner->reference_baseline) ||
        !dspic33_copy(&runner->candidate, &runner->candidate_baseline)) {
        snprintf(error, error_size, "cannot restore simulator baseline");
        return false;
    }
    return true;
}

static void print_cpu_diagnostics(const Dspic33* cpu) {
    uint64_t start = cpu->interrupt_count > 4u ? cpu->interrupt_count - 4u : 0u;
    uint64_t item;
    for (item = start; item < cpu->interrupt_count; item++) {
        size_t index = (size_t)(item % 16u);
        printf("  interrupt #%-6" PRIu64 " irq=%u interrupted=0x%06" PRIx32
               " return=0x%06" PRIx32 "\n",
               item + 1u, cpu->interrupt_log_irq[index],
               cpu->interrupt_log_entry[index], cpu->interrupt_log_return[index]);
    }
    printf("  state PC=0x%06" PRIx32 " W0=0x%04x W1=0x%04x W2=0x%04x "
           "W3=0x%04x W4=0x%04x W5=0x%04x W6=0x%04x W7=0x%04x "
           "W8=0x%04x W9=0x%04x W10=0x%04x W11=0x%04x W12=0x%04x "
           "W13=0x%04x W14=0x%04x W15=0x%04x SR=0x%04x TBLPAG=0x%04x DSRPAG=0x%04x "
           "DSWPAG=0x%04x\n",
           cpu->pc, cpu->w[0], cpu->w[1], cpu->w[2], cpu->w[3], cpu->w[4], cpu->w[5],
           cpu->w[6], cpu->w[7], cpu->w[8], cpu->w[9], cpu->w[10], cpu->w[11],
           cpu->w[12], cpu->w[13], cpu->w[14], cpu->w[15], cpu->sr, cpu->tblpag,
           cpu->dsrpag, cpu->dswpag);
    printf("  control call-depth=%u interrupt-depth=%u instructions=%" PRIu64
           " cycles=%" PRIu64 "\n",
           cpu->call_depth, cpu->interrupt_depth, cpu->instructions, cpu->cycles);
}

static uint32_t stopped_opcode(const Dspic33* cpu) {
    if (cpu->unsupported_opcode != 0u) {
        return cpu->unsupported_opcode;
    }
    return cpu->pc < DSPIC33_PROGRAM_LIMIT ? cpu->program[cpu->pc / 2u] : 0u;
}

static bool run_image(Runner* runner, Dspic33* cpu, bool candidate,
                      const JsonValue* call, const JsonValue* stop, char* error,
                      size_t error_size, bool* execution_failure) {
    uint32_t address;
    Dspic33StopReason reason;
    *execution_failure = false;
    if (call != NULL) {
        if (!mapped_address(runner, call, candidate, &address, error, error_size)) {
            return false;
        }
        cpu->pc = address;
        cpu->call_depth = 0u;
    }
    if (stop != NULL) {
        if (!mapped_address(runner, stop, candidate, &address, error, error_size)) {
            return false;
        }
        reason = dspic33_run_until(cpu, address, runner->instruction_limit);
        if (reason != DSPIC33_STOPPED) {
            *execution_failure = true;
            snprintf(error, error_size,
                     "%s stopped with %s at 0x%06" PRIx32 " opcode=0x%06" PRIx32
                     " irq=%u irq-return=0x%06" PRIx32 " interrupts=%" PRIu64
                     " resets=%" PRIu64 " reset-irq=%u",
                     candidate ? "candidate" : "reference",
                     dspic33_stop_reason_name(reason), cpu->pc, stopped_opcode(cpu),
                     cpu->last_interrupt, cpu->last_interrupt_return,
                     cpu->interrupt_count, cpu->software_reset_count,
                     cpu->reset_interrupt);
            return false;
        }
    } else {
        reason = dspic33_run(cpu, runner->instruction_limit);
        if (reason != DSPIC33_RETURNED) {
            *execution_failure = true;
            snprintf(error, error_size,
                     "%s stopped with %s at 0x%06" PRIx32 " opcode=0x%06" PRIx32
                     " irq=%u irq-return=0x%06" PRIx32 " interrupts=%" PRIu64
                     " resets=%" PRIu64 " reset-irq=%u",
                     candidate ? "candidate" : "reference",
                     dspic33_stop_reason_name(reason), cpu->pc, stopped_opcode(cpu),
                     cpu->last_interrupt, cpu->last_interrupt_return,
                     cpu->interrupt_count, cpu->software_reset_count,
                     cpu->reset_interrupt);
            return false;
        }
    }
    return true;
}

struct RunTask {
    Runner* runner;
    Dspic33* cpu;
    const JsonValue* call;
    const JsonValue* stop;
    bool candidate;
    bool succeeded;
    bool execution_failure;
    uint64_t instructions;
    uint64_t cycles;
    char error[256];
#ifdef _WIN32
    bool terminate;
    HANDLE thread;
    HANDLE start_event;
    HANDLE done_event;
#endif
};

static void run_task(RunTask* task) {
    uint64_t instructions = task->cpu->instructions;
    uint64_t cycles = task->cpu->cycles;
    task->succeeded =
        run_image(task->runner, task->cpu, task->candidate, task->call, task->stop,
                  task->error, sizeof(task->error), &task->execution_failure);
    task->instructions = task->cpu->instructions - instructions;
    task->cycles = task->cpu->cycles - cycles;
}

#ifdef _WIN32
static DWORD WINAPI run_task_thread(LPVOID context) {
    RunTask* task = context;
    for (;;) {
        WaitForSingleObject(task->start_event, INFINITE);
        if (task->terminate) {
            return 0u;
        }
        run_task(task);
        SetEvent(task->done_event);
    }
}

static void stop_run_tasks(Runner* runner) {
    size_t index;
    if (runner->run_tasks == NULL) {
        return;
    }
    for (index = 0u; index < 2u; index++) {
        RunTask* task = &runner->run_tasks[index];
        if (task->thread != NULL) {
            task->terminate = true;
            SetEvent(task->start_event);
        }
    }
    for (index = 0u; index < 2u; index++) {
        RunTask* task = &runner->run_tasks[index];
        if (task->thread != NULL) {
            WaitForSingleObject(task->thread, INFINITE);
            CloseHandle(task->thread);
        }
        if (task->start_event != NULL) {
            CloseHandle(task->start_event);
        }
        if (task->done_event != NULL) {
            CloseHandle(task->done_event);
        }
    }
    free(runner->run_tasks);
    runner->run_tasks = NULL;
}

static bool start_run_tasks(Runner* runner) {
    size_t index;
    runner->run_tasks = calloc(2u, sizeof(*runner->run_tasks));
    if (runner->run_tasks == NULL) {
        return false;
    }
    for (index = 0u; index < 2u; index++) {
        RunTask* task = &runner->run_tasks[index];
        task->runner = runner;
        task->cpu = index == 0u ? &runner->reference : &runner->candidate;
        task->candidate = index != 0u;
        task->start_event = CreateEventA(NULL, FALSE, FALSE, NULL);
        task->done_event = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (task->start_event == NULL || task->done_event == NULL) {
            stop_run_tasks(runner);
            return false;
        }
        task->thread = CreateThread(NULL, 0u, run_task_thread, task, 0u, NULL);
        if (task->thread == NULL) {
            stop_run_tasks(runner);
            return false;
        }
    }
    return true;
}
#else
static void stop_run_tasks(Runner* runner) { (void)runner; }

static bool start_run_tasks(Runner* runner) {
    (void)runner;
    return true;
}
#endif

static bool run_pair(Runner* runner, const JsonValue* call, const JsonValue* stop,
                     char* error, size_t error_size, bool* execution_failure) {
#ifdef _WIN32
    RunTask* reference = &runner->run_tasks[0];
    RunTask* candidate = &runner->run_tasks[1];
    HANDLE done_events[2] = {reference->done_event, candidate->done_event};
    reference->call = call;
    reference->stop = stop;
    reference->succeeded = false;
    reference->error[0] = '\0';
    candidate->call = call;
    candidate->stop = stop;
    candidate->succeeded = false;
    candidate->error[0] = '\0';
    SetEvent(reference->start_event);
    SetEvent(candidate->start_event);
    WaitForMultipleObjects(2u, done_events, TRUE, INFINITE);
#else
    RunTask reference_task = {
        .runner = runner, .cpu = &runner->reference, .call = call, .stop = stop};
    RunTask candidate_task = {.runner = runner,
                              .cpu = &runner->candidate,
                              .call = call,
                              .stop = stop,
                              .candidate = true};
    RunTask* reference = &reference_task;
    RunTask* candidate = &candidate_task;
    run_task(reference);
    run_task(candidate);
#endif
    runner->reference_instructions += reference->instructions;
    runner->candidate_instructions += candidate->instructions;
    runner->reference_cycles += reference->cycles;
    runner->candidate_cycles += candidate->cycles;
    *execution_failure = false;
    if (!reference->succeeded) {
        *execution_failure = reference->execution_failure;
        snprintf(error, error_size, "%s", reference->error);
        print_cpu_diagnostics(&runner->reference);
        return false;
    }
    if (!candidate->succeeded) {
        *execution_failure = candidate->execution_failure;
        snprintf(error, error_size, "%s", candidate->error);
        print_cpu_diagnostics(&runner->candidate);
        return false;
    }
    return true;
}

static bool compare_registers(Runner* runner, const StepParts* parts, size_t* failures,
                              char* error, size_t error_size) {
    const JsonValue* values = scalar_field(parts, "registers");
    size_t index;
    if (values == NULL) {
        return true;
    }
    if (values->type != JSON_ARRAY) {
        snprintf(error, error_size, "register observations must be an array");
        return false;
    }
    for (index = 0u; index < values->as.array.count; index++) {
        const JsonValue* item = values->as.array.items[index];
        const char* name = item->type == JSON_STRING
                               ? json_string(item)
                               : json_string(json_get(item, "name"));
        uint8_t reg;
        int64_t mask = UINT16_MAX;
        int64_t expected;
        bool has_expected = item->type == JSON_OBJECT &&
                            parse_number(json_get(item, "expected"), &expected);
        bool matched;
        if (!register_index(name, &reg)) {
            snprintf(error, error_size, "invalid register observation");
            return false;
        }
        if (item->type == JSON_OBJECT && json_get(item, "mask") != NULL &&
            !parse_number(json_get(item, "mask"), &mask)) {
            return false;
        }
        matched =
            (runner->reference.w[reg] & mask) == (runner->candidate.w[reg] & mask);
        if (has_expected) {
            matched = matched &&
                      (runner->reference.w[reg] & mask) == ((uint16_t)expected & mask);
        }
        runner->comparisons++;
        if (!matched) {
            (*failures)++;
            printf("  register %s: reference=0x%04x candidate=0x%04x", name,
                   runner->reference.w[reg], runner->candidate.w[reg]);
            if (has_expected) {
                printf(" expected=0x%04x", (uint16_t)expected);
            }
            printf("\n");
        }
    }
    return true;
}

static bool compare_memory_item(Runner* runner, const JsonValue* item, size_t* failures,
                                char* error, size_t error_size) {
    uint32_t reference_address;
    uint32_t candidate_address;
    int64_t size_value;
    int64_t mask = -1;
    int64_t expected;
    bool has_expected = parse_number(entry_field(runner, item, "expected"), &expected);
    const char* expected_text = json_string(entry_field(runner, item, "expected"));
    const char* space = json_string(entry_field(runner, item, "space"));
    const JsonValue* expected_location = entry_field(runner, item, "expected_location");
    uint32_t reference_expected_address = 0u;
    uint32_t candidate_expected_address = 0u;
    int64_t expected_location_offset = 0;
    const char* name = json_string(entry_field(runner, item, "name"));
    size_t size;
    size_t index;
    size_t first_difference;
    bool matched = true;
    if (!mapped_address(runner, item, false, &reference_address, error, error_size) ||
        !mapped_address(runner, item, true, &candidate_address, error, error_size) ||
        !field_number(runner, item, "size", 0, &size_value) || size_value < 1) {
        snprintf(error, error_size, "invalid memory observation");
        return false;
    }
    size = (size_t)size_value;
    first_difference = size;
    if (entry_field(runner, item, "mask") != NULL &&
        !parse_number(entry_field(runner, item, "mask"), &mask)) {
        return false;
    }
    for (index = 0u; index < size; index++) {
        if (read_memory_byte(&runner->reference, space,
                             reference_address + (uint32_t)index) !=
            read_memory_byte(&runner->candidate, space,
                             candidate_address + (uint32_t)index)) {
            matched = false;
            first_difference = index;
            break;
        }
    }
    if (!has_expected && expected_text != NULL) {
        uint8_t* expected_bytes = malloc(size);
        if (expected_bytes == NULL) {
            snprintf(error, error_size, "cannot allocate expected memory value");
            return false;
        }
        if (!parse_hex_bytes(expected_text, expected_bytes, size)) {
            free(expected_bytes);
            snprintf(error, error_size, "invalid expected memory value");
            return false;
        }
        for (index = 0u; index < size; index++) {
            bool reference_matches =
                read_memory_byte(&runner->reference, space,
                                 reference_address + (uint32_t)index) ==
                expected_bytes[index];
            bool candidate_matches =
                read_memory_byte(&runner->candidate, space,
                                 candidate_address + (uint32_t)index) ==
                expected_bytes[index];
            if ((!reference_matches || !candidate_matches) &&
                first_difference == size) {
                first_difference = index;
            }
            matched = matched && reference_matches && candidate_matches;
        }
        free(expected_bytes);
    }
    if (expected_location != NULL && size <= 4u) {
        uint32_t reference_value = 0u;
        uint32_t candidate_value = 0u;
        if (!mapped_address(runner, expected_location, false,
                            &reference_expected_address, error, error_size) ||
            !mapped_address(runner, expected_location, true,
                            &candidate_expected_address, error, error_size) ||
            (entry_field(runner, item, "expected_location_offset") != NULL &&
             !parse_number(entry_field(runner, item, "expected_location_offset"),
                           &expected_location_offset))) {
            return false;
        }
        reference_expected_address =
            (uint32_t)(reference_expected_address + expected_location_offset);
        candidate_expected_address =
            (uint32_t)(candidate_expected_address + expected_location_offset);
        for (index = 0u; index < size; index++) {
            reference_value |=
                (uint32_t)read_memory_byte(&runner->reference, space,
                                           reference_address + (uint32_t)index)
                << (index * 8u);
            candidate_value |=
                (uint32_t)read_memory_byte(&runner->candidate, space,
                                           candidate_address + (uint32_t)index)
                << (index * 8u);
        }
        matched = (reference_value & (uint32_t)mask) ==
                      (reference_expected_address & (uint32_t)mask) &&
                  (candidate_value & (uint32_t)mask) ==
                      (candidate_expected_address & (uint32_t)mask);
    } else if (has_expected && size <= 4u) {
        uint32_t reference_value = 0u;
        uint32_t candidate_value = 0u;
        for (index = 0u; index < size; index++) {
            reference_value |=
                (uint32_t)read_memory_byte(&runner->reference, space,
                                           reference_address + (uint32_t)index)
                << (index * 8u);
            candidate_value |=
                (uint32_t)read_memory_byte(&runner->candidate, space,
                                           candidate_address + (uint32_t)index)
                << (index * 8u);
        }
        matched =
            matched &&
            (reference_value & (uint32_t)mask) ==
                ((uint32_t)expected & (uint32_t)mask) &&
            (candidate_value & (uint32_t)mask) == ((uint32_t)expected & (uint32_t)mask);
    }
    runner->comparisons++;
    if (!matched) {
        size_t shown = size > 16u ? 16u : size;
        size_t start = first_difference > 4u ? first_difference - 4u : 0u;
        if (start + shown > size) {
            start = size - shown;
        }
        (*failures)++;
        printf("  memory %s", name == NULL ? "state" : name);
        if (first_difference != size) {
            printf(" difference=+0x%zx", first_difference);
        }
        printf(": reference@0x%05" PRIx32 "=", reference_address + (uint32_t)start);
        for (index = 0u; index < shown; index++) {
            printf("%02x",
                   read_memory_byte(&runner->reference, space,
                                    reference_address + (uint32_t)(start + index)));
        }
        printf(" candidate@0x%05" PRIx32 "=", candidate_address + (uint32_t)start);
        for (index = 0u; index < shown; index++) {
            printf("%02x",
                   read_memory_byte(&runner->candidate, space,
                                    candidate_address + (uint32_t)(start + index)));
        }
        if (start + shown != size) {
            printf("...");
        }
        if (has_expected) {
            printf(" expected=0x%0*" PRIx64, (int)(size * 2u), (uint64_t)expected);
        } else if (expected_text != NULL) {
            printf(" expected=%s", expected_text);
        } else if (expected_location != NULL) {
            printf(" expected=reference:0x%08" PRIx32 ",candidate:0x%08" PRIx32,
                   reference_expected_address, candidate_expected_address);
        }
        printf("\n");
    }
    return true;
}

static bool compare_memory(Runner* runner, const StepParts* parts, size_t* failures,
                           char* error, size_t error_size) {
    size_t part_index;
    bool found = false;
    for (part_index = parts->count; part_index != 0u;) {
        const JsonValue* values = json_get(parts->items[--part_index], "memory");
        size_t index;
        if (values == NULL) {
            continue;
        }
        if (found && part_index < 2u) {
            continue;
        }
        found = true;
        if (values->type != JSON_ARRAY) {
            return false;
        }
        for (index = 0u; index < values->as.array.count; index++) {
            if (!compare_memory_item(runner, values->as.array.items[index], failures,
                                     error, error_size)) {
                return false;
            }
        }
    }
    {
        bool exact = false;
        const JsonValue* enabled = scalar_field(parts, "compare_exact_program_data");
        const JsonValue* values = suite_section(runner, "exact_program_data");
        json_boolean(enabled, &exact);
        if (exact && values != NULL && values->type == JSON_ARRAY) {
            size_t index;
            for (index = 0u; index < values->as.array.count; index++) {
                if (!compare_memory_item(runner, values->as.array.items[index],
                                         failures, error, error_size)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool compare_pins(Runner* runner, const StepParts* parts, size_t* failures,
                         char* error, size_t error_size) {
    const JsonValue* values = scalar_field(parts, "pins");
    size_t index;
    if (values == NULL) {
        return true;
    }
    if (values->type != JSON_ARRAY) {
        return false;
    }
    for (index = 0u; index < values->as.array.count; index++) {
        const JsonValue* item = values->as.array.items[index];
        const char* name = item->type == JSON_STRING
                               ? json_string(item)
                               : json_string(json_get(item, "name"));
        const char* expected =
            item->type == JSON_OBJECT ? json_string(json_get(item, "expected")) : NULL;
        uint8_t port;
        uint8_t bit;
        bool reference_high;
        bool candidate_high;
        bool matched;
        if (!pin_index(name, &port, &bit)) {
            snprintf(error, error_size, "invalid pin observation");
            return false;
        }
        reference_high = (runner->reference.io.gpio[port] & (1u << bit)) != 0u;
        candidate_high = (runner->candidate.io.gpio[port] & (1u << bit)) != 0u;
        matched = reference_high == candidate_high;
        if (expected != NULL) {
            bool expected_high = strcmp(expected, "3.3V") == 0;
            matched = matched && reference_high == expected_high;
        }
        runner->comparisons++;
        if (!matched) {
            (*failures)++;
            printf("  pin %s: reference=%s candidate=%s", name,
                   reference_high ? "3.3V" : "0V", candidate_high ? "3.3V" : "0V");
            if (expected != NULL) {
                printf(" expected=%s", expected);
            }
            printf("\n");
        }
    }
    return true;
}

static bool apply_generated_value(Runner* runner, const JsonValue* specification,
                                  int64_t value, bool range, char* error,
                                  size_t error_size) {
    const char* name = json_string(json_get(specification, "register"));
    const JsonValue* location = json_get(specification, "location");
    int64_t base = 0;
    int64_t shift = 0;
    int64_t offset = 0;
    uint16_t word;
    if (!range) {
        if ((json_get(specification, "base") != NULL &&
             !parse_number(json_get(specification, "base"), &base)) ||
            (json_get(specification, "shift") != NULL &&
             !parse_number(json_get(specification, "shift"), &shift))) {
            return false;
        }
    }
    if (shift < 0 || shift > 15) {
        return false;
    }
    word = (uint16_t)(base | (value << shift));
    if (location != NULL) {
        uint32_t reference_address;
        uint32_t candidate_address;
        if ((json_get(specification, "offset") != NULL &&
             !parse_number(json_get(specification, "offset"), &offset)) ||
            !mapped_address(runner, location, false, &reference_address, error,
                            error_size) ||
            !mapped_address(runner, location, true, &candidate_address, error,
                            error_size)) {
            return false;
        }
        dspic33_write_word(&runner->reference, (uint32_t)(reference_address + offset),
                           word);
        dspic33_write_word(&runner->candidate, (uint32_t)(candidate_address + offset),
                           word);
        return true;
    }
    {
        uint8_t reg;
        if (!register_index(name, &reg)) {
            snprintf(error, error_size, "generated value has no destination");
            return false;
        }
        runner->reference.w[reg] = word;
        runner->candidate.w[reg] = word;
    }
    return true;
}

static bool execute_step(Runner* runner, const char* scenario_name,
                         const char* generated_name, const StepParts* parts,
                         const JsonValue* scenario_call,
                         const JsonValue* range_specification, int64_t range_value,
                         const MatrixSelection* matrix_selections,
                         size_t matrix_selection_count, char* error,
                         size_t error_size) {
    const JsonValue* reset = scalar_field(parts, "reset_before");
    const JsonValue* call = scalar_field(parts, "call");
    const JsonValue* stop = scalar_field(parts, "stop");
    const char* step_name = generated_name;
    bool reset_value = false;
    size_t index;
    size_t failures = 0u;
    bool execution_failure;
    if (step_name == NULL) {
        step_name = json_string(scalar_field(parts, "name"));
    }
    if (step_name == NULL) {
        step_name = "unnamed step";
    }
    if (call == NULL) {
        call = scenario_call;
    }
    if (runner->restore_baseline && !restore_baseline(runner, error, error_size)) {
        return false;
    }
    json_boolean(reset, &reset_value);
    if (reset_value) {
        reset_pair(runner);
    }
    runner->current_step++;
    if (!runner->failures_only) {
        printf("[running] %zu/%zu %s: %s\n", runner->current_step, runner->steps,
               scenario_name, step_name);
        fflush(stdout);
    }
    for (index = 0u; index < parts->count; index++) {
        if (!apply_stimuli_part(runner, parts->items[index], error, error_size)) {
            return false;
        }
    }
    if (range_specification != NULL &&
        !apply_generated_value(runner, range_specification, range_value, true, error,
                               error_size)) {
        return false;
    }
    for (index = 0u; index < matrix_selection_count; index++) {
        if (matrix_selections[index].generated &&
            !apply_generated_value(runner, matrix_selections[index].specification,
                                   matrix_selections[index].value, false, error,
                                   error_size)) {
            return false;
        }
    }
    if (!run_pair(runner, call, stop, error, error_size, &execution_failure)) {
        if (!execution_failure) {
            return false;
        }
        runner->failed++;
        printf("[failed] %zu/%zu %s (execution: %s)\n", runner->current_step,
               runner->steps, step_name, error);
        fflush(stdout);
        return true;
    }
    if (!compare_registers(runner, parts, &failures, error, error_size) ||
        !compare_memory(runner, parts, &failures, error, error_size) ||
        !compare_pins(runner, parts, &failures, error, error_size)) {
        return false;
    }
    if (failures == 0u) {
        runner->passed++;
        if (!runner->failures_only) {
            printf("[passed] %zu/%zu %s\n", runner->current_step, runner->steps,
                   step_name);
        }
    } else {
        runner->failed++;
        printf("[failed] %zu/%zu %s (%zu differences)\n", runner->current_step,
               runner->steps, step_name, failures);
    }
    fflush(stdout);
    return true;
}

static bool value_at(const Runner* runner, const JsonValue* specification, size_t index,
                     int64_t* result) {
    const char* set_name = json_string(json_get(specification, "values"));
    const JsonValue* values;
    int64_t start;
    int64_t stride = 1;
    if (set_name != NULL) {
        values = named_entry(suite_section(runner, "value_sets"), set_name);
        return values != NULL && values->type == JSON_ARRAY &&
               index < values->as.array.count &&
               parse_number(values->as.array.items[index], result);
    }
    if (!parse_number(json_get(specification, "start"), &start)) {
        return false;
    }
    if (json_get(specification, "stride") != NULL &&
        !parse_number(json_get(specification, "stride"), &stride)) {
        return false;
    }
    *result = start + (int64_t)index * stride;
    return true;
}

static bool execute_fixture(Runner* runner, const char* fixture_name,
                            const char* scenario_name, unsigned depth, char* error,
                            size_t error_size) {
    const JsonValue* fixture;
    const JsonValue* nested;
    const JsonValue* steps;
    const JsonValue* suite_defaults =
        json_get(suite_section(runner, "testbench"), "defaults");
    size_t index;
    if (depth > 32u) {
        snprintf(error, error_size, "fixture recursion is too deep");
        return false;
    }
    fixture = named_entry(suite_section(runner, "fixtures"), fixture_name);
    if (fixture == NULL) {
        snprintf(error, error_size, "unknown fixture: %s", fixture_name);
        return false;
    }
    if (fixture->type == JSON_ARRAY) {
        steps = fixture;
        nested = NULL;
    } else {
        nested = json_get(fixture, "fixtures");
        steps = json_get(fixture, "steps");
    }
    if (nested != NULL && nested->type == JSON_ARRAY) {
        for (index = 0u; index < nested->as.array.count; index++) {
            const char* name = json_string(nested->as.array.items[index]);
            if (name == NULL || !execute_fixture(runner, name, scenario_name,
                                                 depth + 1u, error, error_size)) {
                return false;
            }
        }
    }
    if (steps == NULL || steps->type != JSON_ARRAY) {
        return true;
    }
    for (index = 0u; index < steps->as.array.count; index++) {
        StepParts parts = {0};
        if (!add_part(&parts, suite_defaults, error, error_size) ||
            !add_part(&parts, steps->as.array.items[index], error, error_size) ||
            !execute_step(runner, scenario_name, NULL, &parts, NULL, NULL, 0, NULL, 0u,
                          error, error_size)) {
            return false;
        }
    }
    return true;
}

static bool add_common_parts(Runner* runner, const JsonValue* scenario,
                             StepParts* parts, char* error, size_t error_size) {
    return add_part(parts, json_get(suite_section(runner, "testbench"), "defaults"),
                    error, error_size) &&
           add_part(parts, json_get(scenario, "defaults"), error, error_size);
}

static bool execute_regular_steps(Runner* runner, const JsonValue* scenario,
                                  const JsonValue* steps, const char* scenario_name,
                                  char* error, size_t error_size) {
    const JsonValue* scenario_call = json_get(scenario, "call");
    size_t index;
    for (index = 0u; index < steps->as.array.count; index++) {
        StepParts parts = {0};
        if (!step_selected(runner, steps->as.array.items[index])) {
            continue;
        }
        if (!add_common_parts(runner, scenario, &parts, error, error_size) ||
            !add_part(&parts, steps->as.array.items[index], error, error_size) ||
            !execute_step(runner, scenario_name, NULL, &parts, scenario_call, NULL, 0,
                          NULL, 0u, error, error_size)) {
            return false;
        }
    }
    return true;
}

static bool execute_range(Runner* runner, const JsonValue* scenario,
                          const JsonValue* range, const char* scenario_name,
                          char* error, size_t error_size) {
    const JsonValue* scenario_call = json_get(scenario, "call");
    const JsonValue* base = json_get(range, "step");
    const char* reg = json_string(json_get(range, "register"));
    size_t count = values_count(runner, range);
    size_t index;
    for (index = 0u; index < count; index++) {
        StepParts parts = {0};
        int64_t value;
        char name[512];
        if (!value_at(runner, range, index, &value)) {
            return false;
        }
        snprintf(name, sizeof(name), "%s at %s=0x%" PRIx64, scenario_name,
                 reg == NULL ? "value" : reg, (uint64_t)value);
        if (!generated_step_selected(runner, name)) {
            continue;
        }
        if (!add_common_parts(runner, scenario, &parts, error, error_size) ||
            !add_part(&parts, base, error, error_size) ||
            !execute_step(runner, scenario_name, name, &parts, scenario_call, range,
                          value, NULL, 0u, error, error_size)) {
            return false;
        }
    }
    return true;
}

static bool select_matrix_case(const Runner* runner, const JsonValue* dimension,
                               size_t index, MatrixSelection* selection) {
    memset(selection, 0, sizeof(*selection));
    selection->specification = dimension;
    if (dimension->type == JSON_ARRAY) {
        if (index >= dimension->as.array.count) {
            return false;
        }
        selection->explicit_case = dimension->as.array.items[index];
        return true;
    }
    selection->generated = true;
    return value_at(runner, dimension, index, &selection->value);
}

static bool execute_matrix_selection(Runner* runner, const JsonValue* scenario,
                                     const JsonValue* matrix,
                                     const MatrixSelection* selections,
                                     size_t selection_count, const char* scenario_name,
                                     size_t ordinal, char* error, size_t error_size) {
    StepParts parts = {0};
    const JsonValue* scenario_call = json_get(scenario, "call");
    char name[512];
    size_t index;
    snprintf(name, sizeof(name), "%s matrix case %zu", scenario_name, ordinal + 1u);
    if (!generated_step_selected(runner, name)) {
        return true;
    }
    if (!add_common_parts(runner, scenario, &parts, error, error_size) ||
        !add_part(&parts, json_get(matrix, "step"), error, error_size)) {
        return false;
    }
    for (index = 0u; index < selection_count; index++) {
        if (!add_part(&parts, selections[index].explicit_case, error, error_size)) {
            return false;
        }
    }
    return execute_step(runner, scenario_name, name, &parts, scenario_call, NULL, 0,
                        selections, selection_count, error, error_size);
}

static bool execute_product_matrix(Runner* runner, const JsonValue* scenario,
                                   const JsonValue* matrix, const JsonValue* dimensions,
                                   const char* scenario_name, char* error,
                                   size_t error_size) {
    size_t counts[MAX_MATRIX_DIMENSIONS];
    size_t indices[MAX_MATRIX_DIMENSIONS] = {0};
    MatrixSelection selections[MAX_MATRIX_DIMENSIONS];
    size_t ordinal = 0u;
    size_t dimension;
    for (dimension = 0u; dimension < dimensions->as.array.count; dimension++) {
        const JsonValue* value = dimensions->as.array.items[dimension];
        counts[dimension] = value->type == JSON_ARRAY ? value->as.array.count
                                                      : values_count(runner, value);
    }
    for (;;) {
        for (dimension = 0u; dimension < dimensions->as.array.count; dimension++) {
            if (!select_matrix_case(runner, dimensions->as.array.items[dimension],
                                    indices[dimension], &selections[dimension])) {
                return false;
            }
        }
        if (!execute_matrix_selection(runner, scenario, matrix, selections,
                                      dimensions->as.array.count, scenario_name,
                                      ordinal++, error, error_size)) {
            return false;
        }
        dimension = dimensions->as.array.count;
        while (dimension != 0u) {
            dimension--;
            if (++indices[dimension] < counts[dimension]) {
                break;
            }
            indices[dimension] = 0u;
        }
        if (dimension == 0u && indices[0] == 0u) {
            return true;
        }
    }
}

static bool execute_pairwise_matrix(Runner* runner, const JsonValue* scenario,
                                    const JsonValue* matrix,
                                    const JsonValue* dimensions,
                                    const char* scenario_name, char* error,
                                    size_t error_size) {
    MatrixSelection selections[MAX_MATRIX_DIMENSIONS];
    size_t counts[MAX_MATRIX_DIMENSIONS];
    size_t ordinal = 0u;
    size_t left;
    size_t right;
    size_t index;
    size_t other;
    for (index = 0u; index < dimensions->as.array.count; index++) {
        const JsonValue* dimension = dimensions->as.array.items[index];
        counts[index] = dimension->type == JSON_ARRAY ? dimension->as.array.count
                                                      : values_count(runner, dimension);
        if (!select_matrix_case(runner, dimension, 0u, &selections[index])) {
            return false;
        }
    }
    if (!execute_matrix_selection(runner, scenario, matrix, selections,
                                  dimensions->as.array.count, scenario_name, ordinal++,
                                  error, error_size)) {
        return false;
    }
    for (index = 0u; index < dimensions->as.array.count; index++) {
        for (other = 1u; other < counts[index]; other++) {
            MatrixSelection changed;
            if (!select_matrix_case(runner, dimensions->as.array.items[index], other,
                                    &changed)) {
                return false;
            }
            selections[index] = changed;
            if (!execute_matrix_selection(runner, scenario, matrix, selections,
                                          dimensions->as.array.count, scenario_name,
                                          ordinal++, error, error_size) ||
                !select_matrix_case(runner, dimensions->as.array.items[index], 0u,
                                    &selections[index])) {
                return false;
            }
        }
    }
    for (left = 0u; left < dimensions->as.array.count; left++) {
        for (right = left + 1u; right < dimensions->as.array.count; right++) {
            size_t left_case;
            size_t right_case;
            for (left_case = 1u; left_case < counts[left]; left_case++) {
                for (right_case = 1u; right_case < counts[right]; right_case++) {
                    if (!select_matrix_case(runner, dimensions->as.array.items[left],
                                            left_case, &selections[left]) ||
                        !select_matrix_case(runner, dimensions->as.array.items[right],
                                            right_case, &selections[right]) ||
                        !execute_matrix_selection(runner, scenario, matrix, selections,
                                                  dimensions->as.array.count,
                                                  scenario_name, ordinal++, error,
                                                  error_size)) {
                        return false;
                    }
                }
            }
            if (!select_matrix_case(runner, dimensions->as.array.items[left], 0u,
                                    &selections[left]) ||
                !select_matrix_case(runner, dimensions->as.array.items[right], 0u,
                                    &selections[right])) {
                return false;
            }
        }
    }
    return true;
}

static bool execute_matrix(Runner* runner, const JsonValue* scenario,
                           const JsonValue* matrix, const char* scenario_name,
                           char* error, size_t error_size) {
    const JsonValue* dimensions = json_get(matrix, "dimensions");
    const char* strategy = json_string(json_get(matrix, "strategy"));
    if (dimensions == NULL || dimensions->type != JSON_ARRAY ||
        dimensions->as.array.count == 0u ||
        dimensions->as.array.count > MAX_MATRIX_DIMENSIONS) {
        return false;
    }
    if (strategy != NULL && strcmp(strategy, "pairwise") == 0) {
        return execute_pairwise_matrix(runner, scenario, matrix, dimensions,
                                       scenario_name, error, error_size);
    }
    return execute_product_matrix(runner, scenario, matrix, dimensions, scenario_name,
                                  error, error_size);
}

static bool execute_scenario(const char* path, const JsonValue* scenario, void* context,
                             char* error, size_t error_size) {
    Runner* runner = context;
    const char* name = json_string(json_get(scenario, "name"));
    const JsonValue* fixtures;
    const JsonValue* mode;
    size_t index;
    (void)path;
    if (!scenario_selected(runner, scenario)) {
        return true;
    }
    if (scenario_step_count(runner, scenario) == 0u) {
        return true;
    }
    runner->current_scenario++;
    runner->restore_baseline = false;
    reset_pair(runner);
    printf("[scenario] %zu/%zu %s\n", runner->current_scenario, runner->scenarios,
           name);
    fflush(stdout);
    fixtures = json_get(scenario, "fixtures");
    if (fixtures != NULL && fixtures->type == JSON_ARRAY) {
        for (index = 0u; index < fixtures->as.array.count; index++) {
            const char* fixture_name = json_string(fixtures->as.array.items[index]);
            if (fixture_name == NULL ||
                !execute_fixture(runner, fixture_name, name, 0u, error, error_size)) {
                return false;
            }
        }
    }
    mode = json_get(scenario, "steps");
    if (mode != NULL) {
        runner->restore_baseline = false;
        return execute_regular_steps(runner, scenario, mode, name, error, error_size);
    }
    if (!save_baseline(runner, error, error_size)) {
        return false;
    }
    runner->restore_baseline = true;
    mode = json_get(scenario, "vectors");
    if (mode != NULL) {
        return execute_regular_steps(runner, scenario, mode, name, error, error_size);
    }
    mode = json_get(scenario, "range");
    if (mode != NULL) {
        return execute_range(runner, scenario, mode, name, error, error_size);
    }
    mode = json_get(scenario, "matrix");
    return mode != NULL &&
           execute_matrix(runner, scenario, mode, name, error, error_size);
}

static void print_usage(const char* program) {
    fprintf(stderr,
            "Usage: %s --suite FILE [--scenario PATTERN] [--step PATTERN] "
            "[--failures-only] [--max-instructions COUNT]\n",
            program);
}

int firmware_runner_main(int argc, char** argv) {
    Runner runner = {0};
    const char* suite_path = NULL;
    char error[256];
    int index;
    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--suite") == 0 && index + 1 < argc) {
            suite_path = argv[++index];
        } else if (strcmp(argv[index], "--scenario") == 0 && index + 1 < argc) {
            runner.scenario_filter = argv[++index];
        } else if (strcmp(argv[index], "--step") == 0 && index + 1 < argc) {
            runner.step_filter = argv[++index];
        } else if (strcmp(argv[index], "--failures-only") == 0) {
            runner.failures_only = true;
        } else if (strcmp(argv[index], "--max-instructions") == 0 && index + 1 < argc) {
            char* end;
            runner.instruction_limit = strtoull(argv[++index], &end, 0);
            if (*end != '\0' || runner.instruction_limit == 0u) {
                print_usage(argv[0]);
                return 2;
            }
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }
    if (suite_path == NULL || !suite_directory(suite_path, runner.suite_directory,
                                               sizeof(runner.suite_directory))) {
        print_usage(argv[0]);
        return 2;
    }
    runner.suite_path = suite_path;
    printf("[prepare] Loading firmware test specification\n");
    fflush(stdout);
    runner.suite = json_read(suite_path, error, sizeof(error));
    if (runner.suite == NULL) {
        fprintf(stderr, "[error] %s\n", error);
        return 1;
    }
    printf("[prepare] Counting streamed scenarios\n");
    fflush(stdout);
    if (!stream_patterns(&runner, count_scenario, error, sizeof(error)) ||
        runner.scenarios == 0u) {
        if (runner.scenarios == 0u) {
            snprintf(error, sizeof(error),
                     "no scenarios matched the requested filters");
        }
        fprintf(stderr, "[error] %s\n", error);
        json_free((JsonValue*)runner.suite);
        return 1;
    }
    printf("[prepare] Loaded %zu scenarios with %zu test steps\n", runner.scenarios,
           runner.steps);
    fflush(stdout);
    if (!open_images(&runner, error, sizeof(error))) {
        fprintf(stderr, "[error] %s\n", error);
        close_images(&runner);
        json_free((JsonValue*)runner.suite);
        return 1;
    }
    if (!start_run_tasks(&runner)) {
        fprintf(stderr, "[error] cannot start native simulator workers\n");
        close_images(&runner);
        json_free((JsonValue*)runner.suite);
        return 1;
    }
    printf("[ready] Starting native differential execution\n");
    fflush(stdout);
    if (!stream_patterns(&runner, execute_scenario, error, sizeof(error))) {
        fprintf(stderr, "[error] %s\n", error);
        stop_run_tasks(&runner);
        close_images(&runner);
        json_free((JsonValue*)runner.suite);
        return 1;
    }
    printf("[summary] scenarios=%zu steps=%zu passed=%zu failed=%zu comparisons=%zu\n",
           runner.current_scenario, runner.current_step, runner.passed, runner.failed,
           runner.comparisons);
    printf("[work] reference-instructions=%" PRIu64 " candidate-instructions=%" PRIu64
           " reference-cycles=%" PRIu64 " candidate-cycles=%" PRIu64 "\n",
           runner.reference_instructions, runner.candidate_instructions,
           runner.reference_cycles, runner.candidate_cycles);
    stop_run_tasks(&runner);
    close_images(&runner);
    json_free((JsonValue*)runner.suite);
    return runner.failed == 0u ? 0 : 1;
}
