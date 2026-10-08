# Climates and seasons

Every land has a global climate over the whole map and a few local ones its script places (a cold mountain, a hot
desert, a wet valley). Each climate has a temperature that follows the hour and the month, a wind that follows the
season, and a desire to rain that builds day by day until it breeds a storm.

**Progress: 26/29 done, 2 partial — 93%**

## Climates from the land's script

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes the global climate and local ones with a type, a centre and two radii | done | `FeatureScriptCommands::CreateWeatherClimate`, `WeatherSystem::CreateClimate` |
| The global climate covers the whole map with the first climate type, whatever the script says | done | `WeatherSystem::CreateClimate` |
| The script sets each climate's rain desire, dry days, raining days and whether it is raining | done | `WeatherSystem::SetClimateRain` (CREATE_WEATHER_CLIMATE_RAIN) |
| The script sets each climate's temperature and target temperature | done | `WeatherSystem::SetClimateTemperature` |
| The script sets each climate's wind and its angle | done | `WeatherSystem::SetClimateWind` |
| Climate types come from the game's info tables: storm cloud height, speed, lightning waits, overcast, most storms | done | `WeatherSystem::GetInfo` (InfoConstants climate table) |
| Climates are saved and loaded with the game | todo | See ../engine/ |

## Calendar and seasons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A game year lasts 36000 turns, starting on 5 May 1998 | done | `WeatherSystem::GetDaysFromStart` |
| The year has months and four seasons, spring, summer, autumn and winter, by day of the year | done | `WeatherSystem::GetSeason`, `GetMonth` |
| Each climate's rain budget and desire start from the season's values | done | `WeatherSystem::InitialiseClimate` |

## Temperature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each climate's temperature steps towards a target for the hour of the day and the month | done | `WeatherSystem::ProcessTemperature` (tables by hour and by month) |
| The hour table is mapped onto the land's own dusk and dawn | done | `RelativeHour` in `WeatherSystem.cpp` |
| Once at its target the temperature settles, where the game swings across it ten times a second | partial | deliberate deviation noted in `WeatherSystem::ProcessTemperature`; the swing is too small to change rain to snow |
| Storms pull the temperature towards their own within their radius | done | `ApplyStorm` in `WeatherSystem.cpp` |
| Rain turns to snow below freezing | done | `WeatherSystem::CreateStorm` (snow share) |
| The creature feels the temperature where it stands | done | `CreaturePhysiologySystem.cpp` TemperatureAt; see ../creature/ |

## Rain desire and storm breeding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a game day each climate's desire to rain grows at random | done | `WeatherSystem::ProcessRain` |
| The monthly and hourly rain terms are 0, as the game never loads their table | done | `WeatherSystem::ProcessRain` (matches the game) |
| When the desire reaches 1 the climate makes a storm, if it has raining days left and room for another | done | `WeatherSystem::CreateStorm` |
| A local climate makes its storm near its middle; the global one anywhere on the island | done | `WeatherSystem::FindWhereToCreateStorm` |
| A storm spot over water is tried again only when it is off the island, as in the game | done | `WeatherSystem::FindWhereToCreateStorm` |
| Raining days count up while it rains and down while it is dry, capping how long it may rain | done | `WeatherSystem::ProcessClimate` |
| Days since the last rain are counted | done | `Climate::dryDays` |
| A storm leaving its climate (or the global one's storm entering a local climate) starts to clear and the climate wants a new one | done | `WeatherSystem::ProcessClimate` |

## Wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A climate's wind lies between the season's extremes by how close it is to rain | done | `WeatherSystem::ProcessWind` |
| Storms drift with the wind where they are | done | `WeatherSystem::ProcessClimate` |

## Script control

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Challenge scripts pause and restart the whole climate system | done | `CHLApi.cpp` PAUSE_UNPAUSE_CLIMATE_SYSTEM |
| Challenge scripts pause and restart storm creation | done | `CHLApi.cpp` PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM |
| The weather detail option turns the weather system off | partial | the option hides falling rain and snow (`RainSystem`, `SnowfallSystem`); the climates still run underneath (unconfirmed whether the game stops them) |
