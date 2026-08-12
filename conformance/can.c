#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device.h"
#include "dspic33.h"
#include "sfr_side_effect_coverage.h"

static const SfrSideEffectCoverage can_sfr_side_effect_coverage[] = {
    {0x0400u, 0x1000u}, {0x040au, 0x00efu}, {0x0420u, 0xffffu}, {0x0422u, 0xffffu},
    {0x0428u, 0xffffu}, {0x042au, 0xffffu}, {0x0500u, 0x1000u}, {0x050au, 0x00efu},
    {0x0520u, 0xffffu}, {0x0522u, 0xffffu}, {0x0528u, 0xffffu}, {0x052au, 0xffffu},
};

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} CanConformance;

static const uint16_t bases[DSPIC33_CAN_COUNT] = {0x0400u, 0x0500u};
static const uint8_t event_irqs[DSPIC33_CAN_COUNT] = {35u, 56u};
static const uint8_t receive_requests[DSPIC33_CAN_COUNT] = {34u, 55u};
static const uint8_t transmit_requests[DSPIC33_CAN_COUNT] = {70u, 71u};
static void write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value);

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

static void write_transmit_frame(Dspic33* cpu, uint32_t memory,
                                 const Dspic33CanFrame* value) {
    uint16_t words[8] = {0};
    uint32_t sid =
        value->extended ? (value->identifier >> 18u) & 0x7ffu : value->identifier;
    uint32_t eid = value->identifier & 0x3ffffu;
    words[0] = (uint16_t)(sid << 2u);
    if (value->extended) {
        words[0] |= 3u;
        words[1] = (uint16_t)(eid >> 6u);
        words[2] = (uint16_t)((eid & 0x3fu) << 10u);
        if (value->remote) {
            words[2] |= 0x0200u;
        }
    } else if (value->remote) {
        words[0] |= 2u;
    }
    words[2] |= value->length;
    for (uint8_t index = 0u; index < value->length; index++) {
        words[3u + index / 2u] |= (uint16_t)value->data[index] << ((index & 1u) * 8u);
    }
    for (uint8_t index = 0u; index < 8u; index++) {
        write_memory_word(cpu, memory + index * 2u, words[index]);
    }
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

static void register_access_cases(CanConformance* state, Dspic33* cpu) {
    static const uint16_t received_words[] = {0x0c84u, 0x0000u, 0x0004u, 0x5140u,
                                              0x7362u, 0x0000u, 0x0000u, 0x0000u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t receive_data = (uint16_t)(base + 0x40u);
        uint32_t memory = (uint32_t)(0xf600u + channel * 0x100u);
        Dspic33CanFrame input = frame(0x321u, false, false, 4u, 0x40u);
        uint8_t index;
        bool preserved;
        bool active;
        bool exact;

        dspic33_reset(cpu, 0u);
        write_memory_word(cpu, (uint16_t)(base + 0x0eu), 0x5aa5u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), 0u);
        preserved = dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0x5aa5u;
        dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), 0xffffu);
        expect(state,
               preserved && dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0x5aa5u,
               "error counters reject CPU writes");

        dspic33_write_word(
            cpu, base, (uint16_t)((dspic33_read_word(cpu, base) & ~0x07e0u) | 0x02e0u));
        expect(state, (dspic33_read_word(cpu, base) & 0x07e0u) == 0x0240u,
               "requested mode controls operating mode");
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) | 0x00a0u));
        expect(state, (dspic33_read_word(cpu, base) & 0x07e0u) == 0x0240u,
               "operating mode rejects direct writes");

        dspic33_reset(cpu, 0u);
        select_window(cpu, channel, false);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 1u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x8989u);
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) | 0x1000u));
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x4848u) == 0x4040u,
               "abort all marks pending transmissions aborted");
        expect(state,
               (dspic33_read_word(cpu, base) & 0x1000u) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 1u) != 0u &&
                   interrupt_flag(cpu, event_irqs[channel]),
               "abort all self clears and raises transmit event");

        dspic33_reset(cpu, 0u);
        configure_receive(cpu, channel, memory, 4u, 0u);
        configure_filter(cpu, channel, 0u, 0x321u, false, 0x7ffu, true, 0u, 0u);
        enable_filter(cpu, channel, 1u);
        select_window(cpu, channel, false);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, receive_data, 0xa55au);
        expect(state, dspic33_read_word(cpu, receive_data) == 0xa55au,
               "receive data CPU word access");
        dspic33_write_byte(cpu, receive_data, 0x3cu);
        dspic33_write_byte(cpu, (uint16_t)(receive_data + 1u), 0xc3u);
        expect(state, dspic33_read_word(cpu, receive_data) == 0xc33cu,
               "receive data CPU byte access");
        active = dspic33_can_receive(cpu, channel, &input, 0u) &&
                 dspic33_device_advance(cpu, 0u);
        expect(state,
               active && (cpu->io.can_rx_busy & (uint8_t)(1u << channel)) != 0u &&
                   dspic33_read_word(cpu, receive_data) == received_words[0] &&
                   memory_word(cpu, memory) == received_words[0],
               "receive stream overrides CPU backing for DMA");
        dspic33_write_word(cpu, receive_data, 0xc55cu);
        expect(state, dspic33_read_word(cpu, receive_data) == received_words[0],
               "receive stream survives concurrent CPU write");
        expect(state, dspic33_device_advance(cpu, 32u),
               "receive stream completion advance");
        exact = true;
        for (index = 0u; index < 8u; index++) {
            exact =
                exact && memory_word(cpu, memory + index * 2u) == received_words[index];
        }
        expect(state, exact, "receive stream DMA words");
        expect(state,
               (cpu->io.can_rx_busy & (uint8_t)(1u << channel)) == 0u &&
                   dspic33_read_word(cpu, receive_data) == 0xc55cu,
               "receive data backing returns after stream");
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
                expect(state, dspic33_device_advance(cpu, 4096u), "transmit advance");
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

static void clock_timing_cases(CanConformance* state, Dspic33* cpu) {
    static const uint16_t clock_controls[] = {0u, 0x0800u, 0u, 0u};
    static const uint16_t config1_values[] = {0u, 0u, 1u, 0u};
    static const uint16_t config2_values[] = {0u, 0u, 0u, 0x0311u};
    static const uint64_t completion_cycles[] = {208u, 408u, 408u, 508u};
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (uint8_t timing = 0u;
             timing < sizeof(completion_cycles) / sizeof(completion_cycles[0]);
             timing++) {
            uint16_t base = bases[channel];
            uint32_t memory = (uint32_t)(0xb800u + channel * 0x100u);
            Dspic33CanFrame output;
            dspic33_reset(cpu, 0u);
            configure_transmit(cpu, channel, memory);
            write_memory_word(cpu, memory, 2u);
            for (uint8_t word = 1u; word < 8u; word++) {
                write_memory_word(cpu, memory + word * 2u, 0u);
            }
            select_window(cpu, channel, false);
            set_mode(cpu, channel, 4u);
            dspic33_write_word(cpu, (uint16_t)(base + 0x10u), config1_values[timing]);
            dspic33_write_word(cpu, (uint16_t)(base + 0x12u), config2_values[timing]);
            dspic33_write_word(cpu, base,
                               (uint16_t)((dspic33_read_word(cpu, base) & ~0x0800u) |
                                          clock_controls[timing]));
            set_mode(cpu, channel, 0u);
            dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 0x10u)) ==
                           config1_values[timing] &&
                       dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) ==
                           config2_values[timing] &&
                       (dspic33_read_word(cpu, base) & 0x0800u) ==
                           clock_controls[timing],
                   "CAN bit timing configuration is retained");
            expect(state,
                   dspic33_device_advance(cpu, completion_cycles[timing] - 1u) &&
                       !dspic33_can_transmit(cpu, channel, &output),
                   "CAN frame remains active before its final bus bit");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       dspic33_can_transmit(cpu, channel, &output) &&
                       output.identifier == 0u && output.remote && output.length == 0u,
                   "CAN frame completes on its configured B1 clock boundary");
        }
    }
}

