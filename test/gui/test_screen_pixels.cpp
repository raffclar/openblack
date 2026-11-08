/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

#include "Gui/ScreenPixels.h"

using namespace openblack::gui::screen_pixels;

TEST(ScreenPixels, ACornerGoesHalfAPixelOnToTheModernCentreOfItsPixel)
{
	// The game's pixel 10 has its centre at 10. The modern renderer has the centre of the same pixel at 10.5.
	const auto corner = ToTarget({10.0f, 20.0f}, Corners::AsGiven);
	EXPECT_FLOAT_EQ(corner.x, 10.5f);
	EXPECT_FLOAT_EQ(corner.y, 20.5f);
}

TEST(ScreenPixels, TheBoxDrawMovesItsCornersOneMoreHalfPixel)
{
	// The box draw moves the corners half a pixel right and down on the game's screen, so a whole pixel in the target
	const auto corner = ToTarget({10.0f, 20.0f}, Corners::BoxDraw);
	EXPECT_FLOAT_EQ(corner.x, 11.0f);
	EXPECT_FLOAT_EQ(corner.y, 21.0f);
	EXPECT_FLOAT_EQ(CornerShift(Corners::AsGiven), 0.0f);
	EXPECT_FLOAT_EQ(CornerShift(Corners::BoxDraw), 0.5f);
}

TEST(ScreenPixels, AQuadOfWholeCornersFillsTheSamePixelsAndReadsTheTexelEdge)
{
	// A quad from 0 to 4 over a texture of 4 texels. The modern renderer fills a pixel when its centre is inside.
	const auto min = ToTarget({0.0f, 0.0f}, Corners::AsGiven);
	const auto max = ToTarget({4.0f, 0.0f}, Corners::AsGiven);
	EXPECT_FLOAT_EQ(min.x, 0.5f);
	EXPECT_FLOAT_EQ(max.x, 4.5f);
	// Pixel 0 (centre 0.5) is the first pixel that it fills, and pixel 3 (centre 3.5) the last
	const float firstU = (0.5f - min.x) / (max.x - min.x);
	const float lastU = (3.5f - min.x) / (max.x - min.x);
	// Each pixel reads the left edge of its texel, as in the game
	EXPECT_FLOAT_EQ(firstU * 4.0f, 0.0f);
	EXPECT_FLOAT_EQ(lastU * 4.0f, 3.0f);
}

TEST(ScreenPixels, AQuadOfTheBoxDrawFillsThePixelsAfterItsCornersAndReadsTheTexelCentres)
{
	// The game's box from 0 to 4 fills the pixels 1 to 4
	const auto min = ToTarget({0.0f, 0.0f}, Corners::BoxDraw);
	const auto max = ToTarget({4.0f, 0.0f}, Corners::BoxDraw);
	EXPECT_FLOAT_EQ(min.x, 1.0f);
	EXPECT_FLOAT_EQ(max.x, 5.0f);
	const float firstU = (1.5f - min.x) / (max.x - min.x);
	const float lastU = (4.5f - min.x) / (max.x - min.x);
	EXPECT_FLOAT_EQ(firstU * 4.0f, 0.5f);
	EXPECT_FLOAT_EQ(lastU * 4.0f, 3.5f);
}

TEST(ScreenPixels, AClipShowsThePixelsWhoseGameCentreIsOnOrAfterItsEdge)
{
	EXPECT_EQ(ClipEdge(10.0f, Corners::AsGiven, 100), 10);
	EXPECT_EQ(ClipEdge(10.25f, Corners::AsGiven, 100), 11);
	EXPECT_EQ(ClipEdge(9.75f, Corners::AsGiven, 100), 10);
	// The clip of the box draw moves with its corners
	EXPECT_EQ(ClipEdge(10.0f, Corners::BoxDraw, 100), 11);
	EXPECT_EQ(ClipEdge(9.5f, Corners::BoxDraw, 100), 10);
	// The edge stays on the screen
	EXPECT_EQ(ClipEdge(-3.0f, Corners::AsGiven, 100), 0);
	EXPECT_EQ(ClipEdge(130.0f, Corners::BoxDraw, 100), 100);
}

TEST(ScreenPixels, ALineAlongARowFillsTheRowFromThePixelAfterItsLeftEndToItsRightEnd)
{
	// The game's line from 10 to 14 on row 20 runs on the boundary under row 20, from 10.5 to 14.5
	const auto span = LinePixels({10, 20}, {14, 20});
	ASSERT_TRUE(span.has_value());
	EXPECT_EQ(span->first, glm::ivec2(11, 20));
	EXPECT_EQ(span->last, glm::ivec2(14, 20));
	// The way that the line runs does not change its pixels
	const auto back = LinePixels({14, 20}, {10, 20});
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->first, span->first);
	EXPECT_EQ(back->last, span->last);
}

