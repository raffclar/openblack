# Water in the game

Everything about water that is not the drawing of the sea: which cell is water, the water queries and the `LandAvoid` mask, the
water in the scripts, impacts and falls into the water, sinking and drowning, the rings, the sharks, the fish
puzzle, the missionaries' boat, the fixed scenery (waterfall, ark) and what sounds in the water. The sea, the coast and the
rivers are drawn in [rendering.md](rendering.md); the reflections and the underwater cuts, in
[rendering-objects.md](rendering-objects.md).

> **Code rules.** Water state lives in ECS components (sharks, fish shoals, puzzles, scenery entities) and the systems
> are reached through Locator services, never globals; meshes and sounds load through the resource caches; the cell
> predicates and water queries stay pure functions, unit tested with fakes in `test/` (a test that needs the real
> `Land1.lnd` skips itself without it); comments describe behaviour in plain English, with no decompiled names or
> addresses (those belong here). See [openblack-internals.md](openblack-internals.md).

- [Water cells (SeaCells)](#water-cells-seacells)
- [Water queries](#water-queries-gutils-gstream-abode)
- [The creature's `LandAvoid` mask](#the-creatures-landavoid-mask)
- [Water in the scripts (CHL)](#water-in-the-scripts-chl)
- [Impacts and objects that fall into the water](#impacts-and-objects-that-fall-into-the-water)
- [Sinking, drowning and being deleted](#sinking-drowning-and-being-deleted)
- [Water rings](#water-rings)
- [Sharks (class `Whale`)](#sharks-class-whale)
- [Fish puzzle](#fish-puzzle)
- [The missionaries' boat (PetitNavire)](#the-missionaries-boat-petitnavire)
- [Fixed scenery: Land 3 waterfall, Land 4 ark and dinosaur](#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)
- [Water audio](#water-audio)
- [Water in other pages](#water-in-other-pages)
- [Test hooks](#test-hooks), [Sources](#sources), [Pending](#pending)

## Water cells (SeaCells)

All of the original's land/water is decided with five predicates on the **terrain cell** (8 bytes; byte +4 =
raw altitude, byte +6 = LND properties: 0x10 `hasWater`, 0x20 `coastLine`). Previously each module had its own rule
(flags, height ≤ 0, clipped edge); now there is a single module with the exact table, **including the map edge**:

| openblack function | Original | Rule | No cell (outside the map or no block) |
|---|---|---|---|
| `sea_cells::IsWater` | `MapCoords::IsWater` 0x6035B0 | `properties & 0x10` | **water** (1) |
| `sea_cells::IsLand` | `MapCoords::IsLand` 0x603720 | `!(properties & 0x10)` | 0 |
| `sea_cells::IsDryLand` | `MapCoords::IsDryLand` 0x603620 | **altitude ≥ 4**, does not look at the flags | 0 |
| `sea_cells::IsCoastal` | `MapCoords::IsCoastal` 0x6036A0 | `!(properties & 0x10) && (properties & 0x20)` | 0 |
| `sea_cells::InBounds` | `MapCoords::InBounds` 0x6042C0 | the cell falls inside the game map (unsigned comparison) | — |
| `sea_cells::CollideLandscape` | terrain part of `MapCell::Collide` 0x601BD0 | 0x10 outside the map (fn_00601E00), otherwise 1 water / 2 land | 0x10 / 1 |
| `sea_cells::GetSurfaceType` | `GSoundMap::GetSurfaceType` 0x71D8E0 | 6 with no cell, 7 if `!IsLand`, otherwise the material's `surfaceSound` (3 if it is not 1..8) | 6 |

- The cell of a point is `MapCoords` = `ftol(world·6553.6) >> 16` (10 units per cell, truncated; a negative x
  goes off the map through the unsigned high word, it is not clamped to 0). The impact against the water and the drop rule also use
  the cell **rounded to the nearest** (`fistp` in the original, `std::lrint`), half a cell of difference.
- Bits 0x04 (field), 0x08 (fixed) and 0x20 (tree) of `MapCell::Collide` come from the objects of the map cell,
  which openblack does not list yet: `CollideLandscape` only gives the terrain part.
- Consumers today: `CollisionSounds` (impact against the water), `AnimationSounds` (surface of the clips),
  `HandSystem::IsLand` (and with it the fish, the trees and the hand pots), `HandHolding` (gentle drop),
  `ecs::pot_resource::IsWater` (resources lost in the sea), `FishShoals::IsOkToCreateFishFarmAt`, `WaterQueries` and `LandAvoid`.
- **Fish farms**: `GFishFarmInfo::IsOkToCreateAtPos` 0x52D100 = `IsCoastal` **and** that there is not already a farm in that
  cell (`MapCoords::FindType(0x21)` 0x6045C0). No town, no depth, no distance. Today only the script creates
  farms, so the rule is ported (`IsOkToCreateFishFarmAt`) but unused.
- Unit test: `test/test_sea_cells.cpp` (hand-made two-block island + real `Land1.lnd` if installed:
  open sea 1464.2016; shore of altitude 1 at 1485.2015; dry land of altitude 44 at 1788.4/2710).
- The predicates span MapCoords 0x6035B0..0x603840. Each reads the landscape cell `g_ptr_blocks[g_index_block[x >> 4][z >> 4]] + ((x & 15) × 17 + (z & 15)) × 8`, with x, z the high words of the 16.16 MapCoords (world / 10). A cell off 0..511 or without a block is "no cell".
- `IsDryLand` is `cmp al, 4; sbb; inc` (0x603682). `GetSurfaceType` returns 7 for !IsLand at 0x71D966, and 3 for a material sound ≤ 0 or ≥ 9 at 0x71D97A.
- The fish farm object:
  - `FishFarm::ToBeDeleted` 0x52C690 → `DeleteDependancys` 0x52C6B0 (0x52C693). While the fishermen list is not empty, it calls the head villager's SetTopState(163) (vt +0x8E8). `ExitFishing` 0x75B880 then unlinks it, the farm still being available (GameThing +0xA bit 1 is set later, 0x56FB7B) (0x52C6B3..0x52C6D7).
  - `DeleteDependancys` then leaves the town's list +0x788 (0x52C6DA..0x52C741) and g_game +0x205C0C (0x52C75E..0x52C7A7), and calls RemoveMapObject (0x52C744..0x52C758).
  - `AddFisherman` fn_0052D250 does nothing for a null villager (0x52D256..0x52D25A). It puts a new 8-byte node {next, villager} at the head, +0x84++ (0x52D25C..0x52D27B), with no duplicate test (`EnterFishing` tests first, 0x75B85E) and no maximum.
  - `RemoveFisherman` unlinks every node of the villager (+0x84−− each, 0x52D2A6..0x52D2DD) and does not write the villager's +0x118.
  - The farm's score fn_0052D2F0 = ftol(fn_0052D240 (fld 1.0 [0x8AA390]) × (1 − min((float)((u64) +0x84 / (int) maxNoFishermanPerFishFarm (+0x120)), 1))) (0x52D2F3..0x52D34A): 1 with no fisherman, 0 with 1..4.
  - `FishingSpot` fn_0052C870: r = Get2DRadius (5), h = r × 0.5 (0x52C88A..0x52C8A6). It adds GameFloatRand(r) − h to x ("FishFarm.cpp" 0xB8, 0x52C8AA), then to z (0xB9, 0x52C8C2), each axis ftol((pos × 10 × 2^-16 + d) × 65536 / 10) (0x52C8CB..0x52C926); the altitude is copied.
  - `FishFarm::RemoveResource` 0x52CF20 (vt +0xA0): FOOD (type 0) → RemoveFood(n); any other type 0 (0x52CF20..0x52CF37).
  - `RemoveFood` 0x52CED0 with n above the food (0x52CED0..0x52CEE2) returns ftol(food) and sets food = 0 (0x52CEFC..0x52CF09). Otherwise food −= n and it returns n (0x52CEE4..0x52CEF0).
  - `FishFarm::GetTown` 0x52C450 = +0x8C. A town's fish farms (Town +0x788) run newest first (head insertion in the ctor, 0x52C407..0x52C418).
  - Fishing from the hand (`NetworkFriendlyStartLockedSelect` 0x52D770): the HandFood pot holds min(amountPickedUpInitially, GetFoodValue(3)) and goes into the hand at 0x52D840.

## Water queries (`GUtils`, `GStream`, `Abode`)

The original's positions are `MapCoords`: x and z in cell 16.16 (world × 6553.6 truncated, 10 world units
per cell) and y = height **above the ground**, not world altitude. The port keeps that detail inside the module and
exposes `glm::vec3` in world units.

The table root, `hypotenuse` and `GetDistanceInMetres` are not copied in
`WaterQueries.cpp`: they come from `src/Common/GUtilsDistance.{h,cpp}` (`openblack::gutils`), in float and not in double, see
[engine-math.md](engine-math.md#gutils-distances).

| Function | Address | What it does |
|---|---|---|
| `GUtils::GetDistanceInMetres` | 0x74CD70 | xz distance of two `MapCoords`: `hypotenuse(int,int)` 0x74F680 (16.16) × 10/65536 |
| `hypotenuse` / inverse root | 0x74F680 / 0x74F620 | inverse root by a table of 1024 mantissas (`crt_xc_fn_atexitCleanupReg_Utils_0074F580`): ~10 bits, error ≈ 0.1 % |
| `GUtils::FindNearestCoastalTo` | 0x74E2E0 | square spiral of cells from the point until finding a cell **inside the game map** and `MapCoords::IsCoastal`, or until the spiral moves further away than the radius (max. 999999 steps) |
| `GUtils::Spiral` | 0x74D7E0 | the spiral step: table 0xDA59FC = (+1.0), (0,+1), (−1.0), (0,−1), with `dir/2` steps per stretch; it adds **to the high word**, so the cell fraction of the starting point is not lost (`operator+=` 0x605470) |
| `GStream::FindNearestPosTo` | 0x733D30 | the river point (`CREATE_STREAM_POINT`) closest in xz, **strictly** closer than the radius; the y of the result is the point's altitude minus the ground there |
| `GUtils::FindNearestDrinkingWater` | 0x74E3A0 | first the river; if there is one, it also looks for a coast that is no further **from the starting point** than that river (if it finds one, the coast wins) and returns "found" in any case; without a river, the coast within the radius |
| `Abode::FindNearestDrinkingWater` | 0x407020 | the above from the house into its cache: bit 0 of +0x7C = found, +0x80 = the position (**it only changes if it finds something**) |
| `Abode::GetNearestWaterPos` | 0x405FC0 | reads that cache (false if the bit is 0) |

The original's radii: **200** when creating the house (`Abode::Abode` 0x4013E0), **400** the shepherd when the house does not yet have
water (`Villager::ShepherdMoveFlockToWater` 0x768CC0), **500** the tiger (`Tiger::CalculeLairPos` 0x4214DD).

Details that matter:

- The spiral **stops as soon as it moves beyond the radius**, it does not cover the whole square: that is why it does not always return the
  strictly closest coast, but the first one in the spiral.
- `IsCoastal` (0x6036A0) is **land** on the coastline (`!hasWater && coastLine`), not water: the drinking water is a
  land cell next to the sea, and the drinker approaches it. The predicates are in `ECS/SeaCells`.
- `Tiger::CalculeLairPos` (0x421470) calls `FindNearestDrinkingWater(pos, 500)` **for each forest** but does not use the
  result to place the lair: the answer only sets the distance it compares with the forest's score
  (`fn_0053AD00`, `SigmoidThreshold` of trees and distance), and that distance is not read afterwards. The lair is still
  the best-scored forest, so `ECS/AnimalPredators.cpp` does not change (it only notes it).
- `GStream::FindNearestPosTo` walks the streams of [g_game +0x205C5C] newest first (the GStream ctor pushes at the head, 0x733A8E..0x733AA0), and each stream's points (+0x14, next +0xC) in script order. For two points at exactly the same distance the first one found wins.
- The coast spiral's 999999-step counter is set at 0x74E2FF.

## The creature's `LandAvoid` mask

`LandAvoid` (0xD559B0) is 512×512 bytes `[z][x]`, one per terrain cell, built **once per landscape**
in `GLandscape::Open` (0x5E5541) with `ValidateLandAvoid` (0x6E7BA0) and `FloodAnalyse` (0x6E7FA0, `RoutePlan.cpp`):

1. For each cell, the altitudes of its **four corners** (`(x,z)`, `(x,z+1)`, `(x+1,z)`, `(x+1,z+1)`; 0 outside the
   map or with no block) × 0.67: if `max − min > 10` → **1** (too steep); if all four are 0 → **1** (deep
   sea); otherwise → **4** (walkable candidate).
2. The flood seed is the last 4 cell of the last run of 4 cells that ends in a 1 (by rows: the
   counter is reset on each row; a run that reaches the end of the row does not count). In Land1 it comes out as **(191, 362)**.
3. `FloodAnalyse` floods the 4 neighbours ((−1.0), (0,−1), (+1.0), (0,+1)): the reached 4s become **0** and the 1s
   neighbouring the flood become 5. At the end 1 → **2**, 4 → **2** (unreachable) and 5 → **1**.
4. The 0 cells whose terrain has the water bit (or has no block) become **6**: water that can be walked through.

The flood marks: 3 queued (FloodAnalyse 0x6E802C / 0x6E80CB), 5 for a 1 next to the flood (0x6E80D1), reset at
0x6E810D; the seed rule of step 2 is at 0x6E7EC4; a reached cell that is water, off the map or without a block is
walkable water (0x6E7F18).

Final values: **0** reachable land, **6** walkable water, **1** cell to avoid next to the reachable area, **2**
unreachable. `fn_00483890(pos, r)` (r = 7.1 in `fn_00483850`; 7.05 in `fn_00483870`, turning) accepts the position if its
cell is 0 or 6 and no cell centre `(10i+5, 10j+5)` of the neighbouring 3×3 that is not 0/6 lies closer than r.

The creature therefore **wades** as far as where all four corners of the cell have altitude 0 and does not swim (in
`ctrspec27.txt` there is no swimming animation). The Land1 lakes (altitude 1 with water) also come out as **6**: they can be
entered.

In openblack: `land_avoid::Validate(island)` is called when loading the landscape (`Game::LoadLandscape`), `At(x, z)` gives the
value and `IsPosValid(pos, radius)` is `fn_00483890`. Count in Land1: 12 949 cells 0, **1866** cells 6 (the ring of
shallow water and the lakes), 3671 cells 1 and 243 658 cells 2.

**Hook** `OPENBLACK_DUMP_LAND_AVOID=1` (or `=<file>.png`) dumps the mask when loading the landscape: one pixel per
cell, x to the right and z downwards, **green** 0, **blue** 6, **red** 1, **grey** 2 (magenta = impossible value), and
writes the seed and the counts to the log. Checked with the Land1 points: `(146,201)` open sea → red (deep
sea next to the reachable area), `(147-148,201)` shallow coast → blue, `(149,201)` and `(178,271)` land → green,
the lakes `(213,241)` and `(216,309)` → blue.

## Water in the scripts (CHL)

- **`GET_LAND_HEIGHT`** (`GScript::GetLandHeight` 0x6FB1F0): cell `(int)(x·0.1)`, `(int)(z·0.1)` (truncated towards 0, so
  −10 < x < 0 is still cell 0); outside 0..511, with no block or **altitude 0** it returns **−10.0**
  (0xC1200000, "it is in the sea"); otherwise, the interpolated height (`LH3DIsland::GetAltitude`). openblack always returned
  the height: now `sea_cells::ScriptLandHeight` applies the full rule (`CHLApi.cpp`, `GET_LAND_HEIGHT` 151). It is used by
  the challenges `Baywatch` (Land2) and `LostBrother` (Land1). Checked: Land1 open sea (1464, 2016) → −10, dry land
  (1788.4; 2710) → 28.917.
- **`GET_PROPERTY`** (`GScript::GetProperty` 0x70DAE0, jump table 0x70E78C on `property − 1`): for now the
  two water ones are there, the others are still unimplemented.
  - `FLYING` (5, 0x70DCF8) = bit 6 (0x40) of `Object+0x24` = **the object has a `PhysicsObject`** (also the
    obstacles at rest, just like `Object::IsActuallyInTheAir` 0x639410).
  - `DROWNING` (6, 0x70DD0A) = the virtual `IsDrowning` (vt +0x17C): `Villager` 0x756B30 = state **16** DROWNING;
    `Object` 0x63A780 = has a `PhysicsObject` **and** the centre of mass (po +0xCC) is below y = 0;
    `GameThingWithPos` 0x4052D0 = 0. In openblack it is `ecs::IsDrowning` (`ECS/VillagerDrowning`). It is used by the challenges
    `CreatureSavingPeopleDrowningMan` and `PiperSetFree` (Land1) and `ThrowBlokeMain` / `EndOfFlyingNutter` (Land3).
  - With a thing that no longer exists the original writes "Thing no longer valid" and pushes a **float 0**; openblack does the
    same (it often appears in the log because the scripts reference objects that openblack does not create yet).
- **`SET_PROPERTY`** (0x70F380 → fn_0070E820, table 0x70F2BC): `FLYING`, `DROWNING` and `MOVING` go to the error case
  ("Cannot Set Property %d") and **change nothing**; ported that way.
- **Enum fixed**: `ObjectPropertyType` in `src/ScriptHeaders/ScriptEnums.h` merged two entries into
  `InHandGrabypeSpeed`, so from `IN_HAND_GRAB` (10) onwards everything was shifted by one value (`SPEED` 11 … `BUILT_PERCENTAGE`
  22, `ZPOS` 25). Now they are `InHandGrab` and `Speed` as in `bw1-decomp include/chlasm/ScriptEnums.h`. Nobody used the
  numbers before (checked with grep), so the fix breaks nothing.

## Impacts and objects that fall into the water

- **Impact against the water** (`AttemptToAddSoundEvent` 0x6465B7): `!IsDryLand` (altitude < 4) → **ring** at (x; 0.1; z)
  with growth 2R, rate 1/R, cell **0x3F**, 0xFFFFFFFF; also, if in the rounded cell there is no cell or the
  altitude is < 3 → collision type WATER, foam dust 0x28C8F0F4 and `fn_0074F2D0` (which **only** sets the
  global splash flag that scares the fish: there is no spell file effect, the 6 foam "particles" are the
  normal dust of `fn_00845C20`). Altitude 3 = shore: ring + brown dust with the ground sound.
  - The 6 particles (0x646776..0x646854; openblack `ECS/Physics/Dust`): at (x, `GetAltitude`, z), velocity
    (rand(201) − 100)·0.02 per axis, size min(2R, 5), type 4 of the "liquid particles" (`fn_00845D30`, quota 0x400;
    `fn_00846010`: type 4 = 1 s without gravity, type 0 = 3 s, others 2 s; they are removed when the age **exceeds** the life,
    before moving). The colour first goes through `fn_004ED180`: k = clamp(ftol(snow at the point), 0, 255) with the
    `SnowCover` grid [0xEDC344] (128×128, 40 units per cell, bilinear, `fn_0086CA80`) and each channel
    c += floor((base − c)·k/256) towards the base colour of the light [0xFA26A4], alpha the same. Without snow k = 0 and the colour does not
    change; openblack has no `SnowCover` grid here, so it is not applied.
- **Wave when bobbing in the water** (0x645A5E): same ring but **cell 0x30** (the one of the hand splash), not 0x3F.
- **Dropping over the water** (`Object::InitialisePhysicsFromHand` 0x636F00, fully ported; the algorithm is in
  [physics.md](physics.md#water-in-impacts-and-when-dropping)): it only "lands" with `IsDryLand` or with the altitude of the
  rounded cell (fistp) > 1, so over the sea the object stays in physics and floats or sinks
  ([below](#sinking-drowning-and-being-deleted)); a hand pot dropped slowly loses its resource (next point).
  Buoyancy (density that rises 6.67e-5 per substep, drag ×100, deletion below −4R) is in
  [physics.md](physics.md#engine-physob-0x7fb7300x7fe7b0). **faithful**
- **Resources that fall into the sea** (`Pot::AddResourceToPos` 0x66F270): outside the map it does nothing, and what is left after
  merging with piles/storehouses **is lost** if the cell is water (0x66F42D): neither a new pile nor a sound.
  openblack: a single rule, `ecs::pot_resource::AddResourceToPos` (`ECS/PotResource.cpp`, exact to 0x66F270, see
  [magic.md](magic.md)); the hand (`HandResources.cpp`) no longer has its own rule and `pot_resource::IsWater` calls
  `ecs::sea_cells::IsWater` (the only copy of `MapCoords::IsWater` 0x6035B0).

## Sinking, drowning and being deleted

Code: `src/ECS/VillagerDrowning.{h,cpp}` and `src/ECS/ToBeDeleted.{h,cpp}`. **Faithful** except what is marked
pending (the villager's complete death, see [Pending](#pending)).

- **`HasSunk`** (vt +0x7B8), asked on every substep from 0x645A01 when the body is awake, its centre is
  below `R/2` and its **density > 1**; if it says yes, the body stops (v = 0, L = 0) and `EndPhysics` runs
  as if it had come to rest (code 2):
  - `Object::HasSunk` 0x637470 → **no**: rocks, trees, pots, piles and pieces keep going down until `T.y < −4R`
    (code 4) and there they are **deleted** with their class's `ToBeDeleted(0)`. Measured times: rock immediately, villager ~4
    turns, offering pot ~64, animal ~75, normal object ~150, tree ~194, pot ~298, ball ~525.
    While going down they are drawn whole with their normal Draw, after the land: the part below y = 0 is hidden by the
    Z of the drawn cells (which write it even though they are transparent) and is visible over the open-sea 0x02 cells
    (which are not drawn). A tree that sinks next to those cells comes out **cut into rectangles**, also in the
    original ([rendering.md](rendering.md#coast)); the user remembers it that way (2026-10-01): a tree or a rock
    thrown into the sea looked cut off.
  - `Living::HasSunk` 0x5ED370 (animals) → `SetDying`, state LIVING_DEAD 15 and `ToBeDeleted(0)`: the animal disappears.
  - `Villager::HasSunk` 0x750AB0 → `stateCounter = GVillagerInfo::drowningTime` (**600** turns = 60 s) and state
    **DROWNING (16)**. Not available (FINAL 14 DYING, 0x750AB5) → not sunk. With the dead status bit (+0xB4 & 1, a
    corpse): state DYING 14 with `dyingTimeWithoutGraveyard` (`VillagerDrowning.cpp`, [villagers.md](villagers.md), Death).
  - Both `Living`s first notify the creature so that it learns from the player who dropped it
    (`ConsiderMakingCreatureMimicPlayer`, `DETECTED_PLAYER_ACTION_THROW_IN_THE_SEA` 0x15), after the availability test.
    openblack publishes the villager's (`creature_mimic::ConsiderThrownInTheSea`, the player of `VillagerLastInteraction`);
    the animal's waits for a last dropper of animals ([creature.md](creature.md#mimicry-and-the-town-hooks)).
- **`Villager::EndPhysics` 0x5F0A60, water branch** (0x5F0BAF): every villager that **ends physics in a cell with the
  water bit** drowns, also on the shallow shore; there is no LANDED. With life > 0 → DROWNING with `drowningTime`
  (and `lastPlayerToInteract` = whoever threw it, pending); otherwise, `VillagerDead(6 PLAYER_INTERACTION_DROWN)`. This is reached
  through `HasSunk` (the normal case in the sea) or when stopping with contacts in a water cell of altitude ≥ 2; in Land1 **there is no
  water cell with altitude > 1** (checked by walking the map in `test_sea_cells`), so on that island
  it is always reached through `HasSunk`. openblack used `po.body.inWater` (physics) instead of `IsWater(Pos)` (the cell).
- **DROWNING state (16)**, `Villager::Drowning` 0x76A780, once per turn: `--stateCounter` and, at 0, death with
  reason 6. Default clip of the state **252 `P_DROWNING`** (2233 ms loop) and its sound events: 30 ms and 1590 ms
  swimming splash 157 (`editor.sad` 559-562), 257 ms drowning voice 134 (man 563-570, woman 571-578, the child does not
  scream) — they already play because `AnimationSounds` resolves surface 7 (water) of the clip.
  `EnterDrowning` 0x767410 / `ExitDrowning` 0x767420 return 1 (they do nothing).
  - **Runs every turn**: `GGame::ProcessTurn` → `Living::ProcessLiving` 0x5EC810 → `ProcessState` (vt +0x620,
    `Villager::ProcessState` 0x74FF70) → `CallState` 0x7521D0. `GVillagerInfo::processChecksEvery` (+0x2DC) only
    spaces out the periodic block of `CheckEveryTime` (0x750518: old age, hunger…), not the state function.
  - **Bit 0x4000 of `Flags` +0x24 = INDESTRUCTIBLE**: it is set and cleared by `SET_INDESTRUCTABLE` (`GScript::
    SetIndestructable` 0x6FDE20, objects that are not script containers), the puzzles (PuzzleGame, HanoiBlock,
    PuzzlePig) and `GameOSFile::LoadInstance`. `Drowning` 0x76A783 sets the counter to 10 before decrementing it: **an
    indestructible villager never drowns**. openblack: `Indestructible` component and `SET_INDESTRUCTABLE` in
    `CHLApi.cpp`.
  - **`lastPlayerToInteract` (+0x104)** = `PhysicsObject::GetPlayer` 0x647460 (the player of the `GInterfaceStatus` of
    po+0x24: the hand that dropped or threw it, inherited by what it hits; 0 without physics), set in the water branch of
    `EndPhysics`. It is only read by the `VillagerDead` of `Drowning` (the villager's `GetPlayerWhoLastDroppedMe` is the one from
    `GameThing`, which returns NULL, 0x4018B0). openblack does not yet have players to store it in.
  - **Rescuing with the hand**: picking it up puts it IN_HAND (the drowning function stops running) and dropping it on dry land
    takes it out of physics on the spot → `Villager::EndPhysics` without `IsWater` → LANDED. After LANDED it only goes back to the
    previous state (+0x8E) if it has the 0x400 flag (script-controlled) or if that state has +0x104 in its
    `GVillagerStateTableInfo` (file +0xF4): only `InScript` (4) and `In Script Dance` (5); DROWNING does not. It stays
    rescued.
- **Clips for dying in the water**: `DyingAnimation` 0x423770 → **283 `P_INTO_DEAD_DROWNED`** if `IsWater(Pos)`, and
  `DeadAnimation` 0x4237A0 → **249 `P_DEAD_DROWNED`** (`VillagerAnimations`); on land 253 and 243.
- Common **`ToBeDeleted`** (`ECS/ToBeDeleted`): the clean-up of each class before removing the entity — villager
  (`Villager::DeleteDependancys` 0x74FD60: house and homeless list), animal (`Animal::DeleteDependancys` 0x417BA0:
  the AI forgets it), and then out of physics and of the registry. It is used by the deletion at −4R (previously a direct
  `registry.Destroy`), the sinking of animals and the death of the drowned.
- **Scripts**: `GET_PROPERTY(DROWNING)` (and `FLYING`) now answer with `ecs::IsDrowning`; see
  [Water in the scripts](#water-in-the-scripts-chl).

## Water rings

- **Faithful** (`fn_005E5100`, after the land and before the models): per ring, age += (int)(game
  ms · rate), removed at 700; half width max(age·growth/700, 0.0001) (z × aspect, +0x28); alpha (int)((255 − 0.364286·
  (age % 700))·A) >> 8, RGB from the colour; rotation in Y, cell & 63 of the 8×8 sheet of `smoke.raw`/`smokea.raw`, mode 13
  (SRCALPHA/ONE, no Z); drifts with the wind if +0x1C. Hand splash: (x, 0.2, z), growth 7, random angle,
  cell 0x30, 0xB0 + light table[255]. Physics object in the water (0x6466D2): (x, 0.1, z), growth 2·radius, rate
  1/radius, cell 0x3F, white. openblack: `ecs/WaterRings`, `Renderer::DrawWaterRings`; hook `OPENBLACK_TEST_SPLASH="x,z"`
  (one splash per second; the rings only advance with the game running). The colour +0x34 is fixed **on creating** the
  ring (table[255] of that frame for the hand and the waterfall, table[200] for the rain): `AddWaterRing` resolves
  `seaLight` once and the drawing uses +0x34 as is, so a ring made at nightfall or during a lightning flash does not change.
  Particle rings (`Particles/PSysWaterRings`, called by the rules of
  [particles.md](particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules)): explosion
  (`UR_Explosion` 0x67E347: in water or altitude < 4 three rings of growth 5, 7 and 10, cell 0x30, white, at
  (x, altitude, z); on dry land the scorch mark 0x251) and particle ripple (`fn_006A1630`: only in water and more than 2
  (rule+0x40) in xz from the last one, growth 4·atom radius, on the ground).
- The original's quota: 1024 rings of 0x38 bytes at 0xEAB7C8. They are created by the hand splash, the objects that fall
  into the water ([above](#impacts-and-objects-that-fall-into-the-water)), the wake of the [sharks](#sharks-class-whale), the
  foot of the [waterfall](#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur), the fish of the
  [puzzle](#fish-puzzle), the rain and the particles; the swimmers (SuperVillagers) do not exist in openblack yet.
  The water that scares the fish: [rendering-objects.md](rendering-objects.md#fish-farm-fish-shoals).
- The ring count is at 0xEB9AB8 (next to the 1024 × 0x38 pool at 0xEAB7C8).
- The hand splash's random ring angle (CRT Random(0, 2π), 0x5D2057) is drawn before the ring pool is searched (0x5D205C), even when the pool is full.

## Sharks (class `Whale`)

- **Faithful** (class `Whale`, Whale.cpp; `src/ECS/Sharks.{h,cpp}`,
  `ECS/Archetypes/SharkArchetype.*`, `ECS/Components/Shark.h`). `CREATE(Whale = 26, 5000, pos)` → `Whale::Create`
  0x774C50 (`GMobileObjectInfo[24]`; mesh 370 from info.dat is not used) → `CallVirtualFunctionsForCreation` 0x774CA0:
  **scale ×2**, mesh 31 `MSH_SHARK_BONED`, clip 129 `ANM_SHARK_BONED_SWIM` looping, +0x6C (heading) = 0. No AI: per
  turn `Whale::Process` 0x775280 only copies Pos into +0x2C (it is moved by the script's `WALK_PATH` along the focus of the
  tracks `Track21`/`Track20` of `camera.edt`: [camera-tracks.md](camera-tracks.md), `ECS/MobileWalkPaths.*`). Per frame `fn_00774E30` (from `GLandscape::Draw` 0x5E4B26, before the sea):
  clip time += ms; heading = `LH3DMath::GetYAngle` 0x841290 = atan2(dz, dx) in [0, 2π) of Pos − +0x2C (the
  previous one if it did not move); position interpolated with the turn fraction (each end at GetAltitude + relY);
  bottom part in 0xFF303070; wake `fn_00775170`; the top part is `Whale::Draw` 0x774E10 (table[255], default
  plane). No shadow, no reflection, cannot be picked up.
  Wake: a **global** timer 0xDCB984 for all sharks (each one adds the frame's ms to it): if
  it exceeds 50, `%= 50` and a ring at (p.x, 0, p.z), growth 10, aspect 0.5, rate 0.5, cell 0x31, 0x90FFFFFF,
  angle = heading, without writing +0x1C. p = the position of `EBone.matrices[0]` times the matrix of bone `EBone.bones[0]`
  (0x77507D, `fn_007FAE60`; in mesh 31 bone 0 and (−0.079, −0.003, 0.304)): `L3DMesh::GetEBonePoint0`.
  The timer adds integer ms (`g_game+0x250540`, 0x775265: the difference between two integer readings of the game
  clock, `GGame::Loop` 0x54D374); openblack does the same (clock in double and integer ms per frame, without losing
  time at high fps). Space of `[0xC37D9C]`: the one just filled by the shark's own `DrawCutByPlane`
  (`fn_00811C70` → `fn_00839980`/`fn_00839BC0` blend the clip's frames and `fn_00839F10` multiplies each bone
  by its parent and the root by the object's world matrix, +0x14): **world space**, no camera. openblack:
  model × pose (bones in model space) × point, the same product. Hook `OPENBLACK_TEST_SHARK=1` (the two sharks of
  `FollowUs` with their `WALK_PATH`).
  At noon: light fin and tail above the water, dark blue body through the sea,
  white rings coming off the back towards the tail.
- `Whale` is 0x74 bytes, vtable 0x8FEBEC, kept in the list g_game +0x205D04. The ctor copies Pos into +0x2C (`MobileObject::MobileObject` 0x606E40); +0x6C = 0 after the creation.
- `fn_00774E30`: the heading is stored with fstp dword (0x774EEF..0x774EF4); the interpolated position starts at 0x774EFB; the wake uses the first EBone point of its mesh (0x775049..0x7750A3).

## Fish puzzle

Land 4, `PuzzleGame` 14: 30 fish have to be in the net at the same time for 0.5 s. **Faithful**; the fisherman and the
scroll are missing. The normal fish shoals (fish farms: drawing, scaring, fishing, stock) are in
[rendering-objects.md](rendering-objects.md#fish-farm-fish-shoals); their creation from the script, in
[map-loading.md](map-loading.md#fish-farms-create_fish_farm--create_town_fish_farm).

### The net, the bait and the shoals

- **Faithful** (Land 4, `PuzzleGame` 14, `fn_006D7480` branch 0x6D7FCD): bait `{pos, radius 11, need 30,
  500 ms}` + net `FishPlot` (ctor 0x829A30: `Data\MISC\Fishplot.l3d`, a static float with dynamic lighting, drawn
  at 7 points `pos + 11·(cos(i·2π/7), 0, sin(i·2π/7))`, phase 0, closure 1) + 2 shoals of 15 (range 7) at `pos + (±12, 0,
  12)` with `+0x5C = bait`. `fn_00824B90` every frame: `inside = 0` on all baits; per shoal, `n =
  fn_00824DA0` (0 and the shoal no longer moves or is drawn if the bait is `done`; otherwise, the visible fish with
  `dx² + dz² < r²` after moving, 0x824AB8), and if it has a bait: the net under the water (`fn_00829BC0`), `inside += n` and, with
  `inside ≥ 30` and not `done`, `timer += g_game_time_inc`; at 500 ms `done = 1`, the net closes and **each of the 15
  fish of each shoal of that bait** releases a ring (its position, growth 2, rate 1, +0x24/+0x28 = 1, cell
  0x30, white; +0x1C unwritten). At the end, a bait with `inside < 30` resets the timer to 0 (0x824D2E): the 30
  have to be inside at the same time for 0.5 s in a row. `fn_00829BC0` (dt = ms·0.001): if it is closing and `k ≠ 0`, `k =
  max(k − 2·dt, 0)`, radius `1 + 10·k` and the points are repositioned; `phase += 2·dt`; `SetClipPlane(0, −1, 0, 0)`, per
  float `SetPosition((x, y + 0.5·cos(i² + phase), z), 0, 1)` + `DrawCutByPlane` (vt+0x11C, fn_0080C050), and
  `SetClipPlane(0, 1, 0, 0)`. Since it is called **once per shoal**, with the two shoals the phase advances 4/s and the net
  closes in 0.25 s, not in 0.5 (this is replicated). Above the water `fn_00829B50` (from `fn_00824D60`, 0x5E6296): the same
  floats with the default plane. The puzzle shoals are not fished (`fn_00824B10` skips `+0x5C ≠ 0`).
  - openblack: `components::FishBait` / `FishPlot` and `FishShoal::bait` (FishFarm.h), `ecs/FishPuzzle`
    (`CreateFishPuzzle`: the shoals are `FishFarm`s with a full stock and no `Transform`), the rule in
    `ecs::UpdateFishShoals`, `Renderer::DrawFishPlots` (`RendererFishPlot.cpp`; own instances, cut mode of
    `vs_object`): the bottom part in the reflection pass, mirrored, after the fish; the top part in the main pass
    after the rings. The net is drawn once per frame (the original draws it twice, once per shoal, with the
    phase of each call: it is not noticeable). The colour of the cut is the default `+0x4C`, 0xFFFFFFFF (ctor of
    `LH3DMeshedObject` 0x8164F7): the `FishPlot` ctor does not call `SetColour` (vt+0x2C); `UseDynamicLighting`
    (vt+0x58, `fn_008168C0`) only sets bit 0x20 of +4, and the only ones that write +0x4C are the drawings of vt+0x100,
    +0x110, +0x130 and +0x154, which the net does not go through (`fn_00829B50`/`fn_00829BC0` only call vt+0x20 and
    vt+0x11C). The script side (`CREATE_WITH_ANGLE_AND_SCALE` 32/14 and `PLAYED`) is done: see
    [below](#the-script-side-puzzlegame-14-and-played). The fisherman and the scroll are missing.
    Hook `OPENBLACK_TEST_FISH_PUZZLE` ([Test hooks](#test-hooks)).
- The net mesh is loaded once with `LH3DMesh::CreateFromHD("Data\MISC\Fishplot.l3d", 0)` (0x829A63). The 7 floats are 2π/7 apart ([0x9A3964] = 0.897598), at the centre's y (the ctor 0x829A30 and fn_00829BC0 0x829C1F..).
- The shoals' centres are written at 0x6D8089..0x6D8232: +12 in x and z for the first, then x − 24.
- The shoal fish (fn_00824740, also used by the fish farms) draw their randoms in this order:
  1. the size (0x82475E, at least 1e-4 [0x8BF518], 0x824763..0x824774);
  2. the sprite's cell, ftol(r) & 0x3F into +0x28 (0x824797);
  3. the sprite's angle +0x14 (0x8247DB), later overwritten with the heading at every fn_008248E0 (0x824AA1);
  4. the animation phase +0x1C (0x8247F7);
  5. z, y, x in that order (0x824809, 0x824819, 0x82482C), then the centre is added (0x824846..0x824877);
  6. the heading (0x82488D, 0x82489F);
  7. the turn rate ((r + 1) × speed) × [0x900C90] (0x3F20D97C, about π/5; 0x8248B1..0x8248CE).

  The flee time +0x20 starts at 0 (0x8248BC).
- Each frame (`fn_00824B90`): nothing when d² > 90000 (0x824E16). When d² > 40000 the alpha byte is the low byte of ftol((1 − (d² − 40000) × 0.0001) × 255) (0x824E31..0x824E6F), which reaches 0 at d² = 50000 and then wraps.
- The shoal timer (0x824E73..0x824F2C, before the splash): timer −= g_game_time_inc × 0.001 (whole milliseconds). Below 0 the shoal gets a new target: CRT Random(−[+0x58], [+0x58]) for z (0x824EA3), then for x (0x824EB9), y the centre's, and timer = 0.5 × the distance from the previous target (0x824F11).
- On a splash the shoal darts 2 units in a random direction (CRT Random(0, 2π), 0x824F41). The fish within 8 units of the splash flee from it for 2 s, and only then is the shoal's timer set to 2 (0x82503B).
- A bait counts the fish inside its radius in (x, z) at 0x824AB8..0x824AF5.

### The script side (`PuzzleGame` 14 and `PLAYED`)

- `CREATE_WITH_ANGLE_AND_SCALE(32, 14, PuzzlePos, …)` → `fn_006D6680` (puzzlegame.cpp, 0x588 bytes, list
  g_game+0x205D14). It does nothing until `GlobalGameLists::Process` 0x591449 calls `fn_006D7480` every turn:
  0x6D74C3 nothing if +0x3C; if the "played" test (`fn_006D66E0`) gives 1, +0x3C = 1 and out; otherwise, the type's step. The
  type 14 (0x6D7FCD) creates the first time the bait at `ConvertToLHPoint(+0x14)` 0x6041C0 (altitude + the script's y), its net
  and the two shoals (see [above](#the-net-the-bait-and-the-shoals)).
- `PLAYED` (64) = `GScript::Played` 0x6F9DC0: creature → its plan (0x6F9DF4); Living → `IsScriptAnimationComplete` or its
  state; weather → +0x78 == 0; **PuzzleGame** (vt+0x498) → `fn_006D66E0`, switch 0x6D6C94 on type − 1: type 14
  (0x6D6A32) gives 0 without a bait, 1 if `bait+0x18` (done) —and sets +0x3C = 1—, 0 otherwise. Lost object or "Thing not living"
  → 1 (openblack: everything that is not a villager, animal, creature or puzzle gives 1 with "Thing not living"; the Living and the
  creature are still not ported). The `done` is set by `fn_00824B90` when the 30 fish have been inside radius 11 for 500 ms (the shoals only count
  when less than 300 from the camera, as they are drawn).
- `PuzzleGame::ToBeDeleted` 0x6D6FF0: deletes the bait with its `FishPlot` (fn_00829B20) and the two shoals (out of the list
  0xEB99F4, with their 15 fish).
- openblack: `components::PuzzleGame`, `src/ECS/PuzzleGames.{h,cpp}` (creation, turn, `PLAYED`, clean-up of the
  deleted ones), `CreateScriptObject` type 32 and `PLAYED` in `CHLApi`. Only type 14; the other puzzles (Hanoi,
  mazes, totems, chess...) are not ported and their `PLAYED` gives 0.

## The missionaries' boat (PetitNavire)

`PLAY_JC_SPECIAL(6)` in Land 1 (`TheMissionaries`): the launch of the missionaries' ark and its voyage. **Faithful**,
with the differences stated at the end. The reflection of the hull is drawn like the other reflections
([rendering-objects.md](rendering-objects.md#object-reflections-and-hand-shadow-on-objects)); the build
percentage of the ark in the dry dock (`BUILT_PERCENTAGE`) is in
[map-loading.md](map-loading.md#build-percentage-of-a-feature-built_percentage-chl-property-22).

The GScript function of a CHL opcode is in the table 0xC0DB98 + 0x90·opcode.

- **CHL**: `PLAY_JC_SPECIAL` (326) = `GScript::PlayJCSpecial` 0x708ED0, table 0x708F74 on the integer value (0..15):
  0, 1, 2, 4, 5, 6 → `fn_005DF9C0(n)`; 3 → a 0x2C ScriptGFX object (0x828DB0); 14/15 → [0x9CD384] = 1/0. In
  `fn_005DF9C0` case 6 (0x5DFBF8) is `new PetitNavire(0)` (0x68 bytes). `IS_PLAYING_JC_SPECIAL` (327) 0x708FC0 pops
  a **float** (ftol) and returns 1 except with 13, which gives [0xD19C94] (only set by the hand intro, `fn_005DF640`
  0x5DF807; not ported → 0).
- **A single boat** [0xD19CB4]; the ctor 0x5E1020 frees whatever is there (`fn_005E13C0`: releases Boat1/Boat2 and the objects and
  sets the global to 0). Constants (initialisers `crt_xc_fn_JCMisc_005DFED0/005DFF00`): dry dock [0xD19A08] =
  (1881.0833; 8.1316; 3154.1094), exit at sea [0xD199F8] = (1456.54; 0; 3263.06).
- **Objects**: hull MSH_O_ARK (339) static, `SetPosition(dock, 0, 1)`; sailor MSH_P_NORS_SAILOR (504) animated with
  ANM_P_PUSH_OBJECT (346). Animations +0x08..+0x20 = 346, 332 OVERWORKED1, 235 CROWD_WON_2, 333 OVERWORKED2, 406
  TITANIC, 378 SITTING_SWINGING_LEGS, 359 SCRUBBS. Mode 0: its own dynamic shadow (`fn_008745A0`, holder+4 = 1 and
  ShadowInfo+0xC = **0**: it also falls on objects). Mode 1: cow MSH_A_COW_1 (16) with ANM_A_COW_EAT_2 (36), grain
  pile MSH_S_GRAIN_PILE (533) and `LH3DSprite::Create(5)` with the smoke material [0xEA1ABC] (`smoke.raw`, **mode 6**,
  0x80BC7D), +0x14 = 3.92699 (5π/4), flag 0x40 (flat in XZ), cell 0x31; phases +0x50[i] = i·1200 ms.
- **Hull tracks**: `Data\MISC\boat1.anm` ("beach04", 158 frames, 15833 ms, flags 0x501 = looping) and
  `boat2.anm` ("beach_sailing", 44 frames, 4466 ms, 0x501). A single matrix per frame with **determinant −1**;
  `fn_0083AC70` interpolates the 12 floats (like `LH3DAnim::GetPose`) and composes it with the parent matrix M (`fn_007FAFF0`:
  track·M). Then `RotateY(π/2)` 0x5198F0 and `fn_007FAE60(diag(−1, 1, 1))`, which **multiply in front** (local
  space): hull = mirror·RotY(π/2)·track(t)·M, determinant +1. In glm: `M · track · eulerAngleY(−π/2) · scale(−1, 1, 1)`.
- **PreDraw 0x5DFF20** (from `GLandscape::Draw` 0x5E490F, before the sea), with dt = integer `g_game_time_inc`:
  - Mode 0: +0x34 += dt; +0x64 = +0x24; if `!+0x48 || +0x34 > 3000`, +0x24 += dt. If +0x24 > 15833 − 400 it is deleted,
    does `new PetitNavire(1)` and **returns**: that frame the new boat has no PreDraw and its PostDraw draws it once
    at the dry dock without rotating. Otherwise: M = Translate(dry dock); hull y += `GetAltitude(hull.xz)` − `GetAltitude(dock.xz)`
    (0x5E00EB..0x5E0154); `fn_00874850` (shadow); +0x4C = 0xFF303070 and `DrawUnderWater` (vt+0x118, the reflection);
    `fn_00801C90` gives it back the land light.
  - Mode 1: +0x24 = (+0x24 + dt) % 4466 (looping; otherwise, min(…, dur − 1)); +0x34 += dt; at 60000 ms it is deleted. M =
    RotY(π/4) (0x92B210) at (1456.54; 0; 3263.06) + (−k, 0, −k)·0.005·+0x34, with k = `InverseSquareRoot(2)` 0x841170
    (table 0xEEA394 plus one Newton step = 0.70710659): **5 u/s** towards −x −z, 300 units in total. Hull as
    above (no height correction or shadow), 0xFF303070 and `DrawUnderWater`.
  - **One drawing per frame**: there are no "two parts" or two drawings per frame. 0x5E0100-0x5E0190 (mode 0) and
    0x5E0380-0x5E03EE (mode 1) are **mutually exclusive** branches (0x5E0195: `cmp +0x30, 1`); **both** set up the mirror
    diag(−1, 1, 1) (0x5E00C1-0x5E00D9 and 0x5E0350-0x5E03B6) and the π/2 is the hull's `RotateY`. Each frame there is a
    single `DrawUnderWater`, in 0xFF303070.
- **PostDraw 0x5E03F0** (from `fn_005E5CD0` 0x5E6250, after `fn_00824140`):
  - Mode 0, 2D sounds (`GAudio::PlaySoundEffect` 0x429E30, bank GGlobal+0x3BC = `Scriptsfx.sad`, options +0xBC = 2,
    that is +0x50 = mode 2, owner 0, is3D 0; in openblack through `sample_play`, one of the 16 channels)
    when the hull time crosses the threshold (+0x64 < threshold < +0x24): 100 → 62 `MissionaryBoatCreak_01`, 1500 → 61
    `MissionaryBoatSlide_01`, 3900 → 60 `MissionaryBoatSplash_01`.
  - Mode 0, every 200 ms (+0x38 += ftol(dt); > 200 → action and +0x38 = 0): if 3900 < t < 6500, 2 `SmokyStuff::Create`
    (hull + (r2 − 10, **7**, r1), mode 0, size 7, 0xFEFFFFFF); if 1130 < t ≤ 3900, 2 × (hull + (r2, 0, r1), 0, 5,
    0xFFB88C38, sand), with r1 = Random(−20, 20) and r2 = Random(−2, 2) in that order (corrects the report: the ±20 goes in z and
    the −10 / +7 in x / y). Then the hull (vt+0x100).
  - Mode 0, sailors (the same object drawn 5 times): if t > 850: if +0x48, +0x34 = 0; animation +0x0C + (i % 3)·4
    and +0x48 = 0. Position (dry dock.x + {5.2; 5.3; 5.5; 5; 5}[i], ground, dry dock.z + (i − 2.5)·3 + {1; −0.7; 0; 0.4; −0.2}[i]
    + 10) (0xBF2B1C, 0xBF2B44), angle −π/2, frame ({5, 500, 1500, 455, 2000}[i] + +0x34) % duration (0xBF2B30),
    colour = land light at its place.
  - Mode 1, wake (0x5E0785): phase = (phase + dt) % 6000, t = phase/6000; position = hull·(0, 0, 100t − 15) with
    **y = 0.2**; half width +0xC = 30t + 10, aspect +0x10 = 0.5; alpha = ftol((1 − f)·255) with f = (t − u)/(1 − u)
    if t ≥ u (otherwise, u) and u = [0xD19CB8]: **nobody writes that float** (only reads at 0x5E086E..0x5E0893, no
    initialiser), so u = 0 and f = t; it is only drawn if f > 0.2 (double 0.2 at 0x8C7C68).
  - Mode 1, deck (0x5E08E7..0x5E1015, emulated): the matrix of each one is L·hull (`fn_007FAFF0` and copy to
    obj+0x14), L = RotY(a)·scale + t in the hull's frame; colour = the hull's +0x4C. Cow (a = −1,
    t = (−1.778; 10.78; 1.83), frame +0x34 % dur) and again (a = −0.7, t = (−1.778; 10.78; 3.83), +0x34 + 1255);
    grain (scale 0.26, t = (−5.708; 10.854; 3.199)); the sailor object with MSH_P_NORS_F_A_1 (498) and TITANIC at
    (−0.14; 13.213; −19.657) (+0x34), with 504 and TITANIC at (−0.14; 13.213; −18.9) (+0x34 + 500), SITTING rotated π at
    (−6.25; 11.424; −7.227) (+0x34) and at (−5.25; 11.424; −7.227) (+0x34 + 2345), SCRUBBS rotated π at
    (5.881; 10.741; 0.174) (+0x34).
- **SmokyStuff** (0xCC bytes, list 0xEB99CC): `Create` 0x823C90(pos, mode, size, colour) = 15 mode-6 smoke sprites
  that face the camera; each one at pos + (c, b, a) with a, b, c = Random(−size, size), rotation Random(0, 2π), cell 0x10,
  velocity norm(e, size, d)·Random(0.3; 1)·size (mode 0). `fn_00824140` (0x5E619C, dt = ms·0.001) → `fn_00823F70`:
  life −= dt/3 (mode 0), nothing if life ≤ 0; colour (life·100)<<24 | 0x808080 with the argument's rgb; rotation ±5·life + v.x;
  half width ((1 − life)·2 + 1)·size/2; pos += v·dt; cell (int)(life·15); it is freed with life < 0. `Random` 0x81D180
  = a + (b − a)·rand()/32768 (stdcall). Billboard of `LH3DSprite::Draw` 0x84071D: local x → (cos, −sin) on screen,
  local y → (sin, cos).
- openblack: `src/ECS/MissionaryBoat.{h,cpp}` (was `PetitNavire`; state, entities, PreDraw and PostDraw in `Update` with the integer time and the
  remainder kept), `src/ECS/DisappearSmoke.{h,cpp}` (was `SmokyStuff`; the same module as the smoke of the corpse in
  [animals.md](animals.md), `Object::CreateSmokyStuff` 0x63A810), `components::DynamicShadow` (the hull shadow goes into
  `graphics::shadow_list` with the fixed sun, `useSun`, and also falls on objects), `Renderer::DrawBoatReflection` / `CollectBoatSprites` / `DrawBoatSprite` (`RendererBoat.cpp`; the
  reflection in 0x303070 is `Renderer::DrawUnderWater(view, hull, sea_pass::UnderWater(0xFF303070, 0))`, mode 2 of
  `vs_object` with the packed rgb; the sprites, one by one in the
  common transparent queue, LH3DSprite::AddDrawing 0x840CB3). Remaining differences: the deck takes the land light of its
  own place (not the hull's); the hull shadow comes out with the visibility of the normal camera (the original reads the mirror's, (inferred));
  the mode ≠ 0 of `SmokyStuff::Create` (0x823DA7) has no calls here and is not ported.

## Fixed scenery per land: Land 3 waterfall, Land 4 ark and dinosaur

**Faithful** (done), with the differences stated in the openblack point.

- `GWaterfall` (`CREATE_WATERFALL`, Land command 68, 0x7175D1; ctor 0x734130, vtable 0x8EC14C) is an
  **empty** object: `CallVirtualFunctionsForCreation` 0x7341B0 = `ret 4`, it neither draws nor sounds, and no land uses it.
- `DesignedWaterFall` 0x5E3770 (LandFeature.cpp), every frame from fn_005E5CD0 (0x5E5CDA), according to the land
  number (`SET_LAND_NUMBER`, g_game+0x205A08). When the number changes (last land at 0xBF34E4, 74 at startup) it deletes
  the two objects (0xD1A31C / 0xD1A324), the `ScriptMarker` 0xD1A32C, its `SoundTag` 0xD1A330 (ToBeDeleted: the loop
  finishes its pass) and releases the meshes. They are not game objects: loose `LH3DObject`s (no shadow, no cell).
  A new land deletes what the last one had at 0x5E37AB. The meshes (0xD1A320 / 0xD1A328) stay loaded in the mesh
  manager instead of `LH3DMesh::Release`.
  - **Land 3**: `Data\MISC\waterfall3.l3d` (`LH3DObject::Create(0)`, static) at (3059.23; **0**; 3145.33), angle
    4.7, scale 1, dynamic lighting, +0x10 = 1. Every frame `V = V − 0.5·dt` and `V −= (int)V` (it stays in −1..0) with
    `SetUVOffset(0, V)` (vt+0xE8 fn_007F9B70: +0x68/+0x6C and flag 0x400); dt = `g_game_time_inc`·0.001. Every 0.7 s
    of game time (timer 0xD1A338, reset even if the quota is full) a ring at (3018.8; 0.2; 3130.15):
    growth 30, +0x24 = 1, aspect 1, rate 0.3, cell 0x30, angle 0, colour `0x80 << 24 | table[255].rgb`; the
    drift field +0x1C is not written. `SoundTag::Create(marker, 12 G_WaterFlow, false, mode 2, loops −1, 0,
    3D, InGame, 0)` (0x5E3921).
  - Land 3's addresses: the position 0xD1A2C0 with the angle 0xBF34E8 and the scale 0xBF34EC; the waterfall's texture
    V offset is at 0xD1A334; the ring period 0.7 s is 0xBF34F0; from 0x5E3A32 its point, growth (0xBF34F4), rate
    (0xBF34F8) and alpha (0xBF34FC); the ring is made at 0x5E39BA.
  - **Land 4**: `Data\MISC\arche.l3d` (Create(1)) at (3538, altitude, 2129), angle 8.9728, scale 1.1, +0x10 = 10,
    with the same `G_WaterFlow` SoundTag at (3538, 0, 2129); `Data\MISC\dinosaur.l3d` at (2690, altitude, 2590)
    (≈ 138.7: high ground), angle 1.57, scale 1, +0x10 = 10, with its terrain footprint (fn_0081E9E0, the buildings'
    footprint list). `dinosaur.l3d` is the only one of the three with `ContainsLandscapeFeature` (0x8000);
    `waterfall3.l3d` has a footprint block but without that flag and nobody stamps it.
  - Land 4's addresses: 0xD1A2B0 with 0xBF3500 / 0xBF3504 and 0xD1A2A0 with 0xBF3508 / 0xBF350C; both are morphable
    objects (`Create(ecx = 1)` at 0x5E3B08 / 0x5E3CF8).
  - The marker is created with `MapCoords(LHPoint)` (0x603340 stores y − altitude) and `Get3DSoundPos` 0x56FE20 adds the
    altitude back: the sound comes out at exactly y = 0. `G_WaterFlow` (InGame 12; the bank describes it as "Citadel
    waterfall", but it is only used by this waterfall and the ark): volume 50, loop, min 60, **max 110**, scale 6, mode 2.
- openblack: `ECS/DesignedScenery` (`designed_scenery::Update` every frame after the rings, `OnLoadMap` before the
  registry reset): `Transform` + `Mesh` entities (meshes `misc/<file>` loaded on use), `UvScroll` on the
  waterfall, `AddWaterRing` with `seaLight`. The dinosaur's footprint comes out by itself in the footprint pass (mesh with
  `ContainsLandscapeFeature`). Sound: `Audio/SoundTags` (see [Water audio](#water-audio)). The objects' +0x10 (float 1 / 10) is the LOD
  distance factor k of `fn_00815A70` (vt+0x100): D = min((k + 1)·[0xC37EA0]·[0xC3813C], 100000) (0xC3813C is moved by
  `LevelOfDetail`), LOD 1/2/3/4 below 23.33·D / 66.67·D / 86.67·D / beyond (`g_last_distance` 0xEA1AF4) and
  each submesh is drawn if bits 29..31 of its flags contain the LOD: all three meshes have them all
  (0xE0000800), so k changes nothing on screen and is not stored. Differences: the dynamic lighting is the same as for all objects; when loading
  a map the registry is cleared, so the loop is cut abruptly instead of finishing its pass.
- **What scrolls (done)**: nobody reads the 0x400 flag. The static `Draw` 0x80DB30 reads U and V with vt+0x8 /
  vt+0xC (0x80DE86) and leaves them at 0xECA62C / 0xECA630 (0xECA628 = 1 if they are not both 0); the default triangle
  submission (`[0xC386EC]` = `LH3DRender::DrawTriangle` 0x82F810) adds them to the UVs **unless byte +5 of the material
  has bit 0x10** (0x82F8CC). In `waterfall3.l3d` the rock (submesh 0, `Textured`, +5 = 0x14) has it and the water
  (submesh 1, `TexturedChroma`, 0x04) does not: **only the water flows**. (Other paths, `fn_0082FD70` from `fn_00812170` /
  `fn_00817930` and `fn_00884750`, always add it; the static one does not go through them.) In `AllMeshes.g3d` only the
  `S_PHILE*` meshes have that bit, no food pile. openblack: `L3DSubMesh::Primitive::uvOffset` and
  `u_window.w` in `vs_object` (the difference of two frames: only
  the sheet of water changes, and the palms because of the wind).
- **Rings at the foot**: they are drawn with `LH3DSprite::Draw` in mode 13 (`fn_0082ECD0`: SRCALPHA/ONE blending, no alpha
  test, ZWRITEENABLE 0 and without touching ZFUNC): with Z test against the land, as in openblack. The rings at the foot end up almost buried: the ground is at 0 at the point and
  rises to 1.7 within 7 units, so with the Z test only a faint glow is visible.

## Water audio

What sounds with the water and when. The engine (channels, modes, distances, banks) is in [audio.md](audio.md); the
ambience and the hand in the water are, for now, in
[objects-and-resources.md](objects-and-resources.md) ("Hand in the water / gripping
the land" and "Ambience (atmos)"). All **faithful**.

- **Hand in the water**: when starting to grip the terrain over a water cell (`StartLandscapeGrip` fn_005D1AB0), a
  `G_HandInWater_01..10` (InGame 99 + counter) in 3D at (x; 0.2; z), one at a time (clone group 4, mode 3), with the
  splash ring and the fish scare; it does not sound when paused or with something in the hand.
- **Impacts against the water**: with altitude < 3 in the rounded cell the collision type is WATER and the
  `editor.sad` sample from the collision table plays ([physics.md](physics.md#sounds-dust-and-the-look-of-impacts)); `G_BigSplash`
  (mode 2) does not repeat while the same sample of the same object is playing.
- **Drowning**: the events of clip 252 `P_DROWNING` (splash 157 and voice 134,
  [above](#sinking-drowning-and-being-deleted)).
- **The missionaries' boat**: three 2D sounds from `Scriptsfx.sad` at the launch (62, 61, 60;
  [above](#the-missionaries-boat-petitnavire)).
- **Land 3 waterfall and Land 4 ark**: `G_WaterFlow` (InGame 12) looping through a `SoundTag`
  ([above](#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)).
- **Particles that fall into the water**: the bounce and the ripple of `UpdateRuleGravityWithFloor`
  ([particles.md](particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules)).
- **Ambience**: by the ATMOS type of the cells near the camera: 1 SEA `ocean.sad`, 3 COASTAL `shore.sad`, 2
  STILL_FRESH_WATER `lake.sad` (and 9 RUNNING_WATER `stream.sad`, which no base map uses); the sea goes quiet within 20
  of a COASTAL cell. Rivers have no sound of their own (`ATMOS_TYPE_RUNNING_WATER` not analysed, see
  [rendering.md](rendering.md#rivers)).

The sound tag engine used by the waterfall and the ark (it is not in [audio.md](audio.md)):

- **SoundTags** (`Audio/SoundTags`, SoundTag.cpp 0x71E300..0x71ED90): tag {thing or fixed point, offset,
  sample, loop, active}. `ProcessTurn` = `SoundTag::ProcessSoundTags` 0x71E5F0 once per turn (GGame::EndTurn):
  if the thing no longer exists → ToBeDeleted; if it is active, `fn_0071E680` calls `GAudio::PlaySoundEffect` 0x42A100 with
  **the tag itself as owner** of the channel (+0x20), the point (`Get3DSoundPos` of the thing), the offset +0x1C,
  sample +0x28, follow +0x30 (only with a thing; `false` for the scenery ones), mode +0x38, loops +0x3C, is3D +0x44 and its
  bank (`SoundTag::Set` 0x71E4F0); 0x429E30 only starts it with the camera ≤ maxDist (.sad +0x26C) from the point and
  mode 2 leaves the channel that is already playing. In openblack it goes through `sample_play` (owner `Owner::Tag`), so it counts towards the 16
  LHaudio channels; `IsPlaying` = `LHSampleIsPlaying` (fn_0042A2D0) and `ReleaseLoop` = 0x42A310. `SetActive(0)` cuts
  abruptly (LHSampleStop); `Delete` releases the loop (LHSampleReleaseLoop) and the tag dies when the pass finishes.
  `Clear` in `Game::LoadMap` before the registry reset (the emitters are entities). It is the
  complete SoundTag (`audio::tags`, [audio.md](audio.md)): `Delete` = ToBeDeleted 0x71ECB0 →
  CreateSoundTagForDeadObject 0x71ECD0 (releases the loop only if it is playing with loops), and the street lanterns (`LanternSounds`)
  are tags of this module.

## Water in other pages

- Drawing of the sea (rows, undulation, drift, what is under the sea), the coast and its alpha, the rivers and the glow of the
  hand at night over the water: [rendering.md](rendering.md#sea-skyraw--skyaraw),
  [Coast](rendering.md#coast), [Rivers](rendering.md#rivers), [Sky](rendering.md#sky-sun-moon-and-clouds-original).
- Reflections of the hand, the objects, the creature and the boats; the underwater cuts (`DrawCutByPlane`); the fish
  shoals of the fish farms. Everything in the water zone that is drawn under the sea
  (`RendererBoat`, `RendererFishPlot`, `RendererCut`, the fish and the reflected moon) goes through `graphics::sea_pass`
  (plane, mirror, face and light in a single place):
  [The under-sea pass](rendering-objects.md#the-under-sea-pass-graphicssea_pass),
  [rendering-objects.md](rendering-objects.md#object-reflections-and-hand-shadow-on-objects),
  [DrawCutByPlane](rendering-objects.md#cutting-by-the-water-plane-drawcutbyplane),
  [fish shoals](rendering-objects.md#fish-farm-fish-shoals).
- Creation of the fish farms from the script:
  [map-loading.md](map-loading.md#fish-farms-create_fish_farm--create_town_fish_farm).
- Buoyancy and sinking in the physics engine: [physics.md](physics.md#engine-physob-0x7fb7300x7fe7b0); dropping from
  the hand: [physics.md](physics.md#water-in-impacts-and-when-dropping).
- Shark routes (`WALK_PATH`, `camera.edt` tracks): [camera-tracks.md](camera-tracks.md).
- Particles over the water (bounce, ripples, explosion steam): [particles.md](particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules).
- Animals in the sea: [animals.md](animals.md#hand-flight-and-death); leaving something over the sea with the hand:
  [hand-and-interface.md](hand-and-interface.md).

## Test hooks

All in [openblack-internals.md](openblack-internals.md#debug-environment-variables) (and the physics ones in
[physics.md](physics.md#test-hooks)):

- `OPENBLACK_DUMP_LAND_AVOID=1` (or `=<file>.png`): the `LandAvoid` mask (colours [above](#the-creatures-landavoid-mask)).
- `OPENBLACK_TEST_SEA="x,z,type[,height]"` (+ `OPENBLACK_PHYSICS_TRACE=1`): an object in the water that floats, sinks or
  drowns; with `OPENBLACK_TEST_CUT=1` its part under the water is also drawn cut.
- `OPENBLACK_HAND_TEST_DROP="x,z,seconds[,type]"`: the hand gently drops something at (x, z), e.g. a villager in the sea
  (1464; 2016).
- `OPENBLACK_TEST_SPLASH="x,z"`: one hand splash per second (rings, sound, fish scare).
- `OPENBLACK_TEST_SHARK=1` (or `="track,camera[,forward[,from[,to]]]"`) and `OPENBLACK_WALK_PATH_TRACE=1`: the sharks.
- `OPENBLACK_TEST_FISH_PUZZLE="x,z[,inside]"` (+ `OPENBLACK_HAND_TRACE=1`): the fish puzzle.
- `OPENBLACK_TEST_JC_SPECIAL="6[,mode[,frames[,ms]]]"` and `OPENBLACK_BOAT_TRACE=1`: the missionaries' boat.
- `OPENBLACK_SCENERY_TRACE=1` and `OPENBLACK_SOUND_TAG_TRACE=1`: the fixed scenery of Land 3/4 and its sound tags.
- `OPENBLACK_AUDIO_TRACE=1` (channels) and `OPENBLACK_ATMOS_TRACE=<n>` (ambience).
- Tests: `test_sea_cells`, `test_water_queries` and `test_psys_water`.

`test_water_queries` (real Land1, see [openblack-internals.md](openblack-internals.md#tests-and-test-data)):
mask consistent with the cell predicates, nearest coast (and that the spiral keeps the cell fraction),
nearest river point and its y, drinking water no further than the river, the house's cache and distance with the original's
inverse root.

## Sources

- `bw1-decomp` (`src/Black/Object.cpp`, `include/chlasm/ScriptEnums.h`).

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x750AC1..0x750AED` | Villager::HasSunk's creature-learning part (ConsiderMakingCreatureMimicPlayer, THROW_IN_THE_SEA 21, for the player who last dropped it) | `ecs::HasSunk` → `creature_mimic::ConsiderThrownInTheSea` ([creature.md](creature.md#mimicry-and-the-town-hooks)) |
| `0x750AF5..0x750B0F, 0x750B1E` | Villager::HasSunk's dead branch: status bit +0xB4 & 1 (not Living::IsDead) -> SetTopState(14 DYING), counter = dyingTimeWithoutGraveyard (+0x290) | `ecs::HasSunk` |
| `0x5F0C9D` | end of Villager::EndPhysics' water branch (0x5F0BAF-0x5F0C9D) | `ecs::VillagerEndPhysicsInWater` |
| `0x5F0BC0..0x5F0C41` | Villager::EndPhysics in water at 0 life: Living::IsDead (vt +0xAF4) -> DYING with dyingTimeWithoutGraveyard, else counter 0 and VillagerDead with the physics' player or the neutral player (g_game +0x205A5B) | `ecs::VillagerEndPhysicsInWater` |
| `po +0xCC (y of the +0xC8 point)` | Object::IsDrowning reads the physics body's centre of mass y at PhysicsObject +0xCC; flag 0x40 of Object +0x24 = has a PhysicsObject | `ecs::IsDrowning` |
| `0xFA9314` | sea ripple sine table, 16 entries sin(i pi / 8), row r uses phase (frame + 2 (r + 1)) & 15 | `fs_water.sc RowUv` |
| `0xC39904 / 0xC39900 / 0xC39908` | sea row alpha constants: 255 up to view depth 7000, down to 80 at 14000 (rounded by fistp) | `fs_water.sc RowAlpha` |
| `[ebp-8] += [ebp-0x30] (in fn_00879930)` | sea rows' 1 / depth stepped per row, affine in screen y from the range vertices | `fs_water.sc RowUv (u_seaRows.z / .w)` |
| `0x84A390 / 0x84A3D0` | CRT initialisers fn_0084A380 / fn_0084A3C0 setting LH3D's user plane default (0, 1, 0, 0) | `sea_plane.sh u_objectClip` |
| `0x85113C..0x851149` | fn_00850FC0 (DrawUnderWater): code 0x800, "d > 0 out" on the mirrored point | `sea_plane.sh SeaPlaneDiscard, sea_pass::Kept` |

## Pending

Everything else about water is done and faithful to the original.

**Can be done now (water area):**
- The missionaries' boat (`ecs/MissionaryBoat`): its sprites (wake, `DisappearSmoke`) already go one by one in the common
  transparent queue (`Renderer::CollectBoatSprites` / `DrawBoatSprite`, LH3DSprite::AddDrawing 0x840CB3; see
  [rendering-objects.md](rendering-objects.md#the-single-transparent-queue-lh3dzsorter)); missing: the light of the
  deck, which in the original copies the hull's +0x4C colour to each passenger (openblack lights each instance by its
  position); the hull shadow also on objects (its `ShadowInfo` +0xC = 0); the mode ≠ 0 of
  `SmokyStuff::Create` (branch 0x823DA7).
- Foam of impacts against the water (`fn_0074F2D0`): the tint towards the base light according to the snow (`SnowCover`
  [0xEDC344], `fn_004ED180`), now that the weather exists.
- Check that the sound ambience and its group by alignment now read the real values (`CameraWeather` →
  `weather::atmos::GetWeatherSmooth`, `atmos_banks::Alignment` → `Clouds::InfluentialPlayerAlignment`) and not 0.
- Screenshots still to be seen: the moon reflected in the sea; that the food pots and the orbs still animate their
  texture with the material's bit 0x10 gate (two frames need comparing); the rings of the rain
  (`water_drop_cb` 0x54EEA0) and of the water miracle (`SpellWater`) over the sea.
- `GET_PROPERTY` / `SET_PROPERTY`: only `FLYING` and `DROWNING`; the other properties are still unimplemented (they are not
  water-related).

**Blocked by other areas:**
- Creature (does not exist): footsteps in the water and fish scare (`fn_00483290`), going into the water and its limits,
  its reflection (0x5E4A02, 0x65A0A0D0), the animal's report of being thrown into the sea (`Living::HasSunk` 0x5ED3A9;
  the villager's is published, [creature.md](creature.md#mimicry-and-the-town-hooks)), `CheckAllCreaturesForCatching`,
  drinking from the sea, and being the consumer of the `LandAvoid` mask.
- The animals' sinking: `Living::HasSunk` 0x5ED370 returns 0 for an animal that is not available (vt +0x2C,
  0x5ED375) before it reports and deletes it; openblack's animal branch of `ecs::HasSunk` has no such test.
- Villagers: fishermen (`FishermanLookForWater` 0x75B4C0 and its state machine, with
  `RemoveFishFarmFood`), drinking (`FindNearestDrinkingWater`, `CREATE_DRINK_WAYPOINT`), the shepherd who takes the flock
  to the water (`ShepherdMoveFlockToWater` 0x768CC0), `Villager::CreateDroppedResource` and reaction 9
  `REACT_TO_FLYING_OBJECT` when dropping with the hand. The complete death is in [villagers.md](villagers.md) (Death): `VillagerDead`
  0x7506C0 with reason 6, the corpse in the water (dying clip 283, dead clip 249), smoke and skeleton 0x1FF without a
  soul; out of the water the soul of `fn_00828790` (the child one `GVillagerInfo`+0x204 below age +0x138, the adult one
  GetMesh 0x74F880 = +0x214 StdDetail) plays P_DEAD1/2_GOTO_HEAVEN or _HELL, 244/245 or 247/248.
- `lastPlayerToInteract` (+0x104) and `GetPlayerWhoLastDroppedMe`: with a single player, the hand gives PLAYER_ONE
  (inferred) until there are several players.
- SuperVillager swimmers (`M_P_Swim2`, `DrawCutByPlane` and their ring every 1000 ms): they need the Land 1-2 scripts
  (Baywatch, FollowUs).
- `WALK_PATH` of the Livings (0x5EE100, `Living::MoveAlongPath` 0x5EE230): needs the villagers' paths (footpaths).
- Fish puzzle: the fisherman and the challenge scroll (`FishPuzzle.txt`); the rest of the `PuzzleGame` types.

**Done through the shared graphics and audio APIs:**
- The water rings, the fish farm fish and the boat sprites already go through `graphics::billboard`
  and `graphics::frame_anim` (same cells and formulas, no blending between frames). The moon (sign of the
  tilt and of the phase, halo V 0xEDC304) is done, and the fish cell (8 + (ftol(frame) & 15),
  taken before the wrap, dt ≤ 0.1 s, `fn_008248E0`) too. The morphing of the ark and the dinosaur to the ground
  (`UpdateMelting` 0x5E3C55 / 0x5E3DBE) is `MorphWithTerrain` in `ECS/DesignedScenery`. Also, read and left untouched: `fn_008248E0` deducts the
  flee time with the unclamped dt and computes the push after subtracting it (0x82490D..0x82495A); openblack does it with
  the clamped dt and before subtracting.
- Audio (**done**, [audio.md](audio.md)): the sounds that did not go through the 16 channels
  (AnimationSounds, rocks, the camera whoosh, `G_RockPast` and the hand's `PlaySample`s) and the street lanterns
  as `SoundTags`; the audio does not read the ECS and `audio::GetSurfaceType` no longer exists (everyone uses
  `sea_cells::GetSurfaceType`).

**Questions only the user can answer** (memory of the original):
- Was the sea still with the camera stopped? The code says yes (ambient wind 0, no drift).
- The horizon step: about 25 px of mirrored sky above the edge of the sea's 30000 square.
- The moon reflected in the sea and the warm patch of the hand at night over the water (120 × 120).
- A villager dropped gently on the shallow shore also drowns (60 s of the `P_DROWNING` clip), and can be rescued with the
  hand: was it like that?
- The Land 3 waterfall: only the water flows (material bit 0x10); were the rings at the foot visible?
- The beached ark of Land 4 carries the `G_WaterFlow` loop: did it sound like water?
- Calibrate by ear: the hand splash at 40 / 100 / 150 units and the sea, the coast and the loose waves.
- A fireball that falls into the sea: three thin rings and then white steam?
- The launch of the boat: density of the sand dust and volume of the creaks.

Confirmed by the user (2026-10-01): a tree or a rock thrown into the sea looked cut off; the mottling of the shore
may come from the texture pack's `smallbump.raw`; and everything is preferred as in the original.
