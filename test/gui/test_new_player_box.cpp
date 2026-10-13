/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The box a new player types their name into, and the swirling backdrop behind it

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <Gui/MenuBackdrop.h>
#include <Gui/NewPlayerBox.h>
#include <Gui/TextDatabase.h>
#include <SDL_keycode.h>
#include <gtest/gtest.h>

#include "FakeGameFont.h"

using namespace openblack::gui;
using namespace openblack::gui::test;
using openblack::player_profiles::NameCheck;

namespace
{
TextDatabase NewPlayerTexts()
{
	std::u16string script;
	const std::vector<std::pair<std::u16string, std::u16string>> texts = {
	    {u"HELP_TEXT_NONE", u"BAD"},
	    {u"HELP_TEXT_DIALOG_PLEASETYPENAME", u"AB"},
	    {u"HELP_TEXT_DIALOG_PLAYER", u"A"},
	    {u"HELP_TEXT_DIALOG_SYMBOL", u"B"},
	    {u"HELP_TEXT_REQUESTER_BOXES_03", u"AB"},
	    {u"HELP_TEXT_REQUESTER_BOXES_04", u"AW"},
	    {u"HELP_TEXT_DIALOG_PLAYERALREADYEXISTS", u"AA"},
	    {u"HELP_TEXT_DIALOG_INVALIDPLAYERNAME", u"BB"},
	};
	for (const auto& [name, text] : texts)
	{
		script.append(u"ADD_TEXT(1, N, \"").append(name).append(u"\", \"").append(text).append(u"\")\n");
	}
	TextDatabase database;
	database.AddScript(Utf16Script(script));
	return database;
}

struct NewPlayerFixture
{
	TextDatabase texts = NewPlayerTexts();
	GameFont font = LoadFont();
	NewPlayerBox box {texts, font};
	std::vector<std::u16string> taken {u"Matt"};

	NewPlayerFixture()
	{
		box.checkName = [this](std::u16string_view name) {
			if (name.empty())
			{
				return NameCheck::Invalid;
			}
			return std::ranges::find(taken, name) != taken.end() ? NameCheck::AlreadyExists : NameCheck::Accepted;
		};
		box.Show(false);
		// Faded in
		box.Update(1.0f);
	}

	bool Click(glm::ivec2 point)
	{
		box.MouseMove(point);
		box.MouseDown(point);
		return box.MouseUp(point);
	}
};

constexpr glm::ivec2 k_Ok = NewPlayerBox::k_OkPosition + (NewPlayerBox::k_ButtonSize / 2);
constexpr glm::ivec2 k_Cancel = NewPlayerBox::k_CancelPosition + (NewPlayerBox::k_ButtonSize / 2);
} // namespace

TEST(NewPlayerBox, TheNameTypedMakesTheProfileWithTheSymbolPicked)
{
	NewPlayerFixture f;
	EXPECT_TRUE(f.box.IsActive());
	// The name's box has the focus as the box comes up: typing goes straight in
	f.box.TextInput(u"Ann");
	EXPECT_EQ(f.box.GetName(), u"Ann");
	EXPECT_TRUE(f.Click(k_Ok));
	const auto answer = f.box.TakeAnswer();
	ASSERT_TRUE(answer.has_value());
	EXPECT_EQ(answer->name, u"Ann");
	EXPECT_EQ(answer->symbol, 0);
	EXPECT_FALSE(f.box.IsActive());
	// It fades out over a fifth of a second
	EXPECT_TRUE(f.box.IsVisible());
	f.box.Update(0.25f);
	EXPECT_FALSE(f.box.IsVisible());
}

TEST(NewPlayerBox, ReturnAnswersAsOkDoes)
{
	NewPlayerFixture f;
	f.box.TextInput(u"Ann");
	f.box.KeyDown(SDLK_RETURN);
	EXPECT_TRUE(f.box.TakeAnswer().has_value());
}

