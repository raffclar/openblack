#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of one turn of a creature's body as time passes, in test/oracle/data:

- creature_body_turn.txt: from the body (age, turns lived, strength, warmth, fatness, energy, exhaustion, dehydration,
  own size and shown size), the stage of growing up, the action of the plan, whether it moves, the temperature where
  it stands and the species' numbers, the body one turn on. Nothing is carried. The game runs this turn once a game
  turn for a creature from the first stage of growing up on, at 10 turns a second.

Usage: python tools/oracle/cases/creature_body_turn_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions and data (BW1W120, runblack.exe 1.20)
BODY_TURN = 0x4EF510  # thiscall on the body, one argument: the species' row
POSITION_ON_MAP = 0x605C40  # thiscall, one argument: gives the place where the creature stands
TEMPERATURE_AT = 0x771A80  # cdecl, one argument: the temperature at a place, on the FPU
MS_PER_TURN = 0xD01A38

# The plan actions that change the pace of the body: sleep at home, rest to get better, sleep by a thing
ACTIONS = [0, 0x11, 0x3B, 0x4E, 0x15]
PHASES = [1, 2, 3, 4, 5, 13]

# Species numbers: comfort temperature, seconds a year, minutes to grow up, low energy, exhaustion rate, seconds to
# dehydrate, strength decay, energy drain, fat burn. The first rows are the shipped tables after the balance rows.
SPECIES = [
    (15.0, 3600, 1200.0, 0.2, 4e-5, 5000.0, 0.99998, 1.162e-4, 8e-5),
    (11.0, 3600, 1035.0, 0.2, 4e-5, 5000.0, 0.99998, 1.0e-4, 8e-5),
    (-5.0, 3600, 1740.0, 0.2, 4e-5, 5000.0, 0.99998, 1.3e-4, 7e-5),
    (0.0, 10, 120.0, 0.2, 1e-3, 1000.05, 0.999, 0.01, 0.001),
]


class BodyTurn:
    INPUTS = ["age", "turns", "strength", "warmth", "fatness", "energy", "exhaustion", "dehydration", "size", "shown",
              "phase", "action", "moving", "temperature", "comfort", "age_seconds", "grow_minutes", "low_energy",
              "exhaustion_rate", "dehydrate_seconds", "strength_decay", "energy_drain", "fat_burn"]
    OUTPUTS = ["age_after", "strength_after", "warmth_after", "fatness_after", "energy_after", "exhaustion_after",
               "dehydration_after", "size_after"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.body = o.alloc(0x100)
        self.creature = o.alloc(0x1300)
        self.mind = o.alloc(0x1000)
        self.model = o.alloc(0x4A00)
        self.info = o.alloc(0x400)
        self.place = o.alloc(0x20)
        o.write_u32(self.body + 0x54, self.creature)
        o.write_u32(self.body + 0x58, self.model)
        o.write_u32(self.creature + 0x160, self.body)
        o.write_u32(self.creature + 0x164, self.mind)
        o.write_u32(MS_PER_TURN, 100)
        o.stub(POSITION_ON_MAP, eax=self.place, pop=4)

    def run(self, row: tuple[int, ...]) -> tuple[int, ...]:
        o = self.o
        (age, turns, strength, warmth, fatness, energy, exhaustion, dehydration, size, shown, phase, action, moving,
         temperature, comfort, age_seconds, grow, low, rate, dehydrate, decay, drain, burn) = row
        b = self.body
        o.write_u32(b + 0x08, age)
        o.write_u32(b + 0x0C, strength)
        o.write_u32(b + 0x10, warmth)
        o.write_u32(b + 0x14, fatness)
        o.write_u32(b + 0x1C, energy)
        o.write_u32(b + 0x28, 0)
        o.write_u32(b + 0x30, exhaustion)
        o.write_u32(b + 0x34, dehydration)
        o.write_u32(b + 0x6C, size)
        o.write_u32(b + 0x70, turns)
        o.write_u32(self.model + 0x90, shown)
        # The body moves while its model walks
        o.write_u32(self.model + 0x4994, 1 if moving else 0)
        o.write_u32(self.creature + 0x1268, phase)
        o.write_u32(self.mind + 0xF60, action)
        i = self.info
        o.write_u32(i + 0x20C, comfort)
        o.write_u32(i + 0x210, age_seconds)
        o.write_u32(i + 0x214, grow)
        o.write_u32(i + 0x21C, low)
        o.write_u32(i + 0x220, rate)
        o.write_u32(i + 0x224, dehydrate)
        o.write_u32(i + 0x228, decay)
        o.write_u32(i + 0x230, drain)
        o.write_u32(i + 0x234, burn)
        o.stub(TEMPERATURE_AT, eax=0, pop=0, st0=u2f(temperature))
        o.call(BODY_TURN, this=b, args=[i])
        return (o.read_u32(b + 0x08)[0], o.read_u32(b + 0x0C)[0], o.read_u32(b + 0x10)[0], o.read_u32(b + 0x14)[0],
                o.read_u32(b + 0x1C)[0], o.read_u32(b + 0x30)[0], o.read_u32(b + 0x34)[0], o.read_u32(b + 0x6C)[0])


def cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for n in range(3000):
        comfort, age_seconds, grow, low, rate, dehydrate, decay, drain, burn = SPECIES[n % len(SPECIES)]
        period = 10 * age_seconds
        # Some turns lived end a year of its life
        turns = period * rng.randint(1, 5) if rng.random() < 0.2 else rng.randint(1, 200000)
        age = rng.randint(0, 40) if rng.random() < 0.7 else rng.randint(0, 3000)
        energy = rng.choice([rng.uniform(0.0, 1.0), rng.uniform(0.0, 0.25), rng.uniform(0.45, 0.55),
                             rng.uniform(0.95, 1.05), 0.0, 1.0])
        size = rng.choice([rng.uniform(0.0, 2.2), rng.uniform(1.95, 2.0), 0.3, 1.0, 2.0])
        shown = rng.choice([size if 0.05 <= size <= 4.0 else 1.0, rng.uniform(0.05, 2.0), 0.22])
        temperature = rng.choice([float(rng.randint(-20, 40)), rng.uniform(-20.0, 40.0), comfort])
        rows.append((age, turns, f2u(rng.uniform(0.0, 1.0)), f2u(rng.uniform(-1.0, 1.0)), f2u(rng.uniform(0.0, 1.0)),
                     f2u(energy), f2u(rng.choice([rng.uniform(0.0, 1.0), 0.0, 0.999995])),
                     f2u(rng.choice([rng.uniform(0.0, 1.0), 0.0, 0.99999])), f2u(size), f2u(shown),
                     rng.choice(PHASES), rng.choice(ACTIONS), rng.randint(0, 1), f2u(temperature), f2u(comfort),
                     age_seconds, f2u(grow), f2u(low), f2u(rate), f2u(dehydrate), f2u(decay), f2u(drain), f2u(burn)))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0xB0D1)
    with Oracle() as o:
        turn = BodyTurn(o)
        rows = [row + turn.run(row) for row in cases(rng)]
    n = write_pinned(out / "creature_body_turn.txt", "One turn of a creature's body as time passes",
                     ["Floats as float bits; age, turns, phase, action, moving and age_seconds as integers.",
                      "turns is the count of turns lived at the call, this turn included. 10 turns a second.",
                      "Nothing is carried. temperature is the temperature where the creature stands."],
                     BodyTurn.INPUTS + BodyTurn.OUTPUTS, rows)
    print(f"creature_body_turn.txt: {n} rows")


if __name__ == "__main__":
    main()
