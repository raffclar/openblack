# Diagnostics

openblack's ways of measuring and checking itself: the game's command-line switches for testing, logging, the
profiler, frame statistics, benchmarks, screenshots and crash reports.

**Progress: 17/19 done, 0 partial — 89%**

## Command line

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Point the game at the original's files, pick the land to start on, or the testbed | done | `src/main.cpp` (`--game-path`, `--start-level`, `--testbed`) |
| Window size, window mode, renderer backend, vertical sync, interface scale | done | `--width`, `--height`, `--window-mode`, `--backend-type`, `--vsync`, `--ui-scale` |
| The original's detail level, 0 to 6 | done | `--detail-level` |
| Run a set number of frames and quit, for automated runs | done | `--num-frames-to-simulate` |
| A screenshot at a given frame, to a given path | done | `--screenshot-frame`, `--screenshot-path` |
| Run a testbed scenario or benchmark | done | `--scenario`, `--benchmark-*`; see [testbed.md](testbed.md) |

## Logging

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Logs to a file, the terminal or the Android log | done | `--log-file` |
| A log level for each subsystem | done | `--log-level` |
| The log shown in the console window | done | `src/Debug/Console.cpp` |

## Measuring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A profiler of the frame's stages on the CPU and of the renderer's views on the GPU | done | `src/Profiler.cpp`, `src/Debug/Profiler.cpp` |
| Frame statistics logged every so many frames: average, 95th percentile and slowest frame, CPU and GPU, each stage | done | `--frame-stats`, `src/Debug/FrameStatsLog.cpp`, `src/Common/FrameStats.cpp`; tests `FrameStats.*` |
| Benchmarks summed up as mean, 95th percentile and slowest, written as JSON and CSV to compare runs | done | `src/Debug/BenchmarkRecorder.cpp`; tests `BenchmarkRecorder.*` |
| Screenshots from the menu bar | done | "Capture" in `src/Debug/Gui.cpp` |
| A hash of the game's state each turn to check two runs stay the same | todo | listed in the port notes as todo |
| Memory use by system | todo | |

## Crashes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every kind of crash (failed assertions, runtime errors, abort, terminate, fatal signals, system exceptions, fatal renderer errors) is caught | done | `src/Common/CrashHandler.cpp` |
| A report with a stack trace goes to stderr, the log and a file in a crashes folder, with a minidump on Windows | done | `src/Common/CrashReport.cpp`; tests `CrashReport.*` |
| The game exits with a non-zero code instead of waiting on a dialog, breaking into a debugger first if one is attached | done | |
| A switch keeps the system's own crash dialogs | done | `CrashHandler.h` |
