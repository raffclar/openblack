#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of a creature's walk along its route, in test/oracle/data:

- creature_route_place.txt: the place and the heading at a distance along one leg of a route (a straight leg or an
  arc round a circle, either way), and the length of the leg. The game's own functions run as they are.
- creature_walk_turn.txt: one game turn of a walk along a route of two legs: the speed caps, the slope, the speed up,
  the move along the route and the stop at a sharp turn. The walk, the move along the route and the place on a leg run
  as they are. The ground height, the gait, the stand pose, the check of where the creature stands, the start chooser
  and the place of the body are stubbed.
- creature_walk_gait.txt: the blend of the stand, the walk and the run for one turn. It runs as it is, with a table of
  made-up clips in place of the species' clips.
- creature_walk_start.txt: the start towards a point: a turn on the spot, a step or a walk straight on, the heading to
  the point, the side, and the length and the move of the step. It runs as it is, with the clips from a table, the
  real blend of the two steps (with no bones) and the real start of a turn.
- creature_walk_step.txt: one game turn of a step off into the walk. The move phases run as they are in phase 5, with
  the move along the route and the place on a leg. The walk that follows, the start chooser, the ground height, the
  sounds of the clip, the stand pose, the place of the body and the check of where the creature stands are stubbed.

Usage: python tools/oracle/cases/creature_walk_planner_cases.py [output folder]   (default: test/oracle/data)
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

# The game's code (BW1W120, runblack.exe 1.20)
PLACE_ON_LEG = 0x864BA0  # thiscall on the follower (point out, heading out, distance), RET 0xC
LEG_LENGTH = 0x8691A0  # thiscall on a leg (follower), RET 4, ST0
WALK_TURN = 0x491180  # thiscall on the 3D creature, no arguments
MOVE_PHASES = 0x490960  # thiscall on the 3D creature, no arguments
GAIT = 0x48D940  # thiscall on the 3D creature (distance), RET 4
START = 0x483AB0  # thiscall on the 3D creature (from, to), RET 8, EAX the phase
GET_ANIM = 0x619650  # thiscall (clip, flag), RET 8: replaced by a read of a clip table
ALTITUDE = 0x803090  # thiscall on a map position, ST0: stubbed
STAND_POSE = 0x48D250  # thiscall: stubbed
CAN_STAND = 0x483870  # cdecl (point), EAX: stubbed, the creature can stand
SET_POS = 0x6182F0  # thiscall (point), RET 4: stubbed
HOLDER_EMPTY = 0x83B330  # thiscall: stubbed
CHECK_SOUNDS = 0x617970  # thiscall (clip, from, to), RET 0xC: stubbed
ALLOCATE = 0x7DB790  # cdecl (size, file, line): hooked
MEM_ALLOC = 0x842CE0  # cdecl (size): the 3D engine's allocator, hooked
FREE = 0x7AEE98  # cdecl (block): stubbed
CLIP_FREE = 0x85E490  # thiscall: stubbed
SSE2_FLAG = 0xE83A20  # the C library's flag that selects the SSE2 path of its float to int; 0 is the 64-bit path that
# openblack copies on every target (deviations row 177)
QUARTER_TURN = 0xFA51D4  # the float quarter turn of the leg heading, which a start-up initialiser writes as pi x 0.5

# The 3D creature's fields
POS = 0x78
HEADING = 0x84
SCALE = 0x94
CLIP_A = 0x47C0
CLIP_B = 0x47C4
TIME_A = 0x47D0
TIME_B = 0x47D4
WEIGHT_A = 0x47E0
WEIGHT_B = 0x47E4
CREATURE = 0x4834
WALK_SPEED_NOW = 0x4838
ROUTE_HEADING = 0x4840
WALK_SPEED = 0x4844
RUN_SPEED = 0x4848
TURN_MS = 0x48B4
SPIN_MS = 0x48BC
BREATH = 0x4988
PHASE = 0x5190
FOLLOWER = 0x5198
SIDE = 0x521C
BLEND = 0x5220
RADIUS = 0x5228

