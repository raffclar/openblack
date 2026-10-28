#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The summary of a simulation run, and the comparison of two runs, from the run records that the game writes.

A run record is a file of JSON lines that the game writes with --sim-record: a "start" line, a "record" line each
time some turns go by, and an "end" line. This module only reads records and logs. It does not start the game.

    python tools/sim/sim_report.py summary RUN.jsonl [--log RUN.log] [--json OUT.json] [--markdown OUT.md]
    python tools/sim/sim_report.py compare A.jsonl B.jsonl
"""

import argparse
import json
import re
import sys
from collections import Counter

# A turn is a tenth of a second of game time
TURNS_PER_SECOND = 10.0
# The share of the records at the start and at the end of a run that the times are compared over
TREND_SHARE = 0.1
LOG_LINE = re.compile(r"\[(error|critical|warning)\]", re.IGNORECASE)
ASSERT_WORD = re.compile(r"assert", re.IGNORECASE)
# What the game says when a component has entities but no registration: the sync checksum and the saves leave it out, so a
# change in it is never seen. A correct game has none, and every check tool fails when it finds one.
UNREGISTERED_WORDS = "unregistered component storage: "


def unregistered_in_text(lines):
    """The names of the unregistered components that lines of a log, a record or a checksum file tell of. A log tells
    them as the words and the names with commas between them; a record or a checksum line has an "unregistered" list."""
    names = set()
    for line in lines:
        if line.lstrip().startswith("{"):
            if '"unregistered"' not in line:
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                continue
            if not isinstance(value, dict):
                continue
            synced = value.get("synced")
            for holder in (value, synced if isinstance(synced, dict) else {}):
                listed = holder.get("unregistered")
                if isinstance(listed, list):
                    names.update(str(name) for name in listed)
            continue
        at = line.find(UNREGISTERED_WORDS)
        if at >= 0:
            tail = line[at + len(UNREGISTERED_WORDS):].strip()
            names.update(name.strip() for name in tail.split(",") if name.strip())
    return names


def unregistered_in_files(*paths):
    """The names of the unregistered components that these files tell of (a missing file tells of none)"""
    names = set()
    for path in paths:
        if not path:
            continue
        try:
            with open(path, encoding="utf-8", errors="replace") as file:
                names |= unregistered_in_text(file)
        except OSError:
            continue
    return names


def unregistered_line(names):
    """The line that a tool prints for a failure: the words and the sorted names"""
    return UNREGISTERED_WORDS + ", ".join(sorted(names))


# The log line of each thing that a turn does with the hand of a player, and the short name of each deed
HAND_DEED = re.compile(r"Hand deed: the hand of player (\d+) (.+?) thing (\d+)(?: at turn (\d+))?")
HAND_DEED_NAMES = {"takes": "takes", "scoops from": "scoops", "throws": "throws", "puts down": "puts_down",
                   "pours": "pours", "uses what it holds on": "uses"}


def hand_deeds_in_text(lines, until_turn=None):
    """The count of each deed of the hand of each player that the log lines tell of, as {player: {deed: count}}. With
    until_turn, a deed that a line tells to be after that turn is not counted: the server and a client can end on
    different turns, and a deed of the last turn of one is not a difference."""
    deeds = {}
    for line in lines:
        match = HAND_DEED.search(line)
        if match is None:
            continue
        if until_turn is not None and match.group(4) is not None and int(match.group(4)) > until_turn:
            continue
        name = HAND_DEED_NAMES.get(match.group(2), match.group(2))
        counts = deeds.setdefault(int(match.group(1)), {})
        counts[name] = counts.get(name, 0) + 1
    return deeds


def hand_deeds_in_file(path, until_turn=None):
    """As hand_deeds_in_text, for a log file; a missing file tells of none"""
    try:
        with open(path, encoding="utf-8", errors="replace") as file:
            return hand_deeds_in_text(file, until_turn)
    except OSError:
        return {}


def hand_deeds_of_both(first_path, second_path, last_turn):
    """The deeds of the hand in two logs, each counted up to last_turn, and whether they are the same. The game can play
    on a few turns after the last turn that it keeps a record of, so a deed after last_turn is not a difference."""
    first = hand_deeds_in_file(first_path, last_turn)
    second = hand_deeds_in_file(second_path, last_turn)
    return first, second, first == second


# The lines of the winning conditions of a network game: each god gets them, completes them, and one god wins
CONDITION_EVENT = re.compile(r"Turn (\d+): the god of player (\d+) (gets \d+ winning conditions|completed winning condition "
                             r"\d+ at -?\d+|won the game)")


def condition_events_in_file(path, until_turn=None):
    """The events of the winning conditions that a log tells of, as [turn, player, what], in their order. With until_turn,
    an event after that turn is left out: the server and a client can end on different turns."""
    events = []
    try:
        with open(path, encoding="utf-8", errors="replace") as file:
            for line in file:
                match = CONDITION_EVENT.search(line)
                if match is None or (until_turn is not None and int(match.group(1)) > until_turn):
                    continue
                events.append([int(match.group(1)), int(match.group(2)), match.group(3)])
    except OSError:
        pass
    return events


def condition_summary(events):
    """The count of the conditions completed by each player, and the turn and the player of the win"""
    completed = Counter(str(player) for _, player, what in events if what.startswith("completed"))
    won = next(([turn, player] for turn, player, what in events if what == "won the game"), None)
    return {"completed": dict(sorted(completed.items())), "won": won}


def expected_deeds(path):
    """The deeds that a recorded input expects of its hand, as {deed: least count}: the "expect" of an input file in the
    object form. A list of events expects none."""
    if not path:
        return {}
    with open(path, encoding="utf-8") as file:
        value = json.load(file)
    return dict(value.get("expect", {})) if isinstance(value, dict) else {}


def missing_deeds(deeds, expected):
    """The expected deeds that the hand did fewer times than expected, as {deed: [count, least count]}"""
    return {name: [deeds.get(name, 0), least] for name, least in expected.items() if deeds.get(name, 0) < least}


# What the log says of the computer gods (the AI soak). The server and the clients write these lines:
#   Made creature <id> of species <n> for the computer god <god> at <x>, <z>   (the start of a computer god)
#   The computer god <god> starts with the personality of AI seed <seed>
#   Computer god <god> chooses need <name>                                     (a need that the mind chose)
#   Computer god <god> casts <miracle>                                         (a miracle that the god cast)
# and the "Hand deed" lines of the hand of the god. A god number counts from 0, as the hand deeds do.
COMPUTER_GOD_START = re.compile(r"for the computer god (\d+)|The computer god (\d+) starts with the personality")
COMPUTER_GOD_NEED = re.compile(r"Computer god (\d+) chooses need (\S+)")
COMPUTER_GOD_CAST = re.compile(r"Computer god (\d+) casts (\S+)")


def ai_actions_in_text(lines):
    """What the computer gods did, as the log lines tell: {"gods": [god numbers], "per_god": {god: {"needs": {name:
    count}, "hand": {deed: count}, "casts": {miracle: count}}}, "acted": whether any god did anything}"""
    lines = list(lines)
    gods = set()
    per_god = {}

    def entry(god):
        return per_god.setdefault(god, {"needs": {}, "hand": {}, "casts": {}})

    for line in lines:
        match = COMPUTER_GOD_START.search(line)
        if match is not None:
            gods.add(int(match.group(1) or match.group(2)))
        match = COMPUTER_GOD_NEED.search(line)
        if match is not None:
            needs = entry(int(match.group(1)))["needs"]
            needs[match.group(2)] = needs.get(match.group(2), 0) + 1
        match = COMPUTER_GOD_CAST.search(line)
        if match is not None:
            casts = entry(int(match.group(1)))["casts"]
            casts[match.group(2)] = casts.get(match.group(2), 0) + 1
    # The hand deeds of the computer gods only: a human god's hand is not the mind's
    for god, deeds in hand_deeds_in_text(lines).items():
        if god in gods:
            entry(god)["hand"] = deeds
    for god in gods:
        entry(god)
    acted = any(item["needs"] or item["hand"] or item["casts"] for item in per_god.values())
    return {"gods": sorted(gods), "per_god": {str(god): per_god[god] for god in sorted(per_god)}, "acted": acted}


def ai_actions_in_file(path):
    """As ai_actions_in_text, for a log file; a missing file tells of none"""
    try:
        with open(path, encoding="utf-8", errors="replace") as file:
            return ai_actions_in_text(file)
    except OSError:
        return ai_actions_in_text([])


def read_records(lines):
    """The start line, the records and the end line of a run record, from its lines. A line that is not JSON (as the
    last line of a run that stopped as it wrote) is left out."""
    start, records, end = None, [], None
    for line in lines:
        line = line.strip()
        if not line:
            continue
        try:
            item = json.loads(line)
        except ValueError:
            continue
        kind = item.get("type")
        if kind == "start":
            start = item
        elif kind == "record":
            records.append(item)
        elif kind == "end":
            end = item
    return start, records, end


def load(path):
    with open(path, encoding="utf-8") as file:
        return read_records(file)


def scan_log(lines, most=10):
    """The errors, the critical messages and the asserts in a game's log"""
    counts = Counter()
    examples = []
    for line in lines:
        found = LOG_LINE.search(line)
        level = found.group(1).lower() if found else None
        if level == "warning" and not ASSERT_WORD.search(line):
            continue
        if level is None and not ASSERT_WORD.search(line):
            continue
        key = level if level is not None else "assert"
        if ASSERT_WORD.search(line):
            key = "assert"
        counts[key] += 1
        if len(examples) < most:
            examples.append(line.strip()[:300])
    return {"counts": dict(counts), "examples": examples}


