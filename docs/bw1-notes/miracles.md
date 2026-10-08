# Miracles one by one

One section per player miracle: what the spell does, its object, its particles and what is missing. The common core
(tables, life cycle, chants, events and effects, seeds, casting from the hand, worship, influence, alignment,
reactions, life and the fire model) is in [magic.md](magic.md); the particle engine, in
[particles.md](particles.md); time and weather, in [day-night-weather.md](day-night-weather.md#weather-and-climate-srcecsweather).

W120 addresses. This page collects what was verified in `runblack.exe` while porting it.

> **Code rules.** In openblack the spells, their objects and their effects keep their state in ECS components and are
> reached through Locator services; meshes, textures and spell files load through the resource caches; the formulas
> (curves, costs, spirals) are pure functions tested with fakes in `test/`; comments describe behaviour in plain
> English, with no decompiled names or addresses (those belong here). See [the conventions](../refactor/README.md).

- [Food and wood](#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)
- [Water](#water-magicspellsspellwater-particlescreatorsmist)
- [Heal](#heal-magicspellsspellhealcpp-particlesruleshealcpp)
- [Forest](#forest-magicspellsspellforest-magicobjectsmagictree-ecstrees)
- [Flocks](#flocks-magicspellsspellflock-particlesrulesflockcpp)
- [Fireball and lightning](#fireball-and-lightning-magicobjectsmagicfireball-particlesrulesfireballlightning)
- [Shields](#shields-magicspellsspellshield-magicobjectsmapshield-particlesrulesshield)
- [Teleport](#teleport-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)
- [Storm, electric storm and tornado](#storm-electric-storm-and-tornado-magicspellsspellstormandtornado-particlesrulesstorm-ecsweatherlightningflashstormclouds)
- [Lightning explosion and missing PSys classes](#lightning-explosion-and-missing-psys-classes-particlesrulesexplosionkeypointsorientforestcpp)
- [The falling spell](#the-falling-spell-what-fallingspell-draws-magicobjectsfallingspell-graphicsrendererfallingspellcpp)
- [Creature miracles](#creature-miracles-pending)
- [Test hooks](#test-hooks)
- [Sources](#sources)
- [Pending](#pending)

## Food and wood (`Magic/Spells/SpellResource`, `Magic/Objects/Magic{Food,Wood}`, `ECS/PotResource`)

What follows has been read in the exe (W120) except where marked.

- **SpellResource** (MAGIC_TYPE 14 FOOD, 15 FOOD_PU1, 21 WOOD; `GMagicResourceInfo::AllocSpell` 0x5FAC20, +0xEC "first
  event done" set to 0 in fn_00724C80; no Process of its own). `SpellResource::GetMagicInfo` 0x724C70 (the runtime
  record has its fields at file + 0x14).
  - `SpellEvent` 0x724D80: first the default event (EffectValues of FOOD/WOOD: only alignment 1, reaction 21);
    then, except for event 1 or if it is closed, n = `resourceAmountFirstEvent` the first time and
    `resourceAmountPerEvent` afterwards; `PayFor(costPerUnit × n)` (the result is not checked); off the map, on water
    (`IsDryLand` 0x603620: altitude byte ≥ 4) or with strength 0 it pays and nothing falls; otherwise,
    `Pot::AddResourceToPos(pos, IS, type, ftol(tribalPower × n), 0, x)`. The IS that `fn_00724CE0` / `fn_00724D30`
    pass is the leader interface of the spell's player; only a human player has one.
  - Food: 200 the first time (1400 chants), then 18 (126); FOOD_PU1 20; wood 500 (1500), then 20 (60).
  - `HasEnoughChantsAndLifeForRecast` 0x724C90 = costPerUnit × first ≤ chants.
  - **The `poisoned` field of FOOD_PU1 does not poison**: fn_00724CE0 pushes `(pos, IS, FOOD, n, 0, poisoned == 1)` and in
    `AddResourceToPos` the 5th argument ([esp+0x60]) goes to `SetPoisoned` (vt 0x69C, with `IsPoisoned` vt 0x4A4) and the 6th
    ([esp+0x64]) to `SetSpeedUp` (vt 0x864, with vt 0x4A8). PU food makes "speed-up" piles (sparkles
    PILEFOOD_SPEEDUP, 46).
- **`Pot::AddResourceToPos` 0x66F270** (`ECS/PotResource.cpp`, used by the hand when dropping a pot and by the grains):
  - off the map, 0; walks 9 cells of 10 m with `GUtils::Spiral` 0x74D7E0 (the one at pos and its 8 neighbours), in each
    the fixed list (+4) and then the mobile one (+0), while something remains;
  - accepts an object if `IsResourceStore(type)` (vt 0x680) or `IsPot` (vt 0x4B4) and `GetResourceType` (vt 0x690) == type,
    and `IsCloseToEqual(pos, GetDefaultFireCentrePos, Get2DRadius × GetRadiusMultiplierForApplyingPotToPos)` (pot 2,
    store/object 1.2); the first one that qualifies takes what it accepts (`AddResource(type, rest, IS, poisoned, &pos, 0)`,
    vt 0x9C), not the nearest;
  - whatever is left over, if pos is not water, makes a new pile (fn_005FA8B0) with the pile sound fn_0066D1A0 (< 200:
    `77 + t % 6` / `92 + t % 6`; otherwise `75 + (t & 1)` / `86 + t % 6`), `SetPoisoned` and `SetSpeedUp`.
  - **The guidance voice on dropping (pending, fully read).** 0x66F4D8..0x66F509: only if the IS is
    `GGame::MyInterfaceStatus` 0x555880 (in openblack, `dropper.isMyInterface`), with the point and a `RESOURCE_RAIN_TYPE`
    = 1 for food (type 0) and 2 for wood (type 1), 0 for everything else (0x66F4EB..0x66F502). `GGuidance::ResourceDropSFX`
    0x71B570 requires: `GGuidance::PlayNow(1)` 0x71AF50 on the IS's guidance (+0x30) non-zero; a town less than
    100 m away ([0x98013C], `MapCoords::GetNearestTown` 0x6020E0); and `GGuidance::GetResourceDropSample` 0x71B5F0, which adds
    three floats of that town (food +0xC4 + +0x108 + +0x19C; wood +0xC8 + +0x10C + +0x1A0; type 3, the one from
    `DoDeleteObjectAndTakeResource` 0x63A9E6, uses +0xEC + +0x130 + +0x1C4) and above 0.5 ([0x980140]) picks with
    `LocalRand(3)` a HELP_TEXT id among 0x1352/0x1353/0x1354 (food) or 0x1355/0x1356/0x1357 (wood), above
    0.25 ([0x980144]) among 0x135B/0x135C/0x135D (food; wood repeats the same three) and below that nothing. Then
    `GGuidance::PlaySample` 0x71C6F0 (1, the sample, player +0xB5, 1, 0x7F, 0x64, 0x5A, the point, 200 [0x980148], 1).
    Missing: the guidance channel (`Audio/Services/Voices.h`) and those town fields.
  - **The diversion of a store's wood (pending, identified).** `StoragePit::AddResource` 0x732F60, before
    anything else (0x732F67..0x732F99): if its +0x74 is not null and the type is WOOD (1) or ANY (−2), it forwards the whole call to
    the `AddResource` (vt 0x9C) of that object and returns its answer. **+0x74 is `MultiMapFixed::building_site`**, a
    `BuildingSite*` (bw1-decomp `src/Black/MultiMapFixed.h`; StoragePit inherits from Abode, whose own fields start at
    0x7C): a store under construction sends its wood to the building site. openblack has no building sites and nobody builds houses
    (`ECS/Components/Town.h`), so no store can have one and the diversion is unreachable.
  - **The 2D radius of the pile:** `Object::Get2DRadius` 0x638180 = scale × max(+0x24, +0x2C) of the mesh, which
    `LH3DMesh::ComputeBoundingBox` 0x8081B0 fills with the half extent (max − min)/2 of all submeshes;
    `PileFood::Get2DRadius` 0x66F180 multiplies it by `GetProportionRaised` (vt 0x86C). In Land1: MagicFood of 200
    → 1.95 m (accepts at 3.9 m), grows with the pile (accepts at 4.14 m with 218); MagicWood of 500 → 2.04 m (accepts at 4.07 m).
- **Piles** (`Magic/Objects`): MagicFood 0x5FA9F0 (PotInfo 10, MSH_S_GRAIN_PILE, scale 0.3, owner +0xBC) and MagicWood
  0x600E20 (PotInfo 9, MSH_B_WOOD_01, scale 0.7, owner +0xB4); no Process or expiry.
  - `MagicFood` = PileFood(pos, GPotInfo 0xD4D308 = info 10, amount, town, 0, 0, 1.0), `SetScale(0.3)` in `fn_005FAAE0`.
    `MagicWood` = PileResource(pos, GPotInfo 0xD4D1C4 = info 9) → PileWood, `SetScale(0.7)` in `fn_00600EE0`. A NULL
    owner becomes the neutral player (g_game +0x18 + byte g_game[0x205A5B] × 0xA60; wood 0x600E64..0x600E8A).
  - Both piles (type 21, counted as fixed) enter their cell at once, at the tail of its fixed list (MobileObject
    0x607150+0xA9 → `Object::InsertMapObject` 0x636740).
  - `IsAWoodPileOutsideStoragePit` always answers 1 for the magic wood pile (creature AI, not ported).
  - **The shadows of the food pile (identified; already met).** `MagicFood::CallVirtualFunctionsForCreation`
    0x5FAAB0, after the PileFood one 0x66E1A0, calls two setters of the Game3DObject +0x40: vt 0x78(0) at 0x5FAAC8 and
    vt 0x80(0) at 0x5FAAD2. In the LH3DObject vtable 0x9A2974 they are fn_008168A0 (clears bit 0x40 of obj+4: the bit
    checked by the receiver of projected shadows, vt+0x7C fn_007F9870, test 0x80E457) and fn_007F9880 (clears bit
    0x1000): **the magic food pile does not receive projected shadow**. bw1-decomp calls those slots
    `SetCastDynamicShadow` / `SetShadowOnTexture`, but the exe code is the receiving one. Both take the argument in edx (0x5FAAD0 `xor edx,
    edx`). openblack already complies, and in addition leaves it out of all three shadows: `CastsStaticShadow`
    of `RenderingSystem.cpp` rejects any Pot and its `ReceivesDynamicShadow` cites this same call (0x5FAAC8) for
    the MagicFood and HandFood types; `CastsPhysicsShadow` of `Graphics/ShadowList.cpp` also rejects any Pot.
  - **The wood pile does keep them:** `MagicWood::CallVirtualFunctionsForCreation` 0x600F10 is just a call to the
    PileResource one 0x66E300, without those two setters.
- **SF_Food / SF_Wood effect** (`Particles/Rules/Sprinkle.cpp`, `Particles/Creators/Mesh.cpp`):
  - `UR_HandSprinkle` 0x6A0220: a source atom at the gesture position (at most 58 m above the ground); on the first
    step, if this computer's interface is casting, it starts `HandStateGrain` (fn_005B2F70). Velocity
    (0, Δy/dt − 20 if a human casts, 0).
  - The grains are emitted by `UR_WillowWisp` with the spell *enabled*: max(dt × MaxAtoms / DieAge,
    moved / 2.5 m) per step, i.e. **18 per second even if the hand is still** (the plan said a still hand drops
    nothing: that is not so). Each grain that touches land (`LandscapeCollide SendEvent 1`) is a `SpellEvent 3`.
  - `AppearanceRuleTumble` 0x6A6200 (logs) and `ParticleMeshCreator` 0x6A8B00 / `…AnimTextured` 0x6A8DA0 (logs
    MSH_I_OFFERING_WOOD, rain cone): mesh atoms are drawn as instances along with the objects.
  - Fix to the spell file reader: ARRAYs can carry floats (`KeyPoints` of UR_HandSprinkle); before,
    the whole read stopped there and SF_Food, SF_Wood (and any such file) did not load.
- **HandStateGrain** (`HandGrain.cpp`, hand state 8): `fn_005B3000` stores (ClampHand, TotalTime, height,
  angle, loop) and the hand position (+0x1EC); `fn_005B2DA0` on each turn: t += dt / TotalTime, natural cubic
  spline of fn_005B3760 through (0, 0) (0.2, 1) (0.8, 1) (1, 0) (the ctor passes 1e30 at both ends because +0x148 = 1,
  0x5B2CF2; the peak is 1.61); height = v × 10 m, tilt =
  v × 1.07 rad (peak at t = 2 s: 16.1 m and 1.73 rad). With ClampHand (food and wood) `HandStateHolding::Update`
  0x5B3FD4 (vt 0x1C) sets the
  required hand position at the point where it started, and adds the height (vt 0x18). `Spell::CoreCloseDown` 0x720160
  stops it.
  - **The direction of the tilt rotation (verified; faithful).** The angle comes from the file and
    is **positive**: `AngleToRaise 1.07257` (SF_Food.txt line 143, SF_Wood.txt line 114), and fn_005B2DA0 uses it as
    is (0x5B2EF3: height = v × +0x134, tilt = v × +0x138). `ObtainRequiredHandPosition` 0x5B6DE0 builds the normalised
    hand → camera axis (`LH3DTech::g_camera` minus the position, 0x5B6E1B..0x5B6ECE), calls fn_007FB180(matrix,
    axis, angle) and transforms the vector (0, 1, 0). fn_007FB180 writes the **standard** Rodrigues matrix by rows
    (0x7FB1F7 `[ecx]` = x² + (1 − x²)c, 0x7FB207 `[ecx+0xC]` = xy(1 − c) + zs, 0x7FB21C `[ecx+0x18]` =
    xz(1 − c) − ys, …), but the transform at 0x5B6EE8 is by **row vector**: out.x = R00·v.x + R10·v.y + R20·v.z + t.x,
    that is vᵗ·M = Rᵗ·v = R(−angle)·v. That is why `HandPlacement.cpp` rotates with `-tilt`
    (`glm::rotate(mat4(1), -tilt, axis)`).
  - **With ClampHand the hand does not move while the grain falls**, so all grains fall on the same point and
    make a single pile that grows (the plan expected a 20 m line: there is none with `ClampHand 1`, and water, which
    has `ClampHand 0`, does follow the hand). The point is the one the hand had when the stream started, and the loop does not
    take it again.
- **Tests:** `test_food_wood` (spline, loop, costs, sounds, `GetProportionRaised`, the sprinkle emission and the
  reading of KeyPoints; with `OPENBLACK_GAME_PATH` the real rows).

## Water (`Magic/Spells/SpellWater`, `Particles/Creators/Mist`)

MAGIC_TYPE
22 WATER and 23 WATER_PU1, seed 9 WATER. Status: **faithful** except where marked.

### The spell (`SpellWater.cpp`)

- `GMagicWaterInfo::AllocSpell` 0x5FAC70: 0xF4 bytes, vtable 0x8F553C; fn_00724EC0 sets to 0 +0xEC (age of the last
  ring) and +0xF0 (the "putting out a fire" reaction): `water::SpellWaterData`. **The vtable only overrides `Process`**
  (vt 0x528); the rest is the normal `Spell` (InitWithPos, SpellEvent, CloseDown...).
- Getters (`info+0x10 − 0x16`): rain radius fn_005FACE0 = **6** (WATER) / **12** (PU) / 1; ring growth
  fn_005FACC0 = **2** / **4** / 1; `GetRippleEvery` 0x5FAD00 = **0.1**.
- Constants: rain radius 6 / 12 at 0x92BFA8 / 0x92BFAC; ring growth 2 / 4 at 0x92BFA0 / 0x92BFA4; the reach factor 2.5
  at 0x8C581C (0x72510E).
- `SpellWater::Process` 0x724ED0, **one drop per turn** (checked instruction by instruction):
  1. `Spell::Process` 0x720710 and its result, which is always returned; if the reaction +0xF0 is no longer available, to 0;
     if the spell is closed, nothing more.
  2. `r = GameFloatRand(R) × 0.7 + 0.3` (0.3 … 0.7·R + 0.3 m: 4.5 m with WATER, 8.7 m with PU) and
     `a = GameFloatRand(2π)`; the drop P = cast position (+0xCC, the one that follows the hand in a cast with
     the hand) + (r cos a, r sin a), with `y = GetAltitude(P) + 0.2`.
  3. `SpellEvent{2, P, movement 0, strength 1, 0, no target}` (vt 0x52C → `ApplyDefaultSpellEffect`): the EffectValues of
     WATER (**burn −4000**, radius 1 m) × strength × tribal power, `costPerEvent` 10, reaction 21. Its result is not
     checked: the drop and the ring come out even if there are no chants left.
  4. The 9 cells of 10 m from the one of P (`GUtils::Spiral`, direction 1, count 1), each with its fixed list and then
     the mobile one (fn_00603500 / fn_007252D0): each object with `2.5 × GetPower > dist2D(obj, P) − GetRadius` (strict)
     receives `ApplyWaterSpell` (vt 0x67C). `GetPower` of a spell is that of `GameThingWithPos` 0x56FE60 = 1, so the
     reach is **2.5 m from the edge**. `GetRadius` (vt 0x60) is `Get2DRadius` (Object 0x638110 jumps
     to vt 0x64): **5 m for a field** (Field 0x528E80), for the rest the half extent of the mesh × scale.
  5. A ring when `0.1 < age − last` **in single precision** (fld/fsub/fstp dword; with the age adding 0.1 per
     turn it comes out on almost every turn, not on all: not on the first turn, 0.1 − 0 is not greater than 0.1). Colour
     `{0xFF80CBC5, 0xFF8599C5, 0xFFBA97B2, 0xFFB9CA86, 0xFFBD9C8A}[GameRand(5)]`, angle `GameFloatRand(2π)`, in the
     first free slot of the 1024 at 0xEAB7C8: +0 P, +0x0C flag 1, +0x10 age 0, +0x18 growth 2/4, +0x20 angle,
     +0x24 1, +0x28 aspect 1, +0x2C rate 1, +0x30 cell 0x30, +0x34 colour. **The rings fall on land**, not only on
     water (`ecs::AddWaterRing`, from `AddDropRing` in `SpellWater.cpp`; the colour is one of those
     constants, not the light table, and the ring keeps it all its life). When the 1024 ring slots are full, no ring is
     made (0x725236).
- `ApplyWaterSpell` per class (the symbol list only has these three):
  - **Object** 0x63A8E0: if the object is burning (`IsOnFire`) and +0xF0 is empty, `+0xF0 = CreateReaction(spell, 34
    REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE, the spell's player, 1)`. Returns 0.
  - **Tree** 0x74C390: the Object part and then `ecs::ApplyWaterSpell(tree, magicType == 23)` (grow,
    with PU go past the maximum, or a forest sprout every > 40 turns). With sprout and player:
    `GPlayer::FUN_0064DA80(0xE, 1)` —**only does something in multiplayer** (`IsMultiplayerGame` 0x552F80), here nothing— and
    `GAlignment::Update(player, new tree, 1)` 0x4145A0 = `alignment::UpdateForTree(player, true)`. Returns 1.
  - **Field** 0x528F30 (`ecs::ApplyWaterSpellToField` in `ECS/Fields.cpp`): the Object part; with player,
    `ConsiderMakingCreatureMimicPlayer(action 0x21, magic 0x16)` (the creature, not ported); if it is not burning: with `crops ≤ 30`
    (`timesToSow`) it jumps to **31** at once (sown); otherwise, while `growth ≤ 1200`, `growth += 2`
    (`effectOfWaterSpell`, info.dat, the same in the 6 rows) and `food += 2 × 350 / 1200`. `IsUnripe` is called and
    discarded.
- **Putting out fires**: done by part 3 (burn −4000 in 1 m): `ApplyEffectToFireEffectIfNecessary` takes the
  temperature towards ambient − 4000 with the cap `max(10·ΔT/capacity, ΔT)`, so **a single drop puts out** a tree
  burning at 500 (Tc 110, capacity 100). Since the event comes before `ApplyWaterSpell`, the object is no longer burning when
  the water reaches it, and reaction 34 is only born if the drop did not reach it with the 1 m radius but did with the 2.5 m. The
  storms (fn_0072DCC0) are in the storm section.
- The original's mist for the `SPELL_AT_POS` is at the height of "from" (the hook puts it 30 m above the target,
  inferred); with the hand, at the hand, which `UR_HandSprinkle` raises 8 m in 8 s without holding it (`ClampHand 0`).

The water mist (`ParticleMistCreator`, also the one of the storm clouds) is in
[`ParticleMistCreator`](particles.md#particlemistcreator-particlescreatorsmistcpp).

### Fix in `ECS/Effects`: the map cells

**Replaced:** `ApplyEffectToMapPos`, water, fire, lightning, explosion,
storm, healing and pots now walk the ordered lists of `ecs::map_cells`
([engine-math.md](engine-math.md#object-lists-per-cell-ecsmap_cells)): the fixed one from the head and then the mobile one,
**without "already seen"** (`ApplyEffectToMapPos` 0x525274..0x5253BF, water 0x7250CC..0x725179 and fire heat
0x72F699..0x72F6B6 with `HeatTransfer` 0x72F980 have none: a multi-cell object receives the effect once per cell).
The two functions below were removed on 2026-10-07: TownQueries had moved to the map cells and nothing called them. Previous text:

`effects::FixedObjectsInMapCell` / `ObjectsInMapCell` (EffectValues.h). **(approximate)** The openblack grid
(`MapProduction`) only puts a fixed object in the cells whose centre is less than its radius + 1 m away, so a small
tree far from the centre of its cell was in none, and neither `ApplyEffectToMapPos` (fire, lightning, water) nor water
saw it (the burning bush next to the Land1 store was not put out). The original links each object in the cell of its
position: now the fixed list of a cell is the grid's plus the fixed objects whose position falls in it, ordered by
entity. `ApplyEffectToMapPos` uses it for its fixed part.

### Tests and screenshots

- `test_water`: the getters, the drop distance and the reach (strict, 5 m for a field), the rings in single
  precision, the mist colour, the `UR_HandSprinkle` cloud with its mist and, with `OPENBLACK_GAME_PATH`, the rows of
  info.dat (burn −4000, radius 1, `costPerEvent` 10, 6 / 10 s, reaction 21, particles 16/17, cost 5000/7000),
  `effectOfWaterSpell` = 2 in the 6 field rows and SF_Water / SF_WaterPU1 executed (cloud + cone).
- Hook `OPENBLACK_TEST_WATER_SHOT` ([openblack-internals.md](openblack-internals.md#debug-environment-variables)).

### Not ported / pending (water)

- The creature's mimicry and `FUN_0064DA80` (multiplayer).
- SF_WaterInHand / SF_WaterOnHolder use the same mist, which is now drawn; they have not been reviewed in a screenshot.
- The 2D distance uses the exact root: the original goes through `hypotenuse` 0x74F680 with an inverse root on MapCoords
  16.16 (sub-millimetre differences, approximate).

## Heal (`Magic/Spells/SpellHeal.cpp`, `Particles/Rules/Heal.cpp`)

MAGIC_TYPE 10 HEAL and 11 HEAL_PU_ONE, seed 7 HEAL. The spell does not
heal anything by itself: **it looks for the targets and the PSys heals them one by one** with an event of type 5 per chakra.

### Finding targets and casting

- `GMagicHealInfo::FindTargets` 0x5FBB00 (already in `Magic/CastRules.cpp`): radius `dummyVar` 10 (35 with PU) and cap
  `maxToHeal` 20 (100), both × the tribal power if there is a spell; spiral of `ceil(2R/10)²` cells; each living thing that accepts
  the effect and `CanBeHealedByHealSpell` within R becomes a target of the PSys (`AddTarget_`, vt 0x114). **It does not check whether
  it is missing life**: a healthy one also receives its chakra (checked in the game: villagers with life 1 light up).
- `SpellHeal::InitWithPos` 0x72D870 = `Spell::InitWithPos` and, **if it returns 1**, `FindTargets(pos, this)`; it does not check
  how many it found. The hand check is `GMagicHealInfo_vfunc12` 0x5FBD20 (vt 0x30): only with MAGIC_TYPE 10 or
  11 and `FindTargets(pos, NULL) > 0`.
- `SpellTargets::TakeTargetObject` 0x671030 takes out **the last one** of the list (`psys::Effect::TakeTarget`), so the
  PSys consumes the targets in the reverse order to the one it found them.

### `UR_HealSpellChakra` 0x6A0B20 (`Particles/Rules/Heal.cpp`)

Constructor 0x6A0810 (sets the modifier's flags 6 and clears 4: **it is not a creator, but it keeps the effect alive
until it closes**; in openblack it is `Modifier::KeepsAlive`), properties 0x6B1E40. Each turn:

1. Takes targets out of `SpellTargets` while there are any. Those that already have a chakra (global list 0xD4ED38, `g_Chakraed`) or
   no longer exist are discarded; the rest:
   - **sends `SpellEvent{type 5, the target's centre, strength 1, target}`**, which is what really heals
     (`ApplyDefaultSpellEffect` → `ApplyEffect(EV, 0)` → `IncreaseLife`, and `SetPoisoned(0)` if it was poisoned);
   - creates a point atom with its `AtomData` (0x38 bytes, ctor fn_006A0920): the target, its `Get2DRadius` (+0x30) and
     its `GetHeight` (+0x34), and puts it at the start of the global list (fn_006A0A60);
   - the position is fn_006A0AB0: the target's point with the terrain height and, with `TakeCentrePos`, **half the height
     of the object** higher up.
2. Walks its atoms: the **first** one in the list, as soon as it has age > 0, plays `SoundHeal` (SOUND_SPELL_HEAL, only
   once per atom; `SoundSpacing` 0.2 is read and not used). The original's list grows **at the head**
   (`AtomCollection+0x40`; fn_00674BD0, the `mov [ecx+0x40], eax` at 0x674BEE, called from
   `AtomCollection::CommonInitNewAtom` 0x674C7A), so that first atom is the **newest**; the openblack vector
   grows at the end (`PSys.cpp`, `NewAtom`), so the sound goes on the last one (a freshly made atom has age 0, so in both cases the sound waits for the next step). Each
   chakra follows its target and, with `ScalePropObjectSize`, its rule scale (+0x78) is the target's `Get2DRadius`.
3. `fn_006A0E30` gives the chakra's intensity from the age of its **subcollection** (the burst of the next group):
   `t = age / AtomAgeMaxAlpha` up to 1 and then `1 − (age − AtomAgeMaxAlpha) / (AtomAgeZeroAlpha − AtomAgeMaxAlpha)`,
   clamped to 0..1 (`psys::heal::ChakraFade`). With it:
   - the burst atoms receive alpha `int(t × MaxAlpha)` (100.46 → 100 at the peak);
   - the target receives `SetSpecularColor(int(t × SpecularColor))` (`Living` 0x417480, +0xD0), that is the glow
     (200, 255, 255) × t: **the villager lights up** while the chakra lasts.
   When the burst runs out of atoms (after 3 s) the chakra is deleted, and when its `AtomData` is destroyed (fn_006A09A0)
   it leaves the global list and removes the glow from the target (`SetSpecularColor(0)`).
   `SetSpecularColor` stores the whole dword (0x417484) and the fade always sets the alpha to 0xFF (0x6A0EF5), so
   even if the RGB reaches 0 the dword is not 0: the draws, which test the whole dword (fn_0051B3D0 0x51B416,
   `Animal::Draw` 0x51C4D6), keep seeing an own specular (which wins over the poison tint) until the
   `SetSpecularColor(0)` of the destructor (0x6A0A28). openblack: `components::SpecularColour` stays with RGB 0 and only
   the destructor removes it (`Heal.cpp` `ClearSpecularColour`).
- A chakra whose target disappears is deleted (0x6A0D7D: in addition the atom forgets the target); if a chakra has no
  subcollection the rule is released (0x6A0E1C returns 0).
- Bit 4 of `Object+0x24` (0x6A0D8B) also ends the chakra, **without** forgetting the target, so the destructor of the
  `AtomData` removes its glow. That bit is identified: it is `GameThingWithPos::Flags` 1 << 2, the
  `UNAVAILABLE_FOR_STATE_CHANGE` of `bw1-decomp/src/Black/GameThingWithPos.h` (only read inverted, in
  `IsAvailableForStateChange`), and the only place that sets it is `GInterface::PlaceObjectInMagicHand` (0x5DA7C1 →
  fn_005DC330 → fn_005DC2A0 → fn_005FAFC0, the `or byte [esi+0x24], 4` at 0x5FB014): **it is the villager picked up by the hand**.
  openblack does not have that flag, so it uses the object the hand is carrying (`HandSystem::GetHeldObject`) **(approximate)**.
- The specular glow is drawn in `RenderingSystem` / `vs_object`: `components::SpecularColour` goes in the y of the fifth
  column of the instance (`argb_colour::PackInstanceSpecular`, 8 bits per channel, [rendering-objects.md](rendering-objects.md#the-object-colour-fields-in-the-instance)) and is added to the specular of the terrain light, as
  fn_0080BF10 does from fn_0080BEC0 (`Villager::Draw` fn_0051B3D0, `Animal::Draw` 0x51C4D6).

### `CreateRuleFusedSphericalExplode` 0x69F610

The burst of each chakra (5 sprites from `S_SpriteSheet1`, FileOffset 57, 7 frames, scale 5, additive). When the
collection reaches `FuseTime` (0 in the chakra) it makes `NumAtoms` atoms at once: direction from `PSysRandR3` (rejecting the
null vector), normalised, with `|y|` if `OnlyHemisphere`; **a single speed** for the whole burst,
`MinSpeed + rand(MaxSpeed − MinSpeed)` (0.6 in the chakra), with the y × `ScaleYSpeed`. Only the first atom plays
`SoundExplode`; with `DisableParent` (1 by default) the parent atom stops being drawn (flag 0x10). Afterwards the rule is
released. Constructor default values: NumAtoms 100, FuseTime 20, ScaleYSpeed 2, DisableParent 1.

### `UR_HealInHand` 0x6A0F40

The hand wave (SF_HealChakraInHand and SF_HealChakraOnHolder, `WiggleFreq` 0.6; 1 by default): each atom is placed
at `GetCurrentParentPos × sin(collection age × WiggleFreq × 2π)`, and the odd atoms with the opposite sign. It is
literally a multiplication of the parent's position, not an offset.

### Life and poison

- `Object::IncreaseLife` 0x637870 (`Villager::IncreaseLife` 0x753460 just calls it): if life + amount > 1, the amount
  is clipped to `1 − life`; with amount ≤ 0 it touches nothing; otherwise, `SetLife(life + amount)`. Returns the new life. The
  heal effect is `Object::GetHealEffect` 0x637D80 = `EV.heal × defenceMultiplierHeal` (there is no other
  clipping than the 1).
- With heal effect 1 (the HEAL row of info.dat), a villager at 0.3 life goes to 1 at once.
#### Poison (`ECS/Life.h`, `ecs::life`)

- Poisoned: bit 1 of `Living+0xB4` (`Living::IsPoisoned` 0x416F90 vt 0x4A4 / `SetPoisoned` 0x416FA0 vt 0x69C; the
  `Object::SetPoisoned` 0x402780 of pots is another flag, `components::Pot::poisoned`). In openblack it is
  `components::Poisoned`, with `ecs::life::IsPoisoned` / `SetPoisoned`, and `ApplyDefaultSpellEffect` (0x720E34) removes it
  with HEAL or HEAL_PU_ONE.
- **Who poisons**: there is no spell that poisons. The two calls to `SetPoisoned(1)` on a living thing are
  `Villager::AddResource` 0x7564D0 (0x7564E5..0x7564F3: food with the "poisoned" flag, e.g. a poisoned pile
  put into their hands) and `Villager::GetResourceFrom` 0x753390 (0x7533E8..0x7533FC: taking a resource from an object
  whose `IsPoisoned` is 1 — `Pot::IsPoisoned` 0x55D4E0, `StoragePit::IsPoisonedResource` 0x733550 —, which also passes on the
  `IsSpeedUp`). That is, **one is poisoned on receiving the food, not on eating it**. Eating only changes the animation:
  `Villager::EatFood` 0x75C00E and `EatFoodAtHome` 0x75C0BE pick 0xD4 instead of 0xA3 / 0x26 if poisoned. In
  openblack the eating side belongs to the villagers: the API is `ecs::life::TakePoisonedResource`.
- **What the poison does to it**: `Villager::CheckHungry` 0x75BCC0 applies the hunger damage also when the villager
  is **not** hungry, just for being poisoned (0x75BD83..0x75BD9E). The amount (0x75BDA6..0x75BDE0) is written
  `max(1 − food / hungryForFood, 1) × hungerToLifeMultiplier`, with the `max` read at 0x75BDBB..0x75BDCA (`fcom 1.0`,
  `test ah,0x41`, `je` keeps the value only if it is **greater** than 1): since food is never negative
  (0x75BD59..0x75BD6E clips it to 0), the first term never wins and **the damage is a fixed `hungerToLifeMultiplier`** per
  periodic check. Ported as `ecs::life::ProcessPoison`, called from `CheckHungry` (the rest of
  `CheckHungry` is still pending in the villagers). In addition `Villager::DoSleeping` 0x760DB6 **skips the
  life recovery while sleeping** while poisoned (that state is not ported).
- **The tint**: fn_0051B3D0 (the drawing helper of `Living::Draw` 0x51AEEC and `Villager::Draw` 0x51BA74/0x51BAD5), at
  0x51B43D..0x51B45D: if the living thing does **not** have its own specular colour (`Living+0xD0`, the chakra's, which wins) and is
  poisoned, it is drawn with diffuse 0xFFE8FFDD (0x51BB50, which the symbol file calls `Pot::GetPoisonSpecular`
  but goes in the diffuse slot of fn_0080BEC0, the same one the white 0xFFFFFFFF uses) and specular 0xFF001000
  (0x51BB60, which the symbol file calls `Object::GetFireEffect`); fn_0080BF10 **adds** the specular to the
  object's colour channel by channel with saturation (0x80BF2D..0x80BF68). In openblack they are the data `ecs::life::k_PoisonDiffuse` and
  `k_PoisonSpecular`: **drawing it belongs to the shaders** (`LH3DColor`), not visible yet.
  The `Pot::GetPoisonColor` 0x80BEC0 of the symbol file is not a colour: it is the "paint with these two colours and
  `AddForDrawing`" of the 3D object.

### Not ported / not verified (heal)

- **The mist (`ParticleMistCreator`) is not needed for healing**: SF_HealChakra only uses `ParticlePointCreator` and
  `ParticleSpriteCreator`. It remains for water and weather.
- The power-up (SF_HealChakraPU) adds the mesh `MSH_S_HEAL_MESH` (532, the mushroom) with `UR_KPStretchHeight` 0x6A50C0 and
  `UR_KPMoveAtoms` 0x6A60B0 (keypoint interpolation `KPSplineInterpolator::EvalAtT` 0x6A7EB0, a cubic spline
  with the factor 1/6; `MovePropAtomIndex` scales the displacement by `index / (NumAtoms − 1)`) and the sound
  HEAL_MUSHROOM. **It is already ported**:
  the two rules are registered in `Particles/Rules/KeyPoints.cpp`, the creator `ParticleMeshCreatorAnimTextured` is in
  `Particles/Creators/Mesh.cpp` and the `SoundOfCreate` in `Particles/PSys.cpp`. The chakra works the same in the PU (radius 35, up to
  100 targets).
- Mesh 532 is type 4 with byte +5 = 5 (two-sided) and 192 of its 512 triangles face inwards, but the original
  **does not change its material**: `ParticleMeshCreatorAnimTextured::CreateLH3DObject` 0x6A8D20 only does
  `LH3DObject::Create` and six calls from its vtable, none to `GJUtils::SetMaterialProperties` 0x57E220 (the change
  4 → 13 of the physical shield). openblack already reads the two-sided flag from the mesh itself (`L3DSubMesh.h`, byte +5 bit 0),
  so there is nothing to fix here.
- `CanBeHealedByHealSpell` (vt 0xB18) is `!IsDead` in `Living` (0x5EE550 → vt 0xAF4, `Living::IsDead` 0x417270) and gives 0
  for **the whole `Dove` class** (`Dove::CanBeHealedByHealSpell` 0x41EAB0 is an `xor eax,eax`), not only for the spell's
  dove: no bird is healed (crow, dove, swallow, pigeon, seagull, bat, the two spell ones and
  also the **vulture**, `class Vulture : public Dove`, whose vtable 0x8BB7F8 +0xB18 is 0x41EAB0).
  Ported in `CastRules.cpp` with `ecs::animal_ai::IsFlyingSpecies` + the vulture separately (IsFlyingSpecies does not
  include it). CitadelDove / CitadelBat: unidentified class, openblack does not create them (inferred: they are healed). The creature gives
  1.
- It is still inferred that the dead are not healed: openblack has no `IsDead` (the dying states are not ported), so
  a living villager or animal always passes the filter.

### Hooks, tests and screenshots

- `OPENBLACK_TEST_HURT_VILLAGERS="x,z,radius,life[,poisoned[,turn[,heal[,repeat]]]]"`
  ([openblack-internals.md](openblack-internals.md#debug-environment-variables)): hurts the villagers in the radius,
  can cast the miracle right there and writes every change of life, poison and glow.
- `test_heal`: the chakra curve (`ChakraFade`, the alpha 100 at the peak and the glow 160 at 1.2 s), that the rule keeps
  the effect alive without atoms until the `CloseDown`, the fuse and the single speed of the burst, the hand wave, the
  two colours of the poison tint with its damage per check (`Heal.poisonData`) and, with `OPENBLACK_GAME_PATH`, the
  real values of SF_HealChakra and SF_HealChakraInHand.

## Forest (`Magic/Spells/SpellForest`, `Magic/Objects/MagicTree`, `ECS/Trees`)

What follows has been read in the exe (W120). The tree goddess scene (camera take) is postponed.

- **SpellForest** (MAGIC_TYPE 13 NATURE; `GMagicForestInfo::AllocSpell` 0x5FAD90, 0xF8 bytes): +0xEC the Forest,
  +0xF0 "forest created" (fn_007254F0 sets it to 0), +0xF4 the maximum number of trees (`SetMaxObjectsToCreate` 0x7256C0: -1 →
  `finalNoTrees` 18). `InitWithPos` 0x725540 is Spell's.
  - `SpellForest::GetMagicInfo` 0x725A20. The radius 11 is `SpellForest::GetForestRadius` 0x725560 (float 0x9819F4),
    and `fn_00725570` reads 0x9819F8.
  - `CanCast` 0x5FAE80 (vt 0x30): inside the map, land, `fn_005FADF0` (no Abode of **the cell** of the point,
    `FindType(0)`, has `Get2DRadius > distance` to its fire centre) and `ValidPlaceForTree` 0x725C50 (inside,
    land and not `MapCoords::IsFixed`).
  - The radius of `fn_005FADF0` is each object's vt 0x64, and **`Field::Get2DRadius` 0x528E80 is the constant 5 m**
    ([0x8AB6E4]): it is the only class that overrides the slot (the vtables of Object, Abode, Field, Tree and Pot give
    `Object::Get2DRadius` 0x638180 except Field's). But `FindType(0)` (`FindTypeOnMap` 0x6015E0) compares the type of
    the info (+0x10) with 0 ABODE, and that of a field is 18 FIELD (info.dat `fieldType[].type`, checked by
    `test_map_cells`): **a field never prevents the forest**. `NoAbodeCovers` now walks
    `ecs::map_cells::FindType(cell, ABODE, previous)` and no longer looks at fields (before it counted them with 5 m).
  - **`IsFixed` 0x603790 → `MapCell::IsFixed` 0x601EA0 only looks at the first fixed object of the cell** (MapCell +4,
    where `Fixed::InsertMapObjectToCell` 0x52DEA0 puts the newest with `SetFirstObjectFixed`) and its bit +0x24 & 2, which
    only the ctor of `MultiMapFixed` 0x52E1F0 sets (`or byte [esi+0x24], 2` at 0x52E207). That is: `IsFixed` = "the newest
    fixed object of the cell is a MultiMapFixed". The classes that inherit from it, according to bw1-decomp (`src/Black/*.h`): Abode (and
    with it Field and StoragePit), BigForest, CitadelPart (CitadelHeart, WorshipSite, CreaturePen, WorshipTotem), Feature
    (and AnimatedStatic), FishFarm,
    MobileStatic (and with it MagicTeleport and the street lamps), PFootball, PrayerSite, SpellIcon and TotemStatue; `SingleMapFixed`
    (Tree, MapShield, ScriptHighlight, PrayerIcon) does not set it, nor do GFootpath or BuildingSite (they are GameThing). A tree is alone in its cell: a cell whose last fixed object
    is a tree is not "occupied", even if it has a building underneath. openblack now tests that list of components
    (`ecs::fire::traits::IsMultiCellStatic`, was `IsMultiMapFixed`, the common trait; before it only asked "is not a tree", which occupied the cell with any
    SingleMapFixed). `IsFixedCell` is `ecs::map_cells::IsFixed`: the real head of
    the ordered fixed list ([engine-math.md](engine-math.md#object-lists-per-cell-ecsmap_cells)); each tree of the
    event enters at the head when created (hook `InsertMapObject` in `CreateTree`, SingleMapFixed 0x52E620), so
    `g_NewTrees` has been deleted. **(approximate)** what the other owners without a hook create or move (a tree replanted
    with the hand) enters in the next turn's `Sync`, in creation order.
  - `SpellEvent` 0x725830: nothing with type 1 or if there is already a Forest; `ApplyDefaultSpellEffect` (pays costPerEvent 1;
    EffectValues of NATURE: alignment 1; reaction 21) and, if it applies, **the whole forest at once**: N = `fn_00725790` =
    round(+0xF4 × (strength > 0)); step = N > 1 ? 1/(N − 1) : 1; turns = N × **17/13** (the float 0x9819FC =
    1.3076923) × 2π; for i < N: f = i·step, r = 2 + 9·√(1 − (1 − f)²), a = f·turns, point = castPos (+0xC0) +
    (r cos a, r sin a) truncated to MapCoords (ftol × 6553.6); if `ValidPlaceForTree` and the forest has fewer than N
    (fn_00725800), `fn_00725600(point, GetRandomTreeInfo(castPos))`.
  - `GetRandomTreeInfo` 0x725580: `Terrain::GetMaterialInfo` 0x735330 of the cell of the **cast point** (not of
    each tree) and `magicTreeTypes[GameRand(4)]` (one GameRand per tree). `GetTerrainMaterial` 0x735380: 27 (snow) if
    `GClimate::GetSnow ≥ 27` at the corner of the cell; otherwise, the type of the second material of the cell's altitude in
    its country (fn_00804CF0 → fn_00873770, like `GSoundMap`); **a 0 (off the map or without a block) becomes 1**, which is
    DEEP_WATER: palm trees; `> 43` → 0. Grass (18) and almost all: beech, birch, cedar, cedar; sand and water: palm trees;
    snow and ice: conifers.
  - `fn_00725600`: with the first tree it creates the Forest (`new 0x58` → fn_005399E0(pos, creator): the creator's
    player, id = counter 0xBEA238++, at the start of the list g_game +0x205BB4) and marks +0xF0. Then
    `fn_005FD000(pos, spell, info, forest, GameFloatRand(2π), 0, woodValueMultiplier 0.25 × tribal power)` and the
    target scale +0x64 = `fn_007255C0` = **1 − 0.5 × distance/11**: 0.91 for the first (at 2 m) and 0.5 at the edge (the
    plan said 1.0 at the centre: there is no tree at the centre).
  - `Process` 0x7259C0 = `CoreProcess` + `ProcessTrees` 0x725A30 (its argument is not used): if the forest was deleted (+0xA
    **bit 0**, the ToBeDeleted one; not bit 1) → +0xEC = 0 and CloseDown; without a forest it returns 1 while there is a
    PSys and 5 (the spell is deleted) when there is not; with a forest it compares N with the trees (+0x4C + +0x54, unsigned):
    N < trees → `fn_0053A490`: **all** shrink `decaySpeed` 0.05 (fn_0074A3A0: scale − d; ≤ 0 → ToBeDeleted);
    otherwise → `fn_0053A520`: **all** `Tree::Grow(growSpeed 0.01, 0, 0)`. The number both receive is not used.
    Returns 1. The forest is reached through wrappers: shrink `fn_00725B00` → `fn_0053A490`, grow `fn_00725AD0` →
    `fn_0053A520`.
  - `CalculateCostToMaintain` 0x7259E0 = costPerGameTurn (5) + trees × costPerEvent (1): 23 with 18.
  - `CloseDown` 0x55D1E0 = CoreCloseDown + creator = NULL. **The trees do not stay**: without a
    creator `GetSpellStrength` 0x720750 gives 0, so N = 0 and the whole forest shrinks 0.05 per turn until it disappears;
    with the last tree the Forest is deleted and on the next turn the spell (closed spells keep being
    processed: `ProcessSpells` only skips those that are not `IsAvailable` 0x401810, i.e. the deleted ones). A forest
    of a player (`timerWhenPlayerCasting` -1) lasts as long as it is paid for; that of a one-off miracle (120 s) or that of a
    script with a duration goes away when it closes. Real trace (duration 8 s): closes at turn 81, shrinks from ~0.4 and at
    88 none is left; the spell is deleted at 89.
  - `ToBeDeleted` 0x725500: deletes the forest (if it is not already being deleted) with its trees.
  - `GetMaxObjectsToCreate` 0x7256F0 = min(+0xF4, forest ? its trees : (+0xF0 ? 0 : 18)); stored by the seed
    (`StoreChantsAndAgeFromSpell`, vt 0x550: `SpellOps::maxObjectsToCreate`). `HasEnoughChantsAndLifeForRecast`
    0x725730 = that > 0. `AdjustSpellSeedPos` 0x725750 (vt 0x540): altitude = max(altitude, the tallest tree,
    fn_0053A740 with vt 0x42C `GetHeight`), −5 without a forest (+0xEC == 0). **It does have a caller** (what was said before was false):
    the seed drawing 0x729020, `call [edx+0x540]` at **0x72906E** (see the hand seed, below). It is the only
    class that overrides it: the pointer 0x725750 appears only once in the image (vtable 0x8F4FE4 + 0x540) and the other twelve
    spell vtables have `Spell::AdjustSpellSeedPos` 0x55CE60 (`ret 4`). openblack:
    `spell_forest::AdjustSpellSeedPos`.
- **The hand seed over the forest (faithful).** There are two seeds: the `Seed.L3D` atom of the
  PSys (falls spinning and is deleted on touching land, see below) and the `I_Forest` SpellSeed of the hand, which is the one that
  stays and is picked up.
  - On casting (HAND_POSITION) the SpellSeed leaves the hand (0x16) but stays tied to the spell (`deleteSeedOnceCast` 0)
    and to the icon (+0x5C: only deleted by `ClearSpellIconLink` 0x7281D0 from `ToBeDeleted` 0x728280).
  - Each frame `GGame::Process3dEngine` 0x54E023 → `Spell::DrawSpells` 0x7203F0 → `Spell::Draw` 0x720430 (vt
    0x50C) → `DrawSpellSeed` 0x721360 (vt 0x508; with +0xAC it jumps to) **0x729020**: if `fn_00728FC0` (not on the map, not
    IN_HAND nor stored in the hand, `seedFollowsSpell` +0x120, spell open, **icon** +0x5C) and +0x60, altitude =
    `fn_006022C0(seed, 1)` (the top `GetTopPos` 0x638160 = altitude above the ground +0x1C + `GetHeight` of the
    objects of its cell, list +4 and then +0, which are not living and do not move and whose circle overlaps: d² < r_obj² +
    r_seed², 0 if none), vt 0x540, and `LHMatrix::Translation` to (x, ground + altitude, z), +0x44 = 1, +0x48 = 0 and
    `AddForDrawing(seed)` 0x63B5D0, which sends the draw collision: the hand sees it.
  - **The ground seed does not spin.** `LHMatrix::Translation` 0x403530 rewrites the
    whole matrix: identity rows (0x403532..0x403558) and then the point. +0x44 and +0x48 are the `scale` and the `y_angle`
    of LH3DObject (bw1-decomp `LH3DObject.h`; `Game3DObject::SetPosition` 0x63B740 writes them the same way), not "alpha and
    flags". So every frame the `I_Forest` is drawn upright, without rotating and at scale 1, whatever rotation it
    brought from the hand. openblack did it wrong (it kept the hand's rotation): `DrawFromSpell` now sets the
    identity rotation and scale 1 (the four seeds that get here, STORM, NATURE, SHIELD and PHYSICAL_SHIELD, have scale
    1 in `GSpellSeedInfo`). Nothing else moves that seed: `SpellSeed::Draw` 0x518710 is a `ret`; `DrawOutOfMap`
    0x5190A0 only draws it in the hand.
  - **What falls and spins is the atom** `Seed.L3D` of the PSys (`SF_Forest`, group 0): born 9.435 m above the point,
    `UpdateRuleGravity_Seed` (gravity 1.6, `MaxSpeed` 3.34513, undamped: about 4 s of fall) and
    `UpdateRuleRotatePrincipalAxis_Seed` (`AngularVel` 12.2611 rad/s ≈ 1.95 turns/s around its Y,
    `AxisChosen` 1 → 0x6A1218; the angle is dt × AngularVel, `fld [0xD4E0EC]; fmul [+0x24]`, without tying it to anything else).
    0x6A1150 also rotates the fourth row of the atom's matrix (+0x68), but `SetRotationMatrix` 0x674120 leaves it at 0
    and the atom's position is +0x80: there is no orbit, the "spiral" is the mesh's off-centre wing. Its only event is
    `LandscapeCollide_Seed` (type 3): the trees all come out on touching land, not while it falls.
  - **User's recollection:** "the forest seed falls spinning at the same speed at which the
    trees grow; from the temple it stays on the ground while the forest lasts and can be picked up and cast again; from an
    orb it neither stays nor can be picked up". What the exe gives: the spin and the fall belong to the atom (before there are trees); what
    goes "at the speed of the trees" is the `I_Forest`, whose height is that of the tallest tree (fn_0053A740) and that is why it
    **rises** exactly at the pace of the growth (0.01 of scale per turn × rain × alignment). Nothing was found
    that makes it go down while the forest lives (when the spell closes, `fn_00728FC0` fails and it stops being drawn).
    The rest of the recollection matches.
  - The ground is not counted twice: the altitude of a MapCoords is above the ground
    (`GetLHPoint` 0x605C40 = `GetAltitude` + y), so `GetTopPos` is too.
  - Result: −5 m (underground) while the atom falls; when the forest comes out, on the ground at the centre; then it rises with the
    tallest tree (trace `seed … drawn over spell …`, altitude −5 → 0.18 → … → 16 m).
  - Picking up: `SpellSeed::ValidForPlaceInHand` 0x728580 (neutral with g_game +0x14 & 0x2000, or of the hand's player)
    → 225 ms grab `GenericPickup` 0x5D2800 (inside the influence, vt 0x714 = 1) → `PlaceObjectInMagicHand`
    0x5DA6F0 → `InterfaceSetInMagicHand` 0x728810 (`StoreChantsAndAgeFromSpell` 0x728780, `ClearSpellLink` 0x728200:
    the spell closes and the forest shrinks 0.05 per turn). With stored trees it can be cast somewhere else.
  - openblack: `magic::seed::DrawSpells` (`Magic/Core/SpellSeed.cpp`, from `magic::Update`), `FollowsSpell`,
    `ValidForPlaceInHand`; the hand: `HandSystem::FindObjectUnderHand` (`HandPlacement.cpp`) and `PickUpSeedOrStone`
    (`HandApplyToObject.cpp`).
  - **(approximate):** `IsMoving` vt 0x174 of a fixed object is taken as "is in physics" (openblack does not store the
    previous turn's position); the cell lists are those of `ecs::map_cells`.
    +0x44 / +0x48 of the Game3DObject are the drawing scale and the `y_angle` (corrected; before "alpha and flags"); the
    scale of the Object (+0x50, `GetScale` 0x402520) is another one.
  - Not ported in `Spell::DrawSpells`: fn_0064AF20 (per player and neutral, the six slots +0x34 of player +0xA48 with
    fn_0077B3B0; unidentified), fn_00682950 (the invisible collision of the fireballs, list g_game +0x205C9C,
    fn_00682F30) and the vt 0x108(1) of the object +0xB0 in `Spell::Draw`. fn_00725FE0 is a `ret`; fn_0072BF50 are the
    shields (`map_shield::DrawShields`).
  - Only with an icon seed: that of a one-off orb or `OPENBLACK_TEST_SEED` (without icon) is neither drawn nor
    picked up, like the original. `OPENBLACK_TEST_SPELL` (script) does not create a seed.
  - **Temple versus orb.** There is no flag of the orb's own: `seedFollowsSpell` (+0x120) = 1 and `deleteSeedOnceCast`
    (+0x168) = 0 are valid for both (NATURE row of `seed_table.md`). The difference is the icon +0x5C:
    `OneOffSpellSeed::CreateSpellIntoHand` 0x72A730 asks `GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40 (the icon
    of that seed in the player's worship sites that passes `WorshipSpellIcon::ValidForRequestSpell` 0x77FBA0, the one
    with the most available chants fn_0077CBC0) and with an icon creates the seed with fn_007282A0 → fn_00727FF0 (+0x5C = icon,
    scale = icon's scale × the seed's); without an icon, fn_00728300 → fn_007280A0 (+0x5C = 0). Without an icon
    `fn_00728FC0` fails: 0x729020 neither draws it nor sends the draw collision, so the hand does not see it
    (`ValidForPlaceInHand` 0x728580 never gets asked). The seed stays alive, invisible and tied to the spell, until
    `Spell::ToBeDeleted` 0x71FD90 deletes it (vt 0xC of +0xAC at 0x71FE16) along with the spell. **Faithful nuance:** an
    orb touched by a player who already has the forest icon in their temple gives a seed tied to that icon, which does
    stay and can be picked up (openblack ports it the same way: `one_off::CreateSpellIntoHand`).
  - **When it goes away.** With the spell closed (on picking it up, through `ClearSpellLink` 0x728200; or through
    `ProcessFromSpell` 0x728F70 if it leaves its player's influence) it stops being drawn at once. The forest shrinks
    0.05 per turn; when the Forest is deleted (`ProcessTrees` 0x725A30: flag +0xA & 1 → +0xEC = 0 and `CloseDown`) and
    the PSys has finished, `ProcessTrees` returns 5 and the spell is deleted with its seed (0x71FE16). openblack:
    `Spell.cpp` `base::ToBeDeleted`, `SpellForest.cpp` `Process` (`psys != 0 ? 1 : 5`).
  - Test: `test_spell_forest` `seedFollowsSpell`.
  - The atom's spiral while falling could not be seen in a still screenshot (inferred to be the same as the original from the code of
    `RotateAxis` and the off-centre `Seed.L3D`).
  - `fn_00725B20` / `fn_00725BF0` (a tree at a random point of the 2..11 m ring) have no caller.
- **MagicTree** (0x74 bytes; `MagicTree::MagicTree` 0x5FCF50, created by fn_005FD000): `Tree(pos, info, forest,
  maxScale 1.0, angle, scale 0)`, magic flag +0x5C | 2, +0x70 = wood multiplier, +0x6C = the spell's
  player (without a spell, that of g_game +0x205A5B) and `CreateReaction(this, REACT_TO_MAGIC_TREE 8, GetPlayer, 0)`.
  - +0x6C = `GetPlayerNumber` 0x64A790 of the spell's `GetPlayer` (vt 0x1C, spell +0xA4) (0x5FCF91..0x5FCFC8). A spell
    with a NULL +0xA4 would make the original read +0xB5 of NULL; no caller does that.
  - `ToBeDeleted` 0x5FD070: its reactions, `Tree::ToBeDeleted` and, if the forest is still available and has run out of
    trees, the forest.
  - `StartOnFire` 0x5FD0D0 removes reaction 8; `EndOnFire` 0x5FD0E0 creates it again unless the g_game flag
    +0x14 & 0x8000 is set (0x5FD0EB `test ch, 0x80` skips the `CreateReaction`). **That flag is "I am inside
    `GGame::ClearMap` 0x552BB0":** it is set at the start and cleared at the end (bw1-decomp `src/Black/Game.cpp` 1296 and
    1388), and that is why the objects destroyed when changing land do not create reactions nor notify their town
    (`Abode.cpp` 92 and 105) nor anything similar. openblack only calls `EndOnFire` from the fire system
    (`ECS/Fire/FireEffect.cpp`, when a fire goes out), never while clearing a land: the condition always holds
    and there is no guard to port.
  - `GetWoodValueMultiplier` (vt 0x868) = +0x70; Tree's 0x74B810 gives 1. `Tree::GetWoodValue` 0x74B7B0 = life × that
    × woodValue × scale × [0xD1A294]; `HandSystem::DepositInStore` already multiplies it. `GetImpressiveType` 14.
- **Tree** (`ECS/Trees`, `components::Tree`): the ctor 0x749E00 receives (pos, info, forest, maxScale → +0x64,
  angle, scale); if maxScale ≠ scale it marks "growing" (+0x5E bit 0) and the count +0x60 (int16) =
  GameRand(growsAfterNumGameTurns); then `Forest::AddTree` and +0x5E | 2. `TreeArchetype::Create(forestId, pos, info,
  nonScenic, angle, maxSize, size)` does the same; `MagicTree` calls it with its forest's id, maxSize 1 and size 0.
  - `Tree::Grow` 0x74A3F0 (amount, setScale, raiseMax): with raiseMax, maxScale = max(maxScale, scale + amount);
    nothing if the scale is already maxScale; otherwise, min(scale + amount, maxScale) with `SetScale` (vt 0x124) or with
    `SetJustScale` (vt 0x51C) and the 3D matrix rebuilt with the scale, the Y angle and the position. Returns how much it grew.
  - `Tree::Process` 0x74A290 (vt 0x5FC, list of those growing): lowers the count; at 0 it goes back to
    growsAfterNumGameTurns and, if it grows below maxScale, `Grow(amount, 0, 0)`; returns 1 if it is still growing.
    The amount is built at 0x74A2D7..0x74A33E: `growthAmount` (info +0x11C) × (1 + 0.01 [0x8C5840] ×
    `rainingAcceleratorMultiplier` (info +0x130) × `GClimate::GetMaxRainingOrSnowing` 0x771600) × (1 + 0.5 [0x8AA3B4] ×
    `MapCoords::GetAlignment` 0x6057B0). The two queries are made at `MapCoords::GetLHPoint` 0x605C40 of the tree itself
    (the ground plus its y).
  - **`MapCoords::GetAlignment` 0x6057B0 (the "alignment of the place") belongs to the terrain, not to a player:** it starts at 0 and
    for each player that exists (order of `GGame::GetNextPlayer` 0x5508A0) adds
    `Influence::CalculatePlayerInfluence(pos, player, 0, type 0, allies 1)` 0x5CD170 × `GPlayer::GetAlignmentValue`
    0x64D6A0, and clips to −1..1 (0x60580F and 0x60582C). That is, the good or evil of whoever rules there, weighted by their
    influence. (`fn_00605850` then passes it through `GetDiscreteAlignmentValue` 0x414730, which is not used here.)
  - Ported: `ecs::TreeGrowthAmount` (`ECS/Trees.h`, with a test in `test_spell_forest`
    `treeGrowthAmount`), the rain with `weather::GetMaxRainingOrSnowingAt` and the alignment with
    `effects::alignment::LandAlignmentAt` (new in `ECS/Effects/Alignment`). With `OPENBLACK_TREE_TRACE=1` each growth
    turn writes `growth <a> = <growthAmount> x rain <r> (mult <m>) x alignment <al>`. It also reaches the
    miracle's trees, as in the original (their forest is one more in the list).
  - `Tree::ToBeDeleted` 0x74A210: leaves its forest (fn_0053A220) and the global tree list g_game +0x205CDC.
- **Forest** (in openblack, the ids of `ECS/Trees`: `CreateForest`, `IsInForest`, `GrowAllTrees`, `ShrinkAllTrees`,
  `TallestTreeHeight`; the trees store the id in `forestId` and there are no lists; 0x58 bytes: id +0x40, next +0x44, `Trees0` +0x48/+0x4C
  the grown ones, `Trees1` +0x50/+0x54 the growing ones):
  - `AddTree` 0x53A310: tree +0x68 = forest; to `Trees1` if it grows below maxScale, otherwise to `Trees0`; each
    list ordered by `DistanceToForest` 0x53A890 (distance to the forest, 0 without a forest): the new one goes before the first
    one that is farther away. `fn_0053A220` takes it out.
  - `Forest::ProcessForests` 0x539D70 (slot 5 of the turn) → `Forest::Process` 0x539DA0: an empty forest (with +0x38 =
    0) sets +0x34 = 2000 and lowers it each turn; below 2 it is deleted. Otherwise, and with +0x3C ≠ 1, `Tree::Process` of
    each growing tree; those that return 0 move to `Trees0`. Then the new natural trees (every 2000 +
    GameFloatRand(1000) × 0.05 × grown, next to a random one, fn_00539FD0 / fn_0053A010, and
    `GPlayer::FUN_0064da80(14, 1)` 0x539FB7 to the most influential player of the new tree,
    `CalculateMostInfluentialPlayer` 0x603830 at 0x539F9B, if not neutral) are made by `ECS/Trees` also in the miracle's forests: a new
    tree in one of them makes there be more trees than N and the whole forest shrinks, as the original would. It is not known what +0x38 and +0x3C are (0 in the ctor).
  - `ToBeDeleted` 0x539C60: `ToBeDeleted` of each tree of the two lists and leaves the list.
  - **There is no merging of forests**: the miracle always makes a new Forest (`CreateForest(0, the first tree's point)`).
  - With `ECS/Trees`: `ShrinkAllTrees` deletes with `DeleteTree` (Tree::ToBeDeleted 0x74A210) and notifies the listeners;
    the miracles one (`MagicTree.cpp`, `AddTreeDeletedListener`) removes their reactions (0x6E4750), releases the hand and
    notes the forest of a deleted MagicTree. `SpellForest::Process` then deletes that forest if it has run out of trees
    (`DeleteForest` = Forest::ToBeDeleted 0x539C60; the last part of MagicTree::ToBeDeleted 0x5FD070) and closes on the
    next turn, like the original. `SpellForest::ToBeDeleted` deletes the forest with `DeleteForest`. The listener
    receives how the tree goes away: `Removed` (the entity is destroyed: its fire also goes, FireEffect::ToBeDeleted
    0x72EBE0, and the hand releases it) or `BecameDeadTree` (`FellTree` or the hand's dead tree: the DeadTree keeps the
    mesh and the fire, fn_00730960 in its ctor 0x510880, and stops being a MagicTree). The MagicTree's wood multiplier
    (0.25 × tribal power, +0x70, 0x5FD0C0) goes in `Tree::woodValueMultiplier` and the DeadTree copies it (+0x9C, 0x5108F7).
  - **The Forest's player is not needed** (it is not for the alignment of the new trees): fn_005399E0 stores it (0x5399F4 `GetPlayer` of the creator → the GameThing ctor fn_0046B8A0), but
    `Forest::Process` 0x539DA0 never reads it; statistic 14 of a new tree (0x539FB7) goes to the most
    influential player of that tree and `GPlayer::FUN_0064da80` exits immediately outside a multiplayer game
    (`IsMultiplayerGame` at 0x64DA90). (inferred) no other reader was found. openblack keeps `CreateForest(0, …)`.
- **The SF_Forest effect without the scene** (what is seen): the seed `Seed.L3D` (scale 0.207) falls from 9.4 m with
  gravity 1.6 (max. 3.35 m/s) spinning, with SOUND_SPELL_FOREST_1 and a disc of blue blotches; on touching land (turn
  41, 4.1 s) the 18 trees come out. **Everything else hangs from the camera atom** (group 2,
  `ParticleAnimWithCameraCreator`: `Tree_Goddess_test.anm` + `Forest.cam`, pause 4 s, alpha from 0 to 252 between 4.1 and
  9 s and back to 0 between 22.1 and 23.5 s, lives 25 s): at 3.5 s `AddSubCollectionsToAtom` adds the sparkles, the
  vortex (`Vortex.L3D`, 6) and the sparkle root (SOUND_SPELL_FOREST_4); at 5.5 s the butterflies
  (SOUND_SPELL_FOREST_3; `UR_ForestPath` + `UR_Flocking` + `ParticleGoodEvilCreator` → `ParticleAnimCreator`
  S_Butterfly_Flap.anm, or bats M_Bat_Flap.anm); and at 3.5 s the light map `Forest_LMap.raw` with permanent
  sparkles. With the camera creator not ported the atom exists but neither draws nor moves the camera, so what is
  seen is the seed, the blotches, the sparkles, the vortex and the light map. **Not ported:** the goddess and the camera
  (postponed).
- **The butterflies and the bats are there** (this corrects the old "not ported" line): `UR_ForestPath` and
  `ParticleGoodEvilCreator` in `Particles/Rules/Forest.cpp`, `UR_Flocking` in `Particles/Rules/Flock.cpp` and `ParticleAnimCreator`
  in `Particles/Creators/Mesh.cpp`. **And they flap:** `ParticleAnimCreator_Butterfly` / `ParticleAnimCreator_Bats`
  (SF_Forest.txt lines 726 and 756) bring `AnimFileName` `.\Data\SPELLS\Anims\S_Butterfly_Flap.anm` /
  `M_Bat_Flap.anm` with `PlayAnim 1`, `LoopAnim 1`, `RandomiseInitFrame 1`, `SpeedUpFactor 1`, `InitialScale 4` / 1 and
  the meshes `S_Butterfly.l3d` / `MSH_A_BAT_1`. Each atom plays the clip (one cycle every 366 ms the butterfly and every
  800 ms the bat) from a random frame, with its bones (`Particle3DAnim::DrawAt` 0x67A8E0,
  `GetCycleTimeFromFrame` 0x6C85F0). Detail and what remains in
  [Particle meshes](particles.md#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome).
- **Hooks:** `OPENBLACK_TEST_SPELL=NATURE,x,z` (or `13`), `OPENBLACK_TEST_MAGIC_TURN=<n>` and
  `OPENBLACK_TEST_FOREST_SHOT` ([openblack-internals.md](openblack-internals.md#debug-environment-variables)).
  With `OPENBLACK_SPELL_TRACE=1` each turn writes `SpellForest trees <n> wanted <N>: grow|decay <sum>` and the event
  `SpellForest event 3 ...: N trees wanted, M made ..., material m`.
- **Tests:** `test_spell_forest` (the spiral with the exact constants, the quantisation to MapCoords, the target
  scale, N, GetMaxObjectsToCreate, the cost and AdjustSpellSeedPos; with `OPENBLACK_GAME_PATH`, the NATURE row: 18,
  0.1, 0.01, 0.05, 0.25, 10000 chants, 120/-1 s, and the `magicTreeTypes`).

## Flocks (`Magic/Spells/SpellFlock`, `Particles/Rules/Flock.cpp`)

The FLYING_FLOCK (24, doves or bats) and GROUND_FLOCK (25, wolves) miracles. The animals are real animals of the
animals module (`ECS/AnimalAI.h`: `CreateAnimal`, `MoveTo`, `SetStateRaw`, `SetFinalDestination`, `Destination`,
`SetAlpha`, `Remove`, `SetDeathCallback`); what their spell classes SpellDove / SpellBat (AnimalDove.cpp) and
SpellWolf (AnimalSpellWolf.cpp) do differently lives in `SpellFlock.cpp` (component `SpellFlockAnimal`).

### The spell (faithful)

- **Classes:** `SpellFlock` (SpellWithObjects, ctor 0x7231C0 / fn_00723210) and its two children: `SpellFlockFlying`
  (AllocSpell 0x723100, 0x120 bytes) and `SpellFlockGround` (AllocSpell 0x723180, 0x110 bytes). Data
  `SpellFlockData`: +0xF4 the Flock, +0xF8 created, +0xFC accumulator, +0x100/+0x104/+0x108 last exit point
  (MapCoords and height above the ground), +0x110 the PSys of the cast (only the flying one). `SetToZero` of the spell
  data is 0x7239E0. `SpellFlockGround::InitWithPos` 0x724220 = `SpellFlock::InitWithPos`.
- **Constants:** 12 animals per second (GetNumToEmitPerSecond 0x7230E0 / 0x7230F0), angle variation 2.0
  (0x723550 / 0x723560), distance `info+0x60` = 800 m (0x723530 / 0x723540), how many = `fistp(numberToCreate ×
  GetTribalPower)` (0x723B80 / 0x724250: 12 doves, 14 wolves; rounding to nearest even), hunting radius `info+0x64` =
  45 m (fn_00723170). `GMagicFlock*Info` offsets: numberToCreate +0x58, alignmentSwitch +0x5C (alongside +0x60 /
  +0x64).
  - Constant addresses: 12 per second at 0x9819B0 / 0x9819B4; angle variation 2.0 at 0x9819C0 / 0x9819C4; jitter 0.1 at
    0x9819CC / 0x9819D8; scales 2.8 / 3.0 (birds) at 0x9819D0 / 0x9819D4 and 1.5 / 2.0 (wolves) at 0x9819DC /
    0x9819E0, so the extra is `GameFloatRand(top − base)`.
- **`InitWithPos` 0x7232D0:** last point = the hand (PSysProcessInfo +0xC) in MapCoords (ftol × 6553.6) and its height
  above the ground; `new Flock(pos, GFlockInfo 0xC5E624, player, 0xABA52)` (Flock::Flock 0x52F780: radius 0x50, and the
  spell sets the flock distance = the radius, 80); then `Spell::InitWithPos`. The flying one (0x723A80) in addition, if
  this computer's interface casts, creates SF_FlockFlyingCastGood/Evil (122 + evil) at the point, without a spell
  (`GJPSysInterface::Create(NULL, ...)`), with its player; `Draw` 0x724100 advances it each frame and it closes
  (vt 0x118) when all the animals are there.
- **Good or evil:** player's alignment (`GPlayer+0x60 → +8`) `< alignmentSwitch` (−0.2) → evil: particles 125
  FlockFlyingRainEvil (otherwise 124, GetParticleType 0x723A30) and **SpellBat** (GAnimalInfo 21), otherwise **SpellDove** (20).
  The ground one is always 126 FlockGroundDust (0x724290) and **SpellWolf** (22).
- **The exit loop** (Flying `Process` 0x723BC0, Ground 0x7242A0), each turn while created < N:
  1. the spell is placed where the leader is (the first member of the Flock, +0x40 → +8);
  2. `emit += 12 × 100 ms × 0.001` (in float); the exit segment goes from the last point to the current hand;
  3. while `created < emit`: `created++`; `f = (created − emitBefore) / (emit − emitBefore)`; S = old +
     `fistp((new − old) × f)` in x, z and the interpolated height; the birth point carries a jitter
     `GameFloatRand(0.2) − 0.1` m in x and then in z (the destination does not);
  4. it is skipped (counting all the same) if the jittered point is off the map (`MapCoords::InBounds` 0x6042C0), if
     a human casts and `CalculatePlayerInfluence(spell position, player, 0, 0, 1) <= 0`, or if fn_00723570 fails;
  5. the animal: `fn_00419D10(S, info, no town, the Flock, age 0)`; scale = `GetScale()` (1, 0x4247E0) × 2.8 +
     `GameFloatRand(0.2)` (birds) or × 1.5 + `GameFloatRand(0.5)` (wolves), the random first; `SetGameAngle(S → T)`
     (GetAngleFromXZ 0x74D240); to the spell's list (at the front) and, all of them, `AddTarget` to the main PSys;
  6. **birds:** height of the point = that of the hand; T at `altitudeNormal` (GAnimalInfo +0x278, 45 m). The leader:
     `SpellEvent{2, its position, no movement}` (reaction 37 ReactToImpressiveSpell), `SetupMoveToPos(T,
     START_WANDER 0x1F)`, `SetState(0, SPECIAL_MOVE_TO_POS 0x2C)` and the Flock +0x78 = FOLLOW_FLOCK, +0x80 = 3
     (formation), +0x7C = DECIDE_WHAT_TO_DO. The others: `SetupMoveToPos(the leader's GetDestPos (+0x80), 0x1F)` and the same
     0x2C. **So the ±2 rad fan is only the angle each bird is born with; they all fly to the leader's T**;
  7. **wolves:** born on the ground (height 0, T height 0) with `SetPlayer(caster)`, a cloud
     `CreateSpotVisual(S, 9 MAGIC_OBJECT_CREATED, 1, 0)`; the leader does the `SpellEvent 2` and `fn_00420F50(S, T, 45)`; the
     others `fn_00420F50(S, the leader's GetDestPos, 45)`.
  8. every bird is a target of the rain effect: `fn_00723160` = 1, i.e. `created % 1 == 0`.
- **fn_00723570 (the destination):** direction d = that of the camera (PSysProcessInfo +0x18) if a human casts, otherwise
  `castPos − hand`; y = 0; (1, 0, 0) if |d|² < 0.0001. Side: human → v = normalise(S − castPos) and `cross = v.z·d.x −
  v.x·d.z`: +1 if > 0.1, −1 if < −0.1; otherwise (and for the AI) +1 with created even, −1 odd. Angle θ = 2 · created ·
  side / N; d rotated θ in Y (fn_00518BF0, row vector: `(x cos − z sin, x sin + z cos)`), set to the length
  (fn_006805F0). T = S with its 10 m cell (the high word) moved to `ftol((cell·10 + d)/10)` and the part inside
  the cell the same; while T is outside and the next distance (half) exceeds 10 m it is repeated with half
  (800, 400, ..., 12.5). Returns `InBounds(T)`.
- **`SpellFlock::Process` 0x7233D0** (the ending of both): if it is not closed, for each member
  `fn_006D0C20(+0x2C, +0x14, GetRadius)` looks for a shield crossed this turn; if there is one, `SpellEvent{4, its position,
  target = the shield's spell}` (`costPerShieldCollide` 50) and **if it answers 0 (the shield holds) the member
  `SetDying` (vt+0x6A4), which in these classes is the fade-out**; with the shield broken it passes; in both cases a
  spark on the shield (fn_006D0AF0). Then `SpellWithObjects::Process` 0x721290 (its loop 0x721040 calls
  `Animal::ProcessBySpell` 0x417700 = `ProcessFadeOut`), and **the flock** (its +0x14, the centre of the domain of
  `components::Flock`) is placed where the leader is (0x7234F2..0x723519; not the spell, which only moves at the start of each class `Process`, 0x723BE6 / 0x7242C8).
- **Cost** 0x723240: `miembros × costPerEvent (0) + costPerGameTurn` (40 / 35). **CloseDown** 0x723270 =
  SpellWithObjects 0x721300: `CoreCloseDown` and `SetDying` to each object (GetSetObjectsDyingOnCloseDown 0x55CF50 = 1).
  **ToBeDeleted** 0x720FD0: CloseDown, the list out; the flying one deletes its cast PSys (fn_007239B0).

### SpellDove, SpellBat and SpellWolf (what concerns the spell)

- **The fade-out (faithful):** alpha 0..255 in a LH3DLib Zoomer (SpellDove +0x148..+0x174, SpellWolf
  +0x168..+0x194; starts at 255, 0x41F280 / 0x420930). `SetDying` (SpellDove 0x41F5C0, SpellWolf 0x420CF0) **never
  calls Living::SetDying**: if the destination is not yet 0, `vt+0xBD4` (fn_0041F2F0 / fn_00420A20, the
  `SetDestinationWithSpeedAndTime(0, 0, t)` of the Zoomer: quartic from the current value and speed) with
  t = `GetNumTurnsToDieOver` (20, 0x41F620 / 0x420D50) × 100 ms = 2 s. `ProcessFadeOut` (0x41F4C0 / 0x420BF0) advances
  0.1 s per turn and with the alpha exactly 0 does `ToBeDeleted`. "The destination" is the second field of the Zoomer
  (+0x14C / +0x16C; the value is +0x148 / +0x168): once started, another `SetDying` (shield, CloseDown, the arrival) does not
  restart it. The mesh colour is `fistp(alpha) << 24 |
  0xFFFFFF` and translucent if it is not 255 (SetColor 0x41F630; SpellWolf::Draw 0x51C6EC): here `SetAlpha(alpha / 255)` in
  the turn (the alpha only changes in the turn). The animal keeps moving while it fades.
- **Wolf, the corridor** (fn_00420F50, faithful): normal `(DZ, −DX)/|D|` of D = destination − start ((1, 0) if |D|² < 0.0001;
  it is the normal, not the direction), offset `−normal·start`, half width 45 m,
  `+0x80` and `+0x148` = destination (0x421054..0x421081; not the start) and then `SetRunToFinalDest`
  0x4209C0 (0x421084: speed = scale × info.speed4 × 1.1, `SetupMoveToPos(+0x148, SET_DYING)`) **in the spell's
  turn** (`SetupWolf`: the wolf runs from its first turn). Those that follow the leader take its `GetDestPos` (vt 0x860 =
  `MobileWallHug` +0x80, 0x416F70; call at 0x724737): **while the leader hunts, +0x80 is the prey**
  (`SetupMobileMoveToObject` 0x60ACE3 writes its `GetWorkingPos` there), so those wolves run to it and
  fade on arriving at 30 m (read that way; in Land 1 next to the town almost all go away in 2-4 s). The class ctor
  (AllocSpell 0x4207F0) does not use the age of fn_00419D10: `fn_0041FD30` receives `grownUpAge + 1` (0x420816..0x420822: an
  adult, which hunts live prey) and the hunger +0xE4 = `info.hunger` (150, 0x420885..0x42088E). `IsPosOnCorridor` 0x420E10: `|normal·p + offset| <= 45` and the point no more than 45 m
  behind the corner of the wolf's 10 m cell along the corridor (not of the start).
  `SpellWolf::MoveToPos` 0x421300: `Living::MoveToPos`; if the hunger +0xE4 `>=` info+0x20C (the ctor 0x4207F0 sets it
  that way: hungry from the start) `ReactToAnimalFoodNeeds` (vt+0xBC0, the lion's hunt); and less than 30 m
  (0x8BF51C, `GetDistanceInMetres` 0x74CD70 in XZ) from the final destination, `SetDying` (the fade-out).
  `IsHuntingTargetValid` 0x420D60: a Living that is not in DYING/DEAD/DOWNED/BEING_EATEN (0xE, 0xF, 0x11, 0x12) nor
  lying down (+0xB4 & 0x80), inside the corridor and with `IsPosValidForTurnAngle` (vt+0xB3C); in addition the wolf is not
  fading (+0x16C ≠ 0). It is vt+0xBB4: called by the prey search fn_004196D0 (0x419715) and
  `Animal::HuntingMoveToPos` 0x418DB0 (0x418E36, if not valid → vt+0xBB8). **Ported** with
  small hooks in the animal files: `AnimalPredators.cpp` (`IsHuntingTargetValid`, the SpellWolf branch of
  `Abandon` = `HuntingMoveToPosAbaondon` 0x420F30, `SpellWolfMoveToPos`), `AnimalAI.cpp` (the MOVE_TO_POS state of the
  SpellWolf calls `SpellWolfMoveToPos`; `Lion::Eat` vt+0xB50 and `Animal::StartWander` vt+0xB48 also for it) and
  `AnimalAIDetail.h` (the declaration). The arrival at 30 m is no longer checked in the spell's turn.
- **Others:** SpellDove/SpellBat `GetTimeToBank` 0.5 s, `ReactToAnimalNeeds` 0x24, SpellBat
  `CanBeFrighteningToCreature` = 1; SpellWolf `SetSpeed` does nothing, `DecideWhatToDo` / `Wander` /
  `HuntingMoveToPosAbaondon` = SetRunToFinalDest (in `ECS/AnimalAI` and `AnimalPredators`).
- The wolves are `GAnimalInfo` 22 SpellWolf at 0xC50DB8. Per-animal ctors: SpellDove 0x41EAC0, SpellBat 0x41EF60,
  SpellWolf 0x4207F0. SpellBat's vtable 0x8BABC4 has the same vt +0x6A4 (`SetDying`) entry as SpellDove (0x41F5C0).

### The particles (`Particles/Rules/Flock.cpp`, faithful)

- **UR_FollowTargets** 0x6A04B0 (ctor 0x6BFA20, properties 0x6B1DD0): flags 2 without 4 (waits for its targets
  until closing). If the effect is not closing, one new atom per step: with `RemoveTargetFromManager` (1) it takes the
  last added target (TakeTargetObject 0x671030), otherwise the first without removing it (fn_00671080); with
  `UseLHPointTargets` (0) and no objects, a point (which it does not use). The atom stores its target, its rule scale = the
  target's scale, and `SoundCreate` plays except with `SoundOneOnly` (1) if it is not the only atom in the collection.
  Each step each atom is placed on its target (GlobalToLocal); a target that went away is forgotten and, with
  `RemoveAtomWhenTargetDies` (0), its atom is deleted. In SF_FlockFlyingRain* (SOUND_SPELL_DOVES / _BATS) and
  SF_FlockGroundDust (SOUND_SPELL_WOLVES) the UR_WillowWisp trail (3 or 5 glows) hangs from each bird or wolf.
- **UR_Flocking** 0x683580 (ctor 0x683470, properties 0x6ABCD0; used by SF_Forest, SF_Butterflies*, SF_Flies* and
  SF_CreatureSpellItch*): a flock of particles. Per collection it stores the
  flock velocity V (CollectionData 0x560EC0 +0x24, starts at 0). Each step (dt that of the effect, [0xD4E0EC]):
  - the mean of the velocities and of the positions C of its atoms; the target P = the parent atom (or the origin) in the
    collection's frame (fn_00674B10);
  - `V = V (1 − dt K_FlockDamping) + unit(P − C) K_IdealVel f(|P − C|, IdealVelAccnType, F_InvertAccnIdealVel) dt`;
  - for each atom `a` that has another: the sum over the others b of `unit(b − a) f(|b − a| LocalScale, NeghbourAccnType,
    NeighbourAccnInvert)` × `K_NeighbourAccn / n`; the nearest gives the avoidance `unit(nearest − a) f(|..| LocalScale,
    2, no) K_NeighbourAvoidance`; the centre the attraction `unit(C − a) f(|C − a|, AxisChosen, F_InvertAccn)
    K_CentralAttraction`; and the matching `(mean − (v_a − V_before)) K_VelocityMatching` (× LocalScale). Acceleration =
    neighbours + matching + attraction − avoidance, clipped to `F_MaxAccn`; `v' = (v_a − V_before)(1 − dt K_Damping) +
    accel·dt + V`, clipped to `F_MaxVel`;
  - the orientation: with `SpriteRotation` (1 by default) `SetAngleY(atan2(−y, x) + π/2)` of v' seen from the camera
    (fn_006840E0); otherwise, `UpdateBanking` 0x684160: `SetAngleY(−π/2) × Rx(bank) × Rz(−pitch) × SetAngleY(heading)`
    (product of LHMatrix rows), heading = atan2(v.z, v.x), pitch = atan2(v.y, |v.xz|) × ReducePitchBy, bank =
    atan((a.z v.x − a.x v.z) / |v.xz| / GravityForBanking) with a = (v' − v_a)/dt; local +Z faces the velocity;
  - at the end each atom advances `pos += v·dt`.
  The law fn_00683520 (d, type, invert): x = max(d, 0.01) × ScaleModifier; type 0 → 1, 1 → x, 2 → x²; 1/that unless
  inverted (the types are integers 0..2, the setters 0x6AC0D0.. do not accept others; the file writes them as FLOAT).
  By default (ctor): K 1 / 1 / 0 / 1 / 0 / 0 / 0, F_MaxAccn 10, F_MaxVel 100, GravityForBanking 10, ReducePitchBy 1,
  ScaleModifier 1, types 0 / 2 / 1, SpriteRotation 1.
- **EventConditionAtomNearVillagers** 0x67D8E0: the atom's position +0x80 (in metres, as is) in MapCoords is on
  the map and one of the objects of that 10 m cell is a villager. SF_FlockGroundDust declares it but no rule
  uses it.

### Pending / not faithful

- **Done:** the hunting in the corridor, the start in the spell's turn, the arrival in the wolf's state
  (above), and the vt+0x6A4 of the three classes now replaces `Living::SetDying` (`animal_ai::SetSpeciesDying`,
  `ECS/AnimalApi.cpp`; there is no longer DYING nor a corpse for them).
- **Birth angle (faithful by reading):** `SetGameAngle` 0x60DA90 (+0x5C and `SetYAngle`) from `GetAngleFromXZ`
  0x74D240 (jittered point, T; 0x7245E8..0x7245FB) = `LHArcTan` 0x74D0C0 of T − S: it is `detail::AngleOfMapCoords`;
  the drawn face is that of the animals' `FaceAngle` (their `ConvertGameAngleTo3D` convention).
- **(approximate)** The previous position (+0x2C) for the shield is that of the spell's previous turn.
- **(pending, animals)** In openblack the wolves chase (HUNTING_MOVE_TO_POS 41 against villagers inside the
  corridor) but do not get to the leap: they circle at 30-50 m from the prey. Their `SetSpeed` does nothing (0x4209B0), so
  they run at ~1.5 m/turn and the radius of `SetTowardsAngle` (2 × speed / turn, 0x418681) is ~29 m; inside it the
  reduction at 0x4186B4..0x4186FC almost cancels the turn. Read the same in the original (the radius and the formula), but not
  checked against the original game: it may be faithful or a bug in the animals' movement.
- **(inferred)** UR_Flocking with `SpriteRotation` (flies, itch): the matrix 0xEA1D28 is taken as the
  world→camera rotation (x = v·right, y = v·up), as `UR_OrientSpriteWithVelocity` does (Rules/Orient.cpp).
- **Real duration with a hand seed (inferred from the chant rules, measured):** 5000 initial chants
  / 40 (doves) or 35 (wolves) per turn, without recharge: 12.5 s and 14.5 s, not the 25 s / 60 s of the timer (the
  script cast is by the neutral player, which replenishes: there it closes by the timer).
- Not ported: `NeedsContinualPackets` 0x723280 (true while `created < N` if a human casts and it is the interface's
  player; otherwise, `Spell::NeedsContinualPackets` 0x7214C0): decides whether the interface sends packets with the hand; in
  local play openblack refreshes `PSysProcessInfo` every turn (`creator::UpdateSpellInfo`), so it changes nothing (inferred).
  Load/Save neither.

### Hooks, tests and screenshots

- `OPENBLACK_TEST_SPELL=FLYING_FLOCK,x,z` and `GROUND_FLOCK,x,z` (the info.dat names; or 24 / 25): the neutral
  player casts from 30 m above the point, so d = (1, 0) and the side alternates; `OPENBLACK_TEST_FLOCK_SHOT=
  "<turns>,<path.png>[;...]"` requests screenshots that many turns after the cast. With `OPENBLACK_SPELL_TRACE=1` each
  animal writes `SpellFlockFlying|Ground <n> #<N> entity e at (x, z) +h m -> (Tx, Tz)` and every 10 turns each member
  its position, state, alpha, prey (`target`, its distance) and speed; `SetDying (state, final)` says when and in which
  state the fade-out began. To follow them with the camera: `OPENBLACK_TEST_VIEW_ANIMAL="0,35,-90"
  OPENBLACK_TEST_ANIMAL_SPECIES=20` (22 the wolves) `OPENBLACK_TEST_VIEW_LOCK=1`.
- `test_flock`: N with the fistp rounding, good/evil, the fan (direction, side, angle, rotation), the destination (cell,
  truncated towards 0, the halves), the exit interpolation, the wolf's corridor (normal, edge, behind the
  cell) and the fade-out (2 s, 79.7 at half, 0 at 20 turns); with `OPENBLACK_GAME_PATH`, the real rows (12/14,
  −0.2, 800, 45, 40/35 per turn, 50 per collision, 25/60 s, reaction 37, altitudeNormal 45); and from UR_Flocking the law
  fn_00683520 and the UpdateBanking rotation (heading, reduced pitch, bank).

## Fireball and lightning (`Magic/Objects/MagicFireBall`, `Particles/Rules/{Fireball,Lightning}`)

Everything below has been
read in the exe (W120) except what is marked UNVERIFIED or "(inf)".

The fire they light (the heat model, `src/ECS/Fire`, its villagers and its CHL natives) is in
[Fire](magic.md#fire-srcecsfire).

### Fireball (MAGIC_TYPE 1-3, seed 2 FIRE)

- **`MagicFireBall`** (`Magic/Objects/MagicFireBall.cpp`, 0x682970-0x683390): an invisible Object **that is not in the
  map cells** (which is why `IS_FIRE_NEAR` does not find it), created by the rule `AttatchFireBallToAtom` and stuck to its
  atom. Its `FireEffect` starts at the effect's `initialTemperature · strength` and is what burns; the row of
  `GMagicFireBallInfo` (3 rows, 0xD4E3B0) is chosen by the upgrade level. It is deleted when it drops below `deletionTemperature` (the
  atom stays marked as deflected) or when its atom disappears.
  - **Catching**: `ValidForPlaceInHand` 0x682DD0 accepts any that is not the player's own; `fn_00682EA0` creates in the
    hand a new FIRE seed **already ready** (`fn_00729900(0)`) and charged, and the ball goes away.
  - **Absorbing**: `DeleteAndPutIntoSpellSeed` 0x682E10 multiplies the power of the held FIRE seed by
    `1 + catchIncreaseFactor`.
  - `MagicFireBall` is created by `fn_00682B70` (Object(pos, info); the scale comes with the first follow of its atom) at
    the head of the fireball list (g_game +0x205C9C). `MagicFireBall::ToBeDeleted` 0x682C30 takes it off that list, and
    its fire goes too. The ball dies with its atom through the atom data's dtor 0x682FA0.
  - The fire's temperature is the manager's strength (+0x54) × `initialTemperature`. The fire's player is the ball's
    (`MagicFireBall::GetPlayer` 0x682BF0: its manager's player).
  - Catching: `InterfaceTap` 0x682E50 and `InterfaceSetInMagicHand` 0x682E80 both lead to `fn_00682EA0`, which makes the
    seed with `fn_00728300` (seed 2, pu −1, multiplier 1). Absorbing is called from `SpellSeed::ApplyToMagicFireBall`
    0x728AE0 and multiplies the seed's power at +0x8C.
- **PSys rules** (`Particles/Rules/Fireball.cpp`): `UpdateRuleGravityWithFloor` 0x6A1880 (the bounce uses
  `DampingHorozontalBounce` / `DampingVerticalBounce`, the ground drag and the impact sound per size
  class), `CreateWithInitialDirection` 0x69E950, `AttatchFireBallToAtom` 0x682FD0, `EventAlways`,
  `SetAtomHasBeenDeflected` 0x6A26C0, `UR_SideSpin`, `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`,
  `UR_Trail`, and the conditions for deflection, nearby water and steam.
  - `CreateWithInitialDirection` 0x69E950, non-human cast (script, computer, creature): the new 30° solution
    is applied if `|v|² > 0.01` (`fcomp qword [0x8C7620]`, the double 0.01, at 0x69EC60) and `v.y / sqrt(vx² + vz²) >
    tan(0.52370351552963257)` (double [0x9375F0], `fptan` 0x69EC77), all in the FPU. The port compared with 89129 (the 4
    low bytes of the double read as a float, a disassembler error already corrected), so it almost never applied it
    (corrected, `Fireball.cpp`). The new time (0x69EC92..0x69ECC1) is `sqrt((Δy − D·tan)·(−2/g))`, with the radicand set to 0.1 if it comes out negative (Δ [esp+0x64] 0x69EB1B, D [esp+0x38] 0x69EB3F).
  - `CreateWithInitialDirection`, script / computer / creature cast: a ballistic arc from the gesture position to 0.8 of
    the way to the origin, in max(0.025·D, 0.5) s (0x69EB43..0x69EB5A), v = delta / t − (0, −g·0.5·t, 0)
    (0x69EB62..0x69EC39); g is the group's `UpdateRuleGravity` +0x24, 30 without one (0x69E9A0). The target at 0.8 and
    the origin fallback before 0x69EAF0 stay UNVERIFIED.
  - fn_0069E6B0: the hand's direction pitched up 0.3 rad (clamped to ±π/2); fn_0069E620: the hand's speed through
    (0 → 0, 50 → 50, 450 → 200), 200 above 450; the sound size class comes from speed / 200 (fn_0069E610).
  - For the local interface's cast the ball's atom gets a `DrawOffset` (fn_006C7840) that draws it from the hand to the
    start; not ported, so the ball appears at the gesture point.
  - `UR_SideSpin` 0x69E300 (+0x20 ScaleAngularVelocity, +0x24 MaxAngularVelocity, +0x28 TimeToFade): the first time,
    with a player behind the effect, w = curl (manager +0x58) × scale, clamped to ±max, then w·(w/max)²; every step the
    velocity turns about y by w·(1 − age/TimeToFade)·dt. (inferred) defaults; the fireball file sets 0.5, 1.2, 1.5.
  - `AR_FadeAlphaWithHeightAboveLandscape` 0x6A4DE0: alpha from AlphaAtZero on the land to AlphaAtRefHeight at
    RefHeight above it (and beyond).
  - `AddSubCollectionsToAtom` 0x69F220: once per atom (atom data +0x20), the NextGroups under it.
  - `UR_Trail` (MAC 0x6A40F0, first step fn_006A41B0, then fn_006A4280): NumAtoms atoms behind the parent atom along the
    history of its last NumGameTurnsToSpreadOver positions (a ring at collection data +0x24, the whole ring moved back
    along the velocity at 0x6A4308), covering at most MaxTrailLength × the parent's scale, spaced 0.5·k²
    (UseNonLinearSpacing) or evenly, the newest last; fn_006A7E30 gives the i-th newest sample and fn_006A8020 lerps
    samples. The first and last atoms take the HeadGroup / TrailGroup init (−1: no creator, not drawn). (inferred)
    defaults.
- **Review of the double constants** (all the .rdata addresses cited in `src/Magic`, `src/Particles`,
  `src/ECS/Fire`, `src/ECS/Weather`, `src/ECS/Effects` and `src/Worship` that the exe reads with `qword ptr`): wrong only
  [0x8C7620] (above) and, borderline, [0x900C70] of `MapShield` (0.30000001192092896 = 0.3f widened, not 0.3). Right:
  [0x8C79D8] (1e-4f widened: gestures, `Particles/Utility.cpp`), [0x8D45D8] (2π float widened: MapShield, Forest, Storm,
  SpellSeedGraphic), [0x9361E8] (−π/2 float), [0x9003C0] (0.99e30), [0x900AE8] (1.1), [0x980518] (1/30), [0x8AB260]
  (0.5), [0x8CF7D8] / [0x9375E8] (0.6 / 0.3 of the sound size; `Audio/SpellSounds.cpp` compares them in float, which
  only differs with a fraction of exactly 0.6f or 0.3f).

### Lightning (MAGIC_TYPE 4-6, seed 6 LIGHTNING_BOLT; `Particles/Rules/Lightning.cpp`)

`UR_Lightning` 0x6914C0 is a creation rule that maintains a structure of forks while it is channelled. Per
collection it stores a `UR_Lightning_CollectionData` (0xC0 bytes; here an entry keyed on the rule's slot, like
the fireballs, so that nothing is left dangling).

- **Origin and direction.** With `CastingFromHand` the origin is the gesture position (`GetCurrentGesturePosn` 0x673600 =
  `PSysProcessInfo` +0x0C); otherwise, the parent atom. The heading of the cone is **fn_00673650 = `atan2(cameraForward.z,
  cameraForward.x)`**, that is the camera's forward (not the hand's heading; verified: the
  `PSysProcessInfo` starts at manager+0x24, so manager+0x3C/+0x44 are `cameraForward.x/z`).
- **Targets** (`fn_00690F50`, three modes):
  - from the hand (`fn_006901E0`): spiral of `4·ceil(R/10)²` cells of 10 m from the origin with R = `SearchRadius` (60
    in the base lightning); valid is the available object that is not a spell seed (fn_00690090) and whose horizontal direction
    satisfies `dot(normalize(dx,0,dz), (cos heading, sin heading)) > cos(SplitAngle)` — a cone of half-angle 0.2 rad ≈ 11.5°.
    Stops at `MaxLightningObjects`;
  - if fewer than `MinLightningObjects` come out, it fills in with **ground points**: angle `heading + rand(pi/4)` (verified: it is
    `+rand(0, pi/4)`, not centred; the constant is 0x3F490FDB), distance `rand(0.6·R)`, y = terrain + 2 (0x8AB478);
  - `TakeTargetsFromManager` takes the `SpellTargets`; the parent mode (fn_00690880) uses `DefaultSearchRadius` 20.
  - A creature is skipped when `creature+0x12B0 < 0.5` (fn_0047ACC0, UNVERIFIED what it means): not ported.
- **Structure** (`CreateForkStructure` 0x691190): deletes the collection's atoms, creates **one** atom and hangs from it
  `2 · number of targets` subcollections of the `ForkGroup` (0x69119B..0x6911A6), each with `MaxJointsPerFork` atoms of
  `ParticleChainCreator`. It removes the interpolation from each fork (`and byte [edi+0x38], 0xFD` at 0x6912A1; also
  0x691B42 and 0x692B1D): **faithful** (`Collection::flags`, bit 2; 3 by default in the ctor 0x675CA2). fn_00679920 only
  interpolates between the last two steps with that bit (0x67999E); without it it draws the current PSR, so the lightning changes
  shape abruptly at each step (10 Hz), like the original. With `CastingFromHand` and the own hand (fn_00691B80) each
  joint of each fork receives a `DrawOffsetLT` (0x69130C..0x69134C): **faithful**, see below. When re-searching
  targets (fn_00691390) the structure is **not** rebuilt: if there are more targets than forks, the recursion stops
  (0x692786).
- **Order of each step** (`ActualModify` 0x6914C0, faithful): origin and heading; the first time, search and structure; the
  collision with another lightning (fn_006916B0, 0x6915E5); re-search (fn_00691390, 0x6915EE); the sound; **all** the forks
  are released from the root on **every** step, whether or not the manager is in state 2 (0x691651..0x691681; the port said that outside
  state 2 they stayed as they were: that was an error, now they are not drawn); and only in state 2 `UpdateForkStructure`.
- **Each step** (`UpdateForkStructure` 0x691BB0):
  - the scale of the forks is `ForkScale · FP_ForkScale` (+0xAC);
  - field +0x18 of each target ("cooldown") goes down one by one (0x691C26..0x691C31; signed, `jle`). It is
    written when searching (`PSysRand(ftol(AverageLightmapLife / ([0xD01A38] · 0.001)))`, 0x6910C9) and at each strike
    (0x692935), but **nobody reads it to skip a target**. It is just data. The port used it as a filter and that was the main cause of the "goes in jumps"
    (corrected);
  - without collision with another lightning it picks each active target with probability `min(+0x68, (N+1)/2) / N` until reaching that
    number (0x691C5B..0x691CF3). If the lightning is linked with another (data +0x20 or +0x24, the collision of
    fn_006916B0 / fn_00691AD0) it picks **one**: the first active one from `PSysRand(N)` (0x691CF5..0x691D56), **faithful**;
  - before the recursion, `PSysRand(2 · forks)` picks the collection of +0xA4 (0x691D98..0x691DC3), which nobody uses:
    the port only does the roll;
  - if none is picked, that step has no forks (0x691D59..0x691D6E);
  - with `SoundLightning` (+0x80) not NO_SOUND, every step the root atom's sound of that action (`GetSoundOfAction`
    0x674550) is started when it has none while the manager is in state 2 (`StartSound` 0x6745D0), and stopped when it has
    one otherwise (`StopSound` 0x674500) (0x691601..0x69164C);
  - re-searches (fn_00691390) when `RenewSearchEvery > 0` and the age since the last search (+0x5C, set to 0 at
    0x690F66) exceeds it, or with `RenewTargetsOnMove` if the origin has moved more than `RenewTargetsOnMoveFrac · R` from
    +0x3C. fn_006916B0 is not "re-search": it is the collision of two lightnings (list 0xD4EC60), see below.
- **Collision of two lightnings** (fn_006916B0 / fn_00691AD0, **faithful** except the glow, see the end):
  - the data of each collection goes into a global list, the newest at the front (0xD4EC60, count 0xD4EC64; ctor
    0x68FED0..0x68FEEE), with the creation turn at +0xB0 (g_game +0x205A40, 0x68FEA5) and `CastingFromHand` at +0x59
    (0x68FECA). The dtor fn_0068FF50 takes them out and undoes the links;
  - each step, a lightning cast from the hand and not linked (+0x59, +0x20 = 0) sets +0xB4 to 0 and searches the list for an
    **older** lightning (+0xB0 smaller; one from the same turn does not count) cast from the hand, with targets (+0x74) and without a link
    or linked with it. With d1 = its centroid (+0x48) − its origin and d2 = its centroid − this origin, the collision point
    is `T = centroid − 0.7 · 0.5 · (normalize(d1) + (cos h, 0, sin h)) · min(|d1|, |d2|)`, h the heading of this lightning
    (0x6918E8..0x6919F3). They are linked if the headings go towards the same side (`cos h' cos h + sin h' sin h > 0`,
    0x6918AF..0x6918E2), `|T − origin| < SearchRadius` (fn_006901D0) and `normalize(T − origin)` is less than
    1.0367 rad from h (double 0x936D90, 0x691A47..0x691A72). Otherwise, the old one that was linked is unhooked;
  - linking (fn_00691AD0(old, new, CommonGlowGroup)): old +0x20 = new, new +0x24 = old, new +0xB4 = T, and
    if the old one had no link, a `CommonGlowGroup` on the last joint of its fork 0 (fn_00691B50) without
    interpolation (0x691B42). On unhooking, that joint loses its subcollections (fn_00673B20);
  - in fn_00691F30, depth 0: the new one (+0x24) takes the trunk to its +0xB4 and there it **stops** (neither branches nor strike,
    0x692110..0x692136); the old one (+0x20) takes the trunk to the new one's +0xB4 and continues with **one** more fork, depth 1,
    from there to its target with the scale × this +0x60 = **3.0** (only the ctor 0x690183 sets it; it is not a
    property), so opaque and thick (0x69213B..0x692172, 0x6927EC..0x6928EA). Its strike has strength 2 and the same
    event also goes to the new one's spell (fn_00690070, 0x692AA0..0x692AB9);
  - `CommonGlowGroup` (the scale-5 glow of SF_LightningBolt) is **only** used here (0x691AAF is its only read):
    the port put it at the tip of each fork (inferred, without basis), removed;
  - (approximate) the original touches the other lightning's atoms in the step of the one searching; the port leaves it for the next
    step of the affected lightning (`ApplyGlow`), so the glow may arrive or leave one turn late. A data entry whose collection
    stops taking steps (none this turn or the previous one, or 10 s of clock) leaves the list with the dtor's semantics.
- **Search** (faithful): spiral `GUtils::Spiral` 0x74D7E0 with `dir = count = 1` (0x6902A7..0x6902BB): it first updates
  (`--count == 0 → dir++, count = dir/2`) and then returns `table[dir & 3]`, so the first step is −x. fn_006901E0
  walks `4·ceil(R/10)²` cells (0x6902A4) and fn_00690880 (storm) only `ceil(R/10)²` (0x690906, without ×4). An object
  only counts in the cell where its position is (fn_00604F40, 0x6903AE / 0x6909B4), so a multi-cell object
  does not enter twice.
- **Fork tree** (`fn_00691F30(depth, data, fork, origin, list, scale)`, from 0x691DE6 with
  fork 0, depth 0, scale 1 and the next free fork +0xA0 = 1), **faithful**:
  - centroid = sum of the tips of the active targets of the list / size of the list (0x692040..0x69210C);
  - with 2 or more targets the fork stops at `origin + (centroid − origin)·(0.2 + rand(0.4))` (0x69217C..0x6921F2);
    with one it reaches its tip;
  - joint i: `origin + (cut − origin)·i/(n−1)`; the inner ones are displaced `(rand(2·RandomFrac) −
    RandomFrac)·length` **in X and Z** (0x6924D5..0x692511; not Y and Z);
  - scale from `S/(depth+1)` at the root to `S/(depth+2)` at the tip, `S = scale · ForkScale · FP_ForkScale`
    (0x692366..0x69239E, 0x692515..0x692545): the trunk goes from S to S/2;
  - alpha `128 + PSysRand(127)` on all forks; 255 only if the branch's scale is > 1, which only happens in the
    collision of two lightnings (×3.0 of +0x60) (0x6923FF..0x692418, 0x692548..0x69257C);
  - with 2 or more targets it splits them by the sign of `dot((tip − cut).xz, (centroid − origin).xz)`: > 0 to the
    first list, the rest to the second (0x6925B9..0x6926F4). If one comes out empty, it keeps the last of the other
    (0x6926FA..0x69276A). Then it recurses with depth+1 from the last joint, first with one list and then with the
    other, each with the next free fork (0x69276D..0x6927DA, depth-first). It stops if there are no
    forks left. `2N−1` forks come out for N targets struck;
  - with one target, strike (0x6928F9..): +0x18 = `PSysRand(AverageLightmapLife/dt)`; **fn_00691E80** creates the
    `LightMapGroup` atom (`PCreatorLightMapAtom`) at the centroid of the list, which is the tip. fn_00691ED0 (it is not the light
    map) is the "impressive" notification (`GetImpressiveIntensity` -> fn_00692FA0), once per target when
    +0x5C > 0.2 (TODO). Then comes the `SpellEventInfo` type 3 at the tip (fn_00691E00) with **strength 1**. It is 2 only if
    the lightning is linked with another (data +0x20, 0x6929DE..0x6929EE). It is not 2 when the lightning comes out of a parent
    object. The same event also goes to the manager of the linked lightning (0x692AA0,
    fn_00690070), **faithful**.
  - On entry, the fork is re-hooked to the root (0x691F6D..0x691F7D) **before** the tests: if it then
    exits without placing it (it lacks the `DrawOffsetLT`, 0x692007, or the terrain, 0x692262) it is drawn with the joints
    of the last step that placed it. The port does the same.
  - Shield on the segment (0x692268..0x692361), **faithful**: `fn_006D0BC0(cut, 2.5)`; inside one, the entry point
    of the segment from the origin (vt 0xFC `FindIntersect`, margin 2.5), a `SpellEventInfo` 4 there {no movement, strength 1
    (2 linked), no shield test, target the shield's spell (fn_006D0B10)} to the lightning's spell; if it answers
    0, the fork ends at that point (neither branches nor strike). The spark on the shield (fn_006D0AF0) always goes.
  - `NumTexturesToTile` (this +0x48, ctor −1), **faithful**: if it is not −1, the fork's chain is cut into
    `max(1, ftol(NTT · length / data+0x60))` repetitions (0x6923D0..0x6923FC; `Collection::chainTextures`).
    data +0x60 **does** have a writer: fn_00690F50 sets it to the distance from the origin to the centroid of **all** the
    targets (+0x48 = sum of tips / N, 0x6910BD..0x691177); the 1.0 of the ctor (0x68FE7F) only holds until the first
    search. Used by SF_LightningStrike /
    SF_LightningStormPush (15).
  - The tip position is `fn_00691E00`: the object in its map cell, height `GetAltitude + (+0x1c) +
    GetHeight`; a ground point, itself.
  - Without a spell (script or weather lightning) the original applies the static `EffectValues` of the info 0xCC9704 at the
    tip: **UNVERIFIED** which row of `GEffectInfo` it is; not ported.
- **Cut with the terrain** (**faithful**; `LandIslandInterface::RayCast`, common API in
  `3D/LandIslandInterface.h` + `3D/Implementations/LandIsland.cpp`). In fn_00691F30
  0x69220C..0x692262, after choosing the cut point and before the shields:
  - `LH3DIsland::RayCast` fn_00802550(origin, cut, &x, &z) converts both points to cells (x, z · 0.1 [0x8AC404],
    y / 0.67 [0xC3720C]) and calls `RayCastInternal` fn_00802680. If it hits, the point in metres (× 10 [0x8AB414],
    globals 0xE9CD80 / 0xE9CD84).
  - `RayCastInternal` casts a **ray**, not a segment: it moves the end to `end + t·(end − start)`, with t the first
    map edge (0 or 512) in x or in z, that is, up to the edge plus one segment (0x802683..0x802765). The guard
    `t == 1e20` compares the float 1e20 (0x60AD78EC) with the double 1e20 (0x9A2BE0), which are not equal: the end moves
    **always**. Then it clips it to 0.1 .. 511.9 (Cohen-Sutherland, first x and then z; after clipping in z, the code of
    that end becomes 0 without looking at x again) and walks it cell by cell with the four DDA variants
    (0x802C80..0x803078: slopes `dx/dz`, `1/(dx/dz)`, `dy/dz`, `dy/dx`; the z edge is crossed if the x of that crossing
    stays in the column, otherwise the x one). A single column or a single row tests each cell with the whole segment.
  - The cell test fn_0083AE80 reads the four corners of the block itself (cells +4, +0xC, +0x8C, +0x94, also
    the edge row; `GetCellCorners`). If both ends are below the lowest corner or above the highest, there is no
    hit. Otherwise, it tests the plane of the two triangles (0.0)(1.0)(0.1) and (1.1)(1.0)(0.1):
    - u, v within −0.1 .. 1.1 [0x8C9B2C / 0x8AB230];
    - `u + v ≤ 1.1` in the first and `≥ 0.9` [0x8C5844] in the second;
    - the point in front of the start;
    - a denominator smaller than 1e-4 (qword 0x8C79D8) in the first triangle ends the test without looking at the second.
  - Without a hit, a descending ray (end.y ≤ start.y, |dy| ≥ 0.0001 [0x8BF518]) gives its crossing with y = 0. It only counts if it is
    ≤ 7500 m (`dx² + dz² ≤ 5.625e7` [0x9A2BD4]) from the camera (`g_camera` 0xEA1DB8; 0x8025D9..0x802672).
  - If the terrain is closer to the origin (in x z) than the cut (`fcompp; test ah, 1`), it exits **the whole** function
    (0x692262 → 0x692ABE). That fork is not repositioned: it was already re-hooked on entry (0x691F6D) and is drawn as
    the previous step left it. There are no branches, strike or shield test either.
  - The other seven callers in the exe, listed in the comment of `RayCast` for whoever ports them: `GCamera::Update`
    0x442406, fn_0044EF60 0x44F046, `CameraModeNew3::Update` 0x45DA4A, `GLandscape::Draw` 0x5E4848, fn_005E5620
    0x5E5660, fn_00800C30 0x800D79 and fn_0086BD00 0x86BF1C.
  - **(approximate)**: float arithmetic where the original keeps extended x87 values between the `fstp`s; it only changes
    something right at the edge of a cell or a triangle.
  - **(port guard)**: a cap of 4·512 + 4 steps in the walk.
  - With `OPENBLACK_SPELL_TRACE` the trace "fork at depth N ... cut by the land at (x, z)" comes out.
- **`IsAvailable`** (faithful): `SearchAround` (fn_006901E0 / fn_00690880) asks `IsAvailable` (vt +0x2C,
  0x69038E / 0x690997) before the own cell and fn_00690090. It is `GameThing::IsAvailable` 0x401810 (not being
  deleted) and, for a villager, `Villager::IsAvailable` 0x751D50 (not DYING, `ecs::villager::IsAvailable`). The manager
  mode (fn_00690C70, unread) does not use it in the port.
- **Cooldown**: `PSysRand` 0x6729E0 is the function at [0xD4E0BC] (`GameRand` 0x672AF0 or `LocalRand`
  0x672B40, depending on fn_00673340):
  - with n = 0 it gives 0 without rolling (0x510693 / 0x6DE574);
  - otherwise, `LHRand % n` **unsigned** (`div` 0x7DB62B);
  - the `ftol` of `LightmapSteps` reaches it as is (0x691091 / 0x69292F). A turn of 0 ms gives +inf, the ftol gives
    0x80000000 and the roll comes out in 0 .. 2³¹−1;
  - the previous comment ("no cooldown") was *(inferred)* and was wrong;
  - the port does it with `RandomBelow(effect, n)` (was `PSysRand`) in `Lightning.cpp`, which is already `game_random::psys::Rand` (faithful, also
    0x691CF9, 0x691DA0 and the alpha `PSysRand(0x7F) + 0x80` at 0x692551).
- **`DrawOffsetLT`** (**faithful**; `Atom::drawOffset` in `PSys.h`, added in `Effect::Collect`/`CollectChains`). When it is
  cast from the own hand (fn_00691B80 = `CastingFromHand` and `NetUnsafeIsMyInterfaceCasting` 0x673540: the +0x44 of the
  spell, 1 without a spell), each step calls `SetRefPos` 0x6C7600 (vt 0x100) on each joint of the fork with
  `(the step's origin = data +0x30, weight w − w·i/(n−1))`, w = 1 on the **trunk** (depth 0) and **0** on the branches
  (0x691FC7..0x69203E), with the weight clipped to 0..1 (a NaN gives 0). Each frame, `GetOffset` 0x6C7690 gives
  `(my interface's hand position now, GInterface +0x3A0 = CHand, +0x78, − the step's origin) · weight`, which
  fn_00679920 adds to the drawn position, interpolated or not (atom +0x124, 0x679B69..0x679BBF): the start of the lightning
  stays stuck to the hand between steps and the tip does not move. In the port the hand is that of the `HandSystem` (the same one from
  which `PSysProcessInfo` +0x0C comes). `DrawOffsetDecay::ProcessList` 0x68F680 has nothing to do with it: it is the clock of the other
  subclass (`DrawOffsetDecay`). Other rules that create `DrawOffsetLT` and do not use it yet: `UR_FollowCastPosn` 0x69FEAD
  (the origin sprite of SF_LightningBolt, `Rules/HandFollow.cpp`) and `UR_HandSprinkle` 0x6A046B.
- `UR_LightningStrike` 0x6937A0 (SF_LightningStrike / SF_LightningSingleStrike, the script and weather lightning) only creates one
  atom with its `NextGroups` —where the strike's `UR_Lightning` lives— and its `SOUND_SPELL_LIGHTNING`, once.
- **Numbers per event** (confirmed in the log): PU0 burn 800, crush 0.0006,
  hit 0.0017, radius 1 m; PU1 1400/0.0011/0.003; PU2 2000/0.0018/0.0075.
- `LightningForkFlicker` 0x6B24D0 / 0x6A1000: **unused**. None of the 132 spell files uses it.
- **Ribbon colour** (faithful, `Creators/Chain.cpp`, `Graphics/RendererChain.cpp`). Creator default values:
  `FrameHeight` 64, `FrameWidth` 32 and `NumTexturesForWholeChain` −1 (ctor 0x6AA739..0x6AA747; with −1, `CreateChain`
  sets n−1 at 0x6AA8DF). With them, fn_006C8920 sets the **U across the width** `[(frame+FileOffset)·W, +W]/256`. The frame
  is `FrameOfHead` in the last repetition, `FrameOfTail` in the first and 0 in the others. The **V goes along the length**:
  `H/256 · local segment / segments of the repetition`. The lightning thus uses strip 0 of `S_lightning` (white core with a cyan
  halo) repeated 4 times. The vertices are `joint ± side·scale` (half-width = scale, 0x67BA3B..0x67BB0D). The
  ends of neighbouring segments are joined at their midpoint (0x67BD2D..0x67BE82). The V offset
  (0x67BE91..0x67BED5, with `fmod` by `H/256`) is 0 because chain +0x4C = 0 (inferred: no other writer has been looked for).

The lightning's ribbons (`ParticleChainCreator`) and light maps (`ParticleLightMapCreator`) are in
[Chains and light maps](particles.md#chains-and-light-maps-particlescreatorschainlightmapcpp-graphicsrendererchaincpp).

### Tests and screenshots

- `test_lightning`: the registered classes, the properties of `ParticleChainCreator` and `ParticleLightMapCreator`, the UV
  per segment and, with `OPENBLACK_GAME_PATH`, the real files `SF_LightningBolt`, `SF_LightningBoltPUTwo` and
  `SF_LightningStrike` (groups, counters, `SplitAngle`, `ForkScale`, the size of the light map's `.raw`); also
  `drawOffsetLT` (the weight clipping and `GetOffset`), `trunkJointsFollowTheHand` (each trunk joint
  with the step origin and weight 1 − i/(n−1)), `chainTexturesOverride` (`NumTexturesToTile`) and `twoBoltsClash` (two
  effects: the new one stops at the collision point, the old one continues with an opaque fork of scale ×3 and its strike of strength
  2 reaches both spells) and `landCutsTheFork` (with land at height 30 —20.1 m— between the hand at
  30 m and the ground point, the fork is cut and there is no strike; with land at 0, there is). `test_land_raycast` (7 tests)
  covers `RayCast` / `RayCastCells`: the slope, the ray that goes past its end point, above, the fall onto flat ground, the
  y = 0 near or far from the camera, the diagonal walk and the clipping off the map.


## Shields (`Magic/Spells/SpellShield`, `Magic/Objects/MapShield`, `Particles/Rules/Shield`)

Verified in `runblack.exe` while porting it. MAGIC_TYPE SHIELD (19) and PHYSICAL_SHIELD (20) use
the same class `SpellShield` (SpellWithObjects, 0x10C bytes), which makes a `MapShield` object: `MagicShield` (invisible;
the dome is the spell's PSys, SF_DefenseSphere) or `PhysicalShield` (the solid mesh MSH_S_SOLID_SHIELD 554).

### The spell (`SpellShield.cpp`)

- `InitWithPos` 0x72B5F0, in order: the radius (castData +0) is clipped **first from above and then from below**
  (`maxRadius` 1000 if it is not smaller, then `minRadius` 5 if it is not smaller) and rewritten into castData; `Spell::InitWithPos`
  (the magnitude stays in the radius); the REACT_TO_MAGIC_SHIELD (13) reaction of the spell's player with radius r + 30
  (reaction +0x3C, `reactions::SetRadius`); the nearest town less than 500 m away (0x43FA0000, `push` at 0x72B683; `MapCoords::GetNearestTown`
  0x6020E0, all the towns of all the players and of the neutral one); **an anti ring with radius = the magnitude for each
  other active player** (`GGame::GetNextActivePlayer` 0x5508D0: the seven slots with +0x8E0 ≠ 0; here the players
  that the land created); and `MapShield::Create` 0x72BE20 in the object list. If `Spell::InitWithPos` fails, the
  original does everything else anyway (and then deletes it).
- `SpellShield::GetMagicInfo` 0x72B820. The ctor 0x72B4B0 → `fn_0072B3C0` puts the spell in the list and sets +0xF4 /
  +0xF8 / +0xFC = 0. The list: head 0xDA07F0, linked through +0x100, count 0xDA07F4, newest first. The curl is read
  through `fn_007202D0` (Spell +0x98).
- `Process` 0x72B750: releases the strike reaction (+0xF4) when it is no longer there, and `SpellWithObjects::Process`.
- Cost 0x72B7F0: `costPerGameTurn × (magnitude / radiusForNormalCost)²` (20 and 22 × (r/30)²; with r = 40, 35.6 and 39.1
  per turn); `divideCostsByTribalPower` = 1 divides it in `PayFor`. Timers: player -1 (lasts while it is paid for),
  creature and computer 20 s, one-off 40 s.
- `CloseDown` 0x72B840 = `SpellWithObjects::CloseDown` 0x721300: `CoreCloseDown` and, since
  `GetSetObjectsDyingOnCloseDown` (0x55CF50) gives 1, `SetDying` (vt 0x6A4) on each object. `ToBeDeleted` 0x72B500: out
  of the list 0xDA07F0, its reactions go, +0xF8 = 0, `SpellWithObjects::ToBeDeleted` 0x720FD0 (CloseDown and the
  empty list) and the `ToBeDeleted` of each ring.
- `UpdateStruckReaction` 0x72B780 (vt 0x51C): reaction 35 the first time; afterwards it only refreshes its turn
  (reaction +0x2C, `reactions::Stamp`). `SetUpDestroyedReaction` 0x72B7C0 (vt 0x520): removes the 13s it started,
  +0xF8 = 0 and creates the 36. In the `Spell` base both are empty (0x55CE10 / 0x55CE20). `SpellOps` now has
  `updateStruckReaction` / `setUpDestroyedReaction` and `SpellHitSpell` (fn_00720B70) calls them on the struck spell.
- `IsUnder` 0x72BD20: `dist(p, castPos) < GetRadius() − margin` (x, z). `fn_0072BA00` (for `SPELL_AT_POINT`): the first
  available shield whose magnitude covers the point; its mask is tested as `(bit | mask) != 0`, so it does not filter.
  - `fn_0072BA00` (reached from `GScript::SpellAtPoint` 0x70C5F0 / `fn_007217A0` with mask 3) measures
    `GetDistanceInMetres` from **originalCastPos (+0xC0)**, not castPos (+0xCC), and needs Get2DRadius (the magnitude)
    strictly greater than that distance (0x72BA43..0x72BA5B). The type bits are 19 → 2 and 20 → 1.
- Warning for the other classes: in a `Spells/*.cpp`, a function of the anonymous namespace called `CloseDown` and registered
  as `ops.closeDown = CloseDown` inside `openblack::magic::Register...()` **resolves to `magic::CloseDown`** (the
  dispatch), which calls itself endlessly: the game hangs when the spell closes. Here it is called `ShieldCloseDown`.

### The object (`MapShield.cpp`)

- List `g_game +0x205CA4`, the newest first, count +0x205CA8, linked through +0x5C; `MapShield` ctor 0x72C070
  (`FixedObject(pos, info, 0, 1)`).
- The mesh of both shields is 0x22A (554): `GMapShieldInfo` 0xDA05D0 / 0xDA06D8, `MapShield::GetMesh` 0x72C1B0. The
  constants of `ProcessShield` / `DrawShield` are at 0x9828B0..0x9828D0 (the die factor 1.5 at 0x9828C8).
- `ToBeDeleted`: `PhysicalShield` 0x72CC50 deletes its PSys, then `MapShield::ToBeDeleted` 0x72C0F0 takes it out of the
  list.
- **MagicShield** (fn_0072C250): `SetScale(0.017 × r)`; `Draw`, `DrawShield` and `ProcessShield` empty; `SetDying`
  0x72C320 deletes it immediately (3); `IsEffectReceiver` 0; does not interact with physics. `MagicShield::ProcessShield`
  0x72C2E0 is empty; `InteractsWithPhysicsObjects`: PhysicalShield 0x72D600 = 1, MagicShield 0. The magic shield is
  static (`MagicShield::Get3DType` 0x72C340 → `Object::Get3DType` 0x6364F0).
- **PhysicalShield** (fn_0072C9F0 after the zeros and ones of fn_0072CB70):
  - `creationTurn` = the turn; **`SetScale(1.0)` and the height `shieldHeight + raiseWithScale × 1.0` with the 1.0 that
    `finalScale` still has**; `startSpin = clamp(Spell +0x98, ±3)` — **Spell +0x98 is the `curl` of the
    spell's PSysProcessInfo** (+0x64 + 0x34; the curl of the script or of the interface); `endSpin = ±0.15` with the sign of the
    initial one (0 counts as +); `startScale = 0.017 r × 0.01`; `finalScale = 0.017 r` and `SetScale(finalScale)`
    (`finalScale` is set through `fn_0072D5E0`).
  - **The physical shield follows the land:** `PhysicalShield::Get3DType` 0x72CE50 = 1 (a morphable 3D object).
    `UpdateMelting` runs at creation (`CallVirtualFunctionsForCreation` 0x72CD23, `SetUpPhysOb` 0x72CEB8) and again on
    every `DrawShield` after the interpolation (0x72D01E), so it follows the land as it grows.
  - fn_0072CD40: the current and previous matrix and scale, **two `ProcessShield` to prime it** and the effect
    SF_PhysicalShieldFX (PARTICLE_TYPE 0x43) at `(x, ground + height, z)` with magnitude 1, then `SetPlayer`,
    `AddTarget(the shield)` and `SetMagnitude(radius)`. The shield advances it itself each turn with a PSysProcessInfo of
    zeros, strength 1 and active (vt 0x100 in `ProcessShield`).
  - `ProcessShield` 0x72D190 (from `Spell::ProcessSpells` via fn_0072BF80, before the maintenance):
    - `t` = (turn − creation) × 0.1 s. Before 0.5 s, **A = 1 − t/0.5: the shield is born at full size and shrinks
      while it is not drawn**, and the spin does not advance. Afterwards, `u = t − 0.5`: A = x + x² − x³ with x = u/1.5 (1 after
      1.5 s) and the spin `startSpin + (endSpin − startSpin) × B`, B = y + y² − y³ with y = u/6.
    - Dying: `dieTime += dt`; after 1.5 × 1.5 = 2.25 s it is deleted; otherwise, A × (1 − clamp(dieTime/1.5)).
    - scale = start + (final − start) × A; `bob = fmod(bob + 1.3 dt, 2π)`.
    - Height (+0x1C) = `shieldHeight + raiseWithScale × finalScale + (sin(bob) + 1) × bobMagnitude × scale × 0.5`:
      **the bobbing goes with the new scale** (not A); with the data 0, -2 and 3 the shield is between -1.36 and
      +0.68 m relative to the ground at full size.
    - `SetScale(scale)` only if it changed from the previous turn and **differs by more than 0.3 (double 0x900C70) from
      `GetScale`**: the collision scale goes in jumps behind the drawn one (0.68 → 0.276 when shrinking, then
      0.597 when growing, for r = 40).
    - The matrix: identity × scale rotated by the angle in Y (rows 0 and 2), at `(x, ground + height, z)`: diag(scale)
      (0x72D438..0x72D4BA), turned about Y by the angle at +0xEC inline, as `RotateY` 0x5198F0 does
      (0x72D4DB..0x72D558: r0' = c r0 + s r2, r2' = c r2 − s r0).
  - `DrawShield` 0x72CED0 (each frame, via fn_0072BF50): scale and matrix interpolated between the previous turn and
    this one by the turn fraction; alpha (+0x70) = `max(40, min(1, strength) × 255)`; **it is only drawn after 0.5 s**, with
    `SetGlobalAlpha(1)` and the white tint (the alpha of its own texture is seen: in openblack the `Alpha` component = 1, or
    0 before 0.5 s). The PSys alpha (+0x14 → +0x6C, `Effect::SetGlobalAlpha`) is the shield's and, when dying, × the
    clip of dieTime/1.5 **which the code does backwards**: 1 until 1.5 s and 0 afterwards.
  - **Mesh material (`PhysicalShield::CallVirtualFunctionsForCreation` 0x72CCB0, ported):**
    `fn_0057E220(obj3D vt+0xF8, 5, 0xD)` (0x72CCCD) and `(…, 4, 0xD)` (0x72CCE5); vt+0xF8 of the vtable 0x9A32A0 is
    fn_007F9E70 = `[this+0x7C]`, the LH3DMesh. fn_0057E220 walks **all** the submeshes (+0xC / +0x10) and their
    primitives (+4 / +8) and changes only the dword +0 (type) if it equals `from` (0x57E252..0x57E256): neither byte +5 (sides,
    WRAP) nor ALPHAREF. In MSH_S_SOLID_SHIELD (554) submesh 1 (inner layer, AlphaTextured 4) becomes
    AlphaTexturedAlphaAdditiveNz 13 = mode fn_0082ECD0 (SRCALPHA / ONE, no alpha test, **no Z write**); no
    primitive is of type 5. Submesh 0 (outer, TexturedChroma 9, ALPHAREF 200) does not change: with the table 0xC387C8 it goes to mode
    15 (fn_0082E470, SA/ISA, alpha ≥ 200, writes Z). It changes the shared mesh, for all physical shields, like
    the original. In openblack: `L3DMesh::ReplaceMaterialType` / `L3DSubMesh::ReplaceMaterialType` (type and fields of
    `k_MaterialTypeLut[to]`; `twoSided`, `wrap`, `uvOffset` and `alphaCutoutThreshold` remain) from
    `map_shield::Create`. Before, the inner layer was SA/ISA and also wrote Z with alpha 0: murky orange holes,
    a double net and pieces of the back wall missing with straight triangle cuts. Now the holes add their
    pinkish glow and the back wall looks the same in all of them. The drawing order of the submeshes within the object (submesh 0 first) is still
    **(inferred)**; with the inner one additive and without Z it hardly matters.
  - `SetDying` 0x72D170: dying and without a spell; it fades by itself (shrinks in 1.5 s and is deleted at 2.25 s).
  - Physics: `InteractsWithPhysicsObjects` 1, `GetAlwaysRemainsInPhysicsInternalSystem` 1 (it stays in the system even if
    nothing moves nearby: `PhysicsObjects` adds it at each `BeginTurn`), constants of row 10, weight
    `GMapShieldInfo.weight` = 50000 × scale³ (`Object::GetWeight`). The collision mesh is the physics submesh of 554
    (28 vertices), at the object's scale.
  - `ReactToPhysicsImpact` 0x72D610: if whoever hit it (`GetGameObjectWhoHitMe`, the striker) is available,
    `PhysicallyDestroysAbodes` and the spell has strength: `SpellEvent 5` at its MapCoords (x, z, its height above the
    ground) without a target, `PayFor(|v| × mass × chantCostPerImpactMomentum (25) × 0.0001, forced)` with the speed and
    mass of the striker, `Town::UpdateAggressor` (not ported) and the strike or destruction reaction.
    The hitter is `GetGameObjectWhoHitMe` 0x644F00, `PhysicallyDestroysAbodes` is vt 0x7A8, and the payment is
    `fn_0072B830`. When the thrown object has no player, `Town::UpdateAggressor` takes the interface's player (g_game
    +0x205A5B, 0x73C9C7).
- `IsPointDefinietlyWithinShieldVolume` (vt 0x870): MagicShield 0x72B850 is a 3D sphere with radius the spell's
  magnitude; PhysicalShield 0x72B8E0 a cone of its `Get2DRadius` R: `d² < R²` (x, z) and **the height of the point above the
  ground (MapCoords +8, relative)** less than `H (1 − d/R)`, with H = `GetHeight` 0x638120 (mesh +0x28 × scale ×
  2). `fn_0072B990` (`Reaction::ApplyReactionToLivingObjectsAtSquare`): a living thing within the 2D radius of a shield
  ignores a reaction whose origin is not inside it → `map_shield::IsReactionBlockedByShield(living, origin)`, for
  the reaction spreads of the villagers (the fire one has its `TODO` in `VillagerFire.cpp`).

### The villagers' reactions (`ECS/Systems/Implementations/VillagerShield`)

Ported (fully read in the exe). Reaction 13 is created by the spell when cast and is
spread **only once** (`SpreadReaction`), so only the villagers who are nearby at that moment get a chance.

- **Fix to the spreading**: the initiator of reactions 13 / 35 / 36 is the **spell itself**, and in openblack a
  `Spell` entity has no `Transform`, so `reactions::SpreadReaction` cut off before starting and **no
  shield reaction reached anyone**. `Reactions.cpp` now reads the initiator's position with `MapPosOf`: the
  `Transform` if it has one and, otherwise, `components::Spell::position` (+0x14, which is what `Reaction::GetPos`
  0x6E45C0 returns).
- `Villager::ReactToMagicShieldPriority` 0x765BB0: 0 if the initiator is not a `SpellShield` (0x765BCD) or is not
  available (vt 0x2C, 0x765BDD); **without a town it already returns the priority** of ReactionInfo[13] (+0x10, 0xD4FBD4,
  0x765C08 → 0x765C48); with a town, 0 if `TownDesire::GetDesireSignificanceToVillager(town +0x34,
  TOWN_DESIRE_FOR_PROTECTION 3)` 0x746660 is 0 (`test ah, 0x40`) or if `turn − town +0xEB0` (the turn of the last
  aggressor) exceeds `GVillagerInfo` +0x364 (`numGameTurnsAfterAggressionInterestedInShield`). The distance it computes at
  0x765BEF **is discarded** (`fstp st(0)`): it has no influence.
- `Villager::SetupReactToMagicShield` 0x765C60: `AddReaction(reaction, 168)` (vt +0x990 → `Living::AddReaction`
  0x5F0F30: stores the reaction and pushes the final state) and +0xBC = the spell; if it is **not** already under the shield
  (`SpellShield::IsUnder(its position, 0.2 R)` 0x72BD20, 0x765CC4) it walks with `SetupMoveToWithHug` to
  `centre + GetPosFromAngle(heading centre→it ± π/8, 0.8 R (1 − random³))` with final state 168 —that is, it goes
  underneath, biased towards the edge—; and in both cases it leaves the look-at point (+0x10C, `JustWholeMapXZ`) **1 m from itself
  in that same heading**, i.e. looking out of the dome (0x765D8B..0x765DEF).
- `Villager::AmazedByMagicShieldReaction` 0x765E00 (row 168 of the state table; its exit is the thunk 0x5B0100 =
  `Villager::ExitReaction` 0x7527A0): with a town, the spell still available and the protection desire > 0
  (`test ah, 0x41`), `LookAtPos(+0x10C, 0)` one step per turn and, once it is looking, 1 time in 4 (`GameRand(4)`) a
  new point; the state's clip (`AnimFn::AmazedByShield` = `Villager::AmazedByShieldAnimation` 0x4240C0) is kept for
  `(GameFloatRand(10) + 20) × 1000 / duration` loops (`IsReadyForNewAnimation` 0x5EC960) and then drawn again,
  unless it is 286 INTO_POINTING, which after one loop moves to 395 TALKING_AND_POINTING (0x765F55..0x765F8D). If the
  town, the shield or the desire is missing: `SetupWaitForCounter(ftol(GameRand(60) + 20), 163)` 0x76B060 (0x765FC0), with which
  row 57 WAIT_FOR_COUNTER is also ported (`Living::WaitForCounter` 0x5EC310).
- `Town::UpdateAggressor` 0x73C9B0: **only the record** is ported (+0xEAC the aggressor player and +0xEB0 the turn,
  0x73CA82 / 0x73CA98; `components::Town::aggressor` / `aggressorTurn`), which the physical shield's impact writes
  (0x72D74F..0x72D788: `EffectValues(2, 0, the hitter, 1.0, PhysicsObject::GetPlayer 0x647460)` and value 0). Not
  ported: the per-player aggression slots (`fn_0073E0F0` on town + n × 0x80 + 0x9F4, with the value + GTownInfo
  +0xAC when the slot is at 0 and × the weight +0xEB4 / +0xEB8, which then decays × 0.9), the `TownAttackSFX` 0x71B7C0 and the
  help spirit's guidance about a creature or a town attack (0x73CAAA..0x73CB29: not a creature mimicry, see
  [creature.md](creature.md#the-emergencys-creature-part)).
- `MapShield::CreatureMustAvoid` 0x72C170 (vt 0x614): the condition is ported (not controlled by script, +0x24 & 0x400, and
  player different from the shield's; if the shield no longer has a spell, +0x60 = 0 at 0x72C155, its player is that of
  `GameThing::GetPlayer` 0x570130 = the interface's, g_game +0x205A5B, openblack's PLAYER_ONE, not NULL); since openblack has no creature, its player comes in as a parameter and **nobody calls it
  yet** (in the original it is used by the creature's path).
- `GetImpressiveValue` **is still not ported** because there is nothing that asks for it (the impress / belief system is not
  there): `SpellShield::GetImpressiveValue` 0x72BA80 gives 0 for a villager whose town has the shield's player as aggressor (+0xEAC)
  if the reaction is 13 or 35, and otherwise `Spell::GetImpressiveValue` 0x721630, × 4 when it is
  36 (0x72BAED); `PhysicalShield::GetImpressiveValue` 0x72D7F0 is the same with 13 and with base
  `Object::GetImpressiveValue` 0x639860, without the × 4.
- What has not been ported from the spreading: replacing the reaction a villager is already following with another that scored higher
  (0x6E4134, `reactions::MaySwitch`), the same as in fire and teleport; and reactions 35 (strike) and 36
  (destruction), which no living thing attends to yet.
- The two inputs that openblack does not have (the town's protection desire and, while nothing else acts as aggressor, the
  aggressor's turn) leave the reaction **unreachable for a villager with a town**; to see it in the game there is the
  hook `OPENBLACK_TEST_SHIELD_REACTION=1`, which treats both as satisfied.

### The particles (`Particles/Rules/Shield.cpp`)

- **`DefensiveShield` registry** (list 0xD4EE48, `PSysShield.cpp`): `UR_AddDefensiveSphere` 0x6A2A60 creates the first
  time a `DefensiveSphere` (fn_006D0B20) at `GetCurrentParentPos` (the effect's origin, in SF_DefenseSphere) with the
  provider's radius (magnitude × 1.11062); afterwards it only follows the radius. It is deleted with its collection (the `~Effect`).
- `DefensiveShield` (PSysBase, ctor 0x6D0A60: +0x14 prev, +0x18 next, +0x1C the owning PSysManager) and its only kind
  `DefensiveSphere` (0x30 bytes, fn_006D0D20): TSphere {radius +0x20, centre +0x24}. It is removed with fn_006D0B70 from
  UR_AddDefensiveSphere's CollectionData dtor fn_006A2A30.
- The sphere vtable: vt 0xFC `FindIntersect` 0x6D0D50 (→ `GJUtils::FindIntersect`), vt 0x100 `DeflectOffShield`
  0x6D0D80, vt 0x104 `HasCrossedIntoShield` 0x6D0DC0 (`to` inside, `from` not), vt 0x108 `IsPointInShield` 0x6D0DA0 (→
  `GJUtils::PointIsInSphere` 0x57C9F0); the list helpers fn_006D0BC0 / fn_006D0BE0 (first sphere containing a point)
  and fn_006D0C20 / fn_006D0C40 (first sphere a move crossed into).
- `UR_AddDefensiveSphere` (+0x20 SphereRadius, IsMagical): the first run is marked in CollectionData +0x20; the centre
  never moves.
- **API for the fireballs**, in `Particles/Rules/Shield.h`, namespace `psys::shields`:
  `DoAnyShieldDeflections(effect, atom, oldGlobal)` is `UpdateRuleGravityWithFloor::DoAnyShieldDeflections` 0x6A1FA0.
  The original calls it **at the end of each atom's update** (0x6A1F48) only with `CheckShieldDeflections`
  (+0x72), with `oldGlobal` = the atom's global position **at the start of its update** (0x6A1903). Margin =
  1.25 × baseScale × ruleScale; if the atom crossed into a sphere: the intersection (`FindIntersect`
  0x57CCD0, the radius + margin), a spark (`AddImpactTarget`, fn_006D0AF0), and a `SpellEvent 4 {impact, the last
  global movement, 1, target = the shield's spell}` to the atom's own effect; with 0 the atom stays at the
  impact (`GlobalToLocal`) and its velocity bounced (`v −= 2(v·n)n`, 0x57CFD0) and it returns true; with 1 it passes. In the
  spell, `ApplyDefaultSpellEffect` → `SpellHitSpell`: the shield pays strength × `costPerShieldCollide` (FIRE 600/800/
  1000) and the ball an event; if the shield still has strength, strike reaction and 0 (it bounces). There are also
  `FindShieldContainingPoint`, `FindShieldCrossedInto`, `IsPointInShield` (strict `< (r + m)²`),
  `HasCrossedIntoShield` and `SpellOf`. `UR_CloudMoverNew` (0x6D46D9) also calls the function.
- `ApplyDefaultSpellEffect` with `event +0x20` (`checkShields`, 0x720D19): with a sphere that contains the point (margin =
  the effect's radius) it sends itself an event 4 with itself as target; with 0 the event is cut, otherwise a spark
  at the point. Ported as is (the intention is not clear).
- `SpellTargets` also stores **points** (12 bytes, +0x14): `Effect::AddTargetPoint` / `TakeTargetPoint`
  (fn_00670F00 takes a point first and otherwise an object, its position).
- `UpdateRuleShieldSpark` 0x6A2BF0 (an AtomCreateRule): for each impact, while there are fewer than
  `MaxNumAtomsForCollection`, an atom at the impact with its `SoundSpark`, three random angles and a number of waves
  2..5; those older than `SparkLife` are deleted. `ModifySubCollection` 0x6A2F90 draws with its N atoms **an arc from the
  centre of the sphere to the impact**: atom i (from N to 1) at u = i/N at u R along the direction of the impact,
  deviated R × WiggleAmpl × sin(k u π) sin(4t + a3) and sin((k+1) u π) sin(2t + a3); scale 0.04 R (1.5 + 0.4 (1 +
  sin(a3 − 6t))), alpha (1 − t/SparkLife) × 510 between 0 and 255.
  - Properties: +0x20 NextGroups, +0x28 PCreator, +0x2C SphereRadius, +0x30 MaxNumAtomsForCollection, +0x34 SparkLife,
    +0x38 WiggleAmpl, +0x3C SoundSpark. AtomData: three PSysFloatRand(2π) at +0x20..+0x28 (0x6A2CF7..0x6A2D16), the wave
    count +0x2C = ftol(PSysFloatRand(4) + 2) (a float draw, not PSysRand; 0x6A2D1E..0x6A2D31), the orientation +0x30
    rebuilt from the position each step (the spark atom never moves).
  - The orientation fn_006A2D34 / fn_006A3AA7 give an atom from a point about the sphere's centre: the identity turned by
    the colatitude π/2 − atan2(y, |xz|), then by the yaw atan2(z, x); the Y row ends up along the point's direction.
- `UR_InitialSpin` 0x69E490: when the effect has a player, spin = curl (manager +0x58 = PSysProcessInfo +0x34) ×
  ScaleAngularVelocity between ±MaxAngularVelocity, which fades in TimeToFade; rotates the atom about its Y. Ctor
  0x6BF450: ScaleAngularVelocity 1.0 and MaxAngularVelocity 1.0 (0x6BF47B..0x6BF483), TimeToFade 2.0 (0x6BF48C); the
  editor range 0..1 of the three properties is not applied; SF_DefenseSphere.txt gives 1, 5 and 5.
- `UR_VapourEndEffect` 0x6A39E0: the surface patch is placed on its parent (the sphere's tracer), oriented the
  first time with its Y outwards and afterwards rotated by how much the parent moved (axis last × current, angle acos);
  scale = ScaleFactor, **alpha = age × 30** (full at 8.5 s).
- **The spinning dome**: the original interpolates each frame
  **the whole 3×4 matrix** of the atom (fn_00679C30, from fn_00679920 0x6799F9), rotation included; openblack drew the
  interpolated position with the new turn's rotation and every 0.1 s the caps twisted abruptly (the spring). It is now
  interpolated (`Effect::CollectCollection`, PSys.cpp). Checked with `OPENBLACK_TEST_SHIELD_FRAMES` (SHIELD r 40, curl
  5, 2.5 s): from a fraction 0.94 to 0.07 of the next turn only the alpha rings slide; within the turn (0.07 → 0.94) the dome moves as a whole and smoothly. What remains when spinning is from the original: 15 additive caps (mode 13, no Z write, Z test
  only against the world, two-sided, per-patch ordering which in additive mode changes nothing) that overlap 2-3 times, with
  a curvature that does not match the sphere and 4-bit DXT3 alpha rings; when they move the sums change where they
  overlap. The per-vertex light (fn_00858BA0) is still **(approximate)** and the original's fraction goes from 0 to 5
  (0x6799A8..0x6799DF) versus 0..1 in openblack (it only matters if a frame takes longer than a turn).
- `SetCollectionAlpha` 0x6A2720: collection alpha = clamp(provider, 0, 255) (in the dome, strength × 255, 40..255).
- `UR_AtomsAtEPTarget` 0x69A960: takes an object target the first time and makes one atom per extra point of its mesh (5
  if it has none; MSH_S_SOLID_SHIELD has none, so the five go to the object's position); if the object goes away, it deletes
  its atoms.
- `CheckShieldDeflections` 0x6A2570 (atom rule, no file uses it) ported except `MoveToBaseGroup`.
- **`UR_SphereSurfaceTracer` 0x6A32B0 corrected** (PSys.cpp): the radius property is `ScaleSphereRadius` (not
  `SphereRadiusFP`, which does not exist: the dome came out at 1 m), the angles with `fmod 2π`, the alpha `Alpha × ScaleAlpha`
  and, outside a hierarchy, + the parent's position.
- **The tracer's `OrientToSurface` ported** (+0x44, 0x6A351D..0x6A35C4; only SF_DefenseSphereInHand and
  SF_DefenseSphereOnHolder set it to 1, the SF_DefenseSphere dome has it at 0). Each patch is rotated outwards from the sphere at
  its (θ, φ): `fn_006743A0(π/2 − φ)` (rows (cos a, −sin a, 0), (sin a, cos a, 0), (0, 0, 1) and the fourth row at 0) and
  then in each row (x, z) → (c x − s z, c z + s x) with c, s = cos θ, sin θ (= `affine::TurnRows(axis 1)`, was `lh_matrix`); the patch's local
  Y axis ends up on the normal. Without it the 15 MSH_S_SPELLBALLSURFACE02 patches of the in-hand effect
  (`particleTypeInHand` 65 of `GMagicShieldInfo`[0], `SF_DefenseSphereInHand`: a root atom that follows the hand with
  `SetScale` = `RenderHandScale` × 3.5 and `CreateRuleSphere` of 15 patches in group 1) all had the same
  orientation and looked like many pieces spinning in a ring; with it they form **a single ball** that spins in the hand,
  like the original.

The correction of the PSys hierarchies that the dome needs is in
[Correction in the PSys core: the hierarchies](particles.md#fix-in-the-psys-core-the-hierarchies).

### Hooks and screenshots

- `OPENBLACK_TEST_SPELL` accepts `curl` (the seventh value); `OPENBLACK_TEST_SHIELD_SHOT` captures by turns
  ([openblack-internals.md](openblack-internals.md#debug-environment-variables)).
- `test_shield`: clipping, cost, curves, sphere helpers, registry and deflection with a real effect, hierarchy frame and,
  with `OPENBLACK_GAME_PATH`, the real rows (5 / 1000 / 30, 25, 0 / -2 / 3, 20 and 22, weight 50000) and
  `physicalShieldMaterialTypes` (loads 554 from AllMeshes.g3d with bgfx Noop: after (5, 13) and (4, 13) submesh 1 is
  type 13 additive without Z, two-sided and WRAP; submesh 0 stays type 9 with threshold 200/255 and Z).
- `OPENBLACK_TEST_SHIELD_FRAMES="<turns>,<n>,<prefix>[@<slow>]"`: n screenshots in consecutive frames from those
  turns after the first shield (`<prefix>_<i>.png`; the log gives the turn and its fraction). Each screenshot stops the frame
  ~0.45 s at 1600×900: with `-W 640 -H 360` two come out per turn (fraction ~0.07 and ~0.94). `@<slow>` multiplies the
  turn duration; the PSys interpolates with the game's fraction (`game_clock::TurnFraction`, g_game +0x205D64), so
  it also goes slowly. MapShield's own fraction still uses the wall clock (pending:
  [engine-math.md](engine-math.md#game-clock-1)).

### Not ported / UNVERIFIED

- (Ported, above) the villagers' reactions to the shield and the `CreatureMustAvoid` predicate. Still out: the
  creature's path that uses it (0x54AF60), reactions 35 (strike) and 36 (destruction) —nobody attends to them—,
  `GetImpressiveValue` 0x72BA80 / 0x72D7F0 (no impress system to ask for it) and all of `Town::UpdateAggressor`
  except its record. Pending in the towns: the town's protection desire
  (`TownDesire::GetDesireSignificanceToVillager` 0x746660 needs the three desire tables, TownDesire +0x90 / +0xD4 /
  +0x118, which are not there) and that some other aggression (damage, fire, buildings) writes the aggressor; until then only
  villagers **without a town** react (or with the hook `OPENBLACK_TEST_SHIELD_REACTION=1`).
- The shields are not in the map's object grid: `ApplyEffectToMapPos` does not reach them (the physical one would accept
  non-burning effects, `IsEffectReceiver` 0x72CC80; the magic one none).
- `CallVirtualFunctionsForCreation` 0x72CCB0: the base `SingleMapFixed::CallVirtualFunctionsForCreation` 0x52E880 and
  the path links (vt 0x78 / 0x80 / 0x88 / 0x98 / 0x1E8 on [esi+0x40], fn_00644DF0 if (obj+0xA & 1) == 0).
  The material change `fn_0057E220` is ported (above).
- The drawing of the dome patches is that of `Creators/Mesh.cpp`: **resolved** (player colour with blend
  0.5, additive by the ctor's `MeshChangeMaterialProps`, and `DrawCutByPlane` also clips the static mesh, fn_0080C050: `mesh_atoms::Instance::cutByPlane`); see
  [Particle meshes](particles.md#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome). The tracer's `OrientToSurface` is already
  ported (above); `MoveToBaseGroup` (already in the core) is still unused here.
- The meshes' extra points are not loaded (`UR_AtomsAtEPTarget` uses the object's position, exact for 554).
- `Get2DRadius` / `GetHeight` come from the mesh's box. **The same as the original** (already fully checked): `LH3DMesh::ComputeBoundingBox` 0x8081B0 walks all the submeshes (+0xC / +0x10, the
  physics one of 554 included) on load (the header brings it zeroed) with `LH3DSubMesh::ComputeBoundingBox` 0x87FB20 →
  `LH3DPrimitive::ComputeBoundingBox` 0x807F30 (the first point comes out as the maximum and the second as the minimum, 0x807F97..), and
  +0x24 / +0x28 / +0x2C end up being the **half extent**: (max − min) and then × 0.5 (0x80833A / 0x808349 / 0x808355;
  +0x30 is its magnitude). That is why `Object::Get2DRadius` 0x638180 = scale × max(half X, half Z) and `GetHeight` 0x638120 =
  scale × half Y × 2 (`fadd st, st` 0x63813D) = the whole height, which is exactly what openblack does with
  `GetBoundingBox().Size()` (× 0.5 for the radius, as is for the height).
- (Corrected) the fireball now calls `DoAnyShieldDeflections` (`Particles/Rules/Fireball.cpp`).
- The two equal sort keys (the physical shield and its SF_PhysicalShieldFX at the same point) with a non-stable
  `std::sort`: what the Z-sorter does with ties is still unread (0x82F280 turned out to be the callback loop of
  `FinishFrame`, not the ordering; 0x82F460 must be read). It belongs to the common rendering.
- The fast rock (33 m/s) that passes through the physical shield's shell **is not proven to be a deviation**: the
  original uses the same model that openblack ports —20 substeps of 5 ms per turn (`GameTurnUpdate` 0x646046), per-vertex
  contact against the faces of the other body (`CollideVertices` fn_007FDA60 with the centre→vertex segment,
  fn_007FBF80) and springs from `PhysicsConstants.txt` row 10 (`GetPhysicsConstantsType` 0x72D7E0)—, and the mesh branch of
  fn_007FDD60 (the other body with +0x168, fn_008683C0) is not used by any openblack body. The strike **is detected**
  (it pays chants), but the spring does not brake 33 m/s in the two substeps the contact lasts. A screenshot of the
  original is missing to compare; nothing in Physics has been touched.
- In the path of objects with `Alpha` (`Renderer.cpp`), a Standard primitive with `depthWrite` = false (types 6,
  7, 8) still writes Z and does not switch mode table (0xC387C8) per primitive; 554 is no longer affected.
- `Primitive::modulateAlpha` (α = texture × diffuse of modes 3, 5, 6, 8, 10..13, 15, 16) **is read by nobody**:
  `fs_object.sc` always multiplies the alpha by the object's. After the material change the inner layer of 554 is
  of type 13, which is × diffuse, so for the shield it matches the original.
  The alpha test of the outer layer is faithful: the original scales ALPHAREF by the object's alpha (mode 15,
  0x82E579..0x82E5AE: `al = material+4`, `fild`, `fimul` by the alpha byte, `fmul 1/255`, `fsub 5`) and the shader
  compares the unscaled texture alpha against the threshold − 5/255, which is the same.

## Teleport (`src/Magic/Objects/MagicTeleport`, `src/ECS/Systems/Implementations/VillagerTeleport`)

TELEPORT is
MAGIC_TYPE 12; its class is `SpellTeleport` (a `SpellWithObjects`, 0xF4). Each cast leaves an **invisible teleport
stone** (`MagicTeleport`, a MobileStatic → MultiMapFixed) with a particle pool
(ParticleType 73 `SF_TeleportVortex`). A player's stones form a list (GPlayer +0xA58 head, +0xA5C
count; here `PlayerMagic::teleportStones`). A living thing that moves and enters or is dropped on a stone comes out through the
stone of the same player that brings it closest to where it is going.

### The spell and the stone

- `SpellTeleport::InitWithPos` 0x5FBEB0: `MagicTeleport::Create(pos, this)`, the stone enters the spell's object list
  (+0xEC/+0xF0) and then `Spell::InitWithPos`. The spell **has no particle type** (the effect belongs to the
  stone), so InitWithPos sends `SpellEvent 11` (`particleType == 0`) and creates no PSys of its own.
- `GMagicTeleportInfo` vt+0x30 (CanCast at pos) 0x5FBE50: fails if there is a MultiMapFixed (building, field, another
  stone...) less than `fn_005FCCA0` = **6 m** away (`fn_00604C30` with the AsMultiMapFixed predicate); otherwise,
  `GMagicInfo::CanCast` = 1. In openblack `cast_rules::CanCastAt` calls it `teleport::AnyMultiCellStaticNear` (was `AnyMultiMapFixedNear`), which is
  `ecs::map_cells::FindNearestInSpiral(pos, IsMultiCellStaticClass, 6) != null` (spiral, `d < r` and cutoff
  `1.5·best + 10` like fn_00604C30); the stones are in their cells (hook `InsertMapObject` in `teleport::Create`,
  MultiMapFixed 0x52E890+0x184), so they are no longer looked up in the players' lists. Since
  `SPELL_AT_POS` checks nothing (neutral creator, flag at 0), the script plants the stone anyway (the hook
  confirms it: "CanCastAt(A) now false" but the stone is created).
- `MagicTeleport::Create` 0x5FC1F0: `new MagicTeleport(pos, spell)` (ctor 0x5FC130: `MobileStatic(pos, 0xD3B614,...)`),
  `CallVirtualFunctionsForCreation` 0x5FC260 (creates PSys 73 at the world pos and `SetPlayer`), and
  `SetScale(GetScale()·0.01)`. The stone **draws no mesh**; `Draw` 0x5FCCC0 only sends an invisible hand collision
  of radius 3 while the spell has a seed (+0xAC; see "The hand and the stones"), and advances and draws the PSys with the frame time
  (that is why the vortex goes in `SetPerFrame`).
- The stone is `operator new(0xA4)`; its `GMobileStaticInfo` 0xD3B614 comes from `fn_005FC420`. `MagicTeleport::GetPlayer`
  0x5FC430.
- `GMagicTeleportInfo` vt +0x30 0x5FBE50 → `fn_00604C30` with the predicate 0x5FBE90 (`Object::AsMultiMapFixed`, vt
  +0x678) and no excluded object. It walks a spiral of max(3, ceil(2r/10))² cells, with `FindType(−1)` in each.
- Cost and timers (effect 12): costToCreate 5000, initialChants 2000, costPerGameTurn 1, costPerEvent 1,
  `divideCostsByTribalPower = 1`, `costPerKilometer = 200` (GMagicTeleportInfo +0x58). Player timer **-1**
  (the stones persist), creature/CP 25 s, one-off 120 s.
- **How long a pool lives (it is not a bug):** having no timer, it is closed by the chants. A dispenser orb seed
  gives it `initialChants` 2000 and the spell spends 1 per turn, i.e. about **200 s** (trace: 2000 → 1814 in
  186 turns), unless a player keeps paying (`CostToMaintain`). Each useful jump **gives back** chants
  (`PayFor` with negative cost), so a pool that is used lasts longer. Same as in the original.
- `MagicTeleport::ToBeDeleted` 0x5FC310: removes reactions, deletes the PSys, unlinks itself from the player's list and frees
  the list of travellers. `SpellWithObjects::CloseDown` 0x721300 sets each object to die (`SetDying` 0x4027A0 =
  `ToBeDeleted(0)`), so closing the spell deletes the stone. In the code the function is called `TeleportCloseDown`:
  with the name `CloseDown`, inside `magic::RegisterTeleportSpell` it resolved to `magic::CloseDown` (the dispatch)
  and the game hung when the spell closed (checked with `OPENBLACK_TEST_SPELL=TELEPORT,…,3`:
  it closes and is deleted at turn 31).

### Who uses a stone

- `MagicTeleport::ShouldLivingThingReact` 0x5FC590: the living thing moves (IsMoving vt 0x174) and there is another stone T of the same
  player with `1.2 · (|living−this| + |T−dest|) < |living−dest|` (0x8C6C98 = 1.2), with `dest = GetFinalDestPos`
  (vt 0x884) and `FastDistance` 0x74CE10 (fixed point, 6553.6 per metre: `max + min/2`).
  - `Object::IsMoving` 0x402710 (read): the current position (`GameThingWithPos::Pos` +0x14 x, +0x18 z) is not that of
    `Object::coords` (+0x2C, +0x30), which is the previous turn's; that is, **it moved in the last turn**, in
    any state. **(approximate)** openblack does not store the previous turn's position: `villager_teleport::IsMoving`
    checks that it has a movement tag other than ARRIVED and speed > 0 (before only the
    MOVE_TO_POS state counted, so a villager who was going to the worship site or to another reaction did not count).
  - `Villager::GetFinalDestPos` 0x756AD0 → `Living::GetFinalDestPos` 0x5EC1E0 (read): if it has a footpath and node
    (+0xC8/+0xCC), the last non-hidden node of the footpath (`GFootpath::GetEndNonHiddenNode` 0x535120 with the flag
    (+0xB4 >> 3) & 1); otherwise, `MobileWallHug::GetDestPos` (vt 0x860, 0x416F70) = **the goal +0x80, whether it moves or not**.
    openblack always uses the WallHug goal (faithful); **(pending)** the footpath branch, because no openblack `Living`
    walks on a `components::Footpath`. Note: a villager who has never walked has goal (0, 0) (like the
    +0x80 of the original after `SetToZero`), so a forced jump on it is computed against (0, 0).
- `Villager::ReactToTeleportPriority` 0x766200 = `(ShouldLivingThingReact ? 0xFF : 0) & the priority of reaction 20`
  (the REACT_TO_TELEPORT row of `ReactionInfo`). `SetupReactToTeleport` 0x766250 registers the destination in the stone
  (`GetFinalDestPos` vt 0x884 at 0x766297 and `fn_005FC6A0` at 0x7662A5), sets +0xBC = the stone and enters
  `GO_TOWARDS_TELEPORT_REACTION` (201) or its fast twin 251 (0x766380 is a `jmp` to the 201 one, so only
  the state table row changes: animation and speed index). **The condition, read (0x7662B2..0x7662CE):**
  `0xC9 + 0x32 · (speed > threshold)`, with the villager's own speed (`MobileWallHug` +0x5A, the u16 that
  `GetSpeedInMetres` 0x60C070 converts to metres) and the threshold `GMobileWallHugInfo` +0x10C, which is
  `speedGroup.speed2` of info.dat (the same field that `Living::FleeFromPredatorPriority` reads at 0x5F15ED);
  `setle` on the u16, so 251 only if it is **strictly** faster. Ported in
  `villager_teleport::SetupReactToTeleport` (a villager who was already running to the worship site leaves with 251: see the test
  below). `GoToTeleportReaction` 0x7662F0: on arrival (`AreWeThere`) it moves to
  `TELEPORT_REACTION` (202), otherwise `SetupMoveToWithHug(stone)`. `TeleportReaction` 0x7663F0 calls
  `DoTeleport(living, false)` and `StopReactingAndSetState`.
- The reaction is **spread only once**, when the stone is created (`Reaction::CreateReaction` with flag 0; `ProcessReactions`
  does not spread it again, its flag 0xD00DD4 is never set). That is why only the villagers who are already
  moving at that instant react (checked: the villagers wandering around the town react and jump).
- Dropping with the hand: `fn_005FC4B0` requires that the villager's player be the stone's and `teleportCount != 1`.
  `fn_005FC4F0`, in its exact order (read): `SetTopState(FLYING 10)` 0x5FC4FD, `fn_005DA0C0` (the interface leaves it at the
  stone's MapCoords) 0x5FC517, `SetTopState(LANDED 11)` 0x5FC51E, `DecideWhatToDo` (vt 0x8C8) 0x5FC52C,
  `GetFinalDestPos` (vt 0x884) 0x5FC549 → `fn_005FC6A0`, and `DoTeleport(forced)`; returns 1 if it jumped, otherwise 0x17.
  `villager_teleport::LandAt` now goes through FLYING and LANDED (both states exist in openblack's table: 10
  `VillagerCarried`, 11 `VillagerLanded`). **(approximate)** openblack's `DecideWhatToDo` only changes the state, so
  the goal registered afterwards is the one the villager already had.

### The hand and the stones (faithful)

Before, the hand could not do anything with a stone. Now:

- **The hand sees the stone.** `MagicTeleport::Draw` 0x5FCCC0: while the spell (+0x9C) has a seed (+0xAC,
  0x5FCD03), `SendInvisibleDrawCollision(stone, (x, GetAltitude + y, z), 3.0)` (0x5FCD18). 0x519960 projects the
  centre (`ProjectPoint` 0x819390; nothing behind the near plane 0xE839E0), measures the on-screen radius of a point at 3 m
  and, if the mouse falls inside the circle, `SendObjectDrawCollision(stone, depth)`. openblack:
  `HandSystem::PickObjectAlongRay` (`HandPlacement.cpp`) tests a 3 m sphere on each stone of
  `teleport::HandCollisionStones()`: the ray passes less than 3 m from the centre in the plane of its depth.
  **(approximate):** the distance it competes with is the length of the ray up to that plane (like openblack's
  meshes), not the projected w. A script stone (`OPENBLACK_TEST_SPELL`, `OPENBLACK_TEST_TELEPORT`) has no
  seed and the hand does not see it, like the original.
- **Pressing with a villager in the hand over the stone = applying it** (`HandApplyToObject.cpp`,
  `HandSystem::HeldActionPressedOnObject`). `ActionPressedHolding` 0x5D1560: target = `GetCreatureToGiveTo` (no
  creature) or the object under the hand; outside the influence (`[this+0x48]` == 0) and `InterfaceMustBeInInfluenceForInteraction`
  (vt 0x714, 0x4028A0 = 1) it goes to the no-target branch (0x5D15D6..0x5D15E9); then `ValidToApplyThisToObject` (vt 0x71C,
  0x5D1607). For a villager `ApplyOnlyAfterRecSystem` and `ValidForLockedApplyProcess` are 0, so
  `SendApplyToObject` 0x5D30D0 (0x5D1684): checks that the hand is holding something (status +0x90 = the hand's counter,
  **not** the influence as the verification said), repeats the validity (0x5D3126), packet 0x11 (0x5D32A7) and state
  0x12. The packet (0x5DA1A0) repeats the checks and calls vt 0x720 (0x5DA251): `Villager::ApplyThisToObject`
  0x752C40 → stone (0x752FC2) → `ValidToApplyVillagerDirectlyToTeleport` 0x752FEA → `fn_005FC4F0` 0x752FF8, which takes
  the villager out of the hand with fn_005DA0C0 (`RemoveFirstFromHand` fn_005CED60 and the stone's MapCoords) and jumps.
  `HandleApplyResult` fn_005DA100: 5 → nothing; 3 → consumed; if it is not 1 and the object is no longer in the hand, it is returned
  as is (the villager's case, 1 or 0x17); 0x16 → `RemoveFirstFromHand`; 0x17 → fn_005DA0C0 at the
  target's position; 0x18 → fn_005DC1E0. `fn_006E47C0(obj, v)` sets +0x30 = v (the "available" flag) on every reaction the object
  started (+0x14, list g_game +0x205BDC, next +0x40); `HandleApplyResult` calls it with 1 (0x5DA136); not ported.
  `fn_006E47F0` (from the put-in-hand `fn_005DC330`, 0x5DC38C) sets +0x30 = 0 on the object's reactions whose info asks
  `whetherReactionFinishesIfInitiatorInHand`. **Not ported:** the totem branch (`WorshipTotem`, the sacrifice at 0x752C40..0x752FB0): with a totem the
  press still sets up the drop; the `HelpProfile` 6/7 help.
- **Picking up the pool.** `MagicTeleport::ValidForPlaceInHand` 0x5FC440 = the vt 0x6FC of the spell's seed (0 without a
  seed); `InterfaceSetInMagicHand` 0x5FC470 = `GInterfaceStatus::PlaceObjectInMagicHand(seed)` 0x5DC870 and
  returns 0. The seed goes back to the hand (`StoreChantsAndAgeFromSpell`, `ClearSpellLink`: the spell closes and
  `SpellWithObjects::CloseDown` 0x721300 deletes the stone). openblack: `HandSystem::PickUpSeedOrStone`, with the
  225 ms grab and the influence of `GenericPickup` 0x5D2800. Like every object that is not a tree or a forest, it plays
  `SoundTag::Create(MapCoords +0x14, 10 G_PickUpObject, mode 3, 3D, InGame)` 0x71EB60 (0x5D2881..0x5D28B7). **(approximate)** for the seed: openblack only stores the drawn point, not its own
  MapCoords that 0x729020 does not move.
- **What is needed to use it:** two stones of the player (`teleportCount != 1`) and villagers of one of their towns
  (`fn_005FC4B0`), inside their influence. With a single stone it cannot be applied.
- Hook `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1,0,hand"`: only plants B (script); A is cast with the hand
  (`OPENBLACK_TEST_SEED=TELEPORT`, `OPENBLACK_TEST_MAGIC_TURN=200` and a press of `OPENBLACK_TEST_CAST` with the mouse
  at x0,z0); ten turns later it puts in the hand the villager of PLAYER_ONE's town nearest to A, and the
  following presses of `OPENBLACK_TEST_CAST` (real, through `HandSystem::Update`) apply it and then pick up the
  pool. Every 10 turns it writes the stones, their seed and what is in the hand.

### `DoTeleport` 0x5FC790 and the cost

`costPerKilometer` is read through `fn_005FBF20` (the GMagicTeleportInfo). Among the player's other stones it picks the one with the largest saving `s = dist(dest, living) − dist(dest, T)` (metres,
`GetDistance` 0x74CD50); threshold 0 (or −1e6 if forced). If there is a spell: `SpellEvent{2, the stone's position}` and
**`PayFor(−s·costPerKilometer·0.001, true)`** (`fn_005FBF10`). `PayFor` 0x720990 → `fn_00720FC0` does `chants −= cost`
without clipping, divided by `max(tribalPower,1)` with `divideCostsByTribalPower`. **Literal reading: a
useful jump (s > 0) gives chants to the spell; only a forced backward jump (s < 0) costs.** Checked: a forced
jump of −30 m charges 6 (=30·200·0.001); the natural jumps of +50..60 m have a positive saving. Then
`CreateSpotVisual(SPOT_VISUAL 14 VILLAGER_TELEPORT = SF_TeleportVillager, 1.0)` at both ends
(`GParticleContainer::CreateSpotVisual` 0x63E540(pos, 14, 1.0, NULL); the plain `CreateSpotVisual` passes its float to
0x63E4B0 together with the entry's own duration, entry +0x44, so the 1.0 is not a duration) and
`Living::MoveByTeleport` 0x5EC340: sound `G_SpellTeleportEnergiseGo` (InGame 39) where it was, `..Arrive` (InGame 38)
where it arrives, and `MoveMapObject`. Both are `SoundTag::Create(MapCoords, sample, track 0, mode 2, loops 0, +0x40 0,
3D 1, AUDIO_SFX_BANK_TYPE **1 = InGame**, delay 0)` 0x71EB60 (0x5EC342..0x5EC372; the bank is 1, not 2 as the
old comment said): they now go through `audio::tags::CreateAtMapCoords`, not through a direct emitter.

- `GPlayer::Process` 0x6496BC → `fn_005FCC70`/`fn_005FCBA0`: each turn, for each available stone, those that no longer exist
  or no longer follow the stone's reaction drop off the list of travellers (`teleport::ProcessPlayers`, slot 3
  of the turn): `fn_005FCBA0` drops travellers that are not available (vt 0x2C) or whose reaction (`Living::GetReaction`
  0x5ECA60) is no longer the stone's.

### The villagers go to the worship site through the stones (faithful)

`Villager::CheckWorshipActivity` 0x76BAE0 → `Villager::CanIGetToTheWorshipSite` 0x76BC20 → `GPlayer::fn_0064D6B0`
(= `teleport::FindRouteStone`), all re-read:

- `CanIGetToTheWorshipSite(MagicTeleport*& out)`: without a town (vt 0x48) or without a worship site (vt 0x30C) it returns 1;
  if `GetDistanceInMetres` 0x74CD70 (flat: `GetDistance` 0x74CCB0 only uses the first two dwords of the MapCoords) from the
  villager to the site is `<=` `maxDistanceThatVillagersWillGoToWorship` (the town's info +0x148, 500 m in Land 1)
  it returns 1 without a stone (0x76BC5E..0x76BC69); if it is greater, it asks for the player (vt 0x1C; without a player also 1) and calls
  `fn_0064D6B0(the villager's position, the site's position, maxDist)`: 0 → it cannot go (0x76BCA3), otherwise `out` = the stone and 1.
- `fn_0064D6B0` walks the player's stone list (+0xA58, the newest first) and stores **separately** the
  minimum distance to the origin (d1, with the stone that gives it) and to the destination (d2), both starting at `maxDist`
  (0x64D6C5/0x64D6C9); at the end it returns that stone if `d1 + d2 < maxDist` (0x64D720..0x64D731, strictly). A
  single stone can serve both ends (the two minima are kept apart, 0x64D6D5..0x64D71C); the stone it gives back is
  always the one nearest `from` (0x64D6EF). The route through the stones is taken only when it is strictly shorter
  (0x64D72E): 30 + 70 against 100 is not enough. Tests in `test_teleport` (`RouteStoneBothEndsAreLookedForApart`).
- With a stone, `CheckWorshipActivity` (0x76BB99..0x76BC07) does: `SetReactionDoneWhen(REACT_TO_TELEPORT)` 0x6E44A0 (the
  reaction's record is stamped with the turn; if it does not have one, it **adds** it at the end and, with 3 already, removes the oldest,
  0x6E4507..0x6E4540; without the 1800-turn forgetting of `fn_006E4340`. Ported separately in `VillagerWorship.cpp`
  `SetReactionDoneWhen`: the `RefreshRecord` of `StopReacting` only updates and was of no use), the villager leaves the list of reactors of the reaction it was following
  (0x76BBAF..0x76BBF2; openblack does not keep that list in a `Reaction`, and `SetupReactToTeleport` changes its +0x94 / +0xBC
  anyway) and `StartReacting(REACT_TO_TELEPORT, stone, the stone's reaction +0x94)` (vt 0x994 =
  `Living::StartReacting` 0x6E4590, which dispatches by type to `Villager::SetupReactToTeleport`). That is: first
  `GotoWorshipSiteForWorship` sets state 59 and the goal (the site's arrival point), and on top of it
  201/251 towards the stone is pushed; on jumping, `StopReactingAndSetState` → `PopFromPrevious` returns state 58
  (`GOTO_WORSHIP_SITE_FOR_WORSHIP`, the resume state of 59 in info.dat), whose **own state function is
  0x76BCC0 = `GotoWorshipSiteForWorship`** (`VillagerOriginalFns.h`), which sets 59 again and walks: the villager continues
  towards the site from the stone it came out of. That row 58 had no function in openblack (the villager stood
  still): now it is `villager_worship::GotoWorshipSiteForWorshipState`.
- The jump's saving is positive (it goes towards the site), so **the jump gives chants back to the spell**.
- Hook `OPENBLACK_TEST_TELEPORT="x0,z0,0,0,0,worship"` (with `OPENBLACK_TEST_WORSHIP_SITE="NORSE"`): puts stone A
  far from the site (x0,z0, or a land point at 1.15 × maxDist that it looks for itself), stone B 20 m from the site, moves the
  PLAYER_ONE villager nearest 80 m beyond A (so it ends up out of reach) and calls `CheckWorshipActivity`.
- `Creature::MoveByTeleport` 0x479F70 and `Creature::ReactToTeleportPriority` 0x4F3A10 / `SetupReactToTeleport` 0x4F3A60
  are still **pending** (there is no creature).

### SF_TeleportVortex and ZR_SurfRevol (`src/Particles/Rules/SurfRevol`, `src/Graphics/RendererRevolvedSurface.cpp`)

- The file `SF_TeleportVortex` (decompressed from `Data\Spells\ZSpellFiles\SF_TeleportVortex_txt.zzz`, verified):
  `ZR_SurfRevol` FunctionIndex 0 (flat disc), texture `S_TileLandscape.raw` (256×256, with alpha
  `S_TileLandscapeA.raw`), NumU 12 × NumV 5, SpeedU 0 / SpeedV 0.213717, `AlphaFadeIn`/`Out` 0.4, `FadeAlphas` 1,
  `ChangeSpecColor` 1, `MaxUVChange` 1, `MaxVertexChange` 0.35, `HeightAboveLandscape` 0.5,
  `DoRaiseAboveLandscape` 1, `RaiseAboveLandscapeRadius` 30, `MaterialUpdateZBuffer` **0**,
  `MaterialUseTextureAlpha` 1, `UseAdditiveAlpha` 0, `MaterialSetDoubleSided` **0** and `UseLighting` **1**; scale 5 through
  `UR_ChangeScale` on the parent atom (`InitialScale` 2); sound loop `SOUND_SPELL_TELEPORT_POOL` in the
  `SoundOfCreate` of group 0's `CreateRuleAnAtom` (LOOPING 1, SOFTRELEASE 1).
  `MaterialSetDoubleSided 0` is already met: fn_0081C780 sets CULLMODE ((~mat+5)&1)·2+1
  (0x81CC5E..0x81CC6E) and `RendererRevolvedSurface.cpp` uses `render_modes::CullFor(surface.doubleSided, false)`; that
  bgfx's Ccw is the D3DCULL_CCW of these vertices is **(inferred)** (vertices in world space, without mirroring, like the
  models; the pool is still visible from above). The discs do **not** go through `world_triangles`: with as many
  speculars (+0x30, count +0x38) as colours DrawAt takes fn_0081C780 (0x67CAEE), Draw3DWorldTriangle with
  per-vertex specular (0x81C9B9..0x81C9C4), and ZR_SurfRevol sizes both to NumU×NumV (0x685A0E..0x685A3D).
  **(pending)** `UseLighting 1`: openblack draws the pool without lighting, so it looks dimmer than in the original. `UseLighting` (+0x88) requires the
  mesh's normals (`fn_006C9340`) and a D3D light that the `WorldQuad` program does not have, and the culling requires knowing the
  winding of the triangles: it is work for the particle renderer, and doing it
  halfway would leave the pool invisible from above. The installed `S_TileLandscape.raw` is from 2021 (patch), so the
  tone is not comparable either.
- `ZR_SurfRevol::ModifyAtomCollection` 0x686370 (props 0x6B2E80): creates a `RenderParticleGJMeshRotatingUV` (ctor
  0x6C8B60) with a surface-of-revolution mesh built in `fn_006858F0`. The profile comes from FunctionIndex (table 0x6868CC):
  0 `TestDisk` (r=t, y=0), 1 `TestFunnel` (r=t, y=3(√t−1)), 2 `TestFunnelSpout` (r=1.5t, y=3(√2t−1)), 3
  `TestFunnelParab` (r=t, y=3(t²−1)); any other → disc. Vertex (i,j): `u=i/(NumU−1)`, `t=j/(NumV−1)`,
  `(r(t)cos2πu, y(t), r(t)sin2πu)`, uv `(u,t)`. With `FadeAlphas`, the row with `t<AlphaFadeIn` goes in RGB `255·t/fadeIn`
  (alpha 255) and with `t>1−AlphaFadeOut` in alpha that goes down to 0 (RGB 255); with `ChangeSpecColor` the specular is the
  player's colour × `(1−ese factor)` (`GetPlayer3DColor` 0x64B590 → table 0xBFF0B8 with `GetRemapedPlayer`; the neutral is
  0xFF000000). `fn_00685F00` scales the UVs by `TextureWidth/256, TextureHeight/256`; `fn_00685F40` twists the UVs
  `u += (1−t)²·MaxUVChange`; `fn_00685FC0` rotates each row `t·MaxVertexChange` about Y; with `DoRaiseAboveLandscape`
  (`fn_00686980`) the mesh is cut along the terrain's cells and diagonals (`fn_00686D90`) and each vertex rises
  to the terrain below it minus that of the centre, once when it is created (`land_morph`, see
  [rendering-objects.md](rendering-objects.md#meshes-stuck-to-the-ground-land_morph)). Without `DoRaiseAboveLandscape`
  the twists breathe with sin(fmod(collection age × +0x68, 2π)); +0x68 has no property (1.0 from the ctor 0x686200).
  `RenderParticleGJMeshRotatingUV::GameUpdate` 0x6C8BC0 shifts the UVs (SpeedU/V) within the tile; `DrawAt`
  0x67CBA0 draws in mode 6 (colour = texture×diffuse + specular, alpha = texture×diffuse).
  - **Size (faithful, verified):** the mesh has radius 1 and only goes through the atom's drawn matrix
    (`RenderParticleGJMesh::DrawAt` 0x67C150 multiplies each vertex by `DrawData+4`, the PSR of atom+0xD0). That PSR
    already carries the hierarchy: `PostUpdateAtoms` fn_00673EA0 does `cur = the parent's PSR × local` (fn_00673DB0, fn_007FAE60) and
    `scale = +0x74 × +0x78 × the parent's`. The disc atom is born with +0x74 = 1 (ctor `AtomCore` 0x673830) ×
    `Scale` 1 (0x68649E) and is a child (Hierarchies[2] = 1) of the group 2 atom. Radius = 1 × `InitialScale` of group 2 ×
    `UR_ChangeScale`: **6 m in `SF_SpellDispenserVortex`** (InitialScale 6, without UR_ChangeScale; magnitude 1.0 in
    `SpellDispenser::CallVirtualFunctionsForCreation` 0x722840) and **10 m in `SF_TeleportVortex`** (2 × 5).
    `fn_00686980` (DoRaiseAboveLandscape) takes the mesh to world space with that same matrix (fn_00673E40) and returns it with
    its inverse (0x686D6C), so it does not change the size. Error corrected: the port multiplied again
    by the parent's scale (36 m radius for the dispenser, 100 m for the teleport stone).
  - openblack: the `ZR_SurfRevol` rule stores the mesh in a `SurfRevolCreator` (type `Other`) per atom; the radius is
    `atom.scale` (which `Effect::PostUpdate` already multiplies by the parent in a hierarchy). `RendererRevolvedSurface.cpp` draws it with the `WorldQuad` program in two passes (the blended texture and the
    specular added with a 1×1 white texture). The `S_TileLandscape.raw` of this installation is dated 2021
    (replaced by a patch). The dispensers' discs share it.
- `SF_TeleportInHand` and `SF_TeleportOnHolder` (sprites) belong to the hand effects; `UR_FollowLocalHand` **is already ported**
  (`src/Particles/Rules/HandFollow.cpp`, registered in `PSysRegistry.h`: the old note that it was missing was false), although its
  effect in the hand has not been checked in game. The villager flash SV 14 `SF_TeleportVillager` already works
  (`psys::manager::CreateSpotVisual`).

### Tests and screenshots

- `test_teleport`: `FastDistance`, the detour rule (1.2), `ChooseTarget`, the sign of the cost (500 useful m
  give back 100 chants, −20 forced m cost 4) with the real `Spell::PayFor`, `FindRouteStone` (two tests: the basic one and
  `RouteStoneBothEndsAreLookedForApart`, the two minima separately and the strict `<`), the four profiles of
  `ZR_SurfRevol` and the vortex mesh (12×5, fades, triangles, UV and vertex twist, player colours).
- Hook `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,player[,mode]]"` (`OPENBLACK_TEST_TELEPORT_TURN=<n>`,
  `OPENBLACK_TELEPORT_TRACE=1`): plants two stones (like `SPELL_AT_POS`) and, in `walk` mode, makes the nearest villager
  walk towards B; in `drop`, drops it forced on A; `none` only the stones; `hand`, the real hand path;
  `worship`, the trip to the worship site through two stones (see above).

## Storm, electric storm and tornado (`Magic/Spells/SpellStormAndTornado`, `Particles/Rules/Storm`, `ECS/Weather/{LightningFlash,StormClouds}`)

Read in `runblack.exe` for this port. The three MAGIC_TYPEs (16 STORM,
17 STORM_PU1, 18 STORM_PU2) are the same class and cast the same `SF_LightningStormPush`; what changes is the upgrade
level their rules read (−1, 0, 1: derived from the seed, unverified) and the effect rows and those of
`GMagicStormAndTornadoInfo`.

### The spell (`SpellStormAndTornado.cpp`, 0xF8 bytes, vtable 0x9847DC)

- Constructor 0x72D9C0: `Spell(type, creator)`, +0xEC (the whirlwind's PSys) and +0xF0 (the water reaction) to 0
  (`fn_0072DA10` zeroes +0xEC and +0xF0), and it enters **first** in the list 0xDA07F8 (count 0xDA07FC).
- `InitWithPos` 0x72DAA0: the radius (castData +0) is clipped to [minRadius 20, maxRadius 1000] and written back
  (`min(r, max)` and then `max(r, min)`); `Spell::InitWithPos` (the main PSys, magnitude = radius); then a second
  PSys **without a spell**, `GJPSysInterface::Create(NULL, 106 SF_StormCast, the LHPoint of the position, PSysProcessInfo
  +0x24, 1.0, 0)`, whose magnitude becomes the spell's (vt 0x11C).
- `Process` 0x72DB90: forgets the reaction +0xF0 if it is no longer there; if there is a whirlwind, a `PSysProcessInfo` of zeros with strength 1
  and active which the creator fills in (`UpdateSpellInfo`, vt 0x5C), strength 1 again, and a step of the whirlwind; without a creator,
  or when the whirlwind ends (5), it is deleted. Then `Spell::Process`.
- `CalculateCostToMaintain` 0x72DB50 = that of `Spell` × (radius / radiusForNormalCost 40)² (`radiusForNormalCost` is
  info +0x60, `fdiv [info +0x60]`). `GetRadius`/`Get2DRadius`
  (vt 0x60/0x64) = the magnitude. `CloseDown` is that of `Spell`. `ToBeDeleted` 0x72DA20: leaves the list,
  `Spell::ToBeDeleted` and deletes the whirlwind.
- fn_0072DCC0, from the fire's rain cooling (fn_0072EFB0 0x72F2D9, with the position +0x14 of the burning
  object): the first storm spell without a water reaction whose radius exceeds the distance in x, z to its +0x14 receives
  `REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE` (34) from its player, stamped. Connected in `ECS/Fire/FireEffect.cpp`.
- Rows (info.dat): `rainAmount` 50 / 100 / **0**: the tornado **does not rain**. Cost per turn 20/25/30 × (r/40)², per
  event 0/2/10, timers 40 s.

### The cores and the clouds (`UR_CloudMoverNew` 0x6D41C0, `UR_CloudGather` 0x6D4A70)

`SF_LightningStormPush` creates 5 invisible cores (group 0, `CreateRuleSphere`), each with its collection of clouds
(group 2).

- **`UR_CloudMoverNew`** (ctor 0x6D4150: DelayBeforeMove 0, WindDamping 0.06, WindMagnification 60): the first step places
  the cores at the PSys origin; until `DelayBeforeMove` (5 s in the file) it does nothing; afterwards, per core: the
  wind `fn_00771B10(p, smoothed)` (0 if the script casts), `T = wind × WindMagnification × 0.1`, `v += WindDamping ×
  (T − v) × dt`, `p += v × dt`. The spell's position (+0x14) follows the core. With level 1 the cores bounce off the
  shields (`DoAnyShieldDeflections`). 0x6D452C..0x6D4601 measure the slope and scale a **copy** of the velocity that
  nobody reads: in W120 the slope does nothing. CollectionData 0x24 bytes (ctor 0x560C10).
- **`UR_CloudGather`** (ctor 0x6D4700; `TornadoGroup` is not in the file: 10):
  - AtomData 0x38 (ctor 0x560B60) and CollectionData 0x58 (ctor 0x560AF0, dtor fn_006D4900: a detached lightning
    collection is deleted and fn_006D4950 marks the storm). The ctor 0x6D471B sets flags |= 6; it does nothing unless its
    three float providers (Radius, Scale, CloudHeight) exist (0x6D4A7A). fn_006D4970 creates NumAtoms clouds at once,
    each at a random age up to TimeToForm. fn_006D56F0 reads the storm spell's `rainAmount` (GMagicStormAndTornadoInfo,
    file +0x58); < 0 without a spell.
  - First step: lightning flashes if the level ≠ −1; rain yes; the heading = `GetCurrentHeading` 0x673660 =
    `(cameraForward.x, 0, cameraForward.z)` **unnormalised**; only the collection of the **first core** (index 0 in its
    collection, fn_00673CA0) registers the storm, and with level 1 it adds the tornado group to the core. Rate
    `NumAtoms / TimeToForm`; spin ±1 at random; **the first lightning** at `rand(0.5; 1) × SwitchLife + LightningDelay`
    (3 and 8 s in the file: between 9.5 and 11 s). The delay is not an interval: it is the other way round.
  - It emits while `count < due` (the first cloud comes out on the first step) and there are fewer than `NumAtoms`.
  - Each cloud (fn_006D4880): radius `(rand(0.7; 1) + 0.7) × R` (1.4..1.7 R, R = the provider Radius = 1.2 × magnitude),
    random angle, age 0, extra height `rand(HeightVaryAmount)`. After `TimeToForm` it starts again: the clouds
    **spiral towards the centre** in `TimeToForm` s (`r = (1 − f) × radius × (1 + 0.3 cos(age × 0.1 + θ)) × the
    collection's scale`, with `f = age / TimeToForm`), with angular speed `±MaxAngularSpeed × (1 − (2f − 1)²)`.
  - Their look: grow = `f / FracToMaxSize` and then it goes down; grey colour `MaxColor + (MinColor − MaxColor) f`, alpha
    `MinAlpha + (MaxAlpha − MinAlpha) grow`, scale `(MinScale + (MaxScale − MinScale) grow) × CloudScale`, and the
    ratio `MaxCloudRatio + (MinCloudRatio − MaxCloudRatio) f` goes to the atom's stretch, which the mist creator
    uses as its k (`TakeRatioFromMatrix`). The collection, in its first 10 s, multiplies the ratio and the radius by
    `c + (1 − c) × smooth(t)` (`CloudRatioMaxCollection` 1, `CollectionRadiusInitialScale` 2).
  - Past half formed, a cloud calls fn_006D56B0(0.2, 2 − 2f) (0x6D4F8E) and discards the result.
  - Height: `baseScale × extra height × scale + CloudHeight`, plus the average ground height on a 3 × 3 grid of
    points at 0.33 R from the core (with the tornado, that of the PSys origin, which is the base of the tornado).
  - They are drawn as `LH3DMist` with the mist creator (`Particles/Creators/Mist.cpp`): corrected there
    so that the k takes the stretch and the colour is multiplied by the base colour of the light table [0xFA26A4]
    (`LandLightTable::Current().GetRawBase()`, that of the previous frame). The shadow map `S_SMClouds16` **is
    stamped** on the ground (0x67A7BE..0x67A8C1 → fn_006CA280 → fn_0086CFF0 mode 2 → fn_00878C70): `mist_atoms::SubmitFrame`
    calls `land_light::AddStamp` at `(x + 10, 0, z + 10)`, centred, atom alpha / 255.
- **The registered storm** (fn_006D5730, on the values of fn_0083F3F0): inner `max(R, 60)`, outer
  `max(2.5 R, inner + 20, 80)` (the three `fcomp; test ah, 0x41; je` keep the value only if it is greater); fade
  `0.5 × TimeToForm` (4 s), life 1e9 s, strength 1, **0 clouds** (`DrawClouds` draws nothing for the miracle),
  elevation = the provider CloudHeight (1.5 × magnitude); 20 degrees, rain `min(ftol(strength × rainAmount) as a byte,
  100)` (so `strength 3 × 100 = 300 → 44`: 300 & 0xFF = 44 is what meets the `cmp al, 0x64` of 0x6D5915), overcast
  80, snow 0; wind = heading × `strength × lerp(WindMinSpeed,
  WindMaxSpeed, clamp((mag − MagMin) / (MagMax − MagMin)))`, each component clipped to ±128 and rounded to a byte
  (128 gives −128). The position is **not** set when it is created: `fn_006D5950` copies it from the core each step, and since the destination
  of the `GWeather` stays at (0, 0, 0), `GWeather::Update` moves it 0.1 m per turn towards the map origin before
  it is set again (faithful, negligible). On closing it is marked for deletion (fn_006D4950); if someone deletes it (a
  `KILL_STORMS_IN_AREA`), the next step registers another one.
- **Lightning** (0x6D52A0): with lightning flashes and half-formed clouds (those with `f > 0.5`, the static list 0xD4EE88, which each
  collection empties when starting), the next one in `rand(0.5; 1) × SwitchLife / max(tribal power, 1)`; it comes out of a random
  cloud: the first time `AddSubCollection(LightningGroup)` on it; the following ones the same collection **moves** to
  the new cloud (fn_00674A30) and, after `rand(0.5; 1) × LightningLife`, it detaches from the cloud and stays without updating
  until the next one. The lightning is the `UR_Lightning` of the lightning bolt in its parent mode (radius 1 × magnitude): type 3 events with
  the storm's row (STORM_PU1: burn 10000, hit 0.0045 → sets on fire at the first strike). The thunder is `SoundLightning`
  at the cloud, with a random size (< 0.33 → 3, < 0.66 → 2, otherwise 1) and flags |= 0x22.
- **The cloud's glow** (faithful): during `SpecLife` (0.5 s) the lightning's cloud receives in its +0x90 a
  bluish specular (0x6D5205..0x6D5283: `v = ftol((1 − t / SpecLife) × 255)`, ARGB `(255v, 200v, 200v, 255v) >> 8`;
  after `SpecLife`, and in fn_006D4880 0x6D48E5, `0xFF000000`). **It is drawn**:
  `RenderParticle` copies +0x90 to DrawData +0xC (0x679BF4) → `RenderParticleMist::DrawAt` 0x67A6C4/0x67A6D6 pushes it
  to `SetColour` 0x7F9770 (+0x4C colour, **+0x50 specular**) → the effect branch of fn_007FA300 (0x7FA3B1..0x7FA5AF) does not
  touch +0x50 (the normal one overwrites it with the haze at 0x7FA6DD) → fn_0080DB30 0x80DEF5 puts it in [0xE9FE2C] → the
  specular of each vertex (e.g. fn_0084D2D0 0x84D645: vertex +0x14). `D3DRS_SPECULARENABLE` (0x1D) is set once
  when the device is opened (fn_0082C8F0 0x82CC1E..0x82CC5F): 1, except on a Voodoo ([0xEA9E9C], `CheckDescForVoodoo`);
  no other code changes it (search for `push 0x1d` and all calls to `SetRenderState` 0x412940), and mode 6 (fn_0082DF10) does not either. So
  the cloud brightens towards bluish white (colour = texture × diffuse + specular; the alpha does not change) and fades out in
  0.5 s. openblack: `Atom::specular` (+0x90) and `DrawAtom::specular` (`Particles/PSys.h`), written by `CloudGather`
  (`Storm.cpp`), passed by `mist_atoms::SubmitFrame` to `MistDesc::specular` (`Graphics/Mists.h`) and added in the
  effect branch of `Renderer::DrawMist` (`u_cloudSpecular`, `fs_cloud.sc`). The creator's `SpecColorR/G/B`
  (`ParticleCreator::DefineProperties` 0x6B3562..0x6B359B, +0x24/+0x28/+0x2C, 0 in the ctor 0x6A91C4..0x6A91CA) is now
  read (`psys::ReadCreatorProperties`) and `Effect::NewAtom` puts it in +0x90 like fn_006A85E0 0x6A8748..0x6A875B:
  `(R << 16) | (G << 8) | B`, alpha 0; it is 0 in all the dumped files.
  - **The specular's alpha is not fog**: in D3D7 the specular alpha of a TL vertex is the
    vertex fog factor only with `D3DRS_FOGENABLE` (0x1C). The game never sets it: there is no `push 0x1c`
    before `SetRenderState` 0x412940 nor before `IDirect3DDevice7::SetRenderState` (vt +0x50) anywhere in `.text` (byte
    sweep; nor `FOGTABLEMODE` 0x23 or `FOGVERTEXMODE` 0x8C), and the D3D7 default is FALSE. The engine's haze
    is in software (light and specular of each vertex, fn_007FEB30). So the alpha 0xFF / `(255v) >> 8` of +0x90
    changes nothing in the drawing and the port, which only uses the RGB, is faithful. The effect branch of fn_007FA300 only changes the
    light (fn_0081E1F0 = the device's `SetLight(0)`, vt +0x48, with the position (0, 500000, 0) and back,
    0x7FA4F2..0x7FA590) and [0xC39264] (0xD2 → 0x5A).

### The tornado (`UR_Tornado` 0x6D18B0; ctor 0x6D1680)

Only with level 1 (STORM_PU2): group 10 under the first core, and under it group 11 with the rule.

- `UpdateBaseAndTopPoints` 0x6D1C60: the closing fade `1 − (t − close) / FadeOutTime`; the collections' alpha
  `min(age / FadeInTime, fade)`; the top follows the parent (the root of the tornado, which `UR_FollowParent` leaves at the
  core) after `DelayBeforeMove`; **the base wanders around the top** `(n(t) + n(2t)/2, 0, n(1.3t) + n(2.6t)/2) ×
  TornadoScale × TopMoveAmp` with `t = age × TopMoveFreq` (`VSNoise1To1`), on the ground; the PSys origin becomes the
  base; the top at `TornadoScale × TopHeight` above it; velocities per step. **Each step** while it is not closing:
  `SpellEvent` type 2 at the base with its velocity (row 18: hit 0.001, push 0.001, radius 1).
- The funnel: radius `(BaseRadius + (TopRadius − BaseRadius) h²) × TornadoScale` (fn_006D2790; the exponent
  is 2); its axis at height h, `lerp(base, top)` with Schlick's gain `gain(FunnelBendParameter,
  h)` in x, z (fn_006D2910; `bias(b, x) = x^(ln b / ln 0.5)`, 0xD4EEA0 = 1 / ln 0.5) plus a wiggle of
  `TornadoScale × WiggleAmplitude` with phase `h × WiggleCount × π`.
- The flying atoms (`UpdateFlyingAtoms` 0x6D31F0): spin at `lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias, h)) ×
  own speed (0.5..1.5)` (more slowly outside the wall: × `r / (ρ + 0.1)`), the radius approaches the wall with
  `dρ/dt = −0.5 (ρ − r)` integrated with the midpoint method (fn_006D28B0), they rise towards `lerp(base, top, mix)` with k 0.1
  (real objects) or 0.3, and the funnel's velocity at that height is added. The real objects (state 0) and the decorative ones
  (1) go to group 12 (`GroupToMoveToOnceDone`: gravity 10, collide with the ground, 15 s) when they pass 0.9 of the
  height or on closing; the decorative ones start with gravity and blend into the vortex in `PretendBlendTime`.
- **Picking up** (fn_006D21B0): once every three game turns, after `FadeInTime`, **one object at a time**; reach
  `(r(0) + r(1)) × clamp(tribal power, 1, 5)`; spiral of `(ceil(R/10) + 2)²` cells (GUtils::Spiral 0x74D7E0 with
  direction 1 and count 1, 0x6D22E9: −x, −z, +x, +x...); each object with a 3D object whose
  own cell is that one and within its 2D radius + reach. If it fits entirely (`CanBecomeAPhysicsObject`, `2 r(0) > radius`,
  `r(1) > radius`; for pots and piles, the flag of their `GPotInfo`, Pot 0x66E8F0) the spell is asked
  (`SpellEvent` 7, `CanBeDestroyedBySpell`); if it is a pile (`IsPileResource`), `lerp(150, 650,
  clamp(TornadoScale))` (rounded) is taken from it as **a new pile of the hand's type** (vt 0x870 `GetHandPotInfoType`:
  HAND_FOOD 12 / HAND_WOOD 11; from a store's pile it is taken from the store), scaled `rand(0.7; 1.2) ×
  clamp(TornadoScale, 0.2, 1)`, and that one flies. A creature: fn_00477060 (there are no creatures).
  - A pile split by the tornado is created by `Pot::Create` with the hand's pot type (PileFood 0x66EC30 = 12
    HAND_FOOD, PileWood 0x66EC40 = 11 HAND_WOOD) and enters its map cell at once (`MobileObject` 0x607150 + 0xA9,
    CallVirtualFunctionsForCreation).
  - A carried object rises to rand(0.7) of the funnel's height (0x6D257A..0x6D269D).
  - A thrown debris or pretend object: baseVelocity + tornadoScale × speed × (cos θ sin φ, cos φ, sin θ sin φ),
    φ = rand(spread), θ = rand(2π), speed = rand(0.33, 0.66) × the rule's.
- What it carries (`RenderParticleGameObject`, 0x6C9E60): the atom copies the object's matrix (fn_00674150) and the object
  draws the atom's (`DrawAt` 0x67B170). When the atom dies (hits the ground in group 12, or at 15 s), the
  destructor 0x6C9FC0: **a living thing** is left where it is and dies (`DestroyedByEffect`: the villager dies), **anything
  else is deleted** (`ToBeDeleted`): the tornado destroys what it carries.
  - Release (fn_006C9FC0): an object no longer available (vt +0x2C, a dying villager too) is only let go, not killed or
    deleted (0x6C9FD8..0x6CA02F); otherwise fn_006C9EF0 puts it back on the map where the atom is (angles from the
    matrix, altitude 0), `EndPhysics(NULL, 1)`, clears +0xA & 0x10, then `DestroyedByEffect(player, 1.0)` (0x6CA01A;
    `Villager::DestroyedByEffect` 0x7502D0 → VillagerDead(2 SPELL)).
- Dust (`UpdateDebrisAtoms` 0x6D2AF0): `DebrisEmitRate` per second, at most 50, of the dust colour of the ground
  material (`GTerrainMaterialInfo.tornadoDustColorRGB`, the second material of the altitude in its country); decorations
  (fn_006D2E70): `PretendEmitRate × TornadoScale` per second, only on dry land and with the tornado fully visible
  (alpha > 250): chickens and bushes. The funnel meshes (`S_TornadoNonFade.l3d`, fn_006D2A40) at the base, spinning
  `(1 + 0.13 i) × MeshThetaDot`.
  - The dust colour: each channel of the creator's colour × the ground dust's >> 8, alpha kept (0x6D2C21..0x6D2CC5); the
    dust cell is the high words of MapCoords(LHPoint) of the tornado position, passed on unchecked (0x6D2B9B..0x6D2BB2;
    openblack checks the island side).
- `UR_Tornado` data: CollectionData 0x74 (ctor 0x5608A0; the one being processed is kept in 0xD4EEC0 and its collection in
  0xD4EEC4); FlyingAtomData 0x2C (ctor 0x5609F0, a new one in state 2); DebrisCollectionData and FlyingCollectionData
  0x28 (ctors 0x560950 / 0x5609A0: +0x20 made, +0x24 to make). Defaults and inputs: DebrisSpreadAngle 1.0367 (+0xC8);
  the TornadoScale provider is 1 without one, strength 1 (0x6D1924).
- fn_006D2860: the funnel sprites' scale (BaseScale + (TopScale − BaseScale)·h²) × tornadoScale (the sprites are unused
  in the data); fn_006D2710 writes ThetaBias clamped to 0..1 back into the rule.
- Not ported (not used): fn_006D1AD0, the funnel sprites (`NumAtomsToCreate` = 0 in the only file). Read by
  no code in W120: `PretendHeight`, `MaxSearchDistance*`, `LocalSearchDistance`, `PauseBeforeAffectsGameObjects`,
  `UseTornadoStrength`, `K1_Accn`, `ScaleWhipUp`.

### The whirlwind (`UR_StormCast` 0x6D59B0, SF_StormCast)

30 atoms in a ring around the parent (group 0 has a hierarchy and scale = the magnitude): a radius that shrinks from
`MaxRadius` to `MinRadius` at `RadiusDot` and, after `DispersalAge`, grows and fades in `FadeOutTime`; spin
`lerp(ThetaDotMinRadius, ThetaDotMaxRadius, normalised radius) × (1 ± ThetaDotSpread)`; at `InitHeight × scale` above the
ground; the parent advances along the heading **scaled to 20** (+0x64 of the ctor, no property), accelerating from 0 to 1 between
`AccnStartTime` and `AccnEndTime`. Data: CollectionData 0x40 (fn_006D5F40), AtomData 0x2C (fn_006D5F70).

### The flash and the clouds of the registered storms (`ECS/Weather/LightningFlash`, `StormClouds`)

- **The flash** (the 0x24-byte object at GWeather +0x70): `fn_00837290(point, outer radius, intensity)` turns it
  on in the lightning of `GWeather::Update` (0.5 the fork ones 0x83FBF0, 1.0 the sheet ones 0x83FC5D);
  `fn_008372D0` ages it each turn and turns it off after 0.8 s; `fn_00837200` each frame (from
  `LH3DAtmos::Update3D`, only the unmarked storms): `s = 1 − age`, f1 = s, f3 = s³, both 0.1 between 0.2 and 0.5 s,
  × intensity. The terrain light stamp `fn_0086CFF0(f3)` (the 64 × 64 radial map 0xED92F0, `(32 − d) × 9`, made by fn_00837110) is stamped with
  `land_light::Stamp` (see the storm's pending list below). **`weather::LightningFlashAtCamera(camera)`** is [0xFA2768] of
  `Update3D` 0x83587C..0x835903: the nearest storm in x, z (the unmarked ones; the first of equal ones), "inside"
  if `d² < ((inner + outer)/2)²` of its descriptor, and `ftol(clamp(f1, 0, 1) × 255)`. **Faithful quirk**: the
  inside flag is not cleared when another, closer one appears from which one is outside, which then gives its flash.
- **The clouds** (`GWeather::DrawClouds` 0x83FC90, for all the storms, marked or not, from fn_0083F8B0 each
  frame): up to 16 domes (`numClouds` is clipped in the descriptor itself), one new one per frame: k
  `Random(2.5; 5)`, size `(outer + inner) × 2 Random(0.01; 0.015)`, at `(±1, Random(−10, 10) + elevation, ±1)`;
  every 400 **frames** a new destination `(±1, Random(0, 20) + elevation, ±1)` towards which they go in steps of 0.0025;
  position `(outer + inner)/2 × (x, z)` from the drawn position, and the height above the ground; colour = the base of
  the light table with each byte × (1 − blackness/2), alpha `fade × 0.75 × base alpha`, and the distance haze of
  fn_007FEB30; they are drawn above alpha 5. They go to `mists::Submit` (the effect branch). The storm's shadow on the
  ground (`fn_0086CFF0` mode 2 with the map 0xEE9D3C, strength `(blackness + 0.7) × fade`) is stamped with
  `land_light::AddStamp` (`StormClouds.cpp`); with 0 clouds (the miracle) `DrawClouds` exits earlier
  (0x83FC9E `jle 0x8400CB`), so the miracle's storm does not put that shadow, only that of its particle clouds.
  `Random` 0x81D180 is the CRT's `rand()` (`game_random::crt`, seed 1 at startup, faithful). The miracle does not have
  these clouds (0); the climate ones and those of the weather objects do (8 by default).
- **Overcast at the camera** (`Clouds::WeatherOvercastAtCamera`, map-level): `GCamera::Update` 0x4426BA, the overcast byte
  of `LH3DAtmos::GetWeatherSmooth(camera, 1)` × 0.01 → [0xD1A26C], which `DrawSky` 0x5E2215 copies to [0xFA2754] for the
  light table. It can exceed 1 (a byte of up to 127).
  Checked for the miracle: the overcast 80 of fn_006D5730 (+0x4B) reaches the camera through
  `storms::CalcAtmos` (full inside the inner radius, 72 m with radius 60; trace `overcast 80`) and `LandLightTable::Build`
  applies it like the original: cap of the base colour `ftol(255 − 96 × 0.8)` = 178 (0x869ADB) and the haze towards the
  storm one (colour `(c >> 3) + 32`, k 48, near 15, far 350; 0x869DB1..0x869F37). In the Land 1 afternoon the change is
  barely noticeable: what darkens most in the original is the clouds' shadow, which is already
  stamped (see below).

The sky alignment (`fn_0064AC30`, `alignment::GetInterfaceAlignment`), which also changes the clouds, is in
[The sky alignment](magic.md#the-sky-alignment-alignmentgetinterfacealignment).

### Inferred, approximate and pending (storm)

- (unverified) The upgrade level from the seed: −1 / 0 / 1.
- (inferred) What `InitialisePhysics` does with a villager or animal carried by the tornado: here it leaves physics and
  goes to the hand state (IN_HAND, `animal_ai::PlaceInHand`). Bit 0x10 of Object +0x25 that fn_006D2140 looks at is not
  checked. The `GetDistanceInMetres` distance is taken in x, z.
- (approximate) The place where a released living thing dies: where its atom was last seen (the turn or the frame). The
  scale of the carried object is uniform (that of the Y axis of its matrix), as in the original.
- (approximate) Taking resources from a loose pile: the amount and the sinking (like the hand); the empty pile is not
  deleted. Piles are also searched for outside the map grid (openblack does not fill it with the pots).
- (approximate) The tornado's collection is updated in the same step it is created (the original links it at the head of
  the list and it enters on the next step): `Effect::UpdateCollection` now walks by index.
- (approximate) The clouds of the registered storms count frames like the original, but openblack draws more
  frames per second; their atlas counter always advances (not only on screen); the base of the light table is that
  of the previous frame.
- (done with `land_light::Stamp`; checked in the game) The light stamp
  of the flash (`LightningFlash.cpp`), the shadow of the registered storm (`StormClouds.cpp`) and **the shadow of the
  miracle's clouds** (`Mist.cpp`): `RenderParticleMist::DrawAt` 0x67A7BE..0x67A8C1 puts one record per cloud into the list 0xD4EDB8
  (position `(x, 0, z)`, frame 0, alpha = alpha byte of the DrawData × 1/255 clipped to [0, 1]); `PSysLightMaps::AddDrawing`
  0x6CA6E0 → fn_006CA280 stamps it with fn_0086CFF0 in mode 2 (shadow: bpp 1 because `IsShadowMap`,
  `ParticleMistCreator::GetBitmap` 0x6AA540), 16 × 16 cells of 10 m; fn_0086D360 → fn_0086D060 → **fn_00878C70**:
  bilinear, `v = 255 − (255 − texel) × strength / 255` with floor 0x30, and `min(v, byte +3)` in the per-vertex colour of the
  LandBlock's cells (bit 4 of +0x920; ClearLight fn_0086D460); the lightning's light is mode 1 (fn_00878780).
  In the game (by day): the ground under the storm is darker than without it, with dark blotches with sharp edges that move with the clouds, and each lightning
  lights up the ground (and the trees, which take the light of their cell) in a bluish patch. Without the original side by side it cannot
  be stated whether the intensity is the same (the 0x30 floor leaves almost black blotches). Pending (terrain, not
  Storm/Mist): the row/column order of the texel in `land_light::ApplyStamp` versus fn_00878C70; and still not ported are
  the creatures (fn_00477060) and fn_006D1AD0.
- (approximate) Each cloud's atlas counter (`Atom::mistCounter`, the +0x84 of its `LH3DMist`, which the ctor 0x7F9560
  starts at `ftol(Random(0, 16)) & 15`, 0x7F95DC..0x7F95FB): the `Random` 0x81D180 with the CRT's `rand()` (`game_random::crt`), and it advances on
  every frame even if the cloud is not on screen (the original only advances those that `LH3DMist::AddDrawing` 0x7FA7F0 sees).
  Before it was a global counter for all: they no longer pulse in unison.
- **The tornado's funnel**: `ParticleMeshCreatorAnimTextured` **does have** the property `DrawWithLandscapeColor`
  : its `DefineProperties` 0x6B3970, after those of
  `ParticleBaseMeshCreator` 0x6B37A0 and its own, reads it as the last property into its +0x84 (0x6B3AEF..0x6B3AFD; the ctor
  0x6A8BB0 leaves it at 0 at 0x6A8BF6), and its `CreateParticle` 0x6A8F0D..0x6A8F20 passes it to bit 1 of +0x24 (the one checked by
  `Particle3DObj::DrawAt` 0x67A00C for fn_0080BEC0). The file's `DrawWithLandscapeColor=1` counts: the funnel is
  multiplied by the ground light (`Creators/Mesh.cpp`). `UseGlobalAlpha` (+0x5A) is bit 0 of +0x24
  (0x6A8E04..0x6A8E17) → vt 0x48 `SetGlobalAlpha(1)` (0x67A216..0x67A227), the table 0xC387C8, which leaves the material's mode 6
  unchanged (type 4 → 6 at 0x57E120: `SRCALPHA/INVSRCALPHA`, alpha = texture × diffuse). That it is barely visible **is largely
  faithful**: the mesh's texture (skin 0x16AA, 256 × 256 ARGB4444 inside the .l3d) is white with alpha 1/15..12/15
  (mean ≈ 0.45), × `ColorA` 60/255 → ≈ 0.1 per layer; there are 2 two-sided meshes (`CreateRuleSphere_TornadoMesh`
  `NumAtoms=2`). `ParticleMeshCreator::CreateParticle` 0x6A8B00 never sets bit 0 (`UseGlobalAlpha`). Already ported:
  see [Particle meshes](#particle-meshes-and-the-shield-dome). Checked again by day with the ground light: the funnel looks like a translucent grey
  column with what it drags inside; faint but visible, as the data give.

### Hooks, tests and screenshots (storm)

- Hooks: `OPENBLACK_TEST_SPELL=STORM,x,z,60` (and `STORM_PU1`, `STORM_PU2`), `OPENBLACK_TEST_STORM_SHOT`,
  `OPENBLACK_TEST_STORM_STRIKE_SHOT`, `OPENBLACK_TEST_STORM_PILE`, `OPENBLACK_TEST_STORM_CLOUDS` and
  `OPENBLACK_STORM_TRACE` in [openblack-internals.md](openblack-internals.md#debug-environment-variables).
- `test_storm`: the spell's clipping and cost, the fn_006D5730 descriptor (radii, wind, rain and their quirks),
  the cloud ramps, the funnel (radius, bias/gain, RK2), a cloud collection that registers its storm and
  marks it on closing, the flash (curve, map, camera and the flag that stays), the colour of the registered clouds, the
  interface alignment and, with the game, `SF_LightningStormPush` / `SF_StormCast` (their registered classes; the
  whirlwind alone, which ends at 4.4 s); `cloudMistAtoms` (each cloud's own counter and the specular in the drawing
  atoms) and `creatorSpecColour` (the creator's `SpecColorR/G/B`).

## Lightning explosion and missing PSys classes (`Particles/Rules/{Explosion,KeyPoints,Orient,Forest}.cpp`)

The lightning explosion (MAGIC_TYPE 7-9 EXPLOSION_ONE, EXPLOSION_ONE_PU_ONE and _PU_TWO; seed BEAM_EXPLOSION) is a
simple `Spell` (`SpellGeneral.cpp`, class General): everything it does comes from the events of `UR_Explosion`. It casts
particle types 11 / 12 / 13 (`SF_BeamExplosionSingle` / `Many` / `Loads`).

### The files (faithful)

- Group 8 (created at the start): a control atom with `SetPSysCloseDown` 0x6A26D0 after 6 s
  (`EventConditionCollectionDelay`; 13.9 s in Loads), which closes the effect (`PSysManager::SetState(1)` 0x672FF0). In
  Many/Loads a `SpreadingDiskEmitter` distributes 6 (Many, radius 30→50) or 6 + 50 (Loads, 25→40 and 20→60) more explosion
  points.
- Group 7: the parent of each explosion (dies at 16 s); group 6: the `UR_Explosion` collection, without atoms.
- Values of `UR_Explosion` in the three files: `InitialDelay` 0.4, `TimeToDoEventsFor` 5, `SmokeDelay` 1.2,
  `BlastSpeed` 50, `SpreadSpeed` 20, `MaxDistance` 15, `MaxObjectsToDelete` 15, `MaxObjectsToExplode` 15; `BeamDelay`
  does not appear (0, the ctor's). Default values of the ctor 0x67E090 (in the port since the merge with water):
  MaxObjectsToDelete / ToExplode 20, MaxDistance 100, BlastSpeed 10, SpreadSpeed 10, TimeToDoEventsFor 5,
  InitialDelay 3.5, SmokeDelay 3, BeamDelay 0.
- **Fixed (faithful):** `SpreadingDiskEmitter` (MAC 0x6A6610, DefineProperties 0x6AFA90) was registered as a
  normal `DiskEmitter`, which reads `Radius` (0): all the explosions of Many and Loads fell on the same point. The original
  emits **several atoms per step** (loop with `ShouldEmit`) and moves each one `(cos θ r, Height, sin θ r)` with
  r = PSysFloatRand(StartRadius, StopRadius) and θ = PSysFloatRand(2π). Its `StartTime` / `StopTime` (+0x5C / +0x60) are
  properties that the class **does not read**. (`DiskEmitter` 0x6A64D0 did match: one atom per step, random direction in the
  disc with radius PSysFloatRand(Radius) and Height in y. `ShouldEmit` 0x6A63A0 too: period 1/EmissionFreq with
  `Randomise` × (0.5 + rand(0.5)), `MaxTotalAtomsToEmit` and `MaxAtoms`.)

### `UR_Explosion` (InitCollection 0x67E200, ModifyAtomCollection 0x67ECE0, update 0x67E900)

- Properties (DefineProperties 0x6B0B90, an `AtomCreateRule`): +0x2C MaxObjectsToDelete, +0x30 MaxObjectsToExplode,
  +0x34 MaxDistance, +0x38 BlastSpeed, +0x3C SpreadSpeed, +0x40 TimeToDoEventsFor, +0x44 InitialDelay, +0x48 SmokeDelay,
  +0x4C BeamDelay. Per-collection data (0x58 bytes, ctor fn_0067E140): centre +0x24, "not started" +0x20, "target
  remains" +0x21, ring +0x30, list {turn, object} +0x34, exploded +0x48, deleted +0x4C, smoke / beam / stopped
  +0x50 / +0x51 / +0x52 and the beam's container +0x54.
- **Each step** (MAC 0x67ECE0): centre = `GetCurrentParentPos` with y = the ground height. If it is neither stopped nor
  closing: with the collection's age > InitialDelay, `InitCollection` (once). While age < InitialDelay +
  TimeToDoEventsFor, **one `SpellEvent 2` at the centre per step** (velocity 0, strength 1, no shields): the spell's
  default event, burn / crush / hit of the effect (BEAM 200 / 0.01 / 0.01, PU1 400, PU2 800 / 0.02 / 0.02) in its
  radius (5, 5, 10) with `ApplyEffectToMapPos`, paying `costPerEvent` (10 chants) each time: about 50 events. With age >
  BeamDelay, spot visual 36 `BEAM_EXPLOSION_FX` at the centre (scale 1, 60 turns); with age > SmokeDelay, the 23
  SMOKE on dry land or the 22 STEAM on water (`MapCoords::IsDryLand` 0x603620), **magnitude 8** for 4 s. Constants:
  smoke magnitude [0x9357E4] = 8, smoke seconds [0x9357E8] = 4; spot visual numbers pushed at 0x67EE99 (36), 0x67EEDE
  (23), 0x67EEF3 (22).
  - **Turns, not seconds** (faithful): `CreateSpotVisualWithSpecifiedDuration` 0x63E580 receives turns. They are the 60
    of 0x67EE92 and, for the smoke, an inline copy of `NumGameTicksPerSecond`: `(1000 div [0xD01A38]) · 4`, ftol,
    `and 0xFFFF` (0x67EEF8..0x67EF2C; it does not call 0x711630).
  - The port passes them through `manager::CreateSpotVisualTurns`. Before, it passed seconds and `CreateSpotVisual` converted them
    again: with a turn other than 100 ms the count did not come out the same.
  - Both effects come out at `MapCoords(centre)` 0x603160. The container's effect starts at that MapCoords as a point:
    fn_0063E410 0x63E418..0x63E47B, `GetAltitude` + the altitude.

  Stopped or closing: the beam is closed (`GParticleContainer::CloseDown`
  0x63E370) and the list is emptied. Afterwards, always, the update 0x67E900.
- **The symbol `RecursiveUpdateForkStructure@UR_Lightning` 0x67E900 is misnamed**: it is the update of
  `UR_Explosion` (without recursion). Each step the ring grows `SpreadSpeed × dt` ([0xD4E0EC]); targets that are no longer
  available leave; with exploded ≥ MaxObjectsToExplode **and** deleted ≥ MaxObjectsToDelete the list is emptied.
  It walks the list and **handles at most one object per step**: the first one with a 3D object (+0x40) that the ring reaches in
  3D (`|p − centre|² ≤ (GetRadius + ring)²`) receives, if it is inside a shield with margin 2 (fn_006D0BC0), a spark
  (fn_006D0AF0) and an event 4 to the shield (with 0 it is saved); then `GetActualObjectToEffect` (vt 0x5D8) and the question
  `SpellEvent 7` (CanDestroy). With 1: if it is not a creature and there are some left, exploded + 1 and the mesh in pieces (fn_00681260,
  see "The pieces" below); if there are deleted ones left, deleted + 1 and `DestroyedByBeam` (vt 0x500) if [0xC029EC] (1). Whatever it answers,
  the object leaves the list; the one that answers 1 cuts that step's walk.
- **InitCollection 0x67E200**: margin = the radius of the spell's effect (`GMagicEffectInfo` +0x2C = file 0x1C), 5 without a
  spell. Inside a shield (fn_006D0BC0 with that margin): the point where a ray from 200 m higher up cuts the
  sphere (vt 0xFC FindIntersect), spark and event 4 at the centre to the shield's spell; **with 0 the explosion stops
  completely** (+0x52). Then: on dry land a mark (fn_008251C0, `ecs/GroundMarks`, see
  [rendering-objects.md](rendering-objects.md#meshes-stuck-to-the-ground-land_morph)); on water **three rings** (growth 5,
  7 and 10; age 0, angle 0, aspect 1, rate 1, cell 0x30, white; +0x24 = 1.0 unidentified; a single
  implementation, `psys::water_rings::AddExplosionRings` of `Particles/PSysWaterRings`). The targets: r =
  MaxDistance × the spell's tribal power between 1 and 5; the `ceil((r + 20) / 10)²` cells of the spiral
  (`GUtils::Spiral` 0x74D7E0) from the centre's one; from each cell, the available mobile and fixed objects **whose own
  cell is that one** (fn_00604F40) and within `Get2DRadius + r` in x/z (`GetDistanceInMetres` 0x74CD70, a hypotenuse).
  A target's world position is its MapCoords +0x14 as a point (x, z as fild × 10 / 65536; GetAltitude 0x803090 + its
  height above the land +0x1C) (0x67E9E1..0x67EA15, 0x67EB8F..0x67EBCF); the target list is cleared with
  fn_00681F10(0). A map cell's objects are read fixed list (+4) first, then the mobile list (+0), each from its head
  (Explosion 0x67E65C..0x67E678, Lightning 0x690344 / 0x69094B, `EventConditionAtomNearVillagers` FindTypeOnMap 0x67D936 /
  0x67D95F).
  Ring = 0. Finally five rocks `MSH_Z_SPELLROCK01` (567) at the centre ± 4 m that break into pieces (see "The
  pieces" below).
- **The crater** (`ecs::ground_marks`, `src/ECS/GroundMarks`; 0x67E35C..0x67E395, only if `MapCoords::IsDryLand` 0x67E353, that is, if the
  rings do not come out). The class is called `RootsPile` (Mac symbols: `__ct__9RootsPileFRC7LHPointffl`,
  `DrawAll__9RootsPileFv`). **It is not `TemporaryShadow`**: that is fn_00825090 (list 0xEB99FC, a dynamic shadow).
  - `new RootsPile(centre, PSysFloatRand(2π), [0x9357D4] = 8, mesh 0x251)` = fn_008251C0 → fn_00825240. The centre is
    the same point that is queried with `IsDryLand` (0x67E347 and 0x67E392 push the same `LHPoint`): the effect's origin, it is not
    lowered to the ground. What moulds it is `UpdateMelting`, which stores per vertex
    −(height under the vertex − height under the origin) / scale (0x81695B and 0x816A59..0x816A77), the same as
    openblack's `MorphWithTerrain`.
  - The mesh is `MeshPack[0x251]` = 593 `TreeRootsPile`, the same mound of earth as the uprooted tree. The first
    `RootsPile` created stores it in [0xEB9A04], and the following ones use that one even if they ask for another. Both callers ask for
    0x251.
  - `LH3DObject::Create(1)` 0x80B4D0: a **morphable** object (vtable 0x9A2E34). With `UpdateMelting` (vt 0x1E8, once)
    it moulds itself to the terrain: on a slope the hole bends with it.
  - `SetPosition` 0x423140 (vt 0x20): the origin at the centre, rotated by the angle around Y, scale 8 on all three
    axes. vt 0x58(1) sets flag 0x20 only with Light detail [0xC38224]; vt 0x40(0) clears 0x10.
  - Life +8 = 0x3A98 = **15000 ms**, and a `SmokyStuff::Create(centre, 1, 1.0, −1)` (0x8252EB). It is the brown dust of
    mode 1: speed of 1.5 × size (0x823DA7), 0x68503D, gone in 1.5 s.
  - **`RootsPile::DrawAll` fn_00825350**, each frame from fn_005E5CD0 (0x5E6197, just before the smoke
    fn_00824140), with `g_game_time_inc` in ms. With ≤ 1000 ms the colour's alpha is `ftol(ms × 0.255)` ([0x9A2BA8]) and
    `SetGlobalAlpha(1)` (vt 0x48) is called; afterwards ms −= g_game_time_inc. With ≤ 0, fn_00825300 takes it out of the list and
    deletes the object; otherwise, it is drawn as a normal object (`AddDrawing` 0x815A70, land light fn_00801C90). It does not
    sink or change scale: it lasts 14 s whole and fades in the last second.
  - `ClearAllStuff` 0x82AEFD deletes them when changing map.
  - In openblack it is `ecs::ground_marks` (see
    [rendering-objects.md](rendering-objects.md#meshes-stuck-to-the-ground-land_morph)): an entity `Transform` + `Mesh` +
    `MorphWithTerrain` (once, `Melting::Snapshot`) (+ `Alpha` in the last second), with the mode 1 `DisappearSmoke` (was `SmokyStuff`).
    **(pending)** the two drawing flags of the `LH3DObject` have no equivalent: 0x20 of the Light detail
    (fn_008168C0) and 0x10 cleared (fn_007F97A0). It casts no static shadow in either (`CastsStaticShadow`
    requires `Fixed` / `MobileStatic` / …, and the entity is not one). The alpha of the last second goes as `Alpha` (the renderer's
    translucent pass), which is what `SetGlobalAlpha` (flag 0x80, fn_007F9D60) does in the original.
    **(approximate)** the land light of fn_00801C90 on the mound's colour is done by the normal mesh lighting.
  - The uprooted tree's mound (fn_0074BD20 → fn_008251F0, scale `(M+0x24 + M+0x2C) × scale × 0.3`) uses the same
    `ground_marks::Create`, and with it the same mode 1 dust.
- **`Object::CanBeDestroyedBySpell`** (vt 0x778, 0x639960; answers event 7, 0x720DBD, which returns `== 1`):
  `IsEffectReceiver(NULL)` and not the flag +0x25 & 0x40, and if it is in a script (vt 0x448 with g_game +0x25005C → +0x45E8
  and +0x45EC) only for a spell with +0x25 & 4. They say 0: `Creature` 0x47B1E0, `Field` 0x529FF0 and `CitadelPart`
  0x4695D0 (citadel heart, parts, pen, worship site, worship totem). **(inferred)** openblack does not carry
  flag 0x40 nor the script objects: they are taken as 0.
- **`Object::DestroyedByBeam`** (vt 0x500, 0x63AB20) = `ToBeDeleted(0)`: trees and dead trees with `DeleteTree`,
  animals with `animal_ai::Remove`, the rest like the generic `DestroyedByEffect`. `Abode::DestroyedByBeam` 0x402CB0
  (all the Abode classes, the store, the dispenser, the totem): `ReduceLife(GetLife(0))`. **(approximate)** the
  villagers go with `life::Kill` (`Villager::ToBeDeleted` is not ported). `DestroyedByBeam`: a field's vt +0x5B8
  (0x52A0A0) changes nothing; `TotemStatue::ReduceLife` 0x737C90 and a dispenser without an Abode take Object's path in
  openblack (approximate).
- **What happens to the building in the original** (it is not a ruin or a ghost):
  - `Abode::ReduceLife` 0x405D90 → `MultiMapFixed::ReduceLife` 0x52F5E0. A built building (`IsBuilt`, vt 0x890)
    uses `Object::ReduceLife`, so the life stays at 0. One half built loses percentage built
    (fn_0052EDD0).
  - With life < 1, each inhabitant (+0xA0, next +0xE4) does `SetStateWhenTappedOnAbode` 0x752B80. If it crosses
    `GetPercentRepairedForNonFunctional` (vt 0x894): `StopBeingFunctional` (vt 0x918) and, if
    `CausesTownEmergencyIfDamaged`, `Town::SetInStateOfEmergency`.
  - With a town and without a building site (+0x74), `Town::AddBuildingSite` 0x73B8E0. With a site, +0x640 = 1.1 × life − 0.1.
  - Drawing: with the site, `IsDrawBuilding` 0x52F0C0 = 1. Without `DestructionMesh` (+0x90, the rocks' one),
    `GetPercentRepairedFromWhenDamaged` 0x52F010 gives 0.98 × life (0x52F09C) and `GetPercentForDrawBuilding` 0x52EFD0
    = min(built, that) = 0.
  - `Abode::Draw` 0x515F70 → `MultiMapFixed::Draw` 0x518090 → `DrawBuilding` 0x517F90: at 0 it draws nothing (0x517FEF),
    only the fire if it is burning. **The building disappears from view** and remains as a building site until it is repaired. Without a town
    there is no site and it is still drawn whole.
  - **(pending)** openblack's towns have no building sites (`Town::AddBuildingSite`, the repair). That is
    why here the life drops to 0 and the building is still drawn whole, as with fire.

### The visible beam (SF_BeamExplosionFX, PT 138)

- `UR_MoveAtom::ModifyAtomCore` 0x6A5E50 (DefineProperties 0x6AE240: +0x20 StartTime, +0x24 StopTime, +0x28
  MoveSmoothly, +0x2C..+0x34 Start, +0x38..+0x40 Stop): between StartTime and StopTime (inclusive) t = (age − start) /
  (end − start), 1 on the step that reaches the end, `t²(3 − 2t)` with MoveSmoothly; the local position = start + (end −
  start) t. The column `MSH_S_BLAST_CENTRE` goes down from 120 m to the ground in 0.4 s.
- `UR_ChangeScaleXYZ::ModifyAtomCore` 0x6A5240 (DefineProperties 0x6ADE60): ruleScale = the interpolated XZ, stretch =
  Y / XZ (0 if XZ ≤ 0.0001, [0x8BF518]); after StopTime, only the first step writes the final values. The four
  cones `MSH_S_BLAST_CONE` open from 0 to 12 in XZ with Y 8.
- The column carries `FaceCamera` (below). The light map `S_BeamSingleLightMap.raw`, the sound `LASERBEAM_1` and the
  `AddSoundToAtom` of `LASERBEAM_EXPLODE` with camera shake were already ported.

### Particle meshes and the shield dome

The drawing of the PSys meshes (`ParticleMeshCreator`, `UsePlayerColor`, `FaceCamera`, `DrawCutByPlane`), which makes
the shield dome and the beam visible, is in [Particle meshes](particles.md#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome).

In openblack: `Creators/Mesh.{h,cpp}`, the `mesh_atoms::Instance` path in
`RenderingSystem.cpp` and the PSys branch of `vs_object`:

- **Materials** (faithful). Each creator rewrites the materials of its mesh with `GJUtils::SetMaterialProperties` fn_0057E1D0
  the first time it looks it up, on its first particle. The port does it on the first `InitAtom`, with
  `L3DMesh::SetMaterialProperties`. Depending on the creator:
  - `ParticleMeshCreator`: fn_006A8A40 0x6A8A5F..0x6A8A6E with the pack mesh and `MeshChangeMaterialProps`;
  - `ParticleMeshCreatorAnimTextured`: fn_006A8CC0 0x6A8CDF..0x6A8CEE;
  - the mesh file: `PGetSharedMesh` 0x57DF18 when loading it;
  - `ParticleAnimCreator`: fn_006A95E0 0x6A9602 with +0x93 = 1, or fn_0057D420 0x57D433 with the file, always.

  `MaterialProperties` = +0x55 additive, +0x56 Z, +0x57 two-sided, +0x58 change, +0x59 alpha (no property, 1 in the
  ctors 0x6A897D / 0x6A8BD0; for the animated one, +0x90..+0x94). The pack meshes change for all their users, as in
  the original. The forest seed (Seed.L3D, type 2) stays at 8 `TexturedAlphaNz`, which blends with the alpha.
  **(approximate)** if two creators load the same file with different properties, here each applies its own; the
  original kept those of the first.
- **`UseGlobalAlpha`** (faithful). It is bit 0 of the particle's +0x24. `Particle3DObj::DrawAt` 0x67A216..0x67A227 /
  `Particle3DAnim::DrawAt` 0x67A9D6 pass it to `SetGlobalAlpha` (vt 0x48 fn_007F9D60, bit 0x80 of the object's +4). With
  it, the drawing uses the table 0xC387C8 (fn_0080DB30 0x80DEED..0x80DF09), which takes the opaque modes 0, 2, 4, 9 and 17 to
  their alpha versions. Without it it uses 0xC38728: the colour's alpha only shows in the blending modes. Who sets it:
  - `ParticleMeshCreator`: **never**. Its property +0x5A is read (0x6B3902), but `CreateParticle` 0x6A8B00 does not use it, and
    the ctor 0x6C7A23 clears the bit;
  - `AnimTextured`: yes, from +0x5A (0x6A8E04..0x6A8E17);
  - `ParticleAnimCreator`: yes, from +0xA5 (ctor 1 at 0x6A93CA, 0x6A983A..0x6A9840; 0 while it blends two meshes at
    0x67A9A4, which no file does).

  In the port, `Instance::globalAlpha`:
  - only the additive ones and those with global alpha with alpha < 1 go with the fading meshes (table 0xC387C8);
  - the others go with their mesh, and the alpha (1 − [0][3]) is only taken by their blending primitives;
  - modes 12 and 13 are the same in both tables, so the additive ones do not change;
  - in the files, the only non-additive `ParticleMeshCreator` is the SF_Forest seed, and its mode 8 is also the same
    in both tables.
- **DrawData specular** (+0xC, the atom's +0x90). `Particle3DObj::DrawAt` gives it
  to the object along with the colour: fn_0080BEC0(colour, specular) 0x67A012, which fn_0080BF10 adds to the ground's specular, or
  `SetColour` vt 0x2C 0x67A023 (obj +0x50, fn_007F9770).
  - `mesh_atoms::Instance::specular` carries it. It goes in the w of the fourth column as 3e6 + 7 bits per channel, like
    `components::SpecularColour`, and the PSys branch of `vs_object` no longer sets it to 0.
  - **(approximate)**: the low bit of each channel is lost.
  - With `DrawWithLandscapeColor` that w carries the colour, so the specular is not sent. No file gives specular to
    a mesh (`SpecColorR/G/B` 0).

### The other missing classes

- `UR_KPStretchHeight` 0x6A50C0 and `UR_KPMoveAtoms` 0x6A60B0 (the mushroom of the powered-up heal, SF_HealChakraPU):
  `KPSplineInterpolator` curves (`Particles/Rules/KeyPoints.cpp`). The property setter (0x6AE170) copies the pairs (t,
  value) and fn_005B3760 computes the second derivatives (the `spline()` of Numerical Recipes) with **slope 0 at the
  ends**, because each rule's ctor sets the array flag (+8 = 1; without it 1e30, natural); `EvalAtT` 0x6A7EB0
  is `splint` (bisection, h²/6), without clipping outside the keys. StretchHeight: stretch = curve(atom age) between
  StartTime and StopTime (ctor 0 and 5). MoveAtoms: position = `GetCurrentParentPos` + (0, curve(age), 0), × index / (n −
  1) with `MovePropAtomIndex` (index 0 is the newest atom, the head of the original's list).
  - `UR_KPStretchHeight` ctor 0x6A4F50: StartTime 0, StopTime 5, keys (0, 1), (5, 1); +0x20 the keys, +0x2C StartTime,
    +0x30 StopTime; the step that passes StopTime evaluates at StopTime; it writes the atom's stretch (+0x7C).
  - `UR_KPMoveAtoms` ctor 0x6A5F40: StartTime 0, StopTime 5, keys (0, 0), (5, 0), MovePropAtomIndex 0; +0x20 StartTime,
    +0x24 StopTime, +0x28 KeyPointsY, +0x34 MovePropAtomIndex; the window is each atom's own age.
  - The key point array setter drops an odd last value (count / 2 × 2).
- `UR_OrientSpriteWithVelocity` 0x69A790 (the flames of SF_FireBallInHand; `Particles/Rules/Orient.cpp`): the first time v =
  the velocity and k = −10 ln(1 − SmoothFactor); each step v += (velocity − v)(1 − e^(−k dt)); u = −v + (0,
  ProportionDefault, 0) in the camera frame and `SetAngleY(atan2(−u_y, u_x) + π/2)` 0x674360. **(inferred)** the
  matrix 0xEA1D28 is taken as the world → camera rotation (x = u · right, y = u · up).
- `UR_ForestPath` 0x6A3770 (`Particles/Rules/Forest.cpp`): with the collection's age, r = RadiusSpline and h = HeightSpline
  (the same KP curves); per atom three random angles (2π) the first time, θ = fmod(age · ThetaSpeed + θ0, 2π), φ
  likewise with PhiSpeed; p = SphereRadius · r (· ScaleSphereRadius) · (cos θ cos φ SX, sin φ SY, sin θ cos φ SZ), +
  GetCurrentParentPos outside a hierarchy, and + h in y; the velocity = movement / dt.
  - Ctor 0x6A35D0: ThetaSpeed +0x20, PhiSpeed +0x24, SphereRadius +0x28 and ScaleX/Y/Z +0x30..+0x38 all 1,
    ScaleSphereRadius +0x2C NULL, RadiusSpline +0x3C and HeightSpline +0x48 with their own default keys (four per curve,
    0x6A363B..0x6A3715, not ported: every spell file gives both) and zero end slopes.
- The rules that derive a velocity from a move multiply by [0xD4E0F0] = 1 / the step (UR_ForestPath, UR_FollowLocalHand,
  UR_HandSprinkle, the shield sparks, UR_Tornado).
- `UR_FollowCastPosn` for this computer's interface also clears the collection's interpolation flag (+0x38 bit 1).
- `ParticleGoodEvilCreator` 0x6AAA00 (DefineProperties 0x6B40C0, ctor 0x6AA990: `AlignmentSwitch` −0.5): the evil creator
  if the effect has a player with alignment (GPlayer +0x60 → +0x08) < AlignmentSwitch, otherwise the good one
  (`Creator::Resolve`, PSys.h). The butterflies move with `UR_Flocking` (see Flocks).

### The pieces (EXPLODE_OBJECT, `Particles/Rules/ExplodeObject.cpp`; faithful)

- **Who asks for them.** `fn_00681260(object, origin, velocity, 6.0f, 0)` (0x67EC86; also `GScript::DeleteObject`
  0x6F93EF / 0x6F9460, which are not used here) takes the world matrix (vt 0x63C) and the 3D object (+0x40);
  `fn_006812B0(3D object, matrix, origin, velocity, 6.0f, flag)` takes its mesh (vt 0xF8; without a mesh it does nothing) and
  puts {mesh, matrix (LHMatrix, 0x30), origin, velocity, 6.0} (0x48 bytes) at the end of the queue 0xD4E320, or of
  0xD4E308 with flag ≠ 0. The 6.0 (a float, not an integer) is read by nobody. The four callers pass flag 0: the queue
  0xD4E308 and `UR_ExplodeObject2` (0x681560 → fn_0067FFB0, RandomFactor +0x2C 0.3, MaxDepth +0x30 4) never do
  anything (registered, fn_0067FFB0 not ported). In openblack the matrix is that of the `Transform` (position, rotation × scale)
  and the mesh that of the `Mesh` component read from `AllMeshes.g3d` (`explode_object::PackMesh`): a mesh that is not from the
  pack (e.g. an already broken building, `BuildingDamage`) does not break (it is written in the trace). `LH3DMesh::MeshPack` is 0xE9FE34 (AllMeshes.g3d).
- **The five rocks** (0x67E79E..0x67E88E, at the end of `InitCollection`): `LH3DObject::Create(0)` with `MeshPack[0x237]`
  (0 if the pack is shorter); `SetPosition` 0x423140 at centre + (rand(−4, 4), 0, rand(−4, 4)) — the **z** rand
  first (0x67E7E3), then the x one —, scale `PSysFloatRand(0.8, 1.2)` ([0x8C4A04] / [0x8C6C98] × [0x9357DC] = 1) and
  then the angle `PSysFloatRand(2π)`; matrix rows X = (cos, 0, sin) s, Y = (0, s, 0), Z = (−sin, 0, cos) s;
  `fn_006812B0(rock, its matrix, centre − 5 m (fn_0067E8C0), BlastSpeed, 6.0, 0)` and the rock is deleted (vt 4): of the rocks
  only the pieces exist.
- **Who empties the queue.** `PSysGlobal::InitializeOneTimeOnly` 0x68F750 only creates the singleton `PSysUtilityPSys`
  (0xD4E0E8). The effect is made by `fn_006718E0` when +0x10 is NULL: `PSysInterface::Create(NULL, 23, (0, 0, 0), (0, 0,
  0), 1.0, 0)`, from fn_006717F0, which `PSysGlobal::GameLoopEnd` 0x68F5B0 → fn_006721B0 calls **once per turn** (step
  11 of `GGame::ProcessTurn`, after the spells): it processes it (vt 0x100) and, if it returns 5 (finished: `MaxSpellAge`
  25 of `SF_ExplodeObject`), deletes it (fn_006718C0) and on the next turn another is made. `OnClearMap` 0x68F820 empties the
  two queues (fn_006811D0, fn_00681200). In openblack: `explode_object::GameLoopEnd` from `magic::ProcessSpellParticlesEndOfLoop` (0x54E688) and
  `Clear` from `magic::OnLoadMap`.
- **`UR_ExplodeObject`** (vtable 0x938A98; ctor 0x6BECA0: +0x10 |= 6, a creating rule; DefineProperties 0x6B0C70:
  `RandomFactor` +0x30 = 0.3 and `MaxTrigsPerFrag` +0x2C = 15; the file sets RandomFactor 0.889381 and does not give
  MaxTrigsPerFrag). `ModifyAtomCollection` 0x6814E0 takes the entries out **from the last one** and calls
  `ExplodeMesh(collection, NextGroups +0x20, mesh, matrix, MaxTrigsPerFrag, origin, velocity)`.
- **`ExplodeMesh` 0x6807B0** (corrects what was said before: they are not vertices but **edges**):
  - Only the submeshes with the flag 0x20000000 (bit 0 of the LOD mask, bits 29..31) and **without the state bits**
    0x3F0 (bits 4..9: scaffolding, gravestones). Each primitive separately.
  - The three edges of each triangle (fn_0067DF00: (t0, t1), (t1, t2), (t2, t0)); fn_0067DFD0 first stores the
    end with the smaller (z + y) + x (on a tie, the second) and the length² ((dz² + dy²) + dx²).
  - `qsort` (0x7C7E64, the one from the MSVC 6 CRT) by length with a comparator that **never gives 0** (0x682550: 1 if
    a ≥ b); of each run of consecutive equal edges (the six floats, fn_00460640) one is kept.
  - For each unique edge, the list of triangles that have it (search fn_00682580: `bsearch` with 0x682770, and if it fails
    a full walk for the first equal one).
  - The pieces: from the first unused triangle, a **path**: the triangle goes in (fn_0057D630), and the next one is
    the first unused triangle that shares one of its edges (corners 0, 1, 2; the list in its order); it continues
    while it finds one and the piece has ≤ MaxTrigsPerFrag, so a piece has **up to 16** triangles. When the
    last one has no free neighbour the piece ends. The next one starts at the first free triangle from the current one.
  - Each piece is an atom (`AtomCore::Create` + fn_00674DD0 with NextGroups) with a `RenderParticleGJMesh` (ctor
    0x6C8A90, +0x21 = 1) whose `GJMesh` (0x94 bytes, ctor 0x67FF20) has 3 new vertices per triangle (position by
    the matrix, uv and the **normal as is, unrotated**) and the primitive's material (+0 = the primitive).
  - Position +0x80 = the centroid (sum × 1/n) in world space, vertices minus the centroid, rotation the identity.
  - A piece's GJMesh layout: +8 the positions about the centroid (0x680E5A..0x680E89), +0x44 the UVs, +0x80 the normals,
    +0x6C the triangles b, b + 1, b + 2; +0x24 differs from the vertex count (no colours of its own). +0x22 (vertices
    stuck to the land) stays 0 for the pieces (ctor fn_006C8A90, 0x6C8AA5).
  - Velocity +0x34 (0x680F34..0x6810AC): d = centroid − origin scaled to `velocity`; r = `PSysRandR3` scaled to
    |d| × RandomFactor; v = (d.x + r.x, d.y + **r.y × 0.5**, d.z + r.z): the 0.5 ([0x8AA3B4]) only goes on Y.
- **Afterwards**, the file's rules (already ported): gravity 3.345 with floor, spin (`AppearanceRuleTumble`), alpha
  255→0 from 0 to 3 s, scale 1→0 from 1 to 5 s, and gone at 6 s.
- **The drawing, `RenderParticleGJMesh::DrawAt` 0x67C150**: colour = that of the `DrawData` × the land light under the
  drawn position (+0x21; fn_00801C90, byte by byte, alpha included; fn_00801C90 gives two outputs: in its 2nd argument
  the interpolated [brightness] table, 0x8020F0, and in the 3rd the cells' own colours, dword +0 | 0xFF000000,
  interpolated the same way, 0x801F81; DrawAt 0x67C184 only uses the first, the other goes to [ebp − 0x40] and nobody reads it; the 255 ×
  255 >> 8 leaves the opaque alpha at 254), the same for all vertices (the GJMesh has no
  colours: +0x24 ≠ vertices); then the model light ([0xC029C0] = 1): the light [0xEA9E90] by the inverse of the drawn
  matrix, normalised, I = fistp(255 n·l), f = I < 0 ? 90 : 90 + ((255 − 90) I >> 8), RGB × f >> 8; the second light
  [0xD4EC08] is 0. With the `DrawData` alpha ≠ 255 (byte +0xB, before the land light) it is drawn with the alpha mode
  table (0xC387C8, 0x67C9B7..0x67C9C0; back to 0xC38728 at 0x67C9F7). World matrix identity
  (0x67C8ED..0x67C979: the vertices are already in world space) and `Draw3DWorldTriangle` 0x81C090 with the
  primitive's material (GJMesh +0, 0x67C9DD..0x67C9F2): **without haze or specular** (0x81C2BF), the material's culling (+5 bit 0,
  0x81C30C / 0x81C556..0x81C58F) and **one** `DrawTriangle` per primitive, **immediately**: it does not look at [0xC0215D], there is no
  Z object. EXPLODE_OBJECT is Sorted: it is drawn by fn_006718A0 with `Draw_(1)` from `PSysGlobal::DrawLoop` 0x68F60C,
  after `GGame::Draw` 0x54E00A and before the queue flush (`FinishFrame` 0x82F460).
  - **In openblack (faithful)**: the pieces are `Creator::Kind::MeshPiece` (was `GJMesh`; outside
    `mesh_atoms`, `psysAtoms` and any Z queue). Each frame, when drawing, `psys::mesh_pieces::Build`
    (`Particles/Rules/ExplodeObject.cpp`) does on the CPU what DrawAt does: the PSR of fn_00679920, each vertex to world space in the
    order of sums of 0x67C29B..0x67C303, the `DrawData` colour × the land light (`LitColour`, the integer interpolation
    of fn_00801C90 0x801CB8..0x8020F7: cells ftol(x × 0.1), weights ftol(frac × 256), per channel a + ((b − a) w >> 8),
    off the map or without a block table[255]), the per-vertex model light with `model_light::LightInMeshSpace` /
    `Intensity` / `Apply` (ambient [0xC39264]) and the table 0xC387C8 if the alpha ≠ 0xFF. It gathers them in batches per source
    primitive (`graphics::world_triangles::Frame`, `Graphics/WorldTriangles.{h,cpp}`, common helper) and
    `world_triangles::Submit` uploads it into **a single transient vertex buffer** per frame and draws one batch per
    primitive with the `WorldTriangles` program (`vs_world_triangles` + `fs_object`: the alpha test and the alpha of the
    models' stage 0, `render_modes::PrimitiveAlpha` / `PrimitiveState` with the material's culling), in
    `RenderPass::Main` after the models and before the queue (`Renderer.cpp::DrawPass`). There is no bgfx mesh per
    piece nor a GPU buffer cap: 6000 pieces alive at once in BEAM_EXPLOSION_PU2 without hanging. The pieces are always
    `Kind::MeshPiece` (world triangles).
  - **(approximate)** the CPU land light does not carry the cloud-shadow cap that the port applies on the
    GPU to the other objects; nor the alternative route [0xEA9EB4] → fn_007ACD90 (unread). The floating-point
    operations are 32-bit (the x87 is not imitated). On the last row / column of the map the original reads cell 17 of the
    block (+0x88 / +0x90); the port takes the last cell. The light normalisation adds x² + y² + z² where
    0x67C5AE..0x67C5BF does (x² + z²) + y² (last bit of the float).
  - **(approximate)** ALPHAREF of modes 9 / 15 with the table 0xC387C8: the original scales it with the object's diffuse
    [0xC37D8C], which DrawAt does not write (it is that of the last LH3DObject drawn); here, with the `DrawData` alpha.
    The software clipping (0x81C28B..0x81C2A0) and the discarding of triangles of area ≤ 0 on the CPU (0x81C4C6..0x81C510)
    stay on the GPU (CULLMODE, same result except for degenerate ones). All the Sorted pieces go together at that point of
    `Main`, not interleaved with the other DrawLoop effects.
  - **(inferred)** no fog (Draw3DWorldTriangle does not call fn_007FEB30); no second colour set on the pieces
    (+0x30 / +0x38, fn_0057D630 not fully read), so never the 0x67CAEE branch; texture sampling with or without
    repetition like the models ([0xECA614] g_b_need_tilling not checked in DrawLoop); there are no pieces in the reflection
    (DrawLoop runs once per frame).
  - (openblack guard) if the transient vertex buffer (32 MB, `init.limits.transientVbSize` in `Renderer.cpp`)
    is not enough, the last batches are not drawn that frame (single warning `world_triangles: ...`).
  - The Queued / Immediate paths (pieces of an effect with a single Z object or of the hand): `Renderer.cpp` builds
    once per frame a `world_triangles::Frame` with `mesh_pieces::Build(Queued)` and `mesh_pieces::Build(Immediate)` and
    `drawOrderedEffect` (branch `Kind::MeshPiece`) draws each atom in its place in the order of fn_006798B0 with
    `world_triangles::Submit(..., atom)` (DrawAt 0x67C150 does not read [0xC0215D]). Today no effect in the
    data uses them: EXPLODE_OBJECT is Sorted **(inferred**: not all the effect files were walked).
  - The texture of a primitive is given by `world_triangles::PrimitiveTexture(mesh, skinId)` (formerly `GetTexture` of
    `Renderer.cpp`): skins of the mesh or of its SetSkinSource, the texture manager, the skin 0x1001 of the extra
    packs and the error only once; `DrawSubMesh` and `DrawStaticShadowPass` also use it.

### Not ported / pending

- `UR_ExplodeObject2` → fn_0067FFB0 (nobody calls it: the queue 0xD4E308 stays empty).
- `manager::SortedFrame` has no row of its own for the pieces (`Kind::MeshPiece` falls into `others`, `PSysManager.cpp`); the
  flushing by draw paths will have to call `mesh_pieces::BuildAtom` for a Queued / Immediate.
- The villager with `life::Kill` (**(approximate)**) and the flag +0x25 & 0x40 and the script objects
  (**(inferred)**): see `DestroyedByBeam` and `CanBeDestroyedBySpell` above; the citadel, below.
- The "destroyed" building: see "What happens to the building in the original" above (the town's building site is
  missing).
- `GetActualObjectToEffect` of the citadel (CitadelHeart 0x468C30, CitadelPart 0x469780).
- `ER_EmitFromParentAtom` and `CreateRule_GameObjectRef` (SF_SparklesFromObject, SF_ButterfliesOnObject, creatures): no
  player miracle uses them.

### Hooks, tests and screenshots

- `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` casts the explosion (`EXPLOSION_ONE_PU_ONE` / `_PU_TWO` for Many /
  Loads); `OPENBLACK_TEST_EXPLOSION_SHOT="<turns>,<path.png>[;...]"` requests screenshots that many turns after the
  first explosion starts ([openblack-internals.md](openblack-internals.md#debug-environment-variables)). With
  `OPENBLACK_SPELL_TRACE=1`: `Explosion: started ...` with its targets (distance, radius, class), `Explosion: ground
  mark ...` with its angle, each destroyed object (ring, exploded, deleted), the queued rocks and each broken mesh
  (`ExplodeObject: mesh ... in N pieces, from ... at ...`).
- `test_explosion`: ChangeScaleXYZ, MoveAtom, the cadence of events 2 and the closing of a file such as Single, the
  player tint, FaceCamera, the KP curves, the good / evil creator and the mode 1 `DisappearSmoke` dust
  at 1.5 × size (the mark's life and alpha are tested by `ground_marks`); the pieces: the path along edges
  (16 + 4 in a strip of 20, loose pieces, MaxTrigsPerFrag 0), the velocity (the random Y halved) and the queue
  emptied into atoms at the centroid (only LOD 0 without state).

## The falling spell: what FallingSpell draws (`Magic/Objects/FallingSpell`, `Graphics/RendererFallingSpell.cpp`)

The `fall.bik` video, its states, sounds, the temple white and the end are in
[video.md](video.md#the-falling-spell-fallbik) (`Video/FallingSpellVideo`). Here, the rest of `FallingSpell`
(fallingspell.cpp, 0x525CB0..0x527290, fully read: `LightBurst::Init` 0x525D30, `LightBurst::Draw` 0x525DF0,
`FallingSpell::Init` 0x526060, the callback 0x526480 → 0x526530, `Close` 0x5264A0, `Draw` 0x5267D0, the
update 0x526E00). Casting: CHL 203 `SET_AVI_SEQUENCE(on, 2)`; in tests `OPENBLACK_TEST_VIDEO=fall`.

**Faithful:**
- **The 16 "sparks"** are not sparks: they are 16 large puffs of `smoke.raw` (material [0xEA1ABC], mode 6) in
  orange, anchored to the screen. `LH3DSprite::Create(0x10, 1)` 0x52631E (+0x34) and 16 records of 0x20 bytes
  (new(0x200) 0x526335, +0x38): `x`, `y` = Random(−0.1; 1.1) (screen fraction), depth Random(5, 25),
  size Random(4, 8), spin Random(−2, 2), shrink 2 − y, colour (min(2v, 255), v, v/3) with v = ftol(Random(16, 100))
  (0x526352..0x526400). Only with `+0x1C` (the update sets it **once** to 1 on moving to state 1, 13.45 s,
  0x527047; `Draw` rewrites it with "some was seen", 0x526DCA). Each `Draw` (0x526BFD..0x526DCA, g_delta_time):
  alpha = ftol(255 − 32·age) with the **previous** age, age += 0.0013·dt; without alpha it is not drawn; half width
  ((size − shrink·age·0.5) + 1)·0.75 (min. 1e−4), angle spin·age + i, cell ftol(age·8) & 15 (rows 0-1 of the
  8×8 sheet), position `Get3DPointFromScreen((ftol(W·x), ftol(H·y)), depth)` 0x81B370 and then
  y += (spin + 1)(1 − y)·0.0013·dt·0.1; `AddDrawing` 0x840C70 (Z-sorter, far to near). They are drawn with
  `LH3DSprite::Draw` mode A (`billboard::Screen`) and FOV π/4 (`ChangeFov` 0x526EB3 before each `Draw`): in pixels,
  half width = size·(W/2)/(tan(π/8)·depth), hundreds of pixels. They are visible from 13.45 s to about 19.6 s (255/32 of age).
- **The bursts** (`LightBurst`, **one** of 0x400 bytes at +0x3C, `Init` called **twice**: 0x52643E and
  0x52644C; they are not "two LightBursts"): 64 rays (R = LocalFloatRand(1) + 0.5, ×1.4 with LocalRand(16) < 2, ×0.714286
  with LocalRand(16) < 1; phase Random(0, 2π); rate Random(2, 20), negative if Random(0, 1) < 0.5; sway
  Random(−0.8; 0.8)). The end-of-frame callback 0x526530, **from state 2** (37.75 s): centre = the midpoint
  of two points of the creature (fn_004813D0, 0x48F180) projected (fn_008190D0; nothing if it ends up behind the near
  plane, **without advancing**); d = 0.7·dt s: +0x2C += 0.1·d (alpha ftol(255·+0x2C), cap 255), +0x28 += 0.25·d and, past
  0.5, +0x30 += 5·d. Four `LightBurst::Draw`: radius 10·g³, 25·g, 50·g, 75·g² (g = +0x30), a5 = +0x2C, −+0x2C,
  −2·+0x2C, 2·+0x2C, colours (R,G,B) (0x20,0xFF,0x40), (0xFF,0x40,0xFF), (0x40,0x80,0xFF), (0xFF,0x40,0x40) with that
  alpha. Each one: a fan of 64 triangles (2i, 2i+1, 2((i+1)&63)+1), radius
  (R + W·sin(a5·B + P))·radius·(4s²k + 1 − k), s = |sin((i+1)·a6²·π/4)|, k = clamp(1 − a6², 0, 1), angle i·π/32 + a5
  (sin a x, cos a y), centre with the colour and edge with only the alpha; the last spoke closes on the first rim point
  (0x525EE6, `and ecx, 0x3F`); UV u = 0.5 + frac(i/64)/4, v = 0.25 + frac(0.4·a5)/4
  (centre) or 0.25 + frac(|1 + 0.4·a5|)/4 (edge; equal with a5 ≥ 0) of `atmos.raw` with `LH3DAtmos::AdditiveMaterial` [0xEDC364] (mode 13),
  `Draw3DWorldTriangle` 0x526047. With a6 = 0 the fan is a point: it grows with +0x28.
  - `LightBurst` (0x400 bytes) is four arrays of 0x40 floats at +0x000, +0x100, +0x200 and +0x300 (Init 0x525D34
    `mov edi, 0x40`); `Draw` writes 0x80 vertices, the centre and rim point of each spoke (0x526042).
  - `LightBurst::Draw` 0x525DF0 (ecx = the burst, 7 stack arguments, `ret 0x1C`): the depth only places the points for
    the Z sort, because `Get3DPointFromScreen` (0x525E31 / 0x525FA1) projects back onto the same pixels. The UV helpers
    0x525CD0 / 0x525D00 are frac(|x|)/4 + 0.25 or + 0.5, with frac = x − ftol(x).
  - The bursts are placed at the near depth [0xE839E0] × 1.1 (0x52666C..0x526676). The burst callback 0x526530 takes no
    step without the centre (0x5265F7).
- The puffs are drawn by `LH3DSprite::Draw` 0x840530 in mode A (flag 0x40 clear). The Z-sorter key is |pos − g_camera|²,
  and a puff at or before the near plane [0xE839E0] is not drawn.
- **The mode 2 order**: `FallingSpell::Draw` draws the video itself (`thedraw(0)` 0x52689F, which clears the player's flag
  +0x64: the 0x8000 callback of `FinishFrame` no longer paints it, 0x844E3A..0x844E49), then
  `FinishFrame`: the Z-sorter (the puffs), the callback 0x526480 (the bursts), the bands and the fade.
  `Renderer::DrawFinishFrameOverlays` does it that way with `HidesWorld()`.
- **The model light**: `Init` stores [0xEA9E90] in +0x10 (0x5262E0..0x526311), `Draw` sets it to (0, 0, 1000)
  (fn_0081E1F0, for the creature), `Close` restores the stored one (0x5264E8..0x5264F4); `FallingSpell::Draw` keeps the
  model light and puts it back (0x526873..0x526895). **Corrects video.md**: what
  `Close` restores is not the camera, it is this light (fn_0081E1F0 = `model_light::SetLight`).
- **The `fall.cm2` path** (it belongs to the spell, not to the video): `LHLoadData` 0x52609C + copy fn_0086D4A0: header
  {size, duration 48 333 ms, 1449 keys} and keys of 0x48 bytes (position, focus and 12 matrix floats).
  fn_0086D760(ms): key (n−1)·t/dur, fraction **(t − ftol(t/step)·ftol(step))/step** with step = dur/(n−1) = 33.379:
  the remainder uses the integer part of the step, so the fraction exceeds 1 from about 2.9 s onwards and the original's camera
  **extrapolates** from the key (ported the same); a·(1−f) + b·f for position, focus and matrix. The update
  (0x526E9B..0x526F1C) and `Init` (0x526259..0x5262BF) give position and focus ×0.8 and the matrix to fn_00819F50, and
  `ChangeFov(π/4)` (only the update; `Init` does not change the FOV). In fn_0086D760, past the duration, t = duration − 1
  (unsigned `jbe`, 0x86D76E..0x86D77A); the key (n − 1)·t/duration is unsigned and wrapped into n; the next key is the
  same one at the end (0x86D7E2..0x86D7F5).
- `Init`: it first closes an open spell (0x52606A..0x526070). `LH3DSprite::Create(0x10, 1)` applies `SetToZero` 0x8404F0
  and the flag 0x80. The burst values are +0x28 = 0, +0x30 = 1.0 and +0x2C = 0, then
  `RegisterFinishFrameCallback(0, 0, 0x526480, this)` (0x526459..0x526463). The path comes from `LHFileLength` /
  `LHLoadData("data\spells\fall\fall.cm2")` (string 0xBE9C78, 0x52607A..0x5260A2).
- `Close`: nothing when it is not open (0x5264A7). Otherwise `RemoveFinishFrameCallback` and +0x00 = 0; the path is
  freed (`fn_0086D4D0`), then the records, the sprites and the burst. `Draw` does nothing when +0x00 is 0 or when there
  is no film (0x5267F4).
- `ChangeFov` 0x8195B0 takes the horizontal angle: [0xC3812C] = tan(fov/2) × near, [0xC38130] = that / aspect
  (0x4424C6..0x4424E2).
- **fn_00819F50** (0x819F50..0x81A74B, called only by `Init` 0x5262BF and the update 0x526F1C; ecx = position,
  edx = focus, stack = matrix, `ret 4`) is **`LH3DTech::UpdateCamera` 0x819920 with the path's rotation**: the same
  beginning (debug camera [0xEA9EC8] / [0xEA9ECC], the shake fn_008210C0, g_camera 0xEA1DB8 and its focus
  0xEA1DC4, the look-at of `UpdateWorldToCamera` 0x81A10D) and the same ending (matrix B 0xEA1C98, g_world_to_clipping
  0xEA9E40 scaled by [0xE83A00] / [0xE83A04], its inverse, the octant [0xEA9EBC], the plane [0xF03128] → [0xF03118],
  the Y rotation matrix 0xEA1D88 from `GetYAngle` + π/2, the light [0xEA9E90] → [0xEA9E80], `SetD3DMatrix` and
  `SetTransform(VIEW)`, the inverse 0xEA1CF8). What differs: from the first nine floats a0..a8 of the matrix
  (0x81A075..0x81A0F5) it makes (a0, a3, −a6, a1, a4, −a7, a2, a5, −a8), normalises each row of three (fn_007FB5C0, with
  the table-based `InverseSquareRoot` 0x841170) and **copies it over the look-at** at 0xEA1D28..0xEA1D48; the translation
  0xEA1D4C..0xEA1D54 = −(column · position) (0x81A112..0x81A22F). So the camera's right, up and forward
  are rows 0, 1 and −2 of the path matrix; the focus does not rotate it (it only counts for the octant, the Y
  rotation and the shake); a9..a11 are not read.
- **Who sets the game camera.** `GCamera::Update` (0x44233C..0x4423F4) with `g_game+0x205A28` = 1 or 2 takes
  g_camera (that of the previous frame, here the path's) as its drawn camera, and with ≠ 0 it skips `ChangeFov`
  (0x4424F0) and `UpdateCamera` (0x4425F6..0x442602); GCamera's zoomers carry on with their own thing. `Close` does not touch the
  camera: **nothing restores it**, the first frame in mode 0 draws GCamera's again (`ChangeFov` 0x4425D3 with its
  FOV, `UpdateCamera` 0x442622). In openblack: `FallingSpell::Hooks::applyCamera` on each update with the path's
  camera and empty in `Close` (the equivalent of mode 0): the FOV with `SetProjectionMatrixPerspective` (π/4
  horizontal, like `cameraXFov`; on closing the configuration's `cameraXFov`) and the view
  `falling_spell::WorldToCamera` with `Camera::SetDrawnView`, which `GetViewMatrix` returns while it is set without touching
  the zoomers. **Pending**: `Camera::SetDrawnView` is not in the camera yet; until it is, only the FOV changes. Without land or a creature in mode 2 nothing
  is seen (pending: the creature). **(approximate)** exact 1/√ and not the `InverseSquareRoot` table; `Init` and the
  first update in the same frame. **(inferred)** the near plane is still openblack's
  (`GetNearClipping` 0x4424AF runs in all modes from GCamera's camera). **Not ported**: the debug
  camera and the shake on the path camera, and the readers of g_camera in mode 2 (`GetWeatherSmooth`
  0x4426BA, GCamera +0x74).

**(approximate)** `Get3DPointFromScreen` without going through the near plane (it cancels out; the original rounds with it); the
Z-sorter key computed in camera space; `Init` runs on the first frame of the video and not in the turn of the
CHL 203 (the Random/LocalRand come out a bit later in their sequences); a `KickOff` over another one is only detected
when the previous one had already had its puffs. Puffs and bursts are drawn in pixels in the
`ScreenOverlay` view with ZFUNC ALWAYS: in mode 2 nothing writes Z before and the reset quad of `FinishFrame` (d) leaves
z = 1, so its LESSEQUAL always passes. **(inferred)** the mode 2 near plane [0xE839E0] is that of openblack's
camera (the depths 5..25 pass it).

**Pending (needs the creature, which openblack does not have):** the `CreatureFalling` (new(0x57B8) 0x5260D1, copy of the
`LH3DCreature` of the player's creature, ctor 0x47F490, vtable 0x8D8BD8; `UpdateTime(1)`, +0x4A90 = 1, `ResetLook`,
`StartIndividualAction(0xD7, 0)`, `HandGlows` 0 and 2 with `SetScalePowerTime(·, 1, 1, 1)`, 0x526105..0x5261EB); its
drawing in `Draw` (0x5268A4..0x526BFB): hidden from 19 550 to 27 350 ms ([0xC64204] = creature + 0x57B8), otherwise a random
tint A = 0xFF, R = 0x20 + LocalRand(32), G = 8 + LocalRand(8), B = LocalRand(4) (0x526971..0x526998) scaled by 255·t/19 450 or 255·(t − 19 450)/7900, `DrawNow` 0x526A42, and
the `SetScalePowerTime` windows of the hand glows (16 350..19 350 and 31 650..32 650 ms for 0; 17 200..20 200 and
36 500..37 500 for 2); its advance `UpdateTime(t − +0xC)` in the update; the debug keys
0xE85376..0xE85379 that rotate it (0, π/2, π, 3π/2); **the centre of the bursts** (without a creature there is no centre: they are neither
drawn nor advanced); that the `fall.cm2` camera (already applied, see above) be visible, because only the creature uses it.

**Tests and screenshots.** `test_falling_spell` (15): indices and geometry of the fan, ranges of `LightBurst::Init`,
the light stored and restored, `Init` of the puffs, nothing without `+0x1C`, position/size/angle/cell/alpha on screen
and far-to-near order, the fade and the rewritten `+0x1C`, bursts only from state 2 and with a centre (alpha,
+0x28, +0x30), the fraction of fn_0086D760 and its drift (t = 48 000: f = 16.36), the camera ×0.8 and the real `fall.cm2`,
the camera applied on each update and restored in `Close`, and `WorldToCamera` like fn_00819F50.
Hooks: `OPENBLACK_TEST_FALL_LOG=1` (one line per second of video: state, puffs, bursts) and
`OPENBLACK_TEST_FALL_BURST_AT=fx,fy` (test: a centre at that screen fraction instead of the creature).

## Creature miracles (pending)

The 16 creature rows of info.dat are MAGIC_TYPE 26..41 (the last block of
[info.dat tables](magic.md#infodat-tables-srcmagicmagictables)). Their spell class **is not ported**: they run as a simple `Spell`. What is already
known and waits for the creature:

- Creature cast rules: vt 0x30 0x5FA7E0 = 0 and vt 0x2C 0x5FA7F0 ([Cast rules](magic.md#cast-rules-magiccastrules));
  `IsCreatureCastFromAbove` 0x5FB7E0.
- The creature's desires on casting (fn_00721730, at the start of `Spell::InitWithPos` 0x71FE50) and the stealing of
  spells by the creature (worship).
- The mimicry (`ConsiderMakingCreatureMimicPlayer`, in water and forest) and the creature in the explosion, the lightning and the
  tornado (`Creature` 0x47B1E0 `CanBeDestroyedBySpell` = 0, `creature+0x12B0`, fn_00477060).
- Creature hooks still to port: `Creature::MaintainSpell` 0x4F8350, `Creature::UpdateSpellInfo` 0x4F8750, the creature
  cast rule's mind test `fn_004F5230`, `ConsiderMakingCreatureMimicPlayer` 0x4EA900 (action from `fn_004E9DF0`), the
  creature defence multipliers 0x478C00, `Creature::ReduceLife` 0x47DD00, and the town magic theft `fn_0073D500` /
  `fn_0073D5A0`.

## Test hooks

All the `OPENBLACK_*` are in [openblack-internals.md](openblack-internals.md#debug-environment-variables).
Per miracle:

- Water: [Tests and screenshots](#tests-and-screenshots)
- Heal: [Hooks, tests and screenshots](#hooks-tests-and-screenshots)
- Flocks: [Hooks, tests and screenshots](#hooks-tests-and-screenshots-1)
- Fireball and lightning: [Tests and screenshots](#tests-and-screenshots-1)
- Shields: [Hooks and screenshots](#hooks-and-screenshots)
- Teleport: [Tests and screenshots](#tests-and-screenshots-2)
- Storm, electric storm and tornado: [Hooks, tests and screenshots (storm)](#hooks-tests-and-screenshots-storm)
- Lightning explosion and missing PSys classes: [Hooks, tests and screenshots](#hooks-tests-and-screenshots-2)
- Food and wood: the "Tests" bullet of [Food and wood](miracles.md#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource).
- Forest: the "Hooks" and "Tests" bullets of [Forest](miracles.md#forest-magicspellsspellforest-magicobjectsmagictree-ecstrees).

## Sources

- runblack.exe W120 (the addresses throughout this page) and, where cited, the bw1-decomp headers.

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x671879` | the explode effect's per-turn process call with zeroed process info (power 1.0) | `explode_object::GameLoopEnd` |
| `0x67188E` | the finished explode effect (result 5) deleted; remade next turn | `explode_object::GameLoopEnd` |
| `0x6718ED..0x671932` | creation of the always-on EXPLODE_OBJECT effect (type 0x17, zero position/direction, power 1.0) when none | `explode_object::GameLoopEnd` |
| `0x67C175..0x67C184` | mesh piece draw: land light looked up under the matrix translation | `mesh_pieces::AppendPiece / explode_object::LitColour` |
| `0x67C189..0x67C1F6` | mesh piece draw: DrawData colour multiplied by the land light byte by byte | `explode_object::LitColour` |
| `0x67C47F..0x67C4C4` | mesh piece draw: vertices without own colours take the base colour | `mesh_pieces::AppendPiece` |
| `0x67C4CE..0x67C4E4` | mesh piece draw: model light only with the lighting switch on and a normal per vertex | `mesh_pieces::AppendPiece (lit)` |
| `0x67C508..0x67C5E4` | mesh piece draw: light brought into mesh space through the inverse drawn matrix, normalised | `model_light::LightInMeshSpace` |
| `0x67C602..0x67C62C` | mesh piece draw: intensity = fistp(255 * dot(light, normal)) | `model_light::Intensity` |
| `0x67C631..0x67C6A3` | mesh piece draw: RGB scaled by the intensity factor >> 8, alpha kept | `model_light::Apply` |
| `0x67C635 / 0x67C63D` | mesh piece draw: reads of the global ambient [0xC39264] | `model_light::Ambient` |
| `0x67E01F` | edge build: jne on sQ <= sP (C0 or C3) puts the second point first | `MakeEdge` |
| `0x67E060..0x67E084` | edge build: squared length (dz dz + dy dy) + dx dx | `MakeEdge` |
| `0x67E87E` | the rocks' call of fn_006812B0 (queue flag 0) | `explode_object::QueueMesh` |
| `0x6807E2 / 0x6807F1` | ExplodeMesh sub-mesh test: LOD 0 flag 0x20000000 set, status bits 0x3F0 clear | `k_SubMeshLod0 / k_SubMeshStatus` |
| `0x68088B..0x6808E2` | ExplodeMesh: three edges per triangle (fn_0067DF00 -> fn_0067DFD0) into the edge array 0xD4E338 | `explode_object::SplitPrimitive` |
| `0x6808F8` | ExplodeMesh: qsort of the edges by length with comparator 0x682550 | `explode_object::SplitPrimitive (CrtQsort)` |
| `0x68091A..0x68099D` | ExplodeMesh: of a run of equal neighbouring edges the last one kept | `explode_object::SplitPrimitive` |
| `0x68093E / 0x68094E` | ExplodeMesh: the two fn_00460640 calls comparing an edge's two ends | `SameEdge` |
| `0x6809E0..0x680AF0` | ExplodeMesh: per unique edge the triangles that use it | `explode_object::SplitPrimitive (users)` |
| `0x680AF6..0x680D6C` | ExplodeMesh: the piece walk over shared edges up to MaxTrigsPerFrag | `explode_object::SplitPrimitive` |
| `0x680B9B` | ExplodeMesh sets the RenderParticleGJMesh land-light flag (+0x21 = 1) | `mesh_pieces::AppendPiece (LitColour)` |
| `0x680D47..0x680D68` | ExplodeMesh: next start = first unused triangle | `explode_object::SplitPrimitive` |
| `0x680D93..0x680E15` | ExplodeMesh: vertices to world through the matrix, summed | `ExplodeMesh (centre)` |
| `0x680E26..0x680E54` | ExplodeMesh: centroid = sum x (1 / n) | `ExplodeMesh (centre)` |
| `0x680E8B..0x680F31` | ExplodeMesh: atom rotation identity (SetRotationMatrix), position +0x80 = centroid | `ExplodeMesh (atom.rotation/position)` |
| `0x680F8A..0x680FCD` | piece velocity: direction scaled to speed / sqrt((x x + z z) + y y) | `explode_object::PieceVelocity` |
| `0x680FD9..0x681001` | piece velocity: random part length | `d` |
| `0x681068` | piece velocity: random part's Y halved ([0x8AA3B4] = 0.5) | `k_RandomYFactor` |
| `0x681073..0x681093` | piece velocity: random part added to d | `explode_object::PieceVelocity` |
| `0x681253` | a caller of fn_00681260 (queue flag 0) | `explode_object::QueueObject` |
| `0x6812C5` | fn_006812B0 returns when the 3D object's mesh (vt 0xF8) is NULL | `explode_object::QueueMesh` |
| `0x6814F5..0x68153F` | UR_ExplodeObject::ModifyAtomCollection loop emptying the queue from its last entry | `ExplodeObject::ModifyCollection` |
| `0x6825A9..0x682628` | fn_00682580: linear fallback walk when bsearch misses | `FindEdge` |
| `0x6BED00` | UR_ExplodeObject2 ctor: RandomFactor +0x2C = 0.3, MaxDepth +0x30 = 4 | `k_DefaultRandomFactor / k_DefaultMaxDepth` |
| `0x7C7EAF` | CRT qsort: insertion (short sort) for size <= 8 | `CrtQsort` |
| `0x7C7EE3` | CRT qsort: middle element as pivot | `CrtQsort` |
| `0x7C8141` | CRT bsearch (MSVC 6) | `CrtBsearch` |
| `0x801F83..0x8020E2` | land light lookup: per-channel lerp a + ((b - a) w >> 8), imul/shr/mask | `explode_object::LandLight (lerp)` |
| `0x8020E9` | land light lookup: alpha forced to 0xFF | `explode_object::LandLight` |
| `0x938ABC` | UR_ExplodeObject2 vtable | `ExplodeObject2` |
| `0xD4E2A0` | mesh piece draw: transformed vertex buffer | `mesh_pieces::AppendPiece (vertices)` |
| `0xD4E310` | count of the second explode queue, zeroed by fn_006811D0 on map clear | `explode_object::Clear` |
| `0xD4E328` | count of the first explode queue, zeroed by fn_00681200 on map clear | `explode_object::Clear` |
| `0xD4E338` | ExplodeMesh's edge array (0x1C-byte edges) | `explode_object::SplitPrimitive (edges)` |
| `0xD4E350` | ExplodeMesh's unique edge array (0x30 bytes: the edge and a GJArray of triangles) | `explode_object::SplitPrimitive (unique/users)` |

## Pending

What is missing from each miracle, in its section:

- Water: [Not ported / pending (water)](#not-ported--pending-water)
- Heal: [Not ported / not verified (heal)](#not-ported--not-verified-heal)
- Flocks: [Pending / not faithful](#pending--not-faithful)
- Shields: [Not ported / UNVERIFIED](#not-ported--unverified)
- Storm, electric storm and tornado: [Inferred, approximate and pending (storm)](#inferred-approximate-and-pending-storm)
- Lightning explosion and missing PSys classes: [Not ported / pending](#not-ported--pending)
- Food and wood: the voice `GGuidance::ResourceDropSFX` 0x71B570 (the guidance channel and the town fields are missing) and the
  diversion of the wood of a store under construction (`StoragePit` +0x74 = `BuildingSite`, unreachable without building sites).
  The vt 0x78/0x80 of `MagicFood::CallVirtualFunctionsForCreation` are already identified and met
  ([Food and wood](miracles.md#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)).
- Forest: the goddess and the camera (postponed; the flapping of the butterflies and the bats is done)
  ([Forest](miracles.md#forest-magicspellsspellforest-magicobjectsmagictree-ecstrees)).
- Fireball: the `DrawOffset` that draws the locally cast ball from the hand (fn_006C7840).
- Lightning: `LightningForkFlicker` 0x6B24D0 and the EffectValues of lightning without a spell (`NumTexturesToTile`, the recursive fork tree fn_00691F30 and `DrawOffsetLT` are done) ([Lightning](miracles.md#lightning-magic_type-4-6-seed-6-lightning_bolt-particlesruleslightningcpp)).
- Teleport: the lighting of the pool (`UseLighting` 1 of `SF_TeleportVortex`) and the creature ([Teleport](miracles.md#teleport-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)).
- [Creature miracles](miracles.md#creature-miracles-pending), and everything that waits for the creature: the falling
  creature in FallingSpell, enemy creatures in the heartbeat, the creature in water / forest / lightning / tornado.
- Town aggression (`Town::ProcessPlayerInteract` 0x73DEC0, villagers): without it the protection desire only rises
  with a script boost, so villagers seldom react to shields.
- Physics: the pair skip with +0x1A4 == 1 and flag 2 (0x64583E..0x645866) in `Substep`.
- (approximate) the box centre turns only on xz; (inferred) the off-map delete when the object that stays is
  another one (Tree → DeadTree); not tested in game: the off-map delete, the heartbeat and the chants.
- Unverified: `LeaderEvent` (SpellFlock.cpp): "SpellEvent{2 (point), the leader's position, its movement (MobileWallHug::GetMovementDirection 0x60C040: the step +0x64 / +0x6C, zero before its first move), 1.0, no shield test}". miracles.md "The exit loop" step 6 says `SpellEvent{2, its position, no movement}`. The two disagree. UNVERIFIED.
- Unverified: `UpdatePhase` (SpellSeedGraphic.h): "The PSys global phase [0xD4EBF8] (PSysGlobal::DrawLoop 0x68F680: + ms x 0.001 / 3.33, 0..1), per frame". miracles.md (Lightning, DrawOffsetLT) names 0x68F680 `DrawOffsetDecay::ProcessList`. UNVERIFIED which one it is.
- Unverified: `Particles/Rules/Fireball.cpp` `SpeedFromHand`, at `if (flat.x * flat.x + flat.z * flat.z < 0.1f)`: "// 0x69EA5C / 0x69EA6E / 0x69EA81 (0x8AB22C = 0.1)". What the original does when the horizontal direction is that short is not written. Unverified.
