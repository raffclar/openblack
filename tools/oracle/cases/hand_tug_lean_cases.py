#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the lean of a tree that the hand pulls, in test/oracle/data:

- hand_tug_lean.txt: one row for each frame of a pull. The game runs its whole frame of the pull on a heavy tree that
  does not come free. The inputs are the axes and the base of the tree, its spin, the grip height, the hand point, the
  frame time, the height of the tree and whether it is of the rock material. The results are the axes after the
  frame, the spin, the speed of the middle of the tree, the point of the hand on the tree and the stretch.

The rows of a pull follow each other: each row starts from the results of the row before it. The stretch is the
game's own easing, so the test takes the stretch from the data for the hand point.

The hand point comes from the cursor line and the pull plane. The script gives a line straight down from a point 64
above the plane, and the plane is level. The height of the plane is a multiple of 1 / 16384 below 448, so the game's
point on the plane is the hand point exactly.

The spin of the tree has a damping that grows with the spin. With a long frame and a far hand the damping overshoots,
and the spin grows each frame until it is not a number. The data keeps only frames that turn the tree by no more than
half a radian. For larger angles the sine and the cosine of the FPU reduce the angle in their own way.

Usage: python tools/oracle/cases/hand_tug_lean_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
TUG_UPDATE = 0x5B8070  # thiscall on the tug state, args (frame seconds, matrix), RET 8
GET_CAMERA = 0x54C180  # thiscall, RET
CURSOR_LINE = 0x74CBD0  # fastcall (ECX = hand input, EDX = the line's direction out), RET
MAX_FORCE = 0x46BFB0  # thiscall on the hand, RET, result on the FPU
HAND_HEIGHT = 0x46C040  # thiscall on the hand, RET, result on the FPU
HAND_MATRIX = 0x46E160  # RET 0xC
CLIP_SAMPLER = 0x860E00  # RET 0x20
BONE_CHAIN = 0x839F10  # cdecl, RET
MAKE_INVERSE_SQRT_TABLE = 0x8411D0  # cdecl (), fills the table through the 3D engine's allocator
ENGINE_ALLOC = 0x842CE0
CAMERA_POSITION = 0xEA1DB8  # x, y, z

# Functions that the tug never runs, which become the four virtual calls of the held tree
IS_TREE_STUB = 0x6508F0
HEIGHT_STUB = 0x4F78C0
MASS_STUB = 0x74F170
MATERIAL_STUB = 0x47C9D0
SLOT_IS_TREE = 0x338
SLOT_HEIGHT = 0x42C
SLOT_MASS = 0x638
SLOT_MATERIAL = 0x788
ROCK_MATERIAL = 3

# The tug state
STATE_HAND = 0x4
STATE_WAITS = 0xC
STATE_PLANE_NORMAL = 0xA4
STATE_PLANE_POINT = 0xB0
STATE_AXES = 0xEC  # three rows and the base, 12 floats
STATE_STRETCH = 0x11C  # the stretch easing, 12 floats
# The hand
HAND_POINT = 0x78
HAND_KIND = 0x490C
HAND_HELD = 0x4904
HAND_SPIN = 0x4920
HAND_BASE = 0x492C
HAND_SPEED = 0x4938
HAND_GRIP = 0x4944
# The held tree
OBJECT_DRAWN = 0x40

MAX_FORCE_VALUE = 600000.0
MAX_TURN = 0.5  # radians in a frame
HEAVY_MASS = 1.0e6  # its weight is more than the hand's greatest pull, so it never comes free

COLUMNS = ([f"m{i}" for i in range(12)] + ["s0", "s1", "s2", "grip", "hx", "hy", "hz", "dt", "height", "rock"]
           + [f"om{i}" for i in range(12)] + ["os0", "os1", "os2", "ov0", "ov1", "ov2", "oh0", "oh1", "oh2", "ostretch"])


def f(value: float) -> int:
    return f2u(value)


def rounded(value: float) -> float:
    return u2f(f2u(value))


