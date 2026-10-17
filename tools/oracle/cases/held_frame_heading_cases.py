#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the held frame heading lane in test/oracle/data:

- held_heading.txt: the hand's heading from the line of sight through the cursor (the game's heading of a direction),
  and whether the hand takes it (the line must lean more than 0.01 east-west or north-south)
- held_frame.txt: the frame of a thing the hand holds, from the hand's heading itself and the up (the game's own sine
  and cosine of the heading plus a half turn)

Usage: python tools/oracle/cases/held_frame_heading_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
HEADING_OF = 0x7FAA50  # cdecl (direction) -> ST0
HELD_FRAME = 0x46E2F0  # thiscall (hand), (out matrix, position, up)
SIZE_SCALE = 0x8C7C8C  # a float the hand's drawn scale is divided from

# The hand's fields
HAND_HEADING = 0x84
HAND_SCALE_DIVISOR = 0x8C
HAND_SCALE = 0x90
HAND_SIZE = 0x4834
HAND_MIRROR = 0x484C

# The least lean the hand turns to (the double that holds the float 0.01)
LEAST_LEAN = float(np.float32(0.01))

F = np.float32


def bits(value) -> int:
    return int(np.array(value, dtype=np.float32).view(np.uint32))


def unit(v: list) -> list:
    """The game's unit vector, the squares added x, y, z; zero stays"""
    if all(part == 0 for part in v):
        return v
    total = F(F(F(v[0] * v[0]) + F(v[1] * v[1])) + F(v[2] * v[2]))
    share = F(F(1.0) / F(np.sqrt(total)))
    return [F(part * share) for part in v]


def ray_inputs(rng: random.Random) -> list[list]:
    rays = []
    # Lines of sight from a camera above the land, in all directions
    for _ in range(1500):
        rays.append(unit([F(rng.uniform(-1.0, 1.0)), F(rng.uniform(-1.0, 0.2)), F(rng.uniform(-1.0, 1.0))]))
    # Near straight down, around the least lean
    for _ in range(400):
        rays.append([F(rng.uniform(-0.02, 0.02)), F(-1.0), F(rng.uniform(-0.02, 0.02))])
    # Along the axes and the diagonals, where the eighths meet
    for x in (-1.0, -0.5, 0.0, 0.5, 1.0):
        for z in (-1.0, -0.5, 0.0, 0.5, 1.0):
            rays.append([F(x), F(-0.5), F(z)])
    # Exactly at the least lean
    for x, z in ((0.01, 0.0), (-0.01, 0.0), (0.0, 0.01), (0.0, -0.01), (0.01, 0.01)):
        rays.append([F(x), F(-1.0), F(z)])
    return rays


def heading_cases(oracle: Oracle, rng: random.Random) -> list[list[int]]:
    o = oracle
    direction = o.alloc(0x10)
    rows = []
    for ray in ray_inputs(rng):
        o.write_u32(direction, *[bits(v) for v in ray])
        r = o.call(HEADING_OF, args=[direction])
        takes = 1 if abs(float(ray[0])) > LEAST_LEAN or abs(float(ray[2])) > LEAST_LEAN else 0
        rows.append([bits(v) for v in ray] + [takes, r.st0f])
    return rows


def frame_cases(oracle: Oracle, rng: random.Random, headings: list[int]) -> list[list[int]]:
    o = oracle
    hand = o.object(0x4900, {HAND_SCALE_DIVISOR: 1.0, HAND_SCALE: 1.0, HAND_SIZE: 1.0, HAND_MIRROR: 0})
    o.write_f32(SIZE_SCALE, 1.0)
    out = o.alloc(0x30)
    position = o.alloc(0x10)
    up = o.alloc(0x10)
    pi_f = F(math.pi)
    chosen = [bits(F(rng.uniform(-math.pi, math.pi))) for _ in range(800)]
    chosen += [bits(v) for v in (F(0.0), pi_f, -pi_f, F(pi_f / F(2.0)), -F(pi_f / F(2.0)), F(1e-7), F(-1e-7))]
    chosen += headings[:600]
    rows = []
    for heading in chosen:
        tilt = [F(rng.uniform(-0.5, 0.5)), F(1.0), F(rng.uniform(-0.5, 0.5))]
        up_values = unit(tilt) if rng.random() < 0.8 else [F(0.0), F(1.0), F(0.0)]
        place = [F(rng.uniform(-3000.0, 3000.0)) for _ in range(3)]
        o.write_u32(hand + HAND_HEADING, heading)
        o.write_u32(position, *[bits(v) for v in place])
        o.write_u32(up, *[bits(v) for v in up_values])
        o.call(HELD_FRAME, this=hand, args=[out, position, up])
        rows.append([heading] + [bits(v) for v in up_values] + o.read_f32_bits(out, 9))
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x46C7)
    with Oracle() as oracle:
        headings = heading_cases(oracle, rng)
        frames = frame_cases(oracle, rng, [row[4] for row in headings if row[3] == 1])
    write_pinned(out / "held_heading.txt", "The hand's heading from the line of sight through the cursor",
                 ["the line in; whether the hand takes a new heading (a lean over 0.01 in x or z) and the heading out"],
                 ["x", "y", "z", "takes", "heading"], headings)
    write_pinned(out / "held_frame.txt", "The frame of a thing the hand holds, from the hand's heading and the up",
                 ["the heading and the up in; the three rows out (hand scale 1, not mirrored)"],
                 ["heading", "ux", "uy", "uz"] + [f"r{i}" for i in range(9)], frames)
    print(f"held_heading {len(headings)}, held_frame {len(frames)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
