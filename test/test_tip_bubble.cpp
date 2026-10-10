/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "Help/TipBubble.h"

using namespace openblack::help::tip_bubble;

namespace
{
/// Every text one line per character, each character 10 pixels wide
std::vector<std::u16string> OneLinePerCharacter(std::u16string_view text, float /*width*/, float /*size*/)
{
	std::vector<std::u16string> lines;
	for (const auto c : text)
	{
		lines.emplace_back(1, c);
	}
	return lines;
}
float TenPixelsEach(std::u16string_view text, float /*size*/)
{
	return 10.0f * static_cast<float>(text.size());
}
} // namespace

TEST(TipBubble, SizesShrinkWithDepth)
{
	const auto near = SizesAt(2.0f);
	EXPECT_FLOAT_EQ(near.width, 400.0f);
	EXPECT_FLOAT_EQ(near.height, 250.0f);
	EXPECT_FLOAT_EQ(near.margin, 40.0f);
	EXPECT_FLOAT_EQ(near.lineHeight, 19.0f);
	const auto far = SizesAt(1000.0f);
	EXPECT_NEAR(far.width, 28.571f, 1e-3f);
	EXPECT_NEAR(far.height, 17.857f, 1e-3f);
	EXPECT_NEAR(far.margin, 5.714f, 1e-3f);
	EXPECT_NEAR(far.lineHeight, 2.714f, 1e-3f);
	// Half way between the near and far depths, half way between the sizes
	const auto middle = SizesAt((k_NearDepth + k_FarDepth) * 0.5f);
	EXPECT_NEAR(middle.width, (400.0f + far.width) * 0.5f, 1e-3f);
}

TEST(TipBubble, FadesInCloseAndOutFarAway)
{
	EXPECT_FLOAT_EQ(DistanceAlpha(3.0f), 0.0f);
	EXPECT_FLOAT_EQ(DistanceAlpha(5.5f), 0.5f);
	EXPECT_FLOAT_EQ(DistanceAlpha(100.0f), 1.0f);
	EXPECT_FLOAT_EQ(DistanceAlpha(375.0f), 0.5f);
	EXPECT_FLOAT_EQ(DistanceAlpha(450.0f), 0.0f);
}

TEST(TipBubble, NotDrawnTooClose)
{
	EXPECT_FALSE(Place({100.0f, 100.0f}, 0.5f, 640.0f).has_value());
	EXPECT_TRUE(Place({100.0f, 100.0f}, 1.0f, 640.0f).has_value());
}

