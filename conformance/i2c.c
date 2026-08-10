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
} I2cConformance;

static const uint16_t bases[DSPIC33_I2C_COUNT] = {0x0200u, 0x0210u};
static const uint8_t slave_irqs[DSPIC33_I2C_COUNT] = {16u, 49u};
static const uint8_t master_irqs[DSPIC33_I2C_COUNT] = {17u, 50u};

static void expect(I2cConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[i2c-failed] %s\n", name);
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

static uint16_t stored_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] |
                      ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

static uint64_t operation_cycles(uint16_t baud, uint8_t half_periods) {
    return ((uint64_t)(baud + 2u) * half_periods + 1u) / 2u;
}

static uint64_t control_cycles(uint16_t baud) { return operation_cycles(baud, 2u); }

static uint64_t byte_cycles(uint16_t baud) { return operation_cycles(baud, 18u); }

static uint64_t receive_cycles(uint16_t baud) { return operation_cycles(baud, 16u); }

static uint64_t condition_cycles(uint16_t baud) { return operation_cycles(baud, 3u); }

static void configure_dma_channel(Dspic33* cpu, uint8_t channel, uint8_t request,
                                  uint16_t start, uint16_t pad) {
    uint16_t base = (uint16_t)(0x0b00u + channel * 0x10u);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), start);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 10u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 12u), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 14u), 0u);
    dspic33_write_word(cpu, base, 0x8001u);
}

static void enable(Dspic33* cpu, uint8_t channel, uint16_t options, uint16_t baud) {
    uint16_t base = bases[channel];
    dspic33_write_word(cpu, (uint16_t)(base + 4u), baud);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(0x9000u | options));
}

static void register_cases(I2cConformance* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        expect(state, dspic33_read_word(cpu, base) == 0u, "receive reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 2u)) == 0u,
               "transmit reads zero");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0u,
               "baud reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0x1000u,
               "control reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "status reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 10u)) == 0u,
               "address reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 12u)) == 0u,
               "mask reset");
        dspic33_write_word(cpu, base, 0xffffu);
        expect(state, dspic33_read_word(cpu, base) == 0u, "receive read only");
        dspic33_write_word(cpu, (uint16_t)(base + 4u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 4u)) == 0x01ffu,
               "baud mask");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0xbfe0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0xbfe0u,
               "control mask");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "status clear-only mask");
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 10u)) == 0x03ffu,
               "address mask");
        dspic33_write_word(cpu, (uint16_t)(base + 12u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 12u)) == 0x03ffu,
               "slave mask mask");
    }
}

static void timing_cases(I2cConformance* state, Dspic33* cpu) {
    static const uint16_t baud_values[] = {2u, 3u, 17u, 0x01ffu};
    uint8_t channel;
    size_t index;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        for (index = 0u; index < sizeof(baud_values) / sizeof(baud_values[0]);
             index++) {
            uint16_t base = bases[channel];
            uint16_t baud = baud_values[index];
            uint64_t cycles = control_cycles(baud);
            Dspic33I2cTransfer transfer;
            dspic33_reset(cpu, 0u);
            enable(cpu, channel, 1u, baud);
            expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) != 0u,
                   "start active at write");
            expect(state, dspic33_device_advance(cpu, cycles - 1u),
                   "start boundary advance");
            expect(state, !interrupt_flag(cpu, master_irqs[channel]),
                   "start not complete early");
            expect(state, dspic33_device_advance(cpu, 1u), "start completion advance");
            expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) == 0u,
                   "start clears request");
            expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 8u) != 0u,
                   "start sets bus state");
            expect(state, interrupt_flag(cpu, master_irqs[channel]), "start interrupt");
            if (channel == 1u) {
                expect(state, !interrupt_flag(cpu, 51u),
                       "second module does not raise timer eight interrupt");
            }
            expect(state,
                   dspic33_i2c_transmit(cpu, channel, &transfer) &&
                       transfer.type == DSPIC33_I2C_START && transfer.master,
                   "start output");
        }
    }
}

