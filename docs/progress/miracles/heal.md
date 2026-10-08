# Heal

A miracle cast on the land that restores the life of the people, animals and creatures near the cast point and cures
poisoning. The power-up first raises a glowing mushroom and heals a much wider area a few seconds later.

Given by silver scrolls: a heal chest by [The Sacrifice](../story/silver_scrolls/the_sacrifice.md) and [Swap To Brown Bear](../story/silver_scrolls/swap_to_brown_bear.md),
heal dispensers by [The Ogre](../story/silver_scrolls/the_ogre.md) and [The Pied Piper](../story/silver_scrolls/the_pied_piper.md), and a temple that heals all around it by
[The Beach Temple Puzzle](../story/silver_scrolls/the_beach_temple_puzzle.md).

**Progress: 31/35 done, 3 partial — 93%**

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It can't be cast where there is nobody to heal | done | `src/Magic/SpellBehaviours.cpp` (CanCastAt) |
| Cost 6000 / 9000 to cast and 10 for each person healed | done | `src/Magic/SpellChants.cpp` |
| Who is healed is chosen once, at the cast; anyone walking in later is not | done | `SpellBehaviours.cpp` |
| The miracle lasts as long as its effect | done | `src/Magic/SpellLifetime.cpp` |
| The caster's tribal power widens the reach and raises the most it can heal | done | `SpellBehaviours.cpp` (HealReachOf) |
| Plain heal: 10 m, at most 20; power-up: 35 m, at most 100, healing 3.5 s after the cast | done | `src/Magic/HealTargets.cpp`; `miracles.heal_powerup_crowd` |

## Who is healed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Map cells are walked outward from the cast cell; the plain heal looks in only a 2 by 2 block of cells | done | `HealTargets.cpp`, `src/Magic/MapSpiral.cpp`; `test/test_magic_heal.cpp` (ThePlainHealLooksInOnlyFourCells) |
| Each cell's people are measured from the cast point moved into that cell | done | `test/test_magic_heal.cpp` (MeasuresEachCellFromTheCastPointMovedIntoIt) |
| Distance is flat (height ignored) and strictly inside the reach | done | `test/test_magic_heal.cpp` (TheRadiusIsStrict) |
| Within a cell the most recent arrivals are taken first, up to the limit | done | `HealTargets.cpp`; `test/test_magic_heal.cpp` (StopsAtItsMost) |
| Only living villagers and animals, and any creature; doves never; the dead are not revived | done | `src/ECS/Systems/Implementations/MagicLiving.cpp` |
| Someone held in the hand is not healed | partial | The hand now holds people (`HandGrabSystem`, `InHand`); the heal doesn't test for it yet (see `../hand/`) |
| Two heals never work on the same person at once | done | `src/Particles/ParticleHealRules.cpp` |

## What the heal does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each person is healed the moment their glow appears, up to full life at full strength | done | `ParticleHealRules.cpp`, `SpellBehaviours.cpp` |
| It cures poisoning (for example from poisoned food) | done | `MagicLiving.cpp` (CurePoison) |
| Buildings, trees and fields are not healed | done | only living things are targets |
| A creature in a fight gets fight health and stamina back, and its wounds mend | done | `MagicLiving.cpp` |
| A creature's cuts and scars fade when healed | partial | the healing is counted, but scars are not yet placed on the creature's skin (no world-to-skin mapping) |
| A creature healed by another creature thinks better of it | done | `MagicLiving.cpp` |
| Healing counts as a good deed for the caster's alignment | done | `src/Magic/MiracleDeeds.cpp`; see `../worship/` |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers look at the nice miracle (one reaction per miracle) | done | `MagicSystem.cpp`, `src/ECS/Systems/Implementations/ReactionSystem.cpp` |
| A creature watching a player's or creature's heal is impressed and can learn it | done | `ReactionSystem.cpp`, `CreatureMindReactions.cpp`; see `../creature/` |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A glowing chakra follows each healed person's middle, sized to them | done | `ParticleHealRules.cpp` |
| The healed person glows pale cyan-white, rising over 1.5 s and gone by 3 s | done | `ParticleHealRules.cpp`, `RenderingSystem.cpp`; creatures glow too, which the game may not show (unconfirmed) |
| Five spinning sparkles rise and spread from each person | done | `ParticleHealRules.cpp`; `miracles.heal_crowd` |
| The chakra ends when its person goes or is picked up | done | `ParticleHealRules.cpp` |
| Power-up: a tall glowing mushroom grows, turns and collapses over 6 s | done | `src/Particles/ParticleCurveRules.cpp`; `miracles.heal_creatures` |
| Power-up: seven stars shoot up about 37 m and fade | done | `ParticleCurveRules.cpp` |
| Two sparkles swing to and fro around the miracle held in the hand | done | `ParticleHealRules.cpp` (in-hand rule) |
| Effect on a holder of the miracle | partial | data-driven; not checked against the game |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One heal sound per miracle, when the first chakra appears | done | `ParticleHealRules.cpp`; `miracles.heal_crowd` |
| Power-up: the mushroom's sound at the cast, stopped at once if its effect goes | done | `ParticleSoundRelease.cpp` |
| Power-up: the heal sound follows about 3.5 s later | done | `miracles.heal_powerup_crowd` |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast heal at a hurt villager | done | `src/Creature/CreaturePlanActions.cpp`; `miracles.creature_casts_heal`; see `creature_spells.md` |
| A heal in progress is kept in a saved game | todo | openblack has no game saving |
