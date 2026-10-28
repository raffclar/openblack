/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The pictures of gestures and controls on the temple's scroll that the camera is close to, through a made up camera

#include <Temple/ScrollPictures.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace pictures = temple::scroll_pictures;
using Kind = temple::scroll_pictures::WorldPicture::Kind;

namespace
{
/// A camera that puts a point's x and y on the screen as pixels and its z as the depth. Nearer than 1 is behind it,
/// and the screen is 640 by 480.
pictures::ScreenPoint Project(glm::vec3 point)
{
	using Seen = pictures::ScreenPoint::Seen;
	pictures::ScreenPoint out {.pixel = {point.x, point.y}, .depth = point.z};
	if (point.z < 1.0f)
	{
		out.seen = Seen::Behind;
		return out;
	}
	const bool inside = point.x >= 0.0f && point.y >= 0.0f && point.x < 640.0f && point.y < 480.0f;
	out.seen = inside ? Seen::OnScreen : Seen::OffScreen;
	return out;
}

/// A line 12.5 pixels high at a depth of 50 on a texture from (100, 40) to (300, 440)
pictures::WorldPicture Line(Kind kind, int number, float top = 100.0f)
{
	return {
	    .kind = kind,
	    .number = number,
	    .lineTop = {200.0f, top, 50.0f},
	    .lineBelow = {200.0f, top + 12.5f, 50.0f},
	    .textureTopLeft = {100.0f, 40.0f, 50.0f},
	    .textureBottomRight = {300.0f, 440.0f, 50.0f},
	};
}

bool AllShown(int /*action*/)
{
	return true;
}
} // namespace

TEST(ScrollPictures, AGestureIsTwoWholeLinesHighFromTheLineTopWithItsShadowFirst)
{
	pictures::Pictures out;
	pictures::Place(out, Line(Kind::Gesture, 5), Project, AllShown, 1.0f);
	ASSERT_EQ(out.gestures.size(), 2);
	const auto& shadow = out.gestures[0];
	const auto& picture = out.gestures[1];
	EXPECT_TRUE(shadow.icon.shadow);
	EXPECT_FALSE(picture.icon.shadow);
	// 12.5 pixels is 12 whole pixels a line, so the picture is 24 square, centred between the texture's sides
	EXPECT_FLOAT_EQ(picture.icon.min.x, 188.0f);
	EXPECT_FLOAT_EQ(picture.icon.max.x, 212.0f);
	EXPECT_FLOAT_EQ(picture.icon.min.y, 100.0f);
	EXPECT_FLOAT_EQ(picture.icon.max.y, 124.0f);
	EXPECT_EQ(picture.icon.colour, glm::vec4(1.0f));
	// The shadow is two pixels down and right, black
	EXPECT_FLOAT_EQ(shadow.icon.min.x, 190.0f);
	EXPECT_FLOAT_EQ(shadow.icon.min.y, 102.0f);
	EXPECT_EQ(shadow.icon.colour, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	// In front of the line by 0.7, then the shadow one more and the picture two more
	EXPECT_FLOAT_EQ(shadow.depth, 50.0f - 0.7f - 1.0f);
	EXPECT_FLOAT_EQ(picture.depth, 50.0f - 0.7f - 2.0f);
}

TEST(ScrollPictures, TheShadowOffsetFollowsTheScreensScale)
{
	pictures::Pictures out;
	pictures::Place(out, Line(Kind::Gesture, 5), Project, AllShown, 2.0f);
	ASSERT_EQ(out.gestures.size(), 2);
	EXPECT_FLOAT_EQ(out.gestures[0].icon.min.y, 104.0f);
}

TEST(ScrollPictures, AGestureIsCutToTheTexturesTop)
{
	pictures::Pictures out;
	pictures::Place(out, Line(Kind::Gesture, 5, 34.0f), Project, AllShown, 1.0f);
	ASSERT_EQ(out.gestures.size(), 2);
	EXPECT_FLOAT_EQ(out.gestures[1].icon.min.y, 40.0f);
	EXPECT_FLOAT_EQ(out.gestures[1].icon.uvMin.y - static_cast<float>(5 / 4) * 0.25f, 0.25f * 0.25f);
}

TEST(ScrollPictures, OnlyTheGamesGesturesArePictured)
{
	for (const int number : {0, 24})
	{
		pictures::Pictures out;
		pictures::Place(out, Line(Kind::Gesture, number), Project, AllShown, 1.0f);
		EXPECT_TRUE(out.gestures.empty()) << number;
	}
}

TEST(ScrollPictures, NothingShowsUnlessTheLineIsOnTheScreenAndTheTextureInFront)
{
	auto offScreen = Line(Kind::Gesture, 5);
	offScreen.lineBelow.x = 700.0f;
	auto behind = Line(Kind::Gesture, 5);
	behind.textureBottomRight.z = 0.5f;
	// The pictures also use a corner of the texture that is off the screen but in front of the camera
	auto cornerOff = Line(Kind::Gesture, 5);
	cornerOff.textureTopLeft.x = -50.0f;
	for (const auto& [line, shown] : {std::pair {offScreen, false}, std::pair {behind, false}, std::pair {cornerOff, true}})
	{
		pictures::Pictures out;
		pictures::Place(out, line, Project, AllShown, 1.0f);
		EXPECT_EQ(out.gestures.empty(), !shown);
	}
}

TEST(ScrollPictures, AControlStartsOneLineLowerWhenItsActionIsBound)
{
	pictures::Pictures out;
	pictures::Place(out, Line(Kind::Control, 2), Project, AllShown, 1.0f);
	ASSERT_EQ(out.controls.size(), 1);
	const auto& control = out.controls[0];
	EXPECT_EQ(control.action, 2);
	EXPECT_EQ(control.size, 24);
	// Half a picture right of the middle
	EXPECT_EQ(control.right, 212);
	// Its middle row (from 112 to 136) less a fifth of its size, then half its size
	EXPECT_EQ(control.top, static_cast<int>(124.0f - (24.0f * 0.2f)) - 12);
	EXPECT_FLOAT_EQ(control.alpha, 1.0f);

	pictures::Pictures unbound;
	pictures::Place(unbound, Line(Kind::Control, 2), Project, [](int) { return false; }, 1.0f);
	EXPECT_TRUE(unbound.controls.empty());
}

TEST(ScrollPictures, AControlShowsOnlyFullyInsideTheTexture)
{
	pictures::Pictures out;
	pictures::Place(out, Line(Kind::Control, 2, 28.0f), Project, AllShown, 1.0f);
	pictures::Place(out, Line(Kind::Control, 2, 420.0f), Project, AllShown, 1.0f);
	EXPECT_TRUE(out.controls.empty());
}
