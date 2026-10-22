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

TEST(LobbyApi, LoginRequestNamesTheClient)
{
	EXPECT_EQ(ToJson(LoginRequest {.name = "abc", .password = "pw", .client = ClientKind::Desktop}),
	          R"({"client":"desktop","name":"abc","password":"pw"})");
	EXPECT_EQ(ToJson(LoginRequest {.name = "abc", .password = "pw", .client = ClientKind::Browser}),
	          R"({"client":"browser","name":"abc","password":"pw"})");
}

TEST(LobbyApi, RefreshRequestOfTheBrowserHasNoToken)
{
	EXPECT_EQ(ToJson(RefreshRequest {.refreshToken = "r1"}), R"({"refresh_token":"r1"})");
	EXPECT_EQ(ToJson(RefreshRequest {}), "{}");
}

TEST(LobbyApi, SessionNeedsATokenATimeAndAnAccount)
{
	const auto desktop =
	    ParseSession(R"({"access_token":"a","expires_in":900,"refresh_token":"r","account":{"id":7,"name":"abc"},"extra":1})");
	ASSERT_TRUE(desktop.has_value());
	EXPECT_EQ(desktop->accessToken, "a");
	EXPECT_EQ(desktop->expiresIn, 900);
	EXPECT_EQ(desktop->refreshToken, "r");
	EXPECT_EQ(desktop->account.id, 7);
	EXPECT_EQ(desktop->account.name, "abc");

	const auto browser = ParseSession(R"({"access_token":"a","expires_in":900,"account":{"id":7,"name":"abc"}})");
	ASSERT_TRUE(browser.has_value());
	EXPECT_FALSE(browser->refreshToken.has_value());

	EXPECT_FALSE(ParseSession(R"({"expires_in":900,"account":{"id":7,"name":"abc"}})").has_value());
	EXPECT_FALSE(ParseSession(R"({"access_token":"","expires_in":900,"account":{"id":7,"name":"abc"}})").has_value());
	EXPECT_FALSE(ParseSession(R"({"access_token":"a","expires_in":0,"account":{"id":7,"name":"abc"}})").has_value());
	EXPECT_FALSE(ParseSession(R"({"access_token":"a","expires_in":"900","account":{"id":7,"name":"abc"}})").has_value());
	EXPECT_FALSE(ParseSession(R"({"access_token":"a","expires_in":900})").has_value());
	EXPECT_FALSE(ParseSession("not json").has_value());
	EXPECT_FALSE(ParseSession("[]").has_value());
}

TEST(LobbyApi, MeHasTheClansAndThePlaceForTheWeather)
{
	const auto me = ParseMe(R"({"id":3,"name":"abc","email":"a@b.c","email_verified":true,"points":1012.5,
	    "clans":[{"id":1,"name":"Gods","points":2000}],"location":{"country":"gb","area":"","city":"Guildford"}})");
	ASSERT_TRUE(me.has_value());
	EXPECT_EQ(me->id, 3);
	EXPECT_TRUE(me->emailVerified);
	EXPECT_DOUBLE_EQ(me->points, 1012.5);
	ASSERT_EQ(me->clans.size(), 1u);
	EXPECT_EQ(me->clans[0].name, "Gods");
	ASSERT_TRUE(me->location.has_value());
	EXPECT_EQ(me->location->city, "Guildford");

	const auto noPlace = ParseMe(R"({"id":3,"name":"abc","location":null})");
	ASSERT_TRUE(noPlace.has_value());
	EXPECT_FALSE(noPlace->location.has_value());
	EXPECT_TRUE(noPlace->clans.empty());
}

TEST(LobbyApi, ServerListReadsEachServer)
{
	const auto list = ParseServerList(R"({"servers":[{"id":5,"name":"Eden","players":3,"max_players":32,"playing":true,
	    "version":"1.0","password_needed":false,"locked":true,"land":"Test"}]})");
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->servers.size(), 1u);
	const auto& server = list->servers[0];
	EXPECT_EQ(server.id, 5);
	EXPECT_EQ(server.players, 3);
	EXPECT_EQ(server.maxPlayers, 32);
	EXPECT_TRUE(server.playing);
	EXPECT_TRUE(server.locked);
	EXPECT_EQ(server.land, "Test");
	EXPECT_FALSE(ParseServerList(R"({"servers":[{"id":5}]})").has_value());
	EXPECT_FALSE(ParseServerList(R"({"servers":{}})").has_value());
}

