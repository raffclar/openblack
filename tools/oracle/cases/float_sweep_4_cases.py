#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the float sweep lane 4 (magic, particles, dance shapes, physics) in test/oracle/data:

- spin_kept.txt: the physics body's spin kept each step, the share kept each second to the power of the step
- reaction_timing.txt: the four standard reaction times (villager and creature, to react and before again)
- flock_number.txt: the number of animals that a flock miracle makes
- dance_shapes.txt: the points of the circle, the wavy circle, the spiral, the wave and the octagon
- rotate_point.txt: a dance shape point turned by an angle
- trail_path.txt: the length of a gesture trail's path and a point at a share of it
- light_sheet.txt: a sheet of light's wave and strengths after three steps
- force_field.txt: a pulse on a pulsed sheet of light, then one step of its fade

Usage: python tools/oracle/cases/float_sweep_4_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
BODY_SET_UP_CONSTANTS = 0x7FB810  # thiscall (mass, material, dynamic), RET 0xC; the spin kept at +0x148
LIVING_TURNS_TO_REACT = 0x5F18C0  # stdcall-like (unused, type, distance), RET 0xC, EAX
LIVING_TURNS_AGAIN = 0x5F1920
CREATURE_TURNS_TO_REACT = 0x4F29C0
CREATURE_TURNS_AGAIN = 0x4F2A20
REACTION_TABLE = 0xD4F6C4  # 100 bytes a reaction type; type 0 is used
FLOCK_NUMBER = 0x723B80  # thiscall, EAX
FLOCK_INFO = 0x7201D0  # stubbed: EAX is the info, number at +0x58
FLOCK_POWER = 0x7216F0  # stubbed: ST0 is the tribal power
SHAPES = 0xCADB70  # 256 points of 8 bytes for each shape slot (slot 0 is used)
SHAPE_MAKERS = {"circle": 0x50F9A0, "wavy": 0x50FA00, "spiral": 0x50FA80, "wave": 0x50FE70, "octagon": 0x510210}
ROTATE_POINT = 0x50D320  # cdecl (point, angle), EAX:EDX
TRAIL_MEASURE = 0x687CB0  # thiscall
TRAIL_AT = 0x687E30  # thiscall (fraction, out), RET 8
SHEET_UPDATE = 0x83E4F0  # thiscall (seconds), RET 4
SHEET_PULSE = 0x83E610  # thiscall (centre x, y, z, radius), RET 0x10

TRAIL_POINTS = 8
SHEET_POINTS = 10


def spin_kept_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(41)
    body = o.alloc(0x200)
    material = o.alloc(0x40)
    low, high = f2u(0.2), f2u(1.0)
    inputs = [f2u(v) for v in (0.2, 0.25, 0.5, 0.75, 0.8, 0.9, 0.95, 0.99, 0.999, 1.0)]
    inputs += [rng.randint(low, high) for _ in range(3000)]
    inputs += list(range(high - 300, high + 1))
    rows = []
    for bits in inputs:
        o.write_u32(material + 0x10, bits)
        result = o.call(BODY_SET_UP_CONSTANTS, this=body, args=[f32(1.0), material, 0])
        assert result.popped == 12, result
        rows.append([bits, o.read_u32(body + 0x148)[0]])
    return rows


def reaction_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(42)
    rows = []
    for n in range(2000):
        reach = rng.choice([10.0, 20.0, 30.0, 50.0, 100.0]) if n % 3 == 0 else rng.uniform(1.0, 150.0)
        importance = rng.choice([0.0, 0.5, 1.0]) if n % 5 == 0 else rng.uniform(0.0, 1.0)
        turns = [rng.randint(0, 300) for _ in range(4)]
        distance = rng.uniform(0.0, reach + 30.0)
        o.write_u32(REACTION_TABLE, *turns)
        o.write_u32(REACTION_TABLE + 0x18, f2u(reach), f2u(importance))
        row = [f2u(reach), f2u(importance), *turns, f2u(distance)]
        for address in (LIVING_TURNS_TO_REACT, LIVING_TURNS_AGAIN, CREATURE_TURNS_TO_REACT, CREATURE_TURNS_AGAIN):
            result = o.call(address, args=[0, 0, f32(distance)])
            assert result.popped == 12, result
            row.append(result.eax)
        rows.append(row)
    return rows


def flock_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(43)
    info = o.alloc(0x100)
    this = o.alloc(0x100)
    o.stub(FLOCK_INFO, eax=info)
    rows = []
    cases = [(n, p) for n in (0, 1, 2, 3, 5, 10, 20) for p in (0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0)]
    cases += [(rng.randint(0, 60), rng.uniform(0.0, 3.0)) for _ in range(1500)]
    cases += [(rng.randint(1, 1 << 26), rng.uniform(0.0, 3.0)) for _ in range(300)]
    for number, power in cases:
        o.write_u32(info + 0x58, number)
        o.stub(FLOCK_POWER, st0=f32(power))
        result = o.call(FLOCK_NUMBER, this=this)
        rows.append([number, f2u(power), result.eax])
    return rows


