#include "internal.h"

const uint8_t dspic33_internal_configuration_factory_defaults[8] = {
    0xcfu, 0xffu, 0xffu, 0xffu, 0xffu, 0xdfu, 0xcfu, 0xffu,
};

const uint8_t dspic33_internal_configuration_program_masks[8] = {
    0x33u, 0x87u, 0xe7u, 0xffu, 0x3fu, 0xf7u, 0x33u, 0xffu,
};

void dspic33_internal_reset_processor(Dspic33* cpu, uint32_t entry, bool clear_memory);
void dspic33_internal_perform_warm_reset(Dspic33* cpu, uint16_t cause, Dspic33ResetKind kind);
void dspic33_internal_clear_watchdog(Dspic33* cpu);
void dspic33_internal_enter_trap(Dspic33* cpu, uint16_t trap, uint32_t vector, uint8_t priority,
                                 uint16_t status, uint32_t return_pc, bool auxiliary_vector);
void dspic33_internal_enter_address_trap(Dspic33* cpu, uint32_t return_pc);
void dspic33_internal_schedule_soft_trap(Dspic33* cpu, uint16_t trap, uint32_t vector,
                                         uint8_t priority, uint8_t delay);
void dspic33_internal_check_stack_address(Dspic33* cpu, int32_t stack_address, bool limit_wrapped);
static uint16_t stored_word(const Dspic33* cpu, uint16_t address);

bool dspic33_program_range_implemented(uint32_t address, uint32_t size) {
    return (address < DSPIC33_PROGRAM_LIMIT && size <= DSPIC33_PROGRAM_LIMIT - address) ||
           (address >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
            address < DSPIC33_AUXILIARY_PROGRAM_LIMIT &&
            size <= DSPIC33_AUXILIARY_PROGRAM_LIMIT - address);
}

bool dspic33_device_program_range_implemented(const Dspic33* cpu, uint32_t address, uint32_t size) {
    const Dspic33epMuProfile* profile = dspic33_device_profile(cpu);
    return profile != NULL &&
           ((address < profile->program_limit && size <= profile->program_limit - address) ||
            (address >= DSPIC33_AUXILIARY_PROGRAM_BASE &&
             address < DSPIC33_AUXILIARY_PROGRAM_LIMIT &&
             size <= DSPIC33_AUXILIARY_PROGRAM_LIMIT - address));
}

bool dspic33_data_range_valid(uint32_t address, uint32_t size) {
    return address <= DSPIC33_DATA_SIZE && size <= DSPIC33_DATA_SIZE - address;
}

bool dspic33_device_data_range_implemented(const Dspic33* cpu, uint32_t address, uint32_t size) {
    const Dspic33epMuProfile* profile = dspic33_device_profile(cpu);
    return profile != NULL && address <= profile->data_limit &&
           size <= profile->data_limit - address;
}

bool dspic33_internal_program_target_requires_address_error(const Dspic33* cpu, uint32_t address) {
    return address < DSPIC33_AUXILIARY_PROGRAM_LIMIT &&
           !dspic33_device_program_range_implemented(cpu, address, 2u);
}

uint32_t dspic33_internal_device_program_limit(const Dspic33* cpu) {
    const Dspic33epMuProfile* profile = dspic33_device_profile(cpu);
    return profile == NULL ? 0u : profile->program_limit;
}

bool dspic33_internal_vector_segment_execution_address(uint32_t address) {
    return (address & 1u) == 0u && address >= VECTOR_SEGMENT_EXECUTION_BASE &&
           address < VECTOR_SEGMENT_LIMIT;
}

uint32_t dspic33_internal_program_address_add(uint32_t address, int32_t offset) {
    return (uint32_t)((address + offset) & 0x007ffffeu);
}

bool dspic33_internal_auxiliary_program_address(uint32_t address) {
    return address >= DSPIC33_AUXILIARY_PROGRAM_BASE && address < DSPIC33_AUXILIARY_PROGRAM_LIMIT;
}

static bool persistent_program_tagged_address(uint32_t address) {
    return address >= DSPIC33_PERSISTENT_PROGRAM_BASE && address < DSPIC33_PERSISTENT_PROGRAM_LIMIT;
}

uint8_t dspic33_internal_codeguard_configuration(const Dspic33* cpu, bool is_auxiliary) {
    return cpu->configuration[is_auxiliary ? CODEGUARD_AUXILIARY_CONFIGURATION_OFFSET
                                           : CODEGUARD_GENERAL_CONFIGURATION_OFFSET];
}

static bool codeguard_write_protected(uint8_t configuration) {
    return (configuration & 0x01u) == 0u;
}

bool dspic33_internal_codeguard_high_security(uint8_t configuration) {
    const uint8_t security_level = (uint8_t)(configuration & 0x03u);
    const uint8_t required_key = security_level == 0x03u ? 0u : 0x30u;
    return (security_level & 0x02u) == 0u || (configuration & 0x30u) != required_key;
}

