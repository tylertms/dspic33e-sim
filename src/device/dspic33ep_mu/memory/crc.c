#include "device/dspic33ep_mu/internal.h"

uint8_t dspic33_device_internal_crc_data_width(const Dspic33* cpu) {
    return (uint8_t)(((dspic33_device_internal_raw_word(cpu, CRC_CONFIG) >> 8u) & 0x001fu) + 1u);
}

static uint8_t crc_polynomial_width(const Dspic33* cpu) {
    return (uint8_t)((dspic33_device_internal_raw_word(cpu, CRC_CONFIG) & 0x001fu) + 1u);
}

static uint8_t crc_capacity(const Dspic33* cpu) {
    uint8_t width = dspic33_device_internal_crc_data_width(cpu);
    return width <= 8u ? 16u : width <= 16u ? 8u : 4u;
}

static uint32_t crc_width_mask(uint8_t width) {
    return width == 32u ? UINT32_MAX : ((uint32_t)1u << width) - 1u;
}

void dspic33_device_internal_crc_refresh_status(Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    uint16_t status = (uint16_t)((uint16_t)cpu->io.crc.count << 8u);
    if (cpu->io.crc.count == 0u) {
        status |= CRC_EMPTY;
    }
    if (cpu->io.crc.count >= crc_capacity(cpu)) {
        status |= CRC_FULL;
    }
    dspic33_device_internal_raw_write_word(
        cpu, CRC_CONTROL,
        (uint16_t)((control & ~(CRC_WORD_COUNT_MASK | CRC_FULL | CRC_EMPTY)) | status));
}

void dspic33_device_internal_crc_abort(Dspic33* cpu) {
    cpu->io.crc.generation++;
    cpu->io.crc.active = false;
    cpu->io.crc.bits_remaining = 0u;
}

void dspic33_device_internal_crc_reset_runtime(Dspic33* cpu) {
    uint16_t generation = (uint16_t)(cpu->io.crc.generation + 1u);
    uint16_t pmd_generation = cpu->io.crc.pmd_generation;
    uint16_t control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    bool pmd_disabled = cpu->io.crc.pmd_disabled;
    memset(&cpu->io.crc, 0, sizeof(cpu->io.crc));
    cpu->io.crc.generation = generation;
    cpu->io.crc.pmd_generation = pmd_generation;
    cpu->io.crc.pmd_disabled = pmd_disabled;
    dspic33_device_internal_raw_write_word(cpu, CRC_DATA_LOW, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_DATA_HIGH, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_SHIFT_LOW, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_SHIFT_HIGH, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_CONTROL, (uint16_t)(control & ~CRC_GO));
    dspic33_device_internal_crc_refresh_status(cpu);
}

static bool crc_schedule(Dspic33* cpu) {
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CRC, 0u, cpu->io.crc.generation, 1u)) {
        dspic33_device_internal_crc_abort(cpu);
        dspic33_device_internal_raw_write_word(
            cpu, CRC_CONTROL,
            (uint16_t)(dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & ~CRC_GO));
        return false;
    }
    cpu->io.crc.active = true;
    return true;
}

void dspic33_device_internal_crc_start_if_ready(Dspic33* cpu) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    if ((control & (CRC_ENABLE | CRC_GO)) == (CRC_ENABLE | CRC_GO) && !cpu->io.crc.active &&
        cpu->io.crc.count != 0u) {
        crc_schedule(cpu);
    }
}

void dspic33_device_internal_crc_push(Dspic33* cpu, uint32_t value) {
    uint8_t capacity = crc_capacity(cpu);
    uint8_t index;
    if (cpu->io.crc.count >= capacity) {
        return;
    }
    index = (uint8_t)((cpu->io.crc.head + cpu->io.crc.count) % 16u);
    cpu->io.crc.words[index] = value & crc_width_mask(dspic33_device_internal_crc_data_width(cpu));
    cpu->io.crc.count++;
    dspic33_device_internal_crc_refresh_status(cpu);
    dspic33_device_internal_crc_start_if_ready(cpu);
}

static uint32_t crc_shift_register(const Dspic33* cpu) {
    return (uint32_t)dspic33_device_internal_raw_word(cpu, CRC_SHIFT_LOW) |
           ((uint32_t)dspic33_device_internal_raw_word(cpu, CRC_SHIFT_HIGH) << 16u);
}

static void crc_write_shift_register(Dspic33* cpu, uint32_t value) {
    dspic33_device_internal_raw_write_word(cpu, CRC_SHIFT_LOW, (uint16_t)value);
    dspic33_device_internal_raw_write_word(cpu, CRC_SHIFT_HIGH, (uint16_t)(value >> 16u));
}

