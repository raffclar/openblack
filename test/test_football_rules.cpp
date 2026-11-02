/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/FootballRules.h"

namespace rules = openblack::ecs::football_rules;
using rules::Role;
using Check = rules::Test;

TEST(FootballRules, TheAddOnSwitchWorksOnlyOutsideAMultiplayerGame)
{
	EXPECT_TRUE(rules::IsFootballOn(true, false));
	EXPECT_FALSE(rules::IsFootballOn(true, true));
	EXPECT_FALSE(rules::IsFootballOn(false, false));
	EXPECT_FALSE(rules::IsFootballOn(false, true));
}

TEST(FootballRules, AVillagerJoinsAsRefereeThenHomeThenAwayThenCrowd)
{
	EXPECT_EQ(rules::JoinAs(false, 0, 0), rules::Joins::Referee);
	EXPECT_EQ(rules::JoinAs(false, 5, 5), rules::Joins::Referee);
	EXPECT_EQ(rules::JoinAs(true, 4, 0), rules::Joins::Home);
	EXPECT_EQ(rules::JoinAs(true, 5, 4), rules::Joins::Away);
	EXPECT_EQ(rules::JoinAs(true, 5, 5), rules::Joins::Crowd);
}

TEST(FootballRules, TheHomeTeamAttacksTheSecondGoal)
{
	EXPECT_EQ(rules::AttackedGoal(rules::Side::Home), 1u);
	EXPECT_EQ(rules::KeptGoal(rules::Side::Home), 0u);
	EXPECT_EQ(rules::AttackedGoal(rules::Side::Away), 0u);
	EXPECT_EQ(rules::KeptGoal(rules::Side::Away), 1u);
	EXPECT_EQ(rules::Other(rules::Side::Home), rules::Side::Away);
}

TEST(FootballRules, TheWaveGoesRoundTheCrowdThenRests)
{
	rules::Wave wave;
	for (uint32_t place = 0; place < rules::k_WavePlaces; ++place)
	{
		EXPECT_EQ(rules::StepWave(wave), place);
	}
	EXPECT_EQ(wave.rest, rules::k_WaveRestTurns);
	EXPECT_EQ(wave.next, 0u);
	for (uint32_t turn = 0; turn < rules::k_WaveRestTurns; ++turn)
	{
		EXPECT_FALSE(rules::StepWave(wave).has_value());
	}
	EXPECT_EQ(rules::StepWave(wave), 0u);
}

TEST(FootballRules, TooFewPlayersAreSentHomeAfterTheWait)
{
	rules::Match match;
	const auto walking = []() { return 0u; };
	for (uint32_t turn = 1; turn <= rules::k_SendHomeTurns; ++turn)
	{
		EXPECT_FALSE(rules::StepWaiting(match, 2, 5, walking).sendPlayersHome);
	}
	EXPECT_TRUE(rules::StepWaiting(match, 2, 5, walking).sendPlayersHome);
	// The count does not start again, so the players go home each turn after
	EXPECT_TRUE(rules::StepWaiting(match, 5, 2, walking).sendPlayersHome);
	EXPECT_EQ(match.state, rules::PlayState::WaitingForPlayers);
}

TEST(FootballRules, TheKickOffComesWhenTheBallIsOnTheSpotAndFewPlayersWalk)
{
	rules::Match match;
	uint32_t walkers = 5;
	const auto walking = [&walkers]() { return walkers; };
	for (uint32_t turn = 1; turn <= rules::k_KickOffWaitTurns; ++turn)
	{
		const auto step = rules::StepWaiting(match, 3, 3, walking);
		EXPECT_FALSE(step.placeBall);
		EXPECT_FALSE(step.kickOff);
	}
	auto step = rules::StepWaiting(match, 3, 3, walking);
	EXPECT_TRUE(step.placeBall);
	EXPECT_FALSE(step.kickOff);
	EXPECT_TRUE(match.kickOffReady);
	EXPECT_EQ(match.turns, 0u);
	walkers = 4;
	step = rules::StepWaiting(match, 3, 3, walking);
	EXPECT_FALSE(step.placeBall);
	EXPECT_TRUE(step.kickOff);
	EXPECT_EQ(match.state, rules::PlayState::Playing);
	EXPECT_EQ(match.roleTurns, rules::k_ReRoleTurns);
}

