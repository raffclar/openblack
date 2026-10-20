#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes test/oracle/data/zoom_to_places.txt: the temple and realm keys of the world camera.

The game's own key code runs on chosen cameras, places, times and states. It gives back the flight that it starts
(the place, what the camera looks at and the rise), the state that it keeps (the time of the last tap, whether the
camera was flown to a place, and the view to fly back to).

The land's altitude is a fake: (map x + map z) times a constant, in the game's own x87 steps. The test uses the same
fake, so the rows also pin the map position that the key asks the altitude of. The click distance is an input: the
row gives it, as the camera's click parameters keep it.

Usage: python tools/oracle/cases/zoom_to_places_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
ZOOM_TO = 0x457B60  # thiscall (camera mode), (x, z, distance, pitch, tap allowed), RET 0x14
CLICK_PARAMS = 0x459610  # thiscall, 3 arguments: stubbed, the row gives the click distance
FLY_TO = 0x4587F0  # thiscall (origin, focus, rise): replaced with code that keeps its arguments
ALTITUDE = 0x803090  # thiscall (map position) -> ST0: replaced with the fake altitude
HELP_CALLBACK = 0x449140  # cdecl, 3 arguments: stubbed

# The camera mode's fields
MODE_CAMERA = 0x4
MODE_STATE = 0x8
MODE_SAVED_ORIGIN = 0x24
MODE_SAVED_FOCUS = 0x30
MODE_FIGHT = 0x44
MODE_CLICK_DISTANCE = 0x1C0
MODE_LAST_TAP = 0x2F8
# The camera's current focus and origin, x y z
CAMERA_FOCUS = (0x88, 0xB8, 0xE8)
CAMERA_ORIGIN = (0x118, 0x148, 0x178)

# The fake altitude's constant, as bits
ALTITUDE_SCALE_BITS = 0x35C80000

# The key's distances and pitches (the temple's, the realm's)
TEMPLE_DISTANCE = 130.0
TEMPLE_PITCH_BITS = 0x3F490FD8
REALM_DISTANCE = 2200.0
REALM_PITCH_BITS = 0x3EC90FDB

F = np.float32


def bits(value) -> int:
    return int(np.array(value, dtype=np.float32).view(np.uint32))


def capture_code(buffer: int) -> bytes:
    """Machine code that keeps the flight's origin, focus and rise in the buffer, counts the calls and returns"""
    code = b""
    for argument, offset in ((4, 0), (8, 12)):
        code += bytes([0x8B, 0x44, 0x24, argument])  # mov eax, [esp + argument]
        for part in range(3):
            code += bytes([0x8B, 0x50, part * 4])  # mov edx, [eax + part]
            code += bytes([0x89, 0x15]) + struct.pack("<I", buffer + offset + part * 4)  # mov [buffer], edx
    code += bytes([0x8B, 0x44, 0x24, 0x0C])  # mov eax, [esp + 12]
    code += bytes([0xA3]) + struct.pack("<I", buffer + 24)  # mov [buffer + 24], eax
    code += bytes([0xFF, 0x05]) + struct.pack("<I", buffer + 28)  # inc dword [buffer + 28]
    code += bytes([0xC2, 0x0C, 0x00])  # ret 12
    return code


def altitude_code(scale: int) -> bytes:
    """Machine code of the fake altitude: (map x + map z) times the scale, left on the FPU"""
    code = bytes([0xDB, 0x01])  # fild dword [ecx]
    code += bytes([0xDB, 0x41, 0x04])  # fild dword [ecx + 4]
    code += bytes([0xDE, 0xC1])  # faddp
    code += bytes([0xD8, 0x0D]) + struct.pack("<I", scale)  # fmul dword [scale]
    code += bytes([0xC3])  # ret
    return code


def cases(rng: random.Random) -> list[dict]:
    rows = []

    def camera() -> tuple[list, list]:
        focus = [F(rng.uniform(500.0, 4500.0)), F(rng.uniform(0.0, 120.0)), F(rng.uniform(500.0, 4500.0))]
        distance = rng.uniform(5.0, 600.0)
        heading = rng.uniform(-math.pi, math.pi)
        pitch = rng.uniform(-0.3, 1.5)
        origin = [
            F(focus[0] + distance * math.cos(pitch) * math.sin(heading)),
            F(focus[1] + distance * math.sin(pitch)),
            F(focus[2] + distance * math.cos(pitch) * math.cos(heading)),
        ]
        return origin, focus

    for index in range(1200):
        origin, focus = camera()
        if index % 40 == 0:
            # Straight above, where the heading is 0
            origin = [F(focus[0] + rng.uniform(-0.009, 0.009)), F(focus[1] + 200.0), F(focus[2] + rng.uniform(-0.009, 0.009))]
        if index % 97 == 0:
            # Where the camera looks at itself
            focus = list(origin)
        key = rng.choice(["temple", "temple", "no_temple", "realm"])
        now = rng.randrange(1000, 1 << 31)
        since = rng.choice([0, 1, 250, 499, 500, 501, 502, 1000, 100000, now])
        state = rng.choice([0, 0, 1])
        saved_origin, saved_focus = camera()
        if key == "temple":
            place = [F(rng.uniform(500.0, 4500.0)), F(rng.uniform(500.0, 4500.0))]
            distance, pitch, tap = F(TEMPLE_DISTANCE), f32.of_bits(TEMPLE_PITCH_BITS), 1
        else:
            place = [F(rng.uniform(1000.0, 4000.0)), F(rng.uniform(1000.0, 4000.0))]
            distance, pitch, tap = F(REALM_DISTANCE), f32.of_bits(REALM_PITCH_BITS), 1 if key == "no_temple" else 0
        if state == 1 and rng.random() < 0.5:
            # The camera still looks near the place it was flown to
            angle = rng.uniform(-math.pi, math.pi)
            reach = rng.choice([0.0, 50.0, 99.99, 100.0, 100.01, 150.0])
            focus = [F(place[0] + reach * math.cos(angle)), focus[1], F(place[1] + reach * math.sin(angle))]
        rows.append(
            {
                "origin": origin,
                "focus": focus,
                "click": F(rng.choice([50.0, rng.uniform(1.0, 2000.0)])),
                "fight": 1 if rng.random() < 0.1 else 0,
                "state": state,
                "saved_origin": saved_origin,
                "saved_focus": saved_focus,
                "now": now,
                "last": (now - since) & 0xFFFFFFFF,
                "place": place,
                "distance": distance,
                "pitch": pitch,
                "tap": tap,
            }
        )
    return rows


