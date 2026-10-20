#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of push scrolling in test/oracle/data:

- push_scroll_push.txt: the world camera's key handling, its push part: the speed across, forwards and round after a
  frame's push, from the frame's milliseconds, the mouse's movement, the pointer, where the pointer would be beyond the
  screen's sides, the heights of the camera and of its focus, and the speed before. The screen is the game's default
  800x600 in full screen; push scrolling is on and the land is not gripped.
- push_scroll_strafe.txt: the world camera's strafe: the camera, its focus and its turning point moved, the heading
  turned and whether it turned, from the speed, the frame's seconds, the heading, the pitch and the camera's features.

Usage: python tools/oracle/cases/push_scroll_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
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

# The functions (BW1W120, runblack.exe 1.20)
KEY_MOVEMENT = 0x458220  # thiscall (mode), (key) RET 4
STRAFE = 0x45A080  # thiscall (mode), (pos, focus, heading, pitch, seconds, flags) RET 0x18
CAMERA_TIME = 0x555820  # thiscall (game) -> milliseconds
ACTION_HELD = 0x470AF0  # thiscall (control map), (action) RET 4
ROTATE_MODIFIER = 0x457F30  # thiscall (mode) -> bool
HELP_CALLBACK = 0x449140  # cdecl
SMOOTH_TURN = 0x454900  # cdecl
CLICK_PARAMS = 0x459610  # thiscall, RET 0xC

# The globals
GAME = 0xD0195C
CONTROL_MAP_FIELD = 0x250300
LAND_GRIP = 0xC5B0F0
PUSH_ON = 0xC5E154
FEATURES = 0x9CDD6C
SCREEN_WIDTH = 0xE85058
SCREEN_HEIGHT = 0xE8505A
WINDOWED = 0xE850B4
MODE_VTABLE = 0x8C7BFC

# The mode's fields
CAMERA = 0x04
FIGHT = 0x44
SPEED = 0x68
DELTA = 0xA8
POINTER = 0xB0
BEYOND = 0xC0
TURN_POINT = 0x12C
HAND_STATUS = 0x1AC
# The camera's fields: the current heights of its focus and of itself
FOCUS_HEIGHT = 0xB8
CAMERA_HEIGHT = 0x148


def bits(value: float) -> int:
    return f2u(value)


def s32(value: int) -> int:
    return value & 0xFFFFFFFF


def setup(o: Oracle) -> None:
    o.stub(ACTION_HELD, eax=0, pop=4)
    o.stub(ROTATE_MODIFIER, eax=0, pop=0)
    o.stub(HELP_CALLBACK, eax=0, pop=0)
    o.stub(SMOOTH_TURN, eax=0, pop=0)
    o.stub(CLICK_PARAMS, eax=0, pop=0xC)
    control_map = o.alloc(0x100)
    o.write_u32(GAME, (control_map - CONTROL_MAP_FIELD) & 0xFFFFFFFF)
    o.write_u32(LAND_GRIP, 0)
    o.write_u32(PUSH_ON, 1)
    o.write_u32(FEATURES, 0x1BF)
    o.write(SCREEN_WIDTH, struct.pack("<HH", 800, 600))
    o.write_u32(WINDOWED, 0)


