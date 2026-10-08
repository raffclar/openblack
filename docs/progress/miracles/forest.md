# Forest

A seed miracle: thrown onto the land it plants eighteen magic trees in a spiral, which grow while the caster pays for
them and wither away when the miracle ends. Sparkles, a light on the land and a flock of butterflies (or bats, for an
evil god) follow, and the caster's camera takes a short flight round the new forest. There is no power-up.

Given by gold scroll: [The Workshop](../story/gold_scrolls/the_workshop.md).

**Progress: 35/41 done, 3 partial — 89%**

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast with the forest gesture from its seed | done | see `../gesture/` and `dispensers_and_seeds.md` |
| Only on land within influence, in a cell no building covers | done | `src/ECS/Systems/Implementations/ForestSystem.cpp` |
| Not right beside an abode (measured from the abode's middle and its size) | done | `ForestSystem.cpp` (CanGrowAt); `miracles.forest_beside_hut` |
| It can't be cast at an object | done | `src/Magic/SpellBehaviours.cpp` |
| Cost 13000 to cast, then 5 a turn plus 1 for each tree | done | `SpellBehaviours.cpp`, `src/Magic/SpellChants.cpp` |
| No power-up exists | done | `src/Magic/MagicTables.cpp` |
| It can be recast only while it has trees left to make | done | `SpellBehaviours.cpp` |

## Planting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The seed falls, spinning, and plants every tree at once when it lands | done | `src/Particles/ParticleUpdateRules.cpp` (landing), `SpellBehaviours.cpp` (ApplyForestEvent) |
| Eighteen trees on a spiral from 2 to 11 m round the cast point | done | `src/Magic/ForestRules.cpp`; `test/test_magic_shield_forest.cpp` (EighteenTreesSpiralOutFromTwoToEleven) |
| A spot a building covers is skipped, never retried; other trees don't block | done | `ForestSystem.cpp` |
| The kind of tree comes from the ground at the centre: broadleaves, palms on sand or by water, conifers on snow | partial | `ForestSystem.cpp`; `test/test_magic_shield_forest.cpp` (TheGroundsKindsOfTreeAreRolledFor); the water case is not proven against the game |
| Each tree faces a random way; trees further out grow smaller (down to half size) | done | `test/test_magic_shield_forest.cpp` (TreesFurtherOutGrowSmaller) |
| Trees grow a little each turn up to their size | done | `test/test_magic_shield_forest.cpp` (TreesGrowToTheirSizeAndWitherAway) |
| They also grow as other trees do, faster with rain and good land | done | tree growth in the weather work; see `../nature/` |
| The forest belongs to the caster | done | `ForestSystem.cpp` |

## Upkeep and withering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the caster can't afford all the trees, every tree withers each turn and goes at nothing | done | `ForestSystem.cpp`; `test/test_magic_shield_forest.cpp` (ItAffordsItsTreesWhileItHasStrength) |
| When the miracle ends every tree withers away within about two seconds; magic trees never stay | done | `miracles.forest_withers` |
| A forest left alone lasts two minutes | done | spell timers, `src/Magic/SpellLifetime.cpp` |

## Effects on the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers look at the nice miracle when it lands | done | `src/ECS/Systems/Implementations/MagicSystem.cpp` |
| Villagers stop to look at the magic trees | todo | the trees put out their reaction, but villagers have no handling for it (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Creatures take no notice of the magic trees' reaction | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp` |
| A burning magic tree stops drawing looks, and draws them again once put out | done | `src/ECS/Systems/Implementations/FireSystem.cpp` |
| A magic tree gives a quarter of an ordinary tree's wood, scaled by tribal power | partial | rule done (`test/test_magic_shield_forest.cpp` AMagicTreeGivesAQuarterOfTheWoodByTribalPower); villagers don't fell trees for wood yet (see `../resources/`) |
| Shields don't stop the planting; it does no damage | done | correctly absent |

## The caster's camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the caster's camera flies the forest's camera path, placed at the forest | done | `src/Particles/ParticleForestRules.cpp`, `src/ECS/Systems/Implementations/CameraPathSystem.cpp`; see `../camera/` |
| It waits, then glides onto the path and follows it slowed down | done | `ParticleForestRules.cpp`; `test/test_magic_shield_forest.cpp` (CameraZoomer) |
| The flight lasts one play of its animation and ends with the miracle | done | `CameraPathSystem.cpp` |
| A movement key (only on a frame where it would move the camera) or gripping the land gives the camera back | done | `src/Game.cpp`, `CameraPathSystem.cpp`; `miracles.forest_*` scenarios |
| The flight no longer locks the camera and the player's movement (user-reported defect) | partial | fixed in code; the in-game re-check is still owed, along with the other miracles' camera paths |
| The hand stays shown during the flight | done | correctly unchanged |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A trail of rising player-coloured blobs follows the falling seed | done | data-driven |
| An animated light lies on the land for the life of the effect | done | `src/Particles/LightSheet.cpp` |
| At 3.5 s sparkles burst and keep rising, with spinning vortices | done | data-driven; `miracles.forest_grass` |
| At 5.5 s five flocks of ten circle the forest on a squashed sphere | done | `ParticleForestRules.cpp`, `src/Particles/ParticleFlockingRules.cpp`; `test/test_magic_shield_forest.cpp` (TheFlocksCircleOnASquashedSphere) |
| Bats for an evil caster, butterflies otherwise | done | `ParticleForestRules.cpp`; `miracles.forest_bats` |
| The flocks fade in over 4 s and out at 12 to 14 s | done | data-driven |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound with the seed, cut when it lands | done | `src/Particles/ParticleSoundRelease.cpp`; `miracles.forest_grass` |
| A sound with the sparkles at 3.5 s | done | data-driven |
| A sound with the flocks at 5.5 s | done | data-driven |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast a forest | todo | creature casting has no forest action yet (`src/Creature/CreaturePlanActions.cpp`) |
| The forest and its trees are kept in a saved game | todo | openblack has no game saving |
