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

enum {
    CAN_INTERRUPT_ERROR = 0x0020u,
    CAN_ERROR_WARNING = 0x0100u,
    CAN_RECEIVE_WARNING = 0x0200u,
    CAN_TRANSMIT_WARNING = 0x0400u,
    CAN_RECEIVE_PASSIVE = 0x0800u,
    CAN_TRANSMIT_PASSIVE = 0x1000u,
    CAN_BUS_OFF = 0x2000u,
    CAN_ERROR_STATUS_MASK = 0x3f00u,
    FIFO_RELATION_THRESHOLD = 0u,
    FIFO_RELATION_EQUAL = 1u,
    FIFO_RELATION_DISTANT = 2u
};

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

static void clear_interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t flag = (uint16_t)(1u << (irq % 16u));
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~flag));
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

static void interrupt_flag_write_zero_cases(CanConformance* state, Dspic33* cpu) {
    static const uint16_t previous_words[] = {0x00efu, 0x0000u, 0x00a5u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t address = (uint16_t)(bases[channel] + 0x0au);
        uint8_t previous_index;
        dspic33_reset(cpu, 0u);
        for (previous_index = 0u;
             previous_index < sizeof(previous_words) / sizeof(previous_words[0]);
             previous_index++) {
            uint16_t previous = previous_words[previous_index];
            uint32_t requested;
            for (requested = 0u; requested <= UINT16_MAX; requested++) {
                write_memory_word(cpu, address, previous);
                dspic33_write_word(cpu, address, (uint16_t)requested);
                expect(state,
                       dspic33_read_word(cpu, address) ==
                           (uint16_t)(previous & requested),
                       "interrupt flag write-zero clear domain");
            }
        }
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

static void select_transmit_buffer(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t address = (uint16_t)(bases[channel] + 0x30u + (buffer / 2u) * 2u);
    uint8_t shift = (uint8_t)((buffer & 1u) * 8u);
    uint16_t value = dspic33_read_word(cpu, address);
    value =
        (uint16_t)((value & ~(uint16_t)(0xffu << shift)) | (uint16_t)(0x80u << shift));
    dspic33_write_word(cpu, address, value);
}

static bool transmit_buffer_selected(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t address = (uint16_t)(bases[channel] + 0x30u + (buffer / 2u) * 2u);
    uint8_t shift = (uint8_t)((buffer & 1u) * 8u);
    return ((dspic33_read_word(cpu, address) >> shift) & 0x80u) != 0u;
}

static void fifo_overflow_advancement_case(CanConformance* state, Dspic33* cpu,
                                           uint8_t channel, bool listen_all, bool full,
                                           bool wrap) {
    uint16_t base = bases[channel];
    uint32_t memory = (uint32_t)(0xf000u + channel * 0x100u);
    Dspic33CanFrame input = frame(0x456u, false, false, 1u, 0x80u);
    uint8_t buffer = wrap ? 3u : 2u;
    uint8_t expected_next = wrap ? 2u : 3u;
    uint16_t flag = (uint16_t)(1u << buffer);
    uint16_t fnrb;
    dspic33_reset(cpu, 0u);
    configure_receive(cpu, channel, memory, 0u, 2u);
    configure_filter(cpu, channel, 0u, input.identifier, false, 0x7ffu, true, 15u, 0u);
    enable_filter(cpu, channel, 1u);
    select_window(cpu, channel, false);
    set_mode(cpu, channel, listen_all ? 7u : 0u);
    if (wrap) {
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "FIFO overflow wrap preparation");
        expect(state,
               ((dspic33_read_word(cpu, (uint16_t)(base + 8u)) >> 8u) & 0x3fu) ==
                   buffer,
               "FIFO overflow wrap pointer preparation");
        clear_receive_flag(cpu, channel, 2u);
    }
    if (full) {
        write_memory_word(cpu, (uint16_t)(base + 0x20u), flag);
    } else {
        select_transmit_buffer(cpu, channel, buffer);
    }
    write_memory_word(cpu, memory + buffer * 16u, 0xa55au);
    fnrb = (uint16_t)(dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x003fu);
    expect(state,
           dspic33_can_receive(cpu, channel, &input, 0u) &&
               dspic33_device_advance(cpu, 2u),
           "FIFO overflow receive attempt");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 0x28u)) & flag) != 0u,
           "FIFO overflow buffer flag");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 4u) != 0u,
           "FIFO overflow event flag");
    expect(state,
           ((dspic33_read_word(cpu, (uint16_t)(base + 8u)) >> 8u) & 0x3fu) ==
               expected_next,
           "FIFO overflow advances write pointer");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x003fu) == fnrb,
           "FIFO overflow preserves next read pointer");
    expect(state, memory_word(cpu, memory + buffer * 16u) == 0xa55au,
           "FIFO overflow loses message");
    expect(state, receive_full(cpu, channel, buffer) == full,
           "FIFO overflow preserves selected buffer state");
    expect(state, full || transmit_buffer_selected(cpu, channel, buffer),
           "FIFO overflow preserves transmit selection");
}

