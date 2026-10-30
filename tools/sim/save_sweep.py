#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Round trips of the saved world over many testbed scenarios, or over a land, in one run of the game.

At each check the game is held, and three round trips follow with no frame and no turn between:

    world state   the world state that a game server sends is made, loaded and made again (saves.world_state_round_trip).
                  The two states must have the same bytes: a server snapshot and a rejoin load world states.
    saved game    the game saves (A), loads A and saves (B), loads B and saves (C). Each load of a saved game starts a
                  game, as the game does: the player takes the alignment that the profile keeps, and the statistics
                  note the start again. B and C must have the same bytes; A and B can differ only by these starts.

    python tools/sim/save_sweep.py --exe <openblack> --out E:/runs/sweep --scenarios all --turns 100
    python tools/sim/save_sweep.py --exe <openblack> --out E:/runs/land1 --land Land1.txt --at 0,500,2000

The result is one line of JSON for each check in sweep.jsonl, and a summary in sweep.json. The exit code is 0 only when
every world state and every second saved-game round trip has the same bytes.
"""

import argparse
import glob
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "inspector"))
sys.path.insert(0, HERE)

import openblack_inspector_mcp as inspector  # noqa: E402
import save_diff  # noqa: E402
import sim_paths  # noqa: E402
import sim_run  # noqa: E402

# The slots of the three saves of a saved-game round trip: none of them is an autosave slot
SLOTS = (10, 11, 12)
# How long the tool waits for the writer of the game to put a save in its slot, in seconds
WRITE_TIMEOUT = 60.0


def game_arguments(options):
    args = ["-g", options.game_path, "--skip-logos", "--seed", str(options.seed), "--inspect-port", "0", "--background",
            "--sim-fast", "--sim-draw-every", "0", "--sim-frame-ms", "20", "--sim-start-paused", "-n", "4000000000"]
    if options.land:
        args += ["-s", options.land]
        if options.opening:
            args += ["--skip-opening", options.opening]
    else:
        args.append("--testbed")
    return args + options.extra


def saved_world(out, slot, since):
    """The world file of a slot that the game wrote after the time since, when the writer has put it there"""
    deadline = time.monotonic() + WRITE_TIMEOUT
    while time.monotonic() < deadline:
        found = [path for path in glob.glob(os.path.join(out, "Profiles", "**", f"{slot}.obsav"), recursive=True)
                 if os.path.getmtime(path) >= since]
        if found:
            path = max(found, key=os.path.getmtime)
            with open(path, "rb") as file:
                return file.read()
        time.sleep(0.1)
    raise RuntimeError(f"the game wrote no world file for slot {slot}")


def check(game, options, name):
    """The three round trips of one check: the result of each comparison"""
    state = game.wait_ready()
    entry = {"check": name, "turn": state.get("turn")}
    folder = os.path.join(options.out, "states")
    os.makedirs(folder, exist_ok=True)
    first_path = os.path.abspath(os.path.join(folder, "first.obsav"))
    again_path = os.path.abspath(os.path.join(folder, "again.obsav"))
    answer = game.call("saves.world_state_round_trip", {"first": first_path, "again": again_path})
    if not answer.get("loaded") or not answer.get("written"):
        entry["world_state"] = {"error": answer}
    else:
        with open(first_path, "rb") as file_a, open(again_path, "rb") as file_b:
            differs = save_diff.compare_files(file_a.read(), file_b.read())
        entry["world_state"] = {"same": not differs, "parts": save_diff.differing_parts(differs)}
        if differs and options.keep:
            entry["world_state"]["report"] = save_diff.text_report(differs)
    first_slot, second_slot, third_slot = SLOTS
    since = time.time() - 1.0
    if not game.call("saves.save", {"slot": first_slot, "name": "sweep"}).get("saved"):
        entry["saved_game"] = {"error": "the first save did not happen"}
        return entry
    a = saved_world(options.out, first_slot, since)
    results = {}
    for slot, into in ((first_slot, second_slot), (second_slot, third_slot)):
        since = time.time() - 1.0
        answer = game.call("saves.load_and_save", {"slot": slot, "into": into, "name": "sweep"})
        if not answer.get("loaded") or not answer.get("saved"):
            entry["saved_game"] = {"error": answer}
            return entry
        results[into] = saved_world(options.out, into, since)
    b, c = results[second_slot], results[third_slot]
    game_start = save_diff.compare_files(a, b)
    again = save_diff.compare_files(b, c)
    entry["saved_game"] = {"same": not again, "parts": save_diff.differing_parts(again),
                           "game_start": save_diff.differing_parts(game_start)}
    if options.keep:
        if again:
            entry["saved_game"]["report"] = save_diff.text_report(again)
        if game_start:
            entry["saved_game"]["game_start_report"] = save_diff.text_report(game_start)
    return entry


def scenario_ids(game, wanted):
    if wanted != "all":
        return [item for item in wanted.split(",") if item]
    return [item["id"] for item in game.call("game.scenarios")]


def start(options):
    game = sim_run.Game(options.exe, game_arguments(options), options.out)
    game.connect()
    game.wait_ready()
    return game


def sweep(options, record):
    """Every check of the run: a fresh game takes over when one stops"""
    results = []
    game = start(options)
    try:
        if options.land:
            turn = game.call("game.state").get("turn", 0)
            for at in sorted(int(item) for item in options.at.split(",") if item.strip()):
                if at > turn and not game.step_turns(at - turn):
                    break
                turn = game.call("game.state").get("turn", turn)
                results.append(check(game, options, f"{options.land}@{at}"))
                record.write(json.dumps(results[-1]) + "\n")
                record.flush()
            return results
        for scenario in scenario_ids(game, options.scenarios):
            try:
                game.call("game.scenario", {"id": scenario})
                game.wait_ready()
                if options.turns > 0:
                    game.step_turns(options.turns)
                entry = check(game, options, scenario)
            except (RuntimeError, OSError, inspector.GameError) as error:
                entry = {"check": scenario, "error": str(error)}
                if game.process.poll() is not None:
                    entry["exit_code"] = game.process.returncode
                game.stop()
                game = start(options)
            results.append(entry)
            record.write(json.dumps(entry) + "\n")
            record.flush()
            print(json.dumps({key: entry.get(key) for key in ("check", "turn", "error")} |
                             {"world_state": (entry.get("world_state") or {}).get("same"),
                              "saved_game": (entry.get("saved_game") or {}).get("same")}), flush=True)
        return results
    finally:
        game.stop()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", default=None, help="the game; by default the Debug build of this worktree")
    parser.add_argument("--config", default="Debug")
    parser.add_argument("--game-path", default=sim_paths.DEFAULT_GAME_PATH)
    parser.add_argument("--out", required=True)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--scenarios", default="all", help="'all' or a comma list of scenario ids")
    parser.add_argument("--turns", type=int, default=100, help="the turns each scenario plays before its check")
    parser.add_argument("--land", default=None, help="check a land, as Land1.txt, in place of the scenarios")
    parser.add_argument("--opening", default=None, help="with --land: the opening to skip, as cow")
    parser.add_argument("--at", default="0,500,2000", help="with --land: the turns of the checks")
    parser.add_argument("--keep", action="store_true", help="keep the text report of each difference")
    parser.add_argument("extra", nargs="*", help="more arguments for the game, after --")
    options = parser.parse_args(argv)
    options.exe = options.exe or sim_run.default_exe(options.config)
    os.makedirs(options.out, exist_ok=True)
    with open(os.path.join(options.out, "sweep.jsonl"), "w", encoding="utf-8") as record:
        results = sweep(options, record)
    failed = [entry["check"] for entry in results
              if "error" in entry or not (entry.get("world_state") or {}).get("same")
              or not (entry.get("saved_game") or {}).get("same")]
    summary = {"checks": len(results), "failed": failed, "same": not failed}
    with open(os.path.join(options.out, "sweep.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=1)
    print(json.dumps({"checks": len(results), "failed": len(failed), "same": not failed}))
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
