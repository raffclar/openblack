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


# The kinds of the records in the records block (OBJS), as the game numbers them
CREATURE_TYPE = 0x69
LEASH_STATUS_TYPE = 0x102
# A creature whose position has this flag is in the physics, unless particles carry it
POSITION_IN_PHYSICS = 0x40
THING_CARRIED_BY_PARTICLES = 0x10
# The movement states that hug a circle round an object, and the state that steps through
FIRST_CIRCLE_STATE = 0x0C
LAST_CIRCLE_STATE = 0x12
STEP_THROUGH_STATE = 0x0B
# A record of this version or older has no word for the processing of the mind
LAST_VERSION_WITHOUT_PROCESSING = 25
CREATURE_SPELLS = 16
CREED_POWERS = 4
# The most steps of an agenda and the most items of a list, as the game reads them
MOST_STEPS = 32
MOST_LISTED = 1024


class RecordReader:
    """Reads the records block (OBJS) with the keys block (KEYS) beside it, as a list of named fields in the order of
    the bytes. A name is the number of the record in the stream, its kind and the path of the field, for example
    "2:creature.plan.agenda.state"."""

    def __init__(self, objects, keys):
        self.reader = Reader(objects)
        self.objects = 1
        key_reader = Reader(keys)
        count = key_reader.u32()
        key_reader.take(count * 12)
        self.foreign = {key_reader.u32() for _ in range(key_reader.u32())}
        self.fields = []
        self.prefix = ""

    def field(self, path, size):
        self.fields.append((self.prefix + path, bytes(self.reader.take(size))))
        return self.fields[-1][1]

    def u32(self, path):
        return struct.unpack("<I", self.field(path, 4))[0]

    def count(self, path, most):
        value = self.u32(path)
        if value > most:
            raise ValueError(f"the count {path} is too large")
        return value

    def ref(self, path):
        number = self.u32(path)
        if number == 0 or number <= self.objects:
            return
        if number != self.objects + 1:
            raise ValueError(f"the reference {path} has the number {number} out of order")
        self.objects += 1
        if number in self.foreign:
            return
        prefix = self.prefix
        self.record(number)
        self.prefix = prefix

    def record(self, number):
        self.prefix = f"{number}:"
        kind = self.u32("type")
        self.u32("extra")
        if kind == CREATURE_TYPE:
            self.prefix = f"{number}:creature."
            self.creature()
        elif kind == LEASH_STATUS_TYPE:
            self.prefix = f"{number}:leash."
            self.thing()
            self.leash()
        else:
            raise ValueError(f"the record {number} has the unknown kind {kind}")

    def thing(self):
        """The part of every record after its kind: returns the flags of the thing"""
        self.u32("thing.checksum")
        self.u32("thing.destroyed")
        return self.field("thing.flags", 1)[0]

    def creature(self):
        thing_flags = self.thing()
        self.field("position", 12)
        position_flags = struct.unpack("<H", self.field("position.flags", 2))[0]
        for name, size in (("info", 4), ("coords", 12), ("creationIndex", 4), ("life", 4), ("scale", 4),
                           ("heading", 4)):
            self.field(name, size)
        self.ref("fire")
        if position_flags & POSITION_IN_PHYSICS and not thing_flags & THING_CARRIED_BY_PARTICLES:
            self.field("physics.matrix", 48)
            self.field("physics.velocity", 12)
            self.field("physics.spin", 12)
        self.move()
        self.living()
        self.creature_record()

    def move(self):
        for name in ("mobileWord", "turnsToStateChange", "speed", "angle"):
            self.field(f"move.{name}", 2)
        state = self.field("move.moveState", 1)[0]
        if FIRST_CIRCLE_STATE <= state <= LAST_CIRCLE_STATE:
            self.field("move.circle.turnsToObject", 1)
            self.u32("move.circle.entryDistance")
            self.ref("move.circle.object")
            self.u32("move.circle.index")
            self.field("move.circle.coords", 12)
        elif state == STEP_THROUGH_STATE:
            self.field("move.turnsToStepRebuild", 1)
        self.ref("move.footpath")
        self.field("move.goal", 12)
        self.ref("move.target")
        self.field("move.step", 12)

    def living(self):
        self.ref("living.footpath")
        self.ref("living.footpathNode")
        self.field("living.action", 6)
        self.ref("living.reaction")
        self.u32("living.word9C")
        self.u32("living.birthTurn")
        self.field("living.byteA8", 1)
        self.ref("living.dataPath")
        self.ref("living.scriptReminder")
        self.field("living.status", 2)
        self.ref("living.flock")
        self.ref("living.objectBC")
        self.ref("living.footpathAgain")
        self.u32("living.wordC4")
        self.field("living.specular", 4)
        self.field("living.byteD4", 1)
        self.ref("living.danceGroup")
        self.field("living.byteDC", 1)
        for index in range(self.count("living.reactionsDone", MOST_LISTED)):
            self.u32(f"living.reactionsDone.{index}.reaction")
            self.u32(f"living.reactionsDone.{index}.turn")

    def creature_record(self):
        version = self.u32("version")
        self.u32("savedInTemple")
        self.field("index", 1)
        self.field("owner", 1)
        for slot in range(CREATURE_SPELLS):
            for name in ("spell", "phase", "turnsLeft", "found", "holdTurns"):
                self.u32(f"spells.{slot}.{name}")
            self.ref(f"spells.{slot}.miracle")
        for index in range(self.count("queued", MOST_LISTED)):
            self.u32(f"queued.{index}.spell")
            self.u32(f"queued.{index}.seconds")
            self.ref(f"queued.{index}.miracle")
        if version > LAST_VERSION_WITHOUT_PROCESSING:
            self.u32("mindProcessing")
        self.u32("largestSize")
        self.u32("smallestSize")
        self.field("mayGoThroughVortex", 1)
        self.field("fizzedForGood", 1)
        for name in ("fizz", "fizzTarget", "fizzPerSecond", "freeze", "word12C0"):
            self.u32(name)
        for index in range(3):
            self.ref(f"spellVisuals.{index}")
        for index in range(2):
            self.ref(f"handParticles.{index}")
        self.field("targetPoint", 12)
        self.ref("targetParticles")
        self.ref("targetObject")
        self.field("hasTarget", 1)
        self.ref("arena")
        self.field("home", 12)
        self.u32("fightHealth")
        self.u32("autoFight")
        for index in range(self.count("dancePartners", MOST_LISTED)):
            self.ref(f"dancePartners.{index}")
        self.ref("dance")
        for name in ("canDie", "strength", "size", "alignment", "animationPace", "autoScale", "autoScaleShare"):
            self.u32(name)
        if self.u32("plan") != 0:
            self.plan()
        for index in range(CREED_POWERS):
            self.u32(f"creedPowers.{index}.scale")
            self.u32(f"creedPowers.{index}.power")

    def plan(self):
        self.u32("plan.desire")
        self.u32("plan.action")
        for index in range(3):
            self.ref(f"plan.objects.{index}")
        agenda = {name: self.u32(f"plan.agenda.{name}")
                  for name in ("word08", "current", "state", "count", "word18", "word1C", "word20")}
        if agenda["count"] > MOST_STEPS:
            raise ValueError("the agenda has too many steps")
        # The game writes the steps from the current step to the last
        for index in range(max(agenda["count"] - agenda["current"], 0)):
            step = f"plan.agenda.steps.{index}."
            self.u32(step + "action")
            self.ref(step + "object")
            self.u32(step + "integer")
            self.field(step + "point", 12)
            for name in ("real", "spell", "word28"):
                self.u32(step + name)
            self.ref(step + "other")
            self.u32(step + "orderKind")
        self.ref("plan.agenda.creature")
        self.ref("plan.agenda.other")

    def leash(self):
        for name in ("on", "works", "type"):
            self.u32(name)
        self.field("player", 1)
        self.u32("word20")
        self.ref("tiedTo")
        self.u32("length")
        self.u32("fullLength")
        self.ref("holder")
        self.u32("word34")

    def read(self):
        """Every field of the block, in order"""
        for index in range(self.count("creatures", MOST_LISTED)):
            for part in ("", ".leash"):
                self.prefix = ""
                self.ref(f"creatures.{index}{part}")
            self.prefix = ""
            for name in ("agendaStep", "heading", "vanillaNumber"):
                self.u32(f"creatures.{index}.{name}")
        if self.reader.left() != 0:
            raise ValueError("bytes after the last creature")
        return self.fields


