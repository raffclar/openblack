# Object dynamics

How loose objects move once they are in the physics: falling under gravity, tumbling, sliding and rolling on slopes,
bouncing, coming to rest and waking again. The game runs its own rigid-body simulation (every mesh vertex is a contact
point with a spring and friction, probed 0.06 s ahead), not a general physics engine. Collisions are in
[collisions.md](collisions.md), water in [water_physics.md](water_physics.md), throwing and landing in
[throwing_and_landing.md](throwing_and_landing.md). Impact damage, fire and explosions are in
[impact_damage.md](impact_damage.md), [fire.md](fire.md) and [explosions.md](explosions.md). Tornadoes do not push
physics bodies: what a tornado lifts rides its funnel instead (see [../miracles/tornado.md](../miracles/tornado.md)). No
wind or other steady force acts on flying or resting objects: the only pushes are a villager shoving an object out of
its way and the hand's twist after a release, each lasting one turn.

openblack (`physics` branch) runs the game's own simulation: the engine is `src/Physics/` (`Body`, `BodyShapes`,
`Materials`, `TurnRules`, with `components/physconst` reading `Data/PhysicsConstants.txt` through the resource caches),
and `DynamicsSystem` (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`) steps it once a game turn and draws moving
bodies between turns. Each kind's material, weight, shape and rules are in `src/ECS/PhysicsClasses.cpp`. Tests:
`test/test_physics_engine.cpp`, `test/test_physics_manager.cpp`. Bullet has been removed from openblack (user decision
2026-10-08); picking and land lines use the game's own tests (`PickingSystem`).

**Progress: 41/43 done, 2 partial — 98%**

## Simulation and timing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The physics runs in fixed steps of 5 ms, 20 of them per game turn of 0.1 s (200 a second), however fast the frames are | done | `DynamicsSystem::ProcessTurn` runs the turn's 20 steps of `physics::k_StepSeconds` once per game turn |
| The physics stands still while the game is paused | done | `Game::GameLogicLoop` runs no turn while paused, so `DynamicsSystem::ProcessTurn` isn't called |
| A flying object is drawn between where it was at the start and at the end of the last game turn, so its motion looks smooth at any frame rate | done | `DynamicsSystem::UpdateFrame` writes `PhysicsDrawPose` (`Body::DrawPose`, with the quarter turn of boned models), used by `RenderingSystem`; test `PhysicsBody.TheDrawnPoseLiesBetweenTheTurnsPoses` |
| The game logic (AI, reactions) sees each flying object's position and angles at the end of the turn | done | Each moving step writes the body's object pose into the `Transform` (`DynamicsSystem.cpp`) |
| There is no limit on how many objects can be flying at once | done | The physics list grows by 16 when full (`DynamicsSystem::MakeRoomForOne`) |
| A flying object leaves the map cells while it flies and goes back into them when it comes to rest | done | `DynamicsSystem::StartPhysicsAsObject` / `EndPhysicsAsObject` (map cells via `MapInterface`) |
| Objects a script marks as immovable never become physics objects | done | `Immovable` component set by `SET_ID_MOVEABLE` (`CHLApi.cpp`); refused in `AddObject`, starting physics and knocks (`PhysicsClasses.cpp`); test `PhysicsClassesTest.AThingHeldImmovableNeverMoves` |
| Buildings, fields, forests and the creature never become flying objects; rocks, statics, mobile objects, villagers, animals, trees, dead trees, food and wood, building pieces and one-shot miracle orbs can (pots only when their info allows, villagers only when reachable) | done | `physics_classes::CanBecomePhysicsObject` (`src/ECS/PhysicsClasses.cpp`); tests `LivingThings`, `PotsFlyByTheirInfoAndAreHitUnlessInAStoragePit`, `FieldsAreNeverInThePhysics` |

## Bodies and shapes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body is built from the mesh's physics parts (or the LOD-0 parts when it has none): every vertex is a contact point and every triangle a face others can hit; morphing models (food piles, storage pits, the village centre, the temple heart) follow the land under each point | done | `shapes::FromModel` (`src/Physics/BodyShapes.cpp`), `L3DSubMesh::GetBodyGeometry`, land offsets in `PhysicsClasses.cpp`; test `PhysicsShapes.AModelUsesItsCollisionPartsFirst` |
| Trees get a hand-made spindle body of 16 points along the trunk, with the centre of mass at 40% of the height while rooted (the spindle then reaches 0.2 of the height below the base) and at half the height once dead, and 0.3 of the drag | done | `shapes::Tree`; test `PhysicsShapes.ATreeIsASpindle` |
| Villagers and animals get a hand-made body of 12 points on three levels (head, waist, feet), centre of mass at half height, with twice the air drag | done | `shapes::LivingBody`; test `PhysicsShapes.LivingThingsAreTwelvePointBoxes` |
| Pieces broken off buildings are thin slabs of their triangles, weighing 30 per unit of area, with twice the air drag; pieces thinner than 0.4 R² are deleted | done | `shapes::Fragment` (tests `ABuildingPieceIsASlab`, `APieceExactlyAtTheThinLimitIsKept`), made by `BuildingDamageSystem` when a rock breaks a building; each piece splits its own loose parts as it is made (`9ccbe6fa`) |
| How a body tumbles comes from its moment of inertia worked out from its points; the original's formula has one cross term wrong, which changes how lopsided objects spin | done | `Body` inertia (`src/Physics/Body.cpp`); test `PhysicsBody.InertiaKeepsTheGamesCrossTerm` |
| The body's radius is its farthest point from the centre of mass, times the object's scale | done | `shapes::FromModel` |

## Mass and material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An object's mass is its info weight times its scale cubed, and never below 0.01 (trees excepted) | done | `physics_classes::Weight`; test `PhysicsClasses.WeightGrowsWithTheCubeOfTheScaleAndNeverReachesNothing` |
| Villagers weigh 82.5 at scale 1; buildings count as 2000 and never move; the temple heart and the gates 10000, the creature 1000, the physical shield 50000 times its scale cubed, all static | done | `PhysicsClasses.cpp`; tests `BuildingsAreHeavyStaticObstacles`, `GatesTheCaveAndThePhoneBoxCollideWithModelsOfTheirOwn` |
| Each kind of object takes one of 24 material rows from `Data/PhysicsConstants.txt`: density, contact stiffness, contact damping, friction, how much spin is kept each second, and air drag | done | `physics::MaterialTable` (`src/Physics/Materials.cpp`), loaded through the resource caches (`Resources/Loaders.cpp`) |
| Material values are clamped when loaded (density 0.05 to 3, stiffness 0 to 240, damping 0 to 10, friction 0 to 3, spin kept 0.2 to 1, drag 0 to 4); rows past the file's count copy the first one; without the file every row is zero (the game has no built-in values) | done | `components/physconst`, `MaterialTable`; tests `PhysicsMaterials.*`, `PhysicsConstantsFile.*` |
| Which row each kind uses: buildings, the temple heart and the creature row 0, any other movable object row 1, the football 2, rocks and the heavy statics 3, pots 4 (the food offering 5), trees 6, villagers 7, animals 8, one-shot miracle orbs 9, the physical shield 10, building pieces 11, poo 12, vortex 13, the toys 14/15/16/19/20, the scaffold 17, fences 18, the three mushrooms 21–23; wood in the hand row 1 | done | `physics_classes::MaterialRowOf`; tests `RocksAreDenseObstaclesThatBreakBuildings`, `HeavyStaticsAreMadeOfRock`, `FencesAndToysHaveTheirOwnMaterials`, `MobileObjects`. Toys are known by their model, an unknown toy model taking the rock row; see [../nature/toys.md](../nature/toys.md) and, for the scaffold's row 17, [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md). Row 2 is the football's own (`components::Ball`, test `TheFootballIsAToyOfItsOwnMaterial`); the mobile object 'ball' (`MobileObjectInfo::Ball`) is a plain thing of row 1 (`dc312635`) |

## Gravity and flight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every flying body falls under a gravity of 9.81 m/s² | done | `physics::k_Gravity` in `Body::TouchGround` (creature throws still use their own arc in `CreatureThrow.cpp` until they move into the physics, see [throwing_and_landing.md](throwing_and_landing.md)) |
| Air drag grows with the square of the speed, scaled by 0.3 times the radius squared and the material's drag | done | `Body`; test `PhysicsBody.DragGrowsWithTheRadiusAndTheKind` |
| No body flies faster than 124 m/s | done | `Body::Integrate`, `AddObject`; test `PhysicsBody.SpeedIsCapped` |
| No body spins faster than 3π radians a second (one and a half turns) | done | test `PhysicsBody.SpinIsCappedAtOneAndAHalfTurnsASecond` |
| Spin dies away: each second a body keeps its material's share of its spin | done | tests `SpinDiesAwayByItsMaterial`, `SpinKeptEachStepIsTheShareKeptEachSecondSpreadOverTheSteps` |
| Bodies tumble when they hit things off-centre: each contact point's force turns the body | done | `Body::ApplyContacts`; test `PhysicsBody.ASlidingCubeTumblesForwards` |
| Steady pushes on a body last one game turn and are cleared at its end; only a villager shoving an object and the hand's twist after a release set them | done | External force/torque cleared at turn end (`DynamicsSystem.cpp`); twist in `HandGrabSystem` (`hand_grab::ReleaseSpinTorque`), shove in `DynamicsSystem::PushObject` |

## Contacts, sliding and rolling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each contact point pushes back like a spring: the depth at first touch times the stiffness, plus damping on any deeper push | done | `Body::ApplyContacts` |
| Each contact point holds with friction up to friction times the push; past that it slides, so objects stick on gentle slopes and slide down steep ones | done | test `PhysicsBody.FrictionHoldsOnGentleSlopesAndNotWithout` |
| Round or many-pointed objects roll down slopes as friction at the contact turns into spin | done | Contact torques in `Body::ApplyContacts` |
| How high something bounces comes from its material's stiffness and damping, not from a fixed bounce share | done | No bounce factor exists in `Body`; test `PhysicsBody.HowDeepABodySettlesComesFromItsMaterial`. Creature throws still use `CreatureThrow.cpp`'s fixed shares until they move into the physics |
| Fast bodies test their contacts 0.06 s ahead, so they don't pass through thin things | done | test `PhysicsBody.LookingAheadStopsAFastBodyAtAThinWall` |

## Coming to rest, sleeping and waking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body touching something comes to rest when its speed is under 1 m/s, its spin under half that and its turning force under it; its velocity and spin are then cleared | done | `Body::Integrate`; test `PhysicsBody.ADroppedCubeComesToRestOnTheLand` |
| A body can't come to rest before it has been in the physics for its scale times its half height in seconds (a tall object flies longer before settling) | done | test `PhysicsBody.ATallerBodyTakesLongerBeforeItCanRest` |
| From 15 s after that the rest test loosens steadily (the limit grows by 1 every 15 s), so nothing jitters for ever | done | test `PhysicsBody.TheRestTestLoosensAfterLongInThePhysics` |
| At rest the object goes back into the map; one that comes to rest outside the map is deleted | done | `DynamicsSystem::EndPhysicsAsObject` |
| Resting objects near a moving body join the physics as sleeping obstacles, and only those | done | Wake walk in `DynamicsSystem::ProcessTurn` (`turn::WakeBox`); test `PhysicsTurn.MovingBodiesWakeTheCellsTheyWillReach` |
| A moving body wakes the objects in the map cells within its radius plus 0.1 s of its sideways travel | done | `turn::WakeBox` |
| A sleeping obstacle that is hit hard enough is knocked into flight and becomes a physics object of its own | done | The knocked result starts the object's physics through its kind (`DynamicsSystem.cpp`, `PhysicsGameHooks`) |
| Sleeping obstacles with nothing moving near them leave the physics at the start of the next turn (the physical shield always stays) | done | `DynamicsSystem::ProcessTurn` turn start |
| A body that falls more than four of its radii below sea level is deleted | done | test `PhysicsBody.ABodyFarUnderTheSeaIsDeleted` |

## Other things that move objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager clearing ground pushes the obstacle within 2 m with a force of its weight × 9.81, and kicks the football; what they push moves in the physics and villagers' bodies don't hit it | partial | `DynamicsSystem::PushObject` and the pushed-by-living pair rule are done; the football and its kick (`object_physics::KickBall`: straight-line time at the speed, flying up by its fall, taken out of the physics first unless a tornado carries it, laid on the land, pushed by a living; `objects::KickVelocity`, `dc312635`) are ready; no villager clears ground or plays football yet, see [../villager/](../villager/) |
| A forester's felled tree is put into the physics and topples; once it tips past about 11 degrees a tree taller than 10 m makes one falling sound | partial | `object_physics::FellTree` (`9908a856`) fells it as the game does (0.2 × height m/s away from the feller, its spin, pushed by a living, the felled-tree kind) and the manager plays the fall sound; openblack has no forester job to call it |
| Reward chests don't use the physics while falling: they appear 150 m up, fall at 50 m/s turning at π rad/s, and on landing play their thump, shake the camera and raise 27 dust sprites | done | `RewardSystem` (`158404fb`): 150 m at 50 m/s turning π rad/s, sample 174, an all-axes shake of radius 200 centred on the world's origin (the game's own quirk), 27 dust sprites; weight 789 and the movable row once on the land; test `test_reward` |
