#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the land's detail lines in test/oracle/data:

- land_detail_lines.txt: the line where the plane at a depth along the camera's view meets the ground, as the game
  keeps it (a unit direction along the ground and a point on it), for the mesh lines (ground at height 0) and the
  texture line (ground at the camera's height, at most 110.55). A camera that looks straight down keeps the line of
  before.
- land_morph.txt: a block's corner altitudes morphed towards the half or the quarter mesh across a mesh line's band
- land_cross_fade.txt: how opaque a block's coarse texture is at each corner across the texture line's band

Usage: python tools/oracle/cases/land_detail_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
TEXTURE_LINE_UPDATE = 0x7FEE60  # thiscall (line record): the ground at min(camera y, 110.55)
MESH_LINE_UPDATE = 0x7FF0E0  # thiscall (line record): the ground at height 0
MORPH = 0x877D20  # fastcall: ECX block copy, EDX 1 for the half mesh, 0 for the quarter mesh
FADE_START = 0x8756BD  # inside the block draw: the cross-fade alpha of each vertex
FADE_END = 0x8757CE  # the first instruction after the cross-fade loop
FADE_CODE = 0x874AA0  # the block draw's entry, which the cases do not call: the code that runs the loop goes there

# The camera the lines are made from
CAMERA_POSITION = 0xEA1DB8
CAMERA_FORWARD = 0xEA1DD4

# The line records: direction x, y, z (+0), point x, y, z (+0xC), valid (+0x18), depth (+0x1C), width (+0x20),
# band (+0x24)
HALF_LINE = 0xE9C4E0
QUARTER_LINE = 0xE9C4B8
TEXTURE_LINE = 0xE9C508

# The block copy: 17 x 17 cells of 8 bytes, the altitude at +4; the block's lowest x and z
BLOCK_X = 0x90C
BLOCK_Z = 0x910
BLOCK_UV_REBASE = 0x9C4

# The vertices of the block draw: 0x20 bytes each, the diffuse colour at +0x10, u (along z) at +0x18, v (along x) at
# +0x1C; their count
VERTICES = 0xE437E0
VERTEX_COUNT = 0xEA9EA8

SIDE = 17
CORNERS = SIDE * SIDE
STALE = [0.25, 0.0, -0.5, 7.0, 0.0, 9.0]


def f32(value: float) -> float:
    return u2f(f2u(value))


def unit_forward(yaw: float, pitch: float) -> list[float]:
    """A camera direction of unit length in floats: pitch below the horizon, yaw about the vertical"""
    x = math.cos(pitch) * math.sin(yaw)
    y = -math.sin(pitch)
    z = math.cos(pitch) * math.cos(yaw)
    return [f32(x), f32(y), f32(z)]


def cameras(rng: random.Random) -> list[tuple[list[float], list[float]]]:
    result = []
    for _ in range(700):
        position = [f32(rng.uniform(0.0, 5120.0)), f32(rng.uniform(5.0, 2500.0)), f32(rng.uniform(0.0, 5120.0))]
        forward = unit_forward(rng.uniform(-math.pi, math.pi), rng.uniform(0.02, 1.45))
        result.append((position, forward))
    position = [f32(2000.5), f32(300.25), f32(1800.75)]
    # Along the axes, on the diagonals, both signs
    for yaw in (0.0, math.pi / 2, math.pi, -math.pi / 2, math.pi / 4, 3 * math.pi / 4, -math.pi / 4, -3 * math.pi / 4):
        result.append((position, unit_forward(yaw, 0.6)))
    # Near straight down, about the 0.0001 limit of the squared directions, and straight down
    for horizontal in (0.0, 0.005, 0.0099, 0.01, 0.0101, 0.0141, 0.02, 0.05):
        for yaw in (0.0, 0.3, math.pi / 4, 2.0):
            x = horizontal * math.sin(yaw)
            z = horizontal * math.cos(yaw)
            result.append((position, [f32(x), f32(-math.sqrt(max(0.0, 1.0 - horizontal * horizontal))), f32(z)]))
    # A camera at the texture line's ground height and below it
    for height in (110.55, 110.0, 111.0, 20.0):
        result.append(([f32(1000.0), f32(height), f32(1000.0)], unit_forward(0.7, 0.5)))
    return result


def line_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    rows = []
    record = o.alloc(0x40)
    for position, forward in cameras(rng):
        o.write_f32(CAMERA_POSITION, *position)
        o.write_f32(CAMERA_FORWARD, *forward)
        for kind, function, depth in ((0, MESH_LINE_UPDATE, 1600.0), (0, MESH_LINE_UPDATE, 2500.0),
                                      (0, MESH_LINE_UPDATE, 300.0), (1, TEXTURE_LINE_UPDATE, 800.0),
                                      (1, TEXTURE_LINE_UPDATE, 600.0), (1, TEXTURE_LINE_UPDATE, 300.0)):
            o.write_f32(record, *STALE)
            o.write_u32(record + 0x18, 0)
            o.write_f32(record + 0x1C, depth)
            r = o.call(function, this=record)
            if r.depth != 0:
                raise RuntimeError("the line update left values on the FPU")
            out = o.read_u32(record, 7)
            rows.append([kind] + [f2u(v) for v in position] + [f2u(v) for v in forward] + [f2u(depth)] + out)
    return rows


def altitudes_of(seed: int) -> list[int]:
    """The altitudes of a block's corners for a case, as the test makes them again"""
    out = []
    for corner in range(CORNERS):
        value = (seed * 2654435761 + corner * 40503) & 0xFFFFFFFF
        out.append((value >> 13) & 0xFF)
    return out


def pack(values: list[int]) -> list[int]:
    padded = values + [0] * (-len(values) % 4)
    return [struct.unpack("<I", bytes(padded[i:i + 4]))[0] for i in range(0, len(padded), 4)]


def lines_near(o: Oracle, rng: random.Random, depth: float, texture: bool, count: int) -> list[tuple]:
    """Lines from random cameras, and the block corners that lie near each line"""
    record = o.alloc(0x40)
    result = []
    while len(result) < count:
        position = [f32(rng.uniform(1000.0, 4000.0)), f32(rng.uniform(5.0, 1500.0)), f32(rng.uniform(1000.0, 4000.0))]
        forward = unit_forward(rng.uniform(-math.pi, math.pi), rng.uniform(0.05, 1.3))
        o.write_f32(CAMERA_POSITION, *position)
        o.write_f32(CAMERA_FORWARD, *forward)
        o.write_f32(record + 0x1C, depth)
        o.call(TEXTURE_LINE_UPDATE if texture else MESH_LINE_UPDATE, this=record)
        dx, _, dz, px, _, pz = [u2f(v) for v in o.read_u32(record, 6)]
        if o.read_u32(record + 0x18)[0] == 0:
            continue
        # A block whose middle is a little before or beyond the line
        along = rng.uniform(-800.0, 800.0)
        across = rng.uniform(-250.0, 250.0)
        mx = px + along * dx + across * dz
        mz = pz + along * dz - across * dx
        bx = math.floor((mx - 80.0) / 160.0) * 160.0
        bz = math.floor((mz - 80.0) / 160.0) * 160.0
        if bx < 0.0 or bz < 0.0:
            continue
        result.append((o.read_u32(record, 6), f32(bx), f32(bz)))
    return result


def morph_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    rows = []
    block = o.alloc(0x940)
    for kind, line, depth in ((1, HALF_LINE, 1600.0), (0, QUARTER_LINE, 2500.0), (1, HALF_LINE, 300.0)):
        width = 550.0 if depth != 300.0 else 50.0
        for line_bits, bx, bz in lines_near(o, rng, depth, False, 100 if depth != 300.0 else 40):
            seed = rng.randint(0, 0x7FFFFFFF)
            heights = altitudes_of(seed)
            cells = bytearray(0x908)
            for corner, height in enumerate(heights):
                cells[corner * 8 + 4] = height
            o.write(block, bytes(cells))
            o.write_f32(block + BLOCK_X, bx, bz)
            o.write_u32(line, *line_bits)
            o.write_f32(line + 0x20, width)
            r = o.call(MORPH, ecx=block, edx=kind)
            if r.depth != 0:
                raise RuntimeError("the morph left values on the FPU")
            out = o.read(block, 0x908)
            morphed = [out[corner * 8 + 4] for corner in range(CORNERS)]
            rows.append([kind, f2u(bx), f2u(bz), line_bits[0], line_bits[2], line_bits[3], line_bits[5], f2u(width),
                         seed] + pack(morphed))
    return rows


def fade_code(block: int) -> bytes:
    """x86 code that runs the block draw's cross-fade loop for block with a frame of its own, then returns"""
    code = bytes.fromhex("55535657")  # push ebp, ebx, esi, edi
    code += bytes.fromhex("89e5")  # mov ebp, esp
    code += bytes.fromhex("81ec00010000")  # sub esp, 0x100
    code += b"\xbf" + struct.pack("<I", block)  # mov edi, block
    return code


def fade_cases(o: Oracle, rng: random.Random) -> list[list[int]]:
    rows = []
    block = o.alloc(0xA00)
    trampoline = FADE_CODE
    code = fade_code(block)
    call_at = trampoline + len(code)
    code += b"\xe8" + struct.pack("<i", FADE_START - (call_at + 5))  # call the loop
    code += bytes.fromhex("89ec5f5e5b5dc3")  # mov esp, ebp; pop edi, esi, ebx, ebp; ret
    o.write(trampoline, code)
    # The loop ends at a return
    o.write(FADE_END, b"\xc3")
    for depth, width, band in ((800.0, 170.0, 85.0), (600.0, 170.0, 85.0), (300.0, 40.0, 20.0), (400.0, 70.0, 35.0)):
        for line_bits, bx, bz in lines_near(o, rng, depth, True, 60):
            o.write_f32(block + BLOCK_X, bx, bz)
            o.write_f32(block + BLOCK_UV_REBASE, 0.0, 0.0)
            o.write_u32(TEXTURE_LINE, *line_bits)
            o.write_f32(TEXTURE_LINE + 0x20, width, band)
            vertices = bytearray(CORNERS * 0x20)
            for x in range(SIDE):
                for z in range(SIDE):
                    at = (x * SIDE + z) * 0x20
                    vertices[at + 0x10:at + 0x14] = struct.pack("<I", 0xFF123456)
                    vertices[at + 0x18:at + 0x1C] = struct.pack("<f", z * 0.0625)
                    vertices[at + 0x1C:at + 0x20] = struct.pack("<f", x * 0.0625)
            o.write(VERTICES, bytes(vertices))
            o.write(VERTEX_COUNT, struct.pack("<H", CORNERS))
            r = o.call(trampoline)
            if r.depth != 0:
                raise RuntimeError("the cross-fade left values on the FPU")
            out = o.read(VERTICES, CORNERS * 0x20)
            alphas = [out[corner * 0x20 + 0x13] for corner in range(CORNERS)]
            rows.append([f2u(bx), f2u(bz), line_bits[0], line_bits[2], line_bits[3], line_bits[5], f2u(width),
                         f2u(band)] + pack(alphas))
    return rows


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x7E66A1)
    packed = [f"r{i}" for i in range((CORNERS + 3) // 4)]
    with Oracle() as o:
        lines = line_cases(o, rng)
        morphs = morph_cases(o, rng)
        fades = fade_cases(o, rng)
    count = write_pinned(
        out / "land_detail_lines.txt", "The land's detail lines (0x7FF0E0 mesh, 0x7FEE60 texture)",
        ["kind 0: a mesh line, the ground at height 0; kind 1: the texture line, the ground at min(camera y, 110.55)",
         "Before each call the record holds dir (0.25, 0, -0.5), point (7, 0, 9), valid 0: a camera that looks",
         "straight down keeps them. dx, dy, dz: the unit direction along the line; px, py, pz: a point on it."],
        ["kind", "cx", "cy", "cz", "fx", "fy", "fz", "depth", "dx", "dy", "dz", "px", "py", "pz", "valid"], lines)
    print(f"land_detail_lines.txt: {count} rows")
    count = write_pinned(
        out / "land_morph.txt", "A block's corner altitudes morphed towards the coarser mesh (0x877D20)",
        ["kind 1: towards the half mesh, 0: towards the quarter mesh. The line: dx, dz, px, pz, and its width.",
         "The block's corner altitudes come from the seed (see altitudes_of); r0.. hold the morphed altitudes",
         "of the corners [x * 17 + z], four a word, the first in the low byte."],
        ["kind", "bx", "bz", "dx", "dz", "px", "pz", "width", "seed"] + packed, morphs)
    print(f"land_morph.txt: {count} rows")
    count = write_pinned(
        out / "land_cross_fade.txt", "How opaque a block's coarse texture is at each corner (0x8756BD..0x8757CE)",
        ["The texture line: dx, dz, px, pz, its width and its band. Each vertex starts with the alpha 255;",
         "r0.. hold the alpha of the corners [x * 17 + z], four a word, the first in the low byte."],
        ["bx", "bz", "dx", "dz", "px", "pz", "width", "band"] + packed, fades)
    print(f"land_cross_fade.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
