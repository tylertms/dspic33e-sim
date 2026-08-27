#include "internal.h"

void dspic33_set_generic_hard_trap_source(Dspic33* cpu, bool active) {
    dspic33_internal_set_trap_source(cpu, 2u, 0x000008u, 13u, 0u, active);
}

void dspic33_set_generic_soft_trap_source(Dspic33* cpu, bool active) {
    dspic33_internal_set_trap_source(cpu, 6u, 0x000010u, 9u, 1u, active);
}

void dspic33_raise_oscillator_fail_trap(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x08c0u, (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | 0x0002u));
    dspic33_internal_schedule_soft_trap(cpu, 0u, 0x000004u, 15u, 0u);
}

static void schedule_stack_error(Dspic33* cpu, uint8_t delay) {
    dspic33_write_word(cpu, 0x08c0u, (uint16_t)(dspic33_read_word(cpu, 0x08c0u) | 0x0004u));
    dspic33_internal_schedule_soft_trap(cpu, 3u, 0x00000au, 12u, delay);
}

void dspic33_check_stack_address(Dspic33* cpu, int32_t stack_address, bool limit_wrapped,
                                 uint8_t trap_delay) {
    const uint16_t effective_stack_address = (uint16_t)stack_address;
    const bool stack_underflow = stack_address < 0x1000;
    const bool stack_overflow = cpu->splim_enabled && effective_stack_address > cpu->splim;
    if (!stack_underflow && !stack_overflow && !(cpu->splim_enabled && limit_wrapped)) {
        return;
    }
    schedule_stack_error(cpu, trap_delay);
}

void dspic33_internal_check_stack_address(Dspic33* cpu, int32_t stack_address, bool limit_wrapped) {
    dspic33_check_stack_address(cpu, stack_address, limit_wrapped, 2u);
}

bool dspic33_internal_service_pending_soft_trap(Dspic33* cpu) {
    Dspic33PendingSoftTrap* selected_trap = NULL;
    uint8_t current_trap_priority;
    size_t pending_index;

    current_trap_priority =
        (uint8_t)(((cpu->corcon & 0x0008u) != 0u ? 8u : 0u) | ((cpu->sr >> 5u) & 0x07u));

    for (pending_index = 0u; pending_index < 4u; pending_index++) {
        Dspic33PendingSoftTrap* pending_trap = &cpu->pending_soft_traps[pending_index];
        if (pending_trap->active && pending_trap->delay == 0u && pending_trap->priority >= 13u &&
            pending_trap->priority < current_trap_priority) {
            dspic33_internal_perform_warm_reset(cpu, 0x8000u, DSPIC33_RESET_HARDWARE);
            return true;
        }
    }

    for (pending_index = 0u; pending_index < 4u; pending_index++) {
        Dspic33PendingSoftTrap* pending_trap = &cpu->pending_soft_traps[pending_index];
        if (pending_trap->active && pending_trap->delay == 0u &&
            pending_trap->priority > current_trap_priority &&
            (selected_trap == NULL || pending_trap->priority > selected_trap->priority)) {
            selected_trap = pending_trap;
        }
    }

    if (selected_trap != NULL) {
        const uint16_t selected_trap_code = selected_trap->trap;
        const uint32_t selected_vector = selected_trap->vector;
        const uint8_t selected_priority = selected_trap->priority;
        const bool trap_source_active =
            (selected_trap_code == 2u && ((dspic33_read_word(cpu, 0x08c2u) & 0x2000u) != 0u ||
                                          (dspic33_read_word(cpu, 0x08c6u) & 0x0001u) != 0u)) ||
            (selected_trap_code == 4u && (dspic33_read_word(cpu, 0x08c0u) & 0x0010u) != 0u) ||
            (selected_trap_code == 6u && (dspic33_read_word(cpu, 0x08c4u) & 0x0070u) != 0u);
        if (!trap_source_active) {
            selected_trap->active = false;
        }
        dspic33_internal_enter_trap(cpu, selected_trap_code, selected_vector, selected_priority, 0u,
                                    cpu->pc, selected_trap->auxiliary_program);
        return true;
    }
    return false;
}

static bool soft_exception_pending(const Dspic33* cpu) {
    size_t pending_index;

    for (pending_index = 0u; pending_index < 4u; pending_index++) {
        if (cpu->pending_soft_traps[pending_index].active) {
            return true;
        }
    }
    return false;
}

bool dspic33_internal_exception_pending(const Dspic33* cpu) {
    return soft_exception_pending(cpu) || dspic33_device_interrupt_pending(cpu);
}

static bool advance_instruction_cycles(Dspic33* cpu, uint64_t cycles, uint64_t device_ratio) {
    if (cycles > UINT64_MAX / device_ratio ||
        !dspic33_device_advance_instruction(cpu, cycles, cycles * device_ratio)) {
        cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        return false;
    }
    return true;
}

