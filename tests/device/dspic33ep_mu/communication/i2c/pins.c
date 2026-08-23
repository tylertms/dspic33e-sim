#include "device/dspic33ep_mu/communication/i2c/internal.h"

static void slave_pin_ten_bit_cases(TestState* state, Dspic33* cpu) {
    const I2cPinRoute* route = &pin_routes[2];
    const uint16_t register_base = bases[1];
    const uint16_t pin_mask = 0x000cu;

    dspic33_load_configuration_word(cpu, 0xf8000cu, route->configuration);
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x02abu);
    dspic33_i2c_test_enable(cpu, 1u, 0x0400u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xf4u);
    expect(state,
           dspic33_read_word(cpu, register_base) == 0x00f4u &&
               dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false,
                                           false) &&
               dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true),
           "physical 10-bit high address matches and stretches for ACK");
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_read_word(cpu, register_base);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9400u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    expect(state, cpu->io.i2c_slave_pin_state[1] == 7u,
           "physical 10-bit ACK advances to second address phase");

    dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[1]);
    dspic33_i2c_test_drive_byte(route, cpu, 0xabu);
    expect(state,
           dspic33_read_word(cpu, register_base) == 0x00abu &&
               (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0100u) != 0u &&
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]) &&
               dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true),
           "physical 10-bit second address matches and sets ADD10");
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "physical 10-bit second address interrupts on ninth falling edge");
    dspic33_read_word(cpu, register_base);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9400u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    expect(state, cpu->io.i2c_slave_pin_state[1] == 2u,
           "physical 10-bit write enters receive data phase");

    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    expect(state, cpu->io.i2c_slave_pin_state[1] == 1u,
           "physical repeated Start returns to address phase");
    dspic33_i2c_test_drive_byte(route, cpu, 0xf5u);
    expect(state,
           dspic33_read_word(cpu, register_base) == 0x00f5u &&
               (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0104u) == 0x0104u &&
               dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true),
           "physical 10-bit repeated-read address is accepted");
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_read_word(cpu, register_base);
    dspic33_write_word(cpu, (uint16_t)(register_base + 2u), 0x00c3u);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9400u);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(
        state,
        cpu->io.i2c_slave_pin_state[1] == 4u &&
            dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, true),
        "physical 10-bit repeated-read ACK enters transmit phase");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x02abu);
    dspic33_i2c_test_enable(cpu, 1u, 0x0400u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xf2u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, true) &&
               dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, false) &&
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "physical 10-bit high mismatch produces NACK");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x02abu);
    dspic33_i2c_test_enable(cpu, 1u, 0x0400u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xf4u);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_read_word(cpu, register_base);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9400u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_byte(route, cpu, 0xacu);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, false, true) &&
               dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true) &&
               dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, false),
           "physical 10-bit low mismatch ACKs high byte then NACKs low byte");
    dspic33_load_configuration_word(cpu, 0xf8000cu, 0xffffu);
}

