#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of two creature rules, in test/oracle/data:

- creature_breath_period.txt: the breath period that the body sets when it stops moving, for a size. The game's stop
  runs as it is, on a body that does not walk.
- creature_gesture_target.txt: the point that the hand of a creature that draws a gesture goes to, for a heading, a
  radius, a head point and a point of the shape. The game's function runs as it is. The radius is a stub of the
  creature's radius call, and the shape has one point, which the sample at the end of the shape gives.

Usage: python tools/oracle/cases/creature_files_order_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
STOP_MOVING = 0x484260  # thiscall on the 3D creature, ret 0
GESTURE_TARGET = 0x47A3B0  # thiscall on the creature (gesture, share, out), ret 0xC
RADIUS_STUB = 0x401200  # any function of the game, stubbed to give the radius as the creature's vt+0x64
TEMPLATES = 0xD4E760  # the gesture templates, one pointer for each gesture

# The 3D creature's fields
SIZE = 0x90
HEADING = 0x84
BREATH_TARGET = 0x4990
BODY_STATE = 0x4994  # 0: it does not walk, so the stop does not stop a walk
BONES = 0x5178  # the posed bone buffer
HEAD_SLOT = 0x51B8


def f(value: float) -> int:
    return f32(value).bits


def breath_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    rows = []
    sizes = [0.05, 0.1, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0] + [pick.uniform(0.05, 4.0) for _ in range(392)]
    body = o.object(0x5400, {BODY_STATE: 0})
    for size in sizes:
        o.write_f32(body + SIZE, f32(size))
        o.call(STOP_MOVING, this=body)
        rows.append([f(size), o.read_f32_bits(body + BREATH_TARGET)[0]])
    return rows


def gesture_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    rows = []
    vtable = o.alloc(0x100)
    o.write_u32(vtable + 0x64, RADIUS_STUB)
    points = o.alloc(16)
    template = o.object(16, {0: points, 4: 1})
    o.write_u32(TEMPLATES, template)
    bones = o.alloc(0x100)
    out = o.alloc(16)
    body3d = o.object(0x5400, {BONES: bones, HEAD_SLOT: 0})
    thing = o.object(0x100, {0x58: body3d})
    creature = o.object(0x200, {0: vtable, 0x160: thing})
    for case in range(500):
        heading = pick.uniform(-math.pi, math.pi) if case else 0.0
        radius = pick.uniform(0.5, 12.0)
        head = [pick.uniform(-500.0, 3000.0), pick.uniform(0.0, 200.0), pick.uniform(-500.0, 3000.0)]
        shape = [pick.uniform(-1.0, 1.0), pick.uniform(-1.0, 1.0), pick.uniform(0.0, 1.0)]
        o.write_f32(points, *[f32(v) for v in shape])
        o.write_f32(bones + 0x24, *[f32(v) for v in head])
        o.write_f32(body3d + HEADING, f32(heading))
        o.stub(RADIUS_STUB, st0=f32(radius))
        o.call(GESTURE_TARGET, this=creature, args=[0, f32(1.0), out])
        rows.append([f(heading), f(radius)] + [f(v) for v in head] + [f(v) for v in shape] +
                    o.read_f32_bits(out, 3))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x5EED_0722)
    with Oracle() as o:
        breath = breath_rows(o, pick)
        gesture = gesture_rows(o, pick)
    count = write_pinned(
        out / "creature_breath_period.txt",
        "The breath period that a creature's body sets when it stops moving",
        ["size: the body's size. period: the breath period that the stop sets, in seconds."],
        ["size", "period"],
        breath,
    )
    print(f"{count} breath rows")
    count = write_pinned(
        out / "creature_gesture_target.txt",
        "The point that the hand of a creature that draws a gesture goes to",
        [
            "heading: the creature's heading in radians. radius: its radius. head_*: the posed head point.",
            "shape_*: the point of the gesture's shape. x goes across, z from the top down. target_*: the hand's point.",
        ],
        ["heading", "radius", "head_x", "head_y", "head_z", "shape_x", "shape_y", "shape_z", "target_x", "target_y",
         "target_z"],
        gesture,
    )
    print(f"{count} gesture rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
