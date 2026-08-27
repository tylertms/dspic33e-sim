#include <stdlib.h>
#include <string.h>

#include "dspic33.h"

struct Dspic33Coverage {
    uint32_t address;
    size_t size;
    size_t slot_count;
    uint64_t instructions;
    uint64_t outside_range;
    size_t unique_instructions;
    uint8_t slots[];
};

static uint8_t* coverage_slot(Dspic33Coverage* coverage, uint32_t address) {
    if (coverage == NULL || (address & 1u) != 0u || address < coverage->address ||
        (size_t)(address - coverage->address) >= coverage->size) {
        return NULL;
    }
    return &coverage->slots[(address - coverage->address) / 2u];
}

static const uint8_t* const_coverage_slot(const Dspic33Coverage* coverage, uint32_t address) {
    if (coverage == NULL || (address & 1u) != 0u || address < coverage->address ||
        (size_t)(address - coverage->address) >= coverage->size) {
        return NULL;
    }
    return &coverage->slots[(address - coverage->address) / 2u];
}

Dspic33Coverage* dspic33_coverage_create(uint32_t address, size_t size) {
    if ((address & 1u) != 0u || size == 0u || (size & 1u) != 0u || size > UINT32_MAX ||
        (uint64_t)address + size > UINT64_C(0x100000000)) {
        return NULL;
    }
    const size_t slot_count = size / 2u;
    if (slot_count > SIZE_MAX - sizeof(Dspic33Coverage)) {
        return NULL;
    }
    Dspic33Coverage* coverage = calloc(1, sizeof(*coverage) + slot_count);
    if (coverage != NULL) {
        coverage->address = address;
        coverage->size = size;
        coverage->slot_count = slot_count;
    }
    return coverage;
}

void dspic33_coverage_destroy(Dspic33Coverage* coverage) { free(coverage); }

void dspic33_coverage_clear(Dspic33Coverage* coverage) {
    if (coverage == NULL) {
        return;
    }
    const uint32_t address = coverage->address;
    const size_t size = coverage->size;
    const size_t slot_count = coverage->slot_count;
    memset(coverage, 0, sizeof(*coverage) + slot_count);
    coverage->address = address;
    coverage->size = size;
    coverage->slot_count = slot_count;
}

void dspic33_coverage_record(void* context, uint32_t address, uint32_t opcode) {
    Dspic33Coverage* coverage = context;
    if (coverage == NULL) {
        return;
    }
    uint8_t* slot = coverage_slot(coverage, address);
    if (slot == NULL) {
        coverage->outside_range++;
    } else if (*slot == 0u) {
        *slot = 1u;
        coverage->unique_instructions++;
    }
    coverage->instructions++;
    (void)opcode;
}

Dspic33CoverageResult dspic33_coverage_result(const Dspic33Coverage* coverage) {
    if (coverage == NULL) {
        return (Dspic33CoverageResult){0};
    }
    return (Dspic33CoverageResult){
        coverage->instructions,
        coverage->outside_range,
        coverage->unique_instructions,
    };
}

bool dspic33_coverage_executed(const Dspic33Coverage* coverage, uint32_t address) {
    const uint8_t* slot = const_coverage_slot(coverage, address);
    return slot != NULL && *slot != 0u;
}
