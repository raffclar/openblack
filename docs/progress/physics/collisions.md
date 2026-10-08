# Collisions

What flying objects hit and how: the land, other loose objects, buildings, trees, people and the creature, and the edges
of the map. Contacts are springs with friction at every point of a body (see [object_dynamics.md](object_dynamics.md)).
What the hits do to life and buildings is in [impact_damage.md](impact_damage.md).

What a villager carries, and the log it lets fall when it is launched, is in [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md).

openblack (`physics` branch) tests the game's own way: each body's points against the land (`src/ECS/PhysicsGround.cpp`)
and against other bodies' faces or, for the creature, its bones' ellipsoids (`src/Physics/Body.cpp`), with the pair
rules in `src/Physics/PairRules.h` and the turn in `DynamicsSystem`. Bullet has been removed from openblack (user
decision 2026-10-08). Tests: `test/test_physics_engine.cpp`, `test/test_physics_manager.cpp`.

**Progress: 32/33 done, 1 partial — 98%**

## Against the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every point of a body is tested against the land height under it; the push is along the flat normal of the land triangle there | done | `PhysicsGround` (land height, flat triangle normal) used by `Body::TouchGround` |
| Against the land a body uses its own material's stiffness, damping and friction | done | `Body::ApplyContacts` |
| A thrown object is only ever pushed up out of the land when it leaves the hand, never pulled down to it | done | `DynamicsSystem::LetGoFromHand` settles a throw on the land without pulling it down (`SettleOnLand`); test `PhysicsBody.AdjustingToTheGroundOnlyRaisesWhenAsked` |
| A body over the sea stops touching the land while any of its points is under the water | done | `Body::TouchGround` sea branch; test `PhysicsBody.TheSeaIsFeltUnderTheFirstPointOverLandAtTheSeasLevel` |

## Object against object

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two bodies are tested only when they are closer than the sum of their radii; then each point of one is tested against the faces of the other | done | `physics::TestsPoints`, `Body::TouchFaces`; test `PhysicsPairs.WhoTestsAgainstWhom` |
| Between two bodies the softer stiffness and damping are used, and the friction is 0.3 times the smaller friction (things slide off each other easily) | done | `Body::ApplyContacts` |
| What pushes one body pushes the other back as hard, and turns it | done | test `PhysicsBody.TwoBodiesPushEachOtherEquallyAndOppositely` |
| Two resting bodies don't collide with each other, except in the step either was just put into the physics | done | `physics::TestsPoints` |
| A thrown object and its thrower pass through each other for as long as the object stays in the physics list | done | Thrower pair rule (`PairRules.h`) |
| Villagers' bodies pass through things a villager pushed or kicked, and those through villagers | done | Pushed-by-living pair rule; nothing pushes yet (see [object_dynamics.md](object_dynamics.md)) |
| What a released villager was carrying flies without hitting other objects | partial | The no-object-collision flag and pair rule are done; dropping the carried thing (`PhysicsClassHooks::DropCarriedResource`) is still empty |
| Contacts are tested 0.06 s ahead along the body's motion, so fast objects stop at thin walls such as the physical shield | done | test `PhysicsBody.LookingAheadStopsAFastBodyAtAThinWall` |
| Each body remembers what it last hit in the turn; what reacts to a hit knows what hit it, and a body thrown by nobody takes the credit of whoever threw what hit it | done | Turn end in `DynamicsSystem::ProcessTurn` (`PhysicsEntry::hitBy`, credit) |
| How hard a hit was is the mean of the body's total force (gravity included) over the touched steps of the turn; divided by its weight it gives the G of the hit (about 0 for something lying still) | done | `turn::Impact`; test `PhysicsTurn.ImpactIsTheMeanForceAndIgnoresTheSmallest` |

