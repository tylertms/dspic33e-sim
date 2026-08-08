.section .text,code
.include "conformance.inc"

.macro binary_file_word operation, first_id, second_id, left, right, status
    set_status \status
    mov #\right, w0
    mov #\left, w1
    mov w1, _conformance_scratch
    \operation _conformance_scratch
    mov _conformance_scratch, w2
    mov w0, w3
    record_case \first_id, w2, w3

    set_status \status
    mov #\right, w0
    mov #\left, w1
    mov w1, _conformance_scratch
    \operation _conformance_scratch, WREG
    mov w0, w2
    mov _conformance_scratch, w3
    record_case \second_id, w2, w3
.endm

.macro binary_file_byte operation, first_id, second_id, left, right, status
    set_status \status
    mov #\right, w0
    mov #\left, w1
    mov w1, _conformance_scratch
    \operation.b _conformance_scratch
    mov _conformance_scratch, w2
    mov w0, w3
    record_case \first_id, w2, w3

    set_status \status
    mov #\right, w0
    mov #\left, w1
    mov w1, _conformance_scratch
    \operation.b _conformance_scratch, WREG
    mov w0, w2
    mov _conformance_scratch, w3
    record_case \second_id, w2, w3
.endm

.macro unary_file_word operation, first_id, second_id, value, status
    set_status \status
    mov #\value, w1
    mov w1, _conformance_scratch
    \operation _conformance_scratch
    mov _conformance_scratch, w2
    record_case \first_id, w2, w1

    set_status \status
    mov #\value, w1
    mov w1, _conformance_scratch
    mov #0x5a34, w0
    \operation _conformance_scratch, WREG
    mov w0, w2
    mov _conformance_scratch, w1
    record_case \second_id, w2, w1
.endm

.macro unary_file_byte operation, first_id, second_id, value, status
    set_status \status
    mov #\value, w1
    mov w1, _conformance_scratch
    \operation.b _conformance_scratch
    mov _conformance_scratch, w2
    record_case \first_id, w2, w1

    set_status \status
    mov #\value, w1
    mov w1, _conformance_scratch
    mov #0x5a34, w0
    \operation.b _conformance_scratch, WREG
    mov w0, w2
    mov _conformance_scratch, w1
    record_case \second_id, w2, w1
.endm

