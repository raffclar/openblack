/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Gui/LogoEffect.h"
#include "Gui/PauseText.h"

using namespace openblack;
using namespace openblack::gui::pause_text;

namespace
{
constexpr logo_effect::Screen k_Screen {.width = 800, .height = 600};
constexpr float k_WorldLens = 1.2217305f; // 70 degrees

/// A camera at the middle of the world that looks north, the world's up kept up
logo_effect::View NorthView()
{
	return logo_effect::LookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 100.0f});
}

Situation Paused()
{
	return {.paused = true};
}

/// The words of a fader after some steps of 10 milliseconds
std::optional<Shown> StepFor(Fader& fader, const Situation& situation, int steps)
{
	std::optional<Shown> shown;
	for (int i = 0; i < steps; ++i)
	{
		shown = fader.Step(situation, 10);
	}
	return shown;
}
} // namespace

TEST(PauseText, TheKindFollowsThePauseThenTheSpeed)
{
	EXPECT_EQ(KindOf(true, 2.0f), Kind::Paused);
	EXPECT_EQ(KindOf(false, 0.5f), Kind::Slower);
	EXPECT_EQ(KindOf(false, 2.0f), Kind::Faster);
	EXPECT_EQ(KindOf(false, 1.0f), Kind::None);
}

TEST(PauseText, TheEaseIsSmoothAndHeldAtItsEnds)
{
	EXPECT_EQ(Ease(-0.5f), 0.0f);
	EXPECT_EQ(Ease(0.0f), 0.0f);
	EXPECT_NEAR(Ease(0.5f), 0.5f, 1e-6f);
	EXPECT_EQ(Ease(1.0f), 1.0f);
	EXPECT_EQ(Ease(2.0f), 1.0f);
	EXPECT_LT(Ease(0.25f), 0.25f);
}

TEST(PauseText, TheWordsFadeInOverAThirdOfASecond)
{
	Fader fader;
	// The first frame takes the new words at no fade, so nothing shows yet
	EXPECT_FALSE(fader.Step(Paused(), 10).has_value());
	EXPECT_EQ(fader.GetKind(), Kind::Paused);
	const auto first = fader.Step(Paused(), 10);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->kind, Kind::Paused);
	EXPECT_GT(first->eased, 0.0f);
	EXPECT_LT(first->eased, 0.01f);
	// 0.003 a millisecond: full after 340 more milliseconds, and held there
	StepFor(fader, Paused(), 31);
	EXPECT_LT(fader.GetFade(), 1.0f);
	const auto full = StepFor(fader, Paused(), 2);
	ASSERT_TRUE(full.has_value());
	EXPECT_EQ(fader.GetFade(), 1.0f);
	EXPECT_EQ(full->eased, 1.0f);
}

TEST(PauseText, TheOldWordsFadeOutBeforeTheNewWordsShow)
{
	Fader fader;
	StepFor(fader, Paused(), 60);
	ASSERT_EQ(fader.GetFade(), 1.0f);
	// Slower now: the paused words fade out over half a second first
	const Situation slower {.speed = 0.5f};
	const auto leaving = StepFor(fader, slower, 25);
	ASSERT_TRUE(leaving.has_value());
	EXPECT_EQ(leaving->kind, Kind::Paused);
	EXPECT_NEAR(fader.GetFade(), 0.5f, 1e-4f);
	StepFor(fader, slower, 26);
	EXPECT_EQ(fader.GetKind(), Kind::Slower);
	EXPECT_LT(fader.GetFade(), 0.05f);
	const auto coming = StepFor(fader, slower, 10);
	ASSERT_TRUE(coming.has_value());
	EXPECT_EQ(coming->kind, Kind::Slower);
}

TEST(PauseText, ABoxWithABackgroundHidesTheWordsAndStartsTheirFadeAgain)
{
	Fader fader;
	StepFor(fader, Paused(), 60);
	auto behindMenu = Paused();
	behindMenu.boxWithBackground = true;
	EXPECT_FALSE(fader.Step(behindMenu, 10).has_value());
	EXPECT_EQ(fader.GetFade(), 0.0f);
	EXPECT_EQ(fader.GetKind(), Kind::Paused);
	// The box closes with the game still paused: the words fade in again from none
	EXPECT_FALSE(fader.Step(Paused(), 0).has_value());
	const auto again = fader.Step(Paused(), 10);
	ASSERT_TRUE(again.has_value());
	EXPECT_LT(again->eased, 0.01f);
}

