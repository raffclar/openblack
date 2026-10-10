/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/ScriptPopulate.h"

using namespace openblack::ecs::script_populate;

TEST(ScriptPopulate, OnlyTheScriptsObjectTypesCanFillAContainer)
{
	EXPECT_FALSE(IsValidType(0));
	EXPECT_TRUE(IsValidType(1));
	EXPECT_TRUE(IsValidType(6));
	EXPECT_TRUE(IsValidType(41));
	EXPECT_FALSE(IsValidType(42));
	EXPECT_FALSE(IsValidType(-6));
}

TEST(ScriptPopulate, TheCountDropsTheFraction)
{
	EXPECT_EQ(CountOf(20.0f), 20u);
	EXPECT_EQ(CountOf(5.9f), 5u);
	EXPECT_EQ(CountOf(0.5f), 0u);
	// A negative quantity wraps to a great many, as the game's does
	EXPECT_EQ(CountOf(-1.0f), 0xFFFFFFFFu);
	EXPECT_EQ(CountOf(1e30f), 0u);
}

TEST(ScriptPopulate, TheSpreadGrowsATenthOfAMetreAThing)
{
	EXPECT_FLOAT_EQ(SpreadOf(0), 4.0f);
	EXPECT_FLOAT_EQ(SpreadOf(10), 5.0f);
	EXPECT_FLOAT_EQ(SpreadOf(20), 6.0f);
}

TEST(ScriptPopulate, EachThingGoesAcrossThenUpOrDownButNotAlongZ)
{
	std::vector<float> asked;
	std::vector<float> draws {1.0f, 9.0f};
	const auto rand = [&](float x) {
		asked.push_back(x);
		const float draw = draws.front();
		draws.erase(draws.begin());
		return draw;
	};
	const auto place = PlaceOf({100.0f, 50.0f, 200.0f}, 5.0f, rand);
	// Two draws below twice the spread, across first
	EXPECT_EQ(asked, (std::vector {10.0f, 10.0f}));
	EXPECT_FLOAT_EQ(place.x, 96.0f);
	EXPECT_FLOAT_EQ(place.y, 54.0f);
	EXPECT_FLOAT_EQ(place.z, 200.0f);
}
