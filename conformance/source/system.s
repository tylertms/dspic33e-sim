.section .text,code
.include "conformance.inc"

.global _system_conformance_cases
_system_conformance_cases = 12
.global _system_conformance_terminal_count
_system_conformance_terminal_count = 47
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
    cp w0, #21
    bra z, _system_page_zero_direct_probe
    cp w0, #22
    bra z, _system_page_zero_word_read_probe
    cp w0, #23
    bra z, _system_page_zero_word_write_probe
    cp w0, #24
    bra z, _system_page_zero_byte_read_probe
    cp w0, #25
    bra z, _system_page_zero_byte_write_probe
    cp w0, #26
    bra z, _system_eds_page_word_read_probe
    cp w0, #27
    bra z, _system_eds_page_word_write_probe
    cp w0, #28
    bra z, _system_eds_page_byte_read_probe
    cp w0, #29
    bra z, _system_eds_page_byte_write_probe
    cp w0, #30
    bra z, _system_eds_page_move_double_read_probe
    cp w0, #31
    bra z, _system_eds_page_move_double_write_probe
    cp w0, #32
    bra z, _system_program_target_goto_probe
    cp w0, #33
    bra z, _system_program_target_call_probe
    cp w0, #34
    bra z, _system_program_target_goto_long_probe
    cp w0, #35
    bra z, _system_program_target_call_long_probe
    cp w0, #36
    bra z, _system_program_target_return_probe
    cp w0, #37
    bra z, _system_program_target_retfie_probe
    cp w0, #38
    bra z, _system_program_target_bra_dispatch
    cp w0, #39
    bra z, _system_program_target_rcall_dispatch
    cp w0, #40
    bra z, _system_program_target_retlw_probe
    cp w0, #41
    bra z, _system_program_read_table_probe
    cp w0, #42
    bra z, _system_sequential_hole_dispatch
    cp w0, #43
    bra z, _system_program_read_high_probe
    cp w0, #44
    bra z, _system_program_read_high_byte_probe
    cp w0, #45
    bra z, _system_program_read_low_byte_probe
    cp w0, #46
    bra z, _system_program_read_collision_probe
    cp w0, #47
    bra z, _system_program_read_stack_probe
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

.global _system_page_zero_direct_probe
_system_page_zero_direct_probe:
    clr w0
    mov w0, DSRPAG
    mov w0, DSWPAG
    mov #0xa5a5, w2
    mov w2, 0x1000
    clr w3
    mov 0x1000, w3
    clr w4
    mov w3, _system_page_zero_control_state
    mov w4, _system_page_zero_control_state+2
    mov 0x1000, w0
    mov w0, _system_page_zero_control_state+4
    mov 0x1002, w0
    mov w0, _system_page_zero_control_state+6
    mov DSRPAG, w0
    mov w0, _system_page_zero_control_state+8
    mov DSWPAG, w0
    mov w0, _system_page_zero_control_state+10
    mov INTCON1, w0
    mov w0, _system_page_zero_control_state+12
    mov #0x3333, w3
    mov w3, _system_page_zero_control_state+14
    bra _system_page_zero_control_complete

_system_prepare_page_zero_probe:
    mov #0x5a5a, w0
    mov w0, 0x1000
    clr w3
    return

.global _system_page_zero_word_read_probe
_system_page_zero_word_read_probe:
    mov #0x5000, w15
    rcall _system_prepare_page_zero_probe
    clr w0
    mov w0, DSRPAG
    mov #1, w0
    mov w0, DSWPAG
    mov #0x9000, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov [w1++], w2
    mov #0x1111, w3
    return

.global _system_page_zero_word_write_probe
_system_page_zero_word_write_probe:
    mov #0x5000, w15
    rcall _system_prepare_page_zero_probe
    mov #1, w0
    mov w0, DSRPAG
    clr w0
    mov w0, DSWPAG
    mov #0x9000, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov w2, [w1++]
    mov #0x1111, w3
    return

