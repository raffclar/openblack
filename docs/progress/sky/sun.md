# The sun

The sun is a single textured square about 9900 units across, placed 30000 units out along both minus x and minus z
(the north west) and turned to face the island. It rises and sets with the script hour and is drawn into the sky right
after the dome. At the end of the frame a larger orange glare is drawn over everything. The glare dims when land, the
sea's horizon or (at some detail levels) objects hide the sun.

**Progress: 36/43 done, 5 partial — 90%**

## Where it stands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sun's x and z never change: 30000 units out along minus x and minus z | done | `sun::Place` (`src/Graphics/Sun.cpp`); Ghidra confirms the start-up values |
| It follows the script hour (the stretched hours scripts see), not the visual hour | done | `Renderer::DrawSun` uses `GetScriptTime`; test `Sun.RisesAndSetsWithTheScriptTime` |
| Its height is 7500 × (h − 6) / 6, where h is the hour held between 6 and 18 and mirrored about noon: 0 at 6 and 18, 7500 at noon | done | `sun::Place`; test |
| Before 6 and after 18 it stays at height 0, on the horizon, while it fades | done | `sun::Place` |
| It is turned three eighths of a turn about the vertical so it faces the island | done | `SunModel` in `src/Graphics/Renderer.cpp` (rotation of −3π/4; the game turns its heading by 3π/4) |
| It is drawn at the mesh's own size (scale 1) | done | `SunModel` |
| It does not move round the sky: it rises and sets in the same place | done | as the game |

## Fading in and out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fully shown (255) from 6 to 18 | done | `sun::Place`; test |
| It fades in from 3 to 6 at 85 a script hour ((h − 3) × 85) | done | `sun::Place`; test |
| It fades out from 18 to 21 at 85 a script hour (255 − (h − 18) × 85) | done | `sun::Place`; test |
| Before 3 and after 21 it is not drawn at all, and nor is its glare | done | `sun::Place` returns nothing; the game skips the draw when the strength is 0 |
| With the fog option on and an overcast at the camera, its strength is divided by (8 × overcast + 1), overcast capped at 1, truncated to a whole number | done | `sky_dome::ThroughOvercast` (`src/3D/SkyDome.cpp`); test `SkyDome.SunAndMoonDimThroughAnOvercast` |
| Without the fog option an overcast does not dim it | done | `sky_dome::ThroughOvercast` |

## How it looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh is the weather folder's sun model: one square about 9928 units across, facing along its own z | done | `Data/WeatherSystem/sun.l3d`, loaded in `Game.cpp`; the file's missing skin table is handled (CHANGELOG) |
| Its texture is a 256 by 256 picture (sun.raw); the square shows the part from 0.5 to 0.75 across and from 0.25 down | done | `raw/sun` (`SkyInterface::k_SunTextureId`); texture coordinates come from the mesh |
| The texture is cut to 5 bits a channel as the game stores it | done | RGB555 cut for sun.raw (CHANGELOG) |
| It is tinted a warm 0x957C63, with the strength as its alpha | done | `Renderer::DrawSun` colour; the colour value is confirmed in Ghidra |
| It is added to the sky behind it, using its alpha | partial | `Renderer::DrawSun` uses additive blending; the blend mode comes from the model's material in the game, so it is not confirmed |
| Its colour does not change with the time of day, the alignment or the climate, only its strength | done | the colour is fixed in the game |
| There are no lens flares or rings: only the disc and its glare | done | the game draws nothing else for the sun |

## Draw order and depth

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is drawn in the sky, after the dome and before the moon, before the land, the sea and everything else | done | `RenderPass::Sky` (f0f3549b): dome, `DrawSun`, `DrawMoon` |
| It leaves no depth, so the land drawn later covers it | done | `Renderer::DrawSun` (no depth write) |
| The game draws no sun in the sea; only a mirrored moon goes into the sea's sky | partial | openblack draws the sun again in `RenderPass::ReflectionSky`, so it shows in the sea |
| Scripts can stop it being drawn: SET_SUN_DRAW with true hides the sun and its glare until it is set false again or the game restarts its script | todo | `CHLApi.cpp` SET_SUN_DRAW is a stub. In the game it sets the same switch the temple uses |

## The glare

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The glare is the sun again, 1.8 times as large, drawn in orange 0xA06A35 | done | `Renderer::DrawSunGlare`; `sun::k_GlareScale` |
| It is drawn only in frames where the sun itself was drawn | done | `DrawSunGlare` uses the same placement |
| It is drawn last in the 3D frame, over everything, with no depth test (the game sets the depth test to always pass) | done | `Renderer.cpp` (after the translucent pass, no depth test outside the temple) |
| Its alpha is the glare strength × the sun's strength / 255 | done | `Renderer::DrawSunGlare` |
| With the fog option its alpha is also divided by (8 × overcast + 1) | todo | openblack does not dim the glare by the overcast |
| It eases towards its target by 0.01 of the gap for every millisecond of game time, kept between 0 and 255 | done | `sun::EaseGlare`; test `Sun.GlareEasesTowardsWhatShows` |
| While the game is paused it holds where it is | done | `sun::EaseGlare` takes the frame's game time, which is 0 while paused |
| The target is 255 × (1 − 0.2 × the number of hidden samples) | done | `sun::EaseGlare` |

## What hides the glare

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five points are tested: the sun's centre and four points 500 units off it along the world's x and y | done | `sun::GlareSamples` (along the world's x and y); test `test_picking` |
| Each point is first pushed further along the line from the camera by the near distance | done | By the camera's near distance (`sun::GlareSamples`) |
| No point is tested lower than height 10 | done | `sun::k_GlareLowestSample` |
| A point counts as hidden when the land lies between the camera and it, tested along the line by the land's cells | done | The game's land line test (`PickingSystem::LandOrSeaAlong`, `Renderer::DrawSunGlare`) |
| A point also counts as hidden when the line from the camera dips below sea level within 7500 units of the camera, so the sea's horizon hides the setting sun | done | The land line's sea fallback within 7500 (`PickingSystem::LandOrSeaAlong`) |
| At detail levels 3, 4 and 6, with the depth-reading option on, a point also counts as hidden when the depth buffer at its spot on the screen shows something nearer, so models hide it | partial | openblack's physics ray also hits objects, at every detail level, and it doesn't read the depth buffer |
| Looking straight at the sun with nothing in the way gives the full glare | done | |

## The sun's light

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models are lit from a fixed sun far to the north west and up | partial | `model_light::k_Sun` (−500000, 500000, −500000) in `src/Graphics/ModelLight.h`; not confirmed whether the game's light turns with the hour |
| Object shadows fall away from that fixed sun | done | `src/Graphics/ObjectShadows.h` k_Sun; static shadows are still todo (see ../terrain/landscape_rendering.md) |
| The land's and models' colour through the day comes from the light palette, not from the sun's height | done | see lighting.md |
| The creature can choose to look at the sun, which it may do only when it is not night | partial | the plan action exists (`src/Creature/CreaturePlanActions.cpp` LookAtSun, as a look-about); the night check is not confirmed; see ../creature/ |
