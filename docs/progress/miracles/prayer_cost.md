# Prayer power cost

What each miracle costs in prayer power: charging the seed, the prayer it starts with, the upkeep each turn and per
event, refills from whoever cast it, and how tribal power changes the cost and strength. How worshippers make prayer
power is in [../worship/](../worship/).

**Progress: 18/25 done, 4 partial — 80%**

## Who pays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle from an icon is paid and topped up from its worship site's store | partial | the player's prayer power stands in for the site (`src/Magic/PrayerRules.cpp`); no worship sites |
| A player's miracle cast from a globe or dispenser seed with no icon gets no refill at all | done | test `PrayerRules.AMiracleFromAGlobesSeedIsToppedUpByNobodyButTheNeutralPlayer` |
| The neutral player (scripts) gives all a miracle needs; a player with no store gives nothing | done | tests `PrayerRules.TheNeutralPlayerGivesAllAndAPlayerWithoutAStoreNothing`, `PrayerRules.ThePlayerPaysFromTheirStoreAsMuchAsTheyHave` |
| A creature's miracle uses its player's tribal power | done | `MagicSystem.cpp` (core audit item 26); the creature's own energy cost is in [creature_spells.md](creature_spells.md) |

## Charging a seed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A globe's seed comes fully charged for free | done | `GiveSeedToHand` in `MagicSystem.cpp` |
| An icon charges its seed up to the miracle's cost to create (base and each power-up level have their own) | partial | `src/Magic/MagicTables.cpp` costs; `SummonSeed` takes it from prayer power at once as a stand-in for the icon's charging; test `PrayerRules.ASummonedSeedIsChargedAndGivesBackWhatItHoldsWhenDropped` |
| A seed's power is how much of its need it holds, at most full | done | `src/Magic/SpellSeedRules.cpp` |
| A power-up's extra cost is charged by the icon and any surplus is refunded to the site | partial | taken from or given back to prayer power at once; no icons |
| Only a seed made at an icon is topped up and refunds to its icon | done | `src/ECS/Systems/Implementations/MagicHeldSeed.cpp` |
| A dropped or scribbled-away seed returns what it holds to its worship site | partial | refunded to the player's prayer power (`src/Magic/PrayerRules.cpp`) |
| Scripts ask how much prayer power a miracle needs | todo | `GET_MANA_FOR_SPELL` is a stub in `src/CHLApi.cpp` |

## A miracle's prayer while it lives

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cast miracle starts with its initial prayer times the seed's multiplier | done | test `SpellRules.ASeedsCastScalesThePrayerPowerAndTimeAndAFireSeedIsAlwaysTheSameSize` |
| Its safe level is five seconds of upkeep, at most its initial prayer and at least one event's cost | done | `src/Magic/SpellChants.cpp`; test `SpellChants.safetyLevelIsFiveSecondsOfUpkeep` |
| Forest and the shields keep their initial prayer as the safe level | done | test `SpellChants.maintainedShield` |
| A recharged miracle refills its shortfall from its caster each turn | done | tests `SpellChants.refillingCasterHoldsTheSafetyLevel`, `SpellChants.refillUpToCostOrWhole` |
| Each turn it pays its upkeep (at least 1), and each event its event cost | done | tests `SpellChants.eventCost`, `SpellChants.playerCastLivesOnItsStore` |
| Teleport and the shields divide their costs by the tribal power | done | test `SpellChants.tribalPowerDividesCost` |
| Its strength is how full it is times tribal power, the seed's power and its multiplier; at nothing it ends | done | test `SpellChants.strengthEdges` |
| A held miracle's prayer runs down with its upkeep while held | done | test `SpellBehaviours.AHeldMiraclesPrayerPowerRunsDownWithItsUpkeep` |
| Food and wood pay per unit dropped (the first drop brings more), even when it falls in water, and are never refilled | done | test `SpellRules.FoodAndWoodCostByTheirUnitsAndTheFirstGrainBringsMore`; see [food.md](food.md), [wood.md](wood.md) |
| Pressing food or wood again needs enough prayer left for a first drop | done | `src/Magic/SpellRules.cpp` |
| The miracle table's costs per miracle and level (initial, create, per event, per turn, per shield hit) come from the game's data | done | `src/Magic/MagicTables.cpp`; test `SpellChants.rulesFromTables` |

## Tribal power and display

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Tribal power is the product of the miracle's tribes' powers, kept between 0.5 and 100 | done | `src/Magic/MagicTables.cpp` |
| A player's tribal powers grow from the tribes worshipping them | todo | blocked on worship; every tribe counts 1.0 |
| Spending prayer sends motes of light from the caster to the miracle, one per 100 prayer power, when the caster has a worship site | todo | the spending hook in `src/Magic/SpellChants.cpp` has no listener; see [../rendering/light_beams.md](../rendering/light_beams.md) |
