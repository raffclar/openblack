#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the liquid particles and the creature's puke, in test/oracle/data:

- liquid_step.txt: one step of a particle of the pool (falls, meets the land, slides, flies, goes)
- liquid_draw.txt: the size and the frame that the game draws a particle with
- land_light.txt: the land's light at a point, from the luminosity of the four corners of its cell
- land_haze.txt: a colour as the distance haze dims it
- creature_puke.txt: the drops that one turn of the puke animation adds to the pool

The land is a set of blocks laid out in the executable's own tables, with the altitude and the luminosity of each
corner from simple formulas that the tests make again (Land below). The light table also comes from a formula.

Usage: python tools/oracle/cases/liquid_particles_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
STEP = 0x846010  # thiscall (particle), arg seconds; RET 4
DRAW = 0x846280  # thiscall (particle), no args
LIGHT = 0x801C90  # cdecl (point, out light, out land colour)
HAZE = 0x7FEB30  # cdecl (point, specular, colour in and out) -> specular
EFFECTS = 0x4875D0  # thiscall (3D creature), no args: the fart and the puke of each turn
CRT_RAND = 0x7C8837
LOCAL_RAND = 0x6DE570  # cdecl (limit)
GET_ANIM = 0x619650  # thiscall, 2 args
ANIM_TIME = 0x617A10  # thiscall, 3 args
SPRITE_DRAW = 0x840530  # thiscall (sprite)

# Globals
BLOCK_INDEX = 0xE9C964  # 32 x 32 bytes: the block of each 16 x 16 cells, 0 for none
BLOCK_POINTERS = 0xE9C564  # the blocks by their index
LIGHT_TABLE = 0xEDD90C  # 256 colours
INTERIOR = 0xEA9EB4  # set in the temple: the light comes from elsewhere
SEA_CLAMP = 0xC37BF4
SPRITE = 0xEF7534  # the shared sprite of the draw
MATERIALS_ON = 0xC38714
POOL = 0xEF753C  # the pool that the add goes to
PARTICLES_ON = 0xC38214
HAZE_ON = 0xC37204
HAZE_NEAR = 0xC37220
HAZE_FAR = 0xC37224
HAZE_RANGE = 0xE9B6DC
HAZE_K = 0xC37228
CLIP_X = 0xEA9E48
CLIP_Y = 0xEA9E54
CLIP_Z = 0xEA9E60
CLIP_W = 0xEA9E6C

BLOCKS = 4  # 4 x 4 blocks of 16 cells: 640 metres on each side
RECORD = 0x2C


def bits(value: float) -> int:
    return f2u(value)


def round32(value: float) -> float:
    return u2f(f2u(value))


class Land:
    """The corners: altitude 10 + x (a slope along x only, no split), luminosity from a hash of the corner."""

    @staticmethod
    def altitude(x: int, z: int) -> int:
        return 10 + x

    @staticmethod
    def luminosity(x: int, z: int) -> int:
        return (x * 37 + z * 91 + (x * z) % 13 + 5) & 0xFF

    @staticmethod
    def light(level: int) -> int:
        return 0xFF000000 | (((level * 7 + 13) & 0xFF) << 16) | (((level * 29 + 101) & 0xFF) << 8) | ((255 - level) * 3 & 0xFF)

    @staticmethod
    def lay(o: Oracle) -> None:
        index = 1
        for bx in range(BLOCKS):
            for bz in range(BLOCKS):
                block = o.alloc(17 * 17 * 8)
                cells = bytearray(17 * 17 * 8)
                for i in range(17):
                    for j in range(17):
                        x, z = bx * 16 + i, bz * 16 + j
                        cell = (i * 17 + j) * 8
                        cells[cell:cell + 8] = bytes([0x40, 0x80, 0x20, Land.luminosity(x, z), Land.altitude(x, z), 0, 0, 0])
                o.write(block, bytes(cells))
                o.write(BLOCK_POINTERS + index * 4, struct.pack("<I", block))
                o.write(BLOCK_INDEX + bx * 32 + bz, bytes([index]))
                index += 1
        o.write(LIGHT_TABLE, b"".join(struct.pack("<I", Land.light(i)) for i in range(256)))
        o.write_u32(INTERIOR, 0)
        o.write_u32(SEA_CLAMP, 1)


