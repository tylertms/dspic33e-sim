#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

enum {
    DMA_FORCE = 0x8000u,
    DMA_TEST_READ_PAD = 0x0290u,
    DMA_TEST_WRITE_PAD = 0x0298u,
    DMA_TEST_ALT_WRITE_PAD = 0x0904u,
    DMA_TEST_BIDIRECTIONAL_PAD = 0x0608u,
    OPCODE_MOV_W1_POST_INCREMENT_W2 = 0x780131u
};

static const uint16_t readable_pads[] = {0x0144u, 0x014cu, 0x0154u, 0x015cu, 0x0226u, 0x0236u,
                                         0x0248u, 0x0256u, 0x0268u, 0x0290u, 0x02a8u, 0x02b6u,
                                         0x02c8u, 0x0300u, 0x0340u, 0x0440u, 0x0540u, 0x0608u};

static const uint16_t writable_pads[] = {
    0x0224u, 0x0234u, 0x0248u, 0x0254u, 0x0268u, 0x0298u, 0x02a8u, 0x02b4u, 0x02c8u, 0x0442u,
    0x0542u, 0x0608u, 0x0904u, 0x0906u, 0x090eu, 0x0910u, 0x0918u, 0x091au, 0x0922u, 0x0924u};

static const uint8_t dma_irqs[DSPIC33_DMA_COUNT] = {4u,   14u,  24u,  36u,  46u,  61u,  68u, 69u,
                                                    118u, 119u, 120u, 121u, 130u, 131u, 132u};

static uint16_t channel_base(uint8_t channel) { return (uint16_t)(0x0b00u + channel * 0x10u); }

static uint16_t stored_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static void store_peripheral_word(Dspic33* cpu, uint16_t value) {
    cpu->data[DMA_TEST_READ_PAD] = (uint8_t)value;
    cpu->data[DMA_TEST_READ_PAD + 1u] = (uint8_t)(value >> 8u);
}

static bool interrupt_flag(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = dma_irqs[channel];
    return (dspic33_read_word(cpu, (uint16_t)(0x0800u + (irq / 16u) * 2u)) &
            (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t channel) {
    uint8_t irq = dma_irqs[channel];
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    uint16_t mask = (uint16_t)(1u << (irq % 16u));
    dspic33_write_word(cpu, address, (uint16_t)(dspic33_read_word(cpu, address) & ~mask));
}

static void configure_channel(Dspic33* cpu, uint8_t channel, uint16_t control, uint8_t request,
                              uint32_t start_a, uint32_t start_b, uint16_t pad, uint16_t count) {
    uint16_t base = channel_base(channel);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), (uint16_t)start_a);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(start_a >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)start_b);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0au), (uint16_t)(start_b >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), count);
    dspic33_write_word(cpu, base, (uint16_t)(control | 0x8000u));
}

static bool request(Dspic33* cpu, uint8_t source, uint16_t indirect) {
    return dspic33_dma_request(cpu, source, indirect, 0u) && dspic33_device_advance(cpu, 1u);
}

static bool pad_in_set(uint16_t pad, const uint16_t* pads, size_t count) {
    size_t index;
    for (index = 0u; index < count; index++) {
        if (pad == pads[index]) {
            return true;
        }
    }
    return false;
}

static void pad_membership_cases(TestState* state) {
    static const uint16_t firmware_read_pads[] = {0x0236u, 0x0248u, 0x0256u, 0x0300u, 0x0608u};
    static const uint16_t firmware_write_pads[] = {0x0234u, 0x0248u, 0x0254u, 0x0608u};
    uint32_t address;
    uint32_t readable = 0u;
    uint32_t writable = 0u;
    uint32_t either = 0u;
    bool exact = true;
    size_t index;
    for (address = 0u; address <= UINT16_MAX; address++) {
        bool expected_read = pad_in_set((uint16_t)address, readable_pads,
                                        sizeof(readable_pads) / sizeof(readable_pads[0]));
        bool expected_write = pad_in_set((uint16_t)address, writable_pads,
                                         sizeof(writable_pads) / sizeof(writable_pads[0]));
        bool actual_read = dspic33_device_dma_pad_valid((uint16_t)address, false);
        bool actual_write = dspic33_device_dma_pad_valid((uint16_t)address, true);
        exact = exact && actual_read == expected_read && actual_write == expected_write;
        readable += actual_read;
        writable += actual_write;
        either += actual_read || actual_write;
    }
    expect(state, exact && readable == 18u && writable == 20u && either == 33u,
           "DMA PAD direction allowlists are exhaustive");
    exact = true;
    for (index = 0u; index < sizeof(firmware_read_pads) / sizeof(firmware_read_pads[0]); index++) {
        exact = exact && dspic33_device_dma_pad_valid(firmware_read_pads[index], false);
    }
    for (index = 0u; index < sizeof(firmware_write_pads) / sizeof(firmware_write_pads[0]);
         index++) {
        exact = exact && dspic33_device_dma_pad_valid(firmware_write_pads[index], true);
    }
    expect(state, exact, "firmware DMA PAD routes remain supported");
}

