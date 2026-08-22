#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"
#include "test.h"

enum {
    PMP_CONTROL = 0x0600u,
    PMP_MODE = 0x0602u,
    PMP_ADDRESS = 0x0604u,
    PMP_OUTPUT_2 = 0x0606u,
    PMP_DATA = 0x0608u,
    PMP_INPUT_2 = 0x060au,
    PMP_ADDRESS_ENABLE = 0x060cu,
    PMP_STATUS = 0x060eu,
    PMP_PMD = 0x0764u,
    PMP_ENABLE = 0x8000u,
    PMP_STOP_IDLE = 0x2000u,
    PMP_READ_STROBE_ENABLE = 0x0100u,
    PMP_WRITE_STROBE_ENABLE = 0x0200u,
    PMP_CHIP_SELECT_ENABLE = 0x4000u,
    PMP_ADDRESS_INPUT_ENABLE = 0x0003u,
    PMP_BUSY = 0x8000u,
    PMP_INTERRUPT_EACH = 0x2000u,
    PMP_INTERRUPT_RESERVED = 0x4000u,
    PMP_INTERRUPT_LAST = 0x6000u,
    PMP_INCREMENT = 0x0800u,
    PMP_DECREMENT = 0x1000u,
    PMP_DATA_16_BIT = 0x0400u,
    PMP_SLAVE_ADDRESSABLE = 0x0100u,
    PMP_MASTER_MODE_2 = 0x0200u,
    PMP_MASTER_MODE_3 = 0x0300u,
    PMP_BUFFERED_SLAVE = 0x1800u,
    PMP_PARTIAL_MUX = 0x0800u,
    PMP_FULL_MUX = 0x1000u,
    PMP_ONE_CHIP_SELECT = 0x0040u,
    PMP_TWO_CHIP_SELECTS = 0x0080u,
    PMP_FIRMWARE_MODE = 0x22beu,
    PMP_INTERRUPT_FLAG = 0x2000u,
    PMP_INTERRUPT_ENABLE = 0x2000u,
    PMP_IRQ = 45u,
    PMP_PRIORITY = 3u,
    PMP_VECTOR = 0x0100u,
    PMP_DMA_REQUEST = 0x2du,
    PMP_DMA_CHANNEL = 10u,
    PMP_DMA_BASE = 0x0ba0u,
    PMP_DMA_SOURCE = 0x2000u,
    PMP_TRANSFER_COUNT = 8192u,
    PMP_MODULE_DISABLE = 0x0100u,
    PMP_INPUT_FULL = 0x8000u,
    PMP_INPUT_OVERFLOW = 0x4000u,
    PMP_INPUT_BUFFER_MASK = 0x0f00u,
    PMP_OUTPUT_EMPTY = 0x0080u,
    PMP_OUTPUT_UNDERFLOW = 0x0040u,
    PMP_OUTPUT_BUFFER_MASK = 0x000fu,
    OPCODE_POWER_SAVE_SLEEP = 0xfe4000u,
    OPCODE_RESET = 0xfe0000u
};

static uint16_t raw_data_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static void configure_pmp_control(Dspic33* cpu, uint16_t control, uint16_t mode, uint16_t address) {
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    dspic33_write_word(cpu, PMP_MODE, mode);
    dspic33_write_word(cpu, PMP_ADDRESS, address);
    dspic33_write_word(cpu, PMP_CONTROL, (uint16_t)(control | PMP_ENABLE));
}

static void configure_pmp(Dspic33* cpu, uint16_t mode, uint16_t address) {
    configure_pmp_control(cpu, 0u, mode, address);
}

static void configure_dma(Dspic33* cpu, uint8_t channel, uint8_t request, uint32_t source,
                          uint16_t pad, uint16_t count) {
    uint16_t base = (uint16_t)(0x0b00u + channel * 0x10u);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), (uint16_t)source);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(source >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0au), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), count);
    dspic33_write_word(cpu, base, 0xe001u);
}

static void configure_dma_read(Dspic33* cpu, uint8_t channel, uint8_t request, uint32_t destination,
                               uint16_t pad, uint16_t count) {
    uint16_t base = (uint16_t)(0x0b00u + channel * 0x10u);
    dspic33_write_word(cpu, base, 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 2u), request);
    dspic33_write_word(cpu, (uint16_t)(base + 4u), (uint16_t)destination);
    dspic33_write_word(cpu, (uint16_t)(base + 6u), (uint16_t)(destination >> 16u));
    dspic33_write_word(cpu, (uint16_t)(base + 8u), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0au), 0u);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0cu), pad);
    dspic33_write_word(cpu, (uint16_t)(base + 0x0eu), count);
    dspic33_write_word(cpu, base, 0x8001u);
}

static uint16_t pmp_data_latch(const Dspic33* cpu) {
    return (uint16_t)(cpu->data[PMP_DATA] | ((uint16_t)cpu->data[PMP_DATA + 1u] << 8u));
}

static void configure_pmp_read(Dspic33* cpu, uint16_t control, uint16_t mode, uint16_t address,
                               uint16_t previous) {
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    dspic33_write_word(cpu, PMP_DATA, previous);
    configure_pmp_control(cpu, control, mode, address);
}

static void configure_pmp_slave(Dspic33* cpu, uint16_t control, uint16_t mode) {
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    dspic33_write_word(cpu, PMP_MODE, mode);
    dspic33_write_word(cpu, PMP_ADDRESS_ENABLE,
                       (uint16_t)(PMP_CHIP_SELECT_ENABLE | PMP_ADDRESS_INPUT_ENABLE));
    dspic33_write_word(
        cpu, PMP_CONTROL,
        (uint16_t)(control | PMP_ENABLE | PMP_READ_STROBE_ENABLE | PMP_WRITE_STROBE_ENABLE));
}

static void access_cases(TestState* state, Dspic33* cpu) {
    dspic33_reset(cpu, 0u);
    expect(state, dspic33_read_word(cpu, PMP_CONTROL) == 0u, "PMCON reset");
    expect(state, dspic33_read_word(cpu, PMP_MODE) == 0u, "PMMODE reset");
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x008fu, "PMSTAT reset");
    dspic33_write_word(cpu, PMP_CONTROL, 0xffffu);
    dspic33_write_word(cpu, PMP_MODE, 0xffffu);
    dspic33_write_word(cpu, PMP_STATUS, 0xffffu);
    expect(state, dspic33_read_word(cpu, PMP_CONTROL) == 0xbfffu, "PMCON mask");
    expect(state, dspic33_read_word(cpu, PMP_MODE) == 0x7fffu, "PMMODE BUSY read only");
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x40cfu, "PMSTAT access mask");
}

