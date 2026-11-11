#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how a creature pays for its miracles, in test/oracle/data:

- creature_spell_pay.txt: the chants it has, the energy that chants cost it, whether it may cast, and what paying
  gives and does to its energy and exhaustion

Usage: python tools/oracle/cases/creature_spell_pay_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
CHANTS_TO_ENERGY = 0x4F8260  # thiscall (creature), 1 stack arg (chants), returns ST0
MOST_CHANTS = 0x4F82A0  # thiscall (creature), no args, returns ST0
CAN_PAY = 0x4F82F0  # thiscall (creature), 1 stack arg (magic type), returns EAX 0/1
MAINTAIN = 0x4F8350  # thiscall (creature), 2 stack args (spell, amount), returns ST0

COST_TO_CREATE = 0xCC66A8  # the cost to make each magic type, rows of 0x11C bytes
ENERGY_SHARE = 0xCA9CDC  # each magic type's share of the energy, rows of 0x70 bytes
MAGIC_TYPE = 5

NAN = 0x7FC00000
POS_INF = 0x7F800000


class Pay:
    COLUMNS = ["size", "strength", "energy", "exhaustion", "chants_per_energy", "energy_floor", "size_factor",
               "amount", "cost", "share", "most", "to_energy", "can_pay", "paid", "energy_after", "exhaustion_after"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.creature = oracle.alloc(0x200)
        self.info = oracle.alloc(0x400)
        self.physical = oracle.alloc(0x100)
        self.body3d = oracle.alloc(0x100)
        self.spell = oracle.alloc(0x200)
        oracle.write_u32(self.creature + 0x28, self.info)
        oracle.write_u32(self.creature + 0x160, self.physical)
        oracle.write_u32(self.physical + 0x58, self.body3d)
        oracle.write_u32(self.spell + 0xB4, MAGIC_TYPE)

    def run(self, row: list[int]) -> list[int]:
        o = self.o
        size, strength, energy, exhaustion, rate, floor, factor, amount, cost, share = row

        def lay_out() -> None:
            o.write_u32(self.body3d + 0x90, size)
            o.write_u32(self.physical + 0x0C, strength)
            o.write_u32(self.physical + 0x1C, energy)
            o.write_u32(self.physical + 0x30, exhaustion)
            o.write_u32(self.info + 0x294, rate)
            o.write_u32(self.info + 0x298, floor)
            o.write_u32(self.info + 0x390, factor)
            o.write_u32(COST_TO_CREATE + MAGIC_TYPE * 0x11C, cost)
            o.write_u32(ENERGY_SHARE + MAGIC_TYPE * 0x70, share)

        lay_out()
        most = o.call(MOST_CHANTS, this=self.creature)
        to_energy = o.call(CHANTS_TO_ENERGY, this=self.creature, args=[f32.of_bits(amount)])
        assert to_energy.popped == 4, to_energy
        can_pay = o.call(CAN_PAY, this=self.creature, args=[MAGIC_TYPE])
        assert can_pay.popped == 4, can_pay
        paid = o.call(MAINTAIN, this=self.creature, args=[self.spell, f32.of_bits(amount)])
        assert paid.popped == 8, paid
        return [most.st0f, to_energy.st0f, can_pay.eax & 0xFF, paid.st0f, o.read_u32(self.physical + 0x1C)[0],
                o.read_u32(self.physical + 0x30)[0]]


def cases(rng: random.Random) -> list[list[int]]:
    def row(size=1.0, strength=0.5, energy=1.0, exhaustion=0.0, rate=36000.0, floor=0.005, factor=1.0,
            amount=9000.0, cost=9000.0, share=0.3) -> list[int]:
        return [v if isinstance(v, int) else f2u(v)
                for v in (size, strength, energy, exhaustion, rate, floor, factor, amount, cost, share)]

    rows = [
        row(),
        row(amount=0.0), row(amount=-5.0), row(amount=NAN), row(amount=POS_INF),
        row(energy=0.005), row(energy=0.0), row(energy=NAN),
        row(amount=1e9), row(amount=200000.0, strength=1.0),
        row(exhaustion=0.84), row(exhaustion=0.85), row(exhaustion=NAN), row(cost=NAN), row(cost=0.0),
        row(share=NAN), row(share=-1.0), row(share=10.0), row(size=0.3), row(size=2.0, energy=1.9),
        row(size=NAN), row(rate=0.0), row(rate=NAN), row(factor=0.0), row(strength=-1.5),
        row(energy=1.0, size=0.5, share=-2.0),
    ]
    for _ in range(400):
        rows.append(row(size=rng.uniform(0.1, 2.0), strength=rng.uniform(0.0, 1.0), energy=rng.uniform(-0.1, 1.5),
                        exhaustion=rng.uniform(0.0, 1.0), rate=rng.choice([36000.0, rng.uniform(1000.0, 60000.0)]),
                        floor=rng.uniform(0.0, 0.1), factor=rng.uniform(0.0, 2.0),
                        amount=rng.choice([rng.uniform(0.0, 300000.0), rng.uniform(0.0, 5000.0)]),
                        cost=rng.uniform(0.0, 60000.0), share=rng.uniform(0.0, 1.0)))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x4F8350)
    with Oracle() as oracle:
        pay = Pay(oracle)
        rows = [r + pay.run(r) for r in cases(rng)]
        n = write_pinned(out / "creature_spell_pay.txt", "How a creature pays for the miracles it casts",
                         ["Functions: the most chants it has, the energy that chants cost it, whether it may cast",
                          "(the cost to make the miracle against its exhaustion) and what paying an amount gives.",
                          "Inputs and results are float bits, except can_pay (0 or 1).",
                          "energy_after and exhaustion_after are its energy and exhaustion after it paid."],
                         Pay.COLUMNS, rows)
        print(f"creature_spell_pay: {n} rows")


if __name__ == "__main__":
    main()
