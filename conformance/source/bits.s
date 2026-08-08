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

    mutate_register 0x0600, bset, 0x0000, 0
    mutate_register 0x0601, bset, 0x0000, 15
    mutate_register 0x0602, bset.b, 0xa500, 7

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010f
    bset [w4], #8
    mov [w4], w2
    record_case 0x0603, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010f
    bset [w4++], #9
    mov [w4-2], w2
    record_case 0x0604, w2, w4

    mov #_conformance_scratch+1, w4
    clr w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bset.b [w4--], #7
    mov _conformance_scratch, w2
    record_case 0x0605, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4+2]
    set_status 0x010f
    bset [++w4], #10
    mov [w4], w2
    record_case 0x0606, w2, w4

    mov #_conformance_scratch+1, w4
    clr w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bset.b [--w4], #6
    mov [w4], w2
    record_case 0x0607, w2, w4

    mutate_file 0x0608, bset, 0x0000, _conformance_scratch, 15
    mutate_file 0x0609, bset.b, 0x0000, _conformance_scratch+1, 7

    mutate_register 0x060a, bclr, 0xffff, 0
    mutate_register 0x060b, bclr, 0xffff, 15
    mutate_register 0x060c, bclr.b, 0xa5ff, 7

    mov #_conformance_scratch, w4
    mov #0xffff, w1
    mov w1, [w4]
    set_status 0x010f
    bclr [w4], #8
    mov [w4], w2
    record_case 0x060d, w2, w4

    mov #_conformance_scratch, w4
    mov #0xffff, w1
    mov w1, [w4]
    set_status 0x010f
    bclr [w4++], #9
    mov [w4-2], w2
    record_case 0x060e, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0xffff, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bclr.b [w4--], #7
    mov _conformance_scratch, w2
    record_case 0x060f, w2, w4

    mov #_conformance_scratch, w4
    mov #0xffff, w1
    mov w1, [w4+2]
    set_status 0x010f
    bclr [++w4], #10
    mov [w4], w2
    record_case 0x0610, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0xffff, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    bclr.b [--w4], #6
    mov [w4], w2
    record_case 0x0611, w2, w4

    mutate_file 0x0612, bclr, 0xffff, _conformance_scratch, 15
    mutate_file 0x0613, bclr.b, 0xffff, _conformance_scratch+1, 7

    mutate_register 0x0614, btg, 0x0000, 0
    mutate_register 0x0615, btg, 0x8000, 15
    mutate_register 0x0616, btg.b, 0xa580, 7

    mov #_conformance_scratch, w4
    mov #0x0100, w1
    mov w1, [w4]
    set_status 0x010f
    btg [w4], #8
    mov [w4], w2
    record_case 0x0617, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010f
    btg [w4++], #9
    mov [w4-2], w2
    record_case 0x0618, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0x8000, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    btg.b [w4--], #7
    mov _conformance_scratch, w2
    record_case 0x0619, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4+2]
    set_status 0x010f
    btg [++w4], #10
    mov [w4], w2
    record_case 0x061a, w2, w4

    mov #_conformance_scratch+1, w4
    mov #0x0040, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    btg.b [--w4], #6
    mov [w4], w2
    record_case 0x061b, w2, w4

    mutate_file 0x061c, btg, 0x0000, _conformance_scratch, 15
    mutate_file 0x061d, btg.b, 0x8000, _conformance_scratch+1, 7

    mov #0x8000, w2
    set_status 0x010e
    btst.c w2, #15
    record_case 0x061e, w2, w2

    clr w2
    set_status 0x010f
    btst.c w2, #15
    record_case 0x061f, w2, w2

    mov #0x8000, w2
    set_status 0x010f
    btst.z w2, #15
    record_case 0x0620, w2, w2

    clr w2
    set_status 0x010d
    btst.z w2, #15
    record_case 0x0621, w2, w2

    mov #_conformance_scratch, w4
    mov #0x0008, w1
    mov w1, [w4]
    set_status 0x010e
    btst.c [w4], #3
    mov [w4], w2
    record_case 0x0622, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010d
    btst.z [w4++], #3
    mov [w4-2], w2
    record_case 0x0623, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x0008, w1
    mov w1, [w4]
    set_status 0x010e
    btst.c [w4--], #3
    mov [w4+2], w2
    record_case 0x0624, w2, w4

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4+2]
    set_status 0x010d
    btst.z [++w4], #3
    mov [w4], w2
    record_case 0x0625, w2, w4

    mov #_conformance_scratch+2, w4
    mov #0x0008, w1
    mov w1, [w4-2]
    set_status 0x010e
    btst.c [--w4], #3
    mov [w4], w2
    record_case 0x0626, w2, w4

    mov #0x0008, w2
    mov #0xfff3, w3
    set_status 0x010e
    btst.c w2, w3
    record_case 0x0627, w2, w3

    clr w2
    mov #0xfff3, w3
    set_status 0x010f
    btst.c w2, w3
    record_case 0x0628, w2, w3

    mov #0x0008, w2
    mov #0xfff3, w3
    set_status 0x010f
    btst.z w2, w3
    record_case 0x0629, w2, w3

    clr w2
    mov #0xfff3, w3
    set_status 0x010d
    btst.z w2, w3
    record_case 0x062a, w2, w3

    mov #_conformance_scratch, w4
    mov #0x0008, w1
    mov w1, [w4]
    mov #0x0013, w3
    set_status 0x010f
    btst.z [w4], w3
    mov [w4], w2
    record_case 0x062b, w2, w3

    clr w2
    set_status 0x010e
    btsts.c w2, #5
    record_case 0x062c, w2, w2

    mov #0x0020, w2
    set_status 0x010e
    btsts.c w2, #5
    record_case 0x062d, w2, w2

    clr w2
    set_status 0x010d
    btsts.z w2, #5
    record_case 0x062e, w2, w2

    mov #0x0020, w2
    set_status 0x010f
    btsts.z w2, #5
    record_case 0x062f, w2, w2

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    set_status 0x010e
    btsts.c [w4], #5
    mov [w4], w2
    record_case 0x0630, w2, w4

    mov #_conformance_scratch, w4
    mov #0x0020, w1
    mov w1, [w4]
    set_status 0x010f
    btsts.z [w4++], #5
    mov [w4-2], w2
    record_case 0x0631, w2, w4

    mov #_conformance_scratch+2, w4
    clr w1
    mov w1, [w4]
    set_status 0x010e
    btsts.c [w4--], #5
    mov [w4+2], w2
    record_case 0x0632, w2, w4

    mov #_conformance_scratch, w4
    mov #0x0020, w1
    mov w1, [w4+2]
    set_status 0x010f
    btsts.z [++w4], #5
    mov [w4], w2
    record_case 0x0633, w2, w4

    mov #_conformance_scratch+2, w4
    clr w1
    mov w1, [w4-2]
    set_status 0x010e
    btsts.c [--w4], #5
    mov [w4], w2
    record_case 0x0634, w2, w4

    clr w1
    mov w1, _conformance_scratch
    set_status 0x010d
    btsts _conformance_scratch, #15
    mov _conformance_scratch, w2
    record_case 0x0635, w2, w2

    clr w1
    mov w1, _conformance_scratch
    set_status 0x010d
    btsts.b _conformance_scratch+1, #7
    mov _conformance_scratch, w2
    record_case 0x0636, w2, w2

    mov #0x8000, w1
    mov w1, _conformance_scratch
    set_status 0x010f
    btsts.b _conformance_scratch+1, #7
    mov _conformance_scratch, w2
    record_case 0x0637, w2, w2

    clr w2
    clr w3
    set_status 0x010d
    btsc w3, #0
    mov #1, w2
    record_case 0x0638, w2, w3

    clr w2
    mov #1, w3
    set_status 0x010d
    btsc w3, #0
    mov #1, w2
    record_case 0x0639, w2, w3

    clr w2
    mov #1, w3
    set_status 0x010d
    btss w3, #0
    mov #1, w2
    record_case 0x063a, w2, w3

    clr w2
    clr w3
    set_status 0x010d
    btss w3, #0
    mov #1, w2
    record_case 0x063b, w2, w3

    clr w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btsc _conformance_scratch, #15
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x063c, w2, w3

    mov #0x8000, w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btsc.b _conformance_scratch+1, #7
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x063d, w2, w3

    mov #0x8000, w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btss _conformance_scratch, #15
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x063e, w2, w3

    clr w1
    mov w1, _conformance_scratch
    clr w2
    set_status 0x010d
    btss.b _conformance_scratch+1, #7
    mov #1, w2
    mov _conformance_scratch, w3
    record_case 0x063f, w2, w3

    mov #_conformance_scratch, w4
    clr w1
    mov w1, [w4]
    clr w2
    set_status 0x010d
    btsc [w4++], #0
    mov #1, w2
    record_case 0x0640, w2, w4

    mov #_conformance_scratch+2, w4
    mov #1, w1
    mov w1, [w4]
    clr w2
    set_status 0x010d
    btss [w4--], #0
    mov #1, w2
    record_case 0x0641, w2, w4

    clr w2
    clr w3
    set_status 0x010d
    btsc w3, #0
    goto bit_goto_taken
    bra bit_goto_done
bit_goto_taken:
    mov #1, w2
bit_goto_done:
    record_case 0x0642, w2, w3

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
    record_case 0x0643, w2, w3

    clr w2
    mov #1, w3
    set_status 0x010d
    btsc w3, #0
    goto bit_goto_executed
    bra bit_goto_executed_done
bit_goto_executed:
    mov #1, w2
bit_goto_executed_done:
    record_case 0x0644, w2, w3

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
    record_case 0x0645, w2, w3

    end_results
