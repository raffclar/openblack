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
#include <utility>

#include <glm/common.hpp>
#include <gtest/gtest.h>

#include "Gui/ScreenFit.h"

using namespace openblack::gui;

namespace
{
/// The game's own bars for a 16:9 picture on a screen at least that tall: half of what it loses, worked out in single
/// precision and rounded towards zero
int32_t VanillaFilmBar(int32_t width, int32_t height)
{
	const auto lost = static_cast<int32_t>(static_cast<float>(height) - static_cast<float>(width) * 0.5625f);
	return lost / 2;
}
} // namespace

TEST(ScreenFit, FourByThreeScreensAreFilled)
{
	for (const auto& [width, height] : std::array<std::pair<int32_t, int32_t>, 6> {
	         {{640, 480}, {800, 600}, {1024, 768}, {1152, 864}, {1280, 960}, {1600, 1200}}})
	{
		EXPECT_EQ(FitInside(k_FourByThree, width, height), (ScreenRect {.x = 0, .y = 0, .width = width, .height = height}))
		    << width << "x" << height;
	}
}

TEST(ScreenFit, WiderScreensHaveBarsAtTheSides)
{
	EXPECT_EQ(FitInside(k_FourByThree, 1920, 1080), (ScreenRect {.x = 240, .y = 0, .width = 1440, .height = 1080}));
	EXPECT_EQ(FitInside(k_FourByThree, 3440, 1440), (ScreenRect {.x = 760, .y = 0, .width = 1920, .height = 1440}));
	EXPECT_EQ(FitInside(k_FourByThree, 5120, 1440), (ScreenRect {.x = 1600, .y = 0, .width = 1920, .height = 1440}));
	EXPECT_EQ(FitInside(k_SixteenByNine, 5120, 1440), (ScreenRect {.x = 1280, .y = 0, .width = 2560, .height = 1440}));
	EXPECT_EQ(FitInside(k_SixteenByNine, 3440, 1440), (ScreenRect {.x = 440, .y = 0, .width = 2560, .height = 1440}));
	// An odd number of columns left over: each bar takes half, rounded down, and the picture keeps the odd one
	EXPECT_EQ(FitInside(k_FourByThree, 1441, 1080), (ScreenRect {.x = 0, .y = 0, .width = 1441, .height = 1080}));
	EXPECT_EQ(FitInside(k_FourByThree, 1443, 1080), (ScreenRect {.x = 1, .y = 0, .width = 1441, .height = 1080}));
}

TEST(ScreenFit, TallerScreensHaveBarsAboveAndBelow)
{
	EXPECT_EQ(FitInside(k_FourByThree, 1280, 1024), (ScreenRect {.x = 0, .y = 32, .width = 1280, .height = 960}));
	EXPECT_EQ(FitInside(k_SixteenByNine, 640, 480), (ScreenRect {.x = 0, .y = 60, .width = 640, .height = 360}));
	EXPECT_EQ(FitInside(k_SixteenByNine, 1600, 1200), (ScreenRect {.x = 0, .y = 150, .width = 1600, .height = 900}));
	// A portrait screen
	EXPECT_EQ(FitInside(k_FourByThree, 1080, 1920), (ScreenRect {.x = 0, .y = 555, .width = 1080, .height = 810}));
}

TEST(ScreenFit, FilmBarsMatchTheGameOnScreensAtLeastSixteenByNineTall)
{
	for (int32_t width = 320; width <= 4096; width += 7)
	{
		for (int32_t height = (width * 9 + 15) / 16; height <= width * 2; height += 13)
		{
			const auto bar = VanillaFilmBar(width, height);
			ASSERT_EQ(FitInside(k_SixteenByNine, width, height),
			          (ScreenRect {.x = 0, .y = bar, .width = width, .height = height - (2 * bar)}))
			    << width << "x" << height;
		}
	}
}

TEST(ScreenFit, TheFitKeepsTheShapeWithinAPixel)
{
	for (const auto& aspect : {k_FourByThree, k_SixteenByNine, Aspect {.width = 3, .height = 2}})
	{
		for (int32_t width = 100; width <= 6000; width += 97)
		{
			for (int32_t height = 100; height <= 3000; height += 89)
			{
				const auto fit = FitInside(aspect, width, height);
				ASSERT_GE(fit.x, 0);
				ASSERT_GE(fit.y, 0);
				ASSERT_LE(fit.x + fit.width, width);
				ASSERT_LE(fit.y + fit.height, height);
				// Centred: the bars differ by at most a pixel
				ASSERT_LE((width - fit.width - fit.x) - fit.x, 1);
				ASSERT_LE((height - fit.height - fit.y) - fit.y, 1);
				// One side touches the screen's edges
				ASSERT_TRUE(fit.width == width || fit.height == height);
				// The shape is kept to within the rounding of a pixel and a half
				const auto expected = static_cast<double>(fit.width) * aspect.height / aspect.width;
				ASSERT_NEAR(static_cast<double>(fit.height), expected, 1.5 * aspect.height / aspect.width + 1.5)
				    << width << "x" << height;
			}
		}
	}
}