static void register_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint16_t base = channel_base(channel);
        expect(state, dspic33_read_word(cpu, base) == 0u, "DMAxCON reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u, "DMAxREQ reset");
        dspic33_write_word(cpu, base, 0x7fffu);
        expect(state, dspic33_read_word(cpu, base) == 0x7833u, "DMAxCON mask");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x7fffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x00ffu, "DMAxREQ mask");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0xffffu);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0au), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0x00ffu, "DMAxSTAH mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) == 0x00ffu, "DMAxSTBH mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0x3fffu, "DMAxCNT mask");
    }
    dspic33_write_word(cpu, 0x0bf0u, 0xffffu);
    dspic33_write_word(cpu, 0x0bf2u, 0xffffu);
    dspic33_write_word(cpu, 0x0bf4u, 0xffffu);
    dspic33_write_word(cpu, 0x0bf6u, 0xffffu);
    dspic33_write_word(cpu, 0x0bf8u, 0xffffu);
    dspic33_write_word(cpu, 0x0bfau, 0xffffu);
    expect(state, dspic33_read_word(cpu, 0x0bf0u) == 0u, "DMAPWC read only");
    expect(state, dspic33_read_word(cpu, 0x0bf2u) == 0u, "DMARQC read only");
    expect(state, dspic33_read_word(cpu, 0x0bf4u) == 0u, "DMAPPS read only");
    expect(state, dspic33_read_word(cpu, 0x0bf6u) == 0x000fu, "DMALCA reset and read only");
    expect(state, dspic33_read_word(cpu, 0x0bf8u) == 0u, "DSADRL read only");
    expect(state, dspic33_read_word(cpu, 0x0bfau) == 0u, "DSADRH read only");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_DMA, DSPIC33_DMA_COUNT * 2u, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && cpu->io.dma_active == 0u,
           "invalid DMA event source ignored");
}

static void direction_and_width_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x5aa5u);
    configure_channel(cpu, 0u, 0x2001u, 0x44u, 0x2000u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, request(cpu, 0x44u, 0u), "word RAM request");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x5aa5u, "word RAM to peripheral");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u, "word one-shot disables");
    expect(state, interrupt_flag(cpu, 0u), "word full interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0xa55au);
    dspic33_write_word(cpu, DMA_TEST_WRITE_PAD, 0x7e00u);
    configure_channel(cpu, 0u, 0x6001u, 0x45u, 0x2000u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, request(cpu, 0x45u, 0u), "byte RAM request");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x7e5au, "byte RAM to peripheral");

    dspic33_reset(cpu, 0u);
    store_peripheral_word(cpu, 0x369cu);
    configure_channel(cpu, 0u, 0x0001u, 0x46u, 0x2000u, 0u, DMA_TEST_READ_PAD, 0u);
    expect(state, request(cpu, 0x46u, 0u), "word peripheral request");
    expect(state, dspic33_read_word(cpu, 0x2000u) == 0x369cu, "word peripheral to RAM");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0xcc00u);
    store_peripheral_word(cpu, 0x12abu);
    configure_channel(cpu, 0u, 0x4001u, 0x47u, 0x2000u, 0u, DMA_TEST_READ_PAD, 0u);
    expect(state, request(cpu, 0x47u, 0u), "byte peripheral request");
    expect(state, dspic33_read_word(cpu, 0x2000u) == 0xccabu, "byte peripheral to RAM");
}

static void invalid_write_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x5aa5u);
    configure_channel(cpu, 0u, 0x2001u, 0x48u, 0x2000u, 0u, 0x0e32u, 0u);
    expect(state, request(cpu, 0x48u, 0u), "invalid word PAD request");
    expect(state,
           dspic33_read_word(cpu, 0x0e34u) == 0u && interrupt_flag(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u,
           "invalid word PAD invokes no handler and completes");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x00a5u);
    configure_channel(cpu, 0u, 0x6001u, 0x49u, 0x2000u, 0u, 0x0e32u, 0u);
    expect(state, request(cpu, 0x49u, 0u), "invalid low-byte PAD request");
    expect(state, dspic33_read_word(cpu, 0x0e34u) == 0u, "invalid low-byte PAD invokes no handler");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x003cu);
    configure_channel(cpu, 0u, 0x6001u, 0x4au, 0x2000u, 0u, 0x0e33u, 0u);
    expect(state, request(cpu, 0x4au, 0u), "invalid high-byte PAD request");
    expect(state, dspic33_read_word(cpu, 0x0e34u) == 0u,
           "invalid high-byte PAD invokes no handler");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x5aa5u);
    configure_channel(cpu, 0u, 0x2001u, 0x4bu, 0x2000u, 0u, 0x0e32u, 0u);
    expect(state, dspic33_dma_request(cpu, 0x4bu, 0u, 1u),
           "queue invalid PAD with concurrent CPU write");
    dspic33_write_word(cpu, 0x0e32u, 0x7777u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0e34u) == 0x7777u &&
               dspic33_read_word(cpu, 0x0bf0u) == 0u && cpu->trap_count == 0u,
           "invalid write PAD cannot raise a peripheral collision");
}

