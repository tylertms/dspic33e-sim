#include "device/dspic33ep_mu/communication/i2c/internal.h"

void dspic33_i2c_test_slave_power_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint16_t vector = (uint16_t)(0x0240u + channel * 0x20u);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_i2c_test_enable_interrupt(cpu, slave_irqs[channel], 3u, vector);
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x51u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "sleep unmatched slave event advances");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   !dspic33_device_wake(cpu) && cpu->power_state == DSPIC33_POWER_SLEEP,
               "sleep unmatched slave event cannot wake");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_i2c_test_enable_interrupt(cpu, slave_irqs[channel], 3u, vector);
        cpu->w[15] = 0x1800u;
        cpu->power_state = DSPIC33_POWER_SLEEP;
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "sleep matched slave event advances");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00a4u,
               "sleep matched slave event receives address");
        expect(state,
               dspic33_device_advance(cpu, 1u) && dspic33_device_wake(cpu) &&
                   cpu->last_interrupt == slave_irqs[channel] && cpu->pc == vector &&
                   cpu->w[15] == 0x1804u,
               "sleep matched slave event wakes through vector");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        cpu->power_state = DSPIC33_POWER_IDLE;
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "idle-running slave event advances");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00a4u,
               "I2CSIDL clear continues slave operation in Idle");
    }
}

void dspic33_i2c_test_disable_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t requests[] = {1u, 2u, 4u, 8u, 16u};
    uint8_t channel;
    size_t index;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        for (index = 0u; index < sizeof(requests) / sizeof(requests[0]); index++) {
            dspic33_reset(cpu, 0u);
            dspic33_i2c_test_enable(cpu, channel, requests[index], 2u);
            expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & requests[index]) != 0u,
                   "disable request active");
            dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
            expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0x1000u,
                   "disable clears request and releases clock");
            expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
                   "disable clears status");
            expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_receive_cycles(2u)),
                   "disabled request canceled advance");
            expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
                   "disabled request canceled interrupt");
            dspic33_i2c_test_enable(cpu, channel, 1u, 2u);
            expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u)),
                   "disable clean re-enable advance");
            expect(state,
                   (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) == 0u &&
                       dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
                   "disable clean re-enable completion");
        }

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 2u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x5au);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u,
               "disable transmit active");
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0u);
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 6u)) == 0x1000u,
               "disable transmit releases clock");
        expect(state, dspic33_read_word(cpu, (uint16_t)(base + 8u)) == 0u,
               "disable transmit clears status");
        expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_byte_cycles(2u)),
               "disabled transmit canceled advance");
        expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "disabled transmit canceled interrupt");
        dspic33_i2c_test_enable(cpu, channel, 1u, 2u);
        expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u)),
               "disable transmit clean re-enable advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) == 0u &&
                   dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "disable transmit clean re-enable completion");
    }
}

void dspic33_i2c_test_dma_isolation_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3000u, 0xa55au);
    dspic33_i2c_test_configure_dma_channel(cpu, 0u, slave_irqs[1], 0x3000u, bases[1]);
    dspic33_write_word(cpu, (uint16_t)(bases[1] + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 2u);
    expect(state,
           dspic33_i2c_slave_start(cpu, 1u, 0x52u, false, false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "slave event advance without dma");
    expect(state, dspic33_read_word(cpu, 0x3000u) == 0xa55au, "slave event does not request dma");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x3000u, 0x5aa5u);
    dspic33_i2c_test_configure_dma_channel(cpu, 0u, master_irqs[1], 0x3000u, bases[1]);
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u)),
           "master event advance without dma");
    expect(state, dspic33_read_word(cpu, 0x3000u) == 0x5aa5u, "master event does not request dma");
}

