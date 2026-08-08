.section .text,code
.include "conformance.inc"

.global _arithmetic_conformance_cases
_arithmetic_conformance_cases = 57
.global _arithmetic_conformance_group_complete
_arithmetic_conformance_group_complete = 0

.global _run_arithmetic_conformance
_run_arithmetic_conformance:
    begin_results

    set_status 0x0000
    clr w1
    clr w2
    add w1, w2, w3
    record_case 0x0300, w3, w2

    set_status 0x0000
    mov #0xffff, w1
    mov #1, w2
    add w1, w2, w3
    record_case 0x0301, w3, w2

    set_status 0x0000
    mov #0x7fff, w1
    mov #1, w2
    add w1, w2, w3
    record_case 0x0302, w3, w2

    set_status 0x0000
    mov #0x8000, w1
    mov #0x8000, w2
    add w1, w2, w3
    record_case 0x0303, w3, w2

    set_status 0x0000
    mov #0xa5ff, w1
    mov #1, w2
    mov #0x5a34, w3
    add.b w1, w2, w3
    record_case 0x0304, w3, w2

    set_status 0x0000
    mov #0xffff, w1
    clr w2
    addc w1, w2, w3
    record_case 0x0305, w3, w2

    set_status 0x0001
    mov #0xffff, w1
    clr w2
    addc w1, w2, w3
    record_case 0x0306, w3, w2

    set_status 0x0003
    mov #0xffff, w1
    clr w2
    addc w1, w2, w3
    record_case 0x0307, w3, w2

    set_status 0x0000
    clr w1
    clr w2
    sub w1, w2, w3
    record_case 0x0308, w3, w2

    set_status 0x0000
    clr w1
    mov #1, w2
    sub w1, w2, w3
    record_case 0x0309, w3, w2

    set_status 0x0000
    mov #0x8000, w1
    mov #1, w2
    sub w1, w2, w3
    record_case 0x030a, w3, w2

    set_status 0x0000
    mov #0x7fff, w1
    mov #0xffff, w2
    sub w1, w2, w3
    record_case 0x030b, w3, w2

    set_status 0x0000
    mov #0xa500, w1
    mov #1, w2
    mov #0x5a34, w3
    sub.b w1, w2, w3
    record_case 0x030c, w3, w2

    set_status 0x0001
    mov #1, w1
    mov #1, w2
    subb w1, w2, w3
    record_case 0x030d, w3, w2

    set_status 0x0000
    mov #1, w1
    mov #1, w2
    subb w1, w2, w3
    record_case 0x030e, w3, w2

    set_status 0x0000
    mov #1, w1
    mov #3, w2
    subr w1, w2, w3
    record_case 0x030f, w3, w2

    set_status 0x0001
    mov #1, w1
    mov #3, w2
    subbr w1, w2, w3
    record_case 0x0310, w3, w2

    set_status 0x0000
    mov #1, w1
    mov #3, w2
    subbr w1, w2, w3
    record_case 0x0311, w3, w2

    set_status 0x0105
    mov #0x0f0f, w1
    mov #0x00f0, w2
    and w1, w2, w3
    record_case 0x0312, w3, w2

    set_status 0x0105
    mov #0x0f0f, w1
    mov #0x00f0, w2
    xor w1, w2, w3
    record_case 0x0313, w3, w2

    set_status 0x0105
    mov #0x0f0f, w1
    mov #0x00f0, w2
    ior w1, w2, w3
    record_case 0x0314, w3, w2

    set_status 0x0105
    mov #0xa50f, w1
    mov #0x00f0, w2
    mov #0x5a34, w3
    and.b w1, w2, w3
    record_case 0x0315, w3, w2

    set_status 0x0105
    mov #0xa50f, w1
    mov #0x00f0, w2
    mov #0x5a34, w3
    xor.b w1, w2, w3
    record_case 0x0316, w3, w2

    set_status 0x0105
    mov #0xa50f, w1
    mov #0x00f0, w2
    mov #0x5a34, w3
    ior.b w1, w2, w3
    record_case 0x0317, w3, w2

    set_status 0x0000
    mov #0x1234, w1
    mov #0x1234, w2
    cp w1, w2
    record_case 0x0318, w1, w2

    set_status 0x0000
    mov #0x1233, w1
    mov #0x1234, w2
    cp w1, w2
    record_case 0x0319, w1, w2

    set_status 0x0000
    mov #0x1234, w1
    mov #0x1233, w2
    cpb w1, w2
    record_case 0x031a, w1, w2

    set_status 0x0000
    clr w1
    cp0 w1
    record_case 0x031b, w1, w1

    set_status 0x0000
    mov #0x8000, w1
    cp0 w1
    record_case 0x031c, w1, w1

    set_status 0x0000
    mov #1, w1
    neg w1, w2
    record_case 0x031d, w2, w1

    set_status 0x0000
    mov #0x8000, w1
    neg w1, w2
    record_case 0x031e, w2, w1

    set_status 0x0000
    mov #0xa501, w1
    mov #0x5a34, w2
    neg.b w1, w2
    record_case 0x031f, w2, w1

    set_status 0x0105
    mov #0x0f0f, w1
    com w1, w2
    record_case 0x0320, w2, w1

    set_status 0x0105
    mov #0xa50f, w1
    mov #0x5a34, w2
    com.b w1, w2
    record_case 0x0321, w2, w1

    set_status 0x0105
    mov #0x5a34, w2
    clr w2
    record_case 0x0322, w2, w2

    set_status 0x0105
    mov #0x5a34, w2
    clr.b w2
    record_case 0x0323, w2, w2

    set_status 0x0105
    clr w2
    setm w2
    record_case 0x0324, w2, w2

    set_status 0x0105
    mov #0x5a34, w2
    setm.b w2
    record_case 0x0325, w2, w2

    set_status 0x0000
    mov #0x7f00, w1
    add #0x155, w1
    record_case 0x0326, w1, w1

    set_status 0x0000
    mov #0xffff, w1
    add #1, w1
    record_case 0x0327, w1, w1

    set_status 0x0000
    mov #0x0100, w1
    sub #0x155, w1
    record_case 0x0328, w1, w1

    set_status 0x0105
    mov #0xffff, w1
    and #0x155, w1
    record_case 0x0329, w1, w1

    set_status 0x0105
    mov #0x0f0f, w1
    xor #0x155, w1
    record_case 0x032a, w1, w1

    set_status 0x0105
    mov #0x0f0f, w1
    ior #0x155, w1
    record_case 0x032b, w1, w1

    set_status 0x0000
    mov #0x7ff0, w1
    add w1, #31, w2
    record_case 0x032c, w2, w1

    set_status 0x0000
    mov #0x0010, w1
    sub w1, #31, w2
    record_case 0x032d, w2, w1

    set_status 0x0000
    mov #0x7ff0, w1
    mov #31, w2
    add w1, w2, w3
    record_case 0x032e, w3, w2

    set_status 0x0000
    mov #0x7ff0, w1
    add #31, w1
    record_case 0x032f, w1, w1

    set_status 0x0000
    mov #0x000f, w1
    mov #1, w2
    add w1, w2, w3
    record_case 0x0330, w3, w2

    set_status 0x0000
    mov #0x000f, w1
    add #1, w1
    record_case 0x0331, w1, w1

    set_status 0x0000
    mov #0x000f, w1
    add w1, #1, w2
    record_case 0x0332, w2, w1

    set_status 0x0000
    mov #0x0010, w1
    mov #1, w2
    sub w1, w2, w3
    record_case 0x0333, w3, w2

    set_status 0x0000
    mov #0x0010, w1
    sub #1, w1
    record_case 0x0334, w1, w1

    set_status 0x0000
    mov #0x0010, w1
    sub w1, #1, w2
    record_case 0x0335, w2, w1

    set_status 0x0000
    clr w1
    mov #0x0010, w2
    sub w1, w2, w3
    record_case 0x0336, w3, w2

    set_status 0x0000
    clr w1
    sub #0x0010, w1
    record_case 0x0337, w1, w1

    set_status 0x0000
    clr w1
    sub w1, #0x0010, w2
    record_case 0x0338, w2, w1

    end_results
