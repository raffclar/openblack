#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the war rules of a computer god, in test/oracle/data:

- computer_war_town_target.txt: the thing in a town that the god most wants to destroy. The town has one building and
  one field, either of which can be missing. The built share and the life of the building are stubs; the distances
  and their nearness run in the game. chosen is 0 for none, 1 for the building and 2 for the field.
- computer_war_attack_creature.txt: how much the god wants to attack the creature of another player, from the map
  positions of its hand and the creature. The player above the node, the ally test and the most influential player
  are stubs.

Usage: python tools/oracle/cases/computer_war_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
TOWN_TARGET = 0x740C50  # thiscall (town, hand coords or 0), RET 4: the thing
ATTACK_CREATURE = 0x65D0D0  # thiscall (computer player, node), RET 4: the wish in ST0
OBJECT_ABOVE = 0x655CC0  # thiscall (node): the first object above the node
DYNAMIC_CAST = 0x7C5A19  # cdecl, 5 arguments
IS_ALLIED = 0x64D5D0  # thiscall (player, other), RET 4
MOST_INFLUENTIAL = 0x5CD630  # cdecl (coords, 0): the result is not used
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table that the distance reads
# Functions that the oracle makes stubs of, for the virtual calls of the building and the player
BUILT_GETTER = 0x5DC8B0
LIFE_GETTER = 0x4366F0
PLAYER_GETTER = 0x7DB790

# The town: its list of buildings (each links to the next at +0x9C) and its list of fields (nodes of next, field)
TOWN_ABODES = 0x754
TOWN_FIELDS = 0x780
ABODE_NEXT = 0x9C
# A thing: its map position at +0x14 and its table row at +0x28; a field's row is at +0x120 and its food at +0xDC
COORDS = 0x14
INFO = 0x28
FIELD_INFO = 0x120
FIELD_FOOD = 0xDC
# A table row: how much computer gods want to attack the thing at +0xFC; a field row's total food at +0x130
ATTACK_DESIRE = 0xFC
TOTAL_FOOD = 0x130
# Virtual calls of a building
VT_BUILT = 0x880
VT_LIFE = 0x11C
# The computer player and the hand; the player keeps its creature at +0xA4C; the node keeps its object at +0x94
CP_ACTION = 0x34
ACTION_HAND = 0x14
VT_PLAYER = 0x1C
PLAYER_CREATURE = 0xA4C
NODE_OBJECT = 0x94