static void timing_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, (uint16_t)(PMP_INTERRUPT_EACH | PMP_MASTER_MODE_2), 0x1234u);
    dspic33_write_byte(cpu, PMP_DATA, 0x5au);
    expect(state, cpu->io.pmp.active, "WAITM zero starts transfer");
    expect(state, (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "one-cycle transfer has no sampled BUSY interval");
    expect(state, !dspic33_pmp_transmit(cpu, &transfer), "transfer unavailable before completion");
    expect(state, dspic33_device_advance(cpu, 1u), "advance one-cycle transfer");
    expect(state, dspic33_pmp_transmit(cpu, &transfer), "one-cycle transfer completes");
    expect(state, transfer.address == 0x1234u && transfer.value == 0x5au && transfer.cycle == 1u,
           "one-cycle transfer captures address data and cycle");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x4567u);
    dspic33_write_byte(cpu, PMP_DATA, 0x11u);
    expect(state, (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u,
           "firmware timing asserts BUSY");
    dspic33_write_byte(cpu, PMP_DATA, 0x22u);
    dspic33_write_word(cpu, PMP_ADDRESS, 0x7654u);
    dspic33_write_word(cpu, PMP_MODE, PMP_MASTER_MODE_2);
    expect(state, dspic33_device_advance(cpu, 20u), "advance before firmware completion");
    expect(state,
           (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "final cycle clears BUSY before completion");
    dspic33_write_word(cpu, PMP_MODE, PMP_FIRMWARE_MODE);
    dspic33_write_byte(cpu, PMP_DATA, 0x33u);
    expect(state, (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u,
           "final cycle admits back-to-back transfer");
    expect(state, dspic33_device_advance(cpu, 1u), "advance firmware completion");
    expect(state, dspic33_pmp_transmit(cpu, &transfer), "firmware transfer completes");
    expect(state,
           transfer.address == 0x4567u && transfer.value == 0x11u &&
               transfer.mode == PMP_FIRMWARE_MODE && transfer.cycle == 21u,
           "active transfer retains captured configuration");
    expect(state,
           !dspic33_pmp_transmit(cpu, &transfer) && cpu->io.pmp.active &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u,
           "first completion preserves back-to-back transfer");
    expect(state, dspic33_device_advance(cpu, 19u), "advance back-to-back final cycle");
    expect(state,
           (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "back-to-back final cycle clears BUSY before completion");
    expect(state, dspic33_device_advance(cpu, 1u), "advance back-to-back completion");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.address == 0x7654u &&
               transfer.value == 0x33u && transfer.cycle == 41u,
           "back-to-back transfer completes independently");
    expect(state, !dspic33_pmp_transmit(cpu, &transfer), "busy write does not queue transfer");
}

static void access_lane_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_MASTER_MODE_2, 0x2345u);
    dspic33_write_byte(cpu, (uint16_t)(PMP_DATA + 1u), 0xaau);
    expect(state, !cpu->io.pmp.active && cpu->events.count == 0u,
           "high-byte write does not initiate transfer");
    dspic33_write_word(cpu, PMP_DATA, 0xab5cu);
    expect(state, cpu->io.pmp.active, "word write initiates low-byte transfer");
    expect(state, dspic33_device_advance(cpu, 1u), "word write transfer completes");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0x5cu &&
               transfer.address == 0x2345u,
           "word write captures low byte");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_MASTER_MODE_3, 0x3456u);
    dspic33_write_byte(cpu, PMP_DATA, 0x6du);
    expect(state, dspic33_device_advance(cpu, 1u), "master mode three completes");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0x6du &&
               transfer.address == 0x3456u,
           "master mode three emits transfer");
}

static void sixteen_bit_lane_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0x6789u);
    dspic33_write_byte(cpu, (uint16_t)(PMP_DATA + 1u), 0xaau);
    expect(state, !cpu->io.pmp.active && cpu->events.count == 0u,
           "16-bit high-byte write only loads the data latch");
    dspic33_write_byte(cpu, PMP_DATA, 0x55u);
    expect(state, cpu->io.pmp.active && cpu->io.pmp.value == 0xaa55u && cpu->io.pmp.width == 2u,
           "16-bit low-byte write starts both byte phases");
    dspic33_write_word(cpu, PMP_DATA, 0x1234u);
    expect(state, dspic33_read_word(cpu, PMP_DATA) == 0xaa55u,
           "16-bit write while BUSY leaves the active data latch unchanged");
    expect(state, dspic33_device_advance(cpu, 2u), "advance latched 16-bit byte write");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0xaa55u &&
               transfer.width == 2u && transfer.address == 0x6789u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "16-bit byte-lane transfer completes exactly once");
}

typedef struct {
    uint16_t control;
    uint16_t mode;
    uint16_t expected_value;
    uint8_t width;
    uint8_t cycles;
} PmpMasterWriteCase;

static void master_write_matrix_cases(TestState* state, Dspic33* cpu) {
    static const PmpMasterWriteCase cases[] = {
        {0u, PMP_MASTER_MODE_2, 0x005cu, 1u, 1u},
        {PMP_PARTIAL_MUX, PMP_MASTER_MODE_2, 0x005cu, 1u, 2u},
        {PMP_FULL_MUX, PMP_MASTER_MODE_2, 0x005cu, 1u, 3u},
        {0u, PMP_MASTER_MODE_3, 0x005cu, 1u, 1u},
        {PMP_PARTIAL_MUX, PMP_MASTER_MODE_3, 0x005cu, 1u, 2u},
        {PMP_FULL_MUX, PMP_MASTER_MODE_3, 0x005cu, 1u, 3u},
        {0u, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0xab5cu, 2u, 2u},
        {PMP_PARTIAL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0xab5cu, 2u, 3u},
        {PMP_FULL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0xab5cu, 2u, 4u},
        {0u, PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 2u},
        {PMP_PARTIAL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 3u},
        {PMP_FULL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 4u},
    };
    Dspic33PmpTransfer transfer;
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        const PmpMasterWriteCase* current = &cases[index];
        dspic33_reset(cpu, 0u);
        configure_pmp_control(cpu, current->control, current->mode, 0x2468u);
        dspic33_write_word(cpu, PMP_DATA, 0xab5cu);
        expect(state,
               cpu->io.pmp.active &&
                   ((dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u) == (current->cycles > 1u),
               "master write matrix starts with documented BUSY state");
        expect(state, dspic33_device_advance(cpu, current->cycles - 1u),
               "advance master write matrix before completion");
        expect(state,
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
                   !dspic33_pmp_transmit(cpu, &transfer),
               "master write matrix clears BUSY on its final cycle");
        expect(state, dspic33_device_advance(cpu, 1u), "advance master write matrix completion");
        expect(state,
               dspic33_pmp_transmit(cpu, &transfer) && transfer.address == 0x2468u &&
                   transfer.control == (uint16_t)(current->control | PMP_ENABLE) &&
                   transfer.mode == current->mode && transfer.value == current->expected_value &&
                   transfer.width == current->width && transfer.cycle == current->cycles,
               "master write matrix captures width mode mux and timing");
    }
}

static void wait_state_matrix_cases(TestState* state, Dspic33* cpu) {
    static const PmpMasterWriteCase cases[] = {
        {0u, 0x00c3u | PMP_MASTER_MODE_2, 0x005cu, 1u, 1u},
        {0u, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 9u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 13u},
        {PMP_FULL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 17u},
        {0u, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 18u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 22u},
        {PMP_FULL_MUX, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 26u},
    };
    Dspic33PmpTransfer transfer;
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        const PmpMasterWriteCase* current = &cases[index];
        dspic33_reset(cpu, 0u);
        configure_pmp_control(cpu, current->control, current->mode, 0x1357u);
        dspic33_write_word(cpu, PMP_DATA, 0xab5cu);
        expect(state, dspic33_device_advance(cpu, current->cycles - 1u),
               "advance wait-state matrix before completion");
        expect(state,
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
                   !dspic33_pmp_transmit(cpu, &transfer),
               "wait-state matrix clears BUSY before completion");
        expect(state, dspic33_device_advance(cpu, 1u), "advance wait-state matrix completion");
        expect(state,
               dspic33_pmp_transmit(cpu, &transfer) && transfer.value == current->expected_value &&
                   transfer.width == current->width && transfer.cycle == current->cycles,
               "wait-state matrix completes on the documented phase count");
    }
}

