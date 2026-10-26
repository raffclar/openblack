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

/// The land of the server when none is asked for: the first land for two gods of a network game, as the game's first
/// network map
inline constexpr std::string_view k_ServerDefaultLand = "Online Maps/mpm_2p_1.map";

/// The game role of the server: the game socket that the clients join with a ticket of the lobby
struct NetworkSettings
{
	/// The IPv4 address and the TCP port of the game socket. Port 0 lets the system choose a free port.
	std::string address {"127.0.0.1"};
	uint16_t port {0};
	/// The certificate chain and the key of TLS, as PEM files. With none, the socket speaks plain WebSocket, only on a
	/// loopback address, behind a reverse proxy that does the TLS.
	std::optional<std::filesystem::path> certificate;
	std::optional<std::filesystem::path> key;
	/// The id of the server at the lobby, which the tickets name
	std::string serverId;
	/// The keys that sign the tickets: the https:// address of the lobby's /api/v1/ticket-keys, or a file with the same
	/// JSON
	std::string ticketKeys;
	/// The fewest gods with a player that start the game. The game starts when each god in the room is ready and the room
	/// has this many gods at least. The room has a slot for each citadel of the land, which can be more.
	uint32_t minGods {1};
	/// How long a god's player can be away before the god is defeated
	uint32_t comeBackSeconds {300};
	/// The game servers' API of the lobby, such as https://srv.example.org/api/v1. With it, the server reports its room
	/// every 5 seconds, so that the lobby lists the server and gives tickets for it.
	std::string lobbyApi;
	/// The client certificate and its key (PEM files) that the server shows to the lobby for mutual TLS
	std::optional<std::filesystem::path> clientCertificate;
	std::optional<std::filesystem::path> clientKey;
	/// The name of the room in the list of the lobby
	std::string roomName {"Game"};
	/// Tests on this computer only: a PEM file of more certificate authorities for a lobby on a loopback address
	std::optional<std::filesystem::path> lobbyTestAuthorities;
};

/// What the command line of the dedicated server gives
struct ServerSettings
{
	/// The folder of the game's data
	std::filesystem::path gamePath;
	/// The script of the land under the game's Scripts folder, or a land of a network game in the game's folder
	std::string land {k_ServerDefaultLand};
	/// The flat testbed in place of a land
	bool testbed {false};
	/// Tools: the testbed scenario of this id, which the server plays on the testbed from its first frame, as the game's
	/// --scenario does. Thus input that the game recorded can get to what the scenario sets up.
	std::optional<std::string> scenario;
	/// The network map for 32 gods in place of a land
	bool godsRingMap {false};
	/// How the new game on the first land starts, as the game's --skip-opening gives it
	std::optional<new_game_choice::NewGameStart> opening;
	/// The text of --skip-opening, which the game server sends to the clients in the start of the game
	std::string openingName;
	/// The seed of every random number of the world. The date of the world is pinned with it.
	uint32_t seed {1};
	/// The server stops when the world gets to this turn. None for a server that runs until a signal stops it.
	std::optional<uint32_t> turns;
	/// Tests: the loss of a temple at a turn, which every client of the network game must also have
	std::optional<sim_run::TempleLoss> templeLoss;
	/// The game time of each frame in milliseconds
	double frameMilliseconds {k_DefaultFrameMilliseconds};
	/// Tools: play the frames one after the other with no wait for the wall clock
	bool fast {false};
	/// A folder for the server's profile. The server does not use it: it plays as a profile in memory and writes no
	/// profile, no saved game and no player data. The server only says in its log that it did not use the folder.
	std::optional<std::filesystem::path> profileFolder;
	/// Tools: the server writes the run record of each turn to this file, as the game's --sim-record does, with the hash
	/// of each component. It starts the inspector on a free port of 127.0.0.1.
	std::optional<std::filesystem::path> recordPath;
	/// Tools: the run record also has the hash of each field of each component
	bool recordFields {false};
	/// Tools: the server writes each draw on the synced seed to this file, as the game's --sim-trace-draws does
	std::optional<std::filesystem::path> traceDrawsPath;
	/// Tools: the commands of each turn that a game wrote (the game's --sim-commands). The server does them at the same
	/// turns, as if the game sent them over the network.
	std::optional<std::filesystem::path> commandsPath;
	/// Tools: at this turn the server takes its whole world as a world state and loads it again
	std::optional<uint32_t> reloadAt;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	/// Tools: the file of the row dump (row_dump::RowDump). The server writes the rows of a turn at which the world of a
	/// client parts from the server's world, and at the turn of the settings.
	std::optional<std::filesystem::path> rowDumpPath;
	row_dump::Settings rowDump;
#endif
	/// The file of the log, or "stdout"
	/// The game role. With none, the server plays the world alone, as the tools use it.
	std::optional<NetworkSettings> network;
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
