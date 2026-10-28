#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the places of a workshop's yard in test/oracle/data:

- workshop_yard.txt: the place of the yard (0 to 3), the special point matrix of the model at that place's point, and the
  matrix of the workshop's 3D object in; the map place (x and z, the altitude) and the angles about x, y and z out.
  The game's own function for a place of the yard runs: it picks the model's special point of the place, moves it
  through the object's matrix, takes the angles of the moved matrix and keeps the map place with no altitude.

Usage: python tools/oracle/cases/workshop_yard_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
YARD_PLACE = 0x779A60  # thiscall (workshop) (&map place, place, &x angle, &y angle, &z angle); RET 0x14
EXTRA_POINT_MATRIX = 0x7FA050  # the 3D object's matrix of a special point of its mesh (vtable +0x1C8)
LAND_ALTITUDE = 0x803090  # thiscall (map place) -> ST0: stubbed to 0
RETURNS_ZERO = 0x63B290  # a function that nothing calls, stubbed to give 0: the "follows the land" vtable slot

WORKSHOP_OBJECT = 0x40  # the workshop's 3D object
OBJECT_MATRIX = 0x14  # three rows and a place
OBJECT_MESH = 0x7C
MESH_FLAGS = 0x04
MESH_BLOCKS = 0x48
HAS_EXTRA_POINTS = 0x100000
SLOT_EXTRA_POINT = 0x1C8
SLOT_FOLLOWS_LAND = 0x1F4
POINT_COUNT = 9  # the workshop models hold nine special points

COLUMNS = (["place"] + [f"p{i}" for i in range(12)] + [f"b{i}" for i in range(12)]
           + ["cx", "cz", "calt", "ax", "ay", "az"])


def turn_rows(rng: random.Random, tilt: float) -> list[float]:
    """Three rows of a turn about y by a random angle, then leaned a little about x and z"""
    yaw = rng.uniform(-math.pi, math.pi)
    lean_x = rng.uniform(-tilt, tilt)
    lean_z = rng.uniform(-tilt, tilt)
    cy, sy = math.cos(yaw), math.sin(yaw)
    cx, sx = math.cos(lean_x), math.sin(lean_x)
    cz, sz = math.cos(lean_z), math.sin(lean_z)
    # Rows of the turn about z, then x, then y
    rows = [[cz, sz, 0.0], [-sz, cz, 0.0], [0.0, 0.0, 1.0]]
    rows = [[r[0], r[1] * cx + r[2] * sx, -r[1] * sx + r[2] * cx] for r in rows]
    rows = [[r[0] * cy - r[2] * sy, r[1], r[0] * sy + r[2] * cy] for r in rows]
    return [value for row in rows for value in row]


def yard_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x5CAF)
    o = oracle
    o.stub(LAND_ALTITUDE, st0=0.0)
    o.stub(RETURNS_ZERO, eax=0)
    vtable = o.alloc(0x200)
    o.write_u32(vtable + SLOT_EXTRA_POINT, EXTRA_POINT_MATRIX)
    o.write_u32(vtable + SLOT_FOLLOWS_LAND, RETURNS_ZERO)
    points = o.alloc(0x30 * POINT_COUNT)
    block = o.alloc(0x10)
    o.write_u32(block + 0x4, POINT_COUNT, points)
    mesh = o.alloc(0x60)
    o.write_u32(mesh + MESH_FLAGS, HAS_EXTRA_POINTS)
    o.write_u32(mesh + MESH_BLOCKS, block)
    thing = o.alloc(0x100)
    o.write_u32(thing, vtable)
    o.write_u32(thing + OBJECT_MESH, mesh)
    workshop = o.alloc(0x100)
    o.write_u32(workshop + WORKSHOP_OBJECT, thing)
    out = o.alloc(0x10)
    angles = o.alloc(0x10)
    rows = []
    for index in range(1200):
        place = index % 4
        point = [f2u(v) for v in turn_rows(rng, 0.0 if index % 3 else 0.2)]
        point += [f2u(rng.uniform(-30.0, 30.0)), f2u(rng.uniform(-2.0, 5.0)), f2u(rng.uniform(-30.0, 30.0))]
        scale = rng.choice([1.0, 1.0, rng.uniform(0.5, 2.0)])
        building = [f2u(v * scale) for v in turn_rows(rng, 0.0 if index % 2 else 0.1)]
        building += [f2u(rng.uniform(0.0, 5000.0)), f2u(rng.uniform(0.0, 200.0)), f2u(rng.uniform(0.0, 5000.0))]
        # The place of the yard picks its special point: 6, 7 and 8 for the three places, else the place itself
        chosen = 6 + place if place < 3 else place
        for slot in range(POINT_COUNT):
            o.write_u32(points + 0x30 * slot, *([f2u(float(slot))] * 12))
        o.write_u32(points + 0x30 * chosen, *point)
        o.write_u32(thing + OBJECT_MATRIX, *building)
        o.write_u32(out, 0, 0, 0)
        o.write_u32(angles, 0, 0, 0)
        o.call(YARD_PLACE, this=workshop, args=[out, place, angles, angles + 4, angles + 8])
        rows.append([place] + point + building + o.read_u32(out, 3) + o.read_u32(angles, 3))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as o:
        yards = yard_cases(o)
    write_pinned(out / "workshop_yard.txt", "The places of a workshop's yard",
                 ["the place, the special point's matrix and the 3D object's matrix in (three rows, then the place);",
                  "the map place (x, z, altitude) and the angles about x, y and z out"], COLUMNS, yards)
    print(f"workshop_yard {len(yards)}")


if __name__ == "__main__":
    main()
