#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the game's random numbers, in test/oracle/data:

- game_random.txt: one call of one of the four draws of the game (a whole number or a float, on the synced seed or on
  the local seed). The inputs are the draw, the seed before the call, and the range (a whole number for the whole
  number draws, the bits of a float for the float draws). The results are the draw (a whole number, or the bits of the
  float) and the seed after the call.

The four draws run as they are, with the game's generator, on a world object that holds the two seeds. The flag that
writes each draw to the game's log is 0, so the draws write nothing.

Usage: python tools/oracle/cases/game_random_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
GAME_RAND = 0x6DE510  # cdecl (n, file, line): the synced seed
GAME_FLOAT_RAND = 0x6DE530  # cdecl (x, file, line): the synced seed, returns ST0
LOCAL_RAND = 0x6DE570  # cdecl (n): the local seed
LOCAL_FLOAT_RAND = 0x6DE590  # cdecl (x): the local seed, returns ST0
WORLD = 0xD0195C  # the game's world pointer
LOG_FLAG = 0xCD3C80  # not 0: each draw writes to the game's log
SYNCED_SEED = 0x205A30 + 8  # the seed of the synced random numbers in the world
LOCAL_SEED = 0x205A3C  # the seed of the local random numbers in the world

DRAWS = {0: GAME_RAND, 1: GAME_FLOAT_RAND, 2: LOCAL_RAND, 3: LOCAL_FLOAT_RAND}
COLUMNS = ["draw", "seed", "range", "result", "seed_after"]


def f2u(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def ranges(pick: random.Random) -> list[int]:
    """Whole number ranges: the small ones of the creature code, 0, the float draw's own, large and negative ones."""
    fixed = [0, 1, 2, 3, 4, 5, 7, 8, 10, 12, 40, 100, 120, 201, 1000, 20000, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
    return fixed + [pick.getrandbits(32) for _ in range(12)] + [pick.randrange(1, 300) for _ in range(12)]


def floats(pick: random.Random) -> list[int]:
    """Float ranges as bits: the creature code's, 0 and -0, negative, tiny, huge, NaN and infinity."""
    fixed = [0.0, -0.0, 0.2, 0.6, 0.8, 1.0, 1.2, 3.0, 4.0, 5.0, 10.0, 15.0, 20.0, 60.0, 6.2831855, -1.0, -60.0, 1e-30,
             3e38, 0.4, 0.7, 2.0]
    bits = [f2u(x) for x in fixed] + [0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001]
    return bits + [f2u(pick.uniform(-100.0, 100.0)) for _ in range(10)]


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x5EED_6A3E)
    seeds = [0, 1, 0x88F89F, 0xFFFFFFFF, 0x80000000] + [pick.getrandbits(32) for _ in range(5)]
    rows = []
    with Oracle() as o:
        world = o.alloc(0x206000)
        o.write_u32(WORLD, world)
        o.write_u32(LOG_FLAG, 0)
        for draw, address in DRAWS.items():
            synced = draw < 2
            whole = draw % 2 == 0
            seed_at = world + (SYNCED_SEED if synced else LOCAL_SEED)
            for seed in seeds:
                for value in ranges(pick) if whole else floats(pick):
                    o.write_u32(world + SYNCED_SEED, 0x13579BDF)
                    o.write_u32(world + LOCAL_SEED, 0x2468ACE0)
                    o.write_u32(seed_at, seed)
                    arg = value if whole else f32.of_bits(value)
                    args = [arg, 0, 0] if synced else [arg]
                    r = o.call(address, args=args)
                    result = r.eax & 0xFFFFFFFF if whole else r.st0f
                    other = world + (LOCAL_SEED if synced else SYNCED_SEED)
                    untouched = 0x2468ACE0 if synced else 0x13579BDF
                    assert o.read_u32(other)[0] == untouched, "a draw changed the other seed"
                    rows.append([draw, seed, value, result, o.read_u32(seed_at)[0]])
    count = write_pinned(
        out / "game_random.txt",
        "The game's four random number draws and their seeds",
        [
            "draw: 0 GameRand (synced, whole), 1 GameFloatRand (synced, float), 2 LocalRand (local, whole),",
            "3 LocalFloatRand (local, float). range and result are the bits of a float for the float draws.",
            "seed is the seed of the draw's own stream before the call; seed_after is that seed after it.",
        ],
        COLUMNS,
        rows,
    )
    print(f"{count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
