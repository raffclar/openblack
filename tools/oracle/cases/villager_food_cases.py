#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how a town's people eat its store, in test/oracle/data:

- villager_food_desire.txt: how much a villager wants food, from how full it is
- villager_amount_to_eat.txt: how much food a meal is, from how full it is, its dinner and its town's food desire
- villager_eat_held.txt: a meal from what it carries: what it eats and how full it is after
- villager_digest.txt: the check on its belly: the food it burns since the last check, and the life hunger costs it
- town_food_desire.txt: how much a town wants food, from what it holds and the dinners of its people

Usage: python tools/oracle/cases/villager_food_cases.py [output folder]   (default: test/oracle/data)
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
DESIRE_FOR_FOOD = 0x75BB60  # stdcall (food), RET 4; the desire on ST0
AMOUNT_TO_EAT = 0x75BC20  # thiscall (villager), plain RET; the amount in EAX
EAT_FOOD_HELD = 0x75BF20  # thiscall (villager), plain RET; how full it is on ST0
CHECK_HUNGRY = 0x75BCC0  # thiscall (villager), plain RET; whether it set off to eat in EAX
TOWN_FOOD_DESIRE = 0x747F00  # thiscall (town), plain RET; the desire on ST0
# Functions these call that the cases replace with small code of their own
DROP_FOOD = 0x7511E0  # thiscall (villager, amount), RET 4: takes the food out of its hands
USE_FOOD = 0x73B5E0  # thiscall (town, amount), RET 4: counts the food eaten
GET_STORAGE_PIT = 0x73B5B0  # thiscall (town), plain RET
# The game object's pointer and the game turn in it
GAME = 0xD0195C
GAME_TURN = 0x205A40
# The state table's two "set off to eat" words of state 0 (left zero, so that a hungry check tries nothing)
STATE_EATS = 0xDB9F48

# Villager fields (from the disassembly)
INFO = 0x28
SPEED = 0x5A  # 16 bits
FOOD = 0xE8
LAST_CHECK = 0xEC
CARRIED = 0xF4  # 16 bits
# Villager info fields
BASE_SPEED = 0x104  # 32-bit int
NOURISHMENT = 0x2B8
FOOD_PER_TURN = 0x2BC
HUNGRY = 0x2C0
HUNGER_TO_LIFE = 0x2D0
DINNER = 0x2D8  # 32-bit int
# Virtual slots of the villager
GET_PLAYER = 0x1C
GET_TOWN = 0x48
GET_LIFE = 0x11C
IS_MOVING = 0x174
IS_POISONED = 0x4A4
REDUCE_LIFE = 0x5B8
GET_FINAL_STATE = 0xB04
# The player's power of its Japanese tribe
JAPANESE_POWER = 0x74
# Town fields
TOWN_FOOD_DESIRE_FIELD = 0x14C
TOWN_POT = 0x600
TOWN_DINNERS = 0x6F4
TOWN_CARRIED = 0x708
TOWN_INFO_FOOD_WANTED = 0xDC
GET_RESOURCE = 0x98  # slot of a store: (type), RET 4

NO_LIFE_LOST = 0xFFFFFFFF


def u(x: float) -> int:
    return f2u(x)


def code_return_cell(cell: int, pop: int = 0) -> bytes:
    """mov eax, [cell]; ret (pop)"""
    body = b"\xa1" + struct.pack("<I", cell)
    return body + (b"\xc3" if pop == 0 else b"\xc2" + struct.pack("<H", pop))


def code_return_float_cell(cell: int) -> bytes:
    """fld dword [cell]; ret"""
    return b"\xd9\x05" + struct.pack("<I", cell) + b"\xc3"


def code_capture_first(cell: int, pop: int, push_zero: bool) -> bytes:
    """mov eax, [esp + 4]; mov [cell], eax; (fldz;) ret pop"""
    body = b"\x8b\x44\x24\x04" + b"\xa3" + struct.pack("<I", cell)
    if push_zero:
        body += b"\xd9\xee"
    return body + b"\xc2" + struct.pack("<H", pop)


