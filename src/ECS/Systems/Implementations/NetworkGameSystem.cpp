/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "NetworkGameSystem.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Online/NetworkLand.h"
#include "Online/OnlineMapStoreInterface.h"
#include "PlayerSlots.h"

using namespace openblack::ecs::systems;

namespace
{

/// The host of a wss:// address
std::string_view HostOf(std::string_view url)
{
	constexpr std::string_view k_Scheme = "wss://";
	if (!url.starts_with(k_Scheme))
	{
		return {};
	}
	url.remove_prefix(k_Scheme.size());
	const auto end = url.find_first_of(":/?#");
	return url.substr(0, end);
}

bool IsLoopbackHost(std::string_view host)
{
	return host == "localhost" || host.starts_with("127.");
}

int64_t Microseconds()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

NetworkGameSystem::NetworkGameSystem(online::WebSocketClientInterface& sockets, MakeTestClient makeTestClient,
                                     ReadCreature readCreature)
    : _sockets(sockets)
    , _makeTestClient(std::move(makeTestClient))
    , _readCreature(std::move(readCreature))
{
}

NetworkGameSystem::~NetworkGameSystem()
{
	// A player who closes the game leaves the network game
	if (_session && _session->GetState() != online::NetworkSession::State::Ended)
	{
		_session->Leave();
		if (_testClient)
		{
			_testClient->Poll();
		}
	}
	Detach();
}

void NetworkGameSystem::Detach()
{
	if (!_session)
	{
		return;
	}
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().SetSource(nullptr);
	}
	// The next land that loads has the player of this computer as its one human again
	if (Locator::landSystem::has_value())
	{
		Locator::landSystem::value().SetHumansFromSession(false);
	}
	if (Locator::time::has_value())
	{
		Locator::time::value().SetCatchesUp(false);
	}
	if (Locator::syncChecksumSystem::has_value())
	{
		Locator::syncChecksumSystem::value().SetTurnListener({});
	}
	_session.reset();
	_testClient.reset();
}

bool NetworkGameSystem::Join(NetworkJoin join)
{
	Detach();
	online::WebSocketClientInterface* sockets = &_sockets;
	if (join.testAuthorities.has_value())
	{
		// The extra authorities are only for a test server on this computer
		if (!IsLoopbackHost(HostOf(join.url)) || !_makeTestClient)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The test authorities are only for a game server on a loopback address");
			return false;
		}
		_testClient = _makeTestClient(*join.testAuthorities);
		if (!_testClient)
		{
			return false;
		}
		sockets = _testClient.get();
	}
	_session = std::make_unique<online::NetworkSession>(*sockets, Microseconds);
	// A land that loads after the join gets its humans from the session at the start of the game
	if (Locator::landSystem::has_value())
	{
		Locator::landSystem::value().SetHumansFromSession(true);
	}
	_partAt = join.partAt;
	_partAgain = join.partAgain;
	_started = false;
	_endLogged = false;
	_awaitingLand = !join.fromCommandLine;
	_fromCommandLine = join.fromCommandLine;
	_joinReplyRead = false;
	_pendingStart.reset();

	// A join from the command line: the clock of this computer waits for the server's land and start, so that the frames
	// of the wait take no game time
	if (join.fromCommandLine)
	{
		Locator::time::value().SetPaused(true);
	}

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Joining the game server at {}", join.url);
	auto readCreature = _readCreature;
	if (join.testCreatureFile.has_value())
	{
		// A test sends the creature file that it names, in place of the creature of the player's profile
		readCreature = [path = *join.testCreatureFile]() {
			auto& fileSystem = Locator::filesystem::value();
			return fileSystem.Exists(path) ? fileSystem.ReadAll(path) : std::vector<uint8_t> {};
		};
	}
	_session->Join(online::NetworkSessionSettings {
	    .request = {.url = join.url, .headers = {}, .subprotocols = {}},
	    .ticket = std::move(join.ticket),
	    .gamePassword = std::move(join.gamePassword),
	    .name = std::move(join.name),
	    .readLand = [](std::string_view land) -> std::optional<std::vector<uint8_t>> {
		    // The land of the start is a file in the Scripts folder of the game on this computer, or a map file that the
		    // player got from the lobby, or a map file in the "Online Maps" folder of the game
		    const auto* store = Locator::onlineMapStore::has_value() ? &Locator::onlineMapStore::value() : nullptr;
		    return online::ReadStartLand(land, store, Locator::filesystem::value());
	    },
	    .readCreature = std::move(readCreature),
	    .readAlignment =
	        []() {
		        // The god starts the game at the alignment that the player's profile kept from earlier games
		        const auto profile = Locator::playerProfileSystem::has_value()
		                                 ? Locator::playerProfileSystem::value().GetCurrentProfile()
		                                 : std::nullopt;
		        return profile_alignment::Sent(profile.has_value() ? profile->alignment : 0.0);
	        },
	    .readPlaysForClan = [this]() { return _playsForClan; },
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	    .parts =
	        [](uint32_t turn) {
		        // The row dump of the tools writes the rows of the turn, when it started
		        Locator::syncChecksumSystem::value().DumpPart(turn, "parts from the server", 0);
	        },
#else
	    .parts = {},
#endif
	    .origin = join.origin,
	});
	return true;
}

