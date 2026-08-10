#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} DmaConformance;

enum { DMA_FORCE = 0x8000u };

static const uint8_t dma_irqs[DSPIC33_DMA_COUNT] = {
    4u, 14u, 24u, 36u, 46u, 61u, 68u, 69u, 118u, 119u, 120u, 121u, 130u, 131u, 132u};

static void expect(DmaConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[dma-failed] %s\n", name);
    }
}

static uint16_t channel_base(uint8_t channel) {
    return (uint16_t)(0x0b00u + channel * 0x10u);
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
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~mask));
}

static void configure_channel(Dspic33* cpu, uint8_t channel, uint16_t control,
                              uint8_t request, uint32_t start_a, uint32_t start_b,
                              uint16_t pad, uint16_t count) {
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
    return dspic33_dma_request(cpu, source, indirect, 0u) &&
           dspic33_device_advance(cpu, 1u);
}

static void register_cases(DmaConformance* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint16_t base = channel_base(channel);
        expect(state, dspic33_read_word(cpu, base) == 0u, "DMAxCON reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u,
               "DMAxREQ reset");
        dspic33_write_word(cpu, base, 0x7fffu);
        expect(state, dspic33_read_word(cpu, base) == 0x7833u, "DMAxCON mask");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x7fffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x00ffu,
               "DMAxREQ mask");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0xffffu);
        dspic33_write_word(cpu, (uint16_t)(base + 0x0au), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0x00ffu,
               "DMAxSTAH mask");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0au)) == 0x00ffu,
               "DMAxSTBH mask");
        dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 0x0eu)) == 0x3fffu,
               "DMAxCNT mask");
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
    expect(state, dspic33_read_word(cpu, 0x0bf6u) == 0x000fu,
           "DMALCA reset and read only");
    expect(state, dspic33_read_word(cpu, 0x0bf8u) == 0u, "DSADRL read only");
    expect(state, dspic33_read_word(cpu, 0x0bfau) == 0u, "DSADRH read only");
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_DMA, DSPIC33_DMA_COUNT * 2u, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && cpu->io.dma_active == 0u,
           "invalid DMA event source ignored");
}

static void direction_and_width_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x5aa5u);
    configure_channel(cpu, 0u, 0x2001u, 0x44u, 0x2000u, 0u, 0x2100u, 0u);
    expect(state, request(cpu, 0x44u, 0u), "word RAM request");
    expect(state, dspic33_read_word(cpu, 0x2100u) == 0x5aa5u, "word RAM to peripheral");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u,
           "word one-shot disables");
    expect(state, interrupt_flag(cpu, 0u), "word full interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0xa55au);
    dspic33_write_word(cpu, 0x2100u, 0x7e00u);
    configure_channel(cpu, 0u, 0x6001u, 0x45u, 0x2000u, 0u, 0x2100u, 0u);
    expect(state, request(cpu, 0x45u, 0u), "byte RAM request");
    expect(state, dspic33_read_word(cpu, 0x2100u) == 0x7e5au, "byte RAM to peripheral");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2100u, 0x369cu);
    configure_channel(cpu, 0u, 0x0001u, 0x46u, 0x2000u, 0u, 0x2100u, 0u);
    expect(state, request(cpu, 0x46u, 0u), "word peripheral request");
    expect(state, dspic33_read_word(cpu, 0x2000u) == 0x369cu, "word peripheral to RAM");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0xcc00u);
    dspic33_write_word(cpu, 0x2100u, 0x12abu);
    configure_channel(cpu, 0u, 0x4001u, 0x47u, 0x2000u, 0u, 0x2100u, 0u);
    expect(state, request(cpu, 0x47u, 0u), "byte peripheral request");
    expect(state, dspic33_read_word(cpu, 0x2000u) == 0xccabu, "byte peripheral to RAM");
}

