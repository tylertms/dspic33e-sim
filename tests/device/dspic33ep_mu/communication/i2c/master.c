#include "device/dspic33ep_mu/communication/i2c/internal.h"

void dspic33_i2c_test_address_mode_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t register_base = bases[channel];
        uint8_t channel_mask = (uint8_t)(1u << channel);
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0440u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit first address event");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit first address interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0100u) == 0u,
               "ten bit first address status");
        expect(state, dspic33_read_word(cpu, register_base) == 0x00f4u,
               "ten bit first address receive");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) == 0u,
               "ten bit first address stretches");
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9440u);
        expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) != 0u,
               "ten bit second address clock released");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state, dspic33_device_advance(cpu, 1u), "ten bit second address event");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit second address interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0100u) != 0u,
               "ten bit second address status");
        expect(state, dspic33_read_word(cpu, register_base) == 0x00abu,
               "ten bit second address receive");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) == 0u,
               "ten bit second address stretches");
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9440u);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, true, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit repeated read event");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit repeated read interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x010cu) == 0x010cu,
               "ten bit repeated read status");
        expect(state, dspic33_read_word(cpu, register_base) == 0x00f5u,
               "ten bit repeated read receive");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(register_base + 2u), 0x63u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9400u);
        expect(state,
               dspic33_i2c_slave_read(cpu, channel, false, 1u) && dspic33_device_advance(cpu, 1u),
               "ten bit slave nack event");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x8000u) != 0u,
               "ten bit slave nack omits interrupt");
        expect(state,
               (cpu->io.i2c_slave_active & channel_mask) == 0u &&
                   (cpu->io.i2c_slave_read & channel_mask) == 0u,
               "ten bit slave nack resets transmit state");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit address without receive stretch");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) == 0u,
               "ten bit first address always stretches");
        dspic33_read_word(cpu, register_base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "ten bit second address waits for release");
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9400u);
        expect(state, dspic33_device_advance(cpu, 1u), "ten bit second address after release");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0100u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) == 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00abu,
               "ten bit second address always stretches");

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0x0080u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "general call event");
        expect(state, dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "general call interrupt");
        expect(state, (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0200u) != 0u,
               "general call status");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x12u);
        dspic33_i2c_test_enable(cpu, channel, 0x0c80u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x67u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi address event");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_mask) != 0u &&
                   (cpu->io.i2c_slave_read & channel_mask) == 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00ceu,
               "ipmi accepts all addresses");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x0155u);
        dspic33_i2c_test_enable(cpu, channel, 0x0080u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "non-ipmi ten bit preamble without address mode");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_mask) != 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) == 0u,
               "non-ipmi address mode controls preamble matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x0155u);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "non-ipmi mismatched ten bit preamble");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_mask) != 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) == 0u,
               "non-ipmi address controls preamble matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x0155u);
        dspic33_i2c_test_enable(cpu, channel, 0x0c00u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi ten bit preamble with address mode");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_mask) == 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) != 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00f4u,
               "ipmi bypasses enabled ten bit address matching");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9c00u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0100u) != 0u &&
                   (cpu->io.i2c_slave_rejected & channel_mask) == 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) != 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00abu,
               "ipmi bypasses enabled ten bit address low matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x0155u);
        dspic33_i2c_test_enable(cpu, channel, 0x0880u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi ten bit preamble event");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_mask) == 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) != 0u &&
                   (cpu->io.i2c_slave_read & channel_mask) == 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00f4u,
               "ipmi accepts ten bit preamble");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9880u);
        expect(state, dspic33_device_advance(cpu, 1u), "ipmi ten bit address event");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x0100u) != 0u &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) == 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) != 0u &&
                   (cpu->io.i2c_slave_read & channel_mask) == 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00abu,
               "ipmi ten bit address stretches");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(register_base + 6u), 0x9880u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, true, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi ten bit read event");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x4107u) == 0x0106u &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) != 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) == 0u &&
                   (cpu->io.i2c_slave_read & channel_mask) == 0u &&
                   (cpu->io.i2c_slave_rejected & channel_mask) == 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00f5u,
               "ipmi ten bit read aborts without stretching");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x45u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 2u) == 0u,
               "ipmi ten bit read aborts slave transfer");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(register_base + 10u), 0x12u);
        dspic33_i2c_test_enable(cpu, channel, 0x0c80u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x67u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ipmi read address event");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 0x4007u) == 0x0006u &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 6u)) & 0x1000u) != 0u &&
                   (cpu->io.i2c_slave_active & channel_mask) == 0u &&
                   (cpu->io.i2c_slave_read & channel_mask) == 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00cfu,
               "ipmi read interrupts without stretching");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x44u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(register_base + 8u)) & 2u) == 0u &&
                   dspic33_read_word(cpu, register_base) == 0x00cfu,
               "ipmi read aborts slave transfer");
    }
}

