#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of more creature formulas (part 2 of the creature maths sweep), in test/oracle/data:

- creature_faint_turns.txt: the game turns that a knocked-out creature lies, from its size.
- creature_life_after_fight.txt: the life of a creature after it pays for its fight.
- creature_blow_push.txt: where a striker and its opponent go when their bodies overlap on a blow.
- creature_pull_apart.txt: where two fighters go when their bodies overlap in the stance.
- creature_stroke_wait.txt: the wait in milliseconds before the next stroke, from the sum of the feedback.
- creature_stroke_sum.txt and creature_slap_sum.txt: the sum of the feedback after a stroke or a slap.
- creature_rested_openness.txt: how far a creature opens its eyes after it stops an action, from its exhaustion.
- creature_eye_look_seconds.txt: the time that the eyes take to turn to a new look, from the openness.
- creature_throw_velocity.txt: the speed of a thing that a creature throws as it lets go.
- creature_cast_stamina.txt: the stamina that a miracle costs in a fight.
- creature_cast_magnitude.txt: how big a miracle is that a creature casts.

Most rows come from parts of larger functions: the script runs only the game's own instructions of that part (see
the Part class and tools/oracle/README.md). The pull apart runs the whole function.

Usage: python tools/oracle/cases/creature_maths_2_cases.py [output folder]   (default: test/oracle/data)
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

# The functions and parts (BW1W120, runblack.exe 1.20)
FAINT_FIRST = 0x49B807  # the wait of the death agenda: (size x 4 + 8) x turns a second, to an integer
FAINT_AFTER = 0x49B837
TURN_MS = 0xD01A38  # the milliseconds of a game turn (100), set at start-up
LIFE_FIRST = 0x484685  # the end of a fight: life - (1 - fight health) x 0.25, stored as the argument of SetLife
LIFE_AFTER = 0x4846A3
PUSH_FIRST = 0x48B33E  # the push apart on a blow, in the function where a blow lands
PUSH_AFTER = 0x48B434  # after the striker and the opponent move
PUSH_APART = 0x48B49D  # the bodies do not overlap
PULL_APART = 0x48B4B0  # thiscall on the 3D creature: the pull apart in the stance
FORWARD_EXTENT = 0x48B650  # thiscall on a 3D creature, ST0: how far the body reaches in front of it
SET_POSITION = 0x6182F0  # thiscall (position), RET 4
STROKE_WAIT_FIRST = 0x5B2154  # (1 - sum) x 0.5 x 2500, to an integer
STROKE_WAIT_AFTER = 0x5B2171
SLAP_FIRST = 0x5B1ED4  # sum - n x 0.1, n doubled when the sum is past 0.25, not below -1
SLAP_AFTER = 0x5B1F1F
STROKE_FIRST = 0x5B2243  # sum + 0.1, not above 1
STROKE_AFTER = 0x5B226C
REST_EYES = 0x475CC0  # thiscall on the creature: the eyes after it stops an action
BODY_OF = 0x477850  # thiscall on the creature: its 3D creature
LOOK_FIRST = 0x48E3C9  # 0.3 / (openness + 2)
LOOK_AFTER = 0x48E3E8
STONED_FIRST = 0x48E5BB  # the look time of stoned eyes: x 5
STONED_AFTER = 0x48E5C9
THROW_FIRST = 0x48D0A2  # the speed of a thrown thing: (target - release - (0, half g t^2, 0)) / t
THROW_AFTER = 0x48D134
STAMINA_FIRST = 0x4F79B4  # the stamina cost of a miracle in a fight: cost / 10000, from 0 to 1
STAMINA_AFTER = 0x4F79F1
STAMINA_TABLE = 0xCC66A8  # the cost to create of each miracle, rows of 0x70 bytes
SIZE_FIRST = 0x4F7C46  # the magnitude for a thing: its size x 1.1
SIZE_AFTER = 0x4F7C54
CREATURE_FIRST = 0x4F7C65  # the magnitude for a creature: (its scale x 15) x 1.1
CREATURE_AFTER = 0x4F7C77
FIRE_FIRST = 0x4F7C93  # the magnitude of a fire miracle: the caster's height / 10, at most 1
FIRE_AFTER = 0x4F7CB6

