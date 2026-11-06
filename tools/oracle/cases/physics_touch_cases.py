#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the point-against-faces test of two physics bodies, in test/oracle/data:

- physics_touch.txt: two cubes, each with its turn, its centre and its half side. The points of the first body (A)
  are tested against the faces of the second body (B), with the search for the nearest face behind each point. The
  game's own pair test runs as it is, with the face search and the small vector helpers that it calls.

The inputs that the game reads (the points and face normals in the world, the length of each point from the centre)
are made here in numpy float32 in the order that openblack's body set-up uses (glm's matrix times a vector, then the
centre; the normal from the cross product of two edges, made unit with the squares added from z to x). The test
makes the same bodies in openblack and first checks that its inputs have the same bits. Only the results come from
the game.

Usage: python tools/oracle/cases/physics_touch_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
TOUCH_FACES = 0x7FDA60  # thiscall (this = the body with the points), one argument (the body with the faces), RET 4

# Fields of the body (the physics object)
LAST_HIT = 0x78
CENTRE = 0xA0
RADIUS = 0x150
POINT_COUNT = 0x154
CONTACTS = 0x158
POINTS = 0x15C
FACE_COUNT = 0x160
FACES = 0x164
SKELETON = 0x168
RESTING = 0x174
TOUCHED = 0x176
SIZE = 0x180
# Fields of a point
POINT_SIZE = 0x50
AHEAD_LENGTH = 0x20
DEPTH = 0x24
CONTACT = 0x28
WORLD = 0x34
NORMAL = 0x40
OTHER = 0x4C
# Fields of a face
FACE_SIZE = 0x24
FACE_NORMAL = 0x18

F = np.float32

# The cube of the tests: the same corners and faces as the openblack test
CUBE_FACES = [(0, 3, 2), (0, 2, 1), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
              (3, 7, 6), (3, 6, 2), (0, 4, 7), (0, 7, 3), (1, 2, 6), (1, 6, 5)]


def cube_points(half: np.float32) -> list[list[np.float32]]:
    points = []
    for i in range(8):
        x = half if ((i & 1) ^ ((i >> 1) & 1)) != 0 else -half
        y = half if (i & 2) != 0 else -half
        z = half if (i & 4) != 0 else -half
        points.append([F(x), F(y), F(z)])
    return points


