#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the creature's drawn pose between its last two game turns, in test/oracle/data:

- turn_pose_blend.txt: one bone (twelve numbers) of the drawn body. The game's weighted copy runs as it is, two times,
  as the creature's frame calls it: first the newer pose times the weight writes the bone, then the older pose times
  one less the weight adds to it. The older weight is one float subtraction from 1, made here: a subtraction of two
  floats rounds once to a float at each precision, thus the value is the game's value.

Usage: python tools/oracle/cases/turn_pose_cases.py [output folder]   (default: test/oracle/data)
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

# The game's code (BW1W120, runblack.exe 1.20)
WEIGHTED_COPY = 0x4EECD0  # cdecl (out, in, weight, count, overwrite): out = weight * in, or out += weight * in
NUMBERS = 12  # one bone: three axes and the place


def to_f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def main(argv: list[str]) -> int:
    folder = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x7E1B)
    rows = []
    with Oracle() as o:
        out = o.alloc(4 * NUMBERS)
        older = o.alloc(4 * NUMBERS)
        newer = o.alloc(4 * NUMBERS)
        weights = [0.0, 0.01, 0.5, 0.99, 1.0, 1.5, 2.0]
        for case in range(200):
            weight = to_f32(weights[case] if case < len(weights) else rng.choice([rng.random(), rng.randrange(100) * 0.01]))
            olders = [to_f32(rng.uniform(-3000.0, 3000.0) if rng.random() < 0.5 else rng.uniform(-1.5, 1.5))
                      for _ in range(NUMBERS)]
            newers = [to_f32(value + rng.uniform(-40.0, 40.0)) for value in olders]
            older_weight = to_f32(1.0 - weight)
            o.write(older, b"".join(struct.pack("<f", v) for v in olders))
            o.write(newer, b"".join(struct.pack("<f", v) for v in newers))
            o.write(out, bytes(4 * NUMBERS))
            o.call(WEIGHTED_COPY, args=[out, newer, f32(weight), NUMBERS, 1])
            o.call(WEIGHTED_COPY, args=[out, older, f32(older_weight), NUMBERS, 0])
            result = o.read_u32(out, NUMBERS)
            rows.append([f2u(weight)] + [f2u(v) for v in olders] + [f2u(v) for v in newers] + list(result))
    columns = (["weight"] + [f"older{i}" for i in range(NUMBERS)] + [f"newer{i}" for i in range(NUMBERS)] +
               [f"drawn{i}" for i in range(NUMBERS)])
    count = write_pinned(folder / "turn_pose_blend.txt", "The drawn bone between the poses of the last two game turns",
                         ["One bone of twelve numbers; the older weight is 1 - weight as a float."], columns, rows)
    print(f"turn_pose_blend.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
