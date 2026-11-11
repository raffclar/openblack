#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of creature formulas, part 3, in test/oracle/data:

- creature_point_aim.txt: how far across and how high the arm points, and the four weights of the point clips.
- creature_point_turn.txt: the angle from the heading to a point, as seen from the head, before a point.
- creature_hurl_hand_speed.txt: how fast the hand moves in the flat and the high throw.
- creature_hurl_release.txt: where the hand lets go of a thrown thing, from the two throw hands.
- creature_hurl_lift.txt: the speed along the ground and the height speed of a throw, at its start.
- creature_hurl_step.txt: the weights of the two throw clips and the milliseconds that they move on in a turn.
- creature_look_catch.txt: how much a thing that the eye sweep meets catches the creature's eye.
- creature_look_hold.txt: how much the sight that the creature watches holds its eye.
- creature_hair_scale.txt: the head factor of a young creature and the scale of its hair.
- creature_hair_seconds.txt: the seconds of the hair step from the milliseconds of the frame.
- creature_size_scale.txt: the kept size and the drawn scale of a creature.
- creature_toss_velocity.txt: the velocity of a thing that the creature lets go of.

Whole functions of the game run where they can. The parts of larger functions run as slices (see
tools/oracle/README.md): the game's own instructions of the part, with the inputs put where the part reads them.

Usage: python tools/oracle/cases/creature_maths_3_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, u2f, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
POINT_BLEND = 0x487CE0  # thiscall on the 3D creature (byte), RET 4: the aim and the weights of the point clips
CHOOSE_CLIP = 0x619650  # thiscall (clip, flag), RET 8: the clip of a number
MOVE_CLIP = 0x617A10  # thiscall (clip, time, step), RET 0xC: moves a clip on
START_TURN_TEST = 0x4849B0  # in the start of a point: MOV ECX,ESI; CALL to the head bone
START_TURN_AFTER = 0x4849F6  # POP EDI, after FABS: ST0 is the size of the angle, ST1 the angle
HEAD_BONE = 0x4813B0  # thiscall on the 3D creature: a pointer to the place of the head
HAND_SPEED_FIRST = 0x4EDDB4  # in the measure of the throw hands: the speed of the hand of one throw clip (EBX)
HAND_SPEED_AFTER = 0x4EDE1E  # INC EBX, the next clip
HURL_RELEASE = 0x48C540  # thiscall on the 3D creature (heading, slope, out), RET 0xC: where the hand lets go
MEASURE_HANDS = 0x4EDBF0  # thiscall on the 3D creature: measures the throw hands one time
THROW_START = 0x48CD40  # thiscall on the 3D creature, RET: the start of a throw, after any turn
HURL_STEP_FIRST = 0x48CED5  # in the throw handler: the weights of the two throw clips and the step of the turn
TOSS_FIRST = 0x48C745  # in the release of a held thing: the velocity of a thing that the creature lets go of
TOSS_AFTER = 0x48C7F5  # JMP after the velocity is on the stack
SIZE_FIRST = 0x480533  # in the 3D creature's size setter: the kept size and the drawn scale
SIZE_AFTER = 0x4805B7
HEAD_SCALE = 0x4EC450  # thiscall on the 3D creature, ST0: the head factor of a young creature
HAIR_SCALE_FIRST = 0x4ED546  # in the frame's draw preparation: the scale of the hair (head factor x size x 15)
HAIR_SCALE_AFTER = 0x4ED561  # JMP to the hair step, with the scale pushed
HAIR_SECONDS_FIRST = 0x4ED4F2  # in the frame's draw preparation: the seconds of the hair step
HAIR_SECONDS_AFTER = 0x4ED532
LOOK_CATCH_FIRST = 0x4D1DD3  # in the look interest test: CALL to the look cells, then the score of a thing
LOOK_CATCH_AFTER = 0x4D1E8E  # CALL to the hold, with the creature pushed
LOOK_HOLD = 0x4D1CB0  # thiscall on the look tracker (creature), RET 4, ST0: how much the watched sight holds the eye
LOOK_WATCHED_DISTANCE = 0x4D1C20  # thiscall on the look tracker (creature), RET 4, ST0: distance to the watched sight
MS_PER_TURN = 0xD01A38  # the milliseconds of a game turn
HURL_STEP_AFTER = 0x48CFDB  # PUSH EAX (the step); PUSH EDI; PUSH 0x5B; MOV ECX,ESI: the call that moves the clip on


