#include "device/dspic33ep_mu/internal.h"

void dspic33_device_internal_update_nvm_key(Dspic33* cpu, uint16_t key_word) {
    const uint8_t key_value = (uint8_t)key_word;

    if (key_value == 0x55u) {
        cpu->nvm.key_stage = 1u;
        cpu->nvm.key_instruction = cpu->instructions;
        cpu->nvm.key_interrupt_count = cpu->interrupt_count;
        cpu->nvm.key_trap_count = cpu->trap_count;
    } else if (key_value == 0xaau && cpu->nvm.key_stage == 1u &&
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

static void update_crc_data(Dspic33* cpu, uint16_t access_address, uint16_t input_word) {
    const uint16_t register_address = (uint16_t)(access_address & 0xfffeu);
    const uint8_t data_width = dspic33_device_internal_crc_data_width(cpu);
    const uint8_t write_width = crc_write_width(cpu);
    const bool high_byte_access = (access_address & 1u) != 0u;

    if ((dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_ENABLE) == 0u) {
        dspic33_device_internal_raw_write_word(cpu, register_address, 0u);
        return;
    }
    if (register_address == CRC_DATA_LOW) {
        if (data_width <= 8u) {
            if (write_width == 1u) {
                dspic33_device_internal_crc_push(cpu, input_word >> (high_byte_access ? 8u : 0u));
            } else {
                dspic33_device_internal_crc_push(cpu, input_word);
            }
        } else if (data_width <= 16u) {
            if (write_width == 2u) {
                dspic33_device_internal_crc_push(cpu, input_word);
                cpu->io.crc.data_latch = 0u;
            }
        } else if (write_width == 2u) {
            cpu->io.crc.data_latch = (cpu->io.crc.data_latch & 0xffff0000u) | input_word;
        } else if (high_byte_access) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0xffff00ffu) | (input_word & 0xff00u);
        } else {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0xffffff00u) | (input_word & 0x00ffu);
        }
    } else if (data_width > 16u) {
        if (write_width == 2u) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0x0000ffffu) | ((uint32_t)input_word << 16u);
            dspic33_device_internal_crc_push(cpu, cpu->io.crc.data_latch);
            cpu->io.crc.data_latch = 0u;
        } else if (high_byte_access) {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0x00ffffffu) | ((uint32_t)(input_word & 0xff00u) << 16u);
            if (data_width > 24u) {
                dspic33_device_internal_crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        } else {
            cpu->io.crc.data_latch =
                (cpu->io.crc.data_latch & 0xff00ffffu) | ((uint32_t)(input_word & 0x00ffu) << 16u);
            if (data_width <= 24u) {
                dspic33_device_internal_crc_push(cpu, cpu->io.crc.data_latch);
                cpu->io.crc.data_latch = 0u;
            }
        }
    }
    dspic33_device_internal_raw_write_word(cpu, register_address, 0u);
}

static void update_crc_control(Dspic33* cpu, uint16_t previous_control) {
    const uint16_t crc_control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    const bool crc_enabled = (crc_control & CRC_ENABLE) != 0u;
    const bool was_running = (previous_control & CRC_GO) != 0u;
    const bool is_running = (crc_control & CRC_GO) != 0u;

    if (!crc_enabled) {
        dspic33_device_internal_crc_reset_runtime(cpu);
        return;
    }
    if (was_running && !is_running) {
        dspic33_device_internal_crc_abort(cpu);
    }
    dspic33_device_internal_crc_refresh_status(cpu);
    if (!was_running && is_running) {
        dspic33_device_internal_crc_start_if_ready(cpu);
    }
}

void dspic33_device_internal_update_crc_register(Dspic33* cpu, uint16_t access_address,
                                                 uint16_t previous_word, uint16_t requested_word) {
    const uint16_t register_address = (uint16_t)(access_address & 0xfffeu);

    if (register_address == CRC_PMD_ADDRESS) {
        dspic33_device_internal_update_crc_pmd(cpu, previous_word);
    } else if (register_address < CRC_CONTROL || register_address > CRC_SHIFT_HIGH) {
        return;
    } else if (cpu->io.crc.pmd_disabled) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_word);
    } else if (register_address == CRC_CONTROL) {
        update_crc_control(cpu, previous_word);
    } else if (register_address == CRC_DATA_LOW || register_address == CRC_DATA_HIGH) {
        update_crc_data(cpu, access_address, requested_word);
    } else if ((register_address == CRC_SHIFT_LOW || register_address == CRC_SHIFT_HIGH) &&
               (dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_GO) != 0u) {
        dspic33_device_internal_raw_write_word(cpu, register_address, previous_word);
    }
}