## What can be hit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rocks, statics, mobile objects, villagers, animals, dead trees and carried food or wood are obstacles to flying bodies | done | `physics_classes::IsObstacle` (`PhysicsClasses.cpp`); tests in `test_physics_manager.cpp` |
| Standing trees and forests are not obstacles: thrown things pass through them | done | test `PhysicsClassesTest.TreesFlyButAreNotHitWhileDeadTreesAre` |
| Fields are never hit | done | test `PhysicsClassesTest.FieldsAreNeverInThePhysics` |
| Food and wood piles are hit, unless they are part of a storage pit | done | test `PhysicsClassesTest.PotsFlyByTheirInfoAndAreHitUnlessInAStoragePit` |
| Buildings are obstacles only while more than 10% built and still standing (life above 0.01); ruins are not. The village centre always is; worship sites, graveyards, fish farms, football pitches, lanterns, bonfires and totem statues never are | done | `physics_classes::IsObstacle` with `BuildProgress` (a building without construction state counts as built); tests `BuildingsAreHeavyStaticObstacles`, `OtherStaticsStandAsBuildingsDo`, `LanternsAndTheSingingStonesBaseArePlainObjects` |
| A building is a heavy obstacle that never moves; a hit damages it instead | done | Heavy static body (`PhysicsClasses`), and a rock's blow breaks it (`BuildingDamageSystem`, `src/Physics/DamageMesh.*`, test `test_damage_mesh`); see [impact_damage.md](impact_damage.md) |
| Pieces broken off buildings hit only the land | done | Pieces are made with the no-object-collision flag and the building as their thrower (`BuildingDamageSystem`); test `test_damage_mesh` |
| The creature is an obstacle that is never moved by the physics: thrown things hit one ellipsoid per bone | done | Creature body from its posed bones (`shapes::Creature`, `DynamicsSystem`); test `PhysicsShapes.TheCreatureIsHitOnItsSkeleton` |
| A physical shield is always in the physics as a very heavy static dome (weight 50000 × its scale cubed) that thrown things bounce off | done | Shield body from its hull, always kept (`DynamicsSystem`, `MagicShieldSystem.cpp`); test `PhysicsClassesTest.OnlyThePhysicalShieldIsAnObstacle`. Creature throws still use the shield's own mirroring until they move into the physics |
| A spiritual shield and the miracle vortices are not obstacles | done | `physics_classes::IsObstacle` |

## Knock-back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A resting object hit by a flying body is knocked into flight (rocks, villagers, animals, pots) and flies on its own | done | Knocked result in `DynamicsSystem::ProcessTurn`, starting its physics through its kind (`PhysicsGameHooks`) |
| A rock hit hard may split in two, the halves keeping its speed and half its spin | done | `object_physics::SplitRock`; see [impact_damage.md](impact_damage.md) and [../nature/rocks_splitting_and_heat.md](../nature/rocks_splitting_and_heat.md) |
| Whoever threw the first object is credited for what the objects it knocked do | done | Credit passes on at turn end (`DynamicsSystem::ProcessTurn`) |

## Stuck and embedded objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A released object that overlaps things under it is raised, step by step by the deepest overlap through their tops, until nothing pushes it | done | `DynamicsSystem::RaiseClearOfWhatIsUnder` (`Body::OverlapDepth`, restarting after each raise); test `PhysicsBody.OverlapIsMeasuredThroughTheTop` |
| A villager is not raised over things a villager pushed, nor those over a villager | done | `RaiseClearOfWhatIsUnder` |
| The miracle vortex and map shields never raise objects | done | `PhysicsClassHooks::RaisesObjects` |

## Map edges

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Looking up map cells for a body near or past the edge uses the nearest edge cells of the 512 by 512 map | done | `turn::WakeBox` clamps the corners; test `PhysicsTurn.MovingBodiesWakeTheCellsTheyWillReach` |
| An object that comes to rest outside the map is deleted | done | `DynamicsSystem::EndPhysicsAsObject` |
| A body that falls more than four radii under sea level (off the land into the deep) is deleted | done | test `PhysicsBody.ABodyFarUnderTheSeaIsDeleted` |
