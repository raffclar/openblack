#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of one turn of a creature's desires, in test/oracle/data:

- creature_desire_update.txt: one desire with up to four sources that follow no state. The turn fades each source by
  its multiplier, adds each source's sigmoid drive to the source and to the desire's drive, then grows the desire by
  its drive over its growing time or fades it by its decay, holds it between the species' floor and its maximum, and
  adds it to the sum. Impress from seeing things fades too.

Usage: python tools/oracle/cases/creature_desire_update_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions and globals (BW1W120, runblack.exe 1.20)
UPDATE_DESIRES = 0x4DC430  # thiscall (the creature's desires), arg: the creature; RET 4
GAME = 0xD0195C  # the game object; only its flags at +0x14 are read, for a desire that is not activated
MS_PER_TURN = 0xD01A38
SOURCE_TABLE_MULTIPLIER = 0xC8AD78  # stride 0xA4 by source type: the source's multiplier

# The desires object (from the disassembly)
ACTIVATED = 0x8
SUPPRESSED = 0xA8
VALUE = 0x148
INCREASE_SECONDS = 0x1E8
DECAY = 0x288
SOURCES = 0x328  # a list head and a count for each desire
MAXIMUM = 0x468
CREATURE = 0x648
SUM = 0x654
IMPRESS_FROM_SEEING = 0x704
# The creature's info row at +0x28, with the species' desire floor at +0x274
INFO_ROW = 0x28
FLOOR = 0x274

# Source types that have no reader of state in the game, so only the multiplier changes their value
SOURCE_TYPES = [2, 3, 4, 6]
SOURCE_COUNT = 4

COLUMNS = (["value", "decay", "increase_seconds", "maximum", "floor", "suppressed", "sources", "impress", "sum"]
           + [f"{name}{i}" for i in range(SOURCE_COUNT) for name in ("value", "threshold", "multiplier", "drive")]
           + ["out_value", "out_suppressed", "out_sum", "out_impress"]
           + [f"out_{name}{i}" for i in range(SOURCE_COUNT) for name in ("value", "drive")])


def u(x: float) -> int:
    return f2u(x)


class Desires:
    def __init__(self, o: Oracle):
        self.o = o
        self.desires = o.alloc(0x710)
        self.creature = o.alloc(0x40)
        self.info = o.alloc(0x300)
        self.game = o.alloc(0x40)
        self.nodes = [o.alloc(0x10) for _ in range(SOURCE_COUNT)]
        self.sources = [o.alloc(0x10) for _ in range(SOURCE_COUNT)]
        o.write_u32(GAME, self.game)
        o.write_u32(MS_PER_TURN, 100)
        o.write_u32(self.creature + INFO_ROW, self.info)
        o.write_u32(self.desires + CREATURE, self.creature)

    def turn(self, c: dict) -> list[int]:
        o, d = self.o, self.desires
        # Only the first desire is activated; the others have no sources
        o.write_u32(d + ACTIVATED, 1)
        o.write_u32(d + SUPPRESSED, c["suppressed"])
        o.write_f32(d + VALUE, f32.of_bits(c["value"]))
        o.write_f32(d + INCREASE_SECONDS, f32.of_bits(c["increase_seconds"]))
        o.write_f32(d + DECAY, f32.of_bits(c["decay"]))
        o.write_f32(d + MAXIMUM, f32.of_bits(c["maximum"]))
        o.write_f32(d + SUM, f32.of_bits(c["sum"]))
        o.write_f32(d + IMPRESS_FROM_SEEING, f32.of_bits(c["impress"]))
        o.write_f32(self.info + FLOOR, f32.of_bits(c["floor"]))
        count = c["sources"]
        for i in range(SOURCE_COUNT):
            node, source = self.nodes[i], self.sources[i]
            o.write_u32(node, self.nodes[i + 1] if i + 1 < count else 0)
            o.write_u32(node + 4, source)
            o.write_f32(source, f32.of_bits(c[f"value{i}"]))
            o.write_f32(source + 4, f32.of_bits(c[f"threshold{i}"]))
            o.write_f32(source + 8, f32.of_bits(c[f"drive{i}"]))
            o.write_u32(source + 0xC, SOURCE_TYPES[i])
            o.write_f32(SOURCE_TABLE_MULTIPLIER + SOURCE_TYPES[i] * 0xA4, f32.of_bits(c[f"multiplier{i}"]))
        o.write_u32(d + SOURCES, self.nodes[0] if count > 0 else 0)
        o.write_u32(d + SOURCES + 4, count)
        r = o.call(UPDATE_DESIRES, this=d, args=[self.creature])
        assert r.popped == 4 and r.depth == 0, r
        out = [o.read_f32_bits(d + VALUE)[0], o.read_u32(d + SUPPRESSED)[0], o.read_f32_bits(d + SUM)[0],
               o.read_f32_bits(d + IMPRESS_FROM_SEEING)[0]]
        for i in range(SOURCE_COUNT):
            out += [o.read_f32_bits(self.sources[i])[0], o.read_f32_bits(self.sources[i] + 8)[0]]
        return out