class Slice:
    """Runs a part of a game function with inputs on the stack, in registers and on the FPU. After the part, the
    code stores the stack dwords that `read` names and pops `fpu_out` values from the FPU, first ST0."""

    FRAME = 0x100
    count = 0
    REGISTERS = {"eax": 0xB8, "ecx": 0xB9, "edx": 0xBA, "ebx": 0xBB, "ebp": 0xBD, "esi": 0xBE, "edi": 0xBF}

    def __init__(self, oracle: Oracle, first: int, after: int, drop: int = 0):
        o = self.o = oracle
        self.drop = drop
        self.first = first
        # The oracle calls only addresses in the image: the code goes in the last bytes of the image, which the
        # functions here never read. Other case scripts use the last slots, so these start further down.
        self.code = o.base + o.image_size - 0x200 * (8 + Slice.count)
        Slice.count += 1
        self.values = o.alloc(0x40)
        self.out = o.alloc(0x80)
        self.after = after

    def run(self, registers: dict[str, int], stack: dict[int, int], fpu: list[int], read: list[int],
            fpu_out: int = 0) -> tuple:
        o = self.o
        code = bytearray(b"\x55\x53\x56\x57")  # push ebp, ebx, esi, edi
        code += b"\x81\xec" + struct.pack("<I", self.FRAME)  # sub esp, FRAME
        for offset, value in stack.items():
            code += b"\xc7\x84\x24" + struct.pack("<iI", offset, value & 0xFFFFFFFF)
        if fpu:
            o.write_u32(self.values, *fpu)
        for i in range(len(fpu)):
            code += b"\xd9\x05" + struct.pack("<I", self.values + 4 * i)  # fld dword [value]
        for name, value in registers.items():
            code += bytes([self.REGISTERS[name]]) + struct.pack("<I", value & 0xFFFFFFFF)
        jump = self.code + len(code)
        code += b"\xe9" + struct.pack("<i", self.first - (jump + 5))  # jmp first
        back = self.code + len(code)
        code += b"\xa3" + struct.pack("<I", self.out + 0x7C)  # mov [out + 0x7c], eax
        for i, offset in enumerate(read):
            code += b"\x8b\x84\x24" + struct.pack("<i", offset)  # mov eax, [esp + offset]
            code += b"\xa3" + struct.pack("<I", self.out + 4 * i)  # mov [out + 4i], eax
        if self.drop:
            code += b"\x83\xc4" + bytes([self.drop])  # add esp, drop: what the part pushed for a call after it
        for i in range(fpu_out):
            code += b"\xd9\x1d" + struct.pack("<I", self.out + 0x40 + 4 * i)  # fstp dword [out + 0x40 + 4i]
        code += b"\x81\xc4" + struct.pack("<I", self.FRAME)  # add esp, FRAME
        code += b"\x5f\x5e\x5b\x5d\xc3"  # pop edi, esi, ebx, ebp; ret
        o.write(self.code, bytes(code))
        # push back; ret: the part ends with the stack as it was
        o.write(self.after, b"\x68" + struct.pack("<I", back) + b"\xc3")
        result = o.call(self.code)
        outputs = o.read_u32(self.out, len(read)) if read else []
        fpu_values = o.read_u32(self.out + 0x40, fpu_out) if fpu_out else []
        self.eax = o.read_u32(self.out + 0x7C)[0]
        return result, outputs, fpu_values


