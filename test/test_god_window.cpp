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

// With 4 gods or fewer there is no window: the box shows every god, as the game does
TEST(GodWindow, FourGodsOrFewerNeedNoWindow)
{
	EXPECT_FALSE(Scrolls(0));
	EXPECT_FALSE(Scrolls(4));
	EXPECT_TRUE(Scrolls(5));
	EXPECT_EQ(Shown(3, 0), (std::vector<size_t> {0, 1, 2}));
	EXPECT_EQ(Shown(4, 2), (std::vector<size_t> {0, 1, 2, 3}));
	EXPECT_FALSE(CanMoveBack(4, 0));
	EXPECT_FALSE(CanMoveOn(4, 0));
	EXPECT_TRUE(Shown(0, 0).empty());
	EXPECT_TRUE(Label(0, 0).empty());
}

// The window moves a page of 4 gods and stops at each end, always with 4 gods
TEST(GodWindow, TheWindowMovesAPageAndStopsAtTheEnds)
{
	EXPECT_EQ(Move(32, 0, 1), 4u);
	EXPECT_EQ(Move(32, 4, -1), 0u);
	EXPECT_EQ(Move(32, 0, -1), 0u);
	EXPECT_EQ(Move(32, 28, 1), 28u);
	EXPECT_EQ(Move(32, 0, 100), 28u);
	// Five gods: the second page shows gods 2 to 5
	EXPECT_EQ(Move(5, 0, 1), 1u);
	EXPECT_EQ(Shown(5, 1), (std::vector<size_t> {1, 2, 3, 4}));
	EXPECT_EQ(Move(5, 1, -1), 0u);
	// A first god past the end is kept in the range
	EXPECT_EQ(Clamp(10, 9), 6u);
	EXPECT_EQ(Clamp(10, -3), 0u);
	EXPECT_TRUE(CanMoveOn(32, 0));
	EXPECT_FALSE(CanMoveBack(32, 0));
	EXPECT_TRUE(CanMoveBack(32, 28));
	EXPECT_FALSE(CanMoveOn(32, 28));
}

// The text names the gods of the window from 1, and how many gods there are
TEST(GodWindow, TheLabelNamesTheGodsShown)
{
	EXPECT_EQ(Label(32, 0), u"Players 1-4 of 32");
	EXPECT_EQ(Label(32, 4), u"Players 5-8 of 32");
	EXPECT_EQ(Label(32, 28), u"Players 29-32 of 32");
	EXPECT_EQ(Label(7, 3), u"Players 4-7 of 7");
}
