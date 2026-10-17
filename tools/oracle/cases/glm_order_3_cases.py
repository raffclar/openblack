#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the third glm order lane in test/oracle/data:

- creature_eye.txt: the creature's eye drawing step (eyeball and eyelid matrices) for one eye, on chosen and random
  inputs. The eye points are on a mesh that is not skinned, with a bone matrix that is the identity, so that the
  surface points are the vertices as given.
- flock_falloff.txt: the flock rule's falloff of a pull with distance
- set_length.txt: the particle code's "make a vector this long" (the squares added z first)
- hair_step.txt: one step of a creature's hair strand that is not turned, on the same mesh as the eyes

Usage: python tools/oracle/cases/glm_order_3_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
DRAW_EYES = 0x48E260  # thiscall (creature): draws the creature's eyes
BEFORE_EYES = 0x48F550  # thiscall, no arguments: stubbed (the body's own drawing)
LOCAL_RAND = 0x6DE570  # cdecl (range) -> EAX: stubbed to a chosen number
ENGINE_ALLOC = 0x842CE0
MAKE_INVERSE_SQRT_TABLE = 0x8411D0
EYEBALL_OBJECT = 0xC64224
EYELID_OBJECT = 0xC64228
# Functions that DrawNow does not reach with these inputs; stubbed to stand for the draw object's virtual functions
STUB_POP_0 = BEFORE_EYES
STUB_POP_4 = 0x68E430
STUB_POP_8 = 0x8067F0
STUB_POP_12 = 0x80B4D0

VERTICES = 12  # three for each of the four surface points (eye 0, eye 1, lid anchor 0, lid anchor 1)

COLUMNS = (
    [f"v{i}{axis}" for i in range(VERTICES) for axis in "xyz"]
    + [f"s{i}" for i in range(4)]
    + [f"t{i}" for i in range(4)]
    + ["eye", "depth", "yaw", "size", "evil_good", "eye_scale", "ms", "target_x", "target_y", "target_z", "mode",
       "openness", "timer", "blink", "lid_open", "lid_closed", "lid_calm", "rand", "has_lid"]
    + [f"ball{i}" for i in range(12)]
    + [f"lid{i}" for i in range(12)]
)


class Eyes:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.hook_alloc(ENGINE_ALLOC)
        o.call(MAKE_INVERSE_SQRT_TABLE)
        for address, pop in ((STUB_POP_0, 0), (STUB_POP_4, 4), (STUB_POP_8, 8), (STUB_POP_12, 12)):
            o.stub(address, pop=pop)
        vtable = o.alloc(0x200)
        o.write_u32(vtable + 0x100, STUB_POP_0)
        o.write_u32(vtable + 0x108, STUB_POP_0)
        o.write_u32(vtable + 0x2C, STUB_POP_4)
        o.write_u32(vtable + 0x154, STUB_POP_8)
        o.write_u32(vtable + 0x15C, STUB_POP_12)
        # The body's draw object: a fade of 1 draws the eyes plainly and keeps the lids white
        self.body = o.object(0x200, {0: vtable, 0x94: 1.0})
        self.eyeball = o.object(0x100, {0: vtable})
        self.eyelid = o.object(0x100, {0: vtable})
        o.write_u32(EYEBALL_OBJECT, self.eyeball)
        o.write_u32(EYELID_OBJECT, self.eyelid)
        self.vertices = o.alloc(0x20 * VERTICES)
        prim = o.object(0x40, {0x14: self.vertices})
        prims = o.object(0x10, {0: prim})
        group = o.object(0x10, {8: prims})
        groups = o.object(0x10, {0: group})
        self.mesh = o.object(0x40, {4: 0, 0x10: groups})
        self.bones = o.object(0x30, {0: 1.0, 0x10: 1.0, 0x20: 1.0})
        self.creature = o.alloc(0x5800)

    def run(self, case: dict) -> list[int]:
        o = self.o
        c = self.creature
        o.write(c, bytes(0x5800))
        o.write(self.eyeball + 0x14, bytes(0x30))
        o.write(self.eyelid + 0x14, bytes(0x30))
        o.write_u32(c + 0x51E8, 1)
        o.write_u32(c + 0x482C, self.body)
        o.write_u32(c + 0xB4, self.mesh)
        o.write_u32(c + 0x47F0, self.bones)
        for i in range(VERTICES):
            o.write_u32(self.vertices + 0x20 * i, *case["v"][3 * i : 3 * i + 3])
        for i in range(4):
            record = c + 0x5478 + 0x28 * i
            o.write_u32(record + 8, 3 * i, 3 * i + 1, 3 * i + 2)
            o.write_u32(record + 0x20, case["s"][i], case["t"][i])
        eye = case["eye"]
        o.write_u32(c + 0x5518 + 4 * eye, 1)
        o.write_u32(c + 0x5520 + 4 * eye, case["has_lid"])
        o.write_u32(c + 0x5528 + 4 * eye, case["depth"])
        o.write_u32(c + 0x84, case["yaw"])
        o.write_u32(c + 0x90, case["size"])
        o.write_u32(c + 0x9C, case["evil_good"])
        o.write_u32(c + 0x5474, case["eye_scale"])
        o.write_u32(c + 0x48B4, case["ms"])
        o.write_u32(c + 0x48C0, *case["target"])
        o.write_u32(c + 0x5460, case["mode"], case["openness"], case["timer"])
        o.write_u32(c + 0x5470, case["blink"])
        o.write_u32(c + 0x565C, case["lids"][0])
        o.write_u32(c + 0x5668, case["lids"][1])
        o.write_u32(c + 0x5674, case["lids"][2])
        o.stub(LOCAL_RAND, eax=case["rand"])
        o.call(DRAW_EYES, this=c)
        ball = o.read_f32_bits(self.eyeball + 0x14, 12)
        lid = o.read_f32_bits(self.eyelid + 0x14, 12)
        return (case["v"] + case["s"] + case["t"]
                + [eye, case["depth"], case["yaw"], case["size"], case["evil_good"], case["eye_scale"], case["ms"]]
                + case["target"]
                + [case["mode"], case["openness"], case["timer"], case["blink"]] + case["lids"]
                + [case["rand"], case["has_lid"]] + ball + lid)


