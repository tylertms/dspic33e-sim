import argparse
import gzip
import hashlib
import json
import re
import subprocess
import sys
import time
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]

SIMULATOR_SOURCE = PROJECT_ROOT

CONFORMANCE_SOURCE = SIMULATOR_SOURCE / "conformance"

SIMULATOR_BUILD = PROJECT_ROOT / "build" / "simulator"

CONFORMANCE_BUILD = PROJECT_ROOT / "build" / "conformance-firmware"

SIMULATOR = SIMULATOR_BUILD / "dspic33_firmware_runner.exe"

EVENT_CONFORMANCE = SIMULATOR_BUILD / "dspic33_event_conformance.exe"

PROCESSOR_CONFORMANCE = SIMULATOR_BUILD / "dspic33_processor_conformance.exe"

DMA_CONFORMANCE = SIMULATOR_BUILD / "dspic33_dma_conformance.exe"

TIMER_CONFORMANCE = SIMULATOR_BUILD / "dspic33_timer_conformance.exe"

ADC_CONFORMANCE = SIMULATOR_BUILD / "dspic33_adc_conformance.exe"

PWM_CONFORMANCE = SIMULATOR_BUILD / "dspic33_pwm_conformance.exe"

SPI_CONFORMANCE = SIMULATOR_BUILD / "dspic33_spi_conformance.exe"

CAN_CONFORMANCE = SIMULATOR_BUILD / "dspic33_can_conformance.exe"

USB_CONFORMANCE = SIMULATOR_BUILD / "dspic33_usb_conformance.exe"

UART_CONFORMANCE = SIMULATOR_BUILD / "dspic33_uart_conformance.exe"

I2C_CONFORMANCE = SIMULATOR_BUILD / "dspic33_i2c_conformance.exe"

NVM_CONFORMANCE = SIMULATOR_BUILD / "dspic33_nvm_conformance.exe"

CRC_CONFORMANCE = SIMULATOR_BUILD / "dspic33_crc_conformance.exe"

PMP_CONFORMANCE = SIMULATOR_BUILD / "dspic33_pmp_conformance.exe"

INPUT_CAPTURE_CONFORMANCE = (
    SIMULATOR_BUILD / "dspic33_input_capture_conformance.exe"
)

OUTPUT_COMPARE_CONFORMANCE = (
    SIMULATOR_BUILD / "dspic33_output_compare_conformance.exe"
)

QEI_CONFORMANCE = SIMULATOR_BUILD / "dspic33_qei_conformance.exe"

DCI_CONFORMANCE = SIMULATOR_BUILD / "dspic33_dci_conformance.exe"

GPIO_CONFORMANCE = SIMULATOR_BUILD / "dspic33_gpio_conformance.exe"

PPS_CONFORMANCE = SIMULATOR_BUILD / "dspic33_pps_conformance.exe"

AUXILIARY_CLOCK_CONFORMANCE = (
    SIMULATOR_BUILD / "dspic33_auxiliary_clock_conformance.exe"
)

OSCILLATOR_CONFORMANCE = (
    SIMULATOR_BUILD / "dspic33_oscillator_conformance.exe"
)

WATCHDOG_CONFORMANCE = SIMULATOR_BUILD / "dspic33_watchdog_conformance.exe"

INTERRUPT_CONTROL_CONFORMANCE = (
    SIMULATOR_BUILD / "dspic33_interrupt_control_conformance.exe"
)

CORE_SFR_CONFORMANCE = SIMULATOR_BUILD / "dspic33_core_sfr_conformance.exe"

RESET_CONFORMANCE = SIMULATOR_BUILD / "dspic33_reset_conformance.exe"

COMPARATOR_CONFORMANCE = (
    SIMULATOR_BUILD / "dspic33_comparator_conformance.exe"
)

RTCC_CONFORMANCE = SIMULATOR_BUILD / "dspic33_rtcc_conformance.exe"

SFR_RESET_CONFORMANCE = SIMULATOR_BUILD / "dspic33_sfr_reset_conformance.exe"

SFR_ACCESS_CONFORMANCE = (
    SIMULATOR_BUILD / "dspic33_sfr_access_conformance.exe"
)

SFR_MANIFEST_VERIFIER = SIMULATOR_SOURCE / "tools" / "verify_sfr_manifest.py"

SFR_ACCESS_GENERATOR = (
    SIMULATOR_SOURCE / "tools" / "generate_sfr_access_expectations.py"
)

SFR_MANIFEST = SIMULATOR_SOURCE / "generated" / "dspic33ep512mu810_sfr_manifest.json.gz"

SFR_ACCESS_EXPECTED_INVENTORY = {
    "definitions": 993,
    "addresses": 977,
    "aliases": 7,
    "mux-defaults": 16,
    "mux-alternates": 16,
    "normal-bits": 9894,
    "read-only-bits": 2007,
    "reserved-bits": 2928,
    "write-only-bits": 158,
    "side-effect-bits": 540,
    "split-access-addresses": 9,
    "split-access-bits": 98,
    "dependent-normal-addresses": 1,
    "dependent-normal-bits": 1,
    "protected-addresses": 1,
    "protected-normal-bits": 5,
    "protected-set-only-bits": 1,
    "alternate-normal-bits": 244,
    "alternate-read-only-bits": 0,
    "alternate-reserved-bits": 12,
    "alternate-write-only-bits": 0,
    "alternate-side-effect-bits": 0,
}

SFR_ACCESS_EXPECTED_UNRESOLVED = {
    "unresolved-addresses": 0,
    "normal-addresses": 0,
    "normal-bits": 0,
    "read-only-addresses": 0,
    "read-only-bits": 0,
    "reserved-addresses": 0,
    "reserved-bits": 0,
    "write-only-addresses": 0,
    "write-only-bits": 0,
}

SFR_CONDITIONAL_EXPECTED_SUMMARY = {
    "addresses": 68,
    "normal-bits": 1024,
    "read-only-bits": 0,
    "reserved-bits": 64,
    "write-only-bits": 0,
    "side-effect-bits": 0,
    "unresolved-addresses": 0,
    "selector-reset-addresses": 0,
    "selector-reset-bits": 0,
    "selector-switch-addresses": 0,
    "selector-switch-bits": 0,
    "absence-addresses": 0,
    "isolation-addresses": 0,
    "access-unresolved-addresses": 0,
    "access-normal-bits": 0,
    "access-read-only-bits": 0,
    "access-reserved-bits": 0,
    "access-write-only-bits": 0,
}

SFR_DEPENDENT_NORMAL_EXPECTED_SUMMARY = {
    "addresses": 1,
    "normal-write-failures": 0,
    "restricted-read-failures": 0,
    "restricted-write-failures": 0,
    "resurrection-failures": 0,
    "failures": 0,
}

SFR_PROTECTED_EXPECTED_SUMMARY = {
    "addresses": 1,
    "direct-set-failures": 0,
    "direct-clear-failures": 0,
    "failures": 0,
}

SFR_ACCESS_CLASSES = ("normal", "read-only", "reserved", "write-only")

SFR_MAP_EXPECTED_SUMMARY = {
    "words": 2048,
    "implemented": 977,
    "absent": 1071,
    "absent-ranges": 77,
    "direct-byte-checks": 2142,
    "direct-word-checks": 1071,
    "internal-pad-byte-checks": 2142,
    "internal-pad-word-checks": 1071,
    "odd-crossing-checks": 10,
    "lifecycle-checks": 5,
    "failures": 0,
}

SFR_POR_DYNAMIC_EXCLUSIONS = {
    0x0742: (0x7020, "DS70616G-register-9-1-current-clock-status"),
}

SFR_MCLR_DYNAMIC_EXCLUSIONS = {
    0x0620: (0xFFFF, "DS70602B-section-8.4-RTCC-POR-only"),
    0x0622: (0xFFFF, "DS70602B-section-8.4-RTCC-POR-only"),
    0x0624: (0xFFFF, "DS70602B-section-8.4-RTCC-POR-only"),
    0x0626: (0xFFFF, "DS70602B-section-8.4-RTCC-POR-only"),
    0x0728: (0xFFFF, "DS70000609E-NVMCON-RTSP-reset-lifecycle"),
    0x0740: (0xFFFF, "DS70602B-table-8-2-reset-source-status"),
    0x0742: (0xFFFF, "DS70602B-section-8.4-oscillator-POR-only"),
    0x0744: (0xFFFF, "DS70602B-section-8.4-oscillator-POR-only"),
    0x0746: (0xFFFF, "DS70602B-section-8.4-oscillator-POR-only"),
    0x0748: (0xFFFF, "DS70602B-section-8.4-oscillator-POR-only"),
    0x0758: (0xFFFF, "DS70602B-section-8.4-oscillator-POR-only"),
    0x075A: (0xFFFF, "DS70602B-section-8.4-oscillator-POR-only"),
}

BUILT_CONFORMANCE_IMAGE = CONFORMANCE_BUILD / "dspic33-conformance.bin"

CONFORMANCE_IMAGE = CONFORMANCE_SOURCE / "reference-image.bin"

EXTERNAL_ORACLE = CONFORMANCE_SOURCE / "reference-oracle.json"

ORACLE_EVIDENCE_LEDGER = CONFORMANCE_BUILD / "oracle-evidence.json"

EXPECTED_ORACLE_CLASSES = {
    "documented": 96,
    "undefined": 20,
    "external-oracle-limitation": 14,
}

EXPECTED_ADDRESS_ORACLE_CLASSES = {
    "documented": 96,
    "undefined": 20,
    "external-oracle-limitation": 12,
}

OBJDUMP = Path("C:/Program Files (x86)/Microchip/xc16/v1.35/bin/xc16-objdump.exe")

CPU_CONFORMANCE_GROUPS = (
    "arithmetic",
    "bit",
    "branch",
    "divide",
    "extension",
    "loop",
    "move",
    "multiply",
    "shift",
    "stack",
    "system",
    "table",
)

NATIVE_COMPLETE_CPU_GROUPS = (
    "arithmetic",
    "bit",
    "branch",
    "divide",
    "extension",
    "loop",
    "move",
    "multiply",
    "shift",
    "stack",
    "system",
    "table",
)

DEVICE_CONFORMANCE_GROUPS = (
    "interrupt",
    "event",
    "sfr",
    "reset",
    "dma",
    "timer",
    "adc",
    "pwm",
    "spi",
    "can",
    "usb",
    "uart",
    "i2c",
    "nvm",
    "input_capture",
    "output_compare",
    "qei",
    "dci",
    "gpio",
    "pmp",
    "crc",
    "rtcc",
    "comparator",
    "pps",
    "auxiliary_clock",
    "oscillator",
    "watchdog",
)

TARGET_ABSENT_DEVICE_GROUPS = ("ptg",)

NATIVE_ACTIVE_DEVICE_GROUPS = (
    "event",
    "i2c",
    "nvm",
    "input_capture",
    "output_compare",
    "qei",
    "dci",
    "gpio",
    "comparator",
    "crc",
    "pmp",
    "rtcc",
    "auxiliary_clock",
    "oscillator",
    "pps",
    "reset",
    "uart",
    "watchdog",
)

NATIVE_COMPLETE_DEVICE_GROUPS = (
    "auxiliary_clock",
    "adc",
    "can",
    "comparator",
    "crc",
    "dci",
    "dma",
    "event",
    "gpio",
    "i2c",
    "input_capture",
    "interrupt",
    "nvm",
    "oscillator",
    "output_compare",
    "pmp",
    "pps",
    "pwm",
    "qei",
    "reset",
    "rtcc",
    "sfr",
    "spi",
    "timer",
    "uart",
    "usb",
    "watchdog",
)

DATASHEET_STATUS_OVERRIDES = {
    0x032C: 0x0100,
    0x032E: 0x0100,
    0x032F: 0x0100,
    0x0330: 0x0000,
    0x0331: 0x0000,
    0x0332: 0x0000,
    0x0333: 0x0100,
    0x0334: 0x0100,
    0x0335: 0x0100,
    0x0336: 0x0000,
    0x0337: 0x0000,
    0x0338: 0x0000,
    0x03A2: 0x0100,
    0x03A8: 0x0100,
}

DATASHEET_RECORD_WORD_OVERRIDES = {
    0x023D: {1: 0x0001, 2: 0x8800, 3: 0x0001},
    0x023F: {1: 0x0001, 2: 0x4800, 3: 0x0001},
    0x0241: {1: 0x0001, 2: 0x2400, 3: 0x0001},
    0x0243: {1: 0x0001, 2: 0x1400, 3: 0x0001},
    0x0618: {1: 0x0200, 2: 0x010F, 3: 0x1002},
    0x0619: {1: 0x0000, 2: 0x010F, 3: 0x1000},
    0x0631: {1: 0x0020, 2: 0x010D, 3: 0x1002},
    0x0632: {1: 0x0020, 2: 0x010E, 3: 0x1000},
    0x0E00: {3: 0x7A85},
    0x0E03: {3: 0x1F00},
    0x071E: {3: 0x0001},
    0x074E: {2: 0xFFFE},
    0x0759: {1: 0x2400, 2: 0x2400, 3: 0x2400},
    0x077A: {2: 0x5C00},
    0x077B: {2: 0x5C00},
    0x077D: {2: 0xAC00},
    0x077E: {2: 0xAC00},
    0x077F: {2: 0xAC00},
    0x0780: {1: 0xAC00, 2: 0xAC00, 3: 0xAC00},
    0x078C: {2: 0x0000},
    0x078D: {2: 0x0000},
    0x078F: {2: 0x0000},
    0x0790: {2: 0x0000},
    0x0791: {2: 0xFFFE},
    0x0792: {2: 0x8800},
    0x0793: {2: 0x8800},
    0x0795: {2: 0x0000},
    0x0796: {2: 0x0000},
    0x07A5: {2: 0xFFFC},
    0x07A7: {2: 0x4002},
    0x082D: {1: 0x7FFE, 2: 0x8002, 3: 0x0108},
    0x0912: {1: 0x0000, 2: 0x0000, 3: 0x0103},
    0x0913: {1: 0x0000, 2: 0x0000, 3: 0x0103},
    0x0918: {1: 0x0300, 3: 0x0300},
    0x091A: {1: 0x0120, 3: 0x0120},
    0x091B: {1: 0x0220, 3: 0x0220},
    0x091C: {1: 0x0120, 3: 0x0120},
    0x091D: {1: 0x0000, 2: 0x0000, 3: 0x0103},
    0x0A09: {2: 0x0000, 3: 0x0103},
    0x0A16: {1: 0x0020, 3: 0x0020},
    0x0D0D: {1: 0x0001},
    0x0D1B: {1: 0x0508, 3: 0x0508},
    0x0E31: {1: 0x317F, 2: 0x317F},
    0x0E32: {2: 0x317F},
    0x0E33: {2: 0x317F},
    0x0E34: {2: 0x317F},
    0x0E35: {2: 0x317F},
    0x0E36: {2: 0x317F},
    0x0E4F: {1: 0x7FDF, 2: 0x5A85},
    0x0EBA: {2: 0x215A},
    0x0EAD: {1: 0x0007, 2: 0x0005},
    0x0EA2: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x0EA3: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x0EFC: {1: 0xC3FF, 2: 0x42A5, 3: 0x0000},
    0x1016: {2: 0x000F, 3: 0x000F},
    0x1100: {1: 0xA076, 2: 0x0024},
    0x1101: {1: 0xA07A},
    0x1102: {1: 0xA072},
    0x1103: {1: 0xA07A},
    0x1104: {1: 0xA072},
    0x1105: {1: 0xA07A},
    0x1106: {1: 0xA072},
    0x1107: {1: 0xA07A},
    0x1108: {1: 0xA072},
    0x1116: {1: 0x0003, 2: 0x0006},
    0x1117: {1: 0x0003, 2: 0x0006},
    0x1118: {1: 0x0003, 2: 0x0006},
    0x1207: {1: 0xE73F},
    0x120F: {1: 0x8001, 3: 0x2000},
    0x1210: {1: 0x8001, 3: 0x0020},
    0x1300: {1: 0x2FFF},
    0x1305: {1: 0x0FFF, 2: 0x0AA5},
    0x1403: {1: 0x0000},
    0x1408: {1: 0x0000},
    0x140D: {1: 0x0000},
    0x1412: {1: 0x0000},
    0x1414: {1: 0x8001, 2: 0x0400, 3: 0x8000},
    0x1415: {1: 0x8001, 2: 0x0002, 3: 0x8000},
    0x1416: {1: 0x8001, 2: 0x0800, 3: 0x8000},
    0x1417: {1: 0x8001, 2: 0x0800, 3: 0x8000},
    0x1503: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x1506: {1: 0x00EF},
    0x1507: {1: 0x0000, 2: 0x0000},
    0x1516: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x1519: {1: 0x00EF},
    0x151A: {1: 0x0000, 2: 0x0000},
    0x1600: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x1604: {1: 0x0013, 2: 0x0001, 3: 0x0000},
    0x1605: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x1606: {1: 0x00BF, 2: 0x00A5, 3: 0x0000},
    0x1607: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x1609: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x160D: {1: 0x0000, 2: 0x0000, 3: 0x0000},
    0x160E: {1: 0x0000, 2: 0x0000, 3: 0x0000},
}

DATASHEET_STATUS_IGNORE_MASKS = {
    0x0802: 0x0001,
    0x0803: 0x0001,
    0x0805: 0x0001,
    0x0806: 0x0001,
    0x0807: 0x0001,
    0x080B: 0x0001,
    0x080D: 0x0001,
    0x0810: 0x0001,
    0x0811: 0x0001,
    0x0814: 0x0005,
    0x0815: 0x0001,
    0x0817: 0x0001,
    0x081A: 0x0001,
    0x081B: 0x0001,
    0x081C: 0x0001,
    0x081E: 0x0001,
    0x081F: 0x0001,
    0x0823: 0x0001,
    0x0826: 0x0001,
    0x082C: 0x0001,
}

DATASHEET_STATUS_SET_BITS = {
    0x080D: 0x0002,
    0x0815: 0x0002,
}