def read_records(objects, keys):
    """The fields of the records block (OBJS), by name, in order: [(name, bytes)]"""
    return RecordReader(objects, keys).read()


def compare_records(objects_a, keys_a, objects_b, keys_b):
    """How two records blocks differ, field by field: the names of the fields that differ"""
    first, second = read_records(objects_a, keys_a), read_records(objects_b, keys_b)
    if [name for name, _ in first] != [name for name, _ in second]:
        return {"layout": "the two blocks have other fields"}
    return {"fields": [name for (name, a), (_, b) in zip(first, second) if a != b]}


def compare_clocks(a, b):
    """How two clock blocks (TIME) differ: the names of the values that differ"""
    first, second = read_clock(a), read_clock(b)
    return {"values": [name for name in first if first[name] != second.get(name)]}


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
    # The clock block and the records block name their values when the tool can read them
    first = dict(blocks_a)
    if "TIME" in result:
        try:
            result["TIME"]["detail"] = compare_clocks(first["TIME"], second["TIME"][0])
        except (ValueError, struct.error) as error:
            result["TIME"]["detail"] = {"unread": str(error)}
    if "OBJS" in result:
        try:
            result["OBJS"]["detail"] = compare_records(first["OBJS"], first.get("KEYS", b""), second["OBJS"][0],
                                                       second.get("KEYS", [b""])[0])
        except (ValueError, struct.error) as error:
            result["OBJS"]["detail"] = {"unread": str(error)}
    return result


