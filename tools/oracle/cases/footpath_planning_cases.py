#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the footpaths laid in play in test/oracle/data:

- footpath_route_nodes.txt: the nodes a found route gives a footpath (two straight legs and two arcs)
- footpath_end_clear.txt: a path's end moved out of one circle of its building
- footpath_off_middle.txt: a path's end exactly on its building's place moved towards the other end
- footpath_points.txt: whole map units to metres and back, and the distance between two points

Usage: python tools/oracle/cases/footpath_planning_cases.py [output folder]   (default: test/oracle/data)
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
ROUTE_TO_NODES = 0x538340  # cdecl (holder, plan, start node, end node, extra place)
CLEAR_FROM_PREVIOUS = 0x538B10  # thiscall (node): the walkers heading for a node
CLEAR_FOLLOWERS = 0x538B50  # fastcall (node): the same for the other walkers
ERASE_BETWEEN = 0x538B90  # cdecl (start, end) -> the node after the new ones
ALLOCATE = 0x4366F0  # cdecl (size, file, line)
END_CLEAR_CIRCLE = 0x535BD0  # cdecl (thing, centre x, centre z, radius, update), globals below
END_POINT = 0xCD0478  # the end being moved (a pointer to two floats)
END_BUILDING = 0xCD0468  # the building's place, two floats
END_OTHER = 0xCD0470  # the other end, two floats
END_CLEAR = 0x535A80  # cdecl (end, building, other x, other z)
HOLDER_ALLOCATE = 0x7DB790
HOLDER_FREE = 0x7AEE98
GET_POS = 0x401960  # the place of a thing with a position
TO_POINT = 0x535780  # cdecl (out point, place)
ALTITUDE = 0x803090
TO_PLACE = 0x5357C0  # cdecl (out place, point)
RANGE = 0x86FDC0  # thiscall (point), (other) -> ST0

# A route's legs
NODE_SIZE = 0x2C
NODE_FROM = 0x0
NODE_TO = 0x8
NODE_HEADING = 0x10
NODE_WAY = 0x14
NODE_ARC = 0x1C
NODE_NEXT = 0x24
PLAN_START = 0x2C
PLAN_GOAL = 0x34
PLAN_ROUTE = 0x68
CIRCLE_SIZE = 0x14
CIRCLE_CENTRE = 0x8
CIRCLE_RADIUS = 0x10
FOOTPATH_NODE_SIZE = 0x30
FOOTPATH_NODE_FLAGS = 0x14
FOOTPATH_NODE_PLACE = 0x18
FOOTPATH_NODE_NEXT = 0x24

MOST_NODES = 64
CLOCKWISE = 1
ANTICLOCKWISE = 2


def f(value: float) -> float:
    return u2f(f2u(value))


def on_circle(rng: random.Random, centre: tuple[float, float], radius: float) -> tuple[float, float]:
    angle = rng.uniform(0.0, 2.0 * math.pi)
    reach = radius * rng.uniform(0.98, 1.02)
    return f(centre[0] + reach * math.cos(angle)), f(centre[1] + reach * math.sin(angle))


