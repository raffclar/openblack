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

#include <gtest/gtest.h>

#include "Camera/TempleCameraModel.h"
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
