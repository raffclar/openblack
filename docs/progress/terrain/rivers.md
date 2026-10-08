# Rivers and lakes

Rivers are laid by the land's script as a chain of points. Along each stretch a channel cuts the land so the water
underneath shows, and a bed is blended into the land's colour. Lakes are low land below sea level that shows the same
water.

**Progress: 12/18 done, 2 partial — 72%**

## Laying the rivers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes each river and gives it its points in order | done | `FeatureScriptCommands::CreateStream`, `CreateStreamPoint`; `src/ECS/Components/Stream.h` |
| A river runs from each point to the next, never from the last back to the first | done | df44fb14 (ordered stream points) |
| Each stretch has a channel mesh, turned and stretched to reach the next point | done | `src/ECS/Archetypes/StreamSegmentArchetype.cpp` |
| The channel clears the land's alpha so the water drawn underneath shows as the river | done | `shaders/fs_land_alpha.sc`, `src/3D/Implementations/LandIsland.cpp` (land alpha); df44fb14 |
| Each stretch has a bed mesh blended into the land's colour, as a building's footprint is | done | `Renderer::DrawFootprintPass`; df44fb14 |
| Rivers look the same in the sea's reflection | partial | the land alpha is applied to the mirrored land; not checked against the game (unconfirmed) |
| Rivers are drawn for debugging as lines between their points | n/a | openblack-only (`RenderingSystemCommon::PrepareDraw` drawStreams) |
| Rivers are saved and loaded with the game, with their points | todo | See ../engine/ (saving) |
| The game can find the nearest point of a river to a position | todo | not ported; what uses it in the game is unconfirmed |

## Water in rivers and lakes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The water in a river or lake is the sea, with its rows, ripple and colour, seen through the land | done | see ../ocean/sea_surface.md |
| Lakes are land below sea level, drawn flat with the water showing | done | `LandIsland::GetDrawnAltitude`, coast alpha in `src/3D/BlockTexture.cpp` |
| Running water is heard near rivers | done | cell ambient sound type 9 (`src/Audio/SoundMap.cpp`); see ../audio/ |
| Still fresh water is heard near lakes | done | cell ambient sound type 2 (`src/Audio/SoundMap.cpp`) |
| Fires near or in water go out; a fireball over water steams | done | `FireSystem.cpp` (water and coast cells); see ../physics/ |
| Trees and forests don't grow in water | done | `ForestSystem.cpp` |
| A creature wades in shallow water and avoids deep water | done | `CreatureLocomotionSystem.cpp`; see ../creature/ |
| Villagers and creatures can drown or swim in deep water | todo | See ../villager/ and ../physics/ |
| Waterfalls pour with spray and sound where the land's script places them | todo | `FeatureScriptCommands::CreateWaterfall` is an empty stub; no story land script places one (the creature cave's waterfall is in ../temple/) |
| Splashes and rings on rivers and lakes where things fall in | partial | see ../ocean/things_on_the_water.md |
