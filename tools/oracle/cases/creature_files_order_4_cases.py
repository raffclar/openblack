#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the aim of a creature that throws at a thing, in test/oracle/data:

- creature_throw_aim.txt: the game's throw stage runs as it is, with the game's own functions for where a thing, a
  villager and a creature are after some seconds. The ground height is a small function that reads the map position
  (Altitude below; the C++ test has the same rule). The thing's height is a stub. The call that starts the throw is a
  small function that keeps the point and the time that it gets.

Usage: python tools/oracle/cases/creature_files_order_4_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
THROW_STAGE = 0x501130  # thiscall on the creature, the first stage of the throw at a thing
THROW_AT = 0x4811A0  # thiscall on the 3D creature (point, seconds), ret 8: the start of the throw
GET_ALTITUDE = 0x803090  # fastcall (map position)
PLACE_OF_THING = 0x63B8D0  # where a thing is after some seconds (any object)
PLACE_OF_VILLAGER = 0x63B9B0
PLACE_OF_CREATURE = 0x63BAD0
SPARE_CODE = 0x401300  # game code that none of these calls runs, used for the throw start capture
HEIGHT_STUB = 0x401200  # any function of the game, stubbed to give the thing's height
MS_PER_TURN = 0xD01A38
STATE_TABLE_FINAL = 0xDB9E84  # the villager state table: is a final state, a row each 0x114 bytes
VILLAGER_STATE = 3


def f(value: float) -> int:
    return f32(value).bits


def altitude_code() -> bytes:
    """fastcall (map position): float((x ^ z) & 0xFFFF). The C++ test has the same rule."""
    return bytes([
        0x8B, 0x01,  # mov eax, [ecx]
        0x33, 0x41, 0x04,  # xor eax, [ecx + 4]
        0x25, 0xFF, 0xFF, 0x00, 0x00,  # and eax, 0xFFFF
        0x50,  # push eax
        0xDB, 0x04, 0x24,  # fild dword [esp]
        0x58,  # pop eax
        0xC3,  # ret
    ])


def capture_code(buffer: int) -> bytes:
    """thiscall (point, seconds), ret 8: keeps the point's three floats and the seconds in the buffer, returns 1."""
    code = bytes([0x8B, 0x44, 0x24, 0x04])  # mov eax, [esp + 4]
    for i in range(3):
        code += bytes([0x8B, 0x50, 4 * i])  # mov edx, [eax + 4i]
        code += bytes([0x89, 0x15]) + struct.pack("<I", buffer + 4 * i)  # mov [buffer + 4i], edx
    code += bytes([0x8B, 0x54, 0x24, 0x08])  # mov edx, [esp + 8]
    code += bytes([0x89, 0x15]) + struct.pack("<I", buffer + 12)  # mov [buffer + 12], edx
    code += bytes([0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x08, 0x00])  # mov eax, 1; ret 8
    return code


def jump(source: int, target: int) -> bytes:
    return bytes([0xE9]) + struct.pack("<i", target - (source + 5))