static void master_sequence_cases(I2cConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t baud = (uint16_t)(3u + channel);
        uint64_t control = control_cycles(baud);
        uint64_t receive = receive_cycles(baud);
        uint64_t condition = condition_cycles(baud);
        uint64_t byte = byte_cycles(baud);
        Dspic33I2cTransfer transfer;
        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 1u, baud);
        expect(state, dspic33_device_advance(cpu, control), "sequence start");
        clear_interrupt(cpu, master_irqs[channel]);
        expect(state, dspic33_i2c_respond(cpu, channel, 0u, true, byte),
               "queue transmit acknowledge");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x01a5u);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u,
               "transmit busy flags");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x005au);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0080u) != 0u,
               "transmit write collision");
        expect(state, dspic33_device_advance(cpu, receive),
               "transmit shift completion");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4000u,
               "transmit buffer clears before acknowledge");
        expect(state, dspic33_device_advance(cpu, byte - receive - 1u),
               "transmit boundary advance");
        expect(state, !interrupt_flag(cpu, master_irqs[channel]),
               "transmit not complete early");
        expect(state, dspic33_device_advance(cpu, 1u), "transmit complete");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0xc001u) == 0u,
               "transmit acknowledged status");
        expect(state, interrupt_flag(cpu, master_irqs[channel]), "transmit interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_START,
               "sequence start output");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_WRITE && transfer.value == 0xa5u,
               "sequence write output");

        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        expect(state, dspic33_i2c_respond(cpu, channel, 0x6cu, true, receive),
               "queue receive byte");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive), "receive complete");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) != 0u,
               "receive buffer full");
        expect(state, dspic33_read_word(cpu, base) == 0x006cu, "receive value");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "receive read clears full");
        expect(state, interrupt_flag(cpu, master_irqs[channel]), "receive interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_READ && transfer.value == 0x6cu,
               "receive output");

        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9030u);
        expect(state, dspic33_device_advance(cpu, control), "nack complete");
        expect(state, interrupt_flag(cpu, master_irqs[channel]), "nack interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_ACKNOWLEDGE && !transfer.acknowledge,
               "nack output");

        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9002u);
        expect(state, dspic33_device_advance(cpu, condition), "restart complete");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_RESTART,
               "restart output");
        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9004u);
        expect(state, dspic33_device_advance(cpu, condition), "stop complete");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0010u,
               "stop bus state");
        expect(state, interrupt_flag(cpu, master_irqs[channel]), "stop interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_STOP,
               "stop output");
    }
}

static void master_error_cases(I2cConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint64_t control = control_cycles(0u);
        uint64_t receive = receive_cycles(0u);
        uint64_t byte = byte_cycles(0u);
        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 1u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x44u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0080u) != 0u,
               "write during start collision");
        expect(state, dspic33_device_advance(cpu, control), "error start complete");
        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x55u);
        expect(state, dspic33_device_advance(cpu, byte), "unacknowledged complete");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x8000u) != 0u,
               "unacknowledged status");
        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive), "first unread receive");
        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive), "second unread receive");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0042u) == 0x0042u,
               "receive overflow preserves full");
        expect(state,
               dspic33_i2c_collision(cpu, channel, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "collision event");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) != 0u,
               "bus collision status");
        expect(state, interrupt_flag(cpu, master_irqs[channel]),
               "bus collision interrupt");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xffffu);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) != 0u,
               "bus collision not software settable");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) == 0u,
               "bus collision software clear");

        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 1u, 0u);
        expect(state, dspic33_device_advance(cpu, control), "stretch start complete");
        clear_interrupt(cpu, master_irqs[channel]);
        expect(state, dspic33_i2c_respond(cpu, channel, 0u, true, byte + 5u),
               "queue stretched acknowledge");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x61u);
        expect(state, dspic33_device_advance(cpu, byte), "advance to clock stretch");
        expect(state,
               !interrupt_flag(cpu, master_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4000u) != 0u,
               "clock stretch delays completion");
        expect(state, dspic33_device_advance(cpu, 5u), "finish clock stretch");
        expect(state,
               interrupt_flag(cpu, master_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0xc000u) == 0u,
               "stretched acknowledge completes");

        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 1u, 0u);
        expect(state, dspic33_device_advance(cpu, control), "ordered start complete");
        clear_interrupt(cpu, master_irqs[channel]);
        expect(state,
               dspic33_i2c_respond(cpu, channel, 0x11u, true, 20u) &&
                   dspic33_i2c_respond(cpu, channel, 0x22u, true, 10u),
               "queue responses out of order");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive),
               "ordered first receive complete");
        expect(state, dspic33_read_word(cpu, base) == 0x22u,
               "earliest response selected");
        clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive),
               "ordered second response delay");
        expect(state, dspic33_read_word(cpu, base) == 0x11u, "later response selected");
    }
}

