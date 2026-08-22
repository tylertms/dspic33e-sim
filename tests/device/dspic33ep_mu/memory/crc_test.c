#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    CRC_CONTROL = 0x0640u,
    CRC_CONFIG = 0x0642u,
    CRC_POLYNOMIAL_LOW = 0x0644u,
    CRC_POLYNOMIAL_HIGH = 0x0646u,
    CRC_DATA_LOW = 0x0648u,
    CRC_DATA_HIGH = 0x064au,
    CRC_SHIFT_LOW = 0x064cu,
    CRC_SHIFT_HIGH = 0x064eu,
    CRC_PMD_ADDRESS = 0x0764u,
    CRC_ENABLE = 0x8000u,
    CRC_STOP_IDLE = 0x2000u,
    CRC_FULL = 0x0080u,
    CRC_EMPTY = 0x0040u,
    CRC_INTERRUPT_EMPTY = 0x0020u,
    CRC_GO = 0x0010u,
    CRC_LITTLE_ENDIAN = 0x0008u,
    CRC_PMD = 0x0080u,
    CRC_INTERRUPT_FLAG = 0x0008u,
    CRC_BITS_PER_CYCLE = 2u,
    CRC_IRQ = 67u,
    CRC_VECTOR = 0x0200u,
    CRC_PRIORITY = 3u
};

void dspic33_device_internal_run_crc(Dspic33* cpu, uint16_t generation);
static bool interrupt_flag(Dspic33* cpu);

static void empty_active_case(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    cpu->data[CRC_CONTROL] = (uint8_t)(CRC_ENABLE | CRC_GO);
    cpu->data[CRC_CONTROL + 1u] = (uint8_t)((CRC_ENABLE | CRC_GO) >> 8u);
    cpu->io.crc.active = true;
    cpu->io.crc.count = 0u;
    cpu->io.crc.bits_remaining = 0u;
    dspic33_device_internal_run_crc(cpu, cpu->io.crc.generation);
    expect(state,
           !cpu->io.crc.active && (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) == 0u &&
               interrupt_flag(cpu),
           "active empty CRC completes immediately");
}

static uint32_t shift_value(Dspic33* cpu) {
    return (uint32_t)dspic33_read_word(cpu, CRC_SHIFT_LOW) |
           ((uint32_t)dspic33_read_word(cpu, CRC_SHIFT_HIGH) << 16u);
}

static uint8_t valid_words(Dspic33* cpu) {
    return (uint8_t)((dspic33_read_word(cpu, CRC_CONTROL) >> 8u) & 0x1fu);
}

static uint32_t width_mask(uint8_t width) {
    return width == 32u ? UINT32_MAX : ((uint32_t)1u << width) - 1u;
}

static uint32_t reference_crc(const uint32_t* words, uint8_t count, uint8_t data_width,
                              uint8_t polynomial_width, uint32_t polynomial, uint32_t seed,
                              bool little_endian) {
    uint32_t remainder = seed & width_mask(polynomial_width);
    uint32_t taps = (polynomial | 1u) & width_mask(polynomial_width);
    uint8_t word_index;
    for (word_index = 0u; word_index < count; word_index++) {
        uint8_t bit_index;
        for (bit_index = 0u; bit_index < data_width; bit_index++) {
            uint8_t input_index =
                little_endian ? bit_index : (uint8_t)(data_width - bit_index - 1u);
            bool feedback = ((remainder >> (polynomial_width - 1u)) & 1u) != 0u;
            feedback = feedback != ((words[word_index] >> input_index) & 1u);
            remainder = (remainder << 1u) & width_mask(polynomial_width);
            if (feedback) {
                remainder ^= taps;
            }
        }
    }
    return remainder;
}

static bool interrupt_flag(Dspic33* cpu) {
    return (dspic33_read_word(cpu, 0x0808u) & CRC_INTERRUPT_FLAG) != 0u;
}

static void clear_interrupt(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x0808u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0808u) & ~CRC_INTERRUPT_FLAG));
}