def f(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


def place(rng: random.Random) -> tuple[int, int]:
    return rng.randrange(0x10000, 0x7FF0000), rng.randrange(0x10000, 0x7FF0000)


def near(rng: random.Random, x: int, z: int, reach_metres: float) -> tuple[int, int]:
    reach = int(reach_metres * 6553.6)
    return (min(max(x + rng.randrange(-reach, reach + 1), 0), 0x7FFFFFF),
            min(max(z + rng.randrange(-reach, reach + 1), 0), 0x7FFFFFF))


class TownTarget:
    COLUMNS = ["has_abode", "has_field", "hand_x", "hand_z", "abode_x", "abode_z", "built", "life", "abode_desire",
               "field_x", "field_z", "food", "total_food", "field_desire", "chosen"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        vtable = o.alloc(0x1000)
        o.write_u32(vtable + VT_BUILT, BUILT_GETTER)
        o.write_u32(vtable + VT_LIFE, LIFE_GETTER)
        self.abode_info = o.alloc(0x200)
        self.abode = o.object(0x200, {0: vtable, INFO: self.abode_info})
        self.field_info = o.alloc(0x200)
        self.field = o.object(0x200, {FIELD_INFO: self.field_info})
        self.node = o.object(0x10, {4: self.field})
        self.town = o.alloc(0x1000)
        self.hand = o.alloc(0x10)
        o.call(MAKE_INVERSE_SQRT_TABLE)

    def run(self, has_abode: int, has_field: int, hx: int, hz: int, ax: int, az: int, built: int, life: int,
            abode_desire: int, fx: int, fz: int, food: int, total: int, field_desire: int) -> list[int]:
        o = self.o
        o.write_u32(self.town + TOWN_ABODES, self.abode if has_abode else 0)
        o.write_u32(self.town + TOWN_FIELDS, self.node if has_field else 0)
        o.write_u32(self.hand, hx, hz, 0)
        o.write_u32(self.abode + COORDS, ax, az, 0)
        o.stub(BUILT_GETTER, pop=0, st0=f32.of_bits(built))
        o.stub(LIFE_GETTER, pop=0, st0=f32.of_bits(life))
        o.write_u32(self.abode_info + ATTACK_DESIRE, abode_desire)
        o.write_u32(self.field + COORDS, fx, fz, 0)
        o.write_u32(self.field + FIELD_FOOD, food)
        o.write_u32(self.field_info + TOTAL_FOOD, total)
        o.write_u32(self.field_info + ATTACK_DESIRE, field_desire)
        result = o.call(TOWN_TARGET, this=self.town, args=[self.hand])
        assert result.popped == 4 and result.depth == 0, result
        chosen = {0: 0, self.abode: 1, self.field: 2}[result.eax]
        return [chosen]


def town_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        hx, hz = place(rng)
        ax, az = near(rng, hx, hz, rng.choice([50.0, 500.0, 1200.0]))
        fx, fz = rng.choice([(ax, az), near(rng, hx, hz, rng.choice([50.0, 500.0, 1200.0]))])
        built = rng.choice([1.0, 0.0, rng.uniform(0.0, 1.0)])
        life = rng.choice([1.0, rng.uniform(0.0, 1.0)])
        abode_desire = rng.choice([0.0, 1.0, rng.uniform(0.0, 3.0)])
        total = rng.choice([100.0, 500.0, rng.uniform(1.0, 1000.0)])
        field_desire = rng.choice([0.0, 1.0, rng.uniform(0.0, 3.0)])
        food = rng.choice([0.0, total, rng.uniform(0.0, total)])
        if rng.random() < 0.3:
            # The field is worth what the building is worth, to test the order of the products and the tie
            food = life * built * abode_desire * total / field_desire if field_desire else food
        rows.append((int(rng.random() < 0.9), int(rng.random() < 0.9), hx, hz, ax, az, f2u(built), f2u(life),
                     f2u(abode_desire), fx, fz, f2u(food), f2u(total), f2u(field_desire)))
    return rows


class AttackCreature:
    COLUMNS = ["hand_x", "hand_z", "creature_x", "creature_z", "altitude", "wish"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.creature = o.alloc(0x200)
        self.other = o.object(0x1000, {PLAYER_CREATURE: self.creature})
        own = o.alloc(0x1000)
        o.stub(OBJECT_ABOVE, eax=self.other, pop=0)
        o.stub(DYNAMIC_CAST, eax=self.other, pop=0)
        o.stub(IS_ALLIED, eax=0, pop=4)
        o.stub(MOST_INFLUENTIAL, eax=0, pop=0)
        o.stub(PLAYER_GETTER, eax=own, pop=0)
        vtable = o.alloc(0x100)
        o.write_u32(vtable + VT_PLAYER, PLAYER_GETTER)
        self.state = o.alloc(0x100)
        self.player = o.object(0x100, {0: vtable, CP_ACTION: self.state})
        self.node = o.alloc(0x100)
        o.call(MAKE_INVERSE_SQRT_TABLE)

    def run(self, hx: int, hz: int, x: int, z: int, altitude: int) -> list[int]:
        o = self.o
        o.write_u32(self.state + ACTION_HAND, hx, hz, 0)
        o.write_u32(self.creature + COORDS, x, z, altitude)
        o.write_u32(self.node + NODE_OBJECT, 0)
        result = o.call(ATTACK_CREATURE, this=self.player, args=[self.node])
        assert result.popped == 4 and result.depth == 1, result
        assert o.read_u32(self.node + NODE_OBJECT)[0] == self.creature
        return [result.st0f]


def attack_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(3000):
        x, z = place(rng)
        hx, hz = near(rng, x, z, rng.choice([20.0, 99.0, 100.0, 101.0, 300.0, 499.0, 500.0, 900.0]))
        altitude = rng.choice([0.0, rng.uniform(-5.0, 60.0)])
        rows.append((hx, hz, x, z, f2u(altitude)))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x740C50)
    with Oracle() as oracle:
        target = TownTarget(oracle)
        rows = [[*case, *target.run(*case)] for case in town_cases(rng)]
        n = write_pinned(out / "computer_war_town_target.txt", "The thing in a town that a computer god attacks",
                         ["Function: thiscall on the town, arg (hand coords), RET 4: the thing. One building and",
                          "one field, each present when has_* is 1. built and life are stubs of the building's",
                          "virtual calls. chosen: 0 none, 1 the building, 2 the field. Floats are bits; coords are",
                          "16.16 map units at altitude 0."],
                         TownTarget.COLUMNS, rows)
        print(f"computer_war_town_target: {n} rows")
    with Oracle() as oracle:
        attack = AttackCreature(oracle)
        rows = [[*case, *attack.run(*case)] for case in attack_cases(rng)]
        n = write_pinned(out / "computer_war_attack_creature.txt", "The wish of a computer god to attack a creature",
                         ["Function: thiscall on the computer player, arg (node), RET 4; the wish in ST0. The other",
                          "player, the ally test and the most influential player are stubs. The hand is at",
                          "altitude 0; the creature at its altitude. Floats are bits; coords are 16.16 map units."],
                         AttackCreature.COLUMNS, rows)
        print(f"computer_war_attack_creature: {n} rows")


if __name__ == "__main__":
    main()