def route_cases(o: Oracle, rng: random.Random, count: int) -> list[list[int]]:
    o.hook_alloc(ALLOCATE)
    o.stub(CLEAR_FROM_PREVIOUS)
    o.stub(CLEAR_FOLLOWERS)
    rows = []
    for _ in range(count):
        o.reset()
        o.hook_alloc(ALLOCATE)
        o.stub(CLEAR_FROM_PREVIOUS)
        o.stub(CLEAR_FOLLOWERS)
        base = (rng.uniform(500.0, 4500.0), rng.uniform(500.0, 4500.0))
        circles = []
        for _ in range(2):
            radius = f(rng.uniform(0.5, 20.0))
            circles.append(((f(base[0] + rng.uniform(-60.0, 60.0)), f(base[1] + rng.uniform(-60.0, 60.0))), radius))
        start = (f(base[0] + rng.uniform(-80.0, 80.0)), f(base[1] + rng.uniform(-80.0, 80.0)))
        goal = (f(base[0] + rng.uniform(-80.0, 80.0)), f(base[1] + rng.uniform(-80.0, 80.0)))
        a = on_circle(rng, *circles[0])
        b = on_circle(rng, *circles[0])
        c = on_circle(rng, *circles[1])
        d = on_circle(rng, *circles[1])
        ways = [rng.choice([CLOCKWISE, ANTICLOCKWISE]) for _ in range(2)]

        holder = o.alloc(CIRCLE_SIZE * 2)
        for i, (centre, radius) in enumerate(circles):
            o.write_f32(holder + i * CIRCLE_SIZE + CIRCLE_CENTRE, centre[0], centre[1], radius)
        legs = [
            (start, a, 0, ways[0], 0),
            (a, b, -1, 0, 1),
            (b, c, 1, ways[1], 0),
            (c, d, -1, 0, 1),
            (d, goal, -1, 0, 0),
        ]
        nodes = [o.alloc(NODE_SIZE) for _ in legs]
        for i, (frm, to, heading, way, arc) in enumerate(legs):
            o.write_f32(nodes[i] + NODE_FROM, frm[0], frm[1], to[0], to[1])
            o.write_u32(nodes[i] + NODE_HEADING, heading, way)
            o.write_u32(nodes[i] + NODE_ARC, arc)
            o.write_u32(nodes[i] + NODE_NEXT, nodes[i + 1] if i + 1 < len(nodes) else 0)
        route = o.alloc(16)
        o.write_u32(route, nodes[0], nodes[-1])
        plan = o.alloc(0x80)
        o.write_f32(plan + PLAN_START, start[0], start[1], goal[0], goal[1])
        o.write_u32(plan + PLAN_ROUTE, route)
        first = o.alloc(FOOTPATH_NODE_SIZE)
        last = o.alloc(FOOTPATH_NODE_SIZE)
        o.write_u32(first + FOOTPATH_NODE_NEXT, last)
        o.stub(ERASE_BETWEEN, eax=last)
        o.call(ROUTE_TO_NODES, args=[holder, plan, first, last, 0])
        places = []
        at = o.read_u32(first + FOOTPATH_NODE_NEXT)[0]
        while at != last and at != 0 and len(places) < MOST_NODES:
            places.append(o.read_u32(at + FOOTPATH_NODE_PLACE, 2))
            at = o.read_u32(at + FOOTPATH_NODE_NEXT)[0]
        if at != last:
            continue
        row = [f2u(v) for v in (*start, *goal, *a, *b, *c, *d)]
        for centre, radius in circles:
            row += [f2u(centre[0]), f2u(centre[1]), f2u(radius)]
        row += ways + [len(places)]
        flat = [v for place in places for v in place]
        row += flat + [0] * (2 * MOST_NODES - len(flat))
        rows.append(row)
    return rows


def end_clear_cases(o: Oracle, rng: random.Random, count: int) -> list[list[int]]:
    o.reset()
    end = o.alloc(8)
    o.write_u32(END_POINT, end)
    rows = []
    for _ in range(count):
        building = (f(rng.uniform(500.0, 4500.0)), f(rng.uniform(500.0, 4500.0)))
        centre = (f(building[0] + rng.uniform(-6.0, 6.0)), f(building[1] + rng.uniform(-6.0, 6.0)))
        radius = f(rng.uniform(0.5, 12.0))
        point = (f(centre[0] + rng.uniform(-14.0, 14.0)), f(centre[1] + rng.uniform(-14.0, 14.0)))
        o.write_f32(END_BUILDING, *building)
        o.write_f32(end, *point)
        o.call(END_CLEAR_CIRCLE, args=[0, f32(centre[0]), f32(centre[1]), f32(radius), 0])
        moved = o.read_u32(end, 2)
        rows.append([f2u(v) for v in (*point, *building, *centre, radius)] + moved)
    return rows


