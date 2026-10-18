#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the land's shaded luminosity, in test/oracle/data:

- land_shade.txt: shadows and lights stamped on a land, then taken off again
- land_shade_warm.txt: a light image's texels at the land light's warm levels

The land has 27 blocks in three groups of 3 x 3: at the map's first corner, in the middle and at the map's last corner,
so that the stamps meet the map's edges. The luminosity of each cell comes from a formula of its map cell (Land below),
the same in every copy of a shared cell. A row has the blocks in view (a bit for each block), up to three shadows (the
game's shadow list, applied as it applies it each frame) and up to two lights (the game's light stamp, in the row's
order), and the results: a hash of every block's cells after the stamps and after the game took them off again.

Usage: python tools/oracle/cases/land_shade_cases.py [output folder]   (default: test/oracle/data)
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
ADD_STAMP = 0x86CFF0  # fastcall (ECX point, EDX image), args size, centred, strength, mode, last; RET 0x14
APPLY_STAMPS = 0x86D360  # cdecl, no args: the shadow list onto the land, then the cells' colour list
CLEAR_STAMPS = 0x86D460  # cdecl, no args: the blocks under the shadow list back as laid, then the colour list
LIGHT = 0x8229B0  # fastcall (ECX image, EDX side), args x, z, strength
UNLIGHT = 0x822F90  # fastcall (ECX side), args x, z
VILLAGE_LIGHTS = 0x823690  # cdecl: loads and converts the village light image on its first call, then stamps each light

# Globals
BLOCK_INDEX = 0xE9C964  # 32 x 32 bytes: the block of each 16 x 16 cells, 0 for none
BLOCK_POINTERS = 0xE9C564  # the blocks by their index
FULL_LIGHT = 0xEDDD08  # the land light table's brightest colour (its green is read)
COLOUR_LIST_COUNT = 0xEB99A4
LIGHT_LIST = 0xEB99B8
VILLAGE_IMAGE = 0xEB99C8

# What the village light load calls
FILE_LENGTH = 0x7BC960
ALLOC = 0x842CE0
LOAD_DATA = 0x7BCDF0
BRIGHTNESS = 0x86C220

BLOCK_SIZE = 0x930
GROUPS = [(0, 0), (14, 14), (29, 29)]
BLOCKS = [(gx + i, gz + j) for gx, gz in GROUPS for i in range(3) for j in range(3)]
SHADOWS = 3
LIGHTS = 2


class Land:
    @staticmethod
    def luminosity(x: int, z: int) -> int:
        return (x * 37 + z * 91 + (x * z) % 13 + 5) & 0xFF

    @staticmethod
    def cells(bx: int, bz: int) -> bytes:
        cells = bytearray(17 * 17 * 8)
        for i in range(17):
            for j in range(17):
                lum = Land.luminosity(bx * 16 + i, bz * 16 + j)
                at = (i * 17 + j) * 8
                cells[at:at + 8] = bytes([0, 0, 0, lum, 0, lum, 0, 0])
        return bytes(cells)


def image(seed: int, side: int, warm: bool) -> bytes:
    """The texels of a stamp's image from a seed, as the test makes them again"""
    out = bytearray(side * side)
    x = seed & 0xFFFFFFFF
    for i in range(side * side):
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
        out[i] = (x >> 24) % 48 if warm else x >> 24
    return bytes(out)


def fnv(data: bytes) -> int:
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def f32bits(value: float) -> int:
    return f2u(value)


