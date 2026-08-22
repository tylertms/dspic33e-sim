
#include "internal.h"

uint32_t dspic33_internal_direct_move_address(const Dspic33* cpu, uint16_t address, bool write) {
    uint16_t page;
    if (address < 0x8000u) {
        return address;
    }
    page = write ? cpu->dswpag : cpu->dsrpag;
    if (!write && page >= 0x0200u) {
        return PSV_ADDRESS | ((page & 0x0100u) != 0u ? PSV_HIGH_BYTE : 0u) |
               ((uint32_t)(page & 0x00ffu) << 15u) | (address & 0x7fffu);
    }
    return ((uint32_t)page << 15u) | (address & 0x7fffu);
}

bool dspic33_internal_execute_move_offset(Dspic33* cpu, uint32_t opcode) {
    uint16_t encoded = (uint16_t)((((opcode >> 15u) & 0x0fu) << 6u) |
                                  (((opcode >> 11u) & 0x07u) << 3u) | ((opcode >> 4u) & 0x07u));
    int16_t offset = (int16_t)(encoded | ((encoded & 0x0200u) != 0u ? 0xfc00u : 0u));
    uint8_t source = (uint8_t)(opcode & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool store = (opcode & 0x080000u) != 0u;
    uint32_t address;
    if (!byte_mode) {
        offset = (int16_t)(offset * 2);
    }
    if (store) {
        if (!dspic33_internal_indirect_literal_address(cpu, destination, offset, true, &address)) {
            return true;
        }
        if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)cpu->w[source]);
        } else {
            dspic33_write_word(cpu, address, cpu->w[source]);
        }
    } else {
        if (!dspic33_internal_indirect_literal_address(cpu, source, offset, false, &address)) {
            return true;
        }
        if (byte_mode) {
            dspic33_internal_write_working_register_byte(
                cpu, destination, false, dspic33_internal_read_data_byte(cpu, address));
        } else {
            dspic33_internal_write_working_register(cpu, destination,
                                                    dspic33_internal_read_data_word(cpu, address));
        }
    }
    return true;
}

bool dspic33_internal_execute_move_double(Dspic33* cpu, uint32_t opcode) {
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool source_to_direct = (opcode & 0xfff880u) == 0xbe0000u && source_mode <= 5u &&
                            (source_mode != 0u || (source_register & 1u) == 0u);
    bool direct_to_indirect =
        (opcode & 0xffc071u) == 0xbe8000u && destination_mode >= 1u && destination_mode <= 5u;
    uint16_t low;
    uint16_t high;
    uint32_t address;
    uint32_t high_address;
    OperandResolution resolution;
    uint16_t registers[16];
    if (!source_to_direct && !direct_to_indirect) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    memcpy(registers, cpu->w, sizeof(registers));
    if (!dspic33_internal_validate_operand_alignment(cpu, registers, source_mode, source_register,
                                                     0u, 4u, false, false) ||
        !dspic33_internal_validate_operand_alignment(cpu, registers, destination_mode,
                                                     destination_register, 0u, 4u, true, false)) {
        return true;
    }
    if (source_mode == 0u) {
        source_register &= 0x0eu;
        low = cpu->w[source_register];
        high = cpu->w[source_register + 1u];
    } else {
        if (!dspic33_internal_operand_resolution(cpu, source_mode, source_register, 0u, 4u, false,
                                                 &resolution)) {
            return false;
        }
        address = resolution.address;
        low = dspic33_internal_read_data_word(cpu, address);
        if (source_register == 15u) {
            high_address = (uint16_t)(address + 2u);
            high = dspic33_internal_read_data_word(cpu, high_address);
        } else if (dspic33_internal_following_operand_address(cpu, &resolution, false,
                                                              &high_address)) {
            high = dspic33_internal_read_data_word(cpu, high_address);
        } else {
            high = 0u;
        }
    }
    if (destination_mode == 0u) {
        destination_register &= 0x0eu;
        dspic33_internal_write_working_register(cpu, destination_register, low);
        dspic33_internal_write_working_register(cpu, (uint8_t)(destination_register + 1u), high);
        return true;
    }
    if (!dspic33_internal_operand_resolution(cpu, destination_mode, destination_register, 0u, 4u,
                                             true, &resolution)) {
        return false;
    }
    address = resolution.address;
    dspic33_internal_write_word(cpu, address, low);
    if (destination_register == 15u) {
        high_address = (uint16_t)(address + 2u);
        dspic33_internal_write_word(cpu, high_address, high);
    } else if (dspic33_internal_following_operand_address(cpu, &resolution, true, &high_address)) {
        dspic33_internal_write_word(cpu, high_address, high);
    }
    return true;
}

