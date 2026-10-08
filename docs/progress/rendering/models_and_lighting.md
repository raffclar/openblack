# Models and lighting

How the game draws its 3D models (buildings, trees, villagers, animals, creatures, the hand) and how it lights them: the
material modes, skinning, and the sun, ambient and night lights that shade them. A creature's look and body morphs are
in [../creature/](../creature/), the hand's look in [../hand/](../hand/), the sky's light by time of day in
[../sky/](../sky/).

**Progress: 15/26 done, 7 partial — 71%**

## Meshes and skinning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models from the game's mesh pack and model files are drawn with their own textures | done | `src/3D/L3DMesh.cpp`, `src/Graphics/Renderer.cpp` (mesh and submesh drawing) |
| Many copies of one model are drawn together | done | `src/Graphics/Renderer.cpp` instanced draws; openblack batches them, the look is the same |
| Boned models are posed by their skeletons as they animate | partial | creatures and the hand pose and animate (`src/3D/SkeletalAnimation.cpp`, `CreatureAnimationSystem`); villagers are still drawn from their bind pose (port notes) |
| Where the parts of a boned body meet, seam vertices are pulled towards their partners so shoulders, neck and tail bend instead of tearing | done | `src/3D/VertexBlend.cpp`; test_vertex_blend |
| Poses cross-fade from one animation into the next | partial | the hand cross-fades (`src/3D/HandCrossFade.h`); the general skeletal pose cross-fade is todo (port notes) |
| Parts of a model shown only in some states (building stages, graves) | todo | status submeshes are skipped (`Renderer.cpp` submesh drawing) |
| Collision-only parts of models are never drawn | done | `Renderer.cpp` skips physics submeshes |

## Materials and render modes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each material is drawn in one of the game's 19 render modes: its blending, alpha test, depth write and how texture alpha combines with vertex colour | done | `src/Graphics/RenderModes.h` |
| Two-sided materials are drawn from both sides, the others have their back faces culled | done | material culling in `Renderer.cpp` |
| Cut-out textures (leaves, fences) drop their see-through texels | done | alpha-tested render modes, `RenderModes.h` |
| Textures that slide across a model or play frames | partial | sliding textures for mists, particle models and the temple; whether every sliding material of the game's models is covered is unconfirmed |
| A model's materials swapped for others while the game runs | todo | port notes: material type swaps not started |
| Textures cut to the game's low colour formats (16-bit) | todo | openblack keeps full colour; whether to match the banding is undecided (port notes) |
| Shiny environment map over some models (frozen things' ice, the cave's trophies) | partial | ice and trophies (`RendererInterface.h` environment map, `src/3D/CreatureCaveTrophies.h`); other environment-mapped objects unconfirmed |

## Lighting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models are lit per vertex by one light and an ambient level, in whole numbers | done | `src/Graphics/ModelLight.cpp`, `assets/shaders/model_light.sh`; test graphics/test_model_light |
| The light is a distant sun by day and moves beside the hand in the darker half of the night; none in the temple | done | `ModelLight.cpp`; test graphics/test_model_light |
| Models take the colour of the land's light where they stand | done | `assets/shaders/fs_object.sc`, land light (port notes) |
| Trees take the land's light scaled by a brightness worked out each frame from where the camera looks against the light | done | `src/Graphics/TreeBrightness.cpp`; test_tree_brightness |
| Big forests, dead trees and flowers are drawn unlit | done | Unlit component (port notes) |
| The hand is drawn half again as bright as the land's light under it | done | `RendererInterface.h` land light scale |
| Homes with people inside show lit windows at night, blended by their material's alpha | done | `assets/shaders/window_light.sh` (port notes) |
| Village lights and the hand's light brighten models near them at night | partial | they brighten the land's light, which the models take; whether models are lit exactly as the game does is unconfirmed |
| Lights at night cast light and shadows on nearby objects | todo | the game adds lights of several kinds to a dynamic light and shadow list; not started |
| A lightning flash brightens the world | partial | on the land's light (`src/3D/Lightning.cpp`); see [../weather/](../weather/) |
| Hot and burning objects glow red-orange, flickering | done | `src/Fire/FireGraphic.h`; see [../physics/](../physics/) |
| Scripts give objects their own colour and alpha | partial | an object colour and alpha exist in the mesh draw (`RendererInterface.h`); which script calls reach it is unconfirmed |
