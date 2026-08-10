#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

enum {
    OSCILLATOR_CONTROL = 0x0742u,
    OSCILLATOR_IO_LOCK = 0x0040u,
    CAPTURE_BASE = 0x0140u,
    CAPTURE_FP_RISING = 0x1c03u,
    CAPTURE_TRIGGER = 0x00c0u,
    COMPARE_BASE = 0x0900u,
    COMPARE_FP_EDGE_PWM = 0x1c06u,
    COMPARE_SELF_SYNC = 0x001fu,
    COMPARATOR_BASE = 0x0a84u,
    COMPARATOR_ENABLE = 0x8000u,
    OPCODE_MOV_BYTE_W0_W1 = 0x784880u,
    OPCODE_MOV_BYTE_W2_W1 = 0x784882u,
    OPCODE_MOV_BYTE_W3_W1 = 0x784883u
};

typedef struct {
    uint16_t address;
    uint16_t mask;
} PpsRegister;

typedef struct {
    uint16_t address;
    uint8_t pin;
    uint8_t shift;
} PpsOutput;

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} PpsConformance;

static const PpsRegister output_registers[] = {
    {0x0680u, 0x3f3fu}, {0x0682u, 0x3f3fu}, {0x0684u, 0x3f3fu}, {0x0686u, 0x3f3fu},
    {0x0688u, 0x3f3fu}, {0x068au, 0x3f3fu}, {0x068cu, 0x3f3fu}, {0x068eu, 0x3f3fu},
    {0x0690u, 0x3f3fu}, {0x0692u, 0x3f3fu}, {0x0696u, 0x3f3fu}, {0x0698u, 0x3f3fu},
    {0x069au, 0x3f3fu}, {0x069cu, 0x3f3fu}, {0x069eu, 0x3f3fu}};

static const PpsRegister input_registers[] = {
    {0x06a0u, 0x7f00u}, {0x06a2u, 0x7f7fu}, {0x06a4u, 0x7f7fu}, {0x06a6u, 0x7f7fu},
    {0x06a8u, 0x7f7fu}, {0x06aau, 0x7f7fu}, {0x06acu, 0x7f7fu}, {0x06aeu, 0x7f7fu},
    {0x06b0u, 0x7f7fu}, {0x06b2u, 0x7f7fu}, {0x06b4u, 0x7f7fu}, {0x06b6u, 0x7f7fu},
    {0x06b8u, 0x7f7fu}, {0x06bau, 0x7f7fu}, {0x06bcu, 0x7f7fu}, {0x06beu, 0x7f7fu},
    {0x06c0u, 0x7f7fu}, {0x06c2u, 0x7f7fu}, {0x06c4u, 0x7f7fu}, {0x06c6u, 0x7f7fu},
    {0x06c8u, 0x7f7fu}, {0x06cau, 0x007fu}, {0x06ceu, 0x007fu}, {0x06d0u, 0x7f7fu},
    {0x06d2u, 0x007fu}, {0x06d4u, 0x7f7fu}, {0x06d6u, 0x7f7fu}, {0x06d8u, 0x7f7fu},
    {0x06dau, 0x7f7fu}, {0x06dcu, 0x007fu}, {0x06deu, 0x7f7fu}, {0x06e0u, 0x007fu},
    {0x06e2u, 0x7f7fu}, {0x06e4u, 0x7f7fu}, {0x06e6u, 0x7f7fu}, {0x06e8u, 0x7f7fu},
    {0x06eau, 0x7f7fu}, {0x06ecu, 0x7f7fu}, {0x06eeu, 0x7f7fu}, {0x06f0u, 0x7f7fu},
    {0x06f2u, 0x007fu}, {0x06f4u, 0x7f7fu}, {0x06f6u, 0x007fu}};

