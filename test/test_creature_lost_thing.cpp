/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <set>

#include <gtest/gtest.h>

#include "Creature/CreatureLostThing.h"

using namespace openblack;
using namespace openblack::creature_lost_thing;

namespace
{
/// A fake world: the things in the set are gone, and the things in the second set are dying villagers
struct FakeWorld
{
	std::set<uint32_t> gone;
	std::set<uint32_t> dying;

	[[nodiscard]] bool Lost(const InUse& inUse) const
	{
		return creature_lost_thing::Lost(
		    inUse, [this](uint32_t thing) { return gone.contains(thing); },
		    [this](uint32_t thing) { return gone.contains(thing) || dying.contains(thing); });
	}
};
} // namespace

TEST(CreatureLostThing, NothingGoneLosesNothing)
{
	const FakeWorld world {};
	EXPECT_FALSE(world.Lost({.planThings = {1, 2}, .stepThings = {3}, .held = 4}));
}

TEST(CreatureLostThing, AGoneThingOfThePlanIsLost)
{
	const FakeWorld world {.gone = {2}};
	EXPECT_TRUE(world.Lost({.planThings = {1, 2}}));
}

TEST(CreatureLostThing, AGoneThingOfARemainingStepIsLost)
{
	const FakeWorld world {.gone = {3}};
	EXPECT_TRUE(world.Lost({.stepThings = {3}}));
}

TEST(CreatureLostThing, AGoneThingInTheHandIsLost)
{
	const FakeWorld world {.gone = {4}};
	EXPECT_TRUE(world.Lost({.held = 4}));
}

TEST(CreatureLostThing, ADyingVillagerOfAStepIsLostButNotOfThePlan)
{
	const FakeWorld world {.dying = {5}};
	EXPECT_TRUE(world.Lost({.stepThings = {5}}));
	EXPECT_FALSE(world.Lost({.planThings = {5}}));
	EXPECT_FALSE(world.Lost({.held = 5}));
}

TEST(CreatureLostThing, ItsOwnMealIsNeverLost)
{
	const FakeWorld world {.gone = {6}};
	EXPECT_FALSE(world.Lost({.planThings = {6}, .stepThings = {6}, .held = 6, .usedByItself = {6}}));
	EXPECT_TRUE(world.Lost({.planThings = {6}, .usedByItself = {7}}));
}

TEST(CreatureLostThing, PlanThingsAreWhatItIsAboutActsOnAndUses)
{
	creature_planner::Plan plan {};
	plan.about = 1;
	plan.object = 2;
	plan.instrument = 3;
	auto things = PlanThings(plan);
	std::ranges::sort(things);
	EXPECT_EQ(things, (std::vector<uint32_t> {1, 2, 3}));
}

TEST(CreatureLostThing, StepThingsStartAtTheCurrentStep)
{
	std::vector<creature_mind::Step> agenda(3);
	agenda[0].object = 1;
	agenda[1].movement.object = 2;
	agenda[2].order.object = 3;
	agenda[2].cast.object = 4;
	EXPECT_EQ(StepThings(agenda, 1), (std::vector<uint32_t> {2, 3, 4}));
	EXPECT_TRUE(StepThings(agenda, 3).empty());
}

TEST(CreatureLostThing, ATreeThrownIntoAStoreIsNotLost)
{
	constexpr uint32_t k_Self = 9;
	constexpr uint32_t k_Store = 2;
	constexpr uint32_t k_Tree = 3;
	creature_planner::Plan plan {};
	plan.about = 1;
	plan.object = k_Store;
	plan.instrument = k_Tree;
	// Other orders keep the thing to use
	StopUseBeforeThrowIntoStore(plan, {.kind = creature_mind::ObjectOrder::Kind::Throw, .object = k_Store}, k_Self);
	EXPECT_EQ(plan.instrument, std::optional<uint32_t>(k_Tree));
	// The throw into the store makes the creature itself the thing to use
	StopUseBeforeThrowIntoStore(plan, {.kind = creature_mind::ObjectOrder::Kind::ThrowInStore, .object = k_Store}, k_Self);
	EXPECT_EQ(plan.instrument, std::optional<uint32_t>(k_Self));
	// The store takes in the tree, and the creature does not lose a thing
	const FakeWorld world {.gone = {k_Tree}};
	EXPECT_FALSE(world.Lost({.planThings = PlanThings(plan), .stepThings = {k_Store}}));
}
