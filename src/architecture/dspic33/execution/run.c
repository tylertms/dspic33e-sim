#include "internal.h"

uint8_t dspic33_read_byte(Dspic33* cpu, uint32_t address) {
    uint8_t read_value;
    dspic33_internal_record_data_read(cpu, address, 1u);
    cpu->io.cpu_read_address = address;
    cpu->io.cpu_read_width = 1u;
    cpu->io.cpu_read_valid = true;
    read_value = dspic33_internal_read_byte_value(cpu, address);
    cpu->io.cpu_read_valid = false;
    return read_value;
}

uint16_t dspic33_read_word(Dspic33* cpu, uint32_t address) {
    uint16_t low;
    uint16_t high;
    if (!dspic33_internal_check_data_alignment(cpu, address) ||
        !dspic33_internal_check_data_implementation(cpu, address, 2u) ||
        (cpu->address_error && !cpu->address_error_access_allowed)) {
        return 0u;
    }
    dspic33_internal_record_data_read(cpu, address, 2u);
    cpu->io.cpu_read_address = address;
    cpu->io.cpu_read_width = 2u;
    cpu->io.cpu_read_valid = true;
    low = dspic33_internal_read_byte_value(cpu, address);
    high = dspic33_internal_read_byte_value(cpu, address + 1u);
    cpu->io.cpu_read_valid = false;
    return (uint16_t)(low | (high << 8u));
}