TEST(ScreenFit, EmptyShapesOrScreensFillTheScreen)
{
	EXPECT_EQ(FitInside(Aspect {.width = 0, .height = 3}, 800, 600),
	          (ScreenRect {.x = 0, .y = 0, .width = 800, .height = 600}));
	EXPECT_EQ(FitInside(k_FourByThree, 0, 0), (ScreenRect {}));
}

TEST(ScreenFit, TheReferenceScaleIsTheDefaultScreensShareOfThe4By3Middle)
{
	EXPECT_FLOAT_EQ(ReferenceScale(800, 600), 1.0f);
	EXPECT_FLOAT_EQ(ReferenceScale(1600, 1200), 2.0f);
	// Taller than 4:3: by the width
	EXPECT_FLOAT_EQ(ReferenceScale(1280, 1024), 1.6f);
	// Wider: by the 4:3 width as tall as the screen
	EXPECT_FLOAT_EQ(ReferenceScale(1920, 1080), 1.8f);
	EXPECT_FLOAT_EQ(ReferenceScale(2560, 1440), 2.4f);
	EXPECT_FLOAT_EQ(ReferenceScale(3440, 1440), 2.4f);
	EXPECT_FLOAT_EQ(ReferenceScale(5120, 1440), 2.4f);
	EXPECT_EQ(ScaledPixels(30, 1.0f), 30);
	EXPECT_EQ(ScaledPixels(15, 1.8f), 27);
	EXPECT_EQ(ScaledPixels(1, 2.4f), 2);
	// Never less than a pixel
	EXPECT_EQ(ScaledPixels(1, 0.3f), 1);
}

TEST(ScreenFit, TheFrontEndIsAPixelAUnitOnTheDefaultScreen)
{
	EXPECT_EQ(FrontEndSpace(800, 600), (LayoutSpace {.scale = 1.0f, .offset = {0.0f, 0.0f}}));
	// As the game: a pixel a unit, centred, on screens as tall or as wide as the default one and bigger the other way
	EXPECT_EQ(FrontEndSpace(1000, 600), (LayoutSpace {.scale = 1.0f, .offset = {100.0f, 0.0f}}));
	EXPECT_EQ(FrontEndSpace(800, 801), (LayoutSpace {.scale = 1.0f, .offset = {0.0f, 100.0f}}));
}

TEST(ScreenFit, TheFrontEndFillsTheSame4By3ShareOfBiggerScreens)
{
	for (const auto [width, height] : {std::pair {1280, 1024}, std::pair {1600, 1200}, std::pair {1920, 1080},
	                                   std::pair {2560, 1440}, std::pair {3440, 1440}, std::pair {5120, 1440}})
	{
		const auto space = FrontEndSpace(width, height);
		const auto middle = FitInside(k_FourByThree, width, height);
		const auto topLeft = space.ToScreen({0.0f, 0.0f});
		const auto bottomRight = space.ToScreen({800.0f, 600.0f});
		// The layout covers the 4:3 middle to within a pixel, from whole pixels
		EXPECT_EQ(topLeft, glm::floor(topLeft)) << width << "x" << height;
		EXPECT_NEAR(topLeft.x, static_cast<float>(middle.x), 1.0f) << width << "x" << height;
		EXPECT_NEAR(topLeft.y, static_cast<float>(middle.y), 1.0f) << width << "x" << height;
		EXPECT_NEAR(bottomRight.x, static_cast<float>(middle.x + middle.width), 1.0f) << width << "x" << height;
		EXPECT_NEAR(bottomRight.y, static_cast<float>(middle.y + middle.height), 1.0f) << width << "x" << height;
		// and back
		const auto back = space.ToLayout(space.ToScreen({123.0f, 456.0f}));
		EXPECT_NEAR(back.x, 123.0f, 1e-3f);
		EXPECT_NEAR(back.y, 456.0f, 1e-3f);
	}
}

TEST(ScreenFit, TheFrontEndShrinksAsTheGameShrinksItOnSmallScreens)
{
	// The game: min(W / 850, H / 650), centred, so a margin is left round the layout
	const auto space = FrontEndSpace(640, 480);
	EXPECT_FLOAT_EQ(space.scale, 480.0f / 650.0f);
	EXPECT_NEAR(space.offset.x, (640.0f - (800.0f * 480.0f / 650.0f)) * 0.5f, 1e-3f);
	EXPECT_NEAR(space.offset.y, (480.0f - (600.0f * 480.0f / 650.0f)) * 0.5f, 1e-3f);
}