.global _system_page_zero_byte_read_probe
_system_page_zero_byte_read_probe:
    mov #0x5000, w15
    rcall _system_prepare_page_zero_probe
    clr w0
    mov w0, DSRPAG
    mov #1, w0
    mov w0, DSWPAG
    mov #0x9001, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov.b [w1++], w2
    mov #0x1111, w3
    return

.global _system_page_zero_byte_write_probe
_system_page_zero_byte_write_probe:
    mov #0x5000, w15
    rcall _system_prepare_page_zero_probe
    mov #1, w0
    mov w0, DSRPAG
    clr w0
    mov w0, DSWPAG
    mov #0x9001, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov.b w2, [w1++]
    mov #0x1111, w3
    return

.global _system_page_zero_control_complete
_system_page_zero_control_complete:
    bra _system_page_zero_control_complete

_system_prepare_eds_page_probe:
    mov #0x3303, w3
    mov #0x4404, w4
    clr w5
    return

.global _system_eds_page_word_read_probe
_system_eds_page_word_read_probe:
    mov #0x5000, w15
    rcall _system_prepare_eds_page_probe
    mov #2, w0
    mov w0, DSRPAG
    mov #1, w0
    mov w0, DSWPAG
    mov #0x9000, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov [w1++], w2
    mov #0x1111, w5
    return

.global _system_eds_page_word_write_probe
_system_eds_page_word_write_probe:
    mov #0x5000, w15
    rcall _system_prepare_eds_page_probe
    mov #1, w0
    mov w0, DSRPAG
    mov #2, w0
    mov w0, DSWPAG
    mov #0x9000, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov w2, [w1++]
    mov #0x1111, w5
    return

.global _system_eds_page_byte_read_probe
_system_eds_page_byte_read_probe:
    mov #0x5000, w15
    rcall _system_prepare_eds_page_probe
    mov #2, w0
    mov w0, DSRPAG
    mov #1, w0
    mov w0, DSWPAG
    mov #0x9001, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov.b [w1++], w2
    mov #0x1111, w5
    return

.global _system_eds_page_byte_write_probe
_system_eds_page_byte_write_probe:
    mov #0x5000, w15
    rcall _system_prepare_eds_page_probe
    mov #1, w0
    mov w0, DSRPAG
    mov #2, w0
    mov w0, DSWPAG
    mov #0x9001, w1
    mov #0xa5a5, w2
    set_status 0x010f
    mov.b w2, [w1++]
    mov #0x1111, w5
    return

.global _system_eds_page_move_double_read_probe
_system_eds_page_move_double_read_probe:
    mov #0x5000, w15
    rcall _system_prepare_eds_page_probe
    mov #2, w0
    mov w0, DSRPAG
    mov #1, w0
    mov w0, DSWPAG
    mov #0x1101, w1
    mov #0x9000, w4
    mov #0xa5a5, w2
    mov #0x5a5a, w3
    set_status 0x010f
    mov.d [w4], w2
    mov #0x1111, w5
    return

.global _system_eds_page_move_double_write_probe
_system_eds_page_move_double_write_probe:
    mov #0x5000, w15
    rcall _system_prepare_eds_page_probe
    mov #1, w0
    mov w0, DSRPAG
    mov #2, w0
    mov w0, DSWPAG
    mov #0x9000, w1
    mov #0x5555, w2
    mov #0x6666, w3
    set_status 0x010f
    mov.d w2, [w1++]
    mov #0x1111, w5
    return

.equ system_invalid_program_target, 0x55800

.global _system_program_target_goto_probe
_system_program_target_goto_probe:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    goto system_invalid_program_target
    mov #0x1111, w1
    return

.global _system_program_target_call_probe
_system_program_target_call_probe:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    call system_invalid_program_target
    mov #0x1111, w1
    return

