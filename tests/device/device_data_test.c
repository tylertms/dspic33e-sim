#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33ep512mu810_data.h"
#include "test.h"

static uint32_t count_bits(uint8_t value) {
    uint32_t count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

static void test_implementation_map(TestState* state) {
    uint32_t implemented = 0u;
    uint32_t absent_ranges = 0u;
    bool previous_implemented = true;
    for (uint32_t index = 0u; index < DSPIC33_SFR_IMPLEMENTATION_BITMAP_SIZE; index++) {
        implemented += count_bits(dspic33_sfr_implementation_bitmap[index]);
    }
    for (uint32_t slot = 0u; slot < DSPIC33_SFR_WORD_COUNT; slot++) {
        bool expected = (dspic33_sfr_implementation_bitmap[slot >> 3u] &
                         (uint8_t)(1u << (slot & 7u))) != 0u;
        bool actual = dspic33ep512mu810_address_implemented(slot << 1u);
        if (!actual && previous_implemented) {
            absent_ranges++;
        }
        previous_implemented = actual;
        expect(state, actual == expected, "implementation map lookup");
    }
    expect(state, implemented == DSPIC33_SFR_IMPLEMENTED_WORD_COUNT,
           "implemented SFR word count");
    expect(state, DSPIC33_SFR_WORD_COUNT - implemented == DSPIC33_SFR_ABSENT_WORD_COUNT,
           "absent SFR word count");
    expect(state, absent_ranges == DSPIC33_SFR_ABSENT_RANGE_COUNT,
           "absent SFR range count");
    expect(state, dspic33ep512mu810_address_implemented(0x1000u),
           "data memory after SFR range is implemented");
}

static void test_master_clear_resets(TestState* state) {
    uint16_t previous = 0u;
    for (uint32_t index = 0u; index < DSPIC33_SFR_MASTER_CLEAR_RESET_COUNT; index++) {
        const Dspic33SfrMasterClearReset* reset =
            &dspic33_sfr_master_clear_resets[index];
        expect(state, (reset->address & 1u) == 0u,
               "master-clear reset address alignment");
        expect(state, index == 0u || reset->address > previous,
               "master-clear reset address ordering");
        expect(state, dspic33ep512mu810_address_implemented(reset->address),
               "master-clear reset address implementation");
        expect(state, (reset->known_mask & reset->unchanged) == 0u,
               "master-clear reset mask partition");
        expect(state, (reset->value & ~reset->known_mask) == 0u,
               "master-clear reset value mask");
        previous = reset->address;
    }
}

int main(void) {
    TestState state = {0};
    test_implementation_map(&state);
    test_master_clear_resets(&state);
    printf("[device-data] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
