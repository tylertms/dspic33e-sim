#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33.h"
#include "test.h"

enum {
    SFR_LIMIT = 0x1000u,
};

static uint64_t snapshot_hash(Dspic33* cpu) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint16_t address = 0u; address < SFR_LIMIT; address += 2u) {
        uint16_t value = dspic33_read_word(cpu, address);
        hash ^= value & 0xffu;
        hash *= UINT64_C(1099511628211);
        hash ^= value >> 8u;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    dspic33_reset(&cpu, 0u);
    expect(&state, snapshot_hash(&cpu) == UINT64_C(0x083f22f9aefd2c8f), "power-on SFR snapshot");
    for (uint16_t address = 0u; address < SFR_LIMIT; address += 2u) {
        cpu.data[address] = 0xffu;
        cpu.data[address + 1u] = 0xffu;
    }
    dspic33_mclr_reset(&cpu);
    expect(&state, snapshot_hash(&cpu) == UINT64_C(0x2106aea681db4041),
           "master-clear SFR snapshot");
    printf("[sfr-reset] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n", state.cases,
           state.passed, state.failed);
    dspic33_release(&cpu);
    return test_finish(&state);
}