void dspic33_internal_update_logic_flags(Dspic33* cpu, uint16_t value, bool byte_mode) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    value &= mask;
    cpu->sr = (uint16_t)(cpu->sr & ~0x000au);
    if (value == 0u) {
        cpu->sr |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        cpu->sr |= 0x0008u;
    }
}

static void update_add_flags(Dspic33* cpu, uint16_t left, uint16_t right, uint16_t carry_in,
                             uint32_t result, bool byte_mode, bool sticky_zero) {
    uint32_t mask = byte_mode ? 0xffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t digit_mask = byte_mode ? 0x000fu : 0x00ffu;
    uint16_t value = (uint16_t)(result & mask);
    int32_t signed_left = byte_mode ? (int8_t)left : (int16_t)left;
    int32_t signed_right = byte_mode ? (int8_t)right : (int16_t)right;
    int32_t signed_result = signed_left + signed_right + carry_in;
    int32_t signed_minimum = byte_mode ? INT8_MIN : INT16_MIN;
    int32_t signed_maximum = byte_mode ? INT8_MAX : INT16_MAX;
    bool previous_zero = (cpu->sr & 0x0002u) != 0u;
    cpu->sr = (uint16_t)(cpu->sr & ~0x010fu);
    if (value == 0u && (!sticky_zero || previous_zero)) {
        cpu->sr |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        cpu->sr |= 0x0008u;
    }
    if (result > mask) {
        cpu->sr |= 0x0001u;
    }
    if (((left & digit_mask) + (right & digit_mask) + carry_in) > digit_mask) {
        cpu->sr |= 0x0100u;
    }
    if (signed_result < signed_minimum || signed_result > signed_maximum) {
        cpu->sr |= 0x0004u;
    }
}

static void update_subtract_flags(Dspic33* cpu, uint16_t left, uint16_t right, uint16_t borrow,
                                  uint16_t value, bool byte_mode, bool sticky_zero) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t digit_mask = byte_mode ? 0x000fu : 0x00ffu;
    uint32_t subtraction = (right & mask) + borrow;
    uint16_t operand = (uint16_t)(subtraction & mask);
    bool previous_zero = (cpu->sr & 0x0002u) != 0u;
    left = (uint16_t)(left & mask);
    value = (uint16_t)(value & mask);
    cpu->sr = (uint16_t)(cpu->sr & ~0x010fu);
    if (value == 0u && (!sticky_zero || previous_zero)) {
        cpu->sr |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        cpu->sr |= 0x0008u;
    }
    if (left >= subtraction) {
        cpu->sr |= 0x0001u;
    }
    if ((uint32_t)(left & digit_mask) >= (uint32_t)(right & digit_mask) + borrow) {
        cpu->sr |= 0x0100u;
    }
    if ((((left ^ operand) & (left ^ value)) & sign) != 0u) {
        cpu->sr |= 0x0004u;
    }
}

static void update_binary_flags(Dspic33* cpu, uint32_t operation, uint16_t left, uint16_t right,
                                uint16_t borrow, uint16_t carry, uint32_t result, uint16_t value,
                                bool byte_mode, bool with_carry) {
    if (operation == 0x400000u || operation == 0x480000u) {
        update_add_flags(cpu, left, right, operation == 0x480000u ? carry : 0u, result, byte_mode,
                         operation == 0x480000u);
    } else if (operation == 0x500000u || operation == 0x580000u || operation == 0x100000u ||
               operation == 0x180000u) {
        update_subtract_flags(cpu, left, right, borrow, value, byte_mode, with_carry);
    } else {
        dspic33_internal_update_logic_flags(cpu, value, byte_mode);
    }
}

