/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>

#include "Hand/HandTricons.h"

using namespace openblack::hand_tricons;

namespace
{
constexpr uint32_t k_RotateOffered = icon::k_Rotate | (icon::k_Rotate << icon::k_OfferedShift);
constexpr uint32_t k_ZoomOffered = icon::k_Zoom | (icon::k_Zoom << icon::k_OfferedShift);

void ExpectNear(glm::vec2 actual, glm::vec2 expected)
{
	EXPECT_NEAR(actual.x, expected.x, 1e-4f);
	EXPECT_NEAR(actual.y, expected.y, 1e-4f);
}
} // namespace

TEST(HandTricons, TheCameraIconsGoWithTheToolTipsOffOrWhileTheHandHolds)
{
	const uint32_t all = k_RotateOffered | k_ZoomOffered | icon::k_Pitch | icon::k_Cross;
	EXPECT_EQ(Shown(all, true, true), all);
	EXPECT_EQ(Shown(all, false, true), icon::k_Cross);
	EXPECT_EQ(Shown(all, true, false), icon::k_Cross);
}

TEST(HandTricons, AnOfferedIconFadesInToSixTenths)
{
	Fades fades {};
	Fade(fades, {.icons = k_RotateOffered, .seconds = 0.1f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[1], 0.5f);
	Fade(fades, {.icons = k_RotateOffered, .seconds = 0.1f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[1], 0.6f);
	EXPECT_FLOAT_EQ(fades[0], 0.0f);
}

TEST(HandTricons, AnIconInUseRisesToFullAndFallsAway)
{
	Fades fades {0.0f, 0.6f, 0.0f, 0.0f};
	// Held, the drag going round: full strength
	Fade(fades, {.icons = icon::k_Rotate, .seconds = 0.05f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[1], 0.85f);
	Fade(fades, {.icons = icon::k_Rotate, .seconds = 0.05f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[1], 1.0f);
	// Back to only offered, it falls to six tenths
	Fade(fades, {.icons = k_RotateOffered, .seconds = 0.05f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[1], 0.75f);
	Fade(fades, {.icons = k_RotateOffered, .seconds = 1.0f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[1], 0.6f);
	// Gone from the recording, it fades out at the same speed
	Fade(fades, {.icons = 0, .seconds = 0.1f, .demonstration = true});
	EXPECT_NEAR(fades[1], 0.1f, 1e-6f);
	Fade(fades, {.icons = 0, .seconds = 0.1f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[1], 0.0f);
}

TEST(HandTricons, TheWorldCameraMovingDimsTheIconsInUse)
{
	Fades fades {};
	Fade(fades, {.icons = icon::k_Pitch, .seconds = 1.0f, .cameraBusy = true});
	EXPECT_FLOAT_EQ(fades[0], 0.9f);
}

TEST(HandTricons, TheCrossNeverShowsInADemonstration)
{
	Fades fades {0.0f, 0.0f, 0.0f, 0.8f};
	Fade(fades, {.icons = icon::k_Cross, .seconds = 0.01f, .demonstration = true});
	EXPECT_FLOAT_EQ(fades[3], 0.0f);
	// Otherwise it falls slowly to a faint mark
	fades[3] = 0.8f;
	Fade(fades, {.icons = icon::k_Cross, .seconds = 1.0f});
	EXPECT_NEAR(fades[3], 0.5f, 1e-6f);
	Fade(fades, {.icons = icon::k_Cross, .seconds = 10.0f});
	EXPECT_FLOAT_EQ(fades[3], 0.2f);
}

TEST(HandTricons, SitThreeQuartersOfTheWayFromTheLastGripToTheHand)
{
	EXPECT_EQ(Place({400, 300}, {0, 0}, {1280, 720}, false), glm::ivec2(300, 225));
	EXPECT_EQ(Place({400, 300}, {400, 300}, {1280, 720}, false), glm::ivec2(400, 300));
	// Whole pixels, rounded towards zero
	EXPECT_EQ(Place({401, 301}, {0, 0}, {1280, 720}, false), glm::ivec2(300, 225));
}

TEST(HandTricons, KeepSixteenPixelsInsideThePicture)
{
	EXPECT_EQ(Place({0, 0}, {0, 0}, {1280, 720}, false), glm::ivec2(16, 16));
	EXPECT_EQ(Place({2000, 2000}, {2000, 2000}, {1280, 720}, false), glm::ivec2(1264, 704));
	// With the cinema bars on, inside the 16:9 picture across the screen's width
	EXPECT_EQ(Place({640, 0}, {640, 0}, {1280, 1024}, true), glm::ivec2(640, 168));
	EXPECT_EQ(Place({640, 1024}, {640, 1024}, {1280, 1024}, true), glm::ivec2(640, 856));
	// A 16:9 screen has no bars to keep inside
	EXPECT_EQ(Place({640, 0}, {640, 0}, {1280, 720}, true), glm::ivec2(640, 16));
}

TEST(HandTricons, EachIconIsItsFrameOfTheAtmosphereTexture)
{
	const Fades fades {1.0f, 1.0f, 1.0f, 1.0f};
	const auto sprites = Sprites({100.0f, 200.0f}, 10.0f, fades, 0.0f);
	for (size_t i = 0; i < sprites.size(); ++i)
	{
		ASSERT_TRUE(sprites.at(i).has_value());
		const auto& sprite = *sprites.at(i);
		const float u = static_cast<float>(i) * 0.25f;
		ExpectNear(sprite.uvs[0], {u, 0.75f});
		ExpectNear(sprite.uvs[2], {u + 0.25f, 1.0f});
		ExpectNear(sprite.corners[0], {90.0f, 190.0f});
		ExpectNear(sprite.corners[2], {110.0f, 210.0f});
		EXPECT_FLOAT_EQ(sprite.alpha, 1.0f);
	}
}

TEST(HandTricons, OnlyTheRotateArrowTurnsClockwise)
{
	const Fades fades {1.0f, 1.0f, 0.0f, 0.0f};
	const auto sprites = Sprites({0.0f, 0.0f}, 10.0f, fades, glm::radians(90.0f));
	ASSERT_TRUE(sprites[0].has_value());
	ASSERT_TRUE(sprites[1].has_value());
	ExpectNear(sprites[0]->corners[0], {-10.0f, -10.0f});
	// A quarter turn clockwise takes the top left corner to the top right
	ExpectNear(sprites[1]->corners[0], {10.0f, -10.0f});
	ExpectNear(sprites[1]->corners[1], {10.0f, 10.0f});
	EXPECT_FALSE(sprites[2].has_value());
	EXPECT_FALSE(sprites[3].has_value());
}

TEST(HandTricons, AnIconFainterThanAStepOfAlphaIsLeftOut)
{
	const Fades fades {0.003f, 0.5f, 0.0f, 0.0f};
	const auto sprites = Sprites({0.0f, 0.0f}, 10.0f, fades, 0.0f);
	EXPECT_FALSE(sprites[0].has_value());
	ASSERT_TRUE(sprites[1].has_value());
	EXPECT_FLOAT_EQ(sprites[1]->alpha, 127.0f / 255.0f);
}

TEST(HandTricons, TheDemoMouseChangesSideOnlyInTheOuterThirds)
{
	EXPECT_FALSE(LabelOnLeft(600, 1280, false));
	EXPECT_TRUE(LabelOnLeft(900, 1280, false));
	EXPECT_TRUE(LabelOnLeft(600, 1280, true));
	EXPECT_FALSE(LabelOnLeft(400, 1280, true));
}

TEST(HandTricons, TheDemoMouseSitsRightOfTheIconsThenItsWord)
{
	const auto mouse = LayoutDemoMouse({400, 300}, false, 40.0f, false, false, false);
	EXPECT_EQ(mouse.mouseMin, glm::vec2(432.0f, 284.0f));
	EXPECT_EQ(mouse.mouseMax, glm::vec2(464.0f, 316.0f));
	EXPECT_FLOAT_EQ(mouse.labelSize, 21.0f);
	EXPECT_EQ(mouse.labelAt, glm::vec2(466.0f, 289.5f));
	EXPECT_EQ(mouse.labelGlowMin, glm::vec2(466.0f, 289.0f));
	EXPECT_EQ(mouse.labelGlowMax, glm::vec2(506.0f, 310.0f));
	EXPECT_EQ(mouse.labelColour, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_FLOAT_EQ(mouse.labelGlow.a, 85.0f / 255.0f);
	EXPECT_FLOAT_EQ(mouse.mouseGlow.a, 1.0f);
	EXPECT_EQ(DemoLabelSize(true), 25);
}

TEST(HandTricons, OnTheLeftTheWordComesFirstEndingThirtyTwoPixelsFromTheIcons)
{
	const auto mouse = LayoutDemoMouse({1000, 300}, true, 40.0f, false, false, false);
	EXPECT_EQ(mouse.labelAt.x, 894.0f);
	EXPECT_EQ(mouse.mouseMin.x, 936.0f);
	EXPECT_EQ(mouse.mouseMax.x, 968.0f);
}

TEST(HandTricons, TheDemoMouseLightsTheButtonsTheRecordingHolds)
{
	// None lit
	auto mouse = LayoutDemoMouse({400, 300}, false, 40.0f, false, false, false);
	EXPECT_EQ(mouse.uvMin, glm::vec2(0.0f, 0.5f));
	EXPECT_EQ(mouse.uvMax, glm::vec2(0.25f, 0.75f));
	// The move button is the left: the right one's picture mirrored
	mouse = LayoutDemoMouse({400, 300}, false, 40.0f, false, true, false);
	EXPECT_EQ(mouse.uvMin, glm::vec2(0.5f, 0.5f));
	EXPECT_EQ(mouse.uvMax, glm::vec2(0.25f, 0.75f));
	mouse = LayoutDemoMouse({400, 300}, false, 40.0f, false, false, true);
	EXPECT_EQ(mouse.uvMin, glm::vec2(0.25f, 0.5f));
	EXPECT_EQ(mouse.uvMax, glm::vec2(0.5f, 0.75f));
	// Both, for zooming
	mouse = LayoutDemoMouse({400, 300}, false, 40.0f, false, true, true);
	EXPECT_EQ(mouse.uvMin, glm::vec2(0.5f, 0.5f));
	EXPECT_EQ(mouse.uvMax, glm::vec2(0.75f, 0.75f));
}
