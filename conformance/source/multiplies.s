.section .text,code
.include "conformance.inc"
.equ _conformance_y_scratch, 0x9000

.global _multiply_conformance_cases
_multiply_conformance_cases = 163
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

.macro record_multiply_pair id, opcode
    .pword \opcode
    mov w13, w7
    record_accumulator \id, ACCAL, ACCAH, ACCAU
    mov w7, w13
    mov #0x8003, w7
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

    mov #-300, w2
    mov #200, w1
    mov #0x5a5a, w3
    set_status 0x010f
    mulw.ss w2, w1, w2
    record_double_case 0x0797, w2, w3

    mov #-300, w2
    mov #400, w1
    mov #0x5a5a, w3
    set_status 0x010f
    mulw.su w2, w1, w2
    record_double_case 0x0798, w2, w3

    mov #-300, w2
    mov #0x5a5a, w3
    set_status 0x010f
    mulw.su w2, #31, w2
    record_double_case 0x0799, w2, w3

    mov #60000, w2
    mov #-3, w1
    mov #0x5a5a, w3
    set_status 0x010f
    mulw.us w2, w1, w2
    record_double_case 0x079a, w2, w3

    mov #50000, w2
    mov #3, w1
    mov #0x5a5a, w3
    set_status 0x010f
    mulw.uu w2, w1, w2
    record_double_case 0x079b, w2, w3

    mov #50000, w2
    mov #0x5a5a, w3
    set_status 0x010f
    mulw.uu w2, #31, w2
    record_double_case 0x079c, w2, w3

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
    mov #_conformance_y_scratch+2, w1
    mov w0, [w1]
    mov #_conformance_scratch, w8
    mov #_conformance_y_scratch+2, w10
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
    mov #0x1234, w4
    lac w4, A
    record_accumulator 0x076c, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    mov #0x1000, w4
    lac w4, #-3, B
    record_accumulator 0x076d, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    mov #0x8000, w0
    mov w0, _conformance_scratch
    mov #_conformance_scratch, w4
    lac [w4++], #7, A
    record_accumulator 0x076e, ACCAL, ACCAH, ACCAU
    record_case 0x076f, w4, w4

    prepare_dsp_case 0x0000
    mov #0xfffe, w0
    mov w0, _conformance_scratch+4
    mov #_conformance_scratch, w4
    mov #4, w5
    lac [w4+w5], B
    record_accumulator 0x0770, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x0001, 0x0000, ACCAL, ACCAH, ACCAU
    mov #2, w4
    add w4, A
    record_accumulator 0x0771, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    mov #0x0003, w4
    add w4, #-2, B
    record_accumulator 0x0772, ACCBL, ACCBH, ACCBU

    prepare_dsp_case 0x0000
    mov #0x0008, w0
    mov w0, _conformance_scratch
    mov #_conformance_scratch, w4
    add [w4++], #2, A
    record_accumulator 0x0773, ACCAL, ACCAH, ACCAU
    record_case 0x0774, w4, w4

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x000a, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    mov #3, w4
    mac w4*w4, A
    record_accumulator 0x0775, ACCAL, ACCAH, ACCAU

    clr w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x0000, 0x0001, 0x0000, ACCBL, ACCBH, ACCBU
    mov #0x4000, w5
    mac w5*w5, B
    record_accumulator 0x0776, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0x5555, w0
    mov w0, _conformance_scratch
    mov #_conformance_scratch, w8
    mov #3, w4
    mov #4, w5
    mpy.n w4*w5, A, [w8]+=4, w4
    record_accumulator 0x0777, ACCAL, ACCAH, ACCAU
    record_case 0x0778, w4, w8

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x2222, 0x1111, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x8000, 0x1235, 0x0000, ACCBL, ACCBH, ACCBU
    mov #0x4444, w0
    mov w0, _conformance_scratch+8
    mov #_conformance_scratch+8, w8
    mov #0xa5a5, w13
    set_status 0xfc00
    clr A, [w8]+=6, w4, w13
    record_accumulator 0x0779, ACCAL, ACCAH, ACCAU
    record_case 0x077a, w4, w8
    record_case 0x077b, w13, w13

    prepare_dsp_case 0x0000
    load_accumulator 0x8000, 0x2345, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0xaaaa, 0xbbbb, 0x00cc, ACCBL, ACCBH, ACCBU
    mov #0x1357, w0
    mov w0, _conformance_scratch+12
    mov #0x2468, w0
    mov #_conformance_y_scratch+10, w1
    mov w0, [w1]
    mov #_conformance_scratch+12, w9
    mov #_conformance_y_scratch+6, w11
    mov #4, w12
    mov #_conformance_scratch+24, w13
    set_status 0xfc00
    clr B, [w9]-=4, w5, [w11+w12], w4, [w13]+=2
    record_accumulator 0x077c, ACCBL, ACCBH, ACCBU
    record_case 0x077d, w5, w9
    record_case 0x077e, w4, w11
    mov _conformance_scratch+24, w1
    record_case 0x077f, w1, w13
    mov SR, w1
    record_case 0x0780, w1, w1

    prepare_dsp_case 0x0000
    load_accumulator 0x8001, 0x3456, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x1234, 0x5678, 0x0000, ACCBL, ACCBH, ACCBU
    mov #0x7811, w0
    mov w0, _conformance_scratch+16
    mov #0xb2af, w0
    mov #_conformance_y_scratch+16, w1
    mov w0, [w1]
    mov #_conformance_scratch+16, w9
    mov #_conformance_y_scratch+16, w11
    mov #0xa5a5, w13
    push w6
    push w7
    movsac B, [w9], w6, [w11]+=4, w7, w13
    mov w6, w1
    mov w9, w2
    mov w7, w3
    mov w11, w4
    pop w7
    pop w6
    record_case 0x0781, w1, w2
    record_case 0x0782, w3, w4
    record_case 0x0783, w13, w13
    record_accumulator 0x0784, ACCAL, ACCAH, ACCAU

    prepare_dsp_case 0x0000
    load_accumulator 0x1111, 0x2222, 0x0000, ACCAL, ACCAH, ACCAU
    load_accumulator 0x8000, 0x4567, 0x0000, ACCBL, ACCBH, ACCBU
    mov #0xbb00, w0
    mov w0, _conformance_scratch+18
    mov #0x52ce, w0
    mov #_conformance_y_scratch+20, w1
    mov w0, [w1]
    mov #_conformance_scratch+18, w9
    mov #_conformance_y_scratch+16, w11
    mov #4, w12
    mov #_conformance_scratch+26, w13
    movsac A, [w9]-=2, w4, [w11+w12], w5, [w13]+=2
    record_accumulator 0x0785, ACCBL, ACCBH, ACCBU
    record_case 0x0786, w4, w9
    record_case 0x0787, w5, w11
    mov _conformance_scratch+26, w1
    record_case 0x0788, w1, w13

    prepare_dsp_case 0x0000
    load_accumulator 0xaaaa, 0xbbbb, 0x00cc, ACCAL, ACCAH, ACCAU
    load_accumulator 0x1111, 0x2222, 0x0033, ACCBL, ACCBH, ACCBU
    movsac A
    record_accumulator 0x0789, ACCAL, ACCAH, ACCAU
    record_accumulator 0x078a, ACCBL, ACCBH, ACCBU

    mov #1, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0x007f, w0
    mov w0, _conformance_scratch
    mov #0x0028, w0
    mov #_conformance_y_scratch+2, w1
    mov w0, [w1]
    mov #_conformance_scratch, w8
    mov #_conformance_y_scratch+2, w10
    mov #0x009a, w4
    ed w4*w4, A, [w8]+=2, [w10]-=2, w4
    record_accumulator 0x078b, ACCAL, ACCAH, ACCAU
    record_case 0x078c, w4, w8
    record_case 0x078d, w10, w10

    clr w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    mov #0x6a7c, w0
    mov w0, _conformance_scratch+4
    mov #0x2b3d, w0
    mov #_conformance_y_scratch+8, w1
    mov w0, [w1]
    mov #_conformance_scratch, w9
    mov #_conformance_y_scratch, w11
    mov #4, w12
    mov #0xfffe, w5
    ed w5*w5, B, [w9]+=4, [w11+w12], w5
    record_accumulator 0x078e, ACCBL, ACCBH, ACCBU
    record_case 0x078f, w5, w9
    record_case 0x0790, w11, w12

    mov #0x2001, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x0007, 0x0000, 0x0000, ACCAL, ACCAH, ACCAU
    mov #0x7000, w0
    mov w0, _conformance_scratch+6
    mov #0x1000, w0
    mov #_conformance_y_scratch+6, w1
    mov w0, [w1]
    mov #_conformance_scratch+6, w8
    mov #_conformance_y_scratch+6, w10
    push w6
    push w7
    mov #0xffff, w6
    edac w6*w6, A, [w8]-=6, [w10]+=6, w6
    mov w6, w1
    pop w7
    pop w6
    record_accumulator 0x0791, ACCAL, ACCAH, ACCAU
    record_case 0x0792, w1, w8
    record_case 0x0793, w10, w10

    mov #0x2001, w0
    mov w0, CORCON
    prepare_dsp_case 0x0000
    load_accumulator 0x0009, 0x0000, 0x0000, ACCBL, ACCBH, ACCBU
    mov #0x1000, w0
    mov w0, _conformance_scratch+2
    mov #0x7000, w0
    mov #_conformance_y_scratch+2, w1
    mov w0, [w1]
    mov #_conformance_scratch+2, w9
    mov #_conformance_y_scratch+2, w11
    push w6
    push w7
    mov #0xfffe, w7
    edac w7*w7, B, [w9]+=2, [w11]-=2, w7
    mov w7, w1
    pop w7
    pop w6
    record_accumulator 0x0794, ACCBL, ACCBH, ACCBU
    record_case 0x0795, w1, w9
    record_case 0x0796, w11, w11

    mov MODCON, w0
    push w0
    mov XMODSRT, w0
    push w0
    mov XMODEND, w0
    push w0
    mov YMODSRT, w0
    push w0
    mov YMODEND, w0
    push w0
    mov #_conformance_scratch, w0
    mov w0, XMODSRT
    mov #_conformance_scratch+7, w0
    mov w0, XMODEND
    mov #_conformance_y_scratch, w0
    mov w0, YMODSRT
    mov #_conformance_y_scratch+7, w0
    mov w0, YMODEND
    mov #0xcfa8, w0
    mov w0, MODCON
    nop
    mov #_conformance_scratch+6, w8
    mov #_conformance_y_scratch, w10
    mov #3, w4
    mov #4, w5
    mpy w4*w5, A, [w8]+=2, w4, [w10]-=2, w5
    record_accumulator_case 0x079d, w8, w8, w8
    record_accumulator_case 0x079e, w10, w10, w10

    mov w6, w12
    mov w7, w13
    mov #0x2001, w0
    mov w0, CORCON
    mov #0xfffe, w4
    mov #0xfffd, w5
    mov #0x8002, w6
    mov #0x8003, w7
    record_multiply_pair 0x079f, 0xc00113
    record_multiply_pair 0x07a0, 0xc10113
    record_multiply_pair 0x07a1, 0xc20113
    record_multiply_pair 0x07a2, 0xc40113
    record_multiply_pair 0x07a3, 0xc50113
    record_multiply_pair 0x07a4, 0xc60113
    record_multiply_pair 0x07a5, 0xf00111
    record_multiply_pair 0x07a6, 0xf10111
    record_multiply_pair 0x07a7, 0xf20111
    record_multiply_pair 0x07a8, 0xf30111
    mov w12, w6
    mov w13, w7
    pop w0
    mov w0, YMODEND
    pop w0
    mov w0, YMODSRT
    pop w0
    mov w0, XMODEND
    pop w0
    mov w0, XMODSRT
    pop w0
    mov w0, MODCON

    prepare_dsp_case 0x0000
    pop w13
    pop w12
    pop w11
    pop w10
    pop w9
    pop w8
    end_results
