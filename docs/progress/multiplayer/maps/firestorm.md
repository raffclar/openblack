# Firestorm

A hot, dry three-god land thick with fire miracles: each god starts with a large town, eighteen shrine-like empty
villages each hold one miracle dispenser in a ring round a rich Norse wonder town, and over two hundred one-shot
miracles, a fifth of them fire, lie on the ground. It is a copy of one of Lionhead's later online maps, which were only
ever offered from the online servers; its script sets up no computer gods.

**Players:** 3 · **Landscape:** `Data/Landscape/mpm_3p_2.lnd` · **Script:** `Scripts/Playgrounds/Firestorm.txt` · **Mode:** both

**Progress: 14/31 done, 5 partial — 53%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#firestorm-loading) (its "Firestorm" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Where it comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script is the developers' three-player online map with only its landscape path and some villagers' ages changed; the game's own online condition list still names that map (`mpm_3p_2`) | n/a | history of the file; the original download is lost (see bwgame-service's notes on lost maps) |
| Neither the script nor its landscape is among the files the game installs; the skirmish box shows any script placed in its folder | n/a | how it reached this install is not part of the game |

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it by its file name, "Firestorm", and it has no start message | partial | the debug menu lists it under the same name (`src/Level.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it; see the next row |
| No story script runs on a skirmish land | todo | openblack starts the story's control script when a playground is the start land, and leaves story scripts running when one is opened from the debug menu |
| The camera starts at the script's start point, which lies off the land's edge over the sea rather than at your temple | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A low land about 1,450 by 1,600 paces: a third of it at sea level, the rest low ground and a central upland, crossed by four streams | done | `Game::LoadLandscape`; streams `StreamSegmentArchetype` |
| One hot climate over the whole land: about 25 degrees, with a long dry spell set and a steady wind | done | `WeatherSystem`; see ../../weather/ |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%) | done | `DayNightClock` |
| Five patches of mist | done | `MistArchetype` |
| 79 standing features: Egyptian needles, Aztec statues and limestone pillars | done | `FeatureArchetype` |
| The music follows the nearest town's tribe: Celtic, Aztec and Japanese at the three homes, Norse in the middle | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project almost no influence (0.01 of normal) and gods 0.8 of theirs, so each god can act only around its temple and must win towns by belief | done | `InfluenceSystem` |

## The three starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| You (player one) have a temple and a big Celtic town of 18 houses, a storehouse, four fields and about 45 villagers | done | towns, buildings and villagers as in playground_scripts.md |
| Player two has much the same with an Aztec town beside its temple, player three with a Japanese town | done | as above |
| Each home store starts with 10,000 food and 10,000 wood | done | `AbodeArchetype` keeps the amounts |
| Each home town believes in its god at 2 and offers fire, the strongest fire, shield, physical shield, water, creature invisibility and creature itchiness | partial | belief is set; village centre miracles are not offered (`CREATE_NEW_TOWN_SPELL`); see ../../miracles/ |
| Each home town has its own five dispensers: a stronger fire, two fires, heal and shield | todo | `CREATE_SPELL_DISPENSER` is empty |
| Each home town has a gathering point for worshippers | todo | `SET_TOWN_CONGREGATION_POS` logs "not implemented" |
| About 75 one-shot miracles lie round each home: fire, stronger fire, heal, shield, storm, lightning, teleport and creature miracles | todo | `CREATE_ONE_SHOT_SPELL_PU` is empty |

## The other two gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script switches on no computer gods and makes no creatures, so in a skirmish players two and three keep their temples and towns but nobody plays them | todo | openblack also leaves them unplayed, but whether the game's temple command itself makes them computer gods is not determined (that command's code is not decompiled); the skirmish start does not fill empty temples, unlike a network game |
| In a network game, a temple nobody joins gets a fully grown creature of a random kind | todo | see ../../rival_gods/skirmish_opponents.md |

## The ring of shrines and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Eighteen empty Celtic villages, with no houses or people, form a ring round the middle; each half-believes in nobody, has a gathering point and holds one dispenser (heal, creature bigness, weakness, anger, invisibility, compassion, strength, storm, lightning, teleport or fire) | todo | the villages are made as towns with their belief; dispensers and gathering points are not |
| The Norse town in the middle has a wonder, ten houses, about 26 people and 20,000 food and 20,000 wood, believes in nobody at a quarter, and offers heal, a stronger heal, shield, physical shield, water, creature freeze and creature strength | partial | built with its stores and belief; its miracles are not offered |
| 18 pots of magic wood and 28 drinking places for animals | partial | the pots are made; drinking places are not (`CREATE_DRINK_WAYPOINT` is empty) |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 304 trees (oaks, pines, palms, birches, olives, cedars, bushes) and nine big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to ten forests that foresters fell and replant | todo | `CREATE_FOREST` is empty |
| 70 animals in 12 flocks: cows and horses only | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 fields | done | `FieldArchetype` |
| 149 rocks and boulders, three cuddly toys, magic mushrooms and toadstools to pick up, 32 lanterns and a street light | done | `MobileStaticArchetype`, `MobileObjectArchetype`, `StreetLanternArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With no computer gods playing, a skirmish here has no opponent to beat; the skirmish's own rule decides when it ends | todo | not determined what ends a skirmish; see ../skirmish.md |
| Online its winning conditions offered no building goals (houses, wonders, new towns and converts were switched off by default, with a note that a bug was still to be fixed), at most 50 villagers born, 11,000 to 30,000 food or wood, 2,100 to 20,000 belief and at most 2 towns taken | todo | the game's condition list; see ../multiplayer_rules.md |

## Online

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Online this map was downloaded from the servers like the three shipped ones | n/a | the servers and this map's online file are gone; bwgame-service could offer the copy |
| Online every temple belongs to a person, and the camera always starts on your own temple | todo | openblack has no network play; see ../network_play.md |