class Maths:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.stub(CHOOSE_CLIP, eax=0, pop=8)
        o.stub(MOVE_CLIP, eax=0, pop=0xC)
        self.body = o.alloc(0x5300)
        self.head = o.alloc(0x40)
        o.stub(HEAD_BONE, eax=self.head, pop=0)
        self.turn = Slice(o, START_TURN_TEST, START_TURN_AFTER)
        self.hand_speed = Slice(o, HAND_SPEED_FIRST, HAND_SPEED_AFTER)
        self.step_slice = Slice(o, HURL_STEP_FIRST, HURL_STEP_AFTER)
        o.stub(MEASURE_HANDS, eax=0, pop=0)
        self.catch_slice = Slice(o, LOOK_CATCH_FIRST, LOOK_CATCH_AFTER, drop=4)
        self.creature = o.alloc(0x400)
        self.toss_slice = Slice(o, TOSS_FIRST, TOSS_AFTER)
        self.size_slice = Slice(o, SIZE_FIRST, SIZE_AFTER, drop=8)
        self.hair_scale_slice = Slice(o, HAIR_SCALE_FIRST, HAIR_SCALE_AFTER, drop=4)
        self.hair_seconds_slice = Slice(o, HAIR_SECONDS_FIRST, HAIR_SECONDS_AFTER)
        self.mind = o.alloc(0x1B000)
        self.holder = o.alloc(0x100)
        self.tracker = o.alloc(0x40)
        o.write_u32(self.creature + 0x160, self.holder)
        o.write_u32(self.creature + 0x164, self.mind)
        o.write_u32(self.holder + 0x58, self.body)

    def toss(self, yaw: int, mirrored: int, velocity: list[int], scale: int) -> list[int]:
        o = self.o
        o.write_u32(self.body + 0x84, yaw)
        o.write_u32(self.body + 0x94, scale)
        o.write_u32(self.body + 0x490C, *velocity)
        o.write_u32(self.body + 0x5230, mirrored)
        result, values, _ = self.toss_slice.run({"esi": self.body}, {}, [], [0x10, 0x14, 0x18])
        if result.depth != 0:
            raise RuntimeError("the toss velocity left values on the FPU")
        return list(values)

    def size_and_scale(self, size: int, rest: int) -> tuple[int, int]:
        o = self.o
        o.write_u32(self.body + 0x8C, rest)
        result, _, _ = self.size_slice.run({"ecx": self.body}, {0xC: size}, [], [], fpu_out=1)
        kept, scale = o.read_u32(self.body + 0x90, 2)
        return kept, scale

    def hair_scale(self, size: int) -> tuple[int, int]:
        o = self.o
        o.write_u32(self.body + 0x90, size)
        head = o.call(HEAD_SCALE, this=self.body)
        if head.depth != 1:
            raise RuntimeError("the head factor did not leave one value")
        result, (scale,), _ = self.hair_scale_slice.run({"esi": self.body}, {}, [], [0])
        if result.depth != 0:
            raise RuntimeError("the hair scale left values on the FPU")
        return head.st0f, scale

    def hair_seconds(self, ms: int) -> int:
        result, (seconds,), _ = self.hair_seconds_slice.run({}, {0x1C: ms, 0x20: 0}, [], [0x10])
        if result.depth != 0:
            raise RuntimeError("the hair seconds left values on the FPU")
        return seconds

    def look_catch(self, size: int, weight: int, interest: int, distance: int, turns: int, ms: int) -> int:
        o = self.o
        o.write_u32(self.body + 0x90, size)
        o.write_u32(self.mind + 0x1AA2C, weight)
        o.write_u32(self.tracker + 0x1C, turns)
        o.write_u32(MS_PER_TURN, ms)
        result, (score,), _ = self.catch_slice.run({"edi": self.creature, "esi": self.tracker, "ecx": self.holder},
                                                  {0x1C: distance, 0x20: interest}, [], [0x20])
        if result.depth != 0:
            raise RuntimeError("the look score left values on the FPU")
        return score

    def look_hold(self, size: int, kind: int, weight: int, interest: int, distance: int) -> int:
        o = self.o
        o.write_u32(self.body + 0x90, size)
        o.write_u32(self.mind + 0x1AA2C + 4 * kind, weight)
        o.write_u32(self.tracker, 1, kind)
        o.write_u32(self.tracker + 0x18, interest)
        o.stub(LOOK_WATCHED_DISTANCE, eax=0, pop=4, st0=f32.of_bits(distance))
        result = o.call(LOOK_HOLD, this=self.tracker, args=[self.creature])
        if result.depth != 1 or result.popped != 4:
            raise RuntimeError("the look hold did not leave one value")
        return result.st0f

    def hurl_hand_speed(self, at: tuple, before: tuple, high: bool) -> int:
        o = self.o
        b = self.body
        o.write_u32(b + 0x4968, 0, 0, 0, 0)
        stack = {0xC: at[0], 0x10: at[1], 0x14: at[2], 0x18: before[0], 0x1C: before[1], 0x20: before[2]}
        result, _, _ = self.hand_speed.run({"esi": b, "ebx": 0x5C if high else 0x5B}, stack, [], [])
        if result.depth != 0:
            raise RuntimeError("the hand speed left values on the FPU")
        speeds = o.read_u32(b + 0x4968, 4)
        if high:
            if speeds[2] != speeds[3] or speeds[0] or speeds[1]:
                raise RuntimeError("the high throw keeps two different speeds")
            return speeds[2]
        if speeds[1] != speeds[0] ^ 0x80000000 or speeds[2] or speeds[3]:
            raise RuntimeError("the flat throw does not keep the speed and minus the speed")
        return speeds[0]

    def set_hands(self, flat: tuple, high: tuple, flat_speed: int, high_speed: int) -> None:
        o = self.o
        b = self.body
        o.write_u32(b + 0x4930, *high)
        o.write_u32(b + 0x493C, *flat)
        o.write_u32(b + 0x4968, flat_speed, flat_speed ^ 0x80000000, high_speed, high_speed)
        o.write_u32(b + 0x5258, 1)

    def hurl_release(self, place: tuple, scale: int, mirrored: bool, heading: int, slope: int) -> list[int]:
        o = self.o
        b = self.body
        o.write_u32(b + 0x78, *place)
        o.write_u32(b + 0x94, scale)
        o.write_u32(b + 0x5230, 1 if mirrored else 0)
        out = self.head + 0x10
        result = o.call(HURL_RELEASE, this=b, args=[heading, slope, out])
        if result.depth != 0 or result.popped != 0xC:
            raise RuntimeError("the release point left values on the FPU")
        return o.read_u32(out, 3)

    def throw_start(self, place: tuple, scale: int, mirrored: bool, target: tuple, seconds: int) -> list[int]:
        o = self.o
        b = self.body
        o.write_u32(b + 0x78, *place)
        o.write_u32(b + 0x94, scale)
        o.write_u32(b + 0x5230, 1 if mirrored else 0)
        o.write_u32(b + 0x5720, *target, seconds)
        result = o.call(THROW_START, this=b)
        if result.depth != 0:
            raise RuntimeError("the start of a throw left values on the FPU")
        return o.read_u32(b + 0x4978, 2)

    def hurl_step(self, lift: tuple, scale: int, turn_ms: int, clip_step: int, time: int, throw_ms: int) -> list[int]:
        o = self.o
        b = self.body
        o.write_u32(b + 0x518C, 1)
        o.write_u32(b + 0x4978, *lift)
        o.write_u32(b + 0x94, scale)
        o.write_u32(b + 0x48B4, turn_ms)
        o.write_u32(b + 0x48BC, clip_step)
        o.write_u32(b + 0x47D0, time)
        o.write_u32(b + 0x4964, throw_ms)
        result, _, _ = self.step_slice.run({"esi": b}, {}, [], [])
        if result.depth != 0:
            raise RuntimeError("the throw step left values on the FPU")
        return [*o.read_u32(b + 0x47E0, 2), self.step_slice.eax]

    def point_aim(self, place: tuple[int, int, int], size: int, yaw: int, target: tuple[int, int, int],
                  mirrored: bool) -> list[int]:
        o = self.o
        b = self.body
        o.write_u32(b + 0x78, *place)
        o.write_u32(b + 0x84, yaw)
        o.write_u32(b + 0x90, size)
        o.write_u32(b + 0x4A74, *target)
        o.write_u32(b + 0x518C, 1)  # the arm does not choose again
        o.write_u32(b + 0x522C, 1 if mirrored else 0)
        o.write_u32(b + 0x4994, 0)
        o.write_u32(b + 0x47D0, 0)
        o.write_u32(b + 0x4A80, 0)
        result = o.call(POINT_BLEND, this=b, args=[0])
        if result.depth != 0:
            raise RuntimeError("the point blend left values on the FPU")
        return [*o.read_u32(b + 0x5168, 1), *o.read_u32(b + 0x5170, 1), *o.read_u32(b + 0x47E0, 4)]

    def point_turn(self, head: tuple[int, int, int], yaw: int, target: tuple[int, int, int]) -> int:
        o = self.o
        b = self.body
        o.write_u32(self.head, *head)
        o.write_u32(b + 0x84, yaw)
        o.write_u32(b + 0x4A74, *target)
        result, _, (size, angle) = self.turn.run({"esi": b, "edi": b + 0x4A74}, {}, [], [], fpu_out=2)
        if result.depth != 0:
            raise RuntimeError("the turn test left values on the FPU")
        if f2u(abs(u2f(angle))) != size:
            raise RuntimeError("the size of the angle is not the angle without its sign")
        return angle