# The follower's fields
CIRCLES = 0x8  # 20 bytes each: x, z, radius
F_POS = 0x6402C
F_SPEED = 0x64034
F_TRAVELLED = 0x64038
F_TOP = 0x6403C
F_HEADING = 0x64040
F_DEST = 0x64044
F_MOVED = 0x64050
F_STATE = 0x64054
F_NODE = 0x64070
F_PLAN = 0x6408C
FOLLOWER_SIZE = 0x64100

# A leg of a route
N_FROM = 0x0
N_TO = 0x8
N_CIRCLE = 0x10
N_KIND = 0x1C
N_NEXT = 0x24
N_ARC = 0x28
ARC_CIRCLE = 0x10
ARC_WAY = 0x14
NODE_SIZE = 0x30

STAND_CLIPS = 16  # clips 0..15 of the made-up species
CLIP_SIZE = 0x40
CLIP_DURATION = 0x0
CLIP_STRIDE = 0xC
CLIP_MOVE = 0x10
CLIP_FRAMES = 0x1C
SENTINEL = 0x7FC0DEAD  # a NaN that no result can be


def fbits(value: float) -> int:
    return f2u(value)


class Walk:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        # The oracle does not run the start-up initialisers: write the quarter turn as the initialiser does
        o.write_u32(QUARTER_TURN, f2u(u2f(0x40490FDB) * 0.5))
        o.write_u32(SSE2_FLAG, 0)
        o.stub(ALTITUDE, st0=0.0)
        o.stub(STAND_POSE)
        o.stub(CAN_STAND, eax=1)
        o.stub(SET_POS, pop=4)
        o.stub(HOLDER_EMPTY)
        o.stub(CHECK_SOUNDS, pop=0xC)
        o.stub(FREE)
        o.stub(CLIP_FREE)
        o.hook_alloc(ALLOCATE)
        o.hook_alloc(MEM_ALLOC)
        # The clip table: GetAnim(clip, flag) reads the clip from the table
        self.table = o.alloc(4 * STAND_CLIPS)
        self.clips = [o.alloc(CLIP_SIZE) for _ in range(STAND_CLIPS)]
        self.base_pose = o.alloc(0x10)
        for index, clip in enumerate(self.clips):
            o.write_u32(self.table + 4 * index, clip)
            o.write_u32(clip + 0x34, self.base_pose)
        # mov eax, [esp + 4]; mov eax, [eax * 4 + table]; ret 8
        o.write(GET_ANIM, bytes([0x8B, 0x44, 0x24, 0x04, 0x8B, 0x04, 0x85]) + struct.pack("<I", self.table) +
                bytes([0xC2, 0x08, 0x00]))
        self.creature = o.alloc(0x5300)
        self.follower = o.alloc(FOLLOWER_SIZE)
        self.plan = o.alloc(0x80)
        self.route = o.alloc(0x10)
        self.nodes = [o.alloc(NODE_SIZE) for _ in range(2)]
        self.arcs = [o.alloc(0x20) for _ in range(2)]
        o.write_u32(self.creature + FOLLOWER, self.follower)
        o.write_u32(self.plan + 0x68, self.route)

    # A leg: (from, to, arc circle or None, way); circles are (x, z, radius)
    def lay_leg(self, index: int, leg: dict, heads_for: int) -> None:
        o = self.o
        node = self.nodes[index]
        o.write(node, bytes(NODE_SIZE))
        o.write_u32(node + N_FROM, *leg["from"])
        o.write_u32(node + N_TO, *leg["to"])
        o.write_u32(node + N_CIRCLE, heads_for & 0xFFFFFFFF)
        if leg.get("circle") is not None:
            arc = self.arcs[index]
            o.write(arc, bytes(0x20))
            o.write_u32(arc + ARC_CIRCLE, leg["circle"])
            o.write_u32(arc + ARC_WAY, leg["way"])
            o.write_u32(node + N_ARC, arc)
            o.write_u32(node + N_KIND, 1)

    def lay_circles(self, circles: list[tuple[int, int, int]]) -> None:
        for index, circle in enumerate(circles):
            self.o.write_u32(self.follower + CIRCLES + 20 * index, *circle)

    def place(self, leg: dict, circles: list[tuple[int, int, int]], distance: int) -> list[int]:
        o = self.o
        self.lay_circles(circles)
        self.lay_leg(0, leg, -1)
        o.write_u32(self.follower + F_NODE, self.nodes[0])
        out = o.alloc(0x10)
        o.write_u32(out, SENTINEL, SENTINEL, SENTINEL)
        o.call(PLACE_ON_LEG, this=self.follower, args=[out, out + 8, f32.of_bits(distance)])
        length = o.call(LEG_LENGTH, this=self.nodes[0], args=[self.follower]).st0f
        return [*o.read_u32(out, 3), length]

    def walk_turn(self, case: dict) -> list[int]:
        o = self.o
        c, f = self.creature, self.follower
        o.write(c, bytes(0x5300))
        o.write_u32(c + FOLLOWER, f)
        o.write(f + F_POS, bytes(0x100))
        self.lay_circles(case["circles"])
        legs = case["legs"]
        for index, leg in enumerate(legs):
            heads_for = legs[index + 1]["circle"] if index + 1 < len(legs) and legs[index + 1].get("circle") is not None else -1
            self.lay_leg(index, leg, heads_for)
            o.write_u32(self.nodes[index] + N_NEXT, self.nodes[index + 1] if index + 1 < len(legs) else 0)
        o.write_u32(self.route, self.nodes[0])
        o.write_u32(self.route + 4, self.nodes[len(legs) - 1])
        o.write_u32(f + F_NODE, self.nodes[0])
        o.write_u32(f + F_PLAN, self.plan)
        o.write_u32(f + F_STATE, 3)
        o.write_u32(f + F_SPEED, case["speed"])
        o.write_u32(f + F_TRAVELLED, case["travelled"])
        o.write_u32(f + F_TOP, case["top"])
        o.write_u32(f + F_HEADING, case["route_heading"])
        o.write_u32(f + F_DEST, *legs[-1]["to"])
        o.write_u32(c + POS, case["pos"][0], case["pos_y"], case["pos"][1])
        o.write_u32(c + HEADING, case["heading"])
        o.write_u32(c + RADIUS, case["radius"])
        o.write_u32(c + PHASE, 6)
        o.stub(ALTITUDE, st0=f32.of_bits(case["altitude"]))
        o.stub(START, eax=9, pop=8)
        o.stub(GAIT, pop=4)
        o.call(WALK_TURN, this=c)
        node = o.read_u32(f + F_NODE)[0]
        node_index = self.nodes.index(node) if node in self.nodes else 0xFFFFFFFF
        return [o.read_u32(f + F_SPEED)[0], o.read_u32(f + F_TRAVELLED)[0], node_index, *o.read_u32(f + F_POS, 2),
                o.read_u32(f + F_HEADING)[0], o.read_u32(c + POS)[0], o.read_u32(c + POS + 8)[0],
                o.read_u32(c + HEADING)[0], o.read_u32(c + PHASE)[0]]

    def set_clip(self, index: int, duration: int, stride: int = 0, move: tuple[int, int, int] = (0, 0, 0),
                 frames: int = 2) -> None:
        o = self.o
        clip = self.clips[index]
        o.write(clip, bytes(0x34))
        o.write_u32(clip + CLIP_DURATION, duration & 0xFFFFFFFF)
        o.write_u32(clip + CLIP_STRIDE, stride)
        o.write_u32(clip + CLIP_MOVE, *move)
        o.write_u32(clip + CLIP_FRAMES, frames)

    def clip_index(self, pointer: int) -> int:
        return self.clips.index(pointer) if pointer in self.clips else 0xFFFFFFFF

    def gait(self, case: dict) -> list[int]:
        o = self.o
        c = self.creature
        o.write(c, bytes(0x5300))
        for index in range(3):
            self.set_clip(index, case["durations"][index], case["strides"][index])
        o.write_u32(c + SCALE, case["scale"])
        o.write_u32(c + WALK_SPEED_NOW, case["speed"])
        o.write_u32(c + WALK_SPEED, case["walk"])
        o.write_u32(c + RUN_SPEED, case["run"])
        o.write_u32(c + BREATH, case["breath"])
        o.write_u32(c + TIME_B, case["walk_time"])
        o.write_u32(c + CLIP_A, SENTINEL, SENTINEL)
        o.write_u32(c + TIME_A, SENTINEL)
        o.write_u32(c + WEIGHT_A, SENTINEL, SENTINEL)
        o.call(GAIT, this=c, args=[f32.of_bits(case["distance"])])
        clip_a, clip_b = o.read_u32(c + CLIP_A, 2)
        return [self.clip_index(clip_a), self.clip_index(clip_b), *o.read_u32(c + TIME_A, 2), *o.read_u32(c + WEIGHT_A, 2)]

    def start(self, case: dict) -> list[int]:
        o = self.o
        c = self.creature
        o.write(c, bytes(0x5300))
        self.set_clip(0, 2000)
        for index in range(3, 16):
            self.set_clip(index, case["durations"][index], 0, case["moves"][index], case["frames"][index])
        for index in case["missing"]:
            o.write_u32(self.table + 4 * index, 0)
        o.write_u32(c + HEADING, case["heading"])
        o.write_u32(c + SCALE, case["scale"])
        o.write_u32(c + WALK_SPEED, case["walk"])
        points = o.alloc(0x20)
        o.write_u32(points, case["from"][0], 0, case["from"][1])
        o.write_u32(points + 0x10, case["to"][0], 0, case["to"][1])
        phase = o.call(START, this=c, args=[points, points + 0x10]).eax
        for index in case["missing"]:
            o.write_u32(self.table + 4 * index, self.clips[index])
        blend = o.read_u32(c + BLEND)[0]
        duration, move = SENTINEL, [SENTINEL] * 3
        if phase == 5 and blend != 0:
            duration = o.read_u32(blend + CLIP_DURATION)[0]
            move = o.read_u32(blend + CLIP_MOVE, 3)
        return [phase, o.read_u32(c + ROUTE_HEADING)[0], o.read_u32(c + SIDE)[0], duration, *move]

    def step(self, case: dict) -> list[int]:
        o = self.o
        c, f = self.creature, self.follower
        o.write(c, bytes(0x5300))
        o.write_u32(c + FOLLOWER, f)
        o.write(f + F_POS, bytes(0x100))
        legs = case["legs"]
        for index, leg in enumerate(legs):
            self.lay_leg(index, leg, -1)
            o.write_u32(self.nodes[index] + N_NEXT, self.nodes[index + 1] if index + 1 < len(legs) else 0)
        o.write_u32(self.route, self.nodes[0])
        o.write_u32(self.route + 4, self.nodes[len(legs) - 1])
        o.write_u32(f + F_NODE, self.nodes[0])
        o.write_u32(f + F_PLAN, self.plan)
        o.write_u32(f + F_STATE, 3)
        o.write_u32(f + F_SPEED, case["speed"])
        o.write_u32(f + F_TRAVELLED, case["travelled"])
        o.write_u32(f + F_HEADING, case["route_heading"])
        o.write_u32(f + F_DEST, *legs[-1]["to"])
        self.set_clip(1, case["walk_ms"])
        step = self.clips[11]
        self.set_clip(11, case["step_ms"], 0, case["move"], 20)
        o.write_u32(c + BLEND, step)
        o.write_u32(c + HEADING, case["heading"])
        o.write_u32(c + ROUTE_HEADING, case["step_heading"])
        o.write_u32(c + SCALE, case["scale"])
        o.write_u32(c + TIME_A, case["time"])
        o.write_u32(c + TIME_B, SENTINEL)
        o.write_u32(c + TURN_MS, case["turn_ms"])
        o.write_u32(c + SIDE, case["side"])
        o.write_u32(c + PHASE, 5)
        o.write_u32(c + POS, SENTINEL, SENTINEL, SENTINEL)
        o.stub(ALTITUDE, st0=0.0)
        o.stub(START, eax=9, pop=8)
        o.stub(WALK_TURN)
        o.call(MOVE_PHASES, this=c)
        node = o.read_u32(f + F_NODE)[0]
        node_index = self.nodes.index(node) if node in self.nodes else 0xFFFFFFFF
        return [o.read_u32(c + PHASE)[0], o.read_u32(c + HEADING)[0], o.read_u32(c + ROUTE_HEADING)[0],
                o.read_u32(c + TIME_A)[0], o.read_u32(c + TIME_B)[0], o.read_u32(f + F_SPEED)[0],
                o.read_u32(f + F_TRAVELLED)[0], node_index, *o.read_u32(f + F_POS, 2), o.read_u32(f + F_HEADING)[0],
                o.read_u32(c + POS)[0], o.read_u32(c + POS + 8)[0]]


