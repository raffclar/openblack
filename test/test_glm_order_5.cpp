/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Bit tests for game maths that sits inside larger functions of the game, so the oracle cannot call it alone. The
// expected bits were worked out by hand from the game's instructions, one float step at a time.

#include <cstdint>

#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/TempleCameraModel.h"
#include "Hand/HandFeel.h"
#include "Particles/ParticleMaths.h"
#include "Particles/SurfaceOfRevolution.h"

using namespace openblack::particles;

namespace
{
[[nodiscard]] uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

[[nodiscard]] float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}
} // namespace

TEST(GlmOrder5, ASurfaceOfRevolutionTurnsItsProfileWithTheFpuCosine)
{
	// Five points round, three rings: the second point is a quarter of the way round, at the float of pi / 2. The FPU's
	// cosine there, stored as a float, is -4.371139e-8; times the second ring's radius of 0.5 it is -2.1855695e-8.
	const surface::Shape shape {.profile = surface::Profile::Disk, .numU = 5, .numV = 3};
	const auto mesh = surface::Build(shape, 0);
	const auto& quarter = mesh.vertices.at(5 + 1).position;
	EXPECT_EQ(Bits(quarter.x), 0xB2BBBD2Eu);
	EXPECT_EQ(Bits(quarter.z), 0x3F000000u);
}

TEST(GlmOrder5, ASurfaceOfRevolutionHasNoNegativeZeros)
{
	// The game turns each point with a full matrix whose place is zero, so a part that comes to zero is a positive zero.
	// The middle point half the way round has a cosine of -1 and a radius of 0.
	const surface::Shape shape {.profile = surface::Profile::Disk, .numU = 5, .numV = 3};
	const auto mesh = surface::Build(shape, 0);
	const auto& middle = mesh.vertices.at(2).position;
	EXPECT_EQ(Bits(middle.x), 0u);
	EXPECT_EQ(Bits(middle.y), 0u);
	EXPECT_EQ(Bits(middle.z), 0u);
}

TEST(GlmOrder5, AFusedBurstThrowsItsAtomsInTheGamesOrder)
{
	// The squares added z, x, y; the direction times one over the length; the vertical part times the speed, then
	// times the scale
	const glm::vec3 direction {F(0x3F583AFBu), F(0xBF71263Au), F(0xBD8CCF42u)};
	const auto velocity = maths::BurstVelocity(direction, F(0x419763E6u), F(0x400118A3u), true);
	EXPECT_EQ(Bits(velocity.x), 0x4149D60Au);
	EXPECT_EQ(Bits(velocity.y), 0x41E30624u);
	EXPECT_EQ(Bits(velocity.z), 0xBF836F9Eu);
}

TEST(GlmOrder5, ThePoolMapTurnsAndTiltsAsTheGameDoes)
{
	// The way to the camera made one long with its squares added y, z, x; the FPU's arctangents stored as floats
	const auto angles = openblack::TempleCameraModel::MapYawAndPitch({3.7f, -2.1f, 5.3f});
	EXPECT_EQ(Bits(angles.x), 0x3F761A3Cu);
	EXPECT_EQ(Bits(angles.y), 0xBEA0D5D9u);
	// Each cosine and sine stays wide for its product
	const auto around = openblack::TempleCameraModel::MapAround(0.9f, 0.7f);
	EXPECT_EQ(Bits(around.x), 0x3EF36C07u);
	EXPECT_EQ(Bits(around.y), 0x3F24EB73u);
	EXPECT_EQ(Bits(around.z), 0x3F196006u);
}

TEST(GlmOrder5, TheMainRoomLeansAtTheEdgeAsTheGameDoes)
{
	// (dt x 1.5) x (1 - the wide cosine of ramp x pi), then times the speed
	EXPECT_EQ(Bits(openblack::TempleCameraModel::EdgeLeanStep(0.016f, 0.4f, -0.08f)), 0xBAADE43Eu);
}

TEST(GlmOrder5, TheMainRoomOrbitsAsTheGameDoes)
{
	// The way between the low and the high view times the lean, the low added; then turned by a full matrix
	const auto pose = openblack::TempleCameraModel::OrbitPose(0.7f, 0.35f);
	EXPECT_EQ(Bits(pose.origin.x), 0xC128A163u);
	EXPECT_EQ(Bits(pose.origin.y), 0x41428F5Cu);
	EXPECT_EQ(Bits(pose.origin.z), 0x41483482u);
	EXPECT_EQ(Bits(pose.focus.x), 0xBFD66548u);
	EXPECT_EQ(Bits(pose.focus.y), 0x40866666u);
	EXPECT_EQ(Bits(pose.focus.z), 0x3FFE8A1Bu);
}

