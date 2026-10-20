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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/ShadowTexture.h"

using namespace openblack::graphics;

namespace
{
constexpr size_t k_Texels = shadow_texture::k_Texels;

size_t At(size_t row, size_t column)
{
	return (row * k_Texels) + column;
}
} // namespace

// The light of a moving body stands straight above it
TEST(ShadowTexture, TheLightIsStraightAbove)
{
	const auto light = shadow_texture::LightAbove({10.0f, 20.0f, 30.0f});
	EXPECT_EQ(light, glm::vec3(10.0f, 15020.0f, 30.0f));
}

// A texel takes the average of the 2 by 2 texels from itself across and down, rounded down
TEST(ShadowTexture, TheLevelsAreSoftened)
{
	shadow_texture::Canvas canvas;
	canvas.texels.at(At(5, 5)) = 0x7000;
	canvas.texels.at(At(5, 6)) = 0x7000;
	canvas.texels.at(At(6, 5)) = 0x7000;
	const auto levels = shadow_texture::Levels(canvas, 255);
	// Three of seven over four
	EXPECT_EQ(levels.at(At(5, 5)), 5);
	// Two of seven over four
	EXPECT_EQ(levels.at(At(5, 4)), 3);
	EXPECT_EQ(levels.at(At(4, 5)), 3);
	// One of seven over four
	EXPECT_EQ(levels.at(At(4, 4)), 1);
	EXPECT_EQ(levels.at(At(6, 4)), 1);
	EXPECT_EQ(levels.at(At(6, 6)), 0);
	EXPECT_EQ(levels.at(At(7, 7)), 0);
}

// The ring round the texture stays clear, also where the texels next to it are full
TEST(ShadowTexture, TheRingStaysClear)
{
	shadow_texture::Canvas canvas;
	canvas.texels.fill(0x7000);
	const auto levels = shadow_texture::Levels(canvas, 255);
	for (size_t i = 0; i < k_Texels; ++i)
	{
		EXPECT_EQ(levels.at(At(0, i)), 0);
		EXPECT_EQ(levels.at(At(i, 0)), 0);
		EXPECT_EQ(levels.at(At(k_Texels - 1, i)), 0);
		EXPECT_EQ(levels.at(At(i, k_Texels - 1)), 0);
	}
	EXPECT_EQ(levels.at(At(30, 30)), 7);
}

// The opacity scales the softened level down, rounded down
TEST(ShadowTexture, TheOpacityScalesTheLevels)
{
	shadow_texture::Canvas canvas;
	canvas.texels.fill(0x7000);
	EXPECT_EQ(shadow_texture::Levels(canvas, 128).at(At(10, 10)), 3);
	EXPECT_EQ(shadow_texture::Levels(canvas, 0).at(At(10, 10)), 0);
}

// A texture's point outside the mask gives nothing to the texels that it fills
TEST(ShadowTexture, APointOutsideTheMaskGivesNothing)
{
	tree_shadows::AlphaMask mask {};
	mask.fill(0xF0);
	shadow_texture::Canvas canvas;
	shadow_texture::Fill(canvas, mask,
	                     {shadow_texture::Corner {.column = 4, .row = 4, .u = -200, .v = -200},
	                      shadow_texture::Corner {.column = 10, .row = 4, .u = -200, .v = -200},
	                      shadow_texture::Corner {.column = 4, .row = 10, .u = -200, .v = -200}});
	EXPECT_EQ(canvas.texels.at(At(5, 5)), 0);
	shadow_texture::Fill(canvas, mask,
	                     {shadow_texture::Corner {.column = 4, .row = 4, .u = 10, .v = 10},
	                      shadow_texture::Corner {.column = 10, .row = 4, .u = 10, .v = 10},
	                      shadow_texture::Corner {.column = 4, .row = 10, .u = 10, .v = 10}});
	EXPECT_EQ(canvas.texels.at(At(5, 5)), 0x7000);
}
