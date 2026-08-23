#include "allocation_failure.h"

#include <stddef.h>

static bool reject_reallocation;

void* __real_realloc(void* pointer, size_t size);

void* __wrap_realloc(void* pointer, size_t size) {
    return reject_reallocation ? NULL : __real_realloc(pointer, size);
}

void test_reject_reallocation(bool reject) { reject_reallocation = reject; }