TEST(PauseText, TheTempleTheVideoAndTheEditorHideTheWords)
{
	for (const auto hide : {&Situation::insideTemple, &Situation::videoPlaying, &Situation::editing})
	{
		Fader fader;
		StepFor(fader, Paused(), 60);
		auto situation = Paused();
		situation.*hide = true;
		EXPECT_FALSE(fader.Step(situation, 10).has_value());
		EXPECT_EQ(fader.GetFade(), 0.0f);
	}
}

TEST(PauseText, AScriptWithTheSpeedHidesOnlyTheSpeedWords)
{
	Fader paused;
	auto situation = Paused();
	situation.scriptHasSpeed = true;
	EXPECT_TRUE(StepFor(paused, situation, 20).has_value());

	Fader slower;
	const Situation scripted {.speed = 0.5f, .scriptHasSpeed = true};
	EXPECT_FALSE(StepFor(slower, scripted, 20).has_value());
	EXPECT_EQ(slower.GetFade(), 0.0f);
}

TEST(PauseText, NoWordsShowBeforeTheGameStartsButTheFadeGoesOn)
{
	Fader fader;
	const Situation loading {.paused = true, .started = false};
	EXPECT_FALSE(StepFor(fader, loading, 20).has_value());
	EXPECT_GT(fader.GetFade(), 0.5f);
	EXPECT_EQ(fader.GetTurn(), 0.0f);
}

TEST(PauseText, TheLettersTurnOnlyWhileTheWordsShowAndGoRoundTwoTurns)
{
	Fader fader;
	StepFor(fader, Paused(), 2);
	const float before = fader.GetTurn();
	EXPECT_TRUE(fader.Step(Paused(), 100).has_value());
	EXPECT_NEAR(fader.GetTurn() - before, 0.15f, 1e-5f);
	// 0.0015 a millisecond: two full turns in about 8.4 seconds
	StepFor(fader, Paused(), 838);
	EXPECT_LT(fader.GetTurn(), 0.5f);
	EXPECT_LE(fader.GetTurn(), k_FourPi);
	const float shownTurn = fader.GetTurn();
	StepFor(fader, {}, 100);
	StepFor(fader, {}, 10);
	EXPECT_EQ(fader.GetKind(), Kind::None);
	const float afterFade = fader.GetTurn();
	StepFor(fader, {}, 100);
	EXPECT_EQ(fader.GetTurn(), afterFade);
	EXPECT_NE(afterFade, shownTurn);
}

TEST(PauseText, TheLettersAreSpacedInTheFourByThreeMiddle)
{
	EXPECT_EQ(LayoutWidth(k_Screen), 800);
	EXPECT_EQ(LayoutWidth({.width = 1280, .height = 1024}), 1280);
	EXPECT_EQ(LayoutWidth({.width = 1920, .height = 1080}), 1440);
	EXPECT_EQ(LayoutWidth({.width = 5120, .height = 1440}), 1920);
}

TEST(PauseText, TheRowIsCentredAtATwentiethOfTheHeight)
{
	const Shown full {.kind = Kind::Paused, .eased = 1.0f, .turn = 0.0f};
	constexpr size_t k_Length = 11; // Game Paused
	const auto first = PlaceLetter(0, k_Length, full, k_Screen, LayoutWidth(k_Screen), 0.3f);
	const auto last = PlaceLetter(k_Length - 1, k_Length, full, k_Screen, LayoutWidth(k_Screen), 0.3f);
	EXPECT_EQ(first.depth, 30.0f);
	EXPECT_EQ(first.pixel.y, 30);
	// Fully in, the letters share half the width: 400 / 11 pixels apart
	EXPECT_EQ(first.pixel.x, 218);
	EXPECT_EQ(last.pixel.x, 581);
	// Half in, the words are nearer and twice as wide
	const Shown half {.kind = Kind::Paused, .eased = 0.0f, .turn = 0.0f};
	const auto near = PlaceLetter(0, k_Length, half, k_Screen, LayoutWidth(k_Screen), 0.3f);
	EXPECT_EQ(near.depth, 15.0f);
	EXPECT_EQ(near.pixel.x, 36);
	// A wide screen keeps the 4:3 spaces, centred
	const logo_effect::Screen wide {.width = 1920, .height = 1080};
	const auto wideFirst = PlaceLetter(0, k_Length, full, wide, LayoutWidth(wide), 0.3f);
	const auto wideLast = PlaceLetter(k_Length - 1, k_Length, full, wide, LayoutWidth(wide), 0.3f);
	EXPECT_EQ(wideFirst.pixel.y, 54);
	EXPECT_NEAR(static_cast<float>(wideLast.pixel.x - wideFirst.pixel.x), 720.0f * 10.0f / 11.0f, 1.0f);
	EXPECT_NEAR(static_cast<float>(wideLast.pixel.x + wideFirst.pixel.x), 1920.0f, 1.0f);
}

