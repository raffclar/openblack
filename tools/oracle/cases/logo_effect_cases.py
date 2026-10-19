#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the camera maths that the logo effect uses, in test/oracle/data:

- logo_look_at.txt: the camera's axes and offsets from a position and a focus (the 3D engine's world to camera)
- logo_lens.txt: the clip scales and the near plane's extents from a view across and a near distance
- logo_project.txt: the pixel and the depth of a point through a clip matrix
- logo_unproject.txt: the point of the world under a pixel at a depth

Usage: python tools/oracle/cases/logo_effect_cases.py [output folder]   (default: test/oracle/data)
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
WORLD_TO_CAMERA = 0x819690  # thiscall (out matrix), EDX position, args focus, store flag; RET 8
CHANGE_FOV = 0x8195B0  # stdcall (fov); RET 4
PROJECT_POINT = 0x819390  # ECX point, EDX out x, args out y, out depth; RET 8
SCREEN_TO_WORLD = 0x81B370  # ECX pixel, EDX out point, arg depth; RET 4

# Globals
UP = 0xEA1B48  # the world's up, made once behind the guard byte
UP_GUARD = 0xEA1B54
NEAR = 0xE839E0
RESOLUTION = 0xE839E4  # x, y as ints
HALF = 0xE839F0  # x, y as floats
ASPECT = 0xE839EC
CLIP_SCALE = 0xE83A00  # across, up
EXTENT = 0xC3812C  # x, y
CLIP_MATRIX = 0xEA9E40  # 12 floats: columns across, up, depth
AXES = 0xEA1D28  # 12 floats, the world to camera matrix
CAMERA = 0xEA1DB8

LOOK_COLUMNS = ["px", "py", "pz", "fx", "fy", "fz", *[f"m{i}" for i in range(12)]]
LENS_COLUMNS = ["fov", "near", "aspect", "scale_x", "scale_y", "extent_x", "extent_y"]
PROJECT_COLUMNS = [*[f"c{i}" for i in range(12)], "near", "width", "height", "x", "y", "z",
                   "result", "pixel_x", "pixel_y", "depth"]
UNPROJECT_COLUMNS = [*[f"a{i}" for i in range(9)], "cx", "cy", "cz", "extent_x", "extent_y", "near", "width",
                     "height", "pixel_x", "pixel_y", "depth", "x", "y", "z"]


def u(x: float) -> int:
    return f2u(x)


def f32_round(x: float) -> float:
    return u2f(f2u(x))


class Camera:
    def __init__(self, o: Oracle):
        self.o = o
        self.out = o.alloc(0x40)
        self.position = o.alloc(0x10)
        self.focus = o.alloc(0x10)
        self.point = o.alloc(0x10)
        self.pixel = o.alloc(0x10)
        self.result = o.alloc(0x20)
        o.write(UP_GUARD, b"\x01")
        o.write_f32(UP, 0.0, 1.0, 0.0)

    def look_at(self, p: list[int], f: list[int]) -> list[int]:
        o = self.o
        o.write_u32(self.position, *p)
        o.write_u32(self.focus, *f)
        r = o.call(WORLD_TO_CAMERA, ecx=self.out, edx=self.position, args=[self.focus, 1])
        assert r.popped == 8, r
        return o.read_u32(self.out, 12)

    def lens(self, fov: int, near: int, aspect: int) -> list[int]:
        o = self.o
        o.write_u32(NEAR, near)
        o.write_u32(ASPECT, aspect)
        r = o.call(CHANGE_FOV, args=[f32.of_bits(fov)])
        assert r.popped == 4, r
        return o.read_u32(CLIP_SCALE, 2) + o.read_u32(EXTENT, 2)

    def project(self, matrix: list[int], near: int, width: int, height: int, point: list[int]) -> list[int]:
        o = self.o
        o.write_u32(CLIP_MATRIX, *matrix)
        o.write_u32(NEAR, near)
        o.write_u32(RESOLUTION, width, height)
        o.write_f32(HALF, width * 0.5, height * 0.5)
        o.write_u32(self.point, *point)
        o.write_u32(self.result, 0, 0, 0)
        r = o.call(PROJECT_POINT, ecx=self.point, edx=self.result, args=[self.result + 4, self.result + 8])
        assert r.popped == 8, r
        return [r.eax] + o.read_u32(self.result, 3)

    def unproject(self, axes: list[int], camera: list[int], extent: list[int], near: int, width: int, height: int,
                  pixel: tuple[int, int], depth: int) -> list[int]:
        o = self.o
        o.write_u32(AXES, *axes)
        o.write_u32(CAMERA, *camera)
        o.write_u32(EXTENT, *extent)
        o.write_u32(NEAR, near)
        o.write_f32(HALF, width * 0.5, height * 0.5)
        o.write_u32(self.pixel, pixel[0] & 0xFFFFFFFF, pixel[1] & 0xFFFFFFFF)
        r = o.call(SCREEN_TO_WORLD, ecx=self.pixel, edx=self.result, args=[f32.of_bits(depth)])
        assert r.popped == 4, r
        return o.read_u32(self.result, 3)


def vec(rng: random.Random, span: float) -> list[int]:
    return [u(rng.uniform(-span, span)) for _ in range(3)]


