/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <vector>

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraDrag.h"

using namespace openblack::camera_drag;

namespace
{
/// 4:3 and taller screens, where the edge controls are the game's own
constexpr std::array k_GameShapes {glm::ivec2(640, 480), glm::ivec2(800, 600), glm::ivec2(1024, 768), glm::ivec2(1280, 1024),
                                   glm::ivec2(1600, 1200)};
/// Wide and ultrawide screens, 16:9 to 32:9
constexpr std::array k_WideScreens {glm::ivec2(1920, 1080), glm::ivec2(1920, 1200), glm::ivec2(2560, 1440),
                                    glm::ivec2(3440, 1440), glm::ivec2(3840, 1080), glm::ivec2(5120, 1440)};

/// The game's ring, laid over the whole screen, written out here on its own to hold the code to it: the cursor is put
/// on the ring at nine tenths of the half width and of the picture's half height (unless it is near it), and the
/// camera turns by the angle the cursor swept round the screen's middle
struct GameStep
{
	glm::ivec2 cursor;
	float angle;
};
GameStep GameEdgeTurn(glm::ivec2 cursor, glm::ivec2 previous, glm::ivec2 screen, int view)
{
	const auto width = static_cast<float>(screen.x);
	const auto height = static_cast<float>(screen.y);
	const auto picture = static_cast<float>(view);
	auto x = (static_cast<float>(cursor.x) - width * 0.5f) / (width * 0.5f);
	auto y = (static_cast<float>(cursor.y) - height * 0.5f) / (picture * 0.5f);
	auto radius = y * y + x * x;
	if (radius < 0.89f || radius > 0.91f)
	{
		if (radius != 0.0f)
		{
			radius = 0.9f / std::sqrt(radius);
		}
		y *= radius;
		x *= radius;
		cursor.x = static_cast<int>(((x + 1.0f) * width + 1.0f) * 0.5f);
		cursor.y = static_cast<int>((y * picture + height + 1.0f) * 0.5f);
	}
	const auto before =
	    std::atan2(static_cast<float>(previous.x - screen.x / 2), static_cast<float>(previous.y - screen.y / 2));
	auto angle = std::atan2(static_cast<float>(cursor.x - screen.x / 2), static_cast<float>(cursor.y - screen.y / 2)) - before;
	if (angle > glm::pi<float>())
	{
		angle -= glm::two_pi<float>();
	}
	if (angle < -glm::pi<float>())
	{
		angle += glm::two_pi<float>();
	}
	return {cursor, angle};
}

/// A 4:3 screen as tall as the screen
glm::ivec2 FourByThreeOf(glm::ivec2 screen)
{
	return {screen.y * 4 / 3, screen.y};
}

/// A pixel of the 4:3 screen as tall as a wide one, at the same place against its nearer side of the wide screen
glm::ivec2 OnWideScreen(glm::ivec2 pixel, glm::ivec2 wide)
{
	const auto narrow = FourByThreeOf(wide);
	return pixel.x * 2 < narrow.x ? pixel : glm::ivec2(pixel.x + wide.x - narrow.x, pixel.y);
}

/// The hints with nothing dragged at a pixel
uint32_t HintsAt(glm::ivec2 pixel, const EdgeArea& area, bool land, bool windowed)
{
	return IdleTricons(AreaCursor(pixel, area), land, windowed);
}

/// A place on the ring of the edge controls' area, round from the right as on the 4:3 ring, as a pixel of the screen
glm::ivec2 RingPixel(const EdgeArea& area, float place)
{
	const auto across = 0.45f * static_cast<float>(area.width) * std::cos(place);
	const auto down = 0.45f * static_cast<float>(area.viewHeight) * std::sin(place);
	const auto side = std::abs(across) < 0.5f ? 0.0f : (across > 0.0f ? 1.0f : -1.0f);
	return glm::ivec2(glm::round(
	    glm::vec2(static_cast<float>(area.screen.x) * 0.5f + across + side * static_cast<float>(area.Folded()) * 0.5f,
	              static_cast<float>(area.screen.y / 2) + down)));
}

/// The camera's turn, in degrees, for each pixel the mouse moves from a place on the ring
float DegreesPerPixel(const EdgeArea& area, float place, glm::ivec2 pixels)
{
	const auto at = RingPixel(area, place);
	const auto start = EdgeRotate(at, at, area);
	const auto step = EdgeRotate(start.cursor + pixels, start.cursor, area);
	const auto moved = std::sqrt(static_cast<float>(pixels.x * pixels.x + pixels.y * pixels.y));
	return std::abs(glm::degrees(step.turn)) / moved;
}

/// Places round the ring: the sides, the top and bottom, and the four corners between
constexpr std::array k_Places {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f};
/// Mouse movements along the screen's axes, as a hand moves the mouse down the side or across the bottom
constexpr std::array k_Moves {glm::ivec2(8, 0), glm::ivec2(-8, 0), glm::ivec2(0, 8), glm::ivec2(0, -8)};
} // namespace

