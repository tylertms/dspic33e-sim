#include "dspic33.h"

#include <stdlib.h>
#include <string.h>

#include "device.h"
#define DSPIC33_SFR_INCLUDE_MASTER_CLEAR_RESETS
#include "dspic33ep512mu810_sfr_map.h"
#undef DSPIC33_SFR_INCLUDE_MASTER_CLEAR_RESETS

enum {
    PSV_ADDRESS = 0x01000000u,
    PSV_HIGH_BYTE = 0x02000000u,
    PSV_ADDRESS_MASK = 0x007fffffu
};

static const uint64_t ACCUMULATOR_MASK = 0xffffffffffu;
static const uint8_t configuration_factory_defaults[] = {0xcfu, 0xffu, 0xffu, 0xffu,
                                                         0xffu, 0xdfu, 0xcfu, 0xffu};
static const uint8_t configuration_program_masks[] = {0x33u, 0x87u, 0xe7u, 0xffu,
                                                      0x3fu, 0xf7u, 0x33u, 0xffu};

enum {
    PERSISTENT_PROGRAM_PHYSICAL_BASE = 0x2000u,
    PERSISTENT_PROGRAM_PHYSICAL_LIMIT = 0x5000u,
    VECTOR_SEGMENT_EXECUTION_BASE = 0x000002u,
    VECTOR_SEGMENT_LIMIT = 0x000200u,
    CODEGUARD_GENERAL_CONFIGURATION_OFFSET = 0x04u,
    CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET = 0x10u
};

static void reset_processor(Dspic33* cpu, uint32_t entry, bool clear_memory);
typedef enum {
    DSPIC33_RESET_SOFTWARE,
    DSPIC33_RESET_ILLEGAL,
    DSPIC33_RESET_HARDWARE
} Dspic33ResetKind;
static void perform_warm_reset(Dspic33* cpu, uint16_t cause, Dspic33ResetKind kind);
static void clear_watchdog(Dspic33* cpu);
static void enter_trap(Dspic33* cpu, uint16_t trap, uint32_t vector, uint8_t priority,
                       uint16_t status, uint32_t return_pc, bool auxiliary_vector);
static void enter_address_trap(Dspic33* cpu, uint32_t return_pc);
static void schedule_soft_trap(Dspic33* cpu, uint16_t trap, uint32_t vector,
                               uint8_t priority, uint8_t delay);
static void check_stack_address(Dspic33* cpu, int32_t address, bool wrapped);

bool dspic33_program_range_implemented(uint32_t address, uint32_t size) {
    return (address < DSPIC33_PROGRAM_LIMIT &&
            size <= DSPIC33_PROGRAM_LIMIT - address) ||
           (address >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
            address < DSPIC33_AUXILIARY_PROGRAM_LIMIT &&
            size <= DSPIC33_AUXILIARY_PROGRAM_LIMIT - address);
}

static bool program_target_requires_address_error(uint32_t address) {
    return address < DSPIC33_AUXILIARY_PROGRAM_LIMIT &&
           !dspic33_program_range_implemented(address, 2u);
}

static bool vector_segment_execution_address(uint32_t address) {
    return (address & 1u) == 0u && address >= VECTOR_SEGMENT_EXECUTION_BASE &&
           address < VECTOR_SEGMENT_LIMIT;
}

static uint32_t program_address_add(uint32_t address, int32_t offset) {
    return (uint32_t)((address + offset) & 0x007ffffeu);
}

static bool auxiliary_program_address(uint32_t address) {
    return address >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
           address < DSPIC33_AUXILIARY_PROGRAM_LIMIT;
}

static bool persistent_program_tagged_address(uint32_t address) {
    return address >= DSPIC33_PERSISTENT_PROGRAM_BASE &&
           address < DSPIC33_PERSISTENT_PROGRAM_LIMIT;
}

static uint8_t codeguard_configuration(const Dspic33* cpu, bool auxiliary) {
    return cpu->configuration[auxiliary ? CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET
                                        : CODEGUARD_GENERAL_CONFIGURATION_OFFSET];
}

static bool codeguard_write_protected(uint8_t configuration) {
    return (configuration & 0x01u) == 0u;
}

static bool codeguard_high_security(uint8_t configuration) {
    uint8_t protection = (uint8_t)(configuration & 0x03u);
    uint8_t expected_key = protection == 0x03u ? 0u : 0x30u;
    return (protection & 0x02u) == 0u || (configuration & 0x30u) != expected_key;
}

static bool codeguard_programming_allowed(const Dspic33* cpu, uint32_t target) {
    bool auxiliary_target = auxiliary_program_address(target);
    uint8_t configuration = codeguard_configuration(cpu, auxiliary_target);
    if (codeguard_write_protected(configuration)) {
        return false;
    }
    if (codeguard_high_security(configuration)) {
        if (!auxiliary_target && (target & 0x00fff800u) == 0u) {
            return false;
        }
        if (cpu->nvm.auxiliary_origin != auxiliary_target) {
            return false;
        }
    }
    return true;
}

static bool codeguard_program_read_allowed(const Dspic33* cpu, uint32_t target) {
    bool auxiliary_origin;
    bool auxiliary_target;
    if (persistent_program_tagged_address(target)) {
        target -= DSPIC33_PERSISTENT_PROGRAM_BASE;
    }
    if (!cpu->instruction_active || !dspic33_program_range_implemented(target, 2u)) {
        return true;
    }
    auxiliary_origin = auxiliary_program_address(cpu->current_instruction_pc);
    auxiliary_target = auxiliary_program_address(target);
    return auxiliary_origin == auxiliary_target ||
           !codeguard_high_security(codeguard_configuration(cpu, auxiliary_target));
}

bool dspic33_codeguard_admit_program_flow(Dspic33* cpu, uint32_t origin,
                                          uint32_t target) {
    bool restricted = !auxiliary_program_address(origin) &&
                      auxiliary_program_address(target) &&
                      target < DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u &&
                      codeguard_high_security(codeguard_configuration(cpu, true));
    if (restricted) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
    }
    return !restricted;
}

static uint32_t read_cpu_program_word(const Dspic33* cpu, uint32_t address) {
    return codeguard_program_read_allowed(cpu, address)
               ? dspic33_read_program_word(cpu, address)
               : 0u;
}

static bool persistent_program_physical_address(uint32_t address) {
    return address >= PERSISTENT_PROGRAM_PHYSICAL_BASE &&
           address < PERSISTENT_PROGRAM_PHYSICAL_LIMIT;
}

static uint32_t persistent_program_index(uint32_t address) {
    if (address >= DSPIC33_PERSISTENT_PROGRAM_BASE) {
        address -= DSPIC33_PERSISTENT_PROGRAM_BASE;
    }
    return address / 2u;
}

static bool nvm_stalls_cpu(const Dspic33* cpu) {
    uint16_t operation = (uint16_t)(cpu->nvm.control & 0x000fu);
    bool auxiliary_target;
    if (operation == 0u) {
        return true;
    }
    if (operation == 0x0au) {
        auxiliary_target = true;
    } else if (operation == 0x0du) {
        auxiliary_target = false;
    } else {
        auxiliary_target = auxiliary_program_address(cpu->nvm.address);
    }
    return auxiliary_target == auxiliary_program_address(cpu->pc);
}

