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
    ("U1EIR", 0x04C4): 0x0040,
}
DOCUMENTED_SIDE_EFFECT_OVERRIDE_BITS = sum(
    mask.bit_count() for mask in DOCUMENTED_SIDE_EFFECT_OVERRIDES.values()
)
MUX_SELECTOR_PATTERN = re.compile(
    r"^\(\$0x([0-9a-f]+) & 0x([0-9a-f]+)\) == 0x([0-9a-f]+)$"
)


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


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=default_manifest())
    parser.add_argument("--output", type=Path, default=default_output())
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
    override = DOCUMENTED_SIDE_EFFECT_OVERRIDES.get(
        (register["name"], address), 0
    )
    if override & ~normal:
        raise ValueError(
            f"documented side-effect override is not DFP-normal for "
            f"{register['name']} at 0x{address:04x}"
        )
    return {
        "normal": normal & ~override,
        "read_only": pattern_mask(access, "r"),
        "reserved": pattern_mask(access, "-"),
        "write_only": pattern_mask(access, "w"),
        "side_effect": side_effect | override,
    }


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
    documented_overrides = {
        (register["name"], int(register["address"], 16))
        for register in defaults
        if (register["name"], int(register["address"], 16))
        in DOCUMENTED_SIDE_EFFECT_OVERRIDES
    }
    if documented_overrides != set(DOCUMENTED_SIDE_EFFECT_OVERRIDES):
        raise ValueError(
            f"documented side-effect overrides found are {sorted(documented_overrides)}"
        )
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
    return defaults, muxes


def render(defaults, muxes):
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
        "    uint16_t reserved;",
        "    uint16_t write_only;",
        "    uint16_t side_effect;",
        "    uint8_t flags;",
        "    uint8_t aliases;",
        "} Dspic33SfrAccessExpectation;",
        "",
        "typedef struct {",
        "    uint16_t address;",
        "    uint16_t selector_address;",
        "    uint16_t selector_mask;",
        "    uint16_t selector_value;",
        "    uint16_t selector_reset;",
        "    uint16_t normal;",
        "    uint16_t read_only;",
        "    uint16_t reserved;",
        "    uint16_t write_only;",
        "    uint16_t side_effect;",
        "} Dspic33SfrMuxAccessExpectation;",
        "",
        "enum {",
        "    DSPIC33_SFR_ACCESS_MUX_DEFAULT = 0x01u,",
        "    DSPIC33_SFR_ACCESS_HAS_ALIAS = 0x02u,",
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
        lines.append(
            "    {"
            f"{register['address']}u, "
            f"0x{masks['normal']:04x}u, "
            f"0x{masks['read_only']:04x}u, "
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
            f"0x{masks['reserved']:04x}u, "
            f"0x{masks['write_only']:04x}u,\n"
            f"     0x{masks['side_effect']:04x}u"
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
            f"    DSPIC33_SFR_ACCESS_NORMAL_BIT_COUNT = {EXPECTED_ACCESS_BITS['n'] - DOCUMENTED_SIDE_EFFECT_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_ACCESS_READ_ONLY_BIT_COUNT = {EXPECTED_ACCESS_BITS['r']}u,",
            f"    DSPIC33_SFR_ACCESS_RESERVED_BIT_COUNT = {EXPECTED_ACCESS_BITS['-']}u,",
            f"    DSPIC33_SFR_ACCESS_WRITE_ONLY_BIT_COUNT = {EXPECTED_ACCESS_BITS['w']}u,",
            f"    DSPIC33_SFR_ACCESS_SIDE_EFFECT_BIT_COUNT = {EXPECTED_ACCESS_BITS['c'] + EXPECTED_ACCESS_BITS['s'] + DOCUMENTED_SIDE_EFFECT_OVERRIDE_BITS}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_NORMAL_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('n', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_READ_ONLY_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('r', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_RESERVED_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('-', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_WRITE_ONLY_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('w', 0)}u,",
            f"    DSPIC33_SFR_MUX_ACCESS_SIDE_EFFECT_BIT_COUNT = {EXPECTED_MUX_ACCESS_BITS.get('c', 0) + EXPECTED_MUX_ACCESS_BITS.get('s', 0)}u,",
            "};",
            "",
            "#endif",
            "",
        ]
    )
    return "\n".join(lines).encode("ascii")


def main():
    arguments = parse_arguments()
    rendered = render(*load_inventory(arguments.manifest))
    if arguments.check:
        if not arguments.output.exists() or arguments.output.read_bytes() != rendered:
            print(
                f"SFR access expectations are stale: {arguments.output}",
                file=sys.stderr,
            )
            return 1
        print(f"SFR access expectations are current: {arguments.output}")
        return 0
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(rendered)
    print(f"Generated SFR access expectations: {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
