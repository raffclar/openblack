#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the sixth glm order lane in test/oracle/data:

- tumble.txt: a tumbling particle's turn in one step (its speed, the axis it turns about, the turned rows)
- orient_sprite.txt: a sprite's roll from its smoothed velocity as the camera sees it (the first step sets the rate)
- initial_spin.txt: a shield particle's spin about its vertical, slowing over a time
- vapour_turn.txt: a shield dome patch: turned out from the centre in its first step, then turned as its parent moves
- matrix_products.txt: the 3D engine's two matrix products (a matrix times a second one, and a second one put in front)
- axis_turn.txt: the 3D engine's turn about an axis by an angle (a quaternion made into a matrix)
- hierarchy_points.txt: a point of a particle hierarchy moved into the world and a point of the world moved into it

Usage: python tools/oracle/cases/glm_order_6_cases.py [output folder]   (default: test/oracle/data)
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
TUMBLE = 0x6A6200  # thiscall (rule: restrict +0x20, tumble speed +0x24, most +0x28), (atom)
ORIENT_SPRITE = 0x69A790  # thiscall (rule: smoothing +0x20, upward part +0x24), (atom)
INITIAL_SPIN = 0x69E490  # thiscall (rule: scale +0x20, most +0x24, time to fade +0x28), (atom)
VAPOUR_END = 0x6A39E0  # thiscall (rule: scale provider +0x20), (atom)
PRODUCT = 0x7FAFF0  # fastcall (ECX matrix, in and out; EDX the second matrix)
PRE_PRODUCT = 0x7FAE60  # fastcall (ECX matrix, in and out; EDX the matrix put in front)
AXIS_TURN = 0x57D1D0  # cdecl (angle, axis, out)
LOCAL_TO_GLOBAL = 0x6751D0  # thiscall (collection), (point, in and out)
GLOBAL_TO_LOCAL = 0x675410  # thiscall (collection), (point, in and out)
DT = 0xD4E0EC  # the step time, float
CAMERA_AXES = 0xEA1D28  # the camera's 3x3 turn, rows

# The particle records
ATOM_SIZE = 0x130
ATOM_DATA = 0x24  # the list of the rules' data
ATOM_VELOCITY = 0x34
ATOM_BIRTH = 0x40
ATOM_MATRIX = 0x44  # three rows and a place
ATOM_BASE_SCALE = 0x74
ATOM_RULE_SCALE = 0x78
ATOM_STRETCH = 0x7C
ATOM_POSITION = 0x80
ATOM_COLLECTION = 0x120
COLLECTION_EFFECT = 0x30
COLLECTION_FLAGS = 0x38
COLLECTION_PARENT = 0x3C
COLLECTION_HIERARCHY = 0x4C
EFFECT_TIME = 0x70
DATA_RULE = 0x1C


def rand_float(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


def turn_rows(rng: random.Random, wobble: float) -> list[float]:
    """Three rows near a turn: a random turn about a random axis, then each part moved a little"""
    axis = [rng.uniform(-1.0, 1.0) for _ in range(3)]
    length = math.sqrt(sum(a * a for a in axis)) or 1.0
    x, y, z = (a / length for a in axis)
    angle = rng.uniform(-math.pi, math.pi)
    c, s = math.cos(angle), math.sin(angle)
    t = 1.0 - c
    rows = [t * x * x + c, t * x * y + s * z, t * x * z - s * y,
            t * x * y - s * z, t * y * y + c, t * y * z + s * x,
            t * x * z + s * y, t * y * z - s * x, t * z * z + c]
    return [value + rng.uniform(-wobble, wobble) for value in rows]


def tumble_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x7B1E)
    o = oracle
    rule = o.alloc(0x40)
    atom = o.alloc(ATOM_SIZE)
    rows = []
    for index in range(1500):
        restrict = index % 3 == 0
        speed = rand_float(rng, -3.0, 3.0)
        most = rand_float(rng, 0.0, 4.0)
        velocity = [rand_float(rng, -40.0, 40.0) for _ in range(3)]
        if index % 7 == 0:
            velocity[rng.randrange(3)] = 0
        matrix = [f2u(value) for value in turn_rows(rng, 0.2)] + [rand_float(rng, -5.0, 5.0) for _ in range(3)]
        dt = rng.choice([f2u(0.1), f2u(1.0 / 30.0), rand_float(rng, 0.001, 0.2)])
        o.write(rule + 0x20, bytes([1 if restrict else 0, 0, 0, 0]))
        o.write_u32(rule + 0x24, speed, most)
        o.write_u32(atom + ATOM_VELOCITY, *velocity)
        o.write_u32(atom + ATOM_MATRIX, *matrix)
        o.write_u32(DT, dt)
        o.call(TUMBLE, this=rule, args=[atom])
        rows.append([1 if restrict else 0, speed, most, dt] + velocity + matrix[:9]
                    + o.read_f32_bits(atom + ATOM_MATRIX, 9))
    return rows


