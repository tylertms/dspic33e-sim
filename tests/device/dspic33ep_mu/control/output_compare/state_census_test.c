#include <stdint.h>

#include "architecture/dspic33/internal.h"
#include "device/dspic33ep_mu/internal.h"
#include "test.h"

typedef struct {
    uint64_t fingerprint;
    uint32_t finite;
} Census;

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static void configure_channel(Dspic33* cpu, uint8_t channel, uint16_t mode, bool cascade,
                              uint16_t timer, uint16_t r, uint16_t rs, uint8_t phase,
                              uint8_t variant) {
    const uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    const uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    const uint16_t low_base = dspic33_device_internal_output_compare_base(low);
    const uint16_t high_base = dspic33_device_internal_output_compare_base(high);
    const uint16_t control1 = (uint16_t)(OUTPUT_COMPARE_TIMER_SOURCE_FP | mode);
    const uint16_t low_control2 =
        cascade ? (uint16_t)(OUTPUT_COMPARE_32_BIT | OUTPUT_COMPARE_TRISTATE) : 0u;
    const uint16_t high_control2 = cascade ? OUTPUT_COMPARE_32_BIT : 0u;
    dspic33_device_internal_raw_write_word(cpu, low_base, control1);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(low_base + 2u), low_control2);
    dspic33_device_internal_raw_write_word(cpu, high_base, control1);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(high_base + 2u), high_control2);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(low_base + 8u), timer);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(high_base + 8u),
                                           (uint16_t)(timer + variant));
    cpu->io.output_compare.active_r[low] = r;
    cpu->io.output_compare.active_rs[low] = rs;
    cpu->io.output_compare.active_r[high] = (uint16_t)(r + variant);
    cpu->io.output_compare.active_rs[high] = (uint16_t)(rs + variant);
    cpu->io.output_compare.phase[low] = phase;
    cpu->io.output_compare.phase[high] = phase;
    cpu->io.output_compare.sync_emitted[low] = (variant & 1u) != 0u;
    cpu->io.output_compare.sync_reset_pending = variant == 3u ? (uint16_t)(1u << low) : 0u;
    if (variant == 2u) {
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(low_base + 2u),
                                               (uint16_t)(low_control2 | OUTPUT_COMPARE_TRIGGER));
    }
}

static Census census_states(Dspic33* cpu) {
    static const uint16_t values[] = {0u, 1u, 5u, UINT16_MAX};
    Census census = {UINT64_C(14695981039346656037), 0u};
    dspic33_reset(cpu, 0u);
    for (uint16_t mode = 1u; mode <= 7u; mode++) {
        for (uint8_t cascade = 0u; cascade < 2u; cascade++) {
            for (uint8_t phase = 0u; phase < 3u; phase++) {
                for (uint8_t timer = 0u; timer < 4u; timer++) {
                    for (uint8_t r = 0u; r < 4u; r++) {
                        for (uint8_t rs = 0u; rs < 4u; rs++) {
                            const uint8_t variant = (uint8_t)((timer + r + rs + phase) & 3u);
                            uint32_t kind = 0u;
                            configure_channel(cpu, 0u, mode, cascade != 0u, values[timer],
                                              values[r], values[rs], phase, variant);
                            const uint64_t delay =
                                dspic33_device_internal_output_compare_next_timer_event(cpu, 0u,
                                                                                        &kind);
                            census.finite += delay != UINT64_MAX;
                            census.fingerprint = mix(census.fingerprint, kind);
                            census.fingerprint = mix(census.fingerprint, (uint32_t)delay);
                            census.fingerprint = mix(census.fingerprint, (uint32_t)(delay >> 32u));
                        }
                    }
                }
            }
        }
    }
    return census;
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        const Census census = census_states(&cpu);
        expect(&state,
               census.finite == 2016u && census.fingerprint == UINT64_C(7503594019394709523),
               "output compare state census matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
