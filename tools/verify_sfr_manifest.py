import argparse
import gzip
import hashlib
import json
import os
import sys
import xml.etree.ElementTree as ElementTree
from collections import Counter
from pathlib import Path


URI = "http://crownking/edc"
EDC = f"{{{URI}}}"
DEVICE = "DSPIC33EP512MU810"
PACK = "Microchip.dsPIC33E-GM-GP-MC-GU-MU_DFP"
PACK_VERSION = "1.7.401"
MPLABX_VERSION = "6.35"
DFP_SIZE = 1745713
DFP_SHA256 = "44b421dcb5f6cc84fe8a7b295ccda87eb04ed2d3c2836b800713c1ac9016c7a3"
EXPECTED_COUNTS = {
    "access_field_different": 46,
    "access_field_equal": 947,
    "direct_definitions": 949,
    "explicit_aliases": 7,
    "freeze_bits": 60,
    "joined_definitions": 5,
    "joined_members": 12,
    "legacy_aliases": 6,
    "mask_all_different": 9,
    "mask_all_equal": 568,
    "mask_field_access_equal": 379,
    "mask_impl_access_equal": 5,
    "mask_impl_field_equal": 32,
    "mclr_fixed_records": 499,
    "mclr_nonfixed_records": 494,
    "mclr_unchanged_records": 48,
    "migration_aliases": 1,
    "mux_variants": 32,
    "muxed_definitions": 16,
    "por_fixed_records": 593,
    "por_mclr_different": 133,
    "por_mclr_equal": 860,
    "por_nonfixed_records": 400,
    "sfr_addresses": 977,
    "sfr_definitions": 993,
    "sfr_fields": 8412,
    "sfr_mode_adjust_points": 1662,
    "sfr_mode_lists": 998,
    "sfr_modes": 1457,
    "stim_info": 788,
    "xml_elements": 16484,
    "xml_tags": 80,
}
EXPECTED_ACCESS_BITS = {"-": 2945, "c": 167, "n": 10739, "r": 1874, "s": 38, "w": 125}
EXPECTED_POR_BITS = {"-": 2830, "0": 7539, "1": 507, "x": 5009, "y": 3}
EXPECTED_MCLR_BITS = {"-": 2830, "0": 6349, "1": 505, "u": 729, "x": 5472, "y": 3}
EXPECTED_MODE_COUNTS = {
    "DS.0": {"fields": 5097, "modes": 993},
    "DS.1": {"fields": 14, "modes": 6},
    "DS.2": {"fields": 4, "modes": 1},
    "DS.3": {"fields": 3, "modes": 1},
    "LT.0": {"fields": 3198, "modes": 440},
    "LT.1": {"fields": 96, "modes": 16},
}


def tag_name(value):
    return value.rsplit("}", 1)[-1]


def edc_value(element, name):
    return element.attrib.get(EDC + name)


def hex_value(value, digits=0):
    return f"0x{value:0{digits}x}"


def installed_dfp():
    program_files = Path(os.environ.get("ProgramFiles", "C:/Program Files"))
    return (
        program_files
        / "Microchip"
        / "MPLABX"
        / f"v{MPLABX_VERSION}"
        / "packs"
        / "Microchip"
        / "dsPIC33E-GM-GP-MC-GU-MU_DFP"
        / PACK_VERSION
        / "edc"
        / f"{DEVICE}.PIC"
    )


def checked_in_manifest():
    return (
        Path(__file__).resolve().parents[1]
        / "generated"
        / "dspic33ep512mu810_sfr_manifest.json.gz"
    )


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dfp", type=Path, default=installed_dfp())
    parser.add_argument("--manifest", type=Path, default=checked_in_manifest())
    return parser.parse_args()


def verify_source(path):
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if len(raw) != DFP_SIZE:
        raise ValueError(f"DFP size is {len(raw)}, expected {DFP_SIZE}")
    if digest != DFP_SHA256:
        raise ValueError(f"DFP SHA-256 is {digest}, expected {DFP_SHA256}")


