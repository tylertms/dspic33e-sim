.section .text,code
.include "conformance.inc"

.global _multiply_conformance_cases
_multiply_conformance_cases = 33
.global _multiply_conformance_group_complete
_multiply_conformance_group_complete = 0

.macro record_accumulator id, low, high, upper
    mov \low, w1
    mov \high, w2
    mov \upper, w3
    record_accumulator_case \id, w1, w2, w3
.endm

.global _run_multiply_conformance
_run_multiply_conformance:
    begin_results

    mov #0x8000, w2
    mov #0xffff, w3
    set_status 0x010f
    mul.ss w2, w3, w4
    record_double_case 0x0700, w4, w5

    mov #0x8000, w2
    mov #1, w3
    set_status 0x010f
    mul.ss w2, w3, w4
    record_double_case 0x0701, w4, w5

    mov #0x7fff, w2
    mov #0x7fff, w3
    set_status 0x010f
    mul.ss w2, w3, w4
    record_double_case 0x0702, w4, w5

    mov #0x8000, w2
    mov #0xffff, w3
    set_status 0x010f
    mul.su w2, w3, w4
    record_double_case 0x0703, w4, w5

    mov #0xffff, w2
    mov #0x8000, w3
    set_status 0x010f
    mul.us w2, w3, w4
    record_double_case 0x0704, w4, w5

    mov #0xffff, w2
    mov #0xffff, w3
    set_status 0x010f
    mul.uu w2, w3, w4
    record_double_case 0x0705, w4, w5

    mov #0x8000, w2
    set_status 0x010f
    mul.su w2, #0, w4
    record_double_case 0x0706, w4, w5

    mov #0x8000, w2
    set_status 0x010f
    mul.su w2, #31, w4
    record_double_case 0x0707, w4, w5

    mov #0xffff, w2
    set_status 0x010f
    mul.uu w2, #0, w4
    record_double_case 0x0708, w4, w5

    mov #0xffff, w2
    set_status 0x010f
    mul.uu w2, #31, w4
    record_double_case 0x0709, w4, w5

    mov #_conformance_scratch, w4
    mov #4, w1
    mov w1, [w4]
    mov #3, w1
    set_status 0x010f
    mul.ss w1, [w4], w2
    record_double_case 0x070a, w2, w3

    mov #_conformance_scratch, w4
    mov #4, w1
    mov w1, [w4]
    mov #3, w1
    set_status 0x010f
    mul.ss w1, [w4++], w2
    record_case 0x070b, w2, w4

    mov #_conformance_scratch+2, w4
    mov #5, w1
    mov w1, [w4]
    mov #0xfffe, w1
    set_status 0x010f
    mul.su w1, [w4--], w2
    record_case 0x070c, w2, w4

    mov #_conformance_scratch, w4
    mov #0xfffd, w1
    mov w1, [w4+2]
    mov #6, w1
    set_status 0x010f
    mul.us w1, [++w4], w2
    record_case 0x070d, w2, w4

    mov #_conformance_scratch+2, w4
    mov #7, w1
    mov w1, [w4-2]
    mov #6, w1
    set_status 0x010f
    mul.uu w1, [--w4], w2
    record_case 0x070e, w2, w4

    mov #2, w1
    mov w1, _conformance_scratch
    mov #0xffff, w0
    set_status 0x010f
    mul _conformance_scratch
    record_double_case 0x070f, w2, w3

    mov #0x0200, w1
    mov w1, _conformance_scratch
    mov #0xa5ff, w0
    mov #0x5a5a, w3
    set_status 0x010f
    mul.b _conformance_scratch+1
    record_case 0x0710, w2, w3

    mov #1, w0
    mov w0, CORCON
    mov #0x8000, w2
    mov #1, w3
    set_status 0x010f
    mul.ss w2, w3, A
    record_accumulator 0x0711, ACCAL, ACCAH, ACCAU

    mov #1, w0
    mov w0, CORCON
    mov #0x7fff, w2
    mov #2, w3
    set_status 0x010f
    mul.ss w2, w3, B
    record_accumulator 0x0712, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    mov #0x8000, w2
    mov #0xffff, w3
    set_status 0x010f
    mul.su w2, w3, A
    record_accumulator 0x0713, ACCAL, ACCAH, ACCAU

    mov #1, w0
    mov w0, CORCON
    mov #0x7fff, w2
    mov #0xffff, w3
    set_status 0x010f
    mul.su w2, w3, B
    record_accumulator 0x0714, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    mov #0xffff, w2
    mov #0x8000, w3
    set_status 0x010f
    mul.us w2, w3, A
    record_accumulator 0x0715, ACCAL, ACCAH, ACCAU

    mov #1, w0
    mov w0, CORCON
    mov #0xffff, w2
    mov #2, w3
    set_status 0x010f
    mul.us w2, w3, B
    record_accumulator 0x0716, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    mov #0xffff, w2
    mov #0xffff, w3
    set_status 0x010f
    mul.uu w2, w3, A
    record_accumulator 0x0717, ACCAL, ACCAH, ACCAU

    mov #1, w0
    mov w0, CORCON
    mov #0x8000, w2
    mov #0x8000, w3
    set_status 0x010f
    mul.uu w2, w3, B
    record_accumulator 0x0718, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    mov #0x8000, w2
    set_status 0x010f
    mul.su w2, #31, A
    record_accumulator 0x0719, ACCAL, ACCAH, ACCAU

    mov #1, w0
    mov w0, CORCON
    mov #0xffff, w2
    set_status 0x010f
    mul.uu w2, #31, B
    record_accumulator 0x071a, ACCBL, ACCBH, ACCBU

    clr w0
    mov w0, CORCON
    mov #0x4000, w2
    mov #0x4000, w3
    set_status 0x010f
    mul.ss w2, w3, A
    record_accumulator 0x071b, ACCAL, ACCAH, ACCAU

    clr w0
    mov w0, CORCON
    mov #0xc000, w2
    mov #0x4000, w3
    set_status 0x010f
    mul.su w2, w3, B
    record_accumulator 0x071c, ACCBL, ACCBH, ACCBU

    clr w0
    mov w0, CORCON
    mov #0x4000, w2
    mov #0xc000, w3
    set_status 0x010f
    mul.us w2, w3, A
    record_accumulator 0x071d, ACCAL, ACCAH, ACCAU

    clr w0
    mov w0, CORCON
    mov #0xffff, w2
    mov #0xffff, w3
    set_status 0x010f
    mul.uu w2, w3, B
    record_accumulator 0x071e, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    mov #_conformance_scratch, w4
    mov #4, w1
    mov w1, [w4]
    mov #3, w2
    set_status 0x010f
    mul.ss w2, [w4++], A
    mov ACCAL, w1
    record_case 0x071f, w1, w4

    mov #1, w0
    mov w0, CORCON
    mov #0xffff, w2
    set_status 0x010f
    mul.uu w2, #0, A
    record_accumulator 0x0720, ACCAL, ACCAH, ACCAU

    end_results
