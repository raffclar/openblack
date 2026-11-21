#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The memory soak: plays many skirmish games in one game process, or one long run of a land, and samples the memory.

Each sample (every --every seconds of wall time, and at the end of each game) has the working set, the private bytes,
the handles and the threads of the process, and, for a game with the inspector, the game turn, the count of each kind
of component, the sizes of the resource caches and the graphics memory of the last --frame-stats report. The samples
go to <out>/samples.jsonl, one JSON object a line. mem_soak_report.py makes the tables, the slopes and the graphs.

    python tools/soak/mem_soak.py skirmish --exe <bin>/openblack.exe --games 30 --out <folder>
    python tools/soak/mem_soak.py land --exe <bin>/openblack.exe --land Land1.txt --game-hours 2 --out <folder>
    python tools/soak/mem_soak.py server --exe <bin>/openblack-server.exe --gods-ring-map --game-hours 2 --out <folder>

skirmish: the game starts on the first land, then for each game opens the menu, starts a skirmish from the skirmish box
(Start Skirmish Game, or Leave Skirmish Game after the first), presses Go! in the land information box and lets the
computer gods play every god at the fastest speed until a god wins or --game-minutes of game time go by. Then the next
game starts from the menu, through the skirmish box. land: one land plays at the fastest speed for --game-hours of game
time. server: the dedicated server plays its land with --fast for --game-hours of game time, sampled from outside only.