void dspic33_i2c_test_pin_routing_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool copy_high;
    bool high;
    bool initialized;

    expect(state, dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffffu),
           "load standard I2C pin configuration");
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 5u, 0x0030u, 0x0030u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 2u);
    expect(state,
           dspic33_i2c_pin(cpu, 5u, 5u, &high) && high && dspic33_i2c_pin(cpu, 5u, 4u, &high) &&
               high,
           "standard I2C2 pins are selected and released");
    dspic33_write_word(cpu, 0x0e50u, (uint16_t)(dspic33_read_word(cpu, 0x0e50u) & ~0x0030u));
    dspic33_write_word(cpu, 0x0e54u, (uint16_t)(dspic33_read_word(cpu, 0x0e54u) & ~0x0030u));
    expect(state,
           dspic33_i2c_pin(cpu, 5u, 5u, &high) && high && dspic33_i2c_pin(cpu, 5u, 4u, &high) &&
               high,
           "enabled I2C2 owns pins independently of port output state");
    expect(state, !dspic33_i2c_pin(cpu, 0u, 2u, &high) && !dspic33_i2c_pin(cpu, 0u, 3u, &high),
           "standard I2C2 leaves alternate pins under port control");
    dspic33_gpio_drive(cpu, 5u, 0u, 0x0010u);
    expect(state, dspic33_i2c_pin(cpu, 5u, 4u, &high) && !high,
           "released I2C data resolves an externally driven low");
    dspic33_gpio_drive(cpu, 5u, 0x0010u, 0x0010u);
    dspic33_write_word(cpu, (uint16_t)(bases[1] + 6u), 0x9001u);
    expect(state, dspic33_i2c_test_pin_levels(cpu, 5u, 5u, 4u, true, true),
           "active physical master start initially releases clock");
    expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u)),
           "complete pin-plane master start");
    expect(state, dspic33_i2c_test_pin_levels(cpu, 5u, 5u, 4u, false, false),
           "physical master wait state holds clock and data low");
    dspic33_write_word(cpu, (uint16_t)(bases[1] + 6u), 0u);
    expect(state,
           !dspic33_i2c_pin(cpu, 5u, 5u, &high) && dspic33_gpio_pin(cpu, 5u, 5u, &high) && !high,
           "disabled I2C2 returns standard pins to port control");

    expect(state, dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffdfu),
           "load alternate I2C2 pin configuration");
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 0u, 0x000cu, 0x000cu);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    expect(state,
           dspic33_i2c_pin(cpu, 0u, 2u, &high) && high && dspic33_i2c_pin(cpu, 0u, 3u, &high) &&
               high,
           "alternate I2C2 pins are selected and released");
    expect(state, !dspic33_i2c_pin(cpu, 5u, 5u, &high) && !dspic33_i2c_pin(cpu, 5u, 4u, &high),
           "alternate I2C2 releases standard pins");
    dspic33_write_word(cpu, (uint16_t)(bases[1] + 10u), 0x52u);
    dspic33_write_word(cpu, (uint16_t)(bases[1] + 6u), 0x9040u);
    expect(state,
           dspic33_i2c_slave_start(cpu, 1u, 0x52u, false, false, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "alternate I2C2 accepts a slave address");
    expect(state,
           dspic33_i2c_pin(cpu, 0u, 2u, &high) && !high && dspic33_i2c_pin(cpu, 0u, 3u, &high) &&
               high,
           "I2C2 slave clock stretch drives only alternate SCL low");
    dspic33_read_word(cpu, bases[1]);
    dspic33_write_word(cpu, (uint16_t)(bases[1] + 6u), 0x9040u);
    expect(state, dspic33_i2c_pin(cpu, 0u, 2u, &high) && high,
           "I2C2 SCLREL releases the alternate clock");

    expect(state, dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffefu),
           "load alternate I2C1 pin configuration");
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 3u, 0x0600u, 0x0600u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    expect(state,
           dspic33_i2c_pin(cpu, 3u, 10u, &high) && high && dspic33_i2c_pin(cpu, 3u, 9u, &high) &&
               high,
           "alternate I2C1 pins are selected and released");
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 10u), 0x52u);
    dspic33_write_word(cpu, (uint16_t)(bases[0] + 6u), 0x9040u);
    dspic33_i2c_slave_start(cpu, 0u, 0x52u, false, false, 0u);
    dspic33_device_advance(cpu, 0u);
    expect(state,
           dspic33_i2c_pin(cpu, 3u, 10u, &high) && !high && dspic33_i2c_pin(cpu, 3u, 9u, &high) &&
               high,
           "I2C1 slave clock stretch drives only alternate SCL low");

    expect(state, dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffffu),
           "restore standard I2C1 pin selection");
    dspic33_reset(cpu, 0u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    expect(state,
           !dspic33_i2c_pin(cpu, 3u, 10u, &high) && !dspic33_i2c_pin(cpu, 3u, 9u, &high) &&
               !dspic33_i2c_pin(cpu, 6u, 2u, &high) && !dspic33_i2c_pin(cpu, 6u, 3u, &high),
           "MU810 standard I2C1 selection has no bonded serial pins");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 5u, 0x0030u, 0x0030u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_write_word(cpu, 0x0764u, 0x0002u);
    expect(state, dspic33_i2c_pin(cpu, 5u, 5u, &high) && high,
           "pending I2C2 PMD transition retains pin ownership");
    expect(state, dspic33_device_advance(cpu, 1u) && !dspic33_i2c_pin(cpu, 5u, 5u, &high),
           "effective I2C2 PMD disable releases pins");
    dspic33_write_word(cpu, 0x0764u, 0u);
    expect(state, !dspic33_i2c_pin(cpu, 5u, 5u, &high),
           "pending I2C2 PMD enable leaves pins released");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_i2c_pin(cpu, 5u, 5u, &high) && high,
           "effective I2C2 PMD enable restores pin ownership");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize I2C pin-plane copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy I2C pin-plane state");
        expect(state,
               dspic33_i2c_pin(cpu, 5u, 5u, &high) && high &&
                   dspic33_i2c_pin(&copy, 5u, 5u, &copy_high) && copy_high,
               "copy preserves independent I2C pin ownership and input level");
        dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffdfu);
        expect(state,
               dspic33_i2c_pin(cpu, 0u, 2u, &high) && dspic33_i2c_pin(&copy, 5u, 5u, &copy_high),
               "copy preserves an independent I2C pin selection");
        dspic33_release(&copy);
    }

    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    cpu->pc = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "execute warm reset with alternate I2C configuration");
    expect(state,
           !dspic33_i2c_pin(cpu, 0u, 2u, &high) &&
               dspic33_read_configuration_byte(cpu, 0xf8000cu) == 0xdfu,
           "warm reset preserves selection and releases disabled I2C pins");
    dspic33_gpio_drive(cpu, 0u, 0x000cu, 0x000cu);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    expect(state, dspic33_i2c_pin(cpu, 0u, 2u, &high) && high,
           "warm-reset I2C2 re-enable uses retained alternate pins");
    expect(state,
           !dspic33_i2c_pin(cpu, DSPIC33_GPIO_PORT_COUNT, 0u, &high) &&
               !dspic33_i2c_pin(cpu, 0u, 16u, &high) && !dspic33_i2c_pin(cpu, 0u, 2u, NULL),
           "I2C pin API rejects invalid arguments");
    dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffffu);
}

