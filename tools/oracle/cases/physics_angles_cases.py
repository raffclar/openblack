#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the angles that a moved thing takes from its body, in test/oracle/data:

- physics_angles.txt: the matrix of a body (columns b0..b8, three rows). The angles that the game takes off it
  (columns ax, ay, az). The turn that the game makes again from the three angles for a thing that leans (t0..t8). The
  turn that the game makes from the angle about y alone for a thing that does not lean (u0..u8), with a scale of 1.

Usage: python tools/oracle/cases/physics_angles_cases.py [output folder]   (default: test/oracle/data)
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
ANGLES_OF_TURN = 0x7FAB30  # thiscall (matrix) (&y, &x, &z); RET 0xC
UPRIGHT_TURN = 0x423140  # ECX the 3D object, EDX the point, then (y, scale); RET 0x8
ZERO = 0x8AA398  # the angle that makes no turn
ONE = 0x8AA390  # the scale that keeps the turn's values

COLUMNS = ([f"b{i}" for i in range(9)] + ["ax", "ay", "az"] + [f"t{i}" for i in range(9)]
           + [f"u{i}" for i in range(9)])


def f32_bits(value: float) -> int:
    return f2u(struct.unpack("<f", struct.pack("<f", value))[0])


def turn(x: float, y: float, z: float) -> list[float]:
    """Rows of the turn about z, then x, then y (the game's order), in double"""
    cx, sx, cy, sy, cz, sz = math.cos(x), math.sin(x), math.cos(y), math.sin(y), math.cos(z), math.sin(z)
    return [cz * cy - sz * sx * sy, -sz * cx, sz * sx * cy + cz * sy,
            cz * sx * sy + sz * cy, cz * cx, sz * sy - cz * sx * cy,
            -cx * sy, sx, cx * cy]


def quaternion_rows(rng: random.Random) -> list[float]:
    """Rows of a random turn"""
    a, b, c, d = (rng.gauss(0.0, 1.0) for _ in range(4))
    n = math.sqrt(a * a + b * b + c * c + d * d)
    w, x, y, z = a / n, b / n, c / n, d / n
    return [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
            2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
            2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]


def cases() -> list[list[int]]:
    """Chosen bodies (still, turned, tumbled past a quarter turn, on end), then random bodies, some a little off true"""
    chosen = [
        turn(0.0, 0.0, 0.0),
        turn(0.0, 0.7, 0.0),
        turn(0.0, -2.9, 0.0),
        turn(0.0, math.pi, 0.0),
        turn(2.0, 4.0, -0.5),
        turn(-2.2, 0.6, 0.3),
        turn(1.5, -1.0, 2.5),
        turn(0.2, 0.7, -0.4),
        turn(math.pi / 2, 0.3, 0.2),
        turn(-math.pi / 2, -0.3, 0.2),
        # On end: the forward axis is upright, so it has no heading
        [1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0],
        # Upside down
        [-1.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, 0.0, 1.0],
    ]
    rng = random.Random(0xB0D1)
    randoms = []
    for index in range(300):
        rows = quaternion_rows(rng)
        if index % 3 == 0:
            # A body's axes drift a little from true as it turns
            rows = [v + rng.uniform(-2e-4, 2e-4) for v in rows]
        randoms.append(rows)
    return [[f32_bits(v) for v in rows] for rows in chosen + randoms]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rows = []
    with Oracle() as o:
        zero, one = o.read_u32(ZERO)[0], o.read_u32(ONE)[0]
        assert zero == f32_bits(0.0) and one == f32_bits(1.0), (hex(zero), hex(one))
        body = o.alloc(0x40)
        made = o.alloc(0x40)
        angles = o.alloc(0x10)
        object3d = o.alloc(0x80)
        point = o.alloc(0x10)
        for matrix in cases():
            o.write(body, bytes(0x40))
            o.write_u32(body, *matrix)
            o.write(angles, bytes(0x10))
            o.call(ANGLES_OF_TURN, this=body, args=[angles + 4, angles, angles + 8])
            ax, ay, az = o.read_u32(angles, 3)
            o.write(made, bytes(0x40))
            o.call(TURN_MATRIX, this=made, args=[f32.of_bits(ay), f32.of_bits(ax), f32.of_bits(az)])
            leans = o.read_u32(made, 9)
            o.write(object3d, bytes(0x80))
            o.call(UPRIGHT_TURN, ecx=object3d, edx=point, args=[f32.of_bits(ay), f32.of_bits(one)])
            upright = o.read_u32(object3d + 0x14, 9)
            rows.append([*matrix, ax, ay, az, *leans, *upright])
    count = write_pinned(
        out / "physics_angles.txt",
        "The angles that a moved thing takes from its body, and the turn made again from them",
        [
            "b: the matrix of a body (three rows of three)",
            "ax, ay, az: the angles that the game takes off the matrix b",
            "t: the turn about y, then x, then z, from ax, ay, az: the turn of a thing that leans",
            "u: the turn from ay alone, with a scale of 1: the turn of a thing that does not lean",
        ],
        COLUMNS,
        rows,
    )
    print(f"physics_angles.txt: {count} rows")


if __name__ == "__main__":
    main()
