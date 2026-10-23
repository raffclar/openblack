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

	NetworkGameSystem(online::WebSocketClientInterface& sockets, MakeTestClient makeTestClient);
	~NetworkGameSystem() override;
	NetworkGameSystem(const NetworkGameSystem&) = delete;
	NetworkGameSystem& operator=(const NetworkGameSystem&) = delete;
	NetworkGameSystem(NetworkGameSystem&&) = delete;
	NetworkGameSystem& operator=(NetworkGameSystem&&) = delete;

	bool Join(NetworkJoin join) override;
	void PlayAlone() override;
	void Leave() override;
	void Update() override;
	[[nodiscard]] bool InGame() const override;
	[[nodiscard]] const online::NetworkSession* Session() const override { return _session.get(); }

private:
	void Detach();
	/// Loads the world state that the server sent after a resync
	void LoadWorld(const online::NetworkSession::WorldState& world);

	online::WebSocketClientInterface& _sockets;
	MakeTestClient _makeTestClient;
	/// The WSS client of a test join, which only the session uses
	std::unique_ptr<online::WebSocketClientInterface> _testClient;
	std::unique_ptr<online::NetworkSession> _session;
	bool _started {false};
	bool _endLogged {false};
	std::optional<uint32_t> _partAt;
	bool _partAgain {false};
};

} // namespace openblack::ecs::systems
