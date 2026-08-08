.section .text,code
.include "conformance.inc"

.global _bit_conformance_cases
_bit_conformance_cases = 70
.global _bit_conformance_group_complete
_bit_conformance_group_complete = 1

.macro mutate_register id, instruction, initial, bit
    set_status 0x010f
    mov #\initial, w2
    \instruction w2, #\bit
    record_case \id, w2, w2
.endm

.macro mutate_file id, instruction, initial, address, bit
    mov #\initial, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    \instruction \address, #\bit
    mov _conformance_scratch, w2
    record_case \id, w2, w2
.endm

.global _run_bit_conformance
_run_bit_conformance:
    begin_results

    mutate_register 0x0100, bset, 0x0000, 0
    mutate_register 0x0101, bset, 0x0000, 15
    mutate_register 0x0102, bset.b, 0xa500, 7

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010f
    bset [w4], #8
    mov [w4], w2
    record_case 0x0103, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010f
    bset [w4++], #9
    mov [w4-2], w2
    record_case 0x0104, w2, w4

    mov #_conformance_scratch+1, w4
    clr w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bset.b [w4--], #7
    mov _conformance_scratch, w2
    record_case 0x0105, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4+2]
    set_status 0x010f
    bset [++w4], #10
    mov [w4], w2
    record_case 0x0106, w2, w4

    mov #_conformance_scratch+1, w4
    clr w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bset.b [--w4], #6
    mov [w4], w2
    record_case 0x0107, w2, w4

    mutate_file 0x0108, bset, 0x0000, _conformance_scratch, 15
    mutate_file 0x0109, bset.b, 0x0000, _conformance_scratch+1, 7

    mutate_register 0x010a, bclr, 0xffff, 0
    mutate_register 0x010b, bclr, 0xffff, 15
    mutate_register 0x010c, bclr.b, 0xa5ff, 7

    mov #_conformance_scratch, w4
    mov #0xffff, w1
    mov w1, [w4]
    set_status 0x010f
    bclr [w4], #8
    mov [w4], w2
    record_case 0x010d, w2, w4

    mov #_conformance_scratch, w4
    mov #0xffff, w1
    mov w1, [w4]
    set_status 0x010f
    bclr [w4++], #9
    mov [w4-2], w2
    record_case 0x010e, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0xffff, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bclr.b [w4--], #7
    mov _conformance_scratch, w2
    record_case 0x010f, w2, w4

    mov #_conformance_scratch, w4
    mov #0xffff, w1
    mov w1, [w4+2]
    set_status 0x010f
    bclr [++w4], #10
    mov [w4], w2
    record_case 0x0110, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0xffff, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bclr.b [--w4], #6
    mov [w4], w2
    record_case 0x0111, w2, w4

    mutate_file 0x0112, bclr, 0xffff, _conformance_scratch, 15
    mutate_file 0x0113, bclr.b, 0xffff, _conformance_scratch+1, 7

    mutate_register 0x0114, btg, 0x0000, 0
    mutate_register 0x0115, btg, 0x8000, 15
    mutate_register 0x0116, btg.b, 0xa580, 7

    mov #_conformance_scratch, w4
    mov #0x0100, w1
    mov w1, [w4]
    set_status 0x010f
    btg [w4], #8
    mov [w4], w2
    record_case 0x0117, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010f
    btg [w4++], #9
    mov [w4-2], w2
    record_case 0x0118, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0x8000, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    btg.b [w4--], #7
    mov _conformance_scratch, w2
    record_case 0x0119, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4+2]
    set_status 0x010f
    btg [++w4], #10
    mov [w4], w2
    record_case 0x011a, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0x0040, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    btg.b [--w4], #6
    mov [w4], w2
    record_case 0x011b, w2, w4

    mutate_file 0x011c, btg, 0x0000, _conformance_scratch, 15
    mutate_file 0x011d, btg.b, 0x8000, _conformance_scratch+1, 7

    mov #0x8000, w2
    set_status 0x010e
    btst.c w2, #15
    record_case 0x011e, w2, w2

    clr w2
    set_status 0x010f
    btst.c w2, #15
    record_case 0x011f, w2, w2

    mov #0x8000, w2
    set_status 0x010f
    btst.z w2, #15
    record_case 0x0120, w2, w2

    clr w2
    set_status 0x010d
    btst.z w2, #15
    record_case 0x0121, w2, w2

    mov #_conformance_scratch, w4
    mov #0x0008, w1
    mov w1, [w4]
    set_status 0x010e
    btst.c [w4], #3
    mov [w4], w2
    record_case 0x0122, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010d
    btst.z [w4++], #3
    mov [w4-2], w2
    record_case 0x0123, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x0008, w1
    mov w1, [w4]
    set_status 0x010e
    btst.c [w4--], #3
    mov [w4+2], w2
    record_case 0x0124, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4+2]
    set_status 0x010d
    btst.z [++w4], #3
    mov [w4], w2
    record_case 0x0125, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x0008, w1
    mov w1, [w4-2]
    set_status 0x010e
    btst.c [--w4], #3
    mov [w4], w2
    record_case 0x0126, w2, w4

    mov #0x0008, w2
    mov #0xfff3, w3
    set_status 0x010e
    btst.c w2, w3
    record_case 0x0127, w2, w3

    clr w2
    mov #0xfff3, w3
    set_status 0x010f
    btst.c w2, w3
    record_case 0x0128, w2, w3

    mov #0x0008, w2
    mov #0xfff3, w3
    set_status 0x010f
    btst.z w2, w3
    record_case 0x0129, w2, w3

    clr w2
    mov #0xfff3, w3
    set_status 0x010d
    btst.z w2, w3
    record_case 0x012a, w2, w3

    mov #_conformance_scratch, w4
    mov #0x0008, w1
    mov w1, [w4]
    mov #0x0013, w3
    set_status 0x010f
    btst.z [w4], w3
    mov [w4], w2
    record_case 0x012b, w2, w3

    clr w2
    set_status 0x010e
    btsts.c w2, #5
    record_case 0x012c, w2, w2

    mov #0x0020, w2
    set_status 0x010e
    btsts.c w2, #5
    record_case 0x012d, w2, w2

    clr w2
    set_status 0x010d
    btsts.z w2, #5
    record_case 0x012e, w2, w2

    mov #0x0020, w2
    set_status 0x010f
    btsts.z w2, #5
    record_case 0x012f, w2, w2

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010e
    btsts.c [w4], #5
    mov [w4], w2
    record_case 0x0130, w2, w4

    mov #_conformance_scratch, w4
    mov #0x0020, w1
    mov w1, [w4]
    set_status 0x010f
    btsts.z [w4++], #5
    mov [w4-2], w2
    record_case 0x0131, w2, w4

    mov #_conformance_scratch+2, w4
    clr w1
    mov w1, [w4]
    set_status 0x010e
    btsts.c [w4--], #5
    mov [w4+2], w2
    record_case 0x0132, w2, w4

    mov #_conformance_scratch, w4
    mov #0x0020, w1
    mov w1, [w4+2]
    set_status 0x010f
    btsts.z [++w4], #5
    mov [w4], w2
    record_case 0x0133, w2, w4

    mov #_conformance_scratch+2, w4
    clr w1
    mov w1, [w4-2]
    set_status 0x010e
    btsts.c [--w4], #5
    mov [w4], w2
    record_case 0x0134, w2, w4

    clr w1
    mov w1, _conformance_scratch
    set_status 0x010d
    btsts _conformance_scratch, #15
    mov _conformance_scratch, w2
    record_case 0x0135, w2, w2

    clr w1
    mov w1, _conformance_scratch
    set_status 0x010d
    btsts.b _conformance_scratch+1, #7
    mov _conformance_scratch, w2
    record_case 0x0136, w2, w2

    mov #0x8000, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    btsts.b _conformance_scratch+1, #7
    mov _conformance_scratch, w2
    record_case 0x0137, w2, w2

    clr w2
    clr w3
    set_status 0x010d
    btsc w3, #0
    mov #1, w2
    record_case 0x0138, w2, w3

    clr w2
    mov #1, w3
    set_status 0x010d
    btsc w3, #0
    mov #1, w2
    record_case 0x0139, w2, w3

    clr w2
    mov #1, w3
    set_status 0x010d
    btss w3, #0
    mov #1, w2
    record_case 0x013a, w2, w3

    clr w2
    clr w3
    set_status 0x010d
    btss w3, #0
    mov #1, w2
    record_case 0x013b, w2, w3

    clr w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btsc _conformance_scratch, #15
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x013c, w2, w3

    mov #0x8000, w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btsc.b _conformance_scratch+1, #7
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x013d, w2, w3

    mov #0x8000, w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btss _conformance_scratch, #15
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x013e, w2, w3

    clr w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btss.b _conformance_scratch+1, #7
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x013f, w2, w3

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    clr w2
    set_status 0x010d
    btsc [w4++], #0
    mov #1, w2
    record_case 0x0140, w2, w4

    mov #_conformance_scratch+2, w4
    mov #1, w1
    mov w1, [w4]
    clr w2
    set_status 0x010d
    btss [w4--], #0
    mov #1, w2
    record_case 0x0141, w2, w4

    clr w2
    clr w3
    set_status 0x010d
    btsc w3, #0
    goto bit_goto_taken
    bra bit_goto_done
bit_goto_taken:
    mov #1, w2
bit_goto_done:
    record_case 0x0142, w2, w3

    clr w2
    mov #1, w3
    set_status 0x010d
    btss w3, #0
    call bit_call_taken
    bra bit_call_done
bit_call_taken:
    mov #1, w2
    return
bit_call_done:
    record_case 0x0143, w2, w3

    clr w2
    mov #1, w3
    set_status 0x010d
    btsc w3, #0
    goto bit_goto_executed
    bra bit_goto_executed_done
bit_goto_executed:
    mov #1, w2
bit_goto_executed_done:
    record_case 0x0144, w2, w3

    clr w2
    clr w3
    set_status 0x010d
    btss w3, #0
    call bit_call_executed
    bra bit_call_executed_done
bit_call_executed:
    mov #1, w2
    return
bit_call_executed_done:
    record_case 0x0145, w2, w3

    end_results
