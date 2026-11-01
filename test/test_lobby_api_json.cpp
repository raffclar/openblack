/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <LobbyApi.h>
#include <LobbyApiJson.h>
#include <gtest/gtest.h>

using namespace openblack::lobby_api;

namespace
{

/// A server as the lobby lists it
std::string ServerJson()
{
	return R"({"id":"eden","hostname":"Eden","land":"Test","gamemode":"openstaging","locked":true,"password":false,)"
	       R"("numplayers":3,"maxplayers":32,"gamever":"1.0","infoversion":1,"creatureversion":2,"oosversion":3,)"
	       R"("players":[{"account":{"id":4,"name":"abc"},"role":"god","clan":null},)"
	       R"({"account":{"id":5,"name":"def"},"role":"observer","clan":9}],)"
	       R"("url":"wss://bw.example/game/eden","updated":"2026-10-22T10:00:00.000Z"})";
}

std::string TextOf(const std::vector<uint8_t>& bytes)
{
	return {bytes.begin(), bytes.end()};
}

} // namespace

TEST(LobbyApi, AccountNamesAreThreeToThirtyTwoLettersAndDigits)
{
	EXPECT_TRUE(IsValidAccountName("abc"));
	EXPECT_TRUE(IsValidAccountName("Player42"));
	EXPECT_TRUE(IsValidAccountName(std::string(32, 'a')));
	EXPECT_FALSE(IsValidAccountName("ab"));
	EXPECT_FALSE(IsValidAccountName(std::string(33, 'a')));
	EXPECT_FALSE(IsValidAccountName("under_score"));
	EXPECT_FALSE(IsValidAccountName("dash-name"));
	EXPECT_FALSE(IsValidAccountName("with space"));
	EXPECT_FALSE(IsValidAccountName("caf\xC3\xA9"));
	EXPECT_FALSE(IsValidAccountName(""));
}

TEST(LobbyApi, LoginRequestAsksForTheCookieInTheBrowser)
{
	EXPECT_EQ(ToJson(LoginRequest {.name = "abc", .password = "pw", .cookie = false}),
	          R"({"cookie":false,"name":"abc","password":"pw"})");
	EXPECT_EQ(ToJson(LoginRequest {.name = "abc", .password = "pw", .cookie = true}),
	          R"({"cookie":true,"name":"abc","password":"pw"})");
}

TEST(LobbyApi, RefreshRequestOfTheBrowserHasNoToken)
{
	EXPECT_EQ(ToJson(RefreshRequest {.refreshToken = "r1"}), R"({"refresh_token":"r1"})");
	EXPECT_EQ(ToJson(RefreshRequest {}), "{}");
}

TEST(LobbyApi, SessionNeedsATokenATimeAndAnAccount)
{
	const auto desktop = ParseSession(R"({"token_type":"Bearer","access_token":"a",
	    "access_token_expires":"2026-10-22T10:15:00.000Z","refresh_token":"r",
	    "refresh_token_expires":"2026-11-21T10:00:00+00:00","account":{"id":7,"name":"abc"},"extra":1})");
	ASSERT_TRUE(desktop.has_value());
	EXPECT_EQ(desktop->accessToken, "a");
	EXPECT_EQ(desktop->accessTokenExpires, *ParseTime("2026-10-22T10:15:00Z"));
	EXPECT_EQ(desktop->refreshToken, "r");
	EXPECT_EQ(desktop->refreshTokenExpires, *ParseTime("2026-11-21T10:00:00Z"));
	EXPECT_EQ(desktop->account.id, 7);
	EXPECT_EQ(desktop->account.name, "abc");

	// The browser's refresh token is in the cookie
	const auto browser = ParseSession(R"({"access_token":"a","access_token_expires":"2026-10-22T10:15:00Z",
	    "refresh_token":null,"refresh_token_expires":"2026-11-21T10:00:00Z","account":{"id":7,"name":"abc"}})");
	ASSERT_TRUE(browser.has_value());
	EXPECT_FALSE(browser->refreshToken.has_value());

	EXPECT_FALSE(ParseSession(R"({"access_token_expires":"2026-10-22T10:15:00Z","account":{"id":7,"name":"abc"}})"));
	EXPECT_FALSE(
	    ParseSession(R"({"access_token":"","access_token_expires":"2026-10-22T10:15:00Z","account":{"id":7,"name":"abc"}})"));
	EXPECT_FALSE(ParseSession(R"({"access_token":"a","access_token_expires":900,"account":{"id":7,"name":"abc"}})"));
	EXPECT_FALSE(ParseSession(R"({"access_token":"a","access_token_expires":"soon","account":{"id":7,"name":"abc"}})"));
	EXPECT_FALSE(ParseSession(R"({"access_token":"a","access_token_expires":"2026-10-22T10:15:00Z"})"));
	EXPECT_FALSE(ParseSession("not json").has_value());
	EXPECT_FALSE(ParseSession("[]").has_value());
}

