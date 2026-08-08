.section .text,code
.include "conformance.inc"

.global _usb_conformance_cases
_usb_conformance_cases = 39
.global _usb_conformance_group_complete
_usb_conformance_group_complete = 1

.macro usb_register_case identifier, address
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

.global _run_usb_conformance
_run_usb_conformance:
    begin_results

    usb_register_case 0x1600, 0x0488
    usb_register_case 0x1601, 0x048a
    usb_register_case 0x1602, 0x048c
    usb_register_case 0x1603, 0x048e
    usb_register_case 0x1604, 0x0490
    usb_register_case 0x1605, 0x04c0
    usb_register_case 0x1606, 0x04c2
    usb_register_case 0x1607, 0x04c4
    usb_register_case 0x1608, 0x04c6
    usb_register_case 0x1609, 0x04c8
    usb_register_case 0x160a, 0x04ca
    usb_register_case 0x160b, 0x04cc
    usb_register_case 0x160c, 0x04ce
    usb_register_case 0x160d, 0x04d0
    usb_register_case 0x160e, 0x04d2
    usb_register_case 0x160f, 0x04d4
    usb_register_case 0x1610, 0x04d6
    usb_register_case 0x1611, 0x04d8
    usb_register_case 0x1612, 0x04da
    usb_register_case 0x1613, 0x04dc
    usb_register_case 0x1614, 0x04de
    usb_register_case 0x1615, 0x04e0
    usb_register_case 0x1616, 0x04e2
    usb_register_case 0x1617, 0x04e4
    usb_register_case 0x1618, 0x04e6
    usb_register_case 0x1619, 0x04e8
    usb_register_case 0x161a, 0x04ea
    usb_register_case 0x161b, 0x04ec
    usb_register_case 0x161c, 0x04ee
    usb_register_case 0x161d, 0x04f0
    usb_register_case 0x161e, 0x04f2
    usb_register_case 0x161f, 0x04f4
    usb_register_case 0x1620, 0x04f6
    usb_register_case 0x1621, 0x04f8
    usb_register_case 0x1622, 0x04fa
    usb_register_case 0x1623, 0x04fc
    usb_register_case 0x1624, 0x04fe
    usb_register_case 0x1625, 0x0580
    usb_register_case 0x1626, 0x0582

    end_results
