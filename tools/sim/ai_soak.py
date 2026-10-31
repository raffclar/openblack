"""The AI soak: long network games in which computer gods play every god, with the checksum of every turn compared on the
server and on the observers, so that any write in a frame step and any local read in a system shows as a part.

A matrix of games: lands x seeds x AI seeds x modes. Each game is one run of tools/sim/net_check.py. When a game parts
from the server's world, the runner plays the same game again with the row dump of the first different turn, the draw
trace, the hash of each field and the frame guard of the clients, and writes a repro line: the command that plays that
game again. The runs go to <out>/<date>/<game>/ (E:/openblack/sim-runs/ai-soak by default), one row for each game goes
to summary.jsonl there.

    python tools/sim/ai_soak.py --short
    python tools/sim/ai_soak.py --lands TwoGods,ring32 --seeds 1,2 --ai-seeds 0,1 --modes all-computer --turns 2000
    python tools/sim/ai_soak.py --lands TwoGods,mpm_2p_1 --turns 20000 --part-observer
    python tools/sim/ai_soak.py --lines <day>/summary.jsonl
    python tools/sim/ai_soak.py --self-test

The modes: all-computer (the room has no human god; every client is an observer) and all-but-one (client 0 is the human
god with a still hand, the other gods are computer gods, and the observers watch). An AI seed of 0 means the built-in
personalities of the gods. With --part-observer, the world of the last observer of each game parts on purpose at a
random turn of that game (the same turn for the same shuffle seed) and must come back in step from the server's world;
any other part of that observer still fails the game. The games need a build with the developer tools and the AI soak modes (--bin, or the build
folder of this tree).

The runner checks the processor, the memory and the disk before each game and waits until they allow a game. On the PC:
the processor under 75 %, at least 16 GB of memory free, at least 150 GB free on the drive of the output folder. On the
Mac build box, which other lanes also use: the processor under 75 %, at least 6 GB of memory free, at least 30 GB of disk
free, and at most 4 game programs (openblack and openblack-server, of all lanes) with the programs of the new game. The
options --cpu-limit, --free-memory-gb, --free-disk-gb and --most-game-programs change the limits. It stops only the
net_check processes that it started, by their process ids.

On the Mac the tools need Python with the modules cryptography and psutil (the lanes use the virtual environment
~/projects/openblack/cache/simvenv). tools/sim/desync_soak.sh runs a soak on the Mac or on the PC with one command.

Exit code: 0 when every game stayed in step, 1 when a game parted or failed, 2 for a wrong command line or a tree that
cannot play the modes.
"""

import argparse
import dataclasses
import datetime
import json
import os
import random
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

NET_CHECK = os.path.join(HERE, "net_check.py")
import sim_paths  # noqa: E402

DEFAULT_OUT = os.environ.get("OB_AI_SOAK_OUT", os.path.join(sim_paths.DEFAULT_RUNS, "ai-soak"))
DEFAULT_BUILDS = sim_paths.DEFAULT_BUILDS

# The limits that a game needs (the lead's rules for the shared computer)
CPU_LIMIT_PERCENT = 75.0
FREE_MEMORY_GB = 16.0
FREE_DISK_GB = 150.0
# The most turns of a game (net_check's limit)
MOST_TURNS = 100000
# The names of the game programs, which the limit of game programs counts
GAME_PROGRAMS = ("openblack", "openblack-server", "openblack.exe", "openblack-server.exe")


@dataclasses.dataclass(frozen=True)
class Limits:
    """What the computer must allow before a game starts. most_game_programs is the most game programs that may run with
    those of the new game (None: no limit)."""
    cpu_percent: float
    free_memory_gb: float
    free_disk_gb: float
    most_game_programs: object = None


# The PC: the lead's rules for the shared computer
PC_LIMITS = Limits(CPU_LIMIT_PERCENT, FREE_MEMORY_GB, FREE_DISK_GB)
# The Mac build box (12 cores, 24 GB): other lanes build there; at most 4 game programs at a time (the user, 2026-10-28)
MAC_LIMITS = Limits(75.0, 6.0, 30.0, 4)


def default_limits():
    return MAC_LIMITS if sim_paths.IS_MAC else PC_LIMITS