static void advance_pending_soft_traps(Dspic33* cpu, uint64_t cycles) {
    for (size_t trap_index = 0u; trap_index < 4u; trap_index++) {
        Dspic33PendingSoftTrap* pending = &cpu->pending_soft_traps[trap_index];
        if (pending->active && pending->delay != 0u) {
            pending->delay = pending->delay > cycles ? (uint8_t)(pending->delay - cycles) : 0u;
        }
    }
}

bool dspic33_internal_advance_instruction_stall(Dspic33* cpu, uint64_t cycles,
                                                uint64_t device_ratio) {
    if (!advance_instruction_cycles(cpu, cycles, device_ratio)) {
        return false;
    }
    advance_pending_soft_traps(cpu, cycles);
    return true;
}

void dspic33_internal_advance_instruction(Dspic33* cpu, uint64_t cycles, bool separate_wait_cycle,
                                          uint64_t device_ratio) {
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
            for (size_t group_index = 0u; group_index < DSPIC33_IRQ_GROUP_COUNT; group_index++) {
                cpu->interrupt_deferred[group_index] |= nested_interrupt_deferred[group_index];
            }
        }
    } else {
        advance_instruction_cycles(cpu, cycles, device_ratio);
    }
    advance_pending_soft_traps(cpu, cycles);
    dspic33_internal_service_pending_soft_trap(cpu);
}

uint64_t dspic33_internal_instruction_cycles(const Dspic33* cpu, uint32_t opcode,
                                             uint32_t instruction_pc) {
    CompareControlKind compare_kind = dspic33_internal_compare_control_kind(opcode);
    uint8_t bit_kind = (uint8_t)((opcode >> 16u) & 0x07u);
    bool bit_skip = (opcode & 0xf00000u) == 0xa00000u && bit_kind >= 6u;
    bool skip = bit_skip || (compare_kind != COMPARE_CONTROL_NONE &&
                             dspic33_internal_compare_control_displacement(opcode) == 1);
    if (skip) {
        uint32_t sequential = dspic33_internal_program_address_add(instruction_pc, 2);
        uint32_t distance = (cpu->pc - sequential) & 0x007fffffu;
        if (cpu->address_error && cpu->address_error_control_state_completed &&
            cpu->address_error_return == dspic33_internal_device_program_limit(cpu) &&
            instruction_pc + 2u == dspic33_internal_device_program_limit(cpu)) {
            return 2u;
        }
        if ((distance & 1u) == 0u && distance <= 4u) {
            return 1u + distance / 2u;
        }
    }
    if (compare_kind != COMPARE_CONTROL_NONE) {
        return dspic33_internal_compare_control_taken(cpu, opcode, compare_kind) ? 5u : 1u;
    }
    bool branch_taken;
    if (dspic33_internal_relative_branch_condition(cpu, opcode, &branch_taken)) {
        return branch_taken ? 4u : 1u;
    }
    if ((opcode & 0xff0000u) == 0x050000u) {
        return cpu->address_error || dspic33_internal_exception_pending(cpu) ? 5u : 6u;
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
        dspic33_internal_computed_control_transfer_encoding(opcode)) {
        return 4u;
    }
    return 1u;
}

void dspic33_raise_dma_address_trap(Dspic33* cpu) {
    dspic33_write_word(cpu, 0x08c4u, (uint16_t)(dspic33_read_word(cpu, 0x08c4u) | 0x0020u));
    dspic33_internal_enter_trap(cpu, 6u, 0x000010u, 9u, 0u, cpu->pc,
                                dspic33_internal_auxiliary_program_address(cpu->pc));
}