static void address_update_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t control;
        uint16_t mode;
        uint16_t initial;
        uint16_t expected;
    } cases[] = {
        {0u, PMP_INCREMENT, 0xffffu, 0x0000u},
        {0u, PMP_DECREMENT, 0x0000u, 0xffffu},
        {PMP_ONE_CHIP_SELECT, PMP_INCREMENT, 0xffffu, 0x8000u},
        {PMP_ONE_CHIP_SELECT, PMP_DECREMENT, 0x8000u, 0xffffu},
        {PMP_TWO_CHIP_SELECTS, PMP_INCREMENT, 0xffffu, 0xc000u},
        {PMP_TWO_CHIP_SELECTS, PMP_DECREMENT, 0xc000u, 0xffffu},
        {0u, 0x1800u, 0x4567u, 0x4567u},
    };
    Dspic33PmpTransfer transfer;
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        dspic33_reset(cpu, 0u);
        configure_pmp_control(cpu, cases[index].control,
                              (uint16_t)(PMP_MASTER_MODE_2 | cases[index].mode | 0x0004u),
                              cases[index].initial);
        dspic33_write_byte(cpu, PMP_DATA, 0x6au);
        expect(state,
               cpu->io.pmp.active && dspic33_read_word(cpu, PMP_ADDRESS) == cases[index].initial,
               "master address remains stable while BUSY");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance master address to BUSY-clear boundary");
        expect(state,
               !cpu->io.pmp.active &&
                   dspic33_read_word(cpu, PMP_ADDRESS) == cases[index].expected &&
                   !dspic33_pmp_transmit(cpu, &transfer),
               "master address updates when BUSY clears");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance master address operation completion");
        expect(state,
               dspic33_pmp_transmit(cpu, &transfer) && transfer.address == cases[index].initial &&
                   transfer.value == 0x6au,
               "master transfer retains pre-update address");
    }

    dspic33_reset(cpu, 0u);
    configure_pmp_control(cpu, 0x00c0u, PMP_MASTER_MODE_2, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x71u);
    expect(state, !cpu->io.pmp.active && cpu->events.count == 0u,
           "reserved chip-select function rejects master transfer");
    dspic33_reset(cpu, 0u);
    configure_pmp_control(cpu, 0x1800u, PMP_MASTER_MODE_2, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x72u);
    expect(state, !cpu->io.pmp.active && cpu->events.count == 0u,
           "reserved address multiplexing rejects master transfer");
}

static void master_read_pipeline_cases(TestState* state, Dspic33* cpu) {
    uint16_t generation;
    size_t events;
    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, PMP_MASTER_MODE_2, 0x1234u, 0x5aa5u);
    expect(state, dspic33_pmp_respond(cpu, 0x1234u, 0u), "queue 8-bit master response");
    expect(state, dspic33_read_byte(cpu, (uint16_t)(PMP_DATA + 1u)) == 0x5au && !cpu->io.pmp.active,
           "high-byte read does not initiate master access");
    expect(state,
           dspic33_read_byte(cpu, PMP_DATA) == 0xa5u && cpu->io.pmp.active && cpu->io.pmp.reading &&
               cpu->io.pmp.width == 1u && !cpu->io.pmp.last_read_valid,
           "first low-byte read returns dummy and starts access");
    generation = cpu->io.pmp.generation;
    events = cpu->events.count;
    expect(state,
           dspic33_read_byte(cpu, PMP_DATA) == 0xa5u && cpu->io.pmp.generation == generation &&
               cpu->events.count == events,
           "read while active returns same dummy without new access");
    dspic33_write_word(cpu, PMP_DATA, 0x7788u);
    expect(state, pmp_data_latch(cpu) == 0x5aa5u,
           "write while master read is active leaves data latch unchanged");
    expect(state, dspic33_device_advance(cpu, 1u), "complete first 8-bit master read");
    expect(state,
           cpu->io.pmp.last_read_valid && cpu->io.pmp.last_read.value == 0x0034u &&
               cpu->io.pmp.last_read.width == 1u && pmp_data_latch(cpu) == 0x5a34u &&
               cpu->io.pmp.input.count == 0u,
           "8-bit read completion replaces only the low byte");
    expect(state, dspic33_pmp_respond(cpu, 0x5678u, 0u), "queue sequential 8-bit response");
    expect(state, dspic33_read_byte(cpu, PMP_DATA) == 0x34u && cpu->io.pmp.active,
           "sequential read returns previous result and starts next access");
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.last_read.value == 0x0078u &&
               pmp_data_latch(cpu) == 0x5a78u,
           "sequential 8-bit response replaces the prior result");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | PMP_INCREMENT | 0x0004u), 0x4000u,
                       0x1357u);
    expect(state, dspic33_pmp_respond(cpu, 0x1111u, 0u) && dspic33_pmp_respond(cpu, 0x2222u, 0u),
           "queue final-cycle back-to-back responses");
    expect(state, dspic33_read_word(cpu, PMP_DATA) == 0x1357u && dspic33_device_advance(cpu, 2u),
           "advance first read through final BUSY cycle");
    expect(state,
           (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u && cpu->io.pmp.completing_active &&
               !cpu->io.pmp.last_read_valid && dspic33_read_word(cpu, PMP_ADDRESS) == 0x4001u,
           "final cycle retains first read before completion");
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x1357u && cpu->io.pmp.active &&
               cpu->io.pmp.completing_active && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u,
           "final-cycle read returns dummy and starts next access");
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.last_read.value == 0x0011u &&
               cpu->io.pmp.active && dspic33_read_word(cpu, PMP_ADDRESS) == 0x4001u,
           "first read completes while next access remains active");
    expect(state,
           dspic33_device_advance(cpu, 2u) && cpu->io.pmp.last_read.value == 0x0022u &&
               dspic33_read_word(cpu, PMP_ADDRESS) == 0x4002u,
           "second final-cycle read completes without losing state");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, PMP_MASTER_MODE_2, 0u, 0xa5a5u);
    expect(state, dspic33_pmp_respond(cpu, 0x3333u, 5u) && dspic33_pmp_respond(cpu, 0x1111u, 0u),
           "queue out-of-order timed PMP responses");
    dspic33_read_word(cpu, PMP_DATA);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.last_read.value == 0x0011u &&
               cpu->io.pmp.input.count == 1u,
           "master read consumes earliest eligible response");
    dspic33_read_word(cpu, PMP_DATA);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.last_read.value == 0u &&
               cpu->io.pmp.input.count == 1u,
           "master read without ready response returns deterministic zero");
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_read_byte(cpu, PMP_DATA) == 0u &&
               dspic33_device_advance(cpu, 1u) && cpu->io.pmp.last_read.value == 0x0033u &&
               cpu->io.pmp.input.count == 0u,
           "later master read consumes delayed response at its deadline");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0x2345u, 0xbeefu);
    expect(state, dspic33_pmp_respond(cpu, 0x1234u, 0u), "queue 16-bit master response");
    expect(state, dspic33_read_byte(cpu, (uint16_t)(PMP_DATA + 1u)) == 0xbeu && !cpu->io.pmp.active,
           "16-bit high-byte read does not initiate master access");
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0xbeefu && cpu->io.pmp.active &&
               cpu->io.pmp.reading && cpu->io.pmp.width == 2u,
           "16-bit word read returns previous result and starts two phases");
    expect(state,
           dspic33_device_advance(cpu, 2u) && cpu->io.pmp.last_read.value == 0x1234u &&
               cpu->io.pmp.last_read.width == 2u && pmp_data_latch(cpu) == 0x1234u,
           "16-bit master read captures both response bytes");
}

