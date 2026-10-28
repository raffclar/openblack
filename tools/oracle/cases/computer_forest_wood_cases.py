#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the tree that a computer god takes from the forests and the lone trees that it saw, for a
town that needs wood: test/oracle/data/computer_forest_wood.txt.

Each row is one call of the game's rule. The god saw one to three things, the newest first. Each thing is a forest or
a lone tree. A forest has one grown tree, which is its centre tree, and a count of grown trees. The inputs are:

- the map position of the hand in the action (x, z), the hand point of the interface (x, z bits), the influence power
  of the god (bits) and the pull of each place (bits, the same for all things of a row);
- for each thing: its kind (1 forest, 2 lone tree, 0 for no thing), the count of grown trees of a forest, the wood (the
  bits of the wood of the centre tree for a forest, the wood value of the kind for a lone tree), the bits of the radius
  of the tree, the map position of the tree (x, z) and 1 when a script controls the tree.

The results are the bits of the value and the index of the chosen thing (ffffffff for none).

The game's rule runs as it is, with the game's own distance (with its square root table made
as at start-up), sigmoid and coordinate code. Stubs give the pull of a place,
the interface of the player and the land height. The type check of each thing reads a tag in the thing. Small
functions in the arena give the wood and the radius of each tree from fields of the tree.

Usage: python tools/oracle/cases/computer_forest_wood_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
FOREST_WOOD = 0x666CF0  # thiscall (computer player, node), ret 4
DYNAMIC_CAST = 0x7C5A19  # cdecl (object, delta, from type, to type, reference)
PLACE_PULL = 0x658630  # thiscall (computer player, coords), ret 4, ST0
LEADER_INTERFACE = 0x64A9F0  # thiscall (player), EAX
LAND_HEIGHT = 0x803090  # thiscall (coords), ST0
GET_PLAYER = 0x55E3B0  # the game's own getter of the player of a computer player
DISTANCE_FROM_OBJECT = 0x4027C0  # the game's own distance from a thing to a place, less its radius
GET_RADIUS = 0x638110  # the game's own radius, through the slot of the 2D radius
FOREST_TYPE = 0xBDEA58
TREE_TYPE = 0x9D96E0
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table the distance reads, as the game does at start-up
INVERSE_SQRT_TABLE = 0xDA5A10  # 1024 dwords

# Offsets
PLAYER = 0x28  # computer player: its player
ACTION = 0x34  # computer player: its hand action
SEEN = 0x40  # computer player: the head of its seen list
INFLUENCE_POWER = 0x8C  # player
ACTION_HAND = 0x14  # action: map position of the hand
INTERFACE_HAND = 0xC8  # interface: hand point
TAG = 0x08  # thing: the type tag that the patched type check reads
POSITION = 0x14
FLAGS = 0x24
INFO = 0x28
INFO_WOOD = 0x6C
GROWN_LIST = 0x48
GROWN_COUNT = 0x4C
GROWING_LIST = 0x50
TREE_WOOD = 0x80  # tree: the wood that the arena function gives
TREE_RADIUS = 0x84  # tree: the radius that the arena function gives
CONTROLLED_BY_SCRIPT = 0x400
NODE_OBJECT = 0x94

# The type check: the object when its tag is the type asked for, else 0
CAST_CODE = bytes.fromhex("8B442404" "8B4808" "3B4C2410" "7402" "33C0" "C3")


def load_field(offset: int) -> bytes:
    """fld dword ptr [ecx + offset]; ret"""
    return bytes.fromhex("D981") + struct.pack("<I", offset) + bytes.fromhex("C3")


def to_fixed(metres: float) -> int:
    product = u2f(f32(u2f(f32(metres).bits) * u2f(f32(6553.6).bits)).bits)
    return int(product)


def random_thing(pick: random.Random, centre: tuple[int, int]) -> tuple:
    kind = pick.choice([1, 1, 2, 2, 2])
    count = pick.choice([0, 1, 2, 5, 17, pick.randrange(0, 60)]) if kind == 1 else 0
    if kind == 1:
        wood = pick.choice([0.0, 1.0, pick.uniform(0.0, 400.0), pick.uniform(0.0, 4000.0)])
        wood_bits = f32(wood).bits
    else:
        wood_bits = pick.choice([0, 1, 100, 250, 500, pick.randrange(0, 3000)])
    radius = pick.choice([0.0, 0.5, pick.uniform(0.0, 8.0)])
    spread = pick.choice([5.0, 40.0, 200.0, 800.0])
    x = centre[0] + to_fixed(pick.uniform(-spread, spread))
    z = centre[1] + to_fixed(pick.uniform(-spread, spread))
    scripted = 1 if pick.random() < 0.08 else 0
    return kind, count, wood_bits, f32(radius).bits, x & 0xFFFFFFFF, z & 0xFFFFFFFF, scripted


