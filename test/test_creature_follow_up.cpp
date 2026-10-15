/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <deque>

#include <gtest/gtest.h>

#include "Creature/CreatureFollowUp.h"

using namespace openblack;
using namespace openblack::creature_follow_up;
using creature_mind::Step;

namespace
{
/// Gives the rolls in turn, and records the ranges asked for
struct FakeRolls
{
	std::deque<uint32_t> rolls;
	std::vector<uint32_t> ranges;

	[[nodiscard]] std::function<uint32_t(uint32_t)> Function()
	{
		return [this](uint32_t range) {
			ranges.push_back(range);
			const auto roll = rolls.front();
			rolls.pop_front();
			return roll;
		};
	}
};

std::vector<Step> Slap(Feelings feelings, std::deque<uint32_t> rolls, float chance = 0.0f)
{
	FakeRolls fake {.rolls = std::move(rolls)};
	return ShowSlapFeeling(feelings, fake.Function(), chance, 10);
}
} // namespace

TEST(CreatureFollowUp, AMushroomWaitsOnlyWhenNothingElseWaits)
{
	EXPECT_EQ(AfterMushroom(std::nullopt), FollowUp::BecomeHigh);
	EXPECT_EQ(AfterMushroom(FollowUp::ShowSlapFeeling), FollowUp::ShowSlapFeeling);
}

TEST(CreatureFollowUp, YoungFeedbackReactsOneTimeInTwo)
{
	EXPECT_EQ(AfterYoungFeedback(std::nullopt, 0.5f, 0), FollowUp::ShowHappy);
	EXPECT_EQ(AfterYoungFeedback(FollowUp::ShowSlapFeeling, -0.5f, 0), FollowUp::FrightenedOfPlayer);
	EXPECT_EQ(AfterYoungFeedback(FollowUp::ShowSlapFeeling, -0.5f, 1), FollowUp::ShowSlapFeeling);
	EXPECT_EQ(AfterYoungFeedback(std::nullopt, 0.5f, 1), std::nullopt);
}

TEST(CreatureFollowUp, FrightenedOfThePlayerRunsAwayOneTimeInThree)
{
	EXPECT_EQ(FrightenedAction(0), "RunAwayFromPlayer");
	EXPECT_EQ(FrightenedAction(1), "BeFrightenedOnTheSpot");
	EXPECT_EQ(FrightenedAction(2), "BeFrightenedOnTheSpot");
}

TEST(CreatureFollowUp, AStrokeBeforeItCanLearnShowsItIsHappy)
{
	const auto agenda = ShowHappy();
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].kind, Step::Kind::Action);
	EXPECT_EQ(agenda[0].animation, k_Happy);
}

TEST(CreatureFollowUp, ASlapFrightensAKindCreature)
{
	const auto agenda = Slap({.compassion = 0.5f, .anger = 0.5f}, {0});
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda[0].animation, k_Frightened);
}

TEST(CreatureFollowUp, ASlapMakesAnAngryCreatureAngryOrRude)
{
	EXPECT_EQ(Slap({.compassion = 0.2f, .anger = 0.5f}, {0, 0})[0].animation, k_Angry);
	EXPECT_EQ(Slap({.compassion = 0.2f, .anger = 0.5f}, {0, 1})[0].animation, k_RudeGesture);
}

TEST(CreatureFollowUp, ASlapCanMakeItSadOrCry)
{
	EXPECT_EQ(Slap({}, {1})[0].animation, k_Sad);
	EXPECT_EQ(Slap({}, {2})[0].animation, k_Cry);
}

TEST(CreatureFollowUp, ASlapCanMakeItLookAtThePlayerForOneToTwoSeconds)
{
	const auto shortest = Slap({}, {3}, 0.0f);
	ASSERT_EQ(shortest.size(), 1u);
	EXPECT_EQ(shortest[0].kind, Step::Kind::WatchCamera);
	EXPECT_EQ(shortest[0].turns, 10u);
	EXPECT_EQ(shortest[0].face, creature_face::Cue::Sad);
	EXPECT_EQ(shortest[0].heldEyes, creature_mind::Eyes::Stoned);
	EXPECT_EQ(Slap({}, {3}, 0.99f)[0].turns, 19u);
}

TEST(CreatureFollowUp, ASlapRollsFourWaysFirst)
{
	FakeRolls fake {.rolls = {0, 1}};
	static_cast<void>(ShowSlapFeeling({.compassion = 0.0f, .anger = 1.0f}, fake.Function(), 0.0f, 10));
	EXPECT_EQ(fake.ranges, (std::vector<uint32_t> {4, 2}));
}

TEST(CreatureFollowUp, AfterTheMealItEatsAliveWhatItAteFromItsHand)
{
	EXPECT_EQ(creature_follow_up::ActionToDoAgain("EatObjectInHand"), "EatAlive");
	EXPECT_EQ(creature_follow_up::ActionToDoAgain("EatAlive"), "EatAlive");
	EXPECT_EQ(creature_follow_up::ActionToDoAgain("PickUpThing"), "PickUpThing");
}
