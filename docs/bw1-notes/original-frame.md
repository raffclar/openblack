# The original B&W frame (runblack.exe W120, D3D7, LH3D)

Map of a game frame of the original, with addresses: how the frame is launched, the draw order of each
stage, the 19 render modes (D3D states per L3D material type) and the global state (projection, haze,
clearing, levels of detail). The status of each stage in openblack is in [parity.md](parity.md) and the details of what has been
done in [rendering.md](rendering.md) (the world) and [rendering-objects.md](rendering-objects.md) (the models).

Everything is **faithful** (read in the executable) except what is marked **(inferred)**.

- [1. Frame loop](#1-frame-loop)
- [2. Draw stages in order](#2-draw-stages-in-order)
  - [Other cases: temple, video and 2D](#other-cases-temple-video-and-2d)
- [3. Render modes](#3-render-modes)
- [4. Global state](#4-global-state)
- [5. Comparison with openblack](#5-comparison-with-openblack)
- [Pending](#pending)

## 1. Frame loop

`GGame::Loop` 0x54CF20 (decomp `src/Black/Game.cpp`), on each iteration:

- ProcessGraphicsEngine 0x54D850 → [mouse, camera->Update (sets the FOV and the dynamic near plane),
  GInterface::PreDrawProcess 0x5CE9E0 (hand collision, disciple icon), **Process3dEngine 0x54DA80**,
  BMan/debug camera editor, GInterface::PostDrawProcess (only updates leash and collision, does not draw),
  HelpSystem::PostDrawProcess (flag)] → ScriptedScreenShot → **GGame::FlipScreen 0x54D800 → LHScreen::Flip
  0x7DE090** (timing text, software LHMouse::Draw, LHFlip 0x7DE580 and then the frame *clear*
  fn_0082EE70, see [4. Global state](#4-global-state)).
- Process3dEngine: LH3DRender::StartFrame 0x82F0E0 → video (Bink) → switch field_0x205a28 (0 = world,
  1 = citadel/temple, 2 = falling-spell video) → fade / help / influence → **LH3DRender::FinishFrame
  0x82F460** (Z-sort flush + callbacks + EndScene) → debug 2D (CreatureMentalEditor, leash info,
  DisplayHowImpressed, CPU players, countdown text) → LH3DAtmos::Render2D (debug weather map
  with Lock, **(inferred)**).
- StartFrame: fninit/FPU control, delta time (g_delta_time 0xC38134, smoothed fps 0xEC7FC0),
  **BeginScene** (vt+0x14), g_frame++, g_started_frame=1, **zsorter reset fn_0083F3B0**, fn_00813770,
  fn_0085BF00, SetLight/SetProjMatrix only with hardware T&L (never: start_system forces [0xC386E4]=1),
  fn_00821270.

## 2. Draw stages in order

Case 0 = world.

| # | Stage | Function | What it does / states | Order / culling |
|---|---|---|---|---|
| 1 | landscape.PreDraw | GLandscape::PreDraw 0x5E3F60 → LH3DIsland::PreDraw 0x7FF2D0 → fn_00877210 | list of visible blocks (32×32 blocks of 160), fog class per block +0x940 | frustum and near-plane culling with each block's box; list 0xFAA?/0xFA92D8 sorted by distance +0x9BC **front to back** |
| 2 | shadow textures | TemporaryShadow::UpdateAll 0x825190 (and fn_00874850 per SuperVillager) | silhouettes rasterised by the CPU into per-object shadow textures (fn_008801D0, **(inferred)**), list 0xFAA7E0 | — |
| 3 | preparation | LH3DCreature::PrepareForDrawing 0x4ED320 per creature, CHand::PrepareForDrawing 0x46C550, PSysLightMaps::AddDrawing 0x6CA6E0, LH3DLandscape::TextureUpdateThread 0x871F00 (block textures: footprints and decals fn_008721A0/fn_00872FA0), LH3DAtmos::Update3D | does not draw | — |
| 4 | GLandscape::Draw 0x5E42E0 | (detail in 4a–4o) | | |
| 4a | preparation | 3D cursor (Get3DPointFromScreen), fn_00802550, Windmill::PreDraw, Tree::PreDraw (swaying in the wind), fn_008296D0 (8 s fade timer, mode 0xF), fn_005E5830 (hand position; hand night light fn_00823460/fn_0086D360; clouds fn_005E25C0 with the Clouds key) | | |
| 4b | **sky** | GLandAlignement::DrawSky 0x5E2160 (not in wireframe) → fn_0086A330 → light table fn_00869850 + fog parameters, fn_0086B7F0 (sky_{good,ntrl,evil}_{day,dusk,night}.555 → 3 textures, mode 2) → fn_0086B010 | **moon** moon.l3d + additive glow quad 500 (AdditiveMaterial mode 13), and a copy mirrored in Y (reflection, **(inferred)**); **dome** sky.l3d with ZFUNC ALWAYS, 2 passes (texture of the base alignment with α255, then the 2nd alignment with global alpha (alignment−1)·255), darkened by the storm, towards white with lightning; **sun** sun.l3d fn_0086C140, colour 0x957C63, visible from 6 to 18 h, ÷(1+8·clouds). **No stars.** | the sky covers the whole screen (the colour buffer is not cleared) |
| 4c | **reflected land** (LandRef key) | fn_007FF4F0, ZWRITEENABLE off around it (0x5E48B3–0x5E4900) | heights ×−1, light table ×0.5, small bump forced off; land only, no models | mirrored block list |
| 4d | reflections in the sea | PetitNavire::PreDraw 0x5DFF20 (ships); hand DrawUnderWater vt+0x118 with specular 0x65A0A0A0 through the **alternative mode table 0xC387C8**; object in the creature's hand; fn_00646FE0 (physics objects), fn_00775120 (sharks, cut by the water plane, 0x5E4B26), fn_00824B90 (FishFarm fish and the fish puzzle nets), fn_005DFCE0 (footprint UV2, **(inferred)**) | before the sea. The hand, what it holds, the physics objects and the ships are **reflections**: `DrawUnderWater` draws them mirrored in y = 0 and clipped to what is above the water (see [rendering-objects.md](rendering-objects.md#object-reflections-and-hand-shadow-on-objects); it had previously been assumed they were their parts under the water); the FishFarm fish are sprites under the water | |
| 4e | SuperVillagers | list 0xEB9A08: Draw vt+0x610 + shadow fn_00874850; swimming villagers generate water rings | | |
| 4f | hand night glow | fn_005E3F70: additive quad on the ground of ±60, ZFUNC ALWAYS | | |
| 4g | **sea** | fn_00879930 (skipped if [0xECA664] or in wireframe): sky.raw/skya.raw mode 5, rows of 2 px, ZFUNC ALWAYS, no Z write, colour = light table[255], alpha 255→80 between 7000 and 14000, period P = 2000−1800·WaterTiling | | |
| 4h | vortex | fn_005FF310 → fn_005FFBB0 (LandscapeVortex, **(inferred)**) | | |
| 4i | **land** | fn_007FF610: per visible block front to back, transition blocks fn_00877D20, block fn_00874AA0 / SSE fn_007A1800 (blocks in mode 14, per-vertex light table, software haze, 2nd small bump pass in mode 14), then **projected dynamic shadows** per block fn_00878350 (shadow material mode 6) | front to back |
| 4j | debug | fn_0081F820: field-of-view triangles of the scripts | | |
| 4k | citadel heart | fn_00467360 → CitadelHeart::DrawNow 0x4670D0 | | |
| 4l | water rings | fn_005E5100 over 1024×0x38 at 0xEAB7C8 (GWater circle sprite, direct LH3DSprite::Draw) | | |
| 4m | **models** | fn_005E5CD0: object draw list (≤3000, GLandscape::DrawObjects 0xD1D28C) of the visible blocks (dist < VanishObjectDist 100000) + global list; it is rebuilt according to DrawListRebuildCount / 10 turns / land change; with the camera still only the last objects on screen are redrawn. Draw of an object (e.g. MobileObject::Draw 0x518150): light fn_00801C90 ([rendering-objects.md](rendering-objects.md#model-lighting)), fog fn_007FEB30, Game3DObject::AddForDrawing 0x63B5D0 → LH3DObject::AddDrawing fn_00815A70: frustum CheckRegionOnScreen 0x868C80, **LOD** by distance (23.3f/66.7f/86.7f → LOD 1/2/4, fade 86.7f–173.3f, culled beyond that if IsDisappear; distant humans → impostor sprite; **inactive in this executable**: the LevelOfDetail loads are disabled, always LOD 1, see [rendering-objects.md](rendering-objects.md#villager-blobs-object-reflections-and-lod)), IsGlowing → glow sprite, **NeedSorting (mesh with alpha, flag 0x200) → Z-sorter; otherwise immediate Draw** | opaque: block order, unsorted; with alpha: Z-sorter |
| 4m′ | the creature, in the list | Creature::Draw 0x517910 → LH3DCreature::AddForDrawing 0x48E1C0 → DrawNow 0x48E260 | body (fn_00813340: at once; the Z-sorter only with the mesh flag 0x200 or global alpha, which no creature has), the shadows over it, its hair, then its eyes at once ([creature.md](creature.md#drawing-in-the-frame)) | at once, in the list's order |
| 4n | rest of 5E5CD0 | SuperVillagers list 0xEB9A10 (OverrideMaterial with forced alpha-ref 0xA), game list vt+0x610, markers, Reward sprites, PetitNavire::PostDraw, hand_intro, sprite emitters fn_008274A0/fn_00823570/fn_00827B90/fn_00828E50 | | |
| 4o | end | storage pit / LandFeature::DrawWorm | | |
| 5 | creature fight sparkles | LH3DCreature::DrawFightSparkles 0x48DD70 | | |
| 6 | leashes | GInterface::DrawAllLeashes 0x5D9310 (fn_008491B0; leash material mode 15) | | |
| 7 | physics objects | PhysicsObject::DrawAll 0x646DE0 (AddForDrawing, DrawOutOfMap) | like the models | |
| 8 | hand | CHand::UpdateHeldObject; **CHand::AddDrawing 0x46D100 → Z-sorter** (light ×1.5); objects in other players' hands with DrawInHand | Z-sorter | |
| 9 | interface | GInterface::Draw 0x518640 → fn_005FAF80 (magic hand), state vt+0x500 (gesture trail, etc.; gestures in mode 13) | | |
| 10 | liquid particles | DrawLiquidParticles 0x845C50 → Z-sorter | Z-sorter | |
| 11 | debug overlay | GGame::Draw 0x5533B0 (only if g_game->field_0x14 & 0x4000: alignment and belief bars) | | |
| 12 | miscellaneous | CreatureLessonChooser::UpdateDraw, EditorIconBase::DrawMouseOver | | |
| 13 | particles | PSysGlobal::DrawLoop 0x68F5E0 (PSys managers → PSysManager::AddDrawing 0x6797D0 → Z-sorter), FireFly::DrawAll, Spell::DrawSpells 0x7203F0, GParticleContainer::DrawParticleContainers, GPlayer::DrawPlayers (player sparkles, blobs.raw mode 13), TownCentre::DrawAll (DrawPSys) | Z-sorter | |
| 14 | counters | ValueSpinner::Update/AddDrawing (not during the help cinematic) | Z-sorter (**(inferred)**) | |
| 15 | debug | LH3DStorm::DebugDrawAll, LH3DAtmos::DrawWindField | | |
| 16 | **weather** | LH3DAtmos::Render3D 0x836250: per storm (list 0xFA92D8…, storms at dist < 560) and per 80×80 tile, intensity from GetWeather at the 4 corners; if >5: rain fn_008341B0 → Z-sorter callback 0x833F80 (streaks, AtmosMaterial mode 6 atmos.raw, splashes g_water_drop_cb, amount according to the RainSplash key); snow fn_00834290 → callback 0x834120 (snow.raw mode 9). Lightning: LightSheet::DoTheDrawing 0x83E8C0 (mode 13) via GLightSheet::Draw → Z-sorter. Rain colour = (light table base/2 & 0x7f7f7f)+0x7f7f7f. | Z-sorter by the tile's distance² | |
| 17 | camera force field | ForceField of CameraModeNew3 (only with the flag) | | |
| 18 | villager names | VillagerName::AddDrawing → Z-sorter | | |
| 19 | fade | GScript::ProcessFade 0x6EB9D0 or Temple::UpdateFade 0x794280 → sets the screen fade colour [0xFA51D8] (drawn in FinishFrame) | | |
| 20 | 3D help | HelpSystem::Draw3D 0x5C59A0 (help character, arrows) | | |
| 21 | ClearLight 0x5E57B0 | removes the hand light (fn_00822F90/fn_00823780/fn_0086D460) | | |
| 22 | power spins | PowerSpinRunner → Z-sorter (PowerSpin::Draw) | | |
| 23 | influence ring | InfluenceCircle::Draw 0x826C90 (in the world only if the camera is more than 100 high: alpha 0→120 between 100 and 200; mode 6, no culling, wrap, UV offset 0.0001/−0.0002 per ms), Draw3DWorldTriangle **immediate** | | |
| 24 | **FinishFrame 0x82F460** | (a) **Z-sorter flush fn_0082F280** back to front (key = dist² to the camera, max. 2048 entries); (b) "before" callbacks (ascending priority, 0xEC8130): FallingSpell (0x526480), HelpDude 0x5C2E30 (100), CameraModeNew3 0x4562E0 (1000); (c) fn_0086BB60 sky post-process (sun and lens flare, **(inferred)**); (d) **Z-reset quad**: full-screen quad FVF 0x1C4, z=1, rhw=0, colour 0, material [0xEDD494] mode 1, ZFUNC ALWAYS → Z=1 over the whole screen; (e) cinema bars if [0xEB9950] (2D rectangles queued fn_0081E590, height fn_0081E8B0); (f) "after" callbacks: HelpDude 0x5C2E10 (100), **flush of the 2D rectangle queue 0x81E7D0→fn_0081E3C0 (10000; mode 1, ZFUNC ALWAYS, no Z write)**, HelpText 0x5CD020 (20000: help text boxes), LHVideoPlayer::thedraw 0x844E30 (0x8000), start_system 0x6424E0 (0xA0000); (g) fn_00836200 debug weather; (h) **screen fade fn_0086FEE0**: full-screen quad of colour [0xFA51D8], mode 1, ZFUNC ALWAYS, if alpha≠0; then the bar rectangles and the fade reset; (i) EndScene (vt+0x18) | | |

### Other cases: temple, video and 2D

- Case 1 (citadel/temple): Update3D, LH3DSky::g_b_we_are_inside_citadel=1, TemporaryShadow::UpdateAll, DrawSky
  (no sun, hence no glare at the end of the frame), Temple::Draw 0x794370 (its own lights with fn_0081E1F0 SetLight,
  rooms; Citadel* detail keys), Temple::Update, liquid particles. Details in
  [rendering.md](rendering.md#the-temple-interior).
- Case 2: FallingSpell video + liquid particles.
- Video: LHVideoPlayer::DrawToScreen (16:9 bars, alpha fade; FallingSpell 80).
- HUD and 2D: B&W has no classic HUD; the 2D consists of the help text boxes (callback), tooltips and text with
  GatheringText (fonts in mode 6), queued rectangles, fade, bars and the software cursor in Flip
  (**(inferred)**: the cursor is usually the 3D hand).

## 3. Render modes

Table 0xC38728, 19 entries {fn, flag}; the L3D material type is the mode index.

Common to all: colour = TEXTURE×DIFFUSE; ALPHAFUNC GREATEREQUAL set only once (0x82CBA6); ALPHAREF = mat+4 (or the
forced one [0xECA65C] if [0xECA658]); bit 0 of byte +5 of the material → cull NONE, otherwise CCW; bit 2 or g_b_need_tilling →
WRAP, otherwise CLAMP; mode cache [0xC38718] (reset to 0x14 every frame). No mode touches fog, specular,
lighting or stage 1.

| # | fn | States | L3D type / uses |
|---|---|---|---|
| 0 | 82D470 | untextured, opaque, writes Z | Smooth |
| 1 | 82D5C0 | untextured, SA/ISA, writes Z, stage 0 untouched | SmoothAlpha; 2D rectangles, fade, Z-reset quad |
| 2 | 82D820 | textured, opaque, α=tex | Textured; sky domes |
| 3 | 82D920 | SA/ISA, α=tex×diff, writes Z | TexturedAlpha |
| 4 | 82DC20 | SA/ISA, α=tex, writes Z | AlphaTextured (buildings, hand; the wrist fades out with the alpha) |
| 5 | 82DD90 | SA/ISA, α=tex×diff, writes Z | AlphaTexturedAlpha; sea |
| 6 | 82DF10 | like 5, no Z write | …AlphaNz; fonts, influence, human_shadow, dynamic shadows, atmos rain, video |
| 7 | 82D6F0 | untextured, SA/ISA, no Z write | SmoothAlphaNz |
| 8 | 82DAA0 | like 6 | TexturedAlphaNz |
| 9 | 82E080 | SA/ISA **+ alpha test**, α=tex, writes Z | TexturedChroma (trees 0x96), snow |
| 10 | 82E830 | additive SA/ONE + alpha test, α=tex×diff, writes Z | …AdditiveChroma |
| 11 | 82E9C0 | like 10, no Z write | …AdditiveChromaNz |
| 12 | 82EB50 | additive SA/ONE, writes Z | …Additive |
| 13 | 82ECD0 | additive SA/ONE, no Z write | …AdditiveNz: gestures, lightning, moon glow, sparkles, fire, smoke |
| 14 | 82DD90 | = 5 (flag 0) | land blocks, small bump |
| 15 | 82E470 | SA/ISA + alpha test, α=tex×diff, writes Z | TexturedChromaAlpha; leash |
| 16 | 82E6A0 | like 15, no Z write | text cache |
| 17 | 82D820 | = 2 | — |
| 18 | 82E2A0 | Z only (ZERO/ONE) + alpha test | ChromaJustZ; vortex, fizz |

Alternative table 0xC387C8 (object fade / DrawWithGlobalAlpha, hand reflection with DrawUnderWater):
0, 1 → D5C0; 2, 3, 17 → D920; 4, 5 → DD90; 9 → E470 (ref scaled by the object's alpha); the rest the same. Flags word:
nobody reads it ([rendering-objects.md](rendering-objects.md#render-modes-and-materials-render_modes)).

## 4. Global state

- **Projection.** No hardware T&L: CPU transformation to XYZRHW. Horizontal FOV [0xEA1DD0], default 70°
  (1.22173), aspect width/height [0xE839EC]; sz = 1−near/z, rhw = near/z, **no far plane**. Near plane
  [0xE839E0] dynamic according to the camera's height above the ground: 0.3 + 0.16·h clamped to [0.3, 3.5] (0.1 on the
  path cameras, 0.2 in the citadel with fov 90°).
- **Fog.** No D3D fog. Software haze (Fog key [0xC37204]): start and end according to the sky type
  (noon/midnight 400→900, dusk 100→800; storm →15/350), colour = light table base/3; per vertex
  it darkens the diffuse towards "dark" and adds the fog RGB to the specular (**(inferred)**); land per block +0x940,
  objects fn_007FEB30.
- **Clearing.** In Flip, after presenting, only every ~2000 ms (2 frames) in game: black 0xFF000000, z=1. The Z
  is reset every frame with the FinishFrame quad; the colour buffer depends on the sky.
- **Gamma and filtering.** No gamma. LightBoost (only in custom detail) changes the light table divisor.
  Bilinear filtering, no mips, no AA, dithering enabled ([rendering.md](rendering.md)).
- **Level of detail.** fn_00823AD0 from start_system 0x643026; level = detailidx from the registry (<5), otherwise **4**;
  5 = custom (fn_008237B0 reads each key). Table 0x9A3704 (L0..L6):
  - Live: LevelOfDetail (dead), UseSmallBump 1 in all, Clouds/CloudShadows 0001111, WaterTiling
    0/.2/.4/.6/**.8**/.5/1 (level 4 → sea period **560**; 200 only in L6), LandRef 0001111, CitadelReflections
    0000111, CitadelLightmaps 0111111, CitadelGlows/People 0011111, CitadelVolumeLight 0001111, RainSplash
    0,0,3,5,8,8,8, LightBoost 0, Fog 0001111.
  - Only at startup: Weather 0001111, Light 0011111 (object dynamic light flag 0x20), ShadowsOnObjects 0001111
    (flag 0x40), UseHighTexture 0000111 (land textures of 256 versus 128 px), UseMultiLayerOnLandscape
    inverted (1 only in L0), FixeLand.
  - HardwareTnL/MaxObjectDistance are read but ignored. The land detail distances 0xE9C508 depend
    on the number of textures in VRAM. VanishObjectDist 100000 (script).

## 5. Comparison with openblack

The up-to-date parity table is [parity.md](parity.md). The list that was here (2026-09-29) has been removed because a good
part of it was no longer true: it listed as absent in openblack the sun and the moon, the clouds, the object reflections in the sea,
the haze, the dynamic shadows, the particles, the fade and the bars and the water rings, and as different the
FOV, the order of transparents and sprites, the LOD and the TexturedChroma alpha test; today all of that is done (identical
or approximate to the original) according to parity.md.

The only harmless difference remains the same: openblack clears the colour to 0x274659 (the original does not clear it; it is not visible).
What is missing according to parity.md: the drawn snow and the raindrops on the ground; leashes and gestures; the Bink
decoder; the creature (reflections and shadows).

Original data that was only in that list:

- TexturedChroma = alpha test ≥0x96 **and** SA/ISA blending.
- The dynamic shadows fall on the land and, with ShadowsOnObjects, on the objects.
- Footprints and decals baked into the block textures.
- Clouds and cloud shadows from detail level 3.

## Pending

- Stage 1: the address of the visible-block list is only known as "0xFAA?/0xFA92D8".
- Stage 4d: `fn_005DFCE0` (footprint UV2) and stage 4h (`fn_005FF310` → `fn_005FFBB0`, LandscapeVortex) are
  **(inferred)**.
- Stage 24 (c): `fn_0086BB60` as the sky post-process (sun and lens flare) is **(inferred)**.