# Fields of our test objects
POSITION = 0x78
HEADING = 0x84
PARTNER = 0x528C
PARTNER_ACTION = 0x49A0
ARENA = 0x4A98  # centre x, (y), centre z, radius
FIGHT_HEALTH = 0x4AA8
EXTENT = 0x52F0  # not a field of the game: the forward extent that our stub gives
SET_TO = 0x52E0  # not a field of the game: where our SetPosition stub puts the position, and a flag after it
SUM = 0xD8


class Part:
    """Runs a part of a game function with inputs on the stack, in registers and on the FPU.

    The code goes into free bytes at the end of the image (the host calls only addresses in the image). Each exit of
    the part is a 'PUSH back; RET' over its first instruction. An exit has an adjust: the bytes that the part pushed
    (> 0) or popped (< 0) before it, so that the stack frame of our code comes back as it was.
    """

    FRAME = 0x100
    REGISTERS = {"eax": 0xB8, "ecx": 0xB9, "edx": 0xBA, "ebx": 0xBB, "ebp": 0xBD, "esi": 0xBE, "edi": 0xBF}

    def __init__(self, oracle: Oracle, code: int, first: int, exits: list[tuple[int, int]]):
        self.o = oracle
        self.code = code
        self.first = first
        self.exits = exits
        self.values = oracle.alloc(0x40)
        self.out = oracle.alloc(0x40)

    def run(self, registers: dict[str, int], stack: dict[int, int], fpu: list[int], read: list[int]) -> tuple:
        """Runs the part. read are offsets from the stack pointer at the exit. Gives the result, the index of the
        exit and the values read."""
        o = self.o
        code = bytearray(b"\x55\x53\x56\x57")  # push ebp, ebx, esi, edi
        code += b"\x81\xec" + struct.pack("<I", self.FRAME)  # sub esp, FRAME
        for offset, value in stack.items():
            code += b"\xc7\x84\x24" + struct.pack("<iI", offset, value & 0xFFFFFFFF)
        if fpu:
            o.write_u32(self.values, *fpu)
        for i in reversed(range(len(fpu))):
            code += b"\xd9\x05" + struct.pack("<I", self.values + 4 * i)  # fld dword [value]: fpu[0] is ST0
        for name, value in registers.items():
            code += bytes([self.REGISTERS[name]]) + struct.pack("<I", value & 0xFFFFFFFF)
        jump = self.code + len(code)
        code += b"\xe9" + struct.pack("<i", self.first - (jump + 5))  # jmp first
        backs = []
        for index, (_, adjust) in enumerate(self.exits):
            backs.append(self.code + len(code))
            for i, offset in enumerate(read):
                code += b"\x8b\x84\x24" + struct.pack("<i", offset)  # mov eax, [esp + offset]
                code += b"\xa3" + struct.pack("<I", self.out + 4 * (1 + i))  # mov [out + 4 + 4i], eax
            code += b"\xc7\x05" + struct.pack("<II", self.out, index)  # mov dword [out], index
            code += b"\x8d\xa4\x24" + struct.pack("<i", adjust)  # lea esp, [esp + adjust]
            code += b"\x81\xc4" + struct.pack("<I", self.FRAME)  # add esp, FRAME
            code += b"\x5f\x5e\x5b\x5d\xc3"  # pop edi, esi, ebx, ebp; ret
        o.write(self.code, bytes(code))
        for (address, _), back in zip(self.exits, backs):
            o.write(address, b"\x68" + struct.pack("<I", back) + b"\xc3")  # push back; ret
        o.write_u32(self.out, 0xFFFFFFFF)
        result = o.call(self.code)
        values = o.read_u32(self.out, 1 + len(read))
        return result, values[0], values[1:]


