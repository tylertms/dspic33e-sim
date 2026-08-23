#include "device/dspic33ep_mu/communication/pmp/internal.h"

void dspic33_pmp_test_dma_chain_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer = {0};
    uint16_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < PMP_TRANSFER_COUNT; index++) {
        dspic33_write_byte(cpu, (uint16_t)(PMP_DMA_SOURCE + index), (uint8_t)index);
    }
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_pmp_test_configure_dma(cpu, PMP_DMA_CHANNEL, PMP_DMA_REQUEST, PMP_DMA_SOURCE, PMP_DATA,
                                   PMP_TRANSFER_COUNT - 1u);
    dspic33_write_word(cpu, (uint16_t)(PMP_DMA_BASE + 2u), (uint16_t)(0x8000u | PMP_DMA_REQUEST));
    expect(state, dspic33_device_advance(cpu, PMP_TRANSFER_COUNT * 23u),
           "advance complete display DMA chain");
    expect(state, cpu->io.pmp.output.count == PMP_TRANSFER_COUNT,
           "display DMA emits complete frame");
    expect(state, (dspic33_read_word(cpu, PMP_DMA_BASE) & 0x8000u) == 0u,
           "display DMA one-shot channel disables");
    expect(state,
           cpu->io.dma_index[PMP_DMA_CHANNEL] == 0u &&
               (dspic33_read_word(cpu, 0x080eu) & 0x0100u) != 0u,
           "display DMA completes block and raises DMA10IF");
    for (index = 0u; index < PMP_TRANSFER_COUNT; index++) {
        bool available = dspic33_pmp_transmit(cpu, &transfer);
        if (!available || transfer.value != (uint8_t)index) {
            break;
        }
    }
    expect(state, index == PMP_TRANSFER_COUNT, "display DMA preserves all byte values");
    expect(state, !dspic33_pmp_transmit(cpu, &transfer), "display output queue drains");
}

void dspic33_pmp_test_dma_negative_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer = {0};
    uint8_t value;
    dspic33_reset(cpu, 0u);
    for (value = 0u; value < 4u; value++) {
        dspic33_write_byte(cpu, (uint16_t)(0x4000u + value), (uint8_t)(0xa0u + value));
    }
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_pmp_test_configure_dma(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, PMP_DATA, 3u);
    dspic33_write_word(cpu, 0x0b02u, (uint16_t)(0x8000u | PMP_DMA_REQUEST));
    expect(state, dspic33_device_advance(cpu, 100u), "advance alternate DMA channel");
    for (value = 0u; value < 4u; value++) {
        if (!dspic33_pmp_transmit(cpu, &transfer) || transfer.value != (uint8_t)(0xa0u + value)) {
            break;
        }
    }
    expect(state, value == 4u, "generic PMP request chains alternate DMA channel");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, 0x4000u, 0x51u);
    dspic33_write_byte(cpu, 0x4001u, 0x52u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_pmp_test_configure_dma(cpu, 0u, 0x2cu, 0x4000u, PMP_DATA, 1u);
    dspic33_write_word(cpu, 0x0b02u, 0x802cu);
    expect(state, dspic33_device_advance(cpu, 100u), "advance wrong request chain");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0x51u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "wrong IRQSEL stops after forced byte");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, 0x4000u, 0x61u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_pmp_test_configure_dma(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, (uint16_t)(PMP_DATA + 2u),
                                   0u);
    dspic33_write_word(cpu, 0x0b02u, (uint16_t)(0x8000u | PMP_DMA_REQUEST));
    expect(state, dspic33_device_advance(cpu, 30u), "advance wrong PAD transfer");
    expect(state, !dspic33_pmp_transmit(cpu, &transfer), "wrong PAD does not start PMP");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, 0x4000u, 0x71u);
    dspic33_write_byte(cpu, 0x4001u, 0x72u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_MASTER_MODE_2, 0u);
    dspic33_pmp_test_configure_dma(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, PMP_DATA, 1u);
    dspic33_write_word(cpu, 0x0b02u, (uint16_t)(0x8000u | PMP_DMA_REQUEST));
    expect(state, dspic33_device_advance(cpu, 30u), "advance IRQM zero chain");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0x71u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "IRQM zero stops after forced byte");
}

