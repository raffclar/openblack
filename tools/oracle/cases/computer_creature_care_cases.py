#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how free the creature of a computer god is, in test/oracle/data:

- computer_creature_freedom.txt: the freedom of the creature from its leash, its fight, its life, its rest, its energy
  and its plan, from 0 to 1

The calls that find the creature, its leash and its fight are stubs. The life comes from the game's own getter.

Usage: python tools/oracle/cases/computer_creature_care_cases.py [output folder]   (default: test/oracle/data)
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
FREEDOM = 0x658BC0  # thiscall on the computer player, RET; the freedom in ST0
CREATURE_OF = 0x65BCB0  # thiscall on the computer player, RET: the creature or 0
LEASH_STATUS = 0x4CF060  # thiscall on the creature, RET: the status of the leash on it or 0
IS_FIGHTING = 0x47B1C0  # thiscall on the creature, RET: not 0 in a fight
LIFE_GETTER = 0x402600  # thiscall, RET: the float at +0x48 in ST0
WORLD = 0xD0195C  # the pointer to the game world; its turn is at +0x205A40
WORLD_TURN = 0x205A40

# The creature
VTABLE_LIFE = 0x11C
LIFE = 0x48
NEEDS = 0x160  # +0x1C energy, +0x30 exhaustion
MIND = 0x164  # +0xF50 the desire of the plan
ENERGY = 0x1C
EXHAUSTION = 0x30
PLAN_DESIRE = 0xF50
# The leash: the status (+0x12C) holds the turn of the tie (+0x34)
STATUS_TIE = 0x12C
TIED_TURN = 0x34


class Freedom:
    COLUMNS = ["leashed", "turns_since_tied", "fighting", "life", "exhaustion", "energy", "plan_desire", "freedom"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        vtable = o.alloc(0x200)
        o.write_u32(vtable + VTABLE_LIFE, LIFE_GETTER)
        self.needs = o.alloc(0x40)
        self.mind = o.alloc(0x1000)
        self.creature = o.object(0x200, {0: vtable, NEEDS: self.needs, MIND: self.mind})
        self.tie = o.alloc(0x40)
        self.status = o.object(0x200, {STATUS_TIE: self.tie})
        self.world = o.alloc(WORLD_TURN + 0x10)
        o.write_u32(WORLD, self.world)
        self.player = o.alloc(0x40)
        o.stub(CREATURE_OF, eax=self.creature, pop=0)

    def run(self, leashed: int, turns: int, fighting: int, life: int, exhaustion: int, energy: int,
            desire: int) -> list[int]:
        o = self.o
        o.stub(LEASH_STATUS, eax=self.status if leashed else 0, pop=0)
        o.stub(IS_FIGHTING, eax=fighting, pop=0)
        # The turn of the tie is before the turn of the world, and the count can go round 2 to the power 32
        now = 0x10000000
        o.write_u32(self.world + WORLD_TURN, now)
        o.write_u32(self.tie + TIED_TURN, (now - turns) & 0xFFFFFFFF)
        o.write_u32(self.creature + LIFE, life)
        o.write_u32(self.needs + ENERGY, energy)
        o.write_u32(self.needs + EXHAUSTION, exhaustion)
        o.write_u32(self.mind + PLAN_DESIRE, desire)
        result = o.call(FREEDOM, this=self.player)
        assert result.popped == 0 and result.depth == 1, result
        return [result.st0f]


def share(rng: random.Random) -> float:
    # Most shares near the limit of 0.5, some at the ends and on the limit
    return rng.choice([rng.uniform(0.0, 1.0), rng.uniform(0.45, 0.55), 0.5, 0.0, 1.0, rng.uniform(0.0, 0.1)])


def freedom_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        leashed = 0 if rng.random() < 0.05 else 1
        turns = rng.choice([rng.randrange(0, 2000), rng.randrange(440, 460), 450, 451, rng.randrange(0, 1 << 32),
                            rng.randrange(450, 900)])
        fighting = 1 if rng.random() < 0.05 else 0
        desire = 15 if rng.random() < 0.05 else rng.randrange(0, 40)
        rows.append((leashed, turns, fighting, f2u(share(rng)), f2u(share(rng)), f2u(share(rng)), desire))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x658BC0)
    with Oracle() as oracle:
        freedom = Freedom(oracle)
        rows = [[*case, *freedom.run(*case)] for case in freedom_cases(rng)]
        n = write_pinned(out / "computer_creature_freedom.txt", "How free the creature of a computer god is",
                         ["Function: thiscall on the computer player, RET; the freedom in ST0. The creature, the",
                          "leash status and the fight are stubs; the life is the creature's float at +0x48, the",
                          "energy and the exhaustion are its needs (+0x1C, +0x30), the desire is that of its plan.",
                          "Shares and the freedom are float bits; the other columns are whole numbers."],
                         Freedom.COLUMNS, rows)
        print(f"computer_creature_freedom: {n} rows")


if __name__ == "__main__":
    main()
