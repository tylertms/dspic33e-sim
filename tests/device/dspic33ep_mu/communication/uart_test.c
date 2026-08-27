#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device/dspic33ep_mu/device.h"
#include "dspic33.h"
#include "test.h"

void dspic33_uart_test_boundary_cases(TestState* state, Dspic33* cpu);

static const uint16_t bases[DSPIC33_UART_COUNT] = {0x0220u, 0x0230u, 0x0250u, 0x02b0u};
static const uint8_t receive_irqs[DSPIC33_UART_COUNT] = {11u, 30u, 82u, 88u};
static const uint8_t transmit_irqs[DSPIC33_UART_COUNT] = {12u, 31u, 83u, 89u};
static const uint8_t error_irqs[DSPIC33_UART_COUNT] = {65u, 66u, 81u, 87u};
static const uint16_t pmd_addresses[DSPIC33_UART_COUNT] = {0x0760u, 0x0760u, 0x0764u, 0x0766u};
static const uint16_t pmd_masks[DSPIC33_UART_COUNT] = {0x0020u, 0x0040u, 0x0008u, 0x0020u};
static const uint16_t pps_registers[DSPIC33_UART_COUNT] = {0x06c4u, 0x06c6u, 0x06d6u, 0x06d8u};
static const uint8_t tx_functions[DSPIC33_UART_COUNT] = {1u, 3u, 27u, 29u};
static const uint8_t rts_functions[DSPIC33_UART_COUNT] = {2u, 4u, 28u, 30u};

static bool interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

static void clear_interrupt(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    dspic33_write_word(cpu, address, (uint16_t)~(uint16_t)(1u << (irq % 16u)));
}

static uint64_t frame_cycles(uint16_t mode, uint16_t baud, const Dspic33UartFrame* frame) {
    uint64_t clocks = (mode & 0x0008u) != 0u ? 4u : 16u;
    uint64_t bits = frame->break_signal
                        ? 14u
                        : (uint64_t)(1u + frame->data_bits + frame->stop_bits +
                                     (frame->parity == DSPIC33_UART_PARITY_NONE ? 0u : 1u));
    return ((uint64_t)baud + 1u) * clocks * bits;
}

static Dspic33UartParity mode_parity(uint16_t mode) {
    if ((mode & 6u) == 2u) {
        return DSPIC33_UART_PARITY_EVEN;
    }
    if ((mode & 6u) == 4u) {
        return DSPIC33_UART_PARITY_ODD;
    }
    return DSPIC33_UART_PARITY_NONE;
}

static uint8_t mode_data_bits(uint16_t mode) { return (mode & 6u) == 6u ? 9u : 8u; }

static void configure(Dspic33* cpu, uint8_t channel, uint16_t mode, uint16_t status,
                      uint16_t baud) {
    uint16_t base = bases[channel];
    dspic33_write_word(cpu, base, mode);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), baud);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), status);
}

static bool receive_frame(Dspic33* cpu, uint8_t channel, const Dspic33UartFrame* frame,
                          uint64_t delay) {
    return dspic33_uart_receive_frame(cpu, channel, frame, delay) &&
           dspic33_device_advance(cpu, delay);
}

static void configure_dma(Dspic33* cpu, uint8_t channel, uint16_t control, uint8_t request,
                          uint32_t memory, uint16_t pad, uint16_t count) {
    uint16_t base = (uint16_t)(0x0b00u + channel * 0x10u);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), (uint16_t)memory);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(memory >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 8u), (uint16_t)memory);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0au), (uint16_t)(memory >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), count);
    dspic33_write_word(cpu, base, (uint16_t)(0x8000u | control));
}

static uint16_t memory_word(Dspic33* cpu, uint32_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static void write_memory_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
}

static bool frame_parity(uint16_t value, uint8_t bits, Dspic33UartParity parity) {
    bool odd = false;
    uint8_t bit;
    for (bit = 0u; bit < bits; bit++) {
        odd = odd != ((value & (uint16_t)(1u << bit)) != 0u);
    }
    return parity == DSPIC33_UART_PARITY_EVEN ? odd : !odd;
}

static bool drive_uart_level(Dspic33* cpu, bool logical, bool inverted, uint64_t cycles) {
    bool physical = logical != inverted;
    return dspic33_gpio_drive(cpu, 3u, physical ? 1u : 0u, 1u) &&
           dspic33_device_advance(cpu, cycles);
}

static bool drive_uart_frame(Dspic33* cpu, uint16_t mode, uint16_t baud, uint16_t value) {
    uint8_t bits = mode_data_bits(mode);
    Dspic33UartParity parity = mode_parity(mode);
    uint64_t cycles = ((uint64_t)baud + 1u) * ((mode & 8u) != 0u ? 4u : 16u);
    bool inverted = (mode & 0x0010u) != 0u;
    uint8_t bit;
    if (!drive_uart_level(cpu, false, inverted, cycles)) {
        return false;
    }
    for (bit = 0u; bit < bits; bit++) {
        if (!drive_uart_level(cpu, (value & (uint16_t)(1u << bit)) != 0u, inverted, cycles)) {
            return false;
        }
    }
    if (parity != DSPIC33_UART_PARITY_NONE &&
        !drive_uart_level(cpu, frame_parity(value, bits, parity), inverted, cycles)) {
        return false;
    }
    for (bit = 0u; bit < ((mode & 1u) != 0u ? 2u : 1u); bit++) {
        if (!drive_uart_level(cpu, true, inverted, cycles)) {
            return false;
        }
    }
    return true;
}

