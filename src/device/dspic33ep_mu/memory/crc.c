#include "device/dspic33ep_mu/internal.h"

uint8_t dspic33_device_internal_crc_data_width(const Dspic33* cpu) {
    return (uint8_t)(((dspic33_device_internal_raw_word(cpu, CRC_CONFIG) >> 8u) & 0x001fu) + 1u);
}

static uint8_t crc_polynomial_width(const Dspic33* cpu) {
    return (uint8_t)((dspic33_device_internal_raw_word(cpu, CRC_CONFIG) & 0x001fu) + 1u);
}

static uint8_t crc_capacity(const Dspic33* cpu) {
    const uint8_t data_width = dspic33_device_internal_crc_data_width(cpu);
    return data_width <= 8u ? 16u : data_width <= 16u ? 8u : 4u;
}

static uint32_t crc_width_mask(uint8_t bit_width) {
    return bit_width == 32u ? UINT32_MAX : ((uint32_t)1u << bit_width) - 1u;
}

void dspic33_device_internal_crc_refresh_status(Dspic33* cpu) {
    const uint16_t crc_control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    uint16_t crc_status = (uint16_t)((uint16_t)cpu->io.crc.count << 8u);

    if (cpu->io.crc.count == 0u) {
        crc_status |= CRC_EMPTY;
    }
    if (cpu->io.crc.count >= crc_capacity(cpu)) {
        crc_status |= CRC_FULL;
    }
    dspic33_device_internal_raw_write_word(
        cpu, CRC_CONTROL,
        (uint16_t)((crc_control & ~(CRC_WORD_COUNT_MASK | CRC_FULL | CRC_EMPTY)) | crc_status));
}

void dspic33_device_internal_crc_abort(Dspic33* cpu) {
    cpu->io.crc.generation++;
    cpu->io.crc.active = false;
    cpu->io.crc.bits_remaining = 0u;
}

void dspic33_device_internal_crc_reset_runtime(Dspic33* cpu) {
    const uint16_t generation = (uint16_t)(cpu->io.crc.generation + 1u);
    const uint16_t pmd_generation = cpu->io.crc.pmd_generation;
    const uint16_t crc_control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
    const bool pmd_disabled = cpu->io.crc.pmd_disabled;

    memset(&cpu->io.crc, 0, sizeof(cpu->io.crc));
    cpu->io.crc.generation = generation;
    cpu->io.crc.pmd_generation = pmd_generation;
    cpu->io.crc.pmd_disabled = pmd_disabled;
    dspic33_device_internal_raw_write_word(cpu, CRC_DATA_LOW, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_DATA_HIGH, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_SHIFT_LOW, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_SHIFT_HIGH, 0u);
    dspic33_device_internal_raw_write_word(cpu, CRC_CONTROL, (uint16_t)(crc_control & ~CRC_GO));
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
    const uint16_t crc_control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);

    if ((crc_control & (CRC_ENABLE | CRC_GO)) == (CRC_ENABLE | CRC_GO) && !cpu->io.crc.active &&
        cpu->io.crc.count != 0u) {
        crc_schedule(cpu);
    }
}

void dspic33_device_internal_crc_push(Dspic33* cpu, uint32_t data_value) {
    const uint8_t buffer_capacity = crc_capacity(cpu);
    uint8_t buffer_index;

    if (cpu->io.crc.count >= buffer_capacity) {
        return;
    }
    buffer_index = (uint8_t)((cpu->io.crc.head + cpu->io.crc.count) % 16u);
    cpu->io.crc.words[buffer_index] =
        data_value & crc_width_mask(dspic33_device_internal_crc_data_width(cpu));
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
    const uint32_t polynomial_value =
        (uint32_t)dspic33_device_internal_raw_word(cpu, CRC_POLYNOMIAL_LOW) |
        ((uint32_t)dspic33_device_internal_raw_word(cpu, CRC_POLYNOMIAL_HIGH) << 16u);
    cpu->io.crc.shift_data = cpu->io.crc.words[cpu->io.crc.head];
    cpu->io.crc.head = (uint8_t)((cpu->io.crc.head + 1u) % 16u);
    cpu->io.crc.count--;
    cpu->io.crc.data_width = dspic33_device_internal_crc_data_width(cpu);
    cpu->io.crc.polynomial_width = crc_polynomial_width(cpu);
    cpu->io.crc.bits_remaining = cpu->io.crc.data_width;
    cpu->io.crc.polynomial = (polynomial_value | 1u) & crc_width_mask(cpu->io.crc.polynomial_width);
    cpu->io.crc.little_endian =
        (dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_LITTLE_ENDIAN) != 0u;
    dspic33_device_internal_crc_refresh_status(cpu);
    if (cpu->io.crc.count == 0u &&
        (dspic33_device_internal_raw_word(cpu, CRC_CONTROL) & CRC_INTERRUPT_EMPTY) != 0u) {
        dspic33_raise_interrupt(cpu, CRC_IRQ);
    }
}