static const PpsOutput outputs[] = {
    {0x0680u, 64u, 0u},  {0x0680u, 65u, 8u},  {0x0682u, 66u, 0u},  {0x0682u, 67u, 8u},
    {0x0684u, 68u, 0u},  {0x0684u, 69u, 8u},  {0x0686u, 70u, 0u},  {0x0686u, 71u, 8u},
    {0x0688u, 79u, 0u},  {0x0688u, 80u, 8u},  {0x068au, 82u, 0u},  {0x068au, 84u, 8u},
    {0x068cu, 85u, 0u},  {0x068cu, 87u, 8u},  {0x068eu, 96u, 0u},  {0x068eu, 97u, 8u},
    {0x0690u, 98u, 0u},  {0x0690u, 99u, 8u},  {0x0692u, 100u, 0u}, {0x0692u, 101u, 8u},
    {0x0696u, 104u, 0u}, {0x0696u, 108u, 8u}, {0x0698u, 109u, 0u}, {0x0698u, 112u, 8u},
    {0x069au, 113u, 0u}, {0x069au, 118u, 8u}, {0x069cu, 120u, 0u}, {0x069cu, 125u, 8u},
    {0x069eu, 126u, 0u}, {0x069eu, 127u, 8u}};

static const uint8_t physical_sources[] = {
    16u,  17u,  18u,  19u,  20u,  21u,  22u,  23u,  30u,  31u,  32u,  33u,  34u,
    35u,  36u,  37u,  38u,  39u,  40u,  41u,  42u,  43u,  44u,  45u,  46u,  47u,
    49u,  50u,  51u,  52u,  60u,  61u,  62u,  64u,  65u,  66u,  67u,  68u,  69u,
    70u,  71u,  72u,  73u,  74u,  75u,  76u,  77u,  78u,  79u,  80u,  81u,  82u,
    83u,  84u,  85u,  86u,  87u,  88u,  89u,  96u,  97u,  98u,  99u,  100u, 101u,
    104u, 108u, 109u, 112u, 113u, 118u, 119u, 120u, 121u, 124u, 125u, 126u, 127u};

static void expect(PpsConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[pps-failed] %s\n", name);
    }
}

static bool physical_source(uint8_t source) {
    size_t index;
    for (index = 0u; index < sizeof(physical_sources); index++) {
        if (physical_sources[index] == source) {
            return true;
        }
    }
    return false;
}

static bool load_sequence(Dspic33* cpu, uint32_t first, uint32_t second,
                          uint32_t third) {
    return dspic33_load_program_word(cpu, 0x0020u, first) &&
           dspic33_load_program_word(cpu, 0x0022u, second) &&
           dspic33_load_program_word(cpu, 0x0024u, third);
}

static bool write_oscillator_low(Dspic33* cpu, uint8_t value) {
    load_sequence(cpu, OPCODE_MOV_BYTE_W2_W1, OPCODE_MOV_BYTE_W3_W1,
                  OPCODE_MOV_BYTE_W0_W1);
    cpu->pc = 0x0020u;
    dspic33_set_working_register(cpu, 0u, value);
    dspic33_set_working_register(cpu, 1u, OSCILLATOR_CONTROL);
    dspic33_set_working_register(cpu, 2u, 0x46u);
    dspic33_set_working_register(cpu, 3u, 0x57u);
    return dspic33_step(cpu) == DSPIC33_RUNNING &&
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_step(cpu) == DSPIC33_RUNNING;
}

static void configure_capture(Dspic33* cpu) {
    dspic33_write_word(cpu, CAPTURE_BASE, 0u);
    dspic33_write_word(cpu, CAPTURE_BASE + 2u, CAPTURE_TRIGGER);
    dspic33_write_word(cpu, CAPTURE_BASE, CAPTURE_FP_RISING);
}

static bool physical_edge(Dspic33* cpu, uint8_t pin) {
    return dspic33_input_capture_pin(cpu, pin, false, 0u) &&
           dspic33_device_advance(cpu, 0u) &&
           dspic33_input_capture_pin(cpu, pin, true, 0u) &&
           dspic33_device_advance(cpu, 1u);
}

static void configure_compare(Dspic33* cpu) {
    dspic33_write_word(cpu, COMPARE_BASE, 0u);
    dspic33_write_word(cpu, COMPARE_BASE + 2u, 0u);
    dspic33_write_word(cpu, COMPARE_BASE + 4u, 3u);
    dspic33_write_word(cpu, COMPARE_BASE + 6u, 1u);
    dspic33_write_word(cpu, COMPARE_BASE, COMPARE_FP_EDGE_PWM);
    dspic33_write_word(cpu, COMPARE_BASE + 2u, COMPARE_SELF_SYNC);
}