static void fifo_overflow_advancement_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t mode;
        for (mode = 0u; mode < 2u; mode++) {
            uint8_t cause;
            for (cause = 0u; cause < 2u; cause++) {
                uint8_t wrap;
                for (wrap = 0u; wrap < 2u; wrap++) {
                    fifo_overflow_advancement_case(state, cpu, channel, mode != 0u,
                                                   cause == 0u, wrap != 0u);
                }
            }
        }
    }
}

static void receive_flag_write_zero_domain(CanConformance* state, Dspic33* cpu) {
    static const uint8_t offsets[] = {0x20u, 0x22u, 0x28u, 0x2au};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t index;
        dspic33_reset(cpu, 0u);
        select_window(cpu, channel, false);
        for (index = 0u; index < sizeof(offsets); index++) {
            uint16_t address = (uint16_t)(bases[channel] + offsets[index]);
            uint32_t requested;
            for (requested = 0u; requested <= UINT16_MAX; requested++) {
                write_memory_word(cpu, address, UINT16_MAX);
                dspic33_write_word(cpu, address, (uint16_t)requested);
                expect(state, dspic33_read_word(cpu, address) == requested,
                       "receive flag write-zero domain");
            }
        }
    }
}

static void receive_overflow_write_zero_prior_domain(CanConformance* state,
                                                     Dspic33* cpu) {
    static const uint8_t offsets[] = {0x28u, 0x2au};
    static const uint16_t previous_words[] = {0x0000u, 0x5a5au};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t index;
        dspic33_reset(cpu, 0u);
        select_window(cpu, channel, false);
        for (index = 0u; index < sizeof(offsets); index++) {
            uint16_t address = (uint16_t)(bases[channel] + offsets[index]);
            uint8_t previous_index;
            for (previous_index = 0u;
                 previous_index < sizeof(previous_words) / sizeof(previous_words[0]);
                 previous_index++) {
                uint16_t previous = previous_words[previous_index];
                uint32_t requested;
                for (requested = 0u; requested <= UINT16_MAX; requested++) {
                    write_memory_word(cpu, address, previous);
                    dspic33_write_word(cpu, address, (uint16_t)requested);
                    expect(state,
                           dspic33_read_word(cpu, address) ==
                               (uint16_t)(previous & requested),
                           "receive overflow write-zero prior domain");
                }
            }
        }
    }
}

