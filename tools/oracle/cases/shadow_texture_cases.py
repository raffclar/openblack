#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the texture of a moving body's shadow through its texture's alpha, in test/oracle/data:

- shadow_texture_fill.txt: one primitive of six vertices and four triangles. The game's own code flattens the
  vertices from a light straight above the body onto the grid of 32 by 32 texels and fills the triangles into the
  texels through the alpha mask. The row gives the inputs and the 1024 texels after the fill, two to a word.

The mask is the same formula in the test: place i holds ((i * 37 + (i >> 6) * 11) & 15) << 4. The texture coordinates
stay from 0 to 1, so that the fill reads only the mask. Some triangles are flat or thin, to show how the game keeps
the crossings of the edges from one triangle to the next.

Usage: python tools/oracle/cases/shadow_texture_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code and data (BW1W120, runblack.exe 1.20)
PRIMITIVE_FILL = 0x84B7D0  # thiscall (primitive; unused, rasteriser, shadow entry)
DRAWN_OBJECT = 0xEA1AE8  # the object whose matrix moves the vertices
DRAWN_SUBMESH = 0xFA93BC  # the submesh: its first flag bit says that bones move it
VERTICES = 6
TRIANGLES = 4
TEXELS = 32
HEIGHT = 15000.0


def to_f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def mask_bytes() -> bytes:
    return bytes((((i * 37) + ((i >> 6) * 11)) & 15) << 4 for i in range(64 * 64))


def main(argv: list[str]) -> int:
    folder = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x5AD0)
    rows = []
    with Oracle() as o:
        mask = o.alloc(64 * 64, mask_bytes())
        texture = o.object(0x200, {0x12C: mask})
        buffer = o.alloc(TEXELS * TEXELS * 2)
        desc = o.object(0x20, {0x4: buffer, 0x8: mask, 0xC: TEXELS, 0x10: TEXELS})
        down = o.alloc(TEXELS * 12)
        up = o.alloc(TEXELS * 12)
        state = o.object(0x20, {0x0: desc, 0x10: down, 0x14: up})
        submesh = o.alloc(0x40)
        obj = o.alloc(0x80)
        entry = o.alloc(0x4B0)
        vertices = o.alloc(VERTICES * 0x20)
        indices = o.alloc(TRIANGLES * 6)
        primitive = o.object(0x40, {0x8: texture, 0x10: VERTICES, 0x14: vertices, 0x18: TRIANGLES, 0x1C: indices})
        o.write_u32(DRAWN_OBJECT, obj)
        o.write_u32(DRAWN_SUBMESH, submesh)
        for case in range(80):
            scale = to_f32(rng.uniform(0.5, 2.5))
            angle = rng.uniform(-3.1, 3.1)
            tilt = rng.uniform(-1.2, 1.2)
            ca, sa, ct, st = math.cos(angle), math.sin(angle), math.cos(tilt), math.sin(tilt)
            # Rows: x axis, y axis, z axis, then the place
            matrix = [ca * scale, 0.0, -sa * scale,
                      sa * st * scale, ct * scale, ca * st * scale,
                      sa * ct * scale, -st * scale, ca * ct * scale]
            place = [rng.uniform(200.0, 4000.0), rng.uniform(0.0, 120.0), rng.uniform(200.0, 4000.0)]
            matrix = [to_f32(v) for v in matrix + place]
            light = [matrix[9], to_f32(matrix[10] + HEIGHT), matrix[11]]
            level = matrix[10]
            reach = 6.0 * scale
            low_x = to_f32(place[0] - reach * rng.uniform(0.6, 1.2))
            high_x = to_f32(place[0] + reach * rng.uniform(0.6, 1.2))
            low_z = to_f32(place[2] - reach * rng.uniform(0.6, 1.2))
            high_z = to_f32(place[2] + reach * rng.uniform(0.6, 1.2))
            points = []
            for _ in range(VERTICES):
                points.append([to_f32(rng.uniform(-5.0, 5.0)), to_f32(rng.uniform(-1.0, 8.0)),
                               to_f32(rng.uniform(-5.0, 5.0)), to_f32(rng.random()), to_f32(rng.random())])
            if case % 4 == 1:
                # A flat triangle: two corners at one height and depth
                points[1][2] = points[0][2]
                points[1][1] = points[0][1]
            if case % 4 == 2:
                # Corners on top of each other
                points[2] = list(points[0])
            triangles = [[rng.randrange(VERTICES) for _ in range(3)] for _ in range(TRIANGLES)]
            triangles[0] = [0, 1, 2]
            o.write(obj + 0x14, b"".join(struct.pack("<f", v) for v in matrix))
            o.write(entry, bytes(0x4B0))
            o.write_f32(entry + 0x18, level)
            o.write_f32(entry + 0x1C, low_x, high_x, low_z, high_z)
            o.write_f32(entry + 0x440, 1e20)
            o.write_f32(entry + 0x444, *light)
            o.write_f32(entry + 0x450, 0.0, -HEIGHT, 0.0)
            o.write(vertices, b"".join(struct.pack("<8f", p[0], p[1], p[2], p[3], p[4], 0.0, 1.0, 0.0) for p in points))
            o.write(indices, b"".join(struct.pack("<3H", *t) for t in triangles))
            o.write(buffer, bytes(TEXELS * TEXELS * 2))
            o.write(down, bytes(TEXELS * 12))
            o.write(up, bytes(TEXELS * 12))
            o.call(PRIMITIVE_FILL, this=primitive, args=[0, state, entry])
            texels = o.read_u32(buffer, TEXELS * TEXELS // 2)
            row = [f2u(v) for v in matrix] + [f2u(level), f2u(low_x), f2u(high_x), f2u(low_z), f2u(high_z)]
            row += [f2u(v) for v in light]
            for p in points:
                row += [f2u(v) for v in p]
            for t in triangles:
                row += t
            rows.append(row + texels)
    columns = ([f"m{i}" for i in range(12)] + ["level", "lowx", "highx", "lowz", "highz", "lightx", "lighty", "lightz"] +
               [f"p{i}{k}" for i in range(VERTICES) for k in "xyzuv"] +
               [f"t{i}{k}" for i in range(TRIANGLES) for k in "abc"] + [f"texels{i}" for i in range(512)])
    count = write_pinned(folder / "shadow_texture_fill.txt",
                         "The texture of a moving shadow filled through the alpha mask",
                         ["Matrix rows x, y, z axes then the place; the light is straight above the place.",
                          "Mask place i holds ((i * 37 + (i >> 6) * 11) & 15) << 4.",
                          "Texels by rows, two 16-bit texels to a word, the first in the low half."], columns, rows)
    print(f"shadow_texture_fill.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