def aim_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    buffer = o.alloc(16)
    o.write(SPARE_CODE, capture_code(buffer))
    o.write(THROW_AT, jump(THROW_AT, SPARE_CODE))
    o.write(GET_ALTITUDE, altitude_code())
    o.write_u32(MS_PER_TURN, 100)

    thrower3d = o.alloc(0x5000)
    thrower_thing = o.object(0x100, {0x58: thrower3d})
    target_slot = o.alloc(0x40)
    plan = o.alloc(0x1000)
    o.write_u32(plan + 0xFB4, 0)
    o.write_u32(plan + 0xFA8 + 0x34, target_slot)
    mind = o.object(0x1200, {0x160: thrower_thing, 0x164: plan})

    vtable = o.alloc(0x800)
    o.write_u32(vtable + 0x42C, HEIGHT_STUB)
    target3d = o.alloc(0x5000)
    target_thing = o.object(0x100, {0x58: target3d})
    target = o.alloc(0x200)

    rows = []
    for case in range(1500):
        kind = case % 3
        cx, cz = pick.uniform(100.0, 5000.0), pick.uniform(100.0, 5000.0)
        thrower = [cx, pick.uniform(0.0, 200.0), cz]
        reach = pick.choice([pick.uniform(0.0, 5.0), pick.uniform(5.0, 60.0), pick.uniform(60.0, 400.0)])
        angle = pick.uniform(-math.pi, math.pi)
        map_x = int((cx + reach * math.cos(angle)) * 6553.6) & 0x7FFFFFFF
        map_z = int((cz + reach * math.sin(angle)) * 6553.6) & 0x7FFFFFFF
        altitude = 0.0 if pick.random() < 0.6 else pick.uniform(0.0, 30.0)
        height = pick.choice([0.0, pick.uniform(0.5, 40.0)])
        step = [pick.randrange(-30000, 30000), pick.randrange(-30000, 30000)]
        final = 1 if pick.random() < 0.25 else 0
        position = [cx + reach * math.cos(angle) + pick.uniform(-0.01, 0.01), pick.uniform(0.0, 200.0),
                    cz + reach * math.sin(angle) + pick.uniform(-0.01, 0.01)]
        heading = pick.uniform(-4.0, 4.0)
        speed = pick.choice([0.0, pick.uniform(0.0, 40.0)])
        size = pick.uniform(0.3, 3.0)

        o.write_f32(thrower3d + 0x78, *[f32(v) for v in thrower])
        o.write_u32(thrower3d + 0x4994, 0)
        o.write_u32(thrower_thing + 0x28, 1)
        place = (PLACE_OF_THING, PLACE_OF_VILLAGER, PLACE_OF_CREATURE)[kind]
        o.write_u32(vtable + 0x414, place)
        o.write(target, bytes(0x200))
        o.write_u32(target, vtable)
        o.write_u32(target + 0x14, map_x, map_z)
        o.write_f32(target + 0x1C, f32(altitude))
        o.write_u32(target_slot + 0x30, target)
        o.stub(HEIGHT_STUB, st0=f32(height))
        if kind == 1:
            o.write(target + 0x8C, bytes([VILLAGER_STATE]))
            o.write_u32(target + 0x64, step[0] & 0xFFFFFFFF)
            o.write_u32(target + 0x6C, step[1] & 0xFFFFFFFF)
            o.write_u32(STATE_TABLE_FINAL + VILLAGER_STATE * 0x114, final)
        if kind == 2:
            o.write_u32(target + 0x160, target_thing)
            o.write_f32(target3d + 0x78, *[f32(v) for v in position])
            o.write_f32(target3d + 0x84, f32(heading))
            o.write_f32(target3d + 0x90, f32(size))
            o.write_f32(target3d + 0x4838, f32(speed))
        o.write_u32(buffer, 0, 0, 0, 0)
        r = o.call(THROW_STAGE, this=mind)
        if r.eax != 2:
            raise RuntimeError(f"the throw stage gave {r.eax} on case {case}")
        rows.append([kind] + [f(v) for v in thrower] + [map_x, map_z, f(altitude), f(height), step[0], step[1], final]
                    + [f(v) for v in position] + [f(heading), f(speed), f(size)] + o.read_u32(buffer, 4))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x5EED_0726)
    with Oracle() as o:
        aim = aim_rows(o, pick)
    count = write_pinned(
        out / "creature_throw_aim.txt",
        "Where a creature throws at a thing, and how long the throw takes",
        [
            "kind: 0 a thing, 1 a villager, 2 a creature. thrower_*: the creature that throws. map_x, map_z: where the",
            "target stands. altitude: its height over the ground. height: the height of a thing or a villager. step_*,",
            "final: a villager's step and whether its state is a final state. pos_*, heading, speed, size: a creature",
            "target. aim_*, seconds: the point and the time that start the throw. The ground: Altitude in the script.",
        ],
        ["kind", "thrower_x", "thrower_y", "thrower_z", "map_x", "map_z", "altitude", "height", "step_x", "step_z",
         "final", "pos_x", "pos_y", "pos_z", "heading", "speed", "size", "aim_x", "aim_y", "aim_z", "seconds"],
        aim,
    )
    print(f"{count} aim rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