bool dspic33_internal_execute_binary(Dspic33* cpu, uint32_t opcode, uint32_t operation) {
    uint8_t left_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint16_t left = cpu->w[left_register];
    uint16_t right;
    uint32_t result;
    uint16_t value;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool with_carry = operation == 0x480000u || operation == 0x580000u || operation == 0x180000u;
    uint16_t carry = (cpu->sr & 1u) != 0u ? 1u : 0u;
    uint16_t borrow = 0u;
    uint32_t subtraction_right;

    if (destination_mode >= 6u) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }

    cpu->instruction_rmw = true;
    left = byte_mode ? (uint8_t)left : left;
    if ((opcode & 0x0060u) == 0x0060u) {
        right = (uint16_t)(opcode & 0x001fu);
    } else {
        uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
        right = byte_mode
                    ? dspic33_internal_read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                    : dspic33_internal_read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
    }
    if (cpu->illegal_reset) {
        return true;
    }
    right = byte_mode ? (uint8_t)right : right;
    subtraction_right = right;
    if (operation == 0x400000u) {
        result = (uint32_t)left + right;
    } else if (operation == 0x480000u) {
        result = (uint32_t)left + right + carry;
    } else if (operation == 0x500000u || operation == 0x580000u) {
        borrow = with_carry && carry == 0u ? 1u : 0u;
        subtraction_right = (uint32_t)right + borrow;
        result = (uint16_t)(left - subtraction_right);
    } else if (operation == 0x100000u || operation == 0x180000u) {
        borrow = with_carry && carry == 0u ? 1u : 0u;
        uint16_t swap = left;
        left = right;
        right = swap;
        subtraction_right = (uint32_t)right + borrow;
        result = (uint16_t)(left - subtraction_right);
    } else if (operation == 0x600000u) {
        result = left & right;
    } else if (operation == 0x680000u) {
        result = left ^ right;
    } else {
        result = left | right;
    }
    value = (uint16_t)result;
    if (!dspic33_internal_validate_destination_after_source_execution(
            cpu, destination_mode, destination, byte_mode ? 1u : 2u)) {
        if (!cpu->illegal_reset) {
            update_binary_flags(cpu, operation, left, right, borrow, carry, result, value,
                                byte_mode, with_carry);
        }
        return true;
    }
    if (byte_mode) {
        if (!dspic33_internal_write_operand_byte(cpu, destination_mode, destination, 0u,
                                                 (uint8_t)value)) {
            return false;
        }
    } else if (!dspic33_internal_write_operand_word(cpu, destination_mode, destination, 0u,
                                                    value)) {
        return false;
    }
    update_binary_flags(cpu, operation, left, right, borrow, carry, result, value, byte_mode,
                        with_carry);
    return true;
}