def floats(rng: random.Random, low: float, high: float, edges: list[float], count: int) -> list[int]:
    values = [f2u(e) for e in edges]
    values += [f2u(rng.uniform(low, high)) for _ in range(count - len(values))]
    return values


def point_cases(m: Maths, rng: random.Random, out: Path) -> None:
    rows = []
    for i in range(2000):
        place = (rng.uniform(-3000.0, 3000.0), rng.uniform(-50.0, 300.0), rng.uniform(-3000.0, 3000.0))
        size = rng.choice([rng.uniform(0.1, 4.0), 1.0, 0.5])
        yaw = rng.choice([rng.uniform(-math.pi, math.pi), rng.uniform(-7.0, 7.0), 0.0, math.pi, -math.pi])
        reach = rng.choice([rng.uniform(0.0, 200.0), rng.uniform(0.0, 0.2), rng.uniform(0.0, 2000.0)])
        angle = rng.uniform(-math.pi, math.pi)
        rise = rng.uniform(-2.0, 2.0) * reach
        origin_y = place[1] + size * 15.0
        target = (place[0] + reach * math.sin(angle), origin_y + rise, place[2] - reach * math.cos(angle))
        if i % 25 == 0:
            target = place[0], origin_y, place[2]  # no distance
        p = tuple(f2u(v) for v in place)
        t = tuple(f2u(v) for v in target)
        mirrored = rng.random() < 0.5
        rows.append((*p, f2u(size), f2u(yaw), *t, int(mirrored),
                     *m.point_aim(p, f2u(size), f2u(yaw), t, mirrored)))
    write_pinned(out / "creature_point_aim.txt", "The aim of a creature's point",
                 ["The game's share across (right) and share up (high) of the point, and the weights of the four",
                  "point clips, from the place, size and heading (yaw) of the creature and the point that it points",
                  "at. The arm points from the place, size x 15 higher. mirrored is 1 when the left arm points."],
                 ["x", "y", "z", "size", "yaw", "tx", "ty", "tz", "mirrored", "right", "high", "w0", "w1", "w2",
                  "w3"], rows)

    rows = []
    for i in range(2000):
        head = (rng.uniform(-3000.0, 3000.0), rng.uniform(0.0, 300.0), rng.uniform(-3000.0, 3000.0))
        yaw = rng.choice([rng.uniform(-math.pi, math.pi), rng.uniform(-7.0, 7.0), 0.0])
        reach = rng.choice([rng.uniform(0.0, 200.0), rng.uniform(0.0, 0.002), rng.uniform(0.0, 2000.0)])
        angle = rng.uniform(-math.pi, math.pi)
        target = (head[0] + reach * math.sin(angle), rng.uniform(0.0, 300.0), head[2] - reach * math.cos(angle))
        if i % 50 == 0:
            target = head  # no distance
        h = tuple(f2u(v) for v in head)
        t = tuple(f2u(v) for v in target)
        rows.append((*h, f2u(yaw), *t, m.point_turn(h, f2u(yaw), t)))
    write_pinned(out / "creature_point_turn.txt", "The turn before a creature's point",
                 ["The game's angle from the heading (yaw) of the creature to the point, as seen from the head, made",
                  "by the game's own instructions of that part of the start of a point. A size of 0.4712 or more",
                  "turns the creature first."],
                 ["hx", "hy", "hz", "yaw", "tx", "ty", "tz", "angle"], rows)