void dspic33_i2c_test_address_rejection_cases(TestState* state, Dspic33* cpu) {
    uint8_t channel;
    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint8_t channel_bit = (uint8_t)(1u << channel);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, true, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) != 0u,
               "seven bit read control before mismatch");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9000u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x51u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "reject seven bit mismatch");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "seven bit mismatch enters rejected state");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x44u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "ignore data after seven bit mismatch");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore seven bit restart after mismatch");
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 0u) && dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "seven bit stop clears rejection");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "seven bit stop restores matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x01abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "reject ten bit high mismatch");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "ten bit high mismatch enters rejected state");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x45u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "ignore data after ten bit high mismatch");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit restart after high mismatch");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit low bytes after high mismatch");
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 0u) && dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit high stop clears rejection");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u,
               "ten bit stop restores high matching");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00abu &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit stop restores complete matching");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x02abu);
        dspic33_i2c_test_enable(cpu, channel, 0x0400u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02acu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "ten bit low mismatch first address");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u,
               "ten bit matching high byte remains active");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]),
               "reject ten bit low mismatch");
        expect(state,
               (cpu->io.i2c_slave_rejected & channel_bit) != 0u &&
                   (cpu->io.i2c_slave_active & channel_bit) == 0u &&
                   (cpu->io.i2c_slave_read & channel_bit) == 0u,
               "ten bit low mismatch enters rejected state");
        expect(state,
               dspic33_i2c_slave_write(cpu, channel, 0x46u, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "ignore data after ten bit low mismatch");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit restart after low mismatch");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   (cpu->io.i2c_slave_rejected & channel_bit) != 0u,
               "ignore ten bit low byte after low mismatch");
        expect(state,
               dspic33_i2c_slave_stop(cpu, channel, 0u) && dspic33_device_advance(cpu, 0u) &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit low stop clears rejection");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x02abu, false, true, 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & channel_bit) != 0u,
               "ten bit stop restores low matching");
        dspic33_read_word(cpu, base);
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[channel]);
        dspic33_write_word(cpu, (uint16_t)(base + 6u), 0x9400u);
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x0100u) != 0u &&
                   dspic33_read_word(cpu, base) == 0x00abu &&
                   (cpu->io.i2c_slave_rejected & channel_bit) == 0u,
               "ten bit low stop restores complete matching");
    }
}

