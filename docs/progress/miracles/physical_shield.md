# Physical shield

A solid dome the player draws with a circle gesture. It rises out of the land, spins down and bobs, and stops thrown
rocks and other physical objects; each blow drains the caster's prayer power. Miracles pass through it.

Given by gold scroll: [Khazar's Shield Challenge](../story/gold_scrolls/khazars_shield_challenge.md).

**Progress: 24/28 done, 3 partial — 91%**

## Casting and upkeep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast at the centre of a drawn circle, sized by it, held to the side in the hand | done | `src/Magic/CastInput.cpp`, `src/Magic/HandHoldPose.cpp`; see `casting_and_globes.md` |
| Size kept between 5 and 1000 m; upkeep grows with the square of the size | done | `src/Magic/SpellBehaviours.cpp`, `src/Magic/SpellRules.cpp` |
| Other gods lose their influence inside it; their creatures walk round it | done | `src/ECS/Systems/Implementations/MagicShieldSystem.cpp` (as `spiritual_shield.md`) |
| Creatures never cast it | done | correctly absent |

## The dome

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sized by the circle and sunk into the land | done | `src/Magic/ShieldRules.cpp`; `test/test_magic_shield_forest.cpp` (TheDomeIsSizedAndSunkByItsRadius) |
| Hidden for half a second, then grows over 1.5 s, spins down from the hand's spin over 6 s and bobs | done | `ShieldRules.cpp` (StepDome); `test/test_magic_shield_forest.cpp` (TheDomeIsHiddenThenGrowsSpinsDownAndBobs) |
| A dying dome fades over 1.5 s and is gone at 2.25 s | done | `test/test_magic_shield_forest.cpp` (ADyingDomeFadesOverASecondAndAHalfAndGoesAtTwoAndAQuarter) |
| Drawn by its strength, never fainter than a floor | done | `test/test_magic_shield_forest.cpp` (TheDomeIsDrawnByItsStrengthNeverFainterThanForty) |
| The solid shell mesh, its outer layer drawn additive | done | `src/Graphics/RendererShields.cpp` |

## Physics

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its solid shape is a cone under the dome | done | `test/test_magic_shield_forest.cpp` (TheDomesSolidShapeIsACone) |
| A thing crosses its hull only moving against a face | done | `test/test_magic_shield_forest.cpp` (AThingCrossesTheDomeOnlyMovingAgainstAFace) |
| Thrown objects bounce off with the game's spring response | partial | stand-in mirror bounce in `MagicShieldSystem.cpp`; needs the game's physics port (see `../physics/`) |
| The hull follows the dome's size as it grows | partial | the hull is set at full size from the start; the game resizes it, lagging behind the drawn dome |
| A blow by a rock costs the dome by the rock's speed and mass | done | `test/test_magic_shield_forest.cpp` (ABlowCostsTheDomeByItsMomentum) |
| A blow counts a small good deed at the point and marks the thrower as the town's attacker | done | `MagicShieldSystem.cpp` (Impact) |
| Fireballs and other particle miracles pass through it | done | correctly absent |
| The hand can't pick up or feel the dome | partial | The hand refuses shields (`hand_grab::ValidForPlaceInHand`); whether the hand feels the dome isn't researched |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The caster's villagers shelter under it as under the spiritual shield | done | `src/ECS/Systems/Implementations/MagicShieldVillagers.cpp`; see `spiritual_shield.md` |
| A blow gives a "struck" reaction; emptying it gives a "destroyed" one | done | `MagicShieldSystem.cpp` |
| Over the attacker's own town it impresses nothing | done | `src/Magic/Impressiveness.cpp` |
| The caster's creature is impressed and learns the spiritual shield from it | done | `src/ECS/Systems/Implementations/ReactionSystem.cpp` |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing points ride the dome, trailing player-coloured sparkles | done | `src/Particles/ParticleShieldRules.cpp`; `miracles.shield_physical_throw` |
| The sparkles grow with the dome's size and fade down over 6 s | done | data-driven |
| The effect fades with the dome | done | `MagicShieldSystem.cpp` |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound when it is made | done | data-driven; `miracles.shield_physical_throw` |
| A looping hum, let go to ring out when the dome is deleted | done | `src/Particles/ParticleSoundRelease.cpp` |
| A collision sound when things hit it | done | The physics' collision sounds pick it by the shield's collision sound type (`DynamicsSystem`, editor sound bank); see `../physics/throwing_and_landing.md` |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dome is kept in a saved game | todo | openblack has no game saving |
