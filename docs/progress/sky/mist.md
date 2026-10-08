# Mist and haze

Banks of mist the land's scripts lay over valleys and swamps, made of puffs of the smoke texture facing the camera, and
the distance haze that fades the world.

**Progress: 11/17 done, 1 partial — 68%**

## Map mists

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script lays puffs of mist with a position, height, colour, size and edge shrink | done | `FeatureScriptCommands::CreateMist`, `MistArchetype` (e4b54b86) |
| Each puff faces the camera | done | `src/ECS/Components/Mist.h` |
| Puffs animate through sixteen frames of the smoke texture, starting at a random frame | done | `src/3D/Mists.cpp`; `test/test_mists.cpp` |
| Puffs that shrink edge on are round from above and flatter seen level | done | `src/3D/Mists.cpp`; test |
| Edge-shrinking puffs are lit from above, the others by the land's light where they stand | done | `src/ECS/Components/Mist.h` |
| The land cells' colour adds to a mist's light | done | 1bba0e52 |
| Mists are sorted with other translucent things, farthest first | done | 0917023b |
| The game moves on only the mists in view; openblack moves them all | done | `MistSystem.cpp`; looks the same |
| Challenge scripts create a mist with a colour, scale and transparency | todo | `CHLApi.cpp` CREATE_MIST is a stub |
| Challenge scripts fade a mist's scale and transparency over a time | todo | `CHLApi.cpp` SET_MIST_FADE is a stub |
| Scripts read how far an object or mist has faded | todo | `CHLApi.cpp` GET_OBJECT_FADE is a stub |
| Mists are saved with the game | todo | See ../engine/ |
| Creatures notice mists as objects in the world | todo | See ../creature/ |

## Distance haze

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world fades into haze with distance from the camera | done | 4b1a1cc5 |
| The haze colour is a third of the land's and its strength follows the land's brightness | done | `src/3D/LandLightTable.cpp` |
| The haze closes in at dusk and under a storm | done | `src/3D/LandLightTable.cpp`, ab345ff0 |
| Particle mists from spells and effects | partial | 2b97bbe0; see ../rendering/ |
