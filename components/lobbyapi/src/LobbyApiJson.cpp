/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyApiJson.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <system_error>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace openblack::lobby_api
{
namespace
{

using Json = nlohmann::json;

std::string Dump(const Json& value)
{
	// Text that is not valid UTF-8 is replaced, because the library must not throw
	return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::optional<Json> ParseObject(std::string_view text)
{
	auto value = Json::parse(text.begin(), text.end(), nullptr, false);
	if (value.is_discarded() || !value.is_object())
	{
		return std::nullopt;
	}
	return value;
}

const Json* Member(const Json& object, std::string_view key)
{
	const auto found = object.find(key);
	return found != object.end() ? &*found : nullptr;
}

/// Reads a string member. A missing member or null is an error only when the member is required.
bool ReadString(const Json& object, std::string_view key, std::string& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_string())
	{
		return false;
	}
	out = member->get_ref<const std::string&>();
	return true;
}

template <typename Integer>
bool ReadInteger(const Json& object, std::string_view key, Integer& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (member->is_number_unsigned())
	{
		const auto value = member->get<uint64_t>();
		if (value > static_cast<uint64_t>(std::numeric_limits<Integer>::max()))
		{
			return false;
		}
		out = static_cast<Integer>(value);
		return true;
	}
	if (!member->is_number_integer())
	{
		return false;
	}
	const auto value = member->get<int64_t>();
	if (value < static_cast<int64_t>(std::numeric_limits<Integer>::min()) ||
	    value > static_cast<int64_t>(std::numeric_limits<Integer>::max()))
	{
		return false;
	}
	out = static_cast<Integer>(value);
	return true;
}

bool ReadNumber(const Json& object, std::string_view key, double& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_number())
	{
		return false;
	}
	out = member->get<double>();
	return true;
}

bool ReadBool(const Json& object, std::string_view key, bool& out, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_boolean())
	{
		return false;
	}
	out = member->get<bool>();
	return true;
}

/// Reads an array of objects, each with a reader. A missing array is empty when it is not required.
template <typename Item, typename Reader>
bool ReadArray(const Json& object, std::string_view key, std::vector<Item>& out, Reader&& read, bool required = true)
{
	const auto* member = Member(object, key);
	if (member == nullptr || member->is_null())
	{
		return !required;
	}
	if (!member->is_array())
	{
		return false;
	}
	out.clear();
	out.reserve(member->size());
	for (const auto& element : *member)
	{
		if (!element.is_object())
		{
			return false;
		}
		Item item;
		if (!read(element, item))
		{
			return false;
		}
		out.push_back(std::move(item));
	}
	return true;
}

/// Reads an ISO 8601 time member as milliseconds since 1970. Null is 0 when the member is not required.
bool ReadTime(const Json& object, std::string_view key, int64_t& out, bool required = true)
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
	const auto time = ParseTime(text);
	if (!time.has_value())
	{
		return false;
	}
	out = *time;
	return true;
}

bool ReadAccountRef(const Json& object, AccountRef& out)
{
	return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name);
}

bool ReadServerBan(const Json& object, ServerBan& out)
{
	const auto* account = Member(object, "account");
	return account != nullptr && account->is_object() && ReadAccountRef(*account, out.account) &&
	       ReadString(object, "reason", out.reason, false);
}

bool ReadClanSummary(const Json& object, ClanSummary& out)
{
	return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name) &&
	       ReadNumber(object, "points", out.points, false);
}

bool ReadClanRef(const Json& object, ClanRef& out)
{
	return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name) && ReadString(object, "role", out.role);
}

bool ReadRoomPlayer(const Json& object, RoomPlayer& out)
{
	const auto* account = Member(object, "account");
	if (account == nullptr || !account->is_object() || !ReadAccountRef(*account, out.account) ||
	    !ReadString(object, "role", out.role))
	{
		return false;
	}
	int64_t clan = 0;
	const auto* member = Member(object, "clan");
	if (member != nullptr && !member->is_null())
	{
		if (!ReadInteger(object, "clan", clan))
		{
			return false;
		}
		out.clan = clan;
	}
	return true;
}