def cases(rng: random.Random) -> list[dict]:
    result = []

    def case(**given) -> dict:
        c = {
            "value": u(rng.choice([0.0, 0.5, rng.uniform(0.0, 1.0), rng.uniform(-0.1, 1.3)])),
            "decay": u(rng.choice([0.99, 0.995, 0.9999, rng.uniform(0.9, 1.0)])),
            "increase_seconds": u(rng.choice([30.0, 120.0, 600.0, rng.uniform(0.8, 1440.0)])),
            "maximum": u(rng.choice([1.0, 1.0, 0.8, rng.uniform(0.0, 1.2), -0.1])),
            "floor": u(rng.choice([0.0, 0.0, 0.0, 0.05, rng.uniform(0.0, 0.3)])),
            "suppressed": rng.choice([0, 0, 0, 1, 2, 5, rng.randint(0, 2000)]),
            "sources": rng.randint(0, SOURCE_COUNT),
            "impress": u(rng.choice([0.0, 1.0, 0.5, rng.uniform(0.0, 1.0)])),
            "sum": u(rng.choice([0.0, rng.uniform(0.0, 5.0)])),
        }
        for i in range(SOURCE_COUNT):
            c[f"value{i}"] = u(rng.choice([0.0, 1.0, rng.uniform(0.0, 1.0), rng.uniform(-0.2, 1.2)]))
            c[f"threshold{i}"] = u(rng.choice([0.0, 0.5, 1.0, rng.uniform(0.0, 1.0), rng.uniform(-0.5, 1.5)]))
            c[f"multiplier{i}"] = u(rng.choice([1.0, 0.99, 0.999, 0.9, rng.uniform(0.8, 1.0)]))
            c[f"drive{i}"] = u(rng.choice([0.0, rng.uniform(0.0, 50.0)]))
        c.update(given)
        return c

    # The growing times and decays of the shipped tables, every source full
    for seconds in [1.0, 4.0, 30.0, 60.0, 300.0, 1200.0, 0.8, 1440.0]:
        for sources in range(SOURCE_COUNT + 1):
            full = {f"value{i}": u(1.0) for i in range(SOURCE_COUNT)}
            full.update({f"threshold{i}": u(0.5) for i in range(SOURCE_COUNT)})
            full.update({f"multiplier{i}": u(1.0) for i in range(SOURCE_COUNT)})
            result.append(case(increase_seconds=u(seconds), sources=sources, suppressed=0, value=u(0.25),
                               maximum=u(1.0), floor=u(0.0), **full))
    for _ in range(5000):
        result.append(case())
    return result


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261110)
    with Oracle() as o:
        desires = Desires(o)
        rows = []
        for c in cases(rng):
            rows.append([c[k] for k in COLUMNS[:9 + 4 * SOURCE_COUNT]] + desires.turn(c))
        write_pinned(out / "creature_desire_update.txt", "One turn of a creature's desires (the game's own function)",
                     ["One activated desire, 100 ms a turn. The suppressed and sources columns are plain words; all",
                      "other values are float bits. Source i is in the desire's list only when i < sources."],
                     COLUMNS, rows)
    print("done")


if __name__ == "__main__":
    main()
