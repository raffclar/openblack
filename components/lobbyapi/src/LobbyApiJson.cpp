/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LobbyApiJson.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>

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

bool ReadAccountRef(const Json& object, AccountRef& out)
{
	return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name);
}

bool ReadClanSummary(const Json& object, ClanSummary& out)
{
	return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name) &&
	       ReadNumber(object, "points", out.points, false);
}

bool ReadLocation(const Json& object, Location& out)
{
	return ReadString(object, "country", out.country) && ReadString(object, "area", out.area, false) &&
	       ReadString(object, "city", out.city);
}

bool ReadServerEntry(const Json& object, ServerEntry& out)
{
	return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name) &&
	       ReadInteger(object, "players", out.players) && ReadInteger(object, "max_players", out.maxPlayers) &&
	       ReadBool(object, "playing", out.playing, false) && ReadString(object, "version", out.version, false) &&
	       ReadBool(object, "password_needed", out.passwordNeeded, false) && ReadBool(object, "locked", out.locked, false) &&
	       ReadString(object, "land", out.land, false);
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
	    {"client", request.client == ClientKind::Browser ? "browser" : "desktop"},
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
	if (reply.creature.has_value())
	{
		object["creature"] = ClanCreatureJson(*reply.creature);
	}
	if (reply.make.has_value())
	{
		object["new"] = Json {{"type", reply.make->type}, {"name", reply.make->name}};
	}
	return Dump(object);
}

std::string PushFrame(PushOp op, std::string_view channel, int64_t lastSeq)
{
	switch (op)
	{
	case PushOp::Resume:
		return Dump(Json {{"op", "resume"}, {"last_seq", lastSeq}});
	case PushOp::Subscribe:
		return Dump(Json {{"op", "subscribe"}, {"channel", std::string(channel)}});
	case PushOp::Unsubscribe:
		return Dump(Json {{"op", "unsubscribe"}, {"channel", std::string(channel)}});
	case PushOp::Pong:
		return Dump(Json {{"op", "pong"}});
	}
	return {};
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
		       ReadInteger(object, "expires_in", out.expiresIn) && out.expiresIn > 0;
	});
}

std::optional<Me> ParseMe(std::string_view text)
{
	return ParseWith<Me>(text, [](const Json& object, Me& out) {
		if (!ReadInteger(object, "id", out.id) || !ReadString(object, "name", out.name) ||
		    !ReadString(object, "email", out.email, false) || !ReadBool(object, "email_verified", out.emailVerified, false) ||
		    !ReadNumber(object, "points", out.points, false) || !ReadArray(object, "clans", out.clans, ReadClanSummary, false))
		{
			return false;
		}
		const auto* location = Member(object, "location");
		if (location != nullptr && !location->is_null())
		{
			Location place;
			if (!location->is_object() || !ReadLocation(*location, place))
			{
				return false;
			}
			out.location = std::move(place);
		}
		return true;
	});
}

std::optional<ServerList> ParseServerList(std::string_view text)
{
	return ParseWith<ServerList>(
	    text, [](const Json& object, ServerList& out) { return ReadArray(object, "servers", out.servers, ReadServerEntry); });
}

std::optional<JoinTicket> ParseJoinTicket(std::string_view text)
{
	return ParseWith<JoinTicket>(text, [](const Json& object, JoinTicket& out) {
		return ReadString(object, "url", out.url) && ReadString(object, "ticket", out.ticket) && !out.ticket.empty() &&
		       ReadInteger(object, "expires_in", out.expiresIn, false);
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
		return ReadInteger(object, "id", out.id) && ReadString(object, "name", out.name) &&
		       ReadString(object, "description", out.description, false) && ReadString(object, "leader", out.leader, false) &&
		       ReadString(object, "url", out.url, false) && ReadNumber(object, "points", out.points, false) &&
		       ReadInteger(object, "members", out.members, false);
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
		const auto* creature = Member(object, "creature");
		if (creature != nullptr && !creature->is_null())
		{
			ClanCreature value;
			if (!creature->is_object() || !ReadClanCreature(*creature, value))
			{
				return false;
			}
			out.creature = std::move(value);
		}
		const auto* make = Member(object, "new");
		if (make != nullptr && !make->is_null())
		{
			NewClanCreature value;
			if (!make->is_object() || !ReadInteger(*make, "type", value.type) || !ReadString(*make, "name", value.name))
			{
				return false;
			}
			out.make = std::move(value);
		}
		// The answer has one of the two
		return out.creature.has_value() != out.make.has_value();
	});
}

std::optional<Problem> ParseProblem(std::string_view text)
{
	return ParseWith<Problem>(text, [](const Json& object, Problem& out) {
		return ReadString(object, "type", out.type, false) && ReadString(object, "title", out.title, false) &&
		       ReadInteger(object, "status", out.status, false) && ReadString(object, "detail", out.detail, false);
	});
}

std::optional<PushEvent> ParsePushEvent(std::string_view text)
{
	return ParseWith<PushEvent>(text, [](const Json& object, PushEvent& out) {
		if (!ReadInteger(object, "seq", out.seq, false) || !ReadString(object, "type", out.type) || out.type.empty())
		{
			return false;
		}
		const auto* data = Member(object, "data");
		out.data = data != nullptr ? Dump(*data) : std::string("null");
		return true;
	});
}

std::optional<ServerChange> ParseServerChange(std::string_view text)
{
	return ParseWith<ServerChange>(text, [](const Json& object, ServerChange& out) {
		if (!ReadBool(object, "removed", out.removed, false))
		{
			return false;
		}
		if (out.removed)
		{
			return ReadInteger(object, "id", out.server.id);
		}
		return ReadServerEntry(object, out.server);
	});
}

std::optional<PointsChange> ParsePointsChange(std::string_view text)
{
	return ParseWith<PointsChange>(
	    text, [](const Json& object, PointsChange& out) { return ReadNumber(object, "points", out.points); });
}

std::optional<Ban> ParseBan(std::string_view text)
{
	return ParseWith<Ban>(text, [](const Json& object, Ban& out) { return ReadString(object, "reason", out.reason, false); });
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