TEST(EdgeArea, IsTheWholeScreenOnA4By3OrTallerScreen)
{
	for (const auto screen : k_GameShapes)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto area = EdgeAreaOf(screen, false);
		EXPECT_EQ(area, WholeScreenArea(screen, screen.y));
		EXPECT_EQ(area.Folded(), 0);
		EXPECT_EQ(EdgeAreaOf(screen, true), WholeScreenArea(screen, ViewHeight(screen, true)));
	}
}

TEST(EdgeArea, IsA4By3ScreenAsTallOnAWiderScreen)
{
	EXPECT_EQ(EdgeAreaOf({1920, 1080}, false).width, 1440);
	EXPECT_EQ(EdgeAreaOf({2560, 1440}, false).width, 1920);
	EXPECT_EQ(EdgeAreaOf({3440, 1440}, false).width, 1920);
	EXPECT_EQ(EdgeAreaOf({3840, 1080}, false).Folded(), 2400);
	EXPECT_EQ(EdgeAreaOf({5120, 1440}, false).Folded(), 3200);
	// On screens 16:9 or wider the cinema bars take nothing, so the picture stays the screen's height
	EXPECT_EQ(EdgeAreaOf({5120, 1440}, true).viewHeight, 1440);
	EXPECT_EQ(ViewHeight({5120, 1440}, true), 1440);
	// Between 4:3 and 16:9 they take what a 16:9 picture leaves
	EXPECT_EQ(EdgeAreaOf({1920, 1200}, true).viewHeight, 1080);
}

TEST(EdgeArea, OnA4By3OrTallerScreenTheEdgeControlsAreExactlyTheGames)
{
	for (const auto screen : k_GameShapes)
	{
		for (const bool bars : {false, true})
		{
			SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y << (bars ? " with bars" : ""));
			const auto area = EdgeAreaOf(screen, bars);
			const std::array previous {glm::ivec2(screen.x * 19 / 20, screen.y / 2), glm::ivec2(screen.x / 2, screen.y - 3),
			                           glm::ivec2(screen.x / 10, screen.y / 10)};
			for (int y = 0; y < screen.y; y += 23)
			{
				for (int x = 0; x < screen.x; x += 29)
				{
					const glm::ivec2 cursor {x, y};
					EXPECT_EQ(AreaCursor(cursor, area), NormalisedCursor(cursor, screen, area.viewHeight));
					for (const auto from : previous)
					{
						const auto game = GameEdgeTurn(cursor, from, screen, area.viewHeight);
						const auto step = EdgeRotate(cursor, from, area);
						ASSERT_EQ(step.cursor, game.cursor) << x << "," << y;
						ASSERT_EQ(step.angle, game.angle) << x << "," << y;
						// The turn is the game's angle scaled to the default screen (deviation 10), exactly the angle there
						ASSERT_EQ(step.turn, game.angle * ReferenceTurnScale(from, game.cursor, screen, area.viewHeight));
						if (screen == k_ReferenceScreen)
						{
							ASSERT_EQ(step.turn, game.angle);
						}
					}
				}
			}
		}
	}
}

