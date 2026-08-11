.section .text,code
.include "conformance.inc"

.global _interrupt_conformance_cases
_interrupt_conformance_cases = 39
.global _interrupt_conformance_group_complete
_interrupt_conformance_group_complete = 1

.macro interrupt_handler name, flag, identifier, nested
.global __\name
__\name:
    push w0
.if \identifier == 0
    mov _interrupt_mode, w0
    cp w0, #2
    bra nz, 8f
    mov TMR2, w0
    mov w0, _interrupt_entry_timer
8:
.endif
    push w1
    push w2
    bclr IFS0, #\flag
    mov _interrupt_count, w1
    sl w1, #1, w2
    mov #_interrupt_order, w0
    add w0, w2, w2
    mov #\identifier, w0
    mov w0, [w2]
    mov #_interrupt_entry_stack, w0
    add w0, w1, w0
    add w0, w1, w0
    mov w15, [w0]
    mov #_interrupt_handler_status, w0
    add w0, w1, w0
    add w0, w1, w0
    mov SR, w2
    and #0x00f0, w2
    mov w2, [w0]
    mov #_interrupt_handler_marker, w0
    add w0, w1, w0
    add w0, w1, w0
    mov w5, [w0]
    mov #_interrupt_handler_control, w0
    add w0, w1, w0
    add w0, w1, w0
    mov INTTREG, w2
    mov w2, [w0]
    inc _interrupt_count
.if \nested
    mov _interrupt_mode, w0
    cp w0, #1
    bra nz, 1f
    bset IFS0, #4
1:
.endif
    pop w2
    pop w1
    pop w0
    retfie
.endm

interrupt_handler INT0Interrupt, 0, 0, 0
interrupt_handler T1Interrupt, 3, 3, 1
interrupt_handler DMA0Interrupt, 4, 4, 0

_reset_interrupt_probe:
    bclr INTCON2, #15
    disi #0
    clr IFS0
    clr IEC0
    clr IPC0
    clr IPC1
    bclr INTCON1, #15
    bclr RCON, #2
    bclr RCON, #3
    clr _interrupt_count
    clr _interrupt_mode
    clr _interrupt_entry_timer
    clr _interrupt_order
    clr _interrupt_order+2
    clr _interrupt_order+4
    clr _interrupt_order+6
    clr _interrupt_entry_stack
    clr _interrupt_entry_stack+2
    clr _interrupt_entry_stack+4
    clr _interrupt_entry_stack+6
    clr _interrupt_handler_status
    clr _interrupt_handler_status+2
    clr _interrupt_handler_status+4
    clr _interrupt_handler_status+6
    clr _interrupt_handler_marker
    clr _interrupt_handler_marker+2
    clr _interrupt_handler_marker+4
    clr _interrupt_handler_marker+6
    clr _interrupt_handler_control
    clr _interrupt_handler_control+2
    clr _interrupt_handler_control+4
    clr _interrupt_handler_control+6
    set_status 0x0000
    return

.macro priority_case identifier, priority
    rcall _reset_interrupt_probe
    mov #\priority, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_handler_status, w1
    mov _interrupt_count, w2
    record_case \identifier, w1, w2
.endm

