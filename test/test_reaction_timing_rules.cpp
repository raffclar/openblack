/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <InfoConstants.h>
#include <Magic/ReactionTimingRules.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace timing = openblack::magic::reaction_timing;
using timing::Reacting;
using timing::Rule;

namespace
{
/// A row of 100 turns to react and 200 before reacting again (50 and 80 for a creature), reaching 20 metres, nearness
/// counting half
constexpr timing::Row k_Row {.turnsToReact = 100,
                             .turnsBeforeReactingAgain = 200,
                             .creatureTurnsToReact = 50,
                             .creatureTurnsBeforeReactingAgain = 80,
                             .distance = {.maxDistance = 20.0f, .importance = 0.5f}};

timing::PredatorWatch Lion()
{
	return {.animal = true, .speed = 2.0f, .comingAt = false, .distance = 30.0f, .leastRunDistance = 20.0f};
}
} // namespace

// A villager or an animal keeps reacting up to a quarter longer at the thing (half of the half nearness counts), the
// row's turns at ten metres past the row's reach, and less beyond; far enough off it is over at once
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionTimingRules, VillagersReactLongerNearer)
{
	EXPECT_EQ(timing::StandardTurnsToReact(k_Row, Reacting::Villager, 0.0f), 125);
	EXPECT_EQ(timing::StandardTurnsToReact(k_Row, Reacting::Villager, 10.0f), 116);
	EXPECT_EQ(timing::StandardTurnsToReact(k_Row, Reacting::Villager, 30.0f), 100);
	EXPECT_EQ(timing::StandardTurnsToReact(k_Row, Reacting::Other, 40.0f), 91);
	EXPECT_LT(timing::StandardTurnsToReact(k_Row, Reacting::Villager, 400.0f), 0);
	// Waiting goes the same way, by the row's waiting turns
	EXPECT_EQ(timing::StandardTurnsBeforeReactingAgain(k_Row, Reacting::Villager, 0.0f), 250);
	EXPECT_EQ(timing::StandardTurnsBeforeReactingAgain(k_Row, Reacting::Other, 30.0f), 200);
}

// A creature keeps reacting its own turns at the thing, cut by how much nearness counts at the row's reach; it waits
// less the nearer it is
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionTimingRules, CreaturesWeighNearnessTheirOwnWay)
{
	EXPECT_EQ(timing::StandardTurnsToReact(k_Row, Reacting::Creature, 0.0f), 50);
	EXPECT_EQ(timing::StandardTurnsToReact(k_Row, Reacting::Creature, 10.0f), 37);
	EXPECT_EQ(timing::StandardTurnsToReact(k_Row, Reacting::Creature, 20.0f), 25);
	EXPECT_EQ(timing::StandardTurnsBeforeReactingAgain(k_Row, Reacting::Creature, 0.0f), 40);
	EXPECT_EQ(timing::StandardTurnsBeforeReactingAgain(k_Row, Reacting::Creature, 20.0f), 80);
}

// Each kind's rule: the creature, predator, burning thing in the hand and shield have their own; wood its own wait; the
// kinds that only move belief are never held
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionTimingRules, KindsHaveTheirRules)
{
	EXPECT_EQ(timing::ReactRule(Reaction::FleeFromObject), Rule::Standard);
	EXPECT_EQ(timing::ReactRule(Reaction::ReactToFire), Rule::Standard);
	EXPECT_EQ(timing::ReactRule(Reaction::ReactToWood), Rule::Standard);
	EXPECT_EQ(timing::AgainRule(Reaction::ReactToWood), Rule::Wood);
	EXPECT_EQ(timing::ReactRule(Reaction::ReactToCreature), Rule::Creature);
	EXPECT_EQ(timing::AgainRule(Reaction::ReactToCreature), Rule::Creature);
	EXPECT_EQ(timing::ReactRule(Reaction::FleeFromPredator), Rule::Predator);
	EXPECT_EQ(timing::AgainRule(Reaction::FleeFromPredator), Rule::Predator);
	EXPECT_EQ(timing::ReactRule(Reaction::ReactToBurningObjectInHand), Rule::BurningObject);
	EXPECT_EQ(timing::AgainRule(Reaction::ReactToBurningObjectInHand), Rule::BurningObject);
	EXPECT_EQ(timing::ReactRule(Reaction::ReactToMagicShield), Rule::Shield);
	EXPECT_EQ(timing::ReactRule(Reaction::ReactToMagicShieldStruck), Rule::Standard);
	EXPECT_EQ(timing::ReactRule(Reaction::ReactToFightWon), Rule::None);
	EXPECT_EQ(timing::AgainRule(Reaction::ReactToFightWon), Rule::Standard);
	EXPECT_TRUE(timing::ReactsToItself(Reaction::Fainting));
	EXPECT_TRUE(timing::ReactsToItself(Reaction::Confused));
	EXPECT_FALSE(timing::ReactsToItself(Reaction::ReactToDeath));
}

