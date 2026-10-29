"""Checks that the dedicated server plays the same turns as the game.

The script runs the game in the fast simulation mode with a seed, and records the checksum of each turn and the commands
of each turn (--sim-commands). Then it runs apps/openblack-server at its own pace (10 turns a second) with the same seed,
the same land and the same frame time. The server does the commands of the game at the same turns (--commands), as if
the game sent them over the network. Each turn of the server must have the same checksum as the same turn of the game.

    python tools/sim/server_check.py --turns 100
    python tools/sim/server_check.py --turns 100 --config RelWithDebInfo --out E:/runs/server
    python tools/sim/server_check.py --turns 2000 --land Playgrounds/TwoGods.txt --input tools/sim/hand_inputs/twogods.json
    python tools/sim/server_check.py --turns 2000 --testbed --server-fast --wsl-server /root/ob/bin/openblack-server

With --wsl-server, the server is a Linux build that runs in WSL (wsl -d DISTRO): the script gives it the paths of the
game data, the log and the commands as WSL paths (/mnt/<drive>/...). So the Linux server and the Windows game play the
same turns, and each turn's checksum must be the same.

The hand of the game stays still, unless --input gives the game recorded input (--sim-input, from the first frame of
the land): then the hand moves, takes, drops, throws and casts, and the server sees only the commands.
An input of the hand needs a map with temples (TwoGods, the online maps, --gods-ring-map). Land 1 and the testbed
are checks with a still hand: a story land makes no temple at its start, so its network game is decided at the first
turn and the end box takes the input. A run with an input whose game was decided at its first turn is not the same.
The input can also be an object with the list as "events" and a "camera" as camera.set takes it: the game puts its camera
there before the first frame of the land, in place of the start view and its flight (tools/sim/hand_inputs/twogods.json).
The object can also have an "expect", the least count of each deed of the hand ({"scoops": 1, "throws": 1}). The summary
gives the deeds of the hand of each player that the server did ("Hand deed" lines of the logs: takes, scoops, throws,
puts_down, pours, uses), and a run whose hand did fewer than its input expects is not the same.

A testbed scenario can make the game load a saved world. The server keeps no saves and never loads, so the turns are
compared only up to the game's last turn before its first load (the summary gives that turn as
compared_until_load_at_turn).

Both play as the profile of a tool's run, in their own working folders. Both programs start with no console window of their
own. The game starts with --background. The script stops only the
processes that it started, by their own process ids.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_report  # noqa: E402
import sim_paths  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BUILDS = sim_paths.DEFAULT_BUILDS
DEFAULT_GAME_PATH = sim_paths.DEFAULT_GAME_PATH
CHECKSUM_LINE = re.compile(r"turn (\d+) checksum ([0-9a-f]{16})")
# A run that has not finished in this time is stopped
RUN_LIMIT_SECONDS = 900


def worktree_root():
    return os.path.normpath(os.path.join(HERE, "..", ".."))


def default_bin(config):
    return sim_paths.default_bin(config)


def run(command, cwd, limit=RUN_LIMIT_SECONDS, output=None):
    """Runs a program with no console window of its own, and stops it by its own process id after the limit. With an
    output path, the program's standard output and error go to that file."""
    os.makedirs(cwd, exist_ok=True)
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    sink = open(output, "wb") if output else subprocess.DEVNULL
    try:
        process = subprocess.Popen(command, cwd=cwd, stdin=subprocess.DEVNULL, stdout=sink,
                                   stderr=subprocess.STDOUT if output else subprocess.DEVNULL, creationflags=flags)
        try:
            return process.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            raise RuntimeError(f"{command[0]} (pid {process.pid}) did not finish in {limit} s")
    finally:
        if output:
            sink.close()


def wsl_path(path):
    """The path of a Windows file as WSL sees it: C:/a/b is /mnt/c/a/b"""
    path = os.path.abspath(path).replace("\\", "/")
    if len(path) > 1 and path[1] == ":":
        return f"/mnt/{path[0].lower()}{path[2:]}"
    return path


