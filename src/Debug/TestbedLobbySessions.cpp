/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedLobbySessions.h"

#include <algorithm>
#include <utility>

#include <fmt/format.h>

#include "Online/GameBuildId.h"

using namespace openblack;
using namespace openblack::testbed_multiplayer;
using ecs::systems::LobbyCallback;
using ecs::systems::LobbyDone;
using ecs::systems::LobbyErrorKind;
using lobby_api::LobbyState;

namespace
{

constexpr int64_t k_Bob = 31;
constexpr int64_t k_Carol = 32;
constexpr int64_t k_Dan = 33;
constexpr std::string_view k_Code = "K7Q2MX9D";
constexpr std::string_view k_Password = "secret";
constexpr int64_t k_MsPerMinute = 60'000;
constexpr int64_t k_CountdownMs = 10'000;
/// The map of the service that the demonstration rooms play: the players download it
constexpr std::string_view k_RingHash = "00000000000000000000000000000000000000000000000000000000000000aa";

std::string Account(const lobby_api::AccountRef& account)
{
	return fmt::format(R"({{"id":{},"name":"{}"}})", account.id, account.name);
}

lobby_api::LobbySummary Summary(std::string id, std::string name, lobby_api::AccountRef host, std::string map, int32_t gods,
                                int32_t slots, LobbyState state, std::string build)
{
	return {
	    .id = std::move(id),
	    .name = std::move(name),
	    .host = std::move(host),
	    .map = {.file = "Online Maps/" + map + ".map", .name = map, .sha256 = std::string(k_RingHash), .size = 182'000},
	    .gods = gods,
	    .godSlots = slots,
	    .openSlots = slots,
	    .freeSlots = slots - gods,
	    .observers = 0,
	    .locked = false,
	    .state = state,
	    .queuePosition = std::nullopt,
	    .startedAt = std::nullopt,
	    .created = std::nullopt,
	    .buildId = std::move(build),
	};
}

/// The text in a JSON string
std::string Escaped(std::string_view text)
{
	std::string out;
	for (const char c : text)
	{
		if (c == '"' || c == '\\')
		{
			out += '\\';
		}
		if (static_cast<unsigned char>(c) >= 0x20)
		{
			out += c;
		}
	}
	return out;
}

} // namespace

DemoLobbyRoom::DemoLobbyRoom(lobby_api::AccountRef self, int64_t nowMs)
    : _self(std::move(self))
    , _nowMs(nowMs)
    , _build(online::build_id::Own())
{
}

DemoLobbyRoom::~DemoLobbyRoom() = default;

void DemoLobbyRoom::RefreshList(LobbyCallback<LobbyDone> callback)
{
	const lobby_api::AccountRef bob {.id = k_Bob, .name = "Bob"};
	const lobby_api::AccountRef carol {.id = k_Carol, .name = "Carol"};
	const lobby_api::AccountRef dan {.id = k_Dan, .name = "Dan"};
	std::vector<lobby_api::LobbySummary> lobbies;
	lobbies.push_back(Summary("d1", "Friday Ring", bob, "Ring of gods", 3, 8, LobbyState::Open, _build));
	auto locked = Summary("d2", "Clan practice", carol, "Two gods", 1, 2, LobbyState::Open, _build);
	locked.locked = true;
	lobbies.push_back(std::move(locked));
	lobbies.push_back(
	    Summary("d3", "Big battle of the thirty-two gods", dan, "Ring of gods", 32, 32, LobbyState::Open, _build));
	auto playing = Summary("d4", "Evening game", bob, "Three gods", 3, 3, LobbyState::InGame, _build);
	playing.startedAt = _nowMs - (42 * k_MsPerMinute);
	lobbies.push_back(std::move(playing));
	auto queued = Summary("d5", "Waiting room", carol, "Four gods", 4, 4, LobbyState::Queued, _build);
	queued.queuePosition = 2;
	lobbies.push_back(std::move(queued));
	lobbies.push_back(Summary("d6", "Old friends", dan, "Two gods", 1, 2, LobbyState::Open, "0.0.1+old/codec1"));
	_list.Replace({.lobbies = std::move(lobbies), .pool = lobby_api::PoolState {.free = 0, .total = 4}});
	if (callback)
	{
		callback({.value = LobbyDone {}, .error = {}});
	}
}

void DemoLobbyRoom::FindByCode(std::string code, LobbyCallback<std::string> callback)
{
	std::erase(code, '-');
	std::ranges::transform(code, code.begin(), [](char c) { return static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c); });
	if (code != k_Code)
	{
		callback({.value = std::nullopt, .error = {.kind = LobbyErrorKind::Refused, .status = 404, .code = "bad_code"}});
		return;
	}
	callback({.value = std::string("d1"), .error = {}});
}