bool dspic33_internal_execute_compare(Dspic33* cpu, uint32_t opcode) {
    bool byte_mode;
    bool with_borrow;
    uint16_t borrow;
    uint16_t left;
    uint16_t right;
    uint16_t value;
    if ((opcode & 0xff0000u) == 0xe00000u) {
        uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
        if ((opcode & 0x00fb80u) != 0u || mode >= 6u) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        byte_mode = (opcode & 0x000400u) != 0u;
        left = byte_mode
                   ? dspic33_internal_read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                   : dspic33_internal_read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
        right = 0u;
        with_borrow = false;
    } else if ((opcode & 0xff0000u) == 0xe10000u) {
        uint8_t base = (uint8_t)((opcode >> 11u) & 0x0fu);
        byte_mode = (opcode & 0x000400u) != 0u;
        left = byte_mode ? (uint8_t)cpu->w[base] : cpu->w[base];
        if ((opcode & 0x0060u) == 0x0060u) {
            right = (uint16_t)(((opcode >> 2u) & 0x00e0u) | (opcode & 0x001fu));
        } else {
            uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
            if ((opcode & 0x0380u) != 0u) {
                dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
                return true;
            }
            right =
                byte_mode
                    ? dspic33_internal_read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                    : dspic33_internal_read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
        }
        with_borrow = (opcode & 0x008000u) != 0u;
    } else if ((opcode & 0xff0000u) == 0xe30000u) {
        if ((opcode & 0x002000u) != 0u) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        byte_mode = (opcode & 0x004000u) != 0u;
        left = byte_mode ? dspic33_internal_read_data_byte(cpu, address)
                         : dspic33_internal_read_file_word(cpu, address);
        right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
        with_borrow = (opcode & 0x008000u) != 0u;
    } else if ((opcode & 0xff0000u) == 0xe20000u) {
        if ((opcode & 0x00a000u) != 0u) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        byte_mode = (opcode & 0x004000u) != 0u;
        left = byte_mode ? dspic33_internal_read_data_byte(cpu, address)
                         : dspic33_internal_read_file_word(cpu, address);
        right = 0u;
        with_borrow = false;
    } else {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (cpu->illegal_reset) {
        return true;
    }
    borrow = with_borrow && (cpu->sr & 1u) == 0u ? 1u : 0u;
    value = (uint16_t)(left - right - borrow);
    update_subtract_flags(cpu, left, right, borrow, value, byte_mode, with_borrow);
    return true;
}

static void update_unary_flags(Dspic33* cpu, uint8_t family, bool alternate, uint16_t source,
                               uint16_t value, bool byte_mode, bool arithmetic) {
    if (family == 0xe8u) {
        update_add_flags(cpu, source, alternate ? 2u : 1u, 0u,
                         (uint32_t)source + (alternate ? 2u : 1u), byte_mode, false);
    } else if (family == 0xe9u) {
        update_subtract_flags(cpu, source, alternate ? 2u : 1u, 0u, value, byte_mode, false);
    } else if (family == 0xeau && arithmetic) {
        update_subtract_flags(cpu, 0u, source, 0u, value, byte_mode, false);
    } else if (family == 0xeau) {
        dspic33_internal_update_logic_flags(cpu, value, byte_mode);
    }
}

bool dspic33_internal_execute_unary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool alternate = (opcode & 0x008000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool reads_source = family != 0xebu;
    uint16_t source;
    uint16_t value;
    bool arithmetic = true;
    if (destination_mode >= 6u || (reads_source && source_mode >= 6u) ||
        (!reads_source && (opcode & 0x00007fu) != 0u)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    cpu->instruction_rmw = true;
    source = reads_source
                 ? (byte_mode
                        ? dspic33_internal_read_operand_byte(cpu, source_mode, source_register, 0u)
                        : dspic33_internal_read_operand_word(cpu, source_mode, source_register, 0u))
                 : 0u;
    if (cpu->illegal_reset) {
        return true;
    }
    if (family == 0xe8u) {
        value = (uint16_t)(source + (alternate ? 2u : 1u));
    } else if (family == 0xe9u) {
        value = (uint16_t)(source - (alternate ? 2u : 1u));
    } else if (family == 0xeau) {
        value = alternate ? (uint16_t)~source : (uint16_t)(0u - source);
        arithmetic = !alternate;
    } else if (family == 0xebu) {
        value = alternate ? 0xffffu : 0u;
        arithmetic = false;
    } else {
        return false;
    }
    if (!dspic33_internal_validate_destination_after_source_execution(
            cpu, destination_mode, destination_register, byte_mode ? 1u : 2u)) {
        if (!cpu->illegal_reset) {
            update_unary_flags(cpu, family, alternate, source, value, byte_mode, arithmetic);
        }
        return true;
    }
    if (byte_mode) {
        if (!dspic33_internal_write_operand_byte(cpu, destination_mode, destination_register, 0u,
                                                 (uint8_t)value)) {
            return false;
        }
    } else if (!dspic33_internal_write_operand_word(cpu, destination_mode, destination_register, 0u,
                                                    value)) {
        return false;
    }
    update_unary_flags(cpu, family, alternate, source, value, byte_mode, arithmetic);
    return true;
}

static uint16_t table_adjust_pointer(uint16_t pointer, int16_t adjustment) {
    uint16_t adjusted = (uint16_t)(pointer + adjustment);
    if ((pointer & 0x8000u) != 0u && (adjusted & 0x8000u) == 0u) {
        adjusted |= 0x8000u;
    }
    return adjusted;
}

static uint16_t table_operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg, uint8_t width,
                                      bool source) {
    uint16_t address = cpu->w[reg];
    int32_t adjusted_address = cpu->w[reg];
    int32_t effective_address = address;
    bool wrapped = false;
    if (source) {
        dspic33_internal_record_source_address_register(cpu, reg);
    }
    if (mode == 2u || mode == 3u) {
        adjusted_address += mode == 3u ? width : -(int32_t)width;
        wrapped = adjusted_address < 0 || adjusted_address > UINT16_MAX;
        dspic33_internal_write_working_register(
            cpu, reg,
            table_adjust_pointer(cpu->w[reg], mode == 3u ? (int16_t)width : -(int16_t)width));
    } else if (mode == 4u || mode == 5u) {
        adjusted_address += mode == 5u ? width : -(int32_t)width;
        wrapped = adjusted_address < 0 || adjusted_address > UINT16_MAX;
        dspic33_internal_write_working_register(
            cpu, reg,
            table_adjust_pointer(cpu->w[reg], mode == 5u ? (int16_t)width : -(int16_t)width));
        address = reg == 15u ? (uint16_t)adjusted_address : cpu->w[reg];
        effective_address = adjusted_address;
    }
    if (reg == 15u) {
        dspic33_internal_check_stack_address(cpu, effective_address, wrapped);
    }
    return address;
}

bool dspic33_internal_execute_table(Dspic33* cpu, uint32_t opcode) {
    bool write = (opcode & 0x010000u) != 0u;
    bool high = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t table_offset;
    uint32_t address;
    uint32_t word;
    uint16_t value;
    bool unimplemented_read;
    if ((source_mode != 0u &&
         !dspic33_internal_address_register_initialized(cpu, source_register)) ||
        (destination_mode != 0u &&
         !dspic33_internal_address_register_initialized(cpu, destination_register))) {
        return true;
    }
    if (write) {
        value = byte_mode
                    ? dspic33_internal_read_operand_byte(cpu, source_mode, source_register, 0u)
                    : dspic33_internal_read_operand_word(cpu, source_mode, source_register, 0u);
        if (cpu->illegal_reset) {
            return true;
        }
        if (cpu->address_error && !cpu->address_error_access_allowed) {
            return true;
        }
        table_offset = table_operand_address(cpu, destination_mode, destination_register,
                                             byte_mode ? 1u : 2u, false);
    } else {
        table_offset =
            table_operand_address(cpu, source_mode, source_register, byte_mode ? 1u : 2u, true);
        value = 0u;
    }
    address = ((((uint32_t)cpu->tblpag & 0x01ffu) << 16u) | table_offset) & 0x01fffffeu;
    unimplemented_read =
        !write && dspic33_internal_program_target_requires_address_error(cpu, address);
    word = unimplemented_read ? 0u
           : write            ? dspic33_read_program_word(cpu, address)
                              : dspic33_internal_read_cpu_program_word(cpu, address);
    if (write) {
        if (high) {
            if (!byte_mode || (table_offset & 1u) == 0u) {
                word = (word & 0x0000ffffu) | ((uint32_t)(uint8_t)value << 16u);
            }
        } else if (byte_mode) {
            uint8_t shift = (uint8_t)((table_offset & 1u) * 8u);
            word = (word & ~(0xffu << shift)) | ((uint32_t)(uint8_t)value << shift);
        } else {
            word = (word & 0x00ff0000u) | value;
        }
        if (address >= DSPIC33_WRITE_LATCH_BASE && address < DSPIC33_WRITE_LATCH_LIMIT) {
            cpu->write_latches[(address - DSPIC33_WRITE_LATCH_BASE) / 2u] = word & 0x00ffffffu;
        }
        return true;
    }
    if (high) {
        value = byte_mode && (table_offset & 1u) != 0u ? 0u : (uint16_t)((word >> 16u) & 0xffu);
    } else if (byte_mode) {
        value = (uint16_t)((word >> ((table_offset & 1u) * 8u)) & 0xffu);
    } else {
        value = (uint16_t)word;
    }
    if (unimplemented_read) {
        uint32_t destination_address = 0u;
        if (destination_mode == 0u) {
            if (byte_mode) {
                dspic33_internal_write_working_register_byte(cpu, destination_register, false,
                                                             (uint8_t)value);
            } else {
                dspic33_internal_write_working_register(cpu, destination_register, value);
            }
        } else {
            if (!dspic33_internal_validate_destination_after_source_execution(
                    cpu, destination_mode, destination_register, byte_mode ? 1u : 2u)) {
                if (!cpu->illegal_reset) {
                    dspic33_internal_raise_program_read_error(cpu);
                }
                return true;
            }
            if (!dspic33_internal_operand_address(cpu, destination_mode, destination_register, 0u,
                                                  byte_mode ? 1u : 2u, true,
                                                  &destination_address)) {
                return true;
            }
            if (byte_mode) {
                dspic33_write_byte(cpu, destination_address, (uint8_t)value);
            } else {
                dspic33_internal_write_word(cpu, destination_address, value);
            }
        }
        dspic33_internal_raise_program_read_error(cpu);
        return true;
    }
    if (byte_mode) {
        return dspic33_internal_write_operand_byte(cpu, destination_mode, destination_register, 0u,
                                                   (uint8_t)value);
    }
    return dspic33_internal_write_operand_word(cpu, destination_mode, destination_register, 0u,
                                               value);
}

bool dspic33_internal_execute_literal_binary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    uint16_t literal = (uint16_t)((opcode >> 4u) & 0x03ffu);
    uint16_t left = byte_mode ? (uint8_t)cpu->w[destination] : cpu->w[destination];
    uint16_t value;
    uint32_t result;
    uint16_t carry = (cpu->sr & 1u) != 0u ? 1u : 0u;
    if (byte_mode) {
        literal &= 0x00ffu;
    }
    if (family == 0u) {
        result = (uint32_t)left + literal + (alternate ? carry : 0u);
        value = (uint16_t)result;
        update_add_flags(cpu, left, literal, alternate ? carry : 0u, result, byte_mode, alternate);
    } else if (family == 1u) {
        uint16_t borrow = alternate && carry == 0u ? 1u : 0u;
        value = (uint16_t)(left - literal - borrow);
        update_subtract_flags(cpu, left, literal, borrow, value, byte_mode, alternate);
    } else if (family == 2u) {
        value = alternate ? (uint16_t)(left ^ literal) : (uint16_t)(left & literal);
        dspic33_internal_update_logic_flags(cpu, value, byte_mode);
    } else if (!alternate) {
        value = (uint16_t)(left | literal);
        dspic33_internal_update_logic_flags(cpu, value, byte_mode);
    } else {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (byte_mode) {
        dspic33_internal_write_working_register_byte(cpu, destination, false, (uint8_t)value);
    } else {
        dspic33_internal_write_working_register(cpu, destination, value);
    }
    return true;
}

bool dspic33_internal_execute_file_binary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool reverse_subtract = (opcode & 0xff0000u) == 0xbd0000u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool file_destination = (opcode & 0x002000u) != 0u;
    uint16_t address = (uint16_t)(opcode & 0x1fffu);
    uint16_t left;
    uint16_t right;
    uint16_t carry = (cpu->sr & 1u) != 0u ? 1u : 0u;
    uint16_t value;
    uint32_t result;
    if (family == 3u && alternate) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    cpu->instruction_rmw = true;
    left = byte_mode ? dspic33_internal_read_data_byte(cpu, address)
                     : dspic33_internal_read_file_word(cpu, address);
    right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
    if (family == 0u) {
        result = (uint32_t)left + right + (alternate ? carry : 0u);
        value = (uint16_t)result;
        update_add_flags(cpu, left, right, alternate ? carry : 0u, result, byte_mode, alternate);
    } else if (reverse_subtract) {
        uint16_t borrow = alternate && carry == 0u ? 1u : 0u;
        value = (uint16_t)(right - left - borrow);
        update_subtract_flags(cpu, right, left, borrow, value, byte_mode, alternate);
    } else if (family == 1u) {
        uint16_t borrow = alternate && carry == 0u ? 1u : 0u;
        value = (uint16_t)(left - right - borrow);
        update_subtract_flags(cpu, left, right, borrow, value, byte_mode, alternate);
    } else if (family == 2u) {
        value = alternate ? (uint16_t)(left ^ right) : (uint16_t)(left & right);
        dspic33_internal_update_logic_flags(cpu, value, byte_mode);
    } else {
        value = (uint16_t)(left | right);
        dspic33_internal_update_logic_flags(cpu, value, byte_mode);
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

typedef enum { SKIP_RETURN_TARGET, SKIP_RETURN_SEQUENTIAL } SkipReturn;

CompareControlKind dspic33_internal_compare_control_kind(uint32_t opcode) {
    switch (opcode & 0xff8000u) {
    case 0xe78000u:
        return COMPARE_CONTROL_EQUAL;
    case 0xe70000u:
        return COMPARE_CONTROL_NOT_EQUAL;
    case 0xe60000u:
        return COMPARE_CONTROL_GREATER_THAN;
    case 0xe68000u:
        return COMPARE_CONTROL_LESS_THAN;
    default:
        return COMPARE_CONTROL_NONE;
    }
}

int8_t dspic33_internal_compare_control_displacement(uint32_t opcode) {
    uint8_t encoded = (uint8_t)((opcode >> 4u) & 0x3fu);
    return (int8_t)((encoded & 0x20u) != 0u ? encoded | 0xc0u : encoded);
}

static void skip_instruction(Dspic33* cpu, SkipReturn return_kind) {
    uint32_t length;
    uint32_t next;
    uint32_t sequential = cpu->pc;
    uint32_t raw_target;
    uint32_t target;
    if (!dspic33_device_program_range_implemented(cpu, sequential, 2u)) {
        dspic33_internal_raise_program_target_error(cpu, sequential);
        return;
    }
    next = dspic33_read_program_word(cpu, sequential);
    length = dspic33_internal_instruction_length(next);
    raw_target = sequential + length;
    target = raw_target >= DSPIC33_AUXILIARY_PROGRAM_LIMIT ? raw_target & 0x007ffffeu : raw_target;
    cpu->pc = target;
    if (length == 2u && raw_target == dspic33_internal_device_program_limit(cpu)) {
        dspic33_internal_raise_program_target_error(
            cpu, return_kind == SKIP_RETURN_TARGET ? raw_target : sequential);
    } else if (length == 4u && raw_target == dspic33_internal_device_program_limit(cpu)) {
        cpu->sequential_program_hole_pc = raw_target;
    }
}

static int32_t signed_compare_operand(uint16_t value, bool byte_mode) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint32_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint32_t masked = value & mask;
    return (masked & sign) != 0u ? (int32_t)masked - (int32_t)(mask + 1u) : (int32_t)masked;
}

bool dspic33_internal_compare_control_taken(const Dspic33* cpu, uint32_t opcode,
                                            CompareControlKind kind) {
    bool byte_mode = (opcode & 0x000400u) != 0u;
    uint16_t left = cpu->w[(opcode >> 11u) & 0x0fu];
    uint16_t right = cpu->w[opcode & 0x0fu];
    if (byte_mode) {
        left &= 0x00ffu;
        right &= 0x00ffu;
    }
    if (kind == COMPARE_CONTROL_EQUAL) {
        return left == right;
    }
    if (kind == COMPARE_CONTROL_NOT_EQUAL) {
        return left != right;
    }
    if (kind == COMPARE_CONTROL_GREATER_THAN) {
        return signed_compare_operand(left, byte_mode) > signed_compare_operand(right, byte_mode);
    }
    return kind == COMPARE_CONTROL_LESS_THAN &&
           signed_compare_operand(left, byte_mode) < signed_compare_operand(right, byte_mode);
}

bool dspic33_internal_execute_compare_control(Dspic33* cpu, uint32_t opcode) {
    CompareControlKind kind = dspic33_internal_compare_control_kind(opcode);
    int8_t displacement = dspic33_internal_compare_control_displacement(opcode);
    uint32_t target;
    if (kind == COMPARE_CONTROL_NONE) {
        return false;
    }
    if (!dspic33_internal_compare_control_taken(cpu, opcode, kind)) {
        return true;
    }
    if (displacement == 1) {
        skip_instruction(cpu, SKIP_RETURN_SEQUENTIAL);
        return true;
    }
    target = dspic33_internal_program_address_add(cpu->pc, (int32_t)displacement * 2);
    if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
        dspic33_internal_raise_program_target_error(cpu, cpu->pc);
        return true;
    }
    cpu->pc = target;
    return true;
}

