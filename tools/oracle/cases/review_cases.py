#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the three review cases in test/oracle/data:

- catch_reaches.txt: the creature's catch reach test, on chosen, random and edge (one-step) inputs
- sigmoid.txt: the creature sigmoid and the shared sigmoid on a grid, step edges, special values and random inputs
- inverse_square_root.txt: the 3D engine's inverse square root (with the table the game makes at start-up)

Usage: python tools/oracle/cases/review_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned, x80_to_float  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
CATCH_GEOMETRY = 0x47C9D0  # thiscall (creature), 2 stack args (unused, thing), returns EAX 0/1
GET_ANIM = 0x619650  # thiscall, 2 stack args: the clip of an anim; stubbed to return our clip
CREATURE_SIGMOID = 0x4F78C0  # cdecl (a, b) -> ST0
SHARED_SIGMOID = 0x74F170  # cdecl (a, b) -> ST0
INVERSE_SQUARE_ROOT = 0x841170  # cdecl (x) -> ST0
MAKE_INVERSE_SQRT_TABLE = 0x8411D0  # cdecl (), fills the table through the 3D engine's allocator
ENGINE_ALLOC = 0x842CE0  # cdecl (size) -> pointer
INVERSE_SQRT_TABLE_POINTER = 0xEEA394

NAN = 0x7FC00000
POS_INF = 0x7F800000
NEG_INF = 0xFF800000


def next_up(bits: int) -> int:
    """The next float above (bits of a finite float)."""
    if bits == 0x80000000:
        return 1
    return bits + 1 if bits < 0x80000000 else bits - 1


def next_down(bits: int) -> int:
    if bits == 0:
        return 0x80000001
    return bits - 1 if bits < 0x80000000 else bits + 1


def order(bits: int) -> int:
    """A float's bits as an int in the floats' order."""
    return bits if bits < 0x80000000 else -(bits & 0x7FFFFFFF)


def from_order(value: int) -> int:
    return value if value >= 0 else (-value) | 0x80000000


# The catch reach


class Catch:
    COLUMNS = ["thing_x", "thing_z", "velocity_x", "velocity_z", "creature_x", "creature_z", "size", "model_scale",
               "catch_ms", "step_ms", "step_travel", "reaches"]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.creature = oracle.alloc(0x200)
        holder = oracle.alloc(0x100)
        self.body = oracle.alloc(0x4A40)
        self.thing = oracle.alloc(0x200)
        self.clip = oracle.alloc(0x40)
        # The creature finds its 3D body through two pointers
        oracle.write_u32(self.creature + 0x160, holder)
        oracle.write_u32(holder + 0x58, self.body)
        # The thing: no thrower (+0x1C) and no owner object (+0x24), so the alliance checks do not run
        oracle.write_u32(self.thing + 0x1C, 0)
        oracle.write_u32(self.thing + 0x24, 0)
        oracle.stub(GET_ANIM, eax=self.clip, pop=8)

    def run(self, case: list[int]) -> int:
        tx, tz, vx, vz, cx, cz, size, scale, catch_ms, step_ms, step_travel = case
        o = self.o
        o.write_u32(self.thing + 0xC8, tx)
        o.write_u32(self.thing + 0xD0, tz)
        o.write_u32(self.thing + 0x104, vx)
        o.write_u32(self.thing + 0x10C, vz)
        o.write_u32(self.body + 0x78, cx)
        o.write_u32(self.body + 0x80, cz)
        o.write_u32(self.body + 0x90, size)
        o.write_u32(self.body + 0x94, scale)
        o.write_u32(self.body + 0x4A3C, catch_ms)
        o.write_u32(self.clip + 0x0, step_ms)
        o.write_u32(self.clip + 0x10, step_travel)
        result = o.call(CATCH_GEOMETRY, this=self.creature, args=[0, self.thing])
        assert result.popped == 8, result
        return result.eax & 0xFF


