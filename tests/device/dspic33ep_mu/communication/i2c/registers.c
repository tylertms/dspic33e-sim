#include "device/dspic33ep_mu/communication/i2c/internal.h"

void dspic33_device_internal_raw_write_word(Dspic33* cpu, uint16_t address, uint16_t value);

bool dspic33_i2c_test_interrupt_flag(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    return (dspic33_read_word(cpu, address) & (uint16_t)(1u << (irq % 16u))) != 0u;
}

void dspic33_i2c_test_clear_interrupt(Dspic33* cpu, uint8_t irq) {
    uint16_t address = (uint16_t)(0x0800u + (irq / 16u) * 2u);
    dspic33_write_word(
        cpu, address, (uint16_t)(dspic33_read_word(cpu, address) & ~(uint16_t)(1u << (irq % 16u))));
}

void dspic33_i2c_test_enable_interrupt(Dspic33* cpu, uint8_t irq, uint8_t priority,
                                       uint16_t vector) {
    uint16_t enable = (uint16_t)(0x0820u + (irq / 16u) * 2u);
    uint16_t irq_mask = (uint16_t)(1u << (irq % 16u));
    uint16_t ipc = (uint16_t)(0x0840u + (irq / 4u) * 2u);
    uint16_t shift = (uint16_t)((irq % 4u) * 4u);
    dspic33_write_word(cpu, enable, (uint16_t)(dspic33_read_word(cpu, enable) | irq_mask));
    dspic33_write_word(cpu, ipc,
                       (uint16_t)((dspic33_read_word(cpu, ipc) & ~(uint16_t)(7u << shift)) |
                                  (uint16_t)(priority << shift)));
    cpu->program[(0x0014u + irq * 2u) / 2u] = vector;
}

uint16_t dspic33_i2c_test_stored_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

uint64_t dspic33_i2c_test_operation_cycles(uint16_t baud, uint8_t half_periods) {
    return ((uint64_t)(baud + 2u) * half_periods + 1u) / 2u;
}

uint64_t dspic33_i2c_test_control_cycles(uint16_t baud) {
    return dspic33_i2c_test_operation_cycles(baud, 2u);
}

uint64_t dspic33_i2c_test_byte_cycles(uint16_t baud) {
    return dspic33_i2c_test_operation_cycles(baud, 18u);
}

uint64_t dspic33_i2c_test_receive_cycles(uint16_t baud) {
    return dspic33_i2c_test_operation_cycles(baud, 16u);
}

static uint64_t condition_cycles(uint16_t baud) {
    return dspic33_i2c_test_operation_cycles(baud, 3u);
}

void dspic33_i2c_test_configure_dma_channel(Dspic33* cpu, uint8_t channel, uint8_t request,
                                            uint16_t start, uint16_t pad) {
    uint16_t channel_base = (uint16_t)(0x0b00u + channel * 0x10u);
    dspic33_write_word(cpu, channel_base, 0u);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 4u), start);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 6u), 0u);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 8u), 0u);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 10u), 0u);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 12u), pad);
    dspic33_write_word(cpu, (uint16_t)(channel_base + 14u), 0u);
    dspic33_write_word(cpu, channel_base, 0x8001u);
}

void dspic33_i2c_test_enable(Dspic33* cpu, uint8_t channel, uint16_t options, uint16_t baud_rate) {
    uint16_t register_base = bases[channel];
    dspic33_write_word(cpu, (uint16_t)(register_base + 4u), baud_rate);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), (uint16_t)(0x9000u | options));
}

bool dspic33_i2c_test_pop_slave_acknowledgement(Dspic33* cpu, uint8_t channel, bool acknowledge) {
    Dspic33I2cTransfer transfer;
    return dspic33_i2c_transmit(cpu, channel, &transfer) &&
           transfer.type == DSPIC33_I2C_ACKNOWLEDGE && transfer.acknowledge == acknowledge &&
           !transfer.master;
}

bool dspic33_i2c_test_pin_levels(const Dspic33* cpu, uint8_t port, uint8_t clock, uint8_t data,
                                 bool clock_high, bool data_high) {
    bool is_high;
    return dspic33_i2c_pin(cpu, port, clock, &is_high) && is_high == clock_high &&
           dspic33_i2c_pin(cpu, port, data, &is_high) && is_high == data_high;
}

