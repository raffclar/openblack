#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the side that a creature's body starts an action on, in test/oracle/data:

- creature_start_side.txt: one call of one of the body's three starts (a static action, an action played once, a
  faint). The inputs are the start, the game's random seed, whether the body is busy, whether the script slowed the
  body's clips, and whether the hand holds a thing on its left side or its right side. The results are whether the
  start started, the side that the body plays on, and the game's random seed after the call.

The three starts run as they are. Only the clip lookup (a clip is always there), the change of the body's state and
the few calls of the faint that do not touch the side or the random numbers are stubbed. The game's random numbers
run as they are, on a world object that holds the seed.

Usage: python tools/oracle/cases/creature_start_side_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
START_STATIC = 0x484310  # thiscall (clip), ret 4
START_INDIVIDUAL = 0x484B00  # thiscall (clip), ret 4
START_FAINT = 0x484CE0  # thiscall (clip), ret 4
GET_ANIM = 0x619650  # thiscall (clip, flag), ret 8: stubbed, every clip is there
STATE_SET = 0x484EC0  # thiscall (state), ret 4: stubbed
IS_DEAD = 0x484E00  # thiscall, ret 0: stubbed, not dead
IS_ACTUALLY_DEAD = 0x484E10  # thiscall, ret 0: stubbed, not dead
GROUND_ANGLE = 0x480A60  # thiscall, returns ST0: stubbed
PLACE_BODY = 0x863B00  # thiscall (point, angle), ret 8: stubbed
WORLD = 0xD0195C  # the game's world pointer
RAND_SEED = 0x205A30 + 8  # the seed of the game's synced random numbers in the world

# The body's fields
CREATURE = 0x4834  # the creature that owns the body: the game's random numbers when set
BUSY = 0x4994  # the body's state: not 0 while it does an action or walks
SLOWED = 0x4A90  # the script's animation time change
HOLDS = 0x48D0  # the thing that the hand holds
HELD_LEFT = 0x5230  # the hand holds it on the left side
SIDE = 0x522C  # the side of the action: 1 is the left side

STARTS = {0: START_STATIC, 1: START_INDIVIDUAL, 2: START_FAINT}
COLUMNS = ["start", "seed", "busy", "slowed", "holds", "held_left", "started", "side", "seed_after"]


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x5EED_0670)
    rows = []
    with Oracle() as o:
        world = o.alloc(0x206000)
        o.write_u32(WORLD, world)
        o.stub(GET_ANIM, eax=1, pop=8)
        o.stub(STATE_SET, eax=0, pop=4)
        o.stub(IS_DEAD, eax=0)
        o.stub(IS_ACTUALLY_DEAD, eax=0)
        o.stub(GROUND_ANGLE, st0=0.0)
        o.stub(PLACE_BODY, eax=0, pop=8)
        for start in STARTS:
            for case in range(48):
                seed = pick.getrandbits(32)
                busy = case & 1
                slowed = (case >> 1) & 1
                holds = (case >> 2) & 1
                held_left = (case >> 3) & 1
                body = o.object(0x5400, {CREATURE: 1, BUSY: busy, SLOWED: slowed, HOLDS: holds, HELD_LEFT: held_left,
                                         SIDE: 0xFFFFFFFF})
                o.write_u32(world + RAND_SEED, seed)
                r = o.call(STARTS[start], this=body, args=[28])
                side = o.read_u32(body + SIDE)[0]
                seed_after = o.read_u32(world + RAND_SEED)[0]
                rows.append([start, seed, busy, slowed, holds, held_left, r.eax & 0xFF, side, seed_after])
    count = write_pinned(
        out / "creature_start_side.txt",
        "The side that a creature's body starts an action on",
        [
            "start: 0 a static action, 1 an action played once, 2 a faint. The clip is 28 in each call.",
            "side: 1 the left side, 0 the right side, ffffffff when the start did not start (the field keeps its value).",
            "busy, slowed, holds and held_left are 0 or 1. The body has a creature, so the starts use the synced seed.",
        ],
        COLUMNS,
        rows,
    )
    print(f"{count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
