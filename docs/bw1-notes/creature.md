# The creature: groundwork, random streams and what is unknown

What openblack has of the player's creature so far, and what is known of the original's. The creature code comes from
raffclar's tree (`stack/81-creature-mode` at `ff1e79d1`), ported onto ours in four commits as **pure, dormant
groundwork**: formulas, state machines and file formats, unit tested with fakes, that no game code calls yet. No
component, system, Locator slot or call site exists for it; a running game behaves as before, with one exception noted
under [The body](#the-body). Most of the creature classes of `runblack.exe` (`Creature`, `CreatureMental`,
`CreatureAction`, `CreatureAgenda`, `CreatureLook`, `CreatureFace`, `CreatureMimic`, `CreatureRoom`, `LH3DCreature`)
have headers in bw1-decomp but no bodies, so most of what follows is his model, not a reading of the original: the
constants that have no source are listed in [Stand-ins](#stand-ins-and-unverified-constants) and
[Pending](#pending).

- [What is ported](#what-is-ported)
  - [The creature tables in info.dat](#the-creature-tables-in-infodat)
  - [The body](#the-body)
  - [The mind and its file](#the-mind-and-its-file)
  - [Movement, body over time, spells, fights, throws](#movement-body-over-time-spells-fights-throws)
  - [Leash and the hand](#leash-and-the-hand)
  - [Audio, status panel and the follow camera](#audio-status-panel-and-the-follow-camera)
- [The spec file's animation numbers](#the-spec-files-animation-numbers)
- [Random streams](#random-streams)
- [Mimicry and the town hooks](#mimicry-and-the-town-hooks)
  - [What openblack does with a deed](#what-openblack-does-with-a-deed)
  - [Deed table (complete for W120)](#deed-table-complete-for-w120)
  - [Town and villager hooks](#town-and-villager-hooks)
- [Leash natives](#leash-natives)
  - [What openblack does with the leash natives](#what-openblack-does-with-the-leash-natives)
  - [The leash keys](#the-leash-keys)
- [The player's creature](#the-players-creature)
  - [What openblack does with the player's creature](#what-openblack-does-with-the-players-creature)
- [Stand-ins and unverified constants](#stand-ins-and-unverified-constants)
- [Addresses of the original, moved out of the code](#addresses-of-the-original-moved-out-of-the-code)
- [openblack](#openblack)
- [Pending](#pending)

> **Code rules.** The creature's pure logic stays free of global state and draws no random number itself: every draw
> is passed in by its caller, so the systems that come later choose the stream (see [Random streams](#random-streams)).
> No `random_device`, clock or file I/O in it; mind files are bytes in and bytes out, and the loader goes through the
> resource caches. Comments describe behaviour in plain English, with no decompiled names or addresses (those belong
> here). See [openblack-internals.md](openblack-internals.md).

## What is ported

### The creature tables in info.dat

- **`GCreatureInfo`** (one row per species, 0x384 bytes): the 416 bytes that used to be one opaque array are named.
  The species row is at +0x1E4, `startEnergy` +0x1F4, `comfortTemperature` +0x1FC, `growUpMinutes` +0x204,
  `energyDrain` +0x220, `sleepRecover` +0x230, `slowSpeed` / `walkSpeed` / `runSpeed` from +0x240, `runAwayDistance`
  +0x2D8, `sleepLength` +0x364, `pooPerEnergy` +0x370. Four blocks are still unread: +0x234 (0xC bytes), +0x24C
  (0x8C), +0x2DC (0x88), +0x374 (0x10). In the code these offsets survive only as `static_assert`s on the layout.
- **`CreatureActionInfo`**: `strengthGain` / `energyCost` / `exhaustionCost` at +0x0 / +0x4 / +0x8, `desire` +0x98,
  `desireMultiplier` +0xC0. `field0xfc` exists only in patch 1.20 (openblack issue #684).
- **`CreatureInitialDesireInfo`**: 0x1C0 bytes in memory, 0x1B0 in the file, which leaves out the 0x10-byte header;
  our `field0xN` is therefore the original's +0x10+N. Read from `info.dat`:
  - +0x10..+0x2C: the 8 sources of the desire;
  - +0x38 (our `field0x28`): learnable;
  - +0x4C (our `field0x3c`): holds 0.3 to 3 in the data (hunger and tiredness 2, healing 3); bw1-decomp calls it
    `DesireDecay`; his code uses it as the desire's cap;
  - +0x50 / +0x54 (our `field0x40` / `field0x44`): hold 0.999 / 1 in nearly every row (0.9 / 1 in two); bw1-decomp
    calls them `InitialValueMin` / `InitialValueMax`; one number is drawn from this range per desire (see
    [Random streams](#random-streams)), and his code uses it as the desire's per-turn decay;
  - +0x78 (our `field0x68`): the weight (bw1-decomp: `DesireGrowthRate`);
  - +0x80 / +0xC0 / +0x100 / +0x140 / +0x180: the name and the desire, short, trying and doing texts.

  bw1-decomp marks the names of +0x4C..+0x54 as fabricated. In `CreatureDesires::Initialise` 0x4DC100 the drawn number
  goes into an unnamed array (`field_0x288`), while the array that `FindWeakestDesire` 0x4DC7B0 compares as the
  desire's value (`field_0xa8`) is set to 0. The data (a cap of 0.3..3, a drawn range of 0.9..1) also reads like a cap
  and a per-turn decay. So his reading is kept (cap = +0x4C, decay drawn in [+0x50, +0x54], value 0) **(inferred)**:
  it makes the same 40 draws from the same fields in the same order; only the meaning of the stored values is open.
  This reverses the earlier note that the fields had to be swapped.

### The body

- **Meshes** (`src/3D/CreatureBody`): a creature mesh's file name gives the species, the appearance (base, evil, good,
  thin, fat, weak, strong) and the Ogre variants. The mesh folder `Data/CreatureMesh` also holds files whose last name
  part is not an appearance (`A_Tiger2_Base - Realistic Face.l3d`, `Eyeball.l3d`, `Eyelid.l3d`); those are now
  skipped, while an unknown species still names a mesh and logs an error. **This is the one change to running code:**
  before, `Eyeball` or `Eyelid` (whichever loaded first) took the id `creature/0/0`, the Unknown species' base mesh,
  so a creature of type 0 (`CREATURE_TYPE` 0, only the Playgrounds use it) drew an eyeball and now has no mesh. Nothing
  looks these meshes up by name and no creature exists on Land 1 or Land 2, so no verification check changes. Our
  rule is a port decision, not a reading (see [Pending](#pending)).
- **Morph** (`CreatureMorph`): the body is the base mesh pulled towards one mesh on each of three axes (evil-good,
  thin-fat, weak-strong); threshold and vertex blend as the hand's (0.03; base + |a|(m − base),
  [hand-and-interface.md](hand-and-interface.md)).
- **Layers, face, eyes, look** (`CreatureLayers`, `CreatureFace`, `CreatureEyes`, `CreatureLook`): one body animation
  at a time (stand, a one-shot action, or start / loop / end), with the head turn, the face and a gesture on top; the
  faces a feeling pulls and how long they hold; the eyes (eyeball and eyelid set at points on the body's triangles,
  blinking, the lids following the pupils); what an idle creature looks at.
- **Skeletal animation** (`src/3D/SkeletalAnimation`, `CreatureAnimation`): a species' variant animations (evil, good,
  thin, fat) are blended into the base's by the body's axes, element by element on the bone matrices. The rotation is
  our `affine::RotationYXZ` read by rows (the same nine cells as his formula; ours models the original's FPU
  rounding), and the rows are normalised with `std::hypot` as `HandAnimator` does. Angles are read back with his
  `asin` / `atan2` decomposition, not `affine::DecomposeYXZ`: the blended matrices are not unit, and at the gimbal
  (`m6 = m8 = 0`) `DecomposeYXZ` gives NaN as the original does, which would poison the keyframes. Two known
  differences from the hand's animator: in the non-looping edge frame the hand clamps to the last frame and the
  creature wraps to the first; and a bone the animation does not move takes the stand animation's first frame (the
  hand: the bind pose).
- **Skin** (`CreatureTattoo`, `CreatureMarks`, `CreatureSkin`): up to eight tattoos of sixteen designs
  (`Data/Textures/PlayersSymbols.raw`, colours from `tattoocols.raw`); wounds, burns and blood trails that age into
  scars; each base skin blended towards the evil or good skin, 4 bits a channel, weight trunc(|a|·256) ≤ 255 —
  bit for bit the hand's `MorphTexture` blend (a test pins it against `HandMorph.cpp`'s blend over all 256 weights).
- **The default tattoo symbols** come from `Data\Textures\OriginalChooseSymbol.raw`, 256×256 RGB (196 608 bytes),
  the exe's own file: `runblack.exe` names `PlayersSymbols`, `ChooseSymbol` and `OriginalChooseSymbol` and loads them
  at 0x5DE410 / 0x5DE425 / 0x5DE43D ([rendering.md](rendering.md), the `k_AlphaFlagStems` list), each with its `a.raw`
  alpha. His `Game.cpp:1738` reads the defaults from `I_PLAYER_SYMBOLS_.raw` instead. That file and
  `I_PLAYER_SYMBOLS_A.raw` are a repack's copies that the exe never loads: neither name occurs in `runblack.exe`;
  `I_PLAYER_SYMBOLS_.raw` (65 536 bytes, one grey byte a texel) is byte for byte `ChooseSymbola.raw`, the alpha, and
  `I_PLAYER_SYMBOLS_A.raw` (196 608 bytes, RGB) is byte for byte `OriginalChooseSymbol.raw` in the Complete
  Collection's `Data\Textures`. openblack's skin-art loader (`CreatureSkinArtLoader`, loaded at start-up under
  `creature_skin::k_ArtId`) reads the defaults from `OriginalChooseSymbol.raw` as his code reads its file: RGB, the
  level of a texel is its blue >> 4, 64×64 cells, 4 to a row. The players' symbols (`PlayersSymbols.raw`) are optional;
  a cell left blank there takes the default.
- **Hair and footprints** (`CreatureHair`, `CreatureFootprints`): tufts of strands as spring chains whose look moves
  from neutral towards evil or good; a print under the lower foot at each footstep, fading over about five seconds.

### Drawing in the frame

Read from `runblack.exe` (W120), 2026-10-08:

- **Where.** A creature is drawn in the models stage ([original-frame.md](original-frame.md#2-draw-stages-in-order),
  4m): `Creature::Draw` 0x517910 (vt+0x610) points the game object's 3D object (+0x40) at the body
  ([+0x160]+0x58 → +0x482C, 0x517AD1..0x517AE5), then calls `LH3DCreature::AddForDrawing` 0x48E1C0 (vt+0x1C,
  0x517AED). That one lights the body (`fn_00801C90`, haze `fn_007FEB30`), runs `CheckRegionOnScreen` and calls
  `DrawNow` 0x48E260 at once. Other callers of `DrawNow`: `FallingSpell::Draw` 0x526A42, `fn_00640054` 0x640386 (the
  creature snapshot) and `CreatureRoom::DrawAdditional` 0x7888B2 (the Cave).
- **The body** is an animated 3D object: `Morphable::MorphInit` 0x61731A makes it with `LH3DObject::Create(3)`, which
  is vtable 0x9A3068 (0x80B5DE; the hand's body is made the same way). `DrawNow` calls its `AddDrawing` (vt+0x100 =
  `fn_00813340`, 0x48E2A6): off screen or with +0xB8 set nothing; queued whole (`NewZObject` 0x813413, callback
  0x7FA980, key (dx² + dy²) + dz² from +0x38..+0x40) if its Z-sort bit (vt+0x44, from the mesh's 0x200) **or** its
  global-alpha bit (vt+0x4C, +4 & 0x80) is set; otherwise its Draw at once (vt+0x108 = `fn_00813BA0`, 0x8133C4). No
  creature base mesh in `Data/CreatureMesh` has the 0x200 flag (only the Lion's six variants and the three hand meshes
  have it; the body gets the base), and no write of the global-alpha bit to the body was found, so a creature is drawn
  at once, in the draw list's order, never through the Z-sorter **(inferred: the global-alpha part)**.
- **Inside the body's Draw** (`fn_00813BA0`): the mesh, then the shadows cast on objects (the list [0xFAA7E0],
  `fn_0080B050`, 0x813F8E..0x813FE0; a shadow is not drawn on its own caster, +0x464), then the **hair**
  (`LH3DObjectHair` at +0x88, `fn_00848390` 0x814029) only while +0x94 and +0xA0 (the body's cross-fade state) are
  both below 0.2. The hair strands go through `Draw3DWorldTriangle` 0x81C090 with the hair group's material (+0x78), or
  `fn_0081BC10` untextured (`fn_00847060` 0x847696 / 0x8476BC): drawn at once, no Z object.
- **The eyes** come after the body's `AddDrawing` returns: the shared eyeball and eyelid objects ([0xC64224] /
  [0xC64228], `LH3DObject::Create(0)`, `Data\eyeball.l3d` / `Data\eyelid.l3d`, 0x48E2DA..0x48E364), two eyes each,
  drawn at once with vt+0x108 (`fn_0080DB30`), or the cross-fade draws vt+0x15C / vt+0x154 when the body's +0x94 /
  +0xA0 is set (0x48E9A2..0x48E9DD, 0x48EFD3..0x48F00E). So the order is body, shadows over it, hair, eyes; a queued
  body (not seen) would have its eyes drawn before it.
- **No shadow falls on the body.** The receiver flag (+4 & 0x40) is set only through vt+0x78 with 1; no creature code
  calls it (none of the 33 vt+0x78 call sites is on the body), and a bare 3D object starts with +4 = 0x10009
  (0x816537). The game object's own 3D object (`Object::Create3DObject` 0x6365F0) is the one that asks, and it is
  replaced by the body before the draw. openblack: `ReceivesDynamicShadow` leaves out the creatures.
- **The creature's own shadow** is a projected shadow (`CreateDynamicShadow`, [rendering.md](rendering.md#projected-shadows-shadowinfo)),
  drawn with the land blocks (4i) and over the objects that receive; not ported.
- **The hair's material** (read 2026-10-08): one material for every creature, made the first time a creature loads
  (`fn_006186B0`, called from `LH3DCreature::LoadBase` 0x4EB11C and `Morphable::ReadBinary` 0x617DA4):
  `data\c_ape_hair.raw` (format 0x41), `CreateMaterial(9, texture)` 0x618701 into [0xD3F008], then +4 (ALPHAREF) = 0x32
  (0x618713); +5 stays 0 (`CreateMaterial` 0x82FD56), so it is one-sided (CCW) and clamped. `PrepareForDrawing` passes
  it to every hair group (0x4ED539 / 0x4ED5AC → `fn_00848350` → `fn_00847C90`, group +0x78). A group without its
  texture (+0x60 ≠ 1) goes through `fn_0081BC10` with no material, so the triangle draw's default one
  ([0xEA9EC4], `CreateMaterial(0, 0)` at 0x819011): mode 0, opaque. Both write depth, so the hair is never covered by
  what is drawn after it and behind it, whatever the draw-list order.
- openblack (`Renderer::DrawPass`, `Renderer::DrawCreatureParts`): the body in the models' loop at once (the mesh's
  0x200 decides, as for any model), then the shadows over it, the hair and the eyes. The hair in
  `render_modes::materials::k_CreatureHair` (mode 9, ALPHAREF 50; the world quads' shader drops the texels under
  ALPHAREF) and `k_CreatureHairPlain` (mode 0). **(approximate)** both two-sided: the ribbons are built facing the
  camera in openblack's own winding. **(approximate)** the models are drawn per mesh, not in the original's draw-list
  order (near blocks first); with the hair writing depth this only changes which background its blended edges show.
  Before 2026-10-08 the hair was in mode 6 (no depth write), so a model drawn after it and behind it (the temple)
  covered it.

### Drawing between turns

A creature's Transform moves once a turn (`creature_pose::CommitTurnPose`); every frame the locomotion writes
`components::CreatureDrawPose`, the position lerped from the turn's start to its end by the turn fraction and the
heading lerped the short way round (`CreatureLocomotionSystem::Update`). It is the villagers' "one turn behind" scheme
([animation.md](animation.md#drawing-between-turns)) without their slope shear or their yaw rate.

- **(inferred)** the original does the same: its creature is a Living on the same list, so the start-of-turn copy
  (+0x2C) and the per-frame lerp apply. `Creature::Draw` 0x517910 was read only for its stage and order, not for its
  matrix source; bw1-decomp has no body for it.
- openblack (2026-10-08): `creature_pose::BetweenTurns` gives the pose while it belongs to the turn that put the
  Transform where it is (the locomotion started, `toPosition` equal to the Transform's position, no hand or physics
  pose); otherwise the Transform. `ecs::DrawnModel` takes it, decided per row, after the hand's and the physics' poses
  and before `DrawPosition`, with the scale it already had (the pen's drawn scale, else the Transform's): one matrix
  for the whole drawn creature. A standing creature (pose equal to its Transform) keeps the Transform's matrix bit for
  bit. Every reader takes that matrix (`ecs::DrawnBodyModel`): the body's row (`RenderingSystem::WriteEntityRow`),
  hence its shadow and the hair's light; the eyes (`CreatureAnimationSystem::PlaceEyes`), the hair roots, what it
  holds (`UpdateHeldDraw`, its `PhysicsDrawPose` only), the footprints, the hand's touch and pick
  (`CreatureHandSystem`: `CreatureAlong` and the strokes and slaps, the original's hand colliding the drawn, posed
  body), and fire, spell seeds and tree bending (`DrawnModel` / `DrawnPosition`). `creature_pose::DrawnPlacementOf`
  gives the same sources' rotation alone, for where the eyes look ahead and the turn of what it holds. On the
  Transform, because the turn or the hash reads them: the head's look in `PoseBody` (its bones place the held object's
  Transform in the turn), the held object's Transform, the leash's collar end (the rope's tension decides the pull),
  the audio positions and the fight's hits.
- **(approximate)** the hand's touch on a creature changes the game (which creature is picked, strokes and slaps that
  reach its mind): it now reads the drawn body, so it can differ from before by up to one turn's step while the
  creature walks, and in its pen it finds the smaller body that is drawn there.
- **The pen's drawn size reaches every part.** What is sized apart from the matrix shrinks by
  `creature_pose::DrawnSizeShare` (the drawn scale over the Transform's, 1 outside a pen): the eyes' size and how deep
  they sit, the hair's strand length, width and root depth, and the footprints' side; the reflection's bounding
  radius takes `creature_pose::DrawnScale`. **(approximate)** the original's eyes and hair under a drawn size are not
  read: they are scaled as one with the body here, where the original may recompute them from the drawn size (their
  size rules are not linear in it). The hair's stiffness follows its scale, as for a smaller creature.

### The mind and its file

- **The mind file** (`components/creaturemind`, `MindFile`): the `.erc` / mind-file format, read and written from
  bytes. The names are enciphered with MSVC `rand`'s LCG (214013 / 2531011, `>> 16 & 0x7FFF`) seeded with 0x913 for
  each name. The two `.erc` files in `Scripts\CreatureMind`, `C4ba71b36.erc` (a Mandrill, row 14) and `Cacab45d4.erc`
  (a Tiger, row 2), are version 33 and read and write back byte for byte. They are not templates: they are two
  profiles' creatures, the files `LOAD_MY_CREATURE` reads ([The player's creature](#the-players-creature)).
  `Profiles\<name>\creature.lhp` is not a mind file (both profiles' files are refused as an unsupported version): it is
  an export the game writes and never reads back.
- **Mind** (`CreatureDesires`, `CreatureDecisionTree`, `CreatureLearning`, `CreatureWatching`, `CreaturePlanner`,
  `CreatureMindModel`, `CreatureMindTables`, `CreatureMindFileBody`): 40 desires that grow from their sources past
  their thresholds and otherwise fade; two decision trees per desire (what to act on, what to use); learning from
  strokes and slaps credited to recent actions; learning skills and miracles by watching; the planner that picks a goal
  and an action per desire. The sigmoids are ours: `Sigmoid` is `gutils::CreatureSigmoidThreshold`, `SigmoidStep` is
  `gutils::SigmoidThreshold` (the game's 41 exact floats, [engine-math.md](engine-math.md)); his decimal table differs
  in 14 steps (16, 18, 19, 23, 24, 26, 27, 29-33, 35, 36) by 1-2 ULP and gave 1 instead of 0 at a threshold of exactly 1.
- **Idle mind and actions** (`CreatureIdleMind`, `CreaturePlanActions`, `CreatureFeedback`, `CreatureReach`,
  `CreatureObjectActions`): the agenda an idle creature works through once a turn; which of the table's actions it can
  carry out; strokes and slaps; reaching by blending four animations; picking up, eating, tossing and throwing things.

### Movement, body over time, spells, fights, throws

`CreatureLocomotion` (speed towards a fraction of the top speed, slower uphill, walk and run blended by distance),
`CreaturePhysiology` (age, growth, energy and fat, tiredness, thirst, warmth through `gutils::SigmoidThreshold`,
strength, poo), `CreatureSpells` (freeze, small, big, weak, strong, fat, thin, invisible, nice, nasty, itchy and the
unfinished ones; each eases in, holds and eases out; the creature's sound actions per spell are 0x76..0x7E),
`CreatureFight` and `CreatureFightHud` (the duel in a circular arena and its panel) and `CreatureThrow` (the release
velocities). His own flight (`Fly`, bounce and slide) is left out: what a creature throws will fly through our
physics objects.

### Leash and the hand

`LeashRules`, `LeashRope` (40 masses on springs at 200 Hz), `LeashOwnership` (one leashable creature per player),
`LeashKeys` (L, V, B, checked against our `input::GameActionMap`, read once a frame by
`ecs::creature_loop::ProcessLeashKeys`; his shake tracker is left out) and
`CreatureHandRules`. The hand holds only the player's own or an allied player's creature, in or out of the
influence (`Creature::ValidForLockedSelectProcess` 0x476E10 and the action press 0x5D13FB..0x5D1481; raffclar's rule
allowed any god's); openblack has no alliances, so only the player's own. Let go within 450 ms of camera time, the
press was a click: packet 0x5F, the TOGGLE_LEASH path below (openblack: the leash key). Strokes and slaps act at frame
time on the local machine (packet 0x27 only replays them elsewhere); how the creature was treated goes to its mind
with packet 0x59, the mind's "player feedback" fn_004E06A0 (`FinishActionUnsuccessfully("player feedback")`, then a
plan when |v| > 0.01). The hand's side is in [hand-and-interface.md](hand-and-interface.md#the-hand-on-a-creature).

### Audio, status panel and the follow camera

- **`CreatureAudio`** with `src/Audio/Engine/AnimEffectKeys.h`: a creature's sounds sit on moments of its animations
  and are looked up by size, species, ground and action. `VoiceBankStem` gives the species bank's stem (our
  `CreatureBank` builds `audio/sfx/creature/{}.sad`). `SET_CREATURE_SOUND` is 1 after a reset and 0 means only the
  local creature's voice is heard ([audio.md](audio.md)); event modes 1 and 2 map to our Stop and Release (ours: no
  shipped animation uses them). The ground comes from `ecs::sea_cells::GetSurfaceType`, not from his surface rules.
- **`CreatureStatusPanel`**: damage, hunger and tiredness bars and the hand's reward while the hand is over a creature.
- **`CreatureMode`** and **`src/Camera/CreatureFollow`**: Creature Mode's rules (C locks onto the player's creature;
  the double click is our `DOUBLE_CLICK`) and the follow camera, built on our script-camera follow functions
  (`HeadingAndPitchFromPoints`, `PointFromDistanceHeadingAndPitch`, `FollowSeconds`, `ClampFollowDistance`,
  `FollowPitch`, `ThingViewingDistance`): distance 2..1500, least pitch 0.241661, height × 8, pace 2 s easing to 1 s, as
  in [script-camera.md](script-camera.md). Headings run from 0 to 2π.

## The spec file's animation numbers

`Data/ctrspec27.txt`, counted from the first animation after the version line, skipping `=` section lines:

| Number | Animation |
|---|---|
| 0 | stand |
| 16-27 | faces (the first ten pulled when idle) |
| 28-30 | sleep: start, loop, end |
| 31-33 | poo: start, loop, end |
| 34-36 | puke: start, loop, end |
| 37-39 | sit: start, loop, end |
| 52-74 | actions |
| 75 | head turn right / left |
| 76 | head down / up |
| 77, 78 | the same while sitting |
| 96 | eat |
| 106, 107 | faint, get up |
| 200-206 | gestures (202 yawn) |

The morph animation header's fields, from his tree (still `unknown0x0..` in our `MorphFile.h`): duration +0x0,
looping +0x4, stride rate +0x8, stride length +0xC, displacement +0x10..+0x18. `HairHeader`'s +0x0 is the species'
sound-object key.

## Random streams

The game has one random state, described in [engine-math.md](engine-math.md#random-numbers-game_random): the
synchronised GRand (`GameRand` 0x6DE510, `GameFloatRand` 0x6DE530), the local GRand (`LocalRand` 0x6DE570,
`LocalFloatRand` 0x6DE590), the particle systems' choice between the two per effect (`PSysRand` 0x6729E0,
`PSysFloatRand` 0x6729B0) and the C runtime's `rand()` 0x7C8837 with `Random(a, b)` 0x81D180. Our port reaches all of
them through `openblack::game_random` (`src/Common/GameRandom.{h,cpp}`, `Locator::gameRandom`). The turn/draw split is
in [engine-loop.md](engine-loop.md#4-random-streams-between-turn-and-frame): the synchronised stream is the game
logic's; the local, CRT and particle streams are shared by the turn and the frame. `Locator::rng` is only for the
tests' `TestRng`. The rule for the creature (decided 2026-10-07): each draw follows the original's stream; a private
seeded engine only for a draw with no counterpart in the original (visual extras, debug tools); no `random_device`.

Only one creature draw is decompiled:

- **`CreatureDesires::Initialise` 0x4DC100 (M119 0x125EE30)**, bw1-decomp `src/Black/CreatureMentalDesire.cpp:63`: for
  each of the 40 desires, `min + GameFloatRand(max − min)` with min / max at +0x50 / +0x54: **40 draws on the
  synchronised stream**, one per desire, in desire order. Where the drawn number is stored, and what it means, is in
  [The creature tables in info.dat](#the-creature-tables-in-infodat).
- `CreatureDesires::RandomiseIncreaseTime` 0x4DC310 jitters a desire's increase time when the "randomise" cheat flag is
  set; not decompiled, and no normal run reaches it.

Neighbouring facts the creature has to respect:

- **The decompiled eye blink is on the CRT stream.** The high-detail villager's eyes blink every `Random(1000, 5000)` ms
  with a hold of `Random(100, 200)` ms, a glance target of `Random(-0.25, 0.25)` and a squint of `Random(0, 0.3)`
  (0x88358D..0x8836E2; [intro.md](intro.md) "The family in high detail"). The creature's eyes are another object
  (`LH3DCreature`) and their stream is unknown.
- **The mist's frame counter is on the CRT stream.** `LH3DMist`'s constructor 0x7F9560 sets its counter (+0x84) to
  `Random(0, 16) & 15` ([map-loading.md](map-loading.md)); his creature cave's mist uses exactly that formula.
- **The temple's leash draws from the CRT stream when it is created** ([engine-loop.md](engine-loop.md), "Reward and
  TempleLeash creation"). No leash code in his tree draws at all.

In the ported code every one of these draws is passed in by the caller. "His draw" below is the call site in his
systems at `ff1e79d1`, which are not ported yet; "our stream" is what the port must call.

| His draw | Original counterpart | Original stream | Our stream to use | Confidence |
|---|---|---|---|---|
| `CreatureAnimationSystem.cpp:410` eye blink: next open = `random(5000) + 2500` ms (in our `CreatureEyes`: `random(interval) + interval / 2`, interval 5000), one draw per blink, on the frame clock | the creature's eyes are not decompiled; the decompiled blink is the high-detail villager's, `Random(1000, 5000)` ms | CRT for that blink; the creature's unknown | `game_random::crt::Random`, the blink moved onto the turn clock | inferred (approximate) |
| `CreatureMindSystem.cpp:705` `uniform(low, high)` → `creature_desires::Create`, one float per desire (40) | `CreatureDesires::Initialise` 0x4DC100 | synchronised (`GameFloatRand`) | `game_random::GameFloatRange(min, max)` | **checked in bw1-decomp** |
| `CreatureMindLearning.cpp:1117` the same `uniform` in `ClearLearning` | the same function | synchronised | `game_random::GameFloatRange` | **checked in bw1-decomp** |
| `CreatureMindSystem.cpp:702` `random(range)` → `creature_mind::Think`: the idle activity lot, the face, the hang-around direction and distance, the sleep, poo and throw-about places, the need and object-activity choice | not decompiled; every decompiled Living decision draws `GameRand` (`Villager.cpp:1016, 1019`, `VillagerReaction.cpp:180, 259, 326, 805`, `VillagerStates.cpp:737`) | synchronised (turn logic) | `game_random::GameRand(n)` | inferred (strong) |
| `CreatureMindSystem.cpp:914` `ShowFeeling` → `creature_mind::PullFace` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:942` `SitDown` → `creature_mind::SitDown` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:1080` `Sleep` → `creature_mind::Sleep` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:1117` `Poo` → `creature_mind::Poo` (where it goes) | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindSystem.cpp:872` `bernoulli(0.5)` in `PlayAction`: whether an action plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreatureMindLearning.cpp:368` `Random(range)`, used by `creature_plan_actions::Agenda` and `creature_watching::StepMimicry` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureMindLearning.cpp:373` `Chance()` 0..1, used by `creature_watching::StartMimicry`: drawn at the mimic site, during the turn, only for the player's own creature ([Mimicry](#mimicry-and-the-town-hooks)) | not decompiled (the entry point 0x4EA900 is read, the draw inside `Creature::MimicPlayer` is not) | synchronised (logic) | `GameFloatRand(1.0f)` | inferred |
| `CreatureLocomotionSystem.cpp:667` run-away distance `random(40) + species runAway` | `Creature::RunAwayFromObjectReaction` named, not decompiled; the villagers' flee draws are `GameRand` (`VillagerReaction.cpp:900, 903`) | synchronised (logic) | `GameRand(40)` | inferred |
| `CreatureLocomotionSystem.cpp:844` fidget choice `random(3)` | not decompiled | synchronised (logic) | `GameRand(3)` | inferred |
| `CreatureLocomotionSystem.cpp:857` `bernoulli(0.5)`: whether the confused fidget (the third choice above) plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreaturePhysiologySystem.cpp:324` the poo's yaw, `uniform_real(0, 2π)`, for the `LumpOfPoo` object | not decompiled; objects created by the turn's logic belong to the synchronised stream | synchronised (logic) | `GameFloatRand(2π)` | inferred |
| `CreaturePhysiologySystem.cpp:347-355` the 12 puke drops: two `unit(-1, 1)` and one `green(0.4, 0.9)` each (36 draws) | not decompiled; in the original the puke is not an ECS sprite | unknown | `game_random::psys::FloatRand` inside a `StepScope(Local)` if ported as a particle effect, else a private seeded engine | no counterpart |
| `CreatureFightSystem.cpp:626` `bernoulli(0.5)`: whether the faint, lying and getting up sequence plays mirrored | not decompiled | unknown | `GameRand(2)` | inferred (no counterpart found) |
| `CreatureFightSystem.cpp:894` `bernoulli(0.5)`: a taunt, or (his reading of the game) an action past the end of the list that plays nothing, before a fight's Ready stage | not decompiled | synchronised (logic) | `GameRand(2)` | inferred |
| `CreatureFightSystem.cpp:1073` → `fight::ChooseMove`, `fight::ComputerEndsBlock` | not decompiled | synchronised (logic) | `GameRand(n)` | inferred |
| `CreatureFightSystem.cpp:1421-1431` an unblocked blow's wound on the skin atlas, drawn in this order: u, then v (`:1426`, `:1427`, each `uniform_int(16, 239)` from the texel distribution of `:1424`), then the kind in `fight::WoundKind` (`random(2)`, and `random(3)` more only for a wound of blow type 1), then the column (`random(8)`): 4 or 5 draws per wound, in the order of the designated initialisers | not decompiled | synchronised (kept on the creature) | `GameRand(n)`, the same order and ranges | inferred |
| `CreatureCaveEffects.cpp:202` a cave mist's frame counter, `int(Random(0, 16)) & 0xF` | `LH3DMist` constructor 0x7F9560 | CRT | `game_random::crt::Random(0, 16)` | **checked** |
| `CreatureCaveEffects.cpp:146..314` (about 21 draws) the cave's smoke, spray and mist dome | the creature room is not decompiled (`CreatureRoom::DrawAdditional` 0x788630 is known only for its frame-anim cell, on the real clock) | unknown | a private seeded engine owned by the effect | no counterpart |
| `CreatureSpawner.cpp:936` the debug spawner's facing | debug tool | — | a private engine with a fixed seed | no counterpart (debug) |
| `CreatureSpawnerAppearance.cpp:163, 171` the debug mark place and wound kind | debug tool | — | a private engine with a fixed seed | no counterpart (debug) |

In every row his engine is a private `std::mt19937` seeded from `std::random_device` (his `CreatureAnimationSystem.h:34`,
`CreatureMindSystem.h:87`, `CreatureLocomotionSystem.h:55`, `CreaturePhysiologySystem.h:47`, `CreatureFightSystem.h:93`,
`CreatureSpawner.h:182`), except the cave, which uses `Locator::rng`. None of them survives the port. The state hash
covers the synchronised, local and CRT seeds, so only a private engine is off the references.

## Mimicry and the town hooks

The original reports a player's deed to the creature through `GPlayer::ConsiderMakingCreatureMimicPlayer(status,
action, thing, magic)` at 0x4EA900 (bw1-decomp `src/Black/Player.h:163`; W120 `symbols.txt` line 4883). It is a method
of the **player**: it only concerns that player's creature (player +0xA4C). There are **25 direct call sites** in W120
and no indirect ones (the address never appears in the image as a pointer). Of the 46 deed values
(`DETECTED_PLAYER_ACTION`, bw1-decomp `include/chlasm/CreatureEnum.h:745-805`, the 46 rows of `info.creatureMimic`), 40
can be reported and 6 never are. Unless a row says otherwise, `magic` is 0.

**Evidence.** Every caller was disassembled from the game's `runblack.exe` with capstone (2026-10-08). Its layout matches
the W120 `symbols.txt`, but its SHA-1 is not the one in `build.sha1`. The Mac build was not available for a cross-check.
The four sites ported first (deeds 11, 16, 21, 33) were read again from the same exe before they were ported.

### What openblack does with a deed

- `creature_watching::Deed` (`src/Creature/CreatureDeeds.h`): the 46 deeds in the table's order, `k_DeedCount` = 46,
  pinned to the size of `InfoConstants::creatureMimic`.
- `ecs::creature_mimic` (`src/ECS/CreatureMimic.{h,cpp}`): `Consider(player, deed, object, magic)` publishes
  `events::PlayerDeedForMimic` (`src/ECS/Events/CreatureMimicEvents.h`) with the object's position, read through the
  const registry (the origin for something with no `Transform`). Its handler, registered in `Locator.cpp` with the
  other game handlers, passes the deed to `CreatureMindSystemInterface::PlayerDid(player, deed, point, object)` when
  the mind system is there.
- **`PlayerDid` takes the player, and only creatures whose `Creature::owner` is that player watch** (our change to
  raffclar's, whose `PlayerDid` has no player and walks every creature). This is the original's per-player entry point;
  what the original does with `magic` is not read (Pending).
- The mind's draw is `creature_watching::StartMimicry`'s chance (`GameFloatRand(1.0f)`), drawn synchronously at the
  site, during the turn, as the original draws inside its entry point; it is drawn only for the player's own creature,
  once it passes the conditions checked before the draw.
- **Dormant on Land 1 and Land 2**: no creature exists there. The sites below that run on those lands (villagers
  dropped in the sea, water on a player's fields, trees replanted from the hand, and the empathy sites further down)
  publish an event that walks an empty const view, or has no subscriber; nothing is made, moved or drawn.

| Deed | Site (original) | openblack | Notes |
|---|---|---|---|
| 11 PLANT_TREE | `Tree::EndPhysics` 0x74BBB1 | `HandSystem::Replant` (`HandTrees.cpp`) | after the spot visual, before the tree goes back in its cell; player PLAYER_ONE, as the alignment line beside it ([trees.md](trees.md#dropping-and-replanting)). openblack updates the alignment before the deed, the original after it (Pending in trees.md) |
| 16 DAMAGE_BY_THROWING_AT | `Abode::ReactToPhysicsImpact` 0x406286 | `Buildings::ReactToPhysicsImpact` (abodes and storage pits; `StoragePit` 0x733730 and `TownCentre` 0x744380 only forward to the Abode's) | before the test of what hit it; with a player and the **building's own body** marked FROM_HAND, which never happens in practice ([physics.md](physics.md)); `creature_mimic::ShouldMimicBuildingHit` |
| 21 THROW_IN_THE_SEA | `Villager::HasSunk` 0x750AED | `ecs::HasSunk`, villager branch, through `creature_mimic::ConsiderThrownInTheSea` | after `IsAvailable` (0x750AB5), before the dead test; the player is the last dropper's (`VillagerLastInteraction`, the hand's) |
| 33 CAST_WATER_ON_CROPS | `Field::ApplyWaterSpell` 0x528F64 | `water::ApplyWaterSpell`, field branch | after the any-object part (0x528F3A), before the fire test and the sowing; only when the spell has a player; magic always 0x16 (`MagicType::Water`), also for the power-up |

### Corrections to earlier notes

- **Wrong labels in W120 `symbols.txt`.** 0x4EA670 is labelled `DecideOnNewPlan` (line 4882) but is the real
  `Creature::MimicPlayer`: 3 arguments (`ret 0xC`), calls `GetMimickingAction`, called at the end of
  `ConsiderMakingCreatureMimicPlayer`. The Mac symbols put `DecideOnNewPlan` in another TU. 0x4E0DE0 is labelled
  `MimicPlayer` (line 4663) but takes 1 argument (`ret 4`); it is called by `ForceActOnObject` and `fn_005E6BD0` and is
  not a deed site.
- **`Town::UpdateAggressor` is not a deed site.** The code at 0x73CAAA..0x73CB29 calls guidance functions
  (`fn_0071C960`, `fn_0071C9F0`), not the mimic (see [The emergency's creature part](#the-emergencys-creature-part)).
  The claims in our `MapShield.cpp` comment and in [miracles.md](miracles.md) were wrong; the wiki is corrected, the
  code comment is Pending.
- **`Spell::InitWithPos` is not a mimic site.** It has no call to the mimic function. It does make the creature
  empathise (`fn_00721730`, below), which is what our `Spell.cpp` comment says; a `SeeMiracle` call there would be a
  guess.
- **The abode hit tests FROM_HAND on the abode's own body**, not on the hitter's (0x406273 tests the argument's +0x1D8,
  the abode's entry; the hitter is its +0x20). A first reading took it for the hitter's flags. **The port follows the
  exe, not the H1 spec** (H1_SPEC §2.3 M1 had `hit->flags`, which would fire on every hand-thrown rock hitting a house;
  the coordinator's decision of 2026-10-08: follow the exe). The test also comes before the `PhysicallyDestroysAbodes`
  test, so the first guard is split: the valid test, then the mimic, then the destroy test (pure code between them).
- **Fidelity risk at `StoragePit` 0x733844.** When food is put into another player's pit, the original draws
  `GameRand(2)` (bw1-decomp `StoragePit.cpp` line 688) **before any creature check**, so the draw happens even when the
  player has no creature. openblack already draws it (`StoragePitStore::DoCreatureMimicAfterAddingResource`).
- **Disciple off-by-one in `Villager::Landed`.** The deed is disciple type + 21. `VILLAGER_DISCIPLE` has TRADER = 9,
  which has no deed of its own, so the last values shift by one: TRADER gives 30 (CHANGE_HOUSE), CHANGE_HOUSE gives 31
  (WORSHIP), WORSHIP gives 32 (TAKE_OBJECT_HOME), FROM_VORTEX gives 33, NONE gives 21 (THROW_IN_THE_SEA).
  [villagers.md](villagers.md) already states this correctly.
- **The animals' sinking tests availability first.** `Living::HasSunk` 0x5ED370 (the animals') returns 0 when the
  animal is not available (vt +0x2C, 0x5ED375), then reports deed 21 for a last dropper (0x5ED3A9, with no null test of
  the player), then sets it dying and deletes it. openblack's animal branch of `HasSunk` has no availability test
  (Pending).

### Gate in `ConsiderMakingCreatureMimicPlayer` (0x4EA900)

In order:

1. The player has a creature (player +0xA4C) without flag +0x24 & 0x10.
2. The creature's player +0x8E0 is not 2, and creature +0x1268 is at least 3.
3. Leash and table check: per-action table at 0xCAB220, 0xC0 bytes per action, flag at +4.
4. The current reaction's +0x10 is below 0x96.
5. `CanSeePos(thing)`, and the distance is within `fn_004EFC70(+0x160)`.
6. The action's priority (table +0) beats the mimic already running.
7. Then it calls `Creature::MimicPlayer` 0x4EA670.

openblack's `StartMimicry` checks the phase (at least 3), the learning leash, the reaction priority (below 150; the
port passes 0), the sight (within 150) and the chance, in that order; steps 1, 2 and 6 are not all modelled.

### Spell hits: `fn_004E9DF0(thing, magic, player)`

Called from `Spell::ApplyDefaultSpellEffect` at 0x720E8A, only when something is hit, the spell has a player and it is
not a creature cast; the report itself is at 0x720EB7. Returns 46 for "no deed".

- First: if the spell's desire is not ANGER and the nearest town within 100 m belongs to someone else, the deed is 20.
  The desire is `GMagicEffectInfo[magic]` +0x98 (`perceivedPlayerDesire[0]`; table at 0xCC6630, 0x11C bytes per entry).
- Fireball 1-3: 17.
- Lightning and explosion 4-8: 18. 9 (EXPLOSION_ONE_PU_TWO), 12, 13 and 16-20: nothing.
- Heal 10-11: 42.
- Food 14-15: 1 on a worship site, 3 on a storage pit, otherwise nothing.
- Wood 21: 5 on a storage pit, 8 on a building being built.
- Water 22-23: 33 on a field, 34 on something on fire, and **33 on anything else**.
- Flock 24-25: always 20.

### Landing: `ConsiderCreatureMimickingWhenObjectLands` (0x4EAAB0)

Called from `Object::InitialisePhysicsFromHand` (0x637306) and `Object::EndPhysics` (0x63764A). Status = the thing's last
dropper (`GetInterfaceStatusWhoLastDroppedMe`); player = that status's player.

- Within 80 m of the player's citadel (player +0xA48): 36 if the thing's town (`GetTown`) belongs to another player,
  otherwise 32.
- Otherwise: 35 if the thing's town belongs to another player and the nearest town is the dropper's.

### Deed table (complete for W120)

The "openblack" column says what is **ported**; otherwise it gives the site where the report would go, which waits for
the follow-up hooks commit (Pending).

| Deed | Name | Reporting function, call address | Trigger (status / player / thing) | openblack | Conf. |
|---|---|---|---|---|---|
| 0 | PUT_FOOD_IN_WORSHIP_SITE | `WorshipSite::DoCreatureMimicAfterAddingResource` 0x77DF19 | Food added to a worship site; the `MultiMapFixed` test runs first. Dropper's status and player; thing = the site | `ECS/ObjectDelivery.cpp`, `ECS/PotResource.cpp` | High |
| 1 | CAST_MAGIC_FOOD_IN_WORSHIP_SITE | `Spell::ApplyDefaultSpellEffect` 0x720EB7 via `fn_004E9DF0` | Food miracle hits a worship site. Spell's status and player; thing = what was hit; magic = the spell's | `Magic/Core/SpellEvent.cpp` | High |
| 2 | PUT_FOOD_IN_STORAGE_PIT | `StoragePit::DoCreatureMimicAfterAddingResource` 0x733870 | Food put in a pit, status +0x128 is the dropper's own player | ObjectDelivery / PotResource (the deed is computed, not reported) | High |
| 3 | CAST_MAGIC_FOOD_IN_STORAGE_PIT | 0x720EB7 via `fn_004E9DF0` | Food miracle hits a pit, or a workshop (`Workshop` overrides `IsStoragePit`) | SpellEvent.cpp | High |
| 4 | PUT_WOOD_IN_STORAGE_PIT | StoragePit 0x7338B6 | Non-food resource in a pit, own player (after the building-site test) | ObjectDelivery / PotResource (computed) | High |
| 5 | CAST_MAGIC_WOOD_IN_STORAGE_PIT | 0x720EB7 via `fn_004E9DF0` | Wood miracle hits a pit or a workshop | SpellEvent.cpp | High |
| 6 | BUILD_HOUSE | `Scaffold::EndPhysics` 0x6E87A9 | A thrown scaffold stops. Status = physics object +0x24; player = the **scaffold's** owner; thing = the scaffold | `ECS/Scaffolds.cpp` | High |
| 7 | PUT_WOOD_IN_BUILDING_SITE | `MultiMapFixed::DoCreatureMimicAfterAddingResource` 0x52F239 | Wood added to a building being built; returns true and stops the subclass tests | ObjectDelivery / PotResource (computed for a pit) | High |
| 8 | CAST_MAGIC_WOOD_BY_BUILDING_SITE | 0x720EB7 via `fn_004E9DF0` | Wood miracle hits a building being built (not a pit) | SpellEvent.cpp | High |
| 9 | PUT_WOOD_IN_WORKSHOP | `Workshop::DoCreatureMimicAfterAddingResource` 0x77A6AB | Wood added to a workshop (after the building-site test) | ObjectDelivery / PotResource | High |
| 10 | CAST_MAGIC_WOOD_BY_WORKSHOP | **never reported** | Wood cast on a workshop gives 5 instead | none | High |
| 11 | PLANT_TREE | `Tree::EndPhysics` 0x74BBB1 | Replanted tree, after `Forest::AddTree`. Player = `PhysicsObject::GetPlayer`; status = +0x24; thing = the tree | **ported**: `HandTrees.cpp` `Replant` | High |
| 12 | GIVE_TOWN_PROTECTION_WITH_SHIELD | **never reported** | Shield 19-20 gives nothing in `fn_004E9DF0` | none | High |
| 13 | BRING_PEOPLE_TO_WORSHIP | **never reported** | — | none | High |
| 14 | MAKE_ARTEFACT | `Town::AddArtifact` (Mac name; W120 `fn_0073FDA0`) 0x73FE49; `WorshipSite::AddArtifact` 0x77DBA1 | A thrown object suitable as an artifact lands within 50 m and joins a town or worship site (`Fixed::EndPhysics` 0x52E047 / 0x52E0BE). Player = the thrower; status = last dropper; not for trees | none: artifacts not ported (`ECS/Town/TownProcess.cpp`) | High (Mac name: Med) |
| 15 | DAMAGE_BY_THROWING | `PhysicsObject::GameTurnUpdate` 0x6463BE | After the impact sound and `ReactToPhysicsImpact`: a body from the hand (+0x1D8 & 4) with a status, a living thing or rock, not a tree. Status = +0x24; thing = the body | `ECS/Physics/PhysicsObjects.cpp`, after `ReactToPhysicsImpact` | High |
| 16 | DAMAGE_BY_THROWING_AT | `Abode::ReactToPhysicsImpact` 0x406286; `GameTurnUpdate` 0x64643A | Abode hit, with the abode's own body from the hand; thing = the abode. Physics: the same body test when it hit something (+0x20); thing = the thrown body | **ported** for the abode (`Buildings.cpp`); the physics one: PhysicsObjects.cpp | High |
| 17 | DAMAGE_WITH_FIRE | 0x720EB7 via `fn_004E9DF0` | Fireball 1-3 hits something | SpellEvent.cpp | High |
| 18 | DAMAGE_WITH_MAGIC | same | Lightning or explosion 4-8 hits something | SpellEvent.cpp | High |
| 19 | IMPRESS_BY_THROWING | **never reported** | — | none | High |
| 20 | IMPRESS_WITH_MAGIC | same | The desire/town rule, or any flock 24-25 | SpellEvent.cpp | High |
| 21 | THROW_IN_THE_SEA | `Villager::HasSunk` 0x750AED; `Living::HasSunk` 0x5ED3A9 | Sinks while available (`IsAvailable` first). Status = last dropper; thing = itself. `Living::HasSunk` does not null-check the player. Also NONE disciple + 21 | **ported** for the villager (`VillagerDrowning.cpp`); the animal's: no last dropper is kept for animals | High |
| 22-29 | MAKE_DISCIPLE_FARMER … CRAFTSMAN | `Villager::Landed` 0x760744 | First turn landed (+0x90 == 1, flags +0xE0 & 0x20 and & 0x200). Status = last dropper, needs a player. Deed = disciple (+0xF2) + 21 if below 46 | `LivingActionSystem.cpp` (`VillagerLanded`); disciples not ported | High (code) |
| 30 | MAKE_DISCIPLE_CHANGE_HOUSE | same | Reached by TRADER (9) + 21 | same | Med |
| 31 | MAKE_DISCIPLE_WORSHIP | same | Reached by CHANGE_HOUSE (10) + 21 | same | Med |
| 32 | TAKE_OBJECT_HOME | `ConsiderCreatureMimickingWhenObjectLands` 0x4EAB49 (+ disciple WORSHIP) | Dropped within 80 m of own citadel; thing's town is not another player's | `ECS/Physics/FromHand.cpp`, `ECS/LivingPhysics.cpp` | High |
| 33 | CAST_WATER_ON_CROPS | `Field::ApplyWaterSpell` 0x528F64; also 0x720EB7 via `fn_004E9DF0` | Field: spell has a player; magic always 0x16; thing = the field. `fn_004E9DF0`: water hits a field **or anything not on fire** | **ported** for the field (`SpellWater.cpp`); the spell hit: SpellEvent.cpp | High |
| 34 | CAST_WATER_TO_PUT_OUT_FIRE | 0x720EB7 via `fn_004E9DF0` | Water hits something on fire (not a field) | SpellEvent.cpp | High |
| 35 | STEAL_OBJECT_AND_PUT_IN_TOWN | ObjectLands 0x4EAB8E | Not near own citadel; thing's town is another player's; nearest town is the dropper's | FromHand.cpp / LivingPhysics.cpp | High |
| 36 | STEAL_OBJECT_AND_PUT_BY_CITADEL | ObjectLands 0x4EAB34 | Within 80 m of own citadel; thing's town is another player's | same | High |
| 37 | BREAK_ROCKS | `GInterface` tap handler 0x5DA650 (unnamed), call 0x5DA6C8 | Tapping a rock that is valid to tap. Status = interface +0x39C; thing = the rock | `ECS/Systems/Implementations/HandTurn.cpp` | High |
| 38 | THROW_FOOTBALL_IN_GOAL | **never reported** | — | none | High |
| 39 | CATCH_FOOTBALL | **never reported** | — | none | High |
| 40 | SACRIFICE | `Animal::ApplyThisToObject` 0x41B3DF; `Villager::ApplyThisToObject` 0x752D10 | Animal or villager applied to a `WorshipTotem`. Status = the applying hand's; thing = the victim; followed by the alignment update | `HandApplyToObject.cpp` (sacrifice not ported) | High |
| 41 | PLAY_WITH_TOY | `Object::InitialisePhysicsFromHand` 0x637457 | Status, `IsToy`, and no thrower or the thrower is not a creature (bw1-decomp `Object.cpp:603-606`) | `ECS/Physics/FromHand.cpp` (no `IsToy` yet) | High |
| 42 | HEAL | 0x720EB7 via `fn_004E9DF0` | Heal 10-11 hits something | SpellEvent.cpp | High |
| 43 / 44 | STEAL_FOOD_FROM_FARM / _FROM_STORAGE_PIT | StoragePit 0x7338B6 (shared tail; deed pushed at 0x733861) | Food put in a pit and status +0x128 is not the dropper. Deed = 43 + (`GameRand(2)` != 0) | ObjectDelivery / PotResource (computed, the draw made) | High |
| 45 | STEAL_WOOD_FROM_STORAGE_PIT | StoragePit 0x73389A | Non-food in a pit, status +0x128 is not the dropper | same (computed) | High |

### Town and villager hooks

All of these are empty stubs in bw1-decomp (`TownAttitudeToCreature.cpp`, `Town.cpp`, `Player.cpp`,
`CreatureDirectControl.cpp`, `TownDesire.cpp`, `Spell.cpp` are 0-35 lines; `TownCreatureInfo.h` lists only the
destructor and `GetBaseInfo`). Everything below was read with capstone from the shipped `runblack.exe` using the W120
symbols. The exe matches W120: 0x4C80F0 has the expected bytes, and the Creature vtable at 0x8CC810 resolves to the
right symbols.

#### Town step 16: `Town::UpdateAttitudeToCreature` 0x7437F0 (size 0x210)

Not ported (`TownProcess.cpp`, step 16); the numbers below make it portable (Pending).

- **Call site:** `Town::Process` 0x747380 calls it at 0x74744B, right after `ProcessTownEmergency` at 0x747444
  ([villagers.md](villagers.md)). Declared at `Town.h:353`; TU `Black/TownAttitudeToCreature.cpp` 0x743690-0x743A30
  (`splits.txt:2255`).
- **Data:** Town +0x970 is a singly linked list of `{next, TownCreatureInfo*}` nodes; Town +0x974 is its count. Each
  record: +0x10 the Creature, +0x14 the attitude, +0x18 the turn the attitude was set, +0x1C the turn it was reset.
- **Creation:** `Town::CreateCreatureInfo` 0x743720 on first contact (attitude 0, both stamps = turn), called from
  `Villager::SetupReactToCreature` at 0x767673.
- **Reader:** `Town::GetTownAttitudeToCreature(Creature*)` 0x7436F0 returns +0x14, or 0 if there is no record. Read by
  the villager reactions (0x76767B, 0x767779, 0x76849E), `WatchFightAnimation`, `Living::NumGameTurns…ToCreature`
  (0x5F1BBA, 0x5F1CCE) and `Creature::GetImpressiveValue` (0x47B244).
- **Per record, each turn:**
  1. If the creature's GameThing flags byte +0xA has bit 0 set, every node for it is unlinked and freed and the info is
     deleted (vtable +4, argument 1).
  2. Gate: `GetNearestTown(Town+0x14, 10.0)` 0x6020E0 must return this town (literal, and looks always true since it is
     the town's own position) and `CreaturePlan::IsValid` (mind +0xF48) 0x4F12E0 must be true.
  3. The new attitude comes from a member function in the creature-action info table: index = Creature +0x1120,
     entries 0x50 bytes, function at 0x9D16B8 + idx·0x50, called on `[[mind+0xF58]+0x30]`. The table is filled at run
     time by `crt_xc_fn_CreatureAction_00491B90`. Entries seen: 0x768540 None → 1; 0x768550 Fear → 3 (15 actions, e.g.
     idx 15, 18, 45-47, 95-105, 185, 186, 197); 0x768560 Respect → 4; `GameThingWithPos::…Eating` → 1;
     `Living::AttitudeToCreatureEating` 0x768580 → `vt+0x2C8(0) ? 3 : 1`.
  4. Fear (3) is forced when mind +0xF50 == 4, `[mind+0xF58]` vt+0x2C returns 6, and
     `[Creature+0x160]+0x28 == [mind+0xF58]+0x30`.
  5. A result of 1 does nothing. If the attitude changed, or it is 3, it is stored with +0x18 = turn. Only when it
     changed is `fn_004C9FE0(creature, 0x21 = CREATURE_HELP_TYPE_SHOW_TOWN_ATTITUDE_TO_CREATURE, att, 0, 0, 0)` called.
  6. **Timeout:** while attitude != 1, `secs = (turn − info+0x18) / (1000 / [0xD01A38])` (unsigned integer). Durations
     {0: 15.0, 2: 10.0, 3: 30.0, 4: 10.0}, index ≥ 5 → 0. When `secs > dur`, attitude becomes 1 and +0x1C = turn.
- **Attitude values:** 1 = none, 3 = fear, 4 = respect; 0 and 2 unnamed. Five help texts
  (`HELP_TEXT_TOWN_ATTITUDE_TO_CREATURE_01..05`, 1083-1087) fit values 0-4. No writer of 2 was found.

#### The emergency's creature part

- `Town::ProcessTownEmergency` 0x7477A0, `CallAllVillagersToTownEmergency` 0x747890 and `SetInStateOfEmergency`
  0x7479A0 have **no creature code** (no +0xA4C read, no call that reaches a creature).
- The "creature part" our `TownEmergency.cpp` refers to is in **`Town::UpdateAggressor` 0x73C9B0, at
  0x73CAA3-0x73CB2D** ([buildings.md](buildings.md); `Town.h:247`), after the slot update `fn_0073E0F0`:
  - `obj = EffectValues+0x28`. If it exists and `IsCreature()` (vt+0x34): `c = CastCreature()` (vt+0xA4),
    `p = c->GetPlayer()`. If `p` exists, `c` is not the town player's creature (+0xA4C) and
    `p->IsMemberOfThisPlayer(MyInterfaceStatus)`, it calls `fn_0071C960(guidance, c, effect)`: if `[0xC221CC]` is set
    and `HelpSpritesPlayNow(0xB)`, then `HelpSpiritSay(GetRandomSample(0xD99D08), 0xB)`.
  - Otherwise it calls `fn_0071C9F0(guidance, town, effect)` (sample group 0xD) when the caused player is local.
- **Help-spirit audio only, no game state.** It changes nothing in the verification checks.

#### The satisfy activity and `GetVillagerActivityDesire`

- `Town::SetVillagerActivity` 0x73FF10 (`Town.h:169`): best = the football's vt+0x4C (Town +0xEA4); then tries the
  town player's creature (+0xA4C) and the artifacts (+0x994, next +0x20), each with a strict `>`. If best == 0 it
  returns 0, otherwise it calls the winner's vt+0x50 (matches [villagers.md](villagers.md)).
- Creature vtable `??_7Creature@@6B@` 0x8CC810: +0x4C = **0x401870 `GameThing::GetVillagerActivityDesire`**
  (`fld 0.0; ret 4`), +0x50 = `GameThing::SetVillagerActivity` 0x401880, +0x54 = the GameThing default.
- None of the four 0xB44-sized vtables (Creature, Living, SpecialVillager, Villager) overrides +0x4C, and only
  Creature overrides `CastCreature`, so there is no subclass. The only overrides are Football 0x532220, TownArtifact
  0x4262D0 and Town 0x73FF00. `Creature.h` declares none.
- **The creature's part is always 0 and never wins.** openblack's `CheckSatisfyRelaxation` already gives 0; only the
  comment in `VillagerSatisfy.cpp` needs changing (Pending).

#### `GPlayer::MakeCreatureEmpathiseWithPlayerTownDesire` 0x4C80F0

0x3A bytes; `Player.h:183-187`; TU `CreatureDirectControl.cpp` 0x4C5F80-0x4C8440.

- `c = player+0xA4C`. If `c` exists and `Creature::CanSeePos(pos)` 0x477440, it calls
  `fn_004C8050(&mind[c+0x164]+0x18C80, desire, weight)`, which checks `-1 < d < 17` and sets
  `a[0xA0/4 + d] = clamp(a[...] + weight, 0, 1)`. The sister `MakeCreatureEmpathiseWithPlayer` 0x4C80B0 does the same
  through `fn_004C8000` on `a[0..39]` (CREATURE_DESIRES).
- **The object** at mind +0x18C80 is a `CreaturePerceivedPlayerDesires`, at +0x40 inside `CreatureAttitudeToPlayer`
  (mind +0x18C40; ctor `fn_004C8130`, called from the `CreatureMental` ctor at 0x4D23BE;
  `CreatureAttitudeToPlayer.h:21-28`). Layout: +0x00 float[40]; +0xA0 float[17]; +0xE4 57 histories of 0x80 bytes
  (30-float ring, index at +0x78, count at +0x7C). The header's 0x20-byte history struct is a placeholder: the 0x80
  stride totals 0x1D64 = 0x1DA4 − 0x40.
- **Each turn:** `Creature::ProcessState` → `fn_00477A00` (0x477A88) → `fn_004C7EB0`: `fn_004C7F20` multiplies all 57
  values by **0.9995** (float at 0x8CF3D4); `fn_004C7F60` pushes them into the rings only on even turns (skips when
  `g_game+0x205A40 & 1`).
- **Readers:** the town-desire array (+0xA0) is only saved and loaded (`fn_004E79D0` at 0x4E7DA9 from `SaveMind`;
  `fn_004E84A0` at 0x4E8914, version ≥ 0x10). No gameplay reader of it or its histories was found. The creature-desire
  array is read by `CreaturePerceivedPlayerDesires::GetDominantDesire` 0x4C7ED0 (from
  `CreatureRoom::MakePersonalityScrollText` 0x788F23 and `TempleRoom::AddText` 0x79B651), which returns the last
  activated desire with value > 0 and zeroes those values. Feedback also adds to it
  (`UpdateAttitudeToPlayerFromFeedback` 0x4E10D5, index 1 or 2).
- **The callers of 0x4C80F0**, and what openblack does at each:

  | Caller | Call | Desire, weight | openblack |
  |---|---|---|---|
  | `CheckInteractWithWorshipSite` | 0x7572FD, 0x75738B | 9, 0.5; 7, 0.5 | state 171 not ported; not in bw1-decomp's source |
  | `CheckInteractWithAbode` | 0x7574DF | 5 FOR_ABODES, 0.5 (no player test) | state 172 not ported (disciples) |
  | `CheckInteractWithField` | 0x7575F6 | 0 FOR_FOOD, 0.5, with a player | **published** (`VillagerInteract.cpp`) |
  | `CheckInteractWithFishFarm` | 0x757674 | 0 FOR_FOOD, 0.5, with a player | **published** (`VillagerInteract.cpp`) |
  | `CheckInteractWithTree` | 0x7576E8 | 1 FOR_WOOD, 0.5, with a player | state 175 not ported |
  | `TakeWoodFromTree` | 0x75FBF1 | 1 FOR_WOOD, 0.5, with a player | **published** (`VillagerForester.cpp`) |
  | `fn_00721730` from `Spell::InitWithPos` | 0x71FE5D | `GMagicEffectInfo` +0xA0, 1.0, plus +0x98 / +0x9C into 0x4C80B0 | not ported (`Spell.cpp` comment) |

  Indexes 0..16 are `TOWN_DESIRE_INFO_*` (`include/chlasm/Enum.h:1481-1500`, our `TownDesireInfo`). bw1-decomp's source
  shows only five calls (`VillagerCheck.cpp:103,136,152,166`, `VillagerForester.cpp:164`); the two worship-site calls
  and the spell call are from the exe.
- **openblack:** each published site calls `creature_mimic::EmpathiseWithTownDesire(GetPlayerOf(villager), desire,
  0.5, villager)` on its success path only. It publishes `events::CreatureEmpathyWithTownDesire` (player, desire,
  weight, the villager's position) when there is a player. **It has no subscriber yet**: the mind's side (0x4C80F0 and
  its per-turn decay) waits to be ported. A port can be exact (clamped add, ×0.9995 decay per creature turn, even-turn
  history); with no gameplay reader found it would only affect the save file and debug state. These sites run on Land 1
  (foresters taking wood, farmers and fishermen at work) and change nothing.

## Leash natives

**Evidence.** The game folder's exe is W120: the native table's "NONE" entry is at 0x00C0DB98, as
`bw1-decomp/src/Black/ScriptFunctions.cpp:29-30` says. bw1-decomp has the table entries (`ScriptFunctions.cpp:232-401`)
and the symbols (`config/BW1W120/symbols.txt:15400-15423, 15711`) but no bodies; the bodies below were disassembled
from `runblack.exe` with capstone. Mac (BW1M110) names are given in brackets where they differ. All nine are ported
(`Creature/LeashScript`, called from `CHLApi.cpp`); see [What openblack does with them](#what-openblack-does-with-the-leash-natives).

### Common facts

- `GScript::ScriptErrorMessage` (0x6F62B0) is a bare `ret`: every script error message is a no-op in the shipped game.
- VM push types: 1 = Int, 6 = Boolean (our `LHVMTypes.h` `DataType`). `POP` returns the raw 32-bit value; only
  TOGGLE_LEASH converts it (`fld` + `__ftol`).
- "Is a creature" is vtable +0x34; "its player" is vtable +0x1C (`GetPlayer`).
- The leash state is a `GLeashStatus` at `GPlayer::GetLeaderInterfaceStatus()` +0x12C. Fields: +0x14 leash on;
  +0x18 works; +0x1C type in use (a LEASH_TYPE); +0x24 object it is tied to; +0x34 turn it was tied; +0x38 interface
  status.
- There are 9 leash entries, not 8 (TOGGLE_LEASH is the ninth). Ids, names and argument/return counts match our
  `CHLApi.cpp` bindings.

### Script enum and the temple's selection

`bw1-decomp/include/chlasm/Enum.h:556-565` (Lionhead header, "Jonty Barnes 2001-11-15", via Daniels118/chlasm):
`LEASH_TYPE_NONE = -1`, `FIRST = 0`, `EVIL = 1`, `ROPE = 2`, `GOOD = 3`. The original `LAST = 4` is commented out; the
decomp changed it to 3. The game data ships no CHL `Enum.h`.

The temple stores the type as a 0-based index in `TempleLeash::field_0x0` (`CitadelHeart.h:77-80`), -1 for none:

- Setter `fn_004648E0` (= Mac `CitadelHeart::SetLeash(LEASH_TYPE)`): 1 → 0, 2 → 1, 3 → 2; anything else leaves it
  unchanged.
- Getter `fn_00464920`: returns -1 when `heart->leashes` (+0xE4) is null or the index is -1; otherwise 0 → 1, 1 → 2,
  anything else → 3.
- `LeashObj::InterfaceTap` (0x464490) sets the index to -1 when the post already selected is tapped.

### Per native

| Id | Name | W120 function | Pops (first pop = last pushed) | Pushes | Behaviour |
|---|---|---|---|---|---|
| 185 | ATTACH_OBJECT_LEASH_TO_OBJECT | `AttachObjectLeashToObject` 0x6F4480 [Mac `AttachObjectLeachToObject`] | A, then B | — | Swaps A and B if only one is a creature, so either order works. Needs a creature with a player, then `fn_005E6BD0(creature, object)` on the leader's `GLeashStatus`: plays the sound, +0x24 = object, +0x34 = game turn, `SetOn(creature, 1)`, `UpdateLeashLengthWhenAttachedToObject`, then 0x4E0DE0 (labelled `MimicPlayer`), and the TownDesireFlags / TownCentre desire. |
| 186 | ATTACH_OBJECT_LEASH_TO_HAND | `AttachObjectLeashToHand` 0x6F4580 | creature | — | Needs a creature with a player, then `fn_005E6EA0(creature)`: +0x24 = 0, `SetOn(creature, 1)`, a sound for the local player. **No branches**: always unties and turns the leash on (differs from raffclar's tied/Toggle logic). |
| 187 | DETACH_OBJECT_LEASH | `DetachObjectLeash` 0x6F4610 | creature | — | Needs a creature with a player, then `GLeashStatus::SetOn(creature, 0)` (0x5E6F70) on the leader status. Off path: clears +0x14 and +0x24, unsuppresses desires unless +0x1C == 2, clears some creature fields, `StopImmersion(7)`. |
| 222 | IS_LEASHED | `IsLeashed` 0x6F4980 | object | Bool | false if missing, not a creature, or no player; otherwise `Creature::GetInterfaceStatusLeashOn(c) != 0` (0x4CF060): some interface status of the player has +0x14 set. |
| 249 | SET_LEASH_WORKS | `SetLeashWorks` 0x6F4E50 | creature, then value | — | `GLeashStatus+0x18 = raw popped bits` (float bits, unconverted) when it is a creature with a player. |
| 269 | IS_LEASHED_TO_OBJECT | `IsLeashedToObject` 0x6F4FC0 [Mac `ObjectLeashedToObject`] | A, then B | Bool | Same swap as 185. Pushes `GLeashStatus+0x24 == other`; does not test +0x14. Any failure pushes false. |
| 275 | GET_OBJECT_LEASH_TYPE | `GetObjectLeashType` 0x6F5460 | object | **Int** | See below. |
| 305 | SET_DRAW_LEASH | `SetDrawLeash` 0x708C80 | value | — | `GGame+0x250090` (GScript) `+0x78 = raw popped bits` (written at 0x708C9D); matches [intro.md](intro.md). Does not touch the leash state. |
| 354 | TOGGLE_LEASH | `ToggleLeash` 0x6F4430 [Mac `TogglePlayerLeash`] | player (float, `ftol`) | — | See below. |

**TOGGLE_LEASH.** `ConvertScriptPlayerToGamePlayer` (0x6EB9A0) → `GetPlayer` → `GetLeaderInterfaceStatus` →
`GetInterface` → `fn_005D06E0`, which takes `player->creature` (+0xA4C), needs creature vt+0x2C true and Creature
+0x1110 != 0, needs the leash off or on in this same interface status (+0x39C), then calls `fn_005E7140`: if tied
(+0x24) it unties (+0x24 = 0, town desire reset, `StartImmersion(7, 0x80000000)`), otherwise
`SetOn(creature, !+0x14)`.

### GET_OBJECT_LEASH_TYPE (0x6F5460-0x6F54F0)

It does **not** report the creature's own leash. It does `dynamic_cast<Creature>` → `GetPlayer()` → `player->citadel`
(+0xA48, `Player.h:95`) → `citadel->heart` (+0x30) → `fn_00464920(heart)`. It reports the **type selected at that
player's temple**, pushed as Int:

| Case | Pushes |
|---|---|
| Object not found, not a creature, no player, no citadel, or no heart | `0` |
| Heart exists but its TempleLeash is not created, or nothing is selected | **`-1` (`LEASH_TYPE_NONE`)** |
| Selected index 0 / 1 / 2 or more | `1` EVIL / `2` ROPE / `3` GOOD |

**No leash selected pushes -1.** 0 only means the lookup failed. raffclar's "None → 0" is right only for those failure
cases. The port is to push what the original pushes.

### What openblack does with the leash natives

`creature_leash::script` takes the leash service and the things named, so it is tested with a fake
(`test/creature/test_leash_script.cpp`); the natives in `CHLApi.cpp` pop as the table above and push the defaults when
there is no leash service. Both are dormant on Land 1 and Land 2: no leash opcode runs there with the start-up answer 3
(the `SET_DRAW_LEASH` calls are in `CreatureDevLeashIntro` and `AttachToHouse`, which are skipped), and there is no
creature to lead.

- **A thing**: an id of 0 or an entity no longer valid is none, silently (the original's messages are empty). "A
  creature" is an entity with the `Creature` component, looked up through the const registry, so no storage is made.
  The "with a player" part is not checked: the leash service refuses a creature its player can't lead.
- **185 / 269**: the second pop is the creature and the first the thing; when only the first is a creature, they swap.
  185 ties the leash (`TieTo`, which puts the picked leash on first if none is worn); 269 compares the tie.
- **186**: a tied leash is untied back to the hand; a leash not worn is put on (`Toggle`); one already held stays. The
  original always unties and turns the leash on. Both end in the same state only when the leash service lets the
  leash on: `Toggle` (and `TieTo` for 185, through `PutOn`) goes through `WhyNot`, and refuses, logging it and keeping
  it as the last refusal, a creature that is not the one its player leads or that does not know the leash. The script's
  command then does nothing here (Pending).
- **187**: `TakeOff`. **222**: `IsLeashed`. **249**: `SetWorks`, set when the popped bits are not zero.
- **275**: the leash picked for the creature (`Picked`: the one it wears, or else the one to put on next, worn or not)
  as the scripts number it, `LEASH_TYPE_NONE` (-1) for a creature with no leashes, and 0 when the thing is missing or
  not a creature. The original reads the leash picked at the player's temple, worn or not; the port keeps the pick on
  the creature (`CreatureLeash::selected`, set by the hotkeys and by putting a leash on) until the temple leash comes
  (Pending).
- **305**: `script_control::CameraControl::drawLeash` = the popped value as it is; the leash draw reads it.
- **354**: the script player (a float, truncated) through `magic::ScriptPlayerToGamePlayer`, then the leash key for
  that player (`PressKey`, which also refuses a player with no creature to lead). In the port a script player out of
  0..8 does nothing: that range is `ScriptPlayerToGamePlayer`'s own, and the original's conversion's range checks are
  not traced (Pending). The original reaches its own creature checks by another path: the leader's interface status,
  then `fn_005D06E0`'s tests (creature vtable +0x2C, Creature +0x1110, the leash state of that same interface status);
  the port's are `PressKey`'s (`PlayersCreature`, then `WhyNot` in `Carry`). Which creatures each lets through is not
  compared (Pending).

### The leash keys

L, V and B are read once a frame, after the temple's room keys: the first of the three that went down this frame
(down and changed, as the room keys are read) goes to `PressKey` for the local player, only when the player has a
creature they can lead. They are not read while the debug windows have the keyboard (the actions are not framed then)
nor while the world is paused in the citadel. The original's key path is not read (Pending); since at most one turn
runs a frame, a key read at frame time changes the leash before the next turn, as a packet sent for it would.

## The player's creature

The creature a profile brings to a land, read in `runblack.exe` W120 (bw1-decomp has the headers only).

- **`LOAD_MY_CREATURE`** (native 250, `GScript::LoadMyCreature` 0x6FD260) pops z, y and x, drops y, and calls
  `GGame::LoadMyCreatureIntoNewMap(cell x, cell z)` 0x552AC0 (the Mac's name; W120 `fn_00552AC0`) with the high words of
  `ftol(metres × 6553.6)`. It is that function's only caller; the lands' scripts reach it with their own
  `LOAD_MY_CREATURE` (Land 1 ip 68025, Land 2 ip 84316, Land 3 ip 113848, VortexEntry ip 128).
  1. If the local player has a creature (`GPlayer` +0xA4C), nothing.
  2. The file is the local `LHPlayer`'s +4 string (interface +0x1BC), or "Dummy": the profile's registry value
     `HKCU\Software\Lionhead Studios Ltd\Black & White\LHMultiplayer\Profiles\<profile>\file`, read with
     `LHNetGetCurrentProfileString("file")` (0x66BF5B). It is a `C<8 hex>.erc` name the profile keeps; it is not a CRC32
     of the profile's name.
  3. `Creature::Load(name, MapCoords(cells), player, NULL)` 0x4E7FF0 → `LoadFullyQualified(".\scripts\CreatureMind\" +
     name)` 0x4E8040: the version into 0xCAB174, then the species row (< 17), `Creature::CreateCreature(coords,
     &CreatureInfo[row] (0xC60460 + row × 0x394), player)` 0x474B50, +0x1058 = (start data and not 1), the path to
     +0x1228, `CheckValidation` from version 0x15, then the mind (`fn_004E84A0`, which builds the body through
     `Update3DCreatureFromAttributes` at 0x4E8A05). A missing or empty file or a bad row: no creature, no message.
  4. `SetFizz(1.0, 0, false)` then `SetFizz(0.0, 3.0, false)` (0x47AB90): sound tag sfx 0x27 at the creature and the
     fizz at 1, then a fade to 0 at -1/3 per second (+0x12AC lock, +0x12B0 current, +0x12B4 target, +0x12B8 rate). No
     `rand()`.
  `MapCoords(cell, cell)` 0x602FC0 puts the point in the middle of the cell (low words 0x8000), altitude 0.
- **`CURRENT_PROFILE_HAS_CREATURE`** (463, `GGame::CurrentProfileHasACreature` 0x555A30): whether
  `.\Scripts\CreatureMind\<that file>` exists.
- **`CreateCreature`** 0x474B50 returns NULL when the player already has a creature; otherwise `Creature::Create`
  0x474A20 (memory zeroed by `Base::new` 0x4366F0, so the development phase +0x1268 starts at 0). The home +0x1200 is
  the player's +0xA48+0x30 object's position (the citadel's), else the creation point; the creation point is also copied
  to +0x1214. The size is the species' +0x1FC. Its draws are all on the synchronised stream: the Living constructor's
  `GameRand` (0x5EBF89), strength and fat ± 0.1 (`GameFloatRand(0.2)` at 0x4EF3FA and 0x4EF426), each desire's
  initial value and increase time (0x4DC14E, 0x4DC346), two per desire source (0x4DE1B2, 0x4DE21B), about 30 in
  `fn_004E2E70` (0x4E2E93..0x4E33F6), one in `fn_004F1FF0` (0x4F201F) and, on condition, `SpreadReaction` (0x6E3F18);
  a range of 0 draws nothing. Loading from a file adds the mind's sources (0x4E870D).
- **`Physique<file>`** in the same folder is written by `Creature::Save3D` 0x4E70C0 (every 600 ticks for the local
  creature when interface +0x1FC and +0x1058 are 0, on `ToBeDeleted` and on `ClearMap`) and read only by the front
  end's tattoo editor (0x4E7310, from 0x54284A); never by `LOAD_MY_CREATURE`. Layout: int species; floats size
  (LH3D +0x90), strength (physical +0xC), fat (physical +0x14) and alignment; then two counted dword lists (skin marks,
  LH3D +0x5184). `PhysiqueC4ba71b36.erc`: Mandrill, size 2.0, strength 0.84212, fat 0.4, alignment 0.118912, 0 and 3
  entries.
- **`Profiles\<name>\creature.lhp`** is written by `fn_0047B440` with the same 600-tick save: the creature's values
  into `.\scripts\creature_html.tmpl` (`%s\html\%s.html`), then `GenerateCreatureHTML` `fn_00578330` writes an
  "LHNILD" archive with a "CREATURE" section and up to five `creatureshot_%d.jpg`. Nothing reads it back. Its header
  (`fn_005776E0`) is the game's only `srand(time)` (0x577721), followed by a `LocalRand(0x8000)` (0x577731).
- `GGame::GetNextPlayerWithNoCreature` 0x550A30 has no callers (it also ignores its argument).
- **The natives Land 1 reaches on it:**

  | Native | Original | Pops, in order | Does |
  |---|---|---|---|
  | 197 `CALL_PLAYER_CREATURE` | 0x6F3D40 | player | the game player's creature (+0xA4C) through `AddScriptGameThing(c, 0)`; none: "No creature of player %d" and 0 |
  | 223 `SET_CREATURE_HOME` | 0x6F4A20 | z, y, x, the object | a creature: +0x1200 / +0x1204 = `ftol(x × 6553.6)` / `ftol(z × 6553.6)`, +0x1208 = 0 (y dropped) |
  | 208 `SET_CREATURE_DEV_STAGE` | 0x6F4820 | the stage (an int), the object | a creature: `MoveToDevelopmentPhase(stage, 0)` |
  | 205 `DEV_FUNCTION` | 0x6FC130 | the function (an int) | the table below |
  | 282 `CREATURE_IN_DEV_SCRIPT` | 0x6F5680 | the object, the value | a creature: +0x10C0 = the value as popped |

  The object natives print "Thing not found!" / "Thing not creature!" and do nothing for anything else.
- **`DEV_FUNCTION`** (`SCRIPT_DEV_FUNCTION`, bw1-decomp `include/chlasm/ScriptEnums.h`; jump table 0x6FC4E4), on the
  local player's creature:

  | n | Name | Does |
  |---|---|---|
  | 1 | START_DEVELOPMENT_SCRIPTS | `MoveToDevelopmentPhase(0, 0)`, the home copied to +0x11A8, +0x11B4 = 12.0 (no null check) |
  | 2 | ROPE_LEASH_ENABLED | +0x1110 = 1 |
  | 3 | OTHER_LEASHES_ENABLED | +0x1114 = 1, +0x110C = 1 |
  | 4 | LOAD_MY_CREATURE | `SetPacket(0x4E)` |
  | 5 | RESET_ESCAPE_STATE | `TutorialState` = 1 |
  | 6 | MY_CREATURE_POINT_OUT_HIGHLIGHT | a spiral search for a `ScriptHighlight` that is not a did-you-know, then a forced plan |
  | 7 | CLEAR_INTERACTION_MAGNITUDE | mind +0x18C5C = 0 |
  | 8, 9 | MY_CREATURE_CAN_DIE, CANNOT_DIE | +0x1158 = 1, 0 |
  | 10, 11 | CREATURE_HELP_ON, OFF | nothing; 42 calls of `fn_004CA580` |
  | 12 | ENTER_SAVEGAMEROOM | `GoInsideCitadel(5, 0)` |

  Packet 0x4E (handler 0x63D86B, jump table 0x63DDCC) makes or loads the sender's creature at its citadel's home
  (`CLANCREATECREATURE<nn>` start data: `CreateCreature` and phases 0-12; otherwise `Creature::Load`), then sets +0x1108,
  +0x110C, +0x1110 and +0x1114 to 1 and phase 13: multiplayer and skirmish only.
- **`MoveToDevelopmentPhase`** 0x4C59C0: when confined, unleashed and at phase 5 or more, +0x11B4 = 0; the phase to
  +0x1268, +0x126C = 0; every desire unsuppressed and deactivated; then for each phase from 0 to the one asked, its "add"
  list (10 slots) activated and its "remove" list (4 slots) deactivated (42 = empty); +0x194 = 0 and the agenda cleared.
  No draw. The table is `CreatureDevelopmentPhaseEntry[14]` at 0xC84878 (stride 0x84) from `info.dat`'s
  DETAIL_CREATURE_DEVELOPMENT (records of 0x74 bytes from 0x10B6C; the add list at +0x4C in memory, the remove list at
  +0x7C). Adds by phase: 0: 5, 8, 9, 17, 18, 21, 24, 28, 35, 23; 1: 4, 6, 29, 38, 3; 2: 15, 19, 20, 36; 3: 14; 4: 33, 7;
  5 and 6: 16, 37; 7: 31; 8: none; 9: 0, 10, 27, 13; 10: 2; 11: 1; 12: 0, 10, 27; 13: 1, 32, 34, 39, and 29 removed. At
  phase 13, 33 desires are active; off are 11, 12, 22, 25, 26, 29 and 30.
- **`SET_FOCUS` on a creature** (`GScript::SetFocus` 0x6F90B0): a creature is first put under script control (vtable
  +0x440, bit 0x400 of +0x24), then `Creature::SetFocus(LHPoint)` 0x4F6760 (vtable +0x510; other objects take
  `Object::SetFocus` 0x6393A0, the yaw snap). It stores the point in the mind (+0x1D3F4 = +0x1D3F8 = 1, the point at
  +0x1D400), `PrepareCreatureForScriptedAction(1)` 0x4F6A90 (script control, `FinishActionUnsuccessfully`, a scripted
  plan of desire 0x18 and action 0x16 into mind +0xF48, the sub-action agenda at mind +0xFA8 reset), then queues
  sub-action 7 "TurnToFacePos" with `LookAtPosition` 0x4D1460. Each turn its phase 0 (0x501660) is done when the point
  is less than 0.1 away or within π/8 of the creature's heading; otherwise `StopMoving` and `StartTurningAction`;
  phase 1 (0x501730) waits for the body action to end. No draw on this path (the agenda's `GameRand(7)` at 0x4FF086 needs
  a second function, which this sub-action has none of).
- **The tutorial's creatures** (`CreaturesInGlade`, answers 0-2): made by `CREATE` type 12 (`GScript::CreateThing`
  0x6F1B20, case at 0x6F149C) with no player, then `CREATURE_SET_PLAYER` 0x6F41C0 (+0xA4C, +0x1070, the name). Offered:
  the Cow (row 1), the Ape (row 0) and the Tiger (row 2) by (2233, 3152) (ip 44076-44096).
- **The home follows the temple.** `Creature::ProcessState` (0x472E1A..0x472EA5) rewrites the home +0x1200 / +0x1204 /
  +0x1208 on every step, with no state test, whenever the player, `player+0xA44`, the citadel (`player+0xA48`) and its
  heart (`Citadel+0x30`, a `CitadelHeart`) exist: the heart's `Game3dObject` (`Object+0x40`) special point #15 (vtable
  +0x1CC with edx = 15, `Game3DObject::GetSpecialPos` 0x63B040) in world space, x and z × 6553.6 truncated, altitude 0.
  So `SET_CREATURE_HOME` only lasts while there is no heart. On Land 1 point #15 of `b_first_temple.l3d` is the last of
  its 16 extra metrics, local (-9.318, 0, -20.437); with the heart at (1915.05, 2508.89) turned 36.0 rad
  (`CREATE_PLANNED_CITADEL` angle 36000 × 0.001) it is (1895.97, 2520.75), 0.87 m from the script's HomePos.
- **Drawn smaller in the pen.** `Creature::ShrinkDownIfNearCitadel` (its Mac name; W120 `fn_004EFCE0`, called only
  from `Creature::ProcessState` at 0x472F72) takes the real scale (`[+0x160]+0x6C`, what `GetScale` reads) and, when
  the owner's citadel heart is built (`CitadelPart::IsBuilt` 0x464AD0, vtable +0x890), the distance `d` from the
  creature to its home +0x1200 is at most 16 m (`GUtils::GetDistanceInMetres` 0x74CD70) and the creature is between
  the pen's walls, draws it at `0.22 + (real − 0.22) × (clamp(d, 14, 16) − 14) / 2` (constants 0x8D14C4, 0x8D14C8,
  0x8D1520) through `LH3DCreature::SetSize` 0x480530; elsewhere at its real size. The walls: θ0 = the heart's
  `GetYAngle` (+0x4C, radians as stored) + 3.83, θ1 = θ0 + 0.897598 (2π/7); with dx, dz the drawn creature (+0x78,
  +0x80) minus the heart's point in metres, inside is `cos θ0·dz − sin θ0·dx ≥ 0` and `sin θ1·dx − cos θ1·dz ≥ 0`
  (the heart's arms 0 and 1). The real size is never written, so it comes back as the creature walks out; no draw; no
  state. In the Creature Cave (`CreatureRoom::InitEngine` 0x787BD8) the creature's copy takes the real size. On Land 1
  the home is inside the pen (9.78 and 9.01; 0.87 m), so from turn 8 the creature is drawn at 0.22.

### What openblack does with the player's creature

- **The profile's file is a setting:** `--creature-file <name>` (`EngineConfig::profileCreatureFile`), default
  `C4ba71b36.erc`, the game folder's first profile's creature, so every run loads the same one; empty for a profile
  without a creature. `CURRENT_PROFILE_HAS_CREATURE` answers whether that file exists.
- **`LOAD_MY_CREATURE`** (`ECS/PlayerCreature`): nothing when the player leads a creature already (the leash
  system's `PlayersCreature`); else the file through the mind cache, its species, size, alignment and strength
  (`CreatureMindFileBody`), and `CreatureArchetype::Create` at the middle of the point's cell on the ground. The
  fatness starts as the species does (which known field of the file holds it is not checked). The creature faces π, as
  the script-made creatures do (inferred). The fizz is not ported. Our creation's draws are our mind's (the 40 initial
  desires), not the original's list above.
- **The natives:** `CALL_PLAYER_CREATURE` gives the creature the player leads; `SET_CREATURE_HOME` writes the leash's
  home (`LeashSystemInterface::SetHome`), x and z through the 16.16 fixed point, on the ground; `SET_CREATURE_DEV_STAGE`
  sets the mind's development phase, and the desires follow in the mind's next turn (the original at once);
  `DEV_FUNCTION` 2 is `SetKnown(Rope)`, 3 is `SetKnown(Evil)`, `SetKnown(Good)` and `SetLeashable` (+0x110C read as the
  creature its player leads); the other values are not ported; `CREATURE_IN_DEV_SCRIPT` sets `Creature::inDevScript`.
- **The home:** each creature turn starts with `FollowTemplePens` (`ECS/PlayerCreature`, called from
  `ECS/CreatureLoop`): while its player's temple is built, every creature's home is the temple mesh's pen point
  (`TemplePenPoint`, special point 15), through the fixed point and on the ground. `TempleCreatureHome` (where a
  knocked-out creature is carried) takes the same point; it took the mesh's first point before.
- **The pen:** `ShrinkInPens`, right after `FollowTemplePens`, writes the drawn scale to
  `components::CreatureDrawPose::scale` (`BetweenPenWalls`, `PenDrawnSize`), none outside a pen; `ecs::DrawnModel`
  draws a creature at it, its eyes, hair and footprints following ([Drawing between turns](#drawing-between-turns)).
  `Transform::scale` and `Creature::size` keep the real size, so gameplay and the state hash do not see it (the hand
  touches the drawn body, so it touches a smaller one there). The distance is taken from the creature's turn position, not its drawn one.
- **Debug window:** Debug > Windows > Creature spawner (`Debug/CreatureSpawner*`, raffclar's window on our creature
  systems, its pure parts in `CreatureSpawnerModel`): spawn, the creatures and the selected one's looks, audio,
  movement, hands, mind, learning, body, leash and fight; and at the top the player's creature (real and drawn size,
  phase, home, leash, desires and plan, needs, alignment, the hand), with "bring to the hand", "set stage" and the
  leashes. His file dialogs, rope drawing, leash posts and editor hosting are not ported. Closed, it does nothing.
- **The "creature" part of the state hash:** each creature's owner, species, leashable, inDevScript, body, size,
  development phase and home.

## Stand-ins and unverified constants

Values openblack uses where the original's are not known. Each is a stand-in until it is read in the executable.

- **The idle mind** (`CreatureIdleMind.h`), his constants that stand in for what the original's planner decides:
  `k_MinDesireShown` 0.2 (a desire is shown only when it is over all others), `k_ActivityLots` 4 (the idle activities
  weighed by what the creature has learnt), `k_ActOnNeed` 0.3 and `k_ActOnDesire` 0.3 (when a need or a desire beats
  everything else), `k_FaintSeconds` 10 (how long a fainted creature lies out cold). His other idle timings
  (`k_ShowDesireSeconds` 60, `k_FaceRepeatSeconds` 4.2, `k_SitSeconds` 10 + 5, `k_HangAroundDistance` 20, …) have no
  wiki backing either.
- **The mind:** his `MiracleMultiplier` table (17 species) and the planner's and learning's constants.
- **The eyes:** first blink 1000 ms, blink 200 ms, wait `random(5000) + 2500` ms (his; see [Random streams](#random-streams)).
- **Locomotion:** acceleration 12, top-speed margin 1.1, corner π/6, slope 0.6 within [0.3, 1.1], walk 8 and run 20
  per (0.875 × size + 0.25).
- **Physiology:** growth slope 8 (at most 2), growth ×3 while asleep, fat burnt below energy 0.5, youth exhaustion
  4 − 0.15 × age, warmth threshold 0.6 at 0.025 per degree, at least 50 turns of sleep.
- **Spells:** big ×1.8, small ×0.5555556, frozen colour #8CC8FF.
- **Fight:** arena 39 per size (at most 60), damage 0.05 / 0.03, AI chances 16 / 8 / 1, a block ends 1 in 8, a counter
  1 in 5.
- **Throw:** gravity 9.81 (to compare with our physics' gravity).
- **Leash:** hand slack 0.7 s + 22 and at most 3 s + 32; tied 1.5× within 180..360; pull 0.8; rope 40 nodes at 200 Hz.
- **Follow camera:** the clear-view search, 32 headings × 8 samples; his key, wheel and clear-view constants.
- **Start scale without the tables:** `CreatureArchetype::StartScale` gives 0.22 when `info.dat` is not loaded (his
  `k_UnknownStartScale`); with the tables it is the species row's `startScale`.
- **Height:** 15 per unit of size, as [engine-math.md](engine-math.md) gives it (`k_CreatureHeightPerScale`); his
  copies are pinned to ours by tests.

## Addresses of the original, moved out of the code

The ported code's comments describe behaviour in plain words; the original's names and offsets behind them are kept here.

| Address or name | What it is | openblack |
|---|---|---|
| `GCreatureInfo` +0x1E4..+0x370 (size 0x384) | the creature species row; offsets in [The creature tables](#the-creature-tables-in-infodat) | `GCreatureInfo` named fields, `static_assert`s in `InfoConstants.h` |
| `CreatureActionInfo` +0x0 / +0x4 / +0x8 / +0x98 / +0xC0 | strength gain, energy cost, exhaustion cost, desire, desire multiplier | `CreatureActionInfo` named fields |
| `CreatureInitialDesireInfo` +0x4C / +0x50 / +0x54 | `DesireDecay`, `InitialValueMin` / `Max` in bw1-decomp | `creature_mind` tables (`initialMax`, decay range) |
| `CreatureDesires::Initialise` 0x4DC100 | the 40 initial draws | `creature_desires::Create` (draw injected) |
| `CreatureDesires::FindWeakestDesire` 0x4DC7B0 | reads the desire's value array (set to 0 at start) | `creature_desires` |
| `CreatureDesires::RandomiseIncreaseTime` 0x4DC310 | cheat-flag jitter, not decompiled | none |
| MSVC `rand` LCG 214013 / 2531011, seed 0x913 | the mind file's name cipher | `creaturemind::MindFile` |
| `LH3DMist` constructor 0x7F9560, +0x84 | the mist's frame counter, `Random(0, 16) & 15` | (the cave, not ported) |
| `LH3DCreature` | the creature's drawn body and eyes | `CreatureBody`, `CreatureEyes` |

## openblack

- `src/InfoConstants.h`: the named creature tables. `src/3D/CreatureBody.{h,cpp}`: mesh names (`ParseMeshName` and
  `GetIdFromMeshName` return `std::optional`), loaded once by the creature-mesh loop in `Game.cpp`.
- `src/3D/SkeletalAnimation.{h,cpp}` (`openblack::skeletal_animation`, beside the unrelated
  `ecs::components::SkeletalAnimation`), with `FromMorph` for the morph parser's animations.
- `components/morph`: his parser, with the header's named fields (`duration`, `looping`, stride, displacement, hair
  groups, sound events) and the creature block that follows the animations in a `.cbn` (action points, leash bone,
  eyes, tattoo sites, voice bank). The block is read only when the header's first field is not 0: in `Data/CTR/hh.HBN`
  it is 0 (the hand's file), in the `.cbn` files 0x15, so the hand reads the same clips as before.
  `components/rawimage`: headerless `.raw` images.
- **Loaders** (`src/Resources/Loaders.cpp`): the mind loader reads through `Locator::filesystem` and never throws (a
  file that cannot be read leaves `ErrCantOpen`). `LoadCreatureRigs` walks `Data/CTR` at start-up, reads each `.cbn`'s
  `Creature` block into `CreatureRig` (`GetCreatureRigs()`, by `creature::GetRigId`) and preloads the skin meshes each
  rig names into `GetL3DFiles()` under `creature/skins/<name>`; nothing is read later. `Game.cpp` also loads
  `Data/Eyeball.l3d`, `Data/Eyelid.l3d`, `Data/C_Ape_Hair(a).raw` and the skin art. Nothing draws them yet.
- **Components and archetype**: the creature's components in `src/ECS/Components/` (`CreatureLocomotion` holds a
  `route_planner::RouteFollower` behind a pointer in place of his planner and route; `CreatureDrawPose` is ours, the
  per-frame drawn pose). `CreatureArchetype::Create` keeps our rotation (`affine::AngleY`) and creation index, puts the
  components on, draws the body at `DrawnScale` (size × 15 / rest height; the size alone without the mesh) and
  publishes `Teleported`. `SnapTurnStart` sets a creature's `fromPosition` / `toPosition` and drawn pose, looked up
  through the const registry. `ObjectMetrics` reads a creature's scale from `Creature::size`.
- `src/Creature/`: 38 pure modules (body, mind, movement, fight, leash, audio, panel, mode), beside the existing
  `CreatureMind.h` placeholder the loaders use.
  `components/creaturemind/`: the mind file library, linked into `openblack_lib`. `src/Audio/Engine/AnimEffectKeys.h`.
  `src/Camera/CreatureFollow.{h,cpp}`.
- Tests: `test/creature/`, one group `test_creature` (398 tests; one integration test reads saved minds from
  `OPENBLACK_CREATURE_SAVES` and skips without them). None uses the Locator or the game's data.
- **Systems, emplaced and not called yet** (`src/ECS/Systems/`, one `<X>SystemInterface.h` and one
  `Implementations/<X>System` each, all reached through `Locator`): animation, skin, hair, footprints, physiology (made
  with the game) and locomotion (made with each land). Nothing in the turn or the frame calls them; no registry signal
  or event is connected. What changed from his:
  - the turn length is `game_clock` (`k_TurnSeconds`, `MsPerTurn()`), not his time service;
  - the eyes blink on the turn and the CRT stream; the locomotion's run-away distance, fidget and mirror coin and the
    poo's yaw are `GameRand` / `GameFloatRand` ([Random streams](#random-streams)); the puke's 12 drops are kept by the
    physiology system as draw-only data (`GetPukeDrops`), no entity, scattered by a private seeded generator;
  - the skins are painted only from the mesh files the loaders preloaded, never read at frame time;
  - the footprints' April Fools' day reads an injected date, once per land (`Reset`); the only wall-clock read is in
    `Locator.cpp`;
  - locomotion walks on our `RouteFollower` (one per creature, made at its first move) and the `land_avoid` mask
    (`IsPosValid` at 7.1 for a destination and 7.05 while walking, `land_avoid::NearestValid` to put a stray creature
    back). The follower fills its obstacles through `route_plan_world::CheckSquareFunction`, which now tells a creature's
    follower (its plan's context) from a footpath's holder (no plan, unchanged): a creature avoids another player's map
    shield, and a dead tree only while it burns and no script controls it. His own planner, obstacle gathering and
    corner cap are not ported. The `Transform` moves once a turn (`creature_pose::CommitTurnPose`, through the map
    cells); between turns only `CreatureDrawPose` and the animation slots move. A creature put back on the land it can
    stand on publishes `Teleported`.
- **The other systems, emplaced and not called yet**: fight, leash, creature hand, mind (with its learning) and object
  actions, made with the game. What changed from his:
  - **the fight is stepped by the turn**: the animation clock, the moves the animations carry the fighters by, the
    blows' moments and the turns to face move on 100 ms at a time at the start of the fight's turn; the frame only
    draws the body between the turn's start and end (`creature_fight::DrawnTime`) and charges a held blow. Its coins and
    choices draw `GameRand`, a wound's texels u then v first; the camera does not fly to a fight while a script or the
    interface has it; the player is `creature::LocalPlayer()`;
  - the leash has no posts and no shake: those come with the citadel's leash object and the game's gestures; the keys
    are read once a frame ([The leash keys](#the-leash-keys)); `TakeOffHeldLeash` is the shake's effect, for the
    scribble gesture, which nothing calls yet. Its scans are const and
    in entity order; its tying sounds are the in-game bank's 148 and 149, in turn;
  - the creature hand keeps the hand's pose and the creature under it (his `Game` members); there is no stroking by
    command;
  - the mind's draws are `GameRand` / `GameFloatRand`, the 40 first desire draws `GameFloatRange` in desire order; its
    scans pass over things that are going and take the lower entity on a tie; night is the day / night clock's; water is
    found on the `land_avoid` mask;
  - the object actions play by the turn, and their moments (taking hold, knocking down, letting go, eating) come at
    turn time, the palm sampled in the actions' animations at the moment. Something eaten dies as any death goes
    (`life::Kill` for a villager or an animal, the dead list for the rest); a home struck takes a creature's blow
    (`abodes::OnPhysicalDamage`), a tree is felled, a thing is deleted. What is let go of goes back on the map where it
    is, drawn there at once; it does not fly yet. Animals are food and can be picked up (when a hand may hold them).
- **Creature spells** (`src/Magic/Spells/SpellCreature.{h,cpp}`): the creature miracles (MAGIC_TYPE 26..41) are our
  spell class tree's `SpellClass::Creature`, registered last. Its operations are the plain spell's but for two: cast on
  an object it is the plain cast, then, on a creature, `spell_creature::Receive` (held for the spell's time times the
  caster's tribal power, then the spell runs with no time limit until the creature lets it go; a spell of the same kind
  it replaces is closed down); its close-down is the plain one, then no creature's spell points at it any more. A
  creature miracle cast at a place does exactly what it did before as a plain spell; only the "class not ported" warning
  is gone. `spell_creature::ProcessTurn` moves each creature's spells on by a turn (creatures in entity order, through
  the const registry), applies each event through the pure `creature_spells::Apply` (the body value pulled, the freeze
  and fizz, the mind paused, the desire made dominant then least) and plays the sound action from the creature bank. It
  is not called yet. His `MagicSystem` creature methods are not taken.
- **The creature as a spell target**: `fire::traits::IsCreature` is true for a creature that is still there (the
  mourners pass on fire starters that may be gone); `PhysicsObjects::ObjectInfo` gives a creature its species' row, so
  it has the row's burn multiplier, defence multipliers and weight like any object. A creature takes no harm from a
  spell's damage or from burning yet: both reductions skip it (Pending). Explosions and tornadoes already leave
  creatures alone (`CanBeDestroyedBySpell`).
- **Throws**: what a creature lets go of goes into our physics objects, never his own flight. Thrown at a target, it is
  `PhysicsObjects::AddObject` with the creature as the thrower; put down, dropped, tossed aside or let go of by a
  creature that is gone, it starts as the hand's objects do (`from_hand::InitialisePhysicsFromHand`), but is thrown
  above a speed across the ground of 1 rather than 2 (`from_hand::IsThrown`, [physics.md](physics.md)). An object no
  body can be made for is put down where it is. `ecs::creature_physics` gives the creature its body when the physics
  wants one (a ball as tall as the creature, six points and eight faces, mass 1000, not moved by a hit) and its weight;
  `RegisterPhysicsHandlers` is not called yet, and the creature catch hook stays unset.
- **Creature audio** (`CreatureAudioSystem`, made with the game, not called yet): its animations' moments play
  through `Locator::audio`'s `PlayAnimationEffect` with the creature as a tracked owner, from the creature bank
  (`SfxBank::Creature`) or the species' own (`CreatureBank` of `VoiceBankStem`), at the distance from the listener; the
  ground is `sea_cells::GetSurfaceType`. The bank's own filters stand for his temple and wide-screen gates. Whether other
  players' creatures speak is the script's `creatureSound`. Each sound kept on the creature notes its bank and the
  channel it started on (none when filtered out or not started). The footsteps still leave prints.
- **Mimicry and the town hooks** (`src/Creature/CreatureDeeds.h`, `src/ECS/CreatureMimic.{h,cpp}`,
  `src/ECS/Events/CreatureMimicEvents.h`): four deed sites publish the player's deed, which the mind system takes
  through `PlayerDid`, for the player's own creatures only; three villager sites publish the town need a creature may
  share, with no subscriber yet ([Mimicry and the town hooks](#mimicry-and-the-town-hooks)).
- Still his and not ported: the renderer's creature and rope passes, the debug spawner, Creature Mode's wiring and the
  Creature Cave.

## Pending

- **The creature drawn between turns** ([Drawing between turns](#drawing-between-turns)):
  - The leash rope's collar end stays at the Transform, up to one turn's step ahead of the drawn neck. A draw-only fix
    would shift the rope's drawn points by (drawn collar - game collar) x i/n in the rope's drawing, never in the rope.
  - Whether the original clamps the creature's turn fraction to 0.99, as for the livings; ours goes to 1.
  - The drawn yaw: ours is lerped over the turn; whether the original turns it at a rate, as the villagers do.
  - The eyes may be placed twice: `PlaceEyes` builds world matrices and the eyes are drawn with the body's instance
    row, which the instanced shader applies after `u_model`. To check on screen; a fix would be its own commit.

- **The hand on a creature**: allies (`IsAllied` 0x64D5D0) once the game has alliances; the creature's Flags bit 0x400
  (byte +0x25 & 4) and mind +0xF60 / +0xFB4 in `ValidForLockedSelectProcess` (openblack's asleep and frozen stand for
  them); `GoIntoStateOfFinishingAction` (0x477B20) at the lock's start, `LH3DCreature::ReconnectToGame` (0x480730) at
  its end and `GetReadyForNetworkUnfriendlyEndLockedSelect` (0x476ED0): the 3D creature's fields are not ported.

- **The player's creature** ([The player's creature](#the-players-creature)):
  - `SET_FOCUS` on a creature is not ported: the creature is not turned to the camera at Land 1's turn 8. It needs
    script control in the mind (the scripted plan, the current action finished, the agenda cleared) and the π/8 test
    against our `TurnToFace`.
  - The fizz after `LOAD_MY_CREATURE` (sound 0x27 and the 3 s fade); its sound tag's own values are not read.
  - Which field of the mind file holds the fatness (the physique's unnamed floats), and whether the mind load
    (`fn_004E84A0`) sets the development phase.
  - `MoveToDevelopmentPhase`'s other steps (+0x11B4, +0x194, the agenda cleared) and its immediate effect.
  - `DEV_FUNCTION` 1 and 4-12; who reads +0x10C0 (`CREATURE_IN_DEV_SCRIPT`) and +0x1108; what +0x110C is beyond
    "the creature its player leads".
  - Whether `SET_POSITION` puts a creature in script state 4 as it does villagers and animals.
  - A land change: the original's creature goes with the player, so the next land's `LOAD_MY_CREATURE` does nothing;
    whether openblack keeps it across a land change, or makes it again from the file, is to be checked.
  - The tutorial's own creatures (`CreaturesInGlade`), which our runs do not reach.
- **The initial desires**: what the original stores in the drawn array and in +0x4C (decay and cap, as his code reads
  them, or initial value and decay, as bw1-decomp's names say). His `CreatureMindLearning.cpp:425` also builds desires
  at the midpoint of the range without a draw; whether the original has a draw-free set-up is unknown (it changes the
  draw count).
- **The creature's own eye blink**: stream, interval, and whether the high-detail villager's hold and squint are shared.
  Until read, the blink goes on the CRT stream, marked approximate.
- **The mind, plans, mimicry, fidgets, fight and poo draws**: no decompiled function, so "synchronised" is a rule, not a
  reading; each must be confirmed as the classes are decompiled.
- **The puke and the cave's smoke, spray and mist dome**: whether they are particle effects in the original, and with
  which `NET_GAME_TYPE`.
- **The temple leash**: the `TempleLeash` object with the heart, its posts and its CRT draws at creation, which no code
  of ours makes yet.
- **The leash rope in the citadel and while paused** (read 2026-10-08): the rope step (`Leash.cpp` `fn_00848970`,
  sub-steps of 0.005 s, `n = 1 - ftol(dt * -200)`) runs from `GInterface::UpdateAllLeashes` 0x5D9130, called by
  `PostDrawProcess` 0x5CEAB3 every frame with no inside test, so it keeps running in the citadel; only the drawing
  (`DrawAllLeashes` 0x54DF18, on the land path of `Process3dEngine`) stops there. Paused (dt 0) it still takes one
  0.005 s sub-step per frame with its ends pinned; the port's `UpdateLeash` runs ungated, but whether it takes that one
  sub-step at dt 0 is not checked. `GoInsideCitadel` 0x553E10 also turns the player's own leash off first (packet
  0x5B at 0x553F5C, handler 0x63D3CE → `GLeashStatus::SetOn(creature, 0)`), which the port does not do yet (with
  C34). Not read: when that packet applies while paused, interface state 0x15 (the drag rope), the step's physics
  (`fn_008487C0`, `fn_00848830`).
- **How the original picks a species' body meshes**: it does not walk `Data/CreatureMesh` as we do; our skip of names
  whose last part is not an appearance is a port decision.
- **Animation blending**: which angle decomposition the original uses when it blends a creature's variant animations.
- **The creature in the turn**: the original runs it inside `Living::ProcessLiving` ([villagers.md](villagers.md)); the
  port will first run it as one block after the living list (approximate), per creature later.
- **Route planning and walkable land**: his A* lattice is replaced by our route follower and `land_avoid` mask; his
  heading convention against the original's (3D angle + π/2, [engine-math.md](engine-math.md)) is unchecked.
- **The creature's radius**: read from the body in the original, not ported; his default is 5, and the locomotion's
  measure from the mesh's box (half its width plus length, halved) is his, approximate.
- **The creature's route** (`RouteFollower::SetDest`): the original's values for a creature are not known. The port
  passes the arrival ring's outer radius and inner radius as the plan's first two values, the creature's radius, and
  (distance + 1) × 5 as the longest route (the footpaths' factor), with 64 search turns a game turn and no obstacle hook.
  When the route ends with a re-plan in flight, or the destination is inside an obstacle, the creature stands.
- **The fight by the turn**: a blow's clock moves 100 ms at a time, and what goes past the end of an animation is lost as
  the next one starts, as in his code. A blow of 1000 ms at the speed of a blow (0.66) takes 16 turns (1.6 s) where
  his 30-frames-a-second play takes 46 frames (about 1.53 s), so a duel goes about 4 % slower; each blow still lands
  once. The original's own fight step is not read.
- **What a creature does to things** (approximate): the force of a creature's blow on a home (his 3.75 × size is
  dropped for the thrown-object damage), whether an eaten villager dies with a reason the town counts, and where the
  original takes a creature's palm. A dead tree it walks round only while it burns and no script controls it.
- **Water for a creature**: the sea is taken as the cells of the walkable mask it can't reach whose middle is at the
  sea's level, searched 8 cells round; the original's search is not read.
- **The desires' fields**: the port keeps the reading above (cap = +0x4C, decay drawn in [+0x50, +0x54]); a mind file's
  set-up draws nothing (it takes the range's middle, which the file's desires then replace).
- **Footprints on April Fools' day**: when the original reads the date is not read; his code read it at every print,
  the port once per land.
- **The hand on a creature**: the click threshold (his 1000 ms against our 225 ms grab press); the 0xE85 interact
  tooltip's owner rule (his: own awake creature only; check 0x5D7406).
- **Creature audio**: whether `SET_CREATURE_SOUND` 0 mutes only other creatures' voices (his scope) or all their
  events; his species fallback table; the creature music, heartbeat and Guidance ([audio.md](audio.md)).
- **Surface rules**: his snow at depth 27 and material type 0 extras, kept out of the shared `GetSurfaceType`.
- **Fights**: the wound type per blow (his guess), the tier-1 AI gap he notes, and the fight camera's side, which he
  says differs from the game's east view.
- **Leash gestures**: the shake gesture's real thresholds; the `AttachLeash` / `DetachLeash` / `FocusLeash` help
  events' trigger sites.
  - **The SCRIBBLE with a leash in the hand** (parked): `GInterface::ProcessPowerUpSystem` (W120 0x5CF300, Mac
    `ProcessPowerUpSystem__10GInterfaceFv`) has no body in bw1-decomp, and its step 4 in [magic.md](magic.md) lists
    only the held object and the most charged icon. Whether a leash held in the hand is shaken off there, and before or
    after the held object, needs that function read. Until then `TakeOffHeldLeash` is not called and the gestures are
    as before.
  - **The SQUARE_SPIRAL leash picker** (parked): raffclar's tree has none, and the wiki has no numbers.
- **The leash keys**: the original's key path (packets 0xF / 0x10, [audio.md](audio.md), not read): whether it acts at
  the next turn or at once, and whether it needs the creature leashable as the port does.
- **Leash natives, openblack's differences** ([What openblack does](#what-openblack-does-with-the-leash-natives)):
  - `GET_OBJECT_LEASH_TYPE` gives the leash picked for the creature, worn or not, as the original gives the leash
    picked at the player's temple (`TempleLeash`, -1 when nothing is picked). Still different: the port's pick lives
    on the creature, not the player's temple. Until something is picked (a leash put on, or changed with the keys) the
    port reports -1, as the temple's leash does with nothing picked (coordinator, 2026-10-08), though the keys would put
    the learning leash on; the original's start (the `TempleLeash` constructor, and when the heart creates it) is still
    not read, and comes with the temple leash (C34). The original gives 0
    for a player with no temple or heart, which the port does not check; the leash posts that pick at the temple come
    with the temple leash (C34).
  - `ATTACH_OBJECT_LEASH_TO_HAND` and `ATTACH_OBJECT_LEASH_TO_OBJECT` put the leash on through `WhyNot`, which refuses a
    creature that is not the one its player leads or that does not know the leash; the original's natives test only
    "a creature with a player" and then turn the leash on. Whether the original's `SetOn` (on path, 0x5E6F70) refuses
    anything is not read, so the refusal stays.
  - `TOGGLE_LEASH`: the port does nothing for a script player out of 0..8 (`ScriptPlayerToGamePlayer`'s range); the
    range checks in the original's conversion are not traced. Its creature checks (`fn_005D06E0`: vtable +0x2C,
    Creature +0x1110, the interface status's leash state) are not compared with `PressKey`'s.
  - The original keeps one leash state per player, not per creature: `IS_LEASHED_TO_OBJECT` and `SET_LEASH_WORKS` on a
    creature of the player that is not the one they lead act on the player's leash there, and on that creature's own
    (empty) leash here. `SET_LEASH_WORKS` while no leash is worn sets the original's flag but nothing here (the port's
    flag lives on the worn leash). Who reads the flag is not known.
  - The "has a player" test (vtable +0x1C) is not ported: which creatures have no player is not read.
  - When both things of 185 / 269 are creatures, the roles are taken from the CHL syntax (the creature pushed first);
    not checked in the x86.
- **Mimicry** ([Mimicry and the town hooks](#mimicry-and-the-town-hooks)): four deeds are reported (11, 16, 21, 33).
  - The follow-up hooks commit, with every number now read: the resource deeds 0, 2, 4, 7, 9, 43-45 (the pit's deed is
    already computed, its draw made, and dropped); the spell hits through `fn_004E9DF0` (1, 3, 5, 8, 17, 18, 20, 33, 34,
    42; needs `GMagicEffectInfo` +0x98 from `info.dat`); the landing deeds 32, 35, 36; BUILD_HOUSE 6 (the scaffold's
    owner); BREAK_ROCKS 37; DAMAGE_BY_THROWING 15 and the physics' own 16 (the thrown body as the thing).
  - Waiting on other ports: PLAY_WITH_TOY 41 (`IsToy` of `Ball` and `MobileStatic`), the disciples 22-31, MAKE_ARTEFACT
    14 (artifacts), SACRIFICE 40 (worship's sacrifice), the animal's THROW_IN_THE_SEA (who last dropped an animal).
  - What the original does with `magic` (`Creature::MimicPlayer` 0x4EA670 is not read), the per-action table at 0xCAB220
    (priority, flags, filled from `info.dat`) and the gate's steps 1, 2 and 6.
  - The name of `GInterfaceStatus` +0x128 (the header's `LeashStatus`, compared with a `GPlayer*`); physics flag +0x1D8
    & 4 (FROM_HAND), creature +0x1268, player +0x8E0 == 2; whether the disciple off-by-one is the original's bug or the
    `VILLAGER_DISCIPLE` enum is wrong; no cross-check against the Mac build.
  - Code comments to correct: `MapShield.cpp` (a creature mimic after the aggression slot: it is help-spirit guidance)
    and `VillagerSatisfy.cpp` (the creature's activity desire is always 0).
  - The animals' `HasSunk` availability test (`Living::HasSunk` 0x5ED375), missing from openblack's animal branch.
  - No test pins the PLANT_TREE site in `HandSystem::Replant`: it is reached only through a real hand system's end of
    physics for a tree (the hand's set-up, the terrain, the smoke and the spot visual). `Consider`'s own tests pin the
    deed's fields; the site's place is checked by reading.
- **The town hooks**: the mind's side of the empathy (0x4C80F0: the clamped add, the ×0.9995 decay per turn and the
  even-turn history), which the three published sites wait for; the worship-site calls (states 171, 9 and 7) and the
  spell's (`fn_00721730`); `Town::UpdateAttitudeToCreature` 0x7437F0 (step 16; the names of attitudes 0 and 2,
  `fn_004C9FE0`, mind +0xF50 / +0xF58, Creature +0x160, the actions at the Fear indexes; the help texts 1083-1087); the
  help-sample groups 0xD99D08 and 0xD99DA0 of the aggressor guidance; possible readers of the town-desire empathy
  array through registers or pointers (only the immediates 0x18C80, 0x18D20, 0x18DDC and 0x1A1DC were scanned).
- **Leash natives** ([Leash natives](#leash-natives)): no native body is in the decomp, everything is our reading of
  the x86. Unnamed: `fn_005D06E0`, `fn_005E6BD0`, `fn_005E6EA0`, `fn_005E7140`, `fn_004648E0`, `fn_00464920`; the
  meanings of `GLeashStatus` +0x18 / +0x1C / +0x24 and Creature +0x1110 are inferred from use; creature vtable slots
  +0x2C and +0x34 not checked (read from the error strings only); who reads `GLeashStatus` +0x18; the range checks in
  `ConvertScriptPlayerToGamePlayer` 0x6EB9A0 and `GGame::GetPlayer`; no shipped CHL `Enum.h`; PPC Mac bodies not
  disassembled.
- **The mind-file cipher**: whether saving a mind file reseeds anything (the `srand(time)` at 0x577721 is in
  `creature.lhp`'s export, not in the mind file's save; [engine-math.md](engine-math.md)).
- **Playground creatures** (`CREATE_CREATURE_FROM_FILE`, playgrounds and `comp.txt` only): they now start at their
  species' `startScale` and start body (not 0.3), their `Transform` scale is the drawn scale, and the script's type 0
  is taken as the Giant Ape. Whether the original draws a creature at size × 15 / rest height is inferred.
- **The creature's row in the map cells' type** (`map_cells::TypeOf`) is now the species' row, inferred.
- **Spells on a creature**: where the turn calls the creature spells (the port will call them from the creature's turn,
  approximate); a creature's own damage (`Creature::ReduceLife`, [magic.md](magic.md)) and its defence override, so
  that spells and fire hurt it (today both pass it by); the creature as a spell's creator (`MaintainSpell`,
  `UpdateSpellInfo`); the lightning bolt's creature test, a tornado carrying a creature, the three flames on a burning
  creature (their draw stream is not read) and the town's magic stolen by a creature. None has numbers yet.
- **A creature's body in the physics**: the original's is its bounding sphere with one point for each bone and its
  mass is read from the body; the port's is a ball as tall as the creature, mass 1000. Who catches what is thrown (the
  catch hook) and whether a creature's put-down really goes through the hand's start of physics are not read.
- **What a creature throws**: the thrower's player for the flying-object reaction (the hand's start gives the hand's).
- **Every constant in [Stand-ins](#stand-ins-and-unverified-constants)**, and the four unread blocks of `GCreatureInfo`.
- **Double click on a creature (the lock-on) and the camera help.** In `CameraModeNew3::Update` 0x45A960 the lock-on is
  part of the double-click flight: the branch needs camera feature 0x10 (0x45DDA3) and clears the interface's
  double-click flag (0x45DDE9). With a thing under the hand (`GInterface+0x3C8`) it first reports
  `CameraHelpCallback(0x306 DoubleClickObject)` (0x45DE3A, help event 31), then asks `IsCreature` (vt+0x34, 0x45DE4B),
  and after the flight's set-up builds `CameraModeFollow(camera, thing, 1.0, 0, 0)` (ctor 0x44B800, at 0x45E34B); a
  fight arena in range starts `StartFight` 0x45A4D0 instead (0x45E07E clears the follow). openblack's Creature Mode
  locks on only with feature 0x10, as the original, but the world camera still reports 0x305 DoubleClickPos (event 30)
  for every double click, on a thing or not: reporting 0x306 needs the thing under the hand (C29), and the fight arena
  is not ported.
- **The C key** (`ZOOM_TO_CREATURE`) does not yet respect the script interaction modes: `IsActionBlocked`
  (`src/Help/InterfaceInteraction.h`) would block it when camera moves or realm zooms are not allowed, but the action
  map does not ask it yet, so Creature Mode still locks on.
- **Drawing the creature (C31), inferred until a creature is drawn:** the hair's ribbon winding against the
  original's one-sided material (see [Drawing in the frame](#drawing-in-the-frame)); whether the
  creature's shadow is also drawn over objects; the shadow projected from the posed base mesh rather than the morphed
  one; whether its eyes and hair are drawn in the water's reflection; and, for a creature that morphs with the terrain,
  that the morph program (which has no height-map version) stands in for the height-map one. A hair with bit 2 of +0x24 switches FILLMODE to wireframe for 10 of every 20 strand draws (counter [0xEF7530],
  0x84757E..0x8475C2, put back at 0x8476C1..): which hair sets that bit is not known (a debug view, **(inferred)**).
- **The Creature Cave without a creature.** openblack's creature room shows its four scrolls blank and opens no cave
  screen while the player has no creature (as raffclar's port); what the original's cave shows in that state has not
  been checked.
