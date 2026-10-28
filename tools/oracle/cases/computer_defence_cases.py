#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes the pinned data of the defence of a computer god, in test/oracle/data:

- computer_defend_town.txt: the attack on a town of the god that the god wants most to defend against. The inputs are
  the game turn, the number of the god and, for two towns and eight players, the aggression and the turn of the last
  attack. The results are the bits of the wish, the chosen town (0, 1, or 2 for none) and the chosen attacker.
- computer_belief_attack.txt: how much a town of the god is under an attack on its belief. The inputs are the number of
  the god and the bits of the belief and of the recent belief gain of eight players. The result is the bits of the value.
- computer_temple_hit.txt: how much the god wants to react to a thing that hit its temple. The inputs are the game turn
  and the turn of the last hit. The results are the bits of the value and 1 when the god takes the heart as its object.

The game's functions run as they are. The players, towns, beliefs and the game turn are laid out in memory and read by
the game's own code. The game's player list gives the same player for every number, a player that a human plays, so
every number but the god's is an attacker. The belief function sees no allies.

Usage: python tools/oracle/cases/computer_defence_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, f32, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
DEFEND_TOWN = 0x666AE0  # thiscall (mind; &player, &town), ret 8, ST0
BELIEF_ATTACK = 0x438A40  # thiscall (belief; player), ret 4, ST0
TEMPLE_HIT = 0x664FA0  # thiscall (mind; node), ret 4, ST0
GET_PLAYER = 0x5509B0  # thiscall (game; number), ret 4, EAX
PLAYER_NUMBER = 0x64A790  # thiscall (player), EAX
# The defence of a town does not call this function, so its stub serves as the mind's "get player" slot
MIND_PLAYER = PLAYER_NUMBER
G_GAME = 0xD0195C
GAME_TURN = 0x205A40

# Offsets in a player, a town and a mind
PLAYER_NUMBER_BYTE = 0xB5
PLAYER_TYPE = 0x8E0
PLAYER_CITADEL = 0xA48
PLAYER_TOWNS = 0xA50
TOWN_NEXT = 0x75C
RECORD = 0x80
RECORD_AGGRESSION = 0x9F8
RECORD_TURN = 0x9FC
MIND_PLAYER_FIELD = 0x28
NODE_OBJECT = 0x94
PLAYERS = 8
TOWNS = 2


def aggression(pick: random.Random) -> float:
    return pick.choice([0.0, 0.0, -0.5, 0.1, 0.25, 0.5, 0.75, 1.0, 3.0, pick.uniform(0.0, 1.0), pick.uniform(-1, 2)])


def defend_town(o: Oracle, pick: random.Random) -> list[list[int]]:
    rows = []
    for index in range(1500):
        turn = pick.randrange(0, 40000)
        god = pick.randrange(PLAYERS)
        records = []
        for _ in range(TOWNS * PLAYERS):
            if index < 10:
                # No attack counts
                last = pick.randrange(0, turn + 1)
                value = pick.choice([0.0, -0.25])
            elif index < 40:
                # Ties: the same attack in many records
                last = (turn - 300) & 0xFFFFFFFF
                value = 0.5 if pick.random() < 0.7 else 0.0
            else:
                since = pick.choice([0, 1, 599, 600, 1199, 1200, 1201, 5000, pick.randrange(0, 1300)])
                last = pick.choice([(turn - since) & 0xFFFFFFFF, 0, pick.randrange(0, 0xFFFFFFFF)])
                value = aggression(pick)
            records.append((last, f32(value)))
        o.reset()
        game = o.alloc(GAME_TURN + 0x10)
        o.write_u32(G_GAME, game)
        o.write_u32(game + GAME_TURN, turn)
        player = o.alloc(0xC00)
        o.write(player + PLAYER_NUMBER_BYTE, bytes([god]))
        o.write_u32(player + PLAYER_TYPE, 1)
        o.stub(MIND_PLAYER, eax=player, pop=0)
        o.stub(GET_PLAYER, eax=player, pop=4)
        towns = [o.alloc(0xE00) for _ in range(TOWNS)]
        o.write_u32(player + PLAYER_TOWNS, towns[0])
        for number, town in enumerate(towns):
            o.write_u32(town + TOWN_NEXT, towns[number + 1] if number + 1 < TOWNS else 0)
            for slot in range(PLAYERS):
                last, value = records[number * PLAYERS + slot]
                o.write_u32(town + RECORD_TURN + slot * RECORD, last)
                o.write_f32(town + RECORD_AGGRESSION + slot * RECORD, value)
        vtable = o.alloc(0x40)
        o.write_u32(vtable + 0x1C, MIND_PLAYER)
        mind = o.alloc(0x40)
        o.write_u32(mind, vtable)
        outs = o.alloc(0x10)
        result = o.call(DEFEND_TOWN, this=mind, args=[outs, outs + 4])
        assert result.popped == 8, result
        assert result.depth == 1, result
        chosen_player, chosen_town = o.read_u32(outs, 2)
        town_index = towns.index(chosen_town) if chosen_town in towns else TOWNS
        row = [turn, god]
        for last, value in records:
            row += [last, value.bits]
        rows.append(row + [result.st0f, town_index, 1 if chosen_player == player else 0])
    return rows


