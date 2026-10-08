# Reflections

The sea mirrors the world above it: the sky and the land are drawn upside down through the sea level before the sea is
blended over them, so islands, trees and buildings show in the water.

**Progress: 8/12 done, 3 partial — 79%**

## The mirrored world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky is drawn mirrored first, in its own pass | done | `RenderPass::ReflectionSky` (f0f3549b) |
| The land is drawn mirrored from a camera reflected through the sea level | done | `src/Graphics/Renderer.cpp` reflection pass, `Camera::Reflect` |
| The mirrored land is lit at half the land's light | done | `src/Graphics/Renderer.cpp`; port notes |
| The mirrored land has no small bump detail | done | `src/Graphics/Renderer.cpp` (smallBumpMapStrength 0) |
| Only what stands above the sea is mirrored: models are cut at sea level | done | `MeshUniform::SeaClip` (dd15e203) |
| Models in the reflection take half the land's light | done | `Renderer.cpp` reflection light 0.5 |
| Translucent things (mists, clouds, the hand) blend in the reflection, farthest first | done | `RenderPass::ReflectionTranslucent` (0917023b) |
| The game mirrors only the sky, the land and a few moving things; openblack mirrors everything | partial | deliberate deviation (`src/Graphics/Renderer.cpp` comment); looks richer than the game |
| The reflection target resizes with the view | partial | `Ocean::ResizeReflectionFramebuffer` exists; the game's resizable reflection is todo in the port notes |
| Swimmers, sharks and fishing nets are cut by the sea plane | todo | port notes: wait for those objects |
| The sun and moon do not show in the sea | partial | the sky pass is mirrored; whether the game mirrors the sun's glow is unconfirmed |
| The temple's pool reflects the room above it | done | `shaders/fs_reflection.sc`; see ../temple/ |