class Villager:
    def __init__(self, o: Oracle):
        self.o = o
        self.villager = o.alloc(0x200)
        self.vtable = o.alloc(0xC00)
        self.info = o.alloc(0x400)
        self.town = o.alloc(0x200)
        self.player = o.alloc(0x100)
        # Cells the small code reads and writes
        self.cells = o.alloc(0x40)
        self.town_cell, self.player_cell, self.moving_cell, self.poisoned_cell = (self.cells + 4 * i for i in range(4))
        self.life_cell, self.life_lost_cell, self.dropped_cell, self.state_cell = (self.cells + 16 + 4 * i
                                                                                   for i in range(4))
        self.turn_cell = self.cells + 0x30
        o.write_u32(self.villager, self.vtable)
        o.write_u32(self.villager + INFO, self.info)
        slots = {
            GET_TOWN: code_return_cell(self.town_cell),
            GET_PLAYER: code_return_cell(self.player_cell),
            IS_MOVING: code_return_cell(self.moving_cell),
            IS_POISONED: code_return_cell(self.poisoned_cell),
            GET_FINAL_STATE: code_return_cell(self.state_cell),
            GET_LIFE: code_return_float_cell(self.life_cell),
            REDUCE_LIFE: code_capture_first(self.life_lost_cell, 8, True),
        }
        for slot, code in slots.items():
            o.write_u32(self.vtable + slot, o.alloc(len(code) + 16, code))
        o.write(DROP_FOOD, code_capture_first(self.dropped_cell, 4, False))
        o.write(USE_FOOD, b"\xc2\x04\x00")
        o.write_u32(GAME, (self.turn_cell - GAME_TURN) & 0xFFFFFFFF)
        o.write_u32(STATE_EATS, 0, 0)
        o.write_f32(self.life_cell, 0.5)
        o.write_u32(self.state_cell, 0)

    def set_town(self, has_town: bool, desire_bits: int) -> None:
        self.o.write_u32(self.town_cell, self.town if has_town else 0)
        self.o.write_u32(self.town + TOWN_FOOD_DESIRE_FIELD, desire_bits)

    def amount_to_eat(self, c: dict) -> int:
        o, v = self.o, self.villager
        o.write_u32(v + FOOD, c["food"])
        o.write_u32(self.info + DINNER, c["dinner"])
        self.set_town(c["has_town"] != 0, c["town_desire"])
        r = o.call(AMOUNT_TO_EAT, this=v)
        assert r.popped == 0 and r.depth == 0, r
        return r.eax

    def eat_held(self, c: dict) -> tuple[int, int, int]:
        o, v = self.o, self.villager
        meal = self.amount_to_eat(c)
        o.write(v + CARRIED, struct.pack("<h", c["carried"]))
        o.write_u32(self.info + NOURISHMENT, c["nourishment"])
        o.write_u32(self.dropped_cell, 0)
        r = o.call(EAT_FOOD_HELD, this=v)
        assert r.popped == 0 and r.depth == 1, r
        food = o.read_u32(v + FOOD)[0]
        return meal, o.read_u32(self.dropped_cell)[0], food

    def digest(self, c: dict) -> tuple[int, int, int]:
        o, v, i = self.o, self.villager, self.info
        turn = 100000
        o.write_u32(self.turn_cell, turn)
        o.write_u32(v + LAST_CHECK, turn - c["since"])
        o.write(v + SPEED, struct.pack("<H", c["speed"]))
        o.write_u32(i + BASE_SPEED, c["base_speed"])
        o.write_u32(i + FOOD_PER_TURN, c["food_per_turn"])
        o.write_u32(v + FOOD, c["food"])
        o.write_u32(i + HUNGRY, c["hungry"])
        o.write_u32(i + HUNGER_TO_LIFE, c["hunger_to_life"])
        o.write_u32(self.player_cell, self.player if c["has_player"] else 0)
        o.write_u32(self.player + JAPANESE_POWER, c["power"])
        o.write_u32(self.moving_cell, c["moving"])
        o.write_u32(self.poisoned_cell, c["poisoned"])
        o.write_u32(self.life_lost_cell, NO_LIFE_LOST)
        r = o.call(CHECK_HUNGRY, this=v)
        assert r.popped == 0 and r.depth == 0, r
        assert o.read_u32(v + LAST_CHECK)[0] == turn or c["since"] == 0, "the check turn"
        return r.eax, o.read_u32(v + FOOD)[0], o.read_u32(self.life_lost_cell)[0]


