#include "dspic33.h"

#include <stdlib.h>
#include <string.h>

#include "device.h"

static uint16_t read_word(Dspic33* cpu, uint32_t address) {
    if (address < 32u) {
        return cpu->w[address / 2u];
    }
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static void write_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    dspic33_write_word(cpu, address, value);
}

static bool operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg, uint8_t offset_reg,
                            uint8_t width, bool write, uint32_t* address) {
    if (mode == 0u) {
        return false;
    }
    if (mode == 1u) {
        *address = cpu->w[reg];
    } else if (mode == 2u || mode == 3u) {
        *address = cpu->w[reg];
        cpu->w[reg] = (uint16_t)(cpu->w[reg] + (mode == 3u ? width : -width));
    } else if (mode == 4u || mode == 5u) {
        cpu->w[reg] = (uint16_t)(cpu->w[reg] + (mode == 5u ? width : -width));
        *address = cpu->w[reg];
    } else {
        *address = (uint16_t)(cpu->w[reg] + cpu->w[offset_reg]);
    }
    if (*address >= 0x8000u &&
        !((reg == 14u || reg == 15u) && (cpu->corcon & 0x0004u) != 0u)) {
        uint16_t page = write ? cpu->dswpag : cpu->dsrpag;
        *address = (((uint32_t)page << 15u) | (*address & 0x7fffu)) % DSPIC33_DATA_SIZE;
    }
    return true;
}

static uint8_t read_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                 uint8_t offset_reg) {
    uint32_t address;
    if (mode == 0u) {
        return (uint8_t)cpu->w[reg];
    }
    if (!operand_address(cpu, mode, reg, offset_reg, 1u, false, &address)) {
        return 0u;
    }
    return dspic33_read_byte(cpu, address);
}

static uint16_t read_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                  uint8_t offset_reg) {
    uint32_t address;
    if (mode == 0u) {
        return cpu->w[reg];
    }
    if (!operand_address(cpu, mode, reg, offset_reg, 2u, false, &address)) {
        return 0u;
    }
    return read_word(cpu, address);
}

static bool write_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg,
                               uint8_t offset_reg, uint8_t value) {
    uint32_t address;
    if (mode == 0u) {
        cpu->w[reg] = (uint16_t)((cpu->w[reg] & 0xff00u) | value);
        return true;
    }
    if (!operand_address(cpu, mode, reg, offset_reg, 1u, true, &address)) {
        return false;
    }
    dspic33_write_byte(cpu, address, value);
    return true;
}

static bool write_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg,
                               uint8_t offset_reg, uint16_t value) {
    uint32_t address;
    if (mode == 0u) {
        cpu->w[reg] = value;
        return true;
    }
    if (!operand_address(cpu, mode, reg, offset_reg, 2u, true, &address)) {
        return false;
    }
    dspic33_write_word(cpu, address, value);
    return true;
}

static bool execute_move_literal(Dspic33* cpu, uint32_t opcode) {
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    uint16_t literal = (uint16_t)((opcode >> 4u) & 0xffffu);
    cpu->w[destination] = literal;
    return true;
}

static bool execute_move(Dspic33* cpu, uint32_t opcode) {
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t offset_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;

    if (byte_mode) {
        uint8_t value =
            read_operand_byte(cpu, source_mode, source_register, offset_register);
        return write_operand_byte(cpu, destination_mode, destination_register,
                                  offset_register, value);
    }
    return write_operand_word(
        cpu, destination_mode, destination_register, offset_register,
        read_operand_word(cpu, source_mode, source_register, offset_register));
}

static uint32_t indirect_literal_address(Dspic33* cpu, uint8_t reg, int16_t offset,
                                         bool write) {
    uint32_t address = (uint16_t)(cpu->w[reg] + offset);
    if (address >= 0x8000u &&
        !((reg == 14u || reg == 15u) && (cpu->corcon & 0x0004u) != 0u)) {
        uint16_t page = write ? cpu->dswpag : cpu->dsrpag;
        address = (((uint32_t)page << 15u) | (address & 0x7fffu)) % DSPIC33_DATA_SIZE;
    }
    return address;
}

static bool execute_move_offset(Dspic33* cpu, uint32_t opcode) {
    uint16_t encoded =
        (uint16_t)((((opcode >> 15u) & 0x0fu) << 6u) |
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
        address = indirect_literal_address(cpu, destination, offset, true);
        if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)cpu->w[source]);
        } else {
            dspic33_write_word(cpu, address, cpu->w[source]);
        }
    } else {
        address = indirect_literal_address(cpu, source, offset, false);
        if (byte_mode) {
            cpu->w[destination] = (uint16_t)((cpu->w[destination] & 0xff00u) |
                                             dspic33_read_byte(cpu, address));
        } else {
            cpu->w[destination] = dspic33_read_word(cpu, address);
        }
    }
    return true;
}

