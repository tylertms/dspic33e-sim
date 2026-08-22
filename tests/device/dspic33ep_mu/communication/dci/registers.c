#include "device/dspic33ep_mu/communication/dci/internal.h"

uint16_t dspic33_dci_test_configuration(uint8_t width, uint8_t slots, uint8_t buffers) {
    return (uint16_t)(((uint16_t)(buffers - 1u) << 10u) | ((uint16_t)(slots - 1u) << 5u) |
                      (width - 1u));
}

bool dspic33_dci_test_clock_word(Dspic33* cpu, uint16_t value, bool frame_sync) {
    return dspic33_dci_clock(cpu, value, frame_sync, 0u) && dspic33_device_advance(cpu, 0u);
}

void dspic33_dci_test_configure_serial_pins(Dspic33* cpu) {
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~0x0007u));
    dspic33_write_word(cpu, GPIO_TRIS_D, (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | 0x0007u));
    dspic33_write_word(cpu, DCI_PPS_INPUTS, (uint16_t)((PPS_CLOCK_PIN << 8u) | PPS_DATA_PIN));
    dspic33_write_word(cpu, DCI_PPS_FRAME, PPS_FRAME_PIN);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, 0x0007u);
}

bool dspic33_dci_test_drive_serial_edge(Dspic33* cpu, bool high, bool rising, uint16_t clock_mask) {
    uint16_t data = high ? GPIO_DATA_MASK : 0u;
    uint16_t initial_clock = rising ? 0u : clock_mask;
    uint16_t sample_clock = rising ? clock_mask : 0u;
    return dspic33_gpio_drive(cpu, GPIO_PORT_D, data, GPIO_DATA_MASK) &&
           dspic33_gpio_drive(cpu, GPIO_PORT_D, initial_clock, clock_mask) &&
           dspic33_gpio_drive(cpu, GPIO_PORT_D, sample_clock, clock_mask);
}

bool dspic33_dci_test_activate_serial_clock(Dspic33* cpu, bool rising, uint16_t clock_mask) {
    uint8_t edges = cpu->io.dci.serial_startup_bits;
    while (edges-- != 0u) {
        if (!dspic33_dci_test_drive_serial_edge(cpu, false, rising, clock_mask)) {
            return false;
        }
    }
    return true;
}

static bool prepare_serial_data(Dspic33* cpu, bool rising, uint16_t clock_mask) {
    uint16_t control;
    if (!dspic33_dci_test_activate_serial_clock(cpu, rising, clock_mask)) {
        return false;
    }
    control = dspic33_read_word(cpu, DCI_CONTROL1);
    if (cpu->io.dci.serial_delay ||
        (cpu->io.dci.pps_frame_pending && !cpu->io.dci.started &&
         (control & 0x0003u) < DCI_MODE_AC_LINK_16 && (control & DCI_DATA_JUSTIFY) == 0u)) {
        return dspic33_dci_test_drive_serial_edge(cpu, false, rising, clock_mask);
    }
    return true;
}

static bool drive_serial_pin_bit(Dspic33* cpu, bool high, bool rising, uint16_t clock_mask) {
    return prepare_serial_data(cpu, rising, clock_mask) &&
           dspic33_dci_test_drive_serial_edge(cpu, high, rising, clock_mask);
}

