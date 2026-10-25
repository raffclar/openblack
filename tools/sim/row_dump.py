#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Compares two row dumps of the synced world (the game's --sim-row-dump, the server's --row-dump) at one turn.

    python tools/sim/row_dump.py SERVER_ROWS CLIENT_ROWS [--turn T] [--json]

Each dump has one line "turn T reason" for each turn that it wrote, then one line for each field of each row:
"component.field entity bytes". The entity of the synced random numbers (GameRandom.seed, GameRandom.draws) is
4294967295. With no --turn, the script takes the first turn of both dumps with a difference, else the first turn
that both wrote. It prints the
first field that differs (in the order of the first dump), each entity that has a different field with its
components, and the rows that only one dump has.
"""

import argparse
import json
import struct
import sys


def read_dump(path):
    """The turns of a dump: {turn: {"reason": text, "rows": {(field, entity): bytes in hexadecimal}, "order": [...]}}"""
    turns = {}
    current = None
    with open(path, encoding="utf-8") as file:
        for line in file:
            line = line.rstrip("\n")
            if line.startswith("turn "):
                words = line.split(" ", 2)
                turn = int(words[1])
                reason = words[2] if len(words) > 2 else ""
                if reason.endswith("(the rows of the turn are not kept)"):
                    turns.setdefault(turn, {"reason": reason, "rows": None, "order": []})
                    current = None
                    continue
                current = {"reason": reason, "rows": {}, "order": []}
                turns[turn] = current
                continue
            if current is None or not line:
                continue
            parts = line.split(" ")
            if len(parts) != 3:
                continue
            key = (parts[0], int(parts[1]))
            current["rows"][key] = parts[2]
            current["order"].append(key)
    return turns


def entity_text(entity):
    """The number of an entity as the game shows it: its index and its version"""
    return f"{entity & 0xfffff}v{entity >> 20}"


def value_text(hexbytes):
    """The bytes of a field, and as floats or whole numbers when the size fits"""
    if hexbytes is None:
        return "(no row)"
    raw = bytes.fromhex(hexbytes)
    text = hexbytes if len(raw) <= 48 else hexbytes[:96] + "..."
    if raw and len(raw) % 4 == 0 and len(raw) <= 64:
        floats = struct.unpack(f"<{len(raw) // 4}f", raw)
        text += " floats " + ",".join(f"{value:.9g}" for value in floats)
    elif len(raw) in (1, 2):
        text += f" = {int.from_bytes(raw, 'little')}"
    return text


def compare(first, second, turn=None):
    """The differences of two dumps at a turn, as a dictionary"""
    both = sorted(t for t in first if t in second and first[t]["rows"] is not None and second[t]["rows"] is not None)
    if turn is None:
        if not both:
            return {"error": "no turn has rows in both dumps", "first_turns": sorted(first),
                    "second_turns": sorted(second)}
        # The first turn that has a difference, as the turn of a part, else the first turn
        turn = next((t for t in both if first[t]["rows"] != second[t]["rows"]), both[0])
    if turn not in both:
        return {"error": f"turn {turn} has no rows in both dumps", "turns_in_both": both}
    a = first[turn]
    b = second[turn]
    different = [key for key in a["order"] if key in b["rows"] and a["rows"][key] != b["rows"][key]]
    only_first = [key for key in a["order"] if key not in b["rows"]]
    only_second = [key for key in b["order"] if key not in a["rows"]]
    entities = []
    seen = set()
    for field, entity in different + only_first + only_second:
        if entity in seen:
            continue
        seen.add(entity)
        components = sorted({key[0].split(".")[0] for key in a["order"] if key[1] == entity}
                            | {key[0].split(".")[0] for key in b["order"] if key[1] == entity})
        fields = [{"field": key[0], "first": value_text(a["rows"].get(key)), "second": value_text(b["rows"].get(key))}
                  for key in dict.fromkeys(different + only_first + only_second) if key[1] == entity]
        entities.append({"entity": entity, "entity_text": entity_text(entity), "components": components,
                         "fields": fields})
    result = {"turn": turn, "first_reason": a["reason"], "second_reason": b["reason"], "rows": len(a["order"]),
              "different": len(different), "only_first": len(only_first), "only_second": len(only_second),
              "entities": entities}
    if different or only_first or only_second:
        field, entity = (different + only_first + only_second)[0]
        result["first_difference"] = {"entity": entity, "entity_text": entity_text(entity),
                                      "component": field.split(".")[0], "field": field.split(".", 1)[1],
                                      "first": value_text(a["rows"].get((field, entity))),
                                      "second": value_text(b["rows"].get((field, entity)))}
    return result


def print_result(result, limit):
    if "error" in result:
        print(result["error"])
        return
    print(f"turn {result['turn']}: {result['rows']} rows, {result['different']} fields differ, "
          f"{result['only_first']} only in the first dump, {result['only_second']} only in the second")
    first = result.get("first_difference")
    if first is None:
        print("the rows are the same")
        return
    print(f"first difference: entity {first['entity_text']} ({first['entity']}) {first['component']}.{first['field']}")
    print(f"  first:  {first['first']}")
    print(f"  second: {first['second']}")
    for entity in result["entities"][:limit]:
        print(f"entity {entity['entity_text']} ({entity['entity']}): {' '.join(entity['components'])}")
        for field in entity["fields"]:
            print(f"  {field['field']}: {field['first']}  |  {field['second']}")
    if len(result["entities"]) > limit:
        print(f"... {len(result['entities']) - limit} more entities")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("first", help="the first row dump, as the server's")
    parser.add_argument("second", help="the second row dump, as a client's")
    parser.add_argument("--turn", type=int, help="the turn to compare (default: the first turn that both wrote)")
    parser.add_argument("--limit", type=int, default=20, help="the most entities to print")
    parser.add_argument("--json", action="store_true", help="print the result as JSON")
    options = parser.parse_args()
    result = compare(read_dump(options.first), read_dump(options.second), options.turn)
    if options.json:
        print(json.dumps(result, indent=1))
    else:
        print_result(result, options.limit)
    return 1 if "error" in result else 0


if __name__ == "__main__":
    sys.exit(main())
