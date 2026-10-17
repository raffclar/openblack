#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Loads lands and testbed scenarios again and again in one game, to find crashes that a land load causes.

The script starts the game with the renderer given and the inspector on, then loads each land and scenario of the list
in turn, as many times as the cycles say. After each load, it asks the game for its state. It stops at the first load
that fails or at the first crash report, and then stops the game that it started (by its process id only).

    python tools/inspector/land_load_stress.py --exe E:/openblack/builds/<worktree>/bin/Debug/openblack.exe \\
        --game-path C:/projects/black --backend Vulkan --cycles 10

Land names come from level.list and scenario ids from game.scenarios. A scenario is written as scenario:<id>.
"""

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

ADAPTER = Path(__file__).resolve().parent / "openblack_inspector_mcp.py"
DEFAULT_LOADS = ["Land1", "Two Gods", "scenario:idle.fidget", "Four Gods", "scenario:expressions.desire_emotes"]


def call(pid, query, params=None, wait_ms=180000):
    """Sends one query to the game of the process and gives its answer, or None if the game did not answer"""
    command = [sys.executable, str(ADAPTER), "--pid", str(pid), "--timeout", "60", "--wait-ms", str(wait_ms),
               "--call", query, json.dumps({"params": params or {}})]
    done = subprocess.run(command, capture_output=True, text=True, stdin=subprocess.DEVNULL, check=False)
    try:
        return json.loads(done.stdout)
    except ValueError:
        return {"ok": False, "error": (done.stderr or done.stdout).strip()}


def wait_for_inspector(pid, process, timeout):
    """Waits until the game of the process answers the inspector. Gives False if the game stopped or the time is up."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            return False
        answer = call(pid, "game.state", wait_ms=0)
        if answer.get("ok"):
            return True
        time.sleep(1.0)
    return False


def crash_reports(folder, since):
    """The crash reports in the folder written after the time"""
    if not folder.is_dir():
        return []
    return sorted(path for path in folder.glob("openblack-crash-*.txt") if path.stat().st_mtime >= since)


def load(pid, item):
    """Loads one land or scenario and gives the answer"""
    if item.startswith("scenario:"):
        return call(pid, "game.scenario", {"id": item.removeprefix("scenario:")})
    return call(pid, "level.load", {"name": item})


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--exe", required=True, help="The openblack executable")
    parser.add_argument("--game-path", required=True, help="The folder of the game's data")
    parser.add_argument("--backend", default="Vulkan", help="The renderer: Vulkan, D3D12 and so on")
    parser.add_argument("--cycles", type=int, default=5, help="How many times to load the whole list")
    parser.add_argument("--loads", nargs="+", default=DEFAULT_LOADS,
                        help="The lands and scenarios (scenario:<id>) to load in turn")
    parser.add_argument("--log", help="A file for the game's log (none by default)")
    parser.add_argument("--list", action="store_true", help="Print the lands and scenarios of the game, then stop")
    parser.add_argument("--start-timeout", type=float, default=300.0, help="Seconds to wait for the first land")
    args = parser.parse_args()

    exe = Path(args.exe).resolve()
    crashes = exe.parent / "crashes"
    started = time.time()
    command = [str(exe), "-g", args.game_path, "-b", args.backend, "--inspect-port", "0"]
    log = open(args.log, "w", encoding="utf-8") if args.log else subprocess.DEVNULL
    process = subprocess.Popen(command, cwd=exe.parent, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
    print(f"started pid {process.pid}: {' '.join(command)}", flush=True)
    failures = 0
    loads = 0
    try:
        if not wait_for_inspector(process.pid, process, args.start_timeout):
            print("the game did not answer the inspector", flush=True)
            return 1
        if args.list:
            print(json.dumps(call(process.pid, "level.list").get("result"), indent=1))
            print(json.dumps(call(process.pid, "game.scenarios").get("result"), indent=1))
            return 0
        for cycle in range(args.cycles):
            for item in args.loads:
                begin = time.monotonic()
                answer = load(process.pid, item)
                state = call(process.pid, "game.state")
                loads += 1
                seconds = time.monotonic() - begin
                reports = crash_reports(crashes, started)
                alive = process.poll() is None
                ok = answer.get("ok") and state.get("ok") and alive and not reports
                print(f"cycle {cycle + 1} load {loads} {item}: {'ok' if ok else 'FAILED'} in {seconds:.1f} s"
                      + ("" if ok else f" ({answer.get('error') or state.get('error') or 'the game stopped'})"),
                      flush=True)
                if reports:
                    print("crash report: " + ", ".join(str(path) for path in reports), flush=True)
                    return 1
                if not alive:
                    return 1
                if not ok:
                    failures += 1
        print(f"{loads} loads, {failures} failed, no crash", flush=True)
        return 0 if failures == 0 else 1
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
        if args.log:
            log.close()


if __name__ == "__main__":
    sys.exit(main())
