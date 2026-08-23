#include "internal.h"

bool dspic33_internal_flash_read_erratum_sequence_completed(Dspic33* cpu, uint32_t opcode,
                                                            uint32_t instruction_pc,
                                                            bool psv_read) {
    const bool is_table_read = (opcode & 0xff0000u) == 0xba0000u;
    const bool is_flash_read = psv_read || is_table_read;
    const bool is_double_move = (opcode & 0xff0000u) == 0xbe0000u;
    const bool first_group_aligned = (instruction_pc & 2u) == 0u;
    const bool is_repeated_instruction = (opcode & 0xff8000u) == 0x090000u && cpu->rcount != 0u;
    uint32_t instruction_word_count;

    if (dspic33_internal_instruction_changes_program_flow(cpu, opcode, instruction_pc)) {
        dspic33_cancel_flash_read_sequence(cpu);
        return false;
    }
    if (cpu->flash_read_erratum_candidate && is_flash_read) {
        dspic33_cancel_flash_read_sequence(cpu);
        return true;
    }
    cpu->flash_read_erratum_candidate = false;
    if (is_double_move && psv_read && first_group_aligned) {
        cpu->flash_read_connecting_words = 0u;
        cpu->flash_read_erratum_armed = true;
        cpu->flash_read_connecting_ends_repeat = false;
        return false;
    }
    if (!cpu->flash_read_erratum_armed) {
        return false;
    }
    if (is_flash_read && first_group_aligned) {
        dspic33_cancel_flash_read_sequence(cpu);
        return false;
    }
    if (is_flash_read && cpu->flash_read_connecting_ends_repeat) {
        dspic33_cancel_flash_read_sequence(cpu);
        return false;
    }
    if (is_flash_read && cpu->flash_read_connecting_words >= 2u &&
        !cpu->flash_read_connecting_ends_repeat) {
        cpu->flash_read_erratum_candidate = true;
        return false;
    }
    instruction_word_count = dspic33_internal_instruction_length(opcode) / 2u;
    cpu->flash_read_connecting_words =
        cpu->flash_read_connecting_words > UINT16_MAX - instruction_word_count
            ? UINT16_MAX
            : (uint16_t)(cpu->flash_read_connecting_words + instruction_word_count);
    cpu->flash_read_connecting_ends_repeat = is_repeated_instruction;
    return false;
}

bool dspic33_internal_reserved_return_encoding(uint32_t opcode) {
    if ((opcode & 0xff0000u) == 0x050000u) {
        return (opcode & 0x008000u) != 0u;
    }
    return (opcode & 0xff0000u) == 0x060000u && opcode != 0x060000u && opcode != 0x064000u;
}

bool dspic33_internal_literal_control_extension_valid(uint32_t extension) {
    return (extension & 0xffff80u) == 0u;
}

bool dspic33_internal_literal_control_first_word_valid(uint32_t opcode) {
    return (opcode & 1u) == 0u;
}

bool dspic33_internal_byte_extension_encoding_valid(uint32_t opcode) {
    return (opcode & 0x007800u) == 0u && ((opcode >> 4u) & 0x07u) < 6u;
}

bool dspic33_internal_stack_encoding_valid(uint32_t opcode) {
    if ((opcode & 0xff0000u) == 0xf80000u || (opcode & 0xff0000u) == 0xf90000u) {
        return (opcode & 1u) == 0u;
    }
    if ((opcode & 0xff0000u) == 0xfa0000u) {
        return opcode == 0xfa8000u || (opcode & 0xffc001u) == 0xfa0000u;
    }
    return true;
}

bool dspic33_internal_bit_encoding_valid(uint32_t opcode) {
    const uint8_t instruction_kind = (uint8_t)((opcode >> 16u) & 0x07u);
    const bool is_file_access = (opcode & 0x080000u) != 0u;
    const uint8_t addressing_mode = (uint8_t)((opcode >> 4u) & 0x07u);

    if (is_file_access) {
        return instruction_kind != 5u &&
               (instruction_kind != 4u || (opcode & 0x001ffeu) != 0x000042u);
    }
    if (addressing_mode >= 6u) {
        return false;
    }
    if (instruction_kind <= 2u) {
        const bool is_byte_mode = (opcode & 0x000400u) != 0u;
        const uint8_t bit_index = (uint8_t)((opcode >> 12u) & 0x0fu);
        return (opcode & 0x000b80u) == 0u && (!is_byte_mode || bit_index < 8u);
    }
    if (instruction_kind <= 5u) {
        return (opcode & 0x000780u) == 0u;
    }
    return (opcode & 0x000f80u) == 0u;
}

bool dspic33_internal_table_encoding_valid(uint32_t opcode) {
    bool write = (opcode & 0x010000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);

    return write ? source_mode < 6u && destination_mode >= 1u && destination_mode < 6u
                 : source_mode >= 1u && source_mode < 6u && destination_mode < 6u;
}

bool dspic33_internal_system_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);

    if (family == 0xfcu) {
        return (opcode & 0x00c000u) == 0u;
    }
    if (family != 0xfeu) {
        return true;
    }
    if (opcode == 0xfe0000u || opcode == 0xfe2000u || (opcode & 0xfffffeu) == 0xfe4000u ||
        opcode == 0xfe6000u || opcode == 0xfe8000u || opcode == 0xfea000u) {
        return true;
    }
    if ((opcode & 0xfff000u) == 0xfec000u) {
        return ((opcode >> 10u) & 3u) != 3u;
    }
    return (opcode & 0xfff000u) == 0xfed000u && (opcode & 0x0003f0u) == 0u &&
           ((opcode >> 10u) & 3u) != 3u;
}

