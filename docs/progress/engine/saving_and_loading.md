# Saving and loading

Saving the whole game to a slot and loading it back: the world with every object in it, the players and their creature,
the towns, the scripts, the camera, the weather and the temple. Saves live in the player's profile and are made from the
temple's save game room, by the quick save keys, by scripts, and by an autosave. openblack has no save-game system yet.

**Progress: 2/37 done, 3 partial — 9%**

## Slots

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Games are saved into numbered slots kept in the player's profile folder, one folder per slot | todo | no save system; `src/Serializer` only reads footpath data |
| A slot holds the world file, a second file with the scripts' state, and a small info file the save room shows (land, date) | todo | |
| Each save keeps a picture of the moment, at a high and a low resolution, shown on the save room's slot | todo | |
| The temple's save game room lists the slots and saves, loads and deletes, asking the player to confirm each | partial | the room's scroll is drawn with made-up facts (`src/3D/TempleScrolls.cpp`); the room itself: see ../temple/ |
| Quick save and quick load keys save to and load from a slot without going to the temple | todo | bound but marked not yet implemented in `src/Input/KeyBindings.h` |
| A challenge script can save the game into a given slot | todo | the native is a stub in `src/CHLApi.cpp` |

## Autosave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With autosave on, the game saves itself every 6000 turns (ten minutes of play) | todo | the menu's autosave box (`src/Gui/GameMenu.cpp`) is not acted on |
| The autosaves of each land take turns among three slots, so an older one always survives | todo | |
| There is no autosave in a network game, while paused, while help is showing a sequence, or on the multiplayer/skirmish lands (unconfirmed which land is excluded) | todo | see [../multiplayer/skirmish.md](../multiplayer/skirmish.md): the land left out is the Gods' Playground island; a skirmish does autosave. Once the game is over the automatic save and the quick-save on exit stop: see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| A save can be forced at once (for example when leaving a land) whatever the interval | todo | |
| The game can tell whether its own automatic save is there and readable, and load it to continue | todo | |

## What a save holds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A version string and the land's landscape file come first; loading opens that landscape again | todo | |
| Every object in the world is saved, and the links between objects are put back once all are loaded | todo | |
| The players, which one is the player, and the player's creature with its state | todo | |
| The game turn, the time of day and its speed, and the land's influence multipliers | todo | |
| The help system's progress and the player's help statistics | todo | |
| The scripts: every running script, its variables and where it is | partial | the script VM can write and read its state (`components/ScriptLibrary` save/restore state); nothing calls it |
| The camera, the land's starting camera position, camera help settings and the camera bookmarks | todo | |
| The weather's climate and the snow lying on the land | todo | |
| The hand and what it holds | todo | |
| The audio's state | todo | |
| The temple: which rooms are open, the challenge room's list, the hints already seen | todo | |
| The land's good and evil balance | todo | |
| Objects in flight keep their position, speed and spin | todo | |
| Miracles under way keep their place, strength, age and effects | todo | the fields are recorded in the miracle audit for when saves land |
| The random number streams' positions | todo | see game_loop_and_clock.md |

## Writing and reading safely

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A save is written to a temporary file and only replaces the old one once it is complete | todo | |
| A problem while saving is reported to the player | todo | |
| A save that failed to load twice (the game crashed loading it) is not offered again | todo | |
| Loading stops the scripts and clears the land before reading | todo | `Game::PrepareNewLand` clears a land for a new map, not for a save |
| After loading, the game clock carries on from the saved turn | todo | |
| A "saving" and "loading" box shows while the game works | todo | |
| A save's stored checksum is not checked on load, and older versions' saves are read as they are | todo | the original writes but does not check them |

## Creatures outside a save

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature, with its mind and body, goes with the player into the next land | todo | see ../creature/ |
| Creature mind files can be read and written in the game's format | done | `components/creaturemind`; `test_creature_mind_file`, `test_creature_mind_file_body` |
| A land's script can create a computer player's creature from a mind file | done | `FeatureScriptCommands::CreateCreatureFromFile` |
| The player can load a creature from a mind file | partial | only through the debug creature spawner's file dialog (`src/Debug/CreatureSpawnerMindFiles.cpp`) |
| The creature's details are exported to a web page in the profile for the online creature service | n/a | the online service no longer exists |
| Creatures shared with Creature Isle | n/a | expansion not supported |
