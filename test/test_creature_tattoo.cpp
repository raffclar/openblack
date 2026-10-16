/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureTattoo.h"
#include "Creature/TattooPictures.h"

using namespace openblack;
using namespace openblack::creature_tattoo;

namespace
{
constexpr uint32_t k_AtlasSize = 256;
constexpr size_t k_SkinTexels = static_cast<size_t>(k_SkinSize) * k_SkinSize;

/// An atlas whose design cells hold a level in the top four bits of the blue: each cell's own index, or a pattern
std::vector<std::array<uint8_t, 3>> FakeAtlas()
{
	std::vector<std::array<uint8_t, 3>> atlas(static_cast<size_t>(k_AtlasSize) * k_AtlasSize, {0, 0, 0});
	for (uint32_t y = 0; y < k_AtlasSize; ++y)
	{
		for (uint32_t x = 0; x < k_AtlasSize; ++x)
		{
			const auto design = ((y / k_DesignSize) * k_DesignsPerRow) + (x / k_DesignSize);
			// Red and green are ignored; the blue's low bits too
			atlas[(y * k_AtlasSize) + x] = {200, 100, static_cast<uint8_t>((design << 4u) | 0x7u)};
		}
	}
	return atlas;
}

/// A mask of levels counting along its texels
Mask Counting(uint32_t size)
{
	Mask mask {.size = size, .levels = {}};
	for (uint32_t i = 0; i < size * size; ++i)
	{
		mask.levels.push_back(static_cast<uint8_t>(i));
	}
	return mask;
}

Design Solid(uint8_t level)
{
	Design design {};
	for (size_t i = 0; i < design.size(); ++i)
	{
		const auto size = k_DesignSize >> i;
		design.at(i) = {.size = size, .levels = std::vector<uint8_t>(static_cast<size_t>(size) * size, level)};
	}
	return design;
}
} // namespace

TEST(CreatureTattoo, DesignsComeFromTheBlueOfTheirCells)
{
	const auto atlas = FakeAtlas();
	const auto design = DesignFromAtlas(atlas, k_AtlasSize, 6, false);
	ASSERT_EQ(design.front().size, k_DesignSize);
	EXPECT_EQ(design.front().At(0, 0), 6);
	EXPECT_EQ(design.front().At(63, 63), 6);
	// Each smaller mask is half the size
	EXPECT_EQ(design.at(1).size, 32u);
	EXPECT_EQ(design.back().size, 4u);
	EXPECT_EQ(design.back().At(3, 3), 6);
}

TEST(CreatureTattoo, SmallerMasksAverageSquaresOfFourRoundingDown)
{
	std::vector<std::array<uint8_t, 3>> atlas(static_cast<size_t>(k_AtlasSize) * k_AtlasSize, {0, 0, 0});
	// The first design's top left square of four: 15, 0, 0 and 2, which average 4.25
	atlas[0] = {0, 0, 0xF0};
	atlas[k_AtlasSize + 1] = {0, 0, 0x20};
	const auto design = DesignFromAtlas(atlas, k_AtlasSize, 0, false);
	EXPECT_EQ(design.at(1).At(0, 0), 4);
}

TEST(CreatureTattoo, MaskSizeFollowsTheTattoosSize)
{
	EXPECT_EQ(MaskLevel(1.0f), 0u);
	EXPECT_EQ(MaskLevel(0.75f), 0u);
	// 0.74 * 64 is 47.36
	EXPECT_EQ(MaskLevel(0.74f), 1u);
	EXPECT_EQ(MaskLevel(0.375f), 1u);
	EXPECT_EQ(MaskLevel(0.25f), 2u);
	EXPECT_EQ(MaskLevel(0.1f), 3u);
	EXPECT_EQ(MaskLevel(0.05f), 4u);
	// Bigger than the skin is the skin
	EXPECT_EQ(MaskLevel(3.0f), 0u);
}

TEST(CreatureTattoo, MasksTurnAndFlip)
{
	const auto mask = Counting(2);
	// 0 1 / 2 3 turned a quarter clockwise is 2 0 / 3 1
	const auto turned = Oriented(mask, 1, false);
	EXPECT_EQ(turned.levels, (std::vector<uint8_t> {2, 0, 3, 1}));
	EXPECT_EQ(Oriented(mask, 2, false).levels, (std::vector<uint8_t> {3, 2, 1, 0}));
	EXPECT_EQ(Oriented(mask, 3, false).levels, (std::vector<uint8_t> {1, 3, 0, 2}));
	// Flipped left to right after the turn
	EXPECT_EQ(Oriented(mask, 0, true).levels, (std::vector<uint8_t> {1, 0, 3, 2}));
	EXPECT_EQ(Oriented(mask, 1, true).levels, (std::vector<uint8_t> {0, 2, 1, 3}));
}