TEST(NewPlayerBox, NoNameOrANameTakenBringsUpAMessageInstead)
{
	NewPlayerFixture f;
	f.box.KeyDown(SDLK_RETURN);
	EXPECT_TRUE(f.box.IsShowingMessage());
	EXPECT_FALSE(f.box.TakeAnswer().has_value());
	// Typing goes nowhere while the message is up; its OK takes it away
	f.box.TextInput(u"X");
	EXPECT_EQ(f.box.GetName(), u"");
	f.box.KeyDown(SDLK_ESCAPE);
	EXPECT_FALSE(f.box.IsShowingMessage());
	EXPECT_TRUE(f.box.IsActive());
	// The name's box has lost the focus: typing goes nowhere until it is clicked
	f.box.TextInput(u"Matt");
	EXPECT_EQ(f.box.GetName(), u"");
	f.Click(NewPlayerBox::k_Name.Centre());
	f.box.TextInput(u"Matt");
	f.box.KeyDown(SDLK_KP_ENTER);
	EXPECT_TRUE(f.box.IsShowingMessage());
	ASSERT_EQ(f.box.GetNamedControls().size(), 1u);
	EXPECT_TRUE(f.Click(NewPlayerBox::k_MessageOk.Centre()));
	EXPECT_FALSE(f.box.IsShowingMessage());
}

TEST(NewPlayerBox, CancelIsThereOnlyWhenSomeoneHasAProfile)
{
	NewPlayerFixture f;
	// A first start: Cancel is hidden and Escape does nothing
	f.box.KeyDown(SDLK_ESCAPE);
	EXPECT_TRUE(f.box.IsActive());
	EXPECT_FALSE(f.Click(k_Cancel));
	EXPECT_FALSE(f.box.TakeCancelled());

	f.box.Show(true);
	f.box.KeyDown(SDLK_ESCAPE);
	EXPECT_TRUE(f.box.TakeCancelled());
	EXPECT_FALSE(f.box.IsActive());
	EXPECT_FALSE(f.box.TakeAnswer().has_value());
}

TEST(MenuBackdrop, TheSwirlsStartAtRandomLastAngleFirst)
{
	float next = 0.0f;
	const auto swirls = menu_backdrop::Start([&next](float a, float b) {
		next += 1.0f;
		return a + (b - a) * (next / 100.0f);
	});
	// The first swirl's speeds are drawn c, b, a, then its angles c, b, a
	EXPECT_FLOAT_EQ(swirls.at(0).speeds.z, -1.0f + (2.0f * 0.01f));
	EXPECT_FLOAT_EQ(swirls.at(0).speeds.x, -1.0f + (2.0f * 0.03f));
	EXPECT_FLOAT_EQ(swirls.at(0).angles.z, -8.0f + (16.0f * 0.04f));
	EXPECT_FLOAT_EQ(swirls.at(0).angles.x, -8.0f + (16.0f * 0.06f));
}

TEST(MenuBackdrop, ASwirlPullsTheCoordinatesOfThePointsNearIt)
{
	menu_backdrop::Swirls swirls {};
	// Every swirl at the grid's middle across and at its top: sin 0 = 0, cos(pi/2) = 0 puts it 20 squares in and 20 down
	for (auto& swirl : swirls)
	{
		swirl.angles = {0.0f, std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float> / 2.0f};
	}
	menu_backdrop::Layer layer {};
	menu_backdrop::PlaceLayer(swirls, 0, 0.0f, layer);
	// At the swirl each of the four odd and four even swirls adds (2 cos c + 3) * 3 / 10 = 0.9
	const auto at = layer.at((20 * menu_backdrop::k_Rows) + 20);
	EXPECT_NEAR(at.x, 3.6f, 1e-5f);
	EXPECT_NEAR(at.y, 3.6f, 1e-5f);
	// Further away the pull is weaker
	EXPECT_LT(layer.at(0).x, at.x);
}

