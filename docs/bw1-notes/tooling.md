# Tools and formats

How the original is investigated (disassembly scripts over `runblack.exe`), openblack's command-line
tools and the game's data formats: G3D/L3D packs, effects, textures, scripts, `info.dat` and the LND with
the extensions of the BWLandEditor map editor.

- [Disassembly of runblack.exe](#disassembly-of-runblackexe)
- [openblack tools](#openblack-tools)
- [Data formats](#data-formats)
- [LND and BWLandEditor maps](#lnd-and-bwlandeditor-maps)
  - [How it reads the LND](#how-it-reads-the-lnd)
  - [Cell bytes](#cell-bytes)
  - [Low-resolution textures](#low-resolution-textures)
  - [What is adapted in openblack](#what-is-adapted-in-openblack)
  - [Test maps and analysis scripts](#test-maps-and-analysis-scripts)
  - [Differences not checked in the original](#differences-not-checked-in-the-original)
- [Pending](#pending)

## Disassembly of runblack.exe

Python scripts, kept outside the repository (`bwdis.py`, `callers.py`, `refs.py`, `scan.py`, `vtq.py`, `anim_vt.py`,
`vt.py` and `potinfo.py`):

- `python bwdis.py ADDR:SIZE [ADDR:SIZE...]` disassembles `runblack.exe` with capstone. It annotates symbols, floats from the
  data section (`; =0.67`) and strings.
  - Virtual calls `call [reg + off]` are annotated with the vtable of class `VTC` (environment variable, by
    default `Tree`). **The annotated name is only valid if `VTC` is the object's real class**; otherwise, ignore it.
- `python callers.py ADDR` lists who calls a function (searches for `call rel32`).
- `python refs.py ADDR...` lists instructions that reference an address (data or functions).
- `vt.py Clase [regex]` prints a class's vtable; `potinfo.py` reads the pot table from `info.dat`.

Symbols and tricks:

- The symbols come from `bw1-decomp\config\BW1W120\symbols.txt`. Some names are wrong, for example:
  `0x5B3C70` is `HandStateHolding::Update` and `0x5B5E70` is `ObtainRequiredHandPosition`.
- To read tables that are filled in at startup (in `.data` they are zero), look for the `crt_xc_fn_*` initialiser that
  writes to them (e.g. the interface action state table, 0x5D7960).

Reference values (goldens) for the tests that check the original bit for bit:

- Float routines: an independent float32 model of the x87 at 24-bit precision (PC24), written operand by operand from
  the disassembly. Each fadd / fsub / fsubr / fmul / fdiv / fsqrt is rounded to 24 bits. fpatan (like fsin / fcos) is
  not affected by the precision control, so it runs at 64 bits and is rounded only by the caller's `fstp dword`. The
  test then compares the bits. Modelled this way: the LH3D angles (0x7FA990, 0x7FAA50, 0x7FAAF0), PhysOb::SetUpPos
  0x7FC760 and AdjustToGroundLevel 0x7FCB80, HandAngularMomentum 0x5D4EB3.., DrawOrigin 0x6473B4.. and
  CapReleaseVelocity 0x5D4F5F..; with numpy float32, one rounding per operation: 0x7FAFF0, 0x7FAE60,
  LHMatrix::SetInverse 0x7FB290 and UpdateWorldToCamera 0x819690; the walking rows and InitStepsXZ 0x60BFA0 (tables
  0xC31614 / 0xC31E14 / 0xC2307C). game_random's values come from a float32 emulation of the same kind.
- Integer pixel routines: the test copies the original's instructions one by one, keeping register-sized variables,
  and compares the output on every input: 4444 texel 0x8374F0..0x837533 / 0x83767E..0x837687, 555 0x83776A..0x83778D,
  raw 565 0x8376E9..0x83770C, diffuse 0x80BFA3..0x80C00B, the 0x80808081 divides 0x7FA6DF / 0x5E272C, FragMesh face
  light 0x7F82A8..0x7F8363.
- Exe code run under Unicorn: the scripts are kept with each subject's documents (for example the QMixer emulation,
  see [audio.md](audio.md), and the one that runs the exe's `_qsort` 0x7C7E64, see [villagers.md](villagers.md)).

## openblack tools

Built in `cmake-build-presets\ninja-multi-vcpkg\bin\Release`:

> **Code rules.** Tools and tests read the game's files through the same loaders and resource caches as the game;
> unit tests live in `test/`, use fakes and never need the original data (a test that does skips itself when the
> data is absent); comments describe behaviour in plain English, with no decompiled names or addresses (those belong
> here). See [openblack-internals.md](openblack-internals.md).

- `packtool -M pack.g3d` lists meshes; `packtool -m N -e out.l3d pack.g3d` extracts mesh N;
  `packtool -T pack.g3d` lists textures (**ids in hexadecimal**); `packtool -t IDX -e out.dds` extracts the texture by
  index (the id is a different field).
- `l3dtool read -H|-m|-P|-V|-I|-s file.l3d`: header, submeshes, primitives (material, skinID), vertices (positions
  with **one decimal place**), indices, embedded skins.
- `lndtool write ... --points "x y z"`: generates the test terrain (see tests in openblack-internals.md).
- **Shaders with #include**: `bgfx_compile_shaders` only follows the top-level file. The `vs_object_*.sc` variants
  (and `vs_static_shadow_instanced_static.sc`) include `vs_object.sc`/another variant: after changing the included file you have to
  `touch` the variants, or the executable keeps the old version for static and instanced meshes.
- Replay and profile switches (openblack dev tools, not original; [engine-loop.md](engine-loop.md) §7):
  `OPENBLACK_STATE_HASH=<file>` writes a hash of the game state at the end of each turn;
  `OPENBLACK_FIXED_FRAME_MS=<ms>` makes every frame that many ms (game timer, engine timer, audio ticks, the frame's
  delta; not Lightning's 10 s wall-clock timeout, Particles/Rules/Lightning.cpp); `OPENBLACK_PROFILE=<s>` now ends
  each summary with the memory (RAM working set, peak and private bytes; bgfx textures, render targets, GPU,
  transient buffers).

## Data formats

- **Mesh pack** `Data\AllMeshes.g3d`: Lionhead pack with ~626 L3D meshes and their DDS textures. The mesh
  number is the index in the pack, which matches the enum in `Data\AllMeshes.h` **only for that pack** (Creature
  Isle and other packs have other indices).
- **L3D**: header of 19 u32 (magic, flags, size, submeshCount, submeshOffsets, bbox[8], another, skinCount,
  skinOffsets, extraCount, extraOffset, footprintOffset). Embedded skins start with a u32 id followed by
  256×256 16-bit pixels.
- **Effects**: `Data\Spells\ZSpellFiles\*.zzz` = zlib from byte 4 onwards; inside, a text properties file
  (`BEGINCLASS` / `PROPERTY`). Examples: `SF_GripLandscape`, `SF_MultiPickUpWood/Food/FoodFish`, `SF_MultiPutDown*`.
- **Loose textures** `Data\Textures\X.raw` (256×256 RGB) + `Xa.raw` (R8 alpha). Sprite sheets of 8×8 cells.
- **Map scripts** `Scripts\LandN.txt`; the executable's command table (0xC21190…) gives the name and parameter
  types (`A` position, `N` integer, `F` float). E.g.: `CREATE_MOBILE_STATIC` = `ANFFFFF` =
  (pos, type, altitude, X angle, Y angle, Z angle, scale).
- **`Scripts\info.dat`**: object tables (pots, trees, mobile statics...). openblack loads it into `InfoConstants`.

## LND and BWLandEditor maps

BWLandEditor is Daniels118's map editor (Java, GPL-3). Part of its code is ported from
openblack: InfoConstants, L3D/G3D and an old version of `fs_terrain`. What follows comes from the editor's code
(not from the original executable) except where stated otherwise.

### How it reads the LND

The same as openblack, plus three extensions that openblack already supports (`LNDFile`, `LandIsland`):

- At the end of the file there may be the blocks `EXT0` (u32 size of the whole block = 10, u8 version, u8 altitude
  bits 8-16) and `META` (u32 data size, editor data). With more than 8 bits, the high bits of the altitude
  go in the low bits of `saveColor` (`LNDCell::Altitude`, `LandIslandInterface::GetCellAltitude`).
- The grid can have up to 128×128 blocks and more than 255 blocks. The header table only covers 32×32 and
  indices < 256, so `LandIsland` builds its table from each block's `blockX`/`blockZ`. In the 21 original `.lnd`
  files the table matches those fields (`lnd_check.py`).
- The editor corrects a `mapX`/`mapZ` that does not agree with `blockX`/`blockZ`, and `LandIsland` does the same.

### Cell bytes

- **`flags`, according to the editor.** Bit 0 = "transparent"; bits 1-7 = ambient sound: 0 nothing, 2 splashing, 3 ocean,
  4 slow waves, 5 lake, 6 coast, 7 fast waves, 8 jungle, 10 wind, 12 desert, 14 birds, 16 forest, 18 river.
  The odd ones above 8 are variants of the preceding even one.
  The original reads the zone as `(flags >> 2) & 0xF` (`Terrain::GetAtmosType` 0x7352B0): the editor's codes are
  `tipo << 1`; table and usage in [objects-and-resources.md](objects-and-resources.md) («Ambience (atmos)»).
- **`properties` (+6).** Bits 0-3 = country, bit 4 (0x10) = hasWater, bit 5 (0x20) = coastLine, bit 6 (0x40) =
  fullWater, bit 7 (0x80) = split (diagonal, see [engine-math.md](engine-math.md#terrain-height)). To separate
  the open sea from inland water, `lnd_water.py` groups the connected cells with water or without a block: those touching
  the map edge or the void are sea; the rest, lakes or ponds.

### Low-resolution textures

Atlas of 4×4 subtextures of 64×64, one per block, 4 texels per cell, with X and Y swapped. The "unknown" in its
header is the number of blocks in the atlas. `iu_lrs`/`iv_lrs` are integers (0/64/128/192). openblack does not use them (it only
reads them in `LNDFile`).

### What is adapted in openblack

**Our own** (the original maps do not change):

- With more than 8 bits the heightmap goes from R8 to R32F on the same scale (1 = altitude 255). Above 255 the
  country's last material is used, as the editor does.
- The per-island textures (footprints, static shadows, alpha) drop below 256 texels per block as soon as they would exceed 8192.
- The disc that limits the camera (centre 2560, radius 5120) grows with the size of the map.

### Test maps and analysis scripts

`lnd_make_tests.py` generates three maps and their scripts (start with `-s` and the absolute path of the `.txt`):

- `Land1_ext`: Land1 with the editor blocks at the end; identical to Land1, height at (1788.4, 2710) = 28.9173050.
- `Land1_hi`: 10 bits and doubled altitudes; height 57.8346100.
- `Land5_x2`: Land5 twice, grid of 60, 374 blocks.

`.lnd` analysis scripts: `lnd_check` (block table versus blockX/blockZ),
`lnd_make_tests` (test maps), `lnd_beaches` (sand next to water, materials 6 and 11), `lnd_zones` /
`lnd_countries` / `lnd_find_country <lnd> <n>` (sound zones, countries and median position of a country),
`lnd_materials` / `lnd_colours` / `lnd_tile_check` / `lnd_decal_metric` (256×256 RGB555 materials),
`lnd_water` (bodies of water), `lnd_hash` (FNV-1a of the materials).

### Differences not checked in the original

- The editor chooses the material with `min(altitud + ruido/4, 255)`; the original does it **per texel** with
  `min((h >> 8) + ruido, 255)` and the altitude weighted with cones (resolved, [rendering.md](rendering.md#coast)), and openblack
  already follows it (`3D/BlockTexture`).
- openblack's L3D reader takes the footprint width and height from the header; the editor reads them per entry.
- The editor reads Creature Isle's info.dat (627250 bytes; longer tables in InfoConstants.java L23-33); openblack
  does not yet.

## Pending

- **Converting the original assets to open formats.** Which assets and which target formats is not written down
  yet; to be decided.
- **Real-ESRGAN on the NPU** (possible, with doubts: many PCs have no NPU). Use the NPU (Intel AI Boost, through
  OpenVINO) to upscale textures ×4 with Real-ESRGAN faster and without loading the CPU. A tool only, it does not
  change the game; for when openblack is complete.
- `test_help_dude_file.cpp` (file scope), unverified: "the clip samplers (src/Help/SpiritAnimClip.h, was CAnim.h) against values of
  fn_00860E00 / fn_00861EE0 / 0x839F10 run from runblack.exe under an x86 emulator." The note does not name the
  script. A grip evaluation script (`grip_eval.py`) mentions those addresses, but it was not checked that this is
  the script.