class Maths:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        self.free = o.base + o.image_size - 0x1000

        # The forward extent of a body: our stub reads it from a field of our object
        o.write(FORWARD_EXTENT, b"\xd9\x81" + struct.pack("<I", EXTENT) + b"\xc3")  # fld dword [ecx + EXTENT]; ret
        # SetPosition: our stub keeps the position and sets a flag
        stub = bytearray(b"\x8b\x44\x24\x04")  # mov eax, [esp + 4]
        for i in range(3):
            stub += b"\x8b\x50" + bytes([4 * i])  # mov edx, [eax + 4i]
            stub += b"\x89\x91" + struct.pack("<I", SET_TO + 4 * i)  # mov [ecx + SET_TO + 4i], edx
        stub += b"\xc7\x81" + struct.pack("<II", SET_TO + 12, 1)  # mov dword [ecx + SET_TO + 12], 1
        stub += b"\xc2\x04\x00"  # ret 4
        o.write(SET_POSITION, bytes(stub))
        # The height of the land under a point: 0 (virtual slot 0x28, one argument)
        land = self.put(b"\xd9\xee\xc2\x04\x00")  # fldz; ret 4
        self.vtable = o.alloc(0x40)
        o.write_u32(self.vtable + 0x28, land)

        o.write_u32(TURN_MS, 100)
        self.body = o.alloc(0x5300)
        self.other = o.alloc(0x5300)

        self.faint = self.part(FAINT_FIRST, [(FAINT_AFTER, 0)])
        self.life = self.part(LIFE_FIRST, [(LIFE_AFTER, 4)])
        self.push = self.part(PUSH_FIRST, [(PUSH_AFTER, -4), (PUSH_APART, 0)])
        self.stroke_wait = self.part(STROKE_WAIT_FIRST, [(STROKE_WAIT_AFTER, 0)])
        self.slap = self.part(SLAP_FIRST, [(SLAP_AFTER, 0)])
        self.stroke = self.part(STROKE_FIRST, [(STROKE_AFTER, 0)])
        self.look = self.part(LOOK_FIRST, [(LOOK_AFTER, 0)])
        self.stoned = self.part(STONED_FIRST, [(STONED_AFTER, 0)])
        self.throw = self.part(THROW_FIRST, [(THROW_AFTER, 0)])
        self.stamina = self.part(STAMINA_FIRST, [(STAMINA_AFTER, 0)])
        self.size = self.part(SIZE_FIRST, [(SIZE_AFTER, 0)])
        self.creature_size = self.part(CREATURE_FIRST, [(CREATURE_AFTER, 0)])
        self.fire = self.part(FIRE_FIRST, [(FIRE_AFTER, 0)])
        self.creature = o.alloc(0x200)
        self.physical = o.alloc(0x100)
        o.write_u32(self.creature + 0x160, self.physical)

    def put(self, code: bytes) -> int:
        self.free -= (len(code) + 15) & ~15
        self.o.write(self.free, code)
        return self.free

    def part(self, first: int, exits: list[tuple[int, int]]) -> Part:
        self.free -= 0x200
        return Part(self.o, self.free, first, exits)

    def clear(self, obj: int) -> None:
        self.o.write(obj, bytes(0x5300))
        self.o.write_u32(obj, self.vtable)

    def faint_turns(self, size: int) -> int:
        self.clear(self.body)
        self.o.write_u32(self.body + 0x90, size)
        result, _, _ = self.checked(self.faint, {"eax": self.body, "ebp": 0}, {}, [], [])
        return result.eax

    def life_after(self, life: int, health: int) -> int:
        self.clear(self.body)
        self.o.write_u32(self.body + FIGHT_HEALTH, health)
        _, _, values = self.checked(self.life, {"edi": self.body, "ecx": 0}, {}, [life], [0])
        return values[0]

    @staticmethod
    def checked(part: Part, registers, stack, fpu, read):
        result, exit, values = part.run(registers, stack, fpu, read)
        if exit != 0 or result.depth != 0:
            raise RuntimeError(f"the part ended at exit {exit} with {result.depth} values on the FPU")
        return result, exit, values

    def place(self, obj: int, x: int, z: int, heading: int, extent: int) -> None:
        self.clear(obj)
        self.o.write_u32(obj + POSITION, x, 0, z, heading)
        self.o.write_u32(obj + EXTENT, extent)

    def blow_push(self, striker, opponent, heading, striker_extent, opponent_extent) -> tuple[int, ...]:
        """The positions after the push (x, z of each) and 1 when they moved."""
        self.place(self.body, *striker, heading, striker_extent)
        self.place(self.other, *opponent, 0, opponent_extent)
        self.o.write_u32(self.body + PARTNER, self.other)
        result, exit, _ = self.push.run({"esi": self.body, "ecx": self.other, "edi": 0, "ebp": 0}, {}, [], [])
        if result.depth != 0:
            raise RuntimeError("the push left values on the FPU")
        x, _, z = self.o.read_u32(self.body + POSITION, 3)
        ox, _, oz = self.o.read_u32(self.other + POSITION, 3)
        return x, z, ox, oz, int(exit == 0)

    def pull_apart(self, first, second, heading, first_extent, second_extent, centre, radius) -> tuple[int, ...]:
        """The positions after the pull apart (x, z of each) and a flag of each one that moved."""
        self.place(self.body, *first, heading, first_extent)
        self.place(self.other, *second, 0, second_extent)
        o = self.o
        o.write_u32(self.body + PARTNER, self.other)
        o.write_u32(self.other + PARTNER_ACTION, 0)
        o.write_u32(self.body + ARENA, centre[0], 0, centre[1], radius)
        result = o.call(PULL_APART, this=self.body)
        if result.depth != 0:
            raise RuntimeError("the pull apart left values on the FPU")
        x, _, z, moved = o.read_u32(self.body + SET_TO, 4)
        ox, _, oz, other_moved = o.read_u32(self.other + SET_TO, 4)
        return x, z, moved, ox, oz, other_moved

    def stroke_wait_ms(self, total: int) -> int:
        self.clear(self.body)
        self.o.write_u32(self.body + SUM, total)
        result, _, _ = self.checked(self.stroke_wait, {"ebx": self.body}, {}, [], [])
        return result.eax

    def after_slap(self, total: int, hard: bool) -> int:
        self.clear(self.body)
        self.o.write_u32(self.body + SUM, total)
        self.checked(self.slap, {"ebx": self.body}, {0x2C: 2 if hard else 1}, [], [])
        return self.o.read_u32(self.body + SUM)[0]

    def after_stroke(self, total: int) -> int:
        self.clear(self.body)
        self.o.write_u32(self.body + SUM, total)
        self.checked(self.stroke, {"ebx": self.body}, {}, [], [])
        return self.o.read_u32(self.body + SUM)[0]

    def rested_openness(self, exhaustion: int) -> int:
        o = self.o
        self.clear(self.body)
        o.stub(BODY_OF, eax=self.body, pop=0)
        o.write_u32(self.physical + 0x30, exhaustion)
        result = o.call(REST_EYES, this=self.creature)
        if result.depth != 0:
            raise RuntimeError("the eyes left values on the FPU")
        return o.read_u32(self.body + 0x5464)[0]

    def look_seconds(self, openness: int, stoned: bool) -> int:
        self.clear(self.body)
        self.o.write_u32(self.body + 0x5464, openness)
        _, _, values = self.checked(self.look, {"edi": self.body, "ebp": 0}, {}, [], [0x20])
        if stoned:
            _, _, values = self.checked(self.stoned, {}, {0x20: values[0]}, [], [0x20])
        return values[0]

    def throw_velocity(self, target, release, seconds: int) -> list[int]:
        self.clear(self.body)
        self.o.write_u32(self.body + 0x5720, *target, seconds)
        stack = {0x1C: release[0], 0x20: release[1], 0x24: release[2]}
        _, _, values = self.checked(self.throw, {"esi": self.body}, stack, [], [0x4C, 0x50, 0x54])
        return values

    def stamina_cost(self, cost: int) -> int:
        self.o.write_u32(STAMINA_TABLE, cost)
        _, _, values = self.checked(self.stamina, {"ebx": 0, "edi": 0}, {}, [], [0x10])
        return values[0]

    def magnitude_of_thing(self, size: int) -> int:
        self.clear(self.body)
        _, _, values = self.checked(self.size, {"edi": self.body}, {}, [size], [0x10])
        return values[0]

    def magnitude_of_creature(self, scale: int) -> int:
        self.clear(self.body)
        self.o.write_u32(self.body + 0x90, scale)
        result, exit, _ = self.creature_size.run({"eax": self.body}, {}, [], [])
        if exit != 0 or result.depth != 1:
            raise RuntimeError("the magnitude of a creature did not leave one value on the FPU")
        return result.st0f

    def magnitude_of_fire(self, height: int) -> int:
        _, _, values = self.checked(self.fire, {}, {}, [height], [0x10])
        return values[0]


