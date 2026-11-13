/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbiesJson.h"

#include <algorithm>
#include <array>
#include <utility>

#include <nlohmann/json.hpp>

#include "JsonReaders.h"

namespace openblack::lobby_api
{
namespace
{

using json_read::Dump;
using json_read::Json;
using json_read::Member;
using json_read::ParseWith;
using json_read::ReadAccountRef;
using json_read::ReadArray;
using json_read::ReadBool;
using json_read::ReadInteger;
using json_read::ReadString;
using json_read::ReadTime;

template <typename Enum, size_t Count>
std::string_view NameIn(const std::array<std::pair<Enum, std::string_view>, Count>& names, Enum value)
{
	const auto found = std::ranges::find(names, value, &std::pair<Enum, std::string_view>::first);
	return found != names.end() ? found->second : std::string_view {};
}

template <typename Enum, size_t Count>
std::optional<Enum> ValueIn(const std::array<std::pair<Enum, std::string_view>, Count>& names, std::string_view name)
{
	const auto found = std::ranges::find(names, name, &std::pair<Enum, std::string_view>::second);
	return found != names.end() ? std::optional<Enum>(found->first) : std::nullopt;
}

constexpr std::array k_StateNames {
    std::pair {LobbyState::Open, std::string_view("open")},
    std::pair {LobbyState::Countdown, std::string_view("countdown")},
    std::pair {LobbyState::Queued, std::string_view("queued")},
    std::pair {LobbyState::Assigned, std::string_view("assigned")},
    std::pair {LobbyState::InGame, std::string_view("in_game")},
    std::pair {LobbyState::Ended, std::string_view("ended")},
    std::pair {LobbyState::Closed, std::string_view("closed")},
};
constexpr std::array k_SlotKindNames {
    std::pair {SlotKind::Open, std::string_view("open")},
    std::pair {SlotKind::Computer, std::string_view("computer")},
    std::pair {SlotKind::Closed, std::string_view("closed")},
};
constexpr std::array k_VisibilityNames {
    std::pair {LobbyVisibility::Public, std::string_view("public")},
    std::pair {LobbyVisibility::Private, std::string_view("private")},
};
constexpr std::array k_RoleNames {
    std::pair {MemberRole::God, std::string_view("god")},
    std::pair {MemberRole::Observer, std::string_view("observer")},
};

/// The letters of Crockford base32
constexpr std::string_view k_CodeAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

/// Reads a member that holds the name of a value of an enum
template <typename Enum, typename Of>
bool ReadName(const Json& object, std::string_view key, Enum& out, Of of, bool required = true)
{
	std::string text;
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!ReadString(object, key, text))
	{
		return false;
	}
	const auto value = of(text);
	if (!value.has_value())
	{
		return false;
	}
	out = *value;
	return true;
}

/// Reads an integer member that can be missing or null
template <typename Integer>
bool ReadOptionalInteger(const Json& object, std::string_view key, std::optional<Integer>& out)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		out.reset();
		return true;
	}
	Integer value {};
	if (!ReadInteger(object, key, value))
	{
		return false;
	}
	out = value;
	return true;
}

bool ReadOptionalTime(const Json& object, std::string_view key, std::optional<int64_t>& out)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		out.reset();
		return true;
	}
	int64_t value {0};
	if (!ReadTime(object, key, value))
	{
		return false;
	}
	out = value;
	return true;
}

bool ReadOptionalBool(const Json& object, std::string_view key, std::optional<bool>& out)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		out.reset();
		return true;
	}
	bool value {false};
	if (!ReadBool(object, key, value))
	{
		return false;
	}
	out = value;
	return true;
}

/// Reads an account object member that can be missing or null
bool ReadOptionalAccount(const Json& object, std::string_view key, std::optional<AccountRef>& out)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		out.reset();
		return true;
	}
	AccountRef account;
	if (!member->is_object() || !ReadAccountRef(*member, account))
	{
		return false;
	}
	out = std::move(account);
	return true;
}

bool ReadAccountMember(const Json& object, std::string_view key, AccountRef& out)
{
	const auto* member = Member(object, key);
	return member != nullptr && member->is_object() && ReadAccountRef(*member, out);
}