typedef struct {
    uint16_t control;
    uint16_t mode;
    uint16_t expected_value;
    uint8_t width;
    uint8_t cycles;
} PmpMasterReadCase;

static void master_read_matrix_cases(TestState* state, Dspic33* cpu) {
    static const PmpMasterReadCase cases[] = {
        {0u, PMP_MASTER_MODE_2, 0x005cu, 1u, 1u},
        {PMP_PARTIAL_MUX, PMP_MASTER_MODE_2, 0x005cu, 1u, 2u},
        {PMP_FULL_MUX, PMP_MASTER_MODE_2, 0x005cu, 1u, 3u},
        {0u, PMP_MASTER_MODE_3, 0x005cu, 1u, 1u},
        {PMP_PARTIAL_MUX, PMP_MASTER_MODE_3, 0x005cu, 1u, 2u},
        {PMP_FULL_MUX, PMP_MASTER_MODE_3, 0x005cu, 1u, 3u},
        {0u, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0xab5cu, 2u, 2u},
        {PMP_PARTIAL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0xab5cu, 2u, 3u},
        {PMP_FULL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0xab5cu, 2u, 4u},
        {0u, PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 2u},
        {PMP_PARTIAL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 3u},
        {PMP_FULL_MUX, PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 4u},
    };
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        const PmpMasterReadCase* current = &cases[index];
        uint16_t previous;
        dspic33_reset(cpu, 0u);
        configure_pmp_read(cpu, current->control, current->mode, 0x2468u, 0x5aa5u);
        expect(state, dspic33_pmp_respond(cpu, 0xab5cu, 0u), "queue master read matrix response");
        previous = dspic33_read_word(cpu, PMP_DATA);
        expect(state,
               previous == 0x5aa5u && cpu->io.pmp.active && cpu->io.pmp.reading &&
                   ((dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u) == (current->cycles > 1u),
               "master read matrix returns prior value and starts with documented BUSY");
        expect(state, dspic33_device_advance(cpu, current->cycles - 1u),
               "advance master read matrix before completion");
        expect(state,
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u && !cpu->io.pmp.last_read_valid,
               "master read matrix clears BUSY on its final cycle");
        expect(state, dspic33_device_advance(cpu, 1u), "advance master read matrix completion");
        expect(state,
               cpu->io.pmp.last_read_valid && cpu->io.pmp.last_read.address == 0x2468u &&
                   cpu->io.pmp.last_read.control == (uint16_t)(current->control | PMP_ENABLE) &&
                   cpu->io.pmp.last_read.mode == current->mode &&
                   cpu->io.pmp.last_read.value == current->expected_value &&
                   cpu->io.pmp.last_read.width == current->width &&
                   cpu->io.pmp.last_read.cycle == current->cycles &&
                   pmp_data_latch(cpu) == (current->width == 2u ? 0xab5cu : 0x5a5cu),
               "master read matrix captures width mode mux value and timing");
    }
}

static void read_wait_state_matrix_cases(TestState* state, Dspic33* cpu) {
    static const PmpMasterReadCase cases[] = {
        {0u, 0x00c3u | PMP_MASTER_MODE_2, 0x005cu, 1u, 1u},
        {0u, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 9u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 13u},
        {PMP_FULL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 17u},
        {0u, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 18u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 22u},
        {PMP_FULL_MUX, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 26u},
    };
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        const PmpMasterReadCase* current = &cases[index];
        dspic33_reset(cpu, 0u);
        configure_pmp_read(cpu, current->control, current->mode, 0x1357u, 0u);
        expect(state, dspic33_pmp_respond(cpu, 0xab5cu, 0u), "queue wait-state read response");
        dspic33_read_word(cpu, PMP_DATA);
        expect(state, dspic33_device_advance(cpu, current->cycles - 1u),
               "advance read wait-state matrix before completion");
        expect(state,
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u && !cpu->io.pmp.last_read_valid,
               "read wait-state matrix clears BUSY before completion");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   cpu->io.pmp.last_read.value == current->expected_value &&
                   cpu->io.pmp.last_read.width == current->width &&
                   cpu->io.pmp.last_read.cycle == current->cycles,
               "read wait-state matrix completes on documented phase count");
    }
}

static void read_address_update_cases(TestState* state, Dspic33* cpu) {
    static const struct {
        uint16_t control;
        uint16_t mode;
        uint16_t initial;
        uint16_t expected;
    } cases[] = {
        {0u, PMP_INCREMENT, 0xffffu, 0x0000u},
        {0u, PMP_DECREMENT, 0x0000u, 0xffffu},
        {PMP_ONE_CHIP_SELECT, PMP_INCREMENT, 0xffffu, 0x8000u},
        {PMP_ONE_CHIP_SELECT, PMP_DECREMENT, 0x8000u, 0xffffu},
        {PMP_TWO_CHIP_SELECTS, PMP_INCREMENT, 0xffffu, 0xc000u},
        {PMP_TWO_CHIP_SELECTS, PMP_DECREMENT, 0xc000u, 0xffffu},
        {0u, 0x1800u, 0x4567u, 0x4567u},
    };
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        dspic33_reset(cpu, 0u);
        configure_pmp_read(cpu, cases[index].control,
                           (uint16_t)(PMP_MASTER_MODE_2 | cases[index].mode | 0x0004u),
                           cases[index].initial, 0u);
        expect(state, dspic33_pmp_respond(cpu, 0x006au, 0u), "queue read address response");
        dspic33_read_byte(cpu, PMP_DATA);
        expect(state,
               cpu->io.pmp.active && dspic33_read_word(cpu, PMP_ADDRESS) == cases[index].initial,
               "read address remains stable while BUSY");
        expect(state, dspic33_device_advance(cpu, 2u),
               "advance read address to BUSY-clear boundary");
        expect(state,
               !cpu->io.pmp.active &&
                   dspic33_read_word(cpu, PMP_ADDRESS) == cases[index].expected &&
                   !cpu->io.pmp.last_read_valid,
               "read address updates when BUSY clears");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   cpu->io.pmp.last_read.address == cases[index].initial &&
                   cpu->io.pmp.last_read.value == 0x006au,
               "read transaction retains pre-update address");
    }

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0x00c0u, PMP_MASTER_MODE_2, 0u, 0x1111u);
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x1111u && !cpu->io.pmp.active &&
               cpu->events.count == 0u,
           "reserved chip-select function rejects master read");
    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0x1800u, PMP_MASTER_MODE_2, 0u, 0x2222u);
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x2222u && !cpu->io.pmp.active &&
               cpu->events.count == 0u,
           "reserved address multiplexing rejects master read");
}