static void register_cases(PpsConformance* state, Dspic33* cpu) {
    size_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < sizeof(output_registers) / sizeof(output_registers[0]);
         index++) {
        expect(state, dspic33_read_word(cpu, output_registers[index].address) == 0u,
               "RPOR resets clear");
        dspic33_write_word(cpu, output_registers[index].address, 0xffffu);
        expect(state,
               dspic33_read_word(cpu, output_registers[index].address) ==
                   output_registers[index].mask,
               "RPOR applies device mask");
    }
    for (index = 0u; index < sizeof(input_registers) / sizeof(input_registers[0]);
         index++) {
        expect(state, dspic33_read_word(cpu, input_registers[index].address) == 0u,
               "RPINR resets clear");
        dspic33_write_word(cpu, input_registers[index].address, 0xffffu);
        expect(state,
               dspic33_read_word(cpu, input_registers[index].address) ==
                   input_registers[index].mask,
               "RPINR applies device mask");
    }
    dspic33_write_word(cpu, 0x0694u, 0xffffu);
    dspic33_write_word(cpu, 0x06ccu, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x0694u) == 0u, "absent RPOR10 reads zero");
    expect(state, dspic33_read_word(cpu, 0x06ccu) == 0u, "absent RPINR22 reads zero");
    dspic33_write_word(cpu, 0x0680u, 0x1212u);
    dspic33_write_byte(cpu, 0x0680u, 0x2au);
    expect(state, dspic33_read_word(cpu, 0x0680u) == 0x122au,
           "RPOR low byte preserves high mapping");
    dspic33_write_byte(cpu, 0x0681u, 0x35u);
    expect(state, dspic33_read_word(cpu, 0x0680u) == 0x352au,
           "RPOR high byte preserves low mapping");
}

static void source_cases(PpsConformance* state, Dspic33* cpu) {
    uint16_t source;
    uint8_t channel;
    uint8_t input;
    for (source = 0u; source < 128u; source++) {
        bool accepted;
        dspic33_reset(cpu, 0u);
        accepted = dspic33_input_capture_pin(cpu, (uint8_t)source, false, 0u);
        expect(state, accepted == physical_source((uint8_t)source),
               "physical PPS source validity matches device pin map");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x06aeu, 32u);
    dspic33_write_word(cpu, 0x0e1eu, 0u);
    configure_capture(cpu);
    expect(state, physical_edge(cpu, 32u), "digital PPS edge advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 1u,
           "digital input PPS edge reaches mapped capture");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x06aeu, 32u);
    dspic33_write_word(cpu, 0x0e1eu, 1u);
    configure_capture(cpu);
    expect(state, physical_edge(cpu, 32u), "analog PPS stimulus advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "analog mode disables physical PPS input");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x06aeu, 32u);
    dspic33_write_word(cpu, 0x0e1eu, 0u);
    dspic33_write_word(cpu, 0x0e10u, 0xfffeu);
    configure_capture(cpu);
    expect(state, physical_edge(cpu, 32u), "output PPS stimulus advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "output TRIS disables physical PPS input");

    for (channel = 0u; channel < DSPIC33_QEI_COUNT; channel++) {
        for (input = DSPIC33_QEI_INDEX; input <= DSPIC33_QEI_HOME; input++) {
            uint8_t virtual_source =
                (uint8_t)(8u + channel * 2u + input - DSPIC33_QEI_INDEX);
            dspic33_reset(cpu, 0u);
            dspic33_write_word(cpu, 0x06aeu, virtual_source);
            configure_capture(cpu);
            expect(
                state,
                dspic33_qei_input(cpu, channel, (Dspic33QeiInput)input, false, 0u) &&
                    dspic33_device_advance(cpu, 0u) &&
                    dspic33_qei_input(cpu, channel, (Dspic33QeiInput)input, true, 0u) &&
                    dspic33_device_advance(cpu, 1u),
                "B1 filtered QEI virtual source stimulus advances");
            expect(state, cpu->io.input_capture.fifo[0].count == 0u,
                   "B1 filtered QEI virtual source remains inaccessible");
        }
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x06aeu, 8u);
    configure_capture(cpu);
    expect(state,
           dspic33_qei_input(cpu, 0u, DSPIC33_QEI_INDEX, false, 0u) &&
               dspic33_qei_input(cpu, 0u, DSPIC33_QEI_HOME, false, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               dspic33_qei_input(cpu, 0u, DSPIC33_QEI_PHASE_A, true, 0u) &&
               dspic33_qei_input(cpu, 0u, DSPIC33_QEI_PHASE_B, true, 0u) &&
               dspic33_device_advance(cpu, 1u),
           "QEI phase controls advance independently of virtual PPS sources");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "QEI phase inputs do not alias filtered INDEX PPS source");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x06aeu, 1u);
    configure_capture(cpu);
    expect(
        state,
        dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 0u, 0u) &&
            dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_NEGATIVE_2, 100u,
                                     0u) &&
            dspic33_device_advance(cpu, 0u),
        "comparator virtual PPS baseline advances");
    dspic33_write_word(cpu, COMPARATOR_BASE, COMPARATOR_ENABLE);
    expect(state, dspic33_device_advance(cpu, 1u),
           "comparator virtual PPS baseline settles");
    expect(state,
           dspic33_comparator_input(cpu, 0u, DSPIC33_COMPARATOR_INPUT_POSITIVE, 200u,
                                    0u) &&
               dspic33_device_advance(cpu, 2u),
           "B1 comparator virtual PPS transition advances");
    expect(state, cpu->io.input_capture.fifo[0].count == 0u,
           "B1 comparator virtual source requires physical pin remapping");
}