def push_inputs(rng: random.Random) -> list[tuple]:
    cases = []
    # ms, dx, dy, cursor x, y, beyond x, y, camera height, focus height, speed x, y, z
    edges = [(-99, None), (-100, None), (-98, None), (898, None), (897, None), (899, None)]
    for _ in range(1500):
        side = rng.randrange(4)
        ms = rng.choice([0, 1, 5, 10, 16, 17, 33, 50, 100, rng.randrange(0, 200)])
        dx = rng.randrange(-40, 41)
        dy = rng.randrange(-40, 41)
        cx = rng.randrange(0, 800)
        cy = rng.randrange(0, 600)
        bx = cx
        by = cy
        if side == 0:
            cx = 0
            bx = rng.randrange(-400, 1)
        elif side == 1:
            cx = 799
            bx = rng.randrange(799, 1300)
        elif side == 2:
            cy = 0
            by = rng.randrange(-400, 1)
        else:
            cy = 599
            by = rng.randrange(599, 1100)
        camera = rng.uniform(-50.0, 900.0)
        focus = rng.uniform(-50.0, 300.0)
        speed = [rng.uniform(-300.0, 300.0) if rng.random() < 0.5 else 0.0 for _ in range(3)]
        cases.append((ms, dx, dy, cx, cy, bx, by, camera, focus, *speed))
    # The edges, the middle rows and columns, and the speed limits
    for beyond, _ in edges:
        for d in (-3, -1, 0, 1, 3):
            cases.append((16, d, 0, 0 if beyond < 0 else 799, rng.randrange(600), beyond, 300, 120.0, 20.0, 0.0, 0.0, 0.0))
            cases.append((16, 0, d, rng.randrange(800), 0 if beyond < 0 else 599, 400, beyond - 299 if beyond > 0 else beyond,
                          120.0, 20.0, 0.0, 0.0, 0.0))
    for row in (0, 298, 299, 300, 599):
        cases.append((10, -5, 0, 0, row, -150, row, 500.0, 0.0, 1.0, 2.0, 3.0))
    for column in (0, 398, 399, 400, 799):
        for row in (0, 599):
            cases.append((10, 0, 5 if row else -5, column, row, column, 800 if row else -200, 500.0, 0.0, 1.0, 2.0, 3.0))
    for camera, focus in ((0.0, 20.0), (0.0, 80.0), (0.0, 100.0), (20.0, 0.0), (21.0, 1.0), (700.0, 33.4), (666.7, 0.0),
                          (10.0, 30.0), (0.0, 1000.0)):
        cases.append((16, -20, 0, 0, 100, -300, 100, camera, focus, 0.0, 0.0, 0.0))
    return cases


def run_push(o: Oracle, cases: list[tuple]) -> list[list[int]]:
    rows = []
    camera = o.alloc(0x200)
    mode = o.alloc(0x400)
    for ms, dx, dy, cx, cy, bx, by, height, focus, sx, sy, sz in cases:
        o.write(mode, bytes(0x400))
        o.write_u32(mode + CAMERA, camera)
        o.write_f32(camera + CAMERA_HEIGHT, height)
        o.write_f32(camera + FOCUS_HEIGHT, focus)
        o.write_u32(mode + DELTA, s32(dx), s32(dy))
        o.write_u32(mode + POINTER, s32(cx), s32(cy))
        o.write_u32(mode + BEYOND, s32(bx), s32(by))
        o.write_f32(mode + SPEED, sx, sy, sz)
        o.stub(CAMERA_TIME, eax=ms, pop=0)
        o.call(KEY_MOVEMENT, this=mode, args=[0])
        out = o.read_u32(mode + SPEED, 3)
        rows.append([ms, s32(dx), s32(dy), s32(cx), s32(cy), s32(bx), s32(by), bits(height), bits(focus), bits(sx),
                     bits(sy), bits(sz), *out])
    return rows


