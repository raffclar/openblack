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

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "ECS/Systems/LobbyRoomSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class LobbyRoomSystem final: public LobbyRoomSystemInterface
{
public:
	/// The clock of the computer in milliseconds since 1970 (UTC). The service gives the start of the countdown as a time
	/// of the calendar.
	using Clock = std::function<int64_t()>;

	LobbyRoomSystem(LobbyClientSystemInterface& client, std::string buildId, Clock clock);
	~LobbyRoomSystem() override;
	LobbyRoomSystem(const LobbyRoomSystem&) = delete;
	LobbyRoomSystem& operator=(const LobbyRoomSystem&) = delete;
	LobbyRoomSystem(LobbyRoomSystem&&) = delete;
	LobbyRoomSystem& operator=(LobbyRoomSystem&&) = delete;

	void Update() override;
	[[nodiscard]] const std::string& BuildId() const override { return _buildId; }

	void RefreshList(LobbyCallback<LobbyDone> callback) override;
	void WatchList(bool watch) override;
	[[nodiscard]] const online::lobby::LobbyListModel& List() const override { return _list; }
	void FindByCode(std::string code, LobbyCallback<std::string> callback) override;

	void Create(lobby_api::LobbyCreateRequest request, LobbyCallback<LobbyDone> callback) override;
	void Join(std::string lobbyId, std::optional<std::string> password, lobby_api::MemberRole role, std::optional<int32_t> slot,
	          LobbyCallback<LobbyDone> callback) override;
	void QuickJoin(LobbyCallback<LobbyDone> callback) override;

	[[nodiscard]] const online::lobby::LobbyRoom* Room() const override { return _room.get(); }
	void DismissRoom() override;

	void Leave(LobbyCallback<LobbyDone> callback) override;
	void SetReady(bool ready, LobbyCallback<LobbyDone> callback) override;
	void PickSlot(int32_t slot, LobbyCallback<LobbyDone> callback) override;
	void ReportMap(lobby_api::MapReport report, LobbyCallback<LobbyDone> callback) override;
	void SendChat(std::string text, LobbyCallback<LobbyDone> callback) override;
	void Kick(int64_t account, bool ban, LobbyCallback<LobbyDone> callback) override;
	void MakeHost(int64_t account, LobbyCallback<LobbyDone> callback) override;
	void Change(lobby_api::LobbyChangeRequest request, LobbyCallback<LobbyDone> callback) override;
	void Close(LobbyCallback<LobbyDone> callback) override;
	void Start(LobbyCallback<LobbyDone> callback) override;
	void CancelStart(LobbyCallback<LobbyDone> callback) override;
	void RequestTicket(LobbyCallback<lobby_api::JoinTicket> callback) override;

private:
	using Use = std::function<void(const online::HttpResponse& response)>;

	/// Makes the room of a lobby and asks for the events of its channel. With a reload, the system gets the lobby again
	/// after the channel opens, so that the room has the events that came before.
	void OpenRoom(lobby_api::Lobby lobby, bool reload);
	void ReloadRoom();
	/// Uses the whole lobby that a call of the room gave back
	void ReloadFrom(const online::HttpResponse& response);
	/// Stops the events of the channel of a room that ended
	void FollowRoomEnd();
	/// A call of the room of the player. The function uses the answer before the callback runs.
	void RoomCall(online::HttpMethod method, std::string subPath, std::string body, LobbyCallback<LobbyDone> callback,
	              Use use = {});
	/// Gets the lobby after a join, and opens its room
	void EnterLobby(std::string lobbyId, LobbyCallback<LobbyDone> callback);
	void HandleEvent(const lobby_api::PushEvent& event);

	LobbyClientSystemInterface& _client;
	std::string _buildId;
	Clock _clock;
	uint32_t _listener {0};

	online::lobby::LobbyListModel _list;
	std::optional<std::string> _listTag;
	bool _watchingList {false};

	std::unique_ptr<online::lobby::LobbyRoom> _room;
	/// The channel of the room that the system watches, empty for none
	std::string _roomChannel;
	/// The number of the room. An answer that comes after the room changed is ignored.
	uint32_t _roomGeneration {0};
};

} // namespace openblack::ecs::systems