def differing_parts(result):
    """The parts of a result in short: each block that differs as its tag, or each component and section in it as
    the tag, a colon and its name. The clock block and the records block give each value or field that differs, when
    the tool can read them."""
    parts = []
    for key, value in result.items():
        detail = value.get("detail") if isinstance(value, dict) else None
        names = []
        if key in ("TIME", "OBJS"):
            detail = detail or {}
            names += [f"{key}:{name}" for name in detail.get("values", detail.get("fields", []))]
        elif isinstance(detail, dict):
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
GAME_START = {
    "values": {
        "Alignment": {"value"},
        "PlayerStatistics": {"startAlignment", "startCreatureAlignment"},
        "CreatureMindState": {"leftAreaActions", "agendaRefused"},
        "CreatureHandGlows": {"placed", "motion"},
        "ComputerPlayer": {"mind", "creatureLike", "hand"},
        "FootpathFinder": {"plan"},
    },
    # The components whose rows a load can add or remove
    "rows": {"PlayerStatistics": "only_b", "SoundTag": "only_a"},
    # The components that a load can remove from the list of the components
    "order": {"SoundTag"},
    # The bytes at the end of the RCTX block that the start of a game sets: the count of the towns at the start
    "context_bytes": 4,
    # The tags of the sounds that went leave their places in the list of the entities
    "sounds": True,
    # The values of the clock block (TIME) and the fields of the records block (OBJS) that a load can change
    "clock": set(),
    "records": set(),
}
GAME_START_VALUES = GAME_START["values"]
GAME_START_ROWS = GAME_START["rows"]
GAME_START_CONTEXT_BYTES = GAME_START["context_bytes"]

# Each load of a saved game makes each creature again from its record and its mind file, as the game does (user
# question CAS2-1, branch creature-audit2-saves). Each load, also the second, then changes these values only:
# - the mind chooses again from idle with a new plan. Its actions get new random times since they were done. As in
#   the game, the new mind learns the starting lesson and then adds the examples of the file, so each load adds one
#   more copy of the lesson until the tree has 16 examples (vanilla-bugs 279);
# - the marks, the morph and the tattoos come back from the mind file, so their counts of changes go up;
# - the animation and the walk start again, and the object that the creature acted on goes;
# - the mind takes numbers from the synced stream as it starts, so the seed and the count of the draws change;
# - in the creature's record, the agenda of the kept plan has a new state and a new word before its current step.
# A world state keeps each creature exactly, so this allowance is for the saved-game round trips only.
CREATURE_RELOAD = {
    "values": {
        "CreatureMindState": {"idle", "ownPlanSinceStop", "learnt", "planSerial", "agendaSeen"},
        "CreatureMarks": {"marks", "revision"},
        "CreatureMorph": {"revision"},
        "CreatureTattoos": {"revision"},
        "CreatureAnimation": {"breathTarget"},
        "CreatureLocomotion": {"started"},
    },
    "rows": {"CreatureObjectAction": "only_a"},
    "order": {"CreatureObjectAction"},
    "context_bytes": 0,
    "sounds": False,
    "clock": {"syncedSeed", "syncedDraws"},
    "records": {"creature.plan.agenda.word08", "creature.plan.agenda.state"},
}


