#!/usr/bin/env python3
"""Gives the names of the turn steps in the lines that the game logs with --turn-step-times.

The game logs each turn that takes the time of the switch or more, with the time of each of its steps by the number of
the step in game_turn::Step. This script reads the numbers and the names of the steps from src/GameTurnOrder.h and
writes each line again with the names.

    python tools/sim/turn_steps.py <game log> [--turns N]
"""
import argparse
import os
import re
import sys

LINE = re.compile(r"Turn (\d+) took ([\d.]+) ms\. Steps \(number in game_turn::Step\) of 0\.1 ms or more: ?(.*)$")


def step_names(header_text):
    """The names of game_turn::Step in the order of their numbers"""
    start = header_text.index("enum class Step : uint8_t")
    body = header_text[header_text.index("{", start) + 1:header_text.index("};", start)]
    body = re.sub(r"//[^\n]*", "", body)
    return [name.strip().split("=")[0].strip() for name in body.split(",") if name.strip()]


def named(line, names):
    """The turn, its time and its steps with their names, or None for a line that is not a turn line"""
    found = LINE.search(line)
    if found is None:
        return None
    steps = []
    for item in found.group(3).split():
        number, milliseconds = item.split("=")
        index = int(number)
        steps.append((names[index] if index < len(names) else number, float(milliseconds)))
    return int(found.group(1)), float(found.group(2)), steps


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log", help="the game log")
    parser.add_argument("--turns", type=int, default=None, help="only the turns up to this number")
    options = parser.parse_args()
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    with open(os.path.join(root, "src", "GameTurnOrder.h"), encoding="utf-8") as header:
        names = step_names(header.read())
    with open(options.log, encoding="utf-8", errors="replace") as log:
        for line in log:
            turn = named(line, names)
            if turn is None or (options.turns is not None and turn[0] > options.turns):
                continue
            steps = " ".join(f"{name}={milliseconds:.2f}" for name, milliseconds in turn[2])
            print(f"turn {turn[0]} {turn[1]:.2f} ms: {steps}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
