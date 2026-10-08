# Testing and verifying a refactor

A refactor must not change what the game does. This page describes the checks every change in this release passed,
and how to run them.

## 1. Unit and integration tests

- Tests live in `test/` and are grouped into one gtest executable per area: `test_core`, `test_villagers`,
  `test_town`, `test_magic`, `test_audio`, `test_ui_script`, `test_video`, `test_bgfx`, `test_game`, and a few
  standalone ones.
- Run them both ways, because they catch different problems:
  - `ctest` runs each test in its own process;
  - running each `test_*.exe` whole catches state that leaks from one test to the next.
- **Test services.** A gtest listener emplaces fresh state services before every test and resets them after it: random
  streams, clock, particle system, weather, physics objects, players and map script. A service is added to that list
  only when all three of these hold:
  1. it replaces a global that always existed;
  2. it starts empty, as in the game;
  3. it has no behaviour a test should fake.
- **Fakes and mocks.** Everything else is injected by the test itself, with fakes from `test/support` and
  `test/mock`. Tests never check a system through `Locator::X::value()`; the locator is only used to inject.
- **Game data.** Tests that need the original game's data are marked as integration tests and skip themselves when
  `OPENBLACK_GAME_PATH` / `OPENBLACK_TEST_GAME_PATH` is not set. Where possible a synthetic twin with in-memory data
  covers the same logic.

## 2. Behaviour is identical: the fidelity run

Run the game twice, once with the build before the change and once with the build after it, under the same conditions,
then compare.

### Deterministic conditions

| Variable | Purpose |
|---|---|
| `OPENBLACK_FIXED_FRAME_MS=16` | Fixed time step, so the run does not depend on the frame rate |
| `OPENBLACK_MOUSE_AT=0.5,0.5` | The hand stays at the screen centre |
| `OPENBLACK_IGNORE_REAL_INPUT=1` | Real mouse events are ignored, so a person at the machine cannot disturb the run |
| `OPENBLACK_TEST_SKIP_ANSWER=3,<frame>` | Answers the start-up requester with «keep my creature» at that frame |
| `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` | Fixed camera for screenshots |
| `OPENBLACK_CLOUD_SEED=1` | Fixed cloud seed |
| `OPENBLACK_TIME_OF_DAY=<hour>` | Fixed time of day, e.g. 22 for a night screenshot |

Use an **empty `Mods` folder**.

### What is compared

| Variable | Records |
|---|---|
| `OPENBLACK_STATE_HASH=<file>` | One line per turn, with a hash of each part of the world state (transforms, villagers, towns, alignment…). The `pools` part counts component storages and changes when a refactor adds a component type, so it is the only part allowed to differ. |
| `OPENBLACK_TRACE_GAME_RAND=1` | Every random draw, with its stream and game turn. The traces must be identical line by line. |

### Runs

- **Land 1** for 900 frames.
- **The land cycle:** Land 1 → Land 2 → Land 1. This catches state that should, or should not, survive a land load.

## 3. Visual checks

- **Land view.** Take a screenshot of the land view by day and by night, with the camera fixed, before and after the
  change, and compare the pixels: the result must be identical. For night shots, take one throw-away run first, because
  the first run after a build can differ (cold caches).
- **Start menu.** A screenshot of the start menu is informational only: tree foliage sway is not stable between runs.
- **Hand demos.** The hand demos (`OPENBLACK_TEST_HAND_DEMO`) and spell hooks (`OPENBLACK_TEST_SPELL`,
  `OPENBLACK_TEST_CAST`, `OPENBLACK_TEST_STORM_SHOT`) exercise input, gestures and spells. With
  `OPENBLACK_IGNORE_REAL_INPUT=1` they are deterministic, and their state hash and random trace must match too.
- **Temple interior.** `OPENBLACK_AUDIO_TEST_CITADEL=120` enters the temple at hook turn 120 of a fixed-step Land 1 run
  (`OPENBLACK_FIXED_FRAME_MS=33`, cursor fixed); a screenshot at frame 700 must match the reference pixel for pixel, and
  the run, which ends inside the temple, must exit cleanly (also with `120,200`, in and out again).

## 4. Logic only reached by input

Some code is only reached through real input, such as mouse button edges. Move the logic into a pure function without
changing it, write characterisation tests with fake events from a reading of the original code, then move the state
into its owner. The tests must pass before and after the move.

## 5. Performance

A refactor must not make the game slower. Compare the mean frame time of the old and new exe on Land 1, without any
tracing, without a fixed time step, and with no build running. The difference must stay within run-to-run noise
(about 5%).