TEST(FootballRules, AMatchNeedsMoreThanOnePlayerEach)
{
	EXPECT_TRUE(rules::PlaysOn(2, 1));
	EXPECT_TRUE(rules::PlaysOn(1, 2));
	EXPECT_FALSE(rules::PlaysOn(1, 1));
	EXPECT_FALSE(rules::PlaysOn(0, 5));
	EXPECT_FALSE(rules::PlaysOn(5, 0));
}

TEST(FootballRules, ATownMatchStopsWhenItRanLongAndTheTownWantsOtherThings)
{
	rules::Match match;
	match.turns = rules::k_TownMatchShortestTurns - 1;
	EXPECT_FALSE(rules::TownMatchStops(match, false));
	EXPECT_TRUE(rules::TownMatchStops(match, false));
	EXPECT_FALSE(rules::TownMatchStops(match, true));
}

TEST(FootballRules, TheTeamsGetTheirRolesAgainAfterAWhile)
{
	rules::Match match;
	match.roleTurns = rules::k_ReRoleTurns - 1;
	EXPECT_FALSE(rules::RoleTimeIsOver(match));
	EXPECT_EQ(match.roleTurns, rules::k_ReRoleTurns);
	EXPECT_TRUE(rules::RoleTimeIsOver(match));
	EXPECT_EQ(match.roleTurns, 0u);
}

TEST(FootballRules, PlayStartsAgainAfterTheGoalPause)
{
	rules::Match match;
	match.state = rules::PlayState::Scored;
	for (uint32_t turn = 1; turn <= rules::k_GoalPauseTurns; ++turn)
	{
		EXPECT_FALSE(rules::StepGoalPause(match));
	}
	EXPECT_TRUE(rules::StepGoalPause(match));
	EXPECT_EQ(match.state, rules::PlayState::Playing);
	EXPECT_EQ(match.roleTurns, rules::k_ReRoleTurns);
}

TEST(FootballRules, TheTallyCountsAGoalForTheTeamThatMourns)
{
	rules::Match match;
	EXPECT_TRUE(rules::ScoreGoal(match, 0));
	EXPECT_EQ(match.state, rules::PlayState::Scored);
	EXPECT_EQ(match.score.at(0), 1);
	EXPECT_EQ(match.score.at(1), 0);
	EXPECT_FALSE(rules::ScoreGoal(match, 1));
	EXPECT_EQ(match.score.at(1), 1);
	EXPECT_FALSE(match.homeMournedLastGoal);
}

TEST(FootballRules, TheBallScoresWhenItCrossesTheGoalLineUnderTheBar)
{
	const rules::GoalMouth goal {.line = 13.555267f, .height = 1.3487837f, .halfWidth = 2.6265788f};
	EXPECT_TRUE(rules::CrossesGoal({13.0f, 0.5f, 0.0f}, {14.0f, 0.5f, 0.0f}, goal));
	EXPECT_TRUE(rules::CrossesGoal({14.0f, 0.5f, 1.0f}, {13.0f, 0.5f, 1.0f}, goal));
	// Over the bar, wide of the posts, or short of the line
	EXPECT_FALSE(rules::CrossesGoal({13.0f, 2.0f, 0.0f}, {14.0f, 2.0f, 0.0f}, goal));
	EXPECT_FALSE(rules::CrossesGoal({13.0f, 0.5f, 3.0f}, {14.0f, 0.5f, 3.0f}, goal));
	EXPECT_FALSE(rules::CrossesGoal({13.0f, 0.5f, 0.0f}, {13.5f, 0.5f, 0.0f}, goal));
	// A ball that starts on the line counts as it leaves the line backwards
	EXPECT_TRUE(rules::CrossesGoal({goal.line, 5.0f, 9.0f}, {12.0f, 5.0f, 9.0f}, goal));
}

TEST(FootballRules, OnlyTheEndsOfThePitchPutTheBallOutOfPlay)
{
	EXPECT_TRUE(rules::IsOnPitch(5.0f, 10.0f));
	EXPECT_TRUE(rules::IsOnPitch(-10.0f, 10.0f));
	EXPECT_FALSE(rules::IsOnPitch(10.5f, 10.0f));
	EXPECT_FALSE(rules::IsOnPitch(-10.5f, 10.0f));
}

