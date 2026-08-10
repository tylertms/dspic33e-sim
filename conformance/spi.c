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
} SpiConformance;

static const uint16_t bases[DSPIC33_SPI_COUNT] = {0x0240u, 0x0260u, 0x02a0u, 0x02c0u};
static const uint8_t irqs[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};
static const uint8_t error_irqs[DSPIC33_SPI_COUNT] = {9u, 32u, 90u, 122u};
static const uint8_t requests[DSPIC33_SPI_COUNT] = {10u, 33u, 91u, 123u};

static void expect(SpiConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[spi-failed] %s\n", name);
    }
}

static bool interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    dspic33_write_word(
        cpu, address,
        (uint16_t)(dspic33_read_word(cpu, address) & ~(uint16_t)(1u << (irq % 16u))));
}

static uint64_t transfer_cycles(uint16_t control) {
    static const uint8_t primary[] = {64u, 16u, 4u, 1u};
    uint8_t secondary = (uint8_t)(8u - ((control >> 2u) & 7u));
    uint8_t bits = (control & 0x0400u) != 0u ? 16u : 8u;
    return (uint64_t)bits * primary[control & 3u] * secondary;
}

static void configure_spi(Dspic33* cpu, uint8_t channel, uint16_t control,
                          uint16_t control2, uint8_t interrupt_mode) {
    uint16_t base = bases[channel];
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), control);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), control2);
    dspic33_write_word(cpu, base,
                       (uint16_t)(0x8000u | ((uint16_t)interrupt_mode << 2u)));
}

static uint16_t dma_base(uint8_t channel) {
    return (uint16_t)(0x0b00u + channel * 0x10u);
}

static void configure_dma(Dspic33* cpu, uint8_t channel, uint16_t control,
                          uint8_t request, uint32_t memory, uint16_t pad,
                          uint16_t count) {
    uint16_t base = dma_base(channel);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), (uint16_t)memory);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(memory >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), count);
    dspic33_write_word(cpu, base, (uint16_t)(control | 0x8000u));
}

static void register_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        expect(state, dspic33_read_word(cpu, base) == 0u, "status reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u,
               "control one reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "control two reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "buffer reset");
        dspic33_write_word(cpu, base, 0xffffu);
        expect(state, dspic33_read_word(cpu, base) == 0xa01cu, "status mask");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x1fffu,
               "control one mask");
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0xe003u,
               "control two mask");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x0200u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u,
               "slave sample phase forced clear");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x0220u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x0220u,
               "master sample phase writable");
    }
}

static void timing_matrix_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t mode16;
    uint8_t primary;
    uint8_t secondary;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        for (mode16 = 0u; mode16 < 2u; mode16++) {
            for (primary = 0u; primary < 4u; primary++) {
                for (secondary = 0u; secondary < 8u; secondary++) {
                    uint16_t control;
                    uint16_t sent;
                    uint16_t received;
                    uint64_t cycles;
                    if (primary == 3u && secondary == 7u) {
                        continue;
                    }
                    control =
                        (uint16_t)(0x0020u | primary | ((uint16_t)secondary << 2u) |
                                   (mode16 != 0u ? 0x0400u : 0u));
                    sent = (uint16_t)(0x8100u | (channel << 4u) | (primary << 2u) |
                                      (secondary & 3u));
                    received = (uint16_t)(0x5a00u | (secondary << 4u) | primary);
                    cycles = transfer_cycles(control);
                    dspic33_reset(cpu, 0u);
                    configure_spi(cpu, channel, control, 0u, 0u);
                    expect(state, dspic33_spi_receive(cpu, channel, received, cycles),
                           "schedule master response matrix");
                    dspic33_write_word(cpu, (uint16_t)(bases[channel] + 8u), sent);
                    expect(state,
                           (dspic33_read_word(cpu, bases[channel]) & 0x0080u) == 0u,
                           "matrix shift register active");
                    expect(state, dspic33_device_advance(cpu, cycles - 1u),
                           "matrix advance before completion");
                    expect(state,
                           (dspic33_read_word(cpu, bases[channel]) & 0x0001u) == 0u,
                           "matrix not complete early");
                    expect(state, dspic33_device_advance(cpu, 1u),
                           "matrix completion advance");
                    expect(state,
                           (dspic33_read_word(cpu, bases[channel]) & 0x0001u) != 0u,
                           "matrix receive flag");
                    expect(
                        state,
                        dspic33_read_word(cpu, (uint16_t)(bases[channel] + 8u)) ==
                            (mode16 != 0u ? received : (uint16_t)(received & 0x00ffu)),
                        "matrix received value");
                    expect(state, interrupt_flag(cpu, irqs[channel]),
                           "matrix transfer interrupt");
                }
            }
        }
    }
}

