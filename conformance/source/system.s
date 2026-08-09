.section .text,code
.include "conformance.inc"

.global _system_conformance_cases
_system_conformance_cases = 12
.global _system_conformance_terminal_count
_system_conformance_terminal_count = 20
.global _system_conformance_group_complete
_system_conformance_group_complete = 1

.global _run_system_probe
_run_system_probe:
    cp w0, #1
    bra z, _system_sleep_probe
    cp w0, #2
    bra z, _system_idle_probe
    cp w0, #3
    bra z, _system_reset_probe
    cp w0, #4
    bra z, _system_divide_zero_probe
    cp w0, #5
    bra z, _system_sftac_probe
    cp w0, #6
    bra z, _system_sftac_consecutive_probe
    cp w0, #7
    bra z, _system_sftac_repeat_probe
    cp w0, #8
    bra z, _system_do_overflow_probe
    cp w0, #9
    bra z, _system_stack_limit_probe
    cp w0, #10
    bra z, _system_address_error_probe
    cp w0, #11
    bra z, _system_shift_address_error_probe
    cp w0, #12
    bra z, _system_unary_address_error_probe
    cp w0, #13
    bra z, _system_binary_address_error_probe
    cp w0, #14
    bra z, _system_multi_operand_control_probe
    cp w0, #15
    bra z, _system_data_limit_control_probe
    cp w0, #16
    bra z, _system_unimplemented_read_probe
    cp w0, #17
    bra z, _system_unimplemented_write_probe
    cp w0, #18
    bra z, _system_unimplemented_wrap_read_probe
    cp w0, #19
    bra z, _system_unimplemented_wrap_write_probe
    cp w0, #20
    bra z, _system_unused_sfr_probe
    return

.global _system_sleep_probe
_system_sleep_probe:
    pwrsav #0
    return

.global _system_idle_probe
_system_idle_probe:
    pwrsav #1
    return

.global _system_reset_probe
_system_reset_probe:
    mov #0x5a5a, w1
    mov w1, _system_reset_state
    reset
    return

.global _system_divide_zero_probe
_system_divide_zero_probe:
    mov #42, w2
    clr w3
    repeat #17
    div.s w2, w3
    return

.global _system_sftac_probe
_system_sftac_probe:
    mov #0x1234, w4
    lac w4, A
    set_status 0x010f
    mov #17, w5
    sftac A, w5
    mov INTCON1, w1
    mov #0x2222, w2
    return

.global _system_sftac_consecutive_probe
_system_sftac_consecutive_probe:
    mov #0x1234, w4
    lac w4, A
    set_status 0x010f
    mov #17, w5
    mov #0x1111, w1
    sftac A, w5
    sftac B, w5
    mov #0x2222, w2
    return

.global _system_sftac_repeat_probe
_system_sftac_repeat_probe:
    mov #0x1234, w4
    lac w4, A
    set_status 0x010f
    mov #17, w5
    mov #0x1111, w1
    repeat #1
    sftac A, w5
    mov #0x2222, w2
    return

.global _system_do_overflow_probe
_system_do_overflow_probe:
    set_status 0x010f
    mov #0x1111, w1
    do #1, system_do_outer_end
    do #1, system_do_level_2_end
    do #1, system_do_level_3_end
    do #1, system_do_level_4_end
    do #1, system_do_level_5_end
    mov #0x2222, w1
    nop
system_do_level_5_end:
    nop
    nop
system_do_level_4_end:
    nop
    nop
system_do_level_3_end:
    nop
    nop
system_do_level_2_end:
    nop
    nop
system_do_outer_end:
    nop
    return

.global _system_stack_limit_probe
_system_stack_limit_probe:
    mov #0x5000, w15
    mov w15, w14
    mov #0xa5a5, w6
    mov w6, [w15+10]
    clr w1
    clr w2
    clr w3
    clr w6
    add w15, #8, w0
    mov w0, SPLIM
    set_status 0x010f
    nop
    mov [w15+10], w6
    mov #0x1111, w1
    mov #0x2222, w2
    mov #0x3333, w3
    return

