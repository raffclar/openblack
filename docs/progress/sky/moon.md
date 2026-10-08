# The moon

The moon is a small half sphere that keeps its place beside the camera, 4000 units along plus x (east). It swings up
and down on an ellipse with the script hour and is turned to face the camera. Its face is lit by the real moon's phase,
taken from the computer's date, and it is tinted the palette's moon colour for the alignment. A soft glow from the
atmosphere texture is added round it, and a mirrored copy is drawn for the sea.

**Progress: 33/37 done, 2 partial — 92%**

## Where it stands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is placed relative to the camera, not the world, so it never gets nearer as the camera moves | done | `Renderer::DrawMoon` (camera origin + offset) |
| The offset is (4000, 1100 × cos(a) − 150, 800 × sin(a)), with a = script hour × π/12 | done | `moon::Place` (`src/Graphics/Moon.cpp`); the values are confirmed in Ghidra; test `Moon.ShowsAroundMidnight` |
| So it is highest (950 up) at midnight and lowest (1250 down) at noon, swinging north and south by 800 as it goes | done | `moon::Place` |
| It follows the script hour, not the visual hour | done | `Renderer::DrawMoon` uses `GetScriptTime` |
| The offset is worked out when the weather is updated each frame | done | worked out per frame in `DrawMoon` |

## Showing and fading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its strength is floor(0.5 × height − 110), capped at 200 | done | `moon::Place`; test |
| It is drawn only while that is above 0: height over 220, about 19:20 to 04:40 script time | done | `moon::Place`; test |
| It is at its full 200 from height 620 up, about 21:00 to 03:00 | done | `moon::Place` |
| With the fog option on and an overcast, its strength is divided by (8 × overcast + 1), truncated | done | `sky_dome::ThroughOvercast` in `DrawMoon`; test `SkyDome.SunAndMoonDimThroughAnOvercast` |
| When the moon is down, neither it nor its glow is drawn | done | `DrawMoon` returns early |

## Its shape and facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh is the weather folder's moon model, a half sphere about 100 units in radius | done | `Data/WeatherSystem/moon.l3d` (`SkyInterface::k_MoonMeshId`) |
| Its axes are built square to the line from the camera and kept upright, then scaled by 4 | done | `moon::Basis`; test `Moon.FacesTheCamera` |
| The mesh leans back by 7.5 degrees (0.1309 radians) | done | `moon::Model` k_Tilt |
| It is turned about its upright axis by the phase plus half a turn | done | `moon::Model` |
| It is drawn at 0.65 of the basis, so about 270 units in radius at about 4100 units away | done | `moon::Model` k_MeshScale |

## Its face and phase

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The phase comes from the computer's clock, in whole days since 1 January 1970 | done | `moon::Phase`; test `Moon.PhaseFollowsTheRealMoon` |
| Days are counted from 10962 days after 1970 (6 January 2000, a new moon) | done | `moon::Phase` |
| Moon months are days × 0.0338631801 (one every 29.53 days); the phase is (1 − the fraction of a month) × 2π | done | `moon::Phase`; the formula is confirmed in Ghidra |
| The game reads the clock at most once every 2 seconds and keeps the phase between reads | done | openblack reads it every frame, which gives the same phase |
| The face's texture coordinates are projected from the mesh's points turned by the phase: u = (x·cos p − z·sin p) × 0.0025 + 0.25, v = y × 0.0025 + 0.25 | done | `shaders/vs_celestial.sc` (u_celestial.z) |
| So the face shows the top-left quarter of weather.raw, and the lit part swings across it with the phase | done | `raw/weather` |
| The disc's outline comes from weathera.raw, the alpha texture | done | `raw/weathera` (`SkyInterface::k_MoonAlphaTextureId`), `shaders/fs_celestial.sc` |
| There is no separate earthshine: the dark side is whatever the texture shows | done | the game draws nothing else for it (the texture's dark side has not been looked at) |
| Scripts ask how full the moon is: 1 − phase / π below π, (phase − π) / π from π, so 0 halfway through the moon month and 1 at either end | todo | `CHLApi.cpp` GET_MOON_PERCENTAGE is a stub that returns 0 |

## Colour

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is tinted the light palette's moon colour for the alignment the sky shows | done | `LandLightTable::GetMoonColour`; see lighting.md |
| The tint's alpha is the moon's strength | done | `DrawMoon` colour alpha |
| The moon's colour doesn't change through the night, only its strength | done | |

## The glow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A square glow is drawn first, centred on the moon, 500 units each way along the moon's axes (so 4000 units across once scaled by 4) | done | `moon::MakeGlow`; test `Moon.GlowIsASquareAboutTheMoon` |
| It shows the part of the atmosphere texture from 0.25 to 0.49375 both ways, with its alpha texture | done | `moon::MakeGlow` uvs; `raw/ATMOS`, `raw/ATMOSA` |
| Its colour is the moon colour with red a sixth, green a fifth and blue a quarter, at the moon's strength | done | `moon::GlowColour`; confirmed in Ghidra |
| It is added to the sky | done | `DrawMoon` additive state |

## Draw order, depth and the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is drawn in the sky after the dome and the sun, before the land and everything else | done | `RenderPass::Sky` (f0f3549b) |
| The land drawn later covers it | done | openblack also writes the moon's depth so nearer land covers it; how the game handles its depth is not confirmed, but it looks the same |
| A second moon is drawn for the sea's sky: the same moon with its height below the camera flipped and its lean reversed, seen through the sea | partial | openblack draws the moon in the reflection pass from a camera mirrored through sea level, not through the camera's own height; whether the game's mirrored copy draws the body or only the glow is not confirmed |
| Moonlight: at night the land and models take the palette's moon colour | done | `src/3D/LandLightTable.cpp`; see lighting.md |
| The creature can choose to look at the moon, only at night | partial | the plan action exists (`src/Creature/CreaturePlanActions.cpp` LookAtMoon, as a look-about); the night check is not confirmed; see ../creature/ |
| Near a full moon, at night, the game's guidance shows a help sprite about the moon now and then | todo | see ../interface/ (help system) |
