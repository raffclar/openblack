# Storm

The storm miracle throws a swirl from the hand that gathers a ring of dark clouds over the land, bringing rain and
wind that drift the storm with the weather. The extreme (power-up) storm adds lightning from the clouds; the second
power-up is the tornado (see [tornado.md](tornado.md)). Weather outside miracles is in [../weather/](../weather/).

**Progress: 33/35 done, 1 partial — 96%**

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cast radius is held between 20 m and 1000 m and sets the storm's size | done | `src/Magic/SpellBehaviours.cpp` (storm cast) |
| The normal storm has clouds, rain and wind only; the extreme storm adds lightning, the second power-up makes a tornado | done | `src/Particles/ParticleStormRules.cpp` (power-up level) |
| Cost to create, lifetime (40 s) and per-strike cost differ between normal and extreme | done | `src/ECS/Systems/Implementations/MagicSystem.cpp`, effect tables from the game's info |
| Upkeep each turn grows with the square of the radius over the normal-cost radius of 40 m | done | `src/Particles/StormMaths.cpp` (cost to maintain); `test/test_storm_miracle.cpp` |
| In the hand: a small dark cloud ball with flicks of lightning sized by the hand and the seed's strength; each flash lasts half a second, flashes come at randomised gaps | done | `src/Particles/ParticleStormRules.cpp` (in-hand lightning sprite); scenario `miracles.storm_inhand_flash` |

## The swirl at the cast point

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A swirl of 30 dark sprites spins tighter and faster, then disperses after 2.4 s and fades 2 s later | done | `src/Particles/ParticleStormRules.cpp`, `src/Particles/StormMaths.cpp` (swirl radius, turning, alpha) |
| The swirl is thrown along the camera's level heading at up to 20 m/s, speeding up between 1 s and 3 s; cast straight down it stays where it is | done | `ParticleStormRules.cpp` (heading with no fallback direction) |
| The swirl runs only while the miracle's caster exists and is removed when the caster goes | done | `src/ECS/Systems/Implementations/MagicSystem.cpp` |

## Clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clouds are born over about 10 s until the storm has its full number, each starting on a random ring around the centre | done | `ParticleStormRules.cpp` (cloud births), `StormMaths.cpp` (new cloud radius) |
| Each cloud orbits the centre while it gathers, slowing and closing in as it forms, and is reborn when it has lived its gathering time | done | `StormMaths.cpp` (gather progress, angular speed, distance) |
| A cloud's grey shade, transparency and size follow its gathering | done | `StormMaths.cpp` (cloud look) |
| Clouds float about 90 m up, following the average height of the land under the storm | done | `ParticleStormRules.cpp` |
| Clouds darken the land below them as a moving shadow | done | scenario `miracles.storm_clouds_below` |
| When the storm ends the clouds fade out over about 3 s | done | generic fade and removal rules from the effect file; scenario `miracles.storm_village` |

## Rain and wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storm lays a rain storm in the weather with inner and outer radii from its size, fading in over half its gathering time, overcast and rain set from the storm | done | `StormMaths.cpp` (rain storm, rain byte), `src/ECS/Systems/Implementations/WeatherSystem.cpp`; see [../weather/](../weather/) |
| The rain stops at once when the storm ends | done | `WeatherSystem.cpp`; scenario `miracles.storm_village` |
| The storm blows wind along the direction it was thrown, stronger for bigger storms, rounded as the game rounds | done | `StormMaths.cpp` (weather byte, round half to even) |
| After a short delay the storm drifts with the local wind, smoothly and with no slowing on slopes | done | `ParticleStormRules.cpp`, `StormMaths.cpp` (drift); scenario `miracles.storm_drift_slope` |
| A storm cast by a script takes no wind and stays put | done | `MagicSystem.cpp` (neutral caster); scenario `miracles.storm_script` |
| Rain and wind ambience rise and fall with the weather at the camera | done | `src/Audio/SoundMap.cpp`, `src/Audio/AtmosAudio.cpp` |
| The rain cools and puts out fires under the storm, and villagers come to watch the storm putting a fire out (one gathering per storm at a time) | done | `src/ECS/Systems/Implementations/FireSystem.cpp` (rain cooling), `MagicSystem.cpp` (rain watchers) |
| The wind drifts fire sprites and smoke | done | `FireSystem.cpp` (sprite drift by wind) |
| A script can end every storm in an area; a miracle storm caught in it comes back on the next step and fades in again | done | `src/CHLApi.cpp` (kill storms in area), `WeatherSystem.cpp` |

## Lightning (extreme storm)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first strikes come about 9.5 to 11 s after the cast | done | `ParticleStormRules.cpp`; checked in game (`miracles.storm_powerup`) |
| Strikes come from clouds that are more than half gathered, at random waits shortened by tribal power | done | `StormMaths.cpp` (next strike wait) |
| A striking cloud flashes bright for half a second | done | `StormMaths.cpp` (flash light) |
| A bolt hits trees, buildings and other things below within its reach, or the ground when there are too few | done | lightning target search shared with the lightning miracle, see [lightning.md](lightning.md) |
| A bolt carried from one cloud to the next keeps what it was striking | done | `ParticleStormRules.cpp` |
| Struck things catch fire and take damage, and each strike costs prayer power | done | generic miracle effect (`src/Magic/SpellBehaviours.cpp`) |
| Thunder (small, medium or large at random) sounds from the land under the striking cloud, delayed by its distance at the speed of sound | done | `ParticleStormRules.cpp`, `src/Particles/ParticleSounds.cpp` (on-land sound) |
| Thunder far from the camera is not heard, which keeps the number of thunders down | partial | the reasoning matches the game; a per-strike count in game is still owed |
| The extreme storm is turned aside by shields it passes over | done | `ParticleStormRules.cpp` (shield deflection) |

## Reactions and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers flee from the storm, and are impressed by it as the game measures | done | shared miracle reactions, `src/ECS/Systems/Implementations/ReactionSystem.cpp`, `src/Magic/Impressiveness.cpp` |
| A storm and its clouds are kept in a saved game | todo | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
| The storm's clouds and spin keep their look when the camera is close below them | done | scenario `miracles.storm_clouds_below` |
