#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Starts a simulation run of openblack and writes its summary report.

The game runs with a fixed frame time and a seed, so that the run goes the same way at any speed. In the fast mode it
plays its frames one after the other and draws none of them. It writes a run record (JSON lines) every some turns, and
the stability checks flag the problems that they find. The player style drives the hand through the inspector at
fixed turns, so that a style gives the same run each time:

    none      no input at all
    neglect   the hand stays far from the creature, and does nothing
    good      the hand strokes the creature each time some turns go by
    evil      the hand slaps the creature each time some turns go by
    training  the hand trains the creature to be good, as a player does: after each thing it did that it can learn
              from, a stroke when the thing moves the creature to good and a slap when it moves it to evil (see
              training_feedback)

With --stages, the run starts the creature at the first stage of growing up and moves it on one stage at a time, as
the story's lessons do, up to the last; the run then continues as the creature grows old.

    python tools/sim/sim_run.py run --turns 20000 --style good --out E:/runs/good1
    python tools/sim/sim_run.py run --testbed --scenario creature.young --turns 5000
    python tools/sim/sim_run.py match --turns 1500 --every 10      (the normal speed against the fast mode)
    python tools/sim/sim_run.py repeat --turns 3000 --every 1      (two fast runs with the same seed)
    python tools/sim/sim_run.py repeat --turns 2000 --every 1 --runs 20   (twenty runs, each against the first)
    python tools/sim/sim_run.py repeat --turns 2000 --every 1 --runs 5 --frames 10,17,20:1,33,10:1
                                                   (five runs at other frame rates, drawn and not: the synced worlds)
    python tools/sim/sim_run.py repeat --turns 2000 --every 1 --runs 5 --load-at 700
                                                   (runs 2 to 5 save at turn 700 and load that save at once; each
                                                   must go on as the first run, which does not load)
    python tools/sim/sim_run.py roundtrip --at 0,500,2000
                                                   (at each turn: save, load that save, save again at once; the two
                                                   saves must have the same bytes)
