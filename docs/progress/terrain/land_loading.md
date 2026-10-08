# Land loading and the map

Each land is a landscape file of up to 255 blocks of 16 by 16 cells laid on a 32 by 32 block map, opened by the land's
script, with a fixed-point grid of map positions that everything in the game stands on.

**Progress: 16/23 done, 5 partial — 80%**

## Opening a land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script names the landscape file to open, and the land is built from it | done | `src/LHScriptX/FeatureScriptCommands.cpp` (LOAD_LANDSCAPE), `Game::LoadLandscape` |
| A landscape file holds the blocks, which block of the map each one fills, the countries, the material textures, the noise and bump maps | done | `components/lnd` (LNDFile), `src/3D/LandData.h` |
| Map cells hold an altitude, a colour, a luminosity, a country, water, coast and open-water bits, and an ambient sound type | done | `components/lnd/include/LNDFile.h` (LNDCell) |
| Map squares with no block are open sea | done | `src/3D/Implementations/LandIsland.cpp` |
| The five story lands and the playground lands all open | done | `src/Game.cpp` LoadMap; port notes in `PROGRESS-bw-clean.md` |
| Opening a new land clears the old land's weather, rings, footprints, snow and clouds | partial | `Game::LoadMap` resets weather, snow, rings, footprints and clouds (0ea498aa); the other resets come with the systems they reset |
| Blocks are built once into one shared vertex buffer and a physics shape | done | `src/3D/LandBlock.cpp` |
| Far blocks get a coarser mesh and lower-resolution texture, and blocks off screen are skipped | partial | openblack draws every block at full detail with no culling; looks the same or better, but it is not the game's level of detail (`src/3D/LandBlock.cpp`) |
| Block textures are painted in the background while the game plays | partial | openblack paints them all as the land opens (`src/3D/BlockTexture.cpp`); same picture, different timing |
| The land can be saved back to a landscape file (the game's own editor) | todo | openblack has no landscape writer; see ../debug/ for openblack's editor |
| Multiplayer and online lands are downloaded and opened like local ones | todo | See ../multiplayer/; openblack doesn't read the online `.map` file, which holds a header with two sizes, then the land's script text, then the landscape |

## Map positions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Positions are 16.16 fixed point, a 10 m cell per whole unit, with an altitude above the ground | done | `src/3D/MapCoords.h`; `test/test_map_coords.cpp` |
| Metres convert to map positions and back with the game's single-precision rounding, losing a unit on some round trips | done | `src/3D/MapCoords.h`; test `MapCoords.ARoundTripCanLoseAUnit` |
| The map is 512 by 512 cells; positions off it are out of bounds | done | `src/3D/MapCoords.h`; test `MapCoords.CellsAreTheHighWords` |
| Searches walk a square spiral of cells out from the centre | done | `src/3D/MapCoords.h`; test `MapCoords.TheSpiralGrowsASquare` |
| Each map cell keeps the objects standing in it, buildings first, so searches find things near a point | done | `src/ECS/MapCells.cpp`; `test/test_map_cells.cpp` |
| Buildings mark the cells their outline covers | done | `src/ECS/MapCells.cpp`; test `MapCells.ABuildingCoversTheCellsItsOutlineTouchesXByX` |
| A point on the screen is turned into a point on the land (for the hand and the camera) | done | `PickingSystem` land line and pixel point (`src/3D/LandLine.cpp`, `src/ECS/Systems/PickingSystemInterface.h`); test `test_picking` |
| Region questions: does an area hold coast, water, land, hill, forest, field, town or citadel, and where exactly | partial | water and dry-land tests are used by fire, forest, magic and particles (`MagicSystem.cpp` IsDryLand, `ForestSystem.cpp`); hill, coast, field and town region searches are todo |
| Land is told apart as dry land, coast and water by its cell bits | done | `components/lnd` cell properties; used across `src/ECS/Systems/Implementations` |

## Islands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land may hold several separate islands on the one map, with sea between them | done | blocks laid where the file puts them (`src/3D/Implementations/LandIsland.cpp`) |
| The sea stretches out to the horizon all round the map | done | `src/3D/Implementations/Ocean.cpp`, `src/Graphics/SeaRows.cpp`; see ../ocean/sea_surface.md |
| The camera and hand cannot leave the map's bounds | partial | See ../camera/ for the camera limits |
