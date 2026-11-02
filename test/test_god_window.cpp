/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Gui/GodWindowRules.h"

using namespace openblack::gui::god_window;

namespace
{
// The god columns of the statistics box: 60 wide, so the view is 240 wide
constexpr int k_Column = 60;
// A scroll bar as long as the view
constexpr int k_Bar = 240;
} // namespace

// With 4 gods or fewer there is no window: the box shows every god, as the game does
TEST(GodWindow, FourGodsOrFewerNeedNoWindow)
{
	EXPECT_FALSE(Scrolls(0));
	EXPECT_FALSE(Scrolls(4));
	EXPECT_TRUE(Scrolls(5));
	EXPECT_EQ(Shown(3, 0), (std::vector<size_t> {0, 1, 2}));
	EXPECT_EQ(Shown(4, 2), (std::vector<size_t> {0, 1, 2, 3}));
	EXPECT_TRUE(Shown(0, 0).empty());
	EXPECT_TRUE(Label(0, 0).empty());
	EXPECT_EQ(MostScroll(4, k_Column), 0.0f);
	EXPECT_EQ(ClampScroll(4, k_Column, 100.0f), 0.0f);
	EXPECT_EQ(Step(4, k_Column, 0.0f, 1), 0.0f);
	EXPECT_EQ(Wheel(4, k_Column, 0.0f, -3, 0, 20), 0.0f);
}

// A first god past the end is kept in the range
TEST(GodWindow, TheFirstGodStaysInTheRange)
{
	EXPECT_EQ(Clamp(10, 9), 6u);
	EXPECT_EQ(Clamp(10, -3), 0u);
	EXPECT_EQ(Shown(5, 1), (std::vector<size_t> {1, 2, 3, 4}));
}

// The text names the gods of the window from 1, and how many gods there are
TEST(GodWindow, TheLabelNamesTheGodsShown)
{
	EXPECT_EQ(Label(32, 0), u"Players 1-4 of 32");
	EXPECT_EQ(Label(32, 4), u"Players 5-8 of 32");
	EXPECT_EQ(Label(32, 28), u"Players 29-32 of 32");
	EXPECT_EQ(Label(7, 3), u"Players 4-7 of 7");
}

// The scroll stays between the first column and the last 4 columns
TEST(GodWindow, TheScrollStaysInItsRange)
{
	EXPECT_EQ(MostScroll(32, k_Column), 28.0f * 60.0f);
	EXPECT_EQ(MostScroll(5, 100), 100.0f);
	EXPECT_EQ(ClampScroll(32, k_Column, -5.0f), 0.0f);
	EXPECT_EQ(ClampScroll(32, k_Column, 5000.0f), 1680.0f);
	EXPECT_EQ(ClampScroll(32, k_Column, 145.5f), 145.5f);
}

// The 4 gods of the window are the 4 gods whose columns are nearest to the view
TEST(GodWindow, TheWindowHasTheNearestGods)
{
	EXPECT_EQ(FirstAt(32, k_Column, 0.0f), 0u);
	EXPECT_EQ(FirstAt(32, k_Column, 29.0f), 0u);
	EXPECT_EQ(FirstAt(32, k_Column, 31.0f), 1u);
	EXPECT_EQ(FirstAt(32, k_Column, 140.0f), 2u);
	EXPECT_EQ(FirstAt(32, k_Column, 1680.0f), 28u);
	EXPECT_EQ(FirstAt(32, k_Column, 9000.0f), 28u);
	EXPECT_EQ(FirstAt(32, 100, 140.0f), 1u);
}

// An arrow scrolls the view one god, to the edge of a column, and stops at each end
TEST(GodWindow, TheArrowsStepOneGod)
{
	EXPECT_EQ(Step(32, k_Column, 0.0f, 1), 60.0f);
	EXPECT_EQ(Step(32, k_Column, 60.0f, 1), 120.0f);
	// Part of the way along a column, the arrows go to its edges
	EXPECT_EQ(Step(32, k_Column, 140.0f, 1), 180.0f);
	EXPECT_EQ(Step(32, k_Column, 140.0f, -1), 120.0f);
	EXPECT_EQ(Step(32, k_Column, 120.0f, -1), 60.0f);
	EXPECT_EQ(Step(32, k_Column, 0.0f, -1), 0.0f);
	EXPECT_EQ(Step(32, k_Column, 1680.0f, 1), 1680.0f);
	EXPECT_EQ(Step(32, k_Column, 0.0f, 3), 180.0f);
}

