# The shore

Where land meets the sea: the land's texture fades out towards the water, the lowest land lies flat at sea level, and
shallows can be waded.

**Progress: 9/10 done, 0 partial — 90%**

## Coast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's texture fades to clear from altitude 4 down to altitude 1, so the sea shows through at the coast | done | `src/3D/BlockTexture.cpp` coast alpha; `test/test_block_texture.cpp` |
| The fade is ragged by the land's noise, so the waterline wanders | done | `src/3D/BlockTexture.cpp` |
| Land at sea level next to the sea is flattened to height 0 | done | `LandIsland::GetDrawnAltitude` |
| Cells of open sea are not drawn as land | done | 10fa0be1 |
| Coast cells are flagged so fire, forests and spells treat them as wet | done | `FireSystem.cpp`, `ForestSystem.cpp` (coastLine and hasWater bits) |
| There are no breaking waves or foam lines on the shore | done | the game draws none (unconfirmed beyond play) |

## Wading and splashing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature wades in shallow water and keeps out of the deep | done | `CreatureLocomotionSystem.cpp`; see ../creature/ |
| Splash sprites rise round a creature's feet as it walks through water | todo | the game has a wet-feet effect per creature type; not ported |
| A creature's footsteps sound wet in water | done | `CreatureAudioSystem.cpp` |
| The hand gripping the land at the water makes a splash ring | done | `water_rings::HandSplash` (1ec9d2c9) |
