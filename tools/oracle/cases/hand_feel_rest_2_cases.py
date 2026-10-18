#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the second hand feel lane in test/oracle/data:

- hand_drawn_scale.txt: the hand's size from its distance to the camera, and the scale it is drawn at, for a hand
  rest height and, on a creature, the creature's size (zero for no creature). The game's own functions make the size
  and the scale; the creature step runs the game's own instructions, copied into a small stub.
- pour_up.txt: the way the hand stands up while a pour tips it: straight up turned by the tilt about the line from
  the hand to the camera. The turn is the game's own matrix; the product of the straight up with it follows the
  game's order of sums.
- pour_tilt.txt: the pour's tilt a fraction of the way from the last game turn to the next (the game's own function).

Usage: python tools/oracle/cases/hand_feel_rest_2_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
SET_DISTANCE = 0x46C0D0  # thiscall (hand), (distance): the hand's size from its distance
DRAWN_SCALE = 0x46D080  # thiscall (hand), (out matrix): the drawn scale
POW = 0x7C7AE0  # the C library's power, on the FPU (base below the exponent)
HALF = 0x8AB260  # the double 0.5
TURN_ABOUT = 0x7FB180  # ECX out matrix, EDX unit axis, (angle): a turn about an axis
POUR_TILT = 0x5B2D50  # thiscall (grain hand state): the tilt between game turns
GAME = 0xD0195C  # the game object, which holds the fraction of the turn at +0x205D64
TURN_FRACTION = 0x205D64
# The oracle calls only addresses in the image, so the stubs go in the resource section, which no code reads here
STUB_PLACE = 0xFC5000

# The hand's fields
HAND_REST = 0x8C
HAND_SIZE_FACTOR = 0x90
HAND_DRAWN = 0x94
HAND_SIZE = 0x4834
HAND_REACH = 0x4838

F = np.float32


def bits(value) -> int:
    return int(np.array(value, dtype=np.float32).view(np.uint32))


def call_rel(at: int, target: int) -> bytes:
    return b"\xE8" + struct.pack("<i", target - (at + 5))


def creature_stubs(o: Oracle, hand: int):
    """Two stubs with the game's instructions for a hand on a creature: the creature's size to the power of a half,
    stored as a float; then, after the hand's size is set from its distance, the hand's size times that"""
    size = o.alloc(0x10)
    root = o.alloc(0x10)
    first = STUB_PLACE
    code = b"\xD9\x05" + struct.pack("<I", size)  # fld dword [size]
    code += b"\xDD\x05" + struct.pack("<I", HALF)  # fld qword [0.5]
    code += call_rel(first + len(code), POW)
    code += b"\xD9\x1D" + struct.pack("<I", root)  # fstp dword [root]
    code += b"\xC3"
    o.write(first, code)
    second = STUB_PLACE + 0x40
    code = b"\xD9\x05" + struct.pack("<I", root)  # fld dword [root]
    code += b"\xD8\x0D" + struct.pack("<I", hand + HAND_SIZE)  # fmul dword [hand size]
    code += b"\xD9\x1D" + struct.pack("<I", hand + HAND_SIZE)  # fstp dword [hand size]
    code += b"\xC3"
    o.write(second, code)
    return size, root, first, second


def scale_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    hand = o.object(0x4900, {HAND_REACH: 1800.0, HAND_SIZE_FACTOR: 1.0})
    out = o.alloc(0x30)
    size_at, root_at, first, second = creature_stubs(o, hand)
    distances = [F(v) for v in (0.5, 2.0, 3.0, 9.99, 10.0, 10.01, 77.7, 149.9, 150.0, 150.1, 999.0, 1799.0, 1800.0,
                                2500.0)]
    distances += [F(rng.uniform(1.0, 40.0)) for _ in range(300)]
    distances += [F(rng.uniform(10.0, 2000.0)) for _ in range(500)]
    rows = []
    for distance in distances:
        rest = F(rng.choice([3.2, rng.uniform(1.0, 8.0)]))
        creature = F(rng.uniform(0.3, 3.0)) if rng.random() < 0.4 else F(0.0)
        o.write_u32(hand + HAND_REST, bits(rest))
        if creature != 0.0:
            o.write_u32(size_at, bits(creature))
            o.call(first)
        o.call(SET_DISTANCE, this=hand, args=[f32.of_bits(bits(distance))])
        if creature != 0.0:
            o.call(second)
        o.call(DRAWN_SCALE, this=hand, args=[out])
        size = o.read_f32_bits(hand + HAND_SIZE)[0]
        drawn = o.read_f32_bits(hand + HAND_DRAWN)[0]
        rows.append([bits(distance), bits(rest), bits(creature), size, drawn])
    return rows