static void stuffed_frame_timing_cases(CanConformance* state, Dspic33* cpu) {
    static const uint16_t words[][8] = {
        {0x0c84u, 0u, 1u, 0x00a5u, 0u, 0u, 0u, 0u},
        {0x0123u, 0x0d15u, 0x9c08u, 0x0100u, 0x0302u, 0x0504u, 0x0706u, 0u},
    };
    static const uint64_t completion_cycles[] = {236u, 576u};
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (uint8_t frame_index = 0u;
             frame_index < sizeof(completion_cycles) / sizeof(completion_cycles[0]);
             frame_index++) {
            uint16_t base = bases[channel];
            uint32_t memory = (uint32_t)(0xba00u + channel * 0x100u);
            Dspic33CanFrame output;
            dspic33_reset(cpu, 0u);
            configure_transmit(cpu, channel, memory);
            for (uint8_t word = 0u; word < 8u; word++) {
                write_memory_word(cpu, memory + word * 2u, words[frame_index][word]);
            }
            select_window(cpu, channel, false);
            set_mode(cpu, channel, 0u);
            dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
            expect(state,
                   dspic33_device_advance(cpu, completion_cycles[frame_index] - 1u) &&
                       !dspic33_can_transmit(cpu, channel, &output),
                   "stuffed CAN frame remains active before its calculated boundary");
            expect(state,
                   dspic33_device_advance(cpu, 1u) &&
                       dspic33_can_transmit(cpu, channel, &output) &&
                       output.extended == (frame_index != 0u) &&
                       output.length == (frame_index == 0u ? 1u : 8u),
                   "stuffed CAN frame completes on its calculated boundary");
        }
    }
}

static void transmit_abort_timing_cases(CanConformance* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xbc00u + channel * 0x100u);
        Dspic33CanFrame output;
        dspic33_reset(cpu, 0u);
        configure_transmit(cpu, channel, memory);
        write_memory_word(cpu, memory, 2u);
        for (uint8_t word = 1u; word < 8u; word++) {
            write_memory_word(cpu, memory + word * 2u, 0u);
        }
        select_window(cpu, channel, false);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u &&
                   !dspic33_can_transmit(cpu, channel, &output),
               "CAN abort test reaches the on-bus interval");
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) | 0x1000u));
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x0048u) ==
                       0x0040u &&
                   dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_can_transmit(cpu, channel, &output),
               "CAN abort cancels the pending on-bus completion");

        dspic33_reset(cpu, 0u);
        configure_transmit(cpu, channel, memory);
        write_memory_word(cpu, memory, 2u);
        for (uint8_t word = 1u; word < 8u; word++) {
            write_memory_word(cpu, memory + word * 2u, 0u);
        }
        select_window(cpu, channel, false);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) != 0u,
               "individual CAN abort reaches the on-bus interval");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x0083u);
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x0048u) ==
                       0x0040u &&
                   dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_can_transmit(cpu, channel, &output),
               "clearing TXREQ aborts the active CAN transmission");
    }
}

static void transmit_pps_cases(CanConformance* state, Dspic33* cpu) {
    bool high;
    expect(state, !dspic33_can_pin(cpu, 64u, NULL),
           "CAN output rejects null pin level");
    expect(state, !dspic33_can_pin(cpu, 63u, &high),
           "CAN output rejects non-remappable pin");
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xbe00u + channel * 0x100u);
        uint8_t function = (uint8_t)(14u + channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, function);
        expect(state, dspic33_can_pin(cpu, 64u, &high) && high,
               "mapped CAN transmit pin is recessive while idle");
        configure_transmit(cpu, channel, memory);
        for (uint8_t word = 0u; word < 8u; word++) {
            write_memory_word(cpu, memory + word * 2u, 0u);
        }
        select_window(cpu, channel, false);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &high) &&
                   !high,
               "CAN transmit pin drives dominant start of frame");
        expect(state,
               dspic33_device_advance(cpu, 20u) && dspic33_can_pin(cpu, 64u, &high) &&
                   high,
               "CAN transmit pin inserts the sixth complementary stuffed bit");
        expect(state,
               dspic33_device_advance(cpu, 4u) && dspic33_can_pin(cpu, 64u, &high) &&
                   !high,
               "CAN transmit pin resumes frame data after stuffing");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x0083u);
        expect(state, dspic33_can_pin(cpu, 64u, &high) && high,
               "aborted CAN transmit pin returns recessive");
        dspic33_write_word(cpu, 0x0680u, (uint16_t)(function << 8u));
        expect(state,
               !dspic33_can_pin(cpu, 64u, &high) && dspic33_can_pin(cpu, 65u, &high) &&
                   high,
               "CAN transmit output follows PPS remapping");
        dspic33_write_word(
            cpu, 0x0760u,
            (uint16_t)(dspic33_read_word(cpu, 0x0760u) | (uint16_t)(2u << channel)));
        expect(state,
               dspic33_device_advance(cpu, 1u) && !dspic33_can_pin(cpu, 65u, &high),
               "PMD releases the CAN transmit PPS output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, function);
        configure_transmit(cpu, channel, memory);
        for (uint8_t word = 0u; word < 8u; word++) {
            write_memory_word(cpu, memory + word * 2u, 0u);
        }
        select_window(cpu, channel, false);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &high) &&
                   !high,
               "CAN Sleep output test reaches dominant bus phase");
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state, dspic33_can_pin(cpu, 64u, &high) && high,
               "Sleep forces the CAN transmit pin recessive");
    }
}

static bool bridge_can_pins(Dspic33* cpu, uint8_t transmit_channel, uint8_t pin,
                            uint8_t acknowledge_pin, uint64_t bit_cycles,
                            int corrupt_bit, bool* acknowledge_observed) {
    uint16_t bit = 0u;
    while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << transmit_channel)) != 0u &&
           bit < 160u) {
        bool high;
        bool acknowledge_high;
        if (dspic33_can_pin(cpu, acknowledge_pin, &acknowledge_high) &&
            !acknowledge_high) {
            *acknowledge_observed = true;
        }
        if (!dspic33_can_pin(cpu, pin, &high)) {
            return false;
        }
        if (bit == corrupt_bit) {
            high = !high;
        }
        if (!dspic33_can_input_pin(cpu, pin, high && acknowledge_high, 0u) ||
            !dspic33_device_advance(cpu, bit_cycles)) {
            return false;
        }
        bit++;
    }
    return bit != 0u && bit < 160u && dspic33_device_advance(cpu, 32u);
}