.global _system_address_error_probe
_system_address_error_probe:
    mov #0x5000, w15
    mov #_system_address_trap_buffer, w1
    clr w0
    mov w0, [w1]
    mov w0, [w1+2]
    inc w1, w1
    mov #0xa5a5, w2
    clr w3
    set_status 0x010f
    mov w2, [w1++]
    mov #0x1111, w3
    return

_system_prepare_multi_operand_address_error:
    mov #0x0008, w0
    mov w0, T2CON
    mov #0x5555, w0
    mov w0, TMR3HLD
    mov #0x1234, w0
    mov w0, TMR2
    mov #0xaaaa, w0
    mov w0, TMR3HLD
    mov #_system_multi_operand_buffer, w5
    clr w0
    clr [w5]
    mov w0, [w5+2]
    inc w5, w5
    mov #0x0106, w4
    clr w3
    return

.global _system_shift_address_error_probe
_system_shift_address_error_probe:
    mov #0x5000, w15
    rcall _system_prepare_multi_operand_address_error
    asr [w4++], [w5--]
    mov #0x1111, w3
    return

.global _system_unary_address_error_probe
_system_unary_address_error_probe:
    mov #0x5000, w15
    rcall _system_prepare_multi_operand_address_error
    neg [w4++], [w5--]
    mov #0x1111, w3
    return

.global _system_binary_address_error_probe
_system_binary_address_error_probe:
    mov #0x5000, w15
    rcall _system_prepare_multi_operand_address_error
    mov #2, w2
    add w2, [w4++], [w5--]
    mov #0x1111, w3
    return

.global _system_multi_operand_control_probe
_system_multi_operand_control_probe:
    mov #_system_multi_operand_buffer, w4
    mov #0x8001, w0
    mov w0, [w4]
    clr w0
    mov w0, [w4+2]
    asr [w4++], [w4]
    mov w4, _system_multi_operand_control_state
    mov [w4], w0
    mov w0, _system_multi_operand_control_state+2
    mov #_system_multi_operand_buffer, w4
    mov #1, w0
    mov w0, [w4]
    clr w0
    mov w0, [w4+2]
    neg [w4++], [w4]
    mov w4, _system_multi_operand_control_state+4
    mov [w4], w0
    mov w0, _system_multi_operand_control_state+6
    mov #_system_multi_operand_buffer, w4
    mov #1, w0
    mov w0, [w4]
    clr w0
    mov w0, [w4+2]
    mov #2, w2
    add w2, [w4++], [w4]
    mov w4, _system_multi_operand_control_state+8
    mov [w4], w0
    mov w0, _system_multi_operand_control_state+10
.global _system_multi_operand_control_complete
_system_multi_operand_control_complete:
    bra _system_multi_operand_control_complete

.global _system_data_limit_control_probe
_system_data_limit_control_probe:
    mov #0xdffe, w1
    mov #0xa5a5, w0
    mov w0, [w1]
    mov [w1++], w2
    mov w1, _system_data_map_control_state
    mov w2, _system_data_map_control_state+2
    mov #0xdffe, w1
    mov #0x5a5a, w2
    mov w2, [w1++]
    mov w1, _system_data_map_control_state+4
    mov #0xdffe, w1
    mov [w1], w0
    mov w0, _system_data_map_control_state+6
    mov INTCON1, w0
    mov w0, _system_data_map_control_state+8
    mov #0x3333, w3
    mov w3, _system_data_map_control_state+10
    bra _system_data_map_control_complete

.global _system_unimplemented_read_probe
_system_unimplemented_read_probe:
    mov #0x5000, w15
    mov #0xe000, w1
    mov #0x5a5a, w2
    clr w3
    set_status 0x010f
    mov [w1++], w2
    mov #0x1111, w3
    return

.global _system_unimplemented_write_probe
_system_unimplemented_write_probe:
    mov #0x5000, w15
    mov #0xe000, w1
    mov #0xa5a5, w2
    clr w3
    set_status 0x010f
    mov w2, [w1++]
    mov #0x1111, w3
    return

