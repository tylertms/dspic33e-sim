#include "dspic33.h"

#include <stdlib.h>
#include <string.h>

#include "device.h"

enum {
    PSV_ADDRESS = 0x01000000u,
    PSV_HIGH_BYTE = 0x02000000u,
    PSV_ADDRESS_MASK = 0x007fffffu
};

static const uint64_t ACCUMULATOR_MASK = 0xffffffffffu;

static bool accumulator_byte_location(uint32_t address, uint8_t* accumulator,
                                      uint8_t* byte) {
    if (address >= 0x0022u && address <= 0x0027u) {
        *accumulator = 0u;
        *byte = (uint8_t)(address - 0x0022u);
        return true;
    }
    if (address >= 0x0028u && address <= 0x002du) {
        *accumulator = 1u;
        *byte = (uint8_t)(address - 0x0028u);
        return true;
    }
    return false;
}

static int64_t accumulator_value(uint64_t bits) {
    bits &= ACCUMULATOR_MASK;
    return (int64_t)bits - ((bits & 0x8000000000u) != 0u ? 0x10000000000ll : 0ll);
}

static uint8_t read_accumulator_byte(const Dspic33* cpu, uint8_t accumulator,
                                     uint8_t byte) {
    if (byte == 5u) {
        return ((uint64_t)cpu->accumulator[accumulator] & 0x8000000000u) != 0u ? 0xffu
                                                                               : 0u;
    }
    return (uint8_t)((uint64_t)cpu->accumulator[accumulator] >> (byte * 8u));
}

static void write_accumulator_byte(Dspic33* cpu, uint8_t accumulator, uint8_t byte,
                                   uint8_t value) {
    uint64_t bits;
    uint64_t mask;
    if (byte >= 5u) {
        return;
    }
    bits = (uint64_t)cpu->accumulator[accumulator] & ACCUMULATOR_MASK;
    mask = (uint64_t)0xffu << (byte * 8u);
    bits = (bits & ~mask) | ((uint64_t)value << (byte * 8u));
    cpu->accumulator[accumulator] = accumulator_value(bits);
}

static uint32_t read_program_word(const Dspic33* cpu, uint32_t address) {
    if (address < DSPIC33_PROGRAM_LIMIT) {
        return cpu->program[address / 2u];
    }
    if (address >= DSPIC33_PERSISTENT_PROGRAM_BASE &&
        address < DSPIC33_PERSISTENT_PROGRAM_LIMIT) {
        return cpu
            ->persistent_program[(address - DSPIC33_PERSISTENT_PROGRAM_BASE) / 2u];
    }
    if (address >= DSPIC33_WRITE_LATCH_BASE && address < DSPIC33_WRITE_LATCH_LIMIT) {
        return cpu->write_latches[(address - DSPIC33_WRITE_LATCH_BASE) / 2u];
    }
    return 0x00ffffffu;
}

static uint32_t* writable_program_word(Dspic33* cpu, uint32_t address) {
    if (address < DSPIC33_PROGRAM_LIMIT) {
        return &cpu->program[address / 2u];
    }
    if (address >= DSPIC33_PERSISTENT_PROGRAM_BASE &&
        address < DSPIC33_PERSISTENT_PROGRAM_LIMIT) {
        return &cpu->persistent_program[(address - DSPIC33_PERSISTENT_PROGRAM_BASE) /
                                        2u];
    }
    return NULL;
}

static uint16_t read_word(Dspic33* cpu, uint32_t address) {
    return dspic33_read_word(cpu, address);
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
        if (!write && page >= 0x0200u) {
            *address = PSV_ADDRESS | ((page & 0x0100u) != 0u ? PSV_HIGH_BYTE : 0u) |
                       ((uint32_t)(page & 0x00ffu) << 15u) | (*address & 0x7fffu);
        } else {
            *address = ((uint32_t)page << 15u) | (*address & 0x7fffu);
        }
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
        if (!write && page >= 0x0200u) {
            address = PSV_ADDRESS | ((page & 0x0100u) != 0u ? PSV_HIGH_BYTE : 0u) |
                      ((uint32_t)(page & 0x00ffu) << 15u) | (address & 0x7fffu);
        } else {
            address = ((uint32_t)page << 15u) | (address & 0x7fffu);
        }
    }
    return address;
}

