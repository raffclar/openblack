#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Reads the records of desyncs that the game server keeps, and tells how they differ, in words.

The game server keeps one folder for the first desync of each god (default: desyncs/ beside the server, for 14 days).
The folder has facts.json (the facts of both sides, the first fact that differs, the entities that differ), and the
world of the server (server.obsav) and of the player (player.obsav) at the turn of the capture.

    python tools/sim/desync_bundle.py RECORD              (the facts, then the server's world against the player's)
    python tools/sim/desync_bundle.py RECORD_A RECORD_B   (the facts of two records, then their worlds, side by side)
    python tools/sim/desync_bundle.py RECORD --json       (the result as JSON)

The exit code is 0 when nothing differs, 1 when something differs and 2 when a record does not read.
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import save_diff  # noqa: E402

FACTS_FILE = "facts.json"
SERVER_WORLD = "server.obsav"
PLAYER_WORLD = "player.obsav"


def read_record(folder):
    """The facts of a record, and the paths of its worlds (None for a world that the record does not have)"""
    with open(os.path.join(folder, FACTS_FILE), encoding="utf-8") as file:
        facts = json.load(file)
    worlds = {}
    for name in (SERVER_WORLD, PLAYER_WORLD):
        path = os.path.join(folder, name)
        worlds[name] = path if os.path.isfile(path) else None
    return facts, worlds


def fact_lines(facts):
    """The facts of one record in words"""
    lines = [f"Record {facts.get('record')}: game {facts.get('game')}, slot {facts.get('slot')}, parted at turn "
             f"{facts.get('part_turn')}, captured at turn {facts.get('capture_turn')}"]
    first = facts.get("first_fact")
    if first:
        lines.append(f"The first fact that differs is {first['fact']}: {first['words']} "
                     f"(server \"{first['server']}\", player \"{first['player']}\")")
    elif facts.get("player") is None:
        lines.append("The player sent no facts")
    else:
        lines.append("No fact differs: the cause is in the logic of the game")
    for part in facts.get("parts", []):
        if not part.get("compared"):
            lines.append(f"  {part['part']}: not compared ({part.get('server_entities')} entities on the server)")
            continue
        entities = part.get("entities", [])
        if not entities:
            lines.append(f"  {part['part']}: no entity differs")
        for entity in entities:
            lines.append(f"  {part['part']}: entity {entity['entity']}: server {side_words(entity.get('server'))}; "
                         f"player {side_words(entity.get('player'))}")
    if facts.get("missing"):
        lines.append("The capture did not get: " + ", ".join(facts["missing"]))
    return lines


def side_words(side):
    if side is None:
        return "no entity"
    place = side.get("position")
    where = f" at {place[0]:.3f} {place[1]:.3f} {place[2]:.3f}" if place else ""
    return f"{side.get('kind') or 'Entity'}{where} hash {side.get('hash')}"


def facts_difference(facts_a, facts_b):
    """The facts of two records that differ, as (name, a, b). The record's own name and time do not count."""
    differences = []
    for side in ("server", "player"):
        a = facts_a.get(side) or {}
        b = facts_b.get(side) or {}
        for name in sorted(set(a) | set(b)):
            if a.get(name) != b.get(name):
                differences.append((f"{side}.{name}", a.get(name), b.get(name)))
    for name in ("first_fact", "missing"):
        if facts_a.get(name) != facts_b.get(name):
            differences.append((name, facts_a.get(name), facts_b.get(name)))
    return differences


def compare_worlds(path_a, path_b):
    """save_diff of two worlds; None when one of them is not there"""
    if path_a is None or path_b is None:
        return None
    with open(path_a, "rb") as file_a, open(path_b, "rb") as file_b:
        return save_diff.compare_files(file_a.read(), file_b.read())


def one_record(folder):
    facts, worlds = read_record(folder)
    worlds_result = compare_worlds(worlds[SERVER_WORLD], worlds[PLAYER_WORLD])
    return {"facts": facts, "lines": fact_lines(facts), "worlds": worlds_result,
            "worlds_missing": [name for name, path in worlds.items() if path is None]}


def two_records(folder_a, folder_b):
    facts_a, worlds_a = read_record(folder_a)
    facts_b, worlds_b = read_record(folder_b)
    return {"facts": [{"name": name, "a": a, "b": b} for name, a, b in facts_difference(facts_a, facts_b)],
            "lines_a": fact_lines(facts_a), "lines_b": fact_lines(facts_b),
            "server_worlds": compare_worlds(worlds_a[SERVER_WORLD], worlds_b[SERVER_WORLD]),
            "player_worlds": compare_worlds(worlds_a[PLAYER_WORLD], worlds_b[PLAYER_WORLD])}


def world_words(title, result):
    if result is None:
        return f"{title}: one of the worlds is not in the record\n"
    if not result:
        return f"{title}: the same bytes\n"
    return f"{title}:\n" + save_diff.text_report(result)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("record")
    parser.add_argument("other", nargs="?")
    parser.add_argument("--json", action="store_true", help="write the result as JSON")
    options = parser.parse_args(argv)
    try:
        result = two_records(options.record, options.other) if options.other else one_record(options.record)
    except (OSError, ValueError, KeyError) as error:
        sys.stderr.write(f"The record does not read: {error}\n")
        return 2
    if options.json:
        sys.stdout.write(json.dumps(result, indent=1) + "\n")
    elif options.other:
        sys.stdout.write("\n".join(result["lines_a"]) + "\n\n" + "\n".join(result["lines_b"]) + "\n\n")
        if result["facts"]:
            for entry in result["facts"]:
                sys.stdout.write(f"The fact {entry['name']} differs: {entry['a']!r} against {entry['b']!r}\n")
        else:
            sys.stdout.write("The facts of the two records are the same\n")
        sys.stdout.write(world_words("The worlds of the servers", result["server_worlds"]))
        sys.stdout.write(world_words("The worlds of the players", result["player_worlds"]))
    else:
        sys.stdout.write("\n".join(result["lines"]) + "\n")
        sys.stdout.write(world_words("The server's world against the player's world", result["worlds"]))
    if options.other:
        differs = bool(result["facts"]) or bool(result["server_worlds"]) or bool(result["player_worlds"])
    else:
        differs = bool(result["facts"].get("first_fact")) or bool(result["worlds"]) or any(
            part.get("entities") for part in result["facts"].get("parts", []))
    return 1 if differs else 0


if __name__ == "__main__":
    sys.exit(main())
