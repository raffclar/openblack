#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the belief that a scaffold gives, in test/oracle/data:

- scaffold_impressive.txt: the scaffold's scale, the impressiveness of its table, the land's balance of things, the
  reaction's multiplier, the distance, the reach of the reaction, the town's boredom, the artefact multiplier, whether
  the scaffold is on a site, whether the building of the site is an abode, and the raw want, the boost and the second
  boost of the want for abodes and of the want for civic buildings in; the impressiveness out. The game's own function
  runs with the function of a thing in the world in it; the calls that look up the multiplier, the distance, the boredom
  and the artefact are stubs.
- scaffold_spread.txt: the reach and the part of a town to impress, then up to three towns in the order the game walks
  them, each with its distance, its adults and children and the people of up to three abodes, in; the count of the
  people impressed and a sum of their order out. The game's own function runs; the distance is a stub that reads it from
  the town, and each impressed villager writes its number into a list.

Usage: python tools/oracle/cases/scaffold_impression_cases.py [output folder]   (default: test/oracle/data)
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
SCAFFOLD_IMPRESSIVE = 0x6EA770  # thiscall (scaffold) (living, reaction); RET 8; the value on ST0
GET_BUILDING = 0x43BC70  # thiscall (site); stubbed: the building
DEFAULT_MULTIPLIER = 0x6E48A0  # thiscall (reaction) (living); RET 4; stubbed
DISTANCE_IN_METRES = 0x74CD70  # cdecl (a, b); stubbed
REACTION_INFO = 0x6E4700  # thiscall (reaction); stubbed: the row of the reaction
BOREDOM_MULTIPLIER = 0x56FE70  # thiscall (living) (reaction); RET 4; stubbed
ARTEFACT_MODIFIER = 0x639900  # thiscall (object); stubbed
OBJECT_IMPRESSIVE = 0x639390  # thiscall (object): the land's balance times the table value (runs)
GET_POWER = 0x56FE60  # thiscall: 1 (runs)
GET_SCALE = 0x402520  # thiscall: the scale at +0x50 (runs)
THINGS_BALANCE = 0xD1A284

SPREAD = 0x6EAA80  # thiscall (scaffold) (reaction); RET 4
MAP_DISTANCE = 0x605CD0  # thiscall (coords) (coords); RET 4: replaced by code that reads the town's distance
GAME = 0xD0195C
GAME_TOWNS = 0x205C84

VALUE_COLUMNS = ["scale", "table_value", "balance", "multiplier", "distance", "reach", "boredom", "artefact", "on_site",
                 "abode", "raw_abodes", "boost_abodes", "cheat_abodes", "raw_civic", "boost_civic", "cheat_civic",
                 "result"]

TOWNS = 3
ABODES = 3
SPREAD_COLUMNS = (["reach", "proportion", "towns"]
                  + [f"t{t}_{name}" for t in range(TOWNS) for name in
                     ["distance", "adults", "children", "abodes"] + [f"a{a}" for a in range(ABODES)]]
                  + ["impressed", "order_sum"])


def code(o: Oracle, data: bytes) -> int:
    address = o.alloc(0x20)
    o.write(address, data)
    return address


def code_return(value: int) -> bytes:
    """MOV EAX, value; RET"""
    return bytes([0xB8]) + struct.pack("<I", value) + bytes([0xC3])