def rand_point(rng: random.Random) -> tuple[float, float]:
    return (rng.uniform(500.0, 4500.0), rng.uniform(500.0, 4500.0))


def line_leg(start: tuple[float, float], end: tuple[float, float]) -> dict:
    return {"from": (fbits(start[0]), fbits(start[1])), "to": (fbits(end[0]), fbits(end[1]))}


def arc_leg(centre: tuple[float, float], radius: float, a0: float, a1: float, way: int) -> tuple[dict, tuple]:
    start = (centre[0] + radius * math.cos(a0), centre[1] + radius * math.sin(a0))
    end = (centre[0] + radius * math.cos(a1), centre[1] + radius * math.sin(a1))
    leg = line_leg(start, end)
    leg["circle"] = 0
    leg["way"] = way
    return leg, (fbits(centre[0]), fbits(centre[1]), fbits(radius))


def place_cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for _ in range(1500):
        if rng.random() < 0.5:
            a = rand_point(rng)
            length = rng.choice([rng.uniform(0.0, 0.002), rng.uniform(0.0, 5.0), rng.uniform(0.0, 200.0)])
            angle = rng.uniform(-math.pi, math.pi)
            b = (a[0] + length * math.cos(angle), a[1] + length * math.sin(angle))
            leg = line_leg(a, b)
            rows.append([0, *leg["from"], *leg["to"], 0, 0, 0, 0, fbits(rng.uniform(0.0, length + 0.5))])
        else:
            centre = rand_point(rng)
            radius = rng.uniform(0.5, 60.0)
            a0 = rng.uniform(-math.pi, math.pi)
            a1 = rng.uniform(-math.pi, math.pi)
            way = rng.choice([1, 2])
            leg, circle = arc_leg(centre, radius, a0, a1, way)
            rows.append([1, *leg["from"], *leg["to"], *circle, way, fbits(rng.uniform(0.0, 2.0 * math.pi * radius))])
    return rows