def joined(*allowances):
    """One allowance that explains each part that one of the allowances explains"""
    values = {}
    rows = {}
    for allowance in allowances:
        for component, fields in allowance["values"].items():
            values[component] = values.get(component, set()) | fields
        rows.update(allowance["rows"])
    return {"values": values, "rows": rows, "order": set().union(*(a["order"] for a in allowances)),
            "context_bytes": max(a["context_bytes"] for a in allowances),
            "sounds": any(a["sounds"] for a in allowances),
            "clock": set().union(*(a["clock"] for a in allowances)),
            "records": set().union(*(a["records"] for a in allowances))}


def changed_altitudes(data):
    """The count of the land's corners whose altitude the world changed, as a saved game or a world state keeps them
    (LALT block)"""
    _, blocks = read_world(data)
    for tag, block in blocks:
        if tag == "LALT" and len(block) >= 4:
            return struct.unpack_from("<I", block)[0]
    return 0


def record_field(name):
    """The name of a field of the records block with no number of its record"""
    return name.split(":", 1)[-1]


def not_explained(result, allowance):
    """The parts of a comparison of a save with the save after its load that the allowance does not explain. A saved
    game keeps the altitudes that the world changed (deviation 168), so the land does not explain a difference."""
    left = []
    for key, value in result.items():
        detail = value.get("detail") or {}
        if key == "RCTX":
            size = value["bytes"]
            if allowance["context_bytes"] and size["sizes"][0] == size["sizes"][1] and \
                    size["first"] >= size["sizes"][0] - allowance["context_bytes"]:
                continue
            left.append(key)
            continue
        if key == "TIME":
            if "values" not in detail or not set(detail["values"]) <= allowance["clock"]:
                left.append(key)
            continue
        if key == "OBJS":
            if "fields" not in detail or not {record_field(name) for name in detail["fields"]} <= allowance["records"]:
                left.append(key)
            continue
        if key != "ECSW":
            left.append(key)
            continue
        sounds_forgotten = allowance["sounds"] and (
            "SoundTag" in detail.get("components", {}) or
            "SoundTag" in detail.get("component_order", {}).get("only_a", []))
        for part, info in detail.items():
            if part == "entities":
                if not (sounds_forgotten and info["counts"][0] == info["counts"][1]):
                    left.append(f"{key}:{part}")
                continue
            if part == "component_order":
                if info["only_b"] or not set(info["only_a"]) <= allowance["order"]:
                    left.append(f"{key}:{part}")
                continue
            if part != "components":
                left.append(f"{key}:{part}")
                continue
            for component, entry in info.items():
                fields = set(entry.get("values", {}))
                rows = entry.get("rows")
                explained = "fields" not in entry and fields <= allowance["values"].get(component, set())
                if rows is not None:
                    kind = allowance["rows"].get(component)
                    other = "only_a" if kind == "only_b" else "only_b"
                    explained = explained and kind is not None and not rows[other]
                if not explained:
                    left.append(f"{key}:{component}")
    return left


def not_game_start(result):
    """The parts of a comparison of a save with the save after its load that the start of a game does not explain"""
    return not_explained(result, GAME_START)


def not_creature_reload(result):
    """The parts of a comparison of two saves, the second at once after a load of the first, that the creatures that
    the load makes again do not explain"""
    return not_explained(result, CREATURE_RELOAD)


def not_game_start_or_creature_reload(result):
    """The parts of a comparison of a save with the save after its load that neither the start of a game nor the
    creatures that the load makes again explain"""
    return not_explained(result, joined(GAME_START, CREATURE_RELOAD))


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


# The synced part of two world states of a network game (--synced). The game server and each client write their world
# state at the end of the same turn, with a map beside it (the game's --sim-world-states, the server's --world-states).
# The map gives the parts that only show the world on that computer. This compare leaves these parts out:
# - the head: the count of the loads that did not end is of the file on this computer;
# - the camera block (CAMR): each computer has its own camera;
# - in the clock block (TIME), all values but the synced seed and the count of its draws: the clock of the frames, the
#   local seed and the streams of this computer (see LOCAL_CLOCK_VALUES);
# - in the entities block, the presentation parts of the map (synced_state::PresentationParts);
# - in the systems block, the presentation bytes of each section of the map. A section that a computer does not write
#   has no synced bytes;
# - in the scripts block (LHVM), the bytes of the map's scriptPresentation: in a network game each computer runs only its
#   own help scripts, so the main stack, the tasks and the counts of the script machine are of that computer.
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
        elif tag == "LHVM":
            detail = compare_bytes(synced_bytes(one, map_a.get("scriptPresentation", [])),
                                   synced_bytes(other, map_b.get("scriptPresentation", []))) or {}
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
