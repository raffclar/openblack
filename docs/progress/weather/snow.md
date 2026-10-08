# Snow

Cold storms snow. Flakes tumble down near the camera, and the snow piles up on the land, buildings and trees, then melts
away slowly once the storm has gone.

**Progress: 20/21 done, 0 partial — 95%**

## Falling snow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One set of 256 flakes over an 80 unit square is repeated over every quarter block near the camera where it snows | done | `src/3D/Snowfall.cpp` (a7b05fb4); `test/test_snowfall.cpp` |
| Flakes tumble as they drift down in circles | done | `src/3D/Snowfall.cpp`; test |
| A flake that falls through the ground starts again at the top | done | `snowfall::Advance`; test |
| More flakes and more opaque the harder it snows; none for a little snow; fainter from 50 units away | done | `snowfall::ForQuarter`; test |
| Snow falls from the rain's height at the rain's speed | done | `SnowfallSystem` |
| Snow is only drawn with the weather detail option on | done | `SnowfallSystem.cpp` |

## Snow lying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A snowing storm piles snow on a grid of 40 unit cells, most within its inner radius | done | `src/3D/SnowCover.cpp` (85b4ed0d); `test/test_snow_cover.cpp` |
| Towards a storm's edge snow is taken away | done | `snow_cover::Lay`; test |
| Snow depth is capped | done | `snow_cover::k_MaxDepth` |
| Snow melts by 2 in one of eight bands of rows every 0.3 seconds | done | `snow_cover::Melt`; test |
| The land turns white where the snow is deep enough, raggedly by a noise image | done | `shaders/snow.sh`; test |
| Buildings, trees and objects standing in snow show a snow texture over them, more the deeper it lies | done | `shaders/snow_object.sh` (85b4ed0d) |
| Snow shows on the faces turned upward | done | 5a84e39e (mapped by facing) |
| A new land starts with no snow | done | `SnowSystem` Reset |
| Snow lying is saved with the game | todo | See ../engine/ |

## What snow does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature's footsteps crunch in snow | done | `CreatureAudioSystem.cpp` (snow depth) |
| A tornado over snow throws up snow dust | done | `ParticleSystemTornado.cpp`; see ../miracles/tornado.md |
| The forest miracle grows snowy trees on snow | done | `ForestSystem.cpp` (snow depth) |
| Snow puts out fires as rain does | done | `FireSystem.cpp` |
| The creature feels the cold | done | `CreaturePhysiologySystem.cpp`; see ../creature/ |
| Snow-covered peaks painted into the land are not weather and never melt | done | see ../terrain/countries.md |