TEST(ScreenPixels, ALineAlongAColumnFillsTheColumnFromThePixelUnderItsTopEndToItsBottomEnd)
{
	const auto span = LinePixels({7, 30}, {7, 25});
	ASSERT_TRUE(span.has_value());
	EXPECT_EQ(span->first, glm::ivec2(7, 26));
	EXPECT_EQ(span->last, glm::ivec2(7, 30));
}

TEST(ScreenPixels, ALineOfNoLengthOrNotAlongARowOrAColumnFillsNoPixel)
{
	EXPECT_FALSE(LinePixels({3, 3}, {3, 3}).has_value());
	EXPECT_FALSE(LinePixels({3, 3}, {5, 6}).has_value());
}

TEST(ScreenPixels, ABoxOfFourLinesLeavesItsTopLeftCornerEmpty)
{
	// The four lines of a bevel round the pixels 0 to 4: top, bottom, left, right
	const std::array spans {LinePixels({0, 0}, {4, 0}), LinePixels({4, 4}, {0, 4}), LinePixels({0, 4}, {0, 0}),
	                        LinePixels({4, 0}, {4, 4})};
	const auto fills = [&spans](glm::ivec2 pixel) {
		return std::ranges::any_of(spans, [pixel](const auto& span) {
			return span.has_value() && pixel.x >= span->first.x && pixel.x <= span->last.x && pixel.y >= span->first.y &&
			       pixel.y <= span->last.y;
		});
	};
	EXPECT_FALSE(fills({0, 0}));
	EXPECT_TRUE(fills({4, 0}));
	EXPECT_TRUE(fills({0, 4}));
	EXPECT_TRUE(fills({4, 4}));
}

TEST(PixelCentres, TheProjectionMovesAPointHalfAPixelRightAndDownOnItsTarget)
{
	// A plain projection with w = z: the centre of the view goes to the centre of the clip space
	glm::mat4 projection(1.0f);
	projection[2][3] = 1.0f;
	projection[3][3] = 0.0f;
	const glm::vec2 target {800.0f, 600.0f};
	const auto moved = openblack::graphics::pixel_centres::OnTarget(projection, target);
	const glm::vec4 point {0.0f, 0.0f, 5.0f, 1.0f};
	const auto clip = moved * point;
	// To pixels across the target, from the left and from the top
	const float x = (clip.x / clip.w + 1.0f) * 0.5f * target.x;
	const float y = (1.0f - clip.y / clip.w) * 0.5f * target.y;
	EXPECT_FLOAT_EQ(x, 400.5f);
	EXPECT_FLOAT_EQ(y, 300.5f);
	// The depth stays as it was
	EXPECT_FLOAT_EQ(clip.z, (projection * point).z);
	EXPECT_FLOAT_EQ(clip.w, (projection * point).w);
}

TEST(PixelCentres, TheMoveIsTheSameDistanceInPixelsAtEachDepth)
{
	glm::mat4 projection(1.0f);
	projection[2][3] = 1.0f;
	projection[3][3] = 0.0f;
	const glm::vec2 target {1024.0f, 768.0f};
	const auto moved = openblack::graphics::pixel_centres::OnTarget(projection, target);
	for (const float depth : {1.0f, 10.0f, 1000.0f})
	{
		const glm::vec4 point {0.3f * depth, -0.2f * depth, depth, 1.0f};
		const auto before = projection * point;
		const auto after = moved * point;
		EXPECT_NEAR((after.x / after.w - before.x / before.w) * 0.5f * target.x, 0.5f, 1e-4f);
		EXPECT_NEAR((before.y / before.w - after.y / after.w) * 0.5f * target.y, 0.5f, 1e-4f);
	}
}

TEST(PixelCentres, AViewWithNoPixelsKeepsItsProjection)
{
	const glm::mat4 projection(2.0f);
	EXPECT_EQ(openblack::graphics::pixel_centres::OnTarget(projection, {0.0f, 0.0f}), projection);
}

TEST(PixelCentres, TheInterfaceAndThe3DViewsUseTheSameDistance)
{
	EXPECT_FLOAT_EQ(k_PixelCentre, openblack::graphics::pixel_centres::k_GameToModern);
}
