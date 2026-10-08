# Lightning

A held miracle: while the player keeps casting, forked bolts leap from the hand to people, creatures, trees and
buildings in a cone ahead of the camera, setting them alight. Two power-ups add a wider reach, more forks and more
targets at once.

Given by silver scrolls: to a town by [The Greedy Farmer](../story/silver_scrolls/the_greedy_farmer.md) (with its second level) and
[The Plague](../story/silver_scrolls/the_plague.md), and as a dispenser by the evil ending of [The Pied Piper](../story/silver_scrolls/the_pied_piper.md).

**Progress: 39/43 done, 3 partial — 94%**

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Held miracle: bolts strike only while the cast is held; letting go ends it | done | `src/ECS/Systems/Implementations/MagicSystem.cpp`, `src/Magic/CastInput.cpp` |
| Cost to cast 5000 / 7500 / 10000, then 50 / 70 / 90 a turn and 2 a strike; a part-paid strike is weaker | done | `src/Magic/SpellChants.cpp`; `test/test_spell_chants.cpp` |
| A bolt held longer than 6 s closes by itself | done | spell timers in `src/Magic/SpellLifetime.cpp`; seen in `miracles.hand_extreme_lightning` |
| Two power-up levels: reach 60 / 100 / 140 m, cone half-angle widens, more forks and targets | done | data-driven from the bolt's particle files, `src/Particles/ParticleLightningRules.cpp` |
| The bolts aim along the camera's look direction | partial | `ParticleLightningRules.cpp`; looking straight down keeps the last direction where the game falls back to a fixed one (not reachable in normal play) |

## Picking targets

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Searches a square of map cells twice the reach across, cell by cell outward, fixed objects before moving ones | done | `ParticleLightningRules.cpp`, `src/Particles/LightningMaths.cpp`; `test/test_lightning_maths.cpp` (SearchesASquareOfCellsTwiceItsRadiusAcross) |
| Anything available is a target (people, creatures, animals, trees, buildings, fields, rocks…), but not miracle seeds or dying villagers | done | `src/Particles/ParticleWorldLiving.cpp` |
| Only objects inside the cone ahead are taken, up to 6 / 12 / 28 on the first search | done | `maths::InStrikeCone` in `LightningMaths.cpp` |
| Missing targets are made up with points on the ground ahead, up to 3 / 8 / 15 | done | `ParticleLightningRules.cpp` |
| A visible creature in the cone draws every fork to itself | done | `test/test_lightning_maths.cpp` (ACreatureTakesEveryFork); `miracles.lightning_draws_to_creature` |
| Targets are searched again every second, never more than the first count | done | `ParticleLightningRules.cpp` |
| Each step strikes a random share of the targets (at most 4 / 10 / 20 at once) | done | `ParticleLightningRules.cpp` |
| Forks are rebuilt at every search, two for each target struck at once plus two | done | `test/test_lightning_maths.cpp` (KeepsTwoForksForEachTargetStruckAtOnceAndTwoMore) |
| Land between the hand and a fork's split point stops that fork for the step | done | `ParticleLightningRules.cpp` |

## Damage and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A strike burns, crushes and hits everything within a metre of the tip, not just its target | done | `src/Magic/AreaEffect.cpp`, `src/Magic/SpellBehaviours.cpp` |
| Struck things catch fire through the fire system | done | `src/ECS/Systems/Implementations/FireSystem.cpp`; see `../physics/` for fire spreading |
| Villagers flee the miracle (one reaction per miracle) | done | `MagicSystem.cpp` (ReactToSpell) |
| A heavy crush makes onlookers react to the crushed object | done | `MagicSystem.cpp` |
| Each strike counts towards the caster's alignment and the town's grudge | done | `src/Magic/MiracleDeeds.cpp`; see `../worship/` for alignment |
| Struck villagers are not shown as skeletons by the bolt itself | done | correctly absent |
| A bolt striking only a creature leaves nearby villagers in a valid state (no crash) | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp`, `LivingActionSystem.cpp` |
| Electric arcs crawl over struck objects; an object still struck is arced again after each search, at most 100 arcs alive | done | `src/Particles/ParticleArcRules.cpp`; `test/test_lightning_maths.cpp` (ArcsRunFromEndToEnd) |

## Shields and clashing bolts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spiritual shield stops a fork at its surface, costs the shield 10 / 20 / 30 a hit and sparks | done | `ParticleLightningRules.cpp`, `src/ECS/Systems/Implementations/MagicShieldSystem.cpp`; `miracles.shield` |
| Two hand-cast bolts pointing the same way meet at a clash point | done | `maths::ClashPoint` in `LightningMaths.cpp`; `test/test_lightning_maths.cpp` (BoltsPointingTheSameWayClash); `miracles.lightning_clash` |
| A clash glows at the meeting point, the older bolt carries on three times as thick, and both strike at double strength | done | `ParticleLightningRules.cpp` |

## Bolts not cast by hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bolt from a storm cloud searches a circle round the cloud, all kinds, no creature pull | done | `ParticleLightningRules.cpp`; see `storm.md` |
| A bolt given its targets strikes their ground positions within its reach | done | `ParticleLightningRules.cpp`; which effect uses this path is (unconfirmed) |
| A strike with no miracle behind it (scripts) applies the weather lightning's effect | done | `ParticleWorldLiving.cpp` |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing ball at the hand, larger per level | done | data files, `ParticleLightningRules.cpp`; seen in `miracles.lightning_crowd*` |
| Forks drawn as textured chains, re-laid each step, thicker per level | done | `ParticleLightningRules.cpp` |
| Fork joints shrink with depth, jitter sideways and flicker in alpha | done | `ParticleLightningRules.cpp` |
| The trunk stays glued to the drawn hand between steps | done | `ParticleLightningRules.cpp` |
| Each hit lights the land with an animated light map | done | `src/Particles/LightSheet.cpp`; `test/test_land_light_table.cpp` |
| No land or screen flash for the miracle (the storm's flash is separate) | done | correctly absent |
| Flickering sprites around the held miracle in the hand, fading on release | partial | data-driven in-hand effect; not checked against the game |
| Effect on a holder of the miracle | partial | data-driven; not checked against the game |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A crackle loop starts with the cast, a different one per level | done | `src/Particles/ParticleSoundRules.cpp` |
| On release the crackle is let go and fades from that turn, silent in seven turns | done | `src/Particles/ParticleSoundRelease.cpp`, `ParticleSystem.cpp`; `test/test_particle_sound_release.cpp` |
| The crackle sample's own loop points are honoured; a released loop plays out its tail | done | `src/Resources/Loaders.cpp`, `src/Audio/AudioManager.cpp` |
| No thunder for the miracle | done | correctly absent |
| Closing by the 6 s timer stops the crackle the same way | done | same close-down path |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast lightning at a target | done | `src/Creature/CreatureCastAgenda.cpp`, `CreaturePlanActions.cpp`; `miracles.creature_casts_lightning`; see `creature_spells.md` |
| A bolt in progress is kept in a saved game | todo | openblack has no game saving |