static void read_interrupt_dma_cases(TestState* state, Dspic33* cpu) {
    uint16_t index;
    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_INTERRUPT_EACH | PMP_MASTER_MODE_2), 0u, 0u);
    expect(state, dspic33_pmp_respond(cpu, 0x0044u, 0u), "queue interrupting read response");
    dspic33_read_byte(cpu, PMP_DATA);
    expect(state, dspic33_device_advance(cpu, 1u), "complete interrupting PMP read");
    expect(state,
           (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u &&
               cpu->io.pmp.last_read.value == 0x0044u && cpu->io.pmp.output.count == 0u,
           "master read completion raises PMPIF without transmit output");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(
        cpu, 0u,
        (uint16_t)(PMP_INTERRUPT_EACH | PMP_DATA_16_BIT | PMP_MASTER_MODE_2 | PMP_INCREMENT), 0u,
        0u);
    for (index = 0u; index < 4u; index++) {
        expect(state, dspic33_pmp_respond(cpu, (uint16_t)(0x1100u + index), 0u),
               "queue DMA read response");
    }
    configure_dma_read(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, PMP_DATA, 2u);
    expect(state, dspic33_read_word(cpu, PMP_DATA) == 0u, "dummy read starts DMA response chain");
    expect(state, dspic33_device_advance(cpu, 12u), "advance complete PMP read DMA chain");
    expect(state,
           dspic33_read_word(cpu, 0x4000u) == 0x1100u &&
               dspic33_read_word(cpu, 0x4002u) == 0x1101u &&
               dspic33_read_word(cpu, 0x4004u) == 0x1102u,
           "PMP interrupt DMA stores sequential completed reads");
    expect(state,
           (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u &&
               (dspic33_read_word(cpu, 0x0800u) & 0x0010u) != 0u &&
               dspic33_read_word(cpu, PMP_ADDRESS) == 4u && cpu->io.pmp.input.count == 0u,
           "DMA completion leaves one documented pipelined PMP read");
}

static void read_lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize copied PMP read processor");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0x3456u, 0u);
    expect(state,
           dspic33_pmp_respond(cpu, 0xa55au, 0u) && dspic33_read_word(cpu, PMP_DATA) == 0u &&
               dspic33_copy(&copy, cpu),
           "copy active PMP read and response");
    expect(state,
           copy.io.pmp.active && copy.io.pmp.reading && copy.io.pmp.input.count == 1u &&
               copy.events.count == cpu->events.count,
           "copy retains active PMP read state");
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_device_advance(&copy, 3u) &&
               cpu->io.pmp.last_read.value == 0x005au && copy.io.pmp.last_read.value == 0x005au,
           "copied PMP reads complete independently");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0u, 0u);
    expect(state, dspic33_pmp_respond(cpu, 0x7788u, 0u), "queue disabled PMP read response");
    dspic33_read_word(cpu, PMP_DATA);
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    expect(state,
           !cpu->io.pmp.active && !cpu->io.pmp.last_read_valid &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP disable aborts active read");
    expect(state, dspic33_device_advance(cpu, 3u) && !cpu->io.pmp.last_read_valid,
           "stale PMP read event produces no result");

    expect(state, dspic33_pmp_respond(cpu, 0x1122u, 5u), "queue response before processor reset");
    dspic33_reset(cpu, 0u);
    expect(state, cpu->io.pmp.input.count == 0u, "processor reset clears queued PMP responses");

    cpu->io.pmp.input.count = DSPIC33_PMP_QUEUE_SIZE;
    expect(state, !dspic33_pmp_respond(cpu, 0x3344u, 0u), "full PMP response queue rejects input");
    cpu->io.pmp.input.count = 0u;
    cpu->device_cycles = UINT64_MAX;
    expect(state, !dspic33_pmp_respond(cpu, 0x5566u, 1u),
           "PMP response cycle overflow is rejected");

    configure_pmp_read(cpu, 0u, PMP_MASTER_MODE_2, 0u, 0x99aau);
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x99aau && !cpu->io.pmp.active &&
               cpu->events.count == 0u && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP read scheduling overflow rolls back transfer");
    dspic33_release(&copy);
}

static void interrupt_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    uint16_t priority_address = (uint16_t)(0x0840u + (PMP_IRQ / 4u) * 2u);
    uint16_t priority_shift = (uint16_t)((PMP_IRQ % 4u) * 4u);
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_MASTER_MODE_2, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x33u);
    expect(state, dspic33_device_advance(cpu, 1u), "noninterrupting PMP completes");
    expect(state, dspic33_pmp_transmit(cpu, &transfer), "noninterrupting output captured");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
           "IRQM zero does not raise PMPIF");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, (uint16_t)(PMP_INTERRUPT_EACH | PMP_MASTER_MODE_2), 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x44u);
    expect(state, dspic33_device_advance(cpu, 1u), "interrupting PMP completes");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "IRQM each raises PMPIF");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, (uint16_t)(PMP_INTERRUPT_EACH | PMP_MASTER_MODE_2), 0u);
    dspic33_write_word(cpu, 0x0824u, PMP_INTERRUPT_ENABLE);
    dspic33_write_word(
        cpu, priority_address,
        (uint16_t)((dspic33_read_word(cpu, priority_address) & ~(7u << priority_shift)) |
                   (PMP_PRIORITY << priority_shift)));
    cpu->program[(0x0014u + PMP_IRQ * 2u) / 2u] = PMP_VECTOR;
    cpu->w[15] = 0x1800u;
    dspic33_write_byte(cpu, PMP_DATA, 0x45u);
    expect(state, dspic33_device_advance(cpu, 1u), "vectored PMP completes");
    expect(state, dspic33_device_interrupt_pending(cpu), "PMP interrupt pending");
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == PMP_IRQ &&
               cpu->pc == PMP_VECTOR,
           "PMP interrupt 45 vectors");
}

static void dma_chain_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    uint16_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < PMP_TRANSFER_COUNT; index++) {
        dspic33_write_byte(cpu, (uint16_t)(PMP_DMA_SOURCE + index), (uint8_t)index);
    }
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    configure_dma(cpu, PMP_DMA_CHANNEL, PMP_DMA_REQUEST, PMP_DMA_SOURCE, PMP_DATA,
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

static void dma_negative_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    uint8_t value;
    dspic33_reset(cpu, 0u);
    for (value = 0u; value < 4u; value++) {
        dspic33_write_byte(cpu, (uint16_t)(0x4000u + value), (uint8_t)(0xa0u + value));
    }
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    configure_dma(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, PMP_DATA, 3u);
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
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    configure_dma(cpu, 0u, 0x2cu, 0x4000u, PMP_DATA, 1u);
    dspic33_write_word(cpu, 0x0b02u, 0x802cu);
    expect(state, dspic33_device_advance(cpu, 100u), "advance wrong request chain");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0x51u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "wrong IRQSEL stops after forced byte");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, 0x4000u, 0x61u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    configure_dma(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, (uint16_t)(PMP_DATA + 2u), 0u);
    dspic33_write_word(cpu, 0x0b02u, (uint16_t)(0x8000u | PMP_DMA_REQUEST));
    expect(state, dspic33_device_advance(cpu, 30u), "advance wrong PAD transfer");
    expect(state, !dspic33_pmp_transmit(cpu, &transfer), "wrong PAD does not start PMP");

    dspic33_reset(cpu, 0u);
    dspic33_write_byte(cpu, 0x4000u, 0x71u);
    dspic33_write_byte(cpu, 0x4001u, 0x72u);
    configure_pmp(cpu, PMP_MASTER_MODE_2, 0u);
    configure_dma(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, PMP_DATA, 1u);
    dspic33_write_word(cpu, 0x0b02u, (uint16_t)(0x8000u | PMP_DMA_REQUEST));
    expect(state, dspic33_device_advance(cpu, 30u), "advance IRQM zero chain");
    expect(state,
           dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0x71u &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "IRQM zero stops after forced byte");
}