def axes(rng: random.Random) -> list[list[np.float32]]:
    """A random turn, as three columns (glm's order), from a random unit quaternion, rounded to floats"""
    q = [rng.gauss(0.0, 1.0) for _ in range(4)]
    n = math.sqrt(sum(v * v for v in q))
    w, x, y, z = (v / n for v in q)
    rows = [[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
            [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]]
    return [[F(rows[r][c]) for r in range(3)] for c in range(3)]


def turned(m: list[list[np.float32]], v: list[np.float32]) -> list[np.float32]:
    """glm's matrix times a vector: each part adds the columns from the first to the last"""
    return [F(F(F(m[0][i] * v[0]) + F(m[1][i] * v[1])) + F(m[2][i] * v[2])) for i in range(3)]


def add(a, b):
    return [F(a[i] + b[i]) for i in range(3)]


def sub(a, b):
    return [F(a[i] - b[i]) for i in range(3)]


def cross(a, b):
    return [F(F(a[1] * b[2]) - F(b[1] * a[2])), F(F(a[2] * b[0]) - F(b[2] * a[0])), F(F(a[0] * b[1]) - F(b[0] * a[1]))]


def length_zyx(v) -> np.float32:
    return F(np.sqrt(F(F(F(v[2] * v[2]) + F(v[1] * v[1])) + F(v[0] * v[0]))))


def normalise_or_keep(v):
    if v[0] == 0 and v[1] == 0 and v[2] == 0:
        return v
    share = F(F(1.0) / length_zyx(v))
    return [F(p * share) for p in v]


def make_body(half: np.float32, m, centre):
    local = cube_points(half)
    world = [add(turned(m, p), centre) for p in local]
    normals = []
    for a, b, c in CUBE_FACES:
        n = normalise_or_keep(cross(sub(local[b], local[a]), sub(local[c], local[a])))
        normals.append(turned(m, n))
    return {"local": local, "world": world, "normals": normals, "ahead": [length_zyx(p) for p in local],
            "radius": F(np.sqrt(F(F(F(half * half) + F(half * half)) + F(half * half))))}


def write_body(o: Oracle, body: int, points: int, faces: int, b: dict, centre, resting: bool, skeleton: bool) -> None:
    o.write(body, bytes(SIZE))
    o.write(points, bytes(POINT_SIZE * 8))
    o.write(faces, bytes(FACE_SIZE * 12))
    o.write_f32(body + CENTRE, *[float(v) for v in centre])
    o.write_f32(body + RADIUS, float(b["radius"]))
    o.write_u32(body + POINT_COUNT, 8)
    o.write_u32(body + POINTS, points)
    o.write_u32(body + FACE_COUNT, 12)
    o.write_u32(body + FACES, faces)
    o.write_u32(body + SKELETON, 1 if skeleton else 0)
    o.write(body + RESTING, bytes([1 if resting else 0]))
    for i in range(8):
        o.write_f32(points + POINT_SIZE * i + AHEAD_LENGTH, float(b["ahead"][i]))
        o.write_f32(points + POINT_SIZE * i + WORLD, *[float(v) for v in b["world"][i]])
    for i, (ia, ib, ic) in enumerate(CUBE_FACES):
        o.write_u32(faces + FACE_SIZE * i, ia, ib, ic)
        o.write_f32(faces + FACE_SIZE * i + FACE_NORMAL, *[float(v) for v in b["normals"][i]])


def make_case(rng: random.Random, kind: str) -> dict:
    half_b = F(rng.choice([0.5, 1.0, rng.uniform(0.2, 6.0)]))
    half_a = F(rng.choice([0.5, rng.uniform(0.1, 3.0)]))
    base = [rng.uniform(0.0, 5000.0), rng.uniform(-10.0, 300.0), rng.uniform(0.0, 5000.0)]
    if kind == "small":
        base = [rng.uniform(-3.0, 3.0) for _ in range(3)]
    centre_b = [F(v) for v in base]
    # A near B: the centres from just touching to deep inside
    reach = float(half_a + half_b) * rng.choice([0.3, 0.8, 1.0, 1.2, 1.6, 2.0])
    offset = [rng.uniform(-1.0, 1.0) for _ in range(3)]
    n = math.sqrt(sum(v * v for v in offset)) or 1.0
    centre_a = [F(base[i] + offset[i] / n * reach) for i in range(3)]
    if kind == "flat":
        # Faces of B that line up with the world axes
        m_b = [[F(1.0), F(0.0), F(0.0)], [F(0.0), F(1.0), F(0.0)], [F(0.0), F(0.0), F(1.0)]]
    else:
        m_b = axes(rng)
    m_a = axes(rng)
    return {"half_a": half_a, "half_b": half_b, "axes_a": m_a, "axes_b": m_b, "centre_a": centre_a,
            "centre_b": centre_b, "resting_a": rng.random() < 0.25, "resting_b": rng.random() < 0.5,
            "skeleton_a": rng.random() < 0.2}


def main(argv: list[str]) -> int:
    folder = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x70C4)
    kinds = ["world"] * 260 + ["small"] * 100 + ["flat"] * 60
    rows = []
    with Oracle() as o:
        body_a, body_b = o.alloc(SIZE), o.alloc(SIZE)
        points_a, points_b = o.alloc(POINT_SIZE * 8), o.alloc(POINT_SIZE * 8)
        faces_a, faces_b = o.alloc(FACE_SIZE * 12), o.alloc(FACE_SIZE * 12)
        for kind in kinds:
            case = make_case(rng, kind)
            a = make_body(case["half_a"], case["axes_a"], case["centre_a"])
            b = make_body(case["half_b"], case["axes_b"], case["centre_b"])
            write_body(o, body_a, points_a, faces_a, a, case["centre_a"], case["resting_a"], case["skeleton_a"])
            write_body(o, body_b, points_b, faces_b, b, case["centre_b"], case["resting_b"], False)
            o.call(TOUCH_FACES, this=body_a, args=[body_b])
            inputs = ([f2u(float(case["half_a"])), f2u(float(case["half_b"]))] +
                      [f2u(float(v)) for column in case["axes_a"] for v in column] +
                      [f2u(float(v)) for column in case["axes_b"] for v in column] +
                      [f2u(float(v)) for v in case["centre_a"] + case["centre_b"]] +
                      [int(case["resting_a"]), int(case["resting_b"]), int(case["skeleton_a"])] +
                      [f2u(float(v)) for p in a["world"] for v in p] + [f2u(float(v)) for v in a["ahead"]] +
                      [f2u(float(v)) for p in b["world"] for v in p] + [f2u(float(v)) for n in b["normals"] for v in n] +
                      [f2u(float(b["radius"]))])
            outputs = [o.read_u32(body_a + CONTACTS, 1)[0]]
            for i in range(8):
                point = points_a + POINT_SIZE * i
                other = o.read_u32(point + OTHER, 1)[0]
                outputs += (o.read_u32(point + CONTACT, 3) + o.read_u32(point + NORMAL, 3) + o.read_u32(point + DEPTH, 1) +
                            [1 if other == body_b else 0])
            touched_a = o.read(body_a + TOUCHED, 1)[0]
            touched_b = o.read(body_b + TOUCHED, 1)[0]
            outputs += [touched_a, touched_b, 1 if o.read_u32(body_a + LAST_HIT, 1)[0] == body_b else 0,
                        1 if o.read_u32(body_b + LAST_HIT, 1)[0] == body_a else 0]
            rows.append(inputs + outputs)
    columns = (["half_a", "half_b"] + [f"axes_a{i}" for i in range(9)] + [f"axes_b{i}" for i in range(9)] +
               ["ca_x", "ca_y", "ca_z", "cb_x", "cb_y", "cb_z", "resting_a", "resting_b", "skeleton_a"] +
               [f"wa{i}{c}" for i in range(8) for c in "xyz"] + [f"ahead{i}" for i in range(8)] +
               [f"wb{i}{c}" for i in range(8) for c in "xyz"] + [f"nb{i}{c}" for i in range(12) for c in "xyz"] +
               ["radius_b", "contacts"] +
               [name for i in range(8) for name in
                [f"hit{i}x", f"hit{i}y", f"hit{i}z", f"normal{i}x", f"normal{i}y", f"normal{i}z", f"depth{i}",
                 f"other{i}"]] +
               ["touched_a", "touched_b", "last_a", "last_b"])
    count = write_pinned(folder / "physics_touch.txt", "The points of one physics body against the faces of another",
                         ["Two cubes. Axes are glm columns. The points of A are tested against the faces of B; the",
                          "depths start at zero. other is 1 when the point hits B."], columns, rows)
    print(f"physics_touch.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