def floats(rng: random.Random, low: float, high: float, edges: list[float], count: int) -> list[int]:
    values = [f2u(e) for e in edges]
    values += [f2u(rng.uniform(low, high)) for _ in range(count - len(values))]
    return values


def near(value: float, steps: int = 2) -> list[int]:
    """The float of a value and the floats a few steps below and above it"""
    bits = f2u(value)
    return [bits + d for d in range(-steps, steps + 1)]


def fight(m: Maths, rng: random.Random, out: Path) -> None:
    rows = []
    sizes = [f2u(s) for s in [0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 0.3, 0.7, 1.234]]
    # A size where (size x 4 + 8) x 10 is a whole number, and the floats next to it
    for n in range(81, 200, 3):
        sizes += near((n - 80) / 40.0)
    sizes += [f2u(rng.uniform(0.1, 4.0)) for _ in range(1500)]
    for size in sizes:
        rows.append((size, m.faint_turns(size)))
    write_pinned(out / "creature_faint_turns.txt", "The game turns that a knocked-out creature lies",
                 ["The game's count of turns for the wait of the death agenda: (size x 4 + 8) seconds times the turns",
                  "in a second (10, from a turn of 100 ms), to an integer. Made by the game's own instructions of that",
                  "part of the agenda."],
                 ["size", "turns"], rows)

    rows = []
    for health in floats(rng, -0.5, 1.0, [0.0, 1.0, -0.25, 0.5, 1e-8, 0.99999994], 1500):
        life = f2u(rng.choice([rng.uniform(0.0, 1.0), rng.uniform(-0.5, 1.0), 1.0, 0.01]))
        rows.append((life, health, m.life_after(life, health)))
    write_pinned(out / "creature_life_after_fight.txt", "The life of a creature after it pays for its fight",
                 ["The game's life after one payment at the end of a fight: the life less a quarter of the fight health",
                  "that the creature does not have. Made by the game's own instructions of that part."],
                 ["life", "health", "new_life"], rows)

    def fighters(count: int, overlap_share: float):
        for _ in range(count):
            x = rng.uniform(100.0, 3000.0)
            z = rng.uniform(100.0, 3000.0)
            heading = rng.uniform(-math.pi, math.pi)
            apart = rng.uniform(0.0, 40.0)
            way = rng.uniform(-math.pi, math.pi)
            ox = x + apart * math.sin(way)
            oz = z - apart * math.cos(way)
            a = rng.uniform(0.0, 30.0) if rng.random() < overlap_share else rng.uniform(0.0, 5.0)
            b = rng.uniform(0.0, 30.0) if rng.random() < overlap_share else rng.uniform(0.0, 5.0)
            yield (f2u(x), f2u(z)), (f2u(ox), f2u(oz)), f2u(heading), f2u(a), f2u(b)

    rows = []
    for first, second, heading, a, b in fighters(1500, 0.8):
        rows.append((*first, *second, heading, a, b, *m.blow_push(first, second, heading, a, b)))
    # Small positions, where a move can change the exponent
    for _ in range(100):
        first = (f2u(rng.uniform(0.0, 4.0)), f2u(rng.uniform(0.0, 4.0)))
        second = (f2u(rng.uniform(0.0, 4.0)), f2u(rng.uniform(0.0, 4.0)))
        heading, a, b = f2u(rng.uniform(-math.pi, math.pi)), f2u(rng.uniform(0.0, 8.0)), f2u(rng.uniform(0.0, 8.0))
        rows.append((*first, *second, heading, a, b, *m.blow_push(first, second, heading, a, b)))
    write_pinned(out / "creature_blow_push.txt", "Where a striker and its opponent go on a blow",
                 ["The game's positions of the striker and its opponent after the push apart on a turn of a blow. The",
                  "striker faces the heading; reach is how far the body of each one reaches in front of it. moved is",
                  "1 when the bodies overlap and both move. Made by the game's own instructions of that part."],
                 ["x", "z", "other_x", "other_z", "heading", "reach", "other_reach", "new_x", "new_z", "new_other_x",
                  "new_other_z", "moved"], rows)

    rows = []
    for first, second, heading, a, b in fighters(1500, 0.8):
        # The arena: its middle near the fighters, its radius such that some moves leave the range
        cx = f2u(u2f(first[0]) + rng.uniform(-30.0, 30.0))
        cz = f2u(u2f(first[1]) + rng.uniform(-30.0, 30.0))
        radius = f2u(rng.uniform(10.0, 80.0))
        rows.append((*first, *second, heading, a, b, cx, cz, radius,
                     *m.pull_apart(first, second, heading, a, b, (cx, cz), radius)))
    write_pinned(out / "creature_pull_apart.txt", "Where two fighters go when their bodies overlap",
                 ["The game's positions of the first fighter and its opponent after the pull apart in the stance. The",
                  "first one faces the heading; reach is how far the body of each one reaches in front of it; the",
                  "arena has its middle and radius. moved is 1 when that fighter moved (its new place is in 0.8 of the",
                  "radius). The whole function of the game ran; its calls to the reach and to set the position are",
                  "stubs."],
                 ["x", "z", "other_x", "other_z", "heading", "reach", "other_reach", "centre_x", "centre_z", "radius",
                  "new_x", "new_z", "moved", "new_other_x", "new_other_z", "other_moved"], rows)