void dspic33_i2c_test_master_pin_sequence_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t received_values[] = {0x3cu, 0xa6u, 0x59u};
    const uint16_t baud = 2u;
    const uint64_t half = dspic33_i2c_test_operation_cycles(baud, 1u);
    size_t route_index;

    for (route_index = 0u; route_index < sizeof(pin_routes) / sizeof(pin_routes[0]);
         route_index++) {
        const I2cPinRoute* route = &pin_routes[route_index];
        uint16_t base = bases[route->channel];
        uint16_t clock_mask = (uint16_t)(1u << route->clock);
        uint16_t data_mask = (uint16_t)(1u << route->data);
        uint8_t bit_index;

        dspic33_load_configuration_word(cpu, 0xf8000cu, route->configuration);
        dspic33_reset(cpu, 0u);
        dspic33_gpio_drive(cpu, route->port, (uint16_t)(clock_mask | data_mask),
                           (uint16_t)(clock_mask | data_mask));
        dspic33_i2c_test_enable(cpu, route->channel, 1u, baud);
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, true),
               "physical start begins from bus idle");
        expect(state, dspic33_device_advance(cpu, half - 1u),
               "physical start advances before first boundary");
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, true),
               "physical start remains idle before first boundary");
        expect(state, dspic33_device_advance(cpu, 1u), "physical start reaches data boundary");
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false),
            "physical start drives data low while clock is high");
        expect(state, dspic33_device_advance(cpu, half),
               "physical start reaches completion boundary");
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                           false) &&
                   dspic33_i2c_test_interrupt_flag(cpu, master_irqs[route->channel]),
               "physical start completes in the master wait state");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[route->channel]);

        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x00a5u);
        for (bit_index = 0u; bit_index < 8u; bit_index++) {
            bool bit_high = (0xa5u & (uint8_t)(0x80u >> bit_index)) != 0u;
            expect(state,
                   dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                               bit_high),
                   "physical transmit presents data while clock is low");
            expect(state, dspic33_device_advance(cpu, half),
                   "physical transmit reaches rising edge");
            expect(state,
                   dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true,
                                               bit_high),
                   "physical transmit holds data through clock high");
            expect(state, dspic33_device_advance(cpu, half),
                   "physical transmit reaches falling edge");
        }
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, true),
            "physical transmit releases data for acknowledgement");
        dspic33_gpio_drive(cpu, route->port, 0u, data_mask);
        expect(state, dspic33_device_advance(cpu, half),
               "physical transmit reaches acknowledgement sample");
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false),
            "physical transmit samples a driven acknowledgement");
        expect(state, dspic33_device_advance(cpu, half),
               "physical transmit completes acknowledgement");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0xc001u) == 0u &&
                   dspic33_i2c_test_interrupt_flag(cpu, master_irqs[route->channel]),
               "physical acknowledgement completes transmit without NACK");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[route->channel]);
        dspic33_gpio_drive(cpu, route->port, data_mask, data_mask);

        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9008u);
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, true),
            "physical receive begins with released data");
        for (bit_index = 0u; bit_index < 8u; bit_index++) {
            bool bit_high = (received_values[route_index] & (uint8_t)(0x80u >> bit_index)) != 0u;
            dspic33_gpio_drive(cpu, route->port, bit_high ? data_mask : 0u, data_mask);
            expect(state, dspic33_device_advance(cpu, half),
                   "physical receive reaches sampling edge");
            expect(state,
                   dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true,
                                               bit_high),
                   "physical receive samples selected data level");
            expect(state, dspic33_device_advance(cpu, half), "physical receive returns clock low");
        }
        expect(state,
               dspic33_read_word(cpu, base) == received_values[route_index] &&
                   dspic33_i2c_test_interrupt_flag(cpu, master_irqs[route->channel]),
               "physical receive shifts the sampled byte into I2CxRCV");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[route->channel]);
        dspic33_gpio_drive(cpu, route->port, data_mask, data_mask);

        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9010u);
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, false),
            "physical ACK begins with driven data low");
        expect(state, dspic33_device_advance(cpu, half), "physical ACK reaches clock-high phase");
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false),
            "physical ACK holds data low while clock is high");
        expect(state, dspic33_device_advance(cpu, half), "physical ACK reaches completion");
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, true) &&
                dspic33_i2c_test_interrupt_flag(cpu, master_irqs[route->channel]),
            "physical ACK releases data and raises master interrupt");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[route->channel]);

        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9002u);
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, true),
            "physical restart begins from released data");
        expect(state, dspic33_device_advance(cpu, half), "physical restart releases clock");
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, true),
               "physical restart reaches bus idle level");
        expect(state, dspic33_device_advance(cpu, half), "physical restart drives data boundary");
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false),
            "physical restart drives data low while clock is high");
        expect(state, dspic33_device_advance(cpu, half), "physical restart completes");
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                           false) &&
                   dspic33_i2c_test_interrupt_flag(cpu, master_irqs[route->channel]),
               "physical restart returns to master wait state");
        dspic33_i2c_test_clear_interrupt(cpu, master_irqs[route->channel]);

        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9004u);
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, false),
            "physical stop begins with both lines low");
        expect(state, dspic33_device_advance(cpu, half), "physical stop releases clock");
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false),
            "physical stop holds data low after clock release");
        expect(state, dspic33_device_advance(cpu, half), "physical stop releases data");
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, true),
               "physical stop creates the low-to-high data transition");
        expect(state, dspic33_device_advance(cpu, half), "physical stop completes");
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, true) &&
                dspic33_i2c_test_interrupt_flag(cpu, master_irqs[route->channel]),
            "physical stop returns bus to idle and raises interrupt");
    }
    dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffffu);
}