def dance_shape_cases(o: Oracle) -> list[list[int]]:
    rows = []
    for shape, (name, address) in enumerate(SHAPE_MAKERS.items()):
        o.write(SHAPES, bytes(256 * 8))
        o.call(address, args=[0])
        words = o.read_u32(SHAPES, 512)
        for k in range(256):
            rows.append([shape, k, words[2 * k], words[2 * k + 1]])
    return rows


def rotate_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(44)
    point = o.alloc(0x10)
    rows = []
    cases = [(x, y) for x in (0.0, 1.0, -1.0, 32767.5) for y in (0.0, 1.0, -1.0, -32767.5)]
    cases += [(rng.uniform(-40000, 40000), rng.uniform(-40000, 40000)) for _ in range(2000)]
    for x, y in cases:
        angle = rng.uniform(-7.0, 7.0)
        o.write_u32(point, f2u(x), f2u(y))
        result = o.call(ROTATE_POINT, args=[point, f32(angle)])
        rows.append([f2u(x), f2u(y), f2u(angle), result.eax, result.edx])
    return rows


def trail_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(45)
    path = o.alloc(0x40)
    points = o.alloc(16 * TRAIL_POINTS)
    out = o.alloc(0x10)
    rows = []
    for n in range(800):
        coordinates = []
        for i in range(TRAIL_POINTS):
            if i > 0 and n % 4 == 0 and rng.random() < 0.3:
                coordinates.append(list(coordinates[-1]))  # A segment of no length
            else:
                coordinates.append([rng.uniform(-3000, 3000), rng.uniform(0, 200), rng.uniform(-3000, 3000)])
        fraction = rng.choice([0.0, 1.0, 0.5]) if n % 10 == 0 else rng.uniform(0.0, 1.0)
        for i, (x, y, z) in enumerate(coordinates):
            o.write_u32(points + 16 * i, f2u(x), f2u(y), f2u(z), 0)
        o.write_u32(path, 0, points, 0, TRAIL_POINTS, 0, 0, 1)
        o.call(TRAIL_MEASURE, this=path)
        length = o.read_u32(points + 16 * (TRAIL_POINTS - 1) + 12)[0]
        o.write_u32(path, 0)
        result = o.call(TRAIL_AT, this=path, args=[f32(fraction), out])
        assert result.popped == 8, result
        rows.append([f2u(c) for xyz in coordinates for c in xyz] + [f2u(fraction), length, *o.read_u32(out, 3)])
    return rows


def sheet_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(46)
    sheet = o.alloc(0x80)
    strengths = o.alloc(4 * SHEET_POINTS)
    heights = o.alloc(4 * SHEET_POINTS)
    rows = []
    for n in range(600):
        height = rng.uniform(1.0, 60.0)
        shift = rng.uniform(0.01, 0.2)
        steps = [(rng.uniform(0.0, 1.0), rng.uniform(0.0, 0.25)) for _ in range(3)]
        o.write(sheet, bytes(0x80))
        o.write(strengths, bytes(4 * SHEET_POINTS))
        o.write(heights, bytes(4 * SHEET_POINTS))
        o.write_u32(sheet, SHEET_POINTS)
        o.write_u32(sheet + 0x14, f2u(height), f2u(shift), f2u(1.0))
        o.write_u32(sheet + 0x24, strengths, heights)
        row = [f2u(height), f2u(shift)]
        for strength, seconds in steps:
            o.write_u32(sheet + 0x38, f2u(strength))
            result = o.call(SHEET_UPDATE, this=sheet, args=[f32(seconds)])
            assert result.popped == 4, result
            row += [f2u(strength), f2u(seconds)]
        rows.append(row + o.read_u32(heights, SHEET_POINTS) + o.read_u32(strengths, SHEET_POINTS))
    return rows