TEST(FootballRules, TheFartherFirstPlayerFetchesADeadBall)
{
	EXPECT_EQ(rules::DeadBallFetcher(100, 200), rules::Side::Away);
	EXPECT_EQ(rules::DeadBallFetcher(300, 200), rules::Side::Home);
	EXPECT_EQ(rules::DeadBallFetcher(200, 200), rules::Side::Away);
}

TEST(FootballRules, TheNearerHalfAttacksAndTheLastKeepsGoal)
{
	EXPECT_EQ(rules::RoleForRank(0, 5).role, Role::Attacker);
	EXPECT_EQ(rules::RoleForRank(1, 5).number, 1);
	EXPECT_EQ(rules::RoleForRank(2, 5).role, Role::Defender);
	EXPECT_EQ(rules::RoleForRank(2, 5).number, 0);
	EXPECT_EQ(rules::RoleForRank(3, 5).number, 1);
	EXPECT_EQ(rules::RoleForRank(4, 5).role, Role::Goalkeeper);
	EXPECT_EQ(rules::RoleForRank(0, 1).role, Role::Goalkeeper);
	EXPECT_EQ(rules::RoleForRank(0, 2).role, Role::Attacker);
	EXPECT_EQ(rules::RoleForRank(1, 2).role, Role::Goalkeeper);
}

TEST(FootballRules, TheOrderByDistanceKeepsTiesInTeamOrder)
{
	const std::vector<int32_t> distances {5, 3, 3, 1};
	EXPECT_EQ(rules::OrderByDistance(distances), (std::vector<size_t> {3, 1, 2, 0}));
}

TEST(FootballRules, ADefenderMarksOnlyWhenNearerThanTheNearestAttacker)
{
	const std::vector<Role> roles {Role::Attacker, Role::Defender, Role::Attacker, Role::Defender};
	const auto marks = rules::FindMarks(roles, std::vector<uint32_t> {10, 5, 8, 9});
	EXPECT_EQ(marks.attacker, 2u);
	EXPECT_EQ(marks.defender, 1u);
	const auto noDefender = rules::FindMarks(roles, std::vector<uint32_t> {4, 5, 8, 9});
	EXPECT_EQ(noDefender.attacker, 0u);
	EXPECT_FALSE(noDefender.defender.has_value());
}

