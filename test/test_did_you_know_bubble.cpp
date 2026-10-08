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
#include <vector>

#include <gtest/gtest.h>

#include "Help/Bubble.h"

// The did-you-know bubble's Bubble class.

using namespace openblack::help;

TEST(DidYouKnowBubble, DefaultSizes)
{
	// the default sizes of (400, 250, 19)
	const auto s = DefaultBubbleSizes(400.0f, 250.0f, 19.0f);
	EXPECT_FLOAT_EQ(s.margin, 40.0f);
	EXPECT_FLOAT_EQ(s.farWidth, 200.0f / 7.0f);
	EXPECT_FLOAT_EQ(s.farHeight, 125.0f / 7.0f);
	EXPECT_FLOAT_EQ(s.farMargin, 125.0f / 35.0f);
	EXPECT_FLOAT_EQ(s.farTextSize, 19.0f / 7.0f);
}

TEST(DidYouKnowBubble, LayoutNearFarAndFade)
{
	const auto s = DefaultBubbleSizes(400.0f, 250.0f, 19.0f);
	EXPECT_FALSE(LayoutBubble(s, 0.5f, {400.0f, 300.0f}, 10.0f, 0.2f).has_value()); // depth < 1
	// at 3 m (the near depth): full size, fade 0 (it fades in up to 8 m)
	const auto near = LayoutBubble(s, 3.0f, {400.0f, 300.0f}, 10.0f, 0.2f);
	ASSERT_TRUE(near.has_value());
	EXPECT_FLOAT_EQ(near->width, 400.0f);
	EXPECT_FLOAT_EQ(near->fade, 0.0f);
	// top left = (sx + offset - place w, sy - margin - h)
	EXPECT_FLOAT_EQ(near->topLeft.x, 400.0f + 10.0f - 0.2f * 400.0f);
	EXPECT_FLOAT_EQ(near->topLeft.y, 300.0f - 40.0f - 250.0f);
	EXPECT_FALSE(near->tailRight);
	// at 100 m: fully visible
	EXPECT_FLOAT_EQ(LayoutBubble(s, 100.0f, {0.0f, 0.0f}, -10.0f, 0.5f)->fade, 1.0f);
	EXPECT_TRUE(LayoutBubble(s, 100.0f, {0.0f, 0.0f}, -10.0f, 0.5f)->tailRight);
	// at 450 m: far size, faded out
	const auto far = LayoutBubble(s, 450.0f, {0.0f, 0.0f}, 10.0f, 0.2f);
	// a + (b - a) x f in float, step by step as the original: a few ulp off b
	const float f = 1.0f;
	EXPECT_FLOAT_EQ(far->width, s.width + (s.farWidth - s.width) * f);
	EXPECT_FLOAT_EQ(far->fade, 0.0f);
}

TEST(DidYouKnowBubble, TailSide)
{
	EXPECT_FLOAT_EQ(TailFor(0.0f, 800).offset, 10.0f); // left edge: u = -1
	EXPECT_NEAR(TailFor(0.0f, 800).place, 0.8f, 1e-6f);
	EXPECT_FLOAT_EQ(TailFor(400.0f, 800).offset, 10.0f); // the middle: u = 0
	EXPECT_NEAR(TailFor(400.0f, 800).place, 0.2f, 1e-6f);
	EXPECT_FLOAT_EQ(TailFor(600.0f, 800).offset, -10.0f);  // right half
	EXPECT_FLOAT_EQ(TailFor(2000.0f, 800).offset, -10.0f); // past the right edge: u = 1
}

TEST(DidYouKnowBubble, DrawDepth)
{
	// level camera (pitch 0): 5 m in front of the sign; looking straight down: the near plane x 2
	EXPECT_FLOAT_EQ(DrawDepth(100.0f, 0.0f, 1.0f), 95.0f);
	EXPECT_FLOAT_EQ(DrawDepth(100.0f, 1.5707963705062866f, 1.0f), 2.0f);
	EXPECT_FLOAT_EQ(DrawDepth(4.0f, 0.0f, 1.0f), 2.0f); // never nearer than 2 x near
}

