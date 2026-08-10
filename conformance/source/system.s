.section .text,code
.include "conformance.inc"

.global _system_conformance_cases
_system_conformance_cases = 12
.global _system_conformance_terminal_count
_system_conformance_terminal_count = 76
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
    cp w0, #48
    bra z, _system_program_boundary_dispatch
    cp w0, #49
    bra z, _system_program_boundary_dispatch
    cp w0, #50
    bra z, _system_skip_one_word_dispatch
    cp w0, #51
    bra z, _system_skip_two_word_dispatch
    cp w0, #52
    bra z, _system_skip_one_word_dispatch
    cp w0, #53
    bra z, _system_skip_two_word_dispatch
    cp w0, #54
    bra z, _system_program_boundary_dispatch
    cp w0, #55
    bra z, _system_sequential_hole_dispatch
    cp w0, #56
    bra z, _system_do_boundary_dispatch
    cp w0, #57
    bra z, _system_repeat_divide_probe
    cp w0, #58
    bra z, _system_repeat_irq_probe
    cp w0, #59
    bra z, _system_sfr_wait_bset_probe
    cp w0, #60
    bra z, _system_sfr_wait_move_probe
    cp w0, #61
    bra z, _system_sfr_wait_move_double_probe
    cp w0, #62
    bra z, _system_pseudo_linear_probe
    cp w0, #63
    bra z, _system_pseudo_linear_move_double_probe
    cp w0, #64
    bra z, _system_dsp_x_prefetch_probe
    cp w0, #65
    bra z, _system_dsp_x_fault_probe
    cp w0, #66
    bra z, _system_dsp_x_program_fault_probe
    cp w0, #67
    bra z, _system_psv_program_fault_probe
    cp w0, #68
    bra z, _system_psv_program_byte_fault_probe
    cp w0, #69
    bra z, _system_psv_program_double_fault_probe
    cp w0, #70
    bra z, _system_psv_repeat_probe
    cp w0, #71
    bra z, _system_auxiliary_program_probe
    cp w0, #72
    bra z, _system_move_file_load_fault_probe
    cp w0, #73
    bra z, _system_move_file_rmw_fault_probe
    cp w0, #74
    bra z, _system_move_file_store_fault_probe
    cp w0, #75
    bra z, _system_crc_lane_probe
    cp w0, #76
    bra z, _system_output_compare_sync_probe
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

.global _system_repeat_divide_probe
_system_repeat_divide_probe:
    mov #57, w0
    mov w0, _system_probe_selector
    mov #0x5000, w15
    clr INTCON1
    bset INTCON2, #15
    set_status 0x010f
    mov #42, w2
    clr w3
    repeat #17
.global _system_repeat_divide_target
_system_repeat_divide_target:
    div.s w2, w3
    return

.global _system_repeat_irq_probe
_system_repeat_irq_probe:
    mov #0x5000, w15
    clr IFS1
    clr IEC1
    clr IPC5
    bset INTCON2, #15
    mov #4, w0
    mov w0, IPC5
    mov #0x0010, w1
    mov w1, IEC1
    clr w2
    disi #2
    mov w1, IFS1
    repeat #2
.global _system_repeat_irq_target
_system_repeat_irq_target:
    inc w2, w2
    mov w15, _system_repeat_irq_state+12
    mov w2, _system_repeat_irq_state+14
    mov RCOUNT, w0
    mov w0, _system_repeat_irq_state+16
    mov SR, w0
    and #0x00f0, w0
    mov w0, _system_repeat_irq_state+18
.global _system_repeat_irq_complete
_system_repeat_irq_complete:
    bra _system_repeat_irq_complete

.global _system_sfr_wait_bset_probe
_system_sfr_wait_bset_probe:
    mov #59, w0
    mov w0, _system_probe_selector
    mov #0x5000, w15
    clr w0
    mov w0, SR
    mov w0, IFS1
    mov w0, IEC1
    mov w0, IPC5
    bset INTCON2, #15
    mov #4, w0
    mov w0, IPC5
    mov #0x0010, w1
    mov w1, IEC1
    disi #2
    bset IFS1, #4
.global _system_sfr_wait_bset_repeat
_system_sfr_wait_bset_repeat:
    repeat #2
    nop
    return