static bool drive_shared_can_bus(Dspic33* can1, Dspic33* can2, uint8_t active_channel,
                                 uint64_t bit_cycles) {
    uint16_t count = 0u;
    Dspic33* active = active_channel == 0u ? can1 : can2;
    while ((active->io.can_tx_on_bus & (uint8_t)(1u << active_channel)) != 0u &&
           count < 160u) {
        bool can1_high;
        bool can2_high;
        bool bus_high;
        if (!dspic33_can_pin(can1, 65u, &can1_high) ||
            !dspic33_can_pin(can2, 66u, &can2_high)) {
            return false;
        }
        bus_high = can1_high && can2_high;
        if (!dspic33_can_input_pin(can1, 64u, bus_high, 0u) ||
            !dspic33_can_input_pin(can2, 64u, bus_high, 0u) ||
            !dspic33_device_advance(can1, bit_cycles) ||
            !dspic33_device_advance(can2, bit_cycles)) {
            return false;
        }
        count++;
    }
    return count != 0u && count < 160u;
}

static void arbitration_field_cases(CanConformance* state, Dspic33* cpu) {
    Dspic33CanFrame contenders[4][2];
    Dspic33 winner;
    contenders[0][0] = frame(0x155u, false, false, 0u, 0u);
    contenders[0][1] = frame(0x155u, false, true, 0u, 0u);
    contenders[1][0] = frame(0x155u, false, false, 0u, 0u);
    contenders[1][1] = frame(0x5540000u, true, false, 0u, 0u);
    contenders[2][0] = frame(0x1550000u, true, false, 0u, 0u);
    contenders[2][1] = frame(0x1550001u, true, false, 0u, 0u);
    contenders[3][0] = frame(0x1550000u, true, false, 0u, 0u);
    contenders[3][1] = frame(0x1550000u, true, true, 0u, 0u);
    expect(state, dspic33_initialize(&winner),
           "initialize independent CAN arbitration contender");
    for (uint8_t index = 0u; index < 4u; index++) {
        Dspic33CanFrame output;
        dspic33_reset(cpu, 0u);
        dspic33_reset(&winner, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0e00u);
        dspic33_write_word(cpu, 0x06d4u, 0x0040u);
        dspic33_write_word(&winner, 0x0e30u, 0xffffu);
        dspic33_write_word(&winner, 0x0e3eu, 0u);
        dspic33_write_word(&winner, 0x0682u, 0x000fu);
        dspic33_write_word(&winner, 0x06d4u, 0x4000u);
        configure_transmit(cpu, 0u, 0xd400u);
        configure_transmit(&winner, 1u, 0xd600u);
        write_transmit_frame(cpu, 0xd400u, &contenders[index][0]);
        write_transmit_frame(&winner, 0xd600u, &contenders[index][1]);
        select_window(cpu, 0u, false);
        select_window(&winner, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, 0u);
        dspic33_write_word(&winner, 0x0510u, 0u);
        dspic33_write_word(&winner, 0x0512u, 0u);
        set_mode(cpu, 0u, 0u);
        set_mode(&winner, 1u, 0u);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        dspic33_write_word(&winner, 0x0530u, 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) && dspic33_device_advance(&winner, 8u) &&
                   (cpu->io.can_tx_on_bus & 1u) != 0u &&
                   (winner.io.can_tx_on_bus & 2u) != 0u,
               "CAN arbitration field contenders start together");
        expect(state, drive_shared_can_bus(cpu, &winner, 0u, 4u),
               "CAN arbitration field selects the dominant contender");
        expect(state,
               (dspic33_read_word(&winner, 0x0530u) & 0x0028u) == 0x0028u &&
                   dspic33_can_transmit(cpu, 0u, &output) &&
                   output.identifier == contenders[index][0].identifier &&
                   output.extended == contenders[index][0].extended &&
                   output.remote == contenders[index][0].remote,
               "CAN arbitration field records loss and completes the winner");
    }
    dspic33_destroy(&winner);
}

