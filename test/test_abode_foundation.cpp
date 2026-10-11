/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "ECS/AbodeFoundation.h"

namespace abode_foundation = openblack::ecs::abode_foundation;

namespace
{
/// Land sloping down towards -x by `drop` per unit, flat along z
struct FakeSlope
{
	float drop;
	[[nodiscard]] float operator()(glm::vec2 point) const { return 50.0f + point.x * drop; }
};

const glm::mat3 k_NoTurn {1.0f};
} // namespace

TEST(AbodeFoundation, boxAlwaysReachesTheOrigin)
{
	const auto box = abode_foundation::GroundBoxOf({1.0f, 0.0f, 2.0f}, {4.0f, 9.0f, 6.0f});
	EXPECT_FLOAT_EQ(box.minima.x, 0.0f);
	EXPECT_FLOAT_EQ(box.minima.y, 0.0f);
	EXPECT_FLOAT_EQ(box.maxima.x, 4.0f);
	EXPECT_FLOAT_EQ(box.maxima.y, 6.0f);
}

TEST(AbodeFoundation, radiusIsTheLargerHalfSideAtSize)
{
	const auto box = abode_foundation::GroundBoxOf({-3.0f, 0.0f, -5.0f}, {3.0f, 9.0f, 5.0f});
	EXPECT_FLOAT_EQ(abode_foundation::GroundRadius(box, 1.0f), 5.0f);
	EXPECT_FLOAT_EQ(abode_foundation::GroundRadius(box, 2.0f), 10.0f);
}

TEST(AbodeFoundation, cornersTurnAndSizeWithTheBuilding)
{
	const auto box = abode_foundation::GroundBoxOf({-1.0f, 0.0f, -2.0f}, {1.0f, 3.0f, 2.0f});
	const glm::mat3 quarterTurn(glm::eulerAngleY(std::numbers::pi_v<float> / 2.0f));
	const auto corners = abode_foundation::GroundCorners(box, quarterTurn, 2.0f, {100.0f, 200.0f});
	// A quarter turn about up takes the model's z onto world x: the long side now lies along x
	float minX = corners[0].x;
	float maxX = corners[0].x;
	float minZ = corners[0].y;
	float maxZ = corners[0].y;
	for (const auto& c : corners)
	{
		minX = std::min(minX, c.x);
		maxX = std::max(maxX, c.x);
		minZ = std::min(minZ, c.y);
		maxZ = std::max(maxZ, c.y);
	}
	EXPECT_NEAR(maxX - minX, 8.0f, 1e-4f);
	EXPECT_NEAR(maxZ - minZ, 4.0f, 1e-4f);
	EXPECT_NEAR((maxX + minX) * 0.5f, 100.0f, 1e-4f);
	EXPECT_NEAR((maxZ + minZ) * 0.5f, 200.0f, 1e-4f);
}

TEST(AbodeFoundation, dipIsTheLowestCornerBelowTheOrigin)
{
	const std::array heights {10.0f, 9.0f, 12.0f, 8.5f};
	EXPECT_FLOAT_EQ(abode_foundation::DeepestDip(10.0f, heights), -1.5f);
	// On a hump every corner is higher: the building is not raised
	const std::array higher {11.0f, 12.0f, 10.5f, 13.0f};
	EXPECT_FLOAT_EQ(abode_foundation::DeepestDip(10.0f, higher), 0.0f);
}

TEST(AbodeFoundation, sinkIsLimitedByTheBuildingsReach)
{
	// Small buildings go no deeper than 0.8
	EXPECT_FLOAT_EQ(abode_foundation::SinkLimit(2.0f), -0.8f);
	EXPECT_FLOAT_EQ(abode_foundation::Sink(-3.0f, 2.0f), -0.8f);
	// Larger ones by up to a fifth of their reach
	EXPECT_FLOAT_EQ(abode_foundation::SinkLimit(10.0f), -2.0f);
	EXPECT_FLOAT_EQ(abode_foundation::Sink(-3.0f, 10.0f), -2.0f);
	// A shallow dip is followed exactly
	EXPECT_FLOAT_EQ(abode_foundation::Sink(-0.3f, 10.0f), -0.3f);
	EXPECT_FLOAT_EQ(abode_foundation::Sink(0.0f, 10.0f), 0.0f);
}

TEST(AbodeFoundation, buildingOnASlopeSinksToItsLowSide)
{
	// A 6 by 4 house (reach 3) on land falling 0.1 a unit towards -x: its low corners are 0.3 below its origin
	const auto box = abode_foundation::GroundBoxOf({-3.0f, 0.0f, -2.0f}, {3.0f, 5.0f, 2.0f});
	const float sink = abode_foundation::FoundationSink(box, k_NoTurn, 1.0f, {500.0f, 500.0f}, FakeSlope {0.1f});
	EXPECT_NEAR(sink, -0.3f, 1e-4f);
	// A steep slope would bury it deeper than its limit
	const float steep = abode_foundation::FoundationSink(box, k_NoTurn, 1.0f, {500.0f, 500.0f}, FakeSlope {1.0f});
	EXPECT_FLOAT_EQ(steep, -0.8f);
	// Flat land leaves it where it is
	const float flat = abode_foundation::FoundationSink(box, k_NoTurn, 1.0f, {500.0f, 500.0f}, FakeSlope {0.0f});
	EXPECT_FLOAT_EQ(flat, 0.0f);
}
