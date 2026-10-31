#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Scenarios recorded from play: a play-through that a person makes once, then checks again at any time.

    python tools/sim/recorded_scenario.py record NAME [--land Land1.txt | --scenario ID | --testbed] --turns N
    python tools/sim/recorded_scenario.py check MANIFEST... [--config RelWithDebInfo]

record starts the game in a window, at the normal speed, with a fixed frame time. The person plays until the game
reaches turn N, and the game then ends. The game writes the input that the person made and the hash of the synced world
every few turns. record keeps both in a folder, with a manifest that says how to start the same play again.

check starts the game again for each manifest, in the fast mode and without a window, with the recorded input in place of
the person. It passes when the hash of the synced world is the same at every turn that the recording kept. The exit code
is 0 when every manifest passes, 1 when one does not, and 2 when a manifest or a run is unusable. This is developer
tooling, and no game logic.
"""

import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import sim_paths  # noqa: E402
import sim_report  # noqa: E402

MANIFEST_VERSION = 1
# The frame time of a recording and of its replay, in milliseconds. The recorded input is by frame number, so the two
# must play the same frames.
FRAME_MS = 20
DEFAULT_EVERY = 100


def manifest_of(name, start, turns, seed, every, records, input_file):
    """The manifest of a recording: how to start the play again and the hash of the synced world at each recorded turn.

    start is {"land": ...}, {"scenario": ...} or {"testbed": True}."""
    if sum(key in start for key in ("land", "scenario", "testbed")) != 1:
        raise ValueError("a recording starts on a land, a scenario or the testbed")
    hashes = {}
    for record in records:
        if "turn" in record and record.get("hash"):
            hashes[str(record["turn"])] = record["hash"]
    if not hashes:
        raise ValueError("the run record has no hash")
    return {
        "version": MANIFEST_VERSION,
        "name": name,
        "start": start,
        "turns": turns,
        "seed": seed,
        "every": every,
        "frame_ms": FRAME_MS,
        "input": input_file,
        "hashes": hashes,
    }


def read_manifest(path):
    with open(path, encoding="utf-8") as file:
        manifest = json.load(file)
    if manifest.get("version") != MANIFEST_VERSION:
        raise ValueError(f"{path}: the manifest has version {manifest.get('version')}, not {MANIFEST_VERSION}")
    for key in ("name", "start", "turns", "seed", "every", "input", "hashes"):
        if key not in manifest:
            raise ValueError(f"{path}: the manifest has no {key}")
    return manifest


def start_arguments(start):
    """The game arguments that start the land, the scenario or the testbed of a recording"""
    if "scenario" in start:
        return ["--scenario", start["scenario"], "--scenario-hide-window"]
    if start.get("testbed"):
        return ["--testbed"]
    return ["-s", start["land"], "--skip-opening", "cow"]


def compare_hashes(expected, records):
    """The first turn where a replay differs from the recording, as {"turn", "expected", "got"}, and how many turns were
    compared. A turn that the recording kept and the replay did not reach counts as a difference."""
    by_turn = {str(record["turn"]): record.get("hash") for record in records if "turn" in record}
    compared = 0
    for turn in sorted(expected, key=int):
        got = by_turn.get(turn)
        if got is None:
            return {"same": False, "compared": compared, "turn": int(turn), "expected": expected[turn], "got": None}
        compared += 1
        if got != expected[turn]:
            return {"same": False, "compared": compared, "turn": int(turn), "expected": expected[turn], "got": got}
    return {"same": True, "compared": compared}


def exe_of(options):
    return options.exe or sim_paths.program(sim_paths.default_bin(options.config), "openblack")


def common_arguments(options, manifest_or_args):
    return ["-g", options.game_path, "--skip-logos", "--seed", str(manifest_or_args["seed"]), "--sim-frame-ms",
            str(FRAME_MS), "--sim-record-every", str(manifest_or_args["every"])]


def command_record(options):
    os.makedirs(options.out, exist_ok=True)
    record_path = os.path.join(options.out, f"{options.name}.jsonl")
    input_name = f"{options.name}.input.json"
    start = {"scenario": options.scenario} if options.scenario else {"testbed": True} if options.testbed else {"land": options.land}
    args = common_arguments(options, {"seed": options.seed, "every": options.every})
    args += ["--sim-until-turn", str(options.turns), "--sim-record", record_path, "--sim-record-input",
             os.path.join(options.out, input_name), "-n", "4000000000"]
    args += start_arguments(start)
    print(f"Play until turn {options.turns}; the game then ends and the recording is kept.", flush=True)
    code = subprocess.call([exe_of(options)] + args, cwd=options.out)
    if code != 0:
        print(f"the game ended with exit code {code}", file=sys.stderr)
        return 2
    _, records, _ = sim_report.load(record_path)
    manifest = manifest_of(options.name, start, options.turns, options.seed, options.every, records, input_name)
    manifest_path = os.path.join(options.out, f"{options.name}.recorded.json")
    with open(manifest_path, "w", encoding="utf-8") as file:
        json.dump(manifest, file, indent=1)
    print(f"Kept {manifest_path} with {len(manifest['hashes'])} hashes")
    return 0


def check_one(options, path):
    manifest = read_manifest(path)
    folder = os.path.dirname(os.path.abspath(path))
    os.makedirs(options.out, exist_ok=True)
    name = manifest["name"]
    record = os.path.join(options.out, f"{name}.check.jsonl")
    log = os.path.join(options.out, f"{name}.check.log")
    args = common_arguments(options, manifest)
    args += ["--inspect-port", "0", "--inspect-lock-input", "--sim-until-turn", str(manifest["turns"]), "--sim-record",
             record, "-l", log, "-n", "4000000000", "--background", "--sim-fast", "--sim-draw-every", "0",
             "--sim-input", os.path.join(folder, manifest["input"])]
    args += start_arguments(manifest["start"])
    code = subprocess.call([exe_of(options)] + args, cwd=options.out, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if code != 0:
        return {"name": name, "same": False, "error": f"the game ended with exit code {code}"}
    _, records, _ = sim_report.load(record)
    result = compare_hashes(manifest["hashes"], records)
    result["name"] = name
    return result


def command_check(options):
    worst = 0
    for path in options.manifests:
        try:
            result = check_one(options, path)
        except (OSError, ValueError, KeyError) as error:
            print(f"UNUSABLE {path}: {error}")
            worst = max(worst, 2)
            continue
        if result.get("error"):
            print(f"UNUSABLE {result['name']}: {result['error']}")
            worst = max(worst, 2)
        elif result["same"]:
            print(f"PASS {result['name']}: {result['compared']} hashes the same")
        else:
            print(f"FAIL {result['name']}: turn {result['turn']} expected {result['expected']} got {result['got']}")
            worst = max(worst, 1)
    return worst


def parse(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    record = commands.add_parser("record")
    record.add_argument("name")
    where = record.add_mutually_exclusive_group()
    where.add_argument("--land", default="Land1.txt")
    where.add_argument("--scenario")
    where.add_argument("--testbed", action="store_true")
    record.add_argument("--turns", type=int, required=True)
    record.add_argument("--seed", type=int, default=1)
    record.add_argument("--every", type=int, default=DEFAULT_EVERY)
    check = commands.add_parser("check")
    check.add_argument("manifests", nargs="+")
    for sub in (record, check):
        sub.add_argument("--config", default="RelWithDebInfo")
        sub.add_argument("--exe")
        sub.add_argument("--game-path", default=sim_paths.DEFAULT_GAME_PATH)
        sub.add_argument("--out", default=os.path.join(os.getcwd(), "recorded-scenarios"))
    return parser.parse_args(argv)


def main(argv=None):
    options = parse(argv)
    options.out = os.path.abspath(options.out)
    return {"record": command_record, "check": command_check}[options.command](options)


if __name__ == "__main__":
    sys.exit(main())