static void arbitration_cases(CanConformance* state, Dspic33* cpu) {
    Dspic33CanFrame higher = frame(0x400u, false, false, 2u, 0xa0u);
    Dspic33CanFrame lower = frame(0u, false, false, 2u, 0xb0u);
    Dspic33CanFrame output;
    Dspic33 winner;
    expect(state, dspic33_initialize(&winner),
           "initialize independent CAN arbitration winner");
    dspic33_reset(cpu, 0u);
    dspic33_reset(&winner, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0e00u);
    dspic33_write_word(cpu, 0x06d4u, 0x0040u);
    dspic33_write_word(&winner, 0x0e30u, 0xffffu);
    dspic33_write_word(&winner, 0x0e3eu, 0u);
    dspic33_write_word(&winner, 0x0682u, 0x000fu);
    dspic33_write_word(&winner, 0x06d4u, 0x4000u);
    configure_receive(cpu, 0u, 0xd000u, 4u, 0u);
    configure_receive(&winner, 1u, 0xd200u, 4u, 0u);
    configure_filter(cpu, 0u, 0u, lower.identifier, false, 0x7ffu, true, 0u, 0u);
    configure_filter(&winner, 1u, 0u, higher.identifier, false, 0x7ffu, true, 0u, 0u);
    enable_filter(cpu, 0u, 1u);
    enable_filter(&winner, 1u, 1u);
    configure_transmit(cpu, 0u, 0xd400u);
    configure_transmit(&winner, 1u, 0xd600u);
    write_transmit_frame(cpu, 0xd400u, &higher);
    write_transmit_frame(&winner, 0xd600u, &lower);
    select_window(cpu, 0u, false);
    select_window(&winner, 1u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_write_word(&winner, 0x0510u, 0u);
    dspic33_write_word(&winner, 0x0512u, 0u);
    set_mode(cpu, 0u, 0u);
    set_mode(&winner, 1u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    dspic33_write_word(&winner, 0x0530u, 0x008bu);
    expect(state,
           dspic33_device_advance(cpu, 8u) && dspic33_device_advance(&winner, 8u) &&
               (cpu->io.can_tx_on_bus & 1u) != 0u &&
               (winner.io.can_tx_on_bus & 2u) != 0u,
           "competing CAN transmissions enter the bus together");
    expect(state, drive_shared_can_bus(cpu, &winner, 1u, 4u),
           "lower identifier wins CAN arbitration");
    expect(state,
           (dspic33_read_word(cpu, 0x0430u) & 0x0028u) == 0x0028u &&
               (cpu->io.can_tx_busy & 1u) != 0u && (cpu->io.can_tx_on_bus & 1u) == 0u &&
               !dspic33_can_transmit(cpu, 0u, &output),
           "losing CAN transmission records TXLARB and begins an automatic retry");
    expect(state,
           dspic33_can_transmit(&winner, 1u, &output) &&
               output.identifier == lower.identifier,
           "winning CAN frame completes before the retry");
    expect(state,
           cpu->io.can_rx_serial_count[0] != 0u && dspic33_device_advance(cpu, 7u) &&
               dspic33_device_advance(&winner, 7u) &&
               (cpu->io.can_tx_retry_wait & 1u) == 0u &&
               (cpu->io.can_tx_on_bus & 1u) != 0u,
           "losing node monitors the winner and retries after intermission");
    expect(state, drive_shared_can_bus(cpu, &winner, 0u, 4u),
           "retried CAN transmission completes on the shared bus");
    expect(state,
           dspic33_device_advance(cpu, 8u) && dspic33_can_transmit(cpu, 0u, &output) &&
               output.identifier == higher.identifier &&
               (dspic33_read_word(cpu, 0x0430u) & 0x0078u) == 0x0020u &&
               winner.io.can_rx_serial_count[1] != 0u,
           "successful retry preserves TXLARB and clears TXREQ without errors");
    dspic33_destroy(&winner);
}

static bool drive_unacknowledged_can_frame(Dspic33* cpu, uint8_t channel,
                                           uint8_t transmit_pin, uint8_t receive_pin,
                                           uint64_t bit_cycles) {
    uint16_t count = 0u;
    while ((cpu->io.can_tx_on_bus & (uint8_t)(1u << channel)) != 0u && count < 160u) {
        bool high;
        if (!dspic33_can_pin(cpu, transmit_pin, &high) ||
            !dspic33_can_input_pin(cpu, receive_pin, high, 0u) ||
            !dspic33_device_advance(cpu, bit_cycles)) {
            return false;
        }
        count++;
    }
    return count != 0u && count < 160u;
}

static void acknowledge_error_cases(CanConformance* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xdc00u + channel * 0x100u);
        uint8_t function = (uint8_t)(14u + channel);
        Dspic33CanFrame output;
        bool high;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, function);
        dspic33_write_word(cpu, 0x06d4u, channel == 0u ? 65u : (uint16_t)(65u << 8u));
        configure_transmit(cpu, channel, memory);
        Dspic33CanFrame input = frame((uint32_t)(0x240u + channel), false, false, 1u,
                                      (uint8_t)(0xc0u + channel));
        write_transmit_frame(cpu, memory, &input);
        select_window(cpu, channel, false);
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x12u), 0u);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   (cpu->io.can_tx_on_bus & (uint8_t)(1u << channel)) != 0u,
               "unacknowledged CAN frame reaches the bus");
        expect(state, drive_unacknowledged_can_frame(cpu, channel, 64u, 65u, 4u),
               "unacknowledged CAN frame reaches the ACK slot");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x0018u) ==
                       0x0018u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) >> 8u) == 8u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 1u) == 0u &&
                   (cpu->io.can_tx_error_active & (uint8_t)(1u << channel)) != 0u &&
                   !dspic33_can_transmit(cpu, channel, &output),
               "missing CAN ACK sets TXERR and TEC without completing transmission");
        expect(state, dspic33_can_pin(cpu, 64u, &high) && !high,
               "CAN acknowledge error emits an active error flag");
        if (channel == 0u) {
            Dspic33 copy;
            bool copy_high;
            expect(state, dspic33_initialize(&copy), "initialize CAN error-frame copy");
            expect(state,
                   dspic33_copy(&copy, cpu) && copy.io.can_tx_error_active == 1u &&
                       copy.io.can_tx_error_start_cycle[0] ==
                           cpu->io.can_tx_error_start_cycle[0],
                   "copy preserves active CAN error-frame phase");
            expect(state,
                   dspic33_device_advance(cpu, 24u) &&
                       dspic33_device_advance(&copy, 24u) &&
                       dspic33_can_pin(cpu, 64u, &high) && high &&
                       dspic33_can_pin(&copy, 64u, &copy_high) && copy_high &&
                       copy.io.can_tx_error_active == 1u,
                   "copied CAN error frames enter the recessive delimiter together");
            dspic33_destroy(&copy);
        } else {
            expect(state,
                   dspic33_device_advance(cpu, 24u) &&
                       dspic33_can_pin(cpu, 64u, &high) && high &&
                       (cpu->io.can_tx_error_active & (uint8_t)(1u << channel)) != 0u,
                   "CAN active error flag is followed by a recessive delimiter");
        }
        expect(
            state,
            dspic33_device_advance(cpu, 52u) &&
                (cpu->io.can_tx_error_active & (uint8_t)(1u << channel)) == 0u &&
                (cpu->io.can_tx_on_bus & (uint8_t)(1u << channel)) != 0u,
            "unacknowledged CAN transmission automatically retries after intermission");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x0093u);
        expect(state,
               (cpu->io.can_tx_busy & (uint8_t)(1u << channel)) == 0u &&
                   (cpu->io.can_tx_retry_wait & (uint8_t)(1u << channel)) == 0u &&
                   dspic33_can_pin(cpu, 64u, &high) && high,
               "aborting a retried CAN frame clears error-bus state");
    }
}

static void transmit_error_variant_cases(CanConformance* state, Dspic33* cpu) {
    bool high;
    Dspic33CanFrame input = frame(0u, false, false, 0u, 0u);
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 14u);
    dspic33_write_word(cpu, 0x06d4u, 65u);
    configure_transmit(cpu, 0u, 0xdc00u);
    write_transmit_frame(cpu, 0xdc00u, &input);
    select_window(cpu, 0u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state,
           dspic33_device_advance(cpu, 8u) && dspic33_can_pin(cpu, 64u, &high) && !high,
           "CAN dominant-bit mismatch test reaches SOF");
    expect(state,
           dspic33_can_input_pin(cpu, 65u, true, 0u) && dspic33_device_advance(cpu, 4u),
           "CAN transmitter samples recessive while driving dominant");
    expect(state,
           (dspic33_read_word(cpu, 0x0430u) & 0x0038u) == 0x0018u &&
               (dspic33_read_word(cpu, 0x040eu) >> 8u) == 8u &&
               (cpu->io.can_tx_error_active & 1u) != 0u,
           "dominant CAN mismatch is a bit error rather than arbitration loss");
    expect(state, dspic33_can_pin(cpu, 64u, &high) && !high,
           "active CAN bit error drives a dominant error flag");
    dspic33_write_word(cpu, 0x0430u, 0x0093u);

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 14u);
    dspic33_write_word(cpu, 0x06d4u, 65u);
    configure_transmit(cpu, 0u, 0xdc00u);
    write_transmit_frame(cpu, 0xdc00u, &input);
    select_window(cpu, 0u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_error(cpu, 0u, true, 120u, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x040eu) >> 8u) == 120u,
           "CAN transmitter reaches the error-passive boundary precursor");
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state,
           dspic33_device_advance(cpu, 8u) &&
               drive_unacknowledged_can_frame(cpu, 0u, 64u, 65u, 4u),
           "error-passive CAN transmitter encounters a missing ACK");
    expect(state,
           (dspic33_read_word(cpu, 0x040eu) >> 8u) == 128u &&
               (dspic33_read_word(cpu, 0x040au) & 0x1000u) != 0u &&
               (dspic33_read_word(cpu, 0x0430u) & 0x0018u) == 0x0018u,
           "missing ACK transitions the CAN transmitter to error-passive");
    expect(state,
           dspic33_can_pin(cpu, 64u, &high) && high &&
               (cpu->io.can_tx_error_active & 1u) != 0u,
           "error-passive CAN flag remains recessive");
    expect(state,
           dspic33_device_advance(cpu, 76u) &&
               (cpu->io.can_tx_error_active & 1u) == 0u &&
               (cpu->io.can_tx_on_bus & 1u) != 0u,
           "error-passive CAN transmission retries after delimiter and intermission");
    dspic33_write_word(cpu, 0x0430u, 0x0093u);
    expect(state,
           cpu->io.can_tx_busy == 0u && cpu->io.can_tx_retry_wait == 0u &&
               dspic33_can_pin(cpu, 64u, &high) && high,
           "aborting an error-passive retry releases the CAN bus");
}

