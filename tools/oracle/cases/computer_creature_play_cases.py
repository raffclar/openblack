#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the play of a computer god with its creature, in test/oracle/data:

- computer_creature_throw_food.txt: where the hand goes before it throws food at the creature, and where it aims the
  food. The god's whole perform runs; the calls that find the food and the creature, the pick-up, the land height and
  the random numbers are stubs. The steps come back from the action's list of steps.
- computer_creature_cast_wish.txt: the wish of the god to keep the leash tied at the start of a cast, from the times
  that the creature saw the miracle, the times that it must see it, and its freedom (both stubs).
- computer_creature_cast_let_go.txt: whether the god lets go of the tied leash at the start of a cast, from the freedom
  and the wish (stubs), the map positions of the hand and the creature, and the size of the creature. The distance reads
  the game's inverse square root table, which the game's own start-up function fills first.

Usage: python tools/oracle/cases/computer_creature_play_cases.py [output folder]   (default: test/oracle/data)
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
THROW_FOOD = 0x65AA20  # thiscall on the computer player, arg (node), RET 4: 1 when the steps are added
LEAF_OBJECT = 0x655CF0  # thiscall on the node, RET
NODE_OBJECT = 0x655CB0  # thiscall on the node, RET
DYNAMIC_CAST = 0x7C5A19  # cdecl, 5 arguments
FAILURE_TEXT = 0x658230  # thiscall, RET 8: debug text only
PICK_UP = 0x658AA0  # thiscall, RET 4: adds the pick-up step, 1 when it can
LAND_HEIGHT = 0x803090  # thiscall on the map position, RET: the land height in ST0
FLOAT_RAND = 0x6DE530  # cdecl: the random number in ST0
ALLOCATORS = (0x4366F0, 0x7DB790, 0x7C64EE)
FREE = 0x7AEE98  # cdecl (pointer)
WISH = 0x6588F0  # thiscall on the computer player, arg (magic type), RET 4: the wish in ST0
LET_GO = 0x6669D0  # thiscall on the computer player, arg (magic type), RET 4
FREEDOM = 0x658BC0  # thiscall on the computer player, RET: the freedom in ST0
CREATURE_OF = 0x65BCB0  # thiscall on the computer player, RET: the creature
TIMES_NEEDED = 0x4F8CA0  # thiscall on the creature, arg (magic type), RET 4: in ST0
INTERFACE_STATUS = 0x64A9F0  # thiscall on the player, RET: the status of the player
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table that the distance reads, as the game does at start-up
GETTER = 0x5DC8B0  # a function that the oracle makes a stub of, for the vtable calls that give the player

# The computer player and its action state
ACTION_STATE = 0x34
STEPS = 0x28  # the list of steps: the first node (next, step)
HAND = 0x50
HAND_COORDS = 0x14
# A step: its kind and the list of its arguments (next, argument); an argument keeps its place at +0x68
STEP_KIND = 0x14
STEP_ARGUMENTS = 0x18
ARGUMENT_PLACE = 0x68
# A thing: its map position at +0x14, the body (+0x160) with the size at +0x6C
COORDS = 0x14
BODY = 0x160
SIZE = 0x6C
# The player keeps its creature at +0xA4C; the creature's mind (+0x164) counts the sightings of each miracle
PLAYER_CREATURE = 0xA4C
MIND = 0x164
SIGHTINGS = 0x17D38
# The leash: the status of the player (+0x12C) holds what the leash is tied to (+0x24)
STATUS_LEASH = 0x12C
TIED_TO = 0x24


def f(rng: random.Random, low: float, high: float) -> int:
    return f2u(rng.uniform(low, high))