.global _system_sfr_wait_move_probe
_system_sfr_wait_move_probe:
    mov #60, w0
    mov w0, _system_probe_selector
    mov #0x5000, w15
    clr w0
    mov w0, SR
    mov w0, IFS1
    mov w0, IEC1
    mov w0, IPC5
    bset INTCON2, #15
    mov #4, w0
    mov w0, IPC5
    mov #0x0010, w1
    mov w1, IEC1
    disi #3
    mov w1, IFS1
    mov IFS1, w2
.global _system_sfr_wait_move_repeat
_system_sfr_wait_move_repeat:
    repeat #2
    nop
    return

.global _system_sfr_wait_move_double_probe
_system_sfr_wait_move_double_probe:
    mov #61, w0
    mov w0, _system_probe_selector
    mov #0x5000, w15
    clr w0
    mov w0, SR
    mov w0, IFS1
    mov w0, IEC1
    mov w0, IPC5
    bset INTCON2, #15
    mov #4, w0
    mov w0, IPC5
    mov #0x0010, w1
    mov w1, IEC1
    mov #IFS1, w4
    disi #4
    mov w1, IFS1
    mov.d [w4], w6
.global _system_sfr_wait_move_double_repeat
_system_sfr_wait_move_double_repeat:
    repeat #2
    nop
    return

.global _system_pseudo_linear_probe
_system_pseudo_linear_probe:
    mov #0x0200, w0
    mov w0, DSRPAG
    mov #0xffff, w1
    mov #0x1200, w2
    .pword 0x784151
    mov w1, _system_pseudo_linear_state
    mov w2, _system_pseudo_linear_state+2
    mov DSRPAG, w0
    mov w0, _system_pseudo_linear_state+4
    mov #0x0200, w0
    mov w0, DSRPAG
    mov #0xffff, w1
    mov #0x1200, w2
    .pword 0x784131
    mov w1, _system_pseudo_linear_state+6
    mov w2, _system_pseudo_linear_state+8
    mov DSRPAG, w0
    mov w0, _system_pseudo_linear_state+10
    mov #0x0201, w0
    mov w0, DSRPAG
    mov #0x8000, w1
    mov #0x1200, w2
    .pword 0x784141
    mov w1, _system_pseudo_linear_state+12
    mov w2, _system_pseudo_linear_state+14
    mov DSRPAG, w0
    mov w0, _system_pseudo_linear_state+16
    mov #0x0201, w0
    mov w0, DSRPAG
    mov #0x8000, w1
    mov #0x1200, w2
    .pword 0x784121
    mov w1, _system_pseudo_linear_state+18
    mov w2, _system_pseudo_linear_state+20
    mov DSRPAG, w0
    mov w0, _system_pseudo_linear_state+22
.global _system_pseudo_linear_complete
_system_pseudo_linear_complete:
    bra _system_pseudo_linear_complete

.global _system_pseudo_linear_move_double_probe
_system_pseudo_linear_move_double_probe:
    mov #0x5000, w15
    clr INTCON1
    bset INTCON2, #15
    mov #0x0200, w0
    mov w0, DSRPAG
    mov #0xfffe, w1
    clr w2
    clr w3
    mov #0x4444, w4
    mov #0x5555, w5
.global _system_pseudo_linear_move_double
_system_pseudo_linear_move_double:
    .pword 0xbe0131
    return

.global _system_dsp_x_prefetch_probe
_system_dsp_x_prefetch_probe:
    mov #0x0021, w0
    mov w0, CORCON
    clr w0
    mov w0, SR
    mov #0x0200, w0
    mov w0, DSRPAG
    mov #3, w4
    mov #4, w5
    mov #0xfffe, w8
    mov #0x9002, w10
    mov #0x6789, w0
    mov w0, [w10]
    .pword 0xc0045f
    mov ACCAL, w0
    mov w0, _system_dsp_x_prefetch_state
    mov ACCAH, w0
    mov w0, _system_dsp_x_prefetch_state+2
    mov ACCAU, w0
    mov w0, _system_dsp_x_prefetch_state+4
    mov w4, _system_dsp_x_prefetch_state+6
    mov w5, _system_dsp_x_prefetch_state+8
    mov w8, _system_dsp_x_prefetch_state+10
    mov w10, _system_dsp_x_prefetch_state+12
    mov DSRPAG, w0
    mov w0, _system_dsp_x_prefetch_state+14
    mov CORCON, w0
    mov w0, _system_dsp_x_prefetch_state+16
    mov SR, w0
    mov w0, _system_dsp_x_prefetch_state+18
