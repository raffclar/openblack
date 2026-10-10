/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "Gui/LoadingScreenRules.h"
#include "Gui/StartupRules.h"

using namespace openblack::loading;
using namespace openblack::startup;

TEST(LoadingScreen, TipsArePickedByTheSixteenthsOfAMillisecondCount)
{
	// Without a profile the first tip, which explains what the tips are
	EXPECT_EQ(PickTip(123456, false), 0);
	// 16 ms a step through the 34, from tip 1
	EXPECT_EQ(PickTip(0, true), 1);
	EXPECT_EQ(PickTip(15, true), 1);
	EXPECT_EQ(PickTip(16, true), 2);
	EXPECT_EQ(PickTip(33 * 16, true), 34);
	EXPECT_EQ(PickTip(34 * 16, true), 1);
	// 29 and 32 are never shown: the next tip is
	EXPECT_EQ(PickTip(28 * 16, true), 30);
	EXPECT_EQ(PickTip(29 * 16, true), 30);
	EXPECT_EQ(PickTip(31 * 16, true), 33);
	std::set<int32_t> seen;
	for (uint32_t t = 0; t < 34 * 16; t += 16)
	{
		seen.insert(PickTip(t, true));
	}
	EXPECT_EQ(seen.size(), 32u);
	EXPECT_FALSE(seen.contains(29));
	EXPECT_FALSE(seen.contains(32));
	EXPECT_FALSE(seen.contains(0));
	// The count wraps as the computer's millisecond count does
	EXPECT_EQ(PickTip(0xFFFFFFFFu, true), static_cast<int32_t>((0xFFFFFFFFu >> 4) % 34) + 1);
}

TEST(LoadingScreen, TipTextsAreTheTipsOfTheDay)
{
	EXPECT_EQ(TipTextName(0), "HELP_TEXT_TOTD_01");
	EXPECT_EQ(TipTextName(9), "HELP_TEXT_TOTD_10");
	EXPECT_EQ(TipTextName(34), "HELP_TEXT_TOTD_35");
}

TEST(LoadingScreen, ControlCodesAreBlanked)
{
	EXPECT_EQ(BlankControlCodes(u"Press the key.$m17"), u"Press the key.    ");
	EXPECT_EQ(BlankControlCodes(u"A $code and \\more here"), u"A       and       here");
	// The run stops at the hidden-word mark, which stays
	EXPECT_EQ(BlankControlCodes(u"x$ab\xF8FEy"), u"x   \xF8FEy");
	EXPECT_EQ(BlankControlCodes(u"No codes"), u"No codes");
	EXPECT_EQ(BlankControlCodes(u"$"), u" ");
}

TEST(LoadingScreen, Layout)
{
	const auto layout = LayoutFor(1024, 768);
	EXPECT_EQ(layout.picture, (Rect {.left = 184, .top = 38, .right = 184 + 655, .bottom = 38 + 491}));
	// The band's top comes from the screen's width
	EXPECT_EQ(layout.band, (Rect {.left = 0, .top = 614, .right = 1024, .bottom = 729}));
	EXPECT_EQ(layout.text, (Rect {.left = 128, .top = 614, .right = 896, .bottom = 729}));
	EXPECT_EQ(layout.textSize, 49);
	EXPECT_FLOAT_EQ(layout.versionSize, 20.48f);
	// On a wide screen the band starts below the bottom and nothing of it is seen
	const auto wide = LayoutFor(1920, 1080);
	EXPECT_GT(wide.band.top, wide.band.bottom);
}

TEST(LoadingScreen, FadeBringsInTheBlurThenTheSharpPicture)
{
	EXPECT_EQ(FadeOf(0.0f), (Fade {.alpha = 0, .sharp = 0.0f, .sharpAlpha = 0}));
	EXPECT_EQ(FadeOf(0.25f).alpha, 127);
	EXPECT_EQ(FadeOf(0.25f).sharpAlpha, 0);
	EXPECT_EQ(FadeOf(0.5f).alpha, 255);
	EXPECT_EQ(FadeOf(0.75f).sharpAlpha, 127);
	EXPECT_EQ(FadeOf(1.0f), (Fade {.alpha = 255, .sharp = 1.0f, .sharpAlpha = 255}));
	// A coloured box at full strength loses a step of alpha
	EXPECT_EQ(BoxAlpha(0xFF, 255), 254);
	EXPECT_EQ(BoxAlpha(0xA0, 255), 159);
	EXPECT_EQ(BoxAlpha(0xA0, 0), 0);
}

