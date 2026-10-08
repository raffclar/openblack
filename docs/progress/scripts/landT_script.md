# Tutorial land script

`Scripts/LandT.txt` builds the tutorial land: two of the player's towns, a neutral town, toys (skittles, a bowling ball, balls, dice), weeping stones and a pier. The game loads it by its own path and starts the tutorial's control script by name rather than through the story's control script (both names sit in the game; how it is chosen from the menu is unconfirmed). Counts below are from the script; what the tutorial teaches is the story domain's ([../story/](../story/)).

**Progress: 32/45 done, 1 partial — 72%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments |
| The tutorial land has no footpath file, as in the game | done | nothing to load |
| Loading the tutorial land starts the tutorial's own control script | todo | openblack always starts the story's control script (Land 1's challenges) |
| The tutorial can be chosen from the game's menu | todo | only through `--start-level LandT.txt` or the debug land menu |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `LandT.lnd` | done | `Game::LoadLandscape` |
| Is land number 6 of the story | done | map script globals |
| Starts the camera looking at "0.00,0.00" | done | `StartCameraPos` |
| Scales the towns' influence by 1 | done | `InfluenceSystem` |
| Scales the players' influence by 1 | done | `InfluenceSystem` |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 3 towns (japanese 1, celtic 1, indian 1); owners: player one 2, neutral 1 | done | `TownArchetype` |
| Sets 2 towns' belief in a player | done | into the town's belief list |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 wonders (Japanese 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style F) (Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style A) (Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 houses (style C) (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 workshops (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 storehouses (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 graveyards (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 planned buildings for towns to build later (houses (style F) 5) | todo | `CREATE_PLANNED_ABODE` logs "not implemented" |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 4 housewives living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 town leader living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 shepherd living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 forester living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |
| 1 farmer living in their houses | done | `VillagerArchetype` (`CREATE_VILLAGER_POS`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 341 trees and bushes (Oak 101, OakA 69, Bush 51, BushA 20, BushB 15, Beech 14, PalmC 13, ConiferA 12, Palm 12, Conifer 7, Birch 7, Pine 6, PalmB 5, PalmA 5, Olive 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 4 big forest models | done | `BigForestArchetype` |
| 1 × feature "Spikey Pilar Sand" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Pier" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| Sets 42 firefly miracle reward chances | todo | empty; every weight is zero, so a firefly caught here gives nothing; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 fields (Wheat 2) | done | `FieldArchetype` |
| 5 fish farms | todo | `CREATE_TOWN_FISH_FARM` is empty |
| 2 pots (MagicWood 2) | done | `PotArchetype` |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 148 × swallow | todo | `CREATE_NEW_ANIMAL` is empty |
| 129 × seagull | todo | `CREATE_NEW_ANIMAL` is empty |
| 92 × sheep | todo | `CREATE_NEW_ANIMAL` is empty |
| 71 × crow | todo | `CREATE_NEW_ANIMAL` is empty |
| 26 × tortoise | todo | `CREATE_NEW_ANIMAL` is empty |
| 22 × horse | todo | `CREATE_NEW_ANIMAL` is empty |
| 19 flocks | todo | `CREATE_FLOCK` is empty |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 76 × rocks and boulders (Boulder3Sand 12, Boulder2Sand 9, RockSandstone 8, Boulder3Lime 7, FlatrockSandstone 5, Boulder1Lime 5, FlatrockLimestone 5, LongrockSandstone 4, RockLimestone 4, Boulder2Lime 4, Boulder1Sand 3, SharprockLimestone 3, SharprockSandstone 2, SquarerockSandstone 2, LongrockLimestone 2, SquarerockLimestone 1) | done | `MobileStaticArchetype` |
| 27 × toys (ToySkittle 16, ToyBowlingBall 5, ToyBall 4, ToyDie 2) | done | `MobileStaticArchetype`; the toys themselves: [../nature/toys.md](../nature/toys.md) |
| 2 × WeepingStone | done | `MobileStaticArchetype` |
| 6 lanterns (StreetLantern 4, CountryLantern 2) | done | `StreetLanternArchetype` |
| 1 bonfires | partial | a static bonfire; one number ignored |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1 streams through 37 points | done | `StreamSegmentArchetype` |
