# Model rendering: original versus openblack

How the world's objects are drawn: L3D materials, model lighting, textures and sprites, foot blobs,
the single transparent queue, sea reflections and cuts by the water plane, fish shoals, object and hand shadows, LOD, chimney
smoke, objects that face the camera (billboards), frame-animated textures, meshes stuck to the ground and the original's render modes and materials. World rendering (terrain, sea, sky, fog) is in [rendering.md](rendering.md); water
as gameplay, in [water.md](water.md).

- [L3D material blending](#l3d-material-blending)
- [Model lighting](#model-lighting)
- [LH3DColor arithmetic](#lh3dcolor-arithmetic)
- [Texture wrapping or clamping](#texture-wrapping-or-clamping)
- [The single transparent queue (LH3DZSorter)](#the-single-transparent-queue-lh3dzsorter)
- [Villager blobs, object reflections and LOD](#villager-blobs-object-reflections-and-lod)
- [Physics and LOD 0 submeshes](#physics-and-lod-0-submeshes)
- [The under-sea pass (`graphics::sea_pass`)](#the-under-sea-pass-graphicssea_pass)
- [Object reflections and hand shadow on objects](#object-reflections-and-hand-shadow-on-objects)
- [Cutting by the water plane (`DrawCutByPlane`)](#cutting-by-the-water-plane-drawcutbyplane)
- [Fish-farm fish shoals](#fish-farm-fish-shoals)
- [Shadows of physics objects](#shadows-of-physics-objects)
- [Dynamic hand shadow](#dynamic-hand-shadow)
- [Animals: blobs and mesh](#animals-blobs-and-mesh)
- [Chimney smoke (LH3DSmoke)](#chimney-smoke-lh3dsmoke)
- [Objects that face the camera (billboards)](#objects-that-face-the-camera-billboards)
- [Frame-animated textures](#frame-animated-textures)
- [Meshes stuck to the ground (land_morph)](#meshes-stuck-to-the-ground-land_morph)
- [Render modes and materials (render_modes)](#render-modes-and-materials-render_modes)
- [Test hooks](#test-hooks), [Pending](#pending)

Status: everything on this page is **faithful** (read in the original) except what is marked **(approximate)**, the
deviations stated in each section and what is in [Pending](#pending).

> **Code rules.** State lives in ECS components (`src/ECS/Components`) or Locator services, meshes and textures load
> through the resource caches, colour and lighting rules stay pure `constexpr` functions tested with fakes in
> `test/`, and comments describe behaviour in plain English, with no decompiled names or addresses (those belong
> here).

## L3D material blending

**Faithful**.

- `L3DSubMesh` translates the material type into `blend`/`depthWrite`/`thresholdAlpha`, but the renderer did not use
  `blend`: everything came out opaque. Now materials with blending and no alpha cut-out (`AlphaTextured`, `TexturedAlpha`,
  `SmoothAlpha`, `*Nz`, additives without chroma) are drawn with the texture's alpha (`u_skyAlphaThreshold.w`),
  `SRCALPHA/INVSRCALPHA` (or `SRCALPHA/ONE` for the additives), without writing Z for the `Nz` ones, in the `MainBlended` view
  (after everything opaque). `TexturedChroma` keeps the alpha test. The states of each type come from
  `render_modes` ([Render modes and materials](#render-modes-and-materials-render_modes)).
- The hand (`Hand_Boned_Base2`, material `AlphaTextured`) has an alpha gradient in the bottom rows of its skin:
  the wrist fades out. It used to end in a hard white edge ([img/hand_zoom.png](img/hand_zoom.png)).
- Meshes in `AllMeshes.g3d` by material type: `Textured` 512, `TexturedChroma` 217, `Smooth` 181,
  `AlphaTextured` 116 (almost all the `MSH_B_*` buildings), `TexturedChromaAlpha` 6 (bushes, palm trees).
- Test hook `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fixed at a fraction of the window (the hand appears in captures
  without a real mouse).

## Model lighting

**Faithful**.

- **A single rule, integer and on the CPU** (`fn_0084BA90`, D3DTLVERTEX vertices), the one for all normal models
  (buildings, villagers, trees, rocks): `I = fistp(255 · N·L)` (0x84BBAF..0x84BBBE, round to nearest with
  ties to even, the FPU's normal mode), `f = I < 0 ? amb : amb + ((255 − amb)·I >> 8)` (0x84BBC3..0x84BBE5) with
  `amb` = [0xC39264] = 90, and the per-channel diffuse `(c·f) >> 8` truncated, with alpha untouched (0x84BBEA..0x84BC1D). So
  `f` reaches at most 254/256, never 1. Variant with `__ftol` truncation instead of to-even: 0x859649
  (`fn_00859530`, `fn_00859D90`, `fn_00878C70`).
- **The float formula from 166 is never executed** in this build: it is the D3D path `fn_0082C680`, which is only reached by
  `LH3DObject::DrawTnL`, and `DrawTnL` requires the hardware T&L flag [0xECA60C] (0x80DC7A), which `OpenD3D` only sets to 1
  if [0xC386E4] is 0 (0x82D0F5); `start_system` writes 1 to it unconditionally (0x642EA7). That is why openblack no longer has it.
- Object base colour (`fn_00801C90`), one per object and frame: `table[brightness]` of the 4 cells around
  its origin, bilinear; specular = RGB of those cells read as D3DCOLOR (R and B swapped; almost always 0).
- **The light is a point and it moves with the time of day.** LH3DTech stores a single light, [0xEA9E90] (`SetLight` `fn_0081E1F0`;
  saving and restoring is up to the caller, 0x8254A3/0x82551F). `fn_005E5830`, called by `GLandscape::Draw` (0x5E488E)
  once per frame before the models, leaves it at the default sun [0xEA1C88] = (−500000, 500000, −500000)
  (`__xc_a` initialiser `fn_00818920` 0x818930) except in **full night**: if the sky type is > 1.5 (the double at
  [0x8C5838]; `LH3DSky::Time2SkyType` 0x86A1B0 of the visual time computed there, 0x5E58D1..0x5E58DF, 2 = night;
  openblack: `sky_type::At(visual hour)`) it puts it 3 units ([0x8C2C50]) from the
  **hand** towards the camera, with the hand raised to at least 10 ([0x8AB414]) above the terrain beneath it.
  - At start-up `fn_00818950` (0x818960) copies the default sun into the light [0xEA9E90].
  - `fn_0081E1F0` writes the position at 0x81E1F6..0x81E20D and returns it through the hidden pointer of its first
    argument (0x81E2E6).
  - `fn_005E5830`: the focus is lifted at 0x5E58AC..0x5E58C6 (the 10 is used at 0x5E58B1); the sky-type test
    0x5E5A6C..0x5E5A77 (`test ah, 0x41` / `jne`) sends "not greater" **and the unordered case** to the day branch;
    only an exactly null focus→camera vector is left unnormalised (0x5E5AA9..0x5E5AD6); the 3 is used at 0x5E5B12. The
    focus is the hand model's position even while the hand is hidden.
  - A SuperVillager's HD body and its eye meshes are drawn with the light at the default sun (`fn_008254A0`
    0x8254C3..0x8254D1, restored 0x82551F).
  - With the cursor off the terrain (in the sky) the original keeps moving the hand along the mouse ray at its
    distance from the view (`ObtainRequiredHandPosition` 0x5B5E70; `CHand::fn_0046DF60` keeps
    |camera − position| if the ray does not touch land). In openblack `HandSystem::Place` leaves the hand where it was last
    placed (or at its initial spot), so at night the light stays there and the faces of distant models can
    end up with almost only the ambient: **known difference** (pending in the hand system). To test night
    lighting, put the cursor over the terrain (`OPENBLACK_MOUSE_AT`). Without a hand or without a camera, `Renderer::DrawScene`
    leaves the default sun (the daytime branch, 0x5E5B70) **(inferred)**.
- **N·L is done in mesh space, not with the rotated normal**: `fn_00855340` brings the light into the object's space with
  the general inverse of its matrix (`LHMatrix::SetInverse` 0x7FB290) and normalises it (0xF03140); the boned branch does
  `SetInverse` of each bone matrix (0x84BD9E) on the light already transformed to camera space (0x84BDA3), which is the same per
  bone if those matrices go from the bone to the camera and the camera cancels out **(inferred)**. The vertex normal goes in
  raw, neither rotated nor normalised, and the direction comes from the **origin** of the bone or the object. With uniform scale it gives the
  same as rotating the normal; with per-axis scale (swaying a tree, the shear of a field) it does not.
- The plane cut (`DrawCutByPlane`, `fn_00858BA0`) uses the light [0xF03140] in **object** space: the two
  callers set it once with `fn_00855340` on obj+0x14 (static 0x80C0EE, animated 0x811D2F) and `fn_00858BA0`
  reads it in both its branches (rigid 0x858CB1, boned 0x859049); the bone matrices (0x858F77) only move the
  positions. `vs_object` mode 4 uses only the instance matrix.
  - LightInMeshSpace: SetInverse at 0x855349, the light as a point through it (0x85534E..0x8553D3) and
    InverseSquareRoot at 0x85540A; the boned branch 0x84BD82..0x84BDFE.
- Mists and clouds raise the ambient to 210 while they are drawn (`fn_007FA300` 0x7FA56D, back to 90 at
  0x7FA586; the light is saved and raised at 0x7FA53C..0x7FA563 and restored at 0x7FA590). "Unlit" objects go
  through `fn_00856D40`/`fn_0085BA30`, with the base colour [0xC37D8C] as is (0x856D89, 0x85BA68): the selector is the
  edx argument of `fn_0080D910` (0x80D926 `test edx, edx`: `fn_0084BA90` if non-zero, `fn_00856D40` if 0); that
  this edx comes from slot vt+0x5C is **(inferred)**. Primitives with bit 0x1000 (with [0xE9FE44]) do not go
  that way: they take the alternative colour [0xC37D98] (`fn_0080AD90` 0x80ADBC, `fn_0080AF80` 0x80AFAC) and are lit in
  `fn_00859530` (the rule with `__ftol`); what that bit is remains **(not verified)**.
- The hand: base × 1.5 (`CHand::AddDrawing` 0x46D135). Untextured primitives: material colour × base. Chroma:
  `ALPHAREF` = the material's threshold, `GREATEREQUAL`; only modes 9 / 15 of an object with its own alpha use
  `threshold · object alpha / 255 − 5` ([Render modes](#render-modes-and-materials-render_modes)).
- Static shadows do **not** follow this light: `fn_008721A0` (0x8721E1) and `fn_0080ECB0` (0x80EDA8) read [0xEA1C88], the
  fixed sun, so at night they keep the daytime one (`vs_static_shadow_instanced`).
- openblack: `LandIsland::CreateCellMap` (RGBA texture per cell: rgb = colour read as D3DCOLOR, a = brightness);
  `vs_object` does the bilinear filtering and the lighting. A single system with a single API: `src/Graphics/ModelLight.h`
  (`model_light::Light/SetLight/ScopedLight`, `Ambient/ScopedAmbient`, `UpdateFrameLight` = `fn_005E5830`,
  `LightInMeshSpace` = `fn_00855340`, `Apply` for the CPU paths) and its GPU twin
  `assets/shaders/model_light.sh` (`ModelLightI`, `ModelLightFactor`, `ModelLightDiffuse`, `ModelLightLocal` and the
  uniform `u_modelLight`: xyz the light, w the ambient). It is used by `vs_object` (objects, PSys mesh atoms and the cut
  mode) and `vs_cloud` (clouds and mists). `Renderer::DrawScene` calls
  `UpdateFrameLight` once per frame and `ECS/Trees.cpp` uses `model_light::Light()`.
- `model_light::Intensity/Factor/Apply` on the CPU: the pieces of the explosions (`mesh_pieces::AppendPiece`, was `gj_mesh`) and the
  FragMesh (below, with `TwoSided`); the path of the primitives of `fn_00859530` (the `__ftol` variant) is still
  missing. `Renderer::DrawCloud` uses `ScopedLight` + `ScopedAmbient(k_MistAmbient)` and passes `Ambient()` to
  `u_cloud.z`.
- **Broken buildings and their fragments: per-face light, two-sided, on the CPU** (**faithful**). `FragMesh::Draw`
  (`fn_007F7960`) calls `fn_007F7ED0(matrix, position, pass)` for each primitive; three sites call it and there are only
  two object classes: the damaged house (`Abode::Draw` 0x5160E9: the DestructionMesh +0x90, **no matrix** because its
  triangles are already in the world, and the position of its LH3DObject +0x40 + 0x38) and the fragment (`Fragment`
  +0x94: `Fragment::Draw` 0x76EC2A with `GetWorldMatrix` and its translation, and `PhysicsObject::DrawAll` 0x646E77 in
  flight with the LH3DObject's matrix +0x14 and translation +0x38). What `fn_007F7ED0` does, pass 0:
  - **Light**: `L = (light [0xEA9E90] − position)` axis by axis, times `InverseSquareRoot((y·y + z·z) + x·x)`
    (0x7F7ED9..0x7F7F5A), in the **world**. `InverseSquareRoot` 0x841170 is a 128-byte table
    (`MakeInverseSqrtLookupTable` 0x8411D0) and one Newton step `((3 − (x·y)·y)·y)·0.5`, a little below the exact
    value: `affine::InverseSquareRoot` (was `lh_matrix`).
  - **Base colour, one per draw**: `fn_00801C90` at the position (0x7F7F5F, the bilinear land light), times the tint
    +0x10 `(c·t) >> 8` in the 4 channels and the specular + the +0x14 saturated in the 4, except with the pair
    0xFFFFFFFF / 0 (0x7F7F64..0x7F806E; the flag [0xC371B4] is 1 in .data and nothing writes it); then the haze
    `fn_007FEB30` at the position (0x7F807D). Abode::Draw sets the pair every time: with a fire the charring grey
    `fn_00730570` and the glow 0x730480 (both with alpha 0xFF), without a fire 0xFFFFFFFF / 0 (0x5160A6..0x5160D8); the
    fragment keeps the one of the constructor `fn_007F6EE0` (0xFFFFFFFF / 0).
  - **Per triangle** (0x7F809B..0x7F869B): with a matrix, each vertex `((z·r2 + y·r1) + r0·x) + t`; the **face** normal
    `n = (v1 − v0) × (v2 − v0)` normalised with `InverseSquareRoot((x·x + y·y) + z·z)`; **one single**
    `I = fistp(255·((l.z·n.z + l.y·n.y) + l.x·n.x))` (0x7F82A8); the front gets the factor of `I`, the back that of `−I`
    (`neg` 0x7F82AF, the two ambient branches 0x7F82B1..0x7F82EC: the rule of `Factor`) and each channel `(c·f) >> 8`
    with the colour's alpha (0x7F82EF..0x7F8363): `model_light::TwoSided`. So a lit face has its back at the bare
    ambient.
  - **Geometry**: the face in front (v0, v1, v2); the copy 0.45 ([0x8C7C78]) behind along −n, drawn (b2, b1, b0)
    ([0xC371AC] = 1); and with [0xC371A8] = 1, on each edge k with no neighbour (+0x40 + 4k = −1) the wall (fk, bk, fk+1),
    (fk+1, bk, bk+1) **on those same vertices**, so the wall carries the front colour on top and the back colour below.
    Both copies carry the triangle's uv and the draw's specular. Everything goes to `fn_0081C780` every 256 vertices or
    triangles: the indexed sibling of `Draw3DWorldTriangle`, with `g_world_to_clipping` (vertices in the world), the
    material's cull (+5 bit 0; without it, a backface test on the screen 0x81CBC7..0x81CC11 and CULLMODE CCW) and which
    **does copy the specular** of each vertex (0x81C9B9..0x81C9D0).
  - Pass 1 (0x7F7A0F..0x7F7CC6) is the snow on top (texture [0xEDD394], map [0xEDC344]): not ported (see
    [physics.md](physics.md#pending)).
  - openblack: `FragMesh::LightDirection`, `ObjectLight` (base colour), `AppendTriangle` (one triangle) and `AppendDraw`
    (one primitive per batch, in its source material `Primitive::source`), in `world_triangles::Vertex` with
    `specular` (Color1, which `vs_world_triangles` passes to `fs_object`); `Buildings::AppendFragMeshes` gathers the
    broken houses and the fragments every frame (a flying fragment at its `PhysicsDrawPose`, the pose between its last
    two turns that fn_00646FE0 puts in its Game3DObject matrix, as the instances) and `Renderer::DrawPass` sends them to
    the main view with the `WorldTriangles` program (vs_world_triangles + fs_object). The generated L3D mesh
    (`FragMesh::BuildMesh`) stays for the other views (reflections), the picking and the bounds: its FragMesh sub-meshes
    are marked `cpuDrawn` and `Renderer::DrawSubMesh` skips them in Main and MainBlended, also in the projected shadows
    over the object (`DrawShadowsOnObject`): `fn_007F7ED0` is no LH3DObject draw and the shadow loop of `fn_0080DB30`
    does not go through it **(inferred)**. The rebuilt part of a damaged house (that of `DrawBuilding`) still goes
    through the object program. `test_fragmesh_light` compares `TwoSided` with the instructions 0x7F82AA..0x7F8363
    emulated (200 000 cases), the root with values of the table and the vertices of one triangle with and without a
    matrix.
  - **(approximate)** openblack draws them after all the models and not in the order of the object loop; the blended
    primitives go in the Main view (as the pieces of the explosions), not in MainBlended. The FragMesh reflections keep
    the generated mesh and the object program's light (vertex light in mesh space, the same rule; the original's
    reflection of it is not checked).
- **Trap**: `vs_object` is also used by the sky (`fs_sky`); adding a new varying to it leaves the sky white. The
  specular travels in `v_texcoord0.zw` and `v_position.w` (after computing `gl_Position`).

## LH3DColor arithmetic

**Faithful**. An LH3DColor is a D3DCOLOR, 0xAARRGGBB. The engine combines two colours with two
families of operations, and **they all truncate, none rounds**:

- `(c·t) >> 8` per channel: `imul` on the masked byte and `shr 8`. With t = 0xFF each channel loses 1 (0xFF → 0xFE).
- `c·l / 255` per channel: the 0x80808081 trick (`sar 7` plus the sign bit, or `mul` and `shr 7`), which gives exactly
  trunc(x/255) for x from 0 to 65025.

Each routine has its own rule for alpha. When alpha is **preserved**, it is always that of the **first**
argument:

| Routine | What it does | Alpha | openblack |
|---|---|---|---|
| `fn_0080BF10` diffuse (0x80BFA3..0x80C00B) | `(c·t)>>8` → +0x4C | multiplied (0x80BFC5..0x80BFD3) | `MultiplyArgbShift8` (was `MulShr8_4`) |
| `fn_0080BF10` specular (0x80BF1B..0x80BFB9) | `min(a+b, 255)` → +0x50 | added and saturated (`cmp 0xFF`/`jb`) | `AddArgbSaturated` (was `AddSat_4`) |
| `fn_00809D80` (only 0x80A290: object colour × part) | `(a·b)>>8` | that of a (0x809DCF) | `MultiplyRgbShift8KeepAlpha` (was `MulShr8_3KeepA`) |
| `fn_00809DE0` (0x80A2A6: object specular + part) | `min(a+b, 255)` | that of a (0x809E46) | `AddRgbSaturatedKeepAlpha` (was `AddSat_3KeepA`) |
| `fn_0084BA90` 0x84BBEA, `Tree::Draw` 0x74B077, `fn_0074B3A0` | `(c·k)>>8` by a scalar | that of the object (0x74B0BD, 0x74B4C9) | `ScaleRgbShift8KeepAlpha` (was `ScaleShr8_3KeepA`) |
| `fn_007ACF70` (0x7ACF79..0x7AD03C) | `trunc(a·b/255)` | multiplied | `MultiplyArgb` (was `Mul255_4`) |
| `LH3DMist` 0x7FA6C8..0x7FA75F | colour × light `/255` | that of the colour, not the light (0x7FA753) | `MultiplyRgbKeepAlpha` (was `Mul255_3KeepA`) |
| `LH3DCreature::DrawNow` 0x48EF00..0x48EF98 | body × object `/255` | 0xFF (0x48EF8F) | `MultiplyRgbOpaque` (was `Mul255_3OpaqueA`) |
| `fn_005E25C0` 0x5E2729..0x5E273B | cloud edge × alpha `/255` | — (scalar) | `MultiplyBytes` (was `Mul255`) |

Details read from the binary:

- A field: `Field::Draw` calls `fn_0080BEC0` with the field's colour (BlendColor, alpha 0xFF at 0x528510), so its
  final alpha is (0xFF·0xFF)>>8 = **254**. With fire (0x528809..0x528862) the tint is first that colour times the
  charred grey of `fn_00730570`, on all 4 channels (= `MultiplyArgbShift8`).
- The one-shot ball (0x518DDA and 0x519002): tint `([0xBE8E8C] & 0xFF) << 24 | 0xFFFFFF` with [0xBE8E8C] = 0x00010196
  (no writer), so the alpha is (0xFF·0x96)>>8 = 0x95.
- `SpellWolf::Draw`: +0x4C = `fistp(alpha) << 24 | 0xFFFFFF` (0x51C701..0x51C714) and translucency is decided with that
  raw alpha (0x51C71E..0x51C727); when burning, +0x4C × charred on all 4 channels (0x51C751..0x51C7B7) and
  `fn_0080BEC0` with the glow `fn_00730480`.
- **`Tree::Draw` does not call `fn_0080BEC0`**: its +0x4C is `fn_00802120` (0x74AB1B) plus the fog `fn_007FEB30`
  (0x74AB60), and then the brightness [0xC22FA0] by `(c·k)>>8` with alpha preserved (0x74B077..0x74B0C4). The fog goes
  **before** the brightness, the other way round from `vs_object`.
- The burning tree (`fn_0074B3A0`): grey 50, or `ftol(255 − (1 − life)·2550)` with a minimum of 50 if life > 0.9, and
  then unsigned `min(gris, [0xC22FA0])` (`jb`, 0x74B47B..0x74B484); alpha **is preserved** (0x74B4C9, confirmed).
- The charred grey `fn_00730570` is `255 − ceil(175k/256)` (0x730585..0x7305D7): k = 255 gives 80.
- `fn_0080BF10`'s specular compares each of the 4 channels with `cmp 0xFF` / `jb` at 0x80BF5B..0x80BF98; `fn_00809DE0`
  saturates R, G, B with `jle` at 0x809E01, 0x809E19, 0x809E35.
- LH3DMist 0x7FA6C8..0x7FA75F multiplies the mist's colour (+0x4C) by the land light of `fn_00801C90` **after** the
  haze `fn_007FEB30` (channels at 0x7FA6DF, 0x7FA711, 0x7FA740).
- `fn_0080BF10` with t = 0xFFFFFFFF is called by `TownCentre::Draw` (0x5164AD), by the villagers with a specular
  (0x51B424) and by `PhysicalShield` (0x72D0D4): the land light loses 1 in each channel.
- The one-shot ball's support PSys takes the orb's 0x95 alpha (`DrawSpellGraphic` 0x51A252 → SetAlpha 0x55ED50 →
  [0xC0215C], applied in `fn_00679920` 0x679BC2), and the player's seed is lit with the land cell (0x803340); the
  bubble is the cyan texture × the grass light.

openblack: `src/Graphics/ArgbColour.h` (was `Lh3dColour.h`; `argb_colour::`, was `lh3d_colour::`; stateless, all
`constexpr`), with the alpha rule in the name (`Argb`, `Rgb…KeepAlpha`, `RgbOpaque`), plus `Argb`, `Red/Green/Blue/Alpha` and the bgfx conversions with no original
(`ToAbgr(argb)`, `ToAbgr(argb, alpha)`, `ToAbgr(vec4)` rounding and `ToVec4/ToVec3` = byte/255). The GPU twin is
`assets/shaders/argb_colour.sh` (`MultiplyShift8`, `AddSaturated`, `MultiplyDiv255`, `UnpackRgb24`; were
`lh3d_colour.sh` and `Lh3dMulShr8`, `Lh3dAddSat`, `Lh3dMul255`, `Lh3dUnpackRgb24`), included by
`vs_object.sc` (the instance's colour column, the cut colour and the unlit colour from `u_objectLight.z`) and
`vs_foliage.sc` (the crop colour).
`MultiplyDiv255` is floor((c·l + 0.5)/255): division in a shader does not round well (often x·rcp(255)) and a plain
floor(x/255) can give k − 1 when x = 255k; with the + 0.5 the fraction stays within [0.002, 0.998]
(approximate until tested on a GPU). `ScaleRgbShift8KeepAlpha` works for any k: the masks come after each `imul`
(0x74B099 / 0x74B09F / 0x74B0B2), so the channels do not overwrite each other; the callers pass 0..255. `fn_0080BEC0` is
"draw with the terrain colour" for the PSys property `DrawWithLandscapeColor` (`Particle3DObj::DrawAt`
0x67A00C); symbols.txt calls it `GetPoisonColor@Pot`, and the name is ours (inferred). Model
lighting (`model_light::Apply`), the cloud colour (`Clouds::Colour`, 0x5E1ECE..0x5E1F24), the mists
(`RendererMists`), the one-shot ball and the conversions in `Renderer`, `RendererBoat`, `RendererSea`,
`RendererSmoke`, `Dust`, `GameFont` and `ScreenFade` already use it, with no visible change. `test_argb_colour` (was `test_lh3d_colour`) compares
`MultiplyArgbShift8` with an instruction-by-instruction emulation of 0x80BFA3..0x80C00B and `MultiplyBytes` with the two forms of
0x80808081 for all products of two bytes.

### The object colour fields in the instance

**openblack's own transport** (no original): the engine stores the colours in the LH3DObject (obj+0x4C diffuse,
+0x50 specular, +0x54 windows) and the CPU uses them when lighting; openblack carries them per instance to `vs_object`. Each
instance has **five columns** (80 bytes, `i_data0..i_data4`): the matrix and a fifth one with one float per field, each
an integer of at most 2^24 that the float holds exactly. It is written by `argb_colour::PackInstance*`
(`src/Graphics/ArgbColour.h`) into `RenderContext::instanceColours`, and `RenderingSystemCommon::UploadInstances` interleaves
the two lists in the buffer.

| Field | Value | What it is |
|---|---|---|
| x (+0x4C) | 0 | the land light alone (`fn_00801C90` without `fn_0080BF10`) |
| | −1 − rgb (`PackInstanceTint`) | a tint t that multiplies the land light (`(c·t)>>8`): that of `fn_0080BF10`, or `Tree::Draw`'s own (see w) |
| | 1 + rgb (`PackInstanceColour`) | the colour from `SetColorSpecular` 0x7F9770 (vt 0x2C), instead of the land light |
| y (+0x50) | rgb (`PackInstanceSpecular`) | the specular, 8 bits per channel, added with saturation to the land one (0x80BF1B..0x80BFB9) |
| z (+0x54) | 0 or 1 + rgb (`PackInstanceWindow`) | the window colour from `Abode::Draw` (vt 0x30, 0x516068); 0 = off |
| w | 0 or 1 (`PackInstanceTreeTint`) | 1 = the tint goes after the fog: `Tree::Draw` does not call `fn_0080BF10`, it lights with `fn_00802120` (0x74AB1B), applies the fog (0x74AB60) and then multiplies the +0x4C (0x74B077..0x74B0C4; when burning, `fn_0074B3A0` 0x74B48F..0x74B4D3) |

This way colour and specular go together (before, they shared the w of the fourth column and the last one won). Who passes what
(`DrawColoursOf` in `RenderingSystem.cpp`, read in each Draw):

| Object | Tint | Specular | Address |
|---|---|---|---|
| Villager | burning: charred grey; if +0xD0 ≠ 0: white 0xFFFFFFFF; poisoned: 0xFFE8FFDD; otherwise, nothing | fire glow / +0xD0 / 0xFF001000 | `fn_0051B3D0` 0x51B402..0x51B488 |
| Animal | burning: charred; +0xD0 ≠ 0: white; otherwise, nothing (does not check poison) | glow / +0xD0 | `Animal::Draw` 0x51C4C6..0x51C51C |
| Miracle wolf | always white (+0x4C = alpha << 24 \| 0xFFFFFF); burning, × charred on all 4 channels | glow / +0xD0 | 0x51C709..0x51C7E1 |
| Poisoned pot or pile without fire | 0xFFE8FFDD | 0xFF001000 | `Pot::Draw` 0x51BB8F..0x51BBA3, `PileFood::Draw` 0x51C191..0x51C1B8 |
| Town centre miracle icon (`TownCentreSpellIcon`) | white, always (with centre life > 0) | +0x10C (not ported: 0) | `TownCentre::Draw` 0x5164A6..0x5164B2 |
| Worship site miracle icon | white only if +0x10C ≠ 0 (0x519672..0x51967C, 0x5198A8); since +0x10C is not ported, nothing | 0 | `SpellIcon::Draw` 0x519650 |
| Physical shield | white | 0 | `PhysicalShield::DrawShield` 0x72D0D4 |
| One-shot ball | white (its alpha goes in `components::Alpha`) | 0 | 0x519002..0x51901E |
| Field | its colour; burning, × charred (`MultiplyArgbShift8`) | 0 / glow | `Field::Draw` 0x528809..0x52888A |
| Tree | brightness [0xC22FA0]; burning, `TreeDrawColour`; both after the fog (w = 1) | 0 | `Tree::Draw` 0x74B077..0x74B0C4, `fn_0074B3A0` 0x74B48F..0x74B4D3 |
| Anything else with fire (buildings, rocks, dead trees, totems...) | charred grey `fn_00730570` | glow `fn_00730480` | `fn_00518050` (11 callers) and `DrawBuilding` 0x517FD4 |
| Power bands | player colour (`SetColorSpecular`) | 0x141414 ([0xBE8EA0] = 20) | `DrawSpellGraphic` 0x51A370..0x51A3BE; the hand's one writes the fields directly (`PHandFX` Band::Draw +0x4C 0x68D87D / 0x68D8AB, +0x50 0x68D8B1; (inferred) without `fn_00801C90` behind it) |
| PSys mesh atom | DrawData+8 (tint with `DrawWithLandscapeColor`, otherwise `SetColorSpecular`) | 0 (approximate: DrawData+0xC is missing, read at 0x67A012 and 0x67A023; it is lost on both paths) | `Particle3DObj::DrawAt` 0x67A00C..0x67A02F |

The white tint subtracts 1 from each channel (`(c·255)>>8`): before, it was not applied. The tint's alpha is not carried. The
LH3DObject draw copies the whole +0x4C to [0xC37D8C] (0x80DEF8; the +0x50 to [0xE9FE2C], 0x80DEFE) for all objects,
and several routines read it (`fn_007A4170` 0x7A6A2C, 0x7A7E85, `fn_00805CD0` 0x805EAA, `fn_00809E50`); that its alpha only
counts for the ones that fade out is **(inferred)** (those reads were not followed through to the vertex colour). For
those openblack uses `components::Alpha` (approximate: the shield should end up at 0xFE and the wolf at (A·0xFF)>>8).
`test_argb_colour` checks the extremes (−2^24, 2^24, black, the black window switched on) and 200 000 random colours
decoded as in the shader.

- Draws that leave the land light alone in +0x4C (`fn_00801C90` without `fn_0080BF10`): e.g. `MultiMapFixed::Draw`
  0x5180CF, `Animal::Draw` 0x51C4F0.
- +0x54 is written by vt 0x30 = `fn_007F9780` (`mov [ecx + 0x54], edx`), from `Abode::Draw` 0x516068; 0 when the
  windows are not lit (0x51606D..0x516073).
- The window colour (`Abode::Draw` 0x515F70): a grey 0xFFgggggg (0x516054..0x516064), scaled below 256 by the
  intensity, (g·k) >> 8 (0x516044..0x51604F); it flickers with the table 0x8D86D0 = {0, 7, 3, 5, 4, 2, 6, 1} and has a
  per-house time offset from the position. The window sub-meshes are drawn unlit in that flat colour.

**(approximate)** The +0xD0 specular of living beings: the original checks the whole dword with its alpha (0x51B416 /
0x51C4D6 `test eax,eax`), and the healing chakra writes alpha 0xFF (`fn_006A0E30` 0x6A0EF5), so its frames
with rgb 0 keep the white tint; openblack removes `SpecularColour` with rgb 0 (`Particles/Rules/Heal.cpp`) and those
frames go with the land light alone.

**(inferred)** Every instance class that can burn goes through `fn_00518050` or `DrawBuilding`, or carries the same
pair inline (the wolf 0x51C751; that of the house's FragMesh, 0x5160AF, now goes in `Buildings::AppendFragMeshes`);
still to be ported are the inline pairs of `Object::DrawOutOfMap` (0x51C839) and of the physics prediction object
(0x646F8C) (see Pending).

## Texture wrapping or clamping

**Faithful** (done).

- `fn_00850FC0` 0x851779: after setting the mode, `SetD3DTillingOn` if `g_b_need_tilling` (0xECA614) or bit 2 of byte
  +5 of the material; otherwise `SetD3DTillingOff` (CLAMP). `g_b_need_tilling` is copied on each Draw of the 3D object from its bit
  0x200 of Flags1 (vt+0xE4 = `fn_007F9B30`), and that bit is only set by the setter vt+0xE0 from particle meshes
  (`ParticleMeshCreatorAnimTextured`, `ParticleVolBlendMeshCreator`): world objects depend only on the material.
- `AllMeshes.g3d`: 1675 primitives with the bit, 161 without it; none of the latter has UVs outside 0..1, so clamping
  only changes bilinear filtering at the texture edges. openblack: `Primitive::wrap` and sampler flags in
  `Renderer::DrawSubMesh`.

## The single transparent queue (LH3DZSorter)

**Faithful**, with what is marked. API `graphics::zsort`, in `src/Graphics/ZSort.{h,cpp}` (Renderer core).

**The original.** Everything that is drawn with blending in the world goes through **a single queue**:

- `LH3DZSorter::NewZObject` 0x83F310 (32 calls in 31 functions): linked list in a static buffer of 0x800
  entries of 0x18 bytes (0xEDDD30; count 0xEE9D34, head 0xEE9D38). The new entry goes **in front of the first one with a
  strictly smaller key** (`fld cur.clave; fcomp clave; test ah, 1`, 0x83F36A..0x83F376) or at the end (0x83F39A):
  from far to near and, with equal keys, the one that arrived first goes first (stable). With the queue full the **new one is
  lost**, whatever its key (0x83F315/0x83F31C). With a NaN key, `fcomp` says "less" (C0).
- Reset `fn_0083F3B0` from `StartFrame` 0x82F1F9. Drain `fn_0082F280` from `FinishFrame` 0x82F480, only once
  ([0xECA610]), from head to end, copying the user datum K into [0xEE9D30] (0x82F29D) before each
  callback. It goes **after** everything drawn immediately in the frame and **before** the end-of-frame
  callbacks ([original-frame.md](original-frame.md)).
- The key is `LH3DTech::GetValueForZSorter` (inline in W120; Mac 0x010E7360): |P − g_camera|² in `float`
  (g_camera 0xEA1DB8). The x87 sum follows the order of the loads, and with the FPU at 24 bits (fn_007DEE00, 0xFCFF at
  0x7DEE0D; from `InitOneTimeOnly` and from `Process3dEngine` after `FinishFrame`, 0x54E426) each step rounds to
  `float` **(inferred: that nothing between fn_007DEE00 and the AddDrawing calls raises the precision again; D3D7 without FPUPRESERVE
  also leaves it at 24 bits)**: (x² + y²) + z² in the sprites 0x840C70, the mists 0x7FA83C, the smoke 0x7F8D3E, the effects
  0x6797E5, the hand 0x46D1BD, fn_00813340 and fn_00679F60; **(x² + z²) + y²** in `LH3DObject::AddDrawing` 0x815F0F and in
  the rain 0x834215 (`zsort::SumOrder`). Only the last bit can change.
- The `g_zsorter` object (0xECA648, `fn_0083F2B0` in `OpenD3D` 0x82CDC1) is never read: it is not ported.
- The two callers that submit with key 0 (0x5E66B4, 0x68AB5C) are drawn last.
- The rain's K: ((alpha << 8) − ftol(z × −0.0125)) << 8 − ftol(x × −0.0125), constant [0x9A3AC0] = −1/80, in 32-bit
  integers (0x834233..0x834259); outside [0, 20480) the bytes run into each other. The caller clamps alpha to ≤ 0xFF
  and queues nothing with a negative one (0x8341B8, 0x8341CC).
- The rain's callback 0x833F80 reads K back from [0xEE9D30] (0x833F83..0x833FB4): byte 0 the tile x, byte 1 the tile z
  (each × 80 [0x8D060C] to get the tile's corner), byte 2 ([0xEE9D32]) the alpha.
- `CheckRegionOnScreen` 0x868C80 (path with [0xEA9EB4] = 0; the other is `fn_007ACC60`, not read): the centre is the
  box centre through the object's matrix (0x868CBC..0x868D04), the radius the object's scale +0x44 × the box's +0x1C,
  the origin the object's position (matrix +0x38). Z + r < near → off (0x868D88); the eye closer than r to the origin
  → on (0x868DA1..0x868DE1); else rs = r·halfW/(Z·T) around the centre's (sx, sy), not truncated: off when sx + rs <
  0, sx − rs > W, sy + rs < 0 or sy − rs > H (0x868E59..0x868EFD); partly on screen counts.
- Its side effects: `g_b_last_on_screen` [0xEA1AF0], `g_last_selected_box` [0xEA1AD0], `g_last_distance` [0xEA1AF4],
  [0xC37EA0] and the 3D object's vt+0xA0.
- Screen globals of `g_info_transform`: size [0xE839E4] / [0xE839E8], half size [0xE839F0] / [0xE839F4]; [0xC3812C] =
  near·tan(horizontal fov/2).
- Its users: `LH3DObject::AddDrawing`, the PSys atoms (0x679F75), `LH3DMist::AddDrawing` (0x7FA7CE) and the
  SuperVillagers' `fn_00825400` (0x82541D).
- The point test `fn_0081F1D0`: Z < near → off (`fcom [0xE839E0]`, 0x81F1FA); sx = ftol((X/Z + 1)·halfW), sy = ftol((1
  − Y/Z)·halfH) (0x81F214..0x81F27F), truncated towards 0, so a point less than a pixel left of or above the screen
  still counts; on screen when 0 ≤ sx < W and 0 ≤ sy < H (0x81F286..0x81F2A8).
- LH3DMist vt+0x104 = `fn_007FA790`: the same sphere (0x7FA79E..0x7FA7CA) through `CheckRegionOnScreen` (0x7FA7CE),
  then the Draw vt+0x108 at once (0x7FA7E0).
- The influence ripples are one Z object each (`fn_008274A0`, from `fn_005E5CD0` 0x5E6264..0x5E628D); the callback
  0x827500 draws its 7 smoke.raw sprites with DrawSpecial1 in the border's plane.

**The API.**

| Function | Original |
|---|---|
| `Queue<Item>::Begin()` | `fn_0083F3B0` (and the `[0xECA610] = 0` of `StartFrame` 0x82F123) |
| `Queue<Item>::Submit(item, key, user = 0)` | `NewZObject` 0x83F310: stable insertion, cap `k_Capacity` = 0x800 |
| `Queue<Item>::Drain()` | `fn_0082F280`: the entries from far to near, with their K; empty if already drained |
| `Key(p, camera, order)` | `GetValueForZSorter`, in `float`, with the caller's summation order |
| `PackRainUser` / `UnpackRainUser` | 0x834233..0x834259 / 0x833F83: alpha·65536 + 256·trunc(z/80) + trunc(x/80) |

**What goes in and with which point** (`Renderer::DrawPass`, the `sorted` queue, only in the main view):

| What | Original | Key point |
|---|---|---|
| models whose mesh has the 0x200 flag, whole (also those fading out) | `LH3DObject::AddDrawing` 0x815F53 | instance translation (+0x38), (x² + z²) + y² |
| the one-shot ball's bubble, always | 0x815F53 (bit forced at 0x72A4AA) | `sortPoint` (`OneOffSpellSeed::Draw` 0x518E90) |
| the hand, whole | `CHand::AddDrawing` 0x46D203 | the origin +0x38 of the LH3DObject [CHand+0x482C] (0x46D1B7); here the instance translation **(inferred: that it is that origin)** |
| `Sorted` effect: each sprite | `LH3DSprite::AddDrawing` 0x840CB3, from 0x67B0D2 | the sprite (`SortedFrame::sprites`) |
| `Sorted` effect: each mesh atom, also the opaque and cut ones | `fn_00679F60` 0x679FC7, from 0x67A246 | the translation (+0x38..+0x40) |
| `Sorted` effect: each chain | `fn_0067B380`, from 0x6798DF | the joint n/2 |
| `Sorted` effect: each mist | `LH3DMist::AddDrawing` 0x7FA87B, from 0x67A782 | the mist (via `mists::Submit`) |
| `Queued` effect, whole | `PSysManager::AddDrawing` 0x679834 | the effect's `GetOrigin` |
| `components::Sprite` | `LH3DSprite::AddDrawing` 0x840CB3 | the sprite |
| map mists and those of `mists::Submit` | `LH3DMist::AddDrawing` 0x7FA87B | the mist |
| **clouds** | 0x7FA87B, from `fn_005E25C0` 0x5E2813 | the cloud |
| chimney smoke | `LH3DSmoke::AddDrawing` 0x7F8D8E | the chimney |
| **rain, one entry per tile** | `fn_008341B0` 0x83427F | (x, `GetAltitude`, z) of the block centre (+0x90C/+0x910 + 80, 0x8362DB); here `tile.origin` with the height from `LandHeightAt` **(approximate)** |
| **boat sprites, one by one** | 0x840CB3 (wake `PetitNavire::PostDraw` 0x5E08D0, puffs fn_00823F70 0x82411A) | the sprite |

Clouds go in with the sky (as in `GLandscape::Draw`); the rest, with the models. Without entities (debug
view) the queue has only clouds and mists and is drained at the end of the pass.
The `components::Sprite` (dust, pick-up particles, sparkles, fireflies...) go with the transparent models;
before, openblack drew them before all of them and a transparent model behind covered them.

**What goes in and what does not: the models**. `LH3DObject::AddDrawing` 0x815A70 (vt+0x100 of the static,
animated and morphable objects) sends an object to the queue **only** because of bit 0x10 of its +4 (vt+0x44 = `fn_007F97C0`, read at
0x815AC2 and tested at 0x815F0B): with it, the **whole** object (NewZObject 0x815F53, callback 0x7FA980 → Draw
vt+0x108); without it, the Draw **immediately** (0x815F62), blended primitives included. The bit is copied by `SetMesh` 0x7F9E10
from the mesh's 0x200 flag (vt+0x3C = `fn_007F9D40`, mesh+4 & 0x200, 0x7F9E48; vt+0x40 = `fn_007F97A0`,
0x7F9E51..0x7F9E64). mesh+4 are the L3D header flags (`GetChimneyPos` 0x7F9F17 tests 0x400 there,
HasChimney): `L3DMeshFlags::Unknown10`, `L3DMesh::IsZSorted`. The one-shot ball forces the bit (vt+0x40(1) 0x72A4AA,
after its `SetMesh` 0x72A49D). `SetGlobalAlpha` (bit 0x80, vt+0x48 = `fn_007F9D60`) does **not** count in 0x815A70: only
`fn_00813340` (vtable 0x9A3068, that of `Particle3DAnim`) queues because of it (vt+0x4C 0x8133B9). That is why an object that
fades with a mesh without 0x200 is drawn immediately with the table 0xC387C8 (`render_modes::Table::GlobalAlpha`), in the
main view. It affects the physical shield (`PhysicalShield::DrawShield`: `SetGlobalAlpha(1)` 0x72D0CC, `AddForDrawing`
0x72D0E2): its mesh `SpellSolidShield` has the flag in `Data\AllMeshes.g3d` (the game's one, read in full: 626 L3D), so
it stays in the queue. With the flag, in that file, 40 meshes: almost all the miracle ones (`SpellBlast*`, `SpellPhile*`,
`SpellRainCone`, `SpellSolidShield`, `SpellSpellBallSurface02`, `SpellSpellDispenser`, `SpellPulseIn/Out`...), the
caves, the Aztec storage pit, `I_SpellGem`, `ObjectBoxFrame`, `RewardChestExplode`, the vulture and
`TreeWheatInField`. openblack (`Renderer::DrawPass`): `instancedDrawDescs` and `translucentDrawDescs` by the mesh's
flag (and the bubble by its `sortPoint`); the other models, immediately, with all their primitives.
**(inferred)** that every model goes through 0x815A70 (`Game3DObject::AddForDrawing` 0x63B5D0 → vt+0x100). Not all
do: the creature's and the hand's bodies are of class 0x9A3068 (`LH3DObject::Create(3)` from `Morphable::MorphInit`
0x61731A), whose `AddDrawing` `fn_00813340` also queues by the global-alpha bit; the hand always goes to the queue
anyway, and the creature has neither bit ([creature.md](creature.md#drawing-in-the-frame)).

**The three paths of a PSys effect** (`psys::DrawPath`; [particles.md](particles.md)). `fn_00679860` copies the manager's +0xAE
into [0xC0215D] (0x679884) and each atom checks it:

- **`Sorted`** (`Draw_(t, 1)` 0x55EDA0 → `fn_00679840`, +0xAE = 1 at 0x67984E; `Spell::Draw` 0x720441, storm
  0x72DCB7, shield 0x72D160, teleport 0x5FCDC5, dispenser 0x722A13, flock 0x72420D, hand utilities,
  town belief 0x69BF19): the effect **has no Z-object**. `manager::CollectSorted` gives each element with its
  point and `DrawPass` does one `Submit` per element, (x² + y²) + z²: each sprite (`LH3DSprite::AddDrawing` 0x840C70
  from 0x67B0D2, at the LH3DSprite's position, raised by height·size·0.5 with `CentreAtBase`, 0x67AFAB..0x67AFD6),
  each mesh atom, **opaque and cut ones too** (`fn_00679F60` from 0x67A246, at its translation, after
  `CheckRegionOnScreen` 0x679F75; here the box's sphere **(approximate)**), each chain (`fn_0067B380`, at the
  joint n/2: base +0x44, stride 0x1C, index (n − (n >> 31)) >> 1, 0x67B389..0x67B3A1) and each mist
  (`fn_007FA7F0` from 0x67A782, at mist+0x38: it arrives via `mists::Submit`). The `ZR_SurfRevol` discs do not check
  [0xC0215D] (0x67CBA0 → `RenderParticleGJMesh::DrawAt` 0x67C150 → `Draw3DWorldTriangle` 0x81C090, 0x67C9F2): they are
  drawn **immediately**, in the main view after the models (`Spell::DrawSpells` 0x7203F0 comes after the
  models, from 0x54E023), unsorted. When draining, consecutive sprites in the queue go in a single call if they
  share a material (`DrawParticleSprites`, was `DrawPSysSprites`: same order and same states, the same pixels).
- **`Queued`** (`AddDrawing` 0x55EDC0 → `PSysManager::AddDrawing` 0x6797D0, +0xAE = 0 at 0x6797DE; only the seed in
  the ball, the icon, the reward or the map 0x51A2CA and the containers with `GSpotVisualInfo+0x4C == 1` 0x63E26A; and,
  outside PSys, the fire `FireGraphic` 0x73261D): **one** Z-object per effect with key = `GetOrigin`
  (0x6797E5..0x679834). On draining (callback `fn_00679860`) all its elements are drawn immediately, in the
  order of `fn_006798B0` (0x6798B0..0x679912: the collection's atoms, its chain, the child collections;
  `manager::OrderedEffect::items`): sprites with `LH3DSprite::Draw` (0x67B0DF), meshes with `fn_00679F20` (0x67A458;
  vt+0x104, or vt+0x11C cut with bit 4 of +0x24), mists with vt+0x104 = `fn_007FA790` (0x67A78C: the screen
  test and the Draw), discs (0x67CBA0) and chains with `fn_0067B370` (0x6798DD).
- **`Immediate`** (`Draw_(t, 0)`): the effect of the seed in the hand. `CHand::Draw` 0x46D210 draws, inside the
  hand's Z-object, the mesh (0x46D258), the held object (0x46D27C) and then `DrawSpellInHand` (0x46D2AE →
  0x46E680), which does `Draw_(1.0, 0)` (0x46E76A): `manager::HandEffects`, drawn like a `Queued` right behind the
  hand mesh, in the same entry. openblack's held object is not drawn from that entry **(inferred: it does not
  change anything, it is opaque and goes first)**.

That is why the rain and fire of a miracle no longer fight with a dispenser's bubble: before, the whole effect went
with its origin's key, in front of or behind the bubble all at once; now each drop and each flame goes with its own. The
dispenser's disc (which belongs to the dispenser's `Sorted` effect, `Draw_(1.0, 1)` 0x722A13) is drawn immediately, before
the whole queue, so the bubble (mode 12, additive and which **writes Z**, 0x82ECA6) adds its light on top, as in the
original. Test: `test_psys_sorted_queue` (key of each sprite and of the chain, a `Queued` between two sprites of the
same `Sorted`). Trace: `OPENBLACK_ORB_TRACE=1` (the discs with their path and their place).

Rain, on the other hand, does **not** go through `PSysManager::AddDrawing`: it carries its own entry per tile with its own
point (table above); giving it an effect's key would be wrong.

**What changed when unifying** (fixes read from the binary):

1. Everything is stable: the clouds' own sorting and that of the fallback mists were `std::sort` (0x83F36A).
2. Cap of 0x800 with the new one lost (0x83F315/0x83F31C).
3. The clouds go in the queue, mixed by distance with everything (before, always on top; 0x7FA87B, 0x5E2813).
4. The chain ribbons, now inside their effect's object (`fn_006798B0` 0x6798DD), go through the API.
5. The rain goes in the queue, one entry per tile (before, a group behind the list; 0x83427F).
6. The boat sprites, each one in the queue (before, a batch behind the list; 0x840CB3).
7. The hand always goes to the queue and whole (before, its opaque parts in the main view and only the blended ones in the
   list, and only if it had any; 0x46D1B7..0x46D203 tests nothing).
8. PSys effects by their path (above); models to the queue only by their mesh's 0x200 flag, whole.

In addition the key is the squared distance in `float` and not the distance: the same order except for keys that the square root
merged.

**What does not go in, on purpose.** The landscape block order (`LH3DIsland::PreDraw` 0x7FF2D0, opaque, near to
far), the water rings (`LH3DSprite::Draw` **immediate**, 0x5E526C), the villager blobs, the hand's glow
in the water (0x5E4D89) and the sun (`fn_0086BB60`, after the drain, **(inferred)**). Projected shadows
are not Z objects either (no shadow routine is among the 32 callers): the land ones go with each block
(`fn_007FF610` 0x7FF749) and the object ones at the end of each receiver's Draw (`fn_0080DB30` 0x80E457..0x80E4D7,
`fn_00812170` 0x81311A..0x81317C), so they go immediately with an object drawn immediately (without its mesh's 0x200 flag,
0x815F62) and **inside its Z object** with a queued one (0x7FA980 → vt+0x108). openblack does the same (`Renderer::DrawShadowsOnObject` behind its `DrawMesh`, in
Main or from the drain; see [Dynamic shadow on objects](#object-reflections-and-hand-shadow-on-objects)); before, they all went
behind the drain and darkened the bubbles, clouds and sprites in front. The reflection has no queue (in
the original it has not been read **(inferred)**); it stays as it was.

**(approximate)**:
- The arrival order (the tie-break with equal keys) is not the original's: here clouds, models per mesh (a
  `std::map`), faded ones, sprites, meshes and chains of the `Sorted`, `Queued` effects, sprites, mists, smoke,
  rain, boat, influence ripples; there the order of the frame's AddDrawing calls ([original-frame.md](original-frame.md#2-draw-stages-in-order),
  steps 4m..22).
- The rain point is `tile.origin` with the height from Rain.cpp's `LandHeightAt`, not `GetAltitude` 0x803090 on
  `MapCoords(x × 65536 × 0.1, z × 65536 × 0.1)` (0x8341D2..0x834210).
- `CheckRegionOnScreen` of a `Sorted` mesh (0x679F75) is its box's sphere; that of a model (0x815AB1) is not
  done: a model outside the view takes up a slot of the cap that the original does not spend.
- The animated meshes of a `Sorted` (`Particle3DAnim::DrawAt` 0x67A9D9 → `fn_00813340`: to the queue only with bit
  0x10, 0x80 or +0xB8, otherwise immediately) go like the other meshes **(not ported)**.
- The rain travels by its index and not by K: `CollectTiles` already applies the distance fade of fn_00834370, so
  a K built from that alpha would not be the original's.
- The blended primitives of a model drawn immediately go to the `MainBlended` view (`DrawSubMesh`), in front
  of the whole queue but behind everything opaque in the main view; in the original, at the place of its Draw.

The `OPENBLACK_ORB_TRACE` trace still writes the distance (the square root of the key), as before. New trace:
`OPENBLACK_ZSORTER_TRACE=1`.

## Villager blobs, object reflections and LOD

**Faithful** (checked with a Unicorn emulation of `fn_0081FFF0`).
- **Blobs** (`fn_0081FFF0`, from the Draw of animated objects `fn_00812170`): feet = bones 21 and 18 (end of the two
  legs), on the ground + 0.2; D = O·s − ((O·s)·n)·n with O = (√2, 0, √2), s = scale, n = terrain normal;
  quad 1 from foot 21 with V = D + (P18 − P21)/2, quad 2 symmetric; corners C − 0.02V ± U and C + V ± U with
  U = 0.2·norm(1, 0, −1) (fixed width 0.4); UV (0.0)(1.0)(1.1)(0.1), alpha 1 at the feet and 0 at the tip; mode 6, no
  Z, two-sided, `human_shadow.raw` (byte & 0xF0 as alpha). Not if y ≤ 0.2 (in the water), dead or in the creature's
  hand. Animals: points from their EBone data (2 or 4 quads). openblack: `Renderer::DrawHumanShadows`;
  `human_shadow.raw` is cut to 4 bits on load (0x81FCDD, [rendering.md](rendering.md#argb4444-textures)) and
  `fs_blob` only samples it.
  - The quad is built by `fn_0081FE50`: v0 = C − 0.02V + U, v1 = C − 0.02V + W, v2 = C + V + W, v3 = C + V + U; opaque
    at the feet.
  - The feet come from the drawn matrix **with the slope shear**: `Villager::Draw` (0x51BA74 / 0x51BAD5) calls
    `fn_0051B3D0`, which calls `fn_0051AF00` (0x51B3DA) then `fn_0051B220` (0x51B3E1): MulPre(+0x14, S) with S =
    identity + the shear cells m1 / m7 (0x51B35E..0x51B3B8, `fn_007FAE60` at 0x51B3B8), before the 3D draw.
    `Animal::Draw` does the same (0x51C34C / 0x51C353).
  - `fn_0081FFF0` reads the bone buffer [0xC37D9C] that `fn_00812170` fills with the frame's skinning (0x8125DE);
    without a clip, the mesh's rest locals as `LH3DAnim::SetTransform` 0x83A1D0 skins them (only with obj+0x80 null,
    0x812351..0x8123B4).
  - A blob point: M = Mul(bone, clipping-to-world [0xEA9DE0]) (feet 0x8205FB / 0x820667, EBone points 0x8200F3), and
    for an EBone point MulPre(M, E) (0x8200FF), whose translation reads only E's position; the point is the
    translation row.
  - The terrain normal is `LH3DIsland::GetNormal` 0x803630 at the drawn matrix's translation x, z ([edi+0x38] /
    [edi+0x40], 0x812819..0x81282D; call 0x812859).
  - The blob material is mode 6 [0xEB998C] (`fn_0081FAA0` 0x81FD42).
- **Sea reflections** (`GLandscape::Draw` 0x5E490F): `DrawUnderWater` draws the object mirrored at y = 0, unlit,
  clipped so that only what is above the water is reflected: the hand (0x65A0A0A0) and what it holds, **the creature's
  body** (0x65A0A0D0 + specular 0x30; not what it carries), boats (0xFF303070), physics objects (their colour).
  Sharks and swimming SuperVillagers are drawn **cut below the water** (`DrawCutByPlane`, 0xFF303070); the fish-farm
  fish are sprites. Details in the next two sections and in
  [Fish shoals](#fish-farm-fish-shoals).
- **LOD**: `g_last_distance` is the linear depth of the sphere centre; S = min((importance + 1) · radius ·
  LevelOfDetail, 100000). In this executable the two loads of LevelOfDetail are disabled with NOP, S = 100000:
  **always LOD 1**, with no fading or impostors (with the design value 0.5, a villager would switch at 11.5 / 33 / 43 u).
- Testing trap: `OPENBLACK_MOUSE_AT` on an edge of the window activates edge scrolling and moves the
  camera; use interior points.

## Physics and LOD 0 submeshes

**Faithful** (L3D loader rule: the `isPhysics` bit of the submesh header and `lodMask`). A submesh with
`isPhysics` (or without bit 1 of `lodMask`, except windows) is only for physics (`BuildFromVertices` 0x7FBAE0
uses it as a hull) and is **never drawn**. Examples: the miracle dispenser (mesh 557 `SpellSpellCreator`, the one of
`ABODE_SPELL_DISPENSER` in info.dat) has a physics submesh 0 of 16 vertices, a 7-sided prism frustum
(radius 1.9 at the bottom, 1.5 at the top, height 3.1, UV 0, skin 0x4F), and the orb `O_Bibble_up` a smooth sphere. In openblack
all paths skip it: `Renderer::DrawSubMesh` (everything that goes through `DrawMesh`: objects, sorted transparents,
PSys mesh atoms, reflections, the hand, boats, sharks, fish; only the mesh viewer paints it
with `drawAll`), the static shadow (`DrawStaticShadowPass`), projected shadows (`ShadowList.cpp`), `FragMesh` (building
pieces), `PartialBuild` and picking (`L3DMesh::RayIntersect`). Checked on 2026-10-01 in captures: the prism does not
appear on any dispenser.

- The original's static shadow pass also takes LOD 0 only (`fn_00806DA0`).
- The original's ray/mesh test `fn_00865020` gives the hit triangle's normal norm((V1 − V0) × (V2 − V0)), left as it
  is when zero (0x8652C5..0x865405; the zero test at 0x865396..0x8653C7).

## The under-sea pass (`graphics::sea_pass`)

**Faithful** except what is marked. A single API for everything the original draws "below" the sea before it
(`GLandscape::Draw` 0x5E48AE..0x5E4E8C): `src/Graphics/SeaPass.h` (CPU, `namespace graphics::sea_pass`) and its twin
`assets/shaders/sea_plane.sh` (GPU, `u_objectClip`, `SeaPlaneDiscard`, `SeaUnmirror`); test `test/test_sea_pass.cpp`.

The original has three mechanisms and a single plane:

| Mechanism | What it does | Plane | Light |
|---|---|---|---|
| A, mirrored land (`fn_007FF4F0`) | height unit [0xC3720C] = 0.67 × [0x8AB678] = −1.0 (0x7FF515..0x7FF52F), [0xFA92DC] = 1 (the blocks reverse their indices, 0x7FF535) | — | table >> 1 (0x7FF53F..0x7FF564), no small bump ([0xC37210] = 0, 0x7FF566..0x7FF577), no Z write (0x5E48C5..0x5E4900) |
| B, `DrawUnderWater` (vt+0x118: `fn_00811010` static, `fn_00810E20` animated, `fn_00813300` complex → `fn_00850FC0`) | mirrors the object at y = 0 (fsubp 0x851094 / 0x8510BD / 0x8510E5) | tests the **mirrored** point: d > 0 out (0x85111C..0x851149) | constant colour obj+0x4C / +0x50 (0x811033..0x81103F → [0xC37D8C] / [0xE9FE2C], read at 0x851082 / 0x85102F), unlit |
| C, `DrawCutByPlane` (vt+0x11C: `fn_0080C050` static / complex, `fn_00811C70` animated → `fn_00858BA0`) | does **not** mirror (0x858C5D..0x858CAE) | tests the point: d < 0 out (0x858D49..0x858D80) | 90 + 165·I >> 8 on obj+0x4C, + obj+0x50 (see the next section) |

- B and C share the CPU clipper `fn_0081D2C0` (its only callers: 0x8515E7 / 0x8516AD and 0x85930E /
  0x8593C5) and the user plane of `fn_00822560` (world [0xF03128], view [0xF03118]). The default plane
  (0, 1, 0, 0) is set by the initialisers `fn_0084A380` (0x84A39A: [0xF0312C] = 0x3F800000) and `fn_0084A3C0`; the
  swimmers set (0, −1, 0, 0) (0x5E4C4A..0x5E4C5A, dwords, 0xBF800000) and restore it at 0x5E4D76. The shark
  (`fn_00774E30` 0x774FF5..0x77501A, restored at 0x7750E6..0x775106) and the net (`fn_00829BC0` 0x829C91..0x829CB7,
  restored at 0x829D25..0x829D45) set **their own** (0, −1, 0, 0), before the swimmers' loop
  (`sea_pass::k_SharkPlane`, `k_NetPlane`: the same values as `k_SwimPlane`). With
  (0, b, 0, 0) both mechanisms keep the same side of the **real** y: b > 0 → y ≥ 0, b < 0 → y ≤ 0 (the mirror and the
  opposite test cancel out). `sea_pass::Kept(Mechanism, plane)` → `SeaPlane {None, KeepAbove, KeepBelow}`.
  - The original's mirrored land swaps its indices with [0xFA92DC] = 1, read in `fn_00875C60` (0x875D7B / 0x8766B7)
    and `fn_00876910` (0x876A2A / 0x87707B).
- **Face**: the material decides in all three places (the Draw 0x84C34A, B 0x851798..0x8517D1, C 0x8594CF..0x8594ED:
  `((~mat+5) & 1)·2 + 1`, `push 0x16` = CULLMODE). `SeaPassState::FaceCull(Surface, twoSided, unmirror)` brings together the
  four places in openblack: models (C1), moon (C2), sky (C3) and land (C4, which in openblack has the opposite vertex
  order: it is an openblack fact, with no address).
- **openblack**: the `RenderPass::Reflection` pass draws with the mirrored camera (`ReflectionXZCamera`), which already gives
  B's mirror. What C draws inside that pass is **un-mirrored** (`SeaDraw::unmirror`, `SeaUnmirror` in
  `vs_object`, in both its branches), and the plane is a per-fragment discard on the real y (`fs_object`,
  **(approximate)**: strict per pixel, y = 0 stays on both sides; the original clips triangles). The same with
  the fish sprites (`Unmirror` on the CPU) and the moon view (`UnmirrorView`).
- `sea_pass::ForPass(pass)`: `mirrored`, `landLightScale` 0.5, `landWriteZ` and `smallBump` false in Reflection
  (it replaces `DrawSceneDesc::cullBack` and the `mirrored` of `DrawMoon`, which no longer exist).
- `L3DMeshSubmitDesc::sea` (`sea_pass::SeaDraw`: light, plane, unmirror, argb, specular, per-instance colour) replaces
  the six previous fields (`unlitColour`, `landColourOnly`, `clipBelowSea`, `cutByPlane`, `cutColour`,
  `mirrorInSea`). `u_objectLight.x` comes from `SeaDraw::light`: `Normal` 1, `Constant` 2 (B, z = rgb, w = specular),
  `LastDraw` 3 (B with what the last Draw left), `Cut` 4 (C, y = alpha, z = rgb or −1 = that of each instance,
  w = specular). `u_objectClip` = `PackClip` (x plane, y unmirror); the sky writes 0 (bgfx keeps the last value of
  each uniform).
- `Renderer` entry points (`RendererCut.cpp`): `DrawUnderWater(view, mesh, …, SeaDraw)` and `DrawUnderWater(view,
  entity, SeaDraw)` (the hand with `UnderWater(k_HandColour, k_HandSpecular)`, what it holds and the physics objects with
  `UnderWaterLastDraw()`, the boat with `UnderWater(0xFF303070, 0)`), `DrawCutByPlane(view, entity, SeaPlane, argb,
  specular)` (sharks: specular 0, `push 0` 0x775027) and `DrawFishPlots(view, SeaPlane)` (the net: +0x50 = 0 from the
  ctor 0x8164FE). The order is the binary's: sharks (`fn_00775120` 0x5E4B26), fish and nets (`fn_00824B90`
  0x5E4B2B), the swimmers' slot (0x5E4B4C..0x5E4D76) and the hand's glow (0x5E4D89). The boat reflection
  keeps using `ObjectInstanced`: the hull has no `MorphWithTerrain`. An instance that sticks to the ground is not drawn under the sea: the `DrawUnderWater` of the morphable
  vtables (vt+0x118 of 0x9A2E34 / 0x9A2BFC) is a `ret` (0x80BA40), like their `DrawCutByPlane` (0x80BA50).
- Pass colours (`SetColorSpecular` vt+0x2C before the call): hand 0x65A0A0A0 / 0 (0x5E496E / 0x5E496C),
  creature 0x65A0A0D0 / 0x30 (0x5E4ACF / 0x5E4ACD), swimmers 0xFF303070 / 0 (0x5E4C69 / 0x5E4C68), boat 0xFF303070
  (`mov [eax+0x4C]` 0x5E016C, without touching +0x50: **(inferred)** openblack sets 0).
  - The boat's DrawUnderWater (static 0x811010 → `fn_00850FC0`) is called at 0x5E0178. The held object's reflection is
    at 0x5E49A2 and the physics objects' at 0x6470C6 / 0x64717C.
- **The hand's 0x65 alpha is not visible** (**(inferred)**): `fn_00811010` only sets the table 0xC387C8 (0x8110CF)
  if vt+0x4C = `fn_007F9D80` ((+4 >> 7) & 1) returns 1 (0x8110BF, `test eax, eax / je 0x81114C` 0x8110C2..0x8110C4).
  The hand's LH3D object is `LH3DObject::Create(3)` (`Morphable::MorphInit` 0x61731A → 0x80B5B2, complex vtable
  0x9A3068, whose vt+0x118 is `fn_00813300`: [0xC37D9C] = obj+0x80 and then `fn_00811010`), its flags start at
  0x10009 (0x816537) and a byte search found no call to its vt+0x48 (`fn_007F9D60`, Flags1 | 0x80) in
  the 0x30 bytes after a read of [x+0x482C] (it does not rule out a setter via another path): its `DrawUnderWater` uses the
  normal table and its `AlphaTextured` material takes the texture's alpha.
- Around the hand's `DrawUnderWater`: vt+0x58(0) 0x5E497E (`fn_008168C0`: clears Flags1 0x20; only sets it if
  [0xC38224] ≠ 0) and vt+0x58(edi) 0x5E4991 restores it. **(not ported)**: it is not known what Flags1 0x20 does in this
  draw (`fn_00811010` does not test it).
  - vt+0x58 (`fn_008168C0`) is UseDynamicLighting: it only sets bit 0x20 of +4. The creature's slot also calls
    vt+0x58(0) (0x5E4ADF) after SetColorSpecular (vt+0x2C 0x5E4AD6) and before vt+0x118 (0x5E4AE6).
- The **moon** does use the table 0xC387C8: vt+0x48(1) before its Draw (0x86AC05) and before its `DrawUnderWater`
  (0x86AC3B). Mode 4 → 5 (0x82DD90: same blend and Z, ALPHAOP MODULATE(TEXTURE, DIFFUSE)); `fs_celestial` always
  modulates alpha with `u_colour`, so `DrawMoon` already draws like mode 5 in both passes.
- **PSys mesh atoms with `DrawCutByPlane`**: the bit is the atom's `+0x24 & 4`, not `& 0x10`
  (`fn_00679F20` `test al, 4` 0x679F29; set by `CreateParticle` from +0x5F, 0x6A8B94..0x6A8B9A). With the bit, vt+0xF8
  (0x679F2F, returns the mesh), then `LH3DBoundingBox::CheckRegionOnScreen` 0x868C80 (direct call at 0x679F3C;
  if it gives 0 it is skipped, 0x679F43) and vt+0x11C (0x679F4A) instead of the Draw vt+0x104 (0x679F52); also on the sorted
  path (the return of `fn_00679F60` is `fn_00679F20`, 0x679FBC). openblack: `RenderContext::cutAtomInstances` (with
  `psys::manager::k_DrawByPath` all of them, opaque or not, in `psysAtoms`: with their Z-object in a `Sorted`, at their place inside
  the effect in a `Queued`), `sea_pass::CutAtoms` (default plane, instance colour and specular;
  **(approximate)** vertex alpha 0xFF: `fn_00858BA0` takes it from obj+0x4C & 0xFF000000, 0x858C42 → [ebp−0x24], OR at
  0x858D60, that is the alpha of DrawData+8, which the instance does not carry).
  `psys::mesh_atoms::Instance` carries the bit (`cutByPlane`, from `MeshCreator::drawCutByPlane`) and the specular
  DrawData+0xC (`specular`), which `RenderingSystem` reads; the alpha of DrawData+8 (`alpha`) does not reach the cut. An atom
  with `DrawCutByPlane` and `DrawWithLandscapeColor` is registered once and drawn uncut
  **(inferred: no effect with both is known)**. The on-screen box test 0x679F3C is provided by bgfx's
  clipping **(approximate)**.
- **Slots with no user** (constants and a TODO with an address in `Renderer.cpp`): the creature (`DrawUnderWater` after the
  physics objects, 0x5E4A84..0x5E4AE6, if its block is visible (+0x920 & 1), the block's +0x9BC ≤ [0xC37200] = 100000,
  and < [0x8AB35C] = 6 and +0xA0 < [0x8AB244] = 0.2) and swimming SuperVillagers (list [0xEB9A08], animation
  "M_P_Swim2" 0xBF3598, the swimmers' plane, vt+0x11C 0x5E4C77, after `fn_00824B90` (0x5E4B2B: fish and nets) and
  before the hand's glow 0x5E4D89).
  - The swimmers' loop: each SuperVillager's Draw vt+0x610 (0x5E4BC2) and shadow `fn_00874850` (0x5E4BFB) come first;
    the animation name is compared at 0x5E4C07; the swim rings every 1000 ms are made at 0x5E4C7D..0x5E4D4B.

## Object reflections and hand shadow on objects

**Faithful** (done). The common code is in
[The under-sea pass](#the-under-sea-pass-graphicssea_pass).
- **DrawUnderWater** (static 0x811010 → `fn_00850FC0` per primitive; animated 0x810E20; complex 0x813300): world =
  object × vertex, clip = W2C·(x, −y, z), index order reversed, plane (0, 1, 0, 0) that removes what had y < 0.
  Diffuse = obj+0x4C and specular = obj+0x50, **unlit**. The alternative mode table 0xC387C8 only with Flags1 & 0x80
  (the hand does not use it: the 0x65 alpha has no effect with its mode 4 material).
  - Hand: skipped if hand+0xAC; 0x65A0A0A0; then what it holds (hand+0x8C) **with its own colour**.
  - Creature: its LH3D body if its block is visible, y < 6 and obj+0xA0 < 0.2 (unidentified field); 0x65A0A0D0, specular 0x30.
  - Physics objects (`fn_00646FE0`, array 0xD47814, stride 0x1DC): the awake ones only (the asleep byte +0x19C,
    0x647004..0x647011: no resting proxy, so never a broken house), if y > −r (r = maximum distance from a vertex to the
    centre of mass), with no distance limit. A fragment mirrors its own LH3DObject, the Rock of info 0xD3A930 (0x76E9EC)
    in the ctor colour 0xFFFFFFFF / 0 (0x8164F7), not its FragMesh (inferred; openblack mirrors the piece: pending).
  - The "own colour" is what `fn_00801C90` left in obj+0x4C/+0x50 in its last Draw (called by `PhysicsObject::DrawAll`
    0x646F9F, `MobileObject::Draw`, `Rock::Draw`...): the bilinear land light and the cells' specular, without N·L or fog.
  - Boats (`PetitNavire::PreDraw` 0x5DFF20): **one** `DrawUnderWater` per frame of the hull in 0xFF303070 (then
    `fn_00801C90` gives it back the land light). The branches 0x5E0100-0x5E0190 (mode 0, launch: it also corrects y with
    `GetAltitude` and the shadow) and 0x5E0380-0x5E03EE (mode 1, voyage) are mutually exclusive via +0x30, both with the mirror
    diag(−1, 1, 1) on the hull's track (determinant −1) and the `RotateY(π/2)`: there is no second part. openblack:
    `Renderer::DrawBoatReflection` (mode 2 of `vs_object` with the packed rgb). See [water.md](water.md#the-missionaries-boat-petitnavire).
  - openblack: `Renderer::DrawObjectReflections` in the reflection pass (what the hand holds and the awake bodies of the
    physics list 0xD47814 via `PhysicsObjects::ForEach`, with centre y > −r,
    r = `PhysicsBody::Radius` (was `PhysOb`); those thrown from the hand that are not in physics, with the box radius), each through
    `Renderer::DrawUnderWater(view, entity, sea_pass::UnderWaterLastDraw())` (mode 3 of `u_objectLight` in
    `vs_object`, plane KeepAbove).
- **Dynamic shadow on objects**: at the end of each Draw (static `fn_0080DB30` 0x80E457..0x80E4D7, animated
  `fn_00812170` 0x81311A..0x81317C, vt+0x15C `fn_00810720` 0x810CD6 and `fn_00817930` 0x8185AB, morphable 0x80E74B...),
  if the object has Flags1 0x40 (vt+0x7C), for each `ShadowInfo` in the list (from newest to oldest) with
  `fn_00881030`, si+0xC = 0 (the hand, the creature and the boat, 0x5E11BE; physics objects and SuperVillagers set 1: land
  only), that is not the caster (si+0x464 ≠ obj) nor the complex object's own si (vt+0x1A8 / vt+0x1B8), and whose box
  si+0x2C {x0, z0, x1, z1} touches the mesh's XZ box (centre ± half + position, without rotation or scale; vt+0x1BC =
  `fn_007F9E80`): `fn_0080B050` (mode 6 from the current table, which is already back to the normal one also on an object that
  fades, 0x80E197; colour white; `fn_0084E200` with u = (Wx − x0)/(x1 − x0), v = (Wz − z0)/(z1 − z0), `fn_00880770`:
  **vertical projection**; all the darkening goes in the texture's alpha, with the fade baked in).
  - Z test: the static one and `fn_00810720` set ZFUNC EQUAL before each shadow (0x80E484 / 0x810C8F) and LESSEQUAL
    when done (0x80E4CE / 0x810CF2); the animated one does not touch ZFUNC: the frame's LESSEQUAL remains.
    - The receiver test of a static Draw: vt+0x7C (Flags1 0x40) at 0x80E457..0x80E460; the whole filter
      0x80E46C..0x80E49F; ContainsThisBoundingBox at 0x80E497; the caster test at 0x80E47C; the complex object's own
      si (vt+0x1A8 / vt+0x1B8, 0x80E4A5..0x80E4BB) is the hand's body.
  - **Where**: inside the receiver's Draw, so immediately with an object drawn immediately (without the 0x200 flag,
    0x815F62) and in its slot in the queue with a queued one (0x7FA980 → vt+0x108); never behind the drain ([the queue](#the-single-transparent-queue-lh3dzsorter)).
  - Receivers (Flags1 0x40, `Object::Create3DObject` 0x6365F0 if ShadowsOnObjects): all objects except those that call
    vt+0x78(0) (`xor edx, edx; call [eax+0x78]`): trees (0x749FA3), forests (0x439098), flowers (0x527A5D), magic
    food (0x5FAAC8), food in the hand (pot 12, 0x66D180), the creeds (0x50B46E), the town flags
    (`TownDesireFlags`, 0x746DD4), the one-shot balls (`OneOffSpellSeed`, 0x72A4B4), the shields (MagicShield
    0x72C2B4, PhysicalShield 0x72CCF4), the charge of icons and totems (`TChargingData` 0x72675F, 0x780BBB) and crops when being deleted (0x607EC5). When an
    object is picked up it is removed (`SetHeldObject` 0x816842); when it is thrown it is restored.
  - And those that do not go through `Create3DObject` do not receive: a new LH3DObject has the bit at 0 (the
    `LH3DMeshedObject` ctor sets +4 = 0x10009, 0x816537) and vt+0x78 (`fn_008168A0`) only sets it with an argument ≠ 0 and
    [0xC38220] ≠ 0. So the hand's power-up bands (`Band`, `fn_0068CA30` → `LH3DObject::Create` 0x68CA98) and the
    spell icon's one (`CreatePUBand` 0x727080 → `Game3DObject::Create` 0x63ABB0, which jumps to `LH3DObject::Create`)
    do not receive. The icon's mesh does (`fn_00727190`, vt+0x78(1) at 0x727245).
  - Correction: PSys meshes do not call vt+0x78(0), but vt+0x78 with the creator's byte +0x54 (`mov dl, [edi+0x54]`
    at 0x6A8ACE / 0x6A8D65; the same byte goes to vt+0x80). That byte is 0 in both ctors (0x6A8986, 0x6A8BDE) and
    no property writes it (`DefineProperties` 0x6B37A0 / 0x6B38B0 / 0x6B3970; there is no other write in
    0x6A8000..0x6B4000), so they do not receive either. `ParticleAnimCreator::CreateLH3DObject` (0x6A9760) does not call vt+0x78.
  - openblack: `RendererShadows.cpp`. `CollectShadowReceivers` (when the Main view's objects begin: receivers from
    `RenderContext::entityInstances` with `receivesDynamicShadow`, shadows with `onObjects`), `DrawShadowsOnObject`
    (behind the `DrawMesh` of each opaque mesh in Main, or behind its entry in the drain of `graphics::zsort`, in
    `MainBlended`; with the matrices it was drawn with) and `DrawShadowsOnCutObjects` (the parts of the sharks above the water, right behind
    `DrawCutAboveWater` and not one by one **(inferred: they are opaque and the receiver's Z test already discards what is drawn
    in front)**). A receiver that was not drawn in the frame (outside the view, already fully transparent, or past the
    0x800 cap of the queue) receives no shadow: `ClearShadowReceivers` empties the list at the end of the objects, like the
    tail of a Draw that was not executed (0x80E457..0x80E4D7). ZFUNC Equal, except boned meshes and
    morphable ones (`ZFunc::LessEqualInclusive`: GEQUAL = LESSEQUAL with inverted Z; **(inferred)** that a boned mesh is of the animated class;
    the morphable Draw `fn_0080E550` does not touch ZFUNC around its loop 0x80E768..0x80E874).
    `fs_object_shadow` with `shadow.sh`. `ReceivesDynamicShadow` (RenderingSystem.cpp) also leaves out the one-shot
    balls, the shields and the power-up bands (`HandFxPart` and the `Power_Up_Band` mesh). Detail key `shadowsOnObjects` (levels 3–6).
  - **Morphable receivers** (done): the morphable Draw (`fn_0080E550`, vt+0x108 of
    0x9A2E34; in openblack `MorphWithTerrain`) does not use `ContainsThisBoundingBox` but its own circle test
    (0x80E78E..0x80E857), and draws with `fn_0080AE40` (the same mode table and the same CULLMODE as `fn_0080B050`,
    with the blended vertices of [0xF05180]): R = (obj+0x44 · mesh+0x30) + max(x1 − x0, z1 − z0) · 1.4142
    ([0x932D08] `8104b53f` = 1.41419995, **not** the float closest to √2), the mesh centre +0x18..0x20 times the
    object matrix obj+0x14 and the distance in x, z to the box centre ((x0 + x1) · 0.5 [0x8AA3B4]); it is drawn if
    dx² + dz² < R², strict (0x80E84E). It does not check vt+0x1A8 / vt+0x1B8, only si+0x464 (0x80E782).
    `shadow_math::ReachesMorphable`, tested in `test_shadow_math`. **(inferred)** that `MorphWithTerrain` is the
    morphable class (vtable 0x9A2E34, Get3DType 1): the CITADEL class (Get3DType 8, `CitadelHeart` 0x464B40; vtable
    0x9A2BFC) draws with `fn_00882A40`, which calls the static Draw `fn_0080DB30` (0x882AB5), with
    `ContainsThisBoundingBox` and ZFUNC EQUAL; today no entity of type 8 carries the component (`CitadelArchetype` does not
    add it; `CitadelPart` is type 1, 0x4694B0). The test reads obj+0x14 from the matrix openblack draws; it holds as long as
    no `MorphWithTerrain` entity receives the tweaks of `RenderingSystem` (swaying of fields and trees, the
    tilting and shrinking of trees), which are the original's draw matrix and not obj+0x14 (inferred).

## Cutting by the water plane (`DrawCutByPlane`)

**Faithful** (done).

- **DrawCutByPlane** (vt+0x11C: animated `fn_00811C70`; static `LH3DStaticObject` vt 0x9A2974 = `fn_0080C050`, it is **not**
  a `ret`: the `ret` `fn_00815F90` is only the base vtable 0x9A2748): plane from `fn_00822560`, (0, −1, 0, 0) → what remains
  is **y ≤ 0**, (0, 1, 0, 0) → y ≥ 0; CPU clipping per triangle (`fn_0081D2C0`), per-vertex light `fn_00858BA0`:
  I = 255·(L·n) with the light 0xF03140 (that of `fn_0084BA90`), I < 0 → 90, otherwise 90 + (255 − 90)·I >> 8
  (`[0xC39264]` = 90); rgb = colour.rgb·I >> 8, A = colour.A, the object's specular; the material's mode. Used by
  SuperVillagers with `M_P_Swim2`, sharks (`MSH_SHARK_BONED`: the bottom part before the sea in 0xFF303070 and the
  top part in its Draw with table[255]) and the fish puzzle net (Land 4, static). **It does not apply in Land1**.
  openblack (with [sea_pass](#the-under-sea-pass-graphicssea_pass)): `L3DMeshSubmitDesc::sea =
  sea_pass::Cut(plane, argb, specular, pass)` → mode 4 of `u_objectLight` in `vs_object` (the same integer arithmetic;
  I is stored with `fistp` at 0x858CDF: round to nearest, halves to even, not truncated; + the specular in w) and
  per-fragment discard in `fs_object` (`SeaPlaneDiscard`) instead of CPU clipping; inside the Reflection pass
  the mesh is un-mirrored at y = 0 (`unmirror`; culling goes back to CCW). `Renderer::DrawCutByPlane(view, entity,
  SeaPlane, argb, specular)` and `DrawCutBelowWater` (in the reflection pass, before the fish: the entities with
  `components::CutByPlane`, KeepBelow with the shark's own plane `k_SharkPlane`, 0x774FF5..0x77501A). The top part is called by the object's owner instead of
  its normal draw: `CutByPlane::drawAbove` (the sharks) makes the normal pass skip that instance and
  `Renderer::DrawCutAboveWater` draw it with KeepAbove and `LandLightTable::GetRaw(255)`, in the main pass after
  the instanced meshes. `DrawCutByPlane` uses the `SkeletalAnimation` pose if there is one.
  Hook: `OPENBLACK_TEST_CUT=1` with `OPENBLACK_TEST_SEA`.
- The fish puzzle net (FishPlot, 0x829A30..0x829D54): one static LH3DObject of `Data\MISC\Fishplot.l3d` moved to each
  float (SetPosition vt+0x20 + DrawCutByPlane vt+0x11C); its +0x4C is the LH3DMeshedObject default 0xFFFFFFFF (ctor
  0x8164F7), lit per vertex through `fn_0080C050` 0x80C0FD → [0xC37D8C]. The FishPlot ctor never calls SetColour, and
  the only other writers of +0x4C are the draws at vt+0x100 / +0x110 / +0x130 / +0x154, which the net never goes
  through.
- The shark's colour under the water: SetColorSpecular vt+0x2C at 0x775030, then vt+0x11C at 0x775037.

## Fish-farm fish shoals

**Faithful** (done), except what is said to be missing. The fish puzzle and the rest of the water are in
[water.md](water.md); creating the farms from the script, in
[map-loading.md](map-loading.md#fish-farms-create_fish_farm--create_town_fish_farm).

- **Fish farms** (`FishFarm::CallVirtualFunctionsForCreation` 0x52CC10): rings of radius 2, 4... < 50 × 32
  directions, with the sea flattening **disabled** (`[0xC37BF4]` = 0); the first direction with altitude 0 in two
  consecutive radii gives the centre (x', y of the farm, z'). 15 fish (`fn_00824740`): horizontal `misc0.raw` sprite (flag
  0x40: quad rotated in Y with its local x along the heading), half-width 0.8–1.2, position centre + (±5, −1..0, ±5), heading
  ±π, speed 0.5–1.5, turn speed·(1 ± 0.1)·0.6283; cells 8–23 (frame += dt·speed·25; the cell is taken before the wrap −15·ftol(f/15), so cell 23 shows up;
  `frame_anim::FishFrame`).
  Movement `fn_008248E0` (dt ≤ 0.1 s), shoal target `fn_00824DA0` (centre ± 7, timer 0.5·distance);
  beyond 300 it is not drawn, alpha from 200 (with the original's byte overflow). Drawn in mode 6 before the sea.
  - openblack: `FishFarmArchetype`, `ecs::UpdateFishShoals` (frame game time), `Renderer::DrawFishShoals`.
    Since `fs_water` composes the opaque sea with the reflection texture, "what is behind the sea" is the reflection pass:
    the fish are drawn there **mirrored** (y → −y) and without a Z test, over the reflected land (which in the original does not
    write Z). The same would apply to the underwater cuts.
  - **Scare**: the global splash point 0xEA9F40 / flag 0xEB99F0 are set by
    the **start of the terrain grip over water** (`StartLandscapeGrip` fn_005D1AB0, grip button: growth ring
    7 and the sounds `G_HANDINWATER_01..10` per turn), the creature's footsteps with the foot below y 1
    (fn_00483290) and a physics object falling into the water (fn_0074F2D0). Each shoal within 300 of the camera: target =
    centre + 2·(cos r, 0, −sin r), timer 2 s; visible fish within 8 (3D distance) flee for 2 s with heading
    atan2(fz − sz, fx − sx). The flag is cleared at the end of the frame.
  - **Fishing**: with the action button over the water with no other object below, a visible fish at < 2 (x, z; fn_00824B10) makes
    the farm the object of the action; `NetworkFriendlyStartLockedSelect` 0x52D770 puts in the hand a HandFood of
    amountPickedUpInitially (25) **without removing it from the stock**, with the particles `SF_MultiPickUpFoodFish`
    (`S_Spangle_A` cells 48–63, 40 fps). `ProcessInInteract` 0x52D950 per turn: n = (int)(8 + 62·t²), t = turns/60,
    ≤ 1400 and ≤ 20000 − what is in the hand; `RemoveFood` 0x52CED0 removes n (or whatever is left) and the hand receives **n**
    (quirk); 0 → it ends. They cannot be returned.
  - **Stock**: +0x94, 1400 on creation (GFishFarmInfo 0: foodValue 1400, 16 turns per unit, 4 fishermen); `Process`
    0x52D130 adds 1 every 16 turns. Visible fish = (int)(15·stock/1400): those with a high index disappear first and
    the hidden ones neither move nor are drawn. The last argument of `CREATE_TOWN_FISH_FARM` is the GFishFarmInfo index.
  - openblack: `ecs::SplashWater` / `ProcessFishFarmsTurn` / `FindFishFarmAt` / `RemoveFishFarmFood` (FishShoals.cpp),
    `HandFish.cpp` (`SplashHand`, `TryPickUpFish`, `UpdateFishPickUp`), splash when thrown objects land in
    `CollisionSounds::AttemptToAddSoundEvent` (0x6465B7). Missing: the pitch of the sounds, the help text ("Pick up") and the fishermen.

- **Fish puzzle** (Land 4, the `FishPlot` net and its baited shoals): in [water.md](water.md#fish-puzzle).

## Shadows of physics objects

**Faithful** (done).
- `fn_00646FE0` (from `GLandscape::Draw` 0x5E49DC) → `fn_007FCE80` per physics object that is not at rest (byte
  elem+0x19C = PhysOb+0x174) nor with y ≤ −r: if it has no shadow and casts a static shadow (Flags1 0x1000 / 0x2000) or
  is animated, `fn_008745A0` creates a land-only `ShadowInfo` (si+0xC = 1). Pots, magic
  food, flowers, crops, DeadTree, AnimatedStatic and building pieces do not have one (`SetShadowOnTexture(0)`). It is freed in
  `PhysOb::DeInitialise`. No limit on the number and no detail key.
- Light: the object's position + (0, 15000, 0) (0x9A3C10), not the sun: practically vertical projection. Box = the
  minimal one of the projected vertices (no margin). 32×32 silhouette with 4×2 subsamples per texel (`fn_00806F60`), alpha =
  covered subsamples / 15 (max. 8/15) without writing the outer ring (`fn_00880FC0`, table 0xFA95C4); trees
  through the chroma path (texture with alpha and 2×2 filter). Fade at 50–80 radii from the camera to the ground under the
  object (`fn_00874600`, with the 9-block test), **baked** in nibble steps. On the land
  (`fn_00878350`): t' with H = GetAltitude of the caster (one per shadow), nothing on cells with altitude ≤ 1; mode 6 black.
  All the detail, in [rendering.md](rendering.md#projected-shadows-shadowinfo).
- openblack: one more entry of `graphics::shadow_list` (`PhysicsObjects::ForEach` + `CastsPhysicsShadow` from
  `ShadowList.cpp`), rasterised on the CPU with each vertex's pose, with no cap; it is drawn on each block it touches
  (`Renderer::DrawLandShadows`). `Graphics/PhysicsShadows` and its loop of 16 in `fs_terrain` no longer exist.
- **Static shadow of what is not on the map**: the bake (`fn_008721A0`) takes the casters from the map cells
  (`0x5E2A90` / `0x5E2C30`); picking up an object (`fn_005DC330`) or giving it physics (`Object::InitialisePhysics*`) removes it from
  them until it lands (`EndPhysics` → `InsertMapObject`). openblack: `CastsStaticShadow` excludes the object in the
  hand and those that are flying. Quirk not reproduced: only Fixed/MultiMapFixed rebake the block when they leave; the
  old shadow of a tree or a MobileObject stays on the ground until another change rebakes that block.
- Test: `OPENBLACK_TEST_PHYSICS="1490.2140.760.0,0.0,0.6.3"` with the camera `1450.60.2095.1492.6,2140` and `-n 4000`
  (the rocks fall at ~70 u/s; in the capture they are at a height of ~110 and their three shadows can be seen on the ground).

## Dynamic hand shadow

**Faithful** (done; source: the user's captures of the original, 2026-10-02; see
[rendering.md](rendering.md#projected-shadows-shadowinfo)).

- Original: `CHand::CHand` 0x46BC0B → `CreateDynamicShadow` 0x80C020 (if [0xC3820C] ≠ 0, 1 in the data), a
  complex `ShadowInfo` (`fn_00814FD0`) with si+0x3C = 1 (0x80C037: the fill skips the even subrows, 0x880141,
  at most **4/15**), the light 200 above the hand (0x8151C4, [0x8C7B34]), the base at the hand's y (0x8152B1), t' = 1
  on the land (there is no si+0x464) and the held object (si+0, `SetHeldObject` vt+0x234 = `fn_00816830`, only if
  `IsG3DObjectDrawnInHand`) inside the same texture at full density (0x807532..0x8075B7). It falls on the land
  and on objects (si+0xC = 0).
- openblack: the hand's entry in `graphics::shadow_list`, with `k_HandShadowAsOriginal = true` (`ShadowList.h`):
  32×32, at most 4/15, the base at the hand's y and the held object (an orb taken from the dispenser, for
  example) at full density in the same texture, drawn like the others (on each block and on objects at
  their place in the queue). Compared against four captures of the original
  ([img/original_hand_shadow_over_dispenser.png](img/original_hand_shadow_over_dispenser.png),
  [img/original_hand_shadow_orb_over_dispenser.png](img/original_hand_shadow_orb_over_dispenser.png),
  [img/original_hand_shadow_red_orb_over_dispenser.png](img/original_hand_shadow_red_orb_over_dispenser.png),
  [img/original_hand_shadow_orb_over_ground.png](img/original_hand_shadow_orb_over_ground.png)): light silhouette with the
  fingers, dark round shadow of the held orb, whole orbs on top. With `false` the look from before
  the list returns (64×64, 8/15, on the ground under the hand, without the held object; the old `DrawHandShadowPass`), only for
  comparison.

## Animals: blobs and mesh

Moved to [animals.md](animals.md#blobs-and-mesh-of-the-animals): the animals' EBone blobs
(`fn_0081FFF0`) and the mesh / creation scale.

## Chimney smoke (LH3DSmoke)

**Faithful** (done; deviations at the end). Everything read from the
W1.20 disassembly; the doubtful points (fn_007F8E00, 0x7F9F10, 0x5E4310, fn_005DBC60) were reread when porting it.

- **Creation** (`Abode::CallVirtualFunctionsForCreation` 0x403200): if the mesh has the flag 0x400 (in openblack
  `L3DMeshFlags::HasChimney`, formerly `Unknown11`), `LH3DSmoke::Create` 0x7F8B60 at the chimney = **extra point [1]** of
  the mesh (the [0] is the door) times the object's 3×3 matrix (rotation and scale) + its position (with the altitude of the
  foundations) (`LH3DStaticObject::GetChimneyPos` 0x7F9F10). Colour 0x808080 if it is a workshop, 0xFFFFFF otherwise. Houses,
  crèches, workshops, storehouses and wonders; `MSH_B_AMCN_5` has the point at (0,0,0) (smoke from the base, like the
  original). ARK, ARK_DRY_DOCK and APPLE_BARREL have the flag but are not Abodes.
- **When** (`Abode::Draw` 0x516288): only if the building appeared on screen this frame; off screen the smoke
  freezes. `PresentAtHome` (+0xB6, villagers inside) ≠ 0, or a workshop with the scaffold count (+0xC4) ≠ 0 → state 0;
  otherwise, 0 → 2 (it dies out: each puff finishes its life and is reborn hidden) and, when none is drawn, 3 (dead). No
  day/night, weather or fire condition.
- **Particles** (`fn_007F8E00`, the Z-sorter callback, which simulates while drawing): 10 sprites, initial age i·90
  (in 1/255 s), hidden; angle Random(0, π), random spin direction, spin ±dt·0.765. Frame drift W (the same
  for all 10): the **hand** (at < 15 u from the chimney and speed > 1: `handWind`) or Random(−3, 3) in x and z. On passing
  900 it is reborn at the chimney with τ = age/255; otherwise τ = dt. v' = v + 1.5·τ·W; p += (v + v')·τ/2; rises 2.55 u/s. Cell
  (age/20) & 15 (8 per row of `smoke.raw`), half-width age/450 + 0.5, alpha 79 up to 225 and then linear to 0 at 900
  (integers). Material `g_smoke_mat` (mode 6: SRCALPHA/INVSRCALPHA, Z test without write, no light or fog):
  **at night the smoke stays white**, as in the original. Lifetime 3.53 s.
  - `g_smoke_mat` is made by `fn_0080BBD0` (0x80BC76) and used by `LH3DSmoke::Create` (0x7F8CEC); `smoke.raw` is pure
    white in the cells the smoke uses (0-15), so the colour is the vertex colour alone.
  - The Z-sorter entry is made by `LH3DSmoke::AddDrawing` 0x7F8D30.
  - The workshop countdown +0xC4 is run by `fn_007798A0` / `Workshop::Process` 0x7797F0.
- **Hand wind** (`GLandscape::Draw` 0x5E4310): if the hand's 3D object has a mesh, handPos = its position,
  handSpeed = clamp(|v|·0.1, 0.5, 5), handWind = v/|v|·handSpeed (v = 0 → 0), with v = `GInterfaceStatus::HandVelocity`
  (`fn_005DBC60`: v += 0.6·(Δpos·1000/100 − v), per turn (inferred)). Initial values handPos (−10000, 0, 0),
  handWind (1, 0, 0). It does not read the weather or LH3DAtmos.
- **openblack**: `ecs::components::ChimneySmoke` (src/ECS/Components/ChimneySmoke.h, on the Abode from
  `AbodeArchetype::Create`), `ecs::chimney_smoke` (src/ECS/ChimneySmoke.{h,cpp}: Create, Attach, UpdateHandWind,
  UpdateState, Advance), drawing in `Renderer::CollectChimneySmoke` / `DrawChimneySmoke` (src/Graphics/RendererSmoke.cpp):
  one object per chimney in the back-to-front list of the main pass (`ZObject::smoke`, key the
  squared distance to the chimney), its puffs in order 0..9 with the Sprite shader (`smokea.raw`, premultiplied tint,
  ONE/INVSRCALPHA = mode 6, rotation −angle in the screen plane). `Abode::presentAtHome` is raised and lowered by the
  villagers (0x405FA0 / 0x405FB0), and the windows read it like the original (`Abode::Draw` 0x515F78: +0xB6 ≠ 0 and
  then `IsVisualNight` 0x5575E0).
- **Deviations**: the age step keeps the fraction (the original truncates `(int)(dt·255)` every frame: lifetime
  3.75 s at 60 fps, 6.3 s at 144 fps and smoke stopped above 255 fps, and openblack runs without vsync), like the mists;
  "on screen" is the sphere of the building's box against the frustum (approximate); the hand "with a mesh" = the hand is not
  at the origin (approximate); the random drift is per frame like the original, so its amplitude depends on
  the fps (as in the original). The workshops' scaffold count is missing (no workshops that produce).
- **Hook**: `OPENBLACK_TEST_CHIMNEY=all` (all chimneys smoke).

## Objects that face the camera (billboards)

**Faithful**, except what is marked. Everything that orients itself towards the camera goes through a single API, `graphics::billboard`
(`src/3D/Billboard.{h,cpp}`, glm maths only), with one function for each of the original's modes.

**Conventions.**
- LH3D uses row vectors (p' = p·M, fn_0084BA90). Row k is the image of local axis k; in glm it is column k, with the
  same memory.
- LH3D rotations go the opposite way from `glm::rotate`: `SetAngleY(a)` 0x674360 (rows (c,0,s) / (0,1,0) / (−s,0,c)) is
  `glm::rotate(−a, Y)`. The in-place rotations RotateY 0x5198F0 and fn_0086AFA0 mix **rows**: in glm they go **on the
  right** (`M·R(−a)`). UpdateRuleRotatePrincipalAxis 0x6A1150 mixes the components of each row: it goes **on the
  left** (`R(−a)·M`). It is all in `affine` (was `lh_matrix`; [engine-math.md](engine-math.md#lh-matrices)).

**Pass camera.** `CameraFrame::From(camera)` is built once per pass, with the main camera or the
reflected one. It contains:
- the eye, g_camera 0xEA1DB8;
- right / up / forward;
- the view, W2C 0xEA1D28 (`UpdateWorldToCamera` 0x819690, openblack's lookAtLH), its inverse (SetInverse 0x7FB290)
  and the W2C rotation;
- the near plane [0xE839E0], taken from the projection. **(approximate)** `Game.cpp` computes it as
  `LandFeature::GetNearClipping` 0x5E2F30, but only changes the projection if it moves by more than 0.01, so it can lag
  up to 0.01 behind the original;
- the mist matrix 0xEA1C98 = mat3(right, −forward, up) (UpdateCamera 0x819A62..0x819AF3 and fn_00819F50).
- UpdateCamera (its call with `push 1`, 0x819A41) also keeps the normalised forward D in [0xEA1DD4..0xEA1DDC] every
  update; its CPU readers are 0x442CE2, 0x67C701, 0x6EE726, 0x74A8D8, 0x7FEE9D, 0x7FF0E5 and 0x8799F8. D is the third
  column of the W2C (cells 2, 5, 8).

In the reflection pass, the eye and right / up / forward are still those of the main camera, because
`ReflectionXZCamera` only reflects `GetViewMatrix`. On the other hand, the view, its inverse, the W2C rotation and the mist
matrix do come out reflected. In that pass the two groups must not be mixed. Today only `DrawMoon` and `drawSprite` build
a `CameraFrame` in the reflection, and they only use the view and its inverse.

**`Sprite`.** These are the useful fields of the 0x34-byte LH3DSprite, with the default values of SetToZero 0x8404F0:

| Field | Contents |
|---|---|
| +0x00 | position |
| +0x0C | size (half-width) |
| +0x10 | stretch (half-height = size × stretch) |
| +0x14 | angle |
| +0x18 / +0x1C | origin, in world units; **it is subtracted** |
| +0x20 | colour |
| +0x28 | cell (bits 0-5) and 0x40 horizontal |
| +0x30 | cells per row (8) |

`SpriteQuad` returns the 4 corners in the v0..v3 order of 0x840530 and their UVs. `CellUv` does col·(1/n) +
(8/n)·{0, .125, .125, 0} and row·(1/n) + (8/n)·{0, 0, .125, .125} (0xC390CC / 0xC390DC, 8 = [0x8C2C70]), with the cell
& 0x3F. The triangles are {0,1,2}, {0,2,3}. The `components::Sprite` (night lights, fireflies, dust and the hand's
effects) take their `uvMin` from `CellUv(cell, 8)[0]`. Their `Transform::rotation` is not used, because mode A only has
the angle +0x14.
- The triangles {0,1,2}, {0,2,3} are at 0x840B47 / 0x840B57; CellUv is computed at 0x8408D3..0x84092F (1/n, k = 8/n).
- A sprite with flag 0x80 is drawn in the smoke material [0xEA1ABC] (0x840C01..0x840C38).
- The smoke material is also used by the boats (PetitNavire 0x5E1328) and the SmokyStuff puffs (`SmokyStuff::Create`
  0x823D21).

| Mode (function) | Original | Maths | Users in openblack |
|---|---|---|---|
| `Screen` (mode A) | `LH3DSprite::Draw` 0x840530, flag 0x40 = 0 | Square in the screen plane at the sprite's depth: it is parallel to the screen and does not turn towards the eye. local x = {−s − ox, s − ox}, y = {hs − oy, −hs − oy} (0x840831..0x8408CF). Local x goes to (cos, −sin) on screen and y to (sin, cos) (0x84071D..0x84082B), that is, a clockwise rotation. Angle 0 = no rotation (0x840770). Order TL, TR, BR, BL. Draws nothing if the depth is ≤ near (`InFrontOfNear`, 0x84055D..0x840585) | PSys sprites (`RendererParticles.cpp`, was `RendererPSys.cpp`: all the SF, TownBelief, FireGraphic), DisappearSmoke puffs (was `SmokyStuff`; `RendererBoat.cpp`) and, on the GPU, the chimney smoke and the `components::Sprite` |
| `ScreenSpriteModel` | the same mode A in `vs_sprite.sc` | T(pos)·Rz(−angle)·S(half-width, half-height, 1). The shader adds u_invView·(model·(x, y, 0, 0)) in the −1..1 plane, with v = 0 at the top: it is `Screen` with origin 0. The near cut is done on the CPU | `drawSprite` (night lights, fireflies, dust, hand effects, temple sparkles, camera markers) and `DrawChimneySmoke` |
| `Horizontal` (mode B) | flag 0x40 (0x8405FE..0x840704): `SetHorozontal` 0x6AA093, `GWater::InitialiseCircles` 0x54BA84, fn_00824740 0x8247EF | Ry(angle), with rows (c,0,s) / (0,1,0) / (−s,0,c), plus the position. x = {−s − ox, s − ox}, z = {−hs − oy, hs − oy} (0x84085D). It does not depend on the camera and has no near cut | boat wake, fish-farm fish, water rings, the horizontal branch of PSys (SF_ManaPathNew, light maps) |
| `PlaneOfMatrix` | `LH3DSprite::DrawSpecial1` 0x840CC0 | The mode B square in the XZ plane of a given matrix, rotated about its local Y if the angle ≠ 0 (r0' = c·r0 + s·r2, r2' = c·r2 − s·r0, 0x840CEF..0x840D82). It does not use the sprite's position | the 7 rings of the influence ripple (fn_00827500, `RendererInfluence.cpp`) |
| `YawToEye` (mode C) | inline code: `TownCentre::DrawPSys` 0x69BE76..0x69BE8A, fn_00466BB0, `TownDesireFlags::Draw` 0x746BFC, fn_00719E90, `ScriptHighlight::Draw` | θ = atan2(eye.z − p.z, eye.x − p.x) + π/2 ([0x8C78D8]); axes `affine::AngleY(θ)` (the rows of SetAngleY 0x674360). Local +Z goes from the eye to the object | nobody (influence columns, desire flags, ShowNeeds and ScriptHighlight are not ported) |
| `ParticleYaw` (C') | `Particle3DObj::DrawAt` 0x679FD0, FaceCamera +0x4D, 0x67A032..0x67A1C3 | θ = atan2(d.z, d.x) − atan2(r2.z, r2.x), with d = p − eye in XZ; r0' = c·r0 + s·r2, r2' = c·r2 − s·r0; r1 × HeightStretch | PSys meshes with FaceCamera (`Particles/Creators/Mesh.cpp`) |
| `FullSprite` (D) | FaceCameraSprite +0x4C, 0x67A250..0x67A451 | Identity × scale. Then each row (x, y) := (cos φ·x + sin φ·y, cos φ·y − sin φ·x), with φ = π/2 − atan2(d.y, \|d.xz\|) (0x67A367), and then fn_0067A4A0(ψ), with ψ = atan2(d.z, d.x). Local +Y faces the eye and Z stays horizontal | `Mesh.cpp`, after FaceCamera as in the original; no SF activates it |
| `LookAtCentre` | the bubble: fn_00518720, from `OneOffSpellSeed::Draw` 0x518E90 | d = W − eye, with W = the box centre in the world (0x518746..0x5187B8). If \|d.x\| and \|d.z\| are < 1e-4 (the double [0x8C79D8]), d.x becomes ±1e-4 (0x518875..0x5188B4). D = normalize(d), U = normalize(Y − (Y·D)D). The rows (U×D, −D, U) come from inverting with fn_007FB3F0 (0x518B0C). Then M = T(−c)·R·s and translation W − c·R·s (fn_00518B90, fn_00518BF0, fn_0044CF90). After the 1e-4 nudge, d and U are never zero, so the zero test of 0x5188BC..0x5188ED never fires and is not ported | nobody yet. `Magic/Core/OneOffSpellSeed.cpp` keeps its own copy without the nudge (pending: move it to `LookAtCentre`) |
| `BandToEye` | the power bands: fn_0051A830 (if the byte [0xBE8E8E] = 1, which nobody writes), from `DrawSpellGraphic` 0x51A773 | d = T − eye, with T the band's translation; the same 1e-4 nudge as the bubble; D = d / sqrt(d.y² + d.z² + d.x²), U = normalize(Y − (Y·D)D) with Y = (0, 1, 0) at 0xCC62C0. The column matrix (−D, U, U×D) is inverted with fn_007FB3F0 (0x51AB5A), so its rows are −D, U, U×D. Then M = M·R with fn_0046D9D0 and T is put back. In glm, R·(rotation and scale of the band) | `Worship/SpellSeedGraphic.cpp` (the bands of the ball and the icons) |
| `MoonBasis` / `MoonModel` / `MoonHalo` (E) | fn_0086AC60 and fn_0086A930, read in full | See the moon, below | `Renderer::DrawMoon` |
| `MistBasis` / `MistShrunkSize` (F) | `LH3DMist::Draw` fn_007FA300 0x7FA38F, effect branch 0x7FA483..0x7FA539 | The 9 cells are 0xEA1C98. With the effect, row 0 carries the size and rows 1-2 size / (1 + (k − 1)(1 − \|d.y\|/\|d\|)), with no limit. **(approximate)** 1/\|d\| is computed with `std::sqrt` and not with the InverseSquareRoot table 0x841170. **(inferred)** with d = 0 it returns the size | mists (`RendererMists.cpp`, `mists::Submit`) and clouds (`Renderer::DrawCloud`) |
| `ScreenVelocity` (G) | `UR_OrientSpriteWithVelocity` 0x69A790 (0x69A8ED..0x69A94B) and fn_006840E0 (UR_Flocking) | x = w·right, y = w·up (the W2C rotation); `SetAngleY(atan2(−y, x) + π/2)`. With `Screen`, the sprite's +y lies along the on-screen velocity | `Particles/Rules/Orient.cpp`, `Particles/Rules/Flock.cpp` |
| `RibbonSide` / `RibbonHalfWidth` (H) | fn_0067B3F0 (reads g_camera at 0x67B4BA) | side of each end of the segment = (eye − joint) × (tail − head) (0x67B86C..0x67B924: the view from that joint, first the head and then the tail), which is the same direction as normalize(cross(normalize(segment), normalize(joint − eye))). Vertices = joint ± side·(+0xC of the joint)/\|side\| (0x67B9E6..0x67BA70). That +0xC is the PSR scale (ChainJoint::DrawAt 0x679E9A), the same that a sprite takes as its half-size, so the **half-width is the scale** | `Graphics/RendererChain.cpp` (lightning, forks, gesture trail) |
| `VolumeBlendBasis` (I, was `VolBlend`) | `RenderParticleVolBlendMesh::DrawAt` 0x67CCB0 ([0xC029C4] = 1) | a = normalize(row 0), d = normalize(eye − p), b = normalize(a × d), c = d × b; rows (c, b, d) × scale | nobody (no SF uses ParticleVolBlendMeshCreator) |

**PSys sprites** (`Particle3DSprite::DrawAt` 0x67AE80):
- size = the PSR scale, with a minimum of 0.0001 (0x67AEAF);
- angle = atan2(M[0][2], M[0][0]) of the PSR, except with IgnoreRotation (0x67AF6B..0x67AF87, [0xC029A8] = 1);
- origin: ox = OriginX·size and oy = OriginY·size·stretch (CreateSprite 0x6AA0A8 and 0x67AEC4..0x67AF2D);
- CentreAtBase: y += stretch·size·0.5 (0x67AFB4, [0x8AA3B4]);
- cell = (FileOffset + frame) & 63.
- The 0.0001 is [0x8BF518] (0x67AEA4..0x67AED7).
- [0xC029A8] is 1 in the binary and only read there, so the roll is always applied.
- The cell is put in the flags' low 6 bits at 0x67B0A2..0x67B0BA.
- Their material: CreateMaterial(6) + SetMaterialProperties 0x57E120 (`fn_006AA030` 0x6AA052 / 0x6AA05F): mode 13 or 6
  (12 or 5 with MaterialUpdateZBuffer), Z test on, two-sided.

All of that also applies with SetHorozontal. The `FireGraphic` flames use the origin of their shared sprite 0xDA09E8:
+0x1C = −2·size (0x732382..0x732395), that is, `SpriteOriginY` = −1, and not CentreAtBase. That way the base stays at the
point along the screen's "up".

**Moon (mode E).**
- Basis (fn_0086AC60 0x86AC67..0x86AEBD):
  - v = the position in the camera, n = normalize(v), t = normalize(n.z, 0, −n.x), u = n × t;
  - it is brought into the world with SetInverse(W2C) (0x86AE64 / 0x86AE6F) and × [0xFA2750] = 4.0 (0x86A3F4, which overwrites the
    3.0 of 0x86A3D6);
  - local +Z goes from the eye to the moon and X stays horizontal in the view.
- Mesh: that basis, tilted with fn_0086AFA0(α = [0x9A3BF0] = −0.1309), then RotateY(phase + π) 0x5198F0, then
  × 0.65 [0x8AC420]. In glm it is Rz(+7.5°)·Ry(−(phase + π)).
- Halo (fn_0086A930):
  - corners p ∓ 500(r0 + r1) and p ± 500(r0 − r1) (500 = [0x8C78EC], with the rows already × 4);
  - UV (0.25, 0.25), (0.49375, 0.25), (0.25, 0.49375), (0.49375, 0.49375) (0xEDC304; v0 = bottom left);
  - indices {0,1,3, 3,2,0} (0xEDC310).
- Reflection (fn_0086B010 0x86B662..0x86B69C): the second call, at (x, −y, z) with [0xFA2774] = 1, only draws its halo
  (0x86AC0F skips the moon object). The reflected moon is the DrawUnderWater (vt+0x118) of the first one. With openblack's
  reflected camera:
  - the halo is built with the reflected view;
  - the moon uses the main view (`view·mirror(y)`).

**Chains (`RibbonSide`, mode H).**
- `fn_0067B3F0` normalises the side with InverseSquareRoot (0x67B943); the fsqrt at 0x67BA57 then takes out the
  table's error. An exactly zero side stays zero (0x67BA04..0x67BA39), so a zero-length segment collapses to its
  joint.
- Where two segments meet, both pairs of vertices move to their midpoints (0x67BD2D..0x67BE82); [0xD4EC14], 0 here,
  would copy the previous tail instead.
- UseDynamicLighting (not ported) multiplies the chain colour by clamp(0.6 + 0.4·n·L) (0x67BB10..0x67BCCF);
  `ChainJoint::DrawAt` 0x679E80 also jitters the joints (not ported).
- The chain material (`fn_006AA800`) goes through SetMaterialProperties 0x57E120 at 0x6AA84A.

**Differences with what openblack had before.**
- The rotation of PSys sprites went the wrong way. It was compensated by two rules that also rotated the wrong way:
  - `UR_OrientSpriteWithRandomAngle` (0x6A2100; DefineProperties 0x6AC660: +0x20 RandomAngle, +0x24 DefaultAngle)
    now does SetAngleY((1 − PSysFloatRand(2))·RandomAngle + DefaultAngle) at every step (0x6A2187..0x6A21AC);
  - `UpdateRuleRotatePrincipalAxis` is now `rotate(−dt·AngularVel)` (Z 0x6A116B, Y 0x6A1218, X fn_006A12F0). The
    original also rotates the 4th row of the AtomCore matrix (+0x68..+0x70), which SetAngleY 0x674360 sets to zero;
    it is not ported (**inferred**: it is not used for drawing).

  Since both are fixed, the meshes that use the second one also rotate like the original.
- The origin was added; now it is subtracted.
- CentreAtBase added the whole h·s; now it adds h·s·0.5.
- The horizontal PSys branch had no origin, IgnoreRotation or CentreAtBase, and had the Vs reversed: v0/v1
  go to −hs − oy (0x84085D..0x840897).
- Mode A's near cut was missing (0x84055D..0x840590).
- The moon used the view plane and had the signs of the tilt and the phase inverted.
- The halo's Vs were reversed.
- `MistShrunkSize` no longer clamps \|d\| to ≥ 1. It only changes anything with a mist or a cloud less than 1 m from the eye.

All of these are verified in the binary. Confirmed by the user against the original (2026-10-02): the moon, the
width of the lightning ribbons (half-width = the PSR scale, fn_0081C780), the orb's ring that sometimes covers the
bubble depending on the animation, and that the bubble no longer flickers when its atlas starts over.

Everything else gives the same vertices as before: wake, fish, rings, DisappearSmoke, mists and clouds beyond 1 m,
FaceCamera, chains and the bubble.

**Gaps.**
- (inferred) The w vector of UR_OrientSpriteWithVelocity before 0x69A8ED.
- (inferred) With IgnoreRotation, +0x14 is 0 (fn_006A84C0 not read).
- (inferred) The drawing of light maps as a horizontal sprite.
- (inferred) The 4th row of the AtomCore that UR_RotatePrincipalAxis rotates is not used for drawing.
- (approximate) The near of `CameraFrame` lags up to 0.01 behind [0xE839E0].
- (approximate) `MistShrunkSize` uses `std::sqrt`, not the 128-byte InverseSquareRoot table 0x841170
  (MakeInverseSqrtLookupTable 0x8411D0, plus a Newton step at 0x8411B0..0x8411C2).
- (approximate) The steam and smoke of fire come out centred. In the original they inherit the oy = −2·size of the last
  flame drawn with the sprite 0xDA09E8, because 0x7323F0..0x7325C5 do not write +0x18 / +0x1C. fn_00732200 draws, for
  each fire, the flames, then the steam and then the smoke, so it is that fire's oldest flame, or that of the previous
  fire if it has no flames. So in the original they come out 2 times that size higher. Porting it would require an
  origin per atom in `psys::manager::DrawAtom` and knowing the order of the fires.
- Not ported, though read:
  - the clamping to [0, 639] × [0, 479] of the vertices of an unclipped sprite (0x840A47 / 0x840A85);
  - the fn_007A8DB0 path with [0xEA9EB4] ≠ 0;
  - the bubble's 1e-4 nudge on the vertical (0x518875..0x5188B4): it is in `LookAtCentre`, but
    `OneOffSpellSeed.cpp` does not use it yet. Without it, with the camera exactly on the vertical openblack keeps the
    previous rotation.

## Frame-animated textures

**Faithful**, except what is marked. **The original never blends two frames.** Each user picks a whole frame
(`__ftol`, which truncates, or an integer division; only the hand's glow uses `fistp`, which rounds) and draws only
once. What looks smooth comes from two things: many frames, 15 to 25 per second, and continuous UV
offsets (scrolling). PSys interpolates the frame **number** between two steps and then truncates it: that is
a step, not a blend.

There is no common clock either: each user keeps its own accumulator (per object, or global where the original has it global)
with its own constants. That is why the API has **one function per original clock** and the state is kept by the caller, in
the place where the original keeps it.

The clocks are computed in float because the original runs the FPU at 24 bits: `fn_007DEE00` clears the precision bits
(see [camera-tracks.md](camera-tracks.md)). So each fadd, fmul or fsub on the stack rounds like a float operation,
and the value left on the stack between an `fmod` and the `__ftol` is the float one. That is why, when the `fmod` gives a tiny
negative, the vials' "+ 32" rounds to 32.

**API.** `graphics::frame_anim` (`src/3D/FrameAnim.{h,cpp}`), logic only:
- Engine primitives:
  - `SpriteCell` / `SpriteCellUv`: the LH3DSprite cell, `flags & 0x3F` (LH3DSprite::Draw 0x840530). Its UVs are those
    of `billboard::CellUv`, which is not duplicated.
  - `UvOffset` / `IsAnimatedUv` / `OffsetUv`: `SetAnimatedUV_1` (vt +0xE8, 0x7F9B70; +0x68 / +0x6C). Each draw of an
    LH3DObject copies it to [0xECA62C] / [0xECA630], with [0xECA628] = 1 if they are not both 0. DrawTriangle 0x82F8BE..0x82F8F7
    adds it to each vertex **except** if the material has bit 0x10 of byte +5. fn_0082F920, fn_0082FD70 and
    fn_00884750 add it without checking the bit. [0xECA628] is set at 0x80C42C..0x80C485: the offset is on unless
    both components are 0.
  - `PackUvOffset` (openblack): the transport to `vs_object.sc`, v + 4·round(256·frac(u)). It is exact for all the
    original's users: the bubble's quarters, the vials' 32/256 steps and the whole-pixel cells of
    AnimTextured. Only a SlideU of 1000 frames gets rounded to 1/256.
  - `AnimTexturedCell`: Particle3DObjAnimTextured::DrawAt 0x67A530, read in full. With cells, cols = 256 / W (idiv),
    u = (W/256)·(f % cols) and v = (H/256)·(f / cols), with f unsigned. With sliding, u = W·f / (N·256) and
    v = H·f / (N·256) ([0x8D45CC] = 256). openblack adds a guard: cols ≥ 1.
- Clocks (table below).
- Loaders: `LoadBitmapFromFile` (`GJBitmap::LoadBitmapFromFile` 0x57CA90: only with the exact size, 0x57CAD2;
  min(framesInUse, framesInFile) frames (0x57CADB) of Pitch × Pitch taken from the grid of √n per row of
  `fn_0057CB40`) and `FrameTexels` (one frame, `fn_006CA280` 0x6CA2E3); `land_light::LoadBitmapFile` reads the file.
  - `LoadBitmapFromFile` callers: `ParticleLightMapCreator::GetBitmap` 0x6A9D40 (bpp 3),
    `ParticleMistCreator::GetBitmap` 0x6AA540 (bpp 1 or 3) and `fn_007311A0` 0x7312B6 (S_LMFireBall). `fn_0057CB40`
    (0x57CB40..0x57CC3F) takes frame f from column f % n, row f / n of a grid of ftol(sqrt(framesInFile)) frames per
    row.

| Clock | Original | Rule | Users in openblack |
|---|---|---|---|
| `OneOffFrame` | OneOffSpellSeed::UpdateFrame 0x72A570 | phase = fmod(phase + ms·18·0.001, 16) ([0x981FB4], double [0x982820]); cell (f % 4, f / 4)·0.25 ([0x981FB8]) | the bubble (`one_off::UpdateFrames`) |
| `SpellIconFrame` | DrawSpellGraphic 0x519AD0, vials branch (0x519B79..0x519C1B) | +0x34 = fmod(+0x34 − 15·dt, 32), +32 if < 0 ([0x8D86F0], [0x8D8740], [0x8CF134]); u = (f % 8)·(1/256)·32, v = (f / 8)·(1/256)·32 | the creature spell vials (`seed_graphic::DrawSpellGraphic`) |
| `HandFlowFrame` | PHandFX::Draw 0x68D0C0 (0x68D29B..0x68D374) | +0x58 += dt·(−20); with rate > 0, fmod(.., 64) if it exceeds 64; with rate ≤ 0, fmod(.., 64) + 64 if < 0; f = **fistp** (rounds) % 32; (f % 8, f / 8)·0.125 | the hand's glow (`hand_fx`, computed and not drawn) |
| `ParticleFrameAdvance` (was `PSysFrameAdvance`) | AtomCore, fn_00673EA0 (0x673FB8..0x6740D8) | prev = cur; **only with [0xC029DC] and PlayAnim (+0x118) = 1** (otherwise, 0x673FC6..0x673FDE jump to 0x67406A: neither step nor wrap), cur = dt·rate + prev; with rate > 0, while both exceed 2N, −N; with rate ≤ 0, while either is < 0, +2N | all atoms (`Effect::PostUpdate`, with `Atom::playAnim`), the dust, grains and fish of `HandEffects` |
| `ParticleFrameLerp` / `ParticleFrameIndex` (were `PSysFrameLerp` / `PSysFrameIndex`) | fn_00679920 (0x679A7D..0x679B61) | t' = clamp(t, 0, 5) ([0x9357B8]) when looping and clamp(t, 0, 1) without it; f = prev + (cur − prev)·t'; looping ftol(fmod(f, N)) (+N first if negative), without loop ftol(f) in 0..N−1 | PSys sprites, AnimTextured meshes, TownBelief, FireGraphic, light maps |
| `MistCell` / `MistCellUv` / `MistAdvance` | LH3DMist, fn_007FA300 | +0x84 += ftol(ms·0.255) ([0x9A2BA8]), %= 900 if > 900 (0x384); cell (c·45/900) & 15; UV ((f & 7)·0.125, (f >> 3)·0.125 (+0.25 in the effect branch, 0x7FA466; without it, 0x7FA69E)) | map mists, clouds, storm puffs, PSys mists |
| `MistCell` / `SmokeAgeStep` | LH3DSmoke, fn_007F8E00 | dt = min(ms·0.001, 100) ([0x8AB41C]); age += ftol(dt·255) ([0x8AB270]); cell = MistCell(age) | chimney smoke |
| `FireCell` | fn_007321B0 | ftol(fmod(−25·age, 32) + 32) ([0x999668]); age = SpritePos +0x2C | FireGraphic flames |
| `SteamCell` | SteamGetOffsetFromAge 0x7321E0, fn_0073250A | ftol(fmod(25·age, 32)) ([0x99966C]) | FireGraphic steam and grey smoke |
| `FishFrame` | fn_008248E0 (0x824960..0x8249CE) | dt = min(dt, 0.1) ([0x8AB22C]); +0x1C += dt·vel·25 ([0x8C7BD0]); cell (8 + (ftol & 15)) & 63 **before** the wrap; then −= 15·ftol(+0x1C·(1/15)) | fish-farm fish |
| `LanternAdvance` / `LanternStart` / `LanternCell` | fn_00823570 (0x823599..0x82362D); fn_00823240 (0x8233E4..0x8233F8) | **global** clock [0xEB99C4] += ms, %= 700 if > 700 ([0xC383C8]); a = c·31/700; flame i: (10i + 31 − ((start[i] + a) & 31)) & 31. Start = the **global** table 0xC383BC ({0, 13, 0} in the file): each new light writes ftol(Random(0, 31)) into its three entries, so all lights use the values of the last one created | lanterns and bonfires (`night_lights`) |
| `LeashCell` / `LeashScroll` | fn_00466730 (0x46690A..0x466955, 0x466855..0x46687B) | +0x74 += 10·dt, −= 15·ftol(+0x74/15), cell ftol & 63; +0x60 += 0.5·dt, −= ftol | not ported (citadel leashes) |
| `GoldenShowerCell` | fn_006CA990 (0x6CAB1F..0x6CAB5B) | (t/50 + base + drop) % 32, signed | not ported |
| `CreatureRoomCell` | CreatureRoom::DrawAdditional 0x788630 (0x7889A5..0x7889CC) | 31 − (((GetTickCount() >> 5) + i) & 31): **real** clock | not ported |
| `CursorCell` | CameraModeNew3 0x456BED | (GetTickCount() / 50) & 15: real clock, 20 per s | not ported (3D cursor) |
| `HelpSystemCell` | fn_005C0700 (0x5C0A7A..0x5C0AAF) | [0xD15AB0] += fn_005557E0(); (c / 200) & 15 | not ported (HelpDude) |
| `JCSpecialCell` | fn_00828A70 (0x828CDE..0x828D7A) | f += ms·0.01; if f > 15, f = 0 (no fmod); ftol & 15 | not ported |
| `PlayerSymbolCell` / `PlayerSymbolSpin` | PlayerSymbolSprite::Draw 0x69D7E0 | layer 0: +0xC −= ms·0.02 ([0x937538]); layer 1: +0x10 −= ms·0.023 ([0x937534]); +32 while < 0; ftol & 63. Spin of the second sparkle: +0x14 += ms·0.002 ([0x92A544]), −2π while > 2π. The ctor fn_0069D5A0 sets them to 0 | TownBelief sparkles (one accumulator per symbol) |
| `DisappearSmokeCell` (was `SmokyStuffCell`) | fn_00823F70 (0x8240F9..0x824115) | ftol(life·15) & 63 | DisappearSmoke puffs |
| `DustCell` | fn_00846010 (Dust.cpp) | 16 + ((rand % 16 + ftol(2·age)) & 15) | impact dust |
| `InfluenceScroll` | InfluenceCircle::Draw 0x826D2D..0x826D83 | **global** clock [0xEB9A40] = (c + ms) % 10000 (signed); u = c·0.0001 ([0x9A391C]), v = −c·0.0002 ([0x9000DC]); only on frames that pass the camera gate (y > 100) | the influence border (`RendererInfluence.cpp`) |
| `WaterfallScroll` | DesignedWaterFall 0x5E392E..0x5E3972 | V −= 0.5·dt ([0x8AA3B4]), minus its integer part; SetAnimatedUV_1(0, V) | the Land 3 waterfall |
| `GoolooFrame` | fn_005E6390 | t from 500 ms to 0; x = t/500; UV (2·cos x, 1.7·sin(0.7·x)); material byte +4 = 255 − ftol(255·t/500) | ported: `ecs::object_ghosts` (the ghost of an object a store took, a merged pot, a hand pot put down); the material byte as ALPHAREF not applied (**inferred** use) |
| `RotatingUv` / `RotatingUvClock` | RenderParticleGJMeshRotatingUV::DrawAt 0x67CBA0 and GameUpdate 0x6C8BC0 | Draw: lerp(+0x24 → +0x2C, t), lerp(+0x28 → +0x30, t) with t = DrawData +0x14 (0x67CBA8..0x67CBC1); the period (+0x3C, +0x40) is subtracted while it exceeds it (0x67CBC8..0x67CBFC; nothing if negative). Step: the rule adds dt·SpeedU/V to the **target** +0x34/+0x38 and `GameUpdate`, at the end of `PostUpdateAtoms` fn_00673EA0 (0x674080, `vt+0x108`), raises +0x34 and +0x2C by one period while both are below −2·period (0x6C8BDF..0x6C8C5A) and lowers them while both exceed +2·period (0x6C8C5B..0x6C8CCC) — they move as a pair, so the difference the draw interpolates does not change —, and then copies +0x2C → +0x24 and +0x34 → +0x2C (0x6C8CCD..0x6C8CE2) | SurfRevol discs |
| `ChainScroll` | fn_0067B3F0 (0x67BE88..0x67BED5) | +0x3C += ms·rate·0.001, fmod(FrameHeight/256), + that if < 0 | PSys chains |
| `ChainSegmentUv` | fn_006C8920 | see "Chains" below | PSys chains |

- Constants: 0.001 [0x8AA3B0] / [0x8AC418]; 1/256 [0x8D86CC], [0x9357A8], [0x938EBC]; 0.125 [0x8AB620]; 1/15
  [0x8C9D38]; the influence's 10000 is an idiv at 0x826D2D..0x826D59.
- MistCell: c·45/900 is computed as × 0x91A2B3C5 `sar 9`; c = 900 is kept (the wrap is c > 900), so c/20 reaches 45:
  the cells run 0..15, 0..15, 0..13 (14 at c = 900) and back to 0, and cells 14 and 15 are skipped once every 900.
- HandFlowFrame: the rate is +0x5C (−20) and N the byte +0x60 (32); the wrap is at 2N. The original writes the result
  into the hand mesh's vertices, not through SetAnimatedUV_1.
- ParticleFrameLerp: looping is +0x119 LoopAnim; ParticleFrameIndex's result goes to DrawData +0x10.
- RotatingUv only runs while [0xC029B8] = 1 and writes [0xECA62C] / [0xECA630] with the tiling forced; its t =
  DrawData +0x14 is the draw fraction of the step that `PSysManager::AddDrawing` 0x6797D4 keeps at +0xB0. The chains'
  scroll is also gated by [0xC029B8].
- Lanterns: the village light alpha is [0xEB99BC] and the light list [0xEB99B8].
- Leashes: LeashCell runs from `CitadelHeart::DrawNow` 0x46733D; LeashScroll calls SetAnimatedUV_1(+0x60, the colour
  row n × 0.125, or 0.375) at 0x4668A8; constants 10 [0x8C8404] and 0.5 [0x8C8400].
- HelpSystemCell: `fn_005557E0` gives `g_delta_time` capped to 500 when g_game+0x205A28 is 1, else `g_game_time_inc`;
  the cell is (c/200) & 15 (× 0x51EB851F `sar 6`), 5 a second.
- Gooloo (GoolooGooloo 0x5E6540 → `fn_005E6390`): drawn with the offset, then drawn again at (0, 0) with mode 10; its
  material byte +4 is [0xEA1AB4]; constants 500 [0xBF3588], 1.7 as the double [0x92B338] (1.7000000476837158) and 0.7
  [0x900AE0]. Callers: `Object::DoDeleteObjectAndTakeResource` 0x63AAA3, `Pot::ApplyThisToObject` 0x66DE4C (a pot
  merged or put down), `MobileStatic::ApplyThisToObject` 0x608C72 (a gate stone), `Tree` (the sacrifice).
  openblack: `ecs::object_ghosts` keeps {mesh, matrix, ms left} in the world effects' state (no entity, so nothing of
  the game state); `magic::Update` lowers the time by the frame's game ms (`frame_anim::GhostStep`; **inferred**: the
  game step, not the real one); `RenderingSystem::CollectMeshAtoms` draws each ghost as two Sorted mesh atoms, the
  second with `mesh_atoms::Instance::mode` = 10 (AlphaTexturedAlphaAdditiveChroma). **Approximate**: the material
  byte is not used as the alpha reference, and the two draws are two Z objects at the same point (the original draws
  them one after the other).

**Chains.** fn_006C8920 gives the UVs of segment i of S = joints − 1:
- k = ((i + 1)·T − 1) / S, b = k·S / T, n = (k + 1)·S / T − b and j = i − b, all in integers. T is NumTexturesForWholeChain,
  or S when it is −1 (CreateChain 0x6AA8DC).
- F = FileOffset + (k = T − 1 ? FrameOfHead : k = 0 ? FrameOfTail : 0).
- uv0 / uv1 = (F·W, H·j/n) and ((F+1)·W, H·j/n); uv2 / uv3 = the same with j + 1. All × 1/256 ([0x938EBC]) and the scroll
  +0x3C added to the four v.
- That is, **V runs along the chain and U crosses the ribbon** over a column of W pixels. The creator's default
  values are FrameHeight 64, FrameWidth 32 and NumTexturesForWholeChain −1 (0x6AA739..0x6AA747).
- The scroll rate (+0x4C) is only set by UR_SimpleBeam (SpeedV +0x48, 0x6762EE) and `UR_Plasma::CreateArc` (its
  AtomData +0x54, not read, × SpeedV +0x58, 0x676898); the chain ctor 0x6C8830 sets +0x3C = +0x4C = 0. Neither rule
  is ported, so the scroll of all openblack chains is 0.
- Chain sheet fields: +0x1C FrameHeight, +0x20 FrameWidth (ctor 0x6AA740: 0x20), +0x24 FrameOfHead, +0x28
  FrameOfTail, +0x30 the textures over the whole chain, +0x34 FileOffset (copied by CreateChain 0x6AA8B8..0x6AA8EB).
  fn_006C8920 divides by S (idiv 0x6C8936) and T (0x6C893E, 0x6C8949) unchecked, and by n in float (0x6C896B,
  0x6C8971); DefineProperties allows −1..32 for T, so T = 0 would fault there.
- fn_0067B3F0 makes four vertices per segment (0x67BA82..0x67BB0D): v0 = head + side, v1 = head − side,
  v2 = tail + side and v3 = tail − side. It gives them uv0..uv3 (0x67BEE4..0x67BEFD). fn_0081C780 copies each vertex's UV
  as is into LH3DP3::Table1 +0x18 / +0x1C (0x81C9C7..0x81C9D0) and draws with DrawTriangle 0x82F810 (0x81CCB2). That is
  why U = F·W goes on the +side.
- Then 0x67BFAC..0x67BFEE puts the same scroll in [0xECA630] ([0xECA62C] = 0, [0xECA628] = 1), and DrawTriangle
  0x82F8BE adds it again: the material of fn_006AA800 only has bits 0 and 2 of +5. (An earlier reading said that the
  strip used fn_0082F920. That is not so: the call at 0x81D152 is in another function, the one at 0x81CCD0,
  because fn_0081C780 ends at 0x81CCBE.)

**Deliberate fixes** (the original is different from what openblack did):
- PSys:
  - the frame is kept in [0, 2N) with its previous one (fn_00673EA0), and the index is chosen as in fn_00679920 (fmod and
    +N only once; before it was fmod(fmod + N));
  - without PlayAnim there is neither step nor wrap (0x673FC6..0x673FDE), so a negative frame stays as it is;
  - a non-looping atom with a negative rate jumps to ~2N and shows the last frame, not the first. No .zzz in the
    game has LoopAnim 0 with FrameRate < 0 or with RandomiseFrameDirection, so it is not seen in the game. There are 169
    creators with the PlayAnim property (137 sprites, 21 light maps, 6 AnimTextured meshes, 4 animation ones and 1
    animation-with-camera one), and the ten with a negative rate (fireballs, SF_Flash, the tornado and the rain cone)
    all loop.
- PSys mists: each atom carries its own counter (`Atom::mist`). It starts at ftol(Random(0, 16)) & 15 (0x7F95F8) and only
  advances if the mist appears on screen. Before there was a single global counter.
- Storm puffs: the counter only advances if the puff exceeds alpha 5 (AddDrawing) **and** its sphere is on
  screen (`mists::InView`, the test of LH3DMist::AddDrawing 0x7FA7F0). It is no longer (approximate).
- Fish: the cell is taken before the wrap (cell 23 can show up) and the wrap is −15·ftol(f/15).
- Lanterns: the start of each flame comes from the global table 0xC383BC, which every new light rewrites with
  ftol(Random(0, 31)). So all lights run in phase with the values of the last one created (before, each light had
  its own). The clock is integer and only runs if the town's alpha is not 0 and there are lights (0x82357A..0x823593). The halo is
  cell 56 of `smoke` ((flags & ~7) | 0x38, 0x8233BC..0x8233EB).
- TownBelief: the sparkles carry their accumulator per symbol (+0xC, +0x10 and the spin +0x14, from 0), advanced with the
  whole ms of each frame. Before it used the global time (−s·20, −s·23).
- Hand glow: the frame is rounded (fistp 0x68D323), not truncated, and the wrap is "> 64", not "≥ 64".
- Chains: the UVs of fn_006C8920 (before, U ran along the chain with the whole texture repeated), the creator's default
  values, FileOffset and the scroll. In addition, uv0 goes on the +side (before on the −side: U was
  mirrored) and the half-width is the scale, not 0.5·scale, so **the ribbons come out twice as wide**.
- Creature spell vials: the UV animation of 0x519AD0, which was missing before.
- Power bands: `billboard::BandToEye`, see [billboards](#objects-that-face-the-camera-billboards).
- SurfRevol discs: the UV offset is interpolated between two steps (`frame_anim::RotatingUvClock`, DrawAt
  0x67CBA0 with t = DrawData +0x14 and GameUpdate 0x6C8BC0 in full). Before, the rule wrapped the value into [0, period) every
  step and the draw used it as is, so the disc rotated in jumps of one PSys step.

**Same as before, bit for bit:** the bubble, the flames and the steam (cell 32 on the first frame was already there), the
chimney smoke, DisappearSmoke, the boat wake, the rings (fixed cell `& 63` of the 8×8 sheet of `smoke.raw`,
fn_005E5100), the map mists and the clouds, the waterfall, the food piles (0x51C0FD), the AnimTextured cell
layout, the mesh and UVs of the SurfRevol discs, the dust and the hand. The dust when
gripping land and the grains and fish when picking up food now go through `ParticleFrameAdvance` / `ParticleFrameIndex`: they give the
same cells, except for the rounding of adding dt·rate instead of multiplying age·rate.

**Gaps.**
- (approximate) All the mist and smoke clocks keep the fraction of ms·0.255 (or dt·255) from one frame to the
  next. The original's `ftol` would stop the animation above 250 fps. `MistAdvanceExact` is the formula as is.
- (approximate) Storm puffs start with the counter at 0, not at Random(0, 16) & 15: it is the same cell 0.
- `graphics::lh3d::Random` (src/3D/LH3DRandom.h) has been removed; `game_random::crt::Random` replaces it (Random 0x81D180 on the
  CRT's `rand()`, seed 1 at startup: `__initptd` 0x7D2323; the `srand(time)` at 0x577721 only runs when saving
  `creature.lhp`). It is shared by the map mists, the PSys mists, the storm puffs, the rain and the camera
  shake. (approximate) the original has one seed per thread; here one.
- TownBelief takes g_game_time_inc from `game_clock::FrameGameMs()` (0x69D855; before, approximately, from the wall clock).
- The lanterns (fn_00823570), the sky clouds (their movement, their atlas counter and the sky alignment,
  `Renderer::UpdateClouds`), the map mists (`CollectMists`, fn_007FA300) and the chimney smoke
  (`CollectChimneySmoke`, fn_007F8E00) also take g_game_time_inc from `game_clock::FrameGameMs()`. Before it came
  from the wall clock, scaled by the game speed and capped at 100 ms, and the lanterns kept the fraction of
  ms (`WholeMilliseconds`, which is removed: the game clock already gives whole ms and keeps the rest of the turn itself). The
  mists and the smoke are collected once per frame, only in the main view.
- (inferred) That S_Fire is drawn in 8×8 like S_SpriteSheet3.
- (inferred) GoldenShower: t in milliseconds. Gooloo: that the material's byte +4 is the ALPHAREF.
- HandEffects (dust when gripping land, grains and fish when picking up food) is still a hand-made copy of effects that in
  the original are PSys (SF_GripLandscape, ER_MultiPickup). Its frame clocks are already the PSys ones.
- Not ported, with their clock and their test in the API: Gooloo, GoldenShower, the leashes, the creature room and
  the citadel world map, HelpDude, the 3D cursor and JCSpecial. Also HandGlow / fn_0083F270, the scroll that does not belong to LightSheet but to the object of
  fn_0083F100 / fn_0083F210, read by fn_0084F910.
## Meshes stuck to the ground (land_morph)

**Faithful**, except what is marked. Everything that conforms to the terrain goes through a single API, `openblack::land_morph`
(`src/3D/LandMorph.{h,cpp}`), and on the GPU through a single include, `assets/shaders/land_altitude.sh`. Every height is
`LH3DIsland::GetAltitude` 0x803090: on the CPU `LandIsland::HeightAt` (exact, 16.16), on the GPU `LandAltitude` (the same
logic in float: `split` diagonal and flattening next to the sea), at the point's world x, z (`fn_004427B0` /
`fn_00653150`: x·65536·0.1 truncated, [0x8AC408], [0x8AC404]). In the original there is no single routine: there is one
height and four algorithms that apply it.

| # | Algorithm | Original | When | API | Users in openblack |
|---|---|---|---|---|---|
| A | Cut the mesh by the terrain and raise each vertex `y += H(v) − H(origin)` | `fn_00686980` + cut `fn_00686D90` | once on creation (0x6867F9), only with `DoRaiseAboveLandscape`; without the per-frame "breathing" (0x686805) | `SplitByPlane`, `CellPlanes`, `RaiseAboveLandscape` | `Particles/Rules/SurfRevol.cpp`: `SF_TeleportVortex`, `SF_SpellDispenserVortex` |
| B | *Melting*: a delta per vertex `(H(v) − H0) / scale`, in model space, along the local Y | `UpdateMelting` 0x8168F0 + morphable Draw 0x80E550 | on creation (`Snapshot`); `PhysicalShield` on every draw (`Live`) | `components::MorphWithTerrain{mode}`, `Melting`, `MeltingDeltas`, `LandMelting` (GPU), `ObjectProgram` | vs_object_hm_instanced: morphable buildings, fields, piles, BigForest, physical shield, ark and dinosaur, ground marks |
| C | *Bake*: the same delta written into the vertices | FragMesh `fn_007F72B0`; ClampToLandscape `RenderParticleGJMesh::DrawAt` 0x67C313; MeltBorder 0x816350; citadel vt+0x208 `fn_00882B10` | FragMesh when breaking; ClampToLandscape every frame, all primitives, no cut | `Bake`, `Raised`, `BakeAgainstY` | `ECS/Physics/FragMesh.cpp`; `SurfRevol.cpp` (`SF_LandscapeVolcano*`, `SF_LandscapeVortex*`); flames of a morphable object (`ECS/Fire/FireGraphic.cpp`); the totem point over the town centre (`GetExtraPos` 0x80FF20, `AbodeArchetype.cpp`); the temple's outside (`TempleExteriorMorph::BakeToLand`) |
| D | Geometry made on the ground, `y = H + constant` | blobs `fn_0081FFF0`; InfluenceCircle `fn_008265F0`; leash `fn_008491B0`; creature quads `fn_0081F360` | on creation / every frame | `OnGround`, `k_BlobLift`, `k_LeashRibbonLift`, `k_CreatureQuadLift`, `InfluenceCurtain` | blobs (`Renderer::DrawHumanShadows`); picking of morphed objects (`HandPlacement.cpp`); the influence border (`ECS/Influence/InfluenceCircles.cpp`) |

**A, cut and raise.** `fn_00686980(M, mesh)` takes the mesh into the world with the atom's matrix (`fn_00673E40` at
0x6867E8: the local frame of `fn_00673DB0` in the hierarchy, `fn_006752D0`), gets the box of all the primitives
(`fn_006C99E0`: centre = (max + min)·0.5 and half-axes = (max − min)·0.5) and cuts **only the first primitive of the
first group**:

| Step | Detail | Address |
|---|---|---|
| Indices | x0 = −1 − ftol(−0.1·min.x), x1 = 1 − ftol(−0.1·max.x), likewise in z (= ⌊min/10⌋ − 1 .. ⌊max/10⌋ + 1 with x ≥ 0) | 0x6869EF..0x686A41, [0x8C7B10] |
| Planes | x = 10i (n = (1,0,0)), z = 10j (n = (0,0,1)), x + z = 10k and x − z = 10k (n = (s,0,±s), s = 1/√2 in double, through (10i, 0, 10·z1); i from x0 − dz to x1 and from x0 to x1 + dz, dz = z1 − z0) | 0x686A53, 0x686AF9, 0x686BDB, 0x686C8B; [0x8AB414], [0x8AC410] |
| Plane d | −n·p with `fn_00453F50` ((z z' + y y') + x x') | 0x686AB2..0x686ACC |
| Cut | dist = ((y ny + z nz) + x nx) + w; dist ≤ 0 counts as negative; if the three signs are equal there is no cut; A = the first vertex whose sign is the product of the three | `fn_00686D90` 0x686E3B..0x686F9A |
| New vertices | t = −dA/(dO − dA); position, UV and normal = A + t(O − A) (the normal not renormalised); each colour byte cA + ((cO − cA)·ftol(255t) >> 8); diffuse, specular and normals only if there is one per position | 0x686FEB..0x68732B, [0x8AB270] |
| Triangles | n1 (edge AB) and n2 (AC) at the end; tri := (A, n1, n2), (B, C, n2) and (B, n2, n1) are added; only the triangles that already existed are tested | 0x687386..0x687413 |
| Raise | H0 = H(M+0x24, M+0x2C); y = (H(v) − H0) + y in the first primitive | 0x686D01..0x686D55 |
| Back | the inverse of M (`SetInverse` 0x7FB290) | 0x686D61..0x686D78 |

- A GJ mesh primitive is 0x94 bytes: +0x08 positions, +0x1C diffuse ARGB, +0x30 specular ARGB, +0x44 UVs, +0x6C
  triangles, +0x80 normals. `fn_00686D90` cuts the colours and normals only when there is one per position
  (0x686DAA..0x686DC3), and always cuts the UVs.
- The box of `fn_006C99E0` starts with min = 1e7 (0x4B189680) and max = −1e7; centre and half are computed at
  0x6C9A3C..0x6C9AF9 and turned back into min / max at 0x6869BE..0x6869EB.
- `fn_00686980` exits with no group (0x68699F) or no primitive in the first one (0x6869B8).
- The distances go into the static buffer 0xD4E730; each lerp step is stored to a float (`fn_0044CF90`, the LHPoint
  ctor 0x442700, `fn_004605F0`; UVs `fn_0067FEC0` / `fn_0067FEE0`, normals `fn_0044E9F0`); the colour weight is stored
  as a byte (0x687194), the channels lerped at 0x687185..0x68724D and the specular at 0x68726A.
- The plane given to `fn_00686D90` is (n, −n·p) (`fn_0045A7D0`, `fn_00453F50`, fchs); s = 1/sqrt(2.0) is formed at
  0x686B93..0x686BC3.
- The raise order (H(p) − H0) + p.y is the same at 0x686D3D..0x686D4E, 0x67C373..0x67C383 and 0x73235A..0x732366.

Since `GetAltitude` is linear within each land triangle and the cuts follow the cell edges and their two
diagonals (whatever the `split` bit), the cut mesh follows the terrain **exactly**. openblack does it in
`ZR_SurfRevol::MakeSurface`, when creating the surface, with the atom's frame in the hierarchy (`Effect::LocalToGlobal` /
`GlobalToLocal`), and leaves it in local space: when drawing it moves and scales with the atom, like the original's local
deltas. **(approximate)** The frame is the one `surf_revol::Collect` uses when drawing (rotation · baseScale · ruleScale, without
the Y stretch of `fn_00673DB0`): the M of `fn_00673E40` is taken as the drawn PSR of `fn_00673EA0`, and
`Collect` does not apply the stretch. Both users have stretch 1 (no `StretchVertically` nor rules that
change it). Before, each vertex was raised when drawing and with no cuts, so the disc's chords cut through the folds
of the terrain. `HeightAboveLandscape` and `RaiseAboveLandscapeRadius` are not read by anyone (0x6B30EF, 0x6B312A).

**B, melting.** `UpdateMelting` 0x8168F0 exits if there is no delta buffer ([+0x80] == 0) or if vt+0x1B0 ≠ 0 (0x7F9BF0 =
0 in both morphable vtables); a0 = H(+0x38, +0x40), inv = 1/[+0x44]; for each vertex of each submesh,
w = v·M + pos and delta = (H(w) − a0)·inv (0x816A5E..0x816A77); a0 and 1/[+0x44] at 0x816915..0x816977 (the
inverse at 0x816964); each vertex x = ((z·m2c + y·m20) + x·m14) + px and z = ((x·m1c + z·m34) + y·m28) + pz
(0x8169E1..0x816A18). The Draw 0x80E550 adds it to the model's y before the
matrix. In openblack it is done by `LandMelting` in vs_object (programs `ObjectHeightMap*`): in the world it is
column 1 · (H(w) − H(origin)) / scale, with column 1 of the drawn matrix (its up axis) and the scale of
column 0 (rotation times uniform scale; the sways only touch column 1). With the column (0, s, 0) it is
exactly `y += H(w) − H(origin)`, as before. The only tilted morphed object is the ripe field: its sway (`Field::Draw`
0x528570) shears column 1 along z, so its vertices rise H − H0 and also shift
1.75·sway·(H − H0) in z, as in the original (before they only rose). The morphable classes (Get3DType
1 / 8) are in `MorphWithTerrain.h`. The physical shield is one (Get3DType 0x72CE50 = 1); the magic one is not (0x72C340 → the
static one).
- **Snapshot / Live (approximate).** `MorphWithTerrain::mode` records when the original takes the deltas: `Snapshot` on
  creation, `Live` on every draw. `Live` is only `PhysicalShield`: `CallVirtualFunctionsForCreation` 0x72CD23,
  `SetUpPhysOb` 0x72CEB8 and `DrawShield` 0x72D01E, after interpolating the matrix and the scale. The shader computes them on every
  draw for both: frozen deltas would be needed per instance and per vertex, and instances share the
  mesh. They only differ if the land changes after the object is created, and in openblack it only changes in
  `FlattenLandUnderTemple` (`CitadelArchetype.cpp`), while the map is loading.
- **One program.** `land_morph::ObjectProgram(shaders, morph, pass)` is the only choice between `ObjectInstanced` and
  `ObjectHeightMapInstanced` (and their `Shadow`): the main pass, the blended one, the object reflection and the hand
  shadow on objects.
- **Cut by the plane.** The `DrawCutByPlane` of the morphable vtables (vt+0x11C) is 0x80BA50, a `ret`: a cut morphable
  object is not drawn. `Renderer::DrawCutByPlane` exits likewise. Today no morphed object carries `CutByPlane` except the
  food pile of `OPENBLACK_TEST_SEA=...,pot` with `OPENBLACK_TEST_CUT=1`, which is no longer drawn cut.

**C, bake.** `Bake` / `Raised` do `y = (H(v) − H0) + y`, the original's float order:
- FragMesh `fn_007F72B0` (v.y − (H0 − H) on each FragVertex, 0x7F7510..0x7F7563; only if `IsStaticMorphable`,
  vt+0x1F4 → [0xE920E8], 0x7F6FF5): `FragMesh::FromEntity`.
- **ClampToLandscape** (`RenderParticleGJMesh::DrawAt` 0x67C313..0x67C38C; the byte +0x22 set by 0x686432 from the
  rule +0x6C): every frame, all primitives, **no cut**, with H0 at the position of the drawn matrix. Only
  the 5 `SF_Landscape*` carry it (terrain volcano and vortex, `FunctionIndex` 1-3). It is in `surf_revol::Collect`.
- The flames of a morphable object (bit 0 of +0xB5, `fn_00732220` 0x7322A9..0x7322D2 and 0x73230A..0x732366): the same
  delta on each flame.
- MeltBorder 0x816350 (only 0x49D303) and the citadel's `fn_00882B10`: `BakeAgainstY`. `fn_00882B10` is the
  alignment morph's blend (`ProcessAlignement`, vt+0x208), and its bake (`v.y − (pos.y − H)`, 0x882DBE..0x882DD7, the
  world x, z summed in the melting's order) lays every blended vertex on the land. It is called from
  `Citadel::Process` at 0x462FE2 and from `InitTemple` at 0x8829AF. The temple's outside uses it
  ([magic.md](magic.md#the-temples-outside-alignment-and-size)); MeltBorder is not ported.

**D, on the ground.** `OnGround(ground, xz, k) = H + k`. The villager and animal blobs put each point at
H + [0xEAA3C4] (= [0xC381DC] = 0.2, from 0x81E350). The picking in `HandPlacement` puts the origin of a morphed object at
H + the pile's sinking: **(approximate)**, because the drawn origin is the stored y; it makes no difference as long as the stored
y is that one. `InfluenceCurtain` is the curtain of `fn_008265F0`, read in full:

| Datum | Value | Address |
|---|---|---|
| Segments N | clamp(ftol(2πr·0.05), 8, 250) | [0x8AB210], [0x8C7BD4], 0x826659..0x826670 |
| Vertices | 3 per segment, plus 3 that close the ring at angle 0: (cos·r + cx, (H − H0) + H0 + {0, 20, 40}, sin·r + cz) | [0x8C7658], [0x8CF300] |
| U | += (1 − ftol(2πr·(−1/111))) / N | [0x9A3930] |
| V | += min(ftol(r/60), 6) / N; rows v, v + 0.2 and v + 0.4 | [0x8C5818], [0x8AB244], [0x8C7A44] |
| Colour | the player's, [0xEA9EFC + 4·player] | 0x82695B |
| Triangles | (b, b+3, b+4), (b, b+4, b+1), (b+1, b+4, b+5), (b+1, b+5, b+2), b = 3i | 0x8269DA..0x826A34 |

The curtain's caps: 250 segments at 0x826666 and 6 turns at 0x826818; the u step 0x826760..0x8267A8, H0 + 0 / 20 /
40 at 0x8267AC..0x826805, the v step 0x826809..0x826842, the angle 2πi/N at 0x826851..0x826878, the closing column at
angle 0 at 0x826A57..0x826B93.

The leash (`fn_008491B0`: edges at H + 0.1, [0x8AB22C]; used at 0x8497E8 / 0x849818) and the creature quads
(`fn_0081F360`: H + 0.15, [0x8CF110]; used at 0x81F778) only have their constant: they are not ported.

**Ground marks** (`ecs/GroundMarks`). `fn_00825240` (list 0xEB9A00) is an `LH3DObject::Create(1)` of mesh
0x251 (`TreeRootsPile`). It melts into the land once (0x8252C3), lasts 15000 ms and fades out in the last second:
`fn_00825350`, from 0x5E6197, sets alpha ftol(life·0.255) with life ≤ 1000 ([0x9A2BA8]), subtracts `g_game_time_inc` and at 0
deletes it with `fn_00825300`. It has two users:
- the crater of an uprooted tree (`fn_008251F0` ← `fn_0074BD20`, scale (x + z extent)·scale·0.3, [0x8AB23C]);
- the mark of an explosion on dry land (`fn_008251C0` ← `UR_Explosion::InitCollection` 0x67E395: rotation
  `PSysFloatRand(2π)` with `SetPosition` 0x423140, scale [0x9357D4] = 8).

The list is emptied with the map (`ClearAllStuff` 0x82AED0, from `GGame::ClearMap` 0x552F22): `ground_marks::Clear` in
`Game::LoadMap`. **(approximate)** `g_game_time_inc` is an integer of ms that is subtracted as is (0x8253B1..0x8253C0); the
openblack step is float, so the fraction waits for the next frame.

Not ported: the `SmokyStuff::Create(pos, 1, 1, 0xFFFFFFFF)` of 0x8252EB, because SmokyStuff mode 1 is not
ported (the crater keeps the grip dust).

**Not part of this family.** Shadows on the land (`ShadowInfo`, `fn_008745A0` / `fn_00874850` / `fn_00878350`:
they redraw the land itself; see [Shadows of physics objects](#shadows-of-physics-objects)), the
foundations (`GetAltitudeFondation` 0x63ABC0: a rigid sinking), the flattening under the temple (0x882730: it edits the
land), the mist (`LH3DMist`, static Draw) and flat sprites (flag 0x40 of `LH3DSprite::Draw`).

**Gaps.**
- (approximate) Snapshot and Live are the same on the GPU (above).
- (approximate) `LandAltitude` in float versus the 16.16 arithmetic of 0x803090.
- (inferred) The fire's +0x98 / +0xA0, which gives the flames' H0, is the object's position.
- Not ported: the temple entrance and the half-built temple (0x4676CD, `DrawPartialyBuilt` 0x816AD0), the scaffold
  (`Scaffold::SetPhantomMesh` 0x6E8B03), the other users of `GetExtraPos` 0x80FF20 (vt+0x1CC, no direct
  calls; the only ported one is the totem, which does the same sum, (H(p) − H(pos)) + p.y, 0x8100B7..0x8100D0), and the
  curtain, the leash and the quads of D.

## Render modes and materials (render_modes)

**Faithful**, except what is marked. In the original all the blending, alpha test, Z writing and texture stage
of a draw come from **a 16-byte material** (`LH3DMaterial`, `LH3DRender::CreateMaterial` 0x82FD30) and **one of
19 mode functions** (table 0xC38728). In openblack all those draws ask a single API for their bgfx state,
`openblack::graphics::render_modes` (`src/Graphics/RenderModes.{h,cpp}`); nobody writes the blending or the Z by hand any more.

**The material.** `CreateMaterial(mode, texture)`: `new(0x10)`, `inc [0xECA654]` (0x82FD40), +0 mode, +4 = 0
(ALPHAREF), +5 = 0 (flags: bit 0 two-sided, bit 2 wrapping, bit 4 no UV offset), +8 texture,
+0xC = 0xFF0000FF (0x82FD59; nobody reads it, (inferred)). The texture does not belong to the material: smoke.raw is shared by mode 6
[0xEA1ABC] and mode 13 [0xEA1AC4] (0x80BC7D / 0x80BCB8); atmos.raw, AtmosMaterial (mode 6, +5 |= 1 | 4: 0x835C58,
0x835C6F..0x835C79) and AdditiveMaterial (mode 13, +5 |= 1: 0x835C61). In the API: `Material` (the mode and the flags
+5), `State(material, options)` (the material's culling if the draw does not set another) and `materials::k_Smoke`,
`k_SmokeAdditive`, `k_Misc0`, `k_Atmos`, `k_AtmosAdditive`, used by their eight draws, and `k_InfluenceCircle`
([0xEB9A18], burn.raw, mode 6, +5 |= 1 | 4 at 0x826D18 / 0x826D27: the influence border). The texture and the wrapping
(+5 bit 2, `SetD3DTillingOn/Off` 0x82FF10 / 0x82FF50 with `g_b_need_tilling` [0xECA614]) are set by each draw; in the
L3D meshes, `Primitive::wrap`.

- The flags +5 are also written by `fn_0057D4C0` (0x57D4F0); SetMaterialProperties sets bit 0 (two-sided) at
  0x57E1B7..0x57E1C9. `fn_0057E1D0` applies SetMaterialProperties to every primitive of every sub-mesh (from
  PGetSharedMesh 0x57DF24, when MaterialProperties +3 is 1).
- `g_smoke_mat` [0xEA1ABC]: smoke.raw [0xEA1A98], mode 6, +5 |= bl = 1 (0x80BC8D; bl set at 0x80BBF3). Users: the
  chimney smoke, the mists and clouds (`fn_007FA300` 0x7FA30E), the boats' wake, the SmokyStuff puffs, the influence
  ripples and the halo / puff of the advisor spirits.
- [0xEA1AC4] (mode 13) is two-sided (0x80BCC8). [0xEA1AB0] is misc0.raw, mode 6, made at 0x80BD1B, two-sided
  (0x80BD2B).
- [0xD19C8C] is CreateMaterial(13, misc0.raw [0xEA1A90]) with +5 = 0, one-sided (`fn_005DF9C0` 0x5DFA07): made once by
  PLAY_JC_SPECIAL 0 and freed by `Intro::ReleaseAll` 0x5DFCA3; the intro light.
- AtmosMaterial is [0xEDC368] (made at 0x835C36), used by the rain streaks; AdditiveMaterial is [0xEDC364] (0x835C49),
  used by the hand's glow on the sea (0x5E4281).
- [0xEB9A18] is made on the influence border's first draw (0x826D0E..0x826D2A), CreateMaterial(6, burn.raw [0xEA1A9C])
  with ALPHAREF 0; bit 0x10 is clear, so DrawTriangle adds the UV offset. `fn_0080BBD0` makes another material from
  the same texture, [0xEA1AC0] (mode 6, +5 |= 4 only).

**The inline SetMaterial** (some 105 copies, e.g. `SetupThing::DrawLine` 0x412662..0x4126BD): calls
`g_set_render_mode_data[0xECA618][mode].fn(m, stage 0)`, sets the wrapping and `CULLMODE = ((~m[5]) & 1)·2 + 1`
(1 NONE, 3 CCW). In the API: `Select(mode, table)` and `CullFor(twoSided, mirrored)`.

**The 19 modes** (`k_Modes`, table 0xC38728, dumped from the executable; 2, 6, 9, 10, 11, 16 and 18 reread):

| Mode | Function | Blend | ATEST | ZWRITE | Stage 0 alpha | L3D type |
|---|---|---|---|---|---|---|
| 0 | 0x82D470 | no | no | yes | no texture | Smooth |
| 1 | 0x82D5C0 | SA/ISA | no | yes | no texture (does not touch the stage) | SmoothAlpha |
| 2 | 0x82D820 | no | no | yes | texture | Textured |
| 3 | 0x82D920 | SA/ISA | no | yes | texture × diffuse | TexturedAlpha |
| 4 | 0x82DC20 | SA/ISA | no | yes | texture | AlphaTextured |
| 5 | 0x82DD90 | SA/ISA | no | yes | texture × diffuse | AlphaTexturedAlpha |
| 6 | 0x82DF10 | SA/ISA (0x82DF6A / 0x82DF91) | no | **no** (0x82E063) | texture × diffuse | AlphaTexturedAlphaNz |
| 7 | 0x82D6F0 | SA/ISA | no | no | no texture | SmoothAlphaNz |
| 8 | 0x82DAA0 | SA/ISA | no | no | texture × diffuse | TexturedAlphaNz |
| 9 | 0x82E080 | SA/ISA | yes | yes | texture | TexturedChroma |
| 10 | 0x82E830 | **SA/ONE** (0x82E87C) | yes (0x82E88E) | yes | texture × diffuse | …AdditiveChroma |
| 11 | 0x82E9C0 | SA/ONE | yes | no (0x82EAAF) | texture × diffuse | …AdditiveChromaNz |
| 12 | 0x82EB50 | SA/ONE | no | yes | texture × diffuse | …Additive |
| 13 | 0x82ECD0 | SA/ONE | no | no | texture × diffuse | …AdditiveNz |
| 14 | 0x82DD90 | = 5 | | | | (the land) |
| 15 | 0x82E470 | SA/ISA | yes | yes | texture × diffuse | TexturedChromaAlpha |
| 16 | 0x82E6A0 | SA/ISA | yes | no (0x82E78F) | texture × diffuse | TexturedChromaAlphaNz |
| 17 | 0x82D820 | = 2 | | | | — |
| 18 | 0x82E2A0 | **ZERO/ONE** (SRCBLEND 5 at 0x82E2F3 and 1 at 0x82E320) | yes | yes | texture | ChromaJustZ |

ALPHAFUNC is GREATEREQUAL for all of them (0x82CBA6). No mode touches ZFUNC, fog or culling. The table's word
(1 in the blended ones) is not read by anyone.

- Addresses per mode: 2: ABLEND = ATEST = 0 (0x82D841..0x82D862); 6: blend 0x82DF45..0x82DFA8; 7: ZWRITE 0 at
  0x82D807; 8: ZWRITE 0 at 0x82DBF6; 10: Z at 0x82E91F; 11: blend 0x82EA0C, ATEST 0x82EA1E; 16: ATEST
  0x82E6EC..0x82E700.
- Mode 14 is the land blocks' material (`fn_007FEDB0` 0x7FEDE2, `LH3DIsland::Create` 0x80442A).
- ZFUNC EQUAL (3) is also set by the object fade (0x80EBD6); the frame's LESSEQUAL is SetRenderState(0x17, 4) at
  0x82CCC1..0x82CCC5.

**The global alpha table** 0xC387C8 (`k_GlobalAlphaModes`, `Table::GlobalAlpha`): 0, 1 → 1; 2, 3, 17 → 3; 4, 5 → 5;
9 → 15; the rest, unchanged. It is chosen by the Draw of an object with its own alpha (vt+0x4C = `fn_007F9D80`, bit 7 of obj+4;
0x80DF09). In openblack, the objects with `components::Alpha` (in the queue if their mesh has the 0x200 flag, otherwise
immediately) and the translucent PSys mesh atoms (`L3DMeshSubmitDesc::table`).

**ALPHAREF** (`AlphaRef`): the material's +4, or [0xECA65C] if the switch [0xECA658] is set. With the table
0xC387C8, modes 9 and 15 scale it: `max(0, ftol(ref·A·(1/255) − 5))` (0x82E15C..0x82E1CE, 0x82E557..0x82E5C9;
[0x900058], [0x8AB6E4], [0x8AA398]), with A the alpha of the object's diffuse [0xC37D8C]. D3D tests the alpha that **comes out of
stage 0**: the texture's in 9 and 18 (SELECTARG1, 0x82E120 / 0x82E384), texture × object alpha in 10, 11, 15 and
16 (MODULATE, 0x82E510 in 15). `PrimitiveAlpha(drawn mode, table, ref, A)` gives `fs_object` the two uniforms:
`u_skyAlphaThreshold.y` = ALPHAREF / 255 (−1 without test) and `.w` = the stage alpha (0 none: modes with no blending or
test; 1 the texture; 2 texture × diffuse). The shader discards if `round(a·255) < ref`. The drawn mode is the real
one: the primitive's through the table, or the forced one (`L3DMeshSubmitDesc::mode`), so an additive atom (13) or the
hand shadow (6) has no alpha test, as in the original. Static and physics shadows
(`fs_static_shadow`) use the same `PrimitiveAlpha` with the normal table ((inferred)).

- AlphaRef reads [0xECA65C] or +4 at 0x82E166..0x82E17E.
- `SetMaterialProperties` 0x57E120 branch sites: 4 → 6 at 0x57E126; no alpha → 3 at 0x57E138; additive → 13 at
  0x57E142 (`cmp byte [ecx], 1`); the Z test at 0x57E14C; the switch at 0x57E182.

**What each draw sets** (`StateOptions`): ZFUNC (LESSEQUAL 0x82CCC5, EQUAL for the shadows on objects 0x80E488,
ALWAYS for drawing on top), the ZWRITEENABLE 0 written by hand (2D rectangles 0x81E64C, sea 0x879FD9, reflected
land 0x5E48C5..0x5E4900) and openblack's own: writing destination alpha, MSAA, the premultiplied
ONE/INVSRCALPHA formulation of mode 6 (smoke, sprites, land; same colour), the sea that composes the reflection in its
shader and the primitive type. `k_ModelPass` is the opaque model pass. `ModeFromProperties` is
`GJUtils::SetMaterialProperties` 0x57E120 (4 → 6; no alpha → 3; additive → 13; with Z 6 → 5, 13 → 12, 8 → 3, 16 → 9; without Z
5 → 6, 12 → 13, 3 / 2 → 8, 9 → 16), used by `L3DSubMesh` and the particles.

**Who uses it.** L3D meshes (`L3DSubMesh` and `Renderer::DrawSubMesh`: the material type is the mode); the clouds,
mists, smoke, boat, rain, blobs, fish, rings, sun, moon, sea, hand glow in the sea, land, hand
text, screen fade, sprites and the PSys sprites, chains and surfaces, the hand shadow on objects (all
primitives in mode 6 with ZFUNC EQUAL), the additive PSys atoms (mode 13) and the mesh viewer. Left out, because
they are openblack's own techniques with no original mode: the MAX / MIN passes of shadows and rivers and the footprints.

**World triangles, SurfRevol and the influence border** (original).
- `LH3DTech::Draw3DWorldTriangle` 0x81C090: its last argument 0 takes `g_world_to_clipping` as it is
  (0x81C09F..0x81C128; `RenderParticleGJMesh::DrawAt` sets the identity world matrix first, 0x67C8ED..0x67C979); no
  specular (0x81C2BF); `g_NoBackfaceCull` = material +5 bit 0 (0x81C30C..0x81C319); CULLMODE ((~m[5]) & 1)·2 + 1
  (0x81C556..0x81C58F); SetMaterial through the current table (0x81C48E..0x81C4A0); ONE DrawTriangle for the whole
  primitive (0x81C5B1), at once, no Z object. An exploded piece's GJMesh +0 is its source primitive (0x680C49).
- Its indexed sibling `fn_0081C780` is called by FragMesh (`fn_007F7ED0` 0x7F86D8 / 0x7F8730) and
  `RenderParticleGJMesh::DrawAt` (0x67CAEE): the same T&L with `g_world_to_clipping` (0x81C783..0x81C80A), culling
  0x81CA0A..0x81CA1C and 0x81CC57..0x81CC90, SetMaterial 0x81CB79..0x81CBA1.
- `RenderParticleGJMesh::DrawAt` takes `fn_0081C780` instead of Draw3DWorldTriangle (branch 0x67C9CD) when the
  primitive has as many speculars (+0x30, count +0x38) as colours; ZR_SurfRevol sizes both to NumU × NumV
  (0x685A0E..0x685A3D), so the SurfRevol discs always go that way. With a DrawData alpha ≠ 0xFF DrawAt selects the
  table 0xC387C8 (0x67C9B7..0x67C9C0), which leaves modes 5, 6, 12 and 13 unchanged.
- The SurfRevol material: CreateMaterial(6) + SetMaterialProperties (`ZR_SurfRevol::ModifyAtomCollection` 0x6863EC /
  0x6863F9): 13 / 6, or 12 / 5 with MaterialUpdateZBuffer; colour = texture × diffuse + specular, alpha = texture
  alpha × diffuse alpha.
- `RenderParticleGJMeshRotatingUV::DrawAt` 0x67CBA0 never reads [0xC0215D]: a surface has no Z object of its own on
  any path.
- The influence border: `InfluenceCircle::Draw(1)` 0x826C90 is called once a frame from `GGame::Process3dEngine`
  (0x54E3CB..0x54E419, the call at 0x54E414), after everything drawn at once and before FinishFrame's drain; the
  camera gate and the middle row's alpha come from g_camera.y [0xEA1DBC] (0x826CA9..0x826CF3); the middle rows' alpha
  is set at 0x826F0D..0x826F57; [0xEA9EA0] = a copy of `g_world_to_clipping` (0x826D8F..0x826E11);
  Draw3DWorldTriangle(3N + 3, positions, colours, uvs, 4N, indices, [0xEB9A18], 1) (0x826F59..0x826F80) draws every
  circle, the invisible ones too; [0xECA628] = 0 at 0x826F8B.

**Fixes when unifying** (before, openblack's table and the branches of `DrawSubMesh`):
1. Modes 10 and 11 came out SA/ISA (now SA/ONE) and 11 wrote Z.
2. Mode 16 wrote Z.
3. Mode 18 painted colour with SA/ISA; now ZERO/ONE, Z only.
4. ALPHAREF: the −5 was always applied and without the A/255 factor (`fs_object`, `fs_static_shadow`); now it is the exact +4
   except 9 / 15 with the table 0xC387C8.
5. The Z-less primitives (6, 7, 8, 16) of a fading object wrote Z.
6. Entries 14 and 17 of the table were {no blending, no Z}; now 14 = 5 and 17 = 2 (latent: there are no L3Ds with those types).
7. The hand shadow on objects set SA/ONE on additive primitives (12, 13); now they all go in mode 6
   of the shadow's material: `fn_0080B050` 0x80B06A..0x80B08B does the SetMaterial of [si+0x460] (`CreateMaterial(6)`
   in `fn_0087FD50` 0x87FE12) through the current table, and `fn_0084E200` draws each primitive with no state of its own.
8. A chroma primitive with ALPHAREF 0 (or a 9 / 15 that fades with `ref·A < 1530`, which gives 0) blends with its
   texture's alpha (mode 9: SELECTARG1 0x82E120, SA/ISA); before it came out opaque.
9. The hand shadow on objects with its material's culling: +5 = 0 (0x87FE12) gives CULLMODE CCW
   (`fn_0080B050` 0x80B0AD..0x80B0E6) on all primitives; before, two-sided ones also received it from behind.
10. The alpha test and the stage 0 alpha are those of the drawn mode (see ALPHAREF): a fading 9 (→ 15) tests
    texture × A (before, the texture's raw alpha: with ref 0x96 and A = 128 texels from 70 were kept instead of
    from ~140); a fading 2 (→ 3) blends with texture × A (before, only A); in an additive atom (13) types 0 / 2
    use texture × diffuse (before, the whole quad) and the chroma ones lose the alpha test, the same as under the hand
    shadow (6).

In `AllMeshes.g3d` there are no primitives of types 10, 11, 16 or 18 (the counts in [L3D material
blending](#l3d-material-blending)): fixes 1-3 are seen in the miracle meshes and in those that go through
`SetMaterialProperties` (a Z-less `TexturedChroma` becomes 16). Fix 4 is seen on the edges of all the chroma ones (trees with
cut-off 0x96: a little thinner).

**Gaps.**
- (inferred) The object's alpha as a byte: `AlphaByte` rounds `1 − [0][3]` of the instance.
- (inferred) The alpha test comparison in rounded bytes (`fs_object`, `fs_static_shadow`).
- (inferred) The moon without the Z of mode 4 (0x82DC20) and the sky with its meshes' modes.
- (inferred) The modes of the chains (`fn_006AA860`, not read) and of SurfRevol's specular pass (13).
- (inferred) The water rings and fish shoals with [0xEA1AC4] / [0xEA1AB0]: only the references 0x54BA72 and
  0x8247CC, not decoded inside the routine.
- (approximate) The fish with ZFUNC ALWAYS: the original leaves LESSEQUAL (0x82CCC5); it makes no difference because the reflected
  land below did not write Z (0x5E48C5).
- (approximate) The sea without culling: its material (+5 = 4, 0x5E5474; `GLandscape::Draw` passes it [this+4] at 0x5E4E85)
  gives CULLMODE CCW (0x879F54..0x879F86), which keeps all the rows of the original's sea; openblack's sea mesh
  is not those rows.
- (approximate) With the "texture" stage alpha (4, 9, 18) `fs_object` still multiplies it by the object's alpha; in
  the original it is the texture's alone (SELECTARG1). It is the same as long as the object has alpha 255 (outside the
  fades, which switch to 5 / 15, and the plane cut).
- Not ported: the forced ALPHAREF of its writers (burning tree `fn_0074B3A0` 0x74B4E0..0x74B51E, which
  `FireGraphic.cpp` calls "render mode 230"; `TownArtifact::Draw` 0x51CBB3; fixed cut-off of 10 in `fn_005E6350`
  0x5E649C, `Scaffold::Draw` 0x6EA730, `fn_00826470` 0x826488; FragMesh `fn_007F7960`). `AlphaRef` already accepts it.
- Object fading by alpha test (`fn_0080E940` and copies 0x80F0A0 / 0x80F3D0) is another system; openblack does
  the crosshatch pattern of `fs_object`.

## Test hooks

In [openblack-internals.md](openblack-internals.md#debug-environment-variables):

- `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fixed at a fraction of the window (the hand shows up in captures); use interior
  points, on the edge it moves the camera.
- `OPENBLACK_TEST_PHYSICS="x,z,height,vx,vy,vz[,scale[,n]]"`: falling rocks, with their physics shadows.
- `OPENBLACK_TEST_CUT=1` with `OPENBLACK_TEST_SEA`: cut by the water plane.
- `OPENBLACK_HAND_TEST_FISH=1` and `OPENBLACK_TEST_SPLASH="x,z"`: fishing and scaring the fish.
- `OPENBLACK_TEST_CHIMNEY=all`: all chimneys smoke.
- Billboards: `OPENBLACK_TEST_SEED=FIREBALL` / `LIGHTNING_BOLT` with `OPENBLACK_MOUSE_AT` (PSys sprites in the hand:
  origin, CentreAtBase and rotation); `OPENBLACK_TEST_FIRE` (flames); `OPENBLACK_TIME_OF_DAY=22` with `OPENBLACK_CAMERA_LOCK`
  (the moon, centred and at an edge); `OPENBLACK_TEST_ONESHOT` (the bubble).
- Animated textures: `OPENBLACK_TEST_ONESHOT="<seed>,x,z[,pu]"` (the bubble; with pu, the bands that face the
  camera; with a creature vial, its 8×4 sheet), `OPENBLACK_TEST_DISPENSER`, `OPENBLACK_TEST_FIRE` (flames),
  `OPENBLACK_HAND_TEST_FISH=1` and `OPENBLACK_TEST_SPLASH` (fish and rings), `OPENBLACK_TEST_SEED=LIGHTNING_BOLT`
  (chains), `OPENBLACK_TIME_OF_DAY=22` (lanterns), `OPENBLACK_TEST_WEATHER` (storm puffs),
  `OPENBLACK_TEST_CHIMNEY=all` (smoke).
- Transparent ordering and bubble: `OPENBLACK_ORB_TRACE=1` writes per frame the place and key of each
  `ZR_SurfRevol` disc and of each one-shot ball bubble in the sorted list, with the phase, the frame, the packed
  `[1][3]`, the alpha and the clipping of each bubble (see
  [openblack-internals.md](openblack-internals.md#debug-environment-variables)). Scene: a dispenser with
  `OPENBLACK_TEST_DISPENSER` and the camera fixed with `OPENBLACK_CAMERA_LOCK`.
- Transparent queue: `OPENBLACK_ZSORTER_TRACE=1` writes once per second how many entries the queue has of
  each class (models, clouds, rain tiles, boat sprites, effects, surfaces, ribbons, mists, smoke,
  sprites, hand), those lost to the cap and the extreme keys. Scenes: the Land 1 sky with
  `OPENBLACK_CLOUD_SEED=7`, a storm with `OPENBLACK_TEST_WEATHER="x,z,60"`, the boat with `OPENBLACK_BOAT_TRACE=1`
  and the hand with `OPENBLACK_MOUSE_AT`.
- Meshes stuck to the ground: `OPENBLACK_TEST_SPELL="PHYSICAL_SHIELD,x,z,..."` with `OPENBLACK_TEST_SHIELD_SHOT` (the physical
  shield melts into the land), `OPENBLACK_TEST_DISPENSER` and `OPENBLACK_TEST_TELEPORT` (the cut discs),
  `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` and `OPENBLACK_TEST_EXPLOSION_SHOT` (the ground mark; `UR_Explosion` is only
  in `SF_BeamExplosion*`), `OPENBLACK_TEST_TUG`
  (the crater); `OPENBLACK_SPELL_TRACE=1` writes `Explosion: ground mark`.
- Render modes: the test `test_render_modes` (the tables, the states of each site before and after and the fixes);
  scenes with `OPENBLACK_HAND_TEST_TREE` and `OPENBLACK_CAMERA_LOCK` (chroma edges), `OPENBLACK_TEST_ONESHOT` and
  `OPENBLACK_TEST_DISPENSER` (fades and additives), `OPENBLACK_MOUSE_AT` (hand shadow on objects).

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x70911A, fn_007F98E0, 0x7F9900, LH3DObject::AddDrawing 0x815B95..0x815BEC` | feature 19: LH3DObject (+0x40) SetDisappear(0) (vt +0x98 clears obj+4 bit 0x8; IsDisappear vt +0x9C); AddDrawing fades a Disappear object with the distance (obj+0x4C alpha = 255 x part left before g_last_distance [0xEA1AF4], then SetDrawWithGlobalAlpha(1)) | `ThingJcSpecial case 19` |
| `0x84BAA3` | fn_0084BA90's untextured-primitive branch: material colour x object colour (with fn_007ACF70) | `fs_object.sc (u_materialColour.w > 0)` |
| `0x82E181` | where render modes 9 / 15 take ALPHAREF from the table 0xC387C8 (material alpha ref scaled by the object's alpha, - 5) | `fs_object.sc alpha test, render_modes::AlphaRef` |
| `0x48EF10` | LH3DCreature::DrawNow: trunc(c l / 255) per channel via the 0x80808081 multiply | `argb_colour.sh MultiplyDiv255` |
| `0x84BB90..0x84BC1D` | fn_0084BA90's per-vertex model light loop (on the CPU over D3DTLVERTEX) | `model_light.sh ModelLightI / Factor / Diffuse` |
| `0x84BBB5` | fn_0084BA90: fstp of 255 (n . l) to a float32 before the fistp (not copied) | `model_light.sh ModelLightI` |
| `[0xE9FE48]` | bone matrices used by the boned model-light path and by fn_00858BA0 (0x858F77) for positions only | `model_light.sh ModelLightLocal, vs_object.sc cut branch` |
| `0x7F80AE..0x7F8116` | FragMesh fn_007F7ED0: transforms its world-triangle vertices itself | `vs_world_triangles.sc` |
| `0x81C9BE..0x81C9C4` | fn_0081C780 (indexed Draw3DWorldTriangle): copies each vertex's specular (the plain one writes 0, 0x81C2BF) | `vs_world_triangles.sc a_color1` |

## Pending

- FragMesh draw: `Abode::Draw`'s on-screen gate `CheckRegionOnScreen` 0x51609D is not ported (it only saves time);
  the fire tint of a burning broken building is taken from `Burning()` (not verified against 0x5160A6); (not
  verified) whether leaving the CPU-drawn sub-meshes out of RenderPass::Main / MainBlended also drops a broken
  building from the Main view's special draws (the sea cut `DrawCutByPlane` and the under-water pass of a held or
  flying object), which the original draws through the object's own LH3DObject (to check with captures).
- Model lighting, copies still to be unified with `model_light`:
  - `RendererRevolvedSurface.cpp` (was `RendererSurfRevol.cpp`): the GJ mesh goes unlit (`UseLighting` not ported; that it is active is **(inferred)**).
- LH3DColor arithmetic, what is still to be moved to `argb_colour`:
  - after the instance repacking (the `u_objectLight` transport already goes through `sea_pass::SeaDraw` and
    `UnpackRgb24` in modes 2 and 4): the tint's alpha (the wolf, `SpellFlock.cpp`: (0xFF·a)>>8 with translucency
    decided with the raw alpha, 0x51C724; the shield 0xFE); the PSys atom's specular (DrawData+0xC, read at 0x67A012
    and 0x67A023: missing in `psys::mesh_atoms::Instance` and lost on both paths); the +0x10C specular of the icons
    (with it, the white tint of the worship site icons, 0x519672); that `DrawBuilding` never applies fog, burning or
    not (0x517F90..0x518046 does not call `fn_007FEB30`: Abode 0x516129, MultiMapFixed 0x5180A6, WorshipSite
    0x5193E9, SpellIcon 0x519668, Totem 0x51ABC3; the "fix 7" of `LandLightOf`); `LandLightOf` gives all `SpellIcon`s
    {Cell, no fog}, but those of a town centre (`TownCentre::Draw` 0x5164B2 → `fn_0080BEC0`) go with bilinear light
    and fog; the inline colours not ported: the burning physics prediction object (`PhysicsObject::DrawAll`
    0x646F81..0x646F8C: white + glow), `Object::DrawOutOfMap` (0x51C837..0x51C84F) and `CitadelHeart::DrawNow`
    (0x4670DD..0x4670EE: tint +0xA4, specular vt 0x5A4); the +0xD0 specular with alpha (see above, `Heal.cpp`);
  - elsewhere in the code: the copies in `src/Particles` (Mist 0x67A6C1, `TintWithPlayerColour` 0x6A865C, Storm
    0x6D2C21, SurfRevol, Heal), `NightLights`, `LandLightTable` and `RendererChain` / `RendererParticles` (was `RendererPSys`; `ToAbgr`);
  - `ECS/Fire/FireGraphic.cpp` (two exact fixes): `TreeDrawColour` must clamp with `ecs::TreeBrightness()`, not
    with 255 (0x74B47B); `CharringGrey` must be `255 − ceil(175k/256)` (0x730585..0x7305D7; with k = 255 openblack
    gives 81 and the original 80);
  - `fn_00809D80` / `fn_00809DE0` (colour per mesh part, 0x80A290 / 0x80A2A6) and `LH3DCreature::DrawNow` have no
    user in openblack yet; `fn_007ACF70` exists only on the GPU (`fs_object.sc`, in float without truncation).
- Fish shoals: the pitch of the sounds, the help text ("Pick up") and the fishermen.
- Shadows of physics objects: the 2×2 filter of the trees and the rebaking of the static shadow when a tree
  or a MobileObject leaves.
- Reflections and dynamic shadows of the creature and the SuperVillagers (they do not exist yet in openblack). Their
  calls in the under-sea pass already have a slot, constants and a TODO with an address (`sea_pass::k_Creature*`,
  `k_Swimmer*`, `Renderer.cpp`): see [The under-sea pass](#the-under-sea-pass-graphicssea_pass).
- Under-sea pass: add to `psys::mesh_atoms::Instance` the `cutByPlane` bit (`creator->drawCutByPlane` in `Mesh.cpp`,
  the `push_back` of `Collect`), `specular` (DrawData+0xC) and the alpha of DrawData+8, and correct the comment in
  `Mesh.h` (the bit is 4, 0x679F29) and the one in `Mesh.cpp` ("no plane cuts a static mesh": false); then the shield
  dome and the other atoms with `DrawCutByPlane` lose whatever is below y = 0 without touching `Renderer` or the
  shaders. Pending capture: the dome further into the sea (e.g. `PHYSICAL_SHIELD,1800.3120`; the one at 1825.3140
  stays almost entirely on the beach). The +0x50 specular of the boat hull (what its last Draw left,
  **(inferred)** 0).
- Under-sea pass, captures: that the migration to `sea_pass` does not change any pixel **is not demonstrated** with
  captures (the code does give it: hand 0xA0/255, specular 0 of mode 4, same culling, `UnmirrorView` = view·diag(1,
  −1, 1, 1)); two runs of the same exe differed because the sea and the clouds followed the real clock, and some views
  (coast, moon, cut rock, Land 4 net) came out somewhat above that noise. Also missing: the reflection of the boat hull
  (the view shows the ark on land, with no sea in front), a shark before/after pair, and why the first capture of the
  resting physics rock (y = 0.75) came out without a reflection and the others did (an intermittent failure of
  `DrawObjectReflections`; and whether the reflection should be that light, at half light).
- Chimney smoke: the workshops' scaffold count is missing.
- Billboards:
  - port the users of `YawToEye` (influence columns, desire flags, ShowNeeds, ScriptHighlight) and the HelpDude
    ones (basis (R, U, D), HelpDude::Update1 0x5BE302);
  - the oy inherited by the fire's steam and smoke;
  - `MoonModel`: "The moon mesh's matrix, fn_0086AC60 0x86AEC6..0x86AF97: the halo's (fn_005FEDA0), tilted by
    fn_0086AFA0(alpha)". What `fn_005FEDA0` is (a copy of the halo's basis?) is not explained;
  - `VolumeBlendBasis`: "fn_00460710 then runs on c: (inferred) a normalisation, a no-op on that unit vector";
  - PSys sprites: "(inferred) +4 alpha = 1: with 0 they would be 8 / 3, the same states".
- Animated textures:
  - port the users that only have a clock (Gooloo, GoldenShower, the leashes and the creature
    room, HelpDude, the 3D cursor, JCSpecial) and HandGlow / fn_0083F270;
  - the rest of the vials branch of 0x519AD0 (bounce, squashes of the switch 0x519D76);
  - in the chains, UseDynamicLighting (midpoint smoothing is already done; the SurfRevol interpolation too,
    `frame_anim::RotatingUvClock`: GameUpdate 0x6C8BC0 in full);
  - HandEffects as real PSys effects.
- Meshes stuck to the ground:
  - before/after captures of the physical shield, the dispenser disc, the teleport, the ark and the dinosaur
    of Land 4, the lightning explosion mark and the crater;
  - port the creature leash (`fn_008491B0`, `fn_00848600` / `fn_00848830`) and the creature quads (`fn_0081F360`)
    when they have a home in openblack;
  - the temple entrance, the half-built temple, the scaffold and the other users of `GetExtraPos`;
  - the mode 1 `SmokyStuff` of the ground marks.
- Render modes: port the forced ALPHAREF of the burning tree and its other writers; check against the original
  whether the dispenser's stone ring should cover the fading bubble.
- Transparent queue: before and after captures (clouds against models and mists, rain, boat, hand); the reflection
  (not read); `CheckRegionOnScreen` before queuing a model (0x815AB1); the animated meshes of a `Sorted`
  (fn_00813340); port the missing callers (LightSheet, HandGlow fn_0083F100, VillagerName, ValueSpinner, PowerSpin,
  LandscapeVortex, PlayerSymbolSprite, DrawLiquidParticles, fn_006CA930, Gooloo and the two with key 0) with `Submit`.
- `world_triangles::SubmitRaw`: Draw3DWorldTriangle in a CreateMaterial'd `.raw` material (program WorldQuad, `X.raw`
  + `Xa.raw`), used by the influence border and its ripples. `RendererRevolvedSurface.cpp`, `RendererBoat.cpp` and the
  other WorldQuad draws still build their own transient buffers and can move to it.
- `world_triangles`: `RendererRevolvedSurface.cpp` already uses the material's culling; the discs' lighting (`UseLighting`)
  and the specular distribution of fn_0081C780 are still pending
  ([SF_TeleportVortex and ZR_SurfRevol](miracles.md#sf_teleportvortex-and-zr_surfrevol-srcparticlesrulessurfrevol-srcgraphicsrendererrevolvedsurfacecpp)).
  FragMesh's two-sided lighting uses fn_0081C780's per-vertex specular (`world_triangles::Vertex::specular`, Color1);
  the pieces and the SurfRevol discs pass 0.
- Question for the user (transparent queue): **flames behind trees** (only meshes with the 0x200 flag go to the queue,
  SetMesh 0x7F9E48 / AddDrawing 0x815F0B): trees are now drawn immediately and with Z, so the flames of a tree at the
  back are hidden by the foliage of the ones in front, and above they look lighter and without the dark smoke. Was it
  like that in the original?
- Questions for the user (implemented "as in the original" as read from the binary; they depend only on how the game
  looked):
  - **Model lighting at night** (`model_light`): with sky type > 1.5 (double at 0x8C5838) the light is placed 3
    units from the hand, on the camera side (fn_005E5830 0x5E5A7D..0x5E5B64). Does it look like what you remember at
    night?
  - **Under-sea pass** (`sea_pass`): (1) were the shield dome and the PSys mesh effects next to the sea cut flush with
    the water and without a reflection? (0x679F4A → fn_00858BA0; done that way); (2) the hand's reflection has colour
    0x65A0A0A0: grey and semi-transparent?; (3) the original blends the sea over the sky without mirroring it and
    openblack reflects the sky: was it noticeable in the distant water?; (4) morphable objects (houses, fields, ark,
    physical shield) are not reflected (their DrawUnderWater is a `ret`, 0x80BA40; done that way): do you remember it
    the same way?
  - **Projected shadows** (`shadow_list`): (1) the missionaries' boat in Land 1: did its shadow go diagonally (fixed
    sun) and fall on the dock and the sailors? (0x5E11B6 / 0x5E11BE); (2) between 50 and 80 radii, did the shadows of
    thrown objects lighten in steps or smoothly? (0x80769A); (3) was the shadow of a thrown tree soft and as dark as
    that of a rock?; (4) the shadow of the held orb comes out somewhat darker than in the captures (the code's maximum
    8/15 with alpha 255, **(approximate)**); (5) the dispenser's own shadow is the static one (Abode, fn_008721A0),
    which can barely be distinguished in openblack's shots.