static void slave_receive_cases(I2cConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_write_word(cpu, (uint16_t)(base + 12u), 0x01u);
        enable(cpu, channel, 0x0040u, 0u);
        expect(state, dspic33_i2c_slave_start(cpu, channel, 0x53u, false, false, 3u),
               "schedule masked slave address");
        expect(state, dspic33_device_advance(cpu, 2u), "slave address pre-boundary");
        expect(state, !interrupt_flag(cpu, slave_irqs[channel]),
               "slave address not early");
        expect(state, dspic33_device_advance(cpu, 1u), "slave address complete");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]),
               "slave address interrupt");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x003eu) == 0x000au,
               "slave address status");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "slave receive stretch");
        expect(state, dspic33_read_byte(cpu, base) == 0x00a6u,
               "slave address receive value");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "byte receive read clears full");
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x38u, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "queue data during stretch");
        expect(state, !interrupt_flag(cpu, slave_irqs[channel]),
               "stretch delays slave data");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9040u);
        expect(state, dspic33_device_advance(cpu, 1u), "slave data receive");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]), "slave data interrupt");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0022u) == 0x0022u,
               "slave data status");
        expect(state, dspic33_read_word(cpu, base) == 0x0038u, "slave data value");
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "slave stop event");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x033cu) == 0x0030u,
               "slave stop status");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "slave address without receive stretch");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "slave address always stretches");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x44u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]),
               "slave address hold delays data");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state, dspic33_device_advance(cpu, 1u), "slave address hold release");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, base) == 0x0044u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "slave data does not stretch when disabled");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x51u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "mismatched address event");
        expect(state, !interrupt_flag(cpu, slave_irqs[channel]),
               "mismatched address ignored");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 8u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "mismatched start does not stretch");
    }
}

static void slave_transmit_cases(I2cConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t channel_bit = (uint8_t)(1u << channel);
        Dspic33I2cTransfer transfer;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x31u);
        enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x31u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "slave read address");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x000eu) == 0x000eu,
               "slave read address status");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "slave transmitter automatic stretch");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x7du);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 1u) != 0u,
               "slave transmit full");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, true, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "slave transmit acknowledged");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]),
               "slave transmit interrupt");
        expect(state,
               (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) != 0u,
               "slave acknowledge retains transmit state");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "slave acknowledge stretches next byte");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_WRITE && transfer.value == 0x7du &&
                   transfer.acknowledge && !transfer.master,
               "slave transmit output");
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x82u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, false, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "slave transmit not acknowledged");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x8000u) != 0u,
               "slave nack status");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.value == 0x82u && !transfer.acknowledge,
               "slave nack output");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]),
               "slave nack interrupts");
        expect(state,
               (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "slave nack resets transmit state");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "slave nack does not stretch");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x31u);
        enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x31u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "slave stop read address");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x49u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, true, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "slave stop read data");
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "slave stop read event");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x033cu) == 0x0034u,
               "slave stop preserves read and data status");
    }
}

