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

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

TEST(HandTricons, SitThreeQuartersOfTheWayFromTheCursorToTheHand)
{
	EXPECT_EQ(Place({400, 300}, {0, 0}, {1280, 720}, false, true), glm::ivec2(300, 225));
	EXPECT_EQ(Place({400, 300}, {400, 300}, {1280, 720}, false, true), glm::ivec2(400, 300));
	// Whole pixels, rounded towards zero
	EXPECT_EQ(Place({401, 301}, {0, 0}, {1280, 720}, false, true), glm::ivec2(300, 225));
}

TEST(HandTricons, SitOnTheHandWhereItFollowsTheCursor)
{
	// The hand is drawn where the cursor is, so the icons sit on it at any resolution: never pulled a quarter of the
	// way back towards the top-left corner or towards where the land was last gripped
	EXPECT_EQ(Place({899, 1010}, {900, 1010}, {1280, 1024}, false, true), glm::ivec2(899, 1008));
	EXPECT_EQ(Place({1800, 1420}, {1800, 1420}, {2560, 1440}, false, true), glm::ivec2(1800, 1420));
	EXPECT_EQ(Place({40, 700}, {40, 700}, {2560, 1440}, false, true), glm::ivec2(40, 700));
}

TEST(HandTricons, AnchorAtTheHandsFirstBoneNotWhereItHovers)
{
	// A model 100 units below its origin in its first bone, turned upside down about x and drawn at a hundredth of
	// its size, mirrored across like a right hand
	const std::array bones {glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, -100.0f, 0.0f)), glm::mat4(1.0f)};
	const auto upsideDown = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));
	const auto anchor = AnchorPoint({5.0f, 20.0f, 7.0f}, upsideDown, {-0.01f, 0.01f, 0.01f}, bones);
	EXPECT_NEAR(anchor.x, 4.9f, 1e-5f);
	EXPECT_NEAR(anchor.y, 21.0f, 1e-5f);
	EXPECT_NEAR(anchor.z, 7.0f, 1e-5f);
	// Without a pose, the hand's own place
	const auto unposed = AnchorPoint({5.0f, 20.0f, 7.0f}, glm::mat3(1.0f), glm::vec3(1.0f), {});
	EXPECT_EQ(unposed, glm::vec3(5.0f, 20.0f, 7.0f));
}

TEST(HandTricons, WithoutLeaningSitExactlyOnTheHand)
{
	EXPECT_EQ(Place({400, 300}, {0, 0}, {1280, 720}, false, false), glm::ivec2(400, 300));
}

