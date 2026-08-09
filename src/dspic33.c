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

static void reset_processor(Dspic33* cpu, uint32_t entry, bool clear_memory);
static void enter_trap(Dspic33* cpu, uint16_t trap, uint32_t vector, uint8_t priority,
                       uint16_t status, uint32_t return_pc);
static void schedule_soft_trap(Dspic33* cpu, uint16_t trap, uint32_t vector,
                               uint8_t priority, uint8_t delay);
static void check_stack_address(Dspic33* cpu, int32_t address, bool wrapped);

static bool check_data_alignment(Dspic33* cpu, uint32_t address) {
    if (!cpu->instruction_active || (address & 1u) == 0u) {
        return true;
    }
    if (!cpu->address_error) {
        cpu->address_error = true;
        cpu->address_error_return = cpu->pc;
    }
    cpu->address_error_access_allowed = false;
    return false;
}

static bool check_data_implementation(Dspic33* cpu, uint32_t address, uint8_t width) {
    bool first_page = address < 0x10000u && (address & PSV_ADDRESS) == 0u;
    bool implemented = !first_page || address + width <= 0xe000u;
    if (!cpu->instruction_active || cpu->io.dma_transfer_active || implemented) {
        return true;
    }
    if (!cpu->address_error) {
        cpu->address_error = true;
        cpu->address_error_return = cpu->pc;
    }
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = true;
    return false;
}

static void raise_data_page_error(Dspic33* cpu) {
    if (!cpu->address_error) {
        cpu->address_error = true;
        cpu->address_error_return = cpu->pc;
        cpu->address_error_access_allowed = true;
    }
    cpu->address_error_working_state_completed = true;
}

static void write_working_register(Dspic33* cpu, uint8_t reg, uint16_t value) {
    cpu->w[reg] = reg == 15u ? (uint16_t)(value & 0xfffeu) : value;
}

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

static void update_accumulator_combined_status(Dspic33* cpu) {
    cpu->sr &= (uint16_t)~0x0c00u;
    if ((cpu->sr & 0xc000u) != 0u) {
        cpu->sr |= 0x0800u;
    }
    if ((cpu->sr & 0x3000u) != 0u) {
        cpu->sr |= 0x0400u;
    }
}

static void apply_accumulator_result(Dspic33* cpu, uint8_t accumulator,
                                     int64_t result) {
    uint16_t overflow_flag = accumulator == 0u ? 0x8000u : 0x4000u;
    uint16_t saturation_flag = accumulator == 0u ? 0x2000u : 0x1000u;
    uint16_t saturation_enable = accumulator == 0u ? 0x0080u : 0x0040u;
    int64_t minimum = (cpu->corcon & 0x0010u) != 0u ? -0x8000000000ll : INT32_MIN;
    int64_t maximum = (cpu->corcon & 0x0010u) != 0u ? 0x7fffffffffll : INT32_MAX;
    bool accumulator_overflow = result < -0x8000000000ll || result > 0x7fffffffffll;
    bool saturation_status = accumulator_overflow;

    if ((cpu->corcon & saturation_enable) != 0u) {
        if (result < minimum) {
            result = minimum;
            saturation_status = true;
        } else if (result > maximum) {
            result = maximum;
            saturation_status = true;
        }
    }

    cpu->accumulator[accumulator] = accumulator_value((uint64_t)result);
    bool overflow = cpu->accumulator[accumulator] < INT32_MIN ||
                    cpu->accumulator[accumulator] > INT32_MAX;
    cpu->sr &= (uint16_t)~overflow_flag;
    if (overflow) {
        cpu->sr |= overflow_flag;
    }
    if (saturation_status) {
        cpu->sr |= saturation_flag;
    }
    update_accumulator_combined_status(cpu);
}