def value_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(0x5CAF1)
    reaction_info = o.alloc(0x100)
    o.stub(REACTION_INFO, eax=reaction_info)
    living = o.alloc(0x40)
    reaction = o.alloc(0x40)
    info = o.alloc(0x200)
    vtable = o.alloc(0x900)
    o.write_u32(vtable + 0x66C, OBJECT_IMPRESSIVE)
    o.write_u32(vtable + 0x128, GET_POWER)
    o.write_u32(vtable + 0x120, GET_SCALE)
    scaffold = o.alloc(0x100)
    o.write_u32(scaffold, vtable)
    o.write_u32(scaffold + 0x28, info)
    site = o.alloc(0x40)
    town = o.alloc(0x400)
    building_vtable = o.alloc(0x300)
    o.write_u32(building_vtable + 0x48, code(o, code_return(town)))
    is_abode = [code(o, code_return(0)), code(o, code_return(1))]
    building = o.alloc(0x40)
    o.write_u32(building, building_vtable)
    o.stub(GET_BUILDING, eax=building)

    rows = []
    for index in range(3000):
        scale = rng.choice([0.5, 0.7, 1.0, 1.2, 1.6, 2.0, 2.4, rng.uniform(0.4, 3.0), rng.uniform(0.4, 3.0)])
        table_value = rng.choice([0.0, 1.0, rng.uniform(0.0, 5.0), rng.uniform(0.0, 50.0)])
        balance = rng.choice([1.0, 1.0, rng.uniform(0.2, 3.0)])
        multiplier = rng.choice([1.0, rng.uniform(0.0, 2.0), rng.uniform(-1.0, 3.0)])
        reach = rng.choice([50.0, 100.0, rng.uniform(5.0, 200.0)])
        distance = rng.choice([0.0, rng.uniform(0.0, reach), rng.uniform(0.0, reach * 1.5), reach])
        boredom = rng.choice([1.0, rng.uniform(0.0, 1.0)])
        artefact = rng.choice([1.0, 1.0, rng.uniform(1.0, 3.0)])
        on_site = 1 if rng.random() < 0.7 else 0
        abode = 1 if rng.random() < 0.6 else 0
        wants = [rng.choice([0.0, rng.uniform(-1.0, 1.0), rng.uniform(0.0, 2.0)]) for _ in range(6)]

        o.write_u32(THINGS_BALANCE, f2u(balance))
        o.write_u32(info + 0xE8, f2u(table_value))
        o.write_u32(scaffold + 0x50, f2u(scale))
        o.write_u32(scaffold + 0x68, site if on_site else 0)
        o.write_u32(reaction_info + 0x2C, f2u(reach))
        o.write_u32(building_vtable + 0x208, is_abode[abode])
        # The want for abodes and for civic buildings: the raw want, the boost and the second boost of each
        for offset, want in zip([0x1B0, 0x11C, 0xD8, 0x1B4, 0x120, 0xDC], wants):
            o.write_u32(town + offset, f2u(want))
        o.stub(DEFAULT_MULTIPLIER, st0=multiplier, pop=4)
        o.stub(DISTANCE_IN_METRES, st0=distance)
        o.stub(BOREDOM_MULTIPLIER, st0=boredom, pop=4)
        o.stub(ARTEFACT_MODIFIER, st0=artefact)

        result = o.call(SCAFFOLD_IMPRESSIVE, this=scaffold, args=[living, reaction])
        if result.popped != 8 or result.depth != 1:
            raise RuntimeError(f"case {index}: popped {result.popped}, depth {result.depth}")
        rows.append([f2u(scale), f2u(table_value), f2u(balance), f2u(multiplier), f2u(distance), f2u(reach),
                     f2u(boredom), f2u(artefact), on_site, abode] + [f2u(w) for w in wants] + [result.st0f])
    return rows


