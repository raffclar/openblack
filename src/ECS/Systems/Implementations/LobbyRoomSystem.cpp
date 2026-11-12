/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LobbyRoomSystem.h"

#include <algorithm>
#include <utility>

#include <LobbiesJson.h>
#include <LobbyApiJson.h>
#include <fmt/format.h>

namespace openblack::ecs::systems
{

using online::HttpMethod;
using online::HttpResponse;

namespace
{

template <typename Value>
void Fail(const LobbyCallback<Value>& callback, LobbyError error)
{
	if (callback)
	{
		callback(LobbyResult<Value> {.value = std::nullopt, .error = std::move(error)});
	}
}

template <typename Value>
void Succeed(const LobbyCallback<Value>& callback, Value value)
{
	if (callback)
	{
		callback(LobbyResult<Value> {.value = std::move(value), .error = {}});
	}
}

LobbyError NoRoom()
{
	return LobbyError {.kind = LobbyErrorKind::Refused, .code = "no_room"};
}

LobbyError BadAnswer(const HttpResponse& response)
{
	return LobbyError {.kind = LobbyErrorKind::BadAnswer, .status = response.status};
}

/// The id of a lobby goes in the path of a call: only letters, digits and "-" can be in it
bool IsPathId(std::string_view id)
{
	return !id.empty() && std::ranges::all_of(id, [](char character) {
		return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
		       (character >= '0' && character <= '9') || character == '-';
	});
}

} // namespace

LobbyRoomSystem::LobbyRoomSystem(LobbyClientSystemInterface& client, std::string buildId, Clock clock)
    : _client(client)
    , _buildId(std::move(buildId))
    , _clock(std::move(clock))
{
	_listener = _client.AddEventListener([this](const lobby_api::PushEvent& event) { HandleEvent(event); });
}

LobbyRoomSystem::~LobbyRoomSystem()
{
	_client.RemoveEventListener(_listener);
}

void LobbyRoomSystem::Update()
{
	if (_room != nullptr && _clock)
	{
		_room->Tick(_clock());
	}
	FollowRoomEnd();
}

void LobbyRoomSystem::HandleEvent(const lobby_api::PushEvent& event)
{
	if (!event.seq.has_value())
	{
		// A new connection, or a resync, can come after lost events: the system gets the list and the room again
		if (event.type == lobby_api::events::k_Hello || event.type == lobby_api::events::k_Resync)
		{
			if (_watchingList)
			{
				_listTag.reset();
				RefreshList({});
			}
			ReloadRoom();
		}
		return;
	}
	_list.Apply(event);
	if (_room != nullptr)
	{
		_room->Apply(event);
		FollowRoomEnd();
	}
}

void LobbyRoomSystem::RefreshList(LobbyCallback<LobbyDone> callback)
{
	std::vector<online::HttpHeader> headers;
	if (_listTag.has_value())
	{
		headers.emplace_back("If-None-Match", *_listTag);
	}
	_client.SendApiCall(HttpMethod::Get, "/lobbies", {}, std::move(headers),
	                    [this, callback = std::move(callback)](LobbyResult<HttpResponse>&& result) {
		                    if (!result.Ok())
		                    {
			                    Fail(callback, std::move(result.error));
			                    return;
		                    }
		                    const auto& response = *result.value;
		                    if (response.status == 304 && _listTag.has_value())
		                    {
			                    // The list did not change
			                    Succeed(callback, LobbyDone {});
			                    return;
		                    }
		                    auto list = lobby_api::ParseLobbyList(response.Text());
		                    if (!list.has_value())
		                    {
			                    Fail(callback, BadAnswer(response));
			                    return;
		                    }
		                    _list.Replace(std::move(*list));
		                    if (const auto tag = response.Header("etag"); tag.has_value())
		                    {
			                    _listTag = std::string(*tag);
		                    }
		                    else
		                    {
			                    _listTag.reset();
		                    }
		                    Succeed(callback, LobbyDone {});
	                    });
}

void LobbyRoomSystem::WatchList(bool watch)
{
	if (_watchingList == watch)
	{
		return;
	}
	_watchingList = watch;
	_client.WatchChannel(std::string(lobby_api::k_LobbiesChannel), watch);
}

void LobbyRoomSystem::FindByCode(std::string code, LobbyCallback<std::string> callback)
{
	const auto normal = lobby_api::NormalLobbyCode(code);
	if (!normal.has_value())
	{
		Fail(callback, LobbyError {.kind = LobbyErrorKind::Refused, .code = "bad_code"});
		return;
	}
	_client.SendApiCall(HttpMethod::Get, fmt::format("/lobbies/by-code/{}", *normal), {}, {},
	                    [callback = std::move(callback)](LobbyResult<HttpResponse>&& result) {
		                    if (!result.Ok())
		                    {
			                    Fail(callback, std::move(result.error));
			                    return;
		                    }
		                    auto reply = lobby_api::ParseLobbyCodeReply(result.value->Text());
		                    if (!reply.has_value() || !IsPathId(reply->id))
		                    {
			                    Fail(callback, BadAnswer(*result.value));
			                    return;
		                    }
		                    Succeed(callback, std::move(reply->id));
	                    });
}

void LobbyRoomSystem::OpenRoom(lobby_api::Lobby lobby, bool reload)
{
	const auto account = _client.GetAccount();
	if (!account.has_value())
	{
		return;
	}
	if (!_roomChannel.empty())
	{
		_client.WatchChannel(_roomChannel, false);
	}
	++_roomGeneration;
	_room = std::make_unique<online::lobby::LobbyRoom>(std::move(lobby), *account);
	_roomChannel = _room->Channel();
	_client.WatchChannel(_roomChannel, true);
	if (reload)
	{
		ReloadRoom();
	}
}

void LobbyRoomSystem::ReloadRoom()
{
	if (_room == nullptr || _room->Phase() == online::lobby::RoomPhase::Ended)
	{
		return;
	}
	const auto generation = _roomGeneration;
	_client.SendApiCall(HttpMethod::Get, fmt::format("/lobbies/{}", _room->Data().id), {}, {},
	                    [this, generation](LobbyResult<HttpResponse>&& result) {
		                    if (generation != _roomGeneration || _room == nullptr)
		                    {
			                    return;
		                    }
		                    if (!result.Ok())
		                    {
			                    // A lobby that the service does not know now is closed
			                    if (result.error.status == 404 || result.error.status == 410)
			                    {
				                    _room->Finish(online::lobby::EndReason::Closed);
				                    FollowRoomEnd();
			                    }
			                    return;
		                    }
		                    if (auto lobby = lobby_api::ParseLobby(result.value->Text()); lobby.has_value())
		                    {
			                    _room->Reload(std::move(*lobby));
			                    FollowRoomEnd();
		                    }
	                    });
}

void LobbyRoomSystem::FollowRoomEnd()
{
	if (_room != nullptr && _room->Phase() == online::lobby::RoomPhase::Ended && !_roomChannel.empty())
	{
		_client.WatchChannel(_roomChannel, false);
		_roomChannel.clear();
	}
}

void LobbyRoomSystem::DismissRoom()
{
	if (_room == nullptr || _room->Phase() != online::lobby::RoomPhase::Ended)
	{
		return;
	}
	_room.reset();
	++_roomGeneration;
}

void LobbyRoomSystem::Create(lobby_api::LobbyCreateRequest request, LobbyCallback<LobbyDone> callback)
{
	if (_room != nullptr && _room->Phase() != online::lobby::RoomPhase::Ended)
	{
		Fail(callback, LobbyError {.kind = LobbyErrorKind::Refused, .code = "in_room"});
		return;
	}
	// A room that ended goes before the player enters a different room
	DismissRoom();
	request.buildId = _buildId;
	_client.SendApiCall(HttpMethod::Post, "/lobbies", lobby_api::ToJson(request), {},
	                    [this, callback = std::move(callback)](LobbyResult<HttpResponse>&& result) {
		                    if (!result.Ok())
		                    {
			                    Fail(callback, std::move(result.error));
			                    return;
		                    }
		                    auto lobby = lobby_api::ParseLobby(result.value->Text());
		                    if (!lobby.has_value() || !IsPathId(lobby->id))
		                    {
			                    Fail(callback, BadAnswer(*result.value));
			                    return;
		                    }
		                    OpenRoom(std::move(*lobby), false);
		                    Succeed(callback, LobbyDone {});
	                    });
}

void LobbyRoomSystem::Join(std::string lobbyId, std::optional<std::string> password, lobby_api::MemberRole role,
                           std::optional<int32_t> slot, LobbyCallback<LobbyDone> callback)
{
	if (_room != nullptr && _room->Phase() != online::lobby::RoomPhase::Ended)
	{
		Fail(callback, LobbyError {.kind = LobbyErrorKind::Refused, .code = "in_room"});
		return;
	}
	// A room that ended goes before the player enters a different room
	DismissRoom();
	if (!IsPathId(lobbyId))
	{
		Fail(callback, LobbyError {.kind = LobbyErrorKind::Refused, .code = "bad_lobby"});
		return;
	}
	const lobby_api::LobbyJoinRequest request {
	    .password = std::move(password),
	    .buildId = _buildId,
	    .role = role,
	    .slot = slot,
	};
	_client.SendApiCall(HttpMethod::Post, fmt::format("/lobbies/{}/members", lobbyId), lobby_api::ToJson(request), {},
	                    [this, lobbyId, callback = std::move(callback)](LobbyResult<HttpResponse>&& result) mutable {
		                    if (!result.Ok())
		                    {
			                    Fail(callback, std::move(result.error));
			                    return;
		                    }
		                    EnterLobby(std::move(lobbyId), std::move(callback));
	                    });
}

void LobbyRoomSystem::EnterLobby(std::string lobbyId, LobbyCallback<LobbyDone> callback)
{
	// The channel of the lobby opens before the system gets the lobby, so that no event comes between the two
	const auto channel = std::string(lobby_api::k_LobbyChannelPrefix) + lobbyId;
	if (!_roomChannel.empty() && _roomChannel != channel)
	{
		_client.WatchChannel(_roomChannel, false);
	}
	_roomChannel = channel;
	_client.WatchChannel(_roomChannel, true);
	_client.SendApiCall(HttpMethod::Get, fmt::format("/lobbies/{}", lobbyId), {}, {},
	                    [this, lobbyId, callback = std::move(callback)](LobbyResult<HttpResponse>&& result) {
		                    std::optional<lobby_api::Lobby> lobby;
		                    LobbyError error;
		                    if (!result.Ok())
		                    {
			                    error = std::move(result.error);
		                    }
		                    else if (lobby = lobby_api::ParseLobby(result.value->Text());
		                             !lobby.has_value() || lobby->id != lobbyId)
		                    {
			                    error = BadAnswer(*result.value);
		                    }
		                    else if (!_client.GetAccount().has_value())
		                    {
			                    error = LobbyError {.kind = LobbyErrorKind::NotLoggedIn};
		                    }
		                    if (error.kind != LobbyErrorKind::None)
		                    {
			                    // The player has no room: the events of the lobby stop
			                    if (_room == nullptr && !_roomChannel.empty())
			                    {
				                    _client.WatchChannel(_roomChannel, false);
				                    _roomChannel.clear();
			                    }
			                    Fail(callback, std::move(error));
			                    return;
		                    }
		                    // The channel is open, so the room needs no second snapshot
		                    _roomChannel.clear();
		                    OpenRoom(std::move(*lobby), false);
		                    Succeed(callback, LobbyDone {});
	                    });
}

void LobbyRoomSystem::QuickJoin(LobbyCallback<LobbyDone> callback)
{
	if (_room != nullptr && _room->Phase() != online::lobby::RoomPhase::Ended)
	{
		Fail(callback, LobbyError {.kind = LobbyErrorKind::Refused, .code = "in_room"});
		return;
	}
	// A room that ended goes before the player enters a different room
	DismissRoom();
	_client.SendApiCall(HttpMethod::Post, "/lobbies/quick-join",
	                    lobby_api::ToJson(lobby_api::QuickJoinRequest {.buildId = _buildId}), {},
	                    [this, callback = std::move(callback)](LobbyResult<HttpResponse>&& result) {
		                    if (!result.Ok())
		                    {
			                    Fail(callback, std::move(result.error));
			                    return;
		                    }
		                    auto lobby = lobby_api::ParseLobby(result.value->Text());
		                    if (!lobby.has_value() || !IsPathId(lobby->id))
		                    {
			                    Fail(callback, BadAnswer(*result.value));
			                    return;
		                    }
		                    // The answer came before the channel opened: the room gets the lobby again
		                    OpenRoom(std::move(*lobby), true);
		                    Succeed(callback, LobbyDone {});
	                    });
}

void LobbyRoomSystem::RoomCall(HttpMethod method, std::string subPath, std::string body, LobbyCallback<LobbyDone> callback,
                               Use use)
{
	if (_room == nullptr || _room->Phase() == online::lobby::RoomPhase::Ended)
	{
		Fail(callback, NoRoom());
		return;
	}
	const auto generation = _roomGeneration;
	_client.SendApiCall(
	    method, fmt::format("/lobbies/{}{}", _room->Data().id, subPath), std::move(body), {},
	    [this, generation, callback = std::move(callback), use = std::move(use)](LobbyResult<HttpResponse>&& result) {
		    const bool current = generation == _roomGeneration && _room != nullptr;
		    if (!result.Ok())
		    {
			    // The service gives 410 for a lobby that closed
			    if (current && result.error.status == 410)
			    {
				    _room->Finish(online::lobby::EndReason::Closed);
				    FollowRoomEnd();
			    }
			    Fail(callback, std::move(result.error));
			    return;
		    }
		    if (current && use)
		    {
			    use(*result.value);
			    FollowRoomEnd();
		    }
		    Succeed(callback, LobbyDone {});
	    });
}

void LobbyRoomSystem::Leave(LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Delete, "/members/me", {}, std::move(callback),
	         [this](const HttpResponse& /*response*/) { _room->Finish(online::lobby::EndReason::Left); });
}

