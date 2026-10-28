/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ComputerHandFake.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;

TEST(ComputerHandTotem, KindsOfThisPartThatAreNotMadeEndTheActionBadly)
{
	FakeWorld world;
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		if (steps::OwnerOf(step) != steps::StepOwner::Totem || steps::Table().at(kind).made)
		{
			continue;
		}
		HandAction action;
		action.moveFirst = false;
		Add(action, step, {});
		EXPECT_FALSE(Process(action, world.view)) << kind;
		EXPECT_FALSE(Busy(action)) << kind;
	}
}

TEST(ComputerHandTotem, RaiseTotemSetsTheShareAtItsStartAndEndsInThreeTurns)
{
	FakeWorld world;
	constexpr auto k_Totem = static_cast<entt::entity>(9);
	HandAction action;
	action.moveFirst = false;
	Add(action, StepKind::RaiseTotem, {Argument {.object = k_Totem}, Argument {.value = 1.0f}});
	EXPECT_TRUE(Process(action, world.view));
	ASSERT_EQ(world.raised.size(), 1u);
	EXPECT_EQ(world.raised[0].first, k_Totem);
	EXPECT_EQ(world.raised[0].second, 1.0f);
	EXPECT_TRUE(Busy(action));
	EXPECT_TRUE(Process(action, world.view));
	EXPECT_TRUE(Process(action, world.view));
	EXPECT_FALSE(Busy(action));
	EXPECT_EQ(world.raised.size(), 1u);
}

TEST(ComputerHandTotem, TheHandGoesToTheTotemFirst)
{
	FakeWorld world;
	constexpr auto k_Totem = static_cast<entt::entity>(9);
	world.places[k_Totem] = {100.0f, 0.0f};
	HandAction action;
	Add(action, StepKind::RaiseTotem, {Argument {.object = k_Totem}, Argument {.value = 1.0f}});
	EXPECT_TRUE(Process(action, world.view));
	ASSERT_EQ(action.steps.size(), 2u);
	EXPECT_EQ(action.steps.front().kind, StepKind::MoveHandToObject);
	EXPECT_TRUE(world.raised.empty());
}
