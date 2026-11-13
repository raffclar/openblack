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

#include <optional>
#include <string>

#include <Lobbies.h>
#include <LobbyApi.h>

#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "Online/LobbyRoom.h"

namespace openblack::ecs::systems
{

/// The lobby rooms of the lobby service (bwgame-service): the lobby list, and the lobby room of the player from its
/// creation or join to the start of its game. The calls go through the lobby client with the session of the player, so
/// that the desktop game and the browser game use the same calls. The events of the push channels "lobbies" and
/// "lobby-<id>" keep the models current.
///
/// Each call returns at once, and its callback runs later in a frame, as for the lobby client. A call that needs a room
/// fails with Refused when the player has no room.
class LobbyRoomSystemInterface
{
public:
	virtual ~LobbyRoomSystemInterface() = default;

	/// Once a frame, after the lobby client: the countdown of the room moves on
	virtual void Update() = 0;

	/// The build id of this game, which the lobby list and the lobby compare
	[[nodiscard]] virtual const std::string& BuildId() const = 0;

	/// Gets the lobby list again. The call sends the tag of the list that the system has.
	virtual void RefreshList(LobbyCallback<LobbyDone> callback) = 0;
	/// Asks for the changes of the lobby list on the push channel, or stops them
	virtual void WatchList(bool watch) = 0;
	[[nodiscard]] virtual const online::lobby::LobbyListModel& List() const = 0;
	/// The id of a private lobby from its code
	virtual void FindByCode(std::string code, LobbyCallback<std::string> callback) = 0;

	/// Makes a lobby with the player as its host, and opens its room. The system adds the build id.
	virtual void Create(lobby_api::LobbyCreateRequest request, LobbyCallback<LobbyDone> callback) = 0;
	/// Joins a lobby with its password, when it needs one, and opens its room
	virtual void Join(std::string lobbyId, std::optional<std::string> password, lobby_api::MemberRole role,
	                  std::optional<int32_t> slot, LobbyCallback<LobbyDone> callback) = 0;
	/// Joins the open public lobby that the service chooses, and opens its room
	virtual void QuickJoin(LobbyCallback<LobbyDone> callback) = 0;

	/// The room of the player, or none. A room in the phase Ended stays until the screens dismiss it.
	[[nodiscard]] virtual const online::lobby::LobbyRoom* Room() const = 0;
	/// Forgets a room in the phase Ended, after the screens showed its end
	virtual void DismissRoom() = 0;

	virtual void Leave(LobbyCallback<LobbyDone> callback) = 0;
	virtual void SetReady(bool ready, LobbyCallback<LobbyDone> callback) = 0;
	/// Moves the player to a free god slot
	virtual void PickSlot(int32_t slot, LobbyCallback<LobbyDone> callback) = 0;
	/// Sends a chat line to the room
	virtual void SendChat(std::string text, LobbyCallback<LobbyDone> callback) = 0;

	/// The host removes a member. A ban refuses the member until the lobby closes.
	virtual void Kick(int64_t account, bool ban, LobbyCallback<LobbyDone> callback) = 0;
	/// The host gives the host to a different member
	virtual void MakeHost(int64_t account, LobbyCallback<LobbyDone> callback) = 0;
	/// The host changes the lobby. The change clears the ready of each member.
	virtual void Change(lobby_api::LobbyChangeRequest request, LobbyCallback<LobbyDone> callback) = 0;
	/// The host closes the lobby before the start. Each member goes back to the lobby list.
	virtual void Close(LobbyCallback<LobbyDone> callback) = 0;
	/// The host starts the countdown
	virtual void Start(LobbyCallback<LobbyDone> callback) = 0;
	/// The host stops the countdown or the wait in the line
	virtual void CancelStart(LobbyCallback<LobbyDone> callback) = 0;
	/// A ticket for the game server of the room, after the service gave the room a game server
	virtual void RequestTicket(LobbyCallback<lobby_api::JoinTicket> callback) = 0;
};

} // namespace openblack::ecs::systems
