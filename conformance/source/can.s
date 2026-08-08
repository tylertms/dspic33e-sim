.section .text,code
.include "conformance.inc"

.global _can_conformance_cases
_can_conformance_cases = 38
.global _can_conformance_group_complete
_can_conformance_group_complete = 1

.macro can_register_case identifier, address
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

.macro can_control_case identifier, address
    mov \address, w4
    mov #0x2c09, w0
    mov w0, \address
    nop
    mov \address, w1
    mov #0x0400, w0
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

.macro can_window_case identifier, control, address
    mov \control, w5
    bset \control, #0
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
    mov w5, \control
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w4, [w7++]
.endm

.macro can_module_cases identifier, base
    can_control_case \identifier, (\base + 0x00)
    can_register_case (\identifier + 1), (\base + 0x02)
    can_register_case (\identifier + 2), (\base + 0x04)
    can_register_case (\identifier + 3), (\base + 0x06)
    can_register_case (\identifier + 4), (\base + 0x08)
    can_register_case (\identifier + 5), (\base + 0x0a)
    can_register_case (\identifier + 6), (\base + 0x0c)
    can_register_case (\identifier + 7), (\base + 0x0e)
    can_register_case (\identifier + 8), (\base + 0x10)
    can_register_case (\identifier + 9), (\base + 0x12)
    can_register_case (\identifier + 10), (\base + 0x14)
    can_register_case (\identifier + 11), (\base + 0x18)
    can_register_case (\identifier + 12), (\base + 0x1a)
    can_window_case (\identifier + 13), \base, (\base + 0x30)
    can_window_case (\identifier + 14), \base, (\base + 0x32)
    can_window_case (\identifier + 15), \base, (\base + 0x38)
    can_window_case (\identifier + 16), \base, (\base + 0x3a)
    can_window_case (\identifier + 17), \base, (\base + 0x40)
    can_window_case (\identifier + 18), \base, (\base + 0x42)
.endm

.global _run_can_conformance
_run_can_conformance:
    begin_results

    can_module_cases 0x1500, 0x0400
    can_module_cases 0x1513, 0x0500

    end_results
