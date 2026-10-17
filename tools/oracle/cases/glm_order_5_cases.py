#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the fifth glm order lane in test/oracle/data:

- skin_point.txt: a point on a skinned creature mesh as it is posed (the eyes, the hair that is not turned): each
  vertex of a triangle placed by its bone, then the point and the normal of the triangle
- engine_inverse.txt: the 3D engine's inverse of a matrix (three rows and a place)
- mesh_points.txt: the points of a mesh moved by a matrix
- surface_sway.txt: a surface of revolution swayed: its points turned by ring and its texture slid
- surface_normals.txt: the normals of a small mesh, each the sum of its faces' unit normals, made unit again
- glow_view_normal.txt: a corner's normal in the camera's frame as the glowing shell measures it: the rows of the
  matrix made unit by the 3D engine (the game's function), then summed by the normal in the order the shell's draw adds
  them (worked here in floats, as the draw function cannot be called alone)

Usage: python tools/oracle/cases/glm_order_5_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
SKIN_POINT = 0x867400  # thiscall (point record), (mesh, bones, out point, out normal)
SKINNED = 0x100  # the mesh flag of a mesh whose vertices each move with a bone
ENGINE_INVERSE = 0x7FB290  # fastcall (ECX out, EDX in)
MESH_POINTS = 0x6C9B10  # thiscall (mesh: points +0x8, count +0x10), (matrix)
SURFACE_TWIST = 0x685FC0  # thiscall (rule: most twist +0x64), (from, around, rings, to, sway)
SURFACE_SLIDE = 0x685F40  # thiscall (rule: most slide +0x60), (from, around, rings, to, sway)
MESH_NORMALS = 0x6C9000  # thiscall (mesh: points +0x8, count +0x10, faces +0x6C, count +0x74, normals +0x80, count +0x88)
NORMAL_COUNTS = 0xD4EDD8  # the static list of face counts: pointer, end, size; made once (flag 0xD4EDEC)
NORMAL_COUNTS_MADE = 0xD4EDEC
UNIT_ROWS = 0x7FB5C0  # thiscall (3x3 matrix): each row made unit by the engine's inverse square root
MAKE_INVERSE_SQRT_TABLE = 0x8411D0
ENGINE_ALLOC = 0x842CE0  # the 3D engine's allocation, for the inverse square root table

SWAY_AROUND = 5
SWAY_RINGS = 4
NORMAL_POINTS = 6
NORMAL_FACES = [(0, 1, 2), (0, 2, 3), (1, 2, 3), (3, 4, 5), (0, 0, 4), (2, 5, 1), (4, 4, 4), (5, 3, 0)]


def rand_float(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


def random_matrix(rng: random.Random, turn: float, scale_low: float, scale_high: float, place: float) -> list[int]:
    rows3 = [[rng.uniform(-turn, turn) for _ in range(3)] for _ in range(3)]
    for k in range(3):
        rows3[k][k] += rng.choice([1.0, -1.0])
    scale = rng.uniform(scale_low, scale_high)
    return [f2u(value * scale) for row in rows3 for value in row] + [f2u(rng.uniform(-place, place)) for _ in range(3)]


def skin_point_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x5C1A)
    o = oracle
    vertices = o.alloc(0x20 * 3)
    bone_table = o.object(0x10, {0: 0x00000000, 4: 0x00010000, 8: 0x00020000})
    prim = o.object(0x40, {0x14: vertices, 0x24: bone_table})
    prims = o.object(0x10, {0: prim})
    group_list = o.object(0x10, {8: prims})
    groups = o.object(0x10, {0: group_list})
    mesh = o.object(0x40, {4: SKINNED, 0x10: groups})
    bones = o.alloc(0x30 * 3)
    record = o.alloc(0x28)
    point = o.alloc(16)
    normal = o.alloc(16)
    rows = []
    for _ in range(1500):
        head = [rng.uniform(-500, 500), rng.uniform(0, 60), rng.uniform(-500, 500)]
        triangle = [rand_float(rng, -3.0, 3.0) for _ in range(9)]
        bone_values = []
        for _ in range(3):
            rows3 = [[rng.uniform(-1.0, 1.0) for _ in range(3)] for _ in range(3)]
            for k in range(3):
                rows3[k][k] += rng.choice([1.5, -1.5])
            scale = rng.uniform(0.2, 30.0)
            bone_values += [f2u(value * scale) for row in rows3 for value in row]
            bone_values += [f2u(head[k] + rng.uniform(-20.0, 20.0)) for k in range(3)]
        st = [rand_float(rng, 0.0, 0.7), rand_float(rng, 0.0, 0.5)]
        o.write_u32(vertices, *triangle[0:3])
        o.write_u32(vertices + 0x20, *triangle[3:6])
        o.write_u32(vertices + 0x40, *triangle[6:9])
        o.write_u32(bones, *bone_values)
        o.write_u32(record, 0, 0, 0, 1, 2, 0, 1, 2, *st, 0)
        o.call(SKIN_POINT, this=record, args=[mesh, bones, point, normal])
        rows.append(triangle + bone_values + st + o.read_f32_bits(point, 3) + o.read_f32_bits(normal, 3))
    return rows


def inverse_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x1A7E)
    o = oracle
    source = o.alloc(0x30)
    out = o.alloc(0x30)
    rows = []
    for index in range(1500):
        matrix = random_matrix(rng, 0.8, 0.05, 40.0, 3000.0)
        if index % 10 == 9:
            # Nearly flat: the third row the same as the second
            matrix[6:9] = list(matrix[3:6])
        o.write_u32(source, *matrix)
        o.call(ENGINE_INVERSE, ecx=out, edx=source)
        rows.append(matrix + o.read_f32_bits(out, 12))
    return rows


