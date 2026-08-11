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
    DCI_DATA_JUSTIFY = 0x0020u,
    DCI_SAMPLE_RISING = 0x0200u,
    DCI_MODE_I2S = 0x0001u,
    DCI_MODE_AC_LINK_16 = 0x0002u,
    DCI_MODE_AC_LINK_20 = 0x0003u,
    DCI_RECEIVE_OVERFLOW = 0x0008u,
    DCI_RECEIVE_FULL = 0x0004u,
    DCI_TRANSMIT_UNDERFLOW = 0x0002u,
    DCI_TRANSMIT_EMPTY = 0x0001u,
    DCI_PMD = 0x0760u,
    DCI_PMD_MASK = 0x0100u,
    DCI_ERROR_IRQ = 59u,
    DCI_TRANSFER_IRQ = 60u,
    DCI_DMA_REQUEST = 0x3cu,
    DCI_VECTOR = 0x0200u,
    DCI_PPS_INPUTS = 0x06d0u,
    DCI_PPS_FRAME = 0x06d2u,
    DCI_PPS_DATA_OUTPUT = 0x0682u,
    DCI_PPS_CLOCK_FRAME_OUTPUT = 0x0684u,
    GPIO_TRIS_D = 0x0e30u,
    GPIO_ANALOG_D = 0x0e3eu,
    GPIO_PORT_D = 3u,
    GPIO_DATA_MASK = 0x0001u,
    GPIO_CLOCK_MASK = 0x0002u,
    GPIO_FRAME_MASK = 0x0004u,
    GPIO_ANALOG_CLOCK_MASK = 0x0040u,
    PPS_DATA_PIN = 64u,
    PPS_CLOCK_PIN = 65u,
    PPS_FRAME_PIN = 66u,
    PPS_DATA_OUTPUT_PIN = 67u,
    PPS_CLOCK_OUTPUT_PIN = 68u,
    PPS_FRAME_OUTPUT_PIN = 69u,
    PPS_ANALOG_CLOCK_PIN = 70u,
    OPCODE_SLEEP = 0xfe4000u,
    OPCODE_IDLE = 0xfe4001u
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

static void configure_serial_pins(Dspic33* cpu) {
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~0x0007u));
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | 0x0007u));
    dspic33_write_word(cpu, DCI_PPS_INPUTS,
                       (uint16_t)((PPS_CLOCK_PIN << 8u) | PPS_DATA_PIN));
    dspic33_write_word(cpu, DCI_PPS_FRAME, PPS_FRAME_PIN);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, 0x0007u);
}

static bool drive_serial_pin_bit(Dspic33* cpu, bool high, bool rising,
                                 uint16_t clock_mask) {
    uint16_t data = high ? GPIO_DATA_MASK : 0u;
    uint16_t initial_clock = rising ? 0u : clock_mask;
    uint16_t sample_clock = rising ? clock_mask : 0u;
    return dspic33_gpio_drive(cpu, GPIO_PORT_D, data, GPIO_DATA_MASK) &&
           dspic33_gpio_drive(cpu, GPIO_PORT_D, initial_clock, clock_mask) &&
           dspic33_gpio_drive(cpu, GPIO_PORT_D, sample_clock, clock_mask);
}

static bool drive_mapped_serial_word(Dspic33* cpu, uint16_t value, uint8_t width,
                                     bool rising, uint16_t data_mask,
                                     uint16_t clock_mask) {
    uint8_t bit;
    for (bit = 0u; bit < width; bit++) {
        uint16_t data =
            bit < 16u && (value & (uint16_t)(0x8000u >> bit)) != 0u ? data_mask : 0u;
        uint16_t initial_clock = rising ? 0u : clock_mask;
        uint16_t sample_clock = rising ? clock_mask : 0u;
        if (!dspic33_gpio_drive(cpu, GPIO_PORT_D, data, data_mask) ||
            !dspic33_gpio_drive(cpu, GPIO_PORT_D, initial_clock, clock_mask) ||
            !dspic33_gpio_drive(cpu, GPIO_PORT_D, sample_clock, clock_mask)) {
            return false;
        }
    }
    return true;
}

static bool drive_serial_pin_word(Dspic33* cpu, uint16_t value, uint8_t width,
                                  bool rising, uint16_t clock_mask) {
    uint8_t bit;
    for (bit = 0u; bit < width; bit++) {
        bool high = bit < 16u && (value & (uint16_t)(0x8000u >> bit)) != 0u;
        if (!drive_serial_pin_bit(cpu, high, rising, clock_mask)) {
            return false;
        }
    }
    return true;
}

static bool drive_serial_bit(Dspic33* cpu, bool high, bool rising) {
    return drive_serial_pin_bit(cpu, high, rising, GPIO_CLOCK_MASK);
}

static bool drive_serial_word(Dspic33* cpu, uint16_t value, uint8_t width,
                              bool rising) {
    return drive_serial_pin_word(cpu, value, width, rising, GPIO_CLOCK_MASK);
}

