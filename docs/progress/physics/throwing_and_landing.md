# Throwing and landing

The physics side of letting go of something: the speed it leaves the hand (or the creature's hand) with, whether it is
put down or thrown, its flight, and what each kind of object does when it lands. Grabbing, holding and the hand's states
are in [../hand/throwing.md](../hand/throwing.md). Damage done by what lands is in [impact_damage.md](impact_damage.md); burning objects that
start flying leave their fire group (see [fire.md](fire.md)). No craters or ground marks from landings exist in the game.

openblack (`physics` branch): the hand picks things up and lets them go through the game's own release
(`src/ECS/Systems/Implementations/HandGrabSystem.cpp`, `src/Hand/HandGrabRules.cpp`,
`src/ECS/Systems/Implementations/DynamicsFromHand.cpp`) into the game's physics (`DynamicsSystem`), with landing sounds,
dust and rings. The kinds' own landings are in `src/ECS/ObjectPhysics.cpp` (trees replanted or
dead, piles), `VillagerPhysics.cpp` and `PhysicsGameHooks.cpp` (postures, falls, drowning, stores), and the creature lets
go through the same release (`CreatureObjectActionSystem`). Artefacts are kept by towns ([../town/artefacts.md](../town/artefacts.md)), reward chests
fall in `RewardSystem`; scaffolds have no openblack objects yet ([../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md)). Tests: `test/hand/test_hand_grab.cpp`, `test/hand/test_hand_grab_system.cpp`, `test/test_physics_manager.cpp`.

**Progress: 40/45 done, 4 partial — 93%**