TEST(EdgeArea, TheSidesAndCornersOfAWideScreenOfferWhatA4By3ScreenAsTallOffers)
{
	for (const auto screen : k_WideScreens)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto narrow = FourByThreeOf(screen);
		const auto wideArea = EdgeAreaOf(screen, false);
		const auto narrowArea = EdgeAreaOf(narrow, false);
		for (const bool land : {false, true})
		{
			for (const bool windowed : {false, true})
			{
				for (int y = 0; y < screen.y; y += 7)
				{
					for (int x = 0; x < narrow.x; x += 5)
					{
						const glm::ivec2 pixel {x, y};
						ASSERT_EQ(HintsAt(OnWideScreen(pixel, screen), wideArea, land, windowed),
						          HintsAt(pixel, narrowArea, land, windowed))
						    << x << "," << y;
					}
					// The strip between offers what the middle of the 4:3 screen does
					const auto middle = HintsAt({narrow.x / 2, y}, narrowArea, land, windowed);
					for (int x = narrow.x / 2 + 1; x < screen.x - narrow.x / 2; x += 37)
					{
						ASSERT_EQ(HintsAt({x, y}, wideArea, land, windowed), middle) << x << "," << y;
					}
				}
			}
		}
		// Pushed against the right side the cursor turns, as far in as on the 4:3 screen: 5% of its width
		const auto band = narrow.x / 20;
		EXPECT_NE(HintsAt({screen.x - band + 2, screen.y / 2}, wideArea, true, false) & tricon::k_Rotate, 0U);
		EXPECT_EQ(HintsAt({screen.x - band - 2, screen.y / 2}, wideArea, true, false), tricon::k_Idle);
		// The corner turns and, at the very bottom, tilts
		EXPECT_EQ(HintsAt({screen.x - 1, screen.y - 1}, wideArea, true, false),
		          tricon::k_Idle | tricon::k_Rotate | tricon::k_Pitch | tricon::k_Turning);
	}
}

TEST(EdgeArea, ADragIsDecidedAsOnA4By3ScreenAsTall)
{
	for (const auto screen : k_WideScreens)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto wideArea = EdgeAreaOf(screen, false);
		const auto narrowArea = EdgeAreaOf(FourByThreeOf(screen), false);
		const auto side = AreaCursor({screen.x - 5, screen.y / 2}, wideArea);
		EXPECT_EQ(side, AreaCursor({narrowArea.width - 5, screen.y / 2}, narrowArea));
		// Down the side, a slow drag is decided once it has moved 2% of the 4:3 width down: 38 pixels at 1440 high
		// rather than 102 on a 32:9 screen's own width
		const auto needed = narrowArea.width / 50 + 1;
		DragClassifier drag;
		drag.Start(tricon::k_Idle | tricon::k_Rotate | tricon::k_Turning, side, 1000);
		drag.Move({0, needed - 2}, wideArea, 1400, true);
		EXPECT_FALSE(drag.GetMode().has_value());
		drag.Move({0, 2}, wideArea, 1410, true);
		EXPECT_EQ(drag.GetMode(), DragMode::EdgeRotate);
	}
}

