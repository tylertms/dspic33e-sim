#include "device/dspic33ep_mu/communication/can/internal.h"

const uint16_t bases[DSPIC33_CAN_COUNT] = {0x0400u, 0x0500u};
const uint8_t event_irqs[DSPIC33_CAN_COUNT] = {35u, 56u};
const uint8_t receive_requests[DSPIC33_CAN_COUNT] = {34u, 55u};
const uint8_t transmit_requests[DSPIC33_CAN_COUNT] = {70u, 71u};

bool dspic33_can_test_interrupt_flag(Dspic33* cpu, uint8_t interrupt_number) {
    const uint16_t status_address = (uint16_t)(0x0800u + (interrupt_number / 16u) * 2u);

    return (dspic33_read_word(cpu, status_address) & (uint16_t)(1u << (interrupt_number % 16u))) !=
           0u;
}

void dspic33_can_test_clear_interrupt_flag(Dspic33* cpu, uint8_t interrupt_number) {
    const uint16_t status_address = (uint16_t)(0x0800u + (interrupt_number / 16u) * 2u);
    const uint16_t interrupt_mask = (uint16_t)(1u << (interrupt_number % 16u));

    dspic33_write_word(cpu, status_address,
                       (uint16_t)(dspic33_read_word(cpu, status_address) & ~interrupt_mask));
}

static uint16_t dma_channel_base(uint8_t channel_index) {
    return (uint16_t)(0x0b00u + channel_index * 0x10u);
}

static void configure_dma(Dspic33* cpu, uint8_t channel_index, uint16_t dma_control,
                          uint8_t request_source, uint32_t memory_address,
                          uint16_t peripheral_address) {
    const uint16_t channel_base = dma_channel_base(channel_index);

    dspic33_write_word(cpu, channel_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 2u), request_source);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 4u), (uint16_t)memory_address);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 6u), (uint16_t)(memory_address >> 16u));
    dspic33_write_word(cpu, (uint16_t)(channel_base + 0x0cu), peripheral_address);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 0x0eu), 7u);
    dspic33_write_word(cpu, channel_base, (uint16_t)(0x8000u | dma_control));
}

void dspic33_can_test_select_window(Dspic33* cpu, uint8_t channel_index, bool filter_window) {
    const uint16_t can_base = bases[channel_index];
    const uint16_t can_control = dspic33_read_word(cpu, can_base);

    dspic33_write_word(cpu, can_base,
                       filter_window ? (uint16_t)(can_control | 1u)
                                     : (uint16_t)(can_control & ~1u));
}

uint64_t dspic33_can_test_mode_transition_cycles(Dspic33* cpu, uint8_t channel_index) {
    const uint16_t can_base = bases[channel_index];
    const uint16_t configuration_one = dspic33_read_word(cpu, (uint16_t)(can_base + 0x10u));
    const uint16_t configuration_two = dspic33_read_word(cpu, (uint16_t)(can_base + 0x12u));
    const uint16_t can_control = dspic33_read_word(cpu, can_base);
    const uint64_t prescaler = (configuration_one & 0x003fu) + 1u;
    const uint64_t time_quanta = 1u + (configuration_two & 7u) + 1u +
                                 ((configuration_two >> 3u) & 7u) + 1u +
                                 ((configuration_two >> 8u) & 7u) + 1u;
    const uint64_t clock_divisor = (can_control & 0x0800u) != 0u ? 2u : 1u;

    return 11u * prescaler * time_quanta * clock_divisor;
}

void dspic33_can_test_request_mode(Dspic33* cpu, uint8_t channel, uint8_t mode) {
    uint16_t base = bases[channel];
    uint16_t control = dspic33_read_word(cpu, base);
    control = (uint16_t)((control & ~0x0700u) | ((uint16_t)mode << 8u));
    dspic33_write_word(cpu, base, control);
}

void dspic33_can_test_set_mode(Dspic33* cpu, uint8_t channel, uint8_t mode) {
    dspic33_can_test_request_mode(cpu, channel, mode);
    dspic33_device_advance(cpu, dspic33_can_test_mode_transition_cycles(cpu, channel));
}