static uint32_t direct_move_address(const Dspic33* cpu, uint16_t address, bool write) {
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
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t low;
    uint16_t high;
    uint32_t address;
    if (source_mode == 0u) {
        source_register &= 0x0eu;
        low = cpu->w[source_register];
        high = cpu->w[source_register + 1u];
    } else {
        if (!operand_address(cpu, source_mode, source_register, 0u, 4u, false,
                             &address)) {
            return false;
        }
        low = read_word(cpu, address);
        high = read_word(cpu, address + 2u);
    }
    if (destination_mode == 0u) {
        destination_register &= 0x0eu;
        cpu->w[destination_register] = low;
        cpu->w[destination_register + 1u] = high;
        return true;
    }
    if (!operand_address(cpu, destination_mode, destination_register, 0u, 4u, true,
                         &address)) {
        return false;
    }
    write_word(cpu, address, low);
    write_word(cpu, address + 2u, high);
    return true;
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
                                  uint16_t borrow, uint16_t value, bool byte_mode,
                                  bool sticky_zero) {
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
    if ((left & digit_mask) >= (uint32_t)(right & digit_mask) + borrow) {
        cpu->sr |= 0x0100u;
    }
    if ((((left ^ operand) & (left ^ value)) & sign) != 0u) {
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
    uint16_t borrow = 0u;
    uint32_t subtraction_right;

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
        update_subtract_flags(cpu, left, right, borrow, value, byte_mode, with_carry);
    } else {
        update_logic_flags(cpu, value, byte_mode);
    }
    return true;
}

static bool execute_compare(Dspic33* cpu, uint32_t opcode) {
    bool byte_mode;
    bool with_borrow;
    uint16_t borrow;
    uint16_t left;
    uint16_t right;
    uint16_t value;
    if ((opcode & 0xff0000u) == 0xe00000u) {
        uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
        byte_mode = (opcode & 0x000400u) != 0u;
        left = byte_mode ? read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                         : read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
        right = 0u;
        with_borrow = false;
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
        with_borrow = (opcode & 0x008000u) != 0u;
    } else if ((opcode & 0xff8000u) == 0xe20000u) {
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        byte_mode = (opcode & 0x004000u) != 0u;
        left = byte_mode ? dspic33_read_byte(cpu, address)
                         : dspic33_read_word(cpu, address);
        right = 0u;
        with_borrow = false;
    } else {
        return false;
    }
    borrow = with_borrow && (cpu->sr & 1u) == 0u ? 1u : 0u;
    value = (uint16_t)(left - right - borrow);
    update_subtract_flags(cpu, left, right, borrow, value, byte_mode, with_borrow);
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
        update_subtract_flags(cpu, source, alternate ? 2u : 1u, 0u, value, byte_mode,
                              false);
    } else if (family == 0xeau && arithmetic) {
        update_subtract_flags(cpu, 0u, source, 0u, value, byte_mode, false);
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
    address = ((((uint32_t)cpu->tblpag & 0x01ffu) << 16u) | table_offset) & 0x01fffffeu;
    word = read_program_word(cpu, address);
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
        if (address >= DSPIC33_WRITE_LATCH_BASE &&
            address < DSPIC33_WRITE_LATCH_LIMIT) {
            cpu->write_latches[(address - DSPIC33_WRITE_LATCH_BASE) / 2u] =
                word & 0x00ffffffu;
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
        update_subtract_flags(cpu, left, literal, borrow, value, byte_mode, alternate);
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
        update_subtract_flags(cpu, left, right, borrow, value, byte_mode, alternate);
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
    bool indirect = false;
    uint8_t bit;
    uint16_t value;
    uint16_t mask;
    uint32_t address = 0u;
    uint8_t mode = 0u;
    uint8_t reg = 0u;
    bool byte_mode = false;
    if (file) {
        bit = (uint8_t)((opcode >> 13u) & 0x07u);
        address = (uint16_t)(opcode & 0x1fffu);
        value = dspic33_read_byte(cpu, address);
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
            if (!operand_address(cpu, mode, reg, 0u, byte_mode ? 1u : 2u, false,
                                 &address)) {
                return false;
            }
            indirect = true;
            value = byte_mode ? dspic33_read_byte(cpu, address)
                              : dspic33_read_word(cpu, address);
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
        dspic33_write_byte(cpu, address, (uint8_t)value);
    } else if (indirect) {
        if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)value);
        } else {
            dspic33_write_word(cpu, address, value);
        }
    } else if (byte_mode) {
        cpu->w[reg] = (uint16_t)((cpu->w[reg] & 0xff00u) | (uint8_t)value);
    } else {
        cpu->w[reg] = value;
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
        update_subtract_flags(cpu, source, alternate ? 2u : 1u, 0u, value, byte_mode,
                              false);
    } else if (family == 0xeeu) {
        value = alternate ? (uint16_t)~source : (uint16_t)(0u - source);
        if (alternate) {
            update_logic_flags(cpu, value, byte_mode);
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
    uint16_t low = (uint16_t)(address & 0xfffeu);
    low |= (uint16_t)((cpu->corcon >> 2u) & 1u);
    write_word(cpu, cpu->w[15], low);
    cpu->w[15] += 2u;
    write_word(cpu, cpu->w[15], (uint16_t)(address >> 16u));
    cpu->w[15] += 2u;
    cpu->corcon &= (uint16_t)~0x0004u;
    cpu->call_depth++;
}

static uint32_t pop_program_counter(Dspic33* cpu) {
    uint32_t high;
    uint32_t low;
    cpu->w[15] -= 2u;
    high = read_word(cpu, cpu->w[15]) & 0x007fu;
    cpu->w[15] -= 2u;
    low = read_word(cpu, cpu->w[15]);
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0004u) | ((low & 1u) << 2u));
    cpu->call_depth--;
    return (high << 16u) | (low & 0xfffeu);
}

