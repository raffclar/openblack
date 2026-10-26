"""The AI soak: long network games in which computer gods play every god, with the checksum of every turn compared on the
server and on the observers, so that any write in a frame step and any local read in a system shows as a part.

A matrix of games: lands x seeds x AI seeds x modes. Each game is one run of tools/sim/net_check.py. When a game parts
from the server's world, the runner plays the same game again with the row dump of the first different turn, the draw
trace, the hash of each field and the frame guard of the clients, and writes a repro line: the command that plays that
game again. The runs go to <out>/<date>/<game>/ (E:/openblack/sim-runs/ai-soak by default), one row for each game goes
to summary.jsonl there.

    python tools/sim/ai_soak.py --short
    python tools/sim/ai_soak.py --lands TwoGods,ring32 --seeds 1,2 --ai-seeds 0,1 --modes all-computer --turns 2000
    python tools/sim/ai_soak.py --self-test

The modes: all-computer (the room has no human god; every client is an observer) and all-but-one (client 0 is the human
god with a still hand, the other gods are computer gods, and the observers watch). An AI seed of 0 means the built-in
personalities of the gods. The games need a build with the developer tools and the AI soak modes (--bin, or the build
folder of this tree).

The runner checks the processor, the memory and the disk before each game (the processor under 75 %, at least 16 GB of
memory free, at least 150 GB free on the drive of the output folder) and waits until they allow a game. It stops only the
net_check processes that it started, by their process ids.

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
DEFAULT_OUT = os.environ.get("OB_AI_SOAK_OUT", "E:/openblack/sim-runs/ai-soak")
DEFAULT_BUILDS = os.environ.get("OB_BUILDS", "E:/openblack/builds")

# The limits that a game needs (the lead's rules for the shared computer)
CPU_LIMIT_PERCENT = 75.0
FREE_MEMORY_GB = 16.0
FREE_DISK_GB = 150.0
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
    # The flat testbed, for the self-test
    "testbed": ["--testbed"],
}
# The lands of a night's soak. "Death Comes To Those That Wait" joins after the script fix of the plan (package S2).
NIGHT_LANDS = ["TwoGods", "ThreeGods", "FourGods", "IslandWars", "Firestorm", "mpm_2p_1", "mpm_3p_1", "mpm_4p_1", "ring32"]
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

    @property
    def name(self):
        ai = f"ai{self.ai_seed}" if self.ai_seed else "ai-none"
        web = "-web" if self.web_client else ""
        end = "-end" if self.until_end else ""
        return f"{self.land}-s{self.seed}-{ai}-{self.mode}-o{self.observers}{web}-t{self.turns}{end}"


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
    """The longest turn of each client, in milliseconds, as the clients' records tell"""
    return [client.get("max_turn_ms") for client in summary.get("clients", [])]


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
        row["ended"] = summary.get("ended")
    if repro:
        row["repro"] = repro
    if replay:
        row["replay"] = replay
    return row