static void gpio_latch_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x5aa5u);
    configure_channel(cpu, 0u, 0x2001u, 0x48u, 0x2000u, 0u, 0x0e32u, 0u);
    expect(state, request(cpu, 0x48u, 0u), "word GPIO latch request");
    expect(state, dspic33_read_word(cpu, 0x0e34u) == 0x5aa5u,
           "word PORT write updates latch");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x00a5u);
    configure_channel(cpu, 0u, 0x6001u, 0x49u, 0x2000u, 0u, 0x0e32u, 0u);
    expect(state, request(cpu, 0x49u, 0u), "low-byte GPIO latch request");
    expect(state, dspic33_read_word(cpu, 0x0e34u) == 0x00a5u,
           "low-byte PORT write updates latch");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2000u, 0x003cu);
    configure_channel(cpu, 0u, 0x6001u, 0x4au, 0x2000u, 0u, 0x0e33u, 0u);
    expect(state, request(cpu, 0x4au, 0u), "high-byte GPIO latch request");
    expect(state, dspic33_read_word(cpu, 0x0e34u) == 0x3c00u,
           "high-byte PORT write updates latch");
}

static void addressing_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0000u, 0x50u, 0x2200u, 0u, 0x2300u, 2u);
    dspic33_write_word(cpu, 0x2300u, 0x1111u);
    expect(state, request(cpu, 0x50u, 0u), "post-increment first request");
    expect(state, dspic33_read_word(cpu, 0x0b04u) == 0x2200u,
           "post-increment preserves start register");
    dspic33_write_word(cpu, 0x2300u, 0x2222u);
    expect(state, request(cpu, 0x50u, 0u), "post-increment second request");
    expect(state, dspic33_read_word(cpu, 0x0b04u) == 0x2200u,
           "post-increment keeps start register stable");
    dspic33_write_word(cpu, 0x2300u, 0x3333u);
    expect(state, request(cpu, 0x50u, 0u), "post-increment third request");
    expect(state,
           dspic33_read_word(cpu, 0x2200u) == 0x1111u &&
               dspic33_read_word(cpu, 0x2202u) == 0x2222u &&
               dspic33_read_word(cpu, 0x2204u) == 0x3333u,
           "post-increment addresses");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0000u, 0x53u, 0x2200u, 0u, 0x2300u, 3u);
    dspic33_write_word(cpu, 0x2300u, 0x7777u);
    expect(state, request(cpu, 0x53u, 0u) && request(cpu, 0x53u, 0u),
           "post-increment before mode switch");
    dspic33_write_word(cpu, 0x0b00u, 0x8010u);
    dspic33_write_word(cpu, 0x2300u, 0x8888u);
    expect(state, request(cpu, 0x53u, 0u) && request(cpu, 0x53u, 0u),
           "fixed requests after mode switch");
    expect(state,
           dspic33_read_word(cpu, 0x2204u) == 0x8888u &&
               dspic33_read_word(cpu, 0x2206u) == 0u,
           "fixed mode retains current internal pointer");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0010u, 0x51u, 0x2200u, 0u, 0x2300u, 2u);
    dspic33_write_word(cpu, 0x2300u, 0x4444u);
    expect(state, request(cpu, 0x51u, 0u), "fixed first request");
    dspic33_write_word(cpu, 0x2300u, 0x5555u);
    expect(state, request(cpu, 0x51u, 0u), "fixed second request");
    expect(state,
           dspic33_read_word(cpu, 0x2200u) == 0x5555u &&
               dspic33_read_word(cpu, 0x2202u) == 0u,
           "fixed address");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x0021u, 0x52u, 0x2400u, 0u, 0x2300u, 0u);
    dspic33_write_word(cpu, 0x2300u, 0x6a6au);
    expect(state, request(cpu, 0x52u, 0x0034u), "peripheral indirect request");
    expect(state, dspic33_read_word(cpu, 0x2434u) == 0x6a6au,
           "peripheral indirect address");
    expect(state, dspic33_read_word(cpu, 0x0b04u) == 0x2434u,
           "peripheral indirect reports latest address");
}