static void advance_pending_nvm_reset(Dspic33* cpu) {
    if (!dspic33_device_advance_nvm(cpu)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

static uint32_t instruction_length(uint32_t opcode) {
    return (opcode & 0xff0000u) == 0x020000u || (opcode & 0xff0000u) == 0x040000u ||
                   (opcode & 0xff0000u) == 0x080000u
               ? 4u
               : 2u;
}

static void raise_program_target_error(Dspic33* cpu, uint32_t return_pc) {
    cpu->address_error = true;
    cpu->address_error_return = return_pc;
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = true;
    cpu->address_error_control_state_completed = true;
}

static void raise_program_read_error(Dspic33* cpu) {
    if (!cpu->address_error) {
        cpu->address_error = true;
        cpu->address_error_return = cpu->pc;
    }
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = true;
    cpu->address_error_control_state_completed = true;
}

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

static bool data_byte_is_implemented(uint32_t address) {
    uint32_t slot;
    if (address >= 0x1000u) {
        return true;
    }
    slot = (address & 0x0ffeu) >> 1u;
    return (dspic33_sfr_implementation_bitmap[slot >> 3u] &
            (uint8_t)(1u << (slot & 7u))) != 0u;
}

static bool check_data_implementation(Dspic33* cpu, uint32_t address, uint8_t width) {
    bool implemented = (address & PSV_ADDRESS) != 0u || address + width <= 0xe000u;
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
    cpu->initialized_working_registers |= (uint16_t)(1u << reg);
    if (cpu->instruction_active) {
        cpu->instruction_working_register_writes |= (uint16_t)(1u << reg);
    }
}

static void write_working_register_byte(Dspic33* cpu, uint8_t reg, bool high,
                                        uint8_t value) {
    uint16_t shift = high ? 8u : 0u;
    uint16_t mask = high ? 0x00ffu : 0xff00u;
    cpu->w[reg] = (uint16_t)((cpu->w[reg] & mask) | ((uint16_t)value << shift));
    if (reg == 15u) {
        cpu->w[reg] &= 0xfffeu;
    }
    if (cpu->instruction_active) {
        cpu->instruction_working_register_writes |= (uint16_t)(1u << reg);
    }
}

static void record_source_address_register(Dspic33* cpu, uint8_t reg) {
    if (cpu->instruction_active) {
        cpu->instruction_source_address_registers |= (uint16_t)(1u << reg);
    }
}

void dspic33_set_working_register(Dspic33* cpu, uint8_t reg, uint16_t value) {
    if (reg < 16u) {
        write_working_register(cpu, reg, value);
    }
}

static bool address_register_initialized(Dspic33* cpu, uint8_t reg) {
    if ((cpu->initialized_working_registers & (uint16_t)(1u << reg)) != 0u) {
        return true;
    }
    perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
    return false;
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

static void write_status_byte(Dspic33* cpu, bool high_byte, uint8_t value) {
    uint16_t previous = cpu->sr;
    if (!high_byte) {
        uint16_t requested = (uint16_t)(value & 0xefu);
        if ((cpu->data[0x08c1u] & 0x80u) != 0u) {
            requested = (uint16_t)((requested & ~0x00e0u) | (previous & 0x00e0u));
        }
        cpu->sr = (uint16_t)((previous & 0xff10u) | requested);
        return;
    }
    uint16_t requested = (uint16_t)value << 8u;
    uint16_t accumulator_status = (uint16_t)(requested & 0xf000u);
    if ((requested & 0x0800u) == 0u) {
        accumulator_status &= (uint16_t)~0xc000u;
    }
    if ((requested & 0x0400u) == 0u) {
        accumulator_status &= (uint16_t)~0x3000u;
    }
    cpu->sr =
        (uint16_t)((previous & 0x02ffu) | (requested & 0x0100u) | accumulator_status);
    update_accumulator_combined_status(cpu);
}

static void write_disicnt_byte(Dspic33* cpu, bool high_byte, uint8_t value) {
    uint16_t requested;
    if (cpu->disicnt == 0u) {
        return;
    }
    requested = high_byte
                    ? (uint16_t)((cpu->disicnt & 0x00ffu) | ((uint16_t)value << 8u))
                    : (uint16_t)((cpu->disicnt & 0xff00u) | value);
    cpu->disicnt = (uint16_t)(requested & 0x3fffu);
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

static bool configuration_register_index(uint32_t address, uint8_t* index) {
    if (address < DSPIC33_CONFIGURATION_BASE + 4u ||
        address > DSPIC33_CONFIGURATION_BASE + 0x12u || (address & 1u) != 0u) {
        return false;
    }
    *index = (uint8_t)((address - DSPIC33_CONFIGURATION_BASE - 4u) / 2u);
    return *index < sizeof(configuration_factory_defaults);
}

uint32_t dspic33_read_program_word(const Dspic33* cpu, uint32_t address) {
    uint8_t configuration_index;
    if (persistent_program_physical_address(address)) {
        return cpu->persistent_program[persistent_program_index(address)] & 0x00ffffffu;
    }
    if (address < DSPIC33_PROGRAM_LIMIT) {
        return cpu->program[address / 2u] & 0x00ffffffu;
    }
    if (address >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
        address < DSPIC33_AUXILIARY_PROGRAM_LIMIT) {
        return cpu->auxiliary_program[(address - DSPIC33_AUXILIARY_PROGRAM_BASE) / 2u] &
               0x00ffffffu;
    }
    if (address == DSPIC33_CONFIGURATION_BASE ||
        address == DSPIC33_CONFIGURATION_BASE + 2u) {
        return 0u;
    }
    if (configuration_register_index(address, &configuration_index)) {
        return cpu->configuration[4u + configuration_index * 2u];
    }
    if (address == 0xff0000u) {
        return 0x001872u;
    }
    if (address == 0xff0002u) {
        return 0x004002u;
    }
    if (persistent_program_tagged_address(address)) {
        return cpu->persistent_program[persistent_program_index(address)] & 0x00ffffffu;
    }
    if (address >= DSPIC33_WRITE_LATCH_BASE && address < DSPIC33_WRITE_LATCH_LIMIT) {
        return cpu->write_latches[(address - DSPIC33_WRITE_LATCH_BASE) / 2u] &
               0x00ffffffu;
    }
    return 0x00ffffffu;
}

static uint32_t* writable_program_word(Dspic33* cpu, uint32_t address) {
    if (persistent_program_physical_address(address)) {
        return &cpu->persistent_program[persistent_program_index(address)];
    }
    if (address < DSPIC33_PROGRAM_LIMIT) {
        return &cpu->program[address / 2u];
    }
    if (address >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
        address < DSPIC33_AUXILIARY_PROGRAM_LIMIT) {
        return &cpu->auxiliary_program[(address - DSPIC33_AUXILIARY_PROGRAM_BASE) / 2u];
    }
    if (persistent_program_tagged_address(address)) {
        return &cpu->persistent_program[persistent_program_index(address)];
    }
    return NULL;
}

static uint16_t read_word(Dspic33* cpu, uint32_t address) {
    return dspic33_read_word(cpu, address);
}

static void record_non_cpu_sfr_read(Dspic33* cpu, uint32_t address) {
    if (cpu->instruction_active && !cpu->non_cpu_sfr_read &&
        cpu->current_instruction_cycles < 2u && address >= 0x005au &&
        address < 0x1000u && data_byte_is_implemented(address)) {
        cpu->non_cpu_sfr_read = true;
    }
}

static void record_cpu_data_read(Dspic33* cpu, uint32_t address) {
    record_non_cpu_sfr_read(cpu, address);
    if (cpu->instruction_active && (address & PSV_ADDRESS) != 0u) {
        cpu->psv_read = true;
    }
}

static uint8_t read_data_byte(Dspic33* cpu, uint32_t address) {
    record_cpu_data_read(cpu, address);
    return dspic33_read_byte(cpu, address);
}

static uint16_t read_data_word(Dspic33* cpu, uint32_t address) {
    record_cpu_data_read(cpu, address);
    return dspic33_read_word(cpu, address);
}

static uint16_t read_file_word(Dspic33* cpu, uint16_t address) {
    uint16_t value;
    if ((address & 1u) == 0u) {
        return read_data_word(cpu, address);
    }
    if (!check_data_alignment(cpu, address)) {
        cpu->address_error_access_allowed = true;
        value = read_data_word(cpu, address & 0xfffeu);
        cpu->address_error_access_allowed = false;
        cpu->address_error_working_state_completed = true;
        return value;
    }
    return 0u;
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
    uint16_t access_register;
    uint16_t access_data_page;
    uint16_t updated_data_page;
    bool wrapped;
    bool updates_register;
    bool paged_addressing_enabled;
    bool updates_data_page;
    bool unimplemented_data_page;
} OperandResolution;

typedef struct {
    uint32_t address;
    uint16_t updated_register;
    uint16_t updated_data_page;
    uint8_t base_register;
    bool present;
    bool access_valid;
    bool update_valid;
    bool updates_register;
    bool updates_data_page;
} DspPrefetchOutcome;

static const int8_t dsp_prefetch_updates[16] = {
    0, 2, 4, 6, 0, -6, -4, -2, 0, 2, 4, 6, 0, -6, -4, -2,
};

static bool pseudo_linear_addressing_enabled(const Dspic33* cpu, uint8_t mode,
                                             uint8_t reg, bool bit_reversed) {
    return mode >= 2u && mode <= 5u && reg < 14u && !bit_reversed &&
           !modulo_addressing_enabled(cpu, reg, false);
}

static bool pseudo_linear_terminal_page(uint16_t page, bool increment, bool write) {
    if (increment) {
        return page == 0x01ffu || (!write && page == 0x03ffu);
    }
    return page == 0x0001u || (!write && page == 0x0200u);
}

static uint32_t mapped_data_address(uint16_t address, uint16_t page, bool write) {
    if (!write && page >= 0x0200u) {
        return PSV_ADDRESS | ((page & 0x0100u) != 0u ? PSV_HIGH_BYTE : 0u) |
               ((uint32_t)(page & 0x00ffu) << 15u) | (address & 0x7fffu);
    }
    return ((uint32_t)page << 15u) | (address & 0x7fffu);
}

static bool resolve_operand_address(const Dspic33* cpu, const uint16_t* registers,
                                    uint8_t mode, uint8_t reg, uint8_t offset_reg,
                                    uint8_t width, bool write,
                                    OperandResolution* resolution) {
    int32_t delta = 0;
    int32_t effective_address;
    int32_t adjusted_address = 0;
    bool bit_reversed = bit_reversed_addressing_enabled(cpu, mode, reg, width, write);
    uint16_t data_page = write ? cpu->dswpag : cpu->dsrpag;
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
        if (data_page != 0u && registers[reg] >= 0x8000u &&
            (effective_address < 0x8000 || effective_address > UINT16_MAX) &&
            !modulo_addressing_enabled(cpu, reg, false) &&
            (reg < 14u || (reg == 14u && (cpu->corcon & 0x0004u) == 0u))) {
            resolution->address = (uint16_t)(resolution->address | 0x8000u);
        }
    }
    resolution->effective_address = effective_address;
    resolution->access_register = (uint16_t)resolution->address;
    resolution->access_data_page = data_page;
    if (data_page != 0u && resolution->updates_register && registers[reg] >= 0x8000u &&
        pseudo_linear_addressing_enabled(cpu, mode, reg, bit_reversed)) {
        bool increment = mode == 3u || mode == 5u;
        bool transition =
            increment ? adjusted_address > UINT16_MAX : adjusted_address < 0x8000;
        if (transition) {
            uint16_t offset = (uint16_t)adjusted_address & 0x7fffu;
            bool terminal = pseudo_linear_terminal_page(data_page, increment, write);
            resolution->updated_register =
                terminal ? offset : (uint16_t)(offset | 0x8000u);
            resolution->updated_data_page =
                terminal ? data_page
                         : (uint16_t)(increment ? data_page + 1u : data_page - 1u);
            resolution->updates_data_page = !terminal;
            if (mode == 4u || mode == 5u) {
                resolution->access_register = resolution->updated_register;
                resolution->access_data_page = resolution->updated_data_page;
            }
        }
    }
    resolution->address = resolution->access_register;
    resolution->paged_addressing_enabled =
        !((reg == 14u || reg == 15u) && (cpu->corcon & 0x0004u) != 0u);
    if (resolution->access_register >= 0x8000u &&
        resolution->paged_addressing_enabled) {
        resolution->unimplemented_data_page = resolution->access_data_page == 0u;
        resolution->address = mapped_data_address(resolution->access_register,
                                                  resolution->access_data_page, write);
    }
    return true;
}

static bool operand_resolution(Dspic33* cpu, uint8_t mode, uint8_t reg,
                               uint8_t offset_reg, uint8_t width, bool write,
                               OperandResolution* resolution) {
    bool uses_stack_pointer;
    if (!address_register_initialized(cpu, reg) ||
        !resolve_operand_address(cpu, cpu->w, mode, reg, offset_reg, width, write,
                                 resolution)) {
        return false;
    }
    if (!write) {
        record_source_address_register(cpu, reg);
        if (mode >= 6u) {
            record_source_address_register(cpu, offset_reg);
        }
    }
    uses_stack_pointer = reg == 15u || (mode >= 6u && offset_reg == 15u);
    if (uses_stack_pointer) {
        check_stack_address(cpu, resolution->effective_address, resolution->wrapped);
    }
    if (resolution->updates_data_page) {
        if (write) {
            cpu->dswpag = resolution->updated_data_page & 0x01ffu;
        } else {
            cpu->dsrpag = resolution->updated_data_page & 0x03ffu;
        }
    }
    if (resolution->updates_register) {
        write_working_register(cpu, reg, resolution->updated_register);
    }
    if (resolution->unimplemented_data_page) {
        raise_data_page_error(cpu);
    }
    if (!write && cpu->repeat_active != 0u && width == 2u &&
        (mode == 2u || mode == 3u) && (resolution->address & PSV_ADDRESS) != 0u) {
        cpu->psv_repeat_optimized = true;
    }
    return true;
}

static bool operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg, uint8_t offset_reg,
                            uint8_t width, bool write, uint32_t* address) {
    OperandResolution resolution;
    if (!operand_resolution(cpu, mode, reg, offset_reg, width, write, &resolution)) {
        return false;
    }
    *address = resolution.address;
    return true;
}

static bool following_operand_address(Dspic33* cpu, const OperandResolution* resolution,
                                      bool write, uint32_t* address) {
    uint32_t next = (uint32_t)resolution->access_register + 2u;
    if (next > UINT16_MAX) {
        raise_data_page_error(cpu);
        return false;
    }
    *address = (uint16_t)next;
    if (resolution->paged_addressing_enabled && next >= 0x8000u) {
        if (resolution->access_data_page == 0u) {
            raise_data_page_error(cpu);
        }
        *address =
            mapped_data_address((uint16_t)next, resolution->access_data_page, write);
    }
    return true;
}

static bool validate_operand_alignment(Dspic33* cpu, uint16_t* registers, uint8_t mode,
                                       uint8_t reg, uint8_t offset_reg, uint8_t width,
                                       bool write, bool indirect_bit) {
    OperandResolution resolution;
    if (mode == 0u) {
        return true;
    }
    if (!address_register_initialized(cpu, reg) ||
        !resolve_operand_address(cpu, registers, mode, reg, offset_reg, width, write,
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
    if (!cpu->illegal_reset) {
        cpu->address_error_working_state_completed = true;
    }
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
    return read_data_byte(cpu, address);
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
    return read_data_word(cpu, address);
}

static bool write_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg,
                               uint8_t offset_reg, uint8_t value) {
    uint32_t address;
    if (mode == 0u) {
        write_working_register_byte(cpu, reg, false, value);
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

static bool indirect_literal_address(Dspic33* cpu, uint8_t reg, int16_t offset,
                                     bool write, uint32_t* resolved_address) {
    int32_t delta = offset;
    int32_t effective_address;
    uint32_t address;
    if (!address_register_initialized(cpu, reg)) {
        return false;
    }
    if (!write) {
        record_source_address_register(cpu, reg);
    }
    effective_address = (int32_t)cpu->w[reg] + delta;
    address = modulo_address(cpu, reg, effective_address, delta, false);
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
    *resolved_address = address;
    return true;
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
        if (!indirect_literal_address(cpu, destination, offset, true, &address)) {
            return true;
        }
        if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)cpu->w[source]);
        } else {
            dspic33_write_word(cpu, address, cpu->w[source]);
        }
    } else {
        if (!indirect_literal_address(cpu, source, offset, false, &address)) {
            return true;
        }
        if (byte_mode) {
            write_working_register_byte(cpu, destination, false,
                                        read_data_byte(cpu, address));
        } else {
            write_working_register(cpu, destination, read_data_word(cpu, address));
        }
    }
    return true;
}

static bool execute_move_double(Dspic33* cpu, uint32_t opcode) {
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    bool source_to_direct = (opcode & 0xfff880u) == 0xbe0000u && source_mode <= 5u &&
                            (source_mode != 0u || (source_register & 1u) == 0u);
    bool direct_to_indirect = (opcode & 0xffc071u) == 0xbe8000u &&
                              destination_mode >= 1u && destination_mode <= 5u;
    uint16_t low;
    uint16_t high;
    uint32_t address;
    uint32_t high_address;
    OperandResolution resolution;
    uint16_t registers[16];
    if (!source_to_direct && !direct_to_indirect) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
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
        if (!operand_resolution(cpu, source_mode, source_register, 0u, 4u, false,
                                &resolution)) {
            return false;
        }
        address = resolution.address;
        low = read_data_word(cpu, address);
        if (source_register == 15u) {
            high_address = (uint16_t)(address + 2u);
            high = read_data_word(cpu, high_address);
        } else if (following_operand_address(cpu, &resolution, false, &high_address)) {
            high = read_data_word(cpu, high_address);
        } else {
            high = 0u;
        }
    }
    if (destination_mode == 0u) {
        destination_register &= 0x0eu;
        write_working_register(cpu, destination_register, low);
        write_working_register(cpu, (uint8_t)(destination_register + 1u), high);
        return true;
    }
    if (!operand_resolution(cpu, destination_mode, destination_register, 0u, 4u, true,
                            &resolution)) {
        return false;
    }
    address = resolution.address;
    write_word(cpu, address, low);
    if (destination_register == 15u) {
        high_address = (uint16_t)(address + 2u);
        write_word(cpu, high_address, high);
    } else if (following_operand_address(cpu, &resolution, true, &high_address)) {
        write_word(cpu, high_address, high);
    }
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
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
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

    if (destination_mode >= 6u) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }

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
    if (!validate_destination_after_source_execution(cpu, destination_mode, destination,
                                                     byte_mode ? 1u : 2u)) {
        if (!cpu->illegal_reset) {
            update_binary_flags(cpu, operation, left, right, borrow, carry, result,
                                value, byte_mode, with_carry);
        }
        return true;
    }
    if (byte_mode) {
        if (!write_operand_byte(cpu, destination_mode, destination, 0u,
                                (uint8_t)value)) {
            return false;
        }
    } else if (!write_operand_word(cpu, destination_mode, destination, 0u, value)) {
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
        if ((opcode & 0x00fb80u) != 0u || mode >= 6u) {
            perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
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
            if ((opcode & 0x0380u) != 0u) {
                perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
                return true;
            }
            right = byte_mode
                        ? read_operand_byte(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u)
                        : read_operand_word(cpu, mode, (uint8_t)(opcode & 0x0fu), 0u);
        }
        with_borrow = (opcode & 0x008000u) != 0u;
    } else if ((opcode & 0xff0000u) == 0xe30000u) {
        if ((opcode & 0x002000u) != 0u) {
            perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        byte_mode = (opcode & 0x004000u) != 0u;
        left = byte_mode ? read_data_byte(cpu, address) : read_file_word(cpu, address);
        right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
        with_borrow = (opcode & 0x008000u) != 0u;
    } else if ((opcode & 0xff0000u) == 0xe20000u) {
        if ((opcode & 0x00a000u) != 0u) {
            perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        byte_mode = (opcode & 0x004000u) != 0u;
        left = byte_mode ? read_data_byte(cpu, address) : read_file_word(cpu, address);
        right = 0u;
        with_borrow = false;
    } else {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
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
    bool reads_source = family != 0xebu;
    uint16_t source;
    uint16_t value;
    bool arithmetic = true;
    if (destination_mode >= 6u || (reads_source && source_mode >= 6u) ||
        (!reads_source && (opcode & 0x00007fu) != 0u)) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    source =
        reads_source
            ? (byte_mode ? read_operand_byte(cpu, source_mode, source_register, 0u)
                         : read_operand_word(cpu, source_mode, source_register, 0u))
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
    if (!validate_destination_after_source_execution(
            cpu, destination_mode, destination_register, byte_mode ? 1u : 2u)) {
        if (!cpu->illegal_reset) {
            update_unary_flags(cpu, family, alternate, source, value, byte_mode,
                               arithmetic);
        }
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
                                      uint8_t width, bool source) {
    uint16_t address = cpu->w[reg];
    int32_t adjusted_address = cpu->w[reg];
    int32_t effective_address = address;
    bool wrapped = false;
    if (source) {
        record_source_address_register(cpu, reg);
    }
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
    bool unimplemented_read;
    if ((source_mode != 0u && !address_register_initialized(cpu, source_register)) ||
        (destination_mode != 0u &&
         !address_register_initialized(cpu, destination_register))) {
        return true;
    }
    if (write) {
        value = byte_mode ? read_operand_byte(cpu, source_mode, source_register, 0u)
                          : read_operand_word(cpu, source_mode, source_register, 0u);
        if (cpu->illegal_reset) {
            return true;
        }
        if (cpu->address_error && !cpu->address_error_access_allowed) {
            return true;
        }
        table_offset = table_operand_address(
            cpu, destination_mode, destination_register, byte_mode ? 1u : 2u, false);
    } else {
        table_offset = table_operand_address(cpu, source_mode, source_register,
                                             byte_mode ? 1u : 2u, true);
        value = 0u;
    }
    address = ((((uint32_t)cpu->tblpag & 0x01ffu) << 16u) | table_offset) & 0x01fffffeu;
    unimplemented_read = !write && program_target_requires_address_error(address);
    word = unimplemented_read ? 0u
           : write            ? dspic33_read_program_word(cpu, address)
                              : read_cpu_program_word(cpu, address);
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
    if (unimplemented_read) {
        uint32_t destination_address = 0u;
        if (destination_mode == 0u) {
            if (byte_mode) {
                write_working_register_byte(cpu, destination_register, false,
                                            (uint8_t)value);
            } else {
                write_working_register(cpu, destination_register, value);
            }
        } else {
            if (!validate_destination_after_source_execution(
                    cpu, destination_mode, destination_register, byte_mode ? 1u : 2u)) {
                if (!cpu->illegal_reset) {
                    raise_program_read_error(cpu);
                }
                return true;
            }
            if (!operand_address(cpu, destination_mode, destination_register, 0u,
                                 byte_mode ? 1u : 2u, true, &destination_address)) {
                return true;
            }
            if (byte_mode) {
                dspic33_write_byte(cpu, destination_address, (uint8_t)value);
            } else {
                write_word(cpu, destination_address, value);
            }
        }
        raise_program_read_error(cpu);
        return true;
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
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (byte_mode) {
        write_working_register_byte(cpu, destination, false, (uint8_t)value);
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
    uint16_t left;
    uint16_t right;
    uint16_t carry = (cpu->sr & 1u) != 0u ? 1u : 0u;
    uint16_t value;
    uint32_t result;
    if (family == 3u && alternate) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    left = byte_mode ? read_data_byte(cpu, address) : read_file_word(cpu, address);
    right = byte_mode ? (uint8_t)cpu->w[0] : cpu->w[0];
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
        write_working_register_byte(cpu, 0u, false, (uint8_t)value);
    } else {
        write_working_register(cpu, 0u, value);
    }
    return true;
}

typedef enum { SKIP_RETURN_TARGET, SKIP_RETURN_SEQUENTIAL } SkipReturn;

typedef enum {
    COMPARE_CONTROL_NONE,
    COMPARE_CONTROL_EQUAL,
    COMPARE_CONTROL_NOT_EQUAL,
    COMPARE_CONTROL_GREATER_THAN,
    COMPARE_CONTROL_LESS_THAN
} CompareControlKind;

static CompareControlKind compare_control_kind(uint32_t opcode) {
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

static int8_t compare_control_displacement(uint32_t opcode) {
    uint8_t encoded = (uint8_t)((opcode >> 4u) & 0x3fu);
    return (int8_t)((encoded & 0x20u) != 0u ? encoded | 0xc0u : encoded);
}

static void skip_instruction(Dspic33* cpu, SkipReturn return_kind) {
    uint32_t length;
    uint32_t next;
    uint32_t sequential = cpu->pc;
    uint32_t raw_target;
    uint32_t target;
    if (!dspic33_program_range_implemented(sequential, 2u)) {
        raise_program_target_error(cpu, sequential);
        return;
    }
    next = dspic33_read_program_word(cpu, sequential);
    length = instruction_length(next);
    raw_target = sequential + length;
    target = raw_target >= DSPIC33_AUXILIARY_PROGRAM_LIMIT ? raw_target & 0x007ffffeu
                                                           : raw_target;
    cpu->pc = target;
    if (length == 2u && raw_target == DSPIC33_PROGRAM_LIMIT) {
        raise_program_target_error(cpu, return_kind == SKIP_RETURN_TARGET ? raw_target
                                                                          : sequential);
    } else if (length == 4u && raw_target == DSPIC33_PROGRAM_LIMIT) {
        cpu->sequential_program_hole_pc = raw_target;
    }
}

static int32_t signed_compare_operand(uint16_t value, bool byte_mode) {
    uint32_t mask = byte_mode ? 0x00ffu : 0xffffu;
    uint32_t sign = byte_mode ? 0x0080u : 0x8000u;
    uint32_t masked = value & mask;
    return (masked & sign) != 0u ? (int32_t)masked - (int32_t)(mask + 1u)
                                 : (int32_t)masked;
}

static bool compare_control_taken(const Dspic33* cpu, uint32_t opcode,
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
        return signed_compare_operand(left, byte_mode) >
               signed_compare_operand(right, byte_mode);
    }
    return kind == COMPARE_CONTROL_LESS_THAN &&
           signed_compare_operand(left, byte_mode) <
               signed_compare_operand(right, byte_mode);
}

static bool execute_compare_control(Dspic33* cpu, uint32_t opcode) {
    CompareControlKind kind = compare_control_kind(opcode);
    int8_t displacement = compare_control_displacement(opcode);
    uint32_t target;
    if (kind == COMPARE_CONTROL_NONE) {
        return false;
    }
    if (!compare_control_taken(cpu, opcode, kind)) {
        return true;
    }
    if (displacement == 1) {
        skip_instruction(cpu, SKIP_RETURN_SEQUENTIAL);
        return true;
    }
    target = program_address_add(cpu->pc, (int32_t)displacement * 2);
    if (program_target_requires_address_error(target)) {
        raise_program_target_error(cpu, cpu->pc);
        return true;
    }
    cpu->pc = target;
    return true;
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
        value = read_data_byte(cpu, address);
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
            value =
                byte_mode ? read_data_byte(cpu, address) : read_data_word(cpu, address);
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
        write_working_register_byte(cpu, reg, false, (uint8_t)value);
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
    uint16_t source = 0u;
    uint16_t value;
    if (family != 0xefu) {
        source =
            byte_mode ? read_data_byte(cpu, address) : read_file_word(cpu, address);
    }
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
        write_working_register_byte(cpu, 0u, false, (uint8_t)value);
    } else {
        write_working_register(cpu, 0u, value);
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

static bool relative_branch_condition(const Dspic33* cpu, uint32_t opcode, bool* take) {
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

static bool long_control_transfer(uint32_t opcode, uint8_t* source, bool* call) {
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

static bool computed_control_transfer_encoding(uint32_t opcode) {
    uint8_t source;
    bool call;
    return (opcode & 0xfffff0u) == 0x010000u || (opcode & 0xfffff0u) == 0x010200u ||
           (opcode & 0xfffff0u) == 0x010400u || (opcode & 0xfffff0u) == 0x010600u ||
           long_control_transfer(opcode, &source, &call);
}

static bool reserved_return_encoding(uint32_t opcode) {
    if ((opcode & 0xff0000u) == 0x050000u) {
        return (opcode & 0x008000u) != 0u;
    }
    return (opcode & 0xff0000u) == 0x060000u && opcode != 0x060000u &&
           opcode != 0x064000u;
}

static bool literal_control_extension_valid(uint32_t extension) {
    return (extension & 0xffff80u) == 0u;
}

static bool literal_control_first_word_valid(uint32_t opcode) {
    return (opcode & 1u) == 0u;
}

static void push_program_counter(Dspic33* cpu, uint32_t address) {
    uint16_t low = (uint16_t)(address & 0xfffeu);
    low |= (uint16_t)((cpu->corcon >> 2u) & 1u);
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15], low);
    write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15], (uint16_t)(address >> 16u));
    write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    cpu->corcon &= (uint16_t)~0x0004u;
    cpu->call_depth++;
}

static uint32_t pop_program_counter(Dspic33* cpu) {
    uint32_t high;
    uint32_t low;
    record_source_address_register(cpu, 15u);
    check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
    write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
    high = read_word(cpu, cpu->w[15]) & 0x007fu;
    check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
    write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
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
    if (cpu->illegal_reset) {
        return true;
    }

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
    if (cpu->illegal_reset) {
        return true;
    }
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
        byte_mode ? read_data_byte(cpu, address) : read_data_word(cpu, address);
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
        write_working_register_byte(cpu, 0u, false, (uint8_t)value);
    } else {
        write_working_register(cpu, 0u, value);
    }
    return true;
}

static bool multiply_encoding_valid(uint32_t opcode) {
    bool source_signed = (opcode & 0x008000u) != 0u;
    bool literal_mode = (opcode & 0x000060u) == 0x000060u;
    return !source_signed || !literal_mode;
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
        dspic33_device_latch_math_error(cpu, 0x0080u);
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
    if (cpu->illegal_reset) {
        return true;
    }
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

static bool dsp_encoding_valid(uint32_t opcode) {
    if ((opcode & 0xf80000u) == 0xc00000u) {
        uint8_t pair = (uint8_t)((opcode >> 16u) & 7u);
        return (pair != 3u && pair != 7u) ||
               ((opcode & 0x004000u) == 0u && (opcode & 3u) != 3u);
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
    if (!address_register_initialized(cpu, base_register)) {
        return false;
    }
    address = cpu->w[base_register];
    if (operation == 12u) {
        int32_t delta = (int16_t)cpu->w[12];
        address = modulo_address(cpu, base_register, (int32_t)address + delta, delta,
                                 y_space);
    }
    return check_data_alignment(cpu, address);
}

static bool dsp_x_address_valid(uint16_t address, uint16_t page) {
    if (address < 0x8000u) {
        return true;
    }
    if (page >= 0x0200u) {
        uint32_t program_address =
            mapped_data_address(address, page, false) & PSV_ADDRESS_MASK;
        return !program_target_requires_address_error(program_address);
    }
    return page == 1u && address <= 0x8ffeu;
}

static bool dsp_y_address_valid(uint16_t address) {
    return address >= 0x9000u && address <= 0xdffeu;
}

static bool resolve_dsp_x_prefetch(const Dspic33* cpu, uint8_t operation,
                                   DspPrefetchOutcome* outcome) {
    uint8_t base_register = (uint8_t)(8u + (operation >= 8u ? 1u : 0u));
    int8_t update = dsp_prefetch_updates[operation];
    uint8_t mode = update == 0 ? 1u : update > 0 ? 3u : 2u;
    uint8_t width = update < 0 ? (uint8_t)-update : (uint8_t)update;
    if (operation == 12u) {
        mode = 6u;
        width = 2u;
    } else if (width == 0u) {
        width = 2u;
    }
    OperandResolution resolution;
    if (!resolve_operand_address(cpu, cpu->w, mode, base_register, 12u, width, false,
                                 &resolution)) {
        return false;
    }
    outcome->address = resolution.address;
    outcome->updated_register = resolution.updated_register;
    outcome->updated_data_page = resolution.updated_data_page;
    outcome->base_register = base_register;
    outcome->present = true;
    outcome->access_valid =
        dsp_x_address_valid(resolution.access_register, resolution.access_data_page);
    outcome->update_valid =
        !resolution.updates_register ||
        dsp_x_address_valid(resolution.updated_register,
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
        operation == 12u ? (int16_t)cpu->w[12] : dsp_prefetch_updates[operation];
    uint16_t address = cpu->w[base_register];
    if (operation == 12u) {
        address =
            modulo_address(cpu, base_register, (int32_t)address + delta, delta, true);
    }
    outcome->address = address;
    outcome->updated_register = modulo_address(
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

static bool validate_dsp_alignments(Dspic33* cpu, uint8_t x_operation,
                                    uint8_t y_operation, bool memory_write_back) {
    return validate_dsp_prefetch_alignment(cpu, x_operation, false) &&
           validate_dsp_prefetch_alignment(cpu, y_operation, true) &&
           (!memory_write_back || (address_register_initialized(cpu, 13u) &&
                                   check_data_alignment(cpu, cpu->w[13])));
}

static void commit_dsp_prefetch(Dspic33* cpu, const DspPrefetchOutcome* outcome) {
    if (outcome->updates_data_page) {
        cpu->dsrpag = outcome->updated_data_page & 0x03ffu;
    }
    if (outcome->updates_register) {
        write_working_register(cpu, outcome->base_register, outcome->updated_register);
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

static bool execute_dsp_prefetches(Dspic33* cpu, uint8_t x_operation,
                                   uint8_t y_operation, uint16_t* x_value,
                                   uint16_t* y_value, bool* x_present,
                                   bool* y_present) {
    DspPrefetchOutcome x;
    DspPrefetchOutcome y;
    if (!resolve_dsp_prefetch(cpu, x_operation, false, &x) ||
        !resolve_dsp_prefetch(cpu, y_operation, true, &y)) {
        return false;
    }
    if (x.present) {
        record_source_address_register(cpu, x.base_register);
        if (x_operation == 12u) {
            record_source_address_register(cpu, 12u);
        }
    }
    if (y.present) {
        record_source_address_register(cpu, y.base_register);
        if (y_operation == 12u) {
            record_source_address_register(cpu, 12u);
        }
    }
    if (x.present && (x.address & PSV_ADDRESS) != 0u) {
        cpu->psv_read = true;
        if (cpu->repeat_active != 0u && (dsp_prefetch_updates[x_operation] == 2 ||
                                         dsp_prefetch_updates[x_operation] == -2)) {
            cpu->psv_repeat_optimized = true;
        }
    }
    *x_value = x.present && x.access_valid ? read_data_word(cpu, x.address) : 0u;
    *y_value = y.present && y.access_valid ? read_data_word(cpu, y.address) : 0u;
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
    uint16_t value = accumulator_store_value_with_rounding(
        cpu, cpu->accumulator[accumulator ^ 1u], true, false);
    if (mode == 0u) {
        write_working_register(cpu, 13u, value);
    } else if (mode == 1u) {
        dspic33_write_word(cpu, cpu->w[13], value);
        write_working_register(cpu, 13u, (uint16_t)(cpu->w[13] + 2u));
    }
}

static bool execute_dsp_clear_or_move(Dspic33* cpu, uint32_t opcode) {
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
        clear_accumulator_status(cpu, accumulator);
    }
    if (!execute_dsp_prefetches(cpu, x_operation, y_operation, &x_value, &y_value,
                                &x_present, &y_present)) {
        return false;
    }
    if (x_present) {
        write_working_register(cpu, (uint8_t)(4u + ((opcode >> 12u) & 3u)), x_value);
    }
    if (y_present) {
        write_working_register(cpu, (uint8_t)(4u + ((opcode >> 10u) & 3u)), y_value);
    }
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
    bool x_present;
    bool y_present;
    if (!execute_dsp_prefetches(cpu, x_operation, y_operation, &x_value, &y_value,
                                &x_present, &y_present) ||
        !x_present || !y_present) {
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
    uint16_t x_value;
    uint16_t y_value;
    bool x_present;
    bool y_present;
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
    if (!execute_dsp_prefetches(cpu, x_operation, y_operation, &x_value, &y_value,
                                &x_present, &y_present)) {
        return false;
    }
    if (x_present) {
        write_working_register(cpu, (uint8_t)(4u + ((opcode >> 12u) & 3u)), x_value);
    }
    if (y_present) {
        write_working_register(cpu, (uint8_t)(4u + ((opcode >> 10u) & 3u)), y_value);
    }
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
    write_working_register_byte(cpu, destination, false, (uint8_t)adjusted);
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
                       uint16_t status, uint32_t return_pc, bool auxiliary_vector) {
    uint16_t stacked_high;
    uint32_t target =
        dspic33_read_program_word(cpu, auxiliary_vector ? 0x007ffffau : vector) &
        0x007ffffeu;
    uint32_t origin = auxiliary_vector ? DSPIC33_AUXILIARY_PROGRAM_BASE : 0u;
    dspic33_write_word(cpu, 0x08c0u,
                       (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | status));
    if (trap != 1u && program_target_requires_address_error(target)) {
        enter_address_trap(cpu, return_pc);
        return;
    }
    if (!dspic33_codeguard_admit_program_flow(cpu, origin, target)) {
        return;
    }
    cpu->previous_working_register_writes = 0u;
    cpu->sequential_program_hole_pc = 0u;
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15],
               (uint16_t)((return_pc & 0xfffeu) | ((cpu->corcon >> 2u) & 1u)));
    write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    stacked_high = (uint16_t)(((cpu->sr & 0x00ffu) << 8u) |
                              ((cpu->corcon & 0x0008u) != 0u ? 0x0080u : 0u) |
                              ((return_pc >> 16u) & 0x007fu));
    check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
    write_word(cpu, cpu->w[15], stacked_high);
    write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
    cpu->corcon &= (uint16_t)~0x0004u;
    cpu->corcon = priority > 7u ? (uint16_t)(cpu->corcon | 0x0008u)
                                : (uint16_t)(cpu->corcon & ~0x0008u);
    cpu->sr = (uint16_t)((cpu->sr & ~0x00e0u) | ((priority & 7u) << 5u));
    cpu->pc = target;
    cpu->last_trap = trap;
    cpu->last_trap_return = return_pc;
    cpu->trap_count++;
    cpu->interrupt_depth++;
    dspic33_device_latch_interrupt(cpu, (uint8_t)trap, priority);
    cpu->repeat_active = 0u;
    cpu->repeat_pc = 0u;
    cpu->sr &= (uint16_t)~0x0010u;
    if (cpu->stop_on_trap) {
        cpu->stop_reason = DSPIC33_TRAPPED;
    }
}

static void enter_address_trap(Dspic33* cpu, uint32_t return_pc) {
    uint16_t sfa = (uint16_t)(cpu->corcon & 0x0004u);
    cpu->corcon &= (uint16_t)~0x0004u;
    enter_trap(cpu, 1u, 0x000006u, 14u, 0x0008u, return_pc, false);
    if (!cpu->illegal_reset) {
        cpu->corcon |= sfa;
    }
}

void dspic33_raise_program_vector_error(Dspic33* cpu, uint32_t return_pc) {
    enter_address_trap(cpu, return_pc);
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
                pending->auxiliary_program = auxiliary_program_address(
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
    available->auxiliary_program = auxiliary_program_address(
        cpu->instruction_active ? cpu->current_instruction_pc : cpu->pc);
}

void dspic33_set_math_error_source(Dspic33* cpu, bool active) {
    size_t index;
    if (active) {
        schedule_soft_trap(cpu, 4u, 0x00000cu, 11u, 2u);
        return;
    }
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active &&
            cpu->pending_soft_traps[index].trap == 4u) {
            cpu->pending_soft_traps[index].active = false;
        }
    }
}

static void set_trap_source(Dspic33* cpu, uint16_t trap, uint32_t vector,
                            uint8_t priority, uint8_t delay, bool active) {
    size_t index;
    if (active) {
        schedule_soft_trap(cpu, trap, vector, priority, delay);
        return;
    }
    for (index = 0u; index < 4u; index++) {
        if (cpu->pending_soft_traps[index].active &&
            cpu->pending_soft_traps[index].trap == trap) {
            cpu->pending_soft_traps[index].active = false;
        }
    }
}

void dspic33_set_generic_hard_trap_source(Dspic33* cpu, bool active) {
    set_trap_source(cpu, 2u, 0x000008u, 13u, 0u, active);
}

void dspic33_set_generic_soft_trap_source(Dspic33* cpu, bool active) {
    set_trap_source(cpu, 6u, 0x000010u, 9u, 1u, active);
}

void dspic33_raise_oscillator_fail_trap(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x08c0u,
                       (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | 0x0002u));
    schedule_soft_trap(cpu, 0u, 0x000004u, 15u, 0u);
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
        bool source_active =
            (trap == 2u && ((dspic33_read_word(cpu, 0x08c2u) & 0x2000u) != 0u ||
                            (dspic33_read_word(cpu, 0x08c6u) & 0x0001u) != 0u)) ||
            (trap == 4u && (dspic33_read_word(cpu, 0x08c0u) & 0x0010u) != 0u) ||
            (trap == 6u && (dspic33_read_word(cpu, 0x08c4u) & 0x0070u) != 0u);
        if (!source_active) {
            selected->active = false;
        }
        enter_trap(cpu, trap, vector, priority, 0u, cpu->pc,
                   selected->auxiliary_program);
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

static bool advance_instruction_cycles(Dspic33* cpu, uint64_t cycles,
                                       uint64_t device_ratio) {
    if (cycles > UINT64_MAX / device_ratio ||
        !dspic33_device_advance_instruction(cpu, cycles, cycles * device_ratio)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    return true;
}

static void advance_instruction(Dspic33* cpu, uint64_t cycles, bool separate_wait_cycle,
                                uint64_t device_ratio) {
    size_t index;
    if (separate_wait_cycle && cycles > 1u) {
        uint16_t nested_interrupt_deferred[DSPIC33_IRQ_GROUP_COUNT];
        if (!advance_instruction_cycles(cpu, cycles - 1u, device_ratio)) {
            return;
        }
        memcpy(nested_interrupt_deferred, cpu->interrupt_deferred,
               sizeof(nested_interrupt_deferred));
        if (!advance_instruction_cycles(cpu, 1u, device_ratio)) {
            return;
        }
        if (cpu->interrupt_depth != 0u) {
            for (index = 0u; index < DSPIC33_IRQ_GROUP_COUNT; index++) {
                cpu->interrupt_deferred[index] |= nested_interrupt_deferred[index];
            }
        }
    } else {
        advance_instruction_cycles(cpu, cycles, device_ratio);
    }
    for (index = 0u; index < 4u; index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[index];
        if (pending->active && pending->delay != 0u) {
            pending->delay =
                pending->delay > cycles ? (uint8_t)(pending->delay - cycles) : 0u;
        }
    }
    service_pending_soft_trap(cpu);
}

static uint64_t instruction_cycles(const Dspic33* cpu, uint32_t opcode,
                                   uint32_t instruction_pc) {
    CompareControlKind compare_kind = compare_control_kind(opcode);
    uint8_t bit_kind = (uint8_t)((opcode >> 16u) & 0x07u);
    bool bit_skip = (opcode & 0xf00000u) == 0xa00000u && bit_kind >= 6u;
    bool skip = bit_skip || (compare_kind != COMPARE_CONTROL_NONE &&
                             compare_control_displacement(opcode) == 1);
    if (skip) {
        uint32_t sequential = program_address_add(instruction_pc, 2);
        uint32_t distance = (cpu->pc - sequential) & 0x007fffffu;
        if (cpu->address_error && cpu->address_error_control_state_completed &&
            cpu->address_error_return == DSPIC33_PROGRAM_LIMIT &&
            instruction_pc + 2u == DSPIC33_PROGRAM_LIMIT) {
            return 2u;
        }
        if ((distance & 1u) == 0u && distance <= 4u) {
            return 1u + distance / 2u;
        }
    }
    if (compare_kind != COMPARE_CONTROL_NONE) {
        return compare_control_taken(cpu, opcode, compare_kind) ? 5u : 1u;
    }
    bool branch_taken;
    if (relative_branch_condition(cpu, opcode, &branch_taken)) {
        return branch_taken ? 4u : 1u;
    }
    if ((opcode & 0xff0000u) == 0x050000u) {
        return cpu->address_error || exception_pending(cpu) ? 5u : 6u;
    }
    if ((opcode & 0xff0000u) == 0xba0000u) {
        return 5u;
    }
    if ((opcode & 0xff0000u) == 0xbb0000u) {
        return 2u;
    }
    if ((opcode & 0xfffff0u) == 0x088000u || (opcode & 0xff8000u) == 0x080000u) {
        return 2u;
    }
    if ((opcode & 0xff0000u) == 0xbe0000u) {
        return 2u;
    }
    if ((opcode & 0xff0000u) == 0x020000u || (opcode & 0xff0000u) == 0x040000u ||
        (opcode & 0xff0000u) == 0x070000u ||
        computed_control_transfer_encoding(opcode)) {
        return 4u;
    }
    return 1u;
}

void dspic33_raise_dma_address_trap(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x08c4u,
                       (uint16_t)(dspic33_read_word(cpu, 0x08c4u) | 0x0020u));
    enter_trap(cpu, 6u, 0x000010u, 9u, 0u, cpu->pc, auxiliary_program_address(cpu->pc));
}

void dspic33_raise_dma_collision_trap(Dspic33* cpu) {
    enter_trap(cpu, 5u, 0x00000eu, 10u, 0x0020u, cpu->pc,
               auxiliary_program_address(cpu->pc));
}

static bool execute_divide(Dspic33* cpu, uint32_t opcode) {
    bool unsigned_divide = (opcode & 0x008000u) != 0u;
    bool double_word = (opcode & 0x000040u) != 0u;
    uint8_t high_register = (uint8_t)((opcode >> 11u) & 0x0fu);
    uint8_t low_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t divisor_register = (uint8_t)(opcode & 0x0fu);
    uint16_t divisor = cpu->w[divisor_register];
    bool overflow = false;
    bool overflow_flag = false;
    int64_t remainder;
    int64_t quotient;
    if (divisor == 0u) {
        if (cpu->repeat_active != 0u && cpu->rcount == 17u) {
            dspic33_device_latch_math_error(cpu, 0x0040u);
        }
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
        overflow_flag = overflow;
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
        overflow_flag =
            overflow &&
            (!double_word || !(((dividend < 0) != (signed_divisor < 0)) ||
                               dividend > 0x3fffffff || dividend <= -0x40000000));
    }
    write_working_register(cpu, 0u, (uint16_t)quotient);
    write_working_register(cpu, 1u, (uint16_t)remainder);
    update_divide_flags(cpu, remainder, overflow_flag);
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
        if (cpu->repeat_active != 0u && cpu->rcount == 17u) {
            dspic33_device_latch_math_error(cpu, 0x0040u);
        }
        return true;
    }
    if (cpu->repeat_active != 0u && cpu->rcount != 0u) {
        return true;
    }
    dividend = (int32_t)(int16_t)cpu->w[dividend_register] * 32768;
    quotient = dividend / divisor;
    remainder = dividend % divisor;
    overflow = quotient < INT16_MIN || quotient > INT16_MAX;
    write_working_register(cpu, 0u, (uint16_t)quotient);
    write_working_register(cpu, 1u, (uint16_t)remainder);
    update_divide_flags(cpu, remainder, overflow);
    return true;
}

static bool reserved_move_encoding(uint32_t opcode) {
    if ((opcode & 0xfff000u) == 0xfd0000u) {
        return (opcode & 0xfff870u) != 0xfd0000u;
    }
    if ((opcode & 0xff8000u) == 0xfd8000u) {
        return (opcode & 0xffbff0u) != 0xfd8000u;
    }
    if ((opcode & 0xfff000u) == 0xfed000u) {
        return (opcode & 0xfff3f0u) != 0xfed000u;
    }
    return false;
}

static bool execute(Dspic33* cpu, uint32_t opcode) {
    if ((opcode & 0xff0000u) == 0x3f0000u) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (reserved_move_encoding(opcode)) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xff0000u) == 0x010000u &&
        !computed_control_transfer_encoding(opcode)) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (reserved_return_encoding(opcode)) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xfffff0u) == 0xfd4000u) {
        return execute_decimal_adjust(cpu, opcode);
    }
    if ((opcode & 0xffbff0u) == 0xfd8000u) {
        uint8_t reg = (uint8_t)(opcode & 0x0fu);
        uint16_t value = cpu->w[reg];
        if ((opcode & 0x004000u) != 0u) {
            uint8_t low = (uint8_t)value;
            write_working_register_byte(cpu, reg, false,
                                        (uint8_t)((low << 4u) | (low >> 4u)));
        } else {
            write_working_register(cpu, reg, (uint16_t)((value << 8u) | (value >> 8u)));
        }
        return true;
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
        write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
        record_source_address_register(cpu, 15u);
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
        cpu->instruction_working_register_writes |= UINT16_C(0x000f);
        return true;
    }
    if ((opcode & 0xffe000u) == 0xf80000u) {
        check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
        write_word(cpu, cpu->w[15], read_data_word(cpu, opcode & 0x1fffu));
        write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
        return true;
    }
    if ((opcode & 0xffe000u) == 0xf90000u) {
        check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
        write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
        record_source_address_register(cpu, 15u);
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
        write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
        write_working_register(cpu, 14u, cpu->w[15]);
        cpu->corcon |= 0x0004u;
        stack_top = (int32_t)cpu->w[15] + (opcode & 0x007ffeu);
        write_working_register(cpu, 15u, (uint16_t)stack_top);
        check_stack_address(cpu, cpu->w[15], stack_top > UINT16_MAX);
        return true;
    }
    if ((opcode & 0xff0000u) == 0x020000u || (opcode & 0xff0000u) == 0x040000u) {
        bool call = (opcode & 0xff0000u) == 0x020000u;
        uint32_t return_pc;
        uint32_t second;
        uint32_t target;
        if (!literal_control_first_word_valid(opcode)) {
            perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        if (cpu->pc >= DSPIC33_AUXILIARY_PROGRAM_LIMIT) {
            cpu->pc &= 0x007ffffeu;
        }
        if (cpu->pc != DSPIC33_PROGRAM_LIMIT &&
            !dspic33_program_range_implemented(cpu->pc, 2u)) {
            return false;
        }
        second = cpu->pc == DSPIC33_PROGRAM_LIMIT
                     ? 0u
                     : dspic33_read_program_word(cpu, cpu->pc);
        if (!literal_control_extension_valid(second)) {
            perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        target = ((second & 0x007fu) << 16u) | (opcode & 0x00ffffu);
        target &= 0x007ffffeu;
        return_pc = program_address_add(cpu->pc, 2);
        if (call) {
            push_program_counter(cpu, return_pc);
        }
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010000u) {
        uint32_t target;
        push_program_counter(cpu, cpu->pc);
        target = cpu->w[opcode & 0x0fu] & 0xfffeu;
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010200u) {
        uint32_t target;
        push_program_counter(cpu, cpu->pc);
        target = program_address_add(cpu->pc, (int16_t)cpu->w[opcode & 0x0fu] * 2);
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010400u) {
        uint32_t target = cpu->w[opcode & 0x0fu] & 0xfffeu;
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010600u) {
        uint32_t target =
            program_address_add(cpu->pc, (int16_t)cpu->w[opcode & 0x0fu] * 2);
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    uint8_t long_source;
    bool long_call;
    if (long_control_transfer(opcode, &long_source, &long_call)) {
        uint32_t target = ((uint32_t)(cpu->w[long_source + 1u] & 0x007fu) << 16u) |
                          cpu->w[long_source];
        if (long_call) {
            push_program_counter(cpu, cpu->pc);
        }
        target &= 0x007ffffeu;
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x088000u || (opcode & 0xff8000u) == 0x080000u) {
        uint16_t count;
        int16_t displacement;
        uint8_t depth;
        if (!dspic33_program_range_implemented(cpu->pc, 2u)) {
            return false;
        }
        count = (opcode & 0xfffff0u) == 0x088000u ? cpu->w[opcode & 0x0fu]
                                                  : (uint16_t)(opcode & 0x7fffu);
        displacement = (int16_t)(dspic33_read_program_word(cpu, cpu->pc) & 0xffffu);
        cpu->pc = program_address_add(cpu->pc, 2);
        if (cpu->do_depth == 4u) {
            dspic33_write_word(cpu, 0x08c4u,
                               (uint16_t)(dspic33_read_word(cpu, 0x08c4u) | 0x0010u));
            schedule_soft_trap(cpu, 6u, 0x000010u, 9u, 1u);
            return true;
        }
        depth = cpu->do_depth++;
        cpu->do_start[depth] = cpu->pc;
        cpu->do_end[depth] = program_address_add(cpu->pc, (int32_t)displacement * 2);
        cpu->do_count[depth] = count;
        cpu->do_terminate[depth] = 0u;
        cpu->dostart = cpu->do_start[depth];
        cpu->doend = cpu->do_end[depth];
        cpu->dcount = count;
        cpu->corcon =
            (uint16_t)((cpu->corcon & ~0x0700u) | ((uint16_t)cpu->do_depth << 8u));
        cpu->sr |= 0x0200u;
        if (program_target_requires_address_error(cpu->do_start[depth])) {
            raise_program_target_error(cpu, cpu->pc - 2u);
        }
        return true;
    }
    if ((opcode & 0xff8000u) == 0x090000u) {
        cpu->rcount = (uint16_t)(opcode & 0x007fffu);
        cpu->repeat_psv_started = false;
        cpu->repeat_psv_reentry = false;
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
        case 2u:
            cpu->tblpag = value & 0x00ffu;
            break;
        case 3u:
            perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            break;
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
        case 2u:
            cpu->tblpag = value & 0x00ffu;
            break;
        case 3u:
            perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            break;
        }
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x098000u) {
        cpu->rcount = cpu->w[opcode & 0x0fu];
        cpu->repeat_psv_started = false;
        cpu->repeat_psv_reentry = false;
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
        write_working_register_byte(cpu, destination, false, literal);
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
            read_data_word(cpu, direct_move_address(cpu, address, false)));
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
        uint16_t value =
            byte_mode ? read_data_byte(cpu, address) : read_file_word(cpu, address);
        if ((opcode & 0x002000u) == 0u) {
            if (byte_mode) {
                write_working_register_byte(cpu, 0u, false, value);
            } else {
                write_working_register(cpu, 0u, value);
            }
        } else if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)value);
        } else {
            dspic33_write_word(cpu, address, value);
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
        if (cpu->illegal_reset) {
            return true;
        }
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
    if ((opcode & 0xfe0000u) == 0xb80000u && !multiply_encoding_valid(opcode)) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xfe0000u) == 0xb80000u) {
        return execute_multiply(cpu, opcode);
    }
    if ((opcode & 0xff7fffu) == 0xcb0000u || (opcode & 0xff7fffu) == 0xcb1000u ||
        (opcode & 0xff7fffu) == 0xcb3000u) {
        return execute_accumulator_arithmetic(cpu, opcode);
    }
    if (((opcode & 0xf80000u) == 0xc00000u || (opcode & 0xfc0000u) == 0xf00000u) &&
        !dsp_encoding_valid(opcode)) {
        perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
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
            write_working_register(
                cpu, 2u, (uint16_t)((uint8_t)cpu->w[0] * read_data_byte(cpu, address)));
        } else {
            uint32_t product = (uint32_t)cpu->w[0] * read_data_word(cpu, address);
            write_working_register(cpu, 2u, (uint16_t)product);
            write_working_register(cpu, 3u, (uint16_t)(product >> 16u));
        }
        return true;
    }
    if ((opcode & 0xff0000u) == 0xcf0000u) {
        return execute_find_first(cpu, opcode);
    }
    if (compare_control_kind(opcode) != COMPARE_CONTROL_NONE) {
        return execute_compare_control(cpu, opcode);
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
        uint32_t target = program_address_add(cpu->pc, displacement * 2);
        push_program_counter(cpu, cpu->pc);
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    bool branch_taken;
    if (relative_branch_condition(cpu, opcode, &branch_taken)) {
        if (branch_taken) {
            int32_t displacement = (int16_t)(opcode & 0xffffu);
            uint32_t target = program_address_add(cpu->pc, displacement * 2);
            if (program_target_requires_address_error(target)) {
                raise_program_target_error(cpu, cpu->pc);
                return true;
            }
            cpu->pc = target;
        }
        return true;
    }
    if ((opcode & 0xff0000u) == 0x050000u) {
        uint16_t literal = (uint16_t)((opcode >> 4u) & 0x03ffu);
        uint8_t destination = (uint8_t)(opcode & 0x0fu);
        uint32_t return_pc = cpu->pc;
        uint32_t target = pop_program_counter(cpu);
        cpu->instruction_working_register_writes &= UINT16_C(0x7fff);
        cpu->pc = target;
        if ((opcode & 0x004000u) != 0u) {
            write_working_register_byte(cpu, destination, false, (uint8_t)literal);
        } else {
            write_working_register(cpu, destination, literal);
        }
        if (program_target_requires_address_error(target)) {
            raise_program_target_error(cpu, return_pc);
        }
        return true;
    }
    if (opcode == 0xfe0000u) {
        perform_warm_reset(cpu, 0x0040u, DSPIC33_RESET_SOFTWARE);
        return true;
    }
    if ((opcode & 0xfffffeu) == 0xfe4000u) {
        if (cpu->nvm.active) {
            return true;
        }
        uint16_t rcon = (uint16_t)(dspic33_read_word(cpu, 0x0740u) & ~0x001cu);
        cpu->watchdog.ticks = 0u;
        if ((opcode & 1u) == 0u) {
            rcon |= 0x0008u;
            dspic33_device_abort_oscillator_switch(cpu);
            cpu->power_state = DSPIC33_POWER_SLEEP;
            dspic33_device_power_state_changed(cpu);
            cpu->stop_reason = DSPIC33_SLEEPING;
        } else {
            rcon |= 0x0004u;
            cpu->power_state = DSPIC33_POWER_IDLE;
            dspic33_device_power_state_changed(cpu);
            cpu->stop_reason = DSPIC33_IDLING;
        }
        dspic33_write_word(cpu, 0x0740u, rcon);
        return true;
    }
    if (opcode == 0xfe6000u) {
        clear_watchdog(cpu);
        return true;
    }
    if (opcode == 0x000000u || opcode == 0x00075au ||
        (opcode & 0xff0000u) == 0xff0000u) {
        return true;
    }
    return false;
}

