/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Magic/VortexPullRules.h"

using namespace openblack;

TEST(VortexPull, TheWholeReachOnlyEveryThirdTurn)
{
	EXPECT_EQ(vortex::PullReach(0, 50.0f), 50.0f);
	EXPECT_EQ(vortex::PullReach(1, 50.0f), 25.0f);
	EXPECT_EQ(vortex::PullReach(2, 50.0f), 25.0f);
	EXPECT_EQ(vortex::PullReach(3, 50.0f), 50.0f);
}

TEST(VortexPull, TheSearchSpiralsOutToTheReach)
{
	// A vortex in the middle of a cell, well inside the map
	const auto centre = map_coords::FromMetres({1005.0f, 2005.0f});
	const auto cells = vortex::CellsSearched(centre, 25.0f);
	ASSERT_GE(cells.size(), 9u);
	// Its own cell first, then the game's spiral: -x, -z, +x twice, +z twice, ...
	EXPECT_EQ(cells.at(0), glm::ivec2(100, 200));
	EXPECT_EQ(cells.at(1), glm::ivec2(99, 200));
	EXPECT_EQ(cells.at(2), glm::ivec2(99, 199));
	EXPECT_EQ(cells.at(3), glm::ivec2(100, 199));
	EXPECT_EQ(cells.at(4), glm::ivec2(101, 199));
	EXPECT_EQ(cells.at(5), glm::ivec2(101, 200));
	// It stops at the first cell beyond the reach: 2 cells across is 20 m, the corner (2, 2) is beyond 25 m, and the
	// ring of 3 is never reached
	for (const auto& cell : cells)
	{
		EXPECT_LE(std::max(std::abs(cell.x - 100), std::abs(cell.y - 200)), 2);
	}
	// The whole reach looks further
	EXPECT_GT(vortex::CellsSearched(centre, 50.0f).size(), cells.size());
}

TEST(VortexPull, CellsOffTheMapArePassedOver)
{
	// In the map's corner the spiral's cells below 0 are skipped, not counted as beyond the reach
	const auto cells = vortex::CellsSearched(map_coords::FromMetres({5.0f, 5.0f}), 25.0f);
	ASSERT_FALSE(cells.empty());
	EXPECT_EQ(cells.front(), glm::ivec2(0, 0));
	for (const auto& cell : cells)
	{
		EXPECT_TRUE(map_coords::InBounds(cell));
	}
	EXPECT_GT(cells.size(), 1u);
}

TEST(VortexPull, OnlyACreatureLetThroughOnTheLeashIsTaken)
{
	EXPECT_TRUE(vortex::CreatureMayBeTaken(true, true, false));
	EXPECT_FALSE(vortex::CreatureMayBeTaken(true, false, false));
	EXPECT_FALSE(vortex::CreatureMayBeTaken(false, true, false));
	// Or one a script moves
	EXPECT_TRUE(vortex::CreatureMayBeTaken(false, false, true));
}

TEST(VortexPull, ACreatureIsTakenWithinSevenTenthsOfTheModel)
{
	// The vortex's cylinder reaches 20 along x and z: 14
	EXPECT_FLOAT_EQ(vortex::CreatureTakenDistance(20.0f, 20.0f, 1.0f), 14.0f);
	EXPECT_FLOAT_EQ(vortex::CreatureTakenDistance(10.0f, 30.0f, 2.0f), 42.0f);
}

TEST(VortexPull, OnlyAnOpenVortexOutOfTheLandTakesWhatStrikesIt)
{
	EXPECT_TRUE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::Active, true, true));
	// Not while it fades in or out
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::FadeIn, true, true));
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::FadeOut, true, true));
	// The way into a land and the volcano's mouth never do
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::Out, VortexStateType::Active, true, true));
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::Volcano, VortexStateType::Active, true, true));
	// Only a thing still flying, that it would draw in from the land
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::Active, false, true));
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::Active, true, false));
}
