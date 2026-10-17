#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the bird formation and the land line in test/oracle/data:

- formation_goal.txt: where a flock's follower goes for its slot, from the leader, the follower and its place
- land_line.txt: where a line in metres meets the land (the float 0.1 per metre), and the cell test at the plane
  limit (the double of the float 0.0001)

Usage: python tools/oracle/cases/formation_land_line_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
FORMATION = 0x41E890  # thiscall (follower), sets the follower's goal through SET_GOAL
FOLLOW_STATE = 0x5EC2A0  # thiscall (animal) -> EAX, the formation needs 10
SET_GOAL = 0x5F2830  # thiscall (animal), (goal, state), RET 8
LAND_LINE = 0x802550  # ECX from, EDX to (metres), (out x, out z), RET 8
LAND_LINE_SEA_JUMP = 0x8025A9  # the jump to the sea part when no land is hit
CELL_TEST = 0x83AE80  # ECX x, EDX z, (from x, z, y, to x, z, y in cells), RET 0x18
CELL_HIT = 0xE9CD80  # the cell test's hit, x and z floats
BLOCK_INDEX = 0xE9C964  # 32 x 32 bytes: the block of each 16 x 16 cells, 0 for none
BLOCK_POINTER = 0xE9C564  # 256 pointers: the cells of each block, 17 x 17 corners of 8 bytes

# The animal and flock fields the formation reads
ANIMAL_POSITION = 0x14
ANIMAL_FLOCK = 0xB8
FLOCK_HEAD = 0x3C
FLOCK_LEADER = 0x40
FLOCK_COUNT = 0x48
NODE_ANIMAL = 0x8

SIDE = 17


def capture_code(store: int) -> bytes:
    """x86 code that copies the three dwords at its first argument to store, then removes 8 bytes."""
    code = bytes.fromhex("8b442404") + bytes.fromhex("8b08") + b"\x89\x0d" + struct.pack("<I", store)
    code += bytes.fromhex("8b4804") + b"\x89\x0d" + struct.pack("<I", store + 4)
    code += bytes.fromhex("8b4808") + b"\x89\x0d" + struct.pack("<I", store + 8)
    return code + bytes.fromhex("c20800")


def formation_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    goal = o.alloc(16)
    o.write(SET_GOAL, capture_code(goal))
    o.stub(FOLLOW_STATE, eax=10)
    leader = o.alloc(0x200)
    follower = o.alloc(0x200)
    flock = o.alloc(0x100)
    o.write_u32(follower + ANIMAL_FLOCK, flock)
    leader_node = o.alloc(16)
    o.write_u32(leader_node + NODE_ANIMAL, leader)
    o.write_u32(flock + FLOCK_LEADER, leader_node)
    nodes = [o.alloc(16) for _ in range(40)]
    others = [o.alloc(0x200) for _ in range(40)]
    rows = []

    def one(place: int, lx: int, lz: int, fx: int, fz: int) -> None:
        count = place + rng.randint(0, 5)
        index = count - place
        for i in range(count):
            o.write_u32(nodes[i], nodes[i + 1] if i + 1 < count else 0)
            o.write_u32(nodes[i] + NODE_ANIMAL, follower if i == index else others[i])
        o.write_u32(flock + FLOCK_HEAD, nodes[0])
        o.write(flock + FLOCK_COUNT, bytes([count]))
        o.write_u32(leader + ANIMAL_POSITION, lx & 0xFFFFFFFF, lz & 0xFFFFFFFF, 0x1234)
        o.write_u32(follower + ANIMAL_POSITION, fx & 0xFFFFFFFF, fz & 0xFFFFFFFF, 0)
        o.write_u32(goal, 0, 0, 0)
        r = o.call(FORMATION, this=follower)
        if r.eax != 10:
            raise RuntimeError("the formation did not run")
        gx, gz, gy = o.read_u32(goal, 3)
        if gy != 0x1234:
            raise RuntimeError("the goal was not set")
        rows.append([place, lx, lz, fx, fz, gx, gz])

    full = 512 * 65536
    for _ in range(600):
        place = rng.randint(1, 30)
        lx = rng.randint(0, full - 1)
        lz = rng.randint(0, full - 1)
        reach = rng.choice((65536, 30 * 65536, 300 * 65536))
        one(place, lx, lz, lx + rng.randint(-reach, reach), lz + rng.randint(-reach, reach))
    # The leader on the follower, on an axis, near the map's corners and at the far edge where metres round
    for place in (1, 2, 3, 5, 9, 16, 25):
        one(place, 100 * 65536, 100 * 65536, 100 * 65536, 100 * 65536)
        one(place, 8090858, 8090858, 8090858 - 655360, 8090858)
        one(place, 0, 0, 65536, 0)
        one(place, full - 1, full - 1, full - 65536, full - 1)
        one(place, 33000000, 16777217, 33000000, 16777217 + 655360)
    return rows


def land_heights(rng: random.Random) -> list[int]:
    heights = [rng.randint(0, 255) if rng.random() < 0.7 else rng.randint(0, 20) for _ in range(SIDE * SIDE)]
    # A flat area at 0 for the plane limit, and a flat area at 100
    for x in range(2, 6):
        for z in range(2, 6):
            heights[x * SIDE + z] = 0
    for x in range(9, 13):
        for z in range(9, 13):
            heights[x * SIDE + z] = 100
    return heights