TEST(PauseText, AStillLetterStandsUprightFacingTheCamera)
{
	const Shown still {.kind = Kind::Paused, .eased = 1.0f, .turn = 0.0f};
	const auto letter = PlaceLetter(0, 1, still, k_Screen, 800, 0.0f);
	// Along the right, down the screen, and off the page away from the camera
	EXPECT_NEAR(letter.turn[0].x, 1.0f, 1e-6f);
	EXPECT_NEAR(letter.turn[1].y, -1.0f, 1e-6f);
	EXPECT_NEAR(letter.turn[2].z, -1.0f, 1e-6f);
	// A tilt leans the letter back with its row
	const auto tilted = PlaceLetter(0, 1, still, k_Screen, 800, 0.4f);
	EXPECT_NEAR(tilted.turn[1].y, -std::cos(0.4f), 1e-6f);
	EXPECT_NEAR(tilted.turn[1].z, std::sin(0.4f), 1e-6f);
}

TEST(PauseText, TheLettersTipOverThenTurnRoundAndAlwaysShowTheirFace)
{
	// A quarter turn and a little in, the letter tips past its edge and shows its face from the other side
	const Shown tipping {.kind = Kind::Paused, .eased = 1.0f, .turn = k_HalfPi + 0.1f};
	const auto tipped = PlaceLetter(0, 1, tipping, k_Screen, 800, 0.0f);
	EXPECT_NEAR(tipped.turn[0].x, 1.0f, 1e-6f);
	EXPECT_LT(std::abs(tipped.turn[1].y), 0.2f);
	// In the second full turn it turns round the up axis instead
	const Shown turning {.kind = Kind::Paused, .eased = 1.0f, .turn = k_TwoPi + 0.5f};
	const auto turned = PlaceLetter(0, 1, turning, k_Screen, 800, 0.0f);
	EXPECT_NEAR(turned.turn[1].y, -1.0f, 1e-6f);
	EXPECT_NEAR(turned.turn[0].x, std::cos(0.5f), 1e-6f);
	// Each letter lags a third of a radian behind the one before
	const auto second = PlaceLetter(1, 2, turning, k_Screen, 800, 0.0f);
	EXPECT_NEAR(second.turn[0].x, std::cos(0.5f - (1.0f / 3.0f)), 1e-5f);
}

TEST(PauseText, TheTiltIsTheAngleUpToTheRow)
{
	const auto view = NorthView();
	const auto lens = logo_effect::LensOf(k_WorldLens, 1.0f, k_Screen);
	const float tilt = Tilt(view, lens, k_Screen);
	// The row is 270 of the 300 pixels up from the middle, on a lens that sees 0.525 up at a distance of 1
	EXPECT_NEAR(tilt, std::atan(lens.extentY * 270.0f / 300.0f), 1e-5f);
}

TEST(PauseText, TheFrameIsUnderThePixelAtItsDepthInTheCamerasAxes)
{
	const auto view = NorthView();
	const auto lens = logo_effect::LensOf(k_WorldLens, 1.0f, k_Screen);
	const Shown still {.kind = Kind::Paused, .eased = 1.0f, .turn = 0.0f};
	const auto letter = PlaceLetter(0, 1, still, k_Screen, 800, 0.0f);
	const auto frame = FrameOf(letter, view, lens, k_Screen);
	EXPECT_NEAR(frame.origin.z, 30.0f, 1e-4f);
	EXPECT_NEAR(frame.origin.x, 0.0f, 1e-4f);
	EXPECT_GT(frame.origin.y, 0.0f);
	EXPECT_NEAR(glm::distance(frame.axes[0], view.right), 0.0f, 1e-6f);
	EXPECT_NEAR(glm::distance(frame.axes[1], -view.up), 0.0f, 1e-6f);
	EXPECT_NEAR(glm::distance(frame.axes[2], -view.forward), 0.0f, 1e-6f);
}

TEST(PauseText, TheAlphaAndTheLetterOffset)
{
	EXPECT_EQ(AlphaOf(1.0f), 255);
	EXPECT_EQ(AlphaOf(0.5f), 127);
	EXPECT_EQ(AlphaOf(0.0f), 0);
	// 40 font units at size 2 are a width of 1, so the letter starts half of it to the left
	EXPECT_FLOAT_EQ(LetterOffset(40.0f), -0.5f);
}
