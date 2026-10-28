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

TEST(ComputerHandCreature, KindsOfThisPartThatAreNotMadeEndTheActionBadly)
{
	FakeWorld world;
	for (size_t kind = 0; kind < static_cast<size_t>(StepKind::_Count); ++kind)
	{
		const auto step = static_cast<StepKind>(kind);
		if (steps::OwnerOf(step) != steps::StepOwner::Creature || steps::Table().at(kind).made)
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