bool dspic33_internal_divide_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    uint8_t divisor = (uint8_t)(opcode & 0x0fu);

    if (family == 0xd9u) {
        return (opcode & 0x0087f0u) == 0u;
    }
    if (divisor < 2u) {
        return false;
    }
    if (family != 0xd8u || (opcode & 0x000030u) != 0u) {
        return false;
    }
    uint8_t high = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t low = (uint8_t)((opcode >> 7u) & 0x0fu);
    if ((opcode & 0x000040u) == 0u) {
        return high == 0u;
    }
    return (low & 1u) == 0u && high == low + 1u;
}

void dspic33_internal_push_program_counter(Dspic33* cpu, uint32_t address) {
    uint16_t low = (uint16_t)(address & 0xfffeu);
    low |= (uint16_t)((cpu->corcon >> 2u) & 1u);
    dspic33_internal_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    dspic33_internal_write_word(cpu, cpu->w[15], low);
    dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    dspic33_internal_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    dspic33_internal_write_word(cpu, cpu->w[15], (uint16_t)(address >> 16u));
    dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    cpu->corcon &= 0xfffbu;
    cpu->call_depth++;
}

uint32_t dspic33_internal_pop_program_counter(Dspic33* cpu) {
    uint32_t high;
    uint32_t low;
    dspic33_internal_record_source_address_register(cpu, 15u);
    dspic33_internal_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
    dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    high = dspic33_internal_read_word(cpu, cpu->w[15]) & 0x007fu;
    dspic33_internal_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
    dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    low = dspic33_internal_read_word(cpu, cpu->w[15]);
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0004u) | ((low & 1u) << 2u));
    cpu->call_depth--;
    return (high << 16u) | (low & 0xfffeu);
}

bool dspic33_internal_single_shift_encoding_valid(uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    return (family != 0u || (opcode & 0x008000u) == 0u) && ((opcode >> 4u) & 0x07u) < 6u &&
           ((opcode >> 11u) & 0x07u) < 6u;
}

bool dspic33_internal_file_shift_encoding_valid(uint32_t opcode) {
    return ((opcode >> 16u) & 0x03u) != 0u || (opcode & 0x008000u) == 0u;
}

bool dspic33_internal_multiple_shift_encoding_valid(uint32_t opcode, bool left) {
    return (opcode & 0x0030u) == 0u && (!left || (opcode & 0x008000u) == 0u);
}

bool dspic33_internal_find_first_encoding_valid(uint32_t opcode, bool sign_change) {
    uint32_t fixed_mask = sign_change ? 0x00f800u : 0x007800u;
    return (opcode & fixed_mask) == 0u && ((opcode >> 4u) & 0x07u) < 6u;
}

bool dspic33_internal_accumulator_shift_encoding_valid(uint32_t opcode) {
    return (opcode & 0x0080u) == 0u && ((opcode & 0x0040u) != 0u || (opcode & 0x0030u) == 0u);
}

bool dspic33_internal_execute_shift(Dspic33* cpu, uint32_t opcode, bool left) {
    uint8_t source = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool arithmetic = !left && (opcode & 0x008000u) != 0u;
    uint16_t amount;
    uint16_t value;

    amount = (opcode & 0x0040u) != 0u ? (uint16_t)(opcode & 0x0fu)
                                      : (uint16_t)(cpu->w[opcode & 0x0fu] & 0x001fu);
    if (amount >= 16u) {
        value = arithmetic && (cpu->w[source] & 0x8000u) != 0u ? 0xffffu : 0u;
    } else if (left) {
        value = (uint16_t)(cpu->w[source] << amount);
    } else if (arithmetic) {
        value = (uint16_t)((int16_t)cpu->w[source] >> amount);
    } else {
        value = (uint16_t)(cpu->w[source] >> amount);
    }
    dspic33_internal_write_working_register(cpu, destination, value);
    dspic33_internal_update_logic_flags(cpu, value, false);
    return true;
}

bool dspic33_internal_execute_find_first_sign_change(Dspic33* cpu, uint32_t opcode) {
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t source = dspic33_internal_read_operand_word(cpu, source_mode, source_register, 0u);
    bool sign = (source & 0x8000u) != 0u;
    uint8_t shifts = 0u;
    if (cpu->illegal_reset) {
        return true;
    }

    source <<= 1u;
    while (shifts < 15u && ((source & 0x8000u) != 0u) == sign) {
        source <<= 1u;
        shifts++;
    }
    dspic33_internal_write_working_register(cpu, destination, (uint16_t)(-(int16_t)shifts));
    cpu->sr = (uint16_t)((cpu->sr & ~1u) | (shifts == 15u ? 1u : 0u));
    return true;
}