.global _system_dsp_x_prefetch_complete
_system_dsp_x_prefetch_complete:
    bra _system_dsp_x_prefetch_complete

.global _system_dsp_x_fault_probe
_system_dsp_x_fault_probe:
    mov #65, w0
    mov w0, _system_probe_selector
    mov #0x5200, w15
    clr INTCON1
    mov #0x0021, w0
    mov w0, CORCON
    clr w0
    mov w0, SR
    mov #1, w0
    mov w0, DSRPAG
    mov #3, w4
    mov #4, w5
    mov #0x9000, w8
    mov #0x9002, w10
    mov #0xa5a5, w0
    mov w0, [w8]
    mov #0x6789, w0
    mov w0, [w10]
.global _system_dsp_x_fault_instruction
_system_dsp_x_fault_instruction:
    .pword 0xc0041f
    return

.global _system_dsp_x_program_fault_probe
_system_dsp_x_program_fault_probe:
    mov #66, w0
    mov w0, _system_probe_selector
    mov #0x5200, w15
    clr INTCON1
    mov #0x0021, w0
    mov w0, CORCON
    clr w0
    mov w0, SR
    mov #0x020a, w0
    mov w0, DSRPAG
    mov #3, w4
    mov #4, w5
    mov #0xd800, w8
    mov #0x9002, w10
    mov #0x6789, w0
    mov w0, [w10]
.global _system_dsp_x_program_fault_instruction
_system_dsp_x_program_fault_instruction:
    .pword 0xc0041f
    return

.global _system_psv_program_fault_probe
_system_psv_program_fault_probe:
    mov #67, w0
    mov w0, _system_probe_selector
    mov #0x5200, w15
    clr INTCON1
    mov #0x0021, w0
    mov w0, CORCON
    clr w0
    mov w0, SR
    mov #0x020a, w0
    mov w0, DSRPAG
    mov #0xd800, w1
    mov #0xa5a5, w2
    mov #0xbeef, w3
    mov #0x1357, w0
    mov w0, 0x5000
.global _system_psv_program_fault_instruction
_system_psv_program_fault_instruction:
    .pword 0x780131
    mov #0xdead, w3
    return

.global _system_psv_program_byte_fault_probe
_system_psv_program_byte_fault_probe:
    mov #68, w0
    mov w0, _system_probe_selector
    mov #0x5200, w15
    clr INTCON1
    mov #0x0021, w0
    mov w0, CORCON
    clr w0
    mov w0, SR
    mov #0x020a, w0
    mov w0, DSRPAG
    mov #0xd800, w1
    mov #0xa5a5, w2
    mov #0xbeef, w3
    mov #0x1357, w0
    mov w0, 0x5000
.global _system_psv_program_byte_fault_instruction
_system_psv_program_byte_fault_instruction:
    .pword 0x784131
    mov #0xdead, w3
    return

.global _system_psv_program_double_fault_probe
_system_psv_program_double_fault_probe:
    mov #69, w0
    mov w0, _system_probe_selector
    mov #0x5200, w15
    clr INTCON1
    mov #0x0021, w0
    mov w0, CORCON
    clr w0
    mov w0, SR
    mov #0x020a, w0
    mov w0, DSRPAG
    mov #0xd800, w1
    mov #0xa5a5, w2
    mov #0xbeef, w3
    mov #0x1357, w0
    mov w0, 0x5000
.global _system_psv_program_double_fault_instruction
_system_psv_program_double_fault_instruction:
    .pword 0xbe0131
    return