void LobbyRoomSystem::SetReady(bool ready, LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Put, "/members/me", lobby_api::ToJson(lobby_api::MemberChangeRequest {.ready = ready}),
	         std::move(callback), [this](const HttpResponse& response) {
		         if (auto member = lobby_api::ParseLobbyMember(response.Text()); member.has_value())
		         {
			         _room->ApplyOwnMember(std::move(*member));
		         }
	         });
}

void LobbyRoomSystem::PickSlot(int32_t slot, LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Put, "/members/me", lobby_api::ToJson(lobby_api::MemberChangeRequest {.slot = slot}),
	         std::move(callback), [this](const HttpResponse& response) {
		         if (auto member = lobby_api::ParseLobbyMember(response.Text()); member.has_value())
		         {
			         _room->ApplyOwnMember(std::move(*member));
		         }
	         });
}

void LobbyRoomSystem::SendChat(std::string text, LobbyCallback<LobbyDone> callback)
{
	if (_room == nullptr || _room->Phase() == online::lobby::RoomPhase::Ended)
	{
		Fail(callback, NoRoom());
		return;
	}
	_client.SendChatLine(_room->Channel(), std::move(text), std::move(callback));
}

void LobbyRoomSystem::Kick(int64_t account, bool ban, LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Delete, fmt::format("/members/{}{}", account, ban ? "?ban=1" : ""), {}, std::move(callback));
}