static uint16_t shift_single_bit(const Dspic33* cpu, uint16_t source, uint8_t family,
                                 bool alternate, bool byte_mode, uint16_t* next_carry,
                                 bool* carry_affected) {
    uint16_t width_mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign_mask = byte_mode ? 0x0080u : 0x8000u;
    uint16_t carry = (cpu->sr & 1u) != 0u ? 1u : 0u;
    uint16_t value;
    *carry_affected = family < 2u || alternate;
    if (family == 0u) {
        *next_carry = (source & sign_mask) != 0u;
        value = (uint16_t)((source << 1u) & width_mask);
    } else if (family == 1u) {
        *next_carry = source & 1u;
        value = (uint16_t)(source >> 1u);
        if (alternate && (source & sign_mask) != 0u) {
            value |= sign_mask;
        }
    } else if (family == 2u) {
        *next_carry = (source & sign_mask) != 0u;
        value = (uint16_t)((source << 1u) & width_mask);
        value |= alternate ? carry : *next_carry;
    } else {
        *next_carry = source & 1u;
        value = (uint16_t)(source >> 1u);
        if (alternate ? carry != 0u : *next_carry != 0u) {
            value |= sign_mask;
        }
    }
    return value;
}

bool dspic33_internal_execute_single_shift(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    cpu->instruction_rmw = true;
    uint16_t source =
        byte_mode ? dspic33_internal_read_operand_byte(cpu, source_mode, source_register, 0u)
                  : dspic33_internal_read_operand_word(cpu, source_mode, source_register, 0u);
    uint16_t next_carry;
    bool carry_affected;
    if (cpu->illegal_reset) {
        return true;
    }
    uint16_t value =
        shift_single_bit(cpu, source, family, alternate, byte_mode, &next_carry, &carry_affected);
    dspic33_internal_update_logic_flags(cpu, value, byte_mode);
    if (carry_affected) {
        cpu->sr = (uint16_t)((cpu->sr & ~1u) | next_carry);
    }
    if (!dspic33_internal_validate_destination_after_source_execution(
            cpu, destination_mode, destination_register, byte_mode ? 1u : 2u)) {
        return true;
    }
    if (byte_mode) {
        return dspic33_internal_write_operand_byte(cpu, destination_mode, destination_register, 0u,
                                                   (uint8_t)value);
    }
    return dspic33_internal_write_operand_word(cpu, destination_mode, destination_register, 0u,
                                               value);
}

bool dspic33_internal_execute_file_shift(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool file_destination = (opcode & 0x002000u) != 0u;
    uint16_t address = (uint16_t)(opcode & 0x1fffu);
    cpu->instruction_rmw = true;
    uint16_t source = byte_mode ? dspic33_internal_read_data_byte(cpu, address)
                                : dspic33_internal_read_file_word(cpu, address);
    uint16_t next_carry;
    bool carry_affected;
    uint16_t value =
        shift_single_bit(cpu, source, family, alternate, byte_mode, &next_carry, &carry_affected);
    dspic33_internal_update_logic_flags(cpu, value, byte_mode);
    if (carry_affected) {
        cpu->sr = (uint16_t)((cpu->sr & ~1u) | next_carry);
    }
    if (file_destination) {
        if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)value);
        } else {
            dspic33_write_word(cpu, address, value);
        }
    } else if (byte_mode) {
        dspic33_internal_write_working_register_byte(cpu, 0u, false, (uint8_t)value);
    } else {
        dspic33_internal_write_working_register(cpu, 0u, value);
    }
    return true;
}

bool dspic33_internal_multiply_encoding_valid(uint32_t opcode) {
    bool source_signed = (opcode & 0x008000u) != 0u;
    bool literal_mode = (opcode & 0x000060u) == 0x000060u;
    return !source_signed || !literal_mode;
}

bool dspic33_internal_execute_multiply(Dspic33* cpu, uint32_t opcode) {
    bool base_signed = (opcode & 0x010000u) != 0u;
    bool source_signed = (opcode & 0x008000u) != 0u;
    uint8_t base_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint16_t source;
    int64_t left;
    int64_t right;
    int64_t product;
    if ((opcode & 0x000060u) == 0x000060u) {
        source = (uint16_t)(opcode & 0x001fu);
    } else {
        source = dspic33_internal_read_operand_word(cpu, source_mode, source_register, 0u);
        if (cpu->illegal_reset) {
            return true;
        }
    }
    left = base_signed ? (int16_t)cpu->w[base_register] : cpu->w[base_register];
    right = source_signed ? (int16_t)source : source;
    product = left * right;
    if (destination >= 14u) {
        uint8_t accumulator = (uint8_t)(destination & 1u);
        if ((cpu->corcon & 1u) == 0u) {
            product *= 2;
        }
        cpu->accumulator[accumulator] = dspic33_internal_accumulator_value((uint64_t)product);
        return true;
    }
    bool word_result = (destination & 1u) != 0u;
    destination &= 0x0eu;
    dspic33_internal_write_working_register(cpu, destination, (uint16_t)product);
    if (!word_result) {
        dspic33_internal_write_working_register(cpu, (uint8_t)(destination + 1u),
                                                (uint16_t)((uint32_t)product >> 16u));
    }
    return true;
}

bool dspic33_internal_execute_accumulator_arithmetic(Dspic33* cpu, uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint32_t operation = opcode & 0xff7fffu;
    int64_t result;

    if (operation == 0xcb0000u) {
        result = cpu->accumulator[0] + cpu->accumulator[1];
    } else if (operation == 0xcb1000u) {
        result = -cpu->accumulator[accumulator];
    } else if (operation == 0xcb3000u) {
        result = cpu->accumulator[accumulator] - cpu->accumulator[accumulator ^ 1u];
    } else {
        return false;
    }

    dspic33_internal_apply_accumulator_result(cpu, accumulator, result);
    return true;
}

