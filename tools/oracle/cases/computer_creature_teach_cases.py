#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the lessons of a computer god for its creature, in test/oracle/data:

- computer_creature_lesson_wish.txt: how much the god wants to teach its creature an angry, a kind or an impressive
  miracle, from the times that it sent the creature while the creature did not know that kind. The three rules of the
  game run; the call that finds the creature is a stub. The rule also makes the creature the object of the node.

Usage: python tools/oracle/cases/computer_creature_teach_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20): thiscall on the computer player, arg (node), RET 4; the wish in ST0
WISHES = {
    "aggressive": 0x6591C0,
    "compassionate": 0x659250,
    "impressive": 0x6592E0,
}
CREATURE_OF = 0x65BCB0  # thiscall on the computer player, RET: the creature

# The computer player counts the sends of its creature for each kind: to impress, kind, angry
SENDS = {"impressive": 0x114, "compassionate": 0x118, "aggressive": 0x11C}
# The node keeps its object at +0x94
NODE_OBJECT = 0x94


class LessonWish:
    COLUMNS = ["kind", "sends", "wish"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.creature = o.alloc(0x200)
        o.stub(CREATURE_OF, eax=self.creature, pop=0)
        self.player = o.alloc(0x1000)
        self.node = o.alloc(0x100)

    def run(self, kind: int, sends: int) -> list[int]:
        o = self.o
        name = list(WISHES)[kind]
        o.write_u32(self.player + SENDS[name], sends)
        o.write_u32(self.node + NODE_OBJECT, 0)
        result = o.call(WISHES[name], this=self.player, args=[self.node])
        assert result.popped == 4 and result.depth == 1, result
        assert o.read_u32(self.node + NODE_OBJECT)[0] == self.creature
        return [result.st0f]


def wish_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for kind in range(len(WISHES)):
        for sends in [0, 1, 2, 3, 0xFFFFFF, 0x1000000, 0x1000001, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]:
            rows.append((kind, sends))
        for _ in range(300):
            rows.append((kind, rng.choice([rng.randrange(0, 8), rng.randrange(0, 0x100000000)])))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x6591C0)
    with Oracle() as oracle:
        wish = LessonWish(oracle)
        rows = [[*case, *wish.run(*case)] for case in wish_cases(rng)]
        n = write_pinned(out / "computer_creature_lesson_wish.txt", "The wish of a computer god to teach a miracle",
                         ["Functions: thiscall on the computer player, arg (node), RET 4; the wish in ST0. kind 0 is",
                          "angry, 1 kind, 2 impressive; sends is the count of sends of the creature for that kind.",
                          "The creature is a stub. Floats are bits."],
                         LessonWish.COLUMNS, rows)
        print(f"computer_creature_lesson_wish: {n} rows")


if __name__ == "__main__":
    main()
