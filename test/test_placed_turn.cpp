/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Angle.h"
#include "ECS/Components/Feature.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"

using namespace openblack;

namespace
{

void ExpectNear(const glm::mat3& a, const glm::mat3& b, float tolerance)
{
	for (glm::length_t c = 0; c < 3; ++c)
	{
		for (glm::length_t r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(a[c][r], b[c][r], tolerance) << "column " << c << " row " << r;
		}
	}
}

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, TheGameLeansAboutZThenXThenTurnsAboutY)
{
	constexpr float k_X = 0.4f;
	constexpr float k_Y = 0.8f;
	constexpr float k_Z = -0.3f;
	// glm turns the vector by the last matrix first: z, then x, then y
	const auto expected = glm::mat3(glm::eulerAngleY(-k_Y) * glm::eulerAngleX(-k_X) * glm::eulerAngleZ(-k_Z));
	ExpectNear(x87::GameTurnMatrix(k_X, k_Y, k_Z), expected, 1e-6f);
	// The turn that the code used before (x last) is not the game's turn when the thing leans and turns
	const auto before = glm::mat3(glm::eulerAngleXYZ(-k_X, -k_Y, -k_Z));
	EXPECT_GT(std::abs(before[1].x - expected[1].x), 0.05f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, AnUprightTurnIsATurnAboutY)
{
	for (const float y : {0.0f, 0.7f, -2.9f, 3.1f})
	{
		ExpectNear(x87::GameTurnMatrix(0.0f, y, 0.0f), glm::mat3(glm::eulerAngleY(-y)), 1e-6f);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, TheLeanFollowsTheFacing)
{
	// A thing turned a quarter about y and leaned about x tips along the world's x axis, not along z
	const auto turn = x87::GameTurnMatrix(0.5f, 1.5707964f, 0.0f);
	EXPECT_NEAR(turn[1].z, 0.0f, 1e-6f);
	EXPECT_NEAR(std::abs(turn[1].x), std::sin(0.5f), 1e-6f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, TheScaleMultipliesEachValue)
{
	const auto turn = x87::GameTurnMatrix(0.2f, 0.7f, -0.4f);
	const auto scaled = x87::GameTurnMatrixScaled(0.2f, 0.7f, -0.4f, 2.5f);
	for (glm::length_t c = 0; c < 3; ++c)
	{
		for (glm::length_t r = 0; r < 3; ++r)
		{
			EXPECT_EQ(scaled[c][r], 2.5f * turn[c][r]);
		}
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, TheAnglesComeBackFromTheTurn)
{
	const auto angles = x87::GameTurnMatrixAngles(x87::GameTurnMatrix(0.2f, 2.7f, -0.4f));
	EXPECT_NEAR(angles.x, 0.2f, 1e-5f);
	EXPECT_NEAR(angles.y, 2.7f, 1e-5f);
	EXPECT_NEAR(angles.z, -0.4f, 1e-5f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, ATurnAboutTheUprightKeepsTheLeans)
{
	const auto turn = x87::GameTurnMatrix(0.3f, 0.5f, -0.2f);
	const auto turned = ecs::placed_turn::TurnedAboutUpright(turn, 0.4f);
	// The same as a turn of the world about the upright axis
	ExpectNear(turned, glm::mat3(glm::eulerAngleY(0.4f)) * turn, 1e-5f);
	const auto angles = x87::GameTurnMatrixAngles(turned);
	EXPECT_NEAR(angles.x, 0.3f, 1e-5f);
	EXPECT_NEAR(angles.y, 0.1f, 1e-5f);
	EXPECT_NEAR(angles.z, -0.2f, 1e-5f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, OnlyMobileThingsAndDeadTreesLean)
{
	ecs::Registry registry;
	const auto mobileStatic = registry.Create();
	registry.Assign<ecs::components::MobileStatic>(mobileStatic, MobileStaticInfo::Rock);
	const auto deadTree = registry.Create();
	registry.Assign<ecs::components::DeadTree>(deadTree, ecs::components::DeadTree {.type = TreeInfo::Pine});
	const auto feature = registry.Create();
	registry.Assign<ecs::components::Feature>(feature, FeatureInfo::Ark);
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, mobileStatic));
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, deadTree));
	EXPECT_FALSE(ecs::placed_turn::CanLean(registry, feature));
}