bool dspic33_initialize(Dspic33* cpu) {
    size_t index;
    memset(cpu, 0, sizeof(*cpu));
    cpu->program = calloc(DSPIC33_PROGRAM_WORDS, sizeof(*cpu->program));
    cpu->auxiliary_program =
        calloc(DSPIC33_AUXILIARY_PROGRAM_WORDS, sizeof(*cpu->auxiliary_program));
    cpu->persistent_program =
        calloc(DSPIC33_PERSISTENT_PROGRAM_WORDS, sizeof(*cpu->persistent_program));
    cpu->data = calloc(DSPIC33_DATA_SIZE, sizeof(*cpu->data));
    if (cpu->program == NULL || cpu->auxiliary_program == NULL ||
        cpu->persistent_program == NULL || cpu->data == NULL) {
        dspic33_destroy(cpu);
        return false;
    }
    memset(cpu->program, 0xff, DSPIC33_PROGRAM_WORDS * sizeof(*cpu->program));
    memset(cpu->auxiliary_program, 0xff,
           DSPIC33_AUXILIARY_PROGRAM_WORDS * sizeof(*cpu->auxiliary_program));
    memset(cpu->persistent_program, 0xff,
           DSPIC33_PERSISTENT_PROGRAM_WORDS * sizeof(*cpu->persistent_program));
    for (index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        cpu->write_latches[index] = 0x00ffffffu;
    }
    memset(cpu->configuration, 0xff, sizeof(cpu->configuration));
    for (index = 0u; index < sizeof(configuration_factory_defaults) /
                                 sizeof(configuration_factory_defaults[0]);
         index++) {
        cpu->configuration[4u + index * 2u] = configuration_factory_defaults[index];
    }
    return true;
}