static bool drive_until_receive_error(Dspic33* cpu, uint16_t corrupt_bit) {
    for (uint16_t bit = 0u; bit < 160u; bit++) {
        bool transmit_high;
        bool receive_high;
        if ((cpu->io.can_rx_error_active & 2u) != 0u) {
            return true;
        }
        if (!dspic33_can_pin(cpu, 64u, &transmit_high) ||
            !dspic33_can_pin(cpu, 65u, &receive_high)) {
            return false;
        }
        if (bit == corrupt_bit) {
            transmit_high = !transmit_high;
        }
        if (!dspic33_can_input_pin(cpu, 64u, transmit_high && receive_high, 0u) ||
            !dspic33_device_advance(cpu, 4u)) {
            return false;
        }
    }
    return false;
}

static void receive_error_cases(CanConformance* state, Dspic33* cpu) {
    static const uint16_t corrupt_bits[] = {5u, 30u, 42u};
    for (uint8_t index = 0u; index < 3u; index++) {
        bool high;
        Dspic33CanFrame input = frame(0u, false, false, 0u, 0u);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, 0x4000u);
        configure_transmit(cpu, 0u, 0xde00u);
        write_transmit_frame(cpu, 0xde00u, &input);
        select_window(cpu, 0u, false);
        select_window(cpu, 1u, false);
        dspic33_write_word(cpu, 0x0410u, 0u);
        dspic33_write_word(cpu, 0x0412u, 0u);
        dspic33_write_word(cpu, 0x0510u, 0u);
        dspic33_write_word(cpu, 0x0512u, 0u);
        set_mode(cpu, 0u, 0u);
        set_mode(cpu, 1u, 0u);
        dspic33_write_word(cpu, 0x0430u, 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   drive_until_receive_error(cpu, corrupt_bits[index]),
               "physical CAN corruption activates receiver error handling");
        expect(state,
               (dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 1u &&
                   (dspic33_read_word(cpu, 0x050au) & 0x0080u) != 0u &&
                   (dspic33_read_word(cpu, 0x050au) & 0x0020u) == 0u &&
                   !receive_full(cpu, 1u, 0u),
               "CAN receiver error updates REC and IVRIF without B1 ERRIF");
        expect(state, dspic33_can_pin(cpu, 65u, &high) && !high,
               "error-active CAN receiver drives a dominant error flag");
        dspic33_write_word(cpu, 0x0430u, 0x0083u);
        expect(state,
               dspic33_device_advance(cpu, 24u) && dspic33_can_pin(cpu, 65u, &high) &&
                   high && dspic33_device_advance(cpu, 32u) &&
                   (cpu->io.can_rx_error_active & 2u) == 0u,
               "CAN receiver error flag ends after its delimiter");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
    dspic33_write_word(cpu, 0x06d4u, 0x4000u);
    configure_transmit(cpu, 0u, 0xde00u);
    Dspic33CanFrame passive = frame(0u, false, false, 0u, 0u);
    write_transmit_frame(cpu, 0xde00u, &passive);
    select_window(cpu, 0u, false);
    select_window(cpu, 1u, false);
    dspic33_write_word(cpu, 0x0410u, 0u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    dspic33_write_word(cpu, 0x0510u, 0u);
    dspic33_write_word(cpu, 0x0512u, 0u);
    set_mode(cpu, 0u, 0u);
    set_mode(cpu, 1u, 0u);
    expect(state,
           dspic33_can_error(cpu, 1u, false, 127u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "CAN receiver reaches the error-passive boundary precursor");
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state,
           dspic33_device_advance(cpu, 8u) && drive_until_receive_error(cpu, 5u) &&
               (dspic33_read_word(cpu, 0x050eu) & 0x00ffu) == 128u &&
               (dspic33_read_word(cpu, 0x050au) & 0x0800u) != 0u,
           "physical CAN corruption transitions the receiver to error-passive");
    bool high;
    expect(state,
           dspic33_can_pin(cpu, 65u, &high) && high &&
               (cpu->io.can_rx_error_active & 2u) != 0u,
           "error-passive CAN receiver flag remains recessive");
}

static bool drive_can_recessive_bits(Dspic33* cpu, uint8_t pin, uint16_t count) {
    for (uint16_t bit = 0u; bit < count; bit++) {
        if (!dspic33_can_input_pin(cpu, pin, true, 0u) ||
            !dspic33_device_advance(cpu, 4u)) {
            return false;
        }
    }
    return true;
}

static void bus_off_recovery_cases(CanConformance* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xde00u + channel * 0x100u);
        uint8_t function = (uint8_t)(14u + channel);
        bool high;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, function);
        dspic33_write_word(cpu, 0x06d4u, channel == 0u ? 65u : (uint16_t)(65u << 8u));
        configure_transmit(cpu, channel, memory);
        Dspic33CanFrame input =
            frame((uint32_t)(0x300u + channel), false, false, 0u, 0u);
        write_transmit_frame(cpu, memory, &input);
        select_window(cpu, channel, false);
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x12u), 0u);
        set_mode(cpu, channel, 0u);
        expect(state,
               dspic33_can_error(cpu, channel, true, 248u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) >> 8u) == 248u,
               "CAN transmitter reaches the bus-off boundary precursor");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x008bu);
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   drive_unacknowledged_can_frame(cpu, channel, 64u, 65u, 4u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x2000u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x0018u) ==
                       0x0018u,
               "missing ACK at TEC 248 enters CAN bus-off");
        expect(state,
               dspic33_can_pin(cpu, 64u, &high) && high &&
                   cpu->io.can_tx_error_active == 0u && cpu->io.can_tx_retry_wait == 0u,
               "bus-off CAN controller releases the bus and suppresses retry");
        expect(state,
               drive_can_recessive_bits(cpu, 65u, 10u) &&
                   dspic33_can_input_pin(cpu, 65u, false, 0u) &&
                   dspic33_device_advance(cpu, 4u) &&
                   drive_can_recessive_bits(cpu, 65u, 1407u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x2000u) != 0u &&
                   cpu->io.can_bus_off_recessive_bits[channel] == 1407u,
               "dominant CAN bit resets the bus-off recovery sequence");
        expect(state,
               drive_can_recessive_bits(cpu, 65u, 1u) &&
                   dspic33_device_advance(cpu, 4u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x3f00u) == 0u &&
                   dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0u &&
                   (cpu->io.can_tx_on_bus & (uint8_t)(1u << channel)) != 0u,
               "CAN recovers after 128 occurrences of 11 recessive bits");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x0093u);
    }
}