bool dspic33_dci_test_drive_mapped_serial_word(Dspic33* cpu, uint16_t value, uint8_t width,
                                               bool rising, uint16_t data_mask,
                                               uint16_t clock_mask) {
    uint8_t bit;
    if (!prepare_serial_data(cpu, rising, clock_mask)) {
        return false;
    }
    for (bit = 0u; bit < width; bit++) {
        uint16_t data = bit < 16u && (value & (uint16_t)(0x8000u >> bit)) != 0u ? data_mask : 0u;
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

static bool drive_serial_pin_word(Dspic33* cpu, uint16_t value, uint8_t width, bool rising,
                                  uint16_t clock_mask) {
    uint8_t bit;
    for (bit = 0u; bit < width; bit++) {
        bool high = bit < 16u && (value & (uint16_t)(0x8000u >> bit)) != 0u;
        if (!drive_serial_pin_bit(cpu, high, rising, clock_mask)) {
            return false;
        }
    }
    return true;
}

bool dspic33_dci_test_drive_serial_bit(Dspic33* cpu, bool high, bool rising) {
    return drive_serial_pin_bit(cpu, high, rising, GPIO_CLOCK_MASK);
}

bool dspic33_dci_test_drive_serial_word(Dspic33* cpu, uint16_t value, uint8_t width, bool rising) {
    return drive_serial_pin_word(cpu, value, width, rising, GPIO_CLOCK_MASK);
}

uint16_t dspic33_dci_test_serial_word_mask(uint8_t width) {
    return width == 16u ? UINT16_MAX : (uint16_t)(UINT16_MAX << (16u - width));
}

void dspic33_dci_test_configure_external(Dspic33* cpu, uint16_t control, uint8_t width,
                                         uint8_t slots, uint8_t buffers, uint16_t transmit,
                                         uint16_t receive) {
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(width, slots, buffers));
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, transmit);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, receive);
    dspic33_write_word(cpu, DCI_CONTROL1, (uint16_t)(DCI_ENABLE | DCI_EXTERNAL_CLOCK | control));
}

void dspic33_dci_test_configure_internal(Dspic33* cpu, uint16_t control, uint8_t width,
                                         uint8_t slots, uint8_t buffers, uint16_t transmit,
                                         uint16_t receive) {
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(width, slots, buffers));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, transmit);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, receive);
    dspic33_write_word(cpu, DCI_CONTROL1, (uint16_t)(DCI_ENABLE | control));
}

bool dspic33_dci_test_drive_internal_pin_slot(Dspic33* cpu, uint16_t value, uint8_t width,
                                              uint64_t start_delay) {
    uint64_t bit_cycles = ((uint64_t)(dspic33_read_word(cpu, DCI_CONTROL3) & 0x0fffu) + 1u) * 2u;
    uint64_t sample_offset =
        (dspic33_read_word(cpu, DCI_CONTROL1) & DCI_SAMPLE_RISING) != 0u ? 0u : bit_cycles / 2u;
    uint8_t bit;
    for (bit = 0u; bit < width; bit++) {
        uint16_t high =
            bit < 16u && (value & (uint16_t)(0x8000u >> bit)) != 0u ? GPIO_DATA_MASK : 0u;
        if (!dspic33_gpio_drive(cpu, GPIO_PORT_D, high, GPIO_DATA_MASK) ||
            !dspic33_device_advance(cpu, bit == 0u ? start_delay + sample_offset : bit_cycles)) {
            return false;
        }
    }
    return dspic33_device_advance(cpu, bit_cycles - sample_offset);
}

static uint16_t interrupt_word(uint8_t irq, uint16_t base) {
    return (uint16_t)(base + (irq / 16u) * 2u);
}

static uint16_t interrupt_mask(uint8_t irq) { return (uint16_t)(1u << (irq % 16u)); }

bool dspic33_dci_test_interrupt_set(Dspic33* cpu, uint8_t irq) {
    return (dspic33_read_word(cpu, interrupt_word(irq, 0x0800u)) & interrupt_mask(irq)) != 0u;
}

void dspic33_dci_test_clear_interrupt(Dspic33* cpu, uint8_t irq) {
    uint16_t address = interrupt_word(irq, 0x0800u);
    dspic33_write_word(cpu, address,
                       (uint16_t)(dspic33_read_word(cpu, address) & ~interrupt_mask(irq)));
}

