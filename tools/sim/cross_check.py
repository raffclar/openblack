#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Checks that the desktop build and the web build play the same lockstep game, turn by turn.

A network game sends each player's commands to every computer, and each computer plays the same turns with them. Thus
a desktop player and a web player can share a game only when both builds make the same world from the same land, seed
and commands. The check:

1. records: the desktop game plays the land in the fast simulation mode with a seed. Its hand stays still, or it gets
   recorded input (--input, the files of tools/sim/hand_inputs). It writes the commands of each turn (--sim-commands)
   and the checksum of each turn (--sim-checksums).
2. desktop: the desktop game plays the same land and seed with the commands of the record (--sim-play-commands), as a
   computer that only receives them, and writes the checksum of each turn.
3. web: the web build does the same in headless Chrome (web/scripts/cdp-run.mjs). The page gets the commands from its
   own server (the page parameter tool-file), and the checksums come back through the page (--sim-checksums -).

The desktop and web checksums must be the same at each turn. The record's checksums are compared too, for
information: they show whether the commands carry all of the input. With --parts, each checksum keeps the hash of each
part of the synced world, and the report names the parts that differ first.

    python tools/sim/cross_check.py --turns 1000
    python tools/sim/cross_check.py --turns 1000 --testbed
    python tools/sim/cross_check.py --turns 1000 --input tools/sim/hand_inputs/land1.json --parts
    python tools/sim/cross_check.py --turns 300 --web-only --commands E:/runs/x/commands.jsonl

Each program starts with no console window of its own, and the desktop game with --background. The script stops only
the processes that it started, by their own process ids. Each Chrome run has its own profile folder in the output
folder.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DEFAULT_BUILDS = os.environ.get("OB_BUILDS", "E:/openblack/builds")
DEFAULT_GAME_PATH = os.environ.get("OB_GAME_PATH", "C:/projects/black")
DEFAULT_CHROME = os.environ.get("OB_CHROME", "C:/Program Files/Google/Chrome/Application/chrome.exe")
DEFAULT_NODE = os.environ.get("OB_NODE", "node")
# The web game's folder of the files that the page gives it beside the game folder (see launcher.js)
WEB_TOOL_FILES = "/game/tool-files"
# A run that has not finished in this time is stopped
RUN_LIMIT_SECONDS = 1800


def worktree_name():
    return os.path.basename(ROOT)


def run(command, cwd, limit=RUN_LIMIT_SECONDS, log=None):
    """Runs a program with no console window of its own, and stops it by its own process id after the limit"""
    os.makedirs(cwd, exist_ok=True)
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    out = open(log, "w", encoding="utf-8") if log else subprocess.DEVNULL
    try:
        process = subprocess.Popen(command, cwd=cwd, stdout=out, stderr=subprocess.STDOUT if log else subprocess.DEVNULL,
                                   creationflags=flags)
        try:
            return process.wait(timeout=limit)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            raise RuntimeError(f"{command[0]} (pid {process.pid}) did not finish in {limit} s")
    finally:
        if log:
            out.close()


