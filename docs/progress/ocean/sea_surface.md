# The sea

The sea around every island: a textured surface drawn in rows across the screen, each row rippling on its own and
fading with distance, coloured by the land's light, blended over the mirrored world beneath it.

**Progress: 15/16 done, 1 partial — 97%**

## Drawing the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sea is a square 30000 units across at sea level about the middle of the map, reaching the horizon | done | `src/Graphics/SeaRows.cpp`; `test/test_sea_rows.cpp` |
| The sea is drawn in screen rows two pixels apart, from where it meets the top of the screen down to the bottom | done | `src/Graphics/SeaRows.cpp`, `shaders/fs_water.sc` (bbc5d86d, a83415b8) |
| Every other row repeats the texture of the one before, so each texture line holds for two pixels then blends | done | `shaders/fs_water.sc` |
| Each row ripples along the view by its own step of a 16-step sine, close up only | done | `shaders/fs_water.sc`; ripple step in `Renderer::SetSeaUniforms` |
| The ripple moves on a step each frame | done | `Renderer::SetSeaUniforms` |
| The sea is fully opaque near and fades to 80 of 255 far away, showing what lies beneath | done | `shaders/fs_water.sc` depth fade |
| A first row that starts inside the screen is nearly clear, softening the horizon | done | `src/Graphics/SeaRows.cpp` softTop |
| The sea's texture takes the land's light at full luminosity, so it darkens at night and reddens under an evil sky | done | `shaders/fs_water.sc` (sea colour) |
| The sea's texture repeats more finely at higher detail levels | done | `src/Graphics/DetailLevel.h` k_WaterTiling, `sea_rows::Period` |
| At the lowest detail the sea is a still square, its texture repeated 50 times, fully opaque | done | `shaders/fs_water.sc` still sea |
| Ambient wind would drift the sea's texture | n/a | the game's ambient wind is always 0 in play, so the drift never shows (port notes) |
| The sea is drawn after the land so the land's coast fades into it | done | render order in `src/Graphics/Renderer.cpp` |
| The sea is not drawn inside the temple, which has its own pool | done | see ../temple/ |
| The sea is hazed with distance like the land | partial | the sea fades by depth; whether it takes the land's haze colour as the game does is unconfirmed |
| The view under the sea is drawn at the size of the view | done | `src/3D/OceanInterface.h`, `Ocean::ResizeReflectionFramebuffer` |

## Sounds of the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sea is heard from cells of sea, louder by how much of the view is sea and by height | done | `src/Audio/SoundMap.cpp`; `test/audio/test_atmos.cpp`; see ../audio/ |
| Waves on the shore are heard on the coast | done | coast ambient type (`src/Audio/SoundMap.cpp`) |
