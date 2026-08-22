#include "architecture/dspic33/exceptions/internal.h"

static void prepare_nested_do_interrupt_case(TestState* state, Dspic33* cpu, uint32_t entry,
                                             bool nesting_disabled) {
    uint32_t address;
    reset_processor_test(cpu, entry);
    for (address = 0x204u; address <= 0x208u; address += 2u) {
        load_instruction(state, cpu, address, OPCODE_NOP);
    }
    load_instruction(state, cpu, 0x0014u, 0x000300u);
    load_instruction(state, cpu, 0x0016u, 0x000320u);
    for (address = 0x300u; address <= 0x30au; address += 2u) {
        load_instruction(state, cpu, address, OPCODE_NOP);
    }
    load_instruction(state, cpu, 0x30cu, OPCODE_RETFIE);
    load_instruction(state, cpu, 0x320u, OPCODE_NOP);
    load_instruction(state, cpu, 0x322u, OPCODE_RETFIE);
    cpu->w[15] = 0x5000u;
    cpu->do_depth = 1u;
    cpu->do_start[0] = 0x204u;
    cpu->do_end[0] = 0x208u;
    cpu->do_count[0] = 3u;
    cpu->dostart = 0x204u;
    cpu->doend = 0x208u;
    cpu->dcount = 3u;
    cpu->sr |= 0x0200u;
    cpu->corcon = (uint16_t)((cpu->corcon & ~0x0700u) | 0x0100u);
    dspic33_write_word(cpu, 0x08c0u, nesting_disabled ? 0x8000u : 0u);
    dspic33_write_word(cpu, 0x0820u, 0x0003u);
    dspic33_write_word(cpu, 0x0840u, 0x0042u);
    dspic33_write_word(cpu, 0x08c2u, 0x8000u);
}

static void complete_first_nested_do_interrupt_entry(TestState* state, Dspic33* cpu,
                                                     bool expected_armed,
                                                     bool expected_extra_decrement) {
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u && cpu->cycles == 10u &&
               cpu->interrupt_depth == 1u &&
               (cpu->nested_do_extra_decrement_depth != 0u) == expected_extra_decrement &&
               cpu->nested_do_interrupt_armed == expected_armed,
           "first DO-loop interrupt entry evaluates nested request timing");
    dspic33_write_word(cpu, 0x0800u, (uint16_t)(dspic33_read_word(cpu, 0x0800u) & ~1u));
}

static void enter_first_nested_do_interrupt(TestState* state, Dspic33* cpu, bool expected_armed,
                                            uint8_t nested_delay, bool expected_extra_decrement) {
    dspic33_raise_interrupt(cpu, 0u);
    expect(state, cpu->nested_do_interrupt_armed == expected_armed,
           "first DO-loop interrupt request records the erratum window");
    if (nested_delay != 0u) {
        expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, nested_delay),
               "schedule higher-priority request inside interrupt entry");
    }
    complete_first_nested_do_interrupt_entry(state, cpu, expected_armed && nested_delay == 0u,
                                             expected_extra_decrement);
}

static void complete_nested_do_interrupt_case(TestState* state, Dspic33* cpu,
                                              bool expected_extra_decrement) {
    uint8_t index;
    uint8_t main_steps;
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x322u &&
               cpu->interrupt_depth == 2u &&
               (cpu->nested_do_extra_decrement_depth != 0u) == expected_extra_decrement,
           "higher-priority nested interrupt evaluates the exact four-cycle window");
    dspic33_write_word(cpu, 0x0800u, 0u);
    dspic33_write_word(cpu, 0x0820u, 0u);
    for (index = 0u; cpu->interrupt_depth != 0u && index < 8u; index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "nested DO-loop interrupt handlers return normally");
    }
    expect(state, cpu->interrupt_depth == 0u, "nested DO-loop interrupt stack fully unwinds");
    main_steps = (uint8_t)(((0x208u - cpu->pc) / 2u) + 1u);
    for (index = 0u; index < main_steps; index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "interrupted DO-loop reaches its iteration boundary");
    }
    expect(state,
           cpu->pc == 0x204u && cpu->do_depth == 1u &&
               cpu->dcount == (expected_extra_decrement ? 1u : 2u) &&
               cpu->do_count[0] == cpu->dcount && cpu->nested_do_extra_decrement_depth == 0u &&
               cpu->nested_do_extra_decrement_end == 0u && !cpu->nested_do_interrupt_armed,
           "DO-loop iteration applies only the documented nested decrement");
}

