#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the creature's sway and the particle ribbon's corners, in test/oracle/data:

- creature_sway_spring.txt: one game turn of the sway's spring on the two parts of the body. The spring is part of
  the 3D creature's turn; it runs as it is from an entry routine that makes the turn's frame, and a jump at its end
  returns. The creature's mass comes from a stub. The leash push that the game adds is zero, as it is in play.
- creature_sway_kick.txt: a kick of the sway by a force at a point. The game's kick function runs as it is; the land
  height under the point and the creature's mass come from stubs.
- ribbon_corners.txt: the corners of a ribbon of four joints as seen from an eye. The corner part of the game's ribbon
  draw runs as it is from an entry routine, with the 3D engine's inverse square root and its table; a jump returns
  before the scroll and the draw.
- cast_heading.txt: the heading from a creature's hands to the point it casts at, made one long. The end of the
  creature's spell update runs as it is from an entry routine, with the target and the hands in the process info.
- flat_heading.txt: the heading that a particle effect keeps from its process info. The part of the effect manager's
  step that flattens the heading runs as it is from an entry routine; a jump returns after it.

Usage: python tools/oracle/cases/sway_ribbon_cases.py [output folder]   (default: test/oracle/data)
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
GET_MASS = 0x47FA80  # thiscall (creature) -> ST0
KICK = 0x47FAC0  # thiscall (creature), args (point, force), RET 8
SPRING_START = 0x482208  # in the creature's turn: the spring, while the sway is on
SPRING_END = 0x48248E  # after the spring
TURN_EPILOGUE = 0x48249D  # pop edi, esi, ebp, ebx; add esp, 0xA8; ret 4
CORNERS_START = 0x67B854  # in the ribbon draw: the sides, the corners and the inner joints
CORNERS_END = 0x67BE88  # the scroll, after the corners
RIBBON_EPILOGUE = 0x67C045  # pop edi, esi, ebp, ebx; add esp, 0x7C; ret
RIBBON_SIDES = 0xD4E1D8  # the sides, two a segment (the array's data pointer)
RIBBON_CORNERS = 0xD4E1F0  # the corners, four a segment
RIBBON_COLOURS = 0xD4E240  # the colours, two a corner
CAST_HEADING = 0x4F87A1  # in the creature's spell update: the heading from the hands to the target
MANAGER_HEADING = 0x67339F  # in the effect manager's step: the heading across the land, else along x
MANAGER_HEADING_END = 0x6733D2
MAKE_INVERSE_SQRT_TABLE = 0x8411D0
ENGINE_ALLOC = 0x842CE0
# A call must start in the image: the entry routines go over a particle rule's property list function, which these
# tests do not use (151 bytes)
ENTRY = 0x6B1CD0
LAND_HEIGHT = ENTRY + 0x70  # the stub of the creature's land height virtual function (stdcall, one argument)

# The creature's fields
STATE = 0x4994
SIZE = 0x90
LOWER_VELOCITY = 0x4870
UPPER_VELOCITY = 0x487C
LOWER_OFFSET = 0x4888
UPPER_OFFSET = 0x4894
LEASH_PUSH = 0x48A0
SWAY_ON = 0x48AC
TURN_SECONDS = 0x48B8


EXIT = ENTRY + 0x60  # pop esi; pop ebp; add esp, 8; ret 8


def jump(target: int) -> bytes:
    """mov eax, target; jmp eax"""
    return bytes([0xB8]) + struct.pack("<I", target) + bytes([0xFF, 0xE0])


def spring_entry(creature: int) -> bytes:
    code = bytearray()
    code += bytes([0x81, 0xEC]) + struct.pack("<I", 0xA8)  # sub esp, 0xA8
    code += bytes([0x53, 0x55, 0x56, 0x57])  # push ebx, ebp, esi, edi
    code += bytes([0xBE]) + struct.pack("<I", creature)  # mov esi, creature
    for offset in (0x1C, 0x20, 0x24):  # the leash push of this turn: zero
        code += bytes([0xC7, 0x44, 0x24, offset]) + struct.pack("<I", 0)
    code += jump(SPRING_START)
    return bytes(code)


