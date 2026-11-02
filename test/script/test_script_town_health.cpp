/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptTownHealth.h"

using namespace openblack::script::town_health;

TEST(ScriptTownHealth, AddsEachBuildingsPeopleThenTheBuildingThenTheHomeless)
{
	const std::array<AbodeLives, 2> abodes {
	    AbodeLives {.people = {0.5f, 1.0f}, .life = 1.0f},
	    AbodeLives {.people = {}, .life = 0.25f},
	};
	const std::array<float, 2> homeless {0.75f, 0.125f};
	EXPECT_FLOAT_EQ(Total(abodes, homeless), 3.625f);
}

TEST(ScriptTownHealth, AnEmptyTownHasNoLife)
{
	EXPECT_FLOAT_EQ(Total({}, {}), 0.0f);
}

TEST(ScriptTownHealth, AddsInTheGamesOrderAsFloats)
{
	// The people of a building are added together before the building's total joins the town's, so a small life
	// added to a large total is lost in the same way as in the game
	const std::array<AbodeLives, 2> abodes {
	    AbodeLives {.people = {}, .life = 16777216.0f},
	    AbodeLives {.people = {1.0f, 1.0f}, .life = 0.0f},
	};
	EXPECT_EQ(Total(abodes, {}), 16777218.0f);
	const std::array<float, 2> homeless {1.0f, 1.0f};
	const std::array<AbodeLives, 1> big {AbodeLives {.people = {}, .life = 16777216.0f}};
	EXPECT_EQ(Total(big, homeless), 16777216.0f);
}
