.section .text,code
.include "conformance.inc"

.global _uart_conformance_cases
_uart_conformance_cases = 16
.global _uart_conformance_group_complete
_uart_conformance_group_complete = 1

.macro uart_register_case identifier, address
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

.global _run_uart_conformance
_run_uart_conformance:
    begin_results

    uart_register_case 0x1700, 0x0220
    uart_register_case 0x1701, 0x0222
    uart_register_case 0x1703, 0x0226
    uart_register_case 0x1704, 0x0228
    uart_register_case 0x1705, 0x0230
    uart_register_case 0x1706, 0x0232
    uart_register_case 0x1708, 0x0236
    uart_register_case 0x1709, 0x0238
    uart_register_case 0x170a, 0x0250
    uart_register_case 0x170b, 0x0252
    uart_register_case 0x170d, 0x0256
    uart_register_case 0x170e, 0x0258
    uart_register_case 0x170f, 0x02b0
    uart_register_case 0x1710, 0x02b2
    uart_register_case 0x1712, 0x02b6
    uart_register_case 0x1713, 0x02b8

    end_results