## Letting go from the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is held follows the hand through a spring updated every 10 ms (pull 260 times the gap, damping 40 times the speed); the throw speed is the spring's speed at the moment of release | done | `hand_grab::HandSpring` (`src/Hand/HandGrabRules.cpp`); tests `HandGrab.TheSpring*`, `HandGrabSystemWithWorld.TheSpringTakesHoldTheFrameAfterThePressAndThrows` |
| The throw speed is capped at 124 m/s | done | `HandSpring`; test `HandGrab.TheSpringSettlesOnItsTargetAndIsCapped` |
| The hand gives what it lets go no spin; 180 ms later whatever it let go that still has a body gets a one-turn twist of 1.6 × its mass × its speed about the level axis across the hand's motion | done | `hand_grab::ReleaseSpinTorque` in `HandGrabSystem::UpdateFrame`; tests `HandGrab.ATwistTurnsAboutTheLevelAxisAcrossTheHandsMotion`, `HandGrabSystemWithWorld.WhatIsLetGoGetsItsTwistAFifthOfASecondLater` |
| The object starts its flight from the pose it had in the hand | done | `DynamicsSystem::ReleasePose` (the release prediction's zero-turn body) |
| A release counts as a throw when its sideways speed is over 2 m/s (over 1 m/s from the creature); slower it is a drop | done | `hand_grab::IsThrow`, `DynamicsSystem::LetGoFromHand`; test `HandGrab.AThrowIsFastAcrossTheGround` |
| On release the object is moved up out of the ground; a dropped one is also lowered onto it and, unless it is a tree, turned to lie along the slope | done | `DynamicsSystem::LetGoFromHand` (`SettleOnLand`) |
| On release the object is raised until it no longer overlaps anything under it | done | `DynamicsSystem::RaiseClearOfWhatIsUnder`; see [collisions.md](collisions.md) |
| A drop that needed no raising, over dry land (or a land cell above the lowest heights), is put down where it is, without flying | done | `hand_grab::StandsWherePutDown`; test `HandGrab.APutDownStandsOnlyOnLandWithoutBeingRaised` |
| Villagers, animals and fences dropped on ground steeper than about 45 degrees are not put down but slide off in the physics | done | Same rule (land normal y ≥ 0.7); test `HandGrab.PeopleFencesAndUprightTreesLeaveThePhysicsOnLanding` |
| When the hand is forced to let go, the object is put down where it is held, with no speed, and never replanted (a tree dies) | done | `HandGrabSystem::ForceDrop` puts it down with dontReplant, so a tree becomes a dead tree (`object_physics::EndTree`); a scribble with a held thing inside the influence calls it (`GestureSystem`, `ShakeOffHeld`, test in `test_gesture_recognition`) |
| Everything the hand throws or drops is credited to its player, for kills, drownings and impressing | done | The credit rides on the body and passes on through hits (`PhysicsEntry`); blows, alignment, drownings' dropper, the creature's learning and deaths (`villager_fire::DeathCause`, `world_objects::EffectDeath`) read it |
| A thrown (not put down) object makes nearby people and animals react once: villagers look or point (reach 25 m), and any animal nearer than twice the object's speed runs off | partial | The reaction follows the object; villagers run or point and animals nearer than twice its speed flee (`VillagerPhysics`, `AnimalSystem::SetupReactToFlyingObject`); sheltering villagers react only to things inside the shield (`living::ShelterSeesFlyer`, `6afe6217`). Open: dancers' rule has no dance groups to hang on |
| There is no size limit on throwing: rocks wider than 3.6 can't be picked up at all, and anything the hand holds can be thrown | done | `hand_grab::ValidForPlaceInHand`; test `HandGrab.RocksWiderThanTheHandCanLiftStay` |
| A slow release of carried food or wood (speed squared at most 5) puts it straight down as a pile, or into a store close by | done | `HandGrabSystem::LetGo` → `GameHandGrabWorld::PourPot` (putdown particles, piles and stores within 3×3 cells, lost in water) |

## Creature throws

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thrown at a target, the flight time is the time it would take to fall the distance (square root of distance over gravity), and the release speed is the one that reaches the target in that time; a moving target is aimed ahead | done | `src/Creature/CreatureThrow.h` (`FlightTime`, `ReleaseVelocity`); `test/test_creature_interaction.cpp` |
| A creature throws nothing at a target nearer than two thirds of its height | done | `CreatureThrow.h` (`FarEnoughToThrow`) |
| Tossed away, it leaves with 0.6 of the hand's own speed over the last 100 ms of the animation, turned with the creature and mirrored for the other hand | done | `CreatureThrow.h` (`TossVelocity`, `HandVelocity`) |
| Put down, it is placed at the hand's height above the ground with no speed, then let go through the same release as the hand's (settled, raised, landed test) with a spin of one about the vertical | done | `CreatureObjectActionSystem` lets go through `DynamicsSystem::LetGoFromHand` with the creature as thrower and a spin of one about the vertical |
| What the creature throws then flies, bounces and rests in the same physics as hand throws | done | `CreatureObjectActionSystem` → `DynamicsSystem::LetGoFromHand`; the old point flight is deleted |

## Landing, by kind of object

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rocks and other fixed objects stay where they come to rest | done | Ordinary end of physics (`DynamicsSystem::EndPhysicsAsObject`) |
| A rock or other artefact gently put down from the hand by a player within 50 m of a town's building or a worship site becomes that town's (or site's) artefact, and impresses it when it is worth it | partial | `object_physics` artefact landing (`9908a856`): a gentle put-down from the hand by a non-neutral player makes it the artefact of the nearest building's town within 50 m, worth its villager-impressing value, other towns looking at it when worth more than 1; never a creature's put-down; taken out of its town when picked up. openblack has no worship sites to take artefacts. See [../town/artefacts.md](../town/artefacts.md) |
| A tree put down gently on land, held upright (tilted less than about 0.2 radians) and not burning, is planted again with a planting sound, joining a forest nearby or starting one | done | `object_physics::EndTree`: replanted standing on the land with the smoke puff, the 35 m spiral forest search (a town's scenic forest near a town, `bf1c879d`), the spot visual, mimic, alignment and planting sound |
| A tree thrown, dropped tilted, dropped in water or burning falls and becomes a dead tree lying where it stops | done | `object_physics::BecomeDeadTree` keeps its pose, wood multiplier and fire (`FireSystem::MoveFire`), drops its roots (`FallingRoots`), and only its script place moves to the dead tree, which is in a script again only at its next reference and never controlled by it (`ScriptObjectsSystem`, `d39f8402`) |
| Trees planted by the forest miracle that are thrown just stay where they fall, neither replanted nor dead | done | `object_physics::EndTree` (magic trees stay) |
| A tree or dead tree that hits a wood store goes into it as wood | done | `PhysicsGameHooks::ReactToImpact` → `ResourceStoreSystem::TakeObject`; amounts per `StoreRules` (test `test_store_rules`) |
| Thrown carried food or wood that comes to rest on land becomes a pile; in the water it stays a floating object | done | `object_physics::EndPot`: on land it spills into nearby stores and piles or a new pile, a poisoned handful poisoning them; in water it floats on. See [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Thrown food or wood that hits a store, or a pile of the same thing, goes into it | done | `PhysicsGameHooks::ReactToImpact` (store take, or `AddToPile` for a pile of the same) |
| Fences and the three mushrooms that hit a store taking their resource go into it | done | `PhysicsGameHooks::ReactToImpact` → `ResourceStoreSystem::TakeObject`; an indestructible fence stays out; mushrooms give their info food value and only the toadstool poisons the store's food; see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| An animal goes into any store that hits it or that it hits, as much as the animal is worth | partial | Into storage pits by its food value (`ResourceStoreSystem`), and hurt as usual when refused; the game's worship sites, also stores, don't exist in openblack |
| A villager or animal lands lying on its back, its front or its feet by how it is tilted, then gets up | done | Postures, headings and landing clips (`living::*LandingPose`, `villager_physics::Land`, `AnimalSystem`); see [thrown_living.md](thrown_living.md) |
| A scaffold put down gently snaps to a planned building of the town or starts building there; otherwise a little effect plays where it lands | todo | openblack has no scaffolds; in the game only a gentle put-down builds (a thrown one lands with a puff), one dropped within 2 m of a yard place snaps back, and its scale is 0.5 + 0.2 × worth; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Pieces broken off buildings land on the ground only and vanish after 100 turns per triangle | done | `BuildingDamageSystem`: pieces hit only the land, last 100 turns per triangle, and large ones (area over 9) that come to rest join their standing building as rubble |

## Landing effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing or hit plays a sound picked by what hit what: each object has a collision sound type, against the ground, the water or the other object's type | done | Collision code tables and the editor sound bank (`DynamicsSystem`, `turn::` rules) |
| The sound is soft under 1.25 G, hard over 3 G and medium between (by the unscaled info weight); grain is never hard | done | test `PhysicsTurn.LoudnessByHowManyWeightsTheKnockWas` |
| A sound plays only when the body hit another object or hit with more than half its weight, and the same pair stays quiet for 2 turns (at most 128 pairs at once) | done | tests `PhysicsTurn.KnocksSoundWhenHitOrHardEnough`, `APairStaysQuietForTwoTurns` |
| The sound follows the object it belongs to | done | test `PhysicsTurn.CollisionSoundsFollowWhatMadeThemButOne` |
| A rock hitting a building lets the building play its own sound | done | The rock's own sound is skipped (`turn::` sound rules); the building plays its knock (momentum 300–1000, 1000–2000) or crash sound (`BuildingDamageSystem`) |
| Six dust puffs rise where it lands, as big as the object (at most 5 m), growing over an eighth of a second and gone after a second of game time; on snow they blend towards the land's colour by the snow's depth | done | Puffs done (`DynamicsSystem`, `blobs.raw`); on snow they blend towards the land's colour of the moment by the snow's depth (`src/ECS/SnowDust.*`, `05c66517`); test `PhysicsTurn.DustPuffsGrowThenShrink` |
| A body that passes within 10 m of the camera faster than 20 m/s makes one of five whooshing sounds | done | test `PhysicsTurn.FastThingsWhooshPastTheCamera` |

## Thrown things as weapons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A thrown object passes through its thrower for as long as it stays in the physics | done | Thrower pair rule (`PairRules.h`); creature throws still fly outside the physics |
| Whatever a thrown object hits carries the thrower's credit on to what it hits next | done | Turn end in `DynamicsSystem::ProcessTurn` |
| A thrown object that hits the creature hurts it by how hard it hit for the creature's mass (capped at 100 G, a hundredth of that as damage, nothing under a two-hundredth) | done | `HurtCreature` in `PhysicsGameHooks.cpp`; see [impact_damage.md](impact_damage.md) |
| A creature hit by something its own player threw likes the player less, and its fear and anger from being damaged rise | done | `HurtCreature`: attitude by the body's own credit as in the game, fear and anger; see [impact_damage.md](impact_damage.md) |
| Creatures try to catch objects flying towards them: at least 1 m/s across the ground, reachable within 5 s, and only 3% of the time when an unallied player threw it | partial | The offer and the catch are ported (`PhysicsGameHooks::OfferToCatchingCreatures`, `src/Creature/CreatureCatch.*`, `CreatureMindSystem::ForceCatch`, `CreatureObjectActionSystem` catch, tests `test_creature_catch`); eligibility, weight, forced plan, ready phase, either hand (`cf273ae0`), the run-back, busy-body wait and hand points per creature (`6afe6217`) follow the game; the waiting stand pose and the step's check over the block of its height, cells read a row on (`creature_route::StepBlockCircles`, `dc312635`) also follow it, and 'held by another creature' always holds as openblack's creatures hold none; open: the step check's circles of the objects in that block, which the game lays out per kind of object for its route planner and openblack's routes don't yet |
| Thrown objects bounce off a physical shield and cost it prayer power by their momentum | done | Everything bounces off the shield's body in the physics; rocks and the bowling ball cost it power by their momentum (`StrikeShield` in `PhysicsGameHooks.cpp`, `MagicShieldSystem::Impact`); the old mirroring is gone |
