import argparse
import gzip
import json
import re
import sys
from collections import Counter
from pathlib import Path


EXPECTED_DEFINITIONS = 993
EXPECTED_ADDRESSES = 977
EXPECTED_ALIASES = (
    ("CRCDATL", "CRCDAT", "LegacyAlias"),
    ("CRCWDATL", "CRCWDAT", "LegacyAlias"),
    ("PMADDR", "PMDOUT1", "MigrationAlias"),
    ("QEI1GECH", "QEI1ICH", "LegacyAlias"),
    ("QEI1GECL", "QEI1ICL", "LegacyAlias"),
    ("QEI2GECH", "QEI2ICH", "LegacyAlias"),
    ("QEI2GECL", "QEI2ICL", "LegacyAlias"),
)
EXPECTED_MUX_DEFAULTS = 16
EXPECTED_MUX_ALTERNATES = 16
EXPECTED_CONDITIONALS = 68
EXPECTED_CONDITIONAL_NORMAL_BITS = 1024
EXPECTED_CONDITIONAL_RESERVED_BITS = 64
EXPECTED_IMPLEMENTED_WORDS = 977
EXPECTED_ABSENT_WORDS = 1071
EXPECTED_ABSENT_RANGES = 77
EXPECTED_ACCESS_BITS = {
    "-": 2933,
    "c": 167,
    "n": 10495,
    "r": 1874,
    "s": 38,
    "w": 125,
}
EXPECTED_MUX_ACCESS_BITS = {
    "-": 12,
    "n": 244,
}
DOCUMENTED_SIDE_EFFECT_OVERRIDES = {
    ("I2C1CON", 0x0206): 0x101E,
    ("I2C2CON", 0x0216): 0x101E,
    ("U1STA", 0x0222): 0x0C00,
    ("U2STA", 0x0232): 0x0C00,
    ("SPI1BUF", 0x0248): 0xFFFF,
    ("U3STA", 0x0252): 0x0C00,
    ("SPI2BUF", 0x0268): 0xFFFF,
    ("SPI3BUF", 0x02A8): 0xFFFF,
    ("U4STA", 0x02B2): 0x0C00,
    ("SPI4BUF", 0x02C8): 0xFFFF,
    ("C1CTRL1", 0x0400): 0x1000,
    ("C2CTRL1", 0x0500): 0x1000,
    ("ALRMVAL", 0x0620): 0xFFFF,
    ("ALCFGRPT", 0x0622): 0x83FF,
    ("RTCVAL", 0x0624): 0xFFFF,
    ("RCFGCAL", 0x0626): 0xA000,
    ("OSCCON", 0x0742): 0x0001,
    ("DISICNT", 0x0052): 0x3FFF,
    ("INTCON2", 0x08C2): 0x2000,
    ("INTCON3", 0x08C4): 0x0070,
    ("INTCON4", 0x08C6): 0x0001,
    ("U1EIR", 0x04C4): 0x0040,
    ("QEI1STAT", 0x01C4): 0x2AAA,
    ("VEL1CNT", 0x01CC): 0xFFFF,
    ("QEI2STAT", 0x05C4): 0x2AAA,
    ("VEL2CNT", 0x05CC): 0xFFFF,
}
DOCUMENTED_SPLIT_ACCESS_OVERRIDES = {
    ("PORTA", 0x0E02): 0xC6FF,
    ("PORTB", 0x0E12): 0xFFFF,
    ("PORTC", 0x0E22): 0xF01E,
    ("PORTD", 0x0E32): 0xFFFF,
    ("PORTE", 0x0E42): 0x03FF,
    ("PORTF", 0x0E52): 0x313F,
    ("PORTG", 0x0E62): 0xF3C3,
}
DOCUMENTED_NORMAL_OVERRIDES = {
    ("IEC8", 0x0830): 0x7FC0,
}
DOCUMENTED_WRITABLE_OVERRIDES = {
    ("INTCON1", 0x08C0): 0x78C0,
}
DOCUMENTED_READ_ONLY_SIDE_EFFECT_OVERRIDES = {
    ("SR", 0x0042): 0xC800,
}
DOCUMENTED_READ_ONLY_OVERRIDES = {
    ("ACLKCON3", 0x0758): 0x4000,
    ("C1EC", 0x040E): 0xFFFF,
    ("C2EC", 0x050E): 0xFFFF,
    ("U1PWRC", 0x0490): 0x0080,
    ("U1STAT", 0x04C8): 0x00F0,
    ("U1FRML", 0x04D0): 0x00FF,
    ("U1FRMH", 0x04D2): 0x0007,
    ("RCFGCAL", 0x0626): 0x1800,
    ("DMAPWC", 0x0BF0): 0x7FFF,
    ("DMARQC", 0x0BF2): 0x7FFF,
    ("DMAPPS", 0x0BF4): 0x7FFF,
    ("DMALCA", 0x0BF6): 0x000F,
    ("QEI1IOC", 0x01C2): 0x000F,
    ("QEI2IOC", 0x05C2): 0x000F,
    ("PTCON", 0x0C00): 0x1000,
    ("STCON", 0x0C0E): 0x1000,
    ("PORTG", 0x0E62): 0x000C,
    ("INTTREG", 0x08C8): 0x00FF,
    ("CORCON", 0x0044): 0x0004,
}
DOCUMENTED_DEPENDENT_READ_ONLY_OVERRIDES = {
    ("ACCAU", 0x0026): 0xFF00,
    ("ACCBU", 0x002C): 0xFF00,
    ("C1CTRL1", 0x0400): 0x00E0,
    ("C2CTRL1", 0x0500): 0x00E0,
    ("QEI1IOC", 0x01C2): 0x000F,
    ("QEI2IOC", 0x05C2): 0x000F,
}
DOCUMENTED_DEPENDENT_NORMAL_OVERRIDES = {
    ("AD1CON1", 0x0320): (0x0008, 0x0320, 0x0400, 0x0000, 0x0400),
}
DOCUMENTED_PROTECTED_NORMAL_OVERRIDES = {
    ("OSCCON", 0x0742): 0x0742,
}
DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDES = {
    ("OSCCON", 0x0742): 0x0080,
}
DOCUMENTED_RESERVED_OVERRIDES = {
    ("ACLKCON3", 0x0758): 0x0018,
    ("AD2CON2", 0x0362): 0x0040,
    ("C1INTE", 0x040C): 0x0010,
    ("C2INTE", 0x050C): 0x0010,
}
DOCUMENTED_DEVICE_MODE_RESERVED_OVERRIDES = {
    ("U1IE", 0x04C2): 0x0040,
}
DOCUMENTED_WRITE_ONLY_OVERRIDES = {
    ("CRCDATL", 0x0648): 0xFFFF,
    ("CRCDATH", 0x064A): 0xFFFF,
}
DOCUMENTED_SIDE_EFFECT_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_SIDE_EFFECT_OVERRIDES.values()
)
DOCUMENTED_SPLIT_ACCESS_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_SPLIT_ACCESS_OVERRIDES.values()
)
DOCUMENTED_NORMAL_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_NORMAL_OVERRIDES.values()
)
DOCUMENTED_WRITABLE_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_WRITABLE_OVERRIDES.values()
)
DOCUMENTED_READ_ONLY_SIDE_EFFECT_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_READ_ONLY_SIDE_EFFECT_OVERRIDES.values()
)
DOCUMENTED_READ_ONLY_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_READ_ONLY_OVERRIDES.values()
)
DOCUMENTED_DEPENDENT_READ_ONLY_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_DEPENDENT_READ_ONLY_OVERRIDES.values()
)
DOCUMENTED_DEPENDENT_NORMAL_OVERRIDE_BITS = sum(
    descriptor[0].bit_count()
    for descriptor in DOCUMENTED_DEPENDENT_NORMAL_OVERRIDES.values()
)
DOCUMENTED_PROTECTED_NORMAL_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_PROTECTED_NORMAL_OVERRIDES.values()
)
DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDES.values()
)
DOCUMENTED_WRITE_ONLY_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_WRITE_ONLY_OVERRIDES.values()
)
DOCUMENTED_RESERVED_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_RESERVED_OVERRIDES.values()
)
DOCUMENTED_DEVICE_MODE_RESERVED_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_DEVICE_MODE_RESERVED_OVERRIDES.values()
)
MUX_SELECTOR_PATTERN = re.compile(
    r"^\(\$0x([0-9a-f]+) & 0x([0-9a-f]+)\) == 0x([0-9a-f]+)$"
)