void dspic33_can_test_configure_filter(Dspic33* cpu, uint8_t channel, uint8_t filter,
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
    dspic33_can_test_select_window(cpu, channel, true);
    dspic33_write_word(cpu, (uint16_t)(base + 0x30u + mask_index * 4u), receive_mask);
    dspic33_write_word(cpu, (uint16_t)(base + 0x32u + mask_index * 4u), (uint16_t)mask_eid);
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

void dspic33_can_test_enable_filter(Dspic33* cpu, uint8_t channel, uint16_t enabled) {
    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 0x14u), enabled);
}

void dspic33_can_test_configure_receive(Dspic33* cpu, uint8_t channel, uint32_t memory,
                                        uint8_t dmabs, uint8_t fifo_start) {
    uint16_t base = bases[channel];
    configure_dma(cpu, (uint8_t)(channel * 2u), 0x0020u, receive_requests[channel], memory,
                  (uint16_t)(base + 0x40u));
    dspic33_write_byte(cpu, (uint16_t)(base + 6u), fifo_start);
    dspic33_write_byte(cpu, (uint16_t)(base + 7u), (uint8_t)(dmabs << 5u));
}

void dspic33_can_test_fifo_control_write_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t address = (uint16_t)(bases[channel] + 6u);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, address, 0xa012u);
        expect(state, dspic33_read_word(cpu, address) == 0u,
               "CAN FIFO control rejects a word write");
        dspic33_write_byte(cpu, (uint16_t)(address + 1u), 0xa0u);
        expect(state, dspic33_read_word(cpu, address) == 0u,
               "CAN FIFO control rejects DMABS before FSA");
        dspic33_write_byte(cpu, address, 0x12u);
        expect(state, dspic33_read_word(cpu, address) == 0x0012u,
               "CAN FIFO control accepts FSA first");
        dspic33_write_byte(cpu, (uint16_t)(address + 1u), 0xa0u);
        expect(state,
               dspic33_read_word(cpu, address) == 0xa012u &&
                   cpu->io.can_fifo_write[channel] == 0x12u &&
                   (cpu->io.can_fctrl_fsa_ready & (uint8_t)(1u << channel)) == 0u,
               "CAN FIFO control accepts DMABS after FSA");
    }
}

void dspic33_can_test_register_groups(TestState* state, Dspic33* cpu) {
    dspic33_can_test_register_cases(state, cpu);
    dspic33_can_test_register_access_cases(state, cpu);
    dspic33_can_test_fifo_control_write_cases(state, cpu);
    dspic33_can_test_interrupt_flag_write_zero_cases(state, cpu);
    dspic33_can_test_standard_filter_domain(state, cpu);
    dspic33_can_test_extended_filter_cases(state, cpu);
    dspic33_can_test_payload_and_remote_cases(state, cpu);
    dspic33_can_test_devicenet_cases(state, cpu);
    dspic33_can_test_direct_buffer_cases(state, cpu);
    dspic33_can_test_fifo_cases(state, cpu);
    dspic33_can_test_fifo_overflow_advancement_cases(state, cpu);
    dspic33_can_test_receive_flag_write_zero_domain(state, cpu);
}

void dspic33_can_test_configure_transmit(Dspic33* cpu, uint8_t channel, uint32_t memory) {
    configure_dma(cpu, (uint8_t)(channel * 2u + 1u), 0x2020u, transmit_requests[channel], memory,
                  (uint16_t)(bases[channel] + 0x42u));
}

Dspic33CanFrame dspic33_can_test_frame(uint32_t identifier, bool extended, bool remote,
                                       uint8_t length, uint8_t seed) {
    Dspic33CanFrame result;
    uint8_t index;
    memset(&result, 0, sizeof(result));
    result.identifier = identifier;
    result.extended = extended;
    result.remote = remote;
    result.length = length;
    for (index = 0u; index < length && index < sizeof(result.data); index++) {
        result.data[index] = (uint8_t)(seed + index * 17u);
    }
    return result;
}

void dspic33_can_test_write_transmit_frame(Dspic33* cpu, uint32_t memory,
                                           const Dspic33CanFrame* value) {
    uint16_t words[8] = {0};
    uint32_t sid = value->extended ? (value->identifier >> 18u) & 0x7ffu : value->identifier;
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
        dspic33_can_test_write_memory_word(cpu, memory + index * 2u, words[index]);
    }
}

