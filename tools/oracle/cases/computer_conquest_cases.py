#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how a computer god chooses a town to win, in test/oracle/data:

- computer_conquest_town_worth.txt: the town of another player that the god wants most, and how much, with the
  game's own belief, hold, distance and sigmoid code
- computer_conquest_feed_factor.txt: how much the god wants to meet the needs of a town, from the strongest belief of
  a rival that is not an ally
- computer_conquest_creature_help.txt: how much the god wants its creature to go to a thing, from the leash, the
  distance, the size, the rest and a stubbed freedom

Usage: python tools/oracle/cases/computer_conquest_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
TOWN_WORTH = 0x668E20  # thiscall (computer player, other player, out town), ret 8, ST0
FEED_FACTOR = 0x741920  # thiscall (town, player), ret 4, ST0
CREATURE_HELP = 0x65D1E0  # thiscall (computer player, target, same town byte), ret 8, ST0
ICON_TYPE = 0x726380  # thiscall (icon): the magic type; patched to read +0x80 of the icon
HAND_POS = 0x657FE0  # thiscall (computer player, out coords), ret 4
NEXT_PLAYER = 0x5508D0  # thiscall (world, previous player), ret 4: the next player or 0
IS_ALLIED = 0x64D5D0  # thiscall (player, other), ret 4: AL not 0 for an ally
CREATURE_OF = 0x65BCB0  # thiscall (computer player): the creature or 0
LEASH_STATUS = 0x4CF060  # thiscall (creature): the status of the leash or 0
FREEDOM = 0x658BC0  # thiscall (computer player), ST0
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table that the distance reads
WORLD = 0xD0195C
WORLD_PLAYER_COUNT = 0x205A5B  # byte: the number of the neutral player slot
WORLD_PLAYERS = 0x18
PLAYER_SIZE = 0xA60

# Offsets
CP_ACTION = 0x34
CP_REQUESTED = 0x68
ACTION_HAND = 0x14
P_NUMBER = 0xB5
P_POWER = 0x8C
P_ALL_ENABLED = 0x92C
P_PEOPLE = 0x948
P_CITADEL = 0xA48
P_TOWNS = 0xA50
P_ENABLED = 0x970
P_NEXT = 0xA5C  # free: the next player of the patched list
P_ALLY = 0xA58  # free: the ally byte of the patched ally check
T_POS = 0x14
T_OWNER = 0x5BC
T_ADULTS = 0x618
T_CHILDREN = 0x61C
T_NEXT = 0x75C
T_ICONS = 0x778
T_BELIEF = 0x7A0
I_TYPE = 0x80
I_NEXT = 0x110
OBJ_TOWN = 0x7F0  # free: the town that the patched slot gives
C_PHYSICAL = 0x160
C_SIZE = 0x6C
C_EXHAUSTION = 0x30
STATUS_TIE = 0x12C
TIE_TO = 0x24


def ret_eax(value: int) -> bytes:
    """mov eax, value; ret"""
    return b"\xB8" + struct.pack("<I", value & 0xFFFFFFFF) + b"\xC3"


def read_field(offset: int) -> bytes:
    """mov eax, [ecx + offset]; ret"""
    return b"\x8B\x81" + struct.pack("<I", offset) + b"\xC3"


def to_fixed(metres: float) -> int:
    return int(u2f(f32(u2f(f32(metres).bits) * u2f(f32(6553.6).bits)).bits))


def belief(rng: random.Random) -> float:
    return rng.choice([0.0, 0.0, 1.0, 5.0, rng.uniform(0.0, 1.0), rng.uniform(0.0, 10.0), rng.uniform(0.0, 200.0)])


def coord(rng: random.Random) -> int:
    return to_fixed(rng.choice([rng.uniform(500.0, 2000.0), rng.uniform(1000.0, 1100.0), 1000.0]))


def setup_world(o: Oracle) -> int:
    o.call(MAKE_INVERSE_SQRT_TABLE)
    world = o.alloc(WORLD_PLAYER_COUNT + 0x10)
    o.write_u32(WORLD, world)
    o.write(world + WORLD_PLAYER_COUNT, bytes([0]))
    o.write(ICON_TYPE, read_field(I_TYPE))
    return world


