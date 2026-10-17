#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the float sweep for audio, land, nature and animals in test/oracle/data:

- cone_weights.txt: the land texture's cone weights of a cell's four corners, the whole table
- fft.txt: the lip sync's fast Fourier transform, on random data of each size, both ways
- sky_type.txt: the sky type at a time of day, for some day and night times
- village_light.txt: the village lights' intensity at a time of day
- atmos_alignment.txt: the value the ambience keeps for the camera's alignment
- zoomer.txt: the zoomer's glide, from its targets and its steps
- grazer_steer.txt: the grazers' whole number scale and their steering push
- sound_scan.txt: the sound map scan's distance to a cell and the cell's whole metres

Usage: python tools/oracle/cases/float_sweep_3_cases.py [output folder]   (default: test/oracle/data)
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
CONE_WEIGHTS = 0x871560  # cdecl (), fills CONE_TABLE
CONE_TABLE = 0xE3A3E0  # 256 texels, 4 ints each
FOUR1 = 0x428D50  # (data, nn, isign), the caller removes the arguments
SKY_TYPE = 0x86A1B0  # thiscall (sky), (time) -> ST0
SKY_TIMES = {"night_full": 0xFA26A0, "dusk_start": 0xFA269C, "dusk_end": 0xFA2698, "day_full": 0xFA2694}
VILLAGE_INTENSITY = 0x86C220  # cdecl () -> ST0
SKY_HOUR = 0x86A160  # cdecl (), the hour of the day -> ST0
ATMOS_ALIGNMENT = 0x5E2240  # stdcall (goodness), stores the value in the audio object
AUDIO_POINTER = 0xCD3B20
AUDIO_ALIGNMENT = 0x190
ZOOMER_SET = 0x407D60  # thiscall (zoomer), (target, target speed, seconds)
ZOOMER_UPDATE = 0x442720  # thiscall (zoomer), (seconds)
GRAZER_SCALE = 0x41A590  # (value, num, den) -> EAX, removes 12 bytes
GRAZER_STEER = 0x41A5B0  # thiscall (animal, speed at +0x5A), (step, push) -> EAX used up
SCAN_ADD = 0x71D510  # thiscall (scan of a type), (centre, cell)