void dspic33_dci_test_enable_interrupt(Dspic33* cpu, uint8_t irq, uint8_t priority) {
    uint16_t enable = interrupt_word(irq, 0x0820u);
    uint16_t ipc = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(cpu, enable,
                       (uint16_t)(dspic33_read_word(cpu, enable) | interrupt_mask(irq)));
    dspic33_write_word(cpu, ipc,
                       (uint16_t)((dspic33_read_word(cpu, ipc) & ~(uint16_t)(7u << shift)) |
                                  (uint16_t)(priority << shift)));
    cpu->program[(0x0014u + irq * 2u) / 2u] = DCI_VECTOR;
}

void dspic33_dci_test_configure_dma(Dspic33* cpu, uint8_t channel, uint16_t control,
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

void dspic33_dci_test_access_cases(TestState* state, Dspic33* cpu) {
    uint8_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < 4u; index++) {
        expect(state, dspic33_read_word(cpu, (uint16_t)(DCI_RECEIVE_BASE + index * 2u)) == 0u,
               "RXBUF deterministic POR value");
        expect(state, dspic33_read_word(cpu, (uint16_t)(DCI_TRANSMIT_BASE + index * 2u)) == 0u,
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
    expect(state, dspic33_read_word(cpu, DCI_CONTROL1) == 0xafe3u, "DCICON1 access mask");
    expect(state, dspic33_read_word(cpu, DCI_CONTROL2) == 0x0defu, "DCICON2 access mask");
    expect(state, dspic33_read_word(cpu, DCI_CONTROL3) == 0x0fffu, "DCICON3 access mask");
    expect(state, dspic33_read_word(cpu, DCI_STATUS) == 0u, "DCISTAT is read only");
    expect(state, dspic33_read_word(cpu, DCI_TRANSMIT_SLOTS) == UINT16_MAX, "TSCON access mask");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_SLOTS) == UINT16_MAX, "RSCON access mask");
    for (index = 0u; index < 4u; index++) {
        uint16_t rx = (uint16_t)(DCI_RECEIVE_BASE + index * 2u);
        uint16_t tx = (uint16_t)(DCI_TRANSMIT_BASE + index * 2u);
        dspic33_write_word(cpu, rx, 0x55aau);
        dspic33_write_word(cpu, tx, (uint16_t)(0x1000u + index));
        expect(state, dspic33_read_word(cpu, rx) == 0u, "RXBUF rejects CPU write");
        expect(state, dspic33_read_word(cpu, tx) == 0u, "TXBUF is write only");
    }
}

void dspic33_dci_test_width_and_lane_cases(TestState* state, Dspic33* cpu) {
    uint8_t width;
    for (width = 4u; width <= 16u; width++) {
        Dspic33DciTransfer transfer;
        uint16_t mask = width == 16u ? UINT16_MAX : (uint16_t)(UINT16_MAX << (16u - width));
        uint16_t value = (uint16_t)(0xa55au & mask);
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_external(cpu, 0u, width, 1u, 1u, 1u, 1u);
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, value);
        expect(state, dspic33_dci_test_clock_word(cpu, 0x5aa5u, false), "clock width event");
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
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x7b6au, false), "clock byte-read fixture");
    expect(state, dspic33_read_byte(cpu, DCI_RECEIVE_BASE) == 0x6au,
           "RXBUF low-byte read returns low lane");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_RECEIVE_FULL) == 0u,
           "RXBUF byte read clears RFUL");
}