.global _system_unimplemented_wrap_read_probe
_system_unimplemented_wrap_read_probe:
    mov #0x5000, w15
    mov #0xfffe, w1
    mov #0x5a5a, w2
    clr w3
    set_status 0x010f
    mov [w1++], w2
    mov #0x1111, w3
    return

.global _system_unimplemented_wrap_write_probe
_system_unimplemented_wrap_write_probe:
    mov #0x5000, w15
    mov #0xfffe, w1
    mov #0xa5a5, w2
    clr w3
    set_status 0x010f
    mov w2, [w1++]
    mov #0x1111, w3
    return

.global _system_unused_sfr_probe
_system_unused_sfr_probe:
    mov #0x0056, w1
    mov #0x5a5a, w2
    mov [w1++], w2
    mov w1, _system_data_map_control_state
    mov w2, _system_data_map_control_state+2
    mov INTCON1, w0
    mov w0, _system_data_map_control_state+4
    mov #0x3333, w3
    mov w3, _system_data_map_control_state+6
.global _system_data_map_control_complete
_system_data_map_control_complete:
    bra _system_data_map_control_complete

.global __AddressError
__AddressError:
    mov w1, _system_address_trap_state
    mov _system_address_trap_buffer, w0
    mov w0, _system_address_trap_state+2
    mov _system_address_trap_buffer+2, w0
    mov w0, _system_address_trap_state+4
    mov INTCON1, w0
    mov w0, _system_address_trap_state+6
    mov [w15-4], w0
    mov w0, _system_address_trap_state+8
    mov [w15-2], w0
    mov w0, _system_address_trap_state+10
    mov w3, _system_address_trap_state+12
    mov INTTREG, w0
    mov w0, _system_address_trap_state+14
    mov SR, w0
    mov w0, _system_address_trap_state+16
    mov w15, _system_address_trap_state+18
    mov TMR3HLD, w0
    mov w0, _system_multi_operand_trap_state
    mov w4, _system_multi_operand_trap_state+2
    mov w5, _system_multi_operand_trap_state+4
    mov _system_multi_operand_buffer, w0
    mov w0, _system_multi_operand_trap_state+6
    mov _system_multi_operand_buffer+2, w0
    mov w0, _system_multi_operand_trap_state+8
    mov w3, _system_multi_operand_trap_state+10
    mov INTCON1, w0
    mov w0, _system_multi_operand_trap_state+12
    mov [w15-4], w0
    mov w0, _system_multi_operand_trap_state+14
    mov [w15-2], w0
    mov w0, _system_multi_operand_trap_state+16
    mov INTTREG, w0
    mov w0, _system_multi_operand_trap_state+18
    mov w1, _system_data_map_trap_state
    mov w2, _system_data_map_trap_state+2
    mov w3, _system_data_map_trap_state+4
    mov INTCON1, w0
    mov w0, _system_data_map_trap_state+6
    mov [w15-4], w0
    mov w0, _system_data_map_trap_state+8
    mov [w15-2], w0
    mov w0, _system_data_map_trap_state+10
    mov INTTREG, w0
    mov w0, _system_data_map_trap_state+12
    mov SR, w0
    mov w0, _system_data_map_trap_state+14
    mov w15, _system_data_map_trap_state+16
    mov DSRPAG, w0
    mov w0, _system_data_map_trap_state+18
    mov DSWPAG, w0
    mov w0, _system_data_map_trap_state+20
.global _system_address_trap_complete
_system_address_trap_complete:
    bra _system_address_trap_complete

