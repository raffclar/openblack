#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the footpaths sent round things in test/oracle/data:

- footpath_go_round.txt: a path sent round a thing put down on it, by the game's own function
- footpath_stop_going_round.txt: a path brought back when the thing is taken away, from a path the game sent round it

Each case is one path in the game's list. The route planner of the game is replaced by a small function that records
each stretch it is asked to plan (its two anchors and whether a marker is given) and then either finds no way, or
"finds" an empty route: it takes out the stretch's nodes as the real planner's laying does, and lays nothing. The
walkers' lists are empty. The land's height is 0.

Usage: python tools/oracle/cases/footpath_obstacles_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions and globals (BW1W120, runblack.exe 1.20)
GO_ROUND = 0x537290  # cdecl (radius, map position*)
STOP_GOING_ROUND = 0x537DF0  # cdecl (radius, map position*)
PLAN_AGAIN = 0x5387D0  # cdecl (first node, last node, marker map position* or 0) -> found
ERASE_BETWEEN = 0x538B90  # cdecl (first node, last node) -> the node after the first
ALLOCATE = 0x4366F0  # cdecl (size, file, line)
ALTITUDE = 0x803090
MAKE_INVERSE_SQRT_TABLE = 0x74F590
INVERSE_SQRT_TABLE = 0xDA5A10
GAME = 0xD0195C  # the game object; its flags at +0x14, its first footpath at +0x205C64
GAME_FLAGS = 0x14
GAME_FOOTPATHS = 0x205C64
NODE_VTABLE = 0x8DDFBC

PATH_SIZE = 0x40
PATH_HEAD = 0x14
PATH_COUNT = 0x18
PATH_NEXT = 0x1C
NODE_SIZE = 0x30
NODE_FLAGS = 0x14
NODE_PLACE = 0x18
NODE_NEXT = 0x24

PLANNED = 0x2
HIDDEN = 0x4
MARKER = 0x8

MOST_IN = 24
MOST_OUT = 28
MOST_PLANS = 4
UNITS_PER_METRE = 6553.6


def f(value: float) -> float:
    return u2f(f2u(value))


