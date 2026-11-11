#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the maths of the creature's decision trees, in test/oracle/data:

- tree_share_log2.txt: the base 2 logarithm of a share of the examples, as the game finds it (the base 10 logarithm
  times its constant). Every share count / total for totals 1 to 128.
- tree_entropy.txt: how mixed the feedback of 1 to 16 examples is (sign part twice, step part once, times a third).
- tree_leaf_step.txt: the step of the feedback scale of a node, from the mean feedback of its examples; 0xFFFFFFFF
  when the mean is further than a quarter from each step.

The examples of a row are in openblack's order, the oldest first. The game keeps the newest first in its list, so the
script links them in that order.

Usage: python tools/oracle/cases/creature_tree_maths_cases.py [output folder]   (default: test/oracle/data)
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

# The functions (BW1W120, runblack.exe 1.20)
SHARE_LOG2 = 0x4D6EA0  # cdecl (float share), returns ST0
ENTROPY = 0x4D6CD0  # cdecl (example list), returns ST0
LEAF_STEP = 0x4D6700  # thiscall on the node, returns EAX
FEEDBACK = 0xC  # the feedback of an example
MAX_EXAMPLES = 16
STEPS = (-1.0, -0.8, -0.6, -0.4, -0.2, 0.0, 0.2, 0.4, 0.6, 0.8, 1.0)


def to_float(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def feedback_sets(rng: random.Random) -> list[list[float]]:
    sets = [[1.0], [-1.0], [0.0], [-0.0], [1.0, -1.0], [0.8, 0.8], [0.5] * 16, [1.3, -1.3], [1.25, -1.25, 0.15]]
    for _ in range(3000):
        count = rng.randint(1, MAX_EXAMPLES)
        values = []
        for _ in range(count):
            pick = rng.random()
            if pick < 0.4:
                values.append(rng.choice(STEPS))
            elif pick < 0.5:
                values.append(rng.choice(STEPS) + rng.choice((-0.25, 0.25)))
            elif pick < 0.55:
                values.append(rng.uniform(-1.5, 1.5))
            else:
                values.append(rng.uniform(-1.0, 1.0))
        sets.append([to_float(v) for v in values])
    return sets


def example_list(o: Oracle, feedback: list[float]) -> int:
    """The list {head, count}; the last (newest) example is the head"""
    head = 0
    for value in feedback:
        example = o.alloc(0x20)
        o.write_f32(example + FEEDBACK, value)
        link = o.alloc(8)
        o.write_u32(link, head)
        o.write_u32(link + 4, example)
        head = link
    node = o.alloc(0x94)
    o.write_u32(node, head)
    o.write_u32(node + 4, len(feedback))
    return node


def row_of(feedback: list[float], result: int) -> list[int]:
    words = [f2u(v) for v in feedback] + [0] * (MAX_EXAMPLES - len(feedback))
    return [len(feedback)] + words + [result]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(1680)
    sets = feedback_sets(rng)
    columns = ["count"] + [f"f{i}" for i in range(MAX_EXAMPLES)]
    with Oracle() as o:
        logs = []
        for total in range(1, 129):
            for count in range(1, total + 1):
                share = to_float(to_float(count) / to_float(total))
                result = o.call(SHARE_LOG2, args=[f32.of_bits(f2u(share))])
                logs.append([f2u(share), result.st0f])
        entropies = []
        steps = []
        for feedback in sets:
            o.reset()
            node = example_list(o, feedback)
            entropies.append(row_of(feedback, o.call(ENTROPY, args=[node]).st0f))
            steps.append(row_of(feedback, o.call(LEAF_STEP, this=node).eax))
    write_pinned(out / "tree_share_log2.txt", "The game's base 2 logarithm of a share of a tree's examples",
                 ["Each share count / total for totals 1 to 128, as floats"], ["share", "log2"], logs)
    write_pinned(out / "tree_entropy.txt", "The entropy of the feedback of a tree node's examples",
                 ["count examples, oldest first; unused columns 0"], columns + ["entropy"], entropies)
    write_pinned(out / "tree_leaf_step.txt", "The step of the feedback scale of a tree node, from its mean feedback",
                 ["count examples, oldest first; step 0xFFFFFFFF when no step is within a quarter of the mean"],
                 columns + ["step"], steps)


if __name__ == "__main__":
    main()
