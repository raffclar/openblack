#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the food and the wood that a computer god brings to its towns, in test/oracle/data:

- computer_food_shortfall.txt: the food that the town with the most need of food wants more than it has. The inputs
  are the food that the town wants (bits) and the food that it has. The result is the whole number that the god keeps.
- computer_fish_farm_wish.txt: how much the god wants the food of a fish farm. The inputs are the distance from the
  hand to the farm and the pull of the farm's place (bits). The result is the bits of the wish.
- computer_giving_town.txt: the other town that gives food to the town above. The inputs are the need of the node
  above, the pull of each pit (the same for all towns of a row), and the food in the pit and the raw desire for food of
  three towns. The results are the bits of the value, the town that gives (0 to 2, or ffffffff for none) and the bits
  of the food that the god asks for.

The game's rule functions run as they are. The calls they make to the town's food counts, to the hand's place, to the
distance and to the pull of a place are stubs that give the inputs. The town's raw desire, its storage pit, the pit's
food and the pit's food pile are laid out in memory and read by the game's own code.

Usage: python tools/oracle/cases/computer_town_goods_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
TOWN_FOOD_NEED = 0x666C50  # thiscall (computer player, node), ret 4
FISH_FARM_WISH = 0x667030  # thiscall (computer player, node), ret 4
FOOD_FROM_OTHER_TOWN = 0x667120  # thiscall (computer player, node), ret 4
GET_DESIRE = 0x73E400  # thiscall (town, desire), ret 4
FOOD_WANTED = 0x747AF0  # thiscall (town), ST0
FOOD_HELD = 0x747A90  # thiscall (town), EAX
HAND_COORDS = 0x657FE0  # thiscall (computer player, out), ret 4
DISTANCE = 0x605CD0  # thiscall (coords, coords), ret 4, ST0
PLACE_PULL = 0x658630  # thiscall (computer player, coords), ret 4, ST0
OBJECT_ABOVE = 0x655CC0  # thiscall (node), EAX
IS_AVAILABLE = 0x401810  # the game's own check that a thing is in the world
ABODE_RESOURCE = 0x404D30  # the game's own count of a building's food or wood

# Offsets
PLAYER = 0x28  # computer player: its player
TOWNS = 0xA50  # player: first town
NEXT_TOWN = 0x75C  # town: next town
FARMS = 0x788  # town: list of fish farms
PIT = 0x30  # town: storage pit
RAW = 0x19C  # town: raw desire for food (then the boosts at 0x108 and 0xC4)
PIT_FOOD = 0xBC  # pit: count of food
PIT_FOOD_PILE = 0xC4  # pit: food pile
PILE_AMOUNT = 0x70
NODE_PARENT = 0xC
NODE_VALUE = 0xA4
NODE_AMOUNT = 0x78
NODE_ASK = 0x80
NODE_OBJECT = 0x94


def food_shortfall(o: Oracle, pick: random.Random) -> list[list[int]]:
    cases = [(w, h) for w in (0.0, 0.5, 1.0, 99.5, 100.0, 100.49, 250.75, 1000.0, 16777216.0, 16777217.0)
             for h in (0, 1, 50, 100, 101, 999, 16777215, 16777217, 0x7FFFFFFF)]
    cases += [(pick.uniform(0.0, 5000.0), pick.randrange(0, 6000)) for _ in range(400)]
    rows = []
    for wanted, held in cases:
        o.reset()
        town = o.object(0x800)
        player = o.object(0xB00, {TOWNS: town})
        computer = o.object(0x200, {PLAYER: player})
        node = o.object(0xB0)
        o.stub(GET_DESIRE, pop=4, st0=0.5)
        o.stub(FOOD_WANTED, st0=f32(wanted))
        o.stub(FOOD_HELD, eax=held)
        o.call(TOWN_FOOD_NEED, this=computer, args=[node])
        rows.append([f32(wanted).bits, held, o.read_u32(node + NODE_AMOUNT)[0]])
    return rows


def fish_farm_wish(o: Oracle, pick: random.Random) -> list[list[int]]:
    cases = [(d, p) for d in (0.0, 0.5, 1.0, 10.0, 100.0, 124.9, 125.0, 249.0, 250.0, 251.0, 1000.0)
             for p in (0.0, 0.01, 0.25, 0.5, 0.999, 1.0)]
    cases += [(pick.uniform(0.0, 400.0), pick.uniform(0.0, 1.0)) for _ in range(400)]
    rows = []
    for distance, pull in cases:
        o.reset()
        farm = o.object(0x100)
        link = o.object(8, {4: farm})
        town = o.object(0x800, {FARMS: link})
        player = o.object(0xB00, {TOWNS: town})
        computer = o.object(0x200, {PLAYER: player})
        coords = o.object(0x10)
        node = o.object(0xB0)
        o.stub(HAND_COORDS, eax=coords, pop=4)
        o.stub(DISTANCE, pop=4, st0=f32(distance))
        o.stub(PLACE_PULL, pop=4, st0=f32(pull))
        r = o.call(FISH_FARM_WISH, this=computer, args=[node])
        rows.append([f32(distance).bits, f32(pull).bits, r.st0f])
    return rows