# --- The worth of the towns of another player ---

TOWN_FIELDS = ["adults", "children", "godowns", "x", "z", *[f"b{i}" for i in range(8)], "icons", "enabled",
               "requested"]


def town_worth_cases(rng: random.Random) -> list[tuple]:
    cases = []
    for _ in range(2500):
        neutral = 1 if rng.random() < 0.25 else 0
        god = rng.randrange(0, 7)
        owner = 7 if neutral else rng.choice([n for n in range(7) if n != god])
        power = lambda: rng.choice([0.0, 1.0, 100.0, rng.uniform(0.0, 3000.0)])  # noqa: E731
        people = rng.choice([0, 1, 10, rng.randrange(0, 400)])
        hand = (coord(rng), coord(rng))
        towns = []
        for _t in range(rng.choice([0, 1, 1, 2, 3, 3])):
            icons = rng.randrange(0, 4)
            towns.append((rng.choice([0, 3, rng.randrange(0, 200)]), rng.choice([0, rng.randrange(0, 100)]),
                          1 if rng.random() < 0.1 else 0, coord(rng), coord(rng),
                          *[f2u(belief(rng)) for _ in range(8)], icons, rng.randrange(0, 8), rng.randrange(0, 8)))
        cases.append((neutral, f2u(power()), f2u(power()), people, god, owner, *hand, towns))
    return cases


def town_worth(o: Oracle, case: tuple) -> list[int]:
    neutral, owner_power, god_power, people, god, owner, hand_x, hand_z, towns = case
    o.reset()
    world = setup_world(o)
    me = o.object(PLAYER_SIZE + 0x40, {P_POWER: god_power, P_PEOPLE: people, P_CITADEL: 0, P_ALL_ENABLED: 0})
    o.write(me + P_NUMBER, bytes([god]))
    other = world + WORLD_PLAYERS if neutral else o.object(PLAYER_SIZE + 0x40)
    o.write_u32(other + P_POWER, owner_power)
    o.write(other + P_NUMBER, bytes([owner]))
    action = o.object(0x40)
    o.write_u32(action + ACTION_HAND, hand_x, hand_z, 0)
    computer_table = o.object(0x40, {0x1C: o.alloc(16, ret_eax(me))})
    computer = o.object(0x200, {0: computer_table, CP_ACTION: action})
    to_me = o.object(0x40, {0x1C: o.alloc(16, ret_eax(me))})
    to_other = o.object(0x40, {0x1C: o.alloc(16, ret_eax(other))})
    made = []
    for t, town in enumerate(towns):
        adults, children, godowns, x, z = town[:5]
        beliefs = town[5:13]
        icons, enabled, requested = town[13:16]
        obj = o.object(0x800, {0: to_me if godowns else to_other, T_ADULTS: adults, T_CHILDREN: children})
        o.write(obj + T_OWNER, bytes([god if godowns else owner]))
        o.write_u32(obj + T_POS, x, z, 0)
        o.write_u32(obj + T_BELIEF, *beliefs)
        head = 0
        for i in reversed(range(icons)):
            kind = 1 + t * 3 + i
            head = o.object(0x200, {I_TYPE: kind, I_NEXT: head})
            o.write_u32(me + P_ENABLED + 4 * kind, (enabled >> i) & 1)
            o.write_u32(computer + CP_REQUESTED + 4 * kind, (requested >> i) & 1)
        o.write_u32(obj + T_ICONS, head)
        made.append(obj)
    for obj, after in zip(made, made[1:]):
        o.write_u32(obj + T_NEXT, after)
    o.write_u32(other + P_TOWNS, made[0] if made else 0)
    out = o.alloc(4)
    r = o.call(TOWN_WORTH, this=computer, args=[other, out])
    assert r.popped == 8 and r.depth == 1, r
    chosen = o.read_u32(out)[0]
    index = next((i for i, obj in enumerate(made) if obj == chosen), 0xFFFFFFFF)
    row = [neutral, owner_power, god_power, people, god, owner, hand_x, hand_z, len(towns)]
    for t in range(3):
        row += list(towns[t]) if t < len(towns) else [0] * len(TOWN_FIELDS)
    return row + [r.st0f, index]


