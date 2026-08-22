#include "device/dspic33ep_mu/internal.h"

void dspic33_device_internal_update_nvm_key(Dspic33* cpu, uint16_t requested) {
    uint8_t key = (uint8_t)requested;
    if (key == 0x55u) {
        cpu->nvm.key_stage = 1u;
        cpu->nvm.key_instruction = cpu->instructions;
        cpu->nvm.key_interrupt_count = cpu->interrupt_count;
        cpu->nvm.key_trap_count = cpu->trap_count;
    } else if (key == 0xaau && cpu->nvm.key_stage == 1u &&
               cpu->interrupt_count == cpu->nvm.key_interrupt_count &&
               cpu->trap_count == cpu->nvm.key_trap_count) {
        cpu->nvm.key_stage = 2u;
        cpu->nvm.key_instruction = cpu->instructions;
    } else {
        cpu->nvm.key_stage = 0u;
    }
    dspic33_device_internal_raw_write_word(cpu, NVM_KEY, 0u);
}

static uint8_t crc_write_width(const Dspic33* cpu) {
    if (cpu->io.dma_transfer_active) {
        return cpu->io.dma_transfer_width;
    }
    return cpu->io.cpu_write_valid ? cpu->io.cpu_write_width : 1u;
}

static void update_crc_data(Dspic33* cpu, uint16_t address, uint16_t requested) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    uint8_t width = dspic33_device_internal_crc_data_width(cpu);
    uint8_t write_width = crc_write_width(cpu);
    bool high_byte = (address & 1u) != 0u;
    if ((dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_ENABLE) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, base, 0u);
        return;
    }
    if (base == CRC_DATA_LOW) {
        if (width <= 8u) {
            if (write_width == 1u) {
                dspic33_device_internal_crc_push(cpu, requested >> (high_byte ? 8u : 0u));
            } else {
                dspic33_device_internal_crc_push(cpu, requested);
            }
        } else if (width <= 16u) {
            if (write_width == 2u) {
                dspic33_device_internal_crc_push(cpu, requested);
                cpu->io.crc.data_latch = 0u;
            }
        } else if (write_width == 2u) {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffff0000u) | requested;
        } else if (high_byte) {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffff00ffu) | (requested & 0xff00u);
        } else {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffffff00u) | (requested & 0x00ffu);
        }
    } else if (width > 16u) {
        if (write_width == 2u) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0x0000ffffu) | ((uint32_t)requested << 16u);
            dspic33_device_internal_crc_push(cpu, cpu->io.crc.data_latch);
            cpu->io.crc.data_latch = 0u;
        } else if (high_byte) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0x00ffffffu) | ((uint32_t)(requested & 0xff00u) << 16u);
            if (width > 24u) {
                dspic33_device_internal_crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        } else {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0xff00ffffu) | ((uint32_t)(requested & 0x00ffu) << 16u);
            if (width <= 24u) {
                dspic33_device_internal_crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        }
    }
    dspic33_device_internal_raw_write_word(cpu, base, 0u);
}

static void update_crc_control(Dspic33* cpu, uint16_t previous) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    bool enabled = (control & CRC_ENABLE) != 0u;
    bool was_go = (previous & CRC_GO) != 0u;
    bool go = (control & CRC_GO) != 0u;
    if (!enabled) {
        dspic33_device_internal_crc_reset_runtime(cpu);
        return;
    }
    if (was_go && !go) {
        dspic33_device_internal_crc_abort(cpu);
    }
    dspic33_device_internal_crc_refresh_status(cpu);
    if (!was_go && go) {
        dspic33_device_internal_crc_start_if_ready(cpu);
    }
}

