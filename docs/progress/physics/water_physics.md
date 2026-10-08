# Water physics

What happens to things that fall into the sea: the splash, floating or sinking, bobbing and drifting, soaking up water
until they go under, and villagers and animals drowning. The sea itself is in [../ocean/](../ocean/); what a drowning
does to a villager's family and town is in [../villager/](../villager/).

openblack (`physics` branch) floats and sinks bodies in the game's own physics: the sea test, buoyancy, water drag and
soaking are in `src/Physics/Body.cpp` (sea cells from `src/ECS/PhysicsGround.cpp`), and `DynamicsSystem` makes the
water rings (`WaterRingSystem`), the foam puffs and the water sound of a splash, and the bobbing rings. A sunk villager
drowns for its type's drowning time and dies (`villager_physics::Sink`, `Drowning`), a sunk animal dies and is gone
(`PhysicsGameHooks::HasSunk`), and a tree that ends in the water becomes a dead tree (`object_physics::EndTree`). Tests: `test/test_physics_engine.cpp`,
`test/test_physics_manager.cpp`.

**Progress: 26/27 done, 1 partial — 98%**

## Falling into the water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body is in the water when the land under its centre is at sea level, its centre is less than one radius above the sea, and the map cell under its first point has no land | done | `Body::TouchGround`, `PhysicsGround::IsSeaCell`; test `PhysicsBody.TheSeaIsFeltUnderTheFirstPointOverLandAtTheSeasLevel` |
| Only the sea at height 0 counts as water for the physics: raised lakes are not water to it | done | Same test; the surface is world height 0 |
| Hitting deep water plays the water collision sound, six foam puffs and a ring, and makes fish within 8 m dart away | partial | Sound, foam and ring done (`DynamicsSystem` turn end, `turn::` rules); openblack has no fish shoals to scatter |
| Hitting the water makes a ring that grows to twice the body's radius, faster for small bodies | done | `WaterRingSystem` ring at y 0.1, growth 2R, rate 1/R; test `PhysicsTurn.RingsOnTheWater` |
| Six foam-coloured puffs rise where it hits the water | done | Dust puffs in the foam colour (`DynamicsSystem`); test `PhysicsTurn.DustPuffsGrowThenShrinkOverASecond` |
| Over the shallow shore it makes both a ring and dust, with the ground's sound | done | `DynamicsSystem` turn end (dry land / shallow / deep tests in `PhysicsGround`) |

## Floating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The water lifts a body by how deep its centre is (as a share of its size) times its weight, divided by its density: lighter than water floats, heavier sinks | done | test `PhysicsBody.LightBodiesFloatAndHeavyOnesSink` |
| The water slows a body a hundred times more than the air does, by how much of it is under | done | `Body::TouchGround` |
| The water's push acts at the centre; only its turning effect is shared among the points under the surface, so floating things tilt and settle | done | `Body::TouchGround` |
| A floating body bobs; whenever its vertical speed turns round while its centre is below half its radius above the sea, it makes a small ring | done | `DynamicsSystem::ProcessTurn`; test `PhysicsTurn.SinkingAndBobbing` |
| Floating things drift only with their own speed: there are no currents or wind | done | No such force exists in `Body` |
| While under the sea a body never touches the sea floor | done | `Body::TouchGround` |

## Soaking and sinking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Things soak up water: their density rises a little every step they are in it (about 0.013 a second), so floating things sink in the end | done | test `PhysicsBody.FloatingThingsSoakUpWaterAndSinkInTheEnd` |
| How long things float: wood in the hand about 15 s, trees about 20 s, carried food about 30 s, the lightest toys over a minute; animals go under in a few seconds and villagers almost at once; rocks sink straight away | done | Follows from the materials and the soaking rate (`MaterialTable`, `Body`); the bowling ball (density 2.04) sinks and is deleted; see [../nature/toys.md](../nature/toys.md) |
| Something has sunk when it is denser than water and its centre is less than half its radius above the sea | done | `turn::` sinking test in `DynamicsSystem::ProcessTurn`; test `PhysicsTurn.SinkingAndBobbing` |
| Ordinary objects that sink keep going down and are deleted deep under the sea | done | `PhysicsClassHooks::HasSunk` is false for them; −4 R deletion |
| An animal that sinks dies and is gone (the creature may learn "throw in the sea" from it) | done | `PhysicsGameHooks::HasSunk` (the dropper's creature may learn "throw in the sea"); the game's own check that it is still there first isn't made |
| Carried food or wood that comes to rest in the water stays a floating object rather than becoming a pile | done | Ordinary end of physics (`DynamicsSystem::EndPhysicsAsObject`); becoming a pile on land is in [throwing_and_landing.md](throwing_and_landing.md) |
| A tree that ends in the water is not planted again but becomes a dead tree | done | `object_physics::EndTree` (`src/ECS/ObjectPhysics.cpp`) |

## Drowning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager that sinks stops where it is and starts drowning (one already dead goes to dying instead) | done | `villager_physics::Sink` (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`) |
| A villager that lands on a water cell, the shallow shore included, drowns too; one already dead just dies | done | `villager_physics::Land` |
| A drowning villager struggles for its type's drowning time (600 turns, 60 s, for every villager type in info.dat), then dies drowned | done | `villager_physics::Drowning`, `living::StepDrowning` (info `drowningTime`); checked in game: drowned for 600 turns then died |
| The drowning is credited to the player who dropped or threw the villager, else to the last player who interacted with it | done | `villager_physics::DropperOf` (dropper, else the last interacting player) into the death's cause (`villager_fire::DeathCause`, `7f763cbe`) |
| An indestructible villager never finishes drowning (its count is reset to 10 each turn) | done | `Indestructible` (SET_INDESTRUCTABLE, `CHLApi.cpp`) → `living::StepDrowning` holds the count at 10 |
| A drowning villager plays its drowning animation, with swimming splashes and a scream | done | The drowning clip 252 is drawn (`VillagerPose`, `VillagerPosing.cpp`) and its splash and scream events come from `Data/SmallSounds.SAS` (`components/sas`, `ClipSoundPlayer`, test `test_clip_sounds`), fired with the game's own wrap rule for clips that don't loop |
| Scripts can ask whether something is drowning: a villager in the drowning state, or an object in the physics with its centre under the sea | done | GET_PROPERTY answers drowning: a villager in its drowning state, or anything else in the physics with its body's centre below sea level (`CHLApi.cpp`, `732578c9`) |
| Throwing a villager or animal into the sea is something the creature can learn from | done | `villager_physics::Sink`, `PhysicsGameHooks::HasSunk` → `CreatureMindSystem::PlayerDid` (throw in the sea) |
