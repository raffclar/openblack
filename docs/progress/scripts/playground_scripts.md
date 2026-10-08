# Playground scripts

The seven land scripts in `Scripts/Playgrounds/` are the skirmish lands: a player against one to three rival gods (the computer players) on lands made for two, three or four gods, plus openblack's own `construct` test land (not a game file; see [../multiplayer/maps/construct.md](../multiplayer/maps/construct.md)). Each section counts what its script builds and whether openblack builds it. Rival gods' play is the multiplayer and skirmish domain's ([../multiplayer/](../multiplayer/)); openblack opens these lands only through the debug land menu or `--start-level`, and the story's control script still starts on them.

**Progress: 197/364 done, 55 partial — 62%**

## Two Gods: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/TwoGods.txt`, 1,000 lines, landscape `MPM_2P_1.lnd`, two players) loads from first line to last | done | every line is a known command with the right arguments |
| Its footpath file `twogods.fot` is loaded with it | done | `Game::LoadMap` |
| It can be chosen from the game's skirmish menu, with its title and description | partial | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Two Gods: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `MPM_2P_1.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | map script globals |
| Starts the camera looking at "2185.72,2315.78" | done | `StartCameraPos` |
| Scales the towns' influence by 0.4 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |
| Sets 3 land balance numbers (4 = 2.75, 5 = 4, 6 = 20) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock` |
| Titles the land "Two Gods" with 1 description line(s) | partial | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |

## Two Gods: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 10 towns (norse 3, celtic 2, indian 2, african 1, greek 1, egyptian 1); owners: neutral 6, player one 2, player two 2 | done | `TownArchetype` |
| Sets 9 towns' belief in a player | done | into the town's belief list |
| Caps belief 10 times | todo | logs "not implemented" |
| Gives towns 39 miracles to offer at their village centres (storm 6, fire 4, storm pu1 4, flying flock 3, ground flock 3, heal 2, teleport 2, water 2, water pu1 2, nature 2, physical shield 2, storm pu2 2, lightning bolt 2, wood 2, shield 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Two Gods: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 14 houses (style F) (Celtic 8, Norse 3, African 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 13 houses (style A) (Norse 7, Celtic 4, African 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 houses (style B) (Norse 5, Celtic 2, Greek 2, African 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 7 storehouses (Norse 3, Celtic 2, African 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style D) (Norse 2, Celtic 1, African 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 crèches (Celtic 2, African 1, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style E) (African 2, Celtic 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 workshops (Celtic 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style C) (Greek 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 9 village centres (Norse 3, Celtic 2, Indian 2, African 1, Greek 1) | partial | made as buildings; their worship share ignored |
| 2 temples of rival gods (player one 1, player two 1) | done | `CitadelArchetype` |
| 4 worship sites (indian for player one 1, celtic for player one 1, indian for player two 1, celtic for player two 1) | todo | `CREATE_WORSHIP_SITE` is empty |
| 1 miracle dispensers (beam explosion 1) | todo | `CREATE_SPELL_DISPENSER` is empty |
| 12 scaffolds | todo | `CREATE_SCAFFOLD` is empty |

## Two Gods: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 54 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 47 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 25 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 7 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 6 town leaders living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 5 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 3 villagers placed in towns without a house (town leaders 1, farmers 1, foresters 1) | todo | `CREATE_TOWN_VILLAGER` logs "not implemented" |

## Two Gods: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 308 trees and bushes (ConiferA 141, PalmB 57, Bush 43, Palm 25, Birch 22, CypressA 9, PalmA 7, CopseA 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 4 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 3 big forest models | done | `BigForestArchetype` |
| Sets 42 firefly miracle reward chances | todo | empty; shared with Four Gods: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table) |

## Two Gods: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 24 fields (Wheat 13, Cereal 7, Corn 4) | done | `FieldArchetype` |
| 6 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 3 pots (MagicWood 3) | done | `PotArchetype` |

## Two Gods: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 51 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 20 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 × pigeon | todo | `CREATE_NEW_ANIMAL` is empty |
| 8 × wolf | todo | `CREATE_NEW_ANIMAL` is empty |
| 8 × crow | todo | `CREATE_NEW_ANIMAL` is empty |
| 22 flocks | todo | `CREATE_FLOCK` is empty |

## Two Gods: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 39 × rocks and boulders (Rock 39) | done | `MobileStaticArchetype` |
| 12 × Champi (can be picked up) | done | `MobileObjectArchetype` |
| 31 lanterns (StreetLantern 31) | done | `StreetLanternArchetype` |

## Two Gods: miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 9 one-shot miracles lying on the land (water 4, flying flock 4, beam explosion 1) | todo | `CREATE_ONE_SHOT_SPELL_PU` is empty |

## Two Gods: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 1 computer players (player two) | partial | the players are made; their thinking isn't (see ../multiplayer/; the switch turns on each one's computer god mind: [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md)) |
| 1 rival creatures from mind files (ComputerControlledCreature as Leopard 1) | partial | `CreatureArchetype` with the mind loaded; see creature_mind_scripts.md |
| Sets 1 rival creatures' liking for the player | todo | empty |

## Three Gods: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/ThreeGods.txt`, 2,063 lines, landscape `MPM_3P_1.lnd`, three players) loads from first line to last | done | every line is a known command with the right arguments |
| Its footpath file `threegods.fot` is loaded with it | done | `Game::LoadMap` |
| It can be chosen from the game's skirmish menu, with its title and description | partial | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Three Gods: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `MPM_3P_1.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | map script globals |
| Starts the camera looking at "2816.90,2740.75" | done | `StartCameraPos` |
| Scales the towns' influence by 0.4 | done | `InfluenceSystem` |
| Scales the players' influence by 0.7 | done | `InfluenceSystem` |
| Sets 3 land balance numbers (4 = 2.75, 5 = 4, 6 = 20) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1000 with 23% night and 17% dawn and dusk | done | `DayNightClock` |
| Titles the land "Three Gods" with 1 description line(s) | partial | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |
| Creates 3 climates and sets rain, temperature and wind 3, 3 and 3 times | done | `WeatherSystem`; see ../weather/ |
| Creates 2 storms that are there from the start | todo | the command is empty |

