/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <numbers>

#include <Camera/ScriptCameraModel.h>
#include <Camera/WorldTurn.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
constexpr glm::vec3 k_TurnPoint {1040.0f, 5.0f, 1050.0f};
constexpr float k_Distance = 84.0f;
constexpr float k_SightDistance = 70.0f;
} // namespace

TEST(WorldTurn, ThePitchLimitsAreTheBitsOfTheGame)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(world_turn::k_LowestPitch), 0xBF060A92u);
	EXPECT_EQ(std::bit_cast<uint32_t>(world_turn::k_HighestPitch), 0x3FAFEDE0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(world_turn::k_TiltPerInput), 0x3B03126Fu);
	EXPECT_EQ(world_turn::k_TiltPerInput, 0.002f);
}

TEST(WorldTurn, ThePitchStaysInsideTheLimits)
{
	EXPECT_EQ(world_turn::ClampPitch(0.3f), 0.3f);
	EXPECT_EQ(world_turn::ClampPitch(-1.0f), world_turn::k_LowestPitch);
	EXPECT_EQ(world_turn::ClampPitch(world_turn::k_LowestPitch), world_turn::k_LowestPitch);
	EXPECT_EQ(world_turn::ClampPitch(1.5f), world_turn::k_HighestPitch);
	EXPECT_EQ(world_turn::ClampPitch(world_turn::k_HighestPitch), world_turn::k_HighestPitch);
	// A pitch that is not a number fails the first compare, with the lowest pitch
	EXPECT_EQ(world_turn::ClampPitch(std::numeric_limits<float>::quiet_NaN()), world_turn::k_LowestPitch);
}

TEST(WorldTurn, ATurnAddsAHalfTurnForEachWidthOfTheScreen)
{
	const float turn = 23.7f;
	const float width = 1024.0f;
	const float heading = 0.61f;
	EXPECT_EQ(world_turn::Turned(heading, turn, width), ((turn * std::numbers::pi_v<float>) / width) + heading);
	EXPECT_NEAR(world_turn::Turned(0.0f, width, width), std::numbers::pi_v<float>, 1e-6f);
}

TEST(WorldTurn, ATiltSubtractsTheTiltOfTheInputInsideTheLimits)
{
	const float input = 17.3f;
	const float tilt = world_turn::k_TiltPerInput * input;
	EXPECT_EQ(world_turn::Tilted(0.7f, input), 0.7f - tilt);
	EXPECT_EQ(world_turn::Tilted(-0.5f, 100.0f), world_turn::k_LowestPitch);
	EXPECT_EQ(world_turn::Tilted(1.3f, -100.0f), world_turn::k_HighestPitch);
}

TEST(WorldTurn, TheUnitVectorAddsTheSquaresInTheOrderOfTheGame)
{
	// For this vector the order of the squares changes the bits of the length
	const glm::vec3 vector {1.9f, 2.2f, 50.6f};
	const float gameOrder = ((vector.x * vector.x) + (vector.z * vector.z)) + (vector.y * vector.y);
	const float otherOrder = ((vector.x * vector.x) + (vector.y * vector.y)) + (vector.z * vector.z);
	ASSERT_NE(gameOrder, otherOrder);
	const float scale = 1.0f / std::sqrt(gameOrder);
	const auto unit = world_turn::Unit(vector);
	EXPECT_EQ(unit.x, vector.x * scale);
	EXPECT_EQ(unit.y, vector.y * scale);
	EXPECT_EQ(unit.z, vector.z * scale);
	// A vector of zero length stays the same
	EXPECT_EQ(world_turn::Unit(glm::vec3(0.0f)), glm::vec3(0.0f));
}

TEST(WorldTurn, ThePointAlongTheSightIsAtTheDistanceFromTheOrigin)
{
	const auto point = world_turn::AlongSight(k_Origin, k_Focus, k_SightDistance);
	const auto unit = world_turn::Unit(k_Focus - k_Origin);
	EXPECT_EQ(point.x, k_Origin.x + (unit.x * k_SightDistance));
	EXPECT_EQ(point.y, k_Origin.y + (unit.y * k_SightDistance));
	EXPECT_EQ(point.z, k_Origin.z + (unit.z * k_SightDistance));
	EXPECT_NEAR(glm::distance(point, k_Origin), k_SightDistance, 1e-3f);
}