TEST(LobbyApi, TimesAreMillisecondsSince1970)
{
	EXPECT_EQ(ParseTime("1970-01-01T00:00:00Z"), 0);
	EXPECT_EQ(ParseTime("1970-01-01T00:00:01.5Z"), 1500);
	EXPECT_EQ(ParseTime("2001-01-01T12:00:00Z"), int64_t {978350400} * 1000);
	EXPECT_EQ(ParseTime("2001-01-01T13:00:00+01:00"), int64_t {978350400} * 1000);
	EXPECT_EQ(ParseTime("2001-01-01T11:30:00-00:30"), int64_t {978350400} * 1000);
	EXPECT_EQ(ParseTime("2024-02-29T00:00:00.123456Z"), int64_t {1709164800} * 1000 + 123);
	EXPECT_FALSE(ParseTime("2026-10-22T10:15:00").has_value());
	EXPECT_FALSE(ParseTime("2026-13-22T10:15:00Z").has_value());
	EXPECT_FALSE(ParseTime("2026-10-22 10:15Z").has_value());
	EXPECT_FALSE(ParseTime("2026-10-22T10:15:00Zx").has_value());
}

TEST(LobbyApi, MeHasTheClansAndThePlaceForTheWeather)
{
	const auto me = ParseMe(R"({"id":3,"name":"abc","email":"a@b.c","email_verified":true,"points":1012.5,
	    "games_ranked":4,"country":"gb","area":"","city":"Guildford","clans":[{"id":1,"name":"Gods","role":"leader"}],
	    "date_joined":"2026-10-01T09:00:00.000000+00:00"})");
	ASSERT_TRUE(me.has_value());
	EXPECT_EQ(me->id, 3);
	EXPECT_TRUE(me->emailVerified);
	EXPECT_DOUBLE_EQ(me->points, 1012.5);
	EXPECT_EQ(me->gamesRanked, 4);
	ASSERT_EQ(me->clans.size(), 1u);
	EXPECT_EQ(me->clans[0].name, "Gods");
	EXPECT_EQ(me->clans[0].role, "leader");
	EXPECT_EQ(me->country, "gb");
	EXPECT_EQ(me->city, "Guildford");
	EXPECT_EQ(me->dateJoined, *ParseTime("2026-10-01T09:00:00Z"));
	EXPECT_FALSE(ParseMe(R"({"id":3,"name":"abc","clans":[{"id":1,"name":"Gods"}]})").has_value());
}