static void addressing_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0000u, 0x50u, 0x2200u, 0u, DMA_TEST_READ_PAD, 2u);
    store_peripheral_word(cpu, 0x1111u);
    expect(state, request(cpu, 0x50u, 0u), "post-increment first request");
    expect(state, dspic33_read_word(cpu, 0x0b04u) == 0x2200u,
           "post-increment preserves start register");
    store_peripheral_word(cpu, 0x2222u);
    expect(state, request(cpu, 0x50u, 0u), "post-increment second request");
    expect(state, dspic33_read_word(cpu, 0x0b04u) == 0x2200u,
           "post-increment keeps start register stable");
    store_peripheral_word(cpu, 0x3333u);
    expect(state, request(cpu, 0x50u, 0u), "post-increment third request");
    expect(state,
           dspic33_read_word(cpu, 0x2200u) == 0x1111u &&
               dspic33_read_word(cpu, 0x2202u) == 0x2222u &&
               dspic33_read_word(cpu, 0x2204u) == 0x3333u,
           "post-increment addresses");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0000u, 0x53u, 0x2200u, 0u, DMA_TEST_READ_PAD, 3u);
    store_peripheral_word(cpu, 0x7777u);
    expect(state, request(cpu, 0x53u, 0u) && request(cpu, 0x53u, 0u),
           "post-increment before mode switch");
    dspic33_write_word(cpu, 0x0b00u, 0x8010u);
    store_peripheral_word(cpu, 0x8888u);
    expect(state, request(cpu, 0x53u, 0u) && request(cpu, 0x53u, 0u),
           "fixed requests after mode switch");
    expect(state,
           dspic33_read_word(cpu, 0x2204u) == 0x8888u && dspic33_read_word(cpu, 0x2206u) == 0u,
           "fixed mode retains current internal pointer");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0010u, 0x51u, 0x2200u, 0u, DMA_TEST_READ_PAD, 2u);
    store_peripheral_word(cpu, 0x4444u);
    expect(state, request(cpu, 0x51u, 0u), "fixed first request");
    store_peripheral_word(cpu, 0x5555u);
    expect(state, request(cpu, 0x51u, 0u), "fixed second request");
    expect(state,
           dspic33_read_word(cpu, 0x2200u) == 0x5555u && dspic33_read_word(cpu, 0x2202u) == 0u,
           "fixed address");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0021u, 0x52u, 0x2400u, 0u, DMA_TEST_READ_PAD, 0u);
    store_peripheral_word(cpu, 0x6a6au);
    expect(state, request(cpu, 0x52u, 0x0034u), "peripheral indirect request");
    expect(state, dspic33_read_word(cpu, 0x2434u) == 0x6a6au, "peripheral indirect address");
    expect(state, dspic33_read_word(cpu, 0x0b04u) == 0x2434u,
           "peripheral indirect reports latest address");
}

static void operating_mode_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2500u, 0x1001u);
    dspic33_write_word(cpu, 0x2502u, 0x1002u);
    configure_channel(cpu, 0u, 0x2000u, 0x60u, 0x2500u, 0u, DMA_TEST_WRITE_PAD, 1u);
    expect(state, request(cpu, 0x60u, 0u) && request(cpu, 0x60u, 0u), "continuous first block");
    expect(state, interrupt_flag(cpu, 0u), "continuous block interrupt");
    clear_interrupt(cpu, 0u);
    expect(state, request(cpu, 0x60u, 0u), "continuous restarts address");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x1001u, "continuous primary restart");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u, "continuous remains enabled");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2500u, 0xa001u);
    dspic33_write_word(cpu, 0x2520u, 0xb001u);
    configure_channel(cpu, 0u, 0x2002u, 0x61u, 0x2500u, 0x2520u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, request(cpu, 0x61u, 0u), "continuous ping first");
    expect(state,
           stored_word(cpu, DMA_TEST_WRITE_PAD) == 0xa001u &&
               (dspic33_read_word(cpu, 0x0bf4u) & 1u) == 0u,
           "continuous ping primary");
    expect(state, request(cpu, 0x61u, 0u), "continuous pong request");
    expect(state,
           stored_word(cpu, DMA_TEST_WRITE_PAD) == 0xb001u &&
               (dspic33_read_word(cpu, 0x0bf4u) & 1u) != 0u,
           "continuous pong secondary");
    expect(state, request(cpu, 0x61u, 0u), "continuous ping restart");
    expect(state,
           stored_word(cpu, DMA_TEST_WRITE_PAD) == 0xa001u &&
               (dspic33_read_word(cpu, 0x0bf4u) & 1u) == 0u,
           "continuous ping cycles");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2500u, 0xc001u);
    dspic33_write_word(cpu, 0x2520u, 0xd001u);
    configure_channel(cpu, 0u, 0x2003u, 0x62u, 0x2500u, 0x2520u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, request(cpu, 0x62u, 0u), "one-shot ping request");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u,
           "one-shot ping remains for secondary");
    expect(state, request(cpu, 0x62u, 0u), "one-shot pong request");
    expect(state,
           stored_word(cpu, DMA_TEST_WRITE_PAD) == 0xd001u &&
               (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u,
           "one-shot pong disables");
}

