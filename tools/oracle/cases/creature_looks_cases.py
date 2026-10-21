#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the looks that a creature's body takes from the creature, in test/oracle/data:

- creature_looks.txt: from the shown fatness, the fatness, the alignment, the strength and the species' own strength,
  the shown fatness one step on and the evil to good, thin to fat and weak to strong looks. The game's creature turn
  does this at its end, and the loads of a mind file and the swaps of minds do it too.

Usage: python tools/oracle/cases/creature_looks_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The function (BW1W120, runblack.exe 1.20): thiscall on the creature, no arguments
TAKE_LOOKS = 0x477210
# The species' own strength: a float in the rows of the creature table, 0x394 bytes a row
SPECIES_STRENGTH = 0xC6058C


class Looks:
    COLUMNS = ["shown", "fatness", "alignment", "strength", "species_strength", "shown_after", "evil_good", "thin_fat",
               "weak_strong"]

    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.creature = o.alloc(0x200)
        self.body = o.alloc(0x80)
        self.alignment = o.alloc(0x20)
        self.info = o.alloc(0x200)
        self.model = o.alloc(0x100)
        o.write_u32(self.creature + 0x28, self.info)
        o.write_u32(self.creature + 0x160, self.body)
        o.write_u32(self.creature + 0x168, self.alignment)
        o.write_u32(self.body + 0x58, self.model)
        # The first row of the creature table
        o.write_u32(self.info + 0x1F4, 0)

    def run(self, shown: int, fatness: int, alignment: int, strength: int, species: int) -> tuple[int, ...]:
        o = self.o
        o.write_u32(self.body + 0x14, fatness)
        o.write_u32(self.body + 0x18, shown)
        o.write_u32(self.body + 0x0C, strength)
        o.write_u32(self.alignment + 0x08, alignment)
        o.write_u32(SPECIES_STRENGTH, species)
        o.call(TAKE_LOOKS, this=self.creature)
        return (o.read_u32(self.body + 0x18)[0], o.read_u32(self.model + 0x9C)[0], o.read_u32(self.model + 0xA4)[0],
                o.read_u32(self.model + 0xAC)[0])


def cases(rng: random.Random) -> list[tuple[int, int, int, int, int]]:
    rows = []
    edges = [0.0, 1.0, 0.5, 0.25, 0.75, 0.005, 0.995, 0.01, 0.99]
    for shown in edges:
        for fatness in edges:
            rows.append((f2u(shown), f2u(fatness), f2u(0.0), f2u(0.5), f2u(0.5)))
    for alignment in (-1.5, -1.0, -0.5, 0.0, 0.5, 1.0, 1.5):
        for strength in (0.0, 0.25, 0.5, 1.0, 1.25):
            for species in (0.0, 0.5, 1.0):
                rows.append((f2u(0.5), f2u(0.5), f2u(alignment), f2u(strength), f2u(species)))
    for _ in range(1500):
        shown = rng.uniform(0.0, 1.0)
        # Some fatness close to the shown fatness, so that the step is the whole gap
        fatness = shown + rng.uniform(-0.02, 0.02) if rng.random() < 0.4 else rng.uniform(0.0, 1.0)
        fatness = min(max(fatness, 0.0), 1.0)
        rows.append((f2u(shown), f2u(fatness), f2u(rng.uniform(-1.0, 1.0)), f2u(rng.uniform(0.0, 1.0)),
                     f2u(rng.uniform(0.0, 1.0))))
    return rows


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x10C5)
    with Oracle() as o:
        looks = Looks(o)
        rows = [row + looks.run(*row) for row in cases(rng)]
    n = write_pinned(out / "creature_looks.txt", "The looks that a creature's body takes from the creature",
                     ["All values are float bits. The species' own strength is the first row of the creature table."],
                     Looks.COLUMNS, rows)
    print(f"creature_looks.txt: {n} rows")


if __name__ == "__main__":
    main()
