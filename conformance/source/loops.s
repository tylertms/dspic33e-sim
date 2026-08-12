.section .text,code
.include "conformance.inc"

.global _loop_conformance_cases
_loop_conformance_cases = 34
.global _loop_conformance_group_complete
_loop_conformance_group_complete = 1

.macro record_repeat_literal id, count
    clr w2
    set_status 0x0100
    repeat #\count
    inc w2, w2
    mov RCOUNT, w3
    record_case \id, w2, w3
.endm

.macro record_repeat_register id, count
    clr w2
    mov #\count, w3
    set_status 0x0100
    repeat w3
    inc w2, w2
    mov RCOUNT, w4
    record_case \id, w2, w4
.endm

.macro record_do_literal id, count
    clr w2
    set_status 0x0100
    do #\count, 1f
    inc w2, w2
    nop
1:
    nop
    mov DCOUNT, w3
    record_case \id, w2, w3
.endm

.macro record_do_register id, count
    clr w2
    mov #\count, w3
    set_status 0x0100
    do w3, 1f
    inc w2, w2
    nop
1:
    nop
    mov DCOUNT, w4
    record_case \id, w2, w4
.endm

.global _run_loop_conformance
_run_loop_conformance:
    begin_results

    record_repeat_literal 0x0900, 0
    record_repeat_literal 0x0901, 1
    record_repeat_literal 0x0902, 7
    record_repeat_literal 0x0903, 0x7fff

    record_repeat_register 0x0904, 0
    record_repeat_register 0x0905, 1
    record_repeat_register 0x0906, 0x4000
    record_repeat_register 0x0907, 0xffff

    set_status 0x0100
    repeat #0
    mov SR, w2
    record_case 0x0908, w2, w2

    set_status 0x0100
    repeat #1
    mov SR, w2
    record_case 0x0909, w2, w2

    record_do_literal 0x090a, 0
    record_do_literal 0x090b, 1
    record_do_literal 0x090c, 7
    record_do_literal 0x090d, 0x7fff

    record_do_register 0x090e, 0
    record_do_register 0x090f, 1
    record_do_register 0x0910, 0x4000
    record_do_register 0x0911, 0xffff

    set_status 0x0100
    do #0, 1f
loop_dostart_body:
    mov DOSTARTL, w2
    mov DOSTARTH, w3
1:
    nop
    mov #tbloffset(loop_dostart_body), w4
    sub w2, w4, w2
    mov #tblpage(loop_dostart_body), w4
    and #0x007f, w4
    sub w3, w4, w3
    record_double_case 0x0912, w2, w3

    set_status 0x0100
    do #0, loop_doend_body
    nop
    mov DOENDL, w2
    mov DOENDH, w3
loop_doend_body:
    nop
    mov #tbloffset(loop_doend_body), w4
    sub w2, w4, w2
    mov #tblpage(loop_doend_body), w4
    and #0x007f, w4
    sub w3, w4, w3
    record_double_case 0x0913, w2, w3

    clr w2
    set_status 0x0100
    do #1, 2f
    do #2, 1f
    inc w2, w2
    nop
1:
    nop
    nop
2:
    nop
    mov CORCON, w3
    record_case 0x0914, w2, w3

    clr w2
    set_status 0x0100
    do #1, 3f
    do #1, 2f
    do #1, 1f
    inc w2, w2
    nop
1:
    nop
    nop
2:
    nop
    nop
3:
    nop
    mov CORCON, w3
    record_case 0x0915, w2, w3

    clr w2
    set_status 0x0100
    do #1, 1f
    repeat #2
    inc w2, w2
    nop
1:
    nop
    record_case 0x0916, w2, w2

    clr w2
    mov #1, w3
    set_status 0x0100
    btss w3, #0
    do #2, 1f
    inc w2, w2
    nop
1:
    nop
    record_case 0x0917, w2, w2

    set_status 0x0100
    do #0, 1f
    mov SR, w2
    nop
1:
    nop
    record_case 0x0918, w2, w2

    clr w2
    set_status 0x0100
    do #3, 1f
    mov DCOUNT, w3
    add w2, w3, w2
    nop
1:
    nop
    mov DCOUNT, w3
    record_case 0x0919, w2, w3

    set_status 0x0100
    do #0, 1f
    mov CORCON, w2
    nop
1:
    nop
    record_case 0x091a, w2, w2

    set_status 0x0100
    do #1, 2f
    do #1, 1f
    mov CORCON, w2
    nop
1:
    nop
    nop
2:
    nop
    record_case 0x091b, w2, w2

    set_status 0x0100
    do #1, 2f
    do #1, 1f
    nop
    nop
1:
    nop
    mov CORCON, w2
    nop
2:
    nop
    record_case 0x091c, w2, w2

    set_status 0x0100
    do #1, 2f
    mov DOSTARTL, w2
    mov DOENDL, w3
    do #1, 1f
    nop
    nop
1:
    nop
    mov DOSTARTL, w4
    sub w2, w4, w2
    mov DOENDL, w4
    sub w3, w4, w3
    nop
2:
    nop
    record_double_case 0x091d, w2, w3

    clr w2
    set_status 0x0100
    do #7, 1f
    inc w2, w2
    mov #0x0800, w3
    mov w3, CORCON
    nop
1:
    nop
    mov CORCON, w3
    record_case 0x091e, w2, w3

    mov #0x0800, w2
    mov w2, CORCON
    mov CORCON, w2
    record_case 0x091f, w2, w2

    end_results

.section .loop_high,code,address(0x12000)

.global _run_loop_high_conformance
_run_loop_high_conformance:
    begin_results

    set_status 0x0100
    do #1, 1f
loop_high_dostart_body:
    mov DOSTARTL, w2
    mov DOSTARTH, w3
1:
    nop
    mov #tbloffset(loop_high_dostart_body), w4
    sub w2, w4, w2
    mov #tblpage(loop_high_dostart_body), w4
    and #0x007f, w4
    sub w3, w4, w3
    record_double_case 0x0920, w2, w3

    set_status 0x0100
    do #1, loop_high_doend_body
    mov DOENDL, w2
    mov DOENDH, w3
loop_high_doend_body:
    nop
    mov #tbloffset(loop_high_doend_body), w4
    sub w2, w4, w2
    mov #tblpage(loop_high_doend_body), w4
    and #0x007f, w4
    sub w3, w4, w3
    record_double_case 0x0921, w2, w3

    end_results