static void operating_mode_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2500u, 0x1001u);
    dspic33_write_word(cpu, 0x2502u, 0x1002u);
    configure_channel(cpu, 0u, 0x2000u, 0x60u, 0x2500u, 0u, 0x2600u, 1u);
    expect(state, request(cpu, 0x60u, 0u) && request(cpu, 0x60u, 0u),
           "continuous first block");
    expect(state, interrupt_flag(cpu, 0u), "continuous block interrupt");
    clear_interrupt(cpu, 0u);
    expect(state, request(cpu, 0x60u, 0u), "continuous restarts address");
    expect(state, dspic33_read_word(cpu, 0x2600u) == 0x1001u,
           "continuous primary restart");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u,
           "continuous remains enabled");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2500u, 0xa001u);
    dspic33_write_word(cpu, 0x2520u, 0xb001u);
    configure_channel(cpu, 0u, 0x2002u, 0x61u, 0x2500u, 0x2520u, 0x2600u, 0u);
    expect(state, request(cpu, 0x61u, 0u), "continuous ping first");
    expect(state,
           dspic33_read_word(cpu, 0x2600u) == 0xa001u &&
               (dspic33_read_word(cpu, 0x0bf4u) & 1u) == 0u,
           "continuous ping primary");
    expect(state, request(cpu, 0x61u, 0u), "continuous pong request");
    expect(state,
           dspic33_read_word(cpu, 0x2600u) == 0xb001u &&
               (dspic33_read_word(cpu, 0x0bf4u) & 1u) != 0u,
           "continuous pong secondary");
    expect(state, request(cpu, 0x61u, 0u), "continuous ping restart");
    expect(state,
           dspic33_read_word(cpu, 0x2600u) == 0xa001u &&
               (dspic33_read_word(cpu, 0x0bf4u) & 1u) == 0u,
           "continuous ping cycles");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2500u, 0xc001u);
    dspic33_write_word(cpu, 0x2520u, 0xd001u);
    configure_channel(cpu, 0u, 0x2003u, 0x62u, 0x2500u, 0x2520u, 0x2600u, 0u);
    expect(state, request(cpu, 0x62u, 0u), "one-shot ping request");
    expect(state, (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u,
           "one-shot ping remains for secondary");
    expect(state, request(cpu, 0x62u, 0u), "one-shot pong request");
    expect(state,
           dspic33_read_word(cpu, 0x2600u) == 0xd001u &&
               (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u,
           "one-shot pong disables");
}

static void interrupt_and_null_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x1000u, 0x70u, 0x2700u, 0u, 0x2800u, 3u);
    dspic33_write_word(cpu, 0x2800u, 0x1111u);
    expect(state, request(cpu, 0x70u, 0u), "half first request");
    expect(state, !interrupt_flag(cpu, 0u), "half interrupt not early");
    expect(state, request(cpu, 0x70u, 0u), "half second request");
    expect(state, interrupt_flag(cpu, 0u), "half interrupt threshold");
    clear_interrupt(cpu, 0u);
    expect(state, request(cpu, 0x70u, 0u) && request(cpu, 0x70u, 0u),
           "half block completion");
    expect(state, !interrupt_flag(cpu, 0u), "half no full interrupt");

    dspic33_reset(cpu, 0u);
    configure_channel(cpu, 0u, 0x1001u, 0x71u, 0x2700u, 0u, 0x2800u, 2u);
    dspic33_write_word(cpu, 0x2800u, 0x2222u);
    expect(state, request(cpu, 0x71u, 0u), "odd half first request");
    expect(state, !interrupt_flag(cpu, 0u), "odd half not early");
    expect(state, request(cpu, 0x71u, 0u), "odd half second request");
    expect(state, interrupt_flag(cpu, 0u), "odd half rounds up");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2800u, 0x7b7bu);
    configure_channel(cpu, 0u, 0x0801u, 0x72u, 0x2700u, 0u, 0x2800u, 0u);
    expect(state, request(cpu, 0x72u, 0u), "null write request");
    expect(state, dspic33_read_word(cpu, 0x2700u) == 0x7b7bu,
           "null write retains input");
    expect(state, dspic33_read_word(cpu, 0x2800u) == 0u,
           "null write clears peripheral");
}

