#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} CanConformance;

static const uint16_t bases[DSPIC33_CAN_COUNT] = {0x0400u, 0x0500u};
static const uint8_t event_irqs[DSPIC33_CAN_COUNT] = {35u, 56u};
static const uint8_t receive_requests[DSPIC33_CAN_COUNT] = {34u, 55u};
static const uint8_t transmit_requests[DSPIC33_CAN_COUNT] = {70u, 71u};

static void expect(CanConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[can-failed] %s\n", name);
    }
}

static bool interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static uint16_t dma_base(uint8_t channel) {
    return (uint16_t)(0x0b00u + channel * 0x10u);
}

static void configure_dma(Dspic33* cpu, uint8_t channel, uint16_t control,
                          uint8_t request, uint32_t memory, uint16_t pad) {
    uint16_t base = dma_base(channel);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), (uint16_t)memory);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(memory >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), 7u);
    dspic33_write_word(cpu, base, (uint16_t)(0x8000u | control));
}

static void select_window(Dspic33* cpu, uint8_t channel, bool filter) {
    uint16_t base = bases[channel];
    uint16_t control = dspic33_read_word(cpu, base);
    dspic33_write_word(cpu, base,
                       filter ? (uint16_t)(control | 1u) : (uint16_t)(control & ~1u));
}

static void set_mode(Dspic33* cpu, uint8_t channel, uint8_t mode) {
    uint16_t base = bases[channel];
    uint16_t control = dspic33_read_word(cpu, base);
    control = (uint16_t)((control & ~0x0700u) | ((uint16_t)mode << 8u));
    dspic33_write_word(cpu, base, control);
}

static void configure_filter(Dspic33* cpu, uint8_t channel, uint8_t filter,
                             uint32_t identifier, bool extended, uint32_t mask,
                             bool match_type, uint8_t buffer, uint8_t mask_index) {
    uint16_t base = bases[channel];
    uint32_t sid = extended ? (identifier >> 18u) & 0x7ffu : identifier & 0x7ffu;
    uint32_t eid = identifier & 0x3ffffu;
    uint32_t mask_sid = extended ? (mask >> 18u) & 0x7ffu : mask & 0x7ffu;
    uint32_t mask_eid = mask & 0x3ffffu;
    uint16_t filter_sid = (uint16_t)((sid << 5u) | ((eid >> 16u) & 3u));
    uint16_t receive_mask = (uint16_t)((mask_sid << 5u) | ((mask_eid >> 16u) & 3u));
    uint16_t address;
    uint16_t value;
    if (extended) {
        filter_sid |= 8u;
    }
    if (match_type) {
        receive_mask |= 8u;
    }
    select_window(cpu, channel, true);
    dspic33_write_word(cpu, (uint16_t)(base + 0x30u + mask_index * 4u), receive_mask);
    dspic33_write_word(cpu, (uint16_t)(base + 0x32u + mask_index * 4u),
                       (uint16_t)mask_eid);
    dspic33_write_word(cpu, (uint16_t)(base + 0x40u + filter * 4u), filter_sid);
    dspic33_write_word(cpu, (uint16_t)(base + 0x42u + filter * 4u), (uint16_t)eid);
    address = (uint16_t)(base + (filter < 8u ? 0x18u : 0x1au));
    value = dspic33_read_word(cpu, address);
    value = (uint16_t)((value & ~(uint16_t)(3u << ((filter & 7u) * 2u))) |
                       ((uint16_t)mask_index << ((filter & 7u) * 2u)));
    dspic33_write_word(cpu, address, value);
    address = (uint16_t)(base + 0x20u + (filter / 4u) * 2u);
    value = dspic33_read_word(cpu, address);
    value = (uint16_t)((value & ~(uint16_t)(0x0fu << ((filter & 3u) * 4u))) |
                       ((uint16_t)buffer << ((filter & 3u) * 4u)));
    dspic33_write_word(cpu, address, value);
}

static void enable_filter(Dspic33* cpu, uint8_t channel, uint16_t enabled) {
    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 0x14u), enabled);
}