.global _run_interrupt_conformance
_run_interrupt_conformance:
    begin_results

    priority_case 0x0d00, 1
    priority_case 0x0d01, 2
    priority_case 0x0d02, 3
    priority_case 0x0d03, 4
    priority_case 0x0d04, 5
    priority_case 0x0d05, 6
    priority_case 0x0d06, 7

    rcall _reset_interrupt_probe
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    nop
    mov _interrupt_count, w1
    record_case 0x0d07, w1, w1
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_count, w1
    mov _interrupt_order, w2
    record_case 0x0d08, w1, w2

    rcall _reset_interrupt_probe
    bset IEC0, #0
    bset IFS0, #0
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_count, w1
    record_case 0x0d09, w1, w1

    rcall _reset_interrupt_probe
    mov #3, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    set_status 0x0060
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_count, w1
    record_case 0x0d0a, w1, w1

    rcall _reset_interrupt_probe
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    set_status 0x0060
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_handler_status, w1
    mov _interrupt_count, w2
    record_case 0x0d0b, w1, w2

    rcall _reset_interrupt_probe
    mov #6, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    mov #1, w5
    disi #3
    bset INTCON2, #15
    inc w5, w5
    inc w5, w5
    inc w5, w5
    bclr INTCON2, #15
    mov _interrupt_handler_marker, w1
    mov _interrupt_count, w2
    record_case 0x0d0c, w1, w2

    rcall _reset_interrupt_probe
    mov #7, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    mov #1, w5
    disi #3
    bset INTCON2, #15
    inc w5, w5
    bclr INTCON2, #15
    mov _interrupt_handler_marker, w1
    mov _interrupt_count, w2
    record_case 0x0d0d, w1, w2

    rcall _reset_interrupt_probe
    mov #0x4004, w0
    mov w0, IPC0
    mov #4, w0
    mov w0, IPC1
    mov #0x19, w0
    mov w0, IEC0
    mov w0, IFS0
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_order, w1
    record_case 0x0d0e, w1, w1
    mov _interrupt_order+2, w1
    record_case 0x0d0f, w1, w1
    mov _interrupt_order+4, w1
    record_case 0x0d10, w1, w1

    rcall _reset_interrupt_probe
    mov #0x5001, w0
    mov w0, IPC0
    mov #6, w0
    mov w0, IPC1
    mov #0x19, w0
    mov w0, IEC0
    mov w0, IFS0
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_order, w1
    record_case 0x0d11, w1, w1
    mov _interrupt_order+2, w1
    record_case 0x0d12, w1, w1
    mov _interrupt_order+4, w1
    record_case 0x0d13, w1, w1

    rcall _reset_interrupt_probe
    mov #0x3000, w0
    mov w0, IPC0
    mov #6, w0
    mov w0, IPC1
    mov #0x18, w0
    mov w0, IEC0
    mov #1, w0
    mov w0, _interrupt_mode
    bset IFS0, #3
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_order, w1
    mov _interrupt_order+2, w2
    record_double_case 0x0d14, w1, w2
    mov _interrupt_entry_stack, w1
    mov _interrupt_entry_stack+2, w2
    record_double_case 0x0d15, w1, w2
    mov _interrupt_handler_status, w1
    mov _interrupt_handler_status+2, w2
    record_double_case 0x0d16, w1, w2

    rcall _reset_interrupt_probe
    bset INTCON1, #15
    mov #0x3000, w0
    mov w0, IPC0
    mov #6, w0
    mov w0, IPC1
    mov #0x18, w0
    mov w0, IEC0
    mov #1, w0
    mov w0, _interrupt_mode
    bset IFS0, #3
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_order, w1
    mov _interrupt_order+2, w2
    record_double_case 0x0d17, w1, w2
    mov _interrupt_entry_stack, w1
    mov _interrupt_entry_stack+2, w2
    record_double_case 0x0d18, w1, w2
    mov _interrupt_handler_status, w1
    mov _interrupt_handler_status+2, w2
    record_double_case 0x0d19, w1, w2

    rcall _reset_interrupt_probe
    mov #5, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    bset INTCON2, #15
    nop
    bclr INTCON2, #15
    mov _interrupt_handler_control, w1
    record_case 0x0d1a, w1, w1
    mov #0xffff, w0
    mov w0, INTTREG
    mov INTTREG, w1
    record_case 0x0d1b, w1, w1

    rcall _reset_interrupt_probe
    disi #3
    mov INTCON2, w1
    nop
    nop
    mov INTCON2, w2
    record_double_case 0x0d1c, w1, w2

    rcall _reset_interrupt_probe
    bset INTCON1, #15
    set_status 0x0060
    mov SR, w1
    record_case 0x0d1d, w1, w1

    rcall _reset_interrupt_probe
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    bset INTCON2, #15
    bset IFS0, #0
    pwrsav #0
    bclr INTCON2, #15
    mov _interrupt_count, w1
    mov _interrupt_order, w2
    record_double_case 0x0d1e, w1, w2

    rcall _reset_interrupt_probe
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    set_status 0x0080
    bset INTCON2, #15
    bset IFS0, #0
    pwrsav #0
    bclr INTCON2, #15
    mov _interrupt_count, w1
    mov SR, w2
    record_double_case 0x0d1f, w1, w2

    rcall _reset_interrupt_probe
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    bset IFS0, #0
    pwrsav #0
    mov _interrupt_count, w1
    record_case 0x0d20, w1, w1

    rcall _reset_interrupt_probe
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    bset INTCON2, #15
    bset IFS0, #0
    pwrsav #1
    bclr INTCON2, #15
    mov _interrupt_count, w1
    mov _interrupt_order, w2
    record_double_case 0x0d21, w1, w2

    rcall _reset_interrupt_probe
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    set_status 0x0080
    bset INTCON2, #15
    bset IFS0, #0
    pwrsav #1
    bclr INTCON2, #15
    mov _interrupt_count, w1
    mov SR, w2
    record_double_case 0x0d22, w1, w2

    rcall _reset_interrupt_probe
    mov INTCON2, w1
    mov INTTREG, w2
    record_double_case 0x0d23, w1, w2

    rcall _reset_interrupt_probe
    mov #6, w0
    mov w0, IPC0
    bset IEC0, #0
    bset INTCON2, #15
    mov #1, w5
    disi #2
    bset IFS0, #0
    bclr INTCON2, #15
    inc w5, w5
    mov _interrupt_handler_marker, w1
    mov _interrupt_count, w2
    record_double_case 0x0d24, w1, w2

    rcall _reset_interrupt_probe
    clr T2CON
    clr TMR2
    mov #0xffff, w0
    mov w0, PR2
    mov #2, w0
    mov w0, _interrupt_mode
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    bset INTCON2, #15
    bset T2CON, #15
    bset IFS0, #0
    nop
    bclr T2CON, #15
    bclr INTCON2, #15
    mov _interrupt_entry_timer, w1
    record_case 0x0d25, w1, w1

    rcall _reset_interrupt_probe
    bset CORCON, #15
    clr T2CON
    clr TMR2
    mov #0xffff, w0
    mov w0, PR2
    mov #2, w0
    mov w0, _interrupt_mode
    mov #4, w0
    mov w0, IPC0
    bset IEC0, #0
    bset INTCON2, #15
    bset T2CON, #15
    bset IFS0, #0
    nop
    bclr T2CON, #15
    bclr INTCON2, #15
    bclr CORCON, #15
    mov _interrupt_entry_timer, w1
    record_case 0x0d26, w1, w1

    rcall _reset_interrupt_probe
    end_results