bool ReadServerEntry(const Json& object, ServerEntry& out)
{
	return ReadString(object, "id", out.id) && !out.id.empty() && ReadString(object, "hostname", out.hostname) &&
	       ReadString(object, "land", out.land) && ReadString(object, "gamemode", out.gamemode) &&
	       ReadBool(object, "locked", out.locked) && ReadBool(object, "password", out.password) &&
	       ReadInteger(object, "numplayers", out.numplayers) && ReadInteger(object, "maxplayers", out.maxplayers) &&
	       ReadString(object, "gamever", out.gamever) && ReadInteger(object, "infoversion", out.infoversion) &&
	       ReadInteger(object, "creatureversion", out.creatureversion) && ReadInteger(object, "oosversion", out.oosversion) &&
	       ReadArray(object, "players", out.players, ReadRoomPlayer) && ReadString(object, "url", out.url) &&
	       ReadTime(object, "updated", out.updated, false) && ReadString(object, "map", out.map, false);
}

/// A text of hex digits as a number. False when the text is empty, longer than 16 digits or has another character.
bool ReadHex64(const Json& object, std::string_view key, uint64_t& out)
{
	std::string text;
	if (!ReadString(object, key, text) || text.empty() || text.size() > 16)
	{
		return false;
	}
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), out, 16);
	return error == std::errc() && end == text.data() + text.size();
}

