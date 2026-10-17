#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the creature's test of whether it may try a miracle, in test/oracle/data:

- creature_may_try.txt: the share of the sightings that a creature saw against the sightings it needs

Usage: python tools/oracle/cases/creature_cast_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
MAY_CAST = 0x4D2940  # thiscall (creature), 2 stack args (plan, action), returns EAX 0/1
TIMES_NEEDED = 0x4F8CA0  # thiscall (creature), 1 stack arg (magic type), returns ST0; stubbed to give our value
ENERGY_TEST = 0x4F82F0  # thiscall (creature), 1 stack arg (magic type); stubbed to pass
ACTION_MAGIC_TABLE = 0xC6C544  # the magic type of each action, rows of 0x110 bytes
MULTIPLAYER_FLAG = 0xD00E00

ACTION = 0
MAGIC_TYPE = 5
NAN = 0x7FC00000
POS_INF = 0x7F800000


class MayTry:
    COLUMNS = ["seen", "needed", "may_try"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.creature = oracle.alloc(0x200)
        self.mind = oracle.alloc(0x18000)
        oracle.write_u32(self.creature + 0x164, self.mind)
        oracle.write_u32(ACTION_MAGIC_TABLE + ACTION * 0x110, MAGIC_TYPE)
        oracle.write_u32(MULTIPLAYER_FLAG, 0)
        oracle.stub(ENERGY_TEST, eax=1, pop=4)

    def run(self, seen: int, needed_bits: int) -> int:
        o = self.o
        o.write_u32(self.mind + 0x17D38 + MAGIC_TYPE * 4, seen)
        o.stub(TIMES_NEEDED, eax=0, pop=4, st0=f32.of_bits(needed_bits))
        result = o.call(MAY_CAST, this=self.creature, args=[0, ACTION])
        assert result.popped == 8, result
        return result.eax & 0xFF


def cases(rng: random.Random) -> list[tuple[int, int]]:
    chosen = [(0, f2u(0.0)), (3, f2u(0.0)), (0, NAN), (4, NAN), (4, POS_INF), (0, f2u(-1.0)), (5, f2u(-1.0)),
              (4, f2u(9.0)), (9, f2u(18.0)), (10, f2u(9.0)), (9, f2u(9.0))]
    rows = list(chosen)
    for _ in range(300):
        rows.append((rng.randint(0, 60), f2u(rng.uniform(0.5, 80.0))))
    # One step either side of a share of a half and of a share of one
    for seen in (1, 3, 7, 9, 13, 50):
        for needed in (f2u(2.0 * seen), f2u(float(seen))):
            rows += [(seen, needed - 1), (seen, needed), (seen, needed + 1)]
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x4D2940)
    with Oracle() as oracle:
        may_try = MayTry(oracle)
        rows = [[seen, needed, may_try.run(seen, needed)] for seen, needed in cases(rng)]
        n = write_pinned(out / "creature_may_try.txt", "The creature's test of whether it may try a miracle",
                         ["Function: thiscall on the creature, args (plan, action); EAX 1 when it may try.",
                          "seen is the count of sightings (an int); needed is the float bits of the sightings it needs.",
                          "The energy test passes, so only the share of the sightings decides."],
                         MayTry.COLUMNS, rows)
        print(f"creature_may_try: {n} rows, {sum(r[-1] for r in rows)} may try")


if __name__ == "__main__":
    main()
