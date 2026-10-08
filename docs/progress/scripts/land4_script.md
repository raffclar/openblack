# Land 4 script

`Scripts/Land4.txt` builds the fourth land, the first land again under a rival god's curse: its towns, the Aztec temple feature, the volcanic rocks and the Norse gate. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 39/58 done, 2 partial — 69%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (`land4.fot`) |
| The story moves on to this land when Land 3 is finished | todo | the land-loading function does nothing |
| Starting on this land runs this land's challenges | todo | the land control script always begins with Land 1's |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land4.lnd` | done | `Game::LoadLandscape` |
| Is land number 4 of the story | done | map script globals |
| Starts the camera looking at "1810.04,2611.21" | done | `StartCameraPos` |
| Scales the towns' influence by 0.8 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |
| Creates 5 climates and sets rain, temperature and wind 5, 5 and 5 times | done | `WeatherSystem`; see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 8 towns (norse 3, aztec 2, japanese 1, celtic 1, indian 1); owners: neutral 7, player one 1 | done | `TownArchetype` |
| Sets 9 towns' belief in a player | done | into the town's belief list |
| Leaves 3 towns uninhabitable (ruins and empty villages) | todo | logs "not implemented" |
| Gives towns 11 miracles to offer at their village centres (heal pu1 1, food 1, shield 1, wood 1, water pu1 1, fire 1, lightning bolt pu2 1, creature spell angry 1, teleport 1, nature 1, creature spell weak 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 7 houses (style C) (Aztec 6, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style A) (Norse 2, Aztec 2, Celtic 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 houses (style B) (Aztec 3, Japanese 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style D) (Aztec 2, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style F) (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style E) (Celtic 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 crèches (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 graveyards (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 village centres (Aztec 1, Japanese 1) | partial | made as buildings; their worship share ignored |
| 25 planned buildings for towns to build later (houses (style A) 7, houses (style C) 5, houses (style F) 3, houses (style D) 3, workshops 2, houses (style B) 2, houses (style E) 1, village centres 1, graveyards 1) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 24 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 16 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 7 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 fisherman living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 shepherd living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 town leader living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 799 trees and bushes (Bush 315, Conifer 87, OakA 75, ConiferA 70, Oak 60, Birch 59, Olive 29, Beech 26, BushB 18, Hedge 15, Pine 8, BushA 7, Palm 7, PalmB 7, PalmA 6, PalmC 6, Cedar 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 3 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 9 big forest models | done | `BigForestArchetype` |
| 8 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Spikey Pilar Volcanic" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Aztec Suntemple Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 14 patches of mist | done | `MistArchetype` |
| Sets 42 firefly miracle reward chances | todo | empty; eleven common miracles and stronger heal and food about 7% each, rarer stronger ones and nine creature miracles; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 fields (Wheat 12, Cereal 3, Corn 1) | done | `FieldArchetype` |
| 6 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 8 pots (MagicWood 8) | done | `PotArchetype` |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 29 × crow | todo | `CREATE_NEW_ANIMAL` is empty |
| 26 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 26 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 20 × bat | todo | `CREATE_NEW_ANIMAL` is empty |
| 7 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 5 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 5 × dove | todo | `CREATE_NEW_ANIMAL` is empty |
| 15 flocks | todo | `CREATE_FLOCK` is empty |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 118 × rocks and boulders (Boulder3Lime 17, LongrockVolcanic 12, Boulder2Sand 11, LongrockLimestone 10, Boulder2Lime 10, Boulder1Lime 8, Boulder1Sand 7, SharprockLimestone 5, Boulder1Volcanic 5, LongrockSandstone 4, FlatrockSandstone 4, Boulder3Volcanic 4, Boulder3Sand 3, RockLimestone 3, SquarerockLimestone 3, RockSandstone 3, SharprockSandstone 2, FlatrockLimestone 2, RockVolcanic 2, SharprockVolcanic 1, Boulder2Volcanic 1, SquarerockSandstone 1) | done | `MobileStaticArchetype` |
| 89 × fences (CeltFenceShort 62, CeltFenceTall 27) | done | `MobileStaticArchetype` |
| 1 × EgyptBarrel (can be picked up) | done | `MobileObjectArchetype` |
| 1 × Norse Gate (animated) | done | `AnimatedStaticArchetype` |
| 16 lanterns (StreetLantern 8, CountryLantern 8) | done | `StreetLanternArchetype` |
| 2 bonfires | partial | a static bonfire; one number ignored |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 23 streams through 388 points | done | `StreamSegmentArchetype` |
| 1 arenas | todo | `CREATE_ARENA` is empty |
