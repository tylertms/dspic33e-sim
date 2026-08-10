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
} DciConformance;

enum {
    DCI_CONTROL1 = 0x0280u,
    DCI_CONTROL2 = 0x0282u,
    DCI_CONTROL3 = 0x0284u,
    DCI_STATUS = 0x0286u,
    DCI_TRANSMIT_SLOTS = 0x0288u,
    DCI_RECEIVE_SLOTS = 0x028cu,
    DCI_RECEIVE_BASE = 0x0290u,
    DCI_TRANSMIT_BASE = 0x0298u,
    DCI_ENABLE = 0x8000u,
    DCI_STOP_IDLE = 0x2000u,
    DCI_LOOPBACK = 0x0800u,
    DCI_EXTERNAL_CLOCK = 0x0400u,
    DCI_EXTERNAL_FRAME = 0x0100u,
    DCI_UNDERFLOW_LAST = 0x0080u,
    DCI_TRISTATE = 0x0040u,
    DCI_RECEIVE_OVERFLOW = 0x0008u,
    DCI_RECEIVE_FULL = 0x0004u,
    DCI_TRANSMIT_UNDERFLOW = 0x0002u,
    DCI_TRANSMIT_EMPTY = 0x0001u,
    DCI_PMD = 0x0760u,
    DCI_PMD_MASK = 0x0100u,
    DCI_ERROR_IRQ = 59u,
    DCI_TRANSFER_IRQ = 60u,
    DCI_DMA_REQUEST = 0x3cu,
    DCI_VECTOR = 0x0200u
};

static void expect(DciConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[dci-failed] %s\n", name);
    }
}

static uint16_t configuration(uint8_t width, uint8_t slots, uint8_t buffers) {
    return (uint16_t)(((uint16_t)(buffers - 1u) << 10u) |
                      ((uint16_t)(slots - 1u) << 5u) | (width - 1u));
}

static bool clock_word(Dspic33* cpu, uint16_t value, bool frame_sync) {
    return dspic33_dci_clock(cpu, value, frame_sync, 0u) &&
           dspic33_device_advance(cpu, 0u);
}

static void configure_external(Dspic33* cpu, uint16_t control, uint8_t width,
                               uint8_t slots, uint8_t buffers, uint16_t transmit,
                               uint16_t receive) {
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(width, slots, buffers));
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, transmit);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, receive);
    dspic33_write_word(cpu, DCI_CONTROL1,
                       (uint16_t)(DCI_ENABLE | DCI_EXTERNAL_CLOCK | control));
}

static uint16_t interrupt_word(uint8_t irq, uint16_t base) {
    return (uint16_t)(base + (irq / 16u) * 2u);
}

static uint16_t interrupt_mask(uint8_t irq) { return (uint16_t)(1u << (irq % 16u)); }

static bool interrupt_set(Dspic33* cpu, uint8_t irq) {
    return (dspic33_read_word(cpu, interrupt_word(irq, 0x0800u)) &
            interrupt_mask(irq)) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t irq) {
    uint16_t address = interrupt_word(irq, 0x0800u);
    dspic33_write_word(
        cpu, address,
        (uint16_t)(dspic33_read_word(cpu, address) & ~interrupt_mask(irq)));
}

static void enable_interrupt(Dspic33* cpu, uint8_t irq, uint8_t priority) {
    uint16_t enable = interrupt_word(irq, 0x0820u);
    uint16_t ipc = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(
        cpu, enable, (uint16_t)(dspic33_read_word(cpu, enable) | interrupt_mask(irq)));
    dspic33_write_word(
        cpu, ipc,
        (uint16_t)((dspic33_read_word(cpu, ipc) & ~(uint16_t)(7u << shift)) |
                   (uint16_t)(priority << shift)));
    cpu->program[(0x0014u + irq * 2u) / 2u] = DCI_VECTOR;
}

static void configure_dma(Dspic33* cpu, uint8_t channel, uint16_t control,
                          uint32_t memory, uint16_t pad, uint16_t count,
                          uint8_t request) {
    uint16_t base = (uint16_t)(0x0b00u + channel * 0x10u);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), (uint16_t)memory);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(memory >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0au), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), count);
    dspic33_write_word(cpu, base, (uint16_t)(control | 0x8000u));
}

