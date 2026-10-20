#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the town desire voices, in test/oracle/data:

- desire_voice_score.txt: the score of a voice, from the turns since the last voice, the turns since the first look at
  the thing, the distance, the desire and whether it is the last voice
- desire_voice_pick.txt: the voice of a row for a value and the random amount that comes off it (as a sample)
- worship_site_food_desire.txt: how much a worship site wants food, from the need of its dancers and the food in its pot
- temple_strain_sound.txt: one turn of the strain sound of the temple of the player at this machine

Usage: python tools/oracle/cases/town_desire_voices_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same files (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f2u, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The functions (BW1W120, runblack.exe 1.20)
SCORE = 0x71B410  # thiscall (guidance), args thing, distance, desire, sample; RET 0x10; the score on ST0
PICK = 0x71AA90  # thiscall (guidance), args row, value; RET 8; the help id in EAX
SITE_FOOD_DESIRE = 0x77C310  # thiscall (site), plain RET; the desire on ST0
STRAIN_SOUND = 0x463850  # thiscall (temple), arg the most strain; RET 4; writes temple + 0x70
# Functions these call that the cases replace
TURNS_SINCE_HEARD = 0x71ADF0  # thiscall (guidance, kind), RET 4
TURNS_SINCE_SEEN = 0x71AE10  # thiscall (guidance, thing), RET 4
LOCAL_FLOAT_RAND = 0x6DE590  # cdecl (limit), the random amount on ST0
DANCERS_FOOD_NEED = 0x50BF20  # thiscall (dance), plain RET; the need on ST0
SITE_GET_RESOURCE = 0x77BD80  # thiscall (site, type), RET 4; the food in the pot in EAX
TEMPLE_GET_PLAYER = 0x462A50  # thiscall (temple), plain RET; the player in EAX
# The help id to sample table: records of three words from here, the sample in the third
HELP_SAMPLES = 0x96BA30
# The game object, its player index and its players; the milliseconds of a game turn
GAME = 0xD0195C
PLAYER_INDEX = 0x205A59
PLAYERS = 0x18
PLAYER_SIZE = 0xA60
MS_PER_TURN = 0xD01A38

# Fields
LAST_VOICE = 0x98  # guidance: the last desire voice
SITE_DANCE = 0xA0
STRAIN = 0x70  # temple

SCORE_COLUMNS = ["since_heard", "since_seen", "distance", "desire", "same", "score"]
PICK_COLUMNS = ["row", "value", "draw", "sample"]
SITE_COLUMNS = ["need", "pot", "desire"]
STRAIN_COLUMNS = ["local", "level", "most", "level_after"]


def u(x: float) -> int:
    return f2u(x)


def score_cases(rng: random.Random) -> list[dict]:
    cases = []
    chosen = [(0, 0, 0.0, 0.0, 0), (50, 0, 0.0, 1.0, 0), (49, 299, 199.9, 0.99, 1), (25, 150, 100.0, 0.7, 0),
              (25, 150, 100.0, 0.7, 1), (1000, 601, 250.0, 2.0, 0), (10, 30, 12.5, 0.95, 0), (7, 0, 0.5, 1.0, 1)]
    for since, seen, distance, desire, same in chosen:
        cases.append({"since_heard": since, "since_seen": seen, "distance": u(distance), "desire": u(desire),
                      "same": same})
    for _ in range(400):
        cases.append({"since_heard": rng.choice([rng.randrange(0, 60), rng.randrange(0, 5000)]),
                      "since_seen": rng.choice([rng.randrange(0, 320), rng.randrange(0, 700)]),
                      "distance": u(rng.uniform(0.0, 210.0)),
                      "desire": u(rng.choice([rng.uniform(-0.2, 1.2), rng.random()])),
                      "same": rng.randrange(2)})
    return cases


def pick_cases(rng: random.Random) -> list[dict]:
    cases = []
    for row in range(19):
        for value, draw in [(0.85, 0.0), (0.8500001, 0.0000001), (0.65, 0.0), (0.45, 0.0), (0.4499999, 0.0),
                            (1.0, 0.15), (1.0, 0.1500001), (0.9, 0.25), (0.7, 0.26)]:
            cases.append({"row": row, "value": u(value), "draw": u(draw)})
        for _ in range(20):
            value = rng.uniform(-0.1, 1.2)
            cases.append({"row": row, "value": u(value), "draw": u(rng.uniform(0.0, max(value, 0.0) / 3.0))})
    return cases


def site_cases(rng: random.Random) -> list[dict]:
    cases = [{"need": u(0.0), "pot": 0}, {"need": u(10.0), "pot": 0}, {"need": u(10.0), "pot": 10},
             {"need": u(10.0), "pot": 11}, {"need": u(0.0), "pot": 5}, {"need": u(33.3), "pot": 7}]
    for _ in range(300):
        cases.append({"need": u(rng.choice([rng.uniform(0.0, 60.0), rng.uniform(0.0, 5000.0)])),
                      "pot": rng.choice([rng.randrange(0, 60), rng.randrange(0, 6000)])})
    return cases


def strain_cases(rng: random.Random) -> list[dict]:
    cases = []
    for local in (1, 0):
        for level, most in [(0.0, 0.0), (0.0, 0.05), (0.0, 0.1), (0.0, 0.3), (0.5, 0.45), (0.5, 0.4), (0.5, 0.39),
                            (1.0, 3.0), (2.0, -0.5), (0.3, 0.3)]:
            cases.append({"local": local, "level": u(level), "most": u(most)})
    for _ in range(200):
        cases.append({"local": 1, "level": u(rng.uniform(-0.5, 2.0)), "most": u(rng.uniform(-0.5, 2.0))})
    return cases


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rng = random.Random(20261020)
    with Oracle() as o:
        guidance = o.alloc(0x100)
        thing = o.alloc(0x40)

        rows = []
        for c in score_cases(rng):
            # The last voice is 7 and the voice asked about is 7 or 8
            o.write_u32(guidance + LAST_VOICE, 7)
            o.stub(TURNS_SINCE_HEARD, eax=c["since_heard"], pop=4)
            o.stub(TURNS_SINCE_SEEN, eax=c["since_seen"], pop=4)
            sample = 7 if c["same"] else 8
            r = o.call(SCORE, this=guidance, args=[thing, f32.of_bits(c["distance"]), f32.of_bits(c["desire"]), sample])
            assert r.popped == 0x10 and r.depth == 1, r
            rows.append([c[k] for k in SCORE_COLUMNS[:-1]] + [r.st0f])
        write_pinned(out / "desire_voice_score.txt", "The score of a town desire voice (the game's own function)",
                     ["since_heard, since_seen and same are words; distance, desire and score are float bits."],
                     SCORE_COLUMNS, rows)

        rows = []
        for c in pick_cases(rng):
            o.stub(LOCAL_FLOAT_RAND, st0=f32.of_bits(c["draw"]))
            r = o.call(PICK, this=guidance, args=[c["row"], f32.of_bits(c["value"])])
            assert r.popped == 8 and r.depth == 0, r
            help_id = r.eax
            sample = o.read_u32(HELP_SAMPLES + 8 + help_id * 12)[0] if help_id != 0 else 0
            rows.append([c[k] for k in PICK_COLUMNS[:-1]] + [sample])
        write_pinned(out / "desire_voice_pick.txt", "The voice of a row of town desire voices (the game's own function)",
                     ["row and sample are words (the sample of the guidance bank, 0 for none); value and draw are",
                      "float bits. The draw is the random amount that the game takes off the value."],
                     PICK_COLUMNS, rows)

        site = o.alloc(0x200)
        vtable = o.alloc(0x400)
        dance = o.alloc(0x100)
        o.write_u32(site, vtable)
        o.write_u32(vtable + 0x98, SITE_GET_RESOURCE)
        o.write_u32(site + SITE_DANCE, dance)
        rows = []
        for c in site_cases(rng):
            o.stub(DANCERS_FOOD_NEED, st0=f32.of_bits(c["need"]))
            o.stub(SITE_GET_RESOURCE, eax=c["pot"], pop=4)
            r = o.call(SITE_FOOD_DESIRE, this=site)
            assert r.popped == 0 and r.depth == 1, r
            rows.append([c["need"], c["pot"], r.st0f])
        write_pinned(out / "worship_site_food_desire.txt", "How much a worship site wants food (the game's own function)",
                     ["need and desire are float bits; pot is a word."], SITE_COLUMNS, rows)

        game = o.alloc(0x206000)
        o.write_u32(GAME, game)
        o.write(game + PLAYER_INDEX, bytes([1]))
        o.write_u32(MS_PER_TURN, 100)
        temple = o.alloc(0x100)
        temple_vtable = o.alloc(0x100)
        o.write_u32(temple, temple_vtable)
        o.write_u32(temple_vtable + 0x1C, TEMPLE_GET_PLAYER)
        rows = []
        for c in strain_cases(rng):
            player = game + PLAYERS + (1 if c["local"] else 0) * PLAYER_SIZE
            o.stub(TEMPLE_GET_PLAYER, eax=player)
            o.write_u32(temple + STRAIN, c["level"])
            r = o.call(STRAIN_SOUND, this=temple, args=[f32.of_bits(c["most"])])
            assert r.popped == 4 and r.depth == 0, r
            rows.append([c["local"], c["level"], c["most"], o.read_u32(temple + STRAIN)[0]])
        write_pinned(out / "temple_strain_sound.txt", "One turn of a temple's strain sound (the game's own function)",
                     ["local is a word (1: the temple of the player at this machine); the rest are float bits.",
                      "A game turn is 100 milliseconds."], STRAIN_COLUMNS, rows)
    print("done")


if __name__ == "__main__":
    main()
