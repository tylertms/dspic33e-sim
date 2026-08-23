#include "device/dspic33ep_mu/communication/dci/internal.h"

void dspic33_dci_test_pps_disable_timing_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 0u);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) && cpu->io.dci.serial_bits == 1u,
           "advance PPS DCI to exact three-clock disable boundary");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_SAMPLE_RISING | DCI_EXTERNAL_CLOCK);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u,
           "external DCI exact three-clock clear selects current frame");
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) && !cpu->io.dci.started &&
               !cpu->io.dci.initialized && !cpu->io.dci.disable_pending,
           "external DCI exact boundary stops at current frame end");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 0u);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) && cpu->io.dci.serial_bits == 2u,
           "advance PPS DCI inside final three clocks");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_SAMPLE_RISING | DCI_EXTERNAL_CLOCK);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 2u,
           "external DCI late clear selects following frame");
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) && cpu->io.dci.started &&
               cpu->io.dci.disable_frames == 1u,
           "external DCI late clear completes current frame");
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xa000u, 4u, true) && !cpu->io.dci.started &&
               !cpu->io.dci.initialized && !cpu->io.dci.disable_pending,
           "external DCI late clear stops after following frame");
}

void dspic33_dci_test_pps_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copied_cpu;
    bool is_high;

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, true, true) && cpu->io.dci.serial_bits == 3u,
           "advance PPS DCI before active copy");
    expect(state, dspic33_initialize(&copied_cpu) && dspic33_copy(&copied_cpu, cpu),
           "copy active PPS DCI shift state");
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(&copied_cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(&copied_cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(&copied_cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(&copied_cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(&copied_cpu, true, true),
           "complete original and copied PPS DCI shifts");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa500u &&
               dspic33_read_word(&copied_cpu, DCI_RECEIVE_BASE) == 0xa500u,
           "copied PPS DCI shift completes independently");
    dspic33_release(&copied_cpu);

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) && cpu->io.dci.serial_bits == 2u,
           "advance PPS DCI before PMD disable");
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled,
           "disable PPS DCI through delayed PMD transition");
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
               cpu->io.dci.serial_bits == 2u,
           "PPS DCI misses serial edges while PMD-disabled");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled,
           "enable PPS DCI through delayed PMD transition");
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x9000u,
           "PPS DCI resumes retained partial word after PMD");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 4u, 1u, 1u, 0u, 1u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0x6000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x6000u,
           "externally clocked PPS DCI continues in Sleep");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING | DCI_STOP_IDLE, 4u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_drive_serial_bit(cpu, true, true),
           "advance PPS DCI before Idle stop");
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
               cpu->io.dci.serial_bits == 1u,
           "DCISIDL makes PPS DCI miss Idle serial edges");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_CLOCK_MASK);
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "PPS DCI resumes retained partial word after Idle");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(cpu, DCI_SAMPLE_RISING, 8u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_drive_serial_bit(cpu, true, true) &&
               dspic33_dci_test_drive_serial_bit(cpu, false, true) && cpu->io.dci.serial_bits == 2u,
           "advance PPS DCI before warm reset");
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->io.dci.serial_bits == 0u &&
               dspic33_read_word(cpu, DCI_CONTROL1) == 0u &&
               dspic33_gpio_pin(cpu, GPIO_PORT_D, 1u, &is_high) && is_high,
           "warm reset clears PPS shift state and preserves physical levels");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PPS_CLOCK_FRAME_OUTPUT, 0x0d0cu);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_DATA_JUSTIFY);
    expect(state,
           dspic33_device_advance(cpu, 5u) && dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &is_high),
           "advance master PPS output before PMD");
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled &&
               !dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &is_high),
           "PMD releases DCI PPS outputs");
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled &&
               dspic33_dci_pin(cpu, PPS_CLOCK_OUTPUT_PIN, &is_high),
           "PMD clear restores retained DCI PPS output phase");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(
        cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY, 4u, 1u, 1u, 0u, 1u);
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
               dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) &&
               cpu->io.dci.initialized && !cpu->io.dci.started &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "PMD clear cannot replay missed COFS edge");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK | GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xa000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "post-PMD COFS edge starts a new frame");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_serial_pins(cpu);
    dspic33_dci_test_configure_external(
        cpu, DCI_SAMPLE_RISING | DCI_EXTERNAL_FRAME | DCI_DATA_JUSTIFY | DCI_STOP_IDLE, 4u, 1u, 1u,
        0u, 1u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    expect(state,
           dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK) &&
               !cpu->io.dci.pps_frame_pending,
           "DCISIDL makes DCI miss Idle COFS edge");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0xf000u, 4u, true) && cpu->io.dci.initialized &&
               !cpu->io.dci.started && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "Idle resume cannot replay missed COFS edge");
    dspic33_gpio_drive(cpu, GPIO_PORT_D, 0u, GPIO_FRAME_MASK | GPIO_CLOCK_MASK);
    dspic33_gpio_drive(cpu, GPIO_PORT_D, GPIO_FRAME_MASK, GPIO_FRAME_MASK);
    expect(state,
           dspic33_dci_test_drive_serial_word(cpu, 0x5000u, 4u, true) &&
               dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5000u,
           "post-Idle COFS edge starts a new frame");
}

void dspic33_dci_test_mode_and_status_cases(TestState* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    uint8_t retain_last_transmit;

    for (retain_last_transmit = 0u; retain_last_transmit < 2u; retain_last_transmit++) {
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_external(
            cpu, retain_last_transmit != 0u ? DCI_UNDERFLOW_LAST : 0u, 16u, 1u, 1u, 1u, 0u);
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x5a5au);
        expect(state, dspic33_dci_test_clock_word(cpu, 0u, false),
               "clock initial underflow mode word");
        expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x5a5au,
               "initial TXBUF value reaches DCI output");
        expect(state, dspic33_dci_test_clock_word(cpu, 0u, false),
               "clock underflow replacement word");
        expect(state,
               dspic33_dci_transmit(cpu, &transfer) &&
                   transfer.value == (retain_last_transmit != 0u ? 0x5a5au : 0u),
               "UNFM selects last value or zero after underflow");
        expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_UNDERFLOW) != 0u,
               "missing TXBUF sets TUNF");
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x6b6bu);
        expect(state, (dspic33_read_word(cpu, DCI_STATUS) & DCI_TRANSMIT_UNDERFLOW) == 0u,
               "culprit TXBUF write clears TUNF");
    }

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, DCI_LOOPBACK, 16u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1357u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x2468u, false), "clock loopback word");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x1357u,
           "DLOOP routes transmit shadow into receive buffer");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 2u, 1u, 1u, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x7777u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0u, false), "clock driven active slot");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.driven,
           "enabled transmit slot drives output");
    expect(state, dspic33_dci_test_clock_word(cpu, 0u, false), "clock driven disabled slot");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0u && transfer.driven,
           "CSDOM zero drives disabled slot low");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, DCI_TRISTATE, 16u, 2u, 1u, 1u, 0u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x8888u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0u, false), "clock tristate active slot");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.driven,
           "CSDOM one still drives enabled transmit slot");
    expect(state, dspic33_dci_test_clock_word(cpu, 0u, false), "clock tristate disabled slot");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && !transfer.driven,
           "CSDOM one tri-states disabled slot");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 2u, 0u, 1u);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x1001u, false) &&
               dspic33_dci_test_clock_word(cpu, 0x1002u, false),
           "fill two receive buffers");
    expect(state, cpu->io.dci.receive_unread == 0x03u, "RFUL tracks both unread RXBUFs");
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x1001u,
           "read first RXBUF independently");
    expect(state,
           cpu->io.dci.receive_unread == 0x02u &&
               (dspic33_read_word(cpu, DCI_STATUS) & DCI_RECEIVE_FULL) != 0u,
           "RFUL remains until every active RXBUF is read");
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x2001u, false) &&
               dspic33_dci_test_clock_word(cpu, 0x2002u, false),
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
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(16u, 2u, 4u));
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE + 4u, 0x3333u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 0x0001u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 0x0003u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_CLOCK);
    expect(state, dspic33_dci_test_clock_word(cpu, 0u, false), "clock preloaded TXBUF0");
    expect(state, dspic33_dci_transmit(cpu, &transfer) && transfer.value == 0x1111u,
           "TXBUF preload before TSCON supplies active buffer zero");
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0u, false) &&
               dspic33_dci_test_clock_word(cpu, 0u, false),
           "advance mixed slots to preloaded TXBUF2");
    expect(state,
           dspic33_dci_transmit(cpu, &transfer) && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.value == 0x3333u,
           "TXBUF preload before TSCON supplies active buffer two");
}

void dspic33_dci_test_interrupt_dma_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_enable_interrupt(cpu, DCI_ERROR_IRQ, 4u);
    dspic33_dci_test_enable_interrupt(cpu, DCI_TRANSFER_IRQ, 3u);
    cpu->w[15] = 0x1800u;
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x3456u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x1234u, false), "clock IRQ fixture");
    expect(state, dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "block transfer raises IRQ60");
    expect(state, dspic33_dci_test_interrupt_set(cpu, DCI_ERROR_IRQ), "TX underflow raises IRQ59");
    expect(state, dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == DCI_ERROR_IRQ,
           "higher-priority DCI error vectors first");
    expect(state, cpu->pc == DCI_VECTOR, "DCI error uses IRQ59 vector");
    dspic33_dci_test_clear_interrupt(cpu, DCI_ERROR_IRQ);

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, 0x4000u, 0x7a5au);
    dspic33_dci_test_configure_dma(cpu, 1u, 0x2001u, 0x4000u, DCI_TRANSMIT_BASE, 0u,
                                   DCI_DMA_REQUEST);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 1u, 0u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0u, false), "clock initial TX DMA block");
    expect(state, dspic33_device_advance(cpu, 2u), "complete DCI TX DMA transfer");
    expect(state, dspic33_dci_test_clock_word(cpu, 0u, false), "clock loaded TX DMA block");
    expect(state, cpu->io.dci.last_transmit[0] == 0x7a5au, "DCI TX DMA copies RAM to TXBUF0");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_dma(cpu, 0u, 0x0001u, 0x4100u, DCI_RECEIVE_BASE, 0u,
                                   DCI_DMA_REQUEST);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x6b4bu, false), "clock RX DMA block");
    expect(state, dspic33_device_advance(cpu, 2u), "complete DCI RX DMA transfer");
    expect(state, dspic33_read_word(cpu, 0x4100u) == 0x6b4bu, "DCI RX DMA copies RXBUF0 to RAM");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_dma(cpu, 0u, 0x0001u, 0x4200u, DCI_RECEIVE_BASE, 0u,
                                   DCI_DMA_REQUEST);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 2u, 0u, 1u);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x7777u, false) && dspic33_device_advance(cpu, 2u),
           "clock BLEN greater than zero fixture");
    expect(state, dspic33_read_word(cpu, 0x4200u) == 0u,
           "BLEN greater than zero suppresses undefined DMA request");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_dma(cpu, 0u, 0x0001u, 0x4300u, DCI_RECEIVE_BASE, 0u, 0x3bu);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state,
           dspic33_dci_test_clock_word(cpu, 0x8888u, false) && dspic33_device_advance(cpu, 2u),
           "clock wrong IRQSEL fixture");
    expect(state, dspic33_read_word(cpu, 0x4300u) == 0u,
           "wrong IRQSEL receives no DCI DMA request");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_enable_interrupt(cpu, DCI_TRANSFER_IRQ, 3u);
    cpu->w[15] = 0x1800u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0xcccdu, false), "clock sleeping DCI fixture");
    expect(state, dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "sleeping external DCI sets IRQ60");
    expect(state, dspic33_device_advance(cpu, 1u), "advance sleeping DCI interrupt eligibility");
    expect(state,
           dspic33_device_wake(cpu) && cpu->last_interrupt == DCI_TRANSFER_IRQ &&
               cpu->pc == DCI_VECTOR,
           "external DCI wakes from Sleep through IRQ60 vector");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_enable_interrupt(cpu, DCI_TRANSFER_IRQ, 2u);
    cpu->sr = 0x0040u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0xdedeu, false),
           "clock equal-priority DCI fixture");
    expect(state,
           dspic33_device_wake(cpu) && cpu->interrupt_count == 0u &&
               dspic33_dci_test_interrupt_set(cpu, DCI_TRANSFER_IRQ),
           "equal-priority DCI wakes without vector and retains IRQ60");
}