def leg_of(row: list[int]) -> tuple[dict, list]:
    leg = {"from": (row[1], row[2]), "to": (row[3], row[4])}
    circles = []
    if row[0] == 1:
        leg["circle"] = 0
        leg["way"] = row[8]
        circles = [(row[5], row[6], row[7])]
    return leg, circles


def walk_cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for _ in range(1500):
        start = rand_point(rng)
        length = rng.choice([rng.uniform(0.5, 6.0), rng.uniform(5.0, 120.0)])
        angle = rng.uniform(-math.pi, math.pi)
        corner = (start[0] + length * math.cos(angle), start[1] + length * math.sin(angle))
        kind = rng.choice(["last", "line", "arc"])
        travelled = rng.uniform(0.0, length)
        heading_of_leg = math.atan2(corner[0] - start[0], -(corner[1] - start[1]))
        if kind == "arc":
            radius = rng.uniform(0.5, 40.0)
            way = rng.choice([1, 2])
            # The arc starts at the corner, on a circle beside the line
            side = 1.0 if way == 2 else -1.0
            normal = (math.cos(angle + side * math.pi / 2), math.sin(angle + side * math.pi / 2))
            centre = (corner[0] + normal[0] * radius, corner[1] + normal[1] * radius)
            a0 = math.atan2(corner[1] - centre[1], corner[0] - centre[0])
            sweep = rng.uniform(0.1, 3.0) * (1.0 if way == 2 else -1.0)
            second, circle = arc_leg(centre, radius, a0, a0 + sweep, way)
            second["from"] = (fbits(corner[0]), fbits(corner[1]))
            circles = [circle]
            second_kind = 1
        else:
            turn = rng.choice([rng.uniform(-0.4, 0.4), rng.uniform(-math.pi, math.pi)])
            far = rng.uniform(1.0, 80.0)
            end = (corner[0] + far * math.cos(angle + turn), corner[1] + far * math.sin(angle + turn))
            second = line_leg(corner, end)
            circles = []
            second_kind = 0
        first = line_leg(start, corner)
        top = rng.uniform(2.0, 30.0)
        speed = rng.choice([0.0, rng.uniform(0.0, top * 1.2)])
        heading = heading_of_leg + rng.choice([0.0, rng.uniform(-0.6, 0.6)])
        heading = math.atan2(math.sin(heading), math.cos(heading))
        pos_y = rng.uniform(0.0, 50.0)
        altitude = pos_y + rng.choice([0.0, rng.uniform(-8.0, 8.0), rng.uniform(-40.0, 40.0)])
        radius_body = rng.uniform(1.0, 20.0)
        circle = circles[0] if circles else (0, 0, 0)
        rows.append([1 if kind != "last" else 0, second_kind, *first["from"], *first["to"], *second["from"], *second["to"],
                     *circle, second.get("way", 0), fbits(speed), fbits(travelled), fbits(top),
                     fbits(heading_of_leg), fbits(start[0]), fbits(start[1]), fbits(pos_y), fbits(heading),
                     fbits(radius_body), fbits(altitude)])
    return rows