void dspic33_i2c_test_drive_pin(const I2cPinRoute* route, Dspic33* cpu, bool is_clock_pin,
                                bool is_high) {
    uint16_t pin_mask = (uint16_t)(1u << (is_clock_pin ? route->clock : route->data));
    dspic33_gpio_drive(cpu, route->port, is_high ? pin_mask : 0u, pin_mask);
}

void dspic33_i2c_test_drive_byte(const I2cPinRoute* route, Dspic33* cpu, uint8_t byte_value) {
    uint8_t bit_index;
    for (bit_index = 0u; bit_index < 8u; bit_index++) {
        dspic33_i2c_test_drive_pin(route, cpu, true, false);
        dspic33_i2c_test_drive_pin(route, cpu, false,
                                   (byte_value & (uint8_t)(0x80u >> bit_index)) != 0u);
        dspic33_i2c_test_drive_pin(route, cpu, true, true);
    }
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
}

void dspic33_i2c_test_register_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t register_base = bases[channel];
        expect(state, dspic33_read_word(cpu, register_base) == 0u, "receive reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 2u)) == 0u,
               "transmit reads zero");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 4u)) == 0u, "baud reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) == 0x1000u,
               "control reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) == 0u, "status reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 10u)) == 0u,
               "address reset");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 12u)) == 0u, "mask reset");
        dspic33_write_word(cpu, register_base, 0xffffu);
        expect(state, dspic33_read_word(cpu, register_base) == 0u, "receive read only");
        dspic33_write_word(cpu, (uint16_t)(register_base + 4u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 4u)) == 0x01ffu,
               "baud mask");
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0xbfe0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) == 0xbfe0u,
               "control mask");
        dspic33_write_word(cpu, (uint16_t)(register_base + 8u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) == 0u,
               "status clear-only mask");
        expect(state, dspic33_i2c_status(cpu, channel, 0x84c0u),
               "hardware status injection accepts defined bits");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) == 0x84c0u,
               "hardware status injection updates defined bits");
        expect(state, dspic33_i2c_status(cpu, channel, 0x0040u),
               "hardware status injection replaces prior bits");
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) == 0x0040u,
               "hardware status injection preserves replacement");
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 10u)) == 0x03ffu,
               "address mask");
        dspic33_write_word(cpu, (uint16_t)(register_base + 12u), 0xffffu);
        expect(state, dspic33_read_word(cpu, (uint16_t)(register_base + 12u)) == 0x03ffu,
               "slave mask mask");
    }
    expect(state, !dspic33_i2c_status(cpu, DSPIC33_I2C_COUNT, 0u),
           "hardware status injection rejects invalid channel");
    expect(state, !dspic33_i2c_status(cpu, 0u, 0x0001u),
           "hardware status injection rejects software-owned bits");
}