static void receive_flag_read_pointer_cases(CanConformance* state, Dspic33* cpu) {
    static const uint8_t sizes[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t selection;
        for (selection = 0u; selection < sizeof(sizes); selection++) {
            uint8_t size = sizes[selection];
            uint8_t start = (uint8_t)(size / 2u);
            uint8_t buffer;
            for (buffer = start; buffer < size; buffer++) {
                uint16_t address =
                    (uint16_t)(bases[channel] + 0x20u + (buffer >= 16u ? 2u : 0u));
                uint16_t bit = (uint16_t)(1u << (buffer & 15u));
                uint16_t fifo_address = (uint16_t)(bases[channel] + 8u);
                uint8_t expected = buffer + 1u == size ? start : (uint8_t)(buffer + 1u);
                dspic33_reset(cpu, 0u);
                configure_receive(cpu, channel, 0xf800u, selection, start);
                select_window(cpu, channel, false);
                write_memory_word(cpu, address, bit);
                write_memory_word(cpu, fifo_address,
                                  (uint16_t)(((uint16_t)start << 8u) | 0x003fu));
                dspic33_write_word(cpu, address, (uint16_t)~bit);
                expect(state, dspic33_read_word(cpu, address) == 0u,
                       "FIFO receive flag clear");
                expect(state,
                       (dspic33_read_word(cpu, fifo_address) & 0x003fu) == expected,
                       "FIFO receive flag advances read pointer");
                expect(state,
                       (dspic33_read_word(cpu, fifo_address) & 0x3f00u) ==
                           (uint16_t)((uint16_t)start << 8u),
                       "FIFO receive flag preserves write pointer");
            }
            if (start != 0u) {
                uint8_t buffer = (uint8_t)(start - 1u);
                uint16_t address = (uint16_t)(bases[channel] + 0x20u);
                uint16_t bit = (uint16_t)(1u << buffer);
                uint16_t fifo_address = (uint16_t)(bases[channel] + 8u);
                dspic33_reset(cpu, 0u);
                configure_receive(cpu, channel, 0xf800u, selection, start);
                select_window(cpu, channel, false);
                write_memory_word(cpu, address, bit);
                write_memory_word(cpu, fifo_address,
                                  (uint16_t)(((uint16_t)start << 8u) | 0x003eu));
                dspic33_write_word(cpu, address, (uint16_t)~bit);
                expect(state,
                       (dspic33_read_word(cpu, fifo_address) & 0x003fu) == 0x003eu,
                       "direct receive flag preserves FIFO read pointer");
            }
        }
    }
}

static void fifo_interrupt_boundary_case(CanConformance* state, Dspic33* cpu,
                                         uint8_t channel, bool wrap, uint8_t relation) {
    uint16_t base = bases[channel];
    uint16_t fifo_address = (uint16_t)(base + 8u);
    uint16_t interrupt_address = (uint16_t)(base + 0x0au);
    uint32_t memory = (uint32_t)(0xf800u + channel * 0x100u);
    Dspic33CanFrame input = frame(0x456u, false, false, 1u, 0x90u);
    uint8_t preparation = wrap ? 2u : 0u;
    uint8_t index;
    uint8_t expected_fbp = wrap ? 5u : 3u;
    uint8_t fnrb;
    bool asserted = relation == FIFO_RELATION_THRESHOLD;
    if (asserted) {
        fnrb = wrap ? 2u : 4u;
    } else if (relation == FIFO_RELATION_EQUAL) {
        fnrb = expected_fbp;
    } else {
        fnrb = wrap ? 3u : 5u;
    }
    dspic33_reset(cpu, 0u);
    configure_receive(cpu, channel, memory, 1u, 2u);
    configure_filter(cpu, channel, 0u, input.identifier, false, 0x7ffu, true, 15u, 0u);
    enable_filter(cpu, channel, 1u);
    select_window(cpu, channel, false);
    set_mode(cpu, channel, 0u);
    for (index = 0u; index < preparation; index++) {
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "FIFO interrupt boundary preparation");
    }
    write_memory_word(
        cpu, fifo_address,
        (uint16_t)((dspic33_read_word(cpu, fifo_address) & 0x3f00u) | fnrb));
    dspic33_write_word(
        cpu, interrupt_address,
        (uint16_t)(dspic33_read_word(cpu, interrupt_address) & ~0x000au));
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x0008u);
    clear_interrupt_flag(cpu, event_irqs[channel]);
    expect(state,
           dspic33_can_receive(cpu, channel, &input, 0u) &&
               dspic33_device_advance(cpu, 32u),
           "FIFO interrupt boundary receive");
    expect(state,
           ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_fbp,
           "FIFO interrupt uses updated write pointer");
    expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == fnrb,
           "FIFO interrupt preserves read pointer");
    expect(state,
           ((dspic33_read_word(cpu, interrupt_address) & 0x0008u) != 0u) == asserted,
           "FIFO interrupt boundary result");
    expect(state, interrupt_flag(cpu, event_irqs[channel]) == asserted,
           "FIFO interrupt boundary IFS result");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & 0x007fu) ==
               (asserted ? 0x44u : 0x40u),
           "FIFO interrupt boundary vector result");
}