static void receive_pps_cases(CanConformance* state, Dspic33* cpu) {
    expect(state, !dspic33_can_input_pin(cpu, 63u, true, 0u),
           "CAN input rejects non-remappable pin");
    for (uint8_t transmit_channel = 0u; transmit_channel < DSPIC33_CAN_COUNT;
         transmit_channel++) {
        uint8_t receive_channel = (uint8_t)(transmit_channel ^ 1u);
        uint8_t pin = (uint8_t)(64u + transmit_channel);
        uint16_t transmit_base = bases[transmit_channel];
        uint16_t receive_base = bases[receive_channel];
        uint32_t transmit_memory = (uint32_t)(0xd800u + transmit_channel * 0x100u);
        uint32_t receive_memory = (uint32_t)(0xda00u + transmit_channel * 0x100u);
        Dspic33CanFrame input =
            frame(transmit_channel == 0u ? 0x345u : 0x1234567u, transmit_channel != 0u,
                  false, 3u, (uint8_t)(0x60u + transmit_channel * 0x10u));
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
        dspic33_write_word(cpu, 0x06d4u, (uint16_t)(pin | ((uint16_t)pin << 8u)));
        configure_receive(cpu, receive_channel, receive_memory, 4u, 0u);
        configure_filter(cpu, receive_channel, 0u, input.identifier, input.extended,
                         input.extended ? 0x1fffffffu : 0x7ffu, true, 0u, 0u);
        enable_filter(cpu, receive_channel, 1u);
        configure_transmit(cpu, transmit_channel, transmit_memory);
        write_transmit_frame(cpu, transmit_memory, &input);
        select_window(cpu, transmit_channel, false);
        select_window(cpu, receive_channel, false);
        dspic33_write_word(cpu, (uint16_t)(transmit_base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(transmit_base + 0x12u),
                           transmit_channel == 0u ? 0u : 0x0311u);
        dspic33_write_word(cpu, (uint16_t)(receive_base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(receive_base + 0x12u),
                           transmit_channel == 0u ? 0u : 0x0311u);
        set_mode(cpu, transmit_channel, 0u);
        set_mode(cpu, receive_channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(transmit_base + 0x30u), 0x008bu);
        bool acknowledge_observed = false;
        expect(state,
               dspic33_device_advance(cpu, 8u) &&
                   bridge_can_pins(
                       cpu, transmit_channel, pin, (uint8_t)(65u - transmit_channel),
                       transmit_channel == 0u ? 4u : 10u, -1, &acknowledge_observed),
               "CAN PPS serial frame bridge advances");
        expect(state,
               receive_full(cpu, receive_channel, 0u) &&
                   cpu->io.can_rx_serial_count[receive_channel] != 0u &&
                   (cpu->io.can_rx_serial_active & (uint8_t)(1u << receive_channel)) ==
                       0u,
               "CAN PPS receiver accepts a complete stuffed frame");
        expect(state,
               memory_word(cpu, receive_memory) ==
                       (uint16_t)(((input.extended ? (input.identifier >> 18u) & 0x7ffu
                                                   : input.identifier)
                                   << 2u) |
                                  (input.extended ? 3u : 0u)) &&
                   (uint8_t)memory_word(cpu, receive_memory + 6u) == input.data[0] &&
                   (uint8_t)(memory_word(cpu, receive_memory + 6u) >> 8u) ==
                       input.data[1],
               "CAN PPS receiver preserves header and payload bits");
        expect(
            state,
            acknowledge_observed &&
                (dspic33_read_word(cpu, (uint16_t)(transmit_base + 0x30u)) & 0x0010u) ==
                    0u &&
                (dspic33_read_word(cpu, (uint16_t)(transmit_base + 0x0eu)) >> 8u) == 0u,
            "CAN PPS receiver drives the acknowledge slot dominant");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x0680u, 0x0f0eu);
    dspic33_write_word(cpu, 0x06d4u, 0x4000u);
    configure_receive(cpu, 1u, 0xdc00u, 4u, 0u);
    configure_filter(cpu, 1u, 0u, 0u, false, 0x7ffu, true, 0u, 0u);
    enable_filter(cpu, 1u, 1u);
    configure_transmit(cpu, 0u, 0xde00u);
    Dspic33CanFrame invalid = frame(0u, false, false, 0u, 0u);
    write_transmit_frame(cpu, 0xde00u, &invalid);
    select_window(cpu, 0u, false);
    select_window(cpu, 1u, false);
    dspic33_write_word(cpu, 0x050cu, 0x0080u);
    set_mode(cpu, 0u, 0u);
    set_mode(cpu, 1u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    bool acknowledge_observed = false;
    expect(state,
           dspic33_device_advance(cpu, 8u) &&
               bridge_can_pins(cpu, 0u, 64u, 65u, 4u, 42, &acknowledge_observed),
           "corrupted CAN PPS frame advances");
    expect(state,
           (dspic33_read_word(cpu, 0x050au) & 0x0080u) != 0u &&
               !receive_full(cpu, 1u, 0u),
           "corrupted CAN PPS frame raises IVRIF without receive data");
}

static void receive_pps_qualification_cases(CanConformance* state, Dspic33* cpu) {
    static const uint8_t modes[] = {0u, 3u, 7u, 2u, 4u, 1u};
    for (uint8_t index = 0u; index < sizeof(modes); index++) {
        uint8_t mode = modes[index];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e30u, 0xffffu);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_write_word(cpu, 0x06d4u, 64u);
        set_mode(cpu, 0u, mode);
        expect(state,
               dspic33_can_input_pin(cpu, 64u, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (((cpu->io.can_rx_serial_active & 1u) != 0u) ==
                    (mode == 0u || mode == 3u || mode == 7u)),
               "CAN receive mode qualifies physical start of frame");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xfffeu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_serial_active & 1u) == 0u,
           "CAN PPS receiver rejects an output pin");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0x0040u);
    dspic33_write_word(cpu, 0x06d4u, 70u);
    set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 70u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_serial_active & 1u) == 0u,
           "CAN PPS receiver rejects an analog pin");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 65u);
    set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_serial_active & 1u) == 0u,
           "CAN PPS receiver rejects an unmapped pin");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0760u, (uint16_t)(dspic33_read_word(cpu, 0x0760u) | 2u));
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (cpu->io.can_rx_serial_active & 1u) == 0u,
           "PMD suppresses the CAN PPS receiver");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_write_word(cpu, 0x0412u, 0u);
    set_mode(cpu, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x040au) & 0x0040u) == 0u,
           "disabled CAN wake filter rejects physical bus activity");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    dspic33_write_word(cpu, 0x0412u, 0x4000u);
    set_mode(cpu, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x040au) & 0x0040u) != 0u,
           "enabled CAN wake filter accepts physical bus activity");
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
    expect(state, dspic33_device_advance(cpu, 32768u), "priority transmissions");
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
               "unfiltered sleep activity schedule");
        expect(state, (dspic33_read_word(cpu, 0x040au) & 0x0040u) == 0u,
               "disabled CAN wake filter rejects sleep activity");
    }
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0412u, 0x4000u);
    dspic33_write_word(cpu, 0x040cu, 0x0040u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    {
        Dspic33CanFrame input = frame(0x234u, false, false, 1u, 0u);
        expect(state,
               dspic33_can_receive(cpu, 0u, &input, 0u) &&
                   dspic33_device_advance(cpu, 1u),
               "filtered sleep wake schedule");
        expect(state, (dspic33_read_word(cpu, 0x040au) & 0x0040u) != 0u,
               "enabled CAN wake filter raises wake flag");
    }
}

