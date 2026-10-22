/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/common.h>

#include "Debug/SimRun.h"
#include "EngineConfig.h"
#include "Simulation/WorldStart.h"
#include "Story/NewGameChoice.h"

namespace openblack::server
{

/// The game time of one frame of the server. A turn of the game is 100 ms at the normal speed, so each frame plays one
/// turn, and the server plays 10 turns a second.
constexpr double k_DefaultFrameMilliseconds = 100.0;

/// What the command line of the dedicated server gives
struct ServerSettings
{
	/// The folder of the game's data
	std::filesystem::path gamePath;
	/// The script of the land under the game's Scripts folder
	std::string land {k_DefaultStartLevel};
	/// The flat testbed in place of a land
	bool testbed {false};
	/// How the new game on the first land starts, as the game's --skip-opening gives it
	std::optional<new_game_choice::NewGameStart> opening;
	/// The seed of every random number of the world. The date of the world is pinned with it.
	uint32_t seed {1};
	/// The server stops when the world gets to this turn. None for a server that runs until a signal stops it.
	std::optional<uint32_t> turns;
	/// The game time of each frame in milliseconds
	double frameMilliseconds {k_DefaultFrameMilliseconds};
	/// Tools: play the frames one after the other with no wait for the wall clock
	bool fast {false};
	/// The folder of the server's own profile. It is never a folder of a player. With none, the server plays as the
	/// profile of a tool's run, in the Profiles folder of its working folder.
	std::optional<std::filesystem::path> profileFolder;
	/// Tools: the server writes the run record of each turn to this file, as the game's --sim-record does, with the hash
	/// of each component. It starts the inspector on a free port of 127.0.0.1.
	std::optional<std::filesystem::path> recordPath;
	/// Tools: the run record also has the hash of each field of each component
	bool recordFields {false};
	/// The file of the log, or "stdout"
	std::string logFile {"stdout"};
	spdlog::level::level_enum logLevel {spdlog::level::info};
};

/// The configuration of the engine for the server: no window, no device for the renderer or the audio
[[nodiscard]] EngineConfig ServerConfig(const ServerSettings& settings);

/// The clock of the server: each frame takes a fixed game time and, unless the run is fast, waits until the wall clock
/// gives that time. No pointer moves the hand.
[[nodiscard]] sim_run::Request SimulationFor(const ServerSettings& settings);

/// The line of the log for the checksum of one turn, which the tools read
[[nodiscard]] std::string ChecksumLine(uint32_t turn, uint64_t checksum);

/// Reads the turn and the checksum from a line of the log, when the line has them
[[nodiscard]] std::optional<std::pair<uint32_t, uint64_t>> ParseChecksumLine(std::string_view line);

/// Runs the world of the server until it gets to its last turn, or until the stop flag goes on. The function gets the
/// checksum of each turn. Gives the exit code of the program.
int Run(const ServerSettings& settings, const std::atomic<bool>& stop,
        const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn);

} // namespace openblack::server
