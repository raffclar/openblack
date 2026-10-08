# Three Gods

Three gods around a central mountain: you and two computer gods each start with a home town and three neutral towns
of your own tribe nearby, and a rich Tibetan town with a wonder sits in the middle. Each home lies under its own
climate, one of them freezing. Online the same land is the three-player map "King of the hill".

**Players:** 3 · **Landscape:** `Data/Landscape/Multi_Player/MPM_3P_1.lnd` · **Script:** `Scripts/Playgrounds/ThreeGods.txt` · **Mode:** both

**Progress: 11/40 done, 8 partial — 38%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#three-gods-loading) (its "Three Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it under the game's own line "Defeat two gods to control this realm." rather than its file name | partial | the debug menu lists it as "Three Gods" with "Battle three gods for this realm" as a tooltip (`src/Debug/Gui.cpp`, `src/Level.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it; see the next row |
| No story script runs on a skirmish land | todo | openblack starts the story's control script when a playground is the start land, and leaves story scripts running when one is opened from the debug menu |
| The camera starts looking at player three's home town, not yours (the script's start point is that town's centre) | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |
| The land's title "Three Gods" and its line "Battle three gods for this realm" are its start message (the line miscounts: there are two other gods) | todo | the start message commands are ignored |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land about 1,900 by 1,750 paces built around a central mountain that reaches the highest height a land can have, with a fifth of it at sea level round the edges | done | `Game::LoadLandscape`; the online name "King of the hill" refers to it |
| Three local climates: your home lies in a freezing, windy one (about -35 degrees), player three's in a cold, windy one (about 7 degrees), both set to be raining from the start, and player two's in a warm one (about 24 degrees) that starts dry | done | `CREATE_WEATHER_CLIMATE` and its rain, temperature and wind (`WeatherSystem`); see ../../weather/ |
| Two storms are already blowing at the start, one in your cold climate and one in player three's | todo | `CREATE_WEATHER_STORM` is empty |
| Short days and long nights: a day of 1,000 seconds with 23% night and 17% dawn and dusk | done | `DayNightClock` |
| The music follows the nearest town's tribe: Norse around you, Japanese around player two, Celtic around player three, Tibetan in the middle | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project 0.4 of their normal influence and gods 0.7 of theirs, so all three start further apart in reach | done | `InfluenceSystem` |

## The three starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| You (player one) start with a temple and a Norse home town: seven houses, a crèche, a storehouse, a workshop, six fields and about 20 villagers | done | towns, buildings and villagers as in playground_scripts.md |
| Player two starts the same with a Japanese town, player three with a Celtic town | done | as above |
| Each home town's store starts with about 3,600 to 4,000 food and wood, and no worship sites, dispensers or one-shot miracles are placed: everything else is won from towns | done | `AbodeArchetype` keeps the amounts |
| Each home town believes in its god at 1.5 and offers heal, teleport and water | partial | belief is set; village centre miracles are not offered (`CREATE_NEW_TOWN_SPELL`); see ../../miracles/ |
| Each home town has four to six buildings planned for its people to build | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |

## The computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Computer gods are switched on for players two and three | partial | the players are made; nothing thinks for them (see ../skirmish.md) |
| Player two has a lion and player three a horse, both from the generic computer creature mind, standing on their temples | partial | `CREATE_CREATURE_FROM_FILE` (`CreatureArchetype`) with the mind loaded |
| Both creatures are sized to match your own | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is ignored |
| No personalities are loaded, so both play with the default personality | todo | the computer god is not written |
| Every town's belief in either computer god is capped at 1, their own home towns included, while yours is not, a handicap in your favour | todo | `SET_TOWN_BELIEF_CAP` logs "not implemented" |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each god has three neutral towns of its own tribe nearby, laid out alike: one offering shield and physical shield, one storm, flying flock, creature freeze and creature weakness, and a farther fishing town offering fire and lightning | partial | towns, houses and villagers are built; miracles are not offered and fish farms are not made |
| The Tibetan town in the middle has a wonder, a graveyard, nine houses, twelve fields and 20,000 food and 20,000 wood, believes in nobody at 1.5, and offers lightning, stronger lightning, beam explosion, storm and the strongest storm | partial | built with its stores and belief; its miracles are not offered |
| The Tibetan town has twelve more buildings planned, including a wonder | todo | planned buildings are not made |
| Six people stand in towns without a house | todo | `CREATE_TOWN_VILLAGER` logs "not implemented" |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 684 trees (beech, conifers, palms, oaks, birches, olives, pines) and six big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to 19 forests that foresters fell and replant | todo | `CREATE_FOREST` is empty |
| 43 fireflies | todo | `CREATE_FIRE_FLY` is empty; every reward weight is zero on this map, so catching one gives nothing; see [../../nature/fireflies.md](../../nature/fireflies.md) |
| 374 animals in 71 flocks, mostly sheep (192), with pigs, pigeons, cows, gulls, crows and doves | todo | `CREATE_NEW_ANIMAL` is empty |
| 82 fenced wheat fields, 18 fish farms and one magic wood pot | partial | fields and the pot are made; fish farms are not |
| Limestone rocks and boulders and 48 street lanterns | done | `MobileStaticArchetype`, `StreetLanternArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |

## Online as "King of the hill"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "King of the hill - 3 players", described as "3 Player map designed to test your godly powers. Try to take over the neutral towns to build up your holy powerbase, and destroy the other deities!", with a thumbnail | todo | openblack has no network play; see ../network_play.md |
| The online map file holds its own script and this same landscape; the game unpacks both, loads them and deletes them | todo | openblack does not read online map files |
| Online no god is a computer god and the script makes no creatures: each person brings their own | todo | see ../network_play.md |
| Online every town's belief is capped evenly for all three gods (2 in the three home towns, 1 elsewhere) | todo | belief caps are not done |
| Online the neutral Japanese storm town has a gathering point for worshippers | todo | `SET_TOWN_CONGREGATION_POS` logs "not implemented" |
| Online the camera always starts on your own temple | todo | |
| The winning conditions offered for it start at 5,000 food, 5,000 wood, 1,600 belief and up to 12 towns taken | todo | see ../multiplayer_rules.md |
