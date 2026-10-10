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
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Help/HelpTextDisplay.h"

// HelpText display: region,
// stack, layout and escape codes, with a fixed-width font of 10 px per character (a space is 10 px too).

using namespace openblack::help;

namespace
{
const WidthFn k_Fixed = [](TextFont, std::u16string_view text, float) { return static_cast<float>(text.size()) * 10.0f; };

/// A display for a screen of that height (line height height / 30)
HelpTextDisplay Settled(int height = 1080)
{
	HelpTextDisplay display(height);
	return display;
}

std::vector<std::u16string> Words(const TextFrame& frame)
{
	std::vector<std::u16string> out;
	for (const auto& run : frame.runs)
	{
		out.push_back(run.text);
	}
	return out;
}
} // namespace

TEST(HelpTextDisplay, Region)
{
	const TextRegion full = ComputeTextRegion(1920, 1080, 0);
	EXPECT_EQ(full.left, 0);
	EXPECT_EQ(full.top, 916);
	EXPECT_EQ(full.right, 1920);
	EXPECT_EQ(full.bottom, 1052);

	const TextRegion bars = ComputeTextRegion(640, 480, 60);
	EXPECT_EQ(bars.left, 0);
	EXPECT_EQ(bars.top, 350);
	EXPECT_EQ(bars.right, 640);
	EXPECT_EQ(bars.bottom, 410);

	// the click cue: (boxH + 1) / 3 = 137 / 3 = 45; y = 1052 - 22 + 1
	EXPECT_EQ(ClickCueHeight(full), 45);
	EXPECT_EQ(ClickCueY(full), 1031);
}

TEST(HelpTextDisplay, LineHeight)
{
	EXPECT_FLOAT_EQ(HelpTextDisplay(1080).GetBaseLineHeight(), 36.0f);
	EXPECT_FLOAT_EQ(HelpTextDisplay(480).GetBaseLineHeight(), 16.0f);
	EXPECT_NEAR(HelpTextDisplay(1080, true).GetBaseLineHeight(), 1080.0f / 28.0f, 1e-3);
}

TEST(HelpTextDisplay, SingleTextCentred)
{
	HelpTextDisplay display = Settled();
	display.Add(u"Hello world", 0.0f, 1, true);
	display.Advance(1000.0f);
	EXPECT_FLOAT_EQ(display.GetAnimation(), 1.0f);
	const TextFrame frame = display.Layout(1920, 1080, 0, 1, false, k_Fixed);
	EXPECT_TRUE(frame.boxShown);
	EXPECT_EQ(frame.boxAlpha, 0x80);
	EXPECT_EQ(frame.box.top, 916);
	ASSERT_EQ(frame.runs.size(), 2U);
	// widest 110 -> x0 = trunc((1921 - 110) * 0.5) = 905; y = top + trunc(36 / 3)
	EXPECT_FLOAT_EQ(frame.runs[0].x, 905.0f);
	EXPECT_FLOAT_EQ(frame.runs[0].y, 928.0f);
	EXPECT_FLOAT_EQ(frame.runs[1].x, 965.0f);
	EXPECT_FLOAT_EQ(frame.runs[1].y, 928.0f);
	EXPECT_FLOAT_EQ(frame.runs[0].size, 36.0f);
	EXPECT_EQ(frame.runs[0].a, 255);
	EXPECT_EQ(frame.runs[0].font, TextFont::J0);
	EXPECT_FLOAT_EQ(frame.runs[0].clipTop, 916.0f);
	EXPECT_FLOAT_EQ(frame.runs[0].clipBottom, 1052.0f);
}

