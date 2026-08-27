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
    uint16_t y_data_base;
    uint16_t dma_ram_base;
    uint32_t device_id;
    uint16_t pin_count;
    uint16_t io_pin_count;
    uint8_t pwm_generator_count;
    uint32_t adc_channel_mask;
    const uint16_t* gpio_masks;
    uint32_t implemented_words;
    uint32_t absent_ranges;
    size_t reset_count;
    uint64_t implementation_hash;
    uint64_t reset_hash;
} ExpectedProfile;

static const uint16_t gpio_masks_806[DSPIC33_GPIO_PORT_COUNT] = {
    0u, 0xffffu, 0xf000u, 0x0fffu, 0x00ffu, 0x003bu, 0x03ccu, 0u, 0u, 0u};
static const uint16_t gpio_masks_810[DSPIC33_GPIO_PORT_COUNT] = {
    0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x313fu, 0xf3cfu, 0u, 0u, 0u};
static const uint16_t gpio_masks_814[DSPIC33_GPIO_PORT_COUNT] = {
    0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x313fu, 0xf3cfu, 0xffffu, 0xffffu, 0xf803u};

static const ExpectedProfile expected_profiles[] = {
    {DSPIC33EP_MU_DEVICE_256MU806, "dsPIC33EP256MU806", 0x2ac00u, 0x8000u, 0x5000u, 0x7000u,
     0x18614000u, 64u, 51u, 4u, 0xff00ffffu, gpio_masks_806, 935u, 77u, 48u,
     UINT64_C(0xe5286ef2ae44471e), UINT64_C(0xd305ae808264e876)},
    {DSPIC33EP_MU_DEVICE_256MU810, "dsPIC33EP256MU810", 0x2ac00u, 0x8000u, 0x5000u, 0x7000u,
     0x18624000u, 100u, 83u, 6u, UINT32_MAX, gpio_masks_810, 977u, 77u, 49u,
     UINT64_C(0xa0bac28c616ad517), UINT64_C(0xbf0a068cbb5ee5a6)},
    {DSPIC33EP_MU_DEVICE_256MU814, "dsPIC33EP256MU814", 0x2ac00u, 0x8000u, 0x5000u, 0x7000u,
     0x18634000u, 144u, 122u, 7u, UINT32_MAX, gpio_masks_814, 1014u, 80u, 52u,
     UINT64_C(0xbc4bc14d6f2cea50), UINT64_C(0xcf445290ea30727b)},
    {DSPIC33EP_MU_DEVICE_512MU810, "dsPIC33EP512MU810", 0x55800u, 0xe000u, 0x9000u, 0xd000u,
     0x18724000u, 100u, 83u, 6u, UINT32_MAX, gpio_masks_810, 977u, 77u, 49u,
     UINT64_C(0xa0bac28c616ad517), UINT64_C(0xbf0a068cbb5ee5a6)},
    {DSPIC33EP_MU_DEVICE_512MU814, "dsPIC33EP512MU814", 0x55800u, 0xe000u, 0x9000u, 0xd000u,
     0x18734000u, 144u, 122u, 7u, UINT32_MAX, gpio_masks_814, 1014u, 80u, 52u,
     UINT64_C(0xbc4bc14d6f2cea50), UINT64_C(0xcf445290ea30727b)},
};