def shade_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    blocks = []
    for index, (bx, bz) in enumerate(BLOCKS, start=1):
        block = o.alloc(BLOCK_SIZE)
        o.write(BLOCK_POINTERS + index * 4, struct.pack("<I", block))
        o.write(BLOCK_INDEX + bx * 32 + bz, bytes([index]))
        o.write_u32(block + 0x914, bx, bz)
        blocks.append(block)
    laid = [Land.cells(bx, bz) for bx, bz in BLOCKS]
    o.write_u32(COLOUR_LIST_COUNT, 0)
    point = o.alloc(12)
    images = o.alloc(64 * 64)
    rows = []
    for case in range(1500):
        for block, cells in zip(blocks, laid):
            o.write(block, cells)
        view = rng.getrandbits(27) if case % 4 else (1 << 27) - 1
        for i, block in enumerate(blocks):
            o.write_u32(block + 0x920, 4 if view >> i & 1 else 0)
        green = rng.randrange(256)
        o.write_u32(FULL_LIGHT, 0xFF300010 | (green << 8))
        row = [view, green]
        offset = 0
        for s in range(SHADOWS):
            present = 1 if rng.random() < 0.8 else 0
            group = rng.choice(GROUPS)
            side = 40 if rng.random() < 0.6 else rng.randrange(2, 41)
            x = f32bits((group[0] * 16 + rng.uniform(-45.0, 52.0)) * 10.0)
            z = f32bits((group[1] * 16 + rng.uniform(-45.0, 52.0)) * 10.0)
            if rng.random() < 0.1:
                x = f32bits(float(rng.randrange(-40, 50) + group[0] * 16) * 10.0)
            centred = rng.randrange(2)
            pick = rng.random()
            if pick < 0.4:
                strength = f32bits(float(rng.randrange(256)) * u2f(0x3B808081))
            elif pick < 0.9:
                strength = f32bits(rng.uniform(-0.2, 1.3))
            else:
                strength = f32bits(rng.choice([0.0, 1.0, 0.5, 1.0 / 255.0, 2.0]))
            seed = rng.getrandbits(32)
            row += [present, x, z, side, centred, strength, seed]
            if present:
                at = images + offset
                o.write(at, image(seed, side, False))
                offset += side * side
                o.write_u32(point, x, 0, z)
                o.call(ADD_STAMP, ecx=point, edx=at, args=[side, centred, strength, 2, 0])
        o.call(APPLY_STAMPS)
        lights = []
        for _ in range(LIGHTS):
            present = 1 if rng.random() < 0.7 else 0
            group = rng.choice(GROUPS)
            side = rng.choice([12, 14, 14, rng.randrange(2, 21)])
            x = f32bits((group[0] * 16 + rng.uniform(-25.0, 52.0)) * 10.0)
            z = f32bits((group[1] * 16 + rng.uniform(-25.0, 52.0)) * 10.0)
            if rng.random() < 0.15:
                z = f32bits(float(group[1] * 16 + rng.randrange(-20, 50)) * 10.0)
            strength = f32bits(rng.choice([rng.uniform(-0.1, 1.2), float(rng.randrange(256)) * u2f(0x3B808081), 1.0]))
            warm = rng.randrange(2)
            seed = rng.getrandbits(32)
            row += [present, x, z, side, strength, warm, seed]
            if present:
                at = images + offset
                o.write(at, image(seed, side, warm == 1))
                offset += side * side
                o.call(LIGHT, ecx=at, edx=side, args=[x, z, strength])
                lights.append((side, x, z))
        shaded = b"".join(o.read(block, 17 * 17 * 8)[3::8] for block in blocks)
        # The game takes the lights off first, then the shadows
        for side, x, z in lights:
            o.call(UNLIGHT, ecx=side, args=[x, z])
        o.call(CLEAR_STAMPS)
        restored = b"".join(o.read(block, 17 * 17 * 8)[3::8] for block in blocks)
        changed = sum(1 for a, b in zip(shaded, b"".join(c[3::8] for c in laid)) if a != b)
        row += [fnv(shaded), fnv(restored), changed]
        rows.append(row)
    return rows


def shade_columns() -> list[str]:
    columns = ["view", "green"]
    for s in range(SHADOWS):
        columns += [f"s{s}_on", f"s{s}_x", f"s{s}_z", f"s{s}_side", f"s{s}_centred", f"s{s}_strength", f"s{s}_seed"]
    for light in range(LIGHTS):
        columns += [f"l{light}_on", f"l{light}_x", f"l{light}_z", f"l{light}_side", f"l{light}_strength",
                    f"l{light}_warm", f"l{light}_seed"]
    return columns + ["shaded_hash", "restored_hash", "changed"]


def warm_cases(o: Oracle) -> list[list[int]]:
    """The village light image's load converts each texel: give it every byte value as the file's texels"""
    rows = []
    entry = o.alloc(0x28)
    o.write_u32(LIGHT_LIST, entry)
    buffer = o.alloc(0x100)
    o.stub(FILE_LENGTH)
    o.stub(ALLOC, eax=buffer)
    o.stub(LOAD_DATA)
    o.stub(BRIGHTNESS, st0=0.0)
    o.stub(LIGHT, pop=0xC)
    for start in range(0, 256, 0xC4):
        values = bytes((start + i) & 0xFF for i in range(0xC4))
        o.write(buffer, values)
        o.write_u32(VILLAGE_IMAGE, 0)
        o.call(VILLAGE_LIGHTS)
        out = o.read(buffer, 0xC4)
        for i in range(0xC4):
            if start + i < 256:
                rows.append([start + i, out[i]])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        n = write_pinned(out / "land_shade.txt", "Shadows and lights stamped on the land, then taken off",
                         ["The game's shadow list (mode 2, the clouds' darkening) and light stamp on 27 blocks in three",
                          "groups of 3 x 3 at blocks (0, 0), (14, 14) and (29, 29). A cell's luminosity is",
                          "(x 37 + z 91 + (x z) mod 13 + 5) & 255 of its map cell. view: a bit for each block in view,",
                          "in the order of the groups, then x, then z. Images from the seed (see image()). Floats as",
                          "bits. Out: FNV-1a 32 of every block's 289 luminosity bytes after the stamps and after the",
                          "game took them off, and the count of bytes that the stamps changed."],
                         shade_columns(), shade_cases(oracle, random.Random(7401)))
        print(f"land_shade: {n} rows")
        oracle.reset()
        n = write_pinned(out / "land_shade_warm.txt", "A light image's texels at the land light's warm levels",
                         ["The game's load of the village light image, which converts every texel. In: the texel.",
                          "Out: its level."],
                         ["texel", "level"], warm_cases(oracle))
        print(f"land_shade_warm: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
