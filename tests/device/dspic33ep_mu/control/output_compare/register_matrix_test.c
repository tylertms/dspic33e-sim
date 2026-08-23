#include "device/dspic33ep_mu/internal.h"
#include "test.h"

typedef struct {
    uint64_t fingerprint;
    uint32_t cases;
} MatrixResult;

static uint64_t mix(uint64_t fingerprint, uint32_t value) {
    return (fingerprint ^ value) * UINT64_C(1099511628211);
}

static uint16_t changed_value(uint16_t offset, uint16_t control1, uint16_t control2,
                              uint8_t variant) {
    if (offset == 0u) {
        static const uint16_t additions[] = {0u, OUTPUT_COMPARE_STOP_IDLE,
                                             OUTPUT_COMPARE_FAULT_ENABLE_MASK,
                                             OUTPUT_COMPARE_FAULT_STATUS_MASK};
        return (uint16_t)(control1 | additions[variant]);
    }
    if (offset == 2u) {
        static const uint16_t additions[] = {0u, OUTPUT_COMPARE_TRIGGER,
                                             OUTPUT_COMPARE_TRIGGER_STATUS, OUTPUT_COMPARE_INVERT};
        return (uint16_t)(control2 | additions[variant]);
    }
    return (uint16_t)(variant == 0u ? 0u : variant == 1u ? 1u : variant == 2u ? 5u : UINT16_MAX);
}

static void configure_pair(Dspic33* cpu, uint8_t channel, uint16_t mode, bool cascade) {
    const uint8_t low = dspic33_device_internal_output_compare_pair_low(channel);
    const uint8_t high = dspic33_device_internal_output_compare_pair_high(channel);
    const uint16_t low_base = dspic33_device_internal_output_compare_base(low);
    const uint16_t high_base = dspic33_device_internal_output_compare_base(high);
    const uint16_t control1 = (uint16_t)(OUTPUT_COMPARE_TIMER_SOURCE_FP | mode);
    dspic33_device_internal_raw_write_word(cpu, low_base, control1);
    dspic33_device_internal_raw_write_word(cpu, high_base, control1);
    dspic33_device_internal_raw_write_word(
        cpu, (uint16_t)(low_base + 2u),
        cascade ? (uint16_t)(OUTPUT_COMPARE_32_BIT | OUTPUT_COMPARE_TRISTATE)
                : OUTPUT_COMPARE_SYNC_SELF);
    dspic33_device_internal_raw_write_word(cpu, (uint16_t)(high_base + 2u),
                                           cascade ? OUTPUT_COMPARE_32_BIT
                                                   : OUTPUT_COMPARE_SYNC_SELF);
}

static MatrixResult run_matrix(Dspic33* cpu) {
    static const uint16_t offsets[] = {0u, 2u, 4u, 6u};
    MatrixResult result = {UINT64_C(14695981039346656037), 0u};
    for (uint8_t channel = 0u; channel < DSPIC33_OUTPUT_COMPARE_COUNT; channel++) {
        for (uint16_t mode = 1u; mode <= 7u; mode++) {
            for (uint8_t cascade = 0u; cascade < 2u; cascade++) {
                for (uint8_t offset_index = 0u; offset_index < 4u; offset_index++) {
                    for (uint8_t variant = 0u; variant < 4u; variant++) {
                        for (uint8_t write_case = 0u; write_case < 3u; write_case++) {
                            const uint16_t offset = offsets[offset_index];
                            const uint16_t base =
                                dspic33_device_internal_output_compare_base(channel);
                            dspic33_reset(cpu, 0u);
                            configure_pair(cpu, channel, mode, cascade != 0u);
                            const uint16_t control1 = dspic33_device_internal_raw_word(cpu, base);
                            const uint16_t control2 =
                                dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u));
                            const uint16_t address = (uint16_t)(base + offset);
                            const uint16_t current =
                                changed_value(offset, control1, control2, variant);
                            const uint16_t previous =
                                (uint16_t)(current ^ (variant == 0u ? 0u : 1u << variant));
                            dspic33_device_internal_raw_write_word(cpu, address, current);
                            cpu->io.cpu_write_valid = write_case != 0u;
                            cpu->io.cpu_write_address =
                                write_case == 1u ? address : (uint16_t)(address + 1u);
                            cpu->io.cpu_write_width = write_case == 1u ? 2u : 1u;
                            dspic33_device_internal_update_output_compare_register(cpu, address,
                                                                                   previous);
                            result.fingerprint = mix(result.fingerprint, cpu->stop_reason);
                            result.fingerprint =
                                mix(result.fingerprint, (uint32_t)cpu->events.count);
                            result.fingerprint =
                                mix(result.fingerprint, cpu->io.output_compare.generation[channel]);
                            result.fingerprint =
                                mix(result.fingerprint,
                                    cpu->io.output_compare.timer_generation
                                        [dspic33_device_internal_output_compare_pair_low(channel)]);
                            result.fingerprint = mix(
                                result.fingerprint, dspic33_device_internal_raw_word(cpu, address));
                            result.cases++;
                        }
                    }
                }
            }
        }
    }
    return result;
}

int main(void) {
    TestState state = {0};
    Dspic33 cpu;
    const bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "cpu initialized");
    if (initialized) {
        const MatrixResult result = run_matrix(&cpu);
        expect(&state,
               result.cases == 10752u && result.fingerprint == UINT64_C(18300558673284928389),
               "output compare register matrix matches");
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
