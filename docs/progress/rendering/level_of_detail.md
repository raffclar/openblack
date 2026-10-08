# Level of detail

The game's graphics detail levels and what they switch on, and how models and objects get simpler or stop being drawn
with distance. The land's own detail by distance is in [../terrain/](../terrain/); distance haze is in
[render_pipeline.md](render_pipeline.md).

**Progress: 4/14 done, 4 partial — 43%**

## Detail levels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Seven detail levels, 0 to 6, 4 by default; level 5 is the player's custom one | done | `src/Graphics/DetailLevel.h`, `--detail-level` in `src/main.cpp` |
| The sea's texture repeats more finely at higher levels, and is a still square at level 0 | done | `DetailLevel.h`, `src/Graphics/SeaRows.h`; test_sea_rows |
| Clouds are shown from level 3 up | done | `DetailLevel.h`, `src/Graphics/Renderer.cpp` |
| Cloud shadows have their own detail switch | partial | openblack ties cloud shadows to the clouds' switch (`Renderer.cpp`) |
| The fog setting by level | partial | turns the sky's dome towards the haze in an overcast; the land's haze classes and the fog key are todo (port notes) |
| The weather (rain and falling snow) is on only from level 3 up | done | `RainSystem.cpp`, `SnowfallSystem.cpp` |
| The temple's outside has its own detail level | todo | |
| The player picks custom detail settings in the options | todo | no options screen for them; see [../interface/](../interface/) |
| The detail settings are remembered between games | todo | see [../engine/](../engine/) |
| Low resolution textures for slower machines | todo | the game has a low resolution texture switch (unconfirmed which textures it cuts) |

## Models by distance and level

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Objects, villagers and animals use simpler meshes at lower detail levels | todo | always the most detailed (`Renderer.cpp` notes "choose the correct LOD") |
| A model's parts are picked by its level of detail mask | partial | only the parts of the most detailed level are drawn (`Renderer.cpp` submesh drawing) |
| Objects beyond a distance stop being drawn | todo | openblack draws everything; the game's distances are unconfirmed |
| Objects off screen are skipped | n/a | openblack leaves this to the GPU; it only affects speed |
| How far the camera can see before the world is cut off | partial | openblack's far plane is its own; the game's is unconfirmed |