TEST(WorldTurn, AZoomPutsTheCameraAtTheDistanceFromTheTurnPoint)
{
	const float heading = 2.1f;
	const float pitch = 0.4f;
	const auto view = world_turn::TurnAbout(k_Origin, k_Focus, k_TurnPoint, k_Distance, k_SightDistance, heading, pitch, true);
	EXPECT_EQ(view.turnPoint, k_TurnPoint);
	EXPECT_EQ(view.origin, script_camera::PointFromDistanceHeadingAndPitch(k_TurnPoint, k_Distance, heading, pitch));
	// The pitch goes inside the limits first
	const auto steep = world_turn::TurnAbout(k_Origin, k_Focus, k_TurnPoint, k_Distance, k_SightDistance, heading, 1.5f, true);
	EXPECT_EQ(steep.origin,
	          script_camera::PointFromDistanceHeadingAndPitch(k_TurnPoint, k_Distance, heading, world_turn::k_HighestPitch));
}

TEST(WorldTurn, WithoutAZoomThePointAlongTheSightStays)
{
	const float heading = 2.1f;
	const float pitch = 0.4f;
	const auto view = world_turn::TurnAbout(k_Origin, k_Focus, k_TurnPoint, k_Distance, k_SightDistance, heading, pitch, false);
	const auto placed = script_camera::PointFromDistanceHeadingAndPitch(k_TurnPoint, k_Distance, heading, pitch);
	const auto before = world_turn::AlongSight(k_Origin, k_Focus, k_SightDistance);
	const auto after = world_turn::AlongSight(placed, k_TurnPoint, k_SightDistance);
	const auto shift = before - after;
	// The camera and the turn point move by the same shift, in the order of the game
	EXPECT_EQ(view.origin, placed + shift);
	EXPECT_EQ(view.turnPoint, k_TurnPoint + shift);
	EXPECT_NEAR(glm::distance(world_turn::AlongSight(view.origin, view.turnPoint, k_SightDistance), before), 0.0f, 1e-3f);
	EXPECT_NEAR(glm::distance(view.origin, view.turnPoint), k_Distance, 1e-3f);
}

TEST(WorldTurn, TheGameAnglesOfTheTurnPointPutTheCameraBackWhereItWas)
{
	// A step with no turn and no tilt measures the angles and puts the camera back at the same distance
	const auto look = script_camera::HeadingAndPitchFromPoints(k_Origin, k_TurnPoint);
	const float distance = glm::distance(k_Origin, k_TurnPoint);
	const auto view =
	    world_turn::TurnAbout(k_Origin, k_TurnPoint, k_TurnPoint, distance, k_SightDistance, look.heading, look.pitch, true);
	EXPECT_LT(glm::distance(view.origin, k_Origin), 1e-3f);
}

TEST(WorldTurn, TheKeysMoveOverTheLandAlongTheHeading)
{
	// With a heading of 0 the camera looks along the depth. A strafe input moves it across, a forwards input along.
	const float speed = 300.0f;
	const float scale = speed * 0.001f;
	const auto across = world_turn::KeyMove(0.0f, 0.5f, speed, 0.0f, 10.0f);
	EXPECT_NEAR(across.x, -(scale * 10.0f), 1e-6f);
	EXPECT_EQ(across.y, 0.0f);
	EXPECT_NEAR(across.z, 0.0f, 1e-6f);
	// The forwards input moves less when the slope of the pitch is larger, between 0.2 and 2
	const auto along = world_turn::KeyMove(0.0f, 0.5f, speed, 10.0f, 0.0f);
	EXPECT_NEAR(along.z, (scale * 10.0f) / std::tan(0.5f), 1e-5f);
	const auto steep = world_turn::KeyMove(0.0f, 1.3f, speed, 10.0f, 0.0f);
	EXPECT_NEAR(steep.z, (scale * 10.0f) / 2.0f, 1e-5f);
	const auto flat = world_turn::KeyMove(0.0f, 0.05f, speed, 10.0f, 0.0f);
	EXPECT_NEAR(flat.z, (scale * 10.0f) / 0.2f, 1e-4f);
	// A quarter turn of the heading turns the move with it
	const auto turned = world_turn::KeyMove(std::numbers::pi_v<float> * 0.5f, 0.5f, speed, 10.0f, 0.0f);
	EXPECT_NEAR(turned.x, (scale * 10.0f) / std::tan(0.5f), 1e-4f);
	EXPECT_NEAR(turned.z, 0.0f, 1e-4f);
}