def vec(rng: random.Random, low: float, high: float) -> tuple:
    return tuple(f2u(rng.uniform(low, high)) for _ in range(3))


def hurl_cases(m: Maths, rng: random.Random, out: Path) -> None:
    rows = []
    for i in range(1500):
        at = vec(rng, -3.0, 3.0)
        before = tuple(f2u(u2f(v) + rng.choice([rng.uniform(-0.5, 0.5), 0.0])) for v in at)
        if i % 100 == 0:
            before = at  # the hand does not move
        high = rng.random() < 0.5
        rows.append((*at, *before, int(high), m.hurl_hand_speed(at, before, high)))
    write_pinned(out / "creature_hurl_hand_speed.txt", "The speed of the hand in a creature's throw",
                 ["The game's speed of the hand of the flat (high 0) or the high (high 1) throw, from the middle of the",
                  "finger ends at the throw time (a) and 100 ms before (b), made by the game's own instructions of",
                  "that part of the measure of the throw hands."],
                 ["ax", "ay", "az", "bx", "by", "bz", "high", "speed"], rows)

    def hands() -> tuple:
        flat = vec(rng, -2.0, 2.0)
        high = vec(rng, -2.0, 2.0)
        speeds = (f2u(rng.uniform(0.5, 30.0)), f2u(rng.uniform(0.5, 30.0)))
        return flat, high, speeds

    rows = []
    for i in range(1500):
        flat, high, speeds = hands()
        m.set_hands(flat, high, *speeds)
        place = vec(rng, -3000.0, 3000.0)
        scale = f2u(rng.choice([rng.uniform(0.1, 4.0), 1.0]))
        mirrored = rng.random() < 0.5
        heading = f2u(rng.uniform(-math.pi, math.pi))
        slope = f2u(rng.choice([0.5, rng.uniform(-1.5, 1.5)]))
        rows.append((*flat, *high, *speeds, *place, scale, int(mirrored), heading, slope,
                     *m.hurl_release(place, scale, mirrored, heading, slope)))
    write_pinned(out / "creature_hurl_release.txt", "Where a creature's hand lets go in a throw",
                 ["The game's point where the hand lets go: the flat and high hands at the throw time and their",
                  "speeds, the place, scale and side of the creature, the heading and the slope."],
                 ["fx", "fy", "fz", "hx", "hy", "hz", "fs", "hs", "x", "y", "z", "scale", "mirrored", "heading",
                  "slope", "rx", "ry", "rz"], rows)

    rows = []
    for i in range(2000):
        flat, high, speeds = hands()
        m.set_hands(flat, high, *speeds)
        place = vec(rng, -3000.0, 3000.0)
        scale = f2u(rng.choice([rng.uniform(0.1, 4.0), 1.0]))
        mirrored = rng.random() < 0.5
        reach = rng.uniform(5.0, 300.0)
        angle = rng.uniform(-math.pi, math.pi)
        target = (f2u(u2f(place[0]) + reach * math.sin(angle)), f2u(u2f(place[1]) + rng.uniform(-100.0, 100.0)),
                  f2u(u2f(place[2]) - reach * math.cos(angle)))
        seconds = f2u(rng.choice([math.sqrt(reach * 0.10193679481744766), rng.uniform(0.05, 8.0)]))
        rows.append((*flat, *high, *speeds, *place, scale, int(mirrored), *target, seconds,
                     *m.throw_start(place, scale, mirrored, target, seconds)))
    write_pinned(out / "creature_hurl_lift.txt", "The lift of a creature's throw",
                 ["The game's speed along the ground and height speed (kept from minus the speed to the speed) of a",
                  "throw at its start: the hands as in creature_hurl_release.txt, the place, scale and side of the",
                  "creature, the target and the flight time."],
                 ["fx", "fy", "fz", "hx", "hy", "hz", "fs", "hs", "x", "y", "z", "scale", "mirrored", "tx", "ty",
                  "tz", "seconds", "speed", "rise"], rows)

    rows = []
    for i in range(2000):
        flat, high, speeds = hands()
        m.set_hands(flat, high, *speeds)
        speed = rng.uniform(0.5, 80.0)
        lift = (f2u(speed), f2u(rng.choice([rng.uniform(-speed, speed), speed, -speed])))
        scale = f2u(rng.choice([rng.uniform(0.1, 4.0), 1.0]))
        turn_ms = rng.choice([100, 50, 0, rng.randrange(0, 300)])
        clip_step = rng.choice([turn_ms, rng.randrange(0, 300)])
        throw_ms = rng.randrange(0, 3000)
        time = rng.choice([rng.randrange(0, 3000), throw_ms + rng.randrange(-450, 450), throw_ms])
        rows.append((*speeds, *lift, scale, turn_ms, clip_step, time, throw_ms,
                     *m.hurl_step(lift, scale, turn_ms, clip_step, time, throw_ms)))
    write_pinned(out / "creature_hurl_step.txt", "The weights and the step of a creature's throw",
                 ["The game's weights of the flat (w91) and high (w92) throw clips and the milliseconds that they",
                  "move on in a turn, from the hand speeds, the lift (creature_hurl_lift.txt), the scale, the turn's",
                  "milliseconds, the clip step, the clip time and the throw time. Made by the game's own",
                  "instructions of that part of the throw handler."],
                 ["fs", "hs", "speed", "rise", "scale", "turn_ms", "clip_step", "time", "throw_ms", "w91", "w92",
                  "step"], rows)


