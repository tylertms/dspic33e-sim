import argparse
import gzip
import hashlib
import json
import os
import sys
import xml.etree.ElementTree as ElementTree
from collections import Counter
from pathlib import Path


NAMESPACE = "http://crownking/edc"
PREFIX = f"{{{NAMESPACE}}}"
PACK_NAME = "Microchip.dsPIC33E-GM-GP-MC-GU-MU_DFP"
PACK_VERSION = "1.7.401"
MPLABX_VERSION = "6.35"
DEVICE_NAME = "DSPIC33EP512MU810"
SOURCE_SIZE = 1745713
SOURCE_SHA256 = "44b421dcb5f6cc84fe8a7b295ccda87eb04ed2d3c2836b800713c1ac9016c7a3"


def local_name(name):
    return name.rsplit("}", 1)[-1]


def attribute(element, name):
    return element.attrib.get(PREFIX + name)


def hexadecimal(value, digits=0):
    return f"0x{value:0{digits}x}"


def default_dfp_path():
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
        / f"{DEVICE_NAME}.PIC"
    )


def default_output_path():
    return (
        Path(__file__).resolve().parents[1]
        / "generated"
        / "dspic33ep512mu810_sfr_manifest.json.gz"
    )


def source_identity(path):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if len(data) != SOURCE_SIZE:
        raise ValueError(f"DFP size is {len(data)}, expected {SOURCE_SIZE}")
    if digest != SOURCE_SHA256:
        raise ValueError(f"DFP SHA-256 is {digest}, expected {SOURCE_SHA256}")
    return data


def placed_fields(mode):
    cursor = 0
    fields = []
    for child in mode:
        kind = local_name(child.tag)
        if kind == "AdjustPoint":
            cursor += int(attribute(child, "offset"), 0)
        elif kind == "SFRFieldDef":
            width = int(attribute(child, "nzwidth"), 0)
            mask = int(attribute(child, "mask"), 0)
            fields.append(
                {
                    "name": attribute(child, "cname"),
                    "offset": cursor,
                    "width": width,
                    "mask": hexadecimal(mask),
                    "placed_mask": hexadecimal((mask << cursor) & 0xFFFF, 4),
                    "hidden": attribute(child, "islanghidden") == "true",
                }
            )
            cursor += width
    return fields


def register_modes(register):
    modes = []
    for mode in register.findall(f"./{PREFIX}SFRModeList/{PREFIX}SFRMode"):
        modes.append({"id": attribute(mode, "id"), "fields": placed_fields(mode)})
    return modes


def register_aliases(register):
    aliases = []
    alias_list = register.find(f"./{PREFIX}AliasList")
    if alias_list is None:
        return aliases
    for alias in alias_list:
        aliases.append(
            {
                "name": attribute(alias, "cname"),
                "kind": local_name(alias.tag),
            }
        )
    return aliases


def register_kind(register, parents):
    parent = parents[register]
    kind = local_name(parent.tag)
    if kind == "SFRDataSector":
        return "direct", None, None
    if kind == "JoinedSFRDef":
        return "joined_member", attribute(parent, "cname"), None
    if kind == "SelectSFR":
        return "mux_variant", None, attribute(parent, "when")
    raise ValueError(f"unsupported SFR parent {kind}")


def register_record(register, parents):
    kind, joined_name, selector = register_kind(register, parents)
    address = int(attribute(register, "_addr"), 0)
    return {
        "name": attribute(register, "cname"),
        "address": hexadecimal(address, 4),
        "width": int(attribute(register, "nzwidth"), 0),
        "access": attribute(register, "access"),
        "impl": hexadecimal(int(attribute(register, "impl"), 0)),
        "por": attribute(register, "por"),
        "mclr": attribute(register, "mclr"),
        "kind": kind,
        "joined_name": joined_name,
        "selector": selector,
        "hidden": attribute(register, "ishidden") == "true",
        "module": attribute(register, "_modsrc"),
        "aliases": register_aliases(register),
        "modes": register_modes(register),
    }


def access_mask(access):
    result = 0
    for index, character in enumerate(access):
        if character != "-":
            result |= 1 << (15 - index)
    return result


def primary_field_mask(record):
    primary = next(mode for mode in record["modes"] if mode["id"] == "DS.0")
    result = 0
    for field in primary["fields"]:
        result |= int(field["placed_mask"], 0)
    return result


