# Wood miracle

Held at the side of the hand like the horn, the wood miracle pours logs for four seconds; they land as wood piles or
go into a storage pit, a workshop or a building site. There is no extreme version. Wood used for building is in
[../resources/](../resources/).

**Progress: 17/23 done, 2 partial — 78%**

## Casting and paying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wood is held at the side of the hand and pours while the right button is held | done | `src/Magic/CastInput.cpp`, `src/Magic/HandHoldPose.cpp` |
| A pour lasts 4 s (the player's timer) | done | in game the pour lets go 4.0 s after the cast (testbed `miracles.food_wood`) |
| It holds a finite pool that is never refilled | done | `src/Magic/SpellChants.cpp` |
| Each drop pays 3 per unit: the first brings 500 wood, each later one 20 | done | test `SpellRules.FoodAndWoodCostByTheirUnitsAndTheFirstGrainBringsMore` |
| Wood lands only on dry land; the amount is multiplied by the tribal power | done | `src/Magic/SpellBehaviours.cpp` |
| Pressing again needs enough prayer for a first drop | done | `src/Magic/SpellRules.cpp` |
| There is no extreme version | n/a | the game has none |

## The hand and the logs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand rises and tips over 4 s exactly as for food, rolling about the line to the camera | done | `src/Magic/HandMotion.cpp`; test `HandMotion.FoodAndWoodLiftTheHandOverFourSecondsAndStartOver` |
| Stopping eases the hand back over one game turn | done | `src/Magic/HandMotion.cpp` |
| Logs fall from the hand tumbling about the axis across their movement | done | `src/Particles/ParticleHandRules.cpp` |
| The pouring sound loops and fades out softly when let go | done | `src/Particles/ParticleSoundRelease.cpp` |

## Where the wood goes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing log looks outwards in a spiral of nine cells for something to take it | done | test `ResourcePiles.PouringSpiralsOutFromItsCell` |
| A storage pit takes it into its wood piles in order | done | `src/ECS/Systems/Implementations/MagicResources.cpp` |
| A workshop, building site or scaffold takes it | todo | blocked: these don't take wood in openblack yet; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Existing wood piles take it, never beyond full | done | test `ResourcePiles.OnlyPilesLeadingOnHoldNoMoreThanFull` |
| Where nothing takes it a new wood pile appears and rises out of the ground over a second | done | test `ResourcePiles.APileRisesOverASecondAndEndsStill` |
| A wood pile grows evenly with what it holds, never past full size | done | test `ResourcePiles.WoodRisesEvenlyUpToFull` |
| A wood pile never rises above the ground and follows the land | partial | fixed after the user's "wood pile hovers" report; the in-game re-audit of that report is still owed |
| Thuds play as wood lands, small under 200 and big above, heard only within their range | done | `src/Magic/ResourcePiles.cpp` |
| The local player hears the advisor remark on the drop | todo | no guidance voice system |

## What it does to people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby react to a new wood pile, impressed by how much their town wants wood | done | `MagicResources.cpp`, `src/Magic/Impressiveness.cpp` |
| Villagers fetch the wood for building | todo | see [../resources/](../resources/) |
| The creature can learn from the player putting wood in a storage pit or by a building site | partial | storage-pit deed in `src/Magic/MiracleDeeds.cpp`; building-site deed blocked |
| Wood piles are saved with the game | todo | no save system |