# How long the runner waits for the computer to be free before it gives up, in seconds
WAIT_LIMIT_SECONDS = 4 * 3600
POLL_SECONDS = 30.0

# The lands of the soak: the arguments of net_check that choose the land, and whether the land is ready for the soak.
# The map for 32 gods has 7 playable god slots until the player tables grow, so it plays with 7 computer gods.
LANDS = {
    "TwoGods": ["--land", "Playgrounds/TwoGods.txt"],
    "ThreeGods": ["--land", "Playgrounds/ThreeGods.txt"],
    "FourGods": ["--land", "Playgrounds/FourGods.txt"],
    "IslandWars": ["--land", "Playgrounds/Island Wars.txt"],
    "Firestorm": ["--land", "Playgrounds/Firestorm.txt"],
    "DeathComes": ["--land", "Playgrounds/Death Comes To Those That Wait.txt"],
    "mpm_2p_1": ["--land", "Online Maps/mpm_2p_1.map"],
    "mpm_3p_1": ["--land", "Online Maps/mpm_3p_1.map"],
    "mpm_4p_1": ["--land", "Online Maps/mpm_4p_1.map"],
    "ring32": ["--gods-ring-map"],
}
# The lands of a night's soak. "Death Comes To Those That Wait" joins after the script fix of the plan (package S2).
NIGHT_LANDS = ["TwoGods", "ThreeGods", "FourGods", "IslandWars", "Firestorm", "mpm_2p_1", "mpm_3p_1", "mpm_4p_1", "ring32"]
# The lands of the soak of every map: every land of the soak, one game each (desync_soak.sh maps)
MAP_LANDS = list(LANDS)
MODES = ("all-computer", "all-but-one")
# The most desktop observers (the plan: up to 4 desktop clients and one web client)
MOST_OBSERVERS = 4


@dataclasses.dataclass(frozen=True)
class Game:
    """One game of the matrix"""
    land: str
    seed: int
    ai_seed: int
    mode: str
    observers: int
    turns: int
    web_client: bool = False
    until_end: bool = False
    # The turn at which the world of the last observer parts on purpose and comes back (None: no part on purpose)
    part_observer_at: object = None
    # The time limit of the network game in minutes (None: the game's default of 30 minutes, 18000 turns)
    time_limit: object = None

    @property
    def name(self):
        ai = f"ai{self.ai_seed}" if self.ai_seed else "ai-none"
        web = "-web" if self.web_client else ""
        end = "-end" if self.until_end else ""
        part = "" if self.part_observer_at is None else f"-p{self.part_observer_at}"
        return f"{self.land}-s{self.seed}-{ai}-{self.mode}-o{self.observers}{web}-t{self.turns}{end}{part}"

    @property
    def part_client(self):
        """The client (from 1) whose world parts on purpose: the last desktop observer"""
        return (0 if self.mode == "all-computer" else 1) + self.observers


