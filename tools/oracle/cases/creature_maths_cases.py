#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of small creature formulas, in test/oracle/data:

- creature_action_priority.txt: the priority of an action from the creature's opinion of it (0 when the action is
  not allowed in the area that confines the creature).
- creature_novelty.txt: how much an action gains from the turns since the creature last did it.
- creature_target_speed.txt: the speed that a route asks for, from the share of the run speed.
- creature_alignment_damp.txt: an alignment change made smaller or larger by the alignment.
- creature_blow_damage.txt: the damage of one blow and its height band.
- creature_distance_priority.txt: the priority of a thing by its distance.
- creature_reaction_worth.txt: the worth of a desire about a thing: the desire, the usefulness and the nearness.
- creature_feedback_attitude.txt: the attitude to the player and the running average after a feedback.

The first four, the worth and the feedback run whole functions of the game. The blow damage and the distance priority are parts of larger
functions: the script runs only the game's own instructions of that part (a slice). A small piece of code in the
image puts the inputs where the part reads them (registers, the stack, the FPU), jumps to the first
instruction of the part, and comes back from a jump that the script writes over the instruction after the part.

Usage: python tools/oracle/cases/creature_maths_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
ACTION_PRIORITY = 0x4C3990  # thiscall on the creature (action, unused), RET 8, ST0
CONFINED = 0x47CCE0  # thiscall on the creature: 1 when an area confines it
NOVELTY = 0x4F0CA0  # thiscall on the creature (action), RET 4, ST0
REQUIRED_SPEED = 0x47FA20  # thiscall on the 3D creature (fraction), RET 4
DAMP = 0x414660  # thiscall on the alignment (change), RET 4, ST0
BLOW_FIRST = 0x48B05E  # the damage part of the function where a blow lands
BLOW_BLOCK_TEST = 0x48B133  # CALL to the block test (5 bytes)
BLOW_AFTER = 0x48B14A  # the first instruction after the damage
DISTANCE_FIRST = 0x4F1FC1  # the part of the distance priority for a thing that the creature does not hold
DISTANCE_AFTER = 0x4F1FDE  # ADD ESP,0x14; RET 8 (6 bytes)
WORTH = 0x4C45C0  # thiscall on the mind (desire, thing), RET 8, ST0
WORTH_BELIEF = 0x4D7BD0  # thiscall, two arguments, RET 8: the belief about the thing
WORTH_DISTANCE = 0x4F1EF0  # thiscall, two arguments, RET 8, ST0: the distance priority
WORTH_USEFULNESS = 0x4E9900  # thiscall, two arguments, RET 8, ST0: the usefulness of the thing for the desire
FEEDBACK = 0x4E1080  # thiscall on the mind (feedback), RET 4
FEEDBACK_STAMP = 0x4C81D0  # thiscall, one argument, RET 4
FEEDBACK_DESIRE = 0x4C8000  # thiscall, two arguments, RET 8
AVERAGE_FEEDBACK = 0xC64140  # the running average of the feedback of every creature

MIND = 0x164
INFO = 0x28
ACTION_INFO = 0xC6C538  # the field of each action that keeps the full novelty, rows of 0x110 bytes
ACTION_ROW = 0x110
NOVELTY_TURNS = 36000


class Slice:
    """Runs a part of a game function with inputs on the stack, in registers and on the FPU."""

    FRAME = 0x100
    count = 0
    REGISTERS = {"eax": 0xB8, "ecx": 0xB9, "edx": 0xBA, "ebx": 0xBB, "ebp": 0xBD, "esi": 0xBE, "edi": 0xBF}

    def __init__(self, oracle: Oracle, first: int, after: int):
        o = self.o = oracle
        self.first = first
        # The oracle calls only addresses in the image: the code goes in the last bytes of the image, which the
        # functions here never read
        self.code = o.base + o.image_size - 0x200 * (1 + Slice.count)
        Slice.count += 1
        self.values = o.alloc(0x40)
        self.out = o.alloc(0x40)
        self.after = after
        self.original = o.read(after, 6)

    def run(self, registers: dict[str, int], stack: dict[int, int], fpu: list[int], read: list[int]) -> tuple:
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
        for i, offset in enumerate(read):
            code += b"\x8b\x84\x24" + struct.pack("<i", offset)  # mov eax, [esp + offset]
            code += b"\xa3" + struct.pack("<I", self.out + 4 * i)  # mov [out + 4i], eax
        code += b"\x81\xc4" + struct.pack("<I", self.FRAME)  # add esp, FRAME
        code += b"\x5f\x5e\x5b\x5d\xc3"  # pop edi, esi, ebx, ebp; ret
        o.write(self.code, bytes(code))
        # push back; ret: the part ends with the stack as it was
        o.write(self.after, b"\x68" + struct.pack("<I", back) + b"\xc3")
        result = o.call(self.code)
        outputs = o.read_u32(self.out, len(read)) if read else []
        return result, outputs


