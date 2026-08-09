.section .text,code
.include "conformance.inc"

.global _system_conformance_cases
_system_conformance_cases = 12
.global _system_conformance_terminal_count
_system_conformance_terminal_count = 8
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
