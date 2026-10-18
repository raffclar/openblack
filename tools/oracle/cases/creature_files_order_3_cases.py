#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of four creature rules, in test/oracle/data:

- creature_stand_valid.txt: whether a creature may stand at a point, for a radius. The game's test runs as it is, on a
  land whose cells come from LandSort below (the C++ test makes the same land).
- creature_nearest_stand.txt: the game's search for the nearest place to stand, on the same land.
- creature_run_away.txt: the point that a creature runs away to. The game's function runs as it is. The random draw
  and the ground height are stubs. The test for a clear cell is a small function that reads ClearCell below.
- creature_model_touches.txt: whether a corner of a model is in a sphere. The game's function runs as it is, with one
  corner. The thing's world rows are a stub that copies the rows of the row.

Usage: python tools/oracle/cases/creature_files_order_3_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
CAN_STAND = 0x483850  # cdecl (point), the destination clearance
CAN_STAND_RADIUS = 0x483890  # cdecl (point, radius)
NEAREST_STAND = 0x4839D0  # cdecl (point, out); out is left as it was when no point is found
RUN_AWAY = 0x47A500  # thiscall on the creature (threat, out), ret 8
GAME_RAND = 0x6DE510  # cdecl (n, file, line)
GET_ALTITUDE = 0x803090  # thiscall on a map position, ret 0
CELL_TEST = 0x6059A0  # cdecl (creature, x, z, living blocks)
MODEL_TOUCHES = 0x481C60  # cdecl (point, radius, thing)
SPARE_CODE = 0x401300  # game code that none of these calls runs, used for the world rows copy
SPARE_STUB = 0x401200  # any function of the game, stubbed to give mesh 0

LAND_AVOID = 0xD559B0  # 512 x 512 bytes, row after row of x
MAP = 0xD0195C  # the map: its width and height in cells at +0x59C8 and +0x59C4
MESH_PACK = 0xE9FE34
SPIRAL_STEPS = 0xDA59FC  # the spiral's four steps, made at start-up: +x, +z, -x, -z

CELLS = 512


def f(value: float) -> int:
    return f32(value).bits


def land_sort(x: int, z: int) -> int:
    """The sort of a land cell: 0 open, 1 blocked, 4 cut off, 6 water. The C++ test has the same rule."""
    if 100 <= x < 120 and 100 <= z < 130:
        return 1
    if 300 <= x < 340 and 300 <= z < 305:
        return 1
    if 400 <= x < 410 and 400 <= z < 420:
        return 4
    if (x * 31 + z * 17) % 97 == 0:
        return 1
    if (x + z) % 5 == 0:
        return 6
    return 0


def clear_cell(x: int, z: int) -> int:
    """Whether a cell is clear for the run away search. The C++ test has the same rule."""
    if 150 <= x < 170 and 150 <= z < 170:
        return 0
    return 0 if (x * 7 + z * 3) % 4 == 0 else 1


def write_land(o: Oracle) -> None:
    o.write(LAND_AVOID, bytes(land_sort(x, z) for z in range(CELLS) for x in range(CELLS)))


def stand_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    point = o.alloc(16)
    rows = []
    points = []
    for (x0, z0, x1, z1) in [(1000, 1000, 1200, 1300), (3000, 3000, 3400, 3050), (4000, 4000, 4100, 4200)]:
        for _ in range(300):
            # Near the sides of the blocked areas, where the radius decides
            side = pick.randrange(4)
            x = pick.uniform(x0 - 12, x1 + 12) if side < 2 else pick.choice([x0, x1]) + pick.uniform(-12, 12)
            z = pick.uniform(z0 - 12, z1 + 12) if side >= 2 else pick.choice([z0, z1]) + pick.uniform(-12, 12)
            points.append((x, z))
    points += [(pick.uniform(0, 5120), pick.uniform(0, 5120)) for _ in range(600)]
    points += [(pick.uniform(-20, 20), pick.uniform(-20, 5140)) for _ in range(50)]
    points += [(pick.uniform(5100, 5140), pick.uniform(-20, 5140)) for _ in range(50)]
    points += [(-0.5, 100.0), (-0.05, 100.0), (5119.99, 100.0), (5120.0, 100.0), (100.0, -0.01)]
    for x, z in points:
        o.write_f32(point, f32(x), f32(0.0), f32(z))
        radius = 7.1 if pick.random() < 0.5 else 7.05
        if radius == 7.1:
            r = o.call(CAN_STAND, args=[point])
        else:
            r = o.call(CAN_STAND_RADIUS, args=[point, f32(radius)])
        rows.append([f(x), f(z), f(radius), r.eax & 0xFF])
    return rows