static uint32_t count_bits(uint16_t value) {
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

static void test_gpio_sfr_profile(TestState* state, Dspic33* cpu, const ExpectedProfile* expected) {
    static const uint8_t offsets[] = {0u, 4u, 6u, 8u, 10u, 12u, 14u};
    static const uint16_t register_masks[][DSPIC33_GPIO_PORT_COUNT] = {
        {0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x317fu, 0xf3c3u, 0xffffu, 0xffffu, 0xf803u},
        {0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x317fu, 0xf3c3u, 0xffffu, 0xffffu, 0xf803u},
        {0xc03fu, 0u, 0u, 0xff3fu, 0u, 0x317fu, 0xf003u, 0xffffu, 0xffffu, 0xf803u},
        {0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x317fu, 0xf3cfu, 0xffffu, 0xffffu, 0xf803u},
        {0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x317fu, 0xf3c3u, 0xffffu, 0xffffu, 0xf803u},
        {0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x317fu, 0xf3c3u, 0xffffu, 0xffffu, 0xf803u},
        {0x06c0u, 0xffffu, 0x601eu, 0x00c0u, 0x03ffu, 0u, 0x03c0u, 0u, 0u, 0u},
    };

    cpu->configuration[8u] = 0u;
    for (uint8_t port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        const uint16_t base = (uint16_t)(0x0e00u + port * 0x10u);
        const uint16_t bonded = expected->gpio_masks[port];
        for (size_t index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); index++) {
            const uint16_t address = (uint16_t)(base + offsets[index]);
            const uint16_t writable = (uint16_t)(register_masks[index][port] & bonded);
            dspic33_write_word(cpu, address, UINT16_MAX);
            expect(state, dspic33_read_word(cpu, address) == writable,
                   "profile GPIO SFR masks unbonded pins");
        }

        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 14u), 0u);
        dspic33_gpio_input(cpu, port, 0u);
        dspic33_write_word(cpu, base, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), UINT16_MAX);
        uint16_t output = dspic33_read_word(cpu, (uint16_t)(base + 2u));
        expect(state, output == (uint16_t)(register_masks[1][port] & bonded),
               "profile GPIO output uses only bonded latch pins");

        dspic33_write_word(cpu, base, UINT16_MAX);
        expect(state, dspic33_gpio_drive(cpu, port, UINT16_MAX, UINT16_MAX) == (bonded != 0u),
               "profile GPIO input drive availability matches bonded pins");
        uint16_t input = dspic33_read_word(cpu, (uint16_t)(base + 2u));
        expect(state, input == bonded, "profile GPIO input exposes only bonded pins");
        expect(state, dspic33_gpio_release(cpu, port, UINT16_MAX) == (bonded != 0u),
               "profile GPIO input release availability matches bonded pins");
    }
}