void dspic33_pmp_test_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33PmpTransfer original_transfer;
    Dspic33PmpTransfer copy_transfer;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize PMP copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x1111u);
    dspic33_write_byte(cpu, PMP_DATA, 0x81u);
    expect(state, dspic33_device_advance(cpu, 1u), "begin copied PMP transfer");
    expect(state, dspic33_copy(&copy, cpu), "copy active PMP");
    expect(state, copy.io.pmp.active && copy.io.pmp.value == 0x81u && copy.events.count == 2u,
           "copy retains PMP event state");
    expect(state, dspic33_device_advance(cpu, 20u) && dspic33_device_advance(&copy, 20u),
           "complete source and copied PMP");
    expect(state,
           dspic33_pmp_transmit(cpu, &original_transfer) &&
               dspic33_pmp_transmit(&copy, &copy_transfer) &&
               original_transfer.value == copy_transfer.value &&
               original_transfer.address == copy_transfer.address,
           "copied PMP completes independently");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x91u);
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    expect(state, !cpu->io.pmp.active && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP disable aborts active transfer");
    expect(state, dspic33_device_advance(cpu, 21u), "advance stale PMP event");
    expect(state, !dspic33_pmp_transmit(cpu, &original_transfer),
           "stale PMP event produces no output");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    cpu->io.pmp.output.count = DSPIC33_PMP_QUEUE_SIZE;
    dspic33_write_byte(cpu, PMP_DATA, 0xa1u);
    expect(state, !dspic33_device_advance(cpu, 21u), "full output queue stops advance");
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "full PMP output queue reports error");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_byte(cpu, PMP_DATA, 0xb1u);
    expect(state,
           !cpu->io.pmp.active && cpu->events.count == 0u &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP scheduling overflow rolls back transfer");
    dspic33_release(&copy);
}
void dspic33_pmp_test_legacy_slave_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, 0u);
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x008fu, "legacy slave begins empty");
    expect(state, dspic33_pmp_slave_write(cpu, 3u, 0x5au, 2u), "schedule legacy slave write");
    expect(state, dspic33_device_advance(cpu, 1u) && dspic33_read_byte(cpu, PMP_DATA) == 0u,
           "legacy slave write waits for external deadline");
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->data[PMP_DATA] == 0x5au &&
               dspic33_read_word(cpu, PMP_STATUS) == 0x808fu &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "legacy slave write captures low byte and interrupts");
    dspic33_write_word(cpu, 0x0804u, 0u);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0x61u, 0u), "schedule unread legacy overflow");
    expect(state,
           dspic33_device_advance(cpu, 0u) && dspic33_read_word(cpu, PMP_STATUS) == 0xc08fu &&
               cpu->data[PMP_DATA] == 0x5au,
           "legacy slave rejects unread overwrite");
    dspic33_write_word(cpu, PMP_STATUS, 0u);
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x808fu,
           "software clears legacy input overflow");
    expect(state,
           dspic33_read_byte(cpu, PMP_DATA) == 0x5au &&
               dspic33_read_word(cpu, PMP_STATUS) == 0x008fu,
           "legacy input read clears full status");

    dspic33_write_byte(cpu, PMP_ADDRESS, 0xa5u);
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x000fu,
           "legacy output write clears empty status");
    expect(state, dspic33_pmp_slave_read(cpu, 2u, 1u), "schedule legacy slave read");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.address == 0u && transfer.value == 0xa5u && transfer.width == 1u &&
               dspic33_read_word(cpu, PMP_STATUS) == 0x008fu,
           "legacy slave read transmits low output and marks empty");
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0xa5u &&
               dspic33_read_word(cpu, PMP_STATUS) == 0x00cfu,
           "legacy empty read returns latch and sets underflow");
    dspic33_write_byte(cpu, PMP_ADDRESS, 0x77u);
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x004fu,
           "legacy output refill retains underflow until software clear");
    dspic33_write_word(cpu, PMP_STATUS, 0u);
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x000fu,
           "software clears legacy output underflow");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, 0u);
    dspic33_write_word(cpu, PMP_ADDRESS_ENABLE, PMP_ADDRESS_INPUT_ENABLE);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x11u, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA] == 0u,
           "disabled slave chip select rejects write");
    dspic33_write_word(cpu, PMP_ADDRESS_ENABLE, PMP_CHIP_SELECT_ENABLE);
    dspic33_write_word(cpu, PMP_CONTROL, (uint16_t)(PMP_ENABLE | PMP_READ_STROBE_ENABLE));
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x22u, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA] == 0u,
           "disabled slave write strobe rejects write");
    expect(state, !dspic33_pmp_slave_read(cpu, 4u, 0u) && !dspic33_pmp_slave_write(cpu, 4u, 0u, 0u),
           "slave API rejects unavailable address lines");
}