void dspic33_device_internal_update_crc_register(Dspic33* cpu, uint16_t address, uint16_t previous,
                                                 uint16_t requested) {
    uint16_t base = (uint16_t)(address & 0xfffeu);
    if (base == CRC_PMD_ADDRESS) {
        dspic33_device_internal_update_crc_pmd(cpu, previous);
    } else if (base < CRC_CONTROL || base > CRC_SHIFT_HIGH) {
        return;
    } else if (cpu->io.crc.pmd_disabled) {
        dspic33_device_internal_raw_write_word(cpu, base, previous);
    } else if (base == CRC_CONTROL) {
        update_crc_control(cpu, previous);
    } else if (base == CRC_DATA_LOW || base == CRC_DATA_HIGH) {
        update_crc_data(cpu, address, requested);
    } else if ((base == CRC_SHIFT_LOW || base == CRC_SHIFT_HIGH) &&
               (dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_GO) != 0u) {
        dspic33_device_internal_raw_write_word(cpu, base, previous);
    }
}

static void fail_nvm_write(Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, NVM_CONTROL);
    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = false;
    dspic33_device_internal_raw_write_word(cpu, NVM_CONTROL,
                                           (uint16_t)((control & ~NVM_WRITE) | NVM_WRITE_ERROR));
}

static bool nvm_target_valid(const Dspic33* cpu, uint16_t control, uint32_t address) {
    switch (control & 0x000fu) {
    case 0u:
        return address == 0xf80004u || address == 0xf80006u || address == 0xf80008u ||
               address == 0xf8000au || address == 0xf8000cu || address == 0xf8000eu ||
               address == 0xf80010u || address == 0xf80012u;
    case 1u:
        address &= 0x00fffffcu;
        return dspic33_device_program_range_implemented(cpu, address, 4u);
    case 2u:
        address &= 0x00ffff00u;
        return dspic33_device_program_range_implemented(cpu, address,
                                                        DSPIC33_WRITE_LATCH_WORDS * 2u);
    case 3u:
        address &= 0x00fff800u;
        return dspic33_device_program_range_implemented(cpu, address, 0x800u);
    case 0x0au:
    case 0x0du:
        return true;
    default:
        return false;
    }
}

void dspic33_device_internal_update_nvm_control(Dspic33* cpu, uint16_t requested) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, NVM_CONTROL);
    uint32_t target = ((uint32_t)dspic33_device_internal_raw_word(cpu, NVM_ADDRESS_HIGH) << 16u) |
                      dspic33_device_internal_raw_word(cpu, NVM_ADDRESS);
    uint64_t completion_delay = cpu->non_cpu_sfr_read ? 3u : 2u;
    bool write_requested = (requested & NVM_WRITE) != 0u;
    if (cpu->nvm.active) {
        dspic33_device_internal_raw_write_word(cpu, NVM_CONTROL,
                                               (uint16_t)(control | NVM_WRITE | NVM_WRITE_ERROR));
        return;
    }
    if (!write_requested) {
        return;
    }
    if ((control & NVM_WRITE_ENABLE) == 0u || !dspic33_device_internal_nvm_key_authorized(cpu) ||
        cpu->cycles > UINT64_MAX - completion_delay || !nvm_target_valid(cpu, control, target)) {
        fail_nvm_write(cpu);
        return;
    }
    cpu->nvm.control = control;
    cpu->nvm.address = target;
    cpu->nvm.auxiliary_origin =
        cpu->instruction_active ? cpu->current_instruction_pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                                      cpu->current_instruction_pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT
                                : cpu->pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                                      cpu->pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT;
    cpu->nvm.stall_workaround = (dspic33_device_internal_raw_word(cpu, 0x08c2u) & 0x8000u) == 0u;
    memcpy(cpu->nvm.latches, cpu->write_latches, sizeof(cpu->nvm.latches));
    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = true;
    cpu->nvm.completion_cycle = cpu->cycles + completion_delay;
    dspic33_device_internal_raw_write_word(cpu, NVM_CONTROL,
                                           (uint16_t)(control | NVM_WRITE | NVM_WRITE_ERROR));
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_NVM, 0u, 0u,
                          dspic33_device_instruction_cycles(cpu, completion_delay))) {
        fail_nvm_write(cpu);
    }
}
