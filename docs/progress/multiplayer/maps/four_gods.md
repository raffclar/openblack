# Four Gods

Four gods in the four corners of a mountain-ringed land: you and three computer gods each start with the same small
Celtic town, with neutral Japanese and Indian towns between you and a Tibetan wonder town in the middle. Online the
same land is the four-player map "The four corners of Eden".

**Players:** 4 · **Landscape:** `Data/Landscape/Multi_Player/MPM_4P_1.lnd` · **Script:** `Scripts/Playgrounds/FourGods.txt` · **Mode:** both

**Progress: 9/35 done, 10 partial — 40%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#four-gods-loading) (its "Four Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it under the game's own line "Defeat three gods to control this realm." rather than its file name | partial | the debug menu lists it as "Four Gods" with "Battle four other gods for this realm" as a tooltip (`src/Debug/Gui.cpp`, `src/Level.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it; see the next row |
| No story script runs on a skirmish land | todo | openblack starts the story's control script when a playground is the start land, and leaves story scripts running when one is opened from the debug menu |
| The script gives no start point, so the camera starts zoomed onto your own temple | todo | openblack points the camera at the land's corner (0, 0), off the land |
| The land's title "Four Gods" and its line "Battle four other gods for this realm" are its start message (the line miscounts: there are three other gods) | todo | the start message commands are ignored |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land about 1,900 by 1,750 paces with its highest peaks in the four corners, where the temples stand, and lower ground and water between; about a fifth is at sea level | done | `Game::LoadLandscape` |
| One climate over the whole land: a mild 12 degrees, no rain and no wind | done | `WeatherSystem`; see ../../weather/ |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%), the standard day | done | `DayNightClock` |
| The music follows the nearest town's tribe: Celtic in every corner, Japanese and Indian between, Tibetan in the middle | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project half their normal influence and gods their full influence | done | `InfluenceSystem` |

## The four starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each god has a temple in a corner and a Celtic home town beside it: nine or ten houses, a graveyard, a crèche, a storehouse, a workshop, two fields and about 14 villagers | done | towns, buildings and villagers as in playground_scripts.md |
| Each home store starts with 500 food and 500 wood | done | `AbodeArchetype` keeps the amounts |
| Your workshop alone starts with 12,750 wood, a head start over the other three (online too, for whoever plays player one) | done | `AbodeArchetype` keeps the amounts |
| Each home town believes in its god fully and offers heal, teleport, food, water and creature freeze | partial | belief is set; village centre miracles are not offered (`CREATE_NEW_TOWN_SPELL`); see ../../miracles/ |

## The computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Computer gods are switched on for players two, three and four | partial | the players are made; nothing thinks for them (see ../skirmish.md) |
| Player two has a horse, player three a tortoise and player four a zebra, all from the generic computer creature mind | partial | `CREATE_CREATURE_FROM_FILE` (`CreatureArchetype`) with the mind loaded |
| All three creatures start bunched together on one edge of the land, between your temple and player two's, far from players three and four, whose creatures must cross the land to reach home | partial | made where the script puts them; nothing sends them home |
| The creatures are sized to match your own | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is ignored |
| No personalities are loaded, so all three play with the default personality | todo | the computer god is not written |
| Every town's belief in any computer god is capped at 1, their own home towns included, while yours is not, a handicap in your favour | todo | `SET_TOWN_BELIEF_CAP` logs "not implemented" |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Four Japanese towns, one midway along each edge between two corners, each offering shield and physical shield | partial | towns, houses and villagers are built; miracles are not offered |
| Four Indian fishing towns in an inner ring, each offering lightning, creature strength and creature invisibility | partial | built; miracles are not offered and fish farms are not made |
| The Tibetan town in the middle has a wonder, a graveyard, fifteen houses and about 24 people, and offers lightning, the strongest lightning, beam explosion and the strongest beam explosion | partial | built; its miracles are not offered |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 664 trees (mostly conifers and palms) and nine big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to 21 forests that foresters fell and replant | todo | `CREATE_FOREST` is empty |
| 218 animals in 28 flocks: cows, pigs, sheep, pigeons, gulls, doves and 16 wolves | todo | `CREATE_NEW_ANIMAL` is empty |
| 30 fields and 16 fish farms | partial | fields are made; fish farms are not |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |

## Online as "The four corners of Eden"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "The four corners of Eden - 4 players", described as "Tactical action for 4 players. You've got to be quick and clever in this battle for belief...", with a thumbnail | todo | openblack has no network play; see ../network_play.md |
| The online map file holds its own script and this same landscape; the game unpacks both, loads them and deletes them | todo | openblack does not read online map files |
| Online no god is a computer god and the script makes no creatures: each person brings their own | todo | see ../network_play.md |
| Online every town's belief is capped evenly for all four gods (2 in the four home towns, 1 elsewhere) | todo | belief caps are not done |
| Online the camera always starts on your own temple | todo | |
| The winning conditions offered for it start at 1,000 food, 1,000 wood, 1,100 belief and up to 12 towns taken | todo | see ../multiplayer_rules.md |