.global _system_program_target_goto_long_probe
_system_program_target_goto_long_probe:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    mov #0x5800, w0
    mov #0x0005, w1
    goto.l w0
    mov #0x1111, w1
    return

.global _system_program_target_call_long_probe
_system_program_target_call_long_probe:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    mov #0x5800, w0
    mov #0x0005, w1
    call.l w0
    mov #0x1111, w1
    return

.global _system_program_target_return_probe
_system_program_target_return_probe:
    rcall _system_program_target_return_body
    return

_system_program_target_return_body:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    mov #0x5800, w0
    mov w0, [w15++]
    mov #0x0005, w0
    mov w0, [w15++]
    return
    mov #0x1111, w1
    return

.global _system_program_target_retfie_probe
_system_program_target_retfie_probe:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    mov #0x5800, w0
    mov w0, [w15++]
    mov #0x0f05, w0
    mov w0, [w15++]
    retfie
    mov #0x1111, w1
    return

.global _system_program_target_retlw_probe
_system_program_target_retlw_probe:
    mov #0x5000, w15
    clr w0
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    mov #0x5800, w0
    mov w0, [w15++]
    mov #0x0005, w0
    mov w0, [w15++]
    clr w0
    retlw #0x122, w1
    mov #0x1111, w0
    return

.global _system_program_read_table_probe
_system_program_read_table_probe:
    mov #0x5000, w15
    clr w0
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    mov #0x0005, w0
    mov w0, TBLPAG
    mov #0x5800, w1
    clr w0
    tblrdl [w1], w0
    mov #0x1111, w1
    return

.macro prepare_program_read_probe source
    mov #0x5000, w15
    clr w3
    mov #0x010f, w0
    mov w0, SR
    lnk #0
    mov #0x5000, w15
    mov #0x0005, w0
    mov w0, TBLPAG
    mov #\source, w1
    mov #0xa5a5, w0
    mov w0, _system_program_read_buffer
.endm

.global _system_program_read_high_probe
_system_program_read_high_probe:
    prepare_program_read_probe 0x5800
    mov #0xaaaa, w2
    tblrdh [w1], w2
    mov #0x1111, w3
    return

.global _system_program_read_high_byte_probe
_system_program_read_high_byte_probe:
    prepare_program_read_probe 0x5800
    mov #0xaaaa, w2
    tblrdh.b [w1], w2
    mov #0x1111, w3
    return

.global _system_program_read_low_byte_probe
_system_program_read_low_byte_probe:
    prepare_program_read_probe 0x5801
    mov #0xaaaa, w2
    tblrdl.b [w1], w2
    mov #0x1111, w3
    return

.global _system_program_read_collision_probe
_system_program_read_collision_probe:
    prepare_program_read_probe 0x5800
    mov #_system_program_read_buffer+1, w2
    tblrdl [w1++], [w2++]
    mov #0x1111, w3
    return

.global _system_program_read_stack_probe
_system_program_read_stack_probe:
    prepare_program_read_probe 0x5800
    mov #0xaaaa, w2
    mov #0xa5a5, w0
    mov w0, [w15]
    tblrdl [w1], [w15++]
    mov #0x1111, w3
    return

.global _system_program_target_bra_dispatch
_system_program_target_bra_dispatch:
    goto _system_program_target_bra_probe

.global _system_program_target_rcall_dispatch
_system_program_target_rcall_dispatch:
    goto _system_program_target_rcall_probe

.global _system_sequential_hole_dispatch
_system_sequential_hole_dispatch:
    mov #0x4242, w1
    mov w1, _system_sequential_hole_state
    goto _system_sequential_hole_probe

.section .system_program_target_near_limit,code,address(0x557d0)
.global _system_program_target_bra_probe
_system_program_target_bra_probe:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    bra system_invalid_program_target
    mov #0x1111, w1
    return