void dspic33_pmp_test_buffered_slave_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t values[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    Dspic33PmpTransfer transfer = {0};
    uint8_t index;
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u,
                                         (uint16_t)(PMP_BUFFERED_SLAVE | PMP_INTERRUPT_LAST));
    dspic33_write_word(cpu, PMP_ADDRESS, 0x2211u);
    dspic33_write_word(cpu, PMP_OUTPUT_2, 0x4433u);
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0u,
           "buffered output writes fill every slot");
    for (index = 0u; index < 4u; index++) {
        dspic33_write_word(cpu, 0x0804u, 0u);
        expect(state,
               dspic33_pmp_slave_read(cpu, 3u, 1u) && dspic33_device_advance(cpu, 1u) &&
                   dspic33_pmp_transmit(cpu, &transfer),
               "buffered slave read completes");
        expect(state,
               transfer.address == index && transfer.value == values[index] &&
                   (dspic33_read_word(cpu, PMP_STATUS) & PMP_OUTPUT_BUFFER_MASK) ==
                       (uint16_t)((1u << (index + 1u)) - 1u),
               "buffered slave read advances output pointer and empty flags");
        expect(state,
               ((dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u) == (index == 3u),
               "buffered last-slot interrupt mode selects buffer three");
    }
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x008fu,
           "buffered fourth read raises aggregate empty");
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 0u) && dspic33_device_advance(cpu, 0u) &&
               dspic33_pmp_transmit(cpu, &transfer) && transfer.address == 0u &&
               transfer.value == 0x11u &&
               (dspic33_read_word(cpu, PMP_STATUS) & PMP_OUTPUT_UNDERFLOW) != 0u,
           "buffered read pointer wraps and reports underflow");
    dspic33_write_byte(cpu, PMP_ADDRESS, 0xa1u);
    expect(state,
           (dspic33_read_word(cpu, PMP_STATUS) & (PMP_OUTPUT_EMPTY | PMP_OUTPUT_BUFFER_MASK)) ==
               0x000eu,
           "buffered byte refill clears one empty flag and aggregate");

    dspic33_write_word(cpu, 0x0804u, 0u);
    for (index = 0u; index < 4u; index++) {
        expect(state,
               dspic33_pmp_slave_write(cpu, 0u, (uint8_t)(0x51u + index), 1u) &&
                   dspic33_device_advance(cpu, 1u),
               "buffered slave write completes");
        expect(state,
               cpu->data[PMP_DATA + index] == (uint8_t)(0x51u + index) &&
                   (dspic33_read_word(cpu, PMP_STATUS) & PMP_INPUT_BUFFER_MASK) ==
                       (uint16_t)(((1u << (index + 1u)) - 1u) << 8u),
               "buffered slave write advances input pointer and full flags");
    }
    expect(state,
           (dspic33_read_word(cpu, PMP_STATUS) & (PMP_INPUT_FULL | PMP_INTERRUPT_FLAG)) ==
                   PMP_INPUT_FULL &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "buffered fourth write raises aggregate full and interrupt");
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x99u, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA] == 0x51u &&
               (dspic33_read_word(cpu, PMP_STATUS) & PMP_INPUT_OVERFLOW) != 0u,
           "buffered full write wraps discards data and raises overflow");
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x5251u &&
               dspic33_read_word(cpu, PMP_INPUT_2) == 0x5453u &&
               (dspic33_read_word(cpu, PMP_STATUS) & (PMP_INPUT_FULL | PMP_INPUT_BUFFER_MASK)) ==
                   0u,
           "buffered word reads drain all input status bits");
}

