/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Gestures/GestureHints.h"

using namespace openblack;
using namespace openblack::gesture_hints;

namespace
{
constexpr float k_Tolerance = 1e-4f;

gesture::Request Pick(GestureType gesture, LeashType leash)
{
	return {.gesture = gesture, .purpose = gesture::Purpose::PickLeash, .leash = leash};
}

// A circle 4 long and every other gesture 6 long, each a straight line across its square
std::optional<Shape> FakeShapes(GestureType gesture)
{
	const float length = gesture == GestureType::Circle ? 4.0f : 6.0f;
	return Shape {.points = {{0.0f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.5f}}, .length = length};
}
} // namespace

TEST(GestureHints, OnlyTheLeashesThePickerOffersAreHinted)
{
	const std::vector<gesture::Request> requests {
	    {.gesture = GestureType::Scribble, .purpose = gesture::Purpose::ClosePicker},
	    Pick(GestureType::VerticalScribble, LeashType::Evil),
	    Pick(GestureType::Heart, LeashType::Good),
	};
	const auto hints = HintsFor(requests, FakeShapes);
	ASSERT_EQ(hints.size(), 2u);
	// The higher leash takes the square nearest the right edge
	EXPECT_EQ(hints[0].leash, LeashType::Good);
	EXPECT_EQ(hints[0].order, 6);
	EXPECT_EQ(hints[1].leash, LeashType::Evil);
	EXPECT_EQ(hints[1].order, 4);
}

TEST(GestureHints, TheDotRunsAlongAGestureInTimeWithItsLengthAgainstTheCircles)
{
	const std::vector<gesture::Request> requests {Pick(GestureType::Heart, LeashType::Good)};
	const auto hints = HintsFor(requests, FakeShapes);
	ASSERT_EQ(hints.size(), 1u);
	EXPECT_NEAR(hints[0].period, k_CirclePeriod * 6.0f / 4.0f, k_Tolerance);
}

TEST(GestureHints, AGestureWithoutAShapeTakesTheCircles)
{
	const auto onlyCircle = [](GestureType gesture) -> std::optional<Shape> {
		return gesture == GestureType::Circle ? FakeShapes(gesture) : std::nullopt;
	};
	const std::vector<gesture::Request> requests {Pick(GestureType::EShape, LeashType::Rope)};
	const auto hints = HintsFor(requests, onlyCircle);
	ASSERT_EQ(hints.size(), 1u);
	EXPECT_EQ(hints[0].path.size(), 2u);
	EXPECT_NEAR(hints[0].period, k_CirclePeriod, k_Tolerance);
}

TEST(GestureHints, TheRowIsReplacedOnlyWhenItsHintsChange)
{
	const auto a = HintsFor(std::vector {Pick(GestureType::Heart, LeashType::Good)}, nullptr);
	auto shown = HintsFor(std::vector {Pick(GestureType::Heart, LeashType::Good)}, FakeShapes);
	shown[0].dot = 0.5f;
	EXPECT_FALSE(Differs(shown, a));
	const auto b = HintsFor(std::vector {Pick(GestureType::EShape, LeashType::Rope)}, nullptr);
	EXPECT_TRUE(Differs(shown, b));
	EXPECT_TRUE(Differs(shown, {}));
}

TEST(GestureHints, TheSquaresRunLeftFromTheBottomRightCornerByShareOfTheWidth)
{
	const glm::vec2 screen {800.0f, 600.0f};
	const auto first = Square(0, screen);
	EXPECT_NEAR(first.max.x, 800.0f - 12.0f, k_Tolerance);
	EXPECT_NEAR(first.min.x, 800.0f - 12.0f - 28.0f, k_Tolerance);
	EXPECT_NEAR(first.min.y, 600.0f - 28.0f, k_Tolerance);
	EXPECT_NEAR(first.max.y, 600.0f, k_Tolerance);
	const auto second = Square(1, screen);
	EXPECT_NEAR(first.min.x - second.max.x, 4.0f, k_Tolerance);
	const auto picture = PictureSquare(first);
	EXPECT_EQ(picture.max.y, first.min.y);
	EXPECT_NEAR(picture.min.y, first.min.y - 28.0f, k_Tolerance);
}

TEST(GestureHints, OnAWideScreenTheRowIsSizedAsOnA4By3ScreenAsTall)
{
	const auto wide = Square(0, {2560.0f, 1080.0f});
	const auto square = Square(0, {1440.0f, 1080.0f});
	EXPECT_NEAR(wide.max.x - wide.min.x, square.max.x - square.min.x, 1e-3f);
	EXPECT_NEAR(2560.0f - wide.max.x, 1440.0f - square.max.x, 1e-3f);
}

TEST(GestureHints, ThePicturesAreSizedByTheCamerasFieldOfView)
{
	// A 70 degree field of view across a 4:3 screen 800 wide
	const float fov = glm::radians(70.0f);
	EXPECT_NEAR(SpriteSide(k_PictureHalfSize, 800.0f, fov), 0.02f / std::tan(fov / 2.0f) * 800.0f, k_Tolerance);
	EXPECT_NEAR(SpriteSide(k_DotHalfSize, 800.0f, fov), SpriteSide(k_PictureHalfSize, 800.0f, fov) / 2.0f, k_Tolerance);
}

