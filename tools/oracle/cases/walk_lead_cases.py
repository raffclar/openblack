#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of how a creature aims at a villager or an animal that moves, in test/oracle/data:

- creature_walk_lead.txt: the game's reach test for a pick-up runs as it is. The thing is out of reach (the two reach
  lengths and the corner test are stubs), so the test gives the place to walk to. The game's own functions give the
  place of the living thing after a time, its angle and the distances. The ground height is a stub that gives 0.
- creature_reach_lead.txt: the game's start of the 3D pick-up and stomp runs as it is. The corner test is a small
  function that keeps the point that it gets: the place where the hand or foot reaches for the thing.
- creature_moving_reach.txt: the game's reach of a creature on its way, from the four corners and the drawn scale.

Usage: python tools/oracle/cases/walk_lead_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
REACH_TEST = 0x485F40  # thiscall on the 3D creature (thing, out point, kind), ret 0xC
REACH_START = 0x480E10  # thiscall on the 3D creature (thing, kind), ret 8
MOVING_REACH = 0x485EE0  # thiscall on the 3D creature (kind), ret 4
STRAIGHT_REACH = 0x485DF0  # thiscall (kind), ret 4
CORNER_TEST = 0x486160  # thiscall (point, kind, out angle), ret 0xC
DYNAMIC_CAST = 0x7C5A19  # cdecl
GET_ALTITUDE = 0x803090  # fastcall (map position)
PREPARE_PICK_UP = 0x4EE130  # thiscall, no arguments
PREPARE_STOMP = 0x4EE530
HAS_ANIMATION = 0x619650  # thiscall (clip, 0), ret 8
IS_BUSY = 0x4842D0  # thiscall
SET_STATE = 0x484EC0  # thiscall (state), ret 4
IS_MOVING_STUB = 0x401200  # any function of the game, stubbed to give whether the living thing moved
SPARE_CODE = 0x401300  # game code that none of these calls runs, used for the corner test capture

PICK_UP = 0xE
STOMP = 0x10


def f(value: float) -> int:
    return f32(value).bits


def capture_code(buffer: int) -> bytes:
    """thiscall (point, kind, out), ret 0xC: keeps the point's three floats in the buffer, returns 1."""
    code = bytes([0x8B, 0x44, 0x24, 0x04])  # mov eax, [esp + 4]
    for i in range(3):
        code += bytes([0x8B, 0x50, 4 * i])  # mov edx, [eax + 4i]
        code += bytes([0x89, 0x15]) + struct.pack("<I", buffer + 4 * i)  # mov [buffer + 4i], edx
    code += bytes([0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x0C, 0x00])  # mov eax, 1; ret 0xC
    return code


def living_inputs(pick: random.Random, creature_x: float, creature_z: float) -> dict:
    distance = pick.choice([pick.uniform(0.0, 3.0), pick.uniform(3.0, 40.0), pick.uniform(40.0, 300.0)])
    offset_x = pick.uniform(-1.0, 1.0) * distance
    offset_z = pick.uniform(-1.0, 1.0) * distance
    return {
        "map_x": int((creature_x + offset_x) * 6553.6) & 0x7FFFFFFF,
        "map_z": int((creature_z + offset_z) * 6553.6) & 0x7FFFFFFF,
        "offset": 0.0 if pick.random() < 0.6 else pick.uniform(0.0, 5.0),
        "speed": pick.choice([0, pick.randrange(0, 80), pick.randrange(0, 8000), pick.randrange(0, 65536)]),
        "angle": pick.randrange(0, 65536),
        "moved": 0 if pick.random() < 0.15 else 1,
    }


def place_living(o: Oracle, living: int, vtable: int, inputs: dict) -> None:
    o.write(living, bytes(0x100))
    o.write_u32(living, vtable)
    o.write_u32(living + 0x14, inputs["map_x"], inputs["map_z"])
    o.write_f32(living + 0x1C, f32(inputs["offset"]))
    o.write(living + 0x5A, struct.pack("<HH", inputs["speed"], inputs["angle"]))
    o.stub(IS_MOVING_STUB, eax=inputs["moved"])


def walk_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    o.stub(PREPARE_PICK_UP)
    o.stub(STRAIGHT_REACH, pop=4, st0=f32(0.0))
    o.stub(MOVING_REACH, pop=4, st0=f32(0.0))
    o.stub(CORNER_TEST, eax=0, pop=0xC)
    o.stub(GET_ALTITUDE, st0=f32(0.0))
    vtable = o.alloc(0x800)
    o.write_u32(vtable + 0x174, IS_MOVING_STUB)
    living = o.alloc(0x100)
    o.stub(DYNAMIC_CAST, eax=living)
    creature = o.alloc(0x6000)
    out = o.alloc(16)

    rows = []
    for case in range(1500):
        position = [pick.uniform(100.0, 5000.0), pick.uniform(0.0, 100.0), pick.uniform(100.0, 5000.0)]
        size = pick.uniform(0.05, 4.0)
        run = (size * 0.5 * 1.75 + 0.25) * 20.0
        fraction = pick.choice([pick.uniform(0.05, 1.0), 0.4, 1.0])
        moving = case % 2
        inputs = living_inputs(pick, position[0], position[2])
        place_living(o, living, vtable, inputs)
        o.write(creature, bytes(0x6000))
        o.write_f32(creature + 0x78, *[f32(v) for v in position])
        o.write_f32(creature + 0x90, f32(size))
        o.write_f32(creature + 0x483C, f32(fraction))
        o.write_f32(creature + 0x4848, f32(run))
        o.write_f32(creature + 0x484C, f32(12.0))
        o.write_u32(creature + 0x4994, moving)
        o.write_u32(out, 0, 0, 0)
        r = o.call(REACH_TEST, this=creature, args=[living, out, PICK_UP])
        if r.eax != 1:
            raise RuntimeError(f"the reach test gave {r.eax} on case {case}")
        rows.append([f(v) for v in position] + [f(size), f(run), f(fraction), moving, inputs["map_x"],
                                                  inputs["map_z"], f(inputs["offset"]), inputs["speed"],
                                                  inputs["angle"], inputs["moved"]] + o.read_u32(out, 3))
    return rows