void NetworkGameSystem::Attach()
{
	// A network game: the clock never stops for one player
	auto& clock = Locator::time::value();
	clock.SetNetworkGame(true);
	clock.SetCatchesUp(true);
	Locator::playerCommandSystem::value().SetSource(_session.get());
	auto& checksums = Locator::syncChecksumSystem::value();
	checksums.SetEnabled(true);
	// A tool's file of the checksums (--sim-checksums) keeps getting them, to find the part where a client parts
	checksums.SetTurnListener(
	    [session = _session.get(), tool = checksums.GetTurnListener()](const SyncChecksumSystemInterface::TurnChecksum& turn) {
		    session->LocalChecksum(turn.turn, turn.checksum);
		    if (tool)
		    {
			    tool(turn);
		    }
	    });
}

void NetworkGameSystem::SetPlaysForClan(bool playsForClan)
{
	_playsForClan = playsForClan;
}

void NetworkGameSystem::SetReady(bool ready)
{
	if (_session)
	{
		_session->SetReady(ready);
	}
}

std::optional<openblack::game_socket::Start> NetworkGameSystem::TakeStart()
{
	return std::exchange(_pendingStart, std::nullopt);
}

void NetworkGameSystem::BeginPlay()
{
	if (!_session || !_awaitingLand)
	{
		return;
	}
	_awaitingLand = false;
	Attach();
	if (_fromCommandLine)
	{
		// The land of the server loaded: the player is ready, and the world waits for the server's start
		Locator::time::value().SetPaused(true);
		_session->SetReady(true);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land of the game server loaded: the player is ready for the start");
		return;
	}
	Locator::time::value().SetPaused(false);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land of the network game loaded: the world does the server's turns");
}

void NetworkGameSystem::Leave()
{
	if (_session)
	{
		_session->Leave();
	}
}

void NetworkGameSystem::EndNetworkGame()
{
	if (!_session)
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game of this computer ends");
	Detach();
	_started = false;
	_awaitingLand = false;
	_fromCommandLine = false;
	_joinReplyRead = false;
	_stopped = false;
	_pendingStart.reset();
	_resumeTurn.reset();
	// The clock is the clock of a game alone again, and it no longer waits for the turns of a server
	auto& clock = Locator::time::value();
	clock.SetNetworkGame(false);
	clock.SetPaused(false);
	// In a game alone, the player of this computer is the first player again: the next land and its creature need that
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().SetOwnPlayer(PlayerNames::PLAYER_ONE);
	}
	if (Locator::syncChecksumSystem::has_value())
	{
		Locator::syncChecksumSystem::value().SetEnabled(false);
	}
}

void NetworkGameSystem::StopGame()
{
	if (_session)
	{
		_session->StopGame();
	}
}

void NetworkGameSystem::PlayAlone()
{
	if (InGame())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world of this computer turns alone, with no game server");
	Detach();
}