def walk_case(row: list[int]) -> dict:
    first = {"from": (row[2], row[3]), "to": (row[4], row[5])}
    legs = [first]
    circles = []
    if row[0] == 1:
        second = {"from": (row[6], row[7]), "to": (row[8], row[9])}
        if row[1] == 1:
            second["circle"] = 0
            second["way"] = row[13]
            circles = [(row[10], row[11], row[12])]
        legs.append(second)
    return {"legs": legs, "circles": circles, "speed": row[14], "travelled": row[15], "top": row[16],
            "route_heading": row[17], "pos": (row[18], row[19]), "pos_y": row[20], "heading": row[21],
            "radius": row[22], "altitude": row[23]}


def gait_cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for _ in range(1500):
        walk = rng.uniform(2.0, 20.0)
        run = walk * rng.uniform(1.5, 3.0)
        speed = rng.choice([0.0, 1e-5, rng.uniform(0.0, walk), rng.uniform(walk, run * 1.25), walk])
        durations = [rng.randint(500, 4000), rng.randint(400, 3000), rng.randint(300, 2000)]
        strides = [0.0, rng.uniform(20.0, 120.0), rng.uniform(40.0, 250.0)]
        rows.append([fbits(speed), fbits(walk), fbits(run), fbits(speed * 0.1 * rng.uniform(0.5, 1.5)),
                     fbits(rng.uniform(0.05, 4.0)), *durations, *[fbits(s) for s in strides],
                     rng.randint(0, durations[1] - 1), fbits(rng.random())])
    return rows


