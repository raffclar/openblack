# Growth and size

A creature starts small and grows over the game, fast while young and then ever more slowly, up to twice its grown
size. Miracles can make it bigger or smaller for a while. Its size changes how far it sees, how fast it moves, how
much it eats and how hard it hits.

**Progress: 28/36 done, 2 partial — 81%**

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new creature starts at its species' starting size | done | `CreatureArchetype::StartScale` from the creature table |
| It grows fast while young, at full speed until a share of its growing-up time, then falls to its slowest | done | `creature_physiology::Growth`; test `YoungCreaturesGrowFastThenSlowly` |
| It grows more with energy to spare | done | `Growth` |
| It grows three times as fast asleep | done | `k_SleepGrowthFactor` |
| It only grows while standing still | done | test `ItGrowsOnlyStandingStillAndUpToFullSize` |
| It grows by itself up to size 2 and no further | done | `k_MaxGrownSize`; same test |
| It only grows from the third stage of growing up | done | `k_GrowingPhase` |
| A creature made bigger by other means stays as it is rather than growing on | done | `TickTurn` |
| The game's growth runs on game turns, so a faster game makes it grow faster | done | the physiology system's time scale |

## Size limits and how it is shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Size 1 stands about 15 units tall whatever the species' mesh | done | `k_HeightAtSizeOne`; test `ACreatureOfSizeOneIsFifteenUnitsTall` |
| The drawn size is kept between a smallest and a largest | done | `ClampScale`; test `SizeIsKeptBetweenItsLimits` |
| The mesh is scaled by the creature's size and the species' own scale | done | `creature_morph` scale |
| Near its player's citadel the creature is shrunk down so that it fits (unconfirmed by how much) | todo | |
| Inside the temple's creature room it is shown at the room's size | todo | see [creature_cave.md](creature_cave.md) |
| The creature's size is a value in the Creature Cave's attributes scroll | done | `creature_cave` snapshot |

## What size changes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bigger creatures walk and run faster | done | test `SpeedsGrowWithSize` |
| Bigger creatures play their animations more slowly | done | test `BiggerCreaturesPlayMoreSlowly` |
| Bigger creatures see further | done | `creature_look` range |
| Bigger creatures use energy more slowly and fill up on less | done | see [physiology.md](physiology.md) |
| Bigger creatures tire less from actions | done | `ApplyActionCost` |
| Bigger creatures hit harder in fights, and make bigger arenas | done | see [fighting.md](fighting.md) |
| Bigger creatures lie out cold longer when knocked out | done | `CreatureFight` knock-out time |
| The leash held in the hand is longer for bigger creatures | done | see [leash.md](leash.md) |
| Small creatures have bigger eyes for their size | done | test `SmallCreaturesHaveBiggerEyesForTheirSize` |
| Hair grows with size but heads shrink | done | test `ScaleGrowsWithSizeButHeadsShrink` |
| Footprints are bigger for bigger creatures | done | see [locomotion.md](locomotion.md) |
| Sounds are picked by size: heavier footsteps for bigger creatures | done | `creature_audio::SizeKey`; see [animation.md](animation.md) |
| Bigger creatures walk through small trees | done | test `BigCreaturesWalkThroughSmallTrees` |
| Bigger creatures can carry heavier things (unconfirmed) | todo | |
| Bigger creatures are more impressive to villagers | todo | see [town_actions.md](town_actions.md) |

## Miracles and scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The big and small creature spells ease the creature to a bigger or smaller size for a while, then back | done | `creature_spells::SizeTarget`; see [../miracles](../miracles/) |
| Those spells keep it between a smallest and largest size | done | `k_SmallestSize`, `k_LargestSize` |
| Scripts can scale a creature automatically to a size, or stop doing so | todo | `CREATURE_AUTOSCALE` is a stub |
| Scripts can read a creature's grown-up size | todo | `ID_ADULT_SIZE` is a stub |
| Scripts can set the size when a creature is made | partial | creatures made from mind files keep their saved size (`CreatureMindFileBody.cpp`); script creation of creatures is in [saves_and_files.md](saves_and_files.md) |
| Size is kept in a saved creature file and in saved games | partial | creature files keep it (test in `test_creature_mind_file_body.cpp`); saved games are todo |
