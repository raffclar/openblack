# Land script commands

Every land of Black & White, the tutorial, the skirmish playgrounds and the developers' test lands is built by a plain text land script in `Scripts/`: one command a line, read from top to bottom when the land loads. The game knows 106 such commands for building a land and 25 more for the map script that picks the land. The command names below are the words written in the shipped files. Counts are lines in the shipped `Land1`–`Land5`, `LandT`, the seven `Playgrounds` scripts and `demo2`, `comp` and `dance`. openblack reads them in `src/LHScriptX/` (`FeatureScriptCommands.cpp`, `MapScriptCommands.cpp`, `Script.cpp`, `Lexer.cpp`). What each land builds is in the per-land files of this folder.

**Progress: 44/134 done, 12 partial — 37%**

## Reading the scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land script is read line by line, one command and its bracketed arguments a line | done | `Script.cpp`, `Lexer.cpp` |
| Lines starting `rem` and the `**** Player[n] ****` banner lines are skipped | done | `Lexer.cpp` skips both |
| Positions are written as "x,z" in quotes and put on the ground | done | `Script.cpp` takes the land's height there |
| Names may be written without quotes (job names, miracle names) | done | identifiers are taken as text |
| Each argument is converted to what the command wants: a whole number given where a fraction is wanted (or the other way) still works | partial | openblack wants the exact kind and stops the land on a mismatch; every shipped script happens to pass |
| A command the game doesn't know is skipped and the rest of the land still loads | todo | openblack stops reading the land at the first unknown command (the playground *Death Comes To Those That Wait* stops at line 83 of 5,163, on a misspelt villager command) |
| A line missing its closing bracket is still read | todo | openblack stops at it (*Death Comes To Those That Wait* has one at line 2,448) |
| The script reader's variables and `IF` lines | todo | not used by any shipped land script; openblack's reader has neither |
| After the last line the buildings are given to their towns, the towns' starting dates set, the players tidied and the streams built | partial | streams built as their points come; buildings belong to their town by number; the rest unconfirmed |
| The land's footpath file is found from the land script's own name (`land1.fot` for `Land1.txt`) | done | `Game::LoadMap`; the skirmish lands *Death Comes*, *Firestorm*, *Island Wars* and `construct` have none, as in the game |
| An unreadable land is kept as far as it got, not ending the game | partial | `Game::LoadMap` logs and carries on (whether the game keeps it the same way unconfirmed) |