static void clear_receive_flag(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t address = (uint16_t)(bases[channel] + 0x20u + (buffer >= 16u ? 2u : 0u));
    uint16_t value = dspic33_read_word(cpu, address);
    dspic33_write_word(cpu, address, (uint16_t)(value & ~(uint16_t)(1u << (buffer & 15u))));
}

bool dspic33_can_test_receive_full(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t address = (uint16_t)(bases[channel] + 0x20u + (buffer >= 16u ? 2u : 0u));
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (buffer & 15u))) != 0u;
}

uint16_t dspic33_can_test_memory_word(Dspic33* cpu, uint32_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

void dspic33_can_test_write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
}

void dspic33_can_test_register_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        expect(state, dspic33_read_word(cpu, base) == 0x0480u, "control reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0x0040u, "vector reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x14u)) == 0x003fu,
               "filter enable reset");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x001fu, "control two mask");
        dspic33_write_byte(cpu, (uint16_t)(base + 6u), 0xffu);
        dspic33_write_byte(cpu, (uint16_t)(base + 7u), 0xffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0xe01fu,
               "fifo control byte masks");
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0cu)) == 0x00efu,
               "interrupt enable mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x10u)) == 0x00ffu, "baud one mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x12u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x12u)) == 0x47ffu, "baud two mask");
        dspic33_can_test_select_window(cpu, channel, true);
        dspic33_write_word(cpu, (uint16_t)(base + 0x40u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x40u)) == 0xffebu,
               "filter SID mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x42u), 0xa55au);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x42u)) == 0xa55au,
               "filter EID write");
        dspic33_can_test_select_window(cpu, channel, false);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x40u)) == 0u, "window isolation");
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) == 0x8f8fu,
               "buffer control mask");
        dspic33_can_test_set_mode(cpu, channel, 0u);
        expect(state, (dspic33_read_word(cpu, base) & 0x00e0u) == 0u,
               "normal mode acknowledgement");
        dspic33_write_word(cpu, (uint16_t)(base + 0x10u), 0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x10u)) == 0x00ffu,
               "baud locked outside configuration");
        dspic33_can_test_set_mode(cpu, channel, 4u);
    }
}

void dspic33_can_test_register_access_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t received_words[] = {0x0c84u, 0x0000u, 0x0004u, 0x5140u,
                                              0x7362u, 0x0000u, 0x0000u, 0x0000u};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t receive_data = (uint16_t)(base + 0x40u);
        uint32_t memory = (uint32_t)(0xf600u + channel * 0x100u);
        Dspic33CanFrame input = dspic33_can_test_frame(0x321u, false, false, 4u, 0x40u);
        uint8_t index;
        bool preserved;
        bool active;
        bool exact;

        dspic33_reset(cpu, 0u);
        dspic33_can_test_write_memory_word(cpu, (uint16_t)(base + 0x0eu), 0x5aa5u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), 0u);
        preserved = dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0x5aa5u;
        dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), 0xffffu);
        expect(state, preserved && dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0x5aa5u,
               "error counters reject CPU writes");

        dspic33_write_word(cpu, base,
                           (uint16_t)((dspic33_read_word(cpu, base) & ~0x07e0u) | 0x02e0u));
        expect(state,
               (dspic33_read_word(cpu, base) & 0x07e0u) == 0x0280u &&
                   dspic33_device_advance(cpu,
                                          dspic33_can_test_mode_transition_cycles(cpu, channel)) &&
                   (dspic33_read_word(cpu, base) & 0x07e0u) == 0x0240u,
               "requested mode controls operating mode");
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) | 0x00a0u));
        expect(state, (dspic33_read_word(cpu, base) & 0x07e0u) == 0x0240u,
               "operating mode rejects direct writes");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), 1u);
        dspic33_write_word(cpu, (uint16_t)(base + 0x30u), 0x8989u);
        dspic33_write_word(cpu, base, (uint16_t)(dspic33_read_word(cpu, base) | 0x1000u));
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 0x30u)) & 0x4848u) == 0x4040u,
               "abort all marks pending transmissions aborted");
        expect(state,
               (dspic33_read_word(cpu, base) & 0x1000u) == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 1u) != 0u &&
                   dspic33_can_test_interrupt_flag(cpu, event_irqs[channel]),
               "abort all self clears and raises transmit event");

        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, channel, memory, 4u, 0u);
        dspic33_can_test_configure_filter(cpu, channel, 0u, 0x321u, false, 0x7ffu, true, 0u, 0u);
        dspic33_can_test_enable_filter(cpu, channel, 1u);
        dspic33_can_test_select_window(cpu, channel, false);
        dspic33_can_test_set_mode(cpu, channel, 0u);
        dspic33_write_word(cpu, receive_data, 0xa55au);
        expect(state, dspic33_read_word(cpu, receive_data) == 0xa55au,
               "receive data CPU word access");
        dspic33_write_byte(cpu, receive_data, 0x3cu);
        dspic33_write_byte(cpu, (uint16_t)(receive_data + 1u), 0xc3u);
        expect(state, dspic33_read_word(cpu, receive_data) == 0xc33cu,
               "receive data CPU byte access");
        active = dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 0u);
        expect(state,
               active && (cpu->io.can_rx_busy & (uint8_t)(1u << channel)) != 0u &&
                   dspic33_read_word(cpu, receive_data) == received_words[0] &&
                   dspic33_can_test_memory_word(cpu, memory) == received_words[0],
               "receive stream overrides CPU backing for DMA");
        dspic33_write_word(cpu, receive_data, 0xc55cu);
        expect(state, dspic33_read_word(cpu, receive_data) == received_words[0],
               "receive stream survives concurrent CPU write");
        expect(state, dspic33_device_advance(cpu, 32u), "receive stream completion advance");
        exact = true;
        for (index = 0u; index < 8u; index++) {
            exact = exact &&
                    dspic33_can_test_memory_word(cpu, memory + index * 2u) == received_words[index];
        }
        expect(state, exact, "receive stream DMA words");
        expect(state,
               (cpu->io.can_rx_busy & (uint8_t)(1u << channel)) == 0u &&
                   dspic33_read_word(cpu, receive_data) == 0xc55cu,
               "receive data backing returns after stream");
    }
}

