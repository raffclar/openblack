# Land 2 script

`Scripts/Land2.txt` builds the second land: the largest, with fifteen towns, the rival god's temple and worship site, a second rival's town, the Greek and Indian villages, many fields and planned buildings. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 40/69 done, 4 partial — 61%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (`land2.fot`) |
| The story moves on to this land when Land 1 is finished (its control script loads it) | todo | the land-loading function does nothing (`LoadMap` in `CHLApi.cpp`); openblack can only start on it with `--start-level Land2.txt` or the debug land menu |
| Starting on this land runs this land's challenges | todo | the land control script always begins with Land 1's challenges, whatever land is loaded |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land2.lnd` | done | `Game::LoadLandscape` |
| Is land number 2 of the story | done | map script globals |
| Starts the camera looking at "3393.78,3261.76" | done | `StartCameraPos` |
| Scales the towns' influence by 1 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |
| Sets 6 land balance numbers (0 = 2, 1 = 2, 2 = 2, 3 = 2, 4 = 1.5, 5 = 2) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock` |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `WeatherSystem`; see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 15 towns (celtic 5, norse 4, indian 4, greek 2); owners: neutral 9, player three 3, player two 2, player one 1 | done | `TownArchetype`; the quests' towns: id 2 is Greek and Khazar's at the start, id 9 Greek, id 12 Indian (the land's control script calls them Norse in its comments; the map script decides) |
| Sets 11 towns' belief in a player | done | into the town's belief list |
| Caps belief 20 times | todo | logs "not implemented" |
| Leaves 4 towns uninhabitable (ruins and empty villages) | todo | logs "not implemented" |
| Gives 1 towns a gathering place | todo | logs "not implemented" |
| Changes towns' desires 8 times (abodes 4, civic buildings 4) | todo | empty |
| Gives towns 22 miracles to offer at their village centres (heal 3, fire 2, food 2, teleport 2, physical shield 2, lightning bolt 2, shield 2, nature 1, wood 1, creature spell itchy 1, water 1, creature spell angry 1, storm 1, creature spell strong 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 25 houses (style A) (Indian 14, Celtic 8, Greek 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 11 houses (style F) (Indian 6, Celtic 2, Norse 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 storehouses (Indian 4, Celtic 3, Greek 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 9 houses (style C) (Celtic 4, Norse 2, Indian 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 8 houses (style B) (Norse 3, Greek 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style D) (Greek 2, Celtic 2, Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 crèches (Celtic 2, Norse 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style E) (Norse 2, Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 graveyards (Greek 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 workshops (Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 village centres (Indian 4, Celtic 3, Greek 2, Norse 1) | partial | made as buildings; their worship share ignored |
| 89 planned buildings for towns to build later (houses (style A) 32, houses (style F) 18, houses (style C) 8, houses (style E) 7, workshops 6, crèches 6, houses (style B) 4, wonders 4, graveyards 2, houses (style D) 2) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |
| 2 temples of rival gods (player two 1, player three 1) | done | `CitadelArchetype` |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` |
| 2 worship sites (norse for player two 1, celtic for player three 1) | todo | `CREATE_WORSHIP_SITE` is empty |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 115 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 77 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 13 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 11 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 6 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 4 town leaders living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 villagers placed in towns without a house (foresters 1) | todo | `CREATE_TOWN_VILLAGER` logs "not implemented" |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 921 trees and bushes (ConiferA 264, Bush 187, Pine 178, Conifer 104, Oak 43, BushB 43, Beech 29, Birch 21, OakA 18, Palm 10, BushA 6, PalmB 5, PalmC 5, PalmA 4, Olive 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 10 big forest models | done | `BigForestArchetype` |
| 7 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Pilar3 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Pier" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| Sets 42 firefly miracle reward chances | todo | empty; eleven common miracles about 8% each, flocks and nine creature miracles under 1%; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 60 fields (Wheat 34, WheatWithFence 22, Corn 4) | done | `FieldArchetype` |
| 30 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 5 pots (MagicWood 5) | done | `PotArchetype` |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 111 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 79 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 50 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 47 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 41 × crow | todo | `CREATE_NEW_ANIMAL` is empty |
| 33 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 19 × dove | todo | `CREATE_NEW_ANIMAL` is empty |
| 18 × pigeon | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 × wolf | todo | `CREATE_NEW_ANIMAL` is empty |
| 3 × tiger | todo | `CREATE_NEW_ANIMAL` is empty |
| 3 × tortoise | todo | `CREATE_NEW_ANIMAL` is empty |
| 2 × lion | todo | `CREATE_NEW_ANIMAL` is empty |
| 45 flocks | todo | `CREATE_FLOCK` is empty |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 378 × rocks and boulders (RockLimestone 239, Boulder3Lime 32, Boulder2Lime 31, Boulder1Lime 28, SharprockLimestone 15, SquarerockLimestone 11, FlatrockLimestone 8, LongrockLimestone 7, Boulder3Sand 4, Boulder2Sand 2, LongrockSandstone 1) | done | `MobileStaticArchetype` |
| 265 × fences (CeltFenceTall 181, CeltFenceShort 84) | done | `MobileStaticArchetype` |
| 39 lanterns (StreetLantern 36, CountryLantern 3) | done | `StreetLanternArchetype` |
| 1 bonfires | partial | a static bonfire; one number ignored |

## Rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 2 computer players (player two, player three) | partial | the players are made; their thinking isn't (see ../multiplayer/) |