static bool drive_irda_bit(Dspic33* cpu, bool logical, bool idle, uint64_t unit) {
    if (logical) {
        return dspic33_gpio_drive(cpu, 3u, idle ? 1u : 0u, 1u) &&
               dspic33_device_advance(cpu, 16u * unit);
    }
    return dspic33_gpio_drive(cpu, 3u, idle ? 1u : 0u, 1u) &&
           dspic33_device_advance(cpu, 7u * unit) &&
           dspic33_gpio_drive(cpu, 3u, idle ? 0u : 1u, 1u) &&
           dspic33_device_advance(cpu, 3u * unit) &&
           dspic33_gpio_drive(cpu, 3u, idle ? 1u : 0u, 1u) &&
           dspic33_device_advance(cpu, 6u * unit);
}

static bool drive_irda_frame(Dspic33* cpu, uint16_t mode, uint16_t baud, uint8_t value) {
    bool idle = (mode & 0x0010u) == 0u;
    uint64_t unit = (uint64_t)baud + 1u;
    uint8_t bit;
    if (!drive_irda_bit(cpu, false, idle, unit)) {
        return false;
    }
    for (bit = 0u; bit < 8u; bit++) {
        if (!drive_irda_bit(cpu, (value & (uint8_t)(1u << bit)) != 0u, idle, unit)) {
            return false;
        }
    }
    return drive_irda_bit(cpu, true, idle, unit);
}

static void register_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        expect(state, dspic33_read_word(cpu, base) == 0u, "mode reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0x0110u, "status reset");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0xe1f0u,
               "disabled transmitter controls");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u, "baud reset");
        dspic33_write_word(cpu, base, 0xffffu);
        expect(state, dspic33_read_word(cpu, base) == 0xbbffu, "mode write mask");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0xedf0u,
               "status write mask");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0xa55au);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0u,
               "receive register read only");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xa55au);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0xa55au, "baud full width");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        dspic33_write_word(cpu, base, 0x8000u);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0x1234u);
        expect(state, dspic33_read_word(cpu, base) == 0xbbffu,
               "PMD preserves mode and blocks write");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0xa55au,
               "PMD blocks baud write");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0xe1f0u,
               "PMD preserves controls and resets runtime");
    }
}

static void receive_value_domain(TestState* state, Dspic33* cpu) {
    static const uint16_t selections[] = {0u, 2u, 4u, 6u};
    uint8_t channel;
    uint8_t selection;
    uint16_t value;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        for (selection = 0u; selection < 4u; selection++) {
            uint16_t mode = (uint16_t)(0x8000u | selections[selection]);
            dspic33_reset(cpu, 0u);
            configure(cpu, channel, mode, 0u, 0u);
            for (value = 0u; value <= 0x01ffu; value++) {
                Dspic33UartFrame frame;
                uint16_t expected = mode_data_bits(mode) == 9u ? value : value & 0x00ffu;
                memset(&frame, 0, sizeof(frame));
                frame.value = value;
                expect(state, receive_frame(cpu, channel, &frame, 0u), "receive value schedule");
                expect(state,
                       (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x001du) ==
                           0x0011u,
                       "receive value status");
                expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 6u)) == expected,
                       "receive value domain");
            }
        }
    }
}

static void transmit_value_domain(TestState* state, Dspic33* cpu) {
    static const uint16_t selections[] = {0u, 2u, 4u, 6u};
    uint8_t channel;
    uint8_t selection;
    uint16_t value;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        for (selection = 0u; selection < 4u; selection++) {
            uint16_t mode = (uint16_t)(0x8000u | selections[selection]);
            uint64_t cycles;
            Dspic33UartFrame expected_frame;
            dspic33_reset(cpu, 0u);
            configure(cpu, channel, mode, 0x0400u, 0u);
            memset(&expected_frame, 0, sizeof(expected_frame));
            expected_frame.data_bits = mode_data_bits(mode);
            expected_frame.stop_bits = 1u;
            expected_frame.parity = mode_parity(mode);
            cycles = frame_cycles(mode, 0u, &expected_frame);
            for (value = 0u; value <= 0x01ffu; value++) {
                Dspic33UartFrame output;
                dspic33_write_word(cpu, (uint16_t)(bases[channel] + 4u), value);
                expect(state, dspic33_device_advance(cpu, cycles), "transmit value advance");
                expect(state, dspic33_uart_transmit(cpu, channel, &output),
                       "transmit value available");
                expect(state,
                       output.value == (mode_data_bits(mode) == 9u ? value : value & 0x00ffu) &&
                           output.data_bits == mode_data_bits(mode) && output.stop_bits == 1u &&
                           output.parity == mode_parity(mode) && !output.inverted && !output.irda &&
                           !output.break_signal,
                       "transmit value domain");
            }
        }
    }
}