static void test_peripheral_state_comparison(TestState* state, Dspic33epMuDevice device) {
    Dspic33* first = dspic33_create_for_device(device);
    Dspic33* second = dspic33_create_for_device(device);
    uint32_t address = 0u;
    uint32_t first_value = 0u;
    uint32_t second_value = 0u;
    expect(state, first != NULL && second != NULL, "create state comparison devices");
    if (first == NULL || second == NULL) {
        dspic33_destroy(second);
        dspic33_destroy(first);
        return;
    }
    expect(state,
           !dspic33_peripheral_state_equal(NULL, second, &address, &first_value, &second_value) &&
               !dspic33_peripheral_state_equal(first, second, NULL, &first_value, &second_value),
           "peripheral comparison rejects invalid arguments");
    expect(state,
           dspic33_peripheral_state_equal(first, second, &address, &first_value, &second_value),
           "equal peripheral state agrees");
    const uint8_t working_register_value = 0x5au;
    expect(state, dspic33_seed_data(first, 0u, &working_register_value, 1u),
           "seed a working register");
    expect(state,
           dspic33_peripheral_state_equal(first, second, &address, &first_value, &second_value),
           "working register layout does not affect peripheral state");

    uint32_t implemented_address = 0x40u;
    while (!dspic33ep_mu_address_implemented(device, implemented_address)) {
        implemented_address++;
    }
    const uint8_t peripheral_value = 0xa5u;
    expect(state, dspic33_seed_data(first, implemented_address, &peripheral_value, 1u),
           "seed implemented peripheral state");
    expect(state,
           !dspic33_peripheral_state_equal(first, second, &address, &first_value, &second_value) &&
               address == implemented_address && first_value == peripheral_value &&
               first_value != second_value,
           "peripheral comparison reports the first implemented difference");
    expect(state, dspic33_seed_data(second, implemented_address, &peripheral_value, 1u),
           "restore matching peripheral state");
    expect(state, dspic33_seed_data(first, implemented_address + 1u, &peripheral_value, 1u),
           "seed an implemented peripheral high byte");
    expect(state,
           !dspic33_peripheral_state_equal(first, second, &address, &first_value, &second_value) &&
               address == implemented_address && (first_value >> 8u) == peripheral_value &&
               first_value != second_value,
           "peripheral comparison includes both bytes of each SFR");
    expect(state, dspic33_seed_data(second, implemented_address + 1u, &peripheral_value, 1u),
           "restore matching peripheral high byte");
    uint8_t layout_value = (uint8_t)(dspic33_read_byte(second, 0x0122u) ^ 0xffu);
    expect(state, dspic33_seed_data(first, 0x0122u, &layout_value, 1u),
           "seed a live timer position");
    layout_value = (uint8_t)(dspic33_read_byte(second, 0x0908u) ^ 0xffu);
    expect(state, dspic33_seed_data(first, 0x0908u, &layout_value, 1u),
           "seed a live output compare position");
    layout_value = (uint8_t)(dspic33_read_byte(second, 0x0b54u) ^ 0xffu);
    expect(state, dspic33_seed_data(first, 0x0b54u, &layout_value, 1u),
           "seed a translated DMA address");
    layout_value = (uint8_t)(dspic33_read_byte(second, 0x0bf8u) ^ 0xffu);
    expect(state, dspic33_seed_data(first, 0x0bf8u, &layout_value, 1u),
           "seed a translated DMA system address");
    expect(state,
           dspic33_peripheral_state_equal(first, second, &address, &first_value, &second_value),
           "live timing and translated DMA layout do not affect peripheral state");
    dspic33_destroy(second);
    dspic33_destroy(first);
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
    expect(state, profile->y_data_base == expected->y_data_base, "profile Y data base");
    expect(state, profile->dma_ram_base == expected->dma_ram_base, "profile DMA RAM base");
    expect(state, profile->device_id == expected->device_id, "profile device ID");
    expect(state, profile->pin_count == expected->pin_count, "profile package pins");
    expect(state, profile->io_pin_count == expected->io_pin_count, "profile I/O pins");
    expect(state, profile->pwm_generator_count == expected->pwm_generator_count,
           "profile PWM generators");
    expect(state, profile->adc_channel_mask == expected->adc_channel_mask,
           "profile ADC channel mask");

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

    test_peripheral_state_comparison(state, expected->device);

    Dspic33* cpu = dspic33_create_for_device(expected->device);
    expect(state, cpu != NULL, "create profile device");
    expect(state, dspic33_device_profile(cpu) == profile, "created device profile");
    expect(state, dspic33_read_program_word(cpu, 0xff0000u) == expected->device_id >> 16u,
           "profile device ID program word");
    expect(state, dspic33_read_program_word(cpu, 0xff0002u) == 0x4002u,
           "profile revision program word");
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

    uint32_t io_pin_count = 0u;
    for (uint8_t port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t mask = expected->gpio_masks[port];
        io_pin_count += count_bits(mask);
        expect(state, dspic33ep_mu_gpio_port_mask(expected->device, port) == mask,
               "profile GPIO mask");
        cpu->io.gpio_driven[port] = 0u;
        expect(state, dspic33_gpio_drive(cpu, port, UINT16_MAX, UINT16_MAX) == (mask != 0u),
               "profile GPIO drive availability");
        expect(state, cpu->io.gpio_driven[port] == mask, "profile GPIO drive mask");
        expect(state, dspic33_gpio_release(cpu, port, UINT16_MAX) == (mask != 0u),
               "profile GPIO release availability");
    }
    expect(state, io_pin_count == expected->io_pin_count, "profile bonded GPIO count");
    test_gpio_sfr_profile(state, cpu, expected);

    for (uint8_t channel = 0u; channel < DSPIC33_ADC_CHANNEL_COUNT; channel++) {
        const bool channel_implemented =
            (expected->adc_channel_mask & (UINT32_C(1) << channel)) != 0u;
        cpu->io.adc[channel] = 0x0123u;
        dspic33_adc_input(cpu, channel, 0x1abcu);
        expect(state, cpu->io.adc[channel] == (channel_implemented ? 0x0abcu : 0x0123u),
               "profile ADC channel admission follows channel mask");
    }

    expect(state,
           dspic33_pwm_dead_time(cpu, (uint8_t)(expected->pwm_generator_count - 1u), false, 0u),
           "profile last PWM generator accepts input");
    expect(state, !dspic33_pwm_dead_time(cpu, expected->pwm_generator_count, false, 0u),
           "profile first absent PWM generator rejects input");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_read_word(cpu, 0x0872u) ==
               (expected->pwm_generator_count == DSPIC33_PWM_MAX_COUNT ? 0x0004u : 0u),
           "profile PWM7 interrupt priority reset");
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
    expect(&state,
           dspic33ep_mu_gpio_port_mask(DSPIC33EP_MU_DEVICE_COUNT, 0u) == 0u &&
               dspic33ep_mu_gpio_port_mask(DSPIC33EP_MU_DEVICE_512MU814, DSPIC33_GPIO_PORT_COUNT) ==
                   0u,
           "invalid profile GPIO lookup rejected");
    printf("[device-data] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n", state.cases,
           state.passed, state.failed);
    return test_finish(&state);
}
