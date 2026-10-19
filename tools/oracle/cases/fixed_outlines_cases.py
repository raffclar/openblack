#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the outlines of the fixed things, in test/oracle/data:

- fixed_outline_box.txt: the game's outline of a 3D object from its model's box (a circle, or a box with a row of
  circles), made by the game's own code from a matrix, a scale, an angle and a box that the script chooses. Each row
  also has the game's answer to whether a circle of half a unit at a point touches that outline.
- fixed_outline_temple.txt: the temple's outline, made by the game's own code: the circle of the model's box and the
  33 circles that take the place of its parts.

The 3D object and the model are small blocks of memory that the script lays out. The call that gives the model and
the call that gives the temple's angle are stubs. The allocators go to the oracle's arena.

Usage: python tools/oracle/cases/fixed_outlines_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
MAKE_OUTLINE = 0x829390  # thiscall (3D object), ret 4: the outline of a 3D object from its model's box
MAKE_CIRCLE = 0x82AD90  # thiscall (radius, point), ret 8: a circle of the outline
COLLIDE = 0x829140  # thiscall (other circle), ret 4: whether two outlines touch
TEMPLE_OUTLINE = 0x468FB0  # thiscall on the temple's heart: its outline
TEMPLE_MATRIX = 0x467870  # the temple heart's world matrix: a copy of its 3D object's matrix
NEW = 0x7C64EE  # cdecl (size)
NEW_AT = 0x7DB790  # cdecl (size, file, line)
ENGINE_NEW = 0x842CE0  # cdecl (size)
ENGINE_FREE = 0x842D60
FREE = 0x7AEE98
MESH_STUB = 0x401200  # any function of the game, stubbed to give the model
ANGLE_STUB = 0x401300  # any function of the game, stubbed to give the temple's angle

PARTS = 16
TEMPLE_PARTS = 33


def f(value: float) -> int:
    return f32(value).bits


def rf(value: float) -> float:
    """A value rounded to a float"""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def read_f(o: Oracle, address: int) -> int:
    return o.read_u32(address)[0]


def setup(o: Oracle) -> tuple[int, int, int]:
    o.hook_alloc(NEW)
    o.hook_alloc(NEW_AT)
    o.hook_alloc(ENGINE_NEW)
    o.hook_free(ENGINE_FREE)
    o.hook_free(FREE)
    mesh = o.alloc(0x40)
    o.stub(MESH_STUB, eax=mesh, pop=0)
    table = o.alloc(0x100)
    o.write_u32(table + 0xF8, MESH_STUB)
    obj = o.alloc(0x60)
    o.write_u32(obj, table)
    return mesh, obj, table


def lay_out(o: Oracle, mesh: int, obj: int, rows, place, centre, half, scale, angle) -> None:
    o.write_u32(mesh + 0x18, *[f(v) for v in centre])
    o.write_u32(mesh + 0x24, *[f(v) for v in half])
    flat = [v for row in rows for v in row]
    o.write_u32(obj + 0x14, *[f(v) for v in flat])
    o.write_u32(obj + 0x38, f(place[0]), f(0.0), f(place[1]))
    o.write_u32(obj + 0x44, f(scale), f(angle))


def circle_words(o: Oracle, circle: int) -> list[int]:
    return [read_f(o, circle), read_f(o, circle + 8), read_f(o, circle + 0xC), read_f(o, circle + 0x10)]


def parts_of(o: Oracle, outline: int) -> list[int]:
    listed = o.read_u32(outline + 0x24)[0]
    if listed == 0:
        return []
    count = o.read_u32(listed + 4)[0]
    objs = o.read_u32(listed + 8)[0]
    return [o.read_u32(objs + 4 * i)[0] for i in range(count)]


def random_case(rng: random.Random):
    angle = rng.uniform(-math.pi, math.pi)
    scale = rng.choice([1.0, rng.uniform(0.3, 3.0)])
    rows = [[rng.uniform(-3.0, 3.0) for _ in range(3)] for _ in range(3)]
    place = (rng.uniform(0.0, 5120.0), rng.uniform(0.0, 5120.0))
    centre = (rng.uniform(-10.0, 10.0), rng.uniform(-5.0, 5.0), rng.uniform(-10.0, 10.0))
    half = [rng.uniform(0.1, 12.0), rng.uniform(0.1, 12.0), rng.uniform(0.1, 12.0)]
    # Keep to at most 16 circles in a row
    hx = max(rf(scale * half[0]), 1.0)
    hz = max(rf(scale * half[2]), 1.0)
    if max(hx, hz) / min(hx, hz) > PARTS - 2:
        half[2] = half[0]
    return rows, place, centre, half, scale, angle


def chosen_cases():
    upright = lambda a, s: [[rf(math.cos(a) * s), 0.0, rf(math.sin(a) * s)], [0.0, s, 0.0],
                            [rf(-math.sin(a) * s), 0.0, rf(math.cos(a) * s)]]
    cases = []
    # Round, at the ratio, just over it, small sides, a long box each way, no turn
    for half in ([2.0, 1.0, 2.0], [1.4, 1.0, 1.0], [1.4000001, 1.0, 1.0], [0.2, 1.0, 0.5], [7.0, 2.0, 2.0],
                 [2.0, 2.0, 9.5], [3.0, 1.0, 3.0]):
        for angle in (0.0, 0.7853982, -2.5):
            cases.append((upright(angle, 1.0), (1000.0, 2000.0), (0.5, 1.0, -0.25), half, 1.0, angle))
    return cases