"""

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "inspector"))
sys.path.insert(0, HERE)

import openblack_inspector_mcp as inspector  # noqa: E402
import save_diff  # noqa: E402
import sim_report  # noqa: E402

DEFAULT_GAME_PATH = os.environ.get("OB_GAME_PATH", "C:/projects/black")
DEFAULT_BUILDS = os.environ.get("OB_BUILDS", "E:/openblack/builds")
# The frame time of a run, in milliseconds: five frames a turn, near the screen's 60 a second
DEFAULT_FRAME_MS = 20
# How long the driver waits for the game to start, and for a step to end, in seconds
START_TIMEOUT = 300.0
STEP_TIMEOUT = 600.0
# The training style judges what the creature did as the game's alignment does: a plan for a desire that the game's
# table of desires (info.dat, the alignment change of each desire) says moves the creature to good is stroked, and one
# it says moves the creature to evil is slapped. In the game's data these are Compassion, Be friends and Educate friend
# (good), and Anger and Steal (evil). A hunger plan that eats a villager moves it to evil too (the alignment table).
# All other plans move nothing, so the style gives them no feedback.
TRAINING_EATS = "Hunger"
# The game's belief type of a villager
VILLAGER_BELIEF = 6
STYLES = ("none", "neglect", "good", "evil", "training")
# The script native that moves a creature to a stage of growing up, as the story's lessons do
STAGE_NATIVE = "SET_CREATURE_DEV_STAGE"
LAST_STAGE = 13


def worktree_root():
    return os.path.normpath(os.path.join(HERE, "..", ".."))


def default_exe(config):
    name = os.path.basename(worktree_root())
    return os.path.join(DEFAULT_BUILDS, name, "bin", config, "openblack.exe")


def game_arguments(options, record, log, until_turn, fast, driven):
    """The command line of the game for a run"""
    args = ["-g", options.game_path, "--skip-logos", "--seed", str(options.seed), "--inspect-port", "0",
            "--inspect-lock-input", "--sim-frame-ms", str(options.frame_ms), "--sim-until-turn", str(until_turn),
            "--sim-record", record, "--sim-record-every", str(options.every), "-l", log, "-n", "4000000000",
            "--background"]
    if fast:
        args += ["--sim-fast", "--sim-draw-every", str(options.draw_every)]
    if getattr(options, "still_hand", False):
        args.append("--sim-still-hand")
    if options.limits:
        args += ["--sim-limits", options.limits]
    if getattr(options, "hash_fields", False):
        args.append("--sim-hash-fields")
    elif options.hash_parts:
        args.append("--sim-hash-parts")
    # A driven run starts paused, so that the driver gives its input at the same turns each time
    if driven:
        args.append("--sim-start-paused")
    if options.scenario:
        args += ["--scenario", options.scenario, "--scenario-hide-window"]
    elif options.testbed:
        args.append("--testbed")
    else:
        args += ["-s", options.land]
        if options.opening:
            args += ["--skip-opening", options.opening]
    if getattr(options, "network_game", False):
        args.append("--sim-network-game")
    args += options.extra
    return args


class Game:
    """One run of the game that the driver started, found among the running games by its pid"""

    def __init__(self, exe, args, cwd):
        os.makedirs(cwd, exist_ok=True)
        environment = dict(os.environ)
        environment["OPENBLACK_WORKTREE"] = worktree_root()
        # The game never opens a console window of its own, which would come to the front
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self.process = subprocess.Popen([exe] + args, cwd=cwd, env=environment, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL, creationflags=flags)
        self.connection = None

    def connect(self, timeout=START_TIMEOUT):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(f"the game stopped as it started (exit {self.process.returncode})")
            games = [game for game in inspector.read_games(inspector.discovery_folder())
                     if game.get("pid") == self.process.pid]
            if games:
                self.connection = inspector.InspectorConnection(games[0], timeout=30.0)
                return
            time.sleep(0.5)
        raise RuntimeError("the game's inspector did not start")

    def call(self, query, params=None):
        answer = self.connection.request({"query": query, "params": params or {}})
        if not answer.get("ok"):
            raise RuntimeError(f"{query}: {answer.get('error')}")
        return answer.get("result")

    def wait_ready(self, timeout=START_TIMEOUT):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                state = self.call("game.state")
                if state.get("ready"):
                    return state
            except (RuntimeError, OSError, inspector.GameError):
                pass
            time.sleep(0.5)
        raise RuntimeError("the game did not get ready")

    def step(self, params, poll):
        """Runs the game for some frames or turns, then holds it there: false once the game has ended"""
        try:
            self.call("game.step", params)
            deadline = time.monotonic() + STEP_TIMEOUT
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    return False
                if self.call("game.state").get("stepping") is None:
                    return True
                time.sleep(poll)
        except (OSError, inspector.GameError):
            # The game ends itself at its last turn, also in the middle of a step
            if self.process.poll() is not None or self.process.wait(timeout=30) is not None:
                return False
            raise
        raise RuntimeError("a step did not end")

    def step_turns(self, turns):
        return self.step({"turns": turns}, 0.05)

    def step_frames(self, frames):
        return self.step({"frames": frames}, 0.02)

    def wait(self):
        return self.process.wait()

    def stop(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                self.process.kill()


# The hand's work: the inspector's sim.feedback has the player's hand take hold of the creature by a command and
# stroke or slap it, as the mouse button would, then let go. The creature learns from the sum as the hand lets go.
STROKE_PARTS = (3, 0, 4)
# Frames between two strokes, for the stroke's clocks (1.25 s at 20 ms a frame), and before a slap (1 s)
STROKE_WAIT_FRAMES = 70
SLAP_WAIT_FRAMES = 55


def creature_of(game):
    sample = game.call("sim.sample")
    creature = sample.get("creature")
    return creature.get("id") if creature else None


def stroke(game, creature):
    """Strokes the creature on three parts of its body, then lets go: whether a stroke counted"""
    stroked = False
    for part in STROKE_PARTS:
        answer = game.call("sim.feedback", {"kind": "stroke", "id": creature, "part": part})
        stroked = stroked or answer.get("did") == "stroked"
        game.step_frames(STROKE_WAIT_FRAMES)
    game.call("sim.feedback", {"kind": "release", "id": creature})
    game.step_frames(2)
    return stroked


def slap(game, creature):
    """Takes hold of the creature, waits until a slap is ready, slaps it, then lets go: whether the slap counted"""
    game.call("sim.feedback", {"kind": "slap", "id": creature})
    game.step_frames(SLAP_WAIT_FRAMES)
    answer = game.call("sim.feedback", {"kind": "slap", "id": creature})
    game.step_frames(5)
    game.call("sim.feedback", {"kind": "release", "id": creature})
    game.step_frames(2)
    return answer.get("did") == "slapped"


def park_hand(game):
    """Moves the pointer to a corner of the window, away from the creature"""
    game.call("input.pointer", {"screen": [8, 8]})
    game.step_frames(2)


def act(game, style, state):
    """The style's input after a step"""
    creature = creature_of(game)
    if creature is None:
        return None
    if style == "neglect":
        park_hand(game)
        return "parked"
    if style == "good":
        return "stroke" if stroke(game, creature) else None
    if style == "evil":
        return "slap" if slap(game, creature) else None
    if style == "training":
        record = game.call("sim.sample").get("creature") or {}
        context = record.get("context")
        key = (record.get("plan_serial"), json.dumps(context, sort_keys=True))
        if context is None or key == state.get("last_context"):
            return None
        state["last_context"] = key
        feedback = training_feedback(context)
        if feedback == "stroke":
            return "stroke" if stroke(game, creature) else None
        if feedback == "slap":
            return "slap" if slap(game, creature) else None
    return None


