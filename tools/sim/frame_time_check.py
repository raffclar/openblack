#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Runs the same land at different frame times and finds the synced parts that change with the frame time.

The synced world must not change with the frame rate: each client of a network game runs at its own frame rate, and
the lockstep needs the same world on each one at each turn. This tool runs the land once for each frame setting, with
the same seed and a still hand, records the synced hash of each part at each turn (--hash-parts), and compares each run
with the reference run. For each run that parts, it gives each synced part that differs and the first turn at which it
differs.

A frame setting is a fixed frame time in milliseconds (the fast mode, nothing drawn), or "real": the game runs at its
real frame rate, drawn, and each frame takes the real time that it took, as a player's game does.

By default the reference run writes the commands of its player (the reports of the hand and the camera too), and each
other run does those commands at their turns and none of its own, as a computer that receives the commands of a network
game. The camera and the hand of each run then move by the frame rate, but the world gets the same input, so a part that
differs shows logic that the frame rate changes. With --own-commands, each run makes its own commands.

    python tools/sim/frame_time_check.py --turns 600
    python tools/sim/frame_time_check.py --turns 600 --frames 7,16.7,33,50,real --reference 16.7 --network-game
    python tools/sim/frame_time_check.py --exe E:/openblack/builds/master-bin/<commit>/RelWithDebInfo/openblack.exe

The report is frame_time.json in --out, and a table on the standard output. The exit code is 0 when all the runs keep
the same synced world as the reference run.
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import sim_report  # noqa: E402
import sim_run  # noqa: E402

DEFAULT_FRAMES = "7,16.7,33,50,real"
DEFAULT_REFERENCE = "16.7"
REAL = "real"


def parse_settings(text):
    """The frame settings from "7,16.7,real": each is a frame time in milliseconds or the word real"""
    settings = []
    for item in (text or "").split(","):
        item = item.strip().lower()
        if not item:
            continue
        if item != REAL:
            value = float(item)
            if not 1.0 <= value <= 1000.0:
                raise ValueError(f"a frame time from 1 to 1000 ms, not {item}")
            item = str(int(value)) if value.is_integer() else str(value)
        if item not in settings:
            settings.append(item)
    return settings


def setting_name(setting):
    return "frame-real" if setting == REAL else f"frame-{setting}ms"


def commands_path(options):
    """The file of the commands of the reference run, which the other runs do"""
    return os.path.join(options.out, "reference.commands.jsonl")


def run_options(options, setting, reference):
    """The options of sim_run for one frame setting: a fixed frame time in the fast mode, or the real frame time"""
    values = dict(vars(options))
    extra = list(options.extra)
    if not options.own_commands:
        extra += ["--sim-commands", commands_path(options)] if reference else ["--sim-play-commands",
                                                                              commands_path(options)]
    # Recorded input moves the hand of the reference run only: the other runs do its commands
    if options.input and (reference or options.own_commands):
        extra += ["--sim-input", os.path.abspath(options.input)]
    values.update({
        "frame_ms": None if setting == REAL else float(setting),
        "draw_every": 0,
        # A still hand gives each run the same hand, unless recorded input moves it
        "still_hand": not (options.input and (reference or options.own_commands)),
        "hash_parts": True,
        "hash_fields": options.hash_fields,
        "style": "none",
        "stages": 0,
        "scenario": options.scenario,
        "testbed": options.testbed,
        "extra": extra,
    })
    return argparse.Namespace(**values)


def run_setting(options, setting, reference=False):
    """One run at the frame setting: the path of its record and its exit code"""
    record, _, code = sim_run.run_once(run_options(options, setting, reference), setting_name(setting),
                                       setting != REAL, options.turns)
    return record, code


def frame_counts(records):
    """The first and the last frame number of a run's records, to show how many frames a turn took"""
    frames = [record.get("frame") for record in records if record.get("frame") is not None]
    return [frames[0], frames[-1]] if frames else None