Every game runs with --background and the frame limit -n. The tool checks the memory of the computer before it starts a
game and waits while 85 % or more is in use. It stops only the process that it started, by its process id.
"""

import argparse
import json
import os
import re
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import psutil

TURNS_A_SECOND = 10
MEMORY_LIMIT_PERCENT = 85.0
# The --frame-stats lines of the log that give the graphics memory and the buffers
FRAME_MEMORY = re.compile(r"Frame stats #(\d+) memory: process (\d+) MB(?: \((\d+) MB its own\))?; textures (-?[\d.]+) MB, "
                          r"render targets (-?[\d.]+) MB, graphics card (-?[\d.]+) MB of (-?[\d.]+) MB")
FRAME_BUFFERS = re.compile(r"Frame stats #(\d+) buffers: vertex (\d+) of \d+, index (\d+) of \d+, shared vertex (\d+) of "
                           r"\d+, shared index (\d+) of \d+")


class GameError(Exception):
    pass


def discovery_folder():
    """Where a running game keeps the file with its inspector port"""
    return Path(tempfile.gettempdir()) / "openblack-inspector"


class Inspector:
    """One connection to the inspector of the game of one process: one JSON request a line, one answer a line"""

    def __init__(self, pid, timeout=120.0):
        self.pid = pid
        self.timeout = timeout
        self.socket = None
        self.buffer = b""
        self.next_id = 1

    def port(self):
        path = discovery_folder() / f"{self.pid}.json"
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None
        port = record.get("port")
        return port if isinstance(port, int) and port > 0 else None

    def close(self):
        if self.socket is not None:
            try:
                self.socket.close()
            except OSError:
                pass
        self.socket = None
        self.buffer = b""

    def _open(self):
        port = self.port()
        if port is None:
            raise OSError("the game has no inspector port yet")
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=10.0)
        self.buffer = b""

    def call(self, query, params=None, timeout=None):
        """The answer of the game to the query: a dict with ok and result or error"""
        # The list answers come in pages of 20 by default: one page of the most items
        request = {"id": self.next_id, "query": query, "limit": 500}
        self.next_id += 1
        if params:
            request["params"] = params
        line = (json.dumps(request) + "\n").encode("utf-8")
        for attempt in range(2):
            try:
                if self.socket is None:
                    self._open()
                self.socket.sendall(line)
                return self._wait(request["id"], timeout or self.timeout)
            except OSError as error:
                self.close()
                if attempt == 1:
                    return {"ok": False, "error": str(error)}
        return {"ok": False, "error": "no answer"}

    def _wait(self, wanted, timeout):
        deadline = time.monotonic() + timeout
        while True:
            end = self.buffer.find(b"\n")
            if end >= 0:
                line, self.buffer = self.buffer[:end], self.buffer[end + 1:]
                try:
                    answer = json.loads(line.decode("utf-8", errors="replace"))
                except ValueError:
                    continue
                if isinstance(answer, dict) and answer.get("id") == wanted:
                    return answer
                continue
            left = deadline - time.monotonic()
            if left <= 0:
                raise OSError("the game did not answer in time")
            self.socket.settimeout(left)
            chunk = self.socket.recv(1 << 20)
            if not chunk:
                raise OSError("the game closed the connection")
            self.buffer += chunk

    def result(self, query, params=None, timeout=None, loading_wait=900.0):
        """The result of the query; while the game loads a land, the query is asked again"""
        deadline = time.monotonic() + loading_wait
        while True:
            answer = self.call(query, params, timeout)
            if answer.get("ok", False):
                return answer.get("result")
            if "loading" not in str(answer.get("error")) or time.monotonic() >= deadline:
                raise GameError(f"{query}: {answer.get('error')}")
            time.sleep(2)


def memory_in_use_percent():
    return psutil.virtual_memory().percent


def wait_for_memory(limit=MEMORY_LIMIT_PERCENT, log=print):
    """Waits while the memory of the computer is at the limit or more"""
    told = False
    while memory_in_use_percent() >= limit:
        if not told:
            log(f"memory in use {memory_in_use_percent():.0f} %: waiting until it is under {limit:.0f} %")
            told = True
        time.sleep(30)


def process_sample(process):
    """The memory of the process from outside, in megabytes"""
    with process.oneshot():
        info = process.memory_full_info() if sys.platform != "win32" else process.memory_info()
        sample = {
            "working_set_mb": info.rss / 1048576.0,
            "private_mb": getattr(info, "private", getattr(info, "uss", 0)) / 1048576.0,
            "threads": process.num_threads(),
        }
        if sys.platform == "win32":
            sample["handles"] = process.num_handles()
            sample["peak_working_set_mb"] = info.peak_wset / 1048576.0
            sample["pagefile_mb"] = info.pagefile / 1048576.0
    return sample


class LogTail:
    """Reads the new lines of the game's log for the graphics memory of the last --frame-stats report"""

    def __init__(self, path):
        self.path = Path(path)
        self.offset = 0
        self.graphics = {}
        self.lines = []

    def poll(self, keep=None):
        try:
            with open(self.path, "rb") as file:
                file.seek(self.offset)
                data = file.read()
        except OSError:
            return []
        end = data.rfind(b"\n")
        if end < 0:
            return []
        self.offset += end + 1
        found = []
        for raw in data[:end].split(b"\n"):
            line = raw.decode("utf-8", errors="replace")
            if (match := FRAME_MEMORY.search(line)) is not None:
                self.graphics.update({
                    "textures_mb": float(match.group(4)),
                    "render_targets_mb": float(match.group(5)),
                    "gpu_mb": float(match.group(6)),
                })
            elif (match := FRAME_BUFFERS.search(line)) is not None:
                self.graphics.update({
                    "vertex_buffers": int(match.group(2)),
                    "index_buffers": int(match.group(3)),
                    "dynamic_vertex_buffers": int(match.group(4)),
                    "dynamic_index_buffers": int(match.group(5)),
                })
            if keep is not None and keep.search(line):
                found.append(line)
        return found


def items_of(result):
    """The items of a list answer, which comes as {"items": [...]} or as a plain list"""
    items = result.get("items", []) if isinstance(result, dict) else (result or [])
    return [item for item in items if isinstance(item, dict)]


def game_sample(inspector):
    """What the game says of itself: its turn, its entities and components, and its caches"""
    sample = {}
    state = inspector.call("game.state", timeout=60)
    if state.get("ok"):
        result = state.get("result") or {}
        sample["turn"] = result.get("turn")
        sample["paused"] = result.get("paused")
    components = inspector.call("ecs.components", timeout=60)
    if components.get("ok"):
        items = items_of(components.get("result"))
        sample["components"] = {item["name"]: item["count"] for item in items if "name" in item}
        sample["component_rows"] = sum(item.get("count", 0) for item in items)
    resources = inspector.call("engine.resources", timeout=60)
    if resources.get("ok"):
        sample["resources"] = resources.get("result")
    drawn = inspector.call("drawn.components", timeout=60)
    if drawn.get("ok"):
        items = items_of(drawn.get("result"))
        sample["drawn_rows"] = sum(item.get("count", 0) for item in items)
    return sample