namespace
{
// The screens the front end is checked on: the default one, smaller windows, wide, ultrawide and super wide ones
constexpr std::array<std::pair<int32_t, int32_t>, 11> k_Screens {{
    {800, 600},
    {640, 480},
    {400, 300},
    {1024, 768},
    {1280, 720},
    {1366, 768},
    {1920, 1080},
    {2560, 1440},
    {3440, 1440},
    {5120, 1440},
    {1280, 1024},
}};

// Boxes of the dialog space, from min up to but not including max: a button, a list's row, a thin line and a box at
// the corner of the space
struct Box
{
	glm::ivec2 min;
	glm::ivec2 max;
};
constexpr std::array<Box, 5> k_Boxes {{
    {.min = {40, 490}, .max = {150, 550}},
    {.min = {110, 197}, .max = {380, 219}},
    {.min = {325, 250}, .max = {555, 290}},
    {.min = {399, 300}, .max = {400, 301}},
    {.min = {0, 0}, .max = {800, 600}},
}};

// The GPU fills a pixel of a box when the middle of the pixel is in the box: left and top edges in, right and bottom
// edges out
bool Fills(const LayoutSpace& space, const Box& box, glm::ivec2 pixel)
{
	const auto middle = glm::vec2(pixel) + 0.5f;
	const auto min = space.ToScreen(glm::vec2(box.min));
	const auto max = space.ToScreen(glm::vec2(box.max));
	return middle.x >= min.x && middle.y >= min.y && middle.x < max.x && middle.y < max.y;
}

bool Finds(const LayoutSpace& space, const Box& box, glm::ivec2 pixel)
{
	const auto unit = space.PixelToLayout(pixel);
	return unit.x >= box.min.x && unit.y >= box.min.y && unit.x < box.max.x && unit.y < box.max.y;
}
} // namespace

TEST(ScreenFit, APixelOfTheDefaultScreenFindsItsOwnUnit)
{
	const auto space = FrontEndSpace(800, 600);
	for (const auto pixel : {glm::ivec2(0, 0), glm::ivec2(1, 1), glm::ivec2(399, 300), glm::ivec2(799, 599)})
	{
		EXPECT_EQ(space.PixelToLayout(pixel), pixel);
		EXPECT_EQ(space.LayoutToPixel(pixel), pixel);
	}
	// A pixel a unit, centred, as the game finds it on a screen wider than the default one
	EXPECT_EQ(FrontEndSpace(1000, 600).PixelToLayout({100, 0}), glm::ivec2(0, 0));
	EXPECT_EQ(FrontEndSpace(1000, 600).PixelToLayout({99, 5}), glm::ivec2(-1, 5));
}

TEST(ScreenFit, APixelFindsABoxExactlyWhenTheBoxFillsIt)
{
	for (const auto [width, height] : k_Screens)
	{
		const auto space = FrontEndSpace(width, height);
		for (const auto& box : k_Boxes)
		{
			// Every pixel of a band three pixels round each edge of the box, across and down its middle
			const auto min = glm::ivec2(glm::floor(space.ToScreen(glm::vec2(box.min)))) - 3;
			const auto max = glm::ivec2(glm::floor(space.ToScreen(glm::vec2(box.max)))) + 3;
			const auto middle = space.LayoutToPixel((box.min + box.max) / 2);
			for (int32_t x = min.x; x <= max.x; ++x)
			{
				const glm::ivec2 pixel {x, middle.y};
				EXPECT_EQ(Finds(space, box, pixel), Fills(space, box, pixel))
				    << width << "x" << height << " pixel " << x << "," << middle.y;
			}
			for (int32_t y = min.y; y <= max.y; ++y)
			{
				const glm::ivec2 pixel {middle.x, y};
				EXPECT_EQ(Finds(space, box, pixel), Fills(space, box, pixel))
				    << width << "x" << height << " pixel " << middle.x << "," << y;
			}
		}
	}
}

TEST(ScreenFit, ThePixelOfAUnitFindsTheUnitAgainWhenTheLayoutIsNotShrunk)
{
	for (const auto [width, height] : k_Screens)
	{
		const auto space = FrontEndSpace(width, height);
		if (space.scale < 1.0f)
		{
			continue;
		}
		for (int32_t x = 0; x < k_ReferenceWidth; ++x)
		{
			EXPECT_EQ(space.PixelToLayout(space.LayoutToPixel({x, 300})), glm::ivec2(x, 300)) << width << "x" << height;
		}
		for (int32_t y = 0; y < k_ReferenceHeight; ++y)
		{
			EXPECT_EQ(space.PixelToLayout(space.LayoutToPixel({400, y})), glm::ivec2(400, y)) << width << "x" << height;
		}
	}
}

TEST(ScreenFit, OnASmallScreenEveryPixelOfAControlFindsTheControl)
{
	// A unit is less than a pixel, so some units have no pixel of their own, but each pixel still finds the unit under
	// its middle, and a control of the layout's sizes still has pixels
	for (const auto [width, height] : {std::pair {640, 480}, std::pair {400, 300}})
	{
		const auto space = FrontEndSpace(width, height);
		ASSERT_LT(space.scale, 1.0f);
		const Box button {.min = {40, 490}, .max = {150, 550}};
		const auto pixel = space.LayoutToPixel({95, 520});
		EXPECT_TRUE(Finds(space, button, pixel)) << width << "x" << height;
		EXPECT_TRUE(Fills(space, button, pixel)) << width << "x" << height;
	}
}
