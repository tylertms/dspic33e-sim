.section .text,code
.include "conformance.inc"

.global _timer_conformance_cases
_timer_conformance_cases = 25
.global _timer_conformance_group_complete
_timer_conformance_group_complete = 1

.macro timer_register_case identifier, address
    mov \address, w4
    mov #0xffff, w0
    mov w0, \address
    nop
    mov \address, w1
    mov #0x5aa5, w0
    mov w0, \address
    nop
    mov \address, w2
    mov w4, \address
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w4, [w7++]
.endm

.macro timer_running_case identifier, control, counter, period
    clr w0
    mov w0, \control
    mov w0, \counter
    mov #0xffff, w0
    mov w0, \period
    mov #0x8000, w0
    mov w0, \control
    nop
    nop
    mov \counter, w1
    clr w0
    mov w0, \control
    mov \counter, w2
    mov w0, \counter
    mov \period, w3
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
.endm

.macro timer_pair_case identifier, control, holding, low, high
    clr w0
    mov w0, \control
    mov #0x0008, w0
    mov w0, \control
    mov #0x1234, w0
    mov w0, \holding
    mov #0xabcd, w0
    mov w0, \low
    mov \low, w1
    mov \holding, w2
    mov \high, w3
    clr w0
    mov w0, \control
    mov w0, \low
    mov w0, \high
    mov w0, \holding
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
.endm

.global _run_timer_conformance
_run_timer_conformance:
    begin_results

    timer_register_case 0x1100, 0x0104
    timer_register_case 0x1101, 0x0110
    timer_register_case 0x1102, 0x0112
    timer_register_case 0x1103, 0x011e
    timer_register_case 0x1104, 0x0120
    timer_register_case 0x1105, 0x012c
    timer_register_case 0x1106, 0x012e
    timer_register_case 0x1107, 0x013a
    timer_register_case 0x1108, 0x013c

    timer_register_case 0x1109, 0x0102
    timer_register_case 0x110a, 0x010c
    timer_register_case 0x110b, 0x010e
    timer_register_case 0x110c, 0x011a
    timer_register_case 0x110d, 0x011c
    timer_register_case 0x110e, 0x0128
    timer_register_case 0x110f, 0x012a
    timer_register_case 0x1110, 0x0136
    timer_register_case 0x1111, 0x0138

    timer_pair_case 0x1112, 0x0110, 0x0108, 0x0106, 0x010a
    timer_pair_case 0x1113, 0x011e, 0x0116, 0x0114, 0x0118
    timer_pair_case 0x1114, 0x012c, 0x0124, 0x0122, 0x0126
    timer_pair_case 0x1115, 0x013a, 0x0132, 0x0130, 0x0134

    timer_running_case 0x1116, 0x0104, 0x0100, 0x0102
    timer_running_case 0x1117, 0x0110, 0x0106, 0x010c
    timer_running_case 0x1118, 0x0112, 0x010a, 0x010e

    end_results
