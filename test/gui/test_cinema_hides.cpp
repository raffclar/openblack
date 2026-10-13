/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Gui/CinemaHides.h"

using namespace openblack::gui;

namespace
{
constexpr cinema_hides::Bars k_Out {.in = false, .heldByScript = false};
constexpr cinema_hides::Bars k_GameBars {.in = true, .heldByScript = false};
constexpr cinema_hides::Bars k_ScriptBars {.in = true, .heldByScript = true};
} // namespace

TEST(CinemaHides, CreaturePanelHiddenOnlyInAScriptsCutScene)
{
	EXPECT_TRUE(cinema_hides::ShowsCreaturePanel(k_Out));
	EXPECT_TRUE(cinema_hides::ShowsCreaturePanel(k_GameBars));
	EXPECT_FALSE(cinema_hides::ShowsCreaturePanel(k_ScriptBars));
}

TEST(CinemaHides, ToolTipHiddenOnlyInAScriptsCutScene)
{
	EXPECT_TRUE(cinema_hides::ShowsToolTip(k_Out));
	EXPECT_TRUE(cinema_hides::ShowsToolTip(k_GameBars));
	EXPECT_FALSE(cinema_hides::ShowsToolTip(k_ScriptBars));
}

TEST(CinemaHides, FloatingNumbersHiddenWheneverTheBarsAreIn)
{
	EXPECT_TRUE(cinema_hides::ShowsFloatingNumbers(k_Out));
	EXPECT_FALSE(cinema_hides::ShowsFloatingNumbers(k_GameBars));
	EXPECT_FALSE(cinema_hides::ShowsFloatingNumbers(k_ScriptBars));
}

TEST(CinemaHides, AnOwnerWithoutBarsHidesNothing)
{
	constexpr cinema_hides::Bars stale {.in = false, .heldByScript = true};
	EXPECT_TRUE(cinema_hides::ShowsCreaturePanel(stale));
	EXPECT_TRUE(cinema_hides::ShowsToolTip(stale));
	EXPECT_TRUE(cinema_hides::ShowsFloatingNumbers(stale));
}