def training_feedback(context):
    """What the training style gives for the newest thing the creature did: "stroke", "slap" or None. Only a thing it can
    learn from, and that is over, gets feedback."""
    if not context or not context.get("learnable") or context.get("running"):
        return None
    if context.get("desire") == TRAINING_EATS and context.get("object_kind") == VILLAGER_BELIEF:
        return "slap"
    change = context.get("alignment_change") or 0.0
    if change > 0.0:
        return "stroke"
    if change < 0.0:
        return "slap"
    return None


def set_stage(game, stage):
    creature = creature_of(game)
    if creature is None:
        return False
    game.call("script.call", {"native": STAGE_NATIVE, "args": [{"object": creature}, {"int": stage}]})
    return True


def drive(game, options, until_turn):
    """Steps the game turn by turn group and gives the style's input; the log of what it did"""
    actions = []
    state = {}
    game.wait_ready()
    stage = 0 if options.stages else None
    next_stage_turn = 0
    turn = game.call("game.state").get("turn", 0)
    while turn < until_turn:
        if stage is not None and turn >= next_stage_turn and stage <= LAST_STAGE:
            if set_stage(game, stage):
                actions.append({"turn": turn, "did": f"stage {stage}"})
                stage += 1
                next_stage_turn = turn + options.stages
        try:
            if options.style != "none" and turn > 0:
                did = act(game, options.style, state)
                if did:
                    actions.append({"turn": turn, "did": did})
            step = min(options.act_every, until_turn - turn)
            if not game.step_turns(step):
                break
            turn = game.call("game.state").get("turn", turn)
        except (OSError, inspector.GameError):
            # The game ends itself at its last turn, also in the middle of an input or just after a step
            if game.process.poll() is not None or game.process.wait(timeout=30) is not None:
                break
            raise
    # The game ends itself at its last turn
    try:
        game.call("game.resume")
    except (RuntimeError, OSError, inspector.GameError):
        pass
    return actions


# The slots that the save and load checks use: the first save, and the save made at once after its load
FIRST_SLOT = 20
AGAIN_SLOT = 19


def newest_save(options, slot):
    """The newest saved world of a slot in the run's profiles; None when there is none"""
    saves = glob.glob(os.path.join(options.out, "Profiles", "**", f"{slot}.obsav"), recursive=True)
    return max(saves, key=os.path.getmtime) if saves else None


def read_save(options, slot):
    path = newest_save(options, slot)
    if path is None:
        raise RuntimeError(f"the game wrote no world file for slot {slot}")
    with open(path, "rb") as file:
        return file.read()


def save_and_load(game, options):
    """Saves the game, then loads that save and saves again in the same call: the two saved worlds as bytes"""
    # A scenario can load a land of its own; the save waits until the game is ready again
    game.wait_ready()
    if not game.call("saves.save", {"slot": FIRST_SLOT, "name": "check"}).get("saved"):
        raise RuntimeError("the first save did not happen")
    first = read_save(options, FIRST_SLOT)
    answer = game.call("saves.load_and_save", {"slot": FIRST_SLOT, "into": AGAIN_SLOT, "name": "check again"})
    if not answer.get("loaded") or not answer.get("saved"):
        raise RuntimeError(f"the load and the second save did not happen: {answer}")
    return first, read_save(options, AGAIN_SLOT)