void NetworkGameSystem::Update()
{
	if (!_session)
	{
		return;
	}
	if (_testClient)
	{
		_testClient->Poll();
	}
	_session->Update();

	// A test parts this computer's world from the server's once
	if (_partAt.has_value() && Locator::time::value().GetTurn() >= *_partAt)
	{
		_partAt.reset();
		std::ignore = Locator::gameRandom::value().GameRand(1000);
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Test: the world parts from the server's at turn {}",
		                   Locator::time::value().GetTurn());
	}

	// The god of this computer's slot: its pointer moves that god's hand. An observer has no god of its own.
	if (const auto& joined = _session->GetJoined(); joined.has_value() && Locator::handSystem::has_value())
	{
		auto& hands = Locator::handSystem::value();
		const auto own = joined->slot.has_value() ? player_slots::PlayerOfSlot(*joined->slot) : std::nullopt;
		if (!own.has_value())
		{
			hands.ClearOwnPlayer();
		}
		else if (!hands.IsOwnPlayer(*own))
		{
			hands.SetOwnPlayer(*own);
		}
	}

	// The world of a loaded state goes on as the server's turn of the state is here: the next frame does that turn
	if (_resumeTurn.has_value() && _session->HasTurn(*_resumeTurn))
	{
		auto& clock = Locator::time::value();
		clock.SetPaused(false);
		clock.ResetTimerToTurn();
		_resumeTurn.reset();
	}

	// After the last turn of a decided game, the clock of this computer stops. The world stands still, and the frames
	// take no game time, as in a pause.
	if (const auto last = _session->StoppedAfter(); last.has_value() && !_stopped && Locator::time::value().GetTurn() > *last)
	{
		_stopped = true;
		Locator::time::value().SetPaused(true);
		// The log names the last turn by the number of the world, as the line of the decision does
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The turns of the game stopped after turn {}: the clock stops", *last + 1);
	}

	// A join from the command line: the server's reply to the join gives the land, the seed and the start of its world,
	// which the game loads before the player is ready
	if (_fromCommandLine && !_joinReplyRead)
	{
		if (const auto& joined = _session->GetJoined(); joined.has_value())
		{
			_joinReplyRead = true;
			if (joined->start.has_value())
			{
				const auto& start = *joined->start;
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game server plays {} with seed {}: the game loads that land",
				                   network_land::DisplayName(start), start.seed);
				_pendingStart = start;
				_awaitingLand = true;
			}
			else
			{
				// A server that gives no land in its reply: the land of this computer must be the server's land
				Attach();
				_session->SetReady(true);
			}
		}
	}

	if (!_started && _session->GetStart().has_value())
	{
		const auto& start = *_session->GetStart();
		if (_awaitingLand && !_fromCommandLine)
		{
			// A join from the front end: the game loads the server's land, then calls BeginPlay
			_started = true;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game server starts the game on {} with seed {}",
			                   network_land::DisplayName(start), start.seed);
			_pendingStart = start;
			return;
		}
		// A join from the command line that loads the server's land does the start once the land loaded
		if (!_awaitingLand)
		{
			_started = true;
			// The server's world must start from the same land and seed as this computer's world
			const auto& land = Locator::landSystem::value().GetLandPath();
			const bool sameLand = start.testbed || start.godsRingMap || network_land::IsLand(land, start.land);
			const bool sameSeed = Locator::rng::value().GetRunSeed() == start.seed;
			if (!sameLand || !sameSeed)
			{
				SPDLOG_LOGGER_ERROR(
				    spdlog::get("game"), "The game server plays land {} with seed {}; this computer has {} with seed {}",
				    network_land::DisplayName(start), start.seed, land.generic_string(), Locator::rng::value().GetRunSeed());
				_session->Leave();
			}
			else
			{
				Locator::time::value().SetPaused(false);
			}
		}
	}
	// After a resync, or after a join to a game that turns, the server's world comes. This computer's world loads it
	// after the land of the start, and continues from the turn of the world.
	if (_started && !_awaitingLand && !_pendingStart.has_value())
	{
		if (auto world = _session->TakeWorld(); world.has_value())
		{
			LoadWorld(*world);
		}
	}
	if (_session->GetState() == online::NetworkSession::State::Ended && !_endLogged)
	{
		_endLogged = true;
		const auto& compares = _session->GetCompares();
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The network game ended ({}): {} turns compared with the server, {} the same, {} resumes",
		                   online::EndName(_session->GetEnd()), compares.compared, compares.same, _session->Resumes());
	}
}

void NetworkGameSystem::LoadWorld(const online::NetworkSession::WorldState& world)
{
	// The checksum of the last turn takes the world of that turn, before the load replaces it
	Locator::syncChecksumSystem::value().FinishHash();
	const auto start = std::chrono::steady_clock::now();
	if (!Locator::saveGameSystem::has_value() || !Locator::saveGameSystem::value().LoadServerWorldState(world.bytes))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The world state of turn {} from the game server did not load", world.turn);
		_session->Leave();
		return;
	}
	// The log gives the time of the load, so that the tools can see how long a client stops when it comes back in step
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world state of turn {} ({} bytes) loaded in {:.2f} ms", world.turn,
	                   world.bytes.size(),
	                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	// The load starts the land again: the world is still the world of a network game, which turns with the server
	auto& clock = Locator::time::value();
	clock.SetNetworkGame(true);
	clock.SetCatchesUp(true);
	// The clock stands still until the server's turn of the state is here, as it does before the start: the frames until
	// then take no game time, so the world is the same as the server's when that turn starts
	clock.SetPaused(true);
	_resumeTurn = world.turn;
	Locator::playerCommandSystem::value().SetSource(_session.get());
	Locator::syncChecksumSystem::value().SetEnabled(true);
	if (clock.GetTurn() != world.turn)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The world state is at turn {}, not at the server's turn {}", clock.GetTurn(),
		                    world.turn);
	}
	_session->WorldLoaded(world.turn);
	// A test parts the loaded world again at once
	if (_partAgain)
	{
		_partAt = world.turn;
	}
}

bool NetworkGameSystem::InGame() const
{
	return _session && _session->GetState() != online::NetworkSession::State::Ended;
}
