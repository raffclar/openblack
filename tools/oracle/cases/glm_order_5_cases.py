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

Usage: python tools/oracle/cases/glm_order_5_cases.py [output folder]   (default: test/oracle/data)
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
SKIN_POINT = 0x867400  # thiscall (point record), (mesh, bones, out point, out normal)
SKINNED = 0x100  # the mesh flag of a mesh whose vertices each move with a bone


def rand_float(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


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


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        points = skin_point_cases(oracle)
    write_pinned(out / "skin_point.txt", "A point on a skinned creature mesh as it is posed",
                 ["the triangle's three vertices (before the bones), the three bones (rows, then the place), s and t",
                  "in; the point and the normal out"],
                 [f"t{i}" for i in range(9)] + [f"bone{i}" for i in range(36)]
                 + ["s", "t", "px", "py", "pz", "nx", "ny", "nz"], points)
    print(f"skin_point {len(points)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
