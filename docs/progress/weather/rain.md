# Rain

Where a storm rains, streaks of rain fall over the land near the camera, as dense and as opaque as the rain there. Rain
puts out fires, waters crops, drowns out the wildlife and is heard drumming.

**Progress: 15/20 done, 1 partial — 78%**

## Falling rain

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One set of 128 streaks is repeated over every land block near the camera where it rains | done | `src/3D/Rain.cpp` (720abfe2); `test/test_rain.cpp` |
| Each streak runs from under the ground up to the rain's height, with a dashed texture scrolling down it | done | `src/3D/Rain.cpp`, `RainSystem` |
| Streaks slant, fade in and out over their life and start again elsewhere | done | `src/3D/Rain.cpp`; test |
| A block shows more and more opaque streaks the harder it rains over its middle | done | `rain::ForBlock`; test |
| Little rain shows nothing, and the rain thins from 100 units away from the camera | done | `rain::ForBlock`; test |
| The rain's height and speed follow the nearest storm's, or the calm air's, stepping towards them a frame at a time | done | `RainSystem::Update` |
| Streaks are drawn with random numbers in the game's order | done | `src/3D/Rain.cpp` |
| Rain is only drawn with the weather detail option on | done | `RainSystem.cpp` (detail_level::Weather) |
| Rain splashes on the ground | todo | port notes: splashes todo |
| Rain makes rings on the water | todo | port notes; see ../ocean/things_on_the_water.md |
| Rain is not drawn inside the temple | done | (the temple has no weather) |

## What rain does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rain (or snow) puts out fires and slows their spread | done | `FireSystem.cpp` surroundings.rain; see ../physics/ |
| Rain on burning things raises steam | done | `GameParticleWorld::IsRainingAt` (`ParticleSystem.cpp`) |
| Fields grow faster in rain, by their crop's rain rules while growing and ripening | done | `FieldSystem.cpp` (b1ed8d6d); see ../resources/ |
| Rain is heard, louder the harder it rains at the camera | done | `src/Audio/SoundMap.cpp` (rain atmos volume) |
| Birdsong and wildlife fade as the weather worsens | done | `src/Audio/SoundMap.cpp`; `test/audio/test_atmos.cpp` |
| A town that wants rain raises its rain flag; rain on it satisfies the wish | todo | see ../town/ (town desires) |
| Villagers react to rain (shelter, dance for it) | todo | (unconfirmed which); see ../villager/ |
| The water miracle is local rain from the hand | partial | see ../miracles/water.md |
| The storm miracle's rain | done | see ../miracles/storm.md |