TEST(LoadingScreen, LayoutSpaceIsCentredOnBigScreensAndScaledOnSmallOnes)
{
	EXPECT_EQ(Unadjust({112, 84}, 1024, 768), (std::array {0, 0}));
	EXPECT_EQ(Adjust({0, 0}, 1024, 768), (std::array {112, 84}));
	EXPECT_EQ(UnadjustSize(20, 1024, 768), 20);
	// 640 by 480: the scale is 650 / 480, each step rounded towards zero
	const auto layout = Unadjust({320, 240}, 640, 480);
	EXPECT_EQ(layout[0], 399);
	EXPECT_EQ(layout[1], 299);
	EXPECT_EQ(UnadjustSize(12, 640, 480), 16);
}

TEST(LoadingScreen, BarSweeps)
{
	const auto layout = LayoutFor(1024, 768);
	const auto bar = BarFor(layout, 0.0f, 1024, 768);
	// The picture's frame, in the layout's space
	EXPECT_EQ(bar.outline, (Rect {.left = 71, .top = -47, .right = 728, .bottom = 446}));
	EXPECT_EQ(bar.border, 20);
	EXPECT_EQ(bar.body, (Rect {.left = 71, .top = 476, .right = 728, .bottom = 496}));
	// At the start both sweeps sit at the bar's left
	EXPECT_EQ(bar.brightStart, 74);
	EXPECT_EQ(bar.brightEnd, 131);
	EXPECT_EQ(bar.darkStart, 74);
	EXPECT_EQ(bar.darkEnd, 74);
	EXPECT_EQ(bar.sweepTop, 479);
	EXPECT_EQ(bar.brightBottom, 492);
	EXPECT_EQ(bar.darkBottom, 493);
	EXPECT_EQ(bar.topShadow, (Rect {.left = 74, .top = 479, .right = 724, .bottom = 485}));
	EXPECT_EQ(bar.leftShadow, (Rect {.left = 74, .top = 479, .right = 80, .bottom = 492}));
	// Half way, the bright sweep has crossed and the dark one is starting
	const auto half = BarFor(layout, 0.5f, 1024, 768);
	EXPECT_EQ(half.brightStart, 725);
	EXPECT_EQ(half.darkStart, 74);
	// A whole period later it is as at the start
	EXPECT_EQ(BarFor(layout, 1.0f, 1024, 768), bar);
	EXPECT_EQ(BarFor(layout, 2.25f, 1024, 768), BarFor(layout, 0.25f, 1024, 768));
}

TEST(LoadingScreen, TextShrinksUntilItFits)
{
	const auto layout = LayoutFor(1024, 768); // a band 115 high, text from 49
	// Three lines at any size: 37 is the first under 115
	EXPECT_EQ(FitTextSize(layout, [](int32_t size) { return 3.0f * static_cast<float>(size); }), 37);
	EXPECT_EQ(FitTextSize(layout, [](int32_t) { return 10.0f; }), 49);
	// Never under 8
	EXPECT_EQ(FitTextSize(layout, [](int32_t) { return 1000.0f; }), 7);
}

TEST(LoadingScreen, Version)
{
	EXPECT_EQ(VersionText(100, 0), u"V1.00");
	EXPECT_EQ(VersionText(142, 0), u"V1.42");
	EXPECT_EQ(VersionText(105, 3), u"V1.05 Beta 3");
}

TEST(LoadingScreen, ClockRedrawsEveryOneHundredAndFiftyMilliseconds)
{
	LoadingClock clock;
	EXPECT_FALSE(clock.Tick(1000, LoadingClock::Mode::Tips));
	EXPECT_EQ(clock.Time(), 1);
	EXPECT_FALSE(clock.Tick(1149, LoadingClock::Mode::Tips));
	EXPECT_TRUE(clock.Tick(1150, LoadingClock::Mode::Tips));
	EXPECT_EQ(clock.Time(), 151);
	// A long stall counts as 350 ms on the tips screen
	EXPECT_TRUE(clock.Tick(3150, LoadingClock::Mode::Tips));
	EXPECT_EQ(clock.Time(), 501);
	// but in full for the banner
	EXPECT_TRUE(clock.Tick(5150, LoadingClock::Mode::PleaseWait));
	EXPECT_EQ(clock.Time(), 2501);
	EXPECT_FLOAT_EQ(clock.Progress(), 2501.0f / 15000.0f);
	clock.Reset();
	EXPECT_EQ(clock.Time(), 0);
	EXPECT_FALSE(clock.Tick(9000, LoadingClock::Mode::Tips));
	// The millisecond count wrapping round
	LoadingClock wrapping;
	EXPECT_FALSE(wrapping.Tick(0xFFFFFFF0u, LoadingClock::Mode::Tips));
	EXPECT_TRUE(wrapping.Tick(0x100u, LoadingClock::Mode::Tips));
	EXPECT_EQ(wrapping.Time(), 1 + 0x110);
}