def start_cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for _ in range(1500):
        start = rand_point(rng)
        distance = rng.choice([rng.uniform(0.01, 20.0), rng.uniform(0.0, 300.0)])
        angle = rng.uniform(-math.pi, math.pi)
        to = (start[0] + distance * math.cos(angle), start[1] + distance * math.sin(angle))
        moves = []
        frames = []
        for _ in range(6):
            # The steps move forward and to their side; the 180 degree steps move back
            moves.append([fbits(rng.uniform(-30.0, 30.0)), fbits(rng.uniform(-2.0, 2.0)), fbits(rng.uniform(-60.0, 10.0))])
            frames.append(rng.randint(2, 60))
        missing = rng.choice([0, 0, 0, rng.randint(10, 15)])
        rows.append([fbits(start[0]), fbits(start[1]), fbits(to[0]), fbits(to[1]), fbits(rng.uniform(-math.pi, math.pi)),
                     fbits(rng.uniform(0.1, 3.0)), fbits(rng.uniform(2.0, 20.0)), missing,
                     *[m for move in moves for m in move], *frames])
    return rows


def start_case(row: list[int]) -> dict:
    moves = [(0, 0, 0)] * 16
    frames = [2] * 16
    for k in range(6):
        moves[10 + k] = tuple(row[8 + 3 * k:11 + 3 * k])
        frames[10 + k] = row[26 + k]
    return {"from": (row[0], row[1]), "to": (row[2], row[3]), "heading": row[4], "scale": row[5], "walk": row[6],
            "missing": [row[7]] if row[7] else [], "durations": [1000] * 16, "moves": moves, "frames": frames}


