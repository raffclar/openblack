/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