def box_rows(o: Oracle, rng: random.Random, count: int):
    mesh, obj, _ = setup(o)
    cases = chosen_cases() + [random_case(rng) for _ in range(count)]
    for i, (rows, place, centre, half, scale, angle) in enumerate(cases):
        if i and i % 500 == 0:
            o.reset()
            mesh, obj, _ = setup(o)
        lay_out(o, mesh, obj, rows, place, centre, half, scale, angle)
        holder = o.alloc(4)
        o.call(MAKE_OUTLINE, this=holder, args=[obj])
        outline = o.read_u32(holder)[0]
        bounds = circle_words(o, outline)
        parts = parts_of(o, outline)
        if len(parts) > PARTS:
            continue
        part_words = []
        for part in parts:
            part_words += circle_words(o, part)
        part_words += [0] * (4 * (PARTS - len(parts)))
        # A probe near the outline, often on its edge
        reach = struct.unpack("<f", struct.pack("<I", bounds[2]))[0]
        bx = struct.unpack("<f", struct.pack("<I", bounds[0]))[0]
        bz = struct.unpack("<f", struct.pack("<I", bounds[1]))[0]
        turn = rng.uniform(-math.pi, math.pi)
        distance = rng.uniform(0.0, reach + 1.5)
        probe = (rf(bx + distance * math.cos(turn)), rf(bz + distance * math.sin(turn)))
        point = o.alloc(12)
        o.write_u32(point, f(probe[0]), f(0.0), f(probe[1]))
        circle = o.alloc(0x28)
        o.call(MAKE_CIRCLE, this=circle, args=[f32(0.5), point])
        hit = o.call(COLLIDE, this=outline, args=[circle]).eax
        inputs = [f(v) for row in rows for v in row] + [f(place[0]), f(place[1])] + [f(v) for v in centre]
        inputs += [f(v) for v in half] + [f(scale), f(angle), f(probe[0]), f(probe[1])]
        yield inputs + bounds + [len(parts)] + part_words + [hit & 0xFF]


def temple_rows(o: Oracle, rng: random.Random, count: int):
    o.reset()
    mesh, obj, _ = setup(o)
    heart_table = o.alloc(0x700)
    o.write_u32(heart_table + 0x508, ANGLE_STUB)
    o.write_u32(heart_table + 0x63C, TEMPLE_MATRIX)
    heart = o.alloc(0x100)
    o.write_u32(heart, heart_table)
    o.write_u32(heart + 0x40, obj)
    angles = [0.0, 1.5707964, -3.1415927] + [rng.uniform(-math.pi, math.pi) for _ in range(count)]
    for angle in angles:
        scale = rng.uniform(0.5, 2.0)
        rows = [[rf(math.cos(angle) * scale), 0.0, rf(math.sin(angle) * scale)], [0.0, scale, 0.0],
                [rf(-math.sin(angle) * scale), 0.0, rf(math.cos(angle) * scale)]]
        place = (rng.uniform(0.0, 5120.0), rng.uniform(0.0, 5120.0))
        centre = (rng.uniform(-2.0, 2.0), rng.uniform(0.0, 10.0), rng.uniform(-2.0, 2.0))
        half = (rng.uniform(5.0, 25.0), rng.uniform(5.0, 25.0), rng.uniform(5.0, 25.0))
        lay_out(o, mesh, obj, rows, place, centre, half, scale, angle)
        o.write_u32(heart + 0x78, 0)
        o.stub(ANGLE_STUB, eax=0, pop=0, st0=f32(angle))
        o.call(TEMPLE_OUTLINE, this=heart)
        outline = o.read_u32(o.read_u32(heart + 0x78)[0])[0]
        parts = parts_of(o, outline)
        if len(parts) != TEMPLE_PARTS:
            raise RuntimeError(f"the temple has {len(parts)} circles")
        words = []
        for part in parts:
            words += circle_words(o, part)
        inputs = [f(v) for row in rows for v in row] + [f(place[0]), f(place[1])] + [f(v) for v in centre]
        inputs += [f(v) for v in half] + [f(scale), f(angle)]
        yield inputs + circle_words(o, outline) + words


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x0F1DE)
    inputs = [f"r{i}{j}" for i in range(3) for j in range(3)] + ["tx", "tz", "cx", "cy", "cz", "hx", "hy", "hz",
                                                                 "scale", "angle"]
    circle = lambda name: [f"{name}x", f"{name}z", f"{name}r", f"{name}r2"]
    with Oracle() as o:
        box_columns = inputs + ["px", "pz"] + circle("b") + ["count"]
        for i in range(PARTS):
            box_columns += circle(f"p{i}_")
        box_columns.append("hit")
        n = write_pinned(out / "fixed_outline_box.txt", "The outline of a 3D object from its model's box",
                         ["Inputs: the 3D object's rows and place, the model's box, the scale and the angle; a probe.",
                          "Outputs: the bounding circle, the count and circles of the row (zero past the count), and",
                          "whether a circle of half a unit at the probe touches the outline."],
                         box_columns, list(box_rows(o, rng, 3000)))
        print(f"fixed_outline_box.txt: {n} rows")
        temple_columns = inputs + circle("b")
        for i in range(TEMPLE_PARTS):
            temple_columns += circle(f"p{i}_")
        n = write_pinned(out / "fixed_outline_temple.txt", "The temple's outline",
                         ["Inputs: the 3D object's rows and place, the model's box, the scale and the angle.",
                          "Outputs: the bounding circle of the model's box and the 33 circles of the temple."],
                         temple_columns, list(temple_rows(o, rng, 300)))
        print(f"fixed_outline_temple.txt: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