TEST(EdgeArea, TheRingOfAWideScreenIsThe4By3RingAgainstItsSides)
{
	for (const auto screen : k_WideScreens)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto narrow = FourByThreeOf(screen);
		const auto area = EdgeAreaOf(screen, false);
		// The right half of the 4:3 screen against the right side, the left half against the left
		for (const auto from : {glm::ivec2(narrow.x * 19 / 20, narrow.y / 2), glm::ivec2(narrow.x / 20, narrow.y / 3)})
		{
			for (int y = 0; y < screen.y; y += 31)
			{
				for (int x = from.x > narrow.x / 2 ? narrow.x / 2 + 40 : 0;
				     x < (from.x > narrow.x / 2 ? narrow.x : narrow.x / 2 - 40); x += 17)
				{
					const auto game = GameEdgeTurn({x, y}, from, narrow, narrow.y);
					const auto step = EdgeRotate(OnWideScreen({x, y}, screen), OnWideScreen(from, screen), area);
					const auto expected = OnWideScreen(game.cursor, screen);
					ASSERT_LE(std::abs(step.cursor.x - expected.x), 1) << x << "," << y;
					ASSERT_EQ(step.cursor.y, expected.y) << x << "," << y;
					ASSERT_NEAR(step.angle, game.angle, 2e-3f) << x << "," << y;
				}
			}
		}
		// A press in the bottom right corner goes onto the ring as far in from the corner as on the 4:3 screen
		const auto corner = EdgeRotate({screen.x - 1, screen.y - 1}, {screen.x - 1, screen.y - 1}, area);
		const auto narrowCorner = GameEdgeTurn({narrow.x - 1, narrow.y - 1}, {narrow.x - 1, narrow.y - 1}, narrow, narrow.y);
		EXPECT_LE(std::abs((screen.x - corner.cursor.x) - (narrow.x - narrowCorner.cursor.x)), 1);
		EXPECT_EQ(corner.cursor.y, narrowCorner.cursor.y);
		EXPECT_NEAR(corner.angle, 0.0f, 1e-3f);
	}
}

TEST(EdgeArea, TheRingRunsStraightAcrossTheMiddleOfAWideScreen)
{
	for (const auto screen : k_WideScreens)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto area = EdgeAreaOf(screen, false);
		const auto endRadius = 0.45f * static_cast<float>(screen.y);
		const glm::ivec2 bottom {screen.x / 2, static_cast<int>(static_cast<float>(screen.y) * 0.5f + endRadius + 0.5f)};
		// Over the strip the cursor only goes up or down onto the straight part
		const auto down = EdgeRotate({screen.x / 2 + 100, screen.y - 1}, bottom, area);
		EXPECT_EQ(down.cursor.x, screen.x / 2 + 100);
		EXPECT_NEAR(static_cast<float>(down.cursor.y), static_cast<float>(screen.y) * 0.5f + endRadius, 1.0f);
		// Along it, each pixel across turns as a pixel across the bottom of the 4:3 ring, one over its radius
		EXPECT_NEAR(down.angle, 100.0f / endRadius, 1e-4f);
		const glm::ivec2 top {screen.x / 2, screen.y / 2 - static_cast<int>(endRadius)};
		const auto up = EdgeRotate(top + glm::ivec2(-60, 0), top, area);
		EXPECT_NEAR(up.angle, 60.0f / endRadius, 1e-4f);
	}
}

TEST(EdgeArea, GoingRightRoundAWideScreensRingSweepsAWholeTurnAndTheTwoStraightRuns)
{
	for (const auto screen : k_WideScreens)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto area = EdgeAreaOf(screen, false);
		// Round the screen just inside its edges, clockwise, a few pixels at a time
		std::vector<glm::ivec2> path;
		const int inset = 4;
		for (int x = inset; x < screen.x - inset; x += 6)
		{
			path.emplace_back(x, inset);
		}
		for (int y = inset; y < screen.y - inset; y += 6)
		{
			path.emplace_back(screen.x - inset, y);
		}
		for (int x = screen.x - inset; x > inset; x -= 6)
		{
			path.emplace_back(x, screen.y - inset);
		}
		for (int y = screen.y - inset; y > inset; y -= 6)
		{
			path.emplace_back(inset, y);
		}
		path.push_back(path.front());
		auto ring = EdgeRotate(path.front(), path.front(), area).cursor;
		float swept = 0.0f;
		float turned = 0.0f;
		for (const auto point : path)
		{
			const auto step = EdgeRotate(point, ring, area);
			swept += step.angle;
			turned += step.turn;
			ring = step.cursor;
		}
		const auto straight = 2.0f * static_cast<float>(area.Folded()) / (0.45f * static_cast<float>(screen.y));
		EXPECT_NEAR(std::abs(swept), glm::two_pi<float>() + straight, 1e-3f);
		// Turned by the default screen's measure: a whole turn and the straight runs at its 270 pixels' radius, scaled
		// from this ring's
		const auto scale = static_cast<float>(screen.y) / 600.0f;
		EXPECT_NEAR(std::abs(turned), glm::two_pi<float>() * scale + straight * scale, 0.02f * std::abs(turned));
	}
}

