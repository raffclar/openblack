/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include "Creature/CreatureFootball.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace football = openblack::creature_football;
namespace plan_actions = openblack::creature_plan_actions;

namespace
{
uint32_t First(uint32_t /*range*/)
{
	return 0;
}
uint32_t Last(uint32_t range)
{
	return range - 1;
}

plan_actions::Situation FootballSituation(bool handFull, bool otherTeamHasPlayer)
{
	plan_actions::Situation situation;
	situation.handFull = handFull;
	situation.football = plan_actions::Situation::Football {
	    .ball = 7,
	    .goal = {30.0f, -12.0f},
	    .otherFirstPlayer = otherTeamHasPlayer ? std::optional<uint32_t>(9) : std::nullopt,
	};
	return situation;
}
} // namespace

// The creature of a player with an even number plays for the home team
TEST(CreatureFootball, EvenPlayersAreOnTheHomeTeam)
{
	EXPECT_TRUE(football::IsOnHomeTeam(0));
	EXPECT_FALSE(football::IsOnHomeTeam(1));
	EXPECT_TRUE(football::IsOnHomeTeam(2));
	EXPECT_FALSE(football::IsOnHomeTeam(7));
}

// The game compares the creature's side with the side that mourns the goal
TEST(CreatureFootball, MySideScoredIsTheSideThatMourns)
{
	EXPECT_TRUE(football::MySideJustScored(true, true, true));
	EXPECT_FALSE(football::MySideJustScored(true, false, true));
	EXPECT_TRUE(football::MySideJustScored(true, false, false));
	EXPECT_FALSE(football::MySideJustScored(false, true, true));
	EXPECT_TRUE(football::OtherSideJustScored(true, false, true));
	EXPECT_FALSE(football::OtherSideJustScored(true, true, true));
	EXPECT_TRUE(football::OtherSideJustScored(true, true, false));
	EXPECT_FALSE(football::OtherSideJustScored(false, false, true));
}

// Every football action acts on a villager that plays football. Four of them throw the ball at the goal.
TEST(CreatureFootball, TheActionsOfTheGamesTableHaveTheirAgendas)
{
	using plan_actions::Build;
	using plan_actions::Target;
	struct Row
	{
		std::string_view action;
		Target target;
		Build build;
	};
	constexpr std::array k_Rows {
	    Row {"AttackerThrowBallAtGoal", Target::Footballer, Build::ThrowBallAtGoal},
	    Row {"AttackerKickBallAtGoal", Target::Footballer, Build::ThrowBallAtGoal},
	    Row {"DefenderStompOnBall", Target::Footballer, Build::StompOnBall},
	    Row {"DefenderClearBall", Target::Footballer, Build::ThrowBallAtGoal},
	    Row {"GoalieCatchBall", Target::Footballer, Build::ThrowBallAtGoal},
	    Row {"GoalieFoul", Target::Footballer, Build::GoalkeeperFoul},
	    Row {"CelebrateGoal", Target::FootballerMySideScored, Build::CelebrateGoal},
	    Row {"CommiserateGoal", Target::FootballerOtherSideScored, Build::CommiserateGoal},
	};
	for (const auto& row : k_Rows)
	{
		const auto* executor = plan_actions::For(row.action);
		ASSERT_NE(executor, nullptr) << row.action;
		EXPECT_EQ(executor->target, row.target) << row.action;
		EXPECT_EQ(executor->build, row.build) << row.action;
	}
}