void dspic33_pmp_test_addressable_slave_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t order[4] = {3u, 1u, 0u, 2u};
    static const uint8_t outputs[4] = {0x10u, 0x21u, 0x32u, 0x43u};
    Dspic33PmpTransfer transfer;
    uint8_t index;
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u,
                                         (uint16_t)(PMP_SLAVE_ADDRESSABLE | PMP_INTERRUPT_EACH));
    dspic33_write_word(cpu, PMP_ADDRESS, 0x2110u);
    dspic33_write_word(cpu, PMP_OUTPUT_2, 0x4332u);
    for (index = 0u; index < 4u; index++) {
        uint8_t address = order[index];
        dspic33_write_word(cpu, 0x0804u, 0u);
        expect(state,
               dspic33_pmp_slave_read(cpu, address, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_pmp_transmit(cpu, &transfer) && transfer.address == address &&
                   transfer.value == outputs[address],
               "addressable slave read selects requested output byte");
        expect(state,
               (dspic33_read_word(cpu, PMP_STATUS) & (uint16_t)(1u << address)) != 0u &&
                   (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
               "addressable read sets selected empty bit and each interrupt");
    }
    expect(state,
           (dspic33_read_word(cpu, PMP_STATUS) & (PMP_OUTPUT_EMPTY | PMP_OUTPUT_BUFFER_MASK)) ==
               (PMP_OUTPUT_EMPTY | PMP_OUTPUT_BUFFER_MASK),
           "addressable reads derive aggregate empty");
    for (index = 0u; index < 4u; index++) {
        uint8_t address = order[index];
        expect(state,
               dspic33_pmp_slave_write(cpu, address, (uint8_t)(0x80u + address), 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   cpu->data[PMP_DATA + address] == (uint8_t)(0x80u + address) &&
                   (dspic33_read_word(cpu, PMP_STATUS) & (uint16_t)(1u << (8u + address))) != 0u,
               "addressable slave write selects requested input byte");
    }
    expect(state,
           (dspic33_read_word(cpu, PMP_STATUS) & (PMP_INPUT_FULL | PMP_INPUT_BUFFER_MASK)) ==
               (PMP_INPUT_FULL | PMP_INPUT_BUFFER_MASK),
           "addressable writes derive aggregate full");
    expect(state,
           dspic33_pmp_slave_write(cpu, 2u, 0xeeu, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA + 2u] == 0x82u &&
               (dspic33_read_word(cpu, PMP_STATUS) & PMP_INPUT_OVERFLOW) != 0u,
           "addressable full slot rejects overwrite");
    expect(state,
           dspic33_read_byte(cpu, PMP_DATA + 2u) == 0x82u &&
               (dspic33_read_word(cpu, PMP_STATUS) & (PMP_INPUT_FULL | (uint16_t)(1u << 10u))) ==
                   0u,
           "addressable input byte read clears selected and aggregate full");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_SLAVE_ADDRESSABLE);
    dspic33_write_word(cpu, PMP_ADDRESS_ENABLE, PMP_CHIP_SELECT_ENABLE);
    expect(state,
           dspic33_pmp_slave_write(cpu, 3u, 0x77u, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA + 3u] == 0u,
           "addressable slave requires both address input enables");
}

void dspic33_pmp_test_power_management_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    uint16_t address;
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x1234u);
    dspic33_write_byte(cpu, PMP_DATA, 0x61u);
    expect(state, dspic33_device_advance(cpu, 5u), "advance master before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->io.pmp.active && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u,
           "Sleep preserves active master state");
    expect(state,
           dspic33_device_advance(cpu, 100u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               cpu->io.pmp.active,
           "Sleep suspends master transfer indefinitely");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 15u) &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "wake resumes master through retained final BUSY cycle");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x61u && transfer.cycle == 121u,
           "wake completes master after retained remaining delay");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_control(cpu, PMP_STOP_IDLE,
                                           (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x62u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 20u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               cpu->io.pmp.active,
           "PSIDL stops master in Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x62u,
           "Idle exit resumes retained master transfer");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_control(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x63u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x63u,
           "PSIDL clear continues master in Idle");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x3456u);
    dspic33_write_byte(cpu, PMP_DATA, 0x64u);
    expect(state, dspic33_device_advance(cpu, 4u), "advance master before PMD");
    address = dspic33_read_word(cpu, PMP_ADDRESS);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, !cpu->io.pmp.pmd_disabled && dspic33_read_word(cpu, PMP_ADDRESS) == address,
           "PMPMD write retains one-cycle access window");
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled &&
               dspic33_read_word(cpu, PMP_CONTROL) == 0u,
           "PMPMD becomes effective after one cycle and hides registers");
    dspic33_write_word(cpu, PMP_ADDRESS, 0x7777u);
    expect(state,
           dspic33_read_word(cpu, PMP_ADDRESS) == 0u &&
               (uint16_t)(cpu->data[PMP_ADDRESS] | ((uint16_t)cpu->data[PMP_ADDRESS + 1u] << 8u)) ==
                   address,
           "PMPMD rejects hidden register writes");
    expect(state,
           dspic33_device_advance(cpu, 100u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               cpu->io.pmp.active,
           "PMPMD suspends active master transfer");
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state, cpu->io.pmp.pmd_disabled && dspic33_read_word(cpu, PMP_CONTROL) == 0u,
           "PMPMD clear retains one-cycle disabled window");
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled &&
               dspic33_read_word(cpu, PMP_ADDRESS) == address,
           "PMPMD clear restores register access after one cycle");
    expect(state,
           dspic33_device_advance(cpu, 16u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x64u,
           "PMPMD re-enable resumes retained master delay");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled &&
               dspic33_read_word(cpu, PMP_PMD) == 0u,
           "stale PMP PMD transition cannot override latest request");
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state,
           (dspic33_read_word(cpu, PMP_PMD) & PMP_MODULE_DISABLE) == 0u &&
               !cpu->io.pmp.pmd_disabled && cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "PMP PMD scheduling failure rolls back request");
}