bool ReadMap(const Json& object, LobbyMap& out)
{
	return ReadString(object, "file", out.file) && ReadString(object, "name", out.name, false) &&
	       ReadString(object, "sha256", out.sha256, false) && ReadInteger(object, "size", out.size, false);
}

bool ReadMapMember(const Json& object, std::string_view key, LobbyMap& out)
{
	const auto* member = Member(object, key);
	return member != nullptr && member->is_object() && ReadMap(*member, out);
}

bool ReadSettings(const Json& object, LobbySettings& out)
{
	if (!ReadInteger(object, "time_limit_minutes", out.timeLimitMinutes, false) ||
	    !ReadBool(object, "observers_allowed", out.observersAllowed, false))
	{
		return false;
	}
	const auto* conditions = Member(object, "conditions");
	if (conditions == nullptr || conditions->is_null())
	{
		return true;
	}
	if (!conditions->is_array() || conditions->size() > k_ConditionCount)
	{
		return false;
	}
	out.conditions.clear();
	for (const auto& each : *conditions)
	{
		if (!each.is_number_integer())
		{
			return false;
		}
		out.conditions.push_back(each.get<int32_t>());
	}
	return true;
}

bool ReadSettingsMember(const Json& object, std::string_view key, LobbySettings& out)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return true;
	}
	return member->is_object() && ReadSettings(*member, out);
}

bool ReadSummary(const Json& object, LobbySummary& out)
{
	return ReadString(object, "id", out.id) && ReadString(object, "name", out.name) &&
	       ReadAccountMember(object, "host", out.host) && ReadMapMember(object, "map", out.map) &&
	       ReadInteger(object, "gods", out.gods) && ReadInteger(object, "god_slots", out.godSlots) &&
	       ReadOptionalInteger(object, "open_slots", out.openSlots) &&
	       ReadOptionalInteger(object, "free_slots", out.freeSlots) && ReadInteger(object, "observers", out.observers, false) &&
	       ReadBool(object, "locked", out.locked, false) && ReadName(object, "state", out.state, LobbyStateOf) &&
	       ReadOptionalInteger(object, "queue_position", out.queuePosition) &&
	       ReadOptionalTime(object, "started_at", out.startedAt) && ReadOptionalTime(object, "created", out.created) &&
	       ReadString(object, "build_id", out.buildId, false);
}

bool ReadPool(const Json& object, PoolState& out)
{
	return ReadInteger(object, "free", out.free) && ReadInteger(object, "total", out.total);
}

bool ReadMember(const Json& object, LobbyMember& out)
{
	return ReadAccountMember(object, "account", out.account) && ReadName(object, "role", out.role, MemberRoleOf, false) &&
	       ReadOptionalInteger(object, "slot", out.slot) && ReadBool(object, "ready", out.ready, false) &&
	       ReadOptionalTime(object, "joined_at", out.joinedAt) && ReadBool(object, "away", out.away, false) &&
	       ReadBool(object, "have_map", out.haveMap, false) && ReadBool(object, "host", out.host, false);
}

/// "slots": [{"index", "kind", "account"}], one entry for each god slot in the order of the slots
bool ReadSlots(const Json& object, std::vector<SlotKind>& out)
{
	const auto* slots = Member(object, "slots");
	if (slots == nullptr || slots->is_null())
	{
		return true;
	}
	if (!slots->is_array() || slots->size() > static_cast<size_t>(k_MostGodSlots))
	{
		return false;
	}
	out.assign(slots->size(), SlotKind::Open);
	for (const auto& each : *slots)
	{
		int32_t index {0};
		SlotKind kind {SlotKind::Open};
		if (!each.is_object() || !ReadInteger(each, "index", index) || !ReadName(each, "kind", kind, SlotKindOf) || index < 0 ||
		    static_cast<size_t>(index) >= out.size())
		{
			return false;
		}
		out[static_cast<size_t>(index)] = kind;
	}
	return true;
}