static void output_cases(PpsConformance* state, Dspic33* cpu) {
    size_t index;
    bool high;
    dspic33_reset(cpu, 0u);
    configure_compare(cpu);
    for (index = 0u; index < sizeof(output_registers) / sizeof(output_registers[0]);
         index++) {
        dspic33_write_word(cpu, output_registers[index].address, 0x1010u);
    }
    for (index = 0u; index < sizeof(outputs) / sizeof(outputs[0]); index++) {
        expect(state,
               dspic33_output_compare_pin(cpu, outputs[index].pin, &high) && high,
               "all remappable output pins can share one peripheral output");
    }
    dspic33_write_word(cpu, 0x0e40u, 0xffffu);
    dspic33_write_word(cpu, 0x0e4eu, 0xffffu);
    expect(state, dspic33_output_compare_pin(cpu, 80u, &high) && high,
           "PPS output routing overrides TRIS and remains available in analog mode");
    dspic33_write_word(cpu, 0x0680u, 0u);
    expect(state, !dspic33_output_compare_pin(cpu, 64u, &high),
           "null output function disconnects remappable pin");
    dspic33_write_word(cpu, 0x0680u, 0x003fu);
    expect(state, !dspic33_output_compare_pin(cpu, 64u, &high),
           "reserved output function does not alias a modeled peripheral");
}

