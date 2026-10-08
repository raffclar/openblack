# Trees and forests

Everything about the trees: uprooting and the tug, what can be picked up, dropping and replanting, the wood (info.dat and
the API for the villager jobs), the searches for trees and forests, the growth, the drawing, the tree's own fire and the
sacrifice. Everything is **faithful** (read in runblack.exe or in info.dat) except what is marked **(inferred)**,
**(approximate)**, [assumed], *deviation* or **pending**. openblack code: `src/ECS/Trees.{h,cpp}`, `HandTrees.cpp`.

> **Code rules.** Tree and forest state lives in ECS components (`components::Tree`, the forests) and the systems are
> reached through Locator services, never globals; meshes load through the resource caches; the growth, wood and
> search formulas stay pure functions, unit tested with fakes in `test/`; comments describe behaviour in plain
> English, with no decompiled names or addresses (those belong here). See [openblack-internals.md](openblack-internals.md).

- [Uprooting and picking up](#uprooting-and-picking-up)
  - [Tug](#tug-handstatetug-enter-0x5b7df0--update-0x5b8070-in-handtreescpp)
  - [Pick-up rules and BigForest](#pick-up-rules-and-bigforest)
- [Dropping and replanting](#dropping-and-replanting)
- [Alignment](#alignment)
- [Wood and the GTreeInfo table](#wood-and-the-gtreeinfo-table)
- [Trees for the villager jobs](#trees-for-the-villager-jobs-api-srcecstreesh)
- [Searches for trees and forests](#searches-for-trees-and-forests-for-the-villagers)
- [Growth and forests](#growth-treeprocess-0x74a290-treegrow-0x74a3f0)
- [Drawing](#drawing)
- [Fire](#fire)
- [Sacrifice](#sacrifice)
- [Test hooks](#test-hooks) · [Sources](#sources) · [Pending](#pending)

## Uprooting and picking up

- Uprooting: the tug starts **on press** (StartGrab 0x5D1740 calls `CHand::PickUp(obj, 1)` immediately; the 225 ms
  threshold is only for the other objects); the tree leans towards the hand and comes out when the hand has moved more than
  weight/1000 m horizontally from where it grabbed it (details and difference with the original in
  [Tug](#tug-handstatetug-enter-0x5b7df0--update-0x5b8070-in-handtreescpp)). TreeBreak sound, pile of roots
  (mesh 593, 15 s) and dangling roots (mesh 592).
- The pile of roots (the crater) is an `LH3DObject::Create(1)`, **morphable** (fn_00825240 → UpdateMelting vt+0x1E8 once
  when created): it moulds itself to the terrain like the fields and stores (`ecs::ground_marks::Create` from `HandSystem::Uproot`, see
  [rendering-objects.md](rendering-objects.md#meshes-stuck-to-the-ground-land_morph)).

### Tug (`HandStateTug` Enter 0x5B7DF0 / Update 0x5B8070, in HandTrees.cpp)

- **Original**: at the start, the anchor is the base of the
  tree and the drag plane passes through it with the terrain normal, at the height of the hand seen at the anchor's
  distance. After 0.13 s (the state change fade), every frame the hand goes to the intersection of the mouse ray with that
  plane; the grip is at `base + up × drop` (drop = 0.1 × height, minimum 3.2 × hand scale × 0.3 at
  the start); a spring `F = 1000 × (hand − grip)` (cap 600000) tilts it with torque `(r × F)/1000` and quadratic
  friction 4 around the base, and the trunk stretches up to ×1.3 (Zoomer 0.3 s). It comes out when `|F| > GetWeight`
  (scale³ × info.dat weight): grabbed far from the grip point, it comes out straight away. Released earlier, it returns to its posture.
  At the end of each Update (also in the first 0.13 s) the hand is placed at the grip of the stretched trunk
  (`CHand+0x78 = matrix × (0, drop, 0)`); it is only drawn there, the next Update puts it back on the plane.
  **Consequence (2026-09-30, to be confirmed with the original)**: since the hand before pressing is on the mouse
  ray, the plane ends up at the height at which that ray crosses the tree's axis, so the first tug is that height minus
  the drop: a beech of scale 1 (18 m, grip at 1.8 m, weight 1000) only leans if pressed within ~1 m of the
  grip (from 0.8 to 2.8 m above the base); pressed at the crown it comes out after 0.13 s. The openblack from before physics
  only measured the horizontal distance from the cursor to the base and leaned wherever it was pressed.
  **What openblack does (2026-09-30, at the request of the user, who remembers the original that way)**: the literal spring
  is not used (in addition the ×1.3 stretch made the tree go up and down). On press the grabbed point and its
  distance along the mouse ray are stored; the tug is how much the hand (the ray at that distance) has moved horizontally since
  then. The tree leans towards it up to 0.25 rad and comes out when it exceeds weight/1000 m (scale³ × info.dat
  weight). Grabbed anywhere and without moving the mouse, it does not come out. If one day the original can be tested, it
  can be checked by reading the memory of runblack.exe (plane, hand, grip, state).
  Hook: `OPENBLACK_TEST_TUG="x,z,wait,hold"` (+ `OPENBLACK_TEST_TUG_MOUSE2="x,y"`, the cursor moves 0.5 s
  later), traces with `OPENBLACK_HAND_TRACE=1`.
- **Drawn only**: HandStateTug writes nothing to the tree (Object +0x14, angles).
  - Its matrix goes to the G3D (0x5B88ED, the up row × the stretch at 0x5B891D) and is re-applied after Tree::Draw by CHand::UpdateHeldObject 0x46E490 → 0x5B8E60.
  - Exit is HandState::Exit 0x5B02D0 (a bare `ret`), so a release without the uproot draws the tree upright from its logic on the next frame.
  - openblack's lean is now a `HandDrawPose` on the tree, and the Transform is untouched. Tree::Draw's bend / sway matrix is skipped for an object posed by the hand or the physics; its grey always applies.
  - The exact read of the spring: the grip is reset to 0.1 × height every frame (no minimum after the first Update), and the uproot needs `600000 ≥ 9.81 × weight` and `|F| > weight`.
  - The user's formula above stays until the user decides.

### Pick-up rules and BigForest

- **Pick-up rules**: `Tree::ValidForPlaceInHand` = 1 and `IsTuggable` = 1 for the 22 types, at any scale (bushes,
  hedges, palm trees, copses, inside or outside villages). Only the flag 0x2000 (saved games and
  puzzles), being outside the influence or a locked selection prevent it; then it goes through the "tap" path, which for
  trees does nothing. `BigForest` (ported): it is not tugged; on grabbing (225 ms) `InterfaceSetInMagicHand` 0x4393C0
  does `RemoveResource(WOOD, 350)` (the Conifer's wood) and puts a new Conifer in the hand (scale 1, angle 0).
  `RemoveResource` 0x4390D0: the forest's wood (+0x84; on creation woodValue × scale, `Create` 0x438EC0) goes down by 350 and
  **only** when its wood drifts more than 250 away from life × scale × woodValue is it rescaled to wood/woodValue and plants at the
  edge; without enough wood it gives what is left and the forest is deleted (details in
  [Searches for trees and forests](#searches-for-trees-and-forests-for-the-villagers)).
  `AddTreeAround` 0x439220: up to 10 random angles at its radius; on land and without an object in the cell with distance + radius
  less than 4, a Pine of its forest (+0x80), scale 0.05, random angle and maximum size 0.75 + random(0.5). openblack:
  `HandSystem::TakeTreeFromForest` (HandTrees.cpp), `BigForest::wood`, hook `OPENBLACK_HAND_TEST_FOREST=1` (Land1:
  15000 → 14650, scale 0.977). DeadTree/FelledTree: picked up without a tug. Uprooting: `G_TREEBREAK` + 1 evil
  alignment push (`GAlignment::Update`); replanting, good ([Alignment](#alignment)).
- `BigForest::InterfaceSetInMagicHand` 0x4393C0 calls RemoveResource(WOOD, 350) at 0x4393EC. It then calls `Tree::Create(&forest +0x14, Conifer, forest +0x80, 1.0, 0.0, 1.0)` 0x749EE0 (0x43940E), at the forest's MapCoords (0x43940A), not the hand's point.

## Dropping and replanting

- **Dropping** (`Object::InitialisePhysicsFromHand` 0x636F00 + `Tree::EndPhysics` 0x74B830): the tree counts as "put down
  carefully" (flag 8 LANDED of the physics object) if it is not thrown and it did not have to be raised (`IsDryLand` or cell
  altitude > 1). The normal test (y < 0.7 ⇒ it does not land) is **only** for living beings and fences, not for trees
  (bw1-decomp `src/Black/Object.cpp:539`; the previous version of this note applied it to trees too). A
  LANDED tree without `FireEffect` over `IsLand` leaves the physics at once if it arrives almost upright (the x and z angles of its
  YXZ matrix ≤ 0.2 rad ≈ 11.5°; a tree in the hand takes the hand's "up", which follows the surface, so on a
  slope it is tilted) and `Tree::EndPhysics` replants it; tilted (or `dont_replant`) it stays in physics without LANDED, falls and
  ends up as a dead tree. Hot or burning, or LANDED on a water cell: it stays in physics with LANDED and when it stops
  `Tree::EndPhysics` turns it into a dead tree (it keeps its fire). Everything goes through the physics
  (`physics::from_hand::InitialisePhysicsFromHand`, `HandPhysics.cpp`). "Upright" = `LHMatrix::GetYXZ` 0x7FAB30 of its matrix
  with |x| ≤ 0.2 and |z| ≤ 0.2 rad (x = asin(row2.y), z = atan2(−row0.y, row1.y), checked by emulation). Dropped over the sea (neither `IsDryLand` nor cell altitude > 1)
  it **does not land**: it floats ~19 s until it sinks (see [physics.md](physics.md#water-in-impacts-and-when-dropping)). The sound
  (`Tree::DropSfx` 0x74BC60, G_PLANTTREE + tick%3) is played by `PhysicsObject::RemoveObject` 0x646B44 on **every** careful
  drop that ends on land, replanted or not. Thrown = always a dead tree.
- **Forest on replanting** (0x74B8BF): a spiral over the map cells up to 25 + 10 m; for each fixed object
  `d = distance − its 2D radius`. An object of a village (or part of the temple) closer than 25 m ⇒ the tree is "of a village"
  (bit 1 of +0x5E = `isNonScenic`, it is set to **1** inside the village!) and joins the forest **of the village**, which beats
  any other; otherwise, it inherits the forest of the nearest tree with a forest (with no limit of its own, only the 35 m of the
  search); with none and outside a village, it creates a new forest. Effects: white smoke `SmokyStuff` on the ground (in
  openblack, the grab dust), `SPOT_VISUAL_FOREST_CREATED` (0x2C) **whenever it is not in a village**,
  `StartImmersion(0x2E)`, creature mimicry and good alignment (see [Alignment](#alignment)).
  The original takes the village's forest from a list that the village keeps (Town +0x608): the last scenic forest of
  the list (details in "Replanting in a village", in
  [Searches](#searches-for-trees-and-forests-for-the-villagers)); in a village
  without a scenic forest it keeps the nearest tree's forest and, with none, the tree is left **without a forest**. openblack
  already does the same (`ecs::TownForestId` walks that list, `HandTrees.cpp`). *Before*, openblack did not model the list and
  the first tree planted in a village created its forest.
- The search for a forest (`Tree::EndPhysics` 0x74B982..0x74BADB) walks at most 1000 cells (0x3E8, 0x74B982) and stops at the first cell farther than 25 [0xC22FA4] + 10 [0x8AB414] m.
- It walks only the fixed list of each cell (GetFirstObjectFixed 0x74B9C0, GetMapChild 0x74BA66). For each object, d = fn_00605CD0(object, tree) minus its Get2DRadius (vt +0x64, 0x74B9D6..0x74B9E6).
- An object with d < 25 (0x74B9EE) puts the tree "in a town" (0x74BA7B) if it has a town (GetTown vt +0x48) or is a citadel part (`IsCitadelPart` 0x6380C0: info type 8). With a town, every scenic forest of its list becomes the best at distance 0, so the last one wins (0x74BA9C..0x74BAB4); then the search moves to the next cell (0x74BAB6).
- Otherwise a Tree (RTTI 0x74BA20) with a forest (GetForest vt +0x86C) nearer than the best lends its forest (the best starts at 99999 = 0x47C34F80, 0x74B902; test at 0x74BA46). Later cells go on with the same best, after a town too.
- Tree +0x5E bit 1 (isNonScenic) is the script's own flag (CREATE_NEW_TREE 0x716324); the hand sets it to "inside a town" when it replants the tree (`Tree::EndPhysics` 0x74BB5A), taking the "in a town" answer at 0x74BB10..0x74BB62. The new Forest outside a town is made at 0x74BB53.
- **The replant's creature mimicry** (`Tree::EndPhysics` 0x74BB99..0x74BBB1): after `Forest::AddTree` (0x74BB62) and the spot visual (0x74BB7C), with a player (`PhysicsObject::GetPlayer` 0x647460 of the tree's body), `ConsiderMakingCreatureMimicPlayer(status = body +0x24, PLANT_TREE 11, the tree, magic 0)` 0x74BBB1, then `GAlignment::Update(player, tree, 1)` 0x74BBBD, then `Fixed::EndPhysics` 0x74BBCA. openblack: `HandSystem::Replant` publishes the deed (`creature_mimic::Consider`, PLAYER_ONE as its alignment line) after the spot visual and before the tree goes back in its cell ([creature.md](creature.md#mimicry-and-the-town-hooks)). **Difference:** openblack updates the alignment earlier, before the smoke and the spot visual, so before the deed; the original updates it after the deed. Harmless today (the alignment update draws nothing, and no creature exists to hear the deed); putting it back in the original's place is its own measured change (Pending).
- The dead-tree branch (`Tree::EndPhysics` 0x74BBD9..0x74BC37): fn_00510B70 makes the DeadTree, which takes over the tree's 3D object and fire (fn_00730960), and keeps the GetWoodValueMultiplier in +0x9C. InsertMapObject (vt +0x544, 0x74BC0A) puts it in the map; the tree's ToBeDeleted (0x74BC37) removes the tree.
- The uproot packet: `HandStateTug::Update` 0x5B8070 ends the tug with only the mark (0x5B847E) and CHand +0x4908 = 0 (0x5B848C); then state 13's GenericPickup sends packet 0x13 with the tree, state 7. During that wait of up to one turn the original's CHand already draws the held object (every pick-up but the forest's, whose tree is made in the handler).

## Alignment

Uprooting a tree with the hand is an **evil** act (`Tree::InterfaceSetInMagicHand`) and replanting it (`Tree::EndPhysics`)
or the tree that the water plants (`Tree::ApplyWaterSpell`) are **good**: ±`treePullPutAlignmentChange` via
`GAlignment::Update` 0x4145A0. The value (`GAlignment`, GPlayer +0x60), the weight by the current alignment, the rate per
turn, the scripts and what is missing are in
[magic.md](magic.md)
(`src/ECS/Effects/Alignment.*`, `components::Alignment`).

## Wood and the GTreeInfo table

- Over a store = wood `woodValue·scale·GLandBalance[5]` (`Tree::GetDefaultResource` 0x74B7A0, × life). A **dead
  tree** gives less: `DeadTree::GetDefaultResource` 0x511330 = `woodValue·scale` without life or land balance.
- GTreeInfo table (info.dat, runtime = record + 0x10, stride 0x140): wood 800 Oak, 700 Beech/Cedar/Copse,
  500 Birch/Olive, 400 Cypress, 350 Conifer/Pine,
  300 palm trees, 100 hedges, 15 bushes; weight 1000 (bushes 20, hedges 100);
  heat capacity 1000 (bushes 100, hedges 200); sacrifice 400/500/1000 (Oak)/250/350/100/200/110; combustion
  temperature 110 for all.
- The GTreeInfo array: base 0xDA3AD8, stride 0x140, filled at 0x749D30; Pine is index 12 (0xDA49D8).
- `BigForest::GetCarriedTreeType` 0x438DE0 reads its GBigForestInfo (+0x28) +0x124; the four GBigForestInfo rows of info.dat all have carriedType 3 (file offsets 0x8614C, 0x86264, 0x8637C, 0x86494).
- `BigForest::GetWoodValue` 0x4390B0 = life × +0x84.
- In `DeadTree::Create` (0x510C30) the code does `and word [esi+0x98], 0xFFEF`: it clears a flag bit of +0x98 that is not identified.

## Trees for the villager jobs (API `src/ECS/Trees.h`)

- **Deleting** (`DeleteTree` = `Tree::ToBeDeleted` 0x74A210 / `DeadTree::ToBeDeleted` 0x510C90): out of the forest and the
  physics, notifies the listeners (`AddTreeDeletedListener`: fire, reactions, hand) and is deleted. `DeleteForest` =
  `Forest::ToBeDeleted` 0x539C60: deletes each tree from its two lists and leaves the forest list (also the empty
  forest after 2000 turns). `ShrinkAllTrees` uses `DeleteTree` for the tree that would reach 0 (fn_0074A3A0).
  The generic deletion `ecs::ToBeDeleted` (`src/ECS/ToBeDeleted.cpp`; used by the physics, e.g. code 4 = sunk
  in the sea) also sends Tree and DeadTree to `DeleteTree`.
- **Wood**: `TreeWoodValue` = `Tree::GetWoodValue` 0x74B7B0 (life × 1 × woodValue × scale × GLandBalance[5]) or
  `DeadTree::GetWoodValue` 0x511AD0 (life × woodValue × scale³: the original cubes the scale there); `TreeWood` =
  `GetDefaultResource(WOOD)`: `Tree` 0x74B7A0 = (int)GetWoodValue, `DeadTree` 0x511330 = (int)(woodValue × 1 × scale), what
  a store receives (`object_delivery`, reached from the press over a store, `ecs::held_apply`, and from a thrown
  tree that hits a pit, `DepositInStore`).
- **Removing wood from a trunk** (`RemoveWood` = `DeadTree::RemoveResource` 0x511370): if it has ≤ n left, it is deleted and gives what
  it had; otherwise, it **shrinks**: `scale = (wood − n)/(woodValue × multiplier 1)`. Its resource is its
  `GetDefaultResource` (`Object::GetResource` 0x639520: its own if the type matches, 0 otherwise). Checked:
  dead beech of scale 1, 700 → remove 100 → 600, scale 0.857.
- **Trunk type when carrying it** (`TreeCarriedType`): `Tree::GetCarriedTreeType` 0x55D900 = info.dat `carriedType`;
  `DeadTree::GetCarriedTreeType` 0x511A20 = 0-3 if its mesh is one of the 4 trunks of `CarriedObject::Init` 0x462600
  (MeshPack 406, 347, 348, 349), otherwise its tree's `carriedType` (beech = 3, hardwood).
- **Felling** (`FellTree` = `FelledTree::Create` 0x5116A0, which only `Villager::ForesterChopsTree` 0x75FAC0 calls): the tree
  becomes `DeadTree` + `FelledTree` with its mesh (without releasing the roots: that flag is only set by `Tree::EndPhysics`) and
  enters the physics thrown by the woodcutter: `k = 0.4 × height × 0.5`, `a = atan2(x, −z)` of the direction
  woodcutter→tree (fn_007FAA50; 0 if it measures less than √0.001), velocity `(sin a, 0, −cos a)·k` (along that
  direction), spin `0.4·(cos a, 0, sin a)` rad/s **in body space** (`PhysicsObject::AddObject` 0x6443A0 does
  `L = Σ (w·I)_i · row_i` with the tree's matrix, with its Y rotation): in world space `R·(cos a, 0, sin a)·0.4`, so how it falls
  depends on the tree's orientation. In openblack the axis is **negated**: `PhysOb::Integrate` 0x7FE260 rotates the rows with
  `R(ŵ, angle)`, which in openblack's right-handed `PhysicsBody` (was `PhysOb`) is rotating by −angle (its sign convention, see
  [physics.md](physics.md)). Then `Villager::ForesterChopsTree` deletes the tree (`ToBeDeleted`): in openblack it is the same entity,
  so the deletion listeners are notified and the fire passes to the trunk (fn_00730960). Then `PhysOb::AdjustToGroundLevel(false, true)`.
  Then flag 2 (0x51186B, "pushed by a Living": a villager's body does not hit it, `GameTurnUpdate` 0x64583E),
  `RaiseUntilNotIntersecting` 0x644800 and kind +0x1A4 = 2 (0x511883, a felled tree): when it has toppled (row 1's
  y < 0.98, 0x6460A8) and is taller than 10 it sounds once, `SoundTag::Create(MapCoords, GetRandomSample(31, 1), track 0,
  mode 3, 3D, InGame)` (0x6460D5..0x6460F0), and becomes kind 3. **Not ported**: the two reactions 0x0C ("there is wood here": one from the DeadTree constructor 0x510957 and another from `FelledTree::Create`
  0x511889; `FelledTree::EndPhysics` 0x511970 does not add the landing one). `FelledTree::Draw`
  0x511990 adds the trunk to the drawing twice without fire (a `return` is missing in the original): no visible effect.
  Hook `OPENBLACK_TEST_FELL="x,z"`.
- `Forest::ToBeDeleted` also takes the forest out of every town's list: fn_0053AE10 walks the towns (g_game +0x205C84), and each drops it from Town +0x608 (fn_00741A70).
- Felling (`FelledTree::Create` 0x5116A0): the half height is stored at 0x5116C8 / 0x5116D8, k at 0x5116DE.
- The yaw a (0x51179E..0x5117F8) is kept on the FPU stack in double for the fsin / fcos, not rounded to a float. |d|² ≤ 0.001 (0x8AA3B0) gives a = 0 (0x5117BE).
- The spin is w = (cos a·k, 0, sin a·k) × (1 / h2), each product a float (0x5117F8..0x511838). `PhysicsObject::AddObject` builds the angular momentum from it over the rows of the new DeadTree's matrix (`SetUpPos` 0x63A603).
- `DeadTree::DeadTree` 0x510880 takes over the tree's 3D object (mesh, matrix, scale: 0x5108B2..0x5108B7) and info. Its player is the local one (g_game + 0x18 + 0xA60 × byte g_game +0x205A5B, 0x511747..0x511759).
- `SetScale(tree scale)` 0x51092D → `Object::SetScale` 0x639200 rebuilds the matrix only when the scale differs from the DeadTree's own 1.0 (fcomp 0x63920B). The rebuild uses the DeadTree's angles, all 0 (SetYAngle(0) 0x52DDFB), so the result is axis-aligned; at scale 1.0 the tree's matrix and its yaw stay.
- `Villager::ForesterChopsTree` 0x75FAE7 then deletes the tree with ToBeDeleted(0). The DeadTree enters the map when the physics puts it to rest (EndPhysics, vt +0x544).
- The second reaction 12 is created at 0x51187D..0x511889, after RaiseUntilNotIntersecting.

## Searches for trees and forests for the villagers

- **Trees in a cell** (`TreesInCell`; `MapCoords::FindType(6)` 0x6045C0 → `MapCell::FindTypeOnMap` 0x6015E0): type
  6 (`OBJECT_TYPE_FOREST_TREE`) goes in the cell's list of **fixed** objects (MapCell +4), and `Fixed::InsertMapObjectToCell`
  0x52DEA0 puts each object **at the head**: the first is the last one inserted. openblack has no per-cell lists:
  `Tree::mapInsertion` stores that order (when creating the tree and when replanting it, `InsertMapObject` in `Fixed::EndPhysics`).
- **Finding a tree to fell** (`FindTreeNearVillager` = `Villager::FindTreeNearVillager` 0x75FD00): the 9 cells
  around in the order of `GUtils::Spiral` (0x74D7E0, table 0xDA59FC, starting with dir 1 and steps 1: (0.0) (−1.0)
  (−1,−1) (0,−1) (1,−1) (1.0) (1.1) (0.1) (−1.1)), in each one **only the first tree** that is not
  INDESTRUCTIBLE (bit 0x4000 of +0x24: only set by puzzle objects and `LandscapeVortexOut`; no tree in a
  normal game); the nearest by `Dist2D(villager, working position)` from 99999. No more rules: no maximum
  distance, no "of a village" bit (+0x5E & 2), no size, no forest. The original returns 0/1/10 (10 = already touching it,
  `IsTouching`): that is decided by the villager side.
- **Working position** (`TreeWorkingPos` = `Tree::GetWorkingPos` 0x74C040): the tree's plus, towards the villager,
  `Get2DRadius(villager) + 0.9` (0x8C5844). The radius is **the villager's**. `Object2DRadius` = `Object::Get2DRadius`
  0x638180 = scale × max(x, z half-axes of its box).
- **Forests** (Forest, 0x58 bytes): +0x34 empty countdown, +0x38 BigForest, +0x3C = 1 "village" forest
  (scenic), +0x40 id. A **BigForest** has its Forest (its ctor 0x438CE0 creates it at +0x80 and sets +0x38 on it): now in
  openblack too, so the Conifer it gives when picked up and the Pine it plants at its edge belong to that forest.
  `Forest::Process`: it only counts as empty without a BigForest and without trees; **a scenic forest is not processed** (its trees
  do not grow and it does not plant).
- **Scenic village forest** (`MakeScenicForest` = `Town::MakeScenicForest` 0x741B40): takes the trees closer than
  250 + 10 m from the village centre that have no forest, or whose forest is scenic and that are closer to the village
  centre than to that forest's centre (2D distances); if the village had none, it creates it at the centre **only if there is any tree**. The
  cells are those of the `GUtils::Spiral` spiral from the centre cell, which stops at the first cell further than R
  (1369 cells, Chebyshev radius 18: not the whole 260 m disc).
- **Village forest list** (`AssignForestsToTown` = `Town::AssignForestsToTown` 0x73EB00, Town +0x608): it is emptied and
  filled with each forest whose nearest point (the edge of its BigForest or its centre, fn_0053ADB0) is closer than
  `GTownInfo::maxDistanceForTownForest` (250, +0x164) to the store (or the temporary point) and that has wood
  (`ForestWood` = fn_0053B280: its BigForest's plus that of each tree). It is called by `Town::AsssignTownFeature` 0x73EAC0 (for
  each village, after `MakeScenicForest`) and `Scaffold::BuildBuilding`; it is not touched when creating or replanting trees.
  The edge is `Object::GetNearestEdgeToPos` 0x636DA0 (vt+0x83C of BigForest): pos + GetPosFromAngle(angle towards
  `pos`, Get2DRadius).
- **Replanting in a village** (`TownForestId`): the tree joins the **last scenic forest** of the village's list
  (`Tree::EndPhysics` 0x74BA2B); if there is none it keeps the forest of the nearest tree already found, and with
  neither of the two, **no forest** (before, openblack created one).
- **Nearest forest** (`FindNearestForestToPos` = `Town::FindNearestForestToPos` 0x73EC10): in the village's list, the
  one with the nearest point (0 if inside the BigForest's radius) closer than 250; a non-scenic one wins, the scenic one only
  if there is no other. `FindForest(pos, max, onlyEmpty)` = fn_0053A1A0: over the global list, the one with the nearest **centre**
  (empty = without trees and without a BigForest). `ForestCentreTree` = `Forest::GetForestCentreTree` 0x53ABF0.
- **BigForest for the woodcutters**: `BigForestArrivePos` = `GetArrivePos` 0x439360 (its position plus, towards the villager,
  0.5 × its 2D radius); the trees in the hand or in flight are not in any cell (in the original they leave the map);
  `BigForestRemoveWood` = `RemoveResource` 0x4390D0: asks for n / life; if it does not have enough, it gives what it has and
  is deleted; if it does, it subtracts and **only when** its wood drifts more than 250.0 (0x8C6210) away from life × scale × woodValue is it
  rescaled and plants a Pine at the edge (`AddTreeAround` 0x439220: size 0.05, maximum **0.75** (0x8AC3F8) + random(0.5);
  before, openblack used 0.5). The hand uses the same (350 per tree, so it always rescales). Hooks
  `OPENBLACK_TEST_TREE_QUERIES="x,z"` and `OPENBLACK_HAND_TEST_FOREST=1`.
- **Who calls `MakeScenicForest` and `AssignForestsToTown`**: in the original, `Town::AsssignTownFeature` (map load) and
  the finished buildings; in openblack, nobody for now (only the hook
  `OPENBLACK_TEST_TREE_QUERIES`): until then the villages have no forest list and a tree replanted in a village
  is left without a forest. **Pending**.
- In `FindTreeNearVillager`, FindType(6, 0) is called at 0x75FD57; when the first FOREST_TREE is not a Tree (__RTDynamicCast 0x75FD5D) the cell is skipped. The INDESTRUCTIBLE test is at 0x75FD6B; the spiral starts at 0x75FD1A.
- `MakeScenicForest` (0x741BB9..0x741CC5): the spiral walks a copy of the town centre's MapCoords (fn_0073AE10), at most 99999 (0x1869F) cells (stop test fn_00605CD0, 0x741BC2..0x741BD0).
- In each cell, FindType(−1, 0) 0x741BDE .. FindType(−1, obj) 0x741CA1 walks the fixed list, then the mobile one, each from its head (CastTree vt +0xBC). The trees join the scenic forest in that order.
- A tree with a scenic forest (+0x3C, 0x741C1B) whose centre is farther than the town centre (0x741C21..0x741C44) is taken out of that forest by fn_0053A220. The scenic forest is made only before its first tree (0x741C52..0x741C8C).
- `AssignForestsToTown` walks the global forest list newest first; fn_00741AF0 inserts at the head of the town's list, with no duplicates. Its building caller is `Scaffold::BuildBuilding` 0x6E932C.
- `BigForest::ToBeDeleted` 0x438EA7 sets forest +0x38 = 0; the Forest stays. RemoveMapObject is `MultiMapFixed` 0x52E7B0 (out of all its cells).
- `AddTreeAround` 0x439220: the point is position + GetPosFromAngle(GameFloatRand(2π), R) (0x439250..0x439274). The free test (0x43928E..0x4392C9) walks the point's cell, fixed list then mobile, and the BigForest itself counts: no object may have distance (fn_00605CD0, 0x4392A1) plus its GetRadius under 4. That radius is vt +0x60 at 0x4392AE, not Get2DRadius.
- In `AddTreeAround`, maxSize is drawn first (0x4392FC, + 0.75 at 0x439309), then yAngle (0x439322); the ctor keeps arg 4 as +0x64 maxSize (0x749E4A).

## Growth (`Tree::Process` 0x74A290, `Tree::Grow` 0x74A3F0)

- Only the trees **of a forest** grow: in the original only `Forest::Process` 0x539DA0 walks its trees, so
  the loose trees of the script (all those of Land 1 and Land 2, which have forest −1) never grow. Those of Land 3
  (65), Land 4 (82) and Land 5 (164) do.
- A tree is born "growing" (bit 0 of +0x5E) only if its `maxSize` differs from the size it is created with, and its
  counter (+0x60) starts at a random turn in [0, growTurns) (ctor 0x749E00).
- Every `growTurns` turns (10 in the 22 types, i.e. 1 s): `amt = growAmt · (1 + 0.01·rainMultiplier·rain) ·
  (1 + 0.5·terrain alignment)`, and `scale = min(scale + amt, maxSize)`. `growAmt` 0.01 (0.02 conifer/pine, 0.005
  oak/olive/palm). On reaching the maximum it stops growing. `SetScale` is virtual and rebuilds the collision: the obstacle
  circle follows the size.
- openblack: `src/ECS/Trees.cpp` (`ProcessTreesTurn`, `GrowTree`), called from the world turn (`src/Magic/MagicLoop.cpp`). The
  rain and the terrain alignment are read: `TreeGrowthAmount` computes `amt` in the original's order (and `ProcessField`
  in `src/ECS/Fields.cpp` does the same for the fields). Hooks
  `OPENBLACK_TEST_TREE_GROWTH="x,z"` (two saplings, one with a forest and another without), `OPENBLACK_TREE_TRACE=1` (each growth
  step and the brightness) and `OPENBLACK_TEST_REPLANT="x,z,tiltDegrees"` (drops a tree there and says whether it is
  replanted, falls or stays dead).
- **Forests** (`Forest`, ctor 0x539BD0; `ECS/Trees.cpp`): a forest is an object with a centre and an id (CREATE_FOREST, or
  `new Forest(pos, 0)` when replanting outside any forest; id 0 = the next free one). CREATE_TREE and CREATE_NEW_TREE
  look up the script's id in the forest list and, if it does not exist, the tree **has no forest** (0x7162BE): ids 0-6
  of Land5 and the −1 of Land1/Land2 are left without a forest. Every turn (`Forest::Process` 0x539DA0): an empty forest waits
  2000 turns and is deleted; otherwise, its trees grow and it can plant a new one: `r = 2000 + random(1000)`,
  `f = min(1, 0.05·grown)`, `T` = turns since the last tree that any forest planted (global 0xCD04C8),
  `c` = attempts of the forest (+1 per turn); if `c·f·T/300 > r`, it plants next to one of the `random(n/2+1)` grown trees nearest
  to its centre (`Forest::CreateNewTree` 0x539FD0) and `c` goes back to 0. **Planting next to a tree**
  (fn_0053A010): 32 angles from a random one (2π/32 between them) × 5 radii (random integer 5-9, then `(r+2) % 10`),
  the first free spot; the new tree is of the parent's type, size 0.1, maximum `0.8 + random(0.4)` and random angle.
  "Free" (fn_0074C180) = without a fixed object (0.5 circle) and on land; the original reads `(collide & 8) == 0 || IsWater`,
  the water part seems inverted and has been taken as "not in water" [assumed]. With a forest of 20 grown trees
  roughly one new tree appears in the world every minute and a half (checked in Land3: forest 19 planted one).
- The rain (`GClimate::GetMaxRainingOrSnowing`) and `MapCoords::GetAlignment` are both read at the tree's `MapCoords::GetLHPoint` 0x605C40, which is the land altitude plus the MapCoords' own y (0x74A2E5..0x74A31A).
- Planting next to a tree (fn_0053A010): GameFloatRand(2π) is drawn once (0x53A043); GameRand(5) + 5 at the start of each ring (0x53A065..0x53A073). The point is origin + GetPosFromAngle(a, float(r)) via `MapCoords::operator+` 0x605520 (0x53A086..0x53A0A6), tested by fn_0074C180 at 0x53A0B0.
- `Tree::Create`'s arguments are drawn right to left: size 0.1, then yAngle GameFloatRand(2π) (0x53A14E), then maxSize GameFloatRand(0.4) + 0.8 (0x53A168..0x53A16D).
- fn_0074C180 (its only caller is fn_0053A010, at 0x53A0B0): free when a 0.5 m circle at the point touches no collide data of the cell's fixed list (`MapCoords::CollideCollideWithFixe` 0x604FE0 at 0x74C187, `test al, 8` 0x74C18C). Otherwise it returns `MapCoords::IsWater` (0x74C192, returned at 0x74C199).
- Read literally, fn_0074C180 makes a water cell always free, and neither it nor fn_0053A010 (0x53A010..0x53A18F) tests land before `Tree::Create` 0x749EE0. Off the map CollideWithFixed is 0xFFFFFFFF (bit 8 set) and IsWater is 1.
- `Forest::Process`: r = GameFloatRand(1000) + 2000 is at 0x539F04..0x539F0C. `CreateNewTree` draws GameRand(n / 2 + 1) at 0x539FE4, with n = the grown list's count (+0x4C), so the pick is always < n.
- **Water on a tree** (`Tree::ApplyWaterSpell` 0x74C390, `ecs::ApplyWaterSpell`): one
  that is growing grows `waterMultiplier·growAmt`; with spell subtype 0x17 an adult one too, half via
  `GetDistanceModifier(size, 3)` (= `SigmoidThreshold(0.5, 1 − min(size,3)/3)`, 41-step table at 0xC23284),
  above its maximum. An adult one of a watered forest without 0x17, 40 turns after the last tree in the world, plants
  another next to it (the caller gives the good alignment and statistic 0xE). It sounds 0x78 + tick%9 (`G_TreeGrow`).

## Drawing

- **Swaying** (the same wind as the ripe fields, table `T0` and `Tree::PreDraw` 0x74A7C0 in
  [objects-and-resources.md](objects-and-resources.md)): the trees
  (`Tree::Draw` 0x74B016) use the same table with factor 1: x of column 1 = 0, z = scale × T0[i], `i` = bits 2-5 of
  +0x5C; ported in RenderingSystem except with the tree tilted by the hand. The bending next to what passes nearby (bits
  6-9 of +0x5C, table 0xD19A48) is already ported for the hand and the physics objects (see "Bending", below); only the
  creature (slot 2) is missing, which does not exist yet.
- **Wind slot**: `round(yAngle·16/2π) & 15` (0x74A0E7), stored when the tree is created, so trees with the same
  orientation sway together (`components::Tree::windSlot`; before, openblack used a hash of the entity).
- **Brightness by camera** (`Tree::PreDraw` 0x74A883 → global 0xC22FA0, read only by tree code):
  `d = normalize(camera focus − light position)`, `v = normalize_xz(view direction)`,
  `b = dot < 0 ? 200 : 200 + 55·dot`, and `Tree::Draw` 0x74B077 multiplies each RGB channel of the tree's colour by `b/256`
  (0.781 … 0.996). The light is the only one LH3D keeps, [0xEA9E90], the same one for all models
  ([Model light](rendering-objects.md#model-lighting), `src/Graphics/ModelLight.h`): by day the default sun
  [0xEA1C88] = (−500000, 500000, −500000), which is indeed initialised (`fn_00818920` 0x818930; the "(0,0,0) because its
  `setter` is dead code" was a mistake of an earlier note). Only in **deep night** (sky type > 1.5, the double at
  [0x8C5838]; `fn_005E5830`, which places the light after `Tree::PreDraw`, so the trees use that of the previous
  frame) does the light go 3 units from the **hand** towards the camera, with the hand raised at least 10 above the terrain:
  then `dot ≈ cos(camera pitch)` and the trees look lighter.
- **The tree's own colour**: `fn_00802120` in `Tree::Draw` 0x74AB1B takes the 4 cells with weights `CellX >> 8` and
  `CellZ >> 8` (0x802206, 0x802237; SSE 0x7A42AC / 0x7A42BC), not the fraction: in practice, the cell alone (same
  tables 0xEDD90C and cells +3/+0xB/+0x8B/+0x93). Then the haze (0x74AB60). openblack:
  `land_light::ObjectMode::CellShift` (`vs_object`, `LandLightCellShift` of `land_light.sh`); see
  [rendering.md](rendering.md#haze-and-land-light-the-common-api).
  openblack: `ecs::TreeBrightness()` in `ECS/Trees.cpp`, carried as a tint in the x of the instance's fifth column
  with w = 1 (`argb_colour::PackInstanceTreeTint`, [rendering-objects.md](rendering-objects.md#the-object-colour-fields-in-the-instance)): `vs_object` applies it after the haze, like
  `Tree::Draw`, which does not call `fn_0080BF10` but multiplies the +0x4C already hazed (0x74AB60 → 0x74B077..0x74B0C4;
  burning, `fn_0074B3A0` 0x74B48F..0x74B4D3).
- **Ambient leaf sound** (0x74B111): trees taller than 10 with the camera at ≤ 10 in x and z (and < 18 in y)
  sound ~once per second (`LocalRand(1000/frameMs) == 1`): row `{*,*,20,*,70}` of `editor.sad` =
  `G_TreeRustle_01..11` + `G_TreeCreak_01/02`. openblack: `ecs::UpdateTrees` + `AnimationSounds::PlayFromTable`.
- **Bending next to what passes nearby** (`Tree::Draw` 0x74AB8B, `fn_005DF1B0`, table 0xD19A48): every frame the table
  records the object the hand carries (slot 1: every object in the hand is "drawn in the hand"), the physics objects
  in flight (slots 3-13 per turn, `fn_00646FE0`) and the player's creature (slot 2; there is none yet), each with its
  position and a radius = scale × the half-diagonal of its mesh (LH3DMesh +0x30). Each source marks the trees of the 3 × 3
  10 m cells around it (the last one wins). A marked tree bends if its crown is not below the source
  (base + height ≥ y of the source) and the horizontal distance `d` is less than the radius `r`:
  `angle = 0.471239 · (1 − ((r − 0.75)·d/r + 0.75)/r)` (27° at most), around the horizontal axis perpendicular to the
  source→tree direction, the crown **moving away** from the source; only the drawn matrix, and in that frame without swaying.
  Sound on starting to bend: key `{c, *, *, 10, 75}` of `editor.sad` with `c = 3` if the bend is < 0.3 (no
  samples), 2 if < 0.67, 1 otherwise: `G_Crash_Tree_M_01..08` ("rubbing trees"). openblack: `ecs::UpdateTrees`
  (`UpdateTreeBends`, `Tree::bendAngle/bendDirection`) and `RenderingSystem.cpp`. Checked with a rock in the hand
  (`OPENBLACK_HAND_TEST_HOLD=1.5`): the nearby trees move aside and play `G_Crash_Tree_M_05/07`.
- The bend table is fed each frame from `GLandscape::Draw` 0x5E4960 / 0x5E49DC and fn_00646FE0 (fn_005DF1B0). The held object is passed with the G3D's drawn position in the hand (0x5E49C1..0x5E49CA). The physics objects are passed with their interpolated position (0x647182..0x6471A4).
- Ambient leaf sound: the random draw comes first, for every tree drawn, before the distance tests (0x74B111..0x74B140): n = ftol(1000 (float [0x8AB228]) / g_game +0x205D48). With 0 ms (pause) 1000 / 0 is +inf, which __ftol 0x7A1400 turns into 0x80000000, so LocalRand divides by 2^31. The comparison with 1 is at 0x74B135..0x74B13D.

## Fire

The heat model common to all objects (`FireEffect`, `SpreadEffect.cpp`, `src/ECS/Fire`) is in
[magic.md](magic.md); the fireball and the lightning bolt, in
[miracles.md](miracles.md). Here, what
concerns the tree (values of the GTreeInfo table, above):

- **Fire** (`SpreadEffect.cpp` / FireEffect at Object+0x44; turn 0.1 s; **faithful**, ported in `src/ECS/Fire`):
  temperature T, Tc = max(110, 40); it burns if T ≥ Tc. Burning T += 0.1·T/(2Tc) up to 2Tc; cooling (T ≤ previous)
  T −= (T + 10 − amb)·4·H·R·0.1·k/capacity (k = 50 over water with y < 2, 1 + 0.01·rain). Damage: life −=
  (T − Tc)/Tc·0.001 per turn (dies in ~100 s; magic.md writes it as `(T − Tc)/(2·Tc − Tc) · defenceMultiplierBurn ·
  0.1`, with defenceMultiplierBurn = 0.01 in the 22 tree types and in Tree Logs (info.dat) and Tmax = 2·Tc: it is the same formula); charred with life < 0.6. At life 0 the tree disappears. Spreading: every turn it searches within R + 10 m, R =
  1.25·radius2D·clamp((T − 0.8Tc)/1.2Tc); heat q = min(10·dT, 0.5·(Ts − amb)·cap_s) → the target gains q/cap_t (the
  bushes catch fire ~10× sooner). A burning tree can be picked up and keeps burning; held over something that is burning, or
  thrown, it sets fire to what it touches; when it falls it becomes a burning DeadTree. No lightning nor random fire. Visual: colour ×
  max(50, 255 − (1 − life)·2550)/256 (almost black on losing 8 %; capped by the brightness [0xC22FA0]), ALPHAREF forced
  230 + heat·25/255 (cap 254, `OverrideRenderMode`, not ported: [magic.md](magic.md)), scale × 5·life
  below 0.2; flames `FireGraphic` (sprites `S_Fire.raw`, smoke `S_SpriteSheet3.raw`, light `S_LMFireBall.raw`),
  2 flames per tree of 0.2·height; looping fire sound.

## Sacrifice

- **Sacrifice** (postponed by the user): dropping a tree aimed at the **WorshipTotem** (CitadelPart of the worship
  site; `ValidToApplyThisToObject` 0x74BD50): v = sacrificeValue·life·(0.5 + 0.5·life) to the site's mana
  (+0xF0) and to the total (+0xF4), ghost of the tree (`GoolooGooloo`), `G_SACRIFICE_01`, red floating number "%3.0f".
  Villagers ×1.25; food, rocks and pots no. **Pending**: the sacrifice is not ported (`GET_SACRIFICE_TOTAL` gives 0
  in `CHLApi.cpp`). The worship sites it needs already exist (`src/Worship`; `CREATE_WORSHIP_SITE` calls
  `magic::script::CreateWorshipSite`, see
  [magic.md](magic.md)).

## Test hooks

- `OPENBLACK_TEST_TUG="x,z,wait,hold"` (+ `OPENBLACK_TEST_TUG_MOUSE2="x,y"`) and `OPENBLACK_HAND_TRACE=1`: the tug.
- `OPENBLACK_TEST_REPLANT="x,z,tiltDegrees"`: drops a tree and says whether it is replanted, falls or stays dead.
- `OPENBLACK_TEST_FELL="x,z"`: felling.
- `OPENBLACK_TEST_TREE_QUERIES="x,z"`: searches for trees and forests, `MakeScenicForest`, `AssignForestsToTown`.
- `OPENBLACK_HAND_TEST_FOREST=1`: taking from a BigForest.
- `OPENBLACK_TEST_TREE_GROWTH="x,z"` and `OPENBLACK_TREE_TRACE=1`: growth and brightness.
- `OPENBLACK_HAND_TEST_HOLD=1.5`: bending of the trees next to what the hand carries.
- Fire: the hooks of [magic.md](magic.md#test-hooks) (`OPENBLACK_TEST_FIRE`).

## Sources

- bw1-decomp `src/Black/Object.cpp:539`: the normal test on dropping (only living beings and fences).
- info.dat: the values of the trees (wood, weight, `defenceMultiplierBurn` 0.01…).

## Pending

- Tug: check with the original the height of the plane (the Tug's "Consequence") by reading runblack.exe's memory;
  openblack uses the horizontal distance of the hand.
- Replanting: `StartImmersion(0x2E)`; the alignment update's place (openblack: before the smoke, the spot visual and the creature's mimicry; the original: after the mimicry, 0x74BBBD).
- `MakeScenicForest` and `AssignForestsToTown` without a caller (map load and finished buildings): the villages have no
  forest list.
- Growth and fields: the rain and the terrain alignment (`MapCoords::GetAlignment`). The weather already exists
  (`src/ECS/Weather`); only `GrowTree` reading it is missing. For the user it is the first thing that can be done now.
- Bending: the player's creature (slot 2).
- Sacrifice (postponed by the user).
- Putting a tree into a building site under construction (`Scaffold`): the building sites are missing.
- Dead and felled tree: the reaction 0x0C "there is wood here" (DeadTree constructor 0x510957, `FelledTree::Create`
  0x511889) awaits the reaction system; the woodcutters' villager jobs will use the API above. The trunk that
  shrinks as wood is taken from it is already in `RemoveWood`; it only needs the villager jobs to call it.
- Creature: avoiding the trees when walking (no creature yet).
- Sounds: the rustle, the rubbing and `G_TreeGrow` of `Trees.cpp` move to the audio engine without changing
  the choice of sample; check them with `OPENBLACK_HAND_TEST_HOLD=1.5` and `OPENBLACK_TEST_TREE_GROWTH`.