static void receive_fifo_cases(TestState* state, Dspic33* cpu) {
    uint8_t threshold_mode;
    uint8_t index;
    for (threshold_mode = 0u; threshold_mode < 4u; threshold_mode++) {
        uint8_t threshold = threshold_mode < 2u ? 1u : (threshold_mode == 2u ? 3u : 4u);
        dspic33_reset(cpu, 0u);
        configure(cpu, 0u, 0x8000u, (uint16_t)(threshold_mode << 6u), 0u);
        clear_interrupt(cpu, receive_irqs[0]);
        for (index = 0u; index < threshold; index++) {
            Dspic33UartFrame frame;
            memset(&frame, 0, sizeof(frame));
            frame.value = (uint16_t)(0x40u + index);
            expect(state, receive_frame(cpu, 0u, &frame, 0u), "receive threshold schedule");
            expect(state, interrupt_flag(cpu, receive_irqs[0]) == (index + 1u >= threshold),
                   "receive interrupt threshold");
        }
    }

    {
        uint8_t channel;
        for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
            dspic33_reset(cpu, 0u);
            configure(cpu, channel, 0x8000u, 0u, 0u);
            clear_interrupt(cpu, error_irqs[channel]);
            for (index = 0u; index < 6u; index++) {
                Dspic33UartFrame frame;
                memset(&frame, 0, sizeof(frame));
                frame.value = (uint16_t)(0x70u + index);
                expect(state, receive_frame(cpu, channel, &frame, 0u), "receive overrun schedule");
            }
            expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0002u) != 0u,
                   "receive overrun status");
            expect(state, interrupt_flag(cpu, error_irqs[channel]), "receive overrun interrupt");
            for (index = 0u; index < 5u; index++) {
                expect(state,
                       dspic33_read_word(cpu, (uint16_t)(bases[channel] + 6u)) ==
                           (uint16_t)(0x70u + index),
                       "receive overrun held ordering");
            }
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0003u) == 0x0002u,
                   "receive overrun persists after reads");
            dspic33_write_word(cpu, (uint16_t)(bases[channel] + 2u), 0u);
            expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0003u) == 0u,
                   "receive overrun clear resets FIFO");
            {
                Dspic33UartFrame frame;
                memset(&frame, 0, sizeof(frame));
                frame.value = 0x7fu;
                expect(state, receive_frame(cpu, channel, &frame, 0u),
                       "receive overrun recovery schedule");
                expect(state,
                       dspic33_read_word(cpu, (uint16_t)(bases[channel] + 6u)) == frame.value,
                       "receive overrun recovery value");
            }

            dspic33_reset(cpu, 0u);
            configure(cpu, channel, 0x8000u, 0u, 0u);
            for (index = 0u; index < 6u; index++) {
                Dspic33UartFrame frame;
                memset(&frame, 0, sizeof(frame));
                frame.value = (uint16_t)(0x60u + index);
                expect(state, receive_frame(cpu, channel, &frame, 0u),
                       "receive populated overrun schedule");
            }
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0003u) == 0x0003u,
                   "receive populated overrun status");
            expect(state, cpu->io.uart_rx_fifo[channel].count == 4u,
                   "receive populated overrun FIFO depth");
            expect(state, (cpu->io.uart_rx_hold_valid & (uint8_t)(1u << channel)) != 0u,
                   "receive populated overrun held value");
            dspic33_write_word(cpu, (uint16_t)(bases[channel] + 2u), 0u);
            expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0003u) == 0u,
                   "receive populated overrun clear status");
            expect(state, cpu->io.uart_rx_fifo[channel].count == 0u,
                   "receive populated overrun clear FIFO");
            expect(state, (cpu->io.uart_rx_hold_valid & (uint8_t)(1u << channel)) == 0u,
                   "receive populated overrun clear held value");
            {
                Dspic33UartFrame frame;
                memset(&frame, 0, sizeof(frame));
                frame.value = 0x5du;
                expect(state, receive_frame(cpu, channel, &frame, 0u),
                       "receive populated overrun recovery schedule");
                expect(state,
                       dspic33_read_word(cpu, (uint16_t)(bases[channel] + 6u)) == frame.value,
                       "receive populated overrun recovery value");
                expect(state,
                       (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0001u) == 0u,
                       "receive populated overrun recovery empty");
            }
        }
    }
}

static void receive_access_width_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        Dspic33UartFrame frame;
        uint16_t address = (uint16_t)(bases[channel] + 6u);
        dspic33_reset(cpu, 0u);
        configure(cpu, channel, 0x8000u, 0u, 0u);
        memset(&frame, 0, sizeof(frame));
        frame.value = 0x51u;
        expect(state, receive_frame(cpu, channel, &frame, 0u),
               "eight bit byte read first schedule");
        frame.value = 0x52u;
        expect(state, receive_frame(cpu, channel, &frame, 0u),
               "eight bit byte read second schedule");
        expect(state, cpu->io.uart_rx_fifo[channel].count == 2u,
               "eight bit byte read initial depth");
        expect(state, dspic33_read_byte(cpu, address) == 0x51u, "eight bit low byte first value");
        expect(state, cpu->io.uart_rx_fifo[channel].count == 1u, "eight bit low byte first pop");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0001u) != 0u,
               "eight bit low byte remaining status");
        expect(state, dspic33_read_byte(cpu, address) == 0x52u, "eight bit low byte second value");
        expect(state, cpu->io.uart_rx_fifo[channel].count == 0u, "eight bit low byte second pop");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0001u) == 0u,
               "eight bit low byte empty status");

        dspic33_reset(cpu, 0u);
        configure(cpu, channel, 0x8006u, 0u, 0u);
        memset(&frame, 0, sizeof(frame));
        frame.value = 0x1a5u;
        expect(state, receive_frame(cpu, channel, &frame, 0u), "nine bit word read first schedule");
        frame.value = 0x155u;
        expect(state, receive_frame(cpu, channel, &frame, 0u),
               "nine bit word read second schedule");
        expect(state, cpu->io.uart_rx_fifo[channel].count == 2u,
               "nine bit word read initial depth");
        expect(state, dspic33_read_word(cpu, address) == 0x1a5u, "nine bit word read first value");
        expect(state, cpu->io.uart_rx_fifo[channel].count == 1u, "nine bit word read first pop");
        expect(state, dspic33_read_word(cpu, address) == 0x155u, "nine bit word read second value");
        expect(state, cpu->io.uart_rx_fifo[channel].count == 0u, "nine bit word read second pop");
    }
}

static void receive_error_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t combination;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        for (combination = 0u; combination < 4u; combination++) {
            Dspic33UartFrame frame;
            uint16_t expected = (uint16_t)(((combination & 1u) != 0u ? 0x0008u : 0u) |
                                           ((combination & 2u) != 0u ? 0x0004u : 0u));
            dspic33_reset(cpu, 0u);
            configure(cpu, channel, 0x8002u, 0u, 0u);
            clear_interrupt(cpu, error_irqs[channel]);
            memset(&frame, 0, sizeof(frame));
            frame.value = 0x5au;
            frame.parity_error = (combination & 1u) != 0u;
            frame.framing_error = (combination & 2u) != 0u;
            expect(state, receive_frame(cpu, channel, &frame, 0u), "receive error schedule");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x000cu) == expected,
                   "receive buffered error status");
            expect(state, interrupt_flag(cpu, error_irqs[channel]) == (expected != 0u),
                   "receive error interrupt");
            expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 6u)) == 0x5au,
                   "receive error data");
        }
    }
    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8006u, 0u, 0u);
    {
        Dspic33UartFrame frame;
        memset(&frame, 0, sizeof(frame));
        frame.value = 0x1a5u;
        frame.parity_error = true;
        expect(state, receive_frame(cpu, 0u, &frame, 0u), "nine bit parity schedule");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 0x0008u) == 0u,
               "nine bit ignores parity error");
    }
}

