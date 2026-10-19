#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of a town's desire for children, in test/oracle/data:

- town_children_desire.txt: the desire from the town's food desire, its room for children and adults, its wish for
  protection and mercy, its player's alignment and power over children, and its creche
- town_desire_significance.txt: how far a desire (with its boosts) is past the point where it makes villagers act

Usage: python tools/oracle/cases/town_children_desire_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
CHILDREN = 0x748430  # thiscall (town), no args, plain RET; the desire on ST0
SIGNIFICANCE = 0x746660  # thiscall (the town's desires, town + 0x34), arg desire; RET 4
DESIRE_INFO = 0x745F80  # stdcall (desire): the desire's info row; RET 4
# Two functions this path never calls, stubbed as the targets of the town's and the creche's virtual calls
GET_PLAYER_TARGET = 0x751D50
IS_FUNCTIONAL_TARGET = 0x750330

# Town fields (from the disassembly)
RAW = 0x19C
DESIRE = 0x14C
BOOST = 0x108
BOOST_A = 0xC4
ADULTS = 0x618
CHILDREN_COUNT = 0x61C
ADULT_PLACES = 0x644
CHILD_PLACES = 0x650
CRECHE = 0x744
# Player fields: the alignment object at +0x60 (its +8), the tribal powers from +0x68
PLAYER_ALIGNMENT = 0x60
PLAYER_POWERS = 0x68

CHILDREN_COLUMNS = ["food_raw", "food_boost", "food_boost_a", "children", "child_places", "adults", "adult_places",
                    "protection", "protection_boost", "mercy", "has_player", "alignment", "power", "creche",
                    "desire"]
SIGNIFICANCE_COLUMNS = ["desire", "boost", "boost_a", "trigger", "significance"]


def u(x: float) -> int:
    return f2u(x)


class Town:
    def __init__(self, o: Oracle):
        self.o = o
        self.town = o.alloc(0x800)
        self.vtable = o.alloc(0x100)
        self.player = o.alloc(0x100)
        self.alignment = o.alloc(0x20)
        self.creche = o.alloc(0x40)
        self.creche_vtable = o.alloc(0x100)
        self.info = o.alloc(0x40)
        o.write_u32(self.town, self.vtable)
        o.write_u32(self.vtable + 0x1C, GET_PLAYER_TARGET)
        o.write_u32(self.player + PLAYER_ALIGNMENT, self.alignment)
        o.write_u32(self.creche, self.creche_vtable)
        o.write_u32(self.creche_vtable + 0xD4, IS_FUNCTIONAL_TARGET)
        o.stub(DESIRE_INFO, eax=self.info, pop=4)

    def children_desire(self, c: dict) -> int:
        o, t = self.o, self.town
        o.write_f32(t + RAW, f32.of_bits(c["food_raw"]))
        o.write_f32(t + BOOST, f32.of_bits(c["food_boost"]))
        o.write_f32(t + BOOST_A, f32.of_bits(c["food_boost_a"]))
        o.write_f32(t + DESIRE + 3 * 4, f32.of_bits(c["protection"]))
        o.write_f32(t + BOOST + 3 * 4, f32.of_bits(c["protection_boost"]))
        o.write_f32(t + BOOST_A + 3 * 4, 0.0)
        o.write_f32(t + DESIRE + 4 * 4, f32.of_bits(c["mercy"]))
        o.write_f32(t + BOOST + 4 * 4, 0.0)
        o.write_f32(t + BOOST_A + 4 * 4, 0.0)
        o.write_u32(t + CHILDREN_COUNT, c["children"])
        o.write_u32(t + CHILD_PLACES, c["child_places"])
        o.write_u32(t + ADULTS, c["adults"])
        o.write_u32(t + ADULT_PLACES, c["adult_places"])
        o.write_f32(self.alignment + 8, f32.of_bits(c["alignment"]))
        o.write_f32(self.player + PLAYER_POWERS + 4 * 4, f32.of_bits(c["power"]))
        o.stub(GET_PLAYER_TARGET, eax=self.player if c["has_player"] else 0)
        o.write_u32(t + CRECHE, self.creche if c["creche"] != 0 else 0)
        o.stub(IS_FUNCTIONAL_TARGET, eax=1 if c["creche"] == 1 else 0)
        r = o.call(CHILDREN, this=t)
        assert r.depth == 1, r
        return r.st0f

    def significance(self, c: dict) -> int:
        o, d = self.o, 8
        base = self.town + 0x34
        o.write_f32(self.town + DESIRE + d * 4, f32.of_bits(c["desire"]))
        o.write_f32(self.town + BOOST + d * 4, f32.of_bits(c["boost"]))
        o.write_f32(self.town + BOOST_A + d * 4, f32.of_bits(c["boost_a"]))
        o.write_f32(self.info + 0x18, f32.of_bits(c["trigger"]))
        r = o.call(SIGNIFICANCE, this=base, args=[d])
        assert r.popped == 4 and r.depth == 1, r
        return r.st0f


