/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

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

namespace
{
/// The hand's own 4444 blend, which moves its skin towards its evil or good variant: each nibble is blended in place
/// and masked back (src/ECS/Systems/Implementations/HandMorph.cpp). The creature's skin does the same job with the
/// nibbles shifted down; the test pins the two equal so that neither drifts from the other.
uint16_t HandBlend4444(uint16_t a, uint16_t b, uint32_t t)
{
	uint32_t out = 0;
	for (const uint32_t mask : {0xFu, 0xF0u, 0xF00u, 0xF000u})
	{
		out |= ((((a & mask) * (255u - t)) + ((b & mask) * t)) / 255u) & mask;
	}
	return static_cast<uint16_t>(out);
}

/// How the hand turns how far along the evil to good axis it is into a weight
uint32_t HandWeight(float axis)
{
	return std::min<uint32_t>(static_cast<uint32_t>(std::abs(axis) * 256.0f), 255u);
}
} // namespace

TEST(CreatureSkin, TheBlendIsTheHandsBlend)
{
	for (uint32_t weight = 0; weight <= 255; ++weight)
	{
		for (const uint16_t base : {0x0000u, 0x0FFFu, 0xF123u, 0x89ABu, 0x0A50u})
		{
			for (const uint16_t other : {0x0000u, 0x0FFFu, 0xFEDCu, 0x3210u, 0x0B07u})
			{
				ASSERT_EQ(BlendTexel(base, other, static_cast<uint8_t>(weight)), HandBlend4444(base, other, weight))
				    << "weight " << weight << " base " << base << " other " << other;
			}
		}
	}
	for (const float axis : {-1.0f, -0.73f, -0.03f, 0.0f, 0.25f, 0.5f, 0.999f, 1.0f})
	{
		EXPECT_EQ(BlendWeight(axis), HandWeight(axis)) << "axis " << axis;
	}
}