static void special_receive_cases(TestState* state, Dspic33* cpu) {
    Dspic33UartFrame frame;
    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8006u, 0x0020u, 0u);
    memset(&frame, 0, sizeof(frame));
    frame.value = 0x055u;
    expect(state, receive_frame(cpu, 0u, &frame, 0u), "address data schedule");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 1u) == 0u,
           "address detect discards data word");
    frame.value = 0x155u;
    expect(state, receive_frame(cpu, 0u, &frame, 0u), "address word schedule");
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[0] + 6u)) == 0x155u,
           "address detect accepts address word");

    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8080u, 0u, 0u);
    clear_interrupt(cpu, receive_irqs[0]);
    memset(&frame, 0, sizeof(frame));
    frame.value = 0x33u;
    expect(state, receive_frame(cpu, 0u, &frame, 0u), "wake byte schedule");
    expect(state,
           (dspic33_read_word(cpu, bases[0]) & 0x0080u) == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 1u) == 0u &&
               interrupt_flag(cpu, receive_irqs[0]),
           "wake byte ignored and interrupt raised");

    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8020u, 0u, 0x1234u);
    clear_interrupt(cpu, receive_irqs[0]);
    memset(&frame, 0, sizeof(frame));
    frame.value = 0x55u;
    frame.baud_period = 0x0456u;
    expect(state, receive_frame(cpu, 0u, &frame, 0u), "auto baud schedule");
    expect(state,
           (dspic33_read_word(cpu, bases[0]) & 0x0020u) == 0u &&
               dspic33_read_word(cpu, (uint16_t)(bases[0] + 8u)) == 0x0456u &&
               (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 1u) == 0u &&
               interrupt_flag(cpu, receive_irqs[0]),
           "auto baud captures period and ignores byte");
}

static void transmit_fifo_cases(TestState* state, Dspic33* cpu) {
    uint8_t index;
    Dspic33UartFrame output;
    Dspic33UartFrame timing;
    uint64_t cycles;
    memset(&timing, 0, sizeof(timing));
    timing.data_bits = 8u;
    timing.stop_bits = 1u;
    timing.parity = DSPIC33_UART_PARITY_NONE;
    cycles = frame_cycles(0x8000u, 0u, &timing);
    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8000u, 0x0400u, 0u);
    for (index = 0u; index < 6u; index++) {
        dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), (uint16_t)(0x20u + index));
    }
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 0x0200u) != 0u,
           "transmit FIFO full");
    for (index = 0u; index < 5u; index++) {
        expect(state, dspic33_device_advance(cpu, cycles), "transmit FIFO advance");
        expect(state,
               dspic33_uart_transmit(cpu, 0u, &output) && output.value == (uint16_t)(0x20u + index),
               "transmit FIFO ordering");
    }
    expect(state, !dspic33_uart_transmit(cpu, 0u, &output), "transmit FIFO ignores overflow write");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 0x0300u) == 0x0100u,
           "transmit FIFO empty status");
}

static void transmit_timing_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t modes[] = {0x8000u, 0x8001u, 0x8002u, 0x8004u, 0x8006u, 0x8008u, 0x9008u};
    static const uint16_t bauds[] = {0u, 1u, 0x00ffu, 0xffffu};
    uint8_t mode_index;
    uint8_t baud_index;
    for (mode_index = 0u; mode_index < sizeof(modes) / sizeof(modes[0]); mode_index++) {
        for (baud_index = 0u; baud_index < sizeof(bauds) / sizeof(bauds[0]); baud_index++) {
            Dspic33UartFrame expected;
            Dspic33UartFrame output = {0};
            uint64_t cycles;
            uint16_t mode = modes[mode_index];
            uint16_t baud = bauds[baud_index];
            memset(&expected, 0, sizeof(expected));
            expected.data_bits = mode_data_bits(mode);
            expected.stop_bits = (mode & 1u) != 0u ? 2u : 1u;
            expected.parity = mode_parity(mode);
            cycles = frame_cycles(mode, baud, &expected);
            dspic33_reset(cpu, 0u);
            configure(cpu, 0u, mode, 0x0400u, baud);
            dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0xa5u);
            expect(state,
                   dspic33_device_advance(cpu, cycles - 1u) &&
                       !dspic33_uart_transmit(cpu, 0u, &output),
                   "transmit timing lower boundary");
            expect(state,
                   dspic33_device_advance(cpu, 1u) && dspic33_uart_transmit(cpu, 0u, &output),
                   "transmit timing completion boundary");
            expect(state,
                   output.baud_period == baud && output.data_bits == expected.data_bits &&
                       output.stop_bits == expected.stop_bits && output.parity == expected.parity &&
                       output.irda == ((mode & 0x1000u) != 0u),
                   "transmit timing frame metadata");
        }
    }
}

