#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the box that the game puts round a mesh, in test/oracle/data:

- mesh_box.txt: the box of a mesh of one part with three bones. The game puts the bones in their rest pose (each
  bone's matrix through its parent's), moves each group of vertices by its bone, and takes the smallest and largest
  of each axis. The box keeps its middle, its half size and the length of its half diagonal. The draw distances of a
  model come from that length.

Each case has three bones: bone 0 has no parent, bone 1 hangs from bone 0, and bone 2 hangs from bone 0 or from bone 1
(the column parent2). Four vertices in one primitive: one on bone 2, one on bone 0 and two on bone 1. The bones'
turns are random and not unit, so that a turn that is not pure shows the order of the game's maths.

Usage: python tools/oracle/cases/mesh_box_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The function (BW1W120, runblack.exe 1.20)
COMPUTE_BOX = 0x8081B0  # thiscall on the mesh, no arguments
BONE_MATRICES = 0xC37D9C  # the 3D engine's pointer to its buffer of bone matrices

BONES = 3
VERTICES = 4
GROUPS = [(1, 2), (1, 0), (2, 1)]  # (vertex count, bone) in order
NO_PARENT = 0xFFFFFFFF

COLUMNS = (
    ["parent2"]
    + [f"bone{b}_{i}" for b in range(BONES) for i in range(12)]
    + [f"vertex{v}_{a}" for v in range(VERTICES) for a in "xyz"]
    + [f"centre_{a}" for a in "xyz"]
    + [f"half_{a}" for a in "xyz"]
    + ["half_diagonal"]
)


def rounded(value: float) -> int:
    return f2u(value)


class MeshBox:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.write_u32(BONE_MATRICES, o.alloc(0x30 * 16))
        self.mesh = o.alloc(0x40)
        self.part = o.alloc(0x20)
        self.parts = o.alloc(4)
        self.primitive = o.alloc(0x40)
        self.primitives = o.alloc(4)
        self.bones = o.alloc(0x3C * BONES)
        self.vertices = o.alloc(0x20 * VERTICES)
        self.groups = o.alloc(4 * len(GROUPS))
        o.write_u32(self.mesh + 0x4, 0x100)  # the mesh has bones
        o.write_u32(self.mesh + 0xC, 1)
        o.write_u32(self.mesh + 0x10, self.parts)
        o.write_u32(self.parts, self.part)
        o.write_u32(self.part + 0x0, 1)  # the part has bones
        o.write_u32(self.part + 0x4, 1)
        o.write_u32(self.part + 0x8, self.primitives)
        o.write_u32(self.part + 0xC, BONES)
        o.write_u32(self.part + 0x10, self.bones)
        o.write_u32(self.primitives, self.primitive)
        o.write_u32(self.primitive + 0x10, VERTICES)
        o.write_u32(self.primitive + 0x14, self.vertices)
        o.write_u32(self.primitive + 0x20, len(GROUPS))
        o.write_u32(self.primitive + 0x24, self.groups)
        for g, (count, bone) in enumerate(GROUPS):
            o.write_u32(self.groups + 4 * g, count | (bone << 16))

    def case(self, parent2: int, bones: list[list[int]], vertices: list[list[int]]) -> list[int]:
        o = self.o
        parents = [NO_PARENT, 0, parent2]
        for b in range(BONES):
            at = self.bones + 0x3C * b
            o.write_u32(at, parents[b])
            o.write(at + 0xC, struct.pack("<12I", *bones[b]))
        for v in range(VERTICES):
            o.write(self.vertices + 0x20 * v, struct.pack("<3I", *vertices[v]))
        o.call(COMPUTE_BOX, this=self.mesh)
        box = struct.unpack("<7I", o.read(self.mesh + 0x18, 0x1C))
        return [parent2] + [w for bone in bones for w in bone] + [w for v in vertices for w in v] + list(box)


def random_bone(rng: random.Random) -> list[int]:
    turn = [rounded(rng.uniform(-1.5, 1.5)) for _ in range(9)]
    place = [rounded(rng.uniform(-2.0, 2.0)) for _ in range(3)]
    return turn + place


def main() -> int:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(0x4D455348)
    rows = []
    with Oracle() as o:
        box = MeshBox(o)
        # A pure rest pose: every bone unturned in place, so the box is that of the vertices as they are
        unit = [f2u(v) for v in (1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)]
        rows.append(box.case(1, [unit] * BONES, [[f2u(c) for c in (x, -x, 0.5 * x)] for x in (0.25, -1, 2, 0.125)]))
        for i in range(1200):
            bones = [random_bone(rng) for _ in range(BONES)]
            vertices = [[rounded(rng.uniform(-1.0, 1.0)) for _ in range(3)] for _ in range(VERTICES)]
            rows.append(box.case(i % 2, bones, vertices))
    notes = [
        "The mesh box: bones put in their rest pose, vertices moved by their bones, smallest and largest per axis,",
        "then the middle, the half size and the half diagonal. Inputs and results are float bits.",
    ]
    count = write_pinned(out / "mesh_box.txt", "The box round a mesh with bones", notes, COLUMNS, rows)
    print(f"mesh_box.txt: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