bool ReadLobby(const Json& object, Lobby& out)
{
	if (!ReadString(object, "id", out.id) || !ReadString(object, "code", out.code, false) ||
	    !ReadString(object, "name", out.name) || !ReadAccountMember(object, "host", out.host) ||
	    !ReadName(object, "visibility", out.visibility, VisibilityOf, false) ||
	    !ReadBool(object, "locked", out.locked, false) || !ReadMapMember(object, "map", out.map) ||
	    !ReadInteger(object, "god_slots", out.godSlots) || !ReadSettingsMember(object, "settings", out.settings) ||
	    !ReadString(object, "build_id", out.buildId, false) || !ReadName(object, "state", out.state, LobbyStateOf) ||
	    !ReadOptionalInteger(object, "queue_position", out.queuePosition) ||
	    !ReadOptionalTime(object, "countdown_at", out.countdownAt) || !ReadOptionalTime(object, "started_at", out.startedAt) ||
	    !ReadArray(object, "members", out.members, ReadMember) || !ReadSlots(object, out.slotKinds))
	{
		return false;
	}
	return out.godSlots >= 0 && out.godSlots <= k_MostGodSlots;
}

void WriteMapReport(Json& object, const std::optional<MapReport>& report)
{
	if (!report.has_value())
	{
		return;
	}
	object["have_map"] = report->haveMap;
	if (!report->sha256.empty())
	{
		object["map_sha256"] = report->sha256;
	}
}

void WriteBuildHash(Json& object, const std::string& hash)
{
	if (!hash.empty())
	{
		object["build_hash"] = hash;
	}
}

Json MapJson(const LobbyMap& map)
{
	auto object = Json {{"file", map.file}, {"sha256", map.sha256}};
	if (!map.name.empty())
	{
		object["name"] = map.name;
	}
	return object;
}

Json SlotKindsJson(const std::vector<SlotKind>& kinds)
{
	auto out = Json::array();
	for (const auto kind : kinds)
	{
		out.push_back(std::string(SlotKindName(kind)));
	}
	return out;
}

Json SettingsJson(const LobbySettings& settings)
{
	return Json {
	    {"time_limit_minutes", settings.timeLimitMinutes},
	    {"conditions", settings.conditions},
	    {"observers_allowed", settings.observersAllowed},
	};
}

} // namespace

std::string_view LobbyStateName(LobbyState state)
{
	return NameIn(k_StateNames, state);
}

std::optional<LobbyState> LobbyStateOf(std::string_view name)
{
	return ValueIn(k_StateNames, name);
}

std::string_view SlotKindName(SlotKind kind)
{
	return NameIn(k_SlotKindNames, kind);
}

std::optional<SlotKind> SlotKindOf(std::string_view name)
{
	return ValueIn(k_SlotKindNames, name);
}

std::string_view VisibilityName(LobbyVisibility visibility)
{
	return NameIn(k_VisibilityNames, visibility);
}

std::optional<LobbyVisibility> VisibilityOf(std::string_view name)
{
	return ValueIn(k_VisibilityNames, name);
}

std::string_view MemberRoleName(MemberRole role)
{
	return NameIn(k_RoleNames, role);
}

std::optional<MemberRole> MemberRoleOf(std::string_view name)
{
	return ValueIn(k_RoleNames, name);
}

std::string ToJson(const LobbyCreateRequest& request)
{
	auto object = Json {
	    {"name", request.name},
	    {"map", MapJson(request.map)},
	    {"god_slots", request.godSlots},
	    {"slot_kinds", SlotKindsJson(request.slotKinds)},
	    {"visibility", std::string(VisibilityName(request.visibility))},
	    {"settings", SettingsJson(request.settings)},
	    {"build_id", request.buildId},
	};
	WriteBuildHash(object, request.buildHash);
	if (request.password.has_value() && !request.password->empty())
	{
		object["password"] = *request.password;
	}
	return Dump(object);
}