# --- The feed factor of a town ---

def feed_factor_cases(rng: random.Random) -> list[tuple]:
    cases = []
    for _ in range(2500):
        god = rng.randrange(0, 7)
        own = 0 if rng.random() < 0.15 else 1
        others = rng.sample([n for n in range(7) if n != god], rng.randrange(0, 4))
        allies = [1 if rng.random() < 0.25 else 0 for _ in others]
        beliefs = [f2u(belief(rng)) for _ in range(8)]
        cases.append((own, god, others, allies, beliefs))
    return cases


def feed_factor(o: Oracle, case: tuple) -> list[int]:
    own, god, others, allies, beliefs = case
    o.reset()
    world = setup_world(o)
    me = o.object(PLAYER_SIZE + 0x40)
    o.write(me + P_NUMBER, bytes([god]))
    players = [me]
    for number, ally in zip(others, allies):
        p = o.object(PLAYER_SIZE + 0x40)
        o.write(p + P_NUMBER, bytes([number]))
        o.write(p + P_ALLY, bytes([ally]))
        players.append(p)
    owner = me if own else o.object(PLAYER_SIZE + 0x40)
    for p, after in zip(players, players[1:]):
        o.write_u32(p + P_NEXT, after)
    # The next player: the first of the list after none, else the next of the player
    o.write(NEXT_PLAYER, b"\x8B\x44\x24\x04\x85\xC0\x74\x09\x8B\x80" + struct.pack("<I", P_NEXT) + b"\xC2\x04\x00"
            + b"\xB8" + struct.pack("<I", players[0]) + b"\xC2\x04\x00")
    # The ally check: the ally byte of the other player
    o.write(IS_ALLIED, b"\x8B\x44\x24\x04\x8A\x80" + struct.pack("<I", P_ALLY) + b"\xC2\x04\x00")
    table = o.object(0x40, {0x1C: o.alloc(16, ret_eax(owner))})
    town = o.object(0x800, {0: table})
    o.write_u32(town + T_BELIEF, *beliefs)
    r = o.call(FEED_FACTOR, this=town, args=[me])
    assert r.popped == 4 and r.depth == 1, r
    _ = world
    numbers = others + [0xFF] * (3 - len(others))
    ally_flags = allies + [0] * (3 - len(allies))
    return [own, god, len(others), *numbers, *ally_flags, *beliefs, r.st0f]


# --- The wish to send the creature ---

def creature_help_cases(rng: random.Random) -> list[tuple]:
    cases = []
    for _ in range(2500):
        has = 0 if rng.random() < 0.05 else 1
        tied = rng.choice([0, 0, 0, 1, 2, 3, 4])
        same = 0 if rng.random() < 0.2 else 1
        size = rng.choice([0.0, 1.0, 0.25, rng.uniform(0.0, 2.0)])
        exhaustion = rng.choice([0.0, 1.0, rng.uniform(0.0, 1.0)])
        freedom = rng.choice([0.0, 1.0, rng.uniform(0.0, 1.0)])
        spread = rng.choice([10.0, 500.0, 1500.0, 3000.0])
        cx, cz = coord(rng), coord(rng)
        tx = cx + to_fixed(rng.uniform(-spread, spread))
        tz = cz + to_fixed(rng.uniform(-spread, spread))
        cases.append((has, tied, same, cx, cz, tx & 0xFFFFFFFF, tz & 0xFFFFFFFF, f2u(size), f2u(exhaustion),
                      f2u(freedom)))
    return cases


