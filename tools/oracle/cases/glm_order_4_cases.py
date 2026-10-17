#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the fourth glm order lane in test/oracle/data:

- shield_sphere.txt: the 3D engine's sphere tests that the shields use: a point inside a sphere grown by a margin,
  where a line from one point to another goes into it, and a speed turned off its surface
- flock_turn.txt: the flock rule's banked turn of an atom and the turn of an atom drawn as a sprite
- throw_lift.txt: the direction of a fireball that a hand throws, lifted above the hand's movement
- hair_turned.txt: one step of a creature's hair strand that is turned in the frame of its bones

Usage: python tools/oracle/cases/glm_order_4_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
POINT_IN_SPHERE = 0x57C9F0  # cdecl (sphere, point, margin) -> AL
SPHERE_ENTRY = 0x57CCD0  # cdecl (sphere, from, to, margin, out) -> AL; out is the point
DEFLECT_OFF_SPHERE = 0x57CFD0  # cdecl (sphere, point, velocity): the velocity is turned in place
FLOCK_BANKING = 0x684160  # thiscall (rule: gravity +0x44, pitch share +0x48), (atom, velocity, acceleration)
FLOCK_SPRITE = 0x6840E0  # thiscall (rule), (atom, velocity): the turn from the camera's axes
CAMERA_AXES = 0xEA1D28  # the camera's 3x3 turn, rows of three floats
THROW_LIFT = 0x69E6B0  # cdecl (velocity, out)
HAIR_STEP = 0x8467C0  # thiscall (strand), (seconds)
HAIR_BONES = 0xC37D9C  # the bone matrices the strands are placed with
MAKE_INVERSE_SQRT_TABLE = 0x8411D0
ENGINE_ALLOC = 0x842CE0  # the 3D engine's allocation, for the inverse square root table
HAIR_POINTS = 6


def rand_float(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


def sphere_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x5B1E)
    o = oracle
    sphere = o.alloc(16)
    first = o.alloc(16)
    second = o.alloc(16)
    out = o.alloc(16)
    rows = []
    for index in range(1500):
        centre = [rng.uniform(-500, 500), rng.uniform(-20, 80), rng.uniform(-500, 500)]
        radius = rng.uniform(1.0, 60.0)
        margin = rng.choice([0.0, 0.0, 1.25, 2.5, rng.uniform(0.0, 5.0)])

        def near(scale: float) -> list[float]:
            return [centre[k] + rng.uniform(-scale, scale) * radius for k in range(3)]

        kind = index % 5
        if kind == 0:
            start, end = near(3.0), near(0.5)  # from outside to inside
        elif kind == 1:
            start, end = near(0.5), near(3.0)  # starting inside
        elif kind == 2:
            start, end = near(3.0), near(3.0)  # often missing it
        elif kind == 3:
            start = near(3.0)
            end = list(start)  # no way at all
        else:
            start, end = near(1.2), near(1.2)
        values = [f2u(radius)] + [f2u(c) for c in centre]
        o.write_u32(sphere, *values)
        a = [f2u(v) for v in start]
        b = [f2u(v) for v in end]
        m = f2u(margin)
        o.write_u32(first, *a)
        inside = o.call(POINT_IN_SPHERE, args=[sphere, first, m]).eax & 0xFF
        o.write_u32(second, *b)
        o.write_u32(out, 0, 0, 0)
        o.call(SPHERE_ENTRY, args=[sphere, first, second, m, out])
        entry = o.read_f32_bits(out, 3)
        velocity = [rand_float(rng, -80, 80) for _ in range(3)]
        o.write_u32(out, *velocity)
        o.call(DEFLECT_OFF_SPHERE, args=[sphere, first, out])
        deflected = o.read_f32_bits(out, 3)
        rows.append(values + [m] + a + b + velocity + [inside] + entry + deflected)
    return rows