def orient_sprite_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x0517)
    o = oracle
    rule = o.alloc(0x40)
    atom = o.alloc(ATOM_SIZE)
    node = o.alloc(0x40)
    o.write_u32(atom + ATOM_DATA, node)
    o.write_u32(node + DATA_RULE, rule)
    rows = []
    for index in range(1500):
        first = index % 4 == 0
        smoothing = rand_float(rng, 0.0, 0.99)
        upward = rand_float(rng, -2.0, 2.0)
        velocity = [rand_float(rng, -30.0, 30.0) for _ in range(3)]
        smoothed = [rand_float(rng, -30.0, 30.0) for _ in range(3)]
        rate = rand_float(rng, 0.0, 50.0)
        camera = [f2u(value) for value in turn_rows(rng, 0.0)]
        dt = rng.choice([f2u(0.1), f2u(1.0 / 30.0), rand_float(rng, 0.001, 0.2)])
        o.write_u32(rule + 0x20, smoothing, upward)
        o.write_u32(atom + ATOM_VELOCITY, *velocity)
        o.write(node + 0x20, bytes([1 if first else 0, 0, 0, 0]))
        o.write_u32(node + 0x24, *smoothed, rate)
        o.write_u32(CAMERA_AXES, *camera)
        o.write_u32(DT, dt)
        o.call(ORIENT_SPRITE, this=rule, args=[atom])
        rows.append([1 if first else 0, smoothing, upward, dt] + velocity + smoothed + [rate] + camera
                    + o.read_u32(node + 0x24, 4) + o.read_f32_bits(atom + ATOM_MATRIX, 9))
    return rows


def initial_spin_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x5914)
    o = oracle
    rule = o.alloc(0x40)
    atom = o.alloc(ATOM_SIZE)
    node = o.alloc(0x40)
    collection = o.alloc(0x60)
    effect = o.alloc(0x80)
    o.write_u32(atom + ATOM_DATA, node)
    o.write_u32(atom + ATOM_COLLECTION, collection)
    o.write_u32(collection + COLLECTION_EFFECT, effect)
    o.write_u32(node + DATA_RULE, rule)
    rows = []
    for _ in range(1500):
        spin = rand_float(rng, -6.0, 6.0)
        fade = rand_float(rng, 0.1, 4.0)
        age = rng.choice([f2u(0.0), rand_float(rng, -1.0, 5.0)])
        matrix = [f2u(value) for value in turn_rows(rng, 0.2)]
        dt = rng.choice([f2u(0.1), f2u(1.0 / 30.0), rand_float(rng, 0.001, 0.2)])
        o.write_u32(rule + 0x20, f2u(1.0), f2u(10.0), fade)
        o.write_u32(node + 0x20, spin)
        o.write(node + 0x24, bytes(4))
        o.write_u32(atom + ATOM_BIRTH, 0)
        o.write_u32(effect + EFFECT_TIME, age)
        o.write_u32(atom + ATOM_MATRIX, *matrix)
        o.write_u32(DT, dt)
        o.call(INITIAL_SPIN, this=rule, args=[atom])
        rows.append([spin, fade, age, dt] + matrix + o.read_f32_bits(atom + ATOM_MATRIX, 9))
    return rows


