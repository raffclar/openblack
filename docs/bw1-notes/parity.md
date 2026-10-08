# Graphics engine parity: original versus openblack

Complete map of the original frame (with addresses): [original-frame.md](original-frame.md). Details of
what is already done: [rendering.md](rendering.md) (the world), [rendering-objects.md](rendering-objects.md) (the models)
and [water.md](water.md) (the water); each stage links to its section.

Status: **same** (verified), **approx.** (works but differs), **missing**.

| Stage | Original | openblack | Status |
|---|---|---|---|
| Clear | Colour is not cleared; the sky covers the screen | Clears to 0x274659 | approx. (not visible) |
| [Camera](rendering.md#camera) | Horizontal FOV 70°, no far plane, near 0.3–3.5 depending on height | FOV 70°, near 0.3 + 0.16·height (0.3–3.5), far 65536 | same |
| [Sky](rendering.md#sky-sun-moon-and-clouds-original) | 9 images (alignment × time of day), 2 passes, darkens with storms, white with lightning; time thresholds 3.5/7.5/8/8.5 | Type and alignment, the original's thresholds | approx. (no weather) |
| [Sun and moon](rendering.md#sky-sun-moon-and-clouds-original) | `sun.l3d` (6–18 h) and its glow; `moon.l3d` with phase from the real clock and an additive halo; no stars | Same (glow occluded by the terrain with CPU rays); the moon reflected in the sea (halo + mirrored mesh) | approx. |
| [Clouds and cloud shadows](rendering.md#sky-sun-moon-and-clouds-original) | 70 clouds (`mist.l3d` + `smoke.raw`) moving with the wind, colour by alignment; shadows from `sclouds.raw` in the cell brightness | Same | same |
| [Reflected land](rendering.md#sea-skyraw--skyaraw) | Land only, no Z, half light, no small bump | Same | same |
| [Object reflections in the sea](rendering-objects.md#object-reflections-and-hand-shadow-on-objects) | Hand (grey 0xA0A0A0 unlit) and what it holds, physics objects, the creature's body (y < 6, 0x65A0A0D0) and boats, mirrored and clipped above the water, with the colour left on them by `fn_00801C90` | The hand, what it holds, physics objects and boats (`DrawBoatReflection`); the creature is missing (it does not exist yet) | approx. |
| [Underwater](rendering-objects.md#cutting-by-the-water-plane-drawcutbyplane) | Fish-farm fish (15 `misc0.raw` sprites per farm), before the sea; sharks and swimming SuperVillagers cut by y = 0 (`DrawCutByPlane`, 0xFF303070) | The fish shoals, the puzzle net and the sharks (`DrawCutByPlane`), inside the reflection target; there are no SuperVillagers | approx. |
| [Sea](rendering.md#sea-skyraw--skyaraw) | Mode 5 (ARGB4444), period 560 (level 4), wind (0 in game), 2 px rows from the edge of the 30000 quad, rippling, alpha 255→80 and top row at 0x20, level 0 with a ±70000 quad; no fog; the hand's night glow underneath | Same (rows redone per pixel in `fs_water`) | same |
| [Light table](rendering.md#terrain-light-table-0xedd90c) | `palette.raw` by time of day and alignment, overcast cap and lightning flash | Same (`LandLightTable::Build` with the overcast and the flash at the camera) | same |
| [Land](rendering.md#terrain-detail-small-bump) | Blocks front to back, mode 14, per-vertex light table, small bump | Same | same |
| [Distance fog](rendering.md#distance-haze-original-fog-detail-levels-36) | In software: land per vertex, models once per object; not on sea, sky or PSys particles | Same (with the storm and lightning haze) | same |
| [Static shadows](rendering.md#shadows-three-systems-in-the-original) | Baked into the block textures: Fixed/MobileObject/trees, shear x += h, z += h, ×0.5 | `StaticShadow` pass of the whole island every frame (256 px/block), ×0.5 in `fs_terrain` | same (without the original's 4×2 AA) |
| [Projected shadows](rendering.md#projected-shadows-shadowinfo) | `ShadowInfo` list: 32×32 silhouettes rasterised on the CPU (alpha n/15, empty ring, fade at 50–80 radii with the 9-block test, baked to nibbles) of the hand, the creature, thrown objects, the boat (with the sun), the SuperVillagers and the PSys meshes; onto every land block they touch, and the hand, the creature and the boat also onto objects (mode 6, ZFUNC EQUAL, vertical projection), at the end of each receiver's Draw | Same (`graphics::shadow_list`, `RendererShadows.cpp`: one block redraw per shadow; onto the objects behind each one, in Main or in their slot in the queue). The hand as in the original (compared against the user's captures of the original, 2026-10-02); the creature, the prediction, the SuperVillagers and the PSys meshes are missing | approx. |
| [Villager and animal blobs](rendering-objects.md#villager-blobs-object-reflections-and-lod) | Two quads from the feet (bones 21 and 18) towards +X+Z (2·scale, on the terrain plane), width 0.4, alpha 1 → 0; animals: 4 quads from the EBone points of their mesh | Same (villagers and land animals) | same |
| Footprints | Burned into the block textures | Footprint pass | approx. |
| [Model lighting](rendering-objects.md#model-lighting) | Land under the object (bilinear), integer rule `fn_0084BA90` (`I = fistp(255·N·L)` with ties to even, `f = amb + ((255 − amb)·I >> 8)` with ambient 90, diffuse `(c·f) >> 8`), point light [0xEA9E90] brought into mesh space (fixed sun by day, next to the hand in full night), additive specular, hand ×1.5 | Same (`src/Graphics/ModelLight.h` and `assets/shaders/model_light.sh`) | same |
| [L3D materials](rendering-objects.md#l3d-material-blending) | Type = mode; chroma with alpha test (ref − 5) **and** blending | Same, with per-material culling (bit 0 of byte +5; D3DCULL_CCW = CCW in bgfx) and wrap/clamp (bit 2) | same |
| [Transparent ordering](rendering-objects.md#the-single-transparent-queue-lh3dzsorter) | Back-to-front Z-sorter (max. 2048, stable) | A single queue, `graphics::zsort` (stable insertion, cap 0x800, dist² key in `float`): models with their mesh's 0x200 flag, whole (`LH3DObject::AddDrawing` 0x815F0B; the rest immediately, 0x815F62), the single-use bubble, the hand, the elements of Sorted effects one by one and Queued ones whole, sprites, mists, clouds, smoke, rain per tile and the boat's sprites, in `MainBlended` (sequential); shadows on objects inside each receiver | same |
| [Model LOD](rendering-objects.md#villager-blobs-object-reflections-and-lod) | In this executable the loading of `LevelOfDetail` is disabled (NOP at 0x823810 / 0x823B43): always LOD 1, no fading or disappearing | Always LOD 1 | same |
| [Windows](day-night-weather.md) | `Abode::Draw` 0x515F70: unlit `isWindow` submeshes, grey 224..252 that flickers, with someone at home and at night | `night_lights::WindowColour` (+0x54 0xFFgggggg or 0, `PackInstanceWindow`); "someone at home" = `PresentAtHome` (+0xB6), raised and lowered by the villagers, then `IsVisualNight` ([day-night-weather.md](day-night-weather.md)) | approx. |
| Hand | Always Z-sorter (`CHand::AddDrawing` 0x46D203), light ×1.5, wrist with alpha | Same: whole in the queue | same |
| [Chimney smoke](rendering-objects.md#chimney-smoke-lh3dsmoke) | `LH3DSmoke` (10 `smoke.raw` sprites, mode 6) at the chimney (extra point [1] of meshes with flag 0x400) while there are villagers inside or the workshop is producing; the hand pushes it | Same (`RendererSmoke.cpp`); `PresentAtHome` is raised by the villagers; age not truncated; the workshops' scaffold count is missing | approx. |
| [Water rings](water.md#water-rings) | `fn_005E5100`: horizontal `smoke.raw`, mode 13, 700 ms; from the hand's splash, objects falling into the water, swimmers, sharks and bait | Same (hand, thrown objects, sharks, waterfall and the fish puzzle; there are no swimmers) | same |
| [Boats](water.md#the-missionaries-boat-petitnavire) | `PetitNavire` (`PLAY_JC_SPECIAL(6)`): launch via `boat1.anm` with sailors, dust, splashes and 3 sounds; 60 s voyage via `boat2.anm` with deck and wake; reflection 0xFF303070 | Same (`ecs/MissionaryBoat`, `DisappearSmoke`; the sprites, one by one in the common queue); the hull shadow with the fixed sun, also onto objects | approx. |
| [Particles, spells, fireflies, sparkles](particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules) | PSys, everything in the Z-sorter | Generic PSys engine (`src/Particles`): the spell files with the ~25 most used classes, sprites sorted per effect; script effects (`SPECIAL_EFFECT_*`); fireflies; mesh atoms, mists, chains and light maps ([rendering-objects.md](rendering-objects.md)). Missing: the rules of specific spells and moving the hand's effects to this engine | approx. |
| [Rain, snow, lightning](day-night-weather.md#the-drawn-rain-raincpp-graphicsrendererraincpp) | By storm, in 80×80 tiles | The rain per tile in the common queue (`Rain.cpp`, `RendererRain.cpp`), the lightning flash and the storm clouds; no drawn snow, no drops on the ground | approx. |
| Leashes, gestures | Modes 15 and 13 | — | missing |
| [Influence border](magic.md#influence-srcecsinfluence) | `InfluenceCircle::Draw(1)` 0x826C90 every frame in the world view: a 40-high curtain per citadel and town, `burn.raw`, mode 6, scrolled, at once before the Z-sorter drain; rebuilt every 10 turns; same-player overlaps pruned; hand-crossing ripple (7 `smoke.raw` rings, Z-sorted) | Same (`ECS/Influence/InfluenceCircles.cpp`, `RendererInfluence.cpp`); the border latch is set when the player has a temple (no temple fade); no per-land colour remap | approx. |
| [Names, counters, help](rendering.md#text-the-originals-fonts-and-the-hand-message) | Z-sorter and end-of-frame callbacks; fonts `j0`/`f1`/`f3`; texts from `InfoScript2.txt` | Original `j0` font and the message for the amount in the hand; the other messages, names and counters are missing | approx. |
| [Sprites](rendering-objects.md#the-single-transparent-queue-lh3dzsorter) | `LH3DSprite` in the Z-sorter | In the main pass, in the common queue (`components::Sprite`, smoke, boat sprites one by one) | same |
| [Screen fade and bars](rendering.md#screen-fade-and-cinema-bars) | `SET_FADE`/`SET_FADE_IN` per turn; `SET_WIDESCREEN` bars over 2 s; quads in FinishFrame | Same (`ScreenOverlay` view); without the initial black of `OnNewGame` because the intro does not yet reach its `SET_FADE_IN` | approx. |
| [Bink video](video.md#openblack) | `DrawToScreen` 0x54DC6D → `thedraw` 0x844E30 → fn_00845740: one quad per 256x256 tile in mode 6, ZFUNC ALWAYS, no Z; no world while it covers the screen (0x54DD7D) | `Renderer::DrawVideoOverlay` in `ScreenOverlay` (one texture, the same texels per tile), the world is not drawn with `CoversScreen()`; still without decoder (black) or opcode 203 | in progress |
| Temple and citadel | Their own lights, Citadel* keys | `TempleInterior` | approx. |
| Gamma, post-processing, D3D fog | None | None | same |

Level of detail: the original's, 4 by default (`--detail-level`, `Graphics/DetailLevel.h`).

> **Code rules.** Each stage keeps its state in ECS components or Locator services, loads its assets through the
> resource caches and keeps its pure logic testable with fakes in `test/`; comments in the code describe behaviour
> in plain English, with no decompiled names or addresses (those belong in these pages).

## Pending

- The stages still missing or in progress in the table: the drawn snow and the raindrops on the ground, leashes and
  gestures, the Bink decoder (and opcode 203), the creature (reflections and shadows), the prediction, the
  SuperVillagers and the PSys meshes in the projected shadows.
- CRT draws that differ from the original, each a measured behaviour change still to make: the rain is placed at
  every `OnLoadMap` (896 draws) instead of 2 × 896 once at start-up; there is no snow scatter (7168 draws at
  start-up); the clouds make 5 draws instead of 6; the lantern jitter runs while it is light, the lanterns' creation
  draws come at the first `Rescan`, and every light is drawn again when the set changes. Details in
  [day-night-weather.md](day-night-weather.md#pending) and [rendering.md](rendering.md#pending).
