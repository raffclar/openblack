/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureThrow.h"
#include "ECS/ObjectMetrics.h"

using namespace openblack;

namespace
{
constexpr float k_Tolerance = 1e-4f;
} // namespace

TEST(CreatureThrow, FlightTimeIsTheTimeToFallTheDistance)
{
	EXPECT_NEAR(creature_throw::FlightTime(9.81f), 1.0f, k_Tolerance);
	EXPECT_NEAR(creature_throw::FlightTime(4.0f * 9.81f), 2.0f, k_Tolerance);
	EXPECT_EQ(creature_throw::FlightTime(-1.0f), 0.0f);
}

TEST(CreatureThrow, TheReleaseVelocityLandsOnTheTarget)
{
	const glm::vec3 release {1.0f, 12.0f, -3.0f};
	const glm::vec3 target {20.0f, 0.0f, -60.0f};
	const auto seconds = creature_throw::FlightTime(glm::distance(release, target));
	const auto velocity = creature_throw::ReleaseVelocity(target, release, seconds);
	// Gravity's half g t squared is made up in the upward speed
	EXPECT_NEAR(velocity.y, (target.y - release.y) / seconds + 0.5f * creature_throw::k_Gravity * seconds, k_Tolerance);
	// On the curve of a thing falling freely, it is at the target at that time
	const auto landed =
	    release + (velocity * seconds) + glm::vec3(0.0f, -0.5f * creature_throw::k_Gravity * seconds * seconds, 0.0f);
	EXPECT_NEAR(landed.x, target.x, 1e-3f);
	EXPECT_NEAR(landed.y, target.y, 1e-3f);
	EXPECT_NEAR(landed.z, target.z, 1e-3f);
}

TEST(CreatureThrow, ItThrowsAtNothingTooClose)
{
	// Two thirds of its height, 15 units at size 1, as tall as a creature is everywhere
	EXPECT_EQ(creature_throw::k_HeightAtSizeOne, ecs::object::k_CreatureHeightPerScale);
	EXPECT_FALSE(creature_throw::FarEnoughToThrow(9.8f, 1.0f));
	EXPECT_TRUE(creature_throw::FarEnoughToThrow(10.0f, 1.0f));
	EXPECT_FALSE(creature_throw::FarEnoughToThrow(19.0f, 2.0f));
}

TEST(CreatureThrow, HighTargetsBlendInTheHighThrow)
{
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(0.5f, 0.5f, 1.5f), 0.0f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(1.0f, 0.5f, 1.5f), 0.5f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(3.0f, 0.5f, 1.5f), 1.0f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(-2.0f, 0.5f, 1.5f), 0.0f);
	EXPECT_FLOAT_EQ(creature_throw::HighThrowWeight(1.0f, 0.5f, 0.5f), 0.0f);
}

TEST(CreatureThrow, TossingKeepsSomeOfTheHandsSpeed)
{
	// The hand moved 1 unit ahead in the last 100 ms: 10 units a second
	const auto hand = creature_throw::HandVelocity(glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f, 0.0f, -1.0f), 100.0f);
	EXPECT_NEAR(hand.z, -10.0f, k_Tolerance);
	const auto plain = creature_throw::TossVelocity(glm::vec3(3.0f, 1.0f, -10.0f), false, glm::mat3(1.0f), 0.6f);
	EXPECT_NEAR(plain.x, 1.8f, k_Tolerance);
	EXPECT_NEAR(plain.z, -6.0f, k_Tolerance);
	// Tossed with the other hand it goes the other way sideways; turned with the creature
	const auto mirrored = creature_throw::TossVelocity(glm::vec3(3.0f, 1.0f, -10.0f), true, glm::mat3(1.0f), 0.6f);
	EXPECT_NEAR(mirrored.x, -1.8f, k_Tolerance);
	const auto turned = creature_throw::TossVelocity(glm::vec3(0.0f, 0.0f, -10.0f), false,
	                                                 glm::mat3(glm::eulerAngleY(glm::half_pi<float>())), 1.0f);
	EXPECT_NEAR(turned.x, -10.0f, k_Tolerance);
	EXPECT_NEAR(turned.z, 0.0f, k_Tolerance);
}
