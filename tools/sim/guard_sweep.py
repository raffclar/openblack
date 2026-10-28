#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The guard sweep: the frame write guard over the testbed scenarios and the network lands, one game at a time.

The synced world must change only in the game turns. The game's frame write guard (--sim-frame-guard) writes a log line
the first time that a frame step changes a synced part. This tool runs the guard over each scenario and land, and tells
each change as one line: scenario, step, component, kind.

    python tools/sim/guard_sweep.py run --out OUT [--bin FOLDER] [--scenarios all|a,b|@file] [--lands all|land1,...]
                                        [--turns 80] [--land-turns 1000] [--fields] [--no-systems] [--draw-every 0] [--timeout 300]
    python tools/sim/guard_sweep.py summary OUT [--systems] [--json] [--fail]
    python tools/sim/guard_sweep.py --self-test

The kinds: rows (a frame step made or removed an entity), order (it added, removed or moved a row of a component, which
the value hash does not see), value (it changed the values of a component), field (with --fields: the field that
changed), draws (it drew a synced random number), system (it changed the saved state of a system; most of these hold
presentation state, so the summary shows them only with --systems).

A run can stop and go on again: a scenario with a finished record is not run again. Each game stops by itself at its
last turn or frame; a game that goes past the time limit is stopped by its own process id.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_paths  # noqa: E402

GUARD_LINE = re.compile(r"Frame write guard: the frame step (\S+) changed the synced part (.+?) at turn (\d+)"
                        r"(?: \(entity (\d+)\))?(?: \(values ([\d,]+)\))?\s*$")

# The network lands of the sweep: the arguments that choose each land. "land1" plays the first land as a game alone;
# the others play as a network game with computer gods only, as the AI soak does.
LANDS = {
    "land1": ["-s", "Land1.txt", "--skip-opening", "cow"],
    "land1_network": ["-s", "Land1.txt", "--skip-opening", "cow", "--sim-network-game", "--sim-gods-need-no-temple"],
    "twogods": ["-s", "Playgrounds/TwoGods.txt", "--skip-opening", "cow", "--sim-network-game",
                "--sim-all-computer-gods", "--sim-ai-seed", "1"],
    "threegods": ["-s", "Playgrounds/ThreeGods.txt", "--skip-opening", "cow", "--sim-network-game",
                  "--sim-all-computer-gods", "--sim-ai-seed", "1"],
    "ring32": ["--gods-ring-map", "--sim-network-game", "--sim-all-computer-gods", "--sim-ai-seed", "1"],
}
LAND_PREFIX = "land:"


def kind_of(part):
    """The kind and the component of a part that the guard names"""
    if part == "entities":
        return "rows", "-"
    if part == "synced random draws":
        return "draws", "-"
    if part.startswith("order of "):
        return "order", part[len("order of "):]
    if part.startswith("system "):
        return "system", part[len("system "):]
    if "." in part:
        return "field", part
    return "value", part


def parse_log(lines):
    """The guard's changes in the lines of a game log, in the order of the log"""
    changes = []
    for line in lines:
        match = GUARD_LINE.search(line)
        if not match:
            continue
        kind, component = kind_of(match.group(2))
        changes.append({"step": match.group(1), "part": match.group(2), "kind": kind, "component": component,
                        "turn": int(match.group(3)), "entity": int(match.group(4)) if match.group(4) else None,
                        "values": match.group(5)})
    return changes


def scenario_ids(game, argument):
    """The scenarios of the sweep: all of the game's scenarios, a list with commas, or the lines of a file (@file)"""
    if not argument:
        return []
    if argument == "all":
        output = subprocess.run([game, "--list-scenarios"], capture_output=True, text=True, check=True,
                                stdin=subprocess.DEVNULL).stdout
        return [line.strip() for line in output.splitlines() if line.strip()]
    if argument.startswith("@"):
        with open(argument[1:], encoding="utf-8") as file:
            return [line.strip() for line in file if line.strip() and not line.startswith("#")]
    return [item for item in argument.split(",") if item]


def land_names(argument):
    """The lands of the sweep: all of them or a list with commas"""
    if not argument:
        return []
    names = list(LANDS) if argument == "all" else [item for item in argument.split(",") if item]
    unknown = [name for name in names if name not in LANDS]
    if unknown:
        raise SystemExit(f"unknown lands {unknown}; the lands are {list(LANDS)}")
    return names


def game_command(game, game_path, item, log, options):
    """The command line of one game of the sweep"""
    turns = options.land_turns if item.startswith(LAND_PREFIX) else options.turns
    frames = turns * max(1, 100 // max(1, options.frame_ms)) * 4 + 400
    command = [game, "-g", game_path, "--skip-logos", "--seed", "1", "--background", "-l", log, "-n", str(frames),
               "--sim-fast", "--sim-frame-ms", str(options.frame_ms), "--sim-draw-every", str(options.draw_every),
               "--sim-until-turn", str(turns),
               "--sim-frame-guard-fields" if options.fields else "--sim-frame-guard"]
    if options.systems:
        command.append("--sim-frame-guard-systems")
    if item.startswith(LAND_PREFIX):
        command += LANDS[item[len(LAND_PREFIX):]]
    else:
        command += ["--scenario", item, "--scenario-hide-window"]
        if options.network_game:
            command.append("--sim-network-game")
    return command


def folder_name(item):
    return item.replace(":", "_").replace("/", "_")


def run(options):
    game = sim_paths.program(options.bin or sim_paths.default_bin(options.config), "openblack")
    if not os.path.exists(game):
        print(f"no game at {game}")
        return 1
    items = [LAND_PREFIX + name for name in land_names(options.lands)] + scenario_ids(game, options.scenarios)
    if options.limit:
        items = items[: options.limit]
    os.makedirs(options.out, exist_ok=True)
    with open(os.path.join(options.out, "sweep.json"), "w", encoding="utf-8") as file:
        json.dump({"game": game, "turns": options.turns, "land_turns": options.land_turns, "frame_ms": options.frame_ms, "fields": options.fields,
                   "systems": options.systems, "draw_every": options.draw_every, "items": len(items)}, file)
    environment = dict(os.environ)
    if "ALSOFT_CONF" not in environment:
        # No sound on the speakers: the null driver of OpenAL Soft
        silent = os.path.join(os.path.abspath(options.out), "alsoft-silent.ini")
        with open(silent, "w", encoding="utf-8") as file:
            file.write("[general]\ndrivers=null\n")
        environment["ALSOFT_CONF"] = silent
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    records = os.path.join(options.out, "records.jsonl")
    for index, item in enumerate(items):
        folder = os.path.join(options.out, folder_name(item))
        done = os.path.join(folder, "done.json")
        if os.path.exists(done):
            continue
        os.makedirs(folder, exist_ok=True)
        log = os.path.join(folder, "game.log")
        if os.path.exists(log):
            os.remove(log)
        started = time.monotonic()
        process = subprocess.Popen(game_command(game, options.game_path, item, log, options), cwd=folder,
                                   env=environment, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, creationflags=flags)
        with open(os.path.join(options.out, "pid.txt"), "w", encoding="utf-8") as file:
            file.write(str(process.pid))
        timed_out = False
        try:
            code = process.wait(timeout=options.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            # Stop only this game, by its own process
            process.kill()
            code = process.wait()
        changes = []
        if os.path.exists(log):
            with open(log, encoding="utf-8", errors="replace") as file:
                changes = parse_log(file)
        record = {"item": item, "code": code, "timeout": timed_out, "seconds": round(time.monotonic() - started, 1),
                  "changes": changes}
        with open(done, "w", encoding="utf-8") as file:
            json.dump(record, file)
        with open(records, "a", encoding="utf-8") as file:
            file.write(json.dumps(record) + "\n")
        found = sum(1 for change in changes if change["kind"] != "system")
        print(f"[{index + 1}/{len(items)}] {item} exit {code}{' timeout' if timed_out else ''} {record['seconds']} s,"
              f" {found} changes", flush=True)
        # The profile folder of each game is not needed
        shutil.rmtree(os.path.join(folder, "Profiles"), ignore_errors=True)
    return 0


def read_records(out):
    """The last record of each item of a sweep folder"""
    records = {}
    path = os.path.join(out, "records.jsonl")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as file:
            for line in file:
                if line.strip():
                    record = json.loads(line)
                    records[record["item"]] = record
    return list(records.values())


def summarise(records, systems=False):
    """The lines of the summary: one for each scenario, step, component and kind, the first turn and the fields; the
    bad exits; and the count of the scenarios of each step, component and kind"""
    lines = []
    totals = defaultdict(list)
    bad = []
    for record in sorted(records, key=lambda r: r["item"]):
        if record["timeout"] or record["code"] != 0:
            bad.append(f"{record['item']} exit {record['code']}{' timeout' if record['timeout'] else ''}")
        grouped = {}
        for change in record["changes"]:
            if change["kind"] == "system" and not systems:
                continue
            component = change["component"].split(".")[0] if change["kind"] == "field" else change["component"]
            key = (change["step"], component, change["kind"])
            entry = grouped.setdefault(key, {"turn": change["turn"], "entity": change["entity"], "fields": []})
            if change["kind"] == "field":
                entry["fields"].append(change["part"].split(".", 1)[1])
            elif change["values"]:
                entry["fields"].append("values " + change["values"])
        for (step, component, kind), entry in sorted(grouped.items()):
            detail = f" ({', '.join(entry['fields'])})" if entry["fields"] else ""
            where = f" entity {entry['entity']}" if entry["entity"] is not None else ""
            lines.append(f"{record['item']}  {step}  {component}  {kind}  turn {entry['turn']}{where}{detail}")
            totals[(step, component, kind)].append(record["item"])
    count = [f"{len(items):4}  {step}  {component}  {kind}  e.g. {', '.join(items[:3])}"
             for (step, component, kind), items in sorted(totals.items())]
    return lines, bad, count


def summary(options):
    records = read_records(options.out)
    lines, bad, count = summarise(records, options.systems)
    if options.json:
        print(json.dumps({"items": len(records), "changes": lines, "bad": bad}))
    else:
        print(f"{len(records)} scenarios and lands; {len(lines)} changes"
              f"{'' if options.systems else ' (system parts left out)'}; {len(bad)} bad exits")
        for line in lines:
            print(line)
        if bad:
            print("bad exits:")
            for line in bad:
                print("  " + line)
        if count:
            print("by step, component and kind:")
            for line in count:
                print(line)
    return 1 if options.fail and (lines or bad) else 0


def self_test():
    log = [
        "[game] [warning] Frame write guard: the frame step Hand changed the synced part Transform at turn 12 (entity 7)",
        "[game] [warning] Frame write guard: the frame step Hand changed the synced part Transform.position at turn 12"
        " (entity 7)",
        "[game] [warning] Frame write guard: the frame step Entities changed the synced part order of Mesh at turn 3",
        "[game] [warning] Frame write guard: the frame step FrameEffects changed the synced part system snow at turn 1"
        " (values 0,2)",
        "[game] [warning] Frame write guard: the frame step Video changed the synced part entities at turn 5",
        "[game] [info] something else",
    ]
    changes = parse_log(log)
    assert [c["kind"] for c in changes] == ["value", "field", "order", "system", "rows"], changes
    assert changes[0]["entity"] == 7 and changes[2]["component"] == "Mesh" and changes[3]["values"] == "0,2"
    record = {"item": "a.b", "code": 0, "timeout": False, "seconds": 1.0, "changes": changes}
    lines, bad, count = summarise([record, {"item": "c.d", "code": 1, "timeout": True, "seconds": 9, "changes": []}])
    assert "a.b  Hand  Transform  field  turn 12 entity 7 (position)" in lines, lines
    assert "a.b  Entities  Mesh  order  turn 3" in lines, lines
    assert not any("snow" in line for line in lines) and bad == ["c.d exit 1 timeout"], (lines, bad)
    lines, _, _ = summarise([record], systems=True)
    assert "a.b  FrameEffects  snow  system  turn 1 (values 0,2)" in lines, lines
    assert kind_of("synced random draws") == ("draws", "-")
    print("self-test passed")
    return 0


def main():
    if "--self-test" in sys.argv[1:]:
        return self_test()
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    sweep = commands.add_parser("run", help="run the guard over the scenarios and lands")
    sweep.add_argument("--out", required=True)
    sweep.add_argument("--bin", help="the folder of the game; the build of this tree by default")
    sweep.add_argument("--config", default="RelWithDebInfo")
    sweep.add_argument("--game-path", default=sim_paths.DEFAULT_GAME_PATH)
    sweep.add_argument("--scenarios", default="all", help="all, a list with commas, @file, or empty for none")
    sweep.add_argument("--lands", default="all", help="all, a list with commas of " + ",".join(LANDS) + ", or empty")
    sweep.add_argument("--turns", type=int, default=80)
    sweep.add_argument("--land-turns", type=int, default=1000, help="the turns of each land")
    sweep.add_argument("--frame-ms", type=int, default=50)
    sweep.add_argument("--draw-every", type=int, default=0)
    sweep.add_argument("--fields", action="store_true", help="also tell the fields that changed (slower)")
    sweep.add_argument("--no-systems", dest="systems", action="store_false",
                       help="do not look at the saved state of the systems")
    sweep.add_argument("--network-game", action="store_true", help="play the scenarios as a network game")
    sweep.add_argument("--timeout", type=int, default=300, help="the most seconds of one game")
    sweep.add_argument("--limit", type=int, default=0)
    report = commands.add_parser("summary", help="tell the changes of a sweep folder")
    report.add_argument("out")
    report.add_argument("--systems", action="store_true", help="also tell the changes of the systems' saved state")
    report.add_argument("--json", action="store_true")
    report.add_argument("--fail", action="store_true", help="exit 1 when a change or a bad exit is found")
    options = parser.parse_args()
    return run(options) if options.command == "run" else summary(options)


if __name__ == "__main__":
    sys.exit(main())
