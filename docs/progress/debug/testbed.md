# Testbed

The creature testbed: a flat land with a lake, made by openblack, and the scenario runner that sets it up and plays a
scripted timeline on it, from the debug window, the editor or the command line. What the scenarios cover is in
[testbed_scenarios.md](testbed_scenarios.md).

**Progress: 20/23 done, 0 partial — 87%**

## The testbed land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A flat land with a lake, loaded from the menu or `--testbed` instead of a story land | done | `Game::LoadTestbed` in `src/Game.cpp`; test `test_flat_land.cpp` |
| No land script runs on it, so its clock and weather stay as set | done | `Game::LoadTestbed` stops the script tasks |
| The player has a ring of influence and plenty of prayer power on it | done | `Game::LoadTestbed` |
| A grid of every miracle's dispenser laid out in front of the camera, kept or cleared per scenario | done | `src/Debug/TestbedDispenserGrid.cpp`; tests `TestbedDispenserGrid.*` |
| Emptying the testbed of everything put on it | done | "Empty testbed" in `src/Debug/TestbedScenarios.cpp` |

## Scenarios as data

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each scenario is plain data: a name, a description, the facet it shows and an id | done | `src/Debug/TestbedScenarioRegistry.h`; tests `TestbedScenarios.*` |
| The weather, the hour and whether the clock runs, the body's game speed, fainting and fights by themselves | done | |
| The player's alignment and prayer power as it starts | done | |
| Where the camera starts: the testbed's view, an overview of everything, behind or in front of one creature, or a placed shot | done | `Shot` in the registry |
| The creatures with their species, body, age, needs, desires, wounds, mind file and leash | done | |
| The objects, trees, features, villagers, food and wood, buildings and fields of a town, and animals about them | done | |
| Crowds of hundreds to thousands, laid out the same way every run | done | `src/Debug/TestbedCrowd.cpp`; tests `TestbedCrowd.*` |
| A timeline of commands at set seconds: moving, acting, needs, the hand, the leash, fights, learning, the camera keys, Creature Mode and the cave, seeds, gestures, miracles, key presses, the pointer | done | `Command::Kind` in the registry, applied by `src/Debug/TestbedScenarioRunner.cpp` |
| The pointer moved and pressed by the scenario in place of the mouse, through the game's own input | done | `GameActionMap` scripted pointer; tests `GameActionMap.QueuedPressGoesThroughTheKeyPath` |

## Running them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A list of scenarios by facet, searched, with Run, Restart and Stop | done | `src/Debug/TestbedScenarios.cpp`; also the editor's Scenarios tab |
| Overview and testbed camera views, following one of the creatures, and letting go of the camera | done | |
| A readout of each creature's needs and what it is doing as the scenario runs | done | |
| Saving a run's results to a file | done | "Save results" |
| `--scenario ID` starts the game on the testbed running a scenario | done | `src/main.cpp` |
| Benchmarks: a crowd warmed up for some frames, then measured, its results written as JSON and CSV, and the game quits | done | `--benchmark-warmup`, `--benchmark-frames`, `--benchmark-out`; `src/Debug/BenchmarkRecorder.cpp`; tests `BenchmarkRecorder.*` |
| A scenario checks its own outcome and reports pass or fail | todo | scenarios are watched by eye or by their logs; checks live in the unit tests |
| Scenarios on the story lands rather than the flat testbed | todo | |
| Scenarios recorded from play rather than written by hand | todo | |
