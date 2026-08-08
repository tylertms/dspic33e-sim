.section .text,code
.include "conformance.inc"

.global _divide_conformance_cases
_divide_conformance_cases = 46
.global _divide_conformance_group_complete
_divide_conformance_group_complete = 1

.macro record_divide id
    mov w0, w2
    mov w1, w3
    record_double_case \id, w2, w3
.endm

.macro record_divide_overflow id
    mov #0, w2
    mov #0, w3
    record_double_case \id, w2, w3
.endm

.global _run_divide_conformance
_run_divide_conformance:
    begin_results

    mov #12000, w2
    mov #40, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0800

    mov #12003, w2
    mov #40, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0801

    mov #-12003, w2
    mov #40, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0802

    mov #12003, w2
    mov #-40, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0803

    mov #-12003, w2
    mov #-40, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0804

    mov #0x8000, w2
    mov #0xffff, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide_overflow 0x0805

    mov #0xffff, w2
    mov #255, w3
    set_status 0x010f
    repeat #17
    div.u w2, w3
    record_divide 0x0806

    mov #0xffff, w2
    mov #256, w3
    set_status 0x010f
    repeat #17
    div.u w2, w3
    record_divide 0x0807

    clr w2
    mov #2, w3
    mov #256, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide 0x0808

    clr w2
    mov #0xffff, w3
    mov #3, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide 0x0809

    mov #0xffff, w2
    clr w3
    mov #-2, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide 0x080a

    clr w2
    mov #1, w3
    mov #1, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide_overflow 0x080b

    mov #0x3456, w2
    mov #0x0012, w3
    mov #0x1234, w4
    set_status 0x010f
    repeat #17
    div.ud w2, w4
    record_divide 0x080c

    clr w2
    mov #1, w3
    mov #1, w4
    set_status 0x010f
    repeat #17
    div.ud w2, w4
    record_divide_overflow 0x080d

    mov #0x1000, w2
    mov #0x4000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x080e

    mov #0xf000, w2
    mov #0x4000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x080f

    mov #0x1000, w2
    mov #0xc000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x0810

    mov #0xf000, w2
    mov #0xc000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x0811

    mov #0x1001, w2
    mov #0x4000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x0812

    mov #0x4000, w2
    mov #0x4000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide_overflow 0x0813

    clr w2
    mov #0x8000, w3
    mov #-1, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide_overflow 0x0814

    mov #0xffff, w2
    mov #0xffff, w3
    mov #1, w4
    set_status 0x010f
    repeat #17
    div.ud w2, w4
    record_divide_overflow 0x0815

    clr w2
    mov #0x0123, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0816

    mov #0x7fff, w2
    mov #1, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0817

    mov #0x8000, w2
    mov #1, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0818

    mov #0x7fff, w2
    mov #0xffff, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x0819

    mov #1, w2
    mov #0x8000, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x081a

    mov #0xffff, w2
    mov #0x7fff, w3
    set_status 0x010f
    repeat #17
    div.s w2, w3
    record_divide 0x081b

    set_status 0x010f
    mov #-12003, w0
    mov #40, w2
    repeat #17
    div.s w0, w2
    record_divide 0x081c

    clr w2
    mov #0xffff, w3
    set_status 0x010f
    repeat #17
    div.u w2, w3
    record_divide 0x081d

    mov #0xffff, w2
    mov #1, w3
    set_status 0x010f
    repeat #17
    div.u w2, w3
    record_divide 0x081e

    mov #0xffff, w2
    mov #0xffff, w3
    set_status 0x010f
    repeat #17
    div.u w2, w3
    record_divide 0x081f

    mov #1, w2
    mov #0xffff, w3
    set_status 0x010f
    repeat #17
    div.u w2, w3
    record_divide 0x0820

    mov #0xfffe, w1
    mov #257, w2
    set_status 0x010f
    repeat #17
    div.u w1, w2
    record_divide 0x0821

    clr w2
    clr w3
    mov #0x0123, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide 0x0822

    mov #0x7edd, w2
    mov #0x0091, w3
    mov #0x0123, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide 0x0823

    mov #0x8000, w2
    mov #0xff6e, w3
    mov #0x0123, w4
    set_status 0x010f
    repeat #17
    div.sd w2, w4
    record_divide 0x0824

    set_status 0x010f
    mov #0x4567, w0
    mov #0x0123, w1
    mov #0x1234, w2
    repeat #17
    div.sd w0, w2
    record_divide 0x0825

    mov #0x7edd, w2
    mov #0x0091, w3
    mov #0x0123, w4
    set_status 0x010f
    repeat #17
    div.ud w2, w4
    record_divide 0x0826

    clr w2
    clr w3
    mov #0xffff, w4
    set_status 0x010f
    repeat #17
    div.ud w2, w4
    record_divide 0x0827

    set_status 0x010f
    mov #0x4567, w0
    mov #0x0123, w1
    mov #0x1234, w2
    repeat #17
    div.ud w0, w2
    record_divide 0x0828

    clr w2
    mov #0x4000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x0829

    mov #0x3fff, w2
    mov #0x4000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x082a

    mov #0xf001, w2
    mov #0x4000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x082b

    mov #0x1001, w2
    mov #0xc000, w3
    set_status 0x010f
    repeat #17
    divf w2, w3
    record_divide 0x082c

    set_status 0x010f
    mov #0x8002, w0
    mov #0x8001, w1
    repeat #17
    divf w0, w1
    record_divide 0x082d

    end_results