void dspic33_dci_test_slot_buffer_status_cases(TestState* state, Dspic33* cpu) {
    uint8_t index;
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 4u, 4u, 0x000fu, 0x000fu);
    for (index = 0u; index < 4u; index++) {
        dspic33_write_word(cpu, (uint16_t)(DCI_TRANSMIT_BASE + index * 2u),
                           (uint16_t)(0x1000u + index));
    }
    for (index = 0u; index < 4u; index++) {
        expect(state, dspic33_dci_test_clock_word(cpu, (uint16_t)(0x2000u + index), false),
               "clock multibuffer word");
    }
    for (index = 0u; index < 4u; index++) {
        Dspic33DciTransfer transfer;
        expect(state,
               dspic33_dci_transmit(cpu, &transfer) &&
                   transfer.value == (uint16_t)(0x1000u + index) && transfer.slot == index,
               "shared pointer selects transmit buffer by slot");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(DCI_RECEIVE_BASE + index * 2u)) ==
                   (uint16_t)(0x2000u + index),
               "shared pointer selects receive buffer by slot");
    }
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) & (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW)) == 0u,
           "reading every RXBUF clears receive status");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x1010u, false) &&
               (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_EMPTY) == 0u,
           "receive-only transfer leaves TMPTY clear");
    dspic33_read_word(cpu, DCI_RECEIVE_BASE);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x1111u, false), "clock first unread block");
    expect(state, dspic33_dci_test_clock_word(cpu, 0x2222u, false), "clock overflowing block");
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) & (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW)) ==
               (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW),
           "unread replacement sets RFUL and ROV");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2222u,
           "overflow keeps newest completed receive word");
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) & (DCI_RECEIVE_FULL | DCI_RECEIVE_OVERFLOW)) == 0u,
           "culprit RXBUF read clears RFUL and ROV");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 2u, 4u, 0x0001u, 0x0003u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x3000u, false),
           "mixed-slot startup loads active TXBUF0");
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 2u, 0x2222u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 6u, 0x4444u);
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_EMPTY) != 0u,
           "inactive TXBUF writes do not suppress TMPTY");
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_EMPTY) == 0u,
           "active TXBUF0 write clears TMPTY");
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x3001u, false) &&
               dspic33_dci_test_clock_word(cpu, 0x3002u, false) &&
               dspic33_dci_test_clock_word(cpu, 0x3003u, false),
           "mixed-slot pointer reaches TXBUF2");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_UNDERFLOW) != 0u,
           "mixed-slot pointer detects unwritten active TXBUF2");
    expect(state, (cpu->io.dci.transmit_underflow & 0x0au) == 0u,
           "inactive TXBUF1 and TXBUF3 never underflow");
}