static uint16_t serial_word_mask(uint8_t width) {
    return width == 16u ? UINT16_MAX : (uint16_t)(UINT16_MAX << (16u - width));
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
    uint8_t width;
    for (width = 1u; width < 4u; width++) {
        Dspic33DciTransfer transfer;
        dspic33_reset(cpu, 0u);
        configure_external(cpu, 0u, width, 1u, 1u, 1u, 1u);
        expect(state, clock_word(cpu, 0x1234u, true), "clock invalid word width");
        expect(state,
               !dspic33_dci_transmit(cpu, &transfer) &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "invalid word width produces no DCI behavior");
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

static void protocol_geometry_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 0u; mode < 4u; mode++) {
        uint8_t width;
        for (width = 4u; width <= 16u; width++) {
            uint8_t frames;
            for (frames = 1u; frames <= 16u; frames++) {
                uint8_t options;
                for (options = 0u; options < 4u; options++) {
                    Dspic33DciTransfer transfer;
                    uint16_t control = mode;
                    uint16_t expected_mask = UINT16_MAX;
                    uint8_t expected_frames = frames;
                    uint16_t transmit = (uint16_t)(0xa55au + frames + width);
                    uint16_t receive = (uint16_t)(0x5aa5u + frames + width);
                    if ((options & 1u) != 0u) {
                        control |= DCI_DATA_JUSTIFY;
                    }
                    if ((options & 2u) != 0u) {
                        control |= DCI_SAMPLE_RISING;
                    }
                    if (mode < DCI_MODE_AC_LINK_16 && width < 16u) {
                        expected_mask = (uint16_t)(UINT16_MAX << (16u - width));
                    }
                    if (mode == DCI_MODE_AC_LINK_16) {
                        expected_frames = 13u;
                    } else if (mode == DCI_MODE_AC_LINK_20) {
                        expected_frames = 16u;
                    }
                    dspic33_reset(cpu, 0u);
                    configure_external(cpu, control, width, frames, 1u, 1u, 1u);
                    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, transmit);
                    expect(state, clock_word(cpu, receive, false),
                           "clock protocol geometry word");
                    expect(state,
                           dspic33_dci_transmit(cpu, &transfer) &&
                               transfer.slot == 0u &&
                               transfer.value == (transmit & expected_mask) &&
                               transfer.driven,
                           "protocol selects effective transmit geometry");
                    expect(state,
                           dspic33_read_word(cpu, DCI_RECEIVE_BASE) ==
                               (receive & expected_mask),
                           "protocol selects effective receive geometry");
                    expect(state, cpu->io.dci.slot == (expected_frames == 1u ? 0u : 1u),
                           "protocol selects effective frame geometry");
                }
            }
        }
    }
}

static void protocol_frame_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t justification;
    for (justification = 0u; justification < 2u; justification++) {
        uint16_t control = (uint16_t)(DCI_MODE_I2S | DCI_EXTERNAL_FRAME |
                                      (justification != 0u ? DCI_DATA_JUSTIFY : 0u));
        dspic33_reset(cpu, 0u);
        configure_external(cpu, control, 8u, 2u, 2u, 0u, 3u);
        expect(state, clock_word(cpu, 0x1100u, false),
               "I2S slave waits for frame edge");
        expect(state, cpu->io.dci.receive_buffered == 0u && !cpu->io.dci.started,
               "I2S absent frame edge leaves transfer idle");
        expect(state, clock_word(cpu, 0x2200u, true),
               "I2S frame edge starts half-frame");
        expect(state, cpu->io.dci.receive_buffered == 1u && cpu->io.dci.slot == 1u,
               "I2S frame edge transfers first logical word");
        expect(state, clock_word(cpu, 0x3300u, false),
               "I2S half-frame transfers remaining logical word");
        expect(state,
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2200u &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE + 2u) == 0x3300u &&
                   !cpu->io.dci.started,
               "I2S half-frame completes at programmed boundary");
        expect(state, clock_word(cpu, 0x4400u, false),
               "I2S waits for next frame edge after boundary");
        expect(state, cpu->io.dci.receive_buffered == 0u,
               "I2S idle edge gap transfers no word");
    }
}

