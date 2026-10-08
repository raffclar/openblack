# Footprints and marks on the land

What is printed onto the land: the paved ground of buildings, river beds, the creature's footprints, and the rubble
and scars left by blasts.

**Progress: 12/17 done, 2 partial — 76%**

## Building footprints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A building or feature whose model carries a footprint prints its texture onto the land under it | done | `Renderer::DrawFootprintPass` (`src/Graphics/Renderer.cpp`), `shaders/fs_footprint.sc` |
| Footprints are blended into the land's colour, not drawn on top as decals | done | `shaders/fs_terrain.sc` s3_footprints |
| The footprint goes when the building is removed and appears when it is placed | partial | the footprint pass is rebuilt from the models standing; checked for loading, not for buildings built or destroyed in play (unconfirmed) |
| River beds are printed the same way | done | see rivers.md |
| Fields print their furrows | partial | fields are drawn (`src/3D/FieldCrop.cpp`); whether their ground is a footprint is unconfirmed, see ../building/ |

## Creature footprints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature leaves a print under its lower foot as each footstep falls | done | `FootprintSystem` (`src/ECS/Systems/Implementations/FootprintSystem.cpp`); test `CreatureFootprints.LowerFootGetsThePrint` |
| Each species has its own print, sized to the creature | done | test `CreatureFootprints.SpeciesCells` |
| Left and right feet print mirrored | done | test `CreatureFootprints.LeftFootFlipsThePicture` |
| Prints fade away over about five seconds, in steps | done | tests `CreatureFootprints.GoneInAboutFiveSeconds`, `FadesInStepsOfAtLeast200Ms` |
| When the trail is full, new prints are dropped rather than old ones taken | done | test `CreatureFootprints.FullTrailDropsNewPrints` |
| On the first of April every creature leaves smiley faces | done | test `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize` |
| Prints are not saved with the game | done | as the game; `FootprintSystemInterface.h` |
| Puffs of dust rise from the creature's feet as it walks | todo | See ../creature/ |

## Scars and rubble

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A blast leaves a heap of rubble that lies a while and fades in its last second | done | `src/ECS/Components/GroundMark.h`, `ExplosionSystem.cpp` |
| The hand gripping the land throws up a puff of dust | done | `src/3D/GripLandscapeEffect.cpp`; see ../hand/ |
| A spinning vortex marks the ground it passes over | todo | see ../miracles/ (tornado) |
| Burnt ground under a fire or fireball (unconfirmed whether the game marks the land) | todo | (unconfirmed) |