static bool execute_move_double(Dspic33* cpu, uint32_t opcode) {
    if ((opcode & 0xfffff0u) == 0xbe9f80u) {
        uint8_t source = (uint8_t)(opcode & 0x0eu);
        write_word(cpu, cpu->w[15], cpu->w[source]);
        cpu->w[15] += 2u;
        write_word(cpu, cpu->w[15], cpu->w[source + 1u]);
        cpu->w[15] += 2u;
        return true;
    }
    if ((opcode & 0xfff87fu) == 0xbe004fu) {
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0eu);
        cpu->w[15] -= 2u;
        cpu->w[destination + 1u] = read_word(cpu, cpu->w[15]);
        cpu->w[15] -= 2u;
        cpu->w[destination] = read_word(cpu, cpu->w[15]);
        return true;
    }
    if ((opcode & 0xffc070u) == 0xbe0000u) {
        uint8_t source = (uint8_t)(opcode & 0x0eu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0eu);
        cpu->w[destination] = cpu->w[source];
        cpu->w[destination + 1u] = cpu->w[source + 1u];
        return true;
    }
    return false;
}

static void update_logic_flags(Dspic33* cpu, uint16_t value, bool byte_mode) {
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

static void update_add_flags(Dspic33* cpu, uint16_t left, uint16_t right,
                             uint32_t result, bool byte_mode, bool sticky_zero) {
    uint32_t mask = byte_mode ? 0xffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t digit_mask = byte_mode ? 0x000fu : 0x00ffu;
    uint16_t value = (uint16_t)(result & mask);
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
    if (((left & digit_mask) + (right & digit_mask)) > digit_mask) {
        cpu->sr |= 0x0100u;
    }
    if (((~(left ^ right) & (left ^ value)) & sign) != 0u) {
        cpu->sr |= 0x0004u;
    }
}

static void update_subtract_flags(Dspic33* cpu, uint16_t left, uint16_t right,
                                  uint16_t value, bool byte_mode, bool sticky_zero) {
    uint16_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint16_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint16_t digit_mask = byte_mode ? 0x000fu : 0x00ffu;
    bool previous_zero = (cpu->sr & 0x0002u) != 0u;
    left &= mask;
    right &= mask;
    value &= mask;
    cpu->sr = (uint16_t)(cpu->sr & ~0x010fu);
    if (value == 0u && (!sticky_zero || previous_zero)) {
        cpu->sr |= 0x0002u;
    }
    if ((value & sign) != 0u) {
        cpu->sr |= 0x0008u;
    }
    if (left >= right) {
        cpu->sr |= 0x0001u;
    }
    if ((left & digit_mask) >= (right & digit_mask)) {
        cpu->sr |= 0x0100u;
    }
    if ((((left ^ right) & (left ^ value)) & sign) != 0u) {
        cpu->sr |= 0x0004u;
    }
}

static bool execute_binary(Dspic33* cpu, uint32_t opcode, uint32_t operation) {
    uint8_t left_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t left = cpu->w[left_register];
    uint16_t right;
    uint32_t result;
    uint16_t value;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool with_carry =
        operation == 0x480000u || operation == 0x580000u || operation == 0x180000u;
    uint16_t carry = (cpu->sr & 1u) != 0u ? 1u : 0u;

    left = byte_mode ? (uint8_t)left : left;
    if ((opcode & 0x0060u) == 0x0060u) {
        right = (uint16_t)(opcode & 0x001fu);
    } else if ((opcode & 0x0070u) <= 0x0050u) {
        uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
        right = byte_mode ? read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                          : read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
    } else {
        return false;
    }
    right = byte_mode ? (uint8_t)right : right;
    if (operation == 0x400000u) {
        result = (uint32_t)left + right;
    } else if (operation == 0x480000u) {
        result = (uint32_t)left + right + carry;
    } else if (operation == 0x500000u || operation == 0x580000u) {
        uint16_t borrow = with_carry && carry == 0u ? 1u : 0u;
        result = (uint16_t)(left - right - borrow);
    } else if (operation == 0x100000u || operation == 0x180000u) {
        uint16_t borrow = with_carry && carry == 0u ? 1u : 0u;
        uint16_t swap = left;
        left = right;
        right = swap;
        result = (uint16_t)(left - right - borrow);
    } else if (operation == 0x600000u) {
        result = left & right;
    } else if (operation == 0x680000u) {
        result = left ^ right;
    } else {
        result = left | right;
    }
    value = (uint16_t)result;
    if (byte_mode) {
        if (!write_operand_byte(cpu, (uint8_t)((opcode >> 11u) & 0x07u), destination,
                                0u, (uint8_t)value)) {
            return false;
        }
    } else if (!write_operand_word(cpu, (uint8_t)((opcode >> 11u) & 0x07u), destination,
                                   0u, value)) {
        return false;
    }
    if (operation == 0x400000u || operation == 0x480000u) {
        update_add_flags(cpu, left, right + (operation == 0x480000u ? carry : 0u),
                         result, byte_mode, operation == 0x480000u);
    } else if (operation == 0x500000u || operation == 0x580000u ||
               operation == 0x100000u || operation == 0x180000u) {
        update_subtract_flags(cpu, left, right, value, byte_mode, with_carry);
    } else {
        update_logic_flags(cpu, value, byte_mode);
    }
    return true;
}

static bool execute_compare(Dspic33* cpu, uint32_t opcode) {
    bool byte_mode;
    bool borrow;
    uint16_t left;
    uint16_t right;
    uint16_t value;
    if ((opcode & 0xff0000u) == 0xe00000u) {
        uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
        byte_mode = (opcode & 0x000400u) != 0u;
        left = byte_mode ? read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                         : read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
        right = 0u;
        borrow = false;
    } else if ((opcode & 0xff0000u) == 0xe10000u) {
        uint8_t base = (uint8_t)((opcode >> 11u) & 0x0fu);
        byte_mode = (opcode & 0x000400u) != 0u;
        left = byte_mode ? (uint8_t)cpu->w[base] : cpu->w[base];
        if ((opcode & 0x0060u) == 0x0060u) {
            right = (uint16_t)(opcode & 0x001fu);
        } else {
            uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
            right = byte_mode
                        ? read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                        : read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
        }
        borrow = (opcode & 0x008000u) != 0u;
    } else {
        return false;
    }
    if (borrow && (cpu->sr & 1u) == 0u) {
        right++;
    }
    value = (uint16_t)(left - right);
    update_subtract_flags(cpu, left, right, value, byte_mode, borrow);
    return true;
}

static bool execute_unary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool alternate = (opcode & 0x008000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t source = byte_mode
                          ? read_operand_byte(cpu, source_mode, source_register, 0u)
                          : read_operand_word(cpu, source_mode, source_register, 0u);
    uint16_t value;
    bool arithmetic = true;
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
    if (byte_mode) {
        if (!write_operand_byte(cpu, destination_mode, destination_register, 0u,
                                (uint8_t)value)) {
            return false;
        }
    } else if (!write_operand_word(cpu, destination_mode, destination_register, 0u,
                                   value)) {
        return false;
    }
    if (family == 0xe8u) {
        update_add_flags(cpu, source, alternate ? 2u : 1u,
                         (uint32_t)source + (alternate ? 2u : 1u), byte_mode, false);
    } else if (family == 0xe9u) {
        update_subtract_flags(cpu, source, alternate ? 2u : 1u, value, byte_mode,
                              false);
    } else if (family == 0xeau && arithmetic) {
        update_subtract_flags(cpu, 0u, source, value, byte_mode, false);
    } else if (family == 0xeau) {
        update_logic_flags(cpu, value, byte_mode);
    }
    return true;
}

static uint16_t table_operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                      uint8_t width) {
    uint16_t address = cpu->w[reg];
    if (mode == 2u || mode == 3u) {
        cpu->w[reg] = (uint16_t)(cpu->w[reg] + (mode == 3u ? width : -width));
    } else if (mode == 4u || mode == 5u) {
        cpu->w[reg] = (uint16_t)(cpu->w[reg] + (mode == 5u ? width : -width));
        address = cpu->w[reg];
    }
    return address;
}

