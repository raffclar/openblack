#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the turn of a placed thing in test/oracle/data:

- placed_turn.txt: the game's turn from the angles about x, y and z (columns x, y, z), as nine values m0..m8 (three
  rows). Then the same turn with a scale in it (column scale) as the game builds the matrix of a placed thing, as nine
  values s0..s8. Then the angles that the game takes off the turn m0..m8 (columns ax, ay, az).

Usage: python tools/oracle/cases/placed_turn_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
TURN_MATRIX = 0x7FAC10  # thiscall (matrix) (y, x, z): the turn about y, then x, then z; RET 0xC
PLACE_3D_OBJECT = 0x63B680  # fastcall-like: ECX the 3D object, EDX the map place, then (x, y, z, scale); RET 0x10
LAND_ALTITUDE = 0x803090  # thiscall (map place) -> ST0: stubbed to 0
ANGLES_OF_TURN = 0x7FAB30  # thiscall (matrix) (&y, &x, &z); RET 0xC

COLUMNS = (["x", "y", "z", "scale"] + [f"m{i}" for i in range(9)] + [f"s{i}" for i in range(9)]
           + ["ax", "ay", "az"])


def f32_bits(value: float) -> int:
    return f2u(struct.unpack("<f", struct.pack("<f", value))[0])


def cases() -> list[tuple[float, float, float, float]]:
    """Chosen angles (Land 1's dead trees, leans with a turn, whole turns, edges), then random angles"""
    chosen = [
        (0.0, 0.0, 0.0, 1.0),
        (0.0, 0.7, 0.0, 1.0),
        (0.0, -2.9, 0.0, 1.3),
        (0.0, 3.1415927, 0.0, 1.0),
        (-0.007930, -0.000242, -0.055236, 1.0),
        (0.039619, -0.000025, 0.001211, 1.0),
        (-0.008450, 0.000137, 0.033041, 1.0),
        (0.5235988, 0.7853982, 0.0, 1.0),
        (0.0, 0.7853982, 0.5235988, 1.0),
        (0.3490659, 0.7853982, 0.3490659, 0.8),
        (0.4363323, 0.7853982, -0.2617994, 2.5),
        (1.5707964, 0.3, 0.2, 1.0),
        (-1.5707964, -0.3, 0.2, 1.0),
        (2.0, 4.0, -5.0, 0.37),
        (0.2, 0.7, -0.4, 1.0),
    ]
    rng = random.Random(0x7A11)
    randoms = [(rng.uniform(-math.pi, math.pi), rng.uniform(-2 * math.pi, 2 * math.pi), rng.uniform(-math.pi, math.pi),
                rng.choice([1.0, rng.uniform(0.1, 4.0)])) for _ in range(200)]
    return chosen + randoms


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rows = []
    with Oracle() as o:
        o.stub(LAND_ALTITUDE, pop=0, st0=0.0)
        matrix = o.alloc(0x40)
        place = o.alloc(0x10)
        object3d = o.alloc(0x80)
        angles = o.alloc(0x10)
        for x, y, z, scale in cases():
            xb, yb, zb, sb = (f32_bits(v) for v in (x, y, z, scale))
            o.write(matrix, bytes(0x40))
            o.call(TURN_MATRIX, this=matrix, args=[f32.of_bits(yb), f32.of_bits(xb), f32.of_bits(zb)])
            turn = o.read_u32(matrix, 9)
            o.write(object3d, bytes(0x80))
            o.call(PLACE_3D_OBJECT, ecx=object3d, edx=place,
                   args=[f32.of_bits(xb), f32.of_bits(yb), f32.of_bits(zb), f32.of_bits(sb)])
            scaled = o.read_u32(object3d + 0x14, 9)
            # The angles come off the plain turn, with no place in its fourth row
            o.write(angles, bytes(0x10))
            o.call(ANGLES_OF_TURN, this=matrix, args=[angles + 4, angles, angles + 8])
            ax, ay, az = o.read_u32(angles, 3)
            rows.append([xb, yb, zb, sb, *turn, *scaled, ax, ay, az])
    count = write_pinned(
        out / "placed_turn.txt",
        "The game's turn of a placed thing from its angles, with a scale, and the angles taken off the turn",
        [
            "m: the turn about y, then x, then z, from the angles x, y, z in radians (three rows of three)",
            "s: the matrix of a placed thing: the same turn, each value multiplied by the scale",
            "ax, ay, az: the angles that the game takes off the turn m",
        ],
        COLUMNS,
        rows,
    )
    print(f"placed_turn.txt: {count} rows")


if __name__ == "__main__":
    main()