class Town:
    def __init__(self, o: Oracle):
        self.o = o
        self.town = o.alloc(0x800)
        self.vtable = o.alloc(0x100)
        self.info = o.alloc(0x100)
        self.store = o.alloc(0x40)
        self.store_vtable = o.alloc(0x100)
        self.cells = o.alloc(0x20)
        self.pit_cell, self.food_cell, self.player_cell = self.cells, self.cells + 4, self.cells + 8
        o.write_u32(self.town, self.vtable)
        o.write_u32(self.town + INFO, self.info)
        o.write_u32(self.vtable + GET_PLAYER, o.alloc(32, code_return_cell(self.player_cell)))
        o.write_u32(self.player_cell, 0)
        o.write_u32(self.store, self.store_vtable)
        o.write_u32(self.store_vtable + GET_RESOURCE, o.alloc(32, code_return_cell(self.food_cell, 4)))
        o.write(GET_STORAGE_PIT, code_return_cell(self.pit_cell))

    def desire(self, c: dict) -> int:
        o, t = self.o, self.town
        o.write_u32(t + TOWN_CARRIED, c["carried"])
        o.write_u32(self.pit_cell, self.store if c["store"] == 1 else 0)
        o.write_u32(t + TOWN_POT, self.store if c["store"] == 2 else 0)
        o.write_u32(self.food_cell, c["store_food"])
        o.write_u32(self.info + TOWN_INFO_FOOD_WANTED, c["wanted"])
        o.write_u32(t + TOWN_DINNERS, c["dinners"])
        r = o.call(TOWN_FOOD_DESIRE, this=t)
        assert r.popped == 0 and r.depth == 1, r
        return r.st0f


FOOD_EDGES = [0.0, 1e-7, 0.1, 0.25, 0.4999999, 0.5, 0.50000006, 0.75, 0.99999994, 1.0, 1.0000001, 1.1, -0.0]
NOURISHMENTS = [1.0, 1.1, 1.2, 1.3, 1.44, 1.56]


def desire_cases(rng: random.Random) -> list[int]:
    return [u(x) for x in FOOD_EDGES] + [u(rng.uniform(0.0, 1.2)) for _ in range(3000)]


def meal_cases(rng: random.Random) -> list[dict]:
    cases = []
    for food in FOOD_EDGES:
        for dinner in (30, 35, 90, 95):
            for town in (None, 0.0, -0.5, 0.25, 0.5, 1.0, 1.5):
                cases.append({"food": u(food), "dinner": dinner, "has_town": int(town is not None),
                              "town_desire": u(town or 0.0)})
    for _ in range(4000):
        town = rng.choice([None, 0.0, rng.uniform(-0.2, 1.2), rng.uniform(0.0, 0.1)])
        cases.append({"food": u(rng.uniform(0.0, 1.1)), "dinner": rng.choice([30, 35, 55, 75, 85, 90, 95,
                                                                                rng.randint(0, 200)]),
                      "has_town": int(town is not None), "town_desire": u(town or 0.0)})
    return cases


def eat_cases(rng: random.Random) -> list[dict]:
    cases = []
    for c in meal_cases(rng):
        c = dict(c)
        c["carried"] = rng.choice([0, 1, 30, 82, 83, 95, 150, rng.randint(0, 300)])
        c["nourishment"] = u(rng.choice(NOURISHMENTS))
        cases.append(c)
    return cases


def digest_cases(rng: random.Random) -> list[dict]:
    cases = []
    rate = 9e-5
    for since in (1, 8, 9, 10, 17, 100, 5000):
        for food in (0.0, 0.25, 0.5, 0.50081, 0.75, 1.0, 1.1):
            for speed, base, moving in ((0, 300, 0), (300, 300, 1), (450, 300, 1), (450, 300, 0), (150, 300, 1)):
                for player, power in ((0, 1.0), (1, 1.0), (1, 1.5)):
                    for poisoned in (0, 1):
                        cases.append({"since": since, "food": u(food), "food_per_turn": u(rate), "speed": speed,
                                      "base_speed": base, "has_player": player, "power": u(power), "moving": moving,
                                      "hungry": u(0.5), "hunger_to_life": u(0.001), "poisoned": poisoned})
    for _ in range(4000):
        base = rng.choice([300, 250, 400, rng.randint(1, 1000)])
        cases.append({
            "since": rng.choice([9, 10, rng.randint(1, 50), rng.randint(1, 20000)]),
            "food": u(rng.uniform(0.0, 1.1)),
            "food_per_turn": u(rng.choice([rate, rng.uniform(1e-6, 1e-3)])),
            "speed": rng.choice([0, base, rng.randint(0, 3 * base)]) & 0xFFFF,
            "base_speed": base,
            "has_player": rng.randint(0, 1),
            "power": u(rng.choice([1.0, rng.uniform(1.0, 3.0)])),
            "moving": rng.randint(0, 1),
            "hungry": u(rng.choice([0.5, rng.uniform(0.1, 0.9)])),
            "hunger_to_life": u(rng.choice([0.001, rng.uniform(0.0, 0.01)])),
            "poisoned": rng.choice([0, 0, 0, 1]),
        })
    return cases