def step_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    Land.lay(o)
    particle = o.alloc(RECORD)
    rows = []
    for case in range(2400):
        kind = [0, 0, 0, 1, 2, 3, 4][case % 7]
        on_land = 1 if kind == 0 and rng.random() < 0.4 else 0
        x = rng.uniform(1.0, 630.0)
        z = rng.uniform(1.0, 630.0)
        land = (10 + x / 10.0) * 0.67
        y = land + rng.choice([rng.uniform(-0.5, 0.5), rng.uniform(0.0, 5.0), 0.0])
        velocity = [rng.uniform(-20.0, 20.0) for _ in range(3)]
        age = rng.choice([0.0, rng.uniform(0.0, 3.2), 0.999, 1.999, 2.999])
        size = rng.uniform(0.05, 3.0)
        seconds = rng.choice([0.01, 0.016, 0.033, rng.uniform(0.0, 0.2), 0.1])
        draw = rng.randrange(0, 0x8000)
        words = [bits(x), bits(y), bits(z)] + [bits(v) for v in velocity] + [kind, on_land, 0xFF102030, bits(age), bits(size)]
        o.write(particle, struct.pack("<11I", *words))
        o.stub(CRT_RAND, eax=draw)
        result = o.call(STEP, this=particle, args=[f32(seconds)])
        out = o.read_u32(particle, 11)
        rows.append(words[:6] + [kind, on_land, bits(age), bits(size), bits(seconds), draw, result.eax & 0xFF]
                    + out[0:6] + [out[7], out[9]])
    return rows


STEP_COLUMNS = ["x", "y", "z", "vx", "vy", "vz", "kind", "on_land", "age", "size", "seconds", "draw", "gone",
                "x_out", "y_out", "z_out", "vx_out", "vy_out", "vz_out", "on_land_out", "age_out"]


def draw_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    sprite = o.alloc(0x40)
    o.write_u32(SPRITE, sprite)
    o.write_u32(MATERIALS_ON, 0)
    o.stub(SPRITE_DRAW)
    particle = o.alloc(RECORD)
    rows = []
    for case in range(2000):
        kind = case % 5
        life = 3.0 if kind == 0 else (1.0 if kind == 4 else 2.0)
        age = rng.choice([0.0, rng.uniform(0.0, life), rng.uniform(0.0, 0.3), life * 0.125, 0.25])
        variant = rng.randrange(0, 16)
        size = rng.uniform(0.05, 12.0)
        draw = rng.randrange(0, 0x8000)
        words = [0, 0, 0, 0, 0, 0, kind, variant, 0x80406080, bits(age), bits(size)]
        o.write(particle, struct.pack("<11I", *words))
        o.write(sprite, bytes(0x40))
        o.stub(CRT_RAND, eax=draw)
        o.call(DRAW, this=particle)
        out = o.read_u32(sprite, 16)
        rows.append([kind, variant, bits(age), bits(size), draw, out[3], out[10] & 0x3F])
    return rows


def light_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    Land.lay(o)
    point = o.alloc(12)
    light = o.alloc(4)
    colour = o.alloc(4)
    rows = []
    for case in range(3000):
        if case % 10 == 0:
            x, z = rng.uniform(-50.0, 700.0), rng.uniform(-50.0, 700.0)
        elif case % 10 == 1:
            x, z = float(rng.randrange(0, 64)) * 10.0, rng.uniform(0.0, 630.0)
        else:
            x, z = rng.uniform(0.0, 639.9), rng.uniform(0.0, 639.9)
        o.write(point, struct.pack("<3I", bits(x), bits(rng.uniform(0.0, 50.0)), bits(z)))
        o.call(LIGHT, args=[point, light, colour])
        rows.append([bits(x), bits(z), o.read_u32(light)[0]])
    return rows