class Run:
    """One process of the soak with its samples"""

    def __init__(self, args, command, out):
        self.args = args
        self.out = Path(out)
        self.out.mkdir(parents=True, exist_ok=True)
        self.command = command
        self.samples = open(self.out / "samples.jsonl", "a", encoding="utf-8")
        self.events = open(self.out / "events.log", "a", encoding="utf-8")
        self.log_path = self.out / "game.log"
        self.process = None
        self.ps = None
        self.inspector = None
        self.tail = LogTail(self.log_path)
        self.started = time.monotonic()
        self.game = 0

    def log(self, text):
        line = f"{time.strftime('%Y-%m-%d %H:%M:%S')} {text}"
        print(line, flush=True)
        self.events.write(line + "\n")
        self.events.flush()

    def start(self, cwd):
        wait_for_memory(log=self.log)
        flags = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
        self.log("start: " + " ".join(self.command))
        with open(self.out / "stdout.log", "ab") as stdout:
            self.process = subprocess.Popen(self.command, cwd=cwd, stdin=subprocess.DEVNULL, stdout=stdout,
                                            stderr=subprocess.STDOUT, creationflags=flags)
        self.ps = psutil.Process(self.process.pid)
        self.log(f"pid {self.process.pid}")
        (self.out / "pid.txt").write_text(str(self.process.pid))

    def alive(self):
        return self.process is not None and self.process.poll() is None

    def sample(self, kind, extra=None):
        if not self.alive():
            return None
        try:
            record = {"time": round(time.monotonic() - self.started, 1), "kind": kind, "game": self.game}
            record.update(process_sample(self.ps))
        except psutil.Error:
            return None
        self.tail.poll()
        record.update(self.tail.graphics)
        if self.inspector is not None:
            record.update(game_sample(self.inspector))
        if extra:
            record.update(extra)
        self.samples.write(json.dumps(record) + "\n")
        self.samples.flush()
        return record

    def stop(self):
        """Stops the process that this run started, by its id: first as the game asks, then by force"""
        if self.inspector is not None:
            self.inspector.close()
        if self.process is None or self.process.poll() is not None:
            return
        self.log(f"stopping pid {self.process.pid}")
        try:
            self.process.terminate()
            self.process.wait(timeout=60)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=60)

    def wait_inspector(self, timeout=600):
        deadline = time.monotonic() + timeout
        self.inspector = Inspector(self.process.pid)
        while time.monotonic() < deadline:
            if not self.alive():
                raise GameError("the game stopped before its inspector answered")
            answer = self.inspector.call("game.state", timeout=10)
            if answer.get("ok") and (answer.get("result") or {}).get("ready", True):
                return answer.get("result")
            time.sleep(2)
        raise GameError("the inspector did not answer")


def game_command(args, extra):
    command = [str(Path(args.exe).resolve()), "-g", args.game_path, "--background", "-n", str(args.frames),
               "--inspect-port", "0", "--profile-folder", str(Path(args.out).resolve() / "profile"),
               "--frame-stats", str(args.frame_stats), "-l", str(Path(args.out).resolve() / "game.log"),
               "--skip-logos", "--seed", str(args.seed)]
    if args.backend:
        command += ["-b", args.backend]
    if args.no_draw:
        command.append("--no-draw")
    return command + extra


def windows(inspector):
    return {item.get("name"): item for item in items_of(inspector.result("gui.windows"))}


def wait_window(run, name, open_wanted=True, timeout=300):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if not run.alive():
            raise GameError(f"the game stopped while waiting for {name}")
        window = windows(run.inspector).get(name)
        if window is not None and bool(window.get("open")) == open_wanted:
            return window
        time.sleep(1)
    raise GameError(f"the window {name} did not {'open' if open_wanted else 'close'} in {timeout} s")


def press(run, window, button):
    answer = run.inspector.call("gui.press", {"window": window, "button": button})
    run.log(f"press {window} / {button}: {'ok' if answer.get('ok') else answer.get('error')}")
    return answer.get("ok", False)


def game_end_state(inspector):
    answer = inspector.call("players.game_end", timeout=60)
    return answer.get("result") if answer.get("ok") else None