def feedback(m: Maths, rng: random.Random, out: Path) -> None:
    edges = [-1.0, -0.5, 0.0, -0.0, 0.25, 0.5, 0.9, 1.0, 0.1, 0.2, 0.3, -0.9]
    sums = [f2u(e) for e in edges]
    # Sums where (1 - sum) x 1250 is near a whole number of milliseconds
    for n in range(1, 2500, 37):
        sums += near(1.0 - n / 1250.0)
    sums += [f2u(rng.uniform(-1.0, 1.0)) for _ in range(1500)]
    write_pinned(out / "creature_stroke_wait.txt", "The wait before the next stroke",
                 ["The game's wait in whole milliseconds before the next stroke, from the sum of the feedback so far.",
                  "Made by the game's own instructions of that part of the hand's update."],
                 ["sum", "wait"], [(s, m.stroke_wait_ms(s)) for s in sums])

    sums = [f2u(e) for e in edges] + near(0.25) + near(0.9) + near(-0.9) + near(-0.8)
    sums += [f2u(rng.uniform(-1.0, 1.0)) for _ in range(1500)]
    rows = []
    for s in sums:
        rows.append((s, 0, m.after_slap(s, False)))
        rows.append((s, 1, m.after_slap(s, True)))
    write_pinned(out / "creature_slap_sum.txt", "The sum of the feedback after a slap",
                 ["The game's sum of the feedback after a slap that plays. hard is 1 for a hard slap. Made by the game's",
                  "own instructions of that part of the hand's update."],
                 ["sum", "hard", "new_sum"], rows)

    sums = [f2u(e) for e in edges] + near(0.9) + near(-1.0)
    sums += [f2u(rng.uniform(-1.0, 1.0)) for _ in range(1500)]
    write_pinned(out / "creature_stroke_sum.txt", "The sum of the feedback after a stroke",
                 ["The game's sum of the feedback after a stroke that plays. Made by the game's own instructions of",
                  "that part of the hand's update."],
                 ["sum", "new_sum"], [(s, m.after_stroke(s)) for s in sums])