static int64_t shift_accumulator_value(int64_t value, int8_t amount) {
    if (amount < 0) {
        return value * ((int64_t)1 << -amount);
    }
    if (amount == 0 || value >= 0) {
        return value >> amount;
    }
    return -((-value + ((int64_t)1 << amount) - 1) >> amount);
}

bool dspic33_internal_execute_accumulator_shift(Dspic33* cpu, uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t encoded_amount = (uint8_t)(opcode & 0x003fu);
    int16_t amount;
    if ((opcode & 0x0040u) == 0u) {
        encoded_amount = (uint8_t)(cpu->w[opcode & 0x0fu] & 0x003fu);
    }
    amount = (int16_t)(encoded_amount >= 32u ? encoded_amount - 64u : encoded_amount);
    if (amount < -16 || amount > 16) {
        dspic33_device_latch_math_error(cpu, 0x0080u);
        return true;
    }
    dspic33_internal_apply_accumulator_result(
        cpu, accumulator, shift_accumulator_value(cpu->accumulator[accumulator], (int8_t)amount));
    return true;
}

bool dspic33_internal_execute_accumulator_word(Dspic33* cpu, uint32_t opcode, bool add) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t offset_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t encoded_shift = (uint8_t)((opcode >> 7u) & 0x0fu);
    int8_t shift = (int8_t)(encoded_shift >= 8u ? encoded_shift - 16u : encoded_shift);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    int64_t value = (int64_t)(int16_t)dspic33_internal_read_operand_word(
                        cpu, source_mode, source_register, offset_register)
                    << 16u;
    if (cpu->illegal_reset) {
        return true;
    }
    value = shift_accumulator_value(value, shift);
    if (add) {
        value += cpu->accumulator[accumulator];
    }
    dspic33_internal_apply_accumulator_result(cpu, accumulator, value);
    return true;
}

static uint16_t accumulator_store_value_with_rounding(const Dspic33* cpu, int64_t value,
                                                      bool rounded, bool conventional) {
    uint64_t bits = (uint64_t)value & DSPIC33_ACCUMULATOR_MASK;
    uint16_t high = (uint16_t)(bits >> 16u);
    uint16_t low = (uint16_t)bits;
    if (rounded && (conventional || low > 0x8000u || (low == 0x8000u && (high & 1u) != 0u))) {
        value += 0x8000;
    }
    if ((cpu->corcon & 0x0020u) != 0u) {
        if (value > INT32_MAX) {
            return 0x7fffu;
        }
        if (value < INT32_MIN) {
            return 0x8000u;
        }
    }
    return (uint16_t)((uint64_t)value >> 16u);
}

static uint16_t accumulator_store_value(const Dspic33* cpu, int64_t value, bool rounded) {
    return accumulator_store_value_with_rounding(cpu, value, rounded,
                                                 (cpu->corcon & 0x0002u) != 0u);
}

bool dspic33_internal_execute_accumulator_store(Dspic33* cpu, uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t offset_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t encoded_shift = (uint8_t)((opcode >> 7u) & 0x0fu);
    int8_t shift = (int8_t)(encoded_shift >= 8u ? encoded_shift - 16u : encoded_shift);
    uint8_t destination_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_register = (uint8_t)(opcode & 0x0fu);
    int64_t value = shift_accumulator_value(cpu->accumulator[accumulator], shift);
    return dspic33_internal_write_operand_word(
        cpu, destination_mode, destination_register, offset_register,
        accumulator_store_value(cpu, value, (opcode & 0x010000u) != 0u));
}

static bool dsp_multiply_registers(uint32_t opcode, uint8_t* left, uint8_t* right) {
    static const uint8_t pairs[8][2] = {
        {4u, 5u}, {4u, 6u}, {4u, 7u}, {0u, 0u}, {5u, 6u}, {5u, 7u}, {6u, 7u}, {0u, 0u},
    };
    if ((opcode & 0xfc0000u) == 0xf00000u) {
        uint8_t write_back = (uint8_t)(opcode & 3u);
        *left = (uint8_t)(4u + ((opcode >> 16u) & 3u));
        *right = *left;
        return (opcode & 0x004000u) == 0u && write_back <= 1u;
    }
    if ((opcode & 0xf80000u) != 0xc00000u) {
        return false;
    }
    uint8_t pair = (uint8_t)((opcode >> 16u) & 7u);
    if (pair == 3u || pair == 7u) {
        return false;
    }
    *left = pairs[pair][0];
    *right = pairs[pair][1];
    return true;
}

bool dspic33_internal_dsp_encoding_valid(uint32_t opcode) {
    if ((opcode & 0xf80000u) == 0xc00000u) {
        uint8_t pair = (uint8_t)((opcode >> 16u) & 7u);
        return (pair != 3u && pair != 7u) || ((opcode & 0x004000u) == 0u && (opcode & 3u) != 3u);
    }
    if ((opcode & 0xfc0000u) == 0xf00000u) {
        if ((opcode & 0x004000u) == 0u) {
            return (opcode & 2u) == 0u;
        }
        return (opcode & 0x000c02u) == 2u && ((opcode >> 6u) & 0x0fu) != 4u &&
               ((opcode >> 2u) & 0x0fu) != 4u;
    }
    return true;
}