static void crc_load_shift_data(Dspic33* cpu) {
    uint32_t polynomial =
        (uint32_t)dspic33_device_internal_raw_word(cpu, CRC_POLYNOMIAL_LOW) |
        ((uint32_t)dspic33_device_internal_raw_word(cpu, CRC_POLYNOMIAL_HIGH) << 16u);
    cpu->io.crc.shift_data = cpu->io.crc.words[cpu->io.crc.head];
    cpu->io.crc.head = (uint8_t)((cpu->io.crc.head + 1u) % 16u);
    cpu->io.crc.count--;
    cpu->io.crc.data_width = dspic33_device_internal_crc_data_width(cpu);
    cpu->io.crc.polynomial_width = crc_polynomial_width(cpu);
    cpu->io.crc.bits_remaining = cpu->io.crc.data_width;
    cpu->io.crc.polynomial = (polynomial | 1u) & crc_width_mask(cpu->io.crc.polynomial_width);
    cpu->io.crc.little_endian =
        (dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_LITTLE_ENDIAN) != 0u;
    dspic33_device_internal_crc_refresh_status(cpu);
    if (cpu->io.crc.count == 0u &&
        (dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_INTERRUPT_EMPTY) != 0u) {
        dspic33_raise_interrupt(cpu, CRC_IRQ);
    }
}

static void crc_shift_bits(Dspic33* cpu) {
    uint32_t remainder = crc_shift_register(cpu) & crc_width_mask(cpu->io.crc.polynomial_width);
    uint8_t shifted;
    for (shifted = 0u; shifted < CRC_BITS_PER_CYCLE && cpu->io.crc.bits_remaining != 0u;
         shifted++) {
        uint8_t source_bit = cpu->io.crc.little_endian
                                 ? (uint8_t)(cpu->io.crc.data_width - cpu->io.crc.bits_remaining)
                                 : (uint8_t)(cpu->io.crc.bits_remaining - 1u);
        bool feedback = ((remainder >> (cpu->io.crc.polynomial_width - 1u)) & 1u) != 0u;
        feedback = feedback != ((cpu->io.crc.shift_data >> source_bit) & 1u);
        remainder = (remainder << 1u) & crc_width_mask(cpu->io.crc.polynomial_width);
        if (feedback) {
            remainder ^= cpu->io.crc.polynomial;
        }
        cpu->io.crc.bits_remaining--;
    }
    crc_write_shift_register(cpu, remainder);
}

void dspic33_device_internal_run_crc(Dspic33* cpu, uint16_t generation) {
    uint16_t control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    if (generation != cpu->io.crc.generation || !cpu->io.crc.active ||
        (control & (CRC_ENABLE | CRC_GO)) != (CRC_ENABLE | CRC_GO)) {
        return;
    }
    if (cpu->io.crc.pmd_disabled || cpu->power_state == DSPIC33_POWER_SLEEP ||
        (cpu->power_state == DSPIC33_POWER_IDLE && (control & CRC_STOP_IDLE) != 0u)) {
        crc_schedule(cpu);
        return;
    }
    if (cpu->io.crc.bits_remaining == 0u) {
        if (cpu->io.crc.count == 0u) {
            cpu->io.crc.active = false;
            dspic33_device_internal_raw_write_word(cpu, CRC_CONTROL, (uint16_t)(control & ~CRC_GO));
            if ((control & CRC_INTERRUPT_EMPTY) == 0u) {
                dspic33_raise_interrupt(cpu, CRC_IRQ);
            }
            return;
        }
        crc_load_shift_data(cpu);
    }
    crc_shift_bits(cpu);
    if (cpu->io.crc.bits_remaining == 0u && cpu->io.crc.count == 0u) {
        cpu->io.crc.active = false;
        control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
        dspic33_device_internal_raw_write_word(cpu, CRC_CONTROL, (uint16_t)(control & ~CRC_GO));
        if ((control & CRC_INTERRUPT_EMPTY) == 0u) {
            dspic33_raise_interrupt(cpu, CRC_IRQ);
        }
        return;
    }
    crc_schedule(cpu);
}

void dspic33_device_internal_run_crc_pmd(Dspic33* cpu, uint32_t value) {
    uint16_t generation = (uint16_t)(value >> 1u);
    if (generation != cpu->io.crc.pmd_generation) {
        return;
    }
    cpu->io.crc.pmd_disabled = (value & 1u) != 0u;
    if (!cpu->io.crc.pmd_disabled) {
        dspic33_device_internal_crc_start_if_ready(cpu);
    }
}

void dspic33_device_internal_update_crc_pmd(Dspic33* cpu, uint16_t previous) {
    bool disabled = (dspic33_device_internal_raw_word(cpu, CRC_PMD_ADDRESS) & CRC_PMD) != 0u;
    if (((previous & CRC_PMD) != 0u) == disabled) {
        return;
    }
    cpu->io.crc.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CRC, CRC_EVENT_PMD_SOURCE,
                          ((uint32_t)cpu->io.crc.pmd_generation << 1u) | (disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        dspic33_device_internal_raw_write_word(cpu, CRC_PMD_ADDRESS, previous);
        cpu->io.crc.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}