void dspic33_destroy(Dspic33* cpu) {
    free(cpu->program);
    free(cpu->auxiliary_program);
    free(cpu->persistent_program);
    free(cpu->data);
    free(cpu->events.items);
    cpu->program = NULL;
    cpu->auxiliary_program = NULL;
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
    uint32_t* auxiliary_program = destination->auxiliary_program;
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
    memcpy(auxiliary_program, source->auxiliary_program,
           DSPIC33_AUXILIARY_PROGRAM_WORDS * sizeof(*auxiliary_program));
    memcpy(persistent_program, source->persistent_program,
           DSPIC33_PERSISTENT_PROGRAM_WORDS * sizeof(*persistent_program));
    memcpy(data, source->data, DSPIC33_DATA_SIZE);
    if (source->events.count != 0u) {
        memcpy(events, source->events.items,
               source->events.count * sizeof(*source->events.items));
    }
    *destination = *source;
    destination->program = program;
    destination->auxiliary_program = auxiliary_program;
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
    cpu->initialized_working_registers = 0x8000u;
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
    cpu->repeat_psv_started = false;
    cpu->repeat_psv_reentry = false;
    cpu->do_depth = 0u;
    cpu->repeat_pc = 0u;
    memset(cpu->do_start, 0, sizeof(cpu->do_start));
    memset(cpu->do_end, 0, sizeof(cpu->do_end));
    memset(cpu->do_count, 0, sizeof(cpu->do_count));
    memset(cpu->do_terminate, 0, sizeof(cpu->do_terminate));
    cpu->nested_do_interrupt_cycle = 0u;
    cpu->nested_do_interrupt_end = 0u;
    cpu->nested_do_extra_decrement_end = 0u;
    cpu->nested_do_interrupt_depth = 0u;
    cpu->nested_do_interrupt_priority = 0u;
    cpu->nested_do_extra_decrement_depth = 0u;
    cpu->nested_do_interrupt_armed = false;
    cpu->instructions = 0u;
    cpu->cycles = 0u;
    cpu->device_cycles = 0u;
    cpu->unsupported_opcode = 0u;
    cpu->last_interrupt = UINT16_MAX;
    cpu->last_interrupt_return = 0u;
    cpu->interrupt_count = 0u;
    cpu->software_reset_count = 0u;
    cpu->illegal_reset_count = 0u;
    cpu->trap_count = 0u;
    cpu->last_trap_return = 0u;
    cpu->reset_interrupt = UINT16_MAX;
    cpu->last_trap = UINT16_MAX;
    memset(cpu->pending_soft_traps, 0, sizeof(cpu->pending_soft_traps));
    cpu->address_error_return = 0u;
    cpu->instruction_active = false;
    cpu->instruction_advancing = false;
    cpu->interrupt_entry_active = false;
    cpu->interrupt_entry_overlap = 0u;
    cpu->current_instruction_cycles = 0u;
    cpu->instruction_working_register_writes = 0u;
    cpu->instruction_source_address_registers = 0u;
    cpu->previous_working_register_writes = 0u;
    cpu->non_cpu_sfr_read = false;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    cpu->address_error = false;
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = false;
    cpu->address_error_accumulator_state_completed = false;
    cpu->address_error_control_state_completed = false;
    cpu->sequential_program_hole_pc = 0u;
    cpu->illegal_reset = false;
    cpu->reset_locked = false;
    cpu->async_events_enabled = true;
    memset(cpu->interrupt_log_irq, 0xff, sizeof(cpu->interrupt_log_irq));
    memset(cpu->interrupt_log_entry, 0, sizeof(cpu->interrupt_log_entry));
    memset(cpu->interrupt_log_return, 0, sizeof(cpu->interrupt_log_return));
    cpu->events.count = 0u;
    cpu->events.sequence = 0u;
    memset(&cpu->nvm, 0, sizeof(cpu->nvm));
    memset(&cpu->oscillator, 0, sizeof(cpu->oscillator));
    memset(&cpu->watchdog, 0, sizeof(cpu->watchdog));
    for (size_t index = 0u; index < DSPIC33_WRITE_LATCH_WORDS; index++) {
        cpu->write_latches[index] = 0x00ffffffu;
    }
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    cpu->stop_reason = DSPIC33_RUNNING;
    dspic33_device_reset(cpu);
}

