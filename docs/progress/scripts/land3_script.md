# Land 3 script

`Scripts/Land3.txt` builds the third land, where the player's creature is taken from them: the rival god's two towns, their temple and worship sites, the creature's prison pillars, the weeping stones and a wood dispenser. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 46/70 done, 3 partial — 68%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (`land3.fot`) |
| The story moves on to this land when Land 2 is finished | todo | the land-loading function does nothing |
| Starting on this land runs this land's challenges | todo | the land control script always begins with Land 1's |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land3.lnd` | done | `Game::LoadLandscape` |
| Is land number 3 of the story | done | map script globals |
| Starts the camera looking at "3237.21,3157.85" | done | `StartCameraPos` |
| Scales the towns' influence by 0.5 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |
| Sets 3 land balance numbers (4 = 1.25, 5 = 1.21951, 6 = 2.03252) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock` |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `WeatherSystem`; see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 6 towns (celtic 2, tibetan 1, egyptian 1, indian 1, japanese 1); owners: neutral 3, player two 2, player one 1 | done | `TownArchetype` |
| Sets 6 towns' belief in a player | done | into the town's belief list |
| Caps belief 4 times | todo | logs "not implemented" |
| Leaves 1 towns uninhabitable (ruins and empty villages) | todo | logs "not implemented" |
| Gives 2 towns a gathering place | todo | logs "not implemented" |
| Gives towns 23 miracles to offer at their village centres (fire 3, lightning bolt 3, heal 2, food 2, storm 2, water 2, teleport 2, shield 2, physical shield 2, fire pu1 1, nature 1, water pu1 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 houses (style F) (Indian 5, Egyptian 3, Japanese 2, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 houses (style A) (Egyptian 4, Indian 2, Japanese 2, Tibetan 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 houses (style D) (Egyptian 5, Tibetan 2, Japanese 2, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style C) (Tibetan 3, Egyptian 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 storehouses (Tibetan 1, Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style B) (Japanese 3, Egyptian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 crèches (Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style E) (Tibetan 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 village centres (Tibetan 1, Egyptian 1, Indian 1, Japanese 1) | partial | made as buildings; their worship share ignored |
| 70 planned buildings for towns to build later (houses (style F) 21, houses (style B) 11, houses (style C) 11, houses (style A) 9, houses (style D) 9, houses (style E) 4, crèches 2, village centres 1, workshops 1, wonders 1) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |
| 1 temples of rival gods (player two 1) | done | `CitadelArchetype` |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` |
| 2 worship sites (egyptian for player two 1, tibetan for player two 1) | todo | `CREATE_WORSHIP_SITE` is empty |
| 1 miracle dispensers (wood 1) | todo | `CREATE_SPELL_DISPENSER` is empty |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 68 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 46 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 10 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 9 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 7 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 2 town leaders living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 villagers placed in towns without a house (housewives 1) | todo | `CREATE_TOWN_VILLAGER` logs "not implemented" |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1397 trees and bushes (PalmB 454, Palm 279, BushB 132, Conifer 76, PalmA 71, PalmC 69, Beech 54, Cedar 52, Bush 39, HedgeA 39, Pine 38, BushA 33, Olive 26, Hedge 15, ConiferA 8, Copse 6, Cypress 5, Birch 1) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 1 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 16 big forest models | done | `BigForestArchetype` |
| 20 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 6 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Prison Pillar 1" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Prison Pillar 2" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Prison Pillar 3" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Tibetan Large Pillar Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 70 patches of mist | done | `MistArchetype` |
| Sets 42 firefly miracle reward chances | todo | empty; eleven common miracles about 9% each, flocks under 1%, no creature miracles; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 18 fields (Wheat 14, WheatWithFence 4) | done | `FieldArchetype` |
| 7 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 2 pots (MagicWood 2) | done | `PotArchetype` |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 27 × dove | todo | `CREATE_NEW_ANIMAL` is empty |
| 9 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 8 × tortoise | todo | `CREATE_NEW_ANIMAL` is empty |
| 8 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 6 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 6 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 5 × wolf | todo | `CREATE_NEW_ANIMAL` is empty |
| 15 flocks | todo | `CREATE_FLOCK` is empty |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 106 × rocks and boulders (Rock 83, Boulder1Sand 7, Boulder3Lime 3, RockLimestone 3, Boulder2Lime 3, SharprockLimestone 2, Boulder1Lime 2, FlatrockLimestone 2, LongrockLimestone 1) | done | `MobileStaticArchetype` |
| 87 × fences (CeltFenceShort 49, CeltFenceTall 38) | done | `MobileStaticArchetype` |
| 8 × WeepingStoneReward | done | `MobileStaticArchetype` |
| 13 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` |
| 7 × Champi (can be picked up) | done | `MobileObjectArchetype` |
| 6 × Toadstool (can be picked up) | done | `MobileObjectArchetype` |
| 1 lanterns (StreetLantern 1) | done | `StreetLanternArchetype` |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 streams through 18 points | done | `StreamSegmentArchetype` |

## Rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 1 computer players (player two) | partial | the players are made; their thinking isn't (see ../multiplayer/) |
