#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} GpioConformance;

static const uint16_t tris_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e00u, 0x0e10u, 0x0e20u, 0x0e30u, 0x0e40u, 0x0e50u, 0x0e60u};
static const uint16_t port_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e02u, 0x0e12u, 0x0e22u, 0x0e32u, 0x0e42u, 0x0e52u, 0x0e62u};
static const uint16_t latch_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e04u, 0x0e14u, 0x0e24u, 0x0e34u, 0x0e44u, 0x0e54u, 0x0e64u};
static const uint16_t analog_addresses[DSPIC33_GPIO_PORT_COUNT] = {
    0x0e0eu, 0x0e1eu, 0x0e2eu, 0x0e3eu, 0x0e4eu, 0u, 0x0e6eu};
static const uint16_t port_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x313fu, 0xf3cfu};
static const uint16_t latch_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0xc6ffu, 0xffffu, 0xf01eu, 0xffffu, 0x03ffu, 0x313fu, 0xf3c3u};
static const uint16_t analog_masks[DSPIC33_GPIO_PORT_COUNT] = {
    0x06c0u, 0xffffu, 0x601eu, 0x00c0u, 0x03ffu, 0u, 0x03c0u};
static const uint16_t input_only_masks[DSPIC33_GPIO_PORT_COUNT] = {0u, 0u, 0u,     0u,
                                                                   0u, 0u, 0x000cu};

static void expect(GpioConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[gpio-failed] %s\n", name);
    }
}

static void reset_port(Dspic33* cpu, uint8_t port, uint16_t input) {
    dspic33_gpio_input(cpu, port, input);
    dspic33_reset(cpu, 0u);
    if (analog_addresses[port] != 0u) {
        dspic33_write_word(cpu, analog_addresses[port], 0u);
    }
}

static uint16_t expected_pins(uint8_t port, uint16_t input, uint16_t tris,
                              uint16_t latch, uint16_t analog) {
    uint16_t inputs = (uint16_t)((tris & latch_masks[port]) | input_only_masks[port]);
    return (uint16_t)(((input & inputs & ~analog) | (latch & ~inputs)) &
                      port_masks[port]);
}

static void core_cases(GpioConformance* state, Dspic33* cpu) {
    uint8_t port;
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t mixed_tris = (uint16_t)(0xaaaau & latch_masks[port]);
        uint16_t mixed_expected;
        reset_port(cpu, port, 0x3cc3u);
        dspic33_write_word(cpu, tris_addresses[port], 0u);
        dspic33_write_word(cpu, latch_addresses[port], 0xa55au);
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) ==
                   expected_pins(port, 0x3cc3u, 0u, 0xa55au, 0u),
               "PORT output read uses LAT");
        dspic33_gpio_input(cpu, port, 0xffffu);
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) ==
                   expected_pins(port, 0xffffu, 0u, 0xa55au, 0u),
               "PORT output read ignores external inputs");
        dspic33_write_word(cpu, tris_addresses[port], latch_masks[port]);
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) ==
                   expected_pins(port, 0xffffu, latch_masks[port], 0xa55au, 0u),
               "PORT input read uses external levels");
        dspic33_gpio_input(cpu, port, 0x3cc3u);
        dspic33_write_word(cpu, tris_addresses[port], mixed_tris);
        mixed_expected = expected_pins(port, 0x3cc3u, mixed_tris, 0xa55au, 0u);
        expect(state, dspic33_read_word(cpu, port_addresses[port]) == mixed_expected,
               "PORT read composes mixed input and output pins");

        dspic33_write_word(cpu, tris_addresses[port], 0u);
        dspic33_write_word(cpu, port_addresses[port], 0x5aa5u);
        expect(state,
               dspic33_read_word(cpu, latch_addresses[port]) ==
                   (uint16_t)(0x5aa5u & latch_masks[port]),
               "PORT word write updates LAT");
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) ==
                   expected_pins(port, 0x3cc3u, 0u, 0x5aa5u, 0u),
               "PORT word write reads back physical pins");

        dspic33_write_word(cpu, tris_addresses[port], latch_masks[port]);
        dspic33_gpio_input(cpu, port, 0x0f0fu);
        dspic33_write_word(cpu, port_addresses[port], 0xa55au);
        expect(state,
               dspic33_read_word(cpu, latch_addresses[port]) ==
                   (uint16_t)(0xa55au & latch_masks[port]),
               "PORT input-mode write stores LAT value");
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) ==
                   expected_pins(port, 0x0f0fu, latch_masks[port], 0xa55au, 0u),
               "PORT input-mode write leaves input read unchanged");
        dspic33_write_word(cpu, tris_addresses[port], 0u);
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) ==
                   expected_pins(port, 0x0f0fu, 0u, 0xa55au, 0u),
               "PORT stored input-mode LAT appears after output selection");

        dspic33_gpio_input(cpu, port, 0xffffu);
        dspic33_write_word(cpu, tris_addresses[port], 0xffffu);
        dspic33_write_word(cpu, latch_addresses[port], 0xffffu);
        expect(state,
               (dspic33_read_word(cpu, port_addresses[port]) &
                (uint16_t)~port_masks[port]) == 0u,
               "PORT unimplemented bits read zero");

        dspic33_write_word(cpu, latch_addresses[port], 0xa55au);
        dspic33_write_byte(cpu, port_addresses[port], 0x3cu);
        expect(state,
               dspic33_read_word(cpu, latch_addresses[port]) ==
                   (uint16_t)(0x003cu & latch_masks[port]),
               "PORT low-byte write zero-extends into LAT");
        dspic33_write_byte(cpu, (uint16_t)(port_addresses[port] + 1u), 0xc3u);
        expect(state,
               dspic33_read_word(cpu, latch_addresses[port]) ==
                   (uint16_t)(0xc300u & latch_masks[port]),
               "PORT high-byte write zero-extends into LAT");
    }
}