static void configure(Dspic33* cpu, uint8_t data_width, uint8_t polynomial_width,
                      uint32_t polynomial, bool little_endian, bool interrupt_on_empty) {
    uint16_t control = CRC_ENABLE;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, CRC_CONFIG,
                       (uint16_t)(((uint16_t)(data_width - 1u) << 8u) | (polynomial_width - 1u)));
    dspic33_write_word(cpu, CRC_POLYNOMIAL_LOW, (uint16_t)polynomial);
    dspic33_write_word(cpu, CRC_POLYNOMIAL_HIGH, (uint16_t)(polynomial >> 16u));
    dspic33_write_word(cpu, CRC_SHIFT_LOW, 0u);
    dspic33_write_word(cpu, CRC_SHIFT_HIGH, 0u);
    if (little_endian) {
        control |= CRC_LITTLE_ENDIAN;
    }
    if (interrupt_on_empty) {
        control |= CRC_INTERRUPT_EMPTY;
    }
    dspic33_write_word(cpu, CRC_CONTROL, control);
}

static void enqueue(Dspic33* cpu, uint8_t data_width, uint32_t value) {
    if (data_width <= 8u) {
        dspic33_write_byte(cpu, CRC_DATA_LOW, (uint8_t)value);
    } else if (data_width <= 16u) {
        dspic33_write_word(cpu, CRC_DATA_LOW, (uint16_t)value);
    } else {
        dspic33_write_word(cpu, CRC_DATA_LOW, (uint16_t)value);
        dspic33_write_word(cpu, CRC_DATA_HIGH, (uint16_t)(value >> 16u));
    }
}

static void start(Dspic33* cpu) {
    dspic33_write_word(cpu, CRC_CONTROL, (uint16_t)(dspic33_read_word(cpu, CRC_CONTROL) | CRC_GO));
}

static void reset_and_access_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, CRC_CONTROL) == CRC_EMPTY, "CRCCON1 reset");
    expect(state, dspic33_read_word(cpu, CRC_CONFIG) == 0u, "CRCCON2 reset");
    expect(state, dspic33_read_word(cpu, CRC_POLYNOMIAL_LOW) == 0u, "CRCXORL reset");
    expect(state, dspic33_read_word(cpu, CRC_POLYNOMIAL_HIGH) == 0u, "CRCXORH reset");
    expect(state, dspic33_read_word(cpu, CRC_DATA_LOW) == 0u, "CRCDATL reset");
    expect(state, dspic33_read_word(cpu, CRC_DATA_HIGH) == 0u, "CRCDATH reset");
    expect(state, shift_value(cpu) == 0u, "CRCWDAT reset");
    expect(state, !cpu->io.crc.active && cpu->io.crc.count == 0u, "CRC runtime reset");

    dspic33_write_word(cpu, CRC_CONTROL, 0xffffu);
    expect(state, dspic33_read_word(cpu, CRC_CONTROL) == 0xa078u, "CRCCON1 access mask");
    dspic33_write_word(cpu, CRC_CONTROL, 0u);
    expect(state, dspic33_read_word(cpu, CRC_CONTROL) == CRC_EMPTY, "CRC disable resets status");
    dspic33_write_word(cpu, CRC_CONFIG, 0xffffu);
    expect(state, dspic33_read_word(cpu, CRC_CONFIG) == 0x1f1fu, "CRCCON2 access mask");
    dspic33_write_word(cpu, CRC_POLYNOMIAL_LOW, 0xffffu);
    expect(state, dspic33_read_word(cpu, CRC_POLYNOMIAL_LOW) == 0xfffeu,
           "CRCXORL bit zero reserved");
    dspic33_write_word(cpu, CRC_POLYNOMIAL_HIGH, 0xffffu);
    expect(state, dspic33_read_word(cpu, CRC_POLYNOMIAL_HIGH) == 0xffffu, "CRCXORH writable");
    dspic33_write_word(cpu, CRC_DATA_LOW, 0xa55au);
    dspic33_write_word(cpu, CRC_DATA_HIGH, 0x5aa5u);
    expect(state,
           dspic33_read_word(cpu, CRC_DATA_LOW) == 0u &&
               dspic33_read_word(cpu, CRC_DATA_HIGH) == 0u,
           "CRCDAT reads zero");
    expect(state, cpu->io.crc.count == 0u, "disabled CRCDAT writes ignored");
}