static int64_t dsp_multiply_operand(const Dspic33* cpu, uint8_t register_index, uint8_t sign_mode) {
    bool unsigned_operand = sign_mode == 1u || (sign_mode == 2u && (register_index & 1u) == 0u);
    return unsigned_operand ? cpu->w[register_index] : (int16_t)cpu->w[register_index];
}

static bool validate_dsp_prefetch_alignment(Dspic33* cpu, uint8_t operation, bool y_space) {
    uint8_t base_register;
    uint16_t address;
    if (operation == 4u) {
        return true;
    }
    base_register = (uint8_t)((y_space ? 10u : 8u) + (operation >= 8u ? 1u : 0u));
    if (!dspic33_internal_address_register_initialized(cpu, base_register)) {
        return false;
    }
    address = cpu->w[base_register];
    if (operation == 12u) {
        int32_t delta = (int16_t)cpu->w[12];
        address = dspic33_internal_modulo_address(cpu, base_register, (int32_t)address + delta,
                                                  delta, y_space);
    }
    return dspic33_internal_check_data_alignment(cpu, address);
}

static bool dsp_x_address_valid(const Dspic33* cpu, uint16_t address, uint16_t page) {
    if (address < 0x8000u) {
        return true;
    }
    if (page >= 0x0200u) {
        uint32_t program_address =
            dspic33_internal_mapped_data_address(address, page, false) & PSV_ADDRESS_MASK;
        return !dspic33_internal_program_target_requires_address_error(cpu, program_address);
    }
    return page == 1u && address <= 0x8ffeu;
}

static bool dsp_y_address_valid(uint16_t address) {
    return address >= 0x9000u && address <= 0xdffeu;
}

static bool resolve_dsp_x_prefetch(const Dspic33* cpu, uint8_t operation,
                                   DspPrefetchOutcome* outcome) {
    uint8_t base_register = (uint8_t)(8u + (operation >= 8u ? 1u : 0u));
    int8_t update = dspic33_internal_dsp_prefetch_updates[operation];
    uint8_t mode = update == 0 ? 1u : update > 0 ? 3u : 2u;
    uint8_t width = update < 0 ? (uint8_t)-update : (uint8_t)update;
    if (operation == 12u) {
        mode = 6u;
        width = 2u;
    } else if (width == 0u) {
        width = 2u;
    }
    OperandResolution resolution;
    if (!dspic33_internal_resolve_operand_address(cpu, cpu->w, mode, base_register, 12u, width,
                                                  false, &resolution)) {
        return false;
    }
    outcome->address = resolution.address;
    outcome->updated_register = resolution.updated_register;
    outcome->updated_data_page = resolution.updated_data_page;
    outcome->base_register = base_register;
    outcome->present = true;
    outcome->access_valid =
        dsp_x_address_valid(cpu, resolution.access_register, resolution.access_data_page);
    outcome->update_valid =
        !resolution.updates_register ||
        dsp_x_address_valid(cpu, resolution.updated_register,
                            resolution.updates_data_page ? resolution.updated_data_page
                                                         : cpu->dsrpag);
    outcome->updates_register = resolution.updates_register;
    outcome->updates_data_page = resolution.updates_data_page;
    return true;
}

static bool resolve_dsp_y_prefetch(const Dspic33* cpu, uint8_t operation,
                                   DspPrefetchOutcome* outcome) {
    uint8_t base_register = (uint8_t)(10u + (operation >= 8u ? 1u : 0u));
    int32_t delta =
        operation == 12u ? (int16_t)cpu->w[12] : dspic33_internal_dsp_prefetch_updates[operation];
    uint16_t address = cpu->w[base_register];
    if (operation == 12u) {
        address = dspic33_internal_modulo_address(cpu, base_register, (int32_t)address + delta,
                                                  delta, true);
    }
    outcome->address = address;
    outcome->updated_register = dspic33_internal_modulo_address(
        cpu, base_register, (int32_t)cpu->w[base_register] + delta, delta, true);
    outcome->base_register = base_register;
    outcome->present = true;
    outcome->access_valid = dsp_y_address_valid(address);
    outcome->updates_register = operation != 12u;
    outcome->update_valid =
        !outcome->updates_register || dsp_y_address_valid(outcome->updated_register);
    return true;
}

static bool resolve_dsp_prefetch(const Dspic33* cpu, uint8_t operation, bool y_space,
                                 DspPrefetchOutcome* outcome) {
    memset(outcome, 0, sizeof(*outcome));
    if (operation == 4u) {
        return true;
    }
    return y_space ? resolve_dsp_y_prefetch(cpu, operation, outcome)
                   : resolve_dsp_x_prefetch(cpu, operation, outcome);
}

static bool validate_dsp_alignments(Dspic33* cpu, uint8_t x_operation, uint8_t y_operation,
                                    bool memory_write_back) {
    return validate_dsp_prefetch_alignment(cpu, x_operation, false) &&
           validate_dsp_prefetch_alignment(cpu, y_operation, true) &&
           (!memory_write_back || (dspic33_internal_address_register_initialized(cpu, 13u) &&
                                   dspic33_internal_check_data_alignment(cpu, cpu->w[13])));
}