void dspic33_i2c_test_master_pin_lifecycle_cases(TestState* state, Dspic33* cpu) {
    const uint16_t base = bases[1];
    const uint16_t clock_mask = 0x0020u;
    const uint16_t data_mask = 0x0010u;
    const uint64_t half = dspic33_i2c_test_operation_cycles(2u, 1u);
    Dspic33 copy;
    Dspic33I2cTransfer transfer;
    bool high;
    bool initialized;

    dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffffu);
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 5u, clock_mask, (uint16_t)(clock_mask | data_mask));
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    expect(state, dspic33_device_advance(cpu, half), "busy physical bus reaches start sample");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) != 0u &&
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]) && cpu->io.i2c_pin_active == 0u,
           "busy physical bus raises collision and aborts start");
    expect(state,
           dspic33_i2c_transmit(cpu, 1u, &transfer) && transfer.type == DSPIC33_I2C_COLLISION,
           "busy physical bus records collision output");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 5u, (uint16_t)(clock_mask | data_mask),
                       (uint16_t)(clock_mask | data_mask));
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u));
    dspic33_i2c_test_clear_interrupt(cpu, master_irqs[1]);
    dspic33_i2c_transmit(cpu, 1u, &transfer);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x00ffu);
    dspic33_gpio_drive(cpu, 5u, 0u, data_mask);
    expect(state, dspic33_device_advance(cpu, half), "arbitration loss reaches transmit sample");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0400u) != 0u &&
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]) &&
               (cpu->io.i2c_master_active & 2u) == 0u,
           "released-high transmit detects physical arbitration loss");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 5u, (uint16_t)(clock_mask | data_mask),
                       (uint16_t)(clock_mask | data_mask));
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u));
    dspic33_i2c_test_clear_interrupt(cpu, master_irqs[1]);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
    dspic33_gpio_drive(cpu, 5u, 0u, clock_mask);
    expect(state, dspic33_device_advance(cpu, half),
           "physical clock stretch reaches first release");
    expect(state,
           cpu->io.i2c_pin_phase[1] == 0u &&
               dspic33_i2c_test_pin_levels(cpu, 5u, 5u, 4u, false, false) &&
               !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]),
           "external low clock postpones edge and master completion");
    expect(state, dspic33_device_advance(cpu, 3u) && cpu->io.i2c_pin_phase[1] == 0u,
           "physical clock stretch retains phase across repeated retries");
    dspic33_gpio_drive(cpu, 5u, clock_mask, clock_mask);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.i2c_pin_phase[1] == 1u &&
               dspic33_i2c_test_pin_levels(cpu, 5u, 5u, 4u, true, false),
           "clock release resumes the postponed physical edge");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, 5u, (uint16_t)(clock_mask | data_mask),
                       (uint16_t)(clock_mask | data_mask));
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u));
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0u);
    dspic33_device_advance(cpu, half);
    dspic33_write_word(cpu, 0x0764u, 0x0002u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && (cpu->io.i2c_pmd_disabled & 2u) != 0u &&
               cpu->io.i2c_pin_phase[1] == 1u && !dspic33_i2c_pin(cpu, 5u, 5u, &high),
           "PMD disable releases pins and pauses physical byte phase");
    expect(state, dspic33_device_advance(cpu, 8u) && cpu->io.i2c_pin_phase[1] == 1u,
           "PMD-disabled physical byte remains paused");
    dspic33_write_word(cpu, 0x0764u, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && (cpu->io.i2c_pmd_disabled & 2u) == 0u &&
               dspic33_i2c_test_pin_levels(cpu, 5u, 5u, 4u, true, false),
           "PMD re-enable restores retained physical pin phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.i2c_pin_phase[1] == 2u &&
               dspic33_i2c_test_pin_levels(cpu, 5u, 5u, 4u, false, false),
           "PMD-resumed physical byte reaches its next edge");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize active physical I2C copy");
    if (initialized) {
        expect(state, dspic33_copy(&copy, cpu), "copy active physical I2C byte");
        expect(state,
               copy.io.i2c_pin_phase[1] == cpu->io.i2c_pin_phase[1] &&
                   copy.io.i2c_pin_active == cpu->io.i2c_pin_active &&
                   copy.events.count == cpu->events.count && copy.events.items != cpu->events.items,
               "copy retains independent physical I2C phase and events");
        expect(state,
               dspic33_device_advance(cpu, half) && dspic33_device_advance(&copy, half) &&
                   copy.io.i2c_pin_phase[1] == cpu->io.i2c_pin_phase[1],
               "copied physical I2C engines advance equally");
        dspic33_release(&copy);
    }

    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    cpu->pc = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "warm reset executes during physical I2C byte");
    expect(state,
           cpu->io.i2c_pin_active == 0u && cpu->io.i2c_pin_physical == 0u &&
               !dspic33_i2c_pin(cpu, 5u, 5u, &high),
           "warm reset cancels physical I2C phase and releases pins");
}

