/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a clinging advisor holds on to the screen (src/Help/Spirits.h): the game's holds on 4:3 screens and on taller
// ones, and on wider screens the top and bottom holds coming in with the edge, so that an advisor clinging to the
// bottom (as the good advisor does in the pitch lesson) still shows its head and shoulders over the edge.

#include <cmath>

#include <array>

#include <gtest/gtest.h>

#include "Help/Spirits.h"

using namespace openblack::help::spirits;

namespace
{
constexpr std::array<Screen, 3> k_TallScreens {Screen {.width = 640, .height = 480}, Screen {.width = 1024, .height = 768},
                                               Screen {.width = 1280, .height = 1024}};
constexpr std::array<Screen, 4> k_WideScreens {Screen {.width = 1280, .height = 720}, Screen {.width = 1920, .height = 1080},
                                               Screen {.width = 2560, .height = 1440}, Screen {.width = 3440, .height = 1440}};

/// A script's place on the screen, as fractions across and down it, in hover space: both axes in half the screen's
/// width from its middle
glm::vec2 HoverOf(const Screen& screen, float across, float down)
{
	const auto halfW = static_cast<float>(screen.HalfWidth());
	return {(across * static_cast<float>(screen.width) - halfW) / halfW,
	        (down * static_cast<float>(screen.height) - static_cast<float>(screen.HalfHeight())) / halfW};
}

/// How far below the screen's middle a hover point is drawn, in half the screen's height
float DownInHalfHeights(const Screen& screen, float hoverY)
{
	return hoverY * static_cast<float>(screen.HalfWidth()) / static_cast<float>(screen.HalfHeight());
}
} // namespace

TEST(SpiritCling, TheGamesHoldsOnA4By3Screen)
{
	const Screen screen {.width = 640, .height = 480};
	const auto bottom = PlaceCling({-0.2f, 0.75f}, screen);
	EXPECT_EQ(bottom.edge, Edge::Bottom);
	EXPECT_EQ(bottom.hover.x, -0.2f);
	EXPECT_EQ(bottom.hover.y, 0.78f);
	const auto top = PlaceCling({0.1f, -0.5f}, screen);
	EXPECT_EQ(top.edge, Edge::Top);
	EXPECT_EQ(top.hover.y, -0.78f);
	const auto right = PlaceCling({1.0f, 0.4f}, screen);
	EXPECT_EQ(right.edge, Edge::Right);
	EXPECT_EQ(right.hover, glm::vec2(1.04f, 0.4f));
	const auto left = PlaceCling({-1.0f, 0.0f}, screen);
	EXPECT_EQ(left.edge, Edge::Left);
	EXPECT_EQ(left.hover.x, -1.04f);
	// The height is weighed by 1 / 0.78 against the width: just under the line the sides win, on it the ends do
	EXPECT_EQ(PlaceCling({0.6f, 0.46f}, screen).edge, Edge::Right);
	EXPECT_EQ(PlaceCling({0.6f, 0.47f}, screen).edge, Edge::Bottom);
}

TEST(SpiritCling, ScreensTallerThan4By3KeepTheGamesHolds)
{
	for (const auto& screen : k_TallScreens)
	{
		SCOPED_TRACE(screen.width);
		EXPECT_EQ(PlaceCling({0.3f, 0.7f}, screen).hover.y, 0.78f);
		EXPECT_EQ(PlaceCling({0.3f, -0.7f}, screen).hover.y, -0.78f);
		EXPECT_EQ(PlaceCling({0.9f, 0.1f}, screen).hover.x, 1.04f);
	}
}

TEST(SpiritCling, OnWiderScreensTheEndHoldsSitAsOnA4By3Screen)
{
	// On a 4:3 screen the bottom hold is 1.04 half heights below the middle, just past the edge
	const float onFourByThree = DownInHalfHeights({.width = 640, .height = 480}, 0.78f);
	EXPECT_FLOAT_EQ(onFourByThree, 1.04f);
	for (const auto& screen : k_WideScreens)
	{
		SCOPED_TRACE(screen.width);
		const auto bottom = PlaceCling({0.0f, 0.5f}, screen);
		EXPECT_EQ(bottom.edge, Edge::Bottom);
		EXPECT_NEAR(DownInHalfHeights(screen, bottom.hover.y), onFourByThree, 1e-5f);
		const auto top = PlaceCling({0.0f, -0.5f}, screen);
		EXPECT_EQ(top.edge, Edge::Top);
		EXPECT_NEAR(DownInHalfHeights(screen, top.hover.y), -onFourByThree, 1e-5f);
		// The side holds don't change
		EXPECT_EQ(PlaceCling({0.9f, 0.0f}, screen).hover.x, 1.04f);
	}
}

TEST(SpiritCling, EveryScreenPicksTheEdgeA4By3ScreenDoes)
{
	const Screen fourByThree {.width = 640, .height = 480};
	for (const auto& screen : k_WideScreens)
	{
		SCOPED_TRACE(screen.width);
		for (int i = 0; i <= 20; ++i)
		{
			for (int j = 0; j <= 20; ++j)
			{
				const float across = static_cast<float>(i) / 20.0f;
				const float down = static_cast<float>(j) / 20.0f;
				EXPECT_EQ(PlaceCling(HoverOf(screen, across, down), screen).edge,
				          PlaceCling(HoverOf(fourByThree, across, down), fourByThree).edge)
				    << across << ", " << down;
			}
		}
	}
}

TEST(SpiritCling, ThePitchLessonsGoodAdvisorHoldsOnJustBelowTheBottom)
{
	// "make good spirit cling across 0.4 down 1.0"
	for (const auto& screen :
	     {Screen {.width = 640, .height = 480}, Screen {.width = 1280, .height = 1024}, Screen {.width = 2560, .height = 1440}})
	{
		SCOPED_TRACE(screen.width);
		const auto place = PlaceCling(HoverOf(screen, 0.4f, 1.0f), screen);
		EXPECT_EQ(place.edge, Edge::Bottom);
		EXPECT_FLOAT_EQ(place.hover.x, -0.2f);
		const float down = DownInHalfHeights(screen, place.hover.y);
		EXPECT_GT(down, 0.97f); // at the bottom edge or past it
		EXPECT_LE(down, 1.04f + 1e-5f);
	}
}

TEST(SpiritCling, AClingingAdvisorTakesItsHoldFromTheScreen)
{
	const Screen screen {.width = 2560, .height = 1440};
	DudeData good;
	DudeData evil;
	Queries queries;
	queries.localRand = [](int32_t) { return 0u; };
	queries.localFloatRand = [](float) { return 0.0f; };
	queries.random = [](float a, float) { return a; };
	AdvisorSpiritController control(good, evil, queries, screen);
	control.SpiritCling(1, 0.4f, 1.0f);
	const auto& dude = control.Dude(0);
	EXPECT_EQ(dude.ClingEdge(), Edge::Bottom);
	EXPECT_FLOAT_EQ(dude.ClingTarget().y, 0.78f * 0.75f);
}
