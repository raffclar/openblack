#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Compares two saved worlds of openblack (.obsav) and tells which parts differ, in words.

A save that the game makes at once after a load must have the same bytes as the save that it loaded. When it does not,
this tool shows where: each block of the file by its tag; in the entities block (ECSW) each component, each field and
each entity; in the systems block (SYST) each system's section with the first byte that differs. The other blocks show
the places of the bytes that differ.

    python tools/sim/save_diff.py first.obsav second.obsav
    python tools/sim/save_diff.py first.obsav second.obsav --json      (the result as JSON)
    python tools/sim/save_diff.py --synced server.obsav client.obsav   (only the synced parts of two world states)
"""

import argparse
import json
import os
import struct
import sys

WORLD_MAGIC = b"OBSV"
ENTITIES_MAGIC = b"OBEW"
ATTEMPTS_VERSION = 4
# The bytes that a line of the report shows of each value that differs
SHOWN_BYTES = 24


class Reader:
    """Reads the numbers and the names of a block, little end first"""

    def __init__(self, data):
        self.data = data
        self.at = 0

    def u32(self):
        value = struct.unpack_from("<I", self.data, self.at)[0]
        self.at += 4
        return value

    def take(self, count):
        if self.at + count > len(self.data):
            raise ValueError("the block ends early")
        value = self.data[self.at:self.at + count]
        self.at += count
        return value

    def name(self):
        return self.take(self.u32()).decode("utf-8", errors="replace")

    def left(self):
        return len(self.data) - self.at


def read_world(data):
    """The head of a saved world and its blocks in order: ({"version", "attempts"}, [(tag, bytes)])"""
    reader = Reader(data)
    if reader.take(4) != WORLD_MAGIC:
        raise ValueError("not a saved world of openblack")
    head = {"version": reader.u32()}
    if head["version"] >= ATTEMPTS_VERSION:
        head["attempts"] = reader.u32()
    blocks = []
    for _ in range(reader.u32()):
        tag = reader.take(4).decode("ascii", errors="replace")
        blocks.append((tag, bytes(reader.take(reader.u32()))))
    return head, blocks


def read_entities(data):
    """The entities block: {"entities": [ids], "in_use": n, "components": {name: {"fields": [...], "rows": [(entity,
    [field bytes])]}}, "order": [component names]}"""
    reader = Reader(data)
    if reader.take(4) != ENTITIES_MAGIC:
        raise ValueError("not an entities block")
    result = {"version": reader.u32()}
    count = reader.u32()
    result["in_use"] = reader.u32()
    result["entities"] = [reader.u32() for _ in range(count)]
    components = {}
    order = []
    for _ in range(reader.u32()):
        name = reader.name()
        block = Reader(reader.take(reader.u32()))
        fields = [block.name() for _ in range(block.u32())]
        rows = []
        for _ in range(block.u32()):
            entity = block.u32()
            rows.append((entity, [bytes(block.take(block.u32())) for _ in fields]))
        components[name] = {"fields": fields, "rows": rows}
        order.append(name)
    result["components"] = components
    result["order"] = order
    return result


def read_sections(data):
    """The sections of a block of named sections, such as the systems block: [(name, bytes)]"""
    reader = Reader(data)
    sections = []
    for _ in range(reader.u32()):
        name = reader.name()
        sections.append((name, bytes(reader.take(reader.u32()))))
    if reader.left() != 0:
        raise ValueError("bytes after the last section")
    return sections


def entity_text(entity):
    """An entity as its index and its version"""
    return f"{entity & 0xFFFFF}v{entity >> 20}"


def byte_places(a, b):
    """The first byte that differs, the count of the bytes that differ in the common part, and the two sizes"""
    common = min(len(a), len(b))
    places = [i for i in range(common) if a[i] != b[i]]
    first = places[0] if places else (common if len(a) != len(b) else None)
    return {"first": first, "count": len(places), "sizes": [len(a), len(b)]}


def window(data, at):
    """Some bytes of a value from a place, as hexadecimal text"""
    return data[at:at + SHOWN_BYTES].hex(" ")


def compare_bytes(a, b):
    """How two byte strings differ, with the bytes of each at the first place that differs; None when the same"""
    if a == b:
        return None
    result = byte_places(a, b)
    at = result["first"] - result["first"] % 4
    result["a"] = window(a, at)
    result["b"] = window(b, at)
    result["at"] = at
    return result


def compare_entities(a, b):
    """How two entities blocks differ: the list of the entities, then each component, field and row"""
    first, second = read_entities(a), read_entities(b)
    result = {}
    if first["entities"] != second["entities"] or first["in_use"] != second["in_use"]:
        ids_a, ids_b = first["entities"], second["entities"]
        places = [i for i in range(min(len(ids_a), len(ids_b))) if ids_a[i] != ids_b[i]]
        result["entities"] = {
            "counts": [len(ids_a), len(ids_b)], "in_use": [first["in_use"], second["in_use"]],
            "places": [{"index": i, "a": entity_text(ids_a[i]), "b": entity_text(ids_b[i])} for i in places[:20]],
            "places_differ": len(places)}
    if first["order"] != second["order"]:
        result["component_order"] = {"only_a": sorted(set(first["order"]) - set(second["order"])),
                                     "only_b": sorted(set(second["order"]) - set(first["order"]))}
    components = {}
    for name in first["order"]:
        if name not in second["components"]:
            continue
        comp_a, comp_b = first["components"][name], second["components"][name]
        if comp_a == comp_b:
            continue
        entry = {}
        if comp_a["fields"] != comp_b["fields"]:
            entry["fields"] = [comp_a["fields"], comp_b["fields"]]
        rows_a = [entity for entity, _ in comp_a["rows"]]
        rows_b = [entity for entity, _ in comp_b["rows"]]
        if rows_a != rows_b:
            set_a, set_b = set(rows_a), set(rows_b)
            entry["rows"] = {"counts": [len(rows_a), len(rows_b)],
                             "only_a": [entity_text(e) for e in rows_a if e not in set_b][:20],
                             "only_b": [entity_text(e) for e in rows_b if e not in set_a][:20],
                             "same_set_other_order": set_a == set_b}
        values_b = {entity: values for entity, values in comp_b["rows"]}
        fields = {}
        if comp_a["fields"] == comp_b["fields"]:
            for entity, values in comp_a["rows"]:
                other = values_b.get(entity)
                if other is None:
                    continue
                for field, value_a, value_b in zip(comp_a["fields"], values, other):
                    difference = compare_bytes(value_a, value_b)
                    if difference is not None:
                        fields.setdefault(field, []).append({"entity": entity_text(entity), **difference})
        if fields:
            entry["values"] = {field: {"entities": len(rows), "examples": rows[:3]} for field, rows in fields.items()}
        components[name] = entry
    if components:
        result["components"] = components
    return result


def compare_sections(a, b):
    """How two blocks of named sections differ, section by section"""
    first, second = dict(read_sections(a)), dict(read_sections(b))
    names_a = [name for name, _ in read_sections(a)]
    names_b = [name for name, _ in read_sections(b)]
    result = {}
    if names_a != names_b:
        result["names"] = {"only_a": [n for n in names_a if n not in second],
                           "only_b": [n for n in names_b if n not in first]}
    for name in names_a:
        if name in second:
            difference = compare_bytes(first[name], second[name])
            if difference is not None:
                result[name] = difference
    return result


# The blocks that this tool reads further than their bytes
BLOCK_READERS = {"ECSW": compare_entities, "SYST": compare_sections}


def compare_files(data_a, data_b):
    """How two saved worlds differ, block by block; an empty result when they are the same"""
    head_a, blocks_a = read_world(data_a)
    head_b, blocks_b = read_world(data_b)
    result = {}
    if head_a != head_b:
        result["head"] = [head_a, head_b]
    tags_a = [tag for tag, _ in blocks_a]
    tags_b = [tag for tag, _ in blocks_b]
    if tags_a != tags_b:
        result["tags"] = [tags_a, tags_b]
    second = {}
    for tag, data in blocks_b:
        second.setdefault(tag, []).append(data)
    seen = {}
    for tag, data in blocks_a:
        index = seen.get(tag, 0)
        seen[tag] = index + 1
        others = second.get(tag, [])
        if index >= len(others) or data == others[index]:
            continue
        key = tag if index == 0 else f"{tag}#{index}"
        reader = BLOCK_READERS.get(tag)
        try:
            detail = reader(data, others[index]) if reader else None
        except (ValueError, struct.error) as error:
            detail = {"unread": str(error)}
        result[key] = {"bytes": byte_places(data, others[index]), **({"detail": detail} if detail else {})}
        if reader is None:
            result[key]["bytes"] = compare_bytes(data, others[index])
    return result


def text_report(result):
    """The result as lines of text"""
    if not result:
        return "The two saves have the same bytes.\n"
    lines = []
    for key, value in result.items():
        if key in ("head", "tags"):
            lines.append(f"{key}: {value[0]} / {value[1]}")
            continue
        size = value["bytes"]
        lines.append(f"block {key}: sizes {size['sizes'][0]} / {size['sizes'][1]}, {size['count']} bytes differ, "
                     f"first at {size['first']}")
        if "a" in size:
            lines.append(f"    a: {size['a']}")
            lines.append(f"    b: {size['b']}")
        detail = value.get("detail") or {}
        for part, info in detail.items():
            if part == "components":
                for component, entry in info.items():
                    for what in ("fields", "rows"):
                        if what in entry:
                            lines.append(f"    {component} {what}: {json.dumps(entry[what])}")
                    for field, rows in entry.get("values", {}).items():
                        example = rows["examples"][0]
                        lines.append(f"    {component}.{field}: {rows['entities']} entities, e.g. {example['entity']} "
                                     f"at byte {example['at']} sizes {example['sizes']}")
                        lines.append(f"        a: {example['a']}")
                        lines.append(f"        b: {example['b']}")
            elif isinstance(info, dict) and "first" in info and "a" in info:
                lines.append(f"    section {part}: sizes {info['sizes'][0]} / {info['sizes'][1]}, {info['count']} bytes "
                             f"differ, first at {info['first']}")
                lines.append(f"        a: {info['a']}")
                lines.append(f"        b: {info['b']}")
            else:
                lines.append(f"    {part}: {json.dumps(info)}")
    return "\n".join(lines) + "\n"


# The synced part of two world states of a network game (--synced). The game server and each client write their world
# state at the end of the same turn, with a map beside it (the game's --sim-world-states, the server's --world-states).
# The map gives the parts that only show the world on that computer. This compare leaves these parts out:
# - the head: the count of the loads that did not end is of the file on this computer;
# - the camera block (CAMR): each computer has its own camera;
# - in the clock block (TIME), all values but the synced seed and the count of its draws: the clock of the frames, the
#   local seed and the streams of this computer (see LOCAL_CLOCK_VALUES);
# - in the entities block, the presentation parts of the map (synced_state::PresentationParts);
# - in the systems block, the presentation bytes of each section of the map. A section that a computer does not write
#   has no synced bytes.
LOCAL_BLOCKS = {"CAMR"}
# The values of the clock block that each computer keeps for itself
LOCAL_CLOCK_VALUES = {"speed", "paused", "remainderMs", "localSeed", "particleStream", "crtSeed", "runSeed", "streams",
                      "general"}


def read_clock(data):
    """The values of the clock block (TIME), by name"""
    reader = Reader(data)
    clock = {"speed": struct.unpack("<f", reader.take(4))[0], "paused": reader.take(1)[0] != 0,
             "remainderMs": struct.unpack("<i", reader.take(4))[0], "syncedSeed": reader.u32(),
             "localSeed": reader.u32(), "particleStream": reader.take(1)[0]}
    clock["crtSeed"] = reader.u32() if reader.take(1)[0] != 0 else None
    clock["runSeed"] = reader.u32()
    clock["streams"] = [reader.name() for _ in range(reader.u32())]
    clock["general"] = reader.name() if reader.left() > 0 else ""
    clock["syncedDraws"] = struct.unpack("<Q", reader.take(8))[0] if reader.left() > 0 else None
    return clock


def presentation_test(parts, entities):
    """A test of whether a value of a component of an entity only shows the world on one computer. A part with a
    marker covers only the entities that have the marker component (entities: {component: set of entities})."""
    def is_presentation(component, field, entity):
        for part in parts:
            if part["component"] not in ("*", component) or part["field"] not in ("", field):
                continue
            if not part["marker"] or entity in entities.get(part["marker"], set()):
                return True
        return False
    return is_presentation


def whole_presentation(parts, component):
    """True for a component of which each value of each entity only shows the world"""
    return any(part["component"] == component and not part["field"] and not part["marker"] for part in parts)


def synced_entities(a, b, parts):
    """How the synced parts of two entities blocks differ"""
    first, second = read_entities(a), read_entities(b)
    result = {}
    if first["entities"] != second["entities"] or first["in_use"] != second["in_use"]:
        result["entities"] = {"counts": [len(first["entities"]), len(second["entities"])],
                              "in_use": [first["in_use"], second["in_use"]]}
    marks = []
    for block in (first, second):
        marks.append({name: {entity for entity, _ in comp["rows"]} for name, comp in block["components"].items()})
    tests = [presentation_test(parts, marks[0]), presentation_test(parts, marks[1])]
    components = {}
    for name in sorted(set(first["order"]) | set(second["order"])):
        if whole_presentation(parts, name):
            continue
        empty = {"fields": [], "rows": []}
        comp_a, comp_b = first["components"].get(name, empty), second["components"].get(name, empty)
        if comp_a == comp_b:
            continue
        entry = {}
        rows_a = [entity for entity, _ in comp_a["rows"]]
        rows_b = [entity for entity, _ in comp_b["rows"]]
        # The order of the rows of a mark with no values does not count, as for the checksum
        if not comp_a["fields"] and not comp_b["fields"]:
            rows_a, rows_b = sorted(rows_a), sorted(rows_b)
        if rows_a != rows_b:
            entry["rows"] = {"counts": [len(rows_a), len(rows_b)],
                             "only_a": [entity_text(e) for e in rows_a if e not in set(rows_b)][:20],
                             "only_b": [entity_text(e) for e in rows_b if e not in set(rows_a)][:20]}
        if comp_a["fields"] != comp_b["fields"] and comp_a["rows"] and comp_b["rows"]:
            entry["fields"] = [comp_a["fields"], comp_b["fields"]]
        values_b = dict(comp_b["rows"])
        fields = {}
        for entity, values in comp_a["rows"]:
            other = values_b.get(entity)
            if other is None or comp_a["fields"] != comp_b["fields"]:
                continue
            for field, value_a, value_b in zip(comp_a["fields"], values, other):
                if tests[0](name, field, entity) or tests[1](name, field, entity):
                    continue
                difference = compare_bytes(value_a, value_b)
                if difference is not None:
                    fields.setdefault(field, []).append({"entity": entity_text(entity), **difference})
        if fields:
            entry["values"] = {field: {"entities": len(rows), "examples": rows[:3]} for field, rows in fields.items()}
        if entry:
            components[name] = entry
    if components:
        result["components"] = components
    return result


def synced_bytes(data, ranges):
    """The bytes of a section without its presentation ranges"""
    kept, at = bytearray(), 0
    for first, last in sorted(ranges):
        if first > at:
            kept += data[at:first]
        at = max(at, last)
    return bytes(kept + data[at:])


def synced_sections(a, b, map_a, map_b):
    """How the synced bytes of the sections of two systems blocks differ. A section that a computer did not write has
    no synced bytes."""
    result = {}
    sides = []
    for data, mapping in ((a, map_a), (b, map_b)):
        sections = dict(read_sections(data)) if data is not None else {}
        ranges = {section["name"]: section for section in mapping.get("sections", [])}
        for name, section in sections.items():
            if name not in ranges or ranges[name]["size"] != len(section):
                raise ValueError(f"the map does not give the section {name} of its world state")
        sides.append({name: synced_bytes(section, ranges[name]["presentation"]) for name, section in sections.items()})
    for name in sorted(set(sides[0]) | set(sides[1])):
        difference = compare_bytes(sides[0].get(name, b""), sides[1].get(name, b""))
        if difference is not None:
            result[name] = difference
    return result


def compare_synced(data_a, map_a, data_b, map_b):
    """How the synced parts of two world states differ; an empty result when they are the same"""
    _, blocks_a = read_world(data_a)
    _, blocks_b = read_world(data_b)
    result = {}
    if map_a.get("turn") != map_b.get("turn"):
        result["turn"] = [map_a.get("turn"), map_b.get("turn")]
    for value in ("syncedSeed", "syncedDraws"):
        if map_a.get(value) != map_b.get(value):
            result[value] = [map_a.get(value), map_b.get(value)]
    parts = map_a.get("presentationParts", []) + [p for p in map_b.get("presentationParts", [])
                                                  if p not in map_a.get("presentationParts", [])]
    # A tag can come more than one time: the blocks of a tag pair in their order
    def keyed(blocks):
        seen, out = {}, {}
        for tag, data in blocks:
            index = seen.get(tag, 0)
            seen[tag] = index + 1
            out[tag if index == 0 else f"{tag}#{index}"] = data
        return out
    first, second = keyed(blocks_a), keyed(blocks_b)
    for key in list(first) + [k for k in second if k not in first]:
        tag = key.split("#")[0]
        if tag in LOCAL_BLOCKS:
            continue
        one, other = first.get(key), second.get(key)
        if tag == "SYST":
            detail = synced_sections(one, other, map_a, map_b)
        elif one is None or other is None:
            detail = {"only_in": "a" if other is None else "b"}
        elif tag == "TIME":
            clock_a, clock_b = read_clock(one), read_clock(other)
            detail = {name: [clock_a[name], clock_b[name]] for name in clock_a
                      if name not in LOCAL_CLOCK_VALUES and clock_a[name] != clock_b[name]}
        elif tag == "ECSW":
            detail = synced_entities(one, other, parts)
        elif tag == "RCTX":
            # The context keeps some presentation values, as the sections of the systems do
            detail = compare_bytes(synced_bytes(one, map_a.get("contextPresentation", [])),
                                   synced_bytes(other, map_b.get("contextPresentation", []))) or {}
        else:
            detail = compare_bytes(one, other) or {}
        if detail:
            result[key] = detail
    return result


def read_state(path):
    """The bytes of a world state and its map, which is beside it with the extension .json"""
    with open(path, "rb") as file:
        data = file.read()
    with open(os.path.splitext(path)[0] + ".json", encoding="utf-8") as file:
        return data, json.load(file)


def compare_synced_files(path_a, path_b):
    """compare_synced of two world states on the disk"""
    data_a, map_a = read_state(path_a)
    data_b, map_b = read_state(path_b)
    return compare_synced(data_a, map_a, data_b, map_b)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("first")
    parser.add_argument("second")
    parser.add_argument("--json", action="store_true", help="write the result as JSON")
    parser.add_argument("--synced", action="store_true",
                        help="compare only the synced parts of two world states of a network game, with their maps")
    options = parser.parse_args(argv)
    if options.synced:
        result = compare_synced_files(options.first, options.second)
        sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json or result
                         else "The synced parts of the two world states have the same bytes.\n")
        return 0 if not result else 1
    with open(options.first, "rb") as file_a, open(options.second, "rb") as file_b:
        result = compare_files(file_a.read(), file_b.read())
    sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json else text_report(result))
    return 0 if not result else 1


if __name__ == "__main__":
    sys.exit(main())