void dspic33_dci_test_admission_and_clock_cases(TestState* state, Dspic33* cpu) {
    uint8_t width;
    for (width = 1u; width < 4u; width++) {
        Dspic33DciTransfer transfer;
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_external(cpu, 0u, width, 1u, 1u, 1u, 1u);
        expect(state, dspic33_dci_test_clock_word(cpu, 0x1234u, true), "clock invalid word width");
        expect(state,
               !dspic33_dci_transmit(cpu, &transfer) &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "invalid word width produces no DCI behavior");
    }

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, DCI_EXTERNAL_FRAME, 16u, 2u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x1111u, false),
           "external frame waits without FS");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "external frame ignores CSCK before FS");
    expect(state, dspic33_dci_test_clock_word(cpu, 0x2222u, true), "external frame starts on FS");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2222u,
           "external frame captures synchronized first slot");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, DCI_EXTERNAL_FRAME, 16u, 2u, 1u, 0u, 0x0003u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x1234u, true), "start two-slot external frame");
    expect(state, (dspic33_read_word(cpu, DCI_STATUS) & 0x0f00u) == 0x0100u,
           "SLOT reports second slot after first frame word");
    expect(state, dspic33_dci_test_clock_word(cpu, 0x5678u, true),
           "clock spurious FS during frame");
    expect(state,
           (dspic33_read_word(cpu, DCI_STATUS) & 0x0f00u) == 0u &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5678u,
           "active frame ignores FS restart and completes slot one");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state, dspic33_device_advance(cpu, 1u), "apply DCI PMD disable");
    expect(state, dspic33_dci_test_clock_word(cpu, 0x3333u, false),
           "clock external DCI while PMD is effective");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state, dspic33_device_advance(cpu, 1u), "apply DCI PMD enable");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "PMD-blocked clock leaves no receive word after re-enable");
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK | DCI_STOP_IDLE);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x4444u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "DCISIDL stops external DCI in Idle");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x5555u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5555u,
           "external DCI continues in Idle when DCISIDL is clear");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x6666u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6666u,
           "external DCI continues in Sleep");
    cpu->power_state = DSPIC33_POWER_ACTIVE;

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0xb000u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "COFSD output ignores external frame-sync input");
    expect(state,
           dspic33_device_advance(cpu, 64u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "BCG zero disables internal DCI clock");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x9090u, false) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "external DCI rejects nonzero BCG");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state,
           dspic33_device_advance(cpu, 11u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "internal clock waits three CSCK startup cycles");
    expect(state, dspic33_device_advance(cpu, 1u), "internal startup completes");
    dspic33_dci_input(cpu, 0xa000u);
    expect(state,
           dspic33_device_advance(cpu, 15u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "internal four-bit word waits two shifts per bit");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "internal four-bit word completes after sixteen cycles");

    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_dci_input(cpu, 0x5000u);
    expect(state,
           dspic33_device_advance(cpu, 32u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "BCG zero freezes a running internal DCI clock");
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           dspic33_device_advance(cpu, 16u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "restored BCG resumes internal DCI clock");

    dspic33_dci_input(cpu, 0x6000u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_device_advance(cpu, 32u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "Sleep stops internal DCI clock");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state,
           dspic33_device_advance(cpu, 16u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6000u,
           "internal DCI clock resumes after Sleep");
}

void dspic33_dci_test_protocol_geometry_cases(TestState* state, Dspic33* cpu) {
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
                    dspic33_dci_test_configure_external(cpu, control, width, frames, 1u, 1u, 1u);
                    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, transmit);
                    expect(state, dspic33_dci_test_clock_word(cpu, receive, false),
                           "clock protocol geometry word");
                    expect(state,
                           dspic33_dci_transmit(cpu, &transfer) && transfer.slot == 0u &&
                               transfer.value == (transmit & expected_mask) && transfer.driven,
                           "protocol selects effective transmit geometry");
                    expect(state,
                           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == (receive & expected_mask),
                           "protocol selects effective receive geometry");
                    expect(state, cpu->io.dci.slot == (expected_frames == 1u ? 0u : 1u),
                           "protocol selects effective frame geometry");
                }
            }
        }
    }
}

void dspic33_dci_test_protocol_frame_cases(TestState* state, Dspic33* cpu) {
    uint8_t justification;
    for (justification = 0u; justification < 2u; justification++) {
        uint16_t control = (uint16_t)(DCI_MODE_I2S | DCI_EXTERNAL_FRAME |
                                      (justification != 0u ? DCI_DATA_JUSTIFY : 0u));
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_external(cpu, control, 8u, 2u, 2u, 0u, 3u);
        expect(state, dspic33_dci_test_clock_word(cpu, 0x1100u, false),
               "I2S slave waits for frame edge");
        expect(state, cpu->io.dci.receive_buffered == 0u && !cpu->io.dci.started,
               "I2S absent frame edge leaves transfer idle");
        expect(state, dspic33_dci_test_clock_word(cpu, 0x2200u, true),
               "I2S frame edge starts half-frame");
        expect(state, cpu->io.dci.receive_buffered == 1u && cpu->io.dci.slot == 1u,
               "I2S frame edge transfers first logical word");
        expect(state, dspic33_dci_test_clock_word(cpu, 0x3300u, false),
               "I2S half-frame transfers remaining logical word");
        expect(state,
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2200u &&
                   dspic33_read_word(cpu, DCI_RECEIVE_BASE + 2u) == 0x3300u && !cpu->io.dci.started,
               "I2S half-frame completes at programmed boundary");
        expect(state, dspic33_dci_test_clock_word(cpu, 0x4400u, false),
               "I2S waits for next frame edge after boundary");
        expect(state, cpu->io.dci.receive_buffered == 0u, "I2S idle edge gap transfers no word");
    }
}