static void force_and_collision_cases(DmaConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint16_t base = channel_base(channel);
        uint16_t bit = (uint16_t)(1u << channel);
        uint8_t source = (uint8_t)(0x80u + channel);
        uint16_t memory = (uint16_t)(0x2900u + channel * 4u);
        uint16_t pad = (uint16_t)(0x2a00u + channel * 4u);
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
               dspic33_read_word(cpu, pad) == first &&
                   cpu->io.dma_index[channel] == 0u && !interrupt_flag(cpu, channel) &&
                   (dspic33_read_word(cpu, base) & 0x8000u) != 0u,
               "FORCE remains active before completion matrix");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) != 0u,
               "FORCE remains set until completion matrix");
        expect(state, dspic33_device_advance(cpu, 1u), "FORCE completion matrix");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u,
               "FORCE hardware clear matrix");
        expect(state, cpu->io.dma_index[channel] == 1u,
               "FORCE completion advances count matrix");
        expect(state, dspic33_device_advance(cpu, 2u), "FORCE idle advance matrix");
        expect(state,
               dspic33_read_word(cpu, pad) == first && cpu->io.dma_index[channel] == 1u,
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
        expect(state,
               dspic33_device_advance(cpu, 2u) && dspic33_read_word(cpu, pad) == 0u,
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
        dspic33_write_word(cpu, 0x08c0u,
                           (uint16_t)(dspic33_read_word(cpu, 0x08c0u) & ~0x0020u));
        expect(state, dspic33_read_word(cpu, 0x0bf2u) == 0u,
               "peripheral before FORCE collision clear matrix");
        expect(state, dspic33_device_advance(cpu, 2u),
               "peripheral before FORCE advance matrix");
        expect(state,
               dspic33_read_word(cpu, pad) == first && cpu->io.dma_index[channel] == 1u,
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
        expect(state, dspic33_device_advance(cpu, 1u),
               "FORCE before peripheral start matrix");
        expect(state,
               dspic33_read_word(cpu, pad) == first &&
                   cpu->io.dma_index[channel] == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) != 0u,
               "FORCE executes before peripheral matrix");
        expect(state, dspic33_device_advance(cpu, 1u),
               "FORCE before peripheral deferred matrix");
        expect(state,
               dspic33_read_word(cpu, pad) == second &&
                   cpu->io.dma_index[channel] == 1u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u,
               "peripheral executes after FORCE matrix");
        expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
               "FORCE before peripheral trap matrix");
        expect(state,
               dspic33_device_advance(cpu, 1u) && cpu->io.dma_index[channel] == 2u,
               "peripheral collision completion matrix");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, first);
        dspic33_write_word(cpu, (uint16_t)(memory + 2u), second);
        configure_channel(cpu, channel, 0x2000u, source, memory, 0u, pad, 3u);
        expect(state,
               dspic33_dma_request(cpu, source, 0u, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "start active peripheral transfer matrix");
        expect(state, (cpu->io.dma_active & bit) != 0u,
               "peripheral transfer active matrix");
        expect(state,
               dspic33_read_word(cpu, pad) == first && cpu->io.dma_index[channel] == 0u,
               "active peripheral first transfer matrix");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), (uint16_t)(DMA_FORCE | source));
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 2u)) & DMA_FORCE) == 0u,
               "FORCE ignored while active matrix");
        expect(state, (dspic33_read_word(cpu, 0x0bf2u) & bit) != 0u,
               "active FORCE request collision matrix");
        expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
               "active FORCE collision trap matrix");
        expect(state, dspic33_device_advance(cpu, 1u),
               "active transfer completion matrix");
        expect(state, (cpu->io.dma_active & bit) == 0u,
               "active transfer clears matrix");
        expect(state, cpu->io.dma_index[channel] == 1u,
               "active transfer count completes matrix");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_read_word(cpu, pad) == first &&
                   cpu->io.dma_index[channel] == 1u,
               "ignored active FORCE does not transfer matrix");
    }
}

