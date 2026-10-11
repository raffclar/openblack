/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraLimits.h"
#include "Camera/CameraZones.h"

using namespace openblack;
using namespace openblack::camera_limits;
using camera_zones::Exclusion;

namespace
{
/// Flat land at a height
camera_limits::Ground FlatLand(float height)
{
	return {
	    .height = [height](glm::vec2) { return height; },
	    .normal = [](glm::vec2) { return glm::vec3(0.0f, 1.0f, 0.0f); },
	};
}

float AcrossFromCentre(const glm::vec3& point)
{
	return std::hypot(point.x - k_AllowedAreaCentre.x, point.z - k_AllowedAreaCentre.y);
}
} // namespace

TEST(CameraLimits, TheCameraIsBroughtBackToTheRimOfTheDiscRoundTheIsland)
{
	// Inside, or on the rim, it stays
	EXPECT_FALSE(KeepInsideAllowedArea({2560.0f, 900.0f, 2560.0f}).has_value());
	EXPECT_FALSE(KeepInsideAllowedArea({2560.0f + 5120.0f, 50.0f, 2560.0f}).has_value());

	// Outside, straight back across the ground to the rim, at the same height
	const auto back = KeepInsideAllowedArea({2560.0f + 6000.0f, 120.0f, 2560.0f + 8000.0f});
	ASSERT_TRUE(back.has_value());
	EXPECT_NEAR(AcrossFromCentre(*back), 5120.0f, 0.01f);
	EXPECT_FLOAT_EQ(back->y, 120.0f);
	EXPECT_NEAR((back->x - 2560.0f) / (back->z - 2560.0f), 0.75f, 1e-5f);

	// The disc is round, not square: a corner of the map is outside
	EXPECT_TRUE(KeepInsideAllowedArea({-1500.0f, 0.0f, -1500.0f}).has_value());
}

TEST(CameraLimits, TheCameraMovesNoFurtherInAFrameThanItsLimit)
{
	// At least 300 a frame, or 1.6 times the distance to what it looks at each second
	EXPECT_FLOAT_EQ(FrameMoveLimit(100.0f, 1.0f / 30.0f), 300.0f);
	EXPECT_FLOAT_EQ(FrameMoveLimit(10000.0f, 0.05f), 800.0f);

	const glm::vec3 start {1000.0f, 50.0f, 1000.0f};
	// A move of 370, as one far drag asks for, is cut to 300 along the way it was going
	const auto cut = LimitFrameMove(start, start + glm::vec3(296.0f, 0.0f, 222.0f), 300.0f);
	ASSERT_TRUE(cut.has_value());
	EXPECT_NEAR(glm::distance(*cut, start), 300.0f, 1e-3f);
	EXPECT_NEAR(cut->x - start.x, 240.0f, 1e-3f);
	EXPECT_NEAR(cut->z - start.z, 180.0f, 1e-3f);
	// A move within it is kept
	EXPECT_FALSE(LimitFrameMove(start, start + glm::vec3(0.0f, 299.0f, 0.0f), 300.0f).has_value());
}

TEST(CameraLimits, OnlyExclusionsNearTheCameraAtTheStartOfTheFrameCount)
{
	const std::vector<Exclusion> exclusions {
	    {.position = {100.0f, 0.0f, 0.0f}, .radius = 20.0f, .height = 30.0f, .kind = Exclusion::Kind::Dome},
	    {.position = {0.0f, 0.0f, 100.0f}, .radius = 20.0f, .height = 0.0f, .kind = Exclusion::Kind::Cylinder},
	};
	// Within twice the clearance of a dome's edge, and no higher than its height and twice the clearance
	EXPECT_EQ(NearbyExclusions(exclusions, {74.5f, 35.0f, 0.0f}).size(), 1u);
	EXPECT_EQ(NearbyExclusions(exclusions, {73.5f, 35.0f, 0.0f}).size(), 0u);
	EXPECT_EQ(NearbyExclusions(exclusions, {100.0f, 36.5f, 0.0f}).size(), 0u);
	// A cylinder counts at any height
	EXPECT_EQ(NearbyExclusions(exclusions, {0.0f, 9000.0f, 75.0f}).size(), 1u);
}

TEST(CameraLimits, ACylinderPushesTheCameraOutAcrossTheGroundToItsClearance)
{
	const std::vector<Exclusion> nearby {
	    {.position = {0.0f, 0.0f, 0.0f}, .radius = 20.0f, .height = 0.0f, .kind = Exclusion::Kind::Cylinder},
	    {.position = {500.0f, 0.0f, 0.0f}, .radius = 20.0f, .height = 20.0f, .kind = Exclusion::Kind::Dome},
	};
	const auto pushed = PushOutOfCylinders(nearby, {10.0f, 70.0f, 0.0f});
	ASSERT_TRUE(pushed.has_value());
	EXPECT_FLOAT_EQ(pushed->x, 23.0f);
	EXPECT_FLOAT_EQ(pushed->y, 70.0f);
	EXPECT_FLOAT_EQ(pushed->z, 0.0f);
	// Clear of it, or in a dome, it isn't pushed
	EXPECT_FALSE(PushOutOfCylinders(nearby, {23.5f, 70.0f, 0.0f}).has_value());
	EXPECT_FALSE(PushOutOfCylinders(nearby, {500.0f, 10.0f, 0.0f}).has_value());
}

