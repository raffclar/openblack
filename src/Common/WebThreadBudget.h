/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <numeric>
#include <string_view>

/// The threads that the game can run at one time in the browser.
///
/// In the browser, each thread of the game is a web worker. The page makes a set of workers at the start
/// (PTHREAD_POOL_SIZE in src/CMakeLists.txt). A thread past this set gets a new worker, and the new worker loads the
/// program from the site in the middle of play. After a deploy, the site has a different program or none, and the
/// thread does not start. Thus the page must make a worker for each thread that the game can run at one time.
///
/// Each part of the game that starts threads has a row in the list below, with the most threads that it runs at one
/// time. When you add a thread to the game, add it to the list. The build stops when the sum is more than the workers of
/// the page.
namespace openblack::web_threads
{

/// The most threads that all worker pools share in the browser (see WorkerPools)
inline constexpr size_t k_SharedWorkers = 6;
/// The most threads of the load queue (see LoadQueue)
inline constexpr size_t k_LoadWorkers = 6;

struct Part
{
	std::string_view name;
	size_t threads;
};

inline constexpr std::array k_Parts {
    Part {.name = "the game thread (main runs on a thread of its own)", .threads = 1},
    Part {.name = "the proxy thread of the file system of Emscripten", .threads = 1},
    Part {.name = "the flush of the log", .threads = 1},
    Part {.name = "the buffered log file", .threads = 1},
    Part {.name = "the render thread", .threads = 1},
    Part {.name = "the OpenAL mixer", .threads = 1},
    Part {.name = "the sound output to the audio worklet", .threads = 1},
    Part {.name = "the music decoder", .threads = 1},
    Part {.name = "the custom music player (open and decode)", .threads = 2},
    Part {.name = "the shared worker threads of the worker pools and the shadow bake", .threads = k_SharedWorkers},
    Part {.name = "the load queue", .threads = k_LoadWorkers},
    Part {.name = "the side threads: turn, checksum, footprint search, shadow test", .threads = 4},
    Part {.name = "the jobs of the land load: walk map, fight map", .threads = 2},
    Part {.name = "the background jobs: creature blends, shadow bake, temple exterior", .threads = 3},
    Part {.name = "the save writers: saved game, creature carry over, creature page", .threads = 3},
    Part {.name = "the video frame queue", .threads = 1},
    Part {.name = "the block readers of the videos that stream: the video that plays, the intro, the falling spell film",
          .threads = 3},
    Part {.name = "the inspector, in builds with the inspector only", .threads = 1},
};

/// The most threads that the game runs at one time in the browser
inline constexpr size_t k_MostThreads = std::accumulate(k_Parts.begin(), k_Parts.end(), size_t {0},
                                                        [](size_t sum, const Part& part) { return sum + part.threads; });

#if defined(OPENBLACK_WEB_THREAD_POOL_SIZE)
static_assert(k_MostThreads <= OPENBLACK_WEB_THREAD_POOL_SIZE,
              "The page makes fewer workers than the threads that the game can run at one time: make PTHREAD_POOL_SIZE "
              "in src/CMakeLists.txt larger");
#endif

} // namespace openblack::web_threads