static void interrupt_and_null_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x1000u, 0x70u, 0x2700u, 0u, DMA_TEST_READ_PAD, 3u);
    store_peripheral_word(cpu, 0x1111u);
    expect(state, request(cpu, 0x70u, 0u), "half first request");
    expect(state, !interrupt_flag(cpu, 0u), "half interrupt not early");
    expect(state, request(cpu, 0x70u, 0u), "half second request");
    expect(state, interrupt_flag(cpu, 0u), "half interrupt threshold");
    clear_interrupt(cpu, 0u);
    expect(state, request(cpu, 0x70u, 0u) && request(cpu, 0x70u, 0u), "half block completion");
    expect(state, !interrupt_flag(cpu, 0u), "half no full interrupt");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x1001u, 0x71u, 0x2700u, 0u, DMA_TEST_READ_PAD, 2u);
    store_peripheral_word(cpu, 0x2222u);
    expect(state, request(cpu, 0x71u, 0u), "odd half first request");
    expect(state, !interrupt_flag(cpu, 0u), "odd half not early");
    expect(state, request(cpu, 0x71u, 0u), "odd half second request");
    expect(state, interrupt_flag(cpu, 0u), "odd half rounds up");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DMA_TEST_BIDIRECTIONAL_PAD, 0x7b7bu);
    configure_channel(cpu, 0u, 0x0801u, 0x72u, 0x2700u, 0u, DMA_TEST_BIDIRECTIONAL_PAD, 0u);
    expect(state, request(cpu, 0x72u, 0u), "null write request");
    expect(state, dspic33_read_word(cpu, 0x2700u) == 0x7b7bu, "null write retains input");
    expect(state, dspic33_read_word(cpu, DMA_TEST_BIDIRECTIONAL_PAD) == 0u,
           "null write clears peripheral");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2800u, 0x7b7bu);
    configure_channel(cpu, 0u, 0x0001u, 0x73u, 0x2700u, 0u, 0x2800u, 0u);
    expect(state, request(cpu, 0x73u, 0u), "invalid read PAD request");
    expect(state,
           dspic33_read_word(cpu, 0x2700u) == 0u && dspic33_read_word(cpu, 0x2800u) == 0x7b7bu &&
               interrupt_flag(cpu, 0u) && (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u,
           "invalid read PAD supplies zero and completes without a handler");

    dspic33_reset(cpu, 0u);
    store_peripheral_word(cpu, 0x6a6au);
    configure_channel(cpu, 0u, 0x0801u, 0x74u, 0x2700u, 0u, DMA_TEST_READ_PAD, 0u);
    expect(state, request(cpu, 0x74u, 0u), "read-only NULLW PAD request");
    expect(state,
           dspic33_read_word(cpu, 0x2700u) == 0x6a6au &&
               dspic33_read_word(cpu, DMA_TEST_READ_PAD) == 0x6a6au,
           "NULLW invokes no write handler for a read-only PAD");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0801u, 0x75u, 0x2700u, 0u, DMA_TEST_READ_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0x75u, 0u, 1u),
           "queue read-only NULLW with concurrent CPU write");
    store_peripheral_word(cpu, 0x5b5bu);
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, 0x0bf0u) == 0u &&
               cpu->trap_count == 0u,
           "read-only NULLW cannot raise a peripheral collision");
}

static void force_and_collision_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint16_t base = channel_base(channel);
        uint16_t bit = (uint16_t)(1u << channel);
        uint8_t source = (uint8_t)(0x80u + channel);
        uint16_t memory = (uint16_t)(0x2900u + channel * 4u);
        uint16_t pad = DMA_TEST_WRITE_PAD;
        uint16_t first = (uint16_t)(0x1100u + channel);
        uint16_t second = (uint16_t)(0x2200u + channel);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, first);
        dspic33_write_word(cpu, (uint16_t)(memory + 2u), second);
        configure_channel(cpu, channel, 0x2000u, source, memory, 0u, pad, 3u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(DMA_FORCE | source));
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) != 0u,
               "FORCE software set matrix");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), source);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) != 0u,
               "FORCE write zero cannot clear matrix");
        expect(state, dspic33_device_advance(cpu, 1u), "FORCE start matrix");
        expect(state,
               stored_word(cpu, pad) == first && cpu->io.dma_index[channel] == 0u &&
                   !interrupt_flag(cpu, channel) && (dspic33_read_word(cpu, base) & 0x8000u) != 0u,
               "FORCE remains active before completion matrix");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) != 0u,
               "FORCE remains set until completion matrix");
        expect(state, dspic33_device_advance(cpu, 1u), "FORCE completion matrix");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u,
               "FORCE hardware clear matrix");
        expect(state, cpu->io.dma_index[channel] == 1u, "FORCE completion advances count matrix");
        expect(state, dspic33_device_advance(cpu, 2u), "FORCE idle advance matrix");
        expect(state, stored_word(cpu, pad) == first && cpu->io.dma_index[channel] == 1u,
               "FORCE exactly one transfer matrix");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, first);
        configure_channel(cpu, channel, 0x2000u, source, memory, 0u, pad, 1u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(DMA_FORCE | source));
        expect(state, (cpu->io.dma_forced_pending & bit) != 0u,
               "FORCE pending before disable matrix");
        dspic33_write_word(cpu, base, 0x2000u);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u &&
                   (cpu->io.dma_forced_pending & bit) == 0u,
               "disable clears FORCE pending matrix");
        expect(state, dspic33_device_advance(cpu, 2u) && stored_word(cpu, pad) == 0u,
               "disabled FORCE does not transfer matrix");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, first);
        configure_channel(cpu, channel, 0x2000u, source, memory, 0u, pad, 1u);
        expect(state, dspic33_dma_request(cpu, source, 0u, 1u),
               "queue peripheral before FORCE matrix");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(DMA_FORCE | source));
        expect(state, (dspic33_read_word(cpu, 0x0bf2u) & bit) != 0u,
               "peripheral before FORCE collision matrix");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u,
               "peripheral before FORCE discard matrix");
        expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
               "peripheral before FORCE trap matrix");
        dspic33_write_word(cpu, 0x08c0u, (uint16_t)(dspic33_read_word(cpu, 0x08c0u) & ~0x0020u));
        expect(state, dspic33_read_word(cpu, 0x0bf2u) == 0u,
               "peripheral before FORCE collision clear matrix");
        expect(state, dspic33_device_advance(cpu, 2u), "peripheral before FORCE advance matrix");
        expect(state, stored_word(cpu, pad) == first && cpu->io.dma_index[channel] == 1u,
               "peripheral before FORCE one transfer matrix");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, first);
        dspic33_write_word(cpu, (uint16_t)(memory + 2u), second);
        configure_channel(cpu, channel, 0x2000u, source, memory, 0u, pad, 3u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(DMA_FORCE | source));
        expect(state, dspic33_dma_request(cpu, source, 0u, 1u),
               "queue peripheral after FORCE matrix");
        expect(state, (dspic33_read_word(cpu, 0x0bf2u) & bit) != 0u,
               "FORCE before peripheral collision matrix");
        expect(state, dspic33_device_advance(cpu, 1u), "FORCE before peripheral start matrix");
        expect(state,
               stored_word(cpu, pad) == first && cpu->io.dma_index[channel] == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) != 0u,
               "FORCE executes before peripheral matrix");
        expect(state, dspic33_device_advance(cpu, 1u), "FORCE before peripheral deferred matrix");
        expect(state,
               stored_word(cpu, pad) == second && cpu->io.dma_index[channel] == 1u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u,
               "peripheral executes after FORCE matrix");
        expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
               "FORCE before peripheral trap matrix");
        expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.dma_index[channel] == 2u,
               "peripheral collision completion matrix");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, first);
        dspic33_write_word(cpu, (uint16_t)(memory + 2u), second);
        configure_channel(cpu, channel, 0x2000u, source, memory, 0u, pad, 3u);
        expect(state, dspic33_dma_request(cpu, source, 0u, 0u) && dspic33_device_advance(cpu, 0u),
               "start active peripheral transfer matrix");
        expect(state, (cpu->io.dma_active & bit) != 0u, "peripheral transfer active matrix");
        expect(state, stored_word(cpu, pad) == first && cpu->io.dma_index[channel] == 0u,
               "active peripheral first transfer matrix");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(DMA_FORCE | source));
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u,
               "FORCE ignored while active matrix");
        expect(state, (dspic33_read_word(cpu, 0x0bf2u) & bit) != 0u,
               "active FORCE request collision matrix");
        expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
               "active FORCE collision trap matrix");
        expect(state, dspic33_device_advance(cpu, 1u), "active transfer completion matrix");
        expect(state, (cpu->io.dma_active & bit) == 0u, "active transfer clears matrix");
        expect(state, cpu->io.dma_index[channel] == 1u, "active transfer count completes matrix");
        expect(state,
               dspic33_device_advance(cpu, 1u) && stored_word(cpu, pad) == first &&
                   cpu->io.dma_index[channel] == 1u,
               "ignored active FORCE does not transfer matrix");
    }
}