def forest_wood(o: Oracle, pick: random.Random) -> list[list[int]]:
    cases = []
    centre = (to_fixed(1285.0), to_fixed(1285.0))
    # Ties and edges first: equal things, an empty forest, a scripted tree, no power
    same = (2, 0, 100, f32(1.0).bits, centre[0] + to_fixed(20.0), centre[1], 0)
    cases.append((centre, (1285.0, 1285.0), 300.0, 1.0, [same, same]))
    cases.append((centre, (1285.0, 1285.0), 300.0, 1.0, [(1, 0, f32(50.0).bits, 0, centre[0], centre[1], 0), same]))
    cases.append((centre, (1285.0, 1285.0), 300.0, 1.0, [(2, 0, 400, 0, centre[0], centre[1], 1), same]))
    cases.append((centre, (1285.0, 1285.0), 0.0, 1.0, [same]))
    cases.append((centre, (0.0, 0.0), 300.0, 0.5, [same, (1, 3, f32(80.0).bits, 0, centre[0], centre[1], 0)]))
    cases.append((centre, (1285.0, 1285.0), 300.0, 1.0, []))
    for _ in range(600):
        hand = (centre[0] + to_fixed(pick.uniform(-300.0, 300.0)), centre[1] + to_fixed(pick.uniform(-300.0, 300.0)))
        point = pick.choice([(0.0, 0.0), (hand[0] / 6553.6, hand[1] / 6553.6),
                             (pick.uniform(900.0, 1700.0), pick.uniform(900.0, 1700.0))])
        power = pick.choice([0.0, 1.0, 150.0, pick.uniform(0.0, 2000.0)])
        pull = pick.choice([0.0, 1.0, pick.uniform(0.0, 1.0)])
        things = [random_thing(pick, hand) for _ in range(pick.randrange(1, 4))]
        cases.append((hand, point, power, pull, things))

    rows = []
    for hand, point, power, pull, things in cases:
        o.reset()
        o.call(MAKE_INVERSE_SQRT_TABLE)
        assert any(o.read_u32(INVERSE_SQRT_TABLE, 1024))
        o.write(DYNAMIC_CAST, CAST_CODE)
        wood_code = o.alloc(16, load_field(TREE_WOOD))
        radius_code = o.alloc(16, load_field(TREE_RADIUS))
        tree_table = o.alloc(0x870)
        o.write_u32(tree_table + 0x664, wood_code)
        o.write_u32(tree_table + 0x13C, DISTANCE_FROM_OBJECT)
        o.write_u32(tree_table + 0x60, GET_RADIUS)
        o.write_u32(tree_table + 0x64, radius_code)
        interface = o.object(0x200)
        o.write_f32(interface + INTERFACE_HAND, f32(point[0]), f32(0.0), f32(point[1]))
        player = o.object(0xB00, {INFLUENCE_POWER: f32(power)})
        action = o.object(0x300)
        o.write_u32(action + ACTION_HAND, hand[0] & 0xFFFFFFFF, hand[1] & 0xFFFFFFFF, 0)
        computer_table = o.object(0x40, {0x1C: GET_PLAYER})
        trees = []
        head = 0
        links = []
        for kind, count, wood_bits, radius_bits, x, z, scripted in things:
            info = o.object(0x100, {INFO_WOOD: wood_bits if kind == 2 else 0})
            tree = o.object(0x100, {0: tree_table, TAG: TREE_TYPE, INFO: info,
                                    FLAGS: CONTROLLED_BY_SCRIPT if scripted else 0,
                                    TREE_WOOD: wood_bits if kind == 1 else 0, TREE_RADIUS: radius_bits})
            o.write_u32(tree + POSITION, x, z, 0)
            trees.append(tree)
            if kind == 1:
                tree_link = o.object(8, {0: tree})
                thing = o.object(0x100, {TAG: FOREST_TYPE, GROWN_LIST: tree_link, GROWN_COUNT: count, GROWING_LIST: 0})
                o.write_u32(thing + POSITION, x, z, 0)
            else:
                thing = tree
            seen = o.object(0x20, {POSITION: thing})
            links.append(o.object(8, {4: seen}))
        for link, after in zip(links, links[1:]):
            o.write_u32(link, after)
        head = links[0] if links else 0
        computer = o.object(0x200, {0: computer_table, PLAYER: player, ACTION: action, SEEN: head})
        node = o.object(0xB0)
        o.stub(PLACE_PULL, pop=4, st0=f32(pull))
        o.stub(LEADER_INTERFACE, eax=interface)
        o.stub(LAND_HEIGHT, st0=0.0)
        r = o.call(FOREST_WOOD, this=computer, args=[node])
        chosen = o.read_u32(node + NODE_OBJECT)[0]
        index = next((i for i, tree in enumerate(trees) if tree == chosen), 0xFFFFFFFF)
        row = [hand[0] & 0xFFFFFFFF, hand[1] & 0xFFFFFFFF, f32(point[0]).bits, f32(point[1]).bits, f32(power).bits,
               f32(pull).bits]
        for i in range(3):
            row += list(things[i]) if i < len(things) else [0] * 7
        rows.append(row + [r.st0f, index])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x0B00_F0E5)
    with Oracle() as o:
        rows = forest_wood(o, pick)
    columns = ["handX", "handZ", "pointX", "pointZ", "power", "pull"]
    for i in range(3):
        columns += [f"kind{i}", f"count{i}", f"wood{i}", f"radius{i}", f"x{i}", f"z{i}", f"script{i}"]
    columns += ["value", "index"]
    count = write_pinned(
        out / "computer_forest_wood.txt",
        "The tree that a computer god takes from the forests and lone trees that it saw, for a town that needs wood",
        ["handX and handZ are the map position of the hand in the action; pointX and pointZ are the bits of the hand",
         "point of the interface; power is the bits of the influence power; pull is the bits of the pull of each place.",
         "For each thing N, the newest first: kindN is 1 for a forest, 2 for a lone tree, 0 for none; countN is the",
         "count of grown trees of a forest; woodN is the bits of the wood of the centre tree of a forest, or the wood",
         "value of the kind of a lone tree; radiusN is the bits of the radius of the tree; xN and zN are the map",
         "position of the tree; scriptN is 1 when a script controls the tree. value is the bits of the result; index",
         "is the chosen thing (ffffffff for none)."],
        columns,
        rows,
    )
    print(f"computer_forest_wood: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
