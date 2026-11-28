/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "3D/InfluenceCircle.h"
#include "Graphics/FrameUniforms.h"
#include "Graphics/InfluenceCurtains.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
influence::Circle FlatCircle(float radius)
{
	influence::Circle circle {.player = PlayerNames::PLAYER_ONE, .centre = {}, .radius = radius};
	circle.curtain = influence::MakeCurtain([](glm::vec2) { return 0.0f; }, circle.centre, radius);
	circle.hidden.assign(circle.Columns(), false);
	return circle;
}

/// The colour of a point as the border gave it before the static buffers: the strength of the border in the alpha of
/// the middle row of each column that shows
uint32_t ColourWithStrength(const influence::Circle& circle, size_t i, uint32_t rgb, uint8_t strength)
{
	const auto abgr = ((rgb & 0xFFu) << 16u) | (rgb & 0xFF00u) | ((rgb >> 16u) & 0xFFu);
	const auto column = i / 3;
	const bool hidden = circle.hidden.at(column);
	const auto colour = hidden && column == circle.Columns() - 1 ? 0xFFFFFFu : abgr;
	const uint32_t alpha = i % 3 == 1 && !hidden ? strength : 0u;
	return (alpha << 24u) | colour;
}
} // namespace

TEST(InfluenceCurtains, OnlyTheMiddleRowShowsInThePlayersColour)
{
	const auto circle = FlatCircle(100.0f);
	constexpr uint32_t k_Rgb = 0x112233u;
	const auto vertices = CurtainVertices(circle, k_Rgb);
	ASSERT_EQ(vertices.size(), circle.curtain.positions.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		EXPECT_EQ(vertices[i].position, circle.curtain.positions[i]);
		EXPECT_EQ(vertices[i].uv, circle.curtain.uvs[i]);
		EXPECT_EQ(vertices[i].colour & 0xFFFFFFu, 0x332211u);
		EXPECT_EQ(vertices[i].colour >> 24u, i % 3 == 1 ? 0xFFu : 0u);
	}
}

TEST(InfluenceCurtains, HiddenColumnsDoNotShowAndTheLastFadesToWhite)
{
	auto circle = FlatCircle(100.0f);
	const auto last = circle.Columns() - 1;
	circle.hidden.at(0) = true;
	circle.hidden.at(last) = true;
	const auto vertices = CurtainVertices(circle, 0x112233u);
	for (size_t row = 0; row < 3; ++row)
	{
		EXPECT_EQ(vertices[row].colour, 0x332211u);
		EXPECT_EQ(vertices[(last * 3) + row].colour, 0xFFFFFFu);
	}
}

TEST(InfluenceCurtains, TheShaderGivesTheSameColoursAsTheStrengthInTheVertices)
{
	auto circle = FlatCircle(60.0f);
	circle.hidden.at(1) = true;
	circle.hidden.back() = true;
	constexpr uint32_t k_Rgb = 0xFFA247u;
	const auto vertices = CurtainVertices(circle, k_Rgb);
	for (uint32_t strength = 0; strength <= 0xFFu; ++strength)
	{
		const auto values = CurtainFrameValues({0.25f, 0.5f}, static_cast<uint8_t>(strength));
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			// The GPU reads each byte as the byte over 255, and the shader multiplies the alpha by the strength
			const auto before = ColourWithStrength(circle, i, k_Rgb, static_cast<uint8_t>(strength));
			EXPECT_EQ(vertices[i].colour & 0xFFFFFFu, before & 0xFFFFFFu);
			const float alpha = static_cast<float>(vertices[i].colour >> 24u) / 255.0f * values.z;
			EXPECT_EQ(alpha, static_cast<float>(before >> 24u) / 255.0f);
		}
	}
}

TEST(InfluenceCurtains, TheFrameValuesAreTheScrollAndTheStrength)
{
	const auto values = CurtainFrameValues({0.125f, 0.75f}, 120);
	EXPECT_EQ(values.x, 0.125f);
	EXPECT_EQ(values.y, 0.75f);
	EXPECT_EQ(values.z, 120.0f / 255.0f);
	EXPECT_EQ(values.w, 0.0f);
}

TEST(FrameUniforms, TheIslandExtentIsTheCornersOrZero)
{
	EXPECT_EQ(IslandExtentValue(std::nullopt), glm::vec4(0.0f));
	const Extent2 extent {.minimum = {-10.0f, 20.0f}, .maximum = {300.0f, 400.0f}};
	EXPECT_EQ(IslandExtentValue(extent), glm::vec4(-10.0f, 20.0f, 300.0f, 400.0f));
}