static void configure_receive(Dspic33* cpu, uint8_t channel, uint32_t memory,
                              uint8_t dmabs, uint8_t fifo_start) {
    uint16_t base = bases[channel];
    configure_dma(cpu, (uint8_t)(channel * 2u), 0x0020u, receive_requests[channel],
                  memory, (uint16_t)(base + 0x40u));
    dspic33_write_word(cpu, (uint16_t)(base + 6u),
                       (uint16_t)(((uint16_t)dmabs << 13u) | fifo_start));
}

static void configure_transmit(Dspic33* cpu, uint8_t channel, uint32_t memory) {
    configure_dma(cpu, (uint8_t)(channel * 2u + 1u), 0x2020u,
                  transmit_requests[channel], memory,
                  (uint16_t)(bases[channel] + 0x42u));
}

static Dspic33CanFrame frame(uint32_t identifier, bool extended, bool remote,
                             uint8_t length, uint8_t seed) {
    Dspic33CanFrame result;
    uint8_t index;
    memset(&result, 0, sizeof(result));
    result.identifier = identifier;
    result.extended = extended;
    result.remote = remote;
    result.length = length;
    for (index = 0u; index < length; index++) {
        result.data[index] = (uint8_t)(seed + index * 17u);
    }
    return result;
}

static void clear_receive_flag(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t address = (uint16_t)(bases[channel] + 0x20u + (buffer >= 16u ? 2u : 0u));
    uint16_t value = dspic33_read_word(cpu, address);
    dspic33_write_word(cpu, address,
                       (uint16_t)(value & ~(uint16_t)(1u << (buffer & 15u))));
}

static bool receive_full(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t address = (uint16_t)(bases[channel] + 0x20u + (buffer >= 16u ? 2u : 0u));
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (buffer & 15u))) != 0u;
}

static uint16_t memory_word(Dspic33* cpu, uint32_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static void write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
}

static void register_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        expect(state, dspic33_read_word(cpu, base) == 0x0480u, "control reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0x0040u,
               "vector reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x14u)) == 0x003fu,
               "filter enable reset");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u,
               "control two mask");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0xe01fu,
               "fifo control mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0x00efu,
               "interrupt enable mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x10u)) == 0x00ffu,
               "baud one mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x12u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) == 0x47ffu,
               "baud two mask");
        select_window(cpu, channel, true);
        dspic33_write_word(cpu, (uint16_t)(base + 0x40u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x40u)) == 0xffebu,
               "filter SID mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x42u), 0xa55au);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x42u)) == 0xa55au,
               "filter EID write");
        select_window(cpu, channel, false);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x40u)) == 0u,
               "window isolation");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) == 0x8f8fu,
               "buffer control mask");
        set_mode(cpu, channel, 0u);
        expect(state, (dspic33_read_word(cpu, base) & 0x00e0u) == 0u,
               "normal mode acknowledgement");
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x10u)) == 0x00ffu,
               "baud locked outside configuration");
        set_mode(cpu, channel, 4u);
    }
}

static void standard_filter_domain(CanConformance* state, Dspic33* cpu) {
    uint16_t identifier;
    uint16_t expected = 0x5a0u;
    uint16_t mask = 0x7f0u;
    Dspic33CanFrame input;
    dspic33_reset(cpu, 0u);
    configure_receive(cpu, 0u, 0x2000u, 4u, 0u);
    configure_filter(cpu, 0u, 0u, expected, false, mask, true, 0u, 0u);
    enable_filter(cpu, 0u, 1u);
    select_window(cpu, 0u, false);
    set_mode(cpu, 0u, 0u);
    for (identifier = 0u; identifier <= 0x7ffu; identifier++) {
        bool accepted = (identifier & mask) == (expected & mask);
        input = frame(identifier, false, false, 2u, (uint8_t)identifier);
        expect(state, dspic33_can_receive(cpu, 0u, &input, 0u),
               "standard domain schedule");
        expect(state, dspic33_device_advance(cpu, 32u), "standard domain advance");
        expect(state, receive_full(cpu, 0u, 0u) == accepted,
               "standard domain filter result");
        if (accepted) {
            expect(state, memory_word(cpu, 0x2000u) == (uint16_t)(identifier << 2u),
                   "standard domain identifier storage");
            clear_receive_flag(cpu, 0u, 0u);
        }
    }
}