// With an empty hand, the creature feels playful two times in three, then picks up the ball, then throws it at the goal
TEST(CreatureFootball, AThrowAtGoalPicksUpTheBallFirstWithAnEmptyHand)
{
	const auto* executor = plan_actions::For("AttackerThrowBallAtGoal");
	ASSERT_NE(executor, nullptr);
	const auto playful = plan_actions::Agenda(*executor, 3, {0.0f, 0.0f}, FootballSituation(false, true), Last);
	ASSERT_TRUE(playful.has_value());
	ASSERT_EQ(playful->size(), 3u);
	EXPECT_EQ((*playful)[0].kind, Step::Kind::Action);
	EXPECT_EQ((*playful)[0].animation, 67u);
	EXPECT_EQ((*playful)[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ((*playful)[1].order.object, 7u);
	ASSERT_TRUE((*playful)[1].gaze.has_value());
	EXPECT_TRUE((*playful)[1].gaze->whileGoing);
	EXPECT_EQ((*playful)[2].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_FLOAT_EQ((*playful)[2].order.point.x, 30.0f);
	EXPECT_FLOAT_EQ((*playful)[2].order.point.y, -12.0f);
	EXPECT_TRUE((*playful)[2].main);
	const auto plain = plan_actions::Agenda(*executor, 3, {0.0f, 0.0f}, FootballSituation(false, true), First);
	ASSERT_TRUE(plain.has_value());
	ASSERT_EQ(plain->size(), 2u);
	EXPECT_EQ((*plain)[0].order.kind, ObjectOrder::Kind::PickUp);
}

// With something in its hand already, the creature throws that at the goal, and draws no random number
TEST(CreatureFootball, AThrowAtGoalWithAFullHandOnlyThrows)
{
	const auto* executor = plan_actions::For("GoalieCatchBall");
	ASSERT_NE(executor, nullptr);
	uint32_t draws = 0;
	const auto agenda = plan_actions::Agenda(*executor, 3, {0.0f, 0.0f}, FootballSituation(true, true), [&draws](uint32_t) {
		++draws;
		return 0u;
	});
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 1u);
	EXPECT_EQ((*agenda)[0].order.kind, ObjectOrder::Kind::Throw);
	EXPECT_EQ(draws, 0u);
}

// The stomps go on the ball, or on the first player of the other team, with an angry face
TEST(CreatureFootball, TheStompsGoOnTheBallOrTheOtherTeamsFirstPlayer)
{
	const auto* ball = plan_actions::For("DefenderStompOnBall");
	ASSERT_NE(ball, nullptr);
	const auto onBall = plan_actions::Agenda(*ball, 3, {0.0f, 0.0f}, FootballSituation(false, true), First);
	ASSERT_TRUE(onBall.has_value());
	ASSERT_EQ(onBall->size(), 1u);
	EXPECT_EQ((*onBall)[0].order.kind, ObjectOrder::Kind::Destroy);
	EXPECT_EQ((*onBall)[0].order.object, 7u);
	EXPECT_EQ((*onBall)[0].face, creature_face::Cue::Anger);
	EXPECT_TRUE((*onBall)[0].main);
	const auto* foul = plan_actions::For("GoalieFoul");
	ASSERT_NE(foul, nullptr);
	const auto onPlayer = plan_actions::Agenda(*foul, 3, {0.0f, 0.0f}, FootballSituation(false, true), First);
	ASSERT_TRUE(onPlayer.has_value());
	EXPECT_EQ((*onPlayer)[0].order.object, 9u);
	// With no player in the other team, the foul fails
	EXPECT_FALSE(plan_actions::Agenda(*foul, 3, {0.0f, 0.0f}, FootballSituation(false, false), First).has_value());
}

// The ball actions fail when the villager plays on no pitch
TEST(CreatureFootball, TheBallActionsFailWithoutAPitch)
{
	for (const auto* action : {"AttackerKickBallAtGoal", "DefenderClearBall", "DefenderStompOnBall", "GoalieFoul"})
	{
		const auto* executor = plan_actions::For(action);
		ASSERT_NE(executor, nullptr);
		EXPECT_FALSE(plan_actions::Agenda(*executor, 3, {0.0f, 0.0f}, {}, First).has_value()) << action;
	}
}

// The creature is happy about a goal of its side, and sad about a goal of the other side
TEST(CreatureFootball, ItCelebratesAndCommiserates)
{
	const auto* celebrate = plan_actions::For("CelebrateGoal");
	ASSERT_NE(celebrate, nullptr);
	const auto happy = plan_actions::Agenda(*celebrate, 3, {0.0f, 0.0f}, {}, First);
	ASSERT_TRUE(happy.has_value());
	ASSERT_EQ(happy->size(), 1u);
	EXPECT_EQ((*happy)[0].animation, 55u);
	EXPECT_TRUE((*happy)[0].main);
	const auto* commiserate = plan_actions::For("CommiserateGoal");
	ASSERT_NE(commiserate, nullptr);
	const auto sad = plan_actions::Agenda(*commiserate, 3, {0.0f, 0.0f}, {}, First);
	ASSERT_TRUE(sad.has_value());
	EXPECT_EQ((*sad)[0].animation, 56u);
}