bool dspic33_internal_codeguard_programming_allowed(const Dspic33* cpu, uint32_t target) {
    const bool is_auxiliary_target = dspic33_internal_auxiliary_program_address(target);
    const uint8_t codeguard_value =
        dspic33_internal_codeguard_configuration(cpu, is_auxiliary_target);
    if (codeguard_write_protected(codeguard_value)) {
        return false;
    }
    if (dspic33_internal_codeguard_high_security(codeguard_value)) {
        if (!is_auxiliary_target && (target & 0x00fff800u) == 0u) {
            return false;
        }
        if (cpu->nvm.auxiliary_origin != is_auxiliary_target) {
            return false;
        }
    }
    return true;
}

static bool codeguard_program_read_allowed(const Dspic33* cpu, uint32_t target) {
    bool is_auxiliary_origin;
    bool is_auxiliary_target;
    if (persistent_program_tagged_address(target)) {
        target -= DSPIC33_PERSISTENT_PROGRAM_BASE;
    }
    if (!cpu->instruction_active || !dspic33_device_program_range_implemented(cpu, target, 2u)) {
        return true;
    }
    is_auxiliary_origin = dspic33_internal_auxiliary_program_address(cpu->current_instruction_pc);
    is_auxiliary_target = dspic33_internal_auxiliary_program_address(target);
    return is_auxiliary_origin == is_auxiliary_target ||
           !dspic33_internal_codeguard_high_security(
               dspic33_internal_codeguard_configuration(cpu, is_auxiliary_target));
}

bool dspic33_codeguard_admit_program_flow(Dspic33* cpu, uint32_t origin, uint32_t target) {
    bool restricted = !dspic33_internal_auxiliary_program_address(origin) &&
                      dspic33_internal_auxiliary_program_address(target) &&
                      target < DSPIC33_AUXILIARY_PROGRAM_LIMIT - 64u &&
                      dspic33_internal_codeguard_high_security(
                          dspic33_internal_codeguard_configuration(cpu, true));
    if (restricted) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
    }
    return !restricted;
}

uint32_t dspic33_internal_read_cpu_program_word(const Dspic33* cpu, uint32_t address) {
    return codeguard_program_read_allowed(cpu, address) ? dspic33_read_program_word(cpu, address)
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

bool dspic33_internal_nvm_stalls_cpu(const Dspic33* cpu) {
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
        auxiliary_target = dspic33_internal_auxiliary_program_address(cpu->nvm.address);
    }
    return auxiliary_target == dspic33_internal_auxiliary_program_address(cpu->pc);
}

bool dspic33_internal_nvm_stall_erratum_applies(const Dspic33* cpu) {
    return (cpu->nvm.control & 0x000fu) != 0u;
}

bool dspic33_internal_do_flash_access_boundary(const Dspic33* cpu, uint32_t opcode,
                                               uint32_t instruction_pc, bool psv_read) {
    uint8_t depth;
    if (!psv_read && (opcode & 0xfe0000u) != 0xba0000u) {
        return false;
    }
    for (depth = 0u; depth < cpu->do_depth; depth++) {
        if (instruction_pc == cpu->do_start[depth] || instruction_pc == cpu->do_end[depth]) {
            return true;
        }
    }
    return false;
}

void dspic33_internal_advance_pending_nvm_reset(Dspic33* cpu) {
    if (!dspic33_device_advance_nvm(cpu)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
    }
}

uint32_t dspic33_internal_instruction_length(uint32_t opcode) {
    return (opcode & 0xff0000u) == 0x020000u || (opcode & 0xff0000u) == 0x040000u ||
                   (opcode & 0xff0000u) == 0x080000u
               ? 4u
               : 2u;
}

bool dspic33_internal_nested_zero_do_workaround_present(const Dspic33* cpu, uint32_t inner_start) {
    uint32_t outer_start = cpu->do_start[cpu->do_depth - 1u];
    return dspic33_read_program_word(cpu, outer_start) == 0u &&
           dspic33_read_program_word(cpu, dspic33_internal_program_address_add(outer_start, 2)) ==
               0u &&
           dspic33_read_program_word(cpu, inner_start) == 0u;
}

void dspic33_cancel_flash_read_sequence(Dspic33* cpu) {
    cpu->flash_read_connecting_words = 0u;
    cpu->flash_read_erratum_armed = false;
    cpu->flash_read_erratum_candidate = false;
    cpu->flash_read_connecting_ends_repeat = false;
}

void dspic33_internal_raise_program_target_error(Dspic33* cpu, uint32_t return_pc) {
    cpu->address_error = true;
    cpu->address_error_return = return_pc;
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = true;
    cpu->address_error_control_state_completed = true;
}

