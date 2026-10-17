#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the hand that holds a town's totem, in test/oracle/data:

- totem_hand.txt: the hand's frame while it holds the totem (the tilt from its height, the rows from its heading and
  the tilt) and the grip time, from the game's own update of that hand state
- hand_heading.txt: the hand's heading angle from the line of sight through the cursor

Usage: python tools/oracle/cases/totem_hand_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
TOTEM_UPDATE = 0x5B77A0  # thiscall (hand state), args (seconds, matrix); RET 8
MY_INTERFACE = 0x555850  # thiscall on the game: the local interface
DYNAMIC_CAST = 0x7C5A19  # cdecl, 5 args
PROJECT_POINT = 0x5B7C10  # cdecl, 3 args
PIN_IMAGE = 0x7E5550  # thiscall on the mouse, 2 args
GET_ANIM = 0x619650  # thiscall, 2 args: the clip of an anim
PLAY_BONES = 0x860E00  # thiscall, 8 args
BONES_TO_WORLD = 0x839F10  # cdecl (matrix out, mesh, frame): the last call gets the hand's frame as its third arg
TOTEM_HEIGHT = 0x638120  # the totem's vtable +0x42C: its height
TOTEM_HOLD_RADIUS = 0x638C00  # the totem's vtable +0x590: its hold radius
HEADING = 0x7FAA50  # cdecl (point): the angle of a direction across the land, on ST0

ROWS = ["r00", "r01", "r02", "r10", "r11", "r12", "r20", "r21", "r22"]


class TotemHand:
    COLUMNS = ["hand_y", "heading", "hold_radius", "clip_ms", "y_out"] + ROWS + ["grip_ms"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.hand = o.alloc(0x4900)
        self.state = o.alloc(0x100)
        interface = o.alloc(0x400)
        self.totem = o.alloc(0x100)
        self.body = o.alloc(0x100)
        vtable = o.alloc(0x800)
        self.clip = o.alloc(0x40)
        self.matrix = o.alloc(0x40)
        self.frame = o.alloc(0x40)
        o.write_u32(self.state + 0x4, self.hand)
        o.write_u32(interface + 0x3C8, self.totem)
        o.write_u32(self.totem + 0x0, vtable)
        o.write_u32(self.totem + 0x24, 0)
        o.write_u32(self.totem + 0x40, self.body)
        o.write_u32(vtable + 0x42C, TOTEM_HEIGHT)
        o.write_u32(vtable + 0x590, TOTEM_HOLD_RADIUS)
        # The hand's scale is exactly 1: (3.2 / 3.2) x 1 x 1, not mirrored, so the frame is the rotation's rows
        o.write_f32(self.hand + 0x8C, f32.of_bits(0x404CCCCD))
        o.write_f32(self.hand + 0x90, 1.0)
        o.write_f32(self.hand + 0x4834, 1.0)
        o.write_u32(self.hand + 0x484C, 0)
        o.write_u32(self.hand + 0x4860, 0)
        o.write_u32(self.hand + 0x4868, 0)
        o.stub(MY_INTERFACE, eax=interface)
        o.stub(DYNAMIC_CAST, eax=self.totem)
        o.stub(PROJECT_POINT)
        o.stub(PIN_IMAGE, pop=8)
        o.stub(GET_ANIM, eax=self.clip, pop=8)
        o.stub(PLAY_BONES, pop=0x20)
        o.stub(TOTEM_HEIGHT, st0=0.0)
        # The last call copies the 12 floats of the frame (its third arg) to our memory
        code = bytes([0x8B, 0x44, 0x24, 0x0C, 0xBA]) + struct.pack("<I", self.frame)
        for i in range(12):
            code += bytes([0x8B, 0x48, i * 4, 0x89, 0x4A, i * 4])
        code += bytes([0xC3])
        o.write(BONES_TO_WORLD, code)

    def run(self, hand_y: int, heading: int, radius: int, clip_ms: int) -> list[int]:
        o = self.o
        # The totem's height is 0, so the hand's height is the body's height (0 x 0.7 + y)
        o.write_u32(self.body + 0x3C, hand_y)
        o.write_u32(self.hand + 0x84, heading)
        o.write_u32(self.clip, clip_ms)
        o.stub(TOTEM_HOLD_RADIUS, st0=f32.of_bits(radius))
        o.write(self.frame, bytes(48))
        result = o.call(TOTEM_UPDATE, this=self.state, args=[f32(0.02), self.matrix])
        assert result.popped == 8, result
        frame = o.read_u32(self.frame, 9)
        return [o.read_u32(self.hand + 0x7C)[0]] + frame + [o.read_u32(self.state + 0x8)[0]]


def totem_cases(oracle: Oracle) -> list[list[int]]:
    hand = TotemHand(oracle)
    rng = random.Random(14)
    cases: list[list[int]] = []
    # Chosen: high (the fixed tilt), at the low height, just below it, low, on the land and under it
    for y in (10.0, 2.5, 2.4999998, 2.0, 1.0, 0.0, -1.5):
        for heading in (0.0, 0.1, 1.0, -2.0, 3.1, -3.14159274):
            cases.append([f2u(y), f2u(heading), f2u(1.2), 1000])
    # Random hands
    for _ in range(1500):
        cases.append([f2u(rng.uniform(-3.0, 6.0)), f2u(rng.uniform(-math.pi, math.pi)), f2u(rng.uniform(0.0, 4.0)),
                      rng.randint(200, 3000)])
    rows = []
    for case in cases:
        rows.append(case + hand.run(*case))
    return rows


def heading_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(15)
    point = oracle.alloc(0x10)
    cases: list[tuple[float, float]] = [(0.0, 0.0), (0.0, 1.0), (0.0, -1.0), (1.0, 0.0), (-1.0, 0.0), (1.0, 1.0),
                                        (-1.0, 1.0), (1.0, -1.0), (-1.0, -1.0), (0.0005, 0.0005), (0.0007, 0.0007),
                                        (0.001, 0.0), (0.3, -0.3), (-0.3, -0.3)]
    for _ in range(3000):
        angle = rng.uniform(-math.pi, math.pi)
        length = rng.choice([1.0, rng.uniform(0.001, 1.0)])
        cases.append((length * math.sin(angle), length * math.cos(angle)))
    rows = []
    for x, z in cases:
        oracle.write_f32(point, x, -0.5, z)
        result = oracle.call(HEADING, args=[point])
        assert result.depth == 1, result
        rows.append([f2u(x), f2u(z), result.st0f])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        rows = totem_cases(oracle)
        n = write_pinned(out / "totem_hand.txt", "The hand that holds a town's totem: its frame and its grip time",
                         ["The game's update of the hand state, with the hand's scale 1 so the frame is the rotation.",
                          "Inputs: the hand's height and heading (float bits), the totem's hold radius (float bits),",
                          "the grip clip's length in ms (int). Out: the height, the 9 rows of the frame (row-vector",
                          "form, the model's x, y and z), the grip time in ms (int)."],
                         TotemHand.COLUMNS, rows)
        print(f"totem_hand: {n} rows")
        oracle.reset()
        rows = heading_cases(oracle)
        n = write_pinned(out / "hand_heading.txt", "The hand's heading angle from the line of sight",
                         ["x and z of the direction in (float bits), the angle stored as a float out."],
                         ["x", "z", "angle"], rows)
        print(f"hand_heading: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