def eyes(m: Maths, rng: random.Random, out: Path) -> None:
    edges = [0.0, -0.0, 1.0, 0.5, 0.135, 0.13499999, 0.13500001, 1.0000001, 0.99999994, -0.5, 2.0]
    rows = [(e, m.rested_openness(e)) for e in floats(rng, -0.5, 1.5, edges, 1500)]
    rows += [(bits, m.rested_openness(bits)) for bits in [0x7FC00000, 0xFFC00000, 0x7F800000, 0xFF800000]]
    write_pinned(out / "creature_rested_openness.txt", "How far a creature opens its eyes after it stops an action",
                 ["The game's openness of the eyes from the exhaustion: 1 - 2 x exhaustion, from -1 to 0.73. A value",
                  "that is not a number gives -1. The whole function of the game ran."],
                 ["exhaustion", "openness"], rows)

    rows = []
    for openness in floats(rng, -1.0, 0.73, [-1.0, 0.0, 0.73, 0.5, -0.5], 1500):
        stoned = rng.random() < 0.3
        rows.append((openness, int(stoned), m.look_seconds(openness, stoned)))
    write_pinned(out / "creature_eye_look_seconds.txt", "The time that the eyes take to turn to a new look",
                 ["The game's time in seconds from the openness of the eyes: 0.3 / (openness + 2), five times as long",
                  "for stoned eyes. Made by the game's own instructions of those parts of the draw of the eyes."],
                 ["openness", "stoned", "seconds"], rows)