static void fifo_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t widths[] = {1u, 8u, 9u, 16u, 17u, 24u, 25u, 32u};
    uint8_t width_index;
    for (width_index = 0u; width_index < sizeof(widths); width_index++) {
        uint8_t width = widths[width_index];
        uint8_t capacity = width <= 8u ? 16u : width <= 16u ? 8u : 4u;
        uint8_t index;
        configure(cpu, width, 16u, 0x1021u, false, false);
        for (index = 0u; index < capacity; index++) {
            enqueue(cpu, width, index);
        }
        expect(state, valid_words(cpu) == capacity, "width FIFO capacity");
        expect(state, (dspic33_read_word(cpu, CRC_CONTROL) & CRC_FULL) != 0u, "width FIFO full");
        enqueue(cpu, width, UINT32_MAX);
        expect(state, valid_words(cpu) == capacity, "full FIFO rejects word");
    }
    expect(state, dspic33_read_word(cpu, CRC_DATA_LOW) == 0u, "FIFO data read zero");
}

static void lane_cases(TestState* state, Dspic33* cpu) {
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    dspic33_write_byte(cpu, CRC_DATA_LOW, 0x12u);
    dspic33_write_byte(cpu, CRC_DATA_LOW + 1u, 0x34u);
    dspic33_write_word(cpu, CRC_DATA_LOW, 0x5678u);
    expect(state, valid_words(cpu) == 3u, "byte lanes enqueue three words");
    expect(state,
           cpu->io.crc.words[0] == 0x12u && cpu->io.crc.words[1] == 0x34u &&
               cpu->io.crc.words[2] == 0x78u,
           "byte lane order and word-write low lane");

    configure(cpu, 16u, 16u, 0x1021u, false, false);
    dspic33_write_byte(cpu, CRC_DATA_LOW, 0x12u);
    expect(state, valid_words(cpu) == 0u, "word low byte ignored");
    dspic33_write_byte(cpu, CRC_DATA_LOW + 1u, 0x34u);
    expect(state, valid_words(cpu) == 0u, "word high byte ignored");
    dspic33_write_word(cpu, CRC_DATA_LOW, 0x5678u);
    dspic33_write_word(cpu, CRC_DATA_HIGH, 0x9abcu);
    expect(state, valid_words(cpu) == 1u, "word write commits and CRCDATH ignores");
    expect(state, cpu->io.crc.words[0] == 0x5678u, "word lane value");

    configure(cpu, 17u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, CRC_DATA_LOW, 0xabcdu);
    expect(state, valid_words(cpu) == 0u, "17-bit low half waits");
    dspic33_write_byte(cpu, CRC_DATA_HIGH, 0x01u);
    expect(state, valid_words(cpu) == 1u && cpu->io.crc.words[0] == 0x1abcdu,
           "17-bit low high-register byte commits");
    dspic33_write_byte(cpu, CRC_DATA_HIGH + 1u, 0x99u);
    expect(state, valid_words(cpu) == 1u, "17-bit unused lane does not commit");

    configure(cpu, 24u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, CRC_DATA_LOW, 0x3456u);
    dspic33_write_byte(cpu, CRC_DATA_HIGH, 0x12u);
    expect(state, valid_words(cpu) == 1u && cpu->io.crc.words[0] == 0x123456u,
           "24-bit lane assembly");

    configure(cpu, 25u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, CRC_DATA_LOW, 0x3456u);
    dspic33_write_byte(cpu, CRC_DATA_HIGH, 0x12u);
    expect(state, valid_words(cpu) == 0u, "25-bit third byte waits");
    dspic33_write_byte(cpu, CRC_DATA_HIGH + 1u, 0x01u);
    expect(state, valid_words(cpu) == 1u && cpu->io.crc.words[0] == 0x01123456u,
           "25-bit fourth byte commits");

    configure(cpu, 32u, 32u, 0x04c11db7u, false, false);
    dspic33_write_word(cpu, CRC_DATA_LOW, 0xcdefu);
    dspic33_write_word(cpu, CRC_DATA_HIGH, 0x89abu);
    expect(state, valid_words(cpu) == 1u && cpu->io.crc.words[0] == 0x89abcdefu,
           "32-bit word lane assembly");

    configure(cpu, 32u, 32u, 0x04c11db7u, false, false);
    dspic33_write_byte(cpu, CRC_DATA_LOW, 0xefu);
    dspic33_write_byte(cpu, CRC_DATA_LOW + 1u, 0xcdu);
    dspic33_write_word(cpu, CRC_DATA_HIGH, 0x89abu);
    expect(state, valid_words(cpu) == 1u && cpu->io.crc.words[0] == 0x89abcdefu,
           "32-bit byte lanes assemble the low half");
}