void dspic33_can_test_interrupt_flag_write_zero_cases(TestState* state, Dspic33* cpu) {
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
            for (requested = 0u; requested <= UINT16_MAX; requested += 257u) {
                dspic33_can_test_write_memory_word(cpu, address, previous);
                dspic33_write_word(cpu, address, (uint16_t)requested);
                expect(state, dspic33_read_word(cpu, address) == (uint16_t)(previous & requested),
                       "interrupt flag write-zero clear domain");
            }
        }
    }
}

void dspic33_can_test_standard_filter_domain(TestState* state, Dspic33* cpu) {
    uint16_t identifier;
    uint16_t expected = 0x5a0u;
    uint16_t mask = 0x7f0u;
    Dspic33CanFrame input;
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, 0u, 0x2000u, 4u, 0u);
    dspic33_can_test_configure_filter(cpu, 0u, 0u, expected, false, mask, true, 0u, 0u);
    dspic33_can_test_enable_filter(cpu, 0u, 1u);
    dspic33_can_test_select_window(cpu, 0u, false);
    dspic33_can_test_set_mode(cpu, 0u, 0u);
    for (identifier = 0u; identifier <= 0x7ffu; identifier++) {
        bool accepted = (identifier & mask) == (expected & mask);
        input = dspic33_can_test_frame(identifier, false, false, 2u, (uint8_t)identifier);
        expect(state, dspic33_can_receive(cpu, 0u, &input, 0u), "standard domain schedule");
        expect(state, dspic33_device_advance(cpu, 32u), "standard domain advance");
        expect(state, dspic33_can_test_receive_full(cpu, 0u, 0u) == accepted,
               "standard domain filter result");
        if (accepted) {
            expect(state,
                   dspic33_can_test_memory_word(cpu, 0x2000u) == (uint16_t)(identifier << 2u),
                   "standard domain identifier storage");
            clear_receive_flag(cpu, 0u, 0u);
        }
    }
}