def average(values):
    values = [value for value in values if value is not None]
    return sum(values) / len(values) if values else None


def stage_turns(records):
    """The first turn at which the creature is at each stage of growing up"""
    first = {}
    for record in records:
        creature = record.get("creature") or {}
        stage = creature.get("stage")
        if stage is not None and stage not in first:
            first[stage] = record["turn"]
    return {str(stage): turn for stage, turn in sorted(first.items())}


def creature_value(key):
    """A getter of a value of the creature in a record"""
    return lambda record: (record.get("creature") or {}).get(key)


def belief_count(record):
    return ((record.get("creature") or {}).get("beliefs") or {}).get("count")


def value_range(records, getter):
    """The first, last, lowest and highest of a value over the records, none when no record has it"""
    values = [getter(record) for record in records]
    values = [value for value in values if isinstance(value, (int, float))]
    if not values:
        return None
    return {"first": values[0], "last": values[-1], "lowest": min(values), "highest": max(values)}


def name_of(value):
    if isinstance(value, dict):
        return value.get("action")
    return value


def summary(start, records, end, log_lines=None, exit_code=None):
    """The summary of a run as a dict: its speed, the creature's life, the problems and the times"""
    timed = [record for record in records if record.get("turns", 0) > 0]
    first_turn = records[0]["turn"] if records else 0
    last_turn = end["turn"] if end else (records[-1]["turn"] if records else 0)
    wall = end.get("wall_s") if end else (records[-1].get("wall_s") if records else None)
    turns_per_second = (last_turn - first_turn) / wall if wall else None
    share = max(1, int(len(timed) * TREND_SHARE))
    flags = [flag for record in records for flag in record.get("flags", [])]
    flag_counts = Counter(flag["kind"] for flag in flags)
    plans = Counter(name_of((record.get("creature") or {}).get("plan")) or "none" for record in records)
    activities = Counter((record.get("creature") or {}).get("activity") or "none" for record in records)
    memory = [record.get("memory_mb") for record in records if record.get("memory_mb") is not None]
    log = scan_log(log_lines) if log_lines is not None else None
    result = {
        "seed": start.get("seed") if start else None,
        "fast": start.get("fast") if start else None,
        "frame_ms": start.get("frame_ms") if start else None,
        "every": start.get("every") if start else None,
        "records": len(records),
        "first_turn": first_turn,
        "last_turn": last_turn,
        "ended": end is not None,
        "exit_code": exit_code,
        "wall_s": wall,
        "turns_per_second": turns_per_second,
        "times_real_time": turns_per_second / TURNS_PER_SECOND if turns_per_second else None,
        "turn_ms_start": average(record["turn_ms"] for record in timed[:share]),
        "turn_ms_end": average(record["turn_ms"] for record in timed[-share:]),
        "turn_ms_longest": max((record.get("turn_ms_max", 0) for record in timed), default=None),
        "frame_ms_average": average(record.get("frame_ms") for record in timed),
        "frame_ms_longest": max((record.get("frame_ms_max", 0) for record in timed), default=None),
        "memory_mb_start": memory[0] if memory else None,
        "memory_mb_end": memory[-1] if memory else None,
        "memory_mb_highest": max(memory) if memory else None,
        "entities_start": records[0].get("entities") if records else None,
        "entities_end": records[-1].get("entities") if records else None,
        "stages": stage_turns(records),
        "age": value_range(records, creature_value("age")),
        "size": value_range(records, creature_value("size")),
        "hunger": value_range(records, creature_value("hunger")),
        "tiredness": value_range(records, creature_value("tiredness")),
        "health": value_range(records, creature_value("health")),
        "alignment": value_range(records, creature_value("alignment")),
        "beliefs": value_range(records, belief_count),
        "plans": dict(plans.most_common(15)),
        "activities": dict(activities.most_common(15)),
        "flag_counts": dict(flag_counts),
        "flags": flags[:50],
        "longest": (end or {}).get("longest", [])[:20],
        "log": log,
    }
    unregistered = unregistered_in_text(log_lines or ()) | unregistered_in_text(json.dumps(record) for record in records)
    result["unregistered"] = sorted(unregistered)
    problems = sum(flag_counts.values()) + (1 if unregistered else 0)
    if log is not None:
        problems += sum(log["counts"].get(level, 0) for level in ("error", "critical", "assert"))
    crashed = exit_code not in (None, 0)
    result["verdict"] = "stable" if problems == 0 and not crashed and result["ended"] else "problems"
    return result


# The fields of a record that a run of the same seed and inputs must give again. The others (times, memory) change
# from run to run.
SAME_FIELDS = ("hash", "draws", "entities", "reactions", "counts", "creature", "flags")
SEED_NAMES = ("synced", "local")


def differing_fields(record_a, record_b):
    """The names of the fields that two records of the same turn give differently, each seed by its name"""
    fields = [key for key in SAME_FIELDS if record_a.get(key) != record_b.get(key)]
    seeds_a, seeds_b = record_a.get("seeds") or [], record_b.get("seeds") or []
    for index, name in enumerate(SEED_NAMES):
        value_a = seeds_a[index] if index < len(seeds_a) else None
        value_b = seeds_b[index] if index < len(seeds_b) else None
        if value_a != value_b:
            fields.append(f"seeds.{name}")
    return fields


def differing_counts(record_a, record_b):
    """The component counts that two records give differently, as {name: [a, b]}"""
    counts_a, counts_b = record_a.get("counts") or {}, record_b.get("counts") or {}
    return {name: [counts_a.get(name), counts_b.get(name)] for name in sorted(set(counts_a) | set(counts_b))
            if counts_a.get(name) != counts_b.get(name)}


def compare(records_a, records_b):
    """Where two runs part: the first turn recorded in both whose hashes, seeds or counts of draws differ, and what
    differs there. Records from a game that does not write the seeds or the draws are compared by their hashes alone."""
    by_turn_b = {record["turn"]: record for record in records_b}
    compared = 0
    for record in records_a:
        other = by_turn_b.get(record["turn"])
        if other is None:
            continue
        compared += 1
        state_a = (record.get("creature") or {}).get("state_hash")
        state_b = (other.get("creature") or {}).get("state_hash")
        draws_same = record.get("seeds") == other.get("seeds") and record.get("draws") == other.get("draws")
        if record.get("hash") != other.get("hash") or state_a != state_b or not draws_same:
            parts_a, parts_b = record.get("hash_parts", {}), other.get("hash_parts", {})
            parts = sorted(key for key in set(parts_a) | set(parts_b) if parts_a.get(key) != parts_b.get(key))
            return {
                "same": False,
                "compared": compared,
                "turn": record["turn"],
                "frame": [record.get("frame"), other.get("frame")],
                "hash": [record.get("hash"), other.get("hash")],
                "creature_same": state_a == state_b,
                "draws_same": draws_same,
                "draws": [record.get("draws"), other.get("draws")],
                "seeds": [record.get("seeds"), other.get("seeds")],
                "differs": differing_fields(record, other),
                "counts": differing_counts(record, other),
                "parts": parts,
            }
    return {"same": compared > 0, "compared": compared, "turn": None}


def compare_synced(records_a, records_b):
    """Where two runs part in their synced worlds: the first turn recorded in both whose synced hash, synced seed or
    count of synced draws differ. Only the synced part of each record counts: two runs at other frame rates or with
    other draws of the frames give other local draws and other drawn places, but the same synced world."""
    by_turn_b = {record["turn"]: record for record in records_b if record.get("synced")}
    compared = 0
    for record in records_a:
        synced_a = record.get("synced")
        synced_b = (by_turn_b.get(record["turn"]) or {}).get("synced")
        if not synced_a or not synced_b:
            continue
        compared += 1
        if (synced_a.get("hash"), synced_a.get("seed"), synced_a.get("draws")) !=                 (synced_b.get("hash"), synced_b.get("seed"), synced_b.get("draws")):
            parts_a, parts_b = synced_a.get("parts") or {}, synced_b.get("parts") or {}
            parts = sorted(key for key in set(parts_a) | set(parts_b) if parts_a.get(key) != parts_b.get(key))
            return {
                "same": False,
                "compared": compared,
                "turn": record["turn"],
                "frame": [record.get("frame"), by_turn_b[record["turn"]].get("frame")],
                "hash": [synced_a.get("hash"), synced_b.get("hash")],
                "seeds": [synced_a.get("seed"), synced_b.get("seed")],
                "draws": [synced_a.get("draws"), synced_b.get("draws")],
                "parts": parts,
            }
    return {"same": compared > 0, "compared": compared, "turn": None}


