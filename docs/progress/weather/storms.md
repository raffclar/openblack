# Storms and thunder

Natural storms are moving weather systems bred by the climates. Within their outer radius they pull the temperature
towards their own and add rain, snow, cloud and wind, at full strength inside their inner radius. Hot or overcast
storms flash with lightning and roll with thunder.

**Progress: 21/26 done, 0 partial — 81%**

## A storm's life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A storm fades in, lasts a while at full strength and fades out | done | `WeatherSystem::UpdateStorms` |
| A storm travels towards its destination at its speed and drifts with the wind | done | `WeatherSystem::UpdateStorms`, `ProcessClimate` |
| A dead storm lingers a couple of turns before it is removed | done | `Storm::deadTurns` |
| Later storms are laid on top of earlier ones | done | `Storm::serial` |
| A storm's strength, radii and cloud height come from its climate's type | done | `WeatherSystem::CreateStorm` |
| Cold storms snow; the snow share falls away above freezing | done | `WeatherSystem::CreateStorm` |
| Storms are saved and loaded with the game | todo | See ../engine/ |

## The weather at a point

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather is held on a grid of 128 by 128 cells of 40 units, calm air plus every storm over the cell | done | `WeatherSystem::ComputeCell` |
| Cells are worked out when asked and kept until the storms move | done | `WeatherSystem::GetCell` |
| Weather heard and seen at the camera is blended between the four nearest cells | done | `WeatherSystem::GetWeatherSmooth` |
| High above the ground the weather gives way to calm air | done | `WeatherSystem::GetWeatherSmooth` |
| The cloud cover at a point darkens the land and sky | done | `WeatherSystem::GetOvercast`; see ../sky/lighting.md |
| The snow lying in a cell is part of its weather | done | `WeatherSystem::ComputeCell` |

## Lightning and thunder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once faded in, hot or overcast storms flash now and then, at waits drawn from their climate | done | `WeatherSystem::UpdateStorms` (6c9dff13) |
| Bright flashes come with thunder, dim ones with a forked bolt | done | `src/3D/Lightning.cpp`; `test/test_lightning.cpp` |
| A flash lasts a moment, flickering down between a tenth and half a second | done | `lightning::Brightness`; test |
| The flash lights the land and sky while the camera is inside the storm | done | `WeatherSystem::GetLightningFlash`; see ../sky/sky_dome.md |
| Lightning glows on the ground where it struck | done | 1bba0e52; `src/3D/LandColourStamps.cpp` |
| The thunder is one of eleven claps, heard once the sound has travelled to the camera | done | cc9563f3; `WeatherSystem::UpdateStorms` |
| The forked bolt is drawn from the cloud to the ground | todo | port notes: the forked bolt waits for particles |
| A bolt that reaches the ground strikes what is there (unconfirmed for natural storms) | todo | (unconfirmed) |
| Rain and wind sound under a storm | done | `src/Audio/SoundMap.cpp`; see ../audio/ |

## Storm clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each storm draws up to 16 puffs of cloud wandering about its middle | todo | see ../sky/clouds.md |
| A storm's clouds shadow the land | todo | see ../sky/clouds.md |

## Ending storms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts end every storm within an area at once | done | `CHLApi.cpp` KILL_STORMS_IN_AREA, `WeatherSystem::KillStormsInArea` |
| The storm miracle lays its own storm over the land, moved and ended by the miracle | done | `WeatherSystem::AddMiracleStorm`; see ../miracles/storm.md |
| A storm can be forced over the whole island, and storms ended, from the debug window | n/a | openblack-only, `src/Debug/Weather.cpp`; see ../debug/ |
