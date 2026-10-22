"""Checks that the dedicated server plays the same turns as the game.

The script runs apps/openblack-server for some turns at its own pace (10 turns a second), with a seed. Then it runs the
game in the fast simulation mode with the same seed, the same land, the same frame time and a still hand, and records
the checksum of each turn. Each turn of the server must have the same checksum as the same turn of the game.

    python tools/sim/server_check.py --turns 100
    python tools/sim/server_check.py --turns 100 --config RelWithDebInfo --out E:/runs/server

Both programs start with no console window of their own. The game starts with --background. The script stops only the
processes that it started, by their own process ids.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BUILDS = os.environ.get("OB_BUILDS", "E:/openblack/builds")
DEFAULT_GAME_PATH = os.environ.get("OB_GAME_PATH", "C:/projects/black")
CHECKSUM_LINE = re.compile(r"turn (\d+) checksum ([0-9a-f]{16})")
# A run that has not finished in this time is stopped
RUN_LIMIT_SECONDS = 900


def worktree_root():
    return os.path.normpath(os.path.join(HERE, "..", ".."))


def default_bin(config):
    return os.path.join(DEFAULT_BUILDS, os.path.basename(worktree_root()), "bin", config)


def run(command, cwd, limit=RUN_LIMIT_SECONDS):
    """Runs a program with no console window of its own, and stops it by its own process id after the limit"""
    os.makedirs(cwd, exist_ok=True)
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               creationflags=flags)
    try:
        return process.wait(timeout=limit)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        raise RuntimeError(f"{command[0]} (pid {process.pid}) did not finish in {limit} s")


def server_checksums(log):
    """The checksum of each turn in the log of the server"""
    turns = {}
    with open(log, encoding="utf-8", errors="replace") as file:
        for line in file:
            match = CHECKSUM_LINE.search(line)
            if match:
                turns[int(match.group(1))] = match.group(2)
    return turns


def game_checksums(record):
    """The checksum of each turn in the run record of the game"""
    turns = {}
    with open(record, encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
            value = json.loads(line)
            synced = value.get("synced")
            if isinstance(synced, dict) and "checksum" in synced and "turn" in value:
                turns[int(value["turn"])] = synced["checksum"]
    return turns


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
    parser.add_argument("--opening", default="cow")
    parser.add_argument("--frame-ms", type=float, default=100.0)
    parser.add_argument("--config", default="Debug")
    parser.add_argument("--bin", help="the folder of openblack.exe and openblack-server.exe")
    parser.add_argument("--game-path", default=DEFAULT_GAME_PATH)
    parser.add_argument("--out", default=os.path.join(os.getcwd(), "server-check"))
    parser.add_argument("--server-fast", action="store_true", help="the server too plays with no wait")
    options = parser.parse_args()

    folder = options.bin or default_bin(options.config)
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

    server_command = [server_exe, "-g", options.game_path, "-s", options.land, "--skip-opening", options.opening,
                      "--seed", str(options.seed), "-n", str(options.turns), "--frame-ms", str(options.frame_ms),
                      "--profile-folder", os.path.join(server_dir, "profile"), "-l", server_log]
    if options.server_fast:
        server_command.append("--fast")
    started = time.monotonic()
    code = run(server_command, server_dir)
    server_seconds = time.monotonic() - started
    if code != 0:
        print(f"the server ended with exit code {code}; see {server_log}")
        return 1

    game_command = [game_exe, "-g", options.game_path, "--skip-logos", "--seed", str(options.seed),
                    "--sim-fast", "--sim-draw-every", "0", "--sim-frame-ms", str(options.frame_ms),
                    "--sim-until-turn", str(options.turns), "--sim-still-hand", "--sim-record", record,
                    "--sim-record-every", "1", "--inspect-lock-input", "-n", "4000000000", "--background",
                    "-s", options.land, "--skip-opening", options.opening,
                    "--profile-folder", os.path.join(game_dir, "profile"), "-l", game_log]
    code = run(game_command, game_dir)
    if code != 0:
        print(f"the game ended with exit code {code}; see {game_log}")
        return 1

    server = server_checksums(server_log)
    game = game_checksums(record)
    different, compared = compare(server, game, options.turns)
    summary = {
        "turns": options.turns,
        "seed": options.seed,
        "server_seconds": round(server_seconds, 1),
        "server_turns": len(server),
        "game_turns": len(game),
        "compared": len(compared),
        "different": different[:20],
        "same": not different and len(compared) >= options.turns,
    }
    with open(os.path.join(out, "server_check.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=2)
    print(json.dumps(summary))
    return 0 if summary["same"] else 1


if __name__ == "__main__":
    sys.exit(main())