TEST(CreatureTattoo, TexelsMoveTowardsTheColourIn15ths)
{
	// Full level: the colour's top four bits, opaque
	EXPECT_EQ(PaintTexel(0x0123, 15, {0xA5, 0xB6, 0xC7}), 0xFABC);
	// None: the skin, made opaque
	EXPECT_EQ(PaintTexel(0x0123, 0, {0xA5, 0xB6, 0xC7}), 0xF123);
	// Level 5 of black over 0xF on every channel: 0xF * 10 / 15 = 10
	EXPECT_EQ(PaintTexel(0x0FFF, 5, {0, 0, 0}), 0xFAAA);
	// Level 5 of white over black: 15 * 5 / 15 = 5
	EXPECT_EQ(PaintTexel(0x0000, 5, {0xFF, 0xFF, 0xFF}), 0xF555);
	// Rounded down: (3 * 8 + 15 * 7) / 15 = 8.6
	EXPECT_EQ(PaintTexel(0x0333, 7, {0xFF, 0xFF, 0xFF}), 0xF888);
}

TEST(CreatureTattoo, TattoosSpanTheirSizeOfTheLargestMask)
{
	std::vector<uint16_t> skin(k_SkinTexels, 0x0000);
	// Half size: drawn from the 32 texel mask at twice its size, 32 texels across, centred on 100, 120
	const Site site {.enabled = true, .u = 100, .v = 120, .skin = 0, .size = 0.5f, .mirror = false, .rotation = 0};
	Paint(skin, Solid(15), {0xFF, 0xFF, 0xFF}, site);
	const auto at = [&skin](int x, int y) { return skin.at(static_cast<size_t>((y * 256) + x)); };
	EXPECT_EQ(at(84, 104), 0xFFFF);
	EXPECT_EQ(at(115, 135), 0xFFFF);
	EXPECT_EQ(at(83, 104), 0x0000);
	EXPECT_EQ(at(116, 104), 0x0000);
	EXPECT_EQ(at(84, 136), 0x0000);
}

TEST(CreatureTattoo, TattoosOverAnEdgeArentPainted)
{
	std::vector<uint16_t> skin(k_SkinTexels, 0x0000);
	// 64 texels across, centred 20 texels from the left edge
	const Site site {.enabled = true, .u = 20, .v = 128, .skin = 0, .size = 1.0f, .mirror = false, .rotation = 0};
	Paint(skin, Solid(15), {0xFF, 0xFF, 0xFF}, site);
	EXPECT_TRUE(std::ranges::all_of(skin, [](uint16_t texel) { return texel == 0; }));
}

TEST(CreatureTattoo, PaletteColoursBrightenAndDarken)
{
	std::vector<std::array<uint8_t, 3>> palette(static_cast<size_t>(k_PaletteColumns) * k_PaletteRows, {0, 0, 0});
	palette.at((3 * k_PaletteColumns) + 2) = {100, 50, 200};
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.5f), glm::u8vec3(100, 50, 200));
	// All the way to white, and to black
	EXPECT_EQ(PaletteColour(palette, 2, 3, 1.0f), glm::u8vec3(255, 255, 255));
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.0f), glm::u8vec3(0, 0, 0));
	// A quarter: half the colour
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.25f), glm::u8vec3(50, 25, 100));
	// Three quarters: half way to white, rounded down
	EXPECT_EQ(PaletteColour(palette, 2, 3, 0.75f), glm::u8vec3(177, 152, 227));
}

TEST(CreatureTattoo, DesignsGoInTheSlotAsTheEditorPutsThem)
{
	Slots slots {};
	// The first empty slot
	EXPECT_EQ(SlotFor(slots, 2, 5), 0u);
	slots[0] = {.design = 5, .site = 2, .colour = {}};
	slots[1] = {.design = 7, .site = 3, .colour = {}};
	// The same design on the same site recolours it
	EXPECT_EQ(SlotFor(slots, 2, 5), 0u);
	EXPECT_EQ(SlotFor(slots, 4, 1), 2u);
	// Every slot full: the one on the same site
	for (size_t i = 2; i < slots.size(); ++i)
	{
		slots.at(i) = {.design = 0, .site = static_cast<uint8_t>(i), .colour = {}};
	}
	EXPECT_EQ(SlotFor(slots, 3, 9), 1u);
	slots[1].site = 2;
	// Nowhere for it when every slot holds another site's tattoo
	EXPECT_FALSE(SlotFor(slots, 1, 9).has_value());
}