static void crc_shift_bits(Dspic33* cpu) {
    uint32_t crc_remainder = crc_shift_register(cpu) & crc_width_mask(cpu->io.crc.polynomial_width);
    uint8_t cycle_bit;

    for (cycle_bit = 0u; cycle_bit < CRC_BITS_PER_CYCLE && cpu->io.crc.bits_remaining != 0u;
         cycle_bit++) {
        const uint8_t input_bit =
            cpu->io.crc.little_endian
                ? (uint8_t)(cpu->io.crc.data_width - cpu->io.crc.bits_remaining)
                : (uint8_t)(cpu->io.crc.bits_remaining - 1u);
        bool feedback_bit = ((crc_remainder >> (cpu->io.crc.polynomial_width - 1u)) & 1u) != 0u;
        feedback_bit = feedback_bit != ((cpu->io.crc.shift_data >> input_bit) & 1u);
        crc_remainder = (crc_remainder << 1u) & crc_width_mask(cpu->io.crc.polynomial_width);
        if (feedback_bit) {
            crc_remainder ^= cpu->io.crc.polynomial;
        }
        cpu->io.crc.bits_remaining--;
    }
    crc_write_shift_register(cpu, crc_remainder);
}

void dspic33_device_internal_run_crc(Dspic33* cpu, uint16_t event_generation) {
    uint16_t crc_control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);

    if (event_generation != cpu->io.crc.generation || !cpu->io.crc.active ||
        (crc_control & (CRC_ENABLE | CRC_GO)) != (CRC_ENABLE | CRC_GO)) {
        return;
    }
    if (cpu->io.crc.pmd_disabled || cpu->power_state == DSPIC33_POWER_SLEEP ||
        (cpu->power_state == DSPIC33_POWER_IDLE && (crc_control & CRC_STOP_IDLE) != 0u)) {
        crc_schedule(cpu);
        return;
    }
    if (cpu->io.crc.bits_remaining == 0u) {
        if (cpu->io.crc.count == 0u) {
            cpu->io.crc.active = false;
            dspic33_device_internal_raw_write_word(cpu, CRC_CONTROL,
                                                   (uint16_t)(crc_control & ~CRC_GO));
            if ((crc_control & CRC_INTERRUPT_EMPTY) == 0u) {
                dspic33_raise_interrupt(cpu, CRC_IRQ);
            }
            return;
        }
        crc_load_shift_data(cpu);
    }
    crc_shift_bits(cpu);
    if (cpu->io.crc.bits_remaining == 0u && cpu->io.crc.count == 0u) {
        cpu->io.crc.active = false;
        crc_control = dspic33_device_internal_raw_word(cpu, CRC_CONTROL);
        dspic33_device_internal_raw_write_word(cpu, CRC_CONTROL, (uint16_t)(crc_control & ~CRC_GO));
        if ((crc_control & CRC_INTERRUPT_EMPTY) == 0u) {
            dspic33_raise_interrupt(cpu, CRC_IRQ);
        }
        return;
    }
    crc_schedule(cpu);
}

void dspic33_device_internal_run_crc_pmd(Dspic33* cpu, uint32_t event_value) {
    const uint16_t event_generation = (uint16_t)(event_value >> 1u);

    if (event_generation != cpu->io.crc.pmd_generation) {
        return;
    }
    cpu->io.crc.pmd_disabled = (event_value & 1u) != 0u;
    if (!cpu->io.crc.pmd_disabled) {
        dspic33_device_internal_crc_start_if_ready(cpu);
    }
}

void dspic33_device_internal_update_crc_pmd(Dspic33* cpu, uint16_t previous_control) {
    const bool pmd_disabled =
        (dspic33_device_internal_raw_word(cpu, CRC_PMD_ADDRESS) & CRC_PMD) != 0u;

    if (((previous_control & CRC_PMD) != 0u) == pmd_disabled) {
        return;
    }
    cpu->io.crc.pmd_generation++;
    if (!dspic33_schedule(cpu, DSPIC33_EVENT_CRC, CRC_EVENT_PMD_SOURCE,
                          ((uint32_t)cpu->io.crc.pmd_generation << 1u) | (pmd_disabled ? 1u : 0u),
                          dspic33_device_instruction_cycles(cpu, 1u))) {
        dspic33_device_internal_raw_write_word(cpu, CRC_PMD_ADDRESS, previous_control);
        cpu->io.crc.pmd_generation++;
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}