TEST(HelpTextDisplay, GreedyWrapAndMaxLines)
{
	HelpTextDisplay display = Settled();
	// 16 words of 40 px in a box 201 px wide: 4 per line, 3 lines max -> 12 words drawn
	std::u16string text;
	for (int i = 0; i < 16; ++i)
	{
		text += (i == 0 ? u"" : u" ");
		text += std::u16string(4, static_cast<char16_t>(u'a' + i));
	}
	display.Add(text, 0.0f, 1, true);
	display.Advance(1000.0f);
	const TextFrame frame = display.Layout(200, 1080, 0, 2, false, k_Fixed);
	ASSERT_EQ(frame.runs.size(), 12U);
	// the widest line keeps its trailing space (200): x0 = trunc(1 * 0.5) = 0
	EXPECT_FLOAT_EQ(frame.runs[0].x, 0.0f);
	EXPECT_FLOAT_EQ(frame.runs[1].x, 50.0f);
	EXPECT_FLOAT_EQ(frame.runs[3].x, 150.0f);
	EXPECT_FLOAT_EQ(frame.runs[4].x, 0.0f);
	EXPECT_FLOAT_EQ(frame.runs[0].y, 928.0f);
	EXPECT_FLOAT_EQ(frame.runs[4].y, 964.0f);
	EXPECT_FLOAT_EQ(frame.runs[8].y, 1000.0f);
	EXPECT_EQ(frame.runs[11].text, u"llll");
}

TEST(HelpTextDisplay, StackScalesAndAlpha)
{
	HelpTextDisplay display = Settled();
	display.Add(u"A", 0.0f, 1, true);
	display.Add(u"B", 0.0f, 1, true);
	display.Add(u"C", 0.0f, 1, true);
	display.Advance(1000.0f);
	const TextFrame frame = display.Layout(1920, 1080, 0, 2, false, k_Fixed);
	ASSERT_EQ(frame.runs.size(), 3U);
	EXPECT_EQ(frame.runs[0].text, u"C");
	EXPECT_FLOAT_EQ(frame.runs[0].size, 36.0f);
	EXPECT_EQ(frame.runs[0].a, 255);
	EXPECT_FLOAT_EQ(frame.runs[0].y, 928.0f);

	EXPECT_EQ(frame.runs[1].text, u"B");
	EXPECT_NEAR(frame.runs[1].size, 0.86f * 36.0f, 1e-4);
	EXPECT_EQ(frame.runs[1].a, static_cast<int>(0.86f * 255.0f) - 20); // 199
	EXPECT_EQ(frame.runs[1].a, 199);
	EXPECT_FLOAT_EQ(frame.runs[1].y, 916.0f + 48.0f); // 12 + 36

	EXPECT_EQ(frame.runs[2].text, u"A");
	EXPECT_NEAR(frame.runs[2].size, 0.86f * 0.93f * 36.0f, 1e-4);
	EXPECT_EQ(frame.runs[2].a, 183);                  // trunc(0.7998 * 255) - 20
	EXPECT_FLOAT_EQ(frame.runs[2].y, 916.0f + 78.0f); // trunc(48 + 30.96)
}

TEST(HelpTextDisplay, NewestFadesIn)
{
	HelpTextDisplay display = Settled();
	display.Add(u"A", 0.0f, 1, true);
	display.Advance(100.0f); // anim 0.3
	EXPECT_NEAR(display.GetAnimation(), 0.3f, 1e-6);
	const TextFrame frame = display.Layout(1920, 1080, 0, 2, false, k_Fixed);
	ASSERT_EQ(frame.runs.size(), 1U);
	// scale 1.0752688 -> 1 by 0.3: 1.05269; alpha trunc(trunc(268.4) * 0.3) = 80; y from -24 to 12: -13.2
	EXPECT_EQ(frame.runs[0].a, 80);
	EXPECT_NEAR(frame.runs[0].size, 1.0526882f * 36.0f, 1e-3);
	EXPECT_NEAR(frame.runs[0].y, 916.0 - 13.2, 1e-3);
}

TEST(HelpTextDisplay, FourOrSixEntries)
{
	HelpTextDisplay display = Settled();
	for (const char16_t c : std::u16string(u"ABCDEF"))
	{
		display.Add(std::u16string(1, c), 0.0f, 1, true);
	}
	display.Advance(100.0f);
	EXPECT_EQ(display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs.size(), 6U);
	display.Advance(1000.0f);
	const auto words = Words(display.Layout(1920, 1080, 0, 2, false, k_Fixed));
	EXPECT_EQ(words, (std::vector<std::u16string> {u"F", u"E", u"D", u"C"}));
}

