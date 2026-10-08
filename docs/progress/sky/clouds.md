# Clouds

Seventy puffs of cloud drift with the wind along a track across the island, fading in and out at its ends, with two
huge ones pinned on the horizon. Their shadows pass over the land. Storms bring dark clouds of their own.

**Progress: 12/19 done, 3 partial — 71%**

## Fair-weather clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new land lays 70 clouds at random along a track, 300 to 500 high, up to 5000 either side | done | `src/3D/Clouds.cpp` (5670954a); `test/test_clouds.cpp` |
| The first two are pinned at the track's ends on the horizon, 300 times the size | done | `src/3D/Clouds.cpp`; test |
| Clouds move along the track at 70 units a second of game time, back to the start past its end | done | `CloudSystem`; test |
| The track lies three eighths of a turn about the middle of the map | done | `src/3D/Clouds.cpp` |
| Clouds fade in over the first 2000 units of the track and out over the last | done | `src/3D/Clouds.cpp`; test |
| Clouds are puffs of mist that shrink edge on, seen from the side | done | `src/3D/Mists.cpp` |
| Clouds animate through frames of the smoke texture | done | `src/3D/Mists.cpp`; `test/test_mists.cpp` |
| Clouds are white for good, grey for neutral and dark orange for evil, in the land's light | done | `src/3D/Clouds.cpp` |
| Clouds stop while the game is paused | done | `CloudSystem` |
| Clouds are sorted with other translucent things | done | 0917023b |
| The cloud detail option turns clouds on or off | done | `src/Graphics/DetailLevel.h` k_Clouds |
| Clouds' shadows darken the land as they pass | done | 0fd50170; `src/3D/LandColourStamps.cpp` |
| The cloud shadow option is separate from the clouds option | partial | openblack ties shadows to the clouds option; the game has its own switch for cloud shadows |
| The track follows the climate's wind | partial | the track's direction is fixed; whether the game turns it with the wind is unconfirmed |

## Storm clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each natural storm draws its own clouds: up to 16 puffs of mist wandering about its middle at its cloud height | todo | the weather simulation runs (`WeatherSystem`) but natural storms draw no clouds; see ../weather/storms.md |
| Storm clouds are greyed by the storm's overcast and grow with its radius | todo | |
| A storm's clouds cast a shadow on the land, stronger with its overcast | todo | the cloud shadow stamp exists (`src/3D/LandColourStamps.cpp`) but storms don't lay one |
| The storm miracle's clouds gather and drift | partial | see ../miracles/storm.md |
| Scripts change a storm's number of clouds, darkness and height | todo | `CHLApi.cpp` CHANGE_CLOUD_PROPERTIES is a stub; see ../weather/scripted_weather.md |