static bool execute_table(Dspic33* cpu, uint32_t opcode) {
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
    if (write) {
        value = byte_mode ? read_operand_byte(cpu, source_mode, source_register, 0u)
                          : read_operand_word(cpu, source_mode, source_register, 0u);
        table_offset = table_operand_address(cpu, destination_mode,
                                             destination_register, byte_mode ? 1u : 2u);
    } else {
        table_offset = table_operand_address(cpu, source_mode, source_register,
                                             byte_mode ? 1u : 2u);
        value = 0u;
    }
    address = (((uint32_t)cpu->tblpag & 0x00ffu) << 16u) | table_offset;
    address &= 0x00fffffeu;
    if (address >= DSPIC33_PROGRAM_LIMIT) {
        word = 0x00ffffffu;
    } else {
        word = cpu->program[address / 2u];
    }
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
        if (address < DSPIC33_PROGRAM_LIMIT) {
            cpu->program[address / 2u] = word & 0x00ffffffu;
        }
        return true;
    }
    if (high) {
        value = byte_mode && (table_offset & 1u) != 0u
                    ? 0u
                    : (uint16_t)((word >> 16u) & 0xffu);
    } else if (byte_mode) {
        value = (uint16_t)((word >> ((table_offset & 1u) * 8u)) & 0xffu);
    } else {
        value = (uint16_t)word;
    }
    if (byte_mode) {
        return write_operand_byte(cpu, destination_mode, destination_register, 0u,
                                  (uint8_t)value);
    }
    return write_operand_word(cpu, destination_mode, destination_register, 0u, value);
}