static void extended_filter_cases(CanConformance* state, Dspic33* cpu) {
    static const uint32_t identifiers[] = {
        0u, 1u, 0x3ffffu, 0x40000u, 0x1ffffffu, 0x10000000u, 0x15555555u, 0x1fffffffu};
    uint8_t channel;
    uint8_t index;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (index = 0u; index < sizeof(identifiers) / sizeof(identifiers[0]);
             index++) {
            uint32_t identifier = identifiers[index];
            Dspic33CanFrame input = frame(identifier, true, false, 8u, index);
            dspic33_reset(cpu, 0u);
            configure_receive(cpu, channel, 0x3000u, 6u, 0u);
            configure_filter(cpu, channel, 3u, identifier, true, 0x1fffffffu, true, 6u,
                             1u);
            enable_filter(cpu, channel, 1u << 3u);
            select_window(cpu, channel, false);
            set_mode(cpu, channel, 0u);
            expect(state,
                   dspic33_can_receive(cpu, channel, &input, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "extended exact transfer");
            expect(state, receive_full(cpu, channel, 6u), "extended exact full flag");
            expect(state,
                   (((uint32_t)(memory_word(cpu, 0x3060u) >> 2u) & 0x7ffu) << 18u) ==
                       (identifier & 0x1ffc0000u),
                   "extended SID storage");
            expect(state,
                   (((uint32_t)(memory_word(cpu, 0x3062u) & 0x0fffu) << 6u) |
                    (memory_word(cpu, 0x3064u) >> 10u)) == (identifier & 0x3ffffu),
                   "extended EID storage");
        }
    }
}

static void payload_and_remote_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t length;
    uint8_t extended;
    uint8_t remote;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (extended = 0u; extended < 2u; extended++) {
            for (remote = 0u; remote < 2u; remote++) {
                for (length = 0u; length <= 8u; length++) {
                    uint32_t identifier = extended != 0u ? 0x1234567u : 0x567u;
                    Dspic33CanFrame input =
                        frame(identifier, extended != 0u, remote != 0u, length,
                              (uint8_t)(0x20u + length));
                    uint8_t index;
                    dspic33_reset(cpu, 0u);
                    configure_receive(cpu, channel, 0x4000u, 4u, 0u);
                    configure_filter(cpu, channel, 0u, identifier, extended != 0u,
                                     extended != 0u ? 0x1fffffffu : 0x7ffu, true, 2u,
                                     0u);
                    enable_filter(cpu, channel, 1u);
                    select_window(cpu, channel, false);
                    set_mode(cpu, channel, 0u);
                    expect(state,
                           dspic33_can_receive(cpu, channel, &input, 0u) &&
                               dspic33_device_advance(cpu, 32u),
                           "payload transfer");
                    expect(state, (memory_word(cpu, 0x4024u) & 0x0fu) == length,
                           "payload length");
                    for (index = 0u; index < length; index++) {
                        uint16_t word =
                            memory_word(cpu, (uint32_t)(0x4026u + (index / 2u) * 2u));
                        expect(state,
                               (uint8_t)(word >> ((index & 1u) * 8u)) ==
                                   input.data[index],
                               "payload byte");
                    }
                    expect(state,
                           extended != 0u ? ((memory_word(cpu, 0x4024u) & 0x0200u) !=
                                             0u) == (remote != 0u)
                                          : ((memory_word(cpu, 0x4020u) & 2u) != 0u) ==
                                                (remote != 0u),
                           "remote encoding");
                }
            }
        }
    }
}