DOCUMENTED_ORACLE_EVIDENCE = {
    0x032C: ("70000157g.pdf ADD instruction", "DC after ADD W1,#31,W2"),
    0x032E: ("70000157g.pdf ADD instruction", "DC after ADD W1,W2,W3"),
    0x032F: ("70000157g.pdf ADD instruction", "DC after ADD #31,W1"),
    0x0330: ("70000157g.pdf ADD instruction", "DC after ADD W1,W2,W3"),
    0x0331: ("70000157g.pdf ADD instruction", "DC after ADD #1,W1"),
    0x0332: ("70000157g.pdf ADD instruction", "DC after ADD W1,#1,W2"),
    0x0333: ("70000157g.pdf SUB instruction", "DC after SUB W1,W2,W3"),
    0x0334: ("70000157g.pdf SUB instruction", "DC after SUB #1,W1"),
    0x0335: ("70000157g.pdf SUB instruction", "DC after SUB W1,#1,W2"),
    0x0336: ("70000157g.pdf SUB instruction", "DC after underflowing SUB W1,W2,W3"),
    0x0337: ("70000157g.pdf SUB instruction", "DC after underflowing SUB #16,W1"),
    0x0338: ("70000157g.pdf SUB instruction", "DC after underflowing SUB W1,#16,W2"),
    0x03A2: (
        "70000157g.pdf ADD literal instruction",
        "DC and predecrement destination after ADD W2,#31,[--W5]",
    ),
    0x03A8: (
        "70000157g.pdf SUBB literal instruction",
        "DC and predecrement destination after SUBB W2,#31,[--W5]",
    ),
    0x0618: (
        "70000157g.pdf BTG instruction",
        "word toggle result and postincrement after BTG [W4++],#9",
    ),
    0x0619: (
        "70000157g.pdf BTG.B instruction",
        "byte toggle result and postdecrement after BTG.B [W4--],#7",
    ),
    0x0631: (
        "70000157g.pdf BTSTS.Z instruction",
        "Z result and postincrement after BTSTS.Z [W4++],#5",
    ),
    0x0632: (
        "70000157g.pdf BTSTS.C instruction",
        "C result and postdecrement after BTSTS.C [W4--],#5",
    ),
    0x071E: (
        "70000157g.pdf MUL.UU instruction",
        "unsigned W2 by W3 product in accumulator B",
    ),
    0x074E: (
        "70000157g.pdf MPY instruction",
        "signed fractional W4 square product in accumulator A",
    ),
    0x0759: (
        "70000157g.pdf MPY instruction",
        "MPY preserves the documented SR fields",
    ),
    0x077A: (
        "70000157g.pdf CLR accumulator instruction",
        "X prefetch value after CLR A with writeback",
    ),
    0x077B: (
        "70000157g.pdf CLR accumulator instruction",
        "unselected writeback register after CLR A with X prefetch",
    ),
    0x077D: (
        "70000157g.pdf CLR accumulator instruction",
        "X prefetch value and pointer after CLR B",
    ),
    0x077E: (
        "70000157g.pdf CLR accumulator instruction",
        "Y prefetch value and pointer after CLR B",
    ),
    0x077F: (
        "70000157g.pdf CLR accumulator instruction",
        "destination writeback and postincrement after CLR B",
    ),
    0x0780: (
        "70000157g.pdf CLR accumulator instruction",
        "CLR B preserves the documented SR fields",
    ),
    0x078C: (
        "70000157g.pdf ED instruction",
        "X prefetch value and postincrement after ED",
    ),
    0x078D: (
        "70000157g.pdf ED instruction",
        "Y prefetch pointer after ED",
    ),
    0x078F: (
        "70000157g.pdf ED instruction",
        "X prefetch value and pointer after ED to accumulator B",
    ),
    0x0790: (
        "70000157g.pdf ED instruction",
        "indexed Y prefetch pointer after ED to accumulator B",
    ),
    0x0791: (
        "70000157g.pdf EDAC instruction",
        "EDAC W6 square result in accumulator A",
    ),
    0x0792: (
        "70000157g.pdf EDAC instruction",
        "EDAC X prefetch value and pointer",
    ),
    0x0793: (
        "70000157g.pdf EDAC instruction",
        "EDAC Y prefetch pointer",
    ),
    0x0795: (
        "70000157g.pdf EDAC instruction",
        "EDAC W7 square X prefetch and pointer",
    ),
    0x0796: (
        "70000157g.pdf EDAC instruction",
        "EDAC W7 square Y prefetch pointer",
    ),
    0x07A5: (
        "70000157g.pdf MPY square-product encoding",
        "opcode F00111 accumulator A product",
    ),
    0x07A7: (
        "70000157g.pdf MPY square-product encoding",
        "opcode F20111 accumulator A product",
    ),
    0x082D: (
        "70000157g.pdf DIVF instruction",
        "fractional quotient and documented status for DIVF W0,W1",
    ),
    0x0912: (
        "70000157g.pdf DO instruction DOSTART definition",
        "DOSTART address after DO #0",
    ),
    0x0913: (
        "70000157g.pdf DO instruction DOEND definition",
        "DOEND address after DO #0",
    ),
    0x0918: (
        "70000157g.pdf DO instruction RA status definition",
        "RA while a zero-count DO loop is active",
    ),
    0x091A: (
        "70000157g.pdf DO instruction CORCON definition",
        "DO loop activity reflected in CORCON",
    ),
    0x091B: (
        "70000157g.pdf nested DO instruction definition",
        "outer loop state while an inner DO loop completes",
    ),
    0x091C: (
        "70000157g.pdf nested DO instruction definition",
        "restored outer loop state after inner DO completion",
    ),
    0x091D: (
        "S2.pdf section 2.9.2.3 DO stack",
        "restore DOSTART and DOEND after a nested loop completes",
    ),
    0x0A09: (
        "70000157g.pdf PUSH.D and POP.D instructions",
        "W14 source pair and stack-pointer result",
    ),
    0x0A16: (
        "70000157g.pdf CORCON SFA definition",
        "software write cannot set hardware-owned SFA",
    ),
    0x0D0D: (
        "DS70000600E DISI and GIE definitions",
        "GIE release after the final DISI-protected instruction",
    ),
    0x0D1B: (
        "DS70000600E INTTREG register definition",
        "implemented INTTREG fields after all-ones write",
    ),
    0x0EA2: (
        "70616g.pdf CPU register map and S2.pdf section 2.9.2.2",
        "DOSTARTL is software read-only",
    ),
    0x0EA3: (
        "70616g.pdf CPU register map and S2.pdf section 2.9.2.2",
        "DOSTARTH is software read-only",
    ),
    0x0E00: (
        "70616g.pdf register 9-2 CLKDIV notes 2 and 3",
        "retain DOZE while DOZEN is set and mask unimplemented bits",
    ),
    0x0E03: (
        "70580C.pdf register 7-7 REFOCON note 1",
        "retain RODIV while reference-clock output is enabled",
    ),
    0x0E4F: (
        "70616g.pdf IEC8 register map",
        "mask unimplemented IEC8 bits",
    ),
    0x0E31: (
        "DS80000526H data sheet clarification 6 PORTF",
        "implement RF6 in TRISF",
    ),
    0x0E32: (
        "DS80000526H data sheet clarification 6 PORTF",
        "implement RF6 in LATF",
    ),
    0x0E33: (
        "DS80000526H data sheet clarification 6 PORTF",
        "implement RF6 in ODCF",
    ),
    0x0E34: (
        "DS80000526H data sheet clarification 6 PORTF",
        "implement RF6 in CNIEF",
    ),
    0x0E35: (
        "DS80000526H data sheet clarification 6 PORTF",
        "implement RF6 in CNPUF",
    ),
    0x0E36: (
        "DS80000526H data sheet clarification 6 PORTF",
        "implement RF6 in CNPDF",
    ),
    0x0EBA: (
        "DS80000526H data sheet clarification 6 PORTF",
        "include RF6 in GPIO output resolution",
    ),
    0x0EAD: (
        "70616g.pdf register 9-6 ACLKDIV3 and DS80000526H",
        "implement only divider bits 2 through 0",
    ),
    0x0EFC: (
        "DS80000526H errata item 24 RCON.VREGSF",
        "store VREGSF writes while returning zero from software reads",
    ),
    0x1016: (
        "70616g.pdf register 8-13 DMALCA",
        "DMALCA is read-only and resets to no active channel",
    ),
    0x1100: ("70616g.pdf register 13-1 T1CON", "Timer1 control write mask"),
    0x1101: ("70616g.pdf register 13-2 T2CON", "Timer2 control write mask"),
    0x1102: ("70616g.pdf register 13-3 T3CON", "Timer3 control write mask"),
    0x1103: ("70616g.pdf register 13-2 T4CON", "Timer4 control write mask"),
    0x1104: ("70616g.pdf register 13-3 T5CON", "Timer5 control write mask"),
    0x1105: ("70616g.pdf register 13-2 T6CON", "Timer6 control write mask"),
    0x1106: ("70616g.pdf register 13-3 T7CON", "Timer7 control write mask"),
    0x1107: ("70616g.pdf register 13-2 T8CON", "Timer8 control write mask"),
    0x1108: ("70616g.pdf register 13-3 T9CON", "Timer9 control write mask"),
    0x1116: (
        "70616g.pdf section 13 Timer1 operation",
        "T1CON, TMR1 and PR1 state after three instruction cycles",
    ),
    0x1117: (
        "70616g.pdf section 13 Timer2 operation",
        "T2CON, TMR2 and PR2 state after three instruction cycles",
    ),
    0x1118: (
        "70616g.pdf section 13 Timer3 operation",
        "T3CON, TMR3 and PR3 state after three instruction cycles",
    ),
    0x1207: (
        "70616g.pdf register 23-3 AD2CON2",
        "AD2CON2 implemented fields and BUFS read-only state",
    ),
    0x120F: (
        "70616g.pdf section 23 ADC1 manual conversion",
        "AD1 control, channel, buffer and interrupt state",
    ),
    0x1210: (
        "70616g.pdf section 23 ADC2 manual conversion",
        "AD2 control, channel, buffer and interrupt state",
    ),
    0x1300: (
        "70616g.pdf register 16-1 PTCON",
        "PTCON implemented fields and SESTAT hardware state",
    ),
    0x1305: (
        "70616g.pdf register 16-5 STCON",
        "STCON implemented fields and reserved SYNCSRC encodings",
    ),
    0x1403: (
        "70616g.pdf register 18-2 SPI1CON1 note 4",
        "SMP remains clear while SPI1 is in Slave mode",
    ),
    0x1408: (
        "70616g.pdf register 18-2 SPI2CON1 note 4",
        "SMP remains clear while SPI2 is in Slave mode",
    ),
    0x140D: (
        "70616g.pdf register 18-2 SPI3CON1 note 4",
        "SMP remains clear while SPI3 is in Slave mode",
    ),
    0x1412: (
        "70616g.pdf register 18-2 SPI4CON1 note 4",
        "SMP remains clear while SPI4 is in Slave mode",
    ),
    0x1414: (
        "70616g.pdf registers 18-1 through 18-3 SPI1",
        "SPI1 transfer completion, interrupt and buffer status",
    ),
    0x1415: (
        "70616g.pdf registers 18-1 through 18-3 SPI2",
        "SPI2 transfer completion, interrupt and buffer status",
    ),
    0x1416: (
        "70616g.pdf registers 18-1 through 18-3 SPI3",
        "SPI3 transfer completion, interrupt and buffer status",
    ),
    0x1417: (
        "70616g.pdf registers 18-1 through 18-3 SPI4",
        "SPI4 transfer completion, interrupt and buffer status",
    ),
    0x1506: (
        "DS70000353D C1INTE register",
        "C1INTE implemented interrupt-enable bits",
    ),
    0x1503: (
        "DS80000526H data sheet clarification 2 C1FCTRL",
        "reject unsupported 16-bit writes to C1FCTRL",
    ),
    0x1507: (
        "DS70000353D C1EC register",
        "C1EC error counters are software read-only",
    ),
    0x1519: (
        "DS70000353D C2INTE register",
        "C2INTE implemented interrupt-enable bits",
    ),
    0x1516: (
        "DS80000526H data sheet clarification 2 C2FCTRL",
        "reject unsupported 16-bit writes to C2FCTRL",
    ),
    0x151A: (
        "DS70000353D C2EC register",
        "C2EC error counters are software read-only",
    ),
}

UNDEFINED_ORACLE_EVIDENCE = {
    0x0802: "DIV.S",
    0x0803: "DIV.S",
    0x0805: "DIV.S overflow",
    0x0806: "DIV.U",
    0x0807: "DIV.U",
    0x080B: "DIV.SD overflow",
    0x080D: "DIV.UD overflow with zero remainder",
    0x0810: "DIVF",
    0x0811: "DIVF",
    0x0814: "DIV.SD B1 affected overflow",
    0x0815: "DIV.UD overflow with zero remainder",
    0x0817: "DIV.S",
    0x081A: "DIV.S",
    0x081B: "DIV.S",
    0x081C: "DIV.S",
    0x081E: "DIV.U",
    0x081F: "DIV.U",
    0x0823: "DIV.SD",
    0x0826: "DIV.UD",
    0x082C: "DIVF",
}

EXTERNAL_ORACLE_LIMITATION_EVIDENCE = {
    0x023D: (
        "70000157g.pdf page 154 BRA OA",
        "does not retain or branch on the software-written OA and OAB flags",
    ),
    0x023F: (
        "70000157g.pdf page 155 BRA OB",
        "does not retain or branch on the software-written OB and OAB flags",
    ),
    0x0241: (
        "70000157g.pdf page 157 BRA SA",
        "does not retain or branch on the software-written SA and SAB flags",
    ),
    0x0243: (
        "70000157g.pdf page 158 BRA SB",
        "does not retain or branch on the software-written SB and SAB flags",
    ),
    0x1600: (
        "70616g.pdf register 22-12 U1OTGIR",
        "exposes write-one-to-clear status as ordinary storage",
    ),
    0x1604: (
        "70616g.pdf register 22-3 U1PWRC",
        "exposes hardware and writable fields as ordinary storage",
    ),
    0x1605: (
        "70616g.pdf register 22-14 U1IR",
        "exposes write-one-to-clear status as ordinary storage",
    ),
    0x1606: (
        "70616g.pdf register 22-16 U1IE",
        "exposes implemented interrupt enables as ordinary storage",
    ),
    0x1607: (
        "70616g.pdf register 22-18 U1EIR",
        "exposes write-one-to-clear error status as ordinary storage",
    ),
    0x1609: (
        "70616g.pdf register 22-4 U1STAT",
        "exposes hardware-only transaction status as ordinary storage",
    ),
    0x160D: (
        "70616g.pdf register 22-29 U1FRML",
        "exposes the hardware-only frame number as ordinary storage",
    ),
    0x160E: (
        "70616g.pdf register 22-28 U1FRMH",
        "exposes the hardware-only frame number as ordinary storage",
    ),
}


def oracle_rule_identifiers() -> set[int]:
    groups = (
        DATASHEET_STATUS_OVERRIDES,
        DATASHEET_RECORD_WORD_OVERRIDES,
        DATASHEET_STATUS_IGNORE_MASKS,
    )
    identifiers: set[int] = set()
    for group in groups:
        overlap = identifiers & group.keys()
        if overlap:
            formatted = ", ".join(f"0x{value:04x}" for value in sorted(overlap))
            raise RuntimeError(f"Overlapping oracle rules: {formatted}")
        identifiers.update(group)
    return identifiers


def oracle_rule_evidence(identifier: int) -> tuple[str, str, str]:
    if identifier in DOCUMENTED_ORACLE_EVIDENCE:
        authority, reason = DOCUMENTED_ORACLE_EVIDENCE[identifier]
        return "documented", authority, reason
    if identifier in UNDEFINED_ORACLE_EVIDENCE:
        subject = UNDEFINED_ORACLE_EVIDENCE[identifier]
        reason = "exclude only the undefined final C value"
        if identifier == 0x0814:
            return (
                "undefined",
                "70000157g.pdf DIV.SD status definition and DS80000526H item 3",
                "exclude undefined C and B1 erratum-unreliable OV",
            )
        if identifier in DATASHEET_STATUS_SET_BITS:
            reason += " while requiring Z for the zero remainder"
        return "undefined", f"70000157g.pdf {subject} status definition", reason
    if identifier in EXTERNAL_ORACLE_LIMITATION_EVIDENCE:
        authority, limitation = EXTERNAL_ORACLE_LIMITATION_EVIDENCE[identifier]
        return (
            "external-oracle-limitation",
            authority,
            f"frozen external oracle {limitation}",
        )
    raise RuntimeError(f"Oracle rule 0x{identifier:04x} has no evidence")


def oracle_rule_evidence_text(identifier: int) -> str:
    classification, authority, reason = oracle_rule_evidence(identifier)
    return f"class={classification} authority={authority} reason={reason}"


def verify_oracle_evidence_census() -> None:
    evidence_identifiers = (
        set(DOCUMENTED_ORACLE_EVIDENCE)
        | set(UNDEFINED_ORACLE_EVIDENCE)
        | set(EXTERNAL_ORACLE_LIMITATION_EVIDENCE)
    )
    rule_identifiers = oracle_rule_identifiers()
    if evidence_identifiers != rule_identifiers:
        missing = sorted(rule_identifiers - evidence_identifiers)
        extra = sorted(evidence_identifiers - rule_identifiers)
        raise RuntimeError(
            "Oracle evidence census mismatch: "
            f"missing={[f'0x{value:04x}' for value in missing]} "
            f"extra={[f'0x{value:04x}' for value in extra]}"
        )
    overlap = (
        set(DOCUMENTED_ORACLE_EVIDENCE) & set(UNDEFINED_ORACLE_EVIDENCE)
        | set(DOCUMENTED_ORACLE_EVIDENCE) & set(EXTERNAL_ORACLE_LIMITATION_EVIDENCE)
        | set(UNDEFINED_ORACLE_EVIDENCE) & set(EXTERNAL_ORACLE_LIMITATION_EVIDENCE)
    )
    if overlap:
        raise RuntimeError(
            "Oracle evidence classifications overlap: "
            + ",".join(f"0x{value:04x}" for value in sorted(overlap))
        )
    classes = {classification: 0 for classification in EXPECTED_ADDRESS_ORACLE_CLASSES}
    for identifier in sorted(rule_identifiers):
        classification, authority, reason = oracle_rule_evidence(identifier)
        if not authority or not reason:
            raise RuntimeError(
                f"Oracle rule 0x{identifier:04x} has incomplete evidence"
            )
        classes[classification] += 1
    if classes != EXPECTED_ADDRESS_ORACLE_CLASSES:
        raise RuntimeError(
            f"Oracle address evidence census changed: "
            f"expected={EXPECTED_ADDRESS_ORACLE_CLASSES} actual={classes}"
        )