TEST(LobbyApi, ServerListReadsEachServer)
{
	const auto list = ParseServerList(R"({"servers":[)" + ServerJson() + "]}");
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->servers.size(), 1u);
	const auto& server = list->servers[0];
	EXPECT_EQ(server.id, "eden");
	EXPECT_EQ(server.hostname, "Eden");
	EXPECT_EQ(server.gamemode, "openstaging");
	EXPECT_EQ(server.numplayers, 3);
	EXPECT_EQ(server.maxplayers, 32);
	EXPECT_TRUE(server.locked);
	EXPECT_FALSE(server.password);
	EXPECT_EQ(server.oosversion, 3);
	ASSERT_EQ(server.players.size(), 2u);
	EXPECT_EQ(server.players[0].account.name, "abc");
	EXPECT_FALSE(server.players[0].clan.has_value());
	EXPECT_EQ(server.players[1].role, "observer");
	EXPECT_EQ(server.players[1].clan, 9);
	EXPECT_EQ(server.url, "wss://bw.example/game/eden");
	EXPECT_EQ(server.updated, *ParseTime("2026-10-22T10:00:00Z"));
	EXPECT_FALSE(ParseServerList(R"({"servers":[{"id":"eden"}]})").has_value());
	EXPECT_FALSE(ParseServerList(R"({"servers":{}})").has_value());
}

TEST(LobbyApi, NumbersOutOfRangeAreRefused)
{
	std::string server = ServerJson();
	const auto big = server;
	server.replace(server.find(R"("numplayers":3)"), 14, R"("numplayers":4294967296)");
	EXPECT_FALSE(ParseServerList(R"({"servers":[)" + server + "]}").has_value());
	server = big;
	server.replace(server.find(R"("numplayers":3)"), 14, R"("numplayers":1.5)");
	EXPECT_FALSE(ParseServerList(R"({"servers":[)" + server + "]}").has_value());
}