static void routing_and_status_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2b00u, 0x1000u);
    dspic33_write_word(cpu, 0x2b20u, 0x2000u);
    configure_channel(cpu, 2u, 0x2000u, 0x90u, 0x2b20u, 0u, DMA_TEST_ALT_WRITE_PAD, 0u);
    configure_channel(cpu, 0u, 0x2000u, 0x90u, 0x2b00u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, request(cpu, 0x90u, 0u), "shared request routing");
    expect(state,
           stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x1000u &&
               dspic33_read_word(cpu, DMA_TEST_ALT_WRITE_PAD) == 0x2000u,
           "shared request reaches channels");
    expect(state, dspic33_read_word(cpu, 0x0bf6u) == 2u, "lower channel priority executes first");
    expect(state, interrupt_flag(cpu, 0u) && !interrupt_flag(cpu, 2u) && cpu->io.dma_active == 4u,
           "lower-priority channel remains active after arbitration");
    expect(state,
           dspic33_device_advance(cpu, 1u) && interrupt_flag(cpu, 2u) && cpu->io.dma_active == 0u,
           "channel interrupt mapping");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x12346u, 0x55aau);
    configure_channel(cpu, 0u, 0x2001u, 0x91u, 0x12346u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, request(cpu, 0x91u, 0u), "24-bit address request");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x55aau, "24-bit memory transfer");
    expect(state,
           dspic33_read_word(cpu, 0x0bf8u) == 0x2346u && dspic33_read_word(cpu, 0x0bfau) == 0x0001u,
           "DSADR records 24-bit address");
    expect(state, dspic33_read_word(cpu, 0x0bf6u) == 0u, "DMALCA records channel");

    dspic33_reset(cpu, 0u);
    cpu->data[0] = 0x9eu;
    cpu->data[1] = 0x9eu;
    dspic33_write_word(cpu, DMA_TEST_WRITE_PAD, 0xffffu);
    configure_channel(cpu, 0u, 0x2001u, 0x92u, DSPIC33_DATA_SIZE, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, request(cpu, 0x92u, 0u), "out-of-range request");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0xffffu,
           "out-of-range DMA leaves peripheral unchanged");
    expect(state, cpu->last_trap == 6u && cpu->trap_count == 1u, "out-of-range DMA address trap");
    expect(state, (dspic33_read_word(cpu, 0x08c4u) & 0x0020u) != 0u, "out-of-range DMA sets DAE");
}

static void channel_matrix_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint8_t source = (uint8_t)(0xb0u + channel);
        uint16_t memory = (uint16_t)(0x3000u + channel * 4u);
        uint16_t pad = DMA_TEST_WRITE_PAD;
        uint16_t value = (uint16_t)(0x4000u + channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, value);
        configure_channel(cpu, channel, 0x2001u, source, memory, 0u, pad, 0u);
        expect(state, request(cpu, source, 0u), "channel request matrix");
        expect(state, stored_word(cpu, pad) == value, "channel transfer matrix");
        expect(state, interrupt_flag(cpu, channel), "channel interrupt matrix");
        expect(state, dspic33_read_word(cpu, 0x0bf6u) == channel, "channel status matrix");
        expect(state, (dspic33_read_word(cpu, channel_base(channel)) & 0x8000u) == 0u,
               "channel one-shot matrix");
    }
}

