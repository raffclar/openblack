#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of when a creature is far enough from its home to cast the teleport miracle, in
test/oracle/data:

- creature_teleport_from_afar.txt: the creature's map position and its home's map position. The game measures from the
  creature to the middle of the home's cell and lets the creature cast the teleport miracle when that is more than 150
  metres. The test of whether the creature knows the miracle and can pay for it is stubbed to yes.

Usage: python tools/oracle/cases/creature_teleport_from_afar_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions and data (BW1W120, runblack.exe 1.20)
CAN_CAST_TELEPORT = 0x4D2B20  # thiscall on the creature (plan, action), RET 8, returns EAX
CAN_CAST_SPELL = 0x4D2940  # thiscall on the creature (plan, action), RET 8; stubbed: it knows the miracle and can pay
MAKE_INVERSE_SQRT_TABLE = 0x74F590  # cdecl (), fills the table the distance reads, once, as the game does at start-up
INVERSE_SQRT_TABLE = 0xDA5A10  # 1024 dwords
POSITION = 0x14  # the creature's map position
HOME = 0x1200  # the creature's home, a map position
UNITS_PER_METRE = 6553.6


class TeleportFromAfar:
    COLUMNS = ["x", "z", "home_x", "home_z", "far"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.call(MAKE_INVERSE_SQRT_TABLE)
        assert any(o.read_u32(INVERSE_SQRT_TABLE, 1024))
        o.stub(CAN_CAST_SPELL, eax=1, pop=8)
        self.creature = o.alloc(0x1300)

    def run(self, x: int, z: int, home_x: int, home_z: int) -> int:
        o = self.o
        o.write(self.creature + POSITION, struct.pack("<iif", x, z, 0.0))
        o.write(self.creature + HOME, struct.pack("<iif", home_x, home_z, 0.0))
        result = o.call(CAN_CAST_TELEPORT, this=self.creature, args=[0, 0])
        assert result.popped == 8, result
        return result.eax


def cases(rng: random.Random) -> list[tuple[int, int, int, int]]:
    rows = []
    for _ in range(2000):
        home_x = rng.randrange(1 << 16, 5100 << 16)
        home_z = rng.randrange(1 << 16, 5100 << 16)
        # Most near the edge of 150 metres from the middle of the home's cell, where the rounding decides
        reach = rng.choice([rng.uniform(149.9, 150.1), rng.uniform(140.0, 160.0), rng.uniform(0.0, 1000.0)])
        angle = rng.uniform(-math.pi, math.pi)
        centre_x = (home_x & ~0xFFFF) + 0x8000
        centre_z = (home_z & ~0xFFFF) + 0x8000
        x = centre_x + int(reach * math.cos(angle) * UNITS_PER_METRE)
        z = centre_z + int(reach * math.sin(angle) * UNITS_PER_METRE)
        rows.append((x, z, home_x, home_z))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261019)
    inputs = cases(rng)
    rows = []
    with Oracle() as o:
        test = TeleportFromAfar(o)
        for x, z, home_x, home_z in inputs:
            rows.append((x, z, home_x, home_z, test.run(x, z, home_x, home_z)))
    write_pinned(
        out / "creature_teleport_from_afar.txt",
        "When a creature is far enough from its home to cast the teleport miracle",
        [
            "The game's precondition of the action that casts the teleport miracle (thiscall on the creature, its map",
            "position at +0x14 and its home's map position at +0x1200), with the test of whether it knows the miracle",
            "and can pay for it stubbed to yes. far is EAX: 1 when the creature is more than 150 metres from the",
            "middle of its home's cell. The columns are map positions, in 16.16 fixed point.",
        ],
        TeleportFromAfar.COLUMNS,
        rows,
    )
    print(f"{len(rows)} rows, {sum(1 for r in rows if r[4])} far")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
