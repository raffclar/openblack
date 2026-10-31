/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/Mists.h"

namespace mists = openblack::mists;

TEST(Mists, FramesFollowTheCounter)
{
	EXPECT_EQ(mists::Frame(0), 0);
	EXPECT_EQ(mists::Frame(20), 1);
	EXPECT_EQ(mists::Frame(319), 15);
	// Past sixteen frames they start again, and the counter's own end is frame 13
	EXPECT_EQ(mists::Frame(320), 0);
	EXPECT_EQ(mists::Frame(900), 13);

	EXPECT_EQ(mists::FrameOffset(9, false), glm::vec2(0.125f, 0.125f));
	EXPECT_EQ(mists::FrameOffset(9, true), glm::vec2(0.125f, 0.375f));
	EXPECT_EQ(mists::StartCounter(15.9f), 15);
}

TEST(Mists, AnimationKeepsItsPaceAtAnyFrameRate)
{
	// A millisecond at a time moves the counter as far as a whole second does, but for rounding
	int counter = 0;
	float remainder = 0.0f;
	for (int i = 0; i < 1000; ++i)
	{
		mists::Advance(counter, remainder, 1.0f);
	}
	int once = 0;
	float onceRemainder = 0.0f;
	mists::Advance(once, onceRemainder, 1000.0f);
	EXPECT_NEAR(counter, once, 1);
	EXPECT_EQ(once, 255);

	// It wraps only once it passes 900
	int full = 900;
	float none = 0.0f;
	mists::Advance(full, none, 0.0f);
	EXPECT_EQ(full, 900);
	mists::Advance(full, none, 4.0f);
	EXPECT_EQ(full, 1);
}

TEST(Mists, ShrinkEdgeOn)
{
	// Seen from straight above: the full size; seen level: edgeShrink times smaller
	EXPECT_FLOAT_EQ(mists::EdgeOnSize(10.0f, 3.0f, {0.0f, -100.0f, 0.0f}), 10.0f);
	EXPECT_FLOAT_EQ(mists::EdgeOnSize(10.0f, 3.0f, {100.0f, 0.0f, 0.0f}), 10.0f / 3.0f);
	EXPECT_FLOAT_EQ(mists::EdgeOnSize(10.0f, 3.0f, {0.0f, 0.0f, 0.0f}), 10.0f);
}

TEST(Mists, AScriptMistTakesTheWholePartsOfItsColour)
{
	// The Singing Stones' white mist at a transparency of 0.1: an alpha of 25, from 25.5
	EXPECT_EQ(mists::ScriptColour(255.0f, 255.0f, 255.0f, 0.1f), 0x19FFFFFFu);
	EXPECT_EQ(mists::ScriptColour(12.9f, 128.0f, 0.5f, 1.0f), 0xFF0C8000u);
	// Each channel keeps only its low byte
	EXPECT_EQ(mists::ScriptColour(256.0f, 257.0f, -1.0f, 0.0f), 0x000001FFu);
}

TEST(Mists, AFadeStepsItsSizeAndAlphaOnceEachTurn)
{
	// The Singing Stones' fade: the size from 1 to 2 and the alpha from 50 to 0 in 10 seconds, 100 turns
	auto fade = mists::StartFade(1.0f, 2.0f, 50.0f, 0.0f, 10.0f);
	EXPECT_FLOAT_EQ(fade.turns, 100.0f);
	EXPECT_EQ(fade.sizeStep, 1.0f / 100.0f);
	EXPECT_EQ(fade.alphaStep, -50.0f / 100.0f);
	EXPECT_EQ(mists::AlphaByte(fade.alpha), 50);
	float size = 1.0f;
	ASSERT_TRUE(mists::StepFade(fade, size));
	EXPECT_EQ(size, 1.0f + 0.01f);
	EXPECT_EQ(fade.alpha, 49.5f);
	EXPECT_EQ(mists::AlphaByte(fade.alpha), 49);
	for (int turn = 1; turn < 100; ++turn)
	{
		ASSERT_TRUE(mists::StepFade(fade, size));
	}
	EXPECT_NEAR(size, 2.0f, 1e-4f);
	EXPECT_EQ(fade.alpha, 0.0f);
	// The fade is done: nothing changes
	const float last = size;
	EXPECT_FALSE(mists::StepFade(fade, size));
	EXPECT_EQ(size, last);
}

TEST(Mists, AFadeKeepsItsAlphaInAByte)
{
	auto up = mists::StartFade(1.0f, 1.0f, 200.0f, 400.0f, 1.0f);
	float size = 1.0f;
	for (int turn = 0; turn < 10; ++turn)
	{
		ASSERT_TRUE(mists::StepFade(up, size));
	}
	EXPECT_EQ(up.alpha, 255.0f);
	auto down = mists::StartFade(1.0f, 1.0f, 10.0f, -100.0f, 1.0f);
	ASSERT_TRUE(mists::StepFade(down, size));
	EXPECT_EQ(down.alpha, 0.0f);
	// A fade of no time is done at once
	auto none = mists::StartFade(1.0f, 2.0f, 0.0f, 255.0f, 0.0f);
	EXPECT_FALSE(mists::StepFade(none, size));
}

TEST(Mists, ALandMistDrawsWithTheAlphaThatItsWholeAlphaGives)
{
	// 255 times the whole alpha, divided by 256, in the low byte
	EXPECT_EQ(mists::DrawnAlpha(0), 0);
	EXPECT_EQ(mists::DrawnAlpha(1), 0);
	EXPECT_EQ(mists::DrawnAlpha(25), 24);
	EXPECT_EQ(mists::DrawnAlpha(50), 49);
	EXPECT_EQ(mists::DrawnAlpha(128), 127);
	EXPECT_EQ(mists::DrawnAlpha(255), 254);
	EXPECT_EQ(mists::DrawnAlpha(256), 255);
	EXPECT_EQ(mists::DrawnAlpha(300), 42);
	EXPECT_EQ(mists::DrawnAlpha(-1), 255);
}

TEST(Mists, AStartAlphaPastAByteKeepsItsWholePart)
{
	// The start alpha of a fade is not kept in 0 to 255
	const auto fade = mists::StartFade(1.0f, 1.0f, 300.0f, 0.0f, 1.0f);
	const auto whole = mists::WholeAlpha(fade.alpha);
	EXPECT_EQ(whole, 300);
	const uint32_t colour = (static_cast<uint32_t>(mists::AlphaByte(fade.alpha)) << 24u) | 0xFFFFFFu;
	EXPECT_EQ(mists::WholeAlphaOf(colour, mists::AlphaHighBits(whole)), 300);
	EXPECT_EQ(mists::WholeAlphaOf(colour, 0), 44);
	EXPECT_EQ(mists::WholeAlphaOf(0xFFFFFFFFu, mists::AlphaHighBits(-1)), -1);
	EXPECT_EQ(mists::WholeAlpha(-0.5f), 0);
}