def force_field_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(47)
    sheet = o.alloc(0x80)
    points = o.alloc(12 * SHEET_POINTS)
    strengths = o.alloc(4 * SHEET_POINTS)
    heights = o.alloc(4 * SHEET_POINTS)
    rows = []
    for n in range(600):
        xz = [(rng.uniform(-50, 50), rng.uniform(-50, 50)) for _ in range(SHEET_POINTS)]
        cx, cz = rng.uniform(-20, 20), rng.uniform(-20, 20)
        radius = rng.uniform(5.0, 80.0)
        seconds = rng.choice([0.0, 0.01, 1.0 / 60.0, 0.1]) if n % 4 == 0 else rng.uniform(0.0, 3.0)
        o.write(sheet, bytes(0x80))
        o.write(strengths, bytes(4 * SHEET_POINTS))
        for i, (x, z) in enumerate(xz):
            o.write_u32(points + 12 * i, f2u(x), 0, f2u(z))
        o.write_u32(sheet, SHEET_POINTS)
        o.write_u32(sheet + 0x14, f2u(10.0), 0, f2u(1.0))
        o.write_u32(sheet + 0x20, points, strengths, heights)
        o.write_u32(sheet + 0x5C, 1)
        result = o.call(SHEET_PULSE, this=sheet, args=[f32(cx), f32(0.0), f32(cz), f32(radius)])
        assert result.popped == 16, result
        pulsed = o.read_u32(strengths, SHEET_POINTS)
        o.call(SHEET_UPDATE, this=sheet, args=[f32(seconds)])
        row = [f2u(c) for p in xz for c in p] + [f2u(cx), f2u(cz), f2u(radius), f2u(seconds)]
        rows.append(row + pulsed + o.read_u32(strengths, SHEET_POINTS))
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as o:
        n = write_pinned(out / "spin_kept.txt", "A physics body's spin kept each step",
                         ["The material's spin kept each second in, the spin kept each 0.005 s step out (float bits)."],
                         ["per_second", "per_step"], spin_kept_cases(o))
        print(f"spin_kept: {n} rows")
        o.reset()
        n = write_pinned(out / "reaction_timing.txt", "The four standard reaction times",
                         ["Inputs: the row's distance and its importance (float bits), the four turn counts (ints),",
                          "the distance to the thing. Out: villager react and again, creature react and again (ints)."],
                         ["reach", "importance", "turns", "again", "creature_turns", "creature_again", "distance",
                          "react", "react_again", "creature_react", "creature_react_again"], reaction_cases(o))
        print(f"reaction_timing: {n} rows")
        o.reset()
        n = write_pinned(out / "flock_number.txt", "The number of animals that a flock miracle makes",
                         ["The info's number (int) and the tribal power (float bits) in, the number to make out."],
                         ["number", "power", "made"], flock_cases(o))
        print(f"flock_number: {n} rows")
        o.reset()
        n = write_pinned(out / "dance_shapes.txt", "The dance shapes that the game makes with sines and cosines",
                         ["shape: 0 circle, 1 wavy circle, 2 spiral, 3 wave, 4 octagon. k: the point. x, z: ints."],
                         ["shape", "k", "x", "z"], dance_shape_cases(o))
        print(f"dance_shapes: {n} rows")
        o.reset()
        n = write_pinned(out / "rotate_point.txt", "A dance shape point turned by an angle",
                         ["The point (x, y) and the angle in, the turned point out (float bits)."],
                         ["x", "y", "angle", "out_x", "out_y"], rotate_cases(o))
        print(f"rotate_point: {n} rows")
        o.reset()
        columns = [f"p{i}{axis}" for i in range(TRAIL_POINTS) for axis in "xyz"]
        n = write_pinned(out / "trail_path.txt", "A gesture trail's path: its length and a point at a share of it",
                         [f"{TRAIL_POINTS} points and the share in; the length and the point out (float bits)."],
                         columns + ["fraction", "length", "x", "y", "z"], trail_cases(o))
        print(f"trail_path: {n} rows")
        o.reset()
        columns = ["height", "shift"] + [f"{name}{i}" for i in range(3) for name in ("strength", "seconds")]
        columns += [f"h{i}" for i in range(SHEET_POINTS)] + [f"s{i}" for i in range(SHEET_POINTS)]
        n = write_pinned(out / "light_sheet.txt", "A sheet of light after three steps",
                         [f"{SHEET_POINTS} points, the slide speed 1. Each step sets the strength, then steps the time.",
                          "Out: each point's height and strength (float bits)."], columns, sheet_cases(o))
        print(f"light_sheet: {n} rows")
        o.reset()
        columns = [f"p{i}{axis}" for i in range(SHEET_POINTS) for axis in "xz"] + ["cx", "cz", "radius", "seconds"]
        columns += [f"pulsed{i}" for i in range(SHEET_POINTS)] + [f"faded{i}" for i in range(SHEET_POINTS)]
        n = write_pinned(out / "force_field.txt", "A pulse on a pulsed sheet of light and one step of its fade",
                         ["The points (x, z), the pulse's centre and radius, the step in seconds (float bits).",
                          "Out: each strength after the pulse, then after the step."], columns, force_field_cases(o))
        print(f"force_field: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
