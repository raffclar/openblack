#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the lightning bolt's maths, in test/oracle/data:

- lightning_clash.txt: whether a newer bolt from a hand clashes with an older one, and where they meet. The game's
  clash test runs as it is, with one older bolt in the list of bolts. Its call that links the two bolts is replaced by
  a routine that sets a flag.
- lightning_middle.txt: the middle of a bolt's targets and its distance from the bolt's origin. The end of the game's
  target search runs as it is, from an entry routine that makes its frame with the sum of the target tips.

Usage: python tools/oracle/cases/lightning_spell_order_cases.py [output folder]   (default: test/oracle/data)
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

# The game's code (BW1W120, runblack.exe 1.20)
CLASH_TEST = 0x6916B0  # thiscall (rule), args (unused, newer bolt), RET 8
LINK_BOLTS = 0x691AD0  # thiscall (older bolt), args (newer bolt, glow), RET 8; replaced by the flag routine
BOLT_LIST = 0xD4EC60  # the first of the live bolts
MIDDLE_TAIL = 0x691114  # the end of the target search: the middle, then the distance from the origin
# A call must start in the image: the entry routine goes over a particle rule's property list function, which these
# tests do not use (151 bytes)
ENTRY = 0x6B1CD0


def flag_code(flag: int) -> bytes:
    """mov dword [flag], 1; ret 8"""
    return bytes([0xC7, 0x05]) + struct.pack("<II", flag, 1) + bytes([0xC2, 0x08, 0x00])


def middle_entry(data: int, sums: tuple[int, int, int]) -> bytes:
    code = bytearray()
    code += bytes([0x83, 0xEC, 0x18])  # sub esp, 0x18
    code += bytes([0x53, 0x55, 0x56, 0x57])  # push ebx, ebp, esi, edi
    code += bytes([0xBE]) + struct.pack("<I", data)  # mov esi, data
    for offset, value in zip((0x10, 0x14, 0x18), sums):
        code += bytes([0xC7, 0x44, 0x24, offset]) + struct.pack("<I", value)  # mov dword [esp+offset], value
    code += bytes([0xB8]) + struct.pack("<I", MIDDLE_TAIL)  # mov eax, tail
    code += bytes([0xFF, 0xE0])  # jmp eax
    return bytes(code)


class Clash:
    COLUMNS = [
        "newer_x", "newer_y", "newer_z", "newer_cx", "newer_cy", "newer_cz", "newer_heading",
        "older_x", "older_y", "older_z", "older_cx", "older_cy", "older_cz", "older_heading", "reach",
        "clash", "meet_x", "meet_y", "meet_z",
    ]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.flag = oracle.alloc(0x10)
        oracle.write(LINK_BOLTS, flag_code(self.flag))
        self.rule = oracle.alloc(0x100)
        self.newer = oracle.alloc(0xC0)
        self.older = oracle.alloc(0xC0)

    def run(self, inputs: list[int]) -> list[int]:
        o = self.o
        o.write_u32(self.flag, 0)
        o.write_u32(self.rule + 0x68, 0, inputs[14])
        for bolt, values, order in ((self.newer, inputs[0:7], 2), (self.older, inputs[7:14], 1)):
            o.write_u32(bolt + 0x20, 0)
            o.write_u32(bolt + 0x2C, 0)
            o.write_u32(bolt + 0x30, *values[0:3])
            o.write_u32(bolt + 0x48, *values[3:7])
            o.write_u32(bolt + 0x58, 0x100)
            o.write_u32(bolt + 0x74, 1)
            o.write_u32(bolt + 0xB0, order, 0x7F7F7F7F, 0x7F7F7F7F, 0x7F7F7F7F)
        o.write_u32(BOLT_LIST, self.older)
        result = o.call(CLASH_TEST, this=self.rule, args=[0, self.newer])
        assert result.popped == 8 and result.depth == 0, result
        (clash,) = o.read_u32(self.flag)
        return [clash] + o.read_u32(self.newer + 0xB4, 3)