def open_menu(run):
    """Opens the game's menu as Escape does; tries again when a key did not open it"""
    for _ in range(30):
        menu = windows(run.inspector).get("menu") or {}
        if menu.get("open"):
            return menu
        run.inspector.call("gui.open", {"window": "menu"})
        time.sleep(3)
    raise GameError(f"the menu did not open: {json.dumps(windows(run.inspector))[:800]}")


def start_skirmish(run, land):
    """Opens the menu, ends the game through the skirmish button and starts the land from the skirmish box"""
    inspector = run.inspector
    menu = open_menu(run)
    buttons = menu.get("buttons") or []
    button = "Leave Skirmish Game" if "Leave Skirmish Game" in buttons else "Start Skirmish Game"
    if not press(run, "menu", button):
        raise GameError(f"no skirmish button in the menu: {json.dumps(menu)[:400]}")
    # Leave Skirmish Game asks a question first
    if button == "Leave Skirmish Game":
        time.sleep(2)
        if not press(run, "menu", "Yes."):
            raise GameError(f"no question after Leave Skirmish Game: {json.dumps(windows(inspector).get('menu'))[:400]}")
    wait_window(run, "skirmish_box")
    time.sleep(1)
    if not press(run, "skirmish_box", land):
        raise GameError(f"no land {land} in the skirmish box: {json.dumps(windows(inspector).get('skirmish_box'))[:600]}")
    time.sleep(1)
    if not press(run, "skirmish_box", "Start Game"):
        raise GameError("no Start Game in the skirmish box")
    wait_window(run, "skirmish_box", open_wanted=False)
    wait_window(run, "land_info_box", timeout=900)
    time.sleep(1)
    press(run, "land_info_box", "Go!")
    wait_window(run, "land_info_box", open_wanted=False)


def play_until(run, turns, every, every_turns, check_end=True):
    """Lets the game run until it gets to the turn, a god wins, or the game stops. Samples each time every seconds of
    wall time or every_turns game turns go by, which comes first."""
    state = run.inspector.result("game.state")
    start = state.get("turn") or 0
    next_sample = time.monotonic() + every
    next_turn = start + every_turns
    while run.alive():
        time.sleep(1)
        state = run.inspector.call("game.state", timeout=60)
        if not state.get("ok"):
            continue
        turn = (state.get("result") or {}).get("turn") or 0
        if time.monotonic() >= next_sample or turn >= next_turn:
            run.sample("minute", {"game_turns": turn - start})
            next_sample = time.monotonic() + every
            next_turn = turn + every_turns
        if turn - start >= turns:
            return "time", turn - start
        if check_end:
            end = game_end_state(run.inspector)
            if isinstance(end, dict) and end.get("decided"):
                return "won", turn - start
    return "stopped", 0


def skirmish(args):
    out = Path(args.out)
    # The save on exit at the end of each game waits for a picture of a drawn frame, so every frame draws
    extra = ["-s", args.first_land, "--skip-opening", "cow", "--sim-all-computer-gods", "--sim-fast",
             "--sim-frame-ms", str(args.frame_ms), "--sim-draw-every", str(args.draw_every or 1)]
    if args.ai_seed is not None:
        extra += ["--sim-ai-seed", str(args.ai_seed)]
    run = Run(args, game_command(args, extra), out)
    try:
        run.start(cwd=Path(args.exe).resolve().parent)
        run.wait_inspector()
        run.sample("start")
        for game in range(1, args.games + 1):
            run.game = game
            begin = time.monotonic()
            start_skirmish(run, args.land)
            run.sample("game_start")
            how, turns = play_until(run, int(args.game_minutes * 60 * TURNS_A_SECOND), args.every, args.every_turns)
            run.sample("game_end", {"end": how, "game_turns": turns})
            run.log(f"game {game}: {how} after {turns} turns, {time.monotonic() - begin:.0f} s")
            if how == "stopped":
                return 1
        return 0
    except GameError as error:
        run.log(f"FAILED: {error}")
        return 1
    finally:
        run.stop()


def land(args):
    out = Path(args.out)
    extra = (["--gods-ring-map"] if args.gods_ring_map else ["-s", args.land, "--skip-opening", "cow"]) + \
        ["--sim-fast", "--sim-frame-ms", str(args.frame_ms)] + args.game_args
    if args.draw_every:
        extra += ["--sim-draw-every", str(args.draw_every)]
    run = Run(args, game_command(args, extra), out)
    try:
        run.start(cwd=Path(args.exe).resolve().parent)
        run.wait_inspector()
        run.sample("start")
        how, turns = play_until(run, int(args.game_hours * 3600 * TURNS_A_SECOND), args.every, args.every_turns,
                                check_end=False)
        run.sample("end", {"end": how, "game_turns": turns})
        run.log(f"land: {how} after {turns} turns")
        return 0 if how != "stopped" else 1
    except GameError as error:
        run.log(f"FAILED: {error}")
        return 1
    finally:
        run.stop()