// A notch of the wheel moves the view as far as a notch moves a list: away from the player moves back, a horizontal
// wheel to the right moves on
TEST(GodWindow, TheWheelScrollsTheView)
{
	EXPECT_EQ(Wheel(32, k_Column, 0.0f, -7, 0, 20), 140.0f);
	EXPECT_EQ(Wheel(32, k_Column, 140.0f, 2, 0, 20), 100.0f);
	EXPECT_EQ(Wheel(32, k_Column, 100.0f, 0, 3, 20), 160.0f);
	EXPECT_EQ(Wheel(32, k_Column, 10.0f, 1, 0, 20), 0.0f);
	EXPECT_EQ(Wheel(32, k_Column, 1670.0f, -1, 0, 20), 1680.0f);
}

// The view shows the columns that it covers all or part of, cut at its edges
TEST(GodWindow, TheViewShowsTheColumnsItCovers)
{
	EXPECT_EQ(Visible(32, k_Column, 0.0f, 434), (std::vector<VisibleColumn> {{0, 434}, {1, 494}, {2, 554}, {3, 614}}));
	// Two and a third columns on: parts of 5 columns show
	EXPECT_EQ(Visible(32, k_Column, 140.0f, 434),
	          (std::vector<VisibleColumn> {{2, 414}, {3, 474}, {4, 534}, {5, 594}, {6, 654}}));
	EXPECT_EQ(Visible(32, k_Column, 1680.0f, 0), (std::vector<VisibleColumn> {{28, 0}, {29, 60}, {30, 120}, {31, 180}}));
	EXPECT_EQ(Visible(3, k_Column, 50.0f, 0), (std::vector<VisibleColumn> {{0, 0}, {1, 60}, {2, 120}}));
	EXPECT_TRUE(Visible(32, 0, 0.0f, 0).empty());
}

// The thumb is the share of the strip that the view shows, along the bar less 3 at each end
TEST(GodWindow, TheThumbShowsTheShareOfTheView)
{
	// 234 of track: an eighth of it for 4 of 32 gods
	EXPECT_EQ(ThumbOf(32, k_Column, 0.0f, k_Bar), (Thumb {0, 29}));
	EXPECT_EQ(ThumbOf(32, k_Column, 1680.0f, k_Bar), (Thumb {204, 234}));
	EXPECT_EQ(ThumbOf(32, k_Column, 140.0f, k_Bar), (Thumb {17, 46}));
	// With 4 gods the thumb fills the bar
	EXPECT_EQ(ThumbOf(4, k_Column, 0.0f, k_Bar), (Thumb {0, 234}));
}

// A click on the bar finds the thumb, or the bar before or after it
TEST(GodWindow, TheBarFindsTheThumb)
{
	EXPECT_EQ(PartAt(32, k_Column, 140.0f, k_Bar, 10), BarPart::Before);
	EXPECT_EQ(PartAt(32, k_Column, 140.0f, k_Bar, 20), BarPart::Thumb);
	EXPECT_EQ(PartAt(32, k_Column, 140.0f, k_Bar, 48), BarPart::Thumb);
	EXPECT_EQ(PartAt(32, k_Column, 140.0f, k_Bar, 49), BarPart::After);
	// The inset before the thumb is part of the bar before it
	EXPECT_EQ(PartAt(32, k_Column, 0.0f, k_Bar, 0), BarPart::Before);
	EXPECT_EQ(PartAt(32, k_Column, 0.0f, k_Bar, 3), BarPart::Thumb);
}

// A drag of the thumb moves the view as far along the strip as the thumb moves along the bar; a click before or after
// the thumb moves the view one view width
TEST(GodWindow, TheThumbDragsAndTheBarPages)
{
	// 1920 of strip over 234 of track
	EXPECT_FLOAT_EQ(Drag(32, k_Column, 0.0f, 117, k_Bar), 960.0f);
	EXPECT_FLOAT_EQ(Drag(32, k_Column, 960.0f, -117, k_Bar), 0.0f);
	EXPECT_EQ(Drag(32, k_Column, 0.0f, 1000, k_Bar), 1680.0f);
	EXPECT_EQ(Drag(32, k_Column, 100.0f, -1000, k_Bar), 0.0f);
	EXPECT_EQ(Page(32, k_Column, 140.0f, true), 380.0f);
	EXPECT_EQ(Page(32, k_Column, 140.0f, false), 0.0f);
	EXPECT_EQ(Page(32, k_Column, 1600.0f, true), 1680.0f);
}
