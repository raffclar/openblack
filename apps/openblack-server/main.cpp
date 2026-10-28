/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dedicated server of openblack: it runs the world of one land with no window, at 10 turns a second, and logs the
// checksum of the synced world at the end of each turn. With --listen it is the game server of a network game: the
// clients join with a ticket of the lobby, and the server orders their commands into its turns.

#include <csignal>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

#include <cxxopts.hpp>

#include "Common/BuildVersion.h"
#include "Common/CrashHandler.h"
#include "Common/DeveloperTools.h"
#include "Server/DedicatedServer.h"

namespace
{

/// The signal handler sets it, and the loop of the server stops at the end of its frame
std::atomic<bool> g_StopAsked {false};

extern "C" void StopOnSignal(int /*signal*/)
{
	g_StopAsked.store(true);
}

} // namespace

int main(int argc, char* argv[])
{
	// A crash writes a report to the crashes folder and ends the program. A server shows no dialog box.
	openblack::crash_handler::Install();

	cxxopts::Options options("openblack-server", "The dedicated server of openblack: the world of one land, with no window.");
	// clang-format off
	options.add_options()
		("g,game-path", "The folder of the game's data.", cxxopts::value<std::string>())
		("s,start-level", "The script of the land under the game's Scripts folder, or a land of a network game such as Online Maps/mpm_2p_1.map (the default).", cxxopts::value<std::string>()->default_value(std::string(openblack::server::k_ServerDefaultLand)))
		("testbed", "The flat testbed in place of a land.")
		("scenario", "Tools: start on the testbed and play the testbed scenario of this id, as the game's --scenario.", cxxopts::value<std::string>())
		("gods-ring-map", "The network map for 32 gods in place of a land.")
		("skip-opening", "How the new game on the first land starts, as the game's --skip-opening.", cxxopts::value<std::string>()->default_value("cow"))
		("seed", "Tools: the seed of every random number of the world, which also pins the date of the world. Without it, the world starts from the seed of a room and reads the date from the clock of the computer.", cxxopts::value<uint32_t>())
		("n,turns", "Stop when the world gets to this turn. With none, the server runs until a signal stops it.", cxxopts::value<uint32_t>())
		("frame-ms", "The game time of each frame in milliseconds. A turn is 100 ms.", cxxopts::value<double>()->default_value("100"))
		("fast", "Tools: play the frames one after the other with no wait for the wall clock.")
		("profile-folder", "The folder of the server's own profile. With none, the profile of a tool's run in the working folder.", cxxopts::value<std::string>())
		("log-checksums", "Tools: write the checksum of each turn to the log, which server_check and net_check read.")
		("record", "Tools: write the run record of each turn to this file, with the hash of each component.", cxxopts::value<std::string>())
		("record-fields", "Tools: with --record, the hash of each field of each component too.")
		("trace-draws", "Tools: write each draw on the synced random seed to this file, as the game's --sim-trace-draws.", cxxopts::value<std::string>())
		("reload-at", "Tools: at this turn the server takes its whole world into memory as a world state and loads it again, as a client that comes back loads the server's world.", cxxopts::value<uint32_t>())
		("temple-loss", "Tests: at a turn, the temple of a player starts its destruction, as PLAYER:TURN (the player from 0). Each client needs the same switch (--sim-temple-loss).", cxxopts::value<std::string>())
		("all-computer-gods", "Tools: every god with a citadel is a computer god, and the room has no human god. The game starts at once: with --gods N the world turns when N observers joined (for the AI soak).")
		("ai-seed", "Tools: the seed of the personalities of the computer gods. Each client takes it from the start.", cxxopts::value<uint32_t>())
		("commands", "Tools: do the commands of each turn that a game wrote with --sim-commands, at the same turns.", cxxopts::value<std::string>())
		("test-commands", "Tests: the game server lets through the commands of the test tools, such as the fight cheat.")
#if defined(OPENBLACK_DEVELOPER_TOOLS)
		("row-dump", "Tools: keep each field of each row of the synced world of the last turns, and write the rows of a turn to this file when the world of a client parts from the server's world (tools/sim/row_dump.py compares two files).", cxxopts::value<std::string>())
		("row-dump-turn", "Tools: with --row-dump, also write the rows of this turn.", cxxopts::value<uint32_t>())
		("row-dump-keep", "Tools: with --row-dump, the count of the last turns of which the dump keeps the rows.", cxxopts::value<uint32_t>()->default_value("64"))
#endif
		("listen", "The game role: listen for the clients on this IPv4 address and TCP port, such as 127.0.0.1:0 (0 for a free port).", cxxopts::value<std::string>())
		("tls-cert", "The PEM file of the certificate chain of the game socket (wss). Without it, the socket speaks plain WebSocket on a loopback address only, behind a proxy that does the TLS.", cxxopts::value<std::string>())
		("tls-key", "The PEM file of the key of --tls-cert.", cxxopts::value<std::string>())
		("server-id", "The id of the server at the lobby, which the join tickets name.", cxxopts::value<std::string>())
		("ticket-keys", "The keys that sign the join tickets: the https:// address of the lobby's /api/v1/ticket-keys, or a file of the same JSON.", cxxopts::value<std::string>())
		("min-gods", "The fewest gods with a player that start the game. The game starts when each god in the room is ready and the room has this many gods at least. The room has a slot for each citadel of the land.", cxxopts::value<uint32_t>()->default_value("1"))
		("gods", "The old name of --min-gods. --min-gods comes first when the command line has both.", cxxopts::value<uint32_t>())
		("lobby-api", "The game servers' API of the lobby, such as https://srv.example.org/api/v1. With it, the server reports its room every 5 seconds, so that the lobby lists it.", cxxopts::value<std::string>())
		("client-cert", "The PEM file of the client certificate that the server shows to the lobby (mutual TLS).", cxxopts::value<std::string>())
		("client-key", "The PEM file of the key of --client-cert.", cxxopts::value<std::string>())
		("room-name", "The name of the room in the list of the lobby.", cxxopts::value<std::string>()->default_value("Game"))
		("lobby-test-ca", "Tests on this computer only: a PEM file of a certificate authority of a test lobby on a loopback address (127.0.0.1).", cxxopts::value<std::string>())
		("come-back-seconds", "How long a god's player can be away before the god is defeated.", cxxopts::value<uint32_t>()->default_value("300"))
		("l,log-file", "The file of the log, or stdout.", cxxopts::value<std::string>()->default_value("stdout"))
		("version", "Write the version of openblack (the semantic version and the git commit) and exit.")
		("h,help", "Show this help.");
	// clang-format on

	openblack::server::ServerSettings settings;
	try
	{
		// A build without the developer tools ignores the options of the tools and keeps those of a deployed server
		std::vector<char*> arguments(argv, argv + argc);
		if constexpr (!openblack::developer_tools::k_Built)
		{
			const std::vector<std::string_view> texts(arguments.begin(), arguments.end());
			const auto filtered =
			    openblack::developer_tools::FilterDeveloperOptions(texts, openblack::developer_tools::k_ServerDeveloperOptions);
			std::vector<char*> kept;
			kept.reserve(filtered.kept.size());
			std::ranges::transform(filtered.kept, std::back_inserter(kept),
			                       [&arguments](size_t index) { return arguments[index]; });
			arguments = std::move(kept);
			for (const auto& ignored : filtered.ignored)
			{
				std::cerr << "This build has no developer tools: it ignores the option " << ignored << std::endl;
			}
		}
		const auto result = options.parse(static_cast<int>(arguments.size()), arguments.data());
		if (result.count("help") != 0)
		{
			std::cout << options.help() << std::endl;
			return EXIT_SUCCESS;
		}
		if (result.count("version") != 0)
		{
			std::cout << "openblack-server " << openblack::build_version::Text() << std::endl;
			return EXIT_SUCCESS;
		}
		if (result.count("game-path") == 0)
		{
			std::cerr << "The server needs the folder of the game's data: use -g." << std::endl;
			return EXIT_FAILURE;
		}
		settings.gamePath = result["game-path"].as<std::string>();
		settings.land = result["start-level"].as<std::string>();
		settings.testbed = result.count("testbed") != 0;
		if (result.count("scenario") != 0)
		{
			settings.scenario = result["scenario"].as<std::string>();
			settings.testbed = true;
		}
		settings.openingName = result["skip-opening"].as<std::string>();
		settings.opening = openblack::new_game_choice::ParseNewGameStart(settings.openingName);
		settings.godsRingMap = result.count("gods-ring-map") != 0;
		settings.soak.allComputerGods = result.count("all-computer-gods") != 0;
		if (result.count("ai-seed") != 0)
		{
			settings.soak.aiSeed = result["ai-seed"].as<uint32_t>();
		}
		if (const auto refusal = openblack::developer_tools::SoakRefusalOf(openblack::developer_tools::k_Built, settings.soak);
		    refusal.has_value())
		{
			std::cerr << "This server " << *refusal << "." << std::endl;
			return EXIT_FAILURE;
		}
		if (result.count("seed") != 0)
		{
			settings.seed = result["seed"].as<uint32_t>();
		}
		if (result.count("turns") != 0)
		{
			settings.turns = result["turns"].as<uint32_t>();
		}
		if (result.count("temple-loss") != 0)
		{
			settings.templeLoss = openblack::sim_run::ParseTempleLoss(result["temple-loss"].as<std::string>());
		}
		settings.frameMilliseconds = result["frame-ms"].as<double>();
		settings.fast = result.count("fast") != 0;
		if (result.count("profile-folder") != 0)
		{
			settings.profileFolder = std::filesystem::path(result["profile-folder"].as<std::string>());
		}
		settings.logFile = result["log-file"].as<std::string>();
		settings.logChecksums = result.count("log-checksums") != 0;
		if (result.count("record") != 0)
		{
			settings.recordPath = std::filesystem::path(result["record"].as<std::string>());
			settings.recordFields = result.count("record-fields") != 0;
		}
		if (result.count("trace-draws") != 0)
		{
			settings.traceDrawsPath = std::filesystem::path(result["trace-draws"].as<std::string>());
		}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
		if (result.count("row-dump") != 0)
		{
			settings.rowDumpPath = std::filesystem::path(result["row-dump"].as<std::string>());
			settings.rowDump.keptTurns = result["row-dump-keep"].as<uint32_t>();
			if (result.count("row-dump-turn") != 0)
			{
				settings.rowDump.atTurn = result["row-dump-turn"].as<uint32_t>();
			}
		}
#endif
		if (result.count("reload-at") != 0)
		{
			settings.reloadAt = result["reload-at"].as<uint32_t>();
		}
		if (result.count("commands") != 0)
		{
			settings.commandsPath = std::filesystem::path(result["commands"].as<std::string>());
		}
		if (result.count("listen") != 0)
		{
			const auto listen = result["listen"].as<std::string>();
			const auto colon = listen.rfind(':');
			if (colon == std::string::npos)
			{
				std::cerr << "--listen needs an address and a port, such as 127.0.0.1:0." << std::endl;
				return EXIT_FAILURE;
			}
			openblack::server::NetworkSettings network;
			network.address = listen.substr(0, colon);
			network.port = static_cast<uint16_t>(std::stoul(listen.substr(colon + 1)));
			if (result.count("tls-cert") != 0)
			{
				network.certificate = std::filesystem::path(result["tls-cert"].as<std::string>());
			}
			if (result.count("tls-key") != 0)
			{
				network.key = std::filesystem::path(result["tls-key"].as<std::string>());
			}
			if (result.count("server-id") != 0)
			{
				network.serverId = result["server-id"].as<std::string>();
			}
			if (result.count("ticket-keys") != 0)
			{
				network.ticketKeys = result["ticket-keys"].as<std::string>();
			}
			network.minGods = result.count("min-gods") == 0 && result.count("gods") != 0 ? result["gods"].as<uint32_t>()
			                                                                             : result["min-gods"].as<uint32_t>();
			network.comeBackSeconds = result["come-back-seconds"].as<uint32_t>();
			network.testCommands = result.count("test-commands") != 0;
			if (result.count("lobby-api") != 0)
			{
				network.lobbyApi = result["lobby-api"].as<std::string>();
			}
			if (result.count("client-cert") != 0)
			{
				network.clientCertificate = std::filesystem::path(result["client-cert"].as<std::string>());
			}
			if (result.count("client-key") != 0)
			{
				network.clientKey = std::filesystem::path(result["client-key"].as<std::string>());
			}
			network.roomName = result["room-name"].as<std::string>();
			if (result.count("lobby-test-ca") != 0)
			{
				network.lobbyTestAuthorities = std::filesystem::path(result["lobby-test-ca"].as<std::string>());
			}
			settings.network = std::move(network);
		}
		openblack::crash_handler::SetLogFile(settings.logFile);
	}
	catch (const cxxopts::exceptions::exception& error)
	{
		std::cerr << error.what() << std::endl << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	// Ctrl+C, the end of the console and the request of the system to end the program stop the server
	std::signal(SIGINT, StopOnSignal);
	std::signal(SIGTERM, StopOnSignal);
#if defined(SIGBREAK)
	std::signal(SIGBREAK, StopOnSignal);
#endif

	try
	{
		return openblack::server::Run(settings, g_StopAsked, {});
	}
	catch (const std::exception& error)
	{
		openblack::crash_handler::ReportFatal(openblack::crash_report::CrashKind::UncaughtException, error.what(), {}, 0,
		                                      typeid(error).name());
	}
}