static void ac_link_cases(DciConformance* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    uint8_t slot;
    dspic33_reset(cpu, 0u);
    configure_external(cpu, DCI_MODE_AC_LINK_16, 4u, 1u, 2u, 0x3001u, 0x3001u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 2u, 0xccccu);
    for (slot = 0u; slot < 13u; slot++) {
        expect(state, clock_word(cpu, (uint16_t)(0x2000u + slot), false),
               "clock 16-bit AC-Link slot");
        expect(state,
               dspic33_dci_transmit(cpu, &transfer) && transfer.slot == slot &&
                   transfer.driven,
               "16-bit AC-Link reports fixed slot position");
    }
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2000u &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE + 2u) == 0x200cu,
           "16-bit AC-Link buffers tag and slot twelve");
    expect(state, cpu->io.dci.slot == 0u, "16-bit AC-Link wraps after thirteen slots");
    expect(state, (cpu->io.dci.receive_buffered | cpu->io.dci.transmit_buffered) == 0u,
           "16-bit AC-Link ignores slot thirteen enables");

    dspic33_reset(cpu, 0u);
    configure_external(cpu, DCI_MODE_AC_LINK_20, 4u, 1u, 2u, 0x8001u, 0x8001u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1234u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 2u, 0xabcdu);
    for (slot = 0u; slot < 16u; slot++) {
        expect(state, clock_word(cpu, (uint16_t)(0x4000u + slot), false),
               "clock 20-bit AC-Link packed slot");
        expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.slot == slot,
               "20-bit AC-Link reports packed slot position");
    }
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x4000u &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE + 2u) == 0x400fu,
           "20-bit AC-Link buffers first and final packed slots");
    expect(state, cpu->io.dci.slot == 0u,
           "20-bit AC-Link wraps after sixteen packed slots");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 2u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 3u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 3u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 2u, 0x2222u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_MODE_AC_LINK_16);
    expect(state, dspic33_device_advance(cpu, 75u) && cpu->io.dci.output.count == 0u,
           "16-bit AC-Link waits through tag-slot boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.slot == 0u,
           "16-bit AC-Link completes sixteen-clock tag slot");
    expect(state, dspic33_device_advance(cpu, 79u) && cpu->io.dci.output.count == 0u,
           "16-bit AC-Link waits through data-slot boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.slot == 1u,
           "16-bit AC-Link completes twenty-clock data slot");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 2u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 3u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_MODE_AC_LINK_20);
    expect(state, dspic33_device_advance(cpu, 75u) && cpu->io.dci.output.count == 0u,
           "20-bit AC-Link waits through first packed-slot boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.slot == 0u,
           "20-bit AC-Link completes first sixteen-clock packed slot");
    expect(state, dspic33_device_advance(cpu, 63u) && cpu->io.dci.output.count == 0u,
           "20-bit AC-Link waits through second packed-slot boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.slot == 1u,
           "20-bit AC-Link keeps sixteen-clock packed-slot timing");
}

static void protocol_integration_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 0u; mode < 4u; mode++) {
        uint16_t expected = mode >= DCI_MODE_AC_LINK_16 ? 0x5aa5u : 0x5a00u;
        dspic33_reset(cpu, 0u);
        configure_external(cpu, (uint16_t)(mode | DCI_EXTERNAL_FRAME), 8u, 2u, 1u, 0u,
                           1u);
        expect(state, clock_word(cpu, 0x1100u, false),
               "protocol slave clocks without frame indication");
        expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "protocol slave remains idle before frame indication");
        expect(state, clock_word(cpu, 0x2200u, true),
               "protocol slave accepts frame indication");
        expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2200u,
               "protocol slave captures synchronized word");

        dspic33_reset(cpu, 0u);
        configure_dma(cpu, 0u, 0x0001u, (uint32_t)(0x4400u + mode * 2u),
                      DCI_RECEIVE_BASE, 0u, DCI_DMA_REQUEST);
        configure_external(cpu, mode, 8u, 1u, 1u, 0u, 1u);
        expect(state, clock_word(cpu, 0x5aa5u, false),
               "protocol raises receive DMA request");
        expect(state, dspic33_device_advance(cpu, 2u),
               "protocol completes receive DMA transfer");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(0x4400u + mode * 2u)) == expected,
               "protocol DMA observes effective word geometry");
    }

    for (mode = DCI_MODE_AC_LINK_16; mode <= DCI_MODE_AC_LINK_20; mode++) {
        uint8_t width;
        for (width = 1u; width < 4u; width++) {
            dspic33_reset(cpu, 0u);
            configure_external(cpu, mode, width, 1u, 1u, 0u, 1u);
            expect(state,
                   clock_word(cpu, 0xa55au, false) &&
                       dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa55au,
                   "AC-Link ignores programmed word-size field");
        }
    }
}