static void standard_buffer_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t control = 0x043bu;
        uint64_t cycles = transfer_cycles(control);
        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, control, 0u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x1111u, cycles) &&
                   dspic33_spi_receive(cpu, channel, 0x2222u, cycles * 2u),
               "queue standard responses");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xaaaau);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xbbbbu);
        expect(state, (dspic33_read_word(cpu, base) & 0x0002u) != 0u,
               "standard transmit buffer full");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xccccu);
        expect(state, cpu->io.spi_tx_fifo[channel].count == 1u,
               "standard full write ignored");
        expect(state, dspic33_device_advance(cpu, cycles), "standard first completion");
        expect(state, (dspic33_read_word(cpu, base) & 0x0002u) == 0u,
               "standard queued word moved to shift");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x1111u,
               "standard first response order");
        clear_interrupt(cpu, irqs[channel]);
        expect(state, dspic33_device_advance(cpu, cycles),
               "standard second completion");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x2222u,
               "standard second response order");
        expect(state, interrupt_flag(cpu, irqs[channel]), "standard second interrupt");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0u, 0u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x0031u, 1u) &&
                   dspic33_spi_receive(cpu, channel, 0x0032u, 2u),
               "queue standard slave overflow");
        expect(state, dspic33_device_advance(cpu, 2u),
               "standard slave overflow advance");
        expect(state, (dspic33_read_word(cpu, base) & 0x0041u) == 0x0041u,
               "standard overflow flags");
        expect(state, interrupt_flag(cpu, error_irqs[channel]),
               "standard overflow error interrupt");
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) & ~0x0040u));
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) == 0u,
               "standard overflow software clear");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x0031u,
               "standard overflow preserves unread value");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x0033u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "standard receive resumes after clear");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x0033u,
               "standard recovered receive value");
    }
}

static void enhanced_fifo_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t control = 0x043bu;
        uint64_t cycles = transfer_cycles(control);
        uint8_t index;
        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, control, 1u, 5u);
        for (index = 0u; index < 9u; index++) {
            dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0x1000u + index));
        }
        expect(state, cpu->io.spi_tx_fifo[channel].count == 8u,
               "enhanced eight pending words");
        expect(state, (dspic33_read_word(cpu, base) & 0x0702u) == 0x0702u,
               "enhanced full count encoding");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xdeadu);
        expect(state, cpu->io.spi_tx_fifo[channel].count == 8u,
               "enhanced full write ignored");
        for (index = 0u; index < 9u; index++) {
            expect(state, dspic33_device_advance(cpu, cycles),
                   "enhanced transmit completion");
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) ==
                       (uint16_t)(0x1000u + index),
                   "enhanced transmit receive order");
        }
        expect(state, (dspic33_read_word(cpu, base) & 0x00a2u) == 0x00a0u,
               "enhanced queues empty");
        expect(state, interrupt_flag(cpu, irqs[channel]),
               "enhanced final completion interrupt");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x0400u, 1u, 3u);
        for (index = 0u; index < 8u; index++) {
            expect(state,
                   dspic33_spi_receive(cpu, channel, (uint16_t)(0x3000u + index), 1u) &&
                       dspic33_device_advance(cpu, 1u),
                   "enhanced slave receive fill");
        }
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u,
               "enhanced receive depth");
        expect(state, (dspic33_read_word(cpu, base) & 0x0721u) == 0x0701u,
               "enhanced receive full status");
        expect(state, interrupt_flag(cpu, irqs[channel]),
               "enhanced receive full interrupt");
        clear_interrupt(cpu, irqs[channel]);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3fffu, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "enhanced overflow advance");
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) != 0u,
               "enhanced overflow flag");
        expect(state, interrupt_flag(cpu, error_irqs[channel]),
               "enhanced overflow error interrupt");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3eeeu, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "enhanced overflow blocks receive");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u,
               "enhanced blocked receive preserves depth");
        dspic33_write_word(cpu, base,
                           (uint16_t)(dspic33_read_word(cpu, base) & ~0x0040u));
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) == 0u,
               "enhanced overflow software clear");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u,
               "enhanced clear preserves unread words");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x3000u,
               "enhanced first preserved word");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 7u,
               "enhanced read frees receive slot");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3dddu, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "enhanced receive resumes after clear");
        expect(state, (dspic33_read_word(cpu, base) & 0x0040u) == 0u,
               "enhanced recovered receive avoids overflow");
        expect(state, cpu->io.spi_rx_fifo[channel].count == 8u,
               "enhanced recovered receive fills slot");
        for (index = 1u; index < 8u; index++) {
            expect(state,
                   dspic33_read_word(cpu, (uint16_t)(base + 8u)) ==
                       (uint16_t)(0x3000u + index),
                   "enhanced receive read order");
        }
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x3dddu,
               "enhanced recovered receive tail");
        expect(state, (dspic33_read_word(cpu, base) & 0x0021u) == 0x0020u,
               "enhanced receive empty status");
    }
}

