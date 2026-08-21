#ifndef DSPIC33_TEST_H
#define DSPIC33_TEST_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33_internal.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} TestState;

static inline void expect(TestState* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
        return;
    }
    state->failed++;
    printf("[failed] %s\n", name);
}

static inline int test_finish(const TestState* state) {
    printf("[summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state->cases, state->passed, state->failed);
    return state->failed == 0u ? 0 : 1;
}

#endif
