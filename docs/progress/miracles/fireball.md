# Fireball

The player takes a fire seed and throws it: a burning ball (three with the first power-up, eight with the second) flies,
bounces and rolls over the land, setting alight whatever it passes. Enemy balls can be caught, and a held fire seed can
swallow one. How fire then spreads, burns and goes out is physics: See ../physics/fire.md.

Given by silver scrolls: the first and second levels by [The Idol](../story/silver_scrolls/the_idol.md), the second and third
by [Stanley The Wolf](../story/silver_scrolls/stanley_the_wolf.md). Given by gold scroll: the first level by
[Khazar's Fireball Challenge](../story/gold_scrolls/khazars_fireball_challenge.md).

**Progress: 39/47 done, 5 partial — 88%**

## Casting and the throw

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The held fire seed shows flames on the hand, stepped every drawn frame | done | Particle data `SF_FireBallInHand`; `src/ECS/Systems/Implementations/MagicSystem.cpp` |
| A fire seed on a dispenser shows its holder effect | done | Particle type registered; see [dispensers_and_seeds.md](dispensers_and_seeds.md) |
| Letting go throws: the ball leaves the hand along the hand's swing, its speed from the swing through the game's throw-speed table | done | `src/Particles/ParticleFireballRules.cpp`; scenario `miracles.hand_fireball_throws`; test `ParticleMiracleTest.AFireballIsThrownFromTheHandFliesBouncesAndSaysWhereItIsEachStep` |
| The first power-up throws 3 balls and the second 8, scattered in a ring with some random speed | done | Particle data `SF_FireBallThrowPU`/`PU2` read by `ParticleFireballRules.cpp` |
| It costs 3500, 7000 or 10000 to cast, with nothing per turn or per event; the miracle lasts 20 s | done | `src/Magic/SpellRules.cpp`, `src/Magic/SpellChants.cpp`. See [prayer_cost.md](prayer_cost.md) |
| A spinning hand puts side spin on the ball, which curls and fades over 1.5 s | done | `ParticleFireballRules.cpp` (side spin from the measured hand spin) |
| The ball is drawn starting from the hand and eases onto its true path | done | `ParticleFireballRules.cpp` (draw offset decay) |
| Scripts and computer players lob the ball from a point to a target, with no swing | done | `src/Magic/ScriptCast.cpp`; test `test_script_cast.cpp` |
| A creature casts fireballs at what it wants to burn | todo | No fireball plan in `src/Creature/CreaturePlanActions.cpp`. See [creature_spells.md](creature_spells.md) |
| A creature catches an enemy fireball and throws it back | todo | No such creature plan |

## Flight and end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball flies under gravity, bounces off the land and rolls down slopes | done | `src/Particles/ParticleMiracleMaths.cpp` (gravity with floor and slope bounce) |
| A ball almost still on a slope is nudged so it keeps sliding the game's way | done | `ParticleMiracleMaths.cpp` (fixed in the firewater round) |
| A ball bounces off a physical shield it isn't let through | done | Test `ParticleMiracleTest.AFireballBouncesOffAShieldItIsNotLetThrough`. See [physical_shield.md](physical_shield.md) |
| The ball ends when it stops rolling, falls into water, or cools below 400 degrees | done | `ParticleFireballRules.cpp`, `src/ECS/Systems/Implementations/FireSystem.cpp`; scenario `miracles.fireball_water` |
| An ended ball fades out over 1 s and is gone about 4 s later | done | `src/Particles/ParticleUpdateRules.cpp` |
| Rain cools a ball and makes it steam | done | `FireSystem.cpp` (rain cooling), particle steam condition |
| The water miracle puts a ball out | done | `FireSystem.cpp`; scenario `miracles.water_fire` |
| The ball is not a physics object: it can't be tugged, knocks nothing over, and creatures can't pick it up, throw, stomp or fight it | done | The ball entity carries only its position, fire and ball data (`src/ECS/Components/MagicFireBall.h`, `src/ECS/Systems/Implementations/ParticleObjectEffects.cpp`) |

## Setting things alight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each ball is a burning thing as hot as the miracle is strong (6000 degrees at full strength) | done | `ParticleObjectEffects.cpp` (attach a ball); test `test_fire.cpp` |
| The ball's ability to hold heat follows the miracle's live strength | done | `FireSystem.cpp` (fixed in the firewater round) |
| Trees, buildings, fields and people the ball passes catch fire | done | `FireSystem.cpp`; scenarios `miracles.fireball_trees`, `miracles.forest_fire`, `miracles.fireball_building` |
| Villagers who fight fires take the ball's heat like any other fire's | done | `FireSystem.cpp` |
| Villagers set alight run burning and die; those reacting always return to a sensible state | done | `src/ECS/Systems/Implementations/VillagerFire.cpp`, `src/ECS/Systems/Implementations/LivingActionSystem.cpp` (invalid-state fix) |
| A villager the fire kills counts against the caster | partial | Re-audit: the kill's credit to the player and its other side effects are still blocked (open fire item 51) |
| An animal the fire kills falls dead and lies there | partial | Re-audit: animals killed by fire still vanish (`src/ECS/WorldObjects.cpp`); the new dying and dead clips are used only by the tornado's kills (open fire item 56) |
| A town notices its people hurt by fire | partial | Re-audit: the town's injured count has no consumer yet (open fire item 52). See ../town/ |
| A villager on the way to worship is spared the town's fire alarm | partial | Re-audit: blocked on worship, the flag is never set (open fire item 60). See ../worship/ |
| A ball in rain or after setting something alight sizzles | done | `FireSystem.cpp` (ball draws steam only); a ball dropping into the lake cools too fast to sizzle, as in the game |

## Point events, reactions and learning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every step the ball moves the miracle's position and sends a point event there | done | `ParticleFireballRules.cpp`, `src/Magic/SpellBehaviours.cpp` |
| The point event carries the ball's last drawn movement | done | `ParticleFireballRules.cpp` (fixed in the firewater round) |
| People nearby react to the miracle once, the first time it has an effect | done | `src/Magic/ReactionRules.cpp`, `MagicSystem.cpp` |
| The player's creature may copy the fireball where it lands near a town | done | `MagicSystem.cpp` per-event mimic |
| A creature learns the fireball by watching; it must know the plain one before the power-ups | done | `src/Creature/CreatureWatching.cpp`; test `test_creature_learning.cpp`. See ../creature/ |

## Catching and absorbing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can catch an enemy's ball by tapping or grabbing it while it is still visible, and holds it as a seed | partial | `MagicSystem.cpp`; scenario `miracles.fireball_catch`. The catching hand is always player one's, as openblack has no other human players |
| A player can't catch his own ball | done | `MagicSystem.cpp` |
| Pressing with a ready fire seed over a ball takes it in: the seed stays in the hand, a twentieth stronger | done | `MagicSystem.cpp`; scenario `miracles.fireball_absorb` |

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball is a flaming sprite with a trail of ten sprites, including a head sprite on the ball | done | `ParticleFireballRules.cpp` (all trail atoms drawn, fixed in the firewater round) |
| The trail follows the drawn ball, not its true path | done | `ParticleFireballRules.cpp` |
| Each ball leaves flames and smoke on the land (fewer with the second power-up) and lights the land below, fainter the higher it flies | done | Particle data, `src/Particles/ParticleUpdateRules.cpp` (fade with height) |
| A ball hitting water makes a ring; there is no steam cloud | done | `ParticleFireballRules.cpp`; scenario `miracles.fireball_water` |
| Every new effect steps twice on its first turn, so it starts as early as in the game | done | `src/Particles/` effect start (cross-group fix) |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each ball plays a throw whoosh, small, medium or large by its speed | done | `ParticleFireballRules.cpp`; checked in game |
| The plain ball's whoosh fades when it ends, the power-ups' stop at once | done | `src/Particles/ParticleSoundRelease.cpp`; test `test_particle_sound_release.cpp` |
| Hitting the land plays a hit sound by surface and impact speed; hitting water plays its own splash | done | `ParticleFireballRules.cpp`; scenario `miracles.fireball_water` |
| A ball flying past within 40 m of the camera plays a fly-by once, only while the ball exists | done | `ParticleFireballRules.cpp` (fixed in the firewater round) |
| Burning things crackle, the nearest fire holding the loop | done | `FireSystem.cpp`. See ../physics/fire.md |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Balls in flight and their fires are kept in a saved game | todo | openblack has no save games |
