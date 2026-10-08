# Land 1 script

`Scripts/Land1.txt` builds the first land of the story: the player's starting town, the neutral villages around it, the creature's gate with its three totems, the Pied Piper's cave and the magic mushrooms, scattered rocks and fences, and its climates and streams. This file counts what the script makes and whether openblack makes it; the challenges played there are the story domain's ([../story/](../story/)), and the scripts that run them are in [challenge_scripts.md](challenge_scripts.md).

**Progress: 47/67 done, 5 partial — 74%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (`land1.fot`) |
| Loading the land starts the story's control script, which runs Land 1's challenges | partial | `Game::Run` starts the land control script whatever land was loaded; almost every challenge stops at functions that aren't there yet (see challenge_scripts.md) |
| The land is where a new game starts (from the map script) | partial | openblack starts on `Land1.txt` by default (`--start-level`) without reading the map script |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land1.lnd` | done | `Game::LoadLandscape` |
| Is land number 1 of the story | done | map script globals |
| Starts the camera looking at "1441.56,2081.76" | done | `StartCameraPos` |
| Scales the towns' influence by 1 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `WeatherSystem`; see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 6 towns (norse 2, celtic 2, japanese 1, aztec 1); owners: neutral 5, player one 1 | done | `TownArchetype` |
| Sets 6 towns' belief in a player | done | into the town's belief list |
| Leaves 4 towns uninhabitable (ruins and empty villages) | todo | logs "not implemented" |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 houses (style A) (Norse 4, Celtic 3, Aztec 3, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 7 houses (style C) (Aztec 4, Norse 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style E) (Norse 2, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style F) (Norse 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style D) (Aztec 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style B) (Aztec 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 graveyards (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 storehouses (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 crèches (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 village centres (Norse 1, Aztec 1) | partial | made as buildings; their worship share ignored |
| 6 planned buildings for towns to build later (houses (style A) 3, wonders 2, houses (style D) 1) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 26 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 26 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 2 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 shepherd living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1395 trees and bushes (Bush 378, ConiferA 206, PalmB 194, Conifer 116, Pine 113, Palm 108, Birch 77, Beech 73, Oak 38, BushB 36, PalmA 16, OakA 11, Olive 10, PalmC 10, BushA 6, Burnt 3) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 3 dead trees | partial | `DeadTreeArchetype`; their lean ignored |
| 3 big forest models | done | `BigForestArchetype` |
| 9 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Fat Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Pilar3 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 17 patches of mist | done | `MistArchetype` |
| Sets 42 firefly miracle reward chances | todo | empty; heal 77%, fireball, lightning, forest, food, wood and water about 4% each; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 19 fields (Wheat 18, Corn 1) | done | `FieldArchetype` |
| 13 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 4 pots (MagicWood 4) | done | `PotArchetype` |
| 47 drinking places | todo | `CREATE_DRINK_WAYPOINT` is empty |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 40 × dove | todo | `CREATE_NEW_ANIMAL` is empty |
| 22 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 14 × swallow | todo | `CREATE_NEW_ANIMAL` is empty |
| 12 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 10 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 7 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 6 × tortoise | todo | `CREATE_NEW_ANIMAL` is empty |
| 5 × bat | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 flocks | todo | `CREATE_FLOCK` is empty |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 132 × rocks and boulders (Rock 70, Boulder3Lime 20, Boulder1Lime 19, LongrockLimestone 6, RockLimestone 5, SharprockLimestone 5, Boulder2Lime 5, Boulder1Sand 1, Boulder3Chalk 1) | done | `MobileStaticArchetype` |
| 102 × fences (CeltFenceTall 64, CeltFenceShort 38) | done | `MobileStaticArchetype` |
| 4 × toys (ToyDie 3, ToyCuddly 1) | done | `MobileStaticArchetype` |
| 3 × gate totems (GateTotemBlank 1, GateTotemTiger 1, GateTotemApe 1) | done | `MobileStaticArchetype` |
| 36 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` |
| 9 × Toadstool (can be picked up) | done | `MobileObjectArchetype` |
| 4 × EgyptBarrel (can be picked up) | done | `MobileObjectArchetype` |
| 2 × Champi (can be picked up) | done | `MobileObjectArchetype` |
| 2 × Piper Cave Entrance (animated) | done | `AnimatedStaticArchetype` |
| 1 × Norse Gate (animated) | done | `AnimatedStaticArchetype` |
| 1 × Gate Stone Plinth (animated) | done | `AnimatedStaticArchetype` |
| 12 lanterns (StreetLantern 8, CountryLantern 4) | done | `StreetLanternArchetype` |
| 2 bonfires | partial | a static bonfire; one number ignored |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 streams through 187 points | done | `StreamSegmentArchetype` |
| 3 arenas | todo | `CREATE_ARENA` is empty |