static void access_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < 4u; index++) {
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(DCI_RECEIVE_BASE + index * 2u)) == 0u,
               "RXBUF deterministic POR value");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(DCI_TRANSMIT_BASE + index * 2u)) == 0u,
               "TXBUF reads zero at POR");
    }
    expect(state, dspic33_read_word(cpu, DCI_CONTROL1) == 0u, "DCICON1 reset");
    expect(state, dspic33_read_word(cpu, DCI_CONTROL2) == 0u, "DCICON2 reset");
    expect(state, dspic33_read_word(cpu, DCI_CONTROL3) == 0u, "DCICON3 reset");
    expect(state, dspic33_read_word(cpu, DCI_STATUS) == 0u, "DCISTAT reset");
    expect(state, dspic33_read_word(cpu, DCI_TRANSMIT_SLOTS) == 0u, "TSCON reset");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_SLOTS) == 0u, "RSCON reset");

    dspic33_write_word(cpu, DCI_CONTROL1, UINT16_MAX);
    dspic33_write_word(cpu, DCI_CONTROL2, UINT16_MAX);
    dspic33_write_word(cpu, DCI_CONTROL3, UINT16_MAX);
    dspic33_write_word(cpu, DCI_STATUS, UINT16_MAX);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, UINT16_MAX);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, UINT16_MAX);
    expect(state, dspic33_read_word(cpu, DCI_CONTROL1) == 0xafe3u,
           "DCICON1 access mask");
    expect(state, dspic33_read_word(cpu, DCI_CONTROL2) == 0x0defu,
           "DCICON2 access mask");
    expect(state, dspic33_read_word(cpu, DCI_CONTROL3) == 0x0fffu,
           "DCICON3 access mask");
    expect(state, dspic33_read_word(cpu, DCI_STATUS) == 0u, "DCISTAT is read only");
    expect(state, dspic33_read_word(cpu, DCI_TRANSMIT_SLOTS) == UINT16_MAX,
           "TSCON access mask");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_SLOTS) == UINT16_MAX,
           "RSCON access mask");
    for (index = 0u; index < 4u; index++) {
        uint16_t rx = (uint16_t)(DCI_RECEIVE_BASE + index * 2u);
        uint16_t tx = (uint16_t)(DCI_TRANSMIT_BASE + index * 2u);
        dspic33_write_word(cpu, rx, 0x55aau);
        dspic33_write_word(cpu, tx, (uint16_t)(0x1000u + index));
        expect(state, dspic33_read_word(cpu, rx) == 0u, "RXBUF rejects CPU write");
        expect(state, dspic33_read_word(cpu, tx) == 0u, "TXBUF is write only");
    }
}

static void width_and_lane_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t width;
    for (width = 4u; width <= 16u; width++) {
        Dspic33DciTransfer transfer;
        uint16_t mask =
            width == 16u ? UINT16_MAX : (uint16_t)(UINT16_MAX << (16u - width));
        uint16_t value = (uint16_t)(0xa55au & mask);
        dspic33_reset(cpu, 0u);
        configure_external(cpu, 0u, width, 1u, 1u, 1u, 1u);
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, value);
        expect(state, clock_word(cpu, 0x5aa5u, false), "clock width event");
        expect(state,
               dspic33_dci_transmit(cpu, &transfer) && transfer.value == value &&
                   transfer.slot == 0u && transfer.driven,
               "word width masks left-justified transmit data");
        expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == (0x5aa5u & mask),
               "word width masks left-justified receive data");
        expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_RECEIVE_FULL) == 0u,
               "RXBUF word read clears RFUL");
    }

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, clock_word(cpu, 0x7b6au, false), "clock byte-read fixture");
    expect(state, dspic33_read_byte(cpu, DCI_RECEIVE_BASE) == 0x6au,
           "RXBUF low-byte read returns low lane");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_RECEIVE_FULL) == 0u,
           "RXBUF byte read clears RFUL");
}