def look_cases(m: Maths, rng: random.Random, out: Path) -> None:
    def size() -> int:
        return f2u(rng.choice([rng.uniform(0.1, 4.0), 1.0, 0.59999985, 2.0, rng.uniform(0.0, 0.1)]))

    rows = []
    for i in range(2000):
        s = size()
        weight = f2u(rng.choice([rng.uniform(0.0, 1.0), 1.0, 0.5]))
        interest = f2u(rng.choice([0.9, 0.85, 0.7, 0.5, 0.4, 0.3, 0.25, 0.0]))
        distance = f2u(rng.choice([rng.uniform(0.0, 200.0), rng.uniform(0.0, 2000.0), 0.0]))
        ms = rng.choice([100, 100, 50, 33, 250, 1000])
        turns = rng.choice([rng.randrange(0, 400), 0, 199, 200, 201, rng.randrange(0, 100000)])
        rows.append((s, weight, interest, distance, turns, ms, m.look_catch(s, weight, interest, distance, turns, ms)))
    write_pinned(out / "creature_look_catch.txt", "How much a sight catches a creature's eye",
                 ["The game's score of a thing that the eye sweep meets: the shown size of the creature (the look",
                  "range comes from it), the weight for things, the interest of the thing, its distance, the turns",
                  "of watching and the milliseconds of a turn. Made by the game's own instructions of that part of",
                  "the look interest test."],
                 ["size", "weight", "interest", "distance", "turns", "ms", "score"], rows)

    rows = []
    for i in range(2000):
        s = size()
        kind = rng.randrange(3)
        weight = f2u(rng.choice([rng.uniform(0.0, 1.0), 1.0]))
        interest = f2u(rng.choice([0.9, 0.85, 0.7, 0.5, 0.4, 0.3, 0.25, 0.0, rng.uniform(0.0, 1.0)]))
        distance = f2u(rng.choice([rng.uniform(-50.0, 300.0), rng.uniform(0.0, 3000.0), 0.0, -0.0]))
        rows.append((s, kind, weight, interest, distance, m.look_hold(s, kind, weight, interest, distance)))
    rows.append((f2u(1.0), 0, f2u(1.0), f2u(0.5), 0x7FC00000, m.look_hold(f2u(1.0), 0, f2u(1.0), f2u(0.5), 0x7FC00000)))
    write_pinned(out / "creature_look_hold.txt", "How much a watched sight holds a creature's eye",
                 ["The game's hold of the sight that the creature watches: the shown size of the creature (the look",
                  "range comes from it), the kind of sight (0 thing, 1 mountains, 2 water) and the creature's weight",
                  "for it, the kept interest and the distance to the sight. A distance that is not a number holds as",
                  "0."],
                 ["size", "kind", "weight", "interest", "distance", "hold"], rows)