def flock_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0xF10C)
    o = oracle
    rule = o.alloc(0x80)
    atom = o.alloc(0x140)
    velocity = o.alloc(16)
    acceleration = o.alloc(16)
    rows = []
    for index in range(1200):
        gravity = rng.choice([f2u(-2.14159), f2u(10.0), rand_float(rng, -20.0, 20.0)])
        share = rng.choice([f2u(1.0), f2u(0.5), rand_float(rng, 0.0, 1.0)])
        v = [rand_float(rng, -30, 30) for _ in range(3)]
        if index % 10 == 0:
            v[0] = 0
            v[2] = 0  # straight up or down: no heading
        a = [rand_float(rng, -60, 60) for _ in range(3)]
        axes = [rand_float(rng, -1.0, 1.0) for _ in range(9)]
        o.write_u32(rule + 0x44, gravity, share)
        o.write_u32(velocity, *v)
        o.write_u32(acceleration, *a)
        o.write(atom, bytes(0x140))
        o.call(FLOCK_BANKING, this=rule, args=[atom, velocity, acceleration])
        banked = o.read_f32_bits(atom + 0x44, 9)
        o.write_u32(CAMERA_AXES, *axes)
        o.write(atom, bytes(0x140))
        o.call(FLOCK_SPRITE, this=rule, args=[atom, velocity])
        sprite = o.read_f32_bits(atom + 0x44, 9)
        rows.append([gravity, share] + v + a + axes + banked + sprite)
    return rows


def throw_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x7A20)
    o = oracle
    velocity = o.alloc(16)
    out = o.alloc(16)
    rows = []
    for index in range(1500):
        v = [f2u(rng.uniform(-500, 500) * 10 ** rng.uniform(-2, 0)) for _ in range(3)]
        if index % 25 == 0:
            v[0] = 0
            v[2] = 0
        o.write_u32(velocity, *v)
        o.call(THROW_LIFT, args=[velocity, out])
        rows.append(v + o.read_f32_bits(out, 3))
    return rows


