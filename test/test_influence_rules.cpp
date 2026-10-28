/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/InfluenceRules.h"

using openblack::influence_rules::CanHaveInfluence;

TEST(InfluenceRules, APlayerWithNoTempleHasNoInfluenceInANetworkGame)
{
	EXPECT_FALSE(CanHaveInfluence(true, false));
	EXPECT_TRUE(CanHaveInfluence(true, true));
}

TEST(InfluenceRules, EveryPlayerCanHaveInfluenceInAGameAlone)
{
	EXPECT_TRUE(CanHaveInfluence(false, false));
	EXPECT_TRUE(CanHaveInfluence(false, true));
}
