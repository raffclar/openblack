/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "Gui/ScriptTexts.h"

using namespace openblack::gui::script_text;

namespace
{
/// Ten pixels a character
float TenAChar(std::u16string_view words)
{
	return static_cast<float>(words.size()) * 10.0f;
}

ScreenText GameOver(float fade)
{
	// The GameOver script: across 0.25, down 0.45, width 0.5, height 0.1, size 0.075
	return Write(u"Game Over", {0.25f, 0.45f}, {0.5f, 0.1f}, 0.075f, fade, {255, 255, 255});
}
} // namespace

TEST(ScriptTexts, ShownAtOnceWithoutAFade)
{
	const auto text = GameOver(0.0f);
	EXPECT_FLOAT_EQ(text.alpha, 1.0f);
	EXPECT_FLOAT_EQ(text.target, 1.0f);
	EXPECT_FLOAT_EQ(text.rate, 0.0f);
}

TEST(ScriptTexts, FadesInOverItsTime)
{
	auto text = GameOver(3.0f);
	EXPECT_FLOAT_EQ(text.alpha, 0.0f);
	EXPECT_TRUE(Step(text, 1500));
	EXPECT_NEAR(text.alpha, 0.5f, 1e-5f);
	EXPECT_TRUE(Step(text, 3000));
	EXPECT_FLOAT_EQ(text.alpha, 1.0f);
}

TEST(ScriptTexts, GoneOnceFadedOut)
{
	auto text = GameOver(0.0f);
	FadeOut(text, 5.0f);
	EXPECT_TRUE(Step(text, 2500));
	EXPECT_NEAR(text.alpha, 0.5f, 1e-5f);
	EXPECT_FALSE(Step(text, 2500));
	EXPECT_FLOAT_EQ(text.alpha, 0.0f);
}

TEST(ScriptTexts, TakenAwayAtOnceWithoutAFadeOut)
{
	auto text = GameOver(3.0f);
	FadeOut(text, 0.0f);
	EXPECT_FLOAT_EQ(text.alpha, 0.0f);
	EXPECT_FALSE(Step(text, 0));
}

TEST(ScriptTexts, AFadingInTextIsGoneIfTheGamesTimeStandsStill)
{
	// Its alpha starts at nothing, and only the game's time brings it up
	auto text = GameOver(3.0f);
	EXPECT_FALSE(Step(text, 0));
}

TEST(ScriptTexts, AlphaAndColourBytes)
{
	EXPECT_EQ(AlphaByte(1.0f), 255);
	EXPECT_EQ(AlphaByte(0.5f), 127);
	EXPECT_EQ(AlphaByte(-1.0f), 0);
	EXPECT_EQ(AlphaByte(2.0f), 255);
	const auto colour = ColourFromScript(255.9f, 256.0f, 1.5f);
	EXPECT_EQ(colour.r, 255);
	EXPECT_EQ(colour.g, 0);
	EXPECT_EQ(colour.b, 1);
}

TEST(ScriptTexts, WordsAreCutToTheirLimit)
{
	const auto text = Write(std::u16string(2000, u'a'), {}, {}, 0.0f, 0.0f, {});
	EXPECT_EQ(text.words.size(), k_MaxCharacters);
}

TEST(ScriptTexts, GameOverIsCentredInItsBox)
{
	const auto box = Place(GameOver(0.0f), {800.0f, 600.0f});
	EXPECT_FLOAT_EQ(box.min.x, 200.0f);
	EXPECT_FLOAT_EQ(box.min.y, 270.0f);
	EXPECT_FLOAT_EQ(box.max.x, 600.0f);
	EXPECT_FLOAT_EQ(box.max.y, 330.0f);
	EXPECT_FLOAT_EQ(box.size, 45.0f);
	const std::array<std::u16string_view, 1> lines {u"Game Over"};
	const auto laid = Lay(box, lines, TenAChar);
	ASSERT_EQ(laid.size(), 1);
	EXPECT_FLOAT_EQ(laid[0].at.x, 200.0f + ((400.0f - 90.0f) * 0.5f));
	EXPECT_FLOAT_EQ(laid[0].at.y, 270.0f);
	EXPECT_FLOAT_EQ(laid[0].topShare, 1.0f);
	EXPECT_FLOAT_EQ(laid[0].bottomShare, 1.0f);
}