TEST(HelpTextDisplay, SingleLineCentred)
{
	HelpTextDisplay display = Settled();
	display.SetSingleLine(true);
	display.Add(u"A", 0.0f, 1, true);
	EXPECT_TRUE(display.IsSingleLine());
	display.Advance(100.0f);
	const TextFrame frame = display.Layout(1920, 1080, 0, 2, false, k_Fixed);
	ASSERT_EQ(frame.runs.size(), 1U);
	// y = trunc(137 / 2 - 36 / 2) = 50, no zoom: size 36, alpha trunc(255 * 0.3)
	EXPECT_FLOAT_EQ(frame.runs[0].y, 966.0f);
	EXPECT_FLOAT_EQ(frame.runs[0].size, 36.0f);
	EXPECT_EQ(frame.runs[0].a, 76);
	// a second text clears singleLine
	display.Add(u"B", 0.0f, 1, true);
	EXPECT_FALSE(display.IsSingleLine());
}

TEST(HelpTextDisplay, TopToBottom)
{
	HelpTextDisplay display = Settled();
	display.Add(u"A", 0.0f, 1, true);
	display.Add(u"B", 0.0f, 1, true);
	display.Advance(1000.0f);
	const TextFrame frame = display.Layout(1920, 1080, 0, 2, true, k_Fixed);
	ASSERT_EQ(frame.runs.size(), 2U);
	// vAlign 2: 916 + (137 - 36) - 12; older: 916 + (137 - 30.96) - 48
	EXPECT_FLOAT_EQ(frame.runs[0].y, 1005.0f);
	EXPECT_NEAR(frame.runs[1].y, 916.0 + 137.0 - 0.86 * 36.0 - 48.0, 1e-3);
}

TEST(HelpTextDisplay, NarratorColourAndFont)
{
	HelpTextDisplay display = Settled();
	display.Add(u"good", 0.0f, 2, true);
	display.Advance(1000.0f);
	auto run = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs.at(0);
	EXPECT_EQ(run.font, TextFont::F1);
	EXPECT_EQ(run.r, 235);
	EXPECT_EQ(run.g, 235);
	EXPECT_EQ(run.b, 183);

	display.Reset(true);
	display.Add(u"evil", 0.0f, 3, true);
	display.Advance(1000.0f);
	run = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs.at(0);
	EXPECT_EQ(run.font, TextFont::F3);
	EXPECT_EQ(run.r, 255);
	EXPECT_EQ(run.g, 180);
	EXPECT_EQ(run.b, 180);

	display.Reset(true);
	display.Add(u"man", 0.0f, 0, true);
	display.Advance(1000.0f);
	run = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs.at(0);
	EXPECT_EQ(run.font, TextFont::J0);
	EXPECT_EQ(run.r, 255);
	EXPECT_EQ(run.g, 255);
	EXPECT_EQ(run.b, 255);

	// f1 / f3 missing -> j0
	display.SetFontsLoaded(false, false);
	display.Reset(true);
	display.Add(u"good", 0.0f, 2, true);
	display.Advance(1000.0f);
	EXPECT_EQ(display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs.at(0).font, TextFont::J0);
}

TEST(HelpTextDisplay, ColourCode)
{
	HelpTextDisplay display = Settled();
	// $C<n> = 0xBBGGRR; $C0 = the narrator default. The skip after the digits eats the space.
	display.Add(u"$C255 x $C0 y", 0.0f, 1, true);
	display.Advance(1000.0f);
	auto runs = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs;
	ASSERT_EQ(runs.size(), 2U);
	EXPECT_EQ(runs[0].text, u"x");
	EXPECT_EQ(runs[0].r, 255);
	EXPECT_EQ(runs[0].g, 0);
	EXPECT_EQ(runs[0].b, 0);
	EXPECT_EQ(runs[1].text, u"y");
	EXPECT_EQ(runs[1].g, 255);

	// the draw starts with the colour the measure ended with (the measure runs once, before the draw)
	display.Reset(true);
	display.Add(u"a $C65280 b", 0.0f, 1, true);
	display.Advance(1000.0f);
	runs = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs;
	ASSERT_EQ(runs.size(), 2U);
	EXPECT_EQ(runs[0].r, 0);
	EXPECT_EQ(runs[0].g, 255);
	EXPECT_EQ(runs[1].g, 255);
}