def vapour_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x7A90)
    o = oracle
    rule = o.alloc(0x40)
    provider = o.alloc(0x20)
    atom = o.alloc(ATOM_SIZE)
    parent = o.alloc(ATOM_SIZE)
    node = o.alloc(0x40)
    collection = o.alloc(0x60)
    effect = o.alloc(0x80)
    o.write_u32(rule + 0x20, provider)
    o.write_u32(provider + 0xC, f2u(1.0))
    o.write_u32(atom + ATOM_DATA, node)
    o.write_u32(atom + ATOM_COLLECTION, collection)
    o.write_u32(collection + COLLECTION_EFFECT, effect)
    o.write_u32(collection + COLLECTION_PARENT, parent)
    o.write_u32(node + DATA_RULE, rule)
    rows = []
    for index in range(1500):
        first = index % 3 == 0
        position = [rand_float(rng, -30.0, 30.0) for _ in range(3)]
        velocity = [rand_float(rng, -60.0, 60.0) for _ in range(3)]
        if index % 11 == 5:
            velocity = [0, 0, 0]
        matrix = [f2u(value) for value in turn_rows(rng, 0.2)]
        dt = rng.choice([f2u(0.1), f2u(1.0 / 30.0), rand_float(rng, 0.001, 0.2)])
        o.write(node + 0x20, bytes([1 if first else 0, 0, 0, 0]))
        o.write_u32(parent + ATOM_POSITION, *position)
        o.write_u32(parent + ATOM_VELOCITY, *velocity)
        o.write_u32(atom + ATOM_MATRIX, *matrix, 0, 0, 0)
        o.write_u32(effect + EFFECT_TIME, 0)
        o.write_u32(atom + ATOM_BIRTH, 0)
        o.write_u32(DT, dt)
        o.call(VAPOUR_END, this=rule, args=[atom])
        rows.append([1 if first else 0, dt] + position + velocity + matrix + o.read_f32_bits(atom + ATOM_MATRIX, 9))
    return rows


def product_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x9A0D)
    o = oracle
    first = o.alloc(0x30)
    second = o.alloc(0x30)
    rows = []
    for _ in range(800):
        a = [f2u(v * rng.uniform(0.1, 20.0)) for v in turn_rows(rng, 0.3)] + [rand_float(rng, -500.0, 500.0) for _ in range(3)]
        b = [f2u(v * rng.uniform(0.1, 20.0)) for v in turn_rows(rng, 0.3)] + [rand_float(rng, -500.0, 500.0) for _ in range(3)]
        o.write_u32(first, *a)
        o.write_u32(second, *b)
        o.call(PRODUCT, ecx=first, edx=second)
        after = o.read_u32(first, 12)
        o.write_u32(first, *b)
        o.write_u32(second, *a)
        o.call(PRE_PRODUCT, ecx=first, edx=second)
        before = o.read_u32(first, 12)
        rows.append(a + b + after + before)
    return rows


def axis_turn_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0xA715)
    o = oracle
    axis = o.alloc(0x10)
    out = o.alloc(0x30)
    rows = []
    for index in range(1500):
        values = [rand_float(rng, -3.0, 3.0) for _ in range(3)]
        if index % 13 == 0:
            values = [0, 0, 0]
        angle = rand_float(rng, -4.0, 4.0)
        o.write_u32(axis, *values)
        o.call(AXIS_TURN, args=[angle, axis, out])
        rows.append(values + [angle] + o.read_f32_bits(out, 12))
    return rows


