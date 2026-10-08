# Two Gods

The smallest of the game's own battle lands: you and one computer god face each other across a ridged, hilly land,
each with a home town and a second empty village, with six neutral towns between you. Online the same land is the
two-player map "Bombardment".

**Players:** 2 · **Landscape:** `Data/Landscape/Multi_Player/MPM_2P_1.lnd` · **Script:** `Scripts/Playgrounds/TwoGods.txt` · **Mode:** both

**Progress: 12/43 done, 8 partial — 37%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#two-gods-loading) (its "Two Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists every script in the playground folder; this one appears under the game's own line "Defeat another god to control this realm." rather than its file name | partial | the debug menu's "Playground Islands" lists it as "Two Gods" (from its start message), with "Battle another god for this realm" as a tooltip (`src/Debug/Gui.cpp`, `src/Level.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it; see the next row for the scripts |
| No story script runs on a skirmish land | todo | openblack starts the story's control script when a playground is the start land, and leaves story scripts running when one is opened from the debug menu |
| The camera starts on your home town's village centre | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |
| The land's title "Two Gods" and its line "Battle another god for this realm" are its start message | todo | the start message commands are ignored; see skirmish_opponents.md |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A compact land about 1,600 by 1,100 paces, the smallest of the battle lands, of steep ridges and ramps (hills up to three quarters of the highest possible), with a sixth of it at sea level and the sea along one long side | done | `Game::LoadLandscape`; the online description tells players to use "the ramps" to attack towns from afar |
| No climate is set, so the land keeps the game's default weather | todo | what the game's weather does on a land with no climate is not determined |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%), the standard day | done | `DayNightClock` |
| The music follows the tribe of the nearest town: Celtic around both home towns, Norse, African and Greek between | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project 0.4 of their normal influence and gods their full influence | done | `InfluenceSystem` |

## Your start (player one)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Your temple stands at one end of the land | done | `CitadelArchetype` |
| Your Celtic home town: eight houses, a crèche, a storehouse, a workshop and a village centre, six fields and 34 villagers | done | towns, buildings and villagers as in playground_scripts.md |
| Its storehouse starts with 8,000 food and 3,000 wood | done | `AbodeArchetype` keeps the amounts |
| It believes in you fully and offers heal, teleport, water and a stronger water | partial | belief is set; the village centre miracles are not offered (`CREATE_NEW_TOWN_SPELL` logs "not implemented"); see ../../miracles/ |
| A second town of yours, about 300 paces from the first: an Indian village centre with no houses, offering fire, nature, storm, a stronger storm and physical shield | partial | the centre and its belief are made; its miracles are not offered |
| Two worship sites by your temple, Indian and Celtic | todo | `CREATE_WORSHIP_SITE` is empty |
| Water and flying flock one-shot miracles lying near your temple | todo | `CREATE_ONE_SHOT_SPELL_PU` is empty |

## The computer god (player two)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A computer god is switched on for player two | partial | the player is made; nothing thinks for it (see ../skirmish.md) |
| Its temple, Celtic home town and empty Indian village mirror yours at the far end of the land | done | as above |
| Its home town starts with the same stores (8,000 food, 3,000 wood) and the same water miracles | partial | stores done; miracles not offered |
| Its Indian village lacks the physical shield yours offers, the land's one difference between the two gods | todo | town miracles are not offered |
| It has a leopard from the generic computer creature mind, standing beside its home town | partial | `CREATE_CREATURE_FROM_FILE` (`CreatureArchetype`) with the mind loaded; see ../../scripts/creature_mind_scripts.md |
| The leopard is sized to match your own creature | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is ignored |
| No personality is loaded for it, so it plays with the computer god's default personality | todo | the computer god is not written |
| Every town's belief in the computer god is capped (at 1, or 2 in its own home town) while yours is not, a handicap in your favour | todo | `SET_TOWN_BELIEF_CAP` logs "not implemented" |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Six neutral towns with half belief in nobody: nearer you an African town (7 houses) and a Norse fishing town (6, with fish farms); in the middle a Greek town (7); nearer the computer god two Norse towns (5, and 6 with fish farms) | partial | towns, houses and villagers are built; fish farms are not |
| The neutral towns offer storms, flocks, fire, lightning, wood and shield miracles to whoever wins them | todo | town miracles are not offered |
| An Egyptian building site between the two gods: twelve scaffolds and a beam explosion dispenser, with no people | todo | scaffolds and dispensers are empty commands |
| One beam explosion one-shot lying near the computer god's side | todo | one-shots are not made |
| Three pots of magic wood | done | `PotArchetype` |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 308 trees (mostly conifers and palms) and three big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to four forests that foresters fell and replant | todo | `CREATE_FOREST` is empty |
| 151 animals in 22 flocks: sheep, cows, pigs, horses, gulls, pigeons, crows and eight wolves | todo | `CREATE_NEW_ANIMAL` is empty |
| Rocks, street lanterns and Champi mushrooms to pick up | done | `MobileStaticArchetype`, `StreetLanternArchetype`, `MobileObjectArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |

## Online as "Bombardment"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "Bombardment - 2 players", described as "Bombardment action for 2 players. Use the ramps to your advantage as you try to take over enemy towns from afar.", with a thumbnail | todo | openblack has no network play; see ../network_play.md |
| The online map file holds its own script and this same landscape; the game unpacks both into the game folder, loads them and deletes them | todo | openblack does not read online map files |
| Online neither god is a computer god and no creatures are made by the script: each person brings their own | todo | see ../network_play.md |
| Online every town's belief is capped evenly for both gods (2 in the two home towns, 1 elsewhere), unlike the one-sided caps of the skirmish | todo | belief caps are not done |
| Online the two empty Indian villages and the Egyptian building site have gathering points for worshippers | todo | `SET_TOWN_CONGREGATION_POS` logs "not implemented" |
| Online the camera always starts on your own temple | todo | |
| The winning conditions offered for it start at 9,000 food, 4,000 wood, 2,100 belief and up to 8 towns taken | todo | see ../multiplayer_rules.md |