void dspic33_pmp_test_slave_power_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33PmpTransfer original;
    Dspic33PmpTransfer cloned;
    uint16_t priority_address = (uint16_t)(0x0840u + (PMP_IRQ / 4u) * 2u);
    uint16_t priority_shift = (uint16_t)((PMP_IRQ % 4u) * 4u);
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize slave PMP copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, PMP_STOP_IDLE, 0u);
    dspic33_write_word(cpu, 0x0824u, PMP_INTERRUPT_ENABLE);
    dspic33_write_word(
        cpu, priority_address,
        (uint16_t)((dspic33_read_word(cpu, priority_address) & ~(7u << priority_shift)) |
                   (PMP_PRIORITY << priority_shift)));
    cpu->program[(0x0014u + PMP_IRQ * 2u) / 2u] = PMP_VECTOR;
    cpu->w[15] = 0x1800u;
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0x71u, 2u), "schedule sleeping slave write");
    expect(state,
           dspic33_device_advance(cpu, 2u) && cpu->data[PMP_DATA] == 0x71u &&
               dspic33_device_wake(cpu) && cpu->last_interrupt == PMP_IRQ && cpu->pc == PMP_VECTOR,
           "slave write completes and wakes from Sleep");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, PMP_STOP_IDLE, 0u);
    dspic33_write_byte(cpu, PMP_ADDRESS, 0x72u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 1u) && dspic33_device_advance(cpu, 1u) &&
               dspic33_pmp_transmit(cpu, &original) && original.value == 0x72u,
           "PSIDL stopped Idle retains asynchronous slave reads");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_SLAVE_ADDRESSABLE);
    expect(state,
           dspic33_pmp_slave_write(cpu, 2u, 0x81u, 3u) && dspic33_pmp_slave_read(cpu, 1u, 4u) &&
               dspic33_copy(&copy, cpu),
           "copy pending slave transactions");
    dspic33_write_byte(cpu, PMP_ADDRESS + 1u, 0x91u);
    dspic33_write_byte(&copy, PMP_ADDRESS + 1u, 0xa1u);
    expect(state,
           dspic33_device_advance(cpu, 4u) && dspic33_device_advance(&copy, 4u) &&
               cpu->data[PMP_DATA + 2u] == 0x81u && copy.data[PMP_DATA + 2u] == 0x81u &&
               dspic33_pmp_transmit(cpu, &original) && dspic33_pmp_transmit(&copy, &cloned) &&
               original.value == 0x91u && cloned.value == 0xa1u,
           "copied slave events complete independently");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, 0u);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0x82u, 5u), "schedule slave write before reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_device_advance(cpu, 5u) && cpu->data[PMP_DATA] == 0u && cpu->events.count == 0u,
           "reset cancels slave transaction and restores state");

    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, 0u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 1u), "disable PMP before slave event");
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x83u, 1u) && dspic33_device_advance(cpu, 1u) &&
               cpu->data[PMP_DATA] == 0u,
           "PMPMD drops external slave event while disabled");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, 0u);
    cpu->io.pmp.output.count = DSPIC33_PMP_QUEUE_SIZE;
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 0u) && !dspic33_device_advance(cpu, 0u) &&
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "full PMP output queue reports slave read failure");
    dspic33_release(&copy);
}

