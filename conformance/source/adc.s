.section .text,code
.include "conformance.inc"

.global _adc_conformance_cases
_adc_conformance_cases = 17
.global _adc_conformance_group_complete
_adc_conformance_group_complete = 1

.macro adc_register_case identifier, address
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

.macro adc_control_case identifier, address, first, second
    mov \address, w4
    mov #\first, w0
    mov w0, \address
    nop
    mov \address, w1
    mov #\second, w0
    mov w0, \address
    nop
    mov \address, w2
    clr w0
    mov w0, \address
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w4, [w7++]
.endm

.macro adc_manual_case identifier, control, timing, channels, buffer, flags
    clr w0
    mov w0, \control
    mov w0, \timing
    mov w0, \channels
    mov #0x8002, w0
    mov w0, \control
    mov #0x8000, w0
    mov w0, \control
    repeat #63
    nop
    mov \control, w1
    mov \buffer, w2
    mov \flags, w3
    clr w0
    mov w0, \control
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
.endm

.global _run_adc_conformance
_run_adc_conformance:
    begin_results

    adc_register_case 0x1200, 0x0322
    adc_register_case 0x1201, 0x0324
    adc_register_case 0x1202, 0x0326
    adc_register_case 0x1203, 0x0328
    adc_register_case 0x1204, 0x032e
    adc_register_case 0x1205, 0x0330
    adc_register_case 0x1206, 0x0332
    adc_register_case 0x1207, 0x0362
    adc_register_case 0x1208, 0x0364
    adc_register_case 0x1209, 0x0366
    adc_register_case 0x120a, 0x0368
    adc_register_case 0x120b, 0x0370
    adc_register_case 0x120c, 0x0372
    adc_control_case 0x120d, 0x0320, 0x33f8, 0x0400
    adc_control_case 0x120e, 0x0360, 0x33f8, 0x4c01
    adc_manual_case 0x120f, 0x0320, 0x0324, 0x0328, 0x0300, 0x0800
    adc_manual_case 0x1210, 0x0360, 0x0364, 0x0368, 0x0340, 0x0802

    end_results