void dspic33_i2c_test_isolation_and_power_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t pmd_addresses[DSPIC33_I2C_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t pmd_masks[DSPIC33_I2C_COUNT] = {0x0080u, 0x0002u};
    Dspic33I2cTransfer transfer;
    uint8_t channel;
    dspic33_reset(cpu, 0u);
    dspic33_i2c_test_enable(cpu, 0u, 1u, 0u);
    dspic33_i2c_test_enable(cpu, 1u, 1u, 9u);
    expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(0u)),
           "first channel independent completion");
    expect(state,
           dspic33_i2c_test_interrupt_flag(cpu, master_irqs[0]) &&
               !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]),
           "first channel independent interrupt");
    expect(state,
           dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(9u) -
                                           dspic33_i2c_test_control_cycles(0u)),
           "second channel independent completion");
    expect(state, dspic33_i2c_test_interrupt_flag(cpu, master_irqs[1]),
           "second channel independent interrupt");
    expect(state, dspic33_i2c_transmit(cpu, 0u, &transfer) && transfer.type == DSPIC33_I2C_START,
           "first channel independent output");
    expect(state, dspic33_i2c_transmit(cpu, 1u, &transfer) && transfer.type == DSPIC33_I2C_START,
           "second channel independent output");

    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t control = (uint16_t)(bases[channel] + 6u);
        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_write_word(cpu, control, 0x8000u);
        expect(state, dspic33_read_word(cpu, control) == 0x9000u,
               "software cannot clear release without stretch enable");
        dspic33_write_word(cpu, control, 0x8040u);
        expect(state, dspic33_read_word(cpu, control) == 0x8040u,
               "software clears release with stretch enable");
        dspic33_write_word(cpu, control, 0u);
        expect(state, dspic33_read_word(cpu, control) == 0x1000u, "module disable releases clock");
    }
    dspic33_reset(cpu, 0u);
    dspic33_i2c_test_enable(cpu, 0u, 0u, 0u);
    dspic33_write_byte(cpu, 0x0203u, 0x5au);
    expect(state, (dspic33_read_word(cpu, 0x0208u) & 0x4001u) == 0u, "transmit high byte ignored");
    dspic33_write_byte(cpu, 0x0202u, 0x5au);
    expect(state, (dspic33_read_word(cpu, 0x0208u) & 0x4001u) == 0x4001u,
           "transmit low byte starts transfer");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x0206u, 0xb001u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(0u)),
           "idle stop advance");
    expect(state,
           (dspic33_read_word(cpu, 0x0206u) & 1u) != 0u &&
               !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[0]),
           "idle stop freezes module");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state, dspic33_device_advance(cpu, 1u), "idle resume advance");
    expect(state,
           (dspic33_read_word(cpu, 0x0206u) & 1u) == 0u &&
               dspic33_i2c_test_interrupt_flag(cpu, master_irqs[0]),
           "idle resume completes module");

    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t base = bases[channel];
        uint64_t total = dspic33_i2c_test_control_cycles(9u);
        uint64_t elapsed = 4u;
        uint64_t remaining = total - elapsed;

        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 1u, 9u);
        expect(state, dspic33_device_advance(cpu, elapsed), "power stop master partial advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) != 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "power stop master remains pending");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state, dspic33_device_advance(cpu, total + 5u),
               "power stop master disabled advance");
        expect(state,
               (dspic33_i2c_test_stored_word(cpu, (uint16_t)(base + 6u)) & 1u) != 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "power stop freezes master state");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, remaining - 1u),
               "power stop master remaining advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) != 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "power stop master waits exact remainder");
        expect(state, dspic33_device_advance(cpu, 1u), "power stop master completion advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 6u)) & 1u) == 0u &&
                   dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]) &&
                   dspic33_i2c_transmit(cpu, channel, &transfer) &&
                   transfer.type == DSPIC33_I2C_START,
               "power stop master completes after remainder");

        total = dspic33_i2c_test_byte_cycles(3u);
        elapsed = 12u;
        remaining = total - elapsed;
        dspic33_reset(cpu, 0u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 3u);
        dspic33_write_word(cpu, (uint16_t)(base + 2u), 0x5au);
        expect(state, dspic33_device_advance(cpu, elapsed), "power stop transmit partial advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "power stop transmit remains active");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state, dspic33_device_advance(cpu, total + 5u),
               "power stop transmit disabled advance");
        expect(state,
               (dspic33_i2c_test_stored_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4001u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "power stop freezes transmit state");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, remaining - 1u),
               "power stop transmit remaining advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0x4000u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
               "power stop transmit waits exact remainder");
        expect(state, dspic33_device_advance(cpu, 1u), "power stop transmit completion advance");
        expect(state,
               (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 0x4001u) == 0u &&
                   dspic33_i2c_test_interrupt_flag(cpu, master_irqs[channel]),
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
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, delay) &&
                   dspic33_i2c_slave_stop(cpu, channel, delay + 1u) &&
                   dspic33_i2c_slave_start(cpu, channel, 0x53u, false, false, delay + 2u) &&
                   cpu->events.count == 3u,
               "power stop queues long slave sequence");
        expect(state, dspic33_device_advance(cpu, elapsed), "power stop slave partial advance");
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        dspic33_write_word(cpu, (uint16_t)(other_base + 10u), 0x31u);
        dspic33_i2c_test_enable(cpu, other, 0u, 0u);
        expect(state,
               dspic33_i2c_slave_start(cpu, other, 0x31u, false, false, 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "power stop other channel event advance");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[other]) &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, other_base) == 0x0062u && cpu->events.count == 3u,
               "power stop parked events do not block other channel");
        dspic33_i2c_test_clear_interrupt(cpu, slave_irqs[other]);
        expect(state, dspic33_i2c_slave_stop(cpu, other, 0u) && dspic33_device_advance(cpu, 0u),
               "power stop other channel cleanup");
        expect(state, dspic33_device_advance(cpu, 19u), "power stop slave disabled advance");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_i2c_test_stored_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) == 0u &&
                   cpu->events.count == 0u,
               "power stop drops external slave sequence");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, 6u), "power stop slave remaining advance");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "power stop slave waits exact remainder");
        expect(state, dspic33_device_advance(cpu, 1u), "power stop missed slave deadline advance");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   cpu->events.count == 0u &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) == 0u,
               "power stop does not replay missed slave events");
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "power stop accepts new slave event after re-enable");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, base) == 0x00a4u &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) != 0u,
               "power stop receives new slave event after re-enable");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, delay) &&
                   cpu->events.count == 2u &&
                   (cpu->io.i2c_pmd_disabled & (uint8_t)(1u << channel)) == 0u,
               "power stop queues long event while disabled");
        expect(state, dspic33_device_advance(cpu, 25u), "power stop scheduled-disabled advance");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_i2c_test_stored_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u &&
                   cpu->events.count == 1u,
               "power stop holds event scheduled while disabled");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, dspic33_device_advance(cpu, delay - 26u),
               "power stop scheduled-disabled remaining advance");
        expect(state,
               !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (dspic33_read_word(cpu, (uint16_t)(base + 8u)) & 2u) == 0u,
               "power stop scheduled-disabled waits exact delay");
        expect(state, dspic33_device_advance(cpu, 1u),
               "power stop scheduled-disabled completion advance");
        expect(state,
               dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   dspic33_read_word(cpu, base) == 0x00a4u,
               "power stop scheduled-disabled event completes");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, (uint16_t)(base + 10u), 0x52u);
        dspic33_i2c_test_enable(cpu, channel, 0u, 0u);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               dspic33_i2c_slave_start(cpu, channel, 0x52u, false, false, 1u) &&
                   cpu->events.count == 2u &&
                   (cpu->io.i2c_pmd_disabled & (uint8_t)(1u << channel)) == 0u,
               "power stop queues horizon event");
        expect(state, dspic33_device_advance(cpu, UINT64_MAX), "power stop horizon advance");
        expect(state,
               cpu->events.count == 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_slave_active & (uint8_t)(1u << channel)) == 0u,
               "power stop drops horizon external event");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.count == 0u &&
                   !dspic33_i2c_test_interrupt_flag(cpu, slave_irqs[channel]) &&
                   (cpu->io.i2c_pmd_disabled & (uint8_t)(1u << channel)) != 0u,
               "power stop reports unrepresentable re-enable");
    }

    dspic33_reset(cpu, 0u);
    dspic33_i2c_test_enable(cpu, 0u, 1u, 0x01ffu);
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_device_advance(cpu, dspic33_i2c_test_control_cycles(0x01ffu)),
           "reset canceled operation advance");
    expect(state, !dspic33_i2c_test_interrupt_flag(cpu, master_irqs[0]),
           "reset cancels pending operation");
    expect(state, !dspic33_i2c_transmit(cpu, 0u, &transfer), "reset clears output queue");
}