.global _system_psv_repeat_probe
_system_psv_repeat_probe:
    mov #0x0200, w0
    mov w0, DSRPAG
    mov #0xfff8, w1
    clr w2
    disi #31
    repeat #2
    mov [w1++], w2
    mov w1, _system_psv_repeat_state
    mov w2, _system_psv_repeat_state+2
    mov DSRPAG, w0
    mov w0, _system_psv_repeat_state+4
    mov RCOUNT, w0
    mov w0, _system_psv_repeat_state+6
    mov SR, w0
    mov w0, _system_psv_repeat_state+8
    mov CORCON, w0
    mov w0, _system_psv_repeat_state+10
    mov #0x7070, w0
    mov w0, _system_psv_repeat_state+12
.global _system_psv_repeat_complete
_system_psv_repeat_complete:
    bra _system_psv_repeat_complete

.global _system_auxiliary_program_probe
_system_auxiliary_program_probe:
    mov #0x5000, w15
    clr IFS0
    clr IEC0
    clr IPC0
    bset INTCON2, #15
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    goto 0x7fc000

.global _system_auxiliary_program_capture
_system_auxiliary_program_capture:
    mov w3, _system_auxiliary_program_state
    mov INTTREG, w0
    mov w0, _system_auxiliary_program_state+2
    mov [w15-4], w0
    mov w0, _system_auxiliary_program_state+4
    mov [w15-2], w0
    mov w0, _system_auxiliary_program_state+6
    mov w15, _system_auxiliary_program_state+8
    mov #0x007f, w0
    mov w0, TBLPAG
    mov #0xc000, w0
    tblrdl [w0], w4
    mov w4, _system_auxiliary_program_state+10
    mov #0x7171, w0
    mov w0, _system_auxiliary_program_state+12
.global _system_auxiliary_program_complete
_system_auxiliary_program_complete:
    bra _system_auxiliary_program_complete

.global _system_move_file_load_fault_probe
_system_move_file_load_fault_probe:
    mov #72, w0
    mov w0, _system_probe_selector
    mov #0xbf90, w4
    bra _system_move_file_fault_setup

.global _system_move_file_rmw_fault_probe
_system_move_file_rmw_fault_probe:
    mov #73, w0
    mov w0, _system_probe_selector
    mov #0xbfb0, w4
    bra _system_move_file_fault_setup

.global _system_move_file_store_fault_probe
_system_move_file_store_fault_probe:
    mov #74, w0
    mov w0, _system_probe_selector
    mov #0xb7b0, w4

_system_move_file_fault_setup:
    mov #0x5000, w15
    clr INTCON1
    bset INTCON2, #15
    mov #0x1111, w1
    mov #0x3333, w3
    mov #0x5a5a, w2
    mov w2, 0x1000
    mov #0xbf90, w5
    cp w4, w5
    bra z, _system_move_file_load_fault_execute
    mov #0xbfb0, w5
    cp w4, w5
    bra z, _system_move_file_rmw_fault_execute
_system_move_file_store_fault_execute:
    set_status 0x010d
    mov #0xa5a5, w0
.global _system_move_file_store_fault_instruction
_system_move_file_store_fault_instruction:
    .pword 0xb7b001
    return

.global _system_crc_lane_probe
_system_crc_lane_probe:
    clr w0
    mov w0, CRCCON1
    nop
    mov #0x0700, w0
    mov w0, CRCCON2
    mov #0x8000, w0
    mov w0, CRCCON1
    mov #0x3412, w1
    mov w1, CRCDATL
    nop
    mov CRCCON1, w0
    mov w0, _system_crc_lane_state

    clr w0
    mov w0, CRCCON1
    nop
    mov #0x0f00, w0
    mov w0, CRCCON2
    mov #0x8000, w0
    mov w0, CRCCON1
    mov #0x0012, w1
    mov #CRCDATL, w2
    mov.b w1, [w2]
    nop
    mov CRCCON1, w0
    mov w0, _system_crc_lane_state+2

    clr w0
    mov w0, CRCCON1
    nop
    mov #0x8000, w0
    mov w0, CRCCON1
    mov #0x0034, w1
    mov #CRCDATL+1, w2
    mov.b w1, [w2]
    nop
    mov CRCCON1, w0
    mov w0, _system_crc_lane_state+4

    clr w0
    mov w0, CRCCON1
    nop
    mov #0x0700, w0
    mov w0, CRCCON2
    mov #0x8000, w0
    mov w0, CRCCON1
    mov #0x0012, w1
    mov #CRCDATL, w2
    mov.b w1, [w2]
    nop
    mov CRCCON1, w0
    mov w0, _system_crc_lane_state+6
    clr w0
    mov w0, CRCCON1
