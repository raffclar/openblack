/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
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
/// No action has a binding to show
bool NoControls(int /*action*/)
{
	return false;
}
/// Every action has one
bool EveryControl(int /*action*/)
{
	return true;
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
	EXPECT_FALSE(Place({100.0f, 100.0f}, 0.5f, 640.0f, 1.0f).has_value());
	EXPECT_TRUE(Place({100.0f, 100.0f}, 1.0f, 640.0f, 1.0f).has_value());
}

TEST(TipBubble, StandsAMarginAboveTheSignTailTowardsTheMiddle)
{
	// In the middle of the screen: the tail ten pixels right, a fifth of the box left of it
	const auto middle = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(middle.has_value());
	EXPECT_FLOAT_EQ(middle->tailX, 330.0f);
	EXPECT_FLOAT_EQ(middle->min.x, 330.0f - 80.0f);
	EXPECT_FLOAT_EQ(middle->max.y, 400.0f - 40.0f);
	EXPECT_FLOAT_EQ(middle->min.y, 400.0f - 40.0f - 250.0f);
	EXPECT_FALSE(middle->tailFlipped);
	// At the right edge: the tail ten pixels left, flipped, eight tenths of the box left of it
	const auto right = Place({640.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(right.has_value());
	EXPECT_FLOAT_EQ(right->tailX, 630.0f);
	EXPECT_FLOAT_EQ(right->min.x, 630.0f - 320.0f);
	EXPECT_TRUE(right->tailFlipped);
	// At the left edge it still reaches left of the tail, off the screen: nothing keeps it on
	const auto left = Place({0.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(left.has_value());
	EXPECT_FLOAT_EQ(left->tailX, 10.0f);
	EXPECT_FLOAT_EQ(left->min.x, 10.0f - 320.0f);
}

TEST(TipBubble, TheBoxIsNinePiecesAndATailSeeThroughAtTheTop)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
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
	const auto placement = Place({600.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
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
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	// The tip, a blank line and the title: one line each
	const std::array<std::u16string_view, 3> items {u"T", u" ", u"D"};
	const auto words = LayOutWords(*placement, items, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach, NoControls);
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
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	// Enough lines to reach the top: the stack stops at the item that gets there
	const std::u16string tall(20, u'x');
	const std::array<std::u16string_view, 3> items {tall, u" ", u"D"};
	const auto words = LayOutWords(*placement, items, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach, NoControls);
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
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
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

TEST(TipBubble, ItsPixelSizesTakeTheDefaultScreensShare)
{
	// At 2.4 times the default screen's pixels (2560 by 1440 and wider) every size is 2.4 times the game's
	const auto base = Place({1280.0f, 800.0f}, 2.0f, 2560.0f, 1.0f);
	const auto scaled = Place({1280.0f, 800.0f}, 2.0f, 2560.0f, 2.4f);
	ASSERT_TRUE(base.has_value());
	ASSERT_TRUE(scaled.has_value());
	EXPECT_FLOAT_EQ(scaled->sizes.width, 400.0f * 2.4f);
	EXPECT_FLOAT_EQ(scaled->sizes.height, 250.0f * 2.4f);
	EXPECT_FLOAT_EQ(scaled->sizes.margin, base->sizes.margin * 2.4f);
	EXPECT_FLOAT_EQ(scaled->sizes.lineHeight, base->sizes.lineHeight * 2.4f);
	EXPECT_FLOAT_EQ(scaled->pixelScale, 2.4f);
	// The tail's point keeps its place by the sign, ten default pixels to the middle's side
	EXPECT_FLOAT_EQ(scaled->tailX - 1280.0f, (base->tailX - 1280.0f) * 2.4f);
	EXPECT_FLOAT_EQ(scaled->max.y, 800.0f - scaled->sizes.margin);
	// The words' shadows sit two default pixels down and right
	const std::array<std::u16string_view, 1> items {u"Hi"};
	const WidthFn width = [](std::u16string_view text, float size) { return static_cast<float>(text.size()) * size; };
	const WrapFn wrap = [](std::u16string_view text, float, float) {
		return std::vector<std::u16string> {std::u16string(text)};
	};
	const auto words = LayOutWords(*scaled, items, 0.0f, 1.0f, wrap, width, NoControls);
	ASSERT_EQ(words.lines.size(), 1);
	ASSERT_EQ(words.shadows.size(), 1);
	EXPECT_NEAR(words.shadows.front().at.x - words.lines.front().at.x, 2.0f * 2.4f, 1e-3f);
	EXPECT_NEAR(words.shadows.front().at.y - words.lines.front().at.y, 2.0f * 2.4f, 1e-3f);
}

TEST(TipBubble, ItReadsGestureAndControlCodes)
{
	EXPECT_EQ(ReadCode(u"$m27"), (InlineCode {.kind = InlineCode::Kind::Control, .number = 27}));
	EXPECT_EQ(ReadCode(u"$g1"), (InlineCode {.kind = InlineCode::Kind::Gesture, .number = 1}));
	EXPECT_EQ(ReadCode(u"$m 5 and more"), (InlineCode {.kind = InlineCode::Kind::Control, .number = 5}));
	// A code with no number is code 0
	EXPECT_EQ(ReadCode(u"$m"), (InlineCode {.kind = InlineCode::Kind::Control, .number = 0}));
	// Other codes and words aren't picture codes
	EXPECT_FALSE(ReadCode(u"$p").has_value());
	EXPECT_FALSE(ReadCode(u"$M2").has_value());
	EXPECT_FALSE(ReadCode(u"m27").has_value());
	EXPECT_FALSE(ReadCode(u"").has_value());
}

TEST(TipBubble, ACodeSplitsItsTipFromTheBottomUp)
{
	using Kind = Piece::Kind;
	const auto control = [](int number) {
		return Piece {.kind = Kind::Code, .words = {}, .code = {.kind = InlineCode::Kind::Control, .number = number}};
	};
	const auto gesture = [](int number) {
		return Piece {.kind = Kind::Code, .words = {}, .code = {.kind = InlineCode::Kind::Gesture, .number = number}};
	};
	const Piece gap {.kind = Kind::Gap, .words = {}, .code = {}};
	const auto words = [](std::u16string_view text) { return Piece {.kind = Kind::Words, .words = text, .code = {}}; };

	// Words alone
	EXPECT_EQ(SplitCodes(u"Plain words"), std::vector<Piece> {words(u"Plain words")});
	// A code alone, read from its first four characters only
	EXPECT_EQ(SplitCodes(u"$m27"), std::vector<Piece> {control(27)});
	EXPECT_EQ(SplitCodes(u"$m123 and words"), std::vector<Piece> {control(12)});
	// Words then a code: the picture, a gap, the words, and the picture again
	EXPECT_EQ(SplitCodes(u"Press this key. $m27"),
	          (std::vector<Piece> {control(27), gap, words(u"Press this key. "), control(27)}));
	// The first gesture code is found before any control code
	EXPECT_EQ(SplitCodes(u"A $m1 then $g2"), (std::vector<Piece> {gesture(2), gap, words(u"A $m1 then "), gesture(2)}));
}

TEST(TipBubble, AControlCodeShowsItsPictureTwoLinesTall)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	const std::array<std::u16string_view, 1> items {u"T $m27"};
	const auto words = LayOutWords(*placement, items, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach, EveryControl);
	// Two pictures of 38 pixels, the gap of 0.7 lines and the two lines of words
	EXPECT_NEAR(words.contentHeight, 38.0f + 13.3f + 38.0f + 38.0f, 1e-3f);
	const float bottomLine = 360.0f - (40.0f / 3.0f);
	ASSERT_EQ(words.lines.size(), 2u);
	EXPECT_NEAR(words.lines.at(0).at.y, bottomLine - 38.0f - 13.3f - 38.0f, 1e-3f);
	ASSERT_EQ(words.controls.size(), 2u);
	const auto& below = words.controls.at(0);
	EXPECT_EQ(below.action, 27);
	EXPECT_EQ(below.size, 38);
	// Half a picture right of the middle of the margins, and raised a fifth of a picture from the middle of its lines
	EXPECT_EQ(below.right, 469);
	EXPECT_EQ(below.top, 301);
	EXPECT_NEAR(below.alpha, 1.0f, 1.5f / 255.0f);
	EXPECT_EQ(words.controls.at(1).top, 211);
	// Right-aligned after the gap
	EXPECT_EQ(PlaceControl(below, 38.0f, 2.0f), (ControlBox {.min = {429.0f, 301.0f}, .max = {467.0f, 339.0f}}));
	EXPECT_EQ(PlaceControl(below, 60.5f, 2.0f), (ControlBox {.min = {406.0f, 301.0f}, .max = {466.0f, 339.0f}}));
}

TEST(TipBubble, AnUnboundControlTakesNoRoom)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	const std::array<std::u16string_view, 1> items {u"T $m27"};
	const auto words = LayOutWords(*placement, items, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach, NoControls);
	// Only the gap and the words
	EXPECT_TRUE(words.controls.empty());
	EXPECT_NEAR(words.contentHeight, 13.3f + 38.0f, 1e-3f);
}

TEST(TipBubble, AControlFadesInOverTheTopThird)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	const std::array<std::u16string_view, 1> items {u"$m27"};
	const float bottomLine = 360.0f - (40.0f / 3.0f);
	// Its top 20 pixels below the box's top, of the 250 / 3 the fade takes
	const float scroll = 130.0f + 38.0f - bottomLine;
	const auto words = LayOutWords(*placement, items, scroll, 1.0f, OneLinePerCharacter, TenPixelsEach, EveryControl);
	ASSERT_EQ(words.controls.size(), 1u);
	EXPECT_NEAR(words.controls.front().alpha, std::trunc(20.0f / (250.0f / 3.0f) * 255.0f) / 255.0f, 1.5f / 255.0f);
	// Reaching above the box's top it isn't drawn, but still takes its room
	const auto above = LayOutWords(*placement, items, scroll - 30.0f, 1.0f, OneLinePerCharacter, TenPixelsEach, EveryControl);
	EXPECT_TRUE(above.controls.empty());
	EXPECT_NEAR(above.contentHeight, 38.0f, 1e-3f);
}

TEST(TipBubble, AGestureCodeShowsItsPictureWithAShadow)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	const std::array<std::u16string_view, 1> items {u"$g1"};
	const auto words = LayOutWords(*placement, items, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach, NoControls);
	EXPECT_NEAR(words.contentHeight, 38.0f, 1e-3f);
	ASSERT_EQ(words.gestures.size(), 2u);
	const float bottomLine = 360.0f - (40.0f / 3.0f);
	// The shadow first, two pixels down and right in black at half the alpha
	const auto& shadow = words.gestures.at(0);
	const auto& picture = words.gestures.at(1);
	EXPECT_NEAR(shadow.min.x, 433.0f, 1e-3f);
	EXPECT_NEAR(shadow.min.y, bottomLine - 36.0f, 1e-3f);
	EXPECT_EQ(shadow.colour, glm::vec4(0.0f, 0.0f, 0.0f, 128.0f / 255.0f));
	// The picture centred between the margins, the second cell of the first sheet
	EXPECT_EQ(picture.sheet, 0);
	EXPECT_NEAR(picture.min.x, 431.0f, 1e-3f);
	EXPECT_NEAR(picture.max.x, 469.0f, 1e-3f);
	EXPECT_NEAR(picture.min.y, bottomLine - 38.0f, 1e-3f);
	EXPECT_NEAR(picture.max.y, bottomLine, 1e-3f);
	EXPECT_EQ(picture.uvMin, glm::vec2(0.25f, 0.0f));
	EXPECT_EQ(picture.uvMax, glm::vec2(0.5f, 0.25f));
	EXPECT_EQ(picture.colour, glm::vec4(1.0f));
	// Gestures outside 1 to 23 show nothing and take no room
	for (const auto* item : {u"$g0", u"$g24"})
	{
		const std::array<std::u16string_view, 1> none {item};
		const auto nothing = LayOutWords(*placement, none, 0.0f, 1.0f, OneLinePerCharacter, TenPixelsEach, NoControls);
		EXPECT_TRUE(nothing.gestures.empty());
		EXPECT_FLOAT_EQ(nothing.contentHeight, 0.0f);
	}
}

TEST(TipBubble, AGestureIsCutAtTheBoxsTop)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	const std::array<std::u16string_view, 1> items {u"$g1"};
	const float bottomLine = 360.0f - (40.0f / 3.0f);
	// Its top 10 pixels above the box's
	const float scroll = 100.0f + 38.0f - bottomLine;
	const auto words = LayOutWords(*placement, items, scroll, 1.0f, OneLinePerCharacter, TenPixelsEach, NoControls);
	ASSERT_EQ(words.gestures.size(), 2u);
	const auto& picture = words.gestures.at(1);
	EXPECT_NEAR(picture.min.y, 110.0f, 1e-3f);
	EXPECT_NEAR(picture.uvMin.y, (10.0f / 38.0f) * 0.25f, 1e-4f);
	EXPECT_NEAR(picture.max.y, 138.0f, 1e-3f);
}

TEST(TipBubble, GrippedTheWordsFollowTheHand)
{
	Scroll scroll;
	scroll.lines = 4.0f;
	// Before the words were laid out the hand moves nothing
	FollowHand(scroll, 300.0f);
	EXPECT_FLOAT_EQ(scroll.lines, 4.0f);
	EXPECT_FALSE(scroll.handY.has_value());
	scroll.lineHeight = 19.0f;
	// The first gripped frame only notes where the hand is
	FollowHand(scroll, 300.0f);
	EXPECT_FLOAT_EQ(scroll.lines, 4.0f);
	// Up the screen by two lines' height: the words go up with it, two lines further down the tip
	FollowHand(scroll, 262.0f);
	EXPECT_FLOAT_EQ(scroll.lines, 2.0f);
	// Down again by half a line
	FollowHand(scroll, 271.5f);
	EXPECT_FLOAT_EQ(scroll.lines, 2.5f);
	// Let go, the next grip starts again from where the hand is then
	LetGo(scroll);
	EXPECT_FALSE(scroll.handY.has_value());
	FollowHand(scroll, 100.0f);
	EXPECT_FLOAT_EQ(scroll.lines, 2.5f);
}

TEST(TipBubble, TheHandScrollsOnlyAsFarAsTheWordsGo)
{
	const auto placement = Place({320.0f, 400.0f}, 2.0f, 640.0f, 1.0f);
	ASSERT_TRUE(placement.has_value());
	Scroll scroll;
	// 380 pixels of words in a box of 250 less two margins: 210 / 19 lines at most
	scroll.contentHeight = 380.0f;
	scroll.lineHeight = 19.0f;
	ClampScroll(scroll, *placement);
	FollowHand(scroll, 100.0f);
	// Dragged down past the top of the words, held there when next clamped
	FollowHand(scroll, 500.0f);
	ClampScroll(scroll, *placement);
	EXPECT_FLOAT_EQ(scroll.lines, 210.0f / 19.0f);
	// Dragged up past the bottom, held at none
	FollowHand(scroll, -1000.0f);
	ClampScroll(scroll, *placement);
	EXPECT_FLOAT_EQ(scroll.lines, 0.0f);
	EXPECT_FALSE(scroll.moreBelow);
	EXPECT_TRUE(scroll.moreAbove);
}
