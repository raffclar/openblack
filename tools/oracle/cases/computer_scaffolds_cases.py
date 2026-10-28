#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of a computer god's scaffold rules, in test/oracle/data:

- computer_scaffolds_use.txt: how good a scaffold of a workshop is for the god to use
- computer_scaffolds_fill.txt: how much a workshop wants wood for the god

Usage: python tools/oracle/cases/computer_scaffolds_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
BEST_SCAFFOLD = 0x669580  # thiscall (computer player), arg (scaffold* in and out); RET 4; ST0
FILL_WORKSHOP = 0x669B50  # thiscall (computer player), arg node; RET 4; ST0, node object at +0x94

# Calls stubbed or given
GET_PLAYER = 0x751D50  # the computer player's vtable +0x1C, stubbed: the player
IS_WORKSHOP = 0x7792C0  # the workshop's vtable +0x310: 1
GET_POSITION = 0x4019F0  # vtable +0x170: the map position at +0x14
GET_SCALE = 0x402520  # vtable +0x120, stubbed: the scale; ST0
PLACE_PULL = 0x658630  # thiscall, arg place*; RET 4; ST0
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table the distance reads, as the game does at start-up
IS_FUNCTIONAL = 0x406200  # the workshop's vtable +0xD4, stubbed
GET_RESOURCE = 0x404D30  # the workshop's vtable +0x98, stubbed: the wood; RET 4
SPACE_IN_STORE = 0x779AF0  # stubbed: the room for scaffolds
GET_INFO = 0x779A50  # stubbed: the scaffold table row, with the wood for a scaffold at +0x6C

FOR_ABODES = 5
FOR_CIVIC = 6
RAW_DESIRE = 0x19C  # the first of the three parts of a town's desire that the raw desire adds


def scale_bits(value: int) -> int:
    """The scale of a scaffold of a worth, as the game makes it: value * 0.2 + 0.5 in floats"""
    product = struct.unpack("<f", struct.pack("<f", value * struct.unpack("<f", struct.pack("<f", 0.2))[0]))[0]
    return f2u(product + 0.5)


class BestScaffold:
    COLUMNS = ["hand_x", "hand_z", "scaffold_x", "scaffold_z", "power", "value", "scale", "abodes", "civic",
               "adult_places", "pull", "best", "found"]

    def __init__(self, o: Oracle):
        self.o = o
        o.call(MAKE_INVERSE_SQRT_TABLE)
        self.player = o.alloc(0x100)
        player_vtable = o.alloc(0x100)
        self.state = o.alloc(0x100)
        self.gplayer = o.alloc(0xB00)
        self.town = o.alloc(0xA00)
        self.workshop = o.alloc(0x200)
        workshop_vtable = o.alloc(0x400)
        self.scaffold = o.alloc(0x100)
        scaffold_vtable = o.alloc(0x200)
        list_node = o.alloc(8)
        self.found = o.alloc(8)
        o.write_u32(self.player, player_vtable)
        o.write_u32(player_vtable + 0x1C, GET_PLAYER)
        o.write_u32(self.player + 0x34, self.state)
        o.stub(GET_PLAYER, eax=self.gplayer, pop=0)
        o.write_u32(self.gplayer + 0xA50, self.town)
        o.write_u32(self.town + 0x754, self.workshop)
        o.write_u32(self.town + 0x75C, 0)
        o.write_u32(self.workshop, workshop_vtable)
        o.write_u32(workshop_vtable + 0x310, IS_WORKSHOP)
        o.write_u32(self.workshop + 0x9C, 0)
        o.write_u32(self.workshop + 0xE0, list_node)
        o.write_u32(list_node, 0, self.scaffold)
        o.write_u32(self.scaffold, scaffold_vtable)
        o.write_u32(scaffold_vtable + 0x170, GET_POSITION)
        o.write_u32(scaffold_vtable + 0x120, GET_SCALE)

    def run(self, hx: int, hz: int, sx: int, sz: int, power: int, scale: int, abodes: int, civic: int,
            places: int, pull: int) -> list[int]:
        o = self.o
        o.write_u32(self.state + 0x14, hx, hz, 0)
        o.write_u32(self.scaffold + 0x14, sx, sz, 0)
        o.write_u32(self.gplayer + 0x8C, power)
        o.stub(GET_SCALE, st0=f32.of_bits(scale), pop=0)
        o.write_u32(self.town + RAW_DESIRE + 4 * FOR_ABODES, abodes)
        o.write_u32(self.town + RAW_DESIRE + 4 * FOR_CIVIC, civic)
        o.write_u32(self.town + 0x644, places)
        o.stub(PLACE_PULL, st0=f32.of_bits(pull), pop=4)
        o.write_u32(self.found, 0)
        result = o.call(BEST_SCAFFOLD, this=self.player, args=[self.found])
        assert result.popped == 4, result
        return [result.st0f, int(o.read_u32(self.found)[0] == self.scaffold)]


