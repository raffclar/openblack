/******************************************************************************
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

#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/TextureSize.h"

using namespace openblack::graphics;

namespace
{
constexpr std::array<uint16_t, 9> k_Sides {0, 1, 2, 3, 4, 5, 64, 257, 1024};
constexpr std::array<uint16_t, 4> k_Layers {0, 1, 2, 3};
} // namespace

// The dedicated server describes textures with no graphics library. Its sizes must be the sizes that the graphics
// library gives the game, for each format, side and number of layers.
TEST(TextureSize, TheSizesAreTheSizesOfTheGraphicsLibrary)
{
	for (size_t index = 0; index < static_cast<size_t>(TextureFormat::_COUNT); ++index)
	{
		const auto format = static_cast<TextureFormat>(index);
		// The graphics library has no format for alpha alone, so no texture of the game has it
		if (format == TextureFormat::A8)
		{
			continue;
		}
		for (const auto width : k_Sides)
		{
			for (const auto height : k_Sides)
			{
				for (const auto layers : k_Layers)
				{
					bgfx::TextureInfo expected {};
					bgfx::calcTextureSize(expected, width, height, 1, false, false, layers, toBgfx(format));
					const auto actual = CalculateTextureSize(width, height, layers, format);
					SCOPED_TRACE(testing::Message()
					             << "format " << index << " size " << width << "x" << height << " layers " << layers);
					EXPECT_EQ(actual.width, expected.width);
					EXPECT_EQ(actual.height, expected.height);
					EXPECT_EQ(actual.layers, expected.numLayers);
					EXPECT_EQ(actual.bitsPerPixel, expected.bitsPerPixel);
					EXPECT_EQ(actual.storageSize, expected.storageSize);
				}
			}
		}
	}
}

// A texture keeps the format that it was made with
TEST(TextureSize, EachFormatIsTheSameAfterTheGraphicsLibrary)
{
	for (size_t index = 0; index < static_cast<size_t>(TextureFormat::_COUNT); ++index)
	{
		const auto format = static_cast<TextureFormat>(index);
		// The graphics library has no format for alpha alone, so no texture of the game has it
		if (format == TextureFormat::A8)
		{
			continue;
		}
		EXPECT_EQ(fromBgfx(toBgfx(format)), format) << "format " << index;
	}
}