TEST(ScriptTexts, ALineRunningPastTheBottomFadesOutDownToIt)
{
	// Two lines of 45 in a box 60 high: the second starts inside it and runs past its bottom
	const ScreenBox box {.min = {0.0f, 0.0f}, .max = {400.0f, 60.0f}, .size = 45.0f};
	const std::array<std::u16string_view, 3> lines {u"one", u"two", u"three"};
	const auto laid = Lay(box, lines, TenAChar);
	ASSERT_EQ(laid.size(), 2);
	EXPECT_FLOAT_EQ(laid[1].at.y, 45.0f);
	EXPECT_FLOAT_EQ(laid[1].topShare, 1.0f);
	EXPECT_FLOAT_EQ(laid[1].bottomShare, 0.0f);
}

TEST(ScriptTexts, NothingInABoxTooSmall)
{
	const std::array<std::u16string_view, 1> lines {u"words"};
	EXPECT_TRUE(Lay({.min = {0.0f, 0.0f}, .max = {20.0f, 100.0f}, .size = 45.0f}, lines, TenAChar).empty());
	EXPECT_TRUE(Lay({.min = {0.0f, 0.0f}, .max = {400.0f, 40.0f}, .size = 45.0f}, lines, TenAChar).empty());
	EXPECT_EQ(Lay({.min = {0.0f, 0.0f}, .max = {22.5f, 45.0f}, .size = 45.0f}, lines, TenAChar).size(), 1);
}

TEST(ScriptTexts, AClipWindowOpenedAtOnceShowsItsBlackAtFull)
{
	ClipWindow window;
	OpenClipWindow(window, {0.1f, 0.2f}, {0.4f, 0.6f}, 0.0f);
	const auto drawn = Draw(window, {800, 600});
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ(drawn->alpha, 255);
	// Above, under, left of and right of the box: 80 to 400 across, 120 to 480 down
	EXPECT_EQ(drawn->boxes[0].min, glm::ivec2(-1, -1));
	EXPECT_EQ(drawn->boxes[0].max, glm::ivec2(800, 120));
	EXPECT_EQ(drawn->boxes[1].min, glm::ivec2(-1, 480));
	EXPECT_EQ(drawn->boxes[1].max, glm::ivec2(800, 600));
	EXPECT_EQ(drawn->boxes[2].min, glm::ivec2(-1, 120));
	EXPECT_EQ(drawn->boxes[2].max, glm::ivec2(80, 480));
	EXPECT_EQ(drawn->boxes[3].min, glm::ivec2(400, 120));
	EXPECT_EQ(drawn->boxes[3].max, glm::ivec2(800, 480));
}

TEST(ScriptTexts, AClipWindowFadesInAndOutAtItsRate)
{
	ClipWindow window;
	EXPECT_FALSE(Draw(window, {800, 600}).has_value());
	OpenClipWindow(window, {0.5f, 0.2f}, {0.4f, 0.6f}, 0.5f);
	Step(window, 100);
	EXPECT_FLOAT_EQ(window.alpha, 0.2f);
	ASSERT_TRUE(Draw(window, {800, 600}).has_value());
	EXPECT_EQ(Draw(window, {800, 600})->alpha, 51);
	Step(window, 1000);
	EXPECT_FLOAT_EQ(window.alpha, 1.0f);
	CloseClipWindow(window, 0.5f);
	Step(window, 250);
	EXPECT_FLOAT_EQ(window.alpha, 0.5f);
	Step(window, 1000);
	EXPECT_FLOAT_EQ(window.alpha, 0.0f);
	EXPECT_FALSE(Draw(window, {800, 600}).has_value());
	// The box stays where it was
	EXPECT_FLOAT_EQ(window.corner.x, 0.5f);
}

TEST(ScriptTexts, AClipWindowClosedAtOnceIsGone)
{
	ClipWindow window;
	OpenClipWindow(window, {0.1f, 0.2f}, {0.4f, 0.6f}, 0.0f);
	CloseClipWindow(window, 0.0f);
	EXPECT_FLOAT_EQ(window.alpha, 0.0f);
	EXPECT_FALSE(Draw(window, {800, 600}).has_value());
}
