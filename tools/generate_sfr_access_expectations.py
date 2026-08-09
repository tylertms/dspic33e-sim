import argparse
import gzip
import json
import sys
from collections import Counter
from pathlib import Path


EXPECTED_DEFINITIONS = 993
EXPECTED_ADDRESSES = 977
EXPECTED_ALIASES = 7
EXPECTED_MUX_DEFAULTS = 16
EXPECTED_ACCESS_BITS = {
    "-": 2933,
    "c": 167,
    "n": 10495,
    "r": 1874,
    "s": 38,
    "w": 125,
}


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


def load_defaults(path):
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
    for address, variants in sorted(grouped.items()):
        selected = [variant for variant in variants if variant["selector"] is None]
        if len(selected) != 1:
            raise ValueError(
                f"address 0x{address:04x} has {len(selected)} default definitions"
            )
        defaults.append(selected[0])
    access_bits = Counter(
        character for register in defaults for character in register["access"]
    )
    if dict(sorted(access_bits.items())) != EXPECTED_ACCESS_BITS:
        raise ValueError(f"default access bits are {dict(sorted(access_bits.items()))}")
    alias_count = sum(len(register["aliases"]) for register in defaults)
    mux_defaults = sum(register["kind"] == "mux_variant" for register in defaults)
    if alias_count != EXPECTED_ALIASES:
        raise ValueError(f"manifest has {alias_count} default aliases")
    if mux_defaults != EXPECTED_MUX_DEFAULTS:
        raise ValueError(f"manifest has {mux_defaults} mux defaults")
    return defaults


def render(defaults):
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
        "enum {",
        "    DSPIC33_SFR_ACCESS_MUX_DEFAULT = 0x01u,",
        "    DSPIC33_SFR_ACCESS_HAS_ALIAS = 0x02u,",
        "};",
        "",
        "static const Dspic33SfrAccessExpectation dspic33_sfr_access_expectations[] = {",
    ]
    for register in defaults:
        access = register["access"]
        flags = 0
        if register["kind"] == "mux_variant":
            flags |= 1
        if register["aliases"]:
            flags |= 2
        lines.append(
            "    {"
            f"{register['address']}u, "
            f"0x{pattern_mask(access, 'n'):04x}u, "
            f"0x{pattern_mask(access, 'r'):04x}u, "
            f"0x{pattern_mask(access, '-'):04x}u, "
            f"0x{pattern_mask(access, 'w'):04x}u, "
            f"0x{pattern_mask(access, 'cs'):04x}u, "
            f"0x{flags:02x}u, "
            f"{len(register['aliases'])}u"
            "},"
        )
    lines.extend(
        [
            "};",
            "",
            "enum {",
            f"    DSPIC33_SFR_ACCESS_DEFINITION_COUNT = {EXPECTED_DEFINITIONS}u,",
            f"    DSPIC33_SFR_ACCESS_ADDRESS_COUNT = {EXPECTED_ADDRESSES}u,",
            f"    DSPIC33_SFR_ACCESS_ALIAS_COUNT = {EXPECTED_ALIASES}u,",
            f"    DSPIC33_SFR_ACCESS_MUX_DEFAULT_COUNT = {EXPECTED_MUX_DEFAULTS}u,",
            f"    DSPIC33_SFR_ACCESS_NORMAL_BIT_COUNT = {EXPECTED_ACCESS_BITS['n']}u,",
            f"    DSPIC33_SFR_ACCESS_READ_ONLY_BIT_COUNT = {EXPECTED_ACCESS_BITS['r']}u,",
            f"    DSPIC33_SFR_ACCESS_RESERVED_BIT_COUNT = {EXPECTED_ACCESS_BITS['-']}u,",
            f"    DSPIC33_SFR_ACCESS_WRITE_ONLY_BIT_COUNT = {EXPECTED_ACCESS_BITS['w']}u,",
            f"    DSPIC33_SFR_ACCESS_SIDE_EFFECT_BIT_COUNT = {EXPECTED_ACCESS_BITS['c'] + EXPECTED_ACCESS_BITS['s']}u,",
            "};",
            "",
            "#endif",
            "",
        ]
    )
    return "\n".join(lines).encode("ascii")


def main():
    arguments = parse_arguments()
    rendered = render(load_defaults(arguments.manifest))
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
