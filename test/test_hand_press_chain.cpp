/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_press::Choose, the branch the hand's action press takes: each fact alone gives its branch, two facts together give
// the branch the hand tries first, and the creature's branch is false without a creature, so the hand chooses as it did
// before the creature came

#include <cstddef>

#include <array>
#include <utility>

#include <gtest/gtest.h>

#include "ECS/HandPressChain.h"

using openblack::ecs::hand_press::Branch;
using openblack::ecs::hand_press::Choose;
using openblack::ecs::hand_press::EmptyHandOverNothing;
using openblack::ecs::hand_press::Facts;

namespace
{
/// The facts that make each branch hold on its own, in the order the hand tries the branches
const std::array<std::pair<Branch, Facts>, 8> k_Alone {{
    {Branch::ScreenObject, Facts {.screenPress = true}},
    {Branch::Field, Facts {.hovered = true, .hoveredIsField = true}},
    {Branch::Hovered, Facts {.hovered = true}},
    {Branch::TapOnly, Facts {.tapOnlyCursorObject = true}},
    {Branch::Creature, Facts {.creatureTakesPress = true}},
    {Branch::FishFarm, Facts {.fishFarmInInfluence = true}},
    {Branch::Seed, Facts {.held = true, .holdingSeed = true}},
    {Branch::Held, Facts {.held = true}},
}};

/// Every fact that is true in either
Facts Both(const Facts& a, const Facts& b)
{
	return Facts {
	    .screenPress = a.screenPress || b.screenPress,
	    .held = a.held || b.held,
	    .hovered = a.hovered || b.hovered,
	    .hoveredIsField = a.hoveredIsField || b.hoveredIsField,
	    .gripping = a.gripping || b.gripping,
	    .tapOnlyCursorObject = a.tapOnlyCursorObject || b.tapOnlyCursorObject,
	    .cursorIsCreature = a.cursorIsCreature || b.cursorIsCreature,
	    .creatureTakesPress = a.creatureTakesPress || b.creatureTakesPress,
	    .fishFarmInInfluence = a.fishFarmInInfluence || b.fishFarmInInfluence,
	    .holdingSeed = a.holdingSeed || b.holdingSeed,
	    .pickPressHeld = a.pickPressHeld || b.pickPressHeld,
	    .creatureLockBusy = a.creatureLockBusy || b.creatureLockBusy,
	};
}
} // namespace

TEST(HandPressChain, NothingTrueIsNoBranch)
{
	EXPECT_EQ(Choose(Facts {}), Branch::None);
	EXPECT_TRUE(EmptyHandOverNothing(Facts {}));
}

TEST(HandPressChain, EachBranchAloneHolds)
{
	for (const auto& [branch, facts] : k_Alone)
	{
		EXPECT_EQ(Choose(facts), branch) << static_cast<int>(branch);
	}
}

TEST(HandPressChain, TheEarlierBranchWins)
{
	// any two branches' facts together: the branch the hand tries first. Only a full hand rules out what an empty hand
	// needs: then the branch of the full hand wins, unless the screen object came first
	for (size_t i = 0; i < k_Alone.size(); ++i)
	{
		for (size_t j = i + 1; j < k_Alone.size(); ++j)
		{
			const bool fullHandAgainstEmpty = k_Alone[j].second.held && !k_Alone[i].second.held && i != 0;
			const auto expected = fullHandAgainstEmpty ? k_Alone[j].first : k_Alone[i].first;
			EXPECT_EQ(Choose(Both(k_Alone[i].second, k_Alone[j].second)), expected) << i << " " << j;
		}
	}
}

TEST(HandPressChain, TheCreatureComesAfterTheTapOnlyObjectAndBeforeTheFish)
{
	EXPECT_EQ(Choose({.screenPress = true, .creatureTakesPress = true}), Branch::ScreenObject);
	EXPECT_EQ(Choose({.tapOnlyCursorObject = true, .creatureTakesPress = true}), Branch::TapOnly);
	EXPECT_EQ(Choose({.creatureTakesPress = true, .fishFarmInInfluence = true}), Branch::Creature);
	EXPECT_EQ(Choose({.hovered = true, .hoveredIsField = true}), Branch::Field);
}

TEST(HandPressChain, ACreatureTheHandMayNotHoldKeepsTheFishOut)
{
	// another player's creature over a fish farm: the press collides with the creature, so the fish are not scooped
	EXPECT_EQ(Choose({.cursorIsCreature = true, .fishFarmInInfluence = true}), Branch::None);
	EXPECT_EQ(Choose({.cursorIsCreature = true, .creatureTakesPress = true, .fishFarmInInfluence = true}), Branch::Creature);
	// what does not need an empty hand over nothing is as before
	EXPECT_EQ(Choose({.held = true, .cursorIsCreature = true, .holdingSeed = true}), Branch::Seed);
	EXPECT_EQ(Choose({.held = true, .cursorIsCreature = true, .fishFarmInInfluence = true}), Branch::Held);
	EXPECT_EQ(Choose({.hovered = true, .cursorIsCreature = true}), Branch::Hovered);
	EXPECT_EQ(Choose({.tapOnlyCursorObject = true, .cursorIsCreature = true}), Branch::TapOnly);
}

TEST(HandPressChain, TheCreatureNeedsAnEmptyHandOverNothing)
{
	EXPECT_EQ(Choose({.held = true, .creatureTakesPress = true}), Branch::Held);
	EXPECT_EQ(Choose({.held = true, .creatureTakesPress = true, .pickPressHeld = true}), Branch::None);
	EXPECT_EQ(Choose({.gripping = true, .creatureTakesPress = true}), Branch::None);
	EXPECT_EQ(Choose({.hovered = true, .creatureTakesPress = true}), Branch::Hovered);
}

TEST(HandPressChain, AHeldCreatureTakesNoOtherPress)
{
	for (const auto& [branch, facts] : k_Alone)
	{
		auto busy = facts;
		busy.creatureLockBusy = true;
		EXPECT_EQ(Choose(busy), Branch::None) << static_cast<int>(branch);
		EXPECT_FALSE(EmptyHandOverNothing(busy));
	}
}

TEST(HandPressChain, WithoutACreatureTheHandChoosesAsBefore)
{
	// the hand's earlier chain, one case for each of its branches, with no creature: the same branch
	EXPECT_EQ(Choose({.screenPress = true}), Branch::ScreenObject);
	EXPECT_EQ(Choose({.hovered = true, .hoveredIsField = true}), Branch::Field);
	EXPECT_EQ(Choose({.hovered = true, .hoveredIsField = false}), Branch::Hovered);
	EXPECT_EQ(Choose({.tapOnlyCursorObject = true, .fishFarmInInfluence = true}), Branch::TapOnly);
	EXPECT_EQ(Choose({.fishFarmInInfluence = true}), Branch::FishFarm);
	EXPECT_EQ(Choose({.gripping = true, .fishFarmInInfluence = true}), Branch::None);
	EXPECT_EQ(Choose({.held = true, .holdingSeed = true, .pickPressHeld = true}), Branch::Seed);
	EXPECT_EQ(Choose({.held = true}), Branch::Held);
	EXPECT_EQ(Choose({.held = true, .pickPressHeld = true}), Branch::None);
	EXPECT_EQ(Choose({.gripping = true}), Branch::None);
}
