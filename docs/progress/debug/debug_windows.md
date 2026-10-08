# Debug windows

openblack's own debug interface: the menu bar along the top of the screen and the windows it opens, for looking inside
the game while it runs. None of this is in the original game; it is tracked because it is part of openblack's progress.
The creature tools, the testbed, the editor and the diagnostics have their own files.

**Progress: 25/31 done, 3 partial — 85%**

## Menu bar

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The debug interface is drawn with Dear ImGui over the game, in its own render view | done | `src/Debug/Gui.cpp` |
| Load a story land, a playground land or the creature testbed from the menu, with each land's description | done | `Gui.cpp` ("Story Islands", "Playground Islands", "Creature Testbed") |
| Set the time of day and the player's alignment, and read the sky's and camera's alignment | done | "World" menu |
| Open the editor (F2) and each debug window | done | "Debug" menu |
| Show villagers' names, details and states over them, with a debug view | done | "Villager Names" menu; `config.showVillagerNames` and friends |
| Turn the sky, water, land, objects and sprites on and off; wireframe, bounding boxes, footpaths and streams; an overlay of the game's detail settings | done | "View" menu |
| Switch the hand between right and left handed | done | "Hand" menu |
| Change and reset the field of view | done | "Field of View" menu |
| Slow down or speed up the game, by presets or a multiplier | done | "Game Speed" menu |
| Take a screenshot to a file | done | "Capture" menu |
| Quit, and the frame rate shown on the bar | done | |
| The system cursor stays hidden; the game's hand or pointer shows over the debug windows | done | `Gui::Create`, `GameInterface::Draw` |

## Windows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Profiler: the frame's CPU and GPU stages, with the draw toggles | done | `src/Debug/Profiler.cpp` |
| Mesh pack viewer: every mesh with its bounding box, skeleton and animations, and spawning it | done | `src/Debug/MeshViewer.cpp` |
| Texture viewer: every texture of the packs | done | `src/Debug/TextureViewer.cpp` |
| Console: the log, and land script commands typed in with completion and history; what is under the mouse | done | `src/Debug/Console.cpp` |
| Land: block and country counts, the small bump strength, and dumping the land's textures and height map | done | `src/Debug/LandIsland.cpp` |
| Path finding: send a picked villager somewhere by teleport, straight walk or footpath | done | `src/Debug/PathFinding.cpp` |
| Audio: every sound and music pack to play, the emitters, the voice banks, and the ambient loops, one-shots, climates and storms | done | `src/Debug/Audio.cpp` |
| Temple: go to each room of the temple | done | `src/Debug/Temple.cpp` |
| Camera: run the game's camera paths | done | `src/Debug/Camera.cpp` |
| Weather: force rain, snow and storms over the island from presets or by hand | done | `src/Debug/Weather.cpp` |
| Magic: running miracles, any miracle cast or powered up, prayer power, dispensers and bubbles, creatures' spells, particles | done | `src/Debug/Magic.cpp`, `MagicMiracles.cpp`, `MagicParticles.cpp` |
| Gestures: the path drawn, its best template and score, drawing or sending gestures | done | `src/Debug/Gestures.cpp` |
| Key bindings: every action, its binding and whether it is built yet; rebinding and pressing actions | done | `src/Debug/KeyBindingsWindow.cpp` |
| A music window showing the land's music, its tracks and what plays next | partial | the Audio window plays music packs; the game music's choice is not shown |
| A window of a land's game state hash, to check two runs stay the same | todo | listed in the port notes as todo |
| A fixed clock for running the game turn by turn at a set rate | partial | the editor steps one turn at a time; there is no fixed clock |
| Memory use by system | todo | listed in the port notes as todo |
| A window for the help system, the advisors and their messages | todo | waits for the help system (../interface/help_system.md) |
| A window for the land scripts' challenges, their state and their scrolls | partial | the editor's script debugger shows the running tasks ([script_debugger.md](script_debugger.md)), not challenges as such |