TEST(CameraLimits, ADomesRoofIsGroundToTheCamera)
{
	const std::vector<Exclusion> nearby {
	    {.position = {0.0f, 10.0f, 0.0f}, .radius = 50.0f, .height = 40.0f, .kind = Exclusion::Kind::Dome},
	};
	const auto land = FlatLand(12.0f);
	// Its roof is half an ellipse: 40 above its base at the middle, down to its base at the edge
	EXPECT_FLOAT_EQ(SurfaceHeight(land, nearby, true, {0.0f, 0.0f}), 50.0f);
	EXPECT_FLOAT_EQ(SurfaceHeight(land, nearby, true, {30.0f, 0.0f}), 10.0f + 0.8f * 40.0f);
	// Where the land is higher, the land; outside the dome, or with the exclusions off, the land
	EXPECT_FLOAT_EQ(SurfaceHeight(land, nearby, true, {49.99f, 0.0f}), 12.0f);
	EXPECT_FLOAT_EQ(SurfaceHeight(land, nearby, true, {60.0f, 0.0f}), 12.0f);
	EXPECT_FLOAT_EQ(SurfaceHeight(land, nearby, false, {0.0f, 0.0f}), 12.0f);
}

TEST(CameraLimits, TheCameraIsPushedOffTheLandByAtMostHalfItsClearanceAPoint)
{
	const auto land = FlatLand(0.0f);
	// Well clear of the land, nothing moves
	{
		const auto push = PushOffTheGround(land, {}, true, {0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 50.0f});
		EXPECT_FALSE(push.pushed);
		EXPECT_EQ(push.origin, glm::vec3(0.0f, 10.0f, 0.0f));
	}
	// Looking level 2 above the land: the point straight below is 1 under it and pushes the camera up 1, which takes
	// every other point clear
	{
		const auto push = PushOffTheGround(land, {}, true, {0.0f, 2.0f, 0.0f}, {0.0f, 2.0f, 50.0f});
		EXPECT_TRUE(push.pushed);
		EXPECT_NEAR(push.origin.y, 3.0f, 1e-4f);
		EXPECT_FLOAT_EQ(push.origin.x, 0.0f);
		// Straight up has no part backwards, so what it looks at stays
		EXPECT_FALSE(push.focusMoved);
		EXPECT_EQ(push.focus, glm::vec3(0.0f, 2.0f, 50.0f));
	}
	// Deep in the land, each point pushes at most 1.5: from 20 under it comes up only until the top point is out,
	// 3 sqrt(2) / 2 under
	{
		const auto push = PushOffTheGround(land, {}, true, {0.0f, -20.0f, 0.0f}, {0.0f, -20.0f, 50.0f});
		EXPECT_NEAR(push.origin.y, -2.1213203f, 1e-3f);
	}
}

TEST(CameraLimits, APushAwayFromWhatTheCameraLooksAtMovesThatToo)
{
	// Looking straight down onto land 1 below it: the ball's points below the camera are in the land, and their pushes
	// up are away from what it looks at
	const auto land = FlatLand(0.0f);
	const auto push = PushOffTheGround(land, {}, true, {0.0f, 1.0f, 0.0f}, {0.0f, -100.0f, 0.001f});
	EXPECT_TRUE(push.pushed);
	EXPECT_TRUE(push.focusMoved);
	EXPECT_GT(push.focus.y, -100.0f);
}

TEST(CameraLimits, ADomesRoofPushesTheCameraOutFromItsMiddle)
{
	const std::vector<Exclusion> nearby {
	    {.position = {0.0f, 0.0f, 0.0f}, .radius = 50.0f, .height = 50.0f, .kind = Exclusion::Kind::Dome},
	};
	const auto land = FlatLand(0.0f);
	// Just inside the roof, to the side of the middle: pushed outwards, away from the middle
	const glm::vec3 origin {30.0f, 39.0f, 0.0f};
	const auto push = PushOffTheGround(land, nearby, true, origin, origin + glm::vec3(0.0f, 0.0f, 50.0f));
	EXPECT_TRUE(push.pushed);
	EXPECT_GT(glm::length(push.origin), glm::length(origin));
	EXPECT_GT(push.origin.x, origin.x);
	// With the exclusions off, the dome is not felt for
	const auto off = PushOffTheGround(land, nearby, false, origin, origin + glm::vec3(0.0f, 0.0f, 50.0f));
	EXPECT_FALSE(off.pushed);
}