def can_window_registers():
    registers = [
        (0x24, "BUFPNT3"),
        (0x26, "BUFPNT4"),
        (0x38, "RXM2SID"),
        (0x3A, "RXM2EID"),
    ]
    for filter_index in range(1, 16):
        registers.extend(
            (
                (0x40 + filter_index * 4, f"RXF{filter_index}SID"),
                (0x42 + filter_index * 4, f"RXF{filter_index}EID"),
            )
        )
    return tuple(registers)


def default_manifest():
    return (
        Path(__file__).resolve().parents[1]
        / "generated"
        / "dspic33ep512mu810_sfr_manifest.json.gz"
    )


def default_output():
    return (
        Path(__file__).resolve().parents[1]
        / "generated"
        / "dspic33ep512mu810_sfr_access.h"
    )


def default_map_output():
    return (
        Path(__file__).resolve().parents[1]
        / "generated"
        / "dspic33ep512mu810_sfr_map.h"
    )


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=default_manifest())
    parser.add_argument("--output", type=Path, default=default_output())
    parser.add_argument("--map-output", type=Path, default=default_map_output())
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def pattern_mask(pattern, accepted):
    return sum(
        1 << (15 - index)
        for index, character in enumerate(pattern)
        if character in accepted
    )


def access_masks(register):
    access = register["access"]
    normal = pattern_mask(access, "n")
    side_effect = pattern_mask(access, "cs")
    address = int(register["address"], 16)
    identity = (register["name"], address)
    normal_override = DOCUMENTED_NORMAL_OVERRIDES.get(identity, 0)
    writable_override = DOCUMENTED_WRITABLE_OVERRIDES.get(identity, 0)
    read_only_side_effect_override = DOCUMENTED_READ_ONLY_SIDE_EFFECT_OVERRIDES.get(
        identity, 0
    )
    side_effect_override = DOCUMENTED_SIDE_EFFECT_OVERRIDES.get(identity, 0)
    split_access_override = DOCUMENTED_SPLIT_ACCESS_OVERRIDES.get(identity, 0)
    read_only_override = DOCUMENTED_READ_ONLY_OVERRIDES.get(identity, 0)
    dependent_read_only_override = DOCUMENTED_DEPENDENT_READ_ONLY_OVERRIDES.get(
        identity, 0
    )
    dependent_normal = DOCUMENTED_DEPENDENT_NORMAL_OVERRIDES.get(identity)
    dependent_normal_override = dependent_normal[0] if dependent_normal else 0
    protected_normal_override = DOCUMENTED_PROTECTED_NORMAL_OVERRIDES.get(identity, 0)
    protected_set_only_override = DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDES.get(
        identity, 0
    )
    write_only_override = DOCUMENTED_WRITE_ONLY_OVERRIDES.get(identity, 0)
    reserved_override = DOCUMENTED_RESERVED_OVERRIDES.get(
        identity, 0
    ) | DOCUMENTED_DEVICE_MODE_RESERVED_OVERRIDES.get(identity, 0)
    override = (
        side_effect_override
        | split_access_override
        | dependent_normal_override
        | protected_normal_override
        | protected_set_only_override
        | read_only_override
        | write_only_override
        | reserved_override
    )
    if normal_override & ~pattern_mask(access, "-"):
        raise ValueError(
            f"documented normal override is not DFP-reserved for "
            f"{register['name']} at 0x{address:04x}"
        )
    if writable_override & ~pattern_mask(access, "r"):
        raise ValueError(
            f"documented writable override is not DFP-read-only for "
            f"{register['name']} at 0x{address:04x}"
        )
    if read_only_side_effect_override & ~pattern_mask(access, "r"):
        raise ValueError(
            f"documented read-only side-effect override is not DFP-read-only for "
            f"{register['name']} at 0x{address:04x}"
        )
    if (
        read_only_side_effect_override
        & (writable_override | read_only_override | dependent_read_only_override)
        or side_effect_override & read_only_override
        or side_effect_override & split_access_override
        or side_effect_override & dependent_normal_override
        or side_effect_override & write_only_override
        or split_access_override & dependent_normal_override
        or split_access_override & read_only_override
        or split_access_override & write_only_override
        or dependent_normal_override & read_only_override
        or dependent_normal_override & write_only_override
        or protected_normal_override
        & (
            side_effect_override
            | split_access_override
            | dependent_normal_override
            | protected_set_only_override
            | read_only_override
            | write_only_override
        )
        or protected_set_only_override
        & (
            side_effect_override
            | split_access_override
            | dependent_normal_override
            | read_only_override
            | write_only_override
        )
        or read_only_override & write_only_override
        or reserved_override
        & (
            side_effect_override
            | split_access_override
            | dependent_normal_override
            | protected_normal_override
            | protected_set_only_override
            | read_only_override
            | write_only_override
        )
    ):
        raise ValueError(
            f"documented access overrides overlap for {register['name']} "
            f"at 0x{address:04x}"
        )
    if override & ~normal:
        raise ValueError(
            f"documented access override is not DFP-normal for "
            f"{register['name']} at 0x{address:04x}"
        )
    if dependent_read_only_override & ~(pattern_mask(access, "r") | read_only_override):
        raise ValueError(
            f"documented dependent read-only override is not read-only for "
            f"{register['name']} at 0x{address:04x}"
        )
    return {
        "normal": (normal & ~override) | normal_override | writable_override,
        "read_only": (
            pattern_mask(access, "r")
            & ~writable_override
            & ~read_only_side_effect_override
        )
        | read_only_override,
        "dependent_read_only": dependent_read_only_override,
        "reserved": (pattern_mask(access, "-") & ~normal_override) | reserved_override,
        "write_only": pattern_mask(access, "w") | write_only_override,
        "side_effect": side_effect
        | side_effect_override
        | read_only_side_effect_override,
        "split_access": split_access_override,
        "dependent_normal": dependent_normal_override,
        "protected_normal": protected_normal_override,
        "protected_set_only": protected_set_only_override,
    }


