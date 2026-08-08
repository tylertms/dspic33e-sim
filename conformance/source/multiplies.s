.section .text,code
.include "conformance.inc"
.equ _conformance_y_scratch, 0x9000

.global _multiply_conformance_cases
_multiply_conformance_cases = 102
.global _multiply_conformance_group_complete
_multiply_conformance_group_complete = 0

.macro record_accumulator id, low, high, upper
    mov \low, w1
    mov \high, w2
    mov \upper, w3
    record_accumulator_case \id, w1, w2, w3
.endm

.macro load_accumulator low_value, high_value, upper_value, low_register, high_register, upper_register
    mov #\low_value, w0
    mov w0, \low_register
    mov #\high_value, w0
    mov w0, \high_register
    mov #\upper_value, w0
    mov w0, \upper_register
.endm

.macro prepare_dsp_case value
    clr A
    clr B
    set_status \value
.endm

.global _run_multiply_conformance
_run_multiply_conformance:
    begin_results
    push w8
    push w9
    push w10
    push w11
    push w12
    push w13

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

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x0001, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x0002, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    add A
    record_accumulator 0x0721, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0xffff, 0xffff, ACCAL, ACCAH, ACCAU
    load_accumulator 0x0002, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    add B
    record_accumulator 0x0722, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0x7fff, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x0001, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    add A
    record_accumulator 0x0723, ACCAL, ACCAH, ACCAU
    mov SR, w1
    record_case 0x0724, w1, w1

    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0xffff, 0xffff, ACCAL, ACCAH, ACCAU
    load_accumulator 0x0000, 0x8000, 0xffff, ACCBL, ACCBH, ACCBU
    add B
    record_accumulator 0x0725, ACCBL, ACCBH, ACCBU
    mov SR, w1
    record_case 0x0726, w1, w1

    prepare_dsp_case 0x0000
    load_accumulator 0x0005, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x0003, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    sub A
    record_accumulator 0x0727, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x0005, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x0003, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    sub B
    record_accumulator 0x0728, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0x7fff, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0xffff, 0xffff, 0xffff, ACCBL, ACCBH, ACCBU
    sub A
    record_accumulator 0x0729, ACCAL, ACCAH, ACCAU
    mov SR, w1
    record_case 0x072a, w1, w1

    prepare_dsp_case 0x0000
    load_accumulator 0x0005, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    neg A
    record_accumulator 0x072b, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x8000, 0xffff, ACCBL, ACCBH, ACCBU
    neg B
    record_accumulator 0x072c, ACCBL, ACCBH, ACCBU
    mov SR, w1
    record_case 0x072d, w1, w1

    prepare_dsp_case 0x0000
    load_accumulator 0x1234, 0x5678, 0x0000, ACCAL, ACCAH, ACCAU
    clr A
    record_accumulator 0x072e, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0xabcd, 0x8765, 0xffff, ACCBL, ACCBH, ACCBU
    clr B
    record_accumulator 0x072f, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x789a, 0x3456, 0x0012, ACCAL, ACCAH, ACCAU
    sftac A, #0
    record_accumulator 0x0730, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0xff00, 0x120f, 0x0000, ACCAL, ACCAH, ACCAU
    sftac A, #1
    record_accumulator 0x0731, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x0003, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    sftac B, #-1
    record_accumulator 0x0732, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0xffff, 0xffff, ACCBL, ACCBH, ACCBU
    sftac B, #16
    record_accumulator 0x0733, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    load_accumulator 0x0001, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    sftac A, #-16
    record_accumulator 0x0734, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x1234, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    mov #4, w2
    sftac A, w2
    record_accumulator 0x0735, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x0123, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    mov #0xfffc, w2
    sftac B, w2
    record_accumulator 0x0736, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0x7fff, 0x0000, ACCAL, ACCAH, ACCAU
    mov #0xffff, w2
    sftac A, w2
    record_accumulator 0x0737, ACCAL, ACCAH, ACCAU
    mov SR, w1
    record_case 0x0738, w1, w1

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x5678, 0x1234, 0x0000, ACCAL, ACCAH, ACCAU
    sac A, w2
    record_case 0x0739, w2, w2

    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x2468, 0x0000, ACCBL, ACCBH, ACCBU
    sac B, #1, w3
    record_case 0x073a, w3, w3

    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x1234, 0x0000, ACCAL, ACCAH, ACCAU
    sac A, #-1, w4
    record_case 0x073b, w4, w4

    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x7f00, 0x0000, ACCBL, ACCBH, ACCBU
    sac B, #7, w5
    record_case 0x073c, w5, w5

    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x0012, 0x0000, ACCAL, ACCAH, ACCAU
    mov #_conformance_scratch+20, w4
    sac A, #-8, [w4++]
    mov _conformance_scratch+20, w2
    record_case 0x073d, w2, w4

    mov #3, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0xff00, 0x120f, 0x0000, ACCAL, ACCAH, ACCAU
    sac.r A, w2
    record_case 0x073e, w2, w2

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x8000, 0x120f, 0x0000, ACCBL, ACCBH, ACCBU
    sac.r B, w2
    record_case 0x073f, w2, w2

    prepare_dsp_case 0x0000
    load_accumulator 0x8000, 0x120e, 0x0000, ACCBL, ACCBH, ACCBU
    sac.r B, w2
    record_case 0x0740, w2, w2

    mov #0x0021, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x8000, 0x0000, ACCAL, ACCAH, ACCAU
    sac A, w2
    record_case 0x0741, w2, w2

    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0x7fff, 0xffff, ACCBL, ACCBH, ACCBU
    sac B, w2
    record_case 0x0742, w2, w2

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #3, w4
    mov #4, w5
    mpy w4*w5, A
    record_accumulator 0x0743, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    mov #0xfffe, w4
    mov #5, w5
    mpy w4*w5, B
    record_accumulator 0x0744, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    mov #0xfffd, w4
    mpy w4*w4, A
    record_accumulator 0x0745, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    mov #3, w4
    mov #4, w5
    mpy.n w4*w5, A
    record_accumulator 0x0746, ACCAL, ACCAH, ACCAU

    clr w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0x4000, w4
    mov #0x4000, w5
    mpy w4*w5, B
    record_accumulator 0x0747, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x000a, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    mov #3, w4
    mov #4, w5
    mac w4*w5, A
    record_accumulator 0x0748, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x000a, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    mov #3, w4
    mov #4, w5
    msc w4*w5, B
    record_accumulator 0x0749, ACCBL, ACCBH, ACCBU

    mov #0x1001, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0xffff, w4
    mov #2, w5
    mpy w4*w5, A
    record_accumulator 0x074a, ACCAL, ACCAH, ACCAU

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0x7fff, 0x0000, ACCAL, ACCAH, ACCAU
    mov #1, w4
    mov #1, w5
    mac w4*w5, A
    record_accumulator 0x074b, ACCAL, ACCAH, ACCAU
    mov SR, w1
    record_case 0x074c, w1, w1

    mov #0x2001, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0xffff, w4
    mov #0xfffe, w5
    mpy w4*w5, A
    record_accumulator 0x074d, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    mov #0xffff, w4
    mpy w4*w4, A
    record_accumulator 0x074e, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    mov #0xffff, w5
    mpy w5*w5, B
    record_accumulator 0x074f, ACCBL, ACCBH, ACCBU

    clr w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0xffff, w4
    mov #0x4000, w5
    mpy w4*w5, A
    record_accumulator 0x0750, ACCAL, ACCAH, ACCAU

    mov #0x0081, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0x7fff, 0x0000, ACCAL, ACCAH, ACCAU
    mov #1, w4
    mov #1, w5
    mac w4*w5, A
    record_accumulator 0x0751, ACCAL, ACCAH, ACCAU
    mov SR, w1
    record_case 0x0752, w1, w1

    mov #0x0041, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x8000, 0xffff, ACCBL, ACCBH, ACCBU
    mov #1, w4
    mov #1, w5
    msc w4*w5, B
    record_accumulator 0x0753, ACCBL, ACCBH, ACCBU
    mov SR, w1
    record_case 0x0754, w1, w1

    mov #0x0091, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0xffff, 0x007f, ACCAL, ACCAH, ACCAU
    mov #1, w4
    mov #1, w5
    mac w4*w5, A
    record_accumulator 0x0755, ACCAL, ACCAH, ACCAU
    mov SR, w1
    record_case 0x0756, w1, w1

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0xffff, 0xffff, 0x007f, ACCAL, ACCAH, ACCAU
    mov #1, w4
    mov #1, w5
    mac w4*w5, A
    record_accumulator 0x0757, ACCAL, ACCAH, ACCAU
    mov SR, w1
    record_case 0x0758, w1, w1

    prepare_dsp_case 0x2400
    mov #1, w4
    mov #1, w5
    mpy w4*w5, A
    mov SR, w1
    record_case 0x0759, w1, w1

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0x1111, w0
    mov w0, _conformance_scratch
    mov #0x2222, w0
    mov #_conformance_y_scratch, w1
    mov w0, [w1]
    mov #_conformance_scratch, w8
    mov #_conformance_y_scratch, w10
    mov #3, w4
    mov #4, w5
    mpy w4*w5, A, [w8]+=2, w4, [w10]-=2, w5
    record_accumulator 0x0760, ACCAL, ACCAH, ACCAU
    record_case 0x0761, w4, w8
    record_case 0x0762, w5, w10

    prepare_dsp_case 0x0000
    mov #0x3333, w0
    mov w0, _conformance_scratch+4
    mov #0x4444, w0
    mov #_conformance_y_scratch+6, w1
    mov w0, [w1]
    mov #_conformance_scratch, w9
    mov #_conformance_y_scratch+2, w11
    mov #4, w12
    mov #2, w4
    mov #5, w5
    mac w4*w5, A, [w9+w12], w4, [w11+w12], w5
    record_accumulator 0x0763, ACCAL, ACCAH, ACCAU
    record_case 0x0764, w4, w9
    record_case 0x0765, w5, w11

    prepare_dsp_case 0x0000
    mov #_conformance_scratch+6, w8
    mov #2, w4
    mov #3, w5
    mpy w4*w5, B, [w8]-=6, w4
    record_accumulator 0x0766, ACCBL, ACCBH, ACCBU
    record_case 0x0767, w4, w8

    prepare_dsp_case 0x0000
    load_accumulator 0x9000, 0x1234, 0x0000, ACCBL, ACCBH, ACCBU
    mov #1, w4
    mov #1, w5
    mov #0xa5a5, w13
    mac w4*w5, A, w13
    record_case 0x0768, w13, w13
    record_accumulator 0x0769, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x7000, 0x5678, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x000a, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    mov #3, w4
    mov #2, w5
    mov #_conformance_scratch+20, w13
    msc w4*w5, B, [w13]+=2
    mov _conformance_scratch+20, w1
    record_case 0x076a, w1, w13
    record_accumulator 0x076b, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    pop w13
    pop w12
    pop w11
    pop w10
    pop w9
    pop w8
    end_results