static bool execute_shift(Dspic33* cpu, uint32_t opcode, bool left) {
    uint8_t source = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool arithmetic = !left && (opcode & 0x008000u) != 0u;
    uint16_t amount;
    uint16_t value;

    amount =
        (opcode & 0x0040u) != 0u ? (uint16_t)(opcode & 0x0fu) : cpu->w[opcode & 0x0fu];
    if (amount >= 16u) {
        value = arithmetic && (cpu->w[source] & 0x8000u) != 0u ? 0xffffu : 0u;
    } else if (left) {
        value = (uint16_t)(cpu->w[source] << amount);
    } else if (arithmetic) {
        value = (uint16_t)((int16_t)cpu->w[source] >> amount);
    } else {
        value = (uint16_t)(cpu->w[source] >> amount);
    }
    cpu->w[destination] = value;
    update_logic_flags(cpu, value, false);
    return true;
}

static bool execute_find_first_sign_change(Dspic33* cpu, uint32_t opcode) {
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t source = read_operand_word(cpu, source_mode, source_register, 0u);
    bool sign = (source & 0x8000u) != 0u;
    uint8_t shifts = 0u;

    source <<= 1u;
    while (shifts < 15u && ((source & 0x8000u) != 0u) == sign) {
        source <<= 1u;
        shifts++;
    }
    cpu->w[destination] = (uint16_t)(-(int16_t)shifts);
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

static bool execute_single_shift(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t source = byte_mode
                          ? read_operand_byte(cpu, source_mode, source_register, 0u)
                          : read_operand_word(cpu, source_mode, source_register, 0u);
    uint16_t next_carry;
    bool carry_affected;
    uint16_t value = shift_single_bit(cpu, source, family, alternate, byte_mode,
                                      &next_carry, &carry_affected);
    update_logic_flags(cpu, value, byte_mode);
    if (carry_affected) {
        cpu->sr = (uint16_t)((cpu->sr & ~1u) | next_carry);
    }
    if (byte_mode) {
        return write_operand_byte(cpu, destination_mode, destination_register, 0u,
                                  (uint8_t)value);
    }
    return write_operand_word(cpu, destination_mode, destination_register, 0u, value);
}

static bool execute_file_shift(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool byte_mode = (opcode & 0x004000u) != 0u;
    bool file_destination = (opcode & 0x002000u) != 0u;
    uint16_t address = (uint16_t)(opcode & 0x1fffu);
    uint16_t source =
        byte_mode ? dspic33_read_byte(cpu, address) : dspic33_read_word(cpu, address);
    uint16_t next_carry;
    bool carry_affected;
    uint16_t value = shift_single_bit(cpu, source, family, alternate, byte_mode,
                                      &next_carry, &carry_affected);
    update_logic_flags(cpu, value, byte_mode);
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
        cpu->w[0] = (uint16_t)((cpu->w[0] & 0xff00u) | (uint8_t)value);
    } else {
        cpu->w[0] = value;
    }
    return true;
}

static bool execute_multiply(Dspic33* cpu, uint32_t opcode) {
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
        source = read_operand_word(cpu, source_mode, source_register, 0u);
    }
    left = base_signed ? (int16_t)cpu->w[base_register] : cpu->w[base_register];
    right = source_signed ? (int16_t)source : source;
    product = left * right;
    if (destination >= 14u) {
        uint8_t accumulator = (uint8_t)(destination & 1u);
        if ((cpu->corcon & 1u) == 0u) {
            product *= 2;
        }
        cpu->accumulator[accumulator] = accumulator_value((uint64_t)product);
        return true;
    }
    destination &= 0x0eu;
    cpu->w[destination] = (uint16_t)product;
    cpu->w[destination + 1u] = (uint16_t)((uint32_t)product >> 16u);
    return true;
}

static bool execute_find_first(Dspic33* cpu, uint32_t opcode) {
    bool left = (opcode & 0x008000u) != 0u;
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t source = read_operand_word(cpu, source_mode, source_register, 0u);
    uint16_t result = 0u;
    uint8_t bit;
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
    cpu->w[destination] = result;
    cpu->sr = (uint16_t)((cpu->sr & ~1u) | (result == 0u ? 1u : 0u));
    return true;
}

