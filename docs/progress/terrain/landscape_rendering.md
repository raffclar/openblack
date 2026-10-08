# Landscape rendering

How the land is drawn: each block painted from its countries' materials, lifted by noise and a bump map, lit by a
per-cell luminosity through a light table that follows the time of day and the alignment, faded into the sea at the
coast, and hazed with distance.

**Progress: 23/26 done, 1 partial — 90%**

## Painting the blocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each block is painted 16 by 16 texels a cell, in 4 bits a channel | done | `src/3D/BlockTexture.cpp`; `test/test_block_texture.cpp` |
| Each texel takes its country's material for the height there | done | `src/3D/BlockTexture.cpp` |
| The land's noise map shifts the height a texel's material is picked by, so material edges are ragged | done | `src/3D/BlockTexture.cpp` |
| The bump map shades each texel | done | `src/3D/BlockTexture.cpp` |
| Where a cell's corners lie in different countries, each corner paints the texel and the four are blended by distance | done | `src/3D/BlockTexture.cpp` (cone weights); `test/test_block_texture.cpp` |
| The texture fades to clear towards the coast, below altitude 4, so the sea drawn underneath shows through | done | `src/3D/BlockTexture.cpp` coast alpha; `shaders/fs_terrain.sc` |
| Open-sea cells are not drawn at all | done | 10fa0be1; `src/3D/BlockTexture.cpp` |
| A small bump texture is drawn over the land near the camera for fine detail | done | `shaders/fs_terrain.sc` small bump pass (b47d998e) |

## Light and colour

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each cell corner has a luminosity that lights the land through a 256-level light table | done | `src/3D/LandLightTable.cpp`; `test/test_land_light_table.cpp` |
| The light table is rebuilt every frame from the weather palette for the time of day and the alignment the sky shows | done | `src/3D/LandLightTable.cpp`, `src/3D/LandLightFrame.cpp` |
| The darkest levels ramp through a warm colour, and the brightest come out brighter than the palette | done | `src/3D/LandLightTable.cpp` |
| Each cell's own colour is added to the land as a light, the haze applied | done | 1bba0e52 (cell colours as specular) |
| Night lights of villages and the hand are stamped onto the cells | done | a7e48d8f; see ../rendering/ and ../town/ |
| Cloud shadows darken the cells as clouds pass | done | 0fd50170; `src/3D/LandColourStamps.cpp`; see ../sky/clouds.md |
| Lightning stamps a glow on the land where it strikes | done | 1bba0e52; `src/3D/LandColourStamps.cpp`; `test/test_land_colour_stamps.cpp` |
| Static shadows of buildings and trees are baked into the land | todo | port notes: static shadows todo |
| Overcast darkens the land and draws the haze in | done | ab345ff0; `src/3D/LandLightTable.cpp` |
| Normals of the land are worked out per cell triangle for lighting the things on it | done | `src/3D/LandNormal.cpp`; `test/test_land_normal.cpp` |

## Distance and detail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Distant land fades into a haze a third of the land's colour, closing in at dusk | done | 4b1a1cc5; `shaders/haze.sh` |
| Each block picks its own haze class | todo | port notes: per-block haze classes todo |
| The fog detail option turns the haze and overcast tinting on or off | partial | `src/Graphics/DetailLevel.h` k_Fog drives the dome tint; what else the game's fog option switches is unconfirmed (the haze itself ignores it here) |
| Land at sea level is drawn flat at height 0 | done | `LandIsland::GetDrawnAltitude` |
| The land under the sea is drawn mirrored, half lit and without bump detail, for the reflection | done | `src/Graphics/Renderer.cpp`; see ../ocean/reflections.md |
| Snow lying on the land whitens it, raggedly by a noise image | done | 85b4ed0d; see ../weather/snow.md |
| Rivers clear the land's alpha so the water shows | done | df44fb14; see rivers.md |
| Building footprints are blended into the land's colour | done | `Renderer::DrawFootprintPass`; see land_marks.md |