TEST(FootballRules, ThePlayersStandInLinesAcrossThePitch)
{
	EXPECT_FLOAT_EQ(rules::LineOffset(0, 1, 0.8f), 0.0f);
	EXPECT_FLOAT_EQ(rules::LineOffset(0, 2, 0.8f), -0.4f);
	EXPECT_FLOAT_EQ(rules::LineOffset(1, 2, 0.8f), 0.4f);
	EXPECT_FLOAT_EQ(rules::LineOffset(0, 3, 0.5f), -0.25f);
	EXPECT_FLOAT_EQ(rules::LineOffset(1, 3, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(rules::LineOffset(2, 3, 0.5f), 0.25f);
	const auto attacker = rules::AttackerPlace(0, 5);
	EXPECT_FLOAT_EQ(attacker.side, -0.4f);
	EXPECT_FLOAT_EQ(attacker.ahead, 0.33f);
	const auto defender = rules::DefenderPlace(1, 5);
	EXPECT_FLOAT_EQ(defender.side, 0.25f);
	EXPECT_FLOAT_EQ(defender.ahead, -0.76f);
}

TEST(FootballRules, APlaceIsAPartOfTheWayToTheGoalAndToTheSide)
{
	const auto point = rules::PlacePoint({0.0f, 0.0f}, {10.0f, 0.0f}, {.side = 0.5f, .ahead = 0.33f});
	EXPECT_NEAR(point.x, 3.3f, 1e-5f);
	EXPECT_NEAR(point.y, 5.0f, 1e-5f);
	const auto pass = rules::PassPlace({0.0f, 0.0f}, {10.0f, 0.0f}, 0.7f, 3.0f);
	EXPECT_NEAR(pass.x, 7.0f, 1e-5f);
	EXPECT_NEAR(pass.y, 3.0f, 1e-5f);
}

TEST(FootballRules, ThePrioritiesFollowTheDistances)
{
	EXPECT_FLOAT_EQ(rules::ShootPriority(0.0f), 1.0f);
	EXPECT_FLOAT_EQ(rules::ShootPriority(20.0f), 0.5f);
	EXPECT_FLOAT_EQ(rules::ShootPriority(100.0f), 0.1f);
	EXPECT_FLOAT_EQ(rules::DribblePriority(0.0f), 0.1f);
	EXPECT_FLOAT_EQ(rules::DribblePriority(20.0f), 0.5f);
	EXPECT_FLOAT_EQ(rules::DribblePriority(60.0f), 1.0f);
	EXPECT_FLOAT_EQ(rules::RunForPassPriority(2.0f), 0.1f);
	EXPECT_FLOAT_EQ(rules::RunForPassPriority(10.0f), 0.2f);
	EXPECT_FLOAT_EQ(rules::GoToPlacePriority(0.0f), 0.1f);
	EXPECT_FLOAT_EQ(rules::GoToPlacePriority(10.0f), 0.5f);
	EXPECT_FLOAT_EQ(rules::GoToPlacePriority(30.0f), 1.0f);
	EXPECT_FLOAT_EQ(rules::IdlePriority(40.0f, 0.0f), 0.75f);
	EXPECT_FLOAT_EQ(rules::IdlePriority(5.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(rules::IdlePriority(40.0f, 10.0f), 0.0f);
}

TEST(FootballRules, APlayerTakesTheOpenTaskWithTheHighestPriority)
{
	std::vector<uint8_t> asked;
	const auto nearest = [](Check test) { return test == Check::NearestToBall; };
	const std::vector<float> values {0.2f, 0.9f, 0.0f, 0.5f};
	const auto chosen = rules::ChooseTask(Role::Attacker, nearest, [&](uint8_t task) {
		asked.push_back(task);
		return values.at(task);
	});
	EXPECT_EQ(chosen, 1u);
	// Only the open tasks give a priority
	EXPECT_EQ(asked, (std::vector<uint8_t> {0, 1, 2, 3}));
	// A tie keeps the first
	EXPECT_EQ(rules::ChooseTask(Role::Attacker, nearest, [](uint8_t) { return 0.5f; }), 0u);
	// No open task, or no priority above nothing, gives no task
	EXPECT_FALSE(rules::ChooseTask(Role::Attacker, [](Check) { return false; }, [](uint8_t) { return 1.0f; }).has_value());
	EXPECT_FALSE(rules::ChooseTask(Role::Attacker, nearest, [](uint8_t) { return -1.0f; }).has_value());
}

TEST(FootballRules, AGoalkeeperAwayFromItsGoalOnlyGoesBackOrToTheBall)
{
	const auto away = [](Check test) {
		return test == Check::QuiteNearBall || test == Check::NotAtPlace || test == Check::DoesNotTouchBall;
	};
	std::vector<uint8_t> asked;
	static_cast<void>(rules::ChooseTask(Role::Goalkeeper, away, [&asked](uint8_t task) {
		asked.push_back(task);
		return 0.5f;
	}));
	EXPECT_EQ(asked, (std::vector<uint8_t> {static_cast<uint8_t>(rules::GoalkeeperTask::Pass),
	                                        static_cast<uint8_t>(rules::GoalkeeperTask::GoToBall),
	                                        static_cast<uint8_t>(rules::GoalkeeperTask::GoToPlace)}));
	EXPECT_EQ(rules::TasksOf(Role::Goalkeeper).size(), 7u);
	EXPECT_EQ(rules::TasksOf(Role::Defender).size(), 8u);
}

TEST(FootballRules, TheBeliefOfAMatchIsAtMostTwiceTheBeliefForAMatch)
{
	EXPECT_FLOAT_EQ(rules::MatchBelief(10.0f, 0, 30.0f), 0.0f);
	EXPECT_FLOAT_EQ(rules::MatchBelief(10.0f, 3000, 30.0f), 10.0f);
	EXPECT_FLOAT_EQ(rules::MatchBelief(10.0f, 9000, 30.0f), 20.0f);
}

TEST(FootballRules, TheCrowdStandsAtStepsRoundThePitch)
{
	EXPECT_NEAR(rules::CrowdPlaceAngle(10), 1.5707964f, 1e-6f);
	EXPECT_FLOAT_EQ(rules::CrowdPlaceAngle(0), 0.0f);
}

TEST(FootballRules, AScatterMovesAPlaceInTheGameSteps)
{
	EXPECT_EQ(rules::Scatter(65536, 0.0f), 65536);
	EXPECT_EQ(rules::Scatter(0, 1.0f), 6553);
	EXPECT_EQ(rules::Scatter(65536, -1.0f), 58982);
}
