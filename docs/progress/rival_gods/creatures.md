# The computer gods' creatures

Each computer god has a creature, loaded from a mind file the game ships, set up by script and then steered by its god:
fed, healed, taught, played with and sent to win, destroy or defend towns and to fight other creatures.

**Progress: 2/47 done, 4 partial — 9%**

See [../creature/](../creature/) for how any creature thinks and acts, especially
[../creature/decision_making.md](../creature/decision_making.md), [../creature/fighting.md](../creature/fighting.md) and
[../creature/saves_and_files.md](../creature/saves_and_files.md).

## The mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's, Lethys's and Nemesis's creatures come from three mind files that are byte for byte the same (an older file version), so they start alike; the script picks the species | partial | the files can be read (`components/creaturemind`); the story's loading function is a stub |
| Skirmish gods' creatures come from a generic computer creature file in a newer version | done | `CREATE_CREATURE_FROM_FILE` loads it (`FeatureScriptCommands::CreateCreatureFromFile`) |
| One skirmish land uses Nemesis's file for all its gods | done | same |
| Four larger template minds (destroying other creatures, destroying towns, impressing towns, protecting towns) ship with the game but no script loads them, and the game refuses their old layout | n/a | unused; openblack's reader rejects them too (`MindFile.h`) |
| Exported creatures the player saved also sit beside them; they are the player's, not a god's | n/a | see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| A creature loaded for a god belongs to that god and is placed where the script says | partial | the land command does this (`CreatureArchetype`), turned a fixed 180 degrees and at the species' starting size (unconfirmed what the game uses) |

## How the story sets them up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's is a tortoise at 1.2 times size, Lethys's a wolf at 1.5, Nemesis's a lion at 2 | todo | `LOAD_CREATURE` is a stub |
| Each is named from the game's text | todo | `SET_CREATURE_NAME` is a stub |
| Each is made fully grown and scaled automatically by the factor given | todo | |
| Each knows every everyday skill: building, using fields, totems and storehouses, fishing and dancing | todo | `CREATURE_SET_KNOWS_ACTION` is a stub |
| Each knows fireball and lightning with both power-ups, heal and its power-up, teleport, forest, food and its power-up, both storms, tornado, both shields, wood, water and its power-up, both flocks, and all sixteen creature miracles | todo | |
| The explosion miracle was left out of their lessons | n/a | commented out in the game's scripts |
| Strength, warmth, fatness, energy, itchiness, poo, exhaustion, thirst and fighting health are all set to 0.2 | todo | |
| They are released into the world, then again after their alignment is set | todo | |
| Khazar's takes the player's creature's alignment and is made its friend; the others take the opposite | todo | `CREATURE_FORCE_FRIENDS` is a stub |
| The story waits for the player's creature, checking every 5 seconds, before setting alignment | todo | |
| A creature that wins a fight is healed by 1% every 6 seconds until full or until it fights again | todo | |

## Knowledge and size for skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god's creature learns every miracle its god has, and heal, food, wood and water always, as if seen enough to learn | todo | see [skirmish_opponents.md](skirmish_opponents.md) |
| Its size is set from the person's creature: 0.9 to 1.2 times as tall | todo | always the creature of the person at this machine, not the one the land script names; see [skirmish_opponents.md](skirmish_opponents.md) |
| Its height for decisions ignores the shrinking near its temple | todo | |
| A computer god's creature keeps the speed it was given when it stops, where a person's goes back to walking | todo | see [../creature/locomotion.md](../creature/locomotion.md) |

## Looking after it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The god finds its creature and judges how hungry, starving, thirsty, tired, hot, cold, hurt or ill it is | todo | see [ai.md](ai.md) |
| It feeds it from fields or fish farms, or casts food for it | todo | |
| It makes it drink, sleep, go home, or move somewhere warmer or cooler (all off unless a script turns them on) | todo | |
| It heals it when hurt, and restores it from illness | todo | |
| It judges how near and free its creature is before using it | todo | |

## Teaching it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It judges how important it is to teach an ordinary skill, aggressive, compassionate or impressive magic | todo | |
| It only teaches miracles its creature doesn't know, choosing among those marked known from the start | partial | the filter is read (see [../creature/learning_by_observation.md](../creature/learning_by_observation.md)); no god uses it |
| To teach a skill it leads the creature on the leash and does the action in front of it | todo | |
| To teach a miracle it casts it in front of the creature | todo | |
| It teaches its creature to use the town's totem | todo | |
| It weighs the creature's own desire to learn a miracle | todo | |
| It leashes its creature to things or to other creatures | todo | see [../creature/leash.md](../creature/leash.md) |

## Playing with it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It throws food at it to play | todo | |
| It strokes or slaps it (both off by default) | todo | see [../creature/learning_from_feedback.md](../creature/learning_from_feedback.md) |
| It casts on it for a laugh (off by default) | todo | |

## Using it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It sends it to impress a town it wants to win (with a 3300 setting on that action, unconfirmed what it measures) | todo | |
| It sends it to destroy a town | todo | |
| It sends it to defend a town, or to help a town or a building site | todo | |
| It sends it to attack another creature, if it is near enough | todo | |
| It uses it as a diversion against a player who attacks it | todo | |
| Scripts force a god's creature to help a town (Nemesis's battle plan does) | todo | |
| A computer god's creature fights with the computer's moves | partial | see [../creature/fighting.md](../creature/fighting.md) |

## Story scenes with them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lethys's creature torments the captive player's creature on the third land | todo | see [lethys.md](lethys.md) |
| Lethys's creature is made strong and carries Khazar's off on the second land | todo | see [khazar.md](khazar.md) |
| Nemesis's lion is swapped for a double of the player's species for the final fight | todo | see [nemesis.md](nemesis.md) |
| A mirror creature of the player's species, opposite in alignment, angry and fully grown, is made from the player's creature for a fight (unconfirmed whether the game runs it) | todo | `CREATE_CREATURE_FROM_CREATURE`; see [../story/land_5.md](../story/land_5.md) |
| While a script kills Khazar, both gods' creatures have all their desires turned off and are kept at full health | todo | |
| Scripts can turn every desire off and set one desire on, as for the mirror creature's anger at 0.5 | todo | desire script functions are stubs |
| Freed creatures on the third land, the player's and Lethys's, can both be leashed | todo | |