void dspic33_pmp_test_slave_mode_matrix_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t requests[3] = {
        0u,
        PMP_INTERRUPT_EACH,
        PMP_INTERRUPT_LAST,
    };
    static const uint16_t invalid_modes[5] = {
        PMP_INCREMENT,
        PMP_DECREMENT,
        (uint16_t)(PMP_SLAVE_ADDRESSABLE | PMP_INCREMENT),
        (uint16_t)(PMP_SLAVE_ADDRESSABLE | PMP_DECREMENT),
        (uint16_t)(PMP_SLAVE_ADDRESSABLE | PMP_BUFFERED_SLAVE),
    };
    uint8_t index;
    for (index = 0u; index < 3u; index++) {
        dspic33_reset(cpu, 0u);
        dspic33_pmp_test_configure_pmp_slave(cpu, 0u, requests[index]);
        expect(state,
               dspic33_pmp_slave_write(cpu, 0u, (uint8_t)(0x20u + index), 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   cpu->data[PMP_DATA] == (uint8_t)(0x20u + index) &&
                   (dspic33_read_word(cpu, PMP_STATUS) & PMP_INPUT_FULL) != 0u,
               "legacy slave admits every IRQM encoding");
        expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
               "legacy slave interrupts independently of IRQM");

        dspic33_reset(cpu, 0u);
        dspic33_pmp_test_configure_pmp_slave(cpu, 0u,
                                             (uint16_t)(PMP_BUFFERED_SLAVE | requests[index]));
        expect(state,
               dspic33_pmp_slave_write(cpu, 0u, (uint8_t)(0x30u + index), 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   cpu->data[PMP_DATA] == (uint8_t)(0x30u + index),
               "buffered slave admits documented IRQM encoding");
        expect(state,
               ((dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u) ==
                   (requests[index] == PMP_INTERRUPT_EACH),
               "buffered slave IRQM selects first-slot interrupt");

        dspic33_reset(cpu, 0u);
        dspic33_pmp_test_configure_pmp_slave(cpu, 0u,
                                             (uint16_t)(PMP_SLAVE_ADDRESSABLE | requests[index]));
        expect(state,
               dspic33_pmp_slave_write(cpu, 2u, (uint8_t)(0x40u + index), 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   cpu->data[PMP_DATA + 2u] == (uint8_t)(0x40u + index),
               "addressable slave admits documented IRQM encoding");
        expect(state,
               ((dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u) ==
                   (requests[index] == PMP_INTERRUPT_EACH),
               "addressable slave IRQM selects nonfinal address interrupt");
        dspic33_write_word(cpu, 0x0804u, 0u);
        expect(state, dspic33_pmp_slave_read(cpu, 3u, 0u) && dspic33_device_advance(cpu, 0u),
               "addressable slave final-address read completes");
        expect(state,
               ((dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u) ==
                   (requests[index] == PMP_INTERRUPT_EACH || requests[index] == PMP_INTERRUPT_LAST),
               "addressable slave IRQM selects final address interrupt");
    }
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_RESERVED);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x2fu, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA] == 0x2fu,
           "legacy slave applies deterministic reserved IRQM policy");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "legacy reserved IRQM policy retains unconditional interrupt");
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u,
                                         (uint16_t)(PMP_BUFFERED_SLAVE | PMP_INTERRUPT_RESERVED));
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x3fu, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA] == 0x3fu,
           "buffered slave applies deterministic reserved IRQM policy");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
           "buffered reserved IRQM policy raises no interrupt");
    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(
        cpu, 0u, (uint16_t)(PMP_SLAVE_ADDRESSABLE | PMP_INTERRUPT_RESERVED));
    expect(state,
           dspic33_pmp_slave_write(cpu, 2u, 0x4fu, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA + 2u] == 0x4fu,
           "addressable slave applies deterministic reserved IRQM policy");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
           "addressable reserved IRQM write raises no interrupt");
    expect(state, dspic33_pmp_slave_read(cpu, 3u, 0u) && dspic33_device_advance(cpu, 0u),
           "addressable reserved IRQM final read completes");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
           "addressable reserved IRQM final read raises no interrupt");
    for (index = 0u; index < 5u; index++) {
        dspic33_reset(cpu, 0u);
        dspic33_pmp_test_configure_pmp_slave(cpu, 0u, invalid_modes[index]);
        expect(state, dspic33_pmp_slave_write(cpu, 0u, 0x5au, 0u),
               "schedule undocumented slave mode input");
        expect(state,
               dspic33_device_advance(cpu, 0u) && cpu->data[PMP_DATA] == 0u &&
                   dspic33_read_word(cpu, PMP_STATUS) == 0x008fu,
               "undocumented slave mode rejects traffic");
        expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
               "undocumented slave mode raises no interrupt");
    }
}