bool dspic33_internal_execute_bit(Dspic33* cpu, uint32_t opcode) {
    uint8_t kind = (uint8_t)((opcode >> 16u) & 0x07u);
    bool file = (opcode & 0x080000u) != 0u;
    bool indirect = false;
    uint8_t bit;
    uint16_t value;
    uint16_t mask;
    uint32_t address = 0u;
    uint8_t mode = 0u;
    uint8_t reg = 0u;
    bool byte_mode = false;
    cpu->instruction_rmw = true;
    if (file) {
        bit = (uint8_t)((opcode >> 13u) & 0x07u);
        address = (uint16_t)(opcode & 0x1fffu);
        value = dspic33_internal_read_data_byte(cpu, address);
    } else {
        mode = (uint8_t)((opcode >> 4u) & 0x07u);
        reg = (uint8_t)(opcode & 0x0fu);
        byte_mode = (opcode & 0x000400u) != 0u;
        if (kind == 5u) {
            bit = (uint8_t)(cpu->w[(opcode >> 11u) & 0x0fu] & 0x0fu);
        } else {
            bit = (uint8_t)((opcode >> 12u) & 0x0fu);
        }
        if (mode == 0u) {
            value = byte_mode ? (uint8_t)cpu->w[reg] : cpu->w[reg];
        } else {
            uint16_t registers[16];
            memcpy(registers, cpu->w, sizeof(registers));
            if (!dspic33_internal_validate_operand_alignment(
                    cpu, registers, mode, reg, 0u, byte_mode ? 1u : 2u, false, !byte_mode)) {
                return true;
            }
            if (!dspic33_internal_operand_address(cpu, mode, reg, 0u, byte_mode ? 1u : 2u, false,
                                                  &address)) {
                return false;
            }
            if (kind == 4u && address == 0x0042u) {
                dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
                return true;
            }
            indirect = true;
            value = byte_mode ? dspic33_internal_read_data_byte(cpu, address)
                              : dspic33_internal_read_data_word(cpu, address);
        }
    }
    mask = (uint16_t)(1u << bit);
    if (kind == 0u) {
        value |= mask;
    } else if (kind == 1u) {
        value &= (uint16_t)~mask;
    } else if (kind == 2u) {
        value ^= mask;
    } else if (kind == 3u || kind == 5u) {
        bool zero_destination = (opcode & (kind == 5u ? 0x008000u : 0x000800u)) != 0u;
        if (zero_destination || file) {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0002u) | ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0001u) | ((value & mask) != 0u ? 0x0001u : 0u));
        }
        return true;
    } else if (kind == 4u) {
        bool zero_destination = (opcode & 0x000800u) != 0u;
        if (zero_destination || file) {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0002u) | ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0001u) | ((value & mask) != 0u ? 0x0001u : 0u));
        }
        value |= mask;
    } else if (kind == 6u || kind == 7u) {
        bool set = (value & mask) != 0u;
        if ((kind == 6u && set) || (kind == 7u && !set)) {
            skip_instruction(cpu, SKIP_RETURN_TARGET);
        }
        return true;
    }
    if (file) {
        dspic33_write_byte(cpu, address, (uint8_t)value);
    } else if (indirect) {
        if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)value);
        } else {
            dspic33_write_word(cpu, address, value);
        }
    } else if (byte_mode) {
        dspic33_internal_write_working_register_byte(cpu, reg, false, (uint8_t)value);
    } else {
        dspic33_internal_write_working_register(cpu, reg, value);
    }
    return true;
}