bool IsSha256Text(std::string_view text)
{
	return text.size() == 64 &&
	       std::ranges::all_of(text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool ReadOnlineMap(const Json& object, OnlineMap& out)
{
	return ReadInteger(object, "number", out.number) && ReadString(object, "name", out.name) &&
	       ReadString(object, "file", out.file) && !out.file.empty() && ReadInteger(object, "players", out.players) &&
	       ReadString(object, "sha256", out.sha256) && IsSha256Text(out.sha256) &&
	       ReadHex64(object, "checksum", out.checksum) && ReadInteger(object, "size", out.size) &&
	       ReadString(object, "url", out.url) && !out.url.empty();
}

bool ReadRankingEntry(const Json& object, RankingEntry& out)
{
	return ReadInteger(object, "position", out.position) && ReadInteger(object, "id", out.id) &&
	       ReadString(object, "name", out.name) && ReadNumber(object, "points", out.points);
}

bool ReadBase64(const Json& object, std::string_view key, std::vector<uint8_t>& out)
{
	std::string text;
	if (!ReadString(object, key, text))
	{
		return false;
	}
	auto bytes = Base64Decode(text);
	if (!bytes.has_value())
	{
		return false;
	}
	out = std::move(*bytes);
	return true;
}

bool ReadClanCreature(const Json& object, ClanCreature& out)
{
	double alignment = 0.0;
	if (!ReadNumber(object, "alignment", alignment) || !ReadBase64(object, "mind", out.mind) ||
	    !ReadBase64(object, "physique", out.physique))
	{
		return false;
	}
	out.alignment = static_cast<float>(alignment);
	return true;
}

Json ClanCreatureJson(const ClanCreature& creature)
{
	// A float goes to the text with all of its bits, so that the same float comes back
	return Json {
	    {"alignment", static_cast<double>(creature.alignment)},
	    {"mind", Base64Encode(creature.mind)},
	    {"physique", Base64Encode(creature.physique)},
	};
}

/// Reads a JSON object with a reader of its members
template <typename Shape, typename Reader>
std::optional<Shape> ParseWith(std::string_view text, Reader&& read)
{
	const auto object = ParseObject(text);
	if (!object.has_value())
	{
		return std::nullopt;
	}
	Shape shape;
	if (!read(*object, shape))
	{
		return std::nullopt;
	}
	return shape;
}

constexpr std::string_view k_Base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr std::array<int8_t, 256> MakeBase64Values()
{
	std::array<int8_t, 256> values {};
	values.fill(-1);
	for (size_t i = 0; i < k_Base64Alphabet.size(); ++i)
	{
		values.at(static_cast<uint8_t>(k_Base64Alphabet[i])) = static_cast<int8_t>(i);
	}
	return values;
}

constexpr auto k_Base64Values = MakeBase64Values();

/// A name or a file name in the header of a part, with the characters that would end the quoted text changed
std::string QuotedHeaderValue(std::string_view value)
{
	std::string out(value);
	std::ranges::replace_if(out, [](char c) { return c == '"' || c == '\r' || c == '\n'; }, '_');
	return out;
}

bool Contains(std::span<const uint8_t> bytes, std::string_view text)
{
	return !std::ranges::search(bytes, text, {}, {}, [](char c) { return static_cast<uint8_t>(c); }).empty();
}

} // namespace

bool IsValidAccountName(std::string_view name) noexcept
{
	if (name.size() < k_FewestNameCharacters || name.size() > k_MostNameCharacters)
	{
		return false;
	}
	return std::ranges::all_of(
	    name, [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
}

std::string ToJson(const LoginRequest& request)
{
	return Dump(Json {
	    {"name", request.name},
	    {"password", request.password},
	    {"cookie", request.cookie},
	});
}

std::string ToJson(const RefreshRequest& request)
{
	auto object = Json::object();
	if (request.refreshToken.has_value())
	{
		object["refresh_token"] = *request.refreshToken;
	}
	return Dump(object);
}

std::string ToJson(const ClanCreature& creature)
{
	return Dump(ClanCreatureJson(creature));
}

std::string ToJson(const ClanCreatureReply& reply)
{
	auto object = Json::object();
	switch (reply.kind)
	{
	case ClanCreatureKind::None:
		object["kind"] = "none";
		break;
	case ClanCreatureKind::Stored:
		if (reply.creature.has_value())
		{
			object = ClanCreatureJson(*reply.creature);
		}
		object["kind"] = "stored";
		break;
	case ClanCreatureKind::Create:
		object["kind"] = "create";
		if (reply.make.has_value())
		{
			object["creature_type"] = reply.make->type;
			object["name"] = reply.make->name;
		}
		break;
	}
	return Dump(object);
}

std::string SubscribeFrame(bool subscribe, std::string_view channel)
{
	return Dump(Json {{"type", subscribe ? "subscribe" : "unsubscribe"}, {"channel", std::string(channel)}});
}

std::string ResumeFrame(std::string_view epoch, int64_t seq, std::span<const std::string> channels)
{
	auto list = Json::array();
	for (const auto& channel : channels)
	{
		list.push_back(channel);
	}
	return Dump(Json {{"type", "resume"}, {"epoch", std::string(epoch)}, {"seq", seq}, {"channels", std::move(list)}});
}

std::string RoomJoinFrame(std::string_view ticket, std::string_view gamePassword, std::string_view lanName)
{
	auto object = Json {{"type", "room.join"}};
	if (!ticket.empty())
	{
		object["ticket"] = std::string(ticket);
	}
	if (!gamePassword.empty())
	{
		object["game_password"] = std::string(gamePassword);
	}
	if (!lanName.empty())
	{
		object["name"] = std::string(lanName);
	}
	return Dump(object);
}

std::string RoomReadyFrame(bool ready)
{
	return Dump(Json {{"type", "room.ready"}, {"ready", ready}});
}

std::optional<Session> ParseSession(std::string_view text)
{
	return ParseWith<Session>(text, [](const Json& object, Session& out) {
		const auto* account = Member(object, "account");
		if (account == nullptr || !account->is_object() || !ReadAccountRef(*account, out.account))
		{
			return false;
		}
		std::string refresh;
		const auto* refreshMember = Member(object, "refresh_token");
		if (refreshMember != nullptr && !refreshMember->is_null())
		{
			if (!ReadString(object, "refresh_token", refresh) || refresh.empty())
			{
				return false;
			}
			out.refreshToken = std::move(refresh);
		}
		return ReadString(object, "access_token", out.accessToken) && !out.accessToken.empty() &&
		       ReadTime(object, "access_token_expires", out.accessTokenExpires) &&
		       ReadTime(object, "refresh_token_expires", out.refreshTokenExpires, false) &&
		       ReadBool(object, "email_required", out.emailRequired, false);
	});
}

std::optional<Me> ParseMe(std::string_view text)
{
	return ParseWith<Me>(text, [](const Json& object, Me& out) {
		return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name) &&
		       ReadString(object, "email", out.email, false) && ReadBool(object, "email_verified", out.emailVerified, false) &&
		       ReadNumber(object, "points", out.points, false) && ReadInteger(object, "games_ranked", out.gamesRanked, false) &&
		       ReadString(object, "country", out.country, false) && ReadString(object, "area", out.area, false) &&
		       ReadString(object, "city", out.city, false) && ReadArray(object, "clans", out.clans, ReadClanRef, false) &&
		       ReadTime(object, "date_joined", out.dateJoined, false);
	});
}

std::optional<ServerList> ParseServerList(std::string_view text)
{
	return ParseWith<ServerList>(
	    text, [](const Json& object, ServerList& out) { return ReadArray(object, "servers", out.servers, ReadServerEntry); });
}

std::optional<MapList> ParseMapList(std::string_view text)
{
	return ParseWith<MapList>(
	    text, [](const Json& object, MapList& out) { return ReadArray(object, "maps", out.maps, ReadOnlineMap); });
}

std::optional<ServerBanList> ParseServerBanList(std::string_view text)
{
	return ParseWith<ServerBanList>(
	    text, [](const Json& object, ServerBanList& out) { return ReadArray(object, "bans", out.bans, ReadServerBan); });
}

std::optional<JoinTicket> ParseJoinTicket(std::string_view text)
{
	return ParseWith<JoinTicket>(text, [](const Json& object, JoinTicket& out) {
		return ReadString(object, "url", out.url) && ReadString(object, "ticket", out.ticket) && !out.ticket.empty() &&
		       ReadTime(object, "expires", out.expires) && ReadString(object, "role", out.role, false);
	});
}

std::optional<ClanList> ParseClanList(std::string_view text)
{
	return ParseWith<ClanList>(
	    text, [](const Json& object, ClanList& out) { return ReadArray(object, "clans", out.clans, ReadClanSummary); });
}

std::optional<ClanDetails> ParseClanDetails(std::string_view text)
{
	return ParseWith<ClanDetails>(text, [](const Json& object, ClanDetails& out) {
		if (!ReadInteger(object, "id", out.id) || !ReadString(object, "name", out.name) ||
		    !ReadNumber(object, "points", out.points, false))
		{
			return false;
		}
		// The contract of the lobby names the description "motto" and the web site "website". The leader is an account
		// or null, and the members are a list. The older plan gave these as plain values.
		if (!ReadString(object, "motto", out.description, false) ||
		    !ReadString(object, "description", out.description, false) || !ReadString(object, "website", out.url, false) ||
		    !ReadString(object, "url", out.url, false))
		{
			return false;
		}
		if (const auto* leader = Member(object, "leader"); leader != nullptr && leader->is_object())
		{
			AccountRef account;
			if (!ReadAccountRef(*leader, account))
			{
				return false;
			}
			out.leader = std::move(account.name);
		}
		else if (!ReadString(object, "leader", out.leader, false))
		{
			return false;
		}
		if (const auto* members = Member(object, "members"); members != nullptr && members->is_array())
		{
			out.members = static_cast<int32_t>(members->size());
		}
		else if (!ReadInteger(object, "members", out.members, false))
		{
			return false;
		}
		return true;
	});
}

std::string ToJson(const ChatLineRequest& request)
{
	return Dump(Json {{"text", request.text}});
}

std::string ToJson(const UseClanRequest& request)
{
	auto object = Json::object();
	object["clan"] = request.clan.has_value() ? Json(*request.clan) : Json(nullptr);
	return Dump(object);
}

std::string ToJson(const TicketRequest& request)
{
	auto object = Json::object();
	object["role"] = request.role;
	return Dump(object);
}

std::string ToJson(const ServerStatus& status)
{
	auto players = Json::array();
	for (const auto& player : status.players)
	{
		players.push_back(Json {
		    {"account", player.account},
		    {"role", player.role},
		    {"clan", player.clan.has_value() ? Json(*player.clan) : Json(nullptr)},
		});
	}
	return Dump(Json {
	    {"hostname", status.hostname},
	    {"land", status.land},
	    {"gamemode", status.gamemode},
	    {"locked", status.locked},
	    {"password", status.password},
	    {"maxplayers", status.maxplayers},
	    {"gamever", status.gamever},
	    {"players", players},
	});
}

std::optional<AccountNameCheck> ParseAccountNameCheck(std::string_view text)
{
	return ParseWith<AccountNameCheck>(text, [](const Json& object, AccountNameCheck& out) {
		return ReadString(object, "name", out.name) && ReadBool(object, "valid", out.valid) &&
		       ReadBool(object, "available", out.available);
	});
}

std::optional<UsedClan> ParseUsedClan(std::string_view text)
{
	return ParseWith<UsedClan>(text, [](const Json& object, UsedClan& out) {
		const auto* clan = Member(object, "clan");
		if (clan == nullptr)
		{
			return false;
		}
		if (clan->is_null())
		{
			return true;
		}
		ClanRef ref;
		if (!clan->is_object() || !ReadInteger(*clan, "id", ref.id) || !ReadString(*clan, "name", ref.name) ||
		    !ReadString(*clan, "role", ref.role, false))
		{
			return false;
		}
		out.clan = std::move(ref);
		return true;
	});
}

std::optional<ChatLine> ParseChatLine(std::string_view data)
{
	return ParseWith<ChatLine>(data, [](const Json& object, ChatLine& out) {
		const auto* from = Member(object, "from");
		return from != nullptr && from->is_object() && ReadAccountRef(*from, out.from) &&
		       ReadString(object, "channel", out.channel) && ReadString(object, "text", out.text);
	});
}

std::optional<RoomChange> ParseRoomChange(std::string_view data)
{
	return ParseWith<RoomChange>(data, [](const Json& object, RoomChange& out) {
		return ReadString(object, "server", out.server) && ReadString(object, "gamemode", out.gamemode, false) &&
		       ReadArray(object, "players", out.players, ReadRoomPlayer);
	});
}

std::optional<RoomClanChoice> ParseRoomClanChoice(std::string_view data)
{
	return ParseWith<RoomClanChoice>(data, [](const Json& object, RoomClanChoice& out) {
		const auto* account = Member(object, "account");
		if (account == nullptr || !account->is_object() || !ReadAccountRef(*account, out.account) ||
		    !ReadString(object, "server", out.server))
		{
			return false;
		}
		const auto* clan = Member(object, "clan");
		if (clan == nullptr || clan->is_null())
		{
			return true;
		}
		ClanRef ref;
		if (!clan->is_object() || !ReadInteger(*clan, "id", ref.id) || !ReadString(*clan, "name", ref.name) ||
		    !ReadString(*clan, "role", ref.role, false))
		{
			return false;
		}
		out.clan = std::move(ref);
		return true;
	});
}

std::optional<ServerBanned> ParseServerBanned(std::string_view data)
{
	return ParseWith<ServerBanned>(data, [](const Json& object, ServerBanned& out) {
		const auto* server = Member(object, "server");
		return server != nullptr && server->is_object() && ReadString(*server, "id", out.server) &&
		       ReadString(*server, "name", out.name, false) && ReadString(object, "reason", out.reason, false);
	});
}

std::optional<RankingPage> ParseRankingPage(std::string_view text)
{
	return ParseWith<RankingPage>(text, [](const Json& object, RankingPage& out) {
		return ReadInteger(object, "page", out.page) && ReadInteger(object, "pages", out.pages) &&
		       ReadArray(object, "entries", out.entries, ReadRankingEntry);
	});
}

std::optional<PlayerDetails> ParsePlayerDetails(std::string_view text)
{
	return ParseWith<PlayerDetails>(text, [](const Json& object, PlayerDetails& out) {
		return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name) &&
		       ReadNumber(object, "points", out.points, false) && ReadInteger(object, "games_ranked", out.gamesRanked, false) &&
		       ReadArray(object, "clans", out.clans, ReadClanSummary, false);
	});
}

std::optional<RealWeather> ParseRealWeather(std::string_view text)
{
	return ParseWith<RealWeather>(text, [](const Json& object, RealWeather& out) {
		return ReadInteger(object, "temperature", out.temperature) && ReadInteger(object, "visibility", out.visibility) &&
		       ReadInteger(object, "type", out.type) && ReadString(object, "city", out.city) &&
		       ReadString(object, "area", out.area, false) && ReadString(object, "country", out.country);
	});
}

std::optional<CreatureUploadReply> ParseCreatureUploadReply(std::string_view text)
{
	return ParseWith<CreatureUploadReply>(
	    text, [](const Json& object, CreatureUploadReply& out) { return ReadString(object, "url", out.url); });
}

std::optional<ClanCreature> ParseClanCreature(std::string_view text)
{
	return ParseWith<ClanCreature>(text, ReadClanCreature);
}

std::optional<ClanCreatureReply> ParseClanCreatureReply(std::string_view text)
{
	return ParseWith<ClanCreatureReply>(text, [](const Json& object, ClanCreatureReply& out) {
		std::string kind;
		if (!ReadString(object, "kind", kind))
		{
			return false;
		}
		// The members of the other kinds can have null. The reader looks only at the members of the kind.
		if (kind == "none")
		{
			out.kind = ClanCreatureKind::None;
			return true;
		}
		if (kind == "stored")
		{
			ClanCreature value;
			if (!ReadClanCreature(object, value))
			{
				return false;
			}
			out.kind = ClanCreatureKind::Stored;
			out.creature = std::move(value);
			return true;
		}
		if (kind == "create")
		{
			NewClanCreature value;
			if (!ReadInteger(object, "creature_type", value.type) || !ReadString(object, "name", value.name))
			{
				return false;
			}
			out.kind = ClanCreatureKind::Create;
			out.make = std::move(value);
			return true;
		}
		return false;
	});
}

std::optional<Problem> ParseProblem(std::string_view text)
{
	return ParseWith<Problem>(text, [](const Json& object, Problem& out) {
		return ReadString(object, "type", out.type, false) && ReadString(object, "title", out.title, false) &&
		       ReadInteger(object, "status", out.status, false) && ReadString(object, "detail", out.detail, false) &&
		       ReadString(object, "code", out.code, false);
	});
}

std::optional<PushEvent> ParsePushEvent(std::string_view text)
{
	return ParseWith<PushEvent>(text, [](const Json& object, PushEvent& out) {
		if (!ReadString(object, "type", out.type) || out.type.empty() || !ReadString(object, "time", out.time, false))
		{
			return false;
		}
		const auto* seq = Member(object, "seq");
		if (seq != nullptr && !seq->is_null())
		{
			int64_t number = 0;
			if (!ReadInteger(object, "seq", number))
			{
				return false;
			}
			out.seq = number;
		}
		const auto* data = Member(object, "data");
		out.data = data != nullptr ? Dump(*data) : std::string("null");
		return true;
	});
}

std::optional<ServerChange> ParseServerChange(std::string_view type, std::string_view data)
{
	if (type == events::k_ServersRemoved)
	{
		return ParseWith<ServerChange>(data, [](const Json& object, ServerChange& out) {
			out.removed = true;
			return ReadString(object, "id", out.server.id) && !out.server.id.empty() &&
			       ReadString(object, "reason", out.reason, false);
		});
	}
	if (type != events::k_ServersChanged)
	{
		return std::nullopt;
	}
	return ParseWith<ServerChange>(data, [](const Json& object, ServerChange& out) {
		const auto* server = Member(object, "server");
		return server != nullptr && server->is_object() && ReadServerEntry(*server, out.server);
	});
}

std::optional<PushPosition> ParsePushPosition(std::string_view data)
{
	return ParseWith<PushPosition>(data, [](const Json& object, PushPosition& out) {
		return ReadString(object, "epoch", out.epoch) && ReadInteger(object, "seq", out.seq);
	});
}

std::optional<PushError> ParsePushError(std::string_view data)
{
	return ParseWith<PushError>(data, [](const Json& object, PushError& out) {
		return ReadString(object, "code", out.code) && ReadString(object, "detail", out.detail, false) &&
		       ReadString(object, "channel", out.channel, false);
	});
}

std::optional<PointsChange> ParsePointsChange(std::string_view text)
{
	return ParseWith<PointsChange>(text, [](const Json& object, PointsChange& out) {
		return ReadNumber(object, "points", out.points) && ReadInteger(object, "games_ranked", out.gamesRanked, false) &&
		       ReadInteger(object, "position", out.position, false) && ReadNumber(object, "change", out.change, false);
	});
}

std::optional<Ban> ParseBan(std::string_view text)
{
	return ParseWith<Ban>(text, [](const Json& object, Ban& out) { return ReadString(object, "reason", out.reason, false); });
}

std::optional<int64_t> ParseTime(std::string_view text)
{
	// YYYY-MM-DDTHH:MM:SS, then a fraction of a second, then Z or an offset of +HH:MM or -HH:MM
	const auto digits = [&text](size_t start, size_t count) -> std::optional<int32_t> {
		if (start + count > text.size())
		{
			return std::nullopt;
		}
		int32_t value = 0;
		for (size_t i = start; i < start + count; ++i)
		{
			if (text[i] < '0' || text[i] > '9')
			{
				return std::nullopt;
			}
			value = (value * 10) + (text[i] - '0');
		}
		return value;
	};
	const auto at = [&text](size_t i, char c) { return i < text.size() && text[i] == c; };
	const auto year = digits(0, 4);
	const auto month = digits(5, 2);
	const auto day = digits(8, 2);
	const auto hour = digits(11, 2);
	const auto minute = digits(14, 2);
	const auto second = digits(17, 2);
	if (!year || !month || !day || !hour || !minute || !second || !at(4, '-') || !at(7, '-') ||
	    !(at(10, 'T') || at(10, 't') || at(10, ' ')) || !at(13, ':') || !at(16, ':') || *month < 1 || *month > 12 || *day < 1 ||
	    *day > 31 || *hour > 23 || *minute > 59 || *second > 60)
	{
		return std::nullopt;
	}
	size_t i = 19;
	int64_t milliseconds = 0;
	if (at(i, '.'))
	{
		++i;
		int64_t scale = 100;
		const auto first = i;
		while (i < text.size() && text[i] >= '0' && text[i] <= '9')
		{
			milliseconds += (text[i] - '0') * scale;
			scale /= 10;
			++i;
		}
		if (i == first)
		{
			return std::nullopt;
		}
	}
	int64_t offsetMinutes = 0;
	if (at(i, 'Z') || at(i, 'z'))
	{
		++i;
	}
	else if (at(i, '+') || at(i, '-'))
	{
		const auto sign = text[i] == '-' ? -1 : 1;
		const auto offsetHours = digits(i + 1, 2);
		const auto offsetMinute = digits(i + 4, 2);
		if (!offsetHours || !offsetMinute || !at(i + 3, ':'))
		{
			return std::nullopt;
		}
		offsetMinutes = sign * ((*offsetHours * 60) + *offsetMinute);
		i += 6;
	}
	else
	{
		return std::nullopt;
	}
	if (i != text.size())
	{
		return std::nullopt;
	}
	// The days from 1970-01-01 of a date of the civil calendar
	const int64_t y = *year - (*month <= 2 ? 1 : 0);
	const int64_t era = (y >= 0 ? y : y - 399) / 400;
	const int64_t yearOfEra = y - (era * 400);
	const int64_t monthFromMarch = (*month + 9) % 12;
	const int64_t dayOfYear = ((153 * monthFromMarch) + 2) / 5 + *day - 1;
	const int64_t dayOfEra = (yearOfEra * 365) + (yearOfEra / 4) - (yearOfEra / 100) + dayOfYear;
	const int64_t days = (era * 146097) + dayOfEra - 719468;
	const int64_t seconds = (days * 86400) + (*hour * 3600) + (*minute * 60) + *second - (offsetMinutes * 60);
	return (seconds * 1000) + milliseconds;
}

std::string DateText(const Date& date)
{
	return fmt::format("{:04}-{:02}-{:02}", date.year, date.month, date.day);
}

std::string Base64Encode(std::span<const uint8_t> bytes)
{
	std::string out;
	out.reserve((bytes.size() + 2) / 3 * 4);
	size_t i = 0;
	for (; i + 3 <= bytes.size(); i += 3)
	{
		const uint32_t group = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8) |
		                       static_cast<uint32_t>(bytes[i + 2]);
		out.push_back(k_Base64Alphabet[(group >> 18) & 0x3F]);
		out.push_back(k_Base64Alphabet[(group >> 12) & 0x3F]);
		out.push_back(k_Base64Alphabet[(group >> 6) & 0x3F]);
		out.push_back(k_Base64Alphabet[group & 0x3F]);
	}
	const auto rest = bytes.size() - i;
	if (rest == 1)
	{
		const uint32_t group = static_cast<uint32_t>(bytes[i]) << 16;
		out.push_back(k_Base64Alphabet[(group >> 18) & 0x3F]);
		out.push_back(k_Base64Alphabet[(group >> 12) & 0x3F]);
		out.append("==");
	}
	else if (rest == 2)
	{
		const uint32_t group = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8);
		out.push_back(k_Base64Alphabet[(group >> 18) & 0x3F]);
		out.push_back(k_Base64Alphabet[(group >> 12) & 0x3F]);
		out.push_back(k_Base64Alphabet[(group >> 6) & 0x3F]);
		out.push_back('=');
	}
	return out;
}