std::string ToJson(const LobbyChangeRequest& request)
{
	auto object = Json::object();
	if (request.name.has_value())
	{
		object["name"] = *request.name;
	}
	if (request.map.has_value())
	{
		object["map"] = MapJson(*request.map);
	}
	if (request.slotKinds.has_value())
	{
		object["slot_kinds"] = SlotKindsJson(*request.slotKinds);
	}
	if (request.visibility.has_value())
	{
		object["visibility"] = std::string(VisibilityName(*request.visibility));
	}
	if (request.password.has_value())
	{
		object["password"] = *request.password;
	}
	if (request.settings.has_value())
	{
		object["settings"] = SettingsJson(*request.settings);
	}
	return Dump(object);
}

std::string ToJson(const LobbyJoinRequest& request)
{
	auto object = Json {{"build_id", request.buildId}, {"role", std::string(MemberRoleName(request.role))}};
	WriteBuildHash(object, request.buildHash);
	if (request.password.has_value() && !request.password->empty())
	{
		object["password"] = *request.password;
	}
	if (request.slot.has_value())
	{
		object["slot"] = *request.slot;
	}
	WriteMapReport(object, request.map);
	return Dump(object);
}

std::string ToJson(const MemberChangeRequest& request)
{
	auto object = Json::object();
	if (request.ready.has_value())
	{
		object["ready"] = *request.ready;
	}
	if (request.slot.has_value())
	{
		object["slot"] = *request.slot;
	}
	WriteMapReport(object, request.map);
	return Dump(object);
}

std::string ToJson(const QuickJoinRequest& request)
{
	auto object = Json {{"build_id", request.buildId}};
	WriteBuildHash(object, request.buildHash);
	return Dump(object);
}

std::optional<LobbyList> ParseLobbyList(std::string_view text)
{
	return ParseWith<LobbyList>(text, [](const Json& object, LobbyList& out) {
		if (!ReadArray(object, "lobbies", out.lobbies, ReadSummary))
		{
			return false;
		}
		if (const auto* pool = Member(object, "pool"); pool != nullptr && !pool->is_null())
		{
			PoolState state;
			if (!pool->is_object() || !ReadPool(*pool, state))
			{
				return false;
			}
			out.pool = state;
		}
		return true;
	});
}

std::optional<Lobby> ParseLobby(std::string_view text)
{
	return ParseWith<Lobby>(text, ReadLobby);
}

std::optional<LobbyMember> ParseLobbyMember(std::string_view text)
{
	return ParseWith<LobbyMember>(text, ReadMember);
}

std::optional<StartReply> ParseStartReply(std::string_view text)
{
	return ParseWith<StartReply>(text, [](const Json& object, StartReply& out) {
		return ReadName(object, "state", out.state, LobbyStateOf) && ReadOptionalTime(object, "at", out.at) &&
		       ReadOptionalInteger(object, "position", out.position);
	});
}

std::optional<LobbyCodeReply> ParseLobbyCodeReply(std::string_view text)
{
	return ParseWith<LobbyCodeReply>(
	    text, [](const Json& object, LobbyCodeReply& out) { return ReadString(object, "id", out.id) && !out.id.empty(); });
}

std::optional<LobbyTicket> ParseLobbyTicket(std::string_view text)
{
	return ParseWith<LobbyTicket>(text, [](const Json& object, LobbyTicket& out) {
		return ReadString(object, "url", out.ticket.url) && ReadString(object, "ticket", out.ticket.ticket) &&
		       !out.ticket.ticket.empty() && ReadTime(object, "expires", out.ticket.expires) &&
		       ReadString(object, "role", out.ticket.role, false) && ReadString(object, "game_id", out.gameId, false);
	});
}

std::optional<LobbyListChange> ParseLobbyListChange(std::string_view type, std::string_view data)
{
	const bool removed = type == lobby_events::k_LobbiesRemoved;
	if (!removed && type != lobby_events::k_LobbiesChanged)
	{
		return std::nullopt;
	}
	return ParseWith<LobbyListChange>(data, [removed](const Json& object, LobbyListChange& out) {
		if (removed)
		{
			return ReadString(object, "id", out.id) && !out.id.empty() && ReadString(object, "reason", out.reason, false);
		}
		const auto* summary = Member(object, "lobby");
		LobbySummary value;
		if (summary == nullptr || !summary->is_object() || !ReadSummary(*summary, value) || value.id.empty())
		{
			return false;
		}
		out.id = value.id;
		out.summary = std::move(value);
		return true;
	});
}

