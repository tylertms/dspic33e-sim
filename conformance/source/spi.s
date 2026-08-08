.section .text,code
.include "conformance.inc"

.global _spi_conformance_cases
_spi_conformance_cases = 24
.global _spi_conformance_group_complete
_spi_conformance_group_complete = 1

.macro spi_register_case identifier, address
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

.macro spi_sample_case identifier, address, value
    mov \address, w4
    mov #\value, w0
    mov w0, \address
    nop
    mov \address, w1
    mov w4, \address
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w4, [w7++]
    clr w0
    mov w0, [w7++]
.endm

.macro spi_module_cases identifier, status, control1, control2
    spi_register_case \identifier, \status
    spi_register_case (\identifier + 1), \control1
    spi_register_case (\identifier + 2), \control2
    spi_sample_case (\identifier + 3), \control1, 0x0200
    spi_sample_case (\identifier + 4), \control1, 0x0220
.endm

.macro spi_transfer_case identifier, status, control1, control2, buffer, flags, flag, clear
    clr w0
    mov w0, \status
    mov w0, \control2
    mov \flags, w1
    mov #\clear, w0
    and w1, w0, w1
    mov w1, \flags
    mov #0x043b, w0
    mov w0, \control1
    mov #0x8000, w0
    mov w0, \status
    mov #0xa55a, w0
    mov w0, \buffer
    repeat #63
    nop
    mov \status, w1
    mov \flags, w2
    mov #\flag, w0
    and w2, w0, w2
    mov \buffer, w0
    mov \status, w3
    clr w0
    mov w0, \status
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
.endm

.global _run_spi_conformance
_run_spi_conformance:
    begin_results

    spi_module_cases 0x1400, 0x0240, 0x0242, 0x0244
    spi_module_cases 0x1405, 0x0260, 0x0262, 0x0264
    spi_module_cases 0x140a, 0x02a0, 0x02a2, 0x02a4
    spi_module_cases 0x140f, 0x02c0, 0x02c2, 0x02c4
    spi_transfer_case 0x1414, 0x0240, 0x0242, 0x0244, 0x0248, 0x0800, 0x0400, 0xfbff
    spi_transfer_case 0x1415, 0x0260, 0x0262, 0x0264, 0x0268, 0x0804, 0x0002, 0xfffd
    spi_transfer_case 0x1416, 0x02a0, 0x02a2, 0x02a4, 0x02a8, 0x080a, 0x0800, 0xf7ff
    spi_transfer_case 0x1417, 0x02c0, 0x02c2, 0x02c4, 0x02c8, 0x080e, 0x0800, 0xf7ff

    end_results