def random_catch(rng: random.Random) -> list[int]:
    cx, cz = rng.uniform(500, 3500), rng.uniform(500, 3500)
    tx, tz = cx + rng.uniform(-60, 60), cz + rng.uniform(-60, 60)
    # Mostly towards the creature
    angle = math.atan2(cz - tz, cx - tx) + rng.gauss(0, 0.3)
    speed = rng.uniform(0.5, 40)
    vx, vz = speed * math.cos(angle), speed * math.sin(angle)
    size = rng.uniform(0.3, 2.0)
    scale = rng.uniform(0.5, 3.0)
    return [f2u(tx), f2u(tz), f2u(vx), f2u(vz), f2u(cx), f2u(cz), f2u(size), f2u(scale), rng.randint(100, 900),
            rng.randint(300, 1500), f2u(-rng.uniform(1, 30))]


def edge_pair(catch: Catch, case: list[int], column: int, low: int, high: int) -> list[list[int]] | None:
    """Finds two neighbouring floats in one input, between low and high, where the vanilla answer changes."""
    def answer(bits: int) -> int:
        trial = list(case)
        trial[column] = bits
        return catch.run(trial)

    a, b = order(low), order(high)
    answer_a, answer_b = answer(from_order(a)), answer(from_order(b))
    if answer_a == answer_b:
        return None
    while b - a > 1:
        middle = (a + b) // 2
        if answer(from_order(middle)) == answer_a:
            a = middle
        else:
            b = middle
    cases = []
    for value in (from_order(a - 1), from_order(a), from_order(b), from_order(b + 1)):
        trial = list(case)
        trial[column] = value
        cases.append(trial)
    return cases


def catch_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x0CA7C4)
    catch = Catch(oracle)
    cases: list[list[int]] = []
    # Chosen: a thing flying straight at the creature, one passing to the side, one too slow, one too far in time
    chosen = [
        [1010.0, 1000.0, -10.0, 0.0, 1000.0, 1000.0, 1.0, 1.0, 400, 800, -8.0],
        [1010.0, 1003.0, -10.0, 0.0, 1000.0, 1000.0, 1.0, 1.0, 400, 800, -8.0],
        [1000.5, 1000.0, -0.5, 0.0, 1000.0, 1000.0, 1.0, 1.0, 400, 800, -8.0],
        [1100.0, 1000.0, -10.0, 0.0, 1000.0, 1000.0, 1.0, 1.0, 400, 800, -8.0],
        [1000.0, 1010.0, 0.0, -10.0, 1000.0, 1000.0, 1.0, 1.0, 400, 800, -8.0],
        [1000.0, 1000.0, 3.0, 4.0, 1000.0, 1000.0, 1.0, 1.0, 400, 800, -8.0],
    ]
    for c in chosen:
        cases.append([f2u(v) if isinstance(v, float) else v for v in c])
    # Random
    for _ in range(400):
        cases.append(random_catch(rng))
    # Edges: the miss against the reach (creature x), the 5 s limit (thing x) and the spare time (catch ms is an int,
    # so the edge in time goes through the thing's z)
    edges = 0
    attempts = 0
    while edges < 120 and attempts < 4000:
        attempts += 1
        case = random_catch(rng)
        cx = u2f(case[4])
        column, low, high = rng.choice([(4, cx - 80.0, cx + 80.0), (0, u2f(case[0]) - 150, u2f(case[0]) + 150),
                                        (1, u2f(case[1]) - 150, u2f(case[1]) + 150)])
        pair = edge_pair(catch, case, column, f2u(low), f2u(high))
        if pair:
            cases.extend(pair)
            edges += 1
    rows = []
    for case in cases:
        rows.append(case + [catch.run(case)])
    return rows


# The sigmoid


def sigmoid_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x516D01D)
    pairs: list[tuple[int, int]] = []
    thresholds = [-1.5, -1.0, -0.9, -0.5, -0.25, 0.0, 0.1, 0.25, 0.3, 0.5, 0.75, 0.9, 0.99, 1.0, 1.5]
    for a in thresholds:
        for k in range(0, 43):
            edge = a + (k / 20.5 - 1.0)
            if -1.2 <= edge <= 1.2:
                bits = f2u(edge)
                for b in (next_down(next_down(bits)), next_down(bits), bits, next_up(bits), next_up(next_up(bits))):
                    pairs.append((f2u(a), b))
        for b in (-1.0, -0.5, 0.0, 0.25, 0.5, 0.75, 1.0, 1.5):
            pairs.append((f2u(a), f2u(b)))
    specials = [0, 0x80000000, 1, 0x80000001, NAN, POS_INF, NEG_INF, f2u(1.0), f2u(-1.0), f2u(1e-30)]
    for a in specials:
        for b in specials + [f2u(0.5), f2u(-0.5)]:
            pairs.append((a, b))
    for _ in range(1500):
        pairs.append((f2u(rng.uniform(-1.2, 1.2)), f2u(rng.uniform(-1.2, 1.2))))
    rows = []
    for a, b in pairs:
        creature = oracle.call(CREATURE_SIGMOID, args=[f32.of_bits(a), f32.of_bits(b)])
        shared = oracle.call(SHARED_SIGMOID, args=[f32.of_bits(a), f32.of_bits(b)])
        assert creature.depth == 1 and shared.depth == 1
        rows.append([a, b, creature.st0f, shared.st0f])
    return rows


