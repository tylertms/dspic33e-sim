#include "dspic33.h"

#include <stdlib.h>
#include <string.h>

static uint16_t read_word(const Dspic33* cpu, uint16_t address) {
    if (address < 32u) {
        return cpu->w[address / 2u];
    }
    return (uint16_t)(cpu->data[address] |
                      ((uint16_t)cpu->data[(uint16_t)(address + 1u)] << 8u));
}

static void write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    dspic33_write_byte(cpu, address, (uint8_t)value);
    dspic33_write_byte(cpu, (uint16_t)(address + 1u), (uint8_t)(value >> 8u));
}

static uint8_t read_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg) {
    if (mode == 0u) {
        return (uint8_t)cpu->w[reg];
    }
    if (mode == 1u) {
        return dspic33_read_byte(cpu, cpu->w[reg]);
    }
    return 0u;
}

static uint16_t read_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg) {
    if (mode == 0u) {
        return cpu->w[reg];
    }
    if (mode == 1u) {
        return read_word(cpu, cpu->w[reg]);
    }
    return 0u;
}

static bool write_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg, uint8_t value) {
    if (mode == 0u) {
        cpu->w[reg] = (uint16_t)((cpu->w[reg] & 0xff00u) | value);
        return true;
    }
    if (mode == 1u) {
        dspic33_write_byte(cpu, cpu->w[reg], value);
        return true;
    }
    return false;
}

static bool write_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg,
                               uint16_t value) {
    if (mode == 0u) {
        cpu->w[reg] = value;
        return true;
    }
    if (mode == 1u) {
        dspic33_write_byte(cpu, cpu->w[reg], (uint8_t)value);
        dspic33_write_byte(cpu, (uint16_t)(cpu->w[reg] + 1u), (uint8_t)(value >> 8u));
        return true;
    }
    return false;
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
    bool byte_mode = (opcode & 0x004000u) != 0u;

    if (source_mode > 1u || destination_mode > 1u) {
        return false;
    }
    if (byte_mode) {
        uint8_t value = read_operand_byte(cpu, source_mode, source_register);
        return write_operand_byte(cpu, destination_mode, destination_register, value);
    }
    return write_operand_word(cpu, destination_mode, destination_register,
                              read_operand_word(cpu, source_mode, source_register));
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

static void update_logic_flags(Dspic33* cpu, uint16_t value) {
    cpu->sr = (uint16_t)(cpu->sr & ~0x000au);
    if (value == 0u) {
        cpu->sr |= 0x0002u;
    }
    if ((value & 0x8000u) != 0u) {
        cpu->sr |= 0x0008u;
    }
}

static void update_add_flags(Dspic33* cpu, uint16_t left, uint16_t right,
                             uint32_t result) {
    uint16_t value = (uint16_t)result;
    cpu->sr = (uint16_t)(cpu->sr & ~0x010fu);
    if (value == 0u) {
        cpu->sr |= 0x0002u;
    }
    if ((value & 0x8000u) != 0u) {
        cpu->sr |= 0x0008u;
    }
    if (result > UINT16_MAX) {
        cpu->sr |= 0x0001u;
    }
    if (((left & 0x000fu) + (right & 0x000fu)) > 0x000fu) {
        cpu->sr |= 0x0100u;
    }
    if (((~(left ^ right) & (left ^ value)) & 0x8000u) != 0u) {
        cpu->sr |= 0x0004u;
    }
}

static void update_subtract_flags(Dspic33* cpu, uint16_t left, uint16_t right,
                                  uint16_t value) {
    cpu->sr = (uint16_t)(cpu->sr & ~0x010fu);
    if (value == 0u) {
        cpu->sr |= 0x0002u;
    }
    if ((value & 0x8000u) != 0u) {
        cpu->sr |= 0x0008u;
    }
    if (left >= right) {
        cpu->sr |= 0x0001u;
    }
    if ((left & 0x000fu) >= (right & 0x000fu)) {
        cpu->sr |= 0x0100u;
    }
    if ((((left ^ right) & (left ^ value)) & 0x8000u) != 0u) {
        cpu->sr |= 0x0004u;
    }
}

static bool execute_binary(Dspic33* cpu, uint32_t opcode, uint32_t operation) {
    uint8_t left_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint16_t left = cpu->w[left_register];
    uint16_t right;
    uint32_t result;

    if ((opcode & 0x0060u) == 0x0060u) {
        right = (uint16_t)(opcode & 0x001fu);
    } else if ((opcode & 0x0070u) <= 0x0050u) {
        uint8_t mode = (uint8_t)((opcode >> 4u) & 0x07u);
        if (mode > 1u) {
            return false;
        }
        right = read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu));
    } else {
        return false;
    }
    if (operation == 0x400000u) {
        result = (uint32_t)left + right;
        cpu->w[destination] = (uint16_t)result;
        update_add_flags(cpu, left, right, result);
        return true;
    }
    if (operation == 0x500000u) {
        uint16_t value = (uint16_t)(left - right);
        if (!write_operand_word(cpu, (uint8_t)((opcode >> 11u) & 0x07u), destination,
                                value)) {
            return false;
        }
        update_subtract_flags(cpu, left, right, value);
        return true;
    }
    if (operation == 0x600000u) {
        cpu->w[destination] = (uint16_t)(left & right);
    } else {
        cpu->w[destination] = (uint16_t)(left ^ right);
    }
    update_logic_flags(cpu, cpu->w[destination]);
    return true;
}

