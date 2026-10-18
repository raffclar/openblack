#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the particles' stamps on the land, in test/oracle/data:

- land_shade_particle.txt: the stamp that a particle's light map or mist shadow adds to the land's list each frame
- land_shade_particle_strength.txt: a particle stamp's strength from the alpha of the particle's colour

The game keeps one list of the particles' light maps and the mists' shadows, in the order they were drawn. At the
start of the next drawn frame each one adds a stamp to the land's list (see land_shade_cases.py). A row of the first
file gives a list item (its point, its strength, its frame) and its image (grey or colour, side, frames), calls the
game's function for the item and reads the stamp that it added: the corner, the image's first texel, the side, the
alpha and the mode (1: colour, 2: shadow).

The second file calls the game's light map draw on a particle with no image, which only keeps the strength from the
colour's alpha (the mist's draw has the same instructions and then holds the strength between 0 and 1).

Usage: python tools/oracle/cases/land_shade_particle_cases.py [output folder]   (default: test/oracle/data)
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
PARTICLE_STAMP = 0x6CA280  # fastcall (ECX item): the item's stamp to the land's list
LIGHT_MAP_DRAW = 0x67B220  # thiscall (draw data): the light map particle's strength, then its item to the list

# Globals
STAMP_LIST = 0xFA2920  # records of 0x34
STAMP_COUNT = 0xFA51C0
RECORD = 0x34


def particle_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    item = o.alloc(0x20)
    bitmap = o.alloc(0x20)
    pixels = 0x100000
    rows = []
    for case in range(2000):
        o.write_u32(STAMP_COUNT, 0)
        kind = rng.choice([1, 1, 1, 3, 3, 0, 2]) if case % 50 else 1
        side = rng.choice([16, 16, 12, 32, rng.randrange(1, 65)])
        frames = rng.choice([1, 1, 4, 8, rng.randrange(1, 20)])
        frame = rng.choice([0, 0, rng.randrange(0, 40), rng.randrange(0, 0x10000)])
        has_data = 0 if rng.random() < 0.03 else 1
        x = f2u(rng.uniform(-200.0, 5300.0))
        y = f2u(rng.uniform(-50.0, 400.0))
        z = f2u(rng.uniform(-200.0, 5300.0))
        if rng.random() < 0.1:
            x = f2u(float(rng.randrange(-30, 520)) * 10.0 + rng.choice([0.0, 5.0, 9.99]))
        pick = rng.random()
        if pick < 0.6:
            strength = f2u(float(rng.randrange(256)) * u2f(0x3B808081))
        elif pick < 0.9:
            strength = f2u(rng.uniform(-0.2, 1.3))
        else:
            strength = f2u(rng.choice([0.0, 1.0, 0.5, 2.0, -1.0]))
        o.write(bitmap, bytes([kind, 0, 0, 0]))
        o.write_u32(bitmap + 0x4, pixels if has_data else 0)
        o.write_u32(bitmap + 0xC, side)
        o.write(bitmap + 0x10, struct.pack("<H", frames))
        o.write(item + 0x8, struct.pack("<H", frame))
        o.write_u32(item + 0xC, x, y, z)
        o.write_u32(item + 0x18, strength, bitmap)
        o.call(PARTICLE_STAMP, ecx=item)
        added = o.read_u32(STAMP_COUNT)[0]
        row = [kind, side, frames, frame, has_data, x, y, z, strength, added]
        if added:
            record = STAMP_LIST
            rx, ry, rz, image, rside = o.read_u32(record, 5)
            alpha = o.read_u32(record + 0x2C)[0]
            mode = o.read_u32(record + 0x30)[0]
            row += [rx, ry, rz, (image - pixels) & 0xFFFFFFFF, rside, alpha, mode]
        else:
            row += [0, 0, 0, 0, 0, 0, 0]
        rows.append(row)
    return rows


def particle_columns() -> list[str]:
    return ["kind", "side", "frames", "frame", "has_data", "x", "y", "z", "strength", "added", "corner_x", "corner_y",
            "corner_z", "first_texel", "stamp_side", "alpha", "mode"]


def strength_cases(o: Oracle) -> list[list[int]]:
    this = o.alloc(0x50)
    matrix = o.alloc(0x30)
    draw = o.alloc(0x14)
    rows = []
    for alpha in range(256):
        o.write(this, bytes(0x50))
        o.write_u32(draw + 0x4, matrix)
        o.write(draw + 0x8, bytes([0x40, 0x80, 0xC0, alpha]))
        o.call(LIGHT_MAP_DRAW, this=this, args=[draw])
        rows.append([alpha, o.read_u32(this + 0x44)[0]])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        n = write_pinned(out / "land_shade_particle.txt", "A particle's stamp on the land",
                         ["The game's stamp of an item of the particles' list (a light map or a mist's shadow).",
                          "In: the image's kind (bytes a texel: 1 grey, 3 colour, others none), side, frames,",
                          "whether it has texels, the item's frame, point and strength (floats as bits).",
                          "Out: whether a stamp was added, its corner (x, y, z as bits), its first texel's byte",
                          "offset in the image, its side, alpha (of 255) and mode (1 colour, 2 shadow)."],
                         particle_columns(), particle_cases(oracle, random.Random(7621)))
        print(f"land_shade_particle: {n} rows")
        oracle.reset()
        n = write_pinned(out / "land_shade_particle_strength.txt", "A particle stamp's strength from its alpha",
                         ["The game's light map draw keeps the strength of its stamp from the alpha byte of the",
                          "particle's colour. In: the alpha. Out: the strength as float bits."],
                         ["alpha", "strength"], strength_cases(oracle))
        print(f"land_shade_particle_strength: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
