#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the wood flag of a workshop, in test/oracle/data:

- workshop_wood_flag.txt: the wood that the workshop holds and the wood of a scaffold in; how much the flag shows that
  the workshop wants wood out. The game's own function runs; the call that reads the wood of the workshop is a stub that
  reads it from memory.
- need_flag_fill.txt: the need of a flag and the need of its row of the info at the full height in; the share of the
  full height out. The game's own function runs.

Usage: python tools/oracle/cases/workshop_wood_flag_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
WOOD_WANT = 0x779B90  # thiscall (workshop); RET; the value on ST0
SCAFFOLD_INFO = 0xD959D0  # the row that the workshop reads; the wood of a scaffold at +0x6C
FILL = 0x71A200  # thiscall (flag); RET; the share on ST0

WOOD_COLUMNS = ["wood", "scaffold_wood", "result"]
FILL_COLUMNS = ["need", "max_need", "result"]


def wood_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(0x700D1)
    wood = o.alloc(0x10)
    # The wood of the workshop: MOV EAX, [wood]; RET 4
    reader = o.alloc(0x20)
    o.write(reader, bytes([0xA1]) + struct.pack("<I", wood) + bytes([0xC2, 0x04, 0x00]))
    vtable = o.alloc(0x200)
    o.write_u32(vtable + 0x98, reader)
    workshop = o.alloc(0x200)
    o.write_u32(workshop, vtable)
    rows = []
    for index in range(2000):
        scaffold_wood = rng.choice([0, 1, 100, 200, 250, 300, rng.randrange(0, 1000), rng.randrange(0, 1 << 20)])
        amount = rng.choice([0, scaffold_wood, scaffold_wood // 2, rng.randrange(0, 2 * scaffold_wood + 2),
                             rng.randrange(0, 1 << 24), rng.randrange(0, 1 << 32)])
        o.write_u32(wood, amount)
        o.write_u32(SCAFFOLD_INFO + 0x6C, scaffold_wood)
        result = o.call(WOOD_WANT, this=workshop)
        if result.popped != 0 or result.depth != 1:
            raise RuntimeError(f"case {index}: popped {result.popped}, depth {result.depth}")
        rows.append([amount, scaffold_wood, result.st0f])
    return rows


def fill_cases(o: Oracle) -> list[list[int]]:
    rng = random.Random(0x700D2)
    info = o.alloc(0x200)
    flag = o.alloc(0x100)
    o.write_u32(flag + 0x28, info)
    rows = []
    for index in range(2000):
        max_need = rng.choice([1.0, 1.0, 0.5, rng.uniform(0.1, 3.0)])
        need = rng.choice([0.0, max_need, rng.uniform(0.0, 1.0), rng.uniform(0.0, 2.0 * max_need), rng.uniform(-1.0, 1.0)])
        o.write_u32(info + 0x108, f2u(max_need))
        o.write_u32(flag + 0x5C, f2u(need))
        result = o.call(FILL, this=flag)
        if result.popped != 0 or result.depth != 1:
            raise RuntimeError(f"case {index}: popped {result.popped}, depth {result.depth}")
        rows.append([f2u(need), f2u(max_need), result.st0f])
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as o:
        woods = wood_cases(o)
        o.reset()
        fills = fill_cases(o)
    write_pinned(out / "workshop_wood_flag.txt", "How much the wood flag of a workshop shows that it wants wood",
                 ["the wood that the workshop holds and the wood of a scaffold in; the want out"], WOOD_COLUMNS, woods)
    write_pinned(out / "need_flag_fill.txt", "The share of its full height that a flag goes to",
                 ["the need and the need at the full height in; the share out"], FILL_COLUMNS, fills)
    print(f"workshop_wood_flag {len(woods)} need_flag_fill {len(fills)}")


if __name__ == "__main__":
    main()