class Middle:
    COLUMNS = ["sum_x", "sum_y", "sum_z", "count", "origin_x", "origin_y", "origin_z", "mid_x", "mid_y", "mid_z",
               "distance"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.data = oracle.alloc(0x100)

    def run(self, inputs: list[int]) -> list[int]:
        o = self.o
        o.write_u32(self.data + 0x30, *inputs[4:7])
        o.write_u32(self.data + 0x74, inputs[3])
        o.write(ENTRY, middle_entry(self.data, (inputs[0], inputs[1], inputs[2])))
        result = o.call(ENTRY, args=[0, 0])
        assert result.popped == 8 and result.depth == 0, result
        return o.read_u32(self.data + 0x48, 3) + o.read_u32(self.data + 0x60)


def clash_cases() -> list[list[int]]:
    rng = random.Random(0x4C53)
    rows = []

    def add(*values: float) -> None:
        rows.append([f2u(v) for v in values])

    # Chosen cases: two bolts that clash, two that point away from each other, a meeting out of reach
    add(0.0, 10.0, 0.0, 0.0, 2.0, 40.0, 1.5707964, 5.0, 10.0, 0.0, 5.0, 2.0, 40.0, 1.5707964, 100.0)
    add(0.0, 10.0, 0.0, 0.0, 2.0, 40.0, 1.5707964, 5.0, 10.0, 0.0, 5.0, 2.0, -40.0, -1.5707964, 100.0)
    add(0.0, 10.0, 0.0, 0.0, 2.0, 40.0, 1.5707964, 5.0, 10.0, 0.0, 5.0, 2.0, 40.0, 1.5707964, 5.0)
    for _ in range(1200):
        x, z = rng.uniform(0.0, 5000.0), rng.uniform(0.0, 5000.0)
        heading = rng.uniform(-math.pi, math.pi)
        turn = rng.uniform(-1.2, 1.2)
        reach = rng.uniform(20.0, 300.0)
        ox, oz = x + rng.uniform(-60.0, 60.0), z + rng.uniform(-60.0, 60.0)
        length = rng.uniform(5.0, 150.0)
        older_heading = heading + turn
        add(x, rng.uniform(0.0, 60.0), z,
            x + math.cos(heading) * rng.uniform(5.0, 150.0) + rng.uniform(-20.0, 20.0), rng.uniform(0.0, 40.0),
            z + math.sin(heading) * rng.uniform(5.0, 150.0) + rng.uniform(-20.0, 20.0), heading,
            ox, rng.uniform(0.0, 60.0), oz,
            ox + math.cos(older_heading) * length + rng.uniform(-20.0, 20.0), rng.uniform(0.0, 40.0),
            oz + math.sin(older_heading) * length + rng.uniform(-20.0, 20.0), older_heading, reach)
    return rows


def middle_cases() -> list[list[int]]:
    rng = random.Random(0x4D49)
    rows = []
    for _ in range(1200):
        count = rng.randint(1, 30)
        sums = [rng.uniform(0.0, 5000.0) * count, rng.uniform(-10.0, 200.0) * count, rng.uniform(0.0, 5000.0) * count]
        origin = [rng.uniform(0.0, 5000.0), rng.uniform(0.0, 200.0), rng.uniform(0.0, 5000.0)]
        rows.append([f2u(v) for v in sums] + [count] + [f2u(v) for v in origin])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    clash_rows = []
    middle_rows = []
    with Oracle() as o:
        clash = Clash(o)
        for inputs in clash_cases():
            clash_rows.append(inputs + clash.run(inputs))
        middle = Middle(o)
        for inputs in middle_cases():
            middle_rows.append(inputs + middle.run(inputs))
    write_pinned(
        out / "lightning_clash.txt",
        "Whether a newer bolt from a hand clashes with an older one, and where they meet (the game's clash test)",
        [
            "The game's clash test runs as it is, with the older bolt as the only one in the list.",
            "Its call that links the bolts is replaced by a routine that sets the clash flag.",
            "The rule gives the reach (no provider). The meeting is zero when they do not clash.",
        ],
        Clash.COLUMNS,
        clash_rows,
    )
    write_pinned(
        out / "lightning_middle.txt",
        "The middle of a bolt's targets and its distance from the origin (the end of the game's target search)",
        ["The game's code runs as it is, from an entry routine that makes its frame with the sum of the tips."],
        Middle.COLUMNS,
        middle_rows,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