def spread_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(0x5CAF2)
    # The distance of a town is the float at the start of its place: MOV EAX, [ESP + 4]; FLD [EAX]; RET 4
    o.write(MAP_DISTANCE, bytes([0x8B, 0x44, 0x24, 0x04, 0xD9, 0x00, 0xC2, 0x04, 0x00]))
    count = o.alloc(0x10)
    log = o.alloc(0x400)
    # Each impressed villager writes its number (at +0x10) into the list
    recorder = code(o, bytes([0x8B, 0x41, 0x10])  # MOV EAX, [ECX + 0x10]
                    + bytes([0x8B, 0x15]) + struct.pack("<I", count)  # MOV EDX, [count]
                    + bytes([0x89, 0x04, 0x95]) + struct.pack("<I", log)  # MOV [EDX * 4 + log], EAX
                    + bytes([0xFF, 0x05]) + struct.pack("<I", count)  # INC [count]
                    + bytes([0xC2, 0x08, 0x00]))  # RET 8
    villager_vtable = o.alloc(0xA00)
    o.write_u32(villager_vtable + 0x98C, recorder)
    info = o.alloc(0x200)
    scaffold = o.alloc(0x100)
    o.write_u32(scaffold + 0x28, info)
    holder = o.alloc(0x10)
    o.write_u32(GAME, holder - GAME_TOWNS)
    node = o.alloc(0x10)
    o.write_u32(holder, node)
    towns = [o.alloc(0x800) for _ in range(TOWNS)]
    abodes = [[o.alloc(0x100) for _ in range(ABODES)] for _ in range(TOWNS)]
    most = 6
    villagers = [[[o.alloc(0x100) for _ in range(most)] for _ in range(ABODES)] for _ in range(TOWNS)]
    for t in range(TOWNS):
        for a in range(ABODES):
            for v in range(most):
                o.write_u32(villagers[t][a][v], villager_vtable)
                o.write_u32(villagers[t][a][v] + 0x10, t * 100 + a * 10 + v + 1)

    rows = []
    for index in range(2000):
        reach = rng.choice([100.0, 200.0, rng.uniform(10.0, 300.0)])
        proportion = rng.choice([0.25, 0.5, rng.uniform(0.0, 1.0), rng.uniform(0.0, 0.1)])
        town_count = rng.randrange(1, TOWNS + 1)
        o.write_u32(info + 0x11C, f2u(reach), f2u(proportion))
        o.write_u32(node + 4, towns[0])
        row = [f2u(reach), f2u(proportion), town_count]
        for t in range(TOWNS):
            distance = rng.choice([0.0, rng.uniform(0.0, reach), rng.uniform(0.0, reach * 1.5), reach])
            adults = rng.randrange(0, 40)
            children = rng.randrange(0, 20)
            abode_count = rng.randrange(0, ABODES + 1)
            people = [rng.randrange(0, most + 1) for _ in range(ABODES)]
            town = towns[t]
            o.write_u32(town + 0x14, f2u(distance))
            o.write_u32(town + 0x618, adults, children)
            o.write_u32(town + 0x75C, towns[t + 1] if t + 1 < town_count else 0)
            o.write_u32(town + 0x754, abodes[t][0] if abode_count > 0 else 0)
            for a in range(ABODES):
                abode = abodes[t][a]
                o.write_u32(abode + 0x9C, abodes[t][a + 1] if a + 1 < abode_count else 0)
                o.write_u32(abode + 0xA0, villagers[t][a][0] if people[a] > 0 else 0)
                for v in range(most):
                    o.write_u32(villagers[t][a][v] + 0xE4, villagers[t][a][v + 1] if v + 1 < people[a] else 0)
            row += [f2u(distance), adults, children, abode_count] + people
        o.write_u32(count, 0)
        result = o.call(SPREAD, this=scaffold, args=[0])
        if result.popped != 4:
            raise RuntimeError(f"case {index}: popped {result.popped}")
        impressed = o.read_u32(count)[0]
        order = o.read_u32(log, impressed) if impressed else []
        row += [impressed, sum((i + 1) * number for i, number in enumerate(order)) & 0xFFFFFFFF]
        rows.append(row)
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as o:
        values = value_cases(o)
        o.reset()
        spreads = spread_cases(o)
    write_pinned(out / "scaffold_impressive.txt", "How impressive a scaffold is to one watcher",
                 ["the scale, the table value, the land's balance of things, the multiplier, the distance, the reach,",
                  "the boredom, the artefact multiplier, on a site, an abode, the raw want, boost and second boost of",
                  "the want for abodes and for civic buildings in; the impressiveness out"], VALUE_COLUMNS, values)
    write_pinned(out / "scaffold_spread.txt", "The people of the near towns that a new building site impresses",
                 ["the reach and the part of a town to impress, the count of the towns the game walks, then for each",
                  "town its distance, adults, children, count of abodes and the people of each abode in; the count of",
                  "the people impressed and the sum of (place + 1) * (100 town + 10 abode + person + 1) out"],
                 SPREAD_COLUMNS, spreads)
    print(f"scaffold_impressive {len(values)} scaffold_spread {len(spreads)}")


if __name__ == "__main__":
    main()
