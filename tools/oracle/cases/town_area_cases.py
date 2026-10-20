#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the ground a town covers, in test/oracle/data:

- town_area_widen.txt: a building or a field at a place, with a radius, widens the box
- town_area_centre.txt: the middle of the box
- town_area_radius.txt: the radius of the town from the box

Usage: python tools/oracle/cases/town_area_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
WIDEN = 0x73AC90  # thiscall (town), arg the structure; RET 4
CENTRE = 0x73AE10  # thiscall (town), arg the place to fill; RET 4
RADIUS = 0x73D6E0  # thiscall (town), plain RET; the radius on ST0
# A function this path never calls, stubbed as the target of the structure's radius call
RADIUS_TARGET = 0x7DB790

# Town fields: the least corner (x, z) and the greatest corner (x, z)
MIN_X = 0x728
MIN_Z = 0x72C
MAX_X = 0x734
MAX_Z = 0x738
# Structure fields: its place (x, z)
POS_X = 0x14
POS_Z = 0x18

AREA = ["min_x", "min_z", "max_x", "max_z"]
WIDEN_COLUMNS = AREA + ["x", "z", "radius"] + ["new_" + k for k in AREA]
CENTRE_COLUMNS = AREA + ["x", "z"]
RADIUS_COLUMNS = AREA + ["radius"]
EMPTIED = [0x7FFFFFFF, 0x7FFFFFFF, 0, 0]


def fixed(metres: float) -> int:
    return int(metres * 6553.6) & 0xFFFFFFFF


def random_area(rng: random.Random) -> list[int]:
    if rng.random() < 0.2:
        return list(EMPTIED)
    x = rng.uniform(0.0, 5000.0)
    z = rng.uniform(0.0, 5000.0)
    w = rng.uniform(0.0, 200.0)
    h = rng.uniform(0.0, 200.0)
    return [fixed(x), fixed(z), fixed(x + w), fixed(z + h)]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261020)
    with Oracle() as o:
        town = o.alloc(0x1000)
        thing = o.alloc(0x100)
        vtable = o.alloc(0x100)
        o.write_u32(thing, vtable)
        o.write_u32(vtable + 0x60, RADIUS_TARGET)
        place = o.alloc(0x10)

        def set_area(area: list[int]) -> None:
            for offset, value in zip((MIN_X, MIN_Z, MAX_X, MAX_Z), area):
                o.write_u32(town + offset, value & 0xFFFFFFFF)

        def get_area() -> list[int]:
            return [o.read_u32(town + offset)[0] for offset in (MIN_X, MIN_Z, MAX_X, MAX_Z)]

        rows = []
        cases = [(EMPTIED, fixed(100.0), fixed(200.0), 5.0), ([0, 0, 0, 0], fixed(100.0), fixed(200.0), 5.0),
                 ([fixed(95.0), fixed(195.0), fixed(105.0), fixed(205.0)], fixed(140.0), fixed(210.0), 10.0),
                 (EMPTIED, fixed(3.0), fixed(3.0), 5.0), (EMPTIED, fixed(2047.9), fixed(1.5), 0.0)]
        for _ in range(600):
            area = random_area(rng)
            cases.append((area, fixed(rng.uniform(0.0, 5200.0)), fixed(rng.uniform(0.0, 5200.0)),
                          rng.choice([rng.uniform(0.0, 60.0), float(rng.randrange(0, 40)), 0.5 * rng.randrange(0, 80)])))
        for area, x, z, radius in cases:
            set_area(area)
            o.write_u32(thing + POS_X, x)
            o.write_u32(thing + POS_Z, z)
            o.stub(RADIUS_TARGET, st0=radius)
            r = o.call(WIDEN, this=town, args=[thing])
            assert r.popped == 4 and r.depth == 0, r
            rows.append(list(area) + [x, z, f2u(radius)] + get_area())
        write_pinned(out / "town_area_widen.txt", "A building widens the ground a town covers (the game's own function)",
                     ["The corners and the place are 16.16 map words; the radius is float bits."], WIDEN_COLUMNS, rows)

        rows = []
        areas = [list(EMPTIED), [0, 0, 0, 0], [fixed(95.0), fixed(195.0), fixed(150.0), fixed(220.0)]]
        areas += [random_area(rng) for _ in range(400)]
        for area in areas:
            set_area(area)
            r = o.call(CENTRE, this=town, args=[place])
            assert r.popped == 4 and r.depth == 0, r
            rows.append(list(area) + o.read_u32(place, 2))
        write_pinned(out / "town_area_centre.txt", "The middle of the ground a town covers (the game's own function)",
                     ["All values are 16.16 map words."], CENTRE_COLUMNS, rows)

        rows = []
        for area in areas:
            set_area(area)
            r = o.call(RADIUS, this=town)
            assert r.popped == 0 and r.depth == 1, r
            rows.append(list(area) + [r.st0f])
        write_pinned(out / "town_area_radius.txt", "The radius of a town (the game's own function)",
                     ["The corners are 16.16 map words; the radius is float bits."], RADIUS_COLUMNS, rows)
    print("done")


if __name__ == "__main__":
    main()
