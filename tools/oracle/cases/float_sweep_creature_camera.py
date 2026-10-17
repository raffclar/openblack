#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the float sweep for the creature and camera maths in test/oracle/data:

- game_atan2.txt: the game's angle of a vector (the arctangent of the smaller part over the larger part)
- heading_and_pitch.txt: the heading and the pitch from a camera's origin to its focus
- point_from_heading.txt: a point at a distance, heading and pitch from another point
- view_width.txt: the camera's view width at its focus (the tangent of 0.875 times the distance to the focus)
- arena_ring.txt: the points of the ring of light round a fight arena

Usage: python tools/oracle/cases/float_sweep_creature_camera.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
GAME_ATAN2 = 0x7FA990  # cdecl (x, y) -> ST0
HEADING_AND_PITCH = 0x4428D0  # cdecl (origin*, focus*, heading*, pitch*)
POINT_FROM_HEADING = 0x442810  # cdecl (out*, point*, distance, heading, pitch)
VIEW_WIDTH = 0x47AEE0  # thiscall (creature), the camera from GET_CAMERA -> ST0
GET_CAMERA = 0x47D640  # stubbed to give our camera
VIEW_WIDTH_UNUSED = 0x81B370  # fastcall, one stack argument; its result is not used
ARENA_RING = 0x424550  # thiscall (arena): fills the ring's points
RING_RESIZE = 0x83E710  # thiscall, one stack argument: stubbed, the points are laid out by us
LAND_HEIGHT = 0x803090  # thiscall (map position) -> ST0: stubbed, the height is not part of the test

RING_POINTS = 32


def random_float(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


def atan2_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0xA7A2)
    pairs: list[tuple[int, int]] = []
    chosen = [(1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, -1.0), (1.0, 1.0), (-1.0, 1.0), (1.0, -1.0), (-1.0, -1.0),
              (3.0, 4.0), (-4.0, 3.0), (0.5, -0.25), (-0.25, -0.5), (1e-3, 2e-3), (100.0, -0.001)]
    pairs += [(f2u(x), f2u(y)) for x, y in chosen]
    # Every quarter and the edges between them
    for _ in range(600):
        angle = rng.uniform(-math.pi, math.pi)
        length = 10 ** rng.uniform(-3, 4)
        pairs.append((f2u(length * math.cos(angle)), f2u(length * math.sin(angle))))
    for _ in range(100):
        value = rng.uniform(-50, 50)
        pairs.append((f2u(value), f2u(value)))
        pairs.append((f2u(value), f2u(-value)))
    rows = []
    for x, y in pairs:
        result = oracle.call(GAME_ATAN2, args=[x, y])
        rows.append([x, y, result.st0f])
    return rows


def heading_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x4EAD)
    origin = oracle.alloc(16)
    focus = oracle.alloc(16)
    heading = oracle.alloc(4)
    pitch = oracle.alloc(4)
    cases = []
    for _ in range(600):
        f = [random_float(rng, 0, 5000), random_float(rng, 0, 400), random_float(rng, 0, 5000)]
        spread = 10 ** rng.uniform(-2.5, 3)
        o = [f2u(u2f(f[0]) + rng.uniform(-spread, spread)), f2u(u2f(f[1]) + rng.uniform(-spread, spread)),
             f2u(u2f(f[2]) + rng.uniform(-spread, spread))]
        cases.append(o + f)
    # Straight above, and just off it
    for dx, dz in [(0.0, 0.0), (0.005, -0.005), (0.0099, 0.0), (0.0101, 0.0), (0.0, 0.0101), (-0.02, 0.0)]:
        cases.append([f2u(1000.0 + dx), f2u(300.0), f2u(1000.0 + dz), f2u(1000.0), f2u(10.0), f2u(1000.0)])
    rows = []
    for case in cases:
        oracle.write_u32(origin, *case[0:3])
        oracle.write_u32(focus, *case[3:6])
        oracle.call(HEADING_AND_PITCH, args=[origin, focus, heading, pitch])
        rows.append(case + [oracle.read_u32(heading)[0], oracle.read_u32(pitch)[0]])
    return rows