def summary(root, registers, joined, muxed):
    tags = Counter(local_name(element.tag) for element in root.iter())
    parents = {child: parent for parent in root.iter() for child in parent}
    access_bits = Counter(character for record in registers for character in record["access"])
    por_bits = Counter(character for record in registers for character in record["por"])
    mclr_bits = Counter(character for record in registers for character in record["mclr"])
    mode_counts = {}
    for record in registers:
        for mode in record["modes"]:
            counts = mode_counts.setdefault(mode["id"], {"modes": 0, "fields": 0})
            counts["modes"] += 1
            counts["fields"] += len(mode["fields"])
    relationships = Counter()
    access_field_equal = 0
    for record in registers:
        impl = int(record["impl"], 0)
        access = access_mask(record["access"])
        fields = primary_field_mask(record)
        access_field_equal += access == fields
        if impl == access == fields:
            relationships["mask_all_equal"] += 1
        elif access == fields:
            relationships["mask_field_access_equal"] += 1
        elif impl == fields:
            relationships["mask_impl_field_equal"] += 1
        elif impl == access:
            relationships["mask_impl_access_equal"] += 1
        else:
            relationships["mask_all_different"] += 1
    explicit_aliases = [alias for record in registers for alias in record["aliases"]]
    counts = {
        "xml_elements": sum(tags.values()),
        "xml_tags": len(tags),
        "sfr_definitions": len(registers),
        "sfr_addresses": len({record["address"] for record in registers}),
        "direct_definitions": sum(record["kind"] == "direct" for record in registers),
        "joined_definitions": len(joined),
        "joined_members": sum(record["kind"] == "joined_member" for record in registers),
        "muxed_definitions": len(muxed),
        "mux_variants": sum(record["kind"] == "mux_variant" for record in registers),
        "sfr_mode_lists": tags["SFRModeList"],
        "sfr_modes": tags["SFRMode"],
        "sfr_fields": tags["SFRFieldDef"],
        "sfr_mode_adjust_points": sum(
            1
            for element in root.iter()
            if local_name(element.tag) == "AdjustPoint"
            and local_name(parents[element].tag) == "SFRMode"
        ),
        "stim_info": tags["StimInfo"],
        "freeze_bits": tags["FreezeBit"],
        "legacy_aliases": sum(alias["kind"] == "LegacyAlias" for alias in explicit_aliases),
        "migration_aliases": sum(
            alias["kind"] == "MigrationAlias" for alias in explicit_aliases
        ),
        "explicit_aliases": len(explicit_aliases),
        "por_fixed_records": sum(set(record["por"]) <= set("01-") for record in registers),
        "por_nonfixed_records": sum(
            not set(record["por"]) <= set("01-") for record in registers
        ),
        "mclr_fixed_records": sum(
            set(record["mclr"]) <= set("01-") for record in registers
        ),
        "mclr_nonfixed_records": sum(
            not set(record["mclr"]) <= set("01-") for record in registers
        ),
        "por_mclr_equal": sum(record["por"] == record["mclr"] for record in registers),
        "por_mclr_different": sum(
            record["por"] != record["mclr"] for record in registers
        ),
        "mclr_unchanged_records": sum("u" in record["mclr"] for record in registers),
        "access_field_equal": access_field_equal,
        "access_field_different": len(registers) - access_field_equal,
        **relationships,
    }
    return {
        "counts": counts,
        "mode_counts": dict(sorted(mode_counts.items())),
        "access_bits": dict(sorted(access_bits.items())),
        "por_bits": dict(sorted(por_bits.items())),
        "mclr_bits": dict(sorted(mclr_bits.items())),
    }


def joined_records(root):
    records = []
    for joined in root.iter(PREFIX + "JoinedSFRDef"):
        members = [
            attribute(child, "cname")
            for child in joined
            if local_name(child.tag) == "SFRDef"
        ]
        records.append(
            {
                "name": attribute(joined, "cname"),
                "address": hexadecimal(int(attribute(joined, "_addr"), 0), 4),
                "width": int(attribute(joined, "nzwidth"), 0),
                "members": members,
            }
        )
    return sorted(records, key=lambda record: (int(record["address"], 0), record["name"]))


def muxed_records(root):
    records = []
    for muxed in root.iter(PREFIX + "MuxedSFRDef"):
        variants = []
        for selection in muxed.findall(f"./{PREFIX}SelectSFR"):
            register = selection.find(f"./{PREFIX}SFRDef")
            variants.append(
                {
                    "name": attribute(register, "cname"),
                    "selector": attribute(selection, "when"),
                }
            )
        records.append(
            {
                "address": hexadecimal(int(attribute(muxed, "_addr"), 0), 4),
                "width": int(attribute(muxed, "nzwidth"), 0),
                "variants": variants,
            }
        )
    return sorted(records, key=lambda record: int(record["address"], 0))


def manifest(path):
    source_identity(path)
    root = ElementTree.parse(path).getroot()
    if local_name(root.tag) != "PIC" or attribute(root, "name") != DEVICE_NAME:
        raise ValueError("DFP device identity does not match the pinned device")
    parents = {child: parent for parent in root.iter() for child in parent}
    registers = [
        register_record(register, parents)
        for register in root.iter(PREFIX + "SFRDef")
    ]
    registers.sort(key=lambda record: (int(record["address"], 0), record["name"]))
    joined = joined_records(root)
    muxed = muxed_records(root)
    return {
        "schema_version": 1,
        "source": {
            "pack": PACK_NAME,
            "pack_version": PACK_VERSION,
            "mplabx_version": MPLABX_VERSION,
            "file": f"edc/{DEVICE_NAME}.PIC",
            "size": SOURCE_SIZE,
            "sha256": SOURCE_SHA256,
        },
        "device": {
            "name": attribute(root, "name"),
            "architecture": attribute(root, "arch"),
            "processor_id": attribute(root, "procid"),
            "datasheet_id": attribute(root, "dsid"),
            "programming_specification_id": attribute(root, "psid"),
            "mask_set_id": attribute(root, "masksetid"),
            "namespace": NAMESPACE,
            "schema_location": root.attrib.get(
                "{http://www.w3.org/2001/XMLSchema-instance}schemaLocation"
            ),
        },
        "summary": summary(root, registers, joined, muxed),
        "joined": joined,
        "muxed": muxed,
        "registers": registers,
    }


def serialized_manifest(document):
    encoded = (json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n").encode(
        "utf-8"
    )
    return gzip.compress(encoded, compresslevel=9, mtime=0)


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dfp", type=Path, default=default_dfp_path())
    parser.add_argument("--output", type=Path, default=default_output_path())
    parser.add_argument("--check", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    try:
        generated = serialized_manifest(manifest(arguments.dfp))
        if arguments.check:
            current = arguments.output.read_bytes()
            if current != generated:
                raise ValueError(f"manifest is not current: {arguments.output}")
            print(f"SFR manifest is current: {arguments.output}")
            return 0
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_bytes(generated)
        print(f"Generated {arguments.output}")
        return 0
    except (OSError, ValueError, ElementTree.ParseError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