static void transmit_mode_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    uint8_t interrupt_mode;
    Dspic33UartFrame output;
    for (interrupt_mode = 0u; interrupt_mode < 3u; interrupt_mode++) {
        uint16_t status = (uint16_t)(0x0400u | ((interrupt_mode & 1u) != 0u ? 0x2000u : 0u) |
                                     ((interrupt_mode & 2u) != 0u ? 0x8000u : 0u));
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, bases[0], 0x8000u);
        clear_interrupt(cpu, transmit_irqs[0]);
        dspic33_write_word(cpu, (uint16_t)(bases[0] + 2u), status);
        expect(state, interrupt_flag(cpu, transmit_irqs[0]), "transmit enable interrupt mode");
        clear_interrupt(cpu, transmit_irqs[0]);
        dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x3cu);
        expect(state,
               interrupt_mode == 1u ? cpu->stop_reason == DSPIC33_SILICON_RESULT_UNDEFINED
                                    : interrupt_flag(cpu, transmit_irqs[0]) ==
                                          (interrupt_mode == 0u || interrupt_mode == 2u),
               "transmit start interrupt mode");
        clear_interrupt(cpu, transmit_irqs[0]);
        if (interrupt_mode == 1u) {
            expect(state, cpu->events.count == 0u,
                   "B1 final-character interrupt timing remains unspecified");
            expect(state, !dspic33_uart_transmit(cpu, 0u, &output),
                   "B1 unspecified transmit does not report an ideal frame");
            continue;
        }
        expect(state, dspic33_device_advance(cpu, 160u), "transmit interrupt completion advance");
        expect(state, !interrupt_flag(cpu, transmit_irqs[0]), "transmit completion interrupt mode");
        expect(state, dspic33_uart_transmit(cpu, 0u, &output), "transmit interrupt output");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, bases[0], 0x8000u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 2u), 0x2000u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x31u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x32u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 2u), 0x2400u);
    expect(state,
           cpu->stop_reason == DSPIC33_RUNNING && (cpu->io.uart_tx_active & 1u) != 0u &&
               cpu->io.uart_tx_fifo[0].count == 1u,
           "non-final UTXISEL character remains deterministic");
    expect(state,
           dspic33_device_advance(cpu, 160u) &&
               cpu->stop_reason == DSPIC33_SILICON_RESULT_UNDEFINED,
           "UTXISEL becomes unspecified when the final TSR character starts");

    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x9041u, 0x4400u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x5au);
    expect(state, dspic33_device_advance(cpu, 176u), "loopback advance");
    expect(state,
           dspic33_uart_transmit(cpu, 0u, &output) && output.value == 0x5au &&
               output.stop_bits == 2u && output.inverted && output.irda,
           "loopback transmit metadata");
    expect(state, dspic33_read_word(cpu, (uint16_t)(bases[0] + 6u)) == 0x5au,
           "loopback receive data");

    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, bases[channel], 0x8000u);
        clear_interrupt(cpu, transmit_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 2u), 0x0400u);
        expect(state, interrupt_flag(cpu, transmit_irqs[channel]),
               "transmit enable raises interrupt");
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 4u), 0x5au);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 2u), 0u);
        expect(state,
               dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_uart_transmit(cpu, channel, &output) &&
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0511u) == 0x0110u,
               "transmit disable aborts and resets transmitter");

        dspic33_reset(cpu, 0u);
        configure(cpu, channel, 0x8000u, 0x0c00u, 0u);
        clear_interrupt(cpu, transmit_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 4u), 0x7eu);
        expect(state, !interrupt_flag(cpu, transmit_irqs[channel]),
               "break transfer suppresses transmit interrupt");
        expect(state, dspic33_device_advance(cpu, 224u), "break advance");
        expect(state,
               dspic33_uart_transmit(cpu, channel, &output) && output.break_signal &&
                   output.value == 0u && output.data_bits == 12u &&
                   (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 0x0800u) == 0u,
               "break frame and automatic clear");
    }
}

static void b1_transmit_pointer_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    Dspic33UartFrame output;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t value = (uint16_t)(0xa0u + channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, bases[channel], 0x8000u);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 4u), value);
        expect(state,
               cpu->io.uart_tx_fifo[channel].count == 1u && dspic33_device_advance(cpu, 1000u) &&
                   !dspic33_uart_transmit(cpu, channel, &output),
               "B1 UART accepts TXREG before TXEN without transmitting");
        dspic33_write_word(cpu, bases[channel], 0u);
        expect(state, cpu->io.uart_tx_fifo[channel].count == 1u,
               "B1 UART disable preserves write pointer while TXEN is clear");
        dspic33_write_word(cpu, bases[channel], 0x8000u);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 2u), 0x0400u);
        expect(state,
               dspic33_device_advance(cpu, 160u) && dspic33_uart_transmit(cpu, channel, &output) &&
                   output.value == value,
               "B1 UART transmits preserved data after TXEN is set");

        dspic33_reset(cpu, 0u);
        configure(cpu, channel, 0x8000u, 0x0400u, 0u);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 4u), value);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 4u), (uint16_t)(value + 0x10u));
        dspic33_write_word(cpu, bases[channel], 0u);
        expect(state,
               cpu->io.uart_tx_fifo[channel].count == 0u &&
                   (cpu->io.uart_tx_active & (uint8_t)(1u << channel)) == 0u,
               "B1 UART disable clears write pointer while TXEN is set");
        dspic33_write_word(cpu, bases[channel], 0x8000u);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 2u), 0x0400u);
        expect(state,
               dspic33_device_advance(cpu, 1000u) && !dspic33_uart_transmit(cpu, channel, &output),
               "B1 UART cleared write pointer does not transmit stale data");
    }
}

static void cts_cases(TestState* state, Dspic33* cpu) {
    Dspic33UartFrame output;
    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8200u, 0x0400u, 0u);
    expect(state, dspic33_uart_set_cts(cpu, 0u, false, 0u) && dspic33_device_advance(cpu, 0u),
           "CTS block schedule");
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x99u);
    expect(state, dspic33_device_advance(cpu, 1000u) && !dspic33_uart_transmit(cpu, 0u, &output),
           "CTS blocks completion");
    expect(state,
           dspic33_uart_set_cts(cpu, 0u, true, 3u) && dspic33_device_advance(cpu, 2u) &&
               !dspic33_uart_transmit(cpu, 0u, &output),
           "CTS delayed release boundary");
    expect(state, dspic33_device_advance(cpu, 1u), "CTS release event");
    expect(state, dspic33_device_advance(cpu, 159u) && !dspic33_uart_transmit(cpu, 0u, &output),
           "CTS transmit timing boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_uart_transmit(cpu, 0u, &output) &&
               output.value == 0x99u,
           "CTS release completes transmit");
}

