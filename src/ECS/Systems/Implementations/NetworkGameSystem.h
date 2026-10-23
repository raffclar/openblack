/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Online/NetworkSession.h"
#include "Online/WebSocketInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class NetworkGameSystem final: public NetworkGameSystemInterface
{
public:
	/// Makes a WSS client that also trusts a file of certificate authorities, for the tests on this computer. None when
	/// the platform has no such client.
	using MakeTestClient = std::function<std::unique_ptr<online::WebSocketClientInterface>(const std::string& authorities)>;

	/// Reads the creature file of the player's profile. Empty when the player has no creature.
	using ReadCreature = std::function<std::vector<uint8_t>()>;

	NetworkGameSystem(online::WebSocketClientInterface& sockets, MakeTestClient makeTestClient, ReadCreature readCreature);
	~NetworkGameSystem() override;
	NetworkGameSystem(const NetworkGameSystem&) = delete;
	NetworkGameSystem& operator=(const NetworkGameSystem&) = delete;
	NetworkGameSystem(NetworkGameSystem&&) = delete;
	NetworkGameSystem& operator=(NetworkGameSystem&&) = delete;

	bool Join(NetworkJoin join) override;
	void SetReady(bool ready) override;
	void Leave() override;
	[[nodiscard]] std::optional<game_socket::Start> TakeStart() override;
	void BeginPlay() override;
	void Update() override;
	[[nodiscard]] bool InGame() const override;
	[[nodiscard]] const online::NetworkSession* Session() const override { return _session.get(); }
	[[nodiscard]] std::optional<online::StartEvent> TakeStartEvent() override
	{
		return _session ? _session->TakeStartEvent() : std::nullopt;
	}

private:
	void Detach();
	/// The world does the turns of the session
	void Attach();

	online::WebSocketClientInterface& _sockets;
	MakeTestClient _makeTestClient;
	ReadCreature _readCreature;
	/// The WSS client of a test join, which only the session uses
	std::unique_ptr<online::WebSocketClientInterface> _testClient;
	std::unique_ptr<online::NetworkSession> _session;
	bool _started {false};
	/// The join came from the front end: the game loads the land after the start
	bool _awaitingLand {false};
	std::optional<game_socket::Start> _pendingStart;
	bool _endLogged {false};
};

} // namespace openblack::ecs::systems
