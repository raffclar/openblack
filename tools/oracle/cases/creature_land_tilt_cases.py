#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the creature's placement with the land tilt in test/oracle/data:

- creature_land_tilt.txt: the game's placement matrix of a creature (twelve values p0..p11: three axes, then the
  position) from its position (x, y, z), heading, scale, radius and whether another creature holds it (held). The land
  is a test height function h = ((x * x) * kx) + (z * kz), each step a float, so the test makes the same heights. The
  columns cos and sin are the game's cosine and sine of the heading, as it stores them in the matrix.

Usage: python tools/oracle/cases/creature_land_tilt_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The function (BW1W120, runblack.exe 1.20): thiscall (creature) (matrix out); RET 4
PLACEMENT = 0x4EBE10
OBJECT_SIZE = 0x5300
HEIGHT_SLOT = 0x28

COLUMNS = ["x", "y", "z", "heading", "scale", "radius", "held", "kx", "kz", "cos", "sin"] + [f"p{i}" for i in range(12)]


def bits(value: float) -> int:
    return f2u(struct.unpack("<f", struct.pack("<f", value))[0])


def height_code(kx_address: int, kz_address: int) -> bytes:
    """thiscall height(point): ((x * x) * kx) + (z * kz) on the FPU; RET 4"""
    return b"".join([
        bytes([0x8B, 0x44, 0x24, 0x04]),  # mov eax, [esp + 4]
        bytes([0xD9, 0x00]),  # fld dword [eax]
        bytes([0xD8, 0x08]),  # fmul dword [eax]
        bytes([0xD8, 0x0D]) + struct.pack("<I", kx_address),  # fmul dword [kx]
        bytes([0xD9, 0x40, 0x08]),  # fld dword [eax + 8]
        bytes([0xD8, 0x0D]) + struct.pack("<I", kz_address),  # fmul dword [kz]
        bytes([0xDE, 0xC1]),  # faddp st1
        bytes([0xC2, 0x04, 0x00]),  # ret 4
    ])


def cases() -> list[tuple[float, ...]]:
    """(x, y, z, heading, scale, radius, held, kx, kz): flat land, gentle and steep slopes, the clamp edges, a creature
    that another creature holds, a radius of 0, then random cases"""
    chosen = [
        (1000.0, 20.0, 1500.0, 0.0, 1.0, 5.0, 0, 0.0, 0.0),
        (1000.0, 20.0, 1500.0, 0.7, 1.3, 5.0, 0, 0.0, 0.05),
        (1000.0, 20.0, 1500.0, -2.1, 0.8, 7.5, 0, 0.00001, -0.02),
        (500.0, 3.0, 800.0, 3.1415927, 2.0, 12.0, 0, 0.001, 0.4),
        (500.0, 3.0, 800.0, 1.5707964, 2.0, 12.0, 0, -0.001, -0.4),
        (100.0, 3.0, 100.0, 0.3, 1.0, 4.0, 0, 0.0025, 0.5),  # the x slope at the top edge: 0.5 x 0.6 = 0.3
        (100.0, 3.0, 100.0, 0.3, 1.0, 4.0, 0, -0.0025, -0.5),
        (1200.0, 40.0, 900.0, 0.9, 1.0, 6.0, 1, 0.01, 0.3),  # held: flat
        (1200.0, 40.0, 900.0, 0.9, 1.0, 0.0, 0, 0.01, 0.3),  # a radius of 0
        (1200.0, 40.0, 900.0, 0.9, 1.0, 0.0, 1, 0.0, 0.0),
    ]
    rng = random.Random(0x7117)
    randoms = []
    for _ in range(300):
        randoms.append((rng.uniform(0.0, 5120.0), rng.uniform(-5.0, 200.0), rng.uniform(0.0, 5120.0),
                        rng.uniform(-2 * math.pi, 2 * math.pi), rng.uniform(0.1, 4.0), rng.uniform(0.5, 20.0),
                        1 if rng.random() < 0.05 else 0, rng.uniform(-0.0002, 0.0002), rng.uniform(-0.6, 0.6)))
    return chosen + randoms


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rows = []
    with Oracle() as o:
        constants = o.alloc(0x10)
        code = o.alloc(0x40, height_code(constants, constants + 4))
        vtable = o.alloc(0x100)
        o.write_u32(vtable + HEIGHT_SLOT, code)
        creature = o.alloc(OBJECT_SIZE)
        matrix = o.alloc(0x40)
        for x, y, z, heading, scale, radius, held, kx, kz in cases():
            values = [bits(v) for v in (x, y, z, heading, scale, radius)]
            kxb, kzb = bits(kx), bits(kz)
            o.write_u32(constants, kxb, kzb)
            o.write(creature, bytes(OBJECT_SIZE))
            o.write_u32(creature, vtable)
            o.write_u32(creature + 0x78, *values[0:4])
            o.write_u32(creature + 0x94, values[4])
            o.write_u32(creature + 0x5228, values[5])
            # The game's cosine and sine of the heading: a held creature at scale 1 keeps them in its matrix
            o.write_u32(creature + 0x51EC, 1)
            o.write_u32(creature + 0x94, bits(1.0))
            o.write(matrix, bytes(0x40))
            o.call(PLACEMENT, this=creature, args=[matrix])
            cosine, = o.read_u32(matrix, 1)
            sine, = o.read_u32(matrix + 8, 1)
            # The case itself
            o.write_u32(creature + 0x51EC, held)
            o.write_u32(creature + 0x94, values[4])
            o.write(matrix, bytes(0x40))
            o.call(PLACEMENT, this=creature, args=[matrix])
            placed = o.read_u32(matrix, 12)
            rows.append([*values, held, kxb, kzb, cosine, sine, *placed])
    from oracle import write_pinned  # noqa: E402

    count = write_pinned(
        out / "creature_land_tilt.txt",
        "The game's placement matrix of a creature, leaned with the slope of the land under it",
        [
            "land: h = ((x * x) * kx) + (z * kz), each step a float (a test function in place of the island)",
            "cos, sin: the game's cosine and sine of the heading, as stored in its matrix",
            "p0..p11: the matrix, three rows (the x, up and z axes of the body) and then the position",
        ],
        COLUMNS,
        rows,
    )
    print(f"creature_land_tilt.txt: {count} rows")


if __name__ == "__main__":
    main()
