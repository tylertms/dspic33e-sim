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

bool dspic33_device_internal_interrupt_control_write(Dspic33* cpu, uint16_t register_address,
                                                     uint16_t previous_value,
                                                     uint16_t requested_value) {
    uint16_t current_value;

    if (register_address == 0x08c0u) {
        current_value = (uint16_t)(requested_value & 0xfffeu);
        dspic33_device_internal_raw_write_word(cpu, register_address, current_value);
        if ((previous_value & 0x0020u) != 0u && (current_value & 0x0020u) == 0u) {
            dspic33_device_internal_raw_write_word(cpu, DMA_PWC, 0u);
            dspic33_device_internal_raw_write_word(cpu, DMA_RQC, 0u);
        }
        if ((current_value & 0x0010u) == 0u) {
            dspic33_set_math_error_source(cpu, false);
        }
        return true;
    }
    if (register_address == 0x08c2u) {
        current_value = (uint16_t)((previous_value & 0x4000u) | (requested_value & 0xa01fu));
        dspic33_device_internal_raw_write_word(cpu, register_address, current_value);
        if ((current_value & 0x2000u) != 0u) {
            dspic33_device_internal_raw_write_word(
                cpu, 0x08c6u, (uint16_t)(dspic33_device_internal_raw_word(cpu, 0x08c6u) | 0x0001u));
        }
        dspic33_set_generic_hard_trap_source(
            cpu, (current_value & 0x2000u) != 0u ||
                     (dspic33_device_internal_raw_word(cpu, 0x08c6u) & 0x0001u) != 0u);
        if ((current_value & 0x8000u) != 0u) {
            cpu->gie_disable_deferred = 0u;
            cpu->gie_disable_deferred_next = 0u;
        } else if ((previous_value & 0x8000u) != 0u) {
            cpu->gie_disable_deferred_next = 1u;
        }
        return true;
    }
    if (register_address == 0x08c4u) {
        current_value = (uint16_t)(requested_value & 0x0070u);
        dspic33_device_internal_raw_write_word(cpu, register_address, current_value);
        dspic33_set_generic_soft_trap_source(cpu, current_value != 0u);
        return true;
    }
    if (register_address == 0x08c6u) {
        current_value = (uint16_t)(requested_value & 0x0001u);
        dspic33_device_internal_raw_write_word(cpu, register_address, current_value);
        dspic33_set_generic_hard_trap_source(
            cpu, current_value != 0u ||
                     (dspic33_device_internal_raw_word(cpu, 0x08c2u) & 0x2000u) != 0u);
        return true;
    }
    if (register_address == 0x08c8u) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_value);
        return true;
    }
    return false;
}

static uint32_t dma_register_address(const Dspic33* cpu, uint16_t channel_base,
                                     uint16_t register_offset) {
    return ((uint32_t)(dspic33_device_internal_raw_word(
                           cpu, (uint16_t)(channel_base + register_offset + 2u)) &
                       0x00ffu)
            << 16u) |
           dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + register_offset));
}

static void initialize_dma_channel(Dspic33* cpu, uint8_t channel_index) {
    const uint16_t channel_base = dspic33_device_internal_dma_channel_base(channel_index);
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);

    cpu->io.dma_index[channel_index] = 0u;
    cpu->io.dma_start_a[channel_index] = dma_register_address(cpu, channel_base, 4u);
    cpu->io.dma_start_b[channel_index] = dma_register_address(cpu, channel_base, 8u);
    cpu->io.dma_address[channel_index] = cpu->io.dma_start_a[channel_index];
    cpu->io.dma_bank &= (uint16_t)~channel_bit;
    cpu->io.dma_half_raised &= (uint16_t)~channel_bit;
    cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
    cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
    cpu->io.dma_active &= (uint16_t)~channel_bit;
    cpu->io.dma_enabled |= channel_bit;
    dspic33_device_internal_dma_advance_generation(cpu, channel_index);
    dspic33_device_internal_raw_write_word(
        cpu, DMA_PPS, (uint16_t)(dspic33_device_internal_raw_word(cpu, DMA_PPS) & ~channel_bit));
}

void dspic33_device_internal_update_dma_control(Dspic33* cpu, uint8_t channel_index,
                                                uint16_t previous_value) {
    const uint16_t channel_base = dspic33_device_internal_dma_channel_base(channel_index);
    const uint16_t current_control = dspic33_device_internal_raw_word(cpu, channel_base);
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);
    const bool was_enabled = (previous_value & DMA_CON_CHEN) != 0u;
    const bool is_enabled = (current_control & DMA_CON_CHEN) != 0u;

    if (is_enabled && !was_enabled) {
        initialize_dma_channel(cpu, channel_index);
    } else if (!is_enabled && was_enabled) {
        cpu->io.dma_enabled &= (uint16_t)~channel_bit;
        cpu->io.dma_forced_pending &= (uint16_t)~channel_bit;
        cpu->io.dma_peripheral_pending &= (uint16_t)~channel_bit;
        cpu->io.dma_active &= (uint16_t)~channel_bit;
        cpu->io.dma_arbiter_waiting &= (uint16_t)~channel_bit;
        dspic33_device_internal_dma_advance_generation(cpu, channel_index);
        dspic33_device_internal_raw_write_word(
            cpu, (uint16_t)(channel_base + 2u),
            (uint16_t)(dspic33_device_internal_raw_word(cpu, (uint16_t)(channel_base + 2u)) &
                       ~DMA_REQ_FORCE));
    }
}

void dspic33_device_internal_update_dma_request(Dspic33* cpu, uint8_t channel_index,
                                                uint16_t previous_value) {
    const uint16_t channel_base = dspic33_device_internal_dma_channel_base(channel_index);
    const uint16_t request_address = (uint16_t)(channel_base + 2u);
    uint16_t request_value = dspic33_device_internal_raw_word(cpu, request_address);
    const uint16_t channel_bit = dspic33_device_internal_dma_channel_bit(channel_index);

    if ((dspic33_device_internal_raw_word(cpu, channel_base) & DMA_CON_CHEN) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, request_address,
                                               (uint16_t)(request_value & ~DMA_REQ_FORCE));
        return;
    }
    if ((previous_value & DMA_REQ_FORCE) != 0u) {
        dspic33_device_internal_raw_write_word(cpu, request_address,
                                               (uint16_t)(request_value | DMA_REQ_FORCE));
        return;
    }
    if ((request_value & DMA_REQ_FORCE) == 0u) {
        return;
    }
    if ((cpu->io.dma_active & channel_bit) != 0u) {
        dspic33_device_internal_dma_request_collision(cpu, channel_index);
        dspic33_device_internal_raw_write_word(cpu, request_address,
                                               (uint16_t)(request_value & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_peripheral_pending & channel_bit) != 0u) {
        dspic33_device_internal_dma_request_collision(cpu, channel_index);
        dspic33_device_internal_raw_write_word(cpu, request_address,
                                               (uint16_t)(request_value & ~DMA_REQ_FORCE));
        return;
    }
    if ((cpu->io.dma_forced_pending & channel_bit) == 0u &&
        !dspic33_device_internal_schedule_dma_channel(cpu, channel_index, 0u, true, 1u)) {
        dspic33_device_internal_raw_write_word(cpu, request_address,
                                               (uint16_t)(request_value & ~DMA_REQ_FORCE));
    }
}