void dspic33_raise_dma_collision_trap(Dspic33* cpu) {
    dspic33_internal_enter_trap(cpu, 5u, 0x00000eu, 10u, 0x0020u, cpu->pc,
                                dspic33_internal_auxiliary_program_address(cpu->pc));
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
        if (cpu->repeat_active == 0u || cpu->rcount == 17u) {
            dspic33_device_latch_math_error(cpu, 0x0040u);
        }
        return true;
    }
    if (cpu->repeat_active != 0u && cpu->rcount != 0u) {
        return true;
    }
    if (unsigned_divide) {
        uint32_t dividend = double_word
                                ? ((uint32_t)cpu->w[high_register] << 16u) | cpu->w[low_register]
                                : cpu->w[low_register];
        uint32_t unsigned_quotient = dividend / divisor;
        uint32_t unsigned_remainder = dividend % divisor;
        overflow = unsigned_quotient > UINT16_MAX;
        overflow_flag = overflow;
        quotient = unsigned_quotient;
        remainder = unsigned_remainder;
    } else {
        int32_t dividend =
            double_word ? (int32_t)(((uint32_t)cpu->w[high_register] << 16u) | cpu->w[low_register])
                        : (int16_t)cpu->w[low_register];
        int16_t signed_divisor = (int16_t)divisor;
        quotient = (int64_t)dividend / signed_divisor;
        remainder = (int64_t)dividend % signed_divisor;
        overflow = quotient < INT16_MIN || quotient > INT16_MAX;
        overflow_flag =
            overflow && (!double_word || !(((dividend < 0) != (signed_divisor < 0)) ||
                                           dividend > 0x3fffffff || dividend <= -0x40000000));
    }
    dspic33_internal_write_working_register(cpu, 0u, (uint16_t)quotient);
    dspic33_internal_write_working_register(cpu, 1u, (uint16_t)remainder);
    dspic33_internal_update_divide_flags(cpu, remainder, overflow_flag);
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
        if (cpu->repeat_active == 0u || cpu->rcount == 17u) {
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
    dspic33_internal_write_working_register(cpu, 0u, (uint16_t)quotient);
    dspic33_internal_write_working_register(cpu, 1u, (uint16_t)remainder);
    dspic33_internal_update_divide_flags(cpu, remainder, overflow);
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

bool dspic33_internal_execute(Dspic33* cpu, uint32_t opcode) {
    if (!dspic33_internal_system_encoding_valid(opcode)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xff0000u) == 0x3f0000u) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (reserved_move_encoding(opcode)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xfff000u) == 0xfd4000u && (opcode & 0xfffff0u) != 0xfd4000u) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (!dspic33_internal_stack_encoding_valid(opcode)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xff0000u) == 0x010000u &&
        !dspic33_internal_computed_control_transfer_encoding(opcode)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if (dspic33_internal_reserved_return_encoding(opcode)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xfffff0u) == 0xfd4000u) {
        return dspic33_internal_execute_decimal_adjust(cpu, opcode);
    }
    if ((opcode & 0xffbff0u) == 0xfd8000u) {
        uint8_t reg = (uint8_t)(opcode & 0x0fu);
        uint16_t register_value = cpu->w[reg];
        if ((opcode & 0x004000u) != 0u) {
            uint8_t low_byte = (uint8_t)register_value;
            dspic33_internal_write_working_register_byte(
                cpu, reg, false, (uint8_t)((low_byte << 4u) | (low_byte >> 4u)));
        } else {
            dspic33_internal_write_working_register(
                cpu, reg, (uint16_t)((register_value << 8u) | (register_value >> 8u)));
        }
        return true;
    }
    if ((opcode & 0xfff870u) == 0xfd0000u) {
        uint8_t source = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint16_t source_value = cpu->w[source];
        uint16_t destination_value = cpu->w[destination];
        dspic33_internal_write_working_register(cpu, source, destination_value);
        dspic33_internal_write_working_register(cpu, destination, source_value);
        return true;
    }
    if (opcode == 0xfa8000u) {
        if ((cpu->corcon & 0x0004u) == 0u) {
            schedule_stack_error(cpu, 2u);
        }
        dspic33_internal_write_working_register(cpu, 15u, cpu->w[14]);
        dspic33_internal_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
        dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
        dspic33_internal_record_source_address_register(cpu, 15u);
        dspic33_internal_write_working_register(cpu, 14u,
                                                dspic33_internal_read_word(cpu, cpu->w[15]));
        cpu->corcon &= 0xfffbu;
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
    if ((opcode & 0xff0000u) == 0xf80000u) {
        dspic33_internal_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
        dspic33_internal_write_word(cpu, cpu->w[15],
                                    dspic33_internal_read_data_word(cpu, opcode & 0xffffu));
        dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
        return true;
    }
    if ((opcode & 0xff0000u) == 0xf90000u) {
        dspic33_internal_check_stack_address(cpu, (int32_t)cpu->w[15] - 2, cpu->w[15] < 2u);
        dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] - 2u));
        dspic33_internal_record_source_address_register(cpu, 15u);
        dspic33_internal_write_word(cpu, opcode & 0xffffu,
                                    dspic33_internal_read_word(cpu, cpu->w[15]));
        return true;
    }
    if ((opcode & 0xffc001u) == 0xfa0000u) {
        int32_t stack_top;
        if ((cpu->corcon & 0x0004u) != 0u) {
            schedule_stack_error(cpu, 2u);
        }
        dspic33_internal_check_stack_address(cpu, cpu->w[15], cpu->w[15] > 0xfffdu);
        dspic33_internal_write_word(cpu, cpu->w[15], cpu->w[14]);
        dspic33_internal_write_working_register(cpu, 15u, (uint16_t)(cpu->w[15] + 2u));
        dspic33_internal_write_working_register(cpu, 14u, cpu->w[15]);
        cpu->corcon |= 0x0004u;
        stack_top = (int32_t)cpu->w[15] + (opcode & 0x007ffeu);
        dspic33_internal_write_working_register(cpu, 15u, (uint16_t)stack_top);
        dspic33_internal_check_stack_address(cpu, cpu->w[15], stack_top > UINT16_MAX);
        return true;
    }
    if ((opcode & 0xff0000u) == 0x020000u || (opcode & 0xff0000u) == 0x040000u) {
        bool call = (opcode & 0xff0000u) == 0x020000u;
        uint32_t return_pc;
        uint32_t second;
        uint32_t target;
        if (!dspic33_internal_literal_control_first_word_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        if (cpu->pc >= DSPIC33_AUXILIARY_PROGRAM_LIMIT) {
            cpu->pc &= 0x007ffffeu;
        }
        if (cpu->pc != dspic33_internal_device_program_limit(cpu) &&
            !dspic33_device_program_range_implemented(cpu, cpu->pc, 2u)) {
            return false;
        }
        second = cpu->pc == dspic33_internal_device_program_limit(cpu)
                     ? 0u
                     : dspic33_read_program_word(cpu, cpu->pc);
        if (!dspic33_internal_literal_control_extension_valid(second)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        target = ((second & 0x007fu) << 16u) | (opcode & 0x00ffffu);
        target &= 0x007ffffeu;
        return_pc = dspic33_internal_program_address_add(cpu->pc, 2);
        if (call) {
            dspic33_internal_push_program_counter(cpu, return_pc);
        }
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010000u) {
        uint32_t target;
        dspic33_internal_push_program_counter(cpu, cpu->pc);
        target = cpu->w[opcode & 0x0fu] & 0xfffeu;
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010200u) {
        uint32_t target;
        dspic33_internal_push_program_counter(cpu, cpu->pc);
        target = dspic33_internal_program_address_add(cpu->pc, (int16_t)cpu->w[opcode & 0x0fu] * 2);
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010400u) {
        uint32_t target = cpu->w[opcode & 0x0fu] & 0xfffeu;
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x010600u) {
        uint32_t target =
            dspic33_internal_program_address_add(cpu->pc, (int16_t)cpu->w[opcode & 0x0fu] * 2);
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    uint8_t long_source;
    bool long_call;
    if (dspic33_internal_long_control_transfer(opcode, &long_source, &long_call)) {
        uint32_t target =
            ((uint32_t)(cpu->w[long_source + 1u] & 0x007fu) << 16u) | cpu->w[long_source];
        if (long_call) {
            dspic33_internal_push_program_counter(cpu, cpu->pc);
        }
        target &= 0x007ffffeu;
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    if ((opcode & 0xfffff0u) == 0x088000u || (opcode & 0xff8000u) == 0x080000u) {
        uint32_t extension;
        uint16_t do_count;
        int16_t displacement;
        uint8_t depth;
        if (!dspic33_device_program_range_implemented(cpu, cpu->pc, 2u)) {
            return false;
        }
        do_count = (opcode & 0xfffff0u) == 0x088000u ? cpu->w[opcode & 0x0fu]
                                                     : (uint16_t)(opcode & 0x7fffu);
        extension = dspic33_read_program_word(cpu, cpu->pc);
        if ((extension & 0xff0000u) != 0u) {
            return false;
        }
        displacement = (int16_t)extension;
        cpu->pc = dspic33_internal_program_address_add(cpu->pc, 2);
        if (cpu->do_depth == 4u) {
            dspic33_write_word(cpu, 0x08c4u, (uint16_t)(dspic33_read_word(cpu, 0x08c4u) | 0x0010u));
            dspic33_internal_schedule_soft_trap(cpu, 6u, 0x000010u, 9u, 1u);
            return true;
        }
        if (do_count == 0u && cpu->do_depth != 0u &&
            !dspic33_internal_nested_zero_do_workaround_present(cpu, cpu->pc)) {
            cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            return true;
        }
        depth = cpu->do_depth++;
        cpu->do_start[depth] = cpu->pc;
        cpu->do_end[depth] =
            dspic33_internal_program_address_add(cpu->pc, (int32_t)displacement * 2);
        cpu->do_count[depth] = do_count;
        cpu->do_terminate[depth] = 0u;
        cpu->dostart = cpu->do_start[depth];
        cpu->doend = cpu->do_end[depth];
        cpu->dcount = do_count;
        cpu->corcon = (uint16_t)((cpu->corcon & ~0x0700u) | ((uint16_t)cpu->do_depth << 8u));
        cpu->sr |= 0x0200u;
        if (dspic33_internal_program_target_requires_address_error(cpu, cpu->do_start[depth])) {
            dspic33_internal_raise_program_target_error(cpu, cpu->pc - 2u);
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
        uint16_t page_value = (uint16_t)(opcode & 0x03ffu);
        switch ((opcode >> 10u) & 3u) {
        case 0u:
            cpu->dsrpag = page_value;
            break;
        case 1u:
            cpu->dswpag = page_value & 0x01ffu;
            break;
        case 2u:
            cpu->tblpag = page_value & 0x00ffu;
            break;
        }
        return true;
    }
    if ((opcode & 0xfff3f0u) == 0xfed000u) {
        uint16_t page_value = cpu->w[opcode & 0x0fu];
        switch ((opcode >> 10u) & 3u) {
        case 0u:
            cpu->dsrpag = page_value & 0x03ffu;
            break;
        case 1u:
            cpu->dswpag = page_value & 0x01ffu;
            break;
        case 2u:
            cpu->tblpag = page_value & 0x00ffu;
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
        return dspic33_internal_execute_move_literal(cpu, opcode);
    }
    if ((opcode & 0xfff000u) == 0xb3c000u) {
        uint8_t destination = (uint8_t)(opcode & 0x0fu);
        uint8_t literal = (uint8_t)((opcode >> 4u) & 0xffu);
        dspic33_internal_write_working_register_byte(cpu, destination, false, literal);
        return true;
    }
    if ((opcode & 0xfc0000u) == 0xb00000u) {
        return dspic33_internal_execute_literal_binary(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0x780000u) {
        return dspic33_internal_execute_move(cpu, opcode);
    }
    if ((opcode & 0xf00000u) == 0x900000u) {
        return dspic33_internal_execute_move_offset(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xbe0000u) {
        return dspic33_internal_execute_move_double(cpu, opcode);
    }
    if ((opcode & 0xfe0000u) == 0xba0000u) {
        if (!dspic33_internal_table_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_table(cpu, opcode);
    }
    if ((opcode & 0xf00000u) == 0xa00000u) {
        if (!dspic33_internal_bit_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_bit(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0x800000u) {
        uint16_t address = (uint16_t)(((opcode >> 4u) & 0x7fffu) << 1u);
        dspic33_internal_write_working_register(
            cpu, (uint8_t)(opcode & 0x0fu),
            dspic33_internal_read_data_word(
                cpu, dspic33_internal_direct_move_address(cpu, address, false)));
        return true;
    }
    if ((opcode & 0xf80000u) == 0x880000u) {
        uint16_t address = (uint16_t)(((opcode >> 4u) & 0x7fffu) << 1u);
        dspic33_write_word(cpu, dspic33_internal_direct_move_address(cpu, address, true),
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
        return dspic33_internal_execute_file_binary(cpu, opcode);
    }
    if ((opcode & 0xff8000u) == 0xbf8000u) {
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        bool byte_mode = (opcode & 0x004000u) != 0u;
        uint16_t read_value = byte_mode ? dspic33_internal_read_data_byte(cpu, address)
                                        : dspic33_internal_read_file_word(cpu, address);
        if ((opcode & 0x002000u) == 0u) {
            if (byte_mode) {
                dspic33_internal_write_working_register_byte(cpu, 0u, false, (uint8_t)read_value);
            } else {
                dspic33_internal_write_working_register(cpu, 0u, read_value);
            }
        } else if (byte_mode) {
            dspic33_write_byte(cpu, address, (uint8_t)read_value);
        } else {
            dspic33_write_word(cpu, address, read_value);
        }
        dspic33_internal_update_logic_flags(cpu, read_value, byte_mode);
        return true;
    }
    if ((opcode & 0xff0000u) == 0xfb0000u) {
        if (!dspic33_internal_byte_extension_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        bool zero_extend = (opcode & 0x008000u) != 0u;
        uint8_t source_mode = (uint8_t)((opcode >> 4u) & 0x07u);
        uint8_t source_register = (uint8_t)(opcode & 0x0fu);
        uint8_t destination = (uint8_t)((opcode >> 7u) & 0x0fu);
        uint8_t source = dspic33_internal_read_operand_byte(cpu, source_mode, source_register, 0u);
        if (cpu->illegal_reset) {
            return true;
        }
        dspic33_internal_write_working_register(
            cpu, destination, zero_extend ? source : (uint16_t)(int16_t)(int8_t)source);
        dspic33_internal_update_logic_flags(cpu, cpu->w[destination], false);
        cpu->sr = (uint16_t)((cpu->sr & ~0x0001u) | ((cpu->sr & 0x0008u) == 0u ? 0x0001u : 0u));
        return true;
    }
    if ((opcode & 0xfc0000u) == 0xd40000u) {
        if (!dspic33_internal_file_shift_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_file_shift(cpu, opcode);
    }
    if ((opcode & 0xfc0000u) == 0xd00000u) {
        if (!dspic33_internal_single_shift_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_single_shift(cpu, opcode);
    }
    if ((opcode & 0xfe0000u) == 0xb80000u && !dspic33_internal_multiply_encoding_valid(opcode)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xfe0000u) == 0xb80000u) {
        return dspic33_internal_execute_multiply(cpu, opcode);
    }
    if ((opcode & 0xff7fffu) == 0xcb0000u || (opcode & 0xff7fffu) == 0xcb1000u ||
        (opcode & 0xff7fffu) == 0xcb3000u) {
        return dspic33_internal_execute_accumulator_arithmetic(cpu, opcode);
    }
    if (((opcode & 0xf80000u) == 0xc00000u || (opcode & 0xfc0000u) == 0xf00000u) &&
        !dspic33_internal_dsp_encoding_valid(opcode)) {
        dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
        return true;
    }
    if ((opcode & 0xff4000u) == 0xc30000u || (opcode & 0xff4000u) == 0xc70000u) {
        return dspic33_internal_execute_dsp_clear_or_move(cpu, opcode);
    }
    if ((opcode & 0xfc4c00u) == 0xf04000u && (opcode & 3u) >= 2u) {
        return dspic33_internal_execute_euclidean_distance(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0xc00000u || (opcode & 0xfc0000u) == 0xf00000u) {
        return dspic33_internal_execute_dsp_multiply(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xc80000u) {
        if ((opcode & 0xff7f00u) != 0xc80000u ||
            !dspic33_internal_accumulator_shift_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_accumulator_shift(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xc90000u || (opcode & 0xff0000u) == 0xca0000u) {
        return dspic33_internal_execute_accumulator_word(cpu, opcode,
                                                         (opcode & 0xff0000u) == 0xc90000u);
    }
    if ((opcode & 0xfe0000u) == 0xcc0000u) {
        return dspic33_internal_execute_accumulator_store(cpu, opcode);
    }
    if ((opcode & 0xffa000u) == 0xbc0000u) {
        uint16_t address = (uint16_t)(opcode & 0x1fffu);
        if ((opcode & 0x004000u) != 0u) {
            dspic33_internal_write_working_register(
                cpu, 2u,
                (uint16_t)((uint8_t)cpu->w[0] * dspic33_internal_read_data_byte(cpu, address)));
        } else {
            uint32_t product = (uint32_t)cpu->w[0] * dspic33_internal_read_data_word(cpu, address);
            dspic33_internal_write_working_register(cpu, 2u, (uint16_t)product);
            dspic33_internal_write_working_register(cpu, 3u, (uint16_t)(product >> 16u));
        }
        return true;
    }
    if ((opcode & 0xff0000u) == 0xcf0000u) {
        if (!dspic33_internal_find_first_encoding_valid(opcode, false)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_find_first(cpu, opcode);
    }
    if (dspic33_internal_compare_control_kind(opcode) != COMPARE_CONTROL_NONE) {
        return dspic33_internal_execute_compare_control(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xd80000u) {
        if (!dspic33_internal_divide_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return execute_divide(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xd90000u) {
        if (!dspic33_internal_divide_encoding_valid(opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return execute_fractional_divide(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0xdd0000u) {
        if (!dspic33_internal_multiple_shift_encoding_valid(opcode, true)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_shift(cpu, opcode, true);
    }
    if ((opcode & 0xff0000u) == 0xde0000u) {
        if (!dspic33_internal_multiple_shift_encoding_valid(opcode, false)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_shift(cpu, opcode, false);
    }
    if ((opcode & 0xff0000u) == 0xdf0000u) {
        if (!dspic33_internal_find_first_encoding_valid(opcode, true)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return true;
        }
        return dspic33_internal_execute_find_first_sign_change(cpu, opcode);
    }
    if ((opcode & 0xf80000u) == 0x100000u || (opcode & 0xf80000u) == 0x180000u ||
        (opcode & 0xf80000u) == 0x400000u || (opcode & 0xf80000u) == 0x480000u ||
        (opcode & 0xf80000u) == 0x500000u || (opcode & 0xf80000u) == 0x580000u ||
        (opcode & 0xf80000u) == 0x600000u || (opcode & 0xf80000u) == 0x680000u ||
        (opcode & 0xf80000u) == 0x700000u) {
        return dspic33_internal_execute_binary(cpu, opcode, opcode & 0xf80000u);
    }
    if ((opcode & 0xfc0000u) == 0xe00000u) {
        return dspic33_internal_execute_compare(cpu, opcode);
    }
    if ((opcode & 0xfc0000u) == 0xe80000u) {
        return dspic33_internal_execute_unary(cpu, opcode);
    }
    if ((opcode & 0xfc0000u) == 0xec0000u) {
        return dspic33_internal_execute_file_unary(cpu, opcode);
    }
    if ((opcode & 0xff0000u) == 0x070000u) {
        int32_t displacement = (int16_t)(opcode & 0xffffu);
        uint32_t target = dspic33_internal_program_address_add(cpu->pc, displacement * 2);
        dspic33_internal_push_program_counter(cpu, cpu->pc);
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_raise_program_target_error(cpu, cpu->pc);
            return true;
        }
        cpu->pc = target;
        return true;
    }
    bool branch_taken;
    if (dspic33_internal_relative_branch_condition(cpu, opcode, &branch_taken)) {
        if (branch_taken) {
            int32_t displacement = (int16_t)(opcode & 0xffffu);
            uint32_t target = dspic33_internal_program_address_add(cpu->pc, displacement * 2);
            if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
                dspic33_internal_raise_program_target_error(cpu, cpu->pc);
                return true;
            }
            cpu->pc = target;
        }
        return true;
    }
    if ((opcode & 0xff0000u) == 0x050000u) {
        uint16_t literal = (uint16_t)((opcode >> 4u) & 0x03ffu);
        uint8_t destination = (uint8_t)(opcode & 0x0fu);
        if (cpu->call_depth == 0u) {
            if ((opcode & 0x004000u) != 0u) {
                dspic33_internal_write_working_register_byte(cpu, destination, false,
                                                              (uint8_t)literal);
            } else {
                dspic33_internal_write_working_register(cpu, destination, literal);
            }
            cpu->stop_reason = DSPIC33_RETURNED;
            return true;
        }
        uint32_t return_pc = cpu->pc;
        uint32_t target = dspic33_internal_pop_program_counter(cpu);
        cpu->instruction_working_register_writes &= UINT16_C(0x7fff);
        cpu->pc = target;
        if ((opcode & 0x004000u) != 0u) {
            dspic33_internal_write_working_register_byte(cpu, destination, false, (uint8_t)literal);
        } else {
            dspic33_internal_write_working_register(cpu, destination, literal);
        }
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_raise_program_target_error(cpu, return_pc);
        }
        return true;
    }
    if (opcode == 0xfe0000u) {
        dspic33_internal_perform_warm_reset(cpu, 0x0040u, DSPIC33_RESET_SOFTWARE);
        return true;
    }
    if (opcode == 0xfe2000u) {
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
        dspic33_internal_clear_watchdog(cpu);
        return true;
    }
    if ((opcode & 0xff0000u) == 0x000000u || (opcode & 0xff0000u) == 0xff0000u) {
        return true;
    }
    return false;
}

bool dspic33_initialize_for_device(Dspic33* cpu, Dspic33epMuDevice device) {
    if (dspic33ep_mu_profile(device) == NULL) {
        return false;
    }
    memset(cpu, 0, sizeof(*cpu));
    cpu->device = device;
    cpu->program = calloc(DSPIC33_PROGRAM_WORDS, sizeof(*cpu->program));
    cpu->auxiliary_program =
        calloc(DSPIC33_AUXILIARY_PROGRAM_WORDS, sizeof(*cpu->auxiliary_program));
    cpu->persistent_program =
        calloc(DSPIC33_PERSISTENT_PROGRAM_WORDS, sizeof(*cpu->persistent_program));
    cpu->data = calloc(DSPIC33_DATA_SIZE, sizeof(*cpu->data));
    cpu->initialized_data = calloc(DSPIC33_DATA_SIZE, sizeof(*cpu->initialized_data));
    cpu->var_write_domains = calloc(DSPIC33_DATA_SIZE / 4u, sizeof(*cpu->var_write_domains));
    if (cpu->program == NULL || cpu->auxiliary_program == NULL || cpu->persistent_program == NULL ||
        cpu->data == NULL || cpu->initialized_data == NULL || cpu->var_write_domains == NULL) {
        dspic33_release(cpu);
        return false;
    }
    memset(cpu->program, 0xff, DSPIC33_PROGRAM_WORDS * sizeof(*cpu->program));
    memset(cpu->auxiliary_program, 0xff,
           DSPIC33_AUXILIARY_PROGRAM_WORDS * sizeof(*cpu->auxiliary_program));
    memset(cpu->persistent_program, 0xff,
           DSPIC33_PERSISTENT_PROGRAM_WORDS * sizeof(*cpu->persistent_program));
    for (size_t latch_index = 0u; latch_index < DSPIC33_WRITE_LATCH_WORDS; latch_index++) {
        cpu->write_latches[latch_index] = 0x00ffffffu;
    }
    memset(cpu->configuration, 0xff, sizeof(cpu->configuration));
    for (size_t configuration_index = 0u;
         configuration_index < sizeof(dspic33_internal_configuration_factory_defaults) /
                                   sizeof(dspic33_internal_configuration_factory_defaults[0]);
         configuration_index++) {
        cpu->configuration[4u + configuration_index * 2u] =
            dspic33_internal_configuration_factory_defaults[configuration_index];
    }
    return true;
}

bool dspic33_initialize(Dspic33* cpu) {
    return dspic33_initialize_for_device(cpu, DSPIC33EP_MU_DEVICE_512MU810);
}

void dspic33_release(Dspic33* cpu) {
    if (cpu == NULL) {
        return;
    }
    free(cpu->program);
    free(cpu->auxiliary_program);
    free(cpu->persistent_program);
    free(cpu->data);
    free(cpu->initialized_data);
    free(cpu->var_write_domains);
    free(cpu->events.items);
    cpu->program = NULL;
    cpu->auxiliary_program = NULL;
    cpu->persistent_program = NULL;
    cpu->data = NULL;
    cpu->initialized_data = NULL;
    cpu->var_write_domains = NULL;
    cpu->events.items = NULL;
    cpu->events.count = 0u;
    cpu->events.capacity = 0u;
}

Dspic33* dspic33_create(void) { return dspic33_create_for_device(DSPIC33EP_MU_DEVICE_512MU810); }

Dspic33* dspic33_create_for_device(Dspic33epMuDevice device) {
    Dspic33* cpu = malloc(sizeof(*cpu));
    if (cpu == NULL || !dspic33_initialize_for_device(cpu, device)) {
        free(cpu);
        return NULL;
    }
    return cpu;
}

const Dspic33epMuProfile* dspic33_device_profile(const Dspic33* cpu) {
    return cpu == NULL ? NULL : dspic33ep_mu_profile(cpu->device);
}

void dspic33_destroy(Dspic33* cpu) {
    dspic33_release(cpu);
    free(cpu);
}

bool dspic33_copy(Dspic33* destination, const Dspic33* source) {
    if (destination == NULL || source == NULL) {
        return false;
    }
    Dspic33Event* events = destination->events.items;
    size_t event_capacity = destination->events.capacity;
    uint32_t* program = destination->program;
    uint32_t* auxiliary_program = destination->auxiliary_program;
    uint32_t* persistent_program = destination->persistent_program;
    uint8_t* data = destination->data;
    uint8_t* initialized_data = destination->initialized_data;
    uint8_t* var_write_domains = destination->var_write_domains;
    Dspic33Trace trace = destination->trace;
    void* trace_context = destination->trace_context;
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
    memcpy(initialized_data, source->initialized_data, DSPIC33_DATA_SIZE);
    memcpy(var_write_domains, source->var_write_domains, DSPIC33_DATA_SIZE / 4u);
    if (source->events.count != 0u) {
        memcpy(events, source->events.items, source->events.count * sizeof(*source->events.items));
    }
    *destination = *source;
    destination->program = program;
    destination->auxiliary_program = auxiliary_program;
    destination->persistent_program = persistent_program;
    destination->data = data;
    destination->initialized_data = initialized_data;
    destination->var_write_domains = var_write_domains;
    destination->events.items = events;
    destination->events.capacity = event_capacity;
    destination->trace = trace;
    destination->trace_context = trace_context;
    return true;
}

void dspic33_internal_reset_processor(Dspic33* cpu, uint32_t entry, bool clear_memory) {
    memset(cpu->data, 0, clear_memory ? DSPIC33_DATA_SIZE : 0x1000u);
    if (clear_memory) {
        memset(cpu->initialized_data, 0, DSPIC33_DATA_SIZE);
    }
    memset(cpu->initialized_data, 1, 0x1000u);
    dspic33_clear_uninitialized_data_reads(cpu);
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
    dspic33_cancel_flash_read_sequence(cpu);
    dspic33_internal_clear_instruction_transients(cpu);
    memset(cpu->var_write_domains, 0, DSPIC33_DATA_SIZE / 4u);
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
    cpu->reset_occurred = false;
    cpu->reset_instruction_timing = false;
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
    for (size_t latch_index = 0u; latch_index < DSPIC33_WRITE_LATCH_WORDS; latch_index++) {
        cpu->write_latches[latch_index] = 0x00ffffffu;
    }
    cpu->power_state = DSPIC33_POWER_ACTIVE;
    cpu->stop_reason = DSPIC33_RUNNING;
    dspic33_device_reset(cpu);
}