static void run_vector(TestState* state, Dspic33* cpu, const uint32_t* words, uint8_t count,
                       uint8_t data_width, uint8_t polynomial_width, uint32_t polynomial,
                       uint32_t seed, bool little_endian, uint32_t expected, const char* name) {
    uint8_t index;
    configure(cpu, data_width, polynomial_width, polynomial, little_endian, false);
    dspic33_write_word(cpu, CRC_SHIFT_LOW, (uint16_t)seed);
    dspic33_write_word(cpu, CRC_SHIFT_HIGH, (uint16_t)(seed >> 16u));
    for (index = 0u; index < count; index++) {
        enqueue(cpu, data_width, words[index]);
    }
    start(cpu);
    expect(state,
           dspic33_device_advance(cpu, ((uint64_t)count * data_width + CRC_BITS_PER_CYCLE - 1u) /
                                           CRC_BITS_PER_CYCLE),
           "vector advances");
    expect(state, shift_value(cpu) == expected, name);
    expect(state,
           expected == reference_crc(words, count, data_width, polynomial_width, polynomial, seed,
                                     little_endian),
           "literal vector matches independent reference");
    expect(state, (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) == 0u && valid_words(cpu) == 0u,
           "vector completes and empties");
}

static void known_vector_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    static const uint32_t text_with_zeros[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 0u, 0u};
    static const uint32_t endian_words[] = {0x96u, 0u, 0u};
    static const uint32_t word16[] = {0x0201u, 0u};
    static const uint32_t word32[] = {0x04030201u, 0u};
    static const uint32_t narrow[] = {0x2au, 0x15u};
    static const uint32_t wide[] = {0x89abcdefu};
    static const uint32_t seeded[] = {0x31u};
    run_vector(state, cpu, text, 9u, 8u, 16u, 0x1021u, 0u, false, 0x31c3u, "CRC-16 text vector");
    run_vector(state, cpu, text_with_zeros, 11u, 8u, 16u, 0x1021u, 0u, false, 0xdf8bu,
               "CRC-16 text plus zeros vector");
    run_vector(state, cpu, endian_words, 3u, 8u, 16u, 0x1021u, 0u, false, 0xca99u,
               "MSB-first byte vector");
    run_vector(state, cpu, endian_words, 3u, 8u, 16u, 0x1021u, 0u, true, 0x05fau,
               "LSB-first byte vector");
    run_vector(state, cpu, word16, 2u, 16u, 16u, 0x1021u, 0u, false, 0xda58u, "16-bit data vector");
    run_vector(state, cpu, word32, 2u, 32u, 16u, 0x1021u, 0u, false, 0xf6feu, "32-bit data vector");
    run_vector(state, cpu, narrow, 2u, 6u, 5u, 0x04u, 0u, false, 0x03u, "narrow polynomial vector");
    run_vector(state, cpu, narrow, 2u, 6u, 5u, 0x04u, 0u, true, 0x12u,
               "narrow little-endian vector");
    run_vector(state, cpu, wide, 1u, 32u, 32u, 0x04c11db6u, 0u, false, 0x7bd7f146u,
               "32-bit polynomial vector");
    run_vector(state, cpu, wide, 1u, 32u, 32u, 0x04c11db6u, 0u, true, 0xdb503f73u,
               "32-bit little-endian vector");
    run_vector(state, cpu, seeded, 1u, 8u, 16u, 0x1021u, 0x1d0fu, false, 0xeaeeu,
               "seeded CRC vector");
    expect(state, interrupt_flag(cpu), "completion interrupt flag");
}