def extract_fields(mode):
    position = 0
    fields = []
    for node in list(mode):
        if tag_name(node.tag) == "AdjustPoint":
            position += int(edc_value(node, "offset"), 0)
            continue
        if tag_name(node.tag) != "SFRFieldDef":
            continue
        width = int(edc_value(node, "nzwidth"), 0)
        mask = int(edc_value(node, "mask"), 0)
        fields.append(
            {
                "name": edc_value(node, "cname"),
                "offset": position,
                "width": width,
                "mask": hex_value(mask),
                "placed_mask": hex_value((mask << position) & 0xFFFF, 4),
                "hidden": edc_value(node, "islanghidden") == "true",
            }
        )
        position += width
    return fields


def extract_modes(register):
    result = []
    mode_list = register.find(f"./{EDC}SFRModeList")
    if mode_list is None:
        return result
    for mode in list(mode_list):
        if tag_name(mode.tag) == "SFRMode":
            result.append({"id": edc_value(mode, "id"), "fields": extract_fields(mode)})
    return result


def extract_aliases(register):
    result = []
    alias_list = register.find(f"./{EDC}AliasList")
    if alias_list is None:
        return result
    for alias in list(alias_list):
        result.append({"name": edc_value(alias, "cname"), "kind": tag_name(alias.tag)})
    return result


def extract_register(register, parents):
    parent = parents[register]
    parent_type = tag_name(parent.tag)
    if parent_type == "SFRDataSector":
        kind = "direct"
        joined_name = None
        selector = None
    elif parent_type == "JoinedSFRDef":
        kind = "joined_member"
        joined_name = edc_value(parent, "cname")
        selector = None
    elif parent_type == "SelectSFR":
        kind = "mux_variant"
        joined_name = None
        selector = edc_value(parent, "when")
    else:
        raise ValueError(f"unsupported SFR parent {parent_type}")
    return {
        "name": edc_value(register, "cname"),
        "address": hex_value(int(edc_value(register, "_addr"), 0), 4),
        "width": int(edc_value(register, "nzwidth"), 0),
        "access": edc_value(register, "access"),
        "impl": hex_value(int(edc_value(register, "impl"), 0)),
        "por": edc_value(register, "por"),
        "mclr": edc_value(register, "mclr"),
        "kind": kind,
        "joined_name": joined_name,
        "selector": selector,
        "hidden": edc_value(register, "ishidden") == "true",
        "module": edc_value(register, "_modsrc"),
        "aliases": extract_aliases(register),
        "modes": extract_modes(register),
    }


def extract_joined(root):
    result = []
    for joined in root.iter(EDC + "JoinedSFRDef"):
        result.append(
            {
                "name": edc_value(joined, "cname"),
                "address": hex_value(int(edc_value(joined, "_addr"), 0), 4),
                "width": int(edc_value(joined, "nzwidth"), 0),
                "members": [
                    edc_value(child, "cname")
                    for child in list(joined)
                    if tag_name(child.tag) == "SFRDef"
                ],
            }
        )
    return sorted(result, key=lambda item: (int(item["address"], 0), item["name"]))


def extract_muxed(root):
    result = []
    for muxed in root.iter(EDC + "MuxedSFRDef"):
        variants = []
        for selection in list(muxed):
            if tag_name(selection.tag) != "SelectSFR":
                continue
            register = next(
                child for child in list(selection) if tag_name(child.tag) == "SFRDef"
            )
            variants.append(
                {
                    "name": edc_value(register, "cname"),
                    "selector": edc_value(selection, "when"),
                }
            )
        result.append(
            {
                "address": hex_value(int(edc_value(muxed, "_addr"), 0), 4),
                "width": int(edc_value(muxed, "nzwidth"), 0),
                "variants": variants,
            }
        )
    return sorted(result, key=lambda item: int(item["address"], 0))


def bit_mask(access):
    mask = 0
    for position, value in enumerate(access):
        if value != "-":
            mask |= 1 << (15 - position)
    return mask


