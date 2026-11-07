#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of one physics body step (one of the twenty steps of a game turn), in test/oracle/data:

- physics_step.txt: the body's state before the step (spin, turn, place, speed, force, torque, mass, inverse inertia,
  radius, spin kept, contacts, rest counter, resting and touched) and after it (the result, spin, turn, place, speed
  and the stored speed). The game's step runs as it is, with the small vector and matrix helpers it calls. The squared
  spin limit is a global that the game's start-up code makes; the script calls that start-up function first.
- physics_inertia.txt: a body's points, mass, radius and drag, and the inertia, its inverse and the drag that the
  game's set-up of the moment of inertia makes from them (with the engine's matrix inverse that it calls).

Usage: python tools/oracle/cases/physics_step_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
SET_UP_INERTIA = 0x7FB880  # thiscall, no arguments
BODY_STEP = 0x7FE260  # thiscall, no arguments: returns 0 none, 1 moved, 2 stopped, 3 knocked, 4 delete
SPIN_LIMIT_START = 0x7FE240  # start-up code: the squared spin limit global

# Fields of the body (the physics object)
INVERSE_INERTIA = 0x38  # 3 x 3, rows
SPIN = 0x68  # angular momentum
DYNAMIC = 0x74
AXES = 0x7C  # 3 x 3, rows
CENTRE = 0xA0
VELOCITY = 0xDC
SPEED = 0xF4
FORCE = 0x104
TORQUE = 0x110
MASS = 0x134
SPIN_KEPT = 0x148
RADIUS = 0x150
CONTACTS = 0x158
REST_COUNTER = 0x170
RESTING = 0x174
TOUCHED = 0x176
INERTIA = 0x08  # 3 x 3, rows
DRAG = 0x14C
POINT_COUNT = 0x154
POINTS = 0x15C
POINT_SIZE = 0x50
POINT_LOCAL = 0x10
SIZE = 0x180
MAX_POINTS = 16


def to_f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def vec(rng: random.Random, size: float) -> list[float]:
    return [to_f32(rng.uniform(-size, size)) for _ in range(3)]


def axes(rng: random.Random) -> list[float]:
    """A random turn (rows), from a random unit quaternion, rounded to floats"""
    q = [rng.gauss(0.0, 1.0) for _ in range(4)]
    n = math.sqrt(sum(v * v for v in q))
    w, x, y, z = (v / n for v in q)
    rows = [1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
            2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]
    return [to_f32(v) for v in rows]


def inverse_inertia(rng: random.Random) -> list[float]:
    """A symmetric matrix with a large diagonal, as the inverse of a body's inertia"""
    d = [rng.uniform(0.05, 3.0) for _ in range(3)]
    a, b, c = (rng.uniform(-0.3, 0.3) * min(d) for _ in range(3))
    return [to_f32(v) for v in [d[0], a, b, a, d[1], c, b, c, d[2]]]


def make_case(rng: random.Random, kind: str) -> dict:
    mass = to_f32(rng.choice([rng.uniform(0.01, 5.0), rng.uniform(5.0, 3000.0)]))
    radius = to_f32(rng.uniform(0.2, 12.0))
    case = {
        "inverse": inverse_inertia(rng),
        "axes": axes(rng),
        "centre": [to_f32(rng.uniform(0.0, 5000.0)), to_f32(rng.uniform(-10.0, 300.0)), to_f32(rng.uniform(0.0, 5000.0))],
        "mass": mass,
        "radius": radius,
        "kept": to_f32(rng.choice([1.0, rng.uniform(0.99, 1.0), rng.uniform(0.2, 1.0)])),
        "contacts": rng.choice([0, 1, 4, 12]),
        "counter": rng.choice([0, 1, 30, 14999, 15000, 15001, rng.randrange(0, 40000)]),
        "resting": rng.random() < 0.3,
        "touched": rng.random() < 0.6,
        "dynamic": 1,
    }
    if kind == "flying":
        case["spin"] = vec(rng, 50.0)
        case["velocity"] = vec(rng, 60.0)
        case["force"] = vec(rng, 2000.0 * mass)
        case["torque"] = vec(rng, 3000.0)
    elif kind == "near_rest":
        # Small enough that one or more of the rest tests pass
        scale = rng.choice([0.05, 0.3, 1.0, 2.0, 4.0])
        case["contacts"] = rng.choice([1, 3, 8])
        case["spin"] = vec(rng, scale * 0.3)
        case["velocity"] = vec(rng, scale * 0.6)
        case["force"] = vec(rng, scale * mass)
        case["torque"] = vec(rng, scale * radius * radius * mass * 0.5)
    elif kind == "fast":
        case["spin"] = vec(rng, 4000.0)
        case["velocity"] = vec(rng, 400.0)
        case["force"] = vec(rng, 50000.0 * mass)
        case["torque"] = vec(rng, 50000.0)
    elif kind == "deep":
        case["centre"][1] = to_f32(-radius * rng.choice([3.9, 4.0, 4.1, 8.0]))
        case["spin"] = vec(rng, 5.0)
        case["velocity"] = vec(rng, 5.0)
        case["force"] = vec(rng, 50.0)
        case["torque"] = vec(rng, 50.0)
    else:  # still: no spin at all
        case["spin"] = [0.0, 0.0, 0.0]
        case["torque"] = [0.0, 0.0, 0.0]
        case["velocity"] = vec(rng, 3.0)
        case["force"] = vec(rng, 30.0)
    return case


