#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the feelings a creature keeps for other creatures, in test/oracle/data:

- friend_fade.txt: how impressive a creature finds another creature, and the attention that creature gives it, in;
  both after the fade of one thinking turn out.
- friend_missed.txt: how nice the creature finds up to four other creatures, and the attention each gives it, in;
  how much the creature misses its friends out (the value of the desire source).
- friend_attention.txt: the attention that another creature gives the creature, and a change, in; the attention
  after the change, kept from 0 to 1, out.

The game's own functions run on records that the script lays out. The list of records is the one that the game walks.

Usage: python tools/oracle/cases/creature_friendship_cases.py [output folder]   (default: test/oracle/data)
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
FADE = 0x4C4D20  # thiscall (mind); RET
MISSED = 0x4DEDE0  # thiscall (creature); RET; the value on ST0 (no Ghidra function)
ATTENTION = 0x4C4F60  # thiscall (mind) (creature, change); RET 8

# Fields
CREATURE_MIND = 0x164
MIND_RECORDS = 0x1A9F4  # the head of the list of records: each node is (next, record)
MIND_COUNT = 0x1A9F8
RECORD_CREATURE = 0x0
RECORD_NICE = 0x4
RECORD_IMPRESSIVE = 0x8
RECORD_ATTENTION = 0xC

CASES = 4000
RECORDS = 4


def interesting(rng: random.Random, low: float, high: float) -> float:
    """A value from low to high, often at or near an edge of the rules"""
    pick = rng.random()
    if pick < 0.1:
        return rng.choice([0.0, 0.4, 1.0, low, high])
    if pick < 0.25:
        return rng.choice([0.4, 0.0, 1.0]) + rng.choice([-1, 1]) * rng.uniform(0.0, 1e-6)
    return rng.uniform(low, high)


def lay_records(o: Oracle, mind: int, records: list[int], nodes: list[int], count: int) -> None:
    head = 0
    for slot in reversed(range(count)):
        o.write_u32(nodes[slot], head, records[slot])
        head = nodes[slot]
    o.write_u32(mind + MIND_RECORDS, head, count)


def fade_cases(o: Oracle, rng: random.Random, mind: int, records: list[int], nodes: list[int]) -> list[list[int]]:
    rows = []
    for _ in range(CASES):
        impressive = interesting(rng, 0.0, 1.0)
        attention = interesting(rng, 0.0, 1.0)
        o.write_u32(records[0] + RECORD_IMPRESSIVE, f2u(impressive), f2u(attention))
        lay_records(o, mind, records, nodes, 1)
        result = o.call(FADE, this=mind)
        if result.depth != 0:
            raise RuntimeError(f"fade: depth {result.depth}")
        after = o.read_f32_bits(records[0] + RECORD_IMPRESSIVE, 2)
        rows.append([f2u(impressive), f2u(attention), after[0], after[1]])
    return rows


def missed_cases(o: Oracle, rng: random.Random, creature: int, mind: int, records: list[int],
                 nodes: list[int]) -> list[list[int]]:
    rows = []
    for _ in range(CASES):
        count = rng.randrange(0, RECORDS + 1)
        nice = [interesting(rng, -1.0, 1.0) for _ in range(RECORDS)]
        attention = [interesting(rng, 0.0, 1.0) for _ in range(RECORDS)]
        for slot in range(RECORDS):
            o.write_u32(records[slot] + RECORD_NICE, f2u(nice[slot]), 0, f2u(attention[slot]))
        lay_records(o, mind, records, nodes, count)
        result = o.call(MISSED, this=creature)
        if result.depth != 1:
            raise RuntimeError(f"missed: depth {result.depth}")
        rows.append([count] + [f2u(v) for v in nice] + [f2u(v) for v in attention] + [result.st0f])
    return rows


def attention_cases(o: Oracle, rng: random.Random, mind: int, records: list[int], nodes: list[int],
                    other: int) -> list[list[int]]:
    rows = []
    for _ in range(CASES):
        attention = interesting(rng, 0.0, 1.0)
        change = rng.choice([0.02, -0.02, rng.uniform(-1.5, 1.5), rng.uniform(-0.05, 0.05)])
        o.write_u32(records[0] + RECORD_CREATURE, other)
        o.write_u32(records[0] + RECORD_ATTENTION, f2u(attention))
        lay_records(o, mind, records, nodes, 1)
        result = o.call(ATTENTION, this=mind, args=[other, f32(change)])
        if result.popped != 8 or result.depth != 0:
            raise RuntimeError(f"attention: popped {result.popped}, depth {result.depth}")
        after = o.read_f32_bits(records[0] + RECORD_ATTENTION)[0]
        rows.append([f2u(attention), f2u(change), after])
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261110)
    with Oracle() as o:
        mind = o.alloc(MIND_RECORDS + 0x100)
        creature = o.object(0x200, {CREATURE_MIND: mind})
        other = o.alloc(0x200)
        records = [o.alloc(0x10) for _ in range(RECORDS)]
        nodes = [o.alloc(0x10) for _ in range(RECORDS)]
        fade = fade_cases(o, rng, mind, records, nodes)
        missed = missed_cases(o, rng, creature, mind, records, nodes)
        attention = attention_cases(o, rng, mind, records, nodes, other)
    write_pinned(out / "friend_fade.txt", "The fade of a creature's feelings for another creature in one thinking turn",
                 ["how impressive it finds the other creature and the attention the other creature gives it, in;",
                  "both after the fade, out"], ["impressive", "attention", "impressive_after", "attention_after"], fade)
    write_pinned(out / "friend_missed.txt", "How much a creature misses its friends",
                 ["the count of records, then how nice it finds each of four creatures and the attention each gives it,",
                  "in (only the first count records are in the list); the value of the desire source, out"],
                 ["count"] + [f"nice{i}" for i in range(RECORDS)] + [f"attention{i}" for i in range(RECORDS)]
                 + ["missed"], missed)
    write_pinned(out / "friend_attention.txt", "A change to the attention that another creature gives a creature",
                 ["the attention and the change in; the attention after the change, out"],
                 ["attention", "change", "attention_after"], attention)
    print(f"friend_fade {len(fade)} friend_missed {len(missed)} friend_attention {len(attention)}")


if __name__ == "__main__":
    main()
