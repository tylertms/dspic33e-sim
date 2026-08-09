.section .text,code
.include "conformance.inc"

.global _sfr_conformance_cases
_sfr_conformance_cases = 191
.global _sfr_conformance_group_complete
_sfr_conformance_group_complete = 0

.macro sfr_case identifier, address
    mov \address, w1
    setm w0
    mov w0, \address
    mov \address, w2
    mov #0x5aa5, w0
    mov w0, \address
    mov \address, w3
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
    mov w1, \address
.endm

.macro gpio_latch_case identifier, port, lat
    mov \lat, w5
    mov #\port, w4
    mov #0xa55a, w0
    mov w0, [w4]
    mov \lat, w1
    mov #0x003c, w0
    mov.b w0, [w4]
    mov \lat, w2
    mov #0x00c3, w0
    mov.b w0, [w4+1]
    mov \lat, w3
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
    mov w5, \lat
.endm

.macro commit_oscillator_low
    mov #0x0742, w1
    mov #0x0046, w2
    mov #0x0057, w3
    mov.b w2, [w1]
    mov.b w3, [w1]
    mov.b w0, [w1]
.endm

.macro gpio_output_case identifier, tris, port, lat
    mov \tris, w4
    mov \lat, w5
    clr \tris
    mov #0x5aa5, w0
    mov w0, \lat
    nop
    mov \port, w1
    mov #0xa55a, w0
    mov w0, \lat
    nop
    mov \port, w2
    clr \lat
    nop
    mov \port, w3
    mov #\identifier, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
    mov w4, \tris
    mov w5, \lat
.endm

.macro sfr_write_case identifier, address
    mov \address, w1
    setm w0
    mov w0, \address
    mov \address, w2
    mov #0x5aa5, w0
    mov w0, \address
    mov \address, w3
    clr w0
    mov w0, \address
    mov \address, w4
    mov #\identifier, w0
    mov w0, [w7++]
    mov w2, [w7++]
    mov w3, [w7++]
    mov w4, [w7++]
    mov w1, \address
.endm