def validate_documented_overrides(defaults):
    identities = {
        (register["name"], int(register["address"], 16)) for register in defaults
    }
    registers_by_address = {
        int(register["address"], 16): register for register in defaults
    }
    expected = (
        set(DOCUMENTED_SIDE_EFFECT_OVERRIDES)
        | set(DOCUMENTED_SPLIT_ACCESS_OVERRIDES)
        | set(DOCUMENTED_DEPENDENT_NORMAL_OVERRIDES)
        | set(DOCUMENTED_PROTECTED_NORMAL_OVERRIDES)
        | set(DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDES)
        | set(DOCUMENTED_NORMAL_OVERRIDES)
        | set(DOCUMENTED_WRITABLE_OVERRIDES)
        | set(DOCUMENTED_READ_ONLY_SIDE_EFFECT_OVERRIDES)
        | set(DOCUMENTED_READ_ONLY_OVERRIDES)
        | set(DOCUMENTED_DEPENDENT_READ_ONLY_OVERRIDES)
        | set(DOCUMENTED_WRITE_ONLY_OVERRIDES)
        | set(DOCUMENTED_RESERVED_OVERRIDES)
        | set(DOCUMENTED_DEVICE_MODE_RESERVED_OVERRIDES)
    )
    found = identities & expected
    if found != expected:
        raise ValueError(f"documented access overrides found are {sorted(found)}")
    for identity, descriptor in DOCUMENTED_DEPENDENT_NORMAL_OVERRIDES.items():
        _, selector_address, selector_mask, normal_value, restricted_value = descriptor
        selector = registers_by_address.get(selector_address)
        if selector is None or selector_mask & ~pattern_mask(selector["access"], "n"):
            raise ValueError(f"dependent normal selector is invalid for {identity[0]}")
        if (normal_value | restricted_value) & ~selector_mask:
            raise ValueError(
                f"dependent normal selector value is invalid for {identity[0]}"
            )
        if normal_value == restricted_value:
            raise ValueError(
                f"dependent normal selector states match for {identity[0]}"
            )