def nearest_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    point = o.alloc(16)
    out = o.alloc(16)
    rows = []
    points = [(pick.uniform(1000, 1200), pick.uniform(1000, 1300)) for _ in range(120)]
    points += [(pick.uniform(3000, 3400), pick.uniform(2995, 3055)) for _ in range(60)]
    points += [(pick.uniform(0, 5120), pick.uniform(0, 5120)) for _ in range(120)]
    points += [(pick.uniform(3990, 4110), pick.uniform(3990, 4210)) for _ in range(60)]
    for x, z in points:
        o.write_f32(point, f32(x), f32(0.0), f32(z))
        o.write_u32(out, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF)
        o.call(NEAREST_STAND, args=[point, out])
        got = o.read_u32(out, 3)
        found = 0 if got[0] == 0xFFFFFFFF else 1
        rows.append([f(x), f(z), found] + (got if found else [0, 0, 0]))
    return rows


def cell_test_code(table: int) -> bytes:
    """cdecl (creature, x, z, living): table[z * 512 + x] for cells on the map, else 0."""
    return bytes([
        0x8B, 0x44, 0x24, 0x08,  # mov eax, [esp + 8]
        0x8B, 0x4C, 0x24, 0x0C,  # mov ecx, [esp + 0xC]
        0x3D, 0x00, 0x02, 0x00, 0x00,  # cmp eax, 0x200
        0x73, 0x15,  # jae zero
        0x81, 0xF9, 0x00, 0x02, 0x00, 0x00,  # cmp ecx, 0x200
        0x73, 0x0D,  # jae zero
        0xC1, 0xE1, 0x09,  # shl ecx, 9
        0x01, 0xC1,  # add ecx, eax
        0x0F, 0xB6, 0x81]) + struct.pack("<I", table) + bytes([  # movzx eax, byte [ecx + table]
        0xC3,  # ret
        0x31, 0xC0,  # zero: xor eax, eax
        0xC3,  # ret
    ])


