.section .text,code
.include "conformance.inc"

.global _dma_conformance_cases
_dma_conformance_cases = 25
.global _dma_conformance_group_complete
_dma_conformance_group_complete = 1

.macro dma_register_case identifier, address, first, second
    mov \address, w4
    mov #\first, w0
    mov w0, \address
    nop
    mov \address, w1
    mov #\second, w0
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

.macro dma_read_only_reset_case identifier, address
    dma_register_case \identifier, \address, 0x0000, 0x0000
.endm

.global _run_dma_conformance
_run_dma_conformance:
    begin_results

    dma_register_case 0x1000, 0x0b00, 0x7fff, 0x5aa5
    dma_register_case 0x1001, 0x0b10, 0x7fff, 0x5aa5
    dma_register_case 0x1002, 0x0b20, 0x7fff, 0x5aa5
    dma_register_case 0x1003, 0x0b30, 0x7fff, 0x5aa5
    dma_register_case 0x1004, 0x0b40, 0x7fff, 0x5aa5
    dma_register_case 0x1005, 0x0b50, 0x7fff, 0x5aa5
    dma_register_case 0x1006, 0x0b60, 0x7fff, 0x5aa5
    dma_register_case 0x1007, 0x0b70, 0x7fff, 0x5aa5
    dma_register_case 0x1008, 0x0b80, 0x7fff, 0x5aa5
    dma_register_case 0x1009, 0x0b90, 0x7fff, 0x5aa5
    dma_register_case 0x100a, 0x0ba0, 0x7fff, 0x5aa5
    dma_register_case 0x100b, 0x0bb0, 0x7fff, 0x5aa5
    dma_register_case 0x100c, 0x0bc0, 0x7fff, 0x5aa5
    dma_register_case 0x100d, 0x0bd0, 0x7fff, 0x5aa5
    dma_register_case 0x100e, 0x0be0, 0x7fff, 0x5aa5
    dma_register_case 0x100f, 0x0b02, 0x7fff, 0x5aa5
    dma_register_case 0x1010, 0x0b06, 0xffff, 0x5aa5
    dma_register_case 0x1011, 0x0b0a, 0xffff, 0x5aa5
    dma_register_case 0x1012, 0x0b0e, 0xffff, 0x5aa5
    dma_read_only_reset_case 0x1013, 0x0bf0
    dma_read_only_reset_case 0x1014, 0x0bf2
    dma_read_only_reset_case 0x1015, 0x0bf4
    dma_register_case 0x1016, 0x0bf6, 0xffff, 0x5aa5
    dma_register_case 0x1017, 0x0bf8, 0xffff, 0x5aa5
    dma_register_case 0x1018, 0x0bfa, 0xffff, 0x5aa5

    end_results
