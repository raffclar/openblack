# Explosions

Blasts in Black & White: the engine pieces every explosion shares (an area burst of heat and crushing, a shock wave
that runs outward and shatters what it reaches into flying pieces, rubble and scorch heaps, dust puffs, smoke or steam,
camera shake and the bang), and the other things that go off with a bang. The game has no exploding barrels or fire
explosions: fire only flares into extra flames when very hot (see [fire.md](fire.md)). The blast miracle's own timings,
power-ups and casting are in [../miracles/blast.md](../miracles/blast.md); the fireball's impact in
[../miracles/fireball.md](../miracles/fireball.md); lightning strikes in [../miracles/lightning.md](../miracles/lightning.md).
Pieces knocked off a building by a thrown rock are in [impact_damage.md](impact_damage.md).

openblack: the explosion rules in `src/Particles/ParticleBlastRules.cpp` and `src/Particles/ParticleBlast.cpp`, the wave's
effect on objects in `src/ECS/Systems/Implementations/ParticleObjectEffects.cpp`, rubble, dust and shake in
`src/ECS/Systems/Implementations/ExplosionSystem.cpp`, `src/Blast/CameraShake.cpp`, `src/Blast/DustPuff.cpp`, tests in
`test/test_blast.cpp`. These were audited against the game (audit "firewater") and fixed.

**Progress: 38/43 done, 3 partial — 92%**

## The burst

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An explosion acts at the land under it (sea level over water); it starts acting after its delay and keeps acting for its set time | done | Test `Blast.ItActsFromItsInitialDelayForItsTimeToDoEvents` |
| Every turn while it acts it applies its burst: heat (which sets things alight), crushing and a blow, over a fixed radius | done | `ParticleBlastRules.cpp`; heat feeds [fire.md](fire.md) |
| The burst is the same everywhere inside its radius: there is no falloff with distance | done | Flat area effect (`src/Magic/AreaEffect.cpp`); audited as matching the game |
| The burst's strength scales with what the caster paid, the tribe's power and the event's own strength; the radius does not | done | `magic::EventEffectValues` |
| A burst hurts through each thing's defences, moves the caster's alignment and angers the towns it harms | done | `magic::ApplyEffectTo`; see [../worship/](../worship/) |
| Villagers near an explosion react once for the whole explosion, not once per turn | done | `MagicSystem.cpp` (one reaction per spell) |
| A shield over the explosion is struck where the way down enters it, and can stop it | done | Test `Blast.AShieldOverTheBlastIsStruckWhereTheWayDownEntersIt` |

## The shock wave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wave front runs outward at a set speed and reaches each object when it touches the object's edge | done | Test `Blast.ItsWaveReachesFurtherWithTribalPowerAndReachesObjectsByTheirEdge` |
| The wave's reach grows with the caster tribe's power (1 to 5 times) | partial | Formula done; tribal power is always 1 in openblack. See [../worship/](../worship/) |
| The wave takes only things fixed on the map, at most one a turn, and only those it may destroy | done | `ParticleObjectEffects.cpp` |
| Creatures, fields, temples, teleport stones, totems, totem statues, worship totems, pots and one-off seeds are never destroyed by it | done | `world_objects::CanBeDestroyedBySpell` (fixed) |
| Things a script made indestructible, or that the help system holds, are spared | partial | Indestructible things are spared (`Indestructible`, `world_objects::CanBeDestroyedBySpell`); openblack has no help system to hold things |
| A building reached is shattered into pieces but left standing as an empty shell with no life; this includes spell dispensers | done | `world_objects::Destroy` (fixed) |
| Anything else reached (trees, statues, small objects) shatters and is removed | done | Same |
| Villagers are never shattered; they take only the burst's heat and crushing | done | Scenario `miracles.blast` |
| A shield in the wave's way is struck at the point it is reached, with a spark and the wave's direction | done | Fixed (`ParticleBlastRules.cpp`) |
| At most a set number of things shatter and a set number are removed per explosion | done | `ParticleBlastRules.cpp` |

