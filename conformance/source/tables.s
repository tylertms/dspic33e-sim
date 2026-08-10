.section .text,code
.include "conformance.inc"

.global _table_conformance_cases
_table_conformance_cases = 32
.global _table_conformance_group_complete
_table_conformance_group_complete = 1

.macro select_table_data
    mov #tblpage(table_words), w0
    and #0x00ff, w0
    mov w0, TBLPAG
    mov #tbloffset(table_words), w2
.endm

.macro select_write_latches
    mov #0x00fa, w0
    mov w0, TBLPAG
.endm

.global _run_table_conformance
_run_table_conformance:
    begin_results

    set_status 0x0000
    mov #0x01ff, w0
    mov w0, TBLPAG
    mov TBLPAG, w2
    record_case 0x0b00, w2, w2

    select_table_data
    set_status 0x0100
    tblrdl [w2], w3
    record_case 0x0b01, w3, w2

    select_table_data
    tblrdh [w2], w3
    record_case 0x0b02, w3, w2

    select_table_data
    mov #0xa500, w3
    tblrdl.b [w2], w3
    record_case 0x0b03, w3, w2

    select_table_data
    inc w2, w2
    mov #0xa500, w3
    tblrdl.b [w2], w3
    record_case 0x0b04, w3, w2

    select_table_data
    mov #0xa500, w3
    tblrdh.b [w2], w3
    record_case 0x0b05, w3, w2

    select_table_data
    inc w2, w2
    mov #0xa5ff, w3
    tblrdh.b [w2], w3
    record_case 0x0b06, w3, w2

    select_table_data
    tblrdl [w2++], w3
    record_case 0x0b07, w3, w2

    select_table_data
    add #2, w2
    tblrdl [w2--], w3
    record_case 0x0b08, w3, w2

    select_table_data
    tblrdl [++w2], w3
    record_case 0x0b09, w3, w2

    select_table_data
    add #2, w2
    tblrdl [--w2], w3
    record_case 0x0b0a, w3, w2

    select_table_data
    mov #_conformance_scratch, w4
    tblrdl [w2], [w4++]
    mov _conformance_scratch, w3
    record_case 0x0b0b, w3, w4

    select_table_data
    mov #_conformance_scratch+2, w4
    tblrdl [w2], [w4--]
    mov _conformance_scratch+2, w3
    record_case 0x0b0c, w3, w4

    select_table_data
    mov #_conformance_scratch, w4
    tblrdl [w2], [++w4]
    mov _conformance_scratch+2, w3
    record_case 0x0b0d, w3, w4

    select_table_data
    mov #_conformance_scratch+2, w4
    tblrdl [w2], [--w4]
    mov _conformance_scratch, w3
    record_case 0x0b0e, w3, w4

    mov #0xa5a5, w3
    mov w3, _conformance_scratch
    select_table_data
    mov #_conformance_scratch, w4
    tblrdl.b [w2], [w4++]
    mov _conformance_scratch, w3
    record_case 0x0b0f, w3, w4

    mov #1, w0
    mov w0, TBLPAG
    mov #0xfffe, w2
    tblrdl [w2++], w3
    record_case 0x0b10, w3, w2

    tblrdl [w2], w3
    record_case 0x0b11, w3, w2

    select_write_latches
    clr w3
    mov #0xa55a, w2
    tblwtl w2, [w3]
    record_case 0x0b12, w2, w3

    mov #0xabcd, w2
    tblwth w2, [w3]
    record_case 0x0b13, w2, w3

    mov #0x00c3, w2
    tblwtl.b w2, [w3]
    record_case 0x0b14, w2, w3

    inc w3, w3
    mov #0x00d4, w2
    tblwtl.b w2, [w3]
    record_case 0x0b15, w2, w3

    dec w3, w3
    mov #0x00ef, w2
    tblwth.b w2, [w3]
    record_case 0x0b16, w2, w3

    inc w3, w3
    mov #0x0077, w2
    tblwth.b w2, [w3]
    record_case 0x0b17, w2, w3

    mov #0x1234, w2
    mov w2, _conformance_scratch
    mov #_conformance_scratch, w2
    mov #2, w3
    tblwtl [w2++], [w3++]
    record_double_case 0x0b18, w2, w3

    mov #0x5678, w2
    mov w2, _conformance_scratch+2
    mov #_conformance_scratch+4, w2
    mov #6, w3
    tblwtl [--w2], [--w3]
    record_double_case 0x0b19, w2, w3

    mov #0x00a6, w2
    mov w2, _conformance_scratch
    mov #_conformance_scratch, w2
    mov #4, w3
    tblwtl.b [w2++], [w3++]
    record_double_case 0x0b1a, w2, w3

    mov #0x9abc, w2
    mov #0x00fe, w3
    tblwtl w2, [w3]
    record_case 0x0b1b, w2, w3

    mov #0x55aa, w2
    mov #6, w3
    tblwth w2, [w3]
    record_case 0x0b1c, w2, w3

    inc w3, w3
    mov #0x0033, w2
    tblwth.b w2, [w3]
    record_case 0x0b1d, w2, w3

    select_write_latches
    clr w3
    mov #0x00c3, w2
    tblwtl w2, [w3]
    mov #0x00ef, w2
    tblwth w2, [w3]
    mov #2, w3
    mov #0x5678, w2
    tblwtl w2, [w3]
    mov #0x005e, w2
    tblwth w2, [w3]

    mov #1, w0
    mov w0, NVMADRU
    mov #0x5000, w0
    mov w0, NVMADR
    mov #0x4001, w0
    mov w0, NVMCON
    set_status 0x00e0
    mov #0x0055, w0
    mov w0, NVMKEY
    mov #0x00aa, w0
    mov w0, NVMKEY
    bset NVMCON, #15
    nop
    nop
1:
    btss NVMCON, #15
    bra 2f
    bra 1b
2:
    mov #1, w0
    mov w0, TBLPAG
    mov #0x5000, w2
    tblrdl [w2], w3
    tblrdh [w2], w4
    record_double_case 0x0b1e, w3, w4

    add #2, w2
    tblrdl [w2], w3
    tblrdh [w2], w4
    record_double_case 0x0b1f, w3, w4

    end_results

.section .table_page_start,code,address(0x10000)
    .pword 0x112233

.section .table_page_window_start,code,address(0x18000)
    .pword 0x778899

.section .table_nvm_target,code,address(0x15000)
    .pword 0xffffff
    .pword 0xffffff

.section .table_words,code,address(0x13000)
table_words:
    .pword 0x123456
    .pword 0xabcdef
    .pword 0x00ff00
    .pword 0x800001

.section .table_page_end,code,address(0x1fffe)
    .pword 0x445566
