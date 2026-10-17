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
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Hand/HandGrabRules.h"

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

glm::vec3 Vec(const std::array<uint32_t, 3>& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

} // namespace

TEST(GlmOrder6, AHeldThingsAxesAtItsSize)
{
	for (const auto& c : k_AxisCases)
	{
		// Each axis of the matrix is the same case, so every column is checked
		const glm::mat3 axes(Vec(c.axis), Vec(c.axis), Vec(c.axis));
		const auto sized = hand_grab::AxesAtSize(axes, std::bit_cast<float>(c.size));
		for (int column = 0; column < 3; ++column)
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
