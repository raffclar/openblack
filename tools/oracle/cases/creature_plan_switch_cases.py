#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the change of a creature's plan, in test/oracle/data:

- creature_plan_switch.txt: whether the creature changes from the plan that it carries out to the best plan of the
  round, from the two priorities.
- creature_manifest_state.txt: the value of the source of the desire to show how the creature feels, from the sum of
  its desires.

Usage: python tools/oracle/cases/creature_plan_switch_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
SWITCH = 0x4F0500  # thiscall on the creature, no argument: 1 when it takes up the best plan
PLAN_IS_VALID = 0x4F12E0  # thiscall on the plan, no argument
ADOPT = 0x4F0810  # thiscall on the creature, argument: the plan
MANIFEST_SOURCE = 0x4DEB90  # thiscall on the creature, no argument, ST0: the source's value

# The creature and its mind (from the disassembly)
MIND = 0x164
BEST = 0xE98  # the desire of the best plan of the round
PLANS = 0x718  # one plan of 0x30 bytes for each desire
PLAN_SIZE = 0x30
PLAN_PRIORITY = 0x2C
CURRENT_PRIORITY = 0xF74
DESIRE_SUM = 0x65C  # the sum of the desires, as the mind reaches it

# Special values: zero, the smallest denormal, the largest float, infinity and a quiet NaN
SPECIAL = [0x00000000, 0x00000001, 0x00800000, 0x7F7FFFFF, 0x7F800000, 0x7FC00000, 0x80000000]


class Switch:
    COLUMNS = ["best", "current", "switches"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        # Every plan is valid, and the creature takes up nothing: only the test of the two priorities is left
        o.stub(PLAN_IS_VALID, eax=1, pop=0)
        o.stub(ADOPT, eax=0, pop=4)
        self.creature = o.alloc(0x200)
        self.mind = o.alloc(0x1000)
        o.write_u32(self.creature + MIND, self.mind)
        o.write_u32(self.mind + BEST, 0)

    def run(self, best: int, current: int) -> int:
        o = self.o
        o.write_u32(self.mind + PLANS + PLAN_PRIORITY, best)
        o.write_u32(self.mind + CURRENT_PRIORITY, current)
        return o.call(SWITCH, this=self.creature).eax


class Manifest:
    COLUMNS = ["sum", "value"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.creature = o.alloc(0x200)
        self.mind = o.alloc(0x1000)
        o.write_u32(self.creature + MIND, self.mind)

    def run(self, total: int) -> int:
        o = self.o
        o.write_u32(self.mind + DESIRE_SUM, total)
        return o.call(MANIFEST_SOURCE, this=self.creature).st0f


def switch_cases(rng: random.Random) -> list[tuple[int, int]]:
    rows = [(a, b) for a in SPECIAL for b in SPECIAL]
    for _ in range(1500):
        current = rng.choice([rng.uniform(0.0, 1.0), rng.uniform(0.0, 1e-3), 10.0 ** rng.uniform(-40, 1), 0.25, 0.3])
        kind = rng.random()
        if kind < 0.4:
            # Near the line of twice the current priority
            best = current * 2.0 * (1.0 + rng.uniform(-1e-6, 1e-6))
        elif kind < 0.6:
            best = current * rng.uniform(0.0, 4.0)
        else:
            best = rng.choice([rng.uniform(0.0, 1.0), 10.0 ** rng.uniform(-40, 1)])
        rows.append((f2u(best), f2u(current)))
    # Exactly twice, and the floats next to twice
    for _ in range(300):
        current = f2u(rng.uniform(1e-30, 1.0))
        twice = f2u(2.0 * u2f(current))
        for delta in (-1, 0, 1):
            rows.append((twice + delta, current))
    return rows


def manifest_cases(rng: random.Random) -> list[int]:
    rows = list(SPECIAL) + [0xFF800000, 0xBF800000, 0x40400000, 0x40400001, 0x403FFFFF]
    for _ in range(1500):
        rows.append(f2u(rng.choice([rng.uniform(-0.5, 4.0), rng.uniform(2.9, 3.1), rng.uniform(0.0, 40.0),
                                    10.0 ** rng.uniform(-44, 1)])))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261111)
    with Oracle() as o:
        switch = Switch(o)
        switches = [(best, current, switch.run(best, current)) for best, current in switch_cases(rng)]
        manifest = Manifest(o)
        values = [(total, manifest.run(total)) for total in manifest_cases(rng)]
    write_pinned(
        out / "creature_plan_switch.txt",
        "Whether a creature takes up the best plan of the round",
        [
            "The game's test at the end of a round of the planner, for a valid best plan: switches is 1 when the",
            "creature takes up the best plan (best priority) in place of the plan that it carries out (current",
            "priority), else 0.",
        ],
        Switch.COLUMNS,
        switches,
    )
    write_pinned(
        out / "creature_manifest_state.txt",
        "The source of the desire to show how a creature feels",
        ["The game's value of the source, from the sum of the creature's desires (sum)."],
        Manifest.COLUMNS,
        values,
    )
    print(f"{len(switches)} switch rows ({sum(r[2] for r in switches)} switch), {len(values)} manifest rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
