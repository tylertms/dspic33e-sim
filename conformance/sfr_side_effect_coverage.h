#ifndef SFR_SIDE_EFFECT_COVERAGE_H
#define SFR_SIDE_EFFECT_COVERAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint16_t address;
    uint16_t mask;
} SfrSideEffectCoverage;

static void report_sfr_side_effect_coverage(const char* component,
                                            const SfrSideEffectCoverage* coverage,
                                            size_t count, bool conformance_passed) {
    size_t index;
    if (!conformance_passed) {
        return;
    }
    for (index = 0u; index < count; index++) {
        printf("[sfr-side-effect-coverage] component=%s address=0x%04x mask=0x%04x\n",
               component, (unsigned)coverage[index].address,
               (unsigned)coverage[index].mask);
    }
}

#define SFR_SIDE_EFFECT_COVERAGE_COUNT(coverage)                                       \
    (sizeof(coverage) / sizeof((coverage)[0]))

#endif
