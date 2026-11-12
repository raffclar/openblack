/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <set>
#include <string>
#include <variant>
#include <vector>

#include <GameSocket.h>
#include <JoinTicket.h>
#include <LobbyMembers.h>
#include <gtest/gtest.h>

#include "Online/GameBuildId.h"
#include "Server/GameLifetime.h"
#include "Server/LobbyGame.h"

using namespace openblack;
using namespace openblack::server;

namespace
{

std::vector<lobby_members::Member> MembersOf(std::string_view text)
{
	auto parsed = lobby_members::ParseMembers(text);
	EXPECT_TRUE(std::holds_alternative<std::vector<lobby_members::Member>>(parsed))
	    << (std::holds_alternative<std::string>(parsed) ? std::get<std::string>(parsed) : std::string());
	return std::holds_alternative<std::vector<lobby_members::Member>>(parsed)
	           ? std::get<std::vector<lobby_members::Member>>(parsed)
	           : std::vector<lobby_members::Member> {};
}

bool Refused(std::string_view text)
{
	return std::holds_alternative<std::string>(lobby_members::ParseMembers(text));
}

} // namespace

TEST(LobbyMembers, TheFileGivesEachMemberWithItsRoleAndSlot)
{
	const auto members = MembersOf(R"({"members": [{"account": "101", "role": "god", "slot": 1},
	                                              {"account": "102"},
	                                              {"account": "205", "role": "observer"}]})");
	ASSERT_EQ(members.size(), 3u);
	EXPECT_EQ(members[0], (lobby_members::Member {.account = "101", .role = game_socket::Role::God, .slot = 1}));
	// A member with no role is a god, and a god with no slot takes the first free slot
	EXPECT_EQ(members[1], (lobby_members::Member {.account = "102", .role = game_socket::Role::God, .slot = std::nullopt}));
	EXPECT_EQ(members[2].role, game_socket::Role::Observer);
	EXPECT_FALSE(members[2].slot.has_value());
}

TEST(LobbyMembers, TheFileRefusesMembersThatTheRoomCannotTake)
{
	EXPECT_TRUE(Refused("not json"));
	EXPECT_TRUE(Refused(R"({"players": []})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": ""}]})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": 101}]})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": "1"}, {"account": "1"}]})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": "1", "role": "king"}]})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": "1"}, {"account": "2", "role": "observer", "slot": 1}]})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": "1", "slot": 32}]})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": "1", "slot": -1}]})"));
	EXPECT_TRUE(Refused(R"({"members": [{"account": "1", "slot": 3}, {"account": "2", "slot": 3}]})"));
	// A game needs a god
	EXPECT_TRUE(Refused(R"({"members": [{"account": "1", "role": "observer"}]})"));
	std::string many = R"({"members": [)";
	for (int i = 0; i < 65; ++i)
	{
		many += (i == 0 ? "" : ",") + std::string(R"({"account": ")") + std::to_string(i) + "\"}";
	}
	many += "]}";
	EXPECT_TRUE(Refused(many));
}

TEST(LobbyGame, OnlyAMemberOfTheLobbyInItsRoleIsAdmitted)
{
	const lobby_game::Lobby lobby {.id = "L1",
	                               .members = {{.account = "101", .role = game_socket::Role::God, .slot = 0},
	                                           {.account = "205", .role = game_socket::Role::Observer, .slot = std::nullopt}}};
	EXPECT_EQ(lobby.Gods(), 1u);
	EXPECT_EQ(lobby_game::Admit(lobby, "L1", "101", game_socket::Role::God), lobby_game::Admission::Member);
	EXPECT_EQ(lobby_game::Admit(lobby, "L1", "205", game_socket::Role::Observer), lobby_game::Admission::Member);
	EXPECT_EQ(lobby_game::Admit(lobby, "L2", "101", game_socket::Role::God), lobby_game::Admission::OtherLobby);
	EXPECT_EQ(lobby_game::Admit(lobby, "", "101", game_socket::Role::God), lobby_game::Admission::OtherLobby);
	EXPECT_EQ(lobby_game::Admit(lobby, "L1", "999", game_socket::Role::God), lobby_game::Admission::NotMember);
	EXPECT_EQ(lobby_game::Admit(lobby, "L1", "205", game_socket::Role::God), lobby_game::Admission::OtherRole);
}