class ThrowFood:
    COLUMNS = ["hand_x", "hand_y", "hand_z", "coords_x", "coords_z", "altitude", "land", "size", "draw", "go_x", "go_y",
               "go_z", "aim_x", "aim_y", "aim_z"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        for allocator in ALLOCATORS:
            o.hook_alloc(allocator)
        o.hook_free(FREE)
        o.stub(FAILURE_TEXT, pop=8)
        o.stub(PICK_UP, eax=1, pop=4)
        self.body = o.alloc(0x100)
        self.thing = o.object(0x200, {BODY: self.body})
        o.stub(LEAF_OBJECT, eax=self.thing, pop=0)
        o.stub(NODE_OBJECT, eax=self.thing, pop=0)
        o.stub(DYNAMIC_CAST, eax=self.thing, pop=0)
        self.node = o.alloc(0x100)

    def steps(self, state: int) -> list[tuple[int, list[int]]]:
        o = self.o
        found = []
        node = o.read_u32(state + STEPS)[0]
        while node:
            following, step = o.read_u32(node, 2)
            kind = o.read_u32(step + STEP_KIND)[0]
            argument = o.read_u32(o.read_u32(step + STEP_ARGUMENTS)[0] + 4)[0]
            found.append((kind, o.read_u32(argument + ARGUMENT_PLACE, 3)))
            node = following
        return found

    def run(self, hx: int, hy: int, hz: int, x: int, z: int, altitude: int, land: int, size: int,
            draw: int) -> list[int]:
        o = self.o
        state = o.alloc(0x100)
        o.write_u32(state + HAND, hx, hy, hz)
        player = o.object(0x100, {ACTION_STATE: state})
        o.write_u32(self.thing + COORDS, x, z, altitude)
        o.write_u32(self.body + SIZE, size)
        o.stub(LAND_HEIGHT, pop=0, st0=f32.of_bits(land))
        o.stub(FLOAT_RAND, pop=0, st0=f32.of_bits(draw))
        result = o.call(THROW_FOOD, this=player, args=[self.node])
        assert result.eax & 0xFF == 1 and result.popped == 4, result
        steps = self.steps(state)
        assert [kind for kind, _ in steps] == [1, 7], steps
        return [*steps[0][1], *steps[1][1]]


def throw_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(2000):
        cx = rng.randrange(0x10000, 0x7FF0000)
        cz = rng.randrange(0x10000, 0x7FF0000)
        mx = cx * 10.0 / 65536.0
        mz = cz * 10.0 / 65536.0
        reach = rng.choice([5.0, 19.0, 20.0, 21.0, 40.0, 300.0])
        hx = f2u(mx + rng.uniform(-reach, reach))
        hz = f2u(mz + rng.uniform(-reach, reach))
        hy = f(rng, 0.0, 120.0)
        if rng.random() < 0.02:
            # The hand at the place of the creature
            hx, hz = f2u(cx * 10.0 / 65536.0), f2u(cz * 10.0 / 65536.0)
        rows.append((hx, hy, hz, cx, cz, f(rng, -2.0, 2.0), f(rng, 0.0, 80.0), f(rng, 0.3, 2.5), f(rng, 0.0, 4.0)))
    return rows


class CastWish:
    COLUMNS = ["type", "seen", "needed", "freedom", "wish"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.mind = o.alloc(SIGHTINGS + 0x200)
        self.creature = o.object(0x200, {MIND: self.mind})
        self.owner = o.object(0x1000, {PLAYER_CREATURE: self.creature})
        o.stub(GETTER, eax=self.owner, pop=0)
        vtable = o.alloc(0x100)
        o.write_u32(vtable + 0x1C, GETTER)
        self.player = o.object(0x100, {0: vtable})

    def run(self, magic: int, seen: int, needed: int, freedom: int) -> list[int]:
        o = self.o
        o.write_u32(self.mind + SIGHTINGS + 4 * magic, seen)
        o.stub(TIMES_NEEDED, pop=4, st0=f32.of_bits(needed))
        o.stub(FREEDOM, pop=0, st0=f32.of_bits(freedom))
        result = o.call(WISH, this=self.player, args=[magic])
        assert result.popped == 4 and result.depth == 1, result
        return [result.st0f]


def wish_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(2000):
        needed = rng.choice([0.0, rng.uniform(0.5, 20.0), float(rng.randrange(1, 12)), rng.uniform(0.0, 1.0)])
        seen = rng.choice([0, rng.randrange(0, 30), int(needed), int(needed) + 1])
        freedom = rng.choice([0.0, 1.0, rng.uniform(0.0, 1.0)])
        rows.append((rng.randrange(1, 42), seen, f2u(needed), f2u(freedom)))
    return rows


class CastLetGo:
    COLUMNS = ["freedom", "wish", "hand_x", "hand_z", "coords_x", "coords_z", "size", "let_go"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.body = o.alloc(0x100)
        vtable = o.alloc(0x100)
        o.write_u32(vtable + 0x1C, GETTER)
        o.stub(GETTER, eax=0, pop=0)
        self.creature = o.object(0x200, {0: vtable, BODY: self.body})
        o.stub(CREATURE_OF, eax=self.creature, pop=0)
        self.leash = o.alloc(0x100)
        self.status = o.object(0x200, {STATUS_LEASH: self.leash})
        o.stub(INTERFACE_STATUS, eax=self.status, pop=0)
        self.state = o.alloc(0x100)
        self.player = o.object(0x100, {ACTION_STATE: self.state})
        o.call(MAKE_INVERSE_SQRT_TABLE)

    def run(self, freedom: int, wish: int, hx: int, hz: int, x: int, z: int, size: int) -> list[int]:
        o = self.o
        o.stub(WISH, pop=4, st0=f32.of_bits(wish))
        o.stub(FREEDOM, pop=0, st0=f32.of_bits(freedom))
        o.write_u32(self.state + HAND_COORDS, hx, hz, 0)
        o.write_u32(self.creature + COORDS, x, z, 0)
        o.write_u32(self.body + SIZE, size)
        o.write_u32(self.leash + TIED_TO, 0x12345678)
        result = o.call(LET_GO, this=self.player, args=[1])
        assert result.popped == 4 and result.depth == 0, result
        return [1 if o.read_u32(self.leash + TIED_TO)[0] == 0 else 0]


def let_go_cases(rng: random.Random) -> list[tuple[int, ...]]:
    nan = 0x7FC00000
    rows = []
    for _ in range(2000):
        freedom = rng.choice([0.0, 1.0, rng.uniform(0.0, 1.0)])
        wish = rng.choice([nan, nan, f2u(freedom), f2u(freedom * rng.uniform(0.0, 1.0)), f2u(rng.uniform(0.0, 2.0))])
        x = rng.randrange(0x10000, 0x7FF0000)
        z = rng.randrange(0x10000, 0x7FF0000)
        reach = rng.choice([100, 300, 400, 1000]) * 6554
        hx = min(max(x + rng.randrange(-reach, reach), 0), 0x7FFFFFF)
        hz = min(max(z + rng.randrange(-reach, reach), 0), 0x7FFFFFF)
        rows.append((f2u(freedom), wish, hx, hz, x, z, f(rng, 0.3, 2.5)))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x65AA20)
    with Oracle() as oracle:
        throw = ThrowFood(oracle)
        rows = [[*case, *throw.run(*case)] for case in throw_cases(rng)]
        n = write_pinned(out / "computer_creature_throw_food.txt", "Where a computer god throws food at its creature",
                         ["Function: thiscall on the computer player, arg (node), RET 4. The food and the creature are",
                          "one stub thing; the land height and both random numbers are stubs. go_* is the place of",
                          "the move step (kind 1), aim_* the place of the throw step (kind 7). Floats are bits;",
                          "coords are 16.16 map units."],
                         ThrowFood.COLUMNS, rows)
        print(f"computer_creature_throw_food: {n} rows")
    with Oracle() as oracle:
        wish = CastWish(oracle)
        rows = [[*case, *wish.run(*case)] for case in wish_cases(rng)]
        n = write_pinned(out / "computer_creature_cast_wish.txt", "The wish to keep the leash tied at a cast start",
                         ["Function: thiscall on the computer player, arg (magic type), RET 4; the wish in ST0. The",
                          "times needed and the freedom are stubs; seen is the creature's count of sightings."],
                         CastWish.COLUMNS, rows)
        print(f"computer_creature_cast_wish: {n} rows")
    with Oracle() as oracle:
        let_go = CastLetGo(oracle)
        rows = [[*case, *let_go.run(*case)] for case in let_go_cases(rng)]
        n = write_pinned(out / "computer_creature_cast_let_go.txt", "The let-go of a tied leash at a cast start",
                         ["Function: thiscall on the computer player, arg (magic type), RET 4. The wish and the",
                          "freedom are stubs. let_go is 1 when the god cleared what the leash is tied to."],
                         CastLetGo.COLUMNS, rows)
        print(f"computer_creature_cast_let_go: {n} rows")


if __name__ == "__main__":
    main()
