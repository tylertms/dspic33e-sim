#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/data.h"
#include "test.h"

typedef struct {
    Dspic33epMuDevice device;
    const char* name;
    uint32_t program_limit;
    uint32_t data_limit;
    uint32_t device_id;
    uint16_t pin_count;
    uint16_t io_pin_count;
    uint8_t pwm_generator_count;
    uint8_t adc_channel_count;
    uint32_t implemented_words;
    uint32_t absent_ranges;
    size_t reset_count;
    uint64_t implementation_hash;
    uint64_t reset_hash;
} ExpectedProfile;

static const ExpectedProfile expected_profiles[] = {
    {DSPIC33EP_MU_DEVICE_256MU806, "dsPIC33EP256MU806", 0x2ac00u, 0x8000u, 0x18614000u, 64u, 51u,
     4u, 24u, 935u, 77u, 48u, UINT64_C(0xe5286ef2ae44471e), UINT64_C(0xd305ae808264e876)},
    {DSPIC33EP_MU_DEVICE_256MU810, "dsPIC33EP256MU810", 0x2ac00u, 0x8000u, 0x18624000u, 100u, 83u,
     6u, 32u, 977u, 77u, 49u, UINT64_C(0xa0bac28c616ad517), UINT64_C(0xbf0a068cbb5ee5a6)},
    {DSPIC33EP_MU_DEVICE_256MU814, "dsPIC33EP256MU814", 0x2ac00u, 0x8000u, 0x18634000u, 144u, 122u,
     7u, 32u, 1014u, 80u, 52u, UINT64_C(0xbc4bc14d6f2cea50), UINT64_C(0xcf445290ea30727b)},
    {DSPIC33EP_MU_DEVICE_512MU810, "dsPIC33EP512MU810", 0x55800u, 0xe000u, 0x18724000u, 100u, 83u,
     6u, 32u, 977u, 77u, 49u, UINT64_C(0xa0bac28c616ad517), UINT64_C(0xbf0a068cbb5ee5a6)},
    {DSPIC33EP_MU_DEVICE_512MU814, "dsPIC33EP512MU814", 0x55800u, 0xe000u, 0x18734000u, 144u, 122u,
     7u, 32u, 1014u, 80u, 52u, UINT64_C(0xbc4bc14d6f2cea50), UINT64_C(0xcf445290ea30727b)},
};

static uint32_t count_bits(uint8_t value) {
    uint32_t count = 0u;
    while (value != 0u) {
        count += value & 1u;
        value >>= 1u;
    }
    return count;
}

static uint64_t hash_byte(uint64_t hash, uint8_t value) {
    return (hash ^ value) * UINT64_C(1099511628211);
}

static uint64_t hash_word(uint64_t hash, uint16_t value) {
    return hash_byte(hash_byte(hash, (uint8_t)value), (uint8_t)(value >> 8u));
}

