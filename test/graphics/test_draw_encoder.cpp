/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/DrawEncoder.h"

using namespace openblack::graphics;

namespace
{

TEST(BoundStages, BindingsStayUntilADiscardDropsThem)
{
	BoundStages stages;
	stages.Set(0);
	stages.Set(5);
	EXPECT_TRUE(stages.IsSet(0));
	EXPECT_TRUE(stages.IsSet(5));
	EXPECT_FALSE(stages.IsSet(1));
	// A discard that keeps the bindings keeps the stages
	stages.Discarded(0);
	EXPECT_TRUE(stages.IsSet(5));
	stages.Discarded(BoundStages::k_DiscardBindings);
	EXPECT_FALSE(stages.IsSet(0));
	EXPECT_FALSE(stages.IsSet(5));
	stages.Set(3);
	stages.Clear();
	EXPECT_FALSE(stages.IsSet(3));
}

TEST(BoundStages, AStageOutOfRangeIsNeverSet)
{
	BoundStages stages;
	stages.Set(255);
	EXPECT_FALSE(stages.IsSet(255));
}

} // namespace