def load_inventory(path):
    document = json.loads(gzip.decompress(path.read_bytes()).decode("utf-8"))
    registers = document["registers"]
    if len(registers) != EXPECTED_DEFINITIONS:
        raise ValueError(
            f"manifest has {len(registers)} definitions, expected {EXPECTED_DEFINITIONS}"
        )
    grouped = {}
    for register in registers:
        address = int(register["address"], 16)
        grouped.setdefault(address, []).append(register)
    if len(grouped) != EXPECTED_ADDRESSES:
        raise ValueError(
            f"manifest has {len(grouped)} addresses, expected {EXPECTED_ADDRESSES}"
        )
    defaults = []
    alternates = []
    for address, variants in sorted(grouped.items()):
        selected = [variant for variant in variants if variant["selector"] is None]
        if len(selected) != 1:
            raise ValueError(
                f"address 0x{address:04x} has {len(selected)} default definitions"
            )
        defaults.append(selected[0])
        alternates.extend(
            (address, variant)
            for variant in variants
            if variant["selector"] is not None
        )
    access_bits = Counter(
        character for register in defaults for character in register["access"]
    )
    if dict(sorted(access_bits.items())) != EXPECTED_ACCESS_BITS:
        raise ValueError(f"default access bits are {dict(sorted(access_bits.items()))}")
    aliases = tuple(
        sorted(
            (register["name"], alias["name"], alias["kind"])
            for register in registers
            for alias in register["aliases"]
        )
    )
    mux_defaults = sum(register["kind"] == "mux_variant" for register in defaults)
    if aliases != EXPECTED_ALIASES:
        raise ValueError(f"manifest aliases are {aliases}")
    if mux_defaults != EXPECTED_MUX_DEFAULTS:
        raise ValueError(f"manifest has {mux_defaults} mux defaults")
    if len(alternates) != EXPECTED_MUX_ALTERNATES:
        raise ValueError(f"manifest has {len(alternates)} mux alternates")
    alternate_access_bits = Counter(
        character for _, register in alternates for character in register["access"]
    )
    if dict(sorted(alternate_access_bits.items())) != EXPECTED_MUX_ACCESS_BITS:
        raise ValueError(
            f"mux alternate access bits are {dict(sorted(alternate_access_bits.items()))}"
        )
    validate_documented_overrides(defaults)
    muxes = []
    defaults_by_address = {
        int(register["address"], 16): register for register in defaults
    }
    for address, register in alternates:
        match = MUX_SELECTOR_PATTERN.fullmatch(register["selector"])
        if match is None:
            raise ValueError(f"unsupported mux selector {register['selector']}")
        selector_offset, selector_mask, selector_value = (
            int(value, 16) for value in match.groups()
        )
        selector_address = (address & 0xFF00) + selector_offset
        if selector_address not in (0x0400, 0x0500):
            raise ValueError(
                f"unexpected mux selector address 0x{selector_address:04x}"
            )
        selector_register = defaults_by_address[selector_address]
        selector_known = pattern_mask(selector_register["por"], "01")
        selector_reset = pattern_mask(selector_register["por"], "1")
        if selector_mask & ~selector_known:
            raise ValueError(
                f"mux selector reset is unknown at 0x{selector_address:04x}"
            )
        if selector_reset & selector_mask == selector_value:
            raise ValueError(f"mux alternate is selected at reset for 0x{address:04x}")
        muxes.append(
            {
                "register": register,
                "selector_address": selector_address,
                "selector_mask": selector_mask,
                "selector_value": selector_value,
                "selector_reset": selector_reset & selector_mask,
            }
        )
    conditionals = []
    for channel, selector_address in enumerate((0x0400, 0x0500), start=1):
        selector_register = defaults_by_address[selector_address]
        selector_known = pattern_mask(selector_register["por"], "01")
        selector_reset = pattern_mask(selector_register["por"], "1") & 0x0001
        if selector_known & 0x0001 == 0 or selector_reset != 0:
            raise ValueError(
                f"CAN window selector reset is invalid at 0x{selector_address:04x}"
            )
        for offset, suffix in can_window_registers():
            address = selector_address + offset
            register = defaults_by_address[address]
            expected_name = f"C{channel}{suffix}"
            if register["name"] != expected_name or register["kind"] != "direct":
                raise ValueError(
                    f"conditional register at 0x{address:04x} is "
                    f"{register['name']} ({register['kind']})"
                )
            conditionals.append(
                {
                    "register": register,
                    "selector_address": selector_address,
                    "selector_mask": 0x0001,
                    "selector_value": 0x0001,
                    "selector_reset": selector_reset,
                }
            )
    conditional_normal_bits = sum(
        access_masks(conditional["register"])["normal"].bit_count()
        for conditional in conditionals
    )
    conditional_reserved_bits = sum(
        access_masks(conditional["register"])["reserved"].bit_count()
        for conditional in conditionals
    )
    if len(conditionals) != EXPECTED_CONDITIONALS:
        raise ValueError(f"manifest has {len(conditionals)} conditional registers")
    if conditional_normal_bits != EXPECTED_CONDITIONAL_NORMAL_BITS:
        raise ValueError(
            f"conditional registers have {conditional_normal_bits} normal bits"
        )
    if conditional_reserved_bits != EXPECTED_CONDITIONAL_RESERVED_BITS:
        raise ValueError(
            f"conditional registers have {conditional_reserved_bits} reserved bits"
        )
    return defaults, muxes, conditionals