static void pps_serial_input_cases(DciConformance* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~0x0003u));
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | 0x0003u));
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, 0x0003u);
    configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 1u, 1u);
    expect(state, drive_serial_word(cpu, 0xa500u, 8u, true),
           "clock default DCI VSS selection");
    expect(state,
           !cpu->io.dci.initialized && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u &&
               !dspic33_dci_transmit(cpu, &transfer),
           "default DCI VSS selection produces no serial transfer");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_INPUTS, 0x0101u);
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state, drive_serial_word(cpu, 0xf000u, 4u, true),
           "clock unavailable virtual DCI selections");
    expect(state, !cpu->io.dci.initialized,
           "silicon erratum suppresses virtual DCI pin remapping");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x5a00u);
    expect(state, drive_serial_word(cpu, 0xa500u, 8u, true),
           "clock PPS DCI on rising edges");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa500u &&
               dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x5a00u &&
               transfer.slot == 0u && interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "PPS DCI shifts rising-edge data MSb first");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_dma(cpu, 0u, 0x0001u, 0x4400u, DCI_RECEIVE_BASE, 0u, DCI_DMA_REQUEST);
    configure_external(cpu, DCI_SAMPLE_RISING, 16u, 1u, 1u, 0u, 1u);
    expect(state, drive_serial_word(cpu, 0x6b4bu, 16u, true),
           "clock PPS DCI receive DMA block");
    expect(state,
           dspic33_device_advance(cpu, 2u) &&
               dspic33_read_word(cpu, 0x4400u) == 0x6b4bu,
           "PPS DCI receive block requests DMA");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_LOOPBACK, 8u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x5a00u);
    expect(state, drive_serial_word(cpu, 0xa500u, 8u, true),
           "clock PPS DCI loopback word");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5a00u &&
               dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x5a00u,
           "PPS DCI loopback replaces sampled pin data");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 1u, 0u);
    expect(state,
           drive_serial_bit(cpu, false, true) && interrupt_set(cpu, DCI_ERROR_IRQ),
           "PPS DCI transmit underflow raises error interrupt");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, 0u, 8u, 1u, 1u, 0u, 1u);
    expect(state, drive_serial_word(cpu, 0x3c00u, 8u, false),
           "clock PPS DCI on falling edges");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x3c00u,
           "PPS DCI honors falling-edge sample selection");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           drive_serial_bit(cpu, true, true) && drive_serial_bit(cpu, false, true) &&
               cpu->io.dci.serial_bits == 2u,
           "PPS DCI captures partial word before remap");
    dspic33_write_word(cpu, DCI_PPS_INPUTS,
                       (uint16_t)((PPS_FRAME_PIN << 8u) | PPS_DATA_PIN));
    expect(state, drive_serial_bit(cpu, true, true) && cpu->io.dci.serial_bits == 2u,
           "old CSCK pin stops driving after remap");
    expect(state,
           drive_serial_pin_bit(cpu, false, true, GPIO_FRAME_MASK) &&
               drive_serial_pin_bit(cpu, true, true, GPIO_FRAME_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x9000u,
           "new CSCK pin completes retained partial word");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_INPUTS,
                       (uint16_t)((PPS_ANALOG_CLOCK_PIN << 8u) | PPS_DATA_PIN));
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_write_word(
        cpu, GPIO_ANALOG_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) | GPIO_ANALOG_CLOCK_MASK));
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state, drive_serial_pin_word(cpu, 0xf000u, 4u, true, GPIO_ANALOG_CLOCK_MASK),
           "drive analog-selected DCI clock pin");
    expect(state, !cpu->io.dci.initialized,
           "analog-selected DCI clock pin is suppressed");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_ANALOG_CLOCK_MASK);
    dspic33_write_word(
        cpu, GPIO_ANALOG_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~GPIO_ANALOG_CLOCK_MASK));
    expect(state,
           drive_serial_pin_word(cpu, 0x9000u, 4u, true, GPIO_ANALOG_CLOCK_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x9000u,
           "digital DCI clock pin resumes serial transfer");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) & ~GPIO_CLOCK_MASK));
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state, drive_serial_word(cpu, 0xf000u, 4u, true),
           "drive output-configured DCI clock pin");
    expect(state, !cpu->io.dci.initialized,
           "output-configured DCI clock pin is suppressed");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_CLOCK_MASK));
    expect(state,
           drive_serial_word(cpu, 0x6000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6000u,
           "input-configured DCI clock pin resumes serial transfer");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_MODE_AC_LINK_16 | DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u,
                       1u);
    expect(state, drive_serial_word(cpu, 0xa55au, 20u, true),
           "clock PPS 16-bit AC-Link tag slot");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa55au &&
               cpu->io.dci.slot == 1u,
           "PPS 16-bit AC-Link captures sixteen data and four padding clocks");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_MODE_AC_LINK_20 | DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u,
                       1u);
    expect(state, drive_serial_word(cpu, 0x5aa5u, 16u, true),
           "clock PPS 20-bit AC-Link packed slot");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5aa5u &&
               cpu->io.dci.slot == 1u,
           "PPS 20-bit AC-Link captures packed sixteen-clock slot");
}

static void pps_qualification_cases(DciConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_INPUTS,
                       (uint16_t)((PPS_CLOCK_PIN << 8u) | PPS_ANALOG_CLOCK_PIN));
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_write_word(
        cpu, GPIO_ANALOG_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) | GPIO_ANALOG_CLOCK_MASK));
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           drive_mapped_serial_word(cpu, 0xf000u, 4u, true, GPIO_ANALOG_CLOCK_MASK,
                                    GPIO_CLOCK_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "analog CSDI selection samples low");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(
        cpu, GPIO_ANALOG_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~GPIO_ANALOG_CLOCK_MASK));
    expect(state,
           drive_mapped_serial_word(cpu, 0xf000u, 4u, true, GPIO_ANALOG_CLOCK_MASK,
                                    GPIO_CLOCK_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "digital CSDI selection resumes sampling");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) & ~GPIO_DATA_MASK));
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "output-configured CSDI selection samples low");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_DATA_MASK));
    expect(state,
           drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "input-configured CSDI selection resumes sampling");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_FRAME, PPS_ANALOG_CLOCK_PIN);
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_write_word(
        cpu, GPIO_ANALOG_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) | GPIO_ANALOG_CLOCK_MASK));
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY,
                       4u, 1u, 1u, 0u, 1u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_ANALOG_CLOCK_MASK,
                       GPIO_ANALOG_CLOCK_MASK);
    expect(state, drive_serial_word(cpu, 0xf000u, 4u, true) && !cpu->io.dci.initialized,
           "analog COFS selection cannot start a frame");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(
        cpu, GPIO_ANALOG_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~GPIO_ANALOG_CLOCK_MASK));
    expect(state,
           drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "digital COFS selection starts a frame");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) & ~GPIO_FRAME_MASK));
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY,
                       4u, 1u, 1u, 0u, 1u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state, drive_serial_word(cpu, 0xf000u, 4u, true) && !cpu->io.dci.initialized,
           "output-configured COFS selection cannot start a frame");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(
        cpu, GPIO_TRIS_D,
        (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_FRAME_MASK));
    expect(state,
           drive_serial_word(cpu, 0xf000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xf000u,
           "input-configured COFS selection starts a frame");
}