def part_turn_range(turns):
    """The first and the last turn at which an observer can part on purpose in a game of these turns: after the start of
    the game, and early enough that the observer comes back before the end"""
    first = max(1, min(100, turns // 4))
    return first, max(first, turns - max(200, turns // 10))


def random_part_turn(game, seed):
    """The turn of the part on purpose of a game: random, but the same for the same game and seed"""
    first, last = part_turn_range(game.turns)
    return random.Random(f"{seed}:{game.name}").randint(first, last)


def plan(lands, seeds, ai_seeds, modes, observers, turns, web_client=False, until_end=False):
    """The games of the matrix, in the order lands, seeds, AI seeds, modes"""
    games = []
    for land in lands:
        for seed in seeds:
            for ai_seed in ai_seeds:
                for mode in modes:
                    land_turns = turns[land] if isinstance(turns, dict) else turns
                    land_observers = observers[land] if isinstance(observers, dict) else observers
                    games.append(Game(land, seed, ai_seed, mode, land_observers, land_turns, web_client, until_end))
    return games


def short_plan():
    """The short soak of a merge-train car (about 10 minutes): TwoGods with computer gods only, seed 1 and AI seed 1, 2000
    turns, the server and 2 desktop observers; the map for 32 gods with computer gods only, 1200 turns, the server and 1
    desktop observer"""
    return [Game("TwoGods", 1, 1, "all-computer", 2, 2000), Game("ring32", 1, 1, "all-computer", 1, 1200)]


def net_check_arguments(game, out, bin_folder=None, config="RelWithDebInfo", extra=()):
    """The command line of net_check (without the program) that plays a game"""
    arguments = list(LANDS[game.land])
    arguments += ["--seed", str(game.seed), "--turns", str(game.turns), "--config", config, "--out", out, "--hash-parts"]
    if bin_folder:
        arguments += ["--bin", bin_folder]
    if game.mode == "all-computer":
        arguments += ["--all-computer-gods", "--observers", str(game.observers)]
    else:
        arguments += ["--clients", "1", "--observers", str(game.observers)]
    if game.ai_seed:
        arguments += ["--ai-seed", str(game.ai_seed)]
    if game.web_client:
        arguments.append("--web-client")
    if game.until_end:
        arguments.append("--until-end")
    if game.part_observer_at is not None:
        arguments += ["--part-client", str(game.part_client), "--part-second-at", str(game.part_observer_at)]
    if game.time_limit is not None:
        arguments += ["--time-limit", str(game.time_limit)]
    arguments += list(extra)
    return arguments


def replay_extra(first_turn):
    """What a game that parted adds when it plays again: the rows of the first different turn, the draw trace, the hash of
    each field and the frame guard of the clients"""
    return ["--row-dump", "--row-dump-turn", str(first_turn), "--trace-draws", "--hash-fields", "--frame-guard"]


def repro_line(game, bin_folder=None, config="RelWithDebInfo", inject=None):
    """The command line that plays a game again, for a desync lane"""
    parts = ["python", "tools/sim/ai_soak.py", "--lands", game.land, "--seeds", str(game.seed),
             "--ai-seeds", str(game.ai_seed), "--modes", game.mode, "--observers", str(game.observers), "--turns",
             str(game.turns), "--config", config]
    if bin_folder:
        parts += ["--bin", bin_folder]
    if game.web_client:
        parts.append("--web-client")
    if game.until_end:
        parts.append("--until-end")
    if inject is not None:
        parts += ["--inject-part", str(inject)]
    if game.part_observer_at is not None:
        parts += ["--part-observer-at", str(game.part_observer_at)]
    if game.time_limit is not None:
        parts += ["--time-limit", str(game.time_limit)]
    return " ".join(f'"{part}"' if " " in part else part for part in parts)


def first_part(summary):
    """The first turn at which a client of a net_check summary differs from the server, and the client, or None"""
    found = None
    for client in summary.get("clients", []):
        for turn in client.get("different", []):
            if turn >= 0 and (found is None or turn < found[0]):
                found = (turn, client.get("client"))
    return found


def part_names(summary):
    """The parts that differ at the first different turn (the first client that has them)"""
    for client in summary.get("clients", []):
        if client.get("parts_against_server"):
            return client["parts_against_server"]
    return []


def ai_counts(summary):
    """The total of what the computer gods did, from the ai block of a summary"""
    ai = summary.get("ai") or {}
    total = {"gods": len(ai.get("gods", [])), "needs": 0, "hand": 0, "casts": 0}
    for god in (ai.get("per_god") or {}).values():
        total["needs"] += sum(god.get("needs", {}).values())
        total["hand"] += sum(god.get("hand", {}).values())
        total["casts"] += sum(god.get("casts", {}).values())
    return total


def max_turn_ms(summary):
    """The longest turn of each client, in milliseconds, as the clients' records tell, without the record's own time"""
    return [client.get("max_turn_ms") for client in summary.get("clients", [])]


def max_record_ms(summary):
    """The longest time of the record step of each client, in milliseconds"""
    return [client.get("max_record_ms") for client in summary.get("clients", [])]


def classify(code, summary):
    """same: every client in step; part: a client differs from the server; error: the run gave no usable result"""
    if summary is None:
        return "error"
    if first_part(summary) is not None:
        return "part"
    if code == 0 and summary.get("same"):
        return "same"
    return "error"


def summary_row(game, status, summary, started, started_clock, finished_clock, repro=None, replay=None):
    """The row of summary.jsonl for a game"""
    row = {"game": game.name, "land": game.land, "seed": game.seed, "ai_seed": game.ai_seed or None, "mode": game.mode,
           "observers": game.observers, "web_client": game.web_client, "turns": game.turns, "status": status,
           "started": started, "seconds": round(finished_clock - started_clock, 1)}
    if summary is not None:
        part = first_part(summary)
        row["turns_played"] = summary.get("server_turns")
        row["first_part_turn"] = part[0] if part else None
        row["parts"] = part_names(summary)
        row["ai"] = ai_counts(summary)
        row["max_turn_ms"] = max_turn_ms(summary)
        row["max_record_ms"] = max_record_ms(summary)
        row["ended"] = summary.get("ended")
    if game.part_observer_at is not None:
        row["part_observer_at"] = game.part_observer_at
    if repro:
        row["repro"] = repro
    if replay:
        row["replay"] = replay
    return row


class Guard:
    """Waits until the computer allows a game: processor, memory, disk and, where there is a limit, the count of game
    programs. The probes are functions, so that tests give their own."""

    def __init__(self, out, probes=None, sleep=time.sleep, clock=time.monotonic, wait_limit=WAIT_LIMIT_SECONDS,
                 poll=POLL_SECONDS, limits=None):
        self.out = out
        self.probes = probes or default_probes(out)
        self.sleep = sleep
        self.clock = clock
        self.wait_limit = wait_limit
        self.poll = poll
        self.limits = limits or default_limits()

    def blockers(self, programs=0):
        """The reasons why no game with this many programs may start now; empty when the game may start"""
        limits = self.limits
        cpu, memory, disk = self.probes["cpu"](), self.probes["memory_gb"](), self.probes["disk_gb"]()
        reasons = []
        if cpu >= limits.cpu_percent:
            reasons.append(f"processor {cpu:.0f} % (limit {limits.cpu_percent:.0f} %)")
        if memory < limits.free_memory_gb:
            reasons.append(f"free memory {memory:.1f} GB (needs {limits.free_memory_gb:.0f} GB)")
        if disk < limits.free_disk_gb:
            reasons.append(f"free disk {disk:.0f} GB (needs {limits.free_disk_gb:.0f} GB)")
        if limits.most_game_programs is not None and "game_programs" in self.probes:
            running = self.probes["game_programs"]()
            if running + programs > limits.most_game_programs:
                reasons.append(f"game programs {running} running + {programs} new (limit {limits.most_game_programs})")
        return reasons

    def wait(self, programs=0):
        """Waits for the computer; raises TimeoutError after the wait limit"""
        end = self.clock() + self.wait_limit
        while True:
            reasons = self.blockers(programs)
            if not reasons:
                return
            if self.clock() >= end:
                raise TimeoutError("the computer was busy for too long: " + ", ".join(reasons))
            print("waiting for the computer: " + ", ".join(reasons), flush=True)
            self.sleep(self.poll)


def default_probes(out):
    import psutil
    folder = out
    while folder and not os.path.exists(folder):
        parent = os.path.dirname(folder)
        if parent == folder:
            break
        folder = parent
    return {
        # The mean of a few samples: one sample of the processor jumps
        "cpu": lambda: sum(psutil.cpu_percent(interval=1.0) for _ in range(3)) / 3.0,
        "memory_gb": lambda: psutil.virtual_memory().available / 2**30,
        "disk_gb": lambda: shutil.disk_usage(folder or ".").free / 2**30,
        "game_programs": lambda: count_game_programs(psutil),
    }


def count_game_programs(psutil):
    """The game programs (openblack and openblack-server) that run on the computer now, of every lane"""
    count = 0
    for process in psutil.process_iter(["name"]):
        try:
            name = (process.info.get("name") or "").lower()
        except (psutil.Error, AttributeError):
            continue
        count += is_game_program(name)
    return count


def is_game_program(name):
    """Whether a process name is a game program. macOS gives at most 15 letters of a name, so "openblack-server" shows as
    "openblack-serve" there."""
    name = name.lower()
    return name in GAME_PROGRAMS or name == "openblack-serve"


def game_programs(game, inject=None):
    """The programs that a game starts: the server, the human clients and the desktop observers"""
    clients = 0 if game.mode == "all-computer" else 1
    if inject is not None:
        clients = 2
    return 1 + clients + game.observers


def run_net_check(arguments, log_path, limit):
    """Runs net_check; gives its exit code. A run that takes longer than the limit is stopped by its process id."""
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    with open(log_path, "w", encoding="utf-8") as log:
        process = subprocess.Popen([sys.executable, NET_CHECK] + arguments, stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=flags)
        try:
            return process.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            return 124


def read_summary(folder):
    """net_check.json of a run folder, or None"""
    path = os.path.join(folder, "net_check.json")
    if not os.path.exists(path):
        return None
    try:
        with open(path, encoding="utf-8") as file:
            return json.load(file)
    except ValueError:
        return None


def game_limit_seconds(game):
    """The longest time of a game: its turns at 10 a second with the load of the land, and twice for the replay's slowness"""
    return int(game.turns * 0.1 * 2 + 1800)


def play(game, day_folder, options, runner=run_net_check, guard=None, inject=None):
    """Plays one game and, when it parts, plays it again with the tools that find the cause. Gives the summary row."""
    folder = os.path.join(day_folder, game.name)
    os.makedirs(folder, exist_ok=True)
    programs = game_programs(game, inject)
    if guard is not None:
        guard.wait(programs)
    started_clock = time.time()
    started = datetime.datetime.now().isoformat(timespec="seconds")
    extra = ["--web-page", options.web_page] if game.web_client and getattr(options, "web_page", None) else []
    if inject is not None:
        # The second client of two gods parts at the turn and stays apart
        extra += ["--clients", "2", "--part-second-at", str(inject), "--part-second-again"]
    arguments = net_check_arguments(game, os.path.join(folder, "run"), options.bin, options.config, extra)
    code = runner(arguments, os.path.join(folder, "run.log"), game_limit_seconds(game))
    summary = read_summary(os.path.join(folder, "run"))
    status = classify(code, summary)
    repro = None
    replay = None
    if status == "part":
        turn, client = first_part(summary)
        repro = repro_line(game, options.bin, options.config, inject)
        if guard is not None:
            guard.wait(programs)
        replay_folder = os.path.join(folder, "replay")
        replay_arguments = net_check_arguments(game, replay_folder, options.bin, options.config, extra + replay_extra(turn))
        runner(replay_arguments, os.path.join(folder, "replay.log"), game_limit_seconds(game))
        again = read_summary(replay_folder)
        replay = replay_report(again, turn)
        with open(os.path.join(folder, "repro.txt"), "w", encoding="utf-8") as file:
            file.write(repro + "\n")
            file.write(json.dumps({"first_part_turn": turn, "client": client, "parts": part_names(summary), "replay": replay},
                                  indent=2) + "\n")
    if not getattr(options, "keep_programs", False):
        remove_programs(folder)
    return summary_row(game, status, summary, started, started_clock, time.time(), repro, replay)


def remove_programs(folder):
    """Removes the copies of the programs that net_check made in the run folders of a game (the build has them), so that a
    long soak does not fill the disk"""
    for run in ("run", "replay"):
        shutil.rmtree(os.path.join(folder, run, "bin"), ignore_errors=True)


def replay_report(summary, first_turn):
    """What the replay of a game that parted tells: the first different turn again, the rows that differ and the first
    draw that differs with both callers"""
    if summary is None:
        return {"error": "the replay gave no result"}
    report = {"first_part_turn": (first_part(summary) or (None,))[0], "parts": part_names(summary)}
    for client in summary.get("clients", []):
        rows = client.get("rows_against_server")
        draws = client.get("draws_against_server")
        if rows and "rows" not in report:
            report["rows"] = rows
        if draws and "draws" not in report:
            report["draws"] = draws
    return report


def queue_line(path, row):
    """The line that a part adds to the queue handover, for the lead: the game, the first turn, the parts and the repro line"""
    line = (f"- AI soak part ({row['started'][:10]}): {row['game']} parts at turn {row['first_part_turn']}, "
            f"parts {', '.join(row['parts']) or 'none named'}; repro: `{row.get('repro')}`\n")
    with open(path, "a", encoding="utf-8") as file:
        file.write(line)


def table_lines(rows):
    """A short markdown table of the rows of games: one line for each game"""
    lines = ["| Game | Status | Turns | First part | AI (gods / needs / hand / casts) | Longest turn ms | Longest record ms |",
             "|---|---|---|---|---|---|---|"]
    for row in rows:
        ai = row.get("ai") or {}
        # The turn time is without the time of the record step, which has its own column
        longest = largest(row.get("max_turn_ms"))
        record = largest(row.get("max_record_ms"))
        part = row.get("first_part_turn")
        lines.append(f"| {row['game']} | {row['status']} | {row.get('turns_played', '-')} | {'-' if part is None else part} | "
                     f"{ai.get('gods', '-')} / {ai.get('needs', '-')} / {ai.get('hand', '-')} / {ai.get('casts', '-')} | "
                     f"{'-' if longest is None else round(longest, 1)} | {'-' if record is None else round(record, 1)} |")
    return lines


def map_lines(rows):
    """One line for each game, for the merge train: the land, same or PART, the turns played, the first different turn and
    the parts that differ, and the turn of the part on purpose of the observer"""
    lines = []
    for row in rows:
        status = {"same": "same", "part": "PART"}.get(row["status"], "ERROR")
        part = row.get("first_part_turn")
        planned = row.get("part_observer_at")
        # A game in step names no parts: the parts of the row then come from the part on purpose
        parts = ", ".join(row.get("parts") or []) if status == "PART" else ""
        line = (f"{row['land']:<12} {status:<5} turns {row.get('turns_played', '-')}"
                f" first part {'-' if part is None else part} parts {parts or '-'}")
        if planned is not None:
            line += f" observer part {planned}"
        lines.append(line)
    return lines


def largest(values):
    """The largest of the values that are not none, none when there is no such value"""
    return max((value for value in values or [] if value is not None), default=None)


def read_rows(path, since=None):
    """The rows of a summary.jsonl file, those that started on or after a date (YYYY-MM-DD) when it is given"""
    rows = []
    try:
        with open(path, encoding="utf-8") as file:
            for line in file:
                try:
                    row = json.loads(line)
                except ValueError:
                    continue
                if isinstance(row, dict) and (since is None or str(row.get("started", ""))[:10] >= since):
                    rows.append(row)
    except OSError:
        pass
    return rows


def append_row(day_folder, row):
    with open(os.path.join(day_folder, "summary.jsonl"), "a", encoding="utf-8") as file:
        file.write(json.dumps(row) + "\n")


def parse_list(text, kind=str):
    return [kind(part) for part in text.split(",") if part != ""]


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--lands", default="TwoGods", help="the lands, divided by commas: " + ", ".join(LANDS))
    parser.add_argument("--seeds", default="1", help="the seeds of the world, divided by commas")
    parser.add_argument("--ai-seeds", default="1", help="the AI seeds, divided by commas (0: the built-in personalities)")
    parser.add_argument("--modes", default="all-computer", help="the modes, divided by commas: " + ", ".join(MODES))
    parser.add_argument("--observers", type=int, default=2, help="the desktop observers of each game (0 to 4)")
    parser.add_argument("--web-client", action="store_true", help="one more observer in headless Chrome (the web build)")
    parser.add_argument("--web-page", help="the folder of the web page for the web client (the web build's bin/Release)")
    parser.add_argument("--turns", type=int, default=2000, help=f"the turns of each game (up to {MOST_TURNS})")
    parser.add_argument("--until-end", action="store_true", help="each game plays until the game is decided, or its turns")
    parser.add_argument("--shuffle-seed", type=int, help="play the games of the matrix in the order of this seed")
    parser.add_argument("--web-one-game", action="store_true",
                        help="the first game with computer gods only has the web client (one Chrome)")
    parser.add_argument("--budget-minutes", type=float,
                        help="start no new game after this many minutes (a night plays what its time allows)")
    parser.add_argument("--nightly", help="a file that gets the row of each game too (appended)")
    parser.add_argument("--queue-handover", help="a part adds an AI soak part line with the repro line to this file")
    parser.add_argument("--short", action="store_true", help="the short soak of a merge-train car (see short_plan)")
    parser.add_argument("--night", action="store_true", help="every land of the night's list")
    parser.add_argument("--maps", action="store_true", help="every land of the soak (" + ", ".join(MAP_LANDS) + ")")
    parser.add_argument("--part-observer", action="store_true",
                        help="the world of the last observer of each game parts on purpose at a random turn and must "
                             "come back in step (the turn comes from --shuffle-seed and the game)")
    parser.add_argument("--part-observer-at", type=int,
                        help="as --part-observer, but at this turn in each game")
    parser.add_argument("--time-limit", type=int,
                        help="the time limit of each network game in minutes (net_check's --time-limit). The default "
                             "of 30 minutes decides a game after 18000 turns")
    parser.add_argument("--config", default="RelWithDebInfo")
    parser.add_argument("--bin", help="the folder with openblack and openblack-server")
    parser.add_argument("--out", default=DEFAULT_OUT, help="the folder of the runs (a folder of the date goes in it)")
    parser.add_argument("--date", default=datetime.date.today().isoformat(), help="the name of the folder of the date")
    parser.add_argument("--max-games", type=int, help="the most games to play")
    parser.add_argument("--dry-run", action="store_true", help="only write the games of the matrix")
    parser.add_argument("--no-guard", action="store_true", help="do not wait for the processor, the memory and the disk")
    parser.add_argument("--keep-programs", action="store_true",
                        help="keep the copies of the programs in the run folders (by default each game removes them)")
    parser.add_argument("--cpu-limit", type=float, help="start a game only under this processor use, in percent")
    parser.add_argument("--free-memory-gb", type=float, help="start a game only with this much free memory")
    parser.add_argument("--free-disk-gb", type=float, help="start a game only with this much free disk")
    parser.add_argument("--most-game-programs", type=int,
                        help="start a game only when the game programs of all lanes with its own are not more (0: no limit)")
    parser.add_argument("--inject-part", type=int, help="tests: a client's world parts at this turn and stays apart")
    parser.add_argument("--table", help="write a short markdown table of the rows of this summary.jsonl and exit")
    parser.add_argument("--lines", help="write one line for each game of this summary.jsonl and exit (see map_lines)")
    parser.add_argument("--since", help="with --table or --lines, only the games that started on or after this date (YYYY-MM-DD)")
    parser.add_argument("--self-test", action="store_true",
                        help="play a game with a known part and check that the runner finds the part, its turn and the rows")
    return parser


def games_of(options, parser=None):
    """The games that the command line asks for"""
    if options.short:
        return short_plan()
    lands = MAP_LANDS if getattr(options, "maps", False) else NIGHT_LANDS if options.night else parse_list(options.lands)
    unknown = [land for land in lands if land not in LANDS]
    modes = parse_list(options.modes)
    bad_modes = [mode for mode in modes if mode not in MODES]
    problems = []
    if unknown:
        problems.append(f"no such land: {', '.join(unknown)}")
    if bad_modes:
        problems.append(f"no such mode: {', '.join(bad_modes)}")
    if not 0 <= options.observers <= MOST_OBSERVERS:
        problems.append(f"--observers takes 0 to {MOST_OBSERVERS}")
    if not 1 <= options.turns <= MOST_TURNS:
        problems.append(f"--turns takes 1 to {MOST_TURNS}")
    if "all-computer" in modes and options.observers + (1 if options.web_client else 0) == 0:
        problems.append("the mode all-computer needs an observer: --observers or --web-client")
    part_at = getattr(options, "part_observer_at", None)
    parts = getattr(options, "part_observer", False) or part_at is not None
    if parts and options.observers == 0:
        problems.append("--part-observer needs a desktop observer: --observers")
    if part_at is not None and not 1 <= part_at < options.turns:
        problems.append("--part-observer-at takes a turn after 0 and before --turns")
    if getattr(options, "time_limit", None) is not None and options.time_limit < 1:
        problems.append("--time-limit takes 1 minute or more")
    if problems:
        raise ValueError("; ".join(problems))
    games = plan(lands, parse_list(options.seeds, int), parse_list(options.ai_seeds, int), modes, options.observers,
                 options.turns, options.web_client, options.until_end)
    if options.shuffle_seed is not None:
        # A night plays a part of a big matrix: another order each night covers other games
        random.Random(options.shuffle_seed).shuffle(games)
    if options.web_one_game and games:
        # One game of the run has the web client (one Chrome), the first one that has computer gods only
        first = next((i for i, game in enumerate(games) if game.mode == "all-computer"), 0)
        games[first] = dataclasses.replace(games[first], web_client=True)
    time_limit = getattr(options, "time_limit", None)
    if time_limit is not None:
        games = [dataclasses.replace(game, time_limit=time_limit) for game in games]
    if part_at is not None:
        games = [dataclasses.replace(game, part_observer_at=part_at) for game in games]
    elif parts:
        seed = options.shuffle_seed or 0
        games = [dataclasses.replace(game, part_observer_at=random_part_turn(game, seed)) for game in games]
    return games


def self_test(options):
    """Plays a small game whose second client parts on purpose and stays apart, and checks the runner's report: the status
    part, the turn, and the rows. This proves that the harness finds parts. Gives the exit code."""
    part_turn = 100
    game = Game("TwoGods", 1, 0, "all-but-one", 0, 300)
    day = os.path.join(options.out, options.date, "self-test")
    os.makedirs(day, exist_ok=True)
    row = play(game, day, options, guard=None if options.no_guard else Guard(day, limits=limits_of(options)),
               inject=part_turn)
    append_row(day, row)
    problems = []
    if row["status"] != "part":
        problems.append(f"the status is {row['status']}, not part")
    turn = row.get("first_part_turn")
    if turn is None or not part_turn <= turn <= part_turn + 2:
        problems.append(f"the first part turn is {turn}, not {part_turn}")
    replay = row.get("replay") or {}
    rows = replay.get("rows") or {}
    if not rows:
        problems.append("the replay found no row that differs")
    print(json.dumps(row, indent=2))
    if problems:
        print("self-test FAILED: " + "; ".join(problems))
        return 1
    print(f"self-test passed: the part at turn {turn} was found and the rows named")
    return 0


def limits_of(options):
    """The limits of this computer, with the limits that the command line changes"""
    limits = default_limits()
    changes = {}
    if getattr(options, "cpu_limit", None) is not None:
        changes["cpu_percent"] = options.cpu_limit
    if getattr(options, "free_memory_gb", None) is not None:
        changes["free_memory_gb"] = options.free_memory_gb
    if getattr(options, "free_disk_gb", None) is not None:
        changes["free_disk_gb"] = options.free_disk_gb
    if getattr(options, "most_game_programs", None) is not None:
        changes["most_game_programs"] = options.most_game_programs or None
    return dataclasses.replace(limits, **changes)


def main(argv=None):
    parser = build_parser()
    options = parser.parse_args(argv)
    if options.self_test:
        return self_test(options)
    if options.table:
        print("\n".join(table_lines(read_rows(options.table, options.since))))
        return 0
    if options.lines:
        print("\n".join(map_lines(read_rows(options.lines, options.since))))
        return 0
    if options.inject_part is not None and (options.part_observer or options.part_observer_at is not None):
        print("ai_soak: --inject-part does not go with --part-observer")
        return 2
    try:
        games = games_of(options, parser)
    except ValueError as error:
        print(f"ai_soak: {error}")
        return 2
    if options.max_games is not None:
        games = games[:options.max_games]
    began = time.monotonic()
    day = os.path.join(options.out, options.date)
    print(f"{len(games)} games; results in {day}")
    if options.dry_run:
        for game in games:
            print(" ".join(net_check_arguments(game, os.path.join(day, game.name, "run"), options.bin, options.config)))
        return 0
    os.makedirs(day, exist_ok=True)
    guard = None if options.no_guard else Guard(day, limits=limits_of(options))
    failed = 0
    for game in games:
        if options.budget_minutes is not None and (time.monotonic() - began) / 60.0 >= options.budget_minutes:
            print("the time of the run is used: no new game starts")
            break
        print(f"game {game.name}", flush=True)
        try:
            row = play(game, day, options, guard=guard, inject=options.inject_part)
        except TimeoutError as error:
            print(f"ai_soak: {error}")
            return 1
        append_row(day, row)
        if options.nightly:
            os.makedirs(os.path.dirname(os.path.abspath(options.nightly)), exist_ok=True)
            with open(options.nightly, "a", encoding="utf-8") as file:
                file.write(json.dumps(row) + "\n")
        if row["status"] == "part" and options.queue_handover:
            queue_line(options.queue_handover, row)
        print(f"  {row['status']}" + (f", first part at turn {row['first_part_turn']}, parts {row['parts']}"
                                      if row["status"] == "part" else ""), flush=True)
        if row.get("repro"):
            print("  repro: " + row["repro"])
        failed += row["status"] != "same"
    print(f"{len(games) - failed} of {len(games)} games stayed in step")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
