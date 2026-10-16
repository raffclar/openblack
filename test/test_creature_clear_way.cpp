/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureClearWay.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureRouteFollow.h"
#include "Creature/LeashRules.h"

using namespace openblack;
using creature_clear_way::Actions;
using creature_clear_way::Miracle;
using creature_clear_way::Obstacle;

namespace
{
constexpr Actions k_Actions {.throwAround = 263, .kick = 236};
constexpr auto k_Anger = static_cast<uint32_t>(creature_desires::Desire::Anger);
constexpr auto k_Compassion = static_cast<uint32_t>(creature_desires::Desire::Compassion);

/// A random function that gives its answers in turn, and notes each range that it was asked for
struct Rolls
{
	std::vector<uint32_t> answers;
	std::vector<uint32_t> ranges;
	size_t next {0};
	uint32_t operator()(uint32_t range)
	{
		ranges.push_back(range);
		return answers.at(next++);
	}
};
} // namespace

TEST(CreatureClearWay, NeverClearsATempleHeartAStoragePitOrATownCentre)
{
	for (const auto obstacle : {Obstacle {.templeHeart = true, .canPickUp = true, .canKick = true},
	                            Obstacle {.storagePit = true, .canPickUp = true, .canKick = true},
	                            Obstacle {.townCentre = true, .canPickUp = true, .canKick = true}})
	{
		EXPECT_FALSE(creature_clear_way::ClearingAction(obstacle, {}, k_Actions).has_value());
	}
}

TEST(CreatureClearWay, ThrowsBeforeItKicks)
{
	EXPECT_EQ(creature_clear_way::ClearingAction({.canPickUp = true, .canKick = true}, {}, k_Actions), 263u);
	EXPECT_EQ(creature_clear_way::ClearingAction({.canKick = true}, {}, k_Actions), 236u);
}

TEST(CreatureClearWay, CastsTheFirstKnownMiracleOfAnger)
{
	const std::array<Miracle, 5> miracles {
	    Miracle {.known = false, .desire = k_Anger, .action = 10},
	    Miracle {.known = true, .desire = k_Compassion, .action = 11},
	    Miracle {.known = true, .desire = k_Anger, .action = 0},
	    Miracle {.known = true, .desire = k_Anger, .action = 13},
	    Miracle {.known = true, .desire = k_Anger, .action = 14},
	};
	EXPECT_EQ(creature_clear_way::ClearingAction({}, miracles, k_Actions), 13u);
	EXPECT_FALSE(creature_clear_way::ClearingAction({}, std::span(miracles).first(3), k_Actions).has_value());
}

TEST(CreatureClearWay, KeepsAtMostSixteenThingsThatItCanClear)
{
	std::vector<uint32_t> met;
	for (uint32_t thing = 0; thing < 40; ++thing)
	{
		met.push_back(thing);
	}
	met.insert(met.begin(), 4u);
	const auto kept = creature_clear_way::ThingsToClear(met, [](uint32_t thing) { return thing % 2 == 0; });
	ASSERT_EQ(kept.size(), creature_clear_way::k_MostToClear);
	EXPECT_EQ(kept.front(), 4u);
	EXPECT_EQ(kept[1], 0u);
	EXPECT_EQ(kept[2], 2u);
	EXPECT_EQ(kept.back(), 30u);
}

TEST(CreatureClearWay, UsuallyChoosesOneOfTheFirstThree)
{
	Rolls first {.answers = {1, 2}};
	EXPECT_EQ(creature_clear_way::Choose(10, std::ref(first)), 2u);
	EXPECT_EQ(first.ranges, (std::vector<uint32_t> {4, 3}));

	Rolls any {.answers = {0, 7}};
	EXPECT_EQ(creature_clear_way::Choose(10, std::ref(any)), 7u);
	EXPECT_EQ(any.ranges, (std::vector<uint32_t> {4, 10}));

	Rolls few {.answers = {3, 1}};
	EXPECT_EQ(creature_clear_way::Choose(2, std::ref(few)), 1u);
	EXPECT_EQ(few.ranges, (std::vector<uint32_t> {4, 2}));

	Rolls none {};
	EXPECT_FALSE(creature_clear_way::Choose(0, std::ref(none)).has_value());
	EXPECT_TRUE(none.ranges.empty());
}

