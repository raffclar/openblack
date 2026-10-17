/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game adds the three parts of a vector in different orders in different places. These tests use a vector whose
// sum of squares has three different results for the six orders, so a wrong order changes the last bit. Each expected
// value is written out in the order of the game's instructions.

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/ScriptCameraModel.h"
#include "Common/ModelInverseSquareRoot.h"
#include "Common/X87Angle.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureObstacles.h"
#include "Creature/LeashRope.h"
#include "Magic/FlockMiracleRules.h"
#include "Physics/Body.h"

namespace x87 = openblack::x87;
using x87::SumOrder;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// A vector whose squares add to three different floats in the six orders
const glm::vec3 k_Odd {-12.43f, 20.68f, -15.84f};

float SquareSumZyx(const glm::vec3& v)
{
	return ((v.z * v.z) + (v.y * v.y)) + (v.x * v.x);
}

float SquareSumXzy(const glm::vec3& v)
{
	return ((v.x * v.x) + (v.z * v.z)) + (v.y * v.y);
}

float SquareSumYxz(const glm::vec3& v)
{
	return ((v.y * v.y) + (v.x * v.x)) + (v.z * v.z);
}
} // namespace

TEST(GlmOrder, EachOrderAddsItsOwnWay)
{
	EXPECT_EQ(Bits(x87::Dot<SumOrder::Xyz>(k_Odd, k_Odd)), 0x445044ACu);
	EXPECT_EQ(Bits(x87::Dot<SumOrder::Yxz>(k_Odd, k_Odd)), 0x445044ACu);
	EXPECT_EQ(Bits(x87::Dot<SumOrder::Xzy>(k_Odd, k_Odd)), 0x445044ABu);
	EXPECT_EQ(Bits(x87::Dot<SumOrder::Zxy>(k_Odd, k_Odd)), 0x445044ABu);
	EXPECT_EQ(Bits(x87::Dot<SumOrder::Yzx>(k_Odd, k_Odd)), 0x445044AAu);
	EXPECT_EQ(Bits(x87::Dot<SumOrder::Zyx>(k_Odd, k_Odd)), 0x445044AAu);
	// glm adds x, then y, then z
	EXPECT_EQ(Bits(glm::dot(k_Odd, k_Odd)), 0x445044ACu);
	EXPECT_EQ(Bits(x87::DotZyx(k_Odd, k_Odd)), Bits(SquareSumZyx(k_Odd)));
	EXPECT_EQ(Bits(x87::LengthZyx(k_Odd)), Bits(std::sqrt(SquareSumZyx(k_Odd))));
}

TEST(GlmOrder, DistanceAndScaledUseTheOrder)
{
	const glm::vec3 from {1.0f, 2.0f, 3.0f};
	EXPECT_EQ(Bits(x87::Distance<SumOrder::Xzy>(from + k_Odd, from)), Bits(std::sqrt(SquareSumXzy((from + k_Odd) - from))));
	// The scale is divided by the length one time, and each part is multiplied by that share
	const float share = 3.0f / std::sqrt(SquareSumZyx(k_Odd));
	const auto scaled = x87::Scaled<SumOrder::Zyx>(k_Odd, 3.0f);
	EXPECT_EQ(Bits(scaled.x), Bits(k_Odd.x * share));
	EXPECT_EQ(Bits(scaled.y), Bits(k_Odd.y * share));
	EXPECT_EQ(Bits(scaled.z), Bits(k_Odd.z * share));
}

TEST(GlmOrder, BlowTestPointsScaleTheSwingByTheReachOverItsLength)
{
	const glm::vec3 boneBefore {100.0f, 20.0f, 100.0f};
	const auto bone = boneBefore + k_Odd;
	const auto swing = bone - boneBefore;
	const float reach = 7.5f;
	const auto points = openblack::creature_fight::BlowTestPoints(bone, boneBefore, reach);
	const float share = reach / std::sqrt(SquareSumZyx(swing));
	EXPECT_EQ(Bits(points[0].x), Bits((swing.x * share) + bone.x));
	EXPECT_EQ(Bits(points[0].y), Bits((swing.y * share) + bone.y));
	EXPECT_EQ(Bits(points[0].z), Bits((swing.z * share) + bone.z));
}

