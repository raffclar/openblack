# Impact damage

What happens to people, animals, the creature, buildings, rocks and other things when a moving body strikes them or
they strike the ground: crush damage judged by how hard the blow was, buildings cracking where a rock hits, rocks
chipping and splitting, and the feedback (sounds, the thrower's credit, the creature copying the player). How bodies
move and collide is in [object_dynamics.md](object_dynamics.md) and [collisions.md](collisions.md), thrown villagers and
animals in flight in [thrown_living.md](thrown_living.md), and throwing itself in
[throwing_and_landing.md](throwing_and_landing.md).

openblack (`physics` branch) measures every blow the game's way (`DynamicsSystem` turn end, `src/Physics/TurnRules.cpp`)
and hands it to each kind's impact reaction (`PhysicsGameHooks::ReactToImpact`): people and animals are crushed
through the effect path, the creature is hurt and angered, rocks wear and split (`src/ECS/ObjectPhysics.cpp`), and
resources go into stores. Rocks break buildings into pieces through `BuildingDamageSystem` and `src/Physics/DamageMesh.*` (test `test_damage_mesh`); over a broken building the whole model is drawn cut at its repaired share, scaffold, inner walls and the cap over the cut walls included (`da506ca5`, `d39f8402`). Tests: `test/test_physics_manager.cpp`.

**Progress: 42/47 done, 4 partial — 94%**

## How a blow is measured

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At the end of each game turn (after all physics steps), every body whose total force over the turn's touched steps (gravity included) is above a tiny limit gets an impact strength: that total, scaled by 0.05 | done | `turn::Impact` in `DynamicsSystem::ProcessTurn`; test `PhysicsTurn.ImpactIsTheMeanForceAndIgnoresTheSmallest` |
| Each body also remembers which other body hit it that turn, and the player whose hand threw it (taking the hitter's when it has none) | done | `PhysicsEntry::hitBy` and the credit, `DynamicsSystem::ProcessTurn` |
| A blow is judged in multiples of the body's own weight under gravity ("how many g", about 0 for something lying still); a heavy body needs a harder blow to be hurt | done | `ImpactInfo` G passed to the kinds' reactions |
| Every object with a model that is hit by moving things takes part in collisions while something moves near it, so a thrown rock can strike a standing villager or a house (standing trees are not hit) | done | Resting obstacles from the wake walk (`DynamicsSystem::ProcessTurn`) |
| An impact plays a collision sound picked by both materials, at one of three loudness levels by how many g it was | done | See [throwing_and_landing.md](throwing_and_landing.md) "Landing effects" |
| A thrown thing that can be used on what it hit (an animal or a tree into a store, food or wood into a store or a pile of the same; a toadstool poisons the store's food) is used on it instead of hurting it | partial | Trees, dead trees, fences, mushrooms, animals and pots go into a storage pit they meet, a pot into a pile of the same, and a thrown toadstool poisons the store's food (`PhysicsGameHooks::ReactToImpact`, `ResourceStoreSystem::TakeObject`, tests `test_store_rules`); an animal a store refuses is hurt as usual; worship sites and workshops, which the game also counts as stores, don't exist in openblack. See [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |

## People and animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager or animal is hurt only by a blow above 2 g | done | `living::LivingCrush` (`src/Physics/LivingRules.cpp`), `PhysicsGameHooks::ReactToImpact`; test `test_physics_living` |
| The harm is the game's crush effect scaled by (g − 2) × 0.03, applied by what hit it with the thrower's player, through the body's own defences | done | Crush preset × (g − 2) × 0.03 through the effect path with its defences, applied by the striking thing with its thrower's player (`HurtLiving` in `PhysicsGameHooks.cpp`, `magic::EffectSource::appliedBy`); with no thrower no player is credited |
| The same rule is the fall damage: a villager or animal thrown hard enough is hurt or killed by its own landing | done | The landing's knock is the blow (`PhysicsGameHooks::ReactToImpact`); checked in game: a villager thrown up at 30 m/s lost 0.44 life on landing |
| A villager that cannot take effects (not available, at home, in a hand, or hiding in a building) is not hurt | done | `TakesBlows` in `PhysicsGameHooks.cpp` (at home, in a hand, hiding in a building) |
| A villager whose life reaches nothing while standing dies as any villager dies, credited to the thrower's player; one hurt to nothing in flight dies when it lands | done | In flight it dies where it lands, put down to the thrower (`villager_physics::Land`); standing, an effect's death is killed by a spell, put down to the effect's player (`world_objects::EffectDeath`, `732578c9`) |
| An animal killed by a blow dies (it falls and lies; in flight it starts dying when it lands), it does not just vanish | done | Animals killed by any effect fall dead (`AnimalSystem::SetDying`); one killed in the air starts dying where it lands (`LandAnimal` in `PhysicsGameHooks.cpp`) |
| The harm moves the thrower's alignment by the crush it did, weighted by the victim's alignment kind | done | The thrower's player is passed to the effect path, which moves its alignment (`magic::EffectAlignmentChange`) |
| Hurting a town's villager makes the town count the thrower's player as an attacker | done | Through the effect path's aggressor (`world_objects::AttackTown`) only when something applied the harm; a plain fall with no thrower credits nobody (`7f763cbe`) |
| A town keeps a count of its injured people: it rises when a villager's life falls below 0.7 and falls when it climbs back above | done | `Town::injured`, counted at the strict 0.7 crossing on every villager life change (`world_objects` life change, `living::InjuredChange`); test `test_physics_living` |
| Villagers nearby react to someone crushed | done | The crushed reaction comes through the effect path from the striking thing (`magic::EffectSource::appliedBy`), from the victim when nothing struck |
| A villager or animal standing in the way is knocked into the air by any blow its resting body can't hold (the scripted 30 m/s launch happens only through the temple's redirect) | partial | The knock into flight is done (`DynamicsSystem` knocked result, villagers enter FLYING in `PhysicsGameHooks`); the temple's redirect launch isn't ported |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature struck by a thrown body takes crushing harm of how many g the blow was for its mass, capped at 100, times 0.01 (a toy does nothing) | done | `living::CreatureMass`, `living::CreatureCrush` (`src/Physics/LivingRules.cpp`), `HurtCreature` in `PhysicsGameHooks.cpp`; test `test_physics_living`. Toys do nothing: the toy statics and the football (`physics_classes::IsToy`, `dc312635`); see [../nature/toys.md](../nature/toys.md) |
| A thrown thing striking a creature while it fights takes none of its fight health or life and doesn't make it reel | done | `MagicLiving.cpp` (`EffectSource::blow`, `817020d2`); see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |
| A creature a script controls, belonging to the player, isn't hurt by blows | done | `ScriptControlled` from the script object table (`ScriptObjectsSystem`, `410656e9`) spares the local player's creature from blows (`PhysicsGameHooks`); see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |
| Blows worth less than 0.005 are ignored | done | `living::CreatureCrush`; test `test_physics_living` |
| A creature is never killed by blows: at no life it faints | done | Blows reach it now (`HurtCreature`); `world_objects::ReduceLife` knocks it out at no life |
| When its own player hit it, the creature thinks less of its player by the harm done | done | `HurtCreature` → `CreatureMindSystem::UpdateAttitudeFromFeedback`; as in the game, the player tested is the creature body's own credit. See [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |
| Being hit feeds its fear and its anger from being damaged by the harm done (out of a fight, a second time through its cut-and-scar code) | done | `HurtCreature` → `ChangeDesireSource` (fear and anger from damage); out of a fight the game adds them a second time in its cut-and-scar code unless its own player applied the effect, and openblack keeps that quirk (`MagicLiving.cpp`, `817020d2`) |
| A hit sways the creature's upper or lower body by the blow's sideways force (except in some of its animations) | done | `src/Creature/CreatureSway.*` (kick at the striker's centre, the spring, drawn with lean clips 194–197 by `AddLayer`), `CreatureAnimationSystem::KickSway`; test `test_creature_sway`; only the destroying blow's exemption is mapped (the other exempt states have no known meaning) |
| A creature's own throwing (at buildings, villagers, the camera, the sea) uses the same blows | done | What the creature lets go of goes through the hand's release into the physics (`CreatureObjectActionSystem` → `DynamicsSystem::LetGoFromHand`), so its throws strike with the same blows |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only rocks (and the bowling ball) damage buildings; anything else thrown at a house just bounces off | done | Rocks and the bowling ball "break buildings" (`PhysicsClasses`); the building's reaction is `BuildingDamageSystem`; see [../nature/toys.md](../nature/toys.md) |
| The blow is judged by momentum (speed × weight): up to 300 nothing happens | done | `BuildingDamageSystem` momentum bands; test `test_damage_mesh` |
| From 300 to 1000 a light knock sound plays, from 1000 to 2000 a heavier one, and the building is not hurt | done | `BuildingDamageSystem` knock sounds |
| Above 2000 a built building cracks: the part of its model within the rock's size plus 0.7 m of the line through the impact along the rock's motion breaks away | done | `src/Physics/DamageMesh.*` (size classes, longest-edge halving, majority at the smallest, 2048 caps); test `test_damage_mesh` |
| The broken part flies off as its own piece, with 0.3 of the rock's velocity and a small random spin, and then lies as debris | done | `BuildingDamageSystem` pieces with 0.3 of the rock's velocity and the game's random spin order; drawn as slabs |
| The building's damaged model is kept, so later hits keep breaking it down; a building repaired past 0.2 life gets a fresh one | partial | The damaged model is kept and made afresh when struck again while drawn between 0.2 and 1 repaired (`BuildingDamage`, `RepairSite`); over it the whole model is drawn cut at the repaired share with its inner walls and the scaffold rising and cut (`damage::PartialBuildOf`, `u_keepBelow`/`u_inset`, `da506ca5`); the repair start resets on any hurt (`6afe6217`); the cut walls are capped, each cut segment of the outer wall joined to its inner twin, unlit at (object colour × 3) >> 2, inner walls and cap only for a primitive with a whole triangle below the cut and no cap past 500 segments (`graphics::partial_build_cap`, test `test_partial_build_cap`, `d39f8402`); the inset is in the model's own units, as the game sets it in before its matrix; open: openblack's villagers don't repair buildings, and a land-following model isn't capped |
| The building loses life to match the share of its model knocked away (crush, through its defence), with a big crash sound | done | Crush through its defences brings life to the share left, with the crash sound, credited to the striking rock's player (`BuildingDamageSystem`, `6afe6217`) |
| A building completely broken is destroyed (its ruins and rebuilding are in [../building/](../building/)) | done | `BuildingDamageSystem` through `world_objects::ReduceLife` / `DestroyedByEffect` |
| A building damaged this way empties: its people come out | done | `world_objects::ReduceLife`, now reached by blows (`BuildingDamageSystem`) |
| Storage pits and the village centre take blows as other buildings do; scaffolds and totem statues ignore them | done | Abodes, storage pits, the village centre and spell dispensers take blows (`PhysicsGameHooks`, `BuildingDamageSystem`); scaffolds don't exist in openblack, see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| A local player's throw that leaves less than 40% of an on-screen building shows the help sprites about destroying buildings | todo | openblack has no help system; the spot is marked in `BuildingDamageSystem` |
| A creature's blow on a house breaks it like a rock's | done | The creature's blow breaks the building at its own position, straight down, out to its size × 3.75 (`CreatureObjectActionSystem`, `BuildingDamageSystem`) |
| Rocks thrown at an enemy temple are first redirected, with a plasma beam, to one of its owner's buildings (towns oldest first, each town's buildings newest first; life over 0.25 preferred) or else a homeless villager, the redirected hit carrying no speed; otherwise they hurt it by speed × weight × 0.000005, at most 0.2 a blow, and the harm passes to its heart | partial | `StrikeHeart` in `PhysicsGameHooks.cpp`, `physics::temple_heart` (choice and harm, test `test_temple_heart`), towns' gained order and newest-first building and homeless lists, the passed-on building blow with no speed on the damage mesh's last primitive (`BuildingDamageSystem::ReactToPassedOnImpact`, `damage::RandomSurfacePoint`, test `ABlowPassedOnLandsOnTheLastPrimitive`), the homeless villager flung up at 30 with spin (1, 0, 1), the heart's harm through its defence (`894d3474`); open: the plasma beam (the `UR_Plasma` particle rule and its 20-turn limit and sample), temple parts, and the destruction once the heart has no life. See [../temple/](../temple/) |

## Trees, rocks and other things

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thrown things do not knock trees or dead trees down; only things meant to be used on them act | done | Standing trees aren't obstacles (`physics_classes::IsObstacle`); no tree reaction hurts them |
| A rock that lands harder than 4 g, if taller than 0.7 m and not hit by another rock, loses a little life, (g − 4) × 0.005 | done | `object_physics::KnockRock` (`src/ECS/ObjectPhysics.cpp`), `src/Physics/ObjectRules.cpp`; test `test_physics_objects` |
| A rock worn below 1% life splits into two smaller rocks (a burning rock's fire goes to both) | done | `object_physics::SplitRock` (halves of 0.7935 its size with its velocity, half its spin and its fire, `FireSystem::CopyFire`); checked in game |
| Broken pieces, bonfires, scaffolds and totem statues have no reaction to blows | done | No reaction exists for them (`PhysicsClassHooks::ReactToImpact` default); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Rocks thrown at a physical shield cost it power and anger the town it guards | done | `MagicShieldSystem::Impact`; see [../miracles/physical_shield.md](../miracles/physical_shield.md) |

## Feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a person or rock thrown from the hand (not a tree) feels any blow, the player's creature may learn "damage by throwing" from it | done | `PhysicsGameHooks::ImpactFeedback` → `CreatureMindSystem::PlayerDid` (damage by throwing) |
| When it was hit by something, it is also "damage by throwing at" | done | `PhysicsGameHooks::ImpactFeedback` (damage by throwing at) for thrown people and rocks; the building's version of this branch never fires in the game (a building's resting entry never carries the from-hand flag), and none is claimed here |
| Challenge scripts can ask what was last hit and by what, and clear it | done | `DynamicsSystem::RecordHit`; `CHLApi.cpp` GET_HIT_OBJECT, GET_OBJECT_WHICH_HIT, CLEAR_HIT_OBJECT |
