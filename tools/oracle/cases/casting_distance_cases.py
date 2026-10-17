#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the distance and heading tests of a creature that walks to a thing, in test/oracle/data:

- creature_heads_for.txt: whether a creature at a map position with a heading heads for another map position
- map_distance.txt: the game's distance in metres between two map positions

The inputs are points in metres. The script makes them map positions as the game does (a float product by 6553.6,
truncated), so the tests give the same points to openblack's functions, which make the map positions themselves.

Usage: python tools/oracle/cases/casting_distance_cases.py [output folder]   (default: test/oracle/data)
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
IS_DIRECTION_CLOSE = 0x477550  # thiscall (creature: map position at +0x14), args (map position*, parts, heading)
DISTANCE_IN_METRES = 0x74CD70  # cdecl (map position*, map position*), returns ST0
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table the distance reads, once, as the game does at start-up
INVERSE_SQRT_TABLE = 0xDA5A10  # 1024 dwords
FIXED_PER_METRE = 0x45CCCCCD  # 6553.6f
HEADING_PARTS = 6


def to_float(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def to_fixed(metres_bits: int) -> int:
    """Metres to 16.16, a float product truncated towards 0 (the inputs stay in the int32 range)"""
    metres = struct.unpack("<f", struct.pack("<I", metres_bits))[0]
    product = to_float(metres * struct.unpack("<f", struct.pack("<I", FIXED_PER_METRE))[0])
    return int(math.trunc(product))


def write_map_position(oracle: Oracle, address: int, x_bits: int, z_bits: int) -> int:
    oracle.write(address, struct.pack("<iif", to_fixed(x_bits), to_fixed(z_bits), 0.0))
    return address


class HeadsFor:
    COLUMNS = ["creature_x", "creature_z", "point_x", "point_z", "heading", "close"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.creature = oracle.alloc(0x40)
        self.point = oracle.alloc(0x10)

    def run(self, cx: int, cz: int, px: int, pz: int, heading: int) -> int:
        o = self.o
        o.write(self.creature + 0x14, struct.pack("<iif", to_fixed(cx), to_fixed(cz), 0.0))
        point = write_map_position(o, self.point, px, pz)
        result = o.call(IS_DIRECTION_CLOSE, this=self.creature, args=[point, HEADING_PARTS, f32.of_bits(heading)])
        assert result.popped == 12, result
        return result.eax & 0xFF


def heads_for_cases(rng: random.Random) -> list[tuple[int, int, int, int, int]]:
    rows = []
    for _ in range(1500):
        cx = rng.uniform(10.0, 5110.0)
        cz = rng.uniform(10.0, 5110.0)
        reach = rng.choice([rng.uniform(0.0, 12.0), rng.uniform(5.0, 300.0)])
        angle = rng.uniform(-math.pi, math.pi)
        px = cx + reach * math.cos(angle)
        pz = cz + reach * math.sin(angle)
        heading = rng.uniform(-4.0, 8.0)
        rows.append((f2u(cx), f2u(cz), f2u(px), f2u(pz), f2u(heading)))
    # Headings near the edge of the share of a turn, and near game angle 0 where the difference does not go round
    for _ in range(500):
        cx = rng.uniform(1000.0, 4000.0)
        cz = rng.uniform(1000.0, 4000.0)
        angle = rng.uniform(-0.3, 0.3)
        px = cx + 200.0 * math.cos(angle)
        pz = cz + 200.0 * math.sin(angle)
        heading = math.pi / 2 + angle + rng.choice([-1.0, 1.0]) * rng.uniform(0.9, 1.2)
        rows.append((f2u(cx), f2u(cz), f2u(px), f2u(pz), f2u(heading)))
    return rows


class MapDistance:
    COLUMNS = ["a_x", "a_z", "b_x", "b_z", "distance"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        oracle.call(MAKE_INVERSE_SQRT_TABLE)
        assert any(oracle.read_u32(INVERSE_SQRT_TABLE, 1024))
        self.a = oracle.alloc(0x10)
        self.b = oracle.alloc(0x10)

    def run(self, ax: int, az: int, bx: int, bz: int) -> int:
        o = self.o
        result = o.call(DISTANCE_IN_METRES, args=[write_map_position(o, self.a, ax, az), write_map_position(o, self.b, bx, bz)])
        return result.st0f


def distance_cases(rng: random.Random) -> list[tuple[int, int, int, int]]:
    rows = [(f2u(0.0), f2u(0.0), f2u(0.0), f2u(0.0)), (f2u(0.0), f2u(0.0), f2u(0.1), f2u(0.0))]
    for _ in range(1500):
        ax = rng.uniform(0.0, 5120.0)
        az = rng.uniform(0.0, 5120.0)
        reach = rng.choice([rng.uniform(0.0, 1.0), rng.uniform(0.0, 50.0), rng.uniform(0.0, 3000.0)])
        angle = rng.uniform(-math.pi, math.pi)
        rows.append((f2u(ax), f2u(az), f2u(ax + reach * math.cos(angle)), f2u(az + reach * math.sin(angle))))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x477550)
    with Oracle() as oracle:
        heads = HeadsFor(oracle)
        rows = [[*case, heads.run(*case)] for case in heads_for_cases(rng)]
        n = write_pinned(out / "creature_heads_for.txt", "Whether a creature heads for a point near enough",
                         ["Function: thiscall on the creature (its map position at +0x14), args (point, 6, heading).",
                          "Inputs are float bits of metres, made map positions by a float product with 6553.6f,",
                          "truncated; close is EAX (1 when the creature heads for the point)."],
                         HeadsFor.COLUMNS, rows)
        print(f"creature_heads_for: {n} rows, {sum(r[-1] for r in rows)} close")
        distance = MapDistance(oracle)
        rows = [[*case, distance.run(*case)] for case in distance_cases(rng)]
        n = write_pinned(out / "map_distance.txt", "The game's distance in metres between two map positions",
                         ["Function: cdecl (map position, map position), result ST0 stored as a float, with the start-up table.",
                          "Inputs are float bits of metres, made map positions by a float product with 6553.6f,",
                          "truncated."],
                         MapDistance.COLUMNS, rows)
        print(f"map_distance: {n} rows")


if __name__ == "__main__":
    main()