static void clear_accumulator_status(Dspic33* cpu, uint8_t accumulator) {
    cpu->sr &= accumulator == 0u ? (uint16_t)~0xa000u : (uint16_t)~0x5000u;
    update_accumulator_combined_status(cpu);
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

static uint16_t stored_word(const Dspic33* cpu, uint16_t address) {
    return (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
}

static bool modulo_addressing_enabled(const Dspic33* cpu, uint8_t reg, bool y_space) {
    uint16_t modcon = stored_word(cpu, 0x0046u);
    uint16_t enable = y_space ? 0x4000u : 0x8000u;
    uint8_t selector = (uint8_t)((modcon >> (y_space ? 4u : 0u)) & 0x0fu);
    uint16_t start = stored_word(cpu, y_space ? 0x004cu : 0x0048u);
    uint16_t end = stored_word(cpu, y_space ? 0x004eu : 0x004au);
    return (modcon & enable) != 0u && selector != 15u && selector == reg &&
           end >= start;
}

static uint16_t modulo_address(const Dspic33* cpu, uint8_t reg, int32_t address,
                               int32_t delta, bool y_space) {
    uint16_t start = stored_word(cpu, y_space ? 0x004cu : 0x0048u);
    uint16_t end = stored_word(cpu, y_space ? 0x004eu : 0x004au);
    uint32_t length = (uint32_t)end - start + 1u;

    if (!modulo_addressing_enabled(cpu, reg, y_space) || delta == 0) {
        return (uint16_t)address;
    }
    if (delta > 0 && address > end && (uint32_t)(address - end) <= length) {
        return (uint16_t)(start + address - end - 1);
    }
    if (delta < 0 && address < start && (uint32_t)(start - address) <= length) {
        return (uint16_t)(end - (start - address) + 1);
    }
    return (uint16_t)address;
}

static bool bit_reversed_addressing_enabled(const Dspic33* cpu, uint8_t mode,
                                            uint8_t reg, uint8_t width, bool write) {
    uint16_t modcon = stored_word(cpu, 0x0046u);
    uint16_t xbrev = stored_word(cpu, 0x0050u);
    uint8_t selector = (uint8_t)((modcon >> 8u) & 0x0fu);
    return write && width == 2u && (mode == 3u || mode == 5u) &&
           (xbrev & 0x8000u) != 0u && selector != 15u && selector == reg;
}

static uint16_t bit_reversed_address(const Dspic33* cpu, uint16_t address) {
    uint16_t modifier = (uint16_t)(stored_word(cpu, 0x0050u) << 1u);
    uint16_t result = (uint16_t)(address & 1u);
    uint8_t carry = 0u;
    uint8_t bit;
    for (bit = 15u; bit != 0u; bit--) {
        uint16_t mask = (uint16_t)(1u << bit);
        uint8_t sum = (uint8_t)(((address & mask) != 0u ? 1u : 0u) +
                                ((modifier & mask) != 0u ? 1u : 0u) + carry);
        if ((sum & 1u) != 0u) {
            result |= mask;
        }
        carry = (uint8_t)(sum >> 1u);
    }
    return result;
}

typedef struct {
    uint32_t address;
    int32_t effective_address;
    uint16_t updated_register;
    bool wrapped;
    bool updates_register;
    bool increments_data_page;
    bool unimplemented_data_page;
} OperandResolution;

static bool resolve_operand_address(const Dspic33* cpu, const uint16_t* registers,
                                    uint8_t mode, uint8_t reg, uint8_t offset_reg,
                                    uint8_t width, bool write,
                                    OperandResolution* resolution) {
    int32_t delta = 0;
    int32_t effective_address;
    int32_t adjusted_address = 0;
    bool bit_reversed = bit_reversed_addressing_enabled(cpu, mode, reg, width, write);
    memset(resolution, 0, sizeof(*resolution));
    if (mode == 0u) {
        return false;
    }
    if (mode == 1u) {
        effective_address = registers[reg];
        resolution->address = (uint16_t)effective_address;
    } else if (mode == 2u || mode == 3u) {
        effective_address = registers[reg];
        resolution->address = (uint16_t)effective_address;
        delta = mode == 3u ? width : -(int32_t)width;
        adjusted_address = (int32_t)registers[reg] + delta;
        resolution->wrapped = adjusted_address < 0 || adjusted_address > UINT16_MAX;
        resolution->updated_register =
            bit_reversed ? bit_reversed_address(cpu, registers[reg])
                         : modulo_address(cpu, reg, adjusted_address, delta, false);
        resolution->updates_register = true;
        if (mode == 3u && reg < 14u && !bit_reversed &&
            !modulo_addressing_enabled(cpu, reg, false) && registers[reg] >= 0x8000u &&
            adjusted_address > UINT16_MAX && resolution->updated_register == 0u) {
            resolution->updated_register = 0x8000u;
            resolution->increments_data_page = true;
        }
    } else if (mode == 4u || mode == 5u) {
        delta = mode == 5u ? width : -(int32_t)width;
        adjusted_address = (int32_t)registers[reg] + delta;
        resolution->wrapped = adjusted_address < 0 || adjusted_address > UINT16_MAX;
        effective_address = bit_reversed ? registers[reg] : adjusted_address;
        resolution->updated_register =
            bit_reversed ? bit_reversed_address(cpu, registers[reg])
                         : modulo_address(cpu, reg, adjusted_address, delta, false);
        resolution->updates_register = true;
        resolution->address = bit_reversed ? (uint16_t)effective_address
                              : reg == 15u ? (uint16_t)adjusted_address
                                           : resolution->updated_register;
    } else {
        delta = (int16_t)registers[offset_reg];
        effective_address = (int32_t)registers[reg] + delta;
        resolution->wrapped = effective_address < 0 || effective_address > UINT16_MAX;
        resolution->address = modulo_address(cpu, reg, effective_address, delta, false);
    }
    resolution->effective_address = effective_address;
    if (resolution->address >= 0x8000u &&
        !((reg == 14u || reg == 15u) && (cpu->corcon & 0x0004u) != 0u)) {
        uint16_t page = write ? cpu->dswpag : cpu->dsrpag;
        resolution->unimplemented_data_page = page == 0u;
        if (!write && page >= 0x0200u) {
            resolution->address =
                PSV_ADDRESS | ((page & 0x0100u) != 0u ? PSV_HIGH_BYTE : 0u) |
                ((uint32_t)(page & 0x00ffu) << 15u) | (resolution->address & 0x7fffu);
        } else {
            resolution->address =
                ((uint32_t)page << 15u) | (resolution->address & 0x7fffu);
        }
    }
    return true;
}

static bool operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg, uint8_t offset_reg,
                            uint8_t width, bool write, uint32_t* address) {
    OperandResolution resolution;
    bool uses_stack_pointer;
    if (!resolve_operand_address(cpu, cpu->w, mode, reg, offset_reg, width, write,
                                 &resolution)) {
        return false;
    }
    uses_stack_pointer = reg == 15u || (mode >= 6u && offset_reg == 15u);
    if (uses_stack_pointer) {
        check_stack_address(cpu, resolution.effective_address, resolution.wrapped);
    }
    if (resolution.increments_data_page) {
        if (write) {
            cpu->dswpag = (uint16_t)((cpu->dswpag + 1u) & 0x01ffu);
        } else {
            cpu->dsrpag = (uint16_t)((cpu->dsrpag + 1u) & 0x03ffu);
        }
    }
    if (resolution.updates_register) {
        write_working_register(cpu, reg, resolution.updated_register);
    }
    if (resolution.unimplemented_data_page) {
        raise_data_page_error(cpu);
    }
    *address = resolution.address;
    return true;
}

static bool validate_operand_alignment(Dspic33* cpu, uint16_t* registers, uint8_t mode,
                                       uint8_t reg, uint8_t offset_reg, uint8_t width,
                                       bool write, bool indirect_bit) {
    OperandResolution resolution;
    if (mode == 0u) {
        return true;
    }
    if (!resolve_operand_address(cpu, registers, mode, reg, offset_reg, width, write,
                                 &resolution)) {
        return false;
    }
    if ((width != 1u || indirect_bit) &&
        !check_data_alignment(cpu, resolution.address)) {
        return false;
    }
    if (resolution.updates_register) {
        registers[reg] = reg == 15u ? (uint16_t)(resolution.updated_register & 0xfffeu)
                                    : resolution.updated_register;
    }
    return true;
}

static bool validate_destination_after_source_execution(Dspic33* cpu, uint8_t mode,
                                                        uint8_t reg, uint8_t width) {
    uint16_t registers[16];
    if (cpu->address_error && !cpu->address_error_access_allowed) {
        return false;
    }
    memcpy(registers, cpu->w, sizeof(registers));
    if (validate_operand_alignment(cpu, registers, mode, reg, 0u, width, true, false)) {
        return true;
    }
    cpu->address_error_working_state_completed = true;
    return false;
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
        write_working_register(cpu, reg, (uint16_t)((cpu->w[reg] & 0xff00u) | value));
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
        write_working_register(cpu, reg, value);
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
    write_working_register(cpu, destination, literal);
    return true;
}

static bool execute_move(Dspic33* cpu, uint32_t opcode) {
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t offset_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint16_t registers[16];

    memcpy(registers, cpu->w, sizeof(registers));
    if (!validate_operand_alignment(cpu, registers, source_mode, source_register,
                                    offset_register, byte_mode ? 1u : 2u, false,
                                    false) ||
        !validate_operand_alignment(cpu, registers, destination_mode,
                                    destination_register, offset_register,
                                    byte_mode ? 1u : 2u, true, false)) {
        return true;
    }

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
    int32_t delta = offset;
    int32_t effective_address = (int32_t)cpu->w[reg] + delta;
    uint32_t address = modulo_address(cpu, reg, effective_address, delta, false);
    if (reg == 15u) {
        check_stack_address(cpu, effective_address,
                            effective_address < 0 || effective_address > UINT16_MAX);
    }
    if (address >= 0x8000u &&
        !((reg == 14u || reg == 15u) && (cpu->corcon & 0x0004u) != 0u)) {
        uint16_t page = write ? cpu->dswpag : cpu->dsrpag;
        if (page == 0u) {
            raise_data_page_error(cpu);
        }
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
            write_working_register(cpu, destination,
                                   (uint16_t)((cpu->w[destination] & 0xff00u) |
                                              dspic33_read_byte(cpu, address)));
        } else {
            write_working_register(cpu, destination, dspic33_read_word(cpu, address));
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
    uint32_t high_address;
    uint16_t registers[16];
    memcpy(registers, cpu->w, sizeof(registers));
    if (!validate_operand_alignment(cpu, registers, source_mode, source_register, 0u,
                                    4u, false, false) ||
        !validate_operand_alignment(cpu, registers, destination_mode,
                                    destination_register, 0u, 4u, true, false)) {
        return true;
    }
    if (source_mode == 0u) {
        source_register &= 0x0eu;
        low = cpu->w[source_register];
        high = cpu->w[source_register + 1u];
    } else {
        if (!operand_address(cpu, source_mode, source_register, 0u, 4u, false,
                             &address)) {
            return false;
        }
        high_address = source_register == 15u ? (uint16_t)(address + 2u) : address + 2u;
        low = read_word(cpu, address);
        high = read_word(cpu, high_address);
    }
    if (destination_mode == 0u) {
        destination_register &= 0x0eu;
        write_working_register(cpu, destination_register, low);
        write_working_register(cpu, (uint8_t)(destination_register + 1u), high);
        return true;
    }
    if (!operand_address(cpu, destination_mode, destination_register, 0u, 4u, true,
                         &address)) {
        return false;
    }
    high_address =
        destination_register == 15u ? (uint16_t)(address + 2u) : address + 2u;
    write_word(cpu, address, low);
    write_word(cpu, high_address, high);
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
                             uint16_t carry_in, uint32_t result, bool byte_mode,
                             bool sticky_zero) {
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

static void update_binary_flags(Dspic33* cpu, uint32_t operation, uint16_t left,
                                uint16_t right, uint16_t borrow, uint16_t carry,
                                uint32_t result, uint16_t value, bool byte_mode,
                                bool with_carry) {
    if (operation == 0x400000u || operation == 0x480000u) {
        update_add_flags(cpu, left, right, operation == 0x480000u ? carry : 0u, result,
                         byte_mode, operation == 0x480000u);
    } else if (operation == 0x500000u || operation == 0x580000u ||
               operation == 0x100000u || operation == 0x180000u) {
        update_subtract_flags(cpu, left, right, borrow, value, byte_mode, with_carry);
    } else {
        update_logic_flags(cpu, value, byte_mode);
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
    if (!validate_destination_after_source_execution(
            cpu, (uint8_t)((opcode >> 11u) & 0x07u), destination,
            byte_mode ? 1u : 2u)) {
        update_binary_flags(cpu, operation, left, right, borrow, carry, result, value,
                            byte_mode, with_carry);
        return true;
    }
    if (byte_mode) {
        if (!write_operand_byte(cpu, (uint8_t)((opcode >> 11u) & 0x07u), destination,
                                0u, (uint8_t)value)) {
            return false;
        }
    } else if (!write_operand_word(cpu, (uint8_t)((opcode >> 11u) & 0x07u), destination,
                                   0u, value)) {
        return false;
    }
    update_binary_flags(cpu, operation, left, right, borrow, carry, result, value,
                        byte_mode, with_carry);
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
            right = (uint16_t)(((opcode >> 2u) & 0x00e0u) | (opcode & 0x001fu));
        } else {
            uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
            right = byte_mode
                        ? read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                        : read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
        }
        with_borrow = (opcode & 0x008000u) != 0u;
    } else if ((opcode & 0xff0000u) == 0xe30000u) {
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        byte_mode = (opcode & 0x004000u) != 0u;
        left = byte_mode ? dspic33_read_byte(cpu, address)
                         : dspic33_read_word(cpu, address);
        right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
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

static void update_unary_flags(Dspic33* cpu, uint8_t family, bool alternate,
                               uint16_t source, uint16_t value, bool byte_mode,
                               bool arithmetic) {
    if (family == 0xe8u) {
        update_add_flags(cpu, source, alternate ? 2u : 1u, 0u,
                         (uint32_t)source + (alternate ? 2u : 1u), byte_mode, false);
    } else if (family == 0xe9u) {
        update_subtract_flags(cpu, source, alternate ? 2u : 1u, 0u, value, byte_mode,
                              false);
    } else if (family == 0xeau && arithmetic) {
        update_subtract_flags(cpu, 0u, source, 0u, value, byte_mode, false);
    } else if (family == 0xeau) {
        update_logic_flags(cpu, value, byte_mode);
    }
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
    if (!validate_destination_after_source_execution(
            cpu, destination_mode, destination_register, byte_mode ? 1u : 2u)) {
        update_unary_flags(cpu, family, alternate, source, value, byte_mode,
                           arithmetic);
        return true;
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

static uint16_t table_operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                      uint8_t width) {
    uint16_t address = cpu->w[reg];
    int32_t adjusted_address = cpu->w[reg];
    int32_t effective_address = address;
    bool wrapped = false;
    if (mode == 2u || mode == 3u) {
        adjusted_address += mode == 3u ? width : -(int32_t)width;
        wrapped = adjusted_address < 0 || adjusted_address > UINT16_MAX;
        write_working_register(
            cpu, reg,
            table_adjust_pointer(cpu->w[reg],
                                 mode == 3u ? (int16_t)width : -(int16_t)width));
    } else if (mode == 4u || mode == 5u) {
        adjusted_address += mode == 5u ? width : -(int32_t)width;
        wrapped = adjusted_address < 0 || adjusted_address > UINT16_MAX;
        write_working_register(
            cpu, reg,
            table_adjust_pointer(cpu->w[reg],
                                 mode == 5u ? (int16_t)width : -(int16_t)width));
        address = reg == 15u ? (uint16_t)adjusted_address : cpu->w[reg];
        effective_address = adjusted_address;
    }
    if (reg == 15u) {
        check_stack_address(cpu, effective_address, wrapped);
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
        if (cpu->address_error && !cpu->address_error_access_allowed) {
            return true;
        }
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
        update_add_flags(cpu, left, literal, alternate ? carry : 0u, result, byte_mode,
                         alternate);
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
        write_working_register(
            cpu, destination,
            (uint16_t)((cpu->w[destination] & 0xff00u) | (uint8_t)value));
    } else {
        write_working_register(cpu, destination, value);
    }
    return true;
}

static bool execute_file_binary(Dspic33* cpu, uint32_t opcode) {
    uint8_t family = (uint8_t)((opcode >> 16u) & 0x03u);
    bool alternate = (opcode & 0x008000u) != 0u;
    bool reverse_subtract = (opcode & 0xff0000u) == 0xbd0000u;
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
        update_add_flags(cpu, left, right, alternate ? carry : 0u, result, byte_mode,
                         alternate);
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
            uint16_t registers[16];
            memcpy(registers, cpu->w, sizeof(registers));
            if (!validate_operand_alignment(cpu, registers, mode, reg, 0u,
                                            byte_mode ? 1u : 2u, false, !byte_mode)) {
                return true;
            }
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
        write_working_register(cpu, reg,
                               (uint16_t)((cpu->w[reg] & 0xff00u) | (uint8_t)value));
    } else {
        write_working_register(cpu, reg, value);
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
        update_add_flags(cpu, source, alternate ? 2u : 1u, 0u,
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
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15], low);
    cpu->w[15] += 2u;
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15], (uint16_t)(address >> 16u));
    cpu->w[15] += 2u;
    cpu->corcon &= (uint16_t)~0x0004u;
    cpu->call_depth++;
}

static uint32_t pop_program_counter(Dspic33* cpu) {
    uint32_t high;
    uint32_t low;
    check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
    cpu->w[15] -= 2u;
    high = read_word(cpu, cpu->w[15]) & 0x007fu;
    check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
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
    write_working_register(cpu, destination, value);
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
    write_working_register(cpu, destination, (uint16_t)(-(int16_t)shifts));
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
    if (!validate_destination_after_source_execution(
            cpu, destination_mode, destination_register, byte_mode ? 1u : 2u)) {
        return true;
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
    bool word_result = (destination & 1u) != 0u;
    destination &= 0x0eu;
    write_working_register(cpu, destination, (uint16_t)product);
    if (!word_result) {
        write_working_register(cpu, (uint8_t)(destination + 1u),
                               (uint16_t)((uint32_t)product >> 16u));
    }
    return true;
}

static bool execute_accumulator_arithmetic(Dspic33* cpu, uint32_t opcode) {
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

    apply_accumulator_result(cpu, accumulator, result);
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

static bool execute_accumulator_shift(Dspic33* cpu, uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t encoded_amount = (uint8_t)(opcode & 0x003fu);
    int16_t amount =
        (opcode & 0x0040u) != 0u
            ? (int16_t)(encoded_amount >= 32u ? encoded_amount - 64u : encoded_amount)
            : (int16_t)cpu->w[opcode & 0x0fu];
    if (amount < -16 || amount > 16) {
        dspic33_write_word(cpu, 0x08c0u,
                           (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | 0x0090u));
        schedule_soft_trap(cpu, 4u, 0x00000cu, 11u, 2u);
        return true;
    }
    apply_accumulator_result(
        cpu, accumulator,
        shift_accumulator_value(cpu->accumulator[accumulator], amount));
    return true;
}

static bool execute_accumulator_word(Dspic33* cpu, uint32_t opcode, bool add) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t offset_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t encoded_shift = (uint8_t)((opcode >> 7u) & 0x0fu);
    int8_t shift = (int8_t)(encoded_shift >= 8u ? encoded_shift - 16u : encoded_shift);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    int64_t value = (int64_t)(int16_t)read_operand_word(
                        cpu, source_mode, source_register, offset_register)
                    << 16u;
    value = shift_accumulator_value(value, shift);
    if (add) {
        value += cpu->accumulator[accumulator];
    }
    apply_accumulator_result(cpu, accumulator, value);
    return true;
}

static uint16_t accumulator_store_value_with_rounding(const Dspic33* cpu, int64_t value,
                                                      bool rounded, bool conventional) {
    uint64_t bits = (uint64_t)value & ACCUMULATOR_MASK;
    uint16_t high = (uint16_t)(bits >> 16u);
    uint16_t low = (uint16_t)bits;
    if (rounded &&
        (conventional || low > 0x8000u || (low == 0x8000u && (high & 1u) != 0u))) {
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

static uint16_t accumulator_store_value(const Dspic33* cpu, int64_t value,
                                        bool rounded) {
    return accumulator_store_value_with_rounding(cpu, value, rounded,
                                                 (cpu->corcon & 0x0002u) != 0u);
}

static bool execute_accumulator_store(Dspic33* cpu, uint32_t opcode) {
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t offset_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t encoded_shift = (uint8_t)((opcode >> 7u) & 0x0fu);
    int8_t shift = (int8_t)(encoded_shift >= 8u ? encoded_shift - 16u : encoded_shift);
    uint8_t destination_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_register = (uint8_t)(opcode & 0x0fu);
    int64_t value = shift_accumulator_value(cpu->accumulator[accumulator], shift);
    return write_operand_word(
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

static int64_t dsp_multiply_operand(const Dspic33* cpu, uint8_t register_index,
                                    uint8_t sign_mode) {
    bool unsigned_operand =
        sign_mode == 1u || (sign_mode == 2u && (register_index & 1u) == 0u);
    return unsigned_operand ? cpu->w[register_index] : (int16_t)cpu->w[register_index];
}

static bool validate_dsp_prefetch_alignment(Dspic33* cpu, uint8_t operation,
                                            bool y_space) {
    uint8_t base_register;
    uint16_t address;
    if (operation == 4u) {
        return true;
    }
    base_register = (uint8_t)((y_space ? 10u : 8u) + (operation >= 8u ? 1u : 0u));
    address = cpu->w[base_register];
    if (operation == 12u) {
        int32_t delta = (int16_t)cpu->w[12];
        address = modulo_address(cpu, base_register, (int32_t)address + delta, delta,
                                 y_space);
    }
    return check_data_alignment(cpu, address);
}

static bool validate_dsp_alignments(Dspic33* cpu, uint8_t x_operation,
                                    uint8_t y_operation, bool memory_write_back) {
    return validate_dsp_prefetch_alignment(cpu, x_operation, false) &&
           validate_dsp_prefetch_alignment(cpu, y_operation, true) &&
           (!memory_write_back || check_data_alignment(cpu, cpu->w[13]));
}

static bool dsp_prefetch_value(Dspic33* cpu, uint8_t operation, bool y_space,
                               uint16_t* value) {
    static const int8_t updates[16] = {
        0, 2, 4, 6, 0, -6, -4, -2, 0, 2, 4, 6, 0, -6, -4, -2,
    };
    uint8_t base_register;
    int32_t delta;
    uint16_t address;
    if (operation == 4u) {
        return false;
    }
    base_register = (uint8_t)((y_space ? 10u : 8u) + (operation >= 8u ? 1u : 0u));
    address = cpu->w[base_register];
    if (operation == 12u) {
        delta = (int16_t)cpu->w[12];
        address = modulo_address(cpu, base_register, (int32_t)address + delta, delta,
                                 y_space);
    }
    *value = dspic33_read_word(cpu, address);
    delta = updates[operation];
    write_working_register(cpu, base_register,
                           modulo_address(cpu, base_register,
                                          (int32_t)cpu->w[base_register] + delta, delta,
                                          y_space));
    return true;
}

static void execute_dsp_prefetch(Dspic33* cpu, uint8_t operation, uint8_t destination,
                                 bool y_space) {
    uint16_t value;
    if (dsp_prefetch_value(cpu, operation, y_space, &value)) {
        write_working_register(cpu, destination, value);
    }
}

static void execute_dsp_write_back(Dspic33* cpu, uint8_t accumulator, uint8_t mode) {
    uint16_t value = accumulator_store_value_with_rounding(
        cpu, cpu->accumulator[accumulator ^ 1u], true, false);
    if (mode == 0u) {
        cpu->w[13] = value;
    } else if (mode == 1u) {
        dspic33_write_word(cpu, cpu->w[13], value);
        cpu->w[13] = (uint16_t)(cpu->w[13] + 2u);
    }
}

static bool execute_dsp_clear_or_move(Dspic33* cpu, uint32_t opcode) {
    bool clear = (opcode & 0xff0000u) == 0xc30000u;
    uint8_t accumulator = (uint8_t)((opcode >> 15u) & 1u);
    uint8_t write_back = (uint8_t)(opcode & 3u);
    uint8_t x_operation = (uint8_t)((opcode >> 6u) & 0x0fu);
    uint8_t y_operation = (uint8_t)((opcode >> 2u) & 0x0fu);
    if (write_back == 3u) {
        return false;
    }
    if (!validate_dsp_alignments(cpu, x_operation, y_operation, write_back == 1u)) {
        return true;
    }
    if (clear) {
        cpu->accumulator[accumulator] = 0;
        clear_accumulator_status(cpu, accumulator);
    }
    execute_dsp_prefetch(cpu, x_operation, (uint8_t)(4u + ((opcode >> 12u) & 3u)),
                         false);
    execute_dsp_prefetch(cpu, y_operation, (uint8_t)(4u + ((opcode >> 10u) & 3u)),
                         true);
    if (write_back < 2u) {
        execute_dsp_write_back(cpu, accumulator, write_back);
    }
    return true;
}

static bool execute_euclidean_distance(Dspic33* cpu, uint32_t opcode) {
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
    if (!dsp_prefetch_value(cpu, x_operation, false, &x_value) ||
        !dsp_prefetch_value(cpu, y_operation, true, &y_value)) {
        return false;
    }
    apply_accumulator_result(cpu, accumulator,
                             operation == 2u ? cpu->accumulator[accumulator] + product
                                             : product);
    write_working_register(cpu, destination, (uint16_t)(x_value - y_value));
    return true;
}

static bool execute_dsp_multiply(Dspic33* cpu, uint32_t opcode) {
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
    bool replace = square ? write_back == 1u : write_back == 3u;
    bool subtract = (opcode & 0x004000u) != 0u;
    if (!dsp_multiply_registers(opcode, &left_register, &right_register) ||
        sign_mode == 3u) {
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
    apply_accumulator_result(cpu, accumulator, result);
    execute_dsp_prefetch(cpu, x_operation, (uint8_t)(4u + ((opcode >> 12u) & 3u)),
                         false);
    execute_dsp_prefetch(cpu, y_operation, (uint8_t)(4u + ((opcode >> 10u) & 3u)),
                         true);
    if (!square && !replace && write_back < 2u) {
        execute_dsp_write_back(cpu, accumulator, write_back);
    }
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
    write_working_register(cpu, destination, result);
    cpu->sr = (uint16_t)((cpu->sr & ~1u) | (result == 0u ? 1u : 0u));
    return true;
}

static bool execute_decimal_adjust(Dspic33* cpu, uint32_t opcode) {
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
    write_working_register(cpu, destination,
                           (uint16_t)((original & 0xff00u) | (uint8_t)adjusted));
    cpu->sr =
        (uint16_t)((cpu->sr & ~0x0001u) | (carry || adjusted > 0x00ffu ? 1u : 0u));
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

static void enter_trap(Dspic33* cpu, uint16_t trap, uint32_t vector, uint8_t priority,
                       uint16_t status, uint32_t return_pc) {
    uint16_t stacked_high;
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15],
               (uint16_t)((return_pc & 0xfffeu) | ((cpu->corcon >> 2u) & 1u)));
    cpu->w[15] += 2u;
    stacked_high = (uint16_t)(((cpu->sr & 0x00ffu) << 8u) |
                              ((cpu->corcon & 0x0008u) != 0u ? 0x0080u : 0u) |
                              ((return_pc >> 16u) & 0x007fu));
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15], stacked_high);
    cpu->w[15] += 2u;
    cpu->corcon &= (uint16_t)~0x0004u;
    cpu->corcon = priority > 7u ? (uint16_t)(cpu->corcon | 0x0008u)
                                : (uint16_t)(cpu->corcon & ~0x0008u);
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | ((priority & 7u) << 5u));
    dspic33_write_word(cpu, 0x08c0u,
                       (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | status));
    cpu->pc = cpu->program[vector / 2u] & 0x007ffffeu;
    cpu->last_trap = trap;
    cpu->last_trap_return = return_pc;
    cpu->trap_count++;
    cpu->interrupt_depth++;
    dspic33_device_latch_interrupt(cpu, (uint8_t)trap, priority);
    cpu->repeat_active = 0u;
    cpu->rcount = 0u;
    cpu->sr &= (uint16_t)~0x0010u;
    if (cpu->stop_on_trap) {
        cpu->stop_reason = DSPIC33_TRAPPED;
    }
}

static void schedule_soft_trap(Dspic33* cpu, uint16_t trap, uint32_t vector,
                               uint8_t priority, uint8_t delay) {
    Dspic33PendingSoftTrap* available = NULL;
    size_t index;
    for (index = 0u; index < 4u; index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->trap == trap) {
            if (pending->delay > delay) {
                pending->delay = delay;
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
}

static void schedule_stack_error(Dspic33* cpu, uint8_t delay) {
    dspic33_write_word(cpu, 0x08c0u,
                       (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | 0x0004u));
    schedule_soft_trap(cpu, 3u, 0x00000au, 12u, delay);
}

void dspic33_check_stack_address(Dspic33* cpu, int32_t address, bool wrapped,
                                 uint8_t delay) {
    uint16_t effective_address = (uint16_t)address;
    bool underflow = address < 0x1000;
    bool overflow = cpu->splim_enabled && effective_address > cpu->splim;
    if (!underflow && !overflow && !(cpu->splim_enabled && wrapped)) {
        return;
    }
    schedule_stack_error(cpu, delay);
}

static void check_stack_address(Dspic33* cpu, int32_t address, bool wrapped) {
    dspic33_check_stack_address(cpu, address, wrapped, 2u);
}

static bool service_pending_soft_trap(Dspic33* cpu) {
    Dspic33PendingSoftTrap* selected = NULL;
    uint8_t current_priority;
    size_t index;
    current_priority = (uint8_t)(((cpu->corcon & 0x0008u) != 0u ? 8u : 0u) |
                                 ((cpu->sr >> 5u) & 0x07u));
    for (index = 0u; index < 4u; index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay == 0u &&
            pending->priority > current_priority &&
            (selected == NULL || pending->priority > selected->priority)) {
            selected = pending;
        }
    }
    if (selected != NULL) {
        uint16_t trap = selected->trap;
        uint32_t vector = selected->vector;
        uint8_t priority = selected->priority;
        selected->active = false;
        enter_trap(cpu, trap, vector, priority, 0u, cpu->pc);
        return true;
    }
    return false;
}

static bool soft_exception_pending(const Dspic33* cpu) {
    size_t index;
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active) {
            return true;
        }
    }
    return false;
}

static bool exception_pending(const Dspic33* cpu) {
    return soft_exception_pending(cpu) || dspic33_device_interrupt_pending(cpu);
}

static void advance_instruction(Dspic33* cpu, uint64_t cycles) {
    size_t index;
    dspic33_device_advance(cpu, cycles);
    for (index = 0u; index < 4u; index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay != 0u) {
            pending->delay =
                pending->delay > cycles ? (uint8_t)(pending->delay - cycles) : 0u;
        }
    }
    service_pending_soft_trap(cpu);
}

static uint64_t instruction_cycles(const Dspic33* cpu, uint32_t opcode) {
    if ((opcode & 0xff0000u) == 0x050000u) {
        return exception_pending(cpu) ? 5u : 6u;
    }
    if ((opcode & 0xff0000u) == 0xbe0000u) {
        return 2u;
    }
    if ((opcode & 0xff0000u) == 0x020000u || (opcode & 0xfffff0u) == 0x010000u ||
        (opcode & 0xfffff0u) == 0x010200u || (opcode & 0xff0000u) == 0x070000u ||
        (opcode & 0xff87f0u) == 0x018000u) {
        return 4u;
    }
    return 1u;
}

void dspic33_raise_dma_address_trap(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x08c4u,
                       (uint16_t)(dspic33_read_word(cpu, 0x08c4u) | 0x0020u));
    enter_trap(cpu, 6u, 0x000010u, 9u, 0u, cpu->pc);
}

void dspic33_raise_dma_collision_trap(Dspic33* cpu) {
    enter_trap(cpu, 5u, 0x00000eu, 10u, 0x0020u, cpu->pc);
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
    if (divisor == 0u) {
        enter_trap(cpu, 4u, 0x00000cu, 11u, 0x0050u, cpu->pc - 2u);
        return true;
    }
    if (cpu->repeat_active != 0u && cpu->rcount != 0u) {
        return true;
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
    if (divisor == 0) {
        enter_trap(cpu, 4u, 0x00000cu, 11u, 0x0050u, cpu->pc - 2u);
        return true;
    }
    if (cpu->repeat_active != 0u && cpu->rcount != 0u) {
        return true;
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
    if ((opcode & 0xfffff0u) == 0xfd4000u) {
        return execute_decimal_adjust(cpu, opcode);
    }
    if ((opcode & 0xfff870u) == 0xfd0000u) {
        uint8_t source = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint16_t source_value = cpu->w[source];
        uint16_t destination_value = cpu->w[destination];
        write_working_register(cpu, source, destination_value);
        write_working_register(cpu, destination, source_value);
        return true;
    }
    if (opcode == 0xfa8000u) {
        if ((cpu->corcon & 0x0004u) == 0u) {
            schedule_stack_error(cpu, 2u);
        }
        write_working_register(cpu, 15u, cpu->w[14]);
        check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
        cpu->w[15] -= 2u;
        write_working_register(cpu, 14u, read_word(cpu, cpu->w[15]));
        cpu->corcon &= (uint16_t)~0x0004u;
        return true;
    }
    if (opcode == 0xfea000u) {
        memcpy(cpu->shadow_w, cpu->w, sizeof(cpu->shadow_w));
        cpu->shadow_status = (uint16_t)(cpu->sr & 0x010fu);
        return true;
    }
    if (opcode == 0xfe8000u) {
        memcpy(cpu->w, cpu->shadow_w, sizeof(cpu->shadow_w));
        cpu->sr = (uint16_t)((cpu->sr & ~0x010fu) | cpu->shadow_status);
        return true;
    }
    if ((opcode & 0xffe000u) == 0xf80000u) {
        check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
        write_word(cpu, cpu->w[15], read_word(cpu, opcode & 0x1fffu));
        cpu->w[15] += 2u;
        return true;
    }
    if ((opcode & 0xffe000u) == 0xf90000u) {
        check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
        cpu->w[15] -= 2u;
        write_word(cpu, opcode & 0x1fffu, read_word(cpu, cpu->w[15]));
        return true;
    }
    if ((opcode & 0xff8001u) == 0xfa0000u) {
        int32_t stack_top;
        if ((cpu->corcon & 0x0004u) != 0u) {
            schedule_stack_error(cpu, 2u);
        }
        check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
        write_word(cpu, cpu->w[15], cpu->w[14]);
        cpu->w[15] += 2u;
        write_working_register(cpu, 14u, cpu->w[15]);
        cpu->corcon |= 0x0004u;
        stack_top = (int32_t)cpu->w[15] + (opcode & 0x007ffeu);
        write_working_register(cpu, 15u, (uint16_t)stack_top);
        check_stack_address(cpu, cpu->w[15], stack_top > UINT16_MAX);
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
    if ((opcode & 0xfffff0u) == 0x010200u) {
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
        if (cpu->pc >= DSPIC33_PROGRAM_LIMIT) {
            return false;
        }
        count = (opcode & 0xfffff0u) == 0x088000u ? cpu->w[opcode & 0x0fu]
                                                  : (uint16_t)(opcode & 0x7fffu);
        displacement = (int16_t)(cpu->program[cpu->pc / 2u] & 0xffffu);
        cpu->pc += 2u;
        if (cpu->do_depth == 4u) {
            dspic33_write_word(cpu, 0x08c4u,
                               (uint16_t)(dspic33_read_word(cpu, 0x08c4u) | 0x0010u));
            schedule_soft_trap(cpu, 6u, 0x000010u, 9u, 1u);
            return true;
        }
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
            cpu->tblpag = value & 0x00ffu;
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
            cpu->tblpag = value & 0x00ffu;
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
        write_working_register(cpu, destination,
                               (uint16_t)((cpu->w[destination] & 0xff00u) | literal));
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
        write_working_register(
            cpu, (uint8_t)(opcode & 0x0fu),
            read_word(cpu, direct_move_address(cpu, address, false)));
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
    if ((opcode & 0xfc0000u) == 0xb40000u || (opcode & 0xff0000u) == 0xbd0000u) {
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
        write_working_register(
            cpu, destination, zero_extend ? source : (uint16_t)(int16_t)(int8_t)source);
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
    if ((opcode & 0xff7fffu) == 0xcb0000u || (opcode & 0xff7fffu) == 0xcb1000u ||
        (opcode & 0xff7fffu) == 0xcb3000u) {
        return execute_accumulator_arithmetic(cpu, opcode);
    }
    if ((opcode & 0xff4000u) == 0xc30000u || (opcode & 0xff4000u) == 0xc70000u) {
        return execute_dsp_clear_or_move(cpu, opcode);
    }
    if ((opcode & 0xfc4c00u) == 0xf04000u && (opcode & 3u) >= 2u) {
        return execute_euclidean_distance(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0xc00000u || (opcode & 0xfc0000u) == 0xf00000u) {
        return execute_dsp_multiply(cpu, opcode);
    }
    if ((opcode & 0xff7f00u) == 0xc80000u) {
        return execute_accumulator_shift(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xc90000u || (opcode & 0xff0000u) == 0xca0000u) {
        return execute_accumulator_word(cpu, opcode, (opcode & 0xff0000u) == 0xc90000u);
    }
    if ((opcode & 0xfe0000u) == 0xcc0000u) {
        return execute_accumulator_store(cpu, opcode);
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
        cpu->pc = pop_program_counter(cpu);
        if ((opcode & 0x004000u) != 0u) {
            write_working_register(
                cpu, destination,
                (uint16_t)((cpu->w[destination] & 0xff00u) | (uint8_t)literal));
        } else {
            write_working_register(cpu, destination, literal);
        }
        return true;
    }
    if (opcode == 0xfe0000u) {
        uint64_t instructions = cpu->instructions;
        uint64_t cycles = cpu->cycles;
        uint64_t device_cycles = cpu->device_cycles;
        uint64_t software_reset_count = cpu->software_reset_count + 1u;
        uint64_t trap_count = cpu->trap_count;
        uint16_t reset_interrupt = cpu->last_interrupt;
        bool async_events_enabled = cpu->async_events_enabled;
        uint16_t rcon = dspic33_read_word(cpu, 0x0740u);
        reset_processor(cpu, 0u, false);
        cpu->instructions = instructions;
        cpu->cycles = cycles;
        cpu->device_cycles = device_cycles;
        cpu->software_reset_count = software_reset_count;
        cpu->trap_count = trap_count;
        cpu->reset_interrupt = reset_interrupt;
        cpu->async_events_enabled = async_events_enabled;
        dspic33_write_word(cpu, 0x0740u, (uint16_t)(rcon | 0x0040u));
        return true;
    }
    if ((opcode & 0xfffffeu) == 0xfe4000u) {
        uint16_t rcon = (uint16_t)(dspic33_read_word(cpu, 0x0740u) & ~0x001cu);
        if ((opcode & 1u) == 0u) {
            rcon |= 0x0008u;
            cpu->power_state = DSPIC33_POWER_SLEEP;
            cpu->stop_reason = DSPIC33_SLEEPING;
        } else {
            rcon |= 0x0004u;
            cpu->power_state = DSPIC33_POWER_IDLE;
            cpu->stop_reason = DSPIC33_IDLING;
        }
        dspic33_write_word(cpu, 0x0740u, rcon);
        return true;
    }
    if (opcode == 0xfe6000u) {
        return true;
    }
    if (opcode == 0x000000u || opcode == 0x00075au ||
        (opcode & 0xff0000u) == 0xff0000u) {
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

static void reset_processor(Dspic33* cpu, uint32_t entry, bool clear_memory) {
    memset(cpu->data, 0, clear_memory ? DSPIC33_DATA_SIZE : 0x1000u);
    memset(cpu->w, 0, sizeof(cpu->w));
    memset(cpu->shadow_w, 0, sizeof(cpu->shadow_w));
    cpu->shadow_status = 0u;
    memset(cpu->accumulator, 0, sizeof(cpu->accumulator));
    cpu->w[15] = 0x1000u;
    cpu->pc = entry;
    cpu->sr = 0u;
    cpu->corcon = 0x0020u;
    cpu->splim = 0u;
    cpu->splim_enabled = false;
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
    cpu->device_cycles = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_interrupt = UINT16_MAX;
    cpu->last_interrupt_return = 0u;
    cpu->interrupt_count = 0u;
    cpu->software_reset_count = 0u;
    cpu->trap_count = 0u;
    cpu->last_trap_return = 0u;
    cpu->reset_interrupt = UINT16_MAX;
    cpu->last_trap = UINT16_MAX;
    memset(cpu->pending_soft_traps, 0, sizeof(cpu->pending_soft_traps));
    cpu->address_error_return = 0u;
    cpu->instruction_active = false;
    cpu->address_error = false;
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = false;
    cpu->async_events_enabled = true;
    memset(cpu->interrupt_log_irq, 0xff, sizeof(cpu->interrupt_log_irq));
    memset(cpu->interrupt_log_entry, 0, sizeof(cpu->interrupt_log_entry));
    memset(cpu->interrupt_log_return, 0, sizeof(cpu->interrupt_log_return));
    cpu->events.count = 0u;
    cpu->events.sequence = 0u;
    memset(cpu->write_latches, 0xff, sizeof(cpu->write_latches));
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    cpu->stop_reason = DSPIC33_RUNNING;
    dspic33_device_reset(cpu);
}

void dspic33_reset(Dspic33* cpu, uint32_t entry) { reset_processor(cpu, entry, true); }

void dspic33_set_async_events(Dspic33* cpu, bool enabled) {
    cpu->async_events_enabled = enabled;
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
    if ((cpu->address_error && !cpu->address_error_access_allowed) ||
        !check_data_implementation(cpu, address, 1u) || address >= DSPIC33_DATA_SIZE) {
        return;
    }
    if (!cpu->io.dma_transfer_active) {
        cpu->io.cpu_write_cycle = cpu->cycles;
        cpu->io.cpu_write_address = address;
        cpu->io.cpu_write_previous =
            address < 32u ? (uint8_t)(cpu->w[address / 2u] >> ((address & 1u) * 8u))
                          : cpu->data[address];
        cpu->io.cpu_write_width = 1u;
        cpu->io.cpu_write_valid = true;
    }
    if (address < 32u) {
        uint8_t reg = (uint8_t)(address / 2u);
        if ((address & 1u) == 0u) {
            write_working_register(cpu, reg,
                                   (uint16_t)((cpu->w[reg] & 0xff00u) | value));
        } else {
            write_working_register(
                cpu, reg,
                (uint16_t)((cpu->w[reg] & 0x00ffu) | ((uint16_t)value << 8u)));
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
        if (word_address == 0x0054u) {
            if ((address & 1u) == 0u) {
                cpu->tblpag = value;
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
        default:
            break;
        }
        if (reg != NULL) {
            uint16_t preserved_priority = (uint16_t)(*reg & 0x00e0u);
            if ((address & 1u) == 0u) {
                *reg = (uint16_t)((*reg & 0xff00u) | value);
            } else {
                *reg = (uint16_t)((*reg & 0x00ffu) | ((uint16_t)value << 8u));
            }
            if (word_address == 0x0042u && (cpu->data[0x08c1u] & 0x80u) != 0u) {
                *reg = (uint16_t)((*reg & ~0x00e0u) | preserved_priority);
            }
            if (word_address == 0x0020u) {
                *reg &= 0xfffeu;
            } else if (word_address == 0x0032u) {
                *reg &= 0x03ffu;
            } else if (word_address == 0x0034u) {
                *reg &= 0x01ffu;
            }
            return;
        }
        if (word_address == 0x003au || word_address == 0x003cu) {
            uint8_t shift = (uint8_t)((address - 0x003au) * 8u);
            cpu->dostart = (cpu->dostart & ~((uint32_t)0xffu << shift)) |
                           ((uint32_t)value << shift);
            cpu->dostart &= 0x003ffffeu;
            return;
        }
        if (word_address == 0x003eu || word_address == 0x0040u) {
            uint8_t shift = (uint8_t)((address - 0x003eu) * 8u);
            cpu->doend =
                (cpu->doend & ~((uint32_t)0xffu << shift)) | ((uint32_t)value << shift);
            cpu->doend &= 0x003ffffeu;
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
    if (!check_data_alignment(cpu, address) ||
        !check_data_implementation(cpu, address, 2u) ||
        (cpu->address_error && !cpu->address_error_access_allowed) ||
        address + 1u >= DSPIC33_DATA_SIZE) {
        return;
    }
    if (!cpu->io.dma_transfer_active) {
        cpu->io.cpu_write_cycle = cpu->cycles;
        cpu->io.cpu_write_address = address;
        cpu->io.cpu_write_previous =
            address < 32u ? cpu->w[address / 2u]
                          : (uint16_t)(cpu->data[address] |
                                       ((uint16_t)cpu->data[address + 1u] << 8u));
        cpu->io.cpu_write_width = 2u;
        cpu->io.cpu_write_valid = true;
    }
    if (address < 32u || (address >= 0x0020u && address <= 0x0054u)) {
        dspic33_write_byte(cpu, address, (uint8_t)value);
        dspic33_write_byte(cpu, address + 1u, (uint8_t)(value >> 8u));
        if (address == 0x0020u) {
            cpu->splim_enabled = true;
        }
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
    if ((cpu->address_error && !cpu->address_error_access_allowed) ||
        !check_data_implementation(cpu, address, 1u)) {
        return 0u;
    }
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
    if (!check_data_alignment(cpu, address) ||
        !check_data_implementation(cpu, address, 2u) ||
        (cpu->address_error && !cpu->address_error_access_allowed)) {
        return 0u;
    }
    uint16_t low = dspic33_read_byte(cpu, address);
    uint16_t high = dspic33_read_byte(cpu, address + 1u);
    return (uint16_t)(low | (high << 8u));
}

Dspic33StopReason dspic33_step(Dspic33* cpu) {
    uint16_t working_registers[16];
    int64_t accumulators[2];
    uint16_t status;
    uint16_t control;
    uint32_t opcode;
    uint32_t instruction_pc;
    if (cpu->power_state != DSPIC33_POWER_ACTIVE) {
        if (!dspic33_device_wake(cpu)) {
            cpu->stop_reason = cpu->power_state == DSPIC33_POWER_SLEEP
                                   ? DSPIC33_SLEEPING
                                   : DSPIC33_IDLING;
            return cpu->stop_reason;
        }
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        cpu->stop_reason = DSPIC33_RUNNING;
    } else {
        if (!service_pending_soft_trap(cpu)) {
            dspic33_device_service_interrupt(cpu);
        }
    }
    if ((cpu->pc & 1u) != 0u || cpu->pc >= DSPIC33_PROGRAM_LIMIT) {
        cpu->stop_reason = DSPIC33_PROGRAM_BOUNDS;
        return cpu->stop_reason;
    }
    instruction_pc = cpu->pc;
    opcode = cpu->program[cpu->pc / 2u];
    if (opcode == 0x064000u) {
        uint64_t cycles;
        dspic33_device_return_interrupt(cpu);
        cycles = exception_pending(cpu) ? 5u : 6u;
        cpu->instructions++;
        advance_instruction(cpu, cycles);
        return cpu->stop_reason;
    }
    if (opcode == 0x060000u) {
        uint64_t cycles;
        if (cpu->call_depth == 0u) {
            cpu->stop_reason = DSPIC33_RETURNED;
            return cpu->stop_reason;
        }
        cpu->pc = pop_program_counter(cpu);
        cycles = exception_pending(cpu) ? 5u : 6u;
        cpu->instructions++;
        advance_instruction(cpu, cycles);
        return cpu->stop_reason;
    }
    memcpy(working_registers, cpu->w, sizeof(working_registers));
    memcpy(accumulators, cpu->accumulator, sizeof(accumulators));
    status = cpu->sr;
    control = cpu->corcon;
    cpu->pc += 2u;
    cpu->instructions++;
    cpu->instruction_active = true;
    cpu->address_error = false;
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = false;
    if (!execute(cpu, opcode) && !cpu->address_error) {
        cpu->instruction_active = false;
        cpu->pc -= 2u;
        if (cpu->stop_reason == DSPIC33_RUNNING) {
            cpu->unsupported_opcode = opcode;
            cpu->stop_reason = DSPIC33_UNSUPPORTED_INSTRUCTION;
        }
        return cpu->stop_reason;
    }
    cpu->instruction_active = false;
    if (cpu->address_error) {
        uint32_t return_pc = cpu->address_error_return;
        if (!cpu->address_error_working_state_completed) {
            memcpy(cpu->w, working_registers, sizeof(working_registers));
            cpu->sr = status;
        }
        memcpy(cpu->accumulator, accumulators, sizeof(accumulators));
        cpu->corcon = control;
        cpu->address_error = false;
        cpu->address_error_access_allowed = false;
        cpu->address_error_working_state_completed = false;
        enter_trap(cpu, 1u, 0x000006u, 14u, 0x0008u, return_pc);
        advance_instruction(cpu, instruction_cycles(cpu, opcode));
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
    advance_instruction(cpu, instruction_cycles(cpu, opcode));
    if (cpu->power_state != DSPIC33_POWER_ACTIVE && dspic33_device_wake(cpu)) {
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        cpu->stop_reason = DSPIC33_RUNNING;
    }
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
    case DSPIC33_SLEEPING:
        return "sleeping";
    case DSPIC33_IDLING:
        return "idling";
    case DSPIC33_HALTED:
        return "halted";
    case DSPIC33_TRAPPED:
        return "trap";
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