void dspic33_can_test_extended_filter_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t identifiers[] = {0u,         1u,          0x3ffffu,    0x40000u,
                                           0x1ffffffu, 0x10000000u, 0x15555555u, 0x1fffffffu};
    uint8_t channel;
    uint8_t index;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (index = 0u; index < sizeof(identifiers) / sizeof(identifiers[0]); index++) {
            uint32_t identifier = identifiers[index];
            Dspic33CanFrame input = dspic33_can_test_frame(identifier, true, false, 8u, index);
            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_receive(cpu, channel, 0x3000u, 6u, 0u);
            dspic33_can_test_configure_filter(cpu, channel, 3u, identifier, true, 0x1fffffffu, true,
                                              6u, 1u);
            dspic33_can_test_enable_filter(cpu, channel, 1u << 3u);
            dspic33_can_test_select_window(cpu, channel, false);
            dspic33_can_test_set_mode(cpu, channel, 0u);
            expect(state,
                   dspic33_can_receive(cpu, channel, &input, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "extended exact transfer");
            expect(state, dspic33_can_test_receive_full(cpu, channel, 6u),
                   "extended exact full flag");
            expect(state,
                   (((uint32_t)(dspic33_can_test_memory_word(cpu, 0x3060u) >> 2u) & 0x7ffu)
                    << 18u) == (identifier & 0x1ffc0000u),
                   "extended SID storage");
            expect(state,
                   (((uint32_t)(dspic33_can_test_memory_word(cpu, 0x3062u) & 0x0fffu) << 6u) |
                    (dspic33_can_test_memory_word(cpu, 0x3064u) >> 10u)) == (identifier & 0x3ffffu),
                   "extended EID storage");
        }
    }
}

void dspic33_can_test_payload_and_remote_cases(TestState* state, Dspic33* cpu) {
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
                        dspic33_can_test_frame(identifier, extended != 0u, remote != 0u, length,
                                               (uint8_t)(0x20u + length));
                    uint8_t index;
                    dspic33_reset(cpu, 0u);
                    dspic33_can_test_configure_receive(cpu, channel, 0x4000u, 4u, 0u);
                    dspic33_can_test_configure_filter(cpu, channel, 0u, identifier, extended != 0u,
                                                      extended != 0u ? 0x1fffffffu : 0x7ffu, true,
                                                      2u, 0u);
                    dspic33_can_test_enable_filter(cpu, channel, 1u);
                    dspic33_can_test_select_window(cpu, channel, false);
                    dspic33_can_test_set_mode(cpu, channel, 0u);
                    expect(state,
                           dspic33_can_receive(cpu, channel, &input, 0u) &&
                               dspic33_device_advance(cpu, 32u),
                           "payload transfer");
                    expect(state, (dspic33_can_test_memory_word(cpu, 0x4024u) & 0x0fu) == length,
                           "payload length");
                    for (index = 0u; index < length; index++) {
                        uint16_t word = dspic33_can_test_memory_word(
                            cpu, (uint32_t)(0x4026u + (index / 2u) * 2u));
                        expect(state, (uint8_t)(word >> ((index & 1u) * 8u)) == input.data[index],
                               "payload byte");
                    }
                    expect(state,
                           extended != 0u
                               ? ((dspic33_can_test_memory_word(cpu, 0x4024u) & 0x0200u) != 0u) ==
                                     (remote != 0u)
                               : ((dspic33_can_test_memory_word(cpu, 0x4020u) & 2u) != 0u) ==
                                     (remote != 0u),
                           "remote encoding");
                }
            }
        }
    }
}

void dspic33_can_test_devicenet_cases(TestState* state, Dspic33* cpu) {
    uint8_t bits;
    uint8_t length;
    for (bits = 1u; bits <= 31u; bits++) {
        for (length = 0u; length <= 3u; length++) {
            Dspic33CanFrame input = dspic33_can_test_frame(0x321u, false, false, length, 0xa5u);
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
            dspic33_can_test_configure_receive(cpu, 0u, 0x5000u, 4u, 0u);
            dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x321u, false, 0x7ffu, true, 0u, 0u);
            dspic33_can_test_select_window(cpu, 0u, true);
            dspic33_write_word(cpu, 0x0430u, (uint16_t)((0x7ffu << 5u) | 8u));
            dspic33_write_word(cpu, 0x0440u, (uint16_t)((0x321u << 5u) | (data >> 16u)));
            dspic33_write_word(cpu, 0x0442u, (uint16_t)data);
            dspic33_can_test_enable_filter(cpu, 0u, 1u);
            dspic33_write_word(cpu, 0x0402u, bits);
            dspic33_can_test_select_window(cpu, 0u, false);
            dspic33_can_test_set_mode(cpu, 0u, 0u);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
                   "DeviceNet matching transfer");
            expect(state, dspic33_can_test_receive_full(cpu, 0u, 0u), "DeviceNet match");
        }
    }
}

