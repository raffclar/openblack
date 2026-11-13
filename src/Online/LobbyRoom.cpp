/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyRoom.h"

#include <algorithm>
#include <utility>

#include <LobbiesJson.h>
#include <LobbyApiJson.h>
#include <fmt/format.h>

namespace openblack::online::lobby
{
namespace
{

namespace events = lobby_api::lobby_events;

/// The texts that are the same as the texts of vanilla's multiplayer front end
constexpr std::string_view k_HostLeftText = "The channel host left the channel that you were in.";
constexpr std::string_view k_KickedText = "{} has kicked you from the game.";

char LowerAscii(char character)
{
	return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
}

/// True when the text has the part, with no difference between capital and small letters
bool ContainsFolded(std::string_view text, std::string_view part)
{
	if (part.empty())
	{
		return true;
	}
	const auto found = std::ranges::search(text, part, [](char a, char b) { return LowerAscii(a) == LowerAscii(b); });
	return !found.empty();
}

RoomPhase PhaseOf(lobby_api::LobbyState state)
{
	switch (state)
	{
	case lobby_api::LobbyState::Open:
		return RoomPhase::Open;
	case lobby_api::LobbyState::Countdown:
		return RoomPhase::Countdown;
	case lobby_api::LobbyState::Queued:
		return RoomPhase::Queued;
	case lobby_api::LobbyState::Assigned:
		return RoomPhase::Assigned;
	case lobby_api::LobbyState::InGame:
		return RoomPhase::InGame;
	case lobby_api::LobbyState::Ended:
	case lobby_api::LobbyState::Closed:
		break;
	}
	return RoomPhase::Ended;
}

} // namespace

std::optional<std::string> EndMessage(const RoomEnd& end, lobby_api::ClientKind client)
{
	switch (end.reason)
	{
	case EndReason::Left:
	case EndReason::OwnClose:
	case EndReason::GameEnded:
		return std::nullopt;
	case EndReason::HostClosed:
		return "The host closed the game.";
	case EndReason::HostLeft:
		return std::string(k_HostLeftText);
	case EndReason::Kicked:
	case EndReason::Banned:
		return fmt::format(fmt::runtime(k_KickedText), end.by);
	case EndReason::GameFailed:
		return "The game server stopped.";
	case EndReason::NewVersion:
		return client == lobby_api::ClientKind::Browser
		           ? "A new version of openblack is live. Reload the page."
		           : "A new version of openblack is live. Get the new version at bw.zanidrak.com";
	case EndReason::Closed:
		return "The game was closed.";
	}
	return std::nullopt;
}

std::string SystemLineText(const SystemLine& line)
{
	switch (line.kind)
	{
	case SystemLineKind::Joined:
		return fmt::format("{} joined the game.", line.subject);
	case SystemLineKind::Left:
		return fmt::format("{} left the game.", line.subject);
	case SystemLineKind::Kicked:
		return fmt::format("{} was kicked by {}.", line.subject, line.actor);
	case SystemLineKind::Banned:
		return fmt::format("{} was banned by {}.", line.subject, line.actor);
	case SystemLineKind::NewHost:
		return fmt::format("{} is now the host.", line.subject);
	case SystemLineKind::MapChanged:
		return fmt::format("The host changed the map to {}.", line.subject);
	case SystemLineKind::SettingsChanged:
		return "The host changed the settings.";
	case SystemLineKind::ReadyCleared:
		return "Ready was cleared.";
	case SystemLineKind::Countdown:
		return fmt::format("The game starts in {}...", line.number);
	case SystemLineKind::NotReady:
		return fmt::format("The start was cancelled: {} is not ready.", line.subject);
	case SystemLineKind::StartCancelled:
		return "The start was cancelled.";
	case SystemLineKind::CouldNotStart:
		return "The game could not start.";
	case SystemLineKind::NotEnoughJoined:
		return "Not enough players joined the server.";
	case SystemLineKind::Queued:
		return fmt::format("Waiting for a free server: you are number {} in the line.", line.number);
	}
	return {};
}

std::string RoomLineText(const RoomLine& line)
{
	switch (line.kind)
	{
	case RoomLineKind::Own:
		return fmt::format("{}> {}", line.from, line.text);
	case RoomLineKind::Other:
		return fmt::format("{}: {}", line.from, line.text);
	case RoomLineKind::System:
		break;
	}
	return SystemLineText(line.system);
}

LobbyRoom::LobbyRoom(lobby_api::Lobby lobby, lobby_api::AccountRef self)
    : _lobby(std::move(lobby))
    , _self(std::move(self))
    , _phase(PhaseOf(_lobby.state))
{
	if (_phase == RoomPhase::Ended)
	{
		_end = RoomEnd {.reason = EndReason::Closed};
	}
}

std::string LobbyRoom::Channel() const
{
	return std::string(lobby_api::k_LobbyChannelPrefix) + _lobby.id;
}

const lobby_api::LobbyMember* LobbyRoom::FindMember(int64_t account) const
{
	const auto found =
	    std::ranges::find_if(_lobby.members, [account](const auto& member) { return member.account.id == account; });
	return found != _lobby.members.end() ? &*found : nullptr;
}

lobby_api::LobbyMember* LobbyRoom::MutableMember(int64_t account)
{
	const auto found =
	    std::ranges::find_if(_lobby.members, [account](const auto& member) { return member.account.id == account; });
	return found != _lobby.members.end() ? &*found : nullptr;
}

lobby_api::SlotKind LobbyRoom::KindOf(int32_t slot) const
{
	const auto index = static_cast<size_t>(slot);
	return slot >= 0 && index < _lobby.slotKinds.size() ? _lobby.slotKinds[index] : lobby_api::SlotKind::Open;
}

bool LobbyRoom::IsBeforeStart() const noexcept
{
	return _phase == RoomPhase::Open || _phase == RoomPhase::Countdown || _phase == RoomPhase::Queued;
}

std::vector<SlotView> LobbyRoom::Slots() const
{
	std::vector<SlotView> views;
	views.reserve(static_cast<size_t>(std::max(_lobby.godSlots, 0)));
	for (int32_t slot = 0; slot < _lobby.godSlots; ++slot)
	{
		SlotView view {.slot = slot};
		const auto member = std::ranges::find_if(
		    _lobby.members, [slot](const auto& each) { return each.role == lobby_api::MemberRole::God && each.slot == slot; });
		if (member != _lobby.members.end())
		{
			view.holder = SlotHolder::Player;
			view.account = member->account;
			view.ready = member->ready;
			view.host = member->account.id == _lobby.host.id;
			view.own = member->account.id == _self.id;
			view.away = member->away;
			view.haveMap = member->haveMap;
		}
		else
		{
			switch (KindOf(slot))
			{
			case lobby_api::SlotKind::Open:
				view.holder = SlotHolder::Open;
				break;
			case lobby_api::SlotKind::Computer:
				view.holder = SlotHolder::Computer;
				break;
			case lobby_api::SlotKind::Closed:
				view.holder = SlotHolder::Closed;
				break;
			}
		}
		views.push_back(std::move(view));
	}
	return views;
}

std::vector<lobby_api::AccountRef> LobbyRoom::Observers() const
{
	std::vector<lobby_api::AccountRef> observers;
	for (const auto& member : _lobby.members)
	{
		if (member.role == lobby_api::MemberRole::Observer)
		{
			observers.push_back(member.account);
		}
	}
	return observers;
}

int32_t LobbyRoom::GodsInSlots() const
{
	return static_cast<int32_t>(std::ranges::count_if(_lobby.members, [](const auto& member) {
		return member.role == lobby_api::MemberRole::God && member.slot.has_value();
	}));
}

bool LobbyRoom::CanStart() const
{
	if (_phase != RoomPhase::Open || !IsHost() || GodsInSlots() < k_LeastGods)
	{
		return false;
	}
	return std::ranges::all_of(_lobby.members, [this](const auto& member) {
		const bool god = member.role == lobby_api::MemberRole::God && member.slot.has_value();
		// The service sets the ready of the host at the start, and refuses a host with no map
		return !god || member.ready || (member.account.id == _self.id && member.haveMap);
	});
}

bool LobbyRoom::CanReady() const
{
	// The service lets a member be ready only with the map of the lobby
	const auto* own = OwnMember();
	return _phase == RoomPhase::Open && own != nullptr && own->role == lobby_api::MemberRole::God && own->slot.has_value() &&
	       own->haveMap;
}

bool LobbyRoom::CanPickSlot(int32_t slot) const
{
	const auto* own = OwnMember();
	if (_phase != RoomPhase::Open || own == nullptr || own->ready || slot < 0 || slot >= _lobby.godSlots ||
	    KindOf(slot) != lobby_api::SlotKind::Open)
	{
		return false;
	}
	return std::ranges::none_of(_lobby.members, [slot](const auto& member) {
		return member.role == lobby_api::MemberRole::God && member.slot == slot;
	});
}

void LobbyRoom::AddLine(RoomLine line)
{
	if (_lines.size() >= k_MostRoomLines)
	{
		_lines.erase(_lines.begin());
	}
	_lines.push_back(std::move(line));
	Changed();
}

void LobbyRoom::AddSystemLine(SystemLine line)
{
	AddLine(RoomLine {.kind = RoomLineKind::System, .system = std::move(line)});
}

void LobbyRoom::Finish(EndReason reason, std::string by)
{
	if (_phase == RoomPhase::Ended)
	{
		return;
	}
	_phase = RoomPhase::Ended;
	_end = RoomEnd {.reason = reason, .by = std::move(by)};
	Changed();
}

void LobbyRoom::Apply(const lobby_api::PushEvent& event)
{
	if (_phase == RoomPhase::Ended)
	{
		return;
	}
	const std::string_view type = event.type;
	if (type == events::k_LobbyKicked)
	{
		// The host kicked or banned the player. The service sends this to the player only.
		const auto kick = lobby_api::ParseLobbyKick(event.data);
		if (kick.has_value() && kick->lobby == _lobby.id)
		{
			std::erase_if(_lobby.members, [this](const auto& member) { return member.account.id == _self.id; });
			const auto by = kick->by.has_value() ? kick->by->name : std::string();
			Finish(kick->banned ? EndReason::Banned : EndReason::Kicked, by);
		}
		return;
	}
	if (type == lobby_api::events::k_ChatLine)
	{
		const auto line = lobby_api::ParseChatLine(event.data);
		if (line.has_value() && line->channel == Channel())
		{
			const bool own = line->from.id == _self.id;
			AddLine(
			    RoomLine {.kind = own ? RoomLineKind::Own : RoomLineKind::Other, .from = line->from.name, .text = line->text});
		}
		return;
	}
	if (type == events::k_MemberJoined || type == events::k_MemberLeft || type == events::k_MemberKicked ||
	    type == events::k_MemberBanned || type == events::k_MemberChanged || type == events::k_MemberAway ||
	    type == events::k_MemberBack || type == events::k_HostChanged)
	{
		const auto member = lobby_api::ParseMemberEvent(event.data);
		if (member.has_value() && (member->lobby.empty() || member->lobby == _lobby.id))
		{
			ApplyMemberEvent(type, *member);
		}
		return;
	}
	if (type == events::k_LobbyChanged)
	{
		const auto change = lobby_api::ParseLobbyChange(event.data);
		if (change.has_value() && (change->lobby.empty() || change->lobby == _lobby.id))
		{
			ApplyChange(*change);
		}
		return;
	}
	if (type == events::k_StartCountdown || type == events::k_StartCancelled || type == events::k_StartQueued ||
	    type == events::k_GameAssigned || type == events::k_GameStarted || type == events::k_GameEnded ||
	    type == events::k_LobbyClosed)
	{
		const auto state = lobby_api::ParseLobbyStateEvent(event.data);
		if (state.has_value() && (state->lobby.empty() || state->lobby == _lobby.id))
		{
			ApplyStateEvent(type, *state);
		}
	}
}

void LobbyRoom::ApplyMemberEvent(std::string_view type, const lobby_api::MemberEvent& event)
{
	const bool self = event.account.id == _self.id;
	const auto byName = event.by.has_value() ? event.by->name : _lobby.host.name;
	if (type == events::k_MemberJoined)
	{
		if (auto* member = MutableMember(event.account.id); member != nullptr)
		{
			member->role = event.role.value_or(member->role);
			member->slot = event.slot;
			Changed();
			return;
		}
		_lobby.members.push_back(lobby_api::LobbyMember {
		    .account = event.account,
		    .role = event.role.value_or(lobby_api::MemberRole::God),
		    .slot = event.slot,
		    .haveMap = event.haveMap.value_or(true),
		});
		Changed();
		if (!self)
		{
			AddSystemLine({.kind = SystemLineKind::Joined, .subject = event.account.name});
		}
		return;
	}
	if (type == events::k_MemberLeft || type == events::k_MemberKicked || type == events::k_MemberBanned)
	{
		const bool wasHost = event.account.id == _lobby.host.id;
		std::erase_if(_lobby.members, [&event](const auto& member) { return member.account.id == event.account.id; });
		if (type == events::k_MemberLeft)
		{
			AddSystemLine({.kind = SystemLineKind::Left, .subject = event.account.name});
		}
		else
		{
			const auto kind = type == events::k_MemberKicked ? SystemLineKind::Kicked : SystemLineKind::Banned;
			AddSystemLine({.kind = kind, .subject = event.account.name, .actor = byName});
		}
		if (self)
		{
			const auto reason = type == events::k_MemberLeft     ? EndReason::Left
			                    : type == events::k_MemberKicked ? EndReason::Kicked
			                                                     : EndReason::Banned;
			Finish(reason, type == events::k_MemberLeft ? std::string {} : byName);
		}
		else if (wasHost && IsBeforeStart())
		{
			// The lobby ends when its host goes before the start (user answer Q4): each member goes back to the list
			Finish(EndReason::HostLeft);
		}
		return;
	}
	auto* member = MutableMember(event.account.id);
	if (type == events::k_HostChanged)
	{
		_lobby.host = event.account;
		for (auto& each : _lobby.members)
		{
			each.host = each.account.id == event.account.id;
		}
		AddSystemLine({.kind = SystemLineKind::NewHost, .subject = event.account.name});
		return;
	}
	if (member == nullptr)
	{
		return;
	}
	if (type == events::k_MemberChanged)
	{
		if (event.ready.has_value())
		{
			member->ready = *event.ready;
		}
		if (event.slot.has_value())
		{
			member->slot = event.slot;
		}
		if (event.role.has_value())
		{
			member->role = *event.role;
		}
		if (event.haveMap.has_value())
		{
			member->haveMap = *event.haveMap;
		}
	}
	else
	{
		member->away = type == events::k_MemberAway;
	}
	Changed();
}

void LobbyRoom::ApplyChange(const lobby_api::LobbyChange& change)
{
	// The event names the fields only: the room gets the lobby again, and the lines come with the new values
	for (const auto& field : change.fields)
	{
		if (std::ranges::find(_changedFields, field) == _changedFields.end())
		{
			_changedFields.push_back(field);
		}
	}
	if (change.readyCleared)
	{
		for (auto& member : _lobby.members)
		{
			member.ready = false;
		}
		_readyClearedLine = true;
	}
	if (change.mapCleared)
	{
		for (auto& member : _lobby.members)
		{
			member.haveMap = false;
		}
	}
	_reloadWanted = true;
	Changed();
}

void LobbyRoom::WriteChangeLines(const lobby_api::LobbyMap& oldMap)
{
	const bool mapChanged = oldMap.file != _lobby.map.file || oldMap.sha256 != _lobby.map.sha256;
	const bool otherChanged = std::ranges::any_of(_changedFields, [](const auto& field) { return field != "map"; });
	if (mapChanged)
	{
		AddSystemLine(
		    {.kind = SystemLineKind::MapChanged, .subject = !_lobby.map.name.empty() ? _lobby.map.name : _lobby.map.file});
	}
	else if (otherChanged || !_changedFields.empty())
	{
		AddSystemLine({.kind = SystemLineKind::SettingsChanged});
	}
	if (_readyClearedLine)
	{
		AddSystemLine({.kind = SystemLineKind::ReadyCleared});
	}
	_changedFields.clear();
	_readyClearedLine = false;
}

void LobbyRoom::StartCountdown(std::optional<int64_t> at)
{
	_phase = RoomPhase::Countdown;
	_lobby.state = lobby_api::LobbyState::Countdown;
	_lobby.countdownAt = at;
	_lobby.queuePosition.reset();
	_announced = k_CountdownSeconds + 1;
	Changed();
}

void LobbyRoom::EnterQueue(std::optional<int32_t> position)
{
	_phase = RoomPhase::Queued;
	_lobby.state = lobby_api::LobbyState::Queued;
	_lobby.countdownAt.reset();
	if (position.has_value() && position != _lobby.queuePosition)
	{
		_lobby.queuePosition = position;
		AddSystemLine({.kind = SystemLineKind::Queued, .number = *position});
	}
	Changed();
}

void LobbyRoom::ApplyStateEvent(std::string_view type, const lobby_api::LobbyStateEvent& event)
{
	if (type == events::k_StartCountdown)
	{
		StartCountdown(event.at);
		return;
	}
	if (type == events::k_StartCancelled)
	{
		_phase = RoomPhase::Open;
		_lobby.state = lobby_api::LobbyState::Open;
		_lobby.countdownAt.reset();
		_lobby.queuePosition.reset();
		if (event.why == "not_ready" && event.account.has_value())
		{
			AddSystemLine({.kind = SystemLineKind::NotReady, .subject = event.account->name});
		}
		else if (event.why == "server_failed")
		{
			AddSystemLine({.kind = SystemLineKind::CouldNotStart});
		}
		else if (event.why == "not_enough_joined")
		{
			AddSystemLine({.kind = SystemLineKind::NotEnoughJoined});
		}
		else
		{
			AddSystemLine({.kind = SystemLineKind::StartCancelled});
		}
		return;
	}
	if (type == events::k_StartQueued)
	{
		EnterQueue(event.position);
		return;
	}
	if (type == events::k_GameAssigned)
	{
		_phase = RoomPhase::Assigned;
		_lobby.state = lobby_api::LobbyState::Assigned;
		_lobby.gameId = event.gameId;
		_gameUrl = event.url;
		_lobby.countdownAt.reset();
		_lobby.queuePosition.reset();
		Changed();
		return;
	}
	if (type == events::k_GameStarted)
	{
		_phase = RoomPhase::InGame;
		_lobby.state = lobby_api::LobbyState::InGame;
		Changed();
		return;
	}
	if (type == events::k_GameEnded)
	{
		Finish(EndReason::GameEnded);
		return;
	}
	// The close of the lobby
	if (event.reason == "host_closed")
	{
		Finish(IsHost() ? EndReason::OwnClose : EndReason::HostClosed);
	}
	else if (event.reason == "host_left" || event.reason == "host_away")
	{
		Finish(EndReason::HostLeft);
	}
	else if (event.reason == "ended")
	{
		Finish(EndReason::GameEnded);
	}
	else if (event.reason == "new_version")
	{
		Finish(EndReason::NewVersion);
	}
	else if (event.reason == "server_stopped")
	{
		Finish(EndReason::GameFailed);
	}
	else
	{
		Finish(EndReason::Closed);
	}
}

void LobbyRoom::Tick(int64_t nowMs)
{
	if (_phase != RoomPhase::Countdown || !_lobby.countdownAt.has_value())
	{
		return;
	}
	constexpr int64_t k_MsPerSecond = 1000;
	const auto left = *_lobby.countdownAt - nowMs;
	// The seconds that are left, rounded up: 4.2 seconds show as 5
	const auto seconds = left <= 0 ? 0 : static_cast<int32_t>((left + k_MsPerSecond - 1) / k_MsPerSecond);
	if (seconds < 1 || seconds > k_CountdownSeconds || seconds >= _announced)
	{
		return;
	}
	_announced = seconds;
	AddSystemLine({.kind = SystemLineKind::Countdown, .number = seconds});
}

void LobbyRoom::Reload(lobby_api::Lobby lobby)
{
	if (_phase == RoomPhase::Ended || lobby.id != _lobby.id)
	{
		return;
	}
	const bool wasHost = IsHost();
	const auto oldMap = _lobby.map;
	// The lobby does not give its game server: the room keeps the one of "game.assigned"
	auto gameId = std::move(_lobby.gameId);
	_lobby = std::move(lobby);
	if (_lobby.gameId.empty())
	{
		_lobby.gameId = std::move(gameId);
	}
	_phase = PhaseOf(_lobby.state);
	Changed();
	if (_phase == RoomPhase::Ended)
	{
		_phase = RoomPhase::Open;
		Finish(EndReason::Closed);
		return;
	}
	if (OwnMember() == nullptr)
	{
		// The player is not a member now: the events that told why were lost
		Finish(EndReason::Closed);
		return;
	}
	WriteChangeLines(oldMap);
	if (!wasHost && IsHost())
	{
		AddSystemLine({.kind = SystemLineKind::NewHost, .subject = _lobby.host.name});
	}
}

void LobbyRoom::ApplyOwnMember(lobby_api::LobbyMember member)
{
	if (_phase == RoomPhase::Ended || member.account.id != _self.id)
	{
		return;
	}
	if (auto* own = MutableMember(_self.id); own != nullptr)
	{
		*own = std::move(member);
	}
	else
	{
		_lobby.members.push_back(std::move(member));
	}
	Changed();
}

void LobbyRoom::ApplyStart(const lobby_api::StartReply& reply)
{
	if (_phase == RoomPhase::Ended)
	{
		return;
	}
	if (reply.state == lobby_api::LobbyState::Countdown && _phase == RoomPhase::Open)
	{
		StartCountdown(reply.at);
	}
	else if (reply.state == lobby_api::LobbyState::Queued)
	{
		EnterQueue(reply.position);
	}
}

bool IsWrongVersion(const lobby_api::LobbySummary& lobby, std::string_view ownBuild)
{
	return lobby.buildId != ownBuild;
}

bool IsFull(const lobby_api::LobbySummary& lobby)
{
	// The computer slots and the closed slots take no player
	return lobby.freeSlots.has_value() ? *lobby.freeSlots <= 0 : lobby.gods >= lobby.godSlots;
}

bool IsInGame(const lobby_api::LobbySummary& lobby)
{
	return lobby.state == lobby_api::LobbyState::Assigned || lobby.state == lobby_api::LobbyState::InGame ||
	       lobby.state == lobby_api::LobbyState::Ended;
}

bool CanJoin(const lobby_api::LobbySummary& lobby, std::string_view ownBuild)
{
	return lobby.state == lobby_api::LobbyState::Open && !IsWrongVersion(lobby, ownBuild) && !IsFull(lobby);
}

bool DefaultOrderBefore(const lobby_api::LobbySummary& left, const lobby_api::LobbySummary& right)
{
	const auto open = [](const auto& lobby) { return lobby.state == lobby_api::LobbyState::Open && !IsFull(lobby); };
	if (open(left) != open(right))
	{
		return open(left);
	}
	if (left.gods != right.gods)
	{
		return left.gods > right.gods;
	}
	return left.created.value_or(0) > right.created.value_or(0);
}

std::vector<lobby_api::LobbySummary> VisibleLobbies(std::span<const lobby_api::LobbySummary> lobbies, const ListFilter& filter,
                                                    std::string_view ownBuild)
{
	std::vector<lobby_api::LobbySummary> visible;
	for (const auto& lobby : lobbies)
	{
		const bool matches = ContainsFolded(lobby.name, filter.search) || ContainsFolded(lobby.host.name, filter.search) ||
		                     ContainsFolded(lobby.map.name, filter.search);
		if (!matches || (!filter.mapFile.empty() && lobby.map.file != filter.mapFile) || (!filter.showFull && IsFull(lobby)) ||
		    (!filter.showLocked && lobby.locked) || (!filter.showInGame && IsInGame(lobby)) ||
		    (!filter.showOtherVersions && IsWrongVersion(lobby, ownBuild)))
		{
			continue;
		}
		visible.push_back(lobby);
	}
	std::ranges::stable_sort(visible, DefaultOrderBefore);
	return visible;
}

void LobbyListModel::Replace(lobby_api::LobbyList list)
{
	_lobbies = std::move(list.lobbies);
	if (list.pool.has_value())
	{
		_pool = list.pool;
	}
	++_revision;
}

bool LobbyListModel::Apply(const lobby_api::PushEvent& event)
{
	if (event.type == events::k_PoolChanged)
	{
		const auto pool = lobby_api::ParsePoolState(event.data);
		if (!pool.has_value())
		{
			return false;
		}
		_pool = pool;
		++_revision;
		return true;
	}
	const auto change = lobby_api::ParseLobbyListChange(event.type, event.data);
	if (!change.has_value())
	{
		return false;
	}
	const auto found = std::ranges::find(_lobbies, change->id, &lobby_api::LobbySummary::id);
	if (!change->summary.has_value())
	{
		if (found == _lobbies.end())
		{
			return false;
		}
		_lobbies.erase(found);
	}
	else if (found != _lobbies.end())
	{
		*found = *change->summary;
	}
	else
	{
		_lobbies.push_back(*change->summary);
	}
	++_revision;
	return true;
}

void LobbyListModel::Clear()
{
	_lobbies.clear();
	_pool.reset();
	++_revision;
}

const lobby_api::LobbySummary* LobbyListModel::Find(std::string_view id) const
{
	const auto found = std::ranges::find(_lobbies, id, &lobby_api::LobbySummary::id);
	return found != _lobbies.end() ? &*found : nullptr;
}

} // namespace openblack::online::lobby