void dspic33_internal_raise_program_read_error(Dspic33* cpu) {
    if (!cpu->address_error) {
        cpu->address_error = true;
        cpu->address_error_return = cpu->pc;
    }
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = true;
    cpu->address_error_control_state_completed = true;
}

bool dspic33_internal_check_data_alignment(Dspic33* cpu, uint32_t address) {
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

bool dspic33_internal_data_byte_is_implemented(const Dspic33* cpu, uint32_t address) {
    return address >= 0x1000u ? address < DSPIC33_DATA_SIZE
                              : dspic33ep_mu_address_implemented(cpu->device, address);
}

bool dspic33_internal_check_data_implementation(Dspic33* cpu, uint32_t address, uint8_t width) {
    bool implemented =
        (address & PSV_ADDRESS) != 0u || dspic33_device_data_range_implemented(cpu, address, width);
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

void dspic33_internal_raise_data_page_error(Dspic33* cpu) {
    if (!cpu->address_error) {
        cpu->address_error = true;
        cpu->address_error_return = cpu->pc;
        cpu->address_error_access_allowed = true;
    }
    cpu->address_error_working_state_completed = true;
}

void dspic33_internal_write_working_register(Dspic33* cpu, uint8_t reg, uint16_t value) {
    cpu->w[reg] = reg == 15u ? (uint16_t)(value & 0xfffeu) : value;
    cpu->initialized_working_registers |= (uint16_t)(1u << reg);
    if (cpu->instruction_active) {
        cpu->instruction_working_register_writes |= (uint16_t)(1u << reg);
    }
}

void dspic33_internal_write_working_register_byte(Dspic33* cpu, uint8_t reg, bool high,
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

void dspic33_internal_record_source_address_register(Dspic33* cpu, uint8_t reg) {
    if (cpu->instruction_active) {
        cpu->instruction_source_address_registers |= (uint16_t)(1u << reg);
    }
}

void dspic33_set_working_register(Dspic33* cpu, uint8_t reg, uint16_t value) {
    if (reg < 16u) {
        dspic33_internal_write_working_register(cpu, reg, value);
    }
}

bool dspic33_internal_address_register_initialized(Dspic33* cpu, uint8_t reg) {
    if ((cpu->initialized_working_registers & (uint16_t)(1u << reg)) != 0u) {
        return true;
    }
    dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
    return false;
}

bool dspic33_internal_accumulator_byte_location(uint32_t address, uint8_t* accumulator,
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

int64_t dspic33_internal_accumulator_value(uint64_t bits) {
    bits &= DSPIC33_ACCUMULATOR_MASK;
    return (int64_t)bits - ((bits & 0x8000000000u) != 0u ? 0x10000000000ll : 0ll);
}

static void update_accumulator_combined_status(Dspic33* cpu) {
    cpu->sr &= 0xf3ffu;
    if ((cpu->sr & 0xc000u) != 0u) {
        cpu->sr |= 0x0800u;
    }
    if ((cpu->sr & 0x3000u) != 0u) {
        cpu->sr |= 0x0400u;
    }
}

void dspic33_internal_write_status_byte(Dspic33* cpu, bool high_byte, uint8_t value) {
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
        accumulator_status &= 0x3fffu;
    }
    if ((requested & 0x0400u) == 0u) {
        accumulator_status &= 0xcfffu;
    }
    cpu->sr = (uint16_t)((previous & 0x02ffu) | (requested & 0x0100u) | accumulator_status);
    update_accumulator_combined_status(cpu);
}

void dspic33_internal_write_disicnt_byte(Dspic33* cpu, bool high_byte, uint8_t value) {
    uint16_t requested;
    if (cpu->disicnt == 0u) {
        return;
    }
    requested = high_byte ? (uint16_t)((cpu->disicnt & 0x00ffu) | ((uint16_t)value << 8u))
                          : (uint16_t)((cpu->disicnt & 0xff00u) | value);
    cpu->disicnt = (uint16_t)(requested & 0x3fffu);
}

void dspic33_internal_apply_accumulator_result(Dspic33* cpu, uint8_t accumulator, int64_t result) {
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

    cpu->accumulator[accumulator] = dspic33_internal_accumulator_value((uint64_t)result);
    bool overflow =
        cpu->accumulator[accumulator] < INT32_MIN || cpu->accumulator[accumulator] > INT32_MAX;
    cpu->sr &= (uint16_t)~overflow_flag;
    if (overflow) {
        cpu->sr |= overflow_flag;
    }
    if (saturation_status) {
        cpu->sr |= saturation_flag;
    }
    update_accumulator_combined_status(cpu);
    uint16_t interrupt_control = stored_word(cpu, 0x08c0u);
    uint16_t error_flags = 0u;
    uint16_t overflow_enable = accumulator == 0u ? 0x0400u : 0x0200u;
    uint16_t overflow_error = accumulator == 0u ? 0x4000u : 0x2000u;
    uint16_t catastrophic_error = accumulator == 0u ? 0x1000u : 0x0800u;
    if (overflow && (interrupt_control & overflow_enable) != 0u) {
        error_flags |= overflow_error;
    }
    if (accumulator_overflow && (cpu->corcon & saturation_enable) == 0u &&
        (interrupt_control & 0x0100u) != 0u) {
        error_flags |= catastrophic_error;
    }
    if (error_flags != 0u) {
        dspic33_device_latch_math_error(cpu, error_flags);
    }
}

void dspic33_internal_clear_accumulator_status(Dspic33* cpu, uint8_t accumulator) {
    cpu->sr &= accumulator == 0u ? 0x5fffu : 0xafffu;
    update_accumulator_combined_status(cpu);
}

uint8_t dspic33_internal_read_accumulator_byte(const Dspic33* cpu, uint8_t accumulator,
                                               uint8_t byte) {
    if (byte == 5u) {
        return ((uint64_t)cpu->accumulator[accumulator] & 0x8000000000u) != 0u ? 0xffu : 0u;
    }
    return (uint8_t)((uint64_t)cpu->accumulator[accumulator] >> (byte * 8u));
}

void dspic33_internal_write_accumulator_byte(Dspic33* cpu, uint8_t accumulator, uint8_t byte,
                                             uint8_t value) {
    uint64_t bits;
    uint64_t mask;
    if (byte >= 5u) {
        return;
    }
    bits = (uint64_t)cpu->accumulator[accumulator] & DSPIC33_ACCUMULATOR_MASK;
    mask = (uint64_t)0xffu << (byte * 8u);
    bits = (bits & ~mask) | ((uint64_t)value << (byte * 8u));
    cpu->accumulator[accumulator] = dspic33_internal_accumulator_value(bits);
}

bool dspic33_internal_configuration_register_index(uint32_t address, uint8_t* index) {
    if (address < DSPIC33_CONFIGURATION_BASE + 4u || address > DSPIC33_CONFIGURATION_BASE + 0x12u ||
        (address & 1u) != 0u) {
        return false;
    }
    *index = (uint8_t)((address - DSPIC33_CONFIGURATION_BASE - 4u) / 2u);
    return *index < sizeof(dspic33_internal_configuration_factory_defaults);
}

uint32_t dspic33_read_program_word(const Dspic33* cpu, uint32_t address) {
    uint8_t configuration_index;
    if (cpu == NULL) {
        return 0x00ffffffu;
    }
    if (persistent_program_physical_address(address)) {
        return cpu->persistent_program[persistent_program_index(address)] & 0x00ffffffu;
    }
    if (dspic33_device_program_range_implemented(cpu, address, 2u) &&
        address < DSPIC33_PROGRAM_LIMIT) {
        return cpu->program[address / 2u] & 0x00ffffffu;
    }
    if (address >= DSPIC33_AUXILIARY_PROGRAM_BASE && address < DSPIC33_AUXILIARY_PROGRAM_LIMIT) {
        return cpu->auxiliary_program[(address - DSPIC33_AUXILIARY_PROGRAM_BASE) / 2u] &
               0x00ffffffu;
    }
    if (address == DSPIC33_CONFIGURATION_BASE || address == DSPIC33_CONFIGURATION_BASE + 2u) {
        return 0u;
    }
    if (dspic33_internal_configuration_register_index(address, &configuration_index)) {
        return cpu->configuration[4u + configuration_index * 2u];
    }
    if (address == 0xff0000u) {
        return dspic33_device_profile(cpu)->device_id >> 16u;
    }
    if (address == 0xff0002u) {
        return 0x004002u;
    }
    if (persistent_program_tagged_address(address)) {
        return cpu->persistent_program[persistent_program_index(address)] & 0x00ffffffu;
    }
    if (address >= DSPIC33_WRITE_LATCH_BASE && address < DSPIC33_WRITE_LATCH_LIMIT) {
        return cpu->write_latches[(address - DSPIC33_WRITE_LATCH_BASE) / 2u] & 0x00ffffffu;
    }
    return 0x00ffffffu;
}

uint32_t* dspic33_internal_writable_program_word(Dspic33* cpu, uint32_t address) {
    if (persistent_program_physical_address(address)) {
        return &cpu->persistent_program[persistent_program_index(address)];
    }
    if (dspic33_device_program_range_implemented(cpu, address, 2u) &&
        address < DSPIC33_PROGRAM_LIMIT) {
        return &cpu->program[address / 2u];
    }
    if (address >= DSPIC33_AUXILIARY_PROGRAM_BASE && address < DSPIC33_AUXILIARY_PROGRAM_LIMIT) {
        return &cpu->auxiliary_program[(address - DSPIC33_AUXILIARY_PROGRAM_BASE) / 2u];
    }
    if (persistent_program_tagged_address(address)) {
        return &cpu->persistent_program[persistent_program_index(address)];
    }
    return NULL;
}

uint16_t dspic33_internal_read_word(Dspic33* cpu, uint32_t address) {
    return dspic33_read_word(cpu, address);
}

static void record_non_cpu_sfr_read(Dspic33* cpu, uint32_t address) {
    if (cpu->instruction_active && !cpu->non_cpu_sfr_read && cpu->current_instruction_cycles < 2u &&
        address >= 0x005au && address < 0x1000u &&
        dspic33_internal_data_byte_is_implemented(cpu, address)) {
        cpu->non_cpu_sfr_read = true;
    }
}

static void record_cpu_data_read(Dspic33* cpu, uint32_t address) {
    record_non_cpu_sfr_read(cpu, address);
    if (cpu->instruction_active && address >= 0x1000u && (address & PSV_ADDRESS) == 0u) {
        cpu->io.cpu_bus_cycle = cpu->cycles;
    }
    if (cpu->instruction_active && (address & PSV_ADDRESS) != 0u) {
        cpu->psv_read = true;
    }
}

uint8_t dspic33_internal_read_data_byte(Dspic33* cpu, uint32_t address) {
    record_cpu_data_read(cpu, address);
    if (cpu->instruction_rmw) {
        cpu->instruction_rmw_read_valid = true;
        cpu->instruction_rmw_read_address = address;
        cpu->instruction_rmw_read_width = 1u;
    }
    return dspic33_read_byte(cpu, address);
}

uint16_t dspic33_internal_read_data_word(Dspic33* cpu, uint32_t address) {
    record_cpu_data_read(cpu, address);
    if (cpu->instruction_rmw) {
        cpu->instruction_rmw_read_valid = true;
        cpu->instruction_rmw_read_address = address;
        cpu->instruction_rmw_read_width = 2u;
    }
    return dspic33_read_word(cpu, address);
}

void dspic33_internal_mark_data_write(Dspic33* cpu, uint32_t address, uint8_t width) {
    if (address < 0x1000u || address + width > DSPIC33_DATA_SIZE) {
        return;
    }
    memset(&cpu->initialized_data[address], 1, width);
}

void dspic33_internal_record_data_read(Dspic33* cpu, uint32_t address, uint8_t width) {
    if ((!cpu->instruction_active && !cpu->io.dma_transfer_active) || address < 0x1000u ||
        address + width > DSPIC33_DATA_SIZE) {
        return;
    }
    for (uint8_t offset = 0u; offset < width; offset++) {
        if (!dspic33_internal_data_byte_is_implemented(cpu, address + offset)) {
            return;
        }
        if (cpu->initialized_data[address + offset] == 0u) {
            if (cpu->uninitialized_data_read_count == 0u) {
                cpu->first_uninitialized_data_read = address + offset;
            }
            cpu->uninitialized_data_read_count++;
            return;
        }
    }
}

uint64_t dspic33_get_uninitialized_data_read_count(const Dspic33* cpu) {
    return cpu == NULL ? 0u : cpu->uninitialized_data_read_count;
}

uint32_t dspic33_get_first_uninitialized_data_read(const Dspic33* cpu) {
    return cpu == NULL || cpu->uninitialized_data_read_count == 0u
               ? UINT32_MAX
               : cpu->first_uninitialized_data_read;
}

void dspic33_clear_uninitialized_data_reads(Dspic33* cpu) {
    if (cpu == NULL) {
        return;
    }
    cpu->uninitialized_data_read_count = 0u;
    cpu->first_uninitialized_data_read = UINT32_MAX;
}

uint16_t dspic33_internal_read_file_word(Dspic33* cpu, uint16_t address) {
    uint16_t value;
    if ((address & 1u) == 0u) {
        return dspic33_internal_read_data_word(cpu, address);
    }
    if (!dspic33_internal_check_data_alignment(cpu, address)) {
        cpu->address_error_access_allowed = true;
        value = dspic33_internal_read_data_word(cpu, address & 0xfffeu);
        cpu->address_error_access_allowed = false;
        cpu->address_error_working_state_completed = true;
        return value;
    }
    return 0u;
}

void dspic33_internal_record_var_write(Dspic33* cpu, uint32_t address, uint8_t width) {
    uint8_t domain;
    uint8_t other;
    uint8_t offset;
    if (!cpu->instruction_active || cpu->io.dma_transfer_active || (cpu->corcon & 0x8000u) == 0u ||
        address < 0x1000u || address + width > DSPIC33_DATA_SIZE) {
        return;
    }
    domain = cpu->interrupt_depth == 0u ? 1u : 2u;
    other = domain == 1u ? 2u : 1u;
    for (offset = 0u; offset < width; offset++) {
        uint32_t current = address + offset;
        uint32_t index = current >> 2u;
        uint8_t shift = (uint8_t)((current & 3u) * 2u);
        uint8_t domains = (uint8_t)((cpu->var_write_domains[index] >> shift) & 3u);
        if ((domains & other) != 0u) {
            cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
        }
        cpu->var_write_domains[index] |= (uint8_t)(domain << shift);
    }
}

bool dspic33_internal_instruction_rmw_write_matches(const Dspic33* cpu, uint32_t address,
                                                    uint8_t width) {
    uint32_t read_end;
    uint32_t write_end;
    if (!cpu->instruction_rmw || !cpu->instruction_rmw_read_valid) {
        return false;
    }
    read_end = cpu->instruction_rmw_read_address + cpu->instruction_rmw_read_width;
    write_end = address + width;
    return cpu->instruction_rmw_read_address < write_end && address < read_end;
}

void dspic33_internal_clear_instruction_transients(Dspic33* cpu) {
    cpu->instruction_rmw = false;
    cpu->instruction_rmw_read_valid = false;
    cpu->instruction_rmw_read_address = 0u;
    cpu->instruction_rmw_read_width = 0u;
    cpu->io.cpu_bus_cycle = UINT64_MAX;
    cpu->io.cpu_write_rmw = false;
}

void dspic33_internal_write_word(Dspic33* cpu, uint32_t address, uint16_t value) {
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
    return (modcon & enable) != 0u && selector != 15u && selector == reg && end >= start;
}

uint16_t dspic33_internal_modulo_address(const Dspic33* cpu, uint8_t reg, int32_t address,
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

static bool bit_reversed_addressing_enabled(const Dspic33* cpu, uint8_t mode, uint8_t reg,
                                            uint8_t width, bool write) {
    uint16_t modcon = stored_word(cpu, 0x0046u);
    uint16_t xbrev = stored_word(cpu, 0x0050u);
    uint8_t selector = (uint8_t)((modcon >> 8u) & 0x0fu);
    return write && width == 2u && (mode == 3u || mode == 5u) && (xbrev & 0x8000u) != 0u &&
           selector != 15u && selector == reg;
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

const int8_t dspic33_internal_dsp_prefetch_updates[16] = {
    0, 2, 4, 6, 0, -6, -4, -2, 0, 2, 4, 6, 0, -6, -4, -2,
};

static bool pseudo_linear_addressing_enabled(const Dspic33* cpu, uint8_t mode, uint8_t reg,
                                             bool bit_reversed) {
    return mode >= 2u && mode <= 5u && reg < 14u && !bit_reversed &&
           !modulo_addressing_enabled(cpu, reg, false);
}

static bool pseudo_linear_terminal_page(uint16_t page, bool increment, bool write) {
    if (increment) {
        return page == 0x01ffu || (!write && page == 0x03ffu);
    }
    return page == 0x0001u || (!write && page == 0x0200u);
}

uint32_t dspic33_internal_mapped_data_address(uint16_t address, uint16_t page, bool write) {
    if (!write && page >= 0x0200u) {
        return PSV_ADDRESS | ((page & 0x0100u) != 0u ? PSV_HIGH_BYTE : 0u) |
               ((uint32_t)(page & 0x00ffu) << 15u) | (address & 0x7fffu);
    }
    return ((uint32_t)page << 15u) | (address & 0x7fffu);
}

bool dspic33_internal_resolve_operand_address(const Dspic33* cpu, const uint16_t* registers,
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
            bit_reversed
                ? bit_reversed_address(cpu, registers[reg])
                : dspic33_internal_modulo_address(cpu, reg, adjusted_address, delta, false);
        resolution->updates_register = true;
    } else if (mode == 4u || mode == 5u) {
        delta = mode == 5u ? width : -(int32_t)width;
        adjusted_address = (int32_t)registers[reg] + delta;
        resolution->wrapped = adjusted_address < 0 || adjusted_address > UINT16_MAX;
        effective_address = bit_reversed ? registers[reg] : adjusted_address;
        resolution->updated_register =
            bit_reversed
                ? bit_reversed_address(cpu, registers[reg])
                : dspic33_internal_modulo_address(cpu, reg, adjusted_address, delta, false);
        resolution->updates_register = true;
        resolution->address = bit_reversed ? (uint16_t)effective_address
                              : reg == 15u ? (uint16_t)adjusted_address
                                           : resolution->updated_register;
    } else {
        delta = (int16_t)registers[offset_reg];
        effective_address = (int32_t)registers[reg] + delta;
        resolution->wrapped = effective_address < 0 || effective_address > UINT16_MAX;
        resolution->address =
            dspic33_internal_modulo_address(cpu, reg, effective_address, delta, false);
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
        bool transition = increment ? adjusted_address > UINT16_MAX : adjusted_address < 0x8000;
        if (transition) {
            uint16_t offset = (uint16_t)adjusted_address & 0x7fffu;
            bool terminal = pseudo_linear_terminal_page(data_page, increment, write);
            resolution->updated_register = terminal ? offset : (uint16_t)(offset | 0x8000u);
            resolution->updated_data_page =
                terminal ? data_page : (uint16_t)(increment ? data_page + 1u : data_page - 1u);
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
    if (resolution->access_register >= 0x8000u && resolution->paged_addressing_enabled) {
        resolution->unimplemented_data_page = resolution->access_data_page == 0u;
        resolution->address = dspic33_internal_mapped_data_address(
            resolution->access_register, resolution->access_data_page, write);
    }
    return true;
}

bool dspic33_internal_operand_resolution(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                         uint8_t offset_reg, uint8_t width, bool write,
                                         OperandResolution* resolution) {
    bool uses_stack_pointer;
    if (!dspic33_internal_address_register_initialized(cpu, reg) ||
        !dspic33_internal_resolve_operand_address(cpu, cpu->w, mode, reg, offset_reg, width, write,
                                                  resolution)) {
        return false;
    }
    if (!write) {
        dspic33_internal_record_source_address_register(cpu, reg);
        if (mode >= 6u) {
            dspic33_internal_record_source_address_register(cpu, offset_reg);
        }
    }
    uses_stack_pointer = reg == 15u || (mode >= 6u && offset_reg == 15u);
    if (uses_stack_pointer) {
        dspic33_internal_check_stack_address(cpu, resolution->effective_address,
                                             resolution->wrapped);
    }
    if (resolution->updates_data_page) {
        if (write) {
            cpu->dswpag = resolution->updated_data_page & 0x01ffu;
        } else {
            cpu->dsrpag = resolution->updated_data_page & 0x03ffu;
        }
    }
    if (resolution->updates_register) {
        dspic33_internal_write_working_register(cpu, reg, resolution->updated_register);
    }
    if (resolution->unimplemented_data_page) {
        dspic33_internal_raise_data_page_error(cpu);
    }
    if (!write && cpu->repeat_active != 0u && width == 2u && (mode == 2u || mode == 3u) &&
        (resolution->address & PSV_ADDRESS) != 0u) {
        cpu->psv_repeat_optimized = true;
    }
    return true;
}

bool dspic33_internal_operand_address(Dspic33* cpu, uint8_t mode, uint8_t reg, uint8_t offset_reg,
                                      uint8_t width, bool write, uint32_t* address) {
    OperandResolution resolution;
    if (!dspic33_internal_operand_resolution(cpu, mode, reg, offset_reg, width, write,
                                             &resolution)) {
        return false;
    }
    *address = resolution.address;
    return true;
}

bool dspic33_internal_following_operand_address(Dspic33* cpu, const OperandResolution* resolution,
                                                bool write, uint32_t* address) {
    uint32_t next = (uint32_t)resolution->access_register + 2u;
    if (next > UINT16_MAX) {
        dspic33_internal_raise_data_page_error(cpu);
        return false;
    }
    *address = (uint16_t)next;
    if (resolution->paged_addressing_enabled && next >= 0x8000u) {
        if (resolution->access_data_page == 0u) {
            dspic33_internal_raise_data_page_error(cpu);
        }
        *address = dspic33_internal_mapped_data_address((uint16_t)next,
                                                        resolution->access_data_page, write);
    }
    return true;
}

bool dspic33_internal_validate_operand_alignment(Dspic33* cpu, uint16_t* registers, uint8_t mode,
                                                 uint8_t reg, uint8_t offset_reg, uint8_t width,
                                                 bool write, bool indirect_bit) {
    OperandResolution resolution;
    if (mode == 0u) {
        return true;
    }
    if (!dspic33_internal_address_register_initialized(cpu, reg) ||
        !dspic33_internal_resolve_operand_address(cpu, registers, mode, reg, offset_reg, width,
                                                  write, &resolution)) {
        return false;
    }
    if ((width != 1u || indirect_bit) &&
        !dspic33_internal_check_data_alignment(cpu, resolution.address)) {
        return false;
    }
    if (resolution.updates_register) {
        registers[reg] = reg == 15u ? (uint16_t)(resolution.updated_register & 0xfffeu)
                                    : resolution.updated_register;
    }
    return true;
}

bool dspic33_internal_validate_destination_after_source_execution(Dspic33* cpu, uint8_t mode,
                                                                  uint8_t reg, uint8_t width) {
    uint16_t registers[16];
    if (cpu->address_error && !cpu->address_error_access_allowed) {
        return false;
    }
    memcpy(registers, cpu->w, sizeof(registers));
    if (dspic33_internal_validate_operand_alignment(cpu, registers, mode, reg, 0u, width, true,
                                                    false)) {
        return true;
    }
    if (!cpu->illegal_reset) {
        cpu->address_error_working_state_completed = true;
    }
    return false;
}

uint8_t dspic33_internal_read_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                           uint8_t offset_reg) {
    uint32_t address;
    if (mode == 0u) {
        return (uint8_t)cpu->w[reg];
    }
    if (!dspic33_internal_operand_address(cpu, mode, reg, offset_reg, 1u, false, &address)) {
        return 0u;
    }
    return dspic33_internal_read_data_byte(cpu, address);
}

uint16_t dspic33_internal_read_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                            uint8_t offset_reg) {
    uint32_t address;
    if (mode == 0u) {
        return cpu->w[reg];
    }
    if (!dspic33_internal_operand_address(cpu, mode, reg, offset_reg, 2u, false, &address)) {
        return 0u;
    }
    return dspic33_internal_read_data_word(cpu, address);
}

bool dspic33_internal_write_operand_byte(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                         uint8_t offset_reg, uint8_t value) {
    uint32_t address;
    if (mode == 0u) {
        dspic33_internal_write_working_register_byte(cpu, reg, false, value);
        return true;
    }
    if (!dspic33_internal_operand_address(cpu, mode, reg, offset_reg, 1u, true, &address)) {
        return false;
    }
    dspic33_write_byte(cpu, address, value);
    return true;
}

bool dspic33_internal_write_operand_word(Dspic33* cpu, uint8_t mode, uint8_t reg,
                                         uint8_t offset_reg, uint16_t value) {
    uint32_t address;
    if (mode == 0u) {
        dspic33_internal_write_working_register(cpu, reg, value);
        return true;
    }
    if (!dspic33_internal_operand_address(cpu, mode, reg, offset_reg, 2u, true, &address)) {
        return false;
    }
    dspic33_write_word(cpu, address, value);
    return true;
}

bool dspic33_internal_execute_move_literal(Dspic33* cpu, uint32_t opcode) {
    uint8_t destination = (uint8_t)(opcode & 0x0fu);
    uint16_t literal = (uint16_t)((opcode >> 4u) & 0xffffu);
    dspic33_internal_write_working_register(cpu, destination, literal);
    return true;
}

bool dspic33_internal_execute_move(Dspic33* cpu, uint32_t opcode) {
    uint8_t source_register = (uint8_t)(opcode & 0x0fu);
    uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
    uint8_t destination_register = (uint8_t)((opcode >> 7u) & 0x0fu);
    uint8_t destination_mode = (uint8_t)((opcode >> 11u) & 0x07u);
    uint8_t offset_register = (uint8_t)((opcode >> 15u) & 0x0fu);
    bool byte_mode = (opcode & 0x004000u) != 0u;
    uint16_t registers[16];

    memcpy(registers, cpu->w, sizeof(registers));
    if (!dspic33_internal_validate_operand_alignment(cpu, registers, source_mode, source_register,
                                                     offset_register, byte_mode ? 1u : 2u, false,
                                                     false) ||
        !dspic33_internal_validate_operand_alignment(cpu, registers, destination_mode,
                                                     destination_register, offset_register,
                                                     byte_mode ? 1u : 2u, true, false)) {
        return true;
    }

    if (byte_mode) {
        uint8_t value =
            dspic33_internal_read_operand_byte(cpu, source_mode, source_register, offset_register);
        return dspic33_internal_write_operand_byte(cpu, destination_mode, destination_register,
                                                   offset_register, value);
    }
    return dspic33_internal_write_operand_word(
        cpu, destination_mode, destination_register, offset_register,
        dspic33_internal_read_operand_word(cpu, source_mode, source_register, offset_register));
}

bool dspic33_internal_indirect_literal_address(Dspic33* cpu, uint8_t reg, int16_t offset,
                                               bool write, uint32_t* resolved_address) {
    int32_t delta = offset;
    int32_t effective_address;
    uint32_t address;
    if (!dspic33_internal_address_register_initialized(cpu, reg)) {
        return false;
    }
    if (!write) {
        dspic33_internal_record_source_address_register(cpu, reg);
    }
    effective_address = (int32_t)cpu->w[reg] + delta;
    address = dspic33_internal_modulo_address(cpu, reg, effective_address, delta, false);
    if (reg == 15u) {
        dspic33_internal_check_stack_address(
            cpu, effective_address, effective_address < 0 || effective_address > UINT16_MAX);
    }
    if (address >= 0x8000u && !((reg == 14u || reg == 15u) && (cpu->corcon & 0x0004u) != 0u)) {
        uint16_t page = write ? cpu->dswpag : cpu->dsrpag;
        if (page == 0u) {
            dspic33_internal_raise_data_page_error(cpu);
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
