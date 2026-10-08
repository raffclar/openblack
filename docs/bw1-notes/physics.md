# Physics: thrown objects, collisions, damage and rocks that split

Code: `src/ECS/Physics/` (`PhysicsBody`, was `PhysOb`, = rigid body, `PhysicsObjects` = per-turn manager, `FromHand` = what a released
object does), `src/ECS/Rocks.*`, `src/ECS/Components/Life.h`, and the hand part in `HandPhysics.cpp`. Bullet is not
used for this (openblack only uses it for ray casting).

> **Code rules.** A body's state lives in ECS components (`Life`, `BuildingDamage`, `Fragment`, `PhysicsDrawPose`) and
> the per-turn manager is reached as a Locator service, never as a global; each class's part is registered as a
> handler by the system that owns the class; the pure formulas (contacts, damage, the cylinder cut) are tested with
> fakes in `test/`; comments describe behaviour in plain English, with no decompiled names or addresses (those
> belong here). See [the conventions](../refactor/README.md).

- [Engine (PhysOb)](#engine-physob-0x7fb7300x7fe7b0)
- [Bodies](#bodies)
- [Manager](#manager-physicsobjectgameturnupdate-0x644fc0)
- [Buildings that break](#buildings-that-break-abodereacttophysicsimpact-0x406240-fragmesh-0x7f6f00)
- [Sounds, dust and the look of impacts](#sounds-dust-and-the-look-of-impacts)
- [Water in impacts and when dropping](#water-in-impacts-and-when-dropping)
- [Rocks that split](#rocks-that-split-rocksplitintwo-0x6e7560)
- [Who owns what: class handlers and the hand's API](#who-owns-what-class-handlers-and-the-hands-api)
- [The physical shield test](#the-physical-shield-test)
- [Pending](#pending), [Test hooks](#test-hooks), [Sources](#sources)

Status: the engine and what is described here is **faithful** (ported from the original) except what is marked and what is in
[Pending](#pending). Everything about water that is not physics (which cell is water, the impact against the water, sinking and
drowning) is in [water.md](water.md).

## Engine (PhysOb, 0x7FB730..0x7FE7B0)

- **Penalty contacts** on a cloud of vertices (point masses): each vertex below the ground or inside
  another body receives a normal spring and a Coulomb friction anchor. No impulses and no restitution coefficient: the
  bounce comes from the spring and the damping of probing the vertices **0.06 s ahead** (`x + v·0.06`).
- **Penetration against another body** (fn_007FDD60, 0x7FDC90..0x7FDD0E): the ray from the centre T to the predicted
  vertex hits a face of the other body at `hit = T + s·dir`; `pen = predLen − dot(hit − T, dir) / predLen` with
  `predLen = max(|world − T|, 0.001)` (vertex +0x20, read by the `fdiv` at 0x7FDCF5 and the `fsubr` at 0x7FDD01), so
  `pen = (1 − s)·predLen`, the depth of the predicted vertex past the face. It is positive from the first substep the
  0.06 s look-ahead crosses the surface: that is what stops a fast body before it tunnels. openblack once divided by
  +0x1C (`len`, the unpredicted length), and a rock at 33 m/s went through a physical shield.
- **dt 0.005 s, 20 substeps per 0.1 s turn**, semi-implicit Euler. Gravity 9.81; max. speed 124 (the same as the
  hand throw); max. ω 3π. No linear damping; the angular one keeps `d4` per second.
- Constants: dt 0.005 (0x8C7674), gravity 9.81 (0x8CF02C), look-ahead 0.06 (0x8CA27C), max. speed 124 (0xC371FC, the
  same float caps the hand throw), max. ω 3π (0x9A2BB0).
- One substep is `ZeroForces` 0x7FD200, `GroundAndWater` 0x7FD4D0, `ContactForces` 0x7FDE40 and the integration
  fn_007FE260; `SetUpPos` 0x7FC760 builds the body's vertices from the object's matrix (`Object::GetWorldMatrix`
  0x638200).
- The rotation step is only taken when the angle is above 1e-5 (0x99A100); the axis is the step divided by the angle
  (0x7FE706..0x7FE748).
- `AdjustToGroundLevel` 0x7FCB80 reads the normal at the centre's cell (`GetNormal` 0x803630) and starts its
  lowest-vertex search at 1000 (0x7FCB8E); at its end, and at the end of `SetUpPos`, the current matrix is copied into
  the turn-start one (0x7FCE56..0x7FCE6B, 0x7FC94C..0x7FC98F), so a body that was just placed is drawn where it is.
- The backward ray of `fn_007FC310` starts with best = −10000 (0xC61C4000) and keeps the hit with t < 0 nearest to 0.
- `AddObject` 0x6443A0 takes the spin in the body's axes: `t = M·w` with the inertia tensor (PhysOb +0x8), then L = the
  body rows times t (0x6445DB..0x644691). It sets L before `Object::InitialisePhysics` reaches 0x63756A.
- The original accumulates torque as F×r and rotates the rows the other way round; the two inversions cancel out. The port uses r×F and
  columns (same motion). The inertia tensor keeps the original's bug (`I[1][2] = −xz`).
- **Ground**: `GetAltitude` per vertex and the **flat normal of the exact triangle** (`LH3DIsland::GetNormal` 0x803630,
  raw heights without the flattening at the sea edge), with the original's two tables (T1 0xE9B2D8, T2 0xE9A2D8, built by
  fn_00803890): `land_normal::OfCell`, checked branch by branch.
- **Sea**: if the ground under the centre is < 0.0001, the centre is below the radius and the cell has no land:
  buoyancy `frac·m·g/density`, drag ×100, and **no contact with the bottom**. Density rises by 6.67e-5 per
  submerged substep (it soaks up water; the rate is 0x9A2BC8); above 1 it sinks. Below −4R (0x9A2BD0) the object is
  deleted.
- **Corpses sink**: at each turn start an object with `GetLife() < 0.01` gets +0.01 density (`GameTurnUpdate`
  0x644FEE).
- **Rest**: threshold 1 (4 if already at rest), grows after 15000 counts (0x8AB418; growth factor 6.66667e-5,
  0x9A2BCC); the counter starts at −scale·half height·1000 (0x7FB7D6) and adds 5 per substep.
- `Data\PhysicsConstants.txt`: version 3, 24 rows × 6 (density, contact k/mass, penetration k/mass, µ,
  angular fraction kept per second, drag), each column clamped to its range. Rows per class: 0 houses,
  3 rocks, 4/5 pots, 6 trees, 7 villagers, 8 animals, 14–20 toys, 21–23 mushrooms. It is loaded by
  `EditorPhysics::Load` 0x5249D0 into `EditorPhysics::PhysicsConstants` 0xCC63E0; the column minima are at 0x8D8B10 and
  the maxima at 0x8D8B28.

## Bodies

- Mass = `scale³ · info.weight` (Object::GetWeight 0x638480), minimum 0.01. Houses 2000, static.
- By default, the vertices and triangles of the `isPhysics` submeshes (bit 13) or, if there are none, those of LOD 0.
  **The user's modified `AllMeshes.g3d` has no physics submeshes in the rocks**, so the drawing mesh is used
  (40–109 vertices; the original art has 12–24).
- Trees (`SetUpPhysObAsATree` 0x63A230): 16 points in rings along the trunk and 24 faces; centre of mass at
  0.4 H (alive) or 0.5 H (dead). Villagers and animals (0x5EFF40 / 0x5F04E0): box of 12 points and 20 faces, centre at
  half height, drag ×2. The box's 12 points are turned by π/2 about Y with cos stored as the float −4.371139e-8; the
  radius is the length of the turned vertex (0x5F01BD..0x5F0218 / 0x5F0763..0x5F07BE).
- Creature: it never flies (`CanBecomeAPhysicsObject` 0x479D10 = 0) but thrown objects hit it
  (`InteractsWithPhysicsObjects` 0x479D20 = 1). Its body (`Creature::SetUpPhysOb` 0x479B90) is the LH3DCreature's
  bounding sphere, mass 1000, not dynamic, one vertex per skeleton element; its weight is `Creature::GetWeight`
  0x47CD60 = `LH3DCreature::GetMass` 0x47FA80.
- The physical shield gets a heavy body: GMapShieldInfo 0xDA05D0 (magic) / 0xDA06D8 (physical), weight 50000.
- Animated meshes (boned, mesh flag 0x100, `LH3DObject::IsAnimated` vt +0x1AC): at draw time fn_007FCE80 turns the
  rows by π/2 about Y after normalising them (0x7FD013..0x7FD082): r0' = c·r0 + s·r2, r2' = c·r2 − s·r0, row 1
  unchanged.
- Fragment body (`Fragment::SetUpPhysOb` 0x76EC50): the triangles' distinct vertices (exact match), each with the normal
  of the first triangle it was found in. The ×2 is applied at 0x76F2DB to PhysOb +0x14C (the drag). The rest counter
  uses the half height of the mesh the Fragment's ctor builds as a Rock (`Rock(coords, &MS[2] = 0xD3A930 …)` 0x76E9E4,
  scale 1 at 0x76EA32).
- One-shot orb: `OneOffSpellSeed::Create` 0x72A3BE makes a MobileObject with the GMobileObjectInfo at 0xD39F3C, the one
  after WHALE (0xD39E28, stride 0x114), index 25 **(inferred)**.

## Manager (PhysicsObject::GameTurnUpdate 0x644FC0)

- **Once a game turn**, from `GGame::ProcessTurn` 0x54E67E (after `FireFly::ProcessAll`, before `GScript::Process`;
  not while paused): the turn's start, its **20 substeps in one go** (0x64576F..0x64604D, `cmp eax, 0x14` at 0x646046)
  and its end. The game logic sees the end-of-turn pose. openblack: `PhysicsObjects::GameTurnUpdate` from
  `Game::GameLogicLoop`. (The second call in `Process3dEngine` 0x54DAB0 is dead: nothing writes [0xD46A74].)
- **Drawn between turns** (fn_00646FE0 from `GLandscape::Draw` 0x5E49DC, every frame, each awake body):
  fn_007FCE80 lerps the 12 floats of the turn-start matrix (PhysOb +0xAC, copied from +0x7C at 0x645187) to the end
  one cell by cell with the turn fraction (g_game +0x205D64, `game_clock::TurnFraction`), normalises each row
  (fn_007FB5C0, `affine::NormaliseRows` (was `lh_matrix`), with LH3DMath's `InverseSquareRoot` 0x841170 and its table), and takes
  `T − R·s·com` as the origin. `PhysicsDrawPose` carries it to the drawing (instances, blob shadows, a villager's carried prop, the
  trees' bending sources); a body at rest is drawn at its Transform. (pending) the −Radius < T.y filter of the drawing (0x647017), the
  π/2 turn of animated meshes (vt +0x1AC, 0x7FD009) and vt +0x184, the flames of a burning flying object
  (FireGraphic), the roots of a flying tree (they follow its Transform), dynamic shadow receivers (RendererShadows).
- In the original a body is posed between turns only while −Radius < T.y (0x647017..0x64702A); a body sunk deeper than
  its radius keeps the pose it was last drawn at.
- **Turn start** (0x645013..0x645180): an entry whose object is no longer available (`IsAvailable` vt +0x2C at
  0x645018) gets `PhysOb::DeInitialise` 0x7FB730 and is removed; the last entry moves into its slot. This is how the
  body of an object marked for deletion leaves (the dead list itself is in [engine-loop.md](engine-loop.md)). Then the
  end matrix (+0x7C) becomes the turn-start one (+0xAC) (0x645187..0x64519B).
- **Wake walk** (0x6452A6..0x64556A): for each moving body, cells are walked x outer, z inner; a cell is walked only
  once per turn for all the bodies (list of walked cells, at most 0x200, 0x64547B). In each cell the fixed list comes
  first, then the mobile list. An object already in physics is woken (|= 1, 0x6454F7); one with a 3D object gets a
  resting proxy at once (`AddProxy` fn_00644DF0, 0x64551D).
- Box corners (`RaiseUntilNotIntersecting` 0x644877, `GameTurnUpdate` 0x645322): x·6553.6 (0x8AC400) then `__ftol`;
  off the map fn_00604250 clamps each corner to 0..511 ([g_game+0x59C8] − 1).
- Physical shields always stay in the list (`GetAlwaysRemainsInPhysicsInternalSystem`, PhysicalShield 0x72CAF0 = 1),
  whether or not something moves near them.
- `PhysicsObject` +0x1A4: 1 villager (`AddObject` 0x64476D, `AddProxy` 0x644E67), 2 felled tree (`FelledTree::Create`
  0x511883), 3 felled tree that has toppled, else 0.
- **Felled tree falling sound** (0x64609F..0x6460F8): a kind-2 body whose up row has y < 0.98 (0x8CF3FC), and whose
  height is above 10 (0x8AB414), plays once `SoundTag::Create(MapCoords, GetRandomSample(31, 1), track 0, mode 3, 3D,
  InGame)` 0x71EB60; then it becomes kind 3.
- Deletion below −4R (code 4, 0x645B22): `ToBeDeleted(0)` is called only if the object is still available
  (0x645D15..0x645D2B).
- An object `Object::InitialisePhysics` 0x637480 puts in flight leaves the map cells (`IsObjectInMap` vt +0x178 at
  0x6374AC, `RemoveMapObject` vt +0x548 at 0x6374BA); a burning object leaves its fire group
  (`FireEffect::StartedMoving(0)` 0x637589); `CheckAllCreaturesForCatching` 0x47CBD0 runs at 0x63756A. It walks
  the creature list; for each it calls fn_0047C960 first (0x47CBF3) and skips the creature when that returns 0, then
  skips one with Flags & 0x10 (0x47CC00), one in the hand's locked select.
- A knocked resting proxy is woken through `InitialisePhysics` (vt +0x784 at 0x645FB3) only when that call starts
  (Started == 1, 0x645FB9).
- **The list** grows by 16 slots when it is full (`MakeSureEndSlotIsFree` 0x644C40, no upper limit). The wake pass
  stops when the allocated slots are used up (0x6453E2 / 0x645411 / 0x6454B7): no more proxies or wake-ups that turn.
- **Pair skip** (0x64583E..0x645866): a villager's body (+0x1A4 == 1) does not hit what a Living pushed (flag 2:
  `Object::PushObject` 0x6396BA, `Ball::KickBallAtDestination` 0x435D99, `FelledTree::Create` 0x51186B). Nothing in
  openblack sets flag 2 yet (pending, with those three).
- **Pushing** (`Object::PushObject(Living*)` 0x639640, vt +0x81C, from `Villager::CheckForClearArea` 0x75917E): the
  object's body (found with `SearchForPhysicsObject` 0x646950, or made by `InitialisePhysics(0, 0, …)`) gets flag 2 and
  external force += F·d/|d|, d = body centre − the object's MapCoords point, F = `CalculateForceAppliedBy` 0x639620 =
  GetWeight × 9.81 (the pusher is not read). Nothing if d is exactly 0. An object a particle system carries has no
  body, so it gets no force (0x639678). It returns 0.
- Each turn, each moving body looks at the box `|v.xz|·0.1 + R` and adds as **obstacles at rest** the objects
  that interact: rocks and statics, mobile objects, villagers, animals, dead trees, hand pots, houses and
  storehouses. **Standing trees do not interact** (thrown things pass through them); neither do fields, forests nor piles.
  Who interacts: physical shield 1 (0x72D600), magic shield 0, creature 1, fields never
  (`Field::InteractsWithPhysicsObjects` 0x528020); buildings only while standing (MultiMapFixed: `GetPercentBuilt >
  0.1 && life > 0.01`).
- Object–object collision: ray from the centre of A to each vertex against the triangles of B; the forces go to both. An
  obstacle that does not manage to stay still (threshold 4) starts flying: that is how a villager is knocked down or a rock pushed.
- At the end of the turn: `impact = |ΣF|·0.05` (mean force), G = impact / (m·g) (≈1 when resting), and
  `ReactToPhysicsImpact` on both bodies. The damage is attributed to the player who threw what hits.
- **Damage**: villagers and animals, if G > 2, lose `(G−2)·0.03` of life (× defenceMultiplierCrush; 1 for villagers).
  Rocks (not hit by another rock), if G > 4 and height > 0.7: life −(G−4)·0.005, and they split below 0.01.
  Tree or dead tree hitting a storehouse: it becomes wood. A pot or a pile (the hand's pots included) that hits
  something (`Pot::ReactToPhysicsImpact` 0x66DC90): the hit (`GetGameObjectWhoHitMe` 0x644F00) and the pot available,
  and the hit `IsResourceStore` of the pot's type or `IsPot` of the same type → the hit's
  `DeleteObjectAndTakeResource(pot, po +0x24)`; when it took nothing (or no such hit), `MobileObject`'s reaction
  0x607A20. openblack: the Pot class's `reactToImpact` (`HandPhysics.cpp`, `held_apply::PotImpactTakes`). Houses: see
  below.
- Life: `Object::GetLife` (vt +0x11C, 0x402600) reads Object +0x48.
- **Living damage** (`Living::ReactToPhysicsImpact` 0x5ED3E0, shared by Villager): `Object::ApplyEffect` with the crush
  preset g_EffectInfo[3] (crush 1.0) × defenceMultiplierCrush, then `ReduceLife`; at 0 life the class's
  `DestroyedByEffect` (vt +0x5F8, 0x637A79). `Villager::DestroyedByEffect` 0x7502D0 → `VillagerDead(2)` does nothing
  while it flies (0x7506C3): the landing kills it.
- `Rock::ReactToPhysicsImpact` 0x6E7930 holds the "not hit by another rock" test.
- Hits on a resting proxy clear a building's last hitter: `Abode::SetUpPhysOb` 0x402DD0.
- **End of a body's physics** (EndPhysics vt +0x790): `Object::EndPhysics` 0x6375A0 clears IN_PHYSICS, sets the map
  coordinates from Pos and, with insert and the object available (0x637617), puts it back in the map cells when
  `MapCoords::InBounds` 0x6042C0, else deletes it (`ToBeDeleted(0)`). It always returns the object. Villager
  (0x5F0B81) and Animal (0x5F0E01) call it in the middle of theirs, after SetYAngle and before their landing, water
  and death work (`PhysicsObjects::BackInMap` from `ECS/LivingPhysics.cpp`); the other classes after their part. A
  tree that becomes a DeadTree never calls it: the DeadTree goes in the cells with no InBounds test (0x74BC0A).
- When a body stops in `GameTurnUpdate` (0x645EB5..0x645F08), the object `EndPhysics` returned gets
  `SetXYZAngles(GetYXZ(body))`. A DeadTree returned by `Tree::EndPhysics` is a MobileStatic (0x608CE0), so it keeps x,
  y and z.
- Orientation after physics (`SetXYZAngles`, vt +0x514; `GetYXZ` 0x7FAB30): an Object (Villager, Animal, Tree,
  Feature, 0x638D00) takes only the Y angle and stands upright (0x638DBA..0x638E6D), so a tree replanted from the hand
  stands straight; a MobileStatic (DeadTree, Fragment, statics, 0x608CE0) or MobileObject (Pot, Scaffold,
  OneOffSpellSeed, 0x607460) takes YXZ(y, x, z) (`SetYXZMatrixOnly` 0x7FAC10).
- The landed yaw is taken by `RemoveObject` 0x646A50 / `GameTurnUpdate` 0x645D51 before the class's `EndPhysics` as
  `GetYXZ`'s yaw, atan2(−fwd.x, fwd.z), and set again after it (0x646B72 / 0x645F08).
- **In physics or not.** The original's flag IN_PHYSICS (`GameThingWithPos +0x24 & 0x40`) is set by
  `Object::InitialisePhysicsFromHand` (0x636F2F) and cleared by `Object::EndPhysics` (0x6375A0) and `RemoveObject`
  (0x646B56). A body at rest keeps its entry in the list as the resting proxy (0x645EE0) with the flag already
  cleared, so the flag is `PhysicsObjects::IsFlying(entity)` (an entry that is not resting), not
  `PhysicsObjects::Find(entity) != nullptr`.
- **End of flight**: a thrown tree ends up as a DeadTree in the pose it was left in; a hand pot on land
  becomes a pile; villagers and animals get up; a villager that ends up in a cell with water goes to DROWNING (60 s)
  and a sunken animal is deleted (see [water.md](water.md#sinking-drowning-and-being-deleted)).
  - `Villager::EndPhysics` 0x5F0A60 takes its landing pose from the **turn-start** rows (po+0xD4..): with a = right.y,
    a < −0.5 (0x8CEFCC) → posture 1, yaw = GetYAngle(up) (no π, no wrap), creature desire ANGER 0.5; a > 0.5
    (0x8AA3B4) → posture 2, yaw = Wrap(GetYAngle(up) + π), ANGER 0.5; else posture 0, yaw = Wrap(GetYAngle(fwd) + π),
    COMPASSION 0.1. With a player (`GetPlayer` 0x647460) it calls `MakeCreatureEmpathiseWithPlayer` (0x5F0B53).
  - Then `SetYAngle` 0x60DAC0 (+0x4C and GameAngle +0x5C) at 0x5F0B61; if it was not put down gently (no LANDED),
    +0xE0 &= ~0x20 (0x5F0B67..0x5F0B70); then `Object::EndPhysics` 0x5F0B81, the land type into status bits 4-5 of
    +0xB4 and altitude +0x1C = 0 (0x5F0B88..0x5F0BA8).
  - On land with life ≤ 0 (0x5F0CA0..0x5F0CEC): an already dead one goes to state 15 with its counter kept, otherwise
    `VillagerDead(5 PLAYER_INTERACTION, player, 0, 1)`; alive, +0x104 = `GetPlayer()` (0x5F0C83), then
    `SetTopState(LANDED)` 0x5F0D31 → `Villager::Landed` 0x7606E0.
  - `Animal::EndPhysics` 0x5F0D80: land type from the turn-start right.y, the villager's mapping reversed (a > 0.5 → 1,
    a < −0.5 → 2, else 0, 0x5F0D93..0x5F0DC7); yaw = Wrap(GetYAngle(the **current** fwd row) + π)
    (0x5F0DC9..0x5F0DF3); then `Object::EndPhysics` 0x5F0E01, land type, altitude 0 and its life/state work.
  - Without a body (`Living::InitialisePhysicsFromHand` 0x5EFDF8, `EndPhysics(NULL, 1)`): the villager jumps to
    0x5F0B79 and the animal to 0x5F0DF9 with land type 3, with no pose, no empathy and no `SetYAngle`; with no
    PhysicsObject the drowning death uses the neutral player g_game +0x205A5B (0x5F0C17).
  - A put-down villager or animal never enters FLYING: `Living::InitialisePhysicsFromHand` sets FLYING (0x5EFDEB) only
    when the body exists, is not LANDED and still belongs to this object (0x5EFDD3..0x5EFDE0).

## Buildings that break (Abode::ReactToPhysicsImpact 0x406240, FragMesh 0x7F6F00..)

Code: `src/ECS/Physics/Buildings.*`, `FragMesh.*`, components `BuildingDamage` and `Fragment`
(`src/ECS/Components/Fragment.h`), meshes generated in `src/3D/L3DMeshGenerated.cpp`.

- Only rocks (row 3) and the row-20 toy break buildings, with `p = |v|·masa` of the hitter:
  **p > 2000** breaks; 1000–2000 and 300–1000 only make sound (`editor.sad` 431–436 and 437–442).
- On the first hit the building is copied into triangles in world coordinates (LOD 0 submeshes). An **infinite
  cylinder** through the rock's position, in the direction of `0.3·v` and with radius `R + 0.7`, decides:
  - triangles with all 3 vertices inside break;
  - those with 1–2 are split through the midpoint of the longest side (up to 3 times depending on their size);
  - small ones go by majority.
- The broken ones of each primitive fly as one piece with the impact velocity (spin ±1). Its loose parts and everything
  left of the building that does not touch the ground (y < ground + 0.1) fall as still pieces (spin ±2). Loose
  triangles disappear. Groups by shared sides (tolerance 0.01).
- The **building's life** becomes the fraction of triangles remaining; below 0.75 it stops working (pending:
  the villagers coming out and the village emergency); at 0 it disappears: its villagers are left homeless and a storehouse
  loses its piles. The collapse sound plays (`editor.sad` 443–447).
- The damaged building is drawn with its FragMesh: each triangle flat, with a back face 0.45 behind and a wall on
  each open edge, lit per face and two-sided on the CPU every frame as the original does (`fn_007F7ED0`; see
  [rendering-objects.md](rendering-objects.md#model-lighting)); its own generated mesh (with the partly built part
  over it) stays for the reflections. That mesh is the building's `components::DrawMesh` (the drawing-only model, the
  same one a building site's partly built model uses); the `Mesh` component stays the intact model, so the
  physics body, the map cells, the sizes and the static shadow keep it. A hit that leaves the whole building
  (remaining 1) takes the DrawMesh away and hands the draw back to `abodes::RedrawConstruction`. If the same rock
  hits it again, they stop colliding (it passes through on the third contact).
- **Pieces**: body = its distinct vertices and a copy of each one 0.45 behind (no faces: nothing collides with them),
  around its origin; the original's ×2 (0x76F2DB) goes to the **drag**, not to the inertia; the rest counter uses
  the half height of the rock mesh from info.dat. They are created inside `FragMesh::Impact`, before reading what remains
  (so a hit that only splits triangles also releases pieces). A building forgets the rock that hit it when the rock
  stops or is picked up, or when its body is rebuilt (fn_646D60, Abode::SetUpPhysOb).
- `CreateFragment` 0x76EB20 + `Fragment::SetUpPhysOb` 0x76EC50 make a flying piece; its life of 100 turns per triangle
  is set in the Fragment ctor (0x76E9FB).
- `FragPrimitive::Impact` 0x76E2D0: right after `CreateFragment` (0x76E619) three `GameRand(201)` are drawn and dropped
  (0x76E7B1..0x76E7F8), even if the split left the piece no triangles; then the spin `(GameRand(201) − 100)·0.01` per
  axis, z first and x last (0x76E83B / 0x76E864 / 0x76E88D). `SplitUnconnectedGroups` 0x76DC30 draws its spin the same
  way ×0.02 (0x76E120..0x76E172).
- `FragMesh::Impact` is at 0x7F7D40; the back offset 0.45 is 0x8C7C78, the shared-side tolerance 0.01 is the double at
  0x8C7A10.
- A fragment forgets a parent that is no longer available (`Fragment::ProcessTimer` 0x76EAF3..0x76EB06);
  `Fragment::EndPhysics` 0x76F440..0x76F455 only touches the parent if it is available.
- A burning broken building draws its FragMesh with the fire's charring grey (fn_00730570) and glow
  (`GetFireEffectCharingColor` 0x730480), both with alpha 0xFF; otherwise 0xFFFFFFFF / 0 (`Abode::Draw`
  0x5160A6..0x5160E9). A fragment's FragMesh keeps the ctor's 0xFFFFFFFF / 0 (fn_007F6EE0 0x7F6EE5): nothing sets it
  again.
- `FragMesh::Draw` fn_007F7960 → fn_007F7ED0 per primitive in its own material; it sends batches every 256 vertices or
  triangles (0x7F869D..0x7F86D8). The tint flag 0xC371B4 is 1 in .data and nothing writes it.
- Repair draw: `GetPercentRepairedFromWhenDamaged` 0x52F010 = (life − site +0x640) / (1 − site +0x640), with the repair
  site `Abode::ReduceLife` makes at the hit (0x405E7A); `MultiMapFixed::RemoveDamage` (vt +0x8B8, from `Repaired`
  0x52EC70) removes the DestructionMesh and the whole model is drawn again.
- The player of a hit is `PhysicsObject::GetPlayer` 0x647460 of the **hitter's** body (`[arg +0x20]`, 0x406261); with a
  player and FROM_HAND on the **abode's own** body (`test byte [arg +0x1D8], 4` with arg the abode's entry, 0x406273),
  `ConsiderMakingCreatureMimicPlayer(hitter's status +0x24, DAMAGE_BY_THROWING_AT 16, the abode, 0)` (0x406286). It runs
  **before** the `PhysicallyDestroysAbodes` test (vt +0x7A8, 0x406291), so any hitter counts. The FROM_HAND bit is not
  inherited from the hitter (the turn copies only the status +0x24 into a hit body, 0x64629C..0x6462A7) and a resting
  proxy starts with flags 1 | (vt +0x7B4 & 1) << 7 (`AddProxy` fn_00644DF0, 0x644EC2..0x644EE6), so for a building the
  test is never met in practice. `StoragePit` 0x733730 and `TownCentre` 0x744380 only forward to the Abode's. A creature
  thrower is tested at 0x4064BA..0x4064DA. openblack: `Buildings::ReactToPhysicsImpact` tests the building's own
  `PhysicsObject::flags` the same way (`creature_mimic::ShouldMimicBuildingHit`), before its own hitter test.
- **Pieces**: row 11, mass `30·área`, they only collide with the ground, cannot be picked up, last 100 turns per triangle
  (no fade). Very thin ones (area < 0.4 R²) are deleted when created. A large one (area > 9) that falls while the
  building is still standing stays as rubble of the building.
- Quirk of the original: the rubble counts again as building triangles, and if after a hit the count
  reaches 1 the FragMesh is deleted and the house is drawn whole. It is kept like this on purpose, as in the original.
- A building that is not built (`IsBuilt` vt +0x890 = 0 at 0x4062E8, a building site) gets no FragMesh: the hit goes
  straight to `ApplyEffectsDueToPhysicalDestruction` 0x406640 (`abodes::OnPhysicalDamage`, see
  [buildings.md](buildings.md#life-and-damage), where the
  effect preset × defence crush 0.2 takes 0.2 off the percent built), with the player of what hit it
  (`PhysicsObject::GetPlayer` 0x647460: the hand's status +0x24, inherited) and whether a creature threw it (0x4064BA;
  not ported, no creature). openblack's `abode_queries::IsBuilt` reads the building's own state (not under construction
  and percent built ≥ 1).
- Pending: repair by villagers (the "half-built" drawing over the rubble; the FragMesh made anew when the draw percent
  is in [0.2, 1), 0x4062FA..0x40633C; `abodes::GetPercentForDrawBuilding` now exists, whether this branch uses it is
  not verified), creature hits (`ConsiderMakingCreatureMimicPlayer` 0x406286).

## Sounds, dust and the look of impacts

Code: `src/ECS/Physics/CollisionSounds.*`, `Dust.*`, `PartialBuild.*`.

- **Collision sound** (`AttemptToAddSoundEvent` 0x6464F0), once per turn on each awake body with something that
  hit it or `F > 0.5·m·g`: collision type of each side (info `collideSound`; piece = BUSH; DeadTree mesh 406 =
  HOLLOW_WOOD; no object = GROUND, or WATER in the sea), level by `g = impact / (unscaled info weight · 9.81)`
  (3 if < 1.25, 1 if > 3, otherwise 2), and `GAudio::SamplePlayAnimEffect(object, |g_camera − point|, {level, 0, A, B, 75},
  0, editor.sad, track = A ≠ 0x16)` (0x646919): the sample is chosen by the `editor.sad` animation table in the audio
  core ([audio.md](audio.md#the-worlds-callers-on-the-channels)), in 3D on the object, which is the owner
  of the channel. A pair does not sound again until two turns later. A rock against a building: the building sounds.
- **Buildings**: medium hit {2, 0, 0x16, 0x10, 75} and weak {3, …} (`G_Rock_V_Ground_M/S`, 0x406610), collapse
  {1, 0, 0x16, 9, 75} (`G_Crash_Abode`, 0x40671D), in 3D with the building as owner. (Corrects 431–447, which came from reading
  the bank table with a one-column offset.)
- Collision codes: tables A (hitter) 0xBFEE34 and B (hit) 0xBFEDB0, one entry per SOUND_COLLISION_TYPE; the key is
  built at 0x64686C..0x64688A. The pair list is at 0xD47208, at most 128 pairs.
- **Dust**: on falling to the ground, 6 puffs from `data\blobs.raw` (rows 2–3), colour 0x50806040, size `min(2R, 5)`,
  ±2 m/s, life 1 s of game time, they grow in 0.125 s and shrink down to 0; in the sea colour 0x28C8F0F4 + splash + ring;
  in shallow cells, ring + dust. Each building piece releases one per vertex (0x80706050, size 2).
- **Dust velocities**: a ground impact's puffs take `(LocalRand(201) − 100)·0.02` (0x8CF178) on each axis, drawn z, y,
  x (0x6467D1 / 0x6467ED / 0x646809); a building piece's puffs use the synced `GameRand(201)` instead
  (`Fragment::SetUpPhysOb` 0x76EEC5..0x76EF53). The puff's sprite variant is CRT `rand() % 16` (fn_00845FA0
  0x845FDE).
- **Whoosh** (G_ROCKPAST, `InGame.sad` 69–73): a body that enters the camera's 10 m sphere at more than 20 m/s:
  `PlaySoundEffect(0, 69 + GetTickCount() % 5, modo 2, 0, 0, 2D, InGame)` (0x645C12).
- **Half-built hit building**: over its FragMesh the intact model is drawn clipped at
  `pos.y + pct·height` (pct = `(life − s)/(1 − s)`, s = 1.1·life − 0.1 on hit: 1/11), with an inner wall at 0.35 (0.2 if
  the material is two-sided), a cap on the cut and the scaffolding (the submesh with the highest status) coming out of the ground; nothing
  if the cut ends up below 0.2. Without repair by the villagers, it stays like that.
- **Half-built draw** (`DrawPartialyBuilt` fn_00816AD0, LH3DStaticObject vt +0x110, called by
  `MultiMapFixed::DrawBuilding` 0x517F90 and `LH3DCitadel` 0x882A40): pct clamped to [0, 1] (0x816AD6); cut plane d =
  half·scale·pct·2 + y (0x816D6F..0x816D9B); a primitive draws nothing when the plane is under 0.2 (0x8AB244) above the
  origin (fn_0085C7F0 0x85C82F). `DrawBuilding` draws nothing at pct 0 (0x517FE0).
- Only the status-0 sub-meshes and the scaffold (S = max(highest status, 1), 0x816EA8) are drawn; any other status is
  skipped (0x816F54).
- Scaffold: below pct 0.2 it rises, its matrix moved along its up row by −2·half·(1 − 5·pct) (0x816F8B..0x816FEA,
  0x8DAD58 = 5, 0x8C7CE0 = −2); above 0.8 (0x8C4A04) it is cut from the top at d = (1 − (pct − 0.8)·5)·half·scale·2 + y
  (0x81708D..0x8170CB); in between it stands whole. The scaffold draws with no inner walls (0xC392B4 = 0, 0x816F60).
- Inner walls: offset 0xC392AC = 0.2 when the material is two-sided (+5 bit 0), otherwise 0xC392B0 = 0.35; the temple's
  `LH3DCitadel` draw 0x882A40 sets both to 1.0 around its call (0x882A7B / 0x882A85) and also draws at pct 0 (the
  scaffold then lowered by the whole height).
- Every partial-build pass is two-sided (`g_NoBackfaceCull` = 1 at 0x85CD61, CULLMODE NONE at 0x85CE91..0x85CEB1).
- The cap (fn_00820B20) is drawn only if the outer pass cut something, the inner pass cut as many times, and LH3DObject
  +8 bit 0x200 is clear (0x85CF18..0x85CF43); no cap after an overflow of the segment list (at most 1000 segments,
  fn_00820EC0); its colour is 0.75 × the object diffuse (+0x4C), unlit (0x820B46..0x820B93). No writer of the 0x200 bit
  was found in the exe.
- Cut points (fn_0081D790) lie on the world plane (0, −1, 0, h); the corner distances are clamped to ≥ 0.0001
  (0x8BF518) below and ≤ −0.0001 (0x8D8738) above, and every attribute is interpolated, including the lit colour.
- **Shadows**: pieces do not cast any; the broken building keeps the static shadow of its intact model.
- **Pending** (they depend on systems that do not exist yet): snow on the FragMesh (weather storms, snow map);
  the 0.75 colour of the cap. The charring/glow of a burning house's FragMesh goes in `Buildings::AppendFragMeshes`.

## Water in impacts and when dropping

- In [water.md](water.md): the [water cells (SeaCells)](water.md#water-cells-seacells), the
  [impact against the water, the wave when bobbing and the resources that fall into the sea](water.md#impacts-and-objects-that-fall-into-the-water)
  and [sinking, drowning and being deleted](water.md#sinking-drowning-and-being-deleted) (`HasSunk`, state DROWNING 16,
  `ToBeDeleted`). What remains here is dropping from the hand, which decides whether the object lands or stays in physics (also
  over water).
- **Dropping (gently or throwing): `Object::InitialisePhysicsFromHand` 0x636F00** (matched code in bw1-decomp
  `src/Black/Object.cpp:447`), fully ported in `physics::from_hand::InitialisePhysicsFromHand` (`src/ECS/Physics/FromHand.cpp`). Every
  drop goes through here: packet 0x12 calls `ApplyThisToMapCoord` (tree over a wood store → the store
  keeps it, 0x74BFD0) and then `ThrowObjectFromHand(status, 0)` 0x6385E0 with the **spring velocity** (not zero):
  1. `PhysicsObject::AddObject(obj, v, 0, NULL, status)`; `lanzado = v.x² + v.z² > 4` (> 1 if a creature throws it).
  2. `AdjustToGroundLevel(lanzado, !IsAnyKindOfTree)` 0x7FCB80 (not thrown: lowers the body until its lowest
     vertex touches the ground, aligned to the normal except for trees; thrown: only pulls it out of the ground), `ZeroForces`,
     **`RaiseUntilNotIntersecting`** 0x644800 and the FROM_HAND flag (4).
  3. it lands if it was not thrown, **it did not have to be raised** (the body's y did not change; exception: a villager of a computer
     player) and `IsDryLand || the altitude of the rounded cell (fistp) > 1`. `Living` and `Fence` also need
     normal.y ≥ 0.7 (0x6372A6). `IsFence` 0x609110 = `MobileStatic` with mesh 0x38 (American fence) or 0x51/0x52
     (Celtic fences). A thrown fence that hits a store of wood becomes wood (`MobileStatic::ReactToPhysicsImpact`
     0x608FC0; [objects-and-resources.md](objects-and-resources.md#static-objects-mobilestatic-rocks)).
  4. Lands → LANDED flag (8). `Living`, `Fence` or tree (not on fire) on `IsLand` leave physics **on the
     spot** with `RemoveObject(obj, 1, 1)`: they take the body's pose and their `EndPhysics` runs (villager: LANDED or
     drowns; animal: LANDED; tree: replanted, see objects). A tree **tilted** in the hand (|x| or |z| of
     `GetYXZ` > 0.2) or with `dont_replant` stays in physics **without** LANDED. Everything else (rocks, statues, thrown
     pots…) **stays in physics with LANDED**, resting and aligned, until it stops by itself (~1 s for a rock on flat ground).
  5. Does not land (the sea, outside the map, raised on top of something, thrown) → keeps flying or falls;
     `Villager::CreateDroppedResource` 0x750940 (drops the log it carries: pending, openblack does not carry wood),
     `Reaction::CreateReaction(obj, 9 REACT_TO_FLYING_OBJECT)` (pending, no reactions) and
     `Creature::CheckAllCreaturesForCatching` (pending, creature).
  - **Hand pot** (`Pot::InitialisePhysicsFromHand` 0x66DF00): with |v|² ≤ 5 (all three axes) there is no physics:
    `StartMultiPutdown` (particles), `Pot::AddResourceToPos` (merges with piles and storehouses, **is lost in the
    water**), `GoolooGooloo` and `ToBeDeleted`; faster, it flies like any object.
  - `RaiseUntilNotIntersecting` 0x644800: adds as bodies at rest (fn_00644DF0) the objects in the cells under
    the square C ± R that `InteractsWithPhysicsObjects`, and raises the body by what
    max(fn_007FDD60(it, other, (0,−1.0)), fn_007FDD60(other, it, (0,1,0))) says with each body whose spheres overlap,
    repeating while any pushes > 0.001. fn_007FDD60 = the largest `dot(q − hit, dir)` of the vertices q whose
    backward ray (fn_007FC310, t < 0 without limit, unnormalised normal with threshold −0.0001) finds a face
    of the other. A villager is not raised onto something pushed by a `Living` (flag 2, `Object::PushObject` 0x6396BA).
    It skips the object's own thrower (po +0x1C, 0x644987), objects without a 3D object (0x6449D5), and anything whose
    `ShouldPhysicsRaiseObjectUntilNotIntersectingThis` (vt +0x7A4, 0x6377D0) is 0, for example LandscapeVortexIn or
    MapShield. In the original it repeats with no round limit.
  - At 0x637231 the "altitude > 1" test of step 3 is `cmp [cell + 4], 1; jbe` on the rounded cell; the tilt test of
    step 4 reads `LHMatrix::GetYXZ` 0x7FAB30 of the 3D object's matrix (0x637390).
  - `Object::InitialisePhysicsFromHand` ends any release prediction of this object first (0x636F69..0x636F89); the
    flying-object reaction (0x637412) uses the thrower's player (`GInterfaceStatus::GetPlayer`, 0x637405).
  - **Release spin** (`GPacket::ProcessPacket` case 0x40, 0x63CDF7..0x63CEB9; sent by `CHand::GetRequiredState`
    0x46CE01..0x46CE8D about 180 ms after the release, with the hand's displacement since then): if the object has a
    body, its external torque (PhysOb +0x128) += (−m·dz·1.6·|v|, 0, m·dx·1.6·|v|), with m = PhysOb +0x134 and 1.6
    (0x931338). `ZeroForces` copies it every substep and `GameTurnUpdate` clears it at the end of the turn.
  - **The villager's dropped log** (`Villager::CreateDroppedResource` 0x750940; called at 0x6373F4 with the hand's
    angular velocity, which is 0): `InitialisePhysics(v, w, thrower 0, add 1)`, then, with a body, po +0x90 = the
    angular momentum when given, flag 0x10 (no collision with objects, 0x750A6D), `AdjustToGroundLevel(false, true)`
    and `RaiseUntilNotIntersecting` (0x750A54..0x750A89). The log's mesh comes from the `CarriedObject::Init` table
    0xC5E19C (one AllMeshes index per CARRIED_OBJECT, 0 = none; read at 0x750993).
  - `RemoveObject(obj, 1, 1)` 0x646A00: angles and position of the body, `EndPhysics`, and if LANDED and `IsLand`,
    `DropSfx` (vt+0x794: 0 except `Tree::DropSfx` 0x74BC60 = G_PlantTree_01 + tick % 3, which openblack plays when
    replanting); `Tree::EndPhysics` 0x74B830 only replants with LANDED on land and without fire, otherwise, dead tree.
    `Tree::EndPhysics` searches the cells (0x74B9C0) before it inserts the tree, so a replanted tree does not find
    itself.
  - Hook: `OPENBLACK_HAND_TEST_DROP` (types 4 tree, 5 animal added). Checked on Land1 (1788.4; 2710):
    rock → in physics with LANDED and at rest after 0.9 s; tree → replanted; villager and animal → out of physics
    on the spot; pot → pile; villager in the sea (1464; 2016) → sinks and DROWNING 600 turns; rock over a
    building at (1780.4; 2713.3) → raised to y 38.3, without landing.

## Rocks that split (Rock::SplitInTwo 0x6E7560)

- A rock with 2D radius > 3.6 cannot be picked up: **clicking on it hits it immediately**. A rock that can be
  picked up is hit with a short click (< 225 ms). Condition: height > 0.7. No hit counter.
- Two rocks of the same type come out, scale × 0.7935 (∛½: half the volume), only the Y angle, at
  `Pos ± (cos a, 0, sin a)·0.7935·R2D` with random `a`; the original is deleted and the halves enter physics (they fall or
  keep flying with its velocity). Sound G_RockTap_01..04 (130 + counter 0xD559AC) in 3D at the hand's point, with the rock as owner (0x6E751D).
- Strong impacts also split them (see damage).
- `Rock::ValidForPlaceInHand` 0x6E7030 holds the "2D radius > 3.6 cannot be lifted" rule.
- The first draw is `GameFloatRand(2π)` at 0x6E7576; the factor 0.7935 is 0x942110. The offset is `GetPosFromAngle(a,
  R2D·0.7935)` (0x6E75B1), and the halves are made at pos ± o with `MapCoords` operators (0x6E76A9 / 0x6E76CE). Both
  keep the rock's altitude, so each half sits at the ground at its own point plus that altitude.
- A burning rock passes its fire on to both halves (fn_007308F0) before it is deleted.

## Who owns what: class handlers and the hand's API

The original keeps each class's part of the physics in its virtuals (`InitialisePhysics`, `ReactToPhysicsImpact`
vt +0x7AC, `EndPhysics` vt +0x790, `HasSunk` vt +0x7B8). openblack keeps that split with
`PhysicsObjects::SetClassHandlers(PhysicsClass, ClassHandlers)`: one entry per class (`PhysicsObjects::ClassOf`:
Villager, Animal, Tree, DeadTree, Pot, Rock, Fragment, Building, Shield, Other), set by the system that owns the class.
`reactToImpact` gets an `ImpactInfo` (G of the turn, who hit it, the thrower, whether the hand threw it). An empty handler
keeps the physics' own code for the class. The hand registers Tree, DeadTree and Pot (`HandPhysics.cpp`); the
villagers' and animals' code registers Villager and Animal (`ECS/LivingPhysics.cpp`).

- Scaffold's virtuals: before `Object::InitialisePhysicsFromHand`, `DestroyThingsInWay` when flag 0x800
  (0x6EA8A3..0x6EA8AD); after it, flag 0x400 = LANDED && !dont_replant (0x6EA8D2..0x6EA903), not called when no body
  was made (0x6EA8D4); `Scaffold::CanBecomeAPhysicsObject` 0x6EA910 (not in a workshop slot, no site);
  `Scaffold::EndPhysics` calls `Object::EndPhysics` itself in the middle of its work (0x6E87B6).
- `Villager::ThrowObjectFromHand` 0x756AE0..0x756B1C runs before Object's: with dont_replant, a villager of no player
  or of the hand's player gets `SetVillagerDisciple(0, 0, 0)` (`GPlayer::IsMemberOfThisPlayer` 0x64D750).
- `Villager::InitialisePhysics` 0x5EFEF0 runs on every `InitialisePhysics` (not from the hand): `CreateDroppedResource`,
  then `Living::InitialisePhysics` 0x5EFE10 (FLYING; `StorePreviousState` unless IN_HAND 0x18, and `SetTopState(FLYING)`
  must return 1, otherwise a particle system's take is refused).
- `Object::DropSfx` 0x63A7B0 is empty. `Fixed::EndPhysics` reaches `Object::EndPhysics` through 0x52E054 / 0x52E0CB.
- The class's `EndPhysics` 0x6375A0 inserts into the map cells with `InsertMapObject` vt +0x544 at 0x63762C; with NULL
  it also removes the flying-object reaction (9) and calls `ConsiderCreatureMimickingWhenObjectLands`
  (0x63763D..0x63764A).
- An object a particle system carries ([vortex.md](vortex.md#the-objects-a-particle-system-carries-tornado-and-vortex)):
  the take (fn_006CA060) takes the object out of the map cells and sets GameThing +0xA |= 0x10; `Take` is refused when
  the object is IN_PHYSICS (so a second take is refused, 0x637487) or IMMOVABLE, after the class's part (0x637480). On
  `Release` (0x6C9F3B..0x6C9F54), a villager, animal or tree lands upright with the atom's yaw, while a static, mobile
  object or pot keeps the atom's tilt. `Tree::EndPhysics` returns at once for a carried tree (+0xA & 0x10, 0x74B837 /
  0x74B84B), so the tree is not put back in the map cells and stays IN_PHYSICS. Pot (0x66DBD0) and Fixed (0x52DF50)
  have their own `EndPhysics(NULL)`.
- `IMMOVABLE` is GameThingWithPos +0x24 & 0x1000 (SET_ID_MOVEABLE); `GameThingWithPos::IsCannotBePickedUp` 0x401A10
  (vt 0x180) is only overridden by HanoiBlock 0x6DE440.

A class's `dropSfx` (DropSfx vt +0x794) is played by `RemoveObjectWithEndPhysics` (RemoveObject 0x646B2E..0x646B48) for
the object EndPhysics returned, when LANDED and on land; only the tree has one (`Tree::DropSfx` 0x74BC60, registered by
`HandPhysics.cpp`), and a body that comes to rest in GameTurnUpdate (0x645EB5) never plays it.

The hand calls `physics::from_hand::Throw(object, spring velocity, dont_replant)` (`Object::ThrowObjectFromHand`
0x6385E0, after it took the object out of the hand) and `ForceDrop` (`GInterface::ForceDropHeld` 0x5D4350); what the
throw needs from the hand (putting a hand pot down, wood stores, dead trees, roots) comes through
`from_hand::SetHandHooks`. A building that is deleted calls `physics::Buildings::OnBuildingDeleted` first.
A building site gets its partly built mesh from `PartialBuild::BuildMesh(building, intact mesh,
percent, tag)` (fn_816AD0 in the building's own space; Feature::Redraw uses it too) and frees it with
`PartialBuild::EraseMesh`.

**Rows of PhysicsConstants** (`GetPhysicsConstantsType` vt +0x788): a fence (`IsFence` 0x609110) is row 18, tested
after the three "rock" tests and before the toys (`MobileStatic` 0x609270); anything else with no override takes
`Object` 0x6376A0 = `CanBecomeAPhysicsObject ? 1 : 0`. Not ported (their classes are not in openblack): Ball 2, Poo 12,
LandscapeVortexIn 13, Scaffold 17, Creature 0, CitadelHeart 0, FieldCrop 6.

**The release prediction** (GInterface state 12 0x5D4E65..0x5D513D, `physics::from_hand::PredictRelease`): at the
release the hand builds a body that is not in the list (PredictionPhysOb 0xD47088) from the held object at its pose,
sets its angular momentum from the hand's angular velocity h (`L = h·M8`, the inertia tensor with no body rotation,
0x5D4EB3..0x5D4F2E; stored negated, because the original's torque is F×r and this port's r×F), caps the hand's velocity
at 124 (the "fast" flag is the uncapped `|v|² > 1`), runs `AdjustToGroundLevel(1, !(tree || fast))` and then
`min(N, 15)` turns of 20 substeps with no other body and no game logic (fn_00644F20), N = min(ping / 100, 5). The 0x4D
packet carries the velocity, L, the origin (fn_007FD140) and the angles after those turns. Until the throw is applied the
held object is drawn along the recorded poses (fn_00646FE0 0x6471D7..0x647446, `PhysicsDrawPose`); the prediction ends
at InitialisePhysicsFromHand (0x636F69), at a turn start when the object is no longer available (0x6451DA) and at
DeleteAll. `from_hand::Throw` takes the packet's L (0x637013..0x637031). In single player N is mostly 0 or 1.
(pending) the caller sets the packet's pose before Throw, and the hand skips drawing the held object while it is the
prediction's.

Release prediction details: `PhysOb::Initialise` 0x7FB780 on the held object, `Object::SetUpPhysOb` 0x6376B0
(SetUpConstants only if CanBecomeAPhysicsObject and not IMMOVABLE); fn_00644F20 stores the history at 0xD46D80 (N at
[0xD47080], turns since at [0xD47084], incremented in `GameTurnUpdate` 0x646053..0x64605B); the drawn origin uses the
scale [0xD4708C] and the com [0xD47180..0xD47188]. The "fast" test is the uncapped |v|² > 1 (0x8AA390).

**A thrown object always has a body.** `PhysicsObject::AddObject` 0x6443A0 only fails for a class that cannot become a
physics object, an IMMOVABLE one (flag 0x1000) or one already flying; the list never refuses. The angle the hand gives
(`ThrowAngularVelocity`, CHand +0x48D4) is the prediction body's L after `min(ping / 100, 5)` steps against the ground
(fn_00644F20): 0 in a single player game (inferred: ping 0), as in openblack. (approximate) an object
openblack cannot make a body for (no mesh; the original would crash in `PhysOb::Initialise`) is put on the ground
(`physics::from_hand::PlaceWithoutBody`); openblack's old ballistic flight for it is gone.

## The physical shield test

Before and after the penetration fix (`len` → `predLen`, see [Engine](#engine-physob-0x7fb7300x7fe7b0)), traced with
`OPENBLACK_PHYSICS_TRACE=1`: a PhysicalShield of radius 40 and a rock (mass 6.75, radius 0.37) thrown at it at
35.5 m/s.

| turn after the throw | before (len) | after (predLen) |
|---|---|---|
| +23 | 33.7 m/s, 21.5 m from the centre, 0 contacts | 33.7 m/s, 21.5 m, 0 contacts |
| +24 | 33.8 m/s, 38 contacts, G 0.12 | 25.7 m/s, 15 contacts, G 8.3 |
| +25 | 33.9 m/s, through the shield | 8.3 m/s, 5 contacts, G 20.4 |
| +26..+32 | lands inside the dome | slides along the outside (14.3..17.7 m, 7.0..5.2 m/s) |

## Pending

- **The creature catch hook** (`CheckAllCreaturesForCatching` 0x47CBD0) stays unset. For each creature it calls
  fn_0047C960 (0x47CBF3, the creature skipped on 0), then the locked-creature skip (Flags & 0x10, 0x47CC00), then
  fn_004E3D30 (0x47CC2C, compared with the object's `GetWeight`), `CanBePickedUp` and fn_0047C9D0 (0x47CC52); the three
  are not read. An RE request for them, in that order.

- (not verified) the angular damping step `pow(d4, 0.005)` of `PhysOb::SetUpConstants` 0x7FB810: openblack takes the
  float overload of `std::pow`; the original's CRT pow under the game's 24-bit FPU precision (fn_007DEE00, 0x7DEE0D) may
  differ in the last bit.
- (not verified) `CollisionSounds::AttemptToAddSoundEvent`, verbatim: "0x6466AA / 0x6466B3: PhysicsObject +0x178 =
  PhysOb +0x150, the body radius (max |vertex - CoM| x scale); rate 1 / r and growth 2 r with no guard". Not clear what
  "rate" and "growth" refer to (perhaps the dust puff's growth).
- Snow on the FragMesh (needs the weather: snow storms and the 128×128 snow map); the 0.75 colour of the cap of the
  "half-built".
- Buildings: repair by villagers (building site, wood; status not verified, see above), creature hits; what happens to
  the building itself (alignment, aggressor, inhabitants, the town's emergency, −0.2 of a building site) is in
  [buildings.md](buildings.md#life-and-damage).
- Villagers and animals on landing: the original's three postures (see the end-of-flight bullet above and
  [villagers.md](villagers.md)); whether openblack draws all three is not verified.
- Complete villager death (`VillagerDead` 0x7506C0) and the creature's mimicry when something dropped by the
  player sinks: in [water.md](water.md#pending).
- From dropping: the disciple sound
  (`MakeDiscipleSFX`) and `SetVillagerDisciple`, reaction 9, the villager's log and the creature stuff (catching,
  imitating, toys). The mesh branch of fn_007FDD60 (fn_008683C0) is not needed: no openblack body uses it.
  The `Tree`+0x5C & 2 flag of `Tree::EndPhysics` (0x74B882: only `Fixed::EndPhysics`, neither replanting nor dead
  tree) is set only by the `MagicTree` constructor (0x5FCF8D, `or byte [esi+0x5C], 2`; found by a sweep of the whole
  `.text`): they are the trees of the forest miracle, which this tree does not have.
- The terrain normal without the original's quantisation.
- Tooltip "Golpear para Romper" ("Hit to Break", 0xEF7) and checking the player's influence before hitting a rock.
- (unclear state) the invented splash cool-down: it is to be removed, as made up and not in the original (fidelity
  pass of the physics). No page describes it and no cool-down was found in `src`; it may have been removed already
  with the water port. Check before closing it.

## Test hooks

`OPENBLACK_TEST_PHYSICS="x,z,height,vx,vy,vz[,scale[,n]]"`, `OPENBLACK_TEST_HIT_VILLAGER="speed[,scale[,index]]"`,
`OPENBLACK_TEST_THROW_TREE="x,z,vx,vy,vz"`, `OPENBLACK_TEST_HIT_ABODE="speed[,scale[,index[,n]]]"` (n rocks against a house, one every 1.5 s), `OPENBLACK_HAND_TEST_SPLIT="x,z,scale,rounds"`,
`OPENBLACK_PHYSICS_TRACE=1` (position, velocity, contacts, G, density and radius of each body per turn),
`OPENBLACK_TEST_SEA="x,z,type[,height]"` (type = `villager|animal|tree|pot|rock`: creates it at that height above the
point and puts it into physics with no velocity; writes the cell, the density and `GET_LAND_HEIGHT` there and on the reference
land, and traces the counter of the drowning villager every 100 turns),
`OPENBLACK_HAND_TEST_DROP="x,z,seconds[,type]"` (the hand holds a rock 0, a pot of 300 food 1 or of wood 2,
the first villager 3, the first tree 4 or the first animal 5, and **drops it gently** at (x, z) after those seconds of game time; the log says whether it ended up in physics).

## Sources

- Disassembly of `runblack.exe`.
- `bw1-decomp` `src/Black/Object.cpp:447` (`Object::InitialisePhysicsFromHand`, matched).