static void address_mode_cases(I2cConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t channel_bit = (uint8_t)(1u << channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        enable(cpu, channel, 0x0440u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit first address event");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit first address interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) == 0u,
               "ten bit first address status");
        expect(state, dspic33_read_word(cpu, base) == 0x00f4u,
               "ten bit first address receive");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "ten bit first address stretches");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9440u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "ten bit second address clock released");
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state, dspic33_device_advance(cpu, 1u), "ten bit second address event");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit second address interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u,
               "ten bit second address status");
        expect(state, dspic33_read_word(cpu, base) == 0x00abu,
               "ten bit second address receive");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "ten bit second address stretches");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9440u);
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, true, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit repeated read event");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit repeated read interrupt");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x010cu) == 0x010cu,
               "ten bit repeated read status");
        expect(state, dspic33_read_word(cpu, base) == 0x00f5u,
               "ten bit repeated read receive");
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x63u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, false, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "ten bit slave nack event");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x8000u) != 0u,
               "ten bit slave nack omits interrupt");
        expect(state,
               (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "ten bit slave nack resets transmit state");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit address without receive stretch");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "ten bit first address always stretches");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit second address waits for release");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state, dspic33_device_advance(cpu, 1u),
               "ten bit second address after release");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u &&
                   dspic33_read_word(cpu, base) == 0x00abu,
               "ten bit second address always stretches");

        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 0x0080u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "general call event");
        expect(state, interrupt_flag(cpu, slave_irqs[channel]),
               "general call interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0200u) != 0u,
               "general call status");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x12u);
        enable(cpu, channel, 0x0c80u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x67u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi address event");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u &&
                   dspic33_read_word(cpu, base) == 0x00ceu,
               "ipmi accepts all addresses");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x0155u);
        enable(cpu, channel, 0x0080u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "non-ipmi ten bit preamble without address mode");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u,
               "non-ipmi address mode controls preamble matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x0155u);
        enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "non-ipmi mismatched ten bit preamble");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u,
               "non-ipmi address controls preamble matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x0155u);
        enable(cpu, channel, 0x0c00u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi ten bit preamble with address mode");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00f4u,
               "ipmi bypasses enabled ten bit address matching");
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9c00u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00abu,
               "ipmi bypasses enabled ten bit address low matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x0155u);
        enable(cpu, channel, 0x0880u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi ten bit preamble event");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u &&
                   dspic33_read_word(cpu, base) == 0x00f4u,
               "ipmi accepts ten bit preamble");
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9880u);
        expect(state, dspic33_device_advance(cpu, 1u), "ipmi ten bit address event");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u &&
                   dspic33_read_word(cpu, base) == 0x00abu,
               "ipmi ten bit address stretches");
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9880u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, true, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi ten bit read event");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4107u) ==
                       0x0106u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u &&
                   dspic33_read_word(cpu, base) == 0x00f5u,
               "ipmi ten bit read aborts without stretching");
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x45u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "ipmi ten bit read aborts slave transfer");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x12u);
        enable(cpu, channel, 0x0c80u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x67u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi read address event");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4007u) ==
                       0x0006u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u &&
                   dspic33_read_word(cpu, base) == 0x00cfu,
               "ipmi read interrupts without stretching");
        clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x44u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   dspic33_read_word(cpu, base) == 0x00cfu,
               "ipmi read aborts slave transfer");
    }
}

static void address_rejection_cases(I2cConformance* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t channel_bit = (uint8_t)(1u << channel);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) != 0u,
               "seven bit read control before mismatch");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x51u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "reject seven bit mismatch");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "seven bit mismatch enters rejected state");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x44u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "ignore data after seven bit mismatch");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore seven bit restart after mismatch");
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "seven bit stop clears rejection");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "seven bit stop restores matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x01abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "reject ten bit high mismatch");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "ten bit high mismatch enters rejected state");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x45u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "ignore data after ten bit high mismatch");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit restart after high mismatch");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit low bytes after high mismatch");
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit high stop clears rejection");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u,
               "ten bit stop restores high matching");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00abu &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit stop restores complete matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02acu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit low mismatch first address");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u,
               "ten bit matching high byte remains active");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]),
               "reject ten bit low mismatch");
        expect(state,
               (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "ten bit low mismatch enters rejected state");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x46u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "ignore data after ten bit low mismatch");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit restart after low mismatch");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit low byte after low mismatch");
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit low stop clears rejection");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u,
               "ten bit stop restores low matching");
        dspic33_read_word(cpu, base);
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00abu &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit low stop restores complete matching");
    }
}