def run_away_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    clear = o.alloc(CELLS * CELLS)
    all_blocked = bytes(CELLS * CELLS)
    pattern = bytes(clear_cell(x, z) for z in range(CELLS) for x in range(CELLS))
    o.write(CELL_TEST, cell_test_code(clear))
    o.write(SPIRAL_STEPS, struct.pack("<8h", 1, 0, 0, 1, -1, 0, 0, -1))
    world = o.alloc(0x6000)
    o.write_u32(world + 0x59C8, CELLS)
    o.write_u32(world + 0x59C4, CELLS)
    o.write_u32(MAP, world)
    info = o.alloc(0x300)
    thing = o.alloc(0x100)
    creature = o.object(0x200, {0x28: info, 0x160: thing})
    threat = o.alloc(16)
    out = o.alloc(16)
    rows = []
    for layout, blocked in ((0, pattern), (1, all_blocked)):
        o.write(clear, blocked)
        for case in range(700 if layout == 0 else 100):
            if case < 60:
                # Round the blocked square, so that the spiral goes a long way
                cx, cz = pick.randrange(140, 180), pick.randrange(140, 180)
            elif case < 120:
                # Near the land's blocked areas and its sides
                cx, cz = pick.choice([(pick.randrange(95, 125), pick.randrange(95, 135)),
                                      (pick.randrange(0, 6), pick.randrange(0, 512)),
                                      (pick.randrange(505, 512), pick.randrange(0, 512))])
            else:
                cx, cz = pick.randrange(0, 512), pick.randrange(0, 512)
            x = (cx << 16) | pick.randrange(0, 0x10000)
            z = (cz << 16) | pick.randrange(0, 0x10000)
            altitude = 0.0 if pick.random() < 0.7 else pick.uniform(0.0, 20.0)
            corner = (cx * 10.0, cz * 10.0)
            kind = pick.randrange(10)
            if kind == 0:
                tx, tz = corner
            elif kind == 1:
                tx, tz = corner[0] + pick.uniform(-0.001, 0.001), corner[1] + pick.uniform(-0.001, 0.001)
            elif kind == 2:
                tx, tz = corner[0], corner[1] + pick.uniform(-300, 300)
            else:
                tx, tz = corner[0] + pick.uniform(-300, 300), corner[1] + pick.uniform(-300, 300)
            draw = pick.randrange(40)
            run_away = pick.choice([20.0, 40.0, 60.0, 100.0, pick.uniform(10.0, 150.0)])
            ground = pick.uniform(0.0, 120.0)
            o.write_u32(creature + 0x14, x, z)
            o.write_f32(creature + 0x1C, f32(altitude))
            o.write_f32(info + 0x2E8, f32(run_away))
            o.write_f32(threat, f32(tx), f32(0.0), f32(tz))
            o.write_u32(out, 0, 0, 0)
            o.stub(GAME_RAND, eax=draw)
            o.stub(GET_ALTITUDE, st0=f32(ground))
            o.call(RUN_AWAY, this=creature, args=[threat, out])
            rows.append([layout, x, z, f(altitude), f(tx), f(tz), draw, f(run_away), f(ground)] + o.read_u32(out, 3))
    return rows


def rows_copy_code(source: int) -> bytes:
    """thiscall (out), ret 4: copies 12 floats from the source."""
    return bytes([
        0x56,  # push esi
        0x57,  # push edi
        0x8B, 0x7C, 0x24, 0x0C,  # mov edi, [esp + 0xC]
        0xBE]) + struct.pack("<I", source) + bytes([  # mov esi, source
        0xB9, 0x0C, 0x00, 0x00, 0x00,  # mov ecx, 12
        0xF3, 0xA5,  # rep movsd
        0x5F,  # pop edi
        0x5E,  # pop esi
        0xC2, 0x04, 0x00,  # ret 4
    ])


