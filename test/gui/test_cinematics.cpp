/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>
#include <tuple>

#include <gtest/gtest.h>

#include "Gui/CinemaBars.h"
#include "Gui/ScriptFade.h"

using openblack::gui::CinemaBars;
using openblack::gui::ScriptFade;

TEST(ScriptFade, FadesToAColourOverTurns)
{
	ScriptFade fade;
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0u);

	// A second is ten turns of 25.5 each
	fade.FadeTo(0x10, 0x20, 0x30, 1);
	EXPECT_FALSE(fade.IsFinished());
	fade.ProcessTurn();
	EXPECT_EQ(fade.GetColour(), 0x19102030u);
	for (int i = 0; i < 9; ++i)
	{
		fade.ProcessTurn();
	}
	EXPECT_EQ(fade.GetColour() >> 24u, 255u);
	EXPECT_FALSE(fade.IsFinished());
	// It stops on the turn it would pass the end
	fade.ProcessTurn();
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0xFF102030u);
}

TEST(ScriptFade, NoTimeIsAtOnce)
{
	ScriptFade fade;
	fade.FadeTo(0xFF, 0, 0, 0);
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0xFFFF0000u);
	fade.FadeBackToNormal(-1);
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0x00FF0000u);
}

// The script values lose the fraction towards zero, and only the low byte stays
TEST(ScriptFade, ScriptValuesBecomeBytes)
{
	using openblack::gui::FadeColourFromScript;
	using openblack::gui::FadeSecondsFromScript;
	EXPECT_EQ(FadeColourFromScript(255.9f), 255u);
	EXPECT_EQ(FadeColourFromScript(256.0f), 0u);
	EXPECT_EQ(FadeColourFromScript(-1.0f), 255u);
	EXPECT_EQ(FadeSecondsFromScript(3.7f), 3);
	EXPECT_EQ(FadeSecondsFromScript(-1.9f), -1);
	// 200 seconds wrap to a negative byte, so the fade is at once
	EXPECT_EQ(FadeSecondsFromScript(200.0f), -56);
	EXPECT_EQ(FadeSecondsFromScript(300.5f), 44);
	// Out of the range of a whole number, the game gives the low byte of the 64-bit value; NaN gives 0
	EXPECT_EQ(FadeColourFromScript(3.0e9f), 0u);
	EXPECT_EQ(FadeSecondsFromScript(-3.0e9f), 0);
	EXPECT_EQ(FadeSecondsFromScript(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(ScriptFade, FadesBackToClear)
{
	ScriptFade fade;
	fade.FadeTo(0xFF, 0xFF, 0xFF, 0);
	fade.FadeBackToNormal(2);
	fade.ProcessTurn();
	// 255 less 12.75, rounded down
	EXPECT_EQ(fade.GetColour() >> 24u, 242u);
	for (int i = 0; i < 20; ++i)
	{
		fade.ProcessTurn();
	}
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour() >> 24u, 0u);
}

// A new game resets the bars to off at once: no bars show, and none slide out
TEST(CinemaBars, ANewGameStartsWithoutThem)
{
	CinemaBars bars;
	bars.Update(0.0f, 2.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.0f);
	EXPECT_TRUE(bars.IsTransitionFinished());
	bars.Update(1000.0f, 2.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.0f);
	// Turned on, they slide in from nothing
	bars.Set(true, 2.0f);
	bars.Update(1000.0f, 2.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.5f);
}

TEST(CinemaBars, SlideInAndTurnBackFromWhereTheyAre)
{
	CinemaBars bars;
	bars.Update(5000.0f, 2.0f);
	bars.Set(true, 2.0f);
	EXPECT_TRUE(bars.IsOn());
	bars.Update(500.0f, 2.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.25f);
	EXPECT_FALSE(bars.IsTransitionFinished());
	// Out again from a quarter in
	bars.Set(false, 2.0f);
	bars.Update(0.0f, 2.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.25f);
	bars.Update(500.0f, 2.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.0f);
}

// The scripts wait for the bars of the turns. Only the time of the turns moves them, so they finish at the same turn at
// any frame rate. The frames move only the drawn bars.
TEST(CinemaBars, TheTurnsAloneFinishTheSlide)
{
	for (const float frameMs : {7.0f, 16.7f, 33.0f, 50.0f})
	{
		CinemaBars bars;
		bars.Set(true, 1.0f);
		int turns = 0;
		while (!bars.IsTransitionFinished())
		{
			// The frames between two turns slide the drawn bars only
			for (float spent = 0.0f; spent < 100.0f; spent += frameMs)
			{
				bars.Update(frameMs, 1.0f);
			}
			bars.ProcessTurn(100.0f, 1.0f);
			++turns;
		}
		EXPECT_EQ(turns, 10) << frameMs;
	}
}

// The frames change none of the values that a save keeps, so that each computer of a network game saves the same bars
TEST(CinemaBars, TheFramesChangeNoSavedValue)
{
	const auto saved = [](const CinemaBars& bars) {
		std::tuple<bool, float, float, float> values;
		CinemaBars::VisitSaved(bars, [&values](bool on, float timer, float transitionSeconds, float fraction) {
			values = {on, timer, transitionSeconds, fraction};
		});
		return values;
	};
	CinemaBars bars;
	bars.Set(true, 1.0f);
	bars.ProcessTurn(100.0f, 1.0f);
	const auto before = saved(bars);
	bars.Update(16.7f, 2.0f);
	bars.Update(16.7f, 2.0f);
	EXPECT_EQ(saved(bars), before);
	EXPECT_GT(bars.GetFraction(), 0.0f);
}

// A load starts the drawn bars where the bars of the turns are
TEST(CinemaBars, TheDrawnBarsStartFromTheTurnsAfterALoad)
{
	CinemaBars bars;
	bars.Set(true, 2.0f);
	bars.ProcessTurn(1000.0f, 2.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.0f);
	bars.DrawFromTurns();
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.5f);
}

TEST(CinemaBars, BarsMakeThePictureSixteenByNine)
{
	// 768 - 576 is 192, 96 a bar
	EXPECT_EQ(CinemaBars::BarHeight(1024, 768, 1.0f), 96);
	EXPECT_EQ(CinemaBars::BarHeight(1024, 768, 0.5f), 48);
	EXPECT_EQ(CinemaBars::BarHeight(1280, 720, 1.0f), 0);
	EXPECT_EQ(CinemaBars::BarHeight(2560, 1080, 1.0f), 0);
}

// The default 800x600 screen shows the whole 4:3 picture until a script brings the bars in, which then leave 450 rows
TEST(CinemaBars, TheDefaultScreenHasNoneUntilAScriptBringsThemIn)
{
	EXPECT_EQ(CinemaBars::BarHeight(800, 600, 0.0f), 0);
	EXPECT_EQ(CinemaBars::BarHeight(800, 600, 1.0f), 75);
}