static void slave_pin_lifecycle_cases(TestState* state, Dspic33* cpu) {
    const I2cPinRoute* route = &pin_routes[1];
    const uint16_t register_base = bases[1];
    const uint16_t pin_mask = 0x0030u;
    Dspic33 copied_cpu;
    bool copy_initialized;
    bool is_clock_high;
    uint8_t bit_index;

    dspic33_load_configuration_word(cpu, 0xf8000cu, route->configuration);
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    for (bit_index = 0u; bit_index < 3u; bit_index++) {
        dspic33_i2c_test_drive_pin(route, cpu, true, false);
        dspic33_i2c_test_drive_pin(route, cpu, false,
                                   (0xa4u & (uint8_t)(0x80u >> bit_index)) != 0u);
        dspic33_i2c_test_drive_pin(route, cpu, true, true);
    }
    dspic33_write_word(cpu, 0x0764u, 0x0002u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.i2c_slave_pin_bits[1] == 3u &&
               !dspic33_i2c_pin(cpu, route->port, route->clock, &is_clock_high),
           "PMD disable releases and freezes partial physical slave byte");
    dspic33_i2c_test_drive_byte(route, cpu, 0xffu);
    expect(state,
           cpu->io.i2c_slave_pin_bits[1] == 3u &&
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "PMD-disabled physical slave misses external serial edges");
    dspic33_write_word(cpu, 0x0764u, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && (cpu->io.i2c_pmd_disabled & 2u) == 0u,
           "PMD re-enable restores physical slave engine");
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    expect(state, cpu->io.i2c_slave_pin_state[1] == 1u && cpu->io.i2c_slave_pin_bits[1] == 0u,
           "fresh Start replaces partial slave byte after PMD resume");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0x2000u, 0u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xa4u);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           cpu->io.i2c_slave_pin_active == 0u &&
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "I2CSIDL physical slave misses Start and byte edges in Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    expect(state, cpu->io.i2c_slave_pin_state[1] == 1u,
           "physical slave accepts fresh Start after Idle resume");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xa4u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]) &&
               dspic33_read_word(cpu, register_base) == 0xa4u,
           "physical slave continues and raises interrupt in Sleep");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    for (bit_index = 0u; bit_index < 4u; bit_index++) {
        dspic33_i2c_test_drive_pin(route, cpu, true, false);
        dspic33_i2c_test_drive_pin(route, cpu, false,
                                   (0xa4u & (uint8_t)(0x80u >> bit_index)) != 0u);
        dspic33_i2c_test_drive_pin(route, cpu, true, true);
    }
    copy_initialized = dspic33_initialize(&copied_cpu);
    expect(state, copy_initialized, "initialize partial physical slave copy");
    if (copy_initialized) {
        expect(state, dspic33_copy(&copied_cpu, cpu), "copy partial physical slave byte");
        for (bit_index = 4u; bit_index < 8u; bit_index++) {
            bool bit_high = (0xa4u & (uint8_t)(0x80u >> bit_index)) != 0u;
            dspic33_i2c_test_drive_pin(route, cpu, true, false);
            dspic33_i2c_test_drive_pin(route, &copied_cpu, true, false);
            dspic33_i2c_test_drive_pin(route, cpu, false, bit_high);
            dspic33_i2c_test_drive_pin(route, &copied_cpu, false, bit_high);
            dspic33_i2c_test_drive_pin(route, cpu, true, true);
            dspic33_i2c_test_drive_pin(route, &copied_cpu, true, true);
        }
        dspic33_i2c_test_drive_pin(route, cpu, true, false);
        dspic33_i2c_test_drive_pin(route, &copied_cpu, true, false);
        expect(state,
               dspic33_read_word(cpu, register_base) == 0xa4u &&
                   dspic33_read_word(&copied_cpu, register_base) == 0xa4u &&
                   cpu->io.i2c_slave_pin_state[1] == copied_cpu.io.i2c_slave_pin_state[1],
               "copied physical slave engines complete independently and equally");
        dspic33_release(&copied_cpu);
    }

    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    cpu->pc = 0u;
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "warm reset executes during physical slave ACK");
    expect(state,
           cpu->io.i2c_slave_pin_active == 0u && cpu->io.i2c_pin_physical == 0u &&
               cpu->io.i2c_slave_pin_state[1] == 0u,
           "warm reset clears physical slave engine state");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xa4u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9000u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_byte(route, cpu, 0x39u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    expect(state,
           dspic33_i2c_test_pin_levels(cpu, route->port, route->clock, route->data, true, true) &&
               (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0042u) == 0x0042u,
           "uncleared physical address byte makes following data overflow and NACK");
}