def unit_xzy(v: list) -> list:
    """The axis made unit, the squares added x, z, y, unless it is zero"""
    if all(part == 0 for part in v):
        return v
    total = F(F(F(v[0] * v[0]) + F(v[2] * v[2])) + F(v[1] * v[1]))
    share = F(F(1.0) / F(np.sqrt(total)))
    return [F(part * share) for part in v]


def pour_up_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    matrix = o.alloc(0x30)
    axis = o.alloc(0x10)
    rows = []
    for index in range(800):
        camera = [F(rng.uniform(-500.0, 500.0)), F(rng.uniform(50.0, 400.0)), F(rng.uniform(-500.0, 500.0))]
        hand = [F(rng.uniform(-500.0, 500.0)), F(rng.uniform(0.0, 100.0)), F(rng.uniform(-500.0, 500.0))]
        if index < 4:
            hand = list(camera)
        tilt = F(rng.uniform(-1.2, 1.2)) if index % 7 else F(rng.uniform(-1e-3, 1e-3))
        towards = unit_xzy([F(camera[i] - hand[i]) for i in range(3)])
        o.write_u32(axis, *[bits(v) for v in towards])
        o.call(TURN_ABOUT, ecx=matrix, edx=axis, args=[f32.of_bits(bits(tilt))])
        m = [np.array(o.read_f32_bits(matrix + 4 * i), dtype=np.uint32).view(np.float32)[0] for i in range(12)]
        up = [F(0.0), F(1.0), F(0.0)]
        # The game's order: x = (m0 x + m6 z) + m3 y, y = (m1 x + m7 z) + m4 y, z = (m5 y + m2 x) + m8 z, then the place
        x = F(F(F(F(m[0] * up[0]) + F(m[6] * up[2])) + F(m[3] * up[1])) + m[9])
        y = F(F(F(F(m[1] * up[0]) + F(m[7] * up[2])) + F(m[4] * up[1])) + m[10])
        z = F(F(F(F(m[5] * up[1]) + F(m[2] * up[0])) + F(m[8] * up[2])) + m[11])
        rows.append([bits(v) for v in camera + hand] + [bits(tilt), bits(x), bits(y), bits(z)])
    return rows


def pour_tilt_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    game = o.alloc(TURN_FRACTION + 0x10)
    o.write_u32(GAME, game)
    state = o.alloc(0x240)
    rows = []
    for _ in range(400):
        current = F(rng.uniform(-1.3, 1.3))
        last = F(rng.uniform(-1.3, 1.3))
        fraction = F(rng.choice([0.0, 1.0, rng.random()]))
        o.write_u32(state + 0x114, bits(current))
        o.write_u32(state + 0x11C, bits(last))
        o.write_u32(game + TURN_FRACTION, bits(fraction))
        r = o.call(POUR_TILT, this=state)
        rows.append([bits(last), bits(current), bits(fraction), r.st0f])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x46D080)
    with Oracle() as oracle:
        scales = scale_cases(oracle, rng)
        ups = pour_up_cases(oracle, rng)
        tilts = pour_tilt_cases(oracle, rng)
    write_pinned(out / "hand_drawn_scale.txt", "The hand's size from its distance and the scale it is drawn at",
                 ["the distance, the hand's rest height and the creature's size (0: no creature) in; the size and the "
                  "drawn scale out"],
                 ["distance", "rest", "creature", "size", "drawn"], scales)
    write_pinned(out / "pour_up.txt", "The way the hand stands up while a pour tips it",
                 ["the camera, the hand and the tilt in; straight up turned by the tilt about the line to the camera out"],
                 ["cx", "cy", "cz", "hx", "hy", "hz", "tilt", "ux", "uy", "uz"], ups)
    write_pinned(out / "pour_tilt.txt", "The pour's tilt between game turns",
                 ["the last turn's tilt, this turn's tilt and the fraction of the turn in; the tilt out"],
                 ["last", "current", "fraction", "tilt"], tilts)
    print(f"hand_drawn_scale {len(scales)}, pour_up {len(ups)}, pour_tilt {len(tilts)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