static bool execute_literal_binary(Dspic33* cpu, uint32_t opcode) {
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
        update_add_flags(cpu, left, (uint16_t)(literal + (alternate ? carry : 0u)),
                         result, byte_mode, alternate);
    } else if (family == 1u) {
        uint16_t borrow = alternate && carry == 0u ? 1u : 0u;
        value = (uint16_t)(left - literal - borrow);
        update_subtract_flags(cpu, left, (uint16_t)(literal + borrow), value, byte_mode,
                              alternate);
    } else if (family == 2u) {
        value = alternate ? (uint16_t)(left ^ literal) : (uint16_t)(left & literal);
        update_logic_flags(cpu, value, byte_mode);
    } else if (!alternate) {
        value = (uint16_t)(left | literal);
        update_logic_flags(cpu, value, byte_mode);
    } else {
        return false;
    }
    if (byte_mode) {
        cpu->w[destination] =
            (uint16_t)((cpu->w[destination] & 0xff00u) | (uint8_t)value);
    } else {
        cpu->w[destination] = value;
    }
    return true;
}

static bool execute_file_binary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool file_destination = (opcode & 0x002000u) != 0u;
    uint16_t address = (uint16_t)(opcode & 0x1fffu);
    uint16_t left =
        byte_mode ? dspic33_read_byte(cpu, address) : dspic33_read_word(cpu, address);
    uint16_t right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
    uint16_t carry = (cpu->sr & 1u) != 0u ? 1u : 0u;
    uint16_t value;
    uint32_t result;
    if (family == 0u) {
        result = (uint32_t)left + right + (alternate ? carry : 0u);
        value = (uint16_t)result;
        update_add_flags(cpu, left, (uint16_t)(right + (alternate ? carry : 0u)),
                         result, byte_mode, alternate);
    } else if (family == 1u) {
        uint16_t borrow = alternate && carry == 0u ? 1u : 0u;
        value = (uint16_t)(left - right - borrow);
        update_subtract_flags(cpu, left, (uint16_t)(right + borrow), value, byte_mode,
                              alternate);
    } else if (family == 2u) {
        value = alternate ? (uint16_t)(left ^ right) : (uint16_t)(left & right);
        update_logic_flags(cpu, value, byte_mode);
    } else if (!alternate) {
        value = (uint16_t)(left | right);
        update_logic_flags(cpu, value, byte_mode);
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
        cpu->w[0] = (uint16_t)((cpu->w[0] & 0xff00u) | (uint8_t)value);
    } else {
        cpu->w[0] = value;
    }
    return true;
}

static void skip_instruction(Dspic33* cpu) {
    uint32_t next;
    if (cpu->pc >= DSPIC33_PROGRAM_LIMIT) {
        return;
    }
    next = cpu->program[cpu->pc / 2u];
    cpu->pc += ((next & 0xff0000u) == 0x020000u || (next & 0xff0000u) == 0x040000u ||
                (next & 0xff0000u) == 0x080000u)
                   ? 4u
                   : 2u;
}

static bool execute_bit(Dspic33* cpu, uint32_t opcode) {
    uint8_t kind = (uint8_t)((opcode >> 16u) & 0x07u);
    bool file = (opcode & 0x080000u) != 0u;
    uint8_t bit;
    uint16_t value;
    uint16_t mask;
    uint16_t address = 0u;
    uint8_t mode = 0u;
    uint8_t reg = 0u;
    bool byte_mode = false;
    if (file) {
        bit = (uint8_t)((((opcode >> 13u) & 0x07u) << 1u) | (opcode & 1u));
        address = (uint16_t)(((opcode >> 1u) & 0x0fffu) << 1u);
        value = dspic33_read_word(cpu, address);
    } else {
        mode = (uint8_t)((opcode >> 4u) & 0x07u);
        reg = (uint8_t)(opcode & 0x0fu);
        byte_mode = (opcode & 0x000400u) != 0u;
        if (kind == 5u) {
            bit = (uint8_t)(cpu->w[(opcode >> 11u) & 0x0fu] & 0x0fu);
        } else {
            bit = (uint8_t)((opcode >> 12u) & 0x0fu);
        }
        value = byte_mode ? read_operand_byte(cpu, mode, reg, 0u)
                          : read_operand_word(cpu, mode, reg, 0u);
    }
    mask = (uint16_t)(1u << bit);
    if (kind == 0u) {
        value |= mask;
    } else if (kind == 1u) {
        value &= (uint16_t)~mask;
    } else if (kind == 2u) {
        value ^= mask;
    } else if (kind == 3u || kind == 5u) {
        bool zero_destination = (opcode & 0x000800u) != 0u;
        if (zero_destination || file) {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0002u) |
                                 ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0001u) |
                                 ((value & mask) != 0u ? 0x0001u : 0u));
        }
        return true;
    } else if (kind == 4u) {
        bool zero_destination = (opcode & 0x000800u) != 0u;
        if (zero_destination || file) {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0002u) |
                                 ((value & mask) == 0u ? 0x0002u : 0u));
        } else {
            cpu->sr = (uint16_t)((cpu->sr & ~0x0001u) |
                                 ((value & mask) != 0u ? 0x0001u : 0u));
        }
        value |= mask;
    } else if (kind == 6u || kind == 7u) {
        bool set = (value & mask) != 0u;
        if ((kind == 6u && set) || (kind == 7u && !set)) {
            skip_instruction(cpu);
        }
        return true;
    }
    if (file) {
        dspic33_write_word(cpu, address, value);
    } else if (byte_mode) {
        return write_operand_byte(cpu, mode, reg, 0u, (uint8_t)value);
    } else {
        return write_operand_word(cpu, mode, reg, 0u, value);
    }
    return true;
}