static void analog_cases(GpioConformance* state, Dspic33* cpu) {
    uint8_t port;
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t mixed_tris;
        uint16_t expected;
        if (analog_addresses[port] == 0u) {
            continue;
        }
        reset_port(cpu, port, analog_masks[port]);
        dspic33_write_word(cpu, tris_addresses[port], latch_masks[port]);
        dspic33_write_word(cpu, latch_addresses[port], 0u);
        dspic33_write_word(cpu, analog_addresses[port], analog_masks[port]);
        expect(state, dspic33_read_word(cpu, port_addresses[port]) == 0u,
               "ANSEL suppresses external digital input");
        dspic33_write_word(cpu, tris_addresses[port], 0u);
        dspic33_write_word(cpu, latch_addresses[port], analog_masks[port]);
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) == analog_masks[port],
               "ANSEL preserves LAT-driven output");
        dspic33_write_word(cpu, tris_addresses[port], latch_masks[port]);
        dspic33_write_word(cpu, latch_addresses[port], 0u);
        dspic33_write_word(cpu, analog_addresses[port], 0u);
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) == analog_masks[port],
               "clearing ANSEL reveals external input");
        mixed_tris = (uint16_t)(0xaaaau & latch_masks[port]);
        dspic33_gpio_input(cpu, port, 0xffffu);
        dspic33_write_word(cpu, tris_addresses[port], mixed_tris);
        dspic33_write_word(cpu, latch_addresses[port], 0x5555u);
        dspic33_write_word(cpu, analog_addresses[port], analog_masks[port]);
        expected =
            expected_pins(port, 0xffffu, mixed_tris, 0x5555u, analog_masks[port]);
        expect(state, dspic33_read_word(cpu, port_addresses[port]) == expected,
               "ANSEL gates only mixed external input path");
    }
}

static void input_only_usb_cases(GpioConformance* state, Dspic33* cpu) {
    reset_port(cpu, 6u, 0x000cu);
    dspic33_write_word(cpu, tris_addresses[6], 0u);
    dspic33_write_word(cpu, latch_addresses[6], 0u);
    expect(state, (dspic33_read_word(cpu, port_addresses[6]) & 0x000cu) == 0x000cu,
           "RG2 and RG3 reflect high external inputs");
    dspic33_gpio_input(cpu, 6u, 0u);
    expect(state, (dspic33_read_word(cpu, port_addresses[6]) & 0x000cu) == 0u,
           "RG2 and RG3 reflect low external inputs");
    dspic33_gpio_input(cpu, 6u, 0x000cu);
    dspic33_write_word(cpu, port_addresses[6], 0x000cu);
    expect(state, (dspic33_read_word(cpu, latch_addresses[6]) & 0x000cu) == 0u,
           "RG2 and RG3 writes do not create LAT bits");
    dspic33_write_word(cpu, 0x04cau, 0x0001u);
    expect(state, (dspic33_read_word(cpu, port_addresses[6]) & 0x000cu) == 0u,
           "USBEN suppresses RG2 and RG3 digital input");
    dspic33_write_word(cpu, 0x04cau, 0u);
    expect(state, (dspic33_read_word(cpu, port_addresses[6]) & 0x000cu) == 0x000cu,
           "disabling USB reveals RG2 and RG3 input");
}

