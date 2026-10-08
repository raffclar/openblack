# Vortexes and the tornado: objects swallowed, carried and flung

Code: `src/ECS/Vortex.{h,cpp}` and `src/ECS/Components/LandscapeVortex.h` (the vortex
objects), `src/ECS/Physics/ParticleCarriedObjects.{h,cpp}` (was `PSysObjects`; what a particle system carries) and `src/Particles/Rules/Storm.cpp`
(the tornado's rule). The tornado as a miracle (the storm spell, its funnel, dust and pick-up) is in
[miracles.md](miracles.md#the-tornado-ur_tornado-0x6d18b0-ctor-0x6d1680); this page covers the objects.

- [The vortex objects](#the-vortex-objects-landscapevortex-magicvortexcpp-0x5fd2d00x600460)
- [States and fades](#states-and-fades)
- [Taking objects in](#taking-objects-in-the-in-vortex)
- [Bringing them out](#bringing-them-out-the-out-vortex)
- [The land under a vortex](#the-land-under-a-vortex)
- [The objects a particle system carries](#the-objects-a-particle-system-carries-tornado-and-vortex)
- [Drawing](#drawing)
- [What is ported and what is pending](#what-is-ported-and-what-is-pending), [Sources](#sources)
- [Pending](#pending)

## The vortex objects (LandscapeVortex, MagicVortex.cpp 0x5FD2D0..0x600460)

- Three classes, made by `LandscapeVortex::Create(pos, type, 50.0)` 0x5FE6E0 from the CHL `CREATE Vortex` (GScript
  0x6F171B): **In** (0x104 bytes, ctor 0x5FD7F0), **Out** (0x134, 0x5FDE20) and **Volcano** (0xEC, 0x5FD740). Any
  other type makes nothing.
- `CallVirtualFunctionsForCreation` 0x5FEE30 reads the type's **GVortexInfo** row (info.dat, table 0xD38248, stride
  0x40; openblack `InfoConstants::vortex`):
  - the initial state (info +0x20 -> +0xE4);
  - the mesh 0x230 scaled by `baseScale` (+0x28);
  - up to four particle systems: ObjectMover +0x18 (the one that pulls objects), PreLandscape +0x10 (on the ground),
    PostLandscape +0x14 and LightMap +0x1C;
  - for the Volcano only, a looping sound (0xAC).
  The `maxToCreate*` fields are not read: the Out's villager count is a literal 30 (fn_005FD480).
- CHL: `VORTEX_FADE_OUT` 257 (GScript 0x6FD8C0 -> StartFadeOut 0x5FFFD0) and `VORTEX_PARAMETERS` 328 (0x6FE090:
  SetTown 0x5FDFE0 and SetFlockParams 0x5FDFF0 of an Out: the town the arrivals join, where the arriving animals settle
  and the flock's two word fields).
- The global list [0xD38220] (linked through +0x88) is walked once a frame (fn_005FF310 from GLandscape::Draw
  0x5E4E91) and once a turn (fn_005FF330 from PSysGlobal::GameLoopEnd 0x68F5B9).
- A vortex cannot be picked up (ValidForPlaceInHand 0x5FF300 = 0) and is not an effect receiver. In the physics it is
  never a flying body. Only the In is hit by thrown objects: InteractsWithPhysicsObjects 0x5FD8C0 = 1, no raise, row
  13 of PhysicsConstants.

## States and fades

- +0xE4 is `VortexStateType`: 0 Inactive, 1 Active, 2 FadeIn, 3 FadeOut. StartFadeIn fn_005FFFB0 and StartFadeOut
  set the state and +0xB4 = the turn.
- The seconds in the state, `e` (fn_005FF920): `(turns since +0xB4 + the turn fraction) × ms per turn × 0.001`.
- The presence `f` (fn_005FFAD0):
  - Inactive 0, Active 1;
  - FadeIn: 0 for 2 s, then `smooth((e − 2) / 5)`;
  - FadeOut: `smooth(1 − e / 5)` for 5 s, then 0;
  - with `smooth(x) = (3 − 2x)·x·x`.
- The fade and land curves are fn_005FFAD0, fn_005FFAA0 and fn_005FF350; with the retail spline (every y 0) their
  values are exact in float.
- Once a turn (fn_005FF4F0), after 7 s: a FadeIn becomes Active, and a FadeOut becomes Inactive and is deleted
  (`ToBeDeleted`).

## Taking objects in (the In vortex)

- **Once a turn**, only when Active (ProcessContentsOfVortex 0x5FDB60):
  - a spiral walk of up to 9999 cells from the vortex, within Create's 50 m (+0xE8) on every 3rd turn and 25 m on
    the others; the walk stops at the first cell farther than that;
  - every object of each cell that `CanBeSuckedIntoVortex` (vt +0x640) is taken in at rest;
  - a pile gives up to 1000 into a new pile, scaled `(rand(0.3)·0.4 + 0.3) × scale`, and that one is taken.
- **A thrown object that hits it** (ReactToPhysicsImpact 0x5FD8E0) is taken in with its velocity and its body matrix.
  This needs Active, `CanBeSuckedIntoVortex` and IN_PHYSICS.
- The take (fn_005FD9B0):
  - a creature only fizzes, `SetFizz(1, 3, true)`, and only when closer than `Get2DRadius × 0.7`;
  - anything else that `CanBecomeAPhysicsObject`, when the vortex has its ObjectMover particle system (+0x98,
    0x5FDA33), becomes a **VortexObjectInfo**: the object, "from the physics",
    velocity, matrix, the script-held flag, the vortex;
  - the record is queued for the particle rule `UR_VortexAttract` 0x6D39C0. That rule turns each record into a
    particle carrying the object on a spiral into the vortex.
- What arrives and is not script-held is **written to `vortex.txt`** in the save folder (VortexSave, the In's +0xEC,
  `obj->SaveObject` vt +0x82C, fn_0076FA00): see the next section.

## Bringing them out (the Out vortex)

- An In and an Out are **not linked in memory**. The In writes what it swallows to `vortex.txt` (VortexSave mode 0),
  and the Out of the next land reads it back (mode 1, the land-script parser `LHScriptX::InitIfLevel` 0x7E7530).
  That is how the villagers and objects taken on one island come out on the next.
- Once a turn (ProcessContentsOfVortex 0x5FE030), on even turns:
  - it emits at a cell 16 m away at a random angle: the next object of the file, and when the file is empty
    **30 new villagers**;
  - the new villagers are of the town's tribe (type 7, Norse, without a town), half of them housewives and the rest a forester,
    fisherman, farmer, shepherd or leader; every 5th one is a child of `rand(9) + 1` years, the others `rand(6) + 16`.
  The statistics +0x118 / +0x120 / +0x124 count objects, food + wood and villagers.
- The hand-over (fn_005FE3B0):
  - a villager:
    - joins the Out's town, and becomes a disciple 12 (FROM_VORTEX) when there is also a script flock;
    - joins that flock (AddMember); when the flock is in a script, the villager becomes the script's too;
    - is kept alive while thrown (SetLife(1.0));
  - an animal is pointed at the Out's animal Flock (Living::SetFlock), made at SetFlockParams' position:
    - the flock is made at the first arrival, with SetFlockParams' two numbers as its flock distance and domain radius;
    - a new flock replaces it when its tail holds an animal of another kind;
    - SetFlock does not add a member, so the flock stays empty and every animal joins it, whatever its kind;
    - the animal also gets the Out's town (fn_00417C50);
  - anything but a Dove is then **flung** (fn_005FE5F0):
    - `v = (sin a·B, C, −cos a·B)`, with `B = (rand(5) + 8)·s` and `C = (rand(5) + 10)·s`;
    - spin `w = (−v.z, 0, v.x) / (height / 2)` in the body's axes (0x5FE691 reads v.z, 0x5FE6A3 v.x);
    - `InitialisePhysics` with the vortex as thrower: a real physics flight.

## The land under a vortex

- An In or an Out (not the Volcano) **flattens the 11 × 11 cells under it to their mean altitude** as it fades in
  (fn_005FF4F0, fn_005FF350):
  - the cells' original altitudes are saved the first time;
  - each turn the factor `q = 1 − (1 − f)²` grows;
  - every cell becomes `round(alt0 + q·b)`, with `b = mean − alt0` within 50 m, blending to 0 at 56 m.
- The vortex's spline (5 keys at 0, 10, 30, 50, 58 m) has every height 0 in the retail data, so it adds nothing.
- `q` only grows, because FadeOut takes it as 1: the land is **not restored** when the vortex goes.

## The objects a particle system carries (tornado and vortex)

`RenderParticleGameObject` (0x58 bytes, ctor 0x6C9E60) is what a particle holds. The physics part
(`physics::particle_carried_objects`, was `psys_objects`):

- **Attach** fn_006CA0E0: a flying object is not carried (the atom stays empty); otherwise **Take** fn_006CA060.
- **Take**: the class's `InitialisePhysics` with **add 0**. The object leaves the map cells and is flagged
  IN_PHYSICS without a body, and GameThing +0xA & 0x10 ("carried") is set. A villager or an animal enters FLYING; a
  villager first drops what it carries.
- While carried, the object is drawn at the particle's matrix (DrawAt 0x67B170).
- **Release** fn_006C9EF0: angles from the particle's matrix, position on the ground under it, `EndPhysics(NULL)`
  (back in the map), the carried bit cleared. No flight and no impact.
- **The tornado** (UR_Tornado, miracles.md): when the particle dies, a living thing is released and dies
  (`DestroyedByEffect`), and anything else is deleted (the tornado destroys what it carries).
- **The vortex** (UR_VortexAttract): at the end of the spiral the object is released and, when the record says so,
  flung out (above). Otherwise the In's file takes it.
- IN_PHYSICS (+0x24 & 0x40) is set only by `Object::InitialisePhysics` 0x6374A5 and cleared by `RemoveObject` 0x646B56
  and `EndPhysics` 0x6375B9. A resting proxy body never has it.

## Drawing

Once a frame (fn_005FFBB0), drawing only (in openblack, from the render snapshot):
- the ground decal, made while `f > 0` (fn_005FEA70) and released at 0 (fn_005FE8B0), with alpha
  `min(f·255, 235)`;
- the ground particle system, placed at the ground minus `2.5 − 2.2·q`;
- the vortex mesh.

## What is ported and what is pending

> **Code rules.** The vortex state lives in ECS components (`LandscapeVortex`) and its systems are reached through
> Locator services; the info rows come through the resource caches; pure logic such as the fades is tested with
> fakes in `test/`; comments describe behaviour in plain English, with no decompiled names or addresses (those
> belong here). See [the conventions](../refactor/README.md).

| Part | State |
|---|---|
| GVortexInfo, the three classes, Create (radius 50, scale 1), the state and fades, FadeIn -> Active / FadeOut -> delete at 7 s, the turn from GameLoopEnd | ported (Vortex.cpp, MagicLoop.cpp) |
| The land flattening (fn_005FF4F0 / fn_005FF350, the retail zero spline) | ported (`SetCellAltitude` + `RebuildAltitudes`, as the citadel) |
| VORTEX_FADE_OUT / VORTEX_PARAMETERS / CREATE Vortex | ported (CHLApi.cpp) |
| The In's take (spiral, ReactToPhysicsImpact, CanBeSuckedIntoVortex 0x639B60, the same-cell test, VortexObjectInfo) | ported; (pending) the pile split (Storm.cpp's SplitPile to be shared) |
| UR_VortexAttract (the spiral of the carried objects) | pending (the PSys rules) |
| particle_carried_objects Take / Release / Fling, the carried bit | ported (ParticleCarriedObjects.cpp); the tornado's side is in Storm.cpp |
| The Out's emission (the cell 16 m away, the file's objects, 30 new villagers), the hand-over to its town, the thrown list kept alive, the fade held while it emits, the fling | ported |
| The hand-over's flocks (the script flock, the animals' Flock) | ported; (pending) the disciple FROM_VORTEX, the flock's info and player, the animal's town (they need the villager and animal APIs) |
| VortexSave (`vortex.txt` between lands) | the writers: `lhscriptx::WriteCommand` and the physics classes' SaveObject ([land-script-save.md](land-script-save.md)); the reader: one line at a time through the land-script interpreter with the position offset and the last created object (Script's map state); (pending) the other classes' writers and the save folder |
| Drawing (decal, ground particles, mesh) | pending (the render snapshot) |
| Save / Load of the vortex | not ported (openblack has no saved games) |
| The vortex that leaves a land | the scripts open it with CHL `CREATE` of an In at a marker (`challenge.chl`: Land 1 `LeaveThroughVortexL1` at (1700.23, 11.40, 2520.18), once `OpenVortexL1` is set or at once when the creature guide is skipped; Land 2 at (1061.11, 147.79, 3597.97); Land 4 at (2060.18, 11.39, 2591.70); Land 3's script is given its place when it starts). Opening it is ported; going through it to the next land is not (`LeaveLandNow`, then `LandControlAll`'s `LOAD_MAP`, an empty native) |
| Debug window | Debug > Windows > Vortices (`Debug/Vortices`, `VorticesModel`): the vortices on the land with fade out and delete, a new In / Out / Volcano at the hand or at a click, and each land's leaving vortex opened at the script's place |

## Sources

- runblack.exe W120: MagicVortex.cpp 0x5FD2D0..0x600460, VortexSave.cpp 0x76F840..0x7700B0, PSysTornado.cpp
  (UR_VortexAttract 0x6D39C0, UR_Tornado 0x6D18B0), RenderParticleGameObject 0x6C9E60..0x6CA162.

## Pending

- The In's pile split (a pile gives up to 1000 into a new pile, which is taken), to be shared with the tornado's.
- `UR_VortexAttract`: the spiral that carries the taken objects into the vortex.
- The hand-over: the disciple FROM_VORTEX, the flock's info and player, and the animal's town.
- VortexSave: the writers of the other classes, and the save folder.
- Drawing: the ground decal, the ground particle system and the vortex mesh.
- Save / Load of the vortex (openblack has no saved games).