static void master_pin_collision_cases(TestState* state, Dspic33* cpu) {
    const I2cPinRoute* route = &pin_routes[1];
    const uint16_t register_base = bases[1];
    const uint16_t clock_mask = 0x0020u;
    const uint16_t data_mask = 0x0010u;
    const uint16_t pin_mask = (uint16_t)(clock_mask | data_mask);
    const uint64_t half_period = dspic33_i2c_test_operation_cycles(2u, 1u);

    dspic33_load_configuration_word(cpu, 0xf8000cu, route->configuration);
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u));
    dspic33_i2c_test_clear_interrupt(cpu, master_irqs[1]);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9002u);
    dspic33_gpio_drive(cpu, route->port, 0u, data_mask);
    expect(state, dspic33_device_advance(cpu, half_period),
           "repeated-start arbitration reaches released bus phase");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0400u) != 0u &&
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]) && cpu->io.i2c_pin_active == 0u,
           "repeated-start collision aborts when released data remains low");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u));
    dspic33_i2c_test_clear_interrupt(cpu, master_irqs[1]);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9004u);
    dspic33_gpio_drive(cpu, route->port, 0u, data_mask);
    expect(state, dspic33_device_advance(cpu, half_period * 2u),
           "stop arbitration reaches data-release phase");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0400u) != 0u &&
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]) && cpu->io.i2c_pin_active == 0u,
           "stop collision aborts when released data remains low");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(2u));
    dspic33_i2c_test_clear_interrupt(cpu, master_irqs[1]);
    dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9030u);
    dspic33_gpio_drive(cpu, route->port, 0u, data_mask);
    expect(state, dspic33_device_advance(cpu, half_period),
           "released NACK arbitration reaches sample phase");
    expect(state,
           (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0400u) != 0u &&
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]) && cpu->io.i2c_pin_active == 0u,
           "released NACK collision aborts on dominant data");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    cpu->device_cycles = UINT64_MAX;
    dspic33_i2c_test_enable(cpu, 1u, 1u, 2u);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 1u) == 0u &&
               cpu->io.i2c_pin_active == 0u,
           "failed physical condition scheduling rolls back control and pin state");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 2u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, (uint16_t)(register_base + 2u), 0x005au);
    expect(state,
           cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u &&
               (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x4001u) == 0u &&
               dspic33_i2c_test_stored_word(cpu, (uint16_t)(register_base + 2u)) == 0x00ffu &&
               (cpu->io.i2c_master_active & 2u) == 0u,
           "failed physical transmit scheduling rolls back register and runtime state");
}

static void slave_pin_address_policy_cases(TestState* state, Dspic33* cpu) {
    const I2cPinRoute* route = &pin_routes[1];
    const uint16_t register_base = bases[1];
    const uint16_t pin_mask = 0x0030u;

    dspic33_load_configuration_word(cpu, 0xf8000cu, route->configuration);
    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x20u);
    dspic33_write_word(cpu, (uint16_t)(register_base + 12u), 0x7fu);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0x04u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, false) &&
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "address mask cannot admit a reserved slave address");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x02u);
    dspic33_write_word(cpu, (uint16_t)(register_base + 12u), 0x7fu);
    dspic33_i2c_test_enable(cpu, 1u, 0u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0x04u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true) &&
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "classic slave directly matches its configured reserved address");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x02abu);
    dspic33_i2c_test_enable(cpu, 1u, 0x0480u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true) &&
               (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0204u) == 0x0200u &&
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "general call remains a 7-bit write while 10-bit mode is selected");

    dspic33_reset(cpu, 0u);
    dspic33_gpio_drive(cpu, route->port, pin_mask, pin_mask);
    dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x52u);
    dspic33_i2c_test_enable(cpu, 1u, 0x0800u, 0u);
    dspic33_i2c_test_drive_pin(route, cpu, false, false);
    dspic33_i2c_test_drive_byte(route, cpu, 0xceu);
    dspic33_i2c_test_drive_pin(route, cpu, false, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, true);
    dspic33_i2c_test_drive_pin(route, cpu, true, false);
    expect(state,
           dspic33_i2c_test_pop_slave_acknowledgement(cpu, 1u, true) &&
               dspic33_read_word(cpu, register_base) == 0x00ceu &&
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[1]),
           "IPMI mode physically acknowledges an arbitrary address");
}