static void isolation_and_power_cases(I2cConformance* state, Dspic33* cpu) {
    static const uint16_t pmd_addresses[DSPIC33_I2C_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t pmd_masks[DSPIC33_I2C_COUNT] = {0x0080u, 0x0002u};
    Dspic33I2cTransfer transfer;
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    enable(cpu, 0u, 1u, 0u);
    enable(cpu, 1u, 1u, 9u);
    expect(state, dspic33_device_advance(cpu, control_cycles(0u)),
           "first channel independent completion");
    expect(state,
           interrupt_flag(cpu, master_irqs[0]) && !interrupt_flag(cpu, master_irqs[1]),
           "first channel independent interrupt");
    expect(state, dspic33_device_advance(cpu, control_cycles(9u) - control_cycles(0u)),
           "second channel independent completion");
    expect(state, interrupt_flag(cpu, master_irqs[1]),
           "second channel independent interrupt");
    expect(state,
           dspic33_i2c_transmit(cpu, 0u, &transfer) &&
               transfer.type == DSPIC33_I2C_START,
           "first channel independent output");
    expect(state,
           dspic33_i2c_transmit(cpu, 1u, &transfer) &&
               transfer.type == DSPIC33_I2C_START,
           "second channel independent output");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0760u, 0x0080u);
    dspic33_write_word(cpu, 0x0206u, 0x8001u);
    expect(state, dspic33_read_word(cpu, 0x0206u) == 0x1000u,
           "disabled first module ignores writes");
    dspic33_write_word(cpu, 0x0764u, 0x0002u);
    dspic33_write_word(cpu, 0x0216u, 0x8001u);
    expect(state, dspic33_read_word(cpu, 0x0216u) == 0x1000u,
           "disabled second module ignores writes");

    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t control = (uint16_t)(bases[channel] + 6u);
        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 0u, 0u);
        dspic33_write_word(cpu, control, 0x8000u);
        expect(state, dspic33_read_word(cpu, control) == 0x9000u,
               "software cannot clear release without stretch enable");
        dspic33_write_word(cpu, control, 0x8040u);
        expect(state, dspic33_read_word(cpu, control) == 0x8040u,
               "software clears release with stretch enable");
        dspic33_write_word(cpu, control, 0u);
        expect(state, dspic33_read_word(cpu, control) == 0x1000u,
               "module disable releases clock");
    }
    dspic33_reset(cpu, 0u);
    enable(cpu, 0u, 0u, 0u);
    dspic33_write_byte(cpu, 0x0203u, 0x5au);
    expect(state, (dspic33_read_word(cpu, 0x0208u) & 0x4001u) == 0u,
           "transmit high byte ignored");
    dspic33_write_byte(cpu, 0x0202u, 0x5au);
    expect(state, (dspic33_read_word(cpu, 0x0208u) & 0x4001u) == 0x4001u,
           "transmit low byte starts transfer");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0206u, 0xb001u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, dspic33_device_advance(cpu, control_cycles(0u)), "idle stop advance");
    expect(state,
           (dspic33_read_word(cpu, 0x0206u) & 1u) != 0u &&
               !interrupt_flag(cpu, master_irqs[0]),
           "idle stop freezes module");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state, dspic33_device_advance(cpu, 1u), "idle resume advance");
    expect(state,
           (dspic33_read_word(cpu, 0x0206u) & 1u) == 0u &&
               interrupt_flag(cpu, master_irqs[0]),
           "idle resume completes module");

    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint64_t total = control_cycles(9u);
        uint64_t elapsed = 4u;
        uint64_t remaining = total - elapsed;

        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 1u, 9u);
        expect(state, dspic33_device_advance(cpu, elapsed),
               "power stop master partial advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) != 0u &&
                   !interrupt_flag(cpu, master_irqs[channel]),
               "power stop master remains pending");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state, dspic33_device_advance(cpu, total + 5u),
               "power stop master disabled advance");
        expect(state,
               (stored_word(cpu, (uint16_t)(base + 6u)) & 1u) != 0u &&
                   !interrupt_flag(cpu, master_irqs[channel]),
               "power stop freezes master state");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, remaining - 1u),
               "power stop master remaining advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) != 0u &&
                   !interrupt_flag(cpu, master_irqs[channel]),
               "power stop master waits exact remainder");
        expect(state, dspic33_device_advance(cpu, 1u),
               "power stop master completion advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) == 0u &&
                   interrupt_flag(cpu, master_irqs[channel]) &&
                   dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_START,
               "power stop master completes after remainder");

        total = byte_cycles(3u);
        elapsed = 12u;
        remaining = total - elapsed;
        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 0u, 3u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x5au);
        expect(state, dspic33_device_advance(cpu, elapsed),
               "power stop transmit partial advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u &&
                   !interrupt_flag(cpu, master_irqs[channel]),
               "power stop transmit remains active");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state, dspic33_device_advance(cpu, total + 5u),
               "power stop transmit disabled advance");
        expect(state,
               (stored_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u &&
                   !interrupt_flag(cpu, master_irqs[channel]),
               "power stop freezes transmit state");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, remaining - 1u),
               "power stop transmit remaining advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4000u &&
                   !interrupt_flag(cpu, master_irqs[channel]),
               "power stop transmit waits exact remainder");
        expect(state, dspic33_device_advance(cpu, 1u),
               "power stop transmit completion advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0u &&
                   interrupt_flag(cpu, master_irqs[channel]),
               "power stop transmit completes after remainder");
    }

    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t other = (uint8_t)(channel ^ 1u);
        uint16_t other_base = bases[other];
        uint64_t delay = (uint64_t)UINT32_MAX + 9u;
        uint64_t elapsed = delay - 7u;

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_write_word(cpu, (uint16_t)(base + 12u), 1u);
        enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, delay) &&
                   dspic33_i2c_slave_stop(cpu, channel, delay + 1u) &&
                   dspic33_i2c_slave_start(cpu, channel, 0x53u, false, false,
                                           delay + 2u) &&
                   cpu->events.count == 3u,
               "power stop queues long slave sequence");
        expect(state, dspic33_device_advance(cpu, elapsed),
               "power stop slave partial advance");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        dspic33_write_word(cpu, (uint16_t)(other_base + 10u), 0x31u);
        enable(cpu, other, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, other, 0x31u, false, false, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "power stop other channel event advance");
        expect(state,
               interrupt_flag(cpu, slave_irqs[other]) &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, other_base) == 0x0062u &&
                   cpu->events.count == 3u,
               "power stop parked events do not block other channel");
        clear_interrupt(cpu, slave_irqs[other]);
        expect(state,
               dspic33_i2c_slave_stop(cpu, other, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "power stop other channel cleanup");
        expect(state, dspic33_device_advance(cpu, 19u),
               "power stop slave disabled advance");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (stored_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) == 0u &&
                   cpu->events.count == 3u,
               "power stop freezes slave sequence");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, 6u),
               "power stop slave remaining advance");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "power stop slave waits exact remainder");
        expect(state, dspic33_device_advance(cpu, 1u),
               "power stop first slave event advance");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, base) == 0x00a4u &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) != 0u,
               "power stop first slave event order");
        clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state, dspic33_device_advance(cpu, 1u), "power stop slave stop advance");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0010u) != 0u &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) == 0u,
               "power stop slave stop order");
        expect(state, dspic33_device_advance(cpu, 1u),
               "power stop second slave event advance");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, base) == 0x00a6u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "power stop second slave event order");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        enable(cpu, channel, 0u, 0u);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, delay) &&
                   cpu->events.count == 1u,
               "power stop queues long event while disabled");
        expect(state, dspic33_device_advance(cpu, 25u),
               "power stop scheduled-disabled advance");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (stored_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   cpu->events.count == 1u,
               "power stop holds event scheduled while disabled");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, delay - 1u),
               "power stop scheduled-disabled remaining advance");
        expect(state,
               !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "power stop scheduled-disabled waits exact delay");
        expect(state, dspic33_device_advance(cpu, 1u),
               "power stop scheduled-disabled completion advance");
        expect(state,
               interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, base) == 0x00a4u,
               "power stop scheduled-disabled event completes");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        enable(cpu, channel, 0u, 0u);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 1u) &&
                   cpu->events.count == 1u && cpu->events.items[0].paused,
               "power stop queues horizon event");
        expect(state, dspic33_device_advance(cpu, UINT64_MAX),
               "power stop horizon advance");
        expect(state,
               cpu->events.count == 1u && cpu->events.items[0].paused &&
                   !interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) == 0u,
               "power stop retains horizon event");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
                   cpu->events.count == 1u && cpu->events.items[0].paused &&
                   !interrupt_flag(cpu, slave_irqs[channel]),
               "power stop reports unrepresentable resume");
    }

    dspic33_reset(cpu, 0u);
    enable(cpu, 0u, 1u, 0x01ffu);
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, control_cycles(0x01ffu)),
           "reset canceled operation advance");
    expect(state, !interrupt_flag(cpu, master_irqs[0]),
           "reset cancels pending operation");
    expect(state, !dspic33_i2c_transmit(cpu, 0u, &transfer),
           "reset clears output queue");
}

