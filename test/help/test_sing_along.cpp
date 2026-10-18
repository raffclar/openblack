/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "Common/Zoomer.h"
#include "Help/SingAlong.h"

using namespace openblack;
using namespace openblack::help::sing_along;

TEST(SingAlong, OnlyTheThreeSungVersesShowTheHighlight)
{
	EXPECT_FALSE(IsSungVerse(50)); // the missionaries' background tune
	EXPECT_TRUE(IsSungVerse(51));
	EXPECT_TRUE(IsSungVerse(52));
	EXPECT_TRUE(IsSungVerse(53));
	EXPECT_FALSE(IsSungVerse(54));
	EXPECT_FALSE(IsSungVerse(0));
}

TEST(SingAlong, TheGlideTimeIsTheGamesConstant)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_GlideSeconds), 0x3E3851ECu);
}

TEST(SingAlong, TheStepIsRealTimeInTheTempleAndGameTimeOutsideCappedAt500)
{
	EXPECT_EQ(StepMilliseconds(false, 16, 10), 10);
	EXPECT_EQ(StepMilliseconds(true, 16, 10), 16);
	EXPECT_EQ(StepMilliseconds(false, 16, 0), 0); // paused
	EXPECT_EQ(StepMilliseconds(false, 16, -3), 0);
	EXPECT_EQ(StepMilliseconds(false, 16, 900), 500);
	EXPECT_EQ(StepMilliseconds(true, 700, 0), 500);
}

TEST(SingAlong, TheGlowHasTheWordsColourAndHalfItsAlpha)
{
	const auto colour = GlowColour(255, 0, 51, 255);
	EXPECT_FLOAT_EQ(colour.r, 1.0f);
	EXPECT_FLOAT_EQ(colour.g, 0.0f);
	EXPECT_FLOAT_EQ(colour.b, 51.0f / 255.0f);
	EXPECT_FLOAT_EQ(colour.a, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(GlowColour(0, 0, 0, 1).a, 0.0f);
}

TEST(SingAlong, TheBoxGlidesFromWhereItWasAndCutsItsSidesToWholePixels)
{
	Highlight highlight;
	// Three zoomers set off from 0 as the box does, for one step of 20 ms
	Zoomer left;
	Zoomer top;
	Zoomer width;
	left.SetDestination(905.0f, k_GlideSeconds);
	top.SetDestination(928.0f, k_GlideSeconds);
	width.SetDestination(30.0f, k_GlideSeconds);
	left.Update(0.02f);
	top.Update(0.02f);
	width.Update(0.02f);
	const auto box = highlight.Step({905.0f, 928.0f}, 30.0f, 36.0f, 20);
	EXPECT_EQ(box.min.x, static_cast<int>(left.GetValue()));
	EXPECT_EQ(box.min.y, static_cast<int>(top.GetValue()));
	EXPECT_EQ(box.max.x, static_cast<int>(width.GetValue() + left.GetValue()));
	EXPECT_EQ(box.max.y, static_cast<int>(top.GetValue() + 36.0f));
	// Part of the way only
	EXPECT_GT(box.min.x, 0);
	EXPECT_LT(box.min.x, 905);
}

TEST(SingAlong, TheBoxCreepsUpOnTheWordAndStaysWhileTheFramesArePaused)
{
	Highlight highlight;
	Box box {};
	const auto steps = [&highlight, &box](int frames) {
		for (int frame = 0; frame < frames; ++frame)
		{
			box = highlight.Step({905.0f, 928.0f}, 30.0f, 36.0f, 20);
		}
	};
	// Sent on again each frame, the box only creeps up on the word: cut to whole pixels it is one short after 0.6 s
	steps(30);
	EXPECT_EQ(box.min.x, 904);
	EXPECT_EQ(box.max.y, 963);
	// and on it after 1.2 s
	steps(30);
	EXPECT_EQ(box.min.x, 905);
	EXPECT_EQ(box.min.y, 928);
	EXPECT_EQ(box.max.x, 935);
	EXPECT_EQ(box.max.y, 964);
	// The next word: a paused frame does not move the box
	box = highlight.Step({965.0f, 928.0f}, 50.0f, 36.0f, 0);
	EXPECT_EQ(box.min.x, 905);
	// A step longer than the glide puts it on the word
	box = highlight.Step({965.0f, 928.0f}, 50.0f, 36.0f, 500);
	EXPECT_EQ(box.min.x, 965);
	EXPECT_EQ(box.max.x, 1015);
}
