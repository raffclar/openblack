#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of a town's want of a building, with the value of the scaffolds that offer it, in
test/oracle/data:

- scaffold_desire.txt: the building's info (its type, its abode number, its want, the scaffolds it needs and the most
  adults in it), the value of the scaffolds, the town's building sites (of the same type and of another type) and what
  the town has (room, homeless, adults, children, the count of each abode number, its neutral flag, its want of a wonder
  and the trigger of that want) in; the want out. The game's own function runs: it counts the sites of the type, takes
  the want of the type, shares it over the sites, and takes off a part for each scaffold above the ones the building
  needs.

Usage: python tools/oracle/cases/scaffold_desire_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
DESIRE_TO_BE_BUILT = 0x73A1A0  # thiscall (town) (info, scaffold value); RET 8; the want on ST0
IS_NEUTRAL = 0x64AC00  # thiscall (player): stubbed
GET_DESIRE = 0x73E400  # thiscall (town) (desire); RET 4: stubbed, the want of a wonder
DESIRE_INFO = 0x745F80  # thiscall (the town's desires) (desire); RET 4: stubbed, the row of the wonder want

# Info fields
INFO_SCAFFOLDS = 0x10C
INFO_DESIRE = 0x114
INFO_MAX_ADULTS = 0x174
INFO_TYPE = 0x200  # our own fields, read by our own code in the virtual slots
INFO_NUMBER = 0x204
SLOT_TYPE = 0x40
SLOT_NUMBER = 0x44

# Town fields (from the disassembly)
TOWN_PIT = 0x30
ADULTS = 0x618
CHILDREN = 0x61C
ADULT_ROOM = 0x65C
BUILT = 0x718  # one byte for each abode number
TOTEM = 0x740
CRECHE = 0x744
GRAVEYARD = 0x748
WORKSHOPS = 0x750
HOMELESS = 0x76C
SITES = 0x790
CENTRE = 0x9A4
FOOTBALL = 0xEA4
SLOT_PLAYER = 0x1C

# Abode numbers
TOTEM_N, PIT_N, CRECHE_N, WORKSHOP_N, WONDER_N, GRAVEYARD_N, CENTRE_N, FOOTBALL_N, DISPENSER_N, FIELD_N = (
    6, 7, 8, 9, 10, 11, 12, 13, 14, 15)

# The types and the abode numbers that go with them
TYPES = [
    (0x1, None), (0x2, None), (0x4, None), (0xA, None), (0x14, TOTEM_N), (0x24, PIT_N), (0x44, CRECHE_N),
    (0x84, WORKSHOP_N), (0x100, WONDER_N), (0x204, GRAVEYARD_N), (0x404, CENTRE_N), (0x804, None),
    (0x1004, FOOTBALL_N), (0x2004, DISPENSER_N), (0x4004, FIELD_N),
]

COLUMNS = (["type", "number", "desire", "scaffolds", "max_adults", "value", "sites_same", "sites_other",
            "adult_room", "homeless", "adults", "children"] + [f"built{i}" for i in range(16)]
           + ["neutral", "wonder_desire", "wonder_trigger", "result"])


def code_return_field(offset: int) -> bytes:
    """MOV EAX, [ECX + offset]; RET"""
    return bytes([0x8B, 0x81]) + struct.pack("<I", offset) + bytes([0xC3])


def code_return(value: int) -> bytes:
    """MOV EAX, value; RET"""
    return bytes([0xB8]) + struct.pack("<I", value) + bytes([0xC3])


CODE_RETURN_THIS = bytes([0x8B, 0xC1, 0xC3])