def town_cases(rng: random.Random) -> list[dict]:
    cases = []
    for carried in (0.0, 0.5, 81.0, 2300.75):
        for store, food in ((0, 0), (1, 0), (1, 1000), (1, 19834), (1, 20000), (1, 30000), (2, 300)):
            for wanted in (5.0, 0.0, 2.5):
                for dinners in (0.0, 850.0, 4070.0, 4450.0):
                    cases.append({"carried": u(carried), "store": store, "store_food": food, "wanted": u(wanted),
                                  "dinners": u(dinners)})
    for _ in range(4000):
        cases.append({"carried": u(rng.choice([0.0, rng.uniform(0.0, 5000.0)])), "store": rng.choice([0, 1, 1, 2]),
                      "store_food": rng.randint(0, 40000), "wanted": u(rng.choice([5.0, rng.uniform(0.0, 10.0)])),
                      "dinners": u(rng.choice([4070.0, float(rng.randint(0, 9000))]))})
    return cases


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261019)
    with Oracle() as o:
        rows = []
        for food in desire_cases(rng):
            r = o.call(DESIRE_FOR_FOOD, args=[f32.of_bits(food)])
            assert r.popped == 4 and r.depth == 1, r
            rows.append([food, r.st0f])
        write_pinned(out / "villager_food_desire.txt", "How much a villager wants food (the game's own function)",
                     ["All values are float bits."], ["food", "desire"], rows)

        villager = Villager(o)
        columns = ["food", "dinner", "has_town", "town_desire", "amount"]
        rows = [[c[k] for k in columns[:-1]] + [villager.amount_to_eat(c)] for c in meal_cases(rng)]
        write_pinned(out / "villager_amount_to_eat.txt", "How much food a villager's meal is (the game's own function)",
                     ["Food and the town's food desire are float bits; the dinner and the amount are plain words."],
                     columns, rows)

        columns = ["food", "dinner", "has_town", "town_desire", "carried", "nourishment", "meal", "eaten", "food_after"]
        rows = []
        for c in eat_cases(rng):
            meal, eaten, food = villager.eat_held(c)
            rows.append([c[k] for k in columns[:6]] + [meal, eaten, food])
        write_pinned(out / "villager_eat_held.txt", "A villager eats the food it carries (the game's own function)",
                     ["Food, the town's desire and the nourishment are float bits; the rest are plain words.",
                      "The meal is the amount the game asks for first; eaten is what it takes out of its hands."],
                     columns, rows)

        columns = ["since", "food", "food_per_turn", "speed", "base_speed", "has_player", "power", "moving", "hungry",
                   "hunger_to_life", "poisoned", "went", "food_after", "life_lost"]
        rows = []
        for c in digest_cases(rng):
            went, food, lost = villager.digest(c)
            rows.append([c[k] for k in columns[:11]] + [went, food, lost])
        write_pinned(out / "villager_digest.txt", "The check on a villager's belly (the game's own function)",
                     ["Food, rates, the power, the hungry mark and the life lost are float bits; the rest are words.",
                      f"life_lost {NO_LIFE_LOST:08x} means the check took no life. The state table is empty, so a",
                      "hungry villager never sets off to eat here (went 0)."],
                     columns, rows)

        town = Town(o)
        columns = ["carried", "store", "store_food", "wanted", "dinners", "desire"]
        rows = [[c[k] for k in columns[:-1]] + [town.desire(c)] for c in town_cases(rng)]
        write_pinned(out / "town_food_desire.txt", "How much a town wants food (the game's own function)",
                     ["carried, wanted and dinners are float bits; store 0 none, 1 storage pit, 2 temporary pot."],
                     columns, rows)
    print("done")


if __name__ == "__main__":
    main()