bool dspic33_internal_execute_file_unary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool file_destination = (opcode & 0x002000u) != 0u;
    uint16_t address = (uint16_t)(opcode & 0x1fffu);
    uint16_t source = 0u;
    uint16_t value;
    cpu->instruction_rmw = true;
    if (family != 0xefu) {
        source = byte_mode ? dspic33_internal_read_data_byte(cpu, address)
                           : dspic33_internal_read_file_word(cpu, address);
    }
    if (family == 0xecu) {
        value = (uint16_t)(source + (alternate ? 2u : 1u));
        update_add_flags(cpu, source, alternate ? 2u : 1u, 0u,
                         (uint32_t)source + (alternate ? 2u : 1u), byte_mode, false);
    } else if (family == 0xedu) {
        value = (uint16_t)(source - (alternate ? 2u : 1u));
        update_subtract_flags(cpu, source, alternate ? 2u : 1u, 0u, value, byte_mode, false);
    } else if (family == 0xeeu) {
        value = alternate ? (uint16_t)~source : (uint16_t)(0u - source);
        if (alternate) {
            dspic33_internal_update_logic_flags(cpu, value, byte_mode);
        } else {
            update_subtract_flags(cpu, 0u, source, 0u, value, byte_mode, false);
        }
    } else if (family == 0xefu) {
        value = alternate ? 0xffffu : 0u;
    } else {
        return false;
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

static bool branch_condition(const Dspic33* cpu, uint8_t condition, bool* take) {
    bool carry = (cpu->sr & 0x0001u) != 0u;
    bool zero = (cpu->sr & 0x0002u) != 0u;
    bool overflow = (cpu->sr & 0x0004u) != 0u;
    bool negative = (cpu->sr & 0x0008u) != 0u;
    switch (condition) {
    case 0x00u:
        *take = overflow;
        break;
    case 0x01u:
        *take = carry;
        break;
    case 0x02u:
        *take = zero;
        break;
    case 0x03u:
        *take = negative;
        break;
    case 0x04u:
        *take = zero || negative != overflow;
        break;
    case 0x05u:
        *take = negative != overflow;
        break;
    case 0x06u:
        *take = !carry || zero;
        break;
    case 0x07u:
        *take = true;
        break;
    case 0x08u:
        *take = !overflow;
        break;
    case 0x09u:
        *take = !carry;
        break;
    case 0x0au:
        *take = !zero;
        break;
    case 0x0bu:
        *take = !negative;
        break;
    case 0x0cu:
        *take = !zero && negative == overflow;
        break;
    case 0x0du:
        *take = negative == overflow;
        break;
    case 0x0eu:
        *take = carry && !zero;
        break;
    default:
        return false;
    }
    return true;
}

bool dspic33_internal_relative_branch_condition(const Dspic33* cpu, uint32_t opcode, bool* take) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    if (family >= 0x0cu && family <= 0x0fu) {
        *take = (cpu->sr & (uint16_t)(0x8000u >> (family - 0x0cu))) != 0u;
        return true;
    }
    if ((opcode & 0xf00000u) == 0x300000u) {
        return branch_condition(cpu, (uint8_t)(family & 0x0fu), take);
    }
    return false;
}