void dspic33_dci_test_ac_link_cases(TestState* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    uint8_t slot;
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, DCI_MODE_AC_LINK_16, 4u, 1u, 2u, 0x3001u, 0x3001u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 2u, 0xccccu);
    for (slot = 0u; slot < 13u; slot++) {
        expect(state, dspic33_dci_test_clock_word(cpu, (uint16_t)(0x2000u + slot), false),
               "clock 16-bit AC-Link slot");
        expect(state,
               dspic33_dci_transmit(cpu, &transfer) && transfer.slot == slot && transfer.driven,
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
    dspic33_dci_test_configure_external(cpu, DCI_MODE_AC_LINK_20, 4u, 1u, 2u, 0x8001u, 0x8001u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1234u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 2u, 0xabcdu);
    for (slot = 0u; slot < 16u; slot++) {
        expect(state, dspic33_dci_test_clock_word(cpu, (uint16_t)(0x4000u + slot), false),
               "clock 20-bit AC-Link packed slot");
        expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.slot == slot,
               "20-bit AC-Link reports packed slot position");
    }
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x4000u &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE + 2u) == 0x400fu,
           "20-bit AC-Link buffers first and final packed slots");
    expect(state, cpu->io.dci.slot == 0u, "20-bit AC-Link wraps after sixteen packed slots");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 2u));
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
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 2u));
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

void dspic33_dci_test_protocol_integration_cases(TestState* state, Dspic33* cpu) {
    uint8_t mode;
    for (mode = 0u; mode < 4u; mode++) {
        uint16_t expected = mode >= DCI_MODE_AC_LINK_16 ? 0x5aa5u : 0x5a00u;
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_external(cpu, (uint16_t)(mode | DCI_EXTERNAL_FRAME), 8u, 2u, 1u,
                                            0u, 1u);
        expect(state, dspic33_dci_test_clock_word(cpu, 0x1100u, false),
               "protocol slave clocks without frame indication");
        expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
               "protocol slave remains idle before frame indication");
        expect(state, dspic33_dci_test_clock_word(cpu, 0x2200u, true),
               "protocol slave accepts frame indication");
        expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2200u,
               "protocol slave captures synchronized word");

        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_dma(cpu, 0u, 0x0001u, (uint32_t)(0x4400u + mode * 2u),
                                       DCI_RECEIVE_BASE, 0u, DCI_DMA_REQUEST);
        dspic33_dci_test_configure_external(cpu, mode, 8u, 1u, 1u, 0u, 1u);
        expect(state, dspic33_dci_test_clock_word(cpu, 0x5aa5u, false),
               "protocol raises receive DMA request");
        expect(state, dspic33_device_advance(cpu, 2u), "protocol completes receive DMA transfer");
        expect(state, dspic33_read_word(cpu, (uint16_t)(0x4400u + mode * 2u)) == expected,
               "protocol DMA observes effective word geometry");
    }

    for (mode = DCI_MODE_AC_LINK_16; mode <= DCI_MODE_AC_LINK_20; mode++) {
        uint8_t width;
        for (width = 1u; width < 4u; width++) {
            dspic33_reset(cpu, 0u);
            dspic33_dci_test_configure_external(cpu, mode, width, 1u, 1u, 0u, 1u);
            expect(state,
                   dspic33_dci_test_clock_word(cpu, 0xa55au, false) &&
                       dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa55au,
                   "AC-Link ignores programmed word-size field");
        }
    }
}