static void width_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t data_width;
    for (data_width = 1u; data_width <= 32u; data_width++) {
        uint8_t polynomial_width = (uint8_t)(((data_width * 7u) % 32u) + 1u);
        uint32_t word =
            (0xa5a5f00du ^ ((uint32_t)data_width * 0x10204081u)) & width_mask(data_width);
        uint32_t polynomial = 0x04c11db6u ^ ((uint32_t)data_width * 0x01010101u);
        uint32_t seed = 0x1d0f5aa5u ^ ((uint32_t)data_width * 0x11111111u);
        uint32_t expected = reference_crc(&word, 1u, data_width, polynomial_width, polynomial, seed,
                                          (data_width & 1u) != 0u);
        configure(cpu, data_width, polynomial_width, polynomial, (data_width & 1u) != 0u, false);
        dspic33_write_word(cpu, CRC_SHIFT_LOW, (uint16_t)seed);
        dspic33_write_word(cpu, CRC_SHIFT_HIGH, (uint16_t)(seed >> 16u));
        enqueue(cpu, data_width, word);
        start(cpu);
        expect(state,
               dspic33_device_advance(cpu,
                                      (data_width + CRC_BITS_PER_CYCLE - 1u) / CRC_BITS_PER_CYCLE),
               "width matrix advances");
        expect(state,
               shift_value(cpu) == expected && !cpu->io.crc.active &&
                   (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) == 0u,
               "width and polynomial matrix result");
    }
}

static void interrupt_timing_cases(TestState* state, Dspic33* cpu) {
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x12u);
    enqueue(cpu, 8u, 0x34u);
    start(cpu);
    expect(state,
           cpu->io.crc.active && valid_words(cpu) == 2u && cpu->events.count == 1u &&
               !interrupt_flag(cpu),
           "completion-select starts without interrupt");
    expect(state, dspic33_device_advance(cpu, 1u), "first word begins");
    expect(state,
           valid_words(cpu) == 1u && !interrupt_flag(cpu) &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_EMPTY) == 0u,
           "first word pop retains nonempty state");
    expect(state, dspic33_device_advance(cpu, 3u), "first word completes");
    expect(state, valid_words(cpu) == 1u && cpu->io.crc.active && !interrupt_flag(cpu),
           "first word completion does not interrupt");
    expect(state, dspic33_device_advance(cpu, 1u), "last word begins");
    expect(state,
           valid_words(cpu) == 0u && cpu->io.crc.active && !interrupt_flag(cpu) &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_EMPTY) != 0u,
           "completion-select empty remains active");
    expect(state, dspic33_device_advance(cpu, 3u), "last word completes");
    expect(state,
           !cpu->io.crc.active && interrupt_flag(cpu) &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) == 0u,
           "completion-select interrupts after final shift");

    configure(cpu, 8u, 16u, 0x1021u, false, true);
    enqueue(cpu, 8u, 0x12u);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "empty-select begins word");
    expect(state,
           valid_words(cpu) == 0u && cpu->io.crc.active && interrupt_flag(cpu) &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) != 0u,
           "empty-select interrupts before final shift");
    clear_interrupt(cpu);
    expect(state, dspic33_device_advance(cpu, 3u), "empty-select completes word");
    expect(state,
           !cpu->io.crc.active && !interrupt_flag(cpu) &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) == 0u,
           "empty-select completion does not reinterrupt");

    configure(cpu, 8u, 16u, 0x1021u, false, true);
    enqueue(cpu, 8u, 0x12u);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "refill first pop");
    clear_interrupt(cpu);
    enqueue(cpu, 8u, 0x34u);
    expect(state, valid_words(cpu) == 1u && cpu->io.crc.active && cpu->events.count == 1u,
           "refill joins active calculation");
    expect(state, dspic33_device_advance(cpu, 4u), "refill word begins");
    expect(state, valid_words(cpu) == 0u && cpu->io.crc.active && interrupt_flag(cpu),
           "refill empty transition reinterrupts");
    clear_interrupt(cpu);
    expect(state, dspic33_device_advance(cpu, 3u), "refill word completes");
    expect(state, !cpu->io.crc.active && !interrupt_flag(cpu),
           "refill completion follows empty-select mode");
}