## Three Gods: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 13 towns (norse 4, japanese 4, celtic 4, tibetan 1); owners: neutral 10, player one 1, player two 1, player three 1 | done | `TownArchetype` |
| Sets 13 towns' belief in a player | done | into the town's belief list |
| Caps belief 26 times | todo | logs "not implemented" |
| Gives towns 42 miracles to offer at their village centres (lightning bolt 4, lightning bolt pu1 4, storm 4, physical shield 4, heal 3, teleport 3, water 3, shield 3, flying flock 3, creature spell freeze 3, creature spell weak 3, fire 3, beam explosion 1, storm pu2 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Three Gods: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 houses (style B) (Norse 6, Celtic 5, Japanese 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 16 houses (style F) (Norse 6, Japanese 6, Celtic 3, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 15 houses (style E) (Celtic 6, Japanese 4, Tibetan 3, Norse 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 15 houses (style D) (Japanese 6, Celtic 5, Norse 4) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 13 crèches (Norse 4, Japanese 4, Celtic 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 13 houses (style C) (Japanese 5, Tibetan 3, Norse 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 12 storehouses (Norse 4, Celtic 4, Japanese 3, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 houses (style A) (Norse 6, Celtic 2, Tibetan 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 workshops (Norse 1, Japanese 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 wonders (Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 graveyards (Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 13 village centres (Norse 4, Japanese 4, Celtic 4, Tibetan 1) | partial | made as buildings; their worship share ignored |
| 82 planned buildings for towns to build later (houses (style A) 17, graveyards 12, houses (style B) 11, houses (style E) 11, houses (style F) 9, houses (style C) 9, workshops 8, houses (style D) 3, storehouses 1, wonders 1) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |
| 3 temples of rival gods (player one 1, player two 1, player three 1) | done | `CitadelArchetype` |

## Three Gods: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 133 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 96 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 22 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 12 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 2 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 6 villagers placed in towns without a house (foresters 6) | todo | `CREATE_TOWN_VILLAGER` logs "not implemented" |

## Three Gods: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 684 trees and bushes (Beech 128, ConiferA 123, Palm 116, OakA 100, Birch 64, Olive 48, PalmA 43, Pine 36, Conifer 20, Cypress 6) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 19 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 6 big forest models | done | `BigForestArchetype` |
| 43 fireflies | todo | `CREATE_FIRE_FLY` is empty; every map also gets the nightly top-up: [../nature/fireflies.md](../nature/fireflies.md#where-they-come-from) |
| Sets 42 firefly miracle reward chances | todo | empty; every chance is zero, so its fireflies give nothing: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table) |

## Three Gods: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 82 fields (WheatWithFence 82) | done | `FieldArchetype` |
| 18 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 1 pots (MagicWood 1) | done | `PotArchetype` |

## Three Gods: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 192 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 54 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 42 × pigeon | todo | `CREATE_NEW_ANIMAL` is empty |
| 36 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 26 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 17 × crow | todo | `CREATE_NEW_ANIMAL` is empty |
| 7 × dove | todo | `CREATE_NEW_ANIMAL` is empty |
| 71 flocks | todo | `CREATE_FLOCK` is empty |

## Three Gods: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 35 × rocks and boulders (Boulder1Lime 13, Boulder2Lime 7, SharprockLimestone 5, FlatrockLimestone 3, SquarerockLimestone 3, RockLimestone 3, Boulder3Lime 1) | done | `MobileStaticArchetype` |
| 48 lanterns (StreetLantern 48) | done | `StreetLanternArchetype` |

## Three Gods: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 2 computer players (player two, player three) | partial | the players are made; their thinking isn't (see ../multiplayer/; the switch turns on each one's computer god mind: [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md)) |
| 2 rival creatures from mind files (COMPUTERCONTROLLEDCREATURE as Lion 1, COMPUTERCONTROLLEDCREATURE as Horse 1) | partial | `CreatureArchetype` with the mind loaded; see creature_mind_scripts.md |
| Sets 2 rival creatures' liking for the player | todo | empty |

## Four Gods: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/FourGods.txt`, 1,550 lines, landscape `MPM_4P_1.lnd`, four players) loads from first line to last | done | every line is a known command with the right arguments |
| Its footpath file `fourgods.fot` is loaded with it | done | `Game::LoadMap` |
| It can be chosen from the game's skirmish menu, with its title and description | partial | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Four Gods: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `MPM_4P_1.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | map script globals |
| Starts the camera looking at "0.00,0.00" | done | `StartCameraPos` |
| Scales the towns' influence by 0.5 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |
| Sets 3 land balance numbers (4 = 2, 5 = 2, 6 = 10) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock` |
| Titles the land "Four Gods" with 1 description line(s) | partial | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |
| Creates 1 climates and sets rain, temperature and wind 1, 1 and 1 times | done | `WeatherSystem`; see ../weather/ |

## Four Gods: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 13 towns (celtic 4, japanese 4, indian 4, tibetan 1); owners: neutral 9, player one 1, player two 1, player three 1, player four 1 | done | `TownArchetype` |
| Sets 4 towns' belief in a player | done | into the town's belief list |
| Caps belief 39 times | todo | logs "not implemented" |
| Gives towns 44 miracles to offer at their village centres (lightning bolt 5, heal 4, teleport 4, food 4, water 4, creature spell freeze 4, shield 4, physical shield 4, creature spell strong 4, creature spell invisible 4, lightning bolt pu2 1, beam explosion 1, beam explosion pu2 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Four Gods: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 38 houses (style A) (Indian 17, Celtic 11, Japanese 7, Tibetan 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 25 houses (style F) (Indian 11, Celtic 6, Japanese 6, Tibetan 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 19 houses (style E) (Japanese 8, Celtic 5, Indian 4, Tibetan 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 18 houses (style B) (Celtic 9, Japanese 6, Tibetan 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 13 storehouses (Celtic 4, Japanese 4, Indian 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 13 crèches (Celtic 4, Japanese 4, Indian 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 11 houses (style D) (Japanese 6, Tibetan 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 9 houses (style C) (Celtic 4, Japanese 3, Tibetan 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 9 workshops (Celtic 4, Japanese 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 graveyards (Celtic 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 wonders (Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 13 village centres (Celtic 4, Japanese 4, Indian 4, Tibetan 1) | partial | made as buildings; their worship share ignored |
| 4 temples of rival gods (player one 1, player two 1, player three 1, player four 1) | done | `CitadelArchetype` |

## Four Gods: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 55 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 51 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 39 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 16 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 15 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 12 town leaders living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 trader living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |

## Four Gods: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 664 trees and bushes (ConiferA 320, PalmB 200, Palm 77, PalmA 36, Conifer 17, Pine 14) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 21 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 9 big forest models | done | `BigForestArchetype` |
| Sets 42 firefly miracle reward chances | todo | empty; shared with Two Gods: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table) |

## Four Gods: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 30 fields (Wheat 14, Corn 10, Cereal 6) | done | `FieldArchetype` |
| 16 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |

## Four Gods: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 50 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 50 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 34 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 24 × pigeon | todo | `CREATE_NEW_ANIMAL` is empty |
| 24 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 20 × dove | todo | `CREATE_NEW_ANIMAL` is empty |
| 16 × wolf | todo | `CREATE_NEW_ANIMAL` is empty |
| 28 flocks | todo | `CREATE_FLOCK` is empty |

## Four Gods: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 3 computer players (player two, player three, player four) | partial | the players are made; their thinking isn't (see ../multiplayer/; the switch turns on each one's computer god mind: [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md)) |
| 3 rival creatures from mind files (ComputerControlledCreature as Horse 1, ComputerControlledCreature as Tortoise 1, ComputerControlledCreature as Zebra 1) | partial | `CreatureArchetype` with the mind loaded; see creature_mind_scripts.md |
| Sets 3 rival creatures' liking for the player | todo | empty |

## Firestorm: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/Firestorm.txt`, 1,540 lines, landscape `mpm_3p_2.lnd`, three players) loads from first line to last | done | every line is a known command with the right arguments |
| It has no footpath file, as in the game | done | nothing to load |
| It can be chosen from the game's skirmish menu | partial | listed in the debug land menu only |

## Firestorm: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `mpm_3p_2.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | map script globals |
| Starts the camera looking at "1441.56,2081.76" | done | `StartCameraPos` |
| Scales the towns' influence by 0.01 | done | `InfluenceSystem` |
| Scales the players' influence by 0.8 | done | `InfluenceSystem` |
| Sets 3 land balance numbers (4 = 2.5, 5 = 3.01471, 6 = 5) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock` |
| Creates 1 climates and sets rain, temperature and wind 1, 1 and 1 times | done | `WeatherSystem`; see ../weather/ |

## Firestorm: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 22 towns (celtic 19, aztec 1, japanese 1, norse 1); owners: neutral 19, player one 1, player two 1, player three 1 | done | `TownArchetype` |
| Sets 22 towns' belief in a player | done | into the town's belief list |
| Gives 21 towns a gathering place | todo | logs "not implemented" |
| Gives towns 28 miracles to offer at their village centres (shield 4, physical shield 4, water 4, fire 3, fire pu2 3, creature spell invisible 3, creature spell itchy 3, heal 1, heal pu1 1, creature spell freeze 1, creature spell strong 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Firestorm: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 15 houses (style A) (Celtic 7, Aztec 4, Japanese 4) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 14 houses (style B) (Aztec 4, Norse 4, Celtic 3, Japanese 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 12 houses (style D) (Aztec 5, Japanese 4, Celtic 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 11 houses (style C) (Norse 6, Celtic 2, Aztec 2, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 9 houses (style E) (Japanese 4, Aztec 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 storehouses (Celtic 1, Aztec 1, Japanese 1, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style F) (Japanese 2, Celtic 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 wonders (Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 village centres (Celtic 1, Aztec 1, Japanese 1, Norse 1) | partial | made as buildings; their worship share ignored |
| 3 temples of rival gods (player one 1, player two 1, player three 1) | done | `CitadelArchetype` |
| 33 miracle dispensers (fire 7, heal 5, fire pu1 3, shield 3, creature spell big 2, creature spell weak 2, storm 2, lightning bolt 2, teleport 2, creature spell angry 2, creature spell invisible 1, creature spell compassion 1, creature spell strong 1) | todo | `CREATE_SPELL_DISPENSER` is empty |

## Firestorm: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 82 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 31 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 24 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 16 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 15 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 3 town leaders living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |

## Firestorm: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 304 trees and bushes (OakA 65, Oak 57, Bush 43, Pine 40, PalmC 23, Palm 18, Birch 14, PalmA 12, Olive 10, Cedar 8, PalmB 4, Beech 4, BushB 3, BushA 3) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 10 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 9 big forest models | done | `BigForestArchetype` |
| 49 × feature "Egyptian Needle Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 24 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Pilar3 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 5 patches of mist | done | `MistArchetype` |
| Sets 44 firefly miracle reward chances | todo | empty; every chance is zero, so its fireflies give nothing: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table) |

## Firestorm: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 fields (Wheat 6, Corn 6, WheatWithFence 4) | done | `FieldArchetype` |
| 18 pots (MagicWood 18) | done | `PotArchetype` |
| 28 drinking places | todo | `CREATE_DRINK_WAYPOINT` is empty |

## Firestorm: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 45 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 25 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 12 flocks | todo | `CREATE_FLOCK` is empty |

## Firestorm: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 149 × rocks and boulders (Boulder2Lime 36, Boulder3Lime 28, Boulder1Lime 27, FlatrockLimestone 19, LongrockLimestone 14, SquarerockLimestone 6, RockLimestone 6, Rock 5, Boulder2Volcanic 3, SharprockLimestone 3, Boulder1Chalk 2) | done | `MobileStaticArchetype` |
| 3 × toys (ToyCuddly 3) | done | `MobileStaticArchetype` |
| 27 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` |
| 6 × Toadstool (can be picked up) | done | `MobileObjectArchetype` |
| 32 lanterns (CountryLantern 18, StreetLantern 14) | done | `StreetLanternArchetype` |
| 1 street light | done | `StreetLanternArchetype` |

## Firestorm: water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 4 streams through 71 points | done | `StreamSegmentArchetype` |

## Firestorm: miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 225 one-shot miracles lying on the land (fire 49, heal 32, fire pu1 21, shield 18, lightning bolt 14, storm 14, creature spell weak 14, teleport 14, creature spell big 14, creature spell angry 14, creature spell invisible 7, creature spell strong 7, creature spell compassion 7) | todo | `CREATE_ONE_SHOT_SPELL_PU` is empty |

## Island Wars: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/Island Wars.txt`, 1,145 lines, landscape `mpm_4p_2.lnd`, four players) loads from first line to last | done | every line is a known command with the right arguments |
| It has no footpath file, as in the game | done | nothing to load |
| It can be chosen from the game's skirmish menu | partial | listed in the debug land menu only |

## Island Wars: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `mpm_4p_2.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | map script globals |
| Starts the camera looking at "1441.56,2081.76" | done | `StartCameraPos` |
| Scales the towns' influence by 0.2 | done | `InfluenceSystem` |
| Scales the players' influence by 1.2 | done | `InfluenceSystem` |
| Sets 3 land balance numbers (4 = 2.5, 5 = 3.01471, 6 = 5) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock` |

## Island Wars: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 11 towns (japanese 4, indian 4, greek 3); owners: neutral 7, player one 1, player two 1, player three 1, player four 1 | done | `TownArchetype` |
| Sets 11 towns' belief in a player | done | into the town's belief list |
| Gives 7 towns a gathering place | todo | logs "not implemented" |
| Gives towns 66 miracles to offer at their village centres (fire 7, water 6, heal 5, physical shield 5, lightning bolt 5, teleport 4, shield 4, creature spell compassion 4, creature spell freeze 4, creature spell invisible 4, creature spell itchy 4, fire pu1 2, food 2, wood 2, water pu1 2, fire pu2 1, beam explosion 1, beam explosion pu2 1, heal pu1 1, storm 1, storm pu2 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Island Wars: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 31 houses (style B) (Indian 20, Japanese 7, Greek 4) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 14 houses (style A) (Japanese 8, Greek 6) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 7 houses (style F) (Japanese 5, Greek 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style C) (Japanese 4, Greek 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 storehouses (Japanese 4, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 workshops (Japanese 4, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 houses (style D) (Japanese 4, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 houses (style E) (Japanese 4, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 crèches (Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 graveyards (Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 11 village centres (Japanese 4, Indian 4, Greek 3) | partial | made as buildings; their worship share ignored |
| 4 temples of rival gods (player one 1, player two 1, player three 1, player four 1) | done | `CitadelArchetype` |

## Island Wars: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 80 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 28 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 24 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 13 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 9 town leaders living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 5 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |

## Island Wars: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 458 trees and bushes (Bush 65, ConiferA 64, OakA 54, PalmC 47, Beech 36, Conifer 35, PalmA 34, Olive 34, Palm 30, Oak 28, Birch 20, Pine 10, PalmB 1) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 13 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 11 big forest models | done | `BigForestArchetype` |
| Sets 44 firefly miracle reward chances | todo | empty; every chance is zero, so its fireflies give nothing: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table) |

## Island Wars: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 18 fields (Wheat 15, Corn 2, Cereal 1) | done | `FieldArchetype` |
| 24 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 6 pots (MagicWood 6) | done | `PotArchetype` |

## Island Wars: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 39 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 28 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 8 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 18 flocks | todo | `CREATE_FLOCK` is empty |

## Island Wars: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 48 × rocks and boulders (Rock 48) | done | `MobileStaticArchetype` |
| 17 lanterns (StreetLantern 17) | done | `StreetLanternArchetype` |
| 4 bonfires | partial | a static bonfire; one number ignored |

## Island Wars: water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1 influence ring | todo | empty |

## Island Wars: miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 8 one-shot miracles lying on the land (storm 4, creature spell strong 2, creature spell freeze 2) | todo | `CREATE_ONE_SHOT_SPELL_PU` is empty |

## Island Wars: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 3 computer players (player two, player three, player four) | partial | the players are made; their thinking isn't (see ../multiplayer/; the switch turns on each one's computer god mind: [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md)) |
| 3 rival creatures from mind files (NemesisCreature as Leopard 1, NemesisCreature as Gorilla 1, NemesisCreature as BrownBear 1) | partial | `CreatureArchetype` with the mind loaded; see creature_mind_scripts.md |
| Sets 3 rival creatures' liking for the player | todo | empty |

## Death Comes To Those That Wait: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/Death Comes To Those That Wait.txt`, 5,162 lines, landscape `DCTTTW.lnd`, three players) loads from first line to last | todo | openblack stops at line 83 on a misspelt command the game skips; only the first town is built |
| It has no footpath file, as in the game | done | nothing to load |
| It can be chosen from the game's skirmish menu, with its title and description | partial | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Death Comes To Those That Wait: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `DCTTTW.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | map script globals |
| Starts the camera looking at "0.00,0.00" | done | `StartCameraPos` |
| Scales the towns' influence by 0.5 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |
| Sets 6 land balance numbers (4 = 2, 5 = 2, 6 = 10, 1 = 4, 0 = 3.5, 2 = 3.5) | partial | openblack reads numbers 2, 4 and 7 only |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock` |
| Titles the land "Death Comes To Those That Wait -- By Fat Omen" with 1 description line(s) | partial | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |
| Creates 1 climates and sets rain, temperature and wind 1, 1 and 1 times | todo | `WeatherSystem`; see ../weather/; never reached: openblack stops at line 83 |

## Death Comes To Those That Wait: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 25 towns (celtic 4, norse 4, japanese 3, indian 3, greek 3, aztec 2, egyptian 2, african 2, tibetan 2); owners: neutral 22, player one 1, player three 1, player four 1 | partial | `TownArchetype`; only the first town's, before openblack stops at line 83 |
| Sets 17 towns' belief in a player | partial | into the town's belief list; only the first town's, before openblack stops at line 83 |
| Caps belief 57 times | todo | logs "not implemented" |
| Gives towns 102 miracles to offer at their village centres (lightning bolt 10, physical shield 9, shield 8, storm 8, fire 6, creature spell freeze 5, lightning bolt pu1 5, flying flock 5, creature spell weak 4, teleport 3, wood 3, ground flock 3, creature spell strong 3, creature spell itchy 2, heal 2, food 2, water 2, creature spell angry 2, storm pu2 2, explosion 2, explosion pu2 2, heal pu1 1, food pu1 1, water pu1 1, creature spell compassion 1, creature spell big 1, fire pu1 1, fire pu2 1, creature spell invisible 1, creature spell nature 1, lightning bolt pu2 1, storm pu1 1, beam explosion 1, explosion pu1 1, explsion pu1 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Death Comes To Those That Wait: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 46 houses (style A) (Indian 10, Greek 9, Norse 9, Celtic 8, Japanese 3, Aztec 2, African 2, Tibetan 2, Egyptian 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 30 houses (style F) (Indian 10, Norse 6, Japanese 4, Tibetan 3, Aztec 2, Celtic 2, Egyptian 2, Greek 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 25 houses (style E) (Tibetan 5, Celtic 4, Egyptian 3, Japanese 3, Indian 3, Aztec 2, African 2, Greek 2, Norse 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 24 houses (style B) (Celtic 5, Norse 5, Greek 4, Aztec 3, Tibetan 3, Egyptian 2, African 1, Japanese 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 21 storehouses (Celtic 4, Norse 4, Japanese 3, Indian 3, Tibetan 2, Greek 2, Aztec 1, Egyptian 1, African 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 18 crèches (Celtic 3, Indian 3, Norse 3, Egyptian 2, Japanese 2, Tibetan 2, Aztec 1, African 1, Greek 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 16 houses (style C) (Tibetan 4, Norse 3, Celtic 2, Greek 2, Japanese 2, Aztec 1, Egyptian 1, African 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 16 houses (style D) (Celtic 4, Japanese 3, Aztec 2, African 2, Norse 2, Egyptian 1, Tibetan 1, Greek 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 11 wonders (Greek 2, Norse 2, Celtic 2, African 1, Japanese 1, Indian 1, Egyptian 1, Aztec 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 8 workshops (Celtic 2, Aztec 1, Egyptian 1, African 1, Japanese 1, Tibetan 1, Greek 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 4 graveyards (Celtic 2, Aztec 1, Tibetan 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 1 football pitches (Tibetan 1) | partial | `AbodeArchetype` (`CREATE_ABODE`); only the first town's, before openblack stops at line 83 |
| 25 village centres (Norse 5, Celtic 4, Japanese 3, Indian 3, Greek 3, Aztec 2, Egyptian 2, Tibetan 2, African 1) | partial | made as buildings; their worship share ignored; only the first town's, before openblack stops at line 83 |
| 67 planned buildings for towns to build later (houses (style A) 14, houses (style B) 10, houses (style E) 9, graveyards 9, workshops 8, houses (style C) 7, houses (style F) 6, houses (style D) 2, storehouses 1, wonders 1) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |
| 3 temples of rival gods (player one 1, player three 1, player four 1) | partial | `CitadelArchetype`; only the first town's, before openblack stops at line 83 |
| 9 miracle dispensers (beam explosion pu2 1, lightning bolt pu2 1, storm pu2 1, fire pu2 1, flying flock 1, ground flock 1, heal pu1 1, nature 1, food pu1 1) | todo | `CREATE_SPELL_DISPENSER` is empty |

## Death Comes To Those That Wait: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 140 housewives living in their houses | partial | `VillagerArchetype` (`CREATE_VILLAGER_POS`); only the first town's, before openblack stops at line 83 |
| 112 foresters living in their houses | partial | `VillagerArchetype` (`CREATE_VILLAGER_POS`); only the first town's, before openblack stops at line 83 |
| 57 farmers living in their houses | partial | `VillagerArchetype` (`CREATE_VILLAGER_POS`); only the first town's, before openblack stops at line 83 |
| 31 fishermen living in their houses | partial | `VillagerArchetype` (`CREATE_VILLAGER_POS`); only the first town's, before openblack stops at line 83 |
| 15 shepherds living in their houses | partial | `VillagerArchetype` (`CREATE_VILLAGER_POS`); only the first town's, before openblack stops at line 83 |
| 14 town leaders living in their houses | partial | `VillagerArchetype` (`CREATE_VILLAGER_POS`); only the first town's, before openblack stops at line 83 |
| 6 villagers placed in towns without a house (foresters 4, town leaders 1, farmers 1) | todo | `CREATE_TOWN_VILLAGER` logs "not implemented" |
| 119 named villagers, written with a misspelt command the game itself skips | todo | openblack stops loading the land at the first of them (line 83) |

## Death Comes To Those That Wait: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2163 trees and bushes (ConiferA 685, Pine 201, PalmB 201, Palm 194, Bush 167, Beech 156, Conifer 130, OakA 118, Birch 84, PalmA 81, Olive 50, BushB 43, Oak 41, Cypress 6, BushA 4, PalmC 2) | todo | `TreeArchetype` (`CREATE_NEW_TREE`); never reached: openblack stops at line 83 |
| 40 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 9 big forest models | todo | `BigForestArchetype`; never reached: openblack stops at line 83 |
| 86 fireflies | todo | `CREATE_FIRE_FLY` is empty; two lists of 43; every map also gets the nightly top-up: [../nature/fireflies.md](../nature/fireflies.md#where-they-come-from) |
| Sets 167 firefly miracle reward chances | todo | empty; the table it ends with is Land 2's: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table) |

## Death Comes To Those That Wait: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 61 fields (WheatWithFence 30, Wheat 15, Corn 8, Cereal 8) | partial | `FieldArchetype`; only the first town's, before openblack stops at line 83 |
| 40 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |

## Death Comes To Those That Wait: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 351 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 182 × cow | todo | `CREATE_NEW_ANIMAL` is empty |
| 160 × pig | todo | `CREATE_NEW_ANIMAL` is empty |
| 119 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 91 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 86 × dove | todo | `CREATE_NEW_ANIMAL` is empty |
| 84 × pigeon | todo | `CREATE_NEW_ANIMAL` is empty |
| 68 × crow | todo | `CREATE_NEW_ANIMAL` is empty |
| 32 × wolf | todo | `CREATE_NEW_ANIMAL` is empty |
| 15 × tiger | todo | `CREATE_NEW_ANIMAL` is empty |
| 14 × swallow | todo | `CREATE_NEW_ANIMAL` is empty |
| 9 × tortoise | todo | `CREATE_NEW_ANIMAL` is empty |
| 6 × bat | todo | `CREATE_NEW_ANIMAL` is empty |
| 2 × lion | todo | `CREATE_NEW_ANIMAL` is empty |
| 159 flocks | todo | `CREATE_FLOCK` is empty |

## Death Comes To Those That Wait: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 × rocks and boulders (Rock 2) | todo | `MobileStaticArchetype`; never reached: openblack stops at line 83 |
| 2 × fences (CeltFenceShort 2) | todo | `MobileStaticArchetype`; never reached: openblack stops at line 83 |
| 79 lanterns (StreetLantern 67, SingingStoneBase 12) | todo | `StreetLanternArchetype`; never reached: openblack stops at line 83 |

## Death Comes To Those That Wait: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 2 computer players (player three, player four) | todo | the players are made; their thinking isn't (see ../multiplayer/; the switch turns on each one's computer god mind: [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md)); never reached: openblack stops at line 83 |
| 7 rival creatures from mind files (ComputerControlledCreature as Horse 5, ComputerControlledCreature as Tortoise 1, ComputerControlledCreature as Zebra 1) | todo | `CreatureArchetype` with the mind loaded; see creature_mind_scripts.md; never reached: openblack stops at line 83 |
| Sets 7 rival creatures' liking for the player | todo | empty |

## Construct: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/construct.txt`, 32 lines, landscape `construct.lnd`, a test land with one town) loads from first line to last | done | every line is a known command with the right arguments |
| It has no footpath file, as in the game | done | nothing to load |
| It can be chosen from the game's skirmish menu, with its title and description | partial | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Construct: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `construct.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | map script globals |
| Starts the camera looking at "1900.00, 2100.00" | done | `StartCameraPos` |
| Scales the towns' influence by 0.6 | done | `InfluenceSystem` |
| Scales the players' influence by 30 | done | `InfluenceSystem` |
| Titles the land "Construct" with 1 description line(s) | partial | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |

## Construct: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 1 towns (norse 1); owners: player one 1 | done | `TownArchetype` |
| Sets 1 towns' belief in a player | done | into the town's belief list |
| Caps belief 1 times | todo | logs "not implemented" |
| Gives towns 5 miracles to offer at their village centres (food 1, wood 1, heal pu1 1, water pu1 1, fire pu2 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Construct: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1 houses (style A) (Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 temples of rival gods (player one 1) | done | `CitadelArchetype` |
| 1 worship sites (norse for player one 1) | todo | `CREATE_WORSHIP_SITE` is empty |

## Construct: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1 forester living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 housewife living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |

## Construct: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1 × WeepingStone | done | `MobileStaticArchetype` |
| 1 × WeepingStoneReward | done | `MobileStaticArchetype` |
