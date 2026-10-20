#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the throb of the land markers (the camera bookmarks and the leash's target ring), in
test/oracle/data:

- marker_pulse.txt: one step of the markers' shared clock. The inputs are the clock before the step and the frame's
  game time, both in milliseconds. The results are the clock after the step and the bits of the markers' half width
  and half height.

The game's function runs as it is: it reads its clock and the frame's game time from two globals, and writes the clock
and the two sizes to three globals.

Usage: python tools/oracle/cases/marker_pulse_cases.py [output folder]   (default: test/oracle/data)
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
PULSE = 0x43A9E0  # cdecl (): steps the clock and works out the two sizes
CLOCK = 0xC58CC8  # int, milliseconds
FRAME_MS = 0xEA9EC0  # int, the frame's game time in milliseconds
HALF_WIDTH = 0xC58B24  # float
HALF_HEIGHT = 0xC58B20  # float

COLUMNS = ["clock", "frame_ms", "clock_after", "half_width", "half_height"]


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x0B00_C3A2)
    cases = [(clock, 0) for clock in range(0, 2001)]
    cases += [(clock, step) for clock in (0, 1, 999, 1000, 1799, 1800, 1801, 1999, 2000) for step in
              (1, 10, 16, 17, 33, 100, 199, 200, 201, 1000, 1999, 2000, 2001, 4000, 4001)]
    cases += [(pick.randrange(0, 2001), pick.randrange(0, 300)) for _ in range(600)]
    rows = []
    with Oracle() as o:
        for clock, step in cases:
            o.write_u32(CLOCK, clock)
            o.write_u32(FRAME_MS, step)
            o.call(PULSE)
            rows.append([clock, step, o.read_u32(CLOCK)[0], o.read_u32(HALF_WIDTH)[0], o.read_u32(HALF_HEIGHT)[0]])
    count = write_pinned(
        out / "marker_pulse.txt",
        "The throb of the land markers: one step of their shared clock and their two sizes",
        [
            "clock and clock_after are the shared clock in milliseconds before and after the step; frame_ms is the",
            "frame's game time. half_width and half_height are the bits of the floats the markers are drawn at.",
        ],
        COLUMNS,
        rows,
    )
    print(f"{count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