static bool branch_condition(const Dspic33* cpu, uint8_t condition, bool* take) {
    bool zero = (cpu->sr & 0x0002u) != 0u;
    if (condition == 0x07u) {
        *take = true;
        return true;
    }
    if (condition == 0x02u) {
        *take = zero;
        return true;
    }
    if (condition == 0x0au) {
        *take = !zero;
        return true;
    }
    return false;
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
    update_logic_flags(cpu, value);
    return true;
}

static bool execute(Dspic33* cpu, uint32_t opcode) {
    if ((opcode & 0xf00000u) == 0x200000u) {
        return execute_move_literal(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0x780000u) {
        return execute_move(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xbe0000u) {
        return execute_move_double(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0x800000u) {
        uint16_t address = (uint16_t)(((opcode >> 4u) & 0x7fffu) << 1u);
        cpu->w[opcode & 0x0fu] = read_word(cpu, address);
        return true;
    }
    if ((opcode & 0xfff800u) == 0xfb8000u) {
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        cpu->w[destination] = (uint8_t)cpu->w[opcode & 0x0fu];
        update_logic_flags(cpu, cpu->w[destination]);
        return true;
    }
    if ((opcode & 0xff0000u) == 0xdd0000u) {
        return execute_shift(cpu, opcode, true);
    }
    if ((opcode & 0xff0000u) == 0xde0000u) {
        return execute_shift(cpu, opcode, false);
    }
    if ((opcode & 0xf80000u) == 0x400000u) {
        return execute_binary(cpu, opcode, 0x400000u);
    }
    if ((opcode & 0xf80000u) == 0x500000u) {
        return execute_binary(cpu, opcode, 0x500000u);
    }
    if ((opcode & 0xf80000u) == 0x600000u) {
        return execute_binary(cpu, opcode, 0x600000u);
    }
    if ((opcode & 0xf80000u) == 0x680000u) {
        return execute_binary(cpu, opcode, 0x680000u);
    }
    if ((opcode & 0xfff800u) == 0xe80000u) {
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint16_t source = cpu->w[opcode & 0x0fu];
        uint16_t value = (uint16_t)(source + 1u);
        cpu->w[destination] = value;
        update_add_flags(cpu, source, 1u, (uint32_t)source + 1u);
        return true;
    }
    if ((opcode & 0xfff800u) == 0xeb0000u) {
        cpu->w[(opcode >> 7u) & 0x0fu] = 0u;
        update_logic_flags(cpu, 0u);
        return true;
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
    if (opcode == 0x000000u || opcode == 0x00075au) {
        return true;
    }
    return false;
}

bool dspic33_initialize(Dspic33* cpu) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->program = calloc(DSPIC33_PROGRAM_WORDS, sizeof(*cpu->program));
    return cpu->program != NULL;
}

void dspic33_destroy(Dspic33* cpu) {
    free(cpu->program);
    cpu->program = NULL;
}

void dspic33_reset(Dspic33* cpu, uint32_t entry) {
    memset(cpu->data, 0, sizeof(cpu->data));
    memset(cpu->w, 0, sizeof(cpu->w));
    cpu->pc = entry;
    cpu->sr = 0u;
    cpu->call_depth = 0u;
    cpu->instructions = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->stop_reason = DSPIC33_RUNNING;
}

bool dspic33_load_program_word(Dspic33* cpu, uint32_t address, uint32_t word) {
    if ((address & 1u) != 0u || address >= DSPIC33_PROGRAM_LIMIT) {
        return false;
    }
    cpu->program[address / 2u] = word & 0x00ffffffu;
    return true;
}

void dspic33_write_byte(Dspic33* cpu, uint16_t address, uint8_t value) {
    if (address < 32u) {
        uint8_t reg = (uint8_t)(address / 2u);
        if ((address & 1u) == 0u) {
            cpu->w[reg] = (uint16_t)((cpu->w[reg] & 0xff00u) | value);
        } else {
            cpu->w[reg] = (uint16_t)((cpu->w[reg] & 0x00ffu) | ((uint16_t)value << 8u));
        }
        return;
    }
    cpu->data[address] = value;
}

void dspic33_write_word(Dspic33* cpu, uint16_t address, uint16_t value) {
    write_word(cpu, address, value);
}

uint8_t dspic33_read_byte(const Dspic33* cpu, uint16_t address) {
    if (address < 32u) {
        uint16_t value = cpu->w[address / 2u];
        return (uint8_t)(value >> ((address & 1u) * 8u));
    }
    return cpu->data[address];
}

Dspic33StopReason dspic33_run(Dspic33* cpu, uint64_t instruction_limit) {
    while (cpu->instructions < instruction_limit) {
        uint32_t opcode;
        if ((cpu->pc & 1u) != 0u || cpu->pc >= DSPIC33_PROGRAM_LIMIT) {
            cpu->stop_reason = DSPIC33_PROGRAM_BOUNDS;
            return cpu->stop_reason;
        }
        opcode = cpu->program[cpu->pc / 2u];
        if (opcode == 0x060000u) {
            if (cpu->call_depth == 0u) {
                cpu->stop_reason = DSPIC33_RETURNED;
                return cpu->stop_reason;
            }
            cpu->pc = pop_program_counter(cpu);
            cpu->instructions++;
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
    case DSPIC33_UNSUPPORTED_INSTRUCTION:
        return "unsupported instruction";
    case DSPIC33_PROGRAM_BOUNDS:
        return "program bounds";
    case DSPIC33_INSTRUCTION_LIMIT:
        return "instruction limit";
    }
    return "unknown";
}