static void peripheral_collision_cases(TestState* state, Dspic33* cpu) {
    uint8_t can_channel;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3200u, 0x1111u);
    dspic33_write_word(cpu, 0x3202u, 0x2222u);
    configure_channel(cpu, 0u, 0x2000u, 0xc0u, 0x3200u, 0u, DMA_TEST_WRITE_PAD, 1u);
    expect(state, dspic33_dma_request(cpu, 0xc0u, 0u, 1u), "queue peripheral write collision");
    dspic33_write_word(cpu, DMA_TEST_WRITE_PAD, 0x7777u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance peripheral write collision");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x7777u, "CPU peripheral write prevails");
    expect(state, (dspic33_read_word(cpu, 0x0bf0u) & 1u) != 0u, "peripheral collision status");
    expect(state, cpu->last_trap == 5u && cpu->trap_count == 1u, "peripheral collision trap");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
           "peripheral collision DMACERR");
    expect(state, dspic33_dma_request(cpu, 0xc0u, 0u, 0u) && dspic33_device_advance(cpu, 0u),
           "request blocked by collision");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x7777u,
           "collision blocks subsequent transfer");
    dspic33_write_word(cpu, 0x08c0u, (uint16_t)(dspic33_read_word(cpu, 0x08c0u) & ~0x0020u));
    expect(state, dspic33_read_word(cpu, 0x0bf0u) == 0u, "clearing DMACERR clears write collision");
    expect(state, request(cpu, 0xc0u, 0u), "request resumes after collision clear");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x2222u,
           "transfer resumes after collision clear");

    for (can_channel = 0u; can_channel < 2u; can_channel++) {
        uint16_t pad = (uint16_t)(0x0442u + can_channel * 0x0100u);
        uint8_t request_source = (uint8_t)(0xc3u + can_channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x3200u, 0x1111u);
        dspic33_write_word(cpu, 0x3202u, 0x2222u);
        configure_channel(cpu, 0u, 0x2000u, request_source, 0x3200u, 0u, pad, 1u);
        expect(state, dspic33_dma_request(cpu, request_source, 0u, 1u),
               "queue B1 ECAN write collision");
        dspic33_write_word(cpu, pad, 0x7777u);
        expect(state, dspic33_device_advance(cpu, 1u), "advance B1 ECAN write collision");
        expect(state,
               stored_word(cpu, pad) == 0x7777u && dspic33_read_word(cpu, 0x0bf0u) == 0u &&
                   cpu->trap_count == 0u && (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) == 0u,
               "B1 ECAN write collision leaves collision state clear");
        expect(state, dspic33_device_advance(cpu, 1u) && request(cpu, request_source, 0u),
               "B1 ECAN DMA channel accepts a later request");
        expect(state, stored_word(cpu, pad) == 0x2222u, "B1 ECAN DMA transfers after a collision");
    }
}

static void memory_collision_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3400u, 0x1111u);
    configure_channel(cpu, 0u, 0x2001u, 0xc1u, 0x3400u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0xc1u, 0u, 1u), "queue concurrent DMA memory read");
    dspic33_write_word(cpu, 0x3400u, 0x2222u);
    expect(state, dspic33_device_advance(cpu, 1u), "advance concurrent DMA memory read");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x1111u,
           "DMA reads pre-write memory value");
    expect(state, dspic33_read_word(cpu, 0x3400u) == 0x2222u, "CPU memory write remains visible");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3400u, 0xaaaau);
    store_peripheral_word(cpu, 0x3333u);
    configure_channel(cpu, 0u, 0x0001u, 0xc2u, 0x3400u, 0u, DMA_TEST_READ_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0xc2u, 0u, 1u), "queue concurrent DMA memory write");
    dspic33_write_word(cpu, 0x3400u, 0xbbbbu);
    expect(state, dspic33_device_advance(cpu, 1u), "advance concurrent DMA memory write");
    expect(state, dspic33_read_word(cpu, 0x3400u) == 0xbbbbu,
           "CPU write wins concurrent DMA memory write");
    expect(state, dspic33_read_word(cpu, 0x0bf0u) == 0u,
           "memory collision does not set peripheral status");
}

static void stale_request_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2d00u, 0x1111u);
    configure_channel(cpu, 0u, 0x2000u, 0xa0u, 0x2d00u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0xa0u, 0u, 5u), "queue stale request");
    dspic33_write_word(cpu, 0x0b00u, 0x2000u);
    dspic33_write_word(cpu, 0x2d00u, 0x2222u);
    dspic33_write_word(cpu, 0x0b00u, 0xa000u);
    expect(state, dspic33_device_advance(cpu, 5u), "advance stale request");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0u,
           "stale request discarded after re-enable");
    expect(state, request(cpu, 0xa0u, 0u), "new generation request");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x2222u, "new generation transfers");
}