TEST(GestureHints, TheRowFadesOutWhileTheAdvisorsSpeakAndBackAfter)
{
	EXPECT_NEAR(Fade(1.0f, 0.25f, true), 0.5f, k_Tolerance);
	EXPECT_EQ(Fade(0.2f, 0.25f, true), 0.0f);
	EXPECT_NEAR(Fade(0.0f, 0.25f, false), 0.5f, k_Tolerance);
	EXPECT_EQ(Fade(0.9f, 0.25f, false), 1.0f);
	EXPECT_EQ(Fade(1.0f, 0.25f, false), 1.0f);
}

TEST(GestureHints, TheDotWrapsBackOverlappingTheStart)
{
	EXPECT_NEAR(StepDot(0.0f, 0.5f, 2.0f), 0.25f, k_Tolerance);
	EXPECT_NEAR(StepDot(0.95f, 0.2f, 2.0f), 0.15f, k_Tolerance);
	EXPECT_EQ(StepDot(0.3f, 1.0f, 0.0f), 0.3f);
	EXPECT_EQ(DotsAt(0.5f).size(), 1u);
	const auto ends = DotsAt(0.95f);
	ASSERT_EQ(ends.size(), 2u);
	EXPECT_NEAR(ends[1], 0.05f, k_Tolerance);
}

TEST(GestureHints, TheDotIsBrightestMidGesture)
{
	EXPECT_NEAR(DotBrightness(0.5f), 1.0f, k_Tolerance);
	EXPECT_NEAR(DotBrightness(0.0f), 0.0f, k_Tolerance);
	EXPECT_NEAR(DotBrightness(0.25f), 0.75f, k_Tolerance);
	EXPECT_EQ(DotShade(0.5f, 1.0f), 254);
	EXPECT_EQ(DotShade(0.0f, 1.0f), 0);
}

TEST(GestureHints, ThePictureTakesThePlayersColourDimmedWithTheRow)
{
	const auto full = PictureColour(0xFF4646u, 1.0f);
	EXPECT_EQ(full[0], 254);
	EXPECT_EQ(full[1], 69);
	EXPECT_EQ(full[3], 254);
	const auto half = PictureColour(0xFF4646u, 0.5f);
	EXPECT_EQ(half[0], (255 * 127) >> 8);
	EXPECT_EQ(half[3], (255 * 127) >> 8);
}

TEST(GestureHints, PointsAlongAPathAreBetweenItsPoints)
{
	const std::vector<glm::vec3> path {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}};
	EXPECT_NEAR(PointAlong(path, 0.25f).x, 0.5f, k_Tolerance);
	EXPECT_NEAR(PointAlong(path, 0.75f).z, 0.5f, k_Tolerance);
	EXPECT_EQ(PointAlong(path, -1.0f), path.front());
	EXPECT_EQ(PointAlong(path, 1.0f), path.back());
	EXPECT_EQ(PixelOf({.min = {10.0f, 20.0f}, .max = {38.0f, 48.0f}}, {0.5f, 0.5f}), glm::vec2(24.0f, 34.0f));
}

TEST(GestureHints, CollarsTumbleKeepingTheirAxesSquare)
{
	Axes axes;
	for (int i = 0; i < 100; ++i)
	{
		axes = Tumble(axes, LeashType::Good, 0.05f);
	}
	EXPECT_NEAR(glm::length(axes.x), 1.0f, 1e-3f);
	EXPECT_NEAR(glm::dot(axes.x, axes.y), 0.0f, 1e-3f);
	EXPECT_NEAR(glm::dot(axes.y, axes.z), 0.0f, 1e-3f);
	EXPECT_NE(axes, Axes {});
	// No time, no turn
	EXPECT_EQ(Tumble(Axes {}, LeashType::Evil, 0.0f), Axes {});
	// Each leash turns at its own pace
	EXPECT_NE(Tumble(Axes {}, LeashType::Evil, 0.5f), Tumble(Axes {}, LeashType::Good, 0.5f));
}

TEST(GestureHints, TheRowStopsWhileFadedOut)
{
	Row row;
	row.hints = HintsFor(std::vector {Pick(GestureType::Heart, LeashType::Good)}, FakeShapes);
	Step(row, 0.1f, false);
	EXPECT_GT(row.hints[0].dot, 0.0f);
	EXPECT_NE(row.collars.at(3), Axes {});
	row.alpha = 0.0f;
	const auto dot = row.hints[0].dot;
	Step(row, 0.1f, true);
	EXPECT_EQ(row.hints[0].dot, dot);
}

TEST(GestureHints, TheCollarsCameraLooksAtItInTheMiddleOfItsSquare)
{
	const PixelRect rect {.x = 700, .y = 500, .width = 28, .height = 28};
	const glm::vec2 centre {714.0f, 514.0f};
	const auto clip = CollarProjection(rect, centre, 571.0f, 1.0f) * CollarView() * glm::vec4(k_CollarAt, 1.0f);
	EXPECT_NEAR(clip.x / clip.w, 0.0f, k_Tolerance);
	EXPECT_NEAR(clip.y / clip.w, 0.0f, k_Tolerance);
	// A point a pixel's worth to the right at the collar's distance lands a pixel to the right
	const float distance = glm::length(k_CollarAt - k_CollarEye);
	const auto right = glm::vec4(k_CollarAt + glm::vec3(distance / 571.0f, 0.0f, 0.0f), 1.0f);
	const auto moved = CollarProjection(rect, centre, 571.0f, 1.0f) * CollarView() * right;
	EXPECT_NEAR(moved.x / moved.w * 14.0f, 1.0f, 1e-2f);
	EXPECT_EQ(PixelsOf({.min = {700.4f, 499.6f}, .max = {728.4f, 527.6f}}), rect);
}