TEST(HandTricons, KeepSixteenPixelsInsideThePicture)
{
	EXPECT_EQ(Place({0, 0}, {0, 0}, {1280, 720}, false, true), glm::ivec2(16, 16));
	EXPECT_EQ(Place({2000, 2000}, {2000, 2000}, {1280, 720}, false, true), glm::ivec2(1264, 704));
	// With the cinema bars on, inside the 16:9 picture across the screen's width
	EXPECT_EQ(Place({640, 0}, {640, 0}, {1280, 1024}, true, true), glm::ivec2(640, 168));
	EXPECT_EQ(Place({640, 1024}, {640, 1024}, {1280, 1024}, true, true), glm::ivec2(640, 856));
	// A 16:9 screen has no bars to keep inside
	EXPECT_EQ(Place({640, 0}, {640, 0}, {1280, 720}, true, true), glm::ivec2(640, 16));
	// Nor a wider one: the picture is still the screen's height, not taller
	EXPECT_EQ(Place({640, 0}, {640, 0}, {5120, 1440}, true, true), glm::ivec2(640, 16));
	EXPECT_EQ(Place({640, 1440}, {640, 1440}, {5120, 1440}, true, true), glm::ivec2(640, 1424));
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

TEST(HandTricons, WithoutLeaningTheyGoAtTheHand)
{
	EXPECT_EQ(Place({400, 300}, {0, 0}, {1280, 720}, false, false), glm::ivec2(400, 300));
}

namespace
{
constexpr uint32_t k_AllFeatures = 0x1BF;
constexpr uint32_t k_HintRotate = 0x01;
constexpr uint32_t k_HintPitch = 0x02;
constexpr uint32_t k_HintTop = 0x04;
constexpr uint32_t k_HintTurning = 0x40;
constexpr uint32_t k_HintIdle = 0x80;
constexpr uint32_t k_PitchOffered = icon::k_Pitch | (icon::k_Pitch << icon::k_OfferedShift);
} // namespace

TEST(HandTricons, HoveringAtTheBottomOffersThePitchAndRotateArrows)
{
	const auto icons = WorldCamera({.mouseHints = k_HintIdle | k_HintRotate | k_HintPitch | k_HintTurning,
	                                .features = k_AllFeatures,
	                                .cursor = {0.0f, 0.5f}},
	                               1.0f);
	EXPECT_EQ(icons.icons, k_PitchOffered | k_RotateOffered);
	// The rotate arrow lies flat at the bottom of the screen
	EXPECT_NEAR(icons.rotateAngle, 0.0f, 1e-6f);
}

TEST(HandTricons, DraggingToTiltShowsThePitchArrowInUse)
{
	const auto icons = WorldCamera(
	    {.mouseHints = k_HintPitch, .gripping = true, .grippingOnly = true, .pitchDragging = true, .features = k_AllFeatures},
	    0.0f);
	EXPECT_EQ(icons.icons, icon::k_Pitch);
}

TEST(HandTricons, TheTiltKeysShowThePitchArrow)
{
	// Tilting: in use
	auto icons = WorldCamera({.tilt = 0.5f, .features = k_AllFeatures}, 0.0f);
	EXPECT_EQ(icons.icons, icon::k_Pitch);
	// The rotate key held offers tilting and turning, the rotate arrow lying flat
	icons = WorldCamera({.rotateKeyHeld = true, .features = k_AllFeatures}, 2.0f);
	EXPECT_EQ(icons.icons, k_PitchOffered | k_RotateOffered);
	EXPECT_FLOAT_EQ(icons.rotateAngle, 0.0f);
	// The zoom key offers zooming and turning
	icons = WorldCamera({.zoomKeyHeld = true, .features = k_AllFeatures}, 0.0f);
	EXPECT_EQ(icons.icons, k_RotateOffered | k_ZoomOffered);
	// Not while the land is gripped
	icons = WorldCamera({.gripping = true, .zoomKeyHeld = true, .features = k_AllFeatures}, 0.0f);
	EXPECT_EQ(icons.icons, 0u);
}

TEST(HandTricons, TurningRoundTheEdgeTurnsTheRotateArrow)
{
	const auto icons = WorldCamera({.mouseHints = k_HintRotate | k_HintTurning,
	                                .gripping = true,
	                                .grippingOnly = true,
	                                .edgeTurning = true,
	                                .features = k_AllFeatures,
	                                .cursor = {-0.5f, 0.0f}},
	                               0.0f);
	EXPECT_EQ(icons.icons, icon::k_Rotate);
	EXPECT_NEAR(icons.rotateAngle, glm::radians(90.0f), 1e-5f);
}

TEST(HandTricons, TheAngleIsKeptWhenNothingTurns)
{
	const auto icons = WorldCamera({.mouseHints = k_HintIdle, .features = k_AllFeatures}, 0.7f);
	EXPECT_EQ(icons.icons, 0u);
	EXPECT_FLOAT_EQ(icons.rotateAngle, 0.7f);
}

TEST(HandTricons, WhatTheScriptsDontAllowShowsNoIcon)
{
	const auto icons = WorldCamera(
	    {.mouseHints = k_HintIdle | k_HintRotate | k_HintPitch | k_HintTop, .features = k_AllFeatures & ~0x01u}, 0.0f);
	EXPECT_EQ(icons.icons, k_RotateOffered);
}

TEST(HandTricons, WatchingAFightOffersNoTilting)
{
	const auto icons = WorldCamera({.mouseHints = k_HintIdle | k_HintPitch, .fight = true, .features = k_AllFeatures}, 0.0f);
	EXPECT_EQ(icons.icons, 0u);
}

TEST(HandTricons, TheClearViewShowsOnlyTheZoomGlass)
{
	const auto icons =
	    WorldCamera({.mouseHints = k_HintIdle | k_HintPitch, .clearView = 0.6f, .features = k_AllFeatures}, 0.0f);
	EXPECT_EQ(icons.icons, icon::k_Zoom);
}

TEST(HandTricons, TheCrossShowsOutOfTheHandsInfluence)
{
	auto icons = WorldCamera({.features = k_AllFeatures, .handInInfluence = false}, 0.0f);
	EXPECT_EQ(icons.icons, icon::k_Cross);
	icons = WorldCamera({.features = k_AllFeatures & ~k_HelpFeature, .handInInfluence = false}, 0.0f);
	EXPECT_EQ(icons.icons, 0u);
}

TEST(HandTricons, HeldBackOnlyTheCrossShows)
{
	const auto icons = WorldCamera({.mouseHints = k_TooFar, .tilt = 1.0f, .features = k_AllFeatures, .blocked = true}, 0.0f);
	EXPECT_EQ(icons.icons, icon::k_Cross);
}

TEST(HandTricons, TheCrossBrightensWithAThingUnderTheHandAndAtOnceWithTheAction)
{
	Fades fades {};
	Fade(fades, {.icons = icon::k_Cross, .seconds = 1.0f, .crossNudge = CrossNudge::OverThing});
	EXPECT_FLOAT_EQ(fades[3], 0.6f);
	Fade(fades, {.icons = icon::k_Cross, .seconds = 0.01f, .crossNudge = CrossNudge::Action});
	EXPECT_FLOAT_EQ(fades[3], 1.0f);
}

TEST(HandTricons, TheIconsAreAPartOfTheFocalLengthAcrossA4By3Screen)
{
	// A 70 degree field of view across
	const auto scale = 1.0f / std::tan(glm::radians(70.0f) * 0.5f);
	// On 4:3 screens and taller ones, the focal length across the screen in pixels over 22, as the original
	EXPECT_FLOAT_EQ(IconHalfSize(scale, {800, 600}), scale * 400.0f / 22.0f);
	EXPECT_FLOAT_EQ(IconHalfSize(scale, {1600, 1200}), scale * 800.0f / 22.0f);
	EXPECT_FLOAT_EQ(IconHalfSize(scale, {1280, 1024}), scale * 640.0f / 22.0f);
	// On wide screens, measured across a 4:3 screen as tall, so they take the same share of the height as on 4:3
	for (const auto screen :
	     std::array {glm::ivec2(1920, 1080), glm::ivec2(2560, 1440), glm::ivec2(3440, 1440), glm::ivec2(5120, 1440)})
	{
		const auto share = IconHalfSize(scale, screen) / static_cast<float>(screen.y);
		EXPECT_NEAR(share, IconHalfSize(scale, {800, 600}) / 600.0f, 1e-6f) << screen.x << "x" << screen.y;
	}
}

TEST(HandTricons, TheDemonstrationsMouseIsSizedForTheDefaultScreen)
{
	// On the original's 800x600 screen, and any 4:3 one as wide, exactly as it was: a 32 pixel mouse and a 21 pixel word
	EXPECT_FLOAT_EQ(DemoScale({800, 600}), 1.0f);
	EXPECT_EQ(DemoLabelSize(false, DemoScale({800, 600})), 21);
	const auto plain = LayoutDemoMouse({400, 300}, false, 50.0f, false, false, false);
	const auto scaledOne = LayoutDemoMouse({400, 300}, false, 50.0f, false, false, false, DemoScale({800, 600}));
	EXPECT_EQ(plain.mouseMin, scaledOne.mouseMin);
	EXPECT_EQ(plain.mouseMax, scaledOne.mouseMax);
	EXPECT_EQ(plain.labelAt, scaledOne.labelAt);
	EXPECT_EQ(plain.mouseMax - plain.mouseMin, glm::vec2(32.0f));
	EXPECT_FLOAT_EQ(plain.shadowOffset, 1.0f);

	// Bigger screens scale it with the width a 4:3 screen as tall would have
	EXPECT_FLOAT_EQ(DemoScale({1600, 1200}), 2.0f);
	EXPECT_FLOAT_EQ(DemoScale({1920, 1080}), 1.8f);
	EXPECT_FLOAT_EQ(DemoScale({2560, 1440}), 2.4f);
	EXPECT_FLOAT_EQ(DemoScale({3440, 1440}), 2.4f);
	EXPECT_FLOAT_EQ(DemoScale({1280, 1024}), 1.6f);
	const auto wide = LayoutDemoMouse({1280, 720}, false, 120.0f, false, false, false, DemoScale({2560, 1440}));
	EXPECT_NEAR((wide.mouseMax - wide.mouseMin).x, 32.0f * 2.4f, 1.0f);
	EXPECT_NEAR(wide.mouseMin.x - 1280.0f, 32.0f * 2.4f, 1.0f);
	EXPECT_EQ(DemoLabelSize(false, 2.4f), 51);
	EXPECT_FLOAT_EQ(wide.shadowOffset, 2.4f);
}