def drive_to(game, until_turn, at_turns, action):
    """Steps the game to each turn of the list and calls the action there, then lets it run to its end"""
    game.wait_ready()
    turn = game.call("game.state").get("turn", 0)
    results = []
    for at in sorted(at_turns):
        if at > turn and not game.step_turns(at - turn):
            break
        turn = game.call("game.state").get("turn", turn)
        results.append(action(turn))
    try:
        game.call("game.resume")
    except (RuntimeError, OSError, inspector.GameError):
        pass
    return results


def run_once(options, name, fast, until_turn, load_at=None, load=True):
    """One run of the game: its record, its log and its exit code. With load_at, the run is driven to that turn, and
    there (with load) it saves and loads the save at once, then goes on. Without load it only stops at that turn, so
    that it plays its turns as a run that loads does."""
    os.makedirs(options.out, exist_ok=True)
    record = os.path.join(options.out, f"{name}.jsonl")
    log = os.path.join(options.out, f"{name}.log")
    exe = options.exe or default_exe(options.config)
    driven = options.style != "none" or bool(options.stages) or load_at is not None
    game = Game(exe, game_arguments(options, record, log, until_turn, fast, driven), options.out)
    actions = []
    try:
        if load_at is not None:
            game.connect()

            def load_there(turn):
                if not load:
                    return {"turn": turn, "did": "stop"}
                first, again = save_and_load(game, options)
                return {"turn": turn, "did": "save and load", "save_differs": save_diff.compare_files(first, again)}

            actions = drive_to(game, until_turn, [load_at], load_there)
        elif driven:
            game.connect()
            actions = drive(game, options, until_turn)
        code = game.wait()
    finally:
        game.stop()
    with open(os.path.join(options.out, f"{name}.actions.json"), "w", encoding="utf-8") as file:
        json.dump(actions, file, indent=1)
    return record, log, code


def report(options, name, record, log, code):
    start, records, end = sim_report.load(record)
    with open(log, encoding="utf-8", errors="replace") as file:
        log_lines = file.readlines()
    result = sim_report.summary(start, records, end, log_lines, code)
    result["style"] = options.style
    result["land"] = options.scenario or ("testbed" if options.testbed else options.land)
    with open(os.path.join(options.out, f"{name}.summary.json"), "w", encoding="utf-8") as file:
        json.dump(result, file, indent=1)
    text = sim_report.markdown(result, f"Simulation run {name}: {result['land']}, style {options.style}")
    with open(os.path.join(options.out, f"{name}.report.md"), "w", encoding="utf-8") as file:
        file.write(text)
    return result, text


def command_run(options):
    name = options.name or f"{options.style}-seed{options.seed}"
    record, log, code = run_once(options, name, not options.normal, options.turns)
    result, text = report(options, name, record, log, code)
    sys.stdout.write(text)
    return 0 if result["verdict"] == "stable" else 1


def command_match(options):
    """The same run at the normal speed, drawn, and in the fast mode: their hashes must agree at every record"""
    normal = run_once(options, "match-normal", False, options.turns)
    fast = run_once(options, "match-fast", True, options.turns)
    _, records_normal, _ = sim_report.load(normal[0])
    _, records_fast, _ = sim_report.load(fast[0])
    result = sim_report.compare(records_normal, records_fast)
    result["normal_wall_s"] = (sim_report.load(normal[0])[2] or {}).get("wall_s")
    result["fast_wall_s"] = (sim_report.load(fast[0])[2] or {}).get("wall_s")
    result["exit_codes"] = [normal[2], fast[2]]
    with open(os.path.join(options.out, "match.json"), "w", encoding="utf-8") as file:
        json.dump(result, file, indent=1)
    print(json.dumps(result))
    return 0 if result["same"] else 1


def keep_exit_save(options, name):
    """A copy of the world that the run saved as it ended, beside its record; None when it saved none"""
    saves = glob.glob(os.path.join(options.out, "Profiles", "**", "*.obsav"), recursive=True)
    if not saves:
        return None
    kept = os.path.join(options.out, f"{name}.obsav")
    shutil.copyfile(max(saves, key=os.path.getmtime), kept)
    return kept


def compare_exit_saves(first, other):
    """How the worlds that two runs saved as they ended differ, by block; None when a run saved none"""
    if first is None or other is None:
        return None
    with open(first, "rb") as file_a, open(other, "rb") as file_b:
        return sim_report.compare_saves(file_a.read(), file_b.read())