static void can_receive_arbiter_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t requests[] = {34u, 55u};
    size_t index;
    for (index = 0u; index < sizeof(requests) / sizeof(requests[0]); index++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x3600u, 0x1357u);
        configure_channel(cpu, 0u, 0x2001u, requests[index], 0u, 0u, DMA_TEST_WRITE_PAD, 0u);
        expect(state,
               dspic33_dma_request(cpu, requests[index], 0u, 5u) &&
                   cpu->io.dma_peripheral_pending == 1u,
               "delayed CAN receive DMA transaction remains queued");
        expect(state,
               dspic33_dma_request(cpu, requests[index], 0u, 0u) &&
                   cpu->stop_reason == DSPIC33_RUNNING,
               "queued delay alone does not imply a DMA arbiter hold");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x3600u, 0x1357u);
        configure_channel(cpu, 0u, 0x2001u, requests[index], 0u, 0u, DMA_TEST_WRITE_PAD, 0u);
        dspic33_load_program_word(cpu, 0x0200u, OPCODE_MOV_W1_POST_INCREMENT_W2);
        cpu->pc = 0x0200u;
        dspic33_set_working_register(cpu, 1u, 0x3800u);
        dspic33_write_word(cpu, 0x3800u, 0x55aau);
        {
            bool queued = dspic33_dma_request(cpu, requests[index], 0x3600u, 1u);
            Dspic33StopReason stepped = dspic33_step(cpu);
            expect(state,
                   queued && stepped == DSPIC33_RUNNING && cpu->w[2] == 0x55aau &&
                       (cpu->io.dma_arbiter_waiting & 1u) != 0u,
                   "CAN receive DMA transaction waits for the CPU bus master");
        }
        expect(state,
               dspic33_dma_request(cpu, requests[index], 0xd000u, 0u) &&
                   cpu->stop_reason == DSPIC33_SILICON_RESULT_UNDEFINED,
               "B1 CAN receive request remains undefined after ordinary RAM wait");

        cpu->stop_reason = DSPIC33_RUNNING;
        dspic33_write_word(cpu, 0x0b00u, 0x2001u);
        dspic33_write_word(cpu, 0x0b00u, 0xa001u);
        expect(state,
               (cpu->io.dma_arbiter_waiting & 1u) == 0u &&
                   dspic33_dma_request(cpu, requests[index], 0u, 0u) &&
                   cpu->stop_reason == DSPIC33_RUNNING,
               "DMA disable and re-enable clears stale arbiter-wait history");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0058u, 0x0020u);
        dspic33_write_word(cpu, 0x3600u, 0x1357u);
        configure_channel(cpu, 0u, 0x2001u, requests[index], 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
        dspic33_load_program_word(cpu, 0x0200u, OPCODE_MOV_W1_POST_INCREMENT_W2);
        cpu->pc = 0x0200u;
        dspic33_set_working_register(cpu, 1u, 0x3800u);
        dspic33_write_word(cpu, 0x3800u, 0x55aau);
        expect(state,
               dspic33_dma_request(cpu, requests[index], 0u, 1u) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_dma_request(cpu, requests[index], 0u, 0u) &&
                   cpu->stop_reason == DSPIC33_RUNNING,
               "MSTRPR workaround excludes the B1 request-loss boundary");

        dspic33_reset(cpu, 0u);
        configure_channel(cpu, 0u, 0x0001u, requests[index], 0xd000u, 0u, DMA_TEST_WRITE_PAD, 0u);
        dspic33_load_program_word(cpu, 0x0200u, OPCODE_MOV_W1_POST_INCREMENT_W2);
        cpu->pc = 0x0200u;
        dspic33_set_working_register(cpu, 1u, 0x3800u);
        dspic33_write_word(cpu, 0x3800u, 0x55aau);
        expect(state,
               dspic33_dma_request(cpu, requests[index], 0u, 1u) &&
                   dspic33_step(cpu) == DSPIC33_RUNNING &&
                   dspic33_dma_request(cpu, requests[index], 0u, 0u) &&
                   cpu->stop_reason == DSPIC33_RUNNING,
               "dual-port RAM workaround excludes the B1 request-loss boundary");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x3600u, 0x1357u);
        configure_channel(cpu, 0u, 0x2001u, requests[index], 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
        expect(state,
               dspic33_dma_request(cpu, requests[index], 0u, 0u) &&
                   (dspic33_write_word(cpu, 0x3800u, 0x55aau), true) &&
                   dspic33_device_advance(cpu, 0u) && cpu->io.dma_arbiter_waiting == 0u,
               "external setup writes do not synthesize CPU bus contention");
    }
}