## Shattering into pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A shattered model breaks into strips of up to 16 triangles that touch along a shared edge, even across texture seams | done | Test `Blast.AModelBreaksIntoChainsOfAtMostSixteenJoinedTriangles` (fixed to join by position) |
| Each piece flies away from a point 5 m below the centre at the blast's speed, with some randomness | done | Test `Blast.PiecesFlyOutFromBelowTheCentreAtTheBlastsSpeed` |
| Pieces fall, bounce off the land, tumble, fade over 3 s, shrink from 1 to 5 s and are gone at 6 s | done | `ParticleBlastRules.cpp` |
| Pieces make no sound and no ripples when they land, even on water | done | Matches the data |
| Pieces are lit by the land and by their model's own light | done | Test `Blast.BrokenPiecesAreShadedByTheModelLightInTheirOwnFrame` |
| Objects waiting to be shattered are handled newest first | done | Fixed |
| Five rock pieces are thrown out from the centre's height | done | Fixed (`ParticleBlastRules.cpp`) |

## Marks, dust, smoke and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land counts as dry when it is more than 3 above sea level; the same test picks every dry or wet effect below | done | Fixed (`ParticleObjectEffects.cpp`) |
| On dry land a heap of rubble lies at the centre, at a random angle, following the land's shape, for 15 s, fading out over its last second | done | `ExplosionSystem::AddRubble` |
| A brown dust puff of 15 sprites flies up and out, growing and fading over 1.5 s | done | Test `DustPuff.FifteenSpritesFlyUpAndOutGrowingAndFadingOverASecondAndAHalf` |
| Grey smoke rises a little after the bang for a few seconds | done | Smoke particle effect |
| On water there is no rubble or dust: three rings spread and white steam rises instead | done | Scenario `miracles.blast_water` |
| The land itself is not burnt or blackened by explosions or fire | done | No scorch on the land in the game; the rubble heap is the only mark |

## Shake and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An explosion within 200 m of the camera shakes it up and down (the eye and what it looks at), fading over 0.7 s | done | Test `CameraShake.OnlyTheNearestCountsAtFullStrengthWithinItsRadiusFallingOverItsTime` |
| Only the nearest shake counts; between two at the same distance the newest wins | done | `CameraShake.cpp` |
| Shaking is always on; there is no option to turn it off | done | Matches the game |
| Each explosion has a whoosh as it comes down and one of two bangs as it hits, at a random pitch | done | Scenario `miracles.blast` |
| A new effect starts with a double step, so the bang lands on the same turn as in the game | done | "a new effect steps twice at first" |
| Scripts can shake the camera at a point, with a radius, strength and time | done | SHAKE_CAMERA (`CHLApi.cpp`, `410656e9`): all axes, duration rounded to whole milliseconds |
| A reward chest reaching the ground makes a thump and a short shake | done | When the chest reaches the ground: sample 174 and an all-axes shake of radius 200 for 400 ms centred on the world's origin, as the game does (`RewardSystem`, `158404fb`); test `test_reward` |

## Other bangs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can play special effects such as the "bang" smoke burst and the temple explosion at a place or on an object | done | SPECIAL_EFFECT_POSITION / SPECIAL_EFFECT_OBJECT (`CHLApi.cpp`, `410656e9`): spot visual 0–49, seconds × 10 turns, magnitude 1, the local player; the script gets the effect's id rather than a table place |
| Fireworks go up and burst with their own sounds, started by scripts and when a player takes a town | partial | Scripts can start them (special effects); the game also starts town fireworks when a non-neutral player takes a town, and openblack never changes a town's owner (R15, R19) |
| A temple whose heart is destroyed goes through a destruction sequence with explosions and plasma | todo | See [../temple/](../temple/) and [../multiplayer/](../multiplayer/) |
| A failed miracle cast gives a small puff | done | See [../miracles/casting_and_globes.md](../miracles/casting_and_globes.md) |
| The creature smashes rocks in half by hand | done | `object_physics::SmashRock` (rock-tap 139+n, then the split) from the creature's blow (`CreatureObjectActionSystem`) |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Explosions in progress (wave front, waiting pieces) are saved and loaded with the game | todo | openblack has no saved games yet |
