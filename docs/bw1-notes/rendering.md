# World rendering: original versus openblack

How the world is drawn in the original and in openblack: Direct3D states, terrain and small bump, sea and coast, light
table, haze, camera, shadows on the terrain, sky (sun, moon, clouds), rivers, screen fade, text and the map's mist. The
models (materials, light, reflections, object shadows, sprites, smoke) are in
[rendering-objects.md](rendering-objects.md); water as gameplay, in [water.md](water.md); the state of each stage,
in [parity.md](parity.md).

- [The original's Direct3D 7 states](#the-originals-direct3d-7-states)
- [ARGB4444 textures](#argb4444-textures)
- [Terrain detail ("small bump")](#terrain-detail-small-bump)
- [Sea](#sea-skyraw--skyaraw)
- [Coast](#coast)
- [Terrain light table](#terrain-light-table-0xedd90c)
- [Distance haze](#distance-haze-original-fog-detail-levels-36)
- [Haze and land light: the common API](#haze-and-land-light-the-common-api)
- [Camera](#camera)
- [Shadows (three systems in the original)](#shadows-three-systems-in-the-original)
- [Projected shadows (ShadowInfo)](#projected-shadows-shadowinfo)
- [Sky: sun, moon and clouds](#sky-sun-moon-and-clouds-original)
- [Rivers](#rivers)
- [Screen fade and cinema bars](#screen-fade-and-cinema-bars)
- [Text: the original's fonts and the hand message](#text-the-originals-fonts-and-the-hand-message)
- [Particles (PSys)](#particles-psys)
- [Map mist (LH3DMist)](#map-mist-lh3dmist-fn_007fa300)
- [The temple interior](#the-temple-interior)
- [Test hooks](#test-hooks), [Pending](#pending)

Status: everything on this page is **faithful** (original, verified in the executable) except what is marked
**(approximate)** or **(inferred)**, the deviations stated in each section and what is in
[Pending](#pending).

> **Code rules.** State lives in ECS components or Locator services, textures and meshes load through the resource
> caches (`Texture2DLoader` and the other loaders), pure logic (formulas, tables, rasterisers) stays free of global
> state and is tested with fakes in `test/`, and comments describe behaviour in plain English, with no decompiled
> names or addresses (those belong here).

## The original's Direct3D 7 states

`LH3DRender` sets the states with two wrappers, `LH3DRender::SetRenderState` (0x412940) and
`LH3DRender::SetTextureStageState` (0x82B9C0), and also with direct calls into the `IDirect3DDevice7` vtable: `+0x50`
is SetRenderState and `+0x94` is SetTextureStageState.

- **Filtering**: in the default configuration (`0x82CA40`, inside `fn_0082C8F0`, next to `OpenD3D`),
  MINFILTER and MAGFILTER = 2 (**linear**) in stages 0 and 1. `SetupThing` (0x4133A2… and `DrawBevBox` 0x413C20),
  in the 2D frontend, switches to 1 (point) while it draws and restores it afterwards.
- **There are no mipmaps**: `D3DTSS_MIPFILTER` is never set (D3D7's default value is NONE) and the DDS textures
  in `AllMeshes.g3d` only have one level.
- **There is no antialiasing**: neither `D3DRENDERSTATE_ANTIALIAS` nor `EDGEANTIALIAS` appear, nor
  `MAXANISOTROPY` either.
- Dither enabled (`DITHERENABLE`), which was for the 16-bit modes.
- **No cubic filter** (reviewed at the user's request): every MAGFILTER write (cache 0xEC8230/0xEC8630;
  `fn_0082C8F0` 0x82CADB, `fn_00836E20` 0x836FA4, `HelpDude::Feel` 0x5B9CAF, `SetupThing::DrawTab`/`DrawBevBox`) uses
  1 (point) or 2 (linear); never 3/4 (D3DTFG_FLATCUBIC / GAUSSIANCUBIC). There is no filtering option in the registry.
  The installation's `d3dim.dll` / `d3dim700.dll` are UCyborg's "resolution limit remover" (2016), they do not touch
  filtering. A softer look in the original would come from the driver or from the Windows compatibility layer.
- Cut-outs: the `TexturedChroma` materials (trees, fences) use threshold 0x96 (0.588); the pine has
  `MSH_T_CONIFER` 577 and `MSH_T_PINE` 590. Its skin (BGRA4) has ~94.5 % of texels with alpha 0 or 15 and the rest
  intermediate.
- The default configuration also turns on `D3DRENDERSTATE_SPECULARENABLE` (`fn_0082C8F0` 0x82CC54): the vertex
  specular is added to texture × diffuse.
- The mode table 0xC38728 is installed by `LH3DRender::Open` (0x82B4EA).

openblack already matches by default: bilinear, no mips and no MSAA.

## ARGB4444 textures

**Faithful**. Code: `src/Graphics/Argb4444.h` (`graphics::argb4444`, stateless
and, on purpose, without a shader twin); tests `test_argb4444` (they emulate 0x8374F0..0x837533 and 0x83767E..0x837687).

- **The cut.** `LH3DTexture` stores in ARGB4444 the textures created with the alpha flag 0x40 (`Create` 0x8379E0
  with flags 0x41). Format 0 has the masks 0xF000/0xF00/0xF0/0xF (0x85DCA1..0x85DCC2). `fn_00837400` keeps the high
  nibble of each byte, without rounding:
  - B = byte[+2] >> 4 (0x8374F4), G = byte[+1] & 0xF0 (0x837502), R = (byte[+0] & 0xF0) << 4 (0x837512);
  - the alpha of `xa.raw` = byte & 0xF0 (0x837681).
  - D3D7 expands the nibble when sampling, n·17 (inferred: the driver does it). The original **filters nibbles that are
    already cut**, so the cut goes at load time and not in a shader.
  - API: `Quantize(v) = v >> 4`, `Expand(n) = n·17`, `Cut(v) = Expand(Quantize(v)) = (v & 0xF0) | (v >> 4)`;
    `Pack`/`Unpack` of the 16-bit texel; `PackRaw(rgb, alpha)` for the `x.raw` + `xa.raw` pair.
- **Without the flag**, the 4444 branch does not run (`cmp [esp+0x834],0 / je`, 0x8374CB/0x8374D2). It goes to 565
  (0x8376E3..0x83771E) or to 555 (0x837765..0x83779F) depending on [0xEDD46C] (0 on the hardware the game targets:
  555, see `Graphics/Rgb16.h`), with 5 bits per channel: texel = ((R & 0xF8) << 7) | ((G & 0xF8) << 2) | (B >> 3),
  without rounding. **Done**: `Texture2DLoader` cuts each byte with `rgb16::Cut5`
  (= `Unpack555(Pack555(...))`, the bit-repetition expansion D3D does when sampling, inferred) in the
  `k_Rgb555Stems` textures (`Loaders.cpp`) that are exactly 0x30000 bytes (the same guard as `fn_00837300`).
  - [0xEDD46C] is set to 1 only by `fn_0085D7F0` (0x85D929..0x85D930), when the 16-bit format the engine picks has the
    green mask 0x7E0 (565), i.e. the device has no X1R5G5B5; `fn_00837400` tests it at `cmp [0xEDD46C]` 0x8376B9.
  - The 565 branch (0x8376E3..0x83771E) packs ((R & 0xF8) << 8) | ((G & 0xF8) << 3) | (B >> 3) (`shl edx, 5` 0x8376FA,
    `lea edi, [edx + eax·8]` 0x83770C): green is G & 0xF8, so the lowest bit of the 6-bit field is always 0. Only for
    .raw textures, never for the video.
  - Who goes that way: of the 69 calls to `Create` 0x8379E0, the ones that load a file without 0x40 are `Sun.raw`
    (flags 1, 0x81E844..0x81E851) and the saved-game images, `screenshots_lores_%i_map_0.raw` and
    `screenshots_hires_%i_map_0.raw` (flags 1, `fn_00784070` 0x78408F and `fn_00784640` 0x78466A, names from
    `fn_00784430` / `fn_007843F0`), which openblack does not load. The others are 0x41 (the list below), 0x44 / 0xC4 /
    0x104 (in memory, type 4), 0x48, 4 (mesh skins), 2 (`SetPackedTexture`) or the video; the one at 0x822874 is in
    `fn_008227A0`, a command-line mode (the terrain .cmp converter), not in the game.
  - openblack: only `sun`.
- **The 0x44 ones do carry the flag** (0x44 & 0x40; `Create` stores the whole flags in [tex+0x10], 0x837A76): they are
  4444 surfaces, but of type 4 (flags & 0x3F, tables 0x837CD4 / 0x838E84 → 0x838C2E), in-memory textures that do not
  load their file through `fn_00837400`. Such as `ChallengeScroll.raw` (0x44, 0x781BDC and 0x79D59F; strings 0xC25048
  and 0xC2A5D0) and `human_shadow` (below). `ChallengeScroll` is not in the list because the game fills it in itself,
  not because it lacks the flag; how it fills it in has not been read (see [Pending](#pending)).
- **Guards.** The colour has to be exactly 0x30000 bytes (`fn_00837300`, `cmp ecx,0x30000 / sete`, 0x837318); if
  not, it goes through DDS. That is why `S_IceEnvMapGrey.raw`, of 194823 bytes, is not cut, nor is its
  `S_IceEnvMapGreya.raw` (0x10000 bytes): without a valid colour `fn_00837400` never gets to read the alpha. The alpha
  is read with 0x10000 bytes (0x837600): `LHLoadData` reads min(length, 0x10000) (0x7BCEC0..0x7BCECD) and does not fail
  if it is shorter; the rest of the buffer keeps the colour stream (the leftover rule below, from its length onwards),
  and a longer one is truncated without error.
- **Without `a.raw`.** The alpha's name is the colour's without its last 4 characters plus `"a.raw"` (0xC384AC,
  0x8375B4..0x8375C1). If `LHLoadData` fails, it only calls `Report3D` (0x837616) and carries on at 0x83761E with the
  buffer that still holds the colour. The alpha of pixel i then comes from byte i of the colour stream (R0, G0, B0,
  R1…) & 0xF0.
  - In practice it does not happen: all the 0x41 textures come with their `a.raw` (perhaps the saved-game images,
    inferred). `PackRaw` reproduces it.
- **Who carries it.** There are only two calls to `fn_00837400`: 0x838087 in `fn_00837DF0` and 0x838D41 in
  `fn_00838AF0`. Both pass `[tex+0x10] & 0x40` (0x838079 / 0x838D37).
  - Of the 69 calls to `Create`, 32 push an immediate 0x41 and 22 use 0x44.
  - One more computes it: `fn_008227A0` (not `fn_00822560`) 0x822855..0x822874 sets 1, plus 0x40 if the `a.raw` exists (the terrain
    .cmp converter).
- **The `k_AlphaFlagStems` list**, each name with its address:
  - fixed: `Front_end_buttons`, `mousehelp`, `forcefield`, `pin`, `rainbow`, `PlayersSymbols`, `ChooseSymbol`,
    `OriginalChooseSymbol`, `sky` (0x5E5432), `gatheringtext`, `Data\C_Ape_Hair`, `icons`, `PictureTexture`,
    `smallbump`, `Weather`, `atmos`, `snow`, `Data\blobs`, `leash`;
  - particles from `fn_0080BBD0`: `p4t`, `p4`, `smoke`, `s_fire`, `cool_effect`, `misc0`, `burn`;
  - environment maps (table 0xC37EAC): `envmap`, `envmap_glass_fx`, `envmap_glass_fx_inv`, `envmap_glass_fx2_inv`;
  - through `fn_0057DBE0` → `fn_0057DB10` (0x57DB78/0x57DB7B, flags 0x41):
    - GlobalTextures: `S_SpriteSheet1/2/3`, `S_Static`, `S_Hand_Flow`, `S_Fire` and the tables 0xBEF478, 0xBEF484 and
      0xBEF4A0;
    - the `TextureFileName` entries of the spell files, through ParticleSpriteCreator (`fn_006AA030` 0x6AA047) and
      ParticleChainCreator (`fn_006AA800` 0x6AA817): `S_Beam`, `S_lightning`, `S_Spangle_A` (inferred),
      `S_Teleport_Vortex_Texture(01)`, `S_Volcano_Fire`, `S_Volcano_Rock`;
    - ZR_SurfRevol (0x6863E4);
    - LandscapeVortex `fn_005FEA70`, tables 0xBF3F5C / 0xBF3F68: `S_VortexBaseMultiRing`, `S_Volcano_Base`,
      `S_VortexBaseAlphacopy`, `S_Volcano_Base_Alpha`.
  - Load site of each fixed stem: `Front_end_buttons` 0x4120CA, `mousehelp` 0x447584 (0x44757F, [0xC5A314]),
    `forcefield` 0x4568B9, `pin` 0x53C658, `rainbow` 0x5C386B, `PlayersSymbols` 0x5DE410, `ChooseSymbol` 0x5DE425,
    `OriginalChooseSymbol` 0x5DE43D, `sky` 0x5E5432 / 0x5E5461, `gatheringtext` 0x5F8A92 / 0x72E00D / 0x83109F
    (`GatheringText::SetupGameFonts` 0x831096), `Data\C_Ape_Hair` 0x6186C7, `icons` 0x787358, `PictureTexture`
    0x7900FD, `smallbump` `fn_00804830` 0x804844, `Weather` 0x81E831, `atmos` 0x835C29 ([0xEDC370]), `snow` 0x835C7C,
    `Data\blobs` 0x845E08, `leash` 0x8485F2.
  - `fn_0080BBD0` loads, in order: `p4t` 0x80BC1B, `p4` 0x80BC44, `smoke` 0x80BC6D ([0xEA1A98]), `s_fire` 0x80BC99,
    `cool_effect` 0x80BCD4, `misc0` 0x80BD0E ([0xEA1A90]), `burn` 0x80BD37 ([0xEA1A9C]; path string 0xC37F90,
    `Create(path, 0x41, −1, 0)` 0x80BD2F..0x80BD3F).
  - `envmap`: `fn_0080B380` 0x80B393 (table 0xC37EAC).
  - LandscapeVortex `fn_005FEA70` creates its textures at 0x5FEA92 / 0x5FEB1B; the table 0xBF3F5C is filled by
    `fn_005FD4C0` and 0xBF3F68 by `fn_005FD4D0`.
  - `HasAlphaFlag(stem)` accepts the colour's name and the alpha's (the same plus `a`), case-insensitively.
- **`human_shadow.raw`** is not in the list. `fn_0081FAA0` creates its own 0x44 texture (0x81FC58) and reads 0x400 bytes
  (0x81FC86). It writes texel = (v & 0xF0) << 8 (`and cl,0xF0 / mov bh,cl`, 0x81FCDD..0x81FCEC): alpha `Quantize(v)`
  and RGB 0. It is the same cut, with its own entry (`k_HumanShadowStem`).
  - It reads the 0x400 bytes at 0x81FC86 / 0x81FC91 and locks the new texture through `fn_00838AF0` (0x81FCA4).
- **L3D skins**: they are copied as they are (`rep movsd` 0x837B41), because they already come in 4444. With mesh flag
  0x10000 (`test edi,edx` 0x80656D) they would be RGB555 (see [Pending](#pending)).

**openblack.**
- `Texture2DLoader` (FromDiskTag, `Resources/Loaders.cpp`) cuts with `Cut` at load time:
  - every `.raw` whose name satisfies `HasAlphaFlag` and is 0x30000 (colour) or, if it is the alpha (the colour plus
    `a`), is 0x10000 and its sibling colour exists and is 0x30000 (approximate: the original only looks at the colour;
    an alpha of another size is not cut here);
  - and `human_shadow`.
- openblack stores `x.raw` and `xa.raw` as two textures. Each one is cut separately, and bgfx's linear filter
  already works on the 16 levels, like D3D.
- `PackRaw` is used by the small bump (`LandIsland.cpp`), which joins colour and alpha in one texture. A shorter alpha
  takes the tail of the colour stream and a longer one is truncated, like `LHLoadData`; a colour that is not 0x30000
  is rejected (approximate: the original would go through DDS).
- The copies that only expand nibbles that are already done use `Expand` (`CoastAlpha.cpp`, `GameFont.cpp`) or
  `Unpack` (`BlockTexture.cpp`).
- Shaders:
  - `fs_blob.sc` no longer quantises. Before, it did floor(v/17) after filtering: one level less in 120 of the 256
    values and the gradient in steps. Now the blob comes out somewhat darker and without steps.
  - `fs_land_alpha.sc` keeps the rounding to 16 levels (floor(a·15 + 0.5)/15) even though the footprint is already
    BGRA4: the texture is created with a linear filter (`L3DMesh.cpp`) and the rounding absorbs the hardware's error
    when sampling the texel centre; if the value is already exact, nothing changes.
  - `fs_physics_shadow_resolve.sc` (`covered/15`, 0xFA95C4) was already exact.
- Whoever uses these textures already receives the 16 levels: also the glow of the night lights (`NightLights.cpp`
  loads `S_Firea` and `smokea`), the water rings and the boats' puffs.

## Terrain detail ("small bump")

Full report from the disassembly; W120 addresses:
- `fn_00804830` loads `.\data\Textures\smallbump.raw` with the alpha flag (0x41). On first use `fn_00837400` packs it
  into ARGB4444 and puts `smallbumpa.raw` into the alpha nibble (`"a.raw"`, 0x8375C1). Material mode 0xE.
- Mode 0xE = `fn_0082DD90` (mode table 0xC38728, 8-byte {fn, flag} entries): `ALPHABLENDENABLE`,
  `SRCALPHA/INVSRCALPHA`, stage 0 `TEXTURE*DIFFUSE` in colour and alpha.
- `fn_007A1800` (block drawing): second pass per block, unlit, UV × 12 per block (one repeat every
  13.33 units). It is not done on blocks with an overlay material.
- **Vertex diffuse of that pass** (x87 `fn_00874AA0` 0x8758E7–0x87598B, SSE `fn_007A1800` 0x7A2FD0–0x7A3136; both
  paths the same), with d the signed distance to the fade line and e = 20:
  - d ≥ e → `specular & 0xFF000000 | 0xFFFFFF`: white with alpha 255, or **alpha 0 if the vertex altitude is ≤ 1**
    (the specular alpha, see [Coast](#coast));
  - −e < d < e → `fistp(255 − (e − d)·255/40) << 24 | 0xFFFFFF` if the specular alpha is not 0, otherwise 0 (black,
    alpha 0);
  - d ≤ −e → 0.
  Masks of the SSE path: 0xFC01B0 = 0xFF000000, 0xFC01C0 = 0x00FFFFFF, compared with 0 (`[esp+0x460]`, 0x7A1C3B).
  That way the detail **fades (Gouraud) towards each vertex with altitude ≤ 1**: it does not end in a straight edge
  where the shallow water touches the open-sea cells. Skipping the triangles with all 3 vertices at 0 (0x7A31A0)
  changes nothing.
- Fade (`fn_007FEE60`; constants from `fn_007FE7B0`: 50 = 0x42480000, ramp 40 = 0x42200000, e = 20 = 0x41A00000):
  line 50 units in front of the camera on the plane y = min(cam.y, 0.67·165); full up to 20 units before, nothing 20
  after. It depends on the camera's height and angle (not a fixed 200): **when zooming the camera in or out the
  area with detail grows or shrinks, also over the shore, just as in the original**.
- Result: `col = mix(col_lit, smallbump.rgb · c, smallbumpa · a)` with (c, a) the diffuse above → **light** specks, also
  at night. openblack did `col *= 1 − smallbumpa` (dark blotches) with only the alpha and UV × 10.
- Until the shore audit (2026-10-01) openblack gave alpha = fade on every triangle with any vertex of altitude > 1, also
  on its shore vertices: a semi-transparent greenish film over the shallow water near the camera, cut along the
  triangle edges (straight and diagonal) and changing with the zoom. Now `vs_terrain` computes (c, a) per vertex like
  the original (`v_smallBumpFade`, vec2).
- `SetUseSmallBump` (0x87FD30) only changes `[0xC37210]` (registry "UseSmallBump", detail levels).
- `S_TileLandscape.raw` is not for the terrain: it is entry 6 of the global textures table 0xBEF484, used by the
  citadel heart (`fn_00465C70`) **(inferred)**. `L_Smallbump_01.raw` is not used.
- In this installation `smallbump.raw` (2017), `smallbumpa.raw`, `Sky.raw` and `S_TileLandscape*.raw` (2021) come from
  a texture pack, they are not from 2001. The user remembers (2026-10-01) that the mottling over the shallow water may
  come from that `smallbump.raw`: with the 2001 original it would look different; the drawing rule is the same.
- openblack: `LandIsland::CreateSmallBumpTexture` (RGBA with 4-bit quantisation), per-vertex fade in `vs_terrain`
  (`u_smallBumpLine`, `u_skyAndBump.w`), blend in `fs_terrain`.

## Sea (`sky.raw` + `skya.raw`)

From the disassembly (W120):
- `GLandscape::Open` 0x5E5432 loads `.\Data\Textures\sky.raw` (alpha flag; `skya.raw` is the alpha, 4 bits) with
  material mode 5 (= `fn_0082DD90`, the same as 0xE: `SRCALPHA/INVSRCALPHA`, `TEXTURE*DIFFUSE`).
- `GLandscape::Draw`: sky (with the reflected moon) → reflected land (`LandRef`, heights × −1, light table × 0.5,
  `fn_007FF4F0`) → parts under the water (hand, objects, fish…) → hand glow (0x5E4D89) → sea
  (`fn_00879930`) → land. The sea is skipped only if `[0xECA670]` (wireframe, debug) or `[0xECA664]` ≠ 0; the latter is
  set by `PSysLightMaps` (0x6CA57E) to `clamp(level, 0, 1)·190` and the level only goes up/down when a cutscene ends
  (`CameraModePath::Cleanup`): **in normal play the sea is drawn**.
- Strips of 2 px rows on screen, `ZFUNC ALWAYS`, without writing Z. UV = (world + offset) / P,
  P = 2000 − 1800·WaterTiling (560 at the default detail, 200 at the maximum). Offset with the ambient wind
  (−1/330 per ms, applied twice per frame), which is 0 in a game.
  Ripple: each row moves 0.9·sin(i·π/8) along the camera's horizontal front,
  i = (frame + 2·(row+1)) & 15, full beyond 70 depth, nothing below 30.
- Vertex colour = entry 255 of the terrain light table (full light for the time of day); alpha = 255 up to
  depth 7000, dropping linearly to 80 at 14000 (0xC39908). A single colour for the whole sea: **the sea receives no
  cloud shadows** (only the reflected land seen through it).
- WaterTiling is the global 0xC38228 (read by `fn_00879930`).
- The ripple direction is the camera's forward x and z ([0xEA1DD4] / [0xEA1DDC]) scaled to 0.9, unless both are 0.
- The sea material's mode 5 is set by `GLandscape::Open` at 0x5E546C.
- The sea's CPU routines are `fn_008792E0`, `fn_00879500`, `fn_00879930` and `fn_0087A090` (the role of `fn_008792E0`
  is in [Pending](#pending)).
- Texture offsets: [0xFA9370] / [0xFA9374] for detail levels 1..6, [0xFA9390] / [0xFA9394] for level 0.
- Drift constants: −1/330 at 0x9A3C24 (used at 0x879963), −1/330000 at 0x9A3C2C (0x87A130).
- The ambient wind is `g_ambient_wind_direction` (0xEA9E70 / 0xEA9E78) = FastNormalize(int8 `LH3DAtmos::ambient[4]` /
  8, 0, ambient[5] / 8), set every turn by `GGame::ProcessTurn` 0x54E5DC. ambient[4..5] are only written by
  `InitStaticsValues` / `fn_00835AD0` (0), `GGame::Load` and the Internet weather: that is why it is 0 in a game.

In openblack (`fs_water`/`vs_water`, `Graphics/RendererSea.cpp`, `Graphics/SeaRows`): **done like the original**.
- **Sea area** (`fn_00879500`, `sea::ComputeScreenRange`): 30000×30000 quad at y = 0 centred on (2560, 2560)
  (corners −12440 / 17560), clipped with the near plane and the four sides (no far plane). `top` = minimum and
  `bot` = maximum screen y, with their 1/z; `top` is raised to 0 and `bot` lowered to height − 1 without correcting
  its 1/z. Oddity copied: the first vertex can only set `bot` (`if y > bot … else if y < top`). The clipping is the
  original's: vertices 0..3 = (−12440, −12440), (17560, −12440), (17560, 17560), (−12440, 17560) and triangles
  (0, 2, 1), (0, 3, 2) (0x879537); each one goes through the recursive clipper `fn_0081A760` (x87) / `fn_007A3A50`
  (SSE, P4), one plane per bit from 0x20 (near) to 0x02, two new vertices per cut at the end of the table and, if two
  triangles remain, the first one clipped by a recursive call; the new vertices carry only the codes of the following
  planes and their screen y is clamped to 0..height−1 (`g_MaxScreen`, 0x81E130). `bot`/`top` are read in the order of
  that list. Checked with a Unicorn emulation of `fn_00879500`: both paths
  give the same list and the same y (±0.05 px of rounding) in 400 random cameras; `test_sea_rows` carries two cases
  from the emulation. In 30000 cameras over the island the "only `bot`" oddity never moved `top` by more than 1 px.
  **Bug in the original, copied**: to decide whether clipping is needed it adds up the codes of 0, 1, 2 and of entry
  **4** of the table (`[0xE3B5F0]`, instead of 3 at 0xE3B5EC), which is a leftover from the last draw by LH3DP3. If
  that entry and codes 0..2 are 0 with vertex 3 outside, it draws without clipping with the camera x', y', z' of
  vertex 3 (a garbage `top`). It requires seeing corners 0, 1 and 2 at the same time: it does not happen in any of
  200000 cameras over the island's disc (radius 5120, height 3..4000), so entry 4 = 0 is assumed. Near plane:
  `[0xE839E0]`, which `GCamera::Update` 0x4424AF sets every frame with `LandFeature::GetNearClipping` 0x5E2F30 =
  0.3 + 0.16·h (h = camera − `GetAltitude`; 0.3 if h ≤ 0, 3.5 if h > 20; 0.1 with `SET_GRAPHICS_CLIPPING`
  [0xD1A2F8]): it is the `cameraNearClip` that openblack already recalculates in `Game` and the one the sea uses.
  - Clip codes of LH3DP3 (`fn_008797F0` 0x87985A..0x8798C3): 0x20 in front of the near plane, 0x10 / 0x08 right /
    left, 0x04 / 0x02 top / bottom of the 90° frustum of the pre-scaled camera matrix (|x'| ≤ z, |y'| ≤ z); no far
    plane.
  - The sea is clipped with `g_NoBackfaceCull` = 1 (0x8795AC): no triangle is culled.
  - New edge vertex (`fn_0081DD90` / SSE `fn_007A1480`): t = d(in) / (d(in) − d(out)), p = in·(1 − t) + out·t; the
    distance to the plane comes from the jump table 0x81E17C; its clip code only tests the planes after the one being
    cut (jump table 0x81E1B4, 0x81E063), never the near plane again; its screen position is clamped at 0x81E0F1 to
    `g_MaxScreen` 0xC2AB00 (width − 1, height − 1).
  - Triangles left after each cut in `fn_0081A760`: (c, c−a, c−b) 0x81A7C1; (b−a, b, b−c) 0x81A835; (b−a, b, c) then
    (b−a, c, c−a) 0x81A856; (a, a−b, a−c) 0x81A8C8; (a, a−b, c) then (a−b, c−b, c) 0x81A8E9; (a−c, a, b) then (a−c, b,
    b−c) 0x81A928; nothing to clip 0x81A971.
  - `fn_00879500` returns no sea when the whole quad is outside one plane (0x8795CA, 0x879649) or when bottom < 0 or
    top > height − 1 (0x879768..0x8797E2).
  - top starts at the screen height and bottom at −1 (0x879651..0x879762); screen y = (1 − y'/z)·height/2
    (`fn_008797F0` 0x8798E6). The two 1/z are stored as rhw = near/z at [0xFA9384] / [0xFA9388] and divided by near in
    `fn_00879930`.
- **Rows** (`fn_00879930`): rows of vertices r = 0..n at y = ftol(top) + 2r, n = (ftol(bot) − ftol(top) + 2)/2;
  1/z of the row = 1/z(top) + r·(1/z(bot) − 1/z(top))/(n − 1). `vs_water` draws a full-screen quad and `fs_water`
  redoes each row per pixel: point on the plane under the row, ripple on the even rows (full if 1/z < 1/70,
  (1/z − 30)·0.025 if < 1/30), the odd rows repeat the UV of the previous one (2 identical px and 2 interpolated px,
  without perspective correction), alpha per row rounded and **row 0 with alpha 0x20** if ftol(top) ≥ 1.
  It is counted from `top` and with `u_viewRect`, so it does not depend on the API.
  - The rows are set up at 0x879AD7..0x879B3E; the alpha 0x20 of row 0 is decided by "y of row 0 > 0.5" (0x879D5A).
- Outside the rows and below the horizon (between the far edge of the quad and the horizon, or with no sea on screen)
  what is under the sea is seen unblended: the **strip of mirrored sky** above the edge of the sea (about 25 px at 864
  high with the camera at a height of 300). Above the horizon, the sky of the main view.
- **Frame** `[0xFA938C]`: goes up by 1 (& 15) per drawn frame only if the sea is on screen and the game is running
  (`g_game_time_inc` ≠ 0): **while paused the sea lines do not move** (identical captures while paused). At modern fps
  it shimmers faster than at ~30 fps, as the original would at that speed.
- **Drift with the wind** (`sea::Drift`): off += wind·ms·(−1/330), twice (0x879963 always, 0x879A69 if there is sea on
  screen), wrapped to P; at level 0, off0 += wind·ms·(−1/330000) wrapped to (−1, 1). The ambient wind is 0
  (`sea::k_AmbientWind`), so it does not move: the code is there for the day it is not 0.
- **Detail level 0** (`fn_0087A090`, WaterTiling = 0): world quad of ±70000, UV (x + 70000)/2800 and
  (70000 − z)/2800 (V grows towards −z) plus its drift, vertex alpha 255, without ripple or fade.
- **Texture in 4 bits** (`fn_00837400`): at load time, `sky.raw` and `skya.raw` keep `v >> 4` (n/15, like ARGB4444 in
  D3D). The colour of `sky.raw` has a deviation of 5-8 out of 255, so 4-6 levels per channel remain and close up flat
  blotches are seen.
- **What is under the sea** is the reflection target, now **the size of the main view** (it is recreated when the size
  changes; before it was 1024²) and painted **in order** (sequential view, like `GLandscape::Draw` 0x5E48AE–0x5E4E6B):
  mirrored sky, reflected moon, reflected land (`fn_007FF4F0`: half light, **no small bump** because
  `[0xC37210]` = 0, no dynamic shadows; the cloud ones yes; **without writing Z**: `GLandscape::Draw` 0x5E48C5–0x5E4900
  sets ZWRITEENABLE (14) to 0 before `fn_007FF4F0` and to 1 afterwards, so it does not cover anything that comes after
  and, between blocks, the last one drawn wins: the farthest, see [Coast](#coast)), hand and objects under the water,
  schools of fish and the **hand glow**. openblack drew it writing Z (its flat cells at y = 0, even if they were
  transparent, and the mirrored hills covered the reflections that came after); since 2026-10-01 it no longer does. No
  models or sprites. `fs_water` blends `light·sky` over it with alpha `skya·row alpha`, the same as
  SRCALPHA/INVSRCALPHA over the frame.
- Sea with ZFUNC ALWAYS, without writing Z and without culling; the main view is sequential and the land that comes
  afterwards covers it.
- Hook `OPENBLACK_SEA_TRACE=1` (every 500 frames: first row, n, 1/z, step, smooth row, frame, drift).
  Test: `test_sea_rows`.

## Coast

**Faithful** (done). The normal and SSE paths do the same. The earlier attempt (flattening and skipping
cells, nothing more) gave a "stepped sand floor" because it lacked the piece that gives the shore its shape: the
**alpha of the block texture**.
- **Coastal alpha per texel** (`fn_008732C0`, SSE `fn_007AB4B0`, 16×16 texels per cell, 256 per block): altitude
  weighted with cones `h = Σ w_k·alt_k` (`fn_00871560`: `w_k = max(0, 14 − d_k)` to the 4 corners, truncated to Σ = 255
  and +1 to the largest), `h < 0x100` → 0, `h ≥ 0x400` → 15, in between `e = h + 4·(int8)noise` (LND noise,
  `[x·256 + z]`, the same in all blocks) and `idx = 15 + trunc((15e − 14250)/438)` on the table 0xC39814 =
  0xC2AAC0 (16 dwords already shifted 12 bits: nibbles 0,0,0,1,2,4,6,8,9,10,10,11,12,13,14,15). Transparent below an
  altitude of ~1.2–2, opaque towards ~2.9–3.7; the noise draws the shoreline.
- **Open-sea cells** (bit 0x02 of the `flags` byte, `word(cell+6) & 0x200`): their triangles are not emitted
  (0x875DDC, 0x876A8A **and** SSE 0x7A9B14, 0x7AAB14) and the builder zeroes their texels (0x8739F8). 3297 cells in
  Land1. Bit 0x02 is **not** part of the sound code: bits 2..5 of the byte are the ATMOS type.
- **Mesh flattening**: `y = alt ≤ 3 ? 0 : alt·0.67` at each vertex (0x874B95, SSE 0x7A1EE7, `cmpleps` against 3.0
  at 0xFC01F0). The game (`GetAltitude` 0x803121) only flattens next to a base corner ≤ 4; in Land1 there is no cell
  where the two rules give different heights (base > 4 with a corner ≤ 3: 0 of 25600).
- **Specular**: `cell dword | 0xFF000000`; its alpha is 0 if UseSmallBump and alt ≤ 1 (0x874BA9, SSE 0x7A1F16).
  The small bump pass copies it to its diffuse (alpha 0 at those vertices, see
  [Terrain detail](#terrain-detail-small-bump)) and skips the triangles with all 3 vertices like that (SSE
  0x7A31A0). The small bump is **not** modulated by the coastal alpha, but it fades to 0 towards the vertices with
  altitude ≤ 1: over the shallow water its specks only appear next to higher vertices, without an edge.
- **No hard edge next to the open-sea cells**: on the 6 lands, the 0x02 cells have all 4 corners at altitude 0 and
  the coastal alpha of the neighbouring drawn cells is 0 in every texel of the shared edge
  (Land1 871 edges, Land2 1722, Land3 900, Land4 1283, Land5 1808, LandT 556,
  all with maximum nibble 0). A straight edge in openblack cannot come from the coastal alpha.
- **Dynamic shadows** (`fn_00878350`): vertex colour 0 where the altitude byte ≤ 1 → they fade towards the water
  (interpolated), without a hard cut.
- The land is drawn in mode 14 (SRCALPHA/INVSRCALPHA) over the already painted sea and **writes Z even if the alpha is
  0** (`fn_0082DD90` sets ALPHATESTENABLE = 0 at 0x82DE2A and does not touch ZWRITEENABLE). Consequence, in the original
  and in openblack: whatever is **below y = 0** and is drawn afterwards with its normal Draw (a tree or other physical
  object that sinks before being deleted at −4R, see [water.md](water.md#sinking-drowning-and-being-deleted)) is covered in
  all the drawn cells and seen whole, as if floating, over the 0x02 cells, which do not write Z: it comes out **cut into
  rectangles** along the edge of the open-sea cells.
- **Block order** (land and reflected land): the list from `LH3DIsland::PreDraw` (0x7FF45F–0x7FF4DD),
  ascending by `block+0x9BC` = distance from the camera to the centre (x + 80, 0, z + 80) of the block with LandRef
  (`fn_00877210` 0x87722C–0x877296, 0x877C8A–0x877CCD): **the closest first**; `fn_007FF610` and `fn_007FF4F0`
  walk the same list (+0x9B8). openblack sorts them the same way in `Renderer::DrawPass`. Without Z on the reflected
  land, that order decides which mirrored hill covers which.
- openblack: `3D/CoastAlpha` (port of the reference script, identical texel by texel in Land1),
  today the alpha channel of `LandIsland`'s RGBA8 `BlockTexture` (see below; linear filter, rows along +z; it is
  rebuilt in `RebuildAltitudes`);
  `LandBlock` flattens, collapses the 0x02 cells to a point (Bullet's physics shape keeps them) and passes the "shore
  fade" (alt > 1) per vertex; `fs_terrain`: `a = min(costa, LandAlpha)` (the rivers still use `min`), without
  `discard`, the two passes as a premultiplied colour (blend ONE/INV_SRC_ALPHA: `(land+spec)·a·(1−b) +
  (bump+spec)·b`) and the shadows also multiply what is seen behind. `GetDrawnHeightAt` = height of the drawn mesh.
  Differences: a single texture for the island (the bilinear filter crosses the block edges; the original had one
  texture per block).
- **Cone weights, tie-break**: the +1 goes to the largest and, among equals, to the **last** (w3 before w0): the table
  that `fn_00871560` builds at 0xE3A3E0, read from a Unicorn run, gives e.g. at texel (8, 8) 63, 63, 63, 66.
  31 of the 256 texels (the lines i = 8 and j = 8) change with respect to the previous rule (the coastal alpha barely).
- **Colour of the block texture (done)**: `fn_00873790` per cell and `fn_008732C0` per texel, with the block's
  `(x·256 + z)` index for the material, the noise and the bump (`[0xFA7698]`, the LND's bump):
  - `h < 0x100` → texel 0. Otherwise, entry `mat = min((h >> 8) + noise, 255)` of the country's table (`[0xFA75C0 +
    4·country]`, country = low nibble of byte 6 of the cell): {index 0, index 1, coef}. 5-bit colour of the material
    textures (`[0xFA74F8 + 4·i]`, B5G5R5 after the type u16): `c0·coef + c1·(256 − coef)` (the **first** one carries
    the coefficient; only c0 if they are equal), × bump and to 4 bits: R = (Σ(c & 0x7C00)·bump) >> 18, G >> 17, B >> 16,
    each one clamped to 15 (0x873438..0x8734EA); alpha the coastal nibble.
  - If the cell's 4 corners are not of the same country, it is built once per corner country and `fn_00871850`
    blends per 4-bit channel (alpha included) with the cone weights: `floor(Σ channel_k·w_k / 255)`.
  - Checked: the Unicorn emulation of `fn_00873790` matches the
    reference in the 163840 texels of 40 rows of Land1 cells along the x87 path; the SSE one (P4, `fn_007AB4B0` with
    `pmulhuw`) lowers the blue nibble (sometimes the green) by 1 in ~1.8 % of the texels. The x87 one is followed. The
    texture openblack generates (`OPENBLACK_DUMP_BLOCK_TEXTURE`) is identical to the reference in 524288 compared
    texels.
  - `h < 0x100` gives texel 0, black and transparent (0x87339C). The country's table entry is 12 bytes: {index 0,
    index 1, coefficient} at country + 4 + 12·mat (0x8733BF).
  - One build when the 4 corners share the country, else one per corner country (0x8737EA..0x873821); `fn_00871850`
    blends at 0x8718EC..0x871A36 with the 0x80808081 multiply per channel mask (the result is the same for equal
    countries: the weights add up to 255).
  - The LND noise map is [0xFA7694] (the bump map [0xFA7698]); both 256×256, index x·256 + z, the same for every
    block.
  - openblack: `3D/BlockTexture` (`CountryTexel`, `BlendCorners`, `BuildIslandBlockTexture`); `LandIsland` keeps the
    material textures and the bump in CPU and uploads an RGBA8 `BlockTexture` (nibble × 17, colour and coastal alpha)
    that replaces the R8 `CoastAlpha`; `fs_terrain` takes the colour (it already includes the bump) and the alpha from
    it; on top go the footprints, the static shadows and the light as before.
  - This settles the question in tooling.md: the material entry is `min((h >> 8) + noise, 255)`, neither the
    editor's `min(alt + noise/4)` nor openblack's former `(alt + noise) % 256`.

## Terrain light table (0xEDD90C)

- `fn_00869850`, every frame from `GLandAlignement::DrawSky`: 256 colours indexed by the cell's brightness, from
  `Data\WeatherSystem\palette.raw` (32×32 RGBA, one for all the islands). Rows 0..2: colour of the good/neutral/evil
  land according to the time (column = (2 − Time2SkyType)·15: 0 midnight, 15 sunset, 30 noon); rows 3..7 by alignment
  (column = X·15, X = 1 − alignment: 0 good, 2 evil).
- base = lerp of rows 0/1/2 by X (with t = −1 exactly at neutral, a bug in the original that is kept), limited to
  255 − 96·cloudy weather. Entries 48..255 = `(c3·(255−i) + base·i) / 200` (`fn_00869790`; LightBoost from the
  registry, 0 by default) capped at 255; 0..47 another ramp (Land1's land uses 48..255). Lightning: lerp to white.
- Use: land vertex diffuse = table[brightness]; reflected land × 0.5; sea = table[255].
- Values at neutral noon: [48] 686d66, [128] 9eab9f, [218] daf0e0, [255] f3fffb (almost white); midnight
  [255] 3d5b6c; sunset d68e79; evil at noon dbc8ff.
- `palette.raw` is read as R, G, B, A bytes and swapped into D3DCOLOR by the loader (0x835CEC).
- The moon colour (palette row 5 at the alignment column) is stored at [0xFA26DC].
- table[255] ([0xEDDD08] = [0xEDD90C + 0x3FC]) is what the sea reads at 0x87993F, and the light of the missing cells
  (0x8020F8, 0x802525, 0x803365).
- Storm: the overcast amount is clamped to 1 and the clamp written back to [0xFA2754] (0x869DDB); near and far move
  towards 15 / 350 at 0x869E71..0x869E96; the flash also whitens the haze (0x869E98).
- Overcast at the camera: `GCamera::Update` 0x4426BA..0x4426E5 stores (float)(int8)byte 3 (movsx of +0x83) × 0.01
  (0x8C5840) in [0xD1A26C], not clamped; `GLandAlignement::DrawSky` copies it to [0xFA2754] (0x5E2215).
- The copy of the last table is read by the PSys mists (`RenderParticleMist::DrawAt` 0x67A6C7) and the storm puffs
  (`GWeather::DrawClouds` 0x83FF56). The ring creators take table[255] for the hand's rings and table[200] for the
  rain's.
- openblack: `3D/LandLightTable` (exact port, checked against a Python reference), 256×1 texture that
  `vs_terrain` samples per vertex; `u_seaColour` for the sea. `Build(skyType, alignment, overcast, flash)`:
  cap `min(c, ftol(255 − 96·overcast))` without clipping the cloudiness (0x869ADB), flash `c += ((0xFF − c)·f) >> 8` with
  alpha 0xFF across the whole table (0x869C25) and the storm/lightning haze; `LandLightTable::Current().GetRaw(i)` (copy
  of the last `Build`) is the global table 0xEDD90C read by the ring creators (`ECS/WaterRings`); the renderer uses
  `GetRaw` of its own table; `Current()` also keeps the base [0xFA26A4] (`GetRawBase`) and the haze, which are read by
  the PSys mists and the storm clouds. Cloudy = `[0xFA2754]` (`GCamera::Update` 0x4426BA: byte 3 of
  `GetWeatherSmooth` at the camera × 0.01) and flash = `[0xFA2768]` (`Update3D` 0x83587C, the closest storm that
  contains the camera). The cloudiness comes from a single source, `Clouds::WeatherOvercastAtCamera()`; `3D/SkyWeather`
  only gives the flash (`weather::LightningFlashAtCamera(camera)` from `ECS/Weather/LightningFlash`). Test:
  `test_land_light` against a Python generator (exact inputs in binary: with 1.3 or 0.6 the
  float columns fall on the other side of an integer from Python's doubles). The alignment is the sky's smoothed one (`Renderer::_skyAlignment`, [0xBF3378]).
  The sky type of the column is the frame's sample [0xFA26BC] (`sky_type::Frame()`; column
  `sky_type::LightColumn` = (2 − T)·15, haze `sky_type::HazeFactor`, see
  [day-night-weather.md](day-night-weather.md#sky-type-src3dskytype)); `Build(T, …)` receives it as is. The
  original computes the column with `Time2SkyType([0xFA26C4])` (0x869859) and the haze with [0xFA26BC] (0x869D5F): it is
  the same value, because `fn_00869850` runs in `fn_0086A330` right after `fn_0086A2C0` (DrawSky 0x5E2226..0x5E222B).
  With the old convention (2 − T by the forwarder) the table came out identical at every hour (same float column);
  only the last bits of near/far change: v'² = v·v and then `v'²·c + base` (0x869D7A..0x869DAB) instead of
  `base + c·v·v` with v = 1 − |S − 1|, and the exe's exact constants [0x9A3BE0] = 1/900, [0x9A3BD8] =
  0.00013888883, [0x9A3B70] = 1/15, [0x9A3BD4] = 1/350 instead of 0.00111111 / 0.000138889 / 0.0666667 /
  0.00285714 (`test_land_light`, `LandLightTable.MatchesTheOldConventionEveryHour`).

## Distance haze (original, "Fog" detail, levels 3–6)

- Per-frame parameters in `fn_00869850` (0x869CB8..0x869F78 → `fn_007FEAA0` / `fn_007FEAD0`), from the base of the
  light table (with the cloudy weather cap): `k = min(255, (R + 4G + 3B)/8 + 8)`, colour = (R/3, G/3, B/3); with v' = 0
  by day and by night and 1 at sunset: `near = 1/(0.0025 + 0.0075·v'²)`, `far = 1/(0.00111111 +
  0.000138889·v'²)` (400→900 / 100→800). Storm: colour → (c>>3)+32, k → 48, near/far → 15/350 by w;
  lightning: colour and k → 255. The 15/400/64 of the static initialisers are not used.
- `t = clamp((z − near)/(far − near))`, z = view depth along the camera axis. Diffuse ×
  `(256 − trunc((256 − k)·t))/256`; specular += `round(color·t)` with saturation.
- Land: per vertex (`fn_00874AA0` 0x874C5B, SSE `fn_007A1800`), block classes 0/1/2 (block+0x940); the land's
  specular = RGB of the cell (D3DCOLOR, R and B swapped) + haze, added after the small bump.
  Models: once per object at its origin (`fn_007FEB30`), no haze closer than near. It is not applied to the sea, the
  sky (only storm tint), the sun and the moon (alpha ÷ (1 + 8w)), nor to PSys particles, shadows or the hand.
- Values (neutral, clear): noon near 400 far 900 k 211 colour (63, 70, 65); sunset 100/800 k 120 (56, 37,
  31); midnight 400/900 k 81 (16, 24, 28).
- The haze globals: the "Fog" key [0xC37204] (written by `fn_008237B0` 0x8239E2 and `fn_00823AD0` 0x823C1F); near
  [0xC37220] (static 15), far [0xC37224] (static 400), range = far − near [0xE9B6DC] (`fn_007FEAA0`
  0x7FEAB0..0x7FEABB); k [0xC37228] in 1/256 (static 256, an int argument of `fn_007FEAD0`, 0x7FEAE0..0x7FEAF5);
  colour [0xC37214] / [0xC37218] / [0xC3721C] as floats (static 64).
- The packed colour [0xE9B6D8] is `__ftol` of each float: b in byte 0, g in byte 1, r in byte 2, alpha 0
  (0x7FEAFA..0x7FEB26).
- The depth z is the z column of `g_world_to_clipping` 0xEA9E40 (+0x08, +0x14, +0x20, translation +0x2C)
  (0x7FEB4A..0x7FEB75, also 0x874C14 and 0x8773BC).
- Objects (`fn_007FEB30`): with Fog off the specular comes back untouched (0x7FEB36..0x7FEB49); strictly closer than
  near nothing changes (0x7FEB7D..0x7FEB94); t uses fdiv (0x7FEBAD..0x7FEBBE); the diffuse is scaled only when f < 256
  (unsigned, 0x7FEBEB); the haze colour gets alpha 0xFF (0x7FEC80..0x7FEC99); a zero specular becomes that colour as
  it is (0x7FEC9B), any other gets it added with saturation, alpha 0xFF either way (0x7FECA2..0x7FED0D).
- Land vertices (`fn_00874AA0`): with no haze the loop at 0x874B5E jumps to 0x875024; class 1 is 0x874C5B..0x874D1E;
  the specular keeps its **own** alpha (0x874D83), or becomes the haze colour with alpha 0 when it was 0 (0x874D9E);
  then the diffuse (0x874DA1..0x874DF0), only when f < 256 (0x874DA7).
- openblack: a single API, `graphics::haze` (`src/Graphics/Haze.{h,cpp}`) and `assets/shaders/haze.sh` (see the
  next section). Faithful in the rounding: `f = 256 − ftol((256 − k)·t)` (0x7FEBC3), diffuse `(c·f) >> 8` per byte
  with the alpha kept (0x7FEBED), colour with `fistp` (round-to-even, 0x7FEC4A), class 2 with the truncated colour
  [0xE9B6D8] (0x874C48, packed at 0x7FEB26) and saturated add.

## Haze and land light: the common API

**Original.** One haze formula with three implementations (objects `fn_007FEB30`, x87 land `fn_00874AA0`,
SSE land `fn_007A1800`) and four ways of taking the land light under an object, which are not merged:

| Routine | What it does | Users |
|---|---|---|
| `fn_00801C90` (SSE `fn_007A3EC0`) | **integer** bilinear over 4 cells: weights `ftol(frac·256)` [0x8D45CC], first in z (+0x08) and then in x (+0x88), `a + ((b − a)·w >> 8)` per byte; outside the map table[255] and specular 0xFF000000 (0x8020F8) | almost all the models (48 calls) |
| `fn_00802120` (SSE `fn_007A4170`) | the same, with weights `CellX >> 8` and `CellZ >> 8` (0x802206, 0x802237): **almost no interpolation** | `Tree::Draw` 0x74AB1B (and the haze at 0x74AB60), `Scaffold::Draw` 0x6EA6CA, `TownArtifact::Draw` 0x51CB14 |
| `GetAltitudeAndSetColorSpecular` 0x803340 | the cell alone (0x8033FA..0x803413), no haze | `WorshipSite::Draw` 0x519460 (except if it is burning: with `Object +0x44`, the FireEffect, it goes through `fn_00518050` → `fn_0080BEC0`, bilinear and haze; 0x5193FF..0x51940A), `SpellIcon::Draw` 0x5196CC, `Totem::Draw` 0x51ACD1 |
| `fn_00801C90` without `fn_007FEB30` | the bilinear, no haze | `MultiMapFixed::DrawBuilding` 0x517F90 (0x517FB2; with fire only the tint `fn_0080BF10` 0x517FD4): the half-built building. It is called by `MultiMapFixed::Draw` 0x5180A6 if `IsDrawBuilding` (vt +0x8A4: the building plot +0x74, 0x52F0C0; for a Feature, 0x527790: the unfinished ArkDryDock), `Abode::Draw` 0x516129 (with DestructionMesh +0x90: the repaired part) and, with `IsBuilt` (vt +0x890) = 0, `WorshipSite::Draw` 0x5193E9, `SpellIcon::Draw` 0x519668 and `Totem::Draw` 0x51ABC3. `PetitNavire::PreDraw` 0x5DFF20 (hull +0x28, 0x5E018D and 0x5E03DF) and `PostDraw` 0x5E03F0 (sailors 0x5E073B; the deck copies the hull's +0x4C, 0x5E099C..0x5E09A2) never call `fn_007FEB30`. `Scaffold::Draw` 0x6EA5C0: the scaffold through `MobileObject::Draw` (with haze) and the ghost building +0x74 with `fn_00802120` (0x6EA6CA), no haze |
| [0xEDDD08] = table[255] | fixed light | `Dove::Draw` 0x41F75B, `CitadelHeart` 0x466958, clouds `fn_005E1DE0`, sea `fn_00879930` |

Haze class of each block (`fn_00877210` 0x87743D..0x87749B): the depth of the 8 corners of its box
(0x877232..0x877370, one per case of the jump table 0x877D04: centre +0x90C / +0x910 + 80 [0x8D060C] ± 80; in y,
from 0 to `fild(+0x924)` × 0.67 [0xC3720C], or ± that height with [0xE9CD8C]). (inferred) +0x924 is
`LNDBlock::highestAltitude` and [0xE9CD8C] the LandRef detail key.
Bit 2 if a corner goes past far, bit 1 if it is in (near, far]. Class 0 with Fog off or without bits, 1 if there is any
bit 1, otherwise 2.
- Cell layout: every LandBlock holds 17×17 cells of 8 bytes, index (x & 15)·17 + (z & 15) (0x83AED6..0x83AEEC); the
  first dword is the cell colour read as a D3DCOLOR (bytes 0..2 blue, green, red = the file's r, g, b), byte 3 the
  luminosity. The 4 corners are at +4, +0xC, +0x8C, +0x94 (`fn_0083AE80` 0x83AEF7..0x83AF17).
- Cells are global, 0..511 along x and z (world / 10); `g_index_block` 0xE9C964 is 32×32 blocks of 16 cells, index (cx
  >> 4)·32 + (cz >> 4) (0x874650..0x87466F).
- `fn_00801C90`: cell = `__ftol(x·0.1)`, `__ftol(z·0.1)` (0x801CB8..0x801CDC, clipped to 0..0x1FF), weight ftol((f −
  ftol(f))·256) (0x801DCB..0x801E06); the byte lerp a + ((b − a)·w >> 8) with the masks is a + floor((b − a)·w/256),
  alpha 0 (0x801E0B..0x8020F0); off the map 0x8020F8..0x802113.
- `fn_00802120` reads CellX = word [mc + 2] and CellZ = word [mc + 6]; SSE weights 0x7A42AC / 0x7A42BC; off the map
  0x802525.
- `GetAltitudeAndSetColorSpecular` 0x803340: off the map or without a block it exits at 0x803365 / 0x8033DA.

**Light and shadow stamps on the land.** There is no separate texture: they are written into the LandBlock's cells.
- `fn_0086CFF0(pos, texels, pitch, centre, alpha, mode, maximum)` → `fn_0086CF50`: up to 200 stamps (0x86CFF5) in the
  list 0xFA2920 (0x34 bytes, count [0xFA51C0]). The alpha × 255 is clipped and goes through `ftol`. With `centre`, the
  position moves back (pitch − 1)·5 [0x8AB6E4] in x and z.
- `fn_005E5830` → `fn_0086D360` (0x5E592F) applies the list with `fn_0086D060`: cell `ftol(x·0.1)`, weight
  `fistp(255 − frac·255)` & 0xFF, clipping to 0x200 cells and only blocks with +0x920 & 4. Modes 1..8 of the table
  0x86D338 only set a bpp (3, 1, 1, 1, 1, 1, 1, 4); only 1 and 2 stamp.
  - Mode 2, shadow (`fn_00878C70`): bilinear of the map (z with wz, then x with wx), `v = 255 − (255 − r)·alpha/255`
    (0x80808081), **floor 0x30** (0x878DAD) and brightness = **min**(brightness, v) (0x878DBD..0x878DC6).
  - Mode 1, light (`fn_00878780`): per channel, `v = r·alpha/255` added to the cell's colour capped at 0xFF (0x878B09),
    or the maximum if the last argument is not 0 (0x87890D). The map's R goes to the D3DCOLOR's red.
- `ClearLight` 0x5E57B0 → `fn_0086D460`: `fn_00878700` leaves colour 0 and brightness = byte +5 (which in the .lnd files
  has the same value as +3: checked in Land1, Land2 and Norse) and empties the list (0x86D487).
- Callers:
  - PSysLightMaps `fn_006CA280`: + (10, 0, 10) [0x8AB414], centred, mode 1 with bpp 3 and 2 with bpp 1. It is fed by
    `ParticleLightMap::DrawAt` 0x67B220, the shadow of the PSys clouds (0x67A7BE, `GetBitmap` 0x6AA540) and
    `FireGraphic` (0x731633).
  - The map's clouds, `fn_005E25C0` 0x5E2800: sclouds.raw of 40, not centred, mode 2. It covers pitch − 1 = 39
    cells. The map's rows go along x (`fn_0086D060` 0x86D1EC advances `ix·pitch·bpp`) and the bytes along z
    (`fn_00878C70` 0x878DD1): the shadow ends up **transposed** with respect to openblack's earlier one, which read
    `image[z·40 + x]` and covered 41 cells.
  - The storm's shadow, `GWeather::DrawClouds` 0x8400C6: sstorm.raw of 40, centred, mode 2, s = (blackness +
    0.7)·fade.
  - The flash, `fn_00837200` 0x837278: the radial map 0xED92F0 of 64, mode 1.
  - `DanceLight` 0x50F919: not ported.
- `fn_0086D060`'s weight below 0: fistp((i − f)·255) and the cell one less (0x86D110..0x86D135); above 0
  0x86D0E7..0x86D106; & 0xFF at 0x86D19A..0x86D1A7; clipping 0x86D1B5..0x86D241; only modes 1 and 2 stamp (0x86D2E8).
- Light stamp (`fn_00878780`): texel byte c goes into colour byte 2 − c (texel R → D3DCOLOR red), capped at 0xFF
  (0x878B09..0x878B19).
- `ClearLight` goes `fn_0086D460` → `fn_0086D390` → `fn_00878700`.
- The "drawn this frame" bit 4 of block +0x920 is set by `fn_00877210` (0x8774E8).
- The map clouds' stamp: only clouds with an alpha (0x5E27CB), at the cloud's world x, z with y 0
  (0x5E2769..0x5E27B5), `fn_0086CFF0(pos, [0xD1A25C], 40, 0, alpha·(1/255) [0x900058], 2, 0)` (0x5E27DE..0x5E2800,
  `push 0x28` 0x5E27FE). [0xD1A25C] holds `sclouds.raw`; the detail key is "CloudShadows".

**openblack.**
- `graphics::haze`: `Params` (the 7 globals), `Frame()` (the Fog key in a single place), `ApplyObject` =
  `fn_007FEB30`, `BlockClass` = `fn_00877210`, `ApplyVertex` = `fn_00874AA0` and `Uniforms`. `haze.sh`: `HazeT`,
  `HazeFactor`, `ApplyHazeDiffuse`, `HazeColour` (round-to-even), `HazeColourFull` and `HazeAddSaturated`.
- `land_light` (`src/3D/LandLight.{h,cpp}`, `assets/shaders/land_light.sh`): the frame's cells (`BeginFrame`,
  `ApplyStamps`), `At` / `AtCellShift` / `AtCell` / `FullLight`, `AddStamp` / `ApplyStamp` and `LoadBitmapFile` (it
  reads the file and passes it to `graphics::frame_anim::LoadBitmapFromFile` = `GJBitmap::LoadBitmapFromFile` 0x57CA90,
  with the frame layout of `fn_0057CB40`; `frame_anim::FrameTexels` gives one frame, 0x6CA2E3). `Renderer::UpdateClouds`
  uploads the cells as an RGBA texture (colour and brightness) read by `vs_terrain` and `vs_object`.
- The haze class of each block: `haze::BlockCorners` (the 8 corners) and `haze::BlockClassOf`.
- `vs_object`: `u_objectLight.w` = no haze + 2 × `land_light::ObjectMode` per mesh
  (`RenderContext::meshLandLight`). The trees go with `CellShift` and haze; the worship sites (if they are not burning)
  and the icons, with `Cell` and no haze; the Dove class, with `Full` and no haze (`Dove::Draw` only writes +0x4C;
  (inferred) +0x50 stays at 0). The half-built ArkDryDock (`ecs/FeatureBuild.h`) and the boat's hull
  (`missionary_boat::GetHull`, was `petit_navire`) go bilinear and without haze (`DrawBuilding` 0x517F90, `PetitNavire` 0x5E03DF).
- `vs_terrain`: the light and colour of the frame's cell, and the haze according to the block's class (`u_hazeBlock`).
- The shadow of the map's clouds is no longer a separate cap: they are mode 2 stamps (`Clouds::StampShadows`).
- The night lights (`night_lights`, `fn_008229B0`) write into the frame's brightness, after the stamps. `fn_008229B0`
  reads byte +3 of the cell and writes it directly (0x822D9D, 0x822DC3). Before, openblack clipped them with
  `min(loaded brightness, cap)`, so a light did not raise a cell with brightness < 48; now it does.
- `Particles/Creators/LightMap` is no longer a flat sprite: it stamps on the land (`light_map_atoms::SubmitFrame`, once per
  frame: one record per atom, like `DrawAt` 0x67B220). Each atom runs through the map's frames
  (`LightMapCreator::InitAtom` = `CreateParticleLightMap` 0x6A9DEF..0x6A9E16: FrameRate at +0x110, NumFramesInUse at
  +0x114, PlayAnim at +0x118), so the light fades out; stuck on frame 0, the lightning left the ground white. The `UseRandJitter` jitter comes from
  `game_random::LocalFloatRand` (GRand's local seed, the same one `FireGraphic` uses): the 1st draw goes to z, the 2nd
  to y and the 3rd to x (0x67B264..0x67B2A3).

**Remaining differences.**
- (approximate) All blocks are stamped, not only those drawn in the frame (+0x920 bit 4).
- (approximate) Outside the map, or with c00 in a missing block, the shader's bilinear treats each cell as having no
  block (brightness 255, colour 0), instead of giving table[255] and 0xFF000000 to the whole sample (0x801D17..0x801D43 →
  0x8020F8).
- (inferred) In `FireGraphic`, `Object +0x24 & 2` = map object and +0x98 = the object's position.
- (approximate) In `FireGraphic`, the two-sine noise (`VLNoise` 0x590C30 not ported), without the turn fraction, and
  the fire's id instead of `this & 0xFFFF`.
- (approximate) One light mode per mesh, not per instance: if two classes share a mesh (a `DeadTree` or `FelledTree`,
  which use `fn_00801C90`, with that of a living tree), the special mode wins and a warning is logged once.
- `grand_local::LocalRand` / `LocalFloatRand` (3D/LH3DRandom) have been removed: `game_random::LocalRand` /
  `LocalFloatRand` (LHRand over GData +0xC) are called directly instead, and `game_random::crt::Random` for the CRT's
  `Random`. There is no longer a separate generator.
- PLAUSIBLE, not done: the reflection under the water (`vs_object` x = 3) goes without haze, but the original stores in
  +0x4C / +0x50 the light already with haze (`MobileObject::Draw` 0x51818E, `PhysicsObject::DrawAll` 0x646FB1), which
  `DrawUnderWater` reuses.
- Pending: the `vt+0x890 == 0` branches of `WorshipSite::Draw` and `SpellIcon::Draw` (0x5193D9, 0x519658 →
  `DrawBuilding`) and the +0x10C branch of `SpellIcon::Draw` (0x519672, not read).
- Pending: `RendererSea.cpp:164` reads table[255] with `GetColour(255)` and not with `land_light::FullLight`.
- (inferred) The "cell >> 8" and "one cell" modes are only given to `Tree`, `WorshipSite` and `SpellIcon`: the
  scaffolds, the artifacts and the totems do not have their own component yet.
- Pending: the "light without haze" of `DrawBuilding` for the building plots (MultiMapFixed +0x74; openblack does not
  build on plots) and for unbuilt WorshipSite / SpellIcon / Totem; (approximate) the repaired part of a damaged Abode
  is merged with its FragMesh, whose pieces do carry haze (`fn_007F7ED0` 0x7F7F5F, 0x7F807D), and keeps the haze;
  (approximate) the sailors and the boat's deck share a mesh with villagers and cows (one mode per mesh) and keep the
  haze; the scaffold's ghost building is not drawn; the UseLandscapeColor sprites (0x67AFD9) and
  `RenderParticleGJMesh` (0x67C184).

## Camera

- Horizontal FOV 70°, no far plane; the near one follows the height above the ground: `0.3 + 0.16·h` (0.3–3.5).
  openblack recalculates it every frame in `Game` after `camera.Update`.

## Shadows (three systems in the original)

- **Static** (done): `fn_008721A0` (block texture thread, after snow and footprints). All Fixed and MobileObject
  objects cast them (`SetShadowOnTexture` in `Create3DObject` 0x52DE30 / 0x607210), trees and forests with alpha test
  (`DrawTextureShadow`); not AnimatedStatic, DeadTree, flowers, vessels, magic food, crops, shields, seeds, totems.
  LOD 0, shear by the sun x' = x + h, z' = z + h (h above the object's base), coverage 4×2 per texel; the texel's RGB
  × (255 − I/2)/256, that is ×0.5 with full coverage (×0.75 with 128 px textures).
  openblack: `RenderPass::StaticShadow`, `vs_static_shadow_instanced`/`fs_static_shadow` (MAX), the island's R8 texture
  (`LandIsland::GetStaticShadowFramebuffer`), its own instance range (`CastsStaticShadow` in
  `RenderingSystem.cpp`), applied in `fs_terrain` after the footprints.
- **Projected** (the `ShadowInfo` list): its own section, [Projected shadows](#projected-shadows-shadowinfo).
- **Blobs of villagers and animals** (done): `human_shadow.raw` 32×32 between two bones (`fn_0081FFF0`), mode 6;
  see [rendering-objects.md](rendering-objects.md#villager-blobs-object-reflections-and-lod).

## Projected shadows (ShadowInfo)

**Faithful** except where marked.

**Original.**
- A linked list of `ShadowInfo` (0x4AC bytes, head [0xFAA7E0]); each new one goes at the front (`fn_0087FD50`
  0x87FEC4..0x87FEF2). There are only two constructors: `CreateDynamicShadow` (0x80C02C, the hand and the creature) and
  the holder `fn_008745A0` (0x8745BA: physical objects in flight `fn_007FCE80`, the boat `PetitNavire::PetitNavire`
  0x5E11AE, the prediction, PSys and SuperVillagers). Each one has its own 32×32 ARGB4444 texture (si+0x45C) and its
  material `CreateMaterial(6)` with +5 = 0 (single-sided, **CLAMP**, 0x87FE12).
- **Fade** `fn_00874600`: 0 if the block under the emitter is at ≥ 100000 ([0xC37200]); 0 if none of the 9
  blocks at (−60, 0, +60) ([0x8C36A8]) exists, is visible (+0x920 & 1, the 5 outcodes of `fn_00877210` over the 8
  corners of `haze::BlockCorners`) and is at < 100000; otherwise, q = |(x, ground, z) − camera| / (scale · radius):
  255 below 50 ([0xC398F4]), linear down to 0 at 80 ([0xC398F8]).
- **Alpha**: generic `ftol(fundido · base / 255)` ([0x900058], 0x874872); complex (hand) the same below 255 and 255
  otherwise (0x815007..0x815051).
- **Light**: vertical pos + (0, 15000, 0) ([0x9A3C10]); the fixed sun [0xEA1C88] if holder+4 (the boat, 0x5E11B6); the
  hand pos + (0, 200, 0) ([0x8C7B34]); the creature at 3 radii minimum and 45° (0x815058..0x815181, not ported).
- **Silhouette on the CPU** (`fn_00806F60`): projection `fn_00850900` with the **light's absolute y** (t = −Ly / (h − Ly),
  h = max(0, W.y − base)), box = min/max without margin, grid of 128 × 64 subsamples (4 × 2 per texel), triangles
  with the back face discarded (`fn_00850CC0`), half-open 16.16 edges (`fn_0087FF70`), filling by nibbles
  (`fn_00880050`; with si+0x3C the even subrows are not written, 0x880141). Resolve `fn_00880FC0`: texel = popcount
  → alpha **n/15**, the **outer ring** stays at 0. If si+0x10 ≠ 255, **baked fade** n' = floor(n·a/255)
  (0x80769A, `0x80808081`, `and 0xF000`).
- **Chroma path** (the emitter has vt+0x94: trees, dead trees, forests, the hand's food): instead of the
  rasterisation, `DrawTextureShadow32x32` (vt+0x168 = `fn_0080EE80` → `fn_0084B7D0` → `fn_00881DE0`) paints the
  textured triangles with the 64×64 map of its texture's alpha (`fn_00838F00`: high nibble & 0xF0), each texel
  OR `(a & 0xE0) << 7` (at most 7/15), and then a 2×2 OR filter of the nibbles (0x807635..0x807688). If the
  emitter is chroma the held object is dropped (si+0 = 0, 0x8073E8).
- **Land** (`fn_007FF610` 0x7FF749): right after each block of the main land (not the mirrored one), for every active
  shadow whose box touches the block (bx·160 ≤ x1, (bx + 1)·160 ≥ x0; [0x8D151C]), **all of them** (it does not look at
  si+0xC). `fn_00878350`: H = GetAltitude(emitter) if si+0x464, otherwise si+0x18 (the hand: t' = 1), **one H per
  shadow**; t' = (si+0x18 − Ly)/(H − Ly); u, v in the box; diffuse 0 if the altitude byte ≤ 1; codes 0x400 /
  0x40..0x200 and the triangle with a common code is removed. Drawn **immediately** (`LH3DRender::DrawTriangle` 0x82F810
  → IDirect3DDevice7 vt+0x68, 0x82F916).
- **Objects** (`fn_0080B050` → `fn_0084E200`): see
  [rendering-objects.md](rendering-objects.md#object-reflections-and-hand-shadow-on-objects).
- **Where it goes in the frame relative to the Z-sorter**: no shadow is a Z object of its own (`fn_00878350`,
  `fn_0080B050` and `fn_0084E200` are not among the 32 callers of `NewZObject` 0x83F310). The land ones go with the
  land, before the flush (`FinishFrame` 0x82F480); the object ones, at the end of each object's Draw: immediately if it
  is opaque, inside its Z object if it is in the queue.
- List: a new entry is [0xFAA7E0] with +4 = the old head (0x87FED6); `fn_0087FD50` makes its texture with
  `fn_008379E0(0, 0xC4, …)` and clears it (0x87FDBE..0x87FDFC); its material +5 = 0 gives no tiling (0x87864D).
  `fn_0087FF10` takes it out and releases the texture (0x87FF40).
- ShadowInfo fields: si+0x08 active (`fn_00814FD0` clears it when the object is hidden, 0x814FEE; `CHand::Hide`
  0x46C2E0 → `fn_00814FD0` 0x814FE4..0x814FF9, so a hidden hand casts nothing); si+0x10 alpha (0 = not drawn,
  `fn_00881030`); si+0x14 base alpha (0x87FED0); si+0x444 the light, si+0x448 its y; si+0x450 = caster − light
  (`fn_00806F60` 0x80708B..0x8070D4); si+0x464 the caster, written by `fn_00874850` (0x874993), never by
  `fn_00814FD0`.
- Which physics objects get one (`fn_007FCE80` 0x7FCE9D..0x7FCEC7): IsShadowOnTextureChroma (vt+0x94 = `fn_007F98D0`,
  LH3DObject +4 bit 13, tested first, 0x7FCEA1), IsShadowOnTexture (vt+0x84 = `fn_007F98A0`, bit 12) or animated
  (vt+0x1AC). It is removed with the physics (`PhysOb::DeInitialise` 0x7FB772).
- The chroma bit is set through vt+0x90(1) (`fn_007F98B0`) by Tree 0x749F96, BigForest 0x43908B, DeadTree 0x510AC6,
  `FelledTree::Create`'s `fn_00510880` (0x5108DA) and the Pot of info 12 (0x66D173, after the `cmp 0xC` of 0x66D164);
  FieldCrop clears it (0x607EAB).
- Other casters: the SuperVillagers' TemporaryShadow (`fn_00825F20` → `fn_00825090`); the PSys mesh atoms with
  CastHumanShadow (`CreateParticle` 0x6A8B5C..0x6A8B7F → `fn_006CA340` → `fn_008745A0`: si+0xC = 1 at 0x8745C8,
  holder+4 = 0 at 0x8745C1; updated by `fn_006CA540` 0x6CA5A9 → `fn_006CA3D0` (0x6CA3D5..0x6CA3E1) → `fn_00874850`
  with the particle's object; removed by `fn_006C7A80` 0x6C7AA6 → `fn_006CA370` → `fn_0087FF10`). Not ported: the
  creature (LH3DCreature 0x47F543) and the prediction object (`fn_00646FE0` 0x647245).
- A PSys atom's object is an LH3DStaticObject (`CreateLH3DObject` 0x6A8AA6, `LH3DObject::Create(0)` 0x80B4F8):
  vt+0x1AC is 0, so `fn_00874850` skips the pose (0x8748A2..0x8748AA); and it never gets vt+0x90(1)
  (0x6A8AA0..0x6A8AFD; the ctor's +4 = 0x10009 leaves bit 13 clear), so it is never chroma.
- Generic update inputs: the position is obj+0x38..0x40 of the drawn (interpolated, 0x7FCED2) matrix; the scale
  obj+0x44 (a PSys atom's: its PSR +0x30, `Particle3DObj::DrawAt` 0x67A009); the ground GetAltitude 0x874789; the fade
  at 0x874872 (complex 0x815002); alpha 0 tests 0x874898 / 0x815041.
- The light: the vertical one at 0x87490A, the fixed sun at 0x8748E7 (0x8748E0..0x874932); the hand's is obj+0xBC
  (CHand 0x46CB28) + (0, 200, 0).
- The caster's bones: the hand's [0xC37D9C] = CHand+0x47F0 (0x46CB1C), a posed model's (`fn_00839980`), else the
  mesh's rest pose.
- The held object: `SetHeldObject` (vt+0x234 = `fn_00816830`, 0x816855) is only called when IsG3DObjectDrawnInHand
  (vt+0x618, 0x46DC86). The hand's render object CHand+0x4904 is set at the press by `fn_0046DC30` (0x46DCDC, with
  SetCastDynamicShadow(0)) and restored by `CHand::ThrowObject` 0x46DDD0.
- Creature light (not ported): the constant 0.1 is the double [0x8C9D40] (0x8150BF); radius = mesh radius × scale × 3
  (0x815062..0x815081).
- Fade loop (`fn_00874600`): the block under the caster at 0x874603..0x874684; the 3×3 neighbours with x outer and z
  inner, −1..1 (0x87468A..0x874737); cells outside 0..0x1FF (0x87463D / 0x874649) skip the first test and count as no
  block; q = sqrt((dz² + dy²) + dx²) / (scale·radius) with dx, dy, dz stored as floats (0x87478E..0x8747EE); the
  radius is the mesh's +0x30 (half diagonal); the ramp 0x874822..0x874844.
- The box before the first vertex is 0x60AD78EC / 0xE0AD78EC (`fn_00874850` 0x87499B / 0x8749AA, `fn_00806F60`
  0x8070DA).
- Sub-meshes rasterised: those with the LOD 0 bit 0x20000000 ([0xC37D94] = 1), through `fn_0087FA70` → `fn_00850900`;
  caster 0x807109..0x807148, held object 0x8071D3..0x807249. A skinned vertex uses its bone (branch [0xFA93BC] & 1,
  0x850B04; one bone matrix per vertex group {count, bone}, 0x850B04..0x850B2B).
- Projection (`fn_00850900`): the matrix's ty is lowered by the base y (0x85094F..0x85096E; the skinned branch does
  the same to its copy of the bone matrix, 0x850B2D..0x850B35); h = max(0, W.y) (0x8509D4..0x8509E3); the box also
  tracks k = W.z·d.z + W.x·d.x as kMin (0x850A05..0x850A2C, 0x850A1B); Ly is negated at 0x850914; the box grows at
  0x850A70..0x850AB8.
- Two-sided casters: material +5 & 1 or a mist (vt+0x1F8 = IsMist, 1 only in Mist 0x55EB90; no mist gets a ShadowInfo)
  (`fn_00850CC0` 0x850CC9..0x850CEE).
- Winding (`fn_00850CC0`): a one-sided triangle is kept when (r0 − r2)(x1 − x2) ≥ (r1 − r2)(x0 − x2), rows r = ftol(z)
  (0x850D03..0x850DA1), and walked 0→2, 2→1, 1→0; a two-sided one is walked 0→2→1 when (r0 − r1)(x2 − x1) < (r2 −
  r1)(x0 − x1), else 0→1→2 (0x850E29..0x850F6D).
- Grid: [0xC39B08] = 128 and [0xC39B0C] = 64 (0x807077 / 0x807081); one coverage byte per texel at si+0x40, cleared at
  0x80706B..0x807075; bits 0..3 the even subrow, 4..7 the odd one, bit x & 3 for the subsample (masks 0x9A3C74 /
  0x9A3CB4 / 0x9A3CF4 / 0x9A3D34, also 0x9A3D74 / 0x9A3D78).
- Into the grid (0x807265..0x807305): px = (x − x0)·(128/(x1 − x0)), pz = (z − z0)·(64/(z1 − z0)), the factors stored
  as floats first ([0x8C6CA8] = 128, [0x930678] = 64), each clamped to 0 below and to 127 ([0x8C4A00]) / 63
  ([0x9A2BF8]) above.
- Span tables: [0xFA9FC8] the left ends (edges going up), [0xFA97C4] the right ends (going down), the touched rows
  [0xFAA7D0]..[0xFAA7CC]. They are static and never cleared: each triangle only resets the rows ([0xFAA7D0] =
  [0xC39B0C], [0xFAA7CC] = 0, 0x850DB2 / 0x850DBD).
- Edge (`fn_0087FF70`): horizontal edges are skipped (0x87FF7B); edges going up are swapped into the left table
  (0x87FF81..0x87FF9D); off-grid edges are rejected (0x87FFAE / 0x87FFB8); slope = (x1 − x0)/(y1 − y0) with fidiv
  (0x87FFBE..0x87FFD0); y1 is clamped to the rows; with y0 < 0, x0 − y0·slope (0x87FFD8..0x87FFE8); the step is
  ftol(slope·65536) ([0x8AC408]) and each row stores x >> 16 (0x880030).
- Fill (`fn_00880050`): each row of [minRow, maxRow) fills [max(0, l), min(grid x, r)), nothing when empty
  (0x8800A0..0x8800D0); the odd subrow test is `test bl, 1` (0x8800E0); two subrows per texel row
  (0x88019E..0x8801A7).
- Resolve: rows and columns 1..texels − 2 only; the table [0xFA95C4] = popcount(i) << 12 is built by `fn_00880F20`.
  The baked fade is skipped when the alpha is 255 (0x8075BD..0x8075D1).
- The chroma map (`fn_00838F00`): 64×64, cached at texture+0x12C; byte (r, c) = the high byte of texel (ftol(r·h/64),
  ftol(c·w/64)) & 0xF0 (0x838F88..0x838FD0, [0x8D8BD0] = 1/64).
- The chroma path (`fn_0080EE80` into the 16-bit target of `fn_008816F0`): every sub-mesh with the LOD 0 bit
  (0x80EECA..0x80EED5) **except boned ones** (`fn_0084B7D0` 0x84B7F5: [0xFA93BC] & 1), each primitive with its
  texture's map (0x84B97E), through the object's matrix [0xEA1AE8] (no bones), into LH3DP3::Table1 0xE437E0; triangles
  of 6 bytes (0x84B99C..0x84BA71).
- Chroma vertex (0x84B804..0x84B96D): t = (si+0x18 − Ly)/(W.y − Ly) (no clamp of h); x = ((W.x − Lx)·t + Lx −
  x0)·(32/(x1 − x0)), clamped to [1, 31] ([0x8AA390], [0x92B6F4]), z the same; u, v = uv × 63 ([0x9A2BF8]); W = (x·m00
  + z·m20) + y·m10 + t (0x84B83F..0x84B88D).
- Chroma triangle (`fn_00881DE0`): vertices ftol'd (0x84B9AA..0x84BA4C) and clamped to the target
  (0x881DE9..0x881E83), sorted with a the upper vertex (ties: level and to the right; 0x881E86..0x881EEA); edges
  `fn_00881A60` (12-byte x, u, v entries, rows inclusive, steps ftol(d·(1/(y1 − y0 + 1))·65536); a horizontal edge
  stores its ends as they are, not in 16.16); spans `fn_00882080`, inclusive, u and v stepped by an integer division;
  texel index ((v >> 10) & ~0x3F) + (u >> 16) (0x882141..0x882150).
- Order inside `fn_00806F60`: the grid, then a chroma caster (vt+0x94, 0x80732B) drawn into the cleared 0x800-byte
  buffer (0x807339..0x8073E3, its raster 0x8073F5..0x807466 skipped), else the raster; the held object
  (0x807466..0x8075B7); the resolve (0x807601); the chroma blur; the baked fade.
- Visibility of a block (`fn_00877210` 0x8773E4..0x8774E2): the 8 corners through `g_world_to_clipping`; outcodes Z <
  near ([0xE839E0], also read by `CameraModePath::SetUpNearClipping` 0x460F10), X > Z else −Z > X, Y > Z else −Z > Y;
  not visible when one outcode holds for all 8; stored at 0x8774E8 / 0x877CEB.
- Over the land `fn_007FF610` is called from `GLandscape::Draw` 0x5E4E96; `fn_0084E200` draws at once through the
  draw-triangle pointer [0xC386EC] (0x84E8F0).
- Morphable receiver test: when width and depth are equal (or unordered) the depth is taken (0x80E7A7..0x80E7B4).

**openblack.**
- `src/Graphics/ShadowMath.{h,cpp}` (`graphics::shadow_math`, without bgfx or ECS: fade, alphas, lights, projection,
  rasterisation, resolve, chroma filter, baked fade, block and visibility test), with `test_shadow_math`.
- `src/Graphics/ShadowList.{h,cpp}` (`graphics::shadow_list`): the list and its producers (the hand, the physical
  objects in flight from `PhysicsObjects::ForEach` with `CastsPhysicsShadow`, and the `components::DynamicShadow`: the
  boat with `useSun`), one 32×32 R8 texture (n·17, CLAMP) per entry, uploaded every frame. The pose comes from
  `L3DSubMesh::GetSkinBones` / `GetSkinLocalPositions` and from `ecs::PosesByInstance`.
- `src/Graphics/RendererShadows.cpp`: `UpdateShadows` (once per frame), `DrawLandShadows` (in the block loop of
  `Renderer::DrawPass`, Main view, right after each block's `submit`; program `LandShadow` =
  `vs_land_shadow` / `fs_land_shadow`, which shares `land_position.sh` with `vs_terrain` to give the same Z;
  `render_modes::ZFunc::LessEqualInclusive` (the D3DCMP_LESSEQUAL of 0x82CCC5, which with inverted depth is
  GEQUAL; also the one for the animated and morphable receivers), without writing Z, mode 6, the block's culling; texture in stage 11) and the shadows
  on objects (`CollectShadowReceivers`, `DrawShadowsOnObject`, `DrawShadowsOnCutObjects`, `ClearShadowReceivers`). `shadow.sh`:
  `LandShadowUv`, `ObjectShadowUv`, `ShadowKept` (the 0x400 code).
- `fs_terrain` no longer has shadows (gone are `s7_dynamicShadow`, `u_dynamicShadow*` and the loop of 16
  `u_physicsShadow*`); `PhysicsShadows.{h,cpp}`, `vs_dynamic_shadow_instanced`, `fs_physics_shadow_resolve`,
  `DrawHandShadowPass` and the `DynamicShadow` / `PhysicsShadow` / `PhysicsShadowResolve` views were deleted. No cap of
  16.
- Order relative to `graphics::zsort`: the land ones, in Main with their block (before the queue); the object ones,
  after the object's `DrawMesh` in Main or inside its queue entry (`MainBlended`); no new entry in the queue.

**Differences.**
- **The hand (done; source: the user's captures of the original, 2026-10-02)**: `shadow_list::k_HandShadowAsOriginal
  = true`, like the original: si+0x3C = 1 (`CreateDynamicShadow` 0x80C037), so `fn_00880050` does not write the
  even subrows (0x880141..0x880146) and the hand reaches at most 4/15; 32×32 (`fn_0087FD50`); base at the hand's own y
  (si+0x18 = obj+0x3C, 0x8152B1..0x8152B4), with t' = 1 on the land; the held object si+0x00
  (= obj+0x8C, `SetHeldObject` `fn_00816830` 0x816855) rasterised into the same `ShadowInfo` with its own base
  (0x807163) and at full density (si+0x3C saved, set to 0 and restored, 0x807532 / 0x80753D / 0x8075B7; in the loop
  over its primitives 0x80714F..0x807259). A chroma held object uses the `ShadowInfo`'s base (si+0x18, `fn_0084B7D0`
  0x84B7E0), not its own. With `false` openblack's look from before the list comes back (64×64, full density, on the
  ground under the hand, without the held object), only for comparison.
  - What the captures show: the hand's shadow is its silhouette, with the fingers, light grey and half transparent
    (4/15); a held orb casts a darker, round shadow below it (full density); the orbs are drawn whole on top of the
    shadows; the dispenser keeps its shadow.
    - [img/original_hand_shadow_over_dispenser.png](img/original_hand_shadow_over_dispenser.png): the hand over the
      dispenser's orb; behind, on the ground, the light silhouette of the hand.
    - [img/original_hand_shadow_orb_over_dispenser.png](img/original_hand_shadow_orb_over_dispenser.png): the hand with
      an orb over the dispenser; dark, round shadow at its foot.
    - [img/original_hand_shadow_red_orb_over_dispenser.png](img/original_hand_shadow_red_orb_over_dispenser.png): the
      same with another orb.
    - [img/original_hand_shadow_orb_over_ground.png](img/original_hand_shadow_orb_over_ground.png): the hand with an
      orb over the grass; dark, round shadow displaced down and to the left.
  - Comparison (openblack, scene `OPENBLACK_TEST_DISPENSER=NORSE_ABODE_SPELL_DISPENSER,1826,2670,10,2`,
    `OPENBLACK_CAMERA_LOCK=1816,52,2656,1826,37,2670`, 13 h; the hand picks up the orb with `OPENBLACK_TEST_TUG=1700,2500,10,3`
    and `OPENBLACK_TEST_TUG_MOUSE2`): before, the hand's shadow was a dark silhouette (8/15) and the held orb had no
    shadow; now the silhouette is light grey with the fingers, as in the first capture, and with the orb in the hand
    the dark, round shadow of captures 2 and 4 appears below it, on the grass and at the foot of the dispenser; the orb
    stays whole on top.
  - **(approximate)** the darkness of the orb's shadow: on the grass, openblack ends up at 0.46 of the ground's
    luminance (65 / 142) and capture 4 of the original at 0.55 (63 / 114). openblack gives the code's maximum: with the
    trace (`OPENBLACK_SHADOW_TRACE=1`) the shadow carries alpha 255 (fade 255, without
    `BakeAlpha`) and n = 8 at most (4×2 subsamples per texel, `fn_00880FC0`), 8/15 → 1 − 0.533 = 0.467. The difference
    fits a fade in the original below 255 (n' = floor(8·a/255) = 7 for a = 223..254, 0x80769A..0x8076EC; the capture's
    camera is not known), half-covered edge texels or the capture's lighting; nothing has been read in the binary that
    explains it, so the code stays as it is.
  - The dispenser's **own** shadow is not a `ShadowInfo` (the trace only lists the hand's): the dispenser is an
    `Abode`, so it casts a **static** shadow (`CastsStaticShadow`, `fn_008721A0`, shear x + h, z + h).
    With the camera on the opposite side (`OPENBLACK_CAMERA_LOCK=1846,47,2690,1826,37,2670`) it is
    seen as a soft blotch in front of the tripod, lightened by the dispenser's disc and sparks; with the captures'
    camera it falls behind the table.
- **(approximate)** the 0x400 code, which in the original removes whole triangles: on the land, `fs_land_shadow`
  removes the triangle when its three vertices carry the code, but does it per fragment with the interpolated value,
  so along the edges a few stray fragments too many or too few remain; on the objects, `fs_object_shadow` applies it
  per fragment. With vertical light k = 0 and nothing changes.
- **(approximate)** the redraw over the land (Z GEQUAL without writing Z) depends on `vs_terrain` and
  `vs_land_shadow` giving bit-identical depth: they share `land_position.sh`, but they do not carry `precise` /
  `invariant`. On the current backend (D3D11) there are no specks in the captures. If they appear on another backend or
  with another shader compiler, plan B is a minimal bias towards the camera in `vs_land_shadow` (with inverted Z,
  `gl_Position.z += ε·w`) or declaring the position invariant in both. The same applies to the shadow on static
  objects (ZFUNC EQUAL against the object's instanced draw).
- **(inferred)** linear filtering of the texture (that of LH3D's default stage); the boat's visibility with the normal
  camera and not the mirror's; an invisible block has its new distance here (the original keeps the old one).
- Not ported: the creature, the prediction, the SuperVillagers and the PSys meshes (`fn_006CA340` from 0x6A8B76,
  `MeshCreator::InitAtom`).

## Sky: sun, moon and clouds (original)

- The time the sun and the moon use is the **script time** (fixed thresholds 3.5 / 7.5 / 8 / 8.5 h); the game's real
  clock is the visual time with the cycle's thresholds. See [day-night-weather.md](day-night-weather.md).
- The **dome** (the `sky_*.555` textures) follows the sky type of the visual time, with a hysteresis of 0.03 and 32
  rows per frame (`fn_0086A330` / `fn_0086B7F0`; `sky_type::DomeBlend`, `Sky::UpdateDome`). All the detail, in
  [day-night-weather.md](day-night-weather.md#sky-type-src3dskytype).
- **Sun** (`fn_0086C020` / `fn_0086C140`): `sun.l3d` (vertical quad 9928), `sun.raw`, mode 13 (additive SRCALPHA/ONE,
  no Z), at (−30000, y, −30000) rotated 3π/4, y = 7500·(clamp(min(T, 24−T), 6, 12) − 6)/6, colour 0x957C63, alpha
  0 → 255 between 3 and 6 h and 255 → 0 between 18 and 21 h (÷(1 + 8·clouds)). **Glare** (`fn_0086BB60`, at the end of
  the frame): the same mesh ×1.8, colour 0xA06A35, without Z test, visibility from 5 samples occluded by the
  terrain, smoothed 1 %/ms.
  - The alpha A is [0xC395A8]. The glare [0xFA2778] moves by (target − glare)·(`g_game_time_inc` × 0.01 [0x8C4B10]),
    clamped to 0..255 (0x86BBA1..0x86BBF9): the factor is not capped (`g_game_time_inc` ≤ 199) and the glare stays put
    while paused.
- **Moon** (`LH3DAtmos::UpdateGame` 0x8356E0): camera + (4000, 1100·cos θ − 150, 800·sin θ), θ = T·π/12; alpha
  m = min(200, 0.5·y − 110); colour = row 5 of `palette.raw` by alignment. Halo (`fn_0086A930`): 4000 quad on the
  basis of `fn_0086AC60` (+Z from the eye to the moon, X horizontal in the view), `atmos.raw` UV 0.25–0.49375 (v0 at the
  bottom left = 0.25), additive, colour (R/6, G/5, B/4, m). Mesh: `moon.l3d` (loaded without skins, texture
  `weather.raw` by code), that basis ×4, `fn_0086AFA0`(−0.1309) = Rz(+7.5°) in glm, RotateY(phase + π) =
  Ry(−(phase + π)) in glm, ×0.65 (details in [rendering-objects.md](rendering-objects.md#objects-that-face-the-camera-billboards)); the phase comes from the **real clock**
  (2π(1 − frac((days − 10962)/29.5306))) and regenerates the UVs. Normal culling (bit 0 = 0).
  **Reflection** (`fn_0086B010` 0x86B61D; done): the first call draws halo + mesh + the mesh's `DrawUnderWater`
  (vt+0x118: the moon mirrored at y = 0, unlit); the second, with `pos.y = −pos.y` and `[0xFA2774]` = 1, only the
  halo. Same m and same colour, in the sky stage (the reflected land covers it). In openblack, `DrawMoon(…,
  mirrored)` in the reflection pass: the halo with the mirrored camera's view and the mesh with the main one's
  (it comes out flipped, like `DrawUnderWater`, with the culling inverted). The tilt k = −1 of the second call is never
  seen: that call does not draw the mesh (`fn_0086A930` 0x86AC0F). The sun is **not** reflected (`fn_0086C140` is
  called only once).
  - The tilt [0x9A3BF0] is applied with k = +1 at 0x86AEF3..0x86AF0B; `fn_0086AFA0` is 0x86AFAC..0x86B008 (r0' = c·r0
    − s·r1, r1' = c·r1 + s·r0).
  - The basis (0x86AC67..0x86AE4F) leaves an exactly null vector unnormalised (0x86AD26..0x86AD58,
    0x86ADAF..0x86ADD7).
  - Halo: corners at 0x86AA3C..0x86AB67 (half width 2000, the rows already × 4); UV constants 0.25 = 0x3E800000
    (0x86A981) and 0.49375 = 0x3EFCCCCD (0x86A996); the triangles 0xEDC310 are set at 0x86A933..0x86A974.
- **Night-time hand glow on the water** (done; `3D/HandWaterGlow`, `Renderer::DrawHandWaterGlow`):
  `GLandscape::Draw` 0x5E4D89, if k = `[0xD20184]` > 0.01 (k = clamp((120 − mean of the base colour)/15, 0, 1), the same
  as the hand's light on land): colour = row 6 of `palette.raw` (`[0xFA26E0]`) moved a quarter of the way towards
  (255, 128, 64) per channel (integer: floor((3c + target)/4)), alpha clamp(ftol(k·190), 0, 190). `fn_005E3F70`: only
  if some cell of [(x − 70)/10, (x + 70)/10] × [(z − 70)/10, (z + 70)/10] (lower limits clipped to 0..511,
  the upper ones not) has altitude < 5 or has no cell; horizontal quad at y = 0 of (x ± 60, z ± 60), UV
  (0.75; 0.375)–(0.796875; 0.421875) of `atmos.raw` (u with x, v with z), `LH3DAtmos::AdditiveMaterial` (mode 13,
  SRCALPHA/ONE; with the texture `data\textures\atmos.raw` (`fn_00835AD0` 0x835C20..0x835C4E creates `AtmosMaterial` mode 6 and `AdditiveMaterial` mode 13 with it)), ZFUNC ALWAYS, right before the sea. In openblack it goes at the
  end of the reflection pass (a quad at y = 0 is its own mirror) and is seen through the sea; with the hand over
  high land nothing appears.
  - k > 0.01 is tested at 0x5E4D8F; the colour is made at 0x5E4DA0..0x5E4E57; the maximum alpha 190 is 0x8DAD54; a
    cell altitude byte below 5 counts as low (0x5E410A); the cell is ftol then a signed division by 10 (0x66666667,
    `sar 2`: both truncate towards 0).
  - The quad's corners go (x − 60, z − 60), (x + 60, z − 60), (x + 60, z + 60), (x − 60, z + 60) with the UVs of
    0x92B2F8 in that order (a 12×12 texel spot of the 256×256 atmos.raw) and the indices 0x92B2E0 = {0, 1, 2, 2, 3,
    0}; ZFUNC ALWAYS at 0x5E4281, no culling.
- **Clouds** (`CloudInSky::Open` 0x5E23F0, `fn_005E25C0`): 70 + 2 fixed ones, x ∈ ±8000 (wind at 70 u/s, angle 3π/4,
  around (1280, 1280)), y 300–500, z ±5000, size 13–50, k 2.5–5; edge alpha beyond ±6000.
  `mist.l3d` without skins: smoke material `smoke.raw` + `smokea.raw` (mode 6, two-sided, `fn_0080BBD0`). They are
  LH3DMist objects (+0x80 |= 2, effect branch), so they use the same `fn_007FA300`: the **billboard**
  mat3(right, −forward, up) from the "Map mist" section and the **non-uniform scale** s = size/(1 + (k − 1)
  (1 − |dy|/|d|)) with the size in row 0 (width on screen) and s in rows 1 and 2 (depth and height). Near the
  horizon |dy|/|d| ≈ 0.05–0.2, so each cloud is a **horizontal ellipse ~k times wider than it is tall** (2.5–5; the
  two fixed ones, size 300 and k 20, almost flat bands); they only look round right underneath. Animated 8×8 atlas
  (frame (counter/20) & 15, UV ((f&7)/8, (f>>3)/8 + 0.25)), zenith light with ambient 210/256; alignment colour ×
  table[255] · 186/256 + 35;
  alpha 0 on good land, 200 neutral, 255 evil.
  - The cloud count 70 is [0xBF33A8]; the wind angle is the constant [0x92B2A0] = 2.35619455575943 (3π/4).
  - The mist / cloud sphere radius for the screen test is the mesh's +0x30 (0x7FA7FE) × size (+0x88) × 0.55
    ([0x8D3E80], 0x7FA814).
  - **Placement** (`CloudInSky::Open` 0x5E2439..0x5E24F4): five
    `Random` calls per cloud in this order (x, y, z, size, k), also clouds 0 and 1, each one on its own and uniform in
    the box: **the sky clouds are not in groups**. The groups that are seen come from chance (≈1500 u on average
    between 68 clouds, with streaks and gaps) and from perspective; the original's real groups are the storm clouds
    (`GWeather::DrawClouds` 0x83FC90: up to 16 balls per storm around its centre, darkened and with haze; ported,
    see [miracles.md](miracles.md#the-flash-and-the-clouds-of-the-registered-storms-ecsweatherlightningflash-stormclouds)).
  - `Random` 0x81D180 = min + (max − min)·(rand()·3.0518509e−05f) with the MSVC CRT's `rand()` (0x7C8837,
    s = s·214013 + 2531011, (s >> 16) & 0x7FFF), not the synchronised GRand. **Correction (game_random)**: the `srand(time(NULL))` at 0x577721 is in
    fn_005776E0, which is only reached when saving `creature.lhp`: `rand()` starts at seed 1 (`__initptd`
    0x7D2323) and is per thread; what changes the sky between sessions and lands is the `rand()` calls spent before
    **(inferred)**. `GLandscape::Open` → `GLandAlignement::Open` 0x5E1D10
    → `CloudInSky::Open` rebuilds the 70 clouds on every land load. openblack: `Clouds` (the same generator; fixed seed
    with `OPENBLACK_CLOUD_SEED=<n>`; `Clouds::OnLandscapeOpened` from `InitializeLevel`).
  - Step (`fn_005E25C0`): x += inc·70·0.001; past 8000, t = x + 8000, x = t − ftol(t/16000)·16000 − 8000 (only x:
    each cloud comes back along the same line at the same height); edge = fistp((x ± 8000)·0.1275) (rounding), fixed
    ones 192; alpha = edge·A/255 in integers.
  - **Colour** (`fn_005E1DE0`, every frame from `GLandAlignement::DrawSky`): i = trunc(X), f = trunc((X − i)·256), each byte a + floor((b − a)·f/256)
    between 00FFFFFF / C8FFFFFF / FFAAA066 (0xBF339C), RGB·table[255] (c·t >> 8), then c + floor((8960 − 70c)/256) (255 →
    **220**: the original never paints them white). Per-vertex light (`fn_0084BA90`): I = fistp(255·N·L),
    f = 210 + (45·I >> 8) (210..**254**), diffuse (c·f) >> 8. The time of day only comes in through the table (rows 0-2
    of `palette.raw`) and the weather only through its cloudiness cap. Neutral noon ≈ (172..208, 179..217, 177..214),
    light grey that with α ≈ 0.7 over the blue looks whitish; at sunset salmon; evil: opaque ochre; good: none.
    **Land1 starts at 7.3 h script time** (almost full sunset) with the clock stopped: hence the brownish clouds; with
    `OPENBLACK_TIME_OF_DAY=12` they come out light grey as in the original.
    - The result goes to [0xBF3398]. The light product is per channel (R `imul` 0x5E1EE9, G 0x5E1F02, B 0x5E1F1E, each
      `sar 8`); the alpha byte (+0x1B, written at 0x5E1ECA) is the lerped one, untouched; the last step is c + (((35
      << 8) − 70c) >> 8) (0x5E1F28..0x5E1FB4).
  - **Sky alignment** [0xBF3378] (0 good, 1 neutral, 2 evil; it starts at 1 and loading a land does not touch it):
    target [0xBF337C] = (1 − clamp((v + 1)/2, 0, 1))·2 (`fn_005E2240`), with v = `GetAlignmentValue` of the player with
    the most influence at the interface's position (`fn_0064AC30` from `GPlayer::ProcessPlayers` 0x64A697, every turn;
    `DoCitadelMultiplayer` forces 0.5). `DrawSky` 0x5E2160 moves it 0.001 per ms (inc·0.01·0.1) and snaps it when it
    overshoots; it is used by the clouds, the light table and the sky. openblack: `SkyAlignment` (Renderer), target
    `Clouds::InfluentialPlayerAlignment()` (`ecs::effects::alignment::GetInterfaceAlignment()` × 2 − 1, or the
    debug slider, or `OPENBLACK_TEST_SKY_ALIGNMENT` from −1 evil to 1 good; `atmos_banks::Alignment()` uses the
    same value) and cloudiness
    `Clouds::WeatherOvercastAtCamera()` = byte 3 of `weather::atmos::GetWeatherSmooth(camera, 1)` × 0.01 (0 without
    storms).
  - **Animation**: each cloud is an LH3DMist with its own counter +0x84; `LH3DMist::AddDrawing` 0x7FA7F0 (vt+0x100)
    only sends it to the Z-sorter if its sphere (half diagonal of the mesh × size × 0.55) touches the screen, and only
    then does the counter advance in the Draw: the clouds drift out of phase with each other. **There is no blending
    between frames**: `fn_007FA300` computes frame = (counter·45)/900 in integers (0x7FA3F4..0x7FA41B, no fraction),
    sets a single UV offset (vt+0xE8 = 0x7F9B70: +0x68/+0x6C) and draws once (`fn_0080DB30`); mode 6 (`fn_0082DF10`)
    only configures stage 0 (MODULATE texture × diffuse). The change is abrupt every 20 counts (≈78 ms, 16 frames in
    ≈3.5 s); the same goes for the map's mist.
- **Cloud shadows**: `sclouds.raw` 40×40, one texel per cell from the cloud's corner,
  `lum = min(lum, max(48, 255 − (255 − s)·α/255))`; openblack: `Clouds::BuildShadowCap` → per-cell R8 texture used by
  `vs_terrain` and `vs_object` before the light table.
- **Skin ids 0xFF…** (`mist.l3d` 0xFFD0EBBE, `moon.l3d` 0xFF52BA30): they are never resolved; those meshes are
  loaded without skins (`LH3DMesh::Create(data, 1)`) and their texture is set by the code. Moreover their skin table
  starts at the end of the file (like `sun.l3d`): openblack's L3D parser now tolerates it.
- **General fix**: `HashIdentifier(hashed_string)` hashed the number again; `Contains`/`Load` with
  `entt::hashed_string` found nothing (it was the cause of the "missing raw/* textures").

## Rivers

**Faithful** (done).

- **There is no river renderer.** `GStream` (GameThing 0x47, Stream.cpp) stores its points in script order
  (`CREATE_STREAM_POINT` 0x717550 sets y = ground height and appends at the end); the segments are p[i] → p[i+1]
  (openblack linked each point with the closest previous one: corrected, `Stream::points`).
- `GStream::CreateAll` 0x733FF0 (after the script) → `CreateRiver` 0x7341E0: per segment, angle θ = atan2(dz, dx),
  position p[i], scale only in local X = 3D length / 30, and two terrain footprints (fn_0081E9E0):
  `data\river2.l3d` (brown riverbed, blended into the block's colour like a building's footprint, fn_008728A0) and
  `data\river.l3d` (flag +0x38 = 1: only its alpha, which **lowers** the block's alpha nibble: min(dst, src),
  fn_00872AB0; banks 15/15, channel 10-14/15). 32×64 ARGB4444 footprints, 10.6 × 30 units.
- The visible water is **the sea**: it is drawn before the land (ZFUNC ALWAYS) and the land (mode 14, SRCALPHA) lets
  it show through in the channel. No texture, scrolling, particles or LOD of its own; the "flow" is the sea's movement.
- openblack: `ECS/Rivers` creates, when loading the map, two `StreamFootprint` + `Transform` entities per segment; the
  riverbed goes in the footprints pass (`Renderer::DrawRiverFootprints`) and the channel in `RenderPass::LandAlpha`, an
  R8 of the whole island cleared to 1 with MIN blending (`fs_land_alpha`, nearest texel, quantised to 1/15);
  `fs_terrain` multiplies its output alpha by that value.
- Not analysed: the sound `ATMOS_TYPE_RUNNING_WATER` (`audio/sfx/atmos/stream.sad`). The waterfalls: see
  [water.md](water.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur) (`GWaterfall` draws nothing; the one in `waterfall3.l3d` is `DesignedWaterFall`, only in Land 3).

## Screen fade and cinema bars

**Faithful** (done; without the black of `OnNewGame`).
- GScript state (g_game+0x250090): +0xB0 alpha step per turn, +0xB4 alpha, +0xB8 ARGB colour.
  - `SET_FADE(r, g, b, t)` (0x6FCD70 → `SetupScreenFadeTo` 0x6EBA90; all truncated, t as a char): t ≤ 0 → A = 255
    instantly; otherwise, alpha 0 and step 255/(10t) (the A byte does not change until the next turn).
  - `SET_FADE_IN(t)` (0x6FCE00 → 0x6EBB00): t ≤ 0 → A = 0; otherwise, alpha 255 and step −255/(10t).
  - `FADE_FINISHED` = step == 0. `ProcessFade` 0x6EB9D0 once per game turn (100 ms): it does not advance while paused.
- Drawing `fn_0086FEE0` (FinishFrame, right before EndScene, after the help and the 2D rectangles): if A ≠ 0, colour
  quad x 0..W−1, y h'..H−1−h' (h' = h − 1 with bars), mode 1, ZFUNC ALWAYS; then the bars again on top; the colour
  is set to 0 every frame.
- Bars `[0xEB9950]` = f: height `trunc((H − 0.5625·W)·f)/2` (16:9 with f = 1; nothing on wider screens),
  black. `SET_WIDESCREEN` (32, `HelpSystem::SetWideScreen` 0x5C6AD0) slides f linearly over
  `HelpSystemInfo.wideScreenTime` = 2 s of game time, continuing from wherever it is; `WIDESCREEN_TRANSISTION_FINISHED` (132).
- Other sources: `OnNewGame` 0x553980 sets black instantly on Land 1 (the `FollowUs` intro removes it with
  `SET_FADE_IN(12)`); `Temple::UpdateFade` 0x794280 in the citadel (real time, 1/s), not ported.
- `FADE_FINISHED` is `fn_006EBB50`; `WIDESCREEN_TRANSISTION_FINISHED` is 0x6FAC20.
- Each bar's height is computed by `fn_005C5780`.
- Reversing `SET_WIDESCREEN` in the middle of a slide: `GetWideScreenPercentage` 0x5C6AE3 is read before +0x45E8
  changes, then +0x45F0 = (on ? p : 1 − p)·wideScreenTime·1000 (0x5C6B1F..0x5C6B4E).
- `Temple::UpdateFade` writes the colour every frame it runs through `fn_0053CE60` ([0xFA51D8], 0x794361), leaving
  ProcessFade's step and alpha as they are.
- `LH3DRender::FinishFrame` 0x82F460 order: the bars (0x82F652..0x82F6DD), then the callbacks with bit 0x80000000
  (0x82F6E5..0x82F718, among them the video), and the fade `fn_0086FEE0` last (0x82F753), over the film.
- openblack: `3D/ScreenFade`, view `RenderPass::ScreenOverlay`, `Renderer::DrawScreenOverlay`. The black of `OnNewGame`
  is **not** applied: openblack's intro still stops before its `SET_FADE_IN` (`START_CAMERA_CONTROL` and others are
  stubs) and the screen would stay black.

## Text: the original's fonts and the hand message

**Faithful** (done; the offset relative to the hand is **(approximate)**).
- Fonts `Data\j0` ("Ocean Sans MM", the one for the messages), `f1`, `f3`: `.met` = u32 cell height 80, wchar[128]
  name, u32 count, 28-byte records {u16 code, u16 bitmap width, s16, u16, f32 left, f32 width,
  f32 right, u32 offset and u32 size in the `.fnt`}. `.fnt` (`CachePage::RenderChar` 0x830C10): lengths of
  1-bit runs alternating 0/1 starting from 0; one byte, or 0xFF + u16; bitmap of width × 80 by rows.
- Glyph cache: white ARGB4444, alpha by 2×2 blocks with the table 0x9A3990 {0, 4, 8, 12, 15}/15, one transparent
  texel on each side; rows 40..59 at a quarter (sizes < 26). `DrawTextRaw` 0x832C60: s = size/80,
  X0 = x + advance + left·s, X1 = X0 + (width + 2)·s, height = size; advance += (left + width + right)·s; no
  kerning; mode 16 (SRCALPHA/INVSRCALPHA, alpha test ≥ 5, no Z).
- Message with the amount in the hand: `ToolTips::ForceToolTips(0xEEA, amount)` every turn of a locked selection
  (piles, fields, fish farms) and 12 turns afterwards; text 0xEEA from `Scripts\InfoScript2.txt` ("Cantidad:
  %3.0f"; the id is the order of the ADD_TEXT lines). `CameraHelp::DrawKeyOrMouse` 0x447EA0: next to the hand on
  screen, box of H/25 (text at 2/3), yellow text with two black copies at ±1 px; it moves to the other side of the hand
  past 2/3 of the screen. The user checked in the original that **there is no background** (the code's additive box is
  not seen) and that the amount **is shown while the hand holds it**, until it is released.
- `GatheringText::GetStringWidth` 0x831130 = Σ (left + width + right)·size/80.
- `DrawTextRaw` clips the height at 0x832DAB..0x832E20: above the top the glyph is cut (y = top, h −= cut), past the
  bottom h = bottom − y, nothing when h ≤ 0; the v range is cut in the same ratio of the unclipped size
  (0x832E07..0x832E20). HelpText passes its box's top and bottom (0x5CBE04..0x5CBE7F).
- The glyph material's mode 16 is made by `CachePage::Init` 0x830244 (+4 = 5).
- HelpText's fonts (ctor `fn_005CADC0`, table 0xECCD08): +0xC j0, +0x14 f1, +0x10 f3; f1 and f3 fall back to j0 when
  they are missing. The original makes them when HelpText is made, so before the first text is drawn; openblack
  preloads j0, f1 and f3 at the end of each land load (`Renderer::PreloadForLand`), so the first frame that draws
  text no longer creates their atlases. A texture is made without a `bgfx::frame()` of its own: its bytes are copied
  to bgfx (`bgfx::copy`), which needs no frames to stay alive (cine audit #9).
- openblack: `Graphics/GameFont` (R8 atlas with the same rasterisation), `Common/HelpText`, `Renderer::DrawHandToolTip`
  (view `ScreenOverlay`), `fs_text`. Hook `OPENBLACK_TEST_TOOLTIP=<n>`. The offset of the text relative to the hand
  (half a box) is an estimate.
  The other messages are missing (when hovering over piles and stores, "Pick up"...).

## Particles (PSys)

The PSys engine (file format, per-turn step, drawing, script effects, the rules ported for water and the town's
beliefs) is in [particles.md](particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules).

## Map mist (LH3DMist, `fn_007FA300`)

The drawing of the map's mist (`fn_007FA300`, billboards, order, fade and the other mists of `mists::Submit`)
is in [map-loading.md](map-loading.md#drawing-lh3dmist-fn_007fa300), together with its creation (CREATE_MIST).

## The temple interior

The original draws the citadel in case 1 of `GGame::Process3dEngine` (`DoCitadelDraw` 0x555410; the frame's order is in
[original-frame.md](original-frame.md#other-cases-temple-video-and-2d)). openblack: `Renderer::DrawTemplePass`
(`Graphics/RendererTemple.cpp`), raffclar's drawing re-applied on our renderer.

- **No sun and no glare inside (faithful).** `LH3DSky::g_b_we_are_inside_citadel`
  [0xFA2760] is 1 only around `GLandAlignement::DrawSky` (0x555439..0x55544D; the same in `Process3dEngine`).
  DrawSky → fn_0086A330 → fn_0086B010 still draws the dome and the moon, which do not read the flag, but the sun
  (fn_0086C140, called at 0x86B618) returns at once while it is set (0x86C141..0x86C148). The "sun drawn" flag
  [0xFA275C] is only set after the sun is drawn (0x86C20D), and the glare (fn_0086BB60, from `LH3DRender::FinishFrame`
  0x82F4C9) runs only if it is set (0x86BB78..0x86BB7F) and clears it (0x86BB92). So inside the citadel there is no
  glare at all, and its eased visibility [0xFA2778] keeps its value through the visit (the easing at 0x86BBA1 is
  skipped). `GScript::SetSunDraw` also writes [0xFA2760] (0x708EC1); what the script passes has not been read.
  openblack: the temple pass draws the dome and the moon, and neither the sun nor its glare, as the original.
- **The main room's reflection, under its floor and its pool (faithful, relative to the room).** `WorldRoom::Draw`
  0x79E280: with CitadelReflections [0xC381E0] (written at 0x8238E9) and CitadelLightmaps [0xC381E4] (0x82390B),
  fn_00795310(1, …) (0x79E2B5) draws the room mirrored, then its vt+4(1) and fn_007954A0(1), then the floor
  (fn_00795430(0xFF), 0x79E2D9). The mirror multiplies row 1 of the room object's matrix (+0x18: +0x20..+0x28, its
  local y axis) by −1 [0x8AB678] (0x795338..0x795364) and undoes it after the draw (0x7953A7..0x7953CE); the position
  is not touched. fn_007954A0 does the same around fn_0083D8D0 (0x7954C1..0x795525). The room is reflected in its own
  plane y = 0, through its position. openblack's rooms stand at the temple's position (0, 0, 0), turned only about y,
  so the world's y = 0 of `Camera::Reflect` (`DrawTempleReflection`) is that plane, and the pool's reflection
  (`DrawTemplePool`) shows that same mirrored room. Where the original puts the interior in the world has not been
  read ([script-camera.md](script-camera.md)); the plane relative to the room does not depend on it.
- `PictureRoomBase::Draw` 0x78F1F0 (the base of the save-game room) mirrors its room the same way when its +0x124 is
  set (0x78F21F, behind the same two flags) and then draws the floor at alpha 0xD0 (0x78F23A) instead of 0xFF.
  openblack mirrors only the main room ([Pending](#pending)).

## Test hooks

In [openblack-internals.md](openblack-internals.md#debug-environment-variables):

- `OPENBLACK_SEA_TRACE=1` (sea rows every 500 frames) and the test `test_sea_rows`.
- `OPENBLACK_DUMP_BLOCK_TEXTURE` (coast block texture) and the test `test_land_light` (light table).
- `test_haze_land_light` (haze, land light and stamps); `OPENBLACK_TEST_STORM_CLOUDS`, `OPENBLACK_TEST_SPELL`
  and `OPENBLACK_TEST_WEATHER` to see the stamps (storm shadow, lightning light and fireball).
- `OPENBLACK_CLOUD_SEED=<n>`, `OPENBLACK_TIME_OF_DAY=<h>` and `OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>` (sky and clouds).
- `OPENBLACK_TEST_FADE="r,g,b,seconds"` and `OPENBLACK_TEST_WIDESCREEN=1` (fade and bars).
- `OPENBLACK_TEST_TOOLTIP=<n>` (hand message).
- Projected shadows: `OPENBLACK_SHADOW_TRACE=1` (once per second, each `ShadowInfo` with its light, alpha, fade,
  box, t' and maximum n, and the objects that receive a shadow with the view in which it is drawn) and
  `OPENBLACK_DUMP_SHADOWS=<folder>` (each texture ×8 as PNG every 300 frames); the test `test_shadow_math`.

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x5E5510` | GLandscape::Open entry; it calls GLandAlignement::Open -> CloudInSky::Open, a new sky of clouds per land | `Clouds::OnLandscapeOpened (Locator.cpp)` |
| `si+0x1C..0x28` | projected shadow's box: x0, z0, x1, z1 (uploaded as x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)) | `u_shadowBox (vs_land_shadow.sc), u_dynamicShadowBox (fs_object_shadow.sc)` |
| `si+0x440` | projected shadow's least k = x d.x + z d.z; a vertex below it gets the clip code 0x400 | `u_shadowCull.z / u_dynamicShadowCull.z, ShadowKept (shadow.sh)` |
| `0x7FEB98..0x7FEBBE` | fn_007FEB30 (object haze): t = (min(max(z, near), far) - near) / (far - near) | `haze.sh HazeT` |
| `0x874C5B..0x874C9C` | fn_00874AA0 (land haze, class 1): the same t from the clamped view depth | `haze.sh HazeT, vs_terrain.sc` |
| `0x874C90` | fn_00874AA0: f = 256 - __ftol((256 - k) t) (object side 0x7FEBC3) | `haze.sh HazeFactor` |
| `0x7FEBED..0x7FEC30` | fn_007FEB30: diffuse (c f) >> 8 per byte, only when f < 256 (land side 0x874DA1..0x874DF0) | `haze.sh ApplyHazeDiffuse` |
| `0x7FEC32..0x7FEC7E, 0x874CD2..0x874D06` | object / land haze colour c t, each channel with fistp (round to nearest, halves to even) | `haze.sh HazeColour, RoundHalfEven` |
| `0x874D21..0x874D9E (and 0x874D21..0x874DF0)` | fn_00874AA0: specular + haze colour saturated at 0xFF, then the diffuse (c f) >> 8 | `haze.sh HazeAddSaturated, vs_terrain.sc haze block` |
| `0x8784A7..0x8784EC (0x8784A7..0x8784B8 for t')` | fn_00878350: land shadow uv u = (Lx + (x - Lx) t' - x0) / (x1 - x0), v in z | `shadow.sh LandShadowUv, vs_land_shadow.sc u_shadowLight` |
| `0x878502` | fn_00878350: per-vertex clip code 0x400 on the land (k below si+0x440) | `shadow.sh ShadowKept` |
| `0x87846D..0x87848A` | fn_00878350: shadow vertex diffuse 0 at altitude 1 or less | `vs_land_shadow.sc a_color3 / v_shoreFade` |
| `0x874B89 / 0x874B7A` | fn_00874AA0: land vertex diffuse = table[cell byte 3], specular = the cell colour | `vs_terrain.sc LandCell / LandTable` |

## Pending

- Fade: the black of `OnNewGame` (openblack's intro does not reach its `SET_FADE_IN` yet) and `Temple::UpdateFade` in
  the citadel.
- [The temple interior](#the-temple-interior), what is missing:
  - The hand's placement inside the temple is not ported: openblack's hand there is placed by raffclar's code. In the
    original, `TempleRoom::Draw` 0x799FE0 casts the cursor into the room (`InnerCamera::RayCast` 0x795980, called at
    0x79A199) and on a hit writes the point to [0xE360C8..0xE360D0] and InnerCamera+0x440 to [0xE360B8..0xE360C0],
    then copies [0xE360C8] to [0xE360D8] (0x79A1EB..0x79A208) **(inferred: the hand's point)**; the hand's state
    there is `HandStateCitadel` (`Enter` 0x5B0C10, `Update` 0x5B0D00). Neither has been read.
  - The save-game room's reflection (`PictureRoomBase::Draw` 0x78F21F) and its floor at alpha 0xD0.
  - CitadelReflections and CitadelLightmaps: the original draws the main room's reflection only with both on;
    openblack always draws it.
  - The Creature Cave's mist domes are drawn through openblack's land mist, lit by the land under them
    **(approximate)**: how the original lights the cave's mists inside the citadel has not been read.
- Text: the other messages (when hovering over piles and stores, "Pick up"...); the offset of the text relative to the
  hand is an estimate.
- Rivers: the sound `ATMOS_TYPE_RUNNING_WATER` (not analysed, see [water.md](water.md#water-audio)).
- [ARGB4444 textures](#argb4444-textures), what is missing:
  - `ChallengeScroll.raw` (0x44, type 4 in-memory texture): how the game fills it in has not been read.
  - The L3D skins with flag 0x10000 (`L3DMeshFlags::Unknown17`) should be uploaded as X1R5G5B5 and not as BGRA4
    (`L3DMesh.cpp`). Only `Data\d_sky.l3d` carries it and openblack does not load it, so today it is not visible. The
    meshes in `AllMeshes.g3d` have not been checked (inferred).
  - The halving of `[0xEDD470]` with fmt ≠ 3 (step 6 and jump 0x300, 0x8374BB/0x8374C3): the top-left pixel of each
    2×2, at 128. Not ported.
  - The light maps of `Data\Spells\LightMaps` (`Particles/Creators/LightMap.cpp`) also go through `fn_0057DBE0`
    (inferred) and are not cut.
  - The save-game pictures: "the save-game pictures (0x7926DA, 0x79286F) are made by the game: neither has a stem
    here". This page says the save-game images are loaded from files with flags 1 (`fn_00784070` 0x78408F,
    `fn_00784640` 0x78466A); whether 0x7926DA / 0x79286F are a different pair of `Create` calls (with a computed 0x41)
    is not stated.
- Sea: the role of `fn_008792E0`, listed with `fn_00879500` / `fn_00879930` / `fn_0087A090` as "the CPU side of the
  original's sea", is not given anywhere.
- [Projected shadows](#projected-shadows-shadowinfo): the creature, the prediction, the SuperVillagers and the PSys meshes
  (`fn_006CA340`).
- Creature light of the projected shadows (`LightCreature`, not ported): only instruction ranges are known
  (0x815062..0x8151A5: horizontal distance, a test against the radius, against 0.1, a 45° test dy/horizontal < 1); the
  exact flow has not been written down.
- Original lighting: what is still missing is written elsewhere:
  - the creature's shadow: the [Projected shadows](#projected-shadows-shadowinfo) bullet just above;
  - the creature's reflection: [rendering-objects.md](rendering-objects.md#pending) ("Reflections and dynamic shadows
    of the creature and the SuperVillagers");
  - the particles: `UseDynamicLighting` of chains and meshes, in [particles.md](particles.md#pending).