static void fail_nvm_write(Dspic33* cpu) {
    const uint16_t nvm_control = dspic33_device_internal_raw_word(cpu, NVM_CONTROL);

    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = false;
    dspic33_device_internal_raw_write_word(
        cpu, NVM_CONTROL, (uint16_t)((nvm_control & ~NVM_WRITE) | NVM_WRITE_ERROR));
}

static bool nvm_target_valid(const Dspic33* cpu, uint16_t nvm_control, uint32_t target_address) {
    switch (nvm_control & 0x000fu) {
    case 0u:
        return target_address == 0xf80004u || target_address == 0xf80006u ||
               target_address == 0xf80008u || target_address == 0xf8000au ||
               target_address == 0xf8000cu || target_address == 0xf8000eu ||
               target_address == 0xf80010u || target_address == 0xf80012u;
    case 1u:
        target_address &= 0x00fffffcu;
        return dspic33_device_program_range_implemented(cpu, target_address, 4u);
    case 2u:
        target_address &= 0x00ffff00u;
        return dspic33_device_program_range_implemented(cpu, target_address,
                                                        DSPIC33_WRITE_LATCH_WORDS * 2u);
    case 3u:
        target_address &= 0x00fff800u;
        return dspic33_device_program_range_implemented(cpu, target_address, 2u);
    case 0x0au:
    case 0x0du:
        return true;
    default:
        return false;
    }
}

void dspic33_device_internal_update_nvm_control(Dspic33* cpu, uint16_t requested_control) {
    const uint16_t nvm_control = dspic33_device_internal_raw_word(cpu, NVM_CONTROL);
    const uint32_t nvm_target_address =
        ((uint32_t)dspic33_device_internal_raw_word(cpu, NVM_ADDRESS_HIGH) << 16u) |
        dspic33_device_internal_raw_word(cpu, NVM_ADDRESS);
    const uint64_t nvm_completion_delay = cpu->non_cpu_sfr_read ? 3u : 2u;
    const bool write_requested = (requested_control & NVM_WRITE) != 0u;

    if (cpu->nvm.active) {
        dspic33_device_internal_raw_write_word(
            cpu, NVM_CONTROL, (uint16_t)(nvm_control | NVM_WRITE | NVM_WRITE_ERROR));
        return;
    }
    if (!write_requested) {
        return;
    }
    if ((nvm_control & NVM_WRITE_ENABLE) == 0u ||
        !dspic33_device_internal_nvm_key_authorized(cpu) ||
        cpu->cycles > UINT64_MAX - nvm_completion_delay ||
        !nvm_target_valid(cpu, nvm_control, nvm_target_address)) {
        fail_nvm_write(cpu);
        return;
    }
    cpu->nvm.control = nvm_control;
    cpu->nvm.address = nvm_target_address;
    cpu->nvm.auxiliary_origin =
        cpu->instruction_active ? cpu->current_instruction_pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                                      cpu->current_instruction_pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT
                                : cpu->pc >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
                                      cpu->pc < DSPIC33_AUXILIARY_PROGRAM_LIMIT;
    cpu->nvm.stall_workaround = (dspic33_device_internal_raw_word(cpu, 0x08c2u) & 0x8000u) == 0u;
    memcpy(cpu->nvm.latches, cpu->write_latches, sizeof(cpu->nvm.latches));
    cpu->nvm.key_stage = 0u;
    cpu->nvm.active = true;
    cpu->nvm.completion_cycle = cpu->cycles + nvm_completion_delay;
    dspic33_device_internal_raw_write_word(cpu, NVM_CONTROL,
                                           (uint16_t)(nvm_control | NVM_WRITE | NVM_WRITE_ERROR));
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_NVM, 0u, 0u,
                          dspic33_device_instruction_cycles(cpu, nvm_completion_delay))) {
        fail_nvm_write(cpu);
    }
}