std::optional<std::vector<uint8_t>> Base64Decode(std::string_view text)
{
	if (text.size() % 4 != 0)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> out;
	out.reserve(text.size() / 4 * 3);
	for (size_t i = 0; i < text.size(); i += 4)
	{
		const bool last = i + 4 == text.size();
		const auto padding = last ? static_cast<size_t>(std::ranges::count(text.substr(i + 2, 2), '=')) : size_t {0};
		if (padding == 1 && text[i + 3] != '=')
		{
			return std::nullopt;
		}
		uint32_t group = 0;
		for (size_t j = 0; j < 4; ++j)
		{
			const auto c = static_cast<uint8_t>(text[i + j]);
			if (j >= 4 - padding)
			{
				group <<= 6;
				continue;
			}
			const auto value = k_Base64Values.at(c);
			if (value < 0)
			{
				return std::nullopt;
			}
			group = (group << 6) | static_cast<uint32_t>(value);
		}
		out.push_back(static_cast<uint8_t>(group >> 16));
		if (padding < 2)
		{
			out.push_back(static_cast<uint8_t>(group >> 8));
		}
		if (padding < 1)
		{
			out.push_back(static_cast<uint8_t>(group));
		}
	}
	return out;
}

std::string MultipartContentType(std::string_view boundary)
{
	return fmt::format("multipart/form-data; boundary={}", boundary);
}

