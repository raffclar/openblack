/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "ECS/BuildingConstruction.h"

using namespace openblack::building_construction;

TEST(BuildingConstruction, SetBuiltKeepsWhatIsBetweenNoneAndAll)
{
	const auto progress = SetBuilt(0.375f);
	EXPECT_FLOAT_EQ(progress.built, 0.375f);
	EXPECT_FALSE(progress.finished);
}

TEST(BuildingConstruction, SetBuiltNeverGoesBelowNone)
{
	const auto progress = SetBuilt(-0.5f);
	EXPECT_FLOAT_EQ(progress.built, 0.0f);
	EXPECT_FALSE(progress.finished);
}

TEST(BuildingConstruction, SetBuiltFinishesAtAllOrMore)
{
	for (const float built : {1.0f, 1.5f})
	{
		const auto progress = SetBuilt(built);
		EXPECT_FLOAT_EQ(progress.built, 1.0f);
		EXPECT_TRUE(progress.finished);
	}
	EXPECT_FALSE(SetBuilt(0.9f).finished);
}

TEST(BuildingConstruction, BuildByAddsAndFinishes)
{
	EXPECT_FLOAT_EQ(BuildBy(0.25f, 0.5f).built, 0.75f);
	EXPECT_FALSE(BuildBy(0.25f, 0.5f).finished);
	EXPECT_TRUE(BuildBy(0.75f, 0.25f).finished);
	EXPECT_FLOAT_EQ(BuildBy(0.1f, -0.5f).built, 0.0f);
}

TEST(BuildingConstruction, ScriptDesireIsFiveTimesOnTheSite)
{
	EXPECT_FLOAT_EQ(SiteDesire(1.0f), 5.0f);
	EXPECT_FLOAT_EQ(SiteDesire(0.3f), 1.5f);
}

TEST(BuildingConstruction, PlannedAtTheScriptsPlace)
{
	const std::array planned {PlannedCandidate {.position = {100.0f, 100.0f}, .reach = 10.0f},
	                          PlannedCandidate {.position = {1915.05f, 2508.89f}, .reach = 20.0f}};
	const auto found = PlannedAt(planned, {1915.05f, 2508.89f});
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(*found, 1u);
}

TEST(BuildingConstruction, PlannedAtTakesInTheModelsReach)
{
	// 25 away, less a reach of 20 and the search radius of 1, is 4: further than the radius
	const std::array far {PlannedCandidate {.position = {25.0f, 0.0f}, .reach = 20.0f}};
	EXPECT_FALSE(PlannedAt(far, {0.0f, 0.0f}).has_value());
	// 22 away is 1: just within it
	const std::array near {PlannedCandidate {.position = {22.0f, 0.0f}, .reach = 20.0f}};
	EXPECT_TRUE(PlannedAt(near, {0.0f, 0.0f}).has_value());
}

TEST(BuildingConstruction, PlannedAtTakesTheNearestAndTheLaterOnATie)
{
	const std::array planned {PlannedCandidate {.position = {5.0f, 0.0f}, .reach = 10.0f},
	                          PlannedCandidate {.position = {2.0f, 0.0f}, .reach = 10.0f},
	                          PlannedCandidate {.position = {0.0f, 2.0f}, .reach = 10.0f}};
	const auto found = PlannedAt(planned, {0.0f, 0.0f});
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(*found, 2u);
}

TEST(BuildingConstruction, NothingPlannedNothingFound)
{
	EXPECT_FALSE(PlannedAt({}, {0.0f, 0.0f}).has_value());
}

TEST(BuildingConstruction, ModelReachIsTheLargerHalfWidthScaled)
{
	EXPECT_FLOAT_EQ(ModelReach({3.0f, 5.0f}, 2.0f), 10.0f);
	EXPECT_FLOAT_EQ(ModelReach({7.0f, 5.0f}, 1.0f), 7.0f);
}