class Tug:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.hook_alloc(ENGINE_ALLOC)
        o.call(MAKE_INVERSE_SQRT_TABLE)
        # The cursor line: copy the chosen direction to the output
        self.direction = o.alloc(16)
        d = self.direction
        code = (b"\xA1" + struct.pack("<I", d) + b"\x89\x02"
                + b"\xA1" + struct.pack("<I", d + 4) + b"\x89\x42\x04"
                + b"\xA1" + struct.pack("<I", d + 8) + b"\x89\x42\x08"
                + b"\xC3")
        o.write(CURSOR_LINE, code)
        o.write_f32(d, 0.0, -1.0, 0.0)
        o.stub(GET_CAMERA)
        o.stub(MAX_FORCE, st0=MAX_FORCE_VALUE)
        o.stub(HAND_HEIGHT, st0=3.2)
        o.stub(HAND_MATRIX, pop=0xC)
        o.stub(CLIP_SAMPLER, pop=0x20)
        o.stub(BONE_CHAIN)
        o.stub(IS_TREE_STUB, eax=1)
        o.stub(MASS_STUB, st0=HEAVY_MASS)
        table = o.alloc(0x800)
        for slot, address in ((SLOT_IS_TREE, IS_TREE_STUB), (SLOT_HEIGHT, HEIGHT_STUB), (SLOT_MASS, MASS_STUB),
                              (SLOT_MATERIAL, MATERIAL_STUB)):
            o.write_u32(table + slot, address)
        self.drawn = o.alloc(0x60)
        self.thing = o.alloc(0x60)
        o.write_u32(self.thing, table)
        o.write_u32(self.thing + OBJECT_DRAWN, self.drawn)
        self.hand = o.alloc(0x4A00)
        o.write_u32(self.hand + HAND_HELD, self.thing)
        o.write_u32(self.hand + HAND_KIND, 0)
        self.state = o.alloc(0x150)
        o.write_u32(self.state + STATE_HAND, self.hand)
        o.write_u32(self.state + STATE_WAITS, 0)
        o.write_f32(self.state + STATE_PLANE_NORMAL, 0.0, 1.0, 0.0)

    def start(self, axes: list[int], height: float, grip: float, rock: bool) -> None:
        """A new pull: the axes and the base as the game makes them, no spin, no stretch"""
        o = self.o
        o.write_u32(self.state + STATE_AXES, *axes)
        o.write_u32(self.hand + HAND_BASE, *axes[9:12])
        o.write_u32(self.hand + HAND_SPIN, 0, 0, 0)
        stretch = [0] * 12
        stretch[0] = stretch[1] = stretch[7] = f(1.0)
        o.write_u32(self.state + STATE_STRETCH, *stretch)
        o.write_f32(self.hand + HAND_GRIP, grip)
        o.stub(HEIGHT_STUB, st0=height)
        o.stub(MATERIAL_STUB, eax=ROCK_MATERIAL if rock else 1)
        self.height = height
        self.rock = rock

    def frame(self, hand: tuple[float, float, float], dt: float) -> list[int]:
        o = self.o
        hx, hy, hz = hand
        o.write_f32(CAMERA_POSITION, hx, hy + 64.0, hz)
        o.write_f32(self.state + STATE_PLANE_POINT, hx + 1.5, hy, hz - 2.5)
        before = (o.read_u32(self.state + STATE_AXES, 12) + o.read_u32(self.hand + HAND_SPIN, 3)
                  + o.read_u32(self.hand + HAND_GRIP, 1))
        result = o.call(TUG_UPDATE, this=self.state, args=[f32(dt), 0])
        assert result.popped == 8 and result.depth == 0, result
        after = (o.read_u32(self.state + STATE_AXES, 12) + o.read_u32(self.hand + HAND_SPIN, 3)
                 + o.read_u32(self.hand + HAND_SPEED, 3) + o.read_u32(self.hand + HAND_POINT, 3)
                 + o.read_u32(self.state + STATE_STRETCH, 1))
        return before + [f(hx), f(hy), f(hz), f(dt), f(self.height), 1 if self.rock else 0] + after


def start_axes(rng: random.Random) -> list[int]:
    """Axes as the game makes them for a standing tree: a turn about the up axis times the size, and the base"""
    angle = rounded(rng.uniform(0.0, 2.0 * math.pi))
    scale = rounded(rng.uniform(0.4, 2.5))
    c = rounded(math.cos(angle) * scale)
    s = rounded(math.sin(angle) * scale)
    base = [rounded(rng.uniform(0.0, 5000.0)), round(rng.uniform(0.0, 300.0) * 16384) / 16384,
            rounded(rng.uniform(0.0, 5000.0))]
    return [f(c), 0, f(s), 0, f(scale), 0, f(-s), 0, f(c)] + [f(v) for v in base]


def pulls(tug: Tug, rng: random.Random) -> list[list[int]]:
    rows = []
    for pull in range(40):
        axes = start_axes(rng)
        base = [u2f(b) for b in axes[9:12]]
        height = rounded(rng.uniform(4.0, 30.0))
        grip = rounded(height * rng.choice((0.1, 0.65, 0.7)))
        rock = pull % 8 == 7
        tug.start(axes, height, grip, rock)
        # The hand moves off to a side of the grip and then stays or moves on a little each frame
        heading = rng.uniform(0.0, 2.0 * math.pi)
        reach = rng.uniform(0.5, 6.0)
        plane = round((base[1] + rng.uniform(-2.0, 4.0)) * 16384) / 16384
        dt = rounded(rng.choice((0.1, 1.0 / 30.0, 0.0166, 0.05)))
        for step in range(30):
            reach += rng.uniform(-0.2, 0.4)
            hand = (rounded(base[0] + math.cos(heading) * reach), plane, rounded(base[2] + math.sin(heading) * reach))
            row = tug.frame(hand, dt)
            # The pull ends at a turn of more than half a radian in a frame (see the notes)
            spin = [u2f(row[COLUMNS.index(name)]) for name in ("os0", "os1", "os2")]
            if not all(math.isfinite(part) for part in spin) or math.sqrt(sum(p * p for p in spin)) * dt > MAX_TURN:
                break
            rows.append(row)
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261105)
    with Oracle() as oracle:
        rows = pulls(Tug(oracle), rng)
    count = write_pinned(out / "hand_tug_lean.txt", "The lean of a tree that the hand pulls",
                         ["Each row is one frame of a pull on a heavy tree. Rows of one pull follow each other.",
                          "m: the rows and the base of the axes; s: the spin; grip: the grip height; h: the hand point; "
                          "o: the results (axes, spin, speed of the middle, hand point on the tree, stretch)."],
                         COLUMNS, rows)
    print(f"hand_tug_lean.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