TEST(LobbyApi, TicketClansRankingAndPlayer)
{
	const auto ticket =
	    ParseJoinTicket(R"({"url":"wss://bw.example/game/5","ticket":"v4.public.x","expires":"2026-10-22T10:01:00Z"})");
	ASSERT_TRUE(ticket.has_value());
	EXPECT_EQ(ticket->url, "wss://bw.example/game/5");
	EXPECT_EQ(ticket->expires, *ParseTime("2026-10-22T10:01:00Z"));
	EXPECT_FALSE(ParseJoinTicket(R"({"url":"wss://x","ticket":"","expires":"2026-10-22T10:01:00Z"})").has_value());
	EXPECT_FALSE(ParseJoinTicket(R"({"url":"wss://x","ticket":"t"})").has_value());

	const auto clans = ParseClanList(R"({"clans":[{"id":1,"name":"A"},{"id":2,"name":"B","points":3}]})");
	ASSERT_TRUE(clans.has_value());
	EXPECT_EQ(clans->clans.size(), 2u);

	const auto clan = ParseClanDetails(
	    R"({"id":1,"name":"A","description":"motto","leader":"abc","url":"https://a.example","points":1500,"members":4})");
	ASSERT_TRUE(clan.has_value());
	EXPECT_EQ(clan->description, "motto");
	EXPECT_EQ(clan->members, 4);

	const auto ranking =
	    ParseRankingPage(R"({"page":2,"pages":9,"entries":[{"position":26,"id":4,"name":"abc","points":1100.5}]})");
	ASSERT_TRUE(ranking.has_value());
	EXPECT_EQ(ranking->page, 2);
	EXPECT_EQ(ranking->entries.at(0).position, 26);

	const auto player = ParsePlayerDetails(R"({"id":4,"name":"abc","points":1000,"games_ranked":12})");
	ASSERT_TRUE(player.has_value());
	EXPECT_EQ(player->gamesRanked, 12);
}

TEST(LobbyApi, PlayerDetailLinesKeepTheOrderOfTheLobby)
{
	const auto details =
	    ParsePlayerDetailLines(R"({"lines":[{"label":"Points","value":"12"},{"label":"Country","value":"gb"}]})");
	ASSERT_TRUE(details.has_value());
	ASSERT_EQ(details->lines.size(), 2u);
	EXPECT_EQ(details->lines[0].label, "Points");
	EXPECT_EQ(details->lines[1].value, "gb");
	EXPECT_FALSE(ParsePlayerDetailLines(R"({"lines":[{"label":"Points"}]})").has_value());
	EXPECT_FALSE(ParsePlayerDetailLines(R"({})").has_value());
}

TEST(LobbyApi, WeatherHasThePlace)
{
	const auto weather =
	    ParseRealWeather(R"({"temperature":-3,"visibility":40,"type":10,"city":"Oslo","area":null,"country":"no"})");
	ASSERT_TRUE(weather.has_value());
	EXPECT_EQ(weather->temperature, -3);
	EXPECT_EQ(weather->visibility, 40);
	EXPECT_EQ(weather->type, 10);
	EXPECT_EQ(weather->city, "Oslo");
	EXPECT_TRUE(weather->area.empty());
	EXPECT_FALSE(ParseRealWeather(R"({"temperature":-3,"visibility":40,"type":10,"country":"no"})").has_value());
	EXPECT_EQ(DateText(Date {.year = 2026, .month = 1, .day = 9}), "2026-01-09");
}

TEST(LobbyApi, ClanCreatureKeepsTheFilesAndTheBitsOfTheAlignment)
{
	const ClanCreature creature {
	    .alignment = -0.123456789f,
	    .mind = {0, 1, 2, 250, 255},
	    .physique = {9, 8},
	};
	const auto back = ParseClanCreature(ToJson(creature));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(std::memcmp(&back->alignment, &creature.alignment, sizeof(float)), 0);
	EXPECT_EQ(back->mind, creature.mind);
	EXPECT_EQ(back->physique, creature.physique);

	const auto reply =
	    ParseClanCreatureReply(ToJson(ClanCreatureReply {.kind = ClanCreatureKind::Stored, .creature = creature}));
	ASSERT_TRUE(reply.has_value());
	EXPECT_EQ(reply->kind, ClanCreatureKind::Stored);
	ASSERT_TRUE(reply->creature.has_value());
	EXPECT_EQ(reply->creature->mind, creature.mind);
	EXPECT_FALSE(reply->make.has_value());
}

TEST(LobbyApi, ClanCreatureReplyReadsTheKindOfTheLobby)
{
	// The lobby gives every member. Only the members of the kind have a value.
	const auto stored = ParseClanCreatureReply(
	    R"({"kind":"stored","alignment":0.5,"mind":"AAEC","physique":"","creature_type":null,"name":null,"updated":"2026-10-31T10:00:00Z"})");
	ASSERT_TRUE(stored.has_value());
	EXPECT_EQ(stored->kind, ClanCreatureKind::Stored);
	ASSERT_TRUE(stored->creature.has_value());
	EXPECT_EQ(stored->creature->alignment, 0.5f);
	EXPECT_EQ(stored->creature->mind, (std::vector<uint8_t> {0, 1, 2}));
	EXPECT_TRUE(stored->creature->physique.empty());

	const auto make = ParseClanCreatureReply(
	    R"({"kind":"create","alignment":null,"mind":null,"physique":null,"creature_type":3,"name":"Horace","updated":null})");
	ASSERT_TRUE(make.has_value());
	EXPECT_EQ(make->kind, ClanCreatureKind::Create);
	ASSERT_TRUE(make->make.has_value());
	EXPECT_EQ(make->make->type, 3);
	EXPECT_EQ(make->make->name, "Horace");
	EXPECT_FALSE(make->creature.has_value());

	const auto none = ParseClanCreatureReply(R"({"kind":"none","alignment":null,"mind":null,"physique":null})");
	ASSERT_TRUE(none.has_value());
	EXPECT_EQ(none->kind, ClanCreatureKind::None);
	EXPECT_FALSE(none->creature.has_value());
	EXPECT_FALSE(none->make.has_value());

	const auto made = ParseClanCreatureReply(
	    ToJson(ClanCreatureReply {.kind = ClanCreatureKind::Create, .make = NewClanCreature {.type = 7, .name = "Ox"}}));
	ASSERT_TRUE(made.has_value());
	EXPECT_EQ(made->make->type, 7);
	EXPECT_EQ(ParseClanCreatureReply(ToJson(ClanCreatureReply {}))->kind, ClanCreatureKind::None);

	EXPECT_FALSE(ParseClanCreatureReply("{}").has_value());
	EXPECT_FALSE(ParseClanCreatureReply(R"({"kind":"other"})").has_value());
	EXPECT_FALSE(ParseClanCreatureReply(R"({"kind":"stored","alignment":null,"mind":"AAEC","physique":""})").has_value());
	EXPECT_FALSE(ParseClanCreatureReply(R"({"kind":"create","creature_type":3})").has_value());
	EXPECT_FALSE(ParseClanCreatureReply(R"({"creature":{"alignment":0,"mind":"","physique":""}})").has_value());
	EXPECT_FALSE(ParseClanCreature(R"({"alignment":0,"mind":"%%%%","physique":""})").has_value());
}

TEST(LobbyApi, ProblemGivesTheDetail)
{
	const auto problem = ParseProblem(R"({"type":"about:blank","title":"Too Many Requests","status":429,
	    "detail":"Wait 5 seconds","instance":"/api/v1/sessions","code":"throttled"})");
	ASSERT_TRUE(problem.has_value());
	EXPECT_EQ(problem->status, 429);
	EXPECT_EQ(problem->detail, "Wait 5 seconds");
	EXPECT_EQ(problem->code, "throttled");
}

TEST(LobbyApi, PushEventsAndFrames)
{
	const auto event = ParsePushEvent(
	    R"({"seq":12,"type":"servers.removed","time":"2026-10-22T10:00:00.000Z","data":{"id":"eden","reason":"timeout"}})");
	ASSERT_TRUE(event.has_value());
	EXPECT_EQ(event->seq, 12);
	EXPECT_EQ(event->type, events::k_ServersRemoved);
	EXPECT_EQ(event->time, "2026-10-22T10:00:00.000Z");
	const auto removed = ParseServerChange(event->type, event->data);
	ASSERT_TRUE(removed.has_value());
	EXPECT_TRUE(removed->removed);
	EXPECT_EQ(removed->server.id, "eden");
	EXPECT_EQ(removed->reason, "timeout");

	const auto changed = ParseServerChange(events::k_ServersChanged, R"({"server":)" + ServerJson() + "}");
	ASSERT_TRUE(changed.has_value());
	EXPECT_FALSE(changed->removed);
	EXPECT_EQ(changed->server.hostname, "Eden");
	EXPECT_FALSE(ParseServerChange(events::k_ChatLine, "{}").has_value());

	// A control frame has no number
	const auto hello = ParsePushEvent(R"({"type":"hello","data":{"epoch":"ab12","seq":40,"account":{"id":7,"name":"abc"}}})");
	ASSERT_TRUE(hello.has_value());
	EXPECT_FALSE(hello->seq.has_value());
	const auto position = ParsePushPosition(hello->data);
	ASSERT_TRUE(position.has_value());
	EXPECT_EQ(position->epoch, "ab12");
	EXPECT_EQ(position->seq, 40);
	const auto error = ParsePushError(R"({"code":"no_access","detail":"Join the server's room first.","channel":"server-x"})");
	ASSERT_TRUE(error.has_value());
	EXPECT_EQ(error->channel, "server-x");
	EXPECT_FALSE(ParsePushEvent(R"({"seq":1})").has_value());

	EXPECT_EQ(SubscribeFrame(true, k_ServersChannel), R"({"channel":"servers","type":"subscribe"})");
	EXPECT_EQ(SubscribeFrame(false, k_ServersChannel), R"({"channel":"servers","type":"unsubscribe"})");
	const std::vector<std::string> channels {"servers", "lobby"};
	EXPECT_EQ(ResumeFrame("ab12", 41, channels), R"({"channels":["servers","lobby"],"epoch":"ab12","seq":41,"type":"resume"})");
	const auto points =
	    ParsePointsChange(R"({"result":3,"points":1032.5,"games_ranked":5,"clan":null,"position":1,"change":16.0})");
	ASSERT_TRUE(points.has_value());
	EXPECT_DOUBLE_EQ(points->points, 1032.5);
	EXPECT_EQ(points->gamesRanked, 5);
	EXPECT_DOUBLE_EQ(points->change, 16.0);
	EXPECT_EQ(ParseBan(R"({"reason":"cheat"})")->reason, "cheat");
}

TEST(LobbyApi, Base64MatchesTheVectorsOfTheStandard)
{
	// The test vectors of RFC 4648
	const std::array<std::pair<std::string_view, std::string_view>, 7> vectors {{
	    {"", ""},
	    {"f", "Zg=="},
	    {"fo", "Zm8="},
	    {"foo", "Zm9v"},
	    {"foob", "Zm9vYg=="},
	    {"fooba", "Zm9vYmE="},
	    {"foobar", "Zm9vYmFy"},
	}};
	for (const auto& [plain, encoded] : vectors)
	{
		const std::vector<uint8_t> bytes(plain.begin(), plain.end());
		EXPECT_EQ(Base64Encode(bytes), encoded);
		const auto decoded = Base64Decode(encoded);
		ASSERT_TRUE(decoded.has_value()) << encoded;
		EXPECT_EQ(*decoded, bytes);
	}
	EXPECT_FALSE(Base64Decode("Zg=").has_value());
	EXPECT_FALSE(Base64Decode("Z===").has_value());
	EXPECT_FALSE(Base64Decode("Zg=a").has_value());
	EXPECT_FALSE(Base64Decode("Zg==Zg==").has_value());
	EXPECT_FALSE(Base64Decode("Zm9v!A==").has_value());
}

TEST(LobbyApi, MultipartBodyHasEachFileAndTheEnd)
{
	const std::array parts {
	    FormPart {.name = "creature", .fileName = "creature.lhp", .contentType = "application/octet-stream", .bytes = {1, 2}},
	    FormPart {.name = "mind", .fileName = "a\"b.chl", .contentType = "application/octet-stream", .bytes = {3}},
	};
	const auto boundary = MultipartBoundary(parts, 1);
	EXPECT_TRUE(boundary.starts_with("openblack-"));
	const auto body = TextOf(MultipartBody("XYZ", parts));
	EXPECT_EQ(body, std::string("--XYZ\r\nContent-Disposition: form-data; name=\"creature\"; filename=\"creature.lhp\"\r\n"
	                            "Content-Type: application/octet-stream\r\n\r\n\x01\x02\r\n"
	                            "--XYZ\r\nContent-Disposition: form-data; name=\"mind\"; filename=\"a_b.chl\"\r\n"
	                            "Content-Type: application/octet-stream\r\n\r\n\x03\r\n"
	                            "--XYZ--\r\n"));
	EXPECT_EQ(MultipartContentType("XYZ"), "multipart/form-data; boundary=XYZ");
}

TEST(LobbyApi, MultipartBoundaryIsNotInTheFiles)
{
	// A file that holds the first boundary of the seed makes the boundary change
	const std::array first {FormPart {.name = "f", .fileName = "f", .contentType = "x", .bytes = {}}};
	const auto taken = MultipartBoundary(first, 7);
	const std::array parts {FormPart {.name = "f", .fileName = "f", .contentType = "x", .bytes = {taken.begin(), taken.end()}}};
	const auto boundary = MultipartBoundary(parts, 7);
	EXPECT_NE(boundary, taken);
}

TEST(LobbyApiJson, AServerEntryCanNameTheLandThatTheLobbyGives)
{
	// A server that plays a land of the game has no map
	auto list = ParseServerList(R"({"servers":[)" + ServerJson() + "]}");
	ASSERT_TRUE(list.has_value());
	EXPECT_TRUE(list->servers.at(0).map.empty());
	auto withMap = ServerJson();
	withMap.insert(1, R"("map":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",)");
	list = ParseServerList(R"({"servers":[)" + withMap + "]}");
	ASSERT_TRUE(list.has_value());
	EXPECT_EQ(list->servers.at(0).map, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
}