def children_cases(rng: random.Random) -> list[dict]:
    cases = []
    foods = [-0.25, 0.0, 1e-7, 0.1, 0.3, 0.4327743, 0.5, 0.99999994, 1.0, 1.25]
    for food in foods:
        for room in [(0, 4, 10, 20), (4, 4, 30, 20), (3, 4, 20, 20), (0, 0, 0, 0)]:
            for creche in (0, 1, 2):
                for player in (0, 1):
                    cases.append({"food_raw": u(food), "food_boost": 0, "food_boost_a": 0, "children": room[0],
                                  "child_places": room[1], "adults": room[2], "adult_places": room[3],
                                  "protection": 0, "protection_boost": 0, "mercy": 0, "has_player": player,
                                  "alignment": u(0.0), "power": u(1.0), "creche": creche})
    for _ in range(3000):
        cases.append({
            "food_raw": u(rng.uniform(-0.3, 1.3)),
            "food_boost": u(rng.choice([0.0, 0.0, -0.2, rng.uniform(-1.0, 1.0)])),
            "food_boost_a": u(rng.choice([0.0, 0.0, 0.0, rng.uniform(-0.5, 0.5)])),
            "children": rng.randint(0, 40), "child_places": rng.randint(0, 40),
            "adults": rng.randint(0, 120), "adult_places": rng.randint(0, 120),
            "protection": u(rng.choice([0.0, rng.uniform(-0.5, 1.2)])),
            "protection_boost": u(rng.choice([0.0, 0.0, rng.uniform(-1.0, 1.0)])),
            "mercy": u(rng.choice([0.0, rng.uniform(-0.5, 1.2)])),
            "has_player": rng.randint(0, 1),
            "alignment": u(rng.uniform(-1.0, 1.0)),
            "power": u(rng.choice([1.0, rng.uniform(0.0, 2.0)])),
            "creche": rng.randint(0, 2),
        })
    return cases


def significance_cases(rng: random.Random) -> list[dict]:
    cases = []
    for desire in [0.0, 0.25, 0.4499999, 0.45, 0.45000002, 0.5, 0.71, 1.0]:
        for boost in [0.0, -0.2]:
            cases.append({"desire": u(desire), "boost": u(boost), "boost_a": 0, "trigger": u(0.25)})
    for _ in range(2000):
        cases.append({"desire": u(rng.uniform(-1.0, 1.0)), "boost": u(rng.choice([0.0, -0.2, rng.uniform(-1, 1)])),
                      "boost_a": u(rng.choice([0.0, rng.uniform(-1, 1)])),
                      "trigger": u(rng.choice([0.25, 0.01, 0.75, 0.0, rng.uniform(0, 1)]))})
    return cases


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261019)
    with Oracle() as o:
        town = Town(o)
        rows = []
        for c in children_cases(rng):
            result = town.children_desire(c)
            rows.append([c[k] for k in CHILDREN_COLUMNS[:-1]] + [result])
        write_pinned(out / "town_children_desire.txt", "A town's desire for children (the game's own function)",
                     ["Counts and flags are plain words; creche 0 none, 1 working, 2 not working.",
                      "Food, protection, mercy, alignment and power are float bits."],
                     CHILDREN_COLUMNS, rows)
        rows = []
        for c in significance_cases(rng):
            result = town.significance(c)
            rows.append([c[k] for k in SIGNIFICANCE_COLUMNS[:-1]] + [result])
        write_pinned(out / "town_desire_significance.txt",
                     "How far a town's desire is past the point where it makes villagers act",
                     ["All values are float bits; the desire is the town's desire for children."],
                     SIGNIFICANCE_COLUMNS, rows)
    print("done")


if __name__ == "__main__":
    main()