TEST(HelpTextDisplay, NewLineCode)
{
	HelpTextDisplay display = Settled();
	display.Add(u"ab$Ncd", 0.0f, 1, true);
	display.Advance(1000.0f);
	const auto runs = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs;
	ASSERT_EQ(runs.size(), 2U);
	// widest 20 -> x0 = trunc(1901 * 0.5) = 950
	EXPECT_FLOAT_EQ(runs[0].x, 950.0f);
	EXPECT_FLOAT_EQ(runs[1].x, 950.0f);
	EXPECT_FLOAT_EQ(runs[0].y, 928.0f);
	EXPECT_FLOAT_EQ(runs[1].y, 964.0f);
}

TEST(HelpTextDisplay, NumberCodes)
{
	HelpTextDisplay display = Settled();
	display.Add(u"Got $P done", 50.0f, 1, true);
	display.Advance(1000.0f);
	auto runs = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs;
	ASSERT_EQ(runs.size(), 3U);
	EXPECT_EQ(runs[1].text, u"50.000%");
	// measured 30 + 10 + 70 + 40 = 150 (no space after the number); drawn with one
	EXPECT_FLOAT_EQ(runs[0].x, 885.0f);
	EXPECT_FLOAT_EQ(runs[1].x, 925.0f);
	EXPECT_FLOAT_EQ(runs[2].x, 1005.0f);

	display.Reset(true);
	display.Add(u"$D", 2.5f, 1, true);
	display.Advance(1000.0f);
	runs = display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs;
	ASSERT_EQ(runs.size(), 1U);
	EXPECT_EQ(runs[0].text, u"2.500");
}

TEST(HelpTextDisplay, StopLiteralAndUnknownCodes)
{
	HelpTextDisplay display = Settled();
	display.Add(u"ab $1 cd", 0.0f, 1, true);
	display.Advance(1000.0f);
	EXPECT_EQ(Words(display.Layout(1920, 1080, 0, 2, false, k_Fixed)), (std::vector<std::u16string> {u"ab"}));

	display.Reset(true);
	display.Add(u"a$$b $Ix", 0.0f, 1, true);
	display.Advance(1000.0f);
	EXPECT_EQ(Words(display.Layout(1920, 1080, 0, 2, false, k_Fixed)), (std::vector<std::u16string> {u"a", u"$b", u"Ix"}));
}

TEST(HelpTextDisplay, ResetCloseAndGate)
{
	HelpTextDisplay display = Settled();
	display.Add(u"A", 0.0f, 1, true);
	display.Advance(1000.0f);

	// TEXT_DRAW 0: nothing; 1: the text's arg0
	EXPECT_FALSE(display.Layout(1920, 1080, 0, 0, false, k_Fixed).boxShown);
	EXPECT_TRUE(display.Layout(1920, 1080, 0, 0, false, k_Fixed).runs.empty());
	display.Add(u"B", 0.0f, 1, false);
	EXPECT_FALSE(display.Layout(1920, 1080, 0, 1, false, k_Fixed).boxShown);
	EXPECT_TRUE(display.Layout(1920, 1080, 0, 1, false, k_Fixed).runs.empty());
	display.Advance(1000.0f, 1); // no animation while not drawn
	EXPECT_FLOAT_EQ(display.GetAnimation(), 0.0f);
	EXPECT_EQ(display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs.size(), 2U);

	// Close: box hidden, texts not drawn
	display.Close();
	EXPECT_TRUE(display.IsHidden());
	TextFrame frame = display.Layout(1920, 1080, 0, 2, false, k_Fixed);
	EXPECT_FALSE(frame.boxShown);
	EXPECT_TRUE(frame.runs.empty());

	// Reset(false): flags cleared, ring kept
	display.SetSingleLine(true);
	display.Reset(false);
	EXPECT_FALSE(display.IsHidden());
	EXPECT_FALSE(display.IsSingleLine());
	frame = display.Layout(1920, 1080, 0, 2, false, k_Fixed);
	EXPECT_FALSE(frame.boxShown);
	EXPECT_EQ(frame.runs.size(), 2U);

	// Reset(true): ring emptied
	display.Reset(true);
	EXPECT_TRUE(display.Layout(1920, 1080, 0, 2, false, k_Fixed).runs.empty());
	display.Add(u"C", 0.0f, 1, true);
	EXPECT_TRUE(display.Layout(1920, 1080, 0, 2, false, k_Fixed).boxShown);
}