void dspic33_pmp_test_slave_dma_isolation_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t modes[3] = {
        0u,
        PMP_BUFFERED_SLAVE,
        PMP_SLAVE_ADDRESSABLE,
    };
    Dspic33PmpTransfer transfer;
    uint8_t index;
    for (index = 0u; index < 3u; index++) {
        uint8_t address = modes[index] == PMP_SLAVE_ADDRESSABLE ? 2u : 0u;
        dspic33_reset(cpu, 0u);
        dspic33_write_byte(cpu, PMP_DMA_SOURCE, 0xa5u);
        dspic33_write_byte(cpu, PMP_DMA_SOURCE + 1u, 0xa6u);
        dspic33_pmp_test_configure_pmp_slave(cpu, 0u,
                                             (uint16_t)(modes[index] | PMP_INTERRUPT_EACH));
        dspic33_pmp_test_configure_dma(cpu, 0u, PMP_DMA_REQUEST, PMP_DMA_SOURCE, PMP_DATA, 1u);
        expect(state,
               dspic33_pmp_slave_write(cpu, address, (uint8_t)(0x60u + index), 0u) &&
                   dspic33_device_advance(cpu, 0u),
               "slave write completes with waiting PMP DMA channel");
        expect(state,
               (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u && cpu->io.dma_index[0] == 0u &&
                   cpu->data[PMP_DATA + address] == (uint8_t)(0x60u + index),
               "slave write raises no PMP DMA request");
        dspic33_write_byte(cpu, PMP_ADDRESS + address, (uint8_t)(0x70u + index));
        expect(state,
               dspic33_pmp_slave_read(cpu, address, 0u) && dspic33_device_advance(cpu, 0u) &&
                   dspic33_pmp_transmit(cpu, &transfer) &&
                   transfer.value == (uint8_t)(0x70u + index),
               "slave read completes with waiting PMP DMA channel");
        expect(state,
               (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) != 0u && cpu->io.dma_index[0] == 0u &&
                   cpu->data[PMP_DMA_SOURCE] == 0xa5u,
               "slave read raises no PMP DMA request");
    }
}

