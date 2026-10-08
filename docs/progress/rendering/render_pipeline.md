# Render pipeline

How a frame is put together: the passes drawn before the scene, the sky first, the opaque world, then everything that
blends sorted farthest first, the near clipping plane and the distance haze. The sea's reflection pass is in
[../ocean/](../ocean/), the temple's rooms in [../temple/](../temple/).

**Progress: 10/16 done, 2 partial — 69%**

## Frame structure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Before the scene: the land's luminosity and cell colours, the sky dome's bands, the rivers' alpha, the objects' shadows and the creatures' and hand's silhouettes | done | `src/Graphics/RenderPass.h`, `src/Graphics/Renderer.cpp` |
| The sky is drawn first and everything else over it | done | `RenderPass.h` |
| The frame's updaters run in the game's order, getting everything ready before drawing | todo | port notes: not started |
| Films overlaid on the picture, and the order the frame is finished in | todo | port notes: not started |
| The game's pointer is drawn over everything | done | `RenderPass.h` |

## Transparency and ordering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Everything that blends is drawn after the rest of the scene, farthest from the camera first, whatever it is | done | `src/Graphics/ZSort.h`; test_zsort |
| The sort uses the distance squared, in single precision, as the game does | done | `ZSort.h`; test_zsort |
| Models that fade or have alpha, sprites, mists, clouds and the hand all go through the one sort | done | `ZSort.h` (port notes) |
| Chimney smoke, particles and rain each take their place in the sort | done | `Renderer.h` |
| The see-through hand is drawn from both sides, the inside first | partial | the game culls its back faces; openblack draws both sides so the outside blends over the inside (`Renderer.cpp`) |
| Footprints are blended onto the land before the rest of what blends | done | `Renderer.h` |

## Clipping and haze

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The near clipping plane moves out with the camera's height over the land | done | test_near_clipping |
| Scripts can set the clipping close | done | `CinematicDirectorSystem`; test_near_clipping |
| Far land and models fade into the distance haze | partial | `assets/shaders/haze.sh`; the land's per-block haze classes and the fog detail key are todo (port notes) |
| Lines and triangles game code draws straight into the world | todo | port notes: world triangles not started |
| Objects drawn slightly away from where they are, the gap closing over time | todo | the game keeps decaying draw offsets for objects (unconfirmed which) |