def hair_turned_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x7A12)
    o = oracle
    o.hook_alloc(ENGINE_ALLOC)
    o.call(MAKE_INVERSE_SQRT_TABLE)
    vertices = o.alloc(0x20 * 3)
    bone_table = o.object(0x10, {0: 0x00000000, 4: 0x00010000, 8: 0x00020000})
    prim = o.object(0x40, {0x14: vertices, 0x24: bone_table})
    prims = o.object(0x10, {0: prim})
    group_list = o.object(0x10, {8: prims})
    groups = o.object(0x10, {0: group_list})
    mesh = o.object(0x40, {4: 0, 0x10: groups})
    bones = o.alloc(0x30 * 3)
    o.write_u32(HAIR_BONES, bones)
    group = o.alloc(0x80)
    strand = o.alloc(0x80)
    points = o.alloc(0x24 * HAIR_POINTS)
    rows = []
    for index in range(400):
        def f(low: float, high: float) -> int:
            return rand_float(rng, low, high)

        head = [rng.uniform(-200, 200), rng.uniform(0, 30), rng.uniform(-200, 200)]
        triangle = [f(-2.0, 2.0) for _ in range(9)]
        bone_values = []
        for _ in range(3):
            # A bone: three rows, mostly near a turn but not quite one, scaled, and a place near the head
            rows3 = [[rng.uniform(-1.0, 1.0) for _ in range(3)] for _ in range(3)]
            for k in range(3):
                rows3[k][k] += rng.choice([1.5, -1.5])
            scale = rng.uniform(0.5, 2.0)
            bone_values += [f2u(value * scale) for row in rows3 for value in row]
            bone_values += [f2u(head[k] + rng.uniform(-5.0, 5.0)) for k in range(3)]
        st = [f(0.0, 0.6), f(0.0, 0.4)]
        looks = [f(0.2, 1.0) for _ in range(3)] + [f(0.5, 0.95) for _ in range(3)] + \
                [f(0.01, 0.3) for _ in range(3)] + [f(0.01, 0.2) for _ in range(3)]
        angles = [f(-3.2, 3.2) for _ in range(9)]  # x, y, z: neutral, evil, good each
        alignment = f(-1.0, 1.0) if rng.random() < 0.8 else rng.choice([0, f2u(1.0), f2u(-1.0)])
        scale = f(5.0, 40.0)
        seconds = f2u(rng.choice([0.0, 0.01, 0.016, 0.033, 0.1])) if rng.random() < 0.5 else f(0.0, 0.2)
        start = [f2u(head[k % 3] + rng.uniform(-10, 10)) for k in range(3 * HAIR_POINTS)]
        speeds = [f(-30, 30) for _ in range(3 * HAIR_POINTS)]
        o.write_u32(vertices, *triangle[0:3])
        o.write_u32(vertices + 0x20, *triangle[3:6])
        o.write_u32(vertices + 0x40, *triangle[6:9])
        o.write_u32(bones, *bone_values)
        o.write(group, bytes(0x80))
        o.write_u32(group + 0x2C, *looks)
        o.write_u32(group + 0x5C, HAIR_POINTS)
        o.write_u32(group + 0x64, alignment, scale)
        o.write_u32(group + 0x70, mesh)
        o.write(strand, bytes(0x80))
        o.write_u32(strand, *angles)
        o.write_u32(strand + 0x24, 1)
        o.write_u32(strand + 0x34, points)
        o.write_u32(strand + 0x38 + 8, 0, 1, 2, 0, 1, 2)
        o.write_u32(strand + 0x38 + 0x20, *st)
        o.write_u32(strand + 0x60, group)
        o.write(points, bytes(0x24 * HAIR_POINTS))
        o.write_u32(points, *start)
        o.write_u32(points + 0x18 * HAIR_POINTS, *speeds)
        o.call(HAIR_STEP, this=strand, args=[seconds])
        after = o.read_f32_bits(points, 3 * HAIR_POINTS)
        after_speeds = o.read_f32_bits(points + 0x18 * HAIR_POINTS, 3 * HAIR_POINTS)
        rows.append(triangle + bone_values + st + looks + angles + [alignment, scale, seconds] + start + speeds + after
                    + after_speeds)
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        spheres = sphere_cases(oracle)
        flocks = flock_cases(oracle)
        throws = throw_cases(oracle)
        hairs = hair_turned_cases(oracle)
    write_pinned(out / "shield_sphere.txt", "The 3D engine's sphere tests that the shields use",
                 ["the sphere (radius, centre), the margin, the first and second points and a speed in; whether the",
                  "first point is inside, where the line from the first to the second point goes into the sphere, and",
                  "the speed turned off the sphere at the first point out"],
                 ["r", "cx", "cy", "cz", "margin", "ax", "ay", "az", "bx", "by", "bz", "vx", "vy", "vz", "inside",
                  "ex", "ey", "ez", "dx", "dy", "dz"], spheres)
    write_pinned(out / "flock_turn.txt", "The flock rule's turns of an atom",
                 ["the gravity for banking, the share of the climb, the speed, the acceleration and the camera's axes",
                  "(rows) in; the banked turn (rows) and the sprite's turn (rows) out"],
                 ["gravity", "share", "vx", "vy", "vz", "ax", "ay", "az"] + [f"c{i}" for i in range(9)]
                 + [f"b{i}" for i in range(9)] + [f"s{i}" for i in range(9)], flocks)
    write_pinned(out / "throw_lift.txt", "The direction of a fireball that a hand throws",
                 ["the hand's movement in; the direction lifted by 0.3 out"], ["vx", "vy", "vz", "dx", "dy", "dz"],
                 throws)
    hair_columns = ([f"t{i}" for i in range(9)] + [f"bone{i}" for i in range(36)] + ["s", "t"]
                    + [f"look{i}" for i in range(12)] + [f"angle{i}" for i in range(9)]
                    + ["alignment", "scale", "seconds"]
                    + [f"p{i}" for i in range(3 * HAIR_POINTS)] + [f"v{i}" for i in range(3 * HAIR_POINTS)]
                    + [f"rp{i}" for i in range(3 * HAIR_POINTS)] + [f"rv{i}" for i in range(3 * HAIR_POINTS)])
    write_pinned(out / "hair_turned.txt", "One step of a creature's hair strand turned in its bones' frame",
                 ["the triangle's three vertices (before the bones), the three bones (rows, then the place), s and t,",
                  "the looks (length, damping, stiffness, thickness: neutral, evil, good each), the angles (x, y, z:",
                  "neutral, evil, good each), the alignment, the scale and the seconds, the points and speeds before;",
                  f"the points and speeds after. {HAIR_POINTS} points."], hair_columns, hairs)
    print(f"shield_sphere {len(spheres)}, flock_turn {len(flocks)}, throw_lift {len(throws)}, hair_turned {len(hairs)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