std::vector<uint8_t> MultipartBody(std::string_view boundary, std::span<const FormPart> parts)
{
	std::vector<uint8_t> body;
	const auto append = [&body](std::string_view text) { body.insert(body.end(), text.begin(), text.end()); };
	for (const auto& part : parts)
	{
		append(fmt::format("--{}\r\nContent-Disposition: form-data; name=\"{}\"; filename=\"{}\"\r\nContent-Type: {}\r\n\r\n",
		                   boundary, QuotedHeaderValue(part.name), QuotedHeaderValue(part.fileName), part.contentType));
		body.insert(body.end(), part.bytes.begin(), part.bytes.end());
		append("\r\n");
	}
	append(fmt::format("--{}--\r\n", boundary));
	return body;
}

std::string MultipartBoundary(std::span<const FormPart> parts, uint64_t seed)
{
	for (uint64_t attempt = 0;; ++attempt)
	{
		// A mix of the seed and the attempt, so that each attempt gives other digits
		const auto mixed = (seed ^ (attempt * 0x9E3779B97F4A7C15ULL)) * 0xBF58476D1CE4E5B9ULL;
		auto boundary = fmt::format("openblack-{:016x}", mixed);
		if (std::ranges::none_of(parts, [&boundary](const FormPart& part) { return Contains(part.bytes, boundary); }))
		{
			return boundary;
		}
	}
}

} // namespace openblack::lobby_api