.global __StackError
__StackError:
    mov INTCON1, w0
    mov w0, _system_stack_trap_state
    mov SPLIM, w0
    mov w0, _system_stack_trap_state+2
    mov w14, _system_stack_trap_state+4
    mov w15, _system_stack_trap_state+6
    mov [w15-4], w0
    mov w0, _system_stack_trap_state+8
    mov [w15-2], w0
    mov w0, _system_stack_trap_state+10
    mov INTTREG, w0
    mov w0, _system_stack_trap_state+12
    mov w1, _system_stack_trap_state+14
    mov w2, _system_stack_trap_state+16
    mov w3, _system_stack_trap_state+18
    mov w6, _system_stack_trap_state+20
    mov SR, w0
    mov w0, _system_stack_trap_state+22
.global _system_stack_trap_complete
_system_stack_trap_complete:
    bra _system_stack_trap_complete

.global __MathError
__MathError:
    mov [w15-4], w0
    mov w0, _system_trap_state
    mov [w15-2], w0
    mov w0, _system_trap_state+2
    mov INTCON1, w0
    mov w0, _system_trap_state+4
    mov INTTREG, w0
    mov w0, _system_trap_state+6
    mov ACCAL, w0
    mov w0, _system_trap_state+8
    mov ACCAH, w0
    mov w0, _system_trap_state+10
    mov ACCAU, w0
    mov w0, _system_trap_state+12
    mov w1, _system_trap_state+14
    mov w2, _system_trap_state+16
    mov SR, w0
    mov w0, _system_trap_state+18
.global _system_math_trap_complete
_system_math_trap_complete:
    bra _system_math_trap_complete

.global __SoftTrapError
__SoftTrapError:
    mov [w15-4], w0
    mov w0, _system_trap_state
    mov [w15-2], w0
    mov w0, _system_trap_state+2
    mov INTCON3, w0
    mov w0, _system_trap_state+4
    mov INTTREG, w0
    mov w0, _system_trap_state+6
    mov CORCON, w0
    mov w0, _system_trap_state+8
    mov DCOUNT, w0
    mov w0, _system_trap_state+10
    mov DOSTARTL, w0
    mov w0, _system_trap_state+12
    mov DOSTARTH, w0
    mov w0, _system_trap_state+14
    mov DOENDL, w0
    mov w0, _system_trap_state+16
    mov DOENDH, w0
    mov w0, _system_trap_state+18
    mov w1, _system_trap_state+20
    mov SR, w0
    mov w0, _system_trap_state+22
.global _system_soft_trap_complete
_system_soft_trap_complete:
    bra _system_soft_trap_complete

.global _run_system_conformance
_run_system_conformance:
    begin_results

    mov #0x1234, w1
    set_status 0x0000
    nop
    record_case 0x0c00, w1, w1

    mov #0xabcd, w1
    set_status 0x010f
    nop
    record_case 0x0c01, w1, w1

    mov #0x5678, w1
    set_status 0x0000
    nopr
    record_case 0x0c02, w1, w1

    mov #0xcdef, w1
    set_status 0x010f
    nopr
    record_case 0x0c03, w1, w1

    mov #0x9abc, w1
    set_status 0x010f
    .pword 0xff1234
    record_case 0x0c04, w1, w1

    mov #0xdef0, w1
    set_status 0x0000
    .pword 0xffffff
    record_case 0x0c05, w1, w1

    mov #0x0003, w0
    mov w0, RCON
    bset RCON, #4
    set_status 0x0000
    clrwdt
    mov RCON, w1
    record_case 0x0c06, w1, w1

    mov #0x0003, w0
    mov w0, RCON
    bset RCON, #4
    set_status 0x010f
    clrwdt
    mov RCON, w1
    record_case 0x0c07, w1, w1

    set_status 0x0000
    disi #0
    mov DISICNT, w1
    mov DISICNT, w2
    record_case 0x0c08, w1, w2

    set_status 0x010f
    disi #1
    mov DISICNT, w1
    mov DISICNT, w2
    record_case 0x0c09, w1, w2

    set_status 0x0000
    disi #3
    mov DISICNT, w1
    mov DISICNT, w2
    record_case 0x0c0a, w1, w2

    set_status 0x010f
    disi #16383
    mov DISICNT, w1
    mov DISICNT, w2
    record_case 0x0c0b, w1, w2

    disi #0
    end_results
