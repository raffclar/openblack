/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>
#include <vector>

#include <LobbyApiJson.h>
#include <gtest/gtest.h>

#include "Server/LobbyHostReport.h"

using namespace openblack;
using namespace openblack::server;
using namespace openblack::server::lobby_host_report;
namespace host_report = openblack::gathering_box::host_report;

namespace
{

LockstepHost::Seat SeatOf(LockstepHost::SeatState state, std::string account)
{
	LockstepHost::Seat seat;
	seat.state = state;
	seat.account = std::move(account);
	return seat;
}

ecs::components::GameStanding StandingWith(game_end::Condition condition, ecs::components::WinCondition value)
{
	ecs::components::GameStanding standing;
	standing.conditions.at(static_cast<size_t>(condition)) = value;
	return standing;
}

} // namespace

TEST(LobbyHostReport, TheAddressOfTheReport)
{
	EXPECT_EQ(HostReportAddress("https://lobby.test/api/v1", "abc"), "https://lobby.test/api/v1/servers/abc/host-report");
}

TEST(LobbyHostReport, EachGodOfTheFirstFourSlotsIsATeamOfOne)
{
	const std::vector<LockstepHost::Seat> seats {
	    SeatOf(LockstepHost::SeatState::Taken, "41"), SeatOf(LockstepHost::SeatState::Open, "42"),
	    SeatOf(LockstepHost::SeatState::Away, "43"),  SeatOf(LockstepHost::SeatState::Closed, "not-an-account"),
	    SeatOf(LockstepHost::SeatState::Taken, "45"),
	};
	const std::vector<lobby_api::ServerStatusClan> clans {{.account = 43, .clan = 7}, {.account = 99, .clan = 8}};
	const auto teams = TeamsOf(seats, clans);
	EXPECT_EQ(teams[0][0].number, 41u);
	EXPECT_EQ(teams[0][0].clan, 0u);
	// A slot with no player is an empty team
	EXPECT_EQ(teams[1][0].number, 0u);
	// A god whose human is away or left stays in its team
	EXPECT_EQ(teams[2][0].number, 43u);
	EXPECT_EQ(teams[2][0].clan, 7u);
	// A subject that is not an account id gives an empty team
	EXPECT_EQ(teams[3][0].number, 0u);
	for (const auto& team : teams)
	{
		for (size_t member = 1; member < team.size(); ++member)
		{
			EXPECT_EQ(team[member].number, 0u);
			EXPECT_EQ(team[member].clan, 0u);
		}
	}
}

TEST(LobbyHostReport, TheConditionsOfTheGameWithTheStateOfEachOfTheFirstFourPlayers)
{
	const std::vector<game_end::ConditionPick> picks {{.condition = game_end::Condition::HousesBuilt, .value = 20},
	                                                  {.condition = game_end::Condition::TreesGrown, .value = 5}};
	ReportedStandings standings {};
	standings[0] = StandingWith(game_end::Condition::HousesBuilt, {.current = 4, .target = 20, .completed = false});
	standings[1] = StandingWith(game_end::Condition::HousesBuilt, {.current = 25, .target = 20, .completed = true});
	// The third player has a standing but not the condition, and the fourth has no standing
	standings[2] = ecs::components::GameStanding {};
	const auto conditions = ConditionsOf(picks, standings);
	ASSERT_TRUE(conditions[static_cast<size_t>(game_end::Condition::HousesBuilt)].has_value());
	const auto& houses = *conditions[static_cast<size_t>(game_end::Condition::HousesBuilt)];
	EXPECT_EQ(houses.aim, 20);
	EXPECT_EQ(houses.players[0].current, 4);
	EXPECT_FALSE(houses.players[0].met);
	EXPECT_TRUE(houses.players[1].met);
	EXPECT_EQ(houses.players[2].current, 0);
	EXPECT_FALSE(houses.players[3].met);
	ASSERT_TRUE(conditions[static_cast<size_t>(game_end::Condition::TreesGrown)].has_value());
	EXPECT_EQ(conditions[static_cast<size_t>(game_end::Condition::TreesGrown)]->aim, 5);
	EXPECT_FALSE(conditions[static_cast<size_t>(game_end::Condition::VillagersKilled)].has_value());
}

TEST(LobbyHostReport, TheBodyHasTheReportInBase64)
{
	const host_report::Facts facts {.teams = {}, .turn = 1, .decided = false, .conditions = {}};
	const auto bytes = host_report::Encode(facts);
	const auto body = BodyOf(bytes);
	ASSERT_TRUE(body.starts_with(R"({"report":")"));
	ASSERT_TRUE(body.ends_with(R"("})"));
	const auto text = body.substr(11, body.size() - 13);
	const auto decoded = lobby_api::Base64Decode(text);
	ASSERT_TRUE(decoded.has_value());
	EXPECT_EQ(*decoded, bytes);
	// The lobby takes 72 bytes, and 20 bytes for each condition
	EXPECT_EQ(decoded->size(), 72u);
}