bool dspic33_internal_long_control_transfer(uint32_t opcode, uint8_t* source, bool* call) {
    uint8_t encoded_source = (uint8_t)(opcode & 0x0fu);
    uint32_t base;
    if ((encoded_source & 1u) != 0u || encoded_source > 12u) {
        return false;
    }
    base = 0x018000u | ((uint32_t)(encoded_source + 1u) << 11u) | encoded_source;
    if (opcode == base) {
        *call = true;
    } else if (opcode == (base | 0x000400u)) {
        *call = false;
    } else {
        return false;
    }
    *source = encoded_source;
    return true;
}

bool dspic33_internal_computed_control_transfer_encoding(uint32_t opcode) {
    uint8_t source;
    bool call;
    return (opcode & 0xfffff0u) == 0x010000u || (opcode & 0xfffff0u) == 0x010200u ||
           (opcode & 0xfffff0u) == 0x010400u || (opcode & 0xfffff0u) == 0x010600u ||
           dspic33_internal_long_control_transfer(opcode, &source, &call);
}

bool dspic33_internal_instruction_changes_program_flow(const Dspic33* cpu, uint32_t opcode,
                                                       uint32_t instruction_pc) {
    bool branch_taken;
    CompareControlKind compare_kind = dspic33_internal_compare_control_kind(opcode);
    uint8_t family = (uint8_t)(opcode >> 16u);
    if (cpu->pc != dspic33_internal_program_address_add(
                       instruction_pc, (int32_t)dspic33_internal_instruction_length(opcode))) {
        return true;
    }
    if (compare_kind != COMPARE_CONTROL_NONE) {
        return dspic33_internal_compare_control_taken(cpu, opcode, compare_kind);
    }
    if (dspic33_internal_relative_branch_condition(cpu, opcode, &branch_taken)) {
        return branch_taken;
    }
    return family == 0x02u || family == 0x04u || family == 0x05u || family == 0x07u ||
           dspic33_internal_computed_control_transfer_encoding(opcode);
}