std::optional<PoolState> ParsePoolState(std::string_view data)
{
	return ParseWith<PoolState>(data, ReadPool);
}

std::optional<MemberEvent> ParseMemberEvent(std::string_view data)
{
	return ParseWith<MemberEvent>(data, [](const Json& object, MemberEvent& out) {
		if (!ReadString(object, "lobby", out.lobby, false) || !ReadAccountMember(object, "account", out.account) ||
		    !ReadOptionalAccount(object, "by", out.by) || !ReadOptionalInteger(object, "slot", out.slot) ||
		    !ReadOptionalBool(object, "ready", out.ready) || !ReadOptionalBool(object, "have_map", out.haveMap) ||
		    !ReadString(object, "reason", out.reason, false))
		{
			return false;
		}
		if (const auto* role = Member(object, "role"); role != nullptr && !role->is_null())
		{
			MemberRole value {MemberRole::God};
			if (!ReadName(object, "role", value, MemberRoleOf))
			{
				return false;
			}
			out.role = value;
		}
		return true;
	});
}

std::optional<LobbyChange> ParseLobbyChange(std::string_view data)
{
	return ParseWith<LobbyChange>(data, [](const Json& object, LobbyChange& out) {
		if (!ReadString(object, "lobby", out.lobby, false) || !ReadBool(object, "ready_cleared", out.readyCleared, false) ||
		    !ReadBool(object, "map_cleared", out.mapCleared, false))
		{
			return false;
		}
		const auto* fields = Member(object, "fields");
		if (fields == nullptr || fields->is_null())
		{
			return true;
		}
		if (!fields->is_array())
		{
			return false;
		}
		for (const auto& field : *fields)
		{
			if (!field.is_string())
			{
				return false;
			}
			out.fields.push_back(field.get<std::string>());
		}
		return true;
	});
}

std::optional<LobbyStateEvent> ParseLobbyStateEvent(std::string_view data)
{
	return ParseWith<LobbyStateEvent>(data, [](const Json& object, LobbyStateEvent& out) {
		return ReadString(object, "lobby", out.lobby, false) && ReadOptionalTime(object, "at", out.at) &&
		       ReadString(object, "why", out.why, false) && ReadOptionalAccount(object, "account", out.account) &&
		       ReadOptionalInteger(object, "position", out.position) && ReadString(object, "game_id", out.gameId, false) &&
		       ReadString(object, "url", out.url, false) && ReadString(object, "reason", out.reason, false);
	});
}

std::optional<LobbyKick> ParseLobbyKick(std::string_view data)
{
	return ParseWith<LobbyKick>(data, [](const Json& object, LobbyKick& out) {
		return ReadString(object, "lobby", out.lobby) && ReadOptionalAccount(object, "by", out.by) &&
		       ReadBool(object, "banned", out.banned, false);
	});
}

std::optional<std::string> NormalLobbyCode(std::string_view text)
{
	std::string code;
	for (const auto character : text)
	{
		if (character == '-' || character == ' ')
		{
			continue;
		}
		auto upper = (character >= 'a' && character <= 'z') ? static_cast<char>(character - 'a' + 'A') : character;
		// Crockford base32 reads O as 0, and I and L as 1
		if (upper == 'O')
		{
			upper = '0';
		}
		else if (upper == 'I' || upper == 'L')
		{
			upper = '1';
		}
		if (k_CodeAlphabet.find(upper) == std::string_view::npos)
		{
			return std::nullopt;
		}
		code.push_back(upper);
	}
	if (code.size() != k_LobbyCodeCharacters)
	{
		return std::nullopt;
	}
	return code;
}

std::string LobbyCodeText(std::string_view code)
{
	constexpr size_t k_Half = k_LobbyCodeCharacters / 2;
	if (code.size() != k_LobbyCodeCharacters)
	{
		return std::string(code);
	}
	return std::string(code.substr(0, k_Half)) + "-" + std::string(code.substr(k_Half));
}

} // namespace openblack::lobby_api
