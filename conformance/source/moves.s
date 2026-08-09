.section .text,code
.include "conformance.inc"

.global _move_conformance_cases
_move_conformance_cases = 45
.global _move_conformance_group_complete
_move_conformance_group_complete = 0

.global _run_move_conformance
_run_move_conformance:
    begin_results
    push w8
    push w9
    push w10

    set_status 0x0105
    mov #0xa55a, w2
    record_case 0x0500, w2, w2

    set_status 0x0105
    mov #0x5a34, w2
    mov.b #0xa5, w2
    record_case 0x0501, w2, w2

    set_status 0x0105
    mov #0xa55a, w1
    mov w1, w2
    record_case 0x0502, w2, w1

    set_status 0x0105
    mov #0xa55a, w1
    mov #0x1234, w2
    mov.b w1, w2
    record_case 0x0503, w2, w1

    mov #_conformance_scratch, w4
    mov #0x1101, w1
    mov w1, [w4]
    mov [w4], w2
    record_case 0x0504, w2, w4

    mov #_conformance_scratch, w4
    mov #0x1102, w1
    mov w1, [w4]
    mov [w4++], w2
    record_case 0x0505, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x1103, w1
    mov w1, [w4]
    mov [w4--], w2
    record_case 0x0506, w2, w4

    mov #_conformance_scratch, w4
    mov #0x1104, w1
    mov w1, [w4+2]
    mov [++w4], w2
    record_case 0x0507, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x1105, w1
    mov w1, [w4-2]
    mov [--w4], w2
    record_case 0x0508, w2, w4

    mov #_conformance_scratch, w4
    mov #4, w5
    mov #0x1106, w1
    mov w1, [w4+4]
    mov [w4+w5], w2
    record_case 0x0509, w2, w4

    mov #_conformance_scratch, w4
    mov #0x2201, w1
    mov w1, [w4]
    mov [w4], w2
    record_case 0x050a, w2, w4

    mov #_conformance_scratch, w4
    mov #0x2202, w1
    mov w1, [w4++]
    mov [w4-2], w2
    record_case 0x050b, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x2203, w1
    mov w1, [w4--]
    mov [w4+2], w2
    record_case 0x050c, w2, w4

    mov #_conformance_scratch, w4
    mov #0x2204, w1
    mov w1, [++w4]
    mov [w4], w2
    record_case 0x050d, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x2205, w1
    mov w1, [--w4]
    mov [w4], w2
    record_case 0x050e, w2, w4

    mov #_conformance_scratch, w4
    mov #4, w5
    mov #0x2206, w1
    mov w1, [w4+w5]
    mov [w4+4], w2
    record_case 0x050f, w2, w4

    mov #_conformance_scratch, w4
    mov #0xa581, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov.b [w4++], w2
    record_case 0x0510, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0xa582, w1
    mov.b w1, [w4]
    mov #0x5a34, w2
    mov.b [--w4], w2
    record_case 0x0511, w2, w4

    mov #_conformance_scratch, w4
    mov #1, w5
    mov #0xa583, w1
    mov.b w1, [w4+w5]
    mov #0x5a34, w2
    mov.b [w4+w5], w2
    record_case 0x0512, w2, w4

    mov #_conformance_scratch, w4
    mov #0xa591, w1
    mov.b w1, [w4++]
    mov #0x5a34, w2
    mov.b [w4-1], w2
    record_case 0x0513, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0xa592, w1
    mov.b w1, [--w4]
    mov #0x5a34, w2
    mov.b [w4], w2
    record_case 0x0514, w2, w4

    mov #_conformance_scratch, w4
    mov #1, w5
    mov #0xa593, w1
    mov.b w1, [w4+w5]
    mov #0x5a34, w2
    mov.b [w4+1], w2
    record_case 0x0515, w2, w4

    mov #_conformance_scratch, w4
    mov #0x3301, w1
    mov w1, [w4+6]
    mov [w4+6], w2
    record_case 0x0516, w2, w4

    mov #_conformance_scratch, w4
    mov #0x3302, w1
    mov w1, [w4+8]
    mov [w4+8], w2
    record_case 0x0517, w2, w4

    mov #_conformance_scratch, w4
    mov #0xa5a3, w1
    mov.b w1, [w4+3]
    mov #0x5a34, w2
    mov.b [w4+3], w2
    record_case 0x0518, w2, w4

    mov #_conformance_scratch, w4
    mov #0xa5a4, w1
    mov.b w1, [w4+5]
    mov #0x5a34, w2
    mov.b [w4+5], w2
    record_case 0x0519, w2, w4

    mov #0x4401, w2
    mov #0x4402, w3
    mov.d w2, w4
    record_double_case 0x051a, w4, w5

    mov #_conformance_scratch, w4
    mov #0x4403, w1
    mov w1, [w4]
    mov #0x4404, w1
    mov w1, [w4+2]
    mov.d [w4], w2
    record_double_case 0x051b, w2, w3

    mov #_conformance_scratch, w4
    mov #0x4405, w2
    mov #0x4406, w3
    mov.d w2, [w4]
    mov [w4], w2
    mov [w4+2], w3
    record_double_case 0x051c, w2, w3

    mov #0x5501, w1
    mov w1, _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x051d, w2, w1

    mov #0x5502, w1
    mov w1, _conformance_scratch+2
    mov _conformance_scratch+2, w2
    record_case 0x051e, w2, w1

    mov #0xa5b1, w0
    mov.b WREG, _conformance_scratch+4
    mov #0x5a34, w0
    mov.b _conformance_scratch+4, WREG
    mov w0, w2
    record_case 0x051f, w2, w2

    mov #0xa5b2, w0
    mov.b WREG, _conformance_scratch+5
    mov #0x5a34, w0
    mov.b _conformance_scratch+5, WREG
    mov w0, w2
    record_case 0x0520, w2, w2

    mov MODCON, w8
    mov XMODSRT, w9
    mov XMODEND, w10
    mov #_conformance_scratch, w0
    mov w0, XMODSRT
    mov #_conformance_scratch+7, w0
    mov w0, XMODEND
    mov #0x8ff4, w0
    mov w0, MODCON
    nop

    mov #0x6601, w1
    mov w1, _conformance_scratch+6
    mov #_conformance_scratch+6, w4
    mov [w4++], w2
    record_case 0x0521, w2, w4

    mov #0x6602, w1
    mov w1, _conformance_scratch
    mov #_conformance_scratch+6, w4
    mov [++w4], w2
    record_case 0x0522, w2, w4

    mov #0x6603, w1
    mov w1, _conformance_scratch
    mov #_conformance_scratch, w4
    mov [w4--], w2
    record_case 0x0523, w2, w4

    mov #0x6604, w1
    mov w1, _conformance_scratch+6
    mov #_conformance_scratch, w4
    mov [--w4], w2
    record_case 0x0524, w2, w4

    mov #0x66a5, w0
    mov.b WREG, _conformance_scratch+7
    mov #_conformance_scratch+7, w4
    clr w2
    mov.b [w4++], w2
    record_case 0x0525, w2, w4

    mov #_conformance_scratch+6, w5
    mov [w5++], w2
    record_case 0x0526, w2, w5

    mov #0x6607, w1
    mov w1, _conformance_scratch+2
    mov #_conformance_scratch+6, w4
    mov #4, w5
    mov [w4+w5], w2
    record_case 0x0527, w2, w4

    mov #0x0ff4, w0
    mov w0, MODCON
    nop
    mov #_conformance_scratch+6, w4
    mov [w4++], w2
    record_case 0x0528, w2, w4

    mov #0x8ff4, w0
    mov w0, MODCON
    nop
    mov #0x6609, w1
    mov #_conformance_scratch+6, w4
    mov w1, [w4++]
    mov _conformance_scratch+6, w2
    record_case 0x0529, w2, w4

    mov #0x660a, w1
    mov #_conformance_scratch+6, w4
    mov w1, [++w4]
    mov _conformance_scratch, w2
    record_case 0x052a, w2, w4

    mov #0x660b, w1
    mov w1, _conformance_scratch+2
    mov #_conformance_scratch+6, w4
    mov [w4+4], w2
    record_case 0x052b, w2, w4

    mov #0x660c, w1
    mov #_conformance_scratch+6, w4
    mov w1, [w4+4]
    mov _conformance_scratch+2, w2
    record_case 0x052c, w2, w4

    mov w8, MODCON
    mov w9, XMODSRT
    mov w10, XMODEND

    pop w10
    pop w9
    pop w8
    end_results