static void update_divide_flags(Dspic33* cpu, int64_t remainder, bool overflow) {
    cpu->sr &= (uint16_t)~0x000fu;
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

static bool execute_divide(Dspic33* cpu, uint32_t opcode) {
    bool unsigned_divide = (opcode & 0x008000u) != 0u;
    bool double_word = (opcode & 0x000040u) != 0u;
    uint8_t high_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t low_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t divisor_register = (uint8_t)(opcode & 0x0fu);
    uint16_t divisor = cpu->w[divisor_register];
    bool overflow = false;
    int64_t remainder;
    int64_t quotient;
    if (cpu->repeat_active != 0u && cpu->rcount != 0u) {
        return true;
    }
    if (divisor == 0u) {
        cpu->stop_reason = DSPIC33_HALTED;
        return false;
    }
    if (unsigned_divide) {
        uint32_t dividend = double_word ? ((uint32_t)cpu->w[high_register] << 16u) |
                                              cpu->w[low_register]
                                        : cpu->w[low_register];
        uint32_t unsigned_quotient = dividend / divisor;
        uint32_t unsigned_remainder = dividend % divisor;
        overflow = unsigned_quotient > UINT16_MAX;
        quotient = unsigned_quotient;
        remainder = unsigned_remainder;
    } else {
        int32_t dividend = double_word
                               ? (int32_t)(((uint32_t)cpu->w[high_register] << 16u) |
                                           cpu->w[low_register])
                               : (int16_t)cpu->w[low_register];
        int16_t signed_divisor = (int16_t)divisor;
        if (signed_divisor == 0) {
            cpu->stop_reason = DSPIC33_HALTED;
            return false;
        }
        quotient = (int64_t)dividend / signed_divisor;
        remainder = (int64_t)dividend % signed_divisor;
        overflow = quotient < INT16_MIN || quotient > INT16_MAX;
    }
    cpu->w[0] = (uint16_t)quotient;
    cpu->w[1] = (uint16_t)remainder;
    update_divide_flags(cpu, remainder, overflow);
    return true;
}

static bool execute_fractional_divide(Dspic33* cpu, uint32_t opcode) {
    uint8_t dividend_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t divisor_register = (uint8_t)(opcode & 0x0fu);
    int16_t divisor = (int16_t)cpu->w[divisor_register];
    int32_t dividend;
    int32_t quotient;
    int32_t remainder;
    bool overflow;
    if (cpu->repeat_active != 0u && cpu->rcount != 0u) {
        return true;
    }
    if (divisor == 0) {
        cpu->stop_reason = DSPIC33_HALTED;
        return false;
    }
    dividend = (int32_t)(int16_t)cpu->w[dividend_register] * 32768;
    quotient = dividend / divisor;
    remainder = dividend % divisor;
    overflow = quotient < INT16_MIN || quotient > INT16_MAX;
    cpu->w[0] = (uint16_t)quotient;
    cpu->w[1] = (uint16_t)remainder;
    update_divide_flags(cpu, remainder, overflow);
    return true;
}

static bool execute(Dspic33* cpu, uint32_t opcode) {
    if ((opcode & 0xfff870u) == 0xfd0000u) {
        uint8_t source = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint16_t value = cpu->w[source];
        cpu->w[source] = cpu->w[destination];
        cpu->w[destination] = value;
        return true;
    }
    if (opcode == 0xfa8000u) {
        cpu->w[15] = cpu->w[14];
        cpu->w[15] -= 2u;
        cpu->w[14] = read_word(cpu, cpu->w[15]);
        return true;
    }
    if ((opcode & 0xffe000u) == 0xf80000u) {
        write_word(cpu, cpu->w[15], read_word(cpu, opcode & 0x1fffu));
        cpu->w[15] += 2u;
        return true;
    }
    if ((opcode & 0xffe000u) == 0xf90000u) {
        cpu->w[15] -= 2u;
        write_word(cpu, opcode & 0x1fffu, read_word(cpu, cpu->w[15]));
        return true;
    }
    if ((opcode & 0xff8001u) == 0xfa0000u) {
        write_word(cpu, cpu->w[15], cpu->w[14]);
        cpu->w[15] += 2u;
        cpu->w[14] = cpu->w[15];
        cpu->w[15] = (uint16_t)(cpu->w[15] + (opcode & 0x007ffeu));
        return true;
    }
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
    if ((opcode & 0xfffff0u) == 0x010600u) {
        cpu->pc = (uint32_t)(cpu->pc + (int32_t)(int16_t)cpu->w[opcode & 0x0fu] * 2);
        return true;
    }
    if ((opcode & 0xff87f0u) == 0x018000u || (opcode & 0xff87f0u) == 0x018400u) {
        uint8_t source = (uint8_t)(opcode & 0x0eu);
        uint32_t target =
            ((uint32_t)(cpu->w[source + 1u] & 0x007fu) << 16u) | cpu->w[source];
        if ((opcode & 0x000400u) == 0u) {
            push_program_counter(cpu, cpu->pc);
        }
        cpu->pc = target & 0x007ffffeu;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x088000u || (opcode & 0xff8000u) == 0x080000u) {
        uint16_t count;
        int16_t displacement;
        uint8_t depth;
        if (cpu->pc >= DSPIC33_PROGRAM_LIMIT || cpu->do_depth == 4u) {
            return false;
        }
        count = (opcode & 0xfffff0u) == 0x088000u ? cpu->w[opcode & 0x0fu]
                                                  : (uint16_t)(opcode & 0x7fffu);
        displacement = (int16_t)(cpu->program[cpu->pc / 2u] & 0xffffu);
        cpu->pc += 2u;
        depth = cpu->do_depth++;
        cpu->do_start[depth] = cpu->pc;
        cpu->do_end[depth] = (uint32_t)(cpu->pc + (int32_t)displacement * 2);
        cpu->do_count[depth] = count;
        cpu->do_terminate[depth] = 0u;
        cpu->dostart = cpu->do_start[depth];
        cpu->doend = cpu->do_end[depth];
        cpu->dcount = count;
        cpu->corcon =
            (uint16_t)((cpu->corcon & ~0x0700u) | ((uint16_t)cpu->do_depth << 8u));
        cpu->sr |= 0x0200u;
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
    if ((opcode & 0xffc000u) == 0xfc0000u) {
        cpu->disicnt = (uint16_t)((opcode & 0x003fffu) + 1u);
        return true;
    }
    if ((opcode & 0xfff000u) == 0xfec000u) {
        uint16_t value = (uint16_t)(opcode & 0x03ffu);
        switch ((opcode >> 10u) & 3u) {
        case 0u:
            cpu->dsrpag = value;
            break;
        case 1u:
            cpu->dswpag = value & 0x01ffu;
            break;
        case 3u:
            cpu->tblpag = value & 0x01ffu;
            break;
        default:
            return false;
        }
        return true;
    }
    if ((opcode & 0xfff3f0u) == 0xfed000u) {
        uint16_t value = cpu->w[opcode & 0x0fu];
        switch ((opcode >> 10u) & 3u) {
        case 0u:
            cpu->dsrpag = value & 0x03ffu;
            break;
        case 1u:
            cpu->dswpag = value & 0x01ffu;
            break;
        case 3u:
            cpu->tblpag = value & 0x01ffu;
            break;
        default:
            return false;
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
    if ((opcode & 0xf80000u) == 0x780000u) {
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
        cpu->w[opcode & 0x0fu] =
            read_word(cpu, direct_move_address(cpu, address, false));
        return true;
    }
    if ((opcode & 0xf80000u) == 0x880000u) {
        uint16_t address = (uint16_t)(((opcode >> 4u) & 0x7fffu) << 1u);
        dspic33_write_word(cpu, direct_move_address(cpu, address, true),
                           cpu->w[opcode & 0x0fu]);
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
    if ((opcode & 0xff0000u) == 0xfb0000u) {
        bool zero_extend = (opcode & 0x008000u) != 0u;
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint8_t source = read_operand_byte(cpu, source_mode, source_register, 0u);
        cpu->w[destination] = zero_extend ? source : (uint16_t)(int16_t)(int8_t)source;
        update_logic_flags(cpu, cpu->w[destination], false);
        cpu->sr = (uint16_t)((cpu->sr & ~0x0001u) |
                             ((cpu->sr & 0x0008u) == 0u ? 0x0001u : 0u));
        return true;
    }
    if ((opcode & 0xfc0000u) == 0xd40000u) {
        return execute_file_shift(cpu, opcode);
    }
    if ((opcode & 0xfc0000u) == 0xd00000u) {
        return execute_single_shift(cpu, opcode);
    }
    if ((opcode & 0xfe0000u) == 0xb80000u) {
        return execute_multiply(cpu, opcode);
    }
    if ((opcode & 0xffa000u) == 0xbc0000u) {
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        if ((opcode & 0x004000u) != 0u) {
            cpu->w[2] =
                (uint16_t)((uint8_t)cpu->w[0] * dspic33_read_byte(cpu, address));
        } else {
            uint32_t product = (uint32_t)cpu->w[0] * dspic33_read_word(cpu, address);
            cpu->w[2] = (uint16_t)product;
            cpu->w[3] = (uint16_t)(product >> 16u);
        }
        return true;
    }
    if ((opcode & 0xff0000u) == 0xcf0000u) {
        return execute_find_first(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xd80000u) {
        return execute_divide(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xd90000u) {
        return execute_fractional_divide(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xdd0000u) {
        return execute_shift(cpu, opcode, true);
    }
    if ((opcode & 0xff0000u) == 0xde0000u) {
        return execute_shift(cpu, opcode, false);
    }
    if ((opcode & 0xff0000u) == 0xdf0000u) {
        return execute_find_first_sign_change(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0x100000u || (opcode & 0xf80000u) == 0x180000u ||
        (opcode & 0xf80000u) == 0x400000u || (opcode & 0xf80000u) == 0x480000u ||
        (opcode & 0xf80000u) == 0x500000u || (opcode & 0xf80000u) == 0x580000u ||
        (opcode & 0xf80000u) == 0x600000u || (opcode & 0xf80000u) == 0x680000u ||
        (opcode & 0xf80000u) == 0x700000u) {
        return execute_binary(cpu, opcode, opcode & 0xf80000u);
    }
    if ((opcode & 0xfc0000u) == 0xe00000u) {
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
        uint32_t target = (uint32_t)(cpu->pc + displacement * 2);
        push_program_counter(cpu, cpu->pc);
        cpu->pc = target;
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
        uint64_t software_reset_count = cpu->software_reset_count + 1u;
        uint16_t reset_interrupt = cpu->last_interrupt;
        dspic33_reset(cpu, 0u);
        cpu->software_reset_count = software_reset_count;
        cpu->reset_interrupt = reset_interrupt;
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
    cpu->persistent_program =
        calloc(DSPIC33_PERSISTENT_PROGRAM_WORDS, sizeof(*cpu->persistent_program));
    cpu->data = calloc(DSPIC33_DATA_SIZE, sizeof(*cpu->data));
    if (cpu->program == NULL || cpu->persistent_program == NULL || cpu->data == NULL) {
        dspic33_destroy(cpu);
        return false;
    }
    memset(cpu->program, 0xff, DSPIC33_PROGRAM_WORDS * sizeof(*cpu->program));
    memset(cpu->persistent_program, 0xff,
           DSPIC33_PERSISTENT_PROGRAM_WORDS * sizeof(*cpu->persistent_program));
    memset(cpu->write_latches, 0xff, sizeof(cpu->write_latches));
    memset(cpu->configuration, 0xff, sizeof(cpu->configuration));
    return true;
}

void dspic33_destroy(Dspic33* cpu) {
    free(cpu->program);
    free(cpu->persistent_program);
    free(cpu->data);
    free(cpu->events.items);
    cpu->program = NULL;
    cpu->persistent_program = NULL;
    cpu->data = NULL;
    cpu->events.items = NULL;
    cpu->events.count = 0u;
    cpu->events.capacity = 0u;
}

bool dspic33_copy(Dspic33* destination, const Dspic33* source) {
    Dspic33Event* events = destination->events.items;
    size_t event_capacity = destination->events.capacity;
    uint32_t* program = destination->program;
    uint32_t* persistent_program = destination->persistent_program;
    uint8_t* data = destination->data;
    if (event_capacity < source->events.count) {
        Dspic33Event* resized =
            realloc(events, source->events.count * sizeof(*source->events.items));
        if (resized == NULL) {
            return false;
        }
        events = resized;
        event_capacity = source->events.count;
    }
    memcpy(program, source->program, DSPIC33_PROGRAM_WORDS * sizeof(*program));
    memcpy(persistent_program, source->persistent_program,
           DSPIC33_PERSISTENT_PROGRAM_WORDS * sizeof(*persistent_program));
    memcpy(data, source->data, DSPIC33_DATA_SIZE);
    if (source->events.count != 0u) {
        memcpy(events, source->events.items,
               source->events.count * sizeof(*source->events.items));
    }
    *destination = *source;
    destination->program = program;
    destination->persistent_program = persistent_program;
    destination->data = data;
    destination->events.items = events;
    destination->events.capacity = event_capacity;
    return true;
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
    cpu->dsrpag = 1u;
    cpu->dswpag = 1u;
    cpu->disicnt = 0u;
    cpu->call_depth = 0u;
    cpu->interrupt_depth = 0u;
    cpu->repeat_active = 0u;
    cpu->do_depth = 0u;
    cpu->repeat_pc = 0u;
    memset(cpu->do_start, 0, sizeof(cpu->do_start));
    memset(cpu->do_end, 0, sizeof(cpu->do_end));
    memset(cpu->do_count, 0, sizeof(cpu->do_count));
    memset(cpu->do_terminate, 0, sizeof(cpu->do_terminate));
    cpu->instructions = 0u;
    cpu->cycles = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_interrupt = UINT16_MAX;
    cpu->last_interrupt_return = 0u;
    cpu->interrupt_count = 0u;
    cpu->software_reset_count = 0u;
    cpu->reset_interrupt = UINT16_MAX;
    memset(cpu->interrupt_log_irq, 0xff, sizeof(cpu->interrupt_log_irq));
    memset(cpu->interrupt_log_entry, 0, sizeof(cpu->interrupt_log_entry));
    memset(cpu->interrupt_log_return, 0, sizeof(cpu->interrupt_log_return));
    cpu->events.count = 0u;
    cpu->events.sequence = 0u;
    memset(cpu->write_latches, 0xff, sizeof(cpu->write_latches));
    cpu->stop_reason = DSPIC33_RUNNING;
    dspic33_device_reset(cpu);
}

bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word) {
    uint32_t* destination;
    if ((address & 1u) != 0u) {
        return false;
    }
    destination = writable_program_word(cpu, address);
    if (destination == NULL) {
        return false;
    }
    *destination = word & 0x00ffffffu;
    return true;
}

void dspic33_complete_nvm(Dspic33* cpu) {
    uint16_t control = dspic33_read_word(cpu, 0x0728u);
    uint32_t target = (((uint32_t)dspic33_read_word(cpu, 0x072cu) & 0x01ffu) << 16u) |
                      dspic33_read_word(cpu, 0x072au);
    uint32_t count;
    uint32_t index;
    if ((control & 0x000fu) == 1u) {
        target &= 0x01fffffcu;
        count = 2u;
    } else if ((control & 0x000fu) == 2u) {
        target &= 0x01ffff00u;
        count = DSPIC33_WRITE_LATCH_WORDS;
    } else if ((control & 0x000fu) == 3u) {
        target &= 0x01fff800u;
        for (index = 0u; index < 0x400u; index++) {
            uint32_t* destination = writable_program_word(cpu, target + index * 2u);
            if (destination != NULL) {
                *destination = 0x00ffffffu;
            }
        }
        return;
    } else {
        return;
    }
    for (index = 0u; index < count; index++) {
        uint32_t* destination = writable_program_word(cpu, target + index * 2u);
        if (destination != NULL) {
            *destination &= cpu->write_latches[index];
        }
    }
}

bool dspic33_load_configuration_word(Dspic33* cpu, uint32_t address, uint32_t word) {
    uint32_t offset;
    if (address < DSPIC33_CONFIGURATION_BASE ||
        address + 1u >= DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE) {
        return false;
    }
    offset = address - DSPIC33_CONFIGURATION_BASE;
    cpu->configuration[offset] = (uint8_t)word;
    cpu->configuration[offset + 1u] = (uint8_t)(word >> 8u);
    return true;
}

uint8_t dspic33_read_program_byte(const Dspic33* cpu, uint32_t address) {
    uint32_t word_address = address & ~1u;
    uint32_t word;
    word = read_program_word(cpu, word_address);
    return (uint8_t)(word >> ((address & 1u) * 8u));
}

uint8_t dspic33_read_configuration_byte(const Dspic33* cpu, uint32_t address) {
    if (address < DSPIC33_CONFIGURATION_BASE ||
        address >= DSPIC33_CONFIGURATION_BASE + DSPIC33_CONFIGURATION_SIZE) {
        return 0xffu;
    }
    return cpu->configuration[address - DSPIC33_CONFIGURATION_BASE];
}

void dspic33_write_byte(Dspic33* cpu, uint32_t address, uint8_t value) {
    uint16_t previous;
    uint8_t accumulator;
    uint8_t accumulator_byte;
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
    if (accumulator_byte_location(address, &accumulator, &accumulator_byte)) {
        write_accumulator_byte(cpu, accumulator, accumulator_byte, value);
        return;
    }
    if (address >= 0x0020u && address <= 0x0055u) {
        uint16_t* reg = NULL;
        uint16_t word_address = (uint16_t)(address & 0xfffeu);
        if (word_address == 0x0044u) {
            if ((address & 1u) == 0u) {
                uint8_t low = (uint8_t)cpu->corcon;
                low =
                    (uint8_t)((low & 0x04u) | (value & 0xf3u) | (low & value & 0x08u));
                cpu->corcon = (uint16_t)((cpu->corcon & 0xff00u) | low);
            } else {
                uint8_t high = (uint8_t)(cpu->corcon >> 8u);
                if ((value & 0x08u) != 0u && cpu->do_depth != 0u) {
                    cpu->do_terminate[cpu->do_depth - 1u] = 1u;
                }
                high = (uint8_t)((high & 0x07u) | (value & 0xb0u));
                cpu->corcon =
                    (uint16_t)((cpu->corcon & 0x00ffu) | ((uint16_t)high << 8u));
            }
            return;
        }
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
    previous = (uint16_t)(cpu->data[address & ~1u] |
                          ((uint16_t)cpu->data[address | 1u] << 8u));
    cpu->data[address] = value;
    if (address < 0x10000u) {
        dspic33_device_write_byte(cpu, (uint16_t)address, previous);
    }
}

void dspic33_write_word(Dspic33* cpu, uint32_t address, uint16_t value) {
    uint16_t previous;
    if (address + 1u >= DSPIC33_DATA_SIZE) {
        return;
    }
    if (address < 32u || (address >= 0x0020u && address <= 0x0054u)) {
        dspic33_write_byte(cpu, address, (uint8_t)value);
        dspic33_write_byte(cpu, address + 1u, (uint8_t)(value >> 8u));
        return;
    }
    previous =
        (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    cpu->data[address] = (uint8_t)value;
    cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    if (address < 0xffffu) {
        dspic33_device_write_byte(cpu, (uint16_t)(address + 1u), previous);
    }
}

uint8_t dspic33_read_byte(Dspic33* cpu, uint32_t address) {
    uint16_t value;
    uint8_t accumulator;
    uint8_t accumulator_byte;
    if ((address & PSV_ADDRESS) != 0u) {
        uint32_t program_address = address & PSV_ADDRESS_MASK;
        uint32_t word;
        if (program_address >= DSPIC33_PROGRAM_LIMIT) {
            return 0u;
        }
        word = cpu->program[(program_address & ~1u) / 2u];
        if ((address & PSV_HIGH_BYTE) != 0u) {
            return (program_address & 1u) == 0u ? (uint8_t)(word >> 16u) : 0u;
        }
        return (uint8_t)(word >> ((program_address & 1u) * 8u));
    }
    if (address >= DSPIC33_DATA_SIZE) {
        return 0u;
    }
    if (address < 32u) {
        value = cpu->w[address / 2u];
        return (uint8_t)(value >> ((address & 1u) * 8u));
    }
    if (accumulator_byte_location(address, &accumulator, &accumulator_byte)) {
        return read_accumulator_byte(cpu, accumulator, accumulator_byte);
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
    case 0x003au:
        value = (uint16_t)cpu->dostart;
        break;
    case 0x003cu:
        value = (uint16_t)(cpu->dostart >> 16u);
        break;
    case 0x003eu:
        value = (uint16_t)cpu->doend;
        break;
    case 0x0040u:
        value = (uint16_t)(cpu->doend >> 16u);
        break;
    case 0x0042u:
        value = cpu->sr;
        break;
    case 0x0044u:
        value = (uint16_t)(cpu->corcon & ~0x0800u);
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

Dspic33StopReason dspic33_step(Dspic33* cpu) {
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
        dspic33_device_advance(cpu, 3u);
        return cpu->stop_reason;
    }
    if (opcode == 0x060000u) {
        if (cpu->call_depth == 0u) {
            cpu->stop_reason = DSPIC33_RETURNED;
            return cpu->stop_reason;
        }
        cpu->pc = pop_program_counter(cpu);
        cpu->instructions++;
        dspic33_device_advance(cpu, 3u);
        return cpu->stop_reason;
    }
    cpu->pc += 2u;
    cpu->instructions++;
    if (!execute(cpu, opcode)) {
        cpu->pc -= 2u;
        if (cpu->stop_reason == DSPIC33_RUNNING) {
            cpu->unsupported_opcode = opcode;
            cpu->stop_reason = DSPIC33_UNSUPPORTED_INSTRUCTION;
        }
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
    if (cpu->do_depth != 0u && instruction_pc == cpu->do_end[cpu->do_depth - 1u]) {
        uint8_t depth = (uint8_t)(cpu->do_depth - 1u);
        if (cpu->do_count[depth] != 0u && cpu->do_terminate[depth] == 0u) {
            cpu->do_count[depth]--;
            cpu->dcount = cpu->do_count[depth];
            cpu->pc = cpu->do_start[depth];
        } else {
            cpu->do_terminate[depth] = 0u;
            cpu->do_depth--;
            cpu->corcon =
                (uint16_t)((cpu->corcon & ~0x0700u) | ((uint16_t)cpu->do_depth << 8u));
            if (cpu->do_depth == 0u) {
                cpu->dostart = 0u;
                cpu->doend = 0u;
                cpu->dcount = 0u;
                cpu->sr &= (uint16_t)~0x0200u;
            } else {
                depth = (uint8_t)(cpu->do_depth - 1u);
                cpu->dostart = cpu->do_start[depth];
                cpu->doend = cpu->do_end[depth];
                cpu->dcount = cpu->do_count[depth];
            }
        }
    }
    dspic33_device_advance(cpu, 1u);
    return cpu->stop_reason;
}

static Dspic33StopReason run(Dspic33* cpu, uint64_t instruction_limit,
                             uint32_t stop_address, bool stop_enabled) {
    uint64_t start = cpu->instructions;
    cpu->stop_reason = DSPIC33_RUNNING;
    while (instruction_limit == 0u || cpu->instructions - start < instruction_limit) {
        if (stop_enabled && cpu->pc == stop_address) {
            cpu->stop_reason = DSPIC33_STOPPED;
            return cpu->stop_reason;
        }
        if (dspic33_step(cpu) != DSPIC33_RUNNING) {
            return cpu->stop_reason;
        }
    }
    cpu->stop_reason = DSPIC33_INSTRUCTION_LIMIT;
    return cpu->stop_reason;
}

Dspic33StopReason dspic33_run(Dspic33* cpu, uint64_t instruction_limit) {
    return run(cpu, instruction_limit, 0u, false);
}

Dspic33StopReason dspic33_run_until(Dspic33* cpu, uint32_t stop_address,
                                    uint64_t instruction_limit) {
    return run(cpu, instruction_limit, stop_address, true);
}

const char* dspic33_stop_reason_name(Dspic33StopReason reason) {
    switch (reason) {
    case DSPIC33_RUNNING:
        return "running";
    case DSPIC33_RETURNED:
        return "returned";
    case DSPIC33_STOPPED:
        return "stop point";
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