.global _system_crc_lane_complete
_system_crc_lane_complete:
    bra _system_crc_lane_complete

.global _system_output_compare_sync_probe
_system_output_compare_sync_probe:
    clr 0x0900
    clr 0x0902
    mov #4, w0
    mov w0, 0x0904
    mov #2, w0
    mov w0, 0x0906
    mov #0x1c06, w0
    mov w0, 0x0900
    nop
    mov #0x001f, w0
    mov w0, 0x0902
.global _system_output_compare_sync_complete
_system_output_compare_sync_complete:
    bra _system_output_compare_sync_complete
_system_move_file_load_fault_execute:
    set_status 0x010d
    mov #0xa5a5, w0
.global _system_move_file_load_fault_instruction
_system_move_file_load_fault_instruction:
    .pword 0xbf9001
    return
_system_move_file_rmw_fault_execute:
    set_status 0x010d
    mov #0xa5a5, w0
.global _system_move_file_rmw_fault_instruction
_system_move_file_rmw_fault_instruction:
    .pword 0xbfb001
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

.global _system_program_boundary_dispatch
_system_program_boundary_dispatch:
    mov #0x5000, w15
    lnk #0
    mov #0x5000, w15
    clr w1
    mov #0xa5a5, w0
    mov w0, 0x5000
    mov #0x5a5a, w0
    mov w0, 0x5002
    mov #0x010f, w0
    mov w0, SR
    goto _system_sequential_hole_probe
    mov #0x1111, w1
    return

.global _system_do_boundary_dispatch
_system_do_boundary_dispatch:
    mov #0x5000, w15
    lnk #0
    mov #0x5000, w15
    clr w1
    mov #0x010f, w0
    mov w0, SR
    goto _system_skip_one_word_probe
    mov #0x1111, w1
    return

.macro prepare_skip_probe
    mov #0x5000, w15
    clr w1
    mov #0xa5a5, w0
    mov w0, 0x5000
    mov #0x5a5a, w0
    mov w0, 0x5002
    mov #0x0103, w0
    mov w0, SR
    clr w0
    clr w2
    mov w1, _system_skip_state
    mov w15, _system_skip_state+2
    mov 0x5000, w3
    mov w3, _system_skip_state+4
    mov 0x5002, w3
    mov w3, _system_skip_state+6
    mov SR, w3
    mov w3, _system_skip_state+8
    mov CORCON, w3
    mov w3, _system_skip_state+10
    mov INTCON1, w3
    mov w3, _system_skip_state+12
.endm

.global _system_skip_one_word_dispatch
_system_skip_one_word_dispatch:
    prepare_skip_probe
    goto _system_skip_one_word_probe
    mov #0x1111, w1
    return

.global _system_skip_two_word_dispatch
_system_skip_two_word_dispatch:
    prepare_skip_probe
    goto _system_skip_two_word_probe
    mov #0x1111, w1
    return

.section .system_program_boundary_capture,code,address(0x300)
.global _system_program_boundary_capture
_system_program_boundary_capture:
    mov w1, _system_program_boundary_state
    mov w15, _system_program_boundary_state+2
    mov 0x5000, w0
    mov w0, _system_program_boundary_state+4
    mov 0x5002, w0
    mov w0, _system_program_boundary_state+6
    mov SR, w0
    mov w0, _system_program_boundary_state+8
    mov CORCON, w0
    mov w0, _system_program_boundary_state+10
    mov INTCON1, w0
    mov w0, _system_program_boundary_state+12
.global _system_program_boundary_complete
_system_program_boundary_complete:
    bra _system_program_boundary_complete

.section .text,code

.section .system_skip_unexpected,code,address(0x340)
.global _system_skip_unexpected_call
_system_skip_unexpected_call:
    mov #0x1111, w1
    mov w1, _system_skip_state
.global _system_skip_unexpected_complete
_system_skip_unexpected_complete:
    bra _system_skip_unexpected_complete