static void dma_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint32_t receive_memory = (uint32_t)(0x3000u + channel * 0x20u);
        uint32_t transmit_memory = receive_memory + 2u;
        Dspic33UartFrame frame;
        Dspic33UartFrame output;
        dspic33_reset(cpu, 0u);
        configure_dma(cpu, channel, 0x0001u, receive_irqs[channel], receive_memory,
                      (uint16_t)(bases[channel] + 6u), 0u);
        configure(cpu, channel, 0x8002u, 0u, 0u);
        memset(&frame, 0, sizeof(frame));
        frame.value = (uint16_t)(0x80u + channel);
        frame.parity_error = true;
        expect(state, receive_frame(cpu, channel, &frame, 0u), "UART receive DMA schedule");
        expect(state, dspic33_device_advance(cpu, 0u), "UART receive DMA advance");
        expect(state, memory_word(cpu, receive_memory) == (uint16_t)(0x0880u + channel),
               "UART receive DMA data and error bits");

        dspic33_reset(cpu, 0u);
        write_memory_word(cpu, transmit_memory, (uint16_t)(0x1a0u + channel));
        configure_dma(cpu, channel, 0x2001u, transmit_irqs[channel], transmit_memory,
                      (uint16_t)(bases[channel] + 4u), 0u);
        dspic33_write_word(cpu, bases[channel], 0x8006u);
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 2u), 0x0400u);
        expect(state, dspic33_device_advance(cpu, 0u), "UART transmit DMA request");
        expect(state, dspic33_device_advance(cpu, 176u), "UART transmit DMA advance");
        expect(state,
               dspic33_uart_transmit(cpu, channel, &output) &&
                   output.value == (uint16_t)(0x1a0u + channel),
               "UART transmit DMA data");
    }
}

static void physical_pps_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t receive_modes[] = {0x8000u, 0x8012u, 0x800cu, 0x801fu};
    static const uint16_t receive_values[] = {0x00a5u, 0x003cu, 0x005au, 0x0155u};
    uint8_t channel;
    bool high = false;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t mode = receive_modes[channel];
        uint16_t value = receive_values[channel];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        expect(state, dspic33_gpio_drive(cpu, 3u, 1u, 1u), "UART PPS drive receive idle");
        dspic33_write_word(cpu, pps_registers[channel], 64u);
        configure(cpu, channel, mode, 0u, (uint16_t)channel);
        expect(state, drive_uart_level(cpu, true, (mode & 0x0010u) != 0u, 0u),
               "UART PPS establish receive idle");
        expect(state, drive_uart_frame(cpu, mode, (uint16_t)channel, value),
               "UART PPS receive frame timing");
        expect(state, dspic33_read_word(cpu, (uint16_t)(bases[channel] + 6u)) == value,
               "UART PPS receive frame value");
        expect(state, interrupt_flag(cpu, receive_irqs[channel]), "UART PPS receive interrupt");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0680u, tx_functions[channel]);
        configure(cpu, channel, 0x8000u, 0x0400u, 0u);
        expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && high, "UART PPS transmit idle high");
        dspic33_write_word(cpu, (uint16_t)(bases[channel] + 4u), 0x00a5u);
        expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high, "UART PPS transmit start bit");
        expect(state,
               dspic33_device_advance(cpu, 16u) && dspic33_gpio_pin(cpu, 3u, 0u, &high) && high,
               "UART PPS transmit first data bit");
        expect(state,
               dspic33_device_advance(cpu, 16u) && dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high,
               "UART PPS transmit second data bit");
        expect(state,
               dspic33_device_advance(cpu, 128u) && dspic33_gpio_pin(cpu, 3u, 0u, &high) && high,
               "UART PPS transmit stop and idle");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_gpio_drive(cpu, 3u, 2u, 2u);
    dspic33_write_word(cpu, pps_registers[0], (uint16_t)(65u << 8u));
    dspic33_write_word(cpu, 0x0680u, tx_functions[0]);
    configure(cpu, 0u, 0x8200u, 0x0400u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x55u);
    expect(state,
           dspic33_device_advance(cpu, 160u) &&
               !dspic33_uart_transmit(cpu, 0u, &(Dspic33UartFrame){0}),
           "UART PPS high CTS blocks transmit");
    expect(state, dspic33_gpio_drive(cpu, 3u, 0u, 2u) && dspic33_device_advance(cpu, 160u),
           "UART PPS low CTS starts transmit");
    {
        Dspic33UartFrame output;
        expect(state, dspic33_uart_transmit(cpu, 0u, &output) && output.value == 0x55u,
               "UART PPS CTS completed value");
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, rts_functions[0]);
    configure(cpu, 0u, 0x8100u, 0u, 0u);
    expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high, "UART PPS flow RTS ready low");
    dspic33_write_word(cpu, bases[0], 0x8900u);
    expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && high, "UART PPS simplex RTS empty high");
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 2u), 0x0400u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x33u);
    expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high, "UART PPS simplex RTS active low");
}