def haze_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    point = o.alloc(12)
    colour = o.alloc(4)
    rows = []
    for case in range(3000):
        on = 0 if case % 11 == 0 else 1
        near = round32(rng.uniform(100.0, 500.0))
        far = round32(near + rng.uniform(50.0, 800.0))
        span = round32(far - near)
        k = rng.randrange(0, 257)
        clip = [rng.uniform(-1.0, 1.0) for _ in range(3)] + [rng.uniform(-200.0, 200.0)]
        p = [rng.uniform(-1200.0, 1200.0) for _ in range(3)]
        c = rng.randrange(0, 1 << 32)
        o.write_u32(HAZE_ON, on)
        o.write(HAZE_NEAR, struct.pack("<I", bits(near)))
        o.write(HAZE_FAR, struct.pack("<I", bits(far)))
        o.write(HAZE_RANGE, struct.pack("<I", bits(span)))
        o.write_u32(HAZE_K, k)
        for address, value in zip((CLIP_X, CLIP_Y, CLIP_Z, CLIP_W), clip):
            o.write(address, struct.pack("<I", bits(value)))
        o.write(point, struct.pack("<3I", *[bits(v) for v in p]))
        o.write_u32(colour, c)
        o.call(HAZE, args=[point, 0, colour])
        rows.append([on, bits(near), bits(far), k] + [bits(v) for v in clip] + [bits(v) for v in p] + [c, o.read_u32(colour)[0]])
    return rows


HAZE_COLUMNS = ["on", "near", "far", "k", "cx", "cy", "cz", "cw", "x", "y", "z", "colour", "colour_out"]


def puke_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    Land.lay(o)
    pool = o.alloc(0xB01C)
    o.write_u32(POOL, pool)
    o.write_u32(PARTICLES_ON, 1)
    creature = o.alloc(0x5800)
    body = o.alloc(0x100)
    bones = o.alloc(48 * 8)
    o.write_u32(creature + 0x49A0, 0x23)
    o.write_u32(creature + 0x482C, body)
    o.write_u32(creature + 0x545C, 0)
    o.write_u32(creature + 0x5270, 0)
    o.write_u32(creature + 0x5178, bones)
    o.write_u32(creature + 0x51B8, 3)
    o.stub(GET_ANIM, pop=8)
    o.stub(ANIM_TIME, pop=12)
    rows = []
    for case in range(1500):
        size = rng.choice([rng.uniform(0.3, 3.0), 1.0, 2.0])
        draw = rng.randrange(0, 100)
        place = [rng.uniform(1.0, 630.0), rng.uniform(0.0, 60.0), rng.uniform(1.0, 630.0)]
        head = [rng.uniform(-50.0, 700.0) for _ in range(3)]
        on = 0 if case % 3 == 0 else 1
        near = round32(rng.uniform(100.0, 400.0))
        far = round32(near + rng.uniform(100.0, 600.0))
        k = rng.randrange(0, 257)
        clip = [rng.uniform(-1.0, 1.0) for _ in range(3)] + [rng.uniform(-200.0, 200.0)]
        o.write_u32(HAZE_ON, on)
        o.write(HAZE_NEAR, struct.pack("<I", bits(near)))
        o.write(HAZE_FAR, struct.pack("<I", bits(far)))
        o.write(HAZE_RANGE, struct.pack("<I", bits(round32(far - near))))
        o.write_u32(HAZE_K, k)
        for address, value in zip((CLIP_X, CLIP_Y, CLIP_Z, CLIP_W), clip):
            o.write(address, struct.pack("<I", bits(value)))
        o.write(creature + 0x90, struct.pack("<I", bits(size)))
        o.write(body + 0x38, struct.pack("<3I", *[bits(v) for v in place]))
        o.write(bones + 3 * 48 + 0x24, struct.pack("<3I", *[bits(v) for v in head]))
        o.write_u32(pool + 0xB000, 0)
        o.stub(LOCAL_RAND, eax=draw)
        o.call(EFFECTS, this=creature)
        count = o.read_u32(pool + 0xB000)[0]
        drop = o.read_u32(pool, 11)
        last = o.read_u32(pool + (count - 1) * RECORD, 11) if count > 0 else [0] * 11
        if last[3:6] != drop[3:6] or last[8] != drop[8]:
            raise RuntimeError("the drops of one turn differ with one draw")
        rows.append([bits(size), draw] + [bits(v) for v in place] + [bits(v) for v in head]
                    + [on, bits(near), bits(far), k] + [bits(v) for v in clip]
                    + [count] + drop[0:9] + [drop[10]])
    return rows