static void commit_dsp_prefetch(Dspic33* cpu, const DspPrefetchOutcome* outcome) {
    if (outcome->updates_data_page) {
        cpu->dsrpag = outcome->updated_data_page & 0x03ffu;
    }
    if (outcome->updates_register) {
        dspic33_internal_write_working_register(cpu, outcome->base_register,
                                                outcome->updated_register);
    }
}

static void raise_dsp_prefetch_error(Dspic33* cpu) {
    if (!cpu->address_error) {
        cpu->address_error = true;
        cpu->address_error_return = cpu->pc;
    }
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = true;
    cpu->address_error_accumulator_state_completed = true;
}

static bool execute_dsp_prefetches(Dspic33* cpu, uint8_t x_operation, uint8_t y_operation,
                                   uint16_t* x_value, uint16_t* y_value, bool* x_present,
                                   bool* y_present) {
    DspPrefetchOutcome x;
    DspPrefetchOutcome y;
    if (!resolve_dsp_prefetch(cpu, x_operation, false, &x) ||
        !resolve_dsp_prefetch(cpu, y_operation, true, &y)) {
        return false;
    }
    if (x.present) {
        dspic33_internal_record_source_address_register(cpu, x.base_register);
        if (x_operation == 12u) {
            dspic33_internal_record_source_address_register(cpu, 12u);
        }
    }
    if (y.present) {
        dspic33_internal_record_source_address_register(cpu, y.base_register);
        if (y_operation == 12u) {
            dspic33_internal_record_source_address_register(cpu, 12u);
        }
    }
    if (x.present && (x.address & PSV_ADDRESS) != 0u) {
        cpu->psv_read = true;
        if (cpu->repeat_active != 0u &&
            (dspic33_internal_dsp_prefetch_updates[x_operation] == 2 ||
             dspic33_internal_dsp_prefetch_updates[x_operation] == -2)) {
            cpu->psv_repeat_optimized = true;
        }
    }
    *x_value = x.present && x.access_valid ? dspic33_internal_read_data_word(cpu, x.address) : 0u;
    *y_value = y.present && y.access_valid ? dspic33_internal_read_data_word(cpu, y.address) : 0u;
    commit_dsp_prefetch(cpu, &x);
    commit_dsp_prefetch(cpu, &y);
    *x_present = x.present;
    *y_present = y.present;
    if ((x.present && (!x.access_valid || !x.update_valid)) ||
        (y.present && (!y.access_valid || !y.update_valid))) {
        raise_dsp_prefetch_error(cpu);
    }
    return true;
}

static void execute_dsp_write_back(Dspic33* cpu, uint8_t accumulator, uint8_t mode) {
    uint16_t value =
        accumulator_store_value_with_rounding(cpu, cpu->accumulator[accumulator ^ 1u], true, false);
    if (mode == 0u) {
        dspic33_internal_write_working_register(cpu, 13u, value);
    } else if (mode == 1u) {
        dspic33_write_word(cpu, cpu->w[13], value);
        dspic33_internal_write_working_register(cpu, 13u, (uint16_t)(cpu->w[13] + 2u));
    }
}

bool dspic33_internal_execute_dsp_clear_or_move(Dspic33* cpu, uint32_t opcode) {
    bool clear = (opcode & 0xff0000u) == 0xc30000u;
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t write_back = (uint8_t)(opcode & 3u);
    uint8_t x_operation = (uint8_t)((opcode >> 6u) & 0x0fu);
    uint8_t y_operation = (uint8_t)((opcode >> 2u) & 0x0fu);
    uint16_t x_value;
    uint16_t y_value;
    bool x_present;
    bool y_present;
    if (write_back == 3u) {
        return false;
    }
    if (!validate_dsp_alignments(cpu, x_operation, y_operation, write_back == 1u)) {
        return true;
    }
    if (clear) {
        cpu->accumulator[accumulator] = 0;
        dspic33_internal_clear_accumulator_status(cpu, accumulator);
    }
    if (!execute_dsp_prefetches(cpu, x_operation, y_operation, &x_value, &y_value, &x_present,
                                &y_present)) {
        return false;
    }
    if (x_present) {
        dspic33_internal_write_working_register(cpu, (uint8_t)(4u + ((opcode >> 12u) & 3u)),
                                                x_value);
    }
    if (y_present) {
        dspic33_internal_write_working_register(cpu, (uint8_t)(4u + ((opcode >> 10u) & 3u)),
                                                y_value);
    }
    if (write_back < 2u) {
        execute_dsp_write_back(cpu, accumulator, write_back);
    }
    return true;
}