class Maths:
    def __init__(self, oracle: Oracle):
        o = self.o = oracle
        o.stub(CONFINED, eax=0, pop=0)
        o.stub(FEEDBACK_STAMP, eax=0, pop=4)
        o.stub(FEEDBACK_DESIRE, eax=0, pop=8)
        self.creature = o.alloc(0x400)
        self.mind = o.alloc(0x22000)
        self.info = o.alloc(0x400)
        self.body = o.alloc(0x5300)
        self.route = o.alloc(0x64100)
        self.alignment = o.alloc(0x40)
        self.partner = o.alloc(0x5300)
        o.write_u32(self.creature + MIND, self.mind)
        o.write_u32(self.creature + INFO, self.info)
        o.write_u32(self.info + 0x2A8, NOVELTY_TURNS)
        o.write_u32(self.body + 0x5198, self.route)
        o.write_u32(self.body + 0x528C, self.partner)
        self.blow = Slice(o, BLOW_FIRST, BLOW_AFTER)
        self.distance = Slice(o, DISTANCE_FIRST, DISTANCE_AFTER)

    def action_priority(self, opinion: int, disabled: bool) -> int:
        o = self.o
        action = 7
        o.stub(CONFINED, eax=1 if disabled else 0, pop=0)
        o.write_u32(self.mind + 0x1C9C + 4 * action, 1)
        o.write_u32(self.mind + 0x25B8 + 4 * action, opinion)
        return o.call(ACTION_PRIORITY, this=self.creature, args=[action, 0]).st0f

    def novelty(self, turns: int, first: bool) -> int:
        o = self.o
        action = 11
        o.write_u32(self.mind + 0x1CEA0 + 4 * action, turns)
        o.write_u32(ACTION_INFO + ACTION_ROW * action, 0 if first else 1)
        return o.call(NOVELTY, this=self.creature, args=[action]).st0f

    def target_speed(self, fraction: int, run: int) -> int:
        o = self.o
        o.write_u32(self.body + 0x4848, run)
        o.call(REQUIRED_SPEED, this=self.body, args=[fraction])
        return o.read_u32(self.route + 0x6403C)[0]

    def damp(self, change: int, alignment: int) -> int:
        o = self.o
        o.write_u32(self.alignment + 0x8, alignment)
        return o.call(DAMP, this=self.alignment, args=[change]).st0f

    def blow_damage(self, own: tuple[int, int, int], other: tuple[int, int], special: bool, share: int,
                    blocked: bool) -> tuple[int, int]:
        o = self.o
        strength, size, speed = own
        o.write_u32(self.body + 0xAC, strength)
        o.write_u32(self.body + 0x90, size)
        o.write_u32(self.body + 0x49A4, speed)
        o.write_u32(self.body + 0x49A0, 0x82 if special else 0x7D)
        o.write_u32(self.partner + 0xAC, other[0])
        o.write_u32(self.partner + 0x90, other[1])
        # mov al, blocked; three nops: the block test gives its answer with no call
        o.write(BLOW_BLOCK_TEST, bytes([0xB0, 1 if blocked else 0, 0x90, 0x90, 0x90]))
        result, (band, damage) = self.blow.run({"esi": self.body}, {0x28: share}, [], [0x20, 0x30])
        if result.depth != 0:
            raise RuntimeError("the blow part left values on the FPU")
        return damage, band

    def feedback(self, attitude: int, average: int, feedback: int) -> tuple[int, int]:
        o = self.o
        o.write_u32(self.mind + 0x18C58, attitude)
        o.write_u32(AVERAGE_FEEDBACK, average)
        o.call(FEEDBACK, this=self.mind, args=[feedback])
        return o.read_u32(self.mind + 0x18C58)[0], o.read_u32(AVERAGE_FEEDBACK)[0]

    def worth(self, desire: int, value: int, usefulness: int, nearness: int) -> int:
        o = self.o
        o.stub(WORTH_BELIEF, eax=self.alignment, pop=8)
        o.stub(WORTH_DISTANCE, eax=0, pop=8, st0=f32.of_bits(nearness))
        o.stub(WORTH_USEFULNESS, eax=0, pop=8, st0=f32.of_bits(usefulness))
        o.write_u32(self.mind + 0x150 + 4 * desire, value)
        return o.call(WORTH, this=self.mind, args=[desire, self.partner]).st0f

    def distance_priority(self, distance: int, weight: int) -> int:
        result, _ = self.distance.run({}, {0x0: f2u(200.0), 0x1C: distance}, [weight], [])
        if result.depth != 1:
            raise RuntimeError("the distance part did not leave one value on the FPU")
        return result.st0f