static void slot_buffer_status_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t index;
    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 4u, 4u, 0x000fu, 0x000fu);
    for (index = 0u; index < 4u; index++) {
        dspic33_write_word(cpu, (uint16_t)(DCI_TRANSMIT_BASE + index * 2u),
                           (uint16_t)(0x1000u + index));
    }
    for (index = 0u; index < 4u; index++) {
        expect(state, clock_word(cpu, (uint16_t)(0x2000u + index), false),
               "clock multibuffer word");
    }
    for (index = 0u; index < 4u; index++) {
        Dspic33DciTransfer transfer;
        expect(state,
               dspic33_dci_transmit(cpu, &transfer) &&
                   transfer.value == (uint16_t)(0x1000u + index) &&
                   transfer.slot == index,
               "shared pointer selects transmit buffer by slot");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(DCI_RECEIVE_BASE + index * 2u)) ==
                   (uint16_t)(0x2000u + index),
               "shared pointer selects receive buffer by slot");
    }
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) &
            (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW)) == 0u,
           "reading every RXBUF clears receive status");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state,
           clock_word(cpu, 0x1010u, false) &&
               (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_EMPTY) == 0u,
           "receive-only transfer leaves TMPTY clear");
    dspic33_read_word(cpu, DCI_RECEIVE_BASE);
    expect(state, clock_word(cpu, 0x1111u, false), "clock first unread block");
    expect(state, clock_word(cpu, 0x2222u, false), "clock overflowing block");
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) &
            (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW)) ==
               (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW),
           "unread replacement sets RFUL and ROV");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2222u,
           "overflow keeps newest completed receive word");
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) &
            (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW)) == 0u,
           "culprit RXBUF read clears RFUL and ROV");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 2u, 4u, 0x0001u, 0x0003u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    expect(state, clock_word(cpu, 0x3000u, false),
           "mixed-slot startup loads active TXBUF0");
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 2u, 0x2222u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 6u, 0x4444u);
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_EMPTY) != 0u,
           "inactive TXBUF writes do not suppress TMPTY");
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_EMPTY) == 0u,
           "active TXBUF0 write clears TMPTY");
    expect(state,
           clock_word(cpu, 0x3001u, false) && clock_word(cpu, 0x3002u, false) &&
               clock_word(cpu, 0x3003u, false),
           "mixed-slot pointer reaches TXBUF2");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_UNDERFLOW) != 0u,
           "mixed-slot pointer detects unwritten active TXBUF2");
    expect(state, (cpu->io.dci.transmit_underflow & 0x0au) == 0u,
           "inactive TXBUF1 and TXBUF3 never underflow");
}