TEST(TipBubble, StandsAMarginAboveTheSignTailTowardsTheMiddle)
{
	// In the middle of the screen: the tail ten pixels right, a fifth of the box left of it
	const auto middle = Place({320.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(middle.has_value());
	EXPECT_FLOAT_EQ(middle->tailX, 330.0f);
	EXPECT_FLOAT_EQ(middle->min.x, 330.0f - 80.0f);
	EXPECT_FLOAT_EQ(middle->max.y, 400.0f - 40.0f);
	EXPECT_FLOAT_EQ(middle->min.y, 400.0f - 40.0f - 250.0f);
	EXPECT_FALSE(middle->tailFlipped);
	// At the right edge: the tail ten pixels left, flipped, eight tenths of the box left of it
	const auto right = Place({640.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(right.has_value());
	EXPECT_FLOAT_EQ(right->tailX, 630.0f);
	EXPECT_FLOAT_EQ(right->min.x, 630.0f - 320.0f);
	EXPECT_TRUE(right->tailFlipped);
	// At the left edge it still reaches left of the tail, off the screen: nothing keeps it on
	const auto left = Place({0.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(left.has_value());
	EXPECT_FLOAT_EQ(left->tailX, 10.0f);
	EXPECT_FLOAT_EQ(left->min.x, 10.0f - 320.0f);
}

TEST(TipBubble, TheBoxIsNinePiecesAndATailSeeThroughAtTheTop)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(placement.has_value());
	const auto quads = BoxShape(*placement, 1.0f);
	ASSERT_EQ(quads.size(), 10u);
	// The top left corner: from the box's corner a radius in, the texture's first eighth, clear at the top
	const auto& corner = quads.front();
	EXPECT_EQ(corner[0].position, glm::vec2(250.0f, 110.0f));
	EXPECT_EQ(corner[2].position, glm::vec2(290.0f, 150.0f));
	EXPECT_FLOAT_EQ(corner[0].uv.y, 0.075f);
	EXPECT_FLOAT_EQ(corner[2].uv.x, 0.125f);
	EXPECT_FLOAT_EQ(corner[0].alpha, 0.0f);
	EXPECT_FLOAT_EQ(corner[3].alpha, 40.0f / 250.0f);
	// The bottom row is whole
	EXPECT_FLOAT_EQ(quads.at(8)[2].alpha, 1.0f);
	// The tail: a radius square at its point, a little over the box's bottom, the texture's second quarter
	const auto& tail = quads.back();
	EXPECT_EQ(tail[0].position, glm::vec2(std::trunc(330.0f - 6.0f), std::trunc(360.0f - 3.64f)));
	EXPECT_FLOAT_EQ(tail[0].uv.x, 0.25f);
	EXPECT_FLOAT_EQ(tail[1].uv.x, 0.5f);
	EXPECT_FLOAT_EQ(tail[0].alpha, 1.0f);
}

TEST(TipBubble, TheFlippedTailMirrorsAndLeansTheOtherWay)
{
	const auto placement = Place({600.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(placement.has_value());
	const auto quads = BoxShape(*placement, 1.0f);
	const auto& tail = quads.back();
	EXPECT_FLOAT_EQ(tail[0].uv.x, 0.5f);
	EXPECT_FLOAT_EQ(tail[1].uv.x, 0.25f);
	EXPECT_FLOAT_EQ(tail[0].position.x, std::trunc(590.0f - 34.0f));
}

TEST(TipBubble, TheTailKeepsAwayFromTheBoxsEnds)
{
	Placement placement;
	placement.min = {0.0f, 0.0f};
	placement.max = {400.0f, 250.0f};
	placement.sizes = k_NearSizes;
	placement.tailX = 5.0f;
	const auto quads = BoxShape(placement, 1.0f);
	const auto& tail = quads.back();
	EXPECT_FLOAT_EQ(tail[0].position.x, std::trunc(72.0f - 6.0f));
}

TEST(TipBubble, WordsStackFromTheBottomUp)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(placement.has_value());
	// The tip, a blank line and the title: one line each
	const std::array<std::u16string_view, 3> items {u"T", u" ", u"D"};
	const auto words = LayOutWords(*placement, items, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach);
	ASSERT_EQ(words.lines.size(), 3u);
	EXPECT_FLOAT_EQ(words.contentHeight, 57.0f);
	// The bottom line is a third of the margin above the box's bottom
	const float bottomLine = 360.0f - (40.0f / 3.0f);
	EXPECT_EQ(words.lines.at(0).text, u"T");
	EXPECT_FLOAT_EQ(words.lines.at(0).at.y, bottomLine - 19.0f);
	EXPECT_EQ(words.lines.at(2).text, u"D");
	EXPECT_FLOAT_EQ(words.lines.at(2).at.y, bottomLine - 57.0f);
	// Centred between the margins
	const float left = 250.0f + (40.0f / 3.0f);
	const float right = 650.0f - (40.0f / 3.0f);
	EXPECT_FLOAT_EQ(words.lines.at(0).at.x, left + ((right - left - 10.0f) * 0.5f));
	// The shadows two pixels down and right at half the alpha
	ASSERT_EQ(words.shadows.size(), 3u);
	EXPECT_FLOAT_EQ(words.shadows.at(0).at.y, words.lines.at(0).at.y + 2.0f);
	EXPECT_FLOAT_EQ(words.shadows.at(0).topAlpha, 128.0f / 255.0f);
	EXPECT_FLOAT_EQ(words.lines.at(0).topAlpha, 1.0f);
}

TEST(TipBubble, WordsFadeInOverTheTopThird)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(placement.has_value());
	// Enough lines to reach the top: the stack stops at the item that gets there
	const std::u16string tall(20, u'x');
	const std::array<std::u16string_view, 3> items {tall, u" ", u"D"};
	const auto words = LayOutWords(*placement, items, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach);
	EXPECT_FLOAT_EQ(words.contentHeight, 20.0f * 19.0f);
	const float top = 110.0f;
	const float fadeEnd = top + (250.0f / 3.0f);
	for (const auto& line : words.lines)
	{
		EXPECT_GE(line.at.y + line.size, top);
		if (line.at.y < fadeEnd && line.at.y > top)
		{
			// In whole steps of the colour's 255
			EXPECT_FLOAT_EQ(line.topAlpha, std::trunc((line.at.y - top) / (fadeEnd - top) * 255.0f) / 255.0f);
		}
	}
}

TEST(TipBubble, TheScrollHoldsToTheWords)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f);
	ASSERT_TRUE(placement.has_value());
	Scroll scroll;
	// Before the words were laid out nothing holds it
	ClampScroll(scroll, *placement);
	EXPECT_FLOAT_EQ(scroll.lines, k_StartScroll);
	// 380 pixels of words in a box of 250 less two margins: 210 over, eleven and a bit lines
	scroll.contentHeight = 380.0f;
	scroll.lineHeight = 19.0f;
	ClampScroll(scroll, *placement);
	EXPECT_FLOAT_EQ(scroll.lines, 210.0f / 19.0f);
	EXPECT_TRUE(scroll.moreBelow);
	EXPECT_FALSE(scroll.moreAbove);
	scroll.lines = 5.0f;
	ClampScroll(scroll, *placement);
	EXPECT_TRUE(scroll.moreAbove);
	EXPECT_TRUE(scroll.moreBelow);
	// Short words: no scroll
	scroll.contentHeight = 57.0f;
	ClampScroll(scroll, *placement);
	EXPECT_FLOAT_EQ(scroll.lines, 0.0f);
	EXPECT_FALSE(scroll.moreAbove);
	EXPECT_FALSE(scroll.moreBelow);
}

TEST(TipBubble, TheArrowsBlinkAtTheRightSide)
{
	EXPECT_TRUE(ArrowsLit(0));
	EXPECT_TRUE(ArrowsLit(399));
	EXPECT_FALSE(ArrowsLit(400));
	EXPECT_TRUE(ArrowsLit(750));
	Placement placement;
	placement.min = {0.0f, 0.0f};
	placement.max = {400.0f, 90.0f};
	const auto up = UpArrow(placement);
	// A fifth of the height is 18, grown by 2 each side
	EXPECT_FLOAT_EQ(up.min.x, 400.0f - 9.9f - 2.0f);
	EXPECT_FLOAT_EQ(up.max.x, 400.0f - 0.9f + 2.0f);
	EXPECT_FLOAT_EQ(up.min.y, 9.0f - 2.0f);
	EXPECT_FLOAT_EQ(up.max.y, 18.0f + 2.0f);
	const auto down = DownArrow(placement);
	EXPECT_FLOAT_EQ(down.min.y, 72.0f - 2.0f);
	EXPECT_FLOAT_EQ(down.max.y, 81.0f + 2.0f);
}

TEST(TipBubble, OnScreenByTheSignsBall)
{
	View view;
	view.eye = {0.0f, 0.0f, 0.0f};
	view.nearClip = 1.0f;
	view.pixelsPerUnit = 320.0f;
	view.screen = {640.0f, 480.0f};
	// Looking down +z: a point's pixel straight from x and y over the depth
	view.project = [](const glm::vec3& p) {
		return glm::vec3(320.0f + (p.x / p.z * 320.0f), 240.0f - (p.y / p.z * 320.0f), p.z);
	};
	EXPECT_TRUE(OnScreen({.centre = {0.0f, 0.0f, 10.0f}, .radius = 1.0f, .origin = {0.0f, 0.0f, 10.0f}}, view));
	// Off to the side, but its circle still reaches the screen's edge
	EXPECT_TRUE(OnScreen({.centre = {10.5f, 0.0f, 10.0f}, .radius = 1.0f, .origin = {10.5f, 0.0f, 10.0f}}, view));
	EXPECT_FALSE(OnScreen({.centre = {12.0f, 0.0f, 10.0f}, .radius = 1.0f, .origin = {12.0f, 0.0f, 10.0f}}, view));
	// Wholly nearer than the near plane
	EXPECT_FALSE(OnScreen({.centre = {0.0f, 0.0f, -5.0f}, .radius = 1.0f, .origin = {0.0f, 0.0f, -5.0f}}, view));
	// The camera within its reach of the model's origin
	EXPECT_TRUE(OnScreen({.centre = {0.0f, 0.0f, -0.5f}, .radius = 2.0f, .origin = {0.0f, 0.0f, 1.0f}}, view));
}

namespace
{
/// Ten pixels a character at any size, the blank that adds no space free
float Measure(std::u16string_view text, float /*size*/)
{
	float width = 0.0f;
	for (const auto c : text)
	{
		width += c == 0xF8FE ? 0.0f : 10.0f;
	}
	return width;
}
} // namespace

TEST(TipBubble, WrapBreaksAtTheLastBlankThatFits)
{
	// 100 pixels: ten characters
	EXPECT_EQ(Wrap(u"short", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"short"}));
	EXPECT_EQ(Wrap(u"one two three four", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"one two", u"three four"}));
	// The line breaks at the last blank, those before it trimmed
	EXPECT_EQ(Wrap(u"aaaa    bbbbbbb", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"aaaa", u"bbbbbbb"}));
	// A blank starting a line after a line break stays
	EXPECT_EQ(Wrap(u"ab\n cd", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"ab", u" cd"}));
}

TEST(TipBubble, WrapBreaksAfterAHyphenOrWhereItMust)
{
	EXPECT_EQ(Wrap(u"well-known words", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"well-known", u"words"}));
	EXPECT_EQ(Wrap(u"abcdefghijklmnop", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"abcdefghij", u"klmnop"}));
	// Not a character fits: nothing more
	EXPECT_TRUE(Wrap(u"abc", 5.0f, 19.0f, Measure).empty());
}

TEST(TipBubble, WrapEndsLinesAtLineBreaks)
{
	EXPECT_EQ(Wrap(u"ab\ncd", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"ab", u"cd"}));
	// Both of a CR LF break the line: an empty line between
	EXPECT_EQ(Wrap(u"ab\r\ncd", 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"ab", u"", u"cd"}));
	// The blank that adds no space is a place to break too
	const std::u16string tilde = u"aaaaaa" + std::u16string(1, char16_t {0xF8FE}) + u"bbbbbbbbb";
	EXPECT_EQ(Wrap(tilde, 100.0f, 19.0f, Measure), (std::vector<std::u16string> {u"aaaaaa", u"bbbbbbbbb"}));
}