def best_scaffold_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        hx = rng.randint(0x100000, 0x1F00000)
        hz = rng.randint(0x100000, 0x1F00000)
        reach = rng.choice([rng.randint(0, 0x40000), rng.randint(0, 0x400000), 0])
        sx = hx + rng.randint(-reach, reach)
        sz = hz + rng.randint(-reach, reach)
        power = f2u(rng.choice([rng.uniform(1.0, 2000.0), 100.0, rng.uniform(100.0, 600.0), 0.0]))
        value = rng.randint(0, 8)
        abodes = f2u(rng.choice([0.0, rng.uniform(0.0, 1.0), rng.uniform(0.0, 3.0)]))
        civic = f2u(rng.choice([0.0, rng.uniform(0.0, 1.0), rng.uniform(0.0, 3.0)]))
        places = rng.choice([175, 176, rng.randint(0, 400), 0xFFFFFFF0])
        pull = f2u(rng.choice([1.0, rng.uniform(0.0, 1.0), 0.0]))
        rows.append((hx, hz, sx, sz, power, value, scale_bits(value), abodes, civic, places, pull))
    return rows


class FillWorkshop:
    COLUMNS = ["functional", "room", "wood", "wood_value", "most", "value", "chosen"]

    def __init__(self, o: Oracle):
        self.o = o
        self.player = o.alloc(0x100)
        player_vtable = o.alloc(0x100)
        self.gplayer = o.alloc(0xB00)
        self.town = o.alloc(0xA00)
        self.workshop = o.alloc(0x200)
        workshop_vtable = o.alloc(0x400)
        self.info = o.alloc(0x200)
        self.table_row = o.alloc(0x100)
        self.node = o.alloc(0x100)
        o.write_u32(self.player, player_vtable)
        o.write_u32(player_vtable + 0x1C, GET_PLAYER)
        o.stub(GET_PLAYER, eax=self.gplayer, pop=0)
        o.write_u32(self.gplayer + 0xA50, self.town)
        o.write_u32(self.town + 0x754, self.workshop)
        o.write_u32(self.town + 0x75C, 0)
        o.write_u32(self.workshop, workshop_vtable)
        o.write_u32(workshop_vtable + 0x310, IS_WORKSHOP)
        o.write_u32(workshop_vtable + 0xD4, IS_FUNCTIONAL)
        o.write_u32(workshop_vtable + 0x98, GET_RESOURCE)
        o.write_u32(self.workshop + 0x9C, 0)
        o.write_u32(self.workshop + 0x28, self.info)
        o.stub(GET_INFO, eax=self.table_row, pop=0)

    def run(self, functional: int, room: int, wood: int, wood_value: int, most: int) -> list[int]:
        o = self.o
        o.stub(IS_FUNCTIONAL, eax=functional, pop=0)
        o.stub(SPACE_IN_STORE, eax=room & 0xFFFFFFFF, pop=0)
        o.stub(GET_RESOURCE, eax=wood, pop=4)
        o.write_u32(self.table_row + 0x6C, wood_value)
        o.write_u32(self.info + 0x1A8, most)
        o.write_u32(self.node + 0x94, 0)
        result = o.call(FILL_WORKSHOP, this=self.player, args=[self.node])
        assert result.popped == 4, result
        return [result.st0f, int(o.read_u32(self.node + 0x94)[0] == self.workshop)]


def fill_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(2000):
        functional = rng.choice([1, 1, 1, 0])
        room = rng.choice([0, 1, 2, 3, -1, rng.randint(-5, 5)])
        wood_value = rng.choice([100, rng.randint(0, 300)])
        wood = rng.choice([0, wood_value, wood_value - 1, rng.randint(0, 400), 0xFFFFFFFF])
        most = f2u(rng.choice([3.0, 1.0, rng.uniform(0.5, 10.0)]))
        rows.append((functional, room, wood & 0xFFFFFFFF, wood_value, most))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x669580)
    with Oracle() as oracle:
        best = BestScaffold(oracle)
        rows = [[*case, *best.run(*case[0:5], *case[6:11])] for case in best_scaffold_cases(rng)]
        n = write_pinned(out / "computer_scaffolds_use.txt", "How good a scaffold of a workshop is for the god to use",
                         ["Function: thiscall on the computer player, one town with one workshop with one scaffold.",
                          "Places are 16.16 map values. Power, scale, desires, pull and best are float bits.",
                          "value is the worth that the scale gives. The raw desires have only their first part."],
                         BestScaffold.COLUMNS, rows)
        print(f"computer_scaffolds_use: {n} rows")
        oracle.reset()
        fill = FillWorkshop(oracle)
        rows = [[*case, *fill.run(*case)] for case in fill_cases(rng)]
        n = write_pinned(out / "computer_scaffolds_fill.txt", "How much a workshop wants wood for the god",
                         ["Function: thiscall on the computer player, one town with one workshop.",
                          "room is signed. most (the most scaffolds that the workshop makes) and value are float bits."],
                         FillWorkshop.COLUMNS, rows)
        print(f"computer_scaffolds_fill: {n} rows")


if __name__ == "__main__":
    main()
