#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the seventh glm order lane in test/oracle/data:

- turn_about.txt: the 3D engine's turn about a unit axis by an angle, from the angle's sine and cosine
- held_up.txt: the up of a thing the hand holds: straight up turned about the line to the camera, then about the level
  line across it (the two turns and their product by the game; the unit lines and the last move worked in floats in
  the game's order)
- held_basis.txt: the frame of a thing the hand holds, from the hand's heading and the up

Usage: python tools/oracle/cases/glm_order_7_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
TURN_ABOUT = 0x7FB180  # fastcall (ECX out matrix, EDX unit axis), (angle)
PRODUCT = 0x7FAFF0  # fastcall (ECX matrix, in and out; EDX the second matrix)
HELD_FRAME = 0x46E2F0  # thiscall (hand), (out matrix, position, up)
SIZE_SCALE = 0x8C7C8C  # a float the hand's drawn scale is divided from

# The hand's fields
HAND_HEADING = 0x84
HAND_SCALE_DIVISOR = 0x8C
HAND_SCALE = 0x90
HAND_SIZE = 0x4834
HAND_MIRROR = 0x484C

PI_F = np.float32(3.1415927410125732)

F = np.float32


def bits(value) -> int:
    return int(np.array(value, dtype=np.float32).view(np.uint32))


def from_bits(value: int) -> np.float32:
    return np.array(value, dtype=np.uint32).view(np.float32)[()]


def rand_float(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


def unit(v: list, order: str) -> list:
    """The game's unit vector: 1 over the root of the squares added in the order, times each part; zero stays"""
    if all(part == 0 for part in v):
        return v
    squares = {"x": v[0] * v[0], "y": v[1] * v[1], "z": v[2] * v[2]}
    total = F(F(squares[order[0]] + squares[order[1]]) + squares[order[2]])
    share = F(F(1.0) / F(np.sqrt(total)))
    return [F(part * share) for part in v]


def turn_about_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x7FB1)
    o = oracle
    axis = o.alloc(0x10)
    out = o.alloc(0x30)
    rows = []
    for index in range(1500):
        values = [F(rng.uniform(-1.0, 1.0)) for _ in range(3)]
        values = unit(values, "xyz") if index % 11 else [F(0.0)] * 3
        angle = rand_float(rng, -4.0, 4.0)
        o.write_u32(axis, *[bits(v) for v in values])
        o.call(TURN_ABOUT, ecx=out, edx=axis, args=[angle])
        rows.append([bits(v) for v in values] + [angle] + o.read_f32_bits(out, 12))
    return rows


def held_up_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x5B49)
    o = oracle
    axis = o.alloc(0x10)
    roll_turn = o.alloc(0x30)
    pitch_turn = o.alloc(0x30)
    rows = []
    for index in range(1200):
        camera = [F(rng.uniform(-3000.0, 3000.0)), F(rng.uniform(50.0, 900.0)), F(rng.uniform(-3000.0, 3000.0))]
        hand = [F(c + F(rng.uniform(-300.0, 300.0))) for c in camera]
        if index % 17 == 0:
            hand = list(camera)
        roll = F(rng.uniform(-0.6, 0.6))
        pitch = F(rng.uniform(-0.6, 0.6))
        to_camera = unit([F(c - h) for c, h in zip(camera, hand)], "xyz")
        across = unit([F(F(0.0) - to_camera[2]), F(0.0), F(to_camera[0] - F(0.0))], "xzy")
        o.write_u32(axis, *[bits(v) for v in to_camera])
        o.call(TURN_ABOUT, ecx=roll_turn, edx=axis, args=[bits(roll)])
        o.write_u32(axis, *[bits(v) for v in across])
        o.call(TURN_ABOUT, ecx=pitch_turn, edx=axis, args=[bits(pitch)])
        o.call(PRODUCT, ecx=roll_turn, edx=pitch_turn)
        m = [from_bits(b) for b in o.read_f32_bits(roll_turn, 12)]
        point = [F(0.0), F(1.0), F(0.0)]
        # x adds the products of rows 1, 2, 0; y and z add rows 0, 1, 2; then the place
        up_x = F(F(F(F(point[1] * m[3]) + F(point[2] * m[6])) + F(point[0] * m[0])) + m[9])
        up_y = F(F(F(F(point[0] * m[1]) + F(point[1] * m[4])) + F(point[2] * m[7])) + m[10])
        up_z = F(F(F(F(point[0] * m[2]) + F(point[1] * m[5])) + F(point[2] * m[8])) + m[11])
        rows.append([bits(v) for v in camera + hand + [roll, pitch, up_x, up_y, up_z]])
    return rows


def held_basis_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x46E2)
    o = oracle
    hand = o.object(0x4900, {HAND_SCALE_DIVISOR: 1.0, HAND_SCALE: 1.0, HAND_SIZE: 1.0, HAND_MIRROR: 0})
    o.write_f32(SIZE_SCALE, 1.0)
    out = o.alloc(0x30)
    position = o.alloc(0x10)
    up = o.alloc(0x10)
    rows = []
    for _ in range(1200):
        heading = F(rng.uniform(-4.0, 4.0))
        tilt = [F(rng.uniform(-0.5, 0.5)), F(1.0), F(rng.uniform(-0.5, 0.5))]
        up_values = unit(tilt, "xyz")
        place = [F(rng.uniform(-3000.0, 3000.0)) for _ in range(3)]
        o.write_u32(hand + HAND_HEADING, bits(heading))
        o.write_u32(position, *[bits(v) for v in place])
        o.write_u32(up, *[bits(v) for v in up_values])
        o.call(HELD_FRAME, this=hand, args=[out, position, up])
        # The facing from the heading: the sine and the negative cosine of the heading plus a half turn, as floats
        angle = F(heading + PI_F)
        sine = F(math.sin(float(angle)))
        cosine = F(-math.cos(float(angle)))
        rows.append([bits(v) for v in [sine, cosine] + up_values] + o.read_f32_bits(out, 9))
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        turns = turn_about_cases(oracle)
        ups = held_up_cases(oracle)
        bases = held_basis_cases(oracle)
    write_pinned(out / "turn_about.txt", "The 3D engine's turn about a unit axis by an angle, from its sine and cosine",
                 ["the axis and the angle in; the matrix (rows, then the place) out"],
                 ["x", "y", "z", "angle"] + [f"m{i}" for i in range(12)], turns)
    write_pinned(out / "held_up.txt", "The up of a thing the hand holds",
                 ["the camera, the hand, the roll and the pitch in; the up out. The two turns and their product come",
                  "from the game; the unit lines and the last move are worked in floats in the game's order"],
                 ["cx", "cy", "cz", "hx", "hy", "hz", "roll", "pitch", "ux", "uy", "uz"], ups)
    write_pinned(out / "held_basis.txt", "The frame of a thing the hand holds, from the hand's heading and the up",
                 ["the facing (the float sine and negative cosine of the heading plus a half turn) and the up in; the",
                  "three rows out (hand scale 1, not mirrored)"],
                 ["sine", "cosine", "ux", "uy", "uz"] + [f"r{i}" for i in range(9)], bases)
    print(f"turn_about {len(turns)}, held_up {len(ups)}, held_basis {len(bases)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