def desire_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(0x5CAFD)
    o.stub(DESIRE_INFO, eax=0, pop=4)

    def code(data: bytes) -> int:
        address = o.alloc(0x10)
        o.write(address, data)
        return address

    returns_one = code(code_return(1))
    returns_this = code(CODE_RETURN_THIS)
    returns_type = code(code_return_field(0x10))

    # The info and its virtual slots
    info_vtable = o.alloc(0x100)
    o.write_u32(info_vtable + SLOT_TYPE, code(code_return_field(INFO_TYPE)))
    o.write_u32(info_vtable + SLOT_NUMBER, code(code_return_field(INFO_NUMBER)))
    info = o.alloc(0x300)
    o.write_u32(info, info_vtable)

    # A building of a site: alive, the building itself, and its type at +0x10
    building_vtable = o.alloc(0x900)
    o.write_u32(building_vtable + 0x2C, returns_one)
    o.write_u32(building_vtable + 0x8BC, returns_this)
    o.write_u32(building_vtable + 0x8C4, returns_type)
    most_sites = 6
    buildings = [o.alloc(0x20) for _ in range(most_sites)]
    sites = [o.alloc(0x20) for _ in range(most_sites)]
    nodes = [o.alloc(0x10) for _ in range(most_sites)]
    for building, site, node in zip(buildings, sites, nodes):
        o.write_u32(building, building_vtable)
        o.write_u32(site + 0x14, building)
        o.write_u32(node + 4, site)

    # The town, its player and its storage pit
    player = o.alloc(0x10)
    town_vtable = o.alloc(0x100)
    o.write_u32(town_vtable + SLOT_PLAYER, code(code_return(player)))
    pit_vtable = o.alloc(0x40)
    o.write_u32(pit_vtable + 0x2C, returns_one)
    pit = o.alloc(0x10)
    o.write_u32(pit, pit_vtable)
    marker = o.alloc(0x10)  # a building that a town field points at
    trigger_row = o.alloc(0x40)
    town = o.alloc(0x1000)

    rows = []
    for index in range(3000):
        kind, number = TYPES[index % len(TYPES)]
        if number is None:
            number = rng.randrange(6) if kind in (0x2, 0xA) else rng.randrange(16)
        desire = rng.choice([0.0, rng.uniform(0.0, 1.0), rng.uniform(0.0, 1.0), rng.uniform(0.0, 3.0), 0.5])
        scaffolds = rng.choice([0, 1, 1, 2, 3, 5])
        value = rng.choice([0, scaffolds, scaffolds + rng.randrange(1, 8), scaffolds + rng.randrange(0, 3)])
        max_adults = rng.choice([1, 2, 4, 6, 10, 12, 20])
        same = rng.choice([0, 0, 1, 2, 3])
        other = rng.choice([0, 0, 1, 2])
        adults = rng.randrange(0, 120)
        children = rng.randrange(0, 60)
        homeless = rng.randrange(0, 30)
        adult_room = rng.randrange(-20, 60)
        built = [rng.choice([0, 0, 0, 1, 2]) for _ in range(16)]
        neutral = 1 if rng.random() < 0.15 else 0
        wonder_desire = rng.uniform(0.0, 1.0)
        wonder_trigger = rng.uniform(0.0, 1.0)

        o.write(info + 0x100, bytes(0x100))
        o.write_u32(info + INFO_SCAFFOLDS, scaffolds)
        o.write_u32(info + INFO_DESIRE, f2u(desire))
        o.write_u32(info + INFO_MAX_ADULTS, max_adults)
        o.write_u32(info + INFO_TYPE, kind, number)

        o.write(town, bytes(0x1000))
        o.write_u32(town, town_vtable)
        o.write_u32(town + ADULTS, adults, children)
        o.write_u32(town + ADULT_ROOM, adult_room & 0xFFFFFFFF)
        o.write_u32(town + HOMELESS, homeless)
        o.write(town + BUILT, bytes(built))
        o.write_u32(town + TOTEM, marker if built[TOTEM_N] else 0)
        o.write_u32(town + CRECHE, marker if built[CRECHE_N] else 0)
        o.write_u32(town + GRAVEYARD, marker if built[GRAVEYARD_N] else 0)
        o.write_u32(town + WORKSHOPS, built[WORKSHOP_N])
        o.write_u32(town + CENTRE, marker if built[CENTRE_N] else 0)
        o.write_u32(town + FOOTBALL, marker if built[FOOTBALL_N] else 0)
        o.write_u32(town + TOWN_PIT, pit if built[PIT_N] else 0)
        types = [kind] * same + [0x4 if kind != 0x4 else 0x2] * other
        rng.shuffle(types)
        head = 0
        for slot in reversed(range(len(types))):
            o.write_u32(buildings[slot] + 0x10, types[slot])
            o.write_u32(nodes[slot], head)
            head = nodes[slot]
        o.write_u32(town + SITES, head)

        o.stub(IS_NEUTRAL, eax=neutral)
        o.stub(GET_DESIRE, st0=wonder_desire, pop=4)
        o.write_u32(trigger_row + 0x18, f2u(wonder_trigger))
        o.stub(DESIRE_INFO, eax=trigger_row, pop=4)

        result = o.call(DESIRE_TO_BE_BUILT, this=town, args=[info, value])
        if result.popped != 8 or result.depth != 1:
            raise RuntimeError(f"case {index}: popped {result.popped}, depth {result.depth}")
        rows.append([kind, number, f2u(desire), scaffolds, max_adults, value, same, other,
                     adult_room & 0xFFFFFFFF, homeless, adults, children] + built
                    + [neutral, f2u(wonder_desire), f2u(wonder_trigger), result.st0f])
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as o:
        rows = desire_cases(o)
    write_pinned(out / "scaffold_desire.txt", "A town's want of a building, with the value of the scaffolds that offer it",
                 ["the building's type, abode number, want, scaffolds needed and most adults; the scaffold value;",
                  "the sites of the same type and of another type; the town's adult room, homeless, adults, children;",
                  "the count of each abode number; the neutral flag; the wonder want and its trigger in;",
                  "the want out (the game's football flag is off)"], COLUMNS, rows)
    print(f"scaffold_desire {len(rows)}")


if __name__ == "__main__":
    main()