TEST(CreatureClearWay, WalksOnToWithinItsHeightAndAtLeastEightMetres)
{
	EXPECT_FLOAT_EQ(creature_clear_way::WalkOnDistance(3.0f), 8.0f);
	EXPECT_FLOAT_EQ(creature_clear_way::WalkOnDistance(20.0f), 20.0f);
	const auto agenda = creature_clear_way::WalkOn({100.0f, 50.0f}, 30.0f);
	ASSERT_EQ(agenda.size(), 1u);
	EXPECT_EQ(agenda.front().kind, creature_mind::Step::Kind::Move);
	EXPECT_EQ(agenda.front().movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_EQ(agenda.front().movement.point, glm::vec2(100.0f, 50.0f));
	EXPECT_FLOAT_EQ(agenda.front().movement.maxDistance, 30.0f);
	ASSERT_TRUE(agenda.front().gaze.has_value());
	EXPECT_TRUE(agenda.front().gaze->whileGoing);
}

TEST(CreatureClearWay, ItIsNotPulledBackIntoItsAreaWhileItClearsItsWay)
{
	creature_leash::Confinement confinement {.radius = 10.0f, .distance = 30.0f};
	EXPECT_TRUE(creature_leash::WalksBack(confinement));
	confinement.clearingWay = true;
	EXPECT_FALSE(creature_leash::WalksBack(confinement));
}

TEST(CreatureClearWay, ARunAwaySeesToItsDesireOnlyFurtherThanTheRunAwayDistance)
{
	EXPECT_FALSE(creature_emote::RanFarEnough(40.0f, 50.0f));
	EXPECT_FALSE(creature_emote::RanFarEnough(50.0f, 50.0f));
	EXPECT_TRUE(creature_emote::RanFarEnough(50.5f, 50.0f));
}

TEST(CreatureClearWay, AFirstSearchThatGivesUpNotesTheThingsThatItMet)
{
	creature_route::Follower follower;
	follower.Reset({1000.0f, 1000.0f}, 2.0f);
	// A ring of things that touch each other shuts the walker in
	constexpr int k_Things = 24;
	for (int i = 0; i < k_Things; ++i)
	{
		const float angle = 2.0f * 3.14159265f * static_cast<float>(i) / static_cast<float>(k_Things);
		follower.GetObstacles().Add(static_cast<uint32_t>(100 + i),
		                            {1000.0f + 30.0f * std::sin(angle), 1000.0f + 30.0f * std::cos(angle)}, 5.0f);
	}
	follower.SetDestination({.point = {1500.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 6000.0f});
	const auto outcome = follower.Update(256, true, 0.0f);
	ASSERT_EQ(outcome, creature_route::Follower::Outcome::GaveUp);
	EXPECT_FALSE(follower.GetMet().empty());
	for (const auto thing : follower.GetMet())
	{
		EXPECT_GE(thing, 100u);
		EXPECT_LT(thing, 100u + k_Things);
	}
}

TEST(CreatureClearWay, AWalkNearAThingThatGetsNowhereForFiftyTurnsGivesUp)
{
	using creature_cast_moves::CheckStall;
	using creature_cast_moves::Stall;
	EXPECT_EQ(CheckStall(0, 0.0f, false), Stall::Note);
	EXPECT_EQ(CheckStall(50, 0.0f, false), Stall::Note);
	EXPECT_EQ(CheckStall(10, 0.0f, false), Stall::None);
	EXPECT_EQ(CheckStall(49, 0.0f, false), Stall::GiveUp);
	EXPECT_EQ(CheckStall(99, 0.005f, false), Stall::GiveUp);
	EXPECT_EQ(CheckStall(49, 0.01f, false), Stall::None);
	// A creature under the control of a script does not give up
	EXPECT_EQ(CheckStall(49, 0.0f, true), Stall::None);
}

TEST(CreatureClearWay, AWalkNearAThingWhereItCannotStandLeavesTheThingAlone)
{
	using creature_cast_moves::WalkStart;
	using creature_cast_moves::WalkStartOf;
	EXPECT_EQ(WalkStartOf(true, true), WalkStart::Walking);
	// A creature that cannot walk now asks again on the next turn
	EXPECT_EQ(WalkStartOf(false, true), WalkStart::TryAgain);
	// No place to stand at the thing stops the step, and the creature does not choose the thing again for a time
	EXPECT_EQ(WalkStartOf(false, false), WalkStart::NoWay);
}