def look_cases(rng: random.Random) -> list[tuple[list[int], list[int]]]:
    cases = [([u(0.0), u(0.0), u(100.0)], [u(0.0)] * 3),
             ([u(0.0), u(100.0), u(0.0)], [u(0.0)] * 3),
             ([u(0.0), u(-50.0), u(0.0)], [u(0.0)] * 3),
             ([u(0.00005), u(10.0), u(0.0)], [u(0.0)] * 3),
             ([u(3.0)] * 3, [u(3.0)] * 3)]
    # The logo effect's camera: on its orbit, back at the logo, and in its flight through the logo
    for _ in range(400):
        c = [rng.uniform(-100.0, 100.0), rng.uniform(-100.0, 100.0), rng.uniform(-100.0, 100.0)]
        k = rng.choice([1.0, rng.uniform(-2.0, 1.0)])
        position = [u(f32_round(x * k)) for x in c]
        focus = [u(0.0)] * 3 if k == 1.0 else [u(f32_round(-x * (1.0 - k))) for x in c]
        cases.append((position, focus))
    for _ in range(600):
        cases.append((vec(rng, 300.0), vec(rng, 300.0)))
    return cases


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261019)
    with Oracle() as o:
        camera = Camera(o)
        looks = []
        for p, f in look_cases(rng):
            looks.append(p + f + camera.look_at(p, f))
        write_pinned(out / "logo_look_at.txt", "The camera's axes from a position and a focus (the game's own function)",
                     ["m0..m8 are the axes by rows (right, up and forward across each row), m9..m11 the offsets.",
                      "All values are float bits."], LOOK_COLUMNS, looks)

        lenses = []
        for fov in [1.2217305, 0.7853982, 1.0, 0.5]:
            for near in [0.1, 1.0, 0.75, 3.2]:
                for aspect in [4.0 / 3.0, 16.0 / 9.0, 1.25, 1.0]:
                    lenses.append([u(fov), u(near), u(aspect)] + camera.lens(u(fov), u(near), u(aspect)))
        for _ in range(500):
            fov, near, aspect = u(rng.uniform(0.3, 2.0)), u(rng.uniform(0.05, 10.0)), u(rng.uniform(0.5, 4.0))
            lenses.append([fov, near, aspect] + camera.lens(fov, near, aspect))
        write_pinned(out / "logo_lens.txt", "The clip scales and the near plane's extents (the game's own function)",
                     ["All values are float bits."], LENS_COLUMNS, lenses)

        projects = []
        unprojects = []
        for row in looks[:500]:
            matrix = row[6:18]
            scale = u(rng.uniform(0.5, 2.5))
            scale_up = u(u2f(scale) * rng.choice([4.0 / 3.0, 16.0 / 9.0]))
            near = u(rng.choice([0.1, 1.0]))
            width, height = rng.choice([(800, 600), (1024, 768), (1080, 810), (1920, 1440)])
            # The clip matrix by columns, as the camera fills it: the axes times the clip scales, one product each
            right, up, forward = [matrix[0], matrix[3], matrix[6]], [matrix[1], matrix[4], matrix[7]], \
                [matrix[2], matrix[5], matrix[8]]
            offsets = matrix[9:12]

            def times(bits: int, s: int) -> int:
                return u(struct.unpack("<f", struct.pack("<f", u2f(bits) * u2f(s)))[0])

            clip = [times(right[0], scale), times(up[0], scale_up), forward[0],
                    times(right[1], scale), times(up[1], scale_up), forward[1],
                    times(right[2], scale), times(up[2], scale_up), forward[2],
                    times(offsets[0], scale), times(offsets[1], scale_up), offsets[2]]
            position = row[0:3]
            for _ in range(4):
                point = vec(rng, 150.0)
                projects.append(clip + [near, width, height] + point +
                                camera.project(clip, near, width, height, point))
            extent = [u(rng.uniform(0.05, 0.5)), 0]
            extent[1] = u(f32_round(u2f(extent[0]) / rng.choice([4.0 / 3.0, 16.0 / 9.0])))
            for _ in range(4):
                pixel = (rng.randint(-200, width + 200), rng.randint(-200, height + 200))
                depth = u(rng.choice([0.0, rng.uniform(1.0, 200.0)]))
                unprojects.append(matrix[0:9] + position + extent + [near, width, height,
                                                                     pixel[0] & 0xFFFFFFFF, pixel[1] & 0xFFFFFFFF,
                                                                     depth] +
                                  camera.unproject(matrix[0:9], position, extent, near, width, height, pixel, depth))
        write_pinned(out / "logo_project.txt", "A point's pixel and depth through a clip matrix (the game's own function)",
                     ["c0..c11 are the clip matrix by rows of the world's axes: across, up, depth for x, y, z and the "
                      "offset. The products of the axes and the scales are single float multiplies.",
                      "width and height are ints; result is 0 behind the near plane, 1 off the screen, 2 on it.",
                      "pixel_x and pixel_y are ints; the rest are float bits."], PROJECT_COLUMNS, projects)
        write_pinned(out / "logo_unproject.txt", "The point under a pixel at a depth (the game's own function)",
                     ["a0..a8 are the camera's axes by rows; width, height and the pixel are ints; the rest float bits."],
                     UNPROJECT_COLUMNS, unprojects)
    print("done")


if __name__ == "__main__":
    main()
