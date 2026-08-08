.section .text,code
.include "conformance.inc"

.global _stack_conformance_cases
_stack_conformance_cases = 23
.global _stack_conformance_group_complete
_stack_conformance_group_complete = 1

.global _run_stack_conformance
_run_stack_conformance:
    begin_results

    mov w15, w5
    mov #0xa55a, w2
    set_status 0x0100
    push w2
    clr w2
    pop w2
    sub w15, w5, w3
    record_double_case 0x0a00, w2, w3

    mov w15, w5
    push w15
    pop w2
    sub w15, w5, w3
    record_double_case 0x0a01, w2, w3

    mov w15, w5
    mov #0x2340, w2
    push w2
    pop w15
    mov w15, w2
    mov w5, w15
    record_case 0x0a02, w2, w15

    mov #0x5aa5, w2
    mov w2, _conformance_scratch
    push _conformance_scratch
    clr w2
    mov w2, _conformance_scratch
    pop _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x0a03, w2, w2

    mov #0x1357, w2
    mov w2, _conformance_scratch
    mov #_conformance_scratch, w4
    push [w4++]
    pop w2
    record_case 0x0a04, w2, w4

    mov #0x2468, w2
    mov w2, _conformance_scratch+2
    mov #_conformance_scratch, w4
    mov #2, w5
    push [w4+w5]
    pop w2
    record_case 0x0a05, w2, w4

    mov #0xabcd, w2
    push w2
    mov #_conformance_scratch-2, w4
    pop [++w4]
    mov _conformance_scratch, w2
    record_case 0x0a06, w2, w4

    mov w15, w5
    mov #0x1122, w2
    mov #0x3344, w3
    push.d w2
    clr w2
    clr w3
    pop.d w2
    record_double_case 0x0a07, w2, w3
    sub w15, w5, w2
    record_case 0x0a08, w2, w15

    mov w15, w5
    mov w14, w4
    mov #0x4567, w14
    push.d w14
    pop.d w2
    sub w3, w5, w3
    mov w4, w14
    record_double_case 0x0a09, w2, w3

    mov w15, w5
    mov w14, w4
    mov #0x4567, w2
    mov #0x2340, w3
    set_status 0x0100
    push.d w2
    pop.d w14
    mov w14, w2
    mov w15, w3
    mov w5, w15
    mov w4, w14
    record_double_case 0x0a0a, w2, w3

    mov #0x1000, w0
    mov #0x2000, w1
    mov #0x3000, w2
    mov #0x4000, w3
    set_status 0x010f
    push.s
    clr w0
    clr w1
    clr w2
    clr w3
    set_status 0x00e0
    pop.s
    mov w0, w4
    record_double_case 0x0a0b, w4, w1
    record_double_case 0x0a0c, w2, w3
    mov SR, w4
    record_case 0x0a0d, w4, w4

    mov #0x1111, w0
    mov #0x2222, w1
    set_status 0x0101
    push.s
    mov #0x3333, w0
    mov #0x4444, w1
    set_status 0x0102
    push.s
    clr w0
    clr w1
    set_status 0x0000
    pop.s
    mov w0, w4
    record_double_case 0x0a0e, w4, w1
    clr w0
    clr w1
    set_status 0x0000
    pop.s
    mov w0, w4
    record_double_case 0x0a0f, w4, w1

    mov w15, w5
    mov w14, w4
    lnk #0
    mov w14, w2
    mov w15, w3
    ulnk
    record_double_case 0x0a10, w2, w3
    mov [w5], w2
    sub w15, w5, w3
    record_double_case 0x0a11, w2, w3

    mov w14, w4
    lnk #0
    mov CORCON, w2
    ulnk
    mov CORCON, w3
    record_double_case 0x0a12, w2, w3

    mov w15, w5
    mov w14, w4
    lnk #6
    sub w15, w5, w2
    sub w14, w5, w3
    ulnk
    record_double_case 0x0a13, w2, w3

    mov w15, w5
    mov w14, w4
    lnk #0x3ffe
    sub w15, w5, w2
    sub w14, w5, w3
    ulnk
    record_double_case 0x0a14, w2, w3

    mov w14, w4
    lnk #0
    rcall stack_capture_sfa
    mov CORCON, w3
    ulnk
    record_double_case 0x0a15, w2, w3

    mov #0x0024, w2
    mov w2, CORCON
    mov CORCON, w2
    record_case 0x0a16, w2, w2

    end_results

stack_capture_sfa:
    mov CORCON, w2
    return
