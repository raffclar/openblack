# Editor

openblack's in-game editor, toggled with F2: a tool bar, an outliner, an inspector and tabs for the palette, the
testbed's scenarios, the scripts and their log, round the view of the world, where the mouse picks, moves, turns and
places things. The scripts tabs are in [script_debugger.md](script_debugger.md).

**Progress: 25/32 done, 0 partial — 78%**

## Layout

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| F2 opens and closes it; closed, the game runs as without it | done | `src/Editor/EditorWindow.cpp` |
| Tool bar along the top, wrapping onto more rows on a narrow window | done | `EditorWindow::DrawToolbar` |
| Outliner, inspector and the bottom tabs can each be shown or hidden | done | |
| Its keys only act while it is open and the mouse is over the world | done | `EditorWindow::TakesEvent` |
| Its colours shared by every panel | done | `src/Editor/EditorStyle.h` |

## Tools and time

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Select (W), move (E), rotate (R) and the game's own hand (H) | done | `EditorWindow.cpp` |
| Dragging a picked thing over the land, and turning it about the up axis | done | `src/Editor/EditorMath.cpp`; tests `EditorMath.*` |
| Snapping for moving and turning, set from the tool bar | done | |
| Duplicate (D) and Delete, removing what the thing alone owns | done | `src/Editor/EditorEntities.cpp` |
| Put a thing back on the ground | done | "Ground" |
| Pause and play the game, and step one turn (.) while paused | done | |
| Boxes round the picked thing and the one under the mouse | done | |
| Undo and redo | todo | |
| Picking several things at once | todo | |

## Cameras

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A free camera, an orbit round the picked thing (O) and a follow camera (Shift+O) | done | `src/Camera/EditorCameraModel.cpp`, `EditorMath.cpp` |
| Bring the picked thing into view (G) | done | |
| Camera move speed stepped up and down and reset (+, -, 0) | done | `src/Camera/KeyboardMoveSpeed.h` |

## Outliner and inspector

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Everything on the land by kind, searched by words; a click picks, a double click brings into view | done | `src/Editor/Panels/OutlinerPanel.cpp`, `src/Editor/EditorOutline.cpp`; tests `EditorOutline.*` |
| The list is read twice a second and only the visible rows are drawn | done | |
| The picked thing's place, turn and size to change, and what it is made of | done | `src/Editor/Panels/InspectorPanel.cpp` |
| For a creature, every section of the creature tools | done | [creature_tools.md](creature_tools.md) |
| For a villager, its life and its states | done | |
| For a building, a town or a worship site, their own figures | todo | |

## Palette

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Things to place by kind: creatures, villagers and buildings by tribe, trees, features, mobile objects and statics, dispensers and bubbles | done | `src/Editor/Panels/PalettePanel.cpp`, `src/Editor/EditorPalette.cpp`; tests `EditorPalette.*` |
| Names made readable from the game's tables | done | |
| A picked item follows the mouse until a click puts it down, turned by the wheel or [ and ] | done | |
| A whole list laid out at once in rows | done | |
| A building goes to the nearest town, or a new one of its tribe | done | `EditorEntities.cpp` |

## Changing the land itself

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Saving what was placed back to the land's script or a file of its own | todo | changes last until the land is reloaded |
| Raising and lowering the land, and painting its countries | todo | |
| Drawing footpaths and streams | todo | they can be shown, not edited |
| Placing the land's script markers, scrolls and challenge places | todo | |
