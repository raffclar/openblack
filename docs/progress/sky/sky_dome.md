# The sky dome

The sky is a dome of pictures: for each alignment (good, neutral, evil) one by day, one at dusk and one at night,
blended for the time of day and mixed by the alignment the sky shows, then tinted by the weather.

**Progress: 14/16 done, 2 partial — 94%**

## Time of day

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky type runs from 0 at night through 1 at dusk to 2 by day | done | `DayNightClock` sky type (a0ce4593) |
| The dome blends day into dusk, or dusk into night, by the sky type | done | `src/3D/SkyDome.cpp`; `test/test_sky_dome.cpp` |
| The blend is rebuilt only once the sky has moved on far enough (a hysteresis of 0.03) | done | 2fc53b3d; `src/3D/SkyDome.cpp` |
| It is then rebuilt a band of 32 rows a frame, top down | done | `src/3D/SkyDome.cpp`; test |
| After the clock jumps the whole dome is rebuilt at once | done | `SkyDome` (SET_GAME_TIME) |
| Texels blend in whole steps of the pictures' 5-bit channels | done | `src/3D/SkyDome.cpp`; test |
| The blend is done on the GPU rather than on the CPU as the game does | partial | `shaders/fs_sky_dome.sc`; same picture, different method |

## Alignment

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each alignment has its own dome; the sky mixes the two nearest by the alignment it shows | done | `src/3D/SkyDome.cpp` |
| The alignment's opacity is cut to 255ths | done | 2fc53b3d |
| The sky turns towards the alignment at the camera over time, none while paused | done | `AlignmentSystem` (`src/ECS/Systems/AlignmentSystemInterface.h`) |
| An evil sky is darkened, up to 90 of 255, with the weather detail option on | done | `sky_dome` evil darkness (cfb5f43d) |
| Inside the temple the sky dome is still drawn (seen through its openings), without the sun | partial | openblack draws the dome in the temple but also the sun; see sun_and_moon.md and ../temple/ |

## Weather on the sky

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An overcast turns the dome towards the haze's colour, with the fog detail option on | done | cfb5f43d; `src/3D/SkyDome.cpp` |
| A flash of lightning turns the dome towards white | done | cfb5f43d |
| The sky is drawn first, before the scene, and in the sea's reflection | done | `RenderPass::Sky`, `RenderPass::ReflectionSky` (f0f3549b) |
| Stars are part of the night pictures, with no separate star field | done | the night domes carry them (unconfirmed that the game adds none of its own) |