def compare_runs(reference_records, other_records):
    """How a run compares with the reference run in the synced world: where it parts first, and each part that
    differs with its first turn"""
    result = sim_report.compare_synced(reference_records, other_records)
    result["first_by_part"] = sim_report.synced_part_firsts(reference_records, other_records, limit=1000)
    return result


def table(report):
    """The report as text: one line for each run, then the parts that differ in each run"""
    lines = [f"reference: {report['reference']}, turns {report['turns']}, land {report['land']}, "
             f"network game {report['network_game']}, own commands {report['own_commands']}"]
    for run in report["runs"]:
        state = "same" if run.get("same") else f"parts at turn {run.get('turn')}"
        lines.append(f"  {run['setting']:>6}: {state}; frames {run.get('frames')}; exit {run.get('exit_code')}")
        for part, turn in (run.get("first_by_part") or {}).items():
            lines.append(f"          {turn:>6}  {part}")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--frames", default=DEFAULT_FRAMES, help="the frame settings, as '7,16.7,33,50,real'")
    parser.add_argument("--reference", default=DEFAULT_REFERENCE, help="the frame setting that the others match")
    parser.add_argument("--land", default="Land1.txt")
    parser.add_argument("--opening", default="cow")
    parser.add_argument("--testbed", action="store_true")
    parser.add_argument("--scenario")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--turns", type=int, default=600)
    parser.add_argument("--every", type=int, default=1, help="a record each time this many turns go by")
    parser.add_argument("--network-game", action="store_true",
                        help="play the world as a network game does: no pause, and the turns read no part of a frame")
    parser.add_argument("--hash-fields", action="store_true",
                        help="the synced hash of each field of each component too (large records: few turns)")
    parser.add_argument("--input", help="recorded input for the reference run (tools/sim/hand_inputs/land1.json)")
    parser.add_argument("--own-commands", action="store_true",
                        help="each run makes its own commands, not the commands of the reference run")
    parser.add_argument("--limits", default=os.path.join(HERE, "stuck_limits.json"))
    parser.add_argument("--config", default="RelWithDebInfo")
    parser.add_argument("--exe")
    parser.add_argument("--game-path", default=sim_run.DEFAULT_GAME_PATH)
    parser.add_argument("--out", default=os.path.join(os.getcwd(), "sim-runs", "frame-time"))
    parser.add_argument("extra", nargs="*", help="more arguments for the game, after --")
    options = parser.parse_args(argv)
    options.out = os.path.abspath(options.out)
    options.limits = os.path.abspath(options.limits) if options.limits else None

    settings = parse_settings(options.frames)
    reference = parse_settings(options.reference)[0]
    if reference not in settings:
        settings.insert(0, reference)
    # The reference runs first, so that each other run is compared as soon as it ends
    settings.sort(key=lambda setting: setting != reference)

    os.makedirs(options.out, exist_ok=True)
    reference_record, reference_code = run_setting(options, reference, reference=True)
    _, reference_records, _ = sim_report.load(reference_record)
    report = {
        "reference": reference,
        "turns": options.turns,
        "land": options.scenario or ("testbed" if options.testbed else options.land),
        "network_game": options.network_game,
        "own_commands": options.own_commands,
        "reference_exit_code": reference_code,
        "reference_frames": frame_counts(reference_records),
        "runs": [],
    }
    for setting in settings[1:]:
        record, code = run_setting(options, setting)
        _, records, _ = sim_report.load(record)
        result = compare_runs(reference_records, records)
        result.update({"setting": setting, "exit_code": code, "frames": frame_counts(records)})
        report["runs"].append(result)
        print(json.dumps({key: result.get(key) for key in ("setting", "same", "turn", "parts")}), flush=True)
    report["same"] = all(run.get("same") for run in report["runs"]) and reference_code == 0 and all(
        run.get("exit_code") == 0 for run in report["runs"])
    with open(os.path.join(options.out, "frame_time.json"), "w", encoding="utf-8") as file:
        json.dump(report, file, indent=1)
    sys.stdout.write(table(report))
    return 0 if report["same"] else 1


if __name__ == "__main__":
    sys.exit(main())