def giving_town(o: Oracle, pick: random.Random) -> list[list[int]]:
    cases = []
    for need in (0.0, 0.04, 0.05, 0.050001, 0.3, 0.9, 1.0, 1.5):
        for pull in (0.0, 0.5, 1.0):
            cases.append((need, pull, [(5000, 0.1), (5000, 0.05), (10, 0.0)]))
            cases.append((need, pull, [(900, 0.0), (901, 0.2), (2000, 1.5)]))
    for _ in range(500):
        need = pick.uniform(0.0, 1.2)
        pull = pick.choice([0.0, 1.0, pick.uniform(0.0, 1.0)])
        towns = [(pick.randrange(0, 2000), pick.uniform(-0.5, 1.5)) for _ in range(3)]
        cases.append((need, pull, towns))
    rows = []
    for need, pull, towns in cases:
        o.reset()
        table = o.object(0x100, {0x2C: IS_AVAILABLE, 0x98: ABODE_RESOURCE})
        objects = []
        for stock, raw in towns:
            pile = o.object(0x100, {0: table, PILE_AMOUNT: 1})
            pit = o.object(0x100, {0: table, PIT_FOOD: stock, PIT_FOOD_PILE: pile})
            town = o.object(0x800, {PIT: pit, RAW: f32(raw)})
            objects.append((town, pile))
        for (town, _), (after, _) in zip(objects, objects[1:]):
            o.write_u32(town + NEXT_TOWN, after)
        above = o.object(0x800)
        player = o.object(0xB00, {TOWNS: objects[0][0]})
        computer = o.object(0x200, {PLAYER: player})
        parent = o.object(0xB0, {NODE_VALUE: f32(need)})
        node = o.object(0xB0, {NODE_PARENT: parent})
        o.stub(OBJECT_ABOVE, eax=above)
        o.stub(PLACE_PULL, pop=4, st0=f32(pull))
        r = o.call(FOOD_FROM_OTHER_TOWN, this=computer, args=[node])
        chosen = o.read_u32(node + NODE_OBJECT)[0]
        index = next((i for i, (_, pile) in enumerate(objects) if pile == chosen), 0xFFFFFFFF)
        ask = o.read_u32(node + NODE_ASK)[0] if index != 0xFFFFFFFF else 0
        row = [f32(need).bits, f32(pull).bits]
        for stock, raw in towns:
            row += [stock, f32(raw).bits]
        rows.append(row + [r.st0f, index, ask])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x0B00_7A3D)
    with Oracle() as o:
        shortfall = food_shortfall(o, pick)
        farms = fish_farm_wish(o, pick)
        giving = giving_town(o, pick)
    count = write_pinned(
        out / "computer_food_shortfall.txt",
        "A computer god's town with the most need of food: the food it wants more than it has",
        ["wanted is the bits of the food that the town wants; held is the food that it has; amount is the whole",
         "number (two's complement) that the god keeps."],
        ["wanted", "held", "amount"],
        shortfall,
    )
    print(f"computer_food_shortfall: {count} rows")
    count = write_pinned(
        out / "computer_fish_farm_wish.txt",
        "A computer god's wish for the food of a fish farm",
        ["distance is the bits of the distance from the hand to the farm in metres; pull is the bits of the pull of",
         "the farm's place; value is the bits of the wish."],
        ["distance", "pull", "value"],
        farms,
    )
    print(f"computer_fish_farm_wish: {count} rows")
    count = write_pinned(
        out / "computer_giving_town.txt",
        "The other town of a computer god that gives food to the town above",
        ["need is the bits of the value of the node above; pull is the bits of the pull of each storage pit; stockN",
         "and rawN are the food in the pit and the bits of the raw desire for food of town N, in the order of the",
         "towns. value is the bits of the result, index the town that gives (ffffffff for none), ask the bits of the",
         "food that the god asks for (0 for none)."],
        ["need", "pull", "stock0", "raw0", "stock1", "raw1", "stock2", "raw2", "value", "index", "ask"],
        giving,
    )
    print(f"computer_giving_town: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
