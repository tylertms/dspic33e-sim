.section .text,code
.include "conformance.inc"

.macro file_unary_word operation, first_id, second_id, value, status
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

.macro file_unary_byte operation, first_id, second_id, value, status
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

.macro pack_shift_pointer_deltas source_base, destination_base
    mov SR, w0
    mov #\source_base, w1
    sub w4, w1, w3
    and #0x00ff, w3
    mov #\destination_base, w1
    sub w5, w1, w5
    sl w5, #8, w5
    ior w3, w5, w3
    mov w0, SR
.endm

.global _shift_conformance_cases
_shift_conformance_cases = 111
.global _shift_conformance_group_complete
_shift_conformance_group_complete = 1

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

    file_unary_word asr, 0x0130, 0x0131, 0x8001, 0x0105
    file_unary_byte asr, 0x0132, 0x0133, 0xa581, 0x0105
    file_unary_word lsr, 0x0134, 0x0135, 0x8001, 0x0105
    file_unary_byte lsr, 0x0136, 0x0137, 0xa581, 0x0105
    file_unary_word sl, 0x0138, 0x0139, 0x8001, 0x0105
    file_unary_byte sl, 0x013a, 0x013b, 0xa581, 0x0105
    file_unary_word rlc, 0x013c, 0x013d, 0x8001, 0x0105
    file_unary_byte rlc, 0x013e, 0x013f, 0xa581, 0x0105
    file_unary_word rlnc, 0x0140, 0x0141, 0x8001, 0x0105
    file_unary_byte rlnc, 0x0142, 0x0143, 0xa581, 0x0105
    file_unary_word rrc, 0x0144, 0x0145, 0x8001, 0x0105
    file_unary_byte rrc, 0x0146, 0x0147, 0xa581, 0x0105
    file_unary_word rrnc, 0x0148, 0x0149, 0x8001, 0x0105
    file_unary_byte rrnc, 0x014a, 0x014b, 0xa581, 0x0105

    set_status 0x0105
    mov #0x8001, w1
    mov #0x5a34, w2
    asr w1, w2
    record_case 0x014c, w2, w1

    set_status 0x0105
    mov #0xa581, w1
    mov #0x5a34, w2
    asr.b w1, w2
    record_case 0x014d, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    mov #0x5a34, w2
    lsr w1, w2
    record_case 0x014e, w2, w1

    set_status 0x0105
    mov #0xa581, w1
    mov #0x5a34, w2
    lsr.b w1, w2
    record_case 0x014f, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    mov #0x5a34, w2
    sl w1, w2
    record_case 0x0150, w2, w1

    set_status 0x0105
    mov #0xa581, w1
    mov #0x5a34, w2
    sl.b w1, w2
    record_case 0x0151, w2, w1

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x8001, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    asr [w4], [w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0152, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0xa581, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    asr.b [w4++], [w5++]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0153, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x8001, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    lsr [w4--], [w5--]
    mov [w5+2], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0154, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+3, w4
    mov #_conformance_scratch+19, w5
    mov #0xa581, w1
    mov w1, _conformance_scratch+4
    mov #0x5a34, w2
    mov w2, _conformance_scratch+20
    lsr.b [++w4], [++w5]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+3, _conformance_scratch+19
    record_case 0x0155, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x8001, w1
    mov w1, [w4-2]
    mov #0x5a34, w2
    mov w2, [w5-2]
    sl [--w4], [--w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0156, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0xa581, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    sl.b [w4], [w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0157, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x8001, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    rlc [w4++], [w5--]
    mov [w5+2], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0158, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+19, w5
    mov #0xa581, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, _conformance_scratch+20
    rlc.b [w4--], [++w5]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+19
    record_case 0x0159, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x8001, w1
    mov w1, [w4+2]
    mov #0x5a34, w2
    mov w2, [w5]
    rlnc [++w4], [w5++]
    mov [w5-2], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x015a, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+5, w4
    mov #_conformance_scratch+20, w5
    mov #0xa581, w1
    mov w1, _conformance_scratch+4
    mov #0x5a34, w2
    mov w2, [w5]
    rlnc.b [--w4], [w5--]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+5, _conformance_scratch+20
    record_case 0x015b, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x8001, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5+2]
    rrc [w4], [++w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x015c, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+21, w5
    mov #0xa581, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, _conformance_scratch+20
    rrc.b [w4++], [--w5]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+21
    record_case 0x015d, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x8001, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    rrnc [w4--], [w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x015e, w2, w3

    set_status 0x0105
    mov #_conformance_scratch+3, w4
    mov #_conformance_scratch+20, w5
    mov #0xa581, w1
    mov w1, _conformance_scratch+4
    mov #0x5a34, w2
    mov w2, [w5]
    rrnc.b [++w4], [w5++]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+3, _conformance_scratch+20
    record_case 0x015f, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    asr w1, #0, w2
    record_case 0x0160, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    lsr w1, #0, w2
    record_case 0x0161, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    sl w1, #0, w2
    record_case 0x0162, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    asr w1, #15, w2
    record_case 0x0163, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    lsr w1, #15, w2
    record_case 0x0164, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    sl w1, #15, w2
    record_case 0x0165, w2, w1

    set_status 0x0105
    mov #0x8001, w1
    clr w3
    asr w1, w3, w2
    record_case 0x0166, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    clr w3
    lsr w1, w3, w2
    record_case 0x0167, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    clr w3
    sl w1, w3, w2
    record_case 0x0168, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    mov #16, w3
    asr w1, w3, w2
    record_case 0x0169, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    mov #16, w3
    lsr w1, w3, w2
    record_case 0x016a, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    mov #16, w3
    sl w1, w3, w2
    record_case 0x016b, w2, w3

    set_status 0x0105
    mov #0x7fff, w1
    mov #0xffff, w3
    asr w1, w3, w2
    record_case 0x016c, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    mov #0xffff, w3
    lsr w1, w3, w2
    record_case 0x016d, w2, w3

    set_status 0x0105
    mov #0x8001, w1
    mov #0xffff, w3
    sl w1, w3, w2
    record_case 0x016e, w2, w3

    file_unary_word inc, 0x0170, 0x0171, 0xffff, 0x0000
    file_unary_byte inc, 0x0172, 0x0173, 0xa5ff, 0x0000
    file_unary_word inc2, 0x0174, 0x0175, 0xfffe, 0x0000
    file_unary_byte inc2, 0x0176, 0x0177, 0xa5fe, 0x0000
    file_unary_word dec, 0x0178, 0x0179, 0x0000, 0x0000
    file_unary_byte dec, 0x017a, 0x017b, 0xa500, 0x0000
    file_unary_word dec2, 0x017c, 0x017d, 0x0001, 0x0000
    file_unary_byte dec2, 0x017e, 0x017f, 0xa501, 0x0000

    set_status 0x0000
    mov #0xa5fe, w1
    mov #0x5a34, w2
    inc2.b w1, w2
    record_case 0x0180, w2, w1

    set_status 0x0000
    mov #0xa501, w1
    mov #0x5a34, w2
    dec2.b w1, w2
    record_case 0x0181, w2, w1

    set_status 0x0000
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0xffff, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    inc [w4], [w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0182, w2, w3

    set_status 0x0000
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0xa5ff, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    inc.b [w4++], [w5++]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0183, w2, w3

    set_status 0x0000
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0xfffe, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    inc2 [w4--], [w5--]
    mov [w5+2], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0184, w2, w3

    set_status 0x0000
    mov #_conformance_scratch+3, w4
    mov #_conformance_scratch+19, w5
    mov #0xa5fe, w1
    mov w1, _conformance_scratch+4
    mov #0x5a34, w2
    mov w2, _conformance_scratch+20
    inc2.b [++w4], [++w5]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+3, _conformance_scratch+19
    record_case 0x0185, w2, w3

    set_status 0x0000
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x0000, w1
    mov w1, [w4-2]
    mov #0x5a34, w2
    mov w2, [w5-2]
    dec [--w4], [--w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0186, w2, w3

    set_status 0x0000
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0xa500, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    dec.b [w4], [w5]
    mov [w5], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0187, w2, w3

    set_status 0x0000
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+20, w5
    mov #0x0001, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, [w5]
    dec2 [w4++], [w5--]
    mov [w5+2], w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+20
    record_case 0x0188, w2, w3

    set_status 0x0000
    mov #_conformance_scratch+4, w4
    mov #_conformance_scratch+19, w5
    mov #0xa501, w1
    mov w1, [w4]
    mov #0x5a34, w2
    mov w2, _conformance_scratch+20
    dec2.b [w4--], [++w5]
    mov _conformance_scratch+20, w2
    pack_shift_pointer_deltas _conformance_scratch+4, _conformance_scratch+19
    record_case 0x0189, w2, w3

    end_results

.global _conformance_complete
_conformance_complete:
    bra _conformance_complete