static void apply_master_clear_sfr_reset(Dspic33* cpu, const uint8_t* previous) {
    size_t index;
    for (index = 0u; index < DSPIC33_SFR_MASTER_CLEAR_RESET_COUNT; index++) {
        const Dspic33SfrMasterClearReset* reset =
            &dspic33_sfr_master_clear_resets[index];
        uint16_t address = reset->address;
        uint16_t current =
            (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
        uint16_t prior =
            (uint16_t)(previous[address] | ((uint16_t)previous[address + 1u] << 8u));
        uint16_t mask = (uint16_t)(reset->known_mask | reset->unchanged);
        uint16_t value =
            (uint16_t)((current & ~mask) | reset->value | (prior & reset->unchanged));
        cpu->data[address] = (uint8_t)value;
        cpu->data[address + 1u] = (uint8_t)(value >> 8u);
    }
}

static void perform_warm_reset(Dspic33* cpu, uint16_t cause, Dspic33ResetKind kind) {
    static const uint16_t preserved_addresses[] = {
        0x0626u, 0x0728u, 0x0742u, 0x0744u, 0x0746u, 0x0748u, 0x0758u, 0x075au,
    };
    uint16_t
        preserved_values[sizeof(preserved_addresses) / sizeof(preserved_addresses[0])];
    uint8_t previous_sfr[0x1000u];
    uint16_t adc[DSPIC33_ADC_CHANNEL_COUNT];
    uint16_t gpio[DSPIC33_GPIO_PORT_COUNT];
    uint16_t comparator_input[DSPIC33_COMPARATOR_COUNT][DSPIC33_COMPARATOR_INPUT_COUNT];
    uint16_t comparator_reference[DSPIC33_COMPARATOR_REFERENCE_COUNT];
    uint16_t dci_receive[DSPIC33_DCI_BUFFER_COUNT];
    uint16_t dci_input = cpu->io.dci.input;
    Dspic33Rtcc rtcc = cpu->io.rtcc;
    uint64_t instructions = cpu->instructions;
    uint64_t cycles = cpu->cycles;
    uint64_t device_cycles = cpu->device_cycles;
    uint64_t interrupt_count = cpu->interrupt_count;
    uint64_t software_reset_count =
        cpu->software_reset_count + (kind == DSPIC33_RESET_SOFTWARE ? 1u : 0u);
    uint64_t trap_count = cpu->trap_count;
    uint32_t reset_entry = (cpu->configuration[0x0eu] & 0x04u) != 0u ? 0u : 0x007ffffcu;
    bool auxiliary_security_reset =
        reset_entry == 0x007ffffcu &&
        codeguard_high_security(codeguard_configuration(cpu, true));
    uint64_t illegal_reset_count =
        cpu->illegal_reset_count +
        (kind == DSPIC33_RESET_ILLEGAL || auxiliary_security_reset ? 1u : 0u);
    uint64_t auxiliary_pll_remaining = 0u;
    uint64_t oscillator_remaining = 0u;
    uint16_t oscillator_phase = 0u;
    uint16_t reset_interrupt = cpu->last_interrupt;
    uint16_t rcon =
        (uint16_t)(cpu->data[0x0740u] | ((uint16_t)cpu->data[0x0741u] << 8u));
    uint8_t uart_cts = cpu->io.uart_cts;
    uint8_t spi_selected = cpu->io.spi_selected;
    uint16_t timer_gate = cpu->io.timer_gate;
    uint8_t pwm_dead_time_inputs = cpu->io.pwm_dead_time_inputs;
    uint8_t pwm_sync_inputs = cpu->io.pwm_sync_inputs;
    uint8_t output_compare_fault_inputs = cpu->io.output_compare.fault_inputs;
    uint8_t output_compare_fault_direct_mask = cpu->io.output_compare.fault_direct_mask;
    uint32_t pwm_fault_inputs = cpu->io.pwm_fault_inputs;
    uint32_t pwm_current_limit_inputs = cpu->io.pwm_current_limit_inputs;
    bool usb_host_attached = cpu->io.usb_host_attached;
    bool async_events_enabled = cpu->async_events_enabled;
    bool stop_on_trap = cpu->stop_on_trap;
    bool auxiliary_pll_pending = false;
    bool oscillator_pending = false;
    uint32_t auxiliary_pll_generation = cpu->io.auxiliary_pll_generation;
    Dspic33Oscillator oscillator = cpu->oscillator;
    size_t index;
    if (cpu->nvm.active) {
        if (!cpu->nvm.reset_pending) {
            cpu->nvm.reset_cause = cause;
            cpu->nvm.reset_kind = (uint8_t)kind;
            cpu->nvm.reset_pending = true;
        }
        if (kind == DSPIC33_RESET_ILLEGAL) {
            cpu->illegal_reset = true;
        }
        return;
    }
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        if (event->type == DSPIC33_EVENT_AUX_PLL &&
            event->value == auxiliary_pll_generation) {
            auxiliary_pll_remaining =
                event->cycle > device_cycles ? event->cycle - device_cycles : 0u;
            auxiliary_pll_pending = true;
            break;
        }
    }
    for (index = 0u; index < cpu->events.count; index++) {
        const Dspic33Event* event = &cpu->events.items[index];
        if (event->type == DSPIC33_EVENT_OSCILLATOR &&
            event->value == oscillator.generation) {
            oscillator_remaining =
                event->cycle > device_cycles ? event->cycle - device_cycles : 0u;
            oscillator_phase = event->source;
            oscillator_pending = true;
            break;
        }
    }
    for (index = 0u;
         index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        uint16_t address = preserved_addresses[index];
        preserved_values[index] =
            (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    }
    memcpy(previous_sfr, cpu->data, sizeof(previous_sfr));
    memcpy(adc, cpu->io.adc, sizeof(adc));
    memcpy(gpio, cpu->io.gpio, sizeof(gpio));
    memcpy(comparator_input, cpu->io.comparator.input, sizeof(comparator_input));
    memcpy(comparator_reference, cpu->io.comparator.reference,
           sizeof(comparator_reference));
    for (index = 0u; index < DSPIC33_DCI_BUFFER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0290u + index * 2u);
        dci_receive[index] =
            (uint16_t)(cpu->data[address] | ((uint16_t)cpu->data[address + 1u] << 8u));
    }
    reset_processor(cpu, auxiliary_security_reset ? 0u : reset_entry, false);
    apply_master_clear_sfr_reset(cpu, previous_sfr);
    for (index = 0u;
         index < sizeof(preserved_addresses) / sizeof(preserved_addresses[0]);
         index++) {
        uint16_t address = preserved_addresses[index];
        cpu->data[address] = (uint8_t)preserved_values[index];
        cpu->data[address + 1u] = (uint8_t)(preserved_values[index] >> 8u);
    }
    memcpy(cpu->io.adc, adc, sizeof(adc));
    memcpy(cpu->io.gpio, gpio, sizeof(gpio));
    memcpy(cpu->io.comparator.input, comparator_input, sizeof(comparator_input));
    memcpy(cpu->io.comparator.reference, comparator_reference,
           sizeof(comparator_reference));
    for (index = 0u; index < DSPIC33_DCI_BUFFER_COUNT; index++) {
        uint16_t address = (uint16_t)(0x0290u + index * 2u);
        cpu->data[address] = (uint8_t)dci_receive[index];
        cpu->data[address + 1u] = (uint8_t)(dci_receive[index] >> 8u);
    }
    cpu->io.dci.input = dci_input;
    memcpy(cpu->io.rtcc.calendar, rtcc.calendar, sizeof(rtcc.calendar));
    memcpy(cpu->io.rtcc.alarm, rtcc.alarm, sizeof(rtcc.alarm));
    cpu->io.rtcc.prescaler = rtcc.prescaler;
    cpu->io.rtcc.alarm_output = rtcc.alarm_output;
    cpu->io.rtcc.calibration_pending = rtcc.calibration_pending;
    cpu->io.uart_cts = uart_cts;
    cpu->io.spi_selected = spi_selected;
    cpu->io.timer_gate = timer_gate;
    cpu->io.pwm_dead_time_inputs = pwm_dead_time_inputs;
    cpu->io.pwm_sync_inputs = pwm_sync_inputs;
    cpu->io.output_compare.fault_direct_mask = output_compare_fault_direct_mask;
    cpu->io.output_compare.fault_inputs =
        (uint8_t)(output_compare_fault_inputs & output_compare_fault_direct_mask);
    cpu->io.pwm_fault_inputs = pwm_fault_inputs;
    cpu->io.pwm_current_limit_inputs = pwm_current_limit_inputs;
    cpu->io.usb_host_attached = usb_host_attached;
    cpu->instructions = instructions;
    cpu->cycles = cycles;
    cpu->device_cycles = device_cycles;
    cpu->io.auxiliary_pll_generation = auxiliary_pll_generation;
    if (auxiliary_pll_pending &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_AUX_PLL, 0u, auxiliary_pll_generation,
                          auxiliary_pll_remaining)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    oscillator.key_stage = 0u;
    cpu->oscillator = oscillator;
    if (oscillator_pending &&
        !dspic33_schedule(cpu, DSPIC33_EVENT_OSCILLATOR, oscillator_phase,
                          oscillator.generation, oscillator_remaining)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
    cpu->interrupt_count = interrupt_count;
    cpu->software_reset_count = software_reset_count;
    cpu->illegal_reset_count = illegal_reset_count;
    cpu->trap_count = trap_count;
    cpu->reset_interrupt = reset_interrupt;
    cpu->async_events_enabled = async_events_enabled;
    cpu->stop_on_trap = stop_on_trap;
    cpu->illegal_reset = kind == DSPIC33_RESET_ILLEGAL || auxiliary_security_reset;
    cpu->reset_locked = auxiliary_security_reset;
    rcon = (uint16_t)((rcon | cause | (auxiliary_security_reset ? 0x4000u : 0u)) &
                      0xcadfu);
    cpu->data[0x0740u] = (uint8_t)rcon;
    cpu->data[0x0741u] = (uint8_t)(rcon >> 8u);
    dspic33_device_reset_restored(cpu);
}

static bool watchdog_enabled(const Dspic33* cpu) {
    return (cpu->configuration[10u] & 0x80u) != 0u ||
           (cpu->data[0x0740u] & 0x20u) != 0u;
}

static uint32_t watchdog_period(const Dspic33* cpu) {
    uint8_t configuration = cpu->configuration[10u];
    uint32_t prescaler = (configuration & 0x10u) != 0u ? 128u : 32u;
    return prescaler << (configuration & 0x0fu);
}

static void watchdog_timeout(Dspic33* cpu) {
    cpu->watchdog.ticks = 0u;
    if (cpu->power_state != DSPIC33_POWER_ACTIVE) {
        cpu->data[0x0740u] |= 0x10u;
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        cpu->stop_reason = DSPIC33_RUNNING;
        return;
    }
    if (cpu->nvm.active) {
        cpu->watchdog.reset_pending = true;
        return;
    }
    perform_warm_reset(cpu, 0x0010u, DSPIC33_RESET_HARDWARE);
}

static void clear_watchdog(Dspic33* cpu) {
    uint32_t period = watchdog_period(cpu);
    if (watchdog_enabled(cpu) && (cpu->configuration[10u] & 0x40u) == 0u &&
        cpu->watchdog.ticks < period - period / 4u) {
        watchdog_timeout(cpu);
        return;
    }
    cpu->watchdog.ticks = 0u;
}

void dspic33_watchdog_advance_lprc(Dspic33* cpu, uint64_t ticks) {
    while (ticks != 0u && watchdog_enabled(cpu)) {
        uint32_t period = watchdog_period(cpu);
        uint32_t remaining = period - cpu->watchdog.ticks;
        if (ticks < remaining) {
            cpu->watchdog.ticks += (uint32_t)ticks;
            return;
        }
        ticks -= remaining;
        watchdog_timeout(cpu);
        if (cpu->watchdog.reset_pending) {
            return;
        }
    }
    if (!watchdog_enabled(cpu)) {
        cpu->watchdog.ticks = 0u;
    }
}

bool dspic33_watchdog_complete_nvm(Dspic33* cpu) {
    if (!cpu->watchdog.reset_pending) {
        return false;
    }
    cpu->watchdog.reset_pending = false;
    perform_warm_reset(cpu, 0x0010u, DSPIC33_RESET_HARDWARE);
    return true;
}

bool dspic33_complete_nvm_reset(Dspic33* cpu) {
    uint16_t cause;
    Dspic33ResetKind kind;
    if (!cpu->nvm.reset_pending) {
        return false;
    }
    cause = cpu->nvm.reset_cause;
    kind = (Dspic33ResetKind)cpu->nvm.reset_kind;
    cpu->nvm.reset_pending = false;
    perform_warm_reset(cpu, cause, kind);
    return true;
}

void dspic33_configuration_mismatch_reset(Dspic33* cpu) {
    perform_warm_reset(cpu, 0x0200u, DSPIC33_RESET_HARDWARE);
}

void dspic33_mclr_reset(Dspic33* cpu) {
    perform_warm_reset(cpu, 0x0080u, DSPIC33_RESET_HARDWARE);
}

void dspic33_brown_out_reset(Dspic33* cpu) {
    uint16_t previous_rcon = dspic33_read_word(cpu, 0x0740u);
    cpu->nvm.active = false;
    perform_warm_reset(cpu, 0u, DSPIC33_RESET_HARDWARE);
    previous_rcon = (uint16_t)((previous_rcon & 0x0081u) | 0x0002u |
                               (cpu->reset_locked ? 0x4000u : 0u));
    cpu->data[0x0740u] = (uint8_t)previous_rcon;
    cpu->data[0x0741u] = (uint8_t)(previous_rcon >> 8u);
    dspic33_device_brown_out_reset(cpu);
}

void dspic33_reset(Dspic33* cpu, uint32_t entry) {
    reset_processor(cpu, entry, true);
    dspic33_device_power_on_reset(cpu);
}

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

static void erase_program_words(uint32_t* words, uint32_t count) {
    uint32_t index;
    for (index = 0u; index < count; index++) {
        words[index] = 0x00ffffffu;
    }
}

void dspic33_complete_nvm(Dspic33* cpu) {
    uint16_t operation = (uint16_t)(cpu->nvm.control & 0x000fu);
    uint32_t target = cpu->nvm.address;
    uint32_t count;
    uint32_t index;
    if (operation == 0u) {
        uint8_t configuration_index;
        uint32_t offset;
        uint8_t current;
        uint8_t latch;
        uint8_t mask;
        if (!configuration_register_index(target, &configuration_index)) {
            return;
        }
        if (target ==
                DSPIC33_CONFIGURATION_BASE + CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET &&
            ((cpu->configuration[CODEGUARD_GENERAL_CONFIGURATION_OFFSET] & 0x33u) !=
                 0x03u ||
             (cpu->configuration[CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET] & 0x33u) !=
                 0x03u)) {
            return;
        }
        offset = target - DSPIC33_CONFIGURATION_BASE;
        mask = configuration_program_masks[configuration_index];
        current = cpu->configuration[offset];
        latch = (uint8_t)cpu->nvm.latches[0];
        if (configuration_index == 0u || configuration_index == 6u) {
            cpu->configuration[offset] =
                (uint8_t)((current & (uint8_t)~mask) | (latch & 0x30u) |
                          (current & latch & 0x03u));
        } else {
            cpu->configuration[offset] =
                (uint8_t)((current & (uint8_t)~mask) | (latch & mask));
        }
        dspic33_device_configuration_changed(cpu, target, current);
        return;
    }
    if (operation == 0x0au) {
        if (cpu->nvm.auxiliary_origin) {
            return;
        }
        erase_program_words(cpu->auxiliary_program, DSPIC33_AUXILIARY_PROGRAM_WORDS);
        cpu->configuration[0x10u] = configuration_factory_defaults[6u];
        cpu->configuration[0x11u] = 0xffu;
        return;
    }
    if (operation == 0x0du) {
        if (!cpu->nvm.auxiliary_origin) {
            return;
        }
        erase_program_words(cpu->program, DSPIC33_PROGRAM_WORDS);
        erase_program_words(
            cpu->persistent_program + PERSISTENT_PROGRAM_PHYSICAL_BASE / 2u,
            (PERSISTENT_PROGRAM_PHYSICAL_LIMIT - PERSISTENT_PROGRAM_PHYSICAL_BASE) /
                2u);
        cpu->configuration[4u] = configuration_factory_defaults[0u];
        cpu->configuration[5u] = 0xffu;
        return;
    }
    if (operation == 1u) {
        target &= 0x00fffffcu;
        count = 2u;
    } else if (operation == 2u) {
        target &= 0x00ffff00u;
        count = DSPIC33_WRITE_LATCH_WORDS;
    } else if (operation == 3u) {
        target &= 0x00fff800u;
        if (!codeguard_programming_allowed(cpu, target)) {
            return;
        }
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
    if (!codeguard_programming_allowed(cpu, target)) {
        return;
    }
    for (index = 0u; index < count; index++) {
        uint32_t* destination = writable_program_word(cpu, target + index * 2u);
        if (destination != NULL) {
            *destination &= cpu->nvm.latches[index];
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
    word = dspic33_read_program_word(cpu, word_address);
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
    if (!data_byte_is_implemented(address)) {
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
        write_working_register_byte(cpu, reg, (address & 1u) != 0u, value);
        return;
    }
    if (accumulator_byte_location(address, &accumulator, &accumulator_byte)) {
        write_accumulator_byte(cpu, accumulator, accumulator_byte, value);
        return;
    }
    if (address >= 0x0020u && address <= 0x0055u) {
        uint16_t* reg = NULL;
        uint16_t word_address = (uint16_t)(address & 0xfffeu);
        if (word_address == 0x002eu || word_address == 0x0030u) {
            return;
        }
        if (word_address == 0x0042u) {
            write_status_byte(cpu, (address & 1u) != 0u, value);
            return;
        }
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
        if (word_address == 0x0052u) {
            write_disicnt_byte(cpu, (address & 1u) != 0u, value);
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
        default:
            break;
        }
        if (reg != NULL) {
            if ((address & 1u) == 0u) {
                *reg = (uint16_t)((*reg & 0xff00u) | value);
            } else {
                *reg = (uint16_t)((*reg & 0x00ffu) | ((uint16_t)value << 8u));
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
    bool low_implemented;
    bool high_implemented;
    uint16_t previous;
    if (!check_data_alignment(cpu, address) ||
        !check_data_implementation(cpu, address, 2u) ||
        (cpu->address_error && !cpu->address_error_access_allowed) ||
        address + 1u >= DSPIC33_DATA_SIZE) {
        return;
    }
    low_implemented = data_byte_is_implemented(address);
    high_implemented = data_byte_is_implemented(address + 1u);
    if (!low_implemented || !high_implemented) {
        if (low_implemented) {
            dspic33_write_byte(cpu, address, (uint8_t)value);
        }
        if (high_implemented) {
            dspic33_write_byte(cpu, address + 1u, (uint8_t)(value >> 8u));
        }
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
    if (address < 32u) {
        write_working_register(cpu, (uint8_t)(address / 2u), value);
        return;
    }
    if (address >= 0x0020u && address <= 0x0054u) {
        if (address == 0x0052u) {
            if (cpu->disicnt != 0u) {
                cpu->disicnt = (uint16_t)(value & 0x3fffu);
            }
            return;
        }
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

static uint8_t read_byte_value(Dspic33* cpu, uint32_t address) {
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
        if (!dspic33_program_range_implemented(program_address & ~1u, 2u)) {
            if (cpu->instruction_active &&
                program_target_requires_address_error(program_address & ~1u)) {
                raise_program_read_error(cpu);
            }
            return 0u;
        }
        word = read_cpu_program_word(cpu, program_address & ~1u);
        if ((address & PSV_HIGH_BYTE) != 0u) {
            return (program_address & 1u) == 0u ? (uint8_t)(word >> 16u) : 0u;
        }
        return (uint8_t)(word >> ((program_address & 1u) * 8u));
    }
    if (address >= DSPIC33_DATA_SIZE) {
        return 0u;
    }
    if (!data_byte_is_implemented(address)) {
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
    case 0x002eu:
        value = (uint16_t)cpu->pc;
        break;
    case 0x0030u:
        value = (uint16_t)((cpu->pc >> 16u) & 0x007fu);
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

uint8_t dspic33_read_byte(Dspic33* cpu, uint32_t address) {
    uint8_t value;
    cpu->io.cpu_read_address = address;
    cpu->io.cpu_read_width = 1u;
    cpu->io.cpu_read_valid = true;
    value = read_byte_value(cpu, address);
    cpu->io.cpu_read_valid = false;
    return value;
}

uint16_t dspic33_read_word(Dspic33* cpu, uint32_t address) {
    uint16_t low;
    uint16_t high;
    if (!check_data_alignment(cpu, address) ||
        !check_data_implementation(cpu, address, 2u) ||
        (cpu->address_error && !cpu->address_error_access_allowed)) {
        return 0u;
    }
    cpu->io.cpu_read_address = address;
    cpu->io.cpu_read_width = 2u;
    cpu->io.cpu_read_valid = true;
    low = read_byte_value(cpu, address);
    high = read_byte_value(cpu, address + 1u);
    cpu->io.cpu_read_valid = false;
    return (uint16_t)(low | (high << 8u));
}

static void complete_do_loop_iteration(Dspic33* cpu) {
    uint8_t depth = (uint8_t)(cpu->do_depth - 1u);
    if (cpu->do_count[depth] != 0u && cpu->do_terminate[depth] == 0u) {
        cpu->do_count[depth]--;
        cpu->dcount = cpu->do_count[depth];
        cpu->pc = cpu->do_start[depth];
        return;
    }
    cpu->do_terminate[depth] = 0u;
    cpu->do_depth--;
    cpu->corcon =
        (uint16_t)((cpu->corcon & ~0x0700u) | ((uint16_t)cpu->do_depth << 8u));
    if (cpu->do_depth == 0u) {
        cpu->dostart = 0u;
        cpu->doend = 0u;
        cpu->dcount = 0u;
        cpu->sr &= (uint16_t)~0x0200u;
        return;
    }
    depth = (uint8_t)(cpu->do_depth - 1u);
    cpu->dostart = cpu->do_start[depth];
    cpu->doend = cpu->do_end[depth];
    cpu->dcount = cpu->do_count[depth];
}

Dspic33StopReason dspic33_step(Dspic33* cpu) {
    uint16_t working_registers[16];
    uint16_t initialized_working_registers;
    int64_t accumulators[2];
    uint16_t status;
    uint16_t control;
    uint16_t disicnt_before_instruction;
    uint8_t interrupt_entry_overlap;
    uint64_t cycles;
    uint32_t opcode;
    uint32_t instruction_pc;
    bool exception_dispatched;
    bool sequential_hole_fetch;
    bool non_cpu_sfr_wait;
    bool power_save_next;
    bool psv_repeat_access;
    bool psv_repeat_exit_latency;
    uint64_t base_cycles;
    uint64_t device_ratio;
    if (cpu->reset_locked) {
        cpu->illegal_reset = true;
        return cpu->stop_reason;
    }
    cpu->illegal_reset = false;
    if (cpu->nvm.active && cpu->nvm.reset_pending) {
        advance_pending_nvm_reset(cpu);
        return cpu->stop_reason;
    }
    if (cpu->nvm.active && nvm_stalls_cpu(cpu)) {
        if (!dspic33_device_advance_nvm(cpu)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        return cpu->stop_reason;
    }
    if (cpu->power_state != DSPIC33_POWER_ACTIVE) {
        if (!dspic33_device_wake(cpu)) {
            cpu->stop_reason = cpu->power_state == DSPIC33_POWER_SLEEP
                                   ? DSPIC33_SLEEPING
                                   : DSPIC33_IDLING;
            return cpu->stop_reason;
        }
        if (cpu->illegal_reset) {
            if (cpu->nvm.reset_pending) {
                advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        }
        cpu->previous_working_register_writes = 0u;
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        cpu->watchdog.ticks = 0u;
        cpu->stop_reason = DSPIC33_RUNNING;
    } else {
        power_save_next =
            !cpu->nvm.active && !vector_segment_execution_address(cpu->pc) &&
            (cpu->pc & 1u) == 0u && dspic33_program_range_implemented(cpu->pc, 2u) &&
            (dspic33_read_program_word(cpu, cpu->pc) & 0xfffffeu) == 0xfe4000u;
        exception_dispatched = service_pending_soft_trap(cpu);
        if (!exception_dispatched && !power_save_next) {
            exception_dispatched = dspic33_device_service_interrupt(cpu);
        }
        if (cpu->illegal_reset) {
            if (cpu->nvm.reset_pending) {
                advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        }
        if (exception_dispatched) {
            cpu->sequential_program_hole_pc = 0u;
            cpu->previous_working_register_writes = 0u;
        }
    }
    cpu->interrupt_entry_overlap = 0u;
    sequential_hole_fetch = cpu->sequential_program_hole_pc == cpu->pc &&
                            program_target_requires_address_error(cpu->pc);
    if (vector_segment_execution_address(cpu->pc)) {
        uint32_t return_pc = program_address_add(cpu->pc, 2);
        device_ratio = dspic33_device_instruction_cycles(cpu, 1u);
        cpu->sequential_program_hole_pc = 0u;
        enter_address_trap(cpu, return_pc);
        if (!cpu->illegal_reset || cpu->nvm.reset_pending) {
            advance_instruction(cpu, 1u, false, device_ratio);
        }
        return cpu->stop_reason;
    }
    if ((cpu->pc & 1u) != 0u ||
        (!dspic33_program_range_implemented(cpu->pc, 2u) && !sequential_hole_fetch)) {
        cpu->sequential_program_hole_pc = 0u;
        cpu->stop_reason = DSPIC33_PROGRAM_BOUNDS;
        return cpu->stop_reason;
    }
    instruction_pc = cpu->pc;
    device_ratio = dspic33_device_instruction_cycles(cpu, 1u);
    opcode = sequential_hole_fetch ? 0u : dspic33_read_program_word(cpu, cpu->pc);
    cpu->sequential_program_hole_pc = 0u;
    if (opcode == 0x064000u) {
        uint64_t cycles;
        uint32_t target;
        bool dependency_stall =
            (cpu->previous_working_register_writes & UINT16_C(0x8000)) != 0u;
        dspic33_device_return_interrupt(cpu);
        target = cpu->pc;
        cpu->instructions++;
        if (program_target_requires_address_error(target)) {
            enter_address_trap(cpu, program_address_add(instruction_pc, 2));
            cycles = 5u;
        } else if (!dspic33_codeguard_admit_program_flow(cpu, instruction_pc, target)) {
            if (cpu->nvm.reset_pending) {
                advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        } else {
            cycles = exception_pending(cpu) ? 5u : 6u;
        }
        cycles += dependency_stall ? 1u : 0u;
        cpu->previous_working_register_writes = 0u;
        advance_instruction(cpu, cycles, false, device_ratio);
        return cpu->stop_reason;
    }
    if (opcode == 0x060000u) {
        uint64_t cycles;
        uint32_t target;
        bool dependency_stall =
            (cpu->previous_working_register_writes & UINT16_C(0x8000)) != 0u;
        if (cpu->call_depth == 0u) {
            cpu->stop_reason = DSPIC33_RETURNED;
            return cpu->stop_reason;
        }
        target = pop_program_counter(cpu);
        cpu->pc = target;
        cpu->instructions++;
        if (program_target_requires_address_error(target)) {
            enter_address_trap(cpu, program_address_add(instruction_pc, 2));
            cycles = 5u;
        } else if (!dspic33_codeguard_admit_program_flow(cpu, instruction_pc, target)) {
            if (cpu->nvm.reset_pending) {
                advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        } else {
            cycles = exception_pending(cpu) ? 5u : 6u;
        }
        cycles += dependency_stall ? 1u : 0u;
        cpu->previous_working_register_writes = 0u;
        advance_instruction(cpu, cycles, false, device_ratio);
        return cpu->stop_reason;
    }
    memcpy(working_registers, cpu->w, sizeof(working_registers));
    initialized_working_registers = cpu->initialized_working_registers;
    memcpy(accumulators, cpu->accumulator, sizeof(accumulators));
    status = cpu->sr;
    control = cpu->corcon;
    cpu->pc = program_address_add(cpu->pc, 2);
    cpu->instructions++;
    cpu->non_cpu_sfr_read = false;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    cpu->instruction_working_register_writes = 0u;
    cpu->instruction_source_address_registers = 0u;
    cpu->current_instruction_cycles =
        (uint8_t)instruction_cycles(cpu, opcode, instruction_pc);
    cpu->current_instruction_pc = instruction_pc;
    cpu->instruction_active = true;
    cpu->address_error = false;
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = false;
    cpu->address_error_accumulator_state_completed = false;
    cpu->address_error_control_state_completed = false;
    if (!execute(cpu, opcode) && !cpu->address_error && !cpu->illegal_reset) {
        cpu->instruction_active = false;
        cpu->current_instruction_cycles = 0u;
        cpu->instruction_working_register_writes = 0u;
        cpu->instruction_source_address_registers = 0u;
        cpu->non_cpu_sfr_read = false;
        cpu->psv_read = false;
        cpu->psv_repeat_optimized = false;
        cpu->pc -= 2u;
        if (cpu->stop_reason == DSPIC33_RUNNING) {
            cpu->unsupported_opcode = opcode;
            cpu->stop_reason = DSPIC33_UNSUPPORTED_INSTRUCTION;
        }
        return cpu->stop_reason;
    }
    if (!cpu->address_error && !cpu->illegal_reset &&
        cpu->pc !=
            program_address_add(instruction_pc, (int32_t)instruction_length(opcode))) {
        if (!dspic33_codeguard_admit_program_flow(cpu, instruction_pc, cpu->pc)) {
            cpu->instruction_active = false;
            if (cpu->nvm.reset_pending) {
                advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        }
    }
    cpu->instruction_active = false;
    if (cpu->illegal_reset && !cpu->nvm.reset_pending) {
        return cpu->stop_reason;
    }
    base_cycles = instruction_cycles(cpu, opcode, instruction_pc);
    psv_repeat_access = cpu->psv_read && cpu->repeat_active != 0u;
    if (cpu->psv_repeat_optimized) {
        if (cpu->repeat_psv_reentry) {
            cpu->repeat_psv_reentry = false;
            cpu->repeat_psv_started = true;
            cycles = 5u;
        } else if (!cpu->repeat_psv_started) {
            cpu->repeat_psv_started = true;
            cycles = 5u;
        } else {
            cycles = cpu->rcount == 0u ? 6u : 1u;
        }
    } else {
        cycles = cpu->psv_read ? 5u : base_cycles + (cpu->non_cpu_sfr_read ? 1u : 0u);
    }
    if (!psv_repeat_access && (cpu->previous_working_register_writes &
                               cpu->instruction_source_address_registers) != 0u) {
        cycles++;
    }
    psv_repeat_exit_latency = cpu->psv_repeat_optimized && cycles == 1u;
    non_cpu_sfr_wait = cpu->non_cpu_sfr_read;
    disicnt_before_instruction = cpu->disicnt;
    interrupt_entry_overlap = 1u;
    if (cpu->psv_read && !psv_repeat_access) {
        interrupt_entry_overlap += (opcode & 0xff0000u) == 0xbe0000u ? 1u : 3u;
    }
    cpu->current_instruction_cycles = 0u;
    cpu->non_cpu_sfr_read = false;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    if (cpu->address_error) {
        uint32_t return_pc = cpu->address_error_return;
        bool control_state_completed = cpu->address_error_control_state_completed;
        if (!cpu->address_error_working_state_completed) {
            memcpy(cpu->w, working_registers, sizeof(working_registers));
            cpu->initialized_working_registers = initialized_working_registers;
            cpu->sr = status;
        }
        if (!cpu->address_error_accumulator_state_completed) {
            memcpy(cpu->accumulator, accumulators, sizeof(accumulators));
        }
        if (!cpu->address_error_control_state_completed) {
            cpu->corcon = control;
        }
        cpu->address_error = false;
        cpu->address_error_access_allowed = false;
        cpu->address_error_working_state_completed = false;
        cpu->address_error_accumulator_state_completed = false;
        cpu->address_error_control_state_completed = false;
        cpu->previous_working_register_writes = 0u;
        if (control_state_completed) {
            enter_address_trap(cpu, return_pc);
        } else {
            enter_trap(cpu, 1u, 0x000006u, 14u, 0x0008u, return_pc, false);
        }
        if (cpu->illegal_reset && !cpu->nvm.reset_pending) {
            return cpu->stop_reason;
        }
        advance_instruction(cpu, cycles, non_cpu_sfr_wait, device_ratio);
        return cpu->stop_reason;
    }
    cpu->previous_working_register_writes = cpu->instruction_working_register_writes;
    if (cpu->repeat_active != 0u && instruction_pc == cpu->repeat_pc) {
        if (cpu->rcount != 0u) {
            cpu->rcount--;
            cpu->pc = cpu->repeat_pc;
            if (cpu->rcount == 0u) {
                cpu->sr &= (uint16_t)~0x0010u;
            }
        } else {
            cpu->repeat_active = 0u;
            cpu->repeat_psv_started = false;
            cpu->repeat_psv_reentry = false;
            cpu->sr &= (uint16_t)~0x0010u;
        }
    }
    if (cpu->do_depth != 0u && instruction_pc == cpu->do_end[cpu->do_depth - 1u]) {
        uint8_t depth = (uint8_t)(cpu->do_depth - 1u);
        bool extra_decrement = cpu->nested_do_extra_decrement_depth == cpu->do_depth &&
                               cpu->nested_do_extra_decrement_end == instruction_pc;
        uint32_t sequential_pc = cpu->pc;
        if (cpu->nested_do_interrupt_armed &&
            cpu->nested_do_interrupt_depth == cpu->do_depth &&
            cpu->nested_do_interrupt_end == instruction_pc) {
            cpu->nested_do_interrupt_cycle = 0u;
            cpu->nested_do_interrupt_end = 0u;
            cpu->nested_do_interrupt_depth = 0u;
            cpu->nested_do_interrupt_priority = 0u;
            cpu->nested_do_interrupt_armed = false;
        }
        if (extra_decrement) {
            cpu->nested_do_extra_decrement_depth = 0u;
            cpu->nested_do_extra_decrement_end = 0u;
        }
        complete_do_loop_iteration(cpu);
        if (extra_decrement && cpu->do_depth == (uint8_t)(depth + 1u) &&
            cpu->do_end[depth] == instruction_pc) {
            cpu->pc = sequential_pc;
            complete_do_loop_iteration(cpu);
        }
    }
    cpu->sequential_program_hole_pc =
        (cpu->sequential_program_hole_pc == cpu->pc ||
         cpu->pc == instruction_pc + instruction_length(opcode) ||
         (sequential_hole_fetch && cpu->repeat_active != 0u &&
          cpu->pc == instruction_pc)) &&
                program_target_requires_address_error(cpu->pc)
            ? cpu->pc
            : 0u;
    cpu->instruction_advancing = true;
    advance_instruction(cpu, cycles, non_cpu_sfr_wait, device_ratio);
    if (psv_repeat_exit_latency && cpu->repeat_active != 0u &&
        dspic33_device_interrupt_pending(cpu)) {
        advance_instruction(cpu, 4u, false, device_ratio);
    }
    if (disicnt_before_instruction != 0u && cpu->disicnt == 0u &&
        dspic33_device_interrupt_pending(cpu)) {
        cpu->interrupt_entry_overlap = interrupt_entry_overlap;
    }
    cpu->instruction_advancing = false;
    if (cpu->power_state != DSPIC33_POWER_ACTIVE && dspic33_device_wake(cpu)) {
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        cpu->watchdog.ticks = 0u;
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
