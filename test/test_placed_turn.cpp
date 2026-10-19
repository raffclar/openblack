/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <limits>

#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Common/X87Angle.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Transform.h"
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
	const auto angles = ecs::placed_turn::TurnedAboutUpright(glm::vec3(0.3f, 0.5f, -0.2f), 0.4f);
	EXPECT_EQ(angles, glm::vec3(0.3f, 0.5f - 0.4f, -0.2f));
	// The same as a turn of the world about the upright axis
	const auto turn = x87::GameTurnMatrix(0.3f, 0.5f, -0.2f);
	ExpectNear(x87::GameTurnMatrix(angles.x, angles.y, angles.z), glm::mat3(glm::eulerAngleY(0.4f)) * turn, 1e-5f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, OnlyMobileThingsAndDeadTreesLean)
{
	ecs::Registry registry;
	const auto mobileStatic = registry.Create();
	registry.Assign<ecs::components::MobileStatic>(mobileStatic, MobileStaticInfo::Rock);
	const auto deadTree = registry.Create();
	registry.Assign<ecs::components::DeadTree>(deadTree, ecs::components::DeadTree {.type = TreeInfo::Pine});
	const auto ball = registry.Create();
	registry.Assign<ecs::components::Ball>(ball);
	const auto piece = registry.Create();
	registry.Assign<ecs::components::BuildingPiece>(piece);
	const auto feature = registry.Create();
	registry.Assign<ecs::components::Feature>(feature, FeatureInfo::Ark);
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, mobileStatic));
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, deadTree));
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, ball));
	EXPECT_TRUE(ecs::placed_turn::CanLean(registry, piece));
	EXPECT_FALSE(ecs::placed_turn::CanLean(registry, feature));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, AThingThatDoesNotLeanKeepsOnlyItsTurn)
{
	EXPECT_EQ(ecs::placed_turn::KeptAngles(glm::vec3(0.3f, 4.0f, -0.2f), false), glm::vec3(0.0f, 4.0f, 0.0f));
	EXPECT_EQ(ecs::placed_turn::KeptAngles(glm::vec3(0.3f, 4.0f, -0.2f), true), glm::vec3(0.3f, 4.0f, -0.2f));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, AThingReadsBackTheAnglesItGot)
{
	// A lean past a quarter turn makes the same turn as other angles, but the thing keeps the angles it got
	ecs::Registry registry;
	const auto rock = registry.Create();
	registry.Assign<ecs::components::Transform>(rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::MobileStatic>(rock, MobileStaticInfo::Rock);
	ecs::placed_turn::SetAngles(registry, rock, glm::vec3(2.0f, 4.0f, -0.5f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, rock), glm::vec3(2.0f, 4.0f, -0.5f));
	const auto& transform = registry.Get<const ecs::components::Transform>(rock);
	const auto takenOff = x87::GameTurnMatrixAngles(transform.rotation);
	EXPECT_GT(std::abs(takenOff.x - 2.0f), 0.5f);
	ExpectNear(transform.rotation, x87::GameTurnMatrix(2.0f, 4.0f, -0.5f), 0.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, AnUprightThingKeepsItsWholeTurn)
{
	ecs::Registry registry;
	const auto feature = registry.Create();
	registry.Assign<ecs::components::Transform>(feature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Feature>(feature, FeatureInfo::Ark);
	ecs::placed_turn::SetAngles(registry, feature, glm::vec3(0.4f, 4.0f, 0.1f));
	// The angle about y is not brought back into a half turn either way, and the leans are gone
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, feature), glm::vec3(0.0f, 4.0f, 0.0f));
	ExpectNear(registry.Get<const ecs::components::Transform>(feature).rotation, x87::GameUprightTurn(4.0f), 0.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, ANewTurnReplacesTheKeptAngles)
{
	const ecs::components::TurnAngles kept {.angles = glm::vec3(2.0f, 4.0f, -0.5f),
	                                        .turn = x87::GameTurnMatrix(2.0f, 4.0f, -0.5f)};
	EXPECT_EQ(ecs::placed_turn::AnglesOf(&kept, kept.turn, true), kept.angles);
	// An other turn gives the angles that the game finds from the turn
	const auto other = x87::GameTurnMatrix(0.2f, 0.7f, -0.4f);
	EXPECT_EQ(ecs::placed_turn::AnglesOf(&kept, other, true), x87::GameTurnMatrixAngles(other));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(&kept, other, false), glm::vec3(0.0f, x87::GameTurnMatrixAngles(other).y, 0.0f));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(nullptr, other, true), x87::GameTurnMatrixAngles(other));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, KeptAnglesNeedATransform)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	ecs::placed_turn::KeepAngles(registry, thing, glm::vec3(1.0f));
	EXPECT_FALSE(registry.AllOf<ecs::components::TurnAngles>(thing));
	EXPECT_EQ(ecs::placed_turn::AnglesOf(registry, thing), glm::vec3(0.0f));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, AMovedThingTakesTheAnglesOfItsBody)
{
	// A body that tumbled past a quarter turn about x
	const auto axes = glm::mat3(glm::eulerAngleY(-0.6f) * glm::eulerAngleX(-2.2f) * glm::eulerAngleZ(0.3f));
	const auto leans = ecs::placed_turn::TurnFromBody(axes, true);
	EXPECT_EQ(leans.angles, x87::GameTurnMatrixAngles(axes));
	ExpectNear(leans.turn, x87::GameTurnMatrix(leans.angles.x, leans.angles.y, leans.angles.z), 0.0f);
	// The game's turn from those angles is the body's turn
	ExpectNear(leans.turn, axes, 1e-5f);
	// A thing that does not lean stands upright with the heading that the game finds
	const auto upright = ecs::placed_turn::TurnFromBody(axes, false);
	EXPECT_EQ(upright.angles, glm::vec3(0.0f, leans.angles.y, 0.0f));
	ExpectNear(upright.turn, x87::GameUprightTurn(leans.angles.y), 0.0f);
	EXPECT_EQ(upright.turn[1], glm::vec3(0.0f, 1.0f, 0.0f));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(PlacedTurn, TheUprightTurnIsTheTurnAboutY)
{
	for (const float y : {0.7f, -2.9f, 3.1f, 4.0f})
	{
		ExpectNear(x87::GameUprightTurn(y), x87::GameTurnMatrix(0.0f, y, 0.0f), 0.0f);
	}
	// No turn at 0, and no turn when the angle is not a number
	EXPECT_EQ(x87::GameUprightTurn(0.0f), glm::mat3(1.0f));
	EXPECT_EQ(x87::GameUprightTurn(std::numeric_limits<float>::quiet_NaN()), glm::mat3(1.0f));
}
