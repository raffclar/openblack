#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the hand's throw at the release, in test/oracle/data:

- throw_release.txt: the velocity that the throw sends (the hand's velocity, made shorter when it is faster than the
  most), whether the release is fast, and the turns of lead from the latency of the hand's reports

The game's release of a held thing runs with its other calls replaced: the body set-up, the land, the packet send and
the steps of the prediction body. The prediction function itself runs, so the turns of lead are the count that it keeps.

Usage: python tools/oracle/cases/throw_lead_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The code (BW1W120, runblack.exe 1.20)
RELEASE = 0x5D4DB0  # thiscall (interface), no args: the release of a held thing (interface state 12)
PICKED_THING = 0x5DC130  # thiscall, no args
IN_BOUNDS = 0x6042C0  # thiscall on a point, no args
MY_INTERFACE = 0x555850  # thiscall on the game: the local interface
FREE_BODY = 0x7FB730  # thiscall on the prediction body
SET_UP_BODY = 0x7FB780  # thiscall, 2 args
ADJUST_TO_GROUND = 0x7FCB80  # thiscall, 2 args (always 1, the slope flag)
ZERO_FORCES = 0x7FD200
GROUND_FORCES = 0x7FD4D0
CONTACT_FORCES = 0x7FDE40
INTEGRATE = 0x7FE260
OBJECT_MATRIX = 0x7FD140  # thiscall, 1 arg
GET_YXZ = 0x7FAB30  # thiscall, 3 args
SEND_PACKET = 0x551090  # thiscall, 2 args (type, the 12 floats)
LET_GO = 0x5D1850  # thiscall, no args
REPORT_LATENCY = 0xD44454  # the latency of the hand's reports in milliseconds
LEAD_TURNS = 0xD47080  # the turns that the prediction stepped
PREDICTION_ACTIVE = 0xD47824

# Places in the exe that the fake vtable points to. The release does not run them for any other use.
VT_SCALE = 0x7C0000  # +0x120: the scale, on ST0
VT_ANY_TREE = 0x7C0010  # +0x478: 0, not a tree
VT_MAY_DROP = 0x7C0020  # +0x724: 1, RET 8
VT_SET_UP = 0x7C0030  # +0x78C: RET 4


class Release:
    COLUMNS = ["vx", "vy", "vz", "latency_ms", "out_vx", "out_vy", "out_vz", "fast", "lead_turns"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.interface = o.alloc(0x500)
        self.player = o.alloc(0x400)
        self.hand = o.alloc(0x4A00)
        thing = o.alloc(0x100)
        held = o.alloc(0x100)
        vtable = o.alloc(0x800)
        self.flag = o.alloc(0x10)
        self.packet = o.alloc(0x40)
        # The interface is in the state that lets the hand throw, and holds a thing
        o.write_u32(self.interface + 0x38, 0x4000)
        o.write_u32(self.interface + 0x48, 1)
        o.write_u32(self.player + 0x3A0, self.hand)
        o.write_u32(self.hand + 0x4904, held)
        o.write_u32(thing, vtable)
        o.write_u32(held, vtable)
        o.write_u32(vtable + 0x120, VT_SCALE)
        o.write_u32(vtable + 0x478, VT_ANY_TREE)
        o.write_u32(vtable + 0x724, VT_MAY_DROP)
        o.write_u32(vtable + 0x78C, VT_SET_UP)
        o.stub(VT_SCALE, st0=1.0)
        o.stub(VT_ANY_TREE, eax=0)
        o.stub(VT_MAY_DROP, eax=1, pop=8)
        o.stub(VT_SET_UP, pop=4)
        o.stub(PICKED_THING, eax=thing)
        o.stub(IN_BOUNDS, eax=1)
        o.stub(MY_INTERFACE, eax=self.player)
        o.stub(FREE_BODY)
        o.stub(SET_UP_BODY, pop=8)
        for step in (ZERO_FORCES, GROUND_FORCES, CONTACT_FORCES, INTEGRATE):
            o.stub(step)
        o.stub(OBJECT_MATRIX, pop=4)
        o.stub(GET_YXZ, pop=12)
        o.stub(LET_GO)
        # The ground call keeps its second arg: whether the thing lies along the slope
        code = bytes([0x8B, 0x44, 0x24, 0x08, 0xA3]) + struct.pack("<I", self.flag) + bytes([0xC2, 0x08, 0x00])
        o.write(ADJUST_TO_GROUND, code)
        # The packet send copies the 12 floats (its second arg) to our memory
        code = bytes([0x8B, 0x44, 0x24, 0x08, 0xBA]) + struct.pack("<I", self.packet)
        for i in range(12):
            code += bytes([0x8B, 0x48, i * 4, 0x89, 0x4A, i * 4])
        code += bytes([0xC2, 0x08, 0x00])
        o.write(SEND_PACKET, code)

    def run(self, vx: int, vy: int, vz: int, latency_ms: int) -> list[int]:
        o = self.o
        o.write(self.hand + 0x48C8, struct.pack("<3I", vx, vy, vz) + bytes(12))
        o.write_u32(REPORT_LATENCY, latency_ms)
        o.write_u32(PREDICTION_ACTIVE, 0)
        o.write_u32(LEAD_TURNS, 0xFFFFFFFF)
        o.write_u32(self.flag, 0xFFFFFFFF)
        result = o.call(RELEASE, this=self.interface)
        assert result.depth == 0, result
        velocity = o.read_u32(self.packet, 3)
        # The slope flag is the opposite of fast for a thing that is not a tree
        lies_along = o.read_u32(self.flag)[0] & 0xFF
        return velocity + [0 if lies_along else 1, o.read_u32(LEAD_TURNS)[0]]


def release_cases(oracle: Oracle) -> list[list[int]]:
    release = Release(oracle)
    rng = random.Random(149)
    cases: list[list[int]] = []
    # Chosen: still, slow, near the fast limit, at and near the most speed, far over it, with signed zeros
    chosen = [(0.0, 0.0, 0.0), (-0.0, 0.0, -0.0), (0.5, 0.5, 0.5), (0.57735026, 0.57735026, 0.57735026),
              (1.0, 0.0, 0.0), (0.0, 1.0000001, 0.0), (124.0, 0.0, 0.0), (0.0, 0.0, -124.00001),
              (71.59, 71.59, 71.6), (300.0, 0.0, 400.0), (-1000.0, 20.0, 3.0), (1.0e-20, 0.0, 0.0)]
    latencies = [0, 1, 99, 100, 101, 199, 200, 450, 499, 500, 501, 2000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
    for velocity in chosen:
        for latency in latencies:
            cases.append([f2u(velocity[0]), f2u(velocity[1]), f2u(velocity[2]), latency])
    # Random throws near and over the most speed, and random latencies
    for _ in range(2000):
        scale = rng.choice([1.0, 30.0, 80.0, 200.0])
        velocity = [rng.uniform(-scale, scale) for _ in range(3)]
        cases.append([f2u(v) for v in velocity] + [rng.randint(0, 700)])
    return [case + release.run(*case) for case in cases]


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        rows = release_cases(oracle)
        n = write_pinned(out / "throw_release.txt", "The hand's throw at the release: the velocity, fast, the lead",
                         ["The game's release of a held thing that is not a tree, with its other calls replaced.",
                          "Inputs: the hand's throw velocity x y z (float bits) and the report latency in ms (int).",
                          "Out: the velocity in the throw packet (float bits), 1 when the release is fast, and the",
                          "turns of lead that the prediction counted (int)."],
                         Release.COLUMNS, rows)
        print(f"throw_release: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