def hierarchy_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x41E7)
    o = oracle
    levels = 3
    collections = [o.alloc(0x60) for _ in range(levels + 1)]
    atoms = [o.alloc(ATOM_SIZE) for _ in range(levels)]
    point = o.alloc(0x10)
    # Collection 0 holds the point; atom k sits in collection k + 1, and collection k's parent is atom k
    for k in range(levels):
        o.write_u32(collections[k] + COLLECTION_PARENT, atoms[k])
        o.write_u32(atoms[k] + ATOM_COLLECTION, collections[k + 1])
    rows = []
    for _ in range(1200):
        flags = [rng.choice([0, 1, 1]) for _ in range(levels)]
        frames = []
        for k in range(levels):
            matrix = [f2u(v) for v in turn_rows(rng, 0.15)]
            scales = [rand_float(rng, 0.2, 3.0), rand_float(rng, 0.2, 3.0), rand_float(rng, 0.3, 2.0)]
            position = [rand_float(rng, -200.0, 200.0) for _ in range(3)]
            o.write_u32(atoms[k] + ATOM_MATRIX, *matrix, 0, 0, 0)
            o.write_u32(atoms[k] + ATOM_BASE_SCALE, *scales, *position)
            o.write_u32(collections[k + 1] + COLLECTION_HIERARCHY, flags[k])
            frames += matrix + scales + position
        local = [rand_float(rng, -50.0, 50.0) for _ in range(3)]
        world = [rand_float(rng, -300.0, 300.0) for _ in range(3)]
        o.write_u32(point, *local)
        o.call(LOCAL_TO_GLOBAL, this=collections[0], args=[point])
        to_world = o.read_u32(point, 3)
        o.write_u32(point, *world)
        o.call(GLOBAL_TO_LOCAL, this=collections[0], args=[point])
        to_frame = o.read_u32(point, 3)
        rows.append(flags + frames + local + world + to_world + to_frame)
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        tumbles = tumble_cases(oracle)
        sprites = orient_sprite_cases(oracle)
        spins = initial_spin_cases(oracle)
        vapours = vapour_cases(oracle)
        products = product_cases(oracle)
        turns = axis_turn_cases(oracle)
        points = hierarchy_cases(oracle)
    write_pinned(out / "tumble.txt", "A tumbling particle's turn in one step",
                 ["whether the speed is held to the most, the tumble speed, the most, the step time, the velocity and",
                  "the rows in; the turned rows out"],
                 ["restrict", "speed", "most", "dt", "vx", "vy", "vz"] + [f"m{i}" for i in range(9)]
                 + [f"r{i}" for i in range(9)], tumbles)
    write_pinned(out / "orient_sprite.txt", "A sprite's roll from its smoothed velocity as the camera sees it",
                 ["whether it is the first step, the smoothing, the upward part, the step time, the velocity, the",
                  "smoothed velocity and the rate so far, the camera's rows in; the smoothed velocity, the rate and the",
                  "rows out"],
                 ["first", "smoothing", "upward", "dt", "vx", "vy", "vz", "sx", "sy", "sz", "rate"]
                 + [f"c{i}" for i in range(9)] + ["osx", "osy", "osz", "orate"] + [f"r{i}" for i in range(9)], sprites)
    write_pinned(out / "initial_spin.txt", "A shield particle's spin about its vertical, slowing over a time",
                 ["the spin, the time to fade, the age, the step time and the rows in; the turned rows out (scale 1,",
                  "most 10)"],
                 ["spin", "fade", "age", "dt"] + [f"m{i}" for i in range(9)] + [f"r{i}" for i in range(9)], spins)
    write_pinned(out / "vapour_turn.txt", "A shield dome patch turned out from the centre, then as its parent moves",
                 ["whether it is the first step, the step time, the parent's position and velocity, and the rows in;",
                  "the rows out"],
                 ["first", "dt", "px", "py", "pz", "vx", "vy", "vz"] + [f"m{i}" for i in range(9)]
                 + [f"r{i}" for i in range(9)], vapours)
    write_pinned(out / "matrix_products.txt", "The 3D engine's two matrix products",
                 ["two matrices a and b (rows, then the place) in; a times b by the first product, and a put in front",
                  "of b by the second product, out"],
                 [f"a{i}" for i in range(12)] + [f"b{i}" for i in range(12)] + [f"ab{i}" for i in range(12)]
                 + [f"ba{i}" for i in range(12)], products)
    write_pinned(out / "axis_turn.txt", "The 3D engine's turn about an axis by an angle",
                 ["the axis and the angle in; the matrix (rows, then the place) out"],
                 ["x", "y", "z", "angle"] + [f"m{i}" for i in range(12)], turns)
    frame_columns = []
    for k in range(3):
        frame_columns += [f"f{k}m{i}" for i in range(9)] + [f"f{k}base", f"f{k}rule", f"f{k}stretch"]
        frame_columns += [f"f{k}x", f"f{k}y", f"f{k}z"]
    write_pinned(out / "hierarchy_points.txt", "A point of a particle hierarchy moved into the world and back",
                 ["for each of three ancestor atoms, nearest first: whether its group is a hierarchy, then its rows,",
                  "base scale, rule scale, stretch and position; a local point and a world point in; the local point",
                  "in the world and the world point in the hierarchy out"],
                 ["h0", "h1", "h2"] + frame_columns + ["lx", "ly", "lz", "wx", "wy", "wz", "owx", "owy", "owz", "olx",
                                                       "oly", "olz"], points)
    print(f"tumble {len(tumbles)}, orient_sprite {len(sprites)}, initial_spin {len(spins)}, vapour_turn {len(vapours)}, "
          f"matrix_products {len(products)}, axis_turn {len(turns)}, hierarchy_points {len(points)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
