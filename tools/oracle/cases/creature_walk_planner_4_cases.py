#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes pinned data of the milliseconds that a creature lives in a game turn, in test/oracle/data:

- creature_turn_ms.txt: the creature's milliseconds of a turn at its pace (a freeze makes the pace less than 1), and
  the milliseconds that its animations play in those milliseconds at its size. The game does both calculations inside
  larger functions. Thus the case runs the game's own instructions (copied with their constants and the game's float
  to integer conversion) in small pieces of code that it writes over a function that no case calls.

Usage: python tools/oracle/cases/creature_walk_planner_4_cases.py [output folder]   (default: test/oracle/data)
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

# The game's code (BW1W120, runblack.exe 1.20)
FTOL = 0x7A1400  # the C library's float to int conversion
SSE2_FLAG = 0xE83A20  # the C library's flag that selects the SSE2 path of its float to int; 0 is the 64-bit path that
# openblack copies on every target (deviations row 177)
HALF = 0x8AA3B4  # 0.5
SLOWING = 0x8CEFAC  # 0.85
RATE_AT_ZERO = 0x8CF024  # 1.6
SIZE = 0x90  # the 3D creature's size
CODE = 0x526E00  # the falling spell's frame: no case calls it, so the small pieces of code go over it


def call_ftol() -> bytes:
    return b"\xB8" + struct.pack("<I", FTOL) + b"\xFF\xD0"  # mov eax, FTOL; call eax


# cdecl int turn_ms(float pace, int game_ms): fld pace; fimul game_ms; ftol (the creature turn's order)
TURN_MS = b"\xD9\x44\x24\x04" + b"\xDA\x4C\x24\x08" + call_ftol() + b"\xC3"

# cdecl int clip_ms(body*, int ms), the order of the 3D creature's frame:
# push esi; mov esi, body; fild ms; fld [esi+0x90]; fmul 0.5; fmul 0.85; fsubr 1.6; fmul st, st1; ftol; fstp st0
CLIP_MS = (b"\x56" + b"\x8B\x74\x24\x08" + b"\xDB\x44\x24\x0C" + b"\xD9\x86" + struct.pack("<I", SIZE) +
           b"\xD8\x0D" + struct.pack("<I", HALF) + b"\xD8\x0D" + struct.pack("<I", SLOWING) +
           b"\xD8\x2D" + struct.pack("<I", RATE_AT_ZERO) + b"\xD8\xC9" + call_ftol() + b"\xDD\xD8" + b"\x5E\xC3")


def paces(rng: random.Random) -> list[float]:
    fixed = [1.0, 0.0, 0.5, 0.999, 0.001, 0.25, 0.75, 0.1, 0.3, 0.7, 0.9, 1e-7, 0.99999994]
    return fixed + [rng.random() for _ in range(1000)] + [1.0 - rng.random() for _ in range(487)]


def sizes(rng: random.Random) -> list[float]:
    fixed = [0.05, 0.1, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 0.3, 0.7]
    return fixed + [rng.uniform(0.05, 4.0) for _ in range(1488)]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x244_4)
    rows = []
    with Oracle() as o:
        o.write_u32(SSE2_FLAG, 0)
        turn_ms = CODE
        clip_ms = CODE + 0x40
        o.write(turn_ms, TURN_MS)
        o.write(clip_ms, CLIP_MS)
        body = o.alloc(0x100)
        for pace, size in zip(paces(rng), sizes(rng)):
            game_ms = 100 if rng.random() < 0.8 else rng.randint(1, 1000)
            pace_bits = f2u(pace)
            ms = o.call(turn_ms, args=[pace_bits, game_ms]).eax & 0xFFFFFFFF
            o.write_u32(body + SIZE, f2u(size))
            step = o.call(clip_ms, args=[body, ms]).eax & 0xFFFFFFFF
            rows.append([pace_bits, game_ms, ms, f2u(size), step])
    n = write_pinned(out / "creature_turn_ms.txt", "The milliseconds of a creature's turn and of its animations",
                     ["pace and size are float bits; game_ms, turn_ms and clip_ms are whole milliseconds",
                      "turn_ms = the game's ftol(pace x game_ms); clip_ms = ftol((1.6 - (size x 0.5) x 0.85) x turn_ms)"],
                     ["pace", "game_ms", "turn_ms", "size", "clip_ms"], rows)
    print(f"creature_turn_ms.txt: {n} rows")


if __name__ == "__main__":
    main()