static bool run_bit_instruction(Dspic33* cpu, uint32_t opcode, uint16_t address) {
    dspic33_set_working_register(cpu, 4u, address);
    return dspic33_load_program_word(cpu, 0u, opcode) &&
           dspic33_step(cpu) == DSPIC33_RUNNING;
}

static void read_modify_write_cases(GpioConformance* state, Dspic33* cpu) {
    reset_port(cpu, 3u, 0x00f0u);
    dspic33_write_word(cpu, tris_addresses[3], 0xffffu);
    dspic33_write_word(cpu, latch_addresses[3], 0xffffu);
    expect(state,
           run_bit_instruction(cpu, 0xa00014u, port_addresses[3]) &&
               dspic33_read_word(cpu, latch_addresses[3]) == 0x00f1u,
           "PORT BSET reads pins and writes modified LAT");

    reset_port(cpu, 3u, 0x00f0u);
    dspic33_write_word(cpu, tris_addresses[3], 0xffffu);
    dspic33_write_word(cpu, latch_addresses[3], 0xffffu);
    expect(state,
           run_bit_instruction(cpu, 0xa14014u, port_addresses[3]) &&
               dspic33_read_word(cpu, latch_addresses[3]) == 0x00e0u,
           "PORT BCLR reads pins and writes modified LAT");

    reset_port(cpu, 3u, 0xffffu);
    dspic33_write_word(cpu, tris_addresses[3], 0xffffu);
    dspic33_write_word(cpu, latch_addresses[3], 0x1000u);
    expect(state,
           run_bit_instruction(cpu, 0xa00014u, latch_addresses[3]) &&
               dspic33_read_word(cpu, latch_addresses[3]) == 0x1001u,
           "LAT BSET reads and modifies latch state");
}

static void lifecycle_cases(GpioConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    uint8_t port;
    bool initialized;
    for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
        uint16_t input = (uint16_t)(0x9696u ^ ((uint16_t)port * 0x1111u));
        dspic33_gpio_input(cpu, port, input);
        dspic33_reset(cpu, 0u);
        if (analog_addresses[port] != 0u) {
            dspic33_write_word(cpu, analog_addresses[port], 0u);
        }
        expect(state,
               dspic33_read_word(cpu, port_addresses[port]) ==
                   (uint16_t)(input & port_masks[port]),
               "MCU reset preserves physical GPIO stimulus");
    }

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize GPIO copy destination");
    if (initialized) {
        for (port = 0u; port < DSPIC33_GPIO_PORT_COUNT; port++) {
            dspic33_gpio_input(cpu, port,
                               (uint16_t)(0x1234u + (uint16_t)port * 0x1111u));
        }
        expect(state,
               dspic33_copy(&copy, cpu) && copy.io.gpio[0] == 0x0234u &&
                   copy.io.gpio[1] == 0x2345u && copy.io.gpio[2] == 0x3016u &&
                   copy.io.gpio[3] == 0x4567u && copy.io.gpio[4] == 0x0278u &&
                   copy.io.gpio[5] == 0x2109u && copy.io.gpio[6] == 0x708au,
               "copy preserves independent GPIO stimulus");
        dspic33_gpio_input(cpu, 0u, 0xffffu);
        dspic33_gpio_input(&copy, 1u, 0u);
        expect(state,
               copy.io.gpio[0] == 0x0234u && cpu->io.gpio[1] == 0x2345u &&
                   cpu->io.gpio[0] == 0xc6ffu && copy.io.gpio[1] == 0u,
               "copied GPIO stimuli remain independent");
        dspic33_destroy(&copy);
    }

    {
        uint16_t retained = cpu->io.gpio[DSPIC33_GPIO_PORT_COUNT - 1u];
        dspic33_gpio_input(cpu, DSPIC33_GPIO_PORT_COUNT, 0xffffu);
        expect(state, cpu->io.gpio[DSPIC33_GPIO_PORT_COUNT - 1u] == retained,
               "invalid GPIO port input is ignored");
    }

    dspic33_reset(cpu, 0u);
    dspic33_gpio_input(cpu, 0u, 0xffffu);
    expect(state, (dspic33_read_word(cpu, 0x0802u) & 0x0008u) == 0u,
           "GPIO input change does not emulate incomplete CN state");
}

int main(void) {
    Dspic33 cpu;
    GpioConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize GPIO processor");
    if (initialized) {
        core_cases(&state, &cpu);
        analog_cases(&state, &cpu);
        input_only_usb_cases(&state, &cpu);
        read_modify_write_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        expect(&state, state.cases == 129u, "GPIO assertion accounting");
        dspic33_destroy(&cpu);
    }
    printf("[gpio-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