namespace
{
/// The frame as the bytes the renderer is given: every field, floats by their bits
std::vector<uint8_t> FrameBytes(const TextFrame& frame)
{
	std::vector<uint8_t> out;
	const auto put = [&out](const auto& value) {
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		out.insert(out.end(), bytes, bytes + sizeof(value));
	};
	put(frame.boxShown);
	put(frame.box.left);
	put(frame.box.top);
	put(frame.box.right);
	put(frame.box.bottom);
	put(frame.boxAlpha);
	put(frame.controlIcon);
	put(frame.runs.size());
	for (const auto& run : frame.runs)
	{
		put(run.font);
		put(run.text.size());
		for (const auto c : run.text)
		{
			put(c);
		}
		put(std::bit_cast<uint32_t>(run.x));
		put(std::bit_cast<uint32_t>(run.y));
		put(std::bit_cast<uint32_t>(run.size));
		put(run.r);
		put(run.g);
		put(run.b);
		put(run.a);
		put(std::bit_cast<uint32_t>(run.clipTop));
		put(std::bit_cast<uint32_t>(run.clipBottom));
	}
	return out;
}
} // namespace

TEST(HelpTextDisplay, CachedLayoutMatchesLayout)
{
	// a sequence of frames whose inputs change and come back: the cached frame is byte for byte the frame laid out
	// anew, and a frame whose inputs did not change measures nothing
	int measures = 0;
	const WidthFn counted = [&measures](TextFont font, std::u16string_view text, float size) {
		++measures;
		return k_Fixed(font, text, size);
	};
	HelpTextDisplay display(1080);
	int width = 1920;
	int height = 1080;
	int bar = 0;
	int textDraw = 2;
	bool topToBottom = false;
	for (int frame = 0; frame < 120; ++frame)
	{
		switch (frame)
		{
		case 0:
			display.Add(u"Hello world, a text long enough to be broken into more than one line of the box", 0.0f, 1, true);
			break;
		case 30:
			width = 1280;
			height = 720;
			break;
		case 33:
			width = 1920;
			height = 1080;
			break;
		case 36:
			bar = 60;
			break;
		case 40:
			display.Add(u"good", 0.0f, 2, true);
			break;
		case 70:
			display.SetSingleLine(true);
			break;
		case 72:
			display.Add(u"evil", 0.0f, 3, true);
			break;
		case 95:
			display.Close();
			break;
		case 98:
			display.Reset(false);
			break;
		case 101:
			textDraw = 1;
			break;
		case 103:
			textDraw = 0;
			break;
		case 105:
			textDraw = 2;
			topToBottom = true;
			break;
		case 108:
			topToBottom = false;
			break;
		case 110:
			display.Reset(true);
			break;
		case 112:
			display.Add(u"Hello world, a text long enough to be broken into more than one line of the box", 0.0f, 1, true);
			break;
		default:
			break;
		}
		display.Advance(16.0f, textDraw);
		const auto expected = FrameBytes(display.Layout(width, height, bar, textDraw, topToBottom, k_Fixed));
		measures = 0;
		const auto cached = FrameBytes(display.CachedLayout(width, height, bar, textDraw, topToBottom, counted));
		EXPECT_EQ(cached, expected) << "frame " << frame;
		// again with the same inputs: the same frame, nothing measured
		const int afterFirst = measures;
		EXPECT_EQ(FrameBytes(display.CachedLayout(width, height, bar, textDraw, topToBottom, counted)), expected)
		    << "frame " << frame;
		EXPECT_EQ(measures, afterFirst) << "frame " << frame;
		// once the newest text's slide-in is done (anim 1, 21 frames of 16 ms), the frames laid out are the same
		if (frame == 25 || frame == 65)
		{
			EXPECT_EQ(afterFirst, 0) << "frame " << frame;
		}
	}
}
