/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/HandOrientation.h"
#include "Magic/HandHoldPose.h"

using namespace openblack::magic::hand_hold;

namespace
{
constexpr float k_Near = 1e-6f;

/// The hand's heading from a line of sight, when the line leans enough across the land to give one
std::optional<float> HeadingOfSight(glm::vec3 line)
{
	if (!openblack::hand_orientation::LeansAcrossLand(line))
	{
		return std::nullopt;
	}
	return openblack::hand_orientation::HeadingAngle(line);
}

/// The level part of a line, made unit
glm::vec3 Level(glm::vec3 line)
{
	return glm::normalize(glm::vec3(line.x, 0.0f, line.z));
}
} // namespace

TEST(HeldFrameHeading, ALineThatLeansTooLittleGivesNoHeading)
{
	EXPECT_FALSE(HeadingOfSight({0.0f, -1.0f, 0.0f}).has_value());
	EXPECT_FALSE(HeadingOfSight({0.01f, -1.0f, -0.01f}).has_value());
	EXPECT_TRUE(HeadingOfSight({0.0f, -1.0f, 0.0101f}).has_value());
	EXPECT_TRUE(HeadingOfSight({-0.0101f, -1.0f, 0.0f}).has_value());
}

TEST(HeldFrameHeading, TheHeadingIsZeroAlongMinusZAndGrowsTowardsPlusX)
{
	EXPECT_NEAR(*HeadingOfSight({0.0f, -0.5f, -1.0f}), 0.0f, k_Near);
	EXPECT_NEAR(*HeadingOfSight({1.0f, -0.5f, 0.0f}), 1.5707964f, k_Near);
	EXPECT_NEAR(*HeadingOfSight({-1.0f, -0.5f, 0.0f}), -1.5707964f, k_Near);
}

TEST(HeldFrameHeading, AHeldThingFacesBackAlongTheLineOfSight)
{
	// At the centre of the screen and at its two sides, the held thing faces back along each line, not along the
	// camera's own line
	for (const glm::vec3 line : {glm::vec3(0.0f, -0.6f, -1.0f), glm::vec3(0.7f, -0.6f, -1.0f), glm::vec3(-0.7f, -0.6f, -1.0f)})
	{
		const auto facing = HeldFacing(*HeadingOfSight(line));
		EXPECT_EQ(facing.y, 0.0f);
		EXPECT_NEAR(glm::dot(facing, -Level(line)), 1.0f, k_Near);
	}
}

TEST(HeldFrameHeading, TheFrameOfAHeldThingFollowsTheHeading)
{
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	const glm::vec3 left {-0.7f, -0.6f, -1.0f};
	const glm::vec3 right {0.7f, -0.6f, -1.0f};
	const auto atLeft = HeldBasis(HeldFacing(*HeadingOfSight(left)), up);
	const auto atRight = HeldBasis(HeldFacing(*HeadingOfSight(right)), up);
	// The third axis of the frame points along the level line of sight, away from the camera. The frames at the two sides
	// are turned by opposite angles.
	EXPECT_NEAR(glm::dot(atLeft[2], Level(left)), 1.0f, k_Near);
	EXPECT_NEAR(glm::dot(atRight[2], Level(right)), 1.0f, k_Near);
	EXPECT_NEAR(atLeft[2].x, -atRight[2].x, k_Near);
	EXPECT_NEAR(atLeft[2].z, atRight[2].z, k_Near);
	EXPECT_EQ(atLeft[1], up);
}