def server(args):
    """The dedicated server plays its land alone at full speed, with its inspector for the counts"""
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    turns = int(args.game_hours * 3600 * TURNS_A_SECOND)
    command = [str(Path(args.exe).resolve()), "-g", args.game_path, "--fast", "--seed", str(args.seed),
               "-n", str(turns), "--frame-ms", "100", "-l", str(out.resolve() / "game.log"), "--snapshot-every", "0",
               "--profile-folder", str(out.resolve() / "profile"), "--inspect-port", "0"]
    command += ["--gods-ring-map", "--all-computer-gods"] if args.gods_ring_map else ["-s", args.land]
    if args.gods_ring_map and args.ai_seed is not None:
        command += ["--ai-seed", str(args.ai_seed)]
    command += args.game_args
    run = Run(args, command, out)
    try:
        run.start(cwd=Path(args.exe).resolve().parent)
        try:
            run.wait_inspector(timeout=300)
        except GameError as error:
            run.log(f"no inspector on the server ({error}); samples from outside only")
            run.inspector = None
        next_sample = time.monotonic()
        next_turn = 0
        dumps = sorted(int(t) for t in args.dump_turns.split(",")) if args.dump_turns else []
        while run.alive():
            turn = None
            if run.inspector is not None:
                state = run.inspector.call("game.state", timeout=30)
                turn = (state.get("result") or {}).get("turn") if state.get("ok") else None
            if time.monotonic() >= next_sample or (turn is not None and turn >= next_turn):
                run.sample("minute")
                next_sample = time.monotonic() + args.every
                next_turn = (turn or 0) + args.every_turns
            if dumps and turn is not None and turn >= dumps[0]:
                run.log(f"heap dump at turn {turn}")
                heap_dump(run)
                dumps.pop(0)
                if not dumps and args.stop_after_dumps:
                    run.log("the last heap dump is written; stopping the server")
                    break
            time.sleep(1)
        run.log(f"server ended with code {run.process.returncode}")
        return 0 if run.process.returncode == 0 else 1
    finally:
        run.stop()


def quit_game(run, timeout=600):
    """Ends the game as the player does, with Quit Black & White in the menu, and waits for the process to end, so that
    the program runs its own clean-up (a leak check reads the memory after it)"""
    open_menu(run)
    press(run, "menu", "Quit Black & White")
    time.sleep(2)
    menu = windows(run.inspector).get("menu") or {}
    if menu.get("open") and menu.get("page") == "question":
        press(run, "menu", "Yes.")
    try:
        run.process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        raise GameError("the game did not end after Quit")
    run.log(f"the game ended with code {run.process.returncode}")


def play_turns(run, turns, every_turns):
    run.sample("step")
    how, played = play_until(run, turns, 1e9, every_turns, check_end=False)
    if how == "stopped":
        raise GameError("the game stopped")


def heap_dump(run, timeout=1800):
    """Asks a game built with the leak tracer of the lane mem-leak-soak-1 (Windows Debug only) to write its live heap
    blocks, grouped by stack, and waits until it wrote them"""
    import ctypes
    kernel = ctypes.windll.kernel32
    kernel.OpenEventW.restype = ctypes.c_void_p
    synchronize_and_modify = 0x00100000 | 0x0002
    dump = kernel.OpenEventW(synchronize_and_modify, False, f"Local\\leaktrace-dump-{run.process.pid}")
    done = kernel.OpenEventW(synchronize_and_modify, False, f"Local\\leaktrace-done-{run.process.pid}")
    if not dump or not done:
        raise GameError("the game has no leak tracer")
    try:
        kernel.SetEvent(ctypes.c_void_p(dump))
        if kernel.WaitForSingleObject(ctypes.c_void_p(done), int(timeout * 1000)) != 0:
            raise GameError("the heap dump did not end in time")
    finally:
        kernel.CloseHandle(ctypes.c_void_p(dump))
        kernel.CloseHandle(ctypes.c_void_p(done))


