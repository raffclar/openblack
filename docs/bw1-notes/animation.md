# Skeletal animation (villagers and animals)

How the original animates villagers and animals and how openblack reproduces it: format of the ANM clips, playback,
which clip plays in each state, walking speed, size by age, creation index, drawing between turns, objects
in the hand and clip sounds. Everything described about the original is **faithful** (verified in `runblack.exe`) except
where marked otherwise; what is missing in openblack is gathered in [Pending](#pending).

> **Code rules.** Clips and meshes load once through the resource caches; per-object animation state lives in ECS
> components (`components::SkeletalAnimation`, `components::DrawPosition`) and the systems are reached through the
> Locator; the playback and speed formulas stay pure functions, unit tested with fakes in `test/`; comments
> describe behaviour in plain English, with no decompiled names or addresses (those belong here). See
> [openblack-internals.md](openblack-internals.md).

- [Clips (`Data\AllAnims.anm`)](#clips-dataallanimsanm)
- [Playback](#playback)
- [Which clip plays](#which-clip-plays)
- [Hidden villagers](#hidden-villagers)
- [Size and mesh by age](#size-and-mesh-by-age)
- [Walking speed](#walking-speed)
- [Creation index](#creation-index)
- [Drawing between turns](#drawing-between-turns)
- [Objects in the hand](#objects-in-the-hand)
- [Clip sounds](#clip-sounds)
- [Rendering and test hooks](#rendering-and-test-hooks)
- [Animals](#animals)
- [Pending](#pending)

## Clips (`Data\AllAnims.anm`)

- Clip index = enum `ANM_*` from the game's `Data\AllMeshes.h` (441 clips; the `AllMeshes.h` in bw1-decomp is the
  Creature Isle one and has other numbers). In openblack: `ecs::ClipId(index)` (the manager stores hashed ids).
- Header (0x54): 0x20 **duration in ms** of game time; 0x28 **stride** (metres of one cycle; walk man 1.16);
  0x2C..0x34 displacement vector; 0x38 frames; 0x40 clip size in bytes (openblack called it
  `animationDuration`); 0x50 flags: low byte = bones, **0x100 loop**, 0x200 (at runtime) no distance sync
  (stride < 0.05). `LoadAllAnimations` (0x550180) removes the loop from 323 `P_OUT_OF_PRAY`.
- Frame: pointers and count + bones × 12 floats. **There are no per-frame times** (what openblack read as
  time is a pointer): the keys are evenly spaced. Loop: period = duration and the last key returns to the
  first; no loop: period = duration·n/(n−1) (integers) and it stays on the last one.
- Matrices: rows (x, y, z axes, translation), row vectors; each bone **relative to its parent** (same convention as
  the mesh bones). No root displacement (the advance is only in the header).

## Playback

- Pose (`LH3DAnim::GetPose` 0x839980): i = n·t/period; **linear interpolation of the 12 floats** between key i and
  the next one, without quaternions or re-orthonormalising; W = L · W(parent) (the root under the object's matrix).
  openblack: `L3DAnim::SampleLocal`, `graphics::ComputePose` (`3D/SkeletalPose`).
- Time (fn_005167D0): + ms of game time of the frame (0 when paused); with loop modulo duration, without loop it
  stays at the end. States that move (info `field0x14`): advances by distance, metres / scale / stride ×
  duration (fn_0051AF00).
- Clip change (`Living::SetAnim` 0x5ECBA0): **instant cut** (there is no blending even though the engine has it), the
  same clip does not restart. Per-object state: clip and time in ms.
- The distance advance of moving states (metres / scale / the clip's stride × its duration) is computed by fn_00516840.
- `LH3DAnim::GetPose` 0x839980: evenly spaced keys (a looping clip wraps its last key to the first, a one-shot one ends on it); every element of the 3×4 matrices lerped, no re-orthonormalisation. The blend sampler fn_00839BC0 is the same but the next key after the last is always the first (0x839C1C), one-shot clips too.
- The SuperVillager's cross-fade state (fn_00825530 0x8256C8..0x825755, once a frame): the first clip is only kept (0x8256CF..0x8256DD); another clip starts a fade from the clip and time drawn last, with the fadeMs left and weight 1 (0x8256EA..0x825716); the same clip counts the fade down by the frame's ms (0x825722..0x82574F; below 0 it is 0, otherwise weight = left / fadeMs).
- The blend is drawn while there is fade left and +0x30 bit 2 is clear (0x8257B6 / 0x8257C4; the same bit as the yaw stage's 0x8255B8); the last time drawn is stored at 0x825E2D..0x825E36.
- The SuperVillager's cross-fade itself (fn_00825530 0x8259B0..0x825A09): the blend sampler of the new clip stored with weight 1 − [+0x90] (0x8259B6; 0x839C9A), then the old clip's with weight [+0x90] added (0x839D91, 0x839E6B), in the bones' local matrices, no normalisation; then each bone under its parent (fn_00839F10).

## Which clip plays

`Villager::GetAnimId` (0x750110):

- State 0 or ≥ 255: `P_STAND` 385. If the state has an animation function (fixed table 0xD09198, slot 0x60) it
  decides; if not, the state's clip in info.dat (`villagerStateTable.field0x0`; −4 = not drawn).
- Walking (`MoveToPosAnimation` 0x423400): life ≤ 0.15 crawls, ≤ 0.30 limps; by speed (info.dat threshold
  `speedThreshold` 1 men 3/4 m/s, 0 women 2/3): WALK / RUN / SPRINT `_MAN`/`_WOMAN`; carrying a tool or
  load `CARRY_AXE` / `CARRY_OBJECT_RUN`.
- State change (`SetTopState` 0x5F28E0): exit clip of the previous state (except `field0xf0`), otherwise that of the
  new state and its entry clip. While one of those plays the state logic waits (turns × 100 ms ≥
  duration). Quirk of the original: after an exit clip the entry clip is not seen, the state's clip is repeated.
- `Living::SetAnim` 0x5ECBA0, called through GetAnimId (vt +0x900) at 0x5ECB85: a negative id or the clip it already has → nothing; otherwise the clip (LH3D +0x180) and, with n and not dancing, its time from 0 (+0x188, 0x5ECBDB..0x5ECBF8). A state can name its own clip instead of GetAnimId's (`Villager::AmazedByMagicShieldReaction` 0x765FA5 with 395 TALKING_AND_POINTING).
- `Villager::CallOutofAnimationFunction` 0x756620 returns −1 when the next state has no out-of clip (state table file +0xF0, 0xDB9F68). Otherwise it calls the current TOP's into / out-of function with (0, next).
- An out-of clip sets the flags 0x800 / 0x1000 of Villager +0xE0 (+0xE1 |= 0x18, 0x756699).
- `CallIntoAnimationFunction` 0x756590 calls the TOP's function (Villager +0x8C, 0x756598) with (1, entered). An into clip sets 0x800 and clears 0x1000 ((flags | 0x800) & ~0x1000, 0x7565F5).
- `BuildingAnimation` 0x423E20: the working clip after its into clip, otherwise one of the three at random (GameRand 0x6DE510), and it sets the carried object: hammer, saw or heavy mallet.
- The animation functions span 0x423400..0x424442.

openblack: `ECS/VillagerAnimations` (generated table `VillagerAnimationTable.h`), called from
`LivingActionSystem::VillagerSetState` and `Update`. The functions that need what does not exist yet (landing
type, water, vortices, dances, fights, football, creatures) take the original's branch for its absence.

## Hidden villagers

`Villager::Draw` does not draw the villager if the info.dat clip of its current state is −4 (`ANM_DONT_DRAW`, e.g.
AT_HOME) nor with the at-home flag (+0xE0 & 4, `ArriveHome` / `LeaveHome`): openblack removes its mesh in the
meantime (`SkeletalAnimation::hiddenMesh`), so it cannot be picked up either. The draw test is in `Villager::Draw`
0x51B940 (the current state's clip −4).

## Size and mesh by age

`Villager::SetAge` (0x7528C0): child if the age is less than `grownUpAge` (13; openblack used 18), with the meshes
`childMeshHigh..Low`; an adult is at least 18 years old. Scale (`InitialiseScale` + `SetScaleForAge`): adult 0.9 and
then 1.05 − rand(0.1) (in (0.95, 1.05]); child `ageToScale[age − 1]` + rand(0.75 × the distance to
`ageToScale[age + 1]`). The mesh level drawn is LOD 1.

## Walking speed

**Units**: 1 world unit = 1 m (MapCoords 6553.6 per metre). The speed u16 (+0x5A)
is what it advances **per turn** in MapCoords (`GetSpeedInMetres` 0x60C070 = u16 / 6553.6), and the info.dat tables
(speedGroup) are in those units: 1475 = 0.225 m/turn = 2.25 m/s. openblack moved `WallHug::speed` = 2.25 per
turn (10 times too fast): now it is m/s × 0.1. A normal man does 2.25 / 1.16 ≈ 1.94 step cycles per
second, with the feet synchronised (the clip advances with the same distance).

**Computation** (`ECS/VillagerSpeed`):

1. On each state change (`SetStateSpeed` 0x753760) the speed comes from the speedGroup entry requested by the
   final state (index in `villagerStateTable.field0x24`), × 0.85 if it has a town (base of the town's
   needs) × the wood and food loads (no load: 1); injured: 0.4-0.6 × speed4 or 0.5-0.75 × speedDefault.
2. `SetSpeed` (0x750ED0) multiplies it by f = 1 + ((creation index × 47) % 31 − 16) × 0.01, minus: child
   min((13 − age) × 0.02, 0.4); old (> 60) min((age − 60) × 0.02, 0.4); adult 0.1 × life (and hunger cubed)
   and 0.2 for women; it is truncated to u16.
3. The map scale (`GLandBalance::Values[4]`, `SET_GLOBAL_LAND_BALANCE`, `LandBalance.h`: 1.5 in Land2, 1.25 in
   Land3) multiplies everything.

Result: a normal man goes at ~1.7-1.9 m/s and a woman at ~1.4-1.6. What openblack does not have yet (town
desires, hunger, loads, belief, wonder) counts as neutral: see [Pending](#pending).

## Creation index

`ECS/ObjectCreationIndex`: the counter of `Object::Object`
(0x636520, g_game+0x205A48) used by `SetSpeed`.

- Every thing derived from Object takes the next number when created (buildings, trees, features, rocks, villagers,
  animals, pots, the creature, lanterns; the TotemStatue is one); towns, forests, fogs, roads,
  rivers, planes and the hand do not count.
- What openblack does not create yet reserves its number: 7 TownDesireFlags per CREATE_TOWN, the ScriptHighlight of
  storehouse / creche / workshop / wonder / graveyard (except the African ones), ShowNeedsVisuals and the workshop's
  woodpile, the spell icons of the town centre (one per distinct seed, up to 6), dispenser + seed, and heart +
  visual object + TempleLeash of the temple (its places of worship not yet) (**not checked** that each one is still
  missing).
- Resets to 0 when loading a map (ClearMap → GData::Reset 0x510750) and starts at 2 on the first map after start-up
  (two HelpSpirits).
- Checked: the 55 villagers of Land1 have the same index as a simulation of the original's counter. Trace
  `OPENBLACK_OBJECT_INDEX_TRACE=1`.

## Drawing between turns

`ECS/MobileDrawing`: the simulation moves villagers and animals once per turn; every frame
the original draws them between their position at the start of the turn (Living +0x2C, copied in `Living::ProcessLiving`
0x5EC810) and the one at the end, with the turn fraction (0..0.99; fn_0051AF00): one turn behind, without extrapolating,
with both heights interpolated. The fraction and the frame ms come from `game_clock`
([engine-math.md](engine-math.md#game-clock)): when paused the fraction freezes and the ms are 0.

- Only in the states that move for the animation and with a stride clip (animals: if they moved).
- Turning: the villager turns its drawn yaw towards the real one at 0.003 rad/ms (more than 90°: 0.012·|d|/π rad/ms;
  `Villager::Draw` +0x108); animals turn per turn (their AI limits turning, [animals.md](animals.md)). Birds
  bank in turns (`Dove::Draw`, also in `ECS/MobileDrawing`).
- Terrain slope: on the ground (altitude ≤ 0.2) it is sheared with the slope: row0 += a·row1, row2 +=
  b·row1 with a, b the rise of the terrain at one unit along its x, z axes (±0.3; fn_0051B220).
- `components::DrawPosition` only changes the drawing (RenderingSystem, object in the hand, foot shadows with the
  animated pose); the logic uses the Transform. When picking up or landing it is adjusted without sliding.
- Trace `OPENBLACK_DRAW_TRACE=1`.
- The creature is drawn the same way from its own pose (`CreatureDrawPose`, no shear) through `DrawnModel`, its parts
  from the same matrix, [creature.md](creature.md#drawing-between-turns).

## Objects in the hand

`ECS/CarriedProps`: the villager's `CARRIED_OBJECT` (+0xF1, `SkeletalAnimation::carriedObject`, set by
`SetStateCarriedObject` and by `BuildingAnimation`: hammer, saw or mallet) is drawn with its mesh (table of
`CarriedObject::Init` 0x462530: axe 342, fishing rod 355, crook 354, saw 383, bag 343, ball 344, hammer 367,
mallet 378, scythe 384, shovel 390, wood 406, branches 347-349) attached to bone 15 of the pose (the grip at the end
of the arm −X), with the bone's −X, −Z, −Y axes and no offset (`SetLinkedPosition` 0x815FC0).

- It is an entity of its own (Transform + Mesh + `CarriedProp`) that moves every frame. It is not drawn in the hand
  nor in the hidden states.
- Hook `OPENBLACK_TEST_CARRY=<type>`. Today almost no openblack state sets an object (the jobs are missing).

## Clip sounds

`Audio/AnimationSounds`: `Data\SmallSounds.SAS` gives 115 clips a sound group (1 people,
18 cow, 36 pig, 39 sheep, 40 horse...) and events `ms soundId action`.

- When crossing an event, the key {voice (1 man, 2 woman, 3 child), 2, group, surface, soundId} chooses a row of the
  `LHAudioAnimArrayTable` of editor.sad (the one with the most exact columns; tie: the last one) and a random sample
  from its `LHAudioWaveNumTable` list.
- It only sounds within the sample's `maxDist` from the camera (footsteps: 20 m; the padding NULL.wav have 0 and
  never sound).
- Surface: 7 under water; otherwise the info.dat `surfaceSound` of the cell's second material at its altitude
  (1 grass, 2 gravel, 3 hard, 4 mud, 5 snow, 8 leaf litter).
- The THROWN screams only in the first 15 turns (10 in the vortex).
- Also done: those of VillagersBanter.sad (0x92-0x94; 0x92 sounds in the villager's house), stopping the
  saw (action 1) and the sound following the object.
- They go through `audio::PlayAnimationEffect`
  0x42A4B0 ([audio.md](audio.md)) on the 16 channels, like fn_00516510: the distance and the surface are always those of the villager (also
  for the banter that sounds in its house; without a house, at the camera), the maximum distance 800 and the GAudio
  filters (the banter and the tree whispers are userParam 1: they go silent with the script's widescreen), and a
  footstep (4) with that widescreen discards the rest of the clip's events.

## Rendering and test hooks

Each villager with a pose (`components::SkeletalAnimation`) is drawn separately with its bones (`ecs::PosesByInstance`
in the instance loop of `Renderer.cpp`); the rest is still instanced. Object reflections
(`DrawObjectReflections`: what the hand holds and what is thrown) also use the pose.

- `OPENBLACK_TEST_ANIM="clip[,ms]"`: that clip on all villagers (locked; with ms, frozen at that instant; with
  `OPENBLACK_START_PAUSED=1` they do not move).
- Trace: `OPENBLACK_ANIM_TRACE=1`.

## Animals

Their AI, their states and the clip of each state per species are in [animals.md](animals.md) (`ECS/AnimalAI`,
`ECS/AnimalAnimations`); they use the same playback, distance sync, drawing between turns and sounds as the
villagers.

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `fn_00825530 0x8255C1..0x8256BF, 0x825A13` | the SuperVillager yaw stage (not with bit 2, 0x8255B8); the swim branch poses with the object's own matrix | `Update (DrawPosition follow)` |

## Pending

- Speed: the wood and food loads (pending), the town-needs term from the town's desires (the desires exist;
  SetStateSpeed does not read them yet: pending), the town's belief in the player and the player's wonder
  bonus (neutral 1 until they exist). Hunger is in (GetDesireForFood 0x75BB50 at 0x750FDB).
- Age: ported (`ECS/Villager/VillagerAge`, CheckChildGrownUp 0x751079, SetScaleForAge;
  [villagers.md](villagers.md)).
- Clips: the jobs (objects in the hand) and the `GetAnimId` branches that depend on what does not exist yet (see
  [Which clip plays](#which-clip-plays)).