class Paths:
    def __init__(self, o: Oracle):
        self.o = o
        o.hook_alloc(ALLOCATE)
        o.stub(ALTITUDE, st0=0.0)
        # Nodes are deleted through their vtable: the delete does nothing here
        destructor = o.read_u32(NODE_VTABLE + 0xC)[0]
        o.stub(destructor, pop=4)
        o.call(MAKE_INVERSE_SQRT_TABLE)
        assert any(o.read_u32(INVERSE_SQRT_TABLE, 1024))
        self.game = o.alloc(GAME_FOOTPATHS + 0x10)
        o.write_u32(GAME, self.game)
        o.write_u32(self.game + GAME_FLAGS, 0)
        # The planner's stand-in: records (first, last, marker) and, when told, takes out the stretch's nodes
        self.found = o.alloc(4)
        self.count = o.alloc(4)
        self.record = o.alloc(12 * 64)
        code = bytearray()
        code += b"\x8b\x0d" + struct.pack("<I", self.count)  # mov ecx, [count]
        code += b"\x8d\x14\x49"  # lea edx, [ecx + ecx * 2]
        code += b"\xc1\xe2\x02"  # shl edx, 2
        code += b"\x81\xc2" + struct.pack("<I", self.record)  # add edx, record
        code += b"\x8b\x44\x24\x04\x89\x02"  # mov eax, [esp + 4]; mov [edx], eax
        code += b"\x8b\x44\x24\x08\x89\x42\x04"  # mov eax, [esp + 8]; mov [edx + 4], eax
        code += b"\x8b\x44\x24\x0c\x89\x42\x08"  # mov eax, [esp + 12]; mov [edx + 8], eax
        code += b"\x41\x89\x0d" + struct.pack("<I", self.count)  # inc ecx; mov [count], ecx
        code += b"\xa1" + struct.pack("<I", self.found)  # mov eax, [found]
        erase = b"\xff\x74\x24\x08\xff\x74\x24\x08"  # push last; push first
        erase += b"\xb8" + struct.pack("<I", ERASE_BETWEEN) + b"\xff\xd0"  # mov eax, erase; call eax
        erase += b"\x83\xc4\x08"  # add esp, 8
        erase += b"\x8b\x4c\x24\x04\x89\x41\x24"  # mov ecx, [esp + 4]; mov [ecx + 0x24], eax
        code += b"\x85\xc0\x74" + bytes([len(erase)])  # test eax, eax; jz done
        code += erase
        code += b"\xa1" + struct.pack("<I", self.found)  # done: mov eax, [found]
        code += b"\xc3"
        stand_in = o.alloc(len(code) + 16, bytes(code))
        o.write(PLAN_AGAIN, b"\xb8" + struct.pack("<I", stand_in) + b"\xff\xe0")
        self.centre = o.alloc(16)

    def lay(self, nodes: list[tuple[int, int, int]]) -> int:
        o = self.o
        path = o.alloc(PATH_SIZE)
        addresses = [o.alloc(NODE_SIZE) for _ in nodes]
        for i, (x, z, flags) in enumerate(nodes):
            at = addresses[i]
            o.write_u32(at, NODE_VTABLE)
            o.write(at + NODE_FLAGS, bytes([flags | 1]))
            o.write_u32(at + NODE_PLACE, x & 0xFFFFFFFF, z & 0xFFFFFFFF, 0)
            o.write_u32(at + NODE_NEXT, addresses[i + 1] if i + 1 < len(addresses) else 0, 0, 0)
        o.write_u32(path + PATH_HEAD, addresses[0] if addresses else 0, len(nodes), 0)
        o.write_u32(self.game + GAME_FOOTPATHS, path)
        return path

    def read(self, path: int) -> list[tuple[int, int, int, int]]:
        o = self.o
        nodes = []
        at = o.read_u32(path + PATH_HEAD)[0]
        while at != 0 and len(nodes) <= MOST_OUT:
            x, z = o.read_u32(at + NODE_PLACE, 2)
            flags = o.read(at + NODE_FLAGS, 1)[0] & 0xF
            nodes.append((x, z, flags, at))
            at = o.read_u32(at + NODE_NEXT)[0]
        return nodes

    def run(self, function: int, path: int, centre: tuple[int, int], radius: float, found: bool):
        o = self.o
        o.write_u32(self.found, 1 if found else 0)
        o.write_u32(self.count, 0)
        o.write_u32(self.centre, centre[0] & 0xFFFFFFFF, centre[1] & 0xFFFFFFFF, 0)
        o.call(function, args=[f32(radius), self.centre])
        count = o.read_u32(self.count)[0]
        plans = []
        for i in range(count):
            first, last, marker = o.read_u32(self.record + 12 * i, 3)
            plans.append((*o.read_u32(first + NODE_PLACE, 2), *o.read_u32(last + NODE_PLACE, 2), 1 if marker else 0))
        return plans


def random_path(rng: random.Random, centre: tuple[int, int], radius: float) -> list[tuple[int, int, int]]:
    """A path that passes the thing: its ends well away on either side, the nodes between near the line"""
    units = radius * UNITS_PER_METRE
    angle = rng.uniform(0.0, 6.2831853)
    import math
    reach = rng.uniform(2.0, 4.0) * units + 3.0 * UNITS_PER_METRE
    side = rng.uniform(-1.4, 1.4) * units
    ux, uz = math.cos(angle), math.sin(angle)
    nx, nz = -uz, ux
    count = rng.randint(2, 7)
    nodes = []
    for i in range(count):
        t = -1.0 + 2.0 * i / (count - 1)
        wobble = rng.uniform(-0.6, 0.6) * units if 0 < i < count - 1 else 0.0
        x = centre[0] + ux * t * reach + nx * (side + wobble)
        z = centre[1] + uz * t * reach + nz * (side + wobble)
        if 0 < i < count - 1:
            flags = rng.choices([0, PLANNED, HIDDEN, PLANNED | MARKER], weights=[50, 35, 8, 7])[0]
        else:
            flags = 0
        nodes.append((int(x), int(z), flags))
    return nodes


def row_of(nodes_in, centre, radius, found, plans, nodes_out, first_index) -> list[int]:
    row = [centre[0], centre[1], f2u(radius), 1 if found else 0, len(nodes_in)]
    for i in range(MOST_IN):
        row += list(nodes_in[i][:3]) if i < len(nodes_in) else [0, 0, 0]
    row += [len(plans)]
    for i in range(MOST_PLANS):
        row += list(plans[i]) if i < len(plans) else [0, 0, 0, 0, 0]
    row += [first_index, len(nodes_out)]
    for i in range(MOST_OUT):
        row += list(nodes_out[i][:3]) if i < len(nodes_out) else [0, 0, 0]
    return row


