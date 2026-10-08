# The Nomads (Food for Thought)

A cut Land 2 challenge: when an Indian village's storage pit runs low on food, ten wandering homeless men arrive, and
the player decides whether and how to feed them: food from the village's own store, food conjured by a miracle, poisoned
food, or nothing. It was never compiled into the game; its title "The Nomads" exists in the text table but none of its
lines do.

**Land:** 2 (written for its Indian village; never started) · **Giver:** an Indian farmer from the village · **Script:** FoodForThought · **Reward:** a large food reward from the sky, for feeding them with miracle food · **Repeatable:** no

**Progress: 0/0 done, 0 partial — 0%**

Sources: the script's source text, Land 2's map script and the game's text table. Every row is n/a: the game never runs
this quest.

## Is it in the game?

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Not in the compiled challenge file; nothing starts it but a single-quest test launcher | n/a | never started by the game |
| Its villager's hut and storage pit match real buildings in Land 2's neutral Indian village (an Indian house and its storage pit) | n/a | never started by the game: checked against `Land2.txt` |
| Its title "The Nomads" is in the text table and used by no other script; its reminder and all 21 of its lines are missing | n/a | never started by the game |
| Not reachable at all in the shipped game | n/a | never started by the game |
| A leftover header note describes a different quest (raiding marauders: three raids, retreat after two deaths), copied from the cut "The Attackers" | n/a | never started by the game: see [the_attackers.md](./the_attackers.md) |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 5 seconds the village storage pit's food is checked; the quest begins once it is 500 or less | n/a | never started by the game |
| A silver scroll appears over a villager's hut; the good advisor nags "We've got something to do. Let's see what it is." until it is clicked | n/a | never started by the game |
| A flock of ten homeless men (Norse farmers) gathers at the edge of the village, and an Indian farmer comes out of the hut | n/a | never started by the game |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut scene: the camera glides to the homeless (4 seconds); the challenge record opens; two lines | n/a | never started by the game: lines missing |
| When the villager reaches his spot, the camera moves to him: one line; then to the storage pit: one line | n/a | never started by the game |
| The evil advisor and then the good advisor come out for a line each; the camera returns | n/a | never started by the game |
| A crying child was planned at the end of the scene | n/a | never started by the game: only a note |

## What the player does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest watches what the hand holds: miracle food, ordinary food taken within 60 of the storage pit (the village's own), ordinary food from anywhere else, or poisoned food | n/a | never started by the game |
| Putting food down within 30 of the homeless gets one line of thanks, once | n/a | never started by the game |
| Food dropped within 20 of them counts as given; food dropped elsewhere gets one disappointed line, once | n/a | never started by the game |
| The quest ends when a food store appears within 20 of the homeless, when fewer than 3 of them remain, or after 150 checks of 2 seconds (about 5 minutes) | n/a | never started by the game |

## The endings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fed from the village's own store: the villager speaks two lines and the good advisor one; alignment +0.5 | n/a | never started by the game |
| Fed with miracle food (or food brought from outside the village): the villager and the homeless speak, both advisors comment; a large food reward falls from the sky by the homeless with its help text; alignment +1 | n/a | never started by the game |
| Fed with poisoned food: "You gave the wandering tribe poisoned food." the food is removed and every homeless man drops dead; the evil advisor: "Heheh. Classic move, Boss. That'll keep them quiet." | n/a | never started by the game: lines quoted from the script's notes, missing from the text table; this ending sets no alignment, so the record closes at 0 |
| Fewer than 3 left (killed or scattered): the villager speaks and the evil advisor comments; alignment -1 | n/a | never started by the game |
| Time ran out: the homeless walk off at no health and are let go; for about the next 150 seconds, if the camera comes within 30 of the spot, the evil advisor comments once; alignment 0 | n/a | never started by the game |
| Every ending closes the challenge record as complete with that alignment | n/a | never started by the game |
