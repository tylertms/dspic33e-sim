.section .text,code
.include "conformance.inc"

.global _pwm_conformance_cases
_pwm_conformance_cases = 30
.global _pwm_conformance_group_complete
_pwm_conformance_group_complete = 1

.macro pwm_register_case identifier, address
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

.global _run_pwm_conformance
_run_pwm_conformance:
    begin_results

    mov 0x0c00, w4
    mov #0x7fff, w0
    mov w0, 0x0c00
    nop
    mov 0x0c00, w1
    mov #0x2aa5, w0
    mov w0, 0x0c00
    nop
    mov 0x0c00, w2
    mov w4, 0x0c00
    mov #0x1300, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w4, [w7++]

    pwm_register_case 0x1301, 0x0c02
    pwm_register_case 0x1302, 0x0c04
    pwm_register_case 0x1303, 0x0c06
    pwm_register_case 0x1304, 0x0c0a
    pwm_register_case 0x1305, 0x0c0e
    pwm_register_case 0x1306, 0x0c10
    pwm_register_case 0x1307, 0x0c12
    pwm_register_case 0x1308, 0x0c14
    pwm_register_case 0x1309, 0x0c1a
    pwm_register_case 0x130a, 0x0c20
    pwm_register_case 0x130b, 0x0c22
    pwm_register_case 0x130c, 0x0c24
    pwm_register_case 0x130d, 0x0c26
    pwm_register_case 0x130e, 0x0c28
    pwm_register_case 0x130f, 0x0c2a
    pwm_register_case 0x1310, 0x0c2c
    pwm_register_case 0x1311, 0x0c2e
    pwm_register_case 0x1312, 0x0c30
    pwm_register_case 0x1313, 0x0c32
    pwm_register_case 0x1314, 0x0c34
    pwm_register_case 0x1315, 0x0c38
    pwm_register_case 0x1316, 0x0c3a
    pwm_register_case 0x1317, 0x0c3c
    pwm_register_case 0x1318, 0x0c3e
    pwm_register_case 0x1319, 0x0c40
    pwm_register_case 0x131a, 0x0c60
    pwm_register_case 0x131b, 0x0c80
    pwm_register_case 0x131c, 0x0ca0
    pwm_register_case 0x131d, 0x0cc0

    end_results