def f32_round(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def cone_cases(o: Oracle) -> list[list[int]]:
    o.call(CONE_WEIGHTS)
    words = o.read_u32(CONE_TABLE, 256 * 4)
    return [[texel] + words[texel * 4:texel * 4 + 4] for texel in range(256)]


def fft_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    rows = []
    case = 0
    for n in (2, 4, 8, 16, 32, 64, 128, 256):
        for sign in (1, -1):
            for _ in range(2):
                data = [f2u(rng.uniform(-1.0, 1.0) * rng.choice((1.0, 0.001, 300.0))) for _ in range(2 * n)]
                buffer = o.alloc(8 * n)
                o.write_u32(buffer, *data)
                o.call(FOUR1, args=[buffer, n, sign & 0xFFFFFFFF])
                out = o.read_u32(buffer, 2 * n)
                rows += [[case, n, sign & 0xFFFFFFFF, i, data[i], out[i]] for i in range(2 * n)]
                case += 1
    return rows


def sky_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    sky = o.alloc(0x100)
    rows = []
    time_sets = [(4.5, 6.0, 7.0, 8.5), (5.0, 6.5, 7.25, 9.0), (3.3, 5.7, 6.1, 7.9)]
    for _ in range(3):
        a = rng.uniform(2.0, 5.0)
        b = a + rng.uniform(0.1, 2.0)
        c = b + rng.uniform(0.1, 2.0)
        time_sets.append((a, b, c, c + rng.uniform(0.1, 2.0)))
    for times in time_sets:
        bits = [f2u(t) for t in times]
        for name, value in zip(SKY_TIMES, bits):
            o.write_u32(SKY_TIMES[name], value)
        hours = [rng.uniform(0.0, 24.0) for _ in range(150)] + [u2f(b) for b in bits] + [12.0, 0.0, 24.0, 11.999]
        for hour in hours:
            hour_bits = f2u(hour)
            r = o.call(SKY_TYPE, this=sky, args=[hour_bits])
            rows.append(bits + [hour_bits, r.st0f])
    return rows


def village_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    rows = []
    hours = [rng.uniform(0.0, 24.0) for _ in range(200)] + [rng.uniform(5.5, 7.5) for _ in range(100)]
    hours += [rng.uniform(16.0, 18.0) for _ in range(100)] + [16.5, 17.5, 6.0, 7.0, 16.500002, 6.999999]
    for hour in hours:
        hour_bits = f2u(hour)
        o.stub(SKY_HOUR, st0=f32.of_bits(hour_bits))
        r = o.call(VILLAGE_INTENSITY)
        rows.append([hour_bits, r.st0f])
    return rows


def alignment_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    audio = o.alloc(0x200)
    o.write_u32(AUDIO_POINTER, audio)
    rows = []
    alignments = [rng.uniform(-1.2, 1.2) for _ in range(400)] + [-1.0, 1.0, 0.0, -0.6, 0.2, 1e-9, -1e-9, 3.0, -3.0]
    for alignment in alignments:
        a = f32_round(alignment)
        # The caller's two float steps, exact in single precision as the FPU rounds them
        goodness = f32_round(f32_round(a + 1.0) * 0.5)
        o.call(ATMOS_ALIGNMENT, args=[f2u(goodness)])
        rows.append([f2u(a), f2u(goodness), o.read_u32(audio + AUDIO_ALIGNMENT)[0]])
    return rows


class Zoomer:
    def __init__(self, o: Oracle):
        self.o = o
        self.address = o.alloc(0x40)

    def start(self, value: int) -> None:
        self.o.write(self.address, bytes(0x40))
        self.o.write_u32(self.address, value)

    def state(self) -> tuple[int, int]:
        words = self.o.read_u32(self.address, 4)
        return words[0], words[3]


def zoomer_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    zoomer = Zoomer(o)
    rows = []
    for case in range(60):
        start = f2u(rng.uniform(-50.0, 50.0))
        zoomer.start(start)
        rows.append([case, 0, start, 0, 0, *zoomer.state()])
        for _ in range(rng.randint(1, 3)):
            target = f2u(rng.uniform(-80.0, 80.0))
            speed = f2u(rng.choice((0.0, rng.uniform(-5.0, 5.0))))
            seconds = f2u(rng.choice((rng.uniform(0.0005, 0.002), rng.uniform(0.1, 5.0), rng.uniform(5.0, 60.0))))
            o.call(ZOOMER_SET, this=zoomer.address, args=[target, speed, seconds])
            rows.append([case, 1, target, speed, seconds, *zoomer.state()])
            for _ in range(rng.randint(3, 25)):
                dt = f2u(rng.choice((0.033333335, 0.1, rng.uniform(0.001, 0.5))))
                o.call(ZOOMER_UPDATE, this=zoomer.address, args=[dt])
                rows.append([case, 2, dt, 0, 0, *zoomer.state()])
    return rows


def grazer_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    animal = o.alloc(0x100)
    step = o.alloc(0x10)
    push = o.alloc(0x10)
    rows = []
    for _ in range(300):
        value = rng.randint(-60000, 60000)
        den = rng.randint(1, 20000)
        num = rng.choice((den * 9 // 10, den // 5, den * 3 // 5, rng.randint(0, 40000)))
        r = o.call(GRAZER_SCALE, args=[value & 0xFFFFFFFF, num, den])
        rows.append([0, value & 0xFFFFFFFF, num, den, 0, 0, r.eax, 0, 0])
    # Large products that a float holds only to its 24 bits
    for _ in range(100):
        value = rng.randint(-2000000, 2000000)
        num = rng.randint(1000, 2000000)
        den = rng.randint(1, 3000000)
        r = o.call(GRAZER_SCALE, args=[value & 0xFFFFFFFF, num, den])
        rows.append([0, value & 0xFFFFFFFF, num, den, 0, 0, r.eax, 0, 0])
    for _ in range(400):
        speed = rng.randint(1, 20000)
        sx, sz = rng.randint(-speed, speed), rng.randint(-speed, speed)
        px, pz = rng.randint(-3 * speed, 3 * speed), rng.randint(-3 * speed, 3 * speed)
        o.write(animal + 0x5A, struct.pack("<H", speed))
        o.write_u32(step, sx & 0xFFFFFFFF, sz & 0xFFFFFFFF)
        o.write_u32(push, px & 0xFFFFFFFF, pz & 0xFFFFFFFF)
        r = o.call(GRAZER_STEER, this=animal, args=[step, push])
        out = o.read_u32(step, 2)
        rows.append([1, speed, sx & 0xFFFFFFFF, sz & 0xFFFFFFFF, px & 0xFFFFFFFF, pz & 0xFFFFFFFF, r.eax, out[0],
                     out[1]])
    return rows


def scan_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    scan = o.alloc(0x10)
    centre = o.alloc(0x10)
    cell = o.alloc(0x10)
    rows = []
    for _ in range(600):
        cx, cz = rng.randint(0, 512 * 65536 - 1), rng.randint(0, 512 * 65536 - 1)
        gx = max(0, min(511, (cx >> 16) + rng.randint(-12, 12)))
        gz = max(0, min(511, (cz >> 16) + rng.randint(-12, 12)))
        x, z = (gx << 16) | 0x8000, (gz << 16) | 0x8000
        o.write_u32(scan, 0, f2u(65535.0), 0)
        o.write_u32(centre, cx, cz, 0)
        o.write_u32(cell, x, z, 0)
        o.call(SCAN_ADD, this=scan, args=[centre, cell])
        words = o.read_u32(scan, 3)
        rows.append([cx, cz, x, z, words[1], words[2] & 0xFFFF, words[2] >> 16])
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0xF5_3)
    with Oracle() as o:
        n = write_pinned(out / "cone_weights.txt", "The land texture's cone weights of a cell's four corners",
                         ["The whole table the game makes at start-up: texel i * 16 + j, the four weights as ints."],
                         ["texel", "w0", "w1", "w2", "w3"], cone_cases(o))
        print(f"cone_weights: {n} rows")
        o.reset()
        n = write_pinned(out / "fft.txt", "The lip sync's fast Fourier transform",
                         ["Each case: n complex values, a sign of 1 or -1; the float bits in and out per index."],
                         ["case", "n", "sign", "index", "input", "output"], fft_cases(o, rng))
        print(f"fft: {n} rows")
        o.reset()
        n = write_pinned(out / "sky_type.txt", "The sky type at a time of day",
                         ["The four day and night times, the time in, the sky type out (float bits)."],
                         ["night_full", "dusk_start", "dusk_end", "day_full", "time", "sky"], sky_cases(o, rng))
        print(f"sky_type: {n} rows")
        o.reset()
        n = write_pinned(out / "village_light.txt", "The village lights' intensity at a time of day",
                         ["The hour in, the intensity 0 to 255 out (float bits)."],
                         ["hour", "intensity"], village_cases(o, rng))
        print(f"village_light: {n} rows")
        o.reset()
        n = write_pinned(out / "atmos_alignment.txt", "The value the ambience keeps for the camera's alignment",
                         ["alignment in; goodness = (alignment + 1) * 0.5 in single precision, as the caller makes",
                          "it; the value the game stores out (float bits)."],
                         ["alignment", "goodness", "value"], alignment_cases(o, rng))
        print(f"atmos_alignment: {n} rows")
        o.reset()
        n = write_pinned(out / "zoomer.txt", "The zoomer's glide",
                         ["Rows in order for each case. kind 0: a new zoomer at value a. kind 1: a target a, its",
                          "speed b, in c seconds. kind 2: a step of a seconds. value and speed after each row."],
                         ["case", "kind", "a", "b", "c", "value", "speed"], zoomer_cases(o, rng))
        print(f"zoomer: {n} rows")
        o.reset()
        n = write_pinned(out / "grazer_steer.txt", "The grazers' whole number scale and steering push",
                         ["kind 0: Scale(a, b, c) = result. kind 1: speed a, step (b, c), push (d, e): used-up",
                          "result, the step (x, z) after. All ints."],
                         ["kind", "a", "b", "c", "d", "e", "result", "x", "z"], grazer_cases(o, rng))
        print(f"grazer_steer: {n} rows")
        o.reset()
        n = write_pinned(out / "sound_scan.txt", "The sound map scan's distance to a cell",
                         ["The scan's centre and a cell centre in 16.16 map units in; the squared distance in metres",
                          "(float bits) and the cell centre's whole metres out."],
                         ["centre_x", "centre_z", "cell_x", "cell_z", "distance_squared", "metres_x", "metres_z"],
                         scan_cases(o, rng))
        print(f"sound_scan: {n} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