def step_cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for _ in range(1500):
        start = rand_point(rng)
        length = rng.uniform(0.5, 80.0)
        angle = rng.uniform(-math.pi, math.pi)
        corner = (start[0] + length * math.cos(angle), start[1] + length * math.sin(angle))
        turn = rng.choice([rng.uniform(-0.3, 0.3), rng.uniform(-math.pi, math.pi)])
        far = rng.uniform(1.0, 80.0)
        end = (corner[0] + far * math.cos(angle + turn), corner[1] + far * math.sin(angle + turn))
        first, second = line_leg(start, corner), line_leg(corner, end)
        route_heading = math.atan2(corner[0] - start[0], -(corner[1] - start[1]))
        step_heading = route_heading + rng.choice([0.0, rng.uniform(-0.7, 0.7)])
        step_ms = rng.choice([rng.randint(150, 2500), rng.randint(-50, 120)])
        time = rng.randint(0, max(step_ms, 1) + 50)
        rows.append([*first["from"], *first["to"], *second["to"], fbits(rng.uniform(0.0, 12.0)),
                     fbits(rng.uniform(0.0, length)), fbits(route_heading), fbits(step_heading),
                     fbits(rng.uniform(-math.pi, math.pi)), fbits(rng.uniform(0.1, 3.0)), step_ms & 0xFFFFFFFF, time,
                     rng.choice([100, 100, 50, 33]), rng.choice([1, 2]), rng.randint(400, 3000),
                     fbits(rng.uniform(-30.0, 30.0)), fbits(rng.uniform(-2.0, 2.0)), fbits(rng.uniform(-60.0, 10.0))])
    return rows