static void routing_and_status_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2b00u, 0x1000u);
    dspic33_write_word(cpu, 0x2b20u, 0x2000u);
    configure_channel(cpu, 2u, 0x2000u, 0x90u, 0x2b20u, 0u, 0x2c20u, 0u);
    configure_channel(cpu, 0u, 0x2000u, 0x90u, 0x2b00u, 0u, 0x2c00u, 0u);
    expect(state, request(cpu, 0x90u, 0u), "shared request routing");
    expect(state,
           dspic33_read_word(cpu, 0x2c00u) == 0x1000u &&
               dspic33_read_word(cpu, 0x2c20u) == 0x2000u,
           "shared request reaches channels");
    expect(state, dspic33_read_word(cpu, 0x0bf6u) == 2u,
           "lower channel priority executes first");
    expect(state, interrupt_flag(cpu, 0u) && interrupt_flag(cpu, 2u),
           "channel interrupt mapping");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x12346u, 0x55aau);
    configure_channel(cpu, 0u, 0x2001u, 0x91u, 0x12346u, 0u, 0x2c00u, 0u);
    expect(state, request(cpu, 0x91u, 0u), "24-bit address request");
    expect(state, dspic33_read_word(cpu, 0x2c00u) == 0x55aau, "24-bit memory transfer");
    expect(state,
           dspic33_read_word(cpu, 0x0bf8u) == 0x2346u &&
               dspic33_read_word(cpu, 0x0bfau) == 0x0001u,
           "DSADR records 24-bit address");
    expect(state, dspic33_read_word(cpu, 0x0bf6u) == 0u, "DMALCA records channel");

    dspic33_reset(cpu, 0u);
    cpu->data[0] = 0x9eu;
    cpu->data[1] = 0x9eu;
    dspic33_write_word(cpu, 0x2c00u, 0xffffu);
    configure_channel(cpu, 0u, 0x2001u, 0x92u, DSPIC33_DATA_SIZE, 0u, 0x2c00u, 0u);
    expect(state, request(cpu, 0x92u, 0u), "out-of-range request");
    expect(state, dspic33_read_word(cpu, 0x2c00u) == 0xffffu,
           "out-of-range DMA leaves peripheral unchanged");
    expect(state, cpu->last_trap == 6u && cpu->trap_count == 1u,
           "out-of-range DMA address trap");
    expect(state, (dspic33_read_word(cpu, 0x08c4u) & 0x0020u) != 0u,
           "out-of-range DMA sets DAE");
}

static void channel_matrix_cases(DmaConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_DMA_COUNT; channel++) {
        uint8_t source = (uint8_t)(0xb0u + channel);
        uint16_t memory = (uint16_t)(0x3000u + channel * 4u);
        uint16_t pad = (uint16_t)(0x3100u + channel * 4u);
        uint16_t value = (uint16_t)(0x4000u + channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, memory, value);
        configure_channel(cpu, channel, 0x2001u, source, memory, 0u, pad, 0u);
        expect(state, request(cpu, source, 0u), "channel request matrix");
        expect(state, dspic33_read_word(cpu, pad) == value, "channel transfer matrix");
        expect(state, interrupt_flag(cpu, channel), "channel interrupt matrix");
        expect(state, dspic33_read_word(cpu, 0x0bf6u) == channel,
               "channel status matrix");
        expect(state, (dspic33_read_word(cpu, channel_base(channel)) & 0x8000u) == 0u,
               "channel one-shot matrix");
    }
}