static void physical_irda_cases(TestState* state, Dspic33* cpu) {
    for (uint8_t channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t mode = (uint16_t)(0x9000u | ((channel & 1u) != 0u ? 0x0010u : 0u));
        uint16_t baud = (uint16_t)(channel + 1u);
        uint8_t value = (uint8_t)(0x96u + channel);
        bool idle = (mode & 0x0010u) == 0u;

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_gpio_drive(cpu, 3u, idle ? 1u : 0u, 1u);
        dspic33_write_word(cpu, pps_registers[channel], 64u);
        configure(cpu, channel, mode, 0u, baud);
        expect(state, drive_irda_frame(cpu, mode, baud, value), "UART PPS IrDA receive timing");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(bases[channel] + 6u)) == value &&
                   interrupt_flag(cpu, receive_irqs[channel]),
               "UART PPS IrDA decoder feeds receive FIFO");
    }
}

static void physical_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool high = false;
    bool initialized;
    uint16_t mode = 0x8000u;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_gpio_drive(cpu, 3u, 1u, 1u);
    dspic33_write_word(cpu, pps_registers[0], 64u);
    configure(cpu, 0u, 0x8080u, 0u, 0u);
    clear_interrupt(cpu, receive_irqs[0]);
    expect(state,
           dspic33_gpio_drive(cpu, 3u, 0u, 1u) && interrupt_flag(cpu, receive_irqs[0]) &&
               (dspic33_read_word(cpu, bases[0]) & 0x0080u) != 0u,
           "UART PPS wake falling edge interrupts without clearing WAKE");
    expect(state,
           dspic33_gpio_drive(cpu, 3u, 1u, 1u) &&
               (dspic33_read_word(cpu, bases[0]) & 0x0080u) == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 1u) == 0u,
           "UART PPS wake rising edge clears WAKE without receiving character");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0e3eu, 0u);
    dspic33_gpio_drive(cpu, 3u, 1u, 1u);
    dspic33_write_word(cpu, pps_registers[0], 64u);
    configure(cpu, 0u, mode, 0u, 0u);
    expect(state, dspic33_gpio_drive(cpu, 3u, 0u, 1u) && (cpu->io.uart_rx_active & 1u) != 0u,
           "UART PPS falling edge starts physical receive");
    dspic33_write_word(cpu, bases[0], 0x8008u);
    expect(state,
           (cpu->io.uart_rx_active & 1u) == 0u && cpu->io.uart_rx_votes[0] == 0u &&
               cpu->io.uart_rx_shift[0].data_bits == 0u,
           "UART mode change cancels physical receive");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, tx_functions[0]);
    dspic33_gpio_drive(cpu, 3u, 0u, 1u);
    dspic33_write_word(cpu, 0x0e36u, 1u);
    configure(cpu, 0u, mode, 0x0400u, 0u);
    expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high,
           "UART PPS open-drain idle releases to external low");
    expect(state,
           dspic33_gpio_drive(cpu, 3u, 1u, 1u) && dspic33_gpio_pin(cpu, 3u, 0u, &high) && high,
           "UART PPS open-drain idle follows external high");
    dspic33_write_word(cpu, 0x0680u, 0u);
    dspic33_write_word(cpu, 0x0e36u, 0u);
    dspic33_write_word(cpu, 0x0e34u, 0u);
    dspic33_write_word(cpu, 0x0e30u, 0u);
    expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high,
           "UART PPS remap releases pin to GPIO latch");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, tx_functions[0]);
    dspic33_gpio_drive(cpu, 3u, 1u, 1u);
    configure(cpu, 0u, mode, 0x0400u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0u);
    expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high,
           "UART PPS active transmit before PMD");
    dspic33_write_word(cpu, pmd_addresses[0], pmd_masks[0]);
    expect(state, dspic33_gpio_pin(cpu, 3u, 0u, &high) && high && cpu->io.uart_tx_active == 0u,
           "UART PPS PMD aborts transmit and releases idle");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, tx_functions[0]);
    configure(cpu, 0u, mode, 0x0400u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x01u);
    initialized = dspic33_initialize(&copy);
    expect(state, initialized && dspic33_copy(&copy, cpu), "UART PPS copy active transmit");
    if (initialized) {
        bool source_high = false;
        bool copy_high = false;
        expect(state,
               dspic33_device_advance(cpu, 16u) && dspic33_device_advance(&copy, 16u) &&
                   dspic33_gpio_pin(cpu, 3u, 0u, &source_high) &&
                   dspic33_gpio_pin(&copy, 3u, 0u, &copy_high) && source_high == copy_high,
               "UART PPS copied transmit phase matches");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0680u, tx_functions[0]);
    configure(cpu, 0u, 0x9000u, 0x0400u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0u);
    expect(state, dspic33_device_advance(cpu, 6u) && dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high,
           "UART PPS IrDA zero before pulse");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_gpio_pin(cpu, 3u, 0u, &high) && high,
           "UART PPS IrDA zero pulse begins");
    expect(state, dspic33_device_advance(cpu, 3u) && dspic33_gpio_pin(cpu, 3u, 0u, &high) && !high,
           "UART PPS IrDA zero pulse ends");
}

static void physical_auto_baud_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_UART_COUNT; channel++) {
        uint16_t mode = (uint16_t)(0x8020u | ((channel & 1u) != 0u ? 8u : 0u));
        uint16_t expected_baud = (uint16_t)(channel + 1u);
        uint64_t clocks = (mode & 8u) != 0u ? 4u : 16u;
        uint64_t bit_cycles = ((uint64_t)expected_baud + 1u) * clocks;
        uint8_t bit;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, 0x0e3eu, 0u);
        dspic33_gpio_drive(cpu, 3u, 1u, 1u);
        dspic33_write_word(cpu, pps_registers[channel], 64u);
        configure(cpu, channel, mode, 0u, 0x1234u);
        clear_interrupt(cpu, receive_irqs[channel]);
        expect(state, drive_uart_level(cpu, false, false, bit_cycles),
               "UART PPS auto-baud start edge");
        for (bit = 0u; bit < 8u; bit++) {
            expect(state,
                   drive_uart_level(cpu, (0x55u & (uint8_t)(1u << bit)) != 0u, false, bit_cycles),
                   "UART PPS auto-baud data edge");
        }
        expect(state, drive_uart_level(cpu, true, false, bit_cycles),
               "UART PPS auto-baud stop edge");
        expect(state,
               dspic33_read_word(cpu, (uint16_t)(bases[channel] + 8u)) == expected_baud &&
                   (dspic33_read_word(cpu, bases[channel]) & 0x0020u) == 0u &&
                   interrupt_flag(cpu, receive_irqs[channel]),
               "UART PPS auto-baud fifth rising edge result");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[channel] + 2u)) & 1u) == 0u,
               "UART PPS auto-baud does not populate receive FIFO");
    }
}

