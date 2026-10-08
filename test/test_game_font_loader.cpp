/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <stdexcept>
#include <tuple>
#include <vector>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Graphics/GameFont.h"
#include "Resources/ResourcesInterface.h"
#include "support/BgfxShutdown.h"

using openblack::resources::GameFontLoader;

namespace
{
// A valid font makes a texture, which needs a renderer; the fake files are rejected before that, and the valid one is
// made with bgfx's renderer of nothing
class GameFontLoaderTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the loader reports a bad font in the graphics log
		if (spdlog::get("graphics") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("graphics");
		}
	}
};
} // namespace

TEST_F(GameFontLoaderTest, TooShortMetricsAreNotAFont)
{
	const std::vector<uint8_t> met(100, 0);
	const std::vector<uint8_t> fnt(16, 0xFF);
	EXPECT_THROW((void)GameFontLoader {}(GameFontLoader::FromBufferTag {}, met, fnt, "fake"), std::runtime_error);
}

TEST_F(GameFontLoaderTest, WrongCellHeightIsNotAFont)
{
	std::vector<uint8_t> met(264, 0);
	met[0] = 7; // the .met starts with the cell height, 80 for the game's fonts
	const std::vector<uint8_t> fnt(16, 0xFF);
	EXPECT_THROW((void)GameFontLoader {}(GameFontLoader::FromBufferTag {}, met, fnt, "fake"), std::runtime_error);
}

TEST_F(GameFontLoaderTest, EmptyBitmapsAreNotAFont)
{
	std::vector<uint8_t> met(264, 0);
	met[0] = 80;
	EXPECT_THROW((void)GameFontLoader {}(GameFontLoader::FromBufferTag {}, met, {}, "fake"), std::runtime_error);
}

namespace
{
template <typename T>
void Append(std::vector<uint8_t>& data, T value)
{
	const auto offset = data.size();
	data.resize(offset + sizeof(T));
	std::memcpy(data.data() + offset, &value, sizeof(T));
}
} // namespace

// A real font makes its texture, here with bgfx's renderer of nothing: what a glyph is can then be read, and the glyph
// at a quarter of its height that small text is drawn from
TEST_F(GameFontLoaderTest, GlyphsCanBeRead)
{
	bgfx::renderFrame(); // single threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const openblack::test::BgfxShutdown bgfxShutdown;

	// An 80 pixel high font of two glyphs: a solid B 6 pixels wide and a space
	constexpr uint32_t k_Height = 80;
	std::vector<uint8_t> met;
	std::vector<uint8_t> fnt;
	Append(met, k_Height);
	met.resize(met.size() + 0x100, 0);
	Append(met, uint32_t {2});
	for (const auto& [character, width, left, ink, right] :
	     {std::tuple {u'B', uint16_t {6}, 1.0f, 6.0f, 2.0f}, std::tuple {u' ', uint16_t {0}, 0.0f, 15.0f, 0.0f}})
	{
		const auto offset = static_cast<uint32_t>(fnt.size());
		// No clear pixels, then every pixel set
		const uint32_t pixels = width * k_Height;
		fnt.push_back(0);
		if (pixels > 0)
		{
			fnt.push_back(0xFF);
			Append(fnt, static_cast<uint16_t>(pixels));
		}
		Append(met, static_cast<uint16_t>(character));
		Append(met, width);
		Append(met, uint32_t {0});
		Append(met, left);
		Append(met, ink);
		Append(met, right);
		Append(met, offset);
		Append(met, static_cast<uint32_t>(fnt.size() - offset));
	}
	const auto font = GameFontLoader {}(GameFontLoader::FromBufferTag {}, met, fnt, "two glyphs");
	ASSERT_NE(font, nullptr);

	// The atlas is 1024 texels across, and as high as its lines of 64 need, in powers of two
	EXPECT_EQ(font->GetAtlasSize(), glm::u16vec2(1024, 64));
	const auto* b = font->FindGlyph(u'B');
	ASSERT_NE(b, nullptr);
	EXPECT_EQ(b->character, u'B');
	EXPECT_EQ(b->width, 6);
	EXPECT_FLOAT_EQ(b->left, 1.0f);
	EXPECT_FLOAT_EQ(b->ink, 6.0f);
	EXPECT_FLOAT_EQ(b->right, 2.0f);
	// Its half height glyph is 3 texels across and 40 down, after the clear texel its slot starts with
	EXPECT_EQ(b->atlasMin, glm::u16vec2(1, 0));
	EXPECT_EQ(b->atlasMax, glm::u16vec2(4, 40));
	// The space's slot comes after the B's 5 texels
	const auto* space = font->FindGlyph(u' ');
	ASSERT_NE(space, nullptr);
	EXPECT_EQ(space->atlasMin, glm::u16vec2(6, 0));
	// A character the font hasn't got has none, as it has no question mark either
	EXPECT_EQ(font->FindGlyph(u'Z'), nullptr);

	// At a quarter of its height the B is 2 texels across, with a clear column either side: the first wholly covered,
	// the second half, as it takes the half height glyph's last column and the clear one after it
	const auto small = font->GetSmallGlyph(*b);
	EXPECT_EQ(small.width, 4);
	EXPECT_EQ(small.alpha.size(), 4u * openblack::graphics::GameFont::SmallGlyph::k_Height);
	EXPECT_EQ(small.At(0, 0), 0);
	EXPECT_EQ(small.At(1, 0), 15);
	EXPECT_EQ(small.At(2, 19), 7);
	EXPECT_EQ(small.At(3, 10), 0);
	EXPECT_EQ(small.At(1, 20), 0);
	EXPECT_EQ(small.At(-1, 0), 0);
	const auto smallSpace = font->GetSmallGlyph(*space);
	EXPECT_EQ(smallSpace.width, 2);
	EXPECT_EQ(smallSpace.At(1, 1), 0);
}
