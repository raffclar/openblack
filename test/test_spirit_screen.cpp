/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The advisors on screens of any shape (src/Help/SpiritScreen.h): they use the whole screen, measured in half the
// width of a 4:3 screen of its height, so that through the camera (which keeps a 4:3 screen's view up and down on
// wider screens) they look as on a 4:3 screen while their clings, homes and script points reach the real sides.
// Screens 4:3 or taller are measured as the game measures them.

#include <cmath>

#include <algorithm>
#include <array>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraLens.h"
#include "Help/SpiritScreen.h"
#include "Help/SpiritView.h"
#include "Help/Spirits.h"

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
/// The game's lens for the world, 70 degrees across a 4:3 screen
constexpr float k_Lens = 1.2217305f;
constexpr float k_NearClip = 3.5f;

struct ScreenCase
{
	Screen screen;
	int32_t unit;
	int32_t middleLeft;
};

constexpr std::array<ScreenCase, 9> k_Screens {
    ScreenCase {{640, 480}, 320, 0},     ScreenCase {{1024, 768}, 512, 0},    ScreenCase {{1600, 1200}, 800, 0},
    ScreenCase {{1280, 1024}, 640, 0},   ScreenCase {{1280, 720}, 480, 160},  ScreenCase {{1920, 1080}, 720, 240},
    ScreenCase {{2560, 1440}, 960, 320}, ScreenCase {{3440, 1440}, 960, 760}, ScreenCase {{5120, 1440}, 960, 1600},
};

/// The five screens every screen-facing change is checked at
constexpr std::array<Screen, 5> k_CheckScreens {Screen {1280, 1024}, Screen {1920, 1080}, Screen {2560, 1440},
                                                Screen {3440, 1440}, Screen {5120, 1440}};