.global _system_program_target_rcall_probe
_system_program_target_rcall_probe:
    mov #0x5000, w15
    clr w1
    mov #0x010f, w2
    mov w2, SR
    lnk #0
    mov #0x5000, w15
    rcall system_invalid_program_target
    mov #0x1111, w1
    return

.global _system_sequential_hole_probe
_system_sequential_hole_probe = 0x557fe

.global _system_sequential_hole_complete
_system_sequential_hole_complete = 0x55804

.section .text,code

.global __AddressError
__AddressError:
    mov w0, _system_program_target_trap_state
    mov w1, _system_program_target_trap_state+2
    mov w15, _system_program_target_trap_state+4
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
    mov w1, _system_page_zero_trap_state
    mov w2, _system_page_zero_trap_state+2
    mov w3, _system_page_zero_trap_state+4
    mov 0x1000, w0
    mov w0, _system_page_zero_trap_state+6
    mov INTCON1, w0
    mov w0, _system_page_zero_trap_state+8
    mov [w15-4], w0
    mov w0, _system_page_zero_trap_state+10
    mov [w15-2], w0
    mov w0, _system_page_zero_trap_state+12
    mov INTTREG, w0
    mov w0, _system_page_zero_trap_state+14
    mov SR, w0
    mov w0, _system_page_zero_trap_state+16
    mov w15, _system_page_zero_trap_state+18
    mov DSRPAG, w0
    mov w0, _system_page_zero_trap_state+20
    mov DSWPAG, w0
    mov w0, _system_page_zero_trap_state+22
    mov w1, _system_eds_page_trap_state
    mov w2, _system_eds_page_trap_state+2
    mov w3, _system_eds_page_trap_state+4
    mov w4, _system_eds_page_trap_state+6
    mov w5, _system_eds_page_trap_state+8
    mov INTCON1, w0
    mov w0, _system_eds_page_trap_state+10
    mov [w15-4], w0
    mov w0, _system_eds_page_trap_state+12
    mov [w15-2], w0
    mov w0, _system_eds_page_trap_state+14
    mov INTTREG, w0
    mov w0, _system_eds_page_trap_state+16
    mov SR, w0
    mov w0, _system_eds_page_trap_state+18
    mov w15, _system_eds_page_trap_state+20
    mov DSRPAG, w0
    mov w0, _system_eds_page_trap_state+22
    mov DSWPAG, w0
    mov w0, _system_eds_page_trap_state+24
    mov INTCON1, w0
    mov w0, _system_program_target_trap_state+6
    mov [w15-4], w0
    mov w0, _system_program_target_trap_state+8
    mov [w15-2], w0
    mov w0, _system_program_target_trap_state+10
    mov [w15-8], w0
    mov w0, _system_program_target_trap_state+12
    mov [w15-6], w0
    mov w0, _system_program_target_trap_state+14
    mov INTTREG, w0
    mov w0, _system_program_target_trap_state+16
    mov SR, w0
    mov w0, _system_program_target_trap_state+18
    mov CORCON, w0
    mov w0, _system_program_target_trap_state+20
    mov w1, _system_program_read_trap_state
    mov w2, _system_program_read_trap_state+2
    mov w3, _system_program_read_trap_state+4
    mov w15, _system_program_read_trap_state+6
    mov INTCON1, w0
    mov w0, _system_program_read_trap_state+8
    mov [w15-4], w0
    mov w0, _system_program_read_trap_state+10
    mov [w15-2], w0
    mov w0, _system_program_read_trap_state+12
    mov INTTREG, w0
    mov w0, _system_program_read_trap_state+14
    mov SR, w0
    mov w0, _system_program_read_trap_state+16
    mov CORCON, w0
    mov w0, _system_program_read_trap_state+18
    mov TBLPAG, w0
    mov w0, _system_program_read_trap_state+20
    mov _system_program_read_buffer, w0
    mov w0, _system_program_read_trap_state+22
    mov 0x5000, w0
    mov w0, _system_program_read_trap_state+24
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