static void interrupt_mode_cases(SpiConformance* state, Dspic33* cpu) {
    uint16_t base = bases[0];
    uint16_t master = 0x043bu;
    uint64_t cycles = transfer_cycles(master);
    uint8_t index;

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, master, 1u, 7u);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1000u);
    for (index = 0u; index < 7u; index++) {
        dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0x1001u + index));
    }
    expect(state, !interrupt_flag(cpu, irqs[0]), "mode seven not early");
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1008u);
    expect(state, interrupt_flag(cpu, irqs[0]), "mode seven transmit full");

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, master, 1u, 6u);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x2000u);
    expect(state, interrupt_flag(cpu, irqs[0]), "mode six transmit fifo empty");

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, master, 1u, 5u);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x3000u);
    expect(state, !interrupt_flag(cpu, irqs[0]), "mode five not early");
    expect(state, dspic33_device_advance(cpu, cycles), "mode five completion advance");
    expect(state, interrupt_flag(cpu, irqs[0]), "mode five transfer complete");

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, master, 1u, 4u);
    for (index = 0u; index < 9u; index++) {
        dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)(0x4000u + index));
    }
    expect(state, !interrupt_flag(cpu, irqs[0]), "mode four not early");
    expect(state, dspic33_device_advance(cpu, cycles), "mode four opening advance");
    expect(state, interrupt_flag(cpu, irqs[0]), "mode four one opening");

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, 0x0400u, 1u, 1u);
    expect(state,
           dspic33_spi_receive(cpu, 0u, 0x5000u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode one receive advance");
    expect(state, interrupt_flag(cpu, irqs[0]), "mode one data received");

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, 0x0400u, 1u, 2u);
    for (index = 0u; index < 5u; index++) {
        expect(state,
               dspic33_spi_receive(cpu, 0u, index, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "mode two below threshold advance");
    }
    expect(state, !interrupt_flag(cpu, irqs[0]), "mode two below threshold");
    expect(state,
           dspic33_spi_receive(cpu, 0u, 5u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode two threshold advance");
    expect(state, interrupt_flag(cpu, irqs[0]), "mode two three quarters full");

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, 0x0400u, 1u, 3u);
    for (index = 0u; index < 7u; index++) {
        expect(state,
               dspic33_spi_receive(cpu, 0u, index, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "mode three below full advance");
    }
    expect(state, !interrupt_flag(cpu, irqs[0]), "mode three below full");
    expect(state,
           dspic33_spi_receive(cpu, 0u, 7u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode three full advance");
    expect(state, interrupt_flag(cpu, irqs[0]), "mode three receive full");

    dspic33_reset(cpu, 0u);
    configure_spi(cpu, 0u, 0x0400u, 1u, 0u);
    expect(state,
           dspic33_spi_receive(cpu, 0u, 0x6000u, 1u) && dspic33_device_advance(cpu, 1u),
           "mode zero receive advance");
    expect(state, !interrupt_flag(cpu, irqs[0]), "mode zero not on receive");
    expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x6000u,
           "mode zero read value");
    expect(state, interrupt_flag(cpu, irqs[0]), "mode zero receive empty");
}

static void mode_transition_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x003bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x00aau);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x043bu);
        expect(state,
               cpu->io.spi_tx_fifo[channel].count == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "mode width change resets transfer");
        expect(state, (dspic33_read_word(cpu, base) & 0x0003u) == 0u,
               "mode width change clears flags");

        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xaaaau);
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 1u);
        expect(state,
               cpu->io.spi_tx_fifo[channel].count == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "enhanced mode change resets transfer");
        expect(state, (dspic33_read_word(cpu, base) & 0x00a0u) == 0x00a0u,
               "enhanced mode reset status");

        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xbbbbu);
        dspic33_write_word(cpu, base, 0u);
        expect(state,
               cpu->io.spi_tx_fifo[channel].count == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "module disable resets transfer");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "module disable clears buffer");
    }
}