static void acknowledge_rmw_erratum_cases(TestState* state, Dspic33* cpu) {
    uint16_t control = (uint16_t)(bases[0] + 6u);
    dspic33_reset(cpu, 0x0200u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    dspic33_write_word(cpu, control, 0x9010u);
    dspic33_load_program_word(cpu, 0x0200u, 0xa8a206u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED &&
               (dspic33_read_word(cpu, control) & 0x0010u) != 0u,
           "B1 ACKEN clear concurrent with an I2C control RMW remains undefined");

    dspic33_reset(cpu, 0x0200u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    dspic33_write_word(cpu, control, 0x9010u);
    dspic33_load_program_word(cpu, 0x0200u, (uint32_t)(0xec2000u | control));
    expect(state, dspic33_step(cpu) == DSPIC33_SILICON_RESULT_UNDEFINED,
           "B1 ACKEN clear detects non-bit RMW instructions");

    dspic33_reset(cpu, 0x0200u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    dspic33_write_word(cpu, control, 0x9010u);
    cpu->w[0] = dspic33_read_word(cpu, control);
    dspic33_load_program_word(cpu, 0x0200u, (uint32_t)(0x880000u | control / 2u));
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
           "plain MOV remains outside the ACKEN RMW boundary");

    dspic33_reset(cpu, 0x0200u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    dspic33_write_word(cpu, control, 0x9010u);
    dspic33_load_program_word(cpu, 0x0200u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && dspic33_device_advance(cpu, 1u) &&
               (dspic33_read_word(cpu, control) & 0x0010u) == 0u &&
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[0]),
           "ACKEN clears normally without a concurrent I2C control RMW");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    if (!dspic33_initialize(&cpu)) {
        fprintf(stderr, "failed to initialize simulator\n");
        return 2;
    }
    dspic33_i2c_test_register_cases(&state, &cpu);
    dspic33_i2c_test_timing_cases(&state, &cpu);
    dspic33_i2c_test_bus_status_timing_cases(&state, &cpu);
    dspic33_i2c_test_master_sequence_cases(&state, &cpu);
    dspic33_i2c_test_master_error_cases(&state, &cpu);
    dspic33_i2c_test_slave_receive_cases(&state, &cpu);
    dspic33_i2c_test_slave_transmit_cases(&state, &cpu);
    dspic33_i2c_test_address_mode_cases(&state, &cpu);
    dspic33_i2c_test_address_rejection_cases(&state, &cpu);
    dspic33_i2c_test_slave_acknowledgement_cases(&state, &cpu);
    dspic33_i2c_test_disable_cases(&state, &cpu);
    dspic33_i2c_test_isolation_and_power_cases(&state, &cpu);
    dspic33_i2c_test_pmd_transition_cases(&state, &cpu);
    dspic33_i2c_test_slave_power_cases(&state, &cpu);
    dspic33_i2c_test_dma_isolation_cases(&state, &cpu);
    dspic33_i2c_test_pin_routing_cases(&state, &cpu);
    dspic33_i2c_test_master_pin_sequence_cases(&state, &cpu);
    dspic33_i2c_test_master_pin_lifecycle_cases(&state, &cpu);
    dspic33_i2c_test_slave_pin_receive_cases(&state, &cpu);
    dspic33_i2c_test_slave_pin_rejection_and_transmit_cases(&state, &cpu);
    dspic33_i2c_test_boundary_cases(&state, &cpu);
    slave_pin_ten_bit_cases(&state, &cpu);
    slave_pin_lifecycle_cases(&state, &cpu);
    master_pin_collision_cases(&state, &cpu);
    slave_pin_address_policy_cases(&state, &cpu);
    acknowledge_rmw_erratum_cases(&state, &cpu);
    dspic33_release(&cpu);
    return test_finish(&state);
}