def toss_cases(m: Maths, rng: random.Random, out: Path) -> None:
    rows = []
    for i in range(2000):
        yaw = f2u(rng.choice([rng.uniform(-math.pi, math.pi), rng.uniform(-7.0, 7.0), 0.0, math.pi / 2, -math.pi]))
        mirrored = rng.randrange(2)
        velocity = [f2u(rng.choice([rng.uniform(-60.0, 60.0), rng.uniform(-1.0, 1.0), 0.0])) for _ in range(3)]
        scale = f2u(rng.choice([rng.uniform(0.05, 4.0), 1.0, 0.25]))
        rows.append((yaw, mirrored, *velocity, scale, *m.toss(yaw, mirrored, velocity, scale)))
    write_pinned(out / "creature_toss_velocity.txt", "The velocity of a thing that a creature lets go of",
                 ["The game's velocity of a thing that the creature drops or tosses: the creature's heading, whether",
                  "it uses the other hand, the speed of the hand in the model, and the drawn scale. Made by the game's",
                  "own instructions of that part of the release of a held thing."],
                 ["yaw", "mirrored", "vx", "vy", "vz", "scale", "x", "y", "z"], rows)


def size_cases(m: Maths, rng: random.Random, out: Path) -> None:
    rows = []
    rests = [1.0, 63.423, 79.154, 30.5, 120.25]
    special = [0.05, 0.049999997, 0.050000004, 4.0, 3.9999998, 4.0000005, 0.0, -0.0, -1.0, float("nan"), 1.0, 2.0]
    for i, size in enumerate(floats(rng, 0.0, 5.0, special, 1500)):
        rest = f2u(rng.choice(rests + [rng.uniform(1.0, 200.0)]))
        rows.append((size, rest, *m.size_and_scale(size, rest)))
    write_pinned(out / "creature_size_scale.txt", "The kept size and the drawn scale of a creature",
                 ["The game's size of the 3D creature (the size given, kept from 0.05 to 4) and its drawn scale",
                  "(size x 15 / rest height), from the size given and the rest height of its model. Made by the",
                  "game's own instructions of that part of the size setter."],
                 ["size", "rest", "kept", "scale"], rows)