def columns() -> list[str]:
    names = ["centre_x", "centre_z", "radius", "found", "in_count"]
    names += [f"in{i}_{part}" for i in range(MOST_IN) for part in ("x", "z", "flags")]
    names += ["plan_count"]
    names += [f"plan{i}_{part}" for i in range(MOST_PLANS) for part in ("from_x", "from_z", "to_x", "to_z", "marker")]
    names += ["first_index", "out_count"]
    names += [f"out{i}_{part}" for i in range(MOST_OUT) for part in ("x", "z", "flags")]
    return names


def index_of_first_found(nodes_out, plans, found) -> int:
    """With the planner finding its empty route, the index of the stretch's first anchor in the result (the last plan)"""
    if not found or not plans:
        return 0xFFFFFFFF
    fx, fz = plans[-1][0], plans[-1][1]
    for i, node in enumerate(nodes_out):
        if node[0] == fx and node[1] == fz and node[2] & 0xE == 0:
            return i
    return 0xFFFFFFFF


def go_round_cases(paths: Paths, rng: random.Random, count: int) -> list[list[int]]:
    rows = []
    while len(rows) < count:
        centre = (int(rng.uniform(500.0, 4500.0) * UNITS_PER_METRE), int(rng.uniform(500.0, 4500.0) * UNITS_PER_METRE))
        radius = f(rng.uniform(0.8, 14.0))
        nodes = random_path(rng, centre, radius)
        found = rng.random() < 0.3
        path = paths.lay(nodes)
        plans = paths.run(GO_ROUND, path, centre, radius, found)
        out = paths.read(path)
        if len(out) > MOST_OUT or len(plans) > MOST_PLANS:
            continue
        rows.append(row_of(nodes, centre, radius, found, plans, out, index_of_first_found(out, plans, found)))
    return rows


def stop_cases(paths: Paths, rng: random.Random, count: int) -> list[list[int]]:
    rows = []
    while len(rows) < count:
        centre = (int(rng.uniform(500.0, 4500.0) * UNITS_PER_METRE), int(rng.uniform(500.0, 4500.0) * UNITS_PER_METRE))
        radius = f(rng.uniform(0.8, 14.0))
        nodes = random_path(rng, centre, radius)
        path = paths.lay(nodes)
        # The game sends the path round the thing first: the path as it was then is the case's input
        paths.run(GO_ROUND, path, centre, radius, rng.random() < 0.3)
        sent = paths.read(path)
        if len(sent) > MOST_IN:
            continue
        nodes_in = [(x, z, flags) for x, z, flags, _ in sent]
        # Sometimes the thing taken away is a little off or another size
        if rng.random() < 0.3:
            centre = (centre[0] + int(rng.uniform(-1.0, 1.0) * UNITS_PER_METRE), centre[1] + int(rng.uniform(-1.0, 1.0) * UNITS_PER_METRE))
            radius = f(radius * rng.uniform(0.7, 1.3))
        found = rng.random() < 0.4
        plans = paths.run(STOP_GOING_ROUND, path, centre, radius, found)
        out = paths.read(path)
        if len(out) > MOST_OUT or len(plans) > MOST_PLANS:
            continue
        rows.append(row_of(nodes_in, centre, radius, found, plans, out, 0xFFFFFFFF))
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x63F02)
    with Oracle() as o:
        paths = Paths(o)
        notes = ["Node flags: 1 always, 2 planned, 4 hidden, 8 marker. Places in whole map units. A plan row is the",
                 "stretch the planner was asked for: its first and last anchors' places and 1 when a marker was given.",
                 "found 1: the planner's stand-in takes out the stretch's nodes but the hidden ones and the markers, and",
                 "lays nothing. first_index: then the index of the found stretch's first anchor in the result."]
        n = write_pinned(out / "footpath_go_round.txt", "A footpath sent round a thing put down on it", notes,
                         columns(), go_round_cases(paths, rng, 400))
        print(f"footpath_go_round.txt: {n} rows")
        n = write_pinned(out / "footpath_stop_going_round.txt", "A footpath brought back from going round a thing",
                         notes, columns(), stop_cases(paths, rng, 400))
        print(f"footpath_stop_going_round.txt: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