static void selection_and_frame_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x0480u, 0u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x1111u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "unselected slave transaction advance");
        expect(state, (dspic33_read_word(cpu, base) & 1u) == 0u,
               "unselected slave transaction ignored");
        expect(state,
               dspic33_spi_select(cpu, channel, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "select slave");
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x2222u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "selected slave transaction advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x2222u,
               "selected slave transaction received");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x0400u, 0xc000u, 0u);
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3333u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "inactive frame transaction advance");
        expect(state, (dspic33_read_word(cpu, base) & 1u) == 0u,
               "inactive frame transaction ignored");
        expect(state,
               dspic33_spi_select(cpu, channel, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_spi_receive(cpu, channel, 0x4444u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "active frame transaction advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x4444u,
               "active frame transaction received");
    }
}

static void clock_and_power_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t control = 0x143bu;
        uint64_t cycles = transfer_cycles(control);
        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, control, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1111u);
        expect(state, cpu->events.count == 0u, "disabled clock does not schedule");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x043bu);
        expect(state, cpu->events.count == 1u, "clock enable resumes transfer");
        expect(state, dspic33_device_advance(cpu, cycles),
               "resumed clock completion advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x1111u,
               "resumed clock transfer value");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x2222u);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state, dspic33_device_advance(cpu, cycles),
               "master sleep transfer advance");
        expect(state,
               (dspic33_read_word(cpu, base) & 1u) == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "master sleep aborts transfer");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x0400u, 0u, 0u);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_spi_receive(cpu, channel, 0x3333u, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "slave sleep transaction advance");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0x3333u,
               "slave sleep completes transfer");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x4444u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state, dspic33_device_advance(cpu, cycles),
               "master idle running advance");
        expect(state, (dspic33_read_word(cpu, base) & 1u) != 0u,
               "master idle continues by default");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, base, 0xa000u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x5555u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state, dspic33_device_advance(cpu, cycles),
               "master stopped idle advance");
        expect(state,
               (dspic33_read_word(cpu, base) & 1u) == 0u &&
                   (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "master stopped idle aborts transfer");
    }
}

static void pmd_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t pmd_address = channel < 2u ? 0x0760u : 0x076au;
        uint16_t pmd_bit = channel < 2u ? (uint16_t)(0x0008u << channel)
                                        : (uint16_t)(1u << (channel - 2u));
        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, 0x043bu, 0u, 0u);
        dspic33_write_word(cpu, pmd_address,
                           (uint16_t)(dspic33_read_word(cpu, pmd_address) | pmd_bit));
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xaaaau);
        expect(state, (cpu->io.spi_busy & (uint8_t)(1u << channel)) == 0u,
               "pmd blocks transfer");
        dspic33_write_word(cpu, pmd_address,
                           (uint16_t)(dspic33_read_word(cpu, pmd_address) & ~pmd_bit));
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xbbbbu);
        expect(state, (cpu->io.spi_busy & (uint8_t)(1u << channel)) != 0u,
               "pmd clear restores transfer");
    }
}