class Guard:
    """Waits until the computer allows a game: processor, memory and disk. The probes are functions, so that tests give
    their own."""

    def __init__(self, out, probes=None, sleep=time.sleep, clock=time.monotonic, wait_limit=WAIT_LIMIT_SECONDS,
                 poll=POLL_SECONDS):
        self.out = out
        self.probes = probes or default_probes(out)
        self.sleep = sleep
        self.clock = clock
        self.wait_limit = wait_limit
        self.poll = poll

    def blockers(self):
        """The reasons why no game may start now; empty when a game may start"""
        cpu, memory, disk = self.probes["cpu"](), self.probes["memory_gb"](), self.probes["disk_gb"]()
        reasons = []
        if cpu >= CPU_LIMIT_PERCENT:
            reasons.append(f"processor {cpu:.0f} % (limit {CPU_LIMIT_PERCENT:.0f} %)")
        if memory < FREE_MEMORY_GB:
            reasons.append(f"free memory {memory:.1f} GB (needs {FREE_MEMORY_GB:.0f} GB)")
        if disk < FREE_DISK_GB:
            reasons.append(f"free disk {disk:.0f} GB (needs {FREE_DISK_GB:.0f} GB)")
        return reasons

    def wait(self):
        """Waits for the computer; raises TimeoutError after the wait limit"""
        end = self.clock() + self.wait_limit
        while True:
            reasons = self.blockers()
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
    }


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
    if guard is not None:
        guard.wait()
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
            guard.wait()
        replay_folder = os.path.join(folder, "replay")
        replay_arguments = net_check_arguments(game, replay_folder, options.bin, options.config, extra + replay_extra(turn))
        runner(replay_arguments, os.path.join(folder, "replay.log"), game_limit_seconds(game))
        again = read_summary(replay_folder)
        replay = replay_report(again, turn)
        with open(os.path.join(folder, "repro.txt"), "w", encoding="utf-8") as file:
            file.write(repro + "\n")
            file.write(json.dumps({"first_part_turn": turn, "client": client, "parts": part_names(summary), "replay": replay},
                                  indent=2) + "\n")
    return summary_row(game, status, summary, started, started_clock, time.time(), repro, replay)


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
    lines = ["| Game | Status | Turns | First part | AI (gods / needs / hand / casts) | Longest turn ms |",
             "|---|---|---|---|---|---|"]
    for row in rows:
        ai = row.get("ai") or {}
        longest = max((value for value in row.get("max_turn_ms") or [] if value is not None), default=None)
        part = row.get("first_part_turn")
        lines.append(f"| {row['game']} | {row['status']} | {row.get('turns_played', '-')} | {'-' if part is None else part} | "
                     f"{ai.get('gods', '-')} / {ai.get('needs', '-')} / {ai.get('hand', '-')} / {ai.get('casts', '-')} | "
                     f"{'-' if longest is None else round(longest, 1)} |")
    return lines


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
    parser.add_argument("--turns", type=int, default=2000, help="the turns of each game (up to 18000)")
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
    parser.add_argument("--config", default="RelWithDebInfo")
    parser.add_argument("--bin", help="the folder with openblack and openblack-server")
    parser.add_argument("--out", default=DEFAULT_OUT, help="the folder of the runs (a folder of the date goes in it)")
    parser.add_argument("--date", default=datetime.date.today().isoformat(), help="the name of the folder of the date")
    parser.add_argument("--max-games", type=int, help="the most games to play")
    parser.add_argument("--dry-run", action="store_true", help="only write the games of the matrix")
    parser.add_argument("--no-guard", action="store_true", help="do not wait for the processor, the memory and the disk")
    parser.add_argument("--inject-part", type=int, help="tests: a client's world parts at this turn and stays apart")
    parser.add_argument("--table", help="write a short markdown table of the rows of this summary.jsonl and exit")
    parser.add_argument("--since", help="with --table, only the games that started on or after this date (YYYY-MM-DD)")
    parser.add_argument("--self-test", action="store_true",
                        help="play a game with a known part and check that the runner finds the part, its turn and the rows")
    return parser


def games_of(options, parser=None):
    """The games that the command line asks for"""
    if options.short:
        return short_plan()
    lands = NIGHT_LANDS if options.night else parse_list(options.lands)
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
    if not 1 <= options.turns <= 18000:
        problems.append("--turns takes 1 to 18000")
    if "all-computer" in modes and options.observers + (1 if options.web_client else 0) == 0:
        problems.append("the mode all-computer needs an observer: --observers or --web-client")
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
    return games


def self_test(options):
    """Plays a small game whose second client parts on purpose and stays apart, and checks the runner's report: the status
    part, the turn, and the rows. This proves that the harness finds parts. Gives the exit code."""
    part_turn = 40
    game = Game("testbed", 1, 0, "all-but-one", 0, 160)
    day = os.path.join(options.out, options.date, "self-test")
    os.makedirs(day, exist_ok=True)
    row = play(game, day, options, guard=None if options.no_guard else Guard(day), inject=part_turn)
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


def main(argv=None):
    parser = build_parser()
    options = parser.parse_args(argv)
    if options.self_test:
        return self_test(options)
    if options.table:
        print("\n".join(table_lines(read_rows(options.table, options.since))))
        return 0
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
    guard = None if options.no_guard else Guard(day)
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
