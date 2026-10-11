/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a clinging advisor holds on to the screen (src/Help/Spirits.h): the game's holds, and on screens wider than
// 4:3, where the advisors are laid out on a 4:3 area in the middle (src/Help/SpiritStage.h), the top and bottom holds
// still just past the screen's edge, so that an advisor clinging to the bottom (as the good advisor does in the pitch
// lesson) shows its head and shoulders over the edge.

#include <cmath>

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

#include "Help/SpiritStage.h"
#include "Help/Spirits.h"

using namespace openblack::help::spirits;

namespace
{
constexpr std::array<Screen, 3> k_TallScreens {Screen {.width = 640, .height = 480}, Screen {.width = 1024, .height = 768},
                                               Screen {.width = 1280, .height = 1024}};
constexpr std::array<Screen, 5> k_WideScreens {Screen {.width = 1280, .height = 720}, Screen {.width = 1920, .height = 1080},
                                               Screen {.width = 2560, .height = 1440}, Screen {.width = 3440, .height = 1440},
                                               Screen {.width = 5120, .height = 1440}};

/// A script's place on the screen, as fractions across and down the screen the advisors are laid out on, in hover
/// space: both axes in half that screen's width from its middle
glm::vec2 HoverOf(const Screen& layout, float across, float down)
{
	const auto halfW = static_cast<float>(layout.HalfWidth());
	return {(across * static_cast<float>(layout.width) - halfW) / halfW,
	        (down * static_cast<float>(layout.height) - static_cast<float>(layout.HalfHeight())) / halfW};
}

/// How far below the real screen's middle a hover point is drawn, in half the screen's height
float DownInHalfHeights(const Stage& stage, float hoverY)
{
	return hoverY * static_cast<float>(stage.layout.HalfWidth()) / static_cast<float>(stage.screen.HalfHeight());
}
} // namespace

TEST(SpiritCling, TheGamesHolds)
{
	const auto bottom = PlaceCling({-0.2f, 0.75f});
	EXPECT_EQ(bottom.edge, Edge::Bottom);
	EXPECT_EQ(bottom.hover.x, -0.2f);
	EXPECT_EQ(bottom.hover.y, 0.78f);
	const auto top = PlaceCling({0.1f, -0.5f});
	EXPECT_EQ(top.edge, Edge::Top);
	EXPECT_EQ(top.hover.y, -0.78f);
	const auto right = PlaceCling({1.0f, 0.4f});
	EXPECT_EQ(right.edge, Edge::Right);
	EXPECT_EQ(right.hover, glm::vec2(1.04f, 0.4f));
	const auto left = PlaceCling({-1.0f, 0.0f});
	EXPECT_EQ(left.edge, Edge::Left);
	EXPECT_EQ(left.hover.x, -1.04f);
	// The height is weighed by 1 / 0.78 against the width: just under the line the sides win, on it the ends do
	EXPECT_EQ(PlaceCling({0.6f, 0.46f}).edge, Edge::Right);
	EXPECT_EQ(PlaceCling({0.6f, 0.47f}).edge, Edge::Bottom);
}

TEST(SpiritCling, ScreensTallerThan4By3AreLaidOutAsThemselves)
{
	for (const auto& screen : k_TallScreens)
	{
		SCOPED_TRACE(screen.width);
		const Stage stage = StageOf(screen);
		EXPECT_EQ(stage.layout, screen);
		EXPECT_EQ(stage.left, 0);
		EXPECT_EQ(stage.share, 1.0f);
	}
}

TEST(SpiritCling, OnWiderScreensTheEndHoldsSitAsOnA4By3Screen)
{
	// On a 4:3 screen the bottom hold is 1.04 half heights below the middle, just past the edge
	const float onFourByThree = DownInHalfHeights(StageOf({.width = 640, .height = 480}), 0.78f);
	EXPECT_FLOAT_EQ(onFourByThree, 1.04f);
	for (const auto& screen : k_WideScreens)
	{
		SCOPED_TRACE(screen.width);
		const Stage stage = StageOf(screen);
		const auto bottom = PlaceCling({0.0f, 0.5f});
		EXPECT_EQ(bottom.edge, Edge::Bottom);
		EXPECT_NEAR(DownInHalfHeights(stage, bottom.hover.y), onFourByThree, 2e-3f);
		const auto top = PlaceCling({0.0f, -0.5f});
		EXPECT_EQ(top.edge, Edge::Top);
		EXPECT_NEAR(DownInHalfHeights(stage, top.hover.y), -onFourByThree, 2e-3f);
	}
}

TEST(SpiritCling, TheEndHoldsAreWhereTheWideScreenRuleHadThem)
{
	// The holds came in by the screen's height against a 4:3 screen's, min(1, 4H / 3W), in half the screen's width:
	// laid out on the 4:3 area they land on the same pixel row
	for (const auto& screen : k_WideScreens)
	{
		SCOPED_TRACE(screen.width);
		const Stage stage = StageOf(screen);
		const float factor =
		    std::min(1.0f, 4.0f * static_cast<float>(screen.height) / (3.0f * static_cast<float>(screen.width)));
		const float before = 0.78f * factor * static_cast<float>(screen.HalfWidth());
		const float now = PlaceCling({0.0f, 0.5f}).hover.y * static_cast<float>(stage.layout.HalfWidth());
		EXPECT_NEAR(now, before, 1.0f);
	}
}

TEST(SpiritCling, TheGoodAdvisorsPitchLessonHoldIsJustBelowTheBottom)
{
	// "make good spirit cling across 0.4 down 1.0"
	for (const auto& screen : {Screen {.width = 640, .height = 480}, Screen {.width = 1280, .height = 1024},
	                           Screen {.width = 2560, .height = 1440}, Screen {.width = 5120, .height = 1440}})
	{
		SCOPED_TRACE(screen.width);
		const Stage stage = StageOf(screen);
		const auto place = PlaceCling(HoverOf(stage.layout, 0.4f, 1.0f));
		EXPECT_EQ(place.edge, Edge::Bottom);
		EXPECT_FLOAT_EQ(place.hover.x, -0.2f);
		const float down = DownInHalfHeights(stage, place.hover.y);
		EXPECT_GT(down, 0.97f); // at the bottom edge or past it
		EXPECT_LE(down, 1.04f + 2e-3f);
	}
}

TEST(SpiritCling, AClingingAdvisorTakesItsHoldFromTheLayout)
{
	const Stage stage = StageOf({.width = 2560, .height = 1440});
	DudeData good;
	DudeData evil;
	Queries queries;
	queries.localRand = [](int32_t) { return 0u; };
	queries.localFloatRand = [](float) { return 0.0f; };
	queries.random = [](float a, float) { return a; };
	AdvisorSpiritController control(good, evil, queries, stage.layout);
	control.SpiritCling(1, 0.4f, 1.0f);
	const auto& dude = control.Dude(0);
	EXPECT_EQ(dude.ClingEdge(), Edge::Bottom);
	EXPECT_FLOAT_EQ(dude.ClingTarget().y, 0.78f);
	EXPECT_FLOAT_EQ(dude.ClingTarget().x, -0.2f);
}
