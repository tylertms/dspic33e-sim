.section .text,code
.include "conformance.inc"

.global _shift_conformance_cases
_shift_conformance_cases = 22
.global _shift_conformance_group_complete
_shift_conformance_group_complete = 0

.global _run_shift_conformance
_run_shift_conformance:
    begin_results

    set_status 0x0000
    mov #0x0001, w1
    mov #0x5555, w2
    rrc w1, w2
    record_case 0x0100, w2, w1

    set_status 0x0001
    mov #0x0000, w1
    mov #0x5555, w2
    rrc w1, w2
    record_case 0x0101, w2, w1

    set_status 0x0001
    mov #0xa501, w1
    mov #0x5a34, w2
    rrc.b w1, w2
    record_case 0x0102, w2, w1

    set_status 0x0000
    mov #0x8000, w1
    mov #0x5555, w2
    rlc w1, w2
    record_case 0x0103, w2, w1

    set_status 0x0001
    mov #0x0000, w1
    mov #0x5555, w2
    rlc w1, w2
    record_case 0x0104, w2, w1

    set_status 0x0001
    mov #0xa580, w1
    mov #0x5a34, w2
    rlc.b w1, w2
    record_case 0x0105, w2, w1

    set_status 0x0001
    mov #0x0001, w1
    mov #0x5555, w2
    rrnc w1, w2
    record_case 0x0106, w2, w1

    set_status 0x0000
    mov #0x8000, w1
    mov #0x5555, w2
    rlnc w1, w2
    record_case 0x0107, w2, w1

    set_status 0x0001
    mov #0xa581, w1
    mov #0x5a34, w2
    rrnc.b w1, w2
    record_case 0x0108, w2, w1

    set_status 0x0001
    mov #0xa581, w1
    mov #0x5a34, w2
    rlnc.b w1, w2
    record_case 0x0109, w2, w1

    set_status 0x0000
    mov #0x8001, w1
    lsr w1, #1, w2
    record_case 0x0110, w2, w1

    set_status 0x0000
    mov #0x8001, w1
    asr w1, #1, w2
    record_case 0x0111, w2, w1

    set_status 0x0000
    mov #0x8001, w1
    sl w1, #1, w2
    record_case 0x0112, w2, w1

    set_status 0x0000
    mov #0x8001, w1
    mov #15, w3
    lsr w1, w3, w2
    record_case 0x0113, w2, w3

    set_status 0x0000
    mov #0x8001, w1
    mov #15, w3
    asr w1, w3, w2
    record_case 0x0114, w2, w3

    set_status 0x0000
    mov #0x8001, w1
    mov #15, w3
    sl w1, w3, w2
    record_case 0x0115, w2, w3

    set_status 0x0000
    mov #0xffff, w1
    mov #0x5555, w2
    inc.b w1, w2
    record_case 0x0120, w2, w1

    set_status 0x0000
    mov #0xffff, w1
    mov #0x5555, w2
    inc w1, w2
    record_case 0x0121, w2, w1

    set_status 0x0000
    mov #0xfffe, w1
    mov #0x5555, w2
    inc2 w1, w2
    record_case 0x0122, w2, w1

    set_status 0x0000
    mov #0x0000, w1
    mov #0x5555, w2
    dec.b w1, w2
    record_case 0x0123, w2, w1

    set_status 0x0000
    mov #0x0000, w1
    mov #0x5555, w2
    dec w1, w2
    record_case 0x0124, w2, w1

    set_status 0x0000
    mov #0x0001, w1
    mov #0x5555, w2
    dec2 w1, w2
    record_case 0x0125, w2, w1

    end_results

.global _conformance_complete
_conformance_complete:
    bra _conformance_complete