static void fifo_interrupt_boundary_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t wrap;
        for (wrap = 0u; wrap < 2u; wrap++) {
            uint8_t relation;
            for (relation = FIFO_RELATION_THRESHOLD; relation <= FIFO_RELATION_DISTANT;
                 relation++) {
                fifo_interrupt_boundary_case(state, cpu, channel, wrap != 0u, relation);
            }
        }
    }
}

static void receive_flag_hardware_event_case(CanConformance* state, Dspic33* cpu,
                                             uint8_t channel, uint8_t target) {
    uint16_t base = bases[channel];
    uint16_t fifo_address = (uint16_t)(base + 8u);
    uint16_t interrupt_address = (uint16_t)(base + 0x0au);
    uint16_t overflow_address = (uint16_t)(base + 0x28u + (target >= 16u ? 2u : 0u));
    uint16_t target_bit = (uint16_t)(1u << (target & 15u));
    uint32_t memory = (uint32_t)(0xf000u + channel * 0x400u);
    uint16_t preserved_words[8];
    uint8_t buffer;
    uint8_t word;
    dspic33_reset(cpu, 0u);
    configure_receive(cpu, channel, memory, 6u, target);
    configure_filter(cpu, channel, 0u, 0x456u, false, 0x7ffu, true, 15u, 0u);
    enable_filter(cpu, channel, 1u);
    select_window(cpu, channel, false);
    set_mode(cpu, channel, 0u);
    for (buffer = target; buffer < 32u; buffer++) {
        Dspic33CanFrame input =
            frame(0x456u, false, false, 1u, (uint8_t)(0x20u + buffer));
        uint8_t expected_fbp = buffer == 31u ? target : (uint8_t)(buffer + 1u);
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "receive flag hardware fill event");
        expect(state, receive_full(cpu, channel, buffer),
               "receive flag hardware sets RXFUL");
        expect(state,
               memory_word(cpu, memory + buffer * 16u + 6u) ==
                   (uint8_t)(0x20u + buffer),
               "receive flag hardware stores message");
        expect(state,
               ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_fbp,
               "receive flag hardware advances write pointer");
        expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == target,
               "receive flag hardware preserves read pointer");
    }
    for (word = 0u; word < 8u; word++) {
        preserved_words[word] = memory_word(cpu, memory + target * 16u + word * 2u);
    }
    expect(state, (dspic33_read_word(cpu, overflow_address) & target_bit) == 0u,
           "receive overflow flag initially clear");
    expect(state, (dspic33_read_word(cpu, interrupt_address) & 0x0004u) == 0u,
           "receive overflow interrupt initially clear");
    {
        Dspic33CanFrame overflow = frame(0x456u, false, false, 1u, 0xe0u);
        uint8_t expected_fbp = target == 31u ? target : (uint8_t)(target + 1u);
        expect(state,
               dspic33_can_receive(cpu, channel, &overflow, 0u) &&
                   dspic33_device_advance(cpu, 2u),
               "receive overflow hardware event");
        expect(state, receive_full(cpu, channel, target),
               "receive overflow preserves RXFUL");
        expect(state, (dspic33_read_word(cpu, overflow_address) & target_bit) != 0u,
               "receive overflow hardware sets RXOVF");
        expect(state, (dspic33_read_word(cpu, interrupt_address) & 0x0004u) != 0u,
               "receive overflow hardware sets RBOVIF");
        expect(state,
               ((dspic33_read_word(cpu, fifo_address) >> 8u) & 0x003fu) == expected_fbp,
               "receive overflow advances write pointer");
        expect(state, (dspic33_read_word(cpu, fifo_address) & 0x003fu) == target,
               "receive overflow preserves read pointer");
        for (word = 0u; word < 8u; word++) {
            expect(state,
                   memory_word(cpu, memory + target * 16u + word * 2u) ==
                       preserved_words[word],
                   "receive overflow loses complete message");
        }
    }
}

static void receive_flag_hardware_event_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t target;
        for (target = 0u; target < 32u; target++) {
            receive_flag_hardware_event_case(state, cpu, channel, target);
        }
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