def point_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x9017)
    out = oracle.alloc(16)
    point = oracle.alloc(16)
    rows = []
    for _ in range(600):
        case = [random_float(rng, 0, 5000), random_float(rng, -50, 400), random_float(rng, 0, 5000),
                random_float(rng, 0.5, 3000), random_float(rng, -7, 7), random_float(rng, -1.6, 1.6)]
        oracle.write_u32(point, *case[0:3])
        oracle.call(POINT_FROM_HEADING, args=[out, point, case[3], case[4], case[5]])
        rows.append(case + oracle.read_u32(out, 3))
    return rows


def view_width_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x71E3)
    camera = oracle.alloc(0x200)
    oracle.stub(GET_CAMERA, eax=camera)
    oracle.stub(VIEW_WIDTH_UNUSED, pop=4)
    rows = []
    for _ in range(600):
        focus = [random_float(rng, 0, 5000), random_float(rng, 0, 300), random_float(rng, 0, 5000)]
        spread = 10 ** rng.uniform(-1, 3.5)
        origin = [f2u(u2f(v) + rng.uniform(-spread, spread)) for v in focus]
        oracle.write_u32(camera + 0xB0, *origin, *focus)
        result = oracle.call(VIEW_WIDTH, this=0)
        rows.append(origin + focus + [result.st0f])
    return rows


def ring_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0xA4E7A)
    arena = oracle.alloc(0x100)
    points = oracle.alloc(RING_POINTS * 12)
    oracle.write_u32(arena + 0x20, points)
    oracle.write_u32(arena + 0x78, RING_POINTS)
    oracle.stub(RING_RESIZE, pop=4)
    oracle.stub(LAND_HEIGHT, st0=f2u(0.0))
    rows = []
    for _ in range(40):
        centre_x, centre_z, radius = random_float(rng, 0, 5000), random_float(rng, 0, 5000), random_float(rng, 5, 60)
        oracle.write_u32(arena + 0x68, centre_x)
        oracle.write_u32(arena + 0x70, centre_z)
        oracle.write_u32(arena + 0x74, radius)
        oracle.call(ARENA_RING, this=arena)
        for i in range(RING_POINTS):
            x, _, z = oracle.read_u32(points + i * 12, 3)
            rows.append([centre_x, centre_z, radius, i, x, z])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        n = write_pinned(out / "game_atan2.txt", "The game's angle of a vector",
                         ["cdecl (x, y): the angle of (x, y) from the x axis, ST0 stored as a float.",
                          "Rows: 14 chosen, 600 random in every quarter, 200 on the edges between the quarters."],
                         ["x", "y", "angle"], atan2_cases(oracle))
        print(f"game_atan2: {n} rows")
        oracle.reset()
        n = write_pinned(out / "heading_and_pitch.txt", "The heading and the pitch from an origin to a focus",
                         ["cdecl (origin, focus, heading out, pitch out). Rows: 600 random, 6 near straight above."],
                         ["origin_x", "origin_y", "origin_z", "focus_x", "focus_y", "focus_z", "heading", "pitch"],
                         heading_cases(oracle))
        print(f"heading_and_pitch: {n} rows")
        oracle.reset()
        n = write_pinned(out / "point_from_heading.txt", "A point at a distance, heading and pitch from a point",
                         ["cdecl (out, point, distance, heading, pitch). Rows: 600 random."],
                         ["point_x", "point_y", "point_z", "distance", "heading", "pitch", "x", "y", "z"],
                         point_cases(oracle))
        print(f"point_from_heading: {n} rows")
        oracle.reset()
        n = write_pinned(out / "view_width.txt", "The camera's view width at its focus",
                         ["The camera's origin and focus in, tan(0.875) times their distance out. Rows: 600 random."],
                         ["origin_x", "origin_y", "origin_z", "focus_x", "focus_y", "focus_z", "width"],
                         view_width_cases(oracle))
        print(f"view_width: {n} rows")
        oracle.reset()
        n = write_pinned(out / "arena_ring.txt", "The points of the ring of light round a fight arena",
                         ["The arena's centre x and z and radius in, the x and z of each of its 32 points out.",
                          "Rows: 40 random arenas, 32 points each."],
                         ["centre_x", "centre_z", "radius", "index", "x", "z"], ring_cases(oracle))
        print(f"arena_ring: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
