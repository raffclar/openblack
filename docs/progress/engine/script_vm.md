# Script virtual machine

The game's story, challenges and tutorial are written in Black & White's own script language, compiled into one program
(`challenge.chl`) that a small virtual machine runs alongside the world, a step each game turn, calling into the game
through several hundred native functions. Lands themselves are set up by simpler line-by-line land scripts. What the
scripts do, land by land, belongs to [../story/](../story/).

**Progress: 12/36 done, 21 partial — 62%**

## The compiled program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game loads its compiled script program (code, constant data, global variables, scripts and their parameters) at start | done | `components/ScriptLibrary` (`LHVMFile`), `Game::Run` |
| The scripts marked to start on their own are started when the program loads | done | `LHVM::LoadBinary` starts the auto-start list |
| The land control script is started for the game | done | `Game::Run` starts it by name |
| The challenge script sources compile to exactly the program the game ships | done | `components/lhvmcompiler`; `ChlRoundTrip.OriginalSourcesCompileToTheGamesProgram` |
| Creature Isle's version of the program format | n/a | expansion not supported |

## Running scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every running script gets a step each game turn, after the living and before the weather and the miracles | done | `LHVM::LookIn` from `Game::GameLogicLoop` |
| A script runs until it waits, sleeps or yields, and carries on from there next turn | partial | `LHVM::CpuLoop`; runs the story's scripts, not audited instruction by instruction |
| Arithmetic, comparisons, logic and jumps work on integers, floats, positions and objects | partial | opcode handlers in `LHVM.cpp`; not audited against the game |
| Converting between number types truncates as the game does | done | port notes (cast conversions) |
| Taking a value off the stack into a variable releases the object the variable held before | done | port notes (pop releases the old value) |
| A script's elapsed time is counted in tenths of a second as the game counts it | done | port notes (elapsed time ×0.1) |
| Native calls the game doesn't implement drop their arguments so later values don't shift | done | port notes (native calls dropping leftover arguments) |
| A script can start another and either wait for it to finish or carry on alongside it | partial | `Opcode24Call` (sync and async); not audited |
| Exception handlers (a block's "when"/"until" conditions) are checked each turn before the scripts' normal code, and can break out of their block | partial | `LHVM::LookIn`, exception opcodes; not audited |
| Global variables are shared by all scripts; each script has its own locals and parameters | partial | `LHVM`; not audited |
| Objects a script holds are reference counted so the game doesn't delete them under it | partial | `AddReference`/`RemoveReference` in the VM; the game objects' side not checked |
| A script can stop itself, all scripts, scripts of a type, or scripts by name | partial | `StopTask`, `StopAllTasks`, `StopTasksOfType`, `StopScripts` exist; few callers |
| Only some kinds of script run in some situations (for example help scripts versus challenge scripts) | partial | openblack always runs every kind |
| Scripts can ask for their own task number and the kind of script they are | partial | port notes: task number done, script type accessors wait for the help scripts |
| The whole script program can be rebooted (every script stopped and the auto-start ones started again), as the game does when asked to | todo | `LHVM::Reboot` exists, nothing requests it; port notes say openblack has no reboot yet |
| Scripts are stopped and their state kept when the game is saved, and restored on load | partial | the VM writes and reads its state; no save games (see saving_and_loading.md) |
| A script error is reported with its script and line, and the game carries on | partial | error callback logs in `Game::Run`; the game's own handling unconfirmed |

## Native functions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The native function table matches the game's, by name, order, arguments and results | done | `src/CHLApi.cpp`; `ChlLanguage.NativeTableMatchesTheGameBindings` |
| Each native function does what the game's does | partial | about 62 of 464 do something; the rest log "not implemented" (`src/CHLApi.cpp`); which ones each challenge needs: see ../story/ |
| Positions, objects, floats and strings are taken off and put on the stack as the game passes them | partial | stack helpers in `CHLApi.cpp`; not audited |

## Land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts are read line by line, each line a command with numbers, strings and quoted positions | done | `src/LHScriptX/Lexer.cpp`, `Script.cpp` |
| An unreadable line doesn't end the game: the land is kept as far as it got | partial | `Game::LoadMap` stops at the first bad line (unconfirmed whether the game skips just the line) |
| Version, land number, landscape, starting camera position and influence multipliers | done | `FeatureScriptCommands` |
| Towns, town centres, abodes, citadels and town belief | partial | towns, centres, abodes, citadels and belief done; planned abodes, belief caps, uninhabitable towns, congregation points todo |
| Villagers | partial | villagers by position done; town villagers, special villagers and plain villagers todo |
| Trees, forests, big forests, flowers, features, fields, pots, mobile objects and statics, street lights and lanterns, bonfires, mists | partial | these done; dead trees, plain forests, fish farms, walls, pitches, temporary pots, scaffolds, furniture todo |
| Streams and footpaths | partial | streams, stream points, footpaths and nodes done; linking footpaths and waterfalls todo |
| Weather climates | partial | climates with rain, temperature and wind done; storms todo |
| Animals, flocks, creature pens, worship sites, spell icons and dispensers, one-shot spells, fireflies, influence rings, arenas | todo | logged as not implemented, or empty; fireflies: [../nature/fireflies.md](../nature/fireflies.md) |
| Creatures from mind files, and computer players switched on | partial | creatures from files and the computer player switch done; personalities and creature likes todo |
| Land balance, night time and the game's opening messages | partial | global land balance and night time done; per-player balance, opening messages, artefacts and lost-town scale todo |
| Map scripts: the commands that set up a game rather than a land (players, date, turn length, which land and scripts to load, language) | todo | `MapScriptCommands.cpp` throws for each and is never run |
