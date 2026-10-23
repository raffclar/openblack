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
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

#include "Common/RandomNumberManager.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

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
	_started = false;
	_endLogged = false;
	_awaitingLand = !join.landLoaded;
	_pendingStart.reset();

	// A join from the command line: the land loaded, so the world is the network game's world from now on. The clock
	// of this computer waits for the server's start, so that the frames of the wait take no game time.
	if (join.landLoaded)
	{
		Attach();
		Locator::time::value().SetPaused(true);
	}

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Joining the game server at {}", join.url);
	_session->Join(online::NetworkSessionSettings {
	    .request = {.url = join.url, .headers = {}, .subprotocols = {}},
	    .ticket = std::move(join.ticket),
	    .gamePassword = std::move(join.gamePassword),
	    .name = std::move(join.name),
	    .readLand = [](std::string_view land) -> std::optional<std::vector<uint8_t>> {
		    // The land of the start is a file in the Scripts folder of the game on this computer
		    auto& fileSystem = Locator::filesystem::value();
		    const auto path = fileSystem.GetPath<filesystem::Path::Scripts>() / std::filesystem::path(land);
		    if (!fileSystem.Exists(path))
		    {
			    return std::nullopt;
		    }
		    return fileSystem.ReadAll(path);
	    },
	    .readCreature = _readCreature,
	});
	// The land of the game loaded before the join, so the player is ready at once
	if (join.landLoaded)
	{
		_session->SetReady(true);
	}
	return true;
}

void NetworkGameSystem::Attach()
{
	// A network game: the clock never stops for one player
	Locator::time::value().SetNetworkGame(true);
	Locator::playerCommandSystem::value().SetSource(_session.get());
	auto& checksums = Locator::syncChecksumSystem::value();
	checksums.SetEnabled(true);
	checksums.SetTurnListener([session = _session.get()](const SyncChecksumSystemInterface::TurnChecksum& turn) {
		session->LocalChecksum(turn.turn, turn.checksum);
	});
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

	if (!_started && _session->GetStart().has_value())
	{
		_started = true;
		const auto& start = *_session->GetStart();
		// A join from the front end: the game loads the server's land, then calls BeginPlay
		if (_awaitingLand)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game server starts the game on {} with seed {}",
			                   start.testbed ? std::string("the testbed") : start.land, start.seed);
			_pendingStart = start;
			return;
		}
		// The server's world must start from the same land and seed as this computer's world
		const auto& land = Locator::landSystem::value().GetLandPath();
		const bool sameLand = std::filesystem::path(start.land).filename() == land.filename();
		const bool sameSeed = Locator::rng::value().GetRunSeed() == start.seed;
		if (!sameLand || !sameSeed)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"),
			                    "The game server plays land {} with seed {}; this computer has {} with seed {}", start.land,
			                    start.seed, land.string(), Locator::rng::value().GetRunSeed());
			_session->Leave();
		}
		else
		{
			Locator::time::value().SetPaused(false);
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

bool NetworkGameSystem::InGame() const
{
	return _session && _session->GetState() != online::NetworkSession::State::Ended;
}