def strafe_inputs(rng: random.Random) -> list[tuple]:
    cases = []
    for _ in range(2000):
        speed = [rng.uniform(-400.0, 400.0) if rng.random() < 0.7 else 0.0 for _ in range(3)]
        seconds = float(f32(rng.choice([0.0, 0.001, 0.01, 0.016, 0.017, 0.033, 0.1, rng.uniform(0.0, 0.1)])).value) \
            if hasattr(f32, "value") else rng.choice([0.0, 0.001, 0.01, 0.016, 0.017, 0.033, 0.1])
        heading = rng.uniform(-7.0, 7.0)
        pitch = rng.uniform(-0.6, 1.5)
        features = rng.choice([0x1BF, 0x1BF, 0x1BF, 0x0BF, 0x1B7, 0x1BD, 0x1B5])
        cases.append((*speed, seconds, heading, pitch, features))
    # Slopes at the limits, the stop square, and small turns about the counted hundredth
    for pitch in (math.atan(0.2), math.atan(0.2) + 1e-7, math.atan(2.0), math.atan(2.0) - 1e-7, 0.0, -0.3):
        cases.append((10.0, 20.0, 0.0, 0.016, 1.0, pitch, 0x1BF))
    for z in (0.5, 0.625, 0.626, -0.625, -0.626, 1.0):
        cases.append((0.0, 0.0, z, 0.016, 0.3, 0.6, 0x1BF))
    for x in (0.05, 0.1, 0.0999, 0.1001):
        cases.append((x, 0.0, 0.0, 0.016, 0.3, 0.6, 0x1BF))
    return cases


def run_strafe(o: Oracle, cases: list[tuple]) -> list[list[int]]:
    rows = []
    mode = o.alloc(0x400)
    pos = o.alloc(16)
    focus = o.alloc(16)
    heading_at = o.alloc(16)
    pitch_at = o.alloc(16)
    rng = random.Random(7)
    for sx, sy, sz, seconds, heading, pitch, features in cases:
        o.write(mode, bytes(0x400))
        o.write_u32(mode, MODE_VTABLE)
        o.write_u32(mode + FIGHT, 0)
        o.write_u32(FEATURES, features)
        o.write_f32(mode + SPEED, sx, sy, sz)
        start = [rng.uniform(0.0, 5000.0), rng.uniform(0.0, 400.0), rng.uniform(0.0, 5000.0)]
        o.write_f32(pos, *start)
        o.write_f32(focus, *start)
        o.write_f32(mode + TURN_POINT, *start)
        o.write_f32(heading_at, heading)
        o.write_f32(pitch_at, pitch)
        r = o.call(STRAFE, this=mode, args=[pos, focus, heading_at, pitch_at, f32(seconds), 0])
        out_pos = o.read_u32(pos, 3)
        out_focus = o.read_u32(focus, 3)
        out_turn = o.read_u32(mode + TURN_POINT, 3)
        out_heading = o.read_u32(heading_at)[0]
        out_speed = o.read_u32(mode + SPEED, 3)
        rows.append([bits(sx), bits(sy), bits(sz), bits(seconds), bits(heading), bits(pitch), features,
                     *[bits(v) for v in start], *out_pos, *out_focus, *out_turn, out_heading, *out_speed, r.eax & 0xFF])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261020)
    with Oracle() as o:
        setup(o)
        push_rows = run_push(o, push_inputs(rng))
        strafe_rows = run_strafe(o, strafe_inputs(rng))
    write_pinned(out / "push_scroll_push.txt", "Push scrolling: the speed after a frame's push (800x600, full screen)",
                 ["The world camera's key handling with no key held; push scrolling on, the land not gripped.",
                  "Integers are two's complement words; heights and speeds are float bits."],
                 ["ms", "dx", "dy", "x", "y", "beyond_x", "beyond_y", "camera_height", "focus_height", "speed_x",
                  "speed_y", "speed_z", "out_x", "out_y", "out_z"], push_rows)
    write_pinned(out / "push_scroll_strafe.txt", "Push scrolling: the strafe of a frame",
                 ["The camera, its focus and its turning point start at the same place; float bits throughout.",
                  "turned is the low byte of the result: the speed round was not zero."],
                 ["speed_x", "speed_y", "speed_z", "seconds", "heading", "pitch", "features", "start_x", "start_y",
                  "start_z", "pos_x", "pos_y", "pos_z", "focus_x", "focus_y", "focus_z", "turn_x", "turn_y", "turn_z",
                  "out_heading", "out_speed_x", "out_speed_y", "out_speed_z", "turned"], strafe_rows)
    print(f"push {len(push_rows)} rows, strafe {len(strafe_rows)} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
