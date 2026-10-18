#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes more pinned data of a creature's walk, in test/oracle/data:

- creature_join_ahead.txt: how far ahead along its route a creature joins a route found while it walks. The game's
  own function runs as it is: a turn on the spot gives a fixed distance, a step what is left of the step's move, any
  other phase 0.
- creature_size_speeds.txt: the walking and running speeds of a creature of a size, the size that it keeps, and the
  speed that it has after the change. The game's own function runs as it is, with no creature behind the body.

Usage: python tools/oracle/cases/creature_walk_planner_2_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
JOIN_AHEAD = 0x47F280  # cdecl (creature), ST0: the follower's join distance
SET_SIZE = 0x480530  # thiscall on the 3D creature (size), RET 4

# The creature's link to its 3D body: [[creature + 0x160] + 0x58]
C_LINK = 0x160
L_BODY = 0x58

# The 3D creature's fields
SIZE = 0x90
SCALE = 0x94
SIZE_DIVISOR = 0x8C
CREATURE = 0x4834
SPEED_NOW = 0x4838
WALK_SPEED = 0x4844
RUN_SPEED = 0x4848
HEAD_RATE = 0x485C
PHASE = 0x5190
FOLLOWER = 0x5198
BLEND = 0x5220
RADIUS_AT_ONE = 0x5224
F_TRAVELLED = 0x64038
FOLLOWER_SIZE = 0x64100
CLIP_MOVE = 0x10


def rand_float(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


def join_cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for index in range(1500):
        phase = (4, 5, 5, 5, 6, 0)[index % 6]
        move = [rand_float(rng, -60.0, 60.0) for _ in range(3)]
        if index % 7 == 0:
            move[1] = 0
        scale = rand_float(rng, 0.05, 3.0)
        travelled = rand_float(rng, 0.0, 120.0)
        rows.append([phase, *move, scale, travelled])
    return rows


def size_cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for index in range(1500):
        if index % 5 == 0:
            size = rand_float(rng, -1.0, 0.2)
        elif index % 5 == 1:
            size = rand_float(rng, 3.5, 12.0)
        else:
            size = rand_float(rng, 0.05, 4.0)
        speed = rand_float(rng, 0.0, 120.0)
        rows.append([size, speed])
    return rows


class Walk:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.creature = o.alloc(0x200)
        self.link = o.alloc(0x100)
        self.body = o.alloc(0x5800)
        self.follower = o.alloc(FOLLOWER_SIZE)
        self.blend = o.alloc(0x40)

    def join(self, case: list[int]) -> list[int]:
        o = self.o
        o.write(self.body, bytes(0x5800))
        o.write_u32(self.creature + C_LINK, self.link)
        o.write_u32(self.link + L_BODY, self.body)
        o.write_u32(self.body + PHASE, case[0])
        o.write_u32(self.body + BLEND, self.blend)
        o.write_u32(self.blend + CLIP_MOVE, *case[1:4])
        o.write_u32(self.body + SCALE, case[4])
        o.write_u32(self.body + FOLLOWER, self.follower)
        o.write_u32(self.follower + F_TRAVELLED, case[5])
        return [o.call(JOIN_AHEAD, args=[self.creature]).st0f]

    def size(self, case: list[int]) -> list[int]:
        o = self.o
        o.write(self.body, bytes(0x5800))
        o.write_u32(self.body + SIZE_DIVISOR, f2u(1.0))
        o.write_u32(self.body + HEAD_RATE, f2u(1.0))
        o.write_u32(self.body + RADIUS_AT_ONE, f2u(1.0))
        o.write_u32(self.body + CREATURE, 0)
        o.write_u32(self.body + SPEED_NOW, case[1])
        o.call(SET_SIZE, this=self.body, args=[f32.of_bits(case[0])])
        return [*o.read_u32(self.body + WALK_SPEED, 2), o.read_u32(self.body + SIZE)[0],
                o.read_u32(self.body + SPEED_NOW)[0]]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x47F280)
    with Oracle() as oracle:
        walk = Walk(oracle)
        rows = [[*case, *walk.join(case)] for case in join_cases(rng)]
        n = write_pinned(out / "creature_join_ahead.txt", "How far ahead a creature joins a route found while it walks",
                         ["The creature's move phase (4 turn, 5 step, others none), the step's blended move, its scale",
                          "and the distance travelled along the leg. The result is the distance ahead."],
                         ["phase", "move_x", "move_y", "move_z", "scale", "travelled", "ahead"], rows)
        print(f"creature_join_ahead: {n} rows")

        oracle.reset()
        walk = Walk(oracle)
        rows = [[*case, *walk.size(case)] for case in size_cases(rng)]
        n = write_pinned(out / "creature_size_speeds.txt", "The walking and running speeds of a creature of a size",
                         ["The size given and the creature's speed before. The results are the walking and running",
                          "speeds, the size that the creature keeps, and its speed after."],
                         ["size", "speed", "walk", "run", "size_kept", "speed_after"], rows)
        print(f"creature_size_speeds: {n} rows")


if __name__ == "__main__":
    main()