# The inverse square root


def inverse_square_root_cases(oracle: Oracle) -> list[list[int]]:
    rng = random.Random(0x15A7)
    oracle.hook_alloc(ENGINE_ALLOC)
    oracle.call(MAKE_INVERSE_SQRT_TABLE)
    assert oracle.read_u32(INVERSE_SQRT_TABLE_POINTER)[0] != 0
    inputs = [0, 0x80000000, 1, 2, 0x007FFFFF, 0x00800000, 0x00800001, f2u(1e-30), f2u(0.25), f2u(0.5), f2u(1.0),
              f2u(2.0), f2u(3.0), f2u(4.0), f2u(100.0), f2u(1e10), f2u(1e30), 0x7F7FFFFF, POS_INF, NAN, f2u(-1.0),
              f2u(-4.0), NEG_INF]
    # Every table entry, at an odd and an even exponent, with the low mantissa bits clear, set and random
    for index in range(128):
        for exponent in (126, 127, 128, 133, 150, 20, 230):
            for low in (0, 0x1FFFF, rng.getrandbits(17)):
                inputs.append((exponent << 23) | (index << 17) | low)
    for _ in range(3000):
        inputs.append(rng.getrandbits(31))  # positive floats of any exponent (some NaN or infinite)
    for _ in range(500):
        inputs.append(f2u(rng.uniform(0.0, 50000.0)))
    rows = []
    for x in inputs:
        result = oracle.call(INVERSE_SQUARE_ROOT, args=[f32.of_bits(x)])
        assert result.depth == 1
        wide = x80_to_float(result.st0)
        narrow = u2f(result.st0f)
        # 1 when the FPU result is not a float exactly (out of the float range); the caller's store rounds it
        same = (math.isnan(wide) and math.isnan(narrow)) or wide == narrow
        rows.append([x, result.st0f, 0 if same else 1])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    with Oracle() as oracle:
        rows = catch_cases(oracle)
        n = write_pinned(out / "catch_reaches.txt", "The creature's catch reach test (catch geometry of a thrown thing)",
                         ["Function: thiscall on the creature, args (unused, thing); EAX 1 when the creature reaches.",
                          "Floats are bits; catch_ms and step_ms are ints (the body's catch time, the step clip's ms).",
                          "Rows: 6 chosen, 400 random, then groups of 4 around an edge where the answer changes."],
                         Catch.COLUMNS, rows)
        print(f"catch_reaches: {n} rows, {sum(r[-1] for r in rows)} reach")
        oracle.reset()
        rows = sigmoid_cases(oracle)
        n = write_pinned(out / "sigmoid.txt", "The creature sigmoid and the shared sigmoid",
                         ["creature: 0 when b <= 0 or NaN, else the shared sigmoid. shared: the 41-step table.",
                          "Columns a (threshold) and b (value) in, the two results out, all float bits."],
                         ["a", "b", "creature", "shared"], rows)
        print(f"sigmoid: {n} rows")
        oracle.reset()
        rows = inverse_square_root_cases(oracle)
        n = write_pinned(out / "inverse_square_root.txt", "The 3D engine's inverse square root",
                         ["x in, the result stored as a float out; wide is 1 when the FPU result is outside the",
                          "float range, so that the result depends on the caller's store."],
                         ["x", "result", "wide"], rows)
        print(f"inverse_square_root: {n} rows, {sum(r[2] for r in rows)} wide")
    return 0


if __name__ == "__main__":
    sys.exit(main())
