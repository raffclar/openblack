#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how an effect moves the waiting alignment of whoever it came from, in test/oracle/data:

- effect_alignment_order.txt: the waiting alignment after the game's effect update for one object. The update finds
  the change of life of the object, adds the player's effect addition to its size, and then for each kind of effect
  (crush, hit, heal, fly away, then the heat by the harm it does) damps the change by the alignment of the owner and
  adds it to the waiting alignment, one at a time.

The object's life, its alignment kind, the player and the harm of the heat are stubs; the alignment table is written
at its place in the executable. The history of each change is a stub too (the alignment history has its own tests).

Usage: python tools/oracle/cases/effect_alignment_order_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions and data (BW1W120, runblack.exe 1.20)
UPDATE = 0x414410  # thiscall on the alignment (object, effect values, life before), RET 0xC
HISTORY_ADD = 0x414E10  # thiscall, RET 0x10; stubbed
GET_PLAYER = 0x5509B0  # thiscall on the game (index), RET 4; stubbed
HEAT_DAMAGE = 0x72EEC0  # cdecl (object, temperature), returns ST0; stubbed
GAME = 0xD0195C  # the game object that the player lookup takes
ALIGNMENT_TABLE = 0xC4CE30  # the alignment table: a row of 0x48 bytes for each kind of effect, a float per object kind
ROW_SIZE = 0x48
# Two functions that the update never reaches, used as the object's life getter and the alignment kind getter
LIFE_GETTER = 0x7FAAF0
KIND_GETTER = 0x5013B0

COLUMNS = ["alignment", "pending", "life_before", "life_now", "addition", "kind", "crush", "hit", "heal", "fly_away",
           "burn_damage", "w_burn", "w_crush", "w_hit", "w_heal", "w_fly_away", "pending_after"]


def to_float(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


class Update:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.stub(HISTORY_ADD, eax=0, pop=0x10)
        self.alignment = o.alloc(0x20)
        self.thing = o.alloc(0x40)
        self.thing_vtable = o.alloc(0x200)
        self.info = o.alloc(0x40)
        self.info_vtable = o.alloc(0x40)
        self.values = o.alloc(0x40)
        self.player = o.alloc(0x80)
        self.player_info = o.alloc(0x40)
        o.write_u32(self.thing, self.thing_vtable)
        o.write_u32(self.thing_vtable + 0x11C, LIFE_GETTER)
        o.write_u32(self.thing + 0x28, self.info)
        o.write_u32(self.info, self.info_vtable)
        o.write_u32(self.info_vtable + 0x34, KIND_GETTER)
        o.write_u32(self.player + 0x64, self.player_info)
        o.write_u32(GAME, o.alloc(0x10))
        o.stub(GET_PLAYER, eax=self.player, pop=4)

    def run(self, row: list[int]) -> int:
        o = self.o
        (alignment, pending, life_before, life_now, addition, kind, crush, hit, heal, fly_away, burn_damage,
         *weights) = row
        o.write_u32(self.alignment + 0x8, alignment)
        o.write_u32(self.alignment + 0xC, pending)
        o.stub(LIFE_GETTER, eax=0, pop=0, st0=f32.of_bits(life_now))
        o.stub(KIND_GETTER, eax=kind, pop=0)
        o.stub(HEAT_DAMAGE, eax=0, pop=0, st0=f32.of_bits(burn_damage))
        o.write_u32(self.player_info + 0x1C, addition)
        # The temperature itself is not read by the stub; the values hold the other four kinds after it
        o.write(self.values + 0x8, struct.pack("<5I", 0, crush, hit, heal, fly_away))
        for effect_kind, weight in enumerate(weights):
            o.write_u32(ALIGNMENT_TABLE + effect_kind * ROW_SIZE + kind * 4, weight)
        o.call(UPDATE, this=self.alignment, args=[self.thing, self.values, f32.of_bits(life_before)])
        return o.read_u32(self.alignment + 0xC)[0]


def cases(rng: random.Random) -> list[list[int]]:
    rows: list[list[int]] = []

    def add(alignment, pending, before, now, addition, kind, crush, hit, heal, fly, burn, weights):
        rows.append([f2u(to_float(v)) for v in (alignment, pending, before, now, addition)] + [kind] +
                    [f2u(to_float(v)) for v in (crush, hit, heal, fly, burn)] +
                    [f2u(to_float(w)) for w in weights])

    # Chosen: no change of life, a pure heal, a pure harm, each sign of the owner's alignment
    add(0.0, 0.0, 0.5, 0.5, 0.75, 5, 1.0, 1.0, 1.0, 1.0, 1.0, [1.0, -1.0, -0.8, 1.0, -0.2])
    add(0.3, 0.01, 0.2, 0.7, 0.75, 5, 0.0, 0.0, 0.5, 0.0, 0.0, [-1.0, -1.0, -0.8, 1.0, -0.2])
    add(-0.6, -0.02, 0.9, 0.4, 0.75, 5, 0.2, 0.3, 0.0, 0.1, 0.25, [-1.0, -1.0, -0.8, 1.0, -0.2])
    add(0.0, 0.0, 1.0, 0.0, 0.0, 2, 1.0, 0.0, 0.0, 0.0, 0.0, [0.0, -0.5, 0.0, 0.0, 0.0])
    add(-0.0, 0.0, 1.0, 0.0, 0.0, 2, 1.0, 0.0, 0.0, 0.0, 0.0, [0.0, 0.5, 0.0, 0.0, 0.0])
    # Random: mixed kinds and signs, small and large waiting values, where the order of the additions shows
    for _ in range(3000):
        kind = rng.randrange(14)
        add(rng.uniform(-1.0, 1.0), rng.choice([0.0, rng.uniform(-0.05, 0.05), rng.uniform(-1.0, 1.0)]),
            rng.uniform(-0.2, 1.0), rng.uniform(-0.2, 1.0), rng.uniform(0.0, 1.0), kind,
            *[rng.choice([0.0, rng.uniform(-2.0, 2.0)]) for _ in range(5)],
            [rng.uniform(-1.0, 1.0) for _ in range(5)])
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261102)
    rows = cases(rng)
    with Oracle() as o:
        update = Update(o)
        results = [row + [update.run(row)] for row in rows]
    write_pinned(out / "effect_alignment_order.txt", "The waiting alignment after the game's effect update",
                 ["Each row: the owner's alignment and waiting alignment, the object's life before and now, the "
                  "player's effect addition, the object's alignment kind, the four kinds of the effect after the "
                  "heat, the harm of the heat, the five weights of the table (heat first), and the waiting alignment "
                  "after the update."],
                 COLUMNS, results)


if __name__ == "__main__":
    main()
