# Map, demo and test scripts

Besides the story's lands, `Scripts/` holds the map script the game reads first (`map.txt`), a land script for the demo's version of the third land (`demo2.txt`), and two developers' test lands (`comp.txt` for the computer players, `dance.txt` for dances) whose landscapes were never shipped. Land script commands are in [land_script_commands.md](land_script_commands.md).

**Progress: 37/53 done, 2 partial — 72%**

## The map script (`map.txt`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game starts a new game by reading the map script, whose one line loads the first land's script | todo | openblack never reads `map.txt`; it loads `Land1.txt` (or `--start-level`) itself (`Game::Run`) |
| A map script's other commands (players, date and time, turn length, challenges, the language, an old creature, configuration) | todo | see the map script part of [land_script_commands.md](land_script_commands.md) |

## The demo land (`demo2.txt`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The demo's land script builds its version of the third land on the shipped `Land3.lnd`, with older commands (old-style animals and town fields) | done | loads from first line to last in openblack (`--start-level demo2.txt`) |
| It has no footpath file, as in the game | done | nothing to load |
| The demo's own challenges | n/a | the shipped `challenge.chl` is the full game's; the demo's scripts aren't shipped |

## Demo land: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land3.lnd` | done | `Game::LoadLandscape` |
| Is land number 3 of the story | done | map script globals |
| Starts the camera looking at "3237.21,3157.85" | done | `StartCameraPos` |
| Creates 0 climates and sets rain, temperature and wind 1, 1 and 1 times | done | `WeatherSystem`; see ../weather/ |

## Demo land: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 6 towns (celtic 2, japanese 1, tibetan 1, indian 1, egyptian 1); owners: neutral 3, player one 2, player two 1 | done | `TownArchetype` |
| Sets 6 towns' belief in a player | done | into the town's belief list |
| Gives towns 13 miracles to offer at their village centres (heal 2, food 2, lightning bolt 2, creature spell compassion 1, fire 1, water 1, creature spell big 1, storm pu1 1, physical shield 1, wood 1) | todo | the command logs "not implemented"; see ../miracles/ |

## Demo land: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 21 houses (style F) (Indian 11, Japanese 4, Egyptian 3, Celtic 2, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 15 houses (style A) (Japanese 5, Egyptian 5, Tibetan 2, Indian 2, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 houses (style D) (Egyptian 5, Japanese 2, Tibetan 2, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 9 houses (style C) (Egyptian 4, Tibetan 3, Japanese 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 8 houses (style B) (Japanese 6, Egyptian 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style E) (Celtic 2, Tibetan 2, Japanese 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 storehouses (Japanese 1, Celtic 1, Tibetan 1, Indian 1, Egyptian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 workshops (Japanese 1, Celtic 1, Indian 1, Egyptian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 crèches (Japanese 1, Tibetan 1, Indian 1, Egyptian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 village centres (Japanese 1, Tibetan 1, Indian 1, Egyptian 1) | partial | made as buildings; their worship share ignored |
| 69 planned buildings for towns to build later (houses (style F) 19, houses (style C) 12, houses (style B) 11, houses (style A) 11, houses (style D) 9, houses (style E) 4, crèches 1, workshops 1, wonders 1) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |
| 2 temples of rival gods (player one 1, player two 1) | done | `CitadelArchetype` |
| 4 worship sites (japanese for player one 1, celtic for player one 1, egyptian for player two 1, tibetan for player two 1) | todo | `CREATE_WORSHIP_SITE` is empty |

## Demo land: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 69 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 27 fishermen living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 26 foresters living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 16 farmers living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 14 shepherds living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 3 town leaders living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 29 villagers placed in towns without a house (housewives 15, fishermen 14) | todo | `CREATE_TOWN_VILLAGER` logs "not implemented" |

## Demo land: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1643 trees and bushes (PalmB 599, Palm 363, BushB 154, Conifer 88, Pine 80, PalmC 72, BushA 55, Beech 52, Cedar 50, HedgeA 41, PalmA 24, Olive 18, Hedge 16, Copse 10, Cypress 9, ConiferA 8, Bush 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 22 forests that trees belong to (foresters fell and replant them) | todo | `CREATE_FOREST` is empty; the trees are made unattached |
| 14 dead trees | partial | `DeadTreeArchetype`; their lean ignored |
| 16 big forest models | done | `BigForestArchetype` |
| 20 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 4 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Tibetan Large Pillar Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 70 patches of mist | done | `MistArchetype` |
| 38 fireflies | todo | `CREATE_FIRE_FLY` is empty; see [../nature/fireflies.md](../nature/fireflies.md#where-they-come-from) |

## Demo land: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 8 fields (Wheat 8) | done | `FieldArchetype` |
| 7 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 29 pots (MagicWood 27, MagicFood 2) | done | `PotArchetype` |

## Demo land: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 39 × cow (older command) | todo | `CREATE_ANIMAL` is empty |
| 16 × dove (older command) | todo | `CREATE_ANIMAL` is empty |
| 6 × sheep (older command) | todo | `CREATE_ANIMAL` is empty |
| 6 flocks | todo | `CREATE_FLOCK` is empty |

## Demo land: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 94 × rocks and boulders (Rock 87, Boulder1Sand 7) | done | `MobileStaticArchetype` |
| 31 × fences (CeltFenceTall 31) | done | `MobileStaticArchetype` |
| 18 × WeepingStoneReward | done | `MobileStaticArchetype` |
| 1 × Vortex | done | `MobileStaticArchetype` |
| 1 × CreatureCage | done | `MobileStaticArchetype` |

## Demo land: water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1 arenas | todo | `CREATE_ARENA` is empty |

## The computer players' test land (`comp.txt`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads a developers' landscape (`DEVDATA\Landscape\old.led`) that isn't shipped, so neither the game nor openblack can open it | n/a | the landscape is missing from the game |
| Builds five Celtic towns with 43 buildings, 168 villagers, 19 cows, 65 trees and two temples and two worship sites (the player's and a rival's), and switches on a second computer player with an old-style creature | n/a | for reference: what the test land would build; every command it uses is covered in land_script_commands.md |

## The dance test land (`dance.txt`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads a dance test landscape (`dance.led`) that isn't shipped, and builds nothing else | n/a | the landscape is missing from the game |
