# Sprites and billboards

Flat textured pictures drawn in the world rather than models: camera-facing sprites, sprites lying in their own plane,
the glows of lights and lanterns, and text laid out in the world. Particle sprites are in [particles.md](particles.md);
the moon and sun in [../sky/](../sky/); the influence ring in [../worship/](../worship/).

**Progress: 9/12 done, 0 partial — 75%**

## Sprites in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sprites turn to face the camera | done | `src/ECS/Components/Sprite.h`, sprite drawing in `src/Graphics/Renderer.cpp` |
| Sprites lie in their own plane, as their placement turns them | done | `Sprite.h` |
| A sprite either adds light to what is behind it, like a glow, or blends over it by its alpha | done | `Sprite.h` |
| A colour texture takes its alpha from its companion alpha file; a texture without one is a single channel that is both alpha and brightness | done | `Sprite.h` |
| Sprites take their place among everything else that blends, farthest first | done | `src/Graphics/ZSort.h`; test_zsort |
| Animated billboards running on their own frame clocks | todo | port notes: billboards and frame clocks not started |
| Distant trees drawn as flat stand-ins | n/a | the game draws its trees as models (unconfirmed that it has no stand-ins) |

## Glows of lights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A light from the light files glows as two sprites added over each other from the atmosphere texture, the second half as strong and whiter | done | `src/3D/Light.h`, `components/glw`, `src/ECS/Archetypes/GlowArchetype.cpp` |
| Glows of lights set in walls (the temple's windows) lie flat against the wall instead of facing the camera | done | `src/3D/Light.h` |
| Village lanterns and campfires burn with two flames and a glow, the flames playing the fire's frames backwards on one clock and flickering in size | done | `src/3D/VillageLights.cpp`; test_village_lights |
| Plain glows the game draws at points in the world | todo | the game has a glow manager drawing glows and white glows at points (unconfirmed what uses them) |

## Text and symbols in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Text laid out in the world, letter by letter from the font, along a frame | done | `src/3D/OrientedText.cpp` (the temple's signs and scrolls); see [../temple/](../temple/) |
| A player's symbol drawn into textures in the player's colour | todo | the game draws player symbols into textures (unconfirmed where they show) |
