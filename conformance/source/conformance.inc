.macro begin_results
    push w6
    push w7
    mov w0, w6
    mov w0, w7
.endm

.macro end_results
    sub w7, w6, w0
    lsr w0, #1, w0
    pop w7
    pop w6
    return
.endm

.macro set_status value
    mov #\value, w0
    mov w0, SR
.endm

.macro record_case id, result, auxiliary
    mov #\id, w0
    mov w0, [w7++]
    mov \result, [w7++]
    mov SR, w0
    mov w0, [w7++]
    mov \auxiliary, [w7++]
.endm

.macro record_double_case id, low, high
    mov #\id, w0
    mov w0, [w7++]
    mov \low, [w7++]
    mov \high, [w7++]
    mov SR, w0
    mov w0, [w7++]
.endm