def mesh_point_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x3E54)
    o = oracle
    points = o.alloc(0xC * 4)
    mesh = o.object(0x94, {0x8: points, 0x10: 4})
    matrix = o.alloc(0x30)
    rows = []
    for _ in range(400):
        values = random_matrix(rng, 0.8, 0.1, 30.0, 2000.0)
        start = [rand_float(rng, -300.0, 300.0) for _ in range(12)]
        o.write_u32(matrix, *values)
        o.write_u32(points, *start)
        o.call(MESH_POINTS, this=mesh, args=[matrix])
        rows.append(values + start + o.read_f32_bits(points, 12))
    return rows


def sway_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x5A7A)
    o = oracle
    count = SWAY_AROUND * SWAY_RINGS
    points_from = o.alloc(0xC * count)
    points_to = o.alloc(0xC * count)
    uvs_from = o.alloc(8 * count)
    uvs_to = o.alloc(8 * count)
    lists = [o.object(0x10, {0: buffer}) for buffer in (points_from, points_to, uvs_from, uvs_to)]
    rule = o.alloc(0x100)
    rows = []
    for _ in range(300):
        twist = rng.choice([f2u(1.0), rand_float(rng, -4.0, 4.0)])
        slide = rng.choice([f2u(1.0), rand_float(rng, -3.0, 3.0)])
        sway = rng.choice([f2u(1.0), f2u(-1.0), rand_float(rng, -1.0, 1.0)])
        points = [rand_float(rng, -20.0, 20.0) for _ in range(3 * count)]
        uvs = [rand_float(rng, -2.0, 2.0) for _ in range(2 * count)]
        o.write_u32(rule + 0x60, slide, twist)
        o.write_u32(points_from, *points)
        o.write_u32(uvs_from, *uvs)
        o.write(points_to, bytes(0xC * count))
        o.write(uvs_to, bytes(8 * count))
        o.call(SURFACE_TWIST, this=rule, args=[lists[0], SWAY_AROUND, SWAY_RINGS, lists[1], sway])
        o.call(SURFACE_SLIDE, this=rule, args=[lists[2], SWAY_AROUND, SWAY_RINGS, lists[3], sway])
        rows.append([twist, slide, sway] + points + uvs + o.read_f32_bits(points_to, 3 * count)
                    + o.read_f32_bits(uvs_to, 2 * count))
    return rows