## Land set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `SET_GLOBAL_LAND_BALANCE`: Sets one of the land's balance numbers (how fast villagers move, how fast belief grows, how impressive miracles are and others); used 30 times (story 9, playgrounds 21) | partial | all eight kept; only the impressiveness, speed and belief-speed numbers are read (the shipped scripts also set numbers 0, 1, 3, 5 and 6) |
| `LOAD_LANDSCAPE`: Loads the land's height map and textures; used 16 times (story 5, tutorial 1, playgrounds 7, demo and test 3) | done | `Game::LoadLandscape` |
| `SET_LAND_NUMBER`: Says which land of the story this is (0 for skirmish and test lands); used 16 times (story 5, tutorial 1, playgrounds 7, demo and test 3) | done | kept in the map script globals; used by influence and villager speed |
| `START_CAMERA_POS`: Where the camera starts, looking at a point from the game's standard height and angle; used 16 times (story 5, tutorial 1, playgrounds 7, demo and test 3) | done | `StartCameraPos`; test `SetCameraPos` |
| `VERSION`: States the version of the land script format; used 16 times (story 5, tutorial 1, playgrounds 7, demo and test 3) | done | `FeatureScriptCommands::Version` logs it; the game's use of the number unconfirmed |
| `SET_PLAYER_INFLUENCE_MULTIPLIER`: Scales how far every player's influence from their temple and buildings reaches; used 13 times (story 5, tutorial 1, playgrounds 7) | done | `InfluenceSystem` reads it |
| `SET_TOWN_INFLUENCE_MULTIPLIER`: Scales how far every town's influence reaches on this land; used 13 times (story 5, tutorial 1, playgrounds 7) | done | `InfluenceSystem` reads it |
| `SET_NIGHTTIME`: Sets the length of the day and how much of it is night and dawn and dusk; used 8 times (story 2, playgrounds 6) | done | `DayNightClock::SetCycleFromLand` |
| `ADD_GAME_MESSAGE_LINE`: A line of a skirmish land's description; used 5 times (playgrounds 5) | partial | the command is empty; the debug menu's land list reads the line itself for its tooltip |
| `START_GAME_MESSAGE`: The title of a skirmish land shown when choosing it; used 5 times (playgrounds 5) | partial | the command is empty; the debug menu's land list reads the line itself for its tooltip. In the game, reaching it pops up the land info box with this title and the description lines (the skirmish box lists lands by file name): see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| `BRUSH_SIZE`: Sets the editor's brush size; not used by the shipped scripts | n/a | an editor setting, empty in openblack |
| `CREATE_AREA`: Marks an area of the land with a radius; not used by the shipped scripts | todo | empty; what the game uses it for unconfirmed |
| `EDIT_LEVEL`: Opens the land in the game's editor; not used by the shipped scripts | todo | empty |
| `FLY_BY_FILE`: Names a camera fly-by for the land; not used by the shipped scripts | todo | empty |
| `MULTIPLAYER_DEBUG`: Turns on network debugging; not used by the shipped scripts | n/a | a developer switch |
| `SET_A_TOWNS_INFLUENCE_MULTIPLIER`: Scales one town's influence; not used by the shipped scripts | todo | logs "not implemented" |
| `SET_LAND_BALANCE`: Sets a balance number for one player; not used by the shipped scripts | todo | empty |
| `SET_LOST_TOWN_SCALE`: Scales what a player keeps of a town they lose; not used by the shipped scripts | partial | kept and used for boredom; the belief kept by the loser todo (a TODO in the code) |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_NEW_TOWN_SPELL`: Gives a town a miracle it offers at its village centre once won over; used 446 times (story 107, playgrounds 326, demo and test 13) | todo | logs "not implemented" |
| `SET_TOWN_BELIEF_CAP`: Caps how much a town can come to believe in a player; used 157 times (story 24, playgrounds 133) | todo | logs "not implemented" |
| `CREATE_TOWN`: Creates a town at a place, owned by a player or nobody, of a tribe; used 152 times (story 43, tutorial 3, playgrounds 95, demo and test 11) | done | `TownArchetype` |
| `SET_TOWN_BELIEF`: Sets how much a town believes in a player; used 130 times (story 40, tutorial 2, playgrounds 77, demo and test 11) | done | into the town's belief list |
| `SET_TOWN_CONGREGATION_POS`: Where a town's people gather (for example to watch a miracle or wait for the player); used 32 times (story 4, playgrounds 28) | todo | logs "not implemented" |
| `CREATE_TOWN_SPELL`: Gives a town a miracle its people can pray for (the older form); used 17 times (demo and test 17) | todo | logs "not implemented" |
| `SET_TOWN_UNINHABITABLE`: Marks a town that nobody may live in (ruins, deserted villages); used 13 times (story 13) | todo | logs "not implemented" |
| `TOWN_DESIRE_BOOST`: Raises or lowers one of a town's desires (more houses, more civic buildings and so on); used 8 times (story 8) | todo | empty |
| `CREATE_PLANNED_SPELL_ICON`: A planned miracle icon for a town; not used by the shipped scripts | todo | logs "not implemented" |
| `CREATE_SPELL_ICON`: Puts a miracle's icon at a place; not used by the shipped scripts | todo | logs "not implemented" |
| `CREATE_TOWN_CENTRE_SPELL_ICON`: Puts a miracle's icon on a town's village centre; not used by the shipped scripts | todo | logs "not implemented" |
| `CREATE_TOWN_TEMPORARY_POTS`: Gives a town some temporary food and wood; not used by the shipped scripts | todo | empty |
| `SET_TOWN_BALANCE_BELIEF_SCALE`: Scales how much a town's belief counts; not used by the shipped scripts | todo | empty |
| `TOWN_NEEDS_POS`: Where a town's needs are shown; not used by the shipped scripts | todo | empty |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_ABODE`: Creates a building of a town (house, storehouse, crèche, graveyard, workshop, wonder and so on) with its food and wood; used 1109 times (story 266, tutorial 10, playgrounds 708, demo and test 125) | done | `AbodeArchetype`; unknown names skipped |
| `CREATE_PLANNED_ABODE`: Plans a building for a town to build later; used 514 times (story 258, tutorial 5, playgrounds 149, demo and test 102) | todo | logs "not implemented" |
| `CREATE_TOWN_CENTRE`: Creates a town's village centre; used 107 times (story 23, playgrounds 75, demo and test 9) | partial | made as a building; the worship share it is given is ignored |
| `CREATE_SPELL_DISPENSER`: Creates a miracle dispenser of a tribe that gives a miracle, with its angle, scale and recharge time; used 45 times (story 2, playgrounds 43) | todo | empty |
| `CREATE_CITADEL`: Creates a player's temple; used 28 times (story 4, playgrounds 20, demo and test 4) | done | `CitadelArchetype` |
| `CREATE_WORSHIP_SITE`: Creates a worship site for a player and tribe; used 19 times (story 8, playgrounds 5, demo and test 6) | todo | empty |
| `CREATE_SCAFFOLD`: Creates a scaffold for a building; used 12 times (playgrounds 12) | todo | empty; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| `CREATE_PLANNED_CITADEL`: Plans where a player's temple will be built; used 5 times (story 5) | done | `CitadelArchetype::CreatePlan` |
| `CREATE_BASE`: Creates a base; not used by the shipped scripts | todo | empty; unconfirmed what a base is |
| `CREATE_CREATURE_PEN`: Creates a creature pen; not used by the shipped scripts | todo | empty |
| `CREATE_FURNITURE`: Creates furniture; not used by the shipped scripts | todo | empty |
| `CREATE_PITCH`: Creates a football pitch; not used by the shipped scripts | todo | empty |
| `CREATE_PLANNED_WALL_SECTION`: Plans a piece of wall; not used by the shipped scripts | todo | empty |
| `CREATE_PLANNED_WORSHIP_SITE`: Plans a worship site; not used by the shipped scripts | todo | empty |
| `CREATE_WALL_SECTION`: Creates a piece of wall; not used by the shipped scripts | todo | empty |

## Villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_VILLAGER_POS`: Creates a villager of a tribe and job of an age, living in the house at a place; used 2248 times (story 618, tutorial 8, playgrounds 1299, demo and test 323) | done | `VillagerArchetype` |
| `CREATE_TOWN_VILLAGER`: Creates a villager of a tribe and job of an age in a town; used 46 times (story 2, playgrounds 15, demo and test 29) | todo | logs "not implemented" |
| `CREATE_SPECIAL_TOWN_VILLAGER`: Creates one of the game's named villagers in a town; not used by the shipped scripts | todo | logs "not implemented" |
| `CREATE_VILLAGER`: Creates a villager at a place, going to a place; not used by the shipped scripts | todo | logs "not implemented" |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_NEW_TREE`: Creates a tree of a kind, scenic or not, with its angle, size and the size it will grow to; used 11946 times (story 5316, tutorial 341, playgrounds 4581, demo and test 1708) | done | `TreeArchetype` |
| `FIRE_FLY_SPELL_REWARD_PROB`: How likely fireflies are to give each miracle; used 633 times (story 210, tutorial 42, playgrounds 381) | todo | empty; every land's table: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table) |
| `CREATE_MIST`: Creates a patch of mist of a colour and size; used 186 times (story 111, playgrounds 5, demo and test 70) | done | `MistArchetype` |
| `CREATE_NEW_FEATURE`: Creates a feature (pillars, statues, craters, a pier) by name; used 182 times (story 76, tutorial 2, playgrounds 79, demo and test 25) | done | `FeatureArchetype`; unknown names skipped |
| `CREATE_FIRE_FLY`: Creates a firefly that can carry a miracle reward; used 167 times (playgrounds 129, demo and test 38) | todo | empty; the story lands place none and rely on the nightly top-up: [../nature/fireflies.md](../nature/fireflies.md#where-they-come-from) |
| `CREATE_FOREST`: Creates a forest that trees belong to; used 143 times (story 9, playgrounds 107, demo and test 27) | todo | empty; the trees are still made, unattached |
| `CREATE_NEW_BIG_FOREST`: Creates a big forest model; used 113 times (story 46, tutorial 4, playgrounds 47, demo and test 16) | done | `BigForestArchetype` |
| `CREATE_DEAD_TREE`: Creates a dead tree of a kind, leaning; used 18 times (story 3, demo and test 15) | partial | `DeadTreeArchetype`; the lean (roll and pitch) is ignored |
| `CREATE_BIG_FOREST`: Creates a big forest model (older form); not used by the shipped scripts | done | through `CreateNewBigForest` |
| `CREATE_FEATURE`: Creates a feature (pillars, statues, rocks) by number; not used by the shipped scripts | done | `FeatureArchetype` |
| `CREATE_FLOWERS`: Creates flowers; not used by the shipped scripts | done | `FlowersArchetype` |
| `CREATE_TREE`: Creates a tree (older form); not used by the shipped scripts | done | through `CreateNewTree` |

## Fields and food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_NEW_TOWN_FIELD`: Creates a field of a town, of a kind, with its angle; used 393 times (story 160, tutorial 2, playgrounds 231) | done | `FieldArchetype` |
| `CREATE_TOWN_FISH_FARM`: Creates a fish farm of a town; used 185 times (story 69, tutorial 5, playgrounds 104, demo and test 7) | todo | empty |
| `CREATE_POT`: Creates a pot of food or wood with an amount; used 81 times (story 22, tutorial 2, playgrounds 28, demo and test 29) | done | `PotArchetype` |
| `CREATE_DRINK_WAYPOINT`: Marks a place where animals and creatures go to drink; used 75 times (story 47, playgrounds 28) | todo | empty |
| `CREATE_TOWN_FIELD`: Creates a field of a town; used 18 times (demo and test 18) | done | through `CreateNewTownField` |
| `CREATE_FIELD`: Creates a field; not used by the shipped scripts | done | through `CreateNewTownField` |
| `CREATE_FISH_FARM`: Creates a fish farm; not used by the shipped scripts | todo | empty |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_NEW_ANIMAL`: Creates an animal of a kind with its age and owner; used 3529 times (story 934, tutorial 488, playgrounds 2107) | todo | empty |
| `CREATE_FLOCK`: Creates a flock of animals around a place, with its size and spread; used 459 times (story 119, tutorial 19, playgrounds 310, demo and test 11) | todo | empty |
| `CREATE_ANIMAL`: Creates an animal (older form); used 80 times (demo and test 80) | todo | empty |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_MOBILE_STATIC`: Creates a static object (rocks, fences, toys, totems, gates) with its height, angles and scale; used 2101 times (story 1522, tutorial 105, playgrounds 280, demo and test 194) | done | `MobileStaticArchetype` |
| `CREATE_STREET_LANTERN`: Creates a street or country lantern; used 302 times (story 89, tutorial 6, playgrounds 207) | done | `StreetLanternArchetype` |
| `CREATE_MOBILEOBJECT`: Creates a moveable object (mushrooms, barrels, carts, poo) with its angle and scale; used 159 times (story 97, playgrounds 45, demo and test 17) | done | `MobileObjectArchetype` |
| `CREATE_BONFIRE`: Creates a bonfire; used 11 times (story 6, tutorial 1, playgrounds 4) | partial | made as a static bonfire; its third number ignored |
| `CREATE_ANIMATED_STATIC`: Creates an animated object (gates, cave entrances) by name; used 7 times (story 7) | done | `AnimatedStaticArchetype` |
| `CREATE_STREET_LIGHT`: Creates a street light; used 1 times (playgrounds 1) | done | `StreetLanternArchetype` |
| `CREATE_MOBILESTATIC`: Creates a static object (older form); not used by the shipped scripts | done | through `CreateMobileUStatic` |
| `MAKE_LAST_OBJECT_ARTIFACT`: Makes the last object made a town's artefact; not used by the shipped scripts | todo | empty |

## Miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_ONE_SHOT_SPELL_PU`: Creates a one-shot miracle, powered up; used 244 times (story 2, playgrounds 242) | todo | empty |
| `CREATE_ARENA`: Creates an arena where creatures fight; used 5 times (story 4, demo and test 1) | todo | empty |
| `CREATE_INFLUENCE_RING`: Creates a ring of influence for a player; used 1 times (playgrounds 1) | todo | empty |
| `CREATE_ONE_SHOT_SPELL`: Creates a one-shot miracle that can be picked up and cast once; not used by the shipped scripts | todo | empty |

## Weather and land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_STREAM_POINT`: Adds a point to a stream; used 1123 times (story 1015, tutorial 37, playgrounds 71) | done | `StreamSegmentArchetype` between points |
| `CREATE_STREAM`: Starts a stream; used 60 times (story 55, tutorial 1, playgrounds 4) | done | streams are kept |
| `CREATE_WEATHER_CLIMATE_RAIN`: Sets a climate's rain: how much and how many dry and wet days; used 29 times (story 21, playgrounds 6, demo and test 2) | done | `WeatherSystem::SetClimateRain` |
| `CREATE_WEATHER_CLIMATE_TEMP`: Sets a climate's temperature; used 29 times (story 21, playgrounds 6, demo and test 2) | done | `WeatherSystem::SetClimateTemperature` |
| `CREATE_WEATHER_CLIMATE_WIND`: Sets a climate's wind; used 29 times (story 21, playgrounds 6, demo and test 2) | done | `WeatherSystem::SetClimateWind` |
| `CREATE_WEATHER_CLIMATE`: Creates a climate over a circle of the land; used 27 times (story 21, playgrounds 6) | done | `WeatherSystem::CreateClimate` |
| `CREATE_WEATHER_STORM`: Creates a storm that is there from the start, with its rain, lightning and path; used 2 times (playgrounds 2) | todo | empty |
| `COUNTRY_CHANGE`: Changes the land's country (look) around a place; not used by the shipped scripts | todo | empty |
| `CREATE_FOOTPATH_NODE`: Adds a point to a footpath; not used by the shipped scripts | done | nodes kept |
| `CREATE_FOOTPATH`: Starts a footpath; not used by the shipped scripts | done | footpaths kept (the shipped ones come from the land's footpath file) |
| `CREATE_PATH`: Creates a path; not used by the shipped scripts | todo | logs "not implemented" |
| `CREATE_WATERFALL`: Creates a waterfall; not used by the shipped scripts | todo | empty |
| `HEIGHT_CHANGE`: Changes the land's height around a place; not used by the shipped scripts | todo | empty |
| `LINK_FOOTPATH`: Links a footpath to the last object made (a gate or bridge); not used by the shipped scripts | todo | logs "not implemented" |

## Creatures and rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_CREATURE_FROM_FILE`: Creates a rival god's creature of a species from a mind file; used 16 times (playgrounds 16) | partial | `CreatureArchetype` with the mind loaded; faces a fixed way, its size from openblack's starting tables |
| `SET_COMPUTER_PLAYER_CREATURE_LIKE`: Makes a rival god's creature like another player; used 16 times (playgrounds 16) | todo | empty |
| `TOGGLE_COMPUTER_PLAYER`: Turns a rival god (computer player) on or off; used 16 times (story 4, playgrounds 11, demo and test 1) | partial | creates the player; the on/off is ignored. In the game it switches on that player's computer god mind and marks it a computer player, and is ignored while a network game's map loads: see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |
| `CREATE_CREATURE`: Creates a creature (older form); used 1 times (demo and test 1) | todo | logs "not implemented" |
| `LOAD_COMPUTER_PLAYER_PERSONALLTY`: Loads a rival god's personality; not used by the shipped scripts | todo | empty |
| `SET_COMPUTER_PLAYER_PERSONALLTY`: Sets a rival god's personality; not used by the shipped scripts | todo | empty |
| `SET_INTERACT_DESIRE`: Sets the desire to interact; not used by the shipped scripts | todo | empty; unconfirmed whose desire |

## Commands written wrongly in the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `CREATE_SPECIAL_TOWNVILLAGER` (a misspelling of `CREATE_SPECIAL_TOWN_VILLAGER`) is written 119 times in *Death Comes To Those That Wait*; the game doesn't know it and skips each line, so those named villagers are never made | todo | openblack stops the whole land at the first one instead of skipping it |
| One `FIRE_FLY_SPELL_REWARD_PROB` line in *Death Comes To Those That Wait* lacks its closing bracket; the game reads it | todo | openblack would stop there (it already stopped at line 83) |
| One miracle in *Death Comes To Those That Wait* is misspelt (`EXPLSION_PU1`); the game finds no such miracle for that town | n/a | data error in the shipped file; nothing to do beyond ignoring it, which openblack would also need to |

## Map script commands

The map script (`map.txt`) is what the game reads first to choose a land; the shipped one has one line, loading the first land. openblack skips it and loads a land script directly (`--start-level`, default `Land1.txt`).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `SET_NO_PLAYERS`: Sets the number of players; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `LOAD_TRIBE_DANCE`: Loads a tribe's dance; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `SET_DATE`: Sets the date; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `SET_TIME`: Sets the time; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `SET_TURNS_PER_YEAR`: Sets how many game turns make a year; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `SET_GAME_TICK_TIME`: Sets how long a game turn lasts; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `LOAD_FEATURE_SCRIPT`: Loads a land script (the shipped map script loads the first land); used 1 time in `map.txt` | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `PAUSE_GAME`: Pauses the game; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `CREATE_CREATURE`: Creates a creature; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `OUTPUT_VILLAGERS`: Writes the villagers out (debugging); not used by the shipped map script | n/a | developer output |
| `OUTPUT_TOWN`: Writes the towns out (debugging); not used by the shipped map script | n/a | developer output |
| `OUTPUT_CREATURES`: Writes the creatures out (debugging); not used by the shipped map script | n/a | developer output |
| `OUTPUT_COLLIDE`: Writes collisions out (debugging); not used by the shipped map script | n/a | developer output |
| `OUTPUT_ALLOC`: Writes memory use out (debugging); not used by the shipped map script | n/a | developer output |
| `SAVE_FOR_NET_DEBUG`: Saves for network debugging; not used by the shipped map script | n/a | developer output |
| `LOAD_FOR_NET_DEBUG`: Loads for network debugging; not used by the shipped map script | n/a | developer output |
| `LOAD_LANDSCAPE`: Loads a landscape; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `LOAD_GAME_SCRIPT`: Loads the compiled challenge script; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `LOAD_RAW_GAME_SCRIPT`: Loads challenge script source; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `LOAD_LANGUAGE`: Loads the language; not used by the shipped map script | todo | in `MapScriptCommands.cpp`, throws "not implemented"; openblack never runs a map script |
| `LOAD_CHALLENGES`: Loads the challenges; not used by the shipped map script | todo | not in openblack's map command table |
| `OUTPUT_RAND`: Writes the random numbers out (debugging); not used by the shipped map script | n/a | developer output |
| `LOAD_OLD_CREATURE`: Loads the player's old creature; not used by the shipped map script | todo | not in openblack's map command table |
| `CONFIG`: Reads the configuration; not used by the shipped map script | todo | not in openblack's map command table |
| `KEYCONFIG`: Reads the key configuration; not used by the shipped map script | todo | not in openblack's map command table |