static void power_and_lifecycle_cases(TestState* state, Dspic33* cpu, Dspic33* copy) {
    uint64_t sleep_cycle;
    uint64_t remaining;
    uint16_t generation;

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x1111u);
    configure_channel(cpu, 0u, 0x2001u, 0xd0u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state, request(cpu, 0xd0u, 0u), "DMA operates during Idle");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x1111u && interrupt_flag(cpu, 0u),
           "Idle DMA transfer completes and raises interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x2222u);
    configure_channel(cpu, 0u, 0x2001u, 0xd1u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0xd1u, 0u, 3u), "queue DMA before Sleep");
    expect(state, dspic33_device_advance(cpu, 1u), "advance DMA before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    sleep_cycle = cpu->device_cycles;
    remaining = cpu->events.items[0].paused_remaining;
    expect(state, cpu->events.count == 1u && cpu->events.items[0].paused && remaining == 2u,
           "Sleep pauses pending DMA request");
    expect(state,
           dspic33_device_advance(cpu, 20u) && cpu->device_cycles == sleep_cycle + 20u &&
               stored_word(cpu, DMA_TEST_WRITE_PAD) == 0u && !interrupt_flag(cpu, 0u),
           "Sleep suppresses DMA transfer and interrupt");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           cpu->events.count == 1u && !cpu->events.items[0].paused &&
               cpu->events.items[0].cycle == cpu->device_cycles + remaining,
           "wake restores pending DMA interval");
    expect(state,
           dspic33_device_advance(cpu, remaining - 1u) &&
               stored_word(cpu, DMA_TEST_WRITE_PAD) == 0u,
           "DMA remains pending before restored deadline");
    expect(state,
           dspic33_device_advance(cpu, 2u) && stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x2222u &&
               interrupt_flag(cpu, 0u),
           "DMA resumes after wake");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x3333u);
    configure_channel(cpu, 0u, 0x2001u, 0xd2u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_dma_request(cpu, 0xd2u, 0u, 2u), "queue DMA while asleep");
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               cpu->events.items[0].paused_remaining == 2u,
           "asleep DMA request starts paused");
    expect(state, dspic33_copy(copy, cpu), "copy paused DMA state");
    expect(state,
           copy->events.count == 1u && copy->events.items[0].paused &&
               copy->io.dma_peripheral_pending == cpu->io.dma_peripheral_pending &&
               copy->events.items != cpu->events.items,
           "copy preserves independent DMA event state");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    copy->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    dspic33_device_power_state_changed(copy);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_device_advance(copy, 3u) &&
               stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x3333u &&
               stored_word(copy, DMA_TEST_WRITE_PAD) == 0x3333u,
           "copied DMA requests complete independently");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x4444u);
    configure_channel(cpu, 0u, 0x2001u, 0xd3u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0xd3u, 0u, 4u), "queue DMA before cold reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->events.count == 0u && cpu->io.dma_peripheral_pending == 0u &&
               cpu->io.dma_active == 0u,
           "cold reset cancels DMA state");
    expect(state, dspic33_device_advance(cpu, 5u) && stored_word(cpu, DMA_TEST_WRITE_PAD) == 0u,
           "cold reset DMA event cannot execute");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x5555u);
    configure_channel(cpu, 0u, 0x2001u, 0xd4u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0xd4u, 0u, 4u), "queue DMA before warm reset");
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute DMA warm reset");
    expect(state,
           cpu->events.count == 0u && cpu->io.dma_peripheral_pending == 0u &&
               dspic33_read_word(cpu, 0x0b00u) == 0u,
           "warm reset cancels DMA state");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x2000u, 0xd5u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(state, !dspic33_dma_request(cpu, 0xd5u, 0u, 1u),
           "DMA request reports deadline overflow");
    expect(state, cpu->events.count == 0u && cpu->io.dma_peripheral_pending == 0u,
           "failed DMA request leaves no pending state");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x5656u);
    configure_channel(cpu, 0u, 0x2000u, 0xd5u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_DMA, 0u, (uint32_t)cpu->io.dma_generation[0] << 17u,
                            0u) &&
               !dspic33_device_advance(cpu, 0u),
           "execute DMA with unavailable completion deadline");
    expect(state,
           stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x5656u && cpu->io.dma_active == 0u &&
               cpu->io.dma_index[0] == 0u && !interrupt_flag(cpu, 0u) &&
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "DMA completion schedule failure stops without false completion");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x6666u);
    configure_channel(cpu, 0u, 0x2000u, 0xd6u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 1u);
    cpu->io.dma_generation[0] = 0x7fffu;
    expect(state, dspic33_dma_request(cpu, 0xd6u, 0u, 10u), "queue maximum-generation DMA request");
    dspic33_write_word(cpu, 0x0b00u, 0x2000u);
    expect(state, cpu->events.count == 0u && cpu->io.dma_generation[0] == 0x8000u,
           "generation wrap discards aliased DMA request");
    dspic33_write_word(cpu, 0x0b00u, 0xa000u);
    expect(state, dspic33_device_advance(cpu, 10u), "advance past discarded DMA request");
    expect(state, stored_word(cpu, DMA_TEST_WRITE_PAD) == 0u && cpu->io.dma_index[0] == 0u,
           "wrapped stale DMA request remains invalid");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3600u, 0x7777u);
    dspic33_write_word(cpu, 0x3620u, 0x8888u);
    configure_channel(cpu, 1u, 0x2001u, 0xd7u, 0x3620u, 0u, DMA_TEST_ALT_WRITE_PAD, 0u);
    configure_channel(cpu, 0u, 0x2001u, 0xd7u, 0x3600u, 0u, DMA_TEST_WRITE_PAD, 0u);
    expect(state, dspic33_dma_request(cpu, 0xd7u, 0u, 0u), "queue simultaneous DMA arbitration");
    expect(state, dspic33_device_advance(cpu, 0u), "start highest-priority DMA channel");
    expect(state,
           stored_word(cpu, DMA_TEST_WRITE_PAD) == 0x7777u &&
               dspic33_read_word(cpu, DMA_TEST_ALT_WRITE_PAD) == 0u && cpu->io.dma_active == 1u,
           "only highest-priority DMA channel starts");
    expect(state, dspic33_device_advance(cpu, 1u), "complete high-priority and start pending DMA");
    expect(state,
           dspic33_read_word(cpu, DMA_TEST_ALT_WRITE_PAD) == 0x8888u && cpu->io.dma_active == 2u,
           "pending DMA channel starts after current transfer");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.dma_active == 0u,
           "pending DMA channel completes serially");

    generation = cpu->io.dma_generation[0];
    expect(state, generation != 0u, "DMA lifecycle advances generation");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize DMA processor");
    if (initialized) {
        pad_membership_cases(&state);
        register_cases(&state, &cpu);
        direction_and_width_cases(&state, &cpu);
        invalid_write_cases(&state, &cpu);
        addressing_cases(&state, &cpu);
        operating_mode_cases(&state, &cpu);
        interrupt_and_null_cases(&state, &cpu);
        force_and_collision_cases(&state, &cpu);
        routing_and_status_cases(&state, &cpu);
        channel_matrix_cases(&state, &cpu);
        peripheral_collision_cases(&state, &cpu);
        memory_collision_cases(&state, &cpu);
        stale_request_cases(&state, &cpu);
        can_receive_arbiter_erratum_cases(&state, &cpu);
        {
            Dspic33 copy;
            bool copy_initialized = dspic33_initialize(&copy);
            expect(&state, copy_initialized, "initialize DMA copy processor");
            if (copy_initialized) {
                power_and_lifecycle_cases(&state, &cpu, &copy);
                dspic33_release(&copy);
            }
        }
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