static void lifecycle_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33PmpTransfer original_transfer;
    Dspic33PmpTransfer copy_transfer;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize PMP copy");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x1111u);
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
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x91u);
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    expect(state, !cpu->io.pmp.active && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP disable aborts active transfer");
    expect(state, dspic33_device_advance(cpu, 21u), "advance stale PMP event");
    expect(state, !dspic33_pmp_transmit(cpu, &original_transfer),
           "stale PMP event produces no output");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    cpu->io.pmp.output.count = DSPIC33_PMP_QUEUE_SIZE;
    dspic33_write_byte(cpu, PMP_DATA, 0xa1u);
    expect(state, !dspic33_device_advance(cpu, 21u), "full output queue stops advance");
    expect(state, cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "full PMP output queue reports error");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    cpu->device_cycles = UINT64_MAX;
    dspic33_write_byte(cpu, PMP_DATA, 0xb1u);
    expect(state,
           !cpu->io.pmp.active && cpu->events.count == 0u &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP scheduling overflow rolls back transfer");
    dspic33_release(&copy);
}

static void legacy_slave_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, 0u);
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
    configure_pmp_slave(cpu, 0u, 0u);
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

static void buffered_slave_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t values[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    Dspic33PmpTransfer transfer;
    uint8_t index;
    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, (uint16_t)(PMP_BUFFERED_SLAVE | PMP_INTERRUPT_LAST));
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

static void addressable_slave_cases(TestState* state, Dspic33* cpu) {
    static const uint8_t order[4] = {3u, 1u, 0u, 2u};
    static const uint8_t outputs[4] = {0x10u, 0x21u, 0x32u, 0x43u};
    Dspic33PmpTransfer transfer;
    uint8_t index;
    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, (uint16_t)(PMP_SLAVE_ADDRESSABLE | PMP_INTERRUPT_EACH));
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
    configure_pmp_slave(cpu, 0u, PMP_SLAVE_ADDRESSABLE);
    dspic33_write_word(cpu, PMP_ADDRESS_ENABLE, PMP_CHIP_SELECT_ENABLE);
    expect(state,
           dspic33_pmp_slave_write(cpu, 3u, 0x77u, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA + 3u] == 0u,
           "addressable slave requires both address input enables");
}

static void power_management_cases(TestState* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    uint16_t address;
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x1234u);
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
    configure_pmp_control(cpu, PMP_STOP_IDLE, (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0u);
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
    configure_pmp_control(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x63u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x63u,
           "PSIDL clear continues master in Idle");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x3456u);
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

static void slave_power_lifecycle_cases(TestState* state, Dspic33* cpu) {
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
    configure_pmp_slave(cpu, PMP_STOP_IDLE, 0u);
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
    configure_pmp_slave(cpu, PMP_STOP_IDLE, 0u);
    dspic33_write_byte(cpu, PMP_ADDRESS, 0x72u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 1u) && dspic33_device_advance(cpu, 1u) &&
               dspic33_pmp_transmit(cpu, &original) && original.value == 0x72u,
           "PSIDL stopped Idle retains asynchronous slave reads");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_SLAVE_ADDRESSABLE);
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
    configure_pmp_slave(cpu, 0u, 0u);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0x82u, 5u), "schedule slave write before reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           dspic33_device_advance(cpu, 5u) && cpu->data[PMP_DATA] == 0u && cpu->events.count == 0u,
           "reset cancels slave transaction and restores state");

    configure_pmp_slave(cpu, 0u, 0u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 1u), "disable PMP before slave event");
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x83u, 1u) && dspic33_device_advance(cpu, 1u) &&
               cpu->data[PMP_DATA] == 0u,
           "PMPMD drops external slave event while disabled");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, 0u);
    cpu->io.pmp.output.count = DSPIC33_PMP_QUEUE_SIZE;
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 0u) && !dspic33_device_advance(cpu, 0u) &&
               cpu->stop_reason == DSPIC33_EVENT_QUEUE_ERROR,
           "full PMP output queue reports slave read failure");
    dspic33_release(&copy);
}

static void slave_mode_matrix_cases(TestState* state, Dspic33* cpu) {
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
        configure_pmp_slave(cpu, 0u, requests[index]);
        expect(state,
               dspic33_pmp_slave_write(cpu, 0u, (uint8_t)(0x20u + index), 0u) &&
                   dspic33_device_advance(cpu, 0u) &&
                   cpu->data[PMP_DATA] == (uint8_t)(0x20u + index) &&
                   (dspic33_read_word(cpu, PMP_STATUS) & PMP_INPUT_FULL) != 0u,
               "legacy slave admits every IRQM encoding");
        expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
               "legacy slave interrupts independently of IRQM");

        dspic33_reset(cpu, 0u);
        configure_pmp_slave(cpu, 0u, (uint16_t)(PMP_BUFFERED_SLAVE | requests[index]));
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
        configure_pmp_slave(cpu, 0u, (uint16_t)(PMP_SLAVE_ADDRESSABLE | requests[index]));
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
    configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_RESERVED);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x2fu, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA] == 0x2fu,
           "legacy slave applies deterministic reserved IRQM policy");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "legacy reserved IRQM policy retains unconditional interrupt");
    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, (uint16_t)(PMP_BUFFERED_SLAVE | PMP_INTERRUPT_RESERVED));
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x3fu, 0u) && dspic33_device_advance(cpu, 0u) &&
               cpu->data[PMP_DATA] == 0x3fu,
           "buffered slave applies deterministic reserved IRQM policy");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
           "buffered reserved IRQM policy raises no interrupt");
    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, (uint16_t)(PMP_SLAVE_ADDRESSABLE | PMP_INTERRUPT_RESERVED));
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
        configure_pmp_slave(cpu, 0u, invalid_modes[index]);
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

