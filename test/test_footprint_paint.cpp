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

#include <gtest/gtest.h>

#include "3D/FootprintPaint.h"

namespace footprint_paint = openblack::footprint_paint;

namespace
{
/// A value in [0, 1] as a 16-bit render target keeps it
float StoredAt16Bits(float value)
{
	return std::round(value * 65535.0f) / 65535.0f;
}
} // namespace

TEST(FootprintPaint, LandHasOneAndSixTenthsTexelsAUnit)
{
	EXPECT_FLOAT_EQ(footprint_paint::k_TexelsPerUnit, 1.6f);
}

TEST(FootprintPaint, CornersSnapToTheNearestLandTexelCorner)
{
	// 10 units is 16 texels; a quarter of a texel either side of it still snaps there
	EXPECT_EQ(footprint_paint::SnapToTexelCorner({10.0f, 10.0f}), glm::vec2(16.0f, 16.0f));
	EXPECT_EQ(footprint_paint::SnapToTexelCorner({10.15f, 9.85f}), glm::vec2(16.0f, 16.0f));
	// Past half a texel it moves to the next corner
	EXPECT_EQ(footprint_paint::SnapToTexelCorner({10.35f, 9.65f}), glm::vec2(17.0f, 15.0f));
	EXPECT_EQ(footprint_paint::SnapToTexelCorner({-0.4f, 2500.0f}), glm::vec2(-1.0f, 4000.0f));
}

TEST(FootprintPaint, EachLandTexelTakesTheNearestFootprintTexel)
{
	EXPECT_EQ(footprint_paint::FootprintTexel(0.0f, 64), 0);
	EXPECT_EQ(footprint_paint::FootprintTexel(0.49f, 64), 0);
	EXPECT_EQ(footprint_paint::FootprintTexel(0.51f, 64), 1);
	EXPECT_EQ(footprint_paint::FootprintTexel(31.6f, 32), 31);
}

TEST(FootprintPaint, CoordinatesOutsideTheFootprintTakeItsEdge)
{
	EXPECT_EQ(footprint_paint::FootprintTexel(-3.0f, 32), 0);
	EXPECT_EQ(footprint_paint::FootprintTexel(32.0f, 32), 31);
	EXPECT_EQ(footprint_paint::FootprintTexel(64.0f, 64), 63);
}

TEST(FootprintPaint, OpaqueTexelsStillLetALittleLandThrough)
{
	EXPECT_EQ(footprint_paint::Coverage(0), 0);
	EXPECT_EQ(footprint_paint::Coverage(8), 128);
	EXPECT_EQ(footprint_paint::Coverage(15), 240);
	// A white footprint fully opaque over black land reaches only 14 of 15
	EXPECT_EQ(footprint_paint::MixChannel(0, 15, 15), 14);
	// Black over white leaves the land at 0: 15 * 15 / 255 rounds down
	EXPECT_EQ(footprint_paint::MixChannel(15, 0, 15), 0);
}

TEST(FootprintPaint, ClearTexelsLeaveTheLandAsItWas)
{
	for (uint8_t land = 0; land < 16; ++land)
	{
		EXPECT_EQ(footprint_paint::MixChannel(land, 15, 0), land);
	}
	EXPECT_EQ(footprint_paint::Paint(0x9ABC, 0x0FFF), 0x9ABC);
}

TEST(FootprintPaint, ChannelsAreRoundedDown)
{
	// Half covered (8 x 16 = 128 of 255): (4 x 127 + 9 x 128) / 255 = 6.5 -> 6
	EXPECT_EQ(footprint_paint::MixChannel(4, 9, 8), 6);
}

TEST(FootprintPaint, PaintingKeepsTheLandsCoastAlpha)
{
	const auto painted = footprint_paint::Paint(0x3000, 0xFFFF);
	EXPECT_EQ(painted & 0xF000, 0x3000);
	EXPECT_EQ(painted & 0x0FFF, 0x0EEE);
}

TEST(FootprintPaint, TheGpuMixMatchesTheIntegerMixForEveryTexel)
{
	for (uint8_t alpha = 0; alpha < 16; ++alpha)
	{
		const float coverage = static_cast<float>(footprint_paint::Coverage(alpha)) / 255.0f;
		for (uint8_t footprint = 0; footprint < 16; ++footprint)
		{
			const float premultiplied = StoredAt16Bits(static_cast<float>(footprint) / 15.0f * coverage);
			const float storedCoverage = StoredAt16Bits(coverage);
			for (uint8_t land = 0; land < 16; ++land)
			{
				EXPECT_EQ(footprint_paint::MixChannelAsDrawn(land, premultiplied, storedCoverage),
				          footprint_paint::MixChannel(land, footprint, alpha))
				    << "land " << +land << " footprint " << +footprint << " alpha " << +alpha;
			}
		}
	}
}
