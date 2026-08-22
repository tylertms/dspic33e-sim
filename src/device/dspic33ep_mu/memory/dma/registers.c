#include "device/dspic33ep_mu/internal.h"

bool dspic33_device_internal_dma_register_write_mask(uint16_t address, uint16_t* writable) {
    static const uint16_t channel_masks[] = {0xf833u, 0x80ffu, 0xffffu, 0x00ffu,
                                             0xffffu, 0x00ffu, 0xffffu, 0x3fffu};
    if (address >= DMA_CHANNEL_BASE &&
        address < DMA_CHANNEL_BASE + DSPIC33_DMA_COUNT * DMA_CHANNEL_STRIDE) {
        *writable = channel_masks[((address - DMA_CHANNEL_BASE) & 0x000fu) / 2u];
        return true;
    }
    if (address >= DMA_PWC && address <= DMA_SADRH) {
        *writable = 0u;
        return true;
    }
    return false;
}

bool dspic33_device_internal_interrupt_control_write(Dspic33* cpu, uint16_t base, uint16_t previous,
                                                     uint16_t requested) {
    uint16_t current;
    if (base == 0x08c0u) {
        current = (uint16_t)(requested & 0xfffeu);
        dspic33_device_internal_raw_write_word(cpu, base, current);
        if ((previous & 0x0020u) != 0u && (current & 0x0020u) == 0u) {
            dspic33_device_internal_raw_write_word(cpu, DMA_PWC, 0u);
            dspic33_device_internal_raw_write_word(cpu, DMA_RQC, 0u);
        }
        if ((current & 0x0010u) == 0u) {
            dspic33_set_math_error_source(cpu, false);
        }
        return true;
    }
    if (base == 0x08c2u) {
        current = (uint16_t)((previous & 0x4000u) | (requested & 0xa01fu));
        dspic33_device_internal_raw_write_word(cpu, base, current);
        if ((current & 0x2000u) != 0u) {
            dspic33_device_internal_raw_write_word(
                cpu, 0x08c6u, (uint16_t)(dspic33_device_internal_raw_word(cpu, 0x08c6u) | 0x0001u));
        }
        dspic33_set_generic_hard_trap_source(
            cpu, (current & 0x2000u) != 0u ||
                     (dspic33_device_internal_raw_word(cpu, 0x08c6u) & 0x0001u) != 0u);
        if ((current & 0x8000u) != 0u) {
            cpu->gie_disable_deferred = 0u;
            cpu->gie_disable_deferred_next = 0u;
        } else if ((previous & 0x8000u) != 0u) {
            cpu->gie_disable_deferred_next = 1u;
        }
        return true;
    }
    if (base == 0x08c4u) {
        current = (uint16_t)(requested & 0x0070u);
        dspic33_device_internal_raw_write_word(cpu, base, current);
        dspic33_set_generic_soft_trap_source(cpu, current != 0u);
        return true;
    }
    if (base == 0x08c6u) {
        current = (uint16_t)(requested & 0x0001u);
        dspic33_device_internal_raw_write_word(cpu, base, current);
        dspic33_set_generic_hard_trap_source(
            cpu, current != 0u || (dspic33_device_internal_raw_word(cpu, 0x08c2u) & 0x2000u) != 0u);
        return true;
    }
    if (base == 0x08c8u) {
        dspic33_device_internal_raw_write_word(cpu, base, previous);
        return true;
    }
    return false;
}

static uint32_t dma_register_address(const Dspic33* cpu, uint16_t base, uint16_t low_offset) {
    return ((uint32_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + low_offset + 2u)) &
                       0x00ffu)
            << 16u) |
           dspic33_device_internal_raw_word(cpu, (uint16_t)(base + low_offset));
}

static void initialize_dma_channel(Dspic33* cpu, uint8_t channel) {
    uint16_t base = dspic33_device_internal_dma_channel_base(channel);
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    cpu->io.dma_index[channel] = 0u;
    cpu->io.dma_start_a[channel] = dma_register_address(cpu, base, 4u);
    cpu->io.dma_start_b[channel] = dma_register_address(cpu, base, 8u);
    cpu->io.dma_address[channel] = cpu->io.dma_start_a[channel];
    cpu->io.dma_bank &= (uint16_t)~bit;
    cpu->io.dma_half_raised &= (uint16_t)~bit;
    cpu->io.dma_forced_pending &= (uint16_t)~bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
    cpu->io.dma_active &= (uint16_t)~bit;
    cpu->io.dma_enabled |= bit;
    dspic33_device_internal_dma_advance_generation(cpu, channel);
    dspic33_device_internal_raw_write_word(
        cpu, DMA_PPS, (uint16_t)(dspic33_device_internal_raw_word(cpu, DMA_PPS) & ~bit));
}

void dspic33_device_internal_update_dma_control(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dspic33_device_internal_dma_channel_base(channel);
    uint16_t current = dspic33_device_internal_raw_word(cpu, base);
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    bool was_enabled = (previous & DMA_CON_CHEN) != 0u;
    bool enabled = (current & DMA_CON_CHEN) != 0u;
    if (enabled && !was_enabled) {
        initialize_dma_channel(cpu, channel);
    } else if (!enabled && was_enabled) {
        cpu->io.dma_enabled &= (uint16_t)~bit;
        cpu->io.dma_forced_pending &= (uint16_t)~bit;
        cpu->io.dma_peripheral_pending &= (uint16_t)~bit;
        cpu->io.dma_active &= (uint16_t)~bit;
        cpu->io.dma_arbiter_waiting &= (uint16_t)~bit;
        dspic33_device_internal_dma_advance_generation(cpu, channel);
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(base + 2u),
            (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(base + 2u)) &
                       ~DMA_REQ_FORCE));
    }
}
void dspic33_device_internal_update_dma_request(Dspic33* cpu, uint8_t channel, uint16_t previous) {
    uint16_t base = dspic33_device_internal_dma_channel_base(channel);
    uint16_t address = (uint16_t)(base + 2u);
    uint16_t request = dspic33_device_internal_raw_word(cpu, address);
    uint16_t bit = dspic33_device_internal_dma_channel_bit(channel);
    if ((dspic33_device_internal_raw_word(cpu, base) & DMA_CON_CHEN) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((previous & DMA_REQ_FORCE) != 0u) {
        dspic33_device_internal_raw_write_word(cpu, address, (uint16_t)(request | DMA_REQ_FORCE));
        return;
    }
    if ((request & DMA_REQ_FORCE) == 0u) {
        return;
    }
    if ((cpu->io.dma_active & bit) != 0u) {
        dspic33_device_internal_dma_request_collision(cpu, channel);
        dspic33_device_internal_raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_peripheral_pending & bit) != 0u) {
        dspic33_device_internal_dma_request_collision(cpu, channel);
        dspic33_device_internal_raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_forced_pending & bit) == 0u &&
        !dspic33_device_internal_schedule_dma_channel(cpu, channel, 0u, true, 1u)) {
        dspic33_device_internal_raw_write_word(cpu, address, (uint16_t)(request & ~DMA_REQ_FORCE));
    }
}