static void lifecycle_cases(TestState* state, Dspic33* cpu) {
    uint32_t partial;
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    start(cpu);
    expect(state,
           !cpu->io.crc.active && cpu->events.count == 0u &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) != 0u,
           "GO waits for FIFO data");
    enqueue(cpu, 8u, 0x5au);
    expect(state, cpu->io.crc.active && cpu->events.count == 1u, "FIFO write starts pending GO");
    expect(state, dspic33_device_advance(cpu, 4u), "pending GO completes");
    expect(state, shift_value(cpu) == 0xfbbfu, "pending GO result");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x12u);
    enqueue(cpu, 8u, 0x34u);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "abort begins first word");
    partial = shift_value(cpu);
    dspic33_write_word(cpu, CRC_CONTROL, CRC_ENABLE);
    expect(state,
           !cpu->io.crc.active && valid_words(cpu) == 1u &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) == 0u,
           "GO clear aborts current word and retains FIFO");
    expect(state, dspic33_device_advance(cpu, 8u), "aborted event expires");
    expect(state, shift_value(cpu) == partial && valid_words(cpu) == 1u && cpu->events.count == 0u,
           "stale CRC event cannot resume abort");
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 4u), "retained FIFO restarts");
    expect(state, valid_words(cpu) == 0u && !cpu->io.crc.active,
           "restarted CRC completes retained FIFO");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, CRC_SHIFT_LOW, 0xa5a5u);
    enqueue(cpu, 8u, 0x12u);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "seeded calculation begins");
    partial = shift_value(cpu);
    dspic33_write_word(cpu, CRC_SHIFT_LOW, 0xffffu);
    dspic33_write_word(cpu, CRC_SHIFT_HIGH, 0xffffu);
    expect(state, shift_value(cpu) == partial, "active CRCWDAT writes ignored");
    expect(state, dspic33_device_advance(cpu, 3u), "seeded calculation completes");

    configure(cpu, 8u, 16u, 0x1021u, true, true);
    enqueue(cpu, 8u, 0x12u);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "disable begins calculation");
    clear_interrupt(cpu);
    dspic33_write_word(cpu, CRC_CONTROL, 0u);
    expect(state,
           dspic33_read_word(cpu, CRC_CONTROL) == CRC_EMPTY && shift_value(cpu) == 0u &&
               valid_words(cpu) == 0u && !cpu->io.crc.active && cpu->io.crc.bits_remaining == 0u,
           "CRCEN clear flushes runtime state");
    expect(state,
           dspic33_read_word(cpu, CRC_CONFIG) == 0x070fu &&
               dspic33_read_word(cpu, CRC_POLYNOMIAL_LOW) == 0x1020u,
           "CRCEN clear preserves configuration");
    expect(state, dspic33_device_advance(cpu, 8u), "disabled stale event expires");
    expect(state, shift_value(cpu) == 0u && !interrupt_flag(cpu),
           "disabled stale event has no effect");
}

static void power_cases(TestState* state, Dspic33* cpu) {
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x5au);
    start(cpu);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state, dspic33_device_advance(cpu, 8u), "advance sleeping CRC");
    expect(state,
           cpu->io.crc.active && cpu->io.crc.count == 1u && cpu->io.crc.bits_remaining == 0u &&
               shift_value(cpu) == 0u && (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) != 0u,
           "Sleep suspends CRC state");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state, dspic33_device_advance(cpu, 4u), "resume sleeping CRC");
    expect(state, shift_value(cpu) == 0xfbbfu && !cpu->io.crc.active, "CRC resumes after Sleep");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x5au);
    start(cpu);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, dspic33_device_advance(cpu, 4u), "advance active Idle CRC");
    expect(state, shift_value(cpu) == 0xfbbfu && !cpu->io.crc.active,
           "CSIDL clear keeps CRC active in Idle");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, CRC_CONTROL, (uint16_t)(CRC_ENABLE | CRC_STOP_IDLE));
    enqueue(cpu, 8u, 0x5au);
    start(cpu);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, dspic33_device_advance(cpu, 8u), "advance stopped Idle CRC");
    expect(state,
           cpu->io.crc.active && cpu->io.crc.count == 1u && cpu->io.crc.bits_remaining == 0u &&
               shift_value(cpu) == 0u,
           "CSIDL set suspends CRC in Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state, dspic33_device_advance(cpu, 4u), "resume stopped Idle CRC");
    expect(state, shift_value(cpu) == 0xfbbfu && !cpu->io.crc.active,
           "CRC resumes after stopped Idle");

    configure(cpu, 8u, 16u, 0x1021u, false, true);
    dspic33_write_word(cpu, CRC_CONTROL,
                       (uint16_t)(CRC_ENABLE | CRC_STOP_IDLE | CRC_INTERRUPT_EMPTY));
    enqueue(cpu, 8u, 0x5au);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "raise CRC empty interrupt");
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, dspic33_device_advance(cpu, 8u), "hold interrupted CRC in Idle");
    expect(state, interrupt_flag(cpu) && cpu->io.crc.active && cpu->io.crc.bits_remaining == 6u,
           "pending CRC interrupt passes while clocks stop");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
}

