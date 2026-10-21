/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Gui/ScreenFade.h"

using namespace openblack::gui;

namespace
{
const glm::vec3 k_White(1.0f);

void Step(ScreenFade& fade, uint32_t milliseconds, int frames, bool inTemple = false)
{
	for (int i = 0; i < frames; ++i)
	{
		fade.Update(milliseconds, inTemple);
	}
}
} // namespace

TEST(ScreenFade, StartsClear)
{
	const ScreenFade fade;
	EXPECT_EQ(fade.GetColour().a, 0.0f);
}

TEST(ScreenFade, FadesAwayOverASecond)
{
	ScreenFade fade;
	fade.FadeFrom(1.0f, k_White);
	EXPECT_EQ(fade.GetColour(), glm::vec4(1.0f));
	Step(fade, 250, 2);
	EXPECT_FLOAT_EQ(fade.GetAmount(), 0.5f);
	// The alpha is in steps of 1/255 that round down
	EXPECT_FLOAT_EQ(fade.GetColour().a, 127.0f / 255.0f);
	EXPECT_EQ(glm::vec3(fade.GetColour()), k_White);
	Step(fade, 300, 1);
	EXPECT_EQ(fade.GetTurns(), 0u);
	Step(fade, 300, 1);
	EXPECT_EQ(fade.GetColour(), glm::vec4(0.0f));
	EXPECT_EQ(fade.GetTurns(), 1u);
}

// A fade that comes to its amount exactly has not gone past it. Outside the temple it then stays there.
TEST(ScreenFade, AFadeThatComesToItsAmountExactlyStaysThereOutsideTheTemple)
{
	ScreenFade fade;
	fade.FadeFrom(1.0f, k_White);
	Step(fade, 250, 4);
	EXPECT_EQ(fade.GetAmount(), 0.0f);
	Step(fade, 16, 3);
	EXPECT_EQ(fade.GetTurns(), 0u);
	EXPECT_EQ(fade.GetColour(), glm::vec4(k_White, 0.0f));
	// In the temple it steps on, turns and goes back to black
	Step(fade, 16, 1, true);
	EXPECT_EQ(fade.GetTurns(), 1u);
	EXPECT_EQ(fade.GetColour(), glm::vec4(0.0f));
}

// A fade up that comes to its amount exactly does not turn. In the temple it then turns each draw, but it does not fade
// back down.
TEST(ScreenFade, AFadeUpThatComesToItsAmountExactlyDoesNotFadeBackDown)
{
	ScreenFade fade;
	fade.FadeThrough(k_White);
	Step(fade, 500, 2);
	EXPECT_EQ(fade.GetAmount(), 1.0f);
	Step(fade, 16, 2);
	EXPECT_EQ(fade.GetTurns(), 0u);
	Step(fade, 16, 3, true);
	EXPECT_EQ(fade.GetTurns(), 3u);
	EXPECT_EQ(fade.GetColour(), glm::vec4(1.0f));
}

TEST(ScreenFade, StaysCoveredBeyondAWhole)
{
	ScreenFade fade;
	fade.FadeFrom(1.2f);
	Step(fade, 100, 1);
	EXPECT_FLOAT_EQ(fade.GetColour().a, 1.0f);
	Step(fade, 100, 2);
	EXPECT_NEAR(fade.GetAmount(), 0.9f, 1e-5f);
	EXPECT_FLOAT_EQ(fade.GetColour().a, 229.0f / 255.0f);
}

TEST(ScreenFade, KeepsItsColourUntilFadedAway)
{
	ScreenFade fade;
	fade.FadeFrom(1.0f, k_White);
	Step(fade, 500, 1);
	// A fade without a colour of its own takes on the one still fading
	fade.FadeFrom(1.2f);
	EXPECT_EQ(glm::vec3(fade.GetColour()), k_White);
	Step(fade, 500, 3);
	// Faded away, the colour goes back to black for the next
	fade.FadeFrom(1.0f);
	EXPECT_EQ(fade.GetColour(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

TEST(ScreenFade, FadesThroughAColour)
{
	ScreenFade fade;
	fade.FadeThrough(k_White);
	Step(fade, 600, 1);
	EXPECT_FLOAT_EQ(fade.GetAmount(), 0.6f);
	EXPECT_EQ(fade.GetTurns(), 0u);
	Step(fade, 600, 1);
	EXPECT_FLOAT_EQ(fade.GetColour().a, 1.0f);
	EXPECT_EQ(fade.GetTurns(), 1u);
	Step(fade, 600, 1);
	EXPECT_EQ(glm::vec3(fade.GetColour()), k_White);
	Step(fade, 600, 1);
	EXPECT_EQ(fade.GetColour(), glm::vec4(0.0f));
	EXPECT_EQ(fade.GetTurns(), 2u);
}

TEST(ScreenFade, TellsHowMuchItCoversBeyondAWhole)
{
	ScreenFade fade;
	// Coming into the temple, the fade from 1.2 is down to 0.2 after a second
	fade.FadeFrom(1.2f);
	EXPECT_FLOAT_EQ(fade.GetAmount(), 1.2f);
	Step(fade, 250, 4);
	EXPECT_NEAR(fade.GetAmount(), 0.2f, 1e-5f);
	// Cutting the flight short covers the screen again
	fade.FadeFrom(1.0f);
	EXPECT_FLOAT_EQ(fade.GetAmount(), 1.0f);
}

// Outside the temple a fade at its amount stays as it is. Inside, it steps each draw and turns again.
TEST(ScreenFade, AFadeAtItsAmountTurnsAgainOnlyInTheTemple)
{
	ScreenFade fade;
	Step(fade, 16, 3);
	EXPECT_EQ(fade.GetTurns(), 0u);
	Step(fade, 16, 3, true);
	EXPECT_EQ(fade.GetTurns(), 3u);
	EXPECT_EQ(fade.GetAmount(), 0.0f);
	EXPECT_EQ(fade.GetColour(), glm::vec4(0.0f));
}

// The step is the whole milliseconds times a thousandth, each rounded to a float, as the game does it
TEST(ScreenFade, StepsByTheMillisecondsInFloats)
{
	ScreenFade fade;
	fade.FadeFrom(1.0f);
	Step(fade, 7, 1);
	EXPECT_EQ(fade.GetAmount(), 1.0f - 7.0f * 0.001f);
	Step(fade, 33, 1);
	EXPECT_EQ(fade.GetAmount(), (1.0f - 7.0f * 0.001f) - 33.0f * 0.001f);
}
