#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "device.h"
#include "dspic33.h"

typedef struct {
    uint32_t cases;
    uint32_t passed;
    uint32_t failed;
} PmpConformance;

enum {
    PMP_CONTROL = 0x0600u,
    PMP_MODE = 0x0602u,
    PMP_ADDRESS = 0x0604u,
    PMP_DATA = 0x0608u,
    PMP_STATUS = 0x060eu,
    PMP_ENABLE = 0x8000u,
    PMP_BUSY = 0x8000u,
    PMP_INTERRUPT_EACH = 0x2000u,
    PMP_INCREMENT = 0x0800u,
    PMP_DECREMENT = 0x1000u,
    PMP_DATA_16_BIT = 0x0400u,
    PMP_MASTER_MODE_2 = 0x0200u,
    PMP_MASTER_MODE_3 = 0x0300u,
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
    PMP_TRANSFER_COUNT = 8192u
};

static void expect(PmpConformance* state, bool condition, const char* name) {
    state->cases++;
    if (condition) {
        state->passed++;
    } else {
        state->failed++;
        printf("[pmp-failed] %s\n", name);
    }
}

static void configure_pmp_control(Dspic33* cpu, uint16_t control, uint16_t mode,
                                  uint16_t address) {
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    dspic33_write_word(cpu, PMP_MODE, mode);
    dspic33_write_word(cpu, PMP_ADDRESS, address);
    dspic33_write_word(cpu, PMP_CONTROL, (uint16_t)(control | PMP_ENABLE));
}

static void configure_pmp(Dspic33* cpu, uint16_t mode, uint16_t address) {
    configure_pmp_control(cpu, 0u, mode, address);
}