TEST(DidYouKnowBubble, LifeScrollAndDrag)
{
	Bubble b;
	b.KeepAlive();
	EXPECT_FLOAT_EQ(b.Life(), 3.0f);
	b.Age(5000.0f); // 5 s x 0.0002 per ms = 1.0
	EXPECT_FLOAT_EQ(b.Life(), 2.0f);
	b.Age(100000.0f);
	EXPECT_FLOAT_EQ(b.Life(), 0.0f);

	b.Restart(); // 1000 lines and no text yet: nothing to clamp to but 0
	auto marks = b.ClampScroll(250.0f, 40.0f, 19.0f);
	EXPECT_FLOAT_EQ(b.Scroll(), 1000.0f);
	b.SetTextHeight(300.0f);                     // the last draw's text: clamped to the last line that fits
	marks = b.ClampScroll(250.0f, 40.0f, 19.0f); // (300 - 170) / 19
	EXPECT_FLOAT_EQ(b.Scroll(), 130.0f / 19.0f);
	EXPECT_TRUE(marks.above);
	EXPECT_FALSE(marks.below);
	b.Drag(true, 100, 19.0f); // the first frame takes the point
	b.Drag(true, 81, 19.0f);  // 19 pixels up: one line back
	EXPECT_FLOAT_EQ(b.Scroll(), 130.0f / 19.0f - 1.0f);
	marks = b.ClampScroll(250.0f, 40.0f, 19.0f);
	EXPECT_FALSE(marks.below); // last - 1 > scroll is false at exactly one line back
	b.Drag(false, 0, 19.0f);
	b.Drag(true, 0, 19.0f); // a new drag starts from its own point
	EXPECT_FLOAT_EQ(b.Scroll(), 130.0f / 19.0f - 1.0f);
	b.Restart(); // the text height back to 0
	EXPECT_FLOAT_EQ(b.TextHeight(), 0.0f);
}

TEST(DidYouKnowBubble, WrapGatheringText)
{
	// a fixed width of 10 per character
	const auto width = [](std::u16string_view s) { return 10.0f * static_cast<float>(s.size()); };
	auto lines = WrapGatheringText(u"one two three", 70.0f, width); // 7 characters a line
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(lines[0], u"one two");
	EXPECT_EQ(lines[1], u"three");
	lines = WrapGatheringText(u"well-known", 60.0f, width); // breaks after the hyphen, kept
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(lines[0], u"well-");
	EXPECT_EQ(lines[1], u"known");
	lines = WrapGatheringText(u"ab\ncd", 100.0f, width); // a line feed ends the line
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(lines[0], u"ab");
	EXPECT_EQ(lines[1], u"cd");
}

TEST(DidYouKnowBubble, PartWrapMatchesWrapGatheringText)
{
	// the bubble's three parts over a sequence of frames whose text, width and size change and come back: each cached
	// part breaks as WrapGatheringText does, and a part whose inputs did not change measures nothing
	int measures = 0;
	Bubble bubble;
	const std::array<std::u16string, Bubble::k_Parts> first = {u"Did you know that the well-known villagers pray?", u"",
	                                                           u"Tap to close"};
	const std::array<std::u16string, Bubble::k_Parts> second = {u"Another\nfact, shorter", u"", u"Tap to close"};
	for (int frame = 0; frame < 40; ++frame)
	{
		const auto& parts = frame < 10 || frame >= 30 ? first : second;
		const float maxWidth = frame >= 15 && frame < 20 ? 90.0f : 160.0f;
		const float size = frame >= 20 && frame < 25 ? 12.0f : 10.0f;
		for (size_t p = 0; p < Bubble::k_Parts; ++p)
		{
			const auto width = [size](std::u16string_view t) { return size * static_cast<float>(t.size()); };
			const auto counted = [&measures, &width](std::u16string_view t) {
				++measures;
				return width(t);
			};
			const auto expected = WrapGatheringText(parts.at(p), maxWidth, width);
			measures = 0;
			EXPECT_EQ(bubble.PartWrap(p).Wrap(parts.at(p), maxWidth, size, counted), expected) << frame << " " << p;
			const int afterFirst = measures;
			EXPECT_EQ(bubble.PartWrap(p).Wrap(parts.at(p), maxWidth, size, counted), expected) << frame << " " << p;
			EXPECT_EQ(measures, afterFirst);
			const bool changed = frame == 0 || frame == 10 || frame == 15 || frame == 20 || frame == 25 || frame == 30;
			if (!changed)
			{
				EXPECT_EQ(afterFirst, 0) << frame << " " << p;
			}
		}
	}
}