def ds_field_mask(register):
    mode = next(mode for mode in register["modes"] if mode["id"] == "DS.0")
    return sum(int(field["placed_mask"], 0) for field in mode["fields"])


def calculate_summary(root, registers, joined, muxed):
    all_nodes = list(root.iter())
    parents = {child: parent for parent in all_nodes for child in list(parent)}
    tags = Counter(tag_name(node.tag) for node in all_nodes)
    modes = Counter()
    mode_fields = Counter()
    for register in registers:
        for mode in register["modes"]:
            modes[mode["id"]] += 1
            mode_fields[mode["id"]] += len(mode["fields"])
    relationships = Counter()
    access_field_equal = 0
    for register in registers:
        implemented = int(register["impl"], 0)
        access = bit_mask(register["access"])
        fields = ds_field_mask(register)
        access_field_equal += access == fields
        if implemented == access == fields:
            relationships["mask_all_equal"] += 1
        elif fields == access:
            relationships["mask_field_access_equal"] += 1
        elif implemented == fields:
            relationships["mask_impl_field_equal"] += 1
        elif implemented == access:
            relationships["mask_impl_access_equal"] += 1
        else:
            relationships["mask_all_different"] += 1
    aliases = [alias for register in registers for alias in register["aliases"]]
    counts = {
        "xml_elements": len(all_nodes),
        "xml_tags": len(tags),
        "sfr_definitions": len(registers),
        "sfr_addresses": len({register["address"] for register in registers}),
        "direct_definitions": sum(register["kind"] == "direct" for register in registers),
        "joined_definitions": len(joined),
        "joined_members": sum(
            register["kind"] == "joined_member" for register in registers
        ),
        "muxed_definitions": len(muxed),
        "mux_variants": sum(register["kind"] == "mux_variant" for register in registers),
        "sfr_mode_lists": tags["SFRModeList"],
        "sfr_modes": tags["SFRMode"],
        "sfr_fields": tags["SFRFieldDef"],
        "sfr_mode_adjust_points": sum(
            tag_name(node.tag) == "AdjustPoint"
            and tag_name(parents[node].tag) == "SFRMode"
            for node in all_nodes
        ),
        "stim_info": tags["StimInfo"],
        "freeze_bits": tags["FreezeBit"],
        "legacy_aliases": sum(alias["kind"] == "LegacyAlias" for alias in aliases),
        "migration_aliases": sum(
            alias["kind"] == "MigrationAlias" for alias in aliases
        ),
        "explicit_aliases": len(aliases),
        "por_fixed_records": sum(set(register["por"]) <= set("01-") for register in registers),
        "por_nonfixed_records": sum(
            not set(register["por"]) <= set("01-") for register in registers
        ),
        "mclr_fixed_records": sum(
            set(register["mclr"]) <= set("01-") for register in registers
        ),
        "mclr_nonfixed_records": sum(
            not set(register["mclr"]) <= set("01-") for register in registers
        ),
        "por_mclr_equal": sum(register["por"] == register["mclr"] for register in registers),
        "por_mclr_different": sum(
            register["por"] != register["mclr"] for register in registers
        ),
        "mclr_unchanged_records": sum("u" in register["mclr"] for register in registers),
        "access_field_equal": access_field_equal,
        "access_field_different": len(registers) - access_field_equal,
        **relationships,
    }
    return {
        "counts": counts,
        "mode_counts": {
            identifier: {"modes": modes[identifier], "fields": mode_fields[identifier]}
            for identifier in sorted(modes)
        },
        "access_bits": dict(
            sorted(Counter(value for register in registers for value in register["access"]).items())
        ),
        "por_bits": dict(
            sorted(Counter(value for register in registers for value in register["por"]).items())
        ),
        "mclr_bits": dict(
            sorted(Counter(value for register in registers for value in register["mclr"]).items())
        ),
    }


def assert_equal(actual, expected, label):
    if actual != expected:
        raise ValueError(f"{label} does not match the pinned DFP")