static void peripheral_collision_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3200u, 0x1111u);
    dspic33_write_word(cpu, 0x3202u, 0x2222u);
    configure_channel(cpu, 0u, 0x2000u, 0xc0u, 0x3200u, 0u, 0x3300u, 1u);
    expect(state, dspic33_dma_request(cpu, 0xc0u, 0u, 1u),
           "queue peripheral write collision");
    dspic33_write_word(cpu, 0x3300u, 0x7777u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance peripheral write collision");
    expect(state, dspic33_read_word(cpu, 0x3300u) == 0x7777u,
           "CPU peripheral write prevails");
    expect(state, (dspic33_read_word(cpu, 0x0bf0u) & 1u) != 0u,
           "peripheral collision status");
    expect(state, cpu->last_trap == 5u && cpu->trap_count == 1u,
           "peripheral collision trap");
    expect(state, (dspic33_read_word(cpu, 0x08c0u) & 0x0020u) != 0u,
           "peripheral collision DMACERR");
    expect(state,
           dspic33_dma_request(cpu, 0xc0u, 0u, 0u) && dspic33_device_advance(cpu, 0u),
           "request blocked by collision");
    expect(state, dspic33_read_word(cpu, 0x3300u) == 0x7777u,
           "collision blocks subsequent transfer");
    dspic33_write_word(cpu, 0x08c0u,
                       (uint16_t)(dspic33_read_word(cpu, 0x08c0u) & ~0x0020u));
    expect(state, dspic33_read_word(cpu, 0x0bf0u) == 0u,
           "clearing DMACERR clears write collision");
    expect(state, request(cpu, 0xc0u, 0u), "request resumes after collision clear");
    expect(state, dspic33_read_word(cpu, 0x3300u) == 0x2222u,
           "transfer resumes after collision clear");
}

static void memory_collision_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3400u, 0x1111u);
    configure_channel(cpu, 0u, 0x2001u, 0xc1u, 0x3400u, 0u, 0x3500u, 0u);
    expect(state, dspic33_dma_request(cpu, 0xc1u, 0u, 1u),
           "queue concurrent DMA memory read");
    dspic33_write_word(cpu, 0x3400u, 0x2222u);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance concurrent DMA memory read");
    expect(state, dspic33_read_word(cpu, 0x3500u) == 0x1111u,
           "DMA reads pre-write memory value");
    expect(state, dspic33_read_word(cpu, 0x3400u) == 0x2222u,
           "CPU memory write remains visible");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3400u, 0xaaaau);
    dspic33_write_word(cpu, 0x3500u, 0x3333u);
    configure_channel(cpu, 0u, 0x0001u, 0xc2u, 0x3400u, 0u, 0x3500u, 0u);
    expect(state, dspic33_dma_request(cpu, 0xc2u, 0u, 1u),
           "queue concurrent DMA memory write");
    dspic33_write_word(cpu, 0x3400u, 0xbbbbu);
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance concurrent DMA memory write");
    expect(state, dspic33_read_word(cpu, 0x3400u) == 0xbbbbu,
           "CPU write wins concurrent DMA memory write");
    expect(state, dspic33_read_word(cpu, 0x0bf0u) == 0u,
           "memory collision does not set peripheral status");
}

static void stale_request_cases(DmaConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x2d00u, 0x1111u);
    configure_channel(cpu, 0u, 0x2000u, 0xa0u, 0x2d00u, 0u, 0x2e00u, 0u);
    expect(state, dspic33_dma_request(cpu, 0xa0u, 0u, 5u), "queue stale request");
    dspic33_write_word(cpu, 0x0b00u, 0x2000u);
    dspic33_write_word(cpu, 0x2d00u, 0x2222u);
    dspic33_write_word(cpu, 0x0b00u, 0xa000u);
    expect(state, dspic33_device_advance(cpu, 5u), "advance stale request");
    expect(state, dspic33_read_word(cpu, 0x2e00u) == 0u,
           "stale request discarded after re-enable");
    expect(state, request(cpu, 0xa0u, 0u), "new generation request");
    expect(state, dspic33_read_word(cpu, 0x2e00u) == 0x2222u,
           "new generation transfers");
}

int main(void) {
    DmaConformance state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize DMA processor");
    if (initialized) {
        register_cases(&state, &cpu);
        direction_and_width_cases(&state, &cpu);
        gpio_latch_cases(&state, &cpu);
        addressing_cases(&state, &cpu);
        operating_mode_cases(&state, &cpu);
        interrupt_and_null_cases(&state, &cpu);
        force_and_collision_cases(&state, &cpu);
        routing_and_status_cases(&state, &cpu);
        channel_matrix_cases(&state, &cpu);
        peripheral_collision_cases(&state, &cpu);
        memory_collision_cases(&state, &cpu);
        stale_request_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[dma-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
