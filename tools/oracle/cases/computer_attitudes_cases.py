#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how much a computer god wants to attack a rival, in test/oracle/data:

- computer_desire_to_attack.txt: the game's desire of one player to attack another player, from the two players'
  influence power and its average, their places, alignments and the harm each did to the other, the god's hand and
  the abodes of its one town

The script lays out the two players as the function reads them: the place of each player is the heart of its temple,
or the hand of its computer mind when it has no temple. The god's own hand is the hand of its computer mind. The rival
has no creature. The table of the distance is made first, as the game makes it at the start.

Usage: python tools/oracle/cases/computer_attitudes_cases.py [output folder]   (default: test/oracle/data)
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

# The functions (BW1W120, runblack.exe 1.20)
DESIRE_TO_ATTACK = 0x64CDB0  # thiscall (god), args (rival), returns ST0
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table that the distance reads
FIXED_PER_METRE = 0x45CCCCCD  # 6553.6f

# The parts of a player that the function reads
POWER = 0x8C
AVERAGE = 0x90
HARM = 0x94  # 8 floats, by player number
NUMBER = 0xB5  # a byte
ALIGNMENT = 0x60  # pointer to the alignment, whose value is at +0x8
COMPUTER = 0x944  # pointer to the computer mind: switched on at +0x1B8, its hand's action at +0x34 (place at +0x14)
CITADEL = 0xA48  # pointer to the temple: its heart at +0x30 (place at +0x14)
CREATURE = 0xA4C
TOWNS = 0xA50  # the first town: its first abode at +0x754, the next town at +0x75C
ABODE_NEXT = 0x9C
PLACE = 0x14

GOD_NUMBER = 1
RIVAL_NUMBER = 3
COLUMNS = [
    "god_power", "god_average", "god_x", "god_z", "god_alignment", "god_harm",
    "rival_power", "rival_average", "rival_x", "rival_z", "rival_alignment", "rival_harm", "rival_has_temple",
    "hand_x", "hand_z", "abodes", "abode1_x", "abode1_z", "abode2_x", "abode2_z", "abode3_x", "abode3_z",
    "desire",
]


def to_fixed(metres: float) -> int:
    """Metres to 16.16, a float product truncated towards 0"""
    m = struct.unpack("<f", struct.pack("<f", metres))[0]
    product = struct.unpack("<f", struct.pack("<f", m * struct.unpack("<f", struct.pack("<I", FIXED_PER_METRE))[0]))[0]
    return int(math.trunc(product))


class Desire:
    def __init__(self, oracle: Oracle):
        self.o = oracle
        oracle.call(MAKE_INVERSE_SQRT_TABLE)
        self.god = oracle.alloc(0xB00)
        self.rival = oracle.alloc(0xB00)
        self.god_alignment = oracle.alloc(0x10)
        self.rival_alignment = oracle.alloc(0x10)
        self.god_temple = oracle.alloc(0x40)
        self.god_heart = oracle.alloc(0x40)
        self.rival_temple = oracle.alloc(0x40)
        self.rival_heart = oracle.alloc(0x40)
        self.god_mind = oracle.alloc(0x200)
        self.god_action = oracle.alloc(0x40)
        self.rival_mind = oracle.alloc(0x200)
        self.rival_action = oracle.alloc(0x40)
        self.town = oracle.alloc(0x800)
        self.abodes = [oracle.alloc(0xA0) for _ in range(3)]

    def run(self, row: list[int]) -> int:
        o = self.o
        (g_power, g_avg, g_x, g_z, g_align, g_harm, r_power, r_avg, r_x, r_z, r_align, r_harm, r_temple,
         h_x, h_z, count, *abodes) = row

        def place(address: int, x: int, z: int) -> None:
            o.write(address + PLACE, struct.pack("<iiI", x, z, 0))

        for player, number, power, average, alignment, harm, other in (
            (self.god, GOD_NUMBER, g_power, g_avg, self.god_alignment, g_harm, RIVAL_NUMBER),
            (self.rival, RIVAL_NUMBER, r_power, r_avg, self.rival_alignment, r_harm, GOD_NUMBER),
        ):
            o.write_u32(player + POWER, power, average)
            o.write(player + HARM, bytes(32))
            o.write_u32(player + HARM + 4 * other, harm)
            o.write(player + NUMBER, bytes([number]))
            o.write_u32(player + ALIGNMENT, alignment)
            o.write_u32(player + CREATURE, 0)
        o.write_u32(self.god_alignment + 8, g_align)
        o.write_u32(self.rival_alignment + 8, r_align)

        # The god: the heart of its temple, its mind switched on with its hand, and one town
        o.write_u32(self.god + CITADEL, self.god_temple)
        o.write_u32(self.god_temple + 0x30, self.god_heart)
        place(self.god_heart, g_x, g_z)
        o.write_u32(self.god + COMPUTER, self.god_mind)
        o.write_u32(self.god_mind + 0x1B8, 1)
        o.write_u32(self.god_mind + 0x34, self.god_action)
        place(self.god_action, h_x, h_z)
        o.write_u32(self.god + TOWNS, self.town if count else 0)
        o.write_u32(self.town + 0x75C, 0)
        o.write_u32(self.town + 0x754, self.abodes[0] if count else 0)
        for index in range(3):
            place(self.abodes[index], abodes[2 * index], abodes[2 * index + 1])
            o.write_u32(self.abodes[index] + ABODE_NEXT, self.abodes[index + 1] if index + 1 < count else 0)

        # The rival: the heart of its temple, or its switched-on mind's hand
        if r_temple:
            o.write_u32(self.rival + CITADEL, self.rival_temple)
            o.write_u32(self.rival_temple + 0x30, self.rival_heart)
            place(self.rival_heart, r_x, r_z)
            o.write_u32(self.rival + COMPUTER, 0)
        else:
            o.write_u32(self.rival + CITADEL, 0)
            o.write_u32(self.rival + COMPUTER, self.rival_mind)
            o.write_u32(self.rival_mind + 0x1B8, 1)
            o.write_u32(self.rival_mind + 0x34, self.rival_action)
            place(self.rival_action, r_x, r_z)

        result = o.call(DESIRE_TO_ATTACK, this=self.god, args=[self.rival])
        assert result.popped == 4, result
        assert result.depth == 1, result
        return result.st0f


def cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for index in range(2000):
        gx = rng.uniform(100.0, 5000.0)
        gz = rng.uniform(100.0, 5000.0)
        reach = rng.choice([rng.uniform(0.0, 300.0), rng.uniform(0.0, 3000.0), rng.uniform(2000.0, 7000.0)])
        angle = rng.uniform(-math.pi, math.pi)
        rx = gx + reach * math.cos(angle)
        rz = gz + reach * math.sin(angle)

        def power() -> float:
            return rng.choice([0.0, rng.uniform(0.0, 50.0), rng.uniform(100.0, 2500.0)])

        g_power = power()
        r_power = power()
        g_avg = rng.choice([g_power, power(), g_power * rng.uniform(0.5, 1.5)])
        r_avg = rng.choice([r_power, power(), r_power * rng.uniform(0.5, 1.5)])
        g_align = rng.choice([0.0, 1.0, -1.0, rng.uniform(-1.0, 1.0)])
        r_align = rng.choice([0.0, 1.0, -1.0, rng.uniform(-1.0, 1.0)])
        if index % 50 == 0:
            g_align, r_align = 1.0, -1.0
        g_harm = rng.choice([0.0, rng.uniform(0.0, 5.0), rng.uniform(0.0, 500.0)])
        r_harm = rng.choice([0.0, rng.uniform(0.0, 5.0), rng.uniform(0.0, 500.0)])
        r_temple = 1 if rng.random() < 0.8 else 0
        hx = gx + rng.uniform(-200.0, 200.0)
        hz = gz + rng.uniform(-200.0, 200.0)
        count = rng.choice([0, 1, 2, 3])
        abodes = []
        for _ in range(3):
            abodes += [to_fixed(hx + rng.uniform(-150.0, 150.0)), to_fixed(hz + rng.uniform(-150.0, 150.0))]
        rows.append([
            f2u(g_power), f2u(g_avg), to_fixed(gx), to_fixed(gz), f2u(g_align), f2u(g_harm),
            f2u(r_power), f2u(r_avg), to_fixed(rx), to_fixed(rz), f2u(r_align), f2u(r_harm), r_temple,
            to_fixed(hx), to_fixed(hz), count, *abodes,
        ])
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x64CDB0)
    with Oracle() as oracle:
        desire = Desire(oracle)
        rows = [[*case, desire.run(case)] for case in cases(rng)]
    n = write_pinned(out / "computer_desire_to_attack.txt", "How much a computer god wants to attack a rival",
                     ["Function: thiscall on the god, args (rival), result ST0 stored as a float, with the start-up table.",
                      "Places are map positions (16.16 x and z); powers, alignments and harms are float bits.",
                      "god_harm is the harm from the rival to the god, rival_harm the harm from the god to the rival.",
                      "rival_has_temple 0: the rival's place is the hand of its switched-on computer mind.",
                      "abodes: how many of the three abodes the god's one town has, in that order."],
                     COLUMNS, rows)
    print(f"computer_desire_to_attack: {n} rows")


if __name__ == "__main__":
    main()
