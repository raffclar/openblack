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

#include <memory>
#include <optional>
#include <string>

#include "ECS/Systems/LobbyRoomSystemInterface.h"

/// The lobby sessions of the testbed: a lobby service in the game, with no network, that gives a lobby list and a room
/// for the screens of the lobby sessions
namespace openblack::testbed_multiplayer
{

class DemoLobbyRoom final: public ecs::systems::LobbyRoomSystemInterface
{
public:
	/// The player of the testbed, and the clock of the countdown in milliseconds since 1970 (UTC)
	DemoLobbyRoom(lobby_api::AccountRef self, int64_t nowMs);
	~DemoLobbyRoom() override;

	void Update() override {}
	[[nodiscard]] const std::string& BuildId() const override { return _build; }
	void RefreshList(ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void WatchList(bool) override {}
	[[nodiscard]] const online::lobby::LobbyListModel& List() const override { return _list; }
	void FindByCode(std::string code, ecs::systems::LobbyCallback<std::string> callback) override;
	void Create(lobby_api::LobbyCreateRequest request, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void Join(std::string lobbyId, std::optional<std::string> password, lobby_api::MemberRole role, std::optional<int32_t> slot,
	          ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void QuickJoin(ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	[[nodiscard]] const online::lobby::LobbyRoom* Room() const override { return _room.get(); }
	void DismissRoom() override { _room.reset(); }
	void Leave(ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void SetReady(bool ready, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void PickSlot(int32_t slot, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void ReportMap(lobby_api::MapReport report, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void SendChat(std::string text, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void Kick(int64_t account, bool ban, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void MakeHost(int64_t account, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void Change(lobby_api::LobbyChangeRequest request, ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void Close(ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void Start(ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void CancelStart(ecs::systems::LobbyCallback<ecs::systems::LobbyDone> callback) override;
	void RequestTicket(ecs::systems::LobbyCallback<lobby_api::JoinTicket> callback) override;

	// The steps of the scenarios
	/// Another member says a line in the chat of the room
	void SayInRoom(int64_t account, std::string text);
	/// The host of the room kicks the player
	void KickPlayer();
	/// The room waits in the line for a free game server
	void Queue(int32_t position);
	/// A member of the room gets the map, or loses it
	void SetMemberMap(int64_t account, bool haveMap);
	/// A different player comes into a god slot of the room
	void MemberJoins(const lobby_api::AccountRef& account, int32_t slot);
	/// A member of the room becomes ready
	void MemberReady(int64_t account);
	/// The host starts the countdown
	void StartCountdown();
	/// The lobby gives the room a game server after the countdown. Then the room gets this ticket.
	void AssignGame(std::string gameId, lobby_api::JoinTicket ticket);

private:
	/// Sends an event to the room, as the push channel of the room does
	void Send(std::string_view type, std::string data);
	[[nodiscard]] lobby_api::Lobby JoinedLobby(const lobby_api::LobbySummary& summary) const;

	lobby_api::AccountRef _self;
	int64_t _nowMs {0};
	std::string _build;
	online::lobby::LobbyListModel _list;
	std::unique_ptr<online::lobby::LobbyRoom> _room;
	int64_t _seq {0};
	/// The ticket of the game server that the lobby gave the room, none before AssignGame
	std::optional<lobby_api::JoinTicket> _ticket;
};

} // namespace openblack::testbed_multiplayer