static void slave_dma_isolation_cases(TestState* state, Dspic33* cpu) {
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
        configure_pmp_slave(cpu, 0u, (uint16_t)(modes[index] | PMP_INTERRUPT_EACH));
        configure_dma(cpu, 0u, PMP_DMA_REQUEST, PMP_DMA_SOURCE, PMP_DATA, 1u);
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

static void pmp_extended_lifecycle_cases(TestState* state, Dspic33* cpu) {
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
    configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
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
    configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0x61u, 0u) && dspic33_device_advance(cpu, 0u),
           "seed slave state before PMPEN disable");
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    expect(state,
           cpu->data[PMP_DATA] == 0x61u && (raw_data_word(cpu, PMP_STATUS) & 0x0100u) != 0u &&
               cpu->io.pmp.slave_write_index == 1u,
           "PMPEN disable retains slave data status and pointer");
    configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    expect(state,
           raw_data_word(cpu, PMP_STATUS) == 0x008fu && cpu->io.pmp.slave_read_index == 0u &&
               cpu->io.pmp.slave_write_index == 0u,
           "PMPEN re-enable resets slave status and pointers");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0x71u, 0u) && dspic33_pmp_slave_read(cpu, 0u, 0u) &&
               dspic33_device_advance(cpu, 0u),
           "seed slave state before PMD disable");
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled &&
               cpu->io.pmp.slave_read_index == 1u && cpu->io.pmp.slave_write_index == 1u &&
               (raw_data_word(cpu, PMP_STATUS) & 0x0101u) == 0x0101u,
           "PMPMD retains slave status and pointers while disabled");
    for (index = 0u; index < 8u; index++) {
        uint16_t before = raw_data_word(cpu, registers[index]);
        expect(state, dspic33_read_word(cpu, registers[index]) == 0u,
               "disabled PMP read follows deterministic zero policy");
        dspic33_write_word(cpu, registers[index], (uint16_t)(0xa500u + index));
        expect(state, raw_data_word(cpu, registers[index]) == before,
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
    configure_pmp_slave(cpu, 0u, PMP_BUFFERED_SLAVE);
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
    configure_pmp_slave(cpu, 0u, 0u);
    cpu->device_cycles = UINT64_MAX;
    expect(state,
           !dspic33_pmp_slave_write(cpu, 0u, 0x81u, 1u) && !dspic33_pmp_slave_read(cpu, 0u, 1u) &&
               cpu->events.count == 0u,
           "slave scheduling overflow rolls back without state");
    dspic33_release(&copy);
}

static void pmp_power_wake_matrix_cases(TestState* state, Dspic33* cpu) {
    Dspic33 copy;
    Dspic33PmpTransfer transfer;
    uint16_t priority_address = (uint16_t)(0x0840u + (PMP_IRQ / 4u) * 2u);
    uint16_t priority_shift = (uint16_t)((PMP_IRQ % 4u) * 4u);
    bool initialized;

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x91u);
    expect(state,
           dspic33_device_advance(cpu, 20u) && !cpu->io.pmp.active &&
               cpu->io.pmp.completing_active && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "master reaches final completion phase before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->events.count == 1u && cpu->events.items[0].paused,
           "Sleep pauses final PMP completion phase");
    expect(state,
           dspic33_device_advance(cpu, 100u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) == 0u,
           "Sleep suppresses final PMP output and interrupt");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state, !cpu->events.items[0].paused, "Sleep exit resumes final PMP completion phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x91u,
           "resumed final PMP phase emits retained transfer");
    expect(state,
           !cpu->io.pmp.completing_active &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "resumed final PMP phase raises completion interrupt");

    dspic33_reset(cpu, 0u);
    expect(state, dspic33_load_program_word(cpu, 0u, OPCODE_POWER_SAVE_SLEEP),
           "load stepped PWRSAV instruction");
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x92u);
    expect(state, dspic33_step(cpu) == DSPIC33_SLEEPING && cpu->power_state == DSPIC33_POWER_SLEEP,
           "PWRSAV enters Sleep with active PMP transfer");
    expect(state,
           cpu->io.pmp.active && cpu->events.count == 2u && cpu->events.items[0].paused &&
               cpu->events.items[1].paused,
           "PWRSAV instruction pauses both PMP master phases");
    expect(state,
           dspic33_device_advance(cpu, 50u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               cpu->io.pmp.active,
           "stepped PWRSAV keeps master transfer suspended");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_device_advance(cpu, 21u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x92u,
           "stepped PWRSAV transfer resumes after wake");

    dspic33_reset(cpu, 0u);
    cpu->configuration[10u] = 0x80u;
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x93u);
    expect(state, dspic33_device_advance(cpu, 5u), "advance PMP before WDT Sleep wake");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->io.pmp.active && cpu->events.items[0].paused,
           "Sleep pauses PMP before WDT wake");
    dspic33_watchdog_advance_lprc(cpu, 32u);
    expect(state,
           cpu->power_state == DSPIC33_POWER_ACTIVE &&
               (dspic33_read_word(cpu, 0x0740u) & 0x0010u) != 0u,
           "WDT timeout wakes sleeping processor");
    expect(state, !cpu->events.items[0].paused && !cpu->events.items[1].paused,
           "WDT wake resumes PMP master events");
    expect(state,
           dspic33_device_advance(cpu, 16u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x93u,
           "WDT wake completes retained PMP transfer");

    dspic33_reset(cpu, 0u);
    configure_pmp_control(cpu, PMP_STOP_IDLE, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x94u);
    expect(state, dspic33_device_advance(cpu, 20u) && cpu->io.pmp.completing_active,
           "master reaches final completion phase before Idle");
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->events.count == 1u && cpu->events.items[0].paused,
           "PSIDL pauses final PMP completion phase");
    expect(state, dspic33_device_advance(cpu, 40u) && !dspic33_pmp_transmit(cpu, &transfer),
           "PSIDL suppresses final PMP completion while Idle");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state, !cpu->events.items[0].paused, "Idle exit resumes final PMP completion phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0x94u,
           "Idle exit completes retained final PMP phase");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0xa1u, 1u) && dspic33_device_advance(cpu, 1u) &&
               cpu->data[PMP_DATA] == 0xa1u,
           "slave write continues in Idle with PSIDL clear");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "Idle slave write raises PMP interrupt flag");
    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, PMP_STOP_IDLE, PMP_INTERRUPT_EACH);
    dspic33_write_byte(cpu, PMP_ADDRESS, 0xa2u);
    cpu->power_state = DSPIC33_POWER_IDLE;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_read(cpu, 0u, 1u) && dspic33_device_advance(cpu, 1u) &&
               dspic33_pmp_transmit(cpu, &transfer) && transfer.value == 0xa2u,
           "asynchronous slave read continues in Idle with PSIDL set");
    expect(state, (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "PSIDL slave read raises PMP interrupt flag");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0xb1u, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "sleeping slave latches interrupt with IEC disabled");
    expect(state, !dspic33_device_wake(cpu), "disabled PMP interrupt cannot wake processor");
    expect(state, cpu->power_state == DSPIC33_POWER_SLEEP && cpu->last_interrupt != PMP_IRQ,
           "IEC-disabled PMP event retains sleeping state");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    dspic33_write_word(cpu, 0x0824u, PMP_INTERRUPT_ENABLE);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state,
           dspic33_pmp_slave_write(cpu, 0u, 0xb2u, 0u) && dspic33_device_advance(cpu, 0u) &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "sleeping slave latches interrupt at priority zero");
    expect(state, !dspic33_device_wake(cpu), "priority-zero PMP interrupt cannot wake processor");
    expect(state, cpu->power_state == DSPIC33_POWER_SLEEP && cpu->last_interrupt != PMP_IRQ,
           "priority-zero PMP event retains sleeping state");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    dspic33_write_word(cpu, 0x0824u, PMP_INTERRUPT_ENABLE);
    dspic33_write_word(cpu, priority_address, (uint16_t)(PMP_PRIORITY << priority_shift));
    cpu->program[(0x0014u + PMP_IRQ * 2u) / 2u] = PMP_VECTOR;
    cpu->w[15] = 0x1800u;
    cpu->sr = (uint16_t)(PMP_PRIORITY << 5u);
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0xb3u, 0u) && dspic33_device_advance(cpu, 0u),
           "sleeping slave raises equal-priority interrupt");
    expect(state, dspic33_device_wake(cpu), "equal-priority PMP interrupt wakes without vectoring");
    expect(state,
           cpu->pc == 0u && cpu->last_interrupt != PMP_IRQ &&
               (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u,
           "equal-priority wake retains pending PMP interrupt");
    cpu->sr = 0u;
    expect(state, dspic33_device_interrupt_pending(cpu),
           "lowered IPL exposes retained PMP interrupt");
    expect(state,
           dspic33_device_service_interrupt(cpu) && cpu->last_interrupt == PMP_IRQ &&
               cpu->pc == PMP_VECTOR,
           "lowered IPL vectors retained PMP interrupt");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0xc1u);
    expect(state, dspic33_device_advance(cpu, 19u) && cpu->io.pmp.active,
           "advance PMP to cycle before final BUSY phase");
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled && !cpu->io.pmp.active &&
               cpu->io.pmp.completing_active,
           "PMPMD transition pauses newly entered final phase");
    expect(state,
           dspic33_device_advance(cpu, 30u) && !dspic33_pmp_transmit(cpu, &transfer) &&
               cpu->io.pmp.completing_active,
           "PMPMD holds final completion phase indefinitely");
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled &&
               cpu->io.pmp.completing_active,
           "PMPMD clear resumes retained final completion phase");
    expect(state,
           dspic33_device_advance(cpu, 1u) && dspic33_pmp_transmit(cpu, &transfer) &&
               transfer.value == 0xc1u,
           "PMPMD resumed final phase emits transfer");
    expect(state, !cpu->io.pmp.completing_active,
           "PMPMD resumed final phase clears completion state");

    dspic33_reset(cpu, 0u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled,
           "establish PMP PMD disable before cold reset");
    dspic33_reset(cpu, 0u);
    expect(state,
           (dspic33_read_word(cpu, PMP_PMD) & PMP_MODULE_DISABLE) == 0u &&
               !cpu->io.pmp.pmd_disabled,
           "cold reset clears PMP PMD state");
    expect(state, dspic33_read_word(cpu, PMP_STATUS) == 0x008fu,
           "cold reset restores disabled PMP register reset state");

    initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize PMP PMD transition copy");
    if (initialized) {
        dspic33_reset(cpu, 0u);
        dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
        expect(state, dspic33_copy(&copy, cpu), "copy pending PMP PMD transition");
        expect(state, dspic33_device_advance(cpu, 1u) && dspic33_device_advance(&copy, 1u),
               "advance copied PMP PMD transitions");
        expect(state, cpu->io.pmp.pmd_disabled && copy.io.pmp.pmd_disabled,
               "copied PMP PMD transitions complete equally");
        dspic33_write_word(cpu, PMP_PMD, 0u);
        expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled,
               "source PMP PMD copy diverges after clear");
        expect(state,
               copy.io.pmp.pmd_disabled &&
                   (dspic33_read_word(&copy, PMP_PMD) & PMP_MODULE_DISABLE) != 0u,
               "copied PMP PMD state remains independent");
        dspic33_release(&copy);
    }

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    expect(state, dspic33_pmp_slave_write(cpu, 0u, 0xd1u, 5u),
           "schedule absolute slave event across PMP PMD window");
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 2u) && cpu->io.pmp.pmd_disabled,
           "disable PMP before absolute slave event deadline");
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state, dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled,
           "re-enable PMP before absolute slave event deadline");
    expect(state, dspic33_device_advance(cpu, 2u) && cpu->data[PMP_DATA] == 0xd1u,
           "absolute slave event retains deadline across PMP PMD window");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, PMP_FIRMWARE_MODE, 0u, 0x4455u);
    expect(state,
           dspic33_pmp_respond(cpu, 0x6677u, 0u) && dspic33_read_word(cpu, PMP_DATA) == 0x4455u &&
               dspic33_device_advance(cpu, 5u),
           "begin master read before Sleep");
    cpu->power_state = DSPIC33_POWER_SLEEP;
    dspic33_device_power_state_changed(cpu);
    expect(state, cpu->io.pmp.active && cpu->events.items[0].paused,
           "Sleep pauses active master read");
    expect(state,
           dspic33_device_advance(cpu, 40u) && cpu->io.pmp.active &&
               raw_data_word(cpu, PMP_DATA) == 0x4455u,
           "Sleep preserves previous read result while suspended");
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    dspic33_device_power_state_changed(cpu);
    expect(state, !cpu->events.items[0].paused && !cpu->events.items[1].paused,
           "wake resumes both master read phases");
    expect(state,
           dspic33_device_advance(cpu, 16u) && raw_data_word(cpu, PMP_DATA) == 0x4477u &&
               !cpu->io.pmp.active && !cpu->io.pmp.completing_active,
           "wake completes retained master read");

    dspic33_reset(cpu, 0u);
    configure_pmp_slave(cpu, 0u, PMP_INTERRUPT_EACH);
    dspic33_write_byte(cpu, PMP_ADDRESS, 0xe1u);
    dspic33_write_word(cpu, PMP_PMD, PMP_MODULE_DISABLE);
    expect(state, dspic33_device_advance(cpu, 1u) && cpu->io.pmp.pmd_disabled,
           "disable PMP before external slave read");
    expect(state, dspic33_pmp_slave_read(cpu, 0u, 1u) && dspic33_device_advance(cpu, 1u),
           "external slave read deadline occurs while PMP disabled");
    expect(state,
           !dspic33_pmp_transmit(cpu, &transfer) && raw_data_word(cpu, PMP_STATUS) == 0x000fu,
           "disabled PMP drops external slave read without status change");
    dspic33_write_word(cpu, PMP_PMD, 0u);
    expect(state,
           dspic33_device_advance(cpu, 1u) && !cpu->io.pmp.pmd_disabled &&
               !dspic33_pmp_transmit(cpu, &transfer),
           "re-enabled PMP does not replay missed slave read");
}

