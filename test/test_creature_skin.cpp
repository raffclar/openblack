/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"

using namespace openblack;
using namespace openblack::creature_skin;

TEST(CreatureSkin, NeutralShowsNoVariant)
{
	EXPECT_EQ(BlendWeight(0.0f), 0);
}

TEST(CreatureSkin, WeightIsTheAxisIn256StepsTruncated)
{
	EXPECT_EQ(BlendWeight(0.5f), 128);
	EXPECT_EQ(BlendWeight(-0.5f), 128);
	// 0.03 * 256 is 7.68
	EXPECT_EQ(BlendWeight(0.03f), 7);
	EXPECT_EQ(BlendWeight(-0.999f), 255);
}

TEST(CreatureSkin, WeightIsCappedAt255)
{
	EXPECT_EQ(BlendWeight(1.0f), 255);
	EXPECT_EQ(BlendWeight(-1.0f), 255);
	EXPECT_EQ(BlendWeight(3.0f), 255);
}

TEST(CreatureSkin, ChannelIsAWeightedMeanRoundedDown)
{
	EXPECT_EQ(BlendChannel(0, 15, 0), 0);
	EXPECT_EQ(BlendChannel(0, 15, 255), 15);
	// (0 * 127 + 15 * 128) / 255 = 7.53
	EXPECT_EQ(BlendChannel(0, 15, 128), 7);
	// (15 * 127 + 0 * 128) / 255 = 7.47
	EXPECT_EQ(BlendChannel(15, 0, 128), 7);
	EXPECT_EQ(BlendChannel(9, 9, 77), 9);
}

TEST(CreatureSkin, TexelBlendsEachChannelOnItsOwn)
{
	// Alpha, red, green and blue nibbles
	constexpr uint16_t k_Base = 0xF0A3;
	constexpr uint16_t k_Other = 0xF5AF;
	EXPECT_EQ(BlendTexel(k_Base, k_Other, 0), k_Base);
	EXPECT_EQ(BlendTexel(k_Base, k_Other, 255), k_Other);
	// Red 0 -> 5: 640 / 255 = 2.5; green 10 -> 10; blue 3 -> 15: (381 + 1920) / 255 = 9.02
	EXPECT_EQ(BlendTexel(k_Base, k_Other, 128), 0xF2A9);
}

TEST(CreatureSkin, SkinsArePairedByTheirPlaceInTheList)
{
	constexpr std::array<uint32_t, 2> k_Base {0xA, 0xB};
	constexpr std::array<uint32_t, 2> k_Evil {0x1, 0x2};
	EXPECT_EQ(PairedSkin(k_Base, k_Evil, 0xA), 0x1u);
	EXPECT_EQ(PairedSkin(k_Base, k_Evil, 0xB), 0x2u);
	EXPECT_FALSE(PairedSkin(k_Base, k_Evil, 0xC).has_value());
	EXPECT_FALSE(PairedSkin(k_Base, std::span<const uint32_t>(k_Evil).first(1), 0xB).has_value());
}

// The place on the skin adds the step towards the third corner to the step towards the second, then the first corner,
// each step rounded to single precision
TEST(CreatureSkin, TheSkinPointAddsTheStepsThenTheFirstCorner)
{
	const glm::vec2 first {0.1f, 0.7f};
	const glm::vec2 second {0.3f, 0.2f};
	const glm::vec2 third {0.9f, 0.4f};
	const float u = 0.35f;
	const float v = 0.15f;
	const auto point = SkinPoint(first, second, third, u, v);
	const float towardsThirdX = (third.x - first.x) * v;
	const float towardsSecondX = (second.x - first.x) * u;
	const float sumX = towardsThirdX + towardsSecondX;
	const float towardsThirdY = (third.y - first.y) * v;
	const float towardsSecondY = (second.y - first.y) * u;
	const float sumY = towardsThirdY + towardsSecondY;
	EXPECT_EQ(point.x, sumX + first.x);
	EXPECT_EQ(point.y, sumY + first.y);
	EXPECT_EQ(SkinPoint(first, second, third, 0.0f, 0.0f), first);
}