def floats(rng: random.Random, low: float, high: float, edges: list[float], count: int) -> list[int]:
    values = [f2u(e) for e in edges]
    values += [f2u(rng.uniform(low, high)) for _ in range(count - len(values))]
    return values


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261111)
    with Oracle() as o:
        m = Maths(o)

        rows = []
        for opinion in floats(rng, -1.5, 1.5, [-1.0, 0.0, 1.0, -0.5, 0.5, 1e-7, -1e-7], 1500):
            disabled = rng.random() < 0.1
            rows.append((opinion, int(disabled), m.action_priority(opinion, disabled)))
        write_pinned(out / "creature_action_priority.txt", "The priority of a creature's action",
                     ["The game's priority of one action from the opinion of the creature about it. disabled is 1",
                      "when an area confines the creature and the action is not allowed there."],
                     ["opinion", "disabled", "priority"], rows)

        rows = []
        turns = [0, 1, 2, 3, 89, 90, 91, 100, 3599, 3600, 3601, 35999, 36000, 36001, 100000]
        turns += [rng.randrange(0, 40000) for _ in range(1500)]
        for t in turns:
            first = rng.random() < 0.5
            rows.append((t, int(first), m.novelty(t, first)))
        write_pinned(out / "creature_novelty.txt", "The novelty of a creature's action",
                     ["The game's novelty of one action from the turns since the creature last did it (at most",
                      "36000). first is 1 when the action keeps only 0.025 of the novelty."],
                     ["turns", "first", "novelty"], rows)

        rows = []
        for fraction in floats(rng, -0.5, 1.5, [0.0, 1.0, 0.5, -0.0, 1.0000001, 0.9999999], 1000):
            run = f2u(rng.choice([rng.uniform(1.0, 80.0), 20.0 * rng.choice([0.05, 1.0, 2.5, 4.0])]))
            rows.append((fraction, run, m.target_speed(fraction, run)))
        # A share that is not a number asks for no speed
        rows.append((0x7FC00000, f2u(20.0), m.target_speed(0x7FC00000, f2u(20.0))))
        write_pinned(out / "creature_target_speed.txt", "The speed that a creature's route asks for",
                     ["The game's route speed from the share of the run speed that the creature asks for and its",
                      "run speed."],
                     ["fraction", "run", "speed"], rows)

        rows = []
        for change in floats(rng, -0.01, 0.01, [0.0, -0.0, 0.001, -0.001], 1500):
            alignment = f2u(rng.choice([rng.uniform(-1.0, 1.0), 0.0, -0.0, 1.0, -1.0]))
            rows.append((change, alignment, m.damp(change, alignment)))
        write_pinned(out / "creature_alignment_damp.txt", "An alignment change damped by the alignment",
                     ["The game's alignment change after the alignment makes it smaller (the same direction) or",
                      "larger (the other direction)."],
                     ["change", "alignment", "damped"], rows)

        rows = []
        for i in range(3000):
            own = (f2u(rng.uniform(-1.0, 1.0)), f2u(rng.uniform(0.05, 4.0)), f2u(rng.uniform(0.5, 1.5)))
            other = (f2u(rng.uniform(-1.0, 1.0)), f2u(rng.uniform(0.05, 4.0)))
            if i % 10 == 0:
                own = (own[0], own[1], f2u(rng.choice([0.0, 100.0, 1e-3])))
            special = rng.random() < 0.2
            share = f2u(rng.choice([rng.uniform(-0.2, 1.2), 0.33, 0.66, 0.3300001, 0.6600001]))
            blocked = rng.random() < 0.3
            damage, band = m.blow_damage(own, other, special, share, blocked)
            rows.append((*own, *other, int(special), share, int(blocked), damage, band))
        write_pinned(out / "creature_blow_damage.txt", "The damage of a creature's blow",
                     ["The game's damage of one blow and its band (0 high, 1 middle, 2 low), made by the game's own",
                      "instructions of the damage part of the function where a blow lands. strength is the strength",
                      "that the body shows, size the size, speed the kept speed of the blow; special is the special",
                      "move (two times the factor); share is how high the blow lands; blocked is the block test."],
                     ["strength", "size", "speed", "other_strength", "other_size", "special", "share", "blocked",
                      "damage", "band"], rows)

        rows = []
        for distance in floats(rng, 0.0, 400.0, [0.0, 200.0, 199.99998, 200.00002, 1e-3], 1500):
            weight = f2u(rng.choice([rng.uniform(0.0, 1.0), 0.05, 1.0, 0.0]))
            rows.append((distance, weight, m.distance_priority(distance, weight)))
        write_pinned(out / "creature_distance_priority.txt", "The priority of a thing by its distance",
                     ["The game's priority of a thing that the creature does not hold, from its distance and the",
                      "distance weight of the desire, with the farthest distance 200. Made by the game's own",
                      "instructions of that part of the function."],
                     ["distance", "weight", "priority"], rows)

        rows = []
        for _ in range(1500):
            value = f2u(rng.choice([rng.uniform(0.0, 1.0), 0.0, 1.0]))
            usefulness = f2u(rng.choice([rng.uniform(-1.0, 2.0), 0.0, 1.0, 0.1]))
            nearness = f2u(rng.choice([rng.uniform(-1.0, 1.6), 1.6, 1.0, 0.0]))
            rows.append((value, usefulness, nearness, m.worth(rng.randrange(40), value, usefulness, nearness)))
        # A value that is not a number gives 0, as an infinite nearness times a zero desire does
        for value, usefulness, nearness in [(0x3F000000, 0x7FC00000, 0x3F800000), (0, 0x3F800000, 0x7F800000),
                                            (0x3F800000, 0x7F800000, 0x3F800000)]:
            rows.append((value, usefulness, nearness, m.worth(3, value, usefulness, nearness)))
        write_pinned(out / "creature_reaction_worth.txt", "The worth of a creature's desire about a thing",
                     ["The game's worth of one desire about a thing: the value of the desire, the usefulness of the",
                      "thing for it and the priority of the thing by its distance, kept from 0 to 1."],
                     ["value", "usefulness", "nearness", "worth"], rows)

        rows = []
        for feedback in floats(rng, -1.0, 1.0, [0.0, -0.0, 1.0, -1.0], 1500):
            attitude = f2u(rng.uniform(-1.0, 1.0))
            average = f2u(rng.uniform(-1.0, 1.0))
            rows.append((attitude, average, feedback, *m.feedback(attitude, average, feedback)))
        write_pinned(out / "creature_feedback_attitude.txt", "The attitude to the player after a feedback",
                     ["The game's attitude of the creature to the player and the running average of the feedback",
                      "of every creature, after one feedback (a stroke is above 0, a slap below 0)."],
                     ["attitude", "average", "feedback", "new_attitude", "new_average"], rows)

    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
