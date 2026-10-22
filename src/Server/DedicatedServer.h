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

#include "Game.h"

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
	/// The folder of the server's own profile. It is never a folder of a player.
	std::filesystem::path profileFolder;
	/// The file of the log, or "stdout"
	std::string logFile {"stdout"};
	spdlog::level::level_enum logLevel {spdlog::level::info};
};

/// The arguments of the game for a run of the server: no window, no device for the renderer or the audio, and a clock
/// with a fixed frame time that waits for the wall clock unless the run is fast
[[nodiscard]] Arguments GameArgumentsFor(const ServerSettings& settings);

/// The line of the log for the checksum of one turn, which the tools read
[[nodiscard]] std::string ChecksumLine(uint32_t turn, uint64_t checksum);

/// Reads the turn and the checksum from a line of the log, when the line has them
[[nodiscard]] std::optional<std::pair<uint32_t, uint64_t>> ParseChecksumLine(std::string_view line);

/// Runs the world of the server until it gets to its last turn, or until the stop flag goes on. The function gets the
/// checksum of each turn. Gives the exit code of the program.
int Run(const ServerSettings& settings, const std::atomic<bool>& stop,
        const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn);

} // namespace openblack::server
