/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <string>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/ShadowTexture.h"
#include "OracleData.h"

using namespace openblack::graphics;

namespace
{
constexpr size_t k_Vertices = 6;
constexpr size_t k_Triangles = 4;
constexpr std::array<char, 5> k_Parts {'x', 'y', 'z', 'u', 'v'};
constexpr std::array<char, 3> k_Corners {'a', 'b', 'c'};

/// The mask that the data was made with
tree_shadows::AlphaMask MadeMask()
{
	tree_shadows::AlphaMask mask {};
	for (size_t i = 0; i < mask.size(); ++i)
	{
		mask.at(i) = static_cast<uint8_t>((((i * 37) + ((i >> 6U) * 11)) & 15U) << 4U);
	}
	return mask;
}
} // namespace

// A primitive of a moving body filled into its shadow's texture through the alpha mask, as the game fills it
TEST(OracleShadowTexture, TheFilledTexelsGiveTheGameBits)
{
	const auto table = openblack::oracle::Table::Load("shadow_texture_fill.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto column = [&table](const std::string& name) {
		const auto found = table->Column(name);
		EXPECT_TRUE(found.has_value()) << name;
		return found.value_or(0);
	};
	const auto mask = MadeMask();
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto number = [&](const std::string& name) { return table->Float(row, column(name)); };
		glm::mat4 model(1.0f);
		for (glm::length_t axis = 0; axis < 4; ++axis)
		{
			for (glm::length_t part = 0; part < 3; ++part)
			{
				model[axis][part] = number("m" + std::to_string((axis * 3) + part));
			}
		}
		const GroundShadow::Footprint footprint {
		    .low = {number("lowx"), number("lowz")}, .high = {number("highx"), number("highz")}, .nearest = 0.0f};
		const GroundShadow::Cast cast {
		    .light = {number("lightx"), number("lighty"), number("lightz")}, .level = number("level"), .ground = 0.0f};
		std::array<shadow_texture::Corner, k_Vertices> corners {};
		for (size_t i = 0; i < k_Vertices; ++i)
		{
			const auto part = [&](size_t index) { return number("p" + std::to_string(i) + std::string(1, k_Parts.at(index))); };
			corners.at(i) = shadow_texture::CornerOf({part(0), part(1), part(2)}, {part(3), part(4)}, model, footprint, cast);
		}
		shadow_texture::Canvas canvas;
		for (size_t t = 0; t < k_Triangles; ++t)
		{
			std::array<shadow_texture::Corner, 3> triangle {};
			for (size_t k = 0; k < 3; ++k)
			{
				const auto index = table->Bits(row, column("t" + std::to_string(t) + std::string(1, k_Corners.at(k))));
				triangle.at(k) = corners.at(index);
			}
			shadow_texture::Fill(canvas, mask, triangle);
		}
		for (size_t word = 0; word < canvas.texels.size() / 2; ++word)
		{
			const auto game = table->Bits(row, column("texels" + std::to_string(word)));
			const auto ours = static_cast<uint32_t>(canvas.texels.at(word * 2)) |
			                  (static_cast<uint32_t>(canvas.texels.at((word * 2) + 1)) << 16U);
			if (ours != game)
			{
				++different;
				ADD_FAILURE() << "row " << row << " word " << word << ": the game gives " << std::hex << game << ", ours gives "
				              << ours;
				break;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