static void disable_cases(I2cConformance* state, Dspic33* cpu) {
    static const uint16_t requests[] = {1u, 2u, 4u, 8u, 16u};
    uint8_t channel;
    size_t index;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        for (index = 0u; index < sizeof(requests) / sizeof(requests[0]); index++) {
            dspic33_reset(cpu, 0u);
            enable(cpu, channel, requests[index], 2u);
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & requests[index]) !=
                       0u,
                   "disable request active");
            dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
            expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0x1000u,
                   "disable clears request and releases clock");
            expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
                   "disable clears status");
            expect(state, dspic33_device_advance(cpu, receive_cycles(2u)),
                   "disabled request canceled advance");
            expect(state, !interrupt_flag(cpu, master_irqs[channel]),
                   "disabled request canceled interrupt");
            enable(cpu, channel, 1u, 2u);
            expect(state, dspic33_device_advance(cpu, control_cycles(2u)),
                   "disable clean re-enable advance");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) == 0u &&
                       interrupt_flag(cpu, master_irqs[channel]),
                   "disable clean re-enable completion");
        }

        dspic33_reset(cpu, 0u);
        enable(cpu, channel, 0u, 2u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x5au);
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u,
               "disable transmit active");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0x1000u,
               "disable transmit releases clock");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "disable transmit clears status");
        expect(state, dspic33_device_advance(cpu, byte_cycles(2u)),
               "disabled transmit canceled advance");
        expect(state, !interrupt_flag(cpu, master_irqs[channel]),
               "disabled transmit canceled interrupt");
        enable(cpu, channel, 1u, 2u);
        expect(state, dspic33_device_advance(cpu, control_cycles(2u)),
               "disable transmit clean re-enable advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) == 0u &&
                   interrupt_flag(cpu, master_irqs[channel]),
               "disable transmit clean re-enable completion");
    }
}