.global _arithmetic_conformance_cases
_arithmetic_conformance_cases = 127
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

    binary_file_word add, 0x0340, 0x0341, 0xffff, 0x0001, 0x0000
    binary_file_byte add, 0x0342, 0x0343, 0xa5ff, 0x5a01, 0x0000
    binary_file_word addc, 0x0344, 0x0345, 0xffff, 0x0000, 0x0001
    binary_file_byte addc, 0x0346, 0x0347, 0xa5ff, 0x5a00, 0x0001
    binary_file_word sub, 0x0348, 0x0349, 0x0000, 0x0001, 0x0000
    binary_file_byte sub, 0x034a, 0x034b, 0xa500, 0x5a01, 0x0000
    binary_file_word subb, 0x034c, 0x034d, 0x0001, 0x0000, 0x0000
    binary_file_byte subb, 0x034e, 0x034f, 0xa501, 0x5a00, 0x0000
    binary_file_word subr, 0x0350, 0x0351, 0x0001, 0x0003, 0x0000
    binary_file_byte subr, 0x0352, 0x0353, 0xa501, 0x5a03, 0x0000
    binary_file_word subbr, 0x0354, 0x0355, 0x0001, 0x0003, 0x0000
    binary_file_byte subbr, 0x0356, 0x0357, 0xa501, 0x5a03, 0x0000
    binary_file_word and, 0x0358, 0x0359, 0x0ff0, 0x00ff, 0x0105
    binary_file_byte and, 0x035a, 0x035b, 0xa5f0, 0x5a0f, 0x0105
    binary_file_word xor, 0x035c, 0x035d, 0x0ff0, 0x00ff, 0x0105
    binary_file_byte xor, 0x035e, 0x035f, 0xa5f0, 0x5a0f, 0x0105
    binary_file_word ior, 0x0360, 0x0361, 0x0ff0, 0x00ff, 0x0105
    binary_file_byte ior, 0x0362, 0x0363, 0xa5f0, 0x5a0f, 0x0105

    set_status 0x0000
    mov #0x1234, w0
    mov #0x1234, w1
    mov w1, _conformance_scratch
    cp _conformance_scratch
    mov _conformance_scratch, w2
    mov w0, w3
    record_case 0x0364, w2, w3

    set_status 0x0000
    mov #0x5a34, w0
    mov #0xa534, w1
    mov w1, _conformance_scratch
    cp.b _conformance_scratch
    mov _conformance_scratch, w2
    mov w0, w3
    record_case 0x0365, w2, w3

    set_status 0x0000
    mov #0x1233, w0
    mov #0x1234, w1
    mov w1, _conformance_scratch
    cpb _conformance_scratch
    mov _conformance_scratch, w2
    mov w0, w3
    record_case 0x0366, w2, w3

    set_status 0x0000
    mov #0x5a33, w0
    mov #0xa534, w1
    mov w1, _conformance_scratch
    cpb.b _conformance_scratch
    mov _conformance_scratch, w2
    mov w0, w3
    record_case 0x0367, w2, w3

    set_status 0x0000
    mov #0x8000, w1
    mov w1, _conformance_scratch
    cp0 _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x0368, w2, w1

    set_status 0x0000
    mov #0xa580, w1
    mov w1, _conformance_scratch
    cp0.b _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x0369, w2, w1

    unary_file_word neg, 0x036a, 0x036b, 0x8000, 0x0000
    unary_file_byte neg, 0x036c, 0x036d, 0xa580, 0x0000
    unary_file_word com, 0x036e, 0x036f, 0x0ff0, 0x0105
    unary_file_byte com, 0x0370, 0x0371, 0xa5f0, 0x0105

    set_status 0x0105
    mov #0xa55a, w1
    mov w1, _conformance_scratch
    clr _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x0372, w2, w1

    set_status 0x0105
    mov #0xa55a, w1
    mov w1, _conformance_scratch
    clr.b _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x0373, w2, w1

    set_status 0x0105
    mov #0xa55a, w0
    clr WREG
    mov w0, w2
    record_case 0x0374, w2, w2

    set_status 0x0105
    mov #0xa55a, w0
    clr.b WREG
    mov w0, w2
    record_case 0x0375, w2, w2

    set_status 0x0105
    mov #0xa55a, w1
    mov w1, _conformance_scratch
    setm _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x0376, w2, w1

    set_status 0x0105
    mov #0xa55a, w1
    mov w1, _conformance_scratch
    setm.b _conformance_scratch
    mov _conformance_scratch, w2
    record_case 0x0377, w2, w1

    set_status 0x0105
    mov #0xa55a, w0
    setm WREG
    mov w0, w2
    record_case 0x0378, w2, w2

    set_status 0x0105
    mov #0xa55a, w0
    setm.b WREG
    mov w0, w2
    record_case 0x0379, w2, w2

    set_status 0x0001
    mov #0xfc00, w1
    addc #0x03ff, w1
    record_case 0x037a, w1, w1

    set_status 0x0001
    mov #0xa500, w1
    addc.b #0x00ff, w1
    record_case 0x037b, w1, w1

    set_status 0x0000
    mov #0x0400, w1
    subb #0x03ff, w1
    record_case 0x037c, w1, w1

    set_status 0x0000
    mov #0xa500, w1
    subb.b #0x00ff, w1
    record_case 0x037d, w1, w1

    set_status 0x0000
    mov #0xa501, w1
    add.b #0x00ff, w1
    record_case 0x037e, w1, w1

    set_status 0x0000
    mov #0xa500, w1
    sub.b #0x00ff, w1
    record_case 0x037f, w1, w1

    set_status 0x0105
    mov #0xa5ff, w1
    and.b #0x005a, w1
    record_case 0x0380, w1, w1

    set_status 0x0105
    mov #0xa5f0, w1
    xor.b #0x005a, w1
    record_case 0x0381, w1, w1

    set_status 0x0105
    mov #0xa50f, w1
    ior.b #0x005a, w1
    record_case 0x0382, w1, w1

    set_status 0x0001
    mov #0x7fff, w1
    clr w2
    addc w1, w2, w3
    record_case 0x0383, w3, w2

    set_status 0x0001
    mov #0x7fff, w1
    mov #0xffff, w2
    addc w1, w2, w3
    record_case 0x0384, w3, w2

    set_status 0x0001
    mov #0xa57f, w1
    clr w2
    mov #0x5a34, w3
    addc.b w1, w2, w3
    record_case 0x0385, w3, w2

    end_results