bool dspic33_internal_execute_euclidean_distance(Dspic33* cpu, uint32_t opcode) {
    uint8_t operation = (uint8_t)(opcode & 3u);
    uint8_t x_operation = (uint8_t)((opcode >> 6u) & 0x0fu);
    uint8_t y_operation = (uint8_t)((opcode >> 2u) & 0x0fu);
    uint8_t source_register = (uint8_t)(4u + ((opcode >> 16u) & 3u));
    uint8_t destination = (uint8_t)(4u + ((opcode >> 12u) & 3u));
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t sign_mode = (uint8_t)((cpu->corcon >> 12u) & 3u);
    uint16_t x_value;
    uint16_t y_value;
    int64_t operand;
    int64_t product;
    if (operation < 2u || x_operation == 4u || y_operation == 4u || sign_mode == 3u) {
        return false;
    }
    if (!validate_dsp_alignments(cpu, x_operation, y_operation, false)) {
        return true;
    }
    operand = dsp_multiply_operand(cpu, source_register, sign_mode);
    product = operand * operand;
    if ((cpu->corcon & 1u) == 0u) {
        product *= 2;
    }
    bool x_present;
    bool y_present;
    if (!execute_dsp_prefetches(cpu, x_operation, y_operation, &x_value, &y_value, &x_present,
                                &y_present) ||
        !x_present || !y_present) {
        return false;
    }
    dspic33_internal_apply_accumulator_result(
        cpu, accumulator, operation == 2u ? cpu->accumulator[accumulator] + product : product);
    dspic33_internal_write_working_register(cpu, destination, (uint16_t)(x_value - y_value));
    return true;
}

bool dspic33_internal_execute_dsp_multiply(Dspic33* cpu, uint32_t opcode) {
    uint8_t left_register;
    uint8_t right_register;
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t sign_mode = (uint8_t)((cpu->corcon >> 12u) & 3u);
    int64_t left;
    int64_t right;
    int64_t product;
    int64_t result;
    bool square = (opcode & 0xfc0000u) == 0xf00000u;
    uint8_t write_back = (uint8_t)(opcode & 3u);
    uint8_t x_operation = (uint8_t)((opcode >> 6u) & 0x0fu);
    uint8_t y_operation = (uint8_t)((opcode >> 2u) & 0x0fu);
    uint16_t x_value;
    uint16_t y_value;
    bool x_present;
    bool y_present;
    bool replace = square ? write_back == 1u : write_back == 3u;
    bool subtract = (opcode & 0x004000u) != 0u;
    if (!dsp_multiply_registers(opcode, &left_register, &right_register) || sign_mode == 3u) {
        return false;
    }
    if (!validate_dsp_alignments(cpu, x_operation, y_operation,
                                 !square && !replace && write_back == 1u)) {
        return true;
    }
    left = dsp_multiply_operand(cpu, left_register, sign_mode);
    right = dsp_multiply_operand(cpu, right_register, sign_mode);
    product = left * right;
    if ((cpu->corcon & 1u) == 0u) {
        product *= 2;
    }
    if (replace) {
        result = subtract ? -product : product;
    } else {
        result = cpu->accumulator[accumulator] + (subtract ? -product : product);
    }
    dspic33_internal_apply_accumulator_result(cpu, accumulator, result);
    if (!execute_dsp_prefetches(cpu, x_operation, y_operation, &x_value, &y_value, &x_present,
                                &y_present)) {
        return false;
    }
    if (x_present) {
        dspic33_internal_write_working_register(cpu, (uint8_t)(4u + ((opcode >> 12u) & 3u)),
                                                x_value);
    }
    if (y_present) {
        dspic33_internal_write_working_register(cpu, (uint8_t)(4u + ((opcode >> 10u) & 3u)),
                                                y_value);
    }
    if (!square && !replace && write_back < 2u) {
        execute_dsp_write_back(cpu, accumulator, write_back);
    }
    return true;
}