static void admission_and_clock_cases(DciConformance* state, Dspic33* cpu) {
    static const uint16_t unsupported[] = {0x0001u, 0x0002u, 0x0003u, 0x0200u, 0x0020u};
    uint8_t index;
    for (index = 0u; index < sizeof(unsupported) / sizeof(unsupported[0]); index++) {
        Dspic33DciTransfer transfer;
        dspic33_reset(cpu, 0u);
        configure_external(cpu, unsupported[index], 16u, 1u, 1u, 1u, 1u);
        expect(state, clock_word(cpu, 0x1234u, true), "clock unsupported tuple");
        expect(state,
               !dspic33_dci_transmit(cpu, &transfer) &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "unsupported tuple produces no DCI behavior");
    }

    dspic33_reset(cpu, 0u);
    configure_external(cpu, DCI_EXTERNAL_FRAME, 16u, 2u, 1u, 0u, 1u);
    expect(state, clock_word(cpu, 0x1111u, false), "external frame waits without FS");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "external frame ignores CSCK before FS");
    expect(state, clock_word(cpu, 0x2222u, true), "external frame starts on FS");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2222u,
           "external frame captures synchronized first slot");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, DCI_EXTERNAL_FRAME, 16u, 2u, 1u, 0u, 0x0003u);
    expect(state, clock_word(cpu, 0x1234u, true), "start two-slot external frame");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & 0x0f00u) == 0x0100u,
           "SLOT reports second slot after first frame word");
    expect(state, clock_word(cpu, 0x5678u, true), "clock spurious FS during frame");
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) & 0x0f00u) == 0u &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5678u,
           "active frame ignores FS restart and completes slot one");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state, dspic33_device_advance(cpu, 1u), "apply DCI PMD disable");
    expect(state, clock_word(cpu, 0x3333u, false),
           "clock external DCI while PMD is effective");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "apply DCI PMD enable");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "PMD-blocked clock leaves no receive word after re-enable");
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_write_word(cpu, DCI_CONTROL1,
                       DCI_ENABLE | DCI_EXTERNAL_CLOCK | DCI_STOP_IDLE);
    expect(state,
           clock_word(cpu, 0x4444u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "DCISIDL stops external DCI in Idle");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK);
    expect(state,
           clock_word(cpu, 0x5555u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5555u,
           "external DCI continues in Idle when DCISIDL is clear");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           clock_word(cpu, 0x6666u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6666u,
           "external DCI continues in Sleep");
    cpu->power_state = DSPIC33_POWER_ACTIVE;

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state,
           clock_word(cpu, 0xb000u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "COFSD output ignores external frame-sync input");
    expect(state,
           dspic33_device_advance(cpu, 64u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "BCG zero disables internal DCI clock");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK);
    expect(state,
           clock_word(cpu, 0x9090u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "external DCI rejects nonzero BCG");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state,
           dspic33_device_advance(cpu, 11u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "internal clock waits three CSCK startup cycles");
    expect(state, dspic33_device_advance(cpu, 1u), "internal startup completes");
    dspic33_dci_input(cpu, 0xa000u);
    expect(state,
           dspic33_device_advance(cpu, 15u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "internal four-bit word waits two shifts per bit");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "internal four-bit word completes after sixteen cycles");

    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_dci_input(cpu, 0x5000u);
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "BCG zero freezes a running internal DCI clock");
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           dspic33_device_advance(cpu, 16u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "restored BCG resumes internal DCI clock");

    dspic33_dci_input(cpu, 0x6000u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_device_advance(cpu, 32u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "Sleep stops internal DCI clock");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state,
           dspic33_device_advance(cpu, 16u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6000u,
           "internal DCI clock resumes after Sleep");
}

static void mode_and_status_cases(DciConformance* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    uint8_t mode;
    for (mode = 0u; mode < 2u; mode++) {
        dspic33_reset(cpu, 0u);
        configure_external(cpu, mode != 0u ? DCI_UNDERFLOW_LAST : 0u, 16u, 1u, 1u, 1u,
                           0u);
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x5a5au);
        expect(state, clock_word(cpu, 0u, false), "clock initial underflow mode word");
        expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x5a5au,
               "initial TXBUF value reaches DCI output");
        expect(state, clock_word(cpu, 0u, false), "clock underflow replacement word");
        expect(state,
               dspic33_dci_transmit(cpu, &transfer) &&
                   transfer.value == (mode != 0u ? 0x5a5au : 0u),
               "UNFM selects last value or zero after underflow");
        expect(state,
               (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_UNDERFLOW) != 0u,
               "missing TXBUF sets TUNF");
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x6b6bu);
        expect(state,
               (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_UNDERFLOW) == 0u,
               "culprit TXBUF write clears TUNF");
    }

    dspic33_reset(cpu, 0u);
    configure_external(cpu, DCI_LOOPBACK, 16u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1357u);
    expect(state, clock_word(cpu, 0x2468u, false), "clock loopback word");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x1357u,
           "DLOOP routes transmit shadow into receive buffer");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 2u, 1u, 1u, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x7777u);
    expect(state, clock_word(cpu, 0u, false), "clock driven active slot");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.driven,
           "enabled transmit slot drives output");
    expect(state, clock_word(cpu, 0u, false), "clock driven disabled slot");
    expect(state,
           dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0u &&
               transfer.driven,
           "CSDOM zero drives disabled slot low");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, DCI_TRISTATE, 16u, 2u, 1u, 1u, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x8888u);
    expect(state, clock_word(cpu, 0u, false), "clock tristate active slot");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.driven,
           "CSDOM one still drives enabled transmit slot");
    expect(state, clock_word(cpu, 0u, false), "clock tristate disabled slot");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && !transfer.driven,
           "CSDOM one tri-states disabled slot");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 1u, 2u, 0u, 1u);
    expect(state, clock_word(cpu, 0x1001u, false) && clock_word(cpu, 0x1002u, false),
           "fill two receive buffers");
    expect(state, cpu->io.dci.receive_unread == 0x03u,
           "RFUL tracks both unread RXBUFs");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x1001u,
           "read first RXBUF independently");
    expect(state,
           cpu->io.dci.receive_unread == 0x02u &&
               (dspic33_read_word(cpu, DCI_STATUS) & DCI_RECEIVE_FULL) != 0u,
           "RFUL remains until every active RXBUF is read");
    expect(state, clock_word(cpu, 0x2001u, false) && clock_word(cpu, 0x2002u, false),
           "overwrite partially unread receive block");
    expect(state, cpu->io.dci.receive_overflow == 0x02u,
           "ROV identifies only the unread overwritten RXBUF");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2001u,
           "nonculprit RXBUF read preserves ROV");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_RECEIVE_OVERFLOW) != 0u,
           "ROV persists until culprit RXBUF read");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE + 2u) == 0x2002u,
           "read overflow culprit RXBUF");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_RECEIVE_OVERFLOW) == 0u,
           "culprit RXBUF read clears ROV");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(16u, 2u, 4u));
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 4u, 0x3333u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 0x0001u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 0x0003u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK);
    expect(state, clock_word(cpu, 0u, false), "clock preloaded TXBUF0");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x1111u,
           "TXBUF preload before TSCON supplies active buffer zero");
    expect(state, clock_word(cpu, 0u, false) && clock_word(cpu, 0u, false),
           "advance mixed slots to preloaded TXBUF2");
    expect(state,
           dspic33_dci_transmit(cpu, &transfer) &&
               dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x3333u,
           "TXBUF preload before TSCON supplies active buffer two");
}

