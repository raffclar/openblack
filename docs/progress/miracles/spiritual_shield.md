# Spiritual shield

A sphere the player draws with a circle gesture. It stops other miracles (fireballs, lightning, blasts, flocks) at its
surface, keeps other gods' influence out, and the caster's villagers shelter under it. It drains the caster's prayer
power for as long as it stands and breaks when an attack empties it.

**Progress: 30/37 done, 5 partial — 88%**

## Casting and upkeep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast at the centre of a drawn circle, sized by it | done | `src/Magic/CastInput.cpp`; see `../gesture/` for the circle |
| Size kept between 5 and 1000 m | done | `src/Magic/SpellBehaviours.cpp` (Prepare) |
| Cast only on land within the caster's influence | done | `src/Magic/SpellRules.cpp` |
| Upkeep each turn grows with the square of the size and is eased by tribal power | done | `SpellRules.cpp` (ShieldCostToMaintain), `src/Magic/SpellChants.cpp` |
| A player's shield stands while their prayer power pays for it, then fails | done | `SpellChants.cpp`; `miracles.shield_runs_out`; see `prayer_cost.md` |
| On close-down the shield goes at once with its rings and reactions | done | `src/ECS/Systems/Implementations/MagicShieldSystem.cpp` |

## What it blocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The blocking sphere is a little bigger than the circle and full size from the first turn; its centre never moves | done | `src/Magic/ShieldRules.cpp`; `test/test_magic_shield_forest.cpp` (TheSpiritualShieldsSphereIsALittleBiggerThanItsCircle) |
| Fireballs bounce off it like a mirror | done | `src/Particles/ParticleShieldRules.cpp`; see `fireball.md` |
| Lightning forks stop at its surface | done | see `lightning.md` |
| A blast centred inside it is cancelled | done | `src/Particles/ParticleBlastRules.cpp`; see `blast.md` |
| A flock member flying in dies | done | `src/Magic/FlockMiracle.cpp`; see `flocks.md` |
| Each blocked attack drains the shield by the attacker's own cost; when it runs dry the attack gets through and the shield breaks | done | `SpellBehaviours.cpp` (StrikeSpell) |
| An area effect that only meets a shield costs the attacker but doesn't drain the shield (as the game does) | done | `SpellBehaviours.cpp` |
| Other gods' hands have no influence inside it, so they can't cast there | done | `src/ECS/Systems/Implementations/InfluenceSystem.cpp` (anti-influence) |
| Other players' creatures walk round it | done | `MagicShieldSystem.cpp` (CreatureAvoids), `CreatureLocomotionSystem.cpp`; `test/test_magic_shield_forest.cpp` (OtherPlayersCreaturesWalkRoundAShield) |
| Someone inside ignores reactions to things outside it | partial | done for shield reactions; villagers already in a shield reaction still skip other reactions (open in the core audit) |

## Villagers' and creatures' reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The caster's villagers near the shield (its size plus 30 m) and their town within 500 m take notice | done | `MagicShieldSystem.cpp`, `src/ECS/Systems/Implementations/MagicShieldVillagers.cpp`; `miracles.shield_people` |
| They shelter only when their town was attacked lately | done | `test/test_magic_shield_forest.cpp` (VillagersOfATownReactOnlyWhenItWasAttackedLately) |
| Reaction length and the wait before reacting again follow the table, whatever the distance | done | `MagicShieldVillagers.cpp` |
| Sheltering villagers walk well inside on their own side and face outward | done | `src/Magic/ShieldRules.cpp`; `test/test_magic_shield_forest.cpp` (VillagersShelterWellInsideOnTheirOwnSide) |
| Sheltering villagers point, look at the hand or stand | partial | the animation is chosen as the game does (`test/test_magic_shield_forest.cpp`); villagers don't play animations yet |
| A struck shield gives a "struck" reaction | done | `MagicShieldSystem.cpp` (ShieldStruck) |
| When it breaks, villagers run from where it stood | partial | `MagicShieldVillagers.cpp`; the game's hide-in-a-building choice is missing |
| A broken shield impresses four times as much; a shield over the attacker's own town impresses nothing | done | `src/Magic/Impressiveness.cpp`; `test/test_magic_shield_forest.cpp` (ADestroyedShieldImpressesFourTimesAndAnAttackersShieldNotAtAll) |
| The caster's creature is impressed by it as a nice miracle and can learn it | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp` |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fifteen player-coloured patches orbit the sphere, cut off at the ground | done | `ParticleShieldRules.cpp`; `miracles.shield` |
| The sphere grows from a tenth to full size over 2 s and keeps the spin of the casting hand | done | `ParticleShieldRules.cpp` (initial spin) |
| Patches fade in over about 8.5 s and fade with the shield's strength, never below a floor | done | `ParticleShieldRules.cpp` |
| A hit raises up to four crackling spark arcs that wiggle and fade over 3 s | done | `ParticleShieldRules.cpp` (shield spark) |
| On close-down it shrinks over 2 s and is gone at 2.2 s | done | data-driven |
| Orbiting patches round the miracle held in the hand | partial | data-driven in-hand effect; not checked against the game |
| Effect on a holder of the miracle | partial | data-driven; not checked against the game |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A looping hum, one of three sizes by the shield's size | done | `src/Particles/ParticleMaths.cpp` (SoundSizeFromRadius) |
| The hum stops at once when the shield goes | done | `src/Particles/ParticleSoundRelease.cpp`; `test/test_particle_sound_release.cpp` |
| Each hit plays a spark sound | done | `ParticleShieldRules.cpp` |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast the spiritual shield (it's its defensive miracle in fights) | todo | creature casting has no shield action yet (`src/Creature/CreaturePlanActions.cpp`) |
| Shields and their reactions are kept in a saved game | todo | openblack has no game saving |