TEST(LoadingScreen, PleaseWait)
{
	EXPECT_EQ(PleaseWaitAlpha(5000), 0);
	EXPECT_EQ(PleaseWaitAlpha(5002), 1);
	EXPECT_EQ(PleaseWaitAlpha(5510), 255);
	EXPECT_EQ(PleaseWaitAlpha(60000), 255);
}

TEST(LoadingScreen, SixteenBitCopy)
{
	const std::vector<uint8_t> bgrx = {0xFF, 0x80, 0x07, 0, 0x08, 0x04, 0xFF, 0};
	const auto rgb555 = ToSixteenBit(bgrx, 2, 1, Rgb16::Rgb555);
	ASSERT_EQ(rgb555.size(), 2u * 17u);
	EXPECT_EQ(rgb555[0], (0u << 10) | (16u << 5) | 31u);
	EXPECT_EQ(rgb555[1], (31u << 10) | (0u << 5) | 1u);
	EXPECT_EQ(rgb555[2], 0);
	const auto rgb565 = ToSixteenBit(bgrx, 2, 1, Rgb16::Rgb565);
	EXPECT_EQ(rgb565[0], (0u << 11) | (32u << 5) | 31u);
	EXPECT_EQ(ToRgba8(0x7FFF, Rgb16::Rgb555), (std::array<uint8_t, 4> {255, 255, 255, 255}));
	EXPECT_EQ(ToRgba8((16u << 10) | (1u << 5), Rgb16::Rgb555), (std::array<uint8_t, 4> {132, 8, 0, 255}));
	EXPECT_EQ(ToRgba8(0x07E0, Rgb16::Rgb565), (std::array<uint8_t, 4> {0, 255, 0, 255}));
}

TEST(LoadingScreen, BlurAveragesOverlappingBlocks)
{
	// 32 by 16: the left half 31 red, the right half 0; 16 rows of nothing follow
	constexpr uint32_t k_Width = 32;
	constexpr uint32_t k_Height = 16;
	std::vector<uint16_t> pixels(k_Width * (k_Height + 16), 0);
	for (uint32_t y = 0; y < k_Height; ++y)
	{
		for (uint32_t x = 0; x < 16; ++x)
		{
			pixels[y * k_Width + x] = 31u << 10;
		}
	}
	const auto blur = BlurPicture(pixels, k_Width, k_Height, Rgb16::Rgb555);
	ASSERT_EQ(blur.width, 4u);
	ASSERT_EQ(blur.height, 2u);
	ASSERT_EQ(blur.texels.size(), 256u * 256u);
	const auto red = [&blur](uint32_t x, uint32_t y) { return (blur.texels[y * 256 + x] >> 10) & 0x1F; };
	// The first block is all red; the second half red; the third none
	EXPECT_EQ(red(0, 0), 31u);
	EXPECT_EQ(red(1, 0), 15u); // 31 * 128 / 256, rounded down
	EXPECT_EQ(red(2, 0), 0u);
	// The last block runs on into the next row's first eight pixels, which are red
	EXPECT_EQ(red(3, 0), 31u * 8 * 15 / 256);
	// The second row of blocks reads half into the rows after the picture, which are empty
	EXPECT_EQ(red(0, 1), 15u);
	// The opaque bit is set
	EXPECT_EQ(blur.texels[0] & 0x8000, 0x8000);
	// A picture whose eighth is wider than the texture has no blur
	EXPECT_TRUE(BlurPicture(pixels, 8 * 257, 8, Rgb16::Rgb555).texels.empty());
}

