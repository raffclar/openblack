# Coordinates, terrain, object size, game clock, matrices and Zoomer

Basic mathematics of the original engine (LH3D) and how they are ported to openblack: the fixed point of positions, with its
cells and its spiral; the `GUtils` distances and sigmoids; the game clock; the exact terrain height; the
terrain normal; the LH matrices; object size (2D radius and height), and the `Zoomer` interpolator. Everything is **faithful** (verified
in the executable) and ported, except what is marked in [Pending](#pending).

> **Code rules.** These are pure modules (`map_coords`, `gutils`, `ecs::object`, `game_clock`, `affine` (was `lh_matrix`),
> `Zoomer`, `game_random`) with no global state beyond the game's own single clock and random state, reached through
> their namespaces; constants are `constexpr` `k_PascalCase`, written with `std::bit_cast` of the original's bits where
> the bits matter; they are unit tested with fakes in `test/`; comments describe behaviour in plain English, with
> no decompiled names or addresses (those belong here). See [openblack-internals.md](openblack-internals.md).

- [MapCoords](#mapcoords): fixed point, cells, `InBounds`, neighbours and spiral (`map_coords`)
- [GUtils distances](#gutils-distances): table root, `hypotenuse`, `GetDistanceInMetres`,
  `FastDistance` and the sigmoids (`gutils`)
- [GUtils angles](#gutils-angles): `LHArcTan`, the COS/SIN tables, the conversions, from an angle to a
  position, the difference and the direction (`gutils`), and the points around an object (`ecs::object`)
- [Object size](#object-size): 2D radius, radius and height, with the class overrides and
  the derived ones, at mesh level and at object level (`ecs::object`)
- [Game clock](#game-clock): the turn, the turn's ms, the fraction, the frame dt, the pause and the
  speed (`game_clock`)
- [Object lists per cell](#object-lists-per-cell-ecsmap_cells): the two sorted lists of each cell,
  the cells of an object, the searches and the town traversal (`ecs::map_cells`)
- [Terrain height](#terrain-height)
- [Terrain normal](#terrain-normal): `LH3DIsland::GetNormal` and its two tables (`land_normal`)
- [LH matrices](#lh-matrices): the LHMatrix constructors, the inverse and the model (`affine`)
- [Zoomer (LH3DLib)](#zoomer-lh3dlib): `Zoomer` and `Zoomer3`, bit-exact
- [Random numbers (`game_random`)](#random-numbers-game_random): LHRand, the two GRand seeds, the
  PSys streams and the CRT `rand()`
- [Pending](#pending), [Test hooks](#test-hooks), [Sources](#sources)

## MapCoords

✅ Faithful and ported in `src/3D/MapCoords.{h,cpp}`, namespace `openblack::map_coords` (2026-10-01). It is the original's only representation of a position on the map and everything uses it: there are 136 calls to the
`MapCoords(LHPoint)` constructor, 196 to `GetLHPoint`, 71 to `ToMap`, 80 to `InBounds` and 58 to `GUtils::Spiral`.

**Structure** (bw1-decomp `MapCoords.h`): `{int32 x (+0), int32 z (+4), float altitude (+8)}`. x and z are in 16.16: the
high word is the 10 m cell and the low word the fraction (one cell = 0x10000). `altitude` is the height **above the
ground**; the absolute one is `GetAltitude(pos) + altitude`. `JustMapXZ = {int16 x, z}` is a step in cells.

**The FPU runs at 24 bits** (fn_007DEE00, `and 0xFCFF` at 0x7DEE0D; called in `pc_main` and in every `GGame::EndTurn`).
That is why every product is rounded to float. The exact replica is `(int32)(m * 6553.6f)` done in float. Doing the
product in double gives ±1 unit (0.15 mm) in 41 % of the values.

| API (`map_coords`) | Original | What it does |
|---|---|---|
| `k_FixedPerMetre` = 6553.6f | [0x8AC400] = 0x45CCCCCD (6553.60009765625) | metres → 16.16 |
| `k_MetresPerFixed` = 10/65536 | [0x8AA3A4] = 0x39200000, exact | 16.16 → metres |
| `k_MapCells` = 512 | g_game+0x59C4 / +0x59C8 (`GMap::Init(0x200, 0x200)`, 0x6014C8 / 0x6014F1) | cells per side |
| `ToFixed(m)` | `fld; fmul [0x8AC400]; __ftol` 0x7A1400 (`Set` 0x603346..0x603367, 258 inline copies) | truncates towards 0 |
| `ToMetres(f)` | `fild; fmul [0x8AA3A4]` (`GetLHPoint` 0x605C40 = `ConvertToLHPoint` 0x6041C0) | a single rounding (above 2^24, a prior `(float)f` would round twice) |
| `ToFixedGUtils(m)` | `fmul 65536 [0x8AC408]; fdiv 10 [0x99A1BC]; __ftol` (0x74D52F, 0x74D595, 0x74D85A … 0x74F3A3; `MapCoords::SetX` of bw1-decomp) | a **different** function: 1464 m gives 9594470, and `ToFixed` gives 9594471 |
| `Quantise(m)` | `ToMetres(ToFixed(m))` | the position as a MapCoords stores it; **it is not idempotent** (`ToFixed(ToMetres(8090858))` = 8090857, as in the original) |
| `CellOf(f)`, `CellX`/`CellZ`, `Cell`, `CellOf(vec2/vec3)` | `xor; mov si, [ecx+2]` (**unsigned** high word, `ToMap` 0x603433) | a negative is cell 0xFFFF: outside |
| `SignedCellOf(f)` | `movsx` of `JustMapXZ` (0x5E1950; `ApplyEffectToMapPos` 0x525212) | the signed high word |
| `InBounds(MapCoords / ivec2 / vec3, cells = 512)` | `MapCoords::InBounds` 0x6042C0 = `JustMapXZ::InBounds` 0x5E1860: `jae` against +0x59C8 (cx) and +0x59C4 (cz) | the 512 map, not the extent of the land |
| `CellIndex` | `ToMap` 0x603430 = 0x5E1950: `cx * [+0x59C4] + cz`; the cell is at g_game + 0x59FC + index·8 | −1 instead of NULL |
| `AddCells(MapCoords&, JustMapXZ)` (`JustMapXZ` was `JustMapXZ`) | `operator+=(JustMapXZ)` 0x605470: `add word [ecx+2]`, `add word [ecx+6]` | 16-bit addition: the fraction is preserved |
| `k_Neighbours4` | 0xDA59FC (filled by 0x74CA10): (1.0), (0.1), (−1.0), (0,−1) | the spiral table; also read on its own (0x602CC4, 0x768374, 0x770104) |
| `k_Neighbours8` | 0xDA59D8 (filled by 0x74CA60 and read by fn_00504143): (1.0), (1.1), (0.1), (−1.1), (−1.0), (−1,−1), (0,−1), (1,−1), (0.0) | another table |
| `Spiral{dir = 1, count = 1}.Next()` | `GUtils::Spiral` 0x74D7E0: `dec [count]; jne` (0x74D7E9); `++dir; count = dir/2` (cdq/sub/sar, 0x74D7EF..0x74D7F7); then `&tabla[dir & 3]` (0x74D7FE) | first updates and then reads |
| `SpiralIncrement(MapCoords&, Spiral&, step)` | `GUtils::SpiralIncrement` 0x74D810 (only called by `Town::FindClearArea`, at 0x741384) | the same rule and then `x = ToFixedGUtils(tabla.x · step + ToMetres(x))` (0x74D836..0x74D8A8) |
| `CellSpiralSize(r)` | `GetMapCellSpiralSizeFromRadius` 0x74F520: `n = ftol(r · 0.2 [0x8AA3AC])`; `cmp eax, 1; jae` (unsigned: only 0 becomes 1); n² | 4 callers |
| `IncrementSpiralSize(r, step)` | `GetIncrementSpiralSizeFromRadius` 0x74F540: `n = ftol(r · −2 [0x8C7CE0] / step)`; (1 − n)² | 1 caller |
| `FromWorld(vec3)` / `ToWorld(MapCoords)` | `MapCoords(LHPoint)` 0x603160 / `Set` 0x603340 (`altitude = y − GetAltitude(this)` at the truncated position, 0x603371..0x60337C) and `GetLHPoint` 0x605C40 (the sum ground + altitude at 0x605C4D) | the height is given by the island's `GetHeightAt`; without an island it is 0 |

**The spiral.** All the callers reviewed start with `dir = count = 1`: for example `Reaction::SpreadReaction`
0x6E3E51, `GMagicHealInfo::FindTargets` 0x5FBB9A, lightning 0x6902A7..0x6902BB, the tornado 0x6D22E9 or `Tree::EndPhysics`
0x74B98A. They process the central cell first and then advance with `+= *Spiral()`. The `InBounds` check is done on
each cell; the radius is not clipped. The steps from the start are (−1.0), (0,−1), (+1.0)×2, (0,+1)×2, (−1.0)×3,
(0,−1)×3, (+1.0)×4…

The number of cells is computed by each caller, and those formulas stay where they are:
- `CellSpiralSize`, in 4 places.
- `ceil(2R/10)²` in heal (0x5FBB51).
- `max(3, ceil(2R/10))²` at 0x604AF8.
- `ceil((r+20)/10)²` in the explosion (0x67E5F2).
- `4·ceil(R/10)²` in lightning (0x690276).
- `ftol(ceil(…))²` in FireFly (0x52A6A7).

**What is not MapCoords** (not merged):
- The 64×64 grid of fn_005E1890 / `SpellGrid` (`x>>16>>3`).
- The polar spiral of `SpellForest::SpellEvent` 0x725830.
- `fistp(x·0.1)` of `AttemptToAddSoundEvent` 0x6465DF (`sea_cells::RoundedCellOf`).
- `ftol(x·0.1 [0x8AC404])` of `GScript::GetLandHeight` 0x6FB1F0, of `LandAvoid` ([0x8AB22C]) and of the creation of
  `CitadelHeart` (0x8827C7..0x882810, with `jl`/`jg` against 0..0x1FF). `CitadelArchetype.cpp` already does it that way, so the
  «fix» the plan proposed for the citadel was a false positive.
- The distances (`GetDistance` 0x74CCB0, `GetDistanceInMetres` 0x74CD70): see [GUtils distances](#gutils-distances).

**What already uses the API.**
- `sea_cells::CellOf/InBounds` and `MapInterface::GetGridCell` are wrappers. `GetGridCell` is no longer UB with negatives:
  it returns the unsigned high word (0xFFFF).
- These copies were migrated:
  - animals: AnimalAI and AnimalAIDetail (the constant, `CellOf`, `InBounds` and the spiral), AnimalLairs,
    AnimalPredators and AnimalWallHug;
  - villagers and town: VillagerSpeed and TownQueries (conversions, sizes, `SpiralIncrement` and the spiral of
    `CheckForClearArea`), AbodeQueries, StreetLantern and WorshipSite;
  - map and objects: WaterQueries, PotResource, MapProduction, MapCollide, Trees and MobileWalkPaths;
  - effects and magic: Reactions, EffectValues, FireEffect, CastRules, `magic::ToMap`, `Spell::castPos`, SpellFlock and
    SpellWater;
  - weather: Climate and WeatherLand;
  - particles: PSys `Flock`.

**Fidelity fixes** it brought:
- Double products changed to float:
  - WaterQueries: `k_WorldToFixed` was double, and so was the `x·65536.0·0.1f` of `FindNearestStreamPos`.
  - TownQueries: it used the double 6553.6 (not even the float value); also `SpiralIncrement` and the sizes.
  - AbodeQueries (the door, 0x63AFF2), SpellFlock and PSys `Flock`.
- Climate: the return conversion used 0.000152588f = 0x39200008 instead of [0x8AA3A4] = 0x39200000 (3.9 mm at 5 km).
- `InBounds` by the extent of the land, with strict `>` (AnimalAI.cpp, Reactions.cpp), changes to 0x6042C0: x = 0 and
  the cells without a block inside the map now count.
- `floor` instead of truncating (FireEffect, CastRules): it differed for x ∈ (−1.5e−4, 0).
- `GetGridCell(pos − radius)` with negatives (UB):
  - `ApplyEffectToMapPos` now does what the original does: corners with `ToFixedGUtils(ToMetres(x) ∓ r)`, signed
    high words and `InBounds` on each cell (0x52514F..0x525259).
  - `MapProduction` walks the cells with sign and skips the ones outside.
  - A mobile outside the map does not enter the grid (`ToMap` gives NULL).
- `magic::ToMap` and the hand's `castPos` (0x72056B..0x720595) now truncate to 16.16. `ToWorld` does not truncate again,
  because a second round trip can lose one unit.
- `SpreadReaction`: the size is `CellSpiralSize` (0x74F520, with its unsigned `jae`) and the position advances with `AddCells`
  on MapCoords (0x6E3F6E), instead of adding 10 m in float.
- `MobileWalkPaths`: the return conversion no longer rounds twice above 2^24 units.

**Second pass (audit, 2026-10-01).** What was missing from the first, checked again in the binary:
- **All spirals now advance on a MapCoords**, not on a cell in an `int` nor on metres in float. In the
  original the caller copies its MapCoords and moves it with `operator+=(JustMapXZ)` 0x605470, which adds **16 bits to the
  high word**: the fraction does not change and the cell wraps around. That is why a spiral that starts to the left of the map
  (x ∈ (−10, 0), cell 0xFFFF) enters cell 0 with the first `+1` step, whereas with the cell in an `int` it reaches
  0x10000 and stays outside forever. Places corrected and their original:
  - `Magic/CastRules.cpp` `FindHealTargets` ← `FindTargets` (copy 0x5FBB6B, `InBounds` 0x5FBBBA, `ToMap` 0x5FBBCB,
    `Spiral` 0x5FBCE5, `+=` 0x5FBCF1); the `InBounds(vec3(celda·10))` that went round cell → metres → fixed becomes
    `map_coords::InBounds(coords)`.
  - `ECS/Fire/FireEffect.cpp` (copy 0x72F5E1, `InBounds` 0x72F60C, `GetDistanceInMetres` 0x72F674, `Spiral` 0x72F6C3,
    `+=` 0x72F6D0). The distance is measured between the two MapCoords: since the fraction is the same, the difference is
    whole cells. `CellObjects` uses `map_coords::InBounds` instead of its own copy. The `CellObjects` of
    `Particles/Rules/Storm.cpp` no longer repeats the `InBounds`: its only caller does it, the loop of fn_006D21B0 (0x6D2311).
  - `Magic/Spells/SpellWater.cpp` (copy 0x7250A2, `Spiral` 0x725166, `+=` 0x725173; 9 cells, `ebp = 9`).
  - `ECS/AnimalAI.cpp`: `CalcRandomPos` ← `Living::CalcRandomPos` (0x5ED0FE..0x5ED152 the starting point with
    `ToFixedGUtils`, `+=` 0x5ED1C8), `LookForFoodPos` ← `Animal::LookForGrazePos` (`+=` 0x41A945) and the merging of flocks
    (`+=` 0x41A76A); `ECS/AnimalPredators.cpp` `FindPrey` ← fn_00419490 (`+=` 0x41954B). `detail::Spiral::Advance`
    wraps `AddCells`.
- `TownQueries`: `TruncateToInt` (was `Ftol`) receives a **float** (the comment said «x87 extended», which contradicts the `and cw, 0xFCFF` at
  0x7DEE0D). The congregation average (0x7409F3..0x740A1E) does `fild qword` (exact) and `fdiv` at 24 bits, so the
  quotient is rounded once to float before the `__ftol`: with 100 positions it goes above 2^24 and the truncation could come out one
  unit different. `GetPosFromAngle` (0x74D580) uses `ToFixedGUtils`; the cosine is in double (see
  [GUtils angles](#gutils-angles)).
- `Climate`: `CellCentre()` is removed (the high word with a **signed** offset × 10). Neither `ProcessAll`
  (0x771DA0) nor `FindWhereToCreateStorm` (0x772D3E, 0x772D6F) reads the high word: both build the LHPoint with
  `fild; fmul [0x8AA3A4]`, i.e. `Centre()`. The centre cell in `FindWhereToCreateStorm` (0x772C38, 0x772C65) is indeed
  the high word, but **unsigned** (`xor eax, eax; mov ax, [ebp+0x16]`), and it is added with `fiadd`: it is already `CellOf`.
- `SpellFlock`: `DestinationAt` (0x7238E2..0x723905) reads the unsigned high word (`CellOf`) and computes in float, not in
  double; `IsPosOnCorridor` (0x420E67) uses `CellOf` likewise.
- `VillagerSpeed`: the conversion back to metres is marked **(inferred)**. `MobileWallHug::SetSpeed` 0x60FC50 stores the u16 as
  is in +0x5A and the original never converts that speed to metres (it adds it to a MapCoords); openblack stores it in
  metres, so it uses `ToMetres` because it is the original's conversion everywhere else.
- `k_MapCells`: the citation was wrongly attributed. 0x6014C8 and 0x6014F1 are **inside** `GMap::Init` 0x6014C0; the call
  `GMap::Init(0x200, 0x200)` is in `GGame::Init` (0x54F650+0x2A0).

## GUtils distances

✅ Faithful and ported in `src/Common/GUtilsDistance.{h,cpp}`, namespace `openblack::gutils` (2026-10-01).
It is the original's family of distances (the `Utils` unit, 0x74CCA0..0x74F780, plus two `MapCoords` functions): about
**450 direct calls** in `runblack.exe`. It sits on top of `map_coords`. All the «good» distances are **2D
(x, z)**: y is never used.

**Everything goes through a table-based inverse root.** `InvSqrt` 0x74F620 reads a 1024-entry table at 0xDA5A10 that is filled
only once (fn_0074F590, with the flag [0xDA6A10]; `GUtils::SetupUtils` 0x74CCA0 is just a `jmp` to it, and it is called by
`GGame::InitOneTimeOnly` at 0x54F07B, right **after** setting the FPU to 24 bits). The error of the root is from **−0.097 %
to +0.092 %** (full sweep). Visible consequences: a 10 m cell measures 10.0049 m, 100 m measure 100.0244 m and
400 m measure 400.0977 m. `1/InvSqrt(1)` does not give 1, it gives 0.99951171875 (0x3F7FE000).

**The FPU runs at 24 bits** (fn_007DEE00, `and cw, 0xFCFF` at 0x7DEE0D). That is why **the whole module is in float**, without
double and without FMA: three of the openblack copies did the sum or the quotient in double and drifted (see below).

| API (`openblack::gutils`) | Original | Notes |
|---|---|---|
| `InvSqrtTable()` | fn_0074F590, table 0xDA5A10, flag [0xDA6A10] | entry i = the top 10 bits of the mantissa (`& 0x7FE000`) of 1/√f, with f = `0x3F000000 \| i << 14`; an exact 1 is stored as 0x7FE000 (0x74F5ED) |
| `InvSqrt(x)` | `_FUN_0074f620` 0x74F620 | `exp = ((0xBE000000 − (bits & 0x7F800000)) >> 1) & 0x7F800000`, mantissa from the table at `(bits >> 14) & 0x3FF`; the sign is not checked, and x = 0 gives ≈ 2^63 |
| `Hypotenuse(int32, int32)` | `hypotenuse` 0x74F680 | 16.16 in and out: `x = dx·2^-16` [0x99A1D4], `s = float(z·z + x·x)`, `ftol(65536.0 [0x99A1D8] / InvSqrt(s))`. It **truncates** and has no cutoff at zero |
| `Hypotenuse(float, float)` | `hypotenuse` 0x74F6C0 | 0 if \|a\| and \|b\| are both ≤ 1e-4 [0x8BF518] (`test ah, 0x41` is also satisfied by an unordered comparison: a NaN side counts as «≤ 1e-4»); otherwise `1 / InvSqrt(float(a·a + b·b))` [0x8AA390]. It does **not** truncate |
| `ConvertWholeDistanceToMeters(i)` | 0x74DCC0 | `fld 10 [0x99A1BC]; fmul 2^-16 [0x8AC41C]; fimul i`: the integer is exact before rounding (= `map_coords::ToMetres`) |
| `ConvertMetersToWholeDistance(m)` | 0x74DCE0 | `ftol(m / 10 · 65536 [0x8AC408])`; 65536 is a power of two, so it is `ToFixedGUtils` |
| `GetDistance(MapCoords, MapCoords)` | 0x74CCB0 = its twin 0x74CCE0 (byte for byte) | `Hypotenuse(b.x − a.x, b.z − a.z)` |
| `GetDistanceToCell(MapCoords, JustMapXZ)` | fn_0074CD10, with fn_0074E2D0 = `(short(celda) << 16) + 0x8000` | to the **centre** of the cell |
| `GetDistanceInMetres(...)` | 0x74CD70 = 0x74CD50 = `MapCoords::GetDistanceInMetres` 0x605CD0 | `ConvertWholeDistanceToMeters(GetDistance(a, b))`. **383 calls** (202 + 62 + 119). Overloads: MapCoords, `vec3`/`vec2` in metres (truncating to 16.16 like 0x603160) and `ivec2` in 16.16 |
| `GetDistanceInMetresToCell` | fn_0074CD90 | |
| `GetDistance(vec3, vec3)` | `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0 | the x/z differences stored as float (0x74CDEC, 0x74CDFA) and then `Hypotenuse(float, float)`: metres, without going through MapCoords |
| `GetMetresDistanceSq` | `MapCoords::GetMetresDistanceSq` 0x605FB0 | the **exact** square in float: no table, so it is **not** `GetDistanceInMetres` squared (100 × 100 m gives 20000, not 20018) |
| `FastDistance` | `GUtils::FastDistance` 0x74CE10 | `max + (min >> 1)` (`sar`) in MapCoords units: it is not a Euclidean length |
| `ChebyshevDistance` | fn_0074CED0 (with the `abs` fn_0074DD00) | `max(\|dx\|, \|dz\|)`, compared **unsigned** (0x74CF05) |
| `k_Sigmoid`, `detail::k_SigmoidBits` | table 0xC23284, 41 floats in .data | **the bits** are copied; T[0] = exactly 0 and T[37..40] = exactly 1. It is a logistic `1/(1+e^(−1.0232·(i−20)))` *(inferred: by fitting)* |
| `SigmoidThreshold(a, b)` | `GUtils::SigmoidThreshold` 0x74F170 | `a == 1 → 0` (0x74F174; a NaN in a too); otherwise `T[min(ftol((clamp(clamp(b,−1.1) − a,−1.1) + 1)·20.5 [0x99A1D0]), 40)]`. **The threshold is the FIRST argument** |
| `GetDistanceModifier(d, max)` | 0x74F290 = its copy fn_005ECA20 (`ret 8`) | `SigmoidThreshold(0.5 [push 0x3F000000], 1 − min(d, max)/max)`: it **decreases** with distance, from T[30] = 0.99996 at d = 0 to T[10] = 3.6e-5 at d ≥ max (21 of the 41 steps). With max = 0, `0/0` gives NaN and the result is T[0] = 0. `Tree::ApplyWaterSpell` calls it with max = 3 at 0x74C41D (the immediate 0x40400000 at 0x74C40C) |
| `DistanceChangeToBelief(x, y)` | `GBelief::DistanceChangeToBelief` 0x438770 = its copy fn_00657F30 | `SigmoidThreshold(−0.9 [0xBF666666], float(−(x/y)))`: another curve over the same table |
| `CreatureSigmoidThreshold(a, b)` | `Creature::SigmoidThreshold` 0x4F78C0 | adds `b ≤ 0 → 0` (fcomp 0; test ah, 0x41) |

**Routines that are NOT merged** (they give different results): the two `hypotenuse`; `GetDistanceInMetres` (quantised to
1/65536 of a cell) versus `GetDistance(LHPoint)` (direct float); the distance to a cell (to the centre, +0x8000);
`GetMetresDistanceSq` (no table); `FastDistance` and Chebyshev; `SigmoidThreshold` versus the Creature one;
`GetDistanceModifier` (a = 0.5) versus `DistanceChangeToBelief` (a = −0.9).

**Dead code that is not ported** (no `call`, references or pointers): 0x74CDB0, 0x74CE50, 0x74CE80, 0x74F660,
0x74F720 and 0x74F740. The W120 symbols are also wrong in two places: 0x74CD50 is called `ReactionInfo::GetInfo` (it is the
twin of `GetDistanceInMetres`) and `hypotenuse` 0x74F680 appears as `void` when it returns an int in eax.

**Fidelity fixes it brought.**

1. **`WorshipScore` fn_0073C590 had the arguments of `SigmoidThreshold` reversed** (`WorshipPercentage.cpp`): it passed
   `(x, 0.5)` instead of `(0.5, x)`, so the curve came out **mirrored**. With d = 0 it gave 0 where the original gives
   0.99996, and with d ≥ max it gave 0.99996 where the original gives 3.6e-5. Since `AdjustWorshipersWorshipping` 0x73C0F0 sorts
   from highest to lowest score (0x73C180..0x73C1A6), openblack sent **the farthest villagers to pray first**;
   the nearest ones go first. The error was repeated in `WorshipPercentage.h`, in `test_worship.cpp` and in `magic.md`.
2. **`WorshipScore` multiplies by life³, not by life²** (0x73C63A..0x73C644: `mov eax, 2`, and two iterations of
   `dec eax; fmul life; jne` over st0 = life; the modifier is multiplied at the end, at 0x73C646).
3. **`VillagerFire::DistanceModifier` was an invented `smoothstep`**: with max = 400 m it gave 0.156 at 250 m where the
   original gives 0.0444, 0.352 at 220 m (original 0.264) and it saturated to 1 / 0 at the extremes instead of
   0.99996 / 3.6e-5. The threshold that follows (> 0.1, [0x8AB22C]) moved. It is `GetDistanceModifier(d, 400)`, with the
   immediate 400 at 0x765A77 and the distance from 0x765A81.
4. **The `Trees.cpp` table was rounded to 4 decimals** (35 of the 41 entries different; 20 of them within the
   range 10..30, the only one `GetDistanceModifier` uses: T[1..10] were 0 and T[30..36] were 1).
5. **The `WorshipPercentage.cpp` table was rounded to 5 decimals** (the same, T[1..8] = 0 and T[32..36] = 1).
6. **The `AnimalLairs.cpp` sigmoid was computed in double**: at the table's jumps (±40 ulp) a different index came out in
   50-60 of 3321 cases, and 1 in 300 000 with random values.
7. **`hypotenuse(int)` in double** (WaterQueries and AnimalLairs): different in 31 678 of 200 000 samples versus the
   24-bit emulation; almost always 1 unit, but when changing table bucket it reaches 2431 units = **0.37 m at
   1 km**.
8. **`TownQueries::GetDistanceInMetres` used `std::hypot`** without the table (100 m gave 100 m, not the original's 100.0244 m)
   and converted the integer to float **before** multiplying, whereas the original uses `fimul` on the
   exact integer (different in 22 572 of 100 000 integers above 2^24, i.e. more than 2560 m).
9. **`AnimalWallHug` `MoveToCircleHug`** did the root and the ×128 − 1 in double; in the original the two constants are
   loaded as `qword` but the FPU is at 24 bits, so it is all float, and the base is `GetMetresDistanceSq` 0x605FB0
   (0x60D9F0), the exact square.
10. **`FeatureScriptCommands::FindNearestTown`** compared **squared distances**; the original calls fn_00605CD0
    (0x553016, 0x55302E) and compares the distance, so with the table and the quantisation two almost tied towns
    could come out the other way round.
11. **`CastRules::FindHealTargets` measured from the casting point** (the error was older). The original
    (`GMagicHealInfo::FindTargets` 0x5FBB00) only has one MapCoords in the frame, the copy of the argument
    ([ebp−0x24], 0x5FBB6B..0x5FBB82), and it is the one that **walks the spiral** (`operator+=` 0x605470 at 0x5FBCF1). Both cutoffs measure from it:
    `GetDistanceInMetres(coords, objeto)` at 0x5FBBEE `< R`, and the exact square 0x5FBC54..0x5FBCA3
    (`fild [ebp−0x24]` / `[ebp−0x20]`; `(coords − objeto)²` `< R²`, `test ah, 0x41`). So heal takes any
    object within R of the **spiral point** that visits its cell, not of the centre of the miracle.
12. **`AnimalFlee` `AnimalReaction`** (`ApplyReactionToLivingObjectsAtSquare` 0x6E3F90, the ongoing reaction when
    comparing with a new one) measured to the initiator. The original takes `Reaction::GetPos` 0x6E45C0 (0x6E4142), extracts its
    **cell** with fn_005E17C0 (the high words, 0x6E414C) and measures with fn_0074CD90 (0x6E4157) from the animal's MapCoords
    to the **centre** of that cell (`GetDistanceInMetresToCell`): up to 7.07 m of difference in the distance that
    feeds the score fn_006E4620 (0x6E4173).

**What already uses the API.**

- The three copies of the 1/√ table and of `InvSqrt` were deleted (`WaterQueries.cpp`, `AnimalLairs.cpp`, `CHLApi.cpp`), as were the
  four `hypotenuse` and the three private `SigmoidThreshold` (AnimalLairs, Trees, WorshipPercentage).
- `WaterQueries` (`DistanceInMetres`, the `NearestCoastal` cutoff and the distance to the river points),
  `AnimalLairs` (`MapDistance` and `ForestScore` fn_0053AD00), `CHLApi` `GET_DISTANCE` (0x6F8CA0 → 0x74CDE0),
  `TownQueries::GetDistanceInMetres` (and with it `VillagerDecide` and the town search radii).
- Float substitutes replaced by the API **only where the original's call has been read**: `FireEffect` and
  `VillagerFire` (`Distance2D`, its eight uses read one by one: `HeatTransfer` fn_0072F980 0x72FA44,
  `NearestFireToFight` fn_00730070 0x73010A, `IsBesideFire` fn_0075ABA0 0x75ABC7, `OnFire` 0x75B27B,
  `ReactToFirePriority` 0x765610 at 0x76567E and 0x76582B, `ReactToFire` 0x765870 at 0x7658D9 and 0x765A81), `Reactions` `SpreadReaction` (0x6E3E91), `Climate` `FindWhereToCreateStorm` /
  `CreateStorm` / fn_00772330 (0x74CDE0), `CastRules` (the heal radius, 0x5FBBEE, from the spiral), `SpellFlock::WolfArrived` (0x421300),
  `SpellWater::ApplyWaterSpell` (0x7250EC), `EffectValues::ApplyEffectToMapPos` (0x525307), `Trees`
  (`DistanceToForest` 0x53A890 / 0x53AC20 and the scenic forest), `AnimalAI` (`PosWithinDomain` 0x5ED010,
  `SetNewWander` 0x41A3F0, `KeepFlockMemberWithinFlockArea` 0x41ABB0), `AnimalFlee` (`ReactToFoodPriority` 0x5F1710,
  `SetupReactToFlyingObject` 0x4204A0, `ProcessReaction` 0x5F1270; `AnimalReaction` with `GetDistanceInMetresToCell`
  0x6E4157), `AnimalPredators` (fn_00419340), `AnimalWallHug` (0x60D9F0), `StreetLantern` (`GStreetLantern::Create`
  0x7346E0: walks the cell with `MapCoords::FindType(0x1C)` 0x6045C0 and cuts off with `d < 0.5` [0x8AA3B4], `test ah, 1`
  at 0x73470C) and `FeatureScriptCommands::FindNearestTown` (fn_00552FF0). Careful: `GStreetLantern::IsALaternWithinDistance`
  0x734A30 is a **different** routine (the global list of lanterns g_game+0x205C34 and `d <= r`, `test ah, 0x41`), not ported.

## GUtils angles

✅ Faithful and ported in `src/Common/GUtilsAngle.{h,cpp}`, namespace `openblack::gutils` (2026-10-02).
It is the angle family of the `Utils` unit (0x74D0C0..0x74E2D0). It sits on top of `map_coords` and the
distances.

- **Game angle**: 11-bit integer, **2048 per turn**, stored as `u16` (MobileWallHug +0x5C). 0 = +x,
  0x200 = +z, 0x400 = −x, 0x600 = −z. It is `atan2(dz, dx)` in 2048ths with the arctangent table: at most
  **2.27 steps** of error (≈ 0.4°; at 10 m, about 7 cm).
- **3D angle**: radians in float, the same direction, in [0, 2π) when it comes out of GUtils. `Get3DAngleFromXZ` is **not**
  `atan2` in float: it is the game angle quantised and converted to radians.
- **«Scawen» angle**: the 3D one + π/2 (creature, PBall, `Dove::Dying`, `GetFacingDirection`).
- The game-angle routines are integer (`LHArcTan` does `shl 8; div`). The FPU runs at 24 bits, but **fsin/fcos are not
  rounded by the precision control**: they come out in extended precision and are rounded to float, only once, by the following `fmul`.
  That is why `GetPosFromAngle`, `AddDistanceFromAngle` and `GetPointFromAngle` take the cosine in **double** and round
  the product once. With `cosf` it is rounded twice: with an exact model of fcos, 38 of 20 000 values (0.19 %)
  come out 1 MapCoords unit different; with the double, none.

| API (`openblack::gutils`) | Original | Notes |
|---|---|---|
| `ArcTanTable()` | table 0xC2307C (257 `u16`, .data) | `trunc(atan(i/256)·1024/π)`; in double with trunc it gives the 257 entries (with round, 122 differ). Only LHArcTan reads it |
| `SinTable()`, `Sin(a)`, `Cos(a)` | tables 0xC31614 (2560 `i32`) / 0xC31E14 = SIN + 512 | `trunc(65536·sin(i·2π/2048))`; the 2560 entries match (with round, 1220 differ). The original indexes with `a & 0xFFFF`; here `a & 0x7FF` *(inferred: the same for everything the game passes)* |
| `LHArcTan(dx, dz)` | `?LHArcTan@@YAXHH@Z` 0x74D0C0 | x = −dx (0x74D0C5); the eight branches with signed comparisons (`jl`) and unsigned divisions; ties go to the first branch; `& 0x7FF` (0x74D1E2). `n << 8` keeps the low 32 bits, like the `shl` |
| `GetAngleFromDXDZ(dx, dz)` | 0x74D200 | `LHArcTan & 0xFFFF` |
| `GetAngleFromXZ(from, to)` | 0x74D240 (0x74D220 with four integers) | overloads MapCoords, `ivec2` (16.16) and `vec2` (metres: each point becomes a MapCoords **before** subtracting) |
| `Get3DAngleFromXZ(from, to)` | 0x74D270 | `ConvertGameAngleTo3D(GetAngleFromDXDZ(to − from))` |
| `ConvertAngle3DToGame(r)` | 0x74DC30 | `ftol(r · 325.94931 [0x99A1C8]) & 0x7FF`: truncates, and a negative wraps around (−0.5 → 1886). A round trip loses 1 in **365 of 2048** angles |
| `ConvertGameAngleTo3D(a)` | 0x74DC50 | `(a & 0x7FF) · 0.0030679617 [0x99A1CC]`, one rounding; it is bit for bit `float(a) · 2π_f / 2048` |
| `ConvertScawenAngleToGameAngle(r)` | 0x74E290 | `ConvertAngle3DToGame(float(r − π/2 [0x8C78D8]))` |
| `ConvertGameAngleToScawenAngle(a)` | 0x74E2B0 | `float(2a) · 0.0015339808 [0x8C78DC] + π/2`, **without** `& 0x7FF` |
| `GetXByAngle` / `GetZByAngle(a, int d)` | fn_0074D320 / 0x74D340 | `(C·d) >> 16` with 32-bit `imul` and `sar` |
| `GetXByAngle` / `GetZByAngle(a, float d)` | fn_0074D360 / 0x74D380 | `float(C) · d · 2^-16` |
| `StepFromAngle(a, whole)` | fn_0074D3A0 / 0x74D3C0 | `((whole >> 4)·C) >> 12`, both `sar` (signed: `whole` is `int32_t`). The MobileWallHug step |
| `StepFromAngleCoarse(a, whole)` (was `StepFromAngle8`) | fn_0074D3E0 / 0x74D400 | `((whole >> 8)·C) >> 8` |
| `GetX/ZByAngleMetersDistance(a, m)` | 0x74D420 / 0x74D450 | `ftol(float(C) · float(m / 10))` |
| `GetPosFromGameAngle(a, int whole)` | fn_0074D650 | `{StepFromAngle(a, whole), 0}` |
| `GetPosFromGameAngle(a, float m)` | fn_0074D6A0 | the same with `whole = ConvertMetersToWholeDistance(m)`; the `sar 4` discards the 4 low bits |
| `GetPosFromAngle(r, m)` | 0x74D580 (60 callers) | `x = ftol(float(cos(r)·m) · 65536 / 10)`, z with sin, altitude 0 (the literal of `mov [esp+8], 0` 0x74D587); the `GetDistanceInMetres(origen, p)` at 0x74D5F4 is discarded, and with it its temporary origin `{ftol(0 / 10), ftol(0 / 10), 0}` |
| `AddDistanceFromAngle(p, r, m)` | 0x74D510 | `p.x = ftol((float(cos(r)·m) + ToMetres(p.x)) · 65536 / 10)`, same for z; the altitude does not change |
| `GetPointFromAngle(r, m)` | fn_0074D620 | `(cos(r)·m, 0, sin(r)·m)` in float |
| `GetAngleDifference(a, b)` *(inferred name)* | fn_0074D740 | `d = \|a − b\|`; `d > 0x400 ? 0x800 − d : d` |
| `GetAngleSign(from, to)` *(inferred name)* | fn_0074D6F0 | `d = to − from`; 0 → 0; if `\|d\| > 0x400` (unsigned) it wraps around; −1 if d < 0, otherwise +1. **With \|d\| == 0x400 it does not wrap around**: +0x400 → +1, −0x400 → −1 |

`MapCoords` gains `operator+` 0x605520, `operator-` 0x6055C0, `+=` 0x605410 and `-=` 0x6054A0 (in `3D/MapCoords.h`):
they add or subtract x, z **and the altitude**.

**Points around an object** (`ecs::object`, `ObjectMetrics.h`): they are all `this + GetPosFromAngle(ángulo, r)`
with `MapCoords::operator+`, so they keep **this's altitude**. Each one has its own radius, and they are not interchangeable
with one another:

| API (`ecs::object`) | Original | Angle and radius |
|---|---|---|
| `MapCoordsOf(e)` | Object +0x14 | `map_coords::FromWorld` of its `Transform` |
| `GetNearestPosOfObject(this, o)` | 0x636D30 | `G3D(this, o)`, `R2D(o) + R2D(this)` (vt +0x64 of both) |
| `GetNearestEdgeToPos(this, p)` | 0x636DA0 | `G3D(this, p)`, `R2D(this)` |
| `GetNearestEdge(this, ángulo, extra)` | 0x636DF0 | the angle is given by the caller; `R2D(this) + extra` |
| `GetWorkingPos(this, o)` | 0x639550 | `G3D(this, o)`, `R(this) + R(o)` (**GetRadius**, vt +0x60) |
| `TreeGetWorkingPos(tree, o)` | `Tree::GetWorkingPos` 0x74C040 | `G3D(tree, o)`, `R2D(o) + 0.9` [0x8C5844]: only the other's radius |
| `BigForestGetArrivePos(bosque, v)` | `BigForest::GetArrivePos` 0x439360 | `G3D(bosque, v)`, `R(bosque) · 0.5` [0x8AA3B4] |

**Outside the API** (no callers in the original): 0x74D2A0 (the angle between two LHPoints) and 0x74D770 (turning towards an
angle with a maximum step), with no `call`, `jmp`, `jcc` or pointers in the whole image. 0x74D480 (the «octagonal» step) is only
called by fn_005E1890, not ported. **They are not of this family**: `LH3DMath::GetYAngle` 0x841290 and fn_007FAA50 (LH3D, see [LH matrices](#lh-matrices)),
`Atan2Positive` 0x7DB770 (gestures), PuzzleGame's own conversion 0x6F184C.

**Fidelity fixes it brought.**

1. **The animals' `AngleDiff` at 180°** (`AnimalAI.cpp`): `((b − a + 1024) & 2047) − 1024` gave −0x400 when the
   turn was exactly +0x400, and the original (`GetAngleSign` 0x74D6F0, `jbe` at 0x74D709) turns positive. An animal
   facing exactly opposite its goal turned to the other side, and the birds' banking came out with the sign
   flipped. `SetTowardsAngle` (0x418560) now uses `GetAngleSign` and `GetAngleDifference`, like the original.
2. **`GetPosFromAngle` with the cosine in double** (before, `std::cos(float)`, double rounding): it reaches all the
   callers of `town_queries::GetPosFromAngle` (Abode, VillagerDecide, VillagerShield, the congregation).
3. **Unquantised float angles** changed to `Get3DAngleFromXZ` + `GetPosFromAngle` on MapCoords:
   `VillagerFire` (`GetFireFightingPos` 0x75AAE2 / 0x75AB59 and the flight of `OnFire` 0x75B368), `Trees` (fn_0053A010
   0x53A094, `Tree::GetWorkingPos`, the forest edge of fn_0053ADB0 = `GetNearestEdgeToPos`,
   `BigForest::GetArrivePos` and `AddTreeAround` 0x439264), `WorshipSite::GetSpellIconPosFromSlot` 0x77AFC0,
   `AnimalFlee` (`Object::GetWorkingPos` 0x639550) and `Rock::SplitInTwo` 0x6E75B1 (`pos + o` and `pos − o`, 0x6E76A9 /
   0x6E76CE, on this +0x14: the two halves **keep the rock's altitude**, `map_coords::FromWorld` /
   `ToWorld`, as with the tree and the forest of point 5; before they were placed on the ground).
4. **`GetSpellIconPosFromSlot` sets the altitude to 0** with ring > 0 (0x77B002, `mov [esp+0x14], 0` = MapCoords +8)
   before the `+=`: the icon ends up **on the ground**. openblack kept the height above the ground of the special point.
5. **`Tree::GetWorkingPos` and `BigForest::GetArrivePos` keep the altitude** of the tree / forest (`operator+`); before
   the terrain height was simply taken.
6. **`IsPosValidForTurnAngle`** (0x41B210): the centres of the two turning circles are `me + fn_0074D6A0(a ± 0x200, R)`
   in MapCoords, with the `sar 4` that discards the 4 low bits of R, and the distance is `GetDistanceInMetres` 0x74CD70 (with
   the table), not `glm::distance`. R is converted to metres with `ConvertWholeDistanceToMeters` (× 10 / 65536), not with / 6553.6.
   There is no turn test: with `turnAngle` 0 the quotient is inf (NaN without speed), `__ftol` gives 0x80000000,
   R = −327680 m and both distances exceed it (true); the `turn <= 0` branch that was there was removed (it made no difference).
7. **`CalcRandomPos`** (0x5ED0DB..0x5ED152): the random offset is `AddDistanceFromAngle` on the centre's MapCoords
   (before it added in float metres). The final output was already the original's: `me + fn_0074D650(+0x5C, 10)` is
   `me + (0, 0)` because `10 >> 4 = 0`. The two random numbers are `GameFloatRand` (0x5ED0BE the angle, 0x5ED0D2 the
   radius, the latter **always**, without the `range > 0` branch that was there): `GameFloatRand` 0x6DE530 / fn_005106B0 gives 0 with 0 and
   otherwise `float(LHRand(0xFFFF)) · max · 1/65535` ([0x8D6050] = 0x37800080), also with a negative `max`. `SquarePos`
   (fn_0074F310) uses the same one. *(Approximate)* `LHRand` 0x7DB600 is here openblack's generator (0..0xFFFE). The centre
   arrives in metres, as openblack stores positions, and becomes a MapCoords with `FromMetres`; the original receives the
   MapCoords, so a centre that was not one may already end up one unit off (`Quantise` is not idempotent).
8. **The bird formation** (fn_0041E890, 0x41E96A..0x41EA05) is not `AddDistanceFromAngle`: x uses `row` and z uses
   `column`, and the order is `(cos·row)·10` (`fimul` and then `fmul 10`), two roundings, on the leader's MapCoords.
9. **`Dove::Dying`** (0x41F1B0): the velocity is `(sin(s)·v, 0, −cos(s)·v)` with `s = ConvertGameAngleToScawenAngle`;
   the same mathematically, different in bits.
10. **The animals' `AngleOf(vec2)`** subtracted in metres and then truncated: now each point becomes a MapCoords and they are subtracted
    (`GetAngleFromXZ`), like the original. Nine uses (AnimalAI, AnimalBirds, AnimalFlee, AnimalPredators,
    AnimalWallHug).
11. `VillagerFire` `OnFire`: the two `GameFloatRand` (angle and distance) were arguments of a single call, with no guaranteed
    order; now the angle goes first, as at 0x75B32D..0x75B34A.
12. **`SetNewWander`** (`Animal::SetNewWander(MapCoords const&, int, int)` 0x41A3F0): the distance becomes an integer with
    `__ftol` (0x41A421) and is compared **as an int** with `rMax` and `rMin` (0x41A426 `cmp; jle`, 0x41A430 `cmp; jge`). Before
    the float was compared: with `d` in (rMax, rMax + 1) the animal went towards the centre and in the original it does not.
13. **`__ftol` 0x7A1400** is a single function, `map_coords::FtoL` (`MapCoords.h`), used by `ToFixed`,
    `ToFixedGUtils`, `CellSpiralSize`, `IncrementSpiralSize`, `ConvertMetersToWholeDistance` and GameAngle (before,
    `static_cast`, undefined out of range). It imitates the SSE2 branch (`HasSSE2` [0xE83A20], `cvttsd2si`, the one every
    current CPU takes): towards 0, and 0x80000000 for NaN or outside the int32 range. The x87 branch (0x7A141F, `fistp qword` and the
    correction towards 0, the low 32 bits of the int64) would give another value outside that range; it is not reproduced.
14. Remaining copies: `VillagerCore.cpp` `setGameAngle` (the constant 0x99A1CC by hand) uses
    `gutils::ConvertGameAngleTo3D`, like `SetGameAngle` 0x60DAA1; `SpellFlock.cpp` (the two orientations,
    0x74D240) uses `gutils::GetAngleFromXZ(created, target)` instead of `AngleOfMapCoords` with the subtraction done.

**What already uses the API.** `town_queries::GetAngleFromXZ` / `Get3DAngleFromXZ` / `GetPosFromAngle` and
`animal_ai::detail::Cos` / `Sin` / `Step` / `AngleOfMapCoords` are one-line forwards to `gutils` (the copies of the
tables and of `LHArcTan` were deleted from `AnimalAI.cpp`). `AngleOf(vec2)` and `AngleDiff` no longer exist.

## Object size

✅ Faithful and ported in `src/ECS/ObjectMetrics.{h,cpp}`, namespace `openblack::ecs::object` (2026-10-01). These are the virtual functions of `Object` that give the 2D radius, the radius and the height of an object from
its mesh's box: about **540 calls** in the original (151 to vt+0x64, 48 to vt+0x60 and 343 to vt+0x42C, heuristic
count). openblack had them hand-written 33 times, with 6 wrappers, and each copy knew at most one of the
class overrides.

**The mesh box.** `LH3DMesh::ComputeBoundingBox` **0x8081B0** (on load) joins the boxes of all the submeshes and
stores in +0x18..+0x20 the centre, in **+0x24 / +0x28 / +0x2C the half-extents** `(max − min) × 0.5` [0x8AA3B4]
(0x80831C..0x80835B) and in **+0x30 the half-diagonal** `√((hz² + hy²) + hx²)` (0x80835E..0x808379). An animated mesh
(flag +4 bit 0x100) first goes through `LH3DAnim::SetTransform` 0x83A1D0; openblack does not do this (see Pending).

**Two levels.** The original has both, and they do not give the same result:
- **Mesh level**: reads the fields inline, without going through the vtable, so it **does not see any override**.
  `IsSuitableForFixed` 0x603E1E..0x603E5B, fn_00604020 0x604042, `Scaffold` 0x6E956A and 0x6EAC14..0x6EAC56, 0x7350A5,
  the creature 0x4778E9..0x4779B1; half-heights in `Field::Draw` 0x5287B9..0x5287D3, `PhysOb::Initialise` 0x7FB7D9,
  `Tree::Draw` 0x74ABB0, `WorshipTotem::Create` 0x780995, `CitadelHeart` 0x4653FE / 0x467777,
  `Abode::DrawPercentFull` 0x407111, `TownArtifact::Draw` 0x51C9A3 and others. There a field measures what its mesh measures.
- **Object level**: the virtual call, with the table of overrides of the `??_7` in symbols.txt that derive from
  `Object` (sweep of slots +0x60, +0x64, +0x120, +0x13C, +0x42C, +0x568, +0x590, +0x5F4, +0x630, +0x64C,
  +0x6C4, +0x798 and +0x7C4 in all the vtables, 2026-10-01). The classes that are **not** `Object` are not covered:
  the `Citadel` 0x8C7E68 (and the `Planned*`, `SpellSeedGraphic`) keeps `GameThing` 0x405140 / 0x405150 = 0,
  `GameThingWithPos::GetHeight` 0x405500 = 0 and `GetScale` 0x4247E0 = 1; `SpellShield` 0x72B440 (`GetSpellMagnitude`
  0x7202C0) / 0x72B450, `SpellStormAndTornado` 0x72D950 / 0x72D960, `Town` 0x73D6E0, `GArena` 0x424780, `Reaction`
  0x55C7D0, `BuildingSite` 0x43D050 and `AtomCore` 0x673C70 have their own `GetRadius` / `Get2DRadius`; `GStreetLight`
  0x735110 (radius 20 [0x8C7658], fn_00735060) and `Mist` 0x6067D0 (`Mist::Get2DRadius` 0x606660) their own
  `GetDistanceFromObject(MapCoords)`. Nobody asks the API for them: openblack's temple is the `CitadelHeart` (`Temple`),
  which is an `Object`.

Each place is ported **at the level the original uses at that point**: a single-level API would put a 5 m field into
`IsSuitableForFixed` or into the construction sites. Everything is in float (FPU at 24 bits, fn_007DEE00), without double or FMA.

| API (`ecs::object`) | Original | What it does |
|---|---|---|
| **Mesh level** | | |
| `HalfExtents(box)`, `MeshHalfExtents(meshId)` | +0x24/+0x28/+0x2C, 0x80831C..0x80835B | `(max − min) × 0.5`; without a mesh, nothing |
| `HalfDiagonal(half)`, `MeshHalfDiagonal(meshId)` | +0x30, 0x80835E..0x808379 | `√((hz² + hy²) + hx²)`, in that order |
| `Radius2D(half, s)`, `MeshRadius2D(meshId, s)` | 0x6381B1..0x6381E2; inline at 0x603E1E, 0x604042, 0x6E956A, 0x6EAC22, 0x7350A5 | `s × max(hx, hz)` (the `fcompp` takes hx if hz < hx) |
| `Height(half, s)`, `MeshHeight(meshId, s)` | 0x638136..0x63813D; inline in `Tree::Draw` 0x74ABA2..0x74ABC0 | `2 × (hy × s)` (`fmul` and then `fadd st0, st0`) |
| `MeshHalfHeight(meshId)` | 0x5287C5, 0x7FB7D9, 0x74ABB0 | +0x28, unscaled |
| **Object level** | | |
| `GetScale(e)` | vt+0x120: Object 0x402520 = the +0x50 field; Creature 0x47B190 → `GetUserSize` 0x4EF4F0 | the uniform scale of the `Transform` (x); that of a `MapShield` is its `objectScale` (`SetScale` 0x639200), not the drawn one. The creature's is that of the `Transform` **(inferred)** |
| `GetScaleField(e)` | the +0x50 field read by `GetHeight` (0x638139) | not the virtual one |
| `FootprintRadius(e)` (was `ObjectGet2DRadius`) | `Object::Get2DRadius` 0x638180 itself (called directly by `PileFood` at 0x66F192) | `GetScale × max(+0x24, +0x2C)`; without a mesh 0 (0x6381E9) |
| `ObjectHeight(e)` (was `ObjectGetHeight`) | `Object::GetHeight` 0x638120 itself | `2 × +0x28 × [+0x50]`; without a mesh 0 (0x638140) |
| `Get2DRadius(e)` | vt+0x64 | Field 0x528E80 and FishFarm 0x52C470 = **5** [0x8AB6E4]; MagicTeleport 0x5FCCB0 → 0x5FCCA0 = **6** [0x92C108]; MagicFireBall 0x682D20 = `GetScale × 1` [0x935910]; PileFood / MagicFood / PuzzleGrain 0x66F180 = `GetProportionRaised × Object::Get2DRadius`; Creature 0x477F40 (not ported: see Pending); the rest, 0x638180 |
| `GetRadius(e)` | vt+0x60: Object 0x638110 = `jmp [vt+0x64]` | the same as `Get2DRadius` (Creature 0x4792C0 repeats its read) |
| `GetHeight(e)` | vt+0x42C | MagicFireBall 0x682D30 = `jmp [vt+0x64]`; Creature 0x477F50 = size × **15** [0x8C2C40] (the size, the scale of the `Transform`: **(inferred)**); the rest 0x638120 (Field, FishFarm and PileFood do **not** change it) |
| `GetTopPos(e)` | vt+0x630: Object 0x638160; MapShield / MagicShield / PhysicalShield 0x72C1C0 = **0** | `altitud (+0x1C, sobre el suelo) + GetHeight` |
| `GetHeightForHandAboveInteractObject(e)` | vt+0x64C: Object 0x638150 = `jmp [vt+0x42C]`; FishFarm 0x52C840 = **5** [0x8AB6E4] | |
| `GetMeshRadius(e)` | vt+0x568: Object 0x636BD0 = +0x30 unscaled; Field 0x528A30 / FishFarm 0x52C480 = 5 | |
| `PileFoodProportionRaised`, `PileWoodProportionRaised`, `GetProportionRaised(e)` | vt+0x86C: PileFood 0x66EB60, PileWood 0x66F1B0 | see below |
| **Derived** (own routines on top of the API) | | |
| `GetHoldRadius(e, above)` | Object 0x638C00: ABOVE (`GetHoldType` = 1) → `GetHeight × 0.75` [0x8AB274], otherwise `Get2DRadius`; Tree 0x74B610 / DeadTree 0x5110E0 = `Get2DRadius × 0.2` [0x8AB244] | the hold type is known by the hand; SpellSeed 0x728640 (`GetScale × info+0x150`) is set by the caller |
| `GetDefaultFireRadius(e)` | Object 0x639AC0 = `jmp [vt+0x64]`; DeadTree 0x510E10 = `GetHeight × 0.35` [0x8D6974]; WorshipSite 0x77DE10 → 0x77DDD0 = **14** [0x99C9EC] | |
| `GetVillagerHugRadius(e)` | Object 0x4026B0 = `Get2DRadius × 1.05 + 0.0005` [0x8AA3A0] [0x8AA39C]; Tree 0x74A1A0 = `min(Get2DRadius × 0.1, 0.25)` [0x8AB22C] [0x8AB3D4] | |
| `GetRoutePlanRadius(e)` | vt+0x7C4: Object 0x6384C0 without a creature = `Get2DRadius` (0x6384CF); Tree 0x74A140 (copy of 0x74A1A0); CitadelHeart 0x4680C0 = `Get2DRadius × 0.33` [0x8CA268] (in openblack, `Temple`) | the branch with a creature, not ported |
| `GetDistanceFromObject(a, b)` | vt+0x6C4: Object 0x637FB0; WorshipSite 0x77DE20 | `GetDistanceInMetres − (R2D(b) + R2D(a))`; the worship site measures from `CalculateCentrePos` 0x77DD40 and subtracts `14 + R2D(b)` (`GetRealRadius` 0x77DDD0, 0x77DE36..0x77DE60) |
| `GetDistanceFromObject(a, punto)` | vt+0x13C: 0x5702B0 (Object 0x4027C0 calls it) | `GetDistanceInMetres − GetRadius`. No `Object` class overrides it |
| `IsTouching(a, b, m)`, `IsTouching(a, punto)` | 0x637E00 (`≤ m`), 0x637E30 (`≤ 0`) | through the two above, with their overrides |
| `GetBoundingSphere(e)` | vt+0x798: Object 0x637730; Living 0x5ED2F0; MobileStatic 0x608F40 | `h = GetHeight × 0.5`; `r = √(R2D² + h²)`; centre = that of the MapCoords with `y = (GetAltitude + altitud) + h`. The ground is the island's (`LandIsland::HeightAt`, via `map_coords::ToWorld`). Living (villagers, animals) and MobileStatic (rocks, dead and felled trees, bonfires, fragments, teleport stones) use `R2D × 0.5` (0x5ED30D / 0x608F5D). Creature 0x479970 → `LH3DCreature::GetBoundingSphere` 0x47F8D0, not ported: it uses the `Object` one **(inferred)** |
| `WorshipSiteCentre(e)` | `WorshipSite::CalculateCentrePos` 0x77DD40 | `right × 12.55 [0x99C9E8] − forward × 26.1 [0x99C9E4] + position`, per component (0x77DD61..0x77DDB1). The matrix is that of the site's `Transform` **(inferred**: `[this+0x40]+0x14`, as `WorshipScore` already read it, which now calls it) |

**GetProportionRaised** (0x66EB60, food): `p = amount / maxAmountInPot` (unsigned 64-bit `fild`, `fidiv`);
p < 0 → 0 without a floor; p > 1 → 1; **p = 0 stays at 0** (0x66EBB7..0x66EBC2); otherwise, `p = (1 − 0.05)·p + 0.05`
[0x933014]. It returns `1 − (1 − p)²` clamped to 0..1. The wood one (0x66F1B0) applies the floor if p > 0 and clamps, without
the square. With `maxAmountInPot = 0` the original gives inf (→ 1) or NaN (→ 0), the same as dividing by 1.

**What was fixed** (8 fidelity fixes):
1. **Field = 5 m** in fire (`fire::traits`, and with it FireEffect, FireGraphic, Explosion and VillagerFire), the
   animals (`GetWorkingPos` 0x639550 uses vt+0x60), the town (`CheckForClearArea` 0x741457), healing (Heal), the
   forests (`AddTreeAround` 0x439220, vt+0x60) and SpellFlock / SpellWater (which already had it separately).
2. **FishFarm = 5 m**: it was not anywhere.
3. **PileFood × proportion** in all the object queries (before only in `pot_resource`).
4. **MagicFireBall** (radius and height = scale) outside fire: Heal and everything that goes through the API.
5. **The empty pile gives 0**, not 0.0975 (`PotResource.cpp`, and its test `test_food_wood`).
6. **The field sank twice as much**: `Fields.cpp` stored the whole height; the original adds `2·v·scale·[m+0x28]` with the
   **half**-height (0x5287C2..0x5287D1). Now it is the half-height of the mesh level.
7. **Without a mesh, 0**: the hand (0.5 / 1.0), the pile when sinking (1.0) and the field (1.0) gave invented sizes.
8. **The MapShield with a single scale**: a shield's `GetScale` is its `objectScale` in all the queries.

Also: `Trees.h` cited `ComputeBoundingBox` at 0x808180 (it is **0x8081B0**). `Tree::Draw` (the crown when bending,
0x74ABA2..0x74ABC2) is ported at mesh level; the height of the tallest tree (fn_0053A740, 0x53A75D), the felled tree
(0x5116C2) and the rustle of more than 10 m (0x74B1CF) at object level. The `GetDefaultFireRadius` of a worship site is 14 m.

Migrated to the object level: `EffectValues` (see below), `FireObjectTraits`, `PotResource`, `PotArchetype::SetSize`
(0x66E90A / 0x66E918), `SpellWater`, `SpellFlock` (`fn_006D0C20` with vt+0x60), `Heal` (including the rule scale,
vt+0x64 at 0x6A0DC3), `OneOffSpellSeed` (the Z-sorter advance, vt+0x60), `SpellDispenser` (0x722B46),
`TestDispensers`, `TownQueries`, `Trees`, `Rocks` (and with it `LanternSounds` and the rock physics), `AnimalFlee`,
`AbodeArchetype` (0x40327E / 0x40329A) and `HandHolding::ComputeHoldParameters` (the height via vt+0x42C, that of the seed
with the mesh of its info via `Object::GetHeight` 0x638120; the radius via vt+0x64 like `Object::GetHoldRadius`
0x638C22..0x638C26, so a picked-up food pile, also the hand's HandFood, carries its `GetProportionRaised`).
`HandSystem::Update` no longer repeats the proportion with a fixed 1600: it only asks for the parameters again every frame
(info.dat: HandFood is potType 1 = PileFood with `maxAmountInPot` 1600, so only the empty hand changes, which now measures
0). At mesh level: `Fields` and the crown of `Trees`.

The wrappers `effects::ObjectHeight` / `Object2DRadius` (the `Object` routine **without** overrides) no longer
exist: all their callers go through the API (2026-10-02).

## Game clock

✅ Faithful and ported in `src/GameClock.{h,cpp}`, namespace `openblack::game_clock` (2026-10-02).
Game drives it: it starts it in `LoadMap`, decides the turns in `Update` and computes the frame clock right
afterwards. The original **does not have a «give me the time» function**: `GGame::Loop` 0x54CF20 computes the clock once per
iteration and leaves it in GGame fields that hundreds of readers read inline (379 references to the turn, 157 to
`g_game_time_inc`, 29 to the fraction). openblack had it written about 37 times, with different fidelities.

**The game timer** is an `LHTimer` at g_game +0x205D68 (+0x100 base, +0x104 accumulated ms, +0x108
speed factor, 0 = stopped, +0x10C saved factor). `MSeconds` 0x43EB70 = `ftol((GetTickCount − base) · factor +
acumulado)`; `Stop` 0x43E9C0 accumulates and sets the factor to 0; `SetSpeedUpFactor` 0x43EBC0 rebases if it is running and, if
it is stopped, only saves the factor. To start it (start of Loop 0x54CF93, `ResetLocalGameTimer` 0x54C690, unpausing)
the factor is set to 1e-5 (0x3727C5AC) and then `SetSpeedUpFactor(guardado)`: that way it rebases and **the stopped
time does not count**. All in float (FPU at 24 bits).

**When there is a turn** (`LocalTimerSaysDoATurn` 0x54C4A0, called from `ProcessNetworkPackets` 0x54CD45):
- it is due when `MSeconds ≥ turno · 100`, with the 100 hard-coded (0x54C4F0). The comparison is **absolute**, so
  the leftover of one turn carries over to the next;
- when paused (single player) never (0x54C528);
- with more than 2000 ms (0x7D0) of lag it calls `ResetLocalGameTimer` 0x54C570, which sets the timer to
  `turno · 100` from now (0x54C615). The answer of that call is that of the earlier sample (`setge` 0x54C567);
- at most **1 turn per frame** in single player (10 in network play): `neg; sbb; and 9; inc` at 0x54CD0F..0x54CD18. The
  loop asks the timer first and then the cap (0x54CD52), so after the last turn of the frame the
  timer is queried once more.

**The turn** g_game +0x205A40 goes up **when the turn starts** and only when not paused (`GGame::StartTurn` 0x54E4FD..0x54E507),
before `ProcessTurn` 0x54E5C0 and `EndTurn` 0x54E960. During the turn the whole game already reads the new number.

**The frame clock** (`GGame::Loop` 0x54D2A8..0x54D3A6, after the turns and before drawing):
- not paused: `Δ = MSeconds − muestra anterior`; with the same turn `resto += Δ`; with a new turn
  `resto += Δ − 100` (0x54D316); then `resto` is clamped to 0..99 (0x54D325..0x54D337);
- `visual = turno · 100 + resto` (0x54D343); if it is lower than the previous one, that lower value is stored (0x54D350: the clock
  **does go backwards**) and `resto = 0` (0x54D356); what is never negative is the dt, which is 0 that frame;
- `g_game_time_inc` [0xEA9EC0] = g+0x250540 = g+0x205D48 = `visual − anterior` (0x54D366/0x54D374/0x54D380): **whole
  ms**, at most 199, which follow the speed;
- the **fraction** g+0x205D64 = `resto · 0.01` [0x8C4B10] (0x54D392): it goes from 0 to 0.99 and lags one turn behind;
- when paused only the dt is 0 (0x54D39A); **the fraction is preserved**;
- `NetworkTurnsThisFrame` goes back to 0 after drawing (0x54D3C3).

**The wall clock** `g_delta_time` [0xC38134] is another `LHTimer` (`LH3DTech::g_timer` 0xEA1B78), read in
`LH3DRender::StartFrame` 0x82F14E: ms of the frame, 1 if it comes out ≤ 0 (0x82F195), and it does not stop when paused. Its static
constructor (fn_008189F0, in the `__xc_a` table at 0x9C7D60) leaves it **stopped** (speed 0, saved 1);
`LH3DTech::RenderInitialization` 0x818C61..0x818CA3 (called by `LH3DRender::Open` at 0x82B540) starts it: factor
1e-5, `elapsed = MSeconds` (≈ 0), base = `GetTickCount`, factor = the saved one (1). It therefore counts the ms **since the
engine starts**, not since the machine starts. `SetSpeed` 0x5537F0 additionally sets [0xD00DA8] = 0 in both branches
(0x5538AD, 0x5538C8); that address is only written in the whole exe (also in `GNetwork::ProcessOnePacket` 0x634B40),
nobody reads it: it is not ported **(inferred: no indexed reader has been seen)**.

| API (`game_clock`) | Original | What it does |
|---|---|---|
| `k_MsPerTurn` = 100, `MsPerTurn()`, `SetMsPerTurn()` | [0xD01A38]: `GGame::Init` 0x54F4A5, `SET_GAME_TICK_TIME` 0x714DBE | the turn ms that the logic reads |
| `k_SchedulerMsPerTurn` = 100 | literals 0x54C4F0, 0x54D316, 0x54D343, 0x54C615, 0x5550A3, 0x553810 | the 100 of the scheduler and of the frame clock (does not read [0xD01A38]) |
| `k_TurnSeconds` = 0,1f | `push 0x3DCCCCCD` in `ProcessTurn` 0x54E5D1, 0x54E6C3, 0x54E775 | the turn seconds passed by hand |
| `k_MaxLagMs`, `k_MaxTurnsPerFrame` | 0x54C553, 0x54CD0F | 2000 ms; 1 turn per frame |
| `Timer` (`ElapsedMs`, `Stop`, `SetSpeed`, `Start`; were `MSeconds` and `SetSpeedUpFactor`) | LHTimer 0x43EB70 / 0x43E9C0 / 0x43EBC0; start 0x54CF93 | the timer |
| `Turn()`, `SetTurn()` | g+0x205A40 | the turn |
| `IsTurnScheduled()` (was `TimerSaysDoATurn`), `TurnDue()`, `StartTurn()`, `ResetLocalTimer()` | 0x54C4A0, 0x54CD45, 0x54CD93 + 0x54E507, 0x54C570 | the scheduler |
| `Start(paused)` | Loop 0x54CF6B..0x54D003 and 0x54D1F7 | timer from 0, dt and fraction to 0, `ResetLocalGameTimer` |
| `OnLoad()` | `ResolveLoad` 0x555080 | dt and fraction to 0, `visual = turno · 100` (the Loop statics are not touched) |
| `Pause(bool)`, `IsPaused()` | `PauseGame` 0x54AE20 (0x54AE7C..0x54AEE7) | the flag g+0x14 bit 2 and the timer stopped / restarted |
| `PausedTurnTimer::Due(now)` | `ProcessNetworkPackets` 0x54CC88..0x54CCCA (the static [0xCD3AE8]) | the temple's turn every 100 ms of the ticks while paused inside the citadel |
| `SetSpeed(v)`, `Speed()` | `GGame::SetSpeed` 0x5537F0 (0x553800; `SetSpeedUpFactor` inline at 0x553835) | the speed factor; the time already elapsed keeps the previous speed |
| `UpdateFrameClock()` | `GGame::Loop` 0x54D2A8..0x54D3A6, 0x54D3C3 | remainder, visual clock, dt and fraction |
| `FrameGameMs()`, `FrameGameSeconds()` | [0xEA9EC0]; `· 0.001` [0x8AA3B0] | whole game ms of the frame |
| `TurnFraction()` | g+0x205D64 | the turn fraction |
| `VisualMs()` | g+0x25053C | the visual clock |
| `StartEngineTimer()` | `RenderInitialization` 0x818C61..0x818CA3 (`Reset()` leaves it stopped like fn_008189F0) | starts the wall clock from ≈ 0; it is called by `InitializeEngine` (Locator.cpp) when creating the renderer |
| `UpdateRealClock()`, `FrameRealMs()`, `EngineMs()` | `StartFrame` 0x82F14E..0x82F195; `g_timer` 0xEA1C78..0xEA1C80 | the wall clock |
| `CameraFrameMs(playingBack)` | `GetCameraTimeInc` 0x555820 | game dt when playing back the recorded interface, otherwise the wall one |
| `ClampedFrameMs(inTemple)` | fn_005557E0 | wall in the temple, game outside; ≤ 0 → 0, cap 500 |
| `TicksForSeconds(s)` | `ftol(1000 / [0xD01A38] · s)` (integer division): `NumGameTicksPerSecond` 0x711630 and inline at 0x70CCDE, 0x711338, 0x5C61F6 and `GetTicksToChangeOver` 0x66CD00 | seconds → turns; with [0xD01A38] = 0 the original would fail at the `div` (0x711635, unchecked): openblack gives 0 **(inferred)** |

Careful with the name of 0x711630: it is actually the `SetTime` of a script timer (it stores the turn in +0x28 and the
turns in +0x2C, fn_00711610); the conversion is the same as that of the other four.

**Also in the original:**
- The GGame ctor starts the game timer at speed 1 [0x54B58A].
- g_game+0x205A28 (the sequence mode: 1 = inside the citadel, 2 = the falling spell, see [video.md](video.md)):
  `GGame::Init` sets it to 0 [0x54FCA4].
- Others that unpause with `PauseGame(0)`: `Continue` 0x53FF00 of the Escape menu (hides the menu and unpauses; the
  menu keeps the game paused while it is open) and the SkipBox callback, after applying the answer [0x54458B].

openblack speed: `Game::SetGameSpeed(m)` still receives the turn-duration multiplier (2 slow,
0.5 fast) and calls `SetSpeed(1 / m)`.

**What was fixed** (before, each thing had its own clock):
- **The turn leftover.** `GameLogicLoop` did a turn if 100 ms had passed since the previous one and set the mark
  at the frame of the turn: at 30 fps turns lasted 133 ms (25 % slower). Now 3 s at 33 ms per frame
  give 30 turns.
- **The turn goes up at the start.** `_turnCount` went up at the end; during the turn one less was read than in the original.
- **The pause.** It did not stop the clock: when unpausing it did a turn in the same frame and the fraction jumped to 0.99. The
  fraction was 0 while paused; now it freezes.
- **The frame order.** The turns go before the frame clock and before everything that moves per
  frame (villagers, animals, sharks, ship, rings, fish, fireflies, widescreen, magic), as in
  `GGame::Loop`. The ten copies of `_paused ? 0 : dt / mult` in Game.cpp read `FrameGameMs()` / `FrameGameSeconds()`:
  whole ms, 0 when paused, ≤ 199.
- **The sea.** `RendererSea.cpp` took as dt the time since startup (`desc.time`, which is never rebased) and
  `ScrollRows` accumulated it again: the sea scrolled faster and faster. Now it reads `g_game_time_inc`
  (0x879963, 0x87A130).
- **Own fractions.** HandGrain (0x5B2D41/0x5B2D61), the PSys (0x67370D) and the fireflies (0x52ADF6) used the
  wall clock or their own, without speed or pause and with a cap of 1. Now they read `TurnFraction()`.
- **Own clocks.** HandFish and HandResources (`ProcessInInteract` once per turn) counted turns with the hand's real
  dt; now they count the game's turns. The fragments of broken buildings (`Fragment::ProcessTimer`) go
  with the turn, and the physics dt is the game one. TownBelief used the wall clock where the original adds
  `g_game_time_inc · 0.002` (0x69D855); its PSys step, on the other hand, does **not** use the frame time (see
  «TownBelief» below). MissionaryBoat (was PetitNavire; `g_carry`) and the sharks (`s_Clock`) reconstructed the whole
  ms: they now arrive whole.
- **Conversions.** The dispensers (0x70CCDE, 0x711338), the end of the help texts (0x5C61F6) and the ramp for
  picking up from a pile (0x66CD00, before in float and without truncating) use `TicksForSeconds`. Chants and help read
  `MsPerTurn()`. `SET_GAME_TICK_TIME` (it used to throw an exception) writes [0xD01A38] and nothing else.
- **The copies of the turn** (`reactions::Turn`, `magic::CurrentTurn`, the fireball's one, which nobody read, the
  weather and its loop, the trees, the animals, the villagers and the chimney smoke, which did not check whether there was a Game) read
  `Turn()`. `magic::k_TurnMs`, `Chants.h`, `DayNightClock.cpp`, `FireFlies.cpp`, `ChimneySmoke.cpp` and `HelpSystem`
  take their constants from the clock.
- **The help texts clock** (`queries.nowMs`) was an approximate clock of its own; now it is `EngineMs()`, the
  `g_timer` that the original reads at 0x5C6250.

**Audit fixes** (2026-10-02):
- **The wall clock was not started.** `engineTimer` stayed with base 0 and speed 1: `EngineMs()` and
  `UpdateRealClock()` gave the ms of `steady_clock` since its epoch (machine startup). In float, with more than
  4.6 h of the machine switched on (2^24 ms) the ms went in steps of 2 (after days, in steps of 32: the texts clock and
  `FrameRealMs` were quantised), and with more than 24.8 days the conversion to int32 overflowed. Now `Reset()` leaves it stopped
  like fn_008189F0 and `StartEngineTimer()` (RenderInitialization 0x818C71) starts it from ≈ 0.
- **The sea frame counter** [0xFA938C] (`(frame + 1) & 15`) advanced without pause; the original only advances it
  if `g_game_time_inc != 0` (0x879B0A / 0x879B41).
- **Addresses and texts.** `++NetworkTurnsThisFrame` is at 0x54CD93 (at 0x54CE58 is the call to
  `ProcessOneGameTurn`). The visual clock **can** go backwards (0x54D350); what is not negative is the dt.
- **`atmos::UpdateGame`** (WeatherLoop.cpp) uses `k_TurnSeconds` (0x54E5D1) instead of the hand-written 0,1f.
- **[0xD01A38] at runtime.** The fireflies (`FireFly::Process` fn_0052AF90, 0x52AF93: `fild [0xD01A38];
  fmul 0,001`) and the hand wind of the chimney smoke (fn_005DBC60 0x5DBD4E: `1000 / [0xD01A38]`) read the
  variable every turn: now `MsPerTurn()` (before, the constant `k_MsPerTurn`, and `SET_GAME_TICK_TIME` did not reach them).
  `Chants.h` only has the default value; `Spell.cpp` already fills it with `MsPerTurn()`.
- **HelpSystem in float.** The reading seconds (0x5C6211..0x5C6225) and `ShownLongEnough` (fn_005C68C0) were
  computed in double; now in float (the FPU at 24 bits).
- **The fragments** (`Fragment::ProcessTimer` 0x76EAF0) are called by `GGame::ProcessTurn` at 0x54E768: now they run from
  `Game::GameLogicLoop` and not from the physics with a `static` of the turn (which was not reset when loading a map).

**Visible changes that have to be checked with a screenshot:** at 30 fps the game runs 25 % faster (turns last
100 ms); the first turn is played in the first frame; when unpausing there is no jump; with the pause on the
villagers and animals stay where they were (before they went back to the position at the start of the turn); the sea scrolls at
a constant speed; the piles collected from fish farms, fields and heaps follow the game speed and
stop when paused; the grain in the hand, the fireflies and the particle effects interpolate with the game fraction
(they stop when paused and follow the speed); the belief symbols of the towns stop when paused.

## Object lists per cell (`ecs::map_cells`)

`src/ECS/MapCells.{h,cpp}` (2026-10-02). It is the
original's GMap grid (g_game+0x59B8, 8-byte MapCell at +0x59FC, 512 × 512 by `GMap::Init(0x200, 0x200)`
0x6014C0): each cell has **two linked and sorted lists**, +0 the mobile one (`SetFirstObjectMobile` 0x601B60) and +4
the fixed one (0x601B70).

**Which list.** It is decided by the **type** of the info (+0x10), not the class: `DoesObjectTypeCountAsFixed` 0x601510, table
0x60152C (fixed 0, 6-9, 11, 12, 14, 18, 19, 21-26, 28, 29, 31-41, 43, 44; above 0x2C, also −1 and −2
unsigned, not). `InitialiseIsFixedForMapList` 0x63A640 stores it in bit 15 of Object +0x24. The type is read from info.dat
(`map_cells::TypeOf`); what openblack does not store with a row is marked (inferred) in the code.

**Which end.**

| Class (`InsertKind`) | Insertion | End |
|---|---|---|
| SingleMapFixed (Tree, MagicTree, MapShield) | 0x52E620 → `Fixed::InsertMapObjectToCell` 0x52DEA0 | head of the fixed list, its cell |
| MultiMapFixed (Abode, Field, Feature, AnimatedStatic, MobileStatic, DeadTree, BigForest, TotemStatue, WorshipSite, Temple, SpellIcon, MagicTeleport, Fragment) | 0x52E650 → `AssumeFixed` 0x52DEE0 in each cell; children sorted by x and z (`SortChildren` 0x52DC10) | head of the fixed list, all its cells |
| FishFarm | 0x52CA10; `GetNextPos` 0x52C940 gives **a single** position, its own | head of the fixed list, its cell |
| Object (Villager, Animal, Creature, StreetLantern, Pot and piles, OneOffSpellSeed, MobileObject, Shark) | 0x636740 → `Object::InsertMapObjectToCell` 0x636830 | with bit 15, **tail** of the fixed list (pots and piles, street lamps); without it, head of the mobile list (doubly linked, +0x38; the orbs: their info `GMobileObjectInfo` 25 is of type 20) |
| SpellSeed, MagicFireBall, Town, Forest... | `ret` (0x728F30, 0x682D10) | off the map |

Deleting (`RemoveMapObjectFromCell` 0x6368D0) does not change the order of the others. Moving (`MoveMapObject` vt+0x55C): a
single-cell object is only reinserted if it changes cell (0x636A40); a MultiMapFixed, if its MapCoords changes
(0x52E4F0, `operator==` 0x605660). `ActualMoveMapObject` 0x638040 leaves it **at the head**. `SetXYZAnglesAndScale`
(0x638F80 / 0x6074E0 / 0x608D60) also removes it and puts it back in.

**Cells of a MultiMapFixed** (`NewCollideDescriptor` 0x46A860 / `Init` 0x46AB10 / `GetNext` 0x46AD80;
`DescriptorCells`):

1. The shape is `map_collide::FromMesh` (NewCollide 0x829390) and `reach = scale · mesh+0x30 + 1` ([0x8AA390]).
2. Box `ftol((c ∓ reach) · 0.1)` ([0x8AC404]). If the low corner is negative it becomes 0, and only then the high one
   too (0x46ABC9..0x46ABE7).
3. x outside, z inside. A cell is marked if its **7.1 m** circle (0x40E33333) at `(10i + 5, 10j + 5)` touches the
   shape, only if it is on the map. If none is marked, the middle one, `(w/2)·d + d/2` (0x46AD06..0x46AD3B).
4. The insertion **stops** at the first marked cell outside the map (0x52E70D).
5. (approximate) Without a mesh in openblack (fields, teleport stones): the cell of its position.

**Reads of a cell.**

- The original's traversal (`GetFirstIterator` 0x6034D0 + fn_006827E0, and the inline copies) is **the fixed list from its
  head and then the mobile list from its own** (`ForEachInCell` / `ObjectsInCell`). `MobileInCell` is only the mobile list
  (0x603490).
- `FindType(celda, t, anterior)` (0x6045C0 → `FindTypeOnMap` 0x6015E0): with −1, the fixed list and then the mobile one (when the
  fixed list ends it jumps to the mobile one if the previous one's type counts as fixed, 0x601621); with another type, **only its list**
  (0x601646).
- `FindFixedOnMap` 0x601690. `IsFixed` (0x603790 → 0x601EA0) looks **only at the head** of the fixed list: whether it is a
  MultiMapFixed (+0x24 bit 1). `IsOwnCell` is fn_00604F40.

**Collision with what is in the cell** (2026-10-02):

- `ForEachFixed(celda, fn)`: only the fixed list from the head (`GetFirstObjectFixed` 0x6034B0 + `GetMapChild`), with
  the same filter as the other reads. It is the chain of 0x60CAA0 (ObjectCircleIterator), 0x74B9C0
  (`Tree::EndPhysics`) and 0x601D56.
- `Collide(MapCoords)` = `MapCoords::Collide` 0x6033C0 → `MapCell::Collide(MapCoords)` 0x601CE0 → `MapCell::Collide`
  0x601BD0:
  - outside ToMap, `0xFFFFFFFF` (0x6033CC);
  - otherwise, the terrain part (`sea_cells::CollideLandscape`): 0x10 outside the game map (fn_00601E00, and then
    no object is checked any more), otherwise 1 water (or no block) / 2 land (0x601C7D);
  - then the **fixed list** from the head (0x601C78..0x601CAE; the mobile one is not read): type 6 `|= 0x20` (0x601C9E),
    type 0x12 `|= 4` (0x601C99);
  - **bit 8 never comes out**: 0x601CE0 calls `CollideWithFixe` only if the result of 0x601BD0 has bit 8
    (`test bl, 8`, 0x601CEB), and 0x601BD0 never sets it. That branch is dead.
- `CollideWithFixed(MapCoords)` = `MapCoords::CollideCollideWithFixe` 0x604FE0 → `MapCell::CollideWithFixe` 0x601D10:
  - outside ToMap, `0xFFFFFFFF` (0x604FEC);
  - otherwise, the bits of 0x601BD0 (0x601D18) and `| 8` (0x601DAD) if a **0.5 m** circle (`push 0x3F000000`,
    `NewCollide::Obj` 0x82AD90) at `(x, z) = MapCoords · 10/65536` ([0x8AA3A4], 0x601D23..0x601D3C) touches the
    `GetCollideData` (vt+0x858, 0x601D61) of some object in the fixed list (`Obj::Collide` 0x829140).
- The collision data (`CollideDataOf`) are stored in the link **on insertion**, because the original creates them there:
  `SingleMapFixed::InsertMapObject` calls vt+0x864 at 0x52E633 and `MultiMapFixed::InsertMapObject` vt+0x908 at
  0x52E669. Read in the vtables:

  | Class | GetCollideData / CreateCollideData | In openblack |
  |---|---|---|
  | Object, MobileObject, Pot, GStreetLantern, Villager | `Object::GetCollideData` 0x419B30 = `xor eax, eax` | none: the pots, piles and street lamps at the tail of the fixed list do not collide |
  | Tree, MagicTree | `Tree::CreateCollideData` 0x74C5F0: circle of 0.3 (`push 0x3E99999A`) at the position | the same |
  | MapShield and other SingleMapFixed | 0x52F510: `NewCollide(LH3DObject)` 0x829390 | `map_collide::FromMesh` of its mesh |
  | MultiMapFixed (houses, fields, features, rocks, stumps, pieces, totems, icons, stones) | 0x52F550: `NewCollide(LH3DObject)` | `FromMesh`, the same shape that gives its cells |
  | BigForest | 0x439580 = `jmp ReleaseCollideData` | none |
  | FishFarm | 0x52CA10 does not call vt+0x908; the MultiMapFixed ctor sets +0x78 = 0 (0x52E26F) | none |
  | WorshipSite, CitadelHeart | 0x77E490 / 0x468FB0, own shapes | (approximate) that of the mesh |

  (approximate) A fixed object without a mesh in openblack has no shape.

**Searches.**

- `FindNearType` 0x6045F0: a single list (−1 is the mobile one); it does not clip to r.
- `FindNearForScript` 0x604370: signed ±r square, the z count is `(height & 0xFFFF) − bajo + 1`, the totem of a
  worship site (0x77CF30), strict `<` from FLT_MAX.
- `FindNearestInSpiral` fn_00604AF0 / fn_00604C30: `max(3, ceil(2r/10))²` cells, `d < r`, cutoff `1.5·mejor + 10`
  ([0x8AB24C] / [0x930050]).
- `FindNearInfluenced` 0x604870: `GetDistanceModifier(d, r)` 0x74F290 (r is the last argument, read at 0x604A33).
  Nobody uses it yet.
- `TallestOverlapping` fn_006022C0.

**Towns** (they do not use cells):

- `ForEachTown` / `TownsOf` = `GetNextPlayerAndNeutral` 0x550980 (slots 0..7, the neutral one last) × the list of each
  player, which is filled **from the tail** (fn_0064C090): the oldest first ((inferred) by `Town::id`).
- `GetNearestTown` 0x6020E0 and `GetNearestCitadel` 0x602200: strict `<` from r. `GetNearestTownWithCentre`
  fn_00602160: the same, only the towns with +0x9A4 (0x6021A7) **or**, otherwise, fn_00741020 (0x6021B3) =
  `TownHasCentre` (read in full, 0x741020..0x741070): 1 if among its houses (+0x754, next +0x9C) there is one with
  `IsTownCentre` (vt+0x1E0; only TownCentre 0x55DB70 gives 1) or among its planned ones (+0x9A8, next +0x44) one whose
  info (+0x40) gives `GetAbodeNumber` (vt+0x44, GAbodeInfo 0x401260 = info +0x124) == 0xC (ABODE_NUMBER_TOWN_CENTRE; the
  «GetComputerSeen» of the slot's name was false). In info.dat the houses of type 0x404 are exactly those of number 12
  (test `TownCentreInfosAreNumber12`), so openblack checks the number of the town's `Abode` (`townId`).
- `GetNearestTownCells` 0x601F90: octagonal distance in cells; (approximate) without the town's rectangle.
- `GetNearestTownToPos` 0x73B170: `0x7FFF` is any house; with another type **it accepts the towns that do not have it**.
- `FindNearestTownInList` fn_00552FF0: the global list. **It has no ID branch** (read): always the first one and then
  `<`.
- `FindPlayerTownAtPos` = `GScript::FindPlayerTownAtPos` 0x6F72E0 (used by GET_NEAREST_TOWN_OF_PLAYER, 0x6F2ADD): only
  the list of **that** player (GPlayer+0xA50, next +0x75C), `GetDistanceInMetres` 0x74CD70, best = r and
  **`≤`** (`fcom; test ah, 0x41`, 0x6F7312): in a tie the town that comes later wins. It is not `GetNearestTown`
  0x6020E0.
- Users in `src/Worship`: `Citadel::RequestANewWorshipSite` 0x4633F0 calls `GetNearestTownToPos(coords de la
  ciudadela, tribu, 0x7FFF, FLT_MAX)` at 0x46345C. `AssignTownsToWorshipSite` 0x77AF70, `CreateBuiltWorshipSite`
  0x465110, fn_00464F50 and `GPlayer::PostLoadCleanup` 0x64AB90 walk `TownsOf(jugador)`.
- `site::FindAt` = `MapCoords::FindWorshipSite` 0x602460: **a single** `FindTypeOnMap(8, 0)` (0x602479). If it is a
  WorshipSite (0x602493), that one; if it is a WorshipSpellIcon (0x6024AC), its site (vt+0x30C); if it is something else (the citadel
  heart, a town-centre icon), null.

**Clearing.** `GlobalGameLists::ClearMap` 0x591A92 → `fn_006013D0` (the GMap at g_game +0x59B8) runs `MapCell::Clean`
0x601380 on every cell (0x6013FF). `CleanupWhenDeleted` 0x6377F0 takes an object out of its cells through vt +0x548:
`Object` 0x6367A0, `MultiMapFixed` 0x52E7B0, `SingleMapFixed` 0x52E600.

**Maintenance in openblack.**

- Hooks `InsertMapObject` / `RemoveMapObject` / `MoveMapObject` / `OnAnglesOrScaleChanged` where the original calls
  the vtable. The magic code calls them for its own objects: SpellForest trees, piles, storm pieces, orbs,
  MapShield, the stones and the teleported living being, and whatever the tornado carries (`SetHeldOutOfMap`).
- The other owners' objects come in through `Sync()`, which `MapProduction::Rebuild` calls at the start of every turn, on load and in
  `Reactions`: first the removals (destroyed, in the hand, in physics, class change) and then the additions and the
  moves by creation index (inferred).
- Every read also skips what the original would already have taken out: invalid, in the hand or flying.
- **When physics ends** (Object::EndPhysics 0x6375A0, 0x637613..0x63763A; `physics::EndPhysics` in
  PhysicsObjects.cpp): if the object is still available, `MapCoords::InBounds` 0x6042C0 of its position. Inside the
  512 × 512 cells it goes back into the lists (`InsertMapObject`, vt+0x544); outside **it is deleted** (`ToBeDeleted(0)`, vt+0xC).
  Everything that lands goes through there: villagers, animals, pots, scaffolds, grain and the fixed ones (rocks, trees,
  fragments). Object::EndPhysics always returns the object itself (never NULL). The villager (0x5F0B81) and the animal
  (0x5F0E01) call it in the middle of their EndPhysics, after SetYAngle and before their landing, water and death work
  (`PhysicsObjects::BackInMap` from ECS/LivingPhysics); the other classes after their part. A tree that becomes a dead
  tree (Tree::EndPhysics 0x74BBD9..0x74BC3A) never calls it: the DeadTree goes in the cells with no InBounds test
  (0x74BC0A) and is returned, and GameTurnUpdate makes it the resting proxy (0x645EE0).
- **The C22 angle** is the +0x48 of the LH3DObject (0x8294A3), the Y of `LHMatrix::GetYXZ` 0x7FAB30 (row 2), not
  row 0. What follows (approximate): the centre of the box rotates only in xz by that Y, whereas the original goes through
  the full matrix (0x8293CE..0x829413).
- `OPENBLACK_MAPCELLS_CHECK=1` checks the lists on every `Sync` and writes `map_cells: N objects, M cells, E errors`.
- The old API (`MapInterface` / `MapProduction`) keeps only its fixed grid, for the pathfinding wall hug. Its mobile grid
  and `effects::ObjectsInMapCell` / `FixedObjectsInMapCell` were removed on 2026-10-07: nothing read them any more.

## Terrain height

`LH3DIsland::GetAltitude` (0x803090), ported exactly in `LandIsland::GetHeightAt`:

1. Cell `(x>>16, z>>16)`, 16-bit fractions `fx, fz` (`>>8` is used, 0..255).
2. Blocks of 17×17 cells (shared row): neighbours `+1` = z+1, `+17` = x+1. Cell height in `cell.altitude`.
3. If the base vertex ≤ 4, heights ≤ 3 count as 0 (sea edge; global 0xC37BF4 active).
4. The cell's `split` bit (`properties +6 & 0x80`) chooses the diagonal. The 4th corner is extrapolated from the other three
   so that the bilinear blend is flat over the triangle:
   - split and `fz > 0xFFFF - fx`: `c00 = v10 + v01 - v11`; split and not: `c11 = v10 + v01 - v00`
   - no split and `fx > fz`: `c01 = v00 + v11 - v10`; no split and not: `c10 = v00 + v11 - v01`
5. `atX1 = (c11 - c10)*fz + (c10<<8)`, `atX0 = (c01 - c00)*fz + (c00<<8)`,
   `h = (((atX1 - atX0)*fx) >> 8) + atX0`, result `h * 0.67 / 256`.

Verified: Land1 at (1788.4, 2710) = **28.9173050**, the recorded value from the original. openblack's terrain rendering
uses the same triangulation (it matches Bullet's physical terrain to the millimetre).

- The high words of the coords are read signed, and the height is 0 when one is negative or past 0x200
  (0x803095..0x8030B5). fn_00800DA0 changes a cell's altitude in every block that stores it, the shared border row and
  column too.
- `LH3DIsland::RayCast` fn_00802550 → `RayCastInternal` fn_00802680, cell test fn_0083AE80: described in
  [miracles.md](miracles.md) (the lightning's land cut). Also: the Cohen-Sutherland codes are 1 x < 0.1, 2 x > 511.9
  [0x9A2BD8], 4 z < 0.1, 8 z > 511.9 (0x80276B..0x80281C); the cells are walked by column then row
  (0x802B4D..0x802C7B); without a hit, a downward ray (|dy| ≥ 0.0001) gets its crossing of y = 0, true within 7500 m
  of g_camera (0x8025D9..0x802672). Callers: fn_00691F30 0x69221C (lightning), GCamera::Update 0x442406, fn_0044EF60
  0x44F046, CameraModeNew3::Update 0x45DA4A, GLandscape::Draw 0x5E4848, fn_005E5620 0x5E5660, fn_00800C30 0x800D79,
  fn_0086BD00 0x86BF1C.

## Terrain normal

**Faithful.** `LH3DIsland::GetNormal(const LH3DMapCoords&, LHPoint*)` 0x803630 (fastcall: ecx = coords, edx = out), ported
in `land_normal::OfCell` (`src/3D/LandNormal.{h,cpp}`) and called from `LandIsland::GetNormalAt`, next to
`HeightAt`. All of the original's callers build the MapCoords with `ftol(x·65536 [0x8AC408]·0.1 [0x8AC404])`
(fn_004427B0, 0x8126C0, 0x459105, 0x7FCBA2): `65536·0.1f` is exactly `6553.6f`, so it is `map_coords::ToFixed`.

1. Cell `(int16)(x >> 16)`, `(int16)(z >> 16)`; outside [0, 0x200) → (0, 1, 0) (0x80363B..0x80366B). openblack
   compares with `GetCellsPerSide()` (512 in the game's maps).
2. `GetCell` 0x516AA0: NULL if there is no block (`g_index_block` = 0, 0x516ADC) → (0, 1, 0).
3. **Raw** heights (without flattening next to the sea): h00 = [+4], h01 = [+0xC] (z+1), h10 = [+0x8C] (x+1),
   h11 = [+0x94], within the 17×17 block (the edge row belongs to the block: cell 511 is read).
4. Triangle (`split` bit = [+6] & 0x80, 0x803698..0x803752):
   - with split: B = h11 at (10, 10) if `fz > 0xFFFF − fx`, otherwise h00 at (0, 0); P = h10 at (10, 0); Q = h01 at (0, 10);
   - without split: B = h10 at (10, 0) if `fx > fz`, otherwise h01 at (0, 10); P = h11 at (10, 10); Q = h00 at (0, 0).
5. `dP = hP − hB`, `dQ = hQ − hB` in integer; `s = T1[|dP|]·T1[|dQ|]`; `P' = (Px − Bx, dP·0.67 [0xC3720C], Pz − Bz)`,
   `Q'` likewise (Q'x = `−Bx`, an `fchs`).
6. `n = ((P'z·Q'y − P'y·Q'z)·s, (Q'z·P'x − P'z·Q'x)·s, (P'y·Q'x − Q'y·P'x)·s)` (0x8037BA..0x8037F5).
7. `k = fistp(((nz·nz + nx·nx) + ny·ny)·1023 [0x9A2BE8])` (to nearest), `n *= T2[k]`; if `n.y < 0`, `n = −n`.

Tables from the island initialisation fn_00803890 (each step at 24 bits):
- T1 at 0xE9B2D8: `T1[i] = 1/√((0.67·i)² + 100)`, i = 0..255 (0x8038E3..0x803934). The edges P' and Q' are
  perpendicular in xz and measure `√(100 + (0.67·d)²)`, so `s = 1/(|P'||Q'|)`.
- T2 at 0xE9A2D8: `T2[0] = 1`, `T2[j] = 1/√(j·0.000977517 [0x9A2BEC = 1/1023])`, j = 1..1023 (0x803936..0x80396F).

The result is **almost unit length**: the quantisation of T2 leaves a relative error of up to ~0.5/k (in a very
steep cell with k = 7, the length is 0.998). It is never zero: the `dot(n, n) <= 0` check PhysOb had was dead
code. **(port)** With BWLandEditor's 16-bit altitudes, `|d|` can exceed 255: it is computed with the same
formula; and the T2 index is clamped to 1023 (in the original, `|n|² ≤ 1` guarantees it).

**Who uses it:**
- physics (`PhysicsBody.cpp` (was `PhysOb.cpp`) `Normal`/`LandscapeNormal`: `AdjustToGroundLevel` 0x7FCBE2, `GroundAndWater` 0x7FD93F);
- the hand (`HandHolding.cpp`, `InitialisePhysicsFromHand` 0x63729E);
- the fireball (`Fireball.cpp`, `GravityWithFloor` 0x6A1C7F);
- the shadows of villagers and animals (`Renderer.cpp`, fn_00812170 0x8126FD / 0x812859);
- the player camera (`DefaultWorldCameraModel.cpp`, `CameraModeNew3::FindBestAngle` 0x459144).

Before, `GetNormalAt` was central differences of ±0.1 m over `GetHeightAt` (which flattens next to the sea) and physics
had its own copy (`glm::normalize` instead of T2, up in cell 511 and without checking whether there is a block). This is not
`GetNormal`, and is not touched: `LandBlock.cpp` (smooth per-vertex normal of the renderer).

## LH matrices

**Convention.** An `LHMatrix` is 3 rows + translation (row vector, p' = p·M): row k is the image of local axis k.
In glm it is **column** k, with the same memory (`glm::mat4x3` is the 12 floats of an LHMatrix). LH3D rotations
go the opposite way to glm's: the original's angle a is the −a of `glm::rotate`.

**Precision.** The FPU runs at 24 bits, but fsin/fcos are not rounded by the precision control. What the original leaves on
the stack is taken in double and rounded once by the product that uses it. What it stores (`fstp dword`) is a float.
It is the gutils rule («GUtils angles»). **(approximate)** The double is not the 80-bit register: in rare cases
the last bit changes.

**API `openblack::affine`** (`src/3D/ObjectMatrix.{h,cpp}`):

| Function | Original | In glm |
|---|---|---|
| `RotationYXZ(y, x, z)` (was `YXZ`) | `LHMatrix::SetYXZMatrixOnly` 0x7FAC10 | `eulerAngleYXZ(−y, −x, −z)` = `Ry(−y)·Rx(−x)·Rz(−z)` |
| `AngleY(a)` | `AtomCore::SetAngleY` 0x674360; the rotation of `LH3DObject::SetPosition` 0x423140 and `Object::GetWorldMatrix` 0x638200 | `eulerAngleY(−a)` = `Ry(−a)`; = `RotationYXZ(a, 0, 0)` bit for bit |
| `RotationXYZ(x, y, z)` (was `AngleXYZ`) | `AtomCore::SetAngleXYZ` 0x674200 | `Rz(−z)·Ry(−y)·Rx(−x)` (it is not `eulerAngleXYZ`) |
| `RotateY(m, a)` | `LHMatrix::RotateY` 0x5198F0, in place | `m·Ry(−a)` (**on the right**: the object's axes) |
| `RotateZ(m, a)` | fn_0086AFA0, in place | `m·Rz(−a)` (on the right) |
| `TurnRows(m, eje, a)` / `(m, eje, c, s)` | `UpdateRuleRotatePrincipalAxis` 0x6A1150, `AppearanceRuleTumble` 0x6A6200 | `R_eje(−a)·m` (**on the left**: the world) |
| `AxisAngle(eje, a)` | fn_007FB180 (Rodrigues by rows) | `rotate(−a, eje)` |
| `Inverse(m)` | `LHMatrix::SetInverse` 0x7FB290 | inverse, with the determinant floor |
| `PlacementMatrix(p, a, s)` (was `SetPosition`) | `LH3DObject::SetPosition` 0x423140 (vt+0x20) | `T(p)·Ry(−a)·S(s)` |
| `Model(p, R, s)`, `Model(Transform)` | what all the `Set*` write | `T(p)·R·S`, the position as is |

Details, cell by cell:
- **SetYXZMatrixOnly 0x7FAC10** (a = Y, b = X, c = Z): `m0 = (ca·cc) − ((sc·sb)·sa)`, `m1 = −(sc·cb)`,
  `m2 = ((sc·sb)·ca) + (sa·cc)`, `m3 = ((sa·cc)·sb) + (sc·ca)`, `m4 = cc·cb`, `m5 = (sc·sa) − ((ca·cc)·sb)`,
  `m6 = −(cb·sa)`, `m7 = sb`, `m8 = cb·ca`. `cb` (0x7FAC23), `sc` (0x7FAC39), `ca·cc` (0x7FAC41) and
  `sa·cc` (0x7FAC4F) are stored in float; `ca`, `sa`, `sb` and `cc` stay on the stack. It does not touch the translation. CAnim calls it with the
  stored float3 (v0, v1, v2) as `RotationYXZ(v1, v0, v2)` (0x85F28E..0x85F29D).
- **SetAngleY 0x674360**: rows (c, 0, s) / (0, 1, 0) / (−s, 0, c), with `c` and `s` stored in float.
- **SetAngleXYZ 0x674200**: rows (1, 0, 0) / (0, cx, −sx) / (0, sx, cx) (cx, sx in float). Then, in each row,
  `(e0, e2) → (cy·e0 − sy·e2, cy·e2 + sy·e0)` (0x674244..0x6742A2) and `(e0, e1) → (cz·e0 + sz·e1, cz·e1 − sz·e0)`
  (0x6742D2..0x674330). `AtomCore::RandomiseOrientation` 0x6743E0 draws three `PSysFloatRand(2π)`: the **first is z**,
  the second y, the third x (each `fstp [esp]` lands in the slot of the argument it has just pushed).
- **RotateY 0x5198F0**: `r0' = c·r0 + s·r2`, `r2' = c·r2 − s·r0`; r1 and the translation unchanged. **fn_0086AFA0**:
  `r0' = c·r0 − s·r1`, `r1' = c·r1 + s·r0`.
- **TurnRows**: in each row, Z axis (x, y) → (c·x + s·y, c·y − s·x); Y axis (x, z) → (c·x − s·z, c·z + s·x); X axis
  (y, z) → (c·y + s·z, c·z − s·y); the third component is not touched. 0x6A1150 stores `c` in float for Z and Y
  (0x6A117C, 0x6A1229) and leaves `s` on the stack; its X axis (fn_006A12F0) and the Tumble 0x6A627E leave both. That is why there are
  two overloads.
- **fn_007FB180**: `m0 = ((1 − xx)·c) + xx`, `m3 = (xy − xy·c) + s·z`, `m1 = (xy − xy·c) − s·z`,
  `m6 = (xz − xz·c) − s·y`, `m2 = (xz − xz·c) + s·y`, `m4`, `m7 = (zy − zy·c) + s·x`, `m5 = (zy − zy·c) − s·x`, `m8`;
  translation 0. The hand transforms (0, 1, 0) as a row vector (0x5B6EE8): `AxisAngle(eje, a)·v`.
- **SetInverse 0x7FB290**: `det = ((m2·m7 − m8·m1)·m3 + (m5·m1 − m2·m4)·m6) + (m8·m4 − m7·m5)·m0`. If
  `|det| < 1e-10` [0xC371D4], `det = ±1e-10` with the sign of det (+ for 0; 0x7FB2C8..0x7FB2EE). Then each cofactor
  × `1/det`, and the translation `−(t·A⁻¹)` (0x7FB392..0x7FB3DF).
- **SetPosition 0x423140**: four branches on a == 0 and s == 1 (0x423145 / 0x423151); with a ≠ 0, inline RotateY on
  diag(s) (0x4231B3..0x42321C): rows (c·s, 0, s·s) / (0, s, 0) / (−s·s, 0, c·s). With s ≠ 1 the translation is `0 + p`
  (the cells set to 0 plus p: 0x423195..0x4231B0 and 0x423312..0x42332D), which turns −0 into +0; with s == 1 it is copied
  (mov, 0x42325A..0x423268). `affine::PlacementMatrix` does the same.
- The scale multiplies the rows (in glm, the columns) and the translation is written as is (0x423195, 0x6382B7,
  0x607606): `Model`.

**Other LH3D helpers:**
- fn_007FA990 ArcTanOctant(a, b) = atan2(b, a) by octants, fpatan of the smaller over the larger (float quotients):
  a ≥ |b| → atan(b/a) (0x7FA990..0x7FA9AE); b ≥ |a| → [0x8C7B48] (float π/2 as a double) − atan(a/b)
  (0x7FA9BD..0x7FA9DB); a ≤ −|b| → atan(b/a) ± [0x8D45D0] (float π as a double; − when !(b ≥ 0), −0 counts as ≥ 0;
  0x7FA9F0..0x7FAA34); else −π/2 [0x9361E8] − atan(a/b) (0x7FAA3B..0x7FAA47). Ties go to the first branch; (0, 0)
  gives NaN; the first branch returns the unrounded fpatan, the other three a 24-bit result.
- fn_007FAA50 GetYAngle(v): x·x + z·z ≤ 1e-6 [0x9A2BAC] → 0; else ArcTanOctant(−z, x) = atan2(x, −z) (0 along −z, π/2
  along +x); y is not read. `Villager::EndPhysics` turns a row into a yaw this way at 0x5F0AE0..0x5F0AF6 and at
  0x5F0B1A..0x5F0B30: GetYAngle(row), `fadd` π [0x8C36A0] at 24 bits, `fstp`, WrapAngle. Along +x the sum is 3π/2
  rounded, then less 2π: one bit off −π/2.
- fn_007FAAF0 WrapAngle(a): a > π [0x8C36A0] → a − 2π [0x8AB210]; else a < −π [0x8C79A4] → a + 2π; once only (10 →
  3.7168); ±π stays.
- InverseSquareRoot 0x841170: index (bits >> 17) & 0x7F, exponent (0x5F000000 − (e << 22)) & 0xFF800000
  (0x841179..0x8411A0), one Newton step ((3 − (x·y)·y)·y)·0.5 ([0x8C2C50] = 3, [0x8AA3B4] = 0.5, 0x8411B0..0x8411C2).
  The table (0x8411D0): ((bits(1/√x) + 0x2000) >> 15) & 0xFF for x = bits((i | 0x1F80) << 17), 0.5 ≤ x < 2
  (0x8411E7..0x841213), then entry 0x40 = 0xFF (0x841224). A little below the true value.
- fn_007FB5C0 NormaliseRows: each row times InverseSquareRoot of its length squared (0x7FB5E5 / 0x7FB620 / 0x7FB65B),
  no re-orthogonalisation.
- fn_007FAFF0 Mul: a copied first (in place is safe); per row (ai2·b6 + ai1·b3) + ai0·b0 (0x7FAFF3..0x7FB177).
  fn_007FAE60 MulPre: the result's row r is b's row r times a (0x7FAE63..0x7FAFE3).
- LH3DTech::UpdateWorldToCamera 0x819690(eye, target), up (0, 1, 0) at [0xEA1B48]: d = target − eye
  (0x819697..0x8196B5); |dx| and |dz| < 1e-4 (double [0x8C79D8]) → dx = ±1e-4 ([0x8BF518] / [0x8D8738])
  (0x8196B9..0x8196F2); an all-zero d is not normalised (0x8196FA..0x819727); U = up − (up·D)D, not normalised if zero
  (0x8197CF..0x819854); R = U × D (0x819864..0x819896); translation −(R·eye, U·eye, D·eye), each ((−x0·e.x) − x1·e.y) −
  x2·e.z (0x8198BE..0x819904).
- UpdateCamera 0x819AF8..0x819BAB: the W2C's x column × sx [0xE83A00] = 1/tan(fov/2), its y column × sy [0xE83A04] =
  aspect × sx, z copied; aspect [0xE839EC] = W/H (UpdateViewPort 0x81909C); the inverse [0xEA9DE0] by SetInverse at
  0x819BB0. ChangeFov 0x8195B0..0x8195F3: fmul 0.5, fptan, fdivr 1.
- Summation orders: fn_0081F1D0 0x81F1D0..0x81F26B ((m0·x + m6·z) + m3·y) + m9; CheckRegionOnScreen
  0x868D07..0x868D75 z term first, x last; fn_00877370 0x877370..0x8773D8 (Y's order differs from X's and Z's); the
  box centre 0x868CBC..0x868D04.
- fn_00812170 0x8121E6..0x8123B0, the root of the bones: obj+0x14 through W, every cell summed x, z, y; not Mul(object,
  W) of fn_00811010 ([0xEA9E10]), whose column 0 sums z, y, x.
- fn_00839980 0x839B3D..0x839B59: each bone in order (parents first), out[i] = Mul(local[i], parent or root); the exe
  counts the parent indices from the start of each part of the mesh (0x839A26..0x839B59, 0x83A200..0x83A2D4).

**Which constructor each object uses** (vt+0x63C, search in the vtables):
- `Object::GetWorldMatrix` 0x638200, Y only (`T(x, GetAltitude + y, z)·Ry(−GetYAngle)·S`): Abode, Windmill, the
  animals, AnimatedStatic, Feature, BigForest, the creature (vtable 0x8CCE4C), Field, Tree, Villager, the
  worship sites, the spell icons, Totem, StoragePit…
- `MobileObject::GetWorldMatrix` 0x607560 and `MobileStatic::GetWorldMatrix` 0x608DE0, YXZ:
  - MobileObject 0x607560: Arrow, Ball, Pot, PileWood, PileFood, Whale, MagicFood, MagicWood…
  - MobileStatic 0x608DE0: Bonfire, DeadTree, FelledTree, Rock, MagicTeleport, Fragment…
- `Game3DObject::SetPosition` 0x63B740 (LHPoint) / 0x63B680 (MapCoords): `T(p)·YXZ(y, x, z)·S`, with the 9 cells × s.

**How it is used in openblack.** The creations with `AngleY` are the archetypes of Abode, AnimatedStatic, BigForest,
Feature, Tree, Pot, MobileObject and Shark, plus the creature, `DesignedScenery`, the rivers, the worship sites and their
icons, the script's citadel and the ground marks. They also use it when moving: the shark, the paths and the drawing
of villagers and animals («Scawen» angle = `angle + π/2`). Also the physical shield (`MapShield.cpp`: the inline RotateY
of 0x72D4DB..0x72D558 on the identity, with `c` in float, is `AngleY` bit for bit), the rotating glow of the belief
symbols (`TownBelief.cpp`: the angle of sprite +0x14 of 0x69D8C5 carried as the SetAngleY matrix) and
`billboard::YawToEye`. `RotationYXZ` is used by MobileStatic, DeadTree and the hand (`HandAnimator`). `RotationXYZ` is in
`RandomiseOrientation` (PSys). `RotateY`/`RotateZ` are in the moon (`billboard::MoonModel`) and in the ships
(`MissionaryBoat`). `TurnRows` is used by RotateAxis (PSys), the Tumble (`Sprinkle`) and the pick-up particles
(`HandEffects`). `AxisAngle` is in the hand's tilt (`HandPlacement`) and in the physics rotation
(`PhysicsBody::Integrate`). `Model` is used by the renderer and the boxes: `RenderingSystem`, `RenderingSystemTemple`, `Renderer`,
`CarriedProps`, `FeatureBuild`, `Buildings`, `Sharks` and `Archetypes/Utils`.

**Fixed (2026-10-02, demonstrated in the binary):**
- `PSys.cpp` RandomiseOrientation: it was `eulerAngleXYZ(x, y, z)` (the transposed composition), with the random numbers in
  the order x, y, z. Now it is `z, y, x` and `RotationXYZ`.
- `HandEffects.cpp`, the flipping of the pieces when picking up: it went on the right with +a and now goes on the left with −a,
  like 0x6A6200 and `Sprinkle.cpp`.
- `CreatureArchetype.cpp`: `eulerAngleY(+y)` becomes `AngleY(y)`. Not visible today: the script passes π.
- `RenderingSystem.cpp` / `RenderingSystemTemple.cpp`: the model was `R·T(p·R)·S`; now it is `T(p)·R·S`. The earlier
  translation was `R·Rᵀ·p`: a few ulp from p if R is a rotation, but far from p (proportional to |p| ≈ 1000-3000 m) if it is
  not. There are three users whose R is not a rotation, and in them the change is visible:
  - the hand's bands while they fly (`HandMagicFX.cpp` SetTransform: `mat3(M)/scale` of a linear interpolation
    of two matrices);
  - the objects villagers carry on slopes (`CarriedProps.cpp`: the shearX/shearZ shear of fn_0051B220);
  - the physical shield between two turns (`MapShield.cpp` DrawPhysical: interpolates the rows, 0x72CEEC).
- `PhysicsBody::Integrate`, the rotation: the direction was already the original's and now it is built like it. fn_007FE260
  (0x7FE706..0x7FE748) does `inv = 1/ángulo` (fdiv), `eje = paso·inv`, `fn_007FB180(eje, ángulo)` and the rows by that
  matrix (fn_0046D9D0: `r_k' = r_k·M`). Its torque is `F × r` (0x7FE0F9..0x7FE11F and 0x7FD7FD..0x7FD823) and openblack's
  `r × F`, so the original's ω and axis are openblack's with the sign flipped: `AxisAngle(−paso·inv, ángulo)·R`.
  ≈ulp.
- `Particles/Rules/Shield.cpp` (VapourEndEffect, fn_0057D2B0 at 0x6A3D7C): the direction has been read. It is the quaternion
  `(cos(a/2), sin(a/2)·n)` of fn_0057D1D0, its matrix via fn_0057D0B0 (`m1 = xy + wz`: the rotation by +a with the
  right-hand rule, by rows) and the rows multiplied by it (fn_007FAFF0). It rotates each row by +a about `n = last × p` (fn_006A3E20),
  like openblack's `glm::rotate(+a, n)`. The cells are not those of the quaternion **(approximate)**.

**Without a source, left as they were and marked (inferred):**
- `HandHolding.cpp` HeldSway: it remains to read which angle and which axis each fn_007FB180 carries (0x5B49B6..0x5B4ACE).
- The hand's bands (`HandMagicFX.cpp`): they only fit if the bone has Y and Z swapped.
- `HandTrees.cpp`: the toppled tree and the tug (0x5B8700).
- The tree bending in `RenderingSystem.cpp` (0x74B016).
- The sun (`Renderer.cpp`, fn_0086C020).
- `TempleInterior.cpp` (always 0) and `HandArchetype.cpp` (`eulerAngleXYZ`, no effect: HandPlacement rewrites it).
- The villager's initial angle (0x74F950).
- The storage piles: the scale is missing in `AbodeArchetype.cpp`.
- The X-only scale of the rivers.
- The direction of the hand's roll (`HandPlacement.cpp`, the Zoomer CHand+0xD4). The original rotates with
  `fn_007FB180(dir, [CHand+0xD4] + vt+0x14 + [esp+0x20])` (0x5B49A0..0x5B49C8, `rotate(−a)`), but it remains to read the axis
  dir ([esp+0xA4]) and how that matrix reaches the hand's (fn_007FAFF0 0x5B4AE5). openblack rotates by glm's +roll about
  `forward`. Not visible today: the destination is always 0.

## Zoomer (LH3DLib)

**Faithful, checked bit for bit against the original.** API `openblack::Zoomer` and `openblack::Zoomer3` in
`src/Common/Zoomer.{h,cpp}` (0x30-byte structure from bw1-decomp `Lionhead/LH3DLib/development/Zoomer.h`):
- `value` +0x00;
- `destination` +0x04;
- `destinationSpeed` +0x08;
- `speed` +0x0C;
- TimeM2 +0x10 (only ever set to 0, at 0x407D95, 0x441ADC and 0x442744; not stored);
- `time` +0x14;
- `duration` +0x18;
- `startValue` +0x1C;
- `startSpeed` +0x20;
- `c2`, `c3`, `c4` +0x24..+0x2C (coefficients of t²/2, t³/6, t⁴/24).

It is a quartic that starts from the current value and speed and reaches the destination at T with the destination speed and
acceleration 0. Everything is in float and in the x87 order:
- **`SetPosition(p)` 0x441AC0**: value = destination = start = p, and the rest to 0.
- **`SetDestinationWithSpeedAndTime(dest, vDest, T)` 0x407D60**:
  - with `T < 0.001` [0x8AA3B0] (or NaN), `SetPosition(dest)`;
  - otherwise: `A = (T·T)·0.5`, `B = (A·T)·0.33333334` [0x8AB26C], `C = (A·A)·0.16666667` [0x8AB268];
  - the matrix M (rows (C, B, A) / (B, A, T) / (A, T, 1)) is inverted with `affine::Inverse` (0x7FB290);
  - `r1 = (dest − inicio) − T·v_inicio`, `r2 = vDest − v_inicio`;
  - `c4 = (inv10·r2 + inv00·r1) + inv.t.x`, `c3 = (inv01·r1 + inv11·r2) + inv.t.y`,
    `c2 = (inv12·r2 + inv02·r1) + inv.t.z` (0x407E6D..0x407EC5);
  - the order in the exe: A [0x407DAB..0x407DC3] (with `startSpeed = speed` [0x407DBC] and `startValue = value`
    [0x407DC9] in between), B [0x407DD0..0x407DE0], C [0x407DE6..0x407DF9]. Then the matrix M on the stack,
    esp+0x10..+0x3C, with translation 0 [0x407E0A..0x407E40], the inverse [0x407E44], r1 [0x407E49..0x407E55] and r2
    [0x407E57..0x407E5D].
- **The determinant floor.** For this M, `det = −T⁶/144`, which drops below 1e-10 with **T < 0.0493 s**. Then the
  coefficients come out × `(T⁶/144)/1e-10`. The zoomer barely moves and jumps to the destination at the end. A step from 0 to 10
  is worth, at T/4: 2.6171875 with T = 2.5 s and 0.7444 with T = 0.04 s (the closed form would give 2.617).
- **`Update(dt)` 0x442720**: `t = dt + time`. If `t ≥ duration`, it stays at the destination and its speed, with
  `time = duration`: it does not extrapolate. Otherwise, with `a = (t·t)·0.5`, `b = (t·a)·0.33333334` and `C = (a·a)·0.16666667`:
  - `speed = ((t·c2 + a·c3) + b·c4) + v_inicio`;
  - `value = ((((C·c4) + b·c3) + a·c2) + t·v_inicio) + inicio`;
  - `t = dt + time` is `fld dt; fadd [+0x14]` [0x442720..0x442724]. The comparison `fcom [+0x18]; test ah, 1; jne`
    [0x442727..0x442732] follows the curve if t < duration **or if it is unordered**: with t NaN it does not go to
    the destination;
  - the ranges: destination and its speed [0x442734..0x44274E]; a [0x442751..0x442757]; b [0x44275D..0x442761]; speed
    [0x442767..0x44277D]; C [0x442780..0x442784]; value [0x44278A..0x4427A5].
- **`Zoomer3d`** (0x90 bytes: x +0x00, y +0x30, z +0x60):
  - `SetDestinationWithTime` 0x44E760: destination speed 0. There is no three-axis `SetDestinationWithSpeedAndTime`:
    no place in the binary has been seen that gives a Zoomer3d a destination speed other than 0. The x axis calls 0x407D60; y and z are the same code
    inline, and fn_00418A50 just adds an extra 0. The x axis does `push T; push 0; push p.x; call 0x407D60`
    [0x44E770..0x44E77A]; the y and z axes are inlined [0x44E77F..0x44E9E5], with the same threshold and the same
    matrix; the vector times the inverse is done by fn_00418A50: `((v.z·m2i + v.y·m1i) + v.x·m0i) + t.i`, with v.z = 0;
  - `GetCurrentValue` 0x4605D0;
  - `Update`: the three `Zoomer::Update` (GCamera::Update 0x441FEE..0x442029).

**Users:**
- The player camera (`Camera`): `Zoomer3` of the position (GCamera +0x118) and of the focus (+0x88).
  - `CameraModeNew3` gives them a destination every frame with `SetDestinationWithTime` (0x4604A4..0x4604D2).
  - GCamera::Update first calls the mode (vt+8, 0x441FD9) and then `Update(min(dt, 0.1 [0x8AB22C]))`.
  - `Camera::UpdateZoomers` does that.
- The script camera (`script_camera`: position, focus and FOV).
- The hand's roll (`HandPlacement`, CHand+0xD4, 0.4 s) and its distance (g_HandDistZoomer).
- The animals (roll), the piles (sinking, 1 s), the fields (1 s), the totem and the spell's doves and wolves
  (fade).
- **Units matter.** The 0.001 threshold and the determinant floor depend on T, so a Zoomer has to
  be in the original's unit. The totem's one (TotemStatue +0x9C) is in **milliseconds**: SetWorshipPercentage 0x738270
  gives it T = |Δ|·5200 [0x999A98] ms (with the threshold in ms, 0x738293, and an inline copy of 0x407D60 from 0x7382FC), and
  TotemStatue::Draw 0x738960 updates it with the clock's whole ms (0x738967..0x7389B5). openblack passes it ms and
  `segundos·1000` **(approximate: they are not whole ms)**. In seconds, the floor would have slowed changes of |Δ| < 0.0095;
  in ms it only slows those of |Δ| < 9.5·10⁻⁶.
- The callers do not repeat the threshold: `Animal::SetTowardsAngle` calls 0x407D60 directly (0x4185D5), and the threshold
  at 0x407D67..0x407DA8 already does the `SetPosition`.
- `ZoomInterpolator` (the player camera) disappears. It was the same quartic with normalised t, not a quintic. Its
  differences were these:
  - with p0 == p1 it ignored the speed;
  - it divided by p1 − p0;
  - it extrapolated with t > 1;
  - it did not have the 0.001 threshold or the determinant floor.
- The script camera (`ScriptCamera`) keeps its position and focus in two `Zoomer3`.

Checked: `test_camera` `ZoomerMatchesRecording` goes through the original's 11 recordings. The coefficients of each
curve (51 669) and the value and speed of each state (51 636) come out **bit for bit**.

## Random numbers (`game_random`)

`src/Common/GameRandom.{h,cpp}` (namespace `openblack::game_random`). A single state for
the whole game, like the original: the two GRand seeds, the active PSys stream and the seed of the CRT's
`rand()`. Everything **faithful** (read in runblack.exe W120), except what is marked.

- **`_LHRand` 0x7DB600**: `s = ror32(s·9377 + 0x24DF, 13)`, stored rotated (0x7DB629), and returns `s % n` **unsigned**
  (`div` 0x7DB62B). With n = 0 the original divides by 0: all the callers check it beforehand.
- **GRand** (the seeds belong to `GData`, g_game +0x205A30):
  - `GameRand(n)` 0x6DE510 → `GData::Rand` 0x510650: 0 for n = 0 without drawing (0x510693); otherwise, LHRand on the
    **synchronised** seed (+8).
  - `GameFloatRand(x)` 0x6DE530 → `GData::FloatRand` 0x5106B0: 0 for ±0 and **NaN** without drawing (`fcomp 0; test ah,0x40`,
    0x6DE53C / 0x5106FE); otherwise, `(u·x)·k` with u = LHRand(0xFFFF) and k = [0x8D6050] = **0x37800080** (≈ 1/65535).
    It comes out with the sign of x and |r| ≤ 65534/65535·|x|.
  - `LocalRand(n)` 0x6DE570 (long, unsigned division) and `LocalFloatRand(x)` 0x6DE590: the same on the **local**
    seed (+0xC).
  - `GameFloatRange(a, b)` 0x5E1CE0 (the callback [0xEEA380] of `GLandAlignement::Open`): `GameFloatRand(b − a) + a`,
    with b − a rounded beforehand (`fstp`).
- **Seeds** (`Init` / `Reset` / `Save` / `Load`):
  - `GGame::Init` 0x54F4AF sets both to **0x88F89F** (local 0x54F4B4, synchronised 0x54F4BA);
  - `GData::Reset` 0x510750 (`ResetState` 0x5557A0 ← `ClearMap` 0x552BB0 at 0x552E62 ← `StartPlaygroundGame` at
    0x552F4F) sets them to **0**: every LOAD_MAP after the first land (`GScript::LoadMap` 0x6FB36A) and the skirmish
    game (`ResetAndStartPlaygroundGame` 0x54F759). Only the first land of a new campaign starts at
    0x88F89F: `Init` chooses by the startup mode GGame +0x25017C (table 0x54FF60: 1 = new campaign →
    `GSetup::LoadMapScript` 0x54F7AB without `ClearMap`; 4 = skirmish → playground; 0 autosave, 2 load game);
  - `WriteSafe(GData&)` 0x563440 saves both (+8 at 0x56345C, +0xC at 0x563494); `ReadSafe` 0x563620 reads them in the
    same order. openblack has no saved games: `Save`/`Load` have no caller;
  - openblack: `Game::LoadMap` calls `Init()` on the first load and `Reset()` on the others (member
    `_firstMapLoaded`). **(inferred)** openblack has no startup mode: «a script from the Playgrounds folder» acts
    as case 4 and gives `Reset()` on the first load too.
- **PSys** (`game_random::psys`): `PSysFloatRand` 0x6729B0 / `PSysRand` 0x6729E0 call the pointers [0xD4E0C0] /
  [0xD4E0BC]:
  - outside a step they are 0x672990 / 0x6729A0: **0** without drawing (initialiser 0x672A80);
  - `fn_00673340` (the step of each effect) sets them according to +0xAC: synchronised (0x672AB0 / 0x672AF0, via `GameRand`)
    or local (0x672B10 / 0x672B40, via `LocalRand`), and when it finishes it goes back to the 0 ones (0x67349B), **not** to the previous ones:
    the scope does not nest (`psys::StepScope`, opened in `Effect::Step`);
  - +0xAC = `NET_GAME_TYPE == 1`, the sixth argument of `GJPSysInterface::Create` 0x68F2F0 (`sete` 0x68F3AE). The
    effect belonging to a spell (`Spell::InitWithPos`, push 1 at 0x71FF63) and the one-off visuals
    (`GParticleContainer::Create` → fn_0063E410, push 1 at 0x63E436) are synchronised; the others read are local (the hand
    fn_0046E7B0, MagicTeleport, the flock, the storm, the dispenser, the physical shield fn_0072CD40 (push 0 at
    0x72CDA1), the utilities, the object explosion fn_006718E0, the town centre 0x69BC31…). openblack:
    `psys::Effect(…, NetGameType)` and `manager::Start/StartForSpell(…, NetGameType)`, Local by default;
  - the float is `(u·k)·x` (0x672AD7 / 0x672ADD), **in a different order** from GRand: from 0x88F89F with x = 2π,
    GameFloatRand gives 4.314674377441406 and the PSys one 4.3146748542785645. Without «0 for 0»: with x = 0 it still draws;
  - `PSysRand(n)` is `s % n`, not `floor(PSysFloatRand(n))` (first draw from 0x88F89F: 8 versus 6 with n = 10);
  - `FloatRand(a, b)` 0x6729C0 = `FloatRand(b − a) + a`; `RandR3` 0x6729F0: x, y, z = FloatRand(2) − 1 in that order,
    again while `(z² + y²) + x² > 1`. **(approximate)** outside a step the original would never finish (−1, −1,
    −1): here it returns that point once and warns (it is not reachable in the original).
- **CRT** (`game_random::crt`): `rand()` 0x7C8837 (`s = s·0x343FD + 0x269EC3`, `(s >> 16) & 0x7FFF`), `srand` 0x7C882A and
  `Random(a, b)` 0x81D180 = `((rand()·k)·(b − a)) + a`, k = [0x9A3700] = **0x38000100**. The seed is per thread in the
  original and starts at **1** (`__initptd` 0x7D2323); `srand(time)` is only in fn_005776E0 (when saving
  `creature.lhp`), not at startup. openblack: one seed, that of the game thread. CRT draws before the first land:
  `start_system` → `fn_00835AD0` spends 7168 (snow) + 1792 (rain) CRT draws when Weather is on
  ([day-night-weather.md](day-night-weather.md#the-drawn-snow)). That is part of "the `rand()` calls spent before"
  the first sky.
- **Arithmetic**: everything in float, one operation per statement (the FPU runs at 24 bits: fn_007DEE00, `and 0xFCFF` at
  0x7DEE0D), so each x87 `fmul`/`fadd` rounds like a float operation; the constants are written with
  `std::bit_cast` of their bits.
- **PSys noise grid** 0xD066D8 (fn_00590DF0, from `GGame::InitOneTimeOnly` 0x54F0F4, before Init): 256 ×
  `1 − GameFloatRand(2)` from the seed g_game is born with, **0 (inferred)**: the same values in every game
  (`Particles/Noise.cpp`).
- **Already migrated** (first pass): Magic (SpellFlock, SpellForest, SpellWater), Worship (FireFlyReward), ECS/Weather (Climate,
  Storms, WeatherThing, StormClouds, Rain), VillagerFire, ECS/Fire/FireGraphic, PSys (Effect, Mist, LightMap, Mesh,
  Gesture, Lightning, Storm, TownBelief, Noise). `villager::GameRand/GameFloatRand/SetRandForTests` forward to the
  module. The old `graphics::lh3d::Random` / `grand_local::*` (src/3D/LH3DRandom) has been **removed**: its users
  (CameraShake, MistArchetype, the villagers...) now call `game_random` directly (`crt::Random` instead of
  `lh3d::Random`, `LocalRand` / `LocalFloatRand` instead of `grand_local::*`).
- **Since 2026-10-03**: every draw in src now goes through `game_random`: the synchronised GRand (`GameRand` /
  `GameFloatRand`), the local GRand (`LocalRand` / `LocalFloatRand`) or the CRT's `rand()` (`crt::Random` and
  friends), depending on what the original uses at each site (animals, trees, fish, fireflies, fields, rocks,
  fragments, dust, sound and help, clouds, smoke, villagers, CHL RANDOM / RANDOM_ULONG...). `Locator::rng`
  (`RandomNumberManagerInterface`) only remains for the tests' `TestRng` (test_villager_*, test_camera); no game code
  uses it. The sequence still does not match a game of the original (the exact order of all the calls is not
  reproduced): what is faithful is the formula, the resolution and the cycle of the seeds.

## Pending

### MapCoords

Copies of MapCoords that do not use `map_coords` yet (status as of 2026-10-02):

**Batch 2 of the postponed items, migrated (2026-10-02):**
- `Particles/Rules/Storm.cpp`: the tornado dust takes the cell from `MapCoords(LHPoint)` (0x6D2BA3; the port keeps the
  check against the island side, which the original does not do); `PotsByCell` used `map_coords::CellOf` (it was deleted when the cell lists were ported: the pots go at the tail of the fixed list, and each cell is walked fixed then mobile, 0x6D2327); the search
  for what the tornado carries off (fn_006D21B0) walks `map_coords::Spiral` + `AddCells` from the tornado's MapCoords
  (`ToFixed`, 0x6D228D..0x6D22A7), with `InBounds` on each cell (0x6D2311), the own cell by the object's MapCoords
  (fn_00604F40), `GetDistanceInMetres` from the **initial** MapCoords (0x6D2398, not from the cell being walked) and the
  position of the `CanDestroy` event as the MapCoords in metres (0x6D23BA..0x6D2419). Its `SpiralStep` has been deleted.
- `Magic/Objects/MagicTeleport.cpp`: `FastDistance` goes through `gutils::FastDistance` on `FromMetres`;
  `AnyMultiMapFixedNear` (fn_00604C30) walks `Spiral` + `AddCells` over `max(ftol(ceil(2R/10)), 3)²` cells with
  `InBounds` (0x604CC9); `k_UnitsPerMetre` and `ToUnits` have been deleted.
- `Magic/Spells/SpellForest.cpp`: `spell_forest::ToMapCoords` is `ToMetres(FromMetres(p))` (0x725943..0x72595C: the
  product by 6553.6 at 24 bits, no longer in double). `CellOf` already went through `MapInterface::GetGridCell` (= `map_coords`).
- `Magic/Core/SpellSeed.cpp` (fn_006022C0): the cell and the offset within it come from `ToFixed`
  (0x6022E2..0x602300).
- `ECS/Systems/Implementations/HandSpellSeed.cpp`: the circle's MapCoords is `ftol(x·6553.6)`, `ftol(z·6553.6)`,
  altitude 0 (0x5D33DD..0x5D3400).
- Change from double to float (the original's FPU at 24 bits, 0x7DEE0D), reviewed: SpellSeed's `CellOffset`
  (0x6022E2..0x602300), `TopOfObjectsUnder` (0x602388) and SpellForest's `ToMapCoords` (0x725943).

**Lightning and explosion, migrated (2026-10-02):** `Particles/Rules/Lightning.cpp` and `Particles/Rules/Explosion.cpp` now
walk their cells with `map_coords::Spiral` + `AddCells` from the origin's MapCoords (`ToFixed`, 0x69024A /
0x6908B7 / 0x67E56A; no longer `(int)(x·0.1f)`), `InBounds` on each cell and the own cell by the object's MapCoords
(fn_00604F40). Lightning measures from the object's MapCoords in metres (`fild · 10/65536`) with its sums in the
x87 order (cone 0x690410, circle 0x690A06, renew 0x6913BD without root, branch length 0x6923A0), the tip is
`ToWorld(MapCoordsOf) + object::GetHeight` (fn_00691E00, vt+0x42C), the ground of the land points is that of the MapCoords
fn_004427B0 (= `ToFixed`: 65536·0,1f is exactly 6553,6f) and the cooldown `ftol(AverageLightmapLife / ([0xD01A38]·0.001))`
(0x691072, 0x6928FD) no longer uses the dt. The explosion searches with `gutils::GetDistanceInMetres` (0x74CD70) against
`object::Get2DRadius` (vt+0x64), the ring uses `object::GetRadius` (vt+0x60) and `(dz² + dy²) + dx²` (0x67EA1C), the
targets are the MapCoords as a point (0x67E9E1), the turn is `game_clock::Turn()` (+0x205A40) and the smoke lasts
`TicksForSeconds(4)` turns (0x67EEF8). Remaining:
- `Lightning.cpp` `CanBeStruck`: the original first asks `IsAvailable` (vt+0x2C, 0x69038E / 0x690997); the port does not.
- `Lightning.cpp`: `fn_00690C70` (manager targets), `fn_00691E80` (light map height, now `LandAt + 0.1`) not
  read; the cooldown uses `effect.Random` in float where the original calls `PSysRand(int)` 0x6729E0. The original
  passes the `ftol` directly to `PSysRand` (0x691091 / 0x69292F, without checking the sign) and the port skips `PSysRand` with
  `steps <= 0`; `PSysRand` jumps through the pointer [0xD4E0BC], without it having been read what it does with a negative, so the comment
  of `LightmapSteps` («0 ms da +inf, sin enfriamiento») is *(inferred)*.
- `Explosion.cpp`: `manager::CreateSpotVisual` receives seconds and converts them back to turns with `MsPerTurn()`; the
  original passes turns (`CreateSpotVisualWithSpecifiedDuration` 0x63E580, 60 and `TicksForSeconds(4) & 0xFFFF`), so
  with a turn other than 100 ms the count is not the same. Its position does not go through `MapCoords(LHPoint)` 0x603160 either.
  In addition 0x67EEF8..0x67EF2C is an **inline copy** (`div [0xD01A38]; fild qword; fmul 4; ftol; and 0xFFFF`), not a
  call to `NumGameTicksPerSecond` 0x711630; the result is that of `TicksForSeconds(4)`, but the code comment
  should say so.

**Audio** (migrated):
- `Audio/Services/ThingMusic.cpp:81-87`: the round trip in double.
- `Audio/Services/SoundMap.cpp:133-137, 156-157, 188-193, 336-337`: already in float and correct; only using the API is missing.

**Doubtful, not migrated:**
- `CHLApi.cpp:900` (`MOVE_GAME_THING`): truncating there would make a second conversion when walking.
- `SpellResource.cpp:68`: `AddResourceToPos` already converts once, like the original.
- `Trees.cpp:1179-1186`: the cells of the tree bending, with signed cell differences.

**Outside this system, found while auditing it** (not touched; it belongs to its owners):
- `Climate.cpp:231` (`FindWhereToCreateStorm`) compares with `other.outerRadius`, but the original compares with the radius
  of the climate that creates the storm: `fcomp [ebp+0x24]` at 0x772D9C, with `ebp = this`. In `ProcessAll` (0x771DBF) it is indeed
  `[esi+0x24]`, the other climate's, so the difference between the two routines comes from the original.
- `AnimalAI.cpp` `LookForFoodPos`: the number of cells is `(radio/10)²` **in integer**; the original divides in float and
  truncates the square (`fild; fdiv 10 [0x8AB744]; fld st(0); fmul st(1); __ftol`, 0x41A8B3..0x41A8DD): with radius 35 it gives 12
  cells, not 9.

**Others:**
- openblack's positions are float in metres. As long as they are not stored as integer MapCoords, each `ToFixed` of an
  already quantised value can lose one unit (0.15 mm). The cells do not change: a position at an exact multiple of
  0x10000 comes back intact.
- Distances on MapCoords: done, in [GUtils distances](#gutils-distances).
- Review (2026-10-01): `WeatherLand.h` and `WorshipSite.cpp` also change `floor` for the MapCoords
  cell (high word). `PotResource` could walk its spiral with `AddCells` 0x605470, for consistency with the rest.
  `Climate::CellCentre` stays for the two distances of ProcessClimate fn_00772330 (unsigned high word × 10,
  0x7724A6..0x7724DE and 0x772510..0x772548); the rest of the readers use `Centre()`.

### GUtils distances

Copies of distances that do not use `openblack::gutils` yet (status as of 2026-10-02). The rule has been
to migrate **only** where it has been read in the binary that the original calls `GetDistance*` / `hypotenuse`; the rest is left.

**Migrated in batch 2 (2026-10-02):** `MagicTeleport.cpp` (`Distance2D` = `GetDistanceInMetres`:
DoTeleport 0x5FC818 / 0x5FC826 via the twin 0x74CD50, fn_00604C30 0x604CFD, fn_0064D6B0; `FastDistance` via the API),
`MapShield.cpp` (`IsReactionBlockedByShield` 0x72B9B2), `Storm.cpp` (0x6D2398), `SpellStormAndTornado.cpp`
(`ReactToRainOnFire`, fn_0072DCC0 0x72DCE0), `SpellForest.cpp` (fn_005FADF0 0x5FAE30 and fn_007255C0 0x7255CF),
`SpellShield.cpp` (`GetNearestTown` 0x602112 / 0x602193, `IsUnder` 0x72BD3C from castPos +0xCC, `FindShieldAt`
0x72BA4B from **originalCastPos +0xC0** and with strict `Get2DRadius > distance`: before it measured from castPos and accepted
`<=`) and
`ECS/PotResource.cpp` (`IsCloseToEqual` 0x6053C0, from `Pot::AddResourceToPos` 0x66F375). With the GUtils table
root the distances are no longer exact (100 m give 100.02 m: `test_teleport` checks it that way).

**From other owners, not authorised yet:**
- Miracles: `ECS/Influence/Influence.cpp:118-121` (`detail::DistanceXZ`, `std::hypot`, cites 0x74CD70),
  `ECS/Systems/Implementations/VillagerWorship.cpp:161-164` (`FlatDistance`),
  `Worship/WorshipSite.cpp:131, :147`,
  `Magic/Script/CHLFire.cpp:74` and `Magic/Script/CHLSpells.cpp:185`.
- Audio: `ECS/Fire/FireSound.cpp:38-52` (`CameraDistance`: in addition the camera does not go through MapCoords) and
  `Audio/GameQueries.h:47, :72-76` (`nearestTown`, described but not implemented in `Game.cpp`).

**Doubtful, not migrated** (there is no record in the binary that the original uses the routine there):
- `ECS/Trees.cpp:244, :718, :799, :1038`: no address in the citation. (`Worship/Citadel.cpp` `NearestTownOfTribe` no longer
  exists: 0x46345C calls 0x73B170, now `map_cells::GetNearestTownToPos`; see «Object lists per cell».)
- `ECS/Weather/Climate.cpp:431` (`ProcessAll`): does `d2 > r²` and then `sqrt(d2)`, which is not the shape of a call to
  `GetDistance`; `GClimate::ProcessAll` 0x771DBA does call 0x74CDE0 once, but it has not been read where.
- `ECS/AnimalFlee.cpp:596, :619, :653` and `ECS/AnimalPredators.cpp:379, :488, :549, :692`: the report marks them
  *(inferred)* by their place in the file, not by a reading.
- `ECS/Systems/Implementations/PathfindingSystem.cpp:100, :218`: they port `MobileWallHug::MoveTo` 0x60AF20, which calls
  `GetMetresDistanceSq` 0x605FB0 *(inferred)*; their input would have to be migrated to MapCoords first.
- `ECS/Effects/EffectValues.cpp` and `ECS/Fire/FireEffect.cpp` use `gutils::GetDistanceInMetres(vec3, vec3)`, which
  truncates the positions to 16.16 each time. As long as openblack stores the positions in float, that can lose one unit
  (0.15 mm) relative to a stored MapCoords.

**Not yet ported in openblack** (there is no copy to migrate, the API already has them ready): `GetDistanceToCell` /
`GetDistanceInMetresToCell` in `CreatureMental` 0x4D2B3D (the one in `ApplyReactionToLivingObjectsAtSquare` 0x6E4157 already
uses it, in `AnimalFlee`), `ChebyshevDistance` (fn_0074CED0, one caller),
`DistanceChangeToBelief` (0x438770, from the `GetImpressiveValue`) and `CreatureSigmoidThreshold` (0x4F78C0, from
`CreatureDesires::GetIncrementFromSources`).

**Visible changes that have to be checked with a screenshot:** who goes to pray first (`WorshipScore`: now the
nearest ones, and with life³), who goes to put out a fire (`VillagerFire`, 400 m), whom the heal miracle heals (now
everything within R of the spiral point, in a square of `ceil(2R/10)` cells per
side), when an animal changes reaction (distance to the centre of the cell of the ongoing reaction), tree growth with the water
miracle (`GetDistanceModifier(tamaño, 3)`) and the predators' lairs (the sigmoid is no longer computed in double).

### GUtils angles

Status as of 2026-10-02. It has only been migrated where it has been read that the original calls that routine at
that point.

**With the owner of the wall hug** (it is a state change):
- `MobileWallHug::InitStepsXZ` 0x60BFA0 is copied twice, in
  `ECS/Systems/Implementations/PathfindingSystem.cpp:40-51` (`InitializeStep(ToGoal)`) and
  `ECS/Villager/VillagerScript.cpp:86-93` (`InitStepsXZ`), with `glm::atan` in float and the step `(cos, sin) · speed`. The
  original: `GetAngleFromXZ` → +0x5C and the step `StepFromAngle(+0x5C, +0x5A)`. They have to be merged and moved to the API,
  but `WallHug` stores the speed in float metres and the angle in radians. (PathfindingSystem :59 and :541 also have
  their own detour angles.)
- `ECS/Villager/VillagerCore.cpp:959-961` (`LookAtPos`): reads the game angle as `lround(yAngle · 2048 / 2π)`. The
  faithful thing is to store the `u16` +0x5C (`SetGameAngle` 0x60DA90 stores it as is; `SetYAngle` 0x60DAC0 with
  `ConvertAngle3DToGame`). While that does not exist, the `lround` is correct: `ConvertAngle3DToGame` would give a − 1 in 365 of
  the 2048 angles that `setGameAngle` writes.

**For the animals' owner**: the changes to `AnimalAI.cpp` (`AngleDiff`, `IsPosValidForTurnAngle`, `CalcRandomPos`, the
uses of `AngleOf`), `AnimalBirds.cpp` (formation and `BirdDying`), `AnimalFlee.cpp`, `AnimalPredators.cpp` and
`AnimalWallHug.cpp`.

**Review with the Worship owner**: with the altitude at 0 of `GetSpellIconPosFromSlot`, the icons of rings > 0
end up on the ground (that of ring 0 keeps the height of the special point).

**Postponed (owner)**: `Particles/Rules/Lightning.cpp:212` and `Particles/Rules/Storm.cpp:1504` (PSys atan2),
`Magic/Objects/MapShield.cpp:180` and `Magic/Spells/SpellForest.cpp:422` (the polar spiral of 0x725830): none is a copy
of GUtils, there is no need to touch them.

**Already exact, style only**: the hand-written conversions `a · 2π_f / 2048` remaining in `AnimalAI.cpp` (`FaceAngle`,
`SetTowardsAngle`) give `ConvertGameAngleTo3D` bit for bit (without the `& 0x7FF`). `AngleOfRotation` is the inverse of
`FaceAngle`, not a routine of the original.

**No copy to migrate**: `GScript::CastSpellAtPos` 0x70BDD1 computes the angle and throws it away;
`Living::GetFleeingPositionFromStationaryObject` 0x5F2010 normalises in float in the original too; 0x463670
(Citadel) is dead code. FishShoals:207, TestDispensers:281/304, PhysicsObjects:490, Rivers:46,
FishFarmArchetype:68, WorshipSite:721, Climate:224 and SpellWater:179 do their own trigonometry; Sharks:110 is
`LH3DMath::GetYAngle` (LH3D).

**Not ported** (there is no copy, the API already has them): the four-integer overload 0x74D220 (3 callers, none
ported), `GetXByAngleMetersDistance` (`Creature::GetMovementDirection`, `GetRandomLookAhead`,
`RunAwayFromObjectReaction`), `StepFromAngleCoarse` (Villager `Approach*`, PuzzleHorse), `GetPointFromAngle`
(`SetupInspectObject`) and the `ecs::object` functions that nobody calls yet.

**Visible changes that have to be checked with a screenshot:** the spell icons of the outer rings of the worship
site (now on the ground), which way an animal facing exactly opposite its goal turns, and the working positions
next to trees and forests (with the tree's altitude).

### Object size

Status as of 2026-10-02. The rule has been to migrate **only** where it has been read which level the
original uses (the virtual call or the inline read).

**Migrated in batch 2 (2026-10-02):** `Storm.cpp` (`CanSuckUp` 0x6D214C and the search 0x6D238A:
`object::Get2DRadius`), `SpellForest.cpp` (fn_005FADF0 0x5FAE40: `Get2DRadius`, with the API's 5 m field),
`SpellSeed.cpp` (fn_006022C0: the seed's radius is `MeshRadius2D` of its info's mesh × `GetScale`, 0x6022D9;
`GetTopPos` vt+0x630 at 0x602388; `Get2DRadius` vt+0x64 at 0x6023E9 / 0x6023F4), `MapShield.cpp` (`Get2DRadius`
0x72B908 / 0x72B9C2, `GetHeight` 0x72B948; `CollisionScale` is `object::GetScale`; the copies `map_shield::Get2DRadius`
/ `GetHeight` have been deleted), `MagicTeleport.h` (`k_Radius = object::k_MagicTeleportRadius`) and `EffectValues.cpp`
(`ApplyEffectToMapPos`: `GetHeight` vt+0x42C at 0x52536E). The wrappers `effects::ObjectHeight` / `Object2DRadius`
have been deleted.

**Not migrated (doubtful or with more change than a substitution):**
- `HandPlacement.cpp:411-422` (locked pile): the original does not measure the pile there with `GetHeight`. With
  `IsLockedInInteract` (vt+0x6A0, 0x5B3EB3) it takes the position stored in CHand+0x78, converts it to MapCoords with
  `ftol(x · 65536 · 0.1)` (0x5B3ECD..0x5B3F26; it is `ToFixed`: multiplying by 2^16 is exact and 0x3DCCCCCD · 2^16 =
  0x45CCCCCD = 6553.6f, so it rounds the same real number as `x · 6553.6f`, as in fn_004427B0), measures `GetAltitude` (0x5B3F3B) and calls
  `GetHeightForHandAboveInteractObject` (vt+0x64C, 0x5B3F49). It remains to read what it does with that at 0x5B3FDE; changing it touches
  the hand's state.
- `HandPlacement.cpp:719-725` (radius of the living being under the hand) and `:741-749` (radius of what is held): no address.
- `HandTrees.cpp:202-207` (the trunk of the felled tree: `0.2 × Get2DRadius` = Tree's `GetHoldRadius`, 0.3 without a mesh) and
  `:331-341` (the root dust: another formula, unscaled half-axes): no address from the original.

**From audio:** `Audio/Services/LanternSounds.cpp:92, :131` call `Rocks::Height`, which is now
`object::GetHeight`: the value is already the API's; it only remains to call the API directly.

**Closed:** the body's rest counter (PhysOb::Initialise
0x7FB7D9 reads mesh+0x28, the drawn mesh's half height y, × scale × 1000, rounded to nearest by the fistp at 0x7FB7F1;
0x5F0007 in Villager::SetUpPhysOb is GetHeight for the body's shape, not a second half height); the fragments' half
height (the Rock info MS[2] = 0xD3A930's mesh, scale 1, 0x76E9E4 / 0x76EA32); the broad phase's box (half size
`|v.xz| · 0.1 + R` with R = PhysOb +0x150, the 3D radius; fn_006E8160 is the xz length; RaiseUntilNotIntersecting uses R
alone); the water ring of a thrown object (+0x178 = PhysOb +0x150, the body radius, with no guard).

**PLAUSIBLE, not closed, not touched:**
- `HandHolding.cpp:187-192` (picking up a tree: `maxima.y`, not max − min) and `Particles/TownBelief.cpp:191` (`maxima.y` of the
  town centre): it remains to read `UR_TownCentreBelief` 0x69C17A.
- `ComputeHoldParameters` still has its own table of hold types instead of `object::GetHoldRadius` (the hand's
  table also carries the lowering and the type; the radii are already the API's).
- MagicFireBall in physics and in Storm: it depends on whether the ball is an entity with a `Mesh` in those queries.

**Doubtful, not migrated** (there is no record of what the original does at that place):
- The shadow of thrown objects (formerly `Graphics/PhysicsShadows.cpp:155`; now `ShadowList.cpp` uses mesh+0x30 ×
  obj+0x44, the radius of `fn_00874600`).
- `HandTrees.cpp:375, :409` (`0.5 × Size().x`): no address.
- `Particles/TownBelief.cpp:183-196` (the height of the top of the totem): no address.
- `ECS/FireFlies.cpp:98-108` (`MeshHeight` + 2 of houses and street lamps): fn_0052B1D0 is only the filter (`IsAbode` /
  `IsStreetLight`); it has not been read where the height is added.
- `Physics/PartialBuild.cpp:148` (the cut of the construction site): it is in fn_00816AD0, not read.
- `ECS/Trees.cpp:1104-1114` (`MeshHalfDiagonal` of the sources that bend trees): scale × +0x30, and `GetMeshRadius`
  0x636BD0 has no scale; no address.

**Review (2026-10-01):** correct changes that come out of the API and had not been declared: PileFood ×
proportion (0x66F180) and MagicTeleport = 6 (0x5FCCB0) now count in fire and in water, and the MapShield is measured with its
`objectScale`. The fire centre of a WorshipSite is `GetDefaultFireCentrePos` 0x77DDE0 (= `CalculateCentrePos`
0x77DD40, altitude above the land via `Set` 0x603340), together with its 14 m radius (0x77DE10). Still using
`effects::Object2DRadius` / `ObjectHeight` (postponed): migrated in batch 2.

**Not ported:**
- `Creature::Get2DRadius` 0x477F40 / `GetRadius` 0x4792C0 read the LH3DCreature (`[[+0x160]+0x58]+0x5228`), which openblack
  does not have: for now a creature uses the `Object` formula **(inferred)**. The height (0x477F50 = size × 15) is there,
  taking the scale of the `Transform` as the `GetUserSize` 0x4EF4F0 **(inferred)**.
- The creature branch of `GetRoutePlanRadius` 0x6384D8 (needs `NavRadius` 0x480A60).
- `Creature::GetBoundingSphere` 0x479970 (`LH3DCreature::GetBoundingSphere` 0x47F8D0): the creature uses the `Object` one
  **(inferred)**.
- The classes that are not `Object` (Citadel, SpellShield, SpellStormAndTornado, Town, GArena, Reaction, BuildingSite,
  AtomCore, GStreetLight, Mist: see above) do not go through the API; if any of them comes to ask for it, its branch has to be added.
- `GetNearestPosOfObject` 0x636D30: ported (see [GUtils angles](#gutils-angles)); nobody calls it yet in
  openblack.
- The other `GetScale`: `ShowNeedsVisuals` 0x55DD80 (+0x58), `PlannedMultiMapFixed` 0x4050C0 and `SpellSeedGraphic`
  0x727340.
- The mesh-level places openblack does not have yet (`IsSuitableForFixed` 0x603E1E, `Scaffold` 0x6E956A /
  0x6EAC14, 0x7350A5, `PhysOb::Initialise` 0x7FB7D9...): the API is ready for them.
- The box of an animated mesh: the original computes it after `LH3DAnim::SetTransform` 0x83A1D0 (bit 0x100);
  openblack joins the boxes of the vertices as they are. Villagers and animals could have somewhat different half-axes (not measured).

**Visible changes that have to be checked with a screenshot:** the field sinks half as much when emptied; a field or a fish farm measures 5 m for fire, the town (where buildings fit), the animals and the
miracles; a food pile measures according to how full it is (and empty, 0) for fire and the town; a burning worship
site uses 14 m; the height of a creature for fire and healing is 15 × its scale; a food pile picked up
from the map (PileFood, MagicFood, PuzzleGrain) opens the hand according to how full it is, and the empty HandFood closes it completely.

### Game clock

Status as of 2026-10-02.

**Migrated in batch 2 (2026-10-02):**
- `MapShield.cpp`: the fraction of `DrawShields` is `game_clock::TurnFraction()` (`PhysicalShield::DrawShield`
  0x72CEEC / 0x72CF01, g_game +0x205D64); `g_LastTurn` and its wall clock have been deleted.
- `HandSpellSeed.cpp`: `game_clock::Turn()` instead of its `CurrentTurn()`.
- `MagicLoop.cpp`: the turn seconds of `spell_sounds::ProcessTurn` (fn_006D11A0 0x6D11AB..0x6D11C5) and of
  `hand_grain::GameTurnUpdate` (`CHand::GameTurnUpdate` 0x46E4E3..0x46E4FB) are `MsPerTurn() · 0.001f`: the original
  reads [0xD01A38] there, not the hand-written 0,1f.
- `FireGraphic.cpp`: the bursts of steam and smoke read `game_clock::Turn()` (fn_00731AB0 0x731AF8 / 0x731B27,
  fn_00731E50 0x731E7B / 0x731EAE); `g_Turn` has been deleted (`SetTurn` remains for the trace).
- `SpellSeedGraphic.cpp`: the turn of fn_00727350 (0x72736E) is `game_clock::Turn()`.
- `RendererMists.cpp` (fn_007FA300 0x7FA3BE), `RendererSmoke.cpp` (fn_007F8E00 0x7F8F25) and the clouds and the sky
  alignment of `Renderer.cpp` (fn_005E25C0 0x5E25FD, `GLandAlignement::DrawSky` 0x5E2160) do `fild
  g_game_time_inc`: they use `FrameGameMs()` instead of their `static lastTime` with a 100 ms cap and the speed divided.

**Still not migrated:**
- `Graphics/Renderer.cpp`: the sun glare (wall clock without pause or speed: which dt the original uses is
  **(inferred)**).
- `Magic/MagicLoop.cpp` `magic::Update`: passes `FrameGameSeconds() · 1000` to `one_off::UpdateFrames`,
  `mist_atoms::SubmitFrame` and `chain_atoms::AdvanceScroll`; the original gives them the whole ms (`FrameGameMs()`), and the
  round trip through 0,001f may not be exact.
- The readers of `magic::k_TurnMs` (13 uses, `SpellSeedGraphic::ProcessTurn` among them): it is the constant
  `k_MsPerTurn`, not `MsPerTurn()`; it makes no difference as long as nobody changes [0xD01A38].
- **Done**: `Renderer::UpdateClouds` (clouds, sky alignment and `night_lights::Update`),
  `CollectChimneySmoke` and `CollectMists` read `FrameGameMs()` instead of their wall-clock `static lastTime` with a 100 ms cap.
  Readers of [0xEA9EC0] in the original: DrawSky 0x5E2160, fn_005E25C0 0x5E25FD, fn_00823460 0x8234B6, fn_00823570
  0x82359F, fn_007F8E00 0x7F8F25 (the smoke clips to 100 **s**) and fn_007FA300 0x7FA3BE. (pending) the 1 s «rescan»
  of `night_lights::Update` takes the same ms; where it comes from in the original has not been read.
- `Game.cpp:610/612` (fields and trees with real dt): **(inferred)**, not read in `Field::Draw` 0x5286D7 nor in
  `Tree::PreDraw`; if it is `g_game_time_inc` (0x5286D7 reads it) they have to be passed `FrameGameSeconds()`.

**From audio:** `Audio/Services/SoundTags.cpp:145` (local `k_MsPerTurn`, marked «(inferred)»: it is [0xD01A38],
0x54F4A5) → `game_clock::MsPerTurn()`; the duplicate copy of `audio::TickCount` / `MusicStream` → `game_clock::TickCount()`.

**Not ported or doubtful:**
- **Per-turn physics: ported.** `PhysicsObject::GameTurnUpdate` runs once a game turn with its 20
  substeps and the bodies are drawn interpolated with the turn fraction (fn_00646FE0 / fn_007FCE80): physics.md, "Manager".
- **TownBelief** (read, already faithful): `TownCentre::DrawAll` 0x7447F0, from `Process3dEngine` 0x54E032 on **every
  drawn frame (also when paused)**, calls `ProcessPSys` 0x69BCC0 → `GJPSysInterface::Process_` 0x673690, which
  passes as ms **[0xD01A38]** (the ms of one turn, 100) and not those of the frame → fn_00673300 → fn_00673340: dt
  [0xD4E0EC] = ms · 0.001 = 0.1 s (0x673402..0x67340C). So the original advances phase, angles and fight 0.1 s per
  frame (it depends on the fps, as in the exe). Before, openblack did it in `Collect`, which runs when drawing and goes
  **twice per frame** (CollectSorted and CollectQueued): twice as fast and twice as many draws from the local GRand
  stream. Now `town_belief::Step()` (Game.cpp, next to the other frame steps) does the step and the
  draws once per frame with `MsPerTurn() · 0.001`, and `Collect` only reads the state. Also: `fmod 2π` of
  phase, a1 and a2 (0x69C4B7 / 0x69C4F0 / 0x69C50B, the double [0x8D45D8]); the order of DrawAll (list g_game +0x205CFC,
  the ctor 0x743AC3 inserts at the head: newest first; here by creation index); the first Process of an
  effect does two steps (+0xAD = 1 in fn_00672B50 0x672BF7, fn_00673300) and CreatePSys already calls it (0x69BC95).
  (approximate) openblack creates the centre's state the first frame it sees it (the original in MakeFunctional
  0x743F18 / ResolveLoad 0x7448D8) and does not check `IsAvailable` (0x744808). The glows (PlayerSymbolSprite::Draw
  0x69D7E0, g_game_time_inc) advance in the same `Step`, once per frame like Draw_(1) 0x69BF19.
  (pending, from PSys) the double first step of fn_00673300 is not in `psys::Effect` in general.
- **The camera** runs with the profiler's dt (real µs); the original chooses with `GetCameraTimeInc` 0x555820
  (`CameraFrameMs()`, whole wall-clock ms). It has not been changed.
- `PSysManager.cpp:242` (`seconds · 1000 / [0xD01A38]`, now with `MsPerTurn()`): it has not been read in which order the
  original rounds when creating a one-off visual effect; the formula is left.
- `Help/HelpSystem.cpp`: `ReadSpeedFactor` (fn_005C6CB0, returns double; the test `test_help_system` compares it
  in double) and `_endMs = segundos · 1000 + ahora` (0x5C6279..0x5C629B, `fmul; fiadd` at 24 bits) are still in double:
  moving them to float is up to the owner of the help system.
- The flag g+0x14 bit 0x400000 («always do a turn», **(inferred)**), the second `ProcessNetworkPackets` after drawing
  if g+0x205D58 (0x54D3C9) and the cinematic while paused (0x54CCCF..0x54CCEB): they do not exist in openblack. The
  temple's own 100 ms turn while paused (0x54CC88..0x54CCFF) is `game_clock::PausedTurnTimer` and
  `Game::ProcessTempleTurn` ([audio.md](audio.md#the-citadel-interior-in-openblack)).
- `LoadMap` acts both as a new game and as `ResolveLoad` (**(inferred)**: openblack does not load saved games).
- `ECS/AnimalAnimations.cpp:441` (`movedLastTurn · 10`): the 10 turns per second are implicit; it remains to read the
  original.
- `Magic/Objects/ShieldDebugHooks.h:23` says that the PSys does not follow the speed: that is no longer so (its fraction is the
  game's).

### Matrices, Zoomer and normal

Status as of 2026-10-02:
- The rotations without a source in [LH matrices](#lh-matrices) (HeldSway, bands, toppled tree and tug, bending, sun,
  temple, hand, villager, storage, rivers): left as they were until their constructors are read.
- `Game3DObject::SetPositionAndXZYScale` 0x63B390 (`T(p)·Ry(−a)·diag(s·xz, (s·xz)·(y/xz), s·xz)`) and
  `Game3DObject::SetPosition` 0x63B740 do not have their own function: openblack stores the rotation and the scale separately
  in `Transform`, and no place needs them.
- The player camera and the script camera each have their own `Zoomer3`; in the original they are the same ones from GCamera
  **(inferred)**. The player camera's dt is the frame's in µs, not the whole ms of `GetCameraTimeInc`
  0x555820 **(approximate)**.
- `CameraModeNew3`: missing are the second ×2 of the duration, the 1.0 of `MaintainSpell & 0x40` and `[esp+0xB8]`
  (0x4601A9..0x46024A). It is not part of the Zoomer.
- Hand: the Zoomer3d 0xD13FB0 of the «up» when holding (0.4 s), the stretch of the tug +0x11C (0.3 s) and the
  conditions of 0x5B4251..0x5B42CD for the roll. The direction of the roll rotation **(inferred)**: it remains to read the axis of
  0x5B49C8 and what fn_007FAFF0 does at 0x5B4AE5. The sum at 0x5B42DF puts c4·b before c3·a, a different order from
  `Zoomer::Update`: the last bit **(approximate)**.
- The player's FOV (TODO #707) is not a Zoomer.
- `LandIsland::GetNormalAt` compares with `GetCellsPerSide()` instead of 0x200 (the same in the game's maps).
- The normal tables are computed as in fn_00803890 assuming the FPU at 24 bits during the island
  initialisation **(inferred)**.
- The dt of the totem's Zoomer: `segundos·1000` and not the whole ms of 0x738967..0x7389B5 **(approximate)**.
- `Particles/Rules/Shield.cpp`: the cells of the quaternion of fn_0057D0B0 (the direction has been read) **(approximate)**.
- The interpolation of the physical shield between turns: the original interpolates the matrix with the scale inside (0x72CEEC);
  openblack interpolates the rows of the rotation and the scale separately.

### Random numbers

- CHL `RANDOM` (`GScript::Random` 0x6F8DA0: `ftol(min + GameFloatRand(max − min + 1))`, pushed as float) and
  `RANDOM_ULONG` (0x6F8E20: `GameRand(max − min + 1) + min`) now go through the synchronised GRand of `game_random`
  (CHLApi.cpp).
- The trace `OPENBLACK_TRACE_GAME_RAND` takes the place with `std::source_location`, not the stack (`GetCurrentStackString`).
- Threads: no mutex; in Debug an `assert` checks that only the thread that called Init/Reset draws.

## Test hooks

- `game_random` (`test/test_game_random.cpp`): `SeededRandom` (was `LHRand`) from 0x88F89F and from 0, GameRand after Init and after Reset, the
  floats bit for bit (the order (u·x)·k versus (u·k)·x), 0 for 0 / −0 / NaN without touching the seed, x < 0, PSysRand ≠
  floor(PSysFloatRand), the PSys outside a step (0, without drawing), RandR3, the scope that does not nest, the CRT from 1,
  Save/Load and the hooks. `game_random::testing::ScopedState` saves and restores the whole state;
  `testing::SetGameRand` is the hook that used to be in VillagerCore.
- `OPENBLACK_TRACE_GAME_RAND=1`: the original's trace ([0xCD3C80], formats 0xBE899C / 0xBE89D0: seed, turn and
  place) through spdlog at trace level.
- `OPENBLACK_TEST_PSYS_RAND_OUTSIDE=1`: each PSys draw outside an effect's step is counted, its place appears once
  in the log and the total per place on exit. In Land 1 with a storm and a water (2026-10-02): none.

**Checked in the game (2026-10-02):**
- Worship (`OPENBLACK_TEST_WORSHIP="1.0.5"` + `OPENBLACK_WORSHIP_TRACE=1`, Land 2): the 11 villagers nearest to the
  site go (232–278 m); with the previous bug the farthest ones went. The tie-break by life³ is not visible (life does not appear in the
  trace).
- Fire (`OPENBLACK_TEST_FIRE="1785.2.2652.6.450,abode,20"` + `OPENBLACK_FIRE_TRACE=1`, Land 1): the 12 nearby villagers
  react and put it out (215 → 220 ⇄ 216). **Not checked in the game:** the 400 m term with villagers 150–300 m
  from their town (there is no hook; only `test_game_distance`).
- Heal (`OPENBLACK_TEST_HURT_VILLAGERS="1814.0.2660.5.30.0.3.0,200.1,0"`): it heals the cells the spiral walks
  (villagers at 13.3 m with R = 10) and not those it does not visit, like the original.
- Worship site icons (`OPENBLACK_TEST_WORSHIP_SITE="NORSE,0,...,11"`): the outer ring at ground level
  (0x77B002), the inner one on the platform. The step between rings is still 7.5 (the original adds 15, 0x77B100).
- Clock (`OPENBLACK_CLOCK_TRACE=1`, 1356 turns): 10.01 turns/s, 50 turns every 4.995–5.003 s with a single game; the
  loading hitches recover without losing turns.

- `test_map_coords` (`test/test_map_coords.cpp`) checks:
  - the constants, by bits;
  - `ToFixed`, `ToFixedGUtils` and `ToMetres` with the 24-bit values (1464 m → 9594471 versus 9594470;
    16777217 → 0x45200001);
  - the round trip that loses one unit;
  - cells and `InBounds` with negatives;
  - `AddCells`: the fraction is preserved, the cell wraps around through 0xFFFF and, conversely, a spiral that starts in
    cell 0xFFFF (x ∈ (−10, 0)) enters cell 0;
  - the two neighbour tables;
  - the exact sequence of the spiral and the 4×4 square it covers;
  - `SpiralIncrement` and the two sizes.
- `test_game_distance` (`test/test_game_distance.cpp`, was `test_gutils_distance`) checks:
  - the 41 dwords of table 0xC23284, by bits, and that `k_Sigmoid` are those same bits;
  - entries 0, 1, 2, 511, 512 (the 0x7FE000 of the exact 1), 513 and 1023 of the 1/√ table;
  - `InvSqrt(1)` = 0x3F7FE000 and the ≈ 2^63 of zero;
  - `Hypotenuse(int)` with 0, one cell (65568), the diagonal (92691) and the side of the map (33570824);
  - `Hypotenuse(float)` by bits, with the 1e-4 cutoff on both sides;
  - the two unit conversions, including the `fimul` above 2^24;
  - `GetDistanceInMetres` (100 m → 100.0244; 400 m → 400.098), the distance to the centre of a cell,
    `GetMetresDistanceSq` (no table), `FastDistance` and Chebyshev;
  - `SigmoidThreshold` with the threshold in the first argument, the two clamps and the case `a == 1`;
  - `GetDistanceModifier` with the 400 m of `ReactToFire` and the 3 of `Tree::ApplyWaterSpell`, and `max = 0`;
  - `DistanceChangeToBelief`.
  - the cutoff of `Hypotenuse(float)` with a NaN side (returns 0, like the original's unordered comparison).
- `test_game_angle` (`test/test_game_angle.cpp`, was `test_gutils_angle`) checks:
  - the constants by bits;
  - the two tables against the exe dump (sums, a weighted sum and individual entries) and `COS = SIN + 512`;
  - `LHArcTan` on the axes, the diagonals, one point per octant, the overflow of the `shl 8` and the error ≤ 2.27
    steps versus atan2;
  - the conversions: −0.5 → 1886, the NaN, the 365 angles that lose 1 in the round trip, and Scawen by bits;
  - `StepFromAngle` with negative `whole` (`sar`), the 32-bit `imul` of 0x74D320, the (0, 0) of
    `GetPosFromGameAngle(a, 10)` and the 4 lost bits of 0x74D6A0;
  - `GetPosFromAngle` with two cases in which `cosf` gives a different unit, and `AddDistanceFromAngle`;
  - `GetAngleDifference` and `GetAngleSign` at ±0x400;
  - `MapCoords::operator+` / `operator-` with the altitude.
- `test_object_metrics` `PointsAroundAnObject`: the six functions for points around an object, each one with its
  radius and the altitude of `this`.
- `test_worship` calls the real `worship::percentage::WorshipScore`: a villager with life 0.5 at the centre of the
  worship site gives 0.5³ · 0.99996 and one with life 1 beyond d2 gives 3.6e-5 (with the arguments reversed or with life²
  it fails).
- `test_object_metrics` (`test/test_object_metrics.cpp`) checks, with test mesh boxes
  (`object::detail::SetMeshBoxProviderForTests`):
  - the box fields (half-axes, half-diagonal), `Radius2D` and `Height`;
  - that the mesh level does not see overrides and gives 0 without a mesh;
  - the `Object` base and the 0 without a mesh or without `Mesh`;
  - Field and FishFarm = 5 (radius and `GetMeshRadius`, the height is still the mesh's), MagicTeleport = 6,
    MagicFireBall = scale in radius and height, the MapShield's object scale and the creature's height;
  - `GetProportionRaised` of food and wood, with the empty pile at 0, and the food pile's radius;
  - `GetHoldRadius`, `GetDefaultFireRadius` (dead tree, worship site), `GetVillagerHugRadius` and
    the tree's `GetRoutePlanRadius`, the two distances, `IsTouching`, `GetBoundingSphere` and `GetTopPos`;
  - the overrides of the derived ones (`DerivedOverrides`): the sphere of Living and MobileStatic, the shield's
    `GetTopPos`, the hand's height above the fish farm, the `CitadelHeart`'s route radius and the distance and
    `IsTouching` of the worship site with its `WorshipSiteCentre`.
- `test_food_wood`: `PileFoodProportionRaised(0, 1000)` is 0.
- `test_game_clock` (`test/test_game_clock.cpp`) drives the clock with a test `GetTickCount`
  (`game_clock::SetTickSource`) and 33 ms frames. It checks:
  - the first turn in the first frame, with the turn already incremented, `visual = 100` and dt 100;
  - 3 s at 33 ms give 30 turns (not 22), in frames 4, 7, 10… with gaps of 3 or 4;
  - the remainder, the fraction and the dt frame by frame (33, 0.33… 0.99 and, with turn 2, 32);
  - the cap of 99 on the remainder, the dt of 199 and a single turn per frame;
  - the lag of more than 2 s: it is dropped and the next turn waits for `turno · 100`;
  - the pause: no turn, dt 0, the fraction frozen, and when unpausing the stopped time does not count (no extra turn);
  - the speed: the time already elapsed keeps the previous one, the dt follows it, and while paused it is only saved;
  - `OnLoad` (`visual = turno · 100`) and `Start` (the next turn is due immediately);
  - `TicksForSeconds` (truncates; with `SetMsPerTurn(300)`, 1000 / 300 = 3 per second);
  - the wall clock (≥ 1), `EngineMs` and the two selectors (cap of 500); stopped (0 and 1 ms) until
    `StartEngineTimer`, and then it counts from ≈ 0;
  - `EngineTimerAfterLongUptime`: with `GetTickCount` = 0xCE000000 (40 days) the engine ms are exact.
- Environment variable: `OPENBLACK_START_PAUSED=1` starts the game paused (`game_clock::Start(true)`).
- They do not have environment variables of their own.

- `test_zoomer` (`test/test_zoomer.cpp`):
  - `SetPosition`, the 0.001 s threshold (also NaN and negatives) and the arrival at `t ≥ duration` without extrapolating;
  - the step from 0 to 10 in 2.5 s (2.6171875 at T/4) and the determinant floor with T = 0.05 / 0.04 / 0.02 s, by bits;
  - start and destination speeds;
  - two recorded frames of the original's camera;
  - `Zoomer3`.
- `test_camera` `ZoomerMatchesRecording`: each zoomer state of the 11 recordings, coefficients and values bit for
  bit. `ValidateRecordedData` loads the recorded zoomers into the `Camera` and uses `Camera::UpdateZoomers`.
- `test_game_matrix` (`test/test_game_matrix.cpp`, was `test_lh_matrix`):
  - where each axis goes with a quarter turn of each constructor (the signs);
  - the bit-for-bit equalities that follow from the cells: `RotationYXZ(a, 0, 0) = AngleY(a)`, `RotateY(I, a) = AngleY(a)` and
    `AxisAngle(X, a) = RotationXYZ(a, 0, 0)`;
  - the glm composition of each one and the ones that are not (left / right, `eulerAngleXYZ`, the +angles);
  - the floor of `Inverse` (±100 instead of 1e4);
  - `PlacementMatrix` and `Model`.
- `test_land_normal` (`test/test_land_normal.cpp`):
  - the two tables, by bits;
  - a flat cell (1 + 1 ulp) and a slope;
  - the four triangles (by bits and against the plane of the three corners);
  - the quantisation of T2 in a steep cell (length 0.998).

## Sources

- W120 disassembly (`bwdis.py`): 0x603160, 0x603340, 0x603430, 0x6041C0, 0x6042C0, 0x605470, 0x605C40,
  0x5E1860, 0x5E1950, 0x74CA10, 0x74CA60, 0x74D7E0, 0x74D810, 0x74F520, 0x74F540, 0x7A1400, 0x525100..0x525260,
  0x63AFF2, 0x7204D0 (0x72056B..0x720595), 0x882730 (0x8827C7..0x882810) and 0x7DEE00. From the 2nd pass: 0x6014C0,
  0x54F650+0x2A0, 0x5FBB40..0x5FBD10, 0x72F5C0..0x72F6E0, 0x725000..0x725180, 0x5ED080, 0x41A8B0, 0x41A640..0x41A780,
  0x419490, 0x60FC50, 0x7238C0, 0x420E10, 0x771BE0, 0x772BE0, 0x74CDE0 and 0x7409C0..0x740A60.
- bw1-decomp: `src/Black/MapCoords.h`, `Map.h`, `Utils.h` and `Lionhead/LH3DLib/development/LH3DMapCoords.h`.
- GUtils distances: disassembly of 0x74CCA0:1D0, 0x74CE6E:C0, 0x74DCC0:50, 0x74DD00, 0x74E2D0,
  0x74F170:A0, 0x74F290:40, 0x74F580:1A0, 0x74F620, 0x74F680, 0x74F6C0, 0x605CD0, 0x605FB0, 0x5ECA20, 0x657F30,
  0x438770, 0x4F78C0, 0x73C5C0..0x73C64E (life³), 0x552FF0, 0x5252E0, 0x60D9D0 and 0x6E3E60; bytes of 0xC23284
  (164 B), 0x99A1D0, 0x99A1D4, 0x99A1D8, 0x99A1BC, 0x8AC408, 0x8AC41C, 0x8AC400, 0x8AA390, 0x8AA3A4, 0x8AB678,
  0x8AB680, 0x8AB41C, 0x8BF518 and 0x930670.
- bw1-decomp for the distances: `src/Black/Utils.h` (signatures only; `GetDistance` appears as `void`),
  `MapCoords.h:137` and `Lionhead/LH3DLib/development/LH3DMath.h:33`.
- Game clock: disassembly of 0x54C4A0, 0x54C570, 0x54CC30 (0x54CD0F..0x54CD58), 0x54D2A8..0x54D3D3, 0x54AE60:90,
  0x5557E0:60, 0x555820, 0x82F14E:50, 0x711630:30, 0x711610, 0x711280:F0, 0x70CC30:140, 0x66CD00, 0x66CD30:B0,
  0x5C6250:50, 0x714DB0 and, in the audit, 0x818C60:60, 0x8189F0:190, 0x54CD93, 0x54D338:50, 0x879B0A:40,
  0x54E5C0, 0x52AF90, 0x5DBD4E, 0x5C61B0:D0, 0x5C68C0:E0, 0x5537F0:E0, 0x634B40, 0x76EAF0 and 0x54E763. bw1-decomp: `src/Black/Game.cpp` (`PauseGame`, `SetSpeed`,
  `LocalTimerSaysDoATurn`, `ResetLocalGameTimer`, `ProcessNetworkPackets`, `Loop` l. 1834-1996, `ResolveLoad`).
- Object size: disassembly of 0x638110:E0, 0x8082C0:C0, 0x66EB60:C0, 0x66F180, 0x66F1B0:80, 0x477F40,
  0x47B190, 0x4EF4F0, 0x638C00, 0x74B610, 0x5110E0, 0x728640, 0x639AC0, 0x510E10, 0x77DE10, 0x77DDD0, 0x4026B0,
  0x74A1A0, 0x74A140, 0x6384C0, 0x637730, 0x637FB0, 0x5702B0, 0x4027C0, 0x637E00, 0x636D30; the migrated places
  0x53A740, 0x5116A0, 0x74AB80, 0x74B12F, 0x439220, 0x639550, 0x66E900, 0x722B30, 0x403270, 0x6A0D51 and 0x5287A0.
  bw1-decomp: `src/Black/Object.cpp:1062-1104`.
- GUtils angles: disassembly of 0x74D0C0:120, 0x74D200:A0, 0x74D320:180, 0x74D510:180, 0x74D6F0:80,
  0x74DC30:50, 0x74E290:40, 0x605410, 0x6054A0, 0x605520, 0x6055C0, 0x636D30:110, 0x639550:60, 0x74C040:70,
  0x439240:50, 0x439360:70, 0x53A060:70, 0x6E7560:200, 0x77AFC0:C0, 0x75AA90:F0, 0x75B320:80, 0x41B210:140,
  0x418CD0:60, 0x41E930:F0 and 0x41F1B0:80; tables 0xC2307C and 0xC31614 dumped from the exe.
- Matrices, Zoomer and normal (2026-10-02):
  - disassembly of 0x7FAC10:B0, 0x5198F0:70, 0x86AFA0:70, 0x674200:160, 0x674360:50, 0x6743E0:60, 0x6A1150:D0,
    0x6A1218, 0x6A12C7, 0x6A12F0, 0x6A6250:70, 0x7FB180:110, 0x7FB290:160, 0x423140:250, 0x407D60:170, 0x442720:90,
    0x441F80:C0, 0x5B4200:180, 0x803630:260, 0x803890:F0 and 0x516AA0:50;
  - constants read from the exe: 0x9A2BEC, 0x9A2BE8, 0xC3720C, 0x8AB41C, 0xC371D4, 0x8AA3B0, 0x8AB26C, 0x8AB268 and
    0x8AB414.