static void interrupt_dma_cases(DciConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    enable_interrupt(cpu, DCI_ERROR_IRQ, 4u);
    enable_interrupt(cpu, DCI_TRANSFER_IRQ, 3u);
    cpu->w[15] = 0x1800u;
    configure_external(cpu, 0u, 16u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x3456u);
    expect(state, clock_word(cpu, 0x1234u, false), "clock IRQ fixture");
    expect(state, interrupt_set(cpu, DCI_TRANSFER_IRQ), "block transfer raises IRQ60");
    expect(state, interrupt_set(cpu, DCI_ERROR_IRQ), "TX underflow raises IRQ59");
    expect(state,
           dspic33_device_service_interrupt(cpu) &&
               cpu->last_interrupt == DCI_ERROR_IRQ,
           "higher-priority DCI error vectors first");
    expect(state, cpu->pc == DCI_VECTOR, "DCI error uses IRQ59 vector");
    clear_interrupt(cpu, DCI_ERROR_IRQ);

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x4000u, 0x7a5au);
    configure_dma(cpu, 1u, 0x2001u, 0x4000u, DCI_TRANSMIT_BASE, 0u, DCI_DMA_REQUEST);
    configure_external(cpu, 0u, 16u, 1u, 1u, 1u, 0u);
    expect(state, clock_word(cpu, 0u, false), "clock initial TX DMA block");
    expect(state, dspic33_device_advance(cpu, 2u), "complete DCI TX DMA transfer");
    expect(state, clock_word(cpu, 0u, false), "clock loaded TX DMA block");
    expect(state, cpu->io.dci.last_transmit[0] == 0x7a5au,
           "DCI TX DMA copies RAM to TXBUF0");

    dspic33_reset(cpu, 0u);
    configure_dma(cpu, 0u, 0x0001u, 0x4100u, DCI_RECEIVE_BASE, 0u, DCI_DMA_REQUEST);
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, clock_word(cpu, 0x6b4bu, false), "clock RX DMA block");
    expect(state, dspic33_device_advance(cpu, 2u), "complete DCI RX DMA transfer");
    expect(state, dspic33_read_word(cpu, 0x4100u) == 0x6b4bu,
           "DCI RX DMA copies RXBUF0 to RAM");

    dspic33_reset(cpu, 0u);
    configure_dma(cpu, 0u, 0x0001u, 0x4200u, DCI_RECEIVE_BASE, 0u, DCI_DMA_REQUEST);
    configure_external(cpu, 0u, 16u, 1u, 2u, 0u, 1u);
    expect(state, clock_word(cpu, 0x7777u, false) && dspic33_device_advance(cpu, 2u),
           "clock BLEN greater than zero fixture");
    expect(state, dspic33_read_word(cpu, 0x4200u) == 0u,
           "BLEN greater than zero suppresses undefined DMA request");

    dspic33_reset(cpu, 0u);
    configure_dma(cpu, 0u, 0x0001u, 0x4300u, DCI_RECEIVE_BASE, 0u, 0x3bu);
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, clock_word(cpu, 0x8888u, false) && dspic33_device_advance(cpu, 2u),
           "clock wrong IRQSEL fixture");
    expect(state, dspic33_read_word(cpu, 0x4300u) == 0u,
           "wrong IRQSEL receives no DCI DMA request");

    dspic33_reset(cpu, 0u);
    enable_interrupt(cpu, DCI_TRANSFER_IRQ, 3u);
    cpu->w[15] = 0x1800u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, clock_word(cpu, 0xcccdu, false), "clock sleeping DCI fixture");
    expect(state, interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "sleeping external DCI sets IRQ60");
    expect(state, dspic33_device_advance(cpu, 1u),
           "advance sleeping DCI interrupt eligibility");
    expect(state,
           dspic33_device_wake(cpu) && cpu->last_interrupt == DCI_TRANSFER_IRQ &&
               cpu->pc == DCI_VECTOR,
           "external DCI wakes from Sleep through IRQ60 vector");

    dspic33_reset(cpu, 0u);
    enable_interrupt(cpu, DCI_TRANSFER_IRQ, 2u);
    cpu->sr = 0x0040u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, clock_word(cpu, 0xdedeu, false), "clock equal-priority DCI fixture");
    expect(state,
           dspic33_device_wake(cpu) && cpu->interrupt_count == 0u &&
               interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "equal-priority DCI wakes without vector and retains IRQ60");
}