int main(void) {
    Dspic33 cpu;
    TestState state = {0u, 0u, 0u};
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "initialize PMP processor");
    if (initialized) {
        access_cases(&state, &cpu);
        timing_cases(&state, &cpu);
        access_lane_cases(&state, &cpu);
        sixteen_bit_lane_cases(&state, &cpu);
        master_write_matrix_cases(&state, &cpu);
        wait_state_matrix_cases(&state, &cpu);
        address_update_cases(&state, &cpu);
        master_read_pipeline_cases(&state, &cpu);
        master_read_matrix_cases(&state, &cpu);
        read_wait_state_matrix_cases(&state, &cpu);
        read_address_update_cases(&state, &cpu);
        read_interrupt_dma_cases(&state, &cpu);
        read_lifecycle_cases(&state, &cpu);
        interrupt_cases(&state, &cpu);
        dma_chain_cases(&state, &cpu);
        dma_negative_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        legacy_slave_cases(&state, &cpu);
        buffered_slave_cases(&state, &cpu);
        addressable_slave_cases(&state, &cpu);
        power_management_cases(&state, &cpu);
        slave_power_lifecycle_cases(&state, &cpu);
        slave_mode_matrix_cases(&state, &cpu);
        slave_dma_isolation_cases(&state, &cpu);
        pmp_extended_lifecycle_cases(&state, &cpu);
        pmp_power_wake_matrix_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