static void protection_cases(PpsConformance* state, Dspic33* cpu) {
    dspic33_load_configuration_word(cpu, 0xf80008u, 0x005eu);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0010u);
    expect(state, write_oscillator_low(cpu, OSCILLATOR_IO_LOCK),
           "IOLOCK protected set sequence executes");
    expect(state,
           (dspic33_read_word(cpu, OSCILLATOR_CONTROL) & OSCILLATOR_IO_LOCK) != 0u,
           "IOLOCK set locks PPS registers");
    dspic33_write_word(cpu, 0x0680u, 0x0020u);
    expect(state, dspic33_read_word(cpu, 0x0680u) == 0x0010u,
           "locked PPS write is rejected");
    expect(state, write_oscillator_low(cpu, 0u), "repeatable unlock sequence executes");
    expect(state,
           (dspic33_read_word(cpu, OSCILLATOR_CONTROL) & OSCILLATOR_IO_LOCK) == 0u,
           "IOL1WAY disabled permits unlock");
    dspic33_write_word(cpu, 0x0680u, 0x0020u);
    expect(state, dspic33_read_word(cpu, 0x0680u) == 0x0020u,
           "unlocked PPS write is accepted");
    expect(state,
           write_oscillator_low(cpu, OSCILLATOR_IO_LOCK) &&
               write_oscillator_low(cpu, 0u),
           "IOL1WAY disabled permits a second configuration session");
    expect(state,
           (dspic33_read_word(cpu, OSCILLATOR_CONTROL) & OSCILLATOR_IO_LOCK) == 0u,
           "second repeatable unlock succeeds");

    dspic33_load_configuration_word(cpu, 0xf80008u, 0x007eu);
    dspic33_reset(cpu, 0u);
    expect(state, write_oscillator_low(cpu, OSCILLATOR_IO_LOCK),
           "one-way IOLOCK set sequence executes");
    expect(state, cpu->io.pps.one_way_committed, "one-way lock session is recorded");
    expect(state, write_oscillator_low(cpu, 0u), "one-way unlock attempt executes");
    expect(state,
           (dspic33_read_word(cpu, OSCILLATOR_CONTROL) & OSCILLATOR_IO_LOCK) != 0u,
           "IOL1WAY blocks a second PPS configuration session");
}

static void lifecycle_cases(PpsConformance* state, Dspic33* source, Dspic33* copy) {
    uint64_t software_resets;
    uint64_t illegal_resets;
    dspic33_load_configuration_word(source, 0xf80008u, 0x005eu);
    dspic33_reset(source, 0u);
    dspic33_write_word(source, 0x0680u, 0x0010u);
    expect(state, dspic33_copy(copy, source), "copy preserves PPS state");
    expect(state, dspic33_read_word(copy, 0x0680u) == 0x0010u,
           "copy preserves PPS register mapping");
    expect(state,
           copy->io.pps.shadow[0] == source->io.pps.shadow[0] &&
               copy->io.pps.one_way_committed == source->io.pps.one_way_committed,
           "copy preserves independent PPS lifecycle state");
    dspic33_write_word(source, 0x0680u, 0x0020u);
    expect(state, dspic33_read_word(copy, 0x0680u) == 0x0010u,
           "source PPS mutation does not alter copy");

    software_resets = source->software_reset_count;
    illegal_resets = source->illegal_reset_count;
    source->data[0x0680u] = 0x21u;
    expect(state, dspic33_device_advance(source, 0u),
           "PPS shadow mismatch reset advances");
    expect(state, (dspic33_read_word(source, 0x0740u) & 0x0200u) != 0u,
           "PPS shadow mismatch sets RCON configuration-mismatch cause");
    expect(state, source->pc == 0u && dspic33_read_word(source, 0x0680u) == 0u,
           "PPS shadow mismatch resets execution and mappings");
    expect(state,
           source->software_reset_count == software_resets &&
               source->illegal_reset_count == illegal_resets,
           "configuration mismatch is not counted as software or illegal reset");

    source->data[0x0680u] = 0u;
    source->data[0x0681u] = 0x80u;
    expect(state, dspic33_device_advance(source, 0u),
           "unimplemented PPS-bit mutation is ignored by shadow monitor");
    expect(state,
           source->software_reset_count == software_resets &&
               source->illegal_reset_count == illegal_resets,
           "unimplemented PPS bits do not cause configuration mismatch reset");
    dspic33_reset(source, 0u);
    expect(state, source->io.pps.shadow[0] == 0u && !source->io.pps.one_way_committed,
           "cold reset clears PPS shadow and one-way session state");
}

int main(void) {
    Dspic33 source;
    Dspic33 copy;
    PpsConformance state = {0u, 0u, 0u};
    if (!dspic33_initialize(&source) || !dspic33_initialize(&copy)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    register_cases(&state, &source);
    source_cases(&state, &source);
    output_cases(&state, &source);
    protection_cases(&state, &source);
    lifecycle_cases(&state, &source, &copy);
    expect(&state, state.cases == 324u, "PPS assertion arithmetic");
    printf("[pps-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&copy);
    dspic33_destroy(&source);
    return state.failed == 0u ? 0 : 1;
}
