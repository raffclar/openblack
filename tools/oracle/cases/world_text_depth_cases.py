#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the depth at which the game draws a box of words over a point, in test/oracle/data:

- world_text_depth.txt: the drawn depth from the camera's origin and focus, the near plane and the point's depth

Usage: python tools/oracle/cases/world_text_depth_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The function and the globals that it reads (BW1W120, runblack.exe 1.20)
DRAWN_DEPTH = 0x5766E0  # cdecl (point depth), returns ST0
CAMERA_ORIGIN = 0xEA1DB8  # three floats
CAMERA_FOCUS = 0xEA1DC4  # three floats
NEAR_CLIP = 0xE839E0  # float


class DrawnDepth:
    COLUMNS = ["origin_x", "origin_y", "origin_z", "focus_x", "focus_y", "focus_z", "near", "depth", "drawn"]

    def __init__(self, oracle: Oracle):
        self.o = oracle

    def run(self, ox: int, oy: int, oz: int, fx: int, fy: int, fz: int, near: int, depth: int) -> int:
        o = self.o
        o.write(CAMERA_ORIGIN, struct.pack("<III", ox, oy, oz))
        o.write(CAMERA_FOCUS, struct.pack("<III", fx, fy, fz))
        o.write(NEAR_CLIP, struct.pack("<I", near))
        result = o.call(DRAWN_DEPTH, args=[f32.of_bits(depth)])
        return result.st0f


def cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(800):
        fx = rng.uniform(500.0, 4500.0)
        fz = rng.uniform(500.0, 4500.0)
        fy = rng.uniform(0.0, 200.0)
        distance = rng.choice([rng.uniform(5.0, 60.0), rng.uniform(20.0, 2000.0)])
        heading = rng.uniform(-math.pi, math.pi)
        pitch = rng.choice([rng.uniform(-0.2, 1.6), rng.uniform(1.4, 1.5707963), rng.uniform(0.0, 0.1)])
        across = distance * math.cos(pitch)
        ox = fx + across * math.cos(heading)
        oz = fz + across * math.sin(heading)
        oy = fy + distance * math.sin(pitch)
        near = rng.choice([0.5, 1.0, 2.0, rng.uniform(0.1, 4.0)])
        depth = rng.choice([rng.uniform(0.0, 12.0), rng.uniform(1.0, 500.0), rng.uniform(100.0, 5000.0)])
        rows.append(tuple(f2u(v) for v in (ox, oy, oz, fx, fy, fz, near, depth)))
    # Straight above the focus, where the pitch is the fixed straight-down one
    for _ in range(20):
        fx = rng.uniform(500.0, 4500.0)
        fz = rng.uniform(500.0, 4500.0)
        depth = rng.uniform(1.0, 300.0)
        rows.append(tuple(f2u(v) for v in (fx, 300.0, fz, fx, 0.0, fz, 1.0, depth)))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x5766E0)
    with Oracle() as oracle:
        drawn = DrawnDepth(oracle)
        rows = [[*case, drawn.run(*case)] for case in cases(rng)]
        n = write_pinned(out / "world_text_depth.txt", "The depth at which the game draws a box of words over a point",
                         ["Function: cdecl (point depth), result ST0 stored as a float.",
                          "It reads the camera's origin and focus and the near plane, set before each call.",
                          "Rows: 800 random, 20 straight above the focus."],
                         DrawnDepth.COLUMNS, rows)
        print(f"world_text_depth: {n} rows")


if __name__ == "__main__":
    main()
