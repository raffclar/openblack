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