static void configure_dma(Dspic33* cpu, uint8_t channel, uint8_t request,
                          uint32_t source, uint16_t pad, uint16_t count) {
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

static void configure_dma_read(Dspic33* cpu, uint8_t channel, uint8_t request,
                               uint32_t destination, uint16_t pad, uint16_t count) {
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

static void configure_pmp_read(Dspic33* cpu, uint16_t control, uint16_t mode,
                               uint16_t address, uint16_t previous) {
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    dspic33_write_word(cpu, PMP_DATA, previous);
    configure_pmp_control(cpu, control, mode, address);
}

static void access_cases(PmpConformance* state, Dspic33* cpu) {
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

static void timing_cases(PmpConformance* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, (uint16_t)(PMP_INTERRUPT_EACH | PMP_MASTER_MODE_2), 0x1234u);
    dspic33_write_byte(cpu, PMP_DATA, 0x5au);
    expect(state, cpu->io.pmp.active, "WAITM zero starts transfer");
    expect(state, (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "one-cycle transfer has no sampled BUSY interval");
    expect(state, !dspic33_pmp_transmit(cpu, &transfer),
           "transfer unavailable before completion");
    expect(state, dspic33_device_advance(cpu, 1u), "advance one-cycle transfer");
    expect(state, dspic33_pmp_transmit(cpu, &transfer), "one-cycle transfer completes");
    expect(state,
           transfer.address == 0x1234u && transfer.value == 0x5au &&
               transfer.cycle == 1u,
           "one-cycle transfer captures address data and cycle");

    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0x4567u);
    dspic33_write_byte(cpu, PMP_DATA, 0x11u);
    expect(state, (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u,
           "firmware timing asserts BUSY");
    dspic33_write_byte(cpu, PMP_DATA, 0x22u);
    dspic33_write_word(cpu, PMP_ADDRESS, 0x7654u);
    dspic33_write_word(cpu, PMP_MODE, PMP_MASTER_MODE_2);
    expect(state, dspic33_device_advance(cpu, 20u),
           "advance before firmware completion");
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
    expect(state, !dspic33_pmp_transmit(cpu, &transfer),
           "busy write does not queue transfer");
}

static void access_lane_cases(PmpConformance* state, Dspic33* cpu) {
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

static void sixteen_bit_lane_cases(PmpConformance* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0x6789u);
    dspic33_write_byte(cpu, (uint16_t)(PMP_DATA + 1u), 0xaau);
    expect(state, !cpu->io.pmp.active && cpu->events.count == 0u,
           "16-bit high-byte write only loads the data latch");
    dspic33_write_byte(cpu, PMP_DATA, 0x55u);
    expect(state,
           cpu->io.pmp.active && cpu->io.pmp.value == 0xaa55u &&
               cpu->io.pmp.width == 2u,
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

static void master_write_matrix_cases(PmpConformance* state, Dspic33* cpu) {
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
               cpu->io.pmp.active && ((dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) !=
                                      0u) == (current->cycles > 1u),
               "master write matrix starts with documented BUSY state");
        expect(state, dspic33_device_advance(cpu, current->cycles - 1u),
               "advance master write matrix before completion");
        expect(state,
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
                   !dspic33_pmp_transmit(cpu, &transfer),
               "master write matrix clears BUSY on its final cycle");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance master write matrix completion");
        expect(state,
               dspic33_pmp_transmit(cpu, &transfer) && transfer.address == 0x2468u &&
                   transfer.control == (uint16_t)(current->control | PMP_ENABLE) &&
                   transfer.mode == current->mode &&
                   transfer.value == current->expected_value &&
                   transfer.width == current->width &&
                   transfer.cycle == current->cycles,
               "master write matrix captures width mode mux and timing");
    }
}

static void wait_state_matrix_cases(PmpConformance* state, Dspic33* cpu) {
    static const PmpMasterWriteCase cases[] = {
        {0u, 0x00c3u | PMP_MASTER_MODE_2, 0x005cu, 1u, 1u},
        {0u, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 9u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 13u},
        {PMP_FULL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 17u},
        {0u, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 18u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u,
         22u},
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
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance wait-state matrix completion");
        expect(state,
               dspic33_pmp_transmit(cpu, &transfer) &&
                   transfer.value == current->expected_value &&
                   transfer.width == current->width &&
                   transfer.cycle == current->cycles,
               "wait-state matrix completes on the documented phase count");
    }
}

static void address_update_cases(PmpConformance* state, Dspic33* cpu) {
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
        configure_pmp_control(
            cpu, cases[index].control,
            (uint16_t)(PMP_MASTER_MODE_2 | cases[index].mode | 0x0004u),
            cases[index].initial);
        dspic33_write_byte(cpu, PMP_DATA, 0x6au);
        expect(state,
               cpu->io.pmp.active &&
                   dspic33_read_word(cpu, PMP_ADDRESS) == cases[index].initial,
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
               dspic33_pmp_transmit(cpu, &transfer) &&
                   transfer.address == cases[index].initial && transfer.value == 0x6au,
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

static void master_read_pipeline_cases(PmpConformance* state, Dspic33* cpu) {
    uint16_t generation;
    size_t events;
    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, PMP_MASTER_MODE_2, 0x1234u, 0x5aa5u);
    expect(state, dspic33_pmp_respond(cpu, 0x1234u, 0u), "queue 8-bit master response");
    expect(state,
           dspic33_read_byte(cpu, (uint16_t)(PMP_DATA + 1u)) == 0x5au &&
               !cpu->io.pmp.active,
           "high-byte read does not initiate master access");
    expect(state,
           dspic33_read_byte(cpu, PMP_DATA) == 0xa5u && cpu->io.pmp.active &&
               cpu->io.pmp.reading && cpu->io.pmp.width == 1u &&
               !cpu->io.pmp.last_read_valid,
           "first low-byte read returns dummy and starts access");
    generation = cpu->io.pmp.generation;
    events = cpu->events.count;
    expect(state,
           dspic33_read_byte(cpu, PMP_DATA) == 0xa5u &&
               cpu->io.pmp.generation == generation && cpu->events.count == events,
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
    expect(state, dspic33_pmp_respond(cpu, 0x5678u, 0u),
           "queue sequential 8-bit response");
    expect(state, dspic33_read_byte(cpu, PMP_DATA) == 0x34u && cpu->io.pmp.active,
           "sequential read returns previous result and starts next access");
    expect(state,
           dspic33_device_advance(cpu, 1u) && cpu->io.pmp.last_read.value == 0x0078u &&
               pmp_data_latch(cpu) == 0x5a78u,
           "sequential 8-bit response replaces the prior result");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | PMP_INCREMENT | 0x0004u),
                       0x4000u, 0x1357u);
    expect(state,
           dspic33_pmp_respond(cpu, 0x1111u, 0u) &&
               dspic33_pmp_respond(cpu, 0x2222u, 0u),
           "queue final-cycle back-to-back responses");
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x1357u &&
               dspic33_device_advance(cpu, 2u),
           "advance first read through final BUSY cycle");
    expect(state,
           (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
               cpu->io.pmp.completing_active && !cpu->io.pmp.last_read_valid &&
               dspic33_read_word(cpu, PMP_ADDRESS) == 0x4001u,
           "final cycle retains first read before completion");
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x1357u && cpu->io.pmp.active &&
               cpu->io.pmp.completing_active &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u,
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
    expect(state,
           dspic33_pmp_respond(cpu, 0x3333u, 5u) &&
               dspic33_pmp_respond(cpu, 0x1111u, 0u),
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
               dspic33_device_advance(cpu, 1u) &&
               cpu->io.pmp.last_read.value == 0x0033u && cpu->io.pmp.input.count == 0u,
           "later master read consumes delayed response at its deadline");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, PMP_DATA_16_BIT | PMP_MASTER_MODE_2, 0x2345u, 0xbeefu);
    expect(state, dspic33_pmp_respond(cpu, 0x1234u, 0u),
           "queue 16-bit master response");
    expect(state,
           dspic33_read_byte(cpu, (uint16_t)(PMP_DATA + 1u)) == 0xbeu &&
               !cpu->io.pmp.active,
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

static void master_read_matrix_cases(PmpConformance* state, Dspic33* cpu) {
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
        expect(state, dspic33_pmp_respond(cpu, 0xab5cu, 0u),
               "queue master read matrix response");
        previous = dspic33_read_word(cpu, PMP_DATA);
        expect(
            state,
            previous == 0x5aa5u && cpu->io.pmp.active && cpu->io.pmp.reading &&
                ((dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) != 0u) ==
                    (current->cycles > 1u),
            "master read matrix returns prior value and starts with documented BUSY");
        expect(state, dspic33_device_advance(cpu, current->cycles - 1u),
               "advance master read matrix before completion");
        expect(state,
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
                   !cpu->io.pmp.last_read_valid,
               "master read matrix clears BUSY on its final cycle");
        expect(state, dspic33_device_advance(cpu, 1u),
               "advance master read matrix completion");
        expect(state,
               cpu->io.pmp.last_read_valid &&
                   cpu->io.pmp.last_read.address == 0x2468u &&
                   cpu->io.pmp.last_read.control ==
                       (uint16_t)(current->control | PMP_ENABLE) &&
                   cpu->io.pmp.last_read.mode == current->mode &&
                   cpu->io.pmp.last_read.value == current->expected_value &&
                   cpu->io.pmp.last_read.width == current->width &&
                   cpu->io.pmp.last_read.cycle == current->cycles &&
                   pmp_data_latch(cpu) == (current->width == 2u ? 0xab5cu : 0x5a5cu),
               "master read matrix captures width mode mux value and timing");
    }
}

static void read_wait_state_matrix_cases(PmpConformance* state, Dspic33* cpu) {
    static const PmpMasterReadCase cases[] = {
        {0u, 0x00c3u | PMP_MASTER_MODE_2, 0x005cu, 1u, 1u},
        {0u, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 9u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 13u},
        {PMP_FULL_MUX, 0x00c7u | PMP_MASTER_MODE_2, 0x005cu, 1u, 17u},
        {0u, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 18u},
        {PMP_PARTIAL_MUX, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u,
         22u},
        {PMP_FULL_MUX, 0x00c7u | PMP_DATA_16_BIT | PMP_MASTER_MODE_3, 0xab5cu, 2u, 26u},
    };
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); index++) {
        const PmpMasterReadCase* current = &cases[index];
        dspic33_reset(cpu, 0u);
        configure_pmp_read(cpu, current->control, current->mode, 0x1357u, 0u);
        expect(state, dspic33_pmp_respond(cpu, 0xab5cu, 0u),
               "queue wait-state read response");
        dspic33_read_word(cpu, PMP_DATA);
        expect(state, dspic33_device_advance(cpu, current->cycles - 1u),
               "advance read wait-state matrix before completion");
        expect(state,
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u &&
                   !cpu->io.pmp.last_read_valid,
               "read wait-state matrix clears BUSY before completion");
        expect(state,
               dspic33_device_advance(cpu, 1u) &&
                   cpu->io.pmp.last_read.value == current->expected_value &&
                   cpu->io.pmp.last_read.width == current->width &&
                   cpu->io.pmp.last_read.cycle == current->cycles,
               "read wait-state matrix completes on documented phase count");
    }
}

static void read_address_update_cases(PmpConformance* state, Dspic33* cpu) {
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
        expect(state, dspic33_pmp_respond(cpu, 0x006au, 0u),
               "queue read address response");
        dspic33_read_byte(cpu, PMP_DATA);
        expect(state,
               cpu->io.pmp.active &&
                   dspic33_read_word(cpu, PMP_ADDRESS) == cases[index].initial,
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

static void read_interrupt_dma_cases(PmpConformance* state, Dspic33* cpu) {
    uint16_t index;
    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_INTERRUPT_EACH | PMP_MASTER_MODE_2), 0u,
                       0u);
    expect(state, dspic33_pmp_respond(cpu, 0x0044u, 0u),
           "queue interrupting read response");
    dspic33_read_byte(cpu, PMP_DATA);
    expect(state, dspic33_device_advance(cpu, 1u), "complete interrupting PMP read");
    expect(state,
           (dspic33_read_word(cpu, 0x0804u) & PMP_INTERRUPT_FLAG) != 0u &&
               cpu->io.pmp.last_read.value == 0x0044u && cpu->io.pmp.output.count == 0u,
           "master read completion raises PMPIF without transmit output");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u,
                       (uint16_t)(PMP_INTERRUPT_EACH | PMP_DATA_16_BIT |
                                  PMP_MASTER_MODE_2 | PMP_INCREMENT),
                       0u, 0u);
    for (index = 0u; index < 4u; index++) {
        expect(state, dspic33_pmp_respond(cpu, (uint16_t)(0x1100u + index), 0u),
               "queue DMA read response");
    }
    configure_dma_read(cpu, 0u, PMP_DMA_REQUEST, 0x4000u, PMP_DATA, 2u);
    expect(state, dspic33_read_word(cpu, PMP_DATA) == 0u,
           "dummy read starts DMA response chain");
    expect(state, dspic33_device_advance(cpu, 12u),
           "advance complete PMP read DMA chain");
    expect(state,
           dspic33_read_word(cpu, 0x4000u) == 0x1100u &&
               dspic33_read_word(cpu, 0x4002u) == 0x1101u &&
               dspic33_read_word(cpu, 0x4004u) == 0x1102u,
           "PMP interrupt DMA stores sequential completed reads");
    expect(state,
           (dspic33_read_word(cpu, 0x0b00u) & 0x8000u) == 0u &&
               (dspic33_read_word(cpu, 0x0800u) & 0x0010u) != 0u &&
               dspic33_read_word(cpu, PMP_ADDRESS) == 4u &&
               cpu->io.pmp.input.count == 0u,
           "DMA completion leaves one documented pipelined PMP read");
}

