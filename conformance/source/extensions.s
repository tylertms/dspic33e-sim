.section .text,code
.include "conformance.inc"

.global _extension_conformance_cases
_extension_conformance_cases = 35
.global _extension_conformance_group_complete
_extension_conformance_group_complete = 1

.global _run_extension_conformance
_run_extension_conformance:
    begin_results

    set_status 0x0105
    mov #0x0000, w1
    se w1, w2
    record_case 0x0400, w2, w1

    set_status 0x0105
    mov #0x007f, w1
    se w1, w2
    record_case 0x0401, w2, w1

    set_status 0x0105
    mov #0x0080, w1
    se w1, w2
    record_case 0x0402, w2, w1

    set_status 0x0105
    mov #0xa5ff, w1
    se w1, w2
    record_case 0x0403, w2, w1

    set_status 0x0105
    mov #0x0000, w1
    ze w1, w2
    record_case 0x0404, w2, w1

    set_status 0x0105
    mov #0x007f, w1
    ze w1, w2
    record_case 0x0405, w2, w1

    set_status 0x0105
    mov #0x0080, w1
    ze w1, w2
    record_case 0x0406, w2, w1

    set_status 0x0105
    mov #0xa5ff, w1
    ze w1, w2
    record_case 0x0407, w2, w1

    set_status 0x0105
    mov w15, w4
    mov #0x0080, w1
    mov w1, [w4]
    se [w4++], w2
    record_case 0x0408, w2, w4

    set_status 0x0105
    mov w15, w4
    mov #0xa5ff, w1
    mov w1, [w4]
    ze [w4++], w2
    record_case 0x0409, w2, w4

    set_status 0x0000
    mov #0x0000, w1
    ff1l w1, w2
    record_case 0x040a, w2, w1

    set_status 0x0000
    mov #0x0001, w1
    ff1l w1, w2
    record_case 0x040b, w2, w1

    set_status 0x0000
    mov #0x0002, w1
    ff1l w1, w2
    record_case 0x040c, w2, w1

    set_status 0x0000
    mov #0x8000, w1
    ff1l w1, w2
    record_case 0x040d, w2, w1

    set_status 0x0000
    mov #0x4000, w1
    ff1l w1, w2
    record_case 0x040e, w2, w1

    set_status 0x0000
    mov #0xffff, w1
    ff1l w1, w2
    record_case 0x040f, w2, w1

    set_status 0x0000
    mov #0x00f0, w1
    ff1l w1, w2
    record_case 0x0410, w2, w1

    set_status 0x0000
    mov #0x0000, w1
    ff1r w1, w2
    record_case 0x0411, w2, w1

    set_status 0x0000
    mov #0x0001, w1
    ff1r w1, w2
    record_case 0x0412, w2, w1

    set_status 0x0000
    mov #0x0002, w1
    ff1r w1, w2
    record_case 0x0413, w2, w1

    set_status 0x0000
    mov #0x8000, w1
    ff1r w1, w2
    record_case 0x0414, w2, w1

    set_status 0x0000
    mov #0x4000, w1
    ff1r w1, w2
    record_case 0x0415, w2, w1

    set_status 0x0000
    mov #0xffff, w1
    ff1r w1, w2
    record_case 0x0416, w2, w1

    set_status 0x0000
    mov #0x00f0, w1
    ff1r w1, w2
    record_case 0x0417, w2, w1

    set_status 0x0000
    mov #0x0000, w1
    fbcl w1, w2
    record_case 0x0418, w2, w1

    set_status 0x0000
    mov #0x0001, w1
    fbcl w1, w2
    record_case 0x0419, w2, w1

    set_status 0x0000
    mov #0x7fff, w1
    fbcl w1, w2
    record_case 0x041a, w2, w1

    set_status 0x0000
    mov #0x8000, w1
    fbcl w1, w2
    record_case 0x041b, w2, w1

    set_status 0x0000
    mov #0xffff, w1
    fbcl w1, w2
    record_case 0x041c, w2, w1

    set_status 0x0000
    mov #0x4000, w1
    fbcl w1, w2
    record_case 0x041d, w2, w1

    set_status 0x0000
    mov #0xc000, w1
    fbcl w1, w2
    record_case 0x041e, w2, w1

    set_status 0x0000
    mov w15, w4
    mov #0x0040, w1
    mov w1, [w4]
    ff1l [w4++], w2
    record_case 0x041f, w2, w4

    set_status 0x0000
    mov w15, w4
    mov #0x0400, w1
    mov w1, [w4]
    ff1r [w4++], w2
    record_case 0x0420, w2, w4

    set_status 0x0000
    mov #0x00ff, w1
    ze w1, w2
    record_case 0x0421, w2, w1

    set_status 0x0000
    mov #0x007f, w1
    se w1, w2
    record_case 0x0422, w2, w1

    end_results
