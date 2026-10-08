# Countries and ground types

Every cell belongs to one of the land's countries (a style of land such as grassland, desert, snowy mountain or
jungle). A country picks the ground's materials by height, and each material has a ground type that decides the sounds
of footsteps and the ambience.

**Progress: 9/12 done, 0 partial — 75%**

## Country styles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land has up to sixteen countries, each with its own table of materials by altitude | done | `components/lnd` (countries), `src/3D/BlockTexture.cpp` |
| The ground climbs through the country's materials with height (beach, grass, rock, snow on the peaks) | done | `src/3D/BlockTexture.cpp` |
| Countries meet with a soft blend rather than a hard line | done | `src/3D/BlockTexture.cpp`; `test/test_block_texture.cpp` |
| Different lands use different styles (the Norse snow of land 2, the Japanese and Aztec lands of land 3) | done | read from each land's file |
| A land's script can turn a stretch of land into another country while the game runs | todo | `FeatureScriptCommands::CountryChange` is an empty stub |
| A land's script can change the height of the land while the game runs | todo | `FeatureScriptCommands::HeightChange` is an empty stub |

## Ground types

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each material has a ground type (grass, sand, rock, snow, mud and so on) | done | `LandIslandInterface::GetMaterialTypes` |
| A creature's footsteps sound by the ground it walks on, and differently in water and snow | done | `src/ECS/Systems/Implementations/CreatureAudioSystem.cpp`; see ../creature/ |
| Each cell has an ambient sound type (sea, fresh water, coast, jungle, arctic, desert, countryside, swamp, running water) heard around the camera | done | `src/Audio/SoundMap.cpp`, `test/audio/test_atmos.cpp`; see ../audio/ |
| Trees from the forest miracle are picked by the ground they grow on | done | `src/ECS/Systems/Implementations/ForestSystem.cpp`; see ../miracles/forest.md |
| Temperature of the land comes from its climates, not its country | done | see ../weather/climates.md |
| The creature learns the land by regions (highest point, type of ground) as it explores | todo | See ../creature/ |
