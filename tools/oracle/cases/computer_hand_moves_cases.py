#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the moves and the throw of a computer god's hand, in test/oracle/data:

- computer_hand_near.txt: a turn of the hand that moves near a place (kind 4) or near a thing (kind 5): the hand's x
  and z after the turn, the y of its target, and the result (0 next, 2 continue)
- computer_hand_throw.txt: the speed at which the hand throws what it holds at a place (kind 7)

The game's float of the hand over the land is a stub, so the y of the hand is not in the data.

Usage: python tools/oracle/cases/computer_hand_moves_cases.py [output folder]   (default: test/oracle/data)
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

# The functions (BW1W120, runblack.exe 1.20); each is thiscall on the hand's action state, arg (step), RET 4
NEAR_PLACE = 0x6531F0
NEAR_OBJECT = 0x653460
THROW = 0x653670
FLOAT_OVER_LAND = 0x6508F0  # thiscall on the action state, RET
TURN_MILLISECONDS = 0xD01A38  # the length of a game turn, 100
NEAR_MINIMUM = 0xD47F48  # 80 * 0.33, made at start-up
NEAR_MINIMUM_BITS = 0x41D33334
# The calls of the throw
STATUS_FROM = 0x603160  # thiscall, RET 4
STATUS_COPY = 0x57C960  # thiscall, RET 4
INTERFACE_STATUS = 0x64A9F0  # thiscall on the player, RET: the status that holds the throw
ARGUMENT_AT = 0x6559A0  # thiscall on the step, RET 4
GET_HAND = 0x5DC8B0  # thiscall, RET; also the stub of the player's vtable call
LET_GO = 0x5DA400  # thiscall, RET 4

# The action state
HAND = 0x50
TARGET = 0x5C
SPEED = 0x68


class Near:
    COLUMNS = ["kind", "hand_x", "hand_y", "hand_z", "target_x", "target_y", "target_z", "speed", "out_x", "out_z",
               "out_target_y", "result"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.write_u32(TURN_MILLISECONDS, 100)
        o.write_u32(NEAR_MINIMUM, NEAR_MINIMUM_BITS)
        o.stub(FLOAT_OVER_LAND, pop=0)
        self.state = o.alloc(0x100)

    def run(self, kind: int, hx: int, hy: int, hz: int, tx: int, ty: int, tz: int, speed: int) -> list[int]:
        o = self.o
        o.write_u32(self.state + HAND, hx, hy, hz, tx, ty, tz, speed)
        result = o.call(NEAR_PLACE if kind == 4 else NEAR_OBJECT, this=self.state, args=[0])
        assert result.popped == 4, result
        out = o.read_u32(self.state + HAND, 6)
        return [out[0], out[2], out[4], result.eax]


def near_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for kind in (4, 5):
        for _ in range(1200):
            hx = rng.uniform(100.0, 5000.0)
            hz = rng.uniform(100.0, 5000.0)
            reach = rng.choice([rng.uniform(0.0, 30.0), rng.uniform(20.0, 100.0), rng.uniform(70.0, 400.0)])
            angle = rng.uniform(-math.pi, math.pi)
            tx = hx + reach * math.cos(angle)
            tz = hz + reach * math.sin(angle)
            speed = rng.choice([90.0, rng.uniform(1.0, 400.0)])
            rows.append((kind, f2u(hx), f2u(rng.uniform(0.0, 200.0)), f2u(hz), f2u(tx), f2u(rng.uniform(0.0, 200.0)),
                         f2u(tz), f2u(speed)))
        # On the target, and straight north, south, east and west of it
        for dx, dz in ((0.0, 0.0), (0.0, 50.0), (0.0, -50.0), (50.0, 0.0), (-50.0, 0.0), (0.0, 26.4), (26.400002, 0.0)):
            rows.append((kind, f2u(1000.0), f2u(5.0), f2u(1000.0), f2u(1000.0 + dx), f2u(7.0), f2u(1000.0 + dz),
                         f2u(90.0)))
    return rows


class Throw:
    COLUMNS = ["hand_x", "hand_y", "hand_z", "place_x", "place_y", "place_z", "velocity_x", "velocity_y", "velocity_z"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.state = o.alloc(0x100)
        self.status = o.alloc(0x100)
        self.argument = o.alloc(0x100)
        holder = o.alloc(0x10)
        o.write_u32(holder + 4, self.argument)
        vtable = o.alloc(0x40)
        o.write_u32(vtable + 0x1C, GET_HAND)
        o.write_u32(self.state, vtable)
        o.stub(STATUS_FROM, eax=self.status, pop=4)
        o.stub(STATUS_COPY, pop=4)
        o.stub(INTERFACE_STATUS, eax=self.status, pop=0)
        o.stub(ARGUMENT_AT, eax=holder, pop=4)
        o.stub(GET_HAND, eax=self.status, pop=0)
        o.stub(LET_GO, pop=4)

    def run(self, hx: int, hy: int, hz: int, px: int, py: int, pz: int) -> list[int]:
        o = self.o
        o.write_u32(self.state + HAND, hx, hy, hz)
        o.write_u32(self.argument + 0x68, px, py, pz)
        result = o.call(THROW, this=self.state, args=[0])
        assert result.popped == 4, result
        return o.read_u32(self.status + 0x44, 3)


def throw_cases(rng: random.Random) -> list[tuple[int, ...]]:
    rows = []
    for _ in range(1500):
        hx = rng.uniform(100.0, 5000.0)
        hz = rng.uniform(100.0, 5000.0)
        hy = rng.uniform(0.0, 250.0)
        reach = rng.choice([rng.uniform(0.5, 20.0), rng.uniform(10.0, 300.0)])
        angle = rng.uniform(-math.pi, math.pi)
        rows.append((f2u(hx), f2u(hy), f2u(hz), f2u(hx + reach * math.cos(angle)), f2u(rng.uniform(0.0, 250.0)),
                     f2u(hz + reach * math.sin(angle))))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x6531F0)
    with Oracle() as oracle:
        near = Near(oracle)
        rows = [[*case, *near.run(*case)] for case in near_cases(rng)]
        n = write_pinned(out / "computer_hand_near.txt", "A turn of a computer god's hand that moves near a place or a thing",
                         ["Function: thiscall on the action state (hand +0x50, target +0x5C, speed +0x68), arg (step);",
                          "kind 4 is the move near a place, kind 5 near a thing. The float over the land is a stub.",
                          "A turn lasts 100 ms; the minimum distance is 80 * 0.33 as the start-up code makes it.",
                          "Inputs and outputs are float bits; result is EAX (0 next, 2 continue)."],
                         Near.COLUMNS, rows)
        print(f"computer_hand_near: {n} rows, {sum(1 for r in rows if r[-1] == 0)} next")
        oracle.reset()
        throw = Throw(oracle)
        rows = [[*case, *throw.run(*case)] for case in throw_cases(rng)]
        n = write_pinned(out / "computer_hand_throw.txt", "The speed of a throw of a computer god's hand at a place",
                         ["Function: thiscall on the action state (hand +0x50), arg (step); the place is the first",
                          "argument's position (+0x68). The calls around the let-go are stubs; the speed is the",
                          "throw velocity of the interface status (+0x44). Inputs and outputs are float bits."],
                         Throw.COLUMNS, rows)
        print(f"computer_hand_throw: {n} rows")


if __name__ == "__main__":
    main()
