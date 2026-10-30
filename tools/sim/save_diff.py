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
"""

import argparse
import json
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


def differing_parts(result):
    """The parts of a result in short: each block that differs as its tag, or each component and section in it as
    the tag, a colon and its name"""
    parts = []
    for key, value in result.items():
        detail = value.get("detail") if isinstance(value, dict) else None
        names = []
        if isinstance(detail, dict):
            names += [f"{key}:{name}" for name in detail.get("components", {})]
            names += [f"{key}:{name}" for name in detail if name != "components"]
        parts += names or [key]
    return parts


# Each load of a saved game starts a game, as the game does, so a save after the load can differ from the save that
# loaded in these values only:
# - the player takes the alignment that the profile keeps;
# - the statistics note the alignments and the towns at the start again (the towns are the last value of the RCTX block);
# - the creatures' minds forget what a saved game does not keep;
# - the sounds do not come back, and a creature's creed powers come back one by one with no glow placed yet;
# - the mind of each computer god starts again, with no player for the size of its creature and a free hand;
# - a search for a footpath starts again from its two ends.
GAME_START_VALUES = {
    "Alignment": {"value"},
    "PlayerStatistics": {"startAlignment", "startCreatureAlignment"},
    "CreatureMindState": {"leftAreaActions", "agendaRefused"},
    "CreatureHandGlows": {"placed", "motion"},
    "ComputerPlayer": {"mind", "creatureLike", "hand"},
    "FootpathFinder": {"plan"},
}
# The components whose rows a load of a saved game can add or remove
GAME_START_ROWS = {"PlayerStatistics": "only_b", "SoundTag": "only_a"}
# The bytes at the end of the RCTX block that the start of a game sets: the count of the towns at the start
GAME_START_CONTEXT_BYTES = 4


def changed_altitudes(data):
    """The count of the land's corners whose altitude the world changed, as a saved game or a world state keeps them
    (LALT block)"""
    _, blocks = read_world(data)
    for tag, block in blocks:
        if tag == "LALT" and len(block) >= 4:
            return struct.unpack_from("<I", block)[0]
    return 0


def not_game_start(result):
    """The parts of a comparison of a save with the save after its load that the start of a game does not explain. A
    saved game keeps the altitudes that the world changed (deviation 168), so the land does not explain a difference."""
    left = []
    for key, value in result.items():
        if key == "RCTX":
            size = value["bytes"]
            if size["sizes"][0] == size["sizes"][1] and size["first"] >= size["sizes"][0] - GAME_START_CONTEXT_BYTES:
                continue
            left.append(key)
            continue
        if key != "ECSW":
            left.append(key)
            continue
        detail = value.get("detail") or {}
        sounds_forgotten = "SoundTag" in detail.get("components", {}) or \
            "SoundTag" in detail.get("component_order", {}).get("only_a", [])
        for part, info in detail.items():
            if part == "entities":
                # The tags of the sounds that went leave their places in the list of the entities
                if not (sounds_forgotten and info["counts"][0] == info["counts"][1]):
                    left.append(f"{key}:{part}")
                continue
            if part == "component_order":
                if info["only_b"] or not set(info["only_a"]) <= {"SoundTag"}:
                    left.append(f"{key}:{part}")
                continue
            if part != "components":
                left.append(f"{key}:{part}")
                continue
            for component, entry in info.items():
                fields = set(entry.get("values", {}))
                rows = entry.get("rows")
                explained = "fields" not in entry and fields <= GAME_START_VALUES.get(component, set())
                if rows is not None:
                    kind = GAME_START_ROWS.get(component)
                    other = "only_a" if kind == "only_b" else "only_b"
                    explained = explained and kind is not None and not rows[other]
                if not explained:
                    left.append(f"{key}:{component}")
    return left


# A load of a world state ends the visit of this computer's player to the temple: the temple, its help text and its
# dialogue close. These sections differ only when the player was in the temple at the save.
TEMPLE_VISIT_SECTIONS = {"temple", "helpText", "dialogueControl"}


def not_temple_visit(result):
    """The parts of a world state round trip that the end of a visit to the temple does not explain"""
    return [part for part in differing_parts(result)
            if not (part.startswith("SYST:") and part[len("SYST:"):] in TEMPLE_VISIT_SECTIONS)]


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


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("first")
    parser.add_argument("second")
    parser.add_argument("--json", action="store_true", help="write the result as JSON")
    options = parser.parse_args(argv)
    with open(options.first, "rb") as file_a, open(options.second, "rb") as file_b:
        result = compare_files(file_a.read(), file_b.read())
    sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json else text_report(result))
    return 0 if not result else 1


if __name__ == "__main__":
    sys.exit(main())