def server_checksums(log):
    """The checksum of each turn in the log of the server"""
    turns = {}
    with open(log, encoding="utf-8", errors="replace") as file:
        for line in file:
            match = CHECKSUM_LINE.search(line)
            if match:
                turns[int(match.group(1))] = match.group(2)
    return turns


def records_before_first_load(record):
    """The synced part of each turn's record up to the game's first load, as (turn, synced) pairs. A load of a saved world
    takes the turn back. The server never loads, so the turns after the load cannot be compared with it."""
    last = None
    with open(record, encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
            value = json.loads(line)
            synced = value.get("synced")
            if not isinstance(synced, dict) or "turn" not in value:
                continue
            turn = int(value["turn"])
            if last is not None and turn <= last:
                return
            last = turn
            yield turn, synced


def game_checksums(record):
    """The checksum of each turn in the run record of the game, up to its first load"""
    return {turn: synced["checksum"] for turn, synced in records_before_first_load(record) if "checksum" in synced}


def synced_records(record):
    """The synced part of each turn's record up to the first load: the hash of each component, the seed and the draws"""
    return dict(records_before_first_load(record))


def compare_parts(server, game, turns, ignored):
    """The turns at which a part other than the ignored ones, the seed or the draws differ, and the parts that differ"""
    different = []
    parts = {}
    for turn in sorted(server):
        if turn > turns or turn not in game:
            continue
        ours, theirs = game[turn], server[turn]
        names = set(ours.get("parts", {})) | set(theirs.get("parts", {}))
        # A field's part is named after its component and a full stop
        differ = [name for name in names
                  if name.split(".")[0] not in ignored and ours.get("parts", {}).get(name) != theirs.get("parts", {}).get(name)]
        differ += [key for key in ("seed", "draws") if ours.get(key) != theirs.get(key)]
        if differ:
            different.append(turn)
            for name in differ:
                parts.setdefault(name, turn)
    return different, parts


def turns_gone_back(path):
    """The places in a commands file where the turn goes back, as [turn before, turn after]: the game loaded a saved world
    there. The server plays on with no load, so the turns after it cannot be compared."""
    if not os.path.exists(path):
        return []
    turns = []
    with open(path, encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            if line:
                turns.append(int(json.loads(line).get("turn", 0)))
    return [[before, after] for before, after in zip(turns, turns[1:]) if after <= before]


def count_lines(path):
    """The number of lines of a file, 0 when there is no file"""
    if not os.path.exists(path):
        return 0
    with open(path, encoding="utf-8") as file:
        return sum(1 for line in file if line.strip())


def compare(server, game, turns):
    """The turns of the server whose checksum the game does not have or does not match, and the turns compared"""
    compared = [turn for turn in sorted(server) if turn <= turns]
    different = [turn for turn in compared if game.get(turn) != server[turn]]
    return different, compared


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--turns", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--land", default="Land1.txt")
    parser.add_argument("--gods-ring-map", action="store_true", help="the network map for 32 gods")
    parser.add_argument("--testbed", action="store_true", help="the creature testbed instead of a land")
    parser.add_argument("--scenario", help="a testbed scenario that the server and the game both play from the first "
                                           "frame, so that the recorded input gets to what it sets up")
    parser.add_argument("--opening", default="cow")
    parser.add_argument("--all-computer-gods", action="store_true",
                        help="the AI soak: the server and the game both give a computer god to every god with a citadel")
    parser.add_argument("--ai-seed", type=int, help="the seed of the personalities of the computer gods, on both")
    parser.add_argument("--frame-ms", type=float, default=100.0)
    parser.add_argument("--config", default="Debug")
    parser.add_argument("--bin", help="the folder of openblack.exe and openblack-server.exe")
    parser.add_argument("--game-path", default=DEFAULT_GAME_PATH)
    parser.add_argument("--wsl-server", help="the Linux openblack-server in WSL, such as /root/ob/bin/openblack-server")
    parser.add_argument("--wsl-distro", default="Ubuntu", help="with --wsl-server, the WSL distribution")
    parser.add_argument("--out", default=os.path.join(os.getcwd(), "server-check"))
    parser.add_argument("--server-args", default="", help="more arguments for the server, such as \"--reload-at 50\"")
    parser.add_argument("--run-limit", type=int, default=RUN_LIMIT_SECONDS,
                        help="the seconds that the server and the game each have to finish (a long run past turn 18000 needs more)")
    parser.add_argument("--conditions", help="the winning conditions of the network game on both, as the server's "
                        "--conditions and the game's --sim-conditions: [LAND:]NAME=VALUE,...")
    parser.add_argument("--server-fast", action="store_true", help="the server too plays with no wait")
    parser.add_argument("--hash-parts", action="store_true",
                        help="both runs record the hash of each component, to find what made them part")
    parser.add_argument("--hash-fields", action="store_true", help="as --hash-parts, and the hash of each field")
    parser.add_argument("--input", help="a JSON file of recorded input, [{frame, event}...], for the game's hand")
    parser.add_argument("--throw-lead-latency-ms", type=int, help="a fixed latency in ms for the reports of the hand, so that the lead of each throw is the same in each run")
    parser.add_argument("--no-relay", action="store_true",
                        help="the server plays first and gets no commands of the game (the hand reports part them)")
    parser.add_argument("--draw-every", type=int, help="the game draws one frame in this many (0 for none)")
    parser.add_argument("--game-frames", type=int,
                        help="the game stops after this many frames. A scenario that pauses the game never gets to its "
                             "last turn: the check then compares the turns that the game played")
    parser.add_argument("--game-args", default="",
                        help="more arguments of the game, divided by spaces, such as the fight autopilot and "
                             "--sim-record-input to record an input for --input")
    parser.add_argument("--ignore-parts", default="",
                        help="with --hash-parts, components (comma separated) left out of a second comparison of the "
                             "turns, for a difference that waits on a decision")
    options = parser.parse_args()

    # The programs run in their own working folders, so a relative folder is made absolute first
    folder = os.path.abspath(options.bin or default_bin(options.config))
    server_exe = os.path.join(folder, "openblack-server.exe" if os.name == "nt" else "openblack-server")
    game_exe = os.path.join(folder, "openblack.exe" if os.name == "nt" else "openblack")
    out = os.path.abspath(options.out)
    server_dir = os.path.join(out, "server")
    game_dir = os.path.join(out, "game")
    server_log = os.path.join(server_dir, "server.log")
    game_log = os.path.join(game_dir, "game.log")
    record = os.path.join(game_dir, "record.jsonl")
    for path in (server_log, game_log, record):
        if os.path.exists(path):
            os.remove(path)

    where = (["--gods-ring-map"] if options.gods_ring_map else ["--testbed"] if options.testbed
             else ["-s", options.land, "--skip-opening", options.opening])
    server_where = where
    if options.scenario:
        where = ["--scenario", options.scenario, "--scenario-hide-window"]
        server_where = ["--scenario", options.scenario]
    events = None
    if options.input:
        with open(options.input, encoding="utf-8") as file:
            events = json.load(file)
        if isinstance(events, dict):
            events = events.get("events", [])
    relay = not options.no_relay
    commands = os.path.join(game_dir, "commands.jsonl")
    # The game writes the commands of its testbed scenario beside its commands, and the server gives them with the game
    gives = os.path.join(game_dir, "commands.gives.jsonl")
    for path in (commands, gives):
        if os.path.exists(path):
            os.remove(path)

    # A Linux server in WSL takes WSL paths
    local = wsl_path if options.wsl_server else (lambda path: path)
    server_command = [server_exe, "-g", options.game_path] + server_where + [
                      "--seed", str(options.seed), "-n", str(options.turns), "--frame-ms", str(options.frame_ms),
                      "-l", local(server_log), "--log-checksums"]
    if options.wsl_server:
        server_command = ["wsl", "-d", options.wsl_distro, "--", options.wsl_server, "-g",
                          local(options.game_path)] + server_command[3:]
    server_command += options.server_args.split()
    if options.all_computer_gods:
        server_command.append("--all-computer-gods")
    if options.ai_seed is not None:
        server_command += ["--ai-seed", str(options.ai_seed)]
    # A land with no temple at the start (a story land, the testbed) would end the game at its first turn
    land_kind = {"land": options.land, "testbed": options.testbed, "gods_ring_map": options.gods_ring_map,
                 "scenario": options.scenario}
    server_command += sim_report.gods_need_no_temple_args(True, **land_kind)
    if options.server_fast:
        server_command.append("--fast")
    if options.conditions:
        server_command += ["--conditions", options.conditions]
    if options.hash_parts or options.hash_fields:
        server_command += ["--record", local(os.path.join(server_dir, "record.jsonl"))]
    if options.hash_fields:
        server_command.append("--record-fields")
    if relay:
        server_command += ["--commands", local(commands)]

    # The game stops as it gets to its last turn, before the record of that turn: it runs one turn more. The server's
    # world is a network game, so the game plays as one too
    draw_every = options.draw_every if options.draw_every is not None else (1 if events is not None else 0)
    game_command = [game_exe, "-g", options.game_path, "--skip-logos", "--seed", str(options.seed),
                    "--sim-fast", "--sim-draw-every", str(draw_every), "--sim-frame-ms", str(options.frame_ms),
                    "--sim-until-turn", str(options.turns + 1), "--sim-record", record,
                    "--sim-record-every", "1", "--inspect-lock-input", "-n", str(options.game_frames or 4000000000), "--background",
                    "-l", game_log, "--sim-network-game"] + where
    if options.all_computer_gods:
        game_command.append("--sim-all-computer-gods")
    if options.ai_seed is not None:
        game_command += ["--sim-ai-seed", str(options.ai_seed)]
    game_command += sim_report.gods_need_no_temple_args(False, **land_kind)
    if options.conditions:
        game_command += ["--sim-conditions", options.conditions]
    if events is None:
        game_command.append("--sim-still-hand")
    else:
        game_command += ["--sim-input", os.path.abspath(options.input)]
    if relay:
        game_command += ["--sim-commands", commands]
    if options.throw_lead_latency_ms is not None:
        game_command += ["--throw-lead-latency-ms", str(options.throw_lead_latency_ms)]
    game_command += [argument for argument in options.game_args.split(" ") if argument]
    if options.hash_fields:
        game_command.append("--sim-hash-fields")
    elif options.hash_parts:
        game_command.append("--sim-hash-parts")

    def play_server():
        started = time.monotonic()
        # The output of wsl.exe goes to a file, which tells why a Linux server did not start
        code = run(server_command, server_dir, limit=options.run_limit,
                   output=os.path.join(server_dir, "wsl-output.txt") if options.wsl_server else None)
        if code != 0:
            print(f"the server ended with exit code {code}; see {server_log}")
        return code, time.monotonic() - started

    def play_game():
        code = run(game_command, game_dir, limit=options.run_limit)
        if code != 0:
            print(f"the game ended with exit code {code}; see {game_log}")
        return code

    server_seconds = 0.0
    if relay:
        # The server needs the commands of the game
        if play_game() != 0:
            return 1
        # A game that its frame limit stopped early (a paused scenario) played fewer turns: the server plays the same
        # turns, because a paused server also never gets to the last turn
        played = game_checksums(record) if options.game_frames else {}
        if played and max(played) < options.turns:
            server_command[server_command.index("-n") + 1] = str(max(played))
        code, server_seconds = play_server()
        if code != 0:
            return 1
    else:
        code, server_seconds = play_server()
        if code != 0 or play_game() != 0:
            return 1

    server = server_checksums(server_log)
    game = game_checksums(record)
    # A game that its frame limit stopped before its last turn is compared up to the last turn that it played
    compared_turns = options.turns
    ended_by_frames_at_turn = None
    if options.game_frames and game and max(game) < options.turns:
        ended_by_frames_at_turn = max(game)
        compared_turns = ended_by_frames_at_turn
    # A game that loaded a saved world is compared up to its last turn before the first load: the server never loads
    game_loads = turns_gone_back(commands) if relay else []
    compared_until_load_at_turn = None
    if game_loads and game and max(game) < compared_turns:
        compared_until_load_at_turn = max(game)
        compared_turns = compared_until_load_at_turn
    different, compared = compare(server, game, compared_turns)
    summary = {
        "turns": options.turns,
        "seed": options.seed,
        "server_seconds": round(server_seconds, 1),
        "server_turns": len(server),
        "command_turns": count_lines(commands) if relay else None,
        "game_turns": len(game),
        "compared": len(compared),
        "different": different[:20],
        # The last turn up to which every checksum is the same
        "same_until": (different[0] - 1) if different else (compared[-1] if compared else 0),
        "same": not different and len(compared) >= compared_turns,
        "ended_by_frames_at_turn": ended_by_frames_at_turn,
        "game_loads": game_loads,
        "compared_until_load_at_turn": compared_until_load_at_turn,
    }
    if game_loads:
        print(f"the game loaded a saved world (turn before and after): {game_loads}; the server does not, so the turns "
              f"up to {compared_turns} are compared")
    ignored = {name for name in options.ignore_parts.split(",") if name}
    if ignored and (options.hash_parts or options.hash_fields):
        others, parts = compare_parts(synced_records(os.path.join(server_dir, "record.jsonl")), synced_records(record),
                                      compared_turns, ignored)
        summary["ignored"] = sorted(ignored)
        summary["different_without_ignored"] = others[:20]
        summary["parts_without_ignored"] = dict(sorted(parts.items(), key=lambda item: item[1])[:20])
        summary["same_without_ignored"] = not others and len(compared) >= compared_turns
    # What the turns did with the hand: the game and the server do the same commands, so each must count the same deeds.
    # An input that expects deeds of its hand (its "expect") fails the run when the hand did fewer: the checksums are
    # then the same only because the hand did nothing.
    # Only the deeds up to the last turn that the checksums were compared at are counted: the game can play on a few turns
    # after the last one that the record keeps, and a deed in those turns is not a difference (the 32 god map shows it).
    last_compared = compared[-1] if compared else None
    game_deeds, server_deeds, deeds_same = sim_report.hand_deeds_of_both(game_log, server_log, last_compared)
    missing = sim_report.missing_deeds(game_deeds.get(0, {}), sim_report.expected_deeds(options.input))
    summary["hand_deeds"] = {str(player): counts for player, counts in sorted(server_deeds.items())}
    summary["hand_deeds_same"] = deeds_same
    summary["missing_deeds"] = missing
    # A land with no temple for the player's god (Land 1) decides the network game at its first turn: the
    # god is out, its creature goes, and the end box takes the mouse, so a recorded input does nothing there. Such a run
    # is not the same: an input of the hand needs a map with temples
    summary["decided_at_turn"] = sim_report.decided_at_turn_in_file(game_log)
    decided_reason = sim_report.hand_input_on_decided_game(events is not None, summary["decided_at_turn"])
    summary["hand_input_decided"] = decided_reason is not None
    if decided_reason is not None:
        summary["same"] = False
        print(decided_reason)
    if missing or not deeds_same:
        summary["same"] = False
        print(f"the hand did not do what its input expects: {missing}" if missing
              else f"the game and the server count other deeds of the hand: game {game_deeds}, server {server_deeds}")
    # The winning conditions: each god gets them, completes them and wins at the same turns on the game and the server
    if options.conditions:
        game_events = sim_report.condition_events_in_file(game_log, last_compared)
        server_events = sim_report.condition_events_in_file(server_log, last_compared)
        summary["conditions"] = sim_report.condition_summary(server_events)
        summary["condition_events"] = len(server_events)
        summary["condition_events_same"] = game_events == server_events
        if game_events != server_events or not server_events:
            summary["same"] = False
            print(f"the game and the server tell other events of the winning conditions: game {game_events[:10]}, "
                  f"server {server_events[:10]}")
    # A component with no registration is left out of the checksum: a change in it would never show here
    unregistered = sim_report.unregistered_in_files(server_log, game_log, record,
                                                    os.path.join(server_dir, "record.jsonl"))
    summary["unregistered"] = sorted(unregistered)
    if unregistered:
        summary["same"] = False
        print(sim_report.unregistered_line(unregistered))
    with open(os.path.join(out, "server_check.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=2)
    print(json.dumps(summary))
    return 0 if summary["same"] else 1


if __name__ == "__main__":
    sys.exit(main())