namespace
{
std::vector<uint16_t> SkinOfSide(size_t side)
{
	std::vector<uint16_t> texels(side * side);
	for (size_t i = 0; i < texels.size(); ++i)
	{
		texels[i] = static_cast<uint16_t>(0xF000 | (i & 0x0FFF));
	}
	return texels;
}
} // namespace

// Each 4-bit channel of the texel moves up by 4 bits, so full white is 240
TEST(CreatureSkin, TheLidColourMovesEachChannelUpByFourBits)
{
	std::vector<uint16_t> skin(256 * 256, 0x0FFF);
	EXPECT_EQ(ColourAt(skin, {0.5f, 0.5f}), glm::u8vec3(240, 240, 240));
	skin.front() = 0xF8A5;
	EXPECT_EQ(ColourAt(skin, {0.0f, 0.0f}), glm::u8vec3(0x80, 0xA0, 0x50));
}

// The place times the side is cut towards zero, on a skin of 256 or of 128
TEST(CreatureSkin, TheLidColourIsTheTexelAtThePlaceTimesTheSide)
{
	const auto full = SkinOfSide(256);
	const auto texelOf = [](size_t index) {
		const auto texel = static_cast<uint16_t>(0xF000 | (index & 0x0FFF));
		return glm::u8vec3((texel >> 8 & 0xF) << 4, (texel >> 4 & 0xF) << 4, (texel & 0xF) << 4);
	};
	// 0.2 x 256 is 51.2 and 0.1 x 256 is 25.6
	EXPECT_EQ(ColourAt(full, {0.2f, 0.1f}), texelOf((25 * 256) + 51));
	const auto half = SkinOfSide(128);
	// 0.2 x 128 is 25.6 and 0.1 x 128 is 12.8
	EXPECT_EQ(ColourAt(half, {0.2f, 0.1f}), texelOf((12 * 128) + 25));
}

// A place before the first texel by less than one texel gives the first texel. Farther before it, or at or after the
// last texel, the place gives the last texel of the side.
TEST(CreatureSkin, TheLidColourPlaceStopsAtTheLastTexel)
{
	const auto full = SkinOfSide(256);
	const auto texelOf = [&full](size_t x, size_t y) {
		const auto texel = full[(y * 256) + x];
		return glm::u8vec3((texel >> 8 & 0xF) << 4, (texel >> 4 & 0xF) << 4, (texel & 0xF) << 4);
	};
	EXPECT_EQ(ColourAt(full, {-0.001f, 0.0f}), texelOf(0, 0));
	EXPECT_EQ(ColourAt(full, {-0.01f, 0.0f}), texelOf(255, 0));
	EXPECT_EQ(ColourAt(full, {1.0f, 0.5f}), texelOf(255, 128));
	EXPECT_EQ(ColourAt(full, {3.5f, 0.5f}), texelOf(255, 128));
}

TEST(CreatureSkin, ASkinOfAnotherSizeHasNoLidColour)
{
	const std::vector<uint16_t> skin(64 * 64, 0x0FFF);
	EXPECT_FALSE(ColourAt(skin, {0.5f, 0.5f}).has_value());
	EXPECT_FALSE(ColourAt({}, {0.5f, 0.5f}).has_value());
}

// Both lids take the colour under the point that the lid of the first eye with a lid turns towards
TEST(CreatureSkin, TheLidColourComesFromTheFirstEyeWithALid)
{
	creature::CreatureRig::Eyes eyes {};
	EXPECT_EQ(eyes.LidColourPoint(), nullptr);
	eyes.points.at(1).enabled = true;
	eyes.points.at(3).enabled = true;
	EXPECT_EQ(eyes.LidColourPoint(), &eyes.points.at(3));
	eyes.points.at(0).enabled = true;
	EXPECT_EQ(eyes.LidColourPoint(), &eyes.points.at(3));
	eyes.points.at(2).enabled = true;
	EXPECT_EQ(eyes.LidColourPoint(), &eyes.points.at(2));
}