static void devicenet_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t bits;
    uint8_t length;
    for (bits = 1u; bits <= 31u; bits++) {
        for (length = 0u; length <= 3u; length++) {
            Dspic33CanFrame input = frame(0x321u, false, false, length, 0xa5u);
            uint32_t data = 0u;
            uint8_t compared = bits > 18u ? 18u : bits;
            if (compared > length * 8u) {
                compared = (uint8_t)(length * 8u);
            }
            if (length > 0u) {
                data = (uint32_t)input.data[0] << 16u;
            }
            if (length > 1u) {
                data |= (uint32_t)input.data[1] << 8u;
            }
            if (length > 2u) {
                data |= input.data[2];
            }
            data = compared == 0u ? 0u : (data >> (24u - compared)) << (18u - compared);
            dspic33_reset(cpu, 0u);
            configure_receive(cpu, 0u, 0x5000u, 4u, 0u);
            configure_filter(cpu, 0u, 0u, 0x321u, false, 0x7ffu, true, 0u, 0u);
            select_window(cpu, 0u, true);
            dspic33_write_word(cpu, 0x0430u, (uint16_t)((0x7ffu << 5u) | 8u));
            dspic33_write_word(cpu, 0x0440u,
                               (uint16_t)((0x321u << 5u) | (data >> 16u)));
            dspic33_write_word(cpu, 0x0442u, (uint16_t)data);
            enable_filter(cpu, 0u, 1u);
            dspic33_write_word(cpu, 0x0402u, bits);
            select_window(cpu, 0u, false);
            set_mode(cpu, 0u, 0u);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &input, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "DeviceNet matching transfer");
            expect(state, receive_full(cpu, 0u, 0u), "DeviceNet match");
        }
    }
}

static void direct_buffer_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t buffer;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (buffer = 0u; buffer <= 14u; buffer++) {
            Dspic33CanFrame input =
                frame((uint32_t)(0x200u + buffer), false, false, 4u, buffer);
            dspic33_reset(cpu, 0u);
            configure_receive(cpu, channel, 0x6000u, 6u, 0u);
            configure_filter(cpu, channel, 0u, input.identifier, false, 0x7ffu, true,
                             buffer, 0u);
            enable_filter(cpu, channel, 1u);
            select_window(cpu, channel, false);
            set_mode(cpu, channel, 0u);
            expect(state,
                   dspic33_can_receive(cpu, channel, &input, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "direct buffer transfer");
            expect(state, receive_full(cpu, channel, buffer), "direct buffer full");
            expect(state,
                   memory_word(cpu, (uint32_t)(0x6000u + buffer * 16u)) ==
                       (uint16_t)(input.identifier << 2u),
                   "direct buffer address");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) & 2u) !=
                       0u,
                   "direct receive event");
        }
    }
}

static void fifo_cases(CanConformance* state, Dspic33* cpu) {
    static const uint8_t sizes[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u};
    uint8_t selection;
    for (selection = 0u; selection < sizeof(sizes); selection++) {
        uint8_t size = sizes[selection];
        uint8_t start = (uint8_t)(size / 2u);
        uint8_t count = (uint8_t)(size - start);
        uint8_t index;
        dspic33_reset(cpu, 0u);
        configure_receive(cpu, 0u, 0x7000u, selection, start);
        configure_filter(cpu, 0u, 0u, 0x456u, false, 0x7ffu, true, 15u, 0u);
        enable_filter(cpu, 0u, 1u);
        select_window(cpu, 0u, false);
        set_mode(cpu, 0u, 0u);
        for (index = 0u; index < count; index++) {
            Dspic33CanFrame input = frame(0x456u, false, false, 1u, index);
            uint8_t buffer = (uint8_t)(start + index);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &input, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "FIFO transfer");
            expect(state, receive_full(cpu, 0u, buffer), "FIFO full sequence");
            expect(state,
                   memory_word(cpu, (uint32_t)(0x7000u + buffer * 16u + 6u)) == index,
                   "FIFO payload sequence");
        }
        expect(state, ((dspic33_read_word(cpu, 0x0408u) >> 8u) & 0x3fu) == start,
               "FIFO write pointer wrap");
        {
            Dspic33CanFrame overflow = frame(0x456u, false, false, 1u, 0xeeu);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &overflow, 0u) &&
                       dspic33_device_advance(cpu, 2u),
                   "FIFO overflow transfer attempt");
            expect(state,
                   (dspic33_read_word(cpu,
                                      (uint16_t)(0x0428u + (start >= 16u ? 2u : 0u))) &
                    (uint16_t)(1u << (start & 15u))) != 0u,
                   "FIFO overflow flag");
            expect(state, (dspic33_read_word(cpu, 0x040au) & 4u) != 0u,
                   "FIFO overflow interrupt flag");
        }
        clear_receive_flag(cpu, 0u, start);
        expect(state, (dspic33_read_word(cpu, 0x0408u) & 0x3fu) == start + 1u,
               "FIFO next read pointer");
    }
}

