/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <initializer_list>

#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureTurnOrder.h"

using namespace openblack;
using creature_desires::Desire;
namespace turn = openblack::creature_turn_order;

namespace
{
constexpr uint32_t k_TurnsPerSecond = 10;

std::array<bool, creature_desires::k_DesireCount> BodyOnly(std::initializer_list<Desire> desires)
{
	std::array<bool, creature_desires::k_DesireCount> body {};
	for (const auto desire : desires)
	{
		body.at(static_cast<size_t>(desire)) = true;
	}
	return body;
}

creature_desires::Desires AllAt(float value)
{
	creature_desires::Desires desires;
	for (auto& desire : desires.desires)
	{
		desire.value = value;
	}
	return desires;
}
} // namespace

TEST(CreatureTurnOrder, BodyDesiresStayAtZeroBeforeTheFirstStage)
{
	auto desires = AllAt(0.7f);
	turn::HoldBodyDesires(desires, BodyOnly({Desire::Fear, Desire::Tiredness, Desire::Rest}), 0);
	EXPECT_EQ(desires[Desire::Fear].value, 0.0f);
	EXPECT_EQ(desires[Desire::Tiredness].value, 0.0f);
	EXPECT_EQ(desires[Desire::Rest].value, 0.0f);
	EXPECT_EQ(desires[Desire::Curiosity].value, 0.7f);
}

TEST(CreatureTurnOrder, BodyDesiresGrowFromTheFirstStage)
{
	auto desires = AllAt(0.7f);
	turn::HoldBodyDesires(desires, BodyOnly({Desire::Fear}), 1);
	EXPECT_EQ(desires[Desire::Fear].value, 0.7f);
}

TEST(CreatureTurnOrder, PlanStartsAgainOnlyAfterAHoldAndWithStepsToDo)
{
	EXPECT_TRUE(turn::RestartsAfterHold(true, true));
	EXPECT_FALSE(turn::RestartsAfterHold(true, false));
	EXPECT_FALSE(turn::RestartsAfterHold(false, true));
}

TEST(CreatureTurnOrder, HeldByTheHandTheMindStandsStill)
{
	EXPECT_EQ(turn::WhileHeld(false, false), turn::HeldTurn::Think);
	EXPECT_EQ(turn::WhileHeld(false, true), turn::HeldTurn::Think);
	EXPECT_EQ(turn::WhileHeld(true, false), turn::HeldTurn::StandStill);
	EXPECT_EQ(turn::WhileHeld(true, true), turn::HeldTurn::ShowDesire);
}

TEST(CreatureTurnOrder, AHeldCreatureShowsWhatItWantsOnlyWhenItsBodyIsAtRest)
{
	EXPECT_TRUE(turn::ShowsDesireOnHold(false, false, false));
	// The finish of a fight plays during the hold
	EXPECT_FALSE(turn::ShowsDesireOnHold(true, false, false));
	// The turn to the player plays
	EXPECT_FALSE(turn::ShowsDesireOnHold(false, true, false));
	// The finish ended on the last turn, and the turn to the player starts on this turn
	EXPECT_FALSE(turn::ShowsDesireOnHold(false, false, true));
}

TEST(CreatureTurnOrder, FollowingThePlayerIsNotAPlanOfItsOwn)
{
	EXPECT_TRUE(turn::OwnPlanRuns(true, Desire::Hunger));
	EXPECT_FALSE(turn::OwnPlanRuns(true, Desire::FollowPlayerDesire));
	EXPECT_FALSE(turn::OwnPlanRuns(false, Desire::Hunger));
}

TEST(CreatureTurnOrder, CopyTimeoutOccursOnTheTurnAfterFifteenSeconds)
{
	uint32_t turns = 0;
	for (uint32_t i = 0; i < 150; ++i)
	{
		EXPECT_FALSE(turn::StepCopyTimeout(turns, false, k_TurnsPerSecond));
	}
	EXPECT_EQ(turns, 150u);
	EXPECT_TRUE(turn::StepCopyTimeout(turns, false, k_TurnsPerSecond));
	EXPECT_EQ(turns, 0u);
}

TEST(CreatureTurnOrder, CopyTimeoutCountKeepsItsValueWhileAPlanOfItsOwnRan)
{
	uint32_t turns = 40;
	EXPECT_FALSE(turn::StepCopyTimeout(turns, true, k_TurnsPerSecond));
	EXPECT_EQ(turns, 40u);
}

TEST(CreatureTurnOrder, ForcedPlanCountsOnlyWhenTheOrderSaysSo)
{
	EXPECT_TRUE(turn::CompletionOf(Desire::Hunger, false, false, false, 1.0f).counts);
	EXPECT_FALSE(turn::CompletionOf(Desire::Hunger, true, false, false, 1.0f).counts);
	EXPECT_TRUE(turn::CompletionOf(Desire::Hunger, true, true, false, 1.0f).counts);
	const auto forced = turn::CompletionOf(Desire::RestoreHealth, true, false, true, 0.2f);
	EXPECT_FALSE(forced.life.has_value());
	EXPECT_FALSE(forced.climateTemperature);
}

TEST(CreatureTurnOrder, RestoreHealthAddsHalfLifeAboveFull)
{
	const auto completion = turn::CompletionOf(Desire::RestoreHealth, false, false, false, 0.8f);
	ASSERT_TRUE(completion.life.has_value());
	EXPECT_FLOAT_EQ(*completion.life, 1.3f);
	EXPECT_FALSE(completion.climateTemperature);
}

TEST(CreatureTurnOrder, FullRecoveryGivesFullLifeAndTheClimateTemperature)
{
	const auto completion = turn::CompletionOf(Desire::RestoreHealth, false, false, true, 0.2f);
	ASSERT_TRUE(completion.life.has_value());
	EXPECT_EQ(*completion.life, 1.0f);
	EXPECT_TRUE(completion.climateTemperature);
	EXPECT_FALSE(turn::CompletionOf(Desire::Hunger, false, false, false, 0.2f).life.has_value());
}

TEST(CreatureTurnOrder, AReactionStopsBeforeTheDecisionOnlyWhenNoAgendaRuns)
{
	EXPECT_TRUE(turn::StopsReactionBeforeDecision({.planRuns = false, .reacting = true}));
	EXPECT_FALSE(turn::StopsReactionBeforeDecision({.planRuns = true, .reacting = true}));
	EXPECT_FALSE(turn::StopsReactionBeforeDecision({.planRuns = false, .reacting = false}));
}

TEST(CreatureTurnOrder, AForcedPlanOrAReactionKeepsTheCreatureFromDeciding)
{
	EXPECT_TRUE(turn::SkipsDecision({.planRuns = true, .planForced = true}));
	EXPECT_TRUE(turn::SkipsDecision({.planRuns = true, .reacting = true}));
	EXPECT_FALSE(turn::SkipsDecision({.planRuns = true}));
	// With no agenda that runs, the mark and the reaction are gone and the creature decides
	EXPECT_FALSE(turn::SkipsDecision({.planRuns = false, .planForced = true, .reacting = true}));
}

TEST(CreatureTurnOrder, OnlyAFreeDecisionDoesTheThingThatWaits)
{
	EXPECT_TRUE(turn::DoesFollowUp({}, false, false));
	// An agenda still runs
	EXPECT_FALSE(turn::DoesFollowUp({.planRuns = true}, false, false));
	// An order on a thing plans for one desire, and the thing waits until the order is done
	EXPECT_FALSE(turn::DoesFollowUp({}, true, false));
	EXPECT_FALSE(turn::DoesFollowUp({}, false, true));
}