void dspic33_i2c_test_slave_pin_receive_cases(TestState* state, Dspic33* cpu) {
    size_t route_index;
    for (route_index = 0u; route_index < sizeof(pin_routes) / sizeof(pin_routes[0]);
         route_index++) {
        const I2cPinRoute* route = &pin_routes[route_index];
        uint16_t base = bases[route->channel];
        uint16_t pins = (uint16_t)((1u << route->clock) | (1u << route->data));
        uint8_t value = (uint8_t)(0x31u + route_index * 0x22u);

        dspic33_load_configuration_word(cpu, 0xf8000cu, route->configuration);
        dspic33_reset(cpu, 0u);
        dspic33_gpio_drive(cpu, route->port, pins, pins);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, route->channel, 0u, 0u);
        dspic33_i2c_test_drive_pin(route, cpu, false, false);
        expect(state,
               cpu->io.i2c_slave_pin_state[route->channel] == 1u &&
                   (cpu->io.i2c_slave_pin_active & (uint8_t)(1u << route->channel)) != 0u,
               "physical slave detects selected-pin Start condition");
        dspic33_i2c_test_drive_byte(route, cpu, 0xa4u);
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                           false) &&
                   dspic33_read_word(cpu, base) == 0x00a4u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[route->channel]),
               "physical slave receives address before ninth-clock interrupt");
        expect(state, dspic33_i2c_test_pop_slave_acknowledgement(cpu, route->channel, true),
               "physical slave address records logical ACK output");
        dspic33_i2c_test_drive_pin(route, cpu, true, true);
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false) &&
                !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[route->channel]),
            "physical slave drives address ACK while clock is high");
        dspic33_i2c_test_drive_pin(route, cpu, true, false);
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                           false) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[route->channel]) &&
                   cpu->io.i2c_slave_pin_state[route->channel] == 2u,
               "physical slave raises address interrupt and stretches after ninth clock");
        dspic33_read_word(cpu, base);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        dspic33_i2c_test_drive_pin(route, cpu, false, true);

        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[route->channel]);
        dspic33_i2c_test_drive_byte(route, cpu, value);
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                           false) &&
                   dspic33_read_word(cpu, base) == value &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[route->channel]),
               "physical slave receives data and drives ACK before interrupt");
        expect(state, dspic33_i2c_test_pop_slave_acknowledgement(cpu, route->channel, true),
               "physical slave data records logical ACK output");
        dspic33_i2c_test_drive_pin(route, cpu, false, true);
        dspic33_i2c_test_drive_pin(route, cpu, true, true);
        expect(
            state,
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false) &&
                !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[route->channel]),
            "physical slave holds data ACK through ninth clock high");
        dspic33_i2c_test_drive_pin(route, cpu, true, false);
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[route->channel]),
               "physical slave raises data interrupt on ninth falling edge");
        dspic33_i2c_test_drive_pin(route, cpu, false, false);
        dspic33_i2c_test_drive_pin(route, cpu, true, true);
        dspic33_i2c_test_drive_pin(route, cpu, false, true);
        expect(state,
               cpu->io.i2c_slave_pin_active == 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0018u) == 0x0010u &&
                   dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true,
                                               true),
               "physical slave Stop clears serial state and returns bus idle");
    }
}