static uint16_t expected_error_status(bool transmit, uint16_t count, bool bus_off) {
    uint16_t status = 0u;
    if (count >= 96u) {
        status |= CAN_ERROR_WARNING;
        if (transmit) {
            if (count < 128u) {
                status |= CAN_TRANSMIT_WARNING;
            } else if (!bus_off) {
                status |= CAN_TRANSMIT_PASSIVE;
            }
        } else {
            if (count < 128u) {
                status |= CAN_RECEIVE_WARNING;
            } else {
                status |= CAN_RECEIVE_PASSIVE;
            }
        }
    }
    if (bus_off) {
        status |= CAN_BUS_OFF;
    }
    return status;
}

static void configure_error_test(Dspic33* cpu, uint8_t channel) {
    dspic33_reset(cpu, 0u);
    set_mode(cpu, channel, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 0x0cu), CAN_INTERRUPT_ERROR);
}

static void expect_error_step(CanConformance* state, Dspic33* cpu, uint8_t channel,
                              bool transmit, uint8_t increment,
                              uint16_t expected_counts, uint16_t expected_status,
                              bool expected_interrupt) {
    uint16_t status_address = (uint16_t)(bases[channel] + 0x0au);
    uint16_t status;
    expect(state,
           dspic33_can_error(cpu, channel, transmit, increment, 0u) &&
               dspic33_device_advance(cpu, 1u),
           "error counter event schedule");
    status = dspic33_read_word(cpu, status_address);
    expect(state,
           dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) ==
               expected_counts,
           "error counter result");
    expect(state, (status & CAN_ERROR_STATUS_MASK) == expected_status,
           "error state result");
    expect(state, ((status & CAN_INTERRUPT_ERROR) != 0u) == expected_interrupt,
           "error transition flag");
    expect(state, interrupt_flag(cpu, event_irqs[channel]) == expected_interrupt,
           "error transition interrupt");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) ==
               (expected_interrupt ? 0x41u : 0x40u),
           "error transition vector");
}

static void clear_error_interrupt(CanConformance* state, Dspic33* cpu,
                                  uint8_t channel) {
    uint16_t address = (uint16_t)(bases[channel] + 0x0au);
    dspic33_write_word(
        cpu, address,
        (uint16_t)(dspic33_read_word(cpu, address) & ~CAN_INTERRUPT_ERROR));
    clear_interrupt_flag(cpu, event_irqs[channel]);
    expect(state, (dspic33_read_word(cpu, address) & CAN_INTERRUPT_ERROR) == 0u,
           "error flag clear");
    expect(state, !interrupt_flag(cpu, event_irqs[channel]), "error interrupt clear");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "error vector clear");
}

static void error_threshold_domain(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t direction;
        for (direction = 0u; direction < 2u; direction++) {
            bool transmit = direction != 0u;
            uint16_t count;
            for (count = 1u; count <= UINT8_MAX; count++) {
                uint16_t counts = transmit ? (uint16_t)(count << 8u) : count;
                configure_error_test(cpu, channel);
                expect_error_step(state, cpu, channel, transmit, (uint8_t)count, counts,
                                  expected_error_status(transmit, count, false),
                                  count >= 96u);
            }
        }
    }
}

static void receive_error_transition_cases(CanConformance* state, Dspic33* cpu,
                                           uint8_t channel) {
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, false, 95u, 0x005fu, 0u, false);
    expect_error_step(state, cpu, channel, false, 1u, 0x0060u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING, true);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, false, 1u, 0x0061u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING, false);
    expect_error_step(state, cpu, channel, false, 31u, 0x0080u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE, true);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, false, 1u, 0x0081u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE, false);
    expect_error_step(state, cpu, channel, false, 126u, 0x00ffu,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE, false);
    expect_error_step(state, cpu, channel, false, 1u, 0x00ffu,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE, false);
    set_mode(cpu, channel, 4u);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0u,
           "configuration clears receive error counter");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) &
            CAN_ERROR_STATUS_MASK) == 0u,
           "configuration clears receive error state");
    set_mode(cpu, channel, 0u);
    expect_error_step(state, cpu, channel, false, 96u, 0x0060u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING, true);
}