def implementation_bitmap(defaults):
    slots = [False] * 2048
    for register in defaults:
        address = int(register["address"], 16)
        if address >= 0x1000 or address & 1 or register["width"] != 16:
            raise ValueError(
                f"invalid SFR map entry {register['name']} at 0x{address:04x}"
            )
        slots[address >> 1] = True
    implemented_words = sum(slots)
    absent_words = len(slots) - implemented_words
    absent_ranges = sum(
        not implemented and (index == 0 or slots[index - 1])
        for index, implemented in enumerate(slots)
    )
    if implemented_words != EXPECTED_IMPLEMENTED_WORDS:
        raise ValueError(f"SFR map has {implemented_words} implemented words")
    if absent_words != EXPECTED_ABSENT_WORDS:
        raise ValueError(f"SFR map has {absent_words} absent words")
    if absent_ranges != EXPECTED_ABSENT_RANGES:
        raise ValueError(f"SFR map has {absent_ranges} absent ranges")
    return bytes(
        sum(slots[byte * 8 + bit] << bit for bit in range(8)) for byte in range(256)
    )


def render_map(defaults):
    bitmap = implementation_bitmap(defaults)
    lines = [
        "#ifndef DSPIC33EP512MU810_SFR_MAP_H",
        "#define DSPIC33EP512MU810_SFR_MAP_H",
        "",
        "#include <stdint.h>",
        "",
        "enum {",
        "    DSPIC33_SFR_WORD_COUNT = 2048u,",
        "    DSPIC33_SFR_IMPLEMENTATION_BITMAP_SIZE = 256u,",
        f"    DSPIC33_SFR_IMPLEMENTED_WORD_COUNT = {EXPECTED_IMPLEMENTED_WORDS}u,",
        f"    DSPIC33_SFR_ABSENT_WORD_COUNT = {EXPECTED_ABSENT_WORDS}u,",
        f"    DSPIC33_SFR_ABSENT_RANGE_COUNT = {EXPECTED_ABSENT_RANGES}u,",
        "};",
        "",
        "static const uint8_t dspic33_sfr_implementation_bitmap[] = {",
    ]
    for offset in range(0, len(bitmap), 12):
        values = ", ".join(f"0x{value:02x}u" for value in bitmap[offset : offset + 12])
        lines.append(f"    {values},")
    lines.extend(["};", "", "#endif", ""])
    return "\n".join(lines).encode("ascii")