void dspic33_i2c_test_timing_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t baud_values[] = {2u, 3u, 17u, 0x01ffu};
    uint8_t channel;
    size_t baud_index;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        for (baud_index = 0u; baud_index < sizeof(baud_values) / sizeof(baud_values[0]);
             baud_index++) {
            uint16_t register_base = bases[channel];
            uint16_t baud_rate = baud_values[baud_index];
            uint64_t cycles = dspic33_i2c_test_control_cycles(baud_rate);
            Dspic33I2cTransfer transfer;
            dspic33_reset(cpu, 0u);
            dspic33_i2c_test_enable(cpu, channel, 1u, baud_rate);
            expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 1u) != 0u,
                   "start active at write");
            expect(state, dspic33_device_advance(cpu, cycles - 1u), "start boundary advance");
            expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
                   "start not complete early");
            expect(state, dspic33_device_advance(cpu, 1u), "start completion advance");
            expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 1u) == 0u,
                   "start clears request");
            expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 8u) != 0u,
                   "start sets bus state");
            expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
                   "start interrupt");
            if (channel == 1u) {
                expect(state, !dspic33_i2c_test_interrupt_flag(cpu, 51u),
                       "second module does not raise timer eight interrupt");
            }
            expect(state,
                   dspic33_i2c_transmit(cpu, channel, &transfer) &&
                       transfer.type == DSPIC33_I2C_START && transfer.master,
                   "start output");
        }
    }
}
void dspic33_i2c_test_bus_status_timing_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t baud = (uint16_t)(4u + channel * 2u);
        uint64_t start_status = dspic33_i2c_test_operation_cycles(baud, 1u);
        uint64_t start_complete = dspic33_i2c_test_operation_cycles(baud, 2u);
        uint64_t condition_status = dspic33_i2c_test_operation_cycles(baud, 2u);
        uint64_t condition_complete = dspic33_i2c_test_operation_cycles(baud, 3u);

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 1u, baud);
        expect(state, dspic33_device_advance(cpu, start_status - 1u),
               "start status boundary advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0u,
               "start status unchanged early");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0001u) != 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "start request active before status edge");
        expect(state, dspic33_device_advance(cpu, 1u), "start status edge advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0008u,
               "start status edge");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0001u) != 0u,
               "start request remains active at status edge");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "start interrupt remains clear at status edge");
        expect(state, dspic33_device_advance(cpu, start_complete - start_status),
               "start final period advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0001u) == 0u,
               "start request clears after final period");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "start interrupt after final period");

        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9004u);
        expect(state, dspic33_device_advance(cpu, condition_status - 1u),
               "stop status boundary advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0008u,
               "stop status unchanged early");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0004u) != 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "stop request active before status edge");
        expect(state, dspic33_device_advance(cpu, 1u), "stop status edge advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0010u,
               "stop status edge");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0004u) != 0u,
               "stop request remains active at status edge");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "stop interrupt remains clear at status edge");
        expect(state, dspic33_device_advance(cpu, condition_complete - condition_status),
               "stop final period advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0004u) == 0u,
               "stop request clears after final period");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "stop interrupt after final period");

        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9002u);
        expect(state, dspic33_device_advance(cpu, condition_status - 1u),
               "restart status boundary advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0010u,
               "restart status unchanged early");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0002u) != 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "restart request active before status edge");
        expect(state, dspic33_device_advance(cpu, 1u), "restart status edge advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0008u,
               "restart status edge");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0002u) != 0u,
               "restart request remains active at status edge");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "restart interrupt remains clear at status edge");
        expect(state, dspic33_device_advance(cpu, condition_complete - condition_status),
               "restart final period advance");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x0002u) == 0u,
               "restart request clears after final period");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "restart interrupt after final period");
    }
}

void dspic33_i2c_test_master_sequence_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t baud = (uint16_t)(3u + channel);
        uint64_t control = dspic33_i2c_test_control_cycles(baud);
        uint64_t receive = dspic33_i2c_test_receive_cycles(baud);
        uint64_t condition = condition_cycles(baud);
        uint64_t byte = dspic33_i2c_test_byte_cycles(baud);
        Dspic33I2cTransfer transfer;
        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 1u, baud);
        expect(state, dspic33_device_advance(cpu, control), "sequence start");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        expect(state, dspic33_i2c_respond(cpu, channel, 0u, true, byte),
               "queue transmit acknowledge");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x01a5u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u,
               "transmit busy flags");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x005au);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0080u) != 0u,
               "transmit write collision");
        expect(state, dspic33_device_advance(cpu, receive), "transmit shift completion");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4000u,
               "transmit buffer clears before acknowledge");
        expect(state, dspic33_device_advance(cpu, byte - receive - 1u),
               "transmit boundary advance");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "transmit not complete early");
        expect(state, dspic33_device_advance(cpu, 1u), "transmit complete");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0xc001u) == 0u,
               "transmit acknowledged status");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "transmit interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) && transfer.type == DSPIC33_I2C_START,
               "sequence start output");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_WRITE && transfer.value == 0xa5u,
               "sequence write output");

        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
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
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "receive interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) && transfer.type == DSPIC33_I2C_READ &&
                   transfer.value == 0x6cu,
               "receive output");

        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9030u);
        expect(state, dspic33_device_advance(cpu, control), "nack complete");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]), "nack interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_ACKNOWLEDGE && !transfer.acknowledge,
               "nack output");

        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9002u);
        expect(state, dspic33_device_advance(cpu, condition), "restart complete");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_RESTART,
               "restart output");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9004u);
        expect(state, dspic33_device_advance(cpu, condition), "stop complete");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0010u,
               "stop bus state");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]), "stop interrupt");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) && transfer.type == DSPIC33_I2C_STOP,
               "stop output");
    }
}

