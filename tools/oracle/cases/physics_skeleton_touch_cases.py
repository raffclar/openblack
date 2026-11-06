#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the pair test of two physics bodies where the second body has a skeleton, in
test/oracle/data:

- physics_skeleton_touch.txt: a cube (A) against a body (B) with one to four skeleton parts. Each part is a box in its
  own space with the matrix that puts it in the world and the inverse of that matrix. The points of A are tested
  against the ellipsoids in the boxes of B, as the physics does for a creature. The game's own pair test runs as it is,
  with the ellipsoid line test, the 3D engine's normal and its inverse square root (with the table the game makes at
  start-up).

The inputs (the points of A in the world and their lengths from the centre) come from physics_touch_cases.py, so the
test makes the same body in openblack and first checks that the inputs have the same bits. The parts are given as
bits, and openblack takes them as they are. Only the results come from the game.

Usage: python tools/oracle/cases/physics_skeleton_touch_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402
import physics_touch_cases as touch  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
MAKE_INVERSE_SQRT_TABLE = 0x8411D0  # cdecl (), fills the table through the 3D engine's allocator
ENGINE_ALLOC = 0x842CE0  # cdecl (size) -> pointer
INVERSE_SQRT_TABLE_POINTER = 0xEEA394

# Fields of the body with the skeleton: the parts and the mesh (only the flags at +4 are read)
BONES = 0x168
MESH = 0x16C
MESH_FLAGS = 0x4
HAS_BONES = 0x100
# A part: box low and high corners, the matrix into the world, its inverse (rows of three, then the move)
BONE_SIZE = 0x78
BOX_MIN = 0x00
BOX_MAX = 0x0C
TO_WORLD = 0x18
TO_PART = 0x48

MAX_BONES = 4
F = np.float32


def rotation(rng: random.Random) -> list[list[float]]:
    """A random turn as three rows (each row the image of one part axis)"""
    q = [rng.gauss(0.0, 1.0) for _ in range(4)]
    n = math.sqrt(sum(v * v for v in q))
    w, x, y, z = (v / n for v in q)
    return [[1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y)],
            [2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)],
            [2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)]]


def make_bone(rng: random.Random, centre: list[np.float32], spread: float) -> dict:
    low = [rng.uniform(-1.5, -0.05) * spread for _ in range(3)]
    high = [rng.uniform(0.05, 1.5) * spread for _ in range(3)]
    if rng.random() < 0.05:
        # A flat part, which the test skips
        axis = rng.randrange(3)
        high[axis] = low[axis] + rng.choice([0.0, 0.0001, 0.00015])
    rows = rotation(rng)
    scale = rng.choice([1.0, 1.0, rng.uniform(0.5, 2.0)])
    rows = [[v * scale for v in row] for row in rows]
    move = [float(centre[i]) + rng.uniform(-1.0, 1.0) * spread for i in range(3)]
    # The inverse: the turn back (the rows' transpose over the square of the scale), then the move back
    back = [[rows[c][r] / (scale * scale) for c in range(3)] for r in range(3)]
    back_move = [-sum(move[c] * back[c][r] for c in range(3)) for r in range(3)]
    to_world = [F(v) for row in rows for v in row] + [F(v) for v in move]
    to_part = [F(v) for row in back for v in row] + [F(v) for v in back_move]
    return {"min": [F(v) for v in low], "max": [F(v) for v in high], "to_world": to_world, "to_part": to_part}