namespace
{
const glm::vec3 k_Camera {120.5f, 85.25f, -340.75f};
const glm::vec3 k_NearPoint {121.13f, 84.61f, -339.98f};

void ExpectBits(const glm::vec3& ours, uint32_t x, uint32_t y, uint32_t z)
{
	EXPECT_EQ(Bits(ours.x), x);
	EXPECT_EQ(Bits(ours.y), y);
	EXPECT_EQ(Bits(ours.z), z);
}
} // namespace

TEST(GlmOrder5, TheHandsLineOfSightIsTheWayToTheCursorsPoint)
{
	// The cursor's point at the distance, its squares added z, y, x; then the way to it made one long, x, z, y
	ExpectBits(openblack::hand_feel::CursorDirection(k_Camera, k_NearPoint, 247.3f), 0x3F08560Cu, 0xBF0A802Cu, 0x3F26A1A3u);
}

TEST(GlmOrder5, HoldingSomethingTheHandFeelsTowardsTheRaisedGroundInTheGamesOrder)
{
	// The lift is added to the rise from the camera; the squares are added y, x, z
	const auto felt = openblack::hand_feel::FeelDirection(
	    k_Camera, glm::vec3(0.0f), openblack::hand_feel::RaisedGround {.ground = {180.4f, 12.7f, -101.9f}, .lift = 3.6f});
	ExpectBits(felt, 0x3E6FDD5Bu, 0xBE8A0D67u, 0x3F6F1D26u);
}

TEST(GlmOrder5, TheHandRestsOnAModelInTheGamesOrder)
{
	using namespace openblack::hand_feel;
	const auto cursor = CursorDirection(glm::vec3(0.0f), glm::vec3(0.31f, -0.62f, 0.72f), 1.0f);
	const auto felt = FeelDirection(glm::vec3(0.0f), cursor, RaisedGround {.ground = {0.35f, -0.55f, 0.76f}, .lift = 0.0f});
	const auto rest =
	    RestOnModel(k_Camera, cursor, felt, {150.2f, 40.9f, -301.3f}, {-0.4f, 2.1f, 0.9f}, true,
	                Holding {.radius = 4.5f, .creature = CreatureReach {.centre = {140.0f, -295.0f}, .radius = 30.0f}});
	ExpectBits(rest.point, 0x4302E06Cu, 0x4280FE51u, 0xC39E5320u);
	ASSERT_TRUE(rest.up.has_value());
	ExpectBits(*rest.up, 0x3DC25FF6u, 0xBE8025E4u, 0xBF117018u);
}

TEST(GlmOrder5, TheHandHoversBeforeAVillagerInTheGamesOrder)
{
	using namespace openblack::hand_feel;
	const auto felt =
	    FeelDirection(glm::vec3(0.0f), glm::vec3(0.0f), RaisedGround {.ground = {0.35f, -0.55f, 0.76f}, .lift = 0.0f});
	ExpectBits(RestAtPosition(k_Camera, k_NearPoint, felt, {160.3f, 31.2f, -270.4f}, 1.7f), 0x432AAFC8u, 0x420910F6u,
	           0xC38BB4A8u);
}

TEST(GlmOrder5, TheHandsHoverInTheGamesOrder)
{
	using namespace openblack::hand_feel;
	// The limit's squares are added z, y, x; the target is the distance to the cursor's point at the limit, x, z, y
	Hover hover {.camera = k_Camera,
	             .land = glm::vec3(175.0f, 9.5f, -150.0f),
	             .handHeight = 2.4f,
	             .currentDistance = 50.0f,
	             .minDistance = 2.0f,
	             .reach = 1800.0f};
	const float limit = HoverLimit(hover);
	EXPECT_EQ(Bits(limit), 0x4351F404u);
	hover.cursorPoint = PointAlongCursor(k_Camera, k_NearPoint, limit);
	EXPECT_EQ(Bits(HoverDistancesOf(hover).target), 0x4351F406u);
	ExpectBits(UnitUp({0.3f, 1.7f, -0.45f}), 0x3E2C3397u, 0x3F73F3C1u, 0xBE8126B1u);
}