// A villager hiding from a predator keeps at it for ever; a predator that isn't an animal ends it; one charging at a
// villager that was looking at something ends it too
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionTimingRules, Predators)
{
	auto lion = Lion();
	EXPECT_EQ(timing::PredatorTurnsToReact(lion, Reacting::Villager, 120), 120);
	lion.hidingOrLooking = true;
	EXPECT_EQ(timing::PredatorTurnsToReact(lion, Reacting::Villager, 120), timing::k_Forever);
	EXPECT_EQ(timing::PredatorTurnsToReact(lion, Reacting::Other, 120), 120);
	lion = Lion();
	lion.animal = false;
	EXPECT_EQ(timing::PredatorTurnsToReact(lion, Reacting::Villager, 120), 0);
	lion = Lion();
	lion.lookingAtThing = true;
	lion.speed = 6.0f;
	EXPECT_EQ(timing::PredatorTurnsToReact(lion, Reacting::Villager, 120), 120);
	lion.comingAt = true;
	EXPECT_EQ(timing::PredatorTurnsToReact(lion, Reacting::Villager, 120), 0);

	// Waiting: only for a slow animal no nearer than the least running distance
	lion = Lion();
	EXPECT_EQ(timing::PredatorTurnsBeforeReactingAgain(lion, Reacting::Other, 90), 90);
	lion.distance = 10.0f;
	EXPECT_EQ(timing::PredatorTurnsBeforeReactingAgain(lion, Reacting::Other, 90), 0);
	lion = Lion();
	lion.speed = 5.5f;
	EXPECT_EQ(timing::PredatorTurnsBeforeReactingAgain(lion, Reacting::Other, 90), 0);
	lion = Lion();
	lion.lookingIfSafe = true;
	EXPECT_EQ(timing::PredatorTurnsBeforeReactingAgain(lion, Reacting::Villager, 90), 0);
	EXPECT_EQ(timing::PredatorTurnsBeforeReactingAgain(lion, Reacting::Other, 90), 90);
}

// Coming at a point: heading within about 37 degrees of the way to it, height counting
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionTimingRules, ComingAt)
{
	const glm::vec3 origin(0.0f);
	EXPECT_TRUE(timing::ComingAt(origin, {1.0f, 0.0f}, {10.0f, 0.0f, 0.0f}));
	EXPECT_FALSE(timing::ComingAt(origin, {1.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	EXPECT_TRUE(timing::ComingAt(origin, {3.0f, 0.0f}, {10.0f, 7.0f, 0.0f}));
	EXPECT_FALSE(timing::ComingAt(origin, {3.0f, 0.0f}, {10.0f, 8.0f, 0.0f}));
	EXPECT_FALSE(timing::ComingAt(origin, {0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}));
}

// A row's times and distance come from the reaction table
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ReactionTimingRules, RowFromTable)
{
	ReactionInfo info {};
	info.numGameTurnsForNormalThingsToReact = 1;
	info.numGameTurnsForNormalThingsBeforeReactingAgain = 2;
	info.numGameTurnsForCreatureToReact = 3;
	info.numGameTurnsForCreatureBeforeReactingAgain = 4;
	info.maxReactionDistance = 5.0f;
	info.howImportantIsDistance = 0.25f;
	const auto row = timing::RowOf(info);
	EXPECT_EQ(row.turnsToReact, 1u);
	EXPECT_EQ(row.turnsBeforeReactingAgain, 2u);
	EXPECT_EQ(row.creatureTurnsToReact, 3u);
	EXPECT_EQ(row.creatureTurnsBeforeReactingAgain, 4u);
	EXPECT_FLOAT_EQ(row.distance.maxDistance, 5.0f);
	EXPECT_FLOAT_EQ(row.distance.importance, 0.25f);
}
