# Wind

Each climate has a wind, and storms add their own. The wind blows smoke and steam, carries storms across the island,
is heard at the camera, and moves things scripts mark as caught by it.

**Progress: 9/15 done, 3 partial — 70%**

## The wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A climate's wind lies between its season's extremes by how close it is to rain | done | `WeatherSystem::ProcessWind` |
| Storms add their wind within their radius | done | `ApplyStorm` in `WeatherSystem.cpp` |
| The wind at a point is part of the weather grid, and blended at the camera | done | `WeatherSystem::GetWeather`, `GetWeatherSmooth` |
| Storms drift with the wind | done | `WeatherSystem::ProcessClimate` |
| The ambient wind of the world is always calm in play | done | port notes: the sea's wind drift is left out for this reason |

## What the wind moves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smoke and steam from fires drift with the wind | done | `FireSystem.cpp` (smooth wind) |
| Particles of spells and effects drift with the wind | done | `GameParticleWorld::WindAt` (`ParticleSystem.cpp`) |
| Chimney smoke drifts, and swirls in the hand's wake | partial | `ChimneySmokeSystem.cpp` uses the hand's wind; whether it also takes the weather's wind is unconfirmed |
| Trees and ripe crops sway | partial | `VegetationSystem.cpp` sways on a fixed axis at random speeds; whether the game sways them by the wind's strength is unconfirmed |
| Clouds drift along their track | done | see ../sky/clouds.md |
| Scripts mark an object as blown by the wind | todo | `CHLApi.cpp` SET_AFFECTED_BY_WIND is a stub |
| Scripts ask whether a wind miracle is at a point | todo | `CHLApi.cpp` IS_WIND_MAGIC_AT_POS is a stub |
| Gusts of wind push particles in some effects | partial | the gusty-wind rule is in `src/Particles/ParticleUpdateRules.cpp`; not checked against the game here, see ../rendering/ |
| Scripts set and read a player's resistance to wind | todo | `CHLApi.cpp` SET_/GET_PLAYER_WIND_RESISTANCE; see ../miracles/ |

## Hearing the wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wind is heard at the camera, louder the stronger it blows above a threshold | done | `src/Audio/SoundMap.cpp` (wind volume) |