COLUMNS = (
    ["ox", "oy", "oz", "fx", "fy", "fz", "click", "fight", "state"]
    + ["sox", "soy", "soz", "sfx", "sfy", "sfz", "now", "last", "px", "pz", "distance", "pitch", "tap"]
    + ["flights", "rox", "roy", "roz", "rfx", "rfy", "rfz", "rise", "state_after", "last_after"]
    + ["sox_after", "soy_after", "soz_after", "sfx_after", "sfy_after", "sfz_after"]
)


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x7E3B1E)
    rows = []
    with Oracle() as o:
        buffer = o.alloc(0x40)
        scale = o.alloc(0x10)
        o.write_u32(scale, ALTITUDE_SCALE_BITS)
        o.write(FLY_TO, capture_code(buffer))
        o.write(ALTITUDE, altitude_code(scale))
        o.stub(CLICK_PARAMS, pop=12)
        o.stub(HELP_CALLBACK, pop=0)
        table = o.alloc(0x80)
        o.write_u32(table + 0x50, FLY_TO)
        cam = o.alloc(0x200)
        mode = o.alloc(0x300)
        for case in cases(rng):
            o.write(buffer, bytes(0x40))
            o.write(mode, bytes(0x300))
            o.write_u32(mode, table)
            o.write_u32(mode + MODE_CAMERA, cam)
            o.write_u32(mode + MODE_STATE, case["state"])
            o.write_f32(mode + MODE_SAVED_ORIGIN, *[float(v) for v in case["saved_origin"]])
            o.write_f32(mode + MODE_SAVED_FOCUS, *[float(v) for v in case["saved_focus"]])
            o.write_u32(mode + MODE_FIGHT, case["fight"])
            o.write_f32(mode + MODE_CLICK_DISTANCE, float(case["click"]))
            o.write_u32(mode + MODE_LAST_TAP, case["last"])
            for offset, value in zip(CAMERA_FOCUS, case["focus"]):
                o.write_f32(cam + offset, float(value))
            for offset, value in zip(CAMERA_ORIGIN, case["origin"]):
                o.write_f32(cam + offset, float(value))
            o.stub_import("KERNEL32.dll!GetTickCount", eax=case["now"])
            o.call(
                ZOOM_TO,
                this=mode,
                args=[f32(float(case["place"][0])), f32(float(case["place"][1])), f32(float(case["distance"])),
                      case["pitch"] if isinstance(case["pitch"], f32) else f32(float(case["pitch"])), case["tap"]],
            )
            flight = o.read_u32(buffer, 8)
            state_after = o.read_u32(mode + MODE_STATE)[0]
            last_after = o.read_u32(mode + MODE_LAST_TAP)[0]
            saved = o.read_u32(mode + MODE_SAVED_ORIGIN, 6)
            pitch_bits = case["pitch"].bits if isinstance(case["pitch"], f32) else bits(case["pitch"])
            rows.append(
                [bits(v) for v in case["origin"]]
                + [bits(v) for v in case["focus"]]
                + [bits(case["click"]), case["fight"], case["state"]]
                + [bits(v) for v in case["saved_origin"]]
                + [bits(v) for v in case["saved_focus"]]
                + [case["now"], case["last"], bits(case["place"][0]), bits(case["place"][1])]
                + [bits(case["distance"]), pitch_bits, case["tap"]]
                + [flight[7]] + flight[0:7]
                + [state_after, last_after]
                + saved
            )
    notes = [
        "The temple and realm keys of the world camera, run on the game's own code at 24-bit precision.",
        "Inputs: the camera's origin and focus, the click distance, a fight in view, the flown-to state and the saved",
        "view, the time now and of the last tap (ms), the place (x, z), its distance and pitch, and whether a tap is",
        "allowed (the temple key). Outputs: the flights started (0 or 1), the flight's origin, focus and rise, then the",
        "state, the last tap and the saved view after the press.",
        f"The altitude is a fake: (map x + map z) times the float with bits {ALTITUDE_SCALE_BITS:08x}, in x87 steps.",
    ]
    count = write_pinned(out / "zoom_to_places.txt", "The temple and realm keys", notes, COLUMNS, rows)
    print(f"zoom_to_places.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
