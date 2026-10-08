# Food miracle

The horn of plenty: held at the side of the hand and poured with the right button, it showers grain that lands as food
piles or goes straight into a storage pit. Its extreme version makes speed-up food. Grain on villagers' tables is in
[../resources/](../resources/).

**Progress: 24/31 done, 2 partial — 81%**

## Casting and paying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The horn is held at the side of the hand and pours while the right button is held | done | `src/Magic/CastInput.cpp`, `src/Magic/HandHoldPose.cpp` |
| The horn holds a finite pool (5000 prayer, 8000 extreme) that is never refilled; the pour ends when it is spent | done | in game the pour closes about 2.3 s after the cast (testbed `miracles.food_wood`) |
| Each grain pays 7 per unit: the first brings 200 food, each later one 18 (20 extreme) | done | test `SpellRules.FoodAndWoodCostByTheirUnitsAndTheFirstGrainBringsMore` |
| A grain becomes food only on dry land and while the miracle has strength; over water it is paid for and lost | done | test `SpellBehaviours.FoodLandsOnDryLandAsPilesAndPaysForEveryGrain` |
| The amount is multiplied by the tribal power | done | `src/Magic/SpellBehaviours.cpp` |
| Letting go keeps the rest in the horn; pressing again needs enough for a first grain | done | `src/ECS/Systems/Implementations/MagicHeldSeed.cpp`, `src/Magic/SpellRules.cpp` |

## The hand while pouring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand rises and tips the horn over 4 s along a curve flat at both ends (up about 20 units and 121 degrees), then starts over | done | `src/Magic/HandMotion.cpp`; tests `HandMotion.ThePoursCurveIsFlatAtBothEndsAndSwellsToAlmostTwiceItsPoints`, `HandMotion.FoodAndWoodLiftTheHandOverFourSecondsAndStartOver` |
| The tip is a roll about the line to the camera, applied at once | done | test `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera` |
| The hand is pinned where the pour began | done | test `MiracleVisuals.APourThatClampsTheHandKeepsItWhereItBegan` |
| Stopping eases the hand back over one game turn | done | `src/Magic/HandMotion.cpp` |
| The pour stops whenever the local player's miracle ends | done | `MagicSystem.cpp` (core audit item 40) |

## Grain in the air

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grain streams from the horn, more as the hand moves, falls, lands and fades | done | particle rules in `src/Particles/ParticleCreateRules.cpp`, `src/Particles/ParticleHandRules.cpp` |
| The pouring sound loops and fades out softly when let go | done | soft release in `src/Particles/ParticleSoundRelease.cpp` |

## Landing and piles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing grain looks outwards in a spiral of nine cells for something to take it | done | test `ResourcePiles.PouringSpiralsOutFromItsCell` |
| A storage pit takes it (1.2 times as much) into its own food pile | done | `src/ECS/Systems/Implementations/MagicResources.cpp`; testbed `miracles.food_speedup_and_stores` |
| A worship site's food pot takes it (twice as much) | todo | blocked: no worship sites |
| Existing food piles in reach take it in turn, never beyond full | done | test `ResourcePiles.OnlyPilesLeadingOnHoldNoMoreThanFull` |
| Where nothing takes it and the land is dry, a new food pile appears, with no puff | done | `MagicResources.cpp` |
| A new pile rises out of the ground over one second and is drawn only once above it | done | tests `ResourcePiles.APileRisesOverASecondAndEndsStill`, `ResourcePiles.ARiseTurnedMidwayKeepsItsSpeed` |
| A food pile grows quickly at first and never past full size | done | test `ResourcePiles.FoodRisesFastAtFirstAndNeverBeyondFull` |
| The grain texture flows while the pile is still sinking | done | test `ResourcePiles.GrainFlowsAQuarterWhenSunk` |
| Piles sit on the land and follow it | partial | fixed after the user's "pile hovers" report; the in-game re-audit of that report is still owed |
| A thud plays as food lands: small ones under 200 food, big ones above, heard only within their range | done | test `ResourcePiles.ThudsAreSmallUnderTwoHundred` |
| The local player hears the advisor remark on the drop | todo | no guidance voice system |

## Extreme food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The extreme horn starts with more prayer and drops 20 per grain | done | `src/Magic/MagicTables.cpp` |
| A new pile from extreme food is speed-up food and sparkles for as long as it lasts | done | `MagicResources.cpp`; testbed `miracles.food_speedup_and_stores` |
| Villagers who eat speed-up food work four times as fast for 2550 turns | todo | blocked: villagers don't take food from piles |

## What it does to people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby react to a new food pile, impressed by how much their town wants food | done | reaction in `MagicResources.cpp`; impressiveness in `src/Magic/Impressiveness.cpp` |
| Villagers take food from the piles | todo | see [../resources/](../resources/); villagers don't use piles yet |
| The creature can learn from the player feeding a storage pit, a worship site or a building site | partial | storage-pit deed in `src/Magic/MiracleDeeds.cpp`; the worship-site and building-site deeds are blocked |
| Food piles are saved with the game | todo | no save system |