PUKE_COLUMNS = ["size", "draw", "px", "py", "pz", "hx", "hy", "hz", "on", "near", "far", "k", "cx", "cy", "cz", "cw",
                "count", "x_out", "y_out", "z_out", "vx_out", "vy_out", "vz_out", "kind_out", "variant_out", "argb_out",
                "size_out"]


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        n = write_pinned(out / "liquid_step.txt", "One step of a liquid particle",
                         ["The game's step of a particle of the pool, its land the slope of Land (altitude 10 + x a",
                          "corner, no split), the C library's random number fixed at draw for the call. Floats as",
                          "bits; kind, on_land, gone and draw as integers. gone is 1 when the particle went."],
                         STEP_COLUMNS, step_cases(oracle, random.Random(7301)))
        print(f"liquid_step: {n} rows")
        oracle.reset()
        n = write_pinned(out / "liquid_draw.txt", "The size and the frame of a liquid particle as the game draws it",
                         ["The game's draw of a particle, the material and the sprite draw left out. Out: the size",
                          "(float bits) and the frame (0 to 63) that it gives the sprite. draw is the C library's",
                          "random number of the call."],
                         ["kind", "variant", "age", "size", "draw", "size_out", "frame_out"],
                         draw_cases(oracle, random.Random(7302)))
        print(f"liquid_draw: {n} rows")
        oracle.reset()
        n = write_pinned(out / "land_light.txt", "The land's light at a point",
                         ["The game's light of the land at a point, the land of Land (luminosity of a corner",
                          "(x 37 + z 91 + (x z) mod 13 + 5) & 255, light of a level from the formula of Land.light),",
                          "4 x 4 blocks of 16 cells. x and z as float bits, the light as 0xAARRGGBB."],
                         ["x", "z", "light"], light_cases(oracle, random.Random(7303)))
        print(f"land_light: {n} rows")
        oracle.reset()
        n = write_pinned(out / "land_haze.txt", "A colour as the distance haze dims it",
                         ["The game's haze on a colour: on, near, far (the span far - near rounded to a float), k,",
                          "the clip matrix's depth column (x, y, z, w), the point, the colour in and out."],
                         HAZE_COLUMNS, haze_cases(oracle, random.Random(7304)))
        print(f"land_haze: {n} rows")
        oracle.reset()
        n = write_pinned(out / "creature_puke.txt", "The drops of one turn of a creature's puke",
                         ["The game's effects of a 3D creature that plays the puke loop: the light at its place",
                          "(the land of Land, the light formula), the haze, the head bone as the lips, the local",
                          "random numbers fixed at draw, the drops in the real pool. Out: the count and the first",
                          "drop (point, speed, kind, frame, colour, size); every drop of a call is the same."],
                         PUKE_COLUMNS, puke_cases(oracle, random.Random(7305)))
        print(f"creature_puke: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