static void dma_cases(SpiConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_SPI_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t control = 0x043bu;
        uint64_t cycles = transfer_cycles(control);
        uint16_t rx_memory = (uint16_t)(0x5000u + channel * 0x20u);
        uint16_t tx_memory = (uint16_t)(rx_memory + 0x10u);
        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, control, 0u, 0u);
        configure_dma(cpu, 0u, 0x0001u, requests[channel], rx_memory,
                      (uint16_t)(base + 8u), 0u);
        expect(state, dspic33_spi_receive(cpu, channel, 0x6a00u + channel, cycles),
               "schedule dma receive response");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1000u + channel);
        expect(state, dspic33_device_advance(cpu, cycles),
               "dma receive completion advance");
        expect(state, dspic33_read_word(cpu, rx_memory) == 0x6a00u + channel,
               "dma receive value");
        expect(state,
               (dspic33_read_word(cpu, dma_base(0u)) & 0x8000u) != 0u &&
                   !interrupt_flag(cpu, 4u) && cpu->io.dma_index[0] == 0u,
               "dma receive active before completion");
        expect(state, dspic33_device_advance(cpu, 1u),
               "dma receive controller completion advance");
        expect(state,
               (dspic33_read_word(cpu, dma_base(0u)) & 0x8000u) == 0u &&
                   interrupt_flag(cpu, 4u),
               "dma receive one shot complete");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, control, 0u, 0u);
        dspic33_write_word(cpu, tx_memory, 0x7b00u + channel);
        configure_dma(cpu, 1u, 0x2001u, requests[channel], tx_memory,
                      (uint16_t)(base + 8u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x2000u + channel);
        expect(state, dspic33_device_advance(cpu, cycles),
               "dma transmit request advance");
        expect(state, cpu->io.spi_shift[channel] == 0x7b00u + channel,
               "dma transmit loads next shift");
        expect(state,
               (dspic33_read_word(cpu, dma_base(1u)) & 0x8000u) != 0u &&
                   !interrupt_flag(cpu, 14u) && cpu->io.dma_index[1] == 0u,
               "dma transmit active before completion");
        expect(state, dspic33_device_advance(cpu, 1u),
               "dma transmit controller completion advance");
        expect(state,
               (dspic33_read_word(cpu, dma_base(1u)) & 0x8000u) == 0u &&
                   interrupt_flag(cpu, 14u),
               "dma transmit one shot complete");
        expect(state, dspic33_device_advance(cpu, cycles),
               "dma transmitted word completion");
        expect(state, cpu->io.spi_tx[channel].count == 4u,
               "dma transmit trace contains two words");

        dspic33_reset(cpu, 0u);
        configure_spi(cpu, channel, control, 0u, 0u);
        dspic33_write_word(cpu, tx_memory, 0x8c00u + channel);
        configure_dma(cpu, 0u, 0x0001u, requests[channel], rx_memory,
                      (uint16_t)(base + 8u), 0u);
        configure_dma(cpu, 1u, 0x2001u, requests[channel], tx_memory,
                      (uint16_t)(base + 8u), 0u);
        expect(state, dspic33_spi_receive(cpu, channel, 0x9d00u + channel, cycles),
               "schedule duplex dma response");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x3000u + channel);
        expect(state, dspic33_device_advance(cpu, cycles),
               "duplex dma completion advance");
        expect(state, dspic33_read_word(cpu, rx_memory) == 0x9d00u + channel,
               "duplex dma receive value");
        expect(state, cpu->io.spi_shift[channel] == 0x8c00u + channel,
               "duplex dma transmit value");
    }
}

static void copy_and_reset_cases(SpiConformance* state, Dspic33* cpu, Dspic33* copy) {
    uint64_t cycles = transfer_cycles(0x043bu);
    dspic33_reset(cpu, 0u);
    dspic33_reset(copy, 0u);
    configure_spi(cpu, 0u, 0x043bu, 1u, 5u);
    dspic33_write_word(cpu, 0x0248u, 0xaaaau);
    dspic33_write_word(cpu, 0x0248u, 0xbbbbu);
    expect(state, dspic33_copy(copy, cpu), "copy spi state");
    expect(state,
           copy->io.spi_busy == cpu->io.spi_busy &&
               copy->io.spi_tx_fifo[0].count == cpu->io.spi_tx_fifo[0].count,
           "copied spi queues");
    expect(state, dspic33_device_advance(copy, cycles), "advance copied spi state");
    expect(state,
           copy->io.spi_rx_fifo[0].count == 1u && copy->io.spi_shift[0] == 0xbbbbu,
           "copied spi completes independently");
    expect(state, (dspic33_read_word(cpu, 0x0240u) & 1u) == 0u,
           "source spi remains pending");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.spi_busy == 0u && cpu->io.spi_tx_fifo[0].count == 0u &&
               cpu->io.spi_rx_fifo[0].count == 0u,
           "reset clears spi state");
    expect(state,
           dspic33_read_word(cpu, 0x0240u) == 0u &&
               dspic33_read_word(cpu, 0x0248u) == 0u,
           "reset clears spi registers");
}

int main(void) {
    SpiConformance state = {0u, 0u, 0u};
    Dspic33 cpu;
    Dspic33 copy;
    bool initialized = dspic33_initialize(&cpu);
    bool copy_initialized = dspic33_initialize(&copy);
    expect(&state, initialized, "initialize SPI processor");
    expect(&state, copy_initialized, "initialize SPI copy");
    if (initialized && copy_initialized) {
        register_cases(&state, &cpu);
        timing_matrix_cases(&state, &cpu);
        standard_buffer_cases(&state, &cpu);
        enhanced_fifo_cases(&state, &cpu);
        interrupt_mode_cases(&state, &cpu);
        mode_transition_cases(&state, &cpu);
        selection_and_frame_cases(&state, &cpu);
        clock_and_power_cases(&state, &cpu);
        pmd_cases(&state, &cpu);
        dma_cases(&state, &cpu);
        copy_and_reset_cases(&state, &cpu, &copy);
    }
    if (copy_initialized) {
        dspic33_destroy(&copy);
    }
    if (initialized) {
        dspic33_destroy(&cpu);
    }
    printf("[spi-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