TEST(LobbyGame, TheServerRefusesAnotherBuildAndAnotherFileOfTheLand)
{
	EXPECT_FALSE(lobby_game::BuildRefusal("0.1.0+a/codec34", "0.1.0+a/codec34", true).has_value());
	EXPECT_EQ(lobby_game::BuildRefusal("0.1.0+a/codec34", "0.1.0+b/codec34", false), "version");
	EXPECT_EQ(lobby_game::BuildRefusal("0.1.0+a/codec34", "0.1.0+a/codec35", false), "version");
	// An older client gives no build: only the server of a lobby game needs one
	EXPECT_EQ(lobby_game::BuildRefusal("0.1.0+a/codec34", "", true), "version");
	EXPECT_FALSE(lobby_game::BuildRefusal("0.1.0+a/codec34", "", false).has_value());
	// A server with no build id takes every build
	EXPECT_FALSE(lobby_game::BuildRefusal("", "0.1.0+b/codec34", true).has_value());

	EXPECT_FALSE(lobby_game::MapRefusal("aa", "aa").has_value());
	EXPECT_EQ(lobby_game::MapRefusal("aa", "bb"), "map");
	EXPECT_FALSE(lobby_game::MapRefusal("aa", "").has_value());
	EXPECT_FALSE(lobby_game::MapRefusal("", "bb").has_value());
}

TEST(LobbyGame, TheBuildIdHasTheVersionAndTheFormatOfTheTurns)
{
	EXPECT_EQ(online::build_id::Compose("0.1.0+0123456789ab", 34), "0.1.0+0123456789ab/codec34");
	EXPECT_TRUE(online::build_id::Own().find("/codec") != std::string::npos);
}

TEST(LobbyGame, TheJoinCarriesTheBuildAndTheFileOfTheLand)
{
	const game_socket::Join join {.ticket = "t", .gamePassword = {}, .name = {}, .build = "0.1.0+a/codec34", .map = "ab"};
	const auto parsed = game_socket::Parse(game_socket::ToJson(join));
	ASSERT_TRUE(parsed.has_value());
	const auto* back = std::get_if<game_socket::Join>(&*parsed);
	ASSERT_NE(back, nullptr);
	EXPECT_EQ(back->build, "0.1.0+a/codec34");
	EXPECT_EQ(back->map, "ab");
	// An older client sends no build: the join still reads
	const auto older = game_socket::Parse(R"({"type": "room.join", "ticket": "t"})");
	ASSERT_TRUE(older.has_value());
	EXPECT_TRUE(std::get<game_socket::Join>(*older).build.empty());
}

TEST(LobbyGame, TheTicketCanNameTheLobby)
{
	const auto claims = join_ticket::ParseClaims(
	    R"({"iss":"bwgame-service","sub":"101","aud":"g1","iat":"2026-11-12T10:00:00+00:00","nbf":"2026-11-12T10:00:00+00:00","exp":"2026-11-12T10:01:00+00:00","jti":"x","lobby":"L1"})");
	ASSERT_TRUE(claims.has_value());
	EXPECT_EQ(claims->lobby, "L1");
	EXPECT_FALSE(
	    join_ticket::ParseClaims(
	        R"({"iss":"bwgame-service","sub":"101","aud":"g1","iat":"2026-11-12T10:00:00+00:00","nbf":"2026-11-12T10:00:00+00:00","exp":"2026-11-12T10:01:00+00:00","jti":"x","lobby":5})")
	        .has_value());
}

TEST(GameLifetime, EachEndHasItsOwnExitCodeAndLine)
{
	using game_lifetime::End;
	const std::vector<End> ends = {End::AllGodsLeft, End::TimeLimit, End::Deadline, End::NoStart, End::Decided};
	std::set<int> codes;
	for (const auto end : ends)
	{
		codes.insert(game_lifetime::ExitCodeOf(end));
		EXPECT_NE(game_lifetime::LineOf(end).find("(exit code " + std::to_string(game_lifetime::ExitCodeOf(end)) + ")"),
		          std::string::npos);
	}
	EXPECT_EQ(codes.size(), ends.size());
	// A failure of the server is 1, which no end uses
	EXPECT_FALSE(codes.contains(1));
	EXPECT_EQ(game_lifetime::ExitCodeOf(End::AllGodsLeft), 0);
}