def synced_part_firsts(records_a, records_b, limit=40):
    """For each synced part that differs between two runs, the first turn recorded in both at which it differs: the
    parts with the earliest turns first, at most limit of them. Empty without the hashes of the parts."""
    by_turn_b = {record["turn"]: record for record in records_b if record.get("synced")}
    firsts = {}
    for record in records_a:
        parts_a = (record.get("synced") or {}).get("parts")
        parts_b = ((by_turn_b.get(record["turn"]) or {}).get("synced") or {}).get("parts")
        if not parts_a or not parts_b:
            continue
        for key in set(parts_a) | set(parts_b):
            if key not in firsts and parts_a.get(key) != parts_b.get(key):
                firsts[key] = record["turn"]
    return dict(sorted(firsts.items(), key=lambda item: (item[1], item[0]))[:limit])


# The blocks of a saved world, each after its four letter tag, in the order the game writes them
SAVE_TAGS = (b"ECSW", b"RCTX", b"TIME", b"CAMR", b"LHVM", b"SOBJ", b"SYST")


def save_blocks(data):
    """Where each block of a saved world starts: [(start, tag)], by the first place of each tag in the file"""
    places = []
    for tag in SAVE_TAGS:
        start = data.find(tag)
        if start >= 0:
            places.append((start, tag.decode()))
    return sorted(places)


def compare_saves(data_a, data_b):
    """How two saved worlds differ: the count of bytes that differ in each block of the first, and whether the sizes
    differ. Two runs of the same seed and inputs that write the same world give an empty result."""
    result = {}
    if len(data_a) != len(data_b):
        result["size"] = [len(data_a), len(data_b)]
    blocks = save_blocks(data_a)
    for index in range(min(len(data_a), len(data_b))):
        if data_a[index] != data_b[index]:
            name = "head"
            for start, tag in blocks:
                if start <= index:
                    name = tag
            result[name] = result.get(name, 0) + 1
    return result