void dspic33_i2c_test_master_error_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint64_t control = dspic33_i2c_test_control_cycles(0u);
        uint64_t receive = dspic33_i2c_test_receive_cycles(0u);
        uint64_t byte = dspic33_i2c_test_byte_cycles(0u);
        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 1u, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x44u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0080u) != 0u,
               "write during start collision");
        expect(state, dspic33_device_advance(cpu, control), "error start complete");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x55u);
        expect(state, dspic33_device_advance(cpu, byte), "unacknowledged complete");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x8000u) != 0u,
               "unacknowledged status");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive), "first unread receive");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive), "second unread receive");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0042u) == 0x0042u,
               "receive overflow preserves full");
        expect(state, dspic33_i2c_collision(cpu, channel, 0u) && dspic33_device_advance(cpu, 0u),
               "collision event");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) != 0u,
               "bus collision status");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "bus collision interrupt");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0xffffu);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) != 0u,
               "bus collision not software settable");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) == 0u,
               "bus collision software clear");

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 1u, 0u);
        expect(state, dspic33_device_advance(cpu, control), "stretch start complete");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        expect(state, dspic33_i2c_respond(cpu, channel, 0u, true, byte + 5u),
               "queue stretched acknowledge");
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x61u);
        expect(state, dspic33_device_advance(cpu, byte), "advance to clock stretch");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4000u) != 0u,
               "clock stretch delays completion");
        expect(state, dspic33_device_advance(cpu, 5u), "finish clock stretch");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0xc000u) == 0u,
               "stretched acknowledge completes");

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 1u, 0u);
        expect(state, dspic33_device_advance(cpu, control), "ordered start complete");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        expect(state,
               dspic33_i2c_respond(cpu, channel, 0x11u, true, 20u) &&
                   dspic33_i2c_respond(cpu, channel, 0x22u, true, 10u),
               "queue responses out of order");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive), "ordered first receive complete");
        expect(state, dspic33_read_word(cpu, base) == 0x22u, "earliest response selected");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(state, dspic33_device_advance(cpu, receive), "ordered second response delay");
        expect(state, dspic33_read_word(cpu, base) == 0x11u, "later response selected");
    }
}

void dspic33_i2c_test_slave_receive_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_write_word(cpu, (uint16_t)(base + 12u), 0x01u);
        dspic33_i2c_test_enable(cpu, channel, 0x0040u, 0u);
        expect(state, dspic33_i2c_slave_start(cpu, channel, 0x53u, false, false, 3u),
               "schedule masked slave address");
        expect(state, dspic33_device_advance(cpu, 2u), "slave address pre-boundary");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "slave address not early");
        expect(state, dspic33_device_advance(cpu, 1u), "slave address complete");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "slave address interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x003eu) == 0x000au,
               "slave address status");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "slave receive stretch");
        expect(state, dspic33_read_byte(cpu, base) == 0x00a6u, "slave address receive value");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "byte receive read clears full");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x38u, 0u) && dspic33_device_advance(cpu, 0u),
               "queue data during stretch");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "stretch delays slave data");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9040u);
        expect(state, dspic33_device_advance(cpu, 1u), "slave data receive");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "slave data interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0022u) == 0x0022u,
               "slave data status");
        expect(state, dspic33_read_word(cpu, base) == 0x0038u, "slave data value");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state, dspic33_i2c_slave_stop(cpu, channel, 1u) && dspic33_device_advance(cpu, 1u),
               "slave stop event");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x033cu) == 0x0030u,
               "slave stop status");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "slave address without receive stretch");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "slave address always stretches");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x44u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "slave address hold delays data");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state, dspic33_device_advance(cpu, 1u), "slave address hold release");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, base) == 0x0044u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "slave data does not stretch when disabled");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x51u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "mismatched address event");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "mismatched address ignored");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 8u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "mismatched start does not stretch");
    }
}