.global _run_sfr_conformance
_run_sfr_conformance:
    begin_results

    sfr_case 0x0e00, 0x0744
    sfr_case 0x0e01, 0x0746
    sfr_case 0x0e02, 0x0748
    sfr_case 0x0e03, 0x074e
    sfr_case 0x0e04, 0x0840
    sfr_case 0x0e05, 0x0842
    sfr_case 0x0e06, 0x0844
    sfr_case 0x0e07, 0x0848
    sfr_case 0x0e08, 0x084c
    sfr_case 0x0e09, 0x084e
    sfr_case 0x0e0a, 0x0850
    sfr_case 0x0e0b, 0x0858
    sfr_case 0x0e0c, 0x085a
    sfr_case 0x0e0d, 0x0862
    sfr_case 0x0e0e, 0x0864
    sfr_case 0x0e0f, 0x0868
    sfr_case 0x0e10, 0x0870
    sfr_case 0x0e11, 0x0e00
    sfr_case 0x0e12, 0x0e04
    sfr_case 0x0e13, 0x0e06
    sfr_case 0x0e14, 0x0e08
    sfr_case 0x0e15, 0x0e0a
    sfr_case 0x0e16, 0x0e0c
    sfr_case 0x0e17, 0x0e0e
    sfr_case 0x0e18, 0x0e10
    sfr_case 0x0e19, 0x0e14
    sfr_case 0x0e1a, 0x0e18
    sfr_case 0x0e1b, 0x0e1a
    sfr_case 0x0e1c, 0x0e1c
    sfr_case 0x0e1d, 0x0e1e
    sfr_case 0x0e1e, 0x0e20
    sfr_case 0x0e1f, 0x0e24
    sfr_case 0x0e20, 0x0e28
    sfr_case 0x0e21, 0x0e2a
    sfr_case 0x0e22, 0x0e2c
    sfr_case 0x0e23, 0x0e2e
    sfr_case 0x0e24, 0x0e30
    sfr_case 0x0e25, 0x0e34
    sfr_case 0x0e26, 0x0e36
    sfr_case 0x0e27, 0x0e38
    sfr_case 0x0e28, 0x0e3a
    sfr_case 0x0e29, 0x0e3c
    sfr_case 0x0e2a, 0x0e3e
    sfr_case 0x0e2b, 0x0e40
    sfr_case 0x0e2c, 0x0e44
    sfr_case 0x0e2d, 0x0e48
    sfr_case 0x0e2e, 0x0e4a
    sfr_case 0x0e2f, 0x0e4c
    sfr_case 0x0e30, 0x0e4e
    sfr_case 0x0e31, 0x0e50
    sfr_case 0x0e32, 0x0e54
    sfr_case 0x0e33, 0x0e56
    sfr_case 0x0e34, 0x0e58
    sfr_case 0x0e35, 0x0e5a
    sfr_case 0x0e36, 0x0e5c
    sfr_case 0x0e37, 0x0e60
    sfr_case 0x0e38, 0x0e64
    sfr_case 0x0e39, 0x0e66
    sfr_case 0x0e3a, 0x0e68
    sfr_case 0x0e3b, 0x0e6a
    sfr_case 0x0e3c, 0x0e6c
    sfr_case 0x0e3d, 0x0e6e

    bclr INTCON2, #15
    nop
    sfr_write_case 0x0e3e, 0x0800
    sfr_write_case 0x0e3f, 0x0802
    sfr_write_case 0x0e40, 0x0804
    sfr_write_case 0x0e41, 0x0806
    sfr_write_case 0x0e42, 0x0808
    sfr_write_case 0x0e43, 0x080a
    sfr_write_case 0x0e44, 0x080c
    sfr_write_case 0x0e45, 0x080e
    sfr_write_case 0x0e46, 0x0810
    sfr_write_case 0x0e47, 0x0820
    sfr_write_case 0x0e48, 0x0822
    sfr_write_case 0x0e49, 0x0824
    sfr_write_case 0x0e4a, 0x0826
    sfr_write_case 0x0e4b, 0x0828
    sfr_write_case 0x0e4c, 0x082a
    sfr_write_case 0x0e4d, 0x082c
    sfr_write_case 0x0e4e, 0x082e
    sfr_write_case 0x0e4f, 0x0830
    sfr_write_case 0x0e50, 0x0846
    sfr_write_case 0x0e51, 0x084a
    sfr_write_case 0x0e52, 0x0852
    sfr_write_case 0x0e53, 0x0854
    sfr_write_case 0x0e54, 0x0856
    sfr_write_case 0x0e55, 0x085c
    sfr_write_case 0x0e56, 0x085e
    sfr_write_case 0x0e57, 0x0860
    sfr_write_case 0x0e58, 0x086a
    sfr_write_case 0x0e59, 0x086c
    sfr_write_case 0x0e5a, 0x086e
    sfr_write_case 0x0e5b, 0x087a
    sfr_write_case 0x0e5c, 0x087c
    sfr_write_case 0x0e5d, 0x087e
    sfr_write_case 0x0e5e, 0x0880
    sfr_write_case 0x0e5f, 0x0882
    sfr_write_case 0x0e60, 0x0884
    sfr_write_case 0x0e61, 0x0886

    mov OSCCON, w0
    bclr w0, #6
    commit_oscillator_low
    sfr_write_case 0x0e62, 0x0680
    sfr_write_case 0x0e63, 0x0682
    sfr_write_case 0x0e64, 0x0684
    sfr_write_case 0x0e65, 0x0686
    sfr_write_case 0x0e66, 0x0688
    sfr_write_case 0x0e67, 0x068a
    sfr_write_case 0x0e68, 0x068c
    sfr_write_case 0x0e69, 0x068e
    sfr_write_case 0x0e6a, 0x0690
    sfr_write_case 0x0e6b, 0x0692
    sfr_write_case 0x0e6c, 0x0696
    sfr_write_case 0x0e6d, 0x0698
    sfr_write_case 0x0e6e, 0x069a
    sfr_write_case 0x0e6f, 0x069c
    sfr_write_case 0x0e70, 0x069e
    sfr_write_case 0x0e71, 0x06a0
    sfr_write_case 0x0e72, 0x06a2
    sfr_write_case 0x0e73, 0x06a4
    sfr_write_case 0x0e74, 0x06a6
    sfr_write_case 0x0e75, 0x06a8
    sfr_write_case 0x0e76, 0x06aa
    sfr_write_case 0x0e77, 0x06ac
    sfr_write_case 0x0e78, 0x06ae
    sfr_write_case 0x0e79, 0x06b0
    sfr_write_case 0x0e7a, 0x06b2
    sfr_write_case 0x0e7b, 0x06b4
    sfr_write_case 0x0e7c, 0x06b6
    sfr_write_case 0x0e7d, 0x06b8
    sfr_write_case 0x0e7e, 0x06ba
    sfr_write_case 0x0e7f, 0x06bc
    sfr_write_case 0x0e80, 0x06be
    sfr_write_case 0x0e81, 0x06c0
    sfr_write_case 0x0e82, 0x06c2
    sfr_write_case 0x0e83, 0x06c4
    sfr_write_case 0x0e84, 0x06c6
    sfr_write_case 0x0e85, 0x06c8
    sfr_write_case 0x0e86, 0x06ca
    sfr_write_case 0x0e87, 0x06ce
    sfr_write_case 0x0e88, 0x06d0
    sfr_write_case 0x0e89, 0x06d2
    sfr_write_case 0x0e8a, 0x06d4
    sfr_write_case 0x0e8b, 0x06d6
    sfr_write_case 0x0e8c, 0x06d8
    sfr_write_case 0x0e8d, 0x06da
    sfr_write_case 0x0e8e, 0x06dc
    sfr_write_case 0x0e8f, 0x06de
    sfr_write_case 0x0e90, 0x06e0
    sfr_write_case 0x0e91, 0x06e2
    sfr_write_case 0x0e92, 0x06e4
    sfr_write_case 0x0e93, 0x06e6
    sfr_write_case 0x0e94, 0x06e8
    sfr_write_case 0x0e95, 0x06ea
    sfr_write_case 0x0e96, 0x06ec
    sfr_write_case 0x0e97, 0x06ee
    sfr_write_case 0x0e98, 0x06f0
    sfr_write_case 0x0e99, 0x06f2
    sfr_write_case 0x0e9a, 0x06f4
    sfr_write_case 0x0e9b, 0x06f6

    clr 0x0680
    mov OSCCON, w0
    bset w0, #6
    commit_oscillator_low
    setm w0
    mov w0, 0x0680
    mov 0x0680, w1
    mov #0x0e9c, w0
    mov w0, [w7++]
    mov w1, [w7++]
    mov w1, [w7++]
    mov w1, [w7++]

    sfr_write_case 0x0e9d, 0x0020
    sfr_write_case 0x0e9e, 0x0032
    sfr_write_case 0x0e9f, 0x0034
    sfr_write_case 0x0ea0, 0x0036
    sfr_write_case 0x0ea1, 0x0038
    sfr_write_case 0x0ea2, 0x003a
    sfr_write_case 0x0ea3, 0x003c
    sfr_write_case 0x0ea4, 0x003e
    sfr_write_case 0x0ea5, 0x0040
    sfr_write_case 0x0ea6, 0x0046
    sfr_write_case 0x0ea7, 0x0048
    sfr_write_case 0x0ea8, 0x004a
    sfr_write_case 0x0ea9, 0x004c
    sfr_write_case 0x0eaa, 0x004e
    sfr_write_case 0x0efc, 0x0740
    sfr_write_case 0x0efe, 0x072c
    sfr_write_case 0x0eab, 0x0050
    sfr_write_case 0x0eac, 0x0054
    sfr_write_case 0x0ead, 0x075a
    sfr_write_case 0x0eae, 0x0760
    sfr_write_case 0x0eaf, 0x0762
    sfr_write_case 0x0eb0, 0x0764
    sfr_write_case 0x0eb1, 0x0766
    sfr_write_case 0x0eb2, 0x0768
    sfr_write_case 0x0eb3, 0x076a
    sfr_write_case 0x0eb4, 0x076c
    gpio_output_case 0x0eb5, 0x0e00, 0x0e02, 0x0e04
    gpio_output_case 0x0eb6, 0x0e10, 0x0e12, 0x0e14
    gpio_output_case 0x0eb7, 0x0e20, 0x0e22, 0x0e24
    gpio_output_case 0x0eb8, 0x0e30, 0x0e32, 0x0e34
    gpio_output_case 0x0eb9, 0x0e40, 0x0e42, 0x0e44
    gpio_output_case 0x0eba, 0x0e50, 0x0e52, 0x0e54
    gpio_output_case 0x0ebb, 0x0e60, 0x0e62, 0x0e64
    gpio_latch_case 0x0efd, 0x0e32, 0x0e34

    end_results
