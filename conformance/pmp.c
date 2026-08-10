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
    PMP_MASTER_MODE_2 = 0x0200u,
    PMP_MASTER_MODE_3 = 0x0300u,
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

static void configure_pmp(Dspic33* cpu, uint16_t mode, uint16_t address) {
    dspic33_write_word(cpu, PMP_CONTROL, 0u);
    dspic33_write_word(cpu, PMP_MODE, mode);
    dspic33_write_word(cpu, PMP_ADDRESS, address);
    dspic33_write_word(cpu, PMP_CONTROL, PMP_ENABLE);
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
        interrupt_cases(&state, &cpu);
        dma_chain_cases(&state, &cpu);
        dma_negative_cases(&state, &cpu);
        lifecycle_cases(&state, &cpu);
        dspic33_destroy(&cpu);
    }
    printf("[pmp-summary] cases=%u passed=%u failed=%u\n", state.cases, state.passed,
           state.failed);
    return state.failed == 0u ? 0 : 1;
}