void dspic33_pmp_test_pmp_extended_lifecycle_cases(TestState* state, Dspic33* cpu) {
    static const uint16_t registers[8] = {
        PMP_CONTROL, PMP_MODE,    PMP_ADDRESS,        PMP_OUTPUT_2,
        PMP_DATA,    PMP_INPUT_2, PMP_ADDRESS_ENABLE, PMP_STATUS,
    };
    Dspic33 copy;
    Dspic33PmpTransfer source_transfer;
    Dspic33PmpTransfer copy_transfer;
    uint8_t index;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize extended PMP copy");
    if (!initialized) {
        return;
    }

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    dspic33_write_word(cpu, PMP_ADDRESS, 0x2211u);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x31u, 0u) && dspic33_pmp_slave_read(cpu, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && cpu->io.pmp.slave_read_index == 1u &&
               cpu->io.pmp.slave_write_index == 1u,
           "buffered slave establishes sequential pointer state");
    expect(state, dspic33_copy(&copy, cpu), "copy buffered slave pointer state");
    dspic33_write_byte(cpu, PMP_ADDRESS + 1u, 0x42u);
    dspic33_write_byte(&copy, PMP_ADDRESS + 1u, 0x52u);
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 0u) && dspic33_pmp_slave_read(&copy, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && dspic33_device_advance(&copy, 0u) &&
               dspic33_pmp_transmit(cpu, &source_transfer) &&
               dspic33_pmp_transmit(cpu, &source_transfer) &&
               dspic33_pmp_transmit(&copy, &copy_transfer) &&
               dspic33_pmp_transmit(&copy, &copy_transfer) && source_transfer.value == 0x42u &&
               copy_transfer.value == 0x52u,
           "copied buffered pointers advance independently");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0x61u, 0u) && dspic33_device_advance(cpu, 0u),
           "seed slave state before PMPEN disable");
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    expect(state,
           cpu->data[PMP_DATA] == 0x61u &&
               (dspic33_pmp_test_raw_data_word(cpu, PMP_STATUS) & 0x0100u) != 0u &&
               cpu->io.pmp.slave_write_index == 1u,
           "PMPEN disable retains slave data status and pointer");
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    expect(state,
           dspic33_pmp_test_raw_data_word(cpu, PMP_STATUS) == 0x008fu &&
               cpu->io.pmp.slave_read_index == 0u && cpu->io.pmp.slave_write_index == 0u,
           "PMPEN re-enable resets slave status and pointers");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x71u, 0u) && dspic33_pmp_slave_read(cpu, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "seed slave state before PMD disable");
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled &&
               cpu->io.pmp.slave_read_index == 1u && cpu->io.pmp.slave_write_index == 1u &&
               (dspic33_pmp_test_raw_data_word(cpu, PMP_STATUS) & 0x0101u) == 0x0101u,
           "PMPMD retains slave status and pointers while disabled");
    for (index = 0u; index < 8u; index++) {
        uint16_t before = dspic33_pmp_test_raw_data_word(cpu, registers[index]);
        expect(state, dspic33_read_word(cpu, registers[index]) == 0u,
               "disabled PMP read follows deterministic zero policy");
        dspic33_write_word(cpu, registers[index], (uint16_t)(0xa500u + index));
        expect(state, dspic33_pmp_test_raw_data_word(cpu, registers[index]) == before,
               "disabled PMP write preserves backing state");
    }
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled &&
               cpu->io.pmp.slave_read_index == 1u && cpu->io.pmp.slave_write_index == 1u &&
               (dspic33_read_word(cpu, PMP_STATUS) & 0x0101u) == 0x0101u,
           "PMPMD re-enable reveals retained slave state");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled,
           "establish stable PMP PMD disable before warm reset");
    expect(state,
           dspic33_load_program_word(cpu, 0u, OPCODE_RESET) &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, PMP_PMD) & PMP_MODULE_DISABLE) == 0u &&
               !cpu->io.pmp.pmd_disabled && dspic33_read_word(cpu, PMP_STATUS) == 0x008fu,
           "warm reset clears PMP PMD and restores PMP state");
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x79u, 0u) && dspic33_pmp_slave_read(cpu, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u) && cpu->io.pmp.slave_read_index == 1u &&
               cpu->io.pmp.slave_write_index == 1u,
           "seed PMP runtime before enabled warm reset");
    expect(state,
           dspic33_load_program_word(cpu, 0u, OPCODE_RESET) &&
               dspic33_step(cpu) == DSPIC33_RUNNING &&
               (dspic33_read_word(cpu, PMP_PMD) & PMP_MODULE_DISABLE) == 0u &&
               !cpu->io.pmp.pmd_disabled && dspic33_read_word(cpu, PMP_STATUS) == 0x008fu &&
               cpu->io.pmp.slave_read_index == 0u && cpu->io.pmp.slave_write_index == 0u,
           "warm reset clears enabled PMP runtime state");

    dspic33_reset(cpu, 0u);
    dspic33_pmp_test_configure_pmp_slave(cpu, 0u, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(state,
           !dspic33_pmp_slave_write(cpu, 0u, 0x81u, 1u) && !dspic33_pmp_slave_read(cpu, 0u, 1u) &&
               cpu->events.count == 0u,
           "slave scheduling overflow rolls back without state");
    dspic33_release(&copy);
}
