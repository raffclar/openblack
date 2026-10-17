#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the wisps that wind round a creature under a creature spell, in test/oracle/data:

- wisp_orbit.txt: one step of one wisp. The inputs are the box round the creature's bones, the hand, the wisp's
  angles and turn rates, the step time, the wisp's age and the taper. The results are the new angles, the wisp's
  place, its scale and its opacity.

The game's step of the wisps (the creature spell's "generic" rule) runs as it is, from the start of its loop over the
atoms to its end. A small entry routine makes the rule's stack frame (the same size and the same saved
registers as the rule's own start), puts the box and the hand in the frame and jumps into the loop. The loop ends at
the rule's own exit, which gives control back to the oracle.

Usage: python tools/oracle/cases/wisp_orbit_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
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

# The game's code (BW1W120, runblack.exe 1.20)
WISP_LOOP = 0x6793C6  # the start of the generic rule's loop over its atoms
ATOM_AGE = 0x673CC0  # thiscall (atom), no stack args, returns ST0; stubbed to give the age
STEP_SECONDS = 0xD4E0EC  # the step time of the particle systems (float)
# A call must start in the image: the entry routine goes over the rule's property list function, which the loop
# does not use (151 bytes)
ENTRY = 0x6B1CD0

# The rule's constants, as its constructor sets them
FLIGHT_SECONDS = 2.0
WOBBLE = f32.of_bits(0x3E4CCCCD)
WOBBLE_SPEED = 1.0


def entry_code(rule: int, atom: int, frame: dict[int, int]) -> bytes:
    """The entry routine: the rule's own frame, then a jump into its loop"""
    code = bytearray()
    code += bytes([0x81, 0xEC]) + struct.pack("<I", 0xAC)  # sub esp, 0xac
    code += bytes([0x53, 0x55, 0x56, 0x57])  # push ebx, ebp, esi, edi
    code += bytes([0xBB]) + struct.pack("<I", rule)  # mov ebx, rule
    code += bytes([0xBE]) + struct.pack("<I", atom)  # mov esi, atom
    for offset, value in frame.items():
        code += bytes([0xC7, 0x84, 0x24]) + struct.pack("<II", offset, value)  # mov dword [esp+offset], value
    code += bytes([0xB8]) + struct.pack("<I", WISP_LOOP)  # mov eax, loop
    code += bytes([0xFF, 0xE0])  # jmp eax
    return bytes(code)


class WispStep:
    COLUMNS = [
        "centre_x", "centre_z", "bottom", "height", "radius", "hand_x", "hand_y", "hand_z",
        "theta", "phi", "theta_rate", "phi_rate", "dt", "age", "taper", "base_scale", "size",
        "out_theta", "out_phi", "x", "y", "z", "scale", "alpha",
    ]

    def __init__(self, oracle: Oracle):
        self.o = oracle
        self.rule = oracle.object(0x60, {0x3C: FLIGHT_SECONDS, 0x48: WOBBLE, 0x4C: WOBBLE_SPEED})
        self.node = oracle.alloc(0x40)
        self.atom = oracle.alloc(0x140)
        self.body = oracle.alloc(0x100)
        oracle.write_u32(self.node + 0x1C, self.rule)
        oracle.write_u32(self.atom + 0x24, self.node)

    def run(self, inputs: list[int]) -> list[int]:
        o = self.o
        (cx, cz, bottom, height, radius, hx, hy, hz, theta, phi, theta_rate, phi_rate, dt, age, taper, base_scale,
         size) = inputs
        o.write_u32(self.rule + 0x44, taper)
        o.write_u32(self.node + 0x20, theta, phi, theta_rate, phi_rate, base_scale)
        o.write_u32(self.atom + 0x18, 0)
        o.write_u32(self.body + 0x90, size)
        o.write_u32(STEP_SECONDS, dt)
        o.stub(ATOM_AGE, eax=0, pop=0, st0=f32.of_bits(age))
        frame = {
            0x1C: radius, 0x3C: cz, 0x44: self.body, 0x4C: hx, 0x50: hy, 0x54: hz, 0x58: cx, 0x5C: bottom,
            0x90: height,
        }
        code = entry_code(self.rule, self.atom, frame)
        assert len(code) <= 151
        o.write(ENTRY, code)
        result = o.call(ENTRY, args=[0])
        assert result.popped == 4 and result.depth == 0, result
        out_theta, out_phi = o.read_u32(self.node + 0x20, 2)
        x, y, z = o.read_u32(self.atom + 0x80, 3)
        (scale,) = o.read_u32(self.atom + 0x74)
        (colour,) = o.read_u32(self.atom + 0x8C)
        return [out_theta, out_phi, x, y, z, scale, colour >> 24]


def cases() -> list[list[int]]:
    rng = random.Random(0x5749)
    rows = []

    def add(*values: float) -> None:
        rows.append([f2u(v) for v in values])

    # Chosen cases: a wisp still in flight, one at its place, angles near the full turn, no taper, full taper
    add(10.0, -4.0, 2.0, 12.0, 3.0, 8.0, 6.0, -2.0, 0.0, 0.0, 1.0, 1.0, 0.033, 0.5, 0.5, 1.0, 1.0)
    add(10.0, -4.0, 2.0, 12.0, 3.0, 8.0, 6.0, -2.0, 1.0, 2.0, -1.5, 0.75, 0.033, 3.0, 0.5, 1.0, 1.0)
    add(0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 6.28, 6.2, 0.9, 0.9, 0.05, 2.5, 0.0, 0.8, 1.3)
    add(1000.0, 2000.0, 30.0, 25.0, 6.0, 1003.0, 40.0, 1990.0, 3.14159, 0.1, 10.0, 10.0, 0.1, 100.0, 1.0, 1.2, 2.0)
    for _ in range(400):
        add(rng.uniform(-3000.0, 3000.0), rng.uniform(-3000.0, 3000.0), rng.uniform(-50.0, 200.0),
            rng.uniform(0.5, 40.0), rng.uniform(0.5, 15.0), rng.uniform(-3000.0, 3000.0), rng.uniform(-50.0, 200.0),
            rng.uniform(-3000.0, 3000.0), rng.uniform(0.0, 2.0 * math.pi), rng.uniform(0.0, 2.0 * math.pi),
            rng.uniform(-20.0, 20.0), rng.uniform(0.0, 20.0), rng.uniform(0.0, 0.2), rng.uniform(-0.5, 400.0),
            rng.uniform(0.0, 1.0), rng.uniform(0.5, 1.5), rng.uniform(0.3, 3.0))
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rows = []
    with Oracle() as o:
        step = WispStep(o)
        for inputs in cases():
            rows.append(inputs + step.run(inputs))
    write_pinned(
        out / "wisp_orbit.txt",
        "The step of one wisp of a creature spell (the generic rule's loop over its atoms)",
        [
            "The game's loop runs as it is, from an entry routine that makes the rule's frame.",
            "The atom's age is stubbed. Flight 2 s, wobble 0x3E4CCCCD and wobble speed 1, as the constructor sets.",
            "x, y and z are the place that the game writes to the atom, before any change of space.",
        ],
        WispStep.COLUMNS,
        rows,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