static void nested_do_interrupt_erratum_cases(TestState* state, Dspic33* cpu) {
    static const uint32_t entries[] = {0x206u, 0x208u};
    static const uint8_t delays[] = {3u, 4u, 5u};
    static const uint16_t divisors[] = {0x1800u, 0x9800u};
    static const uint8_t divided_deadlines[] = {10u, 6u};
    Dspic33 copy;
    size_t entry_index;
    size_t delay_index;
    size_t divisor_index;

    prepare_nested_do_interrupt_case(state, cpu, 0x206u, false);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "schedule both interrupts from the executing DO instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x208u && cpu->cycles == 1u &&
               cpu->interrupt_depth == 0u && cpu->nested_do_interrupt_armed &&
               cpu->nested_do_interrupt_end == 0x208u && cpu->nested_do_interrupt_depth == 1u,
           "penultimate DO instruction event records the executing address");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u && cpu->cycles == 11u &&
               cpu->interrupt_depth == 1u && cpu->nested_do_extra_decrement_depth == 1u &&
               !cpu->nested_do_interrupt_armed,
           "scheduled higher-priority request reaches the exact entry-cycle deadline");
    dspic33_write_word(cpu, 0x0800u, (uint16_t)(dspic33_read_word(cpu, 0x0800u) & ~1u));
    complete_nested_do_interrupt_case(state, cpu, true);

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, false);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "schedule nested requests from the final DO instruction");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x204u && cpu->dcount == 2u &&
               cpu->nested_do_interrupt_armed && cpu->nested_do_interrupt_end == 0x208u,
           "final DO instruction event survives the loop-back PC update");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->nested_do_extra_decrement_depth == 1u && !cpu->nested_do_interrupt_armed,
           "final DO instruction request participates in the four-cycle erratum");

    prepare_nested_do_interrupt_case(state, cpu, 0x204u, false);
    expect(state,
           dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 1u) &&
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 5u),
           "schedule nested requests outside the final DO instructions");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x206u &&
               !cpu->nested_do_interrupt_armed,
           "earlier DO instruction event does not arm the erratum");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
               cpu->nested_do_extra_decrement_depth == 0u && !cpu->nested_do_interrupt_armed,
           "entry-time second request cannot become a replacement first request");

    for (divisor_index = 0u; divisor_index < sizeof(divisors) / sizeof(divisors[0]);
         divisor_index++) {
        prepare_nested_do_interrupt_case(state, cpu, 0x206u, false);
        dspic33_write_word(cpu, 0x0744u, divisors[divisor_index]);
        expect(state,
               dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 0u, 0u, 2u) &&
                   dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u,
                                    divided_deadlines[divisor_index]),
               "schedule nested requests across a divided instruction boundary");
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->device_cycles == 2u &&
                   cpu->nested_do_interrupt_armed,
               "divided DO instruction records its interrupt request cycle");
        expect(state,
               dspic33_step(cpu) == DSPIC33_RUNNING && cpu->pc == 0x302u &&
                   cpu->nested_do_extra_decrement_depth == 1u && !cpu->nested_do_interrupt_armed,
               "DOZE and ROI preserve the four-instruction-cycle erratum window");
    }

    for (entry_index = 0u; entry_index < sizeof(entries) / sizeof(entries[0]); entry_index++) {
        for (delay_index = 0u; delay_index < sizeof(delays) / sizeof(delays[0]); delay_index++) {
            prepare_nested_do_interrupt_case(state, cpu, entries[entry_index], false);
            enter_first_nested_do_interrupt(state, cpu, true, delays[delay_index],
                                            delays[delay_index] == 4u);
            complete_nested_do_interrupt_case(state, cpu, delays[delay_index] == 4u);
        }
    }

    prepare_nested_do_interrupt_case(state, cpu, 0x204u, false);
    enter_first_nested_do_interrupt(state, cpu, false, 4u, false);
    complete_nested_do_interrupt_case(state, cpu, false);

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, false);
    enter_first_nested_do_interrupt(state, cpu, true, 0u, false);
    dspic33_write_word(cpu, 0x0820u, 0u);
    for (delay_index = 0u; cpu->interrupt_depth != 0u && delay_index < 8u; delay_index++) {
        expect(state, dspic33_step(cpu) == DSPIC33_RUNNING,
               "single DO-loop interrupt handler returns normally");
    }
    expect(state, cpu->interrupt_depth == 0u && cpu->pc == 0x208u && cpu->nested_do_interrupt_armed,
           "single DO-loop interrupt retains its window until the loop boundary");
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->dcount == 2u &&
               !cpu->nested_do_interrupt_armed && cpu->nested_do_interrupt_cycle == 0u &&
               cpu->nested_do_interrupt_end == 0u && cpu->nested_do_interrupt_depth == 0u &&
               cpu->nested_do_interrupt_priority == 0u,
           "DO-loop boundary expires an unused nested-interrupt window");

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, true);
    enter_first_nested_do_interrupt(state, cpu, false, 4u, false);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->interrupt_depth == 1u &&
               cpu->interrupt_count == 1u && cpu->nested_do_extra_decrement_depth == 0u,
           "NSTDIS prevents the nested DO-loop erratum sequence");

    prepare_nested_do_interrupt_case(state, cpu, 0x206u, false);
    dspic33_raise_interrupt(cpu, 0u);
    expect(state, cpu->nested_do_interrupt_armed,
           "copy source records the first DO-loop interrupt request");
    expect(state, dspic33_schedule(cpu, DSPIC33_EVENT_INTERRUPT, 1u, 0u, 4u),
           "copy source schedules the exact nested request");
    expect(state, dspic33_initialize(&copy), "initialize nested DO-loop erratum copy");
    expect(state, dspic33_copy(&copy, cpu), "copy armed nested DO-loop erratum state");
    complete_first_nested_do_interrupt_entry(state, cpu, false, true);
    complete_first_nested_do_interrupt_entry(state, &copy, false, true);
    complete_nested_do_interrupt_case(state, cpu, true);
    complete_nested_do_interrupt_case(state, &copy, true);
    dspic33_release(&copy);

    prepare_nested_do_interrupt_case(state, cpu, 0x208u, false);
    enter_first_nested_do_interrupt(state, cpu, true, 0u, false);
    load_instruction(state, cpu, 0x302u, OPCODE_RESET);
    dspic33_write_word(cpu, 0x0800u, 0u);
    dspic33_write_word(cpu, 0x0820u, 0u);
    expect(state,
           dspic33_step(cpu) == DSPIC33_RUNNING && cpu->software_reset_count == 1u &&
               !cpu->nested_do_interrupt_armed && cpu->nested_do_interrupt_cycle == 0u &&
               cpu->nested_do_interrupt_end == 0u && cpu->nested_do_interrupt_depth == 0u &&
               cpu->nested_do_interrupt_priority == 0u &&
               cpu->nested_do_extra_decrement_depth == 0u &&
               cpu->nested_do_extra_decrement_end == 0u,
           "warm reset clears nested DO-loop erratum state");
}