static void transmit_error_transition_cases(CanConformance* state, Dspic33* cpu,
                                            uint8_t channel) {
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, true, 95u, 0x5f00u, 0u, false);
    expect_error_step(state, cpu, channel, true, 1u, 0x6000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING, true);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0x6100u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING, false);
    expect_error_step(state, cpu, channel, true, 31u, 0x8000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE, true);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0x8100u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE, false);
    expect_error_step(state, cpu, channel, true, 126u, 0xff00u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE, false);
    expect_error_step(state, cpu, channel, true, 1u, 0xff00u,
                      CAN_ERROR_WARNING | CAN_BUS_OFF, true);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0xff00u,
                      CAN_ERROR_WARNING | CAN_BUS_OFF, false);
    set_mode(cpu, channel, 4u);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0u,
           "configuration clears transmit error counter");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) &
            CAN_ERROR_STATUS_MASK) == 0u,
           "configuration clears bus off state");
    set_mode(cpu, channel, 0u);
    expect_error_step(state, cpu, channel, true, 96u, 0x6000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING, true);
}

static void complete_error_test_transmission(CanConformance* state, Dspic33* cpu,
                                             uint8_t channel) {
    uint32_t memory = (uint32_t)(0xe000u + channel * 0x100u);
    Dspic33CanFrame output;
    uint8_t word;
    configure_transmit(cpu, channel, memory);
    for (word = 0u; word < 8u; word++) {
        write_memory_word(cpu, memory + word * 2u, 0u);
    }
    select_window(cpu, channel, false);
    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 0x30u), 0x008bu);
    expect(state, dspic33_device_advance(cpu, 32u),
           "error recovery transmission advance");
    expect(state, dspic33_can_transmit(cpu, channel, &output),
           "error recovery transmission output");
}

static void transmit_error_descending_entry_cases(CanConformance* state, Dspic33* cpu,
                                                  uint8_t channel) {
    uint16_t status_address = (uint16_t)(bases[channel] + 0x0au);
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, true, 128u, 0x8000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE, true);
    clear_error_interrupt(state, cpu, channel);
    complete_error_test_transmission(state, cpu, channel);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0x7f00u,
           "successful transmission decrements error counter");
    expect(state,
           (dspic33_read_word(cpu, status_address) & CAN_ERROR_STATUS_MASK) ==
               (CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING),
           "successful transmission enters error warning");
    expect(state, (dspic33_read_word(cpu, status_address) & CAN_INTERRUPT_ERROR) != 0u,
           "descending error transition flag");
    expect(state, interrupt_flag(cpu, event_irqs[channel]),
           "descending error transition interrupt");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x41u,
           "descending error transition vector");
    clear_error_interrupt(state, cpu, channel);
    complete_error_test_transmission(state, cpu, channel);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0x7e00u,
           "within-warning transmission decrements error counter");
    expect(state,
           (dspic33_read_word(cpu, status_address) & CAN_ERROR_STATUS_MASK) ==
               (CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING),
           "within-warning transmission preserves error state");
    expect(state, (dspic33_read_word(cpu, status_address) & CAN_INTERRUPT_ERROR) == 0u,
           "within-warning transmission does not set error flag");
    expect(state, !interrupt_flag(cpu, event_irqs[channel]),
           "within-warning transmission does not raise error interrupt");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "within-warning transmission keeps default vector");
}

static void interrupt_and_error_cases(CanConformance* state, Dspic33* cpu) {
    uint8_t channel;
    error_threshold_domain(state, cpu);
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        receive_error_transition_cases(state, cpu, channel);
        transmit_error_transition_cases(state, cpu, channel);
        transmit_error_descending_entry_cases(state, cpu, channel);
    }
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
    interrupt_flag_write_zero_cases(&state, &cpu);
    standard_filter_domain(&state, &cpu);
    extended_filter_cases(&state, &cpu);
    payload_and_remote_cases(&state, &cpu);
    devicenet_cases(&state, &cpu);
    direct_buffer_cases(&state, &cpu);
    fifo_cases(&state, &cpu);
    fifo_overflow_advancement_cases(&state, &cpu);
    receive_flag_write_zero_domain(&state, &cpu);
    receive_overflow_write_zero_prior_domain(&state, &cpu);
    receive_flag_read_pointer_cases(&state, &cpu);
    fifo_interrupt_boundary_cases(&state, &cpu);
    receive_flag_hardware_event_cases(&state, &cpu);
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