static void overflow_and_fallback_cases(CanConformance* state, Dspic33* cpu) {
    Dspic33CanFrame input = frame(0x123u, false, false, 2u, 0x40u);
    dspic33_reset(cpu, 0u);
    configure_receive(cpu, 0u, 0x8000u, 4u, 0u);
    configure_filter(cpu, 0u, 0u, 0x123u, false, 0x7ffu, true, 1u, 0u);
    configure_filter(cpu, 0u, 1u, 0x123u, false, 0x7ffu, true, 2u, 0u);
    enable_filter(cpu, 0u, 3u);
    select_window(cpu, 0u, false);
    set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
           "fallback first transfer");
    expect(state, receive_full(cpu, 0u, 1u), "fallback first buffer");
    expect(state,
           dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
           "fallback second transfer");
    expect(state, receive_full(cpu, 0u, 2u), "fallback second buffer");
    expect(state,
           dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 2u),
           "fallback overflow attempt");
    expect(state, (dspic33_read_word(cpu, 0x0428u) & 2u) != 0u,
           "fallback lowest overflow");
}

static void transmission_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t extended;
    uint8_t remote;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (extended = 0u; extended < 2u; extended++) {
            for (remote = 0u; remote < 2u; remote++) {
                uint16_t base = bases[channel];
                uint32_t memory = (uint32_t)(0x9000u + channel * 0x1000u);
                Dspic33CanFrame expected =
                    frame(extended != 0u ? 0x1234567u : 0x345u, extended != 0u,
                          remote != 0u, 8u, (uint8_t)(0x50u + extended * 8u));
                Dspic33CanFrame actual;
                uint16_t words[8] = {0};
                uint8_t index;
                uint32_t sid = expected.extended ? (expected.identifier >> 18u) & 0x7ffu
                                                 : expected.identifier;
                uint32_t eid = expected.identifier & 0x3ffffu;
                words[0] = (uint16_t)(sid << 2u);
                if (expected.extended) {
                    words[0] |= 3u;
                    words[1] = (uint16_t)(eid >> 6u);
                    words[2] = (uint16_t)((eid & 0x3fu) << 10u);
                    if (expected.remote) {
                        words[2] |= 0x0200u;
                    }
                } else if (expected.remote) {
                    words[0] |= 2u;
                }
                words[2] |= expected.length;
                for (index = 0u; index < expected.length; index++) {
                    words[3u + index / 2u] |= (uint16_t)expected.data[index]
                                              << ((index & 1u) * 8u);
                }
                dspic33_reset(cpu, 0u);
                configure_transmit(cpu, channel, memory);
                for (index = 0u; index < 8u; index++) {
                    write_memory_word(cpu, memory + index * 2u, words[index]);
                }
                select_window(cpu, channel, false);
                set_mode(cpu, channel, 0u);
                dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 1u);
                dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
                expect(state, dspic33_device_advance(cpu, 32u), "transmit advance");
                expect(state, dspic33_can_transmit(cpu, channel, &actual),
                       "transmit queue output");
                expect(state,
                       actual.identifier == expected.identifier &&
                           actual.extended == expected.extended &&
                           actual.remote == expected.remote &&
                           actual.length == expected.length,
                       "transmit frame header");
                expect(state, memcmp(actual.data, expected.data, 8u) == 0,
                       "transmit payload");
                expect(state,
                       (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 8u) == 0u,
                       "transmit request clear");
                expect(state,
                       (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 1u) != 0u,
                       "transmit event flag");
                expect(state, interrupt_flag(cpu, event_irqs[channel]),
                       "transmit event interrupt");
            }
        }
    }
}