lobby_api::Lobby DemoLobbyRoom::JoinedLobby(const lobby_api::LobbySummary& summary) const
{
	lobby_api::Lobby lobby {
	    .id = summary.id,
	    .code = std::string(k_Code),
	    .name = summary.name,
	    .host = summary.host,
	    .visibility = lobby_api::LobbyVisibility::Public,
	    .locked = summary.locked,
	    .map = summary.map,
	    .godSlots = summary.godSlots,
	    .slotKinds = std::vector<lobby_api::SlotKind>(static_cast<size_t>(summary.godSlots), lobby_api::SlotKind::Open),
	    .settings = {.timeLimitMinutes = 60, .conditions = {}, .observersAllowed = true},
	    .buildId = summary.buildId,
	    .state = LobbyState::Open,
	    .queuePosition = std::nullopt,
	    .countdownAt = std::nullopt,
	    .startedAt = std::nullopt,
	    .gameId = {},
	    .members = {},
	};
	if (lobby.godSlots > 6)
	{
		lobby.slotKinds[6] = lobby_api::SlotKind::Computer;
		lobby.slotKinds[7] = lobby_api::SlotKind::Closed;
	}
	// The host is ready. Carol still downloads the map of the service.
	lobby.members.push_back({.account = summary.host,
	                         .role = lobby_api::MemberRole::God,
	                         .slot = 0,
	                         .ready = true,
	                         .joinedAt = std::nullopt,
	                         .away = false,
	                         .haveMap = true,
	                         .host = true});
	if (summary.host.id != k_Carol)
	{
		lobby.members.push_back({.account = {.id = k_Carol, .name = "Carol"},
		                         .role = lobby_api::MemberRole::God,
		                         .slot = 1,
		                         .ready = false,
		                         .joinedAt = std::nullopt,
		                         .away = false,
		                         .haveMap = false,
		                         .host = false});
	}
	lobby.members.push_back({.account = _self,
	                         .role = lobby_api::MemberRole::God,
	                         .slot = 2,
	                         .ready = false,
	                         .joinedAt = std::nullopt,
	                         .away = false,
	                         .haveMap = true,
	                         .host = false});
	return lobby;
}