static void pps_frame_cases(DciConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME, 4u, 1u, 1u, 0u, 1u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state, drive_serial_bit(cpu, true, true),
           "sample default-justified DCI frame pulse");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK);
    expect(state,
           drive_serial_word(cpu, 0xa000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "default DJST begins data one serial clock after frame");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY,
                       4u, 1u, 1u, 0u, 1u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK | GPIO_DATA_MASK,
                       GPIO_FRAME_MASK | GPIO_DATA_MASK);
    expect(state, drive_serial_bit(cpu, true, true),
           "sample same-cycle-justified DCI frame pulse");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK);
    expect(state,
           drive_serial_bit(cpu, false, true) && drive_serial_bit(cpu, true, true) &&
               drive_serial_bit(cpu, false, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "DJST begins data during frame serial clock");
    expect(state, drive_serial_word(cpu, 0xf000u, 4u, true),
           "clock falling multi-channel frame transition");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "multi-channel mode ignores falling frame transition");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(
        cpu, DCI_MODE_I2S | DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY,
        4u, 1u, 1u, 0u, 1u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK | GPIO_DATA_MASK,
                       GPIO_FRAME_MASK | GPIO_DATA_MASK);
    expect(state,
           drive_serial_word(cpu, 0x8000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x8000u,
           "I2S rising frame edge starts serial word");
    dspic33_read_word(cpu, DCI_RECEIVE_BASE);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK | GPIO_DATA_MASK);
    expect(state,
           drive_serial_word(cpu, 0x5000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "I2S falling frame edge starts serial word");
}

static void pps_serial_matrix_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 0u; mode < 2u; mode++) {
        uint8_t width;
        for (width = 4u; width <= 16u; width++) {
            uint8_t rising;
            for (rising = 0u; rising < 2u; rising++) {
                uint8_t immediate;
                for (immediate = 0u; immediate < 2u; immediate++) {
                    uint16_t value = (uint16_t)(0xa55au & serial_word_mask(width));
                    uint16_t control = DCI_EXTERNAL_FRAME;
                    if (mode != 0u) {
                        control |= DCI_MODE_I2S;
                    }
                    if (rising != 0u) {
                        control |= DCI_SAMPLE_RISING;
                    }
                    if (immediate != 0u) {
                        control |= DCI_DATA_JUSTIFY;
                    }
                    dspic33_reset(cpu, 0u);
                    configure_serial_pins(cpu);
                    configure_external(cpu, control, width, 1u, 1u, 0u, 1u);
                    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK,
                                       GPIO_FRAME_MASK);
                    if (immediate == 0u) {
                        expect(state, drive_serial_bit(cpu, false, rising != 0u),
                               "clock default-justified PPS matrix frame");
                        if (mode == 0u) {
                            dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK);
                        }
                    }
                    expect(state,
                           drive_serial_word(cpu, value, width, rising != 0u) &&
                               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == value &&
                               !cpu->io.dci.started,
                           "PPS serial matrix captures framed word");
                }
            }
        }
    }
}

static void pps_selection_cases(DciConformance* state, Dspic33* cpu) {
    uint8_t selection;
    bool high;
    for (selection = 0u; selection < 16u; selection++) {
        dspic33_reset(cpu, 0u);
        configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_INPUTS,
                           (uint16_t)((selection << 8u) | PPS_DATA_PIN));
        configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
        expect(state,
               drive_serial_word(cpu, 0xf000u, 4u, true) && !cpu->io.dci.initialized,
               "DCI virtual and reserved clock selections remain inaccessible");

        dspic33_reset(cpu, 0u);
        configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_INPUTS,
                           (uint16_t)((PPS_CLOCK_PIN << 8u) | selection));
        configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
        expect(state,
               drive_serial_word(cpu, 0xf000u, 4u, true) &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "DCI virtual and reserved data selections resolve low");

        dspic33_reset(cpu, 0u);
        configure_serial_pins(cpu);
        dspic33_write_word(cpu, DCI_PPS_FRAME, selection);
        configure_external(cpu,
                           DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY,
                           4u, 1u, 1u, 0u, 1u);
        dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
        expect(state,
               drive_serial_word(cpu, 0xf000u, 4u, true) && !cpu->io.dci.initialized,
               "DCI virtual and reserved frame selections remain inaccessible");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_DATA_JUSTIFY);
    dspic33_device_advance(cpu, 12u);
    for (selection = 0u; selection < 64u; selection++) {
        dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT,
                           (uint16_t)((uint16_t)selection << 8u));
        expect(state,
               dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) ==
                   (selection >= 11u && selection <= 13u),
               "DCI RPOR function admission matches target table");
    }
}