static void test_profile(TestState* state, const ExpectedProfile* expected) {
    const uint8_t data = 0x5au;
    const Dspic33epMuProfile* profile = dspic33ep_mu_profile(expected->device);
    const uint8_t* bitmap = dspic33ep_mu_implementation_bitmap(expected->device);
    size_t reset_count = 0u;
    const Dspic33SfrMasterClearReset* resets =
        dspic33ep_mu_master_clear_resets(expected->device, &reset_count);
    expect(state, profile != NULL, "profile != NULL");
    expect(state, bitmap != NULL, "bitmap != NULL");
    expect(state, resets != NULL, "resets != NULL");
    expect(state, strcmp(profile->name, expected->name) == 0, "profile name");
    expect(state, profile->program_limit == expected->program_limit, "profile program limit");
    expect(state, profile->data_limit == expected->data_limit, "profile data limit");
    expect(state, profile->device_id == expected->device_id, "profile device ID");
    expect(state, profile->pin_count == expected->pin_count, "profile package pins");
    expect(state, profile->io_pin_count == expected->io_pin_count, "profile I/O pins");
    expect(state, profile->pwm_generator_count == expected->pwm_generator_count,
           "profile PWM generators");
    expect(state, profile->adc_channel_count == expected->adc_channel_count,
           "profile ADC channels");

    Dspic33epMuDevice parsed = DSPIC33EP_MU_DEVICE_COUNT;
    expect(state, dspic33ep_mu_device_from_name(expected->name, &parsed), "profile name lookup");
    expect(state, parsed == expected->device, "profile name lookup result");

    uint32_t implemented = 0u;
    uint32_t absent_ranges = 0u;
    bool previous_implemented = true;
    uint64_t implementation_hash = UINT64_C(14695981039346656037);
    for (uint32_t index = 0u; index < DSPIC33_SFR_IMPLEMENTATION_BITMAP_SIZE; index++) {
        implemented += count_bits(bitmap[index]);
        implementation_hash = hash_byte(implementation_hash, bitmap[index]);
    }
    for (uint32_t slot = 0u; slot < DSPIC33_SFR_WORD_COUNT; slot++) {
        bool from_bitmap = (bitmap[slot >> 3u] & (uint8_t)(1u << (slot & 7u))) != 0u;
        bool implemented_address = dspic33ep_mu_address_implemented(expected->device, slot << 1u);
        if (!implemented_address && previous_implemented) {
            absent_ranges++;
        }
        previous_implemented = implemented_address;
        expect(state, implemented_address == from_bitmap, "implementation map lookup");
    }
    expect(state, implemented == expected->implemented_words, "implemented SFR word count");
    expect(state, absent_ranges == expected->absent_ranges, "absent SFR range count");
    expect(state, implementation_hash == expected->implementation_hash,
           "SFR implementation data hash");
    expect(state, dspic33ep_mu_address_implemented(expected->device, 0x1000u),
           "data RAM start implemented");
    expect(state, !dspic33ep_mu_address_implemented(expected->device, expected->data_limit),
           "data RAM limit rejected");

    uint64_t reset_hash = UINT64_C(14695981039346656037);
    uint16_t previous_address = 0u;
    expect(state, reset_count == expected->reset_count, "master-clear reset count");
    for (size_t index = 0u; index < reset_count; index++) {
        const Dspic33SfrMasterClearReset* reset = &resets[index];
        expect(state, (reset->address & 1u) == 0u, "master-clear reset alignment");
        expect(state, index == 0u || reset->address > previous_address,
               "master-clear reset ordering");
        expect(state, dspic33ep_mu_address_implemented(expected->device, reset->address),
               "master-clear reset address implementation");
        expect(state, (reset->known_mask & reset->unchanged) == 0u,
               "master-clear reset mask partition");
        expect(state, (reset->value & ~reset->known_mask) == 0u, "master-clear reset value mask");
        reset_hash = hash_word(reset_hash, reset->address);
        reset_hash = hash_word(reset_hash, reset->known_mask);
        reset_hash = hash_word(reset_hash, reset->value);
        reset_hash = hash_word(reset_hash, reset->unchanged);
        previous_address = reset->address;
    }
    expect(state, reset_hash == expected->reset_hash, "master-clear reset data hash");

    Dspic33* cpu = dspic33_create_for_device(expected->device);
    expect(state, cpu != NULL, "create profile device");
    expect(state, dspic33_device_profile(cpu) == profile, "created device profile");
    expect(state, dspic33_device_program_range_implemented(cpu, 0u, 2u),
           "device program start implemented");
    expect(state, dspic33_load_program_word(cpu, expected->program_limit - 2u, 0x005aa5u),
           "device program end load accepted");
    expect(state, !dspic33_load_program_word(cpu, expected->program_limit, 0x005aa5u),
           "device program limit load rejected");
    expect(state, !dspic33_device_program_range_implemented(cpu, expected->program_limit, 2u),
           "device program limit rejected");
    expect(state, dspic33_device_data_range_implemented(cpu, expected->data_limit - 1u, 1u),
           "device data end accepted");
    expect(state, !dspic33_device_data_range_implemented(cpu, expected->data_limit, 1u),
           "device data limit rejected");
    expect(state, dspic33_seed_data(cpu, expected->data_limit - 1u, &data, 1u),
           "device data end seed accepted");
    expect(state, dspic33_seed_data(cpu, expected->data_limit, &data, 1u),
           "architectural data backing seed accepted");
    dspic33_destroy(cpu);
}

int main(void) {
    TestState state = {0};
    for (size_t index = 0u; index < sizeof(expected_profiles) / sizeof(expected_profiles[0]);
         index++) {
        test_profile(&state, &expected_profiles[index]);
    }
    expect(&state, dspic33ep_mu_profile(DSPIC33EP_MU_DEVICE_COUNT) == NULL,
           "invalid profile rejected");
    expect(&state, dspic33ep_mu_implementation_bitmap(DSPIC33EP_MU_DEVICE_COUNT) == NULL,
           "invalid bitmap rejected");
    expect(&state, !dspic33ep_mu_device_from_name("dsPIC33EP512MU806", NULL),
           "invalid profile lookup rejected");
    Dspic33epMuDevice device = DSPIC33EP_MU_DEVICE_COUNT;
    expect(&state, !dspic33ep_mu_device_from_name("unknown", &device),
           "unknown profile name rejected");
    expect(&state, !dspic33ep_mu_address_implemented(DSPIC33EP_MU_DEVICE_COUNT, 0u),
           "invalid device address rejected");
    printf("[device-data] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n", state.cases,
           state.passed, state.failed);
    return test_finish(&state);
}
