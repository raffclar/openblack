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
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <typeinfo>
#include <vector>

#include <cxxopts.hpp>

#include "Common/BuildVersion.h"
#include "Common/CrashHandler.h"
#include "Common/DeveloperTools.h"
#include "ECS/GameEndConditions.h"
#include "Online/Desktop/UdpLanDiscovery.h"
#include "Server/DedicatedServer.h"
#include "Server/MapExport.h"

namespace
{

/// The signal handler sets it, and the loop of the server stops at the end of its frame
std::atomic<bool> g_StopAsked {false};

extern "C" void StopOnSignal(int /*signal*/)
{
	g_StopAsked.store(true);
}

#if defined(OPENBLACK_DEVELOPER_TOOLS)
/// Searches the local network as the LAN list of the game does, and writes each room that answers
int SearchLocalNetwork(uint32_t seconds)
{
	const auto now = [] {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
		    .count();
	};
	auto search = openblack::online::MakeUdpLanDiscovery(now);
	if (!search->IsAvailable())
	{
		std::cerr << "The search of the local network cannot open a socket." << std::endl;
		return EXIT_FAILURE;
	}
	search->Start();
	const auto end = now() + static_cast<int64_t>(seconds) * 1'000'000;
	size_t shown = 0;
	while (now() < end && !g_StopAsked)
	{
		search->Poll();
		const auto servers = search->GetServers();
		for (; shown < servers.size(); ++shown)
		{
			std::cout << "found " << servers[shown].id << " \"" << servers[shown].hostname << "\" " << servers[shown].url
			          << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	const auto servers = search->GetServers();
	std::cout << "rooms " << servers.size() << std::endl;
	for (const auto& server : servers)
	{
		std::cout << "room " << server.id << " \"" << server.hostname << "\" " << server.numplayers << "/" << server.maxplayers
		          << " " << server.gamever << " " << server.url << std::endl;
	}
	search->Stop();
	return EXIT_SUCCESS;
}
#endif

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
		("gods-ring-map", "The network map for 32 gods in place of a land.")
		("write-gods-ring-map", "Write the file of the network map for 32 gods, which the players get from the lobby, to this file, then stop.", cxxopts::value<std::string>())
		("profile-folder", "The folder of the server's own profile. With none, the profile of a tool's run in the working folder.", cxxopts::value<std::string>())
		("time-limit", "The time limit of the network game in minutes, from 10 to 1440 (24 hours) as the host picks it, in place of the default 30. A build with the developer tools also accepts 1 to 9 minutes for tests. The start sends it to every client.", cxxopts::value<uint32_t>())
		("conditions", "The winning conditions of the network game, as the host picks them: [LAND:]NAME[=VALUE],... with the names of Online Maps/ConditionTemplate.txt or their numbers from 0. A NAME with no VALUE keeps the first value of the land, as the host's check box does. The values must be in the limits of the land (or of LAND) in that file. The start sends them to every client.", cxxopts::value<std::string>())
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
		("lan", "The room is on the local network with no lobby: the server answers the search of the game on UDP port 2613, and a player joins with the name of the profile. The server then needs no --server-id and no --ticket-keys.")
		("lan-host", "With --lan, the computer name of the game socket that the answers give, which the TLS certificate names. Without it, each client uses the address of the server.", cxxopts::value<std::string>())
		("lobby-test-ca", "Tests on this computer only: a PEM file of a certificate authority of a test lobby on a loopback address (127.0.0.1).", cxxopts::value<std::string>())
		("snapshot-folder", "The folder of the snapshots of the game server: the shared world, on the server's own disk, for a restart after a crash.", cxxopts::value<std::string>()->default_value(std::string(openblack::server::snapshots::k_DefaultFolder)))
		("snapshot-every", "The turns between two snapshots of the game server (10 turns a second). 0 stops the snapshots.", cxxopts::value<uint32_t>()->default_value(std::to_string(openblack::server::snapshots::k_DefaultEveryTurns)))
		("snapshot-keep", "The snapshots that the folder keeps, at least 1.", cxxopts::value<uint32_t>()->default_value(std::to_string(openblack::server::snapshots::k_DefaultKeep)))
		("resume-snapshot", "At its start, the game server continues the game of the newest snapshot that reads. With no snapshot, it starts a new game.")
		("come-back-seconds", "How long a god's player can be away before the god is defeated.", cxxopts::value<uint32_t>()->default_value("300"))
		("l,log-file", "The file of the log, or stdout.", cxxopts::value<std::string>()->default_value("stdout"))
		("version", "Write the version of openblack (the semantic version and the git commit) and exit.")
		("h,help", "Show this help.");
	// The options that only developers and tools use. A build without the developer tools ignores them (see
	// FilterDeveloperOptions), and its help does not show them.
	options.add_options("Developer")
		("testbed", "The flat testbed in place of a land.")
		("scenario", "Tools: start on the testbed and play the testbed scenario of this id, as the game's --scenario.", cxxopts::value<std::string>())
		("skip-opening", "How the new game on the first land starts, as the game's --skip-opening.", cxxopts::value<std::string>()->default_value("cow"))
		("seed", "Tools: the seed of every random number of the world, which also pins the date of the world. Without it, the world starts from the seed of a room and reads the date from the clock of the computer.", cxxopts::value<uint32_t>())
		("n,turns", "Stop when the world gets to this turn. With none, the server runs until a signal stops it.", cxxopts::value<uint32_t>())
		("frame-ms", "The game time of each frame in milliseconds. A turn is 100 ms.", cxxopts::value<double>()->default_value("100"))
		("fast", "Tools: play the frames one after the other with no wait for the wall clock.")
		("log-checksums", "Tools: write the checksum of each turn to the log, which server_check and net_check read.")
		("record", "Tools: write the run record of each turn to this file, with the hash of each component.", cxxopts::value<std::string>())
		("record-fields", "Tools: with --record, the hash of each field of each component too.")
		("inspect-port", "The inspector of the server on this port of 127.0.0.1 (0 for a free port), for the operator and the tools on the same computer. It refuses the queries that change the world of a network game.", cxxopts::value<uint16_t>())
		("trace-draws", "Tools: write each draw on the synced random seed to this file, as the game's --sim-trace-draws.", cxxopts::value<std::string>())
		("reload-at", "Tools: at this turn the server takes its whole world into memory as a world state and loads it again, as a client that comes back loads the server's world.", cxxopts::value<uint32_t>())
		("gods-need-no-temple", "Tests: a god with no temple stays in the game, for a land that makes no temple at its start (a story land). Each client needs the same switch (--sim-gods-need-no-temple).")
		("temple-loss", "Tests: at a turn, the temple of a player starts its destruction, as PLAYER:TURN (the player from 0) or FIRST-LAST:TURN for a range of players. Each client needs the same switch (--sim-temple-loss).", cxxopts::value<std::string>())
		("all-computer-gods", "Tools: every god with a citadel is a computer god, and the room has no human god. The game starts at once: with --gods N the world turns when N observers joined (for the AI soak).")
		("ai-seed", "Tools: the seed of the personalities of the computer gods. Each client takes it from the start.", cxxopts::value<uint32_t>())
		("commands", "Tools: do the commands of each turn that a game wrote with --sim-commands, at the same turns.", cxxopts::value<std::string>())
		("test-commands", "Tests: the game server lets through the commands of the test tools, such as the fight cheat.");
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	options.add_options("Developer")
		("row-dump", "Tools: keep each field of each row of the synced world of the last turns, and write the rows of a turn to this file when the world of a client parts from the server's world (tools/sim/row_dump.py compares two files).", cxxopts::value<std::string>())
		("row-dump-turn", "Tools: with --row-dump, also write the rows of this turn.", cxxopts::value<uint32_t>())
		("row-dump-keep", "Tools: with --row-dump, the count of the last turns of which the dump keeps the rows.", cxxopts::value<uint32_t>()->default_value("64"))
		("world-states", "Tools: write the world state of the server to this folder at the end of each turn of --world-state-turns, with a map of its synced bytes (tools/sim/save_diff.py --synced compares two computers).", cxxopts::value<std::string>())
		("world-state-turns", "Tools: with --world-states, the turns, such as 250 or 100,250,400.", cxxopts::value<std::string>())
		("lan-search", "Tests: search the local network for game servers for this number of seconds, as the game's LAN list does. Write each room and stop. Needs no -g.", cxxopts::value<uint32_t>());
#endif
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
			std::cout << (openblack::developer_tools::k_Built ? options.help() : options.help({""})) << std::endl;
			return EXIT_SUCCESS;
		}
		if (result.count("version") != 0)
		{
			std::cout << "openblack-server " << openblack::build_version::Text() << std::endl;
			return EXIT_SUCCESS;
		}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
		if (result.count("lan-search") != 0)
		{
			return SearchLocalNetwork(result["lan-search"].as<uint32_t>());
		}
#endif
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
		if (result.count("write-gods-ring-map") != 0)
		{
			const std::filesystem::path out(result["write-gods-ring-map"].as<std::string>());
			if (const auto error = openblack::server::WriteGodsRingMap(settings.gamePath, out); error.has_value())
			{
				std::cerr << "openblack-server: " << *error << '\n';
				return EXIT_FAILURE;
			}
			std::cout << "openblack-server: wrote the map for 32 gods to " << out.generic_string() << '\n';
			return EXIT_SUCCESS;
		}
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
		settings.godsNeedNoTemple = result.count("gods-need-no-temple") != 0;
		if (result.count("temple-loss") != 0)
		{
			settings.templeLoss = openblack::sim_run::ParseTempleLoss(result["temple-loss"].as<std::string>());
		}
		if (result.count("time-limit") != 0)
		{
			const auto minutes = result["time-limit"].as<uint32_t>();
			if (const auto refusal = openblack::game_end::TimeLimitRefusal(minutes, openblack::developer_tools::k_Built);
			    refusal.has_value())
			{
				std::cerr << "The time limit of --time-limit is " << minutes << " minutes, but " << *refusal << "."
				          << std::endl;
				return EXIT_FAILURE;
			}
			settings.timeLimit = minutes;
		}
		if (result.count("conditions") != 0)
		{
			settings.conditions = openblack::sim_run::ParseConditions(result["conditions"].as<std::string>());
			if (!settings.conditions.has_value())
			{
				std::cerr << "The winning conditions of --conditions are not NAME[=VALUE],... with each condition once."
				          << std::endl;
				return EXIT_FAILURE;
			}
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
		if (result.count("inspect-port") != 0)
		{
			settings.inspectPort = result["inspect-port"].as<uint16_t>();
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
		if (result.count("world-states") != 0)
		{
			const auto turns = result.count("world-state-turns") != 0
			                       ? openblack::world_state_at_turn::ParseTurns(result["world-state-turns"].as<std::string>())
			                       : std::nullopt;
			if (!turns.has_value())
			{
				std::cerr << "--world-states needs --world-state-turns with a list such as 100,250" << std::endl;
				return EXIT_FAILURE;
			}
			settings.worldStates = openblack::world_state_at_turn::Settings {
			    .turns = *turns, .folder = std::filesystem::path(result["world-states"].as<std::string>())};
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
			network.lan = result.count("lan") != 0;
			if (result.count("lan-host") != 0)
			{
				network.lanHost = result["lan-host"].as<std::string>();
			}
			network.snapshots = openblack::server::snapshots::Settings {
			    .folder = std::filesystem::path(result["snapshot-folder"].as<std::string>()),
			    .everyTurns = result["snapshot-every"].as<uint32_t>(),
			    .keep = result["snapshot-keep"].as<uint32_t>(),
			    .resume = result.count("resume-snapshot") != 0,
			};
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
		std::cerr << error.what() << std::endl
		          << (openblack::developer_tools::k_Built ? options.help() : options.help({""})) << std::endl;
		return EXIT_FAILURE;
	}

	// Ctrl+C, the end of the console and the request of the system to end the program stop the server
	std::signal(SIGINT, StopOnSignal);
	std::signal(SIGTERM, StopOnSignal);
#if defined(SIGBREAK)
	std::signal(SIGBREAK, StopOnSignal);
#endif
#if defined(SIGPIPE)
	// The TLS library writes to the socket of a client that can close at any time. A write to a closed socket must give
	// an error that the server handles, not a signal that stops the server.
	std::signal(SIGPIPE, SIG_IGN);
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
