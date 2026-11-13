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
#include <string_view>
#include <vector>

#include "Common/DesyncReports.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "Online/ConnectionLog.h"
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
	void SetPlaysForClan(bool playsForClan) override;
	void PlayAlone() override;
	void EndNetworkGame() override;
	void Leave() override;
	void StopGame() override;
	[[nodiscard]] std::optional<game_socket::Start> TakeStart() override;
	void BeginPlay() override;
	void Update() override;
	[[nodiscard]] bool InGame() const override;
	[[nodiscard]] const online::NetworkSession* Session() const override { return _session.get(); }
	[[nodiscard]] std::optional<online::StartEvent> TakeStartEvent() override
	{
		return _session ? _session->TakeStartEvent() : std::nullopt;
	}
	void SendRoomChat(std::string text, std::string to, bool team) override
	{
		if (_session)
		{
			_session->SendRoomChat(text, to, team);
		}
	}
	[[nodiscard]] std::vector<game_socket::RoomChat> TakeRoomChat() override
	{
		return _session ? _session->TakeRoomChat() : std::vector<game_socket::RoomChat> {};
	}
	[[nodiscard]] std::vector<std::string> TakeObserversLeft() override
	{
		return _session ? _session->TakeObserversLeft() : std::vector<std::string>();
	}
	void LogConnection(std::string_view text) override { _log->Info(text); }
	[[nodiscard]] std::string DiagnosticsReport() const override;
	[[nodiscard]] std::string_view RoomName() const override { return _roomName; }

private:
	void Detach();
	/// The world does the turns of the session
	void Attach();
	/// Loads the world state that the server sent after a resync
	void LoadWorld(const online::NetworkSession::WorldState& world);
	/// The game ended or the session goes: the summary of the desyncs, once
	void FinishDesyncs();

	online::WebSocketClientInterface& _sockets;
	MakeTestClient _makeTestClient;
	ReadCreature _readCreature;
	bool _playsForClan {false};
	/// The name of the room of the last join
	std::string _roomName;
	/// The WSS client of a test join, which only the session uses
	std::unique_ptr<online::WebSocketClientInterface> _testClient;
	std::unique_ptr<online::NetworkSession> _session;
	/// The connection log of the network games of this computer. It lives longer than a session, so that the report
	/// after the end of a session has the lines of the session.
	std::shared_ptr<online::ConnectionLog> _log;
	bool _started {false};
	/// The game loads the server's land, and calls BeginPlay once it loaded
	bool _awaitingLand {false};
	/// The join came from the command line: the game loads the server's land of the server's reply to the join, before
	/// the player is ready
	bool _fromCommandLine {false};
	/// The reply of the server to the join of the command line was read
	bool _joinReplyRead {false};
	/// The clock stopped after the last turn of the game
	bool _stopped {false};
	std::optional<game_socket::Start> _pendingStart;
	bool _endLogged {false};
	std::optional<uint32_t> _partAt;
	bool _partAgain {false};
	/// The turn of a loaded world state, until it is here and the world goes on
	std::optional<uint32_t> _resumeTurn;
	/// The desync events that this run of the game can still send, for all its network games
	error_reports::DesyncRunBudget _desyncBudget;
	/// The desyncs of the network game, from the join to the end of the game
	std::optional<error_reports::DesyncReportRule> _desyncs;
};

} // namespace openblack::ecs::systems
