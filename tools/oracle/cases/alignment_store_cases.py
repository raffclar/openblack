#!/usr/bin/env python3
"""Pinned cases of the value that the land keeps for the camera's alignment, from the original game.

Writes into test/oracle/data:

- alignment_store.txt: the turn's camera alignment step. The alignment of the player of most influence goes in; the
  value of the land and the value of the audio come out.
- sky_land.txt: the frame's sky step. The value of the sky, the value of the land and the frame's whole milliseconds
  go in; the new value of the sky comes out.

Usage: python tools/oracle/cases/alignment_store_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
CAMERA_ALIGNMENT = 0x64AC30  # cdecl (), once a turn
MY_INTERFACE = 0x555850  # thiscall, the interface of this computer's player -> EAX
INTERFACE_STATUS = 0x39C  # the interface's status, with the reported camera at +0xB0
MOST_INFLUENTIAL = 0x603830  # thiscall (map coords) -> EAX, the player
PLAYER_ALIGNMENT = 0x64D6A0  # thiscall (player) -> ST0
LAND_VALUE = 0xBF337C
AUDIO_POINTER = 0xCD3B20
AUDIO_ALIGNMENT = 0x190
DRAW_SKY = 0x5E2160  # cdecl (), once a frame
SKY_VALUE = 0xBF3378
FRAME_MILLISECONDS = 0xEA9EC0
SKY_CALLS = (0x5E1DE0, 0x86A2C0, 0x86A330, 0x86B010)  # the sky colours and the sky draw


def f32_round(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def next_up(value: float, steps: int) -> float:
    """The float that is a number of float steps above (or below, for a negative number) a float."""
    bits = f2u(value)
    if value >= 0.0:
        return u2f(bits + steps)
    return u2f(bits - steps)


def camera_inputs(rng: random.Random) -> list[float]:
    alignments = [-1.0, 1.0, 0.0, -0.5, 0.5, 1e-9, -1e-9, 3.0, -3.0, float("nan")]
    alignments += [f32_round(rng.uniform(-1.2, 1.2)) for _ in range(600)]
    # Goodness near 0: the evilness rounds to 1
    alignments += [f32_round(2.0 * rng.uniform(0.0, 1e-6) - 1.0) for _ in range(100)]
    alignments += [next_up(-1.0, -step) for step in range(1, 40)]
    # Goodness near 0.5 and near 0.25: the evilness changes its exponent
    alignments += [f32_round(rng.uniform(-1e-5, 1e-5)) for _ in range(200)]
    alignments += [f32_round(rng.uniform(-0.5 - 1e-5, -0.5 + 1e-5)) for _ in range(200)]
    alignments += [next_up(0.0, step) for step in range(1, 40)]
    return alignments


def camera_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    audio = o.alloc(0x200)
    o.write_u32(AUDIO_POINTER, audio)
    interface = o.alloc(0x400)
    status = o.alloc(0x200)
    o.write_u32(interface + INTERFACE_STATUS, status)
    o.stub(MY_INTERFACE, eax=interface)
    o.stub(MOST_INFLUENTIAL, eax=0, pop=0)
    rows = []
    for alignment in camera_inputs(rng):
        o.stub(PLAYER_ALIGNMENT, eax=0, pop=0, st0=f32(alignment))
        o.call(CAMERA_ALIGNMENT)
        rows.append([f2u(alignment), o.read_u32(LAND_VALUE)[0], o.read_u32(audio + AUDIO_ALIGNMENT)[0]])
    return rows


def sky_inputs(rng: random.Random) -> list[tuple[float, float, int]]:
    cases = [(1.0, 1.0, 16), (1.0, 2.0, 500), (1.0, 0.0, 2000), (2.0, 0.0, 0), (0.5, 0.5, 0), (0.0, 2.0, 10000)]
    for _ in range(1500):
        sky = f32_round(rng.uniform(0.0, 2.0))
        land = f32_round(rng.uniform(0.0, 2.0)) if rng.random() < 0.8 else f32_round(sky + rng.uniform(-0.01, 0.01))
        milliseconds = rng.choice([rng.randint(0, 40), rng.randint(0, 400), rng.randint(0, 5000)])
        cases.append((sky, land, milliseconds))
    return cases


def sky_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    for address in SKY_CALLS:
        o.stub(address)
    rows = []
    for sky, land, milliseconds in sky_inputs(rng):
        o.write_f32(SKY_VALUE, sky)
        o.write_f32(LAND_VALUE, land)
        o.write_u32(FRAME_MILLISECONDS, milliseconds)
        o.call(DRAW_SKY)
        rows.append([f2u(sky), f2u(land), milliseconds, o.read_u32(SKY_VALUE)[0]])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x5E2240)
    with Oracle() as o:
        n = write_pinned(out / "alignment_store.txt", "The value the land keeps for the camera's alignment",
                         ["The alignment of the player of most influence at the camera in; the value of the land",
                          "(0 good, 2 evil) and the value of the audio out (float bits)."],
                         ["alignment", "land", "audio"], camera_cases(o, rng))
        print(f"alignment_store: {n} rows")
        o.reset()
        n = write_pinned(out / "sky_land.txt", "The sky's move to the value of the land in a frame",
                         ["The value of the sky, the value of the land (float bits) and the frame's whole",
                          "milliseconds in; the value of the sky after the frame out (float bits)."],
                         ["sky", "land", "milliseconds", "after"], sky_cases(o, rng))
        print(f"sky_land: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
