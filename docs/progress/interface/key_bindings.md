# Key bindings

The game's actions and the keys and mouse buttons they are bound to, the Controls page that lists and rebinds them,
and the one-press shortcuts. What each camera or hand action does once pressed belongs to ../camera/ and ../hand/.

**Progress: 15/26 done, 6 partial — 69%**

## The bindings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's default keys and mouse inputs for each of its 33 actions, in the order the options list them | done | `src/Input/KeyBindings.h`; test `KeyBindings.DefaultsMatchTheOptionsScreen` |
| A key can need a held Ctrl, Shift or Alt; either side's modifier counts | done | `KeyBindings.cpp`; test `KeyBindings.EitherSidesModifierKeyCounts` |
| A binding with a modifier wins over the same key without one (Ctrl+S saves rather than showing details) | done | test `KeyBindings.ModifierBindingWinsOverPlainOne` |
| Letting go of a key ends every action bound to it | done | test `KeyBindings.LettingGoOfAKeyEndsAllItsActions` |
| Mouse buttons and the wheel can be bound to actions | done | test `KeyBindings.MouseMapsToActions` |
| Double clicks and both buttons together are read as their own inputs and can't be rebound | done | `UnbindableActionMap` in `src/Input/BindableActions.h`, `GameActionMap.cpp` |
| Land scripts can block actions for a while, which then read as not held | done | `GameActionMap::SetBlockedActions`, fed from the camera help in `Game.cpp` |
| The keys and buttons in use are told apart so the cursor is held still while the mouse turns the camera | done | `src/Input/CursorFreeze.cpp`; tests `CursorFreeze.*` |

## The Controls page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists every action with its key and mouse button, named as the game names them | done | `GameMenu.cpp`; test `GameMenu.ListsTheControls` |
| Picking an action and pressing a key or button binds it anew | partial | the page logs "not available yet"; rebinding works from the debug Key Bindings window (`src/Debug/KeyBindingsWindow.cpp`); test `GameActionMap.RebindingMovesTheAction` |
| Some keys can't be bound (the ones the game keeps for itself) | todo | (unconfirmed which) |
| Binding a key already in use takes it off the other action | partial | conflicts are found (test `KeyBindings.ConflictsAreFound`) but nothing resolves them |
| Load Defaults puts every binding back | partial | done in the debug window (`ResetKeyBindings`); the menu's button does nothing |
| The bindings are kept with the player's profile and read back when it is picked | todo | |
| A key's name is written as the game writes it, such as "Ctrl+S" | done | test `KeyBindings.Names` |

## One-press shortcuts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| F1, Help: asks the advisors about what the hand is over | todo | logs "not implemented"; see [help_system.md](help_system.md) |
| T, Talk: opens the chat line in a network game | todo | see ../multiplayer/ |
| Space flies the camera to the temple, and a second tap back | done | `src/Input/ShortcutKeys.cpp`, `src/Camera/ZoomToPlaces.cpp`; tests `ZoomToPlaces.*` |
| F3 flies the camera over the whole land, and back | done | same |
| F4 to F9 take the player inside the temple, to each room | done | handled by the temple ([../temple/](../temple/)) |
| C flies to the creature and F5 to its room | partial | owned by the creature mode work; see ../creature/ |
| N toggles villagers' names and S their details over them | partial | toggled as the game does (`ShortcutKeys.cpp`), but drawn by the debug overlay (`src/Debug/Gui.cpp`) rather than as the game draws them |
| L leashes or lets go of the creature; V and B pick the previous or next leash | partial | owned by the leash work; see ../creature/ |
| Ctrl+S quick saves and Ctrl+L quick loads | todo | log "not implemented"; see ../engine/ |
| Number keys jump to camera bookmarks and Ctrl with a number sets one | done | `CameraBookmarkSystem`; see ../camera/ |
| An action's key whose feature isn't built yet says so in the log | done | `ShortcutKeys::Update` (openblack only) |
