# Black & White 1 notes for openblack

Welcome. This wiki collects what was found out about how the original Black & White (2001) works, read from
`runblack.exe` (unofficial v1.42 on top of v1.20, W120 layout) and from the original data, and how openblack rebuilds
it. Start with the area you are interested in below; every page opens with what it covers and a table of contents.

- [Pages by area](#pages-by-area)
- [Where do I look for…?](#where-do-i-look-for)
- [How to read the pages](#how-to-read-the-pages)
- [How the code is written](#how-the-code-is-written)
- [External references](#external-references)

## Pages by area

**Engine**

- [openblack-internals.md](openblack-internals.md): where things are in our code, building, tests, debug and test environment variables, known traps.
- [engine-loop.md](engine-loop.md): game turns and frames, the original loop call by call, the interface's packets, random streams, state hash and fixed clock.
- [engine-math.md](engine-math.md): coordinates and cells, distances and sigmoids, object size, the game clock, terrain height, LH matrices, Zoomer.
- [tooling.md](tooling.md): disassembly, symbols, openblack tools, data formats, LND maps.
- [parity.md](parity.md): each stage of the original graphics engine and its status in openblack.
- [original-frame.md](original-frame.md): the original frame: draw order, render modes, states, levels of detail.

**World**

- [map-loading.md](map-loading.md): map loading and the map script: CHL CREATE, fog, herds, script objects, towns and citadel.
- [land-script-save.md](land-script-save.md): writing land-script lines: the exe's line format and the per-class writers.
- [water.md](water.md): water cells and queries, water in scripts, sinking and drowning, sharks, the fish puzzle, the missionaries' boat.
- [trees.md](trees.md): trees and forests: uprooting, replanting, wood, growth, drawing, fire, sacrifice.
- [buildings.md](buildings.md): buildings and towns: resources held by objects, town stores, life and damage, plans and building sites.
- [objects-and-resources.md](objects-and-resources.md): piles and pots, the store, static objects and rocks, fields, pick-up sounds.
- [physics.md](physics.md): thrown objects, collisions, damage, floating, dropping from the hand, buildings and rocks that break.

**Life**

- [villagers.md](villagers.md): villagers: original fields, flags, creation, states, jobs, towns' desires, death.
- [animals.md](animals.md): animals: the original AI (herbivores, predators, birds, flocks), clips per species, remaining differences.
- [creature.md](creature.md): the creature: the ported groundwork (body, mind, movement, fight, leash, audio, follow camera), its random streams, stand-ins and what is unknown.

**Magic and weather**

- [magic.md](magic.md): the magic core: info.dat tables, spell life cycle, casting from the hand and gestures, worship, influence, alignment, fire.
- [miracles.md](miracles.md): each miracle: food and wood, water, heal, forest, flocks, fireball and lightning, shields, teleport, storm and tornado.
- [particles.md](particles.md): the particle engine (PSys): types, classes, hierarchies, creators, particle sound.
- [day-night-weather.md](day-night-weather.md): the day and night clock, night lights, climate, storms and rain.
- [vortex.md](vortex.md): the vortex objects, what they swallow and fling, objects between lands, what a tornado carries.

**Hand and interface**

- [hand-and-interface.md](hand-and-interface.md): the hand: placement, states, grabbing, throwing, the object under the cursor.
- [intro.md](intro.md): the Land 1 intro and the tutorial: dialogue texts, advisor spirits, help events, the family in high detail.
- [script-camera.md](script-camera.md): the script camera: zoomers, camera opcodes, arrival rule, FOV, releasing control.
- [camera-tracks.md](camera-tracks.md): `Data\camera.edt` cameras and tracks, and the walk paths of mobile objects.

**Presentation**

- [rendering.md](rendering.md): drawing the world: terrain, sea and coast, light, haze, sky and clouds, shadows on the terrain, text.
- [rendering-objects.md](rendering-objects.md): drawing models: materials, lighting, sprites, reflections, shadows, LOD, smoke, and the shared drawing systems.
- [animation.md](animation.md): villager and animal animation: clips per state, speed, size, clip sounds, drawing between turns.
- [audio.md](audio.md): the audio engine, banks and formats, music, voices and texts, audio script functions.
- [video.md](video.md): the Bink videos: when each one plays, pacing, pause, widescreen, fade, skipping, audio.

**Data**

- [mods.md](mods.md): modified data packs: how the installation's `AllMeshes.g3d` differs from the original, and ideas waiting for a mod SDK.

## Where do I look for…?

| Topic | Page |
|---|---|
| An address or symbol of `runblack.exe`, the disassembly scripts | [tooling.md](tooling.md) |
| Terrain height, coordinates, matrices, object radius and height, the turn and the frame dt | [engine-math.md](engine-math.md) |
| Picking up, dropping, throwing, the cursor | [hand-and-interface.md](hand-and-interface.md) |
| Food and wood, store, fields | [objects-and-resources.md](objects-and-resources.md) |
| Trees and forests (tug, replanting, growth, fire, sacrifice) | [trees.md](trees.md) |
| What the map script (CHL) creates, fogs, herds, street lamps, towns | [map-loading.md](map-loading.md) |
| Hits, damage, buildings that break, dropping from the hand | [physics.md](physics.md) |
| Vortexes, objects carried by the tornado | [vortex.md](vortex.md) |
| Animations and speed of villagers and animals | [animation.md](animation.md) |
| Villager data and states | [villagers.md](villagers.md) |
| Animal behaviour | [animals.md](animals.md) |
| The creature: info.dat tables, mind files, its random draws, stand-in constants | [creature.md](creature.md) |
| Time of day, lit windows, weather, storms, rain | [day-night-weather.md](day-night-weather.md) |
| How the world is drawn (terrain, sea, sky); whether it already matches the original | [rendering.md](rendering.md), [parity.md](parity.md) |
| How a model is drawn (materials, lighting, reflections, shadows, sprites, smoke) | [rendering-objects.md](rendering-objects.md) |
| Which API to use for a billboard, an animated texture, something stuck to the ground, a blend mode or the transparent order (do not hand-write it) | [rendering-objects.md](rendering-objects.md) (sections for each system), [rendering.md](rendering.md#haze-and-land-light-the-common-api) |
| Order of the original frame | [original-frame.md](original-frame.md) |
| Magic: spells and chants, casting from the hand, gestures, worship, influence, alignment, reactions, fire | [magic.md](magic.md) |
| A specific miracle (food, water, heal, forest, flocks, fireball, lightning, shields, teleport, storm, lightning explosion) | [miracles.md](miracles.md) |
| Particles: types, PSys classes, creators, particle sound, which rule is where | [particles.md](particles.md) |
| Which cell is water; nearest coast, river or drinking water; the creature's `LandAvoid` | [water.md](water.md#water-cells-seacells) |
| Sinking and drowning, sharks, the fish puzzle, the missionaries' boat, waterfall, what sounds in the water | [water.md](water.md) |
| Cameras and tracks of `camera.edt`, shark routes | [camera-tracks.md](camera-tracks.md) |
| How the script moves the camera (MOVE_CAMERA_*, HAS_CAMERA_ARRIVED, lenses) | [script-camera.md](script-camera.md) |
| The intro video or the spell-drop video, skipping it with ESC, the .bik files | [video.md](video.md) |
| The Land 1 intro, the advisors, the dialogue texts, the tutorial's script functions | [intro.md](intro.md) |
| The installation's modified meshes and textures | [mods.md](mods.md) |
| Test hooks (`OPENBLACK_*`), building, debugging | [openblack-internals.md](openblack-internals.md) and the «Test hooks» section of each page |

## How to read the pages

- Everything stated is verified in the executable or measured, unless it is marked:
  - **(inferred)**: deduced from the code or the data, but not read directly;
  - **(approximate)**: openblack comes close to the original but does not match it exactly.
- Every constant or rule carries the address it was read at, or its source data.
- Every page ends with a **Pending** section: what is not ported yet, what is still unverified, and open questions.
- A topic lives on a single page; the others link to it. Addresses and figures are never deleted when editing.

## How the code is written

openblack is a replica: first everything original, verified in the executable, with no guesses. The code is modern
C++20 on EnTT: state lives in ECS components and Locator services, assets load through the resource caches, and
comments describe behaviour in plain English, with no decompiled names or addresses (those belong in this wiki). The
conventions are in [the refactor guide](../refactor/README.md), and how a change is verified (tests, the fidelity run,
pixel-identical screenshots) in [TESTING.md](../refactor/TESTING.md).

## External references

- [openblack/bw1-decomp](https://github.com/openblack/bw1-decomp): matching decompilation of `runblack.exe`;
  `config/BW1W120/symbols.txt` gives names to the addresses.