def validate_registers(registers):
    names = [register["name"] for register in registers]
    if len(names) != len(set(names)):
        raise ValueError("manifest register names are not unique")
    canonical = sorted(registers, key=lambda item: (int(item["address"], 0), item["name"]))
    if registers != canonical:
        raise ValueError("manifest register order is not canonical")
    for register in registers:
        address = int(register["address"], 0)
        if address & 1 or address >= 0x1000:
            raise ValueError(f"invalid SFR address for {register['name']}")
        if len(register["access"]) != 16 or not set(register["access"]) <= set("-nrcsw"):
            raise ValueError(f"invalid access pattern for {register['name']}")
        if len(register["por"]) != 16 or not set(register["por"]) <= set("-01xy"):
            raise ValueError(f"invalid POR pattern for {register['name']}")
        if len(register["mclr"]) != 16 or not set(register["mclr"]) <= set("-01xyu"):
            raise ValueError(f"invalid MCLR pattern for {register['name']}")
        primary_modes = [mode for mode in register["modes"] if mode["id"] == "DS.0"]
        if len(primary_modes) != 1:
            raise ValueError(f"{register['name']} does not have exactly one DS.0 mode")


def verify_manifest(dfp_path, manifest_path):
    verify_source(dfp_path)
    document = json.loads(gzip.decompress(manifest_path.read_bytes()).decode("utf-8"))
    root = ElementTree.parse(dfp_path).getroot()
    parents = {child: parent for parent in root.iter() for child in list(parent)}
    registers = [
        extract_register(register, parents) for register in root.iter(EDC + "SFRDef")
    ]
    registers.sort(key=lambda item: (int(item["address"], 0), item["name"]))
    joined = extract_joined(root)
    muxed = extract_muxed(root)
    summary = calculate_summary(root, registers, joined, muxed)
    expected_source = {
        "pack": PACK,
        "pack_version": PACK_VERSION,
        "mplabx_version": MPLABX_VERSION,
        "file": f"edc/{DEVICE}.PIC",
        "size": DFP_SIZE,
        "sha256": DFP_SHA256,
    }
    expected_device = {
        "name": edc_value(root, "name"),
        "architecture": edc_value(root, "arch"),
        "processor_id": edc_value(root, "procid"),
        "datasheet_id": edc_value(root, "dsid"),
        "programming_specification_id": edc_value(root, "psid"),
        "mask_set_id": edc_value(root, "masksetid"),
        "namespace": URI,
        "schema_location": root.attrib.get(
            "{http://www.w3.org/2001/XMLSchema-instance}schemaLocation"
        ),
    }
    assert_equal(document.get("schema_version"), 1, "schema version")
    assert_equal(document.get("source"), expected_source, "source identity")
    assert_equal(document.get("device"), expected_device, "device identity")
    assert_equal(document.get("registers"), registers, "register inventory")
    assert_equal(document.get("joined"), joined, "joined-register inventory")
    assert_equal(document.get("muxed"), muxed, "mux-register inventory")
    assert_equal(document.get("summary"), summary, "computed summary")
    assert_equal(summary["counts"], EXPECTED_COUNTS, "exact inventory counts")
    assert_equal(summary["access_bits"], EXPECTED_ACCESS_BITS, "access-bit counts")
    assert_equal(summary["por_bits"], EXPECTED_POR_BITS, "POR-bit counts")
    assert_equal(summary["mclr_bits"], EXPECTED_MCLR_BITS, "MCLR-bit counts")
    assert_equal(summary["mode_counts"], EXPECTED_MODE_COUNTS, "mode counts")
    validate_registers(document["registers"])
    return summary


def main():
    arguments = parse_arguments()
    try:
        summary = verify_manifest(arguments.dfp, arguments.manifest)
        counts = summary["counts"]
        print(
            "Verified SFR manifest: "
            f"{counts['sfr_definitions']} definitions, "
            f"{counts['sfr_addresses']} addresses, "
            f"{counts['explicit_aliases']} aliases, "
            f"{counts['mux_variants']} mux variants"
        )
        return 0
    except (OSError, ValueError, KeyError, StopIteration, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