def reach_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    o.reset()
    buffer = o.alloc(16)
    o.write(SPARE_CODE, capture_code(buffer))
    o.write(CORNER_TEST, bytes([0xE9]) + struct.pack("<i", SPARE_CODE - (CORNER_TEST + 5)))
    o.stub(PREPARE_PICK_UP)
    o.stub(PREPARE_STOMP)
    o.stub(HAS_ANIMATION, eax=1, pop=8)
    o.stub(IS_BUSY, eax=0)
    o.stub(SET_STATE, pop=4)
    o.stub(GET_ALTITUDE, st0=f32(0.0))
    vtable = o.alloc(0x800)
    o.write_u32(vtable + 0x174, IS_MOVING_STUB)
    living = o.alloc(0x100)
    o.stub(DYNAMIC_CAST, eax=living)
    creature = o.alloc(0x6000)

    rows = []
    for case in range(1500):
        kind = PICK_UP if case % 2 == 0 else STOMP
        size = pick.choice([pick.uniform(0.05, 4.0), 1.0, 0.5])
        ms = pick.choice([pick.randrange(0, 3000), pick.randrange(-500, 0), 800])
        inputs = living_inputs(pick, 2000.0, 2000.0)
        place_living(o, living, vtable, inputs)
        o.write(creature, bytes(0x6000))
        o.write_f32(creature + 0x78, f32(2000.0), f32(10.0), f32(2000.0))
        o.write_f32(creature + 0x90, f32(size))
        o.write_u32(creature + (0x4A34 if kind == PICK_UP else 0x4A84), ms & 0xFFFFFFFF)
        o.write_u32(buffer, 0, 0, 0)
        r = o.call(REACH_START, this=creature, args=[living, kind])
        if r.eax != 1:
            raise RuntimeError(f"the reach start gave {r.eax} on case {case}")
        rows.append([kind, f(size), ms & 0xFFFFFFFF, inputs["map_x"], inputs["map_z"], f(inputs["offset"]),
                     inputs["speed"], inputs["angle"], inputs["moved"]] + o.read_u32(buffer, 3))
    return rows


def moving_reach_rows(o: Oracle, pick: random.Random) -> list[list[int]]:
    o.reset()
    o.stub(PREPARE_PICK_UP)
    o.stub(PREPARE_STOMP)
    creature = o.alloc(0x6000)
    rows = []
    for case in range(1500):
        kind = PICK_UP if case % 2 == 0 else STOMP
        corners = [pick.uniform(-30.0, 30.0) for _ in range(12)]
        scale = pick.choice([pick.uniform(0.01, 4.0), 1.0])
        o.write_f32(creature + 0x94, f32(scale))
        o.write_f32(creature + (0x49C8 if kind == PICK_UP else 0x4A44), *[f32(v) for v in corners])
        r = o.call(MOVING_REACH, this=creature, args=[kind])
        rows.append([kind, f(scale)] + [f(v) for v in corners] + [r.st0f])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0x5EED_0100)
    with Oracle() as o:
        walk = walk_rows(o, pick)
        reach = reach_rows(o, pick)
        moving = moving_reach_rows(o, pick)
    living_notes = [
        "map_x, map_z: where the living thing is (16.16). offset: its height over the ground (the ground is 0).",
        "speed, angle: the speed and the game angle that it keeps. moved: whether its last turn moved it.",
    ]
    count = write_pinned(
        out / "creature_walk_lead.txt",
        "Where a creature walks to, to get a villager or an animal in reach",
        ["pos_*: the creature. size, run, fraction: its size, top speed and the fraction of it.",
         "moving: 1 when it is on its way."] + living_notes + ["aim_*: the place that it walks to."],
        ["pos_x", "pos_y", "pos_z", "size", "run", "fraction", "moving", "map_x", "map_z", "offset", "speed", "angle",
         "moved", "aim_x", "aim_y", "aim_z"],
        walk,
    )
    print(f"{count} walk rows")
    count = write_pinned(
        out / "creature_reach_lead.txt",
        "Where a creature reaches for a villager or an animal, when its pick-up or stomp starts",
        ["kind: 14 a pick-up, 16 a stomp. size: the creature's size. ms: the milliseconds of the reach.",
         "The creature stands at (2000, 10, 2000)."] + living_notes + ["aim_*: the place that it reaches for."],
        ["kind", "size", "ms", "map_x", "map_z", "offset", "speed", "angle", "moved", "aim_x", "aim_y", "aim_z"],
        reach,
    )
    print(f"{count} reach rows")
    count = write_pinned(
        out / "creature_moving_reach.txt",
        "How far ahead the middle of the four corners of a reach is, for a creature on its way",
        ["kind: 14 a pick-up, 16 a stomp. scale: the drawn scale. c*: the four corners (x, y, z each), in corner",
         "order. reach: the result."],
        ["kind", "scale"] + [f"c{i}{a}" for i in range(4) for a in "xyz"] + ["reach"],
        moving,
    )
    print(f"{count} moving reach rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