def main(argv: list[str]) -> int:
    folder = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x5CE1)
    rows = []
    with Oracle() as o:
        o.hook_alloc(ENGINE_ALLOC)
        o.call(MAKE_INVERSE_SQRT_TABLE)
        assert o.read_u32(INVERSE_SQRT_TABLE_POINTER)[0] != 0
        body_a, body_b = o.alloc(touch.SIZE), o.alloc(touch.SIZE)
        points_a, points_b = o.alloc(touch.POINT_SIZE * 8), o.alloc(touch.POINT_SIZE * 8)
        faces_a, faces_b = o.alloc(touch.FACE_SIZE * 12), o.alloc(touch.FACE_SIZE * 12)
        bones = o.alloc(BONE_SIZE * MAX_BONES)
        mesh = o.alloc(0x10)
        for index in range(480):
            small = index % 4 == 0
            half_b = F(rng.choice([0.5, 1.0, rng.uniform(0.3, 4.0)]))
            half_a = F(rng.choice([0.5, rng.uniform(0.1, 2.0)]))
            if small:
                base = [rng.uniform(-3.0, 3.0) for _ in range(3)]
            else:
                base = [rng.uniform(0.0, 5000.0), rng.uniform(-10.0, 300.0), rng.uniform(0.0, 5000.0)]
            centre_b = [F(v) for v in base]
            reach = float(half_a + half_b) * rng.choice([0.2, 0.6, 1.0, 1.4, 2.0])
            offset = [rng.uniform(-1.0, 1.0) for _ in range(3)]
            n = math.sqrt(sum(v * v for v in offset)) or 1.0
            centre_a = [F(base[i] + offset[i] / n * reach) for i in range(3)]
            m_a = touch.axes(rng)
            m_b = touch.axes(rng)
            count = rng.randint(1, MAX_BONES)
            parts = [make_bone(rng, centre_b, float(half_b)) for _ in range(count)]
            # The body's sphere holds all its parts, as a creature's does
            radius_b = F(float(half_b) * rng.uniform(2.0, 4.0))
            resting_a = rng.random() < 0.25
            resting_b = rng.random() < 0.5
            skeleton_a = rng.random() < 0.2

            a = touch.make_body(half_a, m_a, centre_a)
            b = touch.make_body(half_b, m_b, centre_b)
            b["radius"] = radius_b
            touch.write_body(o, body_a, points_a, faces_a, a, centre_a, resting_a, skeleton_a)
            touch.write_body(o, body_b, points_b, faces_b, b, centre_b, resting_b, False)
            o.write(bones, bytes(BONE_SIZE * MAX_BONES))
            for i, part in enumerate(parts):
                record = bones + BONE_SIZE * i
                o.write_f32(record + BOX_MIN, *[float(v) for v in part["min"]])
                o.write_f32(record + BOX_MAX, *[float(v) for v in part["max"]])
                o.write_f32(record + TO_WORLD, *[float(v) for v in part["to_world"]])
                o.write_f32(record + TO_PART, *[float(v) for v in part["to_part"]])
            o.write_u32(mesh + MESH_FLAGS, HAS_BONES | count)
            o.write_u32(body_b + BONES, bones)
            o.write_u32(body_b + MESH, mesh)
            o.call(touch.TOUCH_FACES, this=body_a, args=[body_b])

            inputs = ([f2u(float(half_a)), f2u(float(half_b))] +
                      [f2u(float(v)) for column in m_a for v in column] +
                      [f2u(float(v)) for column in m_b for v in column] +
                      [f2u(float(v)) for v in centre_a + centre_b] +
                      [int(resting_a), int(resting_b), int(skeleton_a), f2u(float(radius_b)), count])
            for i in range(MAX_BONES):
                if i < count:
                    part = parts[i]
                    values = part["min"] + part["max"] + part["to_world"] + part["to_part"]
                    inputs += [f2u(float(v)) for v in values]
                else:
                    inputs += [0] * 30
            inputs += [f2u(float(v)) for p in a["world"] for v in p] + [f2u(float(v)) for v in a["ahead"]]
            outputs = [o.read_u32(body_a + touch.CONTACTS, 1)[0]]
            for i in range(8):
                point = points_a + touch.POINT_SIZE * i
                other = o.read_u32(point + touch.OTHER, 1)[0]
                outputs += (o.read_u32(point + touch.CONTACT, 3) + o.read_u32(point + touch.NORMAL, 3) +
                            o.read_u32(point + touch.DEPTH, 1) + [1 if other == body_b else 0])
            outputs += [o.read(body_a + touch.TOUCHED, 1)[0], o.read(body_b + touch.TOUCHED, 1)[0],
                        1 if o.read_u32(body_a + touch.LAST_HIT, 1)[0] == body_b else 0,
                        1 if o.read_u32(body_b + touch.LAST_HIT, 1)[0] == body_a else 0]
            rows.append(inputs + outputs)
    bone_columns = []
    for i in range(MAX_BONES):
        bone_columns += [f"bone{i}min{c}" for c in "xyz"] + [f"bone{i}max{c}" for c in "xyz"]
        bone_columns += [f"bone{i}world{k}" for k in range(12)] + [f"bone{i}part{k}" for k in range(12)]
    columns = (["half_a", "half_b"] + [f"axes_a{i}" for i in range(9)] + [f"axes_b{i}" for i in range(9)] +
               ["ca_x", "ca_y", "ca_z", "cb_x", "cb_y", "cb_z", "resting_a", "resting_b", "skeleton_a", "radius_b",
                "bones"] + bone_columns +
               [f"wa{i}{c}" for i in range(8) for c in "xyz"] + [f"ahead{i}" for i in range(8)] + ["contacts"] +
               [name for i in range(8) for name in
                [f"hit{i}x", f"hit{i}y", f"hit{i}z", f"normal{i}x", f"normal{i}y", f"normal{i}z", f"depth{i}",
                 f"other{i}"]] +
               ["touched_a", "touched_b", "last_a", "last_b"])
    count = write_pinned(folder / "physics_skeleton_touch.txt",
                         "The points of one physics body against the skeleton of another",
                         ["A cube (A) against a body (B) with 1 to 4 skeleton parts. Axes are glm columns. A part's",
                          "matrices are the game's rows of three (part x, y and z axes in the world), then the move.",
                          "The depths start at zero. other is 1 when the point hits B."], columns, rows)
    print(f"physics_skeleton_touch.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