TEST(LobbyApi, NumbersOutOfRangeAreRefused)
{
	EXPECT_FALSE(ParseServerList(R"({"servers":[{"id":5,"name":"x","players":4294967296,"max_players":32}]})").has_value());
	EXPECT_FALSE(ParseServerList(R"({"servers":[{"id":5,"name":"x","players":1.5,"max_players":32}]})").has_value());
}

TEST(LobbyApi, TicketClansRankingAndPlayer)
{
	const auto ticket = ParseJoinTicket(R"({"url":"wss://bw.example/game/5","ticket":"v4.public.x","expires_in":60})");
	ASSERT_TRUE(ticket.has_value());
	EXPECT_EQ(ticket->url, "wss://bw.example/game/5");
	EXPECT_EQ(ticket->expiresIn, 60);
	EXPECT_FALSE(ParseJoinTicket(R"({"url":"wss://x","ticket":""})").has_value());

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

	const auto reply = ParseClanCreatureReply(ToJson(ClanCreatureReply {.creature = creature}));
	ASSERT_TRUE(reply.has_value());
	EXPECT_TRUE(reply->creature.has_value());
	EXPECT_FALSE(reply->make.has_value());

	const auto make = ParseClanCreatureReply(R"({"new":{"type":3,"name":"Horace"}})");
	ASSERT_TRUE(make.has_value());
	ASSERT_TRUE(make->make.has_value());
	EXPECT_EQ(make->make->type, 3);
	EXPECT_EQ(make->make->name, "Horace");

	EXPECT_FALSE(ParseClanCreatureReply("{}").has_value());
	EXPECT_FALSE(ParseClanCreature(R"({"alignment":0,"mind":"%%%%","physique":""})").has_value());
}

TEST(LobbyApi, ProblemGivesTheDetail)
{
	const auto problem =
	    ParseProblem(R"({"type":"about:blank","title":"Too Many Requests","status":429,"detail":"Wait 5 seconds"})");
	ASSERT_TRUE(problem.has_value());
	EXPECT_EQ(problem->status, 429);
	EXPECT_EQ(problem->detail, "Wait 5 seconds");
}

TEST(LobbyApi, PushEventsAndFrames)
{
	const auto event = ParsePushEvent(R"({"seq":12,"type":"servers.changed","data":{"id":5,"removed":true}})");
	ASSERT_TRUE(event.has_value());
	EXPECT_EQ(event->seq, 12);
	EXPECT_EQ(event->type, events::k_ServersChanged);
	const auto change = ParseServerChange(event->data);
	ASSERT_TRUE(change.has_value());
	EXPECT_TRUE(change->removed);
	EXPECT_EQ(change->server.id, 5);

	const auto ping = ParsePushEvent(R"({"type":"ping"})");
	ASSERT_TRUE(ping.has_value());
	EXPECT_EQ(ping->seq, 0);
	EXPECT_EQ(ping->data, "null");
	EXPECT_FALSE(ParsePushEvent(R"({"seq":1})").has_value());

	EXPECT_EQ(PushFrame(PushOp::Resume, {}, 41), R"({"last_seq":41,"op":"resume"})");
	EXPECT_EQ(PushFrame(PushOp::Subscribe, k_ServersChannel), R"({"channel":"servers","op":"subscribe"})");
	EXPECT_EQ(PushFrame(PushOp::Unsubscribe, k_ServersChannel), R"({"channel":"servers","op":"unsubscribe"})");
	EXPECT_EQ(PushFrame(PushOp::Pong), R"({"op":"pong"})");
	EXPECT_DOUBLE_EQ(ParsePointsChange(R"({"points":1032.5})")->points, 1032.5);
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