def write_oracle_evidence_ledger(
    evidence_records: list[str],
    failure_records: list[str],
) -> tuple[str, dict[str, int], dict[str, int]]:
    passed_classes = {"documented": 0, "undefined": 0, "external-oracle-limitation": 0}
    passed_rule_identifiers = {
        int(match.group(1), 16)
        for record in evidence_records
        if (match := re.search(r"\bcase=0x([0-9a-f]{4})\b", record)) is not None
    }
    configured_rule_identifiers = oracle_rule_identifiers()
    failed_rule_identifiers = {
        int(match.group(1), 16)
        for record in failure_records
        if (match := re.search(r"\bcase=0x([0-9a-f]{4})\b", record)) is not None
        and int(match.group(1), 16) in configured_rule_identifiers
    }
    evaluated_rule_identifiers = passed_rule_identifiers | failed_rule_identifiers
    if evaluated_rule_identifiers != configured_rule_identifiers:
        inactive = sorted(configured_rule_identifiers - evaluated_rule_identifiers)
        unknown = sorted(evaluated_rule_identifiers - configured_rule_identifiers)
        raise RuntimeError(
            "Oracle evidence activity changed: "
            f"inactive={[f'0x{value:04x}' for value in inactive]} "
            f"unknown={[f'0x{value:04x}' for value in unknown]}"
        )
    if passed_rule_identifiers & failed_rule_identifiers:
        overlap = sorted(passed_rule_identifiers & failed_rule_identifiers)
        raise RuntimeError(
            "Oracle evidence has conflicting outcomes: "
            + ",".join(f"0x{value:04x}" for value in overlap)
        )
    passed_system_selectors = {
        int(match.group(1))
        for record in evidence_records
        if (match := re.search(r"\bsystem-probe=(\d+)\b", record)) is not None
    }
    failed_system_selectors = {
        int(match.group(1))
        for record in failure_records
        if (match := re.search(r"\bsystem-probe=(\d+)\b", record)) is not None
    }
    configured_system_selectors = {71, 75}
    if (
        passed_system_selectors | failed_system_selectors != configured_system_selectors
        or passed_system_selectors & failed_system_selectors
    ):
        raise RuntimeError(
            "Oracle system-probe evidence activity changed: "
            f"passed={sorted(passed_system_selectors)} "
            f"failed={sorted(failed_system_selectors)}"
        )
    for record in evidence_records:
        match = re.search(r"\bclass=([a-z-]+)", record)
        if match is None or match.group(1) not in passed_classes:
            raise RuntimeError(f"Oracle evidence has no classification: {record}")
        passed_classes[match.group(1)] += 1
    failed_classes = {
        classification: EXPECTED_ORACLE_CLASSES[classification]
        - passed_classes[classification]
        for classification in EXPECTED_ORACLE_CLASSES
    }
    if any(count < 0 for count in failed_classes.values()):
        raise RuntimeError(
            f"Oracle evidence census exceeds configured counts: "
            f"expected={EXPECTED_ORACLE_CLASSES} actual={passed_classes}"
        )
    content = (
        json.dumps(
            {
                "schema_version": 2,
                "configured_counts": EXPECTED_ORACLE_CLASSES,
                "passed_counts": passed_classes,
                "failed_counts": failed_classes,
                "typed_cases": evidence_records,
                "failed_cases": failure_records,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n"
    )
    ORACLE_EVIDENCE_LEDGER.parent.mkdir(parents=True, exist_ok=True)
    ORACLE_EVIDENCE_LEDGER.write_text(content, encoding="utf-8", newline="\n")
    return hashlib.sha256(content.encode()).hexdigest(), passed_classes, failed_classes


def run(command: list[str], environment: dict[str, str] | None = None) -> str:
    result = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        env=environment,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        detail = (result.stdout + result.stderr).strip()
        raise RuntimeError(
            detail or f"Command failed with exit code {result.returncode}"
        )
    return result.stdout + result.stderr


def build() -> None:
    run(
        [
            "cmake",
            "-S",
            str(SIMULATOR_SOURCE),
            "-B",
            str(SIMULATOR_BUILD),
            "-G",
            "Ninja",
        ]
    )
    run(["cmake", "--build", str(SIMULATOR_BUILD), "--parallel"])
    run(
        [
            "cmake",
            "-S",
            str(CONFORMANCE_SOURCE),
            "-B",
            str(CONFORMANCE_BUILD),
            "-G",
            "Ninja",
        ]
    )
    run(["cmake", "--build", str(CONFORMANCE_BUILD), "--parallel"])
    if not BUILT_CONFORMANCE_IMAGE.is_file():
        raise RuntimeError("Current conformance firmware was not produced")


def symbols() -> dict[str, int]:
    output = run([str(OBJDUMP), "-t", str(CONFORMANCE_IMAGE)])
    resolved: dict[str, int] = {}
    for line in output.splitlines():
        match = re.match(r"^([0-9a-fA-F]+)\s+.+\s(_[A-Za-z0-9_]+)$", line)
        if match is not None:
            resolved[match.group(2)] = int(match.group(1), 16)
    required = {
        "_conformance_complete",
        "_conformance_output",
        "_main",
        "_run_can_conformance",
        "_run_system_probe",
        "_run_usb_conformance",
        "_system_divide_zero_probe",
        "_system_address_error_probe",
        "_system_address_trap_complete",
        "_system_address_trap_state",
        "_system_binary_address_error_probe",
        "_system_data_limit_control_probe",
        "_system_data_map_control_complete",
        "_system_data_map_control_state",
        "_system_data_map_trap_state",
        "_system_dsp_x_prefetch_complete",
        "_system_dsp_x_prefetch_state",
        "_system_dsp_x_fault_instruction",
        "_system_dsp_x_fault_state",
        "_system_dsp_x_program_fault_instruction",
        "_system_dsp_x_program_fault_state",
        "_system_psv_program_fault_instruction",
        "_system_psv_program_fault_state",
        "_system_psv_program_byte_fault_instruction",
        "_system_psv_program_double_fault_instruction",
        "_system_psv_repeat_complete",
        "_system_psv_repeat_state",
        "_system_auxiliary_program_capture",
        "_system_auxiliary_program_complete",
        "_system_auxiliary_program_state",
        "_system_move_file_load_fault_instruction",
        "_system_move_file_rmw_fault_instruction",
        "_system_move_file_store_fault_instruction",
        "_system_move_file_fault_state",
        "_system_crc_lane_complete",
        "_system_crc_lane_state",
        "_system_output_compare_sync_complete",
        "_system_do_overflow_probe",
        "_system_eds_page_byte_read_probe",
        "_system_eds_page_byte_write_probe",
        "_system_eds_page_move_double_read_probe",
        "_system_eds_page_move_double_write_probe",
        "_system_eds_page_trap_state",
        "_system_eds_page_word_read_probe",
        "_system_eds_page_word_write_probe",
        "_system_idle_probe",
        "_system_math_trap_complete",
        "_system_multi_operand_buffer",
        "_system_multi_operand_control_complete",
        "_system_multi_operand_control_probe",
        "_system_multi_operand_control_state",
        "_system_multi_operand_trap_state",
        "_system_page_zero_byte_read_probe",
        "_system_page_zero_byte_write_probe",
        "_system_page_zero_control_complete",
        "_system_page_zero_control_state",
        "_system_page_zero_direct_probe",
        "_system_page_zero_trap_state",
        "_system_page_zero_word_read_probe",
        "_system_page_zero_word_write_probe",
        "_system_program_target_bra_dispatch",
        "_system_program_target_call_long_probe",
        "_system_program_target_call_probe",
        "_system_program_target_goto_long_probe",
        "_system_program_target_goto_probe",
        "_system_program_target_rcall_dispatch",
        "_system_program_target_retlw_probe",
        "_system_program_target_retfie_probe",
        "_system_program_target_return_probe",
        "_system_program_target_trap_state",
        "_system_program_read_trap_state",
        "_system_program_read_table_probe",
        "_system_program_boundary_capture",
        "_system_program_boundary_complete",
        "_system_program_boundary_state",
        "_system_probe_selector",
        "_system_reset_state",
        "_system_repeat_divide_target",
        "_system_repeat_math_complete",
        "_system_repeat_trap_state",
        "_system_repeat_irq_state",
        "_system_repeat_irq_target",
        "_system_repeat_irq_complete",
        "_system_sfr_wait_bset_repeat",
        "_system_sfr_wait_complete",
        "_system_sfr_wait_move_double_repeat",
        "_system_sfr_wait_move_repeat",
        "_system_sfr_wait_state",
        "_system_pseudo_linear_complete",
        "_system_pseudo_linear_move_double",
        "_system_pseudo_linear_probe",
        "_system_pseudo_linear_state",
        "_system_sftac_consecutive_probe",
        "_system_sftac_probe",
        "_system_sftac_repeat_probe",
        "_system_sequential_hole_complete",
        "_system_sequential_hole_dispatch",
        "_system_sequential_hole_probe",
        "_system_sequential_hole_state",
        "_system_skip_one_word_probe",
        "_system_skip_state",
        "_system_skip_two_word_complete",
        "_system_skip_two_word_probe",
        "_system_skip_unexpected_call",
        "_system_shift_address_error_probe",
        "_system_sleep_probe",
        "_system_soft_trap_complete",
        "_system_stack_limit_probe",
        "_system_stack_trap_complete",
        "_system_stack_trap_state",
        "_system_trap_state",
        "_system_unary_address_error_probe",
        "_system_unimplemented_read_probe",
        "_system_unimplemented_wrap_read_probe",
        "_system_unimplemented_wrap_write_probe",
        "_system_unimplemented_write_probe",
        "_system_unused_sfr_probe",
    }
    missing = required - resolved.keys()
    if missing:
        raise RuntimeError(f"Missing conformance symbols: {', '.join(sorted(missing))}")
    return resolved


def case_count(resolved: dict[str, int]) -> int:
    return sum(
        value for name, value in resolved.items() if name.endswith("_conformance_cases")
    )


def group_progress(
    resolved: dict[str, int],
    groups: tuple[str, ...],
    complete_groups: tuple[str, ...],
) -> tuple[int, int]:
    active = sum(f"_{name}_conformance_cases" in resolved for name in groups)
    complete = sum(name in complete_groups for name in groups)
    return active, complete


def system_probe_program_words(
    selector: int, resolved: dict[str, int]
) -> tuple[tuple[int, int, int], ...]:
    if selector not in (48, 49, 50, 51, 52, 53, 54, 55, 56, 62, 63, 64, 70, 71):
        return ()
    if selector == 71:
        capture = resolved["_system_auxiliary_program_capture"]
        if capture >= 0x55800 or (capture & 1) != 0:
            raise RuntimeError("Invalid auxiliary program capture address")
        return (
            (0x7FC000, 0x200010, 0xFFFFFF),
            (0x7FC002, 0x884000, 0xFFFFFF),
            (0x7FFFFA, 0x7FC100, 0xFFFFFF),
            (0x7FC100, 0x256783, 0xFFFFFF),
            (0x7FC102, 0x040000 | (capture & 0xFFFF), 0xFFFFFF),
            (0x7FC104, (capture >> 16) & 0x007F, 0xFFFFFF),
        )
    if selector == 70:
        return (
            (0x7FF8, 0x001111, 0xFFFFFF),
            (0x7FFA, 0x002222, 0xFFFFFF),
            (0x7FFC, 0x003333, 0xFFFFFF),
        )
    if selector == 64:
        return ((0x7FFE, 0x003456, 0xFFFFFF),)
    if selector in (62, 63):
        low = 0x00A500 if selector == 62 else 0x002233
        high = 0x00005A if selector == 62 else 0x005566
        return ((0x7FFE, low, 0xFFFFFF), (0x8000, high, 0xFFFFFF))
    if selector == 56:
        instruction = resolved["_system_skip_one_word_probe"]
        extension = resolved["_system_sequential_hole_probe"]
        if instruction != 0x557FC or extension != 0x557FE:
            raise RuntimeError("Invalid DO program-boundary probe layout")
        return ((instruction, 0x080001, 0x020340), (extension, 0x000002, 0))
    if selector in (50, 51, 52, 53):
        one_word = resolved["_system_skip_one_word_probe"]
        two_word = resolved["_system_skip_two_word_probe"]
        target = resolved["_system_skip_unexpected_call"]
        if one_word != 0x557FC or two_word != 0x557FA or target != 0x340:
            raise RuntimeError("Invalid skip program-boundary probe layout")
        one_word_selector = selector in (50, 52)
        address = one_word if one_word_selector else two_word
        restore = 0x020000 | target if one_word_selector else 0
        instruction = 0xA70002 if selector in (50, 51) else 0xE78012
        return ((address, instruction, restore),)
    address = resolved["_system_sequential_hole_probe"]
    target = resolved["_system_program_boundary_capture"]
    if address != 0x557FE or target > 0xFFFE or (target & 1) != 0:
        raise RuntimeError("Invalid CALL/GOTO program-boundary probe layout")
    if selector == 54:
        return ((address, 0xE78811, 0),)
    if selector == 55:
        return ((address, 0x090002, 0),)
    instruction = 0x040000 if selector == 48 else 0x020000
    return ((address, instruction | target, 0),)


def external_oracle_results(
    recorded_cases: int, result_size: int
) -> tuple[bytes, dict[int, tuple[int, tuple[int, ...]]]]:
    document = json.loads(EXTERNAL_ORACLE.read_text(encoding="utf-8"))
    if document.get("schema_version") != 1:
        raise RuntimeError("External oracle schema is invalid")
    image_hash = hashlib.sha256(CONFORMANCE_IMAGE.read_bytes()).hexdigest()
    if document.get("conformance_image_sha256") != image_hash:
        raise RuntimeError("External oracle does not match the conformance image")
    if document.get("recorded_cases") != recorded_cases:
        raise RuntimeError("External oracle case count is stale")
    if document.get("result_size") != result_size:
        raise RuntimeError("External oracle result size is stale")
    reference = bytes.fromhex(document["reference_hex"])
    if len(reference) != result_size:
        raise RuntimeError("External oracle result data is truncated")
    if document.get("can_w0c_failures"):
        raise RuntimeError("External oracle CAN write-zero-clear evidence is not exact")
    probes = {
        int(selector): (int(value[0]), tuple(int(word) for word in value[1]))
        for selector, value in document["system_probes"].items()
    }
    return reference, probes


def native_results(size: int, verbose: bool) -> bytes:
    output = run(
        [
            str(SIMULATOR),
            str(CONFORMANCE_IMAGE),
            "0",
            "--stop",
            "_conformance_complete",
            "--dump-memory",
            "_conformance_output",
            str(size),
        ]
    )
    if verbose:
        print(output.rstrip())
    match = re.search(r"^\[memory\].* data=([0-9a-f]+)$", output, re.MULTILINE)
    if match is None:
        raise RuntimeError("Native simulator returned no conformance result memory")
    return bytes.fromhex(match.group(1))


def native_component_conformance(
    executable: Path,
    component: str,
    verbose: bool,
    side_effect_coverage: dict[int, tuple[int, str]],
) -> tuple[int, int]:
    result = subprocess.run(
        [str(executable)],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output = result.stdout + result.stderr
    if verbose or result.returncode != 0:
        print(output.rstrip())
    match = re.search(
        rf"^\[{component}-summary\] cases=(\d+) passed=(\d+) failed=(\d+)$",
        output,
        re.MULTILINE,
    )
    if match is None:
        raise RuntimeError(f"Native {component} conformance returned no summary")
    cases, passed, failed = (int(value) for value in match.groups())
    if passed + failed != cases or (result.returncode == 0) != (failed == 0):
        raise RuntimeError(
            f"Native {component} conformance returned an invalid summary"
        )
    coverage_lines = re.findall(
        r"^\[sfr-side-effect-coverage\] (.+)$", output, re.MULTILINE
    )
    coverage_records = re.findall(
        r"^\[sfr-side-effect-coverage\] component=([a-z0-9-]+) "
        r"address=0x([0-9a-f]{4}) mask=0x([0-9a-f]{4})$",
        output,
        re.MULTILINE,
    )
    if len(coverage_lines) != len(coverage_records):
        raise RuntimeError(
            f"Native {component} conformance returned malformed SFR side-effect coverage"
        )
    for owner, address_text, mask_text in coverage_records:
        address = int(address_text, 16)
        mask = int(mask_text, 16)
        if owner != component:
            raise RuntimeError(
                f"Native {component} conformance reported SFR coverage for {owner}"
            )
        if address in side_effect_coverage:
            previous_mask, previous_owner = side_effect_coverage[address]
            raise RuntimeError(
                f"SFR side-effect coverage overlaps at 0x{address:04x}: "
                f"{previous_owner}=0x{previous_mask:04x}, {owner}=0x{mask:04x}"
            )
        side_effect_coverage[address] = (mask, owner)
    print(
        f"[{component}] cases={cases} passed={passed} failed={failed}",
        flush=True,
    )
    return cases, failed


def native_sfr_reset_snapshots(
    verbose: bool,
) -> tuple[tuple[int, ...], tuple[int, ...]]:
    result = subprocess.run(
        [str(SFR_RESET_CONFORMANCE)],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output = result.stdout + result.stderr
    if verbose or result.returncode != 0:
        print(output.rstrip())
    snapshots = []
    for name in ("sfr-reset-snapshot", "sfr-mclr-snapshot"):
        match = re.search(rf"^\[{name}\] ([0-9a-f]{{8192}})$", output, re.MULTILINE)
        if result.returncode != 0 or match is None:
            raise RuntimeError(f"Native SFR reset conformance returned no {name}")
        payload = match.group(1)
        snapshots.append(
            tuple(int(payload[offset : offset + 4], 16) for offset in range(0, 8192, 4))
        )
    return snapshots[0], snapshots[1]


def parse_numeric_record(output: str, name: str) -> dict[str, int]:
    matches = re.findall(rf"^\[{re.escape(name)}\] (.+)$", output, re.MULTILINE)
    if len(matches) != 1:
        raise RuntimeError(
            f"Native SFR access conformance returned {len(matches)} {name} records"
        )
    fields = re.findall(r"([a-z-]+)=(\d+)", matches[0])
    if not fields or " ".join(f"{key}={value}" for key, value in fields) != matches[0]:
        raise RuntimeError(
            f"Native SFR access conformance returned an invalid {name} record"
        )
    record = {key: int(value) for key, value in fields}
    if len(record) != len(fields):
        raise RuntimeError(
            f"Native SFR access conformance returned duplicate {name} fields"
        )
    return record


def native_sfr_access_census(
    verbose: bool,
) -> tuple[
    dict[str, int],
    dict[str, int],
    dict[str, int],
    dict[str, int],
    dict[int, int],
]:
    result = subprocess.run(
        [str(SFR_ACCESS_CONFORMANCE)],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output = result.stdout + result.stderr
    inventory = parse_numeric_record(output, "sfr-access-inventory")
    summary = parse_numeric_record(output, "sfr-access-summary")
    mux_summary = parse_numeric_record(output, "sfr-mux-summary")
    conditional_summary = parse_numeric_record(output, "sfr-conditional-summary")
    dependent_normal_summary = parse_numeric_record(
        output, "sfr-dependent-normal-summary"
    )
    protected_summary = parse_numeric_record(output, "sfr-protected-summary")
    map_summary = parse_numeric_record(output, "sfr-map-summary")
    side_effect_lines = re.findall(
        r"^\[sfr-side-effect-expected\] (.+)$", output, re.MULTILINE
    )
    side_effect_records = re.findall(
        r"^\[sfr-side-effect-expected\] address=0x([0-9a-f]{4}) "
        r"mask=0x([0-9a-f]{4})$",
        output,
        re.MULTILINE,
    )
    if len(side_effect_lines) != len(side_effect_records):
        raise RuntimeError(
            "Native SFR access conformance returned malformed side effects"
        )
    side_effect_expectations = {
        int(address, 16): int(mask, 16) for address, mask in side_effect_records
    }
    if len(side_effect_expectations) != len(side_effect_records):
        raise RuntimeError(
            "Native SFR access conformance returned duplicate side effects"
        )
    side_effect_bits = sum(
        mask.bit_count() for mask in side_effect_expectations.values()
    )
    if side_effect_bits != inventory["side-effect-bits"]:
        raise RuntimeError(
            f"Native SFR side-effect expectations contain {side_effect_bits} bits"
        )
    expected_summary_fields = {"unresolved-addresses"}
    expected_summary_fields.update(
        f"{access_class}-{suffix}"
        for access_class in SFR_ACCESS_CLASSES
        for suffix in ("addresses", "bits")
    )
    expected_mux_summary_fields = {
        "addresses",
        "unresolved-addresses",
        "selector-reset-addresses",
        "selector-reset-bits",
        "selector-switch-addresses",
        "selector-switch-bits",
        "alternate-unresolved-addresses",
    }
    expected_mux_summary_fields.update(
        f"alternate-{access_class}-{suffix}"
        for access_class in SFR_ACCESS_CLASSES
        for suffix in ("addresses", "bits")
    )
    unresolved_records = len(
        re.findall(r"^\[sfr-access-unresolved\] ", output, re.MULTILINE)
    )
    unresolved_mux_records = len(
        re.findall(r"^\[sfr-mux-unresolved\] ", output, re.MULTILINE)
    )
    unresolved_conditional_records = len(
        re.findall(r"^\[sfr-conditional-unresolved\] ", output, re.MULTILINE)
    )
    if inventory != SFR_ACCESS_EXPECTED_INVENTORY:
        raise RuntimeError(f"Native SFR access inventory is {inventory}")
    if set(summary) != expected_summary_fields:
        raise RuntimeError(f"Native SFR access summary fields are {sorted(summary)}")
    if summary != SFR_ACCESS_EXPECTED_UNRESOLVED:
        raise RuntimeError(f"Native SFR access unresolved inventory is {summary}")
    if set(mux_summary) != expected_mux_summary_fields:
        raise RuntimeError(f"Native SFR mux summary fields are {sorted(mux_summary)}")
    if conditional_summary != SFR_CONDITIONAL_EXPECTED_SUMMARY:
        raise RuntimeError(
            f"Native SFR conditional access summary is {conditional_summary}"
        )
    if dependent_normal_summary != SFR_DEPENDENT_NORMAL_EXPECTED_SUMMARY:
        raise RuntimeError(
            f"Native SFR dependent normal summary is {dependent_normal_summary}"
        )
    if protected_summary != SFR_PROTECTED_EXPECTED_SUMMARY:
        raise RuntimeError(f"Native SFR protected summary is {protected_summary}")
    if map_summary != SFR_MAP_EXPECTED_SUMMARY:
        raise RuntimeError(f"Native SFR implementation map summary is {map_summary}")
    for access_class in SFR_ACCESS_CLASSES:
        addresses = summary[f"{access_class}-addresses"]
        bits = summary[f"{access_class}-bits"]
        if (addresses == 0) != (bits == 0):
            raise RuntimeError(f"Native SFR access {access_class} counts disagree")
        if addresses > summary["unresolved-addresses"]:
            raise RuntimeError(
                f"Native SFR access {access_class} address count is invalid"
            )
        if bits > inventory[f"{access_class}-bits"]:
            raise RuntimeError(f"Native SFR access {access_class} bit count is invalid")
        alternate_addresses = mux_summary[f"alternate-{access_class}-addresses"]
        alternate_bits = mux_summary[f"alternate-{access_class}-bits"]
        if (alternate_addresses == 0) != (alternate_bits == 0):
            raise RuntimeError(
                f"Native SFR mux alternate {access_class} counts disagree"
            )
        if alternate_addresses > mux_summary["alternate-unresolved-addresses"]:
            raise RuntimeError(
                f"Native SFR mux alternate {access_class} address count is invalid"
            )
        if alternate_bits > inventory[f"alternate-{access_class}-bits"]:
            raise RuntimeError(
                f"Native SFR mux alternate {access_class} bit count is invalid"
            )
    if unresolved_records != summary["unresolved-addresses"]:
        raise RuntimeError(
            f"Native SFR access conformance returned {unresolved_records} unresolved "
            f"records for {summary['unresolved-addresses']} addresses"
        )
    if mux_summary["addresses"] != inventory["mux-alternates"]:
        raise RuntimeError(
            f"Native SFR mux conformance checked {mux_summary['addresses']} addresses"
        )
    if unresolved_mux_records != mux_summary["unresolved-addresses"]:
        raise RuntimeError(
            f"Native SFR mux conformance returned {unresolved_mux_records} unresolved "
            f"records for {mux_summary['unresolved-addresses']} addresses"
        )
    if unresolved_conditional_records != conditional_summary["unresolved-addresses"]:
        raise RuntimeError(
            "Native SFR conditional conformance returned "
            f"{unresolved_conditional_records} unresolved records for "
            f"{conditional_summary['unresolved-addresses']} addresses"
        )
    if (
        mux_summary["alternate-unresolved-addresses"]
        > mux_summary["unresolved-addresses"]
    ):
        raise RuntimeError("Native SFR mux alternate unresolved count is invalid")
    for selector in ("reset", "switch"):
        addresses = mux_summary[f"selector-{selector}-addresses"]
        bits = mux_summary[f"selector-{selector}-bits"]
        if (addresses == 0) != (bits == 0):
            raise RuntimeError(f"Native SFR mux selector {selector} counts disagree")
        if addresses > mux_summary["unresolved-addresses"]:
            raise RuntimeError(
                f"Native SFR mux selector {selector} address count is invalid"
            )
        if bits > inventory["mux-alternates"]:
            raise RuntimeError(
                f"Native SFR mux selector {selector} bit count is invalid"
            )
    expected_returncode = (
        0
        if summary["unresolved-addresses"] == 0
        and mux_summary["unresolved-addresses"] == 0
        and conditional_summary["unresolved-addresses"] == 0
        and dependent_normal_summary["failures"] == 0
        and map_summary["failures"] == 0
        else 1
    )
    if result.returncode != expected_returncode:
        raise RuntimeError(
            f"Native SFR access conformance exited {result.returncode}, "
            f"expected {expected_returncode}"
        )
    if verbose:
        print(output.rstrip())
    unresolved_classes = sum(
        summary[f"{access_class}-bits"] != 0 for access_class in SFR_ACCESS_CLASSES
    )
    print(
        f"[sfr-access] addresses={inventory['addresses']} "
        f"unresolved-addresses={summary['unresolved-addresses']} "
        f"normal={summary['normal-addresses']}/{summary['normal-bits']} "
        f"read-only={summary['read-only-addresses']}/{summary['read-only-bits']} "
        f"reserved={summary['reserved-addresses']}/{summary['reserved-bits']} "
        f"write-only={summary['write-only-addresses']}/{summary['write-only-bits']} "
        f"unresolved-classes={unresolved_classes} "
        f"side-effect-bits={inventory['side-effect-bits']} "
        f"side-effect-expected-addresses={len(side_effect_expectations)}",
        flush=True,
    )
    mux_unresolved_classes = sum(
        mux_summary[f"alternate-{access_class}-bits"] != 0
        for access_class in SFR_ACCESS_CLASSES
    )
    mux_unresolved_classes += mux_summary["selector-reset-bits"] != 0
    mux_unresolved_classes += mux_summary["selector-switch-bits"] != 0
    print(
        f"[sfr-mux] addresses={mux_summary['addresses']} "
        f"unresolved-addresses={mux_summary['unresolved-addresses']} "
        f"selector-reset={mux_summary['selector-reset-addresses']}/"
        f"{mux_summary['selector-reset-bits']} "
        f"selector-switch={mux_summary['selector-switch-addresses']}/"
        f"{mux_summary['selector-switch-bits']} "
        f"alternate-unresolved={mux_summary['alternate-unresolved-addresses']} "
        f"normal={mux_summary['alternate-normal-addresses']}/"
        f"{mux_summary['alternate-normal-bits']} "
        f"read-only={mux_summary['alternate-read-only-addresses']}/"
        f"{mux_summary['alternate-read-only-bits']} "
        f"reserved={mux_summary['alternate-reserved-addresses']}/"
        f"{mux_summary['alternate-reserved-bits']} "
        f"write-only={mux_summary['alternate-write-only-addresses']}/"
        f"{mux_summary['alternate-write-only-bits']} "
        f"unresolved-classes={mux_unresolved_classes} "
        f"side-effect-bits={inventory['alternate-side-effect-bits']} "
        f"side-effect-generic-checks=0",
        flush=True,
    )
    print(
        f"[sfr-conditional] addresses={conditional_summary['addresses']} "
        f"unresolved-addresses={conditional_summary['unresolved-addresses']} "
        f"normal-bits={conditional_summary['normal-bits']} "
        f"reserved-bits={conditional_summary['reserved-bits']} "
        f"absence={conditional_summary['absence-addresses']} "
        f"isolation={conditional_summary['isolation-addresses']} "
        f"selector-reset={conditional_summary['selector-reset-addresses']}/"
        f"{conditional_summary['selector-reset-bits']} "
        f"selector-switch={conditional_summary['selector-switch-addresses']}/"
        f"{conditional_summary['selector-switch-bits']}",
        flush=True,
    )
    print(
        f"[sfr-dependent-normal] addresses={dependent_normal_summary['addresses']} "
        f"failures={dependent_normal_summary['failures']}",
        flush=True,
    )
    print(
        f"[sfr-map] words={map_summary['words']} "
        f"implemented={map_summary['implemented']} absent={map_summary['absent']} "
        f"absent-ranges={map_summary['absent-ranges']} "
        f"direct={map_summary['direct-byte-checks']}/"
        f"{map_summary['direct-word-checks']} "
        f"internal-pad={map_summary['internal-pad-byte-checks']}/"
        f"{map_summary['internal-pad-word-checks']} "
        f"odd-crossing={map_summary['odd-crossing-checks']} "
        f"lifecycle={map_summary['lifecycle-checks']} failures=0",
        flush=True,
    )
    return (
        inventory,
        summary,
        mux_summary,
        conditional_summary,
        side_effect_expectations,
    )


def validate_sfr_side_effect_coverage(
    expected: dict[int, int], actual: dict[int, tuple[int, str]]
) -> None:
    missing = sorted(set(expected) - set(actual))
    extra = sorted(set(actual) - set(expected))
    mismatched = sorted(
        address
        for address in set(expected) & set(actual)
        if expected[address] != actual[address][0]
    )
    if missing or extra or mismatched:
        details = []
        if missing:
            details.append(
                "missing=" + ",".join(f"0x{address:04x}" for address in missing)
            )
        if extra:
            details.append("extra=" + ",".join(f"0x{address:04x}" for address in extra))
        if mismatched:
            details.append(
                "mismatched="
                + ",".join(
                    f"0x{address:04x}:0x{actual[address][0]:04x}/"
                    f"0x{expected[address]:04x}"
                    for address in mismatched
                )
            )
        raise RuntimeError(
            "SFR side-effect coverage is incomplete: " + " ".join(details)
        )
    bits = sum(mask.bit_count() for mask, _ in actual.values())
    owners = len({owner for _, owner in actual.values()})
    print(
        f"[sfr-side-effect-coverage] addresses={len(actual)} bits={bits} "
        f"owners={owners} failures=0",
        flush=True,
    )


def verify_sfr_side_effect_coverage_regressions() -> None:
    expected = {0x0100: 0x0001, 0x0102: 0x0002}
    invalid = (
        {0x0100: (0x0001, "first")},
        {
            0x0100: (0x0001, "first"),
            0x0102: (0x0002, "second"),
            0x0104: (0x0004, "third"),
        },
        {0x0100: (0x0001, "first"), 0x0102: (0x0004, "second")},
    )
    for coverage in invalid:
        try:
            validate_sfr_side_effect_coverage(expected, coverage)
        except RuntimeError:
            continue
        raise RuntimeError("SFR side-effect coverage accepted an invalid proof set")


def sfr_pattern_mask(pattern: str, accepted: str) -> int:
    return sum(
        1 << (15 - index)
        for index, character in enumerate(pattern)
        if character in accepted
    )


def sfr_reset_inventory() -> dict[int, dict[str, object]]:
    document = json.loads(gzip.decompress(SFR_MANIFEST.read_bytes()).decode("utf-8"))
    by_address: dict[int, list[dict[str, object]]] = {}
    for register in document["registers"]:
        address = int(register["address"], 16)
        by_address.setdefault(address, []).append(register)
    inventory: dict[int, dict[str, object]] = {}
    for address, registers in by_address.items():
        defaults = [register for register in registers if register["selector"] is None]
        if len(defaults) != 1:
            raise RuntimeError(
                f"SFR address 0x{address:04x} has {len(defaults)} default definitions"
            )
        inventory[address] = defaults[0]
    if len(inventory) != 977:
        raise RuntimeError(f"SFR reset inventory has {len(inventory)} addresses")
    return inventory


def compare_sfr_por_snapshot(candidate: tuple[int, ...]) -> list[str]:
    if len(candidate) != 2048:
        raise RuntimeError(f"SFR POR snapshot has {len(candidate)} halfwords")
    inventory = sfr_reset_inventory()
    failures: list[str] = []
    excluded: list[str] = []
    checked_bits = 0
    mismatched_bits = 0
    known_readable_bits = 0
    unknown_readable_bits = 0
    write_only_known_bits = 0
    reserved_known_bits = 0
    dynamic_excluded_bits = 0
    for address, register in sorted(inventory.items()):
        access = str(register["access"])
        por = str(register["por"])
        name = str(register["name"])
        readable_mask = sfr_pattern_mask(access, "nrcs")
        known_mask = sfr_pattern_mask(por, "01")
        por_value = sfr_pattern_mask(por, "1")
        known_observed_mask = readable_mask & known_mask
        exclusion_mask, exclusion_source = SFR_POR_DYNAMIC_EXCLUSIONS.get(
            address, (0, "")
        )
        excluded_mask = known_observed_mask & exclusion_mask
        checked_mask = known_observed_mask & ~excluded_mask & 0xFFFF
        candidate_value = candidate[address // 2]
        difference = (candidate_value ^ por_value) & checked_mask
        checked_bits += checked_mask.bit_count()
        mismatched_bits += difference.bit_count()
        known_readable_bits += known_observed_mask.bit_count()
        unknown_readable_bits += (readable_mask & ~known_mask & 0xFFFF).bit_count()
        write_only_known_bits += (
            sfr_pattern_mask(access, "w") & known_mask
        ).bit_count()
        reserved_known_bits += (sfr_pattern_mask(access, "-") & known_mask).bit_count()
        dynamic_excluded_bits += excluded_mask.bit_count()
        if difference:
            failures.append(
                f"sfr-por {name}@0x{address:04x} por=0x{por_value:04x} "
                f"native=0x{candidate_value:04x} checked=0x{checked_mask:04x} "
                f"difference=0x{difference:04x}"
            )
        if excluded_mask:
            excluded.append(
                f"{name}@0x{address:04x} mask=0x{excluded_mask:04x} "
                f"source={exclusion_source}"
            )
    print(
        f"[sfr-por] addresses={len(inventory)} checked-bits={checked_bits} "
        f"exact-bits={checked_bits - mismatched_bits} "
        f"mismatched-bits={mismatched_bits} "
        f"mismatched-addresses={len(failures)}",
        flush=True,
    )
    print(
        f"[sfr-reset-metadata] known-readable-bits={known_readable_bits} "
        f"unknown-readable-bits={unknown_readable_bits} "
        f"write-only-known-bits={write_only_known_bits} "
        f"reserved-known-bits={reserved_known_bits} "
        f"dynamic-excluded-bits={dynamic_excluded_bits}",
        flush=True,
    )
    for exclusion in excluded:
        print(f"[sfr-por-exclusion] {exclusion}")
    return failures


def compare_sfr_mclr_snapshot(candidate: tuple[int, ...]) -> list[str]:
    if len(candidate) != 2048:
        raise RuntimeError(f"SFR MCLR snapshot has {len(candidate)} halfwords")
    inventory = sfr_reset_inventory()
    failures: list[str] = []
    excluded: list[str] = []
    checked_bits = 0
    mismatched_bits = 0
    known_readable_bits = 0
    unknown_readable_bits = 0
    unchanged_readable_bits = 0
    dynamic_excluded_bits = 0
    for address, register in sorted(inventory.items()):
        access = str(register["access"])
        mclr = str(register["mclr"])
        name = str(register["name"])
        readable_mask = sfr_pattern_mask(access, "nrcs")
        known_mask = sfr_pattern_mask(mclr, "01")
        unchanged_mask = sfr_pattern_mask(mclr, "u")
        expected_value = sfr_pattern_mask(mclr, "1") | unchanged_mask
        known_observed_mask = readable_mask & (known_mask | unchanged_mask)
        exclusion_mask, exclusion_source = SFR_MCLR_DYNAMIC_EXCLUSIONS.get(
            address, (0, "")
        )
        excluded_mask = known_observed_mask & exclusion_mask
        checked_mask = known_observed_mask & ~excluded_mask & 0xFFFF
        candidate_value = candidate[address // 2]
        difference = (candidate_value ^ expected_value) & checked_mask
        checked_bits += checked_mask.bit_count()
        mismatched_bits += difference.bit_count()
        known_readable_bits += (readable_mask & known_mask).bit_count()
        unchanged_readable_bits += (readable_mask & unchanged_mask).bit_count()
        unknown_readable_bits += (
            readable_mask & ~(known_mask | unchanged_mask) & 0xFFFF
        ).bit_count()
        dynamic_excluded_bits += excluded_mask.bit_count()
        if difference:
            failures.append(
                f"sfr-mclr {name}@0x{address:04x} expected=0x{expected_value:04x} "
                f"native=0x{candidate_value:04x} checked=0x{checked_mask:04x} "
                f"difference=0x{difference:04x}"
            )
        if excluded_mask:
            excluded.append(
                f"{name}@0x{address:04x} mask=0x{excluded_mask:04x} "
                f"source={exclusion_source}"
            )
    print(
        f"[sfr-mclr] addresses={len(inventory)} checked-bits={checked_bits} "
        f"exact-bits={checked_bits - mismatched_bits} "
        f"mismatched-bits={mismatched_bits} "
        f"mismatched-addresses={len(failures)}",
        flush=True,
    )
    print(
        f"[sfr-mclr-metadata] known-readable-bits={known_readable_bits} "
        f"unchanged-readable-bits={unchanged_readable_bits} "
        f"unknown-readable-bits={unknown_readable_bits} "
        f"dynamic-excluded-bits={dynamic_excluded_bits}",
        flush=True,
    )
    for exclusion in excluded:
        print(f"[sfr-mclr-exclusion] {exclusion}")
    return failures


def native_system_probe(
    selector: int,
    stop: str | None,
    memory: str,
    size: int,
    verbose: bool,
    program_words: tuple[tuple[int, int, int], ...] = (),
) -> tuple[int, tuple[int, ...]]:
    command = [
        str(SIMULATOR),
        str(CONFORMANCE_IMAGE),
        "_run_system_probe",
        "--register",
        "W0",
        str(selector),
    ]
    for address, value, _restore in program_words:
        command.extend(["--program-word", hex(address), hex(value)])
    if stop is not None:
        command.extend(["--stop", stop])
    command.extend(["--dump-memory", memory, str(size)])
    output = run(command)
    if verbose:
        print(output.rstrip())
    pc_match = re.search(r"^\[passed\] pc=0x([0-9a-f]+)", output, re.MULTILINE)
    memory_match = re.search(r"^\[memory\].* data=([0-9a-f]+)$", output, re.MULTILINE)
    if pc_match is None or memory_match is None:
        raise RuntimeError(
            f"Native simulator returned incomplete system probe {selector}"
        )
    data = bytes.fromhex(memory_match.group(1))
    if len(data) != size or (size & 1) != 0:
        raise RuntimeError(f"Native simulator returned invalid system probe {selector}")
    words = tuple(
        int.from_bytes(data[offset : offset + 2], "little")
        for offset in range(0, size, 2)
    )
    return int(pc_match.group(1), 16), words


def native_system_probes(
    resolved: dict[str, int], verbose: bool
) -> dict[int, tuple[int, tuple[int, ...]]]:
    reset_pc, reset_rcon = native_system_probe(3, "0", "0x740", 2, verbose)
    reset_state_pc, reset_state = native_system_probe(
        3, "0", "_system_reset_state", 2, verbose
    )
    if reset_pc != reset_state_pc:
        raise RuntimeError("Native reset probes stopped at different addresses")
    return {
        1: native_system_probe(1, None, "0x740", 2, verbose),
        2: native_system_probe(2, None, "0x740", 2, verbose),
        3: (reset_pc, reset_rcon + reset_state),
        4: native_system_probe(
            4, "_system_math_trap_complete", "_system_trap_state", 8, verbose
        ),
        5: native_system_probe(
            5, "_system_math_trap_complete", "_system_trap_state", 20, verbose
        ),
        6: native_system_probe(
            6, "_system_math_trap_complete", "_system_trap_state", 20, verbose
        ),
        7: native_system_probe(
            7, "_system_math_trap_complete", "_system_trap_state", 20, verbose
        ),
        57: native_system_probe(
            57,
            "_system_repeat_math_complete",
            "_system_repeat_trap_state",
            16,
            verbose,
        ),
        58: native_system_probe(
            58,
            "_system_repeat_irq_complete",
            "_system_repeat_irq_state",
            20,
            verbose,
        ),
        59: native_system_probe(
            59,
            "_system_sfr_wait_complete",
            "_system_sfr_wait_state",
            12,
            verbose,
        ),
        60: native_system_probe(
            60,
            "_system_sfr_wait_complete",
            "_system_sfr_wait_state",
            12,
            verbose,
        ),
        61: native_system_probe(
            61,
            "_system_sfr_wait_complete",
            "_system_sfr_wait_state",
            12,
            verbose,
        ),
        62: native_system_probe(
            62,
            "_system_pseudo_linear_complete",
            "_system_pseudo_linear_state",
            24,
            verbose,
            system_probe_program_words(62, resolved),
        ),
        63: native_system_probe(
            63,
            "_system_address_trap_complete",
            "_system_eds_page_trap_state",
            26,
            verbose,
            system_probe_program_words(63, resolved),
        ),
        64: native_system_probe(
            64,
            "_system_dsp_x_prefetch_complete",
            "_system_dsp_x_prefetch_state",
            20,
            verbose,
            system_probe_program_words(64, resolved),
        ),
        65: native_system_probe(
            65,
            "_system_address_trap_complete",
            "_system_dsp_x_fault_state",
            30,
            verbose,
        ),
        66: native_system_probe(
            66,
            "_system_address_trap_complete",
            "_system_dsp_x_program_fault_state",
            30,
            verbose,
        ),
        67: native_system_probe(
            67,
            "_system_address_trap_complete",
            "_system_psv_program_fault_state",
            28,
            verbose,
        ),
        68: native_system_probe(
            68,
            "_system_address_trap_complete",
            "_system_psv_program_fault_state",
            28,
            verbose,
        ),
        69: native_system_probe(
            69,
            "_system_address_trap_complete",
            "_system_psv_program_fault_state",
            28,
            verbose,
        ),
        70: native_system_probe(
            70,
            "_system_psv_repeat_complete",
            "_system_psv_repeat_state",
            14,
            verbose,
            system_probe_program_words(70, resolved),
        ),
        71: native_system_probe(
            71,
            "_system_auxiliary_program_complete",
            "_system_auxiliary_program_state",
            14,
            verbose,
            system_probe_program_words(71, resolved),
        ),
        72: native_system_probe(
            72,
            "_system_address_trap_complete",
            "_system_move_file_fault_state",
            22,
            verbose,
        ),
        73: native_system_probe(
            73,
            "_system_address_trap_complete",
            "_system_move_file_fault_state",
            22,
            verbose,
        ),
        74: native_system_probe(
            74,
            "_system_address_trap_complete",
            "_system_move_file_fault_state",
            22,
            verbose,
        ),
        75: native_system_probe(
            75,
            "_system_crc_lane_complete",
            "_system_crc_lane_state",
            8,
            verbose,
        ),
        76: native_system_probe(
            76,
            "_system_output_compare_sync_complete",
            "0x0908",
            2,
            verbose,
        ),
        8: native_system_probe(
            8, "_system_soft_trap_complete", "_system_trap_state", 24, verbose
        ),
        9: native_system_probe(
            9,
            "_system_stack_trap_complete",
            "_system_stack_trap_state",
            24,
            verbose,
        ),
        10: native_system_probe(
            10,
            "_system_address_trap_complete",
            "_system_address_trap_state",
            20,
            verbose,
        ),
        11: native_system_probe(
            11,
            "_system_address_trap_complete",
            "_system_multi_operand_trap_state",
            20,
            verbose,
        ),
        12: native_system_probe(
            12,
            "_system_address_trap_complete",
            "_system_multi_operand_trap_state",
            20,
            verbose,
        ),
        13: native_system_probe(
            13,
            "_system_address_trap_complete",
            "_system_multi_operand_trap_state",
            20,
            verbose,
        ),
        14: native_system_probe(
            14,
            "_system_multi_operand_control_complete",
            "_system_multi_operand_control_state",
            12,
            verbose,
        ),
        15: native_system_probe(
            15,
            "_system_data_map_control_complete",
            "_system_data_map_control_state",
            12,
            verbose,
        ),
        16: native_system_probe(
            16,
            "_system_address_trap_complete",
            "_system_data_map_trap_state",
            22,
            verbose,
        ),
        17: native_system_probe(
            17,
            "_system_address_trap_complete",
            "_system_data_map_trap_state",
            22,
            verbose,
        ),
        18: native_system_probe(
            18,
            "_system_address_trap_complete",
            "_system_data_map_trap_state",
            22,
            verbose,
        ),
        19: native_system_probe(
            19,
            "_system_address_trap_complete",
            "_system_data_map_trap_state",
            22,
            verbose,
        ),
        20: native_system_probe(
            20,
            "_system_data_map_control_complete",
            "_system_data_map_control_state",
            8,
            verbose,
        ),
        21: native_system_probe(
            21,
            "_system_page_zero_control_complete",
            "_system_page_zero_control_state",
            16,
            verbose,
        ),
        22: native_system_probe(
            22,
            "_system_address_trap_complete",
            "_system_page_zero_trap_state",
            24,
            verbose,
        ),
        23: native_system_probe(
            23,
            "_system_address_trap_complete",
            "_system_page_zero_trap_state",
            24,
            verbose,
        ),
        24: native_system_probe(
            24,
            "_system_address_trap_complete",
            "_system_page_zero_trap_state",
            24,
            verbose,
        ),
        25: native_system_probe(
            25,
            "_system_address_trap_complete",
            "_system_page_zero_trap_state",
            24,
            verbose,
        ),
        26: native_system_probe(
            26,
            "_system_address_trap_complete",
            "_system_eds_page_trap_state",
            26,
            verbose,
        ),
        27: native_system_probe(
            27,
            "_system_address_trap_complete",
            "_system_eds_page_trap_state",
            26,
            verbose,
        ),
        28: native_system_probe(
            28,
            "_system_address_trap_complete",
            "_system_eds_page_trap_state",
            26,
            verbose,
        ),
        29: native_system_probe(
            29,
            "_system_address_trap_complete",
            "_system_eds_page_trap_state",
            26,
            verbose,
        ),
        30: native_system_probe(
            30,
            "_system_address_trap_complete",
            "_system_eds_page_trap_state",
            26,
            verbose,
        ),
        31: native_system_probe(
            31,
            "_system_address_trap_complete",
            "_system_eds_page_trap_state",
            26,
            verbose,
        ),
        32: native_system_probe(
            32,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        33: native_system_probe(
            33,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        34: native_system_probe(
            34,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        35: native_system_probe(
            35,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        36: native_system_probe(
            36,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        37: native_system_probe(
            37,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        38: native_system_probe(
            38,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        39: native_system_probe(
            39,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        40: native_system_probe(
            40,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        41: native_system_probe(
            41,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
        ),
        42: native_system_probe(
            42,
            "_system_sequential_hole_complete",
            "_system_sequential_hole_state",
            2,
            verbose,
        ),
        43: native_system_probe(
            43,
            "_system_address_trap_complete",
            "_system_program_read_trap_state",
            26,
            verbose,
        ),
        44: native_system_probe(
            44,
            "_system_address_trap_complete",
            "_system_program_read_trap_state",
            26,
            verbose,
        ),
        45: native_system_probe(
            45,
            "_system_address_trap_complete",
            "_system_program_read_trap_state",
            26,
            verbose,
        ),
        46: native_system_probe(
            46,
            "_system_address_trap_complete",
            "_system_program_read_trap_state",
            26,
            verbose,
        ),
        47: native_system_probe(
            47,
            "_system_address_trap_complete",
            "_system_program_read_trap_state",
            26,
            verbose,
        ),
        48: native_system_probe(
            48,
            "_system_program_boundary_complete",
            "_system_program_boundary_state",
            14,
            verbose,
            system_probe_program_words(48, resolved),
        ),
        49: native_system_probe(
            49,
            "_system_program_boundary_complete",
            "_system_program_boundary_state",
            14,
            verbose,
            system_probe_program_words(49, resolved),
        ),
        50: native_system_probe(
            50,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
            system_probe_program_words(50, resolved),
        ),
        51: native_system_probe(
            51,
            "_system_skip_two_word_complete",
            "_system_skip_state",
            14,
            verbose,
            system_probe_program_words(51, resolved),
        ),
        52: native_system_probe(
            52,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
            system_probe_program_words(52, resolved),
        ),
        53: native_system_probe(
            53,
            "_system_skip_two_word_complete",
            "_system_skip_state",
            14,
            verbose,
            system_probe_program_words(53, resolved),
        ),
        54: native_system_probe(
            54,
            "_system_address_trap_complete",
            "_system_program_target_trap_state",
            22,
            verbose,
            system_probe_program_words(54, resolved),
        ),
        55: native_system_probe(
            55,
            "_system_skip_two_word_complete",
            "_system_sequential_hole_state",
            2,
            verbose,
            system_probe_program_words(55, resolved),
        ),
        56: native_system_probe(
            56,
            "_system_address_trap_complete",
            "_system_do_boundary_state",
            26,
            verbose,
            system_probe_program_words(56, resolved),
        ),
    }


def compare_system_probes(
    reference: dict[int, tuple[int, tuple[int, ...]]],
    candidate: dict[int, tuple[int, tuple[int, ...]]],
    resolved: dict[str, int],
) -> tuple[list[str], list[str]]:
    failures: list[str] = []
    oracle_evidence_records: list[str] = []
    expected_power = {1: 0x0008, 2: 0x0004, 3: 0x0040}
    for selector, expected in expected_power.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        reference_rcon = reference_words[0] & 0x007C
        candidate_rcon = candidate_words[0] & 0x007C
        if reference_rcon != expected or candidate_rcon != expected:
            failures.append(
                f"system-probe={selector} RCON expected=0x{expected:04x} "
                f"external-oracle=0x{reference_rcon:04x} native=0x{candidate_rcon:04x}"
            )
        if selector == 3 and (
            reference_words[1] != 0x5A5A or candidate_words[1] != 0x5A5A
        ):
            failures.append(
                f"system-probe=3 RAM expected=0x5a5a "
                f"external-oracle=0x{reference_words[1]:04x} native=0x{candidate_words[1]:04x}"
            )
    reference_pc, reference_words = reference[4]
    candidate_pc, candidate_words = candidate[4]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=4 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=4 trap expected={reference_words} actual={candidate_words}"
        )
    if (candidate_words[2] & 0x0050) != 0x0050:
        failures.append(
            f"system-probe=4 INTCON1 expected=0x0050 actual=0x{candidate_words[2]:04x}"
        )
    for selector in (5, 6, 7):
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        if reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} trap expected={reference_words} "
                f"actual={candidate_words}"
            )
        if (candidate_words[2] & 0x0090) != 0x0090:
            failures.append(
                f"system-probe={selector} INTCON1 expected=0x0090 "
                f"actual=0x{candidate_words[2]:04x}"
            )
    reference_pc, reference_words = reference[8]
    candidate_pc, candidate_words = candidate[8]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=8 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=8 trap expected={reference_words} actual={candidate_words}"
        )
    if (candidate_words[2] & 0x0010) != 0x0010:
        failures.append(
            f"system-probe=8 INTCON3 expected=0x0010 actual=0x{candidate_words[2]:04x}"
        )
    if candidate_words[3] != 0x0906:
        failures.append(
            f"system-probe=8 INTTREG expected=0x0906 actual=0x{candidate_words[3]:04x}"
        )
    if (candidate_words[4] & 0x0700) != 0x0400:
        failures.append(
            f"system-probe=8 DL expected=4 actual={(candidate_words[4] >> 8) & 7}"
        )
    if candidate_words[5] != 1:
        failures.append(
            f"system-probe=8 DCOUNT expected=0x0001 actual=0x{candidate_words[5]:04x}"
        )
    if candidate_words[10] != 0x1111:
        failures.append(
            f"system-probe=8 marker expected=0x1111 actual=0x{candidate_words[10]:04x}"
        )
    if (candidate_words[11] & 0x0200) == 0:
        failures.append(
            f"system-probe=8 DA expected=1 actual=0x{candidate_words[11]:04x}"
        )
    reference_pc, reference_words = reference[9]
    candidate_pc, candidate_words = candidate[9]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=9 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=9 trap expected={reference_words} actual={candidate_words}"
        )
    if (candidate_words[0] & 0x0004) != 0x0004:
        failures.append(
            f"system-probe=9 INTCON1 expected=0x0004 actual=0x{candidate_words[0]:04x}"
        )
    if candidate_words[1] != (candidate_words[2] + 8) & 0xFFFF:
        failures.append(
            f"system-probe=9 SPLIM expected=SP+8 actual=0x{candidate_words[1]:04x}"
        )
    if candidate_words[3] != (candidate_words[2] + 4) & 0xFFFF:
        failures.append(
            f"system-probe=9 handler-SP expected=SP+4 actual=0x{candidate_words[3]:04x}"
        )
    if candidate_words[6] != 0x0C03:
        failures.append(
            f"system-probe=9 INTTREG expected=0x0c03 actual=0x{candidate_words[6]:04x}"
        )
    if candidate_words[7:11] != (0x1111, 0x0000, 0x0000, 0xA5A5):
        failures.append(
            f"system-probe=9 execution expected=(4369, 0, 0, 42405) "
            f"actual={candidate_words[7:11]}"
        )
    reference_pc, reference_words = reference[10]
    candidate_pc, candidate_words = candidate[10]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=10 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=10 trap expected={reference_words} actual={candidate_words}"
        )
    if (candidate_words[0] & 1) == 0 or candidate_words[1:3] != (0, 0):
        failures.append(
            f"system-probe=10 inhibited-access expected=odd-pointer,0,0 "
            f"actual={candidate_words[:3]}"
        )
    if (candidate_words[3] & 0x0008) == 0 or candidate_words[6] != 0:
        failures.append(
            f"system-probe=10 trap-state expected=ADDRERR,no-marker "
            f"actual=0x{candidate_words[3]:04x},0x{candidate_words[6]:04x}"
        )
    if candidate_words[7] != 0x0E01:
        failures.append(
            f"system-probe=10 INTTREG expected=0x0e01 actual=0x{candidate_words[7]:04x}"
        )
    for selector in (11, 12, 13):
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        if reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} trap expected={reference_words} "
                f"actual={candidate_words}"
            )
        if candidate_words[:6] != (0x5555, 0x0108, candidate_words[2], 0, 0, 0):
            failures.append(
                f"system-probe={selector} partial-execution expected="
                f"(21845, 264, odd, 0, 0, 0) actual={candidate_words[:6]}"
            )
        if (candidate_words[2] & 1) == 0 or (candidate_words[6] & 0x0008) == 0:
            failures.append(
                f"system-probe={selector} trap-state expected=odd-destination,ADDRERR "
                f"actual=0x{candidate_words[2]:04x},0x{candidate_words[6]:04x}"
            )
        if candidate_words[9] != 0x0E01:
            failures.append(
                f"system-probe={selector} INTTREG expected=0x0e01 "
                f"actual=0x{candidate_words[9]:04x}"
            )
    reference_pc, reference_words = reference[14]
    candidate_pc, candidate_words = candidate[14]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=14 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=14 sequencing expected={reference_words} "
            f"actual={candidate_words}"
        )
    expected_pointer = candidate_words[0]
    if (expected_pointer & 1) != 0 or candidate_words != (
        expected_pointer,
        0xC000,
        expected_pointer,
        0xFFFF,
        expected_pointer,
        0x0003,
    ):
        failures.append(
            f"system-probe=14 sequencing expected="
            f"({expected_pointer}, 49152, {expected_pointer}, 65535, "
            f"{expected_pointer}, 3) actual={candidate_words}"
        )
    reference_pc, reference_words = reference[15]
    candidate_pc, candidate_words = candidate[15]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=15 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=15 boundary expected={reference_words} "
            f"actual={candidate_words}"
        )
    if candidate_words != (0xE000, 0xA5A5, 0xE000, 0x5A5A, 0, 0x3333):
        failures.append(
            f"system-probe=15 boundary expected="
            f"(57344, 42405, 57344, 23130, 0, 13107) "
            f"actual={candidate_words}"
        )
    expected_map_traps = {
        16: (0xE002, 0x0000, 1, 1),
        17: (0xE002, 0xA5A5, 1, 1),
        18: (0x8000, 0x0000, 2, 1),
        19: (0x8000, 0xA5A5, 1, 2),
    }
    for selector, expected in expected_map_traps.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        if reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} trap expected={reference_words} "
                f"actual={candidate_words}"
            )
        pointer, value, read_page, write_page = expected
        if (
            candidate_words[:4] != (pointer, value, 0, 0x0008)
            or candidate_words[5] != 0x0F01
            or candidate_words[6] != 0x0E01
            or candidate_words[8:] != (0x5004, read_page, write_page)
        ):
            failures.append(
                f"system-probe={selector} data-map expected="
                f"({pointer}, {value}, 0, 8, frame, 3841, 3585, status, "
                f"20484, {read_page}, {write_page}) actual={candidate_words}"
            )
    reference_pc, reference_words = reference[20]
    candidate_pc, candidate_words = candidate[20]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=20 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=20 SFR-hole expected={reference_words} "
            f"actual={candidate_words}"
        )
    if candidate_words != (0x0058, 0, 0, 0x3333):
        failures.append(
            f"system-probe=20 SFR-hole expected=(88, 0, 0, 13107) "
            f"actual={candidate_words}"
        )
    reference_pc, reference_words = reference[21]
    candidate_pc, candidate_words = candidate[21]
    if reference_pc != candidate_pc:
        failures.append(
            f"system-probe=21 pc expected=0x{reference_pc:06x} "
            f"actual=0x{candidate_pc:06x}"
        )
    if reference_words != candidate_words:
        failures.append(
            f"system-probe=21 direct-page-zero expected={reference_words} "
            f"actual={candidate_words}"
        )
    if candidate_words != (0xA5A5, 0, 0xA5A5, 0, 0, 0, 0, 0x3333):
        failures.append(
            f"system-probe=21 direct-page-zero expected="
            f"(42405, 0, 42405, 0, 0, 0, 0, 13107) actual={candidate_words}"
        )
    expected_page_zero_traps = {
        22: (0x5A5A, 0x5A5A, 0, 1),
        23: (0xA5A5, 0xA5A5, 1, 0),
        24: (0xA55A, 0x5A5A, 0, 1),
        25: (0xA5A5, 0xA55A, 1, 0),
    }
    for selector, expected in expected_page_zero_traps.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        if reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} page-zero expected={reference_words} "
                f"actual={candidate_words}"
            )
        value, memory, read_page, write_page = expected
        if (
            candidate_words[:5] != (0x9002, value, 0, memory, 0x0008)
            or candidate_words[6:10] != (0x0F01, 0x0E01, 0x01CF, 0x5004)
            or candidate_words[10:] != (read_page, write_page)
        ):
            failures.append(
                f"system-probe={selector} page-zero expected="
                f"(36866, {value}, 0, {memory}, 8, frame, 3841, 3585, 463, "
                f"20484, {read_page}, {write_page}) actual={candidate_words}"
            )
    expected_eds_page_traps = {
        26: ((0x9002, 0, 0x3303, 0x4404, 0), (2, 1)),
        27: ((0x9002, 0xA5A5, 0x3303, 0x4404, 0), (1, 2)),
        28: ((0x9002, 0xA500, 0x3303, 0x4404, 0), (2, 1)),
        29: ((0x9002, 0xA5A5, 0x3303, 0x4404, 0), (1, 2)),
        30: ((0x1101, 0, 0, 0x9000, 0), (2, 1)),
        31: ((0x9004, 0x5555, 0x6666, 0x4404, 0), (1, 2)),
    }
    for selector, expected in expected_eds_page_traps.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        if reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} EDS-page expected={reference_words} "
                f"actual={candidate_words}"
            )
        registers, pages = expected
        if (
            candidate_words[:5] != registers
            or candidate_words[5] != 0x0008
            or candidate_words[7:11] != (0x0F01, 0x0E01, 0x01CF, 0x5004)
            or candidate_words[11:] != pages
        ):
            failures.append(
                f"system-probe={selector} EDS-page expected="
                f"({registers}, 8, frame, 3841, 3585, 463, 20484, {pages}) "
                f"actual={candidate_words}"
            )
    program_target_registers = {
        32: (32, 0),
        33: (33, 0),
        34: (0x5800, 5),
        35: (0x5800, 5),
        36: (5, 0),
        37: (0x0F05, 0),
        38: (38, 0),
        39: (39, 0),
        40: (0, 0x0122),
        41: (0, 0x5800),
    }
    for selector, registers in program_target_registers.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        if reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} program-target expected={reference_words} "
                f"actual={candidate_words}"
            )
        trap_low = candidate_words[4]
        call_low = 0
        call_high = 0
        if selector == 33:
            call_low = (trap_low + 2) | 1
            call_high = candidate_words[5] & 0x007F
        elif selector == 35:
            call_low = trap_low | 1
            call_high = candidate_words[5] & 0x007F
        elif selector == 39:
            call_low = trap_low | 1
            call_high = candidate_words[5] & 0x007F
        expected_stack = 0x5008 if selector in (33, 35, 39) else 0x5004
        expected_high = 0x0F05 if selector in (38, 39) else 0x0F01
        expected_control = 0x002C if selector in (32, 34, 38, 41) else 0x0028
        if (
            candidate_words[:2] != registers
            or candidate_words[2:4] != (expected_stack, 0x0008)
            or candidate_words[5:11]
            != (expected_high, call_low, call_high, 0x0E01, 0x01CF, expected_control)
        ):
            failures.append(
                f"system-probe={selector} program-target expected="
                f"({registers}, {expected_stack}, 8, frame, {expected_high}, "
                f"{call_low}, {call_high}, 3585, 463, {expected_control}) "
                f"actual={candidate_words}"
            )
    reference_pc, reference_words = reference[42]
    candidate_pc, candidate_words = candidate[42]
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=42 sequential-hole expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != 0x55804 or candidate_words != (0x4242,):
        failures.append(
            f"system-probe=42 sequential-hole expected=(pc=0x55804, state=(16962,)) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    expected_program_read_registers = {
        43: (0x5800, 0x0000),
        44: (0x5800, 0xAA00),
        45: (0x5801, 0xAA00),
        47: (0x5800, 0xAAAA),
    }
    for selector in range(43, 48):
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc:
            failures.append(
                f"system-probe={selector} pc expected=0x{reference_pc:06x} "
                f"actual=0x{candidate_pc:06x}"
            )
        if reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} program-read expected={reference_words} "
                f"actual={candidate_words}"
            )
        expected_stack = 0x5006 if selector == 47 else 0x5004
        expected_memory = 0 if selector == 47 else candidate_words[5]
        if selector == 46:
            registers_valid = (
                candidate_words[0] == 0x5802 and (candidate_words[1] & 1) != 0
            )
        else:
            registers_valid = (
                candidate_words[:2] == expected_program_read_registers[selector]
            )
        if (
            not registers_valid
            or candidate_words[2:5] != (0, expected_stack, 0x0008)
            or candidate_words[6:12] != (0x0F01, 0x0E01, 0x01CF, 0x002C, 0x0005, 0xA5A5)
            or candidate_words[12] != expected_memory
        ):
            failures.append(
                f"system-probe={selector} program-read state invalid "
                f"actual={candidate_words}"
            )
    expected_program_boundaries = {
        48: (0x0000, 0x5000, 0xA5A5, 0x5A5A, 0x010F, 0x0024, 0x0000),
        49: (0x0000, 0x5004, 0x5803, 0x0005, 0x010F, 0x0020, 0x0000),
    }
    for selector, expected_words in expected_program_boundaries.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc or reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} program-boundary expected="
                f"(pc=0x{reference_pc:06x}, state={reference_words}) "
                f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
            )
        if candidate_words != expected_words:
            failures.append(
                f"system-probe={selector} program-boundary state expected="
                f"{expected_words} actual={candidate_words}"
            )
    reference_pc, reference_words = reference[50]
    candidate_pc, candidate_words = candidate[50]
    expected_skip_trap = (
        0x0000,
        0x0000,
        0x5004,
        0x0008,
        0x5800,
        0x0305,
        0x0000,
        0x0000,
        0x0E01,
        0x01C3,
        0x0028,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=50 skip-trap expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_words != expected_skip_trap:
        failures.append(
            f"system-probe=50 skip-trap state expected={expected_skip_trap} "
            f"actual={candidate_words}"
        )
    reference_pc, reference_words = reference[51]
    candidate_pc, candidate_words = candidate[51]
    expected_skip_state = (
        0x0000,
        0x5000,
        0xA5A5,
        0x5A5A,
        0x0103,
        0x0020,
        0x0000,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=51 skip-two-word expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != 0x55802 or candidate_words != expected_skip_state:
        failures.append(
            f"system-probe=51 skip-two-word state expected="
            f"(pc=0x055802, state={expected_skip_state}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    expected_compare_skip_traps = {
        52: (
            0x0000,
            0x0000,
            0x5004,
            0x0008,
            0x57FE,
            0x0305,
            0x0000,
            0x0000,
            0x0E01,
            0x01C3,
            0x0028,
        ),
        54: (
            0x010F,
            0x0000,
            0x5004,
            0x0008,
            0x5800,
            0x0F05,
            0x0000,
            0x0000,
            0x0E01,
            0x01CF,
            0x002C,
        ),
    }
    for selector, expected_words in expected_compare_skip_traps.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if reference_pc != candidate_pc or reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} compare-skip-trap expected="
                f"(pc=0x{reference_pc:06x}, state={reference_words}) "
                f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
            )
        if candidate_words != expected_words:
            failures.append(
                f"system-probe={selector} compare-skip-trap state expected="
                f"{expected_words} actual={candidate_words}"
            )
    reference_pc, reference_words = reference[53]
    candidate_pc, candidate_words = candidate[53]
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=53 compare-skip-two-word expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != 0x55802 or candidate_words != expected_skip_state:
        failures.append(
            f"system-probe=53 compare-skip-two-word state expected="
            f"(pc=0x055802, state={expected_skip_state}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[55]
    candidate_pc, candidate_words = candidate[55]
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=55 repeat-boundary expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != 0x55802 or candidate_words != (0x4242,):
        failures.append(
            f"system-probe=55 repeat-boundary state expected="
            f"(pc=0x055802, state=(16962,)) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[56]
    candidate_pc, candidate_words = candidate[56]
    expected_do_boundary = (
        0x0000,
        0x5004,
        0x57FE,
        0x0F05,
        0x0008,
        0x0E01,
        0x03CF,
        0x012C,
        0x0001,
        0x5800,
        0x0005,
        0x5804,
        0x0005,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=56 DO-boundary expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_words != expected_do_boundary:
        failures.append(
            f"system-probe=56 DO-boundary state expected={expected_do_boundary} "
            f"actual={candidate_words}"
        )
    reference_pc, reference_words = reference[57]
    candidate_pc, candidate_words = candidate[57]
    target = resolved["_system_repeat_divide_target"]
    expected_repeat_trap = (
        0x5004,
        target & 0xFFFE,
        0x1000 | ((target >> 16) & 0x007F),
        0x000F,
        0x0060,
        0x0028,
        0x0050,
        0x0B04,
    )
    expected_pc = resolved["_system_repeat_math_complete"]
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=57 repeat-math expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_repeat_trap:
        failures.append(
            f"system-probe=57 repeat-math state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_repeat_trap}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[58]
    candidate_pc, candidate_words = candidate[58]
    target = resolved["_system_repeat_irq_target"]
    expected_repeat_irq = (
        0x5004,
        target & 0xFFFE,
        0x1000 | ((target >> 16) & 0x007F),
        0x0002,
        0x0080,
        0x0020,
        0x5000,
        0x0001,
        0x0000,
        0x0000,
    )
    expected_pc = resolved["_system_repeat_irq_complete"]
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            f"system-probe=58 repeat-irq expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_repeat_irq:
        failures.append(
            f"system-probe=58 repeat-irq state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_repeat_irq}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    wait_targets = {
        59: resolved["_system_sfr_wait_bset_repeat"],
        60: resolved["_system_sfr_wait_move_repeat"],
        61: resolved["_system_sfr_wait_move_double_repeat"],
    }
    expected_pc = resolved["_system_sfr_wait_complete"]
    for selector, target in wait_targets.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        if selector == 61:
            expected_words = (
                0x5004,
                (target + 2) & 0xFFFE,
                0x1000 | ((target >> 16) & 0x007F),
                0x0002,
                0x0080,
                0x0020,
            )
        else:
            expected_words = (
                0x5004,
                target & 0xFFFE,
                (target >> 16) & 0x007F,
                0x0000,
                0x0080,
                0x0020,
            )
        if reference_pc != candidate_pc or reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} SFR-wait expected="
                f"(pc=0x{reference_pc:06x}, state={reference_words}) "
                f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
            )
        if candidate_pc != expected_pc or candidate_words != expected_words:
            failures.append(
                f"system-probe={selector} SFR-wait state expected="
                f"(pc=0x{expected_pc:06x}, state={expected_words}) "
                f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
            )
    expected_pseudo_linear = (
        0x8000,
        0x125A,
        0x0201,
        0x8000,
        0x12A5,
        0x0201,
        0xFFFF,
        0x12A5,
        0x0200,
        0xFFFF,
        0x125A,
        0x0200,
    )
    reference_pc, reference_words = reference[62]
    candidate_pc, candidate_words = candidate[62]
    expected_pc = resolved["_system_pseudo_linear_complete"]
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            "system-probe=62 pseudo-linear expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_pseudo_linear:
        failures.append(
            "system-probe=62 pseudo-linear state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_pseudo_linear}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[63]
    candidate_pc, candidate_words = candidate[63]
    expected_pc = resolved["_system_address_trap_complete"]
    target = resolved["_system_pseudo_linear_move_double"]
    expected_pseudo_linear_trap = (
        0x8002,
        0x2233,
        0x0000,
        0x4444,
        0x5555,
        0x0008,
        (target + 2) & 0xFFFE,
        0x0300 | ((target >> 16) & 0x007F),
        0x0E01,
        0x01C3,
        0x5004,
        0x0201,
        0x0001,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            "system-probe=63 pseudo-linear MOV.D expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_pseudo_linear_trap:
        failures.append(
            "system-probe=63 pseudo-linear MOV.D state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_pseudo_linear_trap}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[64]
    candidate_pc, candidate_words = candidate[64]
    expected_pc = resolved["_system_dsp_x_prefetch_complete"]
    expected_dsp_x_prefetch = (
        0x000C,
        0x0000,
        0x0000,
        0x3456,
        0x6789,
        0x8000,
        0x9000,
        0x0201,
        0x0021,
        0x0000,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            "system-probe=64 DSP X prefetch expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_dsp_x_prefetch:
        failures.append(
            "system-probe=64 DSP X prefetch state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_dsp_x_prefetch}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[65]
    candidate_pc, candidate_words = candidate[65]
    expected_pc = resolved["_system_address_trap_complete"]
    target = resolved["_system_dsp_x_fault_instruction"]
    expected_dsp_x_fault = (
        0x000C,
        0x0000,
        0x0000,
        0x0000,
        0x6789,
        0x9000,
        0x9000,
        0x0001,
        0x0008,
        (target + 2) & 0xFFFE,
        (target >> 16) & 0x007F,
        0x0E01,
        0x00C0,
        0x0029,
        0x5204,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            "system-probe=65 DSP X fault expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_dsp_x_fault:
        failures.append(
            "system-probe=65 DSP X fault state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_dsp_x_fault}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[66]
    candidate_pc, candidate_words = candidate[66]
    expected_pc = resolved["_system_address_trap_complete"]
    target = resolved["_system_dsp_x_program_fault_instruction"]
    expected_dsp_x_program_fault = (
        0x000C,
        0x0000,
        0x0000,
        0x0000,
        0x6789,
        0xD800,
        0x9000,
        0x020A,
        0x0008,
        (target + 2) & 0xFFFE,
        (target >> 16) & 0x007F,
        0x0E01,
        0x00C0,
        0x0029,
        0x5204,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            "system-probe=66 DSP X program fault expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_dsp_x_program_fault:
        failures.append(
            "system-probe=66 DSP X program fault state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_dsp_x_program_fault}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    psv_program_fault_cases = (
        (67, "_system_psv_program_fault_instruction", 0xD802, 0x0000, 0xBEEF),
        (68, "_system_psv_program_byte_fault_instruction", 0xD801, 0xA500, 0xBEEF),
        (69, "_system_psv_program_double_fault_instruction", 0xD804, 0x0000, 0x0000),
    )
    expected_pc = resolved["_system_address_trap_complete"]
    for selector, target_symbol, pointer, low, high in psv_program_fault_cases:
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        target = resolved[target_symbol]
        expected_psv_program_fault = (
            pointer,
            low,
            high,
            0x020A,
            0x0008,
            (target + 2) & 0xFFFE,
            (target >> 16) & 0x007F,
            0x0E01,
            0x00C0,
            0x0029,
            0x5204,
            0x1357,
            selector,
            0x0000,
        )
        if reference_pc != candidate_pc or reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} PSV program fault expected="
                f"(pc=0x{reference_pc:06x}, state={reference_words}) "
                f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
            )
        if candidate_pc != expected_pc or candidate_words != expected_psv_program_fault:
            failures.append(
                f"system-probe={selector} PSV program fault state expected="
                f"(pc=0x{expected_pc:06x}, state={expected_psv_program_fault}) "
                f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
            )
    reference_pc, reference_words = reference[70]
    candidate_pc, candidate_words = candidate[70]
    expected_pc = resolved["_system_psv_repeat_complete"]
    expected_psv_repeat = (
        0xFFFE,
        0x3333,
        0x0200,
        0x0000,
        0x0103,
        0x0020,
        0x7070,
    )
    if reference_pc != candidate_pc or reference_words != candidate_words:
        failures.append(
            "system-probe=70 PSV repeat expected="
            f"(pc=0x{reference_pc:06x}, state={reference_words}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_psv_repeat:
        failures.append(
            "system-probe=70 PSV repeat state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_psv_repeat}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    reference_pc, reference_words = reference[71]
    candidate_pc, candidate_words = candidate[71]
    expected_pc = resolved["_system_auxiliary_program_complete"]
    expected_auxiliary_program = (
        0x5678,
        0x0408,
        0xC006,
        0x037F,
        0x5004,
        0x0010,
        0x7171,
    )
    expected_external_oracle_auxiliary_program = (
        0x5678,
        0x0408,
        0x0000,
        0x0000,
        0x5000,
        0x0010,
        0x7171,
    )
    if (
        reference_pc != expected_pc
        or reference_words != expected_external_oracle_auxiliary_program
    ):
        failures.append(
            "system-probe=71 External oracle auxiliary frame limitation expected="
            f"(pc=0x{expected_pc:06x}, state={expected_external_oracle_auxiliary_program}) "
            f"actual=(pc=0x{reference_pc:06x}, state={reference_words})"
        )
    if candidate_pc != expected_pc or candidate_words != expected_auxiliary_program:
        failures.append(
            "system-probe=71 auxiliary program state expected="
            f"(pc=0x{expected_pc:06x}, state={expected_auxiliary_program}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    if (
        reference_pc == expected_pc
        and reference_words == expected_external_oracle_auxiliary_program
        and candidate_pc == expected_pc
        and candidate_words == expected_auxiliary_program
    ):
        oracle_evidence_records.append(
            "system-probe=71 External oracle executes the auxiliary handler but omits the "
            "documented interrupt return frame class=external-oracle-limitation "
            "authority=dsPIC33-PIC24-FRM-Interrupts-DS70000600E.pdf "
            "reason=External oracle omits the auxiliary interrupt return frame"
        )
    move_file_fault_targets = {
        72: "_system_move_file_load_fault_instruction",
        73: "_system_move_file_rmw_fault_instruction",
        74: "_system_move_file_store_fault_instruction",
    }
    for selector, target_symbol in move_file_fault_targets.items():
        reference_pc, reference_words = reference[selector]
        candidate_pc, candidate_words = candidate[selector]
        target = resolved[target_symbol]
        if reference_pc != candidate_pc or reference_words != candidate_words:
            failures.append(
                f"system-probe={selector} file-MOV fault expected="
                f"(pc=0x{reference_pc:06x}, state={reference_words}) "
                f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
            )
        expected_words = (
            0x5A5A if selector == 72 else 0xA5A5,
            0x1111,
            0x3333,
            0x01C5 if selector in (72, 73) else 0x01CD,
            0x0008,
            (target + 2) & 0xFFFE,
            (0x0500 if selector in (72, 73) else 0x0D00) | ((target >> 16) & 0x007F),
            0x0E01,
            0x5004,
            0x5A5A,
            selector,
        )
        if (
            candidate_pc != resolved["_system_address_trap_complete"]
            or candidate_words != expected_words
        ):
            failures.append(
                f"system-probe={selector} file-MOV fault state expected="
                f"{expected_words} actual={candidate_words}"
            )
    reference_pc, reference_words = reference[75]
    candidate_pc, candidate_words = candidate[75]
    expected_external_oracle_crc_lanes = (
        0x8040,
        0x8040,
        0x8040,
        0x8040,
    )
    expected_crc_lanes = (
        0x8100,
        0x8040,
        0x8040,
        0x8100,
    )
    expected_crc_pc = resolved["_system_crc_lane_complete"]
    if (
        reference_pc != expected_crc_pc
        or reference_words != expected_external_oracle_crc_lanes
    ):
        failures.append(
            "system-probe=75 External oracle CRC lane limitation expected="
            f"(pc=0x{expected_crc_pc:06x}, state={expected_external_oracle_crc_lanes}) "
            f"actual=(pc=0x{reference_pc:06x}, state={reference_words})"
        )
    if candidate_pc != expected_crc_pc or candidate_words != expected_crc_lanes:
        failures.append(
            "system-probe=75 CRC lane state expected="
            f"{expected_crc_lanes} actual={candidate_words}"
        )
    if (
        reference_pc == expected_crc_pc
        and reference_words == expected_external_oracle_crc_lanes
        and candidate_pc == expected_crc_pc
        and candidate_words == expected_crc_lanes
    ):
        oracle_evidence_records.append(
            "system-probe=75 External oracle ignores the documented 8-bit CRC FIFO write "
            "class=external-oracle-limitation authority=S27.pdf "
            "reason=External oracle ignores the high-byte CRC FIFO lane"
        )
    reference_pc, reference_words = reference[76]
    candidate_pc, candidate_words = candidate[76]
    expected_output_compare_pc = resolved["_system_output_compare_sync_complete"]
    expected_output_compare_timer = (0x0003,)
    if (
        reference_pc != expected_output_compare_pc
        or reference_words != expected_output_compare_timer
    ):
        failures.append(
            "system-probe=76 External oracle OC synchronization state expected="
            f"(pc=0x{expected_output_compare_pc:06x}, "
            f"state={expected_output_compare_timer}) "
            f"actual=(pc=0x{reference_pc:06x}, state={reference_words})"
        )
    if (
        candidate_pc != expected_output_compare_pc
        or candidate_words != expected_output_compare_timer
    ):
        failures.append(
            "system-probe=76 OC synchronization state expected="
            f"(pc=0x{expected_output_compare_pc:06x}, "
            f"state={expected_output_compare_timer}) "
            f"actual=(pc=0x{candidate_pc:06x}, state={candidate_words})"
        )
    return failures, oracle_evidence_records


def classify_differences(
    reference: bytes, candidate: bytes, cases: int
) -> tuple[list[str], list[str]]:
    failures: list[str] = []
    oracle_evidence_records: list[str] = []
    rule_identifiers = oracle_rule_identifiers()
    for index in range(cases):
        start = 2 + index * 8
        expected_record = reference[start : start + 8]
        actual_record = candidate[start : start + 8]
        identifier = int.from_bytes(expected_record[:2], "little")
        authoritative_record = expected_record
        normalized_actual = actual_record
        evidence = (
            oracle_rule_evidence_text(identifier)
            if identifier in rule_identifiers
            else ""
        )
        ignored_status = DATASHEET_STATUS_IGNORE_MASKS.get(identifier)
        if ignored_status is not None:
            datasheet_record = bytearray(expected_record)
            external_oracle_status = int.from_bytes(datasheet_record[6:8], "little")
            native_status = int.from_bytes(actual_record[6:8], "little")
            defined_external_oracle_status = (
                external_oracle_status | DATASHEET_STATUS_SET_BITS.get(identifier, 0)
            ) & ~ignored_status
            defined_native_status = native_status & ~ignored_status
            datasheet_record[6:8] = defined_external_oracle_status.to_bytes(2, "little")
            normalized = bytearray(actual_record)
            normalized[6:8] = defined_native_status.to_bytes(2, "little")
            authoritative_record = bytes(datasheet_record)
            normalized_actual = bytes(normalized)
            if authoritative_record == normalized_actual:
                oracle_evidence_records.append(
                    f"case=0x{identifier:04x} "
                    f"raw-equal={str(expected_record == actual_record).lower()} "
                    f"ignored-status=0x{ignored_status:04x} "
                    f"external-oracle=0x{external_oracle_status:04x} "
                    f"defined/native=0x{defined_native_status:04x} {evidence}"
                )
                continue
        elif (
            word_overrides := DATASHEET_RECORD_WORD_OVERRIDES.get(identifier)
        ) is not None:
            datasheet_record = bytearray(expected_record)
            for word, value in word_overrides.items():
                datasheet_record[word * 2 : word * 2 + 2] = value.to_bytes(2, "little")
            authoritative_record = bytes(datasheet_record)
            if authoritative_record == actual_record:
                oracle_evidence_records.append(
                    f"case=0x{identifier:04x} "
                    f"raw-equal={str(expected_record == actual_record).lower()} record "
                    f"external-oracle={expected_record.hex()} "
                    f"datasheet/native={datasheet_record.hex()} {evidence}"
                )
                continue
        elif (
            status_override := DATASHEET_STATUS_OVERRIDES.get(identifier)
        ) is not None:
            datasheet_record = bytearray(expected_record)
            external_oracle_status = int.from_bytes(datasheet_record[4:6], "little")
            datasheet_status = (external_oracle_status & ~0x0100) | status_override
            datasheet_record[4:6] = datasheet_status.to_bytes(2, "little")
            authoritative_record = bytes(datasheet_record)
            if authoritative_record == actual_record:
                oracle_evidence_records.append(
                    f"case=0x{identifier:04x} "
                    f"raw-equal={str(expected_record == actual_record).lower()} status "
                    f"external-oracle=0x{external_oracle_status:04x} "
                    f"datasheet/native=0x{datasheet_status:04x} {evidence}"
                )
                continue
        elif expected_record == actual_record:
            continue
        offset = next(
            item
            for item, values in enumerate(zip(authoritative_record, normalized_actual))
            if values[0] != values[1]
        )
        failures.append(
            f"case=0x{identifier:04x} byte={offset} "
            f"expected=0x{authoritative_record[offset]:02x} "
            f"actual=0x{normalized_actual[offset]:02x} {evidence}".rstrip()
        )
    if reference[2 + cases * 8 :] != candidate[2 + cases * 8 :]:
        failures.append("unused result storage differs")
    return failures, oracle_evidence_records


def verify_classifier_regressions() -> None:
    prefix = b"\x04\x00"
    identifier = (0x0E00).to_bytes(2, "little")
    external_oracle_record = identifier + b"\x00" * 6
    corrected_record = bytearray(external_oracle_record)
    corrected_record[6:8] = (0x7A85).to_bytes(2, "little")
    failures, evidence = classify_differences(
        prefix + external_oracle_record, prefix + external_oracle_record, 1
    )
    if len(failures) != 1 or evidence:
        raise RuntimeError("Oracle classifier accepted a known-wrong exact record")
    failures, evidence = classify_differences(
        prefix + external_oracle_record, prefix + corrected_record, 1
    )
    if failures or len(evidence) != 1 or "raw-equal=false" not in evidence[0]:
        raise RuntimeError("Oracle classifier rejected a documented correction")

    authoritative_record = bytes(corrected_record)
    failures, evidence = classify_differences(
        prefix + authoritative_record, prefix + authoritative_record, 1
    )
    if failures or len(evidence) != 1 or "raw-equal=true" not in evidence[0]:
        raise RuntimeError("Oracle classifier omitted raw-equal typed evidence")

    outside_override = bytearray(corrected_record)
    outside_override[2:4] = (0x0001).to_bytes(2, "little")
    failures, evidence = classify_differences(
        prefix + external_oracle_record, prefix + outside_override, 1
    )
    if len(failures) != 1 or evidence:
        raise RuntimeError("Oracle classifier ignored a mismatch outside an override")

    status_identifier = (0x032C).to_bytes(2, "little")
    status_external_oracle_record = status_identifier + b"\x00" * 6
    status_corrected_record = bytearray(status_external_oracle_record)
    status_corrected_record[4:6] = (0x0100).to_bytes(2, "little")
    failures, evidence = classify_differences(
        prefix + status_external_oracle_record, prefix + status_corrected_record, 1
    )
    if failures or len(evidence) != 1:
        raise RuntimeError("Oracle classifier rejected a status correction")

    divide_identifier = (0x080D).to_bytes(2, "little")
    divide_external_oracle_record = divide_identifier + b"\x00" * 4 + b"\x05\x01"
    for native_status in (0x0106, 0x0107):
        divide_native_record = (
            divide_identifier + b"\x00" * 4 + native_status.to_bytes(2, "little")
        )
        failures, evidence = classify_differences(
            prefix + divide_external_oracle_record, prefix + divide_native_record, 1
        )
        if failures or len(evidence) != 1:
            raise RuntimeError("Oracle classifier rejected an undefined DIV.U C value")

    divide_missing_zero = divide_identifier + b"\x00" * 4 + b"\x04\x01"
    failures, evidence = classify_differences(
        prefix + divide_external_oracle_record, prefix + divide_missing_zero, 1
    )
    if len(failures) != 1 or evidence:
        raise RuntimeError("Oracle classifier ignored a missing defined DIV.U Z flag")

    divsd_identifier = (0x0814).to_bytes(2, "little")
    divsd_record = divsd_identifier + b"\x00" * 4 + b"\x02\x01"
    failures, evidence = classify_differences(
        prefix + divsd_record, prefix + divsd_record, 1
    )
    if failures or len(evidence) != 1 or "raw-equal=true" not in evidence[0]:
        raise RuntimeError("Oracle classifier treated B1 DIV.SD OV equality as exact")

    rcon_identifier = (0x0EFC).to_bytes(2, "little")
    rcon_external_oracle_record = (
        rcon_identifier
        + (0xCBFF).to_bytes(2, "little")
        + (0x4AA5).to_bytes(2, "little")
        + b"\x00\x00"
    )
    rcon_native_record = (
        rcon_identifier
        + (0xC3FF).to_bytes(2, "little")
        + (0x42A5).to_bytes(2, "little")
        + b"\x00\x00"
    )
    failures, evidence = classify_differences(
        prefix + rcon_external_oracle_record, prefix + rcon_native_record, 1
    )
    if failures or len(evidence) != 1:
        raise RuntimeError("Oracle classifier rejected the B1 VREGSF correction")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare dsPIC instruction behavior across simulators"
    )
    parser.add_argument("--verbose", action="store_true")
    arguments = parser.parse_args()
    verify_oracle_evidence_census()
    verify_classifier_regressions()
    verify_sfr_side_effect_coverage_regressions()
    started = time.perf_counter()
    print("[check] Device-pack SFR inventory", flush=True)
    print(run([sys.executable, str(SFR_MANIFEST_VERIFIER)]).rstrip(), flush=True)
    print("[check] Generated SFR access expectations", flush=True)
    sfr_access_generator_output = run(
        [sys.executable, str(SFR_ACCESS_GENERATOR), "--check"]
    ).rstrip()
    print(sfr_access_generator_output, flush=True)
    absence_match = re.search(
        r"^\[target-absence\] groups=([a-z0-9_,]+)$",
        sfr_access_generator_output,
        re.MULTILINE,
    )
    if absence_match is None:
        raise RuntimeError("Generated SFR access expectations omit target absences")
    absent_device_groups = tuple(absence_match.group(1).split(","))
    if absent_device_groups != TARGET_ABSENT_DEVICE_GROUPS:
        raise RuntimeError(
            f"Target-absent device groups are {absent_device_groups}, "
            f"expected {TARGET_ABSENT_DEVICE_GROUPS}"
        )
    if set(absent_device_groups) & set(DEVICE_CONFORMANCE_GROUPS):
        raise RuntimeError("Target-absent device groups overlap conformance groups")
    print("[prepare] Building simulator conformance firmware", flush=True)
    build()
    resolved = symbols()
    recorded_cases = case_count(resolved)
    terminal_cases = resolved.get("_system_conformance_terminal_count", 0)
    cases = recorded_cases + terminal_cases
    active_cpu_groups, complete_cpu_groups = group_progress(
        resolved, CPU_CONFORMANCE_GROUPS, NATIVE_COMPLETE_CPU_GROUPS
    )
    active_device_groups, complete_device_groups = group_progress(
        resolved, DEVICE_CONFORMANCE_GROUPS, NATIVE_COMPLETE_DEVICE_GROUPS
    )
    active_device_groups = sum(
        name in NATIVE_ACTIVE_DEVICE_GROUPS or f"_{name}_conformance_cases" in resolved
        for name in DEVICE_CONFORMANCE_GROUPS
    )
    complete_device_groups = sum(
        name in NATIVE_COMPLETE_DEVICE_GROUPS for name in DEVICE_CONFORMANCE_GROUPS
    )
    result_size = 2 + recorded_cases * 8
    cpu_group_total = len(CPU_CONFORMANCE_GROUPS)
    device_group_total = len(DEVICE_CONFORMANCE_GROUPS)
    print(
        f"[ready] mcu-cases={cases} "
        f"cpu-active={active_cpu_groups}/{cpu_group_total} "
        f"cpu-complete={complete_cpu_groups}/{cpu_group_total} "
        f"({100 * complete_cpu_groups / cpu_group_total:.1f}%) "
        f"device-active={active_device_groups}/{device_group_total} "
        f"device-complete={complete_device_groups}/{device_group_total} "
        f"({100 * complete_device_groups / device_group_total:.1f}%) "
        f"device-absent={len(absent_device_groups)} "
        f"bytes={result_size}",
        flush=True,
    )
    print("[load] Frozen external oracle", flush=True)
    reference, reference_probes = external_oracle_results(recorded_cases, result_size)
    print("[run] Native C emulator", flush=True)
    candidate = native_results(result_size, arguments.verbose)
    print("[run] Native C emulator system probes", flush=True)
    candidate_probes = native_system_probes(resolved, arguments.verbose)
    probe_failures, probe_oracle_evidence = compare_system_probes(
        reference_probes, candidate_probes, resolved
    )
    print(
        f"[system] probes={terminal_cases} "
        f"passed={terminal_cases - min(terminal_cases, len(probe_failures))} "
        f"failed={min(terminal_cases, len(probe_failures))}",
        flush=True,
    )
    print("[run] Native SFR reset snapshots", flush=True)
    candidate_sfr_por, candidate_sfr_mclr = native_sfr_reset_snapshots(
        arguments.verbose
    )
    sfr_reset_failures = compare_sfr_por_snapshot(candidate_sfr_por)
    sfr_reset_failures.extend(compare_sfr_mclr_snapshot(candidate_sfr_mclr))
    print("[run] Native SFR access census", flush=True)
    side_effect_coverage: dict[int, tuple[int, str]] = {}
    (
        _,
        sfr_access_summary,
        sfr_mux_summary,
        sfr_conditional_summary,
        side_effect_expectations,
    ) = native_sfr_access_census(arguments.verbose)
    sfr_access_failure_classes = sum(
        sfr_access_summary[f"{access_class}-bits"] != 0
        for access_class in SFR_ACCESS_CLASSES
    )
    sfr_access_failure_classes += sum(
        sfr_mux_summary[f"alternate-{access_class}-bits"] != 0
        for access_class in SFR_ACCESS_CLASSES
    )
    sfr_access_failure_classes += sfr_mux_summary["selector-reset-bits"] != 0
    sfr_access_failure_classes += sfr_mux_summary["selector-switch-bits"] != 0
    sfr_access_failure_classes += sfr_conditional_summary["unresolved-addresses"] != 0
    print("[run] Native processor semantics", flush=True)
    processor_cases, processor_failures = native_component_conformance(
        PROCESSOR_CONFORMANCE, "processor", arguments.verbose, side_effect_coverage
    )
    print("[run] Native event scheduler", flush=True)
    event_cases, event_failures = native_component_conformance(
        EVENT_CONFORMANCE, "event", arguments.verbose, side_effect_coverage
    )
    print("[run] Native DMA v3", flush=True)
    dma_cases, dma_failures = native_component_conformance(
        DMA_CONFORMANCE, "dma", arguments.verbose, side_effect_coverage
    )
    print("[run] Native timers", flush=True)
    timer_cases, timer_failures = native_component_conformance(
        TIMER_CONFORMANCE, "timer", arguments.verbose, side_effect_coverage
    )
    print("[run] Native ADC", flush=True)
    adc_cases, adc_failures = native_component_conformance(
        ADC_CONFORMANCE, "adc", arguments.verbose, side_effect_coverage
    )
    print("[run] Native high-speed PWM", flush=True)
    pwm_cases, pwm_failures = native_component_conformance(
        PWM_CONFORMANCE, "pwm", arguments.verbose, side_effect_coverage
    )
    print("[run] Native SPI", flush=True)
    spi_cases, spi_failures = native_component_conformance(
        SPI_CONFORMANCE, "spi", arguments.verbose, side_effect_coverage
    )
    print("[run] Native CAN", flush=True)
    can_cases, can_failures = native_component_conformance(
        CAN_CONFORMANCE, "can", arguments.verbose, side_effect_coverage
    )
    print("[run] Native USB", flush=True)
    usb_cases, usb_failures = native_component_conformance(
        USB_CONFORMANCE, "usb", arguments.verbose, side_effect_coverage
    )
    print("[run] Native UART", flush=True)
    uart_cases, uart_failures = native_component_conformance(
        UART_CONFORMANCE, "uart", arguments.verbose, side_effect_coverage
    )
    print("[run] Native I2C", flush=True)
    i2c_cases, i2c_failures = native_component_conformance(
        I2C_CONFORMANCE, "i2c", arguments.verbose, side_effect_coverage
    )
    print("[run] Native NVM", flush=True)
    nvm_cases, nvm_failures = native_component_conformance(
        NVM_CONFORMANCE, "nvm", arguments.verbose, side_effect_coverage
    )
    print("[run] Native CRC", flush=True)
    crc_cases, crc_failures = native_component_conformance(
        CRC_CONFORMANCE, "crc", arguments.verbose, side_effect_coverage
    )
    print("[run] Native PMP", flush=True)
    pmp_cases, pmp_failures = native_component_conformance(
        PMP_CONFORMANCE, "pmp", arguments.verbose, side_effect_coverage
    )
    print("[run] Native Input Capture", flush=True)
    input_capture_cases, input_capture_failures = native_component_conformance(
        INPUT_CAPTURE_CONFORMANCE,
        "input-capture",
        arguments.verbose,
        side_effect_coverage,
    )
    print("[run] Native Output Compare", flush=True)
    output_compare_cases, output_compare_failures = native_component_conformance(
        OUTPUT_COMPARE_CONFORMANCE,
        "output-compare",
        arguments.verbose,
        side_effect_coverage,
    )
    print("[run] Native QEI", flush=True)
    qei_cases, qei_failures = native_component_conformance(
        QEI_CONFORMANCE, "qei", arguments.verbose, side_effect_coverage
    )
    print("[run] Native DCI", flush=True)
    dci_cases, dci_failures = native_component_conformance(
        DCI_CONFORMANCE, "dci", arguments.verbose, side_effect_coverage
    )
    print("[run] Native GPIO", flush=True)
    gpio_cases, gpio_failures = native_component_conformance(
        GPIO_CONFORMANCE, "gpio", arguments.verbose, side_effect_coverage
    )
    print("[run] Native PPS", flush=True)
    pps_cases, pps_failures = native_component_conformance(
        PPS_CONFORMANCE, "pps", arguments.verbose, side_effect_coverage
    )
    print("[run] Native Auxiliary Clock", flush=True)
    auxiliary_clock_cases, auxiliary_clock_failures = native_component_conformance(
        AUXILIARY_CLOCK_CONFORMANCE,
        "auxiliary-clock",
        arguments.verbose,
        side_effect_coverage,
    )
    print("[run] Native Oscillator", flush=True)
    oscillator_cases, oscillator_failures = native_component_conformance(
        OSCILLATOR_CONFORMANCE,
        "oscillator",
        arguments.verbose,
        side_effect_coverage,
    )
    print("[run] Native Watchdog", flush=True)
    watchdog_cases, watchdog_failures = native_component_conformance(
        WATCHDOG_CONFORMANCE, "watchdog", arguments.verbose, side_effect_coverage
    )
    print("[run] Native Interrupt Control", flush=True)
    interrupt_control_cases, interrupt_control_failures = native_component_conformance(
        INTERRUPT_CONTROL_CONFORMANCE,
        "interrupt-control",
        arguments.verbose,
        side_effect_coverage,
    )
    print("[run] Native Core SFR", flush=True)
    core_sfr_cases, core_sfr_failures = native_component_conformance(
        CORE_SFR_CONFORMANCE, "core-sfr", arguments.verbose, side_effect_coverage
    )
    print("[run] Native Reset", flush=True)
    reset_cases, reset_failures = native_component_conformance(
        RESET_CONFORMANCE, "reset", arguments.verbose, side_effect_coverage
    )
    print("[run] Native Comparator", flush=True)
    comparator_cases, comparator_failures = native_component_conformance(
        COMPARATOR_CONFORMANCE, "comparator", arguments.verbose, side_effect_coverage
    )
    print("[run] Native RTCC", flush=True)
    rtcc_cases, rtcc_failures = native_component_conformance(
        RTCC_CONFORMANCE, "rtcc", arguments.verbose, side_effect_coverage
    )
    validate_sfr_side_effect_coverage(side_effect_expectations, side_effect_coverage)
    expected_words = recorded_cases * 4
    reference_words = int.from_bytes(reference[:2], "little")
    candidate_words = int.from_bytes(candidate[:2], "little")
    if reference_words != expected_words or candidate_words != expected_words:
        raise RuntimeError(
            f"Conformance program recorded {reference_words}/{candidate_words} "
            f"of {expected_words} expected words"
        )
    seen_case_ids: set[int] = set()
    duplicate_case_ids: set[int] = set()
    for index in range(recorded_cases):
        start = 2 + index * 8
        identifier = int.from_bytes(reference[start : start + 2], "little")
        if identifier in seen_case_ids:
            duplicate_case_ids.add(identifier)
        seen_case_ids.add(identifier)
    if duplicate_case_ids:
        formatted = ", ".join(f"0x{value:04x}" for value in sorted(duplicate_case_ids))
        raise RuntimeError(f"Duplicate conformance case identifiers: {formatted}")
    missing_oracle_cases = oracle_rule_identifiers() - seen_case_ids
    if missing_oracle_cases:
        formatted = ", ".join(
            f"0x{value:04x}" for value in sorted(missing_oracle_cases)
        )
        raise RuntimeError(f"Oracle rules reference missing cases: {formatted}")
    record_failures, oracle_evidence_records = classify_differences(
        reference, candidate, recorded_cases
    )
    classified_oracle_cases = {
        int(match.group(1), 16)
        for result in record_failures + oracle_evidence_records
        if (match := re.search(r"case=0x([0-9a-fA-F]{4})", result)) is not None
    }
    dormant_oracle_cases = oracle_rule_identifiers() - classified_oracle_cases
    if dormant_oracle_cases:
        formatted = ", ".join(
            f"0x{value:04x}" for value in sorted(dormant_oracle_cases)
        )
        raise RuntimeError(f"Dormant oracle rules: {formatted}")
    probe_failure_selectors = [
        re.search(r"system-probe=(\d+)", failure) for failure in probe_failures
    ]
    if any(match is None for match in probe_failure_selectors):
        raise RuntimeError("System probe failure has no selector")
    failed_probe_selectors = {
        int(match.group(1)) for match in probe_failure_selectors if match is not None
    }
    oracle_evidence_records.extend(probe_oracle_evidence)
    typed_failure_records = [
        failure
        for failure in record_failures + probe_failures
        if (
            (match := re.search(r"case=0x([0-9a-fA-F]{4})", failure)) is not None
            and int(match.group(1), 16) in oracle_rule_identifiers()
        )
        or (
            (match := re.search(r"system-probe=(\d+)", failure)) is not None
            and int(match.group(1)) in {71, 75}
        )
    ]
    oracle_ledger_sha256, oracle_classes, oracle_failed_classes = (
        write_oracle_evidence_ledger(
            oracle_evidence_records,
            typed_failure_records,
        )
    )
    candidate_failures = record_failures + probe_failures
    failures = candidate_failures
    if arguments.verbose:
        for record in oracle_evidence_records:
            print(f"[oracle-evidence] {record}")
    native_unit_cases = (
        processor_cases
        + event_cases
        + dma_cases
        + timer_cases
        + adc_cases
        + pwm_cases
        + spi_cases
        + can_cases
        + usb_cases
        + uart_cases
        + i2c_cases
        + nvm_cases
        + crc_cases
        + pmp_cases
        + input_capture_cases
        + output_compare_cases
        + qei_cases
        + dci_cases
        + gpio_cases
        + pps_cases
        + auxiliary_clock_cases
        + oscillator_cases
        + watchdog_cases
        + interrupt_control_cases
        + core_sfr_cases
        + reset_cases
        + comparator_cases
        + rtcc_cases
    )
    native_unit_failures = (
        processor_failures
        + event_failures
        + dma_failures
        + timer_failures
        + adc_failures
        + pwm_failures
        + spi_failures
        + can_failures
        + usb_failures
        + uart_failures
        + i2c_failures
        + nvm_failures
        + crc_failures
        + pmp_failures
        + input_capture_failures
        + output_compare_failures
        + qei_failures
        + dci_failures
        + gpio_failures
        + pps_failures
        + auxiliary_clock_failures
        + oscillator_failures
        + watchdog_failures
        + interrupt_control_failures
        + core_sfr_failures
        + reset_failures
        + comparator_failures
        + rtcc_failures
    )
    native_device_failures = {
        "interrupt": interrupt_control_failures,
        "event": event_failures,
        "dma": dma_failures,
        "timer": timer_failures,
        "adc": adc_failures,
        "pwm": pwm_failures,
        "spi": spi_failures,
        "can": can_failures,
        "usb": usb_failures,
        "uart": uart_failures,
        "i2c": i2c_failures,
        "nvm": nvm_failures,
        "crc": crc_failures,
        "pmp": pmp_failures,
        "input_capture": input_capture_failures,
        "output_compare": output_compare_failures,
        "qei": qei_failures,
        "dci": dci_failures,
        "gpio": gpio_failures,
        "pps": pps_failures,
        "auxiliary_clock": auxiliary_clock_failures,
        "oscillator": oscillator_failures,
        "watchdog": watchdog_failures,
        "comparator": comparator_failures,
        "rtcc": rtcc_failures,
        "sfr": len(sfr_reset_failures) + sfr_access_failure_classes + core_sfr_failures,
        "reset": reset_failures,
    }
    completed_devices = sum(
        name in NATIVE_COMPLETE_DEVICE_GROUPS
        and native_device_failures.get(name, 0) == 0
        for name in DEVICE_CONFORMANCE_GROUPS
    )
    external_cases = cases
    external_failures = min(
        len(record_failures) + len(failed_probe_selectors), external_cases
    )
    external_typed = min(
        len(oracle_evidence_records), external_cases - external_failures
    )
    external_exact = external_cases - external_failures - external_typed
    coverage_incomplete = (
        complete_cpu_groups != cpu_group_total
        or completed_devices != device_group_total
    )
    elapsed = time.perf_counter() - started
    print(
        f"[declared-groups] cpu={complete_cpu_groups}/{cpu_group_total} "
        f"({100 * complete_cpu_groups / cpu_group_total:.1f}%) "
        f"devices={completed_devices}/{device_group_total} "
        f"({100 * completed_devices / device_group_total:.1f}%)",
        flush=True,
    )
    print(
        f"[external-oracle] cases={external_cases} exact={external_exact} "
        f"typed={external_typed} failed={external_failures}",
        flush=True,
    )
    print(
        f"[external-oracle-evidence] path={ORACLE_EVIDENCE_LEDGER} "
        f"sha256={oracle_ledger_sha256} "
        f"documented={oracle_classes['documented']} "
        f"undefined={oracle_classes['undefined']} "
        f"external-oracle-limitations={oracle_classes['external-oracle-limitation']} "
        f"failed-documented={oracle_failed_classes['documented']} "
        f"failed-undefined={oracle_failed_classes['undefined']} "
        f"failed-external-oracle-limitations={oracle_failed_classes['external-oracle-limitation']}",
        flush=True,
    )
    print(
        f"[native-unit] assertions={native_unit_cases} "
        f"passed={native_unit_cases - native_unit_failures} "
        f"failed={native_unit_failures}",
        flush=True,
    )
    if (
        failures
        or coverage_incomplete
        or processor_failures != 0
        or event_failures != 0
        or dma_failures != 0
        or timer_failures != 0
        or adc_failures != 0
        or pwm_failures != 0
        or spi_failures != 0
        or can_failures != 0
        or usb_failures != 0
        or uart_failures != 0
        or i2c_failures != 0
        or nvm_failures != 0
        or crc_failures != 0
        or pmp_failures != 0
        or input_capture_failures != 0
        or output_compare_failures != 0
        or qei_failures != 0
        or dci_failures != 0
        or gpio_failures != 0
        or pps_failures != 0
        or auxiliary_clock_failures != 0
        or oscillator_failures != 0
        or watchdog_failures != 0
        or interrupt_control_failures != 0
        or core_sfr_failures != 0
        or reset_failures != 0
        or comparator_failures != 0
        or rtcc_failures != 0
        or sfr_reset_failures
        or sfr_access_failure_classes != 0
    ):
        for difference in failures[:8]:
            print(f"[failed] {difference}")
        if len(failures) > 8:
            print(f"[failed] {len(failures) - 8} additional differences")
        for difference in sfr_reset_failures[:8]:
            print(f"[failed] {difference}")
        if len(sfr_reset_failures) > 8:
            print(
                f"[failed] {len(sfr_reset_failures) - 8} additional SFR reset differences"
            )
        if not arguments.verbose:
            for record in oracle_evidence_records[:8]:
                print(f"[typed] {record}")
            if len(oracle_evidence_records) > 8:
                print(
                    f"[typed] {len(oracle_evidence_records) - 8} additional typed cases"
                )
        print(
            f"[summary] status=incomplete external-typed={external_typed} "
            f"external-failures={external_failures} "
            f"native-failures={native_unit_failures} "
            f"sfr-reset-failures={len(sfr_reset_failures)} "
            f"sfr-access-classes={sfr_access_failure_classes} "
            f"sfr-access-addresses={sfr_access_summary['unresolved-addresses']} "
            f"sfr-mux-addresses={sfr_mux_summary['unresolved-addresses']} "
            "sfr-conditional-addresses="
            f"{sfr_conditional_summary['unresolved-addresses']} "
            f"elapsed={elapsed:.1f}s"
        )
        return 1
    print(
        f"[summary] status=complete external-typed={external_typed} "
        "external-failures=0 "
        f"native-failures=0 sfr-reset-failures=0 sfr-access-classes=0 "
        "sfr-access-addresses=0 sfr-mux-addresses=0 "
        f"sfr-conditional-addresses=0 elapsed={elapsed:.1f}s"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