def markdown(result, title="Simulation run"):
    """The summary as a short Markdown report"""

    def number(value, digits=2):
        return "n/a" if value is None else (f"{value:.{digits}f}" if isinstance(value, float) else str(value))

    lines = [f"# {title}", "", f"Verdict: **{result['verdict']}**", ""]
    lines += [
        f"- Turns {result['first_turn']} to {result['last_turn']} in {number(result['wall_s'], 1)} s: "
        f"{number(result['turns_per_second'], 0)} turns a second, {number(result['times_real_time'], 1)} times real time",
        f"- Seed {result['seed']}, fast {result['fast']}, frame {result['frame_ms']} ms, a record every "
        f"{result['every']} turns, {result['records']} records, ended {result['ended']}, exit {result['exit_code']}",
        f"- Turn time {number(result['turn_ms_start'], 3)} ms at the start, {number(result['turn_ms_end'], 3)} ms at "
        f"the end, longest {number(result['turn_ms_longest'], 2)} ms; frame average "
        f"{number(result['frame_ms_average'], 3)} ms, longest {number(result['frame_ms_longest'], 2)} ms",
        f"- Memory {number(result['memory_mb_start'], 0)} MB to {number(result['memory_mb_end'], 0)} MB (highest "
        f"{number(result['memory_mb_highest'], 0)}); entities {result['entities_start']} to {result['entities_end']}",
        "",
        "## Creature",
        "",
        f"- First turn at each stage: {result['stages']}",
    ]
    for key in ("age", "size", "hunger", "tiredness", "health", "alignment", "beliefs"):
        value = result.get(key)
        if value:
            lines.append(f"- {key}: first {number(value['first'], 3)}, last {number(value['last'], 3)}, lowest "
                         f"{number(value['lowest'], 3)}, highest {number(value['highest'], 3)}")
    lines.append(f"- Plans (records): {result['plans']}")
    lines.append(f"- Activities (records): {result['activities']}")
    lines += ["", "## Problems", "", f"- Flags: {result['flag_counts'] or 'none'}"]
    if result.get("unregistered"):
        lines.append(f"- {unregistered_line(result['unregistered'])}")
    for flag in result["flags"][:15]:
        lines.append(f"  - turn {flag['turn']}: {flag['kind']} {flag['what']}: {flag['detail']}")
    if result["log"] is not None:
        lines.append(f"- Log: {result['log']['counts'] or 'no errors, critical messages or asserts'}")
        for example in result["log"]["examples"][:5]:
            lines.append(f"  - `{example}`")
    lines += ["", "## Longest time the same (turns; the limit when known)", ""]
    for run in result["longest"][:12]:
        lines.append(f"- {run['key']}: {run['turns']} (limit {run['limit']})")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    summary_parser = commands.add_parser("summary", help="the summary of one run")
    summary_parser.add_argument("record")
    summary_parser.add_argument("--log")
    summary_parser.add_argument("--json")
    summary_parser.add_argument("--markdown")
    compare_parser = commands.add_parser("compare", help="where two runs part")
    compare_parser.add_argument("first")
    compare_parser.add_argument("second")
    arguments = parser.parse_args(argv)
    if arguments.command == "summary":
        start, records, end = load(arguments.record)
        log_lines = None
        if arguments.log:
            with open(arguments.log, encoding="utf-8", errors="replace") as file:
                log_lines = file.readlines()
        result = summary(start, records, end, log_lines)
        if arguments.json:
            with open(arguments.json, "w", encoding="utf-8") as file:
                json.dump(result, file, indent=1)
        text = markdown(result)
        if arguments.markdown:
            with open(arguments.markdown, "w", encoding="utf-8") as file:
                file.write(text)
        sys.stdout.write(text)
        return 0 if result["verdict"] == "stable" else 1
    _, first, _ = load(arguments.first)
    _, second, _ = load(arguments.second)
    result = compare(first, second)
    print(json.dumps(result))
    return 0 if result["same"] else 1


if __name__ == "__main__":
    sys.exit(main())
