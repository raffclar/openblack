#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the expected results of the ambience volume scenarios (test/audio/scenarios/volumes_*.json) again on the real
FPU at the game's 24-bit precision.

The scenarios came from test/audio/scripts/emu_soundmap.py, which runs the same functions under an emulator. That
emulator starts its FPU at 64-bit precision, not at the game's 24-bit precision, so its results are not the game's.
This script keeps each scenario's inputs (the island, the sound info, the queries, the alignments, the sky cases and
the bank steps) and writes the game's results in "expected".

Usage: python tools/oracle/cases/atmos_volumes_cases.py [scenario.json ...]
(default: test/audio/scenarios/volumes_1.json and volumes_2.json)
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

SOUND_MAP_UPDATE = 0x71D6F0
PROCESS_ATMOS_BANKS = 0x428FE0
ALIGNMENT = 0x5E2240
SKY_SET_TIME = 0x86A2C0
GET_CAMERA = 0x54C180
DEBUG_SET_MESSAGE = 0x511DA0

G_GAME = 0xD0195C
G_GLOBAL_AUDIO = 0xCD3B20
CAMERA_POSITION = 0xEA1DB8
INDEX_BLOCK = 0xE9C964
PTR_BLOCKS = 0xE9C564
SKY_TYPE = 0xFA26BC
SKY_TIMES = 0xFA2694  # dayFull, duskEnd, duskStart, nightFull
SOUND_INFO = 0xD9A908
IMPORT_SET_BANK_VOLUME = 0x8A9788
IMPORT_SET_GROUP = 0x8A978C
CELL_COUNT = 17 * 17


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def u2f(value: int) -> float:
    return struct.unpack("<f", struct.pack("<I", value))[0]


def recorder(o: Oracle) -> tuple[int, int, int]:
    """Two stdcall functions of two arguments in the arena: one keeps its second argument in a list, one does nothing.

    Gives (recorder, nothing, list); the list's first word counts the values after it.
    """
    values = o.alloc(0x1000)
    code = o.alloc(0x40)
    keep = (b"\xA1" + struct.pack("<I", values)                   # mov eax, [values]
            + b"\x8B\x4C\x24\x08"                                # mov ecx, [esp + 8]
            + b"\x89\x0C\x85" + struct.pack("<I", values + 4)     # mov [values + 4 + eax * 4], ecx
            + b"\x40"                                            # inc eax
            + b"\xA3" + struct.pack("<I", values)                 # mov [values], eax
            + b"\xC2\x08\x00")                                   # ret 8
    nothing = b"\x31\xC0\xC2\x08\x00"                            # xor eax, eax; ret 8
    o.write(code, keep)
    o.write(code + 0x20, nothing)
    return code, code + 0x20, values


def run_sound_map(o: Oracle, scenario: dict) -> list[dict]:
    o.write(INDEX_BLOCK, bytes.fromhex(scenario["lookup"]))
    for index, cells in enumerate(scenario["blocks"], start=1):
        block = o.alloc(CELL_COUNT * 8)
        o.write(block, bytes.fromhex(cells))
        o.write_u32(PTR_BLOCKS + index * 4, block)
    for i, value in enumerate(scenario["soundInfo"]):
        o.write_u32(SOUND_INFO + i * 4, value)

    game = o.alloc(0x260000)
    o.write_u32(G_GAME, game)
    o.write_u32(game + 0x59B8 + 0x28, 8)
    o.write_u32(game + 0x59B8 + 0x2C, 0x2000)
    o.write_u32(game + 0x59B8 + 0x30, 0x2000)
    camera = o.alloc(0x200)
    o.write_u32(game + 0x2502C0, camera)
    o.stub(GET_CAMERA, eax=camera)
    o.stub(DEBUG_SET_MESSAGE, eax=0)
    sound_map = o.alloc(0x110)

    results = []
    for query in scenario["queries"]:
        o.write_u32(CAMERA_POSITION, query["x"], query["y"], query["z"])
        weather = query["weather"]
        o.write(camera + 0x80, bytes([0, weather[0] & 0xFF, weather[1] & 0xFF, 0, weather[2] & 0xFF, weather[3] & 0xFF,
                                      0, 0]))
        o.write_u32(SKY_TYPE, query["sky"])
        o.call(SOUND_MAP_UPDATE, this=sound_map)
        results.append({"volumes": o.read_u32(sound_map + 0xB4, 14), "heightAboveLand": o.read_u32(sound_map + 0x108)[0]})
    return results


def run_alignment(o: Oracle, values: list[int]) -> list[int]:
    audio = o.alloc(0x300)
    o.write_u32(G_GLOBAL_AUDIO, audio)
    results = []
    for value in values:
        goodness = bits(f32(f32(u2f(value) + 1.0) * 0.5))
        o.call(ALIGNMENT, args=[goodness])
        results.append(o.read_u32(audio + 0x190)[0])
    return results


def run_sky(o: Oracle, cases: list[dict]) -> list[int]:
    results = []
    for case in cases:
        o.write_u32(SKY_TIMES, *case["times"])
        o.call(SKY_SET_TIME, args=[case["time"]])
        results.append(o.read_u32(SKY_TYPE)[0])
    return results


def run_banks(o: Oracle, steps: list[dict]) -> list[dict]:
    keep, nothing, values = recorder(o)
    o.write_u32(IMPORT_SET_BANK_VOLUME, keep)
    o.write_u32(IMPORT_SET_GROUP, nothing)
    audio = o.alloc(0x300)
    o.write_u32(audio + 0x14, o.alloc(0x100))
    for i in range(14):
        o.write_u32(audio + 0x194 + 4 * i, 0x100 + i)
    results = []
    for step in steps:
        o.write_u32(audio + 0x1CC, *step["targets"])
        o.write_u32(audio + 0x204, *step["current"])
        o.write_u32(values, 0)
        o.call(PROCESS_ATMOS_BANKS, this=audio)
        count = o.read_u32(values)[0]
        sent = o.read_u32(values + 4, count) if count else []
        results.append({"current": o.read_u32(audio + 0x204, 14),
                        "sent": [v - (1 << 32) if v & 0x80000000 else v for v in sent]})
    return results


def main() -> int:
    paths = [Path(p) for p in sys.argv[1:]] or [ROOT / "test" / "audio" / "scenarios" / f"volumes_{i}.json"
                                                for i in (1, 2)]
    for path in paths:
        scenario = json.loads(path.read_text())
        old = scenario["expected"]
        with Oracle() as o:
            expected = {"soundMap": run_sound_map(o, scenario)}
            o.reset()
            expected["alignments"] = run_alignment(o, scenario["alignments"])
            o.reset()
            expected["sky"] = run_sky(o, scenario["sky"])
            o.reset()
            expected["bankSteps"] = run_banks(o, scenario["bankSteps"])
        changed = {key: sum(1 for a, b in zip(old[key], expected[key]) if a != b) for key in expected}
        scenario["expected"] = expected
        with open(path, "w", newline="\n") as f:
            json.dump(scenario, f, separators=(",", ":"))
        print(f"{path.name}: results that changed from the emulator's: {changed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