TEST(GameLifetime, TheTimeLimitOfALobbyGameIsSixHoursAtMost)
{
	EXPECT_FALSE(game_lifetime::LobbyTimeLimitRefusal(360, false).has_value());
	EXPECT_TRUE(game_lifetime::LobbyTimeLimitRefusal(361, false).has_value());
	EXPECT_TRUE(game_lifetime::LobbyTimeLimitRefusal(1440, true).has_value());
	EXPECT_TRUE(game_lifetime::LobbyTimeLimitRefusal(9, false).has_value());
	EXPECT_FALSE(game_lifetime::LobbyTimeLimitRefusal(1, true).has_value());
}

TEST(GameLifetime, TheDeadlineIsTheLobbysButSixHoursAfterTheListenAtTheLatest)
{
	EXPECT_EQ(game_lifetime::DeadlineOf(std::nullopt, 1000), 1000 + 6 * 3600);
	EXPECT_EQ(game_lifetime::DeadlineOf(2000, 1000), 2000);
	EXPECT_EQ(game_lifetime::DeadlineOf(1000 + 7 * 3600, 1000), 1000 + 6 * 3600);
}

TEST(GameLifetime, TheServerEndsForEachReasonInOrder)
{
	using game_lifetime::End;
	const game_lifetime::Limits limits {.joinWindowSeconds = 120, .deadline = 10'000};
	game_lifetime::Room room {.secondsSinceListen = 10, .now = 5'000, .godsInRoom = 1, .minGods = 2};
	EXPECT_EQ(game_lifetime::Check(limits, room), End::None);

	// The time to join ends with too few gods
	room.secondsSinceListen = 120;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::NoStart);
	// With enough gods the room has more time to start, then it ends with no start
	room.godsInRoom = 2;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::None);
	room.secondsSinceListen = 240;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::NoStart);
	// A room with no time to join (no lobby) waits for ever
	EXPECT_EQ(game_lifetime::Check({.joinWindowSeconds = std::nullopt, .deadline = std::nullopt}, room), End::None);

	room.started = true;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::None);
	// The deadline
	room.now = 10'000;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::Deadline);
	// Every god left before the deadline check
	room.gameOver = true;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::AllGodsLeft);
	room.decidedAt = 9'000;
	room.decidedByTimeLimit = true;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::TimeLimit);
	// Every god left before the time to see the end of a win: the server ends at once with the decision
	room.decidedByTimeLimit = false;
	room.now = 9'001;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::Decided);
	room.decidedByTimeLimit = true;

	// A decided game ends after the time to see the end
	room.gameOver = false;
	room.now = 9'000 + game_lifetime::k_AfterDecisionSeconds - 1;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::None);
	room.now = 9'000 + game_lifetime::k_AfterDecisionSeconds;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::TimeLimit);
	room.decidedByTimeLimit = false;
	EXPECT_EQ(game_lifetime::Check(limits, room), End::Decided);
	// The players have 30 seconds to see the end
	EXPECT_EQ(game_lifetime::k_AfterDecisionSeconds, 30);
}

TEST(GameLifetime, TheRoomWaitsForEachGodOfTheLobbyUntilTheTimeToJoinEnds)
{
	EXPECT_EQ(game_lifetime::GodsToWaitFor(3, 2, false), 3u);
	EXPECT_EQ(game_lifetime::GodsToWaitFor(3, 2, true), 2u);
	EXPECT_EQ(game_lifetime::GodsToWaitFor(1, 2, false), 2u);
}

TEST(GameLifetime, TheWarningComesTenMinutesBeforeTheDeadline)
{
	EXPECT_FALSE(game_lifetime::WarningDue(std::nullopt, 100));
	EXPECT_FALSE(game_lifetime::WarningDue(10'000, 10'000 - 601));
	EXPECT_TRUE(game_lifetime::WarningDue(10'000, 10'000 - 600));
}
