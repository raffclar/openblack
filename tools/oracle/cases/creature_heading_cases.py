#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the game's object headings, in test/oracle/data:

- heading_wrap.txt: the game's one-step wrap of an angle into the half turns each side of 0
- creature_facing.txt: whether a creature at a map position with a heading faces another map position (the turn to face
  stage of a creature that turns to a thing). The game returns 2 when the creature faces the thing or stands on top
  of it, and 0 when it must turn.

Usage: python tools/oracle/cases/creature_heading_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
WRAP = 0x7FAAF0  # cdecl (float angle), returns ST0
TURN_TO_FACE_STAGE = 0x5013B0  # thiscall on the creature, no arguments, returns EAX
START_TURN = 0x4F00E0  # thiscall (heading), RET 4: starts a turn; stubbed
TURNS_PER_SECOND = 0xD01A38  # the game's turn length divisor that the facing branch reads
FIXED_PER_METRE = 0x45CCCCCD  # 6553.6f


def to_float(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def to_fixed(metres_bits: int) -> int:
    """Metres to 16.16, a float product truncated towards 0 (the inputs stay in the int32 range)"""
    metres = struct.unpack("<f", struct.pack("<I", metres_bits))[0]
    product = to_float(metres * struct.unpack("<f", struct.pack("<I", FIXED_PER_METRE))[0])
    return int(math.trunc(product))


def wrap_cases(rng: random.Random) -> list[tuple[int]]:
    pi = to_float(math.pi)
    rows = [(f2u(v),) for v in (0.0, -0.0, pi, -pi, 2 * pi, -2 * pi, 3 * pi, -3 * pi)]
    rows += [(f2u(pi) + d,) for d in (-1, 1)] + [(f2u(-pi) + d,) for d in (-1, 1)]
    rows += [(f2u(rng.uniform(-10.0, 10.0)),) for _ in range(1500)]
    return rows


class Facing:
    COLUMNS = ["creature_x", "creature_z", "point_x", "point_z", "heading", "result"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.stub(START_TURN, eax=0, pop=4)
        o.write_u32(TURNS_PER_SECOND, 10)
        self.creature = o.alloc(0x200)
        self.body = o.alloc(0x20)
        self.model = o.alloc(0x49A0)
        self.mind = o.alloc(0x1C00)
        self.sub_action = o.alloc(0x40)
        self.thing = o.alloc(0x40)
        o.write_u32(self.creature + 0x160, self.body)
        o.write_u32(self.creature + 0x164, self.mind)
        o.write_u32(self.body + 0x58, self.model)
        o.write_u32(self.mind + 0xFDC, self.sub_action)
        o.write_u32(self.sub_action + 0x30, self.thing)

    def run(self, cx: int, cz: int, px: int, pz: int, heading: int) -> int:
        o = self.o
        o.write(self.creature + 0x14, struct.pack("<iif", to_fixed(cx), to_fixed(cz), 0.0))
        o.write(self.thing + 0x14, struct.pack("<iif", to_fixed(px), to_fixed(pz), 0.0))
        o.write_f32(self.model + 0x84, f32.of_bits(heading))
        result = o.call(TURN_TO_FACE_STAGE, this=self.creature)
        return result.eax


def facing_cases(rng: random.Random) -> list[tuple[int, int, int, int, int]]:
    rows = []
    for _ in range(1500):
        cx = rng.uniform(10.0, 5110.0)
        cz = rng.uniform(10.0, 5110.0)
        reach = rng.choice([rng.uniform(0.0, 0.2), rng.uniform(0.0, 12.0), rng.uniform(5.0, 300.0)])
        angle = rng.uniform(-math.pi, math.pi)
        px = cx + reach * math.cos(angle)
        pz = cz + reach * math.sin(angle)
        heading = rng.uniform(-4.0, 4.0)
        rows.append((f2u(cx), f2u(cz), f2u(px), f2u(pz), f2u(heading)))
    # Headings near the edge of the facing angle, and across the half turn where the difference wraps
    for _ in range(1000):
        cx = rng.uniform(1000.0, 4000.0)
        cz = rng.uniform(1000.0, 4000.0)
        reach = rng.uniform(1.0, 200.0)
        angle = rng.uniform(-math.pi, math.pi)
        px = cx + reach * math.cos(angle)
        pz = cz + reach * math.sin(angle)
        # The game's heading of this way: 0 to -z, a quarter turn to +x
        towards = math.atan2(math.cos(angle), -math.sin(angle))
        edge = rng.choice([-1.0, 1.0]) * rng.uniform(0.38, 0.41)
        turn = rng.choice([0.0, 2 * math.pi, -2 * math.pi])
        heading = towards + edge + turn
        if -4.5 < heading < 4.5:
            rows.append((f2u(cx), f2u(cz), f2u(px), f2u(pz), f2u(heading)))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x5013B0)
    with Oracle() as oracle:
        rows = [[*case, oracle.call(WRAP, args=[f32.of_bits(case[0])]).st0f] for case in wrap_cases(rng)]
        n = write_pinned(out / "heading_wrap.txt", "The game's one-step wrap of an angle",
                         ["Function: cdecl (float angle), result ST0 stored as a float."],
                         ["angle", "wrapped"], rows)
        print(f"heading_wrap: {n} rows")
        facing = Facing(oracle)
        rows = [[*case, facing.run(*case)] for case in facing_cases(rng)]
        n = write_pinned(out / "creature_facing.txt", "Whether a creature faces a thing that it turns to",
                         ["Function: the turn to face stage, thiscall on the creature (map position at +0x14), its",
                          "model's heading at +0x84, the thing's map position at +0x14. The turn is stubbed.",
                          "Inputs are float bits of metres, made map positions by a float product with 6553.6f,",
                          "truncated; result is EAX: 2 faces or on top, 0 must turn."],
                         Facing.COLUMNS, rows)
        print(f"creature_facing: {n} rows, {sum(1 for r in rows if r[-1] == 2)} facing")


if __name__ == "__main__":
    main()