def throw(m: Maths, rng: random.Random, out: Path) -> None:
    rows = []
    for _ in range(1500):
        release = [rng.uniform(0.0, 3000.0), rng.uniform(0.0, 200.0), rng.uniform(0.0, 3000.0)]
        target = [release[0] + rng.uniform(-300.0, 300.0), rng.uniform(0.0, 200.0),
                  release[2] + rng.uniform(-300.0, 300.0)]
        seconds = f2u(rng.choice([rng.uniform(0.05, 10.0), rng.uniform(1.0, 4.0)]))
        target_bits = [f2u(v) for v in target]
        release_bits = [f2u(v) for v in release]
        rows.append((*target_bits, *release_bits, seconds, *m.throw_velocity(target_bits, release_bits, seconds)))
    write_pinned(out / "creature_throw_velocity.txt", "The speed of a thing that a creature throws",
                 ["The game's speed of a thrown thing as the creature lets go, from the target, the point where it lets",
                  "go and the time of the flight. Made by the game's own instructions of that part of the throw."],
                 ["target_x", "target_y", "target_z", "release_x", "release_y", "release_z", "seconds", "speed_x",
                  "speed_y", "speed_z"], rows)


def casting(m: Maths, rng: random.Random, out: Path) -> None:
    edges = [0.0, -0.0, 10000.0, 9999.999, 10000.001, -1.0, 5000.0, 1e-30]
    costs = floats(rng, -2000.0, 30000.0, edges, 1500) + [0x7FC00000, 0xFFC00000, 0x7F800000, 0xFF800000]
    write_pinned(out / "creature_cast_stamina.txt", "The stamina that a miracle costs in a fight",
                 ["The game's stamina cost of a miracle from its cost to create: the cost / 10000, from 0 to 1. A cost",
                  "that is not a number costs 0. Made by the game's own instructions of that part of the cast."],
                 ["cost", "stamina"], [(c, m.stamina_cost(c)) for c in costs])

    rows = []
    for size in floats(rng, 0.0, 200.0, [0.0, 1.0, 10.0], 700):
        rows.append((0, size, m.magnitude_of_thing(size)))
    for scale in floats(rng, 0.0, 4.0, [0.0, 1.0, 0.5], 700):
        rows.append((1, scale, m.magnitude_of_creature(scale)))
    heights = floats(rng, 0.0, 30.0, [0.0, 10.0, 9.999999, 10.000001, 15.0], 700) + [0x7FC00000, 0x7F800000]
    for height in heights:
        rows.append((2, height, m.magnitude_of_fire(height)))
    write_pinned(out / "creature_cast_magnitude.txt", "How big a miracle is that a creature casts",
                 ["The game's magnitude of a cast. kind 0: at a thing, from its size; kind 1: at a creature, from its",
                  "scale; kind 2: a fire miracle, from the caster's height. Made by the game's own instructions of",
                  "those parts of the cast."],
                 ["kind", "value", "magnitude"], rows)


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261112)
    with Oracle() as o:
        m = Maths(o)
        fight(m, rng, out)
        feedback(m, rng, out)
        eyes(m, rng, out)
        throw(m, rng, out)
        casting(m, rng, out)
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