/// A camera over the land looking down at it, as the game's camera makes its view on a screen of this shape
SpiritView MakeView(Screen screen)
{
	const float aspect = static_cast<float>(screen.width) / static_cast<float>(screen.height);
	const glm::mat4 projection =
	    glm::perspective(camera_lens::VerticalFieldOfView(k_Lens, aspect), aspect, k_NearClip, 65536.0f);
	const glm::vec3 eye(1200.0f, 140.0f, 900.0f);
	const glm::mat4 viewMatrix = glm::lookAt(eye, glm::vec3(1260.0f, 60.0f, 1010.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 toWorld = glm::inverse(viewMatrix);
	SpiritView view;
	view.eye = glm::vec3(toWorld[3]);
	view.right = glm::vec3(toWorld[0]);
	view.up = glm::vec3(toWorld[1]);
	view.forward = glm::vec3(toWorld[2]) * projection[2][3];
	view.halfExtent = {1.0f / projection[0][0], 1.0f / projection[1][1]};
	view.nearClip = k_NearClip;
	view.viewProjection = projection * viewMatrix;
	view.screen = screen;
	return view;
}

/// The 4:3 screen of a screen's height
Screen FourByThreeOf(Screen screen)
{
	return {.width = static_cast<uint16_t>(std::min<int32_t>(screen.width, screen.HoverUnit() * 2)), .height = screen.height};
}

/// A hover point's pixel on a screen
glm::vec2 PixelOfHover(const Screen& screen, glm::vec2 hover)
{
	const auto unit = static_cast<float>(screen.HoverUnit());
	return {hover.x * unit + static_cast<float>(screen.HalfWidth()), hover.y * unit + static_cast<float>(screen.HalfHeight())};
}

struct Controller
{
	SpiritView view;
	// The advisors keep references to their data
	DudeData good {MakeData()};
	DudeData evil {MakeData()};
	AdvisorSpiritController control;

	explicit Controller(Screen screen)
	    : view(MakeView(screen))
	    , control(good, evil, MakeQueries(), screen)
	{
	}

	static DudeData MakeData()
	{
		DudeData data;
		data.nearDepth = 8.733334f;
		data.farDepth = 7.232f;
		data.scale = 0.02552f;
		data.modelSize = 63.42295f;
		return data;
	}

	Queries MakeQueries()
	{
		Queries q;
		q.localRand = [](int32_t) { return 0u; };
		q.localFloatRand = [](float) { return 0.0f; };
		q.random = [](float a, float) { return a; };
		q.nearClip = [this]() { return view.nearClip; };
		q.pointFromScreen = [this](glm::vec2 pixel, float depth) { return PointFromScreen(view, pixel, depth); };
		q.worldToPixel = [this](const glm::vec3& point, bool force) { return WorldToPixel(view, point, force); };
		q.projectPoint = [this](const glm::vec3& point) { return ProjectPoint(view, point); };
		q.cameraAxes = [this]() { return glm::mat3(view.right, view.up, view.forward); };
		q.cameraPosition = [this]() { return view.eye; };
		return q;
	}

	/// Runs frames of a tenth of a second, the pointer resting in the middle of the screen
	void Frames(int count)
	{
		for (int i = 0; i < count; ++i)
		{
			Frame();
		}
	}

	void Frame()
	{
		FrameInput input;
		input.dt = 0.1f;
		input.frameMs = 100;
		input.screen = view.screen;
		input.mouse = {view.screen.HalfWidth(), view.screen.HalfHeight()};
		tickMs += 100;
		input.tickMs = tickMs;
		control.Update(input);
	}

	uint32_t tickMs {0};
};

constexpr int32_t k_Good = 1;
constexpr int32_t k_Evil = 2;
} // namespace

TEST(SpiritScreen, TheHoverUnitIsHalfThe4By3WidthOfTheScreensHeight)
{
	for (const auto& c : k_Screens)
	{
		SCOPED_TRACE(c.screen.width);
		EXPECT_EQ(c.screen.HoverUnit(), c.unit);
		EXPECT_EQ(c.screen.MiddleLeft(), c.middleLeft);
		EXPECT_FLOAT_EQ(c.screen.Reach(), static_cast<float>(c.screen.HalfWidth()) / static_cast<float>(c.unit));
		if (c.middleLeft == 0)
		{
			// The game's own measure, exactly
			EXPECT_EQ(c.screen.HoverUnit(), c.screen.HalfWidth());
			EXPECT_EQ(c.screen.Reach(), 1.0f);
		}
	}
}

TEST(SpiritScreen, ItsMiddleIsWhatA4By3ScreenOfTheSameHeightShows)
{
	// A hover point lands on the same place in the world as on the 4:3 screen of the same height, so the advisors'
	// size, perspective, depth (and so their distance from the near plane) and light are that screen's
	for (const auto& c : k_Screens)
	{
		SCOPED_TRACE(c.screen.width);
		const SpiritView wide = MakeView(c.screen);
		const Screen narrowScreen = FourByThreeOf(c.screen);
		const SpiritView narrow = MakeView(narrowScreen);
		for (const glm::vec2 hover : {glm::vec2(0.0f, 0.0f), glm::vec2(0.66f, 0.1f), glm::vec2(-0.9f, 0.7f)})
		{
			for (const float depth : {8.733334f, 7.232f, -10.0f})
			{
				const glm::vec3 point = PointFromScreen(wide, PixelOfHover(c.screen, hover), depth);
				const glm::vec3 point43 = PointFromScreen(narrow, PixelOfHover(narrowScreen, hover), depth);
				EXPECT_NEAR(glm::distance(point, point43), 0.0f, 2e-3f);
			}
		}
		// The near light, at the left edge of the screen's 4:3 middle, is where the 4:3 screen has it
		const glm::vec2 light(static_cast<float>(c.screen.MiddleLeft()), static_cast<float>(c.screen.HalfHeight()));
		const glm::vec2 light43(0.0f, static_cast<float>(narrowScreen.HalfHeight()));
		EXPECT_NEAR(glm::distance(PointFromScreen(wide, light, -10.0f), PointFromScreen(narrow, light43, -10.0f)), 0.0f, 2e-3f);
	}
}

TEST(SpiritScreen, AnAdvisorIsTheSameShareOfTheScreensHeightEverywhereOnIt)
{
	// The evil advisor's height is ~13.2% of the width of the screen's 4:3 middle (~17.6% of a wider screen's height),
	// in the middle and out at the real screen's sides
	for (const auto& c : k_Screens)
	{
		SCOPED_TRACE(c.screen.width);
		const SpiritView view = MakeView(c.screen);
		const float reach = c.screen.Reach();
		for (const float across : {0.0f, reach - 0.3f, -(reach - 0.3f)})
		{
			const glm::vec3 root = PointFromScreen(view, PixelOfHover(c.screen, {across, 0.0f}), 8.733334f);
			const float height = 0.0268267f * 79.15389f * 0.761f;
			const auto pixel = [&view](const glm::vec3& point) {
				return WorldToPixel(view, point, true).value_or(glm::vec2(0.0f));
			};
			const float drawn = pixel(root).y - pixel(root + view.up * height).y;
			// (on a screen taller than 4:3 the game's share of its width)
			EXPECT_NEAR(drawn / static_cast<float>(c.screen.HoverUnit() * 2), 0.132f, 0.002f);
		}
	}
}

TEST(SpiritScreen, SideClingsHoldOnToTheRealScreensSides)
{
	for (const auto& screen : k_CheckScreens)
	{
		SCOPED_TRACE(screen.width);
		Controller c(screen);
		// "cling across 1.0 down 0.5" and "across 0.0 down 0.4"
		c.control.SpiritCling(k_Good, 1.0f, 0.5f);
		c.control.SpiritCling(k_Evil, 0.0f, 0.4f);
		const float unit = static_cast<float>(screen.HoverUnit());
		const auto& good = c.control.Dude(0);
		const auto& evil = c.control.Dude(1);
		EXPECT_EQ(good.ClingEdge(), Edge::Right);
		EXPECT_EQ(evil.ClingEdge(), Edge::Left);
		// As far past the side as on 4:3: 0.04 of a hover unit
		EXPECT_NEAR(PixelOfHover(screen, good.ClingTarget()).x, static_cast<float>(screen.width) + 0.04f * unit, 1.0f);
		EXPECT_NEAR(PixelOfHover(screen, evil.ClingTarget()).x, -0.04f * unit, 1.0f);
		// and the bottom hold at the same row as on 4:3, its place across the real width's share
		c.control.SpiritCling(k_Good, 0.4f, 1.0f);
		EXPECT_EQ(good.ClingEdge(), Edge::Bottom);
		EXPECT_NEAR(PixelOfHover(screen, good.ClingTarget()).x, 0.4f * static_cast<float>(screen.width), 1.0f);
		EXPECT_NEAR(PixelOfHover(screen, good.ClingTarget()).y, static_cast<float>(screen.HalfHeight()) + 0.78f * unit, 1.0f);
	}
}

TEST(SpiritScreen, ScriptPointsAreSharesOfTheRealScreen)
{
	for (const auto& screen : k_CheckScreens)
	{
		SCOPED_TRACE(screen.width);
		Controller c(screen);
		c.control.SpiritEject(k_Evil, false);
		c.Frames(2);
		c.control.SpiritFly(k_Evil, 0.8f, 0.3f);
		const auto& evil = c.control.Dude(1);
		const glm::vec2 target(evil.HoverX().GetDestination(), evil.HoverY().GetDestination());
		const glm::vec2 pixel = PixelOfHover(screen, target);
		EXPECT_NEAR(pixel.x, 0.8f * static_cast<float>(screen.width), 1.0f);
		EXPECT_NEAR(pixel.y, 0.3f * static_cast<float>(screen.height), 1.0f);
		// A target further out is kept as far in from the side as on 4:3: a quarter of a hover unit
		c.control.SpiritFly(k_Evil, 1.0f, 0.5f);
		EXPECT_NEAR(PixelOfHover(screen, {evil.HoverX().GetDestination(), 0.0f}).x,
		            static_cast<float>(screen.width) - 0.25f * static_cast<float>(screen.HoverUnit()), 1.0f);
	}
}

TEST(SpiritScreen, GoingHomeEndsOffTheScreen)
{
	for (const auto& screen : k_CheckScreens)
	{
		for (const auto& [type, across] : {std::pair {k_Good, 0.95f}, std::pair {k_Evil, 0.05f}, std::pair {k_Good, 0.5f}})
		{
			SCOPED_TRACE(screen.width);
			SCOPED_TRACE(across);
			Controller c(screen);
			const int dude = AdvisorSpiritController::DudeOf(type);
			c.control.SpiritEject(type, false);
			c.Frames(20);
			c.control.SpiritFly(type, across, 0.5f);
			c.Frames(20);
			c.control.SpiritHome(type, false);
			glm::vec2 last = c.control.Dude(dude).Hover();
			int frames = 0;
			while (c.control.State(dude) != ControlState::Home && frames < 100)
			{
				last = c.control.Dude(dude).Hover();
				c.Frame();
				++frames;
			}
			ASSERT_EQ(c.control.State(dude), ControlState::Home);
			// Where it was last drawn, its middle lies at least half a hover unit (about twice its own half width)
			// beyond an edge of the real screen
			const glm::vec2 pixel = PixelOfHover(screen, last);
			const float margin = 0.5f * static_cast<float>(screen.HoverUnit());
			const bool off = pixel.x < -margin || pixel.x > static_cast<float>(screen.width) + margin || pixel.y < -margin ||
			                 pixel.y > static_cast<float>(screen.height) + margin;
			EXPECT_TRUE(off) << pixel.x << ", " << pixel.y;
		}
	}
}

TEST(SpiritScreen, The4By3ScreensHomesAreTheGames)
{
	for (const auto& screen : {Screen {640, 480}, Screen {1280, 1024}, Screen {1600, 1200}})
	{
		SCOPED_TRACE(screen.width);
		const Controller c(screen);
		const int32_t halfW = screen.HalfWidth();
		const int32_t halfH = screen.HalfHeight();
		EXPECT_EQ(c.control.Anchor(0), glm::ivec2(halfW, halfH * 3));
		EXPECT_EQ(c.control.Anchor(1), glm::ivec2(-halfW, halfH));
		EXPECT_EQ(c.control.Anchor(2), glm::ivec2(halfW, -halfH));
		EXPECT_EQ(c.control.Anchor(3), glm::ivec2(halfW * 3, halfH));
	}
}
