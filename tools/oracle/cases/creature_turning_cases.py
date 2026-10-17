#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of a creature's turn on the spot, in test/oracle/data:

- creature_turn_start.txt: the start of a turn to a heading. The game keeps the target as it is given, wraps the turn
  one time, picks the side of the spins and blends the two spins of that side by the size of the turn.
- creature_turn_ease.txt: one step of a turn without spins. The heading closes the part of the gap that the time before
  the step gives, and at the end it is the target.

Usage: python tools/oracle/cases/creature_turning_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
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
TURN_START = 0x48B790  # thiscall on the 3D creature (target heading), RET 4
TURN_STEP = 0x48B970  # thiscall on the 3D creature, no arguments, returns EAX 1 at the end of the turn
GET_ANIM = 0x619650  # thiscall (clip, flag), RET 8: the clip of the creature; stubbed
ALLOCATE = 0x7DB790  # cdecl (size, file, line): the game's allocator; hooked
BLEND = 0x85E9D0  # thiscall (clip a, ?, clip b, ?, weight), RET 0x14: the blend of two spins; replaced
NO_BLEND = 0xFFFFFFFF

# The 3D creature's fields that the turn uses
HEADING = 0x84
CLIP = 0x47C0
TIME = 0x47D0
CLIP_WEIGHT = 0x47E0
TARGET = 0x4840
STEP_MS = 0x48BC
SIDE = 0x521C
BLEND_CLIP = 0x5220


class Turning:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.record = o.alloc(0x10)
        # Every clip is there: a clip whose first value is its length in milliseconds
        self.clip = o.alloc(0x40)
        # The turn reads a value through the clip's pointer at +0x34
        o.write_u32(self.clip + 0x34, self.clip + 0x38)
        o.stub(GET_ANIM, eax=self.clip, pop=8)
        o.hook_alloc(ALLOCATE)
        # The blend keeps the weight that it gets: mov eax, [esp + 0x14]; mov [record], eax; mov eax, record; ret 0x14
        o.write(BLEND, bytes([0x8B, 0x44, 0x24, 0x14, 0xA3]) + struct.pack("<I", self.record) + bytes([0xB8]) +
                struct.pack("<I", self.record) + bytes([0xC2, 0x14, 0x00]))
        self.creature = o.alloc(0x5300)

    def start(self, heading: int, target: int) -> tuple[int, int, int]:
        o = self.o
        o.write_u32(self.record, NO_BLEND)
        o.write_u32(self.creature + SIDE, 0)
        o.write_u32(self.creature + HEADING, heading)
        o.call(TURN_START, this=self.creature, args=[f32.of_bits(target)])
        return (o.read_u32(self.creature + TARGET)[0], o.read_u32(self.creature + SIDE)[0], o.read_u32(self.record)[0])

    def step(self, heading: int, target: int, time_ms: int, length_ms: int, step_ms: int) -> tuple[int, int, int]:
        o = self.o
        o.write_u32(self.clip, length_ms)
        o.write_u32(self.creature + BLEND_CLIP, 0)
        o.write_u32(self.creature + HEADING, heading)
        o.write_u32(self.creature + TARGET, target)
        o.write_u32(self.creature + TIME, time_ms)
        o.write_u32(self.creature + STEP_MS, step_ms)
        result = o.call(TURN_STEP, this=self.creature)
        return (o.read_u32(self.creature + HEADING)[0], o.read_u32(self.creature + TIME)[0], result.eax)


def start_cases(rng: random.Random) -> list[tuple[int, int]]:
    pi = math.pi
    rows = []
    for _ in range(1500):
        rows.append((f2u(rng.uniform(-pi, pi)), f2u(rng.uniform(-2.0 * pi, 2.0 * pi))))
    # Turns of none, of a quarter turn and of a half turn, each side
    for heading in (0.0, 1.0, -2.5, 3.0):
        for turn in (0.0, -0.0, pi / 2, -pi / 2, pi, -pi, 1e-7, -1e-7):
            rows.append((f2u(heading), f2u(heading + turn)))
    # The target of a turn away from a thing: the heading to the thing and a half turn, from 0 to a whole turn
    for _ in range(300):
        rows.append((f2u(rng.uniform(-pi, pi)), f2u(rng.uniform(0.0, 2.0 * pi))))
    return rows


def step_cases(rng: random.Random) -> list[tuple[int, int, int, int, int]]:
    pi = math.pi
    rows = []
    for _ in range(1500):
        length = rng.choice([rng.randint(1, 4000), 1000, 1533, 2000])
        step = rng.choice([100, 50, 33, rng.randint(1, 200)])
        time = rng.randint(0, length - 1)
        rows.append((f2u(rng.uniform(-pi, pi)), f2u(rng.uniform(-2.0 * pi, 2.0 * pi)), time, length, step))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x48B790)
    with Oracle() as oracle:
        turning = Turning(oracle)
        rows = [[*case, *turning.start(*case)] for case in start_cases(rng)]
        n = write_pinned(out / "creature_turn_start.txt", "The start of a creature's turn on the spot to a heading",
                         ["Function: thiscall on the 3D creature (target heading as a float argument), its heading",
                          "at +0x84. Every spin is there. The blend of the two spins is replaced by code that keeps",
                          "its weight. target_kept is +0x4840, side is +0x521C (2: the spins of a turn of 0 or less),",
                          "weight is the float bits of the blend weight, FFFFFFFF when there is no blend."],
                         ["heading", "target", "target_kept", "side", "weight"], rows)
        print(f"creature_turn_start: {n} rows")
        rows = [[*case, *turning.step(*case)] for case in step_cases(rng)]
        n = write_pinned(out / "creature_turn_ease.txt", "One step of a creature's turn on the spot without spins",
                         ["Function: thiscall on the 3D creature with no blend clip (+0x5220 = 0): heading +0x84,",
                          "target +0x4840, time +0x47D0 (ms), the stand clip's length (ms), the step +0x48BC (ms).",
                          "heading_after and time_after are +0x84 and +0x47D0 after the step; ended is EAX."],
                         ["heading", "target", "time", "length", "step", "heading_after", "time_after", "ended"], rows)
        print(f"creature_turn_ease: {n} rows")


if __name__ == "__main__":
    main()
