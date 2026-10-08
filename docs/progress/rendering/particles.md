# Particles

The particle engine that draws the game's effects from its particle files: the groups and atoms, the rules that move
them, the sprites, ribbons, beams, models, mists and light they are drawn as, and the order they are drawn in. What each
miracle's effect looks like is in [../miracles/](../miracles/); gesture trails in [../gesture/](../gesture/).

**Progress: 27/33 done, 2 partial — 85%**

## Effect files and the engine

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Effects are read from the game's particle files with their bitmaps | done | `components/psys`, `src/Particles/ParticleEffect.cpp`; test_particle_file, test_particle_effect |
| An effect's groups make collections of atoms, at its start or under each new atom, and a hierarchy group moves, turns and scales with its atom | done | `ParticleEffect.cpp`; test_particle_effect |
| Every rule the files name creates, moves, shapes, colours or ends particles | partial | about 110 of the game's rule and creator kinds run (`src/Particles/*Rules.cpp`, `ParticleClassRegistry.cpp`); missing: the mana path, the multiple pick-up and put-down emitter, fireworks, plasma, ropes, jointed atoms, spinning rings, stretching height, turning to the velocity, pulling into a vortex, the village centre's belief, effects laid over an object's model or light sheet, colour from the parent, playing an animation, bursting from the parent atom |
| Effects are started by type from the game's list of effects | done | `src/Particles/ParticleTypes.cpp` |
| Effects are cleared when a new land loads | done | `Game.cpp` resets the particle system |

## How particles look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sprites face the camera, cut from cells of a sheet | done | `src/Particles/ParticleSprites.cpp`; test_particle_draw |
| Sprites lie flat on the ground, turned by their own heading | done | `ParticleSprites.cpp`; test_particle_draw |
| Some sprites are tinted by the colour of the land under them | done | `ParticleSprites.h` |
| Sprites are added over what is behind them or blended over it, writing depth or not, as their creator says | done | `ParticleSprites.h` render modes |
| Ribbons run through chains of joints, their width turned to face the camera, meeting halfway at each joint | done | `src/Particles/ParticleDrawFrame.cpp`; test_particle_draw |
| Beams | done | `src/Particles/BeamMaths.cpp`; test_particle_beam |
| Models as particles: plain, with sliding or frame-playing textures, animated, or turned to face the camera and stretched | done | `src/Particles/ParticleCreators.cpp` |
| Models blended by volume and models whose texture rotates | todo | no creator for them |
| Mists | done | `ParticleCreators.cpp`, `src/Graphics/RendererParticles.cpp` |
| Light maps stamped on the land's colours under atoms, playing through frames | done | `ParticleCreators.cpp`, `ParticleDrawFrame.cpp` |
| Shadow maps darkening the land under atoms, as a cloud's shadow does | partial | `ParticleCreators.cpp`; not yet checked against the game |
| The casting player's symbol | done | `ParticleCreators.cpp` symbol sprite creator |
| Symbols of belief rising over worshippers | done | `src/Particles/ParticleBeliefRules.cpp`; see [../worship/](../worship/) |
| The mana path from a worship site | todo | its rule is missing; see [../worship/](../worship/) |
| Objects broken into flying pieces of their model | done | `src/Particles/ParticleBlast.cpp`, broken model pieces in `Renderer.h` |
| Surfaces of revolution: the swirl under a dispenser, the teleport's pool, vortices | done | `src/Particles/SurfaceOfRevolution.cpp`; test_miracle_surfaces |
| Sheets of light standing along a recognised gesture's trail | done | `src/Particles/LightSheet.cpp`; test_gesture_trail |
| Forked lightning | done | `src/Particles/LightningMaths.cpp`; test_lightning_maths |
| Glints sparkling on objects | done | `src/ECS/Systems/Implementations/ParticleWorldGlints.cpp`; test_particle_glints |

## Drawing order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Most effects are sorted piece by piece among everything else that blends, farthest first | done | `src/Particles/ParticleDrawPath.h`, `src/Graphics/ZSort.h` |
| A few spot visuals are drawn whole at their origin, in the order their collections hold them | done | `ParticleDrawPath.h` |
| The miracle in the hand is drawn just after the hand | done | `ParticleDrawPath.h` |
| Sprites sharing a sheet are drawn together | done | `RendererParticles.cpp` instanced sprites; openblack's batching |
| Effects show in the sea's reflection too | done | what the effects draw is gathered once for every pass (`Renderer.h`) |

## Sounds and other outputs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Particles play sounds that follow them, are heard late when far off as thunder is, and fade when let go | done | `src/Particles/ParticleSounds.cpp`; test_particle_sound_release; see [../audio/](../audio/) |
| Glows lit by particles go once nothing lights them | done | `ParticleSystem.cpp` |
| The effects of picking up and putting down several food or wood items at once | todo | the effect types exist (`ParticleTypes.cpp`) but their emitter rule is missing; see [../hand/](../hand/) |
| Effects are saved with the game and restored on load | todo | see [../engine/](../engine/) |
