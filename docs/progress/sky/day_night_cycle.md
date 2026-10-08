# Day and night

The world runs through day, dusk and night on a clock of its own, about 28 minutes a day by default. Each land's
script sets the length of the day and how much of it is night; challenge scripts can stop, set or move the clock.

**Progress: 20/24 done, 2 partial — 88%**

## The clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The time of day runs evenly, a whole day in a set number of seconds of game time, moving on once a turn | done | `src/3D/DayNightClock.cpp`; `test/test_day_night_clock.cpp` |
| The default day lasts 1700 seconds, 8.3% of it full night and 7% of it changing | done | `DayNightClock::k_DefaultDuration` (efc769b2, ae6de30a) |
| A land opens at noon with the clock running | done | `src/3D/DayNightClock.cpp` |
| The sky turns at four hours of each half day: full night, the start and end of dusk, full day | done | `src/3D/DayNightClock.cpp`; fireflies come out and hide by these stages: [../nature/fireflies.md](../nature/fireflies.md#night-and-day) |
| Each land's script sets the day's length and its night and change fractions | done | `FeatureScriptCommands::SetNighttime` (SET_NIGHTTIME) |
| Scripts see a stretched time in which those four hours fall at 3.5, 7.5, 8 and 8.5 | done | `DayNightClock::GetScriptTime` |
| Scripts jump the clock to an hour; the sky's dome is rebuilt at once | done | `CHLApi.cpp` SET_GAME_TIME; a0ce4593, 2fc53b3d |
| Scripts read the hour | done | `CHLApi.cpp` GET_GAME_TIME |
| Scripts move the clock to an hour over some seconds, the short way round | done | `CHLApi.cpp` MOVE_GAME_TIME |
| Scripts stop and restart the clock | done | `CHLApi.cpp` GAME_TIME_ON_OFF |
| Scripts change the day's length and fractions, and reset them to the default | done | `CHLApi.cpp` SET_GAME_TIME_PROPERTIES, RESET_GAME_TIME_PROPERTIES |
| The clock stops while the game is paused | done | `src/ECS/Systems/Implementations/TimeSystem.cpp` |
| The clock is saved and loaded with the game | todo | See ../engine/ |
| Scripts read the real date and time of the player's computer | todo | `CHLApi.cpp` GET_REAL_TIME, GET_REAL_DAY, GET_REAL_MONTH, GET_REAL_YEAR are stubs |

## What night changes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky's dome turns from day through dusk to night | done | see sky_dome.md |
| The land's and models' light follows the time of day | done | see lighting.md |
| The sun sets and the moon rises | done | see sun_and_moon.md |
| Windows of homes with people in light up at night | done | 5ca336bb; see ../building/ |
| Village lanterns and fires glow at night and light the land | partial | a7e48d8f, 1ea6b354; see ../town/ and ../rendering/ |
| The hand gives off light at night | done | `src/Graphics/HandLight.cpp`; see ../hand/ |
| Villagers go home to bed at night | partial | 7211c128; see ../villager/daily_routine.md |
| Night sounds (crickets, owls) replace the day's | done | see ../audio/ |
| The haze closes in at dusk | done | `src/3D/LandLightTable.cpp` |
| Temperature falls at night and rises by day | done | see ../weather/climates.md |