def parse_frames(text):
    """The frame settings of the runs of a frame-rate repeat, from "10,16.7,20:1": each item is the frame time in
    milliseconds, with a part of a millisecond if needed, and after a colon the draw every (0, no frame drawn, by
    default)"""
    frames = []
    for item in (text or "").split(","):
        item = item.strip()
        if not item:
            continue
        ms, _, draw = item.partition(":")
        value = float(ms)
        frames.append((int(value) if value.is_integer() else value, int(draw or 0)))
    return frames


def repeat_result(records_first, records_other, synced=False):
    """How a repeat run compares with the first: where it parts, the counts of its records and its last count of
    draws. With synced, only the synced part of the records counts (a frame-rate repeat)."""
    result = (sim_report.compare_synced if synced else sim_report.compare)(records_first, records_other)
    if synced:
        result["first_by_part"] = sim_report.synced_part_firsts(records_first, records_other)
    result["records"] = [len(records_first), len(records_other)]
    result["draws_last"] = [(records_first or [{}])[-1].get("draws"), (records_other or [{}])[-1].get("draws")]
    return result


def command_repeat(options):
    """The same run several times in the fast mode, with the same seed: their hashes, seeds and counts of draws must
    agree at every record. Each run is compared with the first, and every run that parts from it is reported with its
    first record that differs."""
    runs = max(options.runs, 2)
    # A frame-rate repeat gives each run the next frame setting of the list, and compares only the synced worlds
    frames = parse_frames(options.frames)
    # A driven run (--load-at) waits paused between its steps, and the paused frames move the local draws and the
    # camera on: then only the synced worlds must agree
    synced = bool(frames) or options.load_at is not None

    def run_options(index):
        if not frames:
            return options
        # The hand gives no input in a frame-rate repeat: a hand that follows the pointer eases to it by the frames
        ms, draw = frames[(index - 1) % len(frames)]
        return argparse.Namespace(**{**vars(options), "frame_ms": ms, "draw_every": draw, "still_hand": True})

    # With --load-at the first run stops at the same turn as the others but does not load
    first = run_once(run_options(1), "repeat-1", True, options.turns, options.load_at, False)
    first_save = keep_exit_save(options, "repeat-1")
    _, records_first, _ = sim_report.load(first[0])
    results = []
    for index in range(2, runs + 1):
        other = run_once(run_options(index), f"repeat-{index}", True, options.turns, options.load_at)
        _, records_other, _ = sim_report.load(other[0])
        result = repeat_result(records_first, records_other, synced)
        result["run"] = index
        if frames:
            result["frame_ms"] = [run_options(1).frame_ms, run_options(index).frame_ms]
            result["draw_every"] = [run_options(1).draw_every, run_options(index).draw_every]
        result["exit_codes"] = [first[2], other[2]]
        # Information only: some saved fields follow the graphics card's handles, not the game (see the handover)
        result["save_differs"] = compare_exit_saves(first_save, keep_exit_save(options, f"repeat-{index}"))
        results.append(result)
        shown = ("run", "same", "turn", "differs", "parts", "draws_last", "save_differs", "frame_ms", "draw_every")
        print(json.dumps({key: result.get(key) for key in shown}), flush=True)
    divergences = [result for result in results if not result["same"]]
    exit_ok = all(code == 0 for result in results for code in result["exit_codes"])
    # With two runs the report keeps the form it had, the comparison of the second run with the first
    summary = dict(results[0]) if runs == 2 else {}
    summary.update({
        "same": not divergences and exit_ok,
        "runs": runs,
        "frames": frames,
        "divergences": len(divergences),
        "diverged": [{key: result.get(key) for key in ("run", "turn", "frame", "differs", "seeds", "draws", "hash",
                                                         "counts", "parts", "draws_last", "first_by_part")}
                     for result in divergences],
        "results": results,
    })
    with open(os.path.join(options.out, "repeat.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=1)
    print(json.dumps({key: summary[key] for key in ("same", "runs", "divergences", "diverged")}))
    return 0 if summary["same"] else 1


def command_roundtrip(options):
    """At each turn of --at: saves, loads that save and saves again at once. The two saves must have the same bytes."""
    at_turns = [int(turn) for turn in options.at.split(",") if turn.strip()]
    os.makedirs(options.out, exist_ok=True)
    record = os.path.join(options.out, "roundtrip.jsonl")
    log = os.path.join(options.out, "roundtrip.log")
    exe = options.exe or default_exe(options.config)
    until_turn = max(at_turns) + 1
    game = Game(exe, game_arguments(options, record, log, until_turn, True, True), options.out)

    def check(turn):
        first, again = save_and_load(game, options)
        for slot, data in ((FIRST_SLOT, first), (AGAIN_SLOT, again)):
            with open(os.path.join(options.out, f"turn{turn}-slot{slot}.obsav"), "wb") as file:
                file.write(data)
        result = save_diff.compare_files(first, again)
        print(f"turn {turn}: " + save_diff.text_report(result), end="", flush=True)
        return {"turn": turn, "same": not result, "differs": result}

    code = None
    results = []
    try:
        game.connect()
        results = drive_to(game, until_turn, at_turns, check)
        code = game.wait()
    finally:
        game.stop()
    summary = {"same": len(results) == len(at_turns) and all(r["same"] for r in results), "exit_code": code,
               "results": results}
    with open(os.path.join(options.out, "roundtrip.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=1)
    print(json.dumps({"same": summary["same"], "turns": [r["turn"] for r in results], "exit_code": code}))
    return 0 if summary["same"] else 1


def parse(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    for command in ("run", "match", "repeat", "roundtrip"):
        sub = commands.add_parser(command)
        sub.add_argument("--land", default="Land1.txt", help="the land's script (Land1.txt by default)")
        sub.add_argument("--opening", default="cow",
                         help="the start-of-game answer (--skip-opening) on a land: cow by default; '' for none")
        sub.add_argument("--testbed", action="store_true", help="the creature testbed instead of a land")
        sub.add_argument("--scenario", help="a testbed scenario by its id")
        sub.add_argument("--seed", type=int, default=1)
        sub.add_argument("--style", choices=STYLES, default="none")
        sub.add_argument("--turns", type=int, default=3000, help="the turn to stop at")
        sub.add_argument("--every", type=int, default=100, help="a record each time this many turns go by")
        sub.add_argument("--act-every", type=int, default=200, help="the turns between two inputs of the style")
        sub.add_argument("--stages", type=int, default=0,
                         help="start the creature at stage 0 and move it on one stage each time this many turns go by")
        sub.add_argument("--frame-ms", type=float, default=DEFAULT_FRAME_MS,
                         help="the game time of each frame in ms, with a part of a millisecond if needed (16.7)")
        sub.add_argument("--draw-every", type=int, default=0, help="in the fast mode, draw one frame in this many")
        sub.add_argument("--normal", action="store_true", help="run at the normal speed, drawn, not in the fast mode")
        sub.add_argument("--hash-parts", action="store_true")
        sub.add_argument("--hash-fields", action="store_true",
                         help="as --hash-parts, and the synced hash of each field of each component (large records)")
        sub.add_argument("--limits", default=os.path.join(HERE, "stuck_limits.json"))
        sub.add_argument("--config", default="RelWithDebInfo")
        sub.add_argument("--exe")
        sub.add_argument("--game-path", default=DEFAULT_GAME_PATH)
        sub.add_argument("--out", default=os.path.join(os.getcwd(), "sim-runs"))
        sub.add_argument("--name")
        sub.add_argument("--runs", type=int, default=2,
                         help="repeat: how many runs to make; each is compared with the first (2 by default)")
        sub.add_argument("--frames",
                         help="repeat: a frame-rate repeat, with the frame setting of each run in turn, as "
                              "'10,17,20:1' (the frame time in ms, and after a colon the draw every); only the synced "
                              "worlds must agree")
        sub.add_argument("--load-at", type=int,
                         help="repeat: runs 2 and on save at this turn and load that save at once, then go on")
        sub.add_argument("--at", default="0,500,2000",
                         help="roundtrip: the turns to save, load and save again at, as '0,500,2000'")
        sub.add_argument("--network-game", action="store_true",
                         help="play the world as a network game does: no pause, and the turns read no part of a frame")
        sub.add_argument("extra", nargs="*", help="more arguments for the game, after --")
    return parser.parse_args(argv)


def main(argv=None):
    options = parse(argv)
    options.out = os.path.abspath(options.out)
    if options.limits:
        options.limits = os.path.abspath(options.limits)
    commands = {"run": command_run, "match": command_match, "repeat": command_repeat, "roundtrip": command_roundtrip}
    return commands[options.command](options)


if __name__ == "__main__":
    sys.exit(main())