bool dspic33_internal_execute_find_first(Dspic33* cpu, uint32_t opcode) {
    bool left = (opcode & 0x008000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t source = dspic33_internal_read_operand_word(cpu, source_mode, source_register, 0u);
    uint16_t result = 0u;
    uint8_t bit;
    if (cpu->illegal_reset) {
        return true;
    }
    if (left) {
        for (bit = 0u; bit < 16u; bit++) {
            if ((source & (uint16_t)(0x8000u >> bit)) != 0u) {
                result = (uint16_t)(bit + 1u);
                break;
            }
        }
    } else {
        for (bit = 0u; bit < 16u; bit++) {
            if ((source & (uint16_t)(1u << bit)) != 0u) {
                result = (uint16_t)(bit + 1u);
                break;
            }
        }
    }
    dspic33_internal_write_working_register(cpu, destination, result);
    cpu->sr = (uint16_t)((cpu->sr & ~1u) | (result == 0u ? 1u : 0u));
    return true;
}

bool dspic33_internal_execute_decimal_adjust(Dspic33* cpu, uint32_t opcode) {
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    uint16_t original = cpu->w[destination];
    uint16_t adjusted = (uint8_t)original;
    bool carry = (cpu->sr & 0x0001u) != 0u;
    if ((adjusted & 0x000fu) > 9u || (cpu->sr & 0x0100u) != 0u) {
        adjusted += 6u;
    }
    if (adjusted > 0x009fu || carry) {
        adjusted += 0x0060u;
    }
    dspic33_internal_write_working_register_byte(cpu, destination, false, (uint8_t)adjusted);
    cpu->sr = (uint16_t)((cpu->sr & ~0x0001u) | (carry || adjusted > 0x00ffu ? 1u : 0u));
    return true;
}

void dspic33_internal_update_divide_flags(Dspic33* cpu, int64_t remainder, bool overflow) {
    cpu->sr &= 0xfff0u;
    if (remainder == 0) {
        cpu->sr |= 0x0002u;
    }
    if (remainder < 0) {
        cpu->sr |= 0x0008u;
    }
    if (overflow) {
        cpu->sr |= 0x0004u;
    }
}

void dspic33_internal_enter_trap(Dspic33* cpu, uint16_t trap, uint32_t vector, uint8_t priority,
                                 uint16_t status, uint32_t return_pc, bool auxiliary_vector) {
    uint16_t stacked_high;
    uint8_t current_priority =
        (uint8_t)(((cpu->corcon & 0x0008u) != 0u ? 8u : 0u) | ((cpu->sr >> 5u) & 0x07u));
    uint32_t target =
        dspic33_read_program_word(cpu, auxiliary_vector ? 0x007ffffau : vector) & 0x007ffffeu;
    uint32_t origin = auxiliary_vector ? DSPIC33_AUXILIARY_PROGRAM_BASE : 0u;
    dspic33_cancel_flash_read_sequence(cpu);
    if (priority >= 13u && priority < current_priority) {
        dspic33_internal_perform_warm_reset(cpu, 0x8000u, DSPIC33_RESET_HARDWARE);
        return;
    }
    dspic33_write_word(cpu, 0x08c0u, (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | status));
    if (trap != 1u && dspic33_internal_program_target_requires_address_error(cpu, target)) {
        dspic33_internal_enter_address_trap(cpu, return_pc);
        return;
    }
    if (!dspic33_codeguard_admit_program_flow(cpu, origin, target)) {
        return;
    }
    cpu->previous_working_register_writes = 0u;
    cpu->sequential_program_hole_pc = 0u;
    dspic33_internal_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    dspic33_internal_write_word(cpu, cpu->w[15],
                                (uint16_t)((return_pc & 0xfffeu) | ((cpu->corcon >> 2u) & 1u)));
    dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    stacked_high =
        (uint16_t)(((cpu->sr & 0x00ffu) << 8u) | ((cpu->corcon & 0x0008u) != 0u ? 0x0080u : 0u) |
                   ((return_pc >> 16u) & 0x007fu));
    dspic33_internal_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    dspic33_internal_write_word(cpu, cpu->w[15], stacked_high);
    dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    cpu->corcon &= 0xfffbu;
    cpu->corcon =
        priority > 7u ? (uint16_t)(cpu->corcon | 0x0008u) : (uint16_t)(cpu->corcon & ~0x0008u);
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | ((priority & 7u) << 5u));
    cpu->pc = target;
    cpu->last_trap = trap;
    cpu->last_trap_return = return_pc;
    cpu->trap_count++;
    cpu->interrupt_depth++;
    dspic33_device_latch_interrupt(cpu, (uint8_t)trap, priority);
    cpu->repeat_active = 0u;
    cpu->repeat_pc = 0u;
    cpu->sr &= 0xffefu;
    if (cpu->stop_on_trap) {
        cpu->stop_reason = DSPIC33_TRAPPED;
    }
}

void dspic33_internal_enter_address_trap(Dspic33* cpu, uint32_t return_pc) {
    uint16_t sfa = (uint16_t)(cpu->corcon & 0x0004u);
    cpu->corcon &= 0xfffbu;
    dspic33_internal_enter_trap(cpu, 1u, 0x000006u, 14u, 0x0008u, return_pc, false);
    if (!cpu->reset_occurred) {
        cpu->corcon |= sfa;
    }
}

void dspic33_raise_program_vector_error(Dspic33* cpu, uint32_t return_pc) {
    dspic33_internal_enter_address_trap(cpu, return_pc);
}

void dspic33_internal_schedule_soft_trap(Dspic33* cpu, uint16_t trap, uint32_t vector,
                                         uint8_t priority, uint8_t delay) {
    Dspic33PendingSoftTrap* available = NULL;
    size_t index;
    for (index = 0u; index < 4u; index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->trap == trap) {
            if (pending->delay > delay) {
                pending->delay = delay;
                pending->auxiliary_program = dspic33_internal_auxiliary_program_address(
                    cpu->instruction_active ? cpu->current_instruction_pc : cpu->pc);
            }
            return;
        }
        if (!pending->active && available == NULL) {
            available = pending;
        }
    }
    if (available == NULL) {
        return;
    }
    available->trap = trap;
    available->vector = vector;
    available->priority = priority;
    available->delay = delay;
    available->active = true;
    available->auxiliary_program = dspic33_internal_auxiliary_program_address(
        cpu->instruction_active ? cpu->current_instruction_pc : cpu->pc);
}

void dspic33_set_math_error_source(Dspic33* cpu, bool active) {
    size_t index;
    if (active) {
        dspic33_internal_schedule_soft_trap(cpu, 4u, 0x00000cu, 11u, 2u);
        return;
    }
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active && cpu->pending_soft_traps[index].trap == 4u) {
            cpu->pending_soft_traps[index].active = false;
        }
    }
}

void dspic33_internal_set_trap_source(Dspic33* cpu, uint16_t trap, uint32_t vector,
                                      uint8_t priority, uint8_t delay, bool active) {
    size_t index;
    if (active) {
        dspic33_internal_schedule_soft_trap(cpu, trap, vector, priority, delay);
        return;
    }
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active && cpu->pending_soft_traps[index].trap == trap) {
            cpu->pending_soft_traps[index].active = false;
        }
    }
}
