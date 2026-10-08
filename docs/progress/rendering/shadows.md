# Shadows

The shadows the game draws: the fixed shadows of trees, rocks and buildings on the land, the creature's and the hand's
moving shadows, and the villagers' small ground blobs. Cloud shadows are in [../sky/](../sky/); particle effects that
darken the land are in [particles.md](particles.md).

**Progress: 10/18 done, 3 partial — 64%**

## Shadows on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees, rocks, buildings and features cast shadows onto the land from a fixed far sun, as long as they are high | done | `src/Graphics/ObjectShadows.cpp`; test graphics/test_object_shadows. The game bakes them into the land's textures, openblack into one texture over the island |
| Where shadows overlap the land gets no darker; a fully covered spot is half as bright | done | `ObjectShadows.h`; test graphics/test_object_shadows |
| Shadows follow objects as they are built, felled or moved | done | the object shadow pass is drawn each frame (`Renderer.cpp`) |
| At night homes, the village centre, storehouse, trees and totems cast shadows from the lights | todo | the game marks which objects cast shadows at night (unconfirmed how they are drawn) |
| Shadows projected over other objects, not only the land | todo | port notes: projected shadow list not started |
| The land's own baked shadow map | todo | port notes: static shadows on the terrain not started |
| Short-lived shadows the game keeps for a while | todo | the game updates a list of temporary shadows (unconfirmed what casts them) |

## Creature shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature's silhouette, seen from the light, is drawn into a soft small texture and projected onto the land and what stands on it | done | `src/Graphics/CreatureShadow.cpp`; test graphics/test_creature_shadow |
| The light is kept at least 45 degrees up and no closer than three of the creature's radii | done | `CreatureShadow.cpp`; test graphics/test_creature_shadow |
| The shadow fades out as the camera pulls away, between 50 and 80 creature radii | done | `CreatureShadow.cpp`; test graphics/test_creature_shadow |
| The creature's hair casts no shadow | done | `CreatureShadow.h` |
| Creature shadows don't fall on creatures themselves | done | `RendererInterface.h` |
| Several creatures' shadows at once, the nearest first | partial | openblack draws up to its own cap of the nearest; the game's limit is unconfirmed |

## Hand shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's silhouette is projected onto the land, darkening it | partial | `src/Graphics/HandShadow.cpp`. Differs: the light follows the sun and the shadow fades at night, where the game's light is fixed; the darkness was measured from a screenshot |
| The hand's shadow fades as the camera pulls away from the ground below it | done | `HandShadow.cpp` |

## Ground blobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each villager's foot has a short dark blob stretched over the land away from a low light, fading as it goes | partial | `src/Graphics/GroundBlobs.cpp`; test_ground_blobs. Feet are taken from the bind pose until villagers animate |
| Things in the sea or very low have no blobs | done | `GroundBlobs.h`; test_ground_blobs |
| Animals have ground blobs | todo | waiting for their foot points (port notes) |