def model_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    matrix = o.alloc(48)
    o.write(SPARE_CODE, rows_copy_code(matrix))
    o.stub(SPARE_STUB, eax=0)
    vtable = o.alloc(0x700)
    o.write_u32(vtable + 0x63C, SPARE_CODE)
    o.write_u32(vtable + 0x608, SPARE_STUB)
    thing = o.object(0x100, {0: vtable})
    corner = o.alloc(0x20)
    group = o.object(0x20, {0x10: 1, 0x14: corner})
    groups = o.object(4, {0: group})
    sub = o.object(0x10, {0: 0x20000000, 4: 1, 8: groups})
    subs = o.object(4, {0: sub})
    mesh = o.object(0x20, {4: 0, 0xC: 1, 0x10: subs})
    pack = o.object(8, {0: 1, 4: mesh})
    o.write_u32(MESH_PACK, pack)
    centre = o.alloc(16)
    rows = []
    for case in range(1500):
        yaw, lean_x, lean_z = pick.uniform(-math.pi, math.pi), pick.uniform(-0.6, 0.6), pick.uniform(-0.6, 0.6)
        scale = pick.choice([1.0, pick.uniform(0.3, 3.0)])
        cy, sy, cx, sx, cz, sz = (math.cos(yaw), math.sin(yaw), math.cos(lean_x), math.sin(lean_x), math.cos(lean_z),
                                  math.sin(lean_z))
        if case % 2:
            turn = [[cy, 0.0, sy], [0.0, 1.0, 0.0], [-sy, 0.0, cy]]
        else:
            turn = [[cz * cy - sz * sx * sy, -sz * cx, sz * sx * cy + cz * sy],
                    [cz * sx * sy + sz * cy, cz * cx, sz * sy - cz * sx * cy], [-cx * sy, sx, cx * cy]]
        rows3 = [[u2f(f32(v * scale).bits) for v in row] for row in turn]
        position = [pick.uniform(0.0, 5000.0), pick.uniform(0.0, 300.0), pick.uniform(0.0, 5000.0)]
        point = [pick.uniform(-20.0, 20.0), pick.uniform(0.0, 30.0), pick.uniform(-20.0, 20.0)]
        world = [rows3[1][a] * point[1] + rows3[0][a] * point[0] + rows3[2][a] * point[2] + position[a] for a in range(3)]
        sphere = [world[a] + pick.uniform(-10.0, 10.0) for a in range(3)]
        reach = math.dist(world, sphere)
        radius = reach * (1.0 + pick.uniform(-2e-6, 2e-6)) if case % 3 else pick.uniform(0.5, 20.0)
        values = [v for row in rows3 for v in row] + position
        o.write_f32(matrix, *[f32(v) for v in values])
        o.write_f32(corner, *[f32(v) for v in point])
        o.write_f32(centre, *[f32(v) for v in sphere])
        r = o.call(MODEL_TOUCHES, args=[centre, f32(radius), thing])
        rows.append([f(v) for v in values] + [f(v) for v in point] + [f(v) for v in sphere] + [f(radius), r.eax & 1])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x5EED_0723)
    with Oracle() as o:
        write_land(o)
        stand = stand_rows(o, pick)
        nearest = nearest_rows(o, pick)
        run_away = run_away_rows(o, pick)
        o.reset()
        model = model_rows(o, pick)
    count = write_pinned(
        out / "creature_stand_valid.txt",
        "Whether a creature may stand at a point",
        ["x, z: the point. radius: the clearance. valid: 1 when it may stand there. The land: LandSort in the case script."],
        ["x", "z", "radius", "valid"],
        stand,
    )
    print(f"{count} stand rows")
    count = write_pinned(
        out / "creature_nearest_stand.txt",
        "The nearest place where a creature may stand",
        ["x, z: the point. found: 1 when the search finds a place. out_*: the place, 0 when there is none."],
        ["x", "z", "found", "out_x", "out_y", "out_z"],
        nearest,
    )
    print(f"{count} nearest rows")
    count = write_pinned(
        out / "creature_run_away.txt",
        "The point that a creature runs away to",
        [
            "layout: 0 = the clear cells of ClearCell, 1 = no clear cell. map_x, map_z: the creature's map position.",
            "altitude: its height above the ground. threat_*: the threat. draw: the random draw. run_away: the species'",
            "distance. ground: the ground height that every height lookup gives. point_*: the point.",
        ],
        ["layout", "map_x", "map_z", "altitude", "threat_x", "threat_z", "draw", "run_away", "ground", "point_x",
         "point_y", "point_z"],
        run_away,
    )
    print(f"{count} run away rows")
    count = write_pinned(
        out / "creature_model_touches.txt",
        "Whether a corner of a model is in a sphere of a creature's body",
        ["row*_*: the thing's world rows, sized. pos_*: its position. corner_*: the model's corner. centre_*, radius:",
         "the sphere. touches: 1 when the corner is in the sphere."],
        ["row0_x", "row0_y", "row0_z", "row1_x", "row1_y", "row1_z", "row2_x", "row2_y", "row2_z", "pos_x", "pos_y",
         "pos_z", "corner_x", "corner_y", "corner_z", "centre_x", "centre_y", "centre_z", "radius", "touches"],
        model,
    )
    print(f"{count} model rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