def step_case(row: list[int]) -> dict:
    return {"legs": [{"from": (row[0], row[1]), "to": (row[2], row[3])},
                     {"from": (row[2], row[3]), "to": (row[4], row[5])}],
            "speed": row[6], "travelled": row[7], "route_heading": row[8], "step_heading": row[9],
            "heading": row[10], "scale": row[11], "step_ms": row[12], "time": row[13], "turn_ms": row[14],
            "side": row[15], "walk_ms": row[16], "move": (row[17], row[18], row[19])}


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x491180)
    with Oracle() as oracle:
        walk = Walk(oracle)
        rows = []
        for case in place_cases(rng):
            leg, circles = leg_of(case)
            rows.append([*case, *walk.place(leg, circles, case[9])])
        n = write_pinned(out / "creature_route_place.txt", "The place and the heading along one leg of a route",
                         ["kind 0 is a straight leg, 1 an arc round the circle (x, z, radius) going way (1 clockwise,",
                          "2 anticlockwise). The place and the heading are 7FC0DEAD when the game leaves them as they",
                          "were (a straight leg too short to measure). length is the leg's length."],
                         ["kind", "from_x", "from_z", "to_x", "to_z", "circle_x", "circle_z", "radius", "way",
                          "distance", "place_x", "place_z", "heading", "length"], rows)
        print(f"creature_route_place: {n} rows")

        oracle.reset()
        walk = Walk(oracle)
        rows = [[*case, *walk.walk_turn(walk_case(case))] for case in walk_cases(rng)]
        n = write_pinned(out / "creature_walk_turn.txt", "One game turn of a creature's walk along its route",
                         ["The route has one leg (more 0) or two (more 1); the second is a straight leg (second_arc 0)",
                          "or an arc round the circle going way. The creature stands at (pos_x, pos_y, pos_z) with its",
                          "heading and radius; the ground ahead is at altitude. The follower has its speed, the distance",
                          "travelled along the first leg, its top speed and its heading. The results are the follower's",
                          "speed, travelled, leg (1 is the second), place and heading, the creature's place and heading,",
                          "and the phase (9: the start chooser ran after a sharp turn)."],
                         ["more", "second_arc", "a_x", "a_z", "b_x", "b_z", "c_x", "c_z", "d_x", "d_z", "circle_x",
                          "circle_z", "radius", "way", "speed", "travelled", "top", "route_heading", "pos_x", "pos_z",
                          "pos_y", "heading", "body_radius", "altitude", "speed_after", "travelled_after", "leg",
                          "follower_x", "follower_z", "follower_heading", "creature_x", "creature_z", "heading_after",
                          "phase"], rows)
        print(f"creature_walk_turn: {n} rows")

        oracle.reset()
        walk = Walk(oracle)
        rows = []
        for case in gait_cases(rng):
            rows.append([*case, *walk.gait({"speed": case[0], "walk": case[1], "run": case[2], "distance": case[3],
                                             "scale": case[4], "durations": case[5:8], "strides": case[8:11],
                                             "walk_time": case[11], "breath": case[12]})])
        n = write_pinned(out / "creature_walk_gait.txt", "The blend of a creature's stand, walk and run for one turn",
                         ["The clips 0 (stand), 1 (walk) and 2 (run) have the durations and strides. The results are",
                          "the two clips (FFFFFFFF: not changed), their times and their weights (7FC0DEAD: not",
                          "changed)."],
                         ["speed", "walk", "run", "distance", "scale", "stand_ms", "walk_ms", "run_ms", "stand_stride",
                          "walk_stride", "run_stride", "walk_time", "breath", "clip_a", "clip_b", "time_a", "time_b",
                          "weight_a", "weight_b"], rows)
        print(f"creature_walk_gait: {n} rows")

        oracle.reset()
        walk = Walk(oracle)
        rows = [[*case, *walk.start(start_case(case))] for case in start_cases(rng)]
        n = write_pinned(out / "creature_walk_start.txt", "The start of a creature's walk towards a point",
                         ["The creature at from with its heading, scale and walking speed starts towards to. The steps",
                          "10 to 15 have the moves (x, y, z) and frame counts; missing is a step that the species does",
                          "not have (0: none). The results are the phase (4 turn, 5 step, 6 walk), the heading to the",
                          "point, the side (1: the steps 13 to 15), and for a step its length and blended move."],
                         ["from_x", "from_z", "to_x", "to_z", "heading", "scale", "walk", "missing",
                          *[f"move{k}_{a}" for k in range(10, 16) for a in "xyz"], *[f"frames{k}" for k in range(10, 16)],
                          "phase", "route_heading", "side", "step_ms", "step_x", "step_y", "step_z"], rows)
        print(f"creature_walk_start: {n} rows")

        oracle.reset()
        walk = Walk(oracle)
        rows = [[*case, *walk.step(step_case(case))] for case in step_cases(rng)]
        n = write_pinned(out / "creature_walk_step.txt", "One game turn of a creature's step off into its walk",
                         ["The route has two straight legs a-b and b-c. The follower has its speed, travelled and",
                          "heading. The step has its heading, length (ms), time (ms), side and move; the turn plays",
                          "turn_ms; the walk lasts walk_ms. The results are the phase (6: the walk started, 9: the",
                          "start chooser ran), the creature's heading and step heading, the step time and the walk time",
                          "(7FC0DEAD: not set), the follower's speed, travelled, leg, place and heading, and the",
                          "creature's place (7FC0DEAD: not set; the step places the body through a stubbed call)."],
                         ["a_x", "a_z", "b_x", "b_z", "c_x", "c_z", "speed", "travelled", "route_heading",
                          "step_heading", "heading", "scale", "step_ms", "time", "turn_ms", "side", "walk_ms", "move_x",
                          "move_y", "move_z", "phase", "heading_after", "step_heading_after", "time_after", "walk_time",
                          "speed_after", "travelled_after", "leg", "follower_x", "follower_z", "follower_heading",
                          "creature_x", "creature_z"], rows)
        print(f"creature_walk_step: {n} rows")


if __name__ == "__main__":
    main()