TEST(Startup, ZoomerEasesThereAndStops)
{
	Zoomer zoomer;
	zoomer.SetDestination(1.0f, 0.0f, 0.7f);
	EXPECT_FALSE(zoomer.Arrived());
	zoomer.Advance(350);
	// Half way through the time: 6u^2 - 8u^3 + 3u^4 at a half is 11/16
	EXPECT_NEAR(zoomer.Position(), 11.0f / 16.0f, 1e-5f);
	zoomer.Advance(349);
	EXPECT_NEAR(zoomer.Position(), 1.0f, 1e-4f);
	EXPECT_FALSE(zoomer.Arrived());
	zoomer.Advance(10);
	EXPECT_TRUE(zoomer.Arrived());
	EXPECT_EQ(zoomer.Position(), 1.0f);
	EXPECT_EQ(zoomer.Speed(), 0.0f);
	// Under a millisecond it jumps
	zoomer.SetDestination(5.0f, 0.0f, 0.0005f);
	EXPECT_EQ(zoomer.Position(), 5.0f);
	EXPECT_TRUE(zoomer.Arrived());
}

TEST(Startup, LogoStillFadesInHoldsAndFadesOut)
{
	LogoStill still;
	// The first frame from no time at all is clear
	EXPECT_EQ(still.Frame(0, false), 0);
	int32_t frames = 0;
	uint8_t alpha = 0;
	while (!still.Ended() && frames < 1000)
	{
		alpha = still.Frame(10, false);
		++frames;
	}
	// 0.7 s in, 1.7 s held and 1.1 s out, ten milliseconds a frame, give or take the sums' rounding
	EXPECT_NEAR(frames, 70 + 170 + 110, 3);
	EXPECT_EQ(alpha, 0);

	// Opaque while it holds, and through the start of its fade out
	LogoStill held;
	for (int i = 0; i < 70 + 100; ++i)
	{
		held.Frame(10, false);
	}
	EXPECT_EQ(held.Frame(10, false), 255);
	for (int i = 0; i < 69 + 30; ++i)
	{
		held.Frame(10, false);
	}
	EXPECT_EQ(held.Frame(10, false), 255);
}

TEST(Startup, PressingSkipsALogoStill)
{
	LogoStill still;
	still.Frame(100, false);
	// Pressed while fading in: this frame still shows where it was, then it fades out from opaque
	const uint8_t before = still.Frame(100, true);
	EXPECT_GT(before, 0);
	EXPECT_LT(before, 255);
	EXPECT_EQ(still.Frame(0, false), 255);
	EXPECT_FALSE(still.Ended());
	// Pressed again while fading out: it ends after this frame
	still.Frame(100, true);
	EXPECT_TRUE(still.Ended());
}

TEST(Startup, PreIntro)
{
	EXPECT_TRUE(PlaysPreIntro(true, true));
	EXPECT_TRUE(PlaysPreIntro(false, false));
	EXPECT_FALSE(PlaysPreIntro(false, true));
	EXPECT_TRUE(PreIntroGoesOn(1, 2515, false));
	EXPECT_TRUE(PreIntroGoesOn(2514, 2515, false));
	EXPECT_FALSE(PreIntroGoesOn(2515, 2515, false));
	EXPECT_FALSE(PreIntroGoesOn(10, 2515, true));
	EXPECT_FLOAT_EQ(NextTipFade(0.0f, 16), 0.016f);
}

TEST(LoadingScreen, PleaseWaitBannerAcrossTheMiddle)
{
	const auto banner = PleaseWaitBannerFor(1024, 768);
	EXPECT_EQ(banner.band, (Rect {.left = -1, .top = 354, .right = 1024, .bottom = 414}));
	EXPECT_EQ(banner.edge, 409);
	EXPECT_EQ(banner.shadowAbove, (Rect {.left = -1, .top = 339, .right = 1024, .bottom = 354}));
	EXPECT_EQ(banner.shadowBelow, (Rect {.left = -1, .top = 414, .right = 1024, .bottom = 429}));
}

TEST(LoadingScreen, LayoutTextBoxes)
{
	// A pixel a unit on a big screen, the first line at the box's bottom until it is centred
	EXPECT_EQ(TextBoxFor(k_PleaseWaitText, 400, k_BigTextSize, 1024, 768),
	          (TextBox {.box = {.left = 212, .top = 284, .right = 812, .bottom = 484}, .start = 484, .size = 35.0f}));
	// Scaled down on a small one
	const auto small = TextBoxFor(k_PleaseWaitText, 400, k_BigTextSize, 640, 480);
	EXPECT_EQ(small.box, (Rect {.left = 98, .top = 166, .right = 98 + 443, .bottom = 166 + 147}));
	EXPECT_EQ(small.start, 166 + 147);
	EXPECT_NEAR(small.size, 35.0f * 480.0f / 650.0f, 1e-4f);
}
