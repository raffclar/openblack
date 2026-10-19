#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the walks that the area that holds a creature refuses, in test/oracle/data:

- creature_area_walk.txt: a walk of a new agenda to a point, for a creature that an area holds. The game adds the walk
  to the agenda, or refuses it when the point is further than the radius from the centre of the area. The centre of
  the area is a map position; the game changes it to metres, and the column gives those metres as the game makes them.

Usage: python tools/oracle/cases/creature_area_walk_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from fractions import Fraction
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions and data (BW1W120, runblack.exe 1.20)
ADD_SUB_ACTION = 0x4FF240  # thiscall on the agenda (kind, argument, look, face, six dwords), RET 0x28
IS_CONFINED = 0x47CCE0  # thiscall on the creature, returns EAX; stubbed: the area holds the creature
ALTITUDE = 0x803090  # the land's altitude at a map position, returns ST0; stubbed to 0
OPERATOR_DELETE = 0x7AEE98  # cdecl; stubbed: the argument stays in the arena
POINT_ARGUMENT_TABLE = 0x8CEFD8  # the virtual table of a point argument
METRES_PER_FIXED = 0x8AA3A4  # the float that changes a map position to metres
MOVE_TO_POS = 8


def round_float(value: Fraction) -> int:
    """One rounding to the nearest float, ties to even, of an exact value (normal floats only)"""
    if value == 0:
        return 0
    sign = 0x80000000 if value < 0 else 0
    value = abs(value)
    exponent = value.numerator.bit_length() - value.denominator.bit_length()
    while Fraction(2) ** exponent > value:
        exponent -= 1
    while Fraction(2) ** (exponent + 1) <= value:
        exponent += 1
    scaled = value / Fraction(2) ** (exponent - 23)
    mantissa = int(scaled)
    rest = scaled - mantissa
    if rest > Fraction(1, 2) or (rest == Fraction(1, 2) and mantissa & 1):
        mantissa += 1
    if mantissa == 1 << 24:
        mantissa >>= 1
        exponent += 1
    return sign | ((exponent + 127) << 23) | (mantissa & 0x7FFFFF)


class AreaWalk:
    COLUMNS = ["centre_x", "centre_z", "radius", "point_x", "point_z", "refused"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.stub(IS_CONFINED, eax=1, pop=0)
        o.stub(ALTITUDE, eax=0, pop=0, st0=0.0)
        o.stub(OPERATOR_DELETE, eax=0, pop=0)
        self.scale = Fraction(u2f(o.read_u32(METRES_PER_FIXED)[0]))
        self.agenda = o.alloc(0xC50)
        self.creature = o.alloc(0x1300)
        self.mind = o.alloc(0x1000)
        self.argument = o.alloc(0x20)
        o.write_u32(self.agenda + 0xC28, self.creature)
        o.write_u32(self.creature + 0x164, self.mind)

    def run(self, fixed_x: int, fixed_z: int, radius: int, px: int, pz: int) -> tuple[int, int, int]:
        o = self.o
        o.write_u32(self.agenda + 0x14, 0)
        o.write_u32(self.agenda + 0xC2C, 0)
        o.write(self.creature + 0x11A8, struct.pack("<iif", fixed_x, fixed_z, 0.0))
        o.write_u32(self.creature + 0x11B4, radius)
        o.write_u32(self.argument, POINT_ARGUMENT_TABLE)
        o.write(self.argument + 4, struct.pack("<III", px, 0, pz))
        o.call(ADD_SUB_ACTION, this=self.agenda, args=[MOVE_TO_POS, self.argument, 0, 0, 0, 0, 0, 0, 0, 0])
        refused = o.read_u32(self.agenda + 0xC2C)[0]
        added = o.read_u32(self.agenda + 0x14)[0]
        assert (refused != 0) == (added == 0), (refused, added)
        centre_x = round_float(Fraction(fixed_x) * self.scale)
        centre_z = round_float(Fraction(fixed_z) * self.scale)
        return centre_x, centre_z, refused


def cases(rng: random.Random) -> list[tuple[int, int, int, int, int]]:
    rows = []
    for _ in range(1500):
        fixed_x = rng.randrange(1 << 16, 5100 << 16)
        fixed_z = rng.randrange(1 << 16, 5100 << 16)
        radius = rng.choice([rng.uniform(1.0, 20.0), rng.uniform(20.0, 400.0)])
        # Points near the edge of the area, where the order and rounding of the steps decide
        reach = radius * rng.choice([rng.uniform(0.0, 2.0), rng.uniform(0.999, 1.001), 1.0])
        dx, dz = rng.uniform(-1.0, 1.0), rng.uniform(-1.0, 1.0)
        length = (dx * dx + dz * dz) ** 0.5 or 1.0
        cx = fixed_x / 6553.6
        cz = fixed_z / 6553.6
        rows.append((fixed_x, fixed_z, f2u(radius), f2u(cx + reach * dx / length), f2u(cz + reach * dz / length)))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261018)
    inputs = cases(rng)
    rows = []
    with Oracle() as o:
        walk = AreaWalk(o)
        for fixed_x, fixed_z, radius, px, pz in inputs:
            centre_x, centre_z, refused = walk.run(fixed_x, fixed_z, radius, px, pz)
            rows.append((centre_x, centre_z, radius, px, pz, refused))
    write_pinned(
        out / "creature_area_walk.txt",
        "The walks that the area that holds a creature refuses",
        [
            "The game's agenda adds a walk to a point (MOVE_TO_POS) for a creature that an area holds, with no script",
            "control and no walk back. refused is 1 when the game does not add the walk. The centre columns are the",
            "metres that the game makes from the map position of the centre (one rounding of the map position times",
            "the game's metres per map unit).",
        ],
        AreaWalk.COLUMNS,
        rows,
    )
    print(f"{len(rows)} rows, {sum(1 for r in rows if r[5])} refused")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
