#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the threat to a town of a computer god, in test/oracle/data:

- computer_town_threat.txt: the attack on a town that the town fears most. The inputs are the game turn and, for each
  of the seven players, the turn of its last attack, the harm of its miracles and the two parts of the harm of the
  things that hit the town. The results are the threat (0 or 1), the bits of the fear, of the harm of miracles and of
  the harm of things.

The game's function runs as it is. The town's records of the attacks and the game turn are laid out in memory and read
by the game's own code.

Usage: python tools/oracle/cases/computer_town_threat_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
TOWN_THREAT = 0x741980  # thiscall (town, &fear, &miracle harm, &thing harm), ret 0xC, EAX
G_GAME = 0xD0195C
GAME_TURN = 0x205A40

# Offsets of the records of the attacks of each player in a town, 0x80 apart
RECORD = 0x80
RECORD_TURN = 0x9FC
RECORD_MIRACLE = 0xA38
RECORD_THING = 0xA3C
RECORD_OTHER_THING = 0xA40
PLAYERS = 7


def harm(pick: random.Random) -> float:
    return pick.choice([0.0, 0.0, -1.0, 0.5, 1.0, pick.uniform(0.0, 50.0), pick.uniform(-2.0, 2.0)])


def town_threat(o: Oracle, pick: random.Random) -> list[list[int]]:
    cases = []
    # Each recency of one attack, with the others long ago
    for since in (0, 1, 2, 3, 99, 100, 299, 300, 301, 450, 597, 598, 599, 600, 601, 1000, 0xFFFFFFFF, 0x80000000):
        records = [(0, 0.0, 0.0, 0.0)] * PLAYERS
        turn = 100000
        records = list(records)
        records[pick.randrange(PLAYERS)] = ((turn - since) & 0xFFFFFFFF, 3.0, 1.0, 0.5)
        cases.append((turn, records))
    # Ties and harms that are the same
    for _ in range(40):
        turn = pick.randrange(600, 5000)
        last = pick.randrange(turn - 700, turn + 1)
        records = [(last if pick.random() < 0.5 else 0, harm(pick), harm(pick), harm(pick)) for _ in range(PLAYERS)]
        cases.append((turn, records))
    for _ in range(600):
        turn = pick.randrange(0, 20000)
        records = [((turn - pick.randrange(0, 900)) & 0xFFFFFFFF, harm(pick), harm(pick), harm(pick))
                   for _ in range(PLAYERS)]
        cases.append((turn, records))
    rows = []
    for turn, records in cases:
        o.reset()
        game = o.alloc(GAME_TURN + 0x10)
        o.write_u32(G_GAME, game)
        o.write_u32(game + GAME_TURN, turn)
        town = o.alloc(0xE00)
        row = [turn]
        for index, (last, miracle, thing, other) in enumerate(records):
            base = town + index * RECORD
            o.write_u32(base + RECORD_TURN, last)
            o.write_f32(base + RECORD_MIRACLE, f32(miracle))
            o.write_f32(base + RECORD_THING, f32(thing))
            o.write_f32(base + RECORD_OTHER_THING, f32(other))
            row += [last, f32(miracle).bits, f32(thing).bits, f32(other).bits]
        outs = o.alloc(0x10)
        r = o.call(TOWN_THREAT, this=town, args=[outs, outs + 4, outs + 8])
        fear, miracle_harm, thing_harm = o.read_u32(outs, 3)
        rows.append(row + [r.eax & 0xFF, fear, miracle_harm, thing_harm])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x7A11)
    with Oracle() as o:
        threats = town_threat(o, pick)
    columns = ["turn"]
    for index in range(PLAYERS):
        columns += [f"last{index}", f"miracle{index}", f"thing{index}", f"other{index}"]
    columns += ["threat", "fear", "miracle", "thing"]
    count = write_pinned(
        out / "computer_town_threat.txt",
        "The threat to a town of a computer god from the attack that it fears most",
        ["turn is the game turn; lastN, miracleN, thingN and otherN are the turn of the last attack of player N and the",
         "bits of the harm of its miracles and of the two parts of the harm of things. threat is 1 when there is a",
         "threat; fear, miracle and thing are the bits of the results, which the game writes in every case."],
        columns,
        threats,
    )
    print(f"computer_town_threat: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