TEST(GlmOrder, BlowLineUsesTheUnitDirectionFromOneDivide)
{
	const glm::vec3 origin {0.0f};
	const auto direction = k_Odd * 3.0f;
	const float length = std::sqrt(SquareSumZyx(direction));
	const auto unit = direction * (1.0f / length);
	// A point on the line just short of its end is within it; the same point past the end is not
	EXPECT_TRUE(openblack::creature_fight::WithinBlowLine(unit * (length * 0.99f), origin, direction));
	EXPECT_FALSE(openblack::creature_fight::WithinBlowLine(unit * (length * 1.01f), origin, direction));
	EXPECT_FALSE(openblack::creature_fight::WithinBlowLine(-unit, origin, direction));
}

TEST(GlmOrder, UprightPlacementRoundsTheScaledCosineOneTime)
{
	const float angle = 0.7f;
	// A scale of 2 keeps the product exact, so the expected value is two times the rounded cosine and sine
	const auto rows = openblack::creature_obstacles::UprightPlacement(angle, 2.0f);
	const float c = 2.0f * static_cast<float>(std::cos(static_cast<double>(angle)));
	const float s = 2.0f * static_cast<float>(std::sin(static_cast<double>(angle)));
	EXPECT_EQ(Bits(rows[0].x), Bits(c));
	EXPECT_EQ(Bits(rows[0].z), Bits(s));
	EXPECT_EQ(Bits(rows[2].x), Bits(-s));
	EXPECT_EQ(Bits(rows[2].z), Bits(c));
	EXPECT_EQ(rows[1], glm::vec3(0.0f, 2.0f, 0.0f));
}

TEST(GlmOrder, LeaningPlacementWithNoLeanTurnsAboutTheUprightAxis)
{
	const float angle = 0.7f;
	const auto rows = openblack::creature_obstacles::LeaningPlacement(angle, 0.0f, 0.0f, 3.0f);
	// The cosine and sine round to floats first, then the scale multiplies them
	const float c = static_cast<float>(std::cos(static_cast<double>(angle)));
	const float s = static_cast<float>(std::sin(static_cast<double>(angle)));
	EXPECT_EQ(Bits(rows[0].x), Bits(c * 3.0f));
	EXPECT_EQ(Bits(rows[0].z), Bits(s * 3.0f));
	EXPECT_EQ(Bits(rows[2].x), Bits(-s * 3.0f));
	EXPECT_EQ(Bits(rows[2].z), Bits(c * 3.0f));
	EXPECT_EQ(Bits(rows[1].y), Bits(3.0f));
}

TEST(GlmOrder, PlacedOnLandAddsYThenXThenZ)
{
	const openblack::creature_obstacles::PlacementRows rows {glm::vec3(1.1f, 0.3f, 0.7f), glm::vec3(0.2f, 1.0f, 0.9f),
	                                                         glm::vec3(-0.6f, 0.1f, 1.3f)};
	const glm::vec3 point = k_Odd;
	const glm::vec2 position {500.25f, 812.5f};
	const auto placed = openblack::creature_obstacles::PlacedOnLand(rows, point, position);
	EXPECT_EQ(Bits(placed.x), Bits((((rows[1].x * point.y) + (rows[0].x * point.x)) + (rows[2].x * point.z)) + position.x));
	EXPECT_EQ(Bits(placed.y), Bits((((rows[1].z * point.y) + (rows[0].z * point.x)) + (rows[2].z * point.z)) + position.y));
}

TEST(GlmOrder, RopeConstantsAreTheGameFloats)
{
	using namespace openblack::leash_rope;
	// The mass times the fall acceleration in float, not the decimal -7.848
	EXPECT_EQ(Bits(k_Gravity), 0xC0FB22D2u);
	EXPECT_NE(Bits(k_Gravity), Bits(-7.848f));
	// One part in 41 as a float
	EXPECT_EQ(Bits(k_RestShare), 0x3CC7CE0Cu);
	EXPECT_EQ(Bits(RestLength(32.5f)), Bits(32.5f * std::bit_cast<float>(0x3CC7CE0Cu)));
}

TEST(GlmOrder, HandTurnsToTheCameraByOneShareOfTheLength)
{
	const glm::vec3 creature {300.0f, 10.0f, 400.0f};
	const auto camera = creature + k_Odd;
	const glm::vec3 normal {0.0f, 2.0f, 0.0f};
	const auto glide = openblack::creature_feedback::HandUpGlideTo(true, true, normal, creature, camera);
	ASSERT_TRUE(glide.has_value());
	const auto away = camera - creature;
	const float share = 0.4f / std::sqrt(SquareSumXzy(away));
	EXPECT_EQ(Bits(glide->destination.x), Bits(0.0f + (away.x * share)));
	EXPECT_EQ(Bits(glide->destination.y), Bits(1.0f + (away.y * share)));
	EXPECT_EQ(Bits(glide->destination.z), Bits(0.0f + (away.z * share)));
	// Taking hold, the hand turns its back to the camera: the unit way, added from z to x
	const auto start = openblack::creature_feedback::HandUpOnTakingHold(creature, camera);
	const float inverse = 1.0f / std::sqrt(SquareSumZyx(away));
	EXPECT_EQ(Bits(start.x), Bits(away.x * inverse));
	EXPECT_EQ(Bits(start.z), Bits(away.z * inverse));
}