void dspic33_dci_test_generation_and_frame_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
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
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_EXTERNAL_FRAME);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x1111u, true),
           "external FS before internal startup is accepted as physical input");
    expect(state,
           dspic33_device_advance(cpu, 96u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "pre-start and absent FS cannot arm internal-frame transfer");
    expect(state, dspic33_dci_test_clock_word(cpu, 0x2222u, true),
           "external FS arms initialized internal-frame transfer");
    expect(state,
           dspic33_device_advance(cpu, 63u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "internal-frame word remains pending before full interval");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x2222u,
           "internal-frame word completes after external FS");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    dspic33_write_word(cpu, DCI_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.pmd_disabled &&
               cpu->io.dci.pmd_generation == 2u,
           "rapid PMD toggle rejects stale disabled generation");
}

void dspic33_dci_test_disable_timing_cases(TestState* state, Dspic33* cpu) {
    Dspic33DciTransfer transfer;
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1111u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 12u) && cpu->io.dci.initialized,
           "initialize one-slot DCI frame");
    expect(state, dspic33_device_advance(cpu, 32u), "advance within active DCI slot zero");
    dspic33_dci_input(cpu, 0x2222u);
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state,
           cpu->io.dci.started && cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u &&
               cpu->events.count == 1u,
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
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 64u), "advance to three-clock DCI disable boundary");
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u,
           "three-clock DCI disable selects current frame");
    expect(state,
           dspic33_device_advance(cpu, 12u) && !cpu->io.dci.started && !cpu->io.dci.disable_pending,
           "three-clock DCI disable stops at current frame end");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(16u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x3333u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 65u), "advance inside final three DCI clocks");
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 2u,
           "late DCI disable selects following frame");
    expect(state,
           dspic33_device_advance(cpu, 11u) && cpu->io.dci.started && cpu->io.dci.disable_pending &&
               cpu->io.dci.disable_frames == 1u && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.slot == 0u,
           "late DCI disable completes current frame and continues");
    expect(state, dspic33_device_advance(cpu, 63u) && cpu->io.dci.started,
           "late DCI disable waits through following frame");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.dci.started &&
               !cpu->io.dci.disable_pending && dspic33_dci_transmit(cpu, &transfer) &&
               transfer.slot == 0u,
           "late DCI disable stops after following frame");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 2u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 3u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 12u), "initialize two-slot DCI frame");
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state, cpu->io.dci.disable_pending && cpu->io.dci.disable_frames == 1u,
           "early slot-zero disable retains multi-slot frame");
    expect(state, dspic33_device_advance(cpu, 16u) && cpu->io.dci.started && cpu->io.dci.slot == 1u,
           "multi-slot disable continues after slot zero");
    expect(state,
           dspic33_device_advance(cpu, 16u) && !cpu->io.dci.started && !cpu->io.dci.disable_pending,
           "multi-slot disable stops at frame end");
}

void dspic33_dci_test_internal_clock_lifecycle_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 5u), "advance DCI startup before PMD");
    dspic33_write_word(cpu, DCI_PMD, DCI_PMD_MASK);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.dci.pmd_disabled && cpu->events.count == 1u &&
               cpu->events.items[0].paused && cpu->events.items[0].paused_remaining == 6u,
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
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
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
           dspic33_device_advance(cpu, 50u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "BCG zero holds DCI word indefinitely");
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           dspic33_device_advance(cpu, 10u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "restored BCG waits to retained word boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xa000u,
           "restored BCG completes at retained word boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
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
           dspic33_device_advance(cpu, 50u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "Sleep holds internal DCI word indefinitely");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 10u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u,
           "DCI Sleep wake waits to retained word boundary");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xb000u,
           "DCI Sleep wake completes at retained word boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE | DCI_STOP_IDLE);
    expect(state, dspic33_device_advance(cpu, 17u), "advance DCI before stepped stopped Idle");
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
           dspic33_device_advance(cpu, 11u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xc000u,
           "DCISIDL wake completes retained DCI word");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 17u), "advance DCI before continuing Idle");
    dspic33_dci_input(cpu, 0xd000u);
    expect(state, dspic33_load_program_word(cpu, 0x0200u, OPCODE_IDLE),
           "load continuing DCI Idle instruction");
    cpu->pc = 0x0200u;
    expect(state, dspic33_step(cpu) == DSPIC33_IDLING && !cpu->events.items[0].paused,
           "DCISIDL clear keeps internal DCI clock running in Idle");
    expect(state,
           dspic33_device_advance(cpu, 10u) && dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0xd000u,
           "continuing Idle reaches original DCI word boundary");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_RECEIVE_SLOTS, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 17u), "advance DCI before paused disable");
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    dspic33_write_word(cpu, DCI_CONTROL1, 0u);
    expect(state,
           cpu->events.count == 1u && cpu->events.items[0].paused && cpu->io.dci.disable_pending &&
               cpu->io.dci.started,
           "DCI disable retains paused active frame");
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state,
           dspic33_device_advance(cpu, 11u) && !cpu->io.dci.started &&
               !cpu->io.dci.disable_pending && cpu->events.count == 0u,
           "restored DCI clock completes pending disabled frame");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_ENABLE);
    expect(state, dspic33_device_advance(cpu, 5u), "advance DCI before resume overflow");
    dspic33_write_word(cpu, DCI_CONTROL3, 0u);
    cpu->device_cycles = UINT64_MAX - 5u;
    dspic33_write_word(cpu, DCI_CONTROL3, 1u);
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR && cpu->events.items[0].paused,
           "DCI resume overflow leaves retained event paused");
    cpu->stop_reason = DSPIC33_RUNNING;
}