static void disable_copy_and_api_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33UartFrame frame;
    Dspic33UartFrame first;
    Dspic33UartFrame second;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize UART copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8000u, 0x0400u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x66u);
    expect(state, dspic33_copy(&copy, cpu), "copy pending UART state");
    expect(state, dspic33_device_advance(cpu, 160u) && dspic33_device_advance(&copy, 160u),
           "advance copied UART state");
    expect(state,
           dspic33_uart_transmit(cpu, 0u, &first) && dspic33_uart_transmit(&copy, 0u, &second) &&
               first.value == second.value && first.data_bits == second.data_bits,
           "copied UART output matches");
    dspic33_release(&copy);

    dspic33_reset(cpu, 0u);
    configure(cpu, 0u, 0x8000u, 0x0400u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0x44u);
    dspic33_write_word(cpu, bases[0], 0u);
    expect(state, dspic33_device_advance(cpu, 1000u) && !dspic33_uart_transmit(cpu, 0u, &first),
           "module disable cancels transmit");
    expect(state, (dspic33_read_word(cpu, (uint16_t)(bases[0] + 2u)) & 0x0511u) == 0x0110u,
           "module disable resets runtime status");

    memset(&frame, 0, sizeof(frame));
    frame.value = 0x0200u;
    expect(state, !dspic33_uart_receive_frame(cpu, 0u, &frame, 0u),
           "reject receive value outside nine bits");
    frame.value = 0u;
    expect(state, !dspic33_uart_receive_frame(cpu, DSPIC33_UART_COUNT, &frame, 0u),
           "reject receive channel outside range");
    expect(state, !dspic33_uart_receive_frame(cpu, 0u, NULL, 0u), "reject null receive frame");
    expect(state, !dspic33_uart_set_cts(cpu, DSPIC33_UART_COUNT, true, 0u),
           "reject CTS channel outside range");
    expect(state, !dspic33_uart_transmit(cpu, DSPIC33_UART_COUNT, &frame),
           "reject transmit channel outside range");
    expect(state, !dspic33_uart_transmit(cpu, 0u, NULL), "reject null transmit frame");
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.uart_tx_active == 0u && cpu->io.uart_tx_scheduled == 0u &&
               cpu->io.uart_rx_hold_valid == 0u && cpu->io.uart_cts == 0x0fu,
           "reset clears UART runtime and releases CTS");
}

static void break_rmw_erratum_cases(TestState* state, Dspic33* cpu) {
    Dspic33UartFrame output;
    uint16_t status = (uint16_t)(bases[0] + 2u);
    dspic33_reset(cpu, 0x0200u);
    configure(cpu, 0u, 0x8000u, 0x0c00u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0u);
    dspic33_device_advance(cpu, 223u);
    dspic33_load_program_word(cpu, 0x0200u, 0xa80223u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED &&
               (dspic33_read_word(cpu, status) & 0x0800u) != 0u &&
               dspic33_uart_transmit(cpu, 0u, &output),
           "B1 UTXBRK clear concurrent with a UART status RMW remains undefined");

    dspic33_reset(cpu, 0x0200u);
    configure(cpu, 0u, 0x8000u, 0x0c00u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0u);
    dspic33_device_advance(cpu, 223u);
    dspic33_load_program_word(cpu, 0x0200u, (uint32_t)(0xec2000u | status));
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "B1 UTXBRK clear detects non-bit RMW instructions");

    dspic33_reset(cpu, 0x0200u);
    configure(cpu, 0u, 0x8000u, 0x0c00u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0u);
    dspic33_device_advance(cpu, 223u);
    cpu->w[0] = dspic33_read_word(cpu, status);
    dspic33_load_program_word(cpu, 0x0200u, (uint32_t)(0x880000u | status / 2u));
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "plain MOV remains outside the UTXBRK RMW boundary");

    dspic33_reset(cpu, 0x0200u);
    configure(cpu, 0u, 0x8000u, 0x0c00u, 0u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 4u), 0u);
    dspic33_device_advance(cpu, 223u);
    dspic33_load_program_word(cpu, 0x0200u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, status) & 0x0800u) == 0u &&
               dspic33_uart_transmit(cpu, 0u, &output),
           "UTXBRK clears normally without a concurrent UART status RMW");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize UART processor");
    if (initialized) {
        register_cases(&state, &cpu);
        receive_value_domain(&state, &cpu);
        transmit_value_domain(&state, &cpu);
        receive_fifo_cases(&state, &cpu);
        receive_access_width_cases(&state, &cpu);
        receive_error_cases(&state, &cpu);
        special_receive_cases(&state, &cpu);
        transmit_fifo_cases(&state, &cpu);
        transmit_timing_cases(&state, &cpu);
        transmit_mode_cases(&state, &cpu);
        b1_transmit_pointer_cases(&state, &cpu);
        cts_cases(&state, &cpu);
        dma_cases(&state, &cpu);
        physical_pps_cases(&state, &cpu);
        physical_irda_cases(&state, &cpu);
        physical_lifecycle_cases(&state, &cpu);
        physical_auto_baud_cases(&state, &cpu);
        dspic33_uart_test_boundary_cases(&state, &cpu);
        break_rmw_erratum_cases(&state, &cpu);
        disable_copy_and_api_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
