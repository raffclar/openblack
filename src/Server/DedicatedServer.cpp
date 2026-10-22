/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DedicatedServer.h"

#include <charconv>
#include <cstdlib>

#include <algorithm>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Debug/SimRun.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "Locator.h"

namespace openblack::server
{
namespace
{

constexpr std::string_view k_TurnWord = "turn ";
constexpr std::string_view k_ChecksumWord = " checksum ";

} // namespace

Arguments GameArgumentsFor(const ServerSettings& settings)
{
	Arguments args {
	    .executablePath = "openblack-server",
	    .windowWidth = 0,
	    .windowHeight = 0,
	    .vsync = false,
	    .displayMode = windowing::DisplayMode::Windowed,
	    // No window and no device: the renderer makes its resources and draws nothing
	    .graphicsBackend = GraphicsBackend::Noop,
	    .gamePath = settings.gamePath.string(),
	    .guiScale = 1.0f,
	    .numFramesToSimulate = 0,
	    .logFile = settings.logFile,
	    .logLevels = {},
	    .startLevel = settings.land,
	    .startTestbed = settings.testbed,
	    .background = true,
	    .dedicatedServer = true,
	    .skipLogos = true,
	    .newGameStart = settings.opening,
	    .seed = settings.seed,
	    .profileFolder = settings.profileFolder,
	};
	std::ranges::fill(args.logLevels, settings.logLevel);
	// The clock of the server: each frame takes a fixed game time. Unless the run is fast, each frame waits until the
	// wall clock gives that time. No pointer moves the hand.
	sim_run::Request simulation {};
	simulation.settings.fast = settings.fast;
	simulation.settings.drawEvery = 0;
	simulation.settings.paceToWallClock = !settings.fast;
	simulation.settings.untilTurn = settings.turns;
	simulation.settings.stillHand = true;
	simulation.frameTime = sim_run::FrameTimeFromMs(settings.frameMilliseconds);
	args.simulation = simulation;
	return args;
}

std::string ChecksumLine(uint32_t turn, uint64_t checksum)
{
	return fmt::format("{}{}{}{:016x}", k_TurnWord, turn, k_ChecksumWord, checksum);
}

std::optional<std::pair<uint32_t, uint64_t>> ParseChecksumLine(std::string_view line)
{
	const auto start = line.find(k_TurnWord);
	if (start == std::string_view::npos)
	{
		return std::nullopt;
	}
	line.remove_prefix(start + k_TurnWord.size());
	uint32_t turn = 0;
	const auto turnEnd = std::from_chars(line.data(), line.data() + line.size(), turn);
	if (turnEnd.ec != std::errc {})
	{
		return std::nullopt;
	}
	line.remove_prefix(static_cast<size_t>(turnEnd.ptr - line.data()));
	if (!line.starts_with(k_ChecksumWord))
	{
		return std::nullopt;
	}
	line.remove_prefix(k_ChecksumWord.size());
	uint64_t checksum = 0;
	const auto checksumEnd = std::from_chars(line.data(), line.data() + line.size(), checksum, 16);
	if (checksumEnd.ec != std::errc {})
	{
		return std::nullopt;
	}
	return std::pair {turn, checksum};
}

int Run(const ServerSettings& settings, const std::atomic<bool>& stop,
        const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn)
{
	auto game = std::make_unique<Game>(GameArgumentsFor(settings));
	if (!game->Initialize())
	{
		return EXIT_FAILURE;
	}
	// The server makes the checksum of the synced world at the end of each turn, as each player's game does in a
	// network game
	auto& checksums = Locator::syncChecksumSystem::value();
	checksums.SetEnabled(true);
	checksums.SetTurnListener([&onTurn](const ecs::systems::SyncChecksumSystemInterface::TurnChecksum& turn) {
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "{}", ChecksumLine(turn.turn, turn.checksum));
		if (onTurn)
		{
			onTurn(turn.turn, turn.checksum);
		}
	});

	int exitCode = EXIT_SUCCESS;
	switch (game->StartRun())
	{
	case Game::RunStart::Failed:
		exitCode = EXIT_FAILURE;
		break;
	case Game::RunStart::Done:
		break;
	case Game::RunStart::Play:
		while (!stop.load() && game->RunFrame())
		{
		}
		if (stop.load())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The server stops on a signal");
		}
		game->EndRun();
		break;
	}
	checksums.SetTurnListener({});
	game.reset();
	return exitCode;
}

} // namespace openblack::server
