.section .text,code
.include "conformance.inc"

.global _branch_conformance_cases
_branch_conformance_cases = 45
.global _branch_conformance_group_complete
_branch_conformance_group_complete = 1

.macro record_conditional_branch id, condition, status
    set_status \status
    clr w1
    bra \condition, 1f
    bra 2f
1:
    inc w1, w1
2:
    record_case \id, w1, w1
.endm

.macro record_compare_skip id, instruction, left, right
    mov #\left, w3
    mov #\right, w1
    clr w2
    set_status 0x010f
    \instruction w3, w1
    mov #0x1111, w2
    record_case \id, w2, w3
.endm

.global _run_branch_conformance
_run_branch_conformance:
    begin_results

    record_conditional_branch 0x0200, OV, 0x0004
    record_conditional_branch 0x0201, OV, 0x0000
    record_conditional_branch 0x0202, C, 0x0001
    record_conditional_branch 0x0203, C, 0x0000
    record_conditional_branch 0x0204, Z, 0x0002
    record_conditional_branch 0x0205, Z, 0x0000
    record_conditional_branch 0x0206, N, 0x0008
    record_conditional_branch 0x0207, N, 0x0000
    record_conditional_branch 0x0208, LE, 0x0002
    record_conditional_branch 0x0209, LE, 0x0000
    record_conditional_branch 0x020a, LT, 0x0008
    record_conditional_branch 0x020b, LT, 0x0000
    record_conditional_branch 0x020c, LEU, 0x0000
    record_conditional_branch 0x020d, LEU, 0x0001

    set_status 0x0000
    clr w1
    bra 1f
    bra 2f
1:
    inc w1, w1
2:
    record_case 0x020e, w1, w1

    record_conditional_branch 0x020f, NOV, 0x0000
    record_conditional_branch 0x0210, NOV, 0x0004
    record_conditional_branch 0x0211, NC, 0x0000
    record_conditional_branch 0x0212, NC, 0x0001
    record_conditional_branch 0x0213, NZ, 0x0000
    record_conditional_branch 0x0214, NZ, 0x0002
    record_conditional_branch 0x0215, NN, 0x0000
    record_conditional_branch 0x0216, NN, 0x0008
    record_conditional_branch 0x0217, GT, 0x0000
    record_conditional_branch 0x0218, GT, 0x0002
    record_conditional_branch 0x0219, GE, 0x0000
    record_conditional_branch 0x021a, GE, 0x0008
    record_conditional_branch 0x021b, GTU, 0x0001
    record_conditional_branch 0x021c, GTU, 0x0000

    record_compare_skip 0x021d, cpseq, 0x1234, 0x1234
    record_compare_skip 0x021e, cpseq, 0x1234, 0x4321
    record_compare_skip 0x021f, cpseq.b, 0x80ff, 0x7fff
    record_compare_skip 0x0220, cpseq.b, 0x8000, 0x0001
    record_compare_skip 0x0221, cpsne, 0x1234, 0x4321
    record_compare_skip 0x0222, cpsne, 0x1234, 0x1234
    record_compare_skip 0x0223, cpsne.b, 0x80ff, 0x7fff
    record_compare_skip 0x0224, cpsne.b, 0x8000, 0x0001
    record_compare_skip 0x0225, cpsgt, 0x0001, 0xffff
    record_compare_skip 0x0226, cpsgt, 0xffff, 0x0001
    record_compare_skip 0x0227, cpsgt.b, 0x007f, 0x0080
    record_compare_skip 0x0228, cpsgt.b, 0x0080, 0x007f
    record_compare_skip 0x0229, cpslt, 0xffff, 0x0001
    record_compare_skip 0x022a, cpslt, 0x0001, 0xffff
    record_compare_skip 0x022b, cpslt.b, 0x0080, 0x007f
    record_compare_skip 0x022c, cpslt.b, 0x007f, 0x0080

    end_results