void LobbyRoomSystem::MakeHost(int64_t account, LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Post, "/host", lobby_api::ToJson(lobby_api::HostRequest {.account = account}), std::move(callback));
}

void LobbyRoomSystem::Change(lobby_api::LobbyChangeRequest request, LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Patch, "", lobby_api::ToJson(request), std::move(callback));
}

void LobbyRoomSystem::Close(LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Delete, "", {}, std::move(callback),
	         [this](const HttpResponse& /*response*/) { _room->Finish(online::lobby::EndReason::OwnClose); });
}

void LobbyRoomSystem::Start(LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Post, "/start", {}, std::move(callback), [this](const HttpResponse& response) {
		if (const auto reply = lobby_api::ParseStartReply(response.Text()); reply.has_value())
		{
			_room->ApplyStart(*reply);
		}
	});
}

void LobbyRoomSystem::CancelStart(LobbyCallback<LobbyDone> callback)
{
	RoomCall(HttpMethod::Post, "/start/cancel", {}, std::move(callback));
}

void LobbyRoomSystem::RequestTicket(LobbyCallback<lobby_api::JoinTicket> callback)
{
	if (_room == nullptr || _room->Phase() == online::lobby::RoomPhase::Ended)
	{
		Fail(callback, NoRoom());
		return;
	}
	_client.SendApiCall(HttpMethod::Post, fmt::format("/lobbies/{}/tickets", _room->Data().id), {}, {},
	                    [callback = std::move(callback)](LobbyResult<HttpResponse>&& result) {
		                    if (!result.Ok())
		                    {
			                    Fail(callback, std::move(result.error));
			                    return;
		                    }
		                    auto ticket = lobby_api::ParseJoinTicket(result.value->Text());
		                    if (!ticket.has_value())
		                    {
			                    Fail(callback, BadAnswer(*result.value));
			                    return;
		                    }
		                    // The game socket is encrypted too
		                    if (!online::http_policy::IsWssAddress(ticket->url))
		                    {
			                    Fail(callback,
			                         LobbyError {.kind = LobbyErrorKind::AddressRefused, .status = result.value->status});
			                    return;
		                    }
		                    Succeed(callback, std::move(*ticket));
	                    });
}

} // namespace openblack::ecs::systems