def render(defaults, muxes, conditionals):
    conditional_identities = {
        (conditional["register"]["name"], conditional["register"]["address"])
        for conditional in conditionals
    }
    lines = [
        "#ifndef DSPIC33EP512MU810_SFR_ACCESS_H",
        "#define DSPIC33EP512MU810_SFR_ACCESS_H",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "    uint16_t address;",
        "    uint16_t normal;",
        "    uint16_t read_only;",
        "    uint16_t dependent_read_only;",
        "    uint16_t reserved;",
        "    uint16_t write_only;",
        "    uint16_t side_effect;",
        "    uint8_t flags;",
        "    uint8_t aliases;",
        "} Dspic33SfrAccessExpectation;",
        "",
        "typedef struct {",
        "    uint16_t address;",
        "    uint16_t mask;",
        "} Dspic33SfrSplitAccessExpectation;",
        "",
        "typedef struct {",
        "    uint16_t address;",
        "    uint16_t normal;",
        "    uint16_t set_only;",
        "} Dspic33SfrProtectedAccessExpectation;",
        "",
        "typedef struct {",
        "    uint16_t address;",
        "    uint16_t mask;",
        "    uint16_t selector_address;",
        "    uint16_t selector_mask;",
        "    uint16_t normal_value;",
        "    uint16_t restricted_value;",
        "} Dspic33SfrDependentNormalExpectation;",
        "",
        "typedef struct {",
        "    uint16_t address;",
        "    uint16_t selector_address;",
        "    uint16_t selector_mask;",
        "    uint16_t selector_value;",
        "    uint16_t selector_reset;",
        "    uint16_t normal;",
        "    uint16_t read_only;",
        "    uint16_t dependent_read_only;",
        "    uint16_t reserved;",
        "    uint16_t write_only;",
        "    uint16_t side_effect;",
        "} Dspic33SfrMuxAccessExpectation;",
        "",
        "typedef struct {",
        "    uint16_t address;",
        "    uint16_t selector_address;",
        "    uint16_t selector_mask;",
        "    uint16_t selector_value;",
        "    uint16_t selector_reset;",
        "    uint16_t normal;",
        "    uint16_t read_only;",
        "    uint16_t dependent_read_only;",
        "    uint16_t reserved;",
        "    uint16_t write_only;",
        "    uint16_t side_effect;",
        "} Dspic33SfrConditionalAccessExpectation;",
        "",
        "enum {",
        "    DSPIC33_SFR_ACCESS_MUX_DEFAULT = 0x01u,",
        "    DSPIC33_SFR_ACCESS_HAS_ALIAS = 0x02u,",
        "    DSPIC33_SFR_ACCESS_CONDITIONAL = 0x04u,",
        "};",
        "",
        "static const Dspic33SfrAccessExpectation dspic33_sfr_access_expectations[] = {",
    ]
    for register in defaults:
        masks = access_masks(register)
        flags = 0
        if register["kind"] == "mux_variant":
            flags |= 1
        if register["aliases"]:
            flags |= 2
        if (register["name"], register["address"]) in conditional_identities:
            flags |= 4
        lines.append(
            "    {"
            f"{register['address']}u, "
            f"0x{masks['normal']:04x}u, "
            f"0x{masks['read_only']:04x}u, "
            f"0x{masks['dependent_read_only']:04x}u, "
            f"0x{masks['reserved']:04x}u, "
            f"0x{masks['write_only']:04x}u, "
            f"0x{masks['side_effect']:04x}u, "
            f"0x{flags:02x}u, "
            f"{len(register['aliases'])}u"
            "},"
        )
    lines.extend(
        [
            "};",
            "",
            "static const Dspic33SfrSplitAccessExpectation dspic33_sfr_split_access_expectations[] =",
            "    {",
        ]
    )
    split_access_entries = []
    for register in defaults:
        masks = access_masks(register)
        if masks["split_access"] != 0:
            split_access_entries.append(
                f"{{{register['address']}u, 0x{masks['split_access']:04x}u}},"
            )
    for offset in range(0, len(split_access_entries), 4):
        lines.append("        " + " ".join(split_access_entries[offset : offset + 4]))
    lines.extend(
        [
            "};",
            "",
            "static const Dspic33SfrProtectedAccessExpectation",
            "    dspic33_sfr_protected_access_expectations[] = {",
        ]
    )
    for register in defaults:
        masks = access_masks(register)
        if masks["protected_normal"] != 0 or masks["protected_set_only"] != 0:
            lines.append(
                f"        {{{register['address']}u, "
                f"0x{masks['protected_normal']:04x}u, "
                f"0x{masks['protected_set_only']:04x}u}},"
            )
    lines.extend(
        [
            "};",
            "",
            "static const Dspic33SfrDependentNormalExpectation",
            "    dspic33_sfr_dependent_normal_expectations[] = {",
        ]
    )
    for register in defaults:
        masks = access_masks(register)
        if masks["dependent_normal"] != 0:
            descriptor = DOCUMENTED_DEPENDENT_NORMAL_OVERRIDES[
                (register["name"], int(register["address"], 16))
            ]
            lines.append(
                f"        {{{register['address']}u, 0x{masks['dependent_normal']:04x}u, "
                f"0x{descriptor[1]:04x}u, 0x{descriptor[2]:04x}u, "
                f"0x{descriptor[3]:04x}u, 0x{descriptor[4]:04x}u}},"
            )
    lines.extend(
        [
            "};",
            "",
            "static const Dspic33SfrMuxAccessExpectation dspic33_sfr_mux_access_expectations[] = {",
        ]
    )
    for mux in muxes:
        register = mux["register"]
        masks = access_masks(register)
        lines.append(
            "    {"
            f"{register['address']}u, "
            f"0x{mux['selector_address']:04x}u, "
            f"0x{mux['selector_mask']:04x}u, "
            f"0x{mux['selector_value']:04x}u, "
            f"0x{mux['selector_reset']:04x}u, "
            f"0x{masks['normal']:04x}u, "
            f"0x{masks['read_only']:04x}u, "
            f"0x{masks['dependent_read_only']:04x}u, "
            f"0x{masks['reserved']:04x}u,\n"
            f"     0x{masks['write_only']:04x}u, "
            f"0x{masks['side_effect']:04x}u"
            "},"
        )
    lines.extend(
        [
            "};",
            "",
            "static const Dspic33SfrConditionalAccessExpectation",
            "    dspic33_sfr_conditional_access_expectations[] = {",
        ]
    )
    for conditional in conditionals:
        register = conditional["register"]
        masks = access_masks(register)
        lines.append(
            "        {"
            f"{register['address']}u, "
            f"0x{conditional['selector_address']:04x}u, "
            f"0x{conditional['selector_mask']:04x}u, "
            f"0x{conditional['selector_value']:04x}u, "
            f"0x{conditional['selector_reset']:04x}u, "
            f"0x{masks['normal']:04x}u, "
            f"0x{masks['read_only']:04x}u, "
            f"0x{masks['dependent_read_only']:04x}u,\n"
            f"         0x{masks['reserved']:04x}u, "
            f"0x{masks['write_only']:04x}u, "
            f"0x{masks['side_effect']:04x}u"
            "},"
        )
    lines.extend(
        [
            "};",
            "",
            "enum {",
            f"    DSPIC33_SFR_ACCESS_DEFINITION_COUNT = {EXPECTED_DEFINITIONS}u,",
            f"    DSPIC33_SFR_ACCESS_ADDRESS_COUNT = {EXPECTED_ADDRESSES}u,",
            f"    DSPIC33_SFR_ACCESS_ALIAS_COUNT = {len(EXPECTED_ALIASES)}u,",
            f"    DSPIC33_SFR_ACCESS_MUX_DEFAULT_COUNT = {EXPECTED_MUX_DEFAULTS}u,",
            f"    DSPIC33_SFR_ACCESS_MUX_ALTERNATE_COUNT = {EXPECTED_MUX_ALTERNATES}u,",
            f"    DSPIC33_SFR_ACCESS_CONDITIONAL_COUNT = {EXPECTED_CONDITIONALS}u,",
            f"    DSPIC33_SFR_ACCESS_SPLIT_ACCESS_ADDRESS_COUNT = {len(DOCUMENTED_SPLIT_ACCESS_OVERRIDES)}u,",
            f"    DSPIC33_SFR_ACCESS_DEPENDENT_NORMAL_ADDRESS_COUNT = {len(DOCUMENTED_DEPENDENT_NORMAL_OVERRIDES)}u,",
            f"    DSPIC33_SFR_ACCESS_PROTECTED_ADDRESS_COUNT = {len(set(DOCUMENTED_PROTECTED_NORMAL_OVERRIDES) | set(DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDES))}u,",
            f"    DSPIC33_SFR_ACCESS_NORMAL_BIT_COUNT = {EXPECTED_ACCESS_BITS['n'] + DOCUMENTED_NORMAL_OVERRIDE_BITS + DOCUMENTED_WRITABLE_OVERRIDE_BITS - DOCUMENTED_SIDE_EFFECT_OVERRIDE_BITS - DOCUMENTED_SPLIT_ACCESS_OVERRIDE_BITS - DOCUMENTED_DEPENDENT_NORMAL_OVERRIDE_BITS - DOCUMENTED_PROTECTED_NORMAL_OVERRIDE_BITS - DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDE_BITS - DOCUMENTED_READ_ONLY_OVERRIDE_BITS - DOCUMENTED_WRITE_ONLY_OVERRIDE_BITS - DOCUMENTED_RESERVED_OVERRIDE_BITS - DOCUMENTED_DEVICE_MODE_RESERVED_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_READ_ONLY_BIT_COUNT = {EXPECTED_ACCESS_BITS['r'] - DOCUMENTED_WRITABLE_OVERRIDE_BITS - DOCUMENTED_READ_ONLY_SIDE_EFFECT_OVERRIDE_BITS + DOCUMENTED_READ_ONLY_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_DEPENDENT_READ_ONLY_BIT_COUNT = {DOCUMENTED_DEPENDENT_READ_ONLY_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_RESERVED_BIT_COUNT = {EXPECTED_ACCESS_BITS['-'] - DOCUMENTED_NORMAL_OVERRIDE_BITS + DOCUMENTED_RESERVED_OVERRIDE_BITS + DOCUMENTED_DEVICE_MODE_RESERVED_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_WRITE_ONLY_BIT_COUNT = {EXPECTED_ACCESS_BITS['w'] + DOCUMENTED_WRITE_ONLY_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_SIDE_EFFECT_BIT_COUNT = {EXPECTED_ACCESS_BITS['c'] + EXPECTED_ACCESS_BITS['s'] + DOCUMENTED_SIDE_EFFECT_OVERRIDE_BITS + DOCUMENTED_READ_ONLY_SIDE_EFFECT_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_SPLIT_ACCESS_BIT_COUNT = {DOCUMENTED_SPLIT_ACCESS_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_DEPENDENT_NORMAL_BIT_COUNT = {DOCUMENTED_DEPENDENT_NORMAL_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_PROTECTED_NORMAL_BIT_COUNT = {DOCUMENTED_PROTECTED_NORMAL_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_PROTECTED_SET_ONLY_BIT_COUNT = {DOCUMENTED_PROTECTED_SET_ONLY_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_NORMAL_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('n', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_READ_ONLY_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('r', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_RESERVED_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('-', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_WRITE_ONLY_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('w', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_SIDE_EFFECT_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('c', 0) + EXPECTED_MUX_ACCESS_BITS.get('s', 0)}u,",
            f"    DSPIC33_SFR_CONDITIONAL_ACCESS_NORMAL_BIT_COUNT = {EXPECTED_CONDITIONAL_NORMAL_BITS}u,",
            "    DSPIC33_SFR_CONDITIONAL_ACCESS_READ_ONLY_BIT_COUNT = 0u,",
            f"    DSPIC33_SFR_CONDITIONAL_ACCESS_RESERVED_BIT_COUNT = {EXPECTED_CONDITIONAL_RESERVED_BITS}u,",
            "    DSPIC33_SFR_CONDITIONAL_ACCESS_WRITE_ONLY_BIT_COUNT = 0u,",
            "    DSPIC33_SFR_CONDITIONAL_ACCESS_SIDE_EFFECT_BIT_COUNT = 0u,",
            "};",
            "",
            "#endif",
            "",
        ]
    )
    return "\n".join(lines).encode("ascii")


def main():
    arguments = parse_arguments()
    defaults, muxes, conditionals = load_inventory(arguments.manifest)
    rendered = render(defaults, muxes, conditionals)
    rendered_map = render_map(defaults)
    if arguments.check:
        if not arguments.output.exists() or arguments.output.read_bytes() != rendered:
            print(
                f"SFR access expectations are stale: {arguments.output}",
                file=sys.stderr,
            )
            return 1
        if (
            not arguments.map_output.exists()
            or arguments.map_output.read_bytes() != rendered_map
        ):
            print(
                f"SFR implementation map is stale: {arguments.map_output}",
                file=sys.stderr,
            )
            return 1
        print(
            f"SFR access expectations and implementation map are current: "
            f"{arguments.output}, {arguments.map_output}"
        )
        return 0
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(rendered)
    arguments.map_output.parent.mkdir(parents=True, exist_ok=True)
    arguments.map_output.write_bytes(rendered_map)
    print(
        f"Generated SFR access expectations and implementation map: "
        f"{arguments.output}, {arguments.map_output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
