#!/usr/bin/env python3
"""Makes the pinned data of how the plan of a creature moves its own alignment, from the original game.

Writes into test/oracle/data:

- creature_deed_alignment.txt: one creature turn. The game's deed update adds the change of the plan to the change
  that waits: the alignment table value of the plan's desire times the species scale, or for a hunger plan on a
  villager the weight of the villager's kind in the first (burn) row of the effect alignment table, damped by the
  creature's own alignment. Then the game's alignment turn holds the change that waits between -1 and 1, multiplies
  it by the species' most change for each turn, adds it to the alignment and holds that between -1 and 1.
- creature_deed_alignment_drift.txt: many turns of the same plan from a start alignment, with the species values of
  info.dat (scale 0.2, most change 0.0001). One row each 500 turns: the alignment that the game has then.

The plan check, the cast of the plan's object, its villager test, its alignment kind and the history are stubs. The
desire table and the alignment table are written at their places in the executable.

Usage: python tools/oracle/cases/creature_deed_alignment_cases.py [output folder]   (default: test/oracle/data)
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
DEED = 0x414220  # thiscall on the creature's alignment (creature), RET 4
PROCESS = 0x414140  # thiscall on the creature's alignment (creature), RET 4
PLAN_IS_VALID = 0x4F12E0  # thiscall on the plan, RET; stubbed
DYNAMIC_CAST = 0x7C5A19  # cdecl, five arguments; stubbed
IS_VILLAGER = 0x55CAB0  # thiscall (0), RET 4; stubbed
KIND_GETTER = 0x5013B0  # thiscall on the object's info, RET; stubbed
HISTORY_ADD = 0x414FA0  # thiscall (creature, change), RET 8; stubbed
MOST_CHANGE = 0x47B1F0  # the creature's most change for each turn: its info +0x348
DESIRE_TABLE = 0xC67EE8  # the alignment change of desire 0; a row of 0x1C0 bytes for each desire
DESIRE_ROW = 0x1C0
BURN_ROW = 0xC4CE30  # the first row of the effect alignment table, a float for each alignment kind

COLUMNS = ["alignment", "pending", "valid", "desire", "desire_change", "scale", "most_change", "object", "kind",
           "weight", "pending_deed", "alignment_after", "pending_after"]
DRIFT_COLUMNS = ["start", "valid", "desire", "desire_change", "object", "weight", "turn", "alignment"]

# The species values of info.dat after the creature balance table
INFO_SCALE = 0.2
INFO_MOST_CHANGE = 0.0001
DRIFT_TURNS = 120000
DRIFT_EVERY = 500


def to_float(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


class Turn:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.stub(HISTORY_ADD, eax=0, pop=8)
        self.alignment = o.alloc(0x20)
        self.creature = o.alloc(0x200)
        self.creature_vtable = o.alloc(0x100)
        self.info = o.alloc(0x400)
        self.mind = o.alloc(0x1000)
        self.belief = o.alloc(0x40)
        self.thing = o.alloc(0x40)
        self.thing_vtable = o.alloc(0x400)
        self.thing_info = o.alloc(0x40)
        self.thing_info_vtable = o.alloc(0x40)
        o.write_u32(self.creature, self.creature_vtable)
        o.write_u32(self.creature_vtable + 0x40, MOST_CHANGE)
        o.write_u32(self.creature + 0x28, self.info)
        o.write_u32(self.creature + 0x164, self.mind)
        o.write_u32(self.belief + 0x30, self.thing)
        o.write_u32(self.thing, self.thing_vtable)
        o.write_u32(self.thing_vtable + 0x2C8, IS_VILLAGER)
        o.write_u32(self.thing + 0x28, self.thing_info)
        o.write_u32(self.thing_info, self.thing_info_vtable)
        o.write_u32(self.thing_info_vtable + 0x34, KIND_GETTER)

    def set_up(self, valid: int, desire: int, desire_change: int, scale: int, most_change: int, thing: int, kind: int,
               weight: int) -> None:
        o = self.o
        o.stub(PLAN_IS_VALID, eax=valid, pop=0)
        o.write_u32(self.mind + 0xF50, desire)
        o.write_u32(self.mind + 0xF58, self.belief if thing else 0)
        o.stub(DYNAMIC_CAST, eax=self.thing if thing else 0, pop=0)
        o.stub(IS_VILLAGER, eax=1 if thing == 2 else 0, pop=4)
        o.stub(KIND_GETTER, eax=kind, pop=0)
        o.write_u32(DESIRE_TABLE + desire * DESIRE_ROW, desire_change)
        o.write_u32(BURN_ROW + kind * 4, weight)
        o.write_u32(self.info + 0x384, scale)
        o.write_u32(self.info + 0x348, most_change)

    def run(self, alignment: int, pending: int) -> tuple[int, int, int]:
        o = self.o
        o.write_u32(self.alignment + 0x8, alignment)
        o.write_u32(self.alignment + 0xC, pending)
        o.call(DEED, this=self.alignment, args=[self.creature])
        pending_deed = o.read_u32(self.alignment + 0xC)[0]
        o.call(PROCESS, this=self.alignment, args=[self.creature])
        return pending_deed, o.read_u32(self.alignment + 0x8)[0], o.read_u32(self.alignment + 0xC)[0]


def cases(rng: random.Random) -> list[list[int]]:
    rows: list[list[int]] = []

    def add(alignment, pending, valid, desire, desire_change, scale, most_change, thing, kind, weight):
        rows.append([f2u(to_float(alignment)), f2u(to_float(pending)), valid, desire, f2u(to_float(desire_change)),
                     f2u(to_float(scale)), f2u(to_float(most_change)), thing, kind, f2u(to_float(weight))])

    # The values of info.dat: compassion +1, anger -0.2, a villager eaten (weight -1), a skeleton eaten (weight +1)
    add(0.0, 0.0, 1, 1, 1.0, INFO_SCALE, INFO_MOST_CHANGE, 0, 0, 0.0)
    add(0.0, 0.0, 1, 2, -0.2, INFO_SCALE, INFO_MOST_CHANGE, 0, 0, 0.0)
    add(0.0, 0.0, 1, 4, 0.0, INFO_SCALE, INFO_MOST_CHANGE, 2, 5, -1.0)
    add(-0.5, 0.0, 1, 4, 0.0, INFO_SCALE, INFO_MOST_CHANGE, 2, 5, -1.0)
    add(0.5, 0.0, 1, 4, 0.0, INFO_SCALE, INFO_MOST_CHANGE, 2, 4, 1.0)
    # An animal or food eaten: the hunger desire's own change, 0
    add(0.0, 0.0, 1, 4, 0.0, INFO_SCALE, INFO_MOST_CHANGE, 1, 5, -1.0)
    # No plan carried out: only the change that waits moves the alignment
    add(0.2, 0.3, 0, 1, 1.0, INFO_SCALE, INFO_MOST_CHANGE, 0, 0, 0.0)
    # The ends: a change that waits beyond 1, an alignment beyond 1, zero on each side
    add(0.99999, 5.0, 1, 1, 1.0, 1.0, 0.5, 0, 0, 0.0)
    add(-0.99999, -5.0, 1, 2, -1.0, 1.0, 0.5, 0, 0, 0.0)
    add(-0.0, 0.0, 1, 1, 1.0, INFO_SCALE, INFO_MOST_CHANGE, 0, 0, 0.0)
    add(0.0, 0.0, 1, 2, -0.2, INFO_SCALE, INFO_MOST_CHANGE, 0, 0, 0.0)
    for _ in range(4000):
        desire = rng.choice([4, 4, rng.randrange(40)])
        thing = rng.randrange(3)
        add(rng.choice([0.0, rng.uniform(-1.0, 1.0), rng.uniform(-1e-3, 1e-3)]),
            rng.choice([0.0, rng.uniform(-0.5, 0.5), rng.uniform(-3.0, 3.0)]), rng.choice([1, 1, 1, 0]), desire,
            rng.choice([0.0, rng.uniform(-1.0, 1.0)]), rng.choice([INFO_SCALE, 1.0, rng.uniform(0.0, 2.0)]),
            rng.choice([INFO_MOST_CHANGE, rng.uniform(0.0, 0.5)]), thing, rng.randrange(16),
            rng.choice([-1.0, 1.0, rng.uniform(-4.0, 4.0)]))
    return rows


def drift(turn: Turn) -> list[list[int]]:
    rows: list[list[int]] = []
    # A villager eaten from neutral and from good, a compassion plan from evil, an anger plan from neutral
    chains = [(0.0, 1, 4, 0.0, 2, -1.0), (0.6, 1, 4, 0.0, 2, -1.0), (-0.9, 1, 1, 1.0, 0, 0.0),
              (0.0, 1, 2, -0.2, 0, 0.0)]
    for start, valid, desire, desire_change, thing, weight in chains:
        bits = [f2u(to_float(v)) for v in (start, desire_change, weight)]
        turn.set_up(valid, desire, bits[1], f2u(to_float(INFO_SCALE)), f2u(to_float(INFO_MOST_CHANGE)), thing, 5,
                    bits[2])
        alignment, pending = bits[0], 0
        for number in range(1, DRIFT_TURNS + 1):
            _, alignment, pending = turn.run(alignment, pending)
            if number % DRIFT_EVERY == 0:
                rows.append([bits[0], valid, desire, bits[1], thing, bits[2], number, alignment])
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261111)
    rows = cases(rng)
    with Oracle() as o:
        turn = Turn(o)
        results = []
        for row in rows:
            alignment, pending, valid, desire, desire_change, scale, most_change, thing, kind, weight = row
            turn.set_up(valid, desire, desire_change, scale, most_change, thing, kind, weight)
            results.append(row + list(turn.run(alignment, pending)))
        drift_rows = drift(turn)
    write_pinned(out / "creature_deed_alignment.txt", "One creature turn of the alignment from its plan",
                 ["Each row: the creature's alignment and the change that waits, whether the plan is carried out, the "
                  "plan's desire, that desire's alignment change, the species scale and most change for each turn, "
                  "the plan's object (0 none, 1 not a villager, 2 a villager), its alignment kind and that kind's "
                  "weight in the burn row; then the change that waits after the deed, the alignment and the change "
                  "that waits after the turn."],
                 COLUMNS, results)
    write_pinned(out / "creature_deed_alignment_drift.txt", "The alignment of a creature over many turns of one plan",
                 [f"Each row: the start alignment, whether the plan is carried out, the desire, its change, the "
                  f"object (2 a villager of alignment kind 5), the burn weight, the turn and the alignment then. "
                  f"Species scale {INFO_SCALE}, most change {INFO_MOST_CHANGE}, one row each {DRIFT_EVERY} turns."],
                 DRIFT_COLUMNS, drift_rows)


if __name__ == "__main__":
    main()