def read_checksums(path):
    """The line of each turn of a checksum file, by the turn"""
    turns = {}
    if not os.path.exists(path):
        return turns
    with open(path, encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            if not line:
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                continue
            if "turn" in value:
                turns[int(value["turn"])] = value
    return turns


def compare(first, other, turns):
    """How two checksum files compare up to a turn: the turns compared, the first turn that differs and its parts"""
    compared = [turn for turn in sorted(first) if turn <= turns and turn in other]
    different = [turn for turn in compared if first[turn]["checksum"] != other[turn]["checksum"]]
    result = {
        "compared": len(compared),
        "different": len(different),
        "first_different": different[0] if different else None,
        "same_until": (different[0] - 1) if different else (compared[-1] if compared else 0),
        "same": not different and len(compared) >= turns,
    }
    if different:
        a, b = first[different[0]].get("parts", {}), other[different[0]].get("parts", {})
        result["parts"] = sorted(name for name in set(a) | set(b) if a.get(name) != b.get(name))
        # The first turn at which each part differs
        firsts = {}
        for turn in different:
            pa, pb = first[turn].get("parts", {}), other[turn].get("parts", {})
            for name in set(pa) | set(pb):
                if pa.get(name) != pb.get(name):
                    firsts.setdefault(name, turn)
        result["first_by_part"] = dict(sorted(firsts.items(), key=lambda item: item[1])[:30])
    return result


def where_arguments(options):
    if options.scenario:
        return ["--scenario", options.scenario, "--scenario-hide-window"]
    if options.testbed:
        return ["--testbed"]
    return ["-s", options.land, "--skip-opening", options.opening]


def common_arguments(options):
    """The arguments that every run of the check shares: the land, the seed and the fixed frame time"""
    arguments = ["--skip-logos", "--seed", str(options.seed), "--sim-fast", "--sim-draw-every", "0",
                 "--sim-frame-ms", str(options.frame_ms), "--sim-until-turn", str(options.turns + 1)]
    if options.dump:
        arguments += ["--sim-checksum-dump", options.dump]
    if options.fields:
        arguments.append("--sim-checksum-fields")
    elif options.parts:
        arguments.append("--sim-checksum-parts")
    return arguments + where_arguments(options)


def record(options, out):
    """The desktop game plays the land with its own hand, and writes the commands and the checksum of each turn"""
    folder = os.path.join(out, "record")
    commands = os.path.join(folder, "commands.jsonl")
    checksums = os.path.join(folder, "checksums.jsonl")
    for path in (commands, checksums):
        if os.path.exists(path):
            os.remove(path)
    command = [options.desktop_exe, "-g", options.game_path] + common_arguments(options) + [
        "--sim-commands", commands, "--sim-checksums", checksums, "-n", "4000000000", "--background",
        "-l", os.path.join(folder, "game.log")]
    # The commands and the recorded input come with the run record of the inspector
    command += ["--sim-record", os.path.join(folder, "record.jsonl"), "--sim-record-every", "100000",
                "--inspect-lock-input"]
    if options.input:
        # The picks of the recorded input need drawn frames
        command += ["--sim-input", os.path.abspath(options.input)]
        command[command.index("--sim-draw-every") + 1] = "1"
    else:
        command.append("--sim-still-hand")
    code = run(command, folder)
    if code != 0:
        raise RuntimeError(f"the record run ended with exit code {code}; see {folder}")
    if not os.path.exists(commands):
        raise RuntimeError(f"the record run wrote no commands; see {folder}")
    return commands, checksums


def play_desktop(options, out, commands):
    """The desktop game plays the commands of the record and writes the checksum of each turn"""
    folder = os.path.join(out, "desktop")
    checksums = os.path.join(folder, "checksums.jsonl")
    if os.path.exists(checksums):
        os.remove(checksums)
    command = [options.desktop_exe, "-g", options.game_path] + common_arguments(options) + [
        "--sim-still-hand", "--sim-play-commands", commands, "--sim-checksums", checksums, "-n", "4000000000",
        "--background", "-l", os.path.join(folder, "game.log")]
    code = run(command, folder)
    if code != 0:
        raise RuntimeError(f"the desktop run ended with exit code {code}; see {folder}")
    return checksums


def play_web(options, out, commands):
    """The web build plays the commands of the record in headless Chrome and gives back the checksum of each turn"""
    folder = os.path.join(out, "web")
    os.makedirs(folder, exist_ok=True)
    checksums = os.path.join(folder, "checksums.jsonl")
    page = options.web_page
    # The page's server gives the commands to the page under a name of this run
    name = f"cross-check-{os.getpid()}-commands.jsonl"
    served = os.path.join(page, name)
    shutil.copyfile(commands, served)
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    server = subprocess.Popen([sys.executable, os.path.join(ROOT, "web", "scripts", "serve.py"), page,
                               str(options.port)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              creationflags=flags)
    try:
        time.sleep(1.0)
        arguments = common_arguments(options) + ["--sim-still-hand", "--sim-play-commands",
                                                 f"{WEB_TOOL_FILES}/{name}", "--sim-checksums", "-"]
        if options.renderer:
            arguments += ["--backend-type", options.renderer]
        query = "autoplay&tool-file=" + name + "&args=" + "%20".join(arguments)
        url = f"http://127.0.0.1:{options.port}/index.html?{query}"
        command = [DEFAULT_NODE, os.path.join(ROOT, "web", "scripts", "cdp-run.mjs"), options.chrome, url,
                   str(options.web_seconds), folder, "--game-folder", options.game_path, "--checksums", checksums,
                   "--checksum-turns", str(options.turns)]
        if options.gpu:
            command.append("--gpu")
        code = run(command, folder, limit=options.web_seconds + 600, log=os.path.join(folder, "cdp-run.log"))
        if code != 0:
            raise RuntimeError(f"the web run ended with exit code {code}; see {folder}")
    finally:
        server.kill()
        server.wait()
        if os.path.exists(served):
            os.remove(served)
    return checksums


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--turns", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--land", default="Land1.txt")
    parser.add_argument("--opening", default="cow")
    parser.add_argument("--testbed", action="store_true", help="the creature testbed instead of a land")
    parser.add_argument("--scenario", help="a testbed scenario by its id")
    parser.add_argument("--frame-ms", type=float, default=20.0)
    parser.add_argument("--input", help="recorded input for the hand of the record run (tools/sim/hand_inputs)")
    parser.add_argument("--parts", action="store_true", help="keep the hash of each part of the synced world")
    parser.add_argument("--fields", action="store_true", help="as --parts, and the hash of each field (large lines)")
    parser.add_argument("--dump", help="TURN:Component,Component: each field of each row of those components at the turn")
    parser.add_argument("--config", default="RelWithDebInfo", help="the desktop build's config")
    parser.add_argument("--desktop-exe", help="openblack.exe of the desktop build")
    parser.add_argument("--web-page", help="the folder of the web page (the web build's bin/Release)")
    parser.add_argument("--web-config", default="Release")
    parser.add_argument("--game-path", default=DEFAULT_GAME_PATH)
    parser.add_argument("--chrome", default=DEFAULT_CHROME)
    parser.add_argument("--gpu", action="store_true", help="Chrome draws with the graphics card, not SwiftShader")
    parser.add_argument("--renderer", choices=("WebGPU", "OpenGLES"), default="OpenGLES",
                        help="the web build's renderer (it draws no frame in the check)")
    parser.add_argument("--port", type=int, default=8711)
    parser.add_argument("--web-seconds", type=int, default=1500, help="the longest time of the web run")
    parser.add_argument("--commands", help="play the commands of this file, with no record run")
    parser.add_argument("--web-only", action="store_true", help="with --commands, only the web run")
    parser.add_argument("--desktop-only", action="store_true", help="the record and desktop runs only")
    parser.add_argument("--out", default=os.path.join(os.getcwd(), "cross-check"))
    options = parser.parse_args(argv)

    name = worktree_name()
    options.desktop_exe = options.desktop_exe or os.path.join(DEFAULT_BUILDS, name, "bin", options.config,
                                                              "openblack.exe")
    options.web_page = options.web_page or os.path.join(DEFAULT_BUILDS, f"{name}-web", "bin", options.web_config)
    out = os.path.abspath(options.out)
    os.makedirs(out, exist_ok=True)

    summary = {"turns": options.turns, "seed": options.seed, "land": " ".join(where_arguments(options)),
               "input": options.input, "frame_ms": options.frame_ms}
    started = time.monotonic()
    recorded = None
    if options.commands:
        commands = os.path.abspath(options.commands)
    else:
        commands, recorded = record(options, out)
    desktop = None if options.web_only else play_desktop(options, out, commands)
    web = None if options.desktop_only else play_web(options, out, commands)

    turns = {key: read_checksums(path) for key, path in (("record", recorded), ("desktop", desktop), ("web", web))
             if path}
    summary["lines"] = {key: len(value) for key, value in turns.items()}
    if "desktop" in turns and "web" in turns:
        summary["desktop_web"] = compare(turns["desktop"], turns["web"], options.turns)
    if "record" in turns and "desktop" in turns:
        summary["record_desktop"] = compare(turns["record"], turns["desktop"], options.turns)
    summary["seconds"] = round(time.monotonic() - started, 1)
    summary["same"] = summary.get("desktop_web", {}).get("same", False)
    with open(os.path.join(out, "cross_check.json"), "w", encoding="utf-8") as file:
        json.dump(summary, file, indent=2)
    print(json.dumps(summary))
    if options.desktop_only:
        return 0 if summary.get("record_desktop", {}).get("same", False) else 1
    return 0 if summary["same"] else 1


if __name__ == "__main__":
    sys.exit(main())