void dspic33_dci_test_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33DciTransfer transfer;
    bool initialized;
    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 2u, 1u, 1u, 1u);
    dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x1234u);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x5678u, false), "capture before DCI disable");
    dspic33_write_word(cpu, DCI_CONTROL1, DCI_EXTERNAL_CLOCK);
    expect(state, cpu->io.dci.disable_pending, "DCIEN clear defers through frame end");
    expect(state, dspic33_dci_test_clock_word(cpu, 0x9abcu, false), "finish disabled DCI frame");
    expect(state, !cpu->io.dci.disable_pending,
           "DCIEN clear completes deferred disable at frame end");
    expect(state, !cpu->io.dci.started, "DCIEN clear stops engine after current frame");
    expect(state, (dspic33_read_word(cpu, DCI_CONTROL1) & DCI_ENABLE) == 0u,
           "DCIEN remains clear after frame completion");

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    dspic33_dci_input(cpu, 0x4a4au);
    expect(state, dspic33_dci_test_clock_word(cpu, 0x5b5bu, false), "capture warm-reset RXBUF");
    dspic33_dci_input(cpu, 0x4a4au);
    dspic33_load_program_word(cpu, 0u, 0xfe0000u);
    expect(state, dspic33_step(cpu) == DSPIC33_RUNNING, "execute DCI warm reset");
    expect(state,
           dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0x5b5bu && cpu->io.dci.input == 0x4a4au &&
               dspic33_read_word(cpu, DCI_CONTROL1) == 0u,
           "warm reset preserves RXBUF and physical input but resets DCI engine");
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, DCI_RECEIVE_BASE) == 0u && cpu->io.dci.input == 0u,
           "POR clears deterministic RXBUF and physical input state");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize DCI copy destination");
    if (initialized) {
        dspic33_reset(cpu, 0u);
        dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 1u, 1u);
        dspic33_write_word(cpu, DCI_TRANSMIT_BASE, 0x2468u);
        expect(state, dspic33_dci_clock(cpu, 0x1357u, false, 2u) && dspic33_copy(&copy, cpu),
               "copy DCI state with pending clock event");
        expect(state,
               dspic33_device_advance(&copy, 2u) && dspic33_dci_transmit(&copy, &transfer) &&
                   transfer.value == 0x2468u &&
                   dspic33_read_word(&copy, DCI_RECEIVE_BASE) == 0x1357u && cpu->events.count == 1u,
               "copied DCI event executes independently");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    dspic33_dci_test_configure_external(cpu, 0u, 16u, 1u, 1u, 0u, 1u);
    cpu->io.dci.output.count = DSPIC33_DCI_QUEUE_SIZE;
    expect(state, !dspic33_dci_test_clock_word(cpu, 0xaaaau, false), "clock full output queue");
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
        expect(state, !dspic33_dci_clock(cpu, 0x1234u, false, 1u) && cpu->events.count == queued,
               "DCI clock scheduling failure queues no partial event");
        cpu->device_cycles = cycles;
    }

    dspic33_reset(cpu, 0u);
    {
        uint64_t cycles = cpu->device_cycles;
        cpu->device_cycles = UINT64_MAX;
        dspic33_write_word(cpu, DCI_CONTROL2, dspic33_dci_test_configuration(4u, 1u, 1u));
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
