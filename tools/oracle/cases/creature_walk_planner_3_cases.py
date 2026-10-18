#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes pinned data of a creature that something tells to stop while it walks, in test/oracle/data:

- creature_soft_stop.txt: the route follower's stop. The game's own function runs as it is, with the creature's own
  join distance (a turn on the spot, a step, or nothing) as the follower's stop distance. A follower on its way ends
  the leg that it is on where its speed lets it slow to a stand, and no leg after it is walked. A follower that
  searches from where it stands stands. Other states do not change.

Usage: python tools/oracle/cases/creature_walk_planner_3_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
STOP = 0x863EF0  # thiscall on the follower, no arguments
JOIN_AHEAD = 0x47F280  # cdecl (creature), ST0: the follower's stop distance callback for a creature
FREE = 0x7AEE98  # cdecl (block): stubbed
QUARTER_TURN = 0xFA51D4  # the float quarter turn of the leg heading, which a start-up initialiser writes

# The creature's link to its 3D body: [[creature + 0x160] + 0x58]
C_LINK = 0x160
L_BODY = 0x58
# The 3D creature's fields
SCALE = 0x94
PHASE = 0x5190
FOLLOWER = 0x5198
BLEND = 0x5220
CLIP_MOVE = 0x10

# The follower's fields
CIRCLES = 0x8  # 20 bytes each: x, z, radius
F_SPEED = 0x64034
F_TRAVELLED = 0x64038
F_DEST = 0x64044
F_STATE = 0x64054
F_USER = 0x64060
F_CALLBACK = 0x6406C
F_NODE = 0x64070
F_PLAN = 0x6408C
F_SEARCHES = 0x640B8
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
SENTINEL = 0x7FC0DEAD  # a NaN that no result can be


def fb(value: float) -> int:
    return f2u(value)


def cases(rng: random.Random) -> list[list[int]]:
    rows = []
    for index in range(1500):
        state = (3, 3, 3, 4, 4, 2, 1, 0)[index % 8]
        phase = (6, 6, 4, 5, 5, 0)[rng.randrange(6)]
        move = [fb(rng.uniform(-60.0, 60.0)) for _ in range(3)]
        scale = fb(rng.uniform(0.05, 3.0))
        speed = fb(rng.choice([0.0, rng.uniform(0.0, 3.0), rng.uniform(0.0, 40.0)]))
        if rng.random() < 0.5:
            a = (rng.uniform(500.0, 4500.0), rng.uniform(500.0, 4500.0))
            length = rng.choice([rng.uniform(0.0, 0.05), rng.uniform(0.0, 10.0), rng.uniform(0.0, 200.0)])
            angle = rng.uniform(-math.pi, math.pi)
            b = (a[0] + length * math.cos(angle), a[1] + length * math.sin(angle))
            travelled = fb(rng.uniform(0.0, length))
            rows.append([state, speed, travelled, 0, fb(a[0]), fb(a[1]), fb(b[0]), fb(b[1]), 0, 0, 0, 0, phase, *move,
                         scale])
        else:
            centre = (rng.uniform(500.0, 4500.0), rng.uniform(500.0, 4500.0))
            radius = rng.uniform(0.5, 60.0)
            a0 = rng.uniform(-math.pi, math.pi)
            a1 = rng.uniform(-math.pi, math.pi)
            way = rng.choice([1, 2])
            start = (centre[0] + radius * math.cos(a0), centre[1] + radius * math.sin(a0))
            end = (centre[0] + radius * math.cos(a1), centre[1] + radius * math.sin(a1))
            travelled = fb(rng.uniform(0.0, math.pi * radius))
            rows.append([state, speed, travelled, 1, fb(start[0]), fb(start[1]), fb(end[0]), fb(end[1]),
                         fb(centre[0]), fb(centre[1]), fb(radius), way, phase, *move, scale])
    return rows