def steps(args):
    """Plays the steps in order: skirmish (start one from the menu), turns:N, level:NAME (level.load), levels (logs level.list), save:SLOT,
    load:SLOT, dump (the live heap of a leak trace build), quit (Quit Black & White, then wait for the process to end)"""
    out = Path(args.out)
    extra = ["-s", args.first_land, "--skip-opening", "cow", "--sim-all-computer-gods", "--sim-fast",
             "--sim-frame-ms", str(args.frame_ms), "--sim-draw-every", str(args.draw_every or 1)] + args.game_args
    run = Run(args, game_command(args, extra), out)
    try:
        run.start(cwd=Path(args.exe).resolve().parent)
        run.wait_inspector(timeout=1800)
        run.sample("start")
        for step in args.steps:
            name, _, value = step.partition(":")
            run.log(f"step {step}")
            if name == "skirmish":
                run.game += 1
                start_skirmish(run, args.land)
            elif name == "turns":
                play_turns(run, int(value), args.every_turns)
            elif name == "level":
                run.inspector.result("level.load", {"name": value}, timeout=1800)
                run.wait_inspector(timeout=1800)
            elif name == "save":
                run.log(f"save: {json.dumps(run.inspector.result('saves.save', {'slot': int(value)}, timeout=600))}")
            elif name == "load":
                run.log(f"load: {json.dumps(run.inspector.result('saves.load', {'slot': int(value)}, timeout=1800))}")
                run.wait_inspector(timeout=1800)
            elif name == "levels":
                run.log(f"levels: {json.dumps(run.inspector.result('level.list', {}))}")
            elif name == "dump":
                heap_dump(run)
            elif name == "quit":
                quit_game(run)
                return 0
            else:
                raise GameError(f"no step {name}")
            run.sample("step", {"step": step})
        return 0
    except GameError as error:
        run.log(f"FAILED: {error}")
        return 1
    finally:
        run.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="mode", required=True)
    for name in ("skirmish", "land", "server", "steps"):
        p = sub.add_parser(name)
        p.add_argument("--exe", required=True)
        p.add_argument("--game-path", default=os.environ.get("OB_GAME_PATH", "C:/projects/black"))
        p.add_argument("--out", required=True)
        p.add_argument("--every", type=float, default=60.0, help="Seconds of wall time between two samples")
        p.add_argument("--every-turns", type=int, default=600, help="Game turns between two samples (600: a minute)")
        p.add_argument("--seed", type=int, default=1)
        p.add_argument("--backend", default=None)
        p.add_argument("--frames", type=int, default=50_000_000, help="The frame limit -n of the game")
        p.add_argument("--frame-stats", type=int, default=600)
        p.add_argument("--frame-ms", type=int, default=100, help="Game time of each frame of --sim-fast")
        p.add_argument("--draw-every", type=int, default=0)
        p.add_argument("--no-draw", action="store_true")
        p.add_argument("--land", default="Defeat another god to control this realm." if name == "skirmish" else "Land1.txt",
                       help="skirmish: the land as the skirmish box lists it (Two Gods is the default); land, server: the "
                            "land script")
        p.add_argument("--gods-ring-map", action="store_true")
        p.add_argument("--ai-seed", type=int, default=1)
        p.add_argument("--game-args", nargs=argparse.REMAINDER, default=[])
    sk = sub.choices["skirmish"]
    sk.add_argument("--games", type=int, default=30)
    sk.add_argument("--game-minutes", type=float, default=20.0)
    sk.add_argument("--first-land", default="Land1.txt")
    st = sub.choices["steps"]
    st.add_argument("--first-land", default="Land1.txt")
    st.add_argument("--steps", nargs="+", required=True)
    for name in ("land", "server"):
        sub.choices[name].add_argument("--game-hours", type=float, default=2.0)
    sub.choices["server"].add_argument("--dump-turns", help="Turns at which a server built with the leak tracer writes "
                                       "its live heap (see heap_dump), as 5000,15000")
    sub.choices["server"].add_argument("--stop-after-dumps", action="store_true")
    args = parser.parse_args()
    return {"skirmish": skirmish, "land": land, "server": server, "steps": steps}[args.mode](args)


if __name__ == "__main__":
    sys.exit(main())