TEST(CreatureTattoo, AtLowDetailOddRowsReadAnotherDesign)
{
	const auto atlas = FakeAtlas();
	// The even rows read the design itself, at half its size
	const auto fifth = DesignFromAtlas(atlas, k_AtlasSize, 4, true);
	EXPECT_EQ(fifth.front().At(0, 0), 4);
	EXPECT_EQ(fifth.front().At(63, 62), 4);
	// The odd rows read 64 texels further on in the half atlas: two designs to the right
	EXPECT_EQ(fifth.front().At(0, 1), 6);
	// From the third column on, the odd rows go on to the next row of the half atlas: two designs to the left
	EXPECT_EQ(DesignFromAtlas(atlas, k_AtlasSize, 6, true).front().At(0, 1), 4);
	// The last row of the last two designs is after the end of the half atlas
	const auto last = DesignFromAtlas(atlas, k_AtlasSize, 15, true);
	EXPECT_EQ(last.front().At(0, 62), 15);
	EXPECT_EQ(last.front().At(0, 63), 0);
	EXPECT_EQ(DesignFromAtlas(atlas, k_AtlasSize, 14, true).front().At(10, 63), 0);
	EXPECT_EQ(DesignFromAtlas(atlas, k_AtlasSize, 13, true).front().At(10, 63), 15);
}

TEST(CreatureTattoo, AtLowDetailEachTexelOfTheHalfAtlasShowsTwoTimes)
{
	std::vector<std::array<uint8_t, 3>> atlas(static_cast<size_t>(k_AtlasSize) * k_AtlasSize, {0, 0, 0});
	// The half atlas keeps the even texels of the even rows only
	atlas[2] = {0, 0, 0x30};
	atlas[3] = {0, 0, 0xF0};
	atlas[k_AtlasSize + 2] = {0, 0, 0xF0};
	const auto design = DesignFromAtlas(atlas, k_AtlasSize, 0, true);
	EXPECT_EQ(design.front().At(1, 0), 0);
	EXPECT_EQ(design.front().At(2, 0), 3);
	EXPECT_EQ(design.front().At(3, 0), 3);
	EXPECT_EQ(design.front().At(4, 0), 0);
}

TEST(CreatureTattoo, APictureTakesTheGreyNearestToEachColour)
{
	const std::vector<std::array<uint8_t, 3>> image {{255, 0, 0}, {0, 0, 0}, {128, 128, 129}, {100, 50, 200}};
	const auto picture = PictureFromImage(image, 2, 2);
	// Each channel goes to the middle of its step of eight, then the grey is the mean, rounded
	EXPECT_EQ(picture.at(0), 87);
	EXPECT_EQ(picture.at(1), 4);
	EXPECT_EQ(picture.at(k_DesignSize), 132);
	EXPECT_EQ(picture.at(k_DesignSize + 1), 119);
	// The texels that the image does not have are black
	EXPECT_EQ(picture.at(2), 0);
	EXPECT_EQ(picture.at(2 * k_DesignSize), 0);
}

TEST(CreatureTattoo, APictureTakesTheTopLeftOfALargerImage)
{
	std::vector<std::array<uint8_t, 3>> image(100 * 80, {0, 0, 0});
	image[(63 * 100) + 63] = {255, 255, 255};
	image[64] = {255, 255, 255};
	const auto picture = PictureFromImage(image, 100, 80);
	EXPECT_EQ(picture.back(), 252);
	EXPECT_EQ(picture.at(63), 4);
}

TEST(CreatureTattoo, APictureTakesThePlaceOfItsDesign)
{
	auto atlas = FakeAtlas();
	Picture picture {};
	picture.fill(0x90);
	PutPicture(atlas, k_AtlasSize, 5, picture);
	EXPECT_EQ(DesignFromAtlas(atlas, k_AtlasSize, 5, false).front().At(10, 10), 9);
	EXPECT_EQ(DesignFromAtlas(atlas, k_AtlasSize, 4, false).front().At(10, 10), 4);
}