void dspic33_i2c_test_slave_pin_rejection_and_transmit_cases(TestState* state, Dspic33* cpu) {
    const I2cPinRoute* route = &pin_routes[1];
    const uint16_t base = bases[1];
    const uint16_t clock_mask = 0x0020u;
    const uint16_t data_mask = 0x0010u;
    const uint16_t pins = (uint16_t)(clock_mask | data_mask);
    Dspic33I2cTransfer transfer;
    uint8_t bit;

    dspic33_load_configuration_word(cpu, 0xf8000cu, route->configuration);
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pins, pins);
    dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xa6u);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    expect(state,
           dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, true) &&
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]) &&
               (cpu->io.i2c_slave_rejected & 2u) != 0u,
           "physical unmatched address remains released for NACK");
    expect(state, dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, false),
           "physical unmatched address records logical NACK output");
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    expect(state, cpu->io.i2c_slave_pin_active == 0u,
           "physical Stop clears rejected-address state");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pins, pins);
    dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xa5u);
    expect(state,
           dspic33_read_word(cpu, base) == 0x00a5u &&
               dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true),
           "physical slave-transmit address is received and acknowledged");
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x005au);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
    dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[1]);
    expect(state,
           dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, false),
           "physical slave-transmit data starts after ninth-clock service");
    for (bit = 0u; bit < 8u; bit++) {
        bool bit_high = (0x5au & (uint8_t)(0x80u >> bit)) != 0u;
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                           bit_high),
               "physical slave transmit presents data while clock is low");
        dspic33_i2c_test_drive_pin(route, cpu, true, true);
        expect(state,
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true,
                                           bit_high),
               "physical slave transmit holds data while clock is high");
        dspic33_i2c_test_drive_pin(route, cpu, true, false);
    }
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    expect(state,
           dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, false) &&
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 1u) == 0u &&
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "physical slave clears TBF after eight bits before master ACK");
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    expect(state,
           dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, false),
           "physical slave samples master ACK on ninth rising edge");
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_transmit(cpu, 1u, &transfer) && transfer.type == DSPIC33_I2C_WRITE &&
               transfer.value == 0x5au && transfer.acknowledge && !transfer.master &&
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "physical master ACK completes slave transmit byte");
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    expect(state, cpu->io.i2c_slave_pin_active == 0u,
           "physical Stop completes slave transmit transaction");
}