.section .text,code

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

.global _system_skip_two_word_probe
_system_skip_two_word_probe = 0x557fa

.section .system_skip_boundary,code,address(0x557fc)
.global _system_skip_one_word_probe
_system_skip_one_word_probe:
    .pword 0x020340

.global _system_skip_two_word_complete
_system_skip_two_word_complete = 0x55802

.section .text,code

.global __AddressError
__AddressError:
    mov w0, _system_move_file_fault_state
    mov SR, w0
    mov w0, _system_move_file_fault_state+6
    mov _system_probe_selector, w0
    cp w0, #72
    bra z, _system_move_file_fault_handler
    cp w0, #73
    bra z, _system_move_file_fault_handler
    cp w0, #74
    bra z, _system_move_file_fault_handler
    mov _system_move_file_fault_state+6, w0
    mov w0, SR
    mov _system_move_file_fault_state, w0
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
    mov w1, _system_do_boundary_state
    mov w15, _system_do_boundary_state+2
    mov [w15-4], w0
    mov w0, _system_do_boundary_state+4
    mov [w15-2], w0
    mov w0, _system_do_boundary_state+6
    mov INTCON1, w0
    mov w0, _system_do_boundary_state+8
    mov INTTREG, w0
    mov w0, _system_do_boundary_state+10
    mov SR, w0
    mov w0, _system_do_boundary_state+12
    mov CORCON, w0
    mov w0, _system_do_boundary_state+14
    mov DCOUNT, w0
    mov w0, _system_do_boundary_state+16
    mov DOSTARTL, w0
    mov w0, _system_do_boundary_state+18
    mov DOSTARTH, w0
    mov w0, _system_do_boundary_state+20
    mov DOENDL, w0
    mov w0, _system_do_boundary_state+22
    mov DOENDH, w0
    mov w0, _system_do_boundary_state+24
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
    mov ACCAL, w0
    mov w0, _system_dsp_x_fault_state
    mov ACCAH, w0
    mov w0, _system_dsp_x_fault_state+2
    mov ACCAU, w0
    mov w0, _system_dsp_x_fault_state+4
    mov w4, _system_dsp_x_fault_state+6
    mov w5, _system_dsp_x_fault_state+8
    mov w8, _system_dsp_x_fault_state+10
    mov w10, _system_dsp_x_fault_state+12
    mov DSRPAG, w0
    mov w0, _system_dsp_x_fault_state+14
    mov INTCON1, w0
    mov w0, _system_dsp_x_fault_state+16
    mov [w15-4], w0
    mov w0, _system_dsp_x_fault_state+18
    mov [w15-2], w0
    mov w0, _system_dsp_x_fault_state+20
    mov INTTREG, w0
    mov w0, _system_dsp_x_fault_state+22
    mov SR, w0
    mov w0, _system_dsp_x_fault_state+24
    mov CORCON, w0
    mov w0, _system_dsp_x_fault_state+26
    mov w15, _system_dsp_x_fault_state+28
    mov ACCAL, w0
    mov w0, _system_dsp_x_program_fault_state
    mov ACCAH, w0
    mov w0, _system_dsp_x_program_fault_state+2
    mov ACCAU, w0
    mov w0, _system_dsp_x_program_fault_state+4
    mov w4, _system_dsp_x_program_fault_state+6
    mov w5, _system_dsp_x_program_fault_state+8
    mov w8, _system_dsp_x_program_fault_state+10
    mov w10, _system_dsp_x_program_fault_state+12
    mov DSRPAG, w0
    mov w0, _system_dsp_x_program_fault_state+14
    mov INTCON1, w0
    mov w0, _system_dsp_x_program_fault_state+16
    mov [w15-4], w0
    mov w0, _system_dsp_x_program_fault_state+18
    mov [w15-2], w0
    mov w0, _system_dsp_x_program_fault_state+20
    mov INTTREG, w0
    mov w0, _system_dsp_x_program_fault_state+22
    mov SR, w0
    mov w0, _system_dsp_x_program_fault_state+24
    mov CORCON, w0
    mov w0, _system_dsp_x_program_fault_state+26
    mov w15, _system_dsp_x_program_fault_state+28
    mov w1, _system_psv_program_fault_state
    mov w2, _system_psv_program_fault_state+2
    mov w3, _system_psv_program_fault_state+4
    mov DSRPAG, w0
    mov w0, _system_psv_program_fault_state+6
    mov INTCON1, w0
    mov w0, _system_psv_program_fault_state+8
    mov [w15-4], w0
    mov w0, _system_psv_program_fault_state+10
    mov [w15-2], w0
    mov w0, _system_psv_program_fault_state+12
    mov INTTREG, w0
    mov w0, _system_psv_program_fault_state+14
    mov SR, w0
    mov w0, _system_psv_program_fault_state+16
    mov CORCON, w0
    mov w0, _system_psv_program_fault_state+18
    mov w15, _system_psv_program_fault_state+20
    mov 0x5000, w0
    mov w0, _system_psv_program_fault_state+22
    mov _system_probe_selector, w0
    mov w0, _system_psv_program_fault_state+24
    mov DISICNT, w0
    mov w0, _system_psv_program_fault_state+26