static void complete_do_loop_iteration(Dspic33* cpu) {
    uint8_t depth = (uint8_t)(cpu->do_depth - 1u);
    if (cpu->do_count[depth] != 0u && cpu->do_terminate[depth] != 1u) {
        cpu->do_count[depth]--;
        if (cpu->do_terminate[depth] == 2u) {
            cpu->do_terminate[depth] = 1u;
        }
        cpu->dcount = cpu->do_count[depth];
        cpu->pc = cpu->do_start[depth];
        return;
    }
    cpu->do_terminate[depth] = 0u;
    cpu->do_depth--;
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0700u) | ((uint16_t)cpu->do_depth << 8u));
    if (cpu->do_depth == 0u) {
        cpu->dostart = 0u;
        cpu->doend = 0u;
        cpu->dcount = 0u;
        cpu->sr &= 0xfdffu;
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
    uint8_t do_depth_before_instruction;
    uint8_t interrupt_entry_overlap;
    uint64_t cycles;
    uint32_t opcode;
    uint32_t instruction_pc;
    bool exception_dispatched;
    bool sequential_hole_fetch;
    bool non_cpu_sfr_wait;
    bool power_save_next;
    bool psv_read;
    bool psv_repeat_access;
    bool psv_repeat_exit_latency;
    uint64_t base_cycles;
    uint64_t device_ratio;
    if (cpu->reset_locked) {
        cpu->illegal_reset = true;
        return cpu->stop_reason;
    }
    cpu->reset_occurred = false;
    cpu->reset_instruction_timing = false;
    cpu->illegal_reset = false;
    if (cpu->nvm.active && cpu->nvm.reset_pending) {
        dspic33_internal_advance_pending_nvm_reset(cpu);
        return cpu->stop_reason;
    }
    if (cpu->nvm.active && dspic33_internal_nvm_stalls_cpu(cpu)) {
        if (dspic33_internal_nvm_stall_erratum_applies(cpu) &&
            (!cpu->nvm.stall_workaround || (dspic33_read_word(cpu, 0x08c2u) & 0x8000u) != 0u)) {
            cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
            return cpu->stop_reason;
        }
        if (!dspic33_device_advance_nvm(cpu)) {
            cpu->stop_reason = DSPIC33_EVENT_QUEUE_ERROR;
        }
        return cpu->stop_reason;
    }
    if (cpu->power_state != DSPIC33_POWER_ACTIVE) {
        if (!dspic33_device_wake(cpu)) {
            cpu->stop_reason =
                cpu->power_state == DSPIC33_POWER_SLEEP ? DSPIC33_SLEEPING : DSPIC33_IDLING;
            return cpu->stop_reason;
        }
        if (cpu->reset_occurred || cpu->nvm.reset_pending) {
            if (cpu->nvm.reset_pending) {
                dspic33_internal_advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        }
        cpu->previous_working_register_writes = 0u;
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        cpu->watchdog.ticks = 0u;
        cpu->stop_reason = DSPIC33_RUNNING;
    } else {
        uint32_t next_opcode =
            (cpu->pc & 1u) == 0u && dspic33_device_program_range_implemented(cpu, cpu->pc, 2u)
                ? dspic33_read_program_word(cpu, cpu->pc)
                : 0u;
        if (!dspic33_internal_system_encoding_valid(next_opcode)) {
            dspic33_internal_perform_warm_reset(cpu, 0x4000u, DSPIC33_RESET_ILLEGAL);
            return cpu->stop_reason;
        }
        power_save_next =
            !cpu->nvm.active && !dspic33_internal_vector_segment_execution_address(cpu->pc) &&
            (cpu->pc & 1u) == 0u && dspic33_device_program_range_implemented(cpu, cpu->pc, 2u) &&
            (next_opcode & 0xfffffeu) == 0xfe4000u;
        exception_dispatched = dspic33_internal_service_pending_soft_trap(cpu);
        if (!exception_dispatched && !power_save_next) {
            exception_dispatched = dspic33_device_service_interrupt(cpu);
        }
        if (cpu->reset_occurred || cpu->nvm.reset_pending) {
            if (cpu->nvm.reset_pending) {
                dspic33_internal_advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        }
        if (exception_dispatched) {
            cpu->sequential_program_hole_pc = 0u;
            cpu->previous_working_register_writes = 0u;
            dspic33_cancel_flash_read_sequence(cpu);
        }
    }
    cpu->interrupt_entry_overlap = 0u;
    sequential_hole_fetch = cpu->sequential_program_hole_pc == cpu->pc &&
                            dspic33_internal_program_target_requires_address_error(cpu, cpu->pc);
    if (dspic33_internal_vector_segment_execution_address(cpu->pc)) {
        uint32_t return_pc = dspic33_internal_program_address_add(cpu->pc, 2);
        device_ratio = dspic33_device_instruction_cycles(cpu, 1u);
        cpu->sequential_program_hole_pc = 0u;
        dspic33_cancel_flash_read_sequence(cpu);
        dspic33_internal_enter_address_trap(cpu, return_pc);
        if (!cpu->reset_occurred || cpu->nvm.reset_pending) {
            dspic33_internal_advance_instruction(cpu, 1u, false, device_ratio);
        }
        return cpu->stop_reason;
    }
    if ((cpu->pc & 1u) != 0u ||
        (!dspic33_device_program_range_implemented(cpu, cpu->pc, 2u) && !sequential_hole_fetch)) {
        cpu->sequential_program_hole_pc = 0u;
        cpu->stop_reason = DSPIC33_PROGRAM_BOUNDS;
        return cpu->stop_reason;
    }
    instruction_pc = cpu->pc;
    device_ratio = dspic33_device_instruction_cycles(cpu, 1u);
    opcode = sequential_hole_fetch ? 0u : dspic33_read_program_word(cpu, cpu->pc);
    cpu->sequential_program_hole_pc = 0u;
    if (opcode == 0x064000u) {
        uint64_t return_cycles;
        uint32_t target;
        bool dependency_stall = (cpu->previous_working_register_writes & UINT16_C(0x8000)) != 0u;
        cpu->current_instruction_pc = instruction_pc;
        dspic33_cancel_flash_read_sequence(cpu);
        dspic33_device_return_interrupt(cpu);
        target = cpu->pc;
        cpu->instructions++;
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_enter_address_trap(
                cpu, dspic33_internal_program_address_add(instruction_pc, 2));
            return_cycles = 5u;
        } else if (!dspic33_codeguard_admit_program_flow(cpu, instruction_pc, target)) {
            if (cpu->nvm.reset_pending) {
                dspic33_internal_advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        } else {
            return_cycles = dspic33_internal_exception_pending(cpu) ? 5u : 6u;
        }
        if (cpu->reset_occurred && !cpu->nvm.reset_pending) {
            return cpu->stop_reason;
        }
        return_cycles += dependency_stall ? 1u : 0u;
        cpu->previous_working_register_writes = 0u;
        dspic33_internal_advance_instruction(cpu, return_cycles, false, device_ratio);
        return cpu->stop_reason;
    }
    if (opcode == 0x060000u) {
        uint64_t return_cycles;
        uint32_t target;
        bool dependency_stall = (cpu->previous_working_register_writes & UINT16_C(0x8000)) != 0u;
        cpu->current_instruction_pc = instruction_pc;
        dspic33_cancel_flash_read_sequence(cpu);
        if (cpu->call_depth == 0u) {
            cpu->stop_reason = DSPIC33_RETURNED;
            return cpu->stop_reason;
        }
        target = dspic33_internal_pop_program_counter(cpu);
        cpu->pc = target;
        cpu->instructions++;
        if (dspic33_internal_program_target_requires_address_error(cpu, target)) {
            dspic33_internal_enter_address_trap(
                cpu, dspic33_internal_program_address_add(instruction_pc, 2));
            return_cycles = 5u;
        } else if (!dspic33_codeguard_admit_program_flow(cpu, instruction_pc, target)) {
            if (cpu->nvm.reset_pending) {
                dspic33_internal_advance_pending_nvm_reset(cpu);
            }
            return cpu->stop_reason;
        } else {
            return_cycles = dspic33_internal_exception_pending(cpu) ? 5u : 6u;
        }
        if (cpu->reset_occurred && !cpu->nvm.reset_pending) {
            return cpu->stop_reason;
        }
        return_cycles += dependency_stall ? 1u : 0u;
        cpu->previous_working_register_writes = 0u;
        dspic33_internal_advance_instruction(cpu, return_cycles, false, device_ratio);
        return cpu->stop_reason;
    }
    memcpy(working_registers, cpu->w, sizeof(working_registers));
    initialized_working_registers = cpu->initialized_working_registers;
    memcpy(accumulators, cpu->accumulator, sizeof(accumulators));
    status = cpu->sr;
    control = cpu->corcon;
    cpu->pc = dspic33_internal_program_address_add(cpu->pc, 2);
    cpu->instructions++;
    cpu->non_cpu_sfr_read = false;
    cpu->psv_read = false;
    cpu->psv_repeat_optimized = false;
    cpu->instruction_working_register_writes = 0u;
    cpu->instruction_source_address_registers = 0u;
    do_depth_before_instruction = cpu->do_depth;
    cpu->current_instruction_cycles =
        (uint8_t)dspic33_internal_instruction_cycles(cpu, opcode, instruction_pc);
    cpu->current_instruction_pc = instruction_pc;
    cpu->instruction_active = true;
    dspic33_internal_clear_instruction_transients(cpu);
    cpu->address_error = false;
    cpu->address_error_access_allowed = false;
    cpu->address_error_working_state_completed = false;
    cpu->address_error_accumulator_state_completed = false;
    cpu->address_error_control_state_completed = false;
    if (!dspic33_internal_execute(cpu, opcode) && !cpu->address_error && !cpu->reset_occurred) {
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
        dspic33_internal_clear_instruction_transients(cpu);
        return cpu->stop_reason;
    }
    if (!cpu->address_error && !cpu->reset_occurred &&
        cpu->pc != dspic33_internal_program_address_add(
                       instruction_pc, (int32_t)dspic33_internal_instruction_length(opcode))) {
        if (!dspic33_codeguard_admit_program_flow(cpu, instruction_pc, cpu->pc)) {
            cpu->instruction_active = false;
            if (cpu->nvm.reset_pending) {
                dspic33_internal_advance_pending_nvm_reset(cpu);
            }
            dspic33_internal_clear_instruction_transients(cpu);
            return cpu->stop_reason;
        }
    }
    cpu->instruction_active = false;
    if (cpu->reset_occurred && !cpu->reset_instruction_timing && !cpu->nvm.reset_pending) {
        dspic33_internal_clear_instruction_transients(cpu);
        return cpu->stop_reason;
    }
    base_cycles = dspic33_internal_instruction_cycles(cpu, opcode, instruction_pc);
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
    if (!psv_repeat_access &&
        (cpu->previous_working_register_writes & cpu->instruction_source_address_registers) != 0u) {
        cycles++;
    }
    psv_repeat_exit_latency = cpu->psv_repeat_optimized && cycles == 1u;
    non_cpu_sfr_wait = cpu->non_cpu_sfr_read;
    psv_read = cpu->psv_read;
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
        dspic33_cancel_flash_read_sequence(cpu);
        if (control_state_completed) {
            dspic33_internal_enter_address_trap(cpu, return_pc);
        } else {
            dspic33_internal_enter_trap(cpu, 1u, 0x000006u, 14u, 0x0008u, return_pc, false);
        }
        if (cpu->reset_occurred && !cpu->nvm.reset_pending) {
            dspic33_internal_clear_instruction_transients(cpu);
            return cpu->stop_reason;
        }
        dspic33_internal_advance_instruction(cpu, cycles, non_cpu_sfr_wait, device_ratio);
        dspic33_internal_clear_instruction_transients(cpu);
        return cpu->stop_reason;
    }
    if (dspic33_internal_flash_read_erratum_sequence_completed(cpu, opcode, instruction_pc,
                                                               psv_read)) {
        cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
    }
    if (dspic33_internal_do_flash_access_boundary(cpu, opcode, instruction_pc, psv_read)) {
        cpu->stop_reason = DSPIC33_SILICON_RESULT_UNDEFINED;
    }
    cpu->previous_working_register_writes = cpu->instruction_working_register_writes;
    if (cpu->repeat_active != 0u && instruction_pc == cpu->repeat_pc) {
        if (cpu->rcount != 0u) {
            cpu->rcount--;
            cpu->pc = cpu->repeat_pc;
            if (cpu->rcount == 0u) {
                cpu->sr &= 0xffefu;
            }
        } else {
            cpu->repeat_active = 0u;
            cpu->repeat_psv_started = false;
            cpu->repeat_psv_reentry = false;
            cpu->sr &= 0xffefu;
        }
    }
    if (cpu->do_depth != 0u && cpu->do_depth <= do_depth_before_instruction &&
        instruction_pc == cpu->do_end[cpu->do_depth - 1u]) {
        uint8_t depth = (uint8_t)(cpu->do_depth - 1u);
        bool extra_decrement = cpu->nested_do_extra_decrement_depth == cpu->do_depth &&
                               cpu->nested_do_extra_decrement_end == instruction_pc;
        uint32_t sequential_pc = cpu->pc;
        if (cpu->nested_do_interrupt_armed && cpu->nested_do_interrupt_depth == cpu->do_depth &&
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
         cpu->pc == instruction_pc + dspic33_internal_instruction_length(opcode) ||
         (sequential_hole_fetch && cpu->repeat_active != 0u && cpu->pc == instruction_pc)) &&
                dspic33_internal_program_target_requires_address_error(cpu, cpu->pc)
            ? cpu->pc
            : 0u;
    cpu->instruction_advancing = true;
    dspic33_internal_advance_instruction(cpu, cycles, non_cpu_sfr_wait, device_ratio);
    if (cpu->reset_occurred || cpu->nvm.reset_pending) {
        cpu->instruction_advancing = false;
        dspic33_internal_clear_instruction_transients(cpu);
        return cpu->stop_reason;
    }
    if (psv_repeat_exit_latency && cpu->repeat_active != 0u &&
        dspic33_device_interrupt_pending(cpu)) {
        dspic33_internal_advance_instruction(cpu, 4u, false, device_ratio);
        if (cpu->reset_occurred || cpu->nvm.reset_pending) {
            cpu->instruction_advancing = false;
            dspic33_internal_clear_instruction_transients(cpu);
            return cpu->stop_reason;
        }
    }
    if (disicnt_before_instruction != 0u && cpu->disicnt == 0u &&
        dspic33_device_interrupt_pending(cpu)) {
        cpu->interrupt_entry_overlap = interrupt_entry_overlap;
    }
    cpu->instruction_advancing = false;
    dspic33_internal_clear_instruction_transients(cpu);
    if (cpu->power_state != DSPIC33_POWER_ACTIVE && dspic33_device_wake(cpu)) {
        cpu->power_state = DSPIC33_POWER_ACTIVE;
        dspic33_device_power_state_changed(cpu);
        cpu->watchdog.ticks = 0u;
        cpu->stop_reason = DSPIC33_RUNNING;
    }
    return cpu->stop_reason;
}

static Dspic33StopReason run(Dspic33* cpu, uint64_t instruction_limit, uint32_t stop_address,
                             bool stop_enabled) {
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

static Dspic33Result make_result(const Dspic33* cpu) {
    if (cpu == NULL) {
        return (Dspic33Result){DSPIC33_HALTED, 0u, 0u, 0u, 0u};
    }
    return (Dspic33Result){cpu->stop_reason, cpu->instructions, cpu->cycles, cpu->pc,
                           dspic33_read_program_word(cpu, cpu->current_instruction_pc)};
}

Dspic33Result dspic33_step_result(Dspic33* cpu) {
    if (cpu == NULL) {
        return make_result(cpu);
    }
    dspic33_step(cpu);
    return make_result(cpu);
}

Dspic33Result dspic33_run_with_limits(Dspic33* cpu, Dspic33RunLimits limits) {
    if (cpu == NULL) {
        return make_result(cpu);
    }
    const uint64_t start_instructions = cpu->instructions;
    const uint64_t start_cycles = cpu->cycles;
    cpu->stop_reason = DSPIC33_RUNNING;
    while (cpu->stop_reason == DSPIC33_RUNNING) {
        if ((limits.instruction_limit != 0u &&
             cpu->instructions - start_instructions >= limits.instruction_limit) ||
            (limits.cycle_limit != 0u && cpu->cycles - start_cycles >= limits.cycle_limit)) {
            Dspic33Result limited = make_result(cpu);
            limited.stop = DSPIC33_INSTRUCTION_LIMIT;
            return limited;
        }
        dspic33_step(cpu);
    }
    return make_result(cpu);
}

uint32_t dspic33_get_register(const Dspic33* cpu, uint8_t reg) {
    return cpu != NULL && reg < 16u ? cpu->w[reg] : 0u;
}

uint32_t dspic33_get_program_counter(const Dspic33* cpu) { return cpu != NULL ? cpu->pc : 0u; }

uint32_t dspic33_get_executed_program_counter(const Dspic33* cpu) {
    return cpu != NULL ? cpu->current_instruction_pc : 0u;
}

uint64_t dspic33_get_instruction_count(const Dspic33* cpu) {
    return cpu != NULL ? cpu->instructions : 0u;
}

uint64_t dspic33_get_cycle_count(const Dspic33* cpu) { return cpu != NULL ? cpu->cycles : 0u; }

Dspic33StopReason dspic33_get_stop(const Dspic33* cpu) {
    return cpu != NULL ? cpu->stop_reason : DSPIC33_HALTED;
}

uint32_t dspic33_get_fault_address(const Dspic33* cpu) {
    if (cpu == NULL) {
        return 0u;
    }
    if (cpu->stop_reason != DSPIC33_TRAPPED) {
        return cpu->pc;
    }
    return cpu->address_error_return != 0u ? cpu->address_error_return
                                           : cpu->current_instruction_pc;
}

uint64_t dspic33_get_trap_count(const Dspic33* cpu) { return cpu != NULL ? cpu->trap_count : 0u; }

uint64_t dspic33_get_interrupt_count(const Dspic33* cpu) {
    return cpu != NULL ? cpu->interrupt_count : 0u;
}

uint16_t dspic33_get_last_interrupt(const Dspic33* cpu) {
    return cpu != NULL ? cpu->last_interrupt : UINT16_MAX;
}

uint8_t dspic33_get_interrupt_depth(const Dspic33* cpu) {
    return cpu != NULL ? cpu->interrupt_depth : 0u;
}

void dspic33_set_stop_on_trap(Dspic33* cpu, bool enabled) {
    if (cpu != NULL) {
        cpu->stop_on_trap = enabled;
    }
}

bool dspic33_begin_call(Dspic33* cpu, uint32_t address, bool async_events) {
    if (cpu == NULL || (address & 1u) != 0u ||
        !dspic33_device_program_range_implemented(cpu, address, 2u)) {
        return false;
    }
    cpu->pc = address;
    cpu->call_depth = 0u;
    cpu->stop_reason = DSPIC33_RUNNING;
    dspic33_set_async_events(cpu, async_events);
    return true;
}

bool dspic33_seed_data(Dspic33* cpu, uint32_t address, const void* data, size_t size) {
    if (cpu == NULL || (data == NULL && size != 0u) || size > UINT32_MAX ||
        !dspic33_data_range_valid(address, (uint32_t)size)) {
        return false;
    }
    memcpy(cpu->data + address, data, size);
    return true;
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
    case DSPIC33_SILICON_RESULT_UNDEFINED:
        return "silicon result undefined";
    }
    return "unknown";
}
