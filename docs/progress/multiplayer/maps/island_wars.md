# Island Wars

Four gods on a cluster of low islands: you and three computer gods, all with Nemesis-minded creatures, each hold a
Japanese town on your own island, with a rich Greek town in the middle and Indian fishing villages between. It is a
fan's rework, made with a community map editor, of one of Lionhead's later online maps that were only ever offered from
the online servers.

**Players:** 4 · **Landscape:** `Data/Landscape/mpm_4p_2.lnd` · **Script:** `Scripts/Playgrounds/Island Wars.txt` · **Mode:** skirmish

**Progress: 9/31 done, 11 partial — 47%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#island-wars-loading) (its "Island Wars" sections); this
file covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer
gods are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md) and Nemesis's creature
mind in [../../rival_gods/nemesis.md](../../rival_gods/nemesis.md).

## Where it comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script opens with a note that it was modified with a fan-made map editor; it is the developers' four-player online map (named `mpm_4p_2` in the game's online condition list) with three computer gods and their creatures added, temples moved after their towns, and the two small Greek towns' miracles reshuffled | n/a | history of the file; the original online download is lost |
| Neither the script nor its landscape is among the files the game installs; the skirmish box shows any script placed in its folder | n/a | how it reached this install is not part of the game |

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it by its file name, "Island Wars", and it has no start message | partial | the debug menu lists it under the same name (`src/Level.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it; see the next row |
| No story script runs on a skirmish land | todo | openblack starts the story's control script when a playground is the start land, and leaves story scripts running when one is opened from the debug menu |
| The camera starts at the script's start point, kept from another map, which lies off the land's edge over the sea rather than at your temple | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A small square land about 1,270 paces across, three quarters of it at or near sea level: a scatter of low islands, one per god, round a central island, with nothing higher than about half the highest possible height | done | `Game::LoadLandscape` |
| No climate is set, so the land keeps the game's default weather | todo | what the game's weather does on a land with no climate is not determined |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%) | done | `DayNightClock` |
| The music follows the nearest town's tribe: Japanese at every home, Indian between, Greek in the middle | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project a fifth of their normal influence and gods 1.2 times theirs, so the gods reach across the water | done | `InfluenceSystem` |
| A ring of influence placed near player four's island | todo | `CREATE_INFLUENCE_RING` is empty |

## The four starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every god has a temple and the same Japanese town: eight houses, a storehouse, a workshop, a village centre, three fields, two fish farms and about 20 villagers | partial | buildings and villagers are made; fish farms are not |
| Each home store starts with 10,000 food and its workshop 10,000 wood (20,000 wood in all) | done | `AbodeArchetype` keeps the amounts |
| Each home town believes in its god at 2 and offers heal, teleport, shield, physical shield, water and creature compassion | partial | belief is set; village centre miracles are not offered (`CREATE_NEW_TOWN_SPELL`); see ../../miracles/ |

## The computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Computer gods are switched on for players two, three and four | partial | the players are made; nothing thinks for them (see ../skirmish.md) |
| Player two has a leopard, player three a gorilla and player four a brown bear, each next to its home town, all with Nemesis's creature mind from the story | partial | `CREATE_CREATURE_FROM_FILE` (`CreatureArchetype`) with the mind loaded; see ../../scripts/creature_mind_scripts.md |
| The creatures are sized to match your own | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is ignored |
| No personalities are loaded, so all three play with the default personality | todo | the computer god is not written |
| No belief caps are set for anyone: unlike the game's own lands, the computer gods are not handicapped | done | nothing to cap |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Greek town in the middle has sixteen houses, a crèche, a workshop, a graveyard, six fields, about 39 people and 10,000 food and 20,000 wood, believes in nobody fully, and offers ten miracles: fire, the strongest fire, lightning, beam explosion and its strongest form, heal and a stronger heal, storm and the strongest storm, and physical shield | partial | built with its stores and belief; its miracles are not offered |
| Two Greek village centres with no houses, between the homes, each offering fire, a stronger fire, food, wood and water | partial | the centres and their belief are made; their miracles are not offered |
| Four Indian fishing villages, one on the way from each home to the middle, each with five houses and ten people, offering fire, lightning, creature freeze, invisibility and itchiness | partial | built; miracles are not offered and fish farms are not made |
| The middle and small towns each have a gathering point for worshippers | todo | `SET_TOWN_CONGREGATION_POS` logs "not implemented" |
| Four bonfires by the Indian villages | partial | a static bonfire; one number ignored |
| Eight one-shot miracles: storms, creature strength and creature freeze | todo | `CREATE_ONE_SHOT_SPELL_PU` is empty |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 458 trees (bushes, conifers, oaks, palms, beech, olives) and eleven big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to 13 forests that foresters fell and replant | todo | `CREATE_FOREST` is empty |
| 75 animals in 18 flocks: pigs, gulls and a few sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 18 fields, 24 fish farms and six pots of magic wood | partial | fields and pots are made; fish farms are not |
| 48 rocks and 17 street lanterns | done | `MobileStaticArchetype`, `StreetLanternArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |

## Online

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The developers' original of this land was downloaded from the online servers; its winning conditions started at 11,000 food or wood, 2,100 belief and up to 8 towns taken | n/a | the servers and that map's online file are gone; the game's condition list still holds its numbers (../multiplayer_rules.md) |
