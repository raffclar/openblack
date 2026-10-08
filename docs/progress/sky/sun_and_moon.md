# Sun and moon

How the sun and the moon share the sky. The detail is in [sun.md](sun.md) and [moon.md](moon.md). This page covers what
they have in common: the hours that switch between them, and how they behave in the temple, while paused, in cut scenes
and with the detail options.

**Progress: 8/12 done, 3 partial — 79%**

## Day and night between them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Both follow the script hour, which the clock stretches so dusk and dawn fall at fixed hours | done | `DayNightClock::GetScriptTime`; see day_night_cycle.md |
| The sun shows from 3 to 21 and the moon from about 19:20 to 04:40, so both are up at dusk and dawn | done | `sun::Place`, `moon::Place` |
| Jumping or moving the clock by script moves them at once | done | `CHLApi.cpp` SET_GAME_TIME, MOVE_GAME_TIME |
| The sky's dome, the land's light and the haze change with the hour on their own; the sun and moon light nothing themselves | done | see sky_dome.md and lighting.md |

## In the temple, paused and in cut scenes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Inside the temple the sky is drawn without the sun and without its glare; the moon is still drawn | partial | openblack draws the sun in the temple's sky and its glare after the temple's solid parts (`Renderer.cpp`, inTemple) |
| While the game is paused the clock stops, so neither moves, and the glare holds | done | `TimeSystem`, `sun::EaseGlare` |
| The moon's phase follows the real date even while paused | done | `moon::Phase` |
| The moon stays beside the camera wherever it moves, cut scenes included | done | `Renderer::DrawMoon` |
| Cut scenes don't change how they are drawn | done | not confirmed beyond the code: no cut-scene switch was found in the game's sun or moon code |
| The script switch that hides the sun is cleared when the game restarts its scripts | todo | SET_SUN_DRAW is a stub; see sun.md |

## Options and openblack-only places

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The fog option decides whether an overcast dims the sun, its glare and the moon | partial | the sun and moon follow it (`sky_dome::ThroughOvercast`); the glare doesn't (see sun.md) |
| At detail levels 3, 4 and 6 the glare can also be hidden by models, through the depth buffer | partial | see sun.md |
| On openblack's flat test land the sun and moon behave as on any land | n/a | openblack-only (`src/3D/FlatLand.cpp`); see ../debug/ |
