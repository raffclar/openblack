#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the place of a held thing in the turn, in test/oracle/data:

- held_place.txt: each turn the game puts each thing in a hand at the reported point of the hand. The columns are the
  point (x, y, z), the ground height that the land gives, the map place of the thing (x, z in 16.16 and its altitude
  above the ground) and the map place at which the game asked the land for its ground height.

The land's height function is a small piece of code that keeps the map place that it gets and gives the chosen ground
height, so the data also shows where the game asks for the height. The game asks for it at x * 65536 * 0.1, not at the
x of the thing (x * 6553.6). The two products are the same float: 65536 * 0.1 as a float is 6553.6 as a float, and a
product by 65536 is exact. The data shows it.

Usage: python tools/oracle/cases/held_place_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
PROCESS = 0x5FB190  # thiscall on the magic hand, arg (interface status), RET 4
GROUND = 0x803090  # fastcall (ECX = map place), RET; the float result on the FPU
VALIDATE = 0x5FB130  # thiscall on the magic hand, arg (interface status), RET 4
ANSWER_ONE = 0x6508F0  # a function that becomes "return 1", for the two virtual calls of the held thing

# The magic hand
HAND_COUNT = 0x14  # byte: the count of held things
HAND_THINGS = 0x18  # the held things
# The interface status
STATUS_POINT = 0xC8  # the reported point of the hand: x, y, z
# The held thing
THING_X = 0x14
THING_Z = 0x18
THING_ALTITUDE = 0x1C
VIRTUAL_ANSWERS = (0x2C, 0x804)  # both must give 1, else the hand validates its things


class HeldPlace:
    COLUMNS = ["x", "y", "z", "ground", "place_x", "place_z", "altitude", "ground_x", "ground_z"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        # What the height function keeps (x, z) and gives (the ground height)
        self.seen = o.alloc(16)
        code = (b"\x8B\x01" + b"\xA3" + struct.pack("<I", self.seen)  # mov eax,[ecx]; mov [seen],eax
                + b"\x8B\x41\x04" + b"\xA3" + struct.pack("<I", self.seen + 4)  # mov eax,[ecx+4]; mov [seen+4],eax
                + b"\xD9\x05" + struct.pack("<I", self.seen + 8)  # fld dword [seen+8]
                + b"\xC3")  # ret
        o.write(GROUND, code)
        o.stub(ANSWER_ONE, eax=1)
        o.stub(VALIDATE, pop=4)
        table = o.alloc(0x808)
        for slot in VIRTUAL_ANSWERS:
            o.write_u32(table + slot, ANSWER_ONE)
        self.thing = o.alloc(0x40)
        o.write_u32(self.thing, table)
        self.hand = o.alloc(0x40)
        o.write(self.hand + HAND_COUNT, b"\x01")
        o.write_u32(self.hand + HAND_THINGS, self.thing)
        self.status = o.alloc(0x100)

    def run(self, x: int, y: int, z: int, ground: int) -> list[int]:
        o = self.o
        o.write_u32(self.status + STATUS_POINT, x, y, z)
        o.write_u32(self.seen, 0xDEADBEEF, 0xDEADBEEF, ground)
        o.write_u32(self.thing + THING_X, 0x11111111, 0x22222222, 0x33333333)
        result = o.call(PROCESS, this=self.hand, args=[self.status])
        assert result.popped == 4 and result.depth == 0, result
        place = o.read_u32(self.thing + THING_X, 3)
        seen = o.read_u32(self.seen, 2)
        return [x, y, z, ground] + place + seen


def bits(value: float) -> int:
    return f2u(value)


def near_cells(rng: random.Random) -> list[float]:
    """Values close to the edge of a 10 m cell, where one unit of 16.16 changes the cell"""
    values = []
    for _ in range(150):
        edge = struct.unpack("<f", struct.pack("<f", 10.0 * rng.randrange(0, 512)))[0]
        step = rng.choice((-1, 1)) * rng.randrange(1, 64)
        bits_of_edge = struct.unpack("<I", struct.pack("<f", edge))[0] if edge > 0.0 else 1
        values.append(struct.unpack("<f", struct.pack("<I", max(1, bits_of_edge + step)))[0])
    return values


def cases(rng: random.Random) -> list[tuple[int, int, int, int]]:
    rows = []
    chosen = [(0.0, 0.0, 0.0, 0.0), (-0.0, -0.0, -0.0, 0.0), (10.0, 5.0, 10.0, 5.0), (2560.5, 120.25, 1999.75, 33.5),
              (5119.99, 400.0, 5119.99, 12.0), (-3.25, 7.0, 2.5, 1.5), (0.0001, 0.5, 0.0002, 0.75)]
    rows += [tuple(bits(v) for v in row) for row in chosen]
    for _ in range(1500):
        rows.append((bits(rng.uniform(-100.0, 5200.0)), bits(rng.uniform(-20.0, 600.0)), bits(rng.uniform(-100.0, 5200.0)),
                     bits(rng.uniform(0.0, 250.0))))
    edges = near_cells(rng)
    for value in edges:
        rows.append((bits(value), bits(rng.uniform(0.0, 300.0)), bits(rng.choice(edges)), bits(rng.uniform(0.0, 250.0))))
    # The smallest values. A hand point is always over the land, so no row has a 16.16 product out of the int32 range
    for x in (0x00000001, 0x80000001):
        rows.append((x, bits(1.0), x, bits(0.0)))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261028)
    with Oracle() as oracle:
        held = HeldPlace(oracle)
        rows = [held.run(*row) for row in cases(rng)]
    count = write_pinned(out / "held_place.txt", "The place of a held thing in the turn",
                         ["The game puts a held thing at the reported point of the hand: x and z to 16.16, the altitude "
                          "above the ground that the land gives at the point times 65536 times 0.1.",
                          "The ground height is a chosen value; ground_x and ground_z are the place that the game asked "
                          "the land for."], HeldPlace.COLUMNS, rows)
    print(f"held_place.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