TEST(CreatureTattoo, TheBlockKeepsEachPictureThatATattooUsesOneTime)
{
	Pictures pictures {};
	Picture three {};
	for (size_t i = 0; i < three.size(); ++i)
	{
		three.at(i) = static_cast<uint8_t>(i);
	}
	pictures.at(3) = three;
	pictures.at(9) = Picture {};
	Slots slots {};
	slots.at(0) = {.design = 3, .site = 1, .colour = {}};
	slots.at(2) = {.design = 3, .site = 5, .colour = {}};
	slots.at(4) = {.design = 7, .site = 0, .colour = {}};
	// An empty slot uses no picture
	slots.at(5) = {.design = 9, .site = k_NoSite, .colour = {}};
	const auto block = PicturesBlock(slots, pictures);
	ASSERT_EQ(block.size(), 0x828u);
	EXPECT_EQ(block.at(0), 0x28);
	EXPECT_EQ(block.at(1), 0x08);
	EXPECT_EQ(block.at(4), 1);
	EXPECT_EQ(block.at(5), 0b101);
	EXPECT_EQ(block.at(8), 0x28);
	EXPECT_EQ(block.at(16), 0x28);
	// Two levels to a byte, the first in the low four bits
	EXPECT_EQ(block.at(0x28 + 8), 0x11);
	const auto design = DesignInBlock(block, 2);
	ASSERT_TRUE(design.has_value());
	EXPECT_EQ(design->front().At(16, 0), 1);
	EXPECT_EQ(design->front().At(17, 0), 1);
	EXPECT_EQ(design->front().At(0, 1), 4);
	EXPECT_FALSE(DesignInBlock(block, 4).has_value());
	EXPECT_FALSE(DesignInBlock(block, 5).has_value());
}

TEST(CreatureTattoo, ABlockWithoutPicturesHasItsHeadOnly)
{
	const auto block = PicturesBlock(Slots {}, Pictures {});
	ASSERT_EQ(block.size(), 0x28u);
	EXPECT_EQ(block.at(4), 0);
	EXPECT_EQ(block.at(5), 0);
	EXPECT_FALSE(DesignInBlock(block, 0).has_value());
	// A block too short for the picture at its offset gives none
	auto broken = PicturesBlock(Slots {}, Pictures {});
	broken.at(5) = 1;
	broken.at(8) = 0x28;
	EXPECT_FALSE(DesignInBlock(broken, 0).has_value());
}

namespace
{
/// A bitmap file of 64 by 64 texels, 24 bits a texel, black but for its top left texel
std::vector<uint8_t> Bitmap(std::array<uint8_t, 3> topLeft)
{
	constexpr uint32_t k_Side = 64;
	constexpr uint32_t k_Header = 54;
	const uint32_t size = k_Header + (k_Side * k_Side * 3);
	std::vector<uint8_t> bytes(size, 0);
	const auto word = [&bytes](size_t at, uint32_t value, size_t length) {
		for (size_t i = 0; i < length; ++i)
		{
			bytes.at(at + i) = static_cast<uint8_t>(value >> (8 * i));
		}
	};
	bytes.at(0) = 'B';
	bytes.at(1) = 'M';
	word(2, size, 4);
	word(10, k_Header, 4);
	word(14, 40, 4);
	word(18, k_Side, 4);
	word(22, k_Side, 4);
	word(26, 1, 2);
	word(28, 24, 2);
	// The rows go from the bottom up, and each texel is blue, green, red
	const size_t top = k_Header + ((k_Side - 1) * k_Side * 3);
	bytes.at(top) = topLeft[2];
	bytes.at(top + 1) = topLeft[1];
	bytes.at(top + 2) = topLeft[0];
	return bytes;
}
} // namespace

TEST(TattooPictures, ABitmapGivesAPicture)
{
	const auto picture = tattoo_pictures::FromBitmap(Bitmap({255, 255, 255}));
	ASSERT_TRUE(picture.has_value());
	EXPECT_EQ(picture->at(0), 252);
	EXPECT_EQ(picture->at(1), 4);
	// A small file is not a picture
	EXPECT_FALSE(tattoo_pictures::FromBitmap(std::vector<uint8_t>(100, 0)).has_value());
	EXPECT_FALSE(tattoo_pictures::FromBitmap(std::vector<uint8_t>(5000, 0)).has_value());
}

TEST(TattooPictures, ADesignWithoutAFileHasNoPicture)
{
	EXPECT_EQ(tattoo_pictures::FileName(0), "Symbol1.bmp");
	EXPECT_EQ(tattoo_pictures::FileName(15), "Symbol16.bmp");
	Pictures pictures {};
	Picture old {};
	old.fill(7);
	pictures.at(4) = old;
	const std::map<std::string, std::vector<uint8_t>> files {{"Symbol2.bmp", Bitmap({255, 255, 255})}};
	tattoo_pictures::Read(pictures, [&files](const std::string& file) -> std::optional<std::vector<uint8_t>> {
		const auto found = files.find(file);
		return found != files.end() ? std::optional(found->second) : std::nullopt;
	});
	ASSERT_TRUE(pictures.at(1).has_value());
	EXPECT_EQ(pictures.at(1)->at(0), 252);
	// The picture of the last profile goes when the new profile has no file for it
	EXPECT_FALSE(pictures.at(4).has_value());
	EXPECT_FALSE(pictures.at(0).has_value());
}
