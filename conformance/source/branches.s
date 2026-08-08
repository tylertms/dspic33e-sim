.section .text,code
.include "conformance.inc"

.global _branch_conformance_cases
_branch_conformance_cases = 29

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

    end_results