void dspic33_dci_test_pps_serial_input_cases(TestState* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~0x0003u));
    dspic33_write_word(cpu, GPIO_TRIS_D, (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | 0x0003u));
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, 0x0003u);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 1u, 1u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0xa500u, 8u, true),
           "clock default DCI VSS selection");
    expect(state,
           !cpu->io.dci.initialized && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u &&
               !dspic33_dci_transmit(cpu, &transfer),
           "default DCI VSS selection produces no serial transfer");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_INPUTS, 0x0101u);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true),
           "clock unavailable virtual DCI selections");
    expect(state, !cpu->io.dci.initialized, "silicon erratum suppresses virtual DCI pin remapping");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x5a00u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0xa500u, 8u, true),
           "clock PPS DCI on rising edges");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa500u &&
               dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x5a00u &&
               transfer.slot == 0u && dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "PPS DCI shifts rising-edge data MSb first");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_dma(cpu, 0u, 0x0001u, 0x4400u, DCI_RECEIVE_BASE, 0u,
                                   DCI_DMA_REQUEST);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 16u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0x6b4bu, 16u, true),
           "clock PPS DCI receive DMA block");
    expect(state, dspic33_device_advance(cpu, 2u) && dspic33_read_word(cpu, 0x4400u) == 0x6b4bu,
           "PPS DCI receive block requests DMA");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING | DCI_LOOPBACK, 8u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x5a00u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0xa500u, 8u, true),
           "clock PPS DCI loopback word");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5a00u &&
               dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x5a00u,
           "PPS DCI loopback replaces sampled pin data");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 1u, 0u);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_interrupt_set(cpu, DCI_ERROR_IRQ),
           "PPS DCI transmit underflow raises error interrupt");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, 0u, 8u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0x3c00u, 8u, false),
           "clock PPS DCI on falling edges");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x3c00u,
           "PPS DCI honors falling-edge sample selection");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) && cpu->io.dci.serial_bits == 2u,
           "PPS DCI captures partial word before remap");
    dspic33_write_word(cpu, DCI_PPS_INPUTS, (uint16_t)((PPS_FRAME_PIN << 8u) | PPS_DATA_PIN));
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) && cpu->io.dci.serial_bits == 2u,
           "old CSCK pin stops driving after remap");
    expect(state,
           drive_serial_pin_bit(cpu, false, true, GPIO_FRAME_MASK) &&
               drive_serial_pin_bit(cpu, true, true, GPIO_FRAME_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x9000u,
           "new CSCK pin completes retained partial word");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, DCI_PPS_INPUTS,
                       (uint16_t)((PPS_ANALOG_CLOCK_PIN << 8u) | PPS_DATA_PIN));
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) | GPIO_ANALOG_CLOCK_MASK));
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state, drive_serial_pin_word(cpu, 0xf000u, 4u, true, GPIO_ANALOG_CLOCK_MASK),
           "drive analog-selected DCI clock pin");
    expect(state, !cpu->io.dci.initialized, "analog-selected DCI clock pin is suppressed");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_ANALOG_CLOCK_MASK);
    dspic33_write_word(cpu, GPIO_ANALOG_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_ANALOG_D) & ~GPIO_ANALOG_CLOCK_MASK));
    expect(state,
           drive_serial_pin_word(cpu, 0x9000u, 4u, true, GPIO_ANALOG_CLOCK_MASK) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x9000u,
           "digital DCI clock pin resumes serial transfer");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) & ~GPIO_CLOCK_MASK));
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true),
           "drive output-configured DCI clock pin");
    expect(state, !cpu->io.dci.initialized, "output-configured DCI clock pin is suppressed");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(cpu, GPIO_TRIS_D,
                       (uint16_t)(dspic33_read_word(cpu, GPIO_TRIS_D) | GPIO_CLOCK_MASK));
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0x6000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6000u,
           "input-configured DCI clock pin resumes serial transfer");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_MODE_AC_LINK_16 | DCI_SAMPLE_RISING, 4u, 1u, 1u,
                                        0u, 1u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0xa55au, 20u, true),
           "clock PPS 16-bit AC-Link tag slot");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa55au && cpu->io.dci.slot == 1u,
           "PPS 16-bit AC-Link captures sixteen data and four padding clocks");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_MODE_AC_LINK_20 | DCI_SAMPLE_RISING, 4u, 1u, 1u,
                                        0u, 1u);
    expect(state, dspic33_dci_test_drive_serial_word(cpu, 0x5aa5u, 16u, true),
           "clock PPS 20-bit AC-Link packed slot");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5aa5u && cpu->io.dci.slot == 1u,
           "PPS 20-bit AC-Link captures packed sixteen-clock slot");
}