static void generation_and_frame_cases(DciConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 1u), "advance before stale DCI start");
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state,
           dspic33_device_advance(cpu, 11u) && cpu->io.dci.internal_scheduled &&
               !cpu->io.dci.initialized,
           "stale start event cannot clear current scheduling latch");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.dci.initialized,
           "current generation start event initializes DCI");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_FRAME);
    expect(state, clock_word(cpu, 0x1111u, true),
           "external FS before internal startup is accepted as physical input");
    expect(state,
           dspic33_device_advance(cpu, 96u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "pre-start and absent FS cannot arm internal-frame transfer");
    expect(state, clock_word(cpu, 0x2222u, true),
           "external FS arms initialized internal-frame transfer");
    expect(state,
           dspic33_device_advance(cpu, 63u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "internal-frame word remains pending before full interval");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2222u,
           "internal-frame word completes after external FS");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled &&
               cpu->io.dci.pmd_generation == 2u,
           "rapid PMD toggle rejects stale disabled generation");
}

static void lifecycle_cases(DciConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33DciTransfer transfer;
    bool initialized;
    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 2u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1234u);
    expect(state, clock_word(cpu, 0x5678u, false), "capture before DCI disable");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_EXTERNAL_CLOCK);
    expect(state, cpu->io.dci.disable_pending, "DCIEN clear defers through frame end");
    expect(state, clock_word(cpu, 0x9abcu, false), "finish disabled DCI frame");
    expect(state, !cpu->io.dci.disable_pending,
           "DCIEN clear completes deferred disable at frame end");
    expect(state, !cpu->io.dci.started, "DCIEN clear stops engine after current frame");
    expect(state, (dspic33_read_word(cpu, DCI_CONTROL1) & DCI_ENABLE) == 0u,
           "DCIEN remains clear after frame completion");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    dspic33_dci_input(cpu, 0x4a4au);
    expect(state, clock_word(cpu, 0x5b5bu, false), "capture warm-reset RXBUF");
    dspic33_dci_input(cpu, 0x4a4au);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute DCI warm reset");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5b5bu &&
               cpu->io.dci.input == 0x4a4au &&
               dspic33_read_word(cpu, DCI_CONTROL1) == 0u,
           "warm reset preserves RXBUF and physical input but resets DCI engine");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u && cpu->io.dci.input == 0u,
           "POR clears deterministic RXBUF and physical input state");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize DCI copy destination");
    if (initialized) {
        dspic33_reset(cpu, 0u);
        configure_external(cpu, 0u, 16u, 1u, 1u, 1u, 1u);
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x2468u);
        expect(state,
               dspic33_dci_clock(cpu, 0x1357u, false, 2u) && dspic33_copy(&copy, cpu),
               "copy DCI state with pending clock event");
        expect(state,
               dspic33_device_advance(&copy, 2u) &&
                   dspic33_dci_transmit(&copy, &transfer) &&
                   transfer.value == 0x2468u &&
                   dspic33_read_word(&copy, DCI_RECEIVE_BASE) == 0x1357u &&
                   cpu->events.count == 1u,
               "copied DCI event executes independently");
        dspic33_destroy(&copy);
    }

    dspic33_reset(cpu, 0u);
    configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    cpu->io.dci.output.count = DSPIC33_DCI_QUEUE_SIZE;
    expect(state, !clock_word(cpu, 0xaaaau, false), "clock full output queue");
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
               (dspic33_read_word(cpu, DCI_CONTROL1) & DCI_ENABLE) == 0u,
           "output queue overflow deterministically aborts DCI");
    cpu->stop_reason = DSPIC33_RUNNING;

    dspic33_reset(cpu, 0u);
    {
        uint64_t cycles = cpu->device_cycles;
        size_t queued = cpu->events.count;
        cpu->device_cycles = UINT64_MAX;
        expect(state,
               !dspic33_dci_clock(cpu, 0x1234u, false, 1u) &&
                   cpu->events.count == queued,
               "DCI clock scheduling failure queues no partial event");
        cpu->device_cycles = cycles;
    }

    dspic33_reset(cpu, 0u);
    {
        uint64_t cycles = cpu->device_cycles;
        cpu->device_cycles = UINT64_MAX;
        dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
        dspic33_write_word(cpu, DCI_CONTROL3, 1u);
        dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
        dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
        expect(state,
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
                   (dspic33_read_word(cpu, DCI_CONTROL1) & DCI_ENABLE) == 0u &&
                   cpu->events.count == 0u,
               "internal start schedule failure aborts DCI without stale event");
        cpu->device_cycles = cycles;
        cpu->stop_reason = DSPIC33_RUNNING;
    }

    dspic33_reset(cpu, 0u);
    {
        uint64_t cycles = cpu->device_cycles;
        uint16_t generation = cpu->io.dci.pmd_generation;
        cpu->device_cycles = UINT64_MAX;
        dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
        expect(state,
               dspic33_read_word(cpu, DCI_PMD) == 0u &&
                   cpu->io.dci.pmd_generation == (uint16_t)(generation + 2u) &&
                   !cpu->io.dci.pmd_disabled && cpu->events.count == 0u &&
                   cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
               "DCI PMD schedule failure rolls back and invalidates transition");
        cpu->device_cycles = cycles;
        cpu->stop_reason = DSPIC33_RUNNING;
    }
}

int main(void) {
    Dspic33 cpu;
    DciConformance state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize DCI processor");
    if (initialized) {
        access_cases(&state, &cpu);
        width_and_lane_cases(&state, &cpu);
        slot_buffer_status_cases(&state, &cpu);
        admission_and_clock_cases(&state, &cpu);
        mode_and_status_cases(&state, &cpu);
        interrupt_dma_cases(&state, &cpu);
        generation_and_frame_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        expect(&state, state.cases == 230u, "DCI assertion accounting");
        dspic33_destroy(&cpu);
    }
    printf("[dci-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