static void capture_timestamp_cases(CanConformance* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint32_t memory = (uint32_t)(0xc800u + channel * 0x100u);
        Dspic33CanFrame input = frame(0x345u, false, false, 1u, 0x5au);
        dspic33_reset(cpu, 0u);
        configure_receive(cpu, channel, memory, 4u, 0u);
        configure_filter(cpu, channel, 0u, 0x345u, false, 0x7ffu, true, 0u, 0u);
        enable_filter(cpu, channel, 1u);
        select_window(cpu, channel, false);
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x12u), 0u);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, 0x0148u, 0u);
        dspic33_write_word(cpu, 0x014au, 0u);
        dspic33_write_word(cpu, 0x0148u, 0x1c03u);
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) | 0x0008u));
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.input_capture.input_high & 2u) != 0u,
               "CAN timestamp pulse starts after frame acceptance");
        expect(state,
               dspic33_device_advance(cpu, 3u) &&
                   (cpu->io.input_capture.input_high & 2u) != 0u,
               "CAN timestamp pulse remains high before one bit time");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   cpu->io.input_capture.fifo[1].count == 1u &&
                   (cpu->io.input_capture.input_high & 2u) == 0u,
               "CAN timestamp pulse clears after one bit time");
        dspic33_write_word(cpu, 0x06aeu, 0x4000u);
        expect(state,
               dspic33_input_capture_pin(cpu, 64u, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_input_capture_pin(cpu, 64u, true, 0u) &&
                   dspic33_device_advance(cpu, 1u),
               "IC2 pin edge advances while CAN capture is selected");
        expect(state, cpu->io.input_capture.fifo[1].count == 1u,
               "CANCAP disconnects the physical IC2 pin");
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) & ~0x0008u));
        expect(state,
               dspic33_input_capture_pin(cpu, 64u, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_input_capture_pin(cpu, 64u, true, 0u) &&
                   dspic33_device_advance(cpu, 1u),
               "IC2 pin edge advances after CAN capture is cleared");
        expect(state, cpu->io.input_capture.fifo[1].count == 2u,
               "clearing CANCAP restores the physical IC2 pin");

        dspic33_reset(cpu, 0u);
        configure_receive(cpu, channel, memory, 4u, 0u);
        configure_filter(cpu, channel, 0u, 0x345u, false, 0x7ffu, true, 0u, 0u);
        enable_filter(cpu, channel, 1u);
        select_window(cpu, channel, false);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, 0x0148u, 0x1c03u);
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) &&
                   dspic33_device_advance(cpu, 32u),
               "disabled CAN timestamp receive schedule");
        expect(state, cpu->io.input_capture.fifo[1].count == 0u,
               "CANCAP clear preserves the IC2 pin source");
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
                              uint16_t expected_counts, uint16_t expected_status) {
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
    expect(state, (status & CAN_INTERRUPT_ERROR) == 0u,
           "B1 error transition leaves ERRIF clear");
    expect(state, !interrupt_flag(cpu, event_irqs[channel]),
           "B1 error transition does not interrupt");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "B1 error transition keeps default vector");
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
                                  expected_error_status(transmit, count, false));
            }
        }
    }
}

static void receive_error_transition_cases(CanConformance* state, Dspic33* cpu,
                                           uint8_t channel) {
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, false, 95u, 0x005fu, 0u);
    expect_error_step(state, cpu, channel, false, 1u, 0x0060u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, false, 1u, 0x0061u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING);
    expect_error_step(state, cpu, channel, false, 31u, 0x0080u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, false, 1u, 0x0081u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    expect_error_step(state, cpu, channel, false, 126u, 0x00ffu,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    expect_error_step(state, cpu, channel, false, 1u, 0x00ffu,
                      CAN_ERROR_WARNING | CAN_RECEIVE_PASSIVE);
    set_mode(cpu, channel, 4u);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0u,
           "configuration clears receive error counter");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) &
            CAN_ERROR_STATUS_MASK) == 0u,
           "configuration clears receive error state");
    set_mode(cpu, channel, 0u);
    expect_error_step(state, cpu, channel, false, 96u, 0x0060u,
                      CAN_ERROR_WARNING | CAN_RECEIVE_WARNING);
}

static void transmit_error_transition_cases(CanConformance* state, Dspic33* cpu,
                                            uint8_t channel) {
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, true, 95u, 0x5f00u, 0u);
    expect_error_step(state, cpu, channel, true, 1u, 0x6000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0x6100u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING);
    expect_error_step(state, cpu, channel, true, 31u, 0x8000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0x8100u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    expect_error_step(state, cpu, channel, true, 126u, 0xff00u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    expect_error_step(state, cpu, channel, true, 1u, 0xff00u,
                      CAN_ERROR_WARNING | CAN_BUS_OFF);
    clear_error_interrupt(state, cpu, channel);
    expect_error_step(state, cpu, channel, true, 1u, 0xff00u,
                      CAN_ERROR_WARNING | CAN_BUS_OFF);
    set_mode(cpu, channel, 4u);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0u,
           "configuration clears transmit error counter");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) &
            CAN_ERROR_STATUS_MASK) == 0u,
           "configuration clears bus off state");
    set_mode(cpu, channel, 0u);
    expect_error_step(state, cpu, channel, true, 96u, 0x6000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING);
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
    expect(state, dspic33_device_advance(cpu, 4096u),
           "error recovery transmission advance");
    expect(state, dspic33_can_transmit(cpu, channel, &output),
           "error recovery transmission output");
}

static void transmit_error_descending_entry_cases(CanConformance* state, Dspic33* cpu,
                                                  uint8_t channel) {
    uint16_t status_address = (uint16_t)(bases[channel] + 0x0au);
    configure_error_test(cpu, channel);
    expect_error_step(state, cpu, channel, true, 128u, 0x8000u,
                      CAN_ERROR_WARNING | CAN_TRANSMIT_PASSIVE);
    clear_error_interrupt(state, cpu, channel);
    complete_error_test_transmission(state, cpu, channel);
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0eu)) == 0x7f00u,
           "successful transmission decrements error counter");
    expect(state,
           (dspic33_read_word(cpu, status_address) & CAN_ERROR_STATUS_MASK) ==
               (CAN_ERROR_WARNING | CAN_TRANSMIT_WARNING),
           "successful transmission enters error warning");
    expect(state, (dspic33_read_word(cpu, status_address) & CAN_INTERRUPT_ERROR) == 0u,
           "B1 descending error transition leaves ERRIF clear");
    expect(state, !interrupt_flag(cpu, event_irqs[channel]),
           "B1 descending error transition does not interrupt");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 4u)) & 0x007fu) == 0x40u,
           "B1 descending error transition keeps default vector");
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