static bool execute_file_unary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)(opcode >> 16u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool file_destination = (opcode & 0x002000u) != 0u;
    uint16_t address = (uint16_t)(opcode & 0x1fffu);
    uint16_t source =
        byte_mode ? dspic33_read_byte(cpu, address) : dspic33_read_word(cpu, address);
    uint16_t value;
    if (family == 0xecu) {
        value = (uint16_t)(source + (alternate ? 2u : 1u));
        update_add_flags(cpu, source, alternate ? 2u : 1u,
                         (uint32_t)source + (alternate ? 2u : 1u), byte_mode, false);
    } else if (family == 0xedu) {
        value = (uint16_t)(source - (alternate ? 2u : 1u));
        update_subtract_flags(cpu, source, alternate ? 2u : 1u, value, byte_mode,
                              false);
    } else if (family == 0xeeu) {
        value = alternate ? (uint16_t)~source : (uint16_t)(0u - source);
        if (alternate) {
            update_logic_flags(cpu, value, byte_mode);
        } else {
            update_subtract_flags(cpu, 0u, source, value, byte_mode, false);
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
        cpu->w[0] = (uint16_t)((cpu->w[0] & 0xff00u) | (uint8_t)value);
    } else {
        cpu->w[0] = value;
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

static void push_program_counter(Dspic33* cpu, uint32_t address) {
    write_word(cpu, cpu->w[15], (uint16_t)address);
    cpu->w[15] += 2u;
    write_word(cpu, cpu->w[15], (uint16_t)(address >> 16u));
    cpu->w[15] += 2u;
    cpu->call_depth++;
}

static uint32_t pop_program_counter(Dspic33* cpu) {
    uint32_t high;
    uint32_t low;
    cpu->w[15] -= 2u;
    high = read_word(cpu, cpu->w[15]) & 0x007fu;
    cpu->w[15] -= 2u;
    low = read_word(cpu, cpu->w[15]);
    cpu->call_depth--;
    return (high << 16u) | low;
}

static bool execute_shift(Dspic33* cpu, uint32_t opcode, bool left) {
    uint8_t source = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t amount;
    uint16_t value;

    if ((opcode & 0x0040u) == 0u) {
        return false;
    }
    amount = (uint8_t)(opcode & 0x0fu);
    value = left ? (uint16_t)(cpu->w[source] << amount)
                 : (uint16_t)(cpu->w[source] >> amount);
    cpu->w[destination] = value;
    update_logic_flags(cpu, value, false);
    return true;
}

static bool execute(Dspic33* cpu, uint32_t opcode) {
    if ((opcode & 0xff0000u) == 0x020000u || (opcode & 0xff0000u) == 0x040000u) {
        uint32_t second;
        uint32_t target;
        if (cpu->pc >= DSPIC33_PROGRAM_LIMIT) {
            return false;
        }
        second = cpu->program[cpu->pc / 2u];
        cpu->pc += 2u;
        target = ((second & 0x007fu) << 16u) | (opcode & 0x00ffffu);
        target &= 0x007ffffeu;
        if ((opcode & 0xff0000u) == 0x020000u) {
            push_program_counter(cpu, cpu->pc);
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010000u) {
        push_program_counter(cpu, cpu->pc);
        cpu->pc = cpu->w[opcode & 0x0fu] & 0xfffeu;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x012000u) {
        push_program_counter(cpu, cpu->pc);
        cpu->pc = (uint32_t)(cpu->pc + (int16_t)cpu->w[opcode & 0x0fu] * 2);
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x014000u || (opcode & 0xfffff0u) == 0x010400u) {
        cpu->pc = cpu->w[opcode & 0x0fu] & 0xfffeu;
        return true;
    }
    if ((opcode & 0xff8ff0u) == 0x018000u || (opcode & 0xff8ff0u) == 0x018400u) {
        uint8_t source = (uint8_t)(opcode & 0x0eu);
        uint32_t target =
            ((uint32_t)(cpu->w[source + 1u] & 0x007fu) << 16u) | cpu->w[source];
        if ((opcode & 0x000400u) == 0u) {
            push_program_counter(cpu, cpu->pc);
        }
        cpu->pc = target & 0x007ffffeu;
        return true;
    }
    if ((opcode & 0xff8000u) == 0x090000u) {
        cpu->rcount = (uint16_t)(opcode & 0x007fffu);
        if (cpu->rcount != 0u) {
            cpu->repeat_active = 1u;
            cpu->repeat_pc = cpu->pc;
            cpu->sr |= 0x0010u;
        }
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x098000u) {
        cpu->rcount = cpu->w[opcode & 0x0fu];
        if (cpu->rcount != 0u) {
            cpu->repeat_active = 1u;
            cpu->repeat_pc = cpu->pc;
            cpu->sr |= 0x0010u;
        }
        return true;
    }
    if ((opcode & 0xf00000u) == 0x200000u) {
        return execute_move_literal(cpu, opcode);
    }
    if ((opcode & 0xfff000u) == 0xb3c000u) {
        uint8_t destination = (uint8_t)(opcode & 0x0fu);
        uint8_t literal = (uint8_t)((opcode >> 4u) & 0xffu);
        cpu->w[destination] = (uint16_t)((cpu->w[destination] & 0xff00u) | literal);
        return true;
    }
    if ((opcode & 0xfc0000u) == 0xb00000u) {
        return execute_literal_binary(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0x780000u) {
        return execute_move(cpu, opcode);
    }
    if ((opcode & 0xf00000u) == 0x900000u) {
        return execute_move_offset(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xbe0000u) {
        return execute_move_double(cpu, opcode);
    }
    if ((opcode & 0xfe0000u) == 0xba0000u) {
        return execute_table(cpu, opcode);
    }
    if ((opcode & 0xf00000u) == 0xa00000u) {
        return execute_bit(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0x800000u) {
        uint16_t address = (uint16_t)(((opcode >> 4u) & 0x7fffu) << 1u);
        cpu->w[opcode & 0x0fu] = read_word(cpu, address);
        return true;
    }
    if ((opcode & 0xf80000u) == 0x880000u) {
        uint16_t address = (uint16_t)(((opcode >> 4u) & 0x7fffu) << 1u);
        dspic33_write_word(cpu, address, cpu->w[opcode & 0x0fu]);
        return true;
    }
    if ((opcode & 0xffa000u) == 0xb7a000u) {
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        if ((opcode & 0x004000u) != 0u) {
            dspic33_write_byte(cpu, address, (uint8_t)cpu->w[0]);
        } else {
            dspic33_write_word(cpu, address, cpu->w[0]);
        }
        return true;
    }
    if ((opcode & 0xfc0000u) == 0xb40000u) {
        return execute_file_binary(cpu, opcode);
    }
    if ((opcode & 0xff8000u) == 0xbf8000u) {
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        uint16_t value = byte_mode ? dspic33_read_byte(cpu, address)
                                   : dspic33_read_word(cpu, address);
        if ((opcode & 0x002000u) == 0u) {
            if (byte_mode) {
                cpu->w[0] = (uint16_t)((cpu->w[0] & 0xff00u) | value);
            } else {
                cpu->w[0] = value;
            }
        }
        update_logic_flags(cpu, value, byte_mode);
        return true;
    }
    if ((opcode & 0xfff800u) == 0xfb8000u) {
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        cpu->w[destination] = (uint8_t)cpu->w[opcode & 0x0fu];
        update_logic_flags(cpu, cpu->w[destination], false);
        return true;
    }
    if ((opcode & 0xff0000u) == 0xdd0000u) {
        return execute_shift(cpu, opcode, true);
    }
    if ((opcode & 0xff0000u) == 0xde0000u) {
        return execute_shift(cpu, opcode, false);
    }
    if ((opcode & 0xf80000u) == 0x100000u || (opcode & 0xf80000u) == 0x180000u ||
        (opcode & 0xf80000u) == 0x400000u || (opcode & 0xf80000u) == 0x480000u ||
        (opcode & 0xf80000u) == 0x500000u || (opcode & 0xf80000u) == 0x580000u ||
        (opcode & 0xf80000u) == 0x600000u || (opcode & 0xf80000u) == 0x680000u ||
        (opcode & 0xf80000u) == 0x700000u) {
        return execute_binary(cpu, opcode, opcode & 0xf80000u);
    }
    if ((opcode & 0xfe0000u) == 0xe00000u) {
        return execute_compare(cpu, opcode);
    }
    if ((opcode & 0xfc0000u) == 0xe80000u) {
        return execute_unary(cpu, opcode);
    }
    if ((opcode & 0xfc0000u) == 0xec0000u) {
        return execute_file_unary(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0x070000u) {
        int32_t displacement = (int16_t)(opcode & 0xffffu);
        push_program_counter(cpu, cpu->pc);
        cpu->pc = (uint32_t)(cpu->pc + displacement * 2);
        return true;
    }
    if ((opcode & 0xf00000u) == 0x300000u) {
        bool take;
        if (!branch_condition(cpu, (uint8_t)((opcode >> 16u) & 0x0fu), &take)) {
            return false;
        }
        if (take) {
            int32_t displacement = (int16_t)(opcode & 0xffffu);
            cpu->pc = (uint32_t)(cpu->pc + displacement * 2);
        }
        return true;
    }
    if ((opcode & 0xff0000u) == 0x050000u) {
        uint16_t literal = (uint16_t)((opcode >> 4u) & 0x03ffu);
        uint8_t destination = (uint8_t)(opcode & 0x0fu);
        if ((opcode & 0x004000u) != 0u) {
            cpu->w[destination] =
                (uint16_t)((cpu->w[destination] & 0xff00u) | (uint8_t)literal);
        } else {
            cpu->w[destination] = literal;
        }
        cpu->pc = pop_program_counter(cpu);
        return true;
    }
    if (opcode == 0xfe0000u) {
        dspic33_reset(cpu, 0u);
        return true;
    }
    if (opcode == 0x000000u || opcode == 0x00075au) {
        return true;
    }
    return false;
}

bool dspic33_initialize(Dspic33* cpu) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->program = calloc(DSPIC33_PROGRAM_WORDS, sizeof(*cpu->program));
    cpu->data = calloc(DSPIC33_DATA_SIZE, sizeof(*cpu->data));
    if (cpu->program == NULL || cpu->data == NULL) {
        dspic33_destroy(cpu);
        return false;
    }
    memset(cpu->program, 0xff, DSPIC33_PROGRAM_WORDS * sizeof(*cpu->program));
    return true;
}

void dspic33_destroy(Dspic33* cpu) {
    free(cpu->program);
    free(cpu->data);
    free(cpu->events.items);
    cpu->program = NULL;
    cpu->data = NULL;
    cpu->events.items = NULL;
    cpu->events.count = 0u;
    cpu->events.capacity = 0u;
}

void dspic33_reset(Dspic33* cpu, uint32_t entry) {
    memset(cpu->data, 0, DSPIC33_DATA_SIZE);
    memset(cpu->w, 0, sizeof(cpu->w));
    memset(cpu->accumulator, 0, sizeof(cpu->accumulator));
    cpu->w[15] = 0x1000u;
    cpu->pc = entry;
    cpu->sr = 0u;
    cpu->corcon = 0x0020u;
    cpu->splim = 0u;
    cpu->rcount = 0u;
    cpu->dcount = 0u;
    cpu->dostart = 0u;
    cpu->doend = 0u;
    cpu->tblpag = 0u;
    cpu->dsrpag = 0u;
    cpu->dswpag = 0u;
    cpu->disicnt = 0u;
    cpu->call_depth = 0u;
    cpu->interrupt_depth = 0u;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->repeat_pc = 0u;
    memset(cpu->do_start, 0, sizeof(cpu->do_start));
    memset(cpu->do_end, 0, sizeof(cpu->do_end));
    memset(cpu->do_count, 0, sizeof(cpu->do_count));
    cpu->instructions = 0u;
    cpu->cycles = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->events.count = 0u;
    cpu->events.sequence = 0u;
    cpu->stop_reason = DSPIC33_RUNNING;
    dspic33_device_reset(cpu);
}

bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word) {
    if ((address & 1u) != 0u || address >= DSPIC33_PROGRAM_LIMIT) {
        return false;
    }
    cpu->program[address / 2u] = word & 0x00ffffffu;
    return true;
}

void dspic33_write_byte(Dspic33* cpu, uint32_t address, uint8_t value) {
    if (address >= DSPIC33_DATA_SIZE) {
        return;
    }
    if (address < 32u) {
        uint8_t reg = (uint8_t)(address / 2u);
        if ((address & 1u) == 0u) {
            cpu->w[reg] = (uint16_t)((cpu->w[reg] & 0xff00u) | value);
        } else {
            cpu->w[reg] = (uint16_t)((cpu->w[reg] & 0x00ffu) | ((uint16_t)value << 8u));
        }
        return;
    }
    if (address >= 0x0020u && address <= 0x0055u) {
        uint16_t* reg = NULL;
        uint16_t word_address = (uint16_t)(address & 0xfffeu);
        switch (word_address) {
        case 0x0020u:
            reg = &cpu->splim;
            break;
        case 0x0032u:
            reg = &cpu->dsrpag;
            break;
        case 0x0034u:
            reg = &cpu->dswpag;
            break;
        case 0x0036u:
            reg = &cpu->rcount;
            break;
        case 0x0038u:
            reg = &cpu->dcount;
            break;
        case 0x0042u:
            reg = &cpu->sr;
            break;
        case 0x0044u:
            reg = &cpu->corcon;
            break;
        case 0x0052u:
            reg = &cpu->disicnt;
            break;
        case 0x0054u:
            reg = &cpu->tblpag;
            break;
        default:
            break;
        }
        if (reg != NULL) {
            if ((address & 1u) == 0u) {
                *reg = (uint16_t)((*reg & 0xff00u) | value);
            } else {
                *reg = (uint16_t)((*reg & 0x00ffu) | ((uint16_t)value << 8u));
            }
            return;
        }
    }
    cpu->data[address] = value;
    if (address < 0x10000u) {
        dspic33_device_write_byte(cpu, (uint16_t)address);
    }
}

void dspic33_write_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    if (address + 1u >= DSPIC33_DATA_SIZE) {
        return;
    }
    if (address < 32u || (address >= 0x0020u && address <= 0x0054u)) {
        dspic33_write_byte(cpu, address, (uint8_t)value);
        dspic33_write_byte(cpu, address + 1u, (uint8_t)(value >> 8u));
        return;
    }
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    if (address < 0xffffu) {
        dspic33_device_write_byte(cpu, (uint16_t)(address + 1u));
    }
}

uint8_t dspic33_read_byte(Dspic33* cpu, uint32_t address) {
    uint16_t value;
    if (address >= DSPIC33_DATA_SIZE) {
        return 0u;
    }
    if (address < 32u) {
        value = cpu->w[address / 2u];
        return (uint8_t)(value >> ((address & 1u) * 8u));
    }
    switch (address & 0xfffeu) {
    case 0x0020u:
        value = cpu->splim;
        break;
    case 0x0032u:
        value = cpu->dsrpag;
        break;
    case 0x0034u:
        value = cpu->dswpag;
        break;
    case 0x0036u:
        value = cpu->rcount;
        break;
    case 0x0038u:
        value = cpu->dcount;
        break;
    case 0x0042u:
        value = cpu->sr;
        break;
    case 0x0044u:
        value = cpu->corcon;
        break;
    case 0x0052u:
        value = cpu->disicnt;
        break;
    case 0x0054u:
        value = cpu->tblpag;
        break;
    default:
        if (address < 0x10000u) {
            return dspic33_device_read_byte(cpu, (uint16_t)address, cpu->data[address]);
        }
        return cpu->data[address];
    }
    return (uint8_t)(value >> ((address & 1u) * 8u));
}

uint16_t dspic33_read_word(Dspic33* cpu, uint32_t address) {
    return (uint16_t)(dspic33_read_byte(cpu, address) |
                      ((uint16_t)dspic33_read_byte(cpu, address + 1u) << 8u));
}

Dspic33StopReason dspic33_run(Dspic33* cpu, uint64_t instruction_limit) {
    while (cpu->instructions < instruction_limit) {
        uint32_t opcode;
        uint32_t instruction_pc;
        dspic33_device_service_interrupt(cpu);
        if ((cpu->pc & 1u) != 0u || cpu->pc >= DSPIC33_PROGRAM_LIMIT) {
            cpu->stop_reason = DSPIC33_PROGRAM_BOUNDS;
            return cpu->stop_reason;
        }
        instruction_pc = cpu->pc;
        opcode = cpu->program[cpu->pc / 2u];
        if (opcode == 0x064000u) {
            dspic33_device_return_interrupt(cpu);
            cpu->instructions++;
            if (!dspic33_device_advance(cpu, 3u)) {
                return cpu->stop_reason;
            }
            continue;
        }
        if (opcode == 0x060000u) {
            if (cpu->call_depth == 0u) {
                cpu->stop_reason = DSPIC33_RETURNED;
                return cpu->stop_reason;
            }
            cpu->pc = pop_program_counter(cpu);
            cpu->instructions++;
            if (!dspic33_device_advance(cpu, 3u)) {
                return cpu->stop_reason;
            }
            continue;
        }
        cpu->pc += 2u;
        cpu->instructions++;
        if (!execute(cpu, opcode)) {
            cpu->pc -= 2u;
            cpu->unsupported_opcode = opcode;
            cpu->stop_reason = DSPIC33_UNSUPPORTED_INSTRUCTION;
            return cpu->stop_reason;
        }
        if (cpu->repeat_active != 0u && instruction_pc == cpu->repeat_pc) {
            if (cpu->rcount != 0u) {
                cpu->rcount--;
                cpu->pc = cpu->repeat_pc;
            } else {
                cpu->repeat_active = 0u;
                cpu->sr &= (uint16_t)~0x0010u;
            }
        }
        if (!dspic33_device_advance(cpu, 1u)) {
            return cpu->stop_reason;
        }
    }
    cpu->stop_reason = DSPIC33_INSTRUCTION_LIMIT;
    return cpu->stop_reason;
}

const char* dspic33_stop_reason_name(Dspic33StopReason reason) {
    switch (reason) {
    case DSPIC33_RUNNING:
        return "running";
    case DSPIC33_RETURNED:
        return "returned";
    case DSPIC33_HALTED:
        return "halted";
    case DSPIC33_UNSUPPORTED_INSTRUCTION:
        return "unsupported instruction";
    case DSPIC33_PROGRAM_BOUNDS:
        return "program bounds";
    case DSPIC33_INSTRUCTION_LIMIT:
        return "instruction limit";
    case DSPIC33_EVENT_QUEUE_ERROR:
        return "event queue error";
    }
    return "unknown";
}
