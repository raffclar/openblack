# Scripted and real-world weather

Weather the story places on purpose: weather objects a challenge script creates and steers, storms a land's script
lays, and the real weather of the player's home town, fetched over the internet.

**Progress: 1/15 done, 0 partial — 7%**

## Weather objects from challenge scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge script creates a weather object at a position, with a size | todo | the script object type exists (`src/ScriptHeaders/ScriptEnums.h`) but creating one does nothing |
| The script sets its temperature, rainfall, snowfall, overcast and how fast the rain falls | todo | `CHLApi.cpp` CHANGE_WEATHER_PROPERTIES is a stub |
| The script sets how often it flashes with sheet and forked lightning | todo | `CHLApi.cpp` CHANGE_LIGHTNING_PROPERTIES is a stub |
| The script sets how long it lasts and how long it fades | todo | `CHLApi.cpp` CHANGE_TIME_FADE_PROPERTIES is a stub |
| The script sets its number of clouds, their blackness and height | todo | `CHLApi.cpp` CHANGE_CLOUD_PROPERTIES is a stub |
| The script sends it towards a point at a speed | todo | `CHLApi.cpp` SET_HEADING_AND_SPEED is a stub |
| The script marks whether the wind carries it | todo | `CHLApi.cpp` SET_AFFECTED_BY_WIND is a stub |
| It finishes when its time is up, and scripts can tell | todo | |
| Weather objects are saved with the game | todo | See ../engine/ |
| The land's script lays a storm with its own cloud, rain and lightning settings | todo | `FeatureScriptCommands::CreateWeatherStorm` is an empty stub |

## Real-world weather

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player gives their country and town, from the game's country list | todo | the game ships its list of countries (`Scripts/country.lhw`) |
| Outside multiplayer, the game asks a weather server for the day's weather where the player lives | todo | the game's weather service no longer exists; a replacement source would be needed |
| The real weather is turned into a preset that sets the land's rain, snow, temperature and wind | todo | |
| The last weather fetched is kept on disk for when there is no connection | todo | |

## Options

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather detail option turns falling rain and snow on and off | done | `src/Graphics/DetailLevel.h` k_Weather, `RainSystem.cpp`, `SnowfallSystem.cpp` |
| The weather debug window shows the climates and storms, forces storms and strikes lightning | n/a | openblack-only, `src/Debug/Weather.cpp` (a3342c43, d7d3141f); see ../debug/ |