def ribbon_entry(chain: int, eye: list[int]) -> bytes:
    code = bytearray()
    code += bytes([0x83, 0xEC, 0x7C])  # sub esp, 0x7C
    code += bytes([0x53, 0x55, 0x56, 0x57])  # push ebx, ebp, esi, edi
    code += bytes([0xBF]) + struct.pack("<I", chain)  # mov edi, chain
    for offset, value in zip((0x68, 0x6C, 0x70), eye):
        code += bytes([0xC7, 0x44, 0x24, offset]) + struct.pack("<I", value)
    code += jump(CORNERS_START)
    return bytes(code)


def cast_heading_entry(info: int) -> bytes:
    code = bytearray()
    code += bytes([0x83, 0xEC, 0x0C])  # sub esp, 0xC
    code += bytes([0x56, 0x57])  # push esi, edi
    code += bytes([0xBE]) + struct.pack("<I", info)  # mov esi, info
    code += jump(CAST_HEADING)
    return bytes(code)


def flat_heading_entry(manager: int, info: int) -> bytes:
    code = bytearray()
    code += bytes([0x83, 0xEC, 0x08])  # sub esp, 8
    code += bytes([0x55, 0x56])  # push ebp, esi
    code += bytes([0xBD]) + struct.pack("<I", manager)  # mov ebp, manager
    code += bytes([0xBA]) + struct.pack("<I", info)  # mov edx, info
    code += jump(MANAGER_HEADING)
    return bytes(code)