static void pmd_cases(TestState* state, Dspic33* cpu) {
    uint8_t bits_remaining;
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x5au);
    start(cpu);
    dspic33_write_word(cpu, CRC_PMD_ADDRESS, CRC_PMD);
    expect(state, !cpu->io.crc.pmd_disabled, "CRC PMD disable is delayed");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.crc.pmd_disabled,
           "CRC PMD disable applies after one cycle");
    bits_remaining = cpu->io.crc.bits_remaining;
    expect(state,
           cpu->io.crc.active && bits_remaining == 6u && dspic33_read_word(cpu, CRC_CONTROL) == 0u,
           "CRC PMD hides registers after current cycle");
    dspic33_write_word(cpu, CRC_CONFIG, 0xffffu);
    expect(state, dspic33_device_advance(cpu, 8u), "advance PMD-disabled CRC");
    expect(state, cpu->io.crc.active && cpu->io.crc.bits_remaining == bits_remaining,
           "CRC PMD suspends shift state");
    dspic33_write_word(cpu, CRC_PMD_ADDRESS, 0u);
    expect(state, cpu->io.crc.pmd_disabled, "CRC PMD enable is delayed");
    expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.crc.pmd_disabled,
           "CRC PMD enable applies after one cycle");
    expect(state, dspic33_read_word(cpu, CRC_CONFIG) == 0x070fu,
           "CRC PMD ignores register writes and preserves configuration");
    expect(state, shift_value(cpu) == 0x1021u, "CRC PMD preserves partial remainder");
    expect(state, dspic33_device_advance(cpu, 3u), "finish PMD-resumed CRC");
    expect(state, shift_value(cpu) == 0xfbbfu && !cpu->io.crc.active,
           "CRC completes after PMD resume");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, CRC_PMD_ADDRESS, CRC_PMD);
    dspic33_write_word(cpu, CRC_PMD_ADDRESS, 0u);
    expect(state, cpu->io.crc.pmd_generation == 2u && cpu->events.count == 2u,
           "rapid CRC PMD toggle queues generations");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.crc.pmd_disabled &&
               (dspic33_read_word(cpu, CRC_PMD_ADDRESS) & CRC_PMD) == 0u,
           "stale CRC PMD event cannot override latest state");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, CRC_PMD_ADDRESS, CRC_PMD);
    expect(state,
           (dspic33_read_word(cpu, CRC_PMD_ADDRESS) & CRC_PMD) == 0u &&
               cpu->io.crc.pmd_generation == 2u && !cpu->io.crc.pmd_disabled &&
               cpu->events.count == 0u,
           "failed CRC PMD transition rolls back and invalidates generation");
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "failed CRC PMD transition reports queue error");
}

static void interrupt_service_cases(TestState* state, Dspic33* cpu) {
    uint16_t priority_address = (uint16_t)(0x0840u + (CRC_IRQ / 4u) * 2u);
    uint16_t priority_shift = (uint16_t)((CRC_IRQ % 4u) * 4u);
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, 0x0828u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0828u) | CRC_INTERRUPT_FLAG));
    dspic33_write_word(
        cpu, priority_address,
        (uint16_t)((dspic33_read_word(cpu, priority_address) & ~(7u << priority_shift)) |
                   (CRC_PRIORITY << priority_shift)));
    cpu->program[(0x0014u + CRC_IRQ * 2u) / 2u] = CRC_VECTOR;
    cpu->w[15] = 0x1800u;
    enqueue(cpu, 8u, 0x5au);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 4u), "interrupt vector CRC completes");
    expect(state, dspic33_device_interrupt_pending(cpu), "CRC interrupt pending");
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == CRC_IRQ &&
               cpu->pc == CRC_VECTOR,
           "CRC interrupt 67 vectors");
    expect(state,
           dspic33_read_word(cpu, 0x08c8u) == (uint16_t)((CRC_PRIORITY << 8u) | (CRC_IRQ + 8u)),
           "CRC interrupt status identifies vector");
}