def normal_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x7037)
    o = oracle
    points = o.alloc(0xC * NORMAL_POINTS)
    faces = o.alloc(0xC * len(NORMAL_FACES))
    normals = o.alloc(0xC * NORMAL_POINTS)
    counts = o.alloc(4 * NORMAL_POINTS)
    o.write_u32(faces, *[index for face in NORMAL_FACES for index in face])
    mesh = o.object(0x94, {0x8: points, 0x10: NORMAL_POINTS, 0x6C: faces, 0x74: len(NORMAL_FACES), 0x80: normals,
                           0x84: normals + 0xC * NORMAL_POINTS, 0x88: NORMAL_POINTS})
    # The list of face counts is made already, with the size of the mesh, so the function makes no list of its own
    o.write_u32(NORMAL_COUNTS, counts, counts + 4 * NORMAL_POINTS, NORMAL_POINTS)
    o.write(NORMAL_COUNTS_MADE, bytes([1]))
    rows = []
    for index in range(600):
        values = [rng.uniform(-50.0, 50.0) for _ in range(3 * NORMAL_POINTS)]
        if index % 4 == 0:
            values[3:6] = values[0:3]  # two points the same: faces with no area
        start = [f2u(v) for v in values]
        o.write_u32(points, *start)
        o.write_u32(normals, *[f2u(rng.uniform(-9, 9)) for _ in range(3 * NORMAL_POINTS)])
        o.call(MESH_NORMALS, this=mesh)
        rows.append(start + o.read_f32_bits(normals, 3 * NORMAL_POINTS))
    return rows


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def view_normal_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x6105)
    o = oracle
    o.hook_alloc(ENGINE_ALLOC)
    o.call(MAKE_INVERSE_SQRT_TABLE)
    matrix = o.alloc(0x30)
    rows = []
    for _ in range(800):
        values = random_matrix(rng, 0.9, 0.2, 3.0, 0.0)[0:9]
        normal = [rand_float(rng, -1.0, 1.0) for _ in range(3)]
        o.write_u32(matrix, *values)
        o.call(UNIT_ROWS, ecx=matrix)
        u = [u2f(bits) for bits in o.read_f32_bits(matrix, 9)]
        n = [u2f(bits) for bits in normal]

        def p(row: int, axis: int) -> float:
            return f32(u[3 * row + axis] * n[row])

        view = [f32(f32(p(0, 0) + p(2, 0)) + p(1, 0)),
                f32(f32(p(2, 1) + p(1, 1)) + p(0, 1)),
                f32(f32(p(2, 2) + p(1, 2)) + p(0, 2))]
        rows.append(values + normal + [f2u(v) for v in view])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        points = skin_point_cases(oracle)
        inverses = inverse_cases(oracle)
        moved = mesh_point_cases(oracle)
        sways = sway_cases(oracle)
        normals = normal_cases(oracle)
        views = view_normal_cases(oracle)
    write_pinned(out / "skin_point.txt", "A point on a skinned creature mesh as it is posed",
                 ["the triangle's three vertices (before the bones), the three bones (rows, then the place), s and t",
                  "in; the point and the normal out"],
                 [f"t{i}" for i in range(9)] + [f"bone{i}" for i in range(36)]
                 + ["s", "t", "px", "py", "pz", "nx", "ny", "nz"], points)
    write_pinned(out / "engine_inverse.txt", "The 3D engine's inverse of a matrix",
                 ["the matrix (rows, then the place) in; the inverse (rows, then the place) out"],
                 [f"m{i}" for i in range(12)] + [f"i{i}" for i in range(12)], inverses)
    write_pinned(out / "mesh_points.txt", "The points of a mesh moved by a matrix",
                 ["the matrix (rows, then the place) and four points in; the four points out"],
                 [f"m{i}" for i in range(12)] + [f"p{i}" for i in range(12)] + [f"r{i}" for i in range(12)], moved)
    count = SWAY_AROUND * SWAY_RINGS
    write_pinned(out / "surface_sway.txt", "A surface of revolution swayed",
                 ["the most twist, the most slide, the sway, the points and the texture points in; the turned points",
                  f"and the slid texture points out. {SWAY_AROUND} points around, {SWAY_RINGS} rings."],
                 ["twist", "slide", "sway"] + [f"p{i}" for i in range(3 * count)] + [f"uv{i}" for i in range(2 * count)]
                 + [f"rp{i}" for i in range(3 * count)] + [f"ruv{i}" for i in range(2 * count)], sways)
    faces_text = " ".join(f"{a},{b},{c}" for a, b, c in NORMAL_FACES)
    write_pinned(out / "surface_normals.txt", "The normals of a small mesh",
                 [f"the points in; the normals out. The faces: {faces_text}"],
                 [f"p{i}" for i in range(3 * NORMAL_POINTS)] + [f"n{i}" for i in range(3 * NORMAL_POINTS)], normals)
    write_pinned(out / "glow_view_normal.txt", "A corner's normal in the camera's frame for the glowing shell",
                 ["the matrix's rows and the normal in; the rows made unit by the game, summed by the normal, out"],
                 [f"m{i}" for i in range(9)] + ["nx", "ny", "nz", "vx", "vy", "vz"], views)
    print(f"glow_view_normal {len(views)}, skin_point {len(points)}, engine_inverse {len(inverses)}, mesh_points {len(moved)}, "
          f"surface_sway {len(sways)}, surface_normals {len(normals)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