int main(void) {
    TestState state = {0u, 0u, 0u};
    Dspic33 cpu;
    bool initialized = dspic33_initialize(&cpu);
    expect(&state, initialized, "processor fault test initializes");
    if (initialized) {
        dspic33_fault_test_program_target_address_error_cases(&state, &cpu);
        dspic33_fault_test_program_read_address_error_cases(&state, &cpu);
        dspic33_fault_test_compare_skip_cases(&state, &cpu);
        dspic33_fault_test_compare_branch_target_cases(&state, &cpu);
        dspic33_fault_test_skip_boundary_cases(&state, &cpu);
        dspic33_fault_test_address_error_cases(&state, &cpu);
        dspic33_fault_test_data_map_address_error_cases(&state, &cpu);
        dspic33_fault_test_pseudo_linear_page_cases(&state, &cpu);
        dspic33_fault_test_page_zero_address_error_cases(&state, &cpu);
        dspic33_fault_test_unimplemented_data_page_address_error_cases(&state, &cpu);
        dspic33_fault_test_w15_write_cases(&state, &cpu);
        dspic33_fault_test_valid_stack_frame_cases(&state, &cpu);
        dspic33_fault_test_invalid_lnk_case(&state, &cpu);
        dspic33_fault_test_invalid_ulnk_case(&state, &cpu);
        dspic33_fault_test_simultaneous_trap_case(&state, &cpu);
        dspic33_fault_test_earlier_deadline_case(&state, &cpu);
        dspic33_fault_test_repeat_exception_cases(&state, &cpu);
        dspic33_fault_test_standalone_divide_zero_cases(&state, &cpu);
        dspic33_fault_test_repeat_interrupt_cases(&state, &cpu);
        nested_do_interrupt_erratum_cases(&state, &cpu);
        dspic33_release(&cpu);
    }
    return test_finish(&state);
}