def random_case(rng: random.Random, mode: int, blink: int, eye: int) -> dict:
    def f(low: float, high: float) -> int:
        return f2u(rng.uniform(low, high))

    # A head about a point, the eye's triangle facing out of it, the anchor's triangle near it
    head = [rng.uniform(-200, 200), rng.uniform(0, 30), rng.uniform(-200, 200)]
    vertices = []
    for _ in range(VERTICES):
        vertices += [f2u(head[k] + rng.uniform(-2.0, 2.0)) for k in range(3)]
    looks = rng.random() < 0.7
    target = [f(-250, 250), f(0, 40), f(-250, 250)] if looks else [0, 0, 0]
    return {
        "v": vertices,
        "s": [f(0.0, 0.6) for _ in range(4)],
        "t": [f(0.0, 0.4) for _ in range(4)],
        "eye": eye,
        "depth": f(0.0, 0.3),
        "yaw": f(-7.0, 7.0),
        "size": f(0.2, 3.0),
        "evil_good": f(-1.0, 1.0),
        "eye_scale": f(0.5, 1.5),
        "ms": rng.choice([0, 10, 16, 33, 50, 100, 1000]),
        "target": target,
        "mode": mode,
        "openness": f(-1.0, 0.75),
        "timer": rng.randint(-20, 200),
        "blink": blink,
        "lids": [f(-0.5, 0.5), f(-0.6, -0.2), f(-0.3, 0.4)],
        "rand": rng.randint(0, 200),
        "has_lid": 1 if rng.random() < 0.85 else 0,
    }


def eye_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0xE1E5)
    eyes = Eyes(oracle)
    rows = []
    for mode in range(5):
        for blink in range(3):
            for eye in range(2):
                for _ in range(25):
                    rows.append(eyes.run(random_case(rng, mode, blink, eye)))
    return rows


FLOCK_FALLOFF = 0x683520  # thiscall (rule: scale modifier at +0x4C), (distance, type, invert) -> ST0
SET_LENGTH = 0x6805F0  # thiscall (vector), (length) -> the vector made that long


def falloff_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0xFA11)
    rule = oracle.alloc(0x80)
    rows = []
    chosen = [0.0, 0.005, 0.0099999, 0.01, 0.010001, 0.5, 1.0, 2.0, 3.0, 10.0, 123.456]
    cases = [(f2u(d), f2u(m)) for d in chosen for m in (1.0, 0.4, 0.5, 2.5)]
    cases += [(f2u(10 ** rng.uniform(-3, 3)), f2u(rng.uniform(0.1, 4.0))) for _ in range(400)]
    for distance, modifier in cases:
        oracle.write_u32(rule + 0x4C, modifier)
        for kind in range(3):
            for invert in range(2):
                result = oracle.call(FLOCK_FALLOFF, this=rule, args=[distance, kind, invert])
                rows.append([distance, modifier, kind, invert, result.st0f])
    return rows


def set_length_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x5E71)
    vector = oracle.alloc(16)
    rows = []
    for _ in range(1500):
        v = [f2u(rng.uniform(-100, 100) * 10 ** rng.uniform(-3, 1)) for _ in range(3)]
        length = f2u(10 ** rng.uniform(-2, 3))
        oracle.write_u32(vector, *v)
        oracle.call(SET_LENGTH, this=vector, args=[length])
        rows.append(v + [length] + oracle.read_f32_bits(vector, 3))
    return rows


HAIR_STEP = 0x8467C0  # thiscall (strand), (seconds)
HAIR_BONES = 0xC37D9C  # the bone matrices the strands are placed with
HAIR_POINTS = 6