def set_land(o: Oracle, heights: list[int]) -> None:
    block = o.alloc(SIDE * SIDE * 8)
    for i, h in enumerate(heights):
        o.write(block + i * 8 + 4, bytes([h]))
    o.write_u32(BLOCK_POINTER + 4, block)
    o.write(BLOCK_INDEX, bytes([1]) * 1024)


def land_line_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    heights = land_heights(rng)
    set_land(o, heights)
    # Without land on the line, the game goes on to the sea. Here it returns no hit, so only the land part runs
    o.write(LAND_LINE_SEA_JUMP, bytes.fromhex("743b"))
    rows = [[0, 0, 0, i, 0, h, 0, 0, 0, 0, 0, 0] for i, h in enumerate(heights)]
    points = o.alloc(32)
    out = o.alloc(16)

    def along(start: tuple[float, float, float], end: tuple[float, float, float]) -> None:
        a = [f2u(v) for v in start]
        b = [f2u(v) for v in end]
        o.write_u32(points, *a)
        o.write_u32(points + 16, *b)
        o.write_u32(out, 0, 0)
        o.write_u32(CELL_HIT, 0, 0)
        r = o.call(LAND_LINE, ecx=points, edx=points + 16, args=[out, out + 4])
        hit = o.read_u32(out, 2) if r.eax else [0, 0]
        rows.append([1, 0, 0] + a + b + [r.eax & 0xFF] + hit)

    for _ in range(500):
        x = rng.uniform(0.0, 5120.0)
        z = rng.uniform(0.0, 5120.0)
        y = rng.uniform(20.0, 1500.0)
        dx = rng.uniform(-400.0, 400.0)
        dz = rng.uniform(-400.0, 400.0)
        along((x, y, z), (x + dx, rng.uniform(-50.0, y), z + dz))
    # Straight down, along the axes, and over the flat areas
    for x, z in ((25.0, 25.0), (35.3, 47.1), (105.0, 105.0), (2563.7, 1999.9), (5119.0, 0.5)):
        along((x, 900.0, z), (x, -10.0, z))
        along((x, 300.0, z), (x + 200.0, 0.0, z))
        along((x, 300.0, z), (x, 0.0, z + 200.0))

    def cell(x: int, z: int, start: tuple[int, int, int], end: tuple[int, int, int]) -> None:
        o.write_u32(CELL_HIT, 0, 0)
        # The game takes each point as x, z, y
        r = o.call(CELL_TEST, ecx=x, edx=z, args=[start[0], start[2], start[1], end[0], end[2], end[1]])
        hit = o.read_u32(CELL_HIT, 2) if r.eax else [0, 0]
        rows.append([2, x, z] + list(start) + list(end) + [r.eax & 0xFF] + hit)

    # The plane limit: a climb of 0.0001 in single precision over a flat cell at 0, and the floats next to it
    limit = f2u(0.0001)
    for climb in (limit - 2, limit - 1, limit, limit + 1, limit + 2):
        for sign in (0, 0x80000000):
            for x, z, u, v in ((2, 2, 0.5, 0.5), (3, 4, 0.25, 0.125), (4, 3, 0.75, 0.0625)):
                start = (f2u(x + u), 0 if sign == 0 else climb, f2u(z + v))
                end = (f2u(x + u), climb if sign == 0 else 0, f2u(z + v))
                cell(x, z, start, end)
    # Lines through random cells
    for _ in range(300):
        x = rng.randint(0, 511)
        z = rng.randint(0, 511)
        start = (x + rng.uniform(-0.5, 1.5), rng.uniform(-10.0, 300.0), z + rng.uniform(-0.5, 1.5))
        end = (start[0] + rng.uniform(-2.0, 2.0), rng.uniform(-10.0, 300.0), start[2] + rng.uniform(-2.0, 2.0))
        cell(x, z, tuple(f2u(v) for v in start), tuple(f2u(v) for v in end))
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x0DA7A)
    with Oracle() as o:
        n = write_pinned(out / "formation_goal.txt", "Where a flock's follower goes for its slot",
                         ["The follower's place in the flock (counted from the leader), the leader's and the",
                          "follower's x and z in 16.16 map units in; the goal's x and z in 16.16 map units out."],
                         ["place", "leader_x", "leader_z", "follower_x", "follower_z", "goal_x", "goal_z"],
                         formation_cases(o, rng))
        print(f"formation_goal: {n} rows")
        o.reset()
        n = write_pinned(out / "land_line.txt", "Where a line meets the land",
                         ["The land is one block of 17 x 17 corners, the same in each 16 x 16 cells of the map.",
                          "kind 0: corner a (x * 17 + z) has the height c. kind 1: the line from (a, b, c) to",
                          "(d, e, f) in metres, the land part only; hit 1 and the point (hit_x, hit_z) in metres.",
                          "kind 2: the cell test of cell (x, z) for the line in cells; hit 1 and the point in cells.",
                          "Floats are bits."],
                         ["kind", "x", "z", "a", "b", "c", "d", "e", "f", "hit", "hit_x", "hit_z"],
                         land_line_cases(o, rng))
        print(f"land_line: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
