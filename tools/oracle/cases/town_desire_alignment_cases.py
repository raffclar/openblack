#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how a town's wants move its player's alignment, in test/oracle/data:

- town_desire_alignment.txt: the wants and their boosts, the turns over the level before and after, the table's three
  values of each desire, the people of the town and of the players, the most change a turn, and the change

Usage: python tools/oracle/cases/town_desire_alignment_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
CHANGE = 0x7466D0  # thiscall (the town's desires), plain RET; the change on ST0
HISTORY_ADD = 0x414D40  # the debug history of the alignment; RET 0xC

DESIRES = 17
# The town's desires: the cheat boost, the script boost, the desire, the turns over the level, the town
BOOST_A = 0x90
BOOST = 0xD4
DESIRE = 0x118
TURNS = 0x410
TOWN = 0x160
# The town: its info, its children and adults
TOWN_INFO = 0x28
CHILDREN = 0x618
ADULTS = 0x61C
# The town info: the most change a turn
MAX_CHANGE = 0xBC
# The table of desires, one row of 0x90 bytes for each: the level, the importance and the most turns
DESIRE_TABLE = 0xDA2930
ROW = 0x90
AFTER = 0x4C
IMPORTANCE = 0x50
MAX_TURNS = 0x54
# The game, its eight players (0xA60 bytes each, the neutral player last), their active flag and their people
GAME_POINTER = 0xD0195C
FIRST_PLAYER = 0x18
PLAYER_SIZE = 0xA60
PLAYERS = 8
ACTIVE = 0x8E0
PEOPLE = 0x948

COLUMNS = ([f"boost_a{d}" for d in range(DESIRES)] + [f"desire{d}" for d in range(DESIRES)] +
           [f"boost{d}" for d in range(DESIRES)] + [f"turns{d}" for d in range(DESIRES)] +
           [f"after{d}" for d in range(DESIRES)] + [f"importance{d}" for d in range(DESIRES)] +
           [f"max_turns{d}" for d in range(DESIRES)] + ["children", "adults"] +
           [f"active{p}" for p in range(PLAYERS)] + [f"people{p}" for p in range(PLAYERS)] + ["max_change", "change"] +
           [f"new_turns{d}" for d in range(DESIRES)])


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261021)
    with Oracle() as o:
        desires = o.alloc(0x500)
        town = o.alloc(0x700)
        info = o.alloc(0x100)
        game = o.alloc(0x5400)
        o.write_u32(desires + TOWN, town)
        o.write_u32(town + TOWN_INFO, info)
        o.write_u32(GAME_POINTER, game)
        o.stub(HISTORY_ADD, pop=0xC)

        rows = []
        for case in range(800):
            row_values: dict[str, list[int]] = {}

            def floats(name: str, low: float, high: float) -> list[int]:
                values = []
                for _ in range(DESIRES):
                    pick = rng.random()
                    if pick < 0.15:
                        value = 0.0
                    elif pick < 0.25:
                        value = rng.choice([0.5, 1.0, -0.5, 0.25])
                    else:
                        value = rng.uniform(low, high)
                    values.append(f2u(value))
                row_values[name] = values
                return values

            boost_a = floats("boost_a", -0.3, 0.3)
            desire = floats("desire", -1.0, 1.0)
            boost = floats("boost", -0.5, 0.5)
            after = floats("after", -0.2, 1.2)
            importance = floats("importance", -2.0, 2.0)
            turns = [rng.choice([0, 0, 1, rng.randrange(0, 50), rng.randrange(0, 5000)]) for _ in range(DESIRES)]
            max_turns = [rng.choice([1, 10, 100, rng.randrange(1, 3000), 0 if case % 50 == 7 else 200])
                         for _ in range(DESIRES)]
            children = rng.choice([0, rng.randrange(0, 40)])
            adults = rng.choice([0, rng.randrange(0, 120)])
            active = [rng.choice([0, 1, 1]) for _ in range(PLAYERS)]
            people = [rng.choice([0, rng.randrange(0, 400)]) for _ in range(PLAYERS)]
            if case % 10 == 3:
                people = [0] * PLAYERS
            max_change = f2u(rng.choice([0.0005, 0.001, rng.uniform(0.0, 0.01)]))
            for d in range(DESIRES):
                o.write_u32(desires + BOOST_A + 4 * d, boost_a[d])
                o.write_u32(desires + DESIRE + 4 * d, desire[d])
                o.write_u32(desires + BOOST + 4 * d, boost[d])
                o.write_u32(desires + TURNS + 4 * d, turns[d])
                o.write_u32(DESIRE_TABLE + ROW * d + AFTER, after[d])
                o.write_u32(DESIRE_TABLE + ROW * d + IMPORTANCE, importance[d])
                o.write_u32(DESIRE_TABLE + ROW * d + MAX_TURNS, max_turns[d])
            o.write_u32(town + CHILDREN, children)
            o.write_u32(town + ADULTS, adults)
            o.write_u32(info + MAX_CHANGE, max_change)
            for p in range(PLAYERS):
                o.write_u32(game + FIRST_PLAYER + PLAYER_SIZE * p + ACTIVE, active[p])
                o.write_u32(game + FIRST_PLAYER + PLAYER_SIZE * p + PEOPLE, people[p])
            r = o.call(CHANGE, this=desires)
            assert r.popped == 0 and r.depth == 1, r
            new_turns = o.read_u32(desires + TURNS, DESIRES)
            rows.append(boost_a + desire + boost + turns + after + importance + max_turns + [children, adults] + active +
                        people + [max_change, r.st0f] + list(new_turns))
        write_pinned(out / "town_desire_alignment.txt",
                     "How a town's wants move its player's alignment (the game's own function)",
                     ["Wants, levels, importances, the most change and the change are float bits; the rest are whole "
                      "numbers. The neutral player is the last player and always counts."], COLUMNS, rows)
    print("done")


if __name__ == "__main__":
    main()