def off_middle_cases(o: Oracle, rng: random.Random, count: int) -> list[list[int]]:
    o.reset()
    holder = o.alloc(0x64100)
    o.stub(HOLDER_ALLOCATE, eax=holder)
    o.stub(HOLDER_FREE)
    no_circles = o.alloc(16, bytes.fromhex("c21000"))
    vtable = o.alloc(0x800)
    o.write_u32(vtable + 0x100, GET_POS)
    o.write_u32(vtable + 0x7C0, no_circles)
    o.stub(ALTITUDE, st0=0.0)
    building = o.alloc(0x40)
    o.write_u32(building, vtable)
    end = o.alloc(8)
    rows = []
    for _ in range(count):
        place = (rng.randint(0x1000000, 0x1F000000), rng.randint(0x1000000, 0x1F000000))
        o.write_u32(building + 0x14, place[0], place[1], 0)
        o.call(TO_POINT, args=[end, building + 0x14])
        point = o.read_u32(end, 2)
        other = (f(u2f(point[0]) + rng.uniform(-50.0, 50.0)), f(u2f(point[1]) + rng.uniform(-50.0, 50.0)))
        o.call(END_CLEAR, args=[end, building, f32(other[0]), f32(other[1])])
        rows.append([*place, *point, f2u(other[0]), f2u(other[1]), *o.read_u32(end, 2)])
    return rows


def point_cases(o: Oracle, rng: random.Random, count: int) -> list[list[int]]:
    o.reset()
    o.stub(ALTITUDE, st0=0.0)
    place = o.alloc(16)
    point = o.alloc(8)
    other = o.alloc(8)
    back = o.alloc(16)
    rows = []
    for i in range(count):
        whole = (rng.randint(0, 0x2000000), rng.randint(0, 0x2000000)) if i % 4 else (rng.randint(0, 0x7FFF), 0x1FFFFFF)
        o.write_u32(place, whole[0], whole[1], 0)
        o.call(TO_POINT, args=[point, place])
        metres = o.read_u32(point, 2)
        jitter = (f(u2f(metres[0]) + rng.uniform(-30.0, 30.0)), f(u2f(metres[1]) + rng.uniform(-30.0, 30.0)))
        o.write_f32(other, *jitter)
        o.call(TO_PLACE, args=[back, other])
        returned = o.read_u32(back, 2)
        r = o.call(RANGE, this=point, args=[other])
        rows.append([*whole, *metres, f2u(jitter[0]), f2u(jitter[1]), *returned, r.st0f])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x63F00)
    with Oracle() as o:
        columns = ["start_x", "start_z", "goal_x", "goal_z", "a_x", "a_z", "b_x", "b_z", "c_x", "c_z", "d_x", "d_z",
                   "c0_x", "c0_z", "c0_r", "c1_x", "c1_z", "c1_r", "way0", "way1", "count"]
        columns += [f"n{i}_{axis}" for i in range(MOST_NODES) for axis in ("x", "z")]
        n = write_pinned(out / "footpath_route_nodes.txt", "The nodes a found route gives a footpath",
                         ["Legs: start to a heading circle 0, the arc a to b, b to c heading circle 1, the arc c to d,",
                          "then d to the goal. The nodes laid between the path's two ends, in whole map units."],
                         columns, route_cases(o, rng, 400))
        print(f"footpath_route_nodes.txt: {n} rows")
        n = write_pinned(out / "footpath_end_clear.txt", "A path's end moved out of one circle of its building", [],
                         ["end_x", "end_z", "building_x", "building_z", "centre_x", "centre_z", "radius", "moved_x",
                          "moved_z"], end_clear_cases(o, rng, 1000))
        print(f"footpath_end_clear.txt: {n} rows")
        n = write_pinned(out / "footpath_off_middle.txt", "A path's end on its building's place moved off it", [],
                         ["place_x", "place_z", "end_x", "end_z", "other_x", "other_z", "moved_x", "moved_z"],
                         off_middle_cases(o, rng, 300))
        print(f"footpath_off_middle.txt: {n} rows")
        n = write_pinned(out / "footpath_points.txt", "Whole map units to metres and back, and the range", [],
                         ["place_x", "place_z", "point_x", "point_z", "other_x", "other_z", "back_x", "back_z",
                          "range"], point_cases(o, rng, 1000))
        print(f"footpath_points.txt: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
