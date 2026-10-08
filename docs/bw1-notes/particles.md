# Particles (PSys)

The original's particle engine as the miracles use it: the particle types, the class registry, the PSys linked to a
spell, the hierarchies, the creators (meshes, chains, light maps, mist), the sound of the particles and an index of
the rules with the page where each one is. The drawing of the world particles (and what remains of the PSys in the
render) is in [The PSys in the world](#the-psys-in-the-world-format-step-drawing-and-water-rules); the miracles, in
[miracles.md](miracles.md); the core of the magic, in [magic.md](magic.md).

> **Code rules.** In openblack the effects are reached through Locator services and the spell files, textures and
> meshes load once through the resource caches; resources are owned with RAII and std types; the formulas (frame
> steps, chain UVs, sound sizes) are tested with fakes in `test/`; comments describe behaviour in plain English,
> with no decompiled names or addresses (those belong here). See [the conventions](../refactor/README.md).

- [Particle types](#particle-types-srcparticlesparticletypes)
- [PSys class registry](#psys-class-registry-srcparticlespsysregistry)
- [PSys linked to the spell](#psys-linked-to-the-spell-particlesspelllinkh)
- [Fix in the PSys core: the hierarchies](#fix-in-the-psys-core-the-hierarchies)
- [Creators](#creators)
- [Sound of the particles](#sound-of-the-particles-srcaudiospellsounds-srcparticlesrulessoundcpp)
- [The PSys in the world: format, step, drawing and water rules](#the-psys-in-the-world-format-step-drawing-and-water-rules)
- [Rule index](#rule-index)
- [Test hooks](#test-hooks)
- [Sources](#sources)
- [Pending](#pending)

## Particle types (`src/Particles/ParticleTypes`)

`fn_0068EA00` fills once the table of 150 names `0xD4EBB0` with `.\Data\Spells\ZSpellFiles\<nombre>.txt`; 121
have a file and the rest stay NULL (NONE, TORNADO, FIREWORK, FOOD_IN_HAND...). Some names are stored through a
register (`mov eax, str; mov [ecx+off], eax`), which an older name table lost: FOOD and FOOD_POISONED →
SF_Food, HEAL and HEAL_FX → SF_HealChakra, LANDSCAPE_VORTEX_OUT_BEFORE → SF_LandscapeVortexInBefore (sic).

## PSys class registry (`src/Particles/PSysRegistry`)

Class name → factory for the modifiers (rules, emitters) and the creators. `PSys.cpp` registers the classes that
already existed (`RegisterCoreModifiers`); each new rules or creators file (`src/Particles/Rules/`, `src/Particles/Creators/`)
will have its `RegisterXxx()`, called from the explicit list `RegisterAll()` in `PSysRegistry.cpp`. Whatever is not
registered remains "not ported yet" (no effect). The registered creators derive from `Creator` and call
`ReadCreatorProperties` first.

- The original resolves the class names of a spell file through its persistence registry in `PSysFileData::OnLoaded`
  0x672620; the file is parsed by fn_006B4C40.
- Creator base classes: `ParticleCreator` (common properties fn_006A85E0) and `ParticleSpriteCreator` 0x6AA0D0; the
  mesh creators' DefineProperties are `ParticleBaseMeshCreator` 0x6B37A0, `ParticleMeshCreator` 0x6B38B0,
  `ParticleMeshCreatorAnimTextured` 0x6B3970 and `ParticleAnimCreator` 0x6B3D70; `ParticleChainCreator` props
  0x6B4760; `ParticleLightMapCreator` props 0x6B45E0.

## PSys linked to the spell (`Particles/SpellLink.h`)

- The spell owns its effect and advances it itself with its PSysProcessInfo (`manager::StartForSpell` /
  `ProcessForSpell`); `ProcessTurn` does not touch it.
- `StrengthFloatProvider` = `info.power`; `EventConditionTrueWhenEnabled` = `info.enabled`.
- `LandscapeCollide` with `SendEvent` sends event 3 (global position, movement of the step, strength 1) before deleting
  the atom. Event 1 is sent at the start (fn_00673070).
- `LandscapeCollide` is 0x67D6E0: its `SendEvent` flag is +0x20 and the event 3 carries no shield check.
- `EventAlways::ModifyAtomCore` 0x69DBB0: every step a type 2 event at the atom (global position, last global
  movement, strength 1, no shield check).
- `AtomCore::GetLastGlobalMovement` 0x674420: the drawn position minus the previous drawn one (0 until the atom has
  been drawn twice).
- `PSysManager::SpellEvent` 0x6734C0 forwards the event to `Spell::SpellEvent` (vtable) and returns its result (1
  applied, 0 not); 0 when the effect has no spell.
- `PSysManager::GetPowerUpLevel` 0x673510: the spell's level (−1 base, 0, 1); −1 without a spell.
- The spell queries a rule can make: `Spell::NetUnsafeIsMyInterfaceCasting` 0x7201E0 (spell +0x44, this computer's
  interface casts it), `Spell::IsHumanPlayerCasting` 0x720230 (+0x4C), `Spell::IsScriptCasting` 0x720270 (its creator
  is the script's player, g_game +0x205A5B, the neutral one), `Spell::GetPlayer` 0x55CDF0 (+0xA4).
- `PSysManager::IsInState(1)` 0x673050 is "closing"; `IsInState(2)` is "enabled" (`PSysProcessInfo` +0x38).
- `SpellTargets` points: the `AddTarget` that takes an LHPoint is 0x670CF0 (12 bytes per point at +0x14); fn_006710B0
  counts points plus objects; fn_00670E30 is the cyclic read cursor.
- (inf) While a PSys class is not ported, a spell's effect counts it as a creator until it is closed.
  Otherwise, an effect with no ported classes would end in the first turn and cut the spell's cycle short.

## Fix in the PSys core: the hierarchies

- In the original (ctor of `AtomCollection` 0x6748B0, `LocalToGlobal` 0x6751D0 / fn_006752D0, fn_00673DB0,
  `CommonInitNewAtom` 0x674C60): a collection is in a hierarchy if **any** ancestor belongs to a group marked in
  `Hierarchies` (not only the parent); its frame is the product of the local matrices of **the ancestor atoms whose
  group is marked** (the others do not count), and that matrix carries the atom's **scale** (baseScale × ruleScale, the Y
  × the stretch). A new atom is born at 0 if its parent belongs to a marked group and otherwise at the parent's position.
- Ported in `Effect::CreateCollection`, `LocalToGlobal` / `GlobalToLocal` (new), `GlobalPosition`,
  `SpawnPosition` and `PostUpdate` (the frame and its scale pass down through the unmarked atoms; the drawn scale of an
  atom in a hierarchy is also multiplied). The dome needs it: the patches (group 2) and the sparks (group 4) are in the
  frame of the root, which grows from 0.1 to 1 in 2 s (`UR_ChangeScale`) and shrinks when it closes. It also changes,
  towards the original, SF_Bonfire, SF_Smoke/Steam (root 0.8) and the effects in the hand and on the pedestal that scale
  their root (`SetScale`). No rule with its own `GlobalToLocal` (HandFollow, Fireball) is in an affected collection.
- `AtomCollection::GlobalToLocal` 0x675410 is the inverse of the `LocalToGlobal` frame (outermost ancestor first);
  `AtomCore::GlobalToLocal` 0x673D40 brings a point into the parent atom's frame (atom +0x80) in a hierarchy and leaves
  it global otherwise; `CalculateGlobalPos` 0x673D80.
- `CommonInitNewAtom` 0x674C60: with no parent the new atom starts at the effect's origin; with a parent of an
  unmarked group, at the parent's position (fn_006744B0).
- `GetCurrentParentPos` 0x674AF0 (the parent atom's +0x80, or the manager's origin) and `GetCurrentParentVel` 0x674BB0.
- `AtomCore::AddSubCollections` 0x673B70 adds new sub-collections of the given groups under an atom.
- fn_006731B0 makes a new atom in the first root collection of a group (the lightning's light maps go into their
  InitiallyCreated group, not under the fork); nothing when the group has no root collection.
- `AtomCore::MoveToBaseGroup` 0x673BD0: fn_00673180 finds the first root collection of the group and fn_00673C00
  unlinks the atom and links it at the head of that one; the atom keeps its position, velocity and data (the tornado's
  flung objects, 0x6D33F1). With no such collection the original leaves the atom in none (never updated nor drawn
  again); openblack deletes it (approximate).
- Per-collection rule data: the `BaseCollectionModifierData` list at collection +0x24 (each with its modifier at
  +0x1C), found by modifier (UR_CloudGather 0x6D4AB0, UR_Tornado 0x6D18C3, its sub-collections 0x6D2B10 / 0x6D2EA6) and
  destroyed with the collection.
- `PSysManager::SetOrigin` 0x672E10.

## Creators

The registered creators derive from `Creator` ([PSys class registry](particles.md#psys-class-registry-srcparticlespsysregistry)). Those of the miracles:

### The particle meshes (`Creators/Mesh.cpp`, Particle3DObj::DrawAt 0x679FD0) and the shield dome

- **Faithful:** the ctor of `ParticleMeshCreator` 0x6A8960 sets `MeshChangeMaterialProps` = 1 and two-sided = 1 (before,
  0 was taken): thus **`UseAdditiveAlpha` applies to the dome** (SF_DefenseSphere does not give MeshChangeMaterialProps)
  and to the lightning bolt. With it, `GJUtils::SetMaterialProperties` 0x57E120 gives mode 13 (SRCALPHA / ONE, no Z
  write): the transparent pass now draws it that way (`RenderContext::additiveInstances`, `Renderer.cpp`).
- **Faithful:** `UsePlayerColor` / `UsePlayerColorBlend` (fn_006A85E0, for **all** creators, sprites included: it was not
  applied in any of them): pc = `GetPlayerColour` of the effect's player with alpha 0xFF (the neutral's black becomes
  white); with blend b = ftol(Blend × 255) & 0xFF ≠ 255, each channel 255 + ((c − 255) b >> 8); the atom's colour × pc
  >> 8 per channel. The dome uses 0.5 (with player 1's red: (254, 162, 162)). `psys::TintWithPlayerColour`.
- **Faithful:** `FaceCamera` (0x67A032): θ = atan2(d.z, d.x) − atan2(r2.z, r2.x) with d = position − camera in x/z and r2
  the Z row; r0' = cos θ r0 + sin θ r2, r2' = cos θ r2 − sin θ r0; the Y row × `HeightStretch` (ctor 1).
  `FaceCameraSprite` (0x67A250) is not used by any file; it is ported (`billboard::FullSprite`) for completeness. Both
  are in [rendering-objects.md](rendering-objects.md#objects-that-face-the-camera-billboards).
- With this **the shield dome is now visible**: the 32 plates
  `MSH_S_SPELLBALLSURFACE02` are drawn in additive mode with the player's colour and form a clear bubble,
  instead of the barely visible patch from before.
- `ParticleBaseMeshCreator` ctor 0x6A87C0: MeshEnum −1, HeightStretch 1, FaceCamera / FaceCameraSprite / the pulse 0;
  `ParticleMeshCreatorAnimTextured`'s ctor 0x6A8BB0 calls it and sets the same flags as `ParticleMeshCreator`
  (0x6A8BBD..0x6A8BF6).
- MaterialProperties +0x59 (the alpha flag, no property, 1 from the ctors): 0 would make every material TexturedAlpha
  (`GJUtils::SetMaterialProperties` 0x57E13D).
- `Particle3DObj::DrawAt` 0x679FD0 tests `FaceCamera` (+0x4D) first (0x67A040) and only otherwise `FaceCameraSprite`
  (+0x4C, 0x67A250), which restarts from the identity times the scale.
- The object's scale obj+0x44 is the drawn PSR's +0x30 (0x67A000..0x67A009 / 0x67A445..0x67A44E).
- Frame count of an atom (+0x114): NumFrames, or 1000 for the sliding textures (AnimTextured) and the animated meshes
  (fn_006A97F0 0x6A9854 / 0x6A988C); AnimTextured's first frame is `PSysRand(N)` over that N (0x6A8E6E, 0x6A8EE2).
- **`DrawCutByPlane` does cut:** the call changes (fn_00679F20 `test al, 4` 0x679F29: vt 0x11C 0x679F4A instead of
  vt 0x104 0x679F52, in both paths), and for the `LH3DStaticObject` of a particle (`LH3DObject::Create(0)`
  0x80B4F8, vtable 0x9A2974) vt 0x11C is fn_0080C050, which is **not** a direct draw: plane from fn_00822560, clipping
  per triangle (fn_0081D2C0) and light fn_00858BA0, the same as the animated fn_00811C70
  ([rendering-objects.md](rendering-objects.md#cutting-by-the-water-plane-drawcutbyplane)). The bit is +0x24 & 4 of the
  atom, set from +0x5F of the creator (0x6A8B94..0x6A8B9A): `psys::mesh_atoms::Instance::cutByPlane`, which
  `RenderingSystem` sends to `sea_pass::CutAtoms` (default plane, what is at y ≥ 0 is visible).
- **(approximate)** the colour goes through the object tint of `vs_object` (−1 − r·65536 − g·256 − b in the x of the fifth
  column, `argb_colour::PackInstanceTint`; without `DrawWithLandscapeColor`, 1 + rgb with `PackInstanceColour`), which multiplies the ground light: that is what `DrawWithLandscapeColor` (fn_0080BEC0) does; without that flag the
  original sets only the colour (`SetColour` vt 0x2C → obj +0x4C / +0x50). Not ported: `UseScriptHightlightPulse`
  (fn_0070A510), `UseDynamicLighting` (bit 0x20), `UseGlobalAlpha` and the per-object Z
  order. `CastHumanShadow` (+0x5D) is ported: CreateParticle 0x6A8B76 creates a
  ShadowInfo per atom (fn_006CA340) and DrawAt puts it, with the particle's object, in the list [0xD4EDCC]
  (0x67A45D..0x67A494); every frame GGame::Process3dEngine 0x54DEAD → PSysLightMaps::AddDrawing 0x6CA6E0 →
  fn_006CA540 walks the list and updates each shadow (fn_006CA3D0, called at 0x6CA5A9 → fn_00874850), so the
  original does cast it. In openblack `mesh_atoms::HumanShadows` (the list of the last Collect) is read by
  `graphics::shadow_list`. It is not noticeable because it is 0 in the 20 mesh creators of the data. They do not
  receive shadows either: +0x54 (vt+0x78) is always 0 (`k_ReceivesShadow`).
- `CastHumanShadow` +0x5D exists only in `ParticleMeshCreator` (DefineProperties 0x6B393E): in AnimTextured +0x5D is
  its `NeverClip` (vt+0x98, 0x6A8D7C) and ParticleAnimCreator has none. The particle's dtor fn_006C7A80 0x6C7AA6 takes
  its shadow out (fn_006CA370 → fn_008745E0 → fn_0087FF10).
- **`ParticleAnimCreator`** (the forest butterflies and bats, SF_Butterflies, SF_ButterfliesOnObject):
  each atom is a boned mesh that plays an .anm. **Faithful**, except for what is marked.
  - **On creation** (CreateParticle, vt 0x10 0x6A98C0 → `fn_006A97F0`): each particle has its own type 2 object
    (`CreateLH3DObject` 0x6A9760: `LH3DObject::Create(2)`, the mesh with vt 0xF4 and the clip with vt 0x180). In the atom
    (0x6A9843..0x6A98AD): the rate +0x110 = 1000 ([0x8AB228]) / ms of the clip (LH3DAnim +0x20) × `SpeedUpFactor` (+0x44)
    × 1000; +0x114 = 1000 frames per cycle; +0x118 `PlayAnim` (+0xA1) and +0x119 `LoopAnim` (+0xC). With
    `RandomiseInitFrame` (+0xA2), the first frame is `PSysRand(1000)` (SetFrame 0x674100, in +0x108 and +0x10C).
    Default values of the ctor (0x6A93A7..0x6A93E5): SpeedUpFactor 1, PlayAnim 0, RandomiseInitFrame 0,
    NeverClip 0, UseDynamicLighting 1, UseGlobalAlpha 1, FrameToStartBlend 0, FrameToEndBlend 1000.
  - The ctor 0x6A9200 also sets +0x90 UseAdditiveAlpha 0, +0x91 Z write 0, +0x92 double-sided 1, +0x93 and +0x94 1;
    it has no `MeshChangeMaterialProps` property.
  - **On drawing** (`Particle3DAnim::DrawAt` 0x67A8E0): the integer frame of fn_00679920 (DrawData +0x10, 0..999)
    becomes clip time with `GetCycleTimeFromFrame` 0x6C85F0: ms × f / 1000, in integers (imul and then
    × 0x10624DD3 sar 6, which rounds towards 0). That time goes to the particle (+0x28) and to the object (vt 0x188 fn_0080B880:
    +0x84). The drawing of the type 2 object (fn_008175B0, 0x8177B8..0x8177CE) sets the bones with `LH3DAnim::GetPose`
    0x839980 of clip +0x80 at that time. **Frame 0 is not drawn**: 0x67A9B7..0x67A9BE exits before vt 0xF8 and
    the draw.
  - **The clip** (`fn_006A9570`): with AnimEnum −1, the `AnimFileName` loaded with fn_00839900 (the whole file with
    LHLoadData 0x83993C, then the fixes of fn_0083A610; thus LH3DAnim +0x20 is the 0x20 of the header).
    `AnimFileName` is a std::string at creator +0x80 whose pointer +0x84 fn_006A9570 reads (0x6A959D).
    S_Butterfly_Flap.anm lasts 366 ms (2732 frames per second) and M_Bat_Flap.anm 800 ms (1250 per second). The
    meshes: `S_Butterfly.l3d` (with bones, flags 0x22103) and `MSH_A_BAT_1`.
  - **In openblack**: `psys::AnimFrameRate` and `psys::AnimCycleTime` (`Particles/Creators/Mesh.h`). `mesh_atoms::Collect`
    puts the bones in `Instance::pose` with `graphics::ComputePose`, the same code as the villagers and the animals.
    `RenderingSystem` passes them to `RenderContext::instancePoses`, and `ecs::PosesByInstance(renderCtx)` merges them with
    those of the entities. The renderer draws each atom with its bones, like a villager: the 32-bone variants of
    `vs_object` (`Renderer::BonesVariant32`), no new shader. `mesh_atoms::Any()` replaces `!Collect().empty()`
    in `magic::Update`, so as not to compute the poses twice per frame.
  - (faithful, game_random) `PSysRand` (function pointer [0xD4E0BC]) is the one from `game_random::psys`: `s % n` over the
    step's stream (0x6A98A2 in ParticleAnimCreator, 0x6A8EE2 in AnimTextured).
  - (pending) `AnimEnum` (+0x7C, `LH3DAnim::AnimPack` [0xEDD508], pack[0] out of range, 0x6A957C..0x6A959A):
    no file uses it. The DrawAt blend 0x67A946..0x67A9B1 to `MeshFileName1/2` (+0x38 / +0x3C, both are
    needed) between `FrameToStartBlend` and `FrameToEndBlend` (vt 0xDC fn_007F9A80): `NULL_STRING` in all of them.
    `UseSuperSortedPolys` (vt 0xD4; 0 in all), `UseDynamicLighting` (vt 0x58 fn_008168C0, the model light), `UseGlobalAlpha` (vt 0x48 fn_007F9D60, bit 0x80 of the object's +4; 1 in all: the alpha is
    drawn as in every PSys mesh) and `NeverClip` (vt 0x98 fn_007F98E0 with !NeverClip). The renderer culls by
    the sphere of the rest box, which is not the original's clipping. `ParticleAnimWithCameraCreator` (the goddess and
    the camera) is still not ported.

### Chains and light maps (`Particles/Creators/{Chain,LightMap}.cpp`, `Graphics/RendererChain.cpp`)

- **`ParticleChainCreator`** 0x6AA900: the atoms of a collection are the joints of **one** ribbon. The per-collection
  drawing (fn_0067B3F0) orients it to the camera: at each joint the side vector is
  `normalize(cross(camera − joint, segment direction)) · scale` and the vertices are `joint ± side`
  (the PSR's whole scale is the half-width, 0x67B9E6..0x67BB0D; an exactly null side, e.g. a segment of
  length 0, stays null, 0x67BA04..0x67BA39). Where two segments meet, the vertices are moved to their midpoint
  (0x67BD2D..0x67BE82). Indices per segment (0, 1, 2) and (1, 3, 2) (0x67B77A..0x67B7D8). UV (fn_006C8920, with uv0 at
  head + side): **U goes across the width**, `[(frame+FileOffset)·FrameWidth, +FrameWidth]/256`, with frame
  `FrameOfHead` in the last repetition, `FrameOfTail` in the first and 0 in the rest. **V goes along the length**,
  `FrameHeight/256` per repetition; there are `NumTexturesForWholeChain` repetitions, −1 = one per segment. Details in
  [rendering-objects.md](rendering-objects.md#frame-animated-textures).
  - Default values of the creator: 64 high, 32 wide, −1 (ctor 0x6AA739..0x6AA747).
  - Thus the lightning bolt uses strip 0 of `S_Lightning.raw` (white core and cyan halo), 4 times.
  - Before, the port had 256×256 and the axes swapped, and the ribbon came out almost transparent.
  - `ChainJoint::DrawAt` 0x679E80; the ribbon fn_0067B3F0 calls the UV fn_006C8920 at 0x67BEFD with frame 0. Textures
    `S_Lightning.raw` / `S_Beam.raw`; users: the lightning bolt, the storm lightning, the gesture trail and the
    creature beam.
  - `ParticleChainCreator` ctor 0x6AA6E0..0x6AA751; `CreateChain` 0x6AA880 copies the layout to the Chain (ctor
    0x6C8830): FrameWidth +0x64 → chain +0x20, FileOffset → chain +0x34, repeats → chain +0x30.
  - Repeat of each segment (fn_006C8920): segment s falls in repeat k = ((s + 1)·T − 1) / (n − 1) (integer division),
    which starts at segment k·(n − 1)/T; with a single repeat the head frame wins (the k == T − 1 test comes first).
    openblack counts the joints drawn now, the original the ones the chain was made with (approximate).
  - `NumTexturesForWholeChain` 0 (allowed by DefineProperties) makes the original divide by zero (idiv 0x6C893E);
    only −1 is replaced.

  For this the `Effect` has a new `CollectChains` that returns the chain-type collections with their
  joints **in order** (the normal `Collect` flattens them). It is drawn after the sorted sprites, in
  `MainBlended`. A collection without bit 2 of +0x38 (the forks of the lightning bolt) is drawn without interpolation
  (fn_00679920 0x67999E); the frame is always interpolated (0x679A79).
  - The vertical UV scroll over time (chain +0x3C, `frame_anim::ChainScroll`, 0x67BE91, `fmod` by
    `FrameHeight/256`) is ported, but its rate +0x4C is only set by UR_SimpleBeam and UR_Plasma, not ported: it is 0
    in all of openblack's chains. The chain's V scroll and offset are gated by [0xC029B8] (1).
  - Not ported: `UseDynamicLighting` (colour × `clamp(0.6 + 0.4·(n·L))`).
  - `OPENBLACK_PSYS_CHAIN_TRACE=1` writes per frame how many ribbons there are, with how many joints, their texture and
    where they go from and to: it serves to tell "is not drawn" apart from "there are none in that frame".
- **`ParticleLightMapCreator`** 0x6A9D80: `GJBitmap::LoadBitmapFromFile(name, Pitch, 3, NumFramesInFile,
  NumFramesInUse)` are square `Pitch × Pitch` frames stacked, RGB (3 B/px) or grey (1 B/px) — the lightning bolt's,
  `S_lightning_lightmap_with_border.raw`, is 1200 B = 5·5·16·3. The original puts them in the list 0xD4EDB8 and
  `PSysLightMaps::AddDrawing` 0x6CA6E0 **stamps** them (fn_0086CFF0) onto the terrain's dynamic light texture.
  - In openblack `Particles/Creators/LightMap.cpp` stamps each atom into the land cells with `land_light::AddStamp`
    (fn_0086CFF0, centred, + (10, 0, 10)), and the mist creator stamps the storm's shadow map the same way.
  - `ParticleLightMapCreator` ctor 0x6A9CE0 defaults: Pitch 1, 1 frame, FrameRate 1, no animation, no jitter; its
    bitmap (creator +0x34) comes from `GetBitmap` 0x6A9D40.
  - `ParticleLightMap::DrawAt` 0x67B220: frame = DrawData +0x10, position = the atom's +0x24, alpha = DrawData alpha /
    255 ([0x9357AC]); the record goes to the list 0xD4EDB8 (0x67B35A), which fn_006CA660 empties after
    `PSysLightMaps::AddDrawing` 0x6CA6E0.
  - `UseRandJitter`: three `LocalFloatRand(RandJitter)` at every draw, the first to z, the second to y, the third to x
    (0x67B264..0x67B2A3).
- Fix along the way: `TextureBaseName` resolves the name of the `TextureFileName` with **the capitalisation the file has
  in `Data\Textures`** (the spell files write `S_Lightning.raw` and the file is `S_lightning.raw`), so
  now the sprites that were not drawn before also find their texture.

### `ParticleMistCreator` (`Particles/Creators/Mist.cpp`)

- Constructor 0x6AA380: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 frame,
  InitialScaleMin 1, Ratio 0. Properties 0x6B3C00 (Pitch 1..12, frames 1..32, InitialScaleMin and Ratio 0..5).
- `CreateParticleMist` 0x6AA610: atom scale = `RandomiseScale ? PSysFloatRand(InitialScaleMin, InitialScale) :
  InitialScale`. `CreateLH3DMist` 0x6AA5A0: an `LH3DObject` of type 7 (the `mist.l3d` dome), `k = Ratio` or
  `2.5 + LocalFloatRand(2.5)` if it is 0, and `+0x80 |= 2` (the effect branch).
- `RenderParticleMist::DrawAt` 0x67A670: size = the PSR's scale, with TakeRatioFromMatrix `k = M[1][1]/M[0][0]`, colour
  = the atom's × `[0xFA26A4]` (the base of the terrain light table, with alpha forced to 0xFF): per channel
  `(c × g) >> 8`. Here every frame `mist_atoms::SubmitFrame` (from `magic::Update`) passes each atom to
  the "map" `mists::Submit` (the same `DrawMist`). The base is read by
  `LandLightTable::Current().GetRawBase()` (the global copy of the last table).
- `GetBitmap` 0x6AA540 loads only with `LoadLightMap` (+0x7A): 1 byte per texel with `IsShadowMap` (+0x79), else 3.
  (inferred) the file's Pitch is used as written, outside the 1..12 property range: `S_SMClouds16` of
  SF_LightningStormPush is Pitch 16 (256 bytes, 16 × 16 × 1).
- A `Ratio` of NaN counts as 0 (fcomp 0; test ah, 0x40), so it also draws `LocalFloatRand(2.5) + 2.5`.
- `CreateParticleMist` 0x6AA683..0x6AA6AF: atom +0x110 = 1.0, +0x114 = 1, PlayAnim 0, LoopAnim = the creator's +0x0C:
  one frame that never steps.
- `RenderParticleMist::DrawAt` 0x67A774: in a Sorted effect ([0xC0215D] set) vt 0x100 = fn_007FA7F0 (own Z object at
  mist +0x38, NewZObject 0x7FA87B); otherwise vt 0x104 = fn_007FA790 (0x67A78C), drawn at once inside the effect's
  draw.
- **PSys randomness** (`game_random::psys`, engine-math.md "Random numbers"): each effect carries its NET_GAME_TYPE
  (+0xAC, `Effect(…, NetGameType)`); its step (`Effect::Step` = fn_00673340) sets the synchronised or the local stream and
  at the end the "0" one (0x67349B). Outside a step `PSysFloatRand`/`PSysRand` give 0 without drawing (what
  `fn_00673070` creates when the effect is born happens outside a step, as in the original). `RandR3` without the cap of
  64 attempts. `CreateParticle3DSprite`: the random scale (0x6AA1B4) is only for the sprites, the initial frame is
  `PSysRand(NumFrames)` (0x6AA1E4), the direction `PSysRand(0x100) > 0x80` (0x6AA231), `AtomCore::Create` draws
  `PSysRand(0x100)` (0x673816); the noise grid comes from seed 0 (inferred).
- Each PSys mist carries its own atlas counter (`Atom::mist`), as the original carries one per object. It starts at
  `Random(0,16) & 15` (0x7F95F8; the CRT's `rand()`, `game_random::crt`, the same as the map mists and the storm puffs, not the PSys series) and only
  advances if the mist is on screen (`mists::InView`). The k of each mist is `Ratio`, or `LocalFloatRand(2.5) + 2.5` when it is 0 (CreateLH3DMist 0x6AA5C0..0x6AA5EE, `Atom::mistK`), and the order of CreateParticleMist 0x6AA610 is the original's: the counter (CRT), the k (local GRand) and then the scale (`PSysFloatRand(min, max)` 0x6AA66D). The terrain shadow /
  light map of a mist with `TextureFileName` (the storm) is stamped into the land cells like the light maps.
- SF_Water: the cloud (183, 181, 255, 200), scale 0.2 (0.4 in PU), Ratio 2, and in its group 4 the rain cone
  `MSH_S_RAIN_CONE` (`ParticleMeshCreatorAnimTextured`, from 0.1 to −24 m, scale 0.7 / 1.4, sliding UV).
- **Fix in `PSys.cpp` (`MakeCreator`)**: the registered creators are looked up before requiring the name to start
  with "Particle" and end with "Creator"; `ParticleMeshCreatorAnimTextured` ends with "Textured", so the rain cone
  (and any other AnimTextured) was never created.

## Sound of the particles (`src/Audio/SpellSounds`, `src/Particles/Rules/Sound.cpp`)

The loops and hits of the miracles (teleport pool, tornado, shield, lightning bolts, fireballs...) are played by
the PSys atoms. What follows is verified in the exe and in `LHaudiodllR.dll`.

- **`SOUND_ACTION` property** (`SoundActionProperty::ReadProperty` 0x585A70, `src/Particles/SoundAction`):
  `<SOUND_*|NO_SOUND> LOOPING b ONLYONE b SOFTRELEASE b USESURFACE b`. The name is looked up in `Data\SoundAction.h`, which
  the game reads on the fly (fn_00585590, `LHParseFile::FindEnumVal` 0x7BE530); unknown or NO_SOUND → -1. It stores
  LOOPING (bit 0), SOFTRELEASE (bit 2) and USESURFACE (bit 3); ONLYONE is read and discarded. The code sets 0x22 (bit 1
  "delay by distance", bit 5 "ground height") on the thunderclaps.
- **PSysSoundAction** (0x18): action, then the slots passed to the bank (surface +4, size +8, alignment
  +0xC), FadeStep +0x10 and the flags +0x14. Default values (inline ctor, e.g. CreateRuleAnAtom 0x69F350):
  surface 1, **size 2**, alignment 2, FadeStep 0. Correction to the report: without SoundRadiusFP the size is 2, not 0
  (the result is the same in the "*" rows).
- **`AtomCore::StartSound` 0x6745D0**: global position of the atom (with 0x20, terrain height); with USESURFACE,
  `GSoundMap::GetSurfaceType`; alignment of the owning player (`GetDiscreteAlignmentValue` 0..6 → 1,1,2,2,2,3,3; still
  not linked to the player: the slot stays, no row of spells.sad looks at it). It creates a `PSysSound` (0x40, ctor
  fn_006D0F70) at the start of the atom's list (+0x2C) and in the global one 0xD4EE70. With bit 1 it does not play: +0x34 =
  distance to the camera / **347**. Otherwise, `GAudio::SamplePlayAnimEffect` 0x42A4B0 (mode 0) with the attributes
  {size, alignment, 1, surface, action} over spells.sad.
- **`GSoundMap::GetSurfaceType` 0x71D8E0** (`src/Audio/SoundMap`): 6 outside the 512-cell grid or without a block;
  7 if the cell is not land (`MapCoords::IsLand` 0x603720: bit 0x10 of the cell properties, `hasWater`); otherwise,
  the material's `surfaceSound` (1..8, other → 3). The animation sounds now use this same function (before,
  "height ≤ 0 → 7").
- **Bank** (`LHSamplePlayAnimEffect` 0x100146F0 of the DLL): `LHFindAttribRow` (most exact row, `src/Audio/AnimEffectBank`,
  alias of the core's `AnimEffectTable`: its tables are copied from those of the registered bank,
  shared with the animation sounds), a random sample from the list and **it does not play if the camera is farther away
  than the sample's maxDist** (S_TeleportPool: 170). The sample's playback mode (+0x274, bit 0x400) 2 =
  does nothing if it is already playing for that object; thus the loop can be "re-emitted" every turn without duplicating.
  The mode argument: **0 play, 1 `LHSampleStop`, 2 `LHSampleReleaseLoop`**.
  `GAudio::SamplePlayAnimEffect` also filters by help/cinema modes and the inside of the citadel: not ported.
- **Lifetime of the PSysSound** (fn_006D11A0, from `PSysGlobal::GameLoopEnd` 0x68F5B0 once per turn, with the turn's
  duration):
  - live atom, LOOPING: if the camera is closer than **1200**, it is re-emitted (mode 0).
  - live atom, delayed: if +0x34 > 0 the turn is subtracted from it; when it passes 0 (strictly negative) it plays. A delay
    that lands exactly on 0 never plays (it is kept).
  - dead atom (StopSound 0x674500 sets +0x38 = 0; the atom's destructor does the same): if it is still playing, with
    FadeStep it lowers the volume `vol − FadeStep` (0..127, no less than 0) every turn, and once (+0x3C) sends mode 2 with
    SOFTRELEASE (the loop finishes its pass) or 1 without it (cut). When it is no longer playing, it is deleted.
  - `PSysSound::Get3DSoundPos` 0x6D1000: the drawn position of the atom (+0xF4), with 0x20 at ground level.
  - The cull constant lbl_0093A460 is the 1200 (0x6D131E); with no channel left the sound is deleted through vt +4
    (0x6D12B5). The camera distance is fn_00442D50.
- `PSysSound`: vtable 0x93A468; the destructor 0x6D0FC0 takes it out of the global list (fn_006D1110).
  `AtomCore::GetSoundOfAction` 0x674550 gives the newest sound of that action on the atom; `AtomCore::StopAllSounds`
  0x6747E0; NO_SOUND plays nothing (0x6745D8).
- **Rules** (`Rules/Sound.cpp`): `StartStopSoundOnCondition` 0x69DC40 (SoundCondition true or none → plays if the
  atom does not already have that action; false → stops it with FadeStep), `AddSoundToAtom` 0x69DCA0 (once per atom on
  reaching Delay and with the condition; StopOtherSoundsFirst; the camera shake `LH3DCameraChecker::Create` is not ported),
  `RemoveSoundFromAtom` 0x69DDD0 (once: stops that action with FadeStep).
- **Who plays on creation**: CreateRuleAnAtom (SoundOfCreate, size classes from `CreateRuleAnAtom::ModifyAtomCollection`
  0x69F410; with SoundRadiusFP: < Small 200 → 3, < Medium 500 → 2,
  otherwise 1), CreateRuleSphere (only the first atom), EmitterRuleConical and UR_WillowWisp (SoundEmission on each atom).
  For the fireball: `SizeFromThrow` (> 0.6 → 1, > 0.3 → 2, otherwise 3) and `SizeFromImpactSpeed` of
  UpdateRuleGravityWithFloor fn_006A1630 (< Medium → 3, < Large → 2, otherwise 1; before that it requires alpha ≥
  MinAlpha, |v| ≥ Small and that the atom does not already have that action).
- In openblack: `audio::spell_sounds::ProcessTurn` goes in slot 11, inside `magic::ProcessSpellParticlesEndOfLoop` (was `ProcessPSysGameLoopEnd`)
  (`MagicLoop.cpp`), which `Game::GameLogicLoop` calls at 0x54E688, after `psys::manager::ProcessTurn` (slot 9),
  the fireflies and the physics, and before `GScript::Process`. Thus the sound sees this turn's atoms, as in the
  original, and in the original's order. The bank loader in
  `Game.cpp` stopped reading a
  .sad at the first empty sample: spells.sad has an empty one at 31, so 32..88 were missing (teleport
  pool, lightning bolts, fireworks...). Now it skips it and continues.
- Test: `OPENBLACK_PSYS_SOUND_TRACE=1 OPENBLACK_TEST_PSYS="SF_TeleportVortex,1478,2129,0,1,5"` in Land1 (initial
  camera at 157 from the point, within the 170 of S_TeleportPool): "start SOUND_SPELL_TELEPORT_POOL (76) ... ->
  spells.sad/39 (S_TeleportPool.wav) looping", at 5 s "release loop" and half a second later "deleted" (it finishes its
  pass). `test_spell_sounds` checks the enum, the flags, the sizes and, with `OPENBLACK_GAME_PATH`, the real
  rows of spells.sad.
- Not ported: the owner's alignment, the camera shake, the game-state filters, the finite repetitions
  (loop > 0 is played once) and the DLL's global distance cap (+0x44 of the audio system).

## The PSys in the world: format, step, drawing and water rules

Comes from rendering.md (the part of the PSys that the render had). **Faithful** except for what is stated as not ported.

- Files: `Data\Spells\ZSpellFiles\SF_X_txt.zzz` (u32 size + zlib) with editor text: header
  `BEGINPROPERTIES` (DeleteOnCloseDown, Hierarchies[25], InitiallyCreated[25], MaxSpellAge) and blocks
  `BEGINCLASS <Class> <Name>`. A loose `.txt` with the same name takes priority (`LHLoadData`).
- Spell file cache: `PSysFileData` 0xD4EBC0; the loose `.txt` is read first through `LHLoadData` 0x7BCDF0, else
  `<name>_txt.zzz`.
- Model: each modifier has a `Group` (0..24) and a `Condition`; a *collection* is a live instance of a group;
  `InitiallyCreated` creates the roots at the origin; `NextGroups` gives each new atom its subcollections; `Hierarchies`
  puts the child atoms in the parent's local frame. Nothing moves by itself: only the rules.
- Per-turn step (dt = 0.1 s) with the previous and current draw state, interpolated when drawing with the turn fraction.
  The atom's frame is stored by fn_00673EA0 in [0, 2N) together with the previous one, and it is only moved with PlayAnim
  (`Atom::playAnim`, +0x118; without it neither step nor wrap-around); fn_00679920 interpolates the frame **number**
  and truncates it (a step, without blending two frames): `frame_anim::ParticleFrameAdvance` /
  `ParticleFrameLerp` / `ParticleFrameIndex` (were `PSysFrame*`), see [rendering-objects.md](rendering-objects.md#frame-animated-textures).
  End: without atoms nor creation rules, or age > MaxSpellAge; `CloseDown` activates `TrueOnCloseDown`, releases the
  `RemoveOnCloseDown` rules and deletes immediately if `DeleteOnCloseDown`.
- Per-atom conditions: `Modifier::ModifyCollection` 0x675B10 applies an atom rule to each atom whose atom-level
  condition holds; the alpha condition 0x67DC80 is "alpha > AlphaValue".
- fn_00673290: a modifier with flag 2 (+0x10) and without 4 keeps an effect without atoms alive while it is attached
  and the effect is not closing.
- `AtomCore::Create` 0x6737F0; `SetAtomAge` fn_00673CE0 (birth = time − x).
- The common creator part fn_006A85E0 sets the atom's +0x74 = InitialScale (0x6A8761). Only `CreateParticle3DSprite`
  draws a random scale: `RandomiseScale` (+0x7A) ? (PSysFloatRand(0.7) + 0.3) × InitialScale (0x6AA1B4..0x6AA1D6); the
  other creators do not (the mist and the animated meshes draw their own).
- Sprite frame: `RandomiseInitFrame` (+0x7B) ? PSysRand(NumFrames +0x4C) : InitFrame (+0x70) (0x6AA1D9..0x6AA1F5);
  `RandomiseFrameDirection` (+0x7F) reverses the rate. The SetAngleY(PSysFloatRand(2π)) of +0x80 (0x6AA1FA..0x6AA213)
  never runs: +0x80 is 0 from the ctor 0x6A9F66 and no property sets it (DefineProperties 0x6B4380).
- `DrawOffsetLT` (0x28 bytes, ctor 0x6C75A0) is set with `AtomCore::SetDrawOffset` 0x673AF0 into the atom's +0x124;
  another `DrawOffset` kind, fn_006C7840, draws an atom relative to the hand until it decays (used by UR_WillowWisp's
  `DoDrawOffsets` +0x80 and the fireball's local cast; not ported).
- DrawData +0 is the atom it was taken from (0x679C0C); `PSysManager::AddDrawing` keeps the draw fraction at +0xB0
  (0x6797D4) and passes it as DrawData +0x14.
- The three draw entries are in `GJPSysInterface`'s vtable 0x8FA8A0.
- `Particle3DSprite::DrawAt`: the sprite's size is the scale, at least 0.0001 (0x67AEA4..0x67AEBE).
- Every owner list of effects (GParticleContainer fn_0063E0F0 0x63E151, Spell 0x71FC4C, MapShield 0x72C0AC, the seed
  graphics 0x726E70, the vortices 0x5FEA49) puts a new effect at the head and walks from the head: the newest is
  stepped and drawn first.
- The spell dispensers' effect and the flying flock's cast are stepped by their owner's Draw with the frame's ms
  (`SpellDispenser::Draw` 0x722940, 0x7229FE; `FlockFlying::Draw` 0x724100, 0x7241D1); openblack steps them once a
  turn (pending).
- Drawing: **three paths** depending on who draws the effect (`psys::DrawPath`, `PSysManager::SetDrawPath`). **Sorted** = `Draw_(t, 1)`
  (fn_00679840, +0xAE = 1 at 0x67984E): the effect has no Z-sorter object; each sprite (0x840C70), each mesh,
  the opaque ones too (fn_00679F60, key its translation), each mist (fn_007FA7F0, key mist+0x38) and each chain
  (fn_0067B380, key the joint n/2) enter the queue on their own; the ZR_SurfRevol surfaces are drawn
  immediately (0x67CBA0). It is used by `Spell::Draw` 0x720441, the storm 0x72DCB7, the shield 0x72D160, the teleport
  0x5FCDC5, the dispenser 0x722A13, the flock 0x72420D, the hand utilities, `TownCentre::DrawPSys` 0x69BF19:
  it is the default path. **Queued** = `AddDrawing` 0x6797D0 (+0xAE = 0 at 0x6797DE): **one** object at `GetOrigin`
  and inside it all the atoms in the order of fn_006798B0 (atoms of the collection, its chain, child collections); only
  the seed in the ball or the icon (0x51A2CA) and the containers with `GSpotVisualInfo` +0x4C `SingleZSort` = 1
  (0x63E190, 0x63E26A; all the info.dat entries with a file have it). **Immediate** = `Draw_(t, 0)`: everything
  right where the call is; the effect in the hand (`CHand::DrawSpellInHand` 0x46E76A, inside the hand
  object). openblack: `manager::CollectSorted` / `CollectQueued` / `HandEffects`; until the renderer uses them
  (`manager::k_DrawByPath`) everything is still drawn as one object per effect at its origin. Sprites from
  `S_SpriteSheet{1,2,3}` (8×8 cells of 32 px, cell = (FileOffset + frame) & 63), screen-aligned quad
  with rotation atan2(M[0][2], M[0][0]) or XZ plane (`SetHorozontal`); additive mode 13 (102 of 137) or 6, no Z write
  except `MaterialUpdateZBuffer`; no light nor haze except `UseLandscapeColor` (not done).
- Scripts: `SPECIAL_EFFECT_POSITION` / `_OBJECT` (CHL 52/53) → `GParticleContainer` with the `GSpotVisualInfo` table
  (50 entries → PARTICLE_TYPE → file); duration in seconds (−1 forever, 0 the table's lifetime); it stays where the
  object was when it was made and closes if the object disappears; returns an object that the script can delete.
- `SPECIAL_EFFECT_POSITION` (`GScript::SpecialEffectPosition` 0x70C330) →
  `GParticleContainer::CreateSpotVisualWithSpecifiedDuration` 0x63E580. `SPECIAL_EFFECT_OBJECT` (0x70C460):
  `CreateSpotVisualWithSpecifiedDuration(obj +0x14, type, 1.0, duration, obj)` (0x70C50B).
- The container does not follow its owner: `Process` 0x63E33D..0x63E349 puts the effect at `GetLHPoint(+0x14)`, the
  container's own MapCoords with the y read from the land again, and nothing moves +0x14; the owner (+0x28) is only
  tested for availability (its loss still ends the effect).
- Spot visuals: the `GSpotVisualInfo` table is 0xD44470. `GParticleContainer::Create` 0x63E508 → fn_0063E410 creates the
  effect with NET_GAME_TYPE 1 (push 1 at 0x63E436), synchronised, at the LHPoint of the MapCoords it keeps at +0x14
  (0x63E42F..0x63E474).
- The container's draw flag +0x38 = (`SingleZSort` == 1) (0x63E190..0x63E198); fn_0063E240 reads it to draw with
  `AddDrawing` (0x63E26A) or `Draw_(1)` (0x63E277).
- Durations: `CreateSpotVisual` 0x63E540 passes the entry's own life (+0x44); `CreateSpotVisualWithSpecifiedDuration`
  0x63E580 passes its int straight to `Create` 0x63E4B0 → fn_0063E410 (+0x30, 0x63E489). `Process` 0x63E2A0..0x63E2B1:
  below 0 forever, else decremented and closed at ≤ 0, so 0 closes it on its first Process.
- Container list: g_game +0x205BCC (next +0x3C), a new one at the head (fn_0063E0F0 0x63E151..0x63E168).
  `ProcessParticleContainers` 0x63E090 walks it from the head (the newest), taking the next one first (0x63E0A3); each
  `Process` 0x63E280 closes its effect when its owner has gone or its turns are over, sets its origin and steps it
  (`Process_` 0x6736B0); a result of 5 deletes the container (`ToBeDeleted` 0x63E1D0) with its effect.
- openblack: `src/Particles/PSysFile` (reader), `PSys` (collections, atoms, rules: CreateRuleAnAtom/Sphere, emitters
  Simple/Disk/Conical, UR_WillowWisp, deletion rules, AR_FadeAlpha/FadeCollectionAlpha/FadeOutOnceConditionTrue,
  UR_ChangeScale, SetScale, SetAtomAlpha, UpdateRuleGravity, UR_UpdatePosnFromVelocity, UR_GustyWind (with its own
  noise: VLNoise3To1 not ported), UpdateRuleRotatePrincipalAxis, FollowOrigin, UR_FollowParent, ForceConstant*,
  UR_SphereSurfaceTracer, UR_OrientSpriteWithRandomAngle; conditions and float providers), `PSysManager`
  (effects, script containers, test hook) and `Graphics/RendererParticles.cpp` (was `RendererPSys.cpp`). The unported classes are logged
  once ("not ported yet") and do nothing.
- **`UpdateRuleGravityWithFloor`** (`Particles/Rules/Fireball.cpp`, a single class shared with the miracles; ctor 0x6A1510, `ModifyAtomCollection` 0x6A1880).
  Default values of the ctor: MaxSpeed 100, Gravity 10, Damping 0, WindMagnification 100, UseWind 1, bounces 0.5/0.5,
  GroundDrag 0, ImpactSpeed 5/20/40, MinAlphaForImpactSoundOrRipple 60, ripple distance 2 (+0x40) and ripple enabled
  (+0x71 = 1), with no property. Per atom, without the per-atom `Condition`:
  - damp = UseDamping and (not DisableDampingForNonHuman or `IsHumanPlayerCasting` 0x673580, which is 0 without a `Spell`);
    wind likewise with UseWind / DisableWindForNonHuman. With wind: v += (wind·WindMagnification·0.1 − v)·Damping·dt
    (wind = `fn_00771B10` = `GClimate::GetWeather(p, 1)`: (int8 x/8, 0, int8 z/8); here `weather::GetWindAt(p, true)` from ECS/Weather); otherwise,
    with damping: v ·= 1 − dt·Damping.
  - v is stored and the atom moves **before** gravity. Ground = `GetAltitude(x, z)`; lowest point = global y
    (`RenderParticle::GetLowestPoint` 0x6C79B0; the mesh one, `Particle3DObj` 0x6C7AE0, is not there because there are no
    mesh particles). Above: v.y −= clamp(v.y + MaxSpeed, 0, 1)·Gravity·atom gravity·dt.
  - Below: it is raised to the ground; d = n·v with the terrain normal; if d < 0, impact (`fn_006A1630`) and bounce:
    vn = n·d, vt = v − vn, drag m = min(dt·GroundDrag, |vt|) in the direction of vt (if |vt|² < 1e-4 the direction
    is +x), v = vt·DampingHorozontalBounce·surface − vn·DampingVerticalBounce. Surface (UseSurfaceForBounce):
    table 0x937574 by `GetSurfaceType` = 1,1,1,1,1,1,**0.2** (deep water),**0.2** (shallow),1,1,25.
  - Impact `fn_006A1630`: nothing if alpha < MinAlpha, if ImpactSound is NO_SOUND (−1), if |d| < ImpactSpeedSmall, if the
    atom's sound is still playing or if ImpactSoundCondition fails; level 3/2/1 according to ImpactSpeedMedium/Large; and a
    ripple on the water. Consequence: the pieces of `SF_ExplodeObject` (NO_SOUND) **never** make a ripple; only the fireball
    (`SF_FireBallThrow*`, SOUND_SPELL_FIREBALL_HIT) makes one. The atoms of this engine do not play sounds yet
    (`AtomCore::StartSound` 0x6745D0), so the "while it is playing" wait does not apply. `CheckShieldDeflections`
    (creature shields) is not there. Unit test `test_psys_water`.
  - Property layout (UpdateRuleGravity DefineProperties 0x6AC1B0): +0x20 MaxSpeed, +0x24 Gravity, +0x28 Damping, +0x2C
    WindMagnification, +0x30 UseDamping, +0x31 UseWind, +0x32 / +0x33 Disable{Wind,Damping}ForNonHuman; its own +0x34
    DampingHorozontalBounce, +0x38 DampingVerticalBounce, +0x3C GroundDrag, +0x44 ImpactSound, +0x5C its condition,
    +0x60..+0x68 ImpactSpeed{Small,Medium,Large}, +0x6C MinAlphaForImpactSoundOrRipple, +0x70 UseSurfaceForBounce, +0x72
    CheckShieldDeflections.
  - The ripple (`AtomDataRipple`, fn_006A1630 from 0x6A1CC2): only where `MapCoords::IsWater`, and only when the atom is
    farther than the rule's +0x40 in x, z from its last ripple (`minDistance² < dx² + dz²`, 0x6A176F..0x6A179B), which
    then moves there (AtomDataRipple +0x20, (0, 0, 0) at creation). The ring: at the atom, growth 4 × the atom's
    `Object::GetRadius` (0x6A17CE), cell 0x30, colour 0xFFFFFFFF, angle 0, rate and aspect 1; nothing when the pool is
    full.
- **`UR_Explosion`** (`Particles/Rules/Explosion.cpp`, [miracles.md](miracles.md); the rings here are
  `Particles/PSysWaterRings` `AddExplosionRings`, the only implementation; ctor 0x67E090, `ModifyAtomCollection` 0x67ECE0, `InitCollection`
  0x67E200), in `SF_BeamExplosionSingle/Many/Loads`. By default InitialDelay 3.5, SmokeDelay 3, BeamDelay 0. Point =
  `GetCurrentParentPos` (the +0x80 of the parent atom or the origin) with y = ground altitude. If the effect closes, it
  closes the lightning container. With collection age > InitialDelay: water rings (above) or scorching; > BeamDelay:
  visual spot BEAM_EXPLOSION_FX (magnitude 1, 60 turns); > SmokeDelay: **SMOKE on dry land, STEAM over water**
  (`IsDryLand`), magnitude 8, 4 s. The 3rd argument of `CreateSpotVisualWithSpecifiedDuration` is the effect's
  magnitude (`GJPSysInterface::Create` 0x68F3A1 `SetScale`). The damage to objects, the `SpellEvent` 2 and the shield are
  done by the miracle's side ([miracles.md](miracles.md)); the ground mark (`fn_008251C0`: a morphable mesh 0x251 of scale 8, not a shadow; `TemporaryShadow` is
  `fn_00825090`) is in `ecs/GroundMarks` ([rendering-objects.md](rendering-objects.md#meshes-stuck-to-the-ground-land_morph));
  not ported: the mesh debris.
  The ring growth constant is [0x9357D8] = 10 (× 0.5 at 0x67E39F, × 0.7 at 0x67E43A, × 1 at 0x67E4D1);
  `MapCoords::IsDryLand` there means altitude ≥ 4, and each ring is skipped when the 1024 slots are full.
  Test: `OPENBLACK_TEST_PSYS="SF_BeamExplosionSingle,1464,2016,0,1"`, camera `1452,14,2002,1464,0,2016`, capture at
  frame 272 of 300 (rings) or 360 of 400 (steam); `OPENBLACK_PSYS_TRACE=1` writes the explosion.
- Test: `OPENBLACK_TEST_PSYS="SF_Bonfire,1790,2630,0,1"` with the camera `1775,45,2600,1790,30,2630`, `-n 5000`
  (bonfire with flames and smoke); `OPENBLACK_PSYS_TRACE=1` writes the atoms and age of each effect every 20 turns.
- **Beliefs over the town centre** (`src/Particles/TownBelief.cpp`): each functional centre has TOWN_BELIEF (SF_TownBelief, `UR_TownCentreBelief` 0x69BF30), which advances once per frame with dt = 0.1 s. One symbol per player with belief: the first one (rank 0) still 2 units above the top of the totem; the others orbit (radius and speed by belief, at 2.5 per height rank) and the second one fights (flashes). It is drawn with two glows from S_SpriteSheet3 (player colour and white spinning) and the symbol. The human's symbol is the "player symbol" cell of the profile (registry; 0 without it, as in this installation) copied from ChooseSymbol (PlayerSymbol::OpenOnce 0x5DE2F0); the rivals use .cps images (not done). Base: the totem (`components::TotemStatue` of fields): x/z of the pedestal, y = baseY + height of the icon mesh × scale + 2. The owner's SpellColumn column is missing.
  - AtomData +0x24 / +0x28 / +0x2C are a1, a2 and the phase; a1 and a2 start at PSysFloatRand(2π) each, a1 first
    (0x69C0B8..0x69C0D5), the phase at 0. +0x30 is the wait before the next fight, +0x34 the fight timer, +0x38 its
    length, all 0 from the ctor (0x69C6C4..0x69C6D6).
  - The fight: only the second symbol fights (0x69C388..0x69C3A7); its wait counts down by dt (0x69C473); then the
    timer += dt and, past the length, the next wait and fight length are drawn together, wait first
    (0x69C3C3..0x69C424), timer back to 0. f = timer / length (0 for ≤ 0 or NaN, at most 1), fight = 1 − (2f − 1)²,
    which lowers the symbol in the position of the same step (0x69C537); speed × ((SpeedUpDuringFight − 1)·fight + 1)
    (0x69C47F..0x69C496).
  - The motion (0x69C4A5..0x69C510): phase += dt·PhaseSpeed, a1 += ((cos(phase) + 1)·0.25 + 0.5)·speed·dt·0.846,
    a2 += speed·dt, each fmod 2π. The exe keeps each sum on the x87 stack into `__CIfmod` and takes fcos of the
    unrounded result (0x69C4BF); openblack uses float sums (approximate).
  - The ranks (fn_0073BB10): a player ranks above another with more belief, or as much and a higher number; the belief
    is the town's GBelief +0x8. The candidates are slots 0..6 of `GetNextActivePlayer` 0x5508D0 (0x69BFF4; the neutral
    one has no symbol), but the rank counts all 8 slots, the neutral one included (0x73BC20..0x73BC24).
  - The symbols: `PlayerSymbol::CreateFinalTextureSymbols` 0x5DEF00 makes them; ChooseSymbol is 4 × 4 cells of 64 × 64;
    the computer players get the Lethis / Kazarr / Nemesis `.cps` greyscale images. Each symbol is its own Z object
    (`PlayerSymbolSprite::AddDrawing` 0x69D790).
  - Removal: a centre leaves the list g_game +0x205CFC in its `ToBeDeleted` (0x743B68..0x743BAD); fn_0069BCA0 (from
    `TownCentre::ToBeDeleted` 0x743B45) deletes the centre's effect (+0xC8) and clears +0xC8.

## Rule index

Each PSys class that the miracles use, with its address and where it is described. Those in `src/Particles/Rules/` are
registered in `PSysRegistry.cpp`; the rest remain "not ported yet".

| Class | Address | File | Where |
|---|---|---|---|
| `CreateRuleAnAtom`, `CreateRuleSphere`, `EmitterRuleConical` | ctor 0x69F350 and MAC 0x69F410 (CreateRuleAnAtom: one atom at the spawn point + offset, once, with its SoundOfCreate); 0x69E160 (CreateRuleSphere: NumAtoms atoms inside a ball, once); 0x6A6A60 (EmitterRuleConical) | — | [Sound of the particles](particles.md#sound-of-the-particles-srcaudiospellsounds-srcparticlesrulessoundcpp) |
| `StartStopSoundOnCondition`, `AddSoundToAtom`, `RemoveSoundFromAtom` | 0x69DC40, 0x69DCA0, 0x69DDD0 | `Rules/Sound.cpp` | [Sound of the particles](particles.md#sound-of-the-particles-srcaudiospellsounds-srcparticlesrulessoundcpp) |
| `StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, `LandscapeCollide` (SendEvent) | `LandscapeCollide` 0x67D6E0 | `SpellLink.h` | [PSys linked to the spell](particles.md#psys-linked-to-the-spell-particlesspelllinkh) |
| `MagnitudeFloatProvider` | UpdateParams 0x69DA90 | — | [The size of the fireball thrown with the hand](magic.md#the-size-of-the-fireball-cast-with-the-hand) |
| `UR_FollowLocalHand`, `UR_FollowCastPosn` | 0x69A6A0, 0x69FE30; `UR_FollowLocalHand` DefineProperties 0x6B1B90 (`UseGraspPos` +0x20 is read and not used) | `Rules/HandFollow.cpp` | [The hand](magic.md#the-hand-handmagicfxcpp-phandfx-and-the-effect-in-the-hand) |
| `ZR_ChainGesture`, `CreateRuleMakeChain` | 0x68A080, 0x69FD10; DefineProperties 0x6B0170, NumAtoms 0x6B0B60 | `Rules/Gesture.cpp` | [Utility effects](magic.md#utility-effects-particlesutilitycpp-psysutilitypsys-0xd4e0e8) |
| `UR_GesturingRecognised` | 0x6884F0 / 0x688910; DefineProperties 0x6B0560 | — | [Utility effects](magic.md#utility-effects-particlesutilitycpp-psysutilitypsys-0xd4e0e8) |
| `UR_HandSprinkle`, `UR_WillowWisp`, `AppearanceRuleTumble` | 0x6A0220, 0x6A6D20, 0x6A6200; DefineProperties 0x6B19D0 (UR_HandSprinkle: `FracToCloseDownOn` +0x38 and `KeyPoints` +0x44 are read and not used by the rule), 0x6ABC10 (AppearanceRuleTumble) | `Rules/Sprinkle.cpp` (UR_HandSprinkle) | [Food and wood](miracles.md#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource) |
| `UR_HealSpellChakra` | 0x6A0B20 | `Rules/Heal.cpp` | [`UR_HealSpellChakra` 0x6A0B20](miracles.md#ur_healspellchakra-0x6a0b20-particlesruleshealcpp) |
| `CreateRuleFusedSphericalExplode` | 0x69F610 | `Rules/Heal.cpp` | [`CreateRuleFusedSphericalExplode` 0x69F610](miracles.md#createrulefusedsphericalexplode-0x69f610) |
| `UR_HealInHand` | 0x6A0F40 | `Rules/Heal.cpp` | [`UR_HealInHand` 0x6A0F40](miracles.md#ur_healinhand-0x6a0f40) |
| `UR_KPStretchHeight`, `UR_KPMoveAtoms` | 0x6A50C0, 0x6A60B0; ctors 0x6A4F50, 0x6A5F40 | `Rules/KeyPoints.cpp` | [The other missing classes](miracles.md#the-other-missing-classes) |
| `UR_FollowTargets`, `UR_Flocking`, `EventConditionAtomNearVillagers` | 0x6A04B0, 0x683580, 0x67D8E0; `UR_FollowTargets` AtomData 0x28 bytes, vtable 0x8FD3EC, ctor 0x560210 (+0x24 the target, +0x20 the game turn it was last seen, unused) | `Rules/Flock.cpp` | [The particles](miracles.md#the-particles-particlesrulesflockcpp-faithful) |
| `UR_ForestPath`, `ParticleGoodEvilCreator` | 0x6A3770, 0x6AAA00; `UR_ForestPath` ctor 0x6A35D0, DefineProperties 0x6AEE60, AtomData 0x30 bytes ctor 0x55FAC0 | `Rules/Forest.cpp`, `PSys.h` | [The other missing classes](miracles.md#the-other-missing-classes) |
| `ParticleAnimWithCameraCreator`, `ParticleAnimCreator` | —; fn_006A97F0, DrawAt 0x67A8E0 | not ported; `Creators/Mesh.cpp` | [The particle meshes](#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome), [Forest](miracles.md#forest-magicspellsspellforest-magicobjectsmagictree-ecstrees) |
| `UpdateRuleGravityWithFloor`, `CreateWithInitialDirection`, `AttatchFireBallToAtom`, `SetAtomHasBeenDeflected`, `UR_SideSpin`, `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`, `UR_Trail` | 0x6A1880, 0x69E950, 0x682FD0, 0x6A26C0; `UR_SideSpin` 0x69E300, `AR_FadeAlphaWithHeightAboveLandscape` 0x6A4DE0, `AddSubCollectionsToAtom` 0x69F220, `UR_Trail` MAC 0x6A40F0; `CreateWithInitialDirection` DefineProperties 0x6B1230 | `Rules/Fireball.cpp` | [Fireball](miracles.md#fireball-magic_type-1-3-seed-2-fire) |
| `UR_OrientSpriteWithVelocity` | 0x69A790; DefineProperties 0x6AC610 (+0x20 SmoothFactor, +0x24 ProportionDefault); AtomData 0x38 bytes, ctor 0x560DC0 (+0x20 first, +0x24 smoothed velocity, +0x30 its rate) | `Rules/Orient.cpp` | [The other missing classes](miracles.md#the-other-missing-classes) |
| `UR_Lightning`, `UR_LightningStrike` | 0x6914C0, 0x6937A0; `UR_Lightning` props 0x6B2500, ctor defaults 0x6900B0, CollectionData ctor 0x68FE20; file PSysLightning.cpp 0x68FD50-0x6941B0 | `Rules/Lightning.cpp` | [Lightning bolt](miracles.md#lightning-magic_type-4-6-seed-6-lightning_bolt-particlesruleslightningcpp) |
| `LightningForkFlicker` | 0x6B24D0 | not ported | [Lightning bolt](miracles.md#lightning-magic_type-4-6-seed-6-lightning_bolt-particlesruleslightningcpp) |
| `UR_AddDefensiveSphere`, `UpdateRuleShieldSpark`, `UR_InitialSpin`, `UR_VapourEndEffect`, `SetCollectionAlpha`, `UR_AtomsAtEPTarget`, `CheckShieldDeflections` | 0x6A2A60, 0x6A2BF0, 0x69E490, 0x6A39E0, 0x6A2720, 0x69A960, 0x6A2570; DefineProperties: `UR_InitialSpin` 0x6B1520, `UR_AddDefensiveSphere` 0x6AE8C0, `UpdateRuleShieldSpark` 0x6AEA00, `CheckShieldDeflections` 0x6AF540 | `Rules/Shield.cpp` | [The particles](miracles.md#the-particles-particlesrulesshieldcpp) |
| `UR_SphereSurfaceTracer` | 0x6A32B0; DefineProperties 0x6AEBB0: +0x20 ThetaSpeed, +0x24 PhiSpeed, +0x28 SphereRadius, +0x2C ScaleSphereRadius, +0x30 ScaleAlpha, +0x34..0x3C ScaleX/Y/Z, +0x40 Alpha, +0x44 OrientToSurface | `PSys.cpp` | [The particles](miracles.md#the-particles-particlesrulesshieldcpp) |
| `ZR_SurfRevol`, `UR_ChangeScale` | 0x686370; profiles TestDisk 0x685860, TestFunnel 0x6868E0, TestFunnelSpout 0x686940, TestFunnelParab 0x686910; ctor 0x686200; DefineProperties 0x6AF6F0 | `Rules/SurfRevol` | [SF_TeleportVortex and ZR_SurfRevol](miracles.md#sf_teleportvortex-and-zr_surfrevol-srcparticlesrulessurfrevol-srcgraphicsrendererrevolvedsurfacecpp) |
| `UR_Explosion` | 0x67E200, 0x67ECE0, 0x67E900 | `Rules/Explosion.cpp` | [`UR_Explosion`](miracles.md#ur_explosion-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900) |
| `SpreadingDiskEmitter`, `DiskEmitter`, `SetPSysCloseDown`, `EventConditionCollectionDelay` | 0x6A6610, 0x6A64D0, 0x6A26D0; `SetPSysCloseDown` DefineProperties 0x6ACC60 (base properties only: it closes the effect for every atom that passes the condition) | — | [The files](miracles.md#the-files-faithful) |
| `UR_MoveAtom`, `UR_ChangeScaleXYZ` | 0x6A5E50, 0x6A5240 | — | [The visible lightning bolt](miracles.md#the-visible-beam-sf_beamexplosionfx-pt-138) |
| `UR_ExplodeObject`, `UR_ExplodeObject2`, `ER_EmitFromParentAtom`, `CreateRule_GameObjectRef` | 0x6814E0, 0x681560 | not ported | [Not ported / pending](miracles.md#not-ported--pending) |
| `UR_CloudMoverNew`, `UR_CloudGather` | 0x6D41C0, 0x6D4A70; props 0x6ACD10, 0x6ACD70; file PSysTornado.cpp 0x6D15D0-0x6D6160 | `Rules/Storm` | [The cores and the clouds](miracles.md#the-cores-and-the-clouds-ur_cloudmovernew-0x6d41c0-ur_cloudgather-0x6d4a70) |
| `UR_Tornado`, `UR_FollowParent` | 0x6D18B0; props 0x6AD550 | `Rules/Storm` | [The tornado](miracles.md#the-tornado-ur_tornado-0x6d18b0-ctor-0x6d1680) |
| `UR_StormCast` | 0x6D59B0; props 0x6AD3E0, ctor 0x6BCFB0 | `Rules/Storm` | [The whirlwind](miracles.md#the-whirlwind-ur_stormcast-0x6d59b0-sf_stormcast) |
| `ParticleMeshCreator`, `ParticleMeshCreatorAnimTextured` | ctor 0x6A8960; 0x6A8B00, 0x6A8DA0 | `Creators/Mesh.cpp` | [The particle meshes](particles.md#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome) |
| `ParticleChainCreator`, `ParticleLightMapCreator` | 0x6AA900, 0x6A9D80 | `Creators/{Chain,LightMap}.cpp` | [Chains and light maps](particles.md#chains-and-light-maps-particlescreatorschainlightmapcpp-graphicsrendererchaincpp) |
| `ParticleMistCreator` | ctor 0x6AA380 | `Creators/Mist.cpp` | [`ParticleMistCreator`](particles.md#particlemistcreator-particlescreatorsmistcpp) |
| `ParticlePointCreator`, `ParticleSpriteCreator` | — | already there | [Not ported / not verified](miracles.md#not-ported--not-verified-heal) |
| `EmitterRule::ShouldEmit`, `EmitterRuleSimple` | 0x6A63A0, 0x6A6700 | — | Simple and Disk make one atom per step; Conical loops only with `AllowMultipleEmits` (0x6A6BF4); SpreadingDiskEmitter always loops (0x6A66DC) |
| `EventAlways` | 0x69DBB0 | `Rules/Fireball.cpp` | [PSys linked to the spell](#psys-linked-to-the-spell-particlesspelllinkh) |
| `AR_FadeAlpha`, `UR_ChangeScale` | 0x6A4C40, 0x6A4EE0 | — | window rules: lerp inside [start, stop], the stop value on the first step past stop, nothing otherwise |
| `AR_FadeOutOnceConditionTrue` | 0x6A7CF0 | — | once ConditionStartFadeOut (+0x24) holds (or there is none) age, alpha and scale are kept and it fades out over TimeToFadeOut from there |
| `UpdateRuleGravity` | 0x6A1410 | — | — |
| `UR_GustyWind` | 0x6A7500 (noise `VLNoise3To1` 0x590CA0, not ported) | — | — |
| `UR_OrientSpriteWithRandomAngle` | 0x6A2100 (DefineProperties 0x6AC660: +0x20 RandomAngle, +0x24 DefaultAngle) | — | [rendering-objects.md](rendering-objects.md) |
| `EC_DeflectionInAtomsHierarchy`, `EC_DeflectionInCollectionsHierarchy`, `EventConditionAtomHasBeenDeflected` | 0x67DAF0, 0x67DB20, 0x67DB70 | `Rules/Fireball.cpp` | the atom or a parent deflected / a collection's parent atom and theirs / the atom's flag bit 3 |
| `EventConditionAtomCloseWater`, `EventConditionFireBallSteam` | 0x67DCA0, 0x67DDD0 | `Rules/Fireball.cpp` | not deflected, below CutOffHeight (+0x10) above the land and over water (or off the land) / not deflected and raining or snowing there (`GClimate::GetMaxRainingOrSnowing`) |

- `UR_WillowWisp` 0x6A6D20 emits atoms along the path of the parent atom (the origin without one). CollectionData 0x38
  bytes: +0x20 the last position, +0x2C the amount emitted so far, +0x30 the atoms made, +0x34 "first". The rate is
  `MaxAtoms / DieAge` (fild +0x4C, fdiv +0x40, 0x6A6F08, no guard); `MaxAtoms` is no cap. `SmoothingValue` and
  `AccelerationForCast` are not read in W120. `EmitConditionOfParent` is tested on the parent atom (none: nothing is
  emitted, 0x6A6F28..0x6A6F46). Each atom is placed along last → P (0x6A7075) and aged by its fraction of dt with
  fn_00673CE0 (0x6A70CC..0x6A70F7). (inferred) `EmitDueToMovingMaxRate` 0 means no cap (0x6A6FB6).
- `AppearanceRuleTumble` turns by |v|·TumbleSpeed·dt (at most MaxTumbleSpeed with RestrictMaxRotation) about z when
  |v.z| < |v.x| (0x6A6293..0x6A62AC), else about x (0x6A6317..0x6A6330).

## Test hooks

- `OPENBLACK_TEST_PSYS`, `OPENBLACK_PSYS_SOUND_TRACE` and `OPENBLACK_PSYS_CHAIN_TRACE`, in
  [openblack-internals.md](openblack-internals.md#debug-environment-variables); `test_spell_sounds` and
  `test_lightning`.

## Sources

- runblack.exe (the addresses above) and, for the sound bank, `LHaudiodllR.dll`.

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x67C175..0x67C6B2` | RenderParticleGJMesh::DrawAt: the CPU lighting of the mesh's world triangles (land light + model light) | `vs_world_triangles.sc` |

## Pending

- Sound: the owner's alignment, the camera shake, the game-state filters, the finite repetitions
  and the global distance cap (see [Sound of the particles](particles.md#sound-of-the-particles-srcaudiospellsounds-srcparticlesrulessoundcpp)).
- Chains: `UseDynamicLighting` (the V scroll is 0 in all the data).
- Meshes: `UseScriptHightlightPulse`, `UseDynamicLighting`, `UseGlobalAlpha`, the per-object Z
  order and `FaceCameraSprite`; from `ParticleAnimCreator`, `AnimEnum`, the blend to `MeshFileName1/2`,
  `UseDynamicLighting` and `UseGlobalAlpha`. The original lighting of the particles depends on these
  `UseDynamicLighting` flags (see also [rendering.md](rendering.md#pending)).
- The `DrawOffset` that draws an atom relative to the hand (fn_006C7840: UR_WillowWisp's `DoDrawOffsets`, the
  fireball's local cast).
- The spell dispensers' effect and the flying flock's cast are stepped once a turn in openblack, not by their owner's
  Draw with the frame's ms.
- World PSys: mesh creators, mist, chains, animation, light maps (they are stamped onto the terrain light), the
  spell and town rules (`UR_TownCentreBelief` is already there: see above), `CreateRule_GameObjectRef` (the glow of the
  keys of the Land1 gate, SF_HighlightOnObject), the sounds, and moving the hand effects of `HandEffects.cpp` onto this engine.
- The unported rules of the index (pieces, `LightningForkFlicker`, `ER_EmitFromParentAtom`, `CreateRule_GameObjectRef`).
- Unverified: `Particles/Creators/Mesh.h` `drawWithLandscapeColour`: "///< +0x5E: the particle's +0x24 bit 2 (CreateParticle 0x6A8B82)" versus `Mesh.cpp` `MakeMeshCreator` and miracles.md: "puts it in the particle's +0x24 bit 1". Bit 1 or bit 2: unverified.
- Unverified: `Particles/PSysManager.h` `CreateSpotVisual`: "GParticleContainer::CreateSpotVisualWithSpecifiedDuration 0x63E580: SPOT_VISUAL index ..., seconds (< 0: forever; 0: the entry's own life)" versus `CreateSpotVisualTurns` / `manager::CreateSpotVisualTurns` and miracles.md: 0x63E580 takes turns and 0 closes at the first Process. The second reading is the more detailed one (kept above); unverified which applies to the script native's seconds.
- Unverified: `Particles/PSys.cpp` `LandscapeCollide` / `EventAlways` use `GetLastGlobalMovement 0x674420 (inf: the atom's velocity over this step)`, while `Rules/Shield.cpp` says it is "the drawn position minus the one before (0 until it was drawn twice)". The Shield reading is kept above; unverified.