void dspic33_can_test_direct_buffer_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t buffer;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        for (buffer = 0u; buffer <= 14u; buffer++) {
            Dspic33CanFrame input =
                dspic33_can_test_frame((uint32_t)(0x200u + buffer), false, false, 4u, buffer);
            dspic33_reset(cpu, 0u);
            dspic33_can_test_configure_receive(cpu, channel, 0x6000u, 6u, 0u);
            dspic33_can_test_configure_filter(cpu, channel, 0u, input.identifier, false, 0x7ffu,
                                              true, buffer, 0u);
            dspic33_can_test_enable_filter(cpu, channel, 1u);
            dspic33_can_test_select_window(cpu, channel, false);
            dspic33_can_test_set_mode(cpu, channel, 0u);
            expect(state,
                   dspic33_can_receive(cpu, channel, &input, 0u) &&
                       dspic33_device_advance(cpu, 32u),
                   "direct buffer transfer");
            expect(state, dspic33_can_test_receive_full(cpu, channel, buffer),
                   "direct buffer full");
            expect(state,
                   dspic33_can_test_memory_word(cpu, (uint32_t)(0x6000u + buffer * 16u)) ==
                       (uint16_t)(input.identifier << 2u),
                   "direct buffer address");
            expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 0x0au)) & 2u) != 0u,
                   "direct receive event");
        }
    }
}

void dspic33_can_test_fifo_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t sizes[] = {4u, 6u, 8u, 12u, 16u, 24u, 32u};
    uint8_t selection;
    for (selection = 0u; selection < sizeof(sizes); selection++) {
        uint8_t size = sizes[selection];
        uint8_t start = (uint8_t)(size / 2u);
        uint8_t count = (uint8_t)(size - start);
        uint8_t index;
        dspic33_reset(cpu, 0u);
        dspic33_can_test_configure_receive(cpu, 0u, 0x7000u, selection, start);
        dspic33_can_test_configure_filter(cpu, 0u, 0u, 0x456u, false, 0x7ffu, true, 15u, 0u);
        dspic33_can_test_enable_filter(cpu, 0u, 1u);
        dspic33_can_test_select_window(cpu, 0u, false);
        dspic33_can_test_set_mode(cpu, 0u, 0u);
        for (index = 0u; index < count; index++) {
            Dspic33CanFrame input = dspic33_can_test_frame(0x456u, false, false, 1u, index);
            uint8_t buffer = (uint8_t)(start + index);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &input, 0u) && dspic33_device_advance(cpu, 32u),
                   "FIFO transfer");
            expect(state, dspic33_can_test_receive_full(cpu, 0u, buffer), "FIFO full sequence");
            expect(state,
                   dspic33_can_test_memory_word(cpu, (uint32_t)(0x7000u + buffer * 16u + 6u)) ==
                       index,
                   "FIFO payload sequence");
        }
        expect(state, ((dspic33_read_word(cpu, 0x0408u) >> 8u) & 0x3fu) == start,
               "FIFO write pointer wrap");
        {
            Dspic33CanFrame overflow = dspic33_can_test_frame(0x456u, false, false, 1u, 0xeeu);
            expect(state,
                   dspic33_can_receive(cpu, 0u, &overflow, 0u) && dspic33_device_advance(cpu, 2u),
                   "FIFO overflow transfer attempt");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(0x0428u + (start >= 16u ? 2u : 0u))) &
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
    value = (uint16_t)((value & ~(uint16_t)(0xffu << shift)) | (uint16_t)(0x80u << shift));
    dspic33_write_word(cpu, address, value);
}

static bool transmit_buffer_selected(Dspic33* cpu, uint8_t channel, uint8_t buffer) {
    uint16_t address = (uint16_t)(bases[channel] + 0x30u + (buffer / 2u) * 2u);
    uint8_t shift = (uint8_t)((buffer & 1u) * 8u);
    return ((dspic33_read_word(cpu, address) >> shift) & 0x80u) != 0u;
}