def hair_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x4A12)
    eyes = Eyes(oracle)  # the same mesh, bones and inverse square root table
    o = oracle
    o.write_u32(HAIR_BONES, eyes.bones)
    group = o.alloc(0x80)
    strand = o.alloc(0x80)
    points = o.alloc(0x24 * HAIR_POINTS)
    rows = []
    for _ in range(400):
        def f(low: float, high: float) -> int:
            return f2u(rng.uniform(low, high))

        head = [rng.uniform(-200, 200), rng.uniform(0, 30), rng.uniform(-200, 200)]
        triangle = [f2u(head[k % 3] + rng.uniform(-2.0, 2.0)) for k in range(9)]
        st = [f(0.0, 0.6), f(0.0, 0.4)]
        looks = [f(0.2, 1.0) for _ in range(3)] + [f(0.5, 0.95) for _ in range(3)] +                 [f(0.01, 0.3) for _ in range(3)] + [f(0.01, 0.2) for _ in range(3)]
        alignment = f(-1.0, 1.0) if rng.random() < 0.8 else rng.choice([0, f2u(1e-9), f2u(-1e-9), f2u(1.0)])
        scale = f(5.0, 40.0)
        seconds = f2u(rng.choice([0.0, 0.01, 0.016, 0.033, 0.1, 1.0])) if rng.random() < 0.5 else f(0.0, 0.2)
        start = [f2u(head[k % 3] + rng.uniform(-10, 10)) for k in range(3 * HAIR_POINTS)]
        speeds = [f(-30, 30) for _ in range(3 * HAIR_POINTS)]
        o.write_u32(eyes.vertices, *triangle[0:3])
        o.write_u32(eyes.vertices + 0x20, *triangle[3:6])
        o.write_u32(eyes.vertices + 0x40, *triangle[6:9])
        o.write(group, bytes(0x80))
        o.write_u32(group + 0x2C, *looks)
        o.write_u32(group + 0x5C, HAIR_POINTS)
        o.write_u32(group + 0x64, alignment, scale)
        o.write_u32(group + 0x70, eyes.mesh)
        o.write(strand, bytes(0x80))
        o.write_u32(strand + 0x34, points)
        o.write_u32(strand + 0x38 + 8, 0, 1, 2)
        o.write_u32(strand + 0x38 + 0x20, *st)
        o.write_u32(strand + 0x60, group)
        o.write(points, bytes(0x24 * HAIR_POINTS))
        o.write_u32(points, *start)
        o.write_u32(points + 0x18 * HAIR_POINTS, *speeds)
        o.call(HAIR_STEP, this=strand, args=[seconds])
        after = o.read_f32_bits(points, 3 * HAIR_POINTS)
        after_speeds = o.read_f32_bits(points + 0x18 * HAIR_POINTS, 3 * HAIR_POINTS)
        rows.append(triangle + st + looks + [alignment, scale, seconds] + start + speeds + after + after_speeds)
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        rows = eye_cases(oracle)
        falloffs = falloff_cases(oracle)
        lengths = set_length_cases(oracle)
        hairs = hair_cases(oracle)
    hair_columns = ([f"t{i}" for i in range(9)] + ["s", "t"] + [f"look{i}" for i in range(12)]
                    + ["alignment", "scale", "seconds"]
                    + [f"p{i}" for i in range(3 * HAIR_POINTS)] + [f"v{i}" for i in range(3 * HAIR_POINTS)]
                    + [f"rp{i}" for i in range(3 * HAIR_POINTS)] + [f"rv{i}" for i in range(3 * HAIR_POINTS)])
    write_pinned(out / "hair_step.txt", "One step of a creature's hair strand (not turned)",
                 ["the triangle's three vertices, s and t, the looks (length, damping, stiffness, thickness: neutral,",
                  "evil, good each), the alignment, the scale and the seconds, the points and speeds before; the",
                  f"points and speeds after. {HAIR_POINTS} points."], hair_columns, hairs)
    write_pinned(out / "flock_falloff.txt", "The flock rule's falloff of a pull with distance",
                 ["distance, scale modifier, type and invert in; the falloff out"],
                 ["distance", "modifier", "type", "invert", "falloff"], falloffs)
    write_pinned(out / "set_length.txt", "A vector made a length long by the particle code",
                 ["x, y, z and the length in; the vector out"], ["x", "y", "z", "length", "rx", "ry", "rz"], lengths)
    count = write_pinned(
        out / "creature_eye.txt", "The creature's eye drawing step, one eye at a time",
        ["Inputs: twelve vertices (three for each surface point), each point's s and t, the eye, its depth, the",
         "creature's angle, size, good or evil body and eye size, the frame time in ms, the look target (zero for",
         "none), the eyes' mode, openness, blink timer and state, the lid angles, the random number for each dart and",
         "whether the eye has a lid. Outputs: the eyeball's and the eyelid's 4x3 matrices (rows, then the origin);",
         "the eyelid is zero when the eye has no lid. The look's zoomers start at zero."],
        COLUMNS, rows)
    print(f"creature_eye.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