static void dma_isolation_cases(I2cConformance* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3000u, 0xa55au);
    configure_dma_channel(cpu, 0u, slave_irqs[1], 0x3000u, bases[1]);
    dspic33_write_word(cpu, (uint16_t)(bases[1] + 10u), 0x52u);
    enable(cpu, 1u, 0u, 2u);
    expect(state,
           dspic33_i2c_slave_start(cpu, 1u, 0x52u, false, false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "slave event advance without dma");
    expect(state, dspic33_read_word(cpu, 0x3000u) == 0xa55au,
           "slave event does not request dma");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3000u, 0x5aa5u);
    configure_dma_channel(cpu, 0u, master_irqs[1], 0x3000u, bases[1]);
    enable(cpu, 1u, 1u, 2u);
    expect(state, dspic33_device_advance(cpu, control_cycles(2u)),
           "master event advance without dma");
    expect(state, dspic33_read_word(cpu, 0x3000u) == 0x5aa5u,
           "master event does not request dma");
}

int main(void) {
    Dspic33 cpu;
    I2cConformance state = {0u, 0u, 0u};
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    register_cases(&state, &cpu);
    timing_cases(&state, &cpu);
    master_sequence_cases(&state, &cpu);
    master_error_cases(&state, &cpu);
    slave_receive_cases(&state, &cpu);
    slave_transmit_cases(&state, &cpu);
    address_mode_cases(&state, &cpu);
    address_rejection_cases(&state, &cpu);
    disable_cases(&state, &cpu);
    isolation_and_power_cases(&state, &cpu);
    dma_isolation_cases(&state, &cpu);
    dspic33_destroy(&cpu);
    printf("[i2c-summary] cases=%" PRIu32 " passed=%" PRIu32 " failed=%" PRIu32 "\n",
           state.cases, state.passed, state.failed);
    return state.failed == 0u ? 0 : 1;
}