def inertia_rows(o: Oracle, rng: random.Random) -> list[list[int]]:
    rows = []
    body = o.alloc(SIZE)
    points = o.alloc(POINT_SIZE * MAX_POINTS)
    for case in range(300):
        count = rng.choice([1, 2, 3, 8, 12, 16, rng.randrange(1, MAX_POINTS + 1)])
        size = rng.choice([0.3, 1.0, 4.0, 20.0])
        locals_ = [vec(rng, size) for _ in range(count)]
        if case % 10 == 0:
            locals_ = [[v[0], 0.0, v[2]] for v in locals_]  # flat: a determinant near zero
        mass = to_f32(rng.choice([rng.uniform(0.01, 5.0), rng.uniform(5.0, 3000.0)]))
        radius = to_f32(rng.uniform(0.2, 30.0))
        drag = to_f32(rng.uniform(0.0, 4.0))
        o.write(body, bytes(SIZE))
        o.write(points, bytes(POINT_SIZE * MAX_POINTS))
        for index, local in enumerate(locals_):
            o.write_f32(points + POINT_SIZE * index + POINT_LOCAL, *local)
        o.write_u32(body + DYNAMIC, 1)
        o.write_f32(body + MASS, mass)
        o.write_f32(body + RADIUS, radius)
        o.write_f32(body + DRAG, drag)
        o.write_u32(body + POINT_COUNT, count)
        o.write_u32(body + POINTS, points)
        o.call(SET_UP_INERTIA, this=body)
        padded = locals_ + [[0.0, 0.0, 0.0]] * (MAX_POINTS - count)
        rows.append([count, f2u(mass), f2u(radius), f2u(drag)] + [f2u(v) for p in padded for v in p] +
                    o.read_u32(body + INERTIA, 9) + o.read_u32(body + INVERSE_INERTIA, 9) + o.read_u32(body + DRAG, 1))
    return rows


def main(argv: list[str]) -> int:
    folder = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0xB0D1)
    kinds = ["flying"] * 160 + ["near_rest"] * 200 + ["fast"] * 40 + ["deep"] * 20 + ["still"] * 20
    rows = []
    with Oracle() as o:
        o.call(SPIN_LIMIT_START)
        body = o.alloc(SIZE)
        for kind in kinds:
            case = make_case(rng, kind)
            o.write(body, bytes(SIZE))
            o.write_f32(body + INVERSE_INERTIA, *case["inverse"])
            o.write_f32(body + SPIN, *case["spin"])
            o.write_u32(body + DYNAMIC, case["dynamic"])
            o.write_f32(body + AXES, *case["axes"])
            o.write_f32(body + CENTRE, *case["centre"])
            o.write_f32(body + VELOCITY, *case["velocity"])
            o.write_f32(body + FORCE, *case["force"])
            o.write_f32(body + TORQUE, *case["torque"])
            o.write_f32(body + MASS, case["mass"])
            o.write_f32(body + SPIN_KEPT, case["kept"])
            o.write_f32(body + RADIUS, case["radius"])
            o.write_u32(body + CONTACTS, case["contacts"])
            o.write_u32(body + REST_COUNTER, case["counter"])
            o.write(body + RESTING, bytes([1 if case["resting"] else 0, 0, 1 if case["touched"] else 0]))
            result = o.call(BODY_STEP, this=body)
            inputs = ([f2u(v) for v in case["inverse"] + case["spin"] + case["axes"] + case["centre"] +
                       case["velocity"] + case["force"] + case["torque"]] +
                      [f2u(case["mass"]), f2u(case["kept"]), f2u(case["radius"]), case["contacts"], case["counter"],
                       int(case["resting"]), int(case["touched"])])
            outputs = ([result.eax & 0xFF] + o.read_u32(body + SPIN, 3) + o.read_u32(body + AXES, 9) +
                       o.read_u32(body + CENTRE, 3) + o.read_u32(body + VELOCITY, 3) + o.read_u32(body + SPEED, 1))
            rows.append(inputs + outputs)
    columns = ([f"inv{i}" for i in range(9)] + ["sx", "sy", "sz"] + [f"axes{i}" for i in range(9)] +
               ["cx", "cy", "cz", "vx", "vy", "vz", "fx", "fy", "fz", "tx", "ty", "tz", "mass", "kept", "radius",
                "contacts", "counter", "resting", "touched", "result", "out_sx", "out_sy", "out_sz"] +
               [f"out_axes{i}" for i in range(9)] +
               ["out_cx", "out_cy", "out_cz", "out_vx", "out_vy", "out_vz", "out_speed"])
    count = write_pinned(folder / "physics_step.txt", "One physics body step",
                         ["Matrices are rows. Spin is the angular momentum. The result: 0 none, 1 moved, 2 stopped,",
                          "3 knocked, 4 delete."], columns, rows)
    print(f"physics_step.txt: {count} rows")
    with Oracle() as o:
        rows = inertia_rows(o, random.Random(0x1DE7))
    columns = (["count", "mass", "radius", "drag"] + [f"p{i}{a}" for i in range(MAX_POINTS) for a in "xyz"] +
               [f"inertia{i}" for i in range(9)] + [f"inverse{i}" for i in range(9)] + ["out_drag"])
    count = write_pinned(folder / "physics_inertia.txt", "The moment of inertia of a physics body",
                         ["Matrices are rows. Unused points are zero."], columns, rows)
    print(f"physics_inertia.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
