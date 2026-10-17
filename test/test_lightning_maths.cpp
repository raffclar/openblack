/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Particles/LightningMaths.h"

using namespace openblack::particles::maths;

TEST(LightningMaths, SearchesASquareOfCellsTwiceItsRadiusAcross)
{
	EXPECT_EQ(StrikeSearchCells(60.0f), 144u);
	EXPECT_EQ(StrikeSearchCells(100.0f), 400u);
	EXPECT_EQ(StrikeSearchCells(140.0f), 784u);
	EXPECT_EQ(StrikeSearchCells(5.0f), 4u);
	// A radius of zero, or a little less than zero, searches no cells; the square of a whole cell less than zero is one
	EXPECT_EQ(StrikeSearchCells(0.0f), 0u);
	EXPECT_EQ(StrikeSearchCells(-5.0f), 0u);
	EXPECT_EQ(StrikeSearchCells(-10.0f), 4u);
}

TEST(LightningMaths, ABoltFromACloudSearchesASquareItsRadiusAcross)
{
	EXPECT_EQ(CloudSearchCells(60.0f), 36u);
	EXPECT_EQ(CloudSearchCells(25.0f), 9u);
	EXPECT_EQ(CloudSearchCells(5.0f), 1u);
	EXPECT_EQ(CloudSearchCells(0.0f), 0u);
	EXPECT_EQ(CloudSearchCells(-9.0f), 0u);
}

TEST(LightningMaths, KeepsTwoForksForEachTargetItFound)
{
	EXPECT_EQ(ForkCount(0), 0u);
	EXPECT_EQ(ForkCount(3), 6u);
	EXPECT_EQ(ForkCount(28), 56u);
}

TEST(LightningMaths, ACreatureTakesEveryFork)
{
	const std::vector<bool> none {false, false, false};
	EXPECT_EQ(PreferCreatures(none), (std::vector<size_t> {0, 1, 2}));
	const std::vector<bool> one {false, true, false, false};
	EXPECT_EQ(PreferCreatures(one), (std::vector<size_t> {1, 1, 1, 1}));
	// Two creatures share the forks in turn
	const std::vector<bool> two {true, false, false, true, false};
	EXPECT_EQ(PreferCreatures(two), (std::vector<size_t> {0, 3, 0, 3, 0}));
}

TEST(LightningMaths, BoltsPointingTheSameWayClash)
{
	const float north = std::numbers::pi_v<float> / 2.0f;
	const BoltPose older {.origin = {-15.0f, 10.0f, 0.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	const BoltPose newer {.origin = {5.0f, 10.0f, 0.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	const auto meeting = ClashPoint(newer, older, 60.0f);
	ASSERT_TRUE(meeting.has_value());
	// Between the older bolt's middle and the hands, in front of the newer one
	EXPECT_LT(meeting->z, 40.0f);
	EXPECT_GT(meeting->z, 0.0f);
	// Out of reach, or pointing apart, they don't
	EXPECT_FALSE(ClashPoint(newer, older, 5.0f).has_value());
	BoltPose away = newer;
	away.heading = -north;
	EXPECT_FALSE(ClashPoint(away, older, 60.0f).has_value());
}

TEST(LightningMaths, AClashMeetingFarToTheSideIsOutsideTheWidestAngle)
{
	const float north = std::numbers::pi_v<float> / 2.0f;
	const BoltPose older {.origin = {-15.0f, 10.0f, 0.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	// The meeting is in reach, but about 72 degrees off the newer bolt's heading. Without a unit way, its length would
	// make it pass.
	const BoltPose newer {.origin = {60.0f, 10.0f, -10.0f}, .centroid = {0.0f, 2.0f, 40.0f}, .heading = north};
	EXPECT_FALSE(ClashPoint(newer, older, 100.0f).has_value());
}

TEST(LightningMaths, ArcsRunFromEndToEnd)
{
	const ArcEnds ends {.from = {0.0f, 0.0f, 0.0f},
	                    .to = {4.0f, 0.0f, 0.0f},
	                    .fromNormal = {0.0f, 1.0f, 0.0f},
	                    .toNormal = {0.0f, -1.0f, 0.0f}};
	const auto fromTangent = ArcTangent(ends.fromNormal, glm::vec3(0.0f), 0.2f, 4.0f, 3.0f);
	EXPECT_EQ(fromTangent, glm::vec3(0.0f, 12.0f, 0.0f));
	const std::vector<glm::vec3> still(8, glm::vec3(0.0f));
	const auto joints = ArcJoints(ends, fromTangent, ArcTangent(ends.toNormal, glm::vec3(0.0f), 0.2f, 4.0f, 3.0f), still, 0.1f);
	ASSERT_EQ(joints.size(), 8u);
	EXPECT_NEAR(glm::distance(joints.front(), ends.from), 0.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(joints.back(), ends.to), 0.0f, 1e-4f);
	// It leaves upwards along the first normal
	EXPECT_GT(joints[1].y, 0.0f);
	// The jitter moves every joint by its share of the arc's length
	const std::vector<glm::vec3> jitters(8, glm::vec3(1.0f, 0.0f, 0.0f));
	const auto jittered = ArcJoints(ends, fromTangent, glm::vec3(0.0f), jitters, 0.1f);
	EXPECT_NEAR(jittered.front().x, 0.4f, 1e-5f);
}

TEST(LightningMaths, ArcMathsRoundsEachStepToAFloatInTheGamesOrder)
{
	// The tangent's scale is the length times the scale, before it multiplies the nudged normal
	EXPECT_EQ(ArcTangent({0.246f, 0.484f, 0.59f}, {0.885f, 0.48f, 0.845f}, 0.2f, 1.261f, 3.0531f),
	          glm::vec3(1.6285329f, 2.2329764f, 2.9221191f));
	// Each term of the cubic is found on its own and the terms are added from the highest power down, the start last
	const ArcEnds ends {.from = {1.5f, 0.25f, -2.0f}, .to = {4.75f, 1.0f, 3.5f}};
	const std::vector<glm::vec3> still(8, glm::vec3(0.0f));
	const auto joints = ArcJoints(ends, {0.3f, 2.1f, -0.7f}, {-1.1f, 0.4f, 0.9f}, still, 0.1f);
	EXPECT_EQ(joints[2], glm::vec3(2.2521868f, 0.6814869f, -1.0641398f));
	EXPECT_EQ(joints[6], glm::vec3(4.6906705f, 0.953207f, 3.0886292f));
}