class CastHeading:
    COLUMNS = ["target_x", "target_y", "target_z", "hands_x", "hands_y", "hands_z", "heading_x", "heading_y",
               "heading_z"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.info = oracle.alloc(0x40)
        oracle.write(ENTRY, cast_heading_entry(self.info))

    def run(self, inputs: list[int]) -> list[int]:
        o = self.o
        o.write_u32(self.info, *inputs[0:6])
        o.write_u32(self.info + 0x18, 0x7F7F7F7F, 0x7F7F7F7F, 0x7F7F7F7F)
        result = o.call(ENTRY, args=[0, 0])
        assert result.popped == 8 and result.depth == 0, result
        return o.read_u32(self.info + 0x18, 3)


class FlatHeading:
    COLUMNS = ["x", "y", "z", "flat_x", "flat_y", "flat_z"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.manager = oracle.alloc(0x100)
        self.info = oracle.alloc(0x40)
        oracle.write(EXIT, bytes([0x5E, 0x5D, 0x83, 0xC4, 0x08, 0xC2, 0x08, 0x00]))
        oracle.write(MANAGER_HEADING_END, jump(EXIT))
        oracle.write(ENTRY, flat_heading_entry(self.manager, self.info))

    def run(self, inputs: list[int]) -> list[int]:
        o = self.o
        o.write_u32(self.manager + 0x3C, *inputs[0:3])
        o.write_u32(self.manager + 0x40, 0)  # the step sets y to zero before it tests the heading
        result = o.call(ENTRY, args=[0, 0])
        assert result.popped == 8 and result.depth == 0, result
        return o.read_u32(self.manager + 0x3C, 3)


class Spring:
    COLUMNS = ["seconds", "mass",
               "lower_vx", "lower_vy", "lower_vz", "upper_vx", "upper_vy", "upper_vz",
               "lower_ox", "lower_oy", "lower_oz", "upper_ox", "upper_oy", "upper_oz",
               "out_lower_vx", "out_lower_vy", "out_lower_vz", "out_upper_vx", "out_upper_vy", "out_upper_vz",
               "out_lower_ox", "out_lower_oy", "out_lower_oz", "out_upper_ox", "out_upper_oy", "out_upper_oz",
               "on"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.creature = oracle.alloc(0x5800)
        oracle.write(SPRING_END, jump(TURN_EPILOGUE))
        oracle.write(ENTRY, spring_entry(self.creature))

    def run(self, inputs: list[int]) -> list[int]:
        o, c = self.o, self.creature
        o.stub(GET_MASS, st0=f32.of_bits(inputs[1]))
        o.write_u32(c + TURN_SECONDS, inputs[0])
        o.write_u32(c + LOWER_VELOCITY, *inputs[2:5])
        o.write_u32(c + UPPER_VELOCITY, *inputs[5:8])
        o.write_u32(c + LOWER_OFFSET, *inputs[8:11])
        o.write_u32(c + UPPER_OFFSET, *inputs[11:14])
        o.write_u32(c + LEASH_PUSH, 0, 0, 0)
        o.write(c + SWAY_ON, bytes([1]))
        result = o.call(ENTRY, args=[0])
        assert result.popped == 4 and result.depth == 0, result
        assert o.read_u32(c + LEASH_PUSH, 3) == [0, 0, 0]
        return (o.read_u32(c + LOWER_VELOCITY, 6) + o.read_u32(c + LOWER_OFFSET, 6)
                + [o.read(c + SWAY_ON, 1)[0]])


class Kick:
    COLUMNS = ["seconds", "mass", "size", "point_y", "land", "force_x", "force_y", "force_z",
               "lower_vx", "lower_vy", "lower_vz", "upper_vx", "upper_vy", "upper_vz",
               "out_lower_vx", "out_lower_vy", "out_lower_vz", "out_upper_vx", "out_upper_vy", "out_upper_vz", "on"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.creature = oracle.alloc(0x5800)
        self.vtable = oracle.alloc(0x100)
        self.point = oracle.alloc(0x10)
        self.force = oracle.alloc(0x10)
        oracle.write_u32(self.vtable + 0x28, LAND_HEIGHT)
        oracle.write_u32(self.creature, self.vtable)
        oracle.write_u32(self.creature + STATE, 0)

    def run(self, inputs: list[int]) -> list[int]:
        o, c = self.o, self.creature
        o.stub(GET_MASS, st0=f32.of_bits(inputs[1]))
        o.stub(LAND_HEIGHT, pop=4, st0=f32.of_bits(inputs[4]))
        o.write_u32(c + TURN_SECONDS, inputs[0])
        o.write_u32(c + SIZE, inputs[2])
        o.write_u32(self.point, f2u(10.0), inputs[3], f2u(20.0))
        o.write_u32(self.force, *inputs[5:8])
        o.write_u32(c + LOWER_VELOCITY, *inputs[8:11])
        o.write_u32(c + UPPER_VELOCITY, *inputs[11:14])
        o.write(c + SWAY_ON, bytes([0]))
        result = o.call(KICK, this=c, args=[self.point, self.force])
        assert result.popped == 8 and result.depth == 0, result
        return o.read_u32(c + LOWER_VELOCITY, 6) + [o.read(c + SWAY_ON, 1)[0]]


class Ribbon:
    JOINTS = 4
    COLUMNS = ([f"joint{j}_{a}" for j in range(4) for a in "xyz"] + [f"width{j}" for j in range(4)]
               + ["eye_x", "eye_y", "eye_z"] + [f"corner{c}_{a}" for c in range(12) for a in "xyz"])

    def __init__(self, oracle: Oracle):
        self.o = oracle
        oracle.hook_alloc(ENGINE_ALLOC)
        oracle.call(MAKE_INVERSE_SQRT_TABLE)
        self.chain = oracle.alloc(0x100)
        self.joints = oracle.alloc(0x1C * self.JOINTS)
        self.sides = oracle.alloc(0x100)
        self.corners = oracle.alloc(0x200)
        self.colours = oracle.alloc(0x200)
        oracle.write_u32(self.chain + 0x44, self.joints, self.JOINTS)
        oracle.write(self.chain + 0x38, bytes([0]))
        oracle.write_u32(RIBBON_SIDES, self.sides)
        oracle.write_u32(RIBBON_CORNERS, self.corners)
        oracle.write_u32(RIBBON_COLOURS, self.colours)
        oracle.write(CORNERS_END, jump(RIBBON_EPILOGUE))

    def run(self, inputs: list[int]) -> list[int]:
        o = self.o
        for j in range(self.JOINTS):
            o.write_u32(self.joints + 0x1C * j, *inputs[3 * j:3 * j + 3], inputs[12 + j])
        o.write(ENTRY, ribbon_entry(self.chain, inputs[16:19]))
        result = o.call(ENTRY)
        assert result.popped == 0 and result.depth == 0, result
        return o.read_u32(self.corners, 36)


def spring_cases() -> list[list[int]]:
    rng = random.Random(0x5357)
    rows = []

    def add(*values: float) -> None:
        rows.append([f2u(v) for v in values])

    # The old pin: the speed is above its limit at the start
    add(0.1, 1000.0, 5.0, 3.0, 2.0, 0.0, 0.0, 0.0, 0.1, 0.2, 0.3, 0.0, 0.0, 0.0)
    # A kick across the land only, as in play: y stays zero
    add(0.1, 1000.0, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    add(0.1, 800.0, 0.0, 0.0, 0.0, -3.0, 0.0, 7.5, 0.0, 0.0, 0.0, 0.4, 0.0, -0.2)
    # Negative zeros
    add(0.1, 500.0, -0.0, -0.0, -0.0, 0.0, -0.0, 0.0, -0.0, 0.0, -0.0, 0.0, -0.0, -0.0)
    # Nearly settled
    add(0.1, 1000.0, 0.01, 0.0, 0.01, 0.0, 0.0, 0.0, 0.02, 0.0, -0.01, 0.0, 0.0, 0.0)
    for _ in range(1500):
        seconds = rng.randint(0, 200) * 0.001 if rng.random() < 0.5 else 0.1
        mass = rng.uniform(50.0, 4000.0)
        values = [seconds, mass]
        for _ in range(2):  # velocities
            values += [rng.uniform(-9.0, 9.0), rng.uniform(-1.0, 1.0) if rng.random() < 0.3 else 0.0,
                       rng.uniform(-9.0, 9.0)]
        for _ in range(2):  # offsets
            values += [rng.uniform(-2.0, 2.0), rng.uniform(-0.5, 0.5) if rng.random() < 0.3 else 0.0,
                       rng.uniform(-2.0, 2.0)]
        add(*values)
    return rows


def kick_cases() -> list[list[int]]:
    rng = random.Random(0x4B49)
    rows = []

    def add(*values: float) -> None:
        rows.append([f2u(v) for v in values])

    add(0.05, 1000.0, 1.0, 10.0, 0.0, 1000.0, 500.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    add(1.0, 100.0, 1.0, 2.0, 0.0, 0.0, 0.0, 1e6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    # On the split height exactly: the lower body
    add(0.1, 1000.0, 1.0, 9.0, 0.0, 300.0, 0.0, 300.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    for _ in range(1500):
        size = rng.uniform(0.3, 3.0)
        land = rng.uniform(0.0, 300.0)
        point_y = land + rng.uniform(-2.0, 2.0) * size * 9.0 + size * 9.0
        add(rng.randint(0, 200) * 0.001 if rng.random() < 0.5 else 0.1, rng.uniform(50.0, 4000.0), size, point_y,
            land, rng.uniform(-1e5, 1e5), rng.uniform(-1e5, 1e5), rng.uniform(-1e5, 1e5),
            rng.uniform(-8.0, 8.0), 0.0, rng.uniform(-8.0, 8.0), rng.uniform(-8.0, 8.0), 0.0, rng.uniform(-8.0, 8.0))
    return rows


def ribbon_cases() -> list[list[int]]:
    rng = random.Random(0x5249)
    rows = []

    def add(*values: float) -> None:
        rows.append([f2u(v) for v in values])

    # The old pin's joints and eye, with a fourth joint
    add(0.0, 0.0, 0.0, 10.0, 2.0, 0.0, 20.0, 0.0, 5.0, 30.0, 5.0, 5.0, 1.0, 2.0, 1.5, 0.5, 5.0, 50.0, -40.0)
    add(0.0, 0.0, 0.0, 10.0, 2.0, 0.0, 20.0, 0.0, 5.0, 30.0, 5.0, 5.0, 1.0, 2.0, 1.5, 0.5, -30.0, 2.0, 60.0)
    # The eye on the line of the first segment: its sides are zero
    add(0.0, 0.0, 0.0, 10.0, 0.0, 0.0, 20.0, 5.0, 0.0, 30.0, 5.0, 5.0, 1.0, 2.0, 1.5, 0.5, -50.0, 0.0, 0.0)
    for _ in range(1500):
        base = [rng.uniform(0.0, 5000.0), rng.uniform(0.0, 300.0), rng.uniform(0.0, 5000.0)]
        joints = []
        point = base
        for _ in range(4):
            joints += point
            point = [point[0] + rng.uniform(-30.0, 30.0), point[1] + rng.uniform(-30.0, 30.0),
                     point[2] + rng.uniform(-30.0, 30.0)]
        widths = [rng.uniform(0.05, 8.0) for _ in range(4)]
        eye = [base[0] + rng.uniform(-400.0, 400.0), base[1] + rng.uniform(-50.0, 400.0),
               base[2] + rng.uniform(-400.0, 400.0)]
        add(*joints, *widths, *eye)
    return rows


def cast_heading_cases() -> list[list[int]]:
    rng = random.Random(0x4348)
    rows = []

    def add(*values: float) -> None:
        rows.append([f2u(v) for v in values])

    add(10.0, 0.0, 20.0, 10.0, 0.0, 20.0)  # the same point: zero
    add(10.0, 5.0, 20.0, 10.0, 5.0, 20.0)
    add(-0.0, 0.0, 0.0, 0.0, -0.0, 0.0)
    add(100.0, 0.0, 100.0, 100.0, 50.0, 100.0)  # straight down
    for _ in range(2000):
        hands = [rng.uniform(0.0, 5000.0), rng.uniform(0.0, 300.0), rng.uniform(0.0, 5000.0)]
        reach = rng.choice([0.01, 1.0, 30.0, 300.0])
        target = [h + rng.uniform(-reach, reach) for h in hands]
        add(*target, *hands)
    return rows


def flat_heading_cases() -> list[list[int]]:
    rng = random.Random(0x464C)
    rows = []

    def add(*values: float) -> None:
        rows.append([f2u(v) for v in values])

    for values in ((0.0, -1.0, 0.0), (0.0, 1.0, 0.0), (-0.0, -1.0, -0.0), (0.01, -0.99, 0.0), (0.0, -0.99, 0.01),
                   (0.007, -0.99, 0.007), (0.0071, -0.99, 0.0071), (1.0, 0.0, 0.0), (-0.6, -0.5, 0.6)):
        add(*values)
    for _ in range(1500):
        scale = rng.choice([1.0, 0.02, 0.01, 0.0075, 0.005])
        add(rng.uniform(-scale, scale), rng.uniform(-1.0, 1.0), rng.uniform(-scale, scale))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as o:
        spring = Spring(o)
        spring_rows = [inputs + spring.run(inputs) for inputs in spring_cases()]
    with Oracle() as o:
        kick = Kick(o)
        kick_rows = [inputs + kick.run(inputs) for inputs in kick_cases()]
    with Oracle() as o:
        ribbon = Ribbon(o)
        ribbon_rows = [inputs + ribbon.run(inputs) for inputs in ribbon_cases()]
    with Oracle() as o:
        cast = CastHeading(o)
        cast_rows = [inputs + cast.run(inputs) for inputs in cast_heading_cases()]
    with Oracle() as o:
        flat = FlatHeading(o)
        flat_rows = [inputs + flat.run(inputs) for inputs in flat_heading_cases()]
    write_pinned(
        out / "cast_heading.txt",
        "The heading from a creature's hands to the point it casts at (the end of the creature's spell update)",
        ["The code runs as it is from an entry routine. The heading starts as a marker; zero ways keep the zero way."],
        CastHeading.COLUMNS,
        cast_rows,
    )
    write_pinned(
        out / "flat_heading.txt",
        "The heading that a particle effect keeps from its process info (the effect manager's step)",
        ["The code runs as it is from an entry routine; y is zero first, as the step sets it."],
        FlatHeading.COLUMNS,
        flat_rows,
    )
    write_pinned(
        out / "creature_sway_spring.txt",
        "One game turn of the creature sway's spring (the game's spring in the 3D creature's turn)",
        [
            "The spring runs as it is from an entry routine; a jump at its end returns.",
            "The mass comes from a stub. The leash push is zero, as in play. 'on' is the sway's flag after the turn.",
        ],
        Spring.COLUMNS,
        spring_rows,
    )
    write_pinned(
        out / "creature_sway_kick.txt",
        "A kick of the creature sway by a force at a point (the game's kick function)",
        [
            "The point is (10, point_y, 20). The land height and the mass come from stubs.",
            "The upper body takes the kick when point_y - land > size x 9.",
        ],
        Kick.COLUMNS,
        kick_rows,
    )
    write_pinned(
        out / "ribbon_corners.txt",
        "The corners of a ribbon of four joints as seen from an eye (the corner part of the game's ribbon draw)",
        [
            "The corner part runs as it is from an entry routine, with the 3D engine's inverse square root table.",
            "Four corners a segment: the first joint plus and minus its side, then the second joint's.",
        ],
        Ribbon.COLUMNS,
        ribbon_rows,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