static void invalid_message_cases(CanConformance* state, Dspic33* cpu) {
    expect(state, !dspic33_can_invalid(cpu, DSPIC33_CAN_COUNT, 0u),
           "invalid CAN message rejects unavailable channel");
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0x0080u);
        clear_interrupt_flag(cpu, event_irqs[channel]);
        expect(state, dspic33_can_invalid(cpu, channel, 2u),
               "schedule invalid CAN message");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u &&
                   !interrupt_flag(cpu, event_irqs[channel]),
               "invalid CAN message waits for its event boundary");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) != 0u &&
                   interrupt_flag(cpu, event_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 4u)) & 0x007fu) == 0x40u,
               "invalid CAN message raises IVRIF and the event interrupt");
        dspic33_write_word(
            cpu, (uint16_t)(base + 0x0au),
            (uint16_t)(dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & ~0x0080u));
        clear_interrupt_flag(cpu, event_irqs[channel]);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u &&
                   !interrupt_flag(cpu, event_irqs[channel]),
               "software clears the invalid CAN message event");

        dspic33_reset(cpu, 0u);
        expect(state,
               dspic33_can_invalid(cpu, channel, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u,
               "configuration mode suppresses invalid CAN message events");

        dspic33_reset(cpu, 0u);
        set_mode(cpu, channel, 0u);
        dspic33_write_word(
            cpu, 0x0760u,
            (uint16_t)(dspic33_read_word(cpu, 0x0760u) | (uint16_t)(2u << channel)));
        expect(state,
               dspic33_can_invalid(cpu, channel, 0u) &&
                   dspic33_device_advance(cpu, 1u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 0x0080u) == 0u,
               "PMD-disabled CAN suppresses invalid message events");
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
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, 14u);
    configure_transmit(cpu, 0u, 0xd100u);
    write_memory_word(cpu, 0xd100u, 2u);
    for (uint8_t word = 1u; word < 8u; word++) {
        write_memory_word(cpu, 0xd100u + word * 2u, 0u);
    }
    select_window(cpu, 0u, false);
    set_mode(cpu, 0u, 0u);
    dspic33_write_word(cpu, 0x0430u, 0x008bu);
    expect(state, dspic33_device_advance(cpu, 8u) && (cpu->io.can_tx_busy & 1u) != 0u,
           "copy reaches pending CAN bus completion");
    expect(state, dspic33_copy(&copy, cpu), "copy pending CAN bus state");
    expect(state,
           dspic33_device_advance(cpu, 20u) && dspic33_device_advance(&copy, 20u),
           "copied CAN bus phases advance");
    bool source_high;
    bool copy_high;
    expect(state,
           dspic33_can_pin(cpu, 64u, &source_high) &&
               dspic33_can_pin(&copy, 64u, &copy_high) && source_high && copy_high,
           "copy preserves CAN transmit bit phase");
    expect(state,
           dspic33_device_advance(cpu, 980u) && dspic33_device_advance(&copy, 980u),
           "copied CAN bus completions advance");
    Dspic33CanFrame source_output;
    Dspic33CanFrame copy_output;
    expect(state,
           dspic33_can_transmit(cpu, 0u, &source_output) &&
               dspic33_can_transmit(&copy, 0u, &copy_output) &&
               source_output.identifier == copy_output.identifier,
           "copy preserves pending CAN bus completion");
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0xffffu);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_write_word(cpu, 0x06d4u, 64u);
    set_mode(cpu, 0u, 0u);
    expect(state,
           dspic33_can_input_pin(cpu, 64u, false, 0u) &&
               dspic33_device_advance(cpu, 3u) &&
               cpu->io.can_rx_serial_count[0] == 1u &&
               (cpu->io.can_rx_serial_active & 1u) != 0u,
           "copy reaches active CAN serial reception");
    expect(state, dspic33_copy(&copy, cpu), "copy active CAN serial state");
    expect(state,
           dspic33_can_input_pin(cpu, 64u, true, 1u) &&
               dspic33_can_input_pin(&copy, 64u, true, 1u) &&
               dspic33_device_advance(cpu, 4u) && dspic33_device_advance(&copy, 4u) &&
               cpu->io.can_rx_serial_count[0] == 2u &&
               copy.io.can_rx_serial_count[0] == 2u &&
               cpu->io.can_rx_serial_bits[0][1] == copy.io.can_rx_serial_bits[0][1],
           "copy preserves CAN serial receive phase");
    dspic33_destroy(&copy);
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.can_rx[0].count == 0u && cpu->io.can_tx[0].count == 0u &&
               cpu->io.can_rx_busy == 0u && cpu->io.can_tx_busy == 0u &&
               cpu->io.can_rx_serial_active == 0u &&
               cpu->io.can_rx_serial_count[0] == 0u && cpu->io.can_rx_pin_high == 3u &&
               cpu->io.can_rx_physical_active == 0u && cpu->io.can_rx_ack == 0u &&
               cpu->io.can_tx_retry_wait == 0u && cpu->io.can_tx_error_active == 0u &&
               cpu->io.can_rx_error_active == 0u &&
               cpu->io.can_bus_off_recessive_bits[0] == 0u &&
               cpu->io.can_bus_off_recessive_bits[1] == 0u,
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
    register_access_cases(&state, &cpu);
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
    clock_timing_cases(&state, &cpu);
    stuffed_frame_timing_cases(&state, &cpu);
    transmit_abort_timing_cases(&state, &cpu);
    transmit_pps_cases(&state, &cpu);
    receive_pps_cases(&state, &cpu);
    arbitration_field_cases(&state, &cpu);
    arbitration_cases(&state, &cpu);
    acknowledge_error_cases(&state, &cpu);
    transmit_error_variant_cases(&state, &cpu);
    receive_error_cases(&state, &cpu);
    bus_off_recovery_cases(&state, &cpu);
    receive_pps_qualification_cases(&state, &cpu);
    priority_and_abort_cases(&state, &cpu);
    mode_and_power_cases(&state, &cpu);
    capture_timestamp_cases(&state, &cpu);
    interrupt_and_error_cases(&state, &cpu);
    invalid_message_cases(&state, &cpu);
    copy_and_reset_cases(&state, &cpu);
    expect(&state, state.cases == 1462687u, "CAN assertion accounting");
    report_sfr_side_effect_coverage(
        "can", can_sfr_side_effect_coverage,
        SFR_SIDE_EFFECT_COVERAGE_COUNT(can_sfr_side_effect_coverage),
        state.failed == 0u);
    printf("[can-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    dspic33_destroy(&cpu);
    return state.failed == 0u ? 0 : 1;
}