static void pps_output_cases(DciConformance* state, Dspic33* cpu) {
    bool high;
    expect(state,
           !dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, NULL) &&
               !dspic33_dci_pin(cpu, 0u, &high),
           "DCI pin API rejects invalid queries");
    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x0b00u);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME, 4u, 1u, 1u, 1u, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state, drive_serial_bit(cpu, false, true),
           "start PPS DCI data output frame");
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && high,
           "CSDO RPOR mapping drives transmit MSb");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && high,
           "CSDO holds transmit MSb before first data sample");
    expect(state, drive_serial_bit(cpu, true, true), "sample first PPS DCI output bit");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && !high,
           "CSDO advances on opposite serial clock edge");
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x000bu);
    expect(state,
           !dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) &&
               dspic33_dci_pin(cpu, PPS_FRAME_PIN, &high) && !high,
           "CSDO output follows live RPOR remapping");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x0b00u);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_TRISTATE, 4u, 1u, 1u, 0u, 0u);
    expect(state, drive_serial_bit(cpu, false, true), "start tri-stated PPS DCI slot");
    expect(state, !dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high),
           "CSDOM releases disabled CSDO slot");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_DATA_JUSTIFY);
    expect(state,
           dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) && high &&
               dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "master DCI drives CSCK during startup with inactive COFS");
    expect(state, dspic33_device_advance(cpu, 12u),
           "advance master DCI through startup clocks");
    expect(state, dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) && high,
           "master DCI begins data word with asserted CSCK");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
           "master multi-channel DCI asserts COFS for first clock");
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance master DCI beyond first serial clock");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "master multi-channel DCI negates COFS after first clock");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_DATA_OUTPUT, 0x0b00u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL1,
                       DCI_ENABLE | DCI_DATA_JUSTIFY | DCI_SAMPLE_RISING);
    expect(state, dspic33_device_advance(cpu, 12u),
           "advance rising-sample master through startup clocks");
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && high,
           "rising-sample master presents first CSDO bit before rising edge");
    expect(state, dspic33_device_advance(cpu, 2u),
           "advance rising-sample master to falling edge");
    expect(state, dspic33_dci_pin(cpu, PPS_DATA_OUTPUT_PIN, &high) && !high,
           "rising-sample master advances CSDO on falling edge");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 8u),
           "advance default-justified master to final startup clock");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
           "default DJST asserts COFS one clock before data");
    expect(state, dspic33_device_advance(cpu, 4u),
           "advance default-justified master to data boundary");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "default DJST negates COFS when first data bit begins");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_MODE_I2S | DCI_DATA_JUSTIFY);
    expect(state, dspic33_device_advance(cpu, 12u),
           "advance I2S master through startup clocks");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && high,
           "I2S master drives right-channel COFS high first");
    expect(state, dspic33_device_advance(cpu, 16u),
           "advance I2S master through right-channel word");
    expect(state, dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high) && !high,
           "I2S master toggles COFS for left-channel word");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME, 4u, 1u, 1u, 0u, 0u);
    expect(state,
           !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high) &&
               !dspic33_dci_pin(cpu, PPS_FRAME_OUTPUT_PIN, &high),
           "slave DCI releases externally directed CSCK and COFS outputs");
}

static void pps_disable_timing_cases(DciConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 0u);
    expect(state, drive_serial_bit(cpu, true, true) && cpu->io.dci.serial_bits == 1u,
           "advance PPS DCI to exact three-clock disable boundary");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_SAMPLE_RISING | DCI_EXTERNAL_CLOCK);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u,
           "external DCI exact three-clock clear selects current frame");
    expect(state,
           drive_serial_bit(cpu, false, true) && drive_serial_bit(cpu, true, true) &&
               drive_serial_bit(cpu, false, true) && !cpu->io.dci.started &&
               !cpu->io.dci.initialized && !cpu->io.dci.disable_pending,
           "external DCI exact boundary stops at current frame end");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 0u);
    expect(state,
           drive_serial_bit(cpu, true, true) && drive_serial_bit(cpu, false, true) &&
               cpu->io.dci.serial_bits == 2u,
           "advance PPS DCI inside final three clocks");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_SAMPLE_RISING | DCI_EXTERNAL_CLOCK);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 2u,
           "external DCI late clear selects following frame");
    expect(state,
           drive_serial_bit(cpu, true, true) && drive_serial_bit(cpu, false, true) &&
               cpu->io.dci.started && cpu->io.dci.disable_frames == 1u,
           "external DCI late clear completes current frame");
    expect(state,
           drive_serial_word(cpu, 0xa000u, 4u, true) && !cpu->io.dci.started &&
               !cpu->io.dci.initialized && !cpu->io.dci.disable_pending,
           "external DCI late clear stops after following frame");
}