static void priority_and_abort_cases(CanConformance* state, Dspic33* cpu) {
    Dspic33CanFrame output;
    uint8_t buffer;
    dspic33_reset(cpu, 0u);
    configure_transmit(cpu, 0u, 0xb000u);
    for (buffer = 0u; buffer < 8u; buffer++) {
        write_memory_word(cpu, (uint32_t)(0xb000u + buffer * 16u),
                          (uint16_t)((0x100u + buffer) << 2u));
        write_memory_word(cpu, (uint32_t)(0xb004u + buffer * 16u), 0u);
    }
    set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x8988u);
    dspic33_write_word(cpu, 0x0432u, 0x8a8bu);
    dspic33_write_word(cpu, 0x0434u, 0x8a89u);
    dspic33_write_word(cpu, 0x0436u, 0x8b8bu);
    expect(state, dspic33_device_advance(cpu, 80u), "priority transmissions");
    expect(state, dspic33_can_transmit(cpu, 0u, &output) && output.identifier == 0x107u,
           "highest priority natural order");
    expect(state, dspic33_can_transmit(cpu, 0u, &output) && output.identifier == 0x106u,
           "second highest natural order");
    dspic33_reset(cpu, 0u);
    select_window(cpu, 0u, false);
    dspic33_write_word(cpu, 0x0430u, 0x8989u);
    dspic33_write_word(cpu, 0x0400u,
                       (uint16_t)(dspic33_read_word(cpu, 0x0400u) | 0x1000u));
    expect(state, (dspic33_read_word(cpu, 0x0430u) & 0x4848u) == 0x4040u,
           "abort flags and request clear");
    expect(state, (dspic33_read_word(cpu, 0x0400u) & 0x1000u) == 0u,
           "abort all self clear");
}