def hair_cases(m: Maths, rng: random.Random, out: Path) -> None:
    rows = []
    sizes = [0.0, -0.0, -1.0, 2.0, 1.9999999, 2.0000002, 1.0, 0.05, 4.0, float("nan")]
    for size in floats(rng, 0.0, 4.5, sizes, 2000):
        rows.append((size, *m.hair_scale(size)))
    write_pinned(out / "creature_hair_scale.txt", "The scale of a creature's hair",
                 ["The game's head factor of a young creature (1.8 - 0.5 x the size kept from 0 to 2) and the scale",
                  "of its hair (head factor x size x 15), from the shown size. The hair scale is made by the game's",
                  "own instructions of that part of the frame's draw preparation."],
                 ["size", "head", "scale"], rows)

    rows = []
    for ms in [0, 1, 2, 3, 7, 10, 16, 17, 33, 40, 100, 999, 1000, 1001, 5000] + [rng.randrange(0, 1200) for _ in range(985)]:
        rows.append((ms, m.hair_seconds(ms)))
    write_pinned(out / "creature_hair_seconds.txt", "The seconds of a creature's hair step",
                 ["The game's seconds of the hair step from the milliseconds of the frame (times 0.001, from 0 to 1),",
                  "made by the game's own instructions of that part of the frame's draw preparation."],
                 ["ms", "seconds"], rows)


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261111)
    with Oracle() as o:
        m = Maths(o)
        point_cases(m, rng, out)
        hurl_cases(m, rng, out)
        look_cases(m, rng, out)
        hair_cases(m, rng, out)
        size_cases(m, rng, out)
        toss_cases(m, rng, out)
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