static void pps_lifecycle_cases(DciConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool high;
    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 0u, 1u);
    expect(state,
           drive_serial_bit(cpu, true, true) && drive_serial_bit(cpu, false, true) &&
               drive_serial_bit(cpu, true, true) && cpu->io.dci.serial_bits == 3u,
           "advance PPS DCI before active copy");
    expect(state, dspic33_initialize(&copy) && dspic33_copy(&copy, cpu),
           "copy active PPS DCI shift state");
    expect(
        state,
        drive_serial_bit(cpu, false, true) && drive_serial_bit(cpu, false, true) &&
            drive_serial_bit(cpu, true, true) && drive_serial_bit(cpu, false, true) &&
            drive_serial_bit(cpu, true, true) && drive_serial_bit(&copy, false, true) &&
            drive_serial_bit(&copy, false, true) &&
            drive_serial_bit(&copy, true, true) &&
            drive_serial_bit(&copy, false, true) && drive_serial_bit(&copy, true, true),
        "complete original and copied PPS DCI shifts");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa500u &&
               dspic33_read_word(&copy, DCI_RECEIVE_BASE) == 0xa500u,
           "copied PPS DCI shift completes independently");
    dspic33_destroy(&copy);

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           drive_serial_bit(cpu, true, true) && drive_serial_bit(cpu, false, true) &&
               cpu->io.dci.serial_bits == 2u,
           "advance PPS DCI before PMD disable");
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled,
           "disable PPS DCI through delayed PMD transition");
    expect(state,
           drive_serial_word(cpu, 0xf000u, 4u, true) && cpu->io.dci.serial_bits == 2u,
           "PPS DCI misses serial edges while PMD-disabled");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled,
           "enable PPS DCI through delayed PMD transition");
    expect(state,
           drive_serial_bit(cpu, false, true) && drive_serial_bit(cpu, true, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x9000u,
           "PPS DCI resumes retained partial word after PMD");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           drive_serial_word(cpu, 0x6000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6000u,
           "externally clocked PPS DCI continues in Sleep");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_STOP_IDLE, 4u, 1u, 1u, 0u, 1u);
    expect(state, drive_serial_bit(cpu, true, true),
           "advance PPS DCI before Idle stop");
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           drive_serial_word(cpu, 0xf000u, 4u, true) && cpu->io.dci.serial_bits == 1u,
           "DCISIDL makes PPS DCI miss Idle serial edges");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state,
           drive_serial_bit(cpu, false, true) && drive_serial_bit(cpu, true, true) &&
               drive_serial_bit(cpu, false, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "PPS DCI resumes retained partial word after Idle");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 0u, 1u);
    expect(state,
           drive_serial_bit(cpu, true, true) && drive_serial_bit(cpu, false, true) &&
               cpu->io.dci.serial_bits == 2u,
           "advance PPS DCI before warm reset");
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.dci.serial_bits == 0u &&
               dspic33_read_word(cpu, DCI_CONTROL1) == 0u &&
               dspic33_gpio_pin(cpu, GPIO_PORT_D, 1u, &high) && high,
           "warm reset clears PPS shift state and preserves physical levels");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_DATA_JUSTIFY);
    expect(state,
           dspic33_device_advance(cpu, 5u) &&
               dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high),
           "advance master PPS output before PMD");
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled &&
               !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high),
           "PMD releases DCI PPS outputs");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled &&
               dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &high),
           "PMD clear restores retained DCI PPS output phase");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY,
                       4u, 1u, 1u, 0u, 1u);
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled,
           "disable framed PPS DCI through PMD");
    expect(state,
           dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK) &&
               !cpu->io.dci.pps_frame_pending,
           "PMD-disabled DCI misses COFS edge");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               drive_serial_word(cpu, 0xf000u, 4u, true) && !cpu->io.dci.initialized,
           "PMD clear cannot replay missed COFS edge");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK | GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state,
           drive_serial_word(cpu, 0xa000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "post-PMD COFS edge starts a new frame");

    dspic33_reset(cpu, 0u);
    configure_serial_pins(cpu);
    configure_external(
        cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY | DCI_STOP_IDLE,
        4u, 1u, 1u, 0u, 1u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK) &&
               !cpu->io.dci.pps_frame_pending,
           "DCISIDL makes DCI miss Idle COFS edge");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state, drive_serial_word(cpu, 0xf000u, 4u, true) && !cpu->io.dci.initialized,
           "Idle resume cannot replay missed COFS edge");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK | GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state,
           drive_serial_word(cpu, 0x5000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "post-Idle COFS edge starts a new frame");
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

static void disable_timing_cases(DciConformance* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 12u) && cpu->io.dci.initialized,
           "initialize one-slot DCI frame");
    expect(state, dspic33_device_advance(cpu, 32u),
           "advance within active DCI slot zero");
    dspic33_dci_input(cpu, 0x2222u);
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state,
           cpu->io.dci.started && cpu->io.dci.disable_pending &&
               cpu->io.dci.disable_frames == 1u && cpu->events.count == 1u,
           "slot-zero disable retains current one-slot frame");
    expect(state, dspic33_device_advance(cpu, 31u) && cpu->io.dci.started,
           "slot-zero disable waits through active word");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.started &&
               !cpu->io.dci.disable_pending &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2222u &&
               dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x1111u,
           "slot-zero disable completes current frame before stopping");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 64u),
           "advance to three-clock DCI disable boundary");
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u,
           "three-clock DCI disable selects current frame");
    expect(state,
           dspic33_device_advance(cpu, 12u) && !cpu->io.dci.started &&
               !cpu->io.dci.disable_pending,
           "three-clock DCI disable stops at current frame end");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x3333u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 65u),
           "advance inside final three DCI clocks");
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 2u,
           "late DCI disable selects following frame");
    expect(state,
           dspic33_device_advance(cpu, 11u) && cpu->io.dci.started &&
               cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u &&
               dspic33_dci_transmit(cpu, &transfer) && transfer.slot == 0u,
           "late DCI disable completes current frame and continues");
    expect(state, dspic33_device_advance(cpu, 63u) && cpu->io.dci.started,
           "late DCI disable waits through following frame");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.started &&
               !cpu->io.dci.disable_pending && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.slot == 0u,
           "late DCI disable stops after following frame");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 2u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 3u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 12u), "initialize two-slot DCI frame");
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u,
           "early slot-zero disable retains multi-slot frame");
    expect(state,
           dspic33_device_advance(cpu, 16u) && cpu->io.dci.started &&
               cpu->io.dci.slot == 1u,
           "multi-slot disable continues after slot zero");
    expect(state,
           dspic33_device_advance(cpu, 16u) && !cpu->io.dci.started &&
               !cpu->io.dci.disable_pending,
           "multi-slot disable stops at frame end");
}