static void fifo_overflow_advancement_case(TestState* state, Dspic33* cpu, uint8_t channel,
                                           bool listen_all, bool full, bool wrap) {
    uint16_t base = bases[channel];
    uint32_t memory = (uint32_t)(0xf000u + channel * 0x100u);
    Dspic33CanFrame input = dspic33_can_test_frame(0x456u, false, false, 1u, 0x80u);
    uint8_t buffer = wrap ? 3u : 2u;
    uint8_t expected_next = wrap ? 2u : 3u;
    uint16_t flag = (uint16_t)(1u << buffer);
    uint16_t fnrb;
    dspic33_reset(cpu, 0u);
    dspic33_can_test_configure_receive(cpu, channel, memory, 0u, 2u);
    dspic33_can_test_configure_filter(cpu, channel, 0u, input.identifier, false, 0x7ffu, true, 15u,
                                      0u);
    dspic33_can_test_enable_filter(cpu, channel, 1u);
    dspic33_can_test_select_window(cpu, channel, false);
    dspic33_can_test_set_mode(cpu, channel, listen_all ? 7u : 0u);
    if (wrap) {
        expect(state,
               dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 32u),
               "FIFO overflow wrap preparation");
        expect(state, ((dspic33_read_word(cpu, (uint16_t)(base + 8u)) >> 8u) & 0x3fu) == buffer,
               "FIFO overflow wrap pointer preparation");
        clear_receive_flag(cpu, channel, 2u);
    }
    if (full) {
        dspic33_can_test_write_memory_word(cpu, (uint16_t)(base + 0x20u), flag);
    } else {
        select_transmit_buffer(cpu, channel, buffer);
    }
    dspic33_can_test_write_memory_word(cpu, memory + buffer * 16u, 0xa55au);
    fnrb = (uint16_t)(dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x003fu);
    expect(state, dspic33_can_receive(cpu, channel, &input, 0u) && dspic33_device_advance(cpu, 2u),
           "FIFO overflow receive attempt");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 0x28u)) & flag) != 0u,
           "FIFO overflow buffer flag");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) & 4u) != 0u,
           "FIFO overflow event flag");
    expect(state, ((dspic33_read_word(cpu, (uint16_t)(base + 8u)) >> 8u) & 0x3fu) == expected_next,
           "FIFO overflow advances write pointer");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x003fu) == fnrb,
           "FIFO overflow preserves next read pointer");
    expect(state, dspic33_can_test_memory_word(cpu, memory + buffer * 16u) == 0xa55au,
           "FIFO overflow loses message");
    expect(state, dspic33_can_test_receive_full(cpu, channel, buffer) == full,
           "FIFO overflow preserves selected buffer state");
    expect(state, full || transmit_buffer_selected(cpu, channel, buffer),
           "FIFO overflow preserves transmit selection");
}

void dspic33_can_test_fifo_overflow_advancement_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t mode;
        for (mode = 0u; mode < 2u; mode++) {
            uint8_t cause;
            for (cause = 0u; cause < 2u; cause++) {
                uint8_t wrap;
                for (wrap = 0u; wrap < 2u; wrap++) {
                    fifo_overflow_advancement_case(state, cpu, channel, mode != 0u, cause == 0u,
                                                   wrap != 0u);
                }
            }
        }
    }
}

void dspic33_can_test_receive_flag_write_zero_domain(TestState* state, Dspic33* cpu) {
    static const uint8_t offsets[] = {0x20u, 0x22u, 0x28u, 0x2au};
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_CAN_COUNT; channel++) {
        uint8_t index;
        dspic33_reset(cpu, 0u);
        dspic33_can_test_select_window(cpu, channel, false);
        for (index = 0u; index < sizeof(offsets); index++) {
            uint16_t address = (uint16_t)(bases[channel] + offsets[index]);
            uint32_t requested;
            for (requested = 0u; requested <= UINT16_MAX; requested += 257u) {
                dspic33_can_test_write_memory_word(cpu, address, UINT16_MAX);
                dspic33_write_word(cpu, address, (uint16_t)requested);
                expect(state, dspic33_read_word(cpu, address) == requested,
                       "receive flag write-zero domain");
            }
        }
    }
}