void dspic33_i2c_test_slave_transmit_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t channel_bit = (uint8_t)(1u << channel);
        Dspic33I2cTransfer transfer;
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x31u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x31u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "slave read address");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x000eu) == 0x000eu,
               "slave read address status");
        expect(state, dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "slave read address acknowledgement");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) == 0u,
               "slave transmitter automatic stretch");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x7du);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 1u) != 0u,
               "slave transmit full");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, true, 1u) && dspic33_device_advance(cpu, 1u),
               "slave transmit acknowledged");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
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
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x82u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, false, 1u) && dspic33_device_advance(cpu, 1u),
               "slave transmit not acknowledged");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x8000u) != 0u,
               "slave nack status");
        expect(state,
               dspic33_i2c_transmit(cpu, channel, &transfer) && transfer.value == 0x82u &&
                   !transfer.acknowledge,
               "slave nack output");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "slave nack interrupts");
        expect(state,
               (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "slave nack resets transmit state");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "slave nack does not stretch");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x31u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x31u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "slave stop read address");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x49u);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, true, 1u) && dspic33_device_advance(cpu, 1u),
               "slave stop read data");
        expect(state, dspic33_i2c_slave_stop(cpu, channel, 1u) && dspic33_device_advance(cpu, 1u),
               "slave stop read event");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x033cu) == 0x0034u,
               "slave stop preserves read and data status");
    }
}

void dspic33_i2c_test_slave_acknowledgement_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "matched address acknowledgement output");
        expect(state, !dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "matched address emits one acknowledgement");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x51u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false),
               "mismatched address negative acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0x0080u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "general call acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 12u), 0x007fu);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "disabled general call negative acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 6u), 0x9800u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "IPMI general call acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 6u), 0x9800u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "IPMI accepts a ten-bit high address");
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 6u), 0x9800u);
        bool ipmi_advanced = dspic33_device_advance(cpu, 1u);
        expect(state,
               ipmi_advanced && cpu->io.i2c_tx[channel].count == 1u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0142u) == 0x0142u,
               "IPMI ten-bit continuation records unread overflow");

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_device_internal_raw_write_word(cpu, (uint16_t)(base + 6u), 0x9800u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 0x1000u) != 0u,
               "IPMI read releases the slave clock");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0040u) != 0u,
               "repeated unread slave address sets overflow");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit high address acknowledgement output");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit low address acknowledgement output");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, true, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit repeated read acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit full restart high acknowledgement output");
        dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit full restart low acknowledgement output");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, true, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x014au) == 0x014au &&
                   dspic33_read_word(cpu, base) == 0x00abu,
               "ten bit repeated read preserves full data and records overflow");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit buffered high acknowledgement output");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0142u) == 0x0142u,
               "ten bit buffered low negative acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit reconfigured high acknowledgement output");
        dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit reconfigured low acknowledgement output");
        dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x01abu);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, true, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false) &&
                   (cpu->io.i2c_slave_rejected & (uint8_t)(1u << channel)) != 0u,
               "ten bit reconfigured repeated read negative acknowledgement");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x01abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) && cpu->events.count == 0u &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false),
               "ten bit high mismatch negative acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02acu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "ten bit low mismatch high acknowledgement output");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) && cpu->events.count == 0u &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false),
               "ten bit low mismatch negative acknowledgement output");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "first buffered address acknowledgement output");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0042u) == 0x0042u,
               "full receive buffer address negative acknowledgement");
        dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "cleared receive buffer address acknowledgement recovery");
        dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x31u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "received data acknowledgement output");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x42u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false),
               "full receive buffer data negative acknowledgement");
        expect(state, dspic33_read_word(cpu, base) == 0x0031u,
               "overflow retains prior received data");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x53u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, false) &&
                   dspic33_read_word(cpu, base) == 0x0053u,
               "overflow status data negative acknowledgement");
        dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x64u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true),
               "cleared overflow data acknowledgement recovery");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        cpu->io.i2c_tx[channel].count = DSPIC33_I2C_QUEUE_SIZE;
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   !dspic33_device_advance(cpu, 0u) &&
                   cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR &&
                   cpu->io.i2c_tx[channel].count == DSPIC33_I2C_QUEUE_SIZE,
               "slave acknowledgement output queue failure");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        cpu->device_cycles = UINT64_MAX;
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   !dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_pop_slave_acknowledgement(cpu, channel, true) &&
                   cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u,
               "ten bit second address scheduling failure");
    }
}
