/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Hand/HandGrabRules.h"
#include "Particles/SurfaceOfRevolution.h"

using namespace openblack;

namespace
{

/// An axis, the size, and the axis at that size, as bits worked from the game's instructions
struct AxisCase
{
	std::array<uint32_t, 3> axis;
	uint32_t size;
	std::array<uint32_t, 3> sized;
};

// Worked in floats in the game's order: the squares added z, y, then x; the size divided by the root; each part times
// that share. Each case gives other bits when the squares are added x, y, then z.
constexpr std::array<AxisCase, 4> k_AxisCases {{
    {{0x4039298Bu, 0x4032778Du, 0x3F6C6CCEu}, 0x4024F71Bu, {0x3FE783E7u, 0x3FDF24D6u, 0x3F13CE34u}},
    {{0xBFEDEB89u, 0xBFC63011u, 0xC03472C3u}, 0x40010F47u, {0xBF8124B0u, 0xBF572734u, 0xBFC3E526u}},
    {{0x4034D636u, 0x3FE90521u, 0xBF966AA1u}, 0x4064BC6Eu, {0x4035828Du, 0x3FE9E333u, 0xBF96F9FAu}},
    {{0xBFBAA367u, 0x3FF89D85u, 0x3F142C18u}, 0x3FB15EF9u, {0xBF4F2C25u, 0x3F89FBEFu, 0x3EA4796Du}},
}};

/// A matrix (three rows and a place), a light's point, and the point in the matrix's frame made unit
struct LightCase
{
	std::array<uint32_t, 12> matrix;
	std::array<uint32_t, 3> light;
	std::array<uint32_t, 3> towards;
};

// Worked in floats in the game's order: x adds the products of rows 0, 2, 1, y and z add rows 2, 1, 0, then the place;
// the unit share is 1 over the root of the squares added x, z, then y. Each case gives other bits in glm's order.
constexpr std::array<LightCase, 3> k_LightCases {{
    {{0x3FF6FFE6u, 0x3FBEAC3Bu, 0xBF57C063u, 0x3FEC46D8u, 0x3E20A8CAu, 0x3F361931u, 0xBF972724u, 0x3FE1C79Bu, 0x3F4337A4u,
      0x423AA030u, 0x421D7F26u, 0xC1A0F809u},
     {0xC30ACF60u, 0xC3A705A0u, 0xC3B12628u},
     {0xBED0BDDCu, 0xBF523509u, 0xBECC818Du}},
    {{0x3F7FCA44u, 0x3FB0943Au, 0xBFF6BFDAu, 0x3F93526Fu, 0xBF0906F0u, 0x3EA0CE7Bu, 0xBFFB5A14u, 0xBFE81362u, 0xBFA35E85u,
      0x4236126Cu, 0xC1F2C85Du, 0x41CC96D1u},
     {0x43D6D3E2u, 0x43DD059Cu, 0xC31B9E41u},
     {0x3F5C1E5Eu, 0x3ECF9815u, 0xBE9EDED8u}},
    {{0xBF14B117u, 0x3DCA5B79u, 0x3F8D1BD7u, 0xBFC8AD4Bu, 0x3F7E5C0Fu, 0x3F982E18u, 0x3FB829DBu, 0xBFED3EA0u, 0x3FE43FECu,
      0xC223872Fu, 0xC17ED0ADu, 0x413152F6u},
     {0x43D10B29u, 0xC3200A5Cu, 0x43D4194Cu},
     {0x3EC4CD1Eu, 0xBF1CBB8Du, 0x3F30E27Du}},
}};

/// A normal, the unit way to the light, the ambient level and the light level
struct LevelCase
{
	std::array<uint32_t, 3> normal;
	std::array<uint32_t, 3> towards;
	uint32_t ambient;
	uint32_t level;
};

// The products added z, y, then x, times 255 and rounded: each case rounds to another whole number in glm's order
constexpr std::array<LevelCase, 3> k_LevelCases {{
    {{0xBD3746A7u, 0x3E9241F5u, 0x3F4081BFu}, {0xBE870FEDu, 0x3E13301Bu, 0xBF4EE78Cu}, 40u, 40u},
    {{0x3F651976u, 0xBE5A0CD6u, 0x3F12A8EAu}, {0xBF036FFEu, 0x3E079767u, 0x3E042CD8u}, 40u, 40u},
    {{0x3F003517u, 0x3EEEBEAEu, 0xBEE05DB8u}, {0xBC0D4529u, 0xBE9A6D6Fu, 0xBF687330u}, 40u, 94u},
}};

glm::vec3 Vec(const std::array<uint32_t, 3>& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

} // namespace

TEST(GlmOrder6, AHeldThingsAxesAtItsSize)
{
	for (const auto& c : k_AxisCases)
	{
		// The second and third axes are the same case; the first axis adds its squares in another order (GlmOrder7)
		const glm::mat3 axes(Vec(c.axis), Vec(c.axis), Vec(c.axis));
		const auto sized = hand_grab::AxesAtSize(axes, std::bit_cast<float>(c.size));
		for (int column = 1; column < 3; ++column)
		{
			for (int part = 0; part < 3; ++part)
			{
				EXPECT_EQ(std::bit_cast<uint32_t>(sized[column][part]), c.sized.at(static_cast<size_t>(part)));
			}
		}
	}
	// An axis of zero length stays
	const auto zero = hand_grab::AxesAtSize(glm::mat3(0.0f), 2.0f);
	EXPECT_EQ(zero[1], glm::vec3(0.0f));
}

TEST(GlmOrder6, TheLightOfALitParticleMesh)
{
	for (const auto& c : k_LightCases)
	{
		glm::mat4 matrix {1.0f};
		for (int i = 0; i < 4; ++i)
		{
			for (int k = 0; k < 3; ++k)
			{
				matrix[i][k] = std::bit_cast<float>(c.matrix.at(static_cast<size_t>(3 * i + k)));
			}
		}
		const auto towards = particles::surface::TowardsLight(matrix, Vec(c.light));
		for (int part = 0; part < 3; ++part)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(towards[part]), c.towards.at(static_cast<size_t>(part)));
		}
	}
	for (const auto& c : k_LevelCases)
	{
		EXPECT_EQ(particles::surface::LightLevel(Vec(c.normal), Vec(c.towards), c.ambient), c.level);
	}
}