def belief(pick: random.Random) -> float:
    return pick.choice([0.0, 0.0, 0.008, 0.0080001, 0.01, 0.5, 1.0, 2.0, -0.5, pick.uniform(0.0, 0.05),
                        pick.uniform(0.0, 3.0)])


def belief_attack(o: Oracle, pick: random.Random) -> list[list[int]]:
    rows = []
    for _ in range(1500):
        god = pick.randrange(PLAYERS)
        beliefs = [f32(belief(pick)) for _ in range(PLAYERS)]
        recents = [f32(belief(pick)) for _ in range(PLAYERS)]
        o.reset()
        game = o.alloc(GAME_TURN + 0x10)
        o.write_u32(G_GAME, game)
        o.stub(PLAYER_NUMBER, eax=god, pop=0)
        # No player is in the list, so no player is an ally of the god
        o.stub(GET_PLAYER, eax=0, pop=4)
        player = o.alloc(0xC00)
        town_belief = o.alloc(0x100)
        o.write_f32(town_belief + 8, *beliefs)
        o.write_f32(town_belief + 0x28, *recents)
        result = o.call(BELIEF_ATTACK, this=town_belief, args=[player])
        assert result.popped == 4, result
        assert result.depth == 1, result
        rows.append([god] + [b.bits for b in beliefs] + [r.bits for r in recents] + [result.st0f])
    return rows


def temple_hit(o: Oracle, pick: random.Random) -> list[list[int]]:
    rows = []
    cases = []
    for since in (0, 1, 19, 20, 21, 22, 100, 299, 300, 301, 599, 600, 601, 1000, 0x7FFFFFFF, 0xFFFFFFFF):
        cases.append((100000, (100000 - since) & 0xFFFFFFFF))
    cases.append((5, 0))
    for _ in range(500):
        turn = pick.randrange(0, 60000)
        cases.append((turn, pick.choice([(turn - pick.randrange(0, 700)) & 0xFFFFFFFF, pick.randrange(1, 60000)])))
    for turn, last in cases:
        o.reset()
        game = o.alloc(GAME_TURN + 0x10)
        o.write_u32(G_GAME, game)
        o.write_u32(game + GAME_TURN, turn)
        player = o.alloc(0xC00)
        citadel = o.alloc(0x100)
        heart = o.alloc(0x100)
        o.write_u32(player + PLAYER_CITADEL, citadel)
        o.write_u32(citadel + 0x30, heart)
        o.write_u32(heart + 0x8C, last)
        mind = o.alloc(0x40)
        o.write_u32(mind + MIND_PLAYER_FIELD, player)
        node = o.alloc(0x100)
        result = o.call(TEMPLE_HIT, this=mind, args=[node])
        assert result.popped == 4, result
        assert result.depth == 1, result
        taken = o.read_u32(node + NODE_OBJECT)[0]
        rows.append([turn, last, result.st0f, 1 if taken == heart else 0])
    return rows


def main(argv: list[str]) -> int:
    out = Path(argv[0]) if argv else ROOT / "test" / "oracle" / "data"
    pick = random.Random(0xDEF1)
    with Oracle() as o:
        defends = defend_town(o, pick)
        attacks = belief_attack(o, pick)
        hits = temple_hit(o, pick)
    columns = ["turn", "god"]
    for town in range(TOWNS):
        for slot in range(PLAYERS):
            columns += [f"last{town}_{slot}", f"aggression{town}_{slot}"]
    columns += ["wish", "town", "attacker"]
    count = write_pinned(
        out / "computer_defend_town.txt",
        "The attack on a town of a computer god that the god wants most to defend against",
        ["turn is the game turn and god the number of the god. lastT_P and aggressionT_P are the turn of the last",
         "attack of player P on town T and the bits of the aggression of the town against P. wish is the bits of the",
         "result; town is the chosen town (2 for none); attacker is 1 when the game names the attacker."],
        columns,
        defends,
    )
    print(f"computer_defend_town: {count} rows")
    columns = ["god"] + [f"belief{n}" for n in range(PLAYERS)] + [f"recent{n}" for n in range(PLAYERS)] + ["value"]
    count = write_pinned(
        out / "computer_belief_attack.txt",
        "How much a town of a computer god is under an attack on its belief",
        ["god is the number of the god; beliefN and recentN are the bits of the belief of the town in player N and of",
         "the recent belief gain of player N. value is the bits of the result. No player is an ally of the god."],
        columns,
        attacks,
    )
    print(f"computer_belief_attack: {count} rows")
    count = write_pinned(
        out / "computer_temple_hit.txt",
        "How much a computer god wants to react to a thing that hit its temple",
        ["turn is the game turn and last the turn of the last hit on the temple. value is the bits of the result;",
         "taken is 1 when the god takes the heart of its temple as the object of the node."],
        ["turn", "last", "value", "taken"],
        hits,
    )
    print(f"computer_temple_hit: {count} rows")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