static void read_lifecycle_cases(PmpConformance* state, Dspic33* cpu) {
    Dspic33 copy;
    bool initialized = dspic33_initialize(&copy);
    expect(state, initialized, "initialize copied PMP read processor");
    if (!initialized) {
        return;
    }
    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0x3456u, 0u);
    expect(state,
           dspic33_pmp_respond(cpu, 0xa55au, 0u) &&
               dspic33_read_word(cpu, PMP_DATA) == 0u && dspic33_copy(&copy, cpu),
           "copy active PMP read and response");
    expect(state,
           copy.io.pmp.active && copy.io.pmp.reading && copy.io.pmp.input.count == 1u &&
               copy.events.count == cpu->events.count,
           "copy retains active PMP read state");
    expect(state,
           dspic33_device_advance(cpu, 3u) && dspic33_device_advance(&copy, 3u) &&
               cpu->io.pmp.last_read.value == 0x005au &&
               copy.io.pmp.last_read.value == 0x005au,
           "copied PMP reads complete independently");

    dspic33_reset(cpu, 0u);
    configure_pmp_read(cpu, 0u, (uint16_t)(PMP_MASTER_MODE_2 | 0x0004u), 0u, 0u);
    expect(state, dspic33_pmp_respond(cpu, 0x7788u, 0u),
           "queue disabled PMP read response");
    dspic33_read_word(cpu, PMP_DATA);
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    expect(state,
           !cpu->io.pmp.active && !cpu->io.pmp.last_read_valid &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP disable aborts active read");
    expect(state, dspic33_device_advance(cpu, 3u) && !cpu->io.pmp.last_read_valid,
           "stale PMP read event produces no result");

    expect(state, dspic33_pmp_respond(cpu, 0x1122u, 5u),
           "queue response before processor reset");
    dspic33_reset(cpu, 0u);
    expect(state, cpu->io.pmp.input.count == 0u,
           "processor reset clears queued PMP responses");

    cpu->io.pmp.input.count = DSPIC33_PMP_QUEUE_SIZE;
    expect(state, !dspic33_pmp_respond(cpu, 0x3344u, 0u),
           "full PMP response queue rejects input");
    cpu->io.pmp.input.count = 0u;
    cpu->device_cycles = UINT64_MAX;
    expect(state, !dspic33_pmp_respond(cpu, 0x5566u, 1u),
           "PMP response cycle overflow is rejected");

    configure_pmp_read(cpu, 0u, PMP_MASTER_MODE_2, 0u, 0x99aau);
    expect(state,
           dspic33_read_word(cpu, PMP_DATA) == 0x99aau && !cpu->io.pmp.active &&
               cpu->events.count == 0u &&
               (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
           "PMP read scheduling overflow rolls back transfer");
    dspic33_destroy(&copy);
}

static void interrupt_cases(PmpConformance* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    uint16_t priority_address = (uint16_t)(0x0840u + (PMP_IRQ / 4u) * 2u);
    uint16_t priority_shift = (uint16_t)((PMP_IRQ % 4u) * 4u);
    dspic33_reset(cpu, 0u);
    configure_pmp(cpu, PMP_MASTER_MODE_2, 0u);
    dspic33_write_byte(cpu, PMP_DATA, 0x33u);
    expect(state, dspic33_device_advance(cpu, 1u), "noninterrupting PMP completes");
    expect(state, dspic33_pmp_transmit(cpu, &transfer),
           "noninterrupting output captured");
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
    dspic33_write_word(cpu, priority_address,
                       (uint16_t)((dspic33_read_word(cpu, priority_address) &
                                   ~(7u << priority_shift)) |
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

static void dma_chain_cases(PmpConformance* state, Dspic33* cpu) {
    Dspic33PmpTransfer transfer;
    uint16_t index;
    dspic33_reset(cpu, 0u);
    for (index = 0u; index < PMP_TRANSFER_COUNT; index++) {
        dspic33_write_byte(cpu, (uint16_t)(PMP_DMA_SOURCE + index), (uint8_t)index);
    }
    configure_pmp(cpu, PMP_FIRMWARE_MODE, 0u);
    configure_dma(cpu, PMP_DMA_CHANNEL, PMP_DMA_REQUEST, PMP_DMA_SOURCE, PMP_DATA,
                  PMP_TRANSFER_COUNT - 1u);
    dspic33_write_word(cpu, (uint16_t)(PMP_DMA_BASE + 2u),
                       (uint16_t)(0x8000u | PMP_DMA_REQUEST));
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

static void dma_negative_cases(PmpConformance* state, Dspic33* cpu) {
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
        if (!dspic33_pmp_transmit(cpu, &transfer) ||
            transfer.value != (uint8_t)(0xa0u + value)) {
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
    expect(state, !dspic33_pmp_transmit(cpu, &transfer),
           "wrong PAD does not start PMP");

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

static void lifecycle_cases(PmpConformance* state, Dspic33* cpu) {
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
    expect(state,
           copy.io.pmp.active && copy.io.pmp.value == 0x81u && copy.events.count == 2u,
           "copy retains PMP event state");
    expect(state,
           dspic33_device_advance(cpu, 20u) && dspic33_device_advance(&copy, 20u),
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
    expect(state,
           !cpu->io.pmp.active && (dspic33_read_word(cpu, PMP_MODE) & PMP_BUSY) == 0u,
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
    dspic33_destroy(&copy);
}

int main(void) {
    Dspic33 cpu;
    PmpConformance state = {0u, 0u, 0u};
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
        expect(&state, state.cases == 380u, "PMP assertion accounting");
        dspic33_destroy(&cpu);
    }
    printf("[pmp-summary] cases=%u passed=%u failed=%u\n", state.cases, state.passed,
           state.failed);
    return state.failed == 0u ? 0 : 1;
}
