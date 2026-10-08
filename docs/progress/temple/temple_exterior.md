# Temple exterior and entrance

The player's temple on the island: built at the start of the story, it changes its look with the player's alignment and
influence, and its entrance is the way inside.

**Progress: 9/16 done, 2 partial — 62%**

## Its look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's outside is one of three sizes by the player's share of influence, in five stages from evil to good | partial | `src/3D/TempleExteriorMorph.cpp`, `TempleExteriorSystem`; test `test_temple_exterior_morph`. The share of influence is not fed in yet (TODO in `TempleExteriorSystem.cpp`) |
| Its mesh is blended from the four nearest of those meshes | done | `TempleExteriorMorph`; test `test_temple_exterior_morph` |
| Its textures blend from evil to neutral to good | done | `TempleExteriorMorph`; test `test_temple_exterior_morph` |
| The look moves a step a turn towards the player's alignment, the rest of the way when near | done | `TempleExteriorSystem::UpdateTurn` |
| Each player's temple follows its own player | done | `TempleExteriorSystem`; how a temple is damaged, destroyed and lost: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| The first, unfinished temple of the opening, built by the villagers | todo | see ../story/land_1.md and ../building/construction.md; a cut early draft of the opening at the temple: [see_the_citadel.md](../story/silver_scrolls/see_the_citadel.md) |
| The temple has worship sites around it | todo | see ../worship/worship_sites.md |

## Going in and out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The entrance is its own object at the temple's door | done | `src/ECS/Archetypes/CitadelArchetype.cpp` (`TempleEntrance`) |
| The Action button on the entrance takes the player inside | done | `Game.cpp` picks the entrance under the cursor (`TempleExteriorSystem::EntranceAt`) |
| The camera flies to the entrance and in | todo | see ../camera/temple_camera.md |
| Scripts take the player in and out of the temple | done | enter/exit temple command (`src/CHLApi.cpp`) |
| Scripts ask whether the player is inside the temple and where its entrance is | todo | stubs in `src/CHLApi.cpp` |
| Leaving fades out to white and back on the island | done | `TempleInterior::FadeToWhite` |
| Inside, the world is paused, its ambience fades out and the temple's music plays | partial | the ambience fades (`AtmosAudio`); the temple's music see ../audio/music.md |
| Help is held back inside the temple | todo | see ../interface/help_system.md |
| A key takes the player straight into a room (the creature's cave) | done | F5, `CreatureCaveSystem` |