def creature_help(o: Oracle, case: tuple) -> list[int]:
    has, tied, same, cx, cz, tx, tz, size, exhaustion, freedom = case
    o.reset()
    setup_world(o)
    town_slot = o.alloc(16, read_field(OBJ_TOWN))
    table = o.object(0x100, {0x48: town_slot})
    town_a = o.object(0x40)
    town_b = o.object(0x40)
    target = o.object(0x800, {0: table, OBJ_TOWN: town_a})
    o.write_u32(target + T_POS, tx, tz, 0)
    physical = o.object(0x100, {C_SIZE: size, C_EXHAUSTION: exhaustion})
    creature = o.object(0x200, {C_PHYSICAL: physical})
    o.write_u32(creature + T_POS, cx, cz, 0)
    # 1: tied to the target; 2: to a thing of the target's town; 3: to a thing of another town; 4: to a thing of no town
    tie_to = {0: 0, 1: target,
              2: o.object(0x800, {0: table, OBJ_TOWN: town_a}),
              3: o.object(0x800, {0: table, OBJ_TOWN: town_b}),
              4: o.object(0x800, {0: table, OBJ_TOWN: 0})}[tied]
    tie = o.object(0x40, {TIE_TO: tie_to})
    status = o.object(0x200, {STATUS_TIE: tie})
    o.stub(CREATURE_OF, eax=creature if has else 0)
    o.stub(LEASH_STATUS, eax=status if tied else 0)
    o.stub(FREEDOM, st0=f32.of_bits(freedom))
    computer = o.object(0x200)
    r = o.call(CREATURE_HELP, this=computer, args=[target, same])
    assert r.popped == 8 and r.depth == 1, r
    return [*case, r.st0f]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as o:
        rows = [town_worth(o, case) for case in town_worth_cases(random.Random(0x668E20))]
        columns = ["neutral", "owner_power", "god_power", "god_people", "god", "owner", "hand_x", "hand_z", "towns"]
        for t in range(3):
            columns += [f"t{t}_{name}" for name in TOWN_FIELDS]
        columns += ["worth", "index"]
        n = write_pinned(out / "computer_conquest_town_worth.txt",
                         "The town of another player that a computer god wants most, and how much",
                         ["Function: thiscall on the computer player (other player, out town), ret 8, ST0.",
                          "Powers, beliefs and the worth are float bits; positions are map units; the rest are whole",
                          "numbers. Town N: adults, children, godowns (1 when the god owns it), x, z, the belief in",
                          "each player, the count of icons, and the bit of each icon that the god can cast and that",
                          "the god asked for. index is the chosen town, ffffffff for none."],
                         columns, rows)
        print(f"computer_conquest_town_worth: {n} rows")
        rows = [feed_factor(o, case) for case in feed_factor_cases(random.Random(0x741920))]
        columns = ["own", "god", "players", "p0", "p1", "p2", "a0", "a1", "a2", *[f"b{i}" for i in range(8)], "factor"]
        n = write_pinned(out / "computer_conquest_feed_factor.txt",
                         "How much a computer god wants to meet the needs of a town",
                         ["Function: thiscall on the town (player), ret 4, ST0. own is 1 when the god owns the town;",
                          "pN are the numbers of the other players in the list (ff for none), aN are 1 for an ally;",
                          "beliefs and the factor are float bits."],
                         columns, rows)
        print(f"computer_conquest_feed_factor: {n} rows")
        rows = [creature_help(o, case) for case in creature_help_cases(random.Random(0x65D1E0))]
        columns = ["creature", "tied", "same_town", "cx", "cz", "tx", "tz", "size", "exhaustion", "freedom", "wish"]
        n = write_pinned(out / "computer_conquest_creature_help.txt",
                         "How much a computer god wants its creature to go to a thing",
                         ["Function: thiscall on the computer player (target, same town byte), ret 8, ST0.",
                          "tied: 0 no tie, 1 the target, 2 a thing of the target's town, 3 a thing of another town,",
                          "4 a thing of no town. Positions are map units; size, exhaustion, freedom and the wish are",
                          "float bits. The creature, its leash and its freedom are stubs."],
                         columns, rows)
        print(f"computer_conquest_creature_help: {n} rows")


if __name__ == "__main__":
    main()
