/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>

#include "3D/HandWaterGlow.h"
#include "Graphics/SeaRows.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
glm::mat4 ViewProjection(glm::vec3 origin, glm::vec3 focus, glm::vec2 size)
{
	// like Camera: lookAt and a perspective of 70 degrees horizontally
	const float aspect = size.x / size.y;
	const float yFov = glm::atan(glm::tan(glm::radians(70.0f) / 2.0f) / aspect) * 2.0f;
	return glm::perspective(yFov, aspect, 1.0f, 65536.0f) * glm::lookAt(origin, focus, glm::vec3(0.0f, 1.0f, 0.0f));
}
} // namespace

TEST(SeaRows, LookingOutToSeaTheFarEdgeIsUnderTheHorizon)
{
	const glm::vec2 size(1600.0f, 900.0f);
	// camera 300 up, looking west (-x), slightly down: the quad's far edge x = -12440 is 13740 away
	const auto range = sea::ComputeScreenRange(ViewProjection({1300, 300, 1900}, {-2000, 220, 1900}, size), size, 1.0f);
	ASSERT_TRUE(range.has_value());
	EXPECT_GT(range->top, 0.0f);
	EXPECT_LT(range->top, size.y * 0.5f);
	EXPECT_FLOAT_EQ(range->bottom, size.y - 1.0f);
	EXPECT_NEAR(1.0f / range->inverseDepthTop, 13740.0f, 50.0f);

	const auto rows = sea::MakeRows(*range);
	EXPECT_EQ(rows.first, static_cast<int>(range->top));
	EXPECT_EQ(rows.count, (static_cast<int>(range->bottom) - rows.first + 2) / 2);
	EXPECT_TRUE(rows.softTop);
}

TEST(SeaRows, LookingUpThereIsNoSea)
{
	const glm::vec2 size(1600.0f, 900.0f);
	const auto range = sea::ComputeScreenRange(ViewProjection({1300, 300, 1900}, {1300, 5000, 1950}, size), size, 1.0f);
	EXPECT_FALSE(range.has_value());
}

TEST(SeaRows, LookingDownTheSeaFillsTheScreen)
{
	const glm::vec2 size(1600.0f, 900.0f);
	const auto range = sea::ComputeScreenRange(ViewProjection({1300, 300, 1900}, {1310, 0, 1910}, size), size, 1.0f);
	ASSERT_TRUE(range.has_value());
	EXPECT_FLOAT_EQ(range->top, 0.0f);
	EXPECT_FLOAT_EQ(range->bottom, size.y - 1.0f);
	EXPECT_FALSE(sea::MakeRows(*range).softTop);
}

TEST(SeaRows, MatchesTheEmulatedOriginal)
{
	// the original's sea range run in an emulator (the x87 and SSE clippers give the same list) for the same camera
	// rows, near plane 1, 1600 x 900
	const glm::vec2 size(1600.0f, 900.0f);
	struct Case
	{
		glm::vec3 origin;
		glm::vec3 focus;
		float top;
		float inverseDepthTop;
		float inverseDepthBottom;
	};
	const Case cases[] = {
	    {{1480, 108, 3923}, {882, 184, 3242}, 551.5968f, 4.6737532e-05f, 0.0028604118f},
	    {{3054, 442, 898}, {4376, -421, -732}, 10.192239f, 5.4945657e-05f, 0.0016845674f},
	};
	for (const auto& c : cases)
	{
		const auto range = sea::ComputeScreenRange(ViewProjection(c.origin, c.focus, size), size, 1.0f);
		ASSERT_TRUE(range.has_value());
		EXPECT_NEAR(range->top, c.top, 0.05f);
		EXPECT_FLOAT_EQ(range->bottom, size.y - 1.0f);
		EXPECT_NEAR(range->inverseDepthTop, c.inverseDepthTop, c.inverseDepthTop * 1e-3f);
		EXPECT_NEAR(range->inverseDepthBottom, c.inverseDepthBottom, c.inverseDepthBottom * 1e-3f);
	}
}

TEST(SeaRows, DriftWrapsAtThePeriod)
{
	sea::Drift drift;
	drift.ScrollRows(1000.0f, sea::k_AmbientWind, 560.0f);
	EXPECT_EQ(drift.GetRowsOffset(), glm::vec2(0.0f));
	// with a wind of (1, 0): -1000/330 per second, wrapped by trunc(off / P) * P
	for (int i = 0; i < 100; ++i)
	{
		drift.ScrollRows(1000.0f, glm::vec2(1.0f, 0.0f), 2.0f);
	}
	EXPECT_GT(drift.GetRowsOffset().x, -2.0f);
	EXPECT_LE(drift.GetRowsOffset().x, 0.0f);
}

TEST(HandWaterGlow, ColourAndIntensity)
{
	// the base colour mean of 90 gives (120 - 90) / 15 = 2 -> 1
	EXPECT_FLOAT_EQ(hand_water_glow::Intensity(glm::vec3(90.0f / 255.0f)), 1.0f);
	EXPECT_FLOAT_EQ(hand_water_glow::Intensity(glm::vec3(120.0f / 255.0f)), 0.0f);
	EXPECT_FLOAT_EQ(hand_water_glow::Intensity(glm::vec3(111.0f / 255.0f)), 0.6f);
	// row 6 = (200, 100, 0): (200 + 55 / 4, 100 + 28 / 4, 0 + 64 / 4) = (213, 107, 16), alpha 190
	EXPECT_EQ(hand_water_glow::Colour(0xFFC86400u, 1.0f), 0xBED56B10u);
	// G over 128 and B over 64 move down: (255, 200, 100) -> (255, 182, 91); alpha 0.5 * 190 truncated = 95
	EXPECT_EQ(hand_water_glow::Colour(0xFFFFC864u, 0.5f), 0x5FFFB65Bu);
}