void DemoLobbyRoom::Create(lobby_api::LobbyCreateRequest request, LobbyCallback<LobbyDone> callback)
{
	lobby_api::Lobby lobby {
	    .id = "c1",
	    .code = std::string(k_Code),
	    .name = request.name,
	    .host = _self,
	    .visibility = request.visibility,
	    .locked = request.password.has_value(),
	    .map = request.map,
	    .godSlots = request.godSlots,
	    .slotKinds = request.slotKinds,
	    .settings = request.settings,
	    .buildId = _build,
	    .state = LobbyState::Open,
	    .queuePosition = std::nullopt,
	    .countdownAt = std::nullopt,
	    .startedAt = std::nullopt,
	    .gameId = {},
	    .members = {{.account = _self,
	                 .role = lobby_api::MemberRole::God,
	                 .slot = 0,
	                 .ready = false,
	                 .joinedAt = std::nullopt,
	                 .away = false,
	                 .haveMap = true,
	                 .host = true}},
	};
	_room = std::make_unique<online::lobby::LobbyRoom>(std::move(lobby), _self);
	// Two players come in: Bob is ready, and Carol downloads a map of the service
	Send(lobby_api::lobby_events::k_MemberJoined,
	     R"({"account":{"id":31,"name":"Bob"},"role":"god","slot":1,"have_map":true})");
	Send(lobby_api::lobby_events::k_MemberChanged, R"({"account":{"id":31,"name":"Bob"},"ready":true})");
	const bool download = !request.map.sha256.empty();
	Send(lobby_api::lobby_events::k_MemberJoined,
	     fmt::format(R"({{"account":{{"id":32,"name":"Carol"}},"role":"god","slot":2,"have_map":{}}})", !download));
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::Join(std::string lobbyId, std::optional<std::string> password, lobby_api::MemberRole,
                         std::optional<int32_t>, LobbyCallback<LobbyDone> callback)
{
	if (_list.Entries().empty())
	{
		RefreshList({});
	}
	const auto* summary = _list.Find(lobbyId);
	if (summary == nullptr)
	{
		callback({.value = std::nullopt, .error = {.kind = LobbyErrorKind::Refused, .status = 404, .code = "no_such_lobby"}});
		return;
	}
	if (summary->locked && password != k_Password)
	{
		const auto code =
		    password.has_value() ? lobby_api::lobby_problems::k_WrongPassword : lobby_api::lobby_problems::k_PasswordNeeded;
		callback({.value = std::nullopt, .error = {.kind = LobbyErrorKind::Refused, .status = 403, .code = std::string(code)}});
		return;
	}
	_room = std::make_unique<online::lobby::LobbyRoom>(JoinedLobby(*summary), _self);
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::QuickJoin(LobbyCallback<LobbyDone> callback)
{
	Join("d1", std::nullopt, lobby_api::MemberRole::God, std::nullopt, std::move(callback));
}

void DemoLobbyRoom::Leave(LobbyCallback<LobbyDone> callback)
{
	_room.reset();
	if (callback)
	{
		callback({.value = LobbyDone {}, .error = {}});
	}
}

void DemoLobbyRoom::Send(std::string_view type, std::string data)
{
	if (_room != nullptr)
	{
		_room->Apply({.seq = ++_seq, .type = std::string(type), .time = {}, .data = std::move(data)});
	}
}

void DemoLobbyRoom::SetReady(bool ready, LobbyCallback<LobbyDone> callback)
{
	Send(lobby_api::lobby_events::k_MemberChanged, fmt::format(R"({{"account":{},"ready":{}}})", Account(_self), ready));
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::PickSlot(int32_t slot, LobbyCallback<LobbyDone> callback)
{
	Send(lobby_api::lobby_events::k_MemberChanged, fmt::format(R"({{"account":{},"slot":{}}})", Account(_self), slot));
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::ReportMap(lobby_api::MapReport report, LobbyCallback<LobbyDone> callback)
{
	SetMemberMap(_self.id, report.haveMap);
	if (callback)
	{
		callback({.value = LobbyDone {}, .error = {}});
	}
}

void DemoLobbyRoom::SetMemberMap(int64_t account, bool haveMap)
{
	if (_room == nullptr)
	{
		return;
	}
	const auto* member = _room->FindMember(account);
	if (member != nullptr)
	{
		Send(lobby_api::lobby_events::k_MemberChanged,
		     fmt::format(R"({{"account":{},"have_map":{}}})", Account(member->account), haveMap));
	}
}

void DemoLobbyRoom::SendChat(std::string text, LobbyCallback<LobbyDone> callback)
{
	if (_room != nullptr)
	{
		SayInRoom(_self.id, std::move(text));
	}
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::SayInRoom(int64_t account, std::string text)
{
	if (_room == nullptr)
	{
		return;
	}
	const auto* member = _room->FindMember(account);
	if (member == nullptr)
	{
		return;
	}
	Send(lobby_api::events::k_ChatLine,
	     fmt::format(R"({{"channel":"{}","from":{},"text":"{}"}})", _room->Channel(), Account(member->account), Escaped(text)));
}

void DemoLobbyRoom::Kick(int64_t account, bool ban, LobbyCallback<LobbyDone> callback)
{
	if (_room != nullptr && _room->FindMember(account) != nullptr)
	{
		const auto* member = _room->FindMember(account);
		Send(ban ? lobby_api::lobby_events::k_MemberBanned : lobby_api::lobby_events::k_MemberKicked,
		     fmt::format(R"({{"account":{},"by":{}}})", Account(member->account), Account(_self)));
	}
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::MakeHost(int64_t account, LobbyCallback<LobbyDone> callback)
{
	if (_room != nullptr && _room->FindMember(account) != nullptr)
	{
		Send(lobby_api::lobby_events::k_HostChanged,
		     fmt::format(R"({{"account":{},"by":{}}})", Account(_room->FindMember(account)->account), Account(_self)));
	}
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::Change(lobby_api::LobbyChangeRequest request, LobbyCallback<LobbyDone> callback)
{
	if (_room == nullptr)
	{
		callback({.value = LobbyDone {}, .error = {}});
		return;
	}
	auto lobby = _room->Data();
	if (request.map.has_value())
	{
		lobby.map = *request.map;
		for (auto& member : lobby.members)
		{
			member.ready = false;
			member.haveMap = member.account.id == _self.id || request.map->sha256.empty();
		}
	}
	if (request.slotKinds.has_value())
	{
		lobby.slotKinds = *request.slotKinds;
	}
	if (request.password.has_value())
	{
		lobby.locked = !request.password->empty();
	}
	if (request.name.has_value())
	{
		lobby.name = *request.name;
	}
	_room->Reload(std::move(lobby));
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::Close(LobbyCallback<LobbyDone> callback)
{
	Send(lobby_api::lobby_events::k_LobbyClosed, R"({"reason":"host_closed"})");
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::Start(LobbyCallback<LobbyDone> callback)
{
	if (_room != nullptr)
	{
		_room->ApplyStart({.state = LobbyState::Countdown, .at = _nowMs + k_CountdownMs, .position = std::nullopt});
	}
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::CancelStart(LobbyCallback<LobbyDone> callback)
{
	Send(lobby_api::lobby_events::k_StartCancelled, R"({"why":"host_cancelled"})");
	callback({.value = LobbyDone {}, .error = {}});
}

void DemoLobbyRoom::RequestTicket(LobbyCallback<lobby_api::JoinTicket> callback)
{
	// The demonstration has no game server
	callback({.value = std::nullopt, .error = {.kind = LobbyErrorKind::Network}});
}

void DemoLobbyRoom::KickPlayer()
{
	if (_room != nullptr)
	{
		Send(lobby_api::lobby_events::k_LobbyKicked,
		     fmt::format(R"({{"lobby":"{}","by":{},"banned":false}})", _room->Data().id, Account(_room->Data().host)));
	}
}

void DemoLobbyRoom::Queue(int32_t position)
{
	if (_room != nullptr)
	{
		_room->ApplyStart({.state = LobbyState::Queued, .at = std::nullopt, .position = position});
	}
}