static void internal_clock_lifecycle_cases(DciConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 5u), "advance DCI startup before PMD");
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled &&
               cpu->events.count == 1u && cpu->events.items[0].paused &&
               cpu->events.items[0].paused_remaining == 6u,
           "PMD retains remaining DCI startup phase");
    expect(state, dspic33_device_advance(cpu, 40u) && !cpu->io.dci.initialized,
           "PMD holds DCI startup indefinitely");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled &&
               !cpu->events.items[0].paused,
           "PMD enable resumes retained DCI startup phase");
    expect(state, dspic33_device_advance(cpu, 5u) && !cpu->io.dci.initialized,
           "resumed DCI startup waits to retained boundary");
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.dci.initialized,
           "resumed DCI startup completes at retained boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 17u), "advance DCI into internal word");
    dspic33_dci_input(cpu, 0xa000u);
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               cpu->events.items[0].paused_remaining == 11u,
           "BCG zero retains remaining DCI word phase");
    expect(state,
           dspic33_device_advance(cpu, 50u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "BCG zero holds DCI word indefinitely");
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           dspic33_device_advance(cpu, 10u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "restored BCG waits to retained word boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "restored BCG completes at retained word boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 17u), "advance DCI before stepped Sleep");
    dspic33_dci_input(cpu, 0xb000u);
    expect(state, dspic33_load_program_word(cpu, 0x0200u, OPCODE_SLEEP),
           "load DCI Sleep instruction");
    cpu->pc = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_SLEEPING && cpu->events.items[0].paused &&
               cpu->events.items[0].paused_remaining == 11u,
           "PWRSAV Sleep retains remaining DCI word phase");
    expect(state,
           dspic33_device_advance(cpu, 50u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "Sleep holds internal DCI word indefinitely");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 10u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "DCI Sleep wake waits to retained word boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xb000u,
           "DCI Sleep wake completes at retained word boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_STOP_IDLE);
    expect(state, dspic33_device_advance(cpu, 17u),
           "advance DCI before stepped stopped Idle");
    dspic33_dci_input(cpu, 0xc000u);
    expect(state, dspic33_load_program_word(cpu, 0x0200u, OPCODE_IDLE),
           "load stopped DCI Idle instruction");
    cpu->pc = 0x0200u;
    expect(state,
           dspic33_step(cpu) == DSPIC33_IDLING && cpu->events.items[0].paused &&
               cpu->events.items[0].paused_remaining == 11u,
           "DCISIDL retains remaining DCI word phase");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 11u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xc000u,
           "DCISIDL wake completes retained DCI word");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 17u),
           "advance DCI before continuing Idle");
    dspic33_dci_input(cpu, 0xd000u);
    expect(state, dspic33_load_program_word(cpu, 0x0200u, OPCODE_IDLE),
           "load continuing DCI Idle instruction");
    cpu->pc = 0x0200u;
    expect(state, dspic33_step(cpu) == DSPIC33_IDLING && !cpu->events.items[0].paused,
           "DCISIDL clear keeps internal DCI clock running in Idle");
    expect(state,
           dspic33_device_advance(cpu, 10u) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xd000u,
           "continuing Idle reaches original DCI word boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 17u),
           "advance DCI before paused disable");
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused &&
               cpu->io.dci.disable_pending && cpu->io.dci.started,
           "DCI disable retains paused active frame");
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           dspic33_device_advance(cpu, 11u) && !cpu->io.dci.started &&
               !cpu->io.dci.disable_pending && cpu->events.count == 0u,
           "restored DCI clock completes pending disabled frame");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 5u),
           "advance DCI before resume overflow");
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    cpu->device_cycles = UINT64_MAX - 5u;
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.items[0].paused,
           "DCI resume overflow leaves retained event paused");
    cpu->stop_reason = DSPIC33_RUNNING;
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
        protocol_geometry_cases(&state, &cpu);
        protocol_frame_cases(&state, &cpu);
        ac_link_cases(&state, &cpu);
        protocol_integration_cases(&state, &cpu);
        pps_serial_input_cases(&state, &cpu);
        pps_frame_cases(&state, &cpu);
        pps_serial_matrix_cases(&state, &cpu);
        pps_selection_cases(&state, &cpu);
        pps_qualification_cases(&state, &cpu);
        pps_output_cases(&state, &cpu);
        pps_disable_timing_cases(&state, &cpu);
        pps_lifecycle_cases(&state, &cpu);
        mode_and_status_cases(&state, &cpu);
        interrupt_dma_cases(&state, &cpu);
        generation_and_frame_cases(&state, &cpu);
        disable_timing_cases(&state, &cpu);
        internal_clock_lifecycle_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        expect(&state, state.cases == 14076u, "DCI assertion accounting");
        dspic33_destroy(&cpu);
    }
    printf("[dci-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