TEST(EdgeArea, EveryWhereOnTheRingAMouseMovementTurnsAsOnThe800By600Screen)
{
	const auto reference = EdgeAreaOf(k_ReferenceScreen, false);
	for (const auto screen : k_WideScreens)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto area = EdgeAreaOf(screen, false);
		for (const auto place : k_Places)
		{
			for (const auto move : k_Moves)
			{
				SCOPED_TRACE(testing::Message() << "place " << place << " pi, move " << move.x << "," << move.y);
				const auto expected = DegreesPerPixel(reference, place * glm::pi<float>(), move);
				const auto got = DegreesPerPixel(area, place * glm::pi<float>(), move);
				EXPECT_NEAR(got, expected, 0.08f * expected + 0.01f);
			}
		}
		// Across the straight top and bottom, a pixel turns 1/270 of a radian, as across the top and bottom at 800x600
		const auto ends = glm::degrees(1.0f / 270.0f);
		const glm::ivec2 bottom {screen.x / 2 - area.Folded() / 4, static_cast<int>(static_cast<float>(screen.y) * 0.95f)};
		const auto start = EdgeRotate(bottom, bottom, area);
		const auto step = EdgeRotate(start.cursor + glm::ivec2(8, 0), start.cursor, area);
		EXPECT_NEAR(glm::degrees(step.turn) / 8.0f, ends, ends * 0.01f);
	}
	// On 4:3 and taller screens the sides and the top and bottom turn as at 800x600 (deviation 10)
	const float sides = glm::degrees(1.0f / 360.0f);
	const float ends = glm::degrees(1.0f / 270.0f);
	for (const auto screen : k_GameShapes)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto area = EdgeAreaOf(screen, false);
		EXPECT_NEAR(DegreesPerPixel(area, 0.0f, {0, 8}), sides, sides * 0.02f);
		EXPECT_NEAR(DegreesPerPixel(area, glm::pi<float>(), {0, -8}), sides, sides * 0.02f);
		EXPECT_NEAR(DegreesPerPixel(area, glm::half_pi<float>(), {-8, 0}), ends, ends * 0.02f);
		EXPECT_NEAR(DegreesPerPixel(area, -glm::half_pi<float>(), {8, 0}), ends, ends * 0.02f);
	}
}

TEST(EdgeArea, TheTurnCarriesOnSmoothlyRoundTheCornerOntoTheStraight)
{
	for (const auto screen : k_WideScreens)
	{
		SCOPED_TRACE(testing::Message() << screen.x << "x" << screen.y);
		const auto area = EdgeAreaOf(screen, false);
		const auto endRadius = 0.45f * static_cast<float>(screen.y);
		// From low on the right side, down and left round the corner, then left along the bottom, a few pixels a step
		auto ring = EdgeRotate({screen.x - 5, screen.y * 3 / 4}, {screen.x - 5, screen.y * 3 / 4}, area).cursor;
		float previousTurn = 0.0f;
		for (int stepIndex = 0; stepIndex < 200 || ring.x > screen.x / 2; ++stepIndex)
		{
			const auto move = stepIndex < 200 ? glm::ivec2(-3, 3) : glm::ivec2(-4, 0);
			const auto step = EdgeRotate(ring + move, ring, area);
			// Always the same way round, never a jump: at most what four pixels give at the 4:3 ring's nearest point
			ASSERT_LE(step.angle, 0.0f) << stepIndex;
			ASSERT_LT(std::abs(step.angle), 4.5f / endRadius) << stepIndex;
			previousTurn = step.turn;
			ring = step.cursor;
		}
		// It has come round onto the straight bottom, to the middle of the screen
		EXPECT_NEAR(static_cast<float>(ring.y), static_cast<float>(screen.y) * 0.5f + endRadius, 1.0f);
		EXPECT_LT(previousTurn, 0.0f);
	}
}