void dspic33_i2c_test_pmd_transition_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t pmd_addresses[DSPIC33_I2C_COUNT] = {0x0760u, 0x0764u};
    static const uint16_t pmd_masks[DSPIC33_I2C_COUNT] = {0x0080u, 0x0002u};
    Dspic33 copy;
    uint8_t channel;
    bool initialized;

    for (channel = 0u; channel < DSPIC33_I2C_COUNT; channel++) {
        uint16_t baud = (uint16_t)(bases[channel] + 4u);
        uint8_t bit = (uint8_t)(1u << channel);

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               (cpu->io.i2c_pmd_disabled & bit) == 0u && cpu->io.i2c_pmd_generation[channel] == 1u,
               "PMD disable is delayed");
        dspic33_write_word(cpu, baud, 0x0055u);
        expect(state, dspic33_read_word(cpu, baud) == 0x0055u,
               "PMD disable permits current cycle access");
        expect(state, dspic33_device_advance(cpu, 1u) && (cpu->io.i2c_pmd_disabled & bit) != 0u,
               "PMD disable applies after one cycle");
        dspic33_write_word(cpu, baud, 0x00aau);
        expect(state, dspic33_i2c_test_stored_word(cpu, baud) == 0x0055u,
               "PMD disabled module ignores writes");
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state,
               (cpu->io.i2c_pmd_disabled & bit) != 0u && cpu->io.i2c_pmd_generation[channel] == 2u,
               "PMD enable is delayed");
        dspic33_write_word(cpu, baud, 0x00bbu);
        expect(state, dspic33_i2c_test_stored_word(cpu, baud) == 0x0055u,
               "PMD enable blocks access until transition");
        expect(state, dspic33_device_advance(cpu, 1u) && (cpu->io.i2c_pmd_disabled & bit) == 0u,
               "PMD enable applies after one cycle");
        dspic33_write_word(cpu, baud, 0x00ccu);
        expect(state, dspic33_read_word(cpu, baud) == 0x00ccu, "PMD enabled module accepts writes");

        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        dspic33_write_word(cpu, pmd_addresses[channel], 0u);
        expect(state, cpu->io.i2c_pmd_generation[channel] == 2u && cpu->events.count == 2u,
               "rapid PMD toggle queues generations");
        expect(state,
               dspic33_device_advance(cpu, 1u) && (cpu->io.i2c_pmd_disabled & bit) == 0u &&
                   (dspic33_read_word(cpu, pmd_addresses[channel]) & pmd_masks[channel]) == 0u,
               "stale PMD event cannot override latest state");

        dspic33_reset(cpu, 0u);
        cpu->device_cycles = UINT64_MAX;
        dspic33_write_word(cpu, pmd_addresses[channel], pmd_masks[channel]);
        expect(state,
               (dspic33_read_word(cpu, pmd_addresses[channel]) & pmd_masks[channel]) == 0u &&
                   cpu->io.i2c_pmd_generation[channel] == 2u &&
                   (cpu->io.i2c_pmd_disabled & bit) == 0u && cpu->events.count == 0u,
               "failed PMD transition rolls back and invalidates generation");
        expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
               "failed PMD transition reports queue error");
    }

    dspic33_reset(cpu, 0u);
    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize pending PMD copy");
    if (!initialized) {
        return;
    }
    dspic33_write_word(cpu, pmd_addresses[0], pmd_masks[0]);
    expect(state, dspic33_copy(&copy, cpu), "copy pending PMD transition");
    expect(state,
           copy.io.i2c_pmd_generation[0] == 1u && copy.io.i2c_pmd_disabled == 0u &&
               copy.events.count == 1u && copy.events.items != cpu->events.items,
           "copy retains independent pending PMD state");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u) &&
               cpu->io.i2c_pmd_disabled == 1u && copy.io.i2c_pmd_disabled == 1u,
           "copied PMD transitions complete equally");
    dspic33_release(&copy);

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, pmd_addresses[0], pmd_masks[0]);
    dspic33_reset(cpu, 0u);
    expect(state,
           cpu->io.i2c_pmd_generation[0] == 0u && cpu->io.i2c_pmd_disabled == 0u &&
               cpu->events.count == 0u &&
               (dspic33_read_word(cpu, pmd_addresses[0]) & pmd_masks[0]) == 0u,
           "reset cancels pending PMD transition");
}
