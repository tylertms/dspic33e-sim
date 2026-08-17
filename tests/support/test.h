#ifndef DSPIC33_TEST_H
#define DSPIC33_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} TestState;

static void expect(TestState* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
        return;
    }
    state->failed++;
    printf("[failed] %s\n", name);
}

#endif