.global _system_address_trap_complete
_system_address_trap_complete:
    bra _system_address_trap_complete

_system_move_file_fault_handler:
    mov w1, _system_move_file_fault_state+2
    mov w3, _system_move_file_fault_state+4
    mov INTCON1, w0
    mov w0, _system_move_file_fault_state+8
    mov [w15-4], w0
    mov w0, _system_move_file_fault_state+10
    mov [w15-2], w0
    mov w0, _system_move_file_fault_state+12
    mov INTTREG, w0
    mov w0, _system_move_file_fault_state+14
    mov w15, _system_move_file_fault_state+16
    mov 0x1000, w0
    mov w0, _system_move_file_fault_state+18
    mov _system_probe_selector, w0
    mov w0, _system_move_file_fault_state+20
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
    mov _system_probe_selector, w0
    cp w0, #57
    bra z, _system_repeat_math_handler
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

_system_repeat_math_handler:
    mov w15, _system_repeat_trap_state
    mov [w15-4], w0
    mov w0, _system_repeat_trap_state+2
    mov [w15-2], w0
    mov #0x107f, w1
    and w0, w1, w0
    mov w0, _system_repeat_trap_state+4
    mov RCOUNT, w0
    mov w0, _system_repeat_trap_state+6
    mov SR, w0
    and #0x00f0, w0
    mov w0, _system_repeat_trap_state+8
    mov CORCON, w0
    mov w0, _system_repeat_trap_state+10
    mov INTCON1, w0
    mov w0, _system_repeat_trap_state+12
    mov INTTREG, w0
    mov w0, _system_repeat_trap_state+14
.global _system_repeat_math_complete
_system_repeat_math_complete:
    bra _system_repeat_math_complete

.global __INT1Interrupt
__INT1Interrupt:
    mov SR, w3
    mov _system_probe_selector, w0
    cp w0, #59
    bra geu, _system_sfr_wait_interrupt
    mov w15, _system_repeat_irq_state
    mov [w15-4], w0
    mov w0, _system_repeat_irq_state+2
    mov [w15-2], w0
    mov #0x107f, w1
    and w0, w1, w0
    mov w0, _system_repeat_irq_state+4
    mov RCOUNT, w0
    mov w0, _system_repeat_irq_state+6
    mov w3, w0
    and #0x00f0, w0
    mov w0, _system_repeat_irq_state+8
    mov CORCON, w0
    mov w0, _system_repeat_irq_state+10
    clr RCOUNT
    clr w0
    mov w0, IEC1
    mov w0, IFS1
    retfie

_system_sfr_wait_interrupt:
    mov w15, _system_sfr_wait_state
    mov [w15-4], w0
    mov w0, _system_sfr_wait_state+2
    mov [w15-2], w0
    mov #0x107f, w1
    and w0, w1, w0
    mov w0, _system_sfr_wait_state+4
    mov RCOUNT, w0
    mov w0, _system_sfr_wait_state+6
    mov w3, _system_sfr_wait_state+8
    mov CORCON, w0
    mov w0, _system_sfr_wait_state+10
.global _system_sfr_wait_complete
_system_sfr_wait_complete:
    bra _system_sfr_wait_complete

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