class Stop:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.write_u32(QUARTER_TURN, f2u(u2f(0x40490FDB) * 0.5))
        o.stub(FREE)
        self.creature = o.alloc(0x200)
        self.link = o.alloc(0x100)
        self.body = o.alloc(0x5800)
        self.blend = o.alloc(0x40)
        self.follower = o.alloc(FOLLOWER_SIZE)
        self.plan = o.alloc(0x80)
        self.route = o.alloc(0x10)
        self.nodes = [o.alloc(NODE_SIZE) for _ in range(2)]
        self.arc = o.alloc(0x20)

    def run(self, case: list[int]) -> list[int]:
        o = self.o
        c, b, f = self.creature, self.body, self.follower
        (state, speed, travelled, kind, fx, fz, tx, tz, cx, cz, radius, way, phase, mx, my, mz, scale) = case
        o.write(b, bytes(0x5800))
        o.write_u32(c + C_LINK, self.link)
        o.write_u32(self.link + L_BODY, b)
        o.write_u32(b + PHASE, phase)
        o.write_u32(b + BLEND, self.blend)
        o.write_u32(self.blend + CLIP_MOVE, mx, my, mz)
        o.write_u32(b + SCALE, scale)
        o.write_u32(b + FOLLOWER, f)
        o.write(f + 0x64000, bytes(0x100))
        o.write_u32(f + CIRCLES, cx, cz, radius)
        o.write_u32(f + F_SPEED, speed)
        o.write_u32(f + F_TRAVELLED, travelled)
        o.write_u32(f + F_DEST, SENTINEL, SENTINEL)
        o.write_u32(f + F_STATE, state)
        o.write_u32(f + F_USER, c)
        o.write_u32(f + F_CALLBACK, JOIN_AHEAD)
        o.write_u32(f + F_SEARCHES, 0)
        # Planning frees the plan through its destructor: no plan then, as the stop only drops it
        o.write_u32(f + F_PLAN, 0 if state == 2 else self.plan)
        o.write_u32(self.plan + 0x68, self.route)
        o.write_u32(self.route + 4, 0)
        node, after = self.nodes
        o.write(node, bytes(NODE_SIZE))
        o.write(after, bytes(NODE_SIZE))
        o.write_u32(node + N_FROM, fx, fz, tx, tz)
        o.write_u32(node + N_CIRCLE, 0xFFFFFFFF)
        o.write_u32(node + N_NEXT, after)
        o.write_u32(after + N_FROM, tx, tz, tx, tz)
        o.write_u32(after + N_CIRCLE, 0xFFFFFFFF)
        if kind == 1:
            o.write(self.arc, bytes(0x20))
            o.write_u32(self.arc + ARC_CIRCLE, 0)
            o.write_u32(self.arc + ARC_WAY, way)
            o.write_u32(node + N_ARC, self.arc)
            o.write_u32(node + N_KIND, 1)
        o.write_u32(f + F_NODE, node)
        o.call(STOP, this=f)
        to = o.read_u32(node + N_TO, 2)
        dest = o.read_u32(f + F_DEST, 2)
        after_state = o.read_u32(f + F_STATE)[0]
        last = o.read_u32(self.route + 4)[0]
        next_after = o.read_u32(node + N_NEXT)[0]
        cut = 1 if (last == node and next_after == 0) else 0
        return [*to, *dest, after_state, cut]


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x863EF0)
    with Oracle() as oracle:
        stop = Stop(oracle)
        rows = [[*case, *stop.run(case)] for case in cases(rng)]
        n = write_pinned(out / "creature_soft_stop.txt", "The route follower's stop, as the game stops a walk",
                         ["The follower's state (2 planning, 3 walking, 4 walking and searching), its speed and how far",
                          "along the leg it is; the leg (0 straight, 1 an arc round the circle, which way); the",
                          "creature's move phase, step move and scale for its stop distance. The results are the end of",
                          "the leg, the follower's destination, its state after, and 1 when the leg is the last."],
                         ["state", "speed", "travelled", "arc", "from_x", "from_z", "to_x", "to_z", "centre_x",
                          "centre_z", "radius", "way", "phase", "move_x", "move_y", "move_z", "scale", "end_x", "end_z",
                          "dest_x", "dest_z", "state_after", "last"], rows)
        print(f"creature_soft_stop: {n} rows")


if __name__ == "__main__":
    main()