TEST(GlmOrder, ScriptCameraPullsIntoTheWorldWithYThenXThenZ)
{
	using openblack::script_camera::k_WorldCentre;
	const auto from = k_Odd * 300.0f;
	const glm::vec3 destination = glm::vec3(k_WorldCentre, 0.0f, k_WorldCentre) + from;
	const auto pulled = openblack::script_camera::PullIntoWorld(destination);
	ASSERT_TRUE(pulled.has_value());
	const auto fromCentre = destination - glm::vec3(k_WorldCentre, 0.0f, k_WorldCentre);
	const float scale = 1.0f / (std::sqrt(SquareSumYxz(fromCentre)) * openblack::script_camera::k_WorldScale);
	EXPECT_EQ(Bits(pulled->x), Bits((fromCentre.x * scale) + k_WorldCentre));
	EXPECT_EQ(Bits(pulled->y), Bits(fromCentre.y * scale));
	EXPECT_EQ(Bits(pulled->z), Bits((fromCentre.z * scale) + k_WorldCentre));
}

TEST(GlmOrder, GameAnglesUndoTheGameTurn)
{
	const float y = 0.6f;
	const float x = -0.3f;
	const float z = 0.2f;
	const auto rows = x87::GameTurnYxz(y, x, z);
	const auto angles = x87::GameAnglesYxz(rows[0], rows[2]);
	EXPECT_NEAR(angles.x, x, 1e-6f);
	EXPECT_NEAR(angles.y, y, 1e-6f);
	EXPECT_NEAR(angles.z, z, 1e-6f);
	// The angle about y is the game's angle of the third row, rounded to a float
	EXPECT_EQ(Bits(angles.y), Bits(x87::RoundToFloat(x87::GameAtan2(-rows[2].x, rows[2].z))));
}

TEST(GlmOrder, GameTurnRoundsEachProduct)
{
	const auto rows = x87::GameTurnYxz(0.6f, -0.3f, 0.2f);
	const double sx = std::sin(static_cast<double>(-0.3f));
	const float sz = static_cast<float>(std::sin(static_cast<double>(0.2f)));
	// The middle of the third row is the sine about x, rounded; the first row's middle is the sine about z times the
	// cosine about x, both floats
	EXPECT_EQ(Bits(rows[2].y), Bits(static_cast<float>(sx)));
	EXPECT_EQ(Bits(rows[0].y), Bits(-(sz * static_cast<float>(std::cos(static_cast<double>(-0.3f))))));
}

TEST(GlmOrder, WolfEatsFromItsOwnPlaceAlongTheWay)
{
	const glm::vec3 wolf {1200.5f, 0.25f, 980.75f};
	const auto prey = wolf + k_Odd;
	const float scale = 1.3f;
	const auto goal = openblack::magic::flock::EatingPosition(prey, wolf, scale);
	const auto towards = prey - wolf;
	const float share = scale / std::sqrt(SquareSumZyx(towards));
	EXPECT_EQ(Bits(goal.x), Bits(wolf.x + (towards.x - (towards.x * share))));
	EXPECT_EQ(Bits(goal.y), Bits(wolf.y + (towards.y - (towards.y * share))));
	EXPECT_EQ(Bits(goal.z), Bits(wolf.z + (towards.z - (towards.z * share))));
}

TEST(GlmOrder, EllipsoidNormalUsesTheEngineInverseSquareRoot)
{
	// A unit box about the origin, not turned: the line along -x from x = 5 meets it at x = 1
	openblack::physics::Ellipsoid bone {.boxMin = glm::vec3(-1.0f), .boxMax = glm::vec3(1.0f)};
	const std::array bones {bone};
	const auto hit = openblack::physics::EllipsoidHit(bones, glm::vec3(5.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->first.point, glm::vec3(1.0f, 0.0f, 0.0f));
	const float inverse = openblack::gutils::ModelInverseSquareRoot(1.0f);
	EXPECT_EQ(Bits(hit->first.normal.x), Bits(inverse));
}