TEST(MenuBackdrop, TheAnglesTurnAsEachPointIsPlacedAndWrap)
{
	menu_backdrop::Swirls swirls {};
	swirls.at(0).speeds = {1.0f, 0.0f, 0.0f};
	menu_backdrop::Layer layer {};
	// A frame of 1000 ms: each of the 1271 points turns the angle by 1e-4
	menu_backdrop::PlaceLayer(swirls, 0, 1000.0f * menu_backdrop::k_TurnPerMillisecond, layer);
	EXPECT_NEAR(swirls.at(0).angles.x, 0.1271f, 1e-4f);
	// The other layer's swirls stay
	EXPECT_EQ(swirls.at(8).angles.x, 0.0f);
	EXPECT_NEAR(menu_backdrop::Wrap(4.0f), 4.0f - (2.0f * std::numbers::pi_v<float>), 1e-6f);
	EXPECT_NEAR(menu_backdrop::Wrap(-4.0f), -4.0f + (2.0f * std::numbers::pi_v<float>), 1e-6f);
	EXPECT_EQ(menu_backdrop::Wrap(3.0f), 3.0f);
}

TEST(MenuBackdrop, ThePointsSpreadOverTheScreenInWholePixels)
{
	EXPECT_EQ(menu_backdrop::PointOnScreen(0, 0, {800, 600}), glm::ivec2(0, 0));
	EXPECT_EQ(menu_backdrop::PointOnScreen(40, 30, {800, 600}), glm::ivec2(800, 600));
	EXPECT_EQ(menu_backdrop::PointOnScreen(1, 1, {1366, 1024}), glm::ivec2(34, 34));
}

TEST(MenuBackdrop, TheScreenFadesInFromBlackAndOutToBlackOverASecond)
{
	menu_backdrop::Fade fade;
	menu_backdrop::StepFade(fade, false, 499);
	EXPECT_EQ(fade.alpha, 127);
	menu_backdrop::StepFade(fade, false, 496);
	EXPECT_EQ(fade.alpha, 1);
	// At 0 there is no black at all
	menu_backdrop::StepFade(fade, false, 10);
	EXPECT_EQ(fade.milliseconds, 0);
	EXPECT_EQ(fade.alpha, 0);
	EXPECT_FALSE(menu_backdrop::FadeOver(fade));
	// Once the box has gone it goes back to black, and is over past 999
	menu_backdrop::StepFade(fade, true, 999);
	EXPECT_EQ(fade.alpha, 254);
	EXPECT_FALSE(menu_backdrop::FadeOver(fade));
	menu_backdrop::StepFade(fade, true, 1);
	EXPECT_EQ(fade.alpha, 255);
	EXPECT_TRUE(menu_backdrop::FadeOver(fade));
}

TEST(MenuBackdrop, TheTextureKeepsFourBitsAColour)
{
	EXPECT_EQ(menu_backdrop::SixteenBitLevel(0), 0);
	EXPECT_EQ(menu_backdrop::SixteenBitLevel(15), 0);
	EXPECT_EQ(menu_backdrop::SixteenBitLevel(16), 17);
	EXPECT_EQ(menu_backdrop::SixteenBitLevel(0x7f), 7 * 17);
	EXPECT_EQ(menu_backdrop::SixteenBitLevel(255), 255);
}

TEST(MenuBackdrop, TheSwirlsTurnAtMostSixtyTimesASecond)
{
	menu_backdrop::Pace pace;
	EXPECT_FALSE(menu_backdrop::StepPace(pace, 10.0f).has_value());
	EXPECT_EQ(menu_backdrop::StepPace(pace, 10.0f), 20);
	EXPECT_EQ(menu_backdrop::StepPace(pace, 40.0f), 40);
}

TEST(MenuBackdrop, TheFramesMillisecondsCountFromTheLastFrameItWasDrawn)
{
	// The first frame the backdrop is drawn over the game turns nothing; then each frame counts from the last
	EXPECT_FLOAT_EQ(menu_backdrop::FrameMilliseconds(std::nullopt, 5000), 0.0f);
	EXPECT_FLOAT_EQ(menu_backdrop::FrameMilliseconds(5000, 5016), 16.0f);
	// The clock wrapping round still gives the time between
	EXPECT_FLOAT_EQ(menu_backdrop::FrameMilliseconds(0xFFFFFFF0u, 0x10u), 32.0f);
}
