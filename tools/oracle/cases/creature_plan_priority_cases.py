#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the priority of a creature's plan, in test/oracle/data:

- creature_plan_priority.txt: the priority that the game gives to the plan of a desire, from the strength of the
  desire, the usefulness of the whole thing that the desire is about (or none), the priority of the action, the
  usefulness of the thing acted on and the usefulness of the thing used (or none).

Usage: python tools/oracle/cases/creature_plan_priority_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The function (BW1W120, runblack.exe 1.20): thiscall on the creature, argument the plan, writes the priority into the
# plan. The plan is valid when it has a desire, a thing to act on and an action; the game's tables are empty in the
# oracle, so no desire needs a whole thing and no action needs a thing to use.
PLAN_PRIORITY = 0x4F1D50
MIND = 0x164
DESIRE_VALUES = 0x150
PLAN_SIZE = 0x30


class PlanPriority:
    COLUMNS = ["desire", "whole", "has_whole", "action", "acted_on", "used", "has_used", "priority"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.creature = o.alloc(0x200)
        self.mind = o.alloc(0x400)
        self.plan = o.alloc(0x40)
        o.write_u32(self.creature + MIND, self.mind)

    def run(self, desire: int, whole: int | None, action: int, acted_on: int, used: int | None) -> int:
        o = self.o
        o.write_u32(self.mind + DESIRE_VALUES, desire)
        o.write(self.plan, bytes(PLAN_SIZE))
        o.write_u32(self.plan + 0x08, 0)
        o.write_u32(self.plan + 0x0C, 1 if whole is not None else 0)
        o.write_u32(self.plan + 0x10, 1)
        o.write_u32(self.plan + 0x14, 1 if used is not None else 0)
        o.write_u32(self.plan + 0x18, 5)
        o.write_u32(self.plan + 0x1C, whole or 0)
        o.write_u32(self.plan + 0x20, acted_on)
        o.write_u32(self.plan + 0x24, used or 0)
        o.write_u32(self.plan + 0x28, action)
        o.call(PLAN_PRIORITY, this=self.creature, args=[self.plan])
        return o.read_f32_bits(self.plan + 0x2C)[0]


def cases(rng: random.Random) -> list[tuple[int, int | None, int, int, int | None]]:
    rows = []
    for _ in range(2000):
        desire = f2u(rng.uniform(0.0, 1.0))
        whole = f2u(rng.uniform(0.0, 2.0)) if rng.random() < 0.7 else None
        # Most action priorities are above the cap; some are below it or are only the novelty
        action = f2u(rng.choice([rng.uniform(0.5, 0.505), rng.uniform(0.0, 0.02), 0.01]))
        acted_on = f2u(rng.choice([rng.uniform(0.0, 2.0), 0.1, 1.6]))
        used = f2u(rng.choice([0.1, rng.uniform(0.0, 1.0)])) if rng.random() < 0.6 else None
        rows.append((desire, whole, action, acted_on, used))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261110)
    rows = []
    with Oracle() as o:
        priority = PlanPriority(o)
        for desire, whole, action, acted_on, used in cases(rng):
            result = priority.run(desire, whole, action, acted_on, used)
            rows.append((desire, whole or 0, int(whole is not None), action, acted_on, used or 0, int(used is not None),
                         result))
    write_pinned(
        out / "creature_plan_priority.txt",
        "The priority of a creature's plan",
        [
            "The game's priority of the plan of one desire. whole is the usefulness of the whole thing that the desire",
            "is about, used the usefulness of the thing used; has_whole and has_used are 0 when the plan has no such",
            "thing (the game then takes 0.1). action is the priority of the action with its novelty, acted_on the",
            "usefulness of the thing acted on.",
        ],
        PlanPriority.COLUMNS,
        rows,
    )
    print(f"{len(rows)} rows, {sum(1 for r in rows if u2f(r[7]) == 0.0)} at 0")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