static void no_dma_cases(TestState* state, Dspic33* cpu) {
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, 0x2200u, 0x5aa5u);
    dspic33_write_word(cpu, 0x0b00u, 0u);
    dspic33_write_word(cpu, 0x0b02u, CRC_IRQ);
    dspic33_write_word(cpu, 0x0b04u, 0x2200u);
    dspic33_write_word(cpu, 0x0b06u, 0u);
    dspic33_write_word(cpu, 0x0b0cu, CRC_SHIFT_LOW);
    dspic33_write_word(cpu, 0x0b0eu, 0u);
    dspic33_write_word(cpu, 0x0b00u, 0xa001u);
    enqueue(cpu, 8u, 0x5au);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 4u), "CRC with DMA channel completes");
    expect(state, interrupt_flag(cpu), "CRC completion still raises interrupt");
    expect(state,
           dspic33_read_word(cpu, 0x2200u) == 0x5aa5u && cpu->io.dma_index[0] == 0u &&
               cpu->io.dma_active == 0u && cpu->io.dma_peripheral_pending == 0u,
           "CRC interrupt makes no DMA request");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u,
           "no CRC DMA request leaves one-shot channel enabled");
}

static void copy_reset_failure_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize CRC copy");
    if (!initialized) {
        return;
    }
    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x12u);
    enqueue(cpu, 8u, 0x34u);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "copy source begins CRC");
    expect(state, dspic33_copy(&copy, cpu), "copy active CRC");
    expect(state,
           copy.io.crc.active && copy.io.crc.bits_remaining == 6u && copy.io.crc.count == 1u &&
               copy.events.count == 1u,
           "copy retains active CRC state");
    expect(state, copy.events.items != cpu->events.items, "copy CRC event storage independent");
    expect(state, dspic33_device_advance(cpu, 7u) && dspic33_device_advance(&copy, 7u),
           "source and copy complete CRC");
    expect(state,
           shift_value(&copy) == shift_value(cpu) &&
               dspic33_read_word(&copy, CRC_CONTROL) == dspic33_read_word(cpu, CRC_CONTROL),
           "copied CRC result matches source");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    dspic33_write_word(cpu, CRC_PMD_ADDRESS, CRC_PMD);
    expect(state, dspic33_copy(&copy, cpu), "copy pending CRC PMD transition");
    expect(state,
           copy.io.crc.pmd_generation == 1u && !copy.io.crc.pmd_disabled &&
               copy.events.count == 1u && copy.events.items != cpu->events.items,
           "copy retains independent pending CRC PMD state");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u) &&
               cpu->io.crc.pmd_disabled && copy.io.crc.pmd_disabled,
           "copied CRC PMD transitions complete equally");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x12u);
    start(cpu);
    expect(state, dspic33_device_advance(cpu, 1u), "reset begins CRC");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_read_word(cpu, CRC_CONTROL) == CRC_EMPTY && !cpu->io.crc.active &&
               cpu->io.crc.count == 0u && !cpu->io.crc.pmd_disabled &&
               cpu->io.crc.pmd_generation == 0u && cpu->events.count == 0u,
           "POR aborts CRC and clears events");

    configure(cpu, 8u, 16u, 0x1021u, false, false);
    enqueue(cpu, 8u, 0x12u);
    cpu->device_cycles = UINT64_MAX;
    start(cpu);
    expect(state,
           !cpu->io.crc.active && cpu->events.count == 0u && valid_words(cpu) == 1u &&
               (dspic33_read_word(cpu, CRC_CONTROL) & CRC_GO) == 0u,
           "CRC event overflow aborts without consuming FIFO");
    dspic33_release(&copy);
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize CRC processor");
    if (initialized) {
        reset_and_access_cases(&state, &cpu);
        fifo_cases(&state, &cpu);
        empty_active_case(&state, &cpu);
        lane_cases(&state, &cpu);
        known_vector_cases(&state, &cpu);
        width_matrix_cases(&state, &cpu);
        interrupt_timing_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        power_cases(&state, &cpu);
        pmd_cases(&state, &cpu);
        interrupt_service_cases(&state, &cpu);
        no_dma_cases(&state, &cpu);
        copy_reset_failure_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