static void mode_and_power_cases(CanConformance* state, Dspic33* cpu) {
    static const uint8_t modes[] = {0u, 1u, 2u, 3u, 4u, 7u};
    uint8_t index;
    for (index = 0u; index < sizeof(modes); index++) {
        uint8_t mode = modes[index];
        Dspic33CanFrame input = frame(0x234u, false, false, 1u, mode);
        dspic33_reset(cpu, 0u);
        configure_receive(cpu, 0u, 0xc000u, 4u, 0u);
        configure_filter(cpu, 0u, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
        enable_filter(cpu, 0u, mode == 7u ? 0u : 1u);
        select_window(cpu, 0u, false);
        set_mode(cpu, 0u, mode);
        expect(state, ((dspic33_read_word(cpu, 0x0400u) >> 5u) & 7u) == mode,
               "mode acknowledgement matrix");
        expect(state,
               dspic33_can_receive(cpu, 0u, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "mode receive schedule");
        expect(state,
               receive_full(cpu, 0u, 0u) ==
                   (mode == 0u || mode == 2u || mode == 3u || mode == 7u),
               "mode receive behavior");
    }
    dspic33_reset(cpu, 0u);
    configure_receive(cpu, 0u, 0xc000u, 4u, 0u);
    configure_filter(cpu, 0u, 0u, 0x234u, false, 0x7ffu, true, 0u, 0u);
    enable_filter(cpu, 0u, 1u);
    select_window(cpu, 0u, false);
    set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0760u, (uint16_t)(dspic33_read_word(cpu, 0x0760u) | 2u));
    {
        Dspic33CanFrame input = frame(0x234u, false, false, 1u, 0u);
        expect(state,
               dspic33_can_receive(cpu, 0u, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "PMD receive schedule");
        expect(state, !receive_full(cpu, 0u, 0u), "PMD blocks receive");
    }
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x040cu, 0x0040u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    {
        Dspic33CanFrame input = frame(0x234u, false, false, 1u, 0u);
        expect(state,
               dspic33_can_receive(cpu, 0u, &input, 0u) &&
                   dspic33_device_advance(cpu, 1u),
               "sleep wake schedule");
        expect(state, (dspic33_read_word(cpu, 0x040au) & 0x0040u) != 0u,
               "sleep wake flag");
    }
}

static void interrupt_and_error_cases(CanConformance* state, Dspic33* cpu) {
    static const uint8_t increments[] = {1u, 95u, 1u, 31u, 1u, 126u, 128u};
    uint8_t index;
    dspic33_reset(cpu, 0u);
    set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x040cu, 0x0020u);
    for (index = 0u; index < sizeof(increments); index++) {
        bool transmit = index >= 4u;
        expect(state,
               dspic33_can_error(cpu, 0u, transmit, increments[index], 0u) &&
                   dspic33_device_advance(cpu, 1u),
               "error event schedule");
        expect(state, (dspic33_read_word(cpu, 0x040au) & 0x0020u) != 0u,
               "error event flag");
        expect(state, (dspic33_read_word(cpu, 0x0404u) & 0x007fu) == 0x41u,
               "error vector code");
        expect(state, interrupt_flag(cpu, event_irqs[0]), "error event interrupt");
        dspic33_write_word(cpu, 0x040au,
                           (uint16_t)(dspic33_read_word(cpu, 0x040au) & ~0x0020u));
    }
    expect(state, (dspic33_read_word(cpu, 0x040eu) & 0x00ffu) == 128u,
           "receive error count");
    expect(state, (dspic33_read_word(cpu, 0x040au) & 0x0b00u) == 0x0b00u,
           "receive passive state");
    expect(state, (dspic33_read_word(cpu, 0x040eu) >> 8u) == 0xffu,
           "transmit error saturation");
    expect(state, (dspic33_read_word(cpu, 0x040au) & 0x3500u) == 0x3500u,
           "transmit bus off state");
    set_mode(cpu, 0u, 4u);
    expect(state, dspic33_read_word(cpu, 0x040eu) == 0u,
           "configuration clears error counts");
}

static void copy_and_reset_cases(CanConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    Dspic33CanFrame input = frame(0x456u, false, false, 3u, 0x70u);
    expect(state, initialized, "initialize CAN copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_receive(cpu, 0u, 0xd000u, 4u, 0u);
    configure_filter(cpu, 0u, 0u, 0x456u, false, 0x7ffu, true, 0u, 0u);
    enable_filter(cpu, 0u, 1u);
    select_window(cpu, 0u, false);
    set_mode(cpu, 0u, 0u);
    expect(state, dspic33_can_receive(cpu, 0u, &input, 2u),
           "copy pending receive schedule");
    expect(state, dspic33_copy(&copy, cpu), "copy pending CAN state");
    expect(state,
           dspic33_device_advance(cpu, 32u) && dspic33_device_advance(&copy, 32u),
           "copy advance");
    expect(state, receive_full(cpu, 0u, 0u) && receive_full(&copy, 0u, 0u),
           "copy receives identically");
    expect(state, memory_word(cpu, 0xd000u) == memory_word(&copy, 0xd000u),
           "copy DMA contents");
    dspic33_destroy(&copy);
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.can_rx[0].count == 0u && cpu->io.can_tx[0].count == 0u &&
               cpu->io.can_rx_busy == 0u && cpu->io.can_tx_busy == 0u,
           "reset clears CAN runtime");
}

int main(void) {
    CanConformance state = {0u, 0u, 0u};
    Dspic33 cpu;
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "[can-error] cannot initialize emulator\n");
        return 2;
    }
    register_cases(&state, &cpu);
    standard_filter_domain(&state, &cpu);
    extended_filter_cases(&state, &cpu);
    payload_and_remote_cases(&state, &cpu);
    devicenet_cases(&state, &cpu);
    direct_buffer_cases(&state, &cpu);
    fifo_cases(&state, &cpu);
    overflow_and_fallback_cases(&state, &cpu);
    transmission_cases(&state, &cpu);
    priority_and_abort_cases(&state, &cpu);
    mode_and_power_cases(&state, &cpu);
    interrupt_and_error_cases(&state, &cpu);
    copy_and_reset_cases(&state, &cpu);
    printf("[can-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&cpu);
    return state.failed == 0u ? 0 : 1;
}
