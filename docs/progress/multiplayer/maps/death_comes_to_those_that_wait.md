# Death Comes To Those That Wait

A fan-made four-corner land by "Fat Omen", reworked from Four Gods: you start rich, with nine powerful dispensers in an
empty corner, but seven computer creatures begin together a short walk from your temple, among 22 neutral towns of
every tribe, most with wonders. Its start line warns "Make your moves fast now or never!".

**Players:** 4 (two computer gods, plus creatures for four players with no temple) · **Landscape:** `Data/Landscape/Multi_Player/DCTTTW.lnd` · **Script:** `Scripts/Playgrounds/Death Comes To Those That Wait.txt` · **Mode:** skirmish

**Progress: 5/33 done, 3 partial — 20%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#death-comes-to-those-that-wait-loading) (its "Death Comes
To Those That Wait" sections); this file covers the land as a player meets it. How skirmish is set up is in
[../skirmish.md](../skirmish.md); the computer gods are in
[../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

Most rows below are todo for one reason: openblack stops reading this script at line 83, on a misspelt command the game
skips, so only the dispensers, your home town and your temple are built.

## Where it comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A fan's land, signed "By Fat Omen" in its title, built on Four Gods: the same land size, the same corner temples and home towns for players three and four, and the same spot for the computer creatures | n/a | history of the file; neither file is among those the game installs |

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it by its file name, "Death Comes To Those That Wait" | partial | the debug menu lists it by its start message title "Death Comes To Those That Wait -- By Fat Omen" with the warning as a tooltip (`src/Level.cpp`); no skirmish box |
| The whole script loads, the game skipping lines it does not know | todo | openblack stops at line 83 (`CREATE_SPECIAL_TOWNVILLAGER`, a misspelling) and builds nothing after it |
| No story script runs on a skirmish land | todo | openblack starts the story's control script when a playground is the start land, and leaves story scripts running when one is opened from the debug menu |
| The script gives no start point, so the camera starts zoomed onto your own temple | todo | openblack points the camera at the land's corner (0, 0), off the land |
| Its start message is the title and "Make your moves fast now or never!" | todo | the start message commands are ignored |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land the size of Four Gods' (about 1,900 by 1,750 paces) but raised and reshaped: hilly almost everywhere, mountains reaching the highest possible height, and only an eighth at sea level | done | `Game::LoadLandscape` (the landscape loads before the script stops) |
| One climate over the whole land: a mild 12 degrees, no rain and no wind | todo | never reached: openblack stops at line 83 |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%) | done | `DayNightClock` (set before line 83) |
| Six land balance settings, three more than the game's own lands set | partial | openblack reads numbers 2, 4 and 7 only |
| Towns project half their normal influence and gods their full influence | done | `InfluenceSystem` |
| The music follows the nearest town's tribe, here every one of the game's nine tribes | partial | `src/Audio/GameMusic.cpp`; only your Aztec town exists in openblack |

## Your start (player one)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Your temple in a corner, beside an Aztec home town of nine houses, a graveyard, a crèche, a storehouse, a workshop, two fields and about 14 villagers | done | built before openblack stops |
| A vast store: 5,000,000 food and 5,000,000 wood in the storehouse and 1,275,000 more wood in the workshop | done | `AbodeArchetype` keeps the amounts |
| Your town offers stronger heal, teleport, stronger food, stronger water, creature itchiness and wood | todo | village centre miracles are not offered (`CREATE_NEW_TOWN_SPELL`); see ../../miracles/ |
| Nine Indian miracle dispensers belonging to your town but standing in the empty far corner where Four Gods' player two lived: the strongest beam explosion, lightning, storm and fire, flying and ground flocks, stronger heal, nature and stronger food | todo | `CREATE_SPELL_DISPENSER` is empty |
| 119 special villagers meant for every town are written with a misspelt command, so the game makes none of them | todo | openblack stops loading at the first of them instead of skipping it |

## The creatures and computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Players three and four are computer gods with Celtic towns and temples in their corners, as in Four Gods | todo | never reached: openblack stops at line 83 |
| Player three has a tortoise and player four a zebra from the generic computer creature mind | todo | never reached |
| Players two, five, six and seven get a horse each from the same mind but no temple, no town and no computer god | todo | never reached; how the game drives a creature whose player has no god is not determined |
| A fifth horse is given to "player eight", a name the game does not have (it knows players one to seven and neutral) | todo | what the game does with it is not determined; openblack has no player eight either |
| All seven creatures start on one spot about 500 paces from your temple: the danger the title warns of | todo | never reached |
| Each creature is sized to match your own | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is ignored |
| Belief in players two, three and four is capped in every town (at 1, or at a half in one Egyptian town), yours not | todo | `SET_TOWN_BELIEF_CAP` logs "not implemented" |

## Neutral towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 22 neutral towns of all nine tribes (Norse, Greek, Indian, Japanese, African, Celtic, Egyptian, Tibetan, Aztec), twelve of them with a wonder | todo | never reached |
| Two towns share the number 15 (a Greek town and the Tibetan town in the middle) | todo | what the game does with the repeated number is not determined; never reached in openblack |
| The Tibetan town in the middle has a graveyard, a football pitch, 21 houses, about 18 people, 20,000 food and 20,000 wood, and twelve more buildings planned (the Greek town given the same number has its own wonder) | todo | never reached; its pitch never plays: football is always off in multiplayer ([../../town/football.md](../../town/football.md#the-add-on-and-its-switch)) |
| The towns offer 102 miracles between them, from lightning and shields to the strongest storms and explosions | todo | never reached; town miracles are not offered either |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2,163 trees in 40 forests, nine big forests and 86 fireflies | todo | never reached; only 35 of the fireflies stand on one of the map's trees, the rest are removed at the first nightfall and the land tops up to 50; its last reward table is Land 2's; see [../../nature/fireflies.md](../../nature/fireflies.md) |
| 1,219 animals in 159 flocks, among them 15 tigers, 9 tortoises, 2 lions, bats and swallows as well as farm animals, wolves and birds | todo | never reached; animals are not made either |
| 61 fields and 40 fish farms | todo | only your town's two fields are made |
| 79 lanterns, twelve of them singing stone bases | todo | never reached |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |
